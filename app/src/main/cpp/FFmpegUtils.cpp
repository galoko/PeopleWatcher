#include "FFmpegUtils.h"

#include <stdexcept>
#include <android/log.h>

#include "exceptionUtils.h"

extern "C" {
#include "libavutil/error.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"

#include "log.h"
}

void FFmpegEncoder::startRecord(RecordType recordType, int width, int height, const char *filePath) {

    free();

    input_time_base = av_make_q(1, 1000 * 1000 * 1000);

    // search for all structs we need, before we allocate something

    AVOutputFormat *out_format = av_guess_format(NULL, filePath, NULL);
    if (out_format == NULL)
        throw std::runtime_error("Couldn't find output format");

    AVCodec *video_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (video_codec == NULL)
        throw std::runtime_error("Couldn't find video codec");

    // initializing video codec

    my_assert(video_codec_ctx == NULL);
    video_codec_ctx = avcodec_alloc_context3(video_codec);
    if (video_codec_ctx == NULL)
        throw std::runtime_error("Couldn't allocate video codec context");

    video_codec_ctx->width = width;
    video_codec_ctx->height = height;
    video_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    video_codec_ctx->time_base = av_make_q(1, 30);
    video_codec_ctx->thread_count = 4;
    video_codec_ctx->profile = FF_PROFILE_H264_HIGH;
    video_codec_ctx->level = 42;

    // sync codec with output format (important)

    if (out_format->flags & AVFMT_GLOBALHEADER)
        video_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    my_assert(video_params == NULL);
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

    av_check_error(avcodec_open2(video_codec_ctx, video_codec, &video_params));

    // setup file format

    my_assert(format_ctx == NULL);
    av_check_error(avformat_alloc_output_context2(&format_ctx, out_format, NULL, NULL));

    // add video stream to file

    video_stream = avformat_new_stream(format_ctx, video_codec);
    if (video_stream == NULL)
        throw std::runtime_error("Couldn't create video stream");

    video_stream->codecpar->codec_type = video_codec_ctx->codec_type;
    video_stream->codecpar->codec_id = video_codec_ctx->codec_id;
    video_stream->codecpar->codec_tag = video_codec_ctx->codec_tag;
    video_stream->codecpar->width = video_codec_ctx->width;
    video_stream->codecpar->height = video_codec_ctx->height;
    video_stream->codecpar->format = video_codec_ctx->pix_fmt;
    video_stream->codecpar->profile = video_codec_ctx->profile;
    video_stream->codecpar->level = video_codec_ctx->level;
    video_stream->codecpar->bit_rate = video_codec_ctx->bit_rate;

    // copy extra data from codec if any
    // this is also part of syncing codec with output format
    // output format may will this data to produce valid output

    size_t extradata_size = (size_t) video_codec_ctx->extradata_size;
    video_stream->codecpar->extradata_size = extradata_size;
    if (extradata_size > 0) {
        video_stream->codecpar->extradata = (uint8_t *) av_mallocz(
                extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
        memcpy(video_stream->codecpar->extradata, video_codec_ctx->extradata, extradata_size);
    }

    // creating actual file on disk
    av_check_error(avio_open(&format_ctx->pb, filePath, AVIO_FLAG_WRITE));

    // initialize file header
    av_check_error(avformat_write_header(format_ctx, NULL));

    // filters

    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");

    my_assert(video_filter_graph == NULL);
    video_filter_graph = avfilter_graph_alloc();

    char args[512];

    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d",
             video_codec_ctx->width, video_codec_ctx->height, video_codec_ctx->pix_fmt,
             input_time_base.num, input_time_base.den);

    av_check_error(avfilter_graph_create_filter(&video_buffersrc_ctx, buffersrc, "in", args, NULL,
                                                video_filter_graph));

    av_check_error(avfilter_graph_create_filter(&video_buffersink_ctx, buffersink, "out", NULL, NULL,
                                         video_filter_graph));

    my_assert(outputs == NULL);
    outputs = avfilter_inout_alloc();
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = video_buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    my_assert(inputs == NULL);
    inputs = avfilter_inout_alloc();
    inputs->name        = av_strdup("out");
    inputs->filter_ctx  = video_buffersink_ctx;
    inputs->pad_idx     = 0;
    inputs->next        = NULL;

    switch (recordType) {
        case TestData:
            snprintf(args, sizeof(args), "null");
            break;
        case Record:
            snprintf(args, sizeof(args),
                     "drawtext=fontfile='/system/fonts/DroidSans.ttf':" \
                     "text=%%{localtime\\}:x=5:y=5:fontsize=24:" \
                     "fontcolor=white@0.75:box=1:boxcolor=black@0.75");
            break;
        default:
            snprintf(args, sizeof(args), "null");
            my_assert(false);
    }

    av_check_error(avfilter_graph_parse_ptr(video_filter_graph, args, &inputs, &outputs, NULL));

    av_check_error(avfilter_graph_config(video_filter_graph, NULL));

    filtered_video_frame = av_frame_alloc();
}

