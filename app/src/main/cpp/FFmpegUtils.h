#ifndef PEOPLEWATCHER_FFMPEGUTILS_H
#define PEOPLEWATCHER_FFMPEGUTILS_H

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/frame.h"
#include "libavfilter/avfilter.h"
};

#include "media/NdkMediaCodec.h"

void setup_ffmpeg_log();
void av_check_error(int ret);

enum RecordType {
    TestData,
    Record
};

#undef USE_FFMPEG_ENCODER
#define USE_X264

class FFmpegEncoder
{
private:

    // timebase
    AVRational input_time_base, encoder_time_base;

    // file format
    AVFormatContext *format_ctx;
    AVStream *video_stream;
    bool isHeaderWritten;

#ifdef  USE_FFMPEG_ENCODER
    // ffmpeg codec
    AVCodecContext *video_codec_ctx;
    AVDictionary *video_params;
#else
    // hardware codec
    AMediaFormat* format;
    AMediaCodec* codec;
#endif

    // filters
    AVFilterGraph *video_filter_graph;
    AVFilterInOut *inputs, *outputs;
    AVFilterContext *video_buffersink_ctx, *video_buffersrc_ctx;
    AVFrame *filtered_video_frame;

    void encodeFrame(AVFrame *frame);
    void writePacket(AVPacket *packet);

    void free(void);
public:
    void startRecord(RecordType recordType, int width, int height, const char *filePath);
    void writeFrame(AVFrame* frame);
    void closeRecord(void);

    static void TestMemoryLeak(RecordType recordType, int width, int height, const char *filePath);
};

#endif //PEOPLEWATCHER_FFMPEGUTILS_H
