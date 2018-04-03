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
#include "generalUtils.h"
}

#define ENCODER_TAG "PW_ENCODER"

void FFmpegEncoder::startRecord(RecordType recordType, int width, int height, const char *filePath) {

    free();

    input_time_base = av_make_q(1, 1000 * 1000 * 1000); // nanoseconds

    // search for all structs we need, before we allocate something

    AVOutputFormat *out_format = av_guess_format(NULL, filePath, NULL);
    if (out_format == NULL)
        throw std::runtime_error("Couldn't find output format");

#ifdef USE_FFMPEG_ENCODER
    // find encoder
    AVCodec *video_codec;
#ifdef USE_X264
    video_codec = avcodec_find_encoder_by_name("libx264");
#else
    video_codec = avcodec_find_encoder_by_name("libopenh264");
#endif
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
    video_codec_ctx->time_base = av_make_q(1, 60);
    video_codec_ctx->profile = FF_PROFILE_H264_CONSTRAINED_BASELINE;
    video_codec_ctx->level = 30;

    // sync codec with output format (important)

    if (out_format->flags & AVFMT_GLOBALHEADER)
        video_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    my_assert(video_params == NULL);
#ifdef USE_X264
    av_dict_set(&video_params, "preset", "ultrafast", 0);
    av_dict_set(&video_params, "tune", "stillimage", 0);
    av_dict_set(&video_params, "x264-params", "qp=30:cabac=0:deblock=1:subme=1:ref=0:b-adapt=0:me=dia", 0);
#else
    av_dict_set(&video_params, "profile", "baseline", 0);
    av_dict_set(&video_params, "cabac", "0", 0);
    video_codec_ctx->bit_rate = 3 * 1000 * 1000;
#endif

    av_check_error(avcodec_open2(video_codec_ctx, video_codec, &video_params));

    encoder_time_base = video_codec_ctx->time_base;
#else
    // initialize MediaCodec

    my_assert(codec == NULL);
    codec = AMediaCodec_createEncoderByType("video/avc");
    if (codec == NULL)
        throw std::runtime_error("Couldn't create media codec encoder");

    my_assert(format == NULL);
    format = AMediaFormat_new();
    AMediaFormat_setString(format, "mime", "video/avc");
    AMediaFormat_setInt32(format, "width", width);
    AMediaFormat_setInt32(format, "height", height);
    AMediaFormat_setInt32(format, "i-frame-interval", 60);
    AMediaFormat_setInt32(format, "frame-rate", 20);
    AMediaFormat_setInt32(format, "color-format", 19);
    AMediaFormat_setInt32(format, "priority", 0);

    // constant bitrate
    AMediaFormat_setInt32(format, "bitrate-mode", 1);
    AMediaFormat_setInt32(format, "bitrate", 1 * 1000 * 1000);

    media_status_t status;

    status = AMediaCodec_configure(codec, format, NULL, NULL, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
    if (status != AMEDIA_OK)
        throw new std::runtime_error("Couldn't configure media codec encoder");

    status = AMediaCodec_start(codec);
    if (status != AMEDIA_OK)
        throw new std::runtime_error("Couldn't start media codec encoder");

    AMediaFormat_delete(format);
    format = NULL;

    encoder_time_base = av_make_q(1, 1000 * 1000); // microseconds
#endif

    // setup file format

    my_assert(format_ctx == NULL);
    av_check_error(avformat_alloc_output_context2(&format_ctx, out_format, NULL, NULL));

    // add video stream to file

    video_stream = avformat_new_stream(format_ctx, NULL);
    if (video_stream == NULL)
        throw std::runtime_error("Couldn't create video stream");

    video_stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    video_stream->codecpar->codec_id = AV_CODEC_ID_H264;
    video_stream->codecpar->width = width;
    video_stream->codecpar->height = height;
    video_stream->codecpar->format = AV_PIX_FMT_YUV420P;

    // creating actual file on disk

    av_check_error(avio_open(&format_ctx->pb, filePath, AVIO_FLAG_WRITE | AVIO_FLAG_NONBLOCK));

#ifdef USE_FFMPEG_ENCODER
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

    // initialize file header
    av_check_error(avformat_write_header(format_ctx, NULL));

    isHeaderWritten = true;
#else
    isHeaderWritten = false;
#endif

    // filters

    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");

    my_assert(video_filter_graph == NULL);
    video_filter_graph = avfilter_graph_alloc();

    char args[512];

    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d",
             video_stream->codecpar->width, video_stream->codecpar->height,
             video_stream->codecpar->format,
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
                                                     input_time_base, encoder_time_base);

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

    double startTime = getTime();

#ifdef USE_FFMPEG_ENCODER
    av_check_error(avcodec_send_frame(video_codec_ctx, frame));

    av_frame_unref(frame);

    while (true) {

        AVPacket packet;
        av_init_packet(&packet);

        int ret = avcodec_receive_packet(video_codec_ctx, &packet);
        if (ret >= 0) {

            av_packet_rescale_ts(&packet, encoder_time_base, video_stream->time_base);
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
#else
    if (frame == NULL) {

        media_status_t status;

        status = AMediaCodec_flush(codec);
        if (status != AMEDIA_OK)
            throw new std::runtime_error("Couldn't flush media codec encoder");

        print_log(ANDROID_LOG_INFO, ENCODER_TAG, "flushing media codec\n");
    }
    else {
        ssize_t inputBufferIndex = AMediaCodec_dequeueInputBuffer(codec, -1);
        if (inputBufferIndex >= 0) {

            size_t bufferSize;
            uint8_t *buffer = AMediaCodec_getInputBuffer(codec, (size_t) inputBufferIndex,
                                                         &bufferSize);
            if (buffer == NULL)
                throw new std::runtime_error("Input buffer is NULL");

            size_t frameSize = (size_t) frame->linesize[0] * frame->height +
                               (frame->linesize[1] * frame->height / 2) * 2;

            if (frameSize > bufferSize)
                throw new std::runtime_error("Input buffer is smaller than frame");

            memcpy(buffer, frame->data[0], (size_t) frame->linesize[0] * frame->height);
            buffer += frame->linesize[0] * frame->height;

            memcpy(buffer, frame->data[1], (size_t) frame->linesize[1] * (frame->height / 2));
            buffer += frame->linesize[1] * (frame->height / 2);

            memcpy(buffer, frame->data[2], (size_t) frame->linesize[2] * (frame->height / 2));

            media_status_t status;

            status = AMediaCodec_queueInputBuffer(codec, (size_t) inputBufferIndex, 0, frameSize,
                                                  (uint64_t) frame->pts, 0);
            if (status != AMEDIA_OK)
                throw new std::runtime_error("Couldn't put buffer back into media codec encoder");
        } else if (inputBufferIndex == -1) {
            print_log(ANDROID_LOG_WARN, ENCODER_TAG, "Media codec encoder dropped frame\n");
        } else
            throw new std::runtime_error("Error while getting input buffer");

        av_frame_unref(frame);
    }

    AMediaCodecBufferInfo info;

    while (true) {
        int outputBufferIndex = AMediaCodec_dequeueOutputBuffer(codec, &info, 0);
        if (outputBufferIndex >= 0) {

            size_t bufferSize;
            uint8_t *buffer = AMediaCodec_getOutputBuffer(codec, (size_t) outputBufferIndex,
                                                          &bufferSize);
            if (buffer == NULL)
                throw new std::runtime_error("Output buffer is NULL");

            if (!isHeaderWritten) {
                if ((info.flags & 2) != 0) {
                    size_t extradata_size = (size_t) info.size;
                    video_stream->codecpar->extradata_size = extradata_size;
                    if (extradata_size > 0) {
                        video_stream->codecpar->extradata = (uint8_t *) av_mallocz(
                                extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
                        memcpy(video_stream->codecpar->extradata, buffer + info.offset,
                               extradata_size);
                    }

                    // initialize file header
                    av_check_error(avformat_write_header(format_ctx, NULL));

                    isHeaderWritten = true;
                } else {
                    throw new std::runtime_error(
                            "Cannot get SPS and PPS nal units, header couldn't be written");
                }
            } else {
                AVPacket packet;
                av_init_packet(&packet);

                packet.data = buffer + info.offset;
                packet.size = info.size;
                packet.pts = info.presentationTimeUs;
                packet.dts = packet.pts;

                av_packet_rescale_ts(&packet, encoder_time_base, video_stream->time_base);
                packet.stream_index = video_stream->index;

                writePacket(&packet);
            }

            AMediaCodec_releaseOutputBuffer(codec, (size_t) outputBufferIndex, false);
        } else if (outputBufferIndex == -3) {
            throw new std::runtime_error("-3 from dequeueOutputBuffer, what should I do?");
        } else if (outputBufferIndex == -2) {
            // throw new std::runtime_error("-2 from dequeueOutputBuffer, what should I do?");
        } else if (outputBufferIndex == -1) {
            break;
        } else
            throw new std::runtime_error("Error while getting output buffer");
    }
#endif

    double elapsed = getTime() - startTime;

    print_log(ANDROID_LOG_DEBUG, ENCODER_TAG, "%f ms per frame\n", elapsed * 1000);
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
    print_log(ANDROID_LOG_INFO, ENCODER_TAG, "flushing filters\n");
    writeFrame(NULL);

    // flush codec
    print_log(ANDROID_LOG_INFO, ENCODER_TAG, "flushing codec\n");
    encodeFrame(NULL);

    // flush output file
    print_log(ANDROID_LOG_INFO, ENCODER_TAG, "flushing file\n");
    av_check_error(av_interleaved_write_frame(format_ctx, NULL));

    print_log(ANDROID_LOG_INFO, ENCODER_TAG, "writing headers\n");
    av_check_error(av_write_trailer(format_ctx));

    print_log(ANDROID_LOG_INFO, ENCODER_TAG, "record closed\n");

    free();
}

void FFmpegEncoder::free(void) {

    // file output

    video_stream = NULL;
    if (format_ctx != NULL) {

        avio_closep(&format_ctx->pb);

        avformat_free_context(format_ctx);
        format_ctx = NULL;
    }

    // video encoder

#ifdef USE_FFMPEG_ENCODER
    av_dict_free(&video_params);
    avcodec_free_context(&video_codec_ctx);
#else
    if (format != NULL) {
        AMediaFormat_delete(format);
        format = NULL;
    }
    if (codec != NULL) {
        AMediaCodec_stop(codec);
        AMediaCodec_delete(codec);
        codec = NULL;
    }
#endif

    // filters

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

#define FFMPEG_TAG "PW_FFMPEG"

void av_log_callback(void *avcl, int level, const char *fmt, va_list vl) {

    enum android_LogPriority priority;

    if (level > AV_LOG_INFO)
        return;

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

    __android_log_vprint(priority, FFMPEG_TAG, fmt, vl);
}

void setup_ffmpeg_log(void) {
    av_log_set_callback(av_log_callback);
}