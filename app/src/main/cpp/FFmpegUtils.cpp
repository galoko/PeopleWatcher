#include "FFmpegUtils.h"

#include <stdexcept>
#include <android/log.h>

#include "exceptionUtils.h"

extern "C" {
#include "libavutil/error.h"
#include "libavutil/avutil.h"
}

void FFmpegEncoder::startRecord(RecordType recordType, int width, int height, const char *filePath) {

    AVCodec *video_codec = avcodec_find_encoder(AV_CODEC_ID_H264);

    video_codec_ctx = avcodec_alloc_context3(video_codec);
    if (video_codec_ctx == NULL)
        throw std::runtime_error("Couldn't allocate video codec context");

    video_codec_ctx->width = width;
    video_codec_ctx->height = height;
    video_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    video_codec_ctx->bit_rate = 1 * 1000 * 1000;
    video_codec_ctx->time_base = av_make_q(1, 30);
    video_codec_ctx->thread_count = 4;
    video_codec_ctx->profile = FF_PROFILE_H264_HIGH;
    video_codec_ctx->level = 42;

    AVDictionary *video_params = NULL;
    av_dict_set(&video_params, "preset", "veryfast", 0);

    switch (recordType) {
        case TestData:
            av_dict_set(&video_params, "crf", "17", 0);
            break;
        case Record:
            av_dict_set(&video_params, "crf", "40", 0);
            break;
        default:
            my_assert(false);
    }

    AVOutputFormat *out_format = av_guess_format(NULL, filePath, NULL);

    if (out_format->flags & AVFMT_GLOBALHEADER)
        video_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    av_check_error(avcodec_open2(video_codec_ctx, video_codec, &video_params));

    AVFormatContext *format_ctx;
    av_check_error(avformat_alloc_output_context2(&format_ctx, out_format, NULL, NULL));

    video_stream = avformat_new_stream(format_ctx, video_codec);
    if (video_stream == NULL)
        throw std::runtime_error("Couldn't create video stream");

    video_stream->codecpar->codec_type = video_codec_ctx->codec_type;
    video_stream->codecpar->codec_id   = video_codec_ctx->codec_id;
    video_stream->codecpar->codec_tag  = video_codec_ctx->codec_tag;
    video_stream->codecpar->width      = video_codec_ctx->width;
    video_stream->codecpar->height     = video_codec_ctx->height;
    video_stream->codecpar->format     = video_codec_ctx->pix_fmt;
    video_stream->codecpar->profile    = video_codec_ctx->profile;
    video_stream->codecpar->level      = video_codec_ctx->level;
    video_stream->codecpar->bit_rate   = video_codec_ctx->bit_rate;

    size_t extradata_size = (size_t) video_codec_ctx->extradata_size;
    video_stream->codecpar->extradata_size = extradata_size;
    if (extradata_size > 0) {
        video_stream->codecpar->extradata = (uint8_t *) av_mallocz(extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
        memcpy(video_stream->codecpar->extradata, video_codec_ctx->extradata, extradata_size);
    }

    av_check_error(avio_open(&format_ctx->pb, filePath, AVIO_FLAG_READ_WRITE));

    av_check_error(avformat_write_header(format_ctx, NULL));

    // filters

    AVRational input_time_base = av_make_q(1, 1000 * 1000 * 1000);

    AVFilterGraph* video_filter_graph = avfilter_graph_alloc();
    const AVFilter* buffersrc = avfilter_get_by_name("buffer");
    const AVFilter* buffersink = avfilter_get_by_name("buffersink");

    AVFilterInOut* outputs = avfilter_inout_alloc();
    AVFilterInOut* inputs = avfilter_inout_alloc();

    char args[512];
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             video_codec_ctx->width, video_codec_ctx->height, video_codec_ctx->pix_fmt,
             input_time_base.num, input_time_base.den,
             video_codec_ctx->sample_aspect_ratio.num, video_codec_ctx->sample_aspect_ratio.den);

    av_check_error(avfilter_graph_create_filter(&video_buffersrc_ctx, buffersrc, "in", args, NULL, video_filter_graph));

    av_check_error(avfilter_graph_create_filter(&video_buffersink_ctx, buffersink, "out", NULL, NULL, video_filter_graph));
}

void FFmpegEncoder::free(void) {

}

void FFmpegEncoder::writeFrame(AVFrame* frame) {

}

void FFmpegEncoder::closeRecord(void) {

    free();
}

// log & error handling

class ffmpeg_error : public std::runtime_error
{
public:
    ffmpeg_error(int errnum) throw();
};

std::string get_ffmpeg_error_str(int ret) {

    return std::string(av_err2str(ret));
}

ffmpeg_error::ffmpeg_error(int ret) throw()
        : std::runtime_error(get_ffmpeg_error_str(ret)) { }

void av_check_error(int ret) {

    if (ret < 0)
        throw ffmpeg_error(ret);
}

void av_log_callback(void *avcl, int level, const char *fmt, va_list vl) {

    enum android_LogPriority priority;

    switch (level) {
        case AV_LOG_PANIC:
            priority = ANDROID_LOG_FATAL;
            break;
        case AV_LOG_FATAL:
            priority = ANDROID_LOG_FATAL;
            break;
        case AV_LOG_ERROR:
            priority = ANDROID_LOG_ERROR;
            break;
        case AV_LOG_WARNING:
            priority = ANDROID_LOG_WARN;
            break;
        case AV_LOG_INFO:
            priority = ANDROID_LOG_INFO;
            break;
        case AV_LOG_VERBOSE:
            priority = ANDROID_LOG_VERBOSE;
            break;
        case AV_LOG_DEBUG:
            priority = ANDROID_LOG_DEBUG;
            break;
        default:
            priority = ANDROID_LOG_UNKNOWN;
            break;
    }

    __android_log_vprint(priority, "FFmpeg", fmt, vl);
}

void setup_ffmpeg_log(void) {
    av_log_set_callback(av_log_callback);
}