void FFmpegEncoder::writeFrame(AVFrame* frame) {

    int ret = av_buffersrc_add_frame(video_buffersrc_ctx, frame);
    if (ret < 0) {
        av_frame_free(&frame);
        av_check_error(ret);
    }

    while (true) {
        ret = av_buffersink_get_frame(video_buffersink_ctx, filtered_video_frame);
        if (ret >= 0) {

            filtered_video_frame->pts = av_rescale_q(filtered_video_frame->pts,
                                                     input_time_base, video_codec_ctx->time_base);

            encodeFrame(filtered_video_frame);
        } else
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else {
            av_check_error(ret);
            break;
        }
    }
}

void FFmpegEncoder::encodeFrame(AVFrame *frame) {

    av_check_error(avcodec_send_frame(video_codec_ctx, frame));

    av_frame_unref(frame);

    while (true) {

        AVPacket packet;
        av_init_packet(&packet);

        int ret = avcodec_receive_packet(video_codec_ctx, &packet);
        if (ret >= 0) {

            av_packet_rescale_ts(&packet, video_codec_ctx->time_base, video_stream->time_base);
            packet.stream_index = video_stream->index;

            writePacket(&packet);
        } else
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else {
            av_check_error(ret);
            break;
        }
    }
}

void FFmpegEncoder::writePacket(AVPacket *packet) {

    int ret = av_interleaved_write_frame(format_ctx, packet);
    if (ret < 0) {
        av_packet_unref(packet);
        av_check_error(ret);
    }
}

void FFmpegEncoder::closeRecord(void) {

    // flush filters
    writeFrame(NULL);

    // flush codec
    encodeFrame(NULL);

    // flush output file
    av_check_error(av_interleaved_write_frame(format_ctx, NULL));

    av_check_error(av_write_trailer(format_ctx));

    free();
}

void FFmpegEncoder::free(void) {

    video_stream = NULL;
    if (format_ctx != NULL) {

        avio_closep(&format_ctx->pb);

        avformat_free_context(format_ctx);
        format_ctx = NULL;
    }

    av_dict_free(&video_params);
    avcodec_free_context(&video_codec_ctx);

    avfilter_graph_free(&video_filter_graph);

    video_buffersink_ctx = NULL;
    video_buffersrc_ctx = NULL;

    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    av_frame_free(&filtered_video_frame);
}

void FFmpegEncoder::TestMemoryLeak(RecordType recordType, int width, int height, const char *filePath) {

    FFmpegEncoder instance = FFmpegEncoder();

    for (int counter = 0; counter < 10 * 1000 * 1000; counter++) {
        instance.startRecord(recordType, width, height, filePath);

        instance.closeRecord();
    }

    my_assert(false);
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