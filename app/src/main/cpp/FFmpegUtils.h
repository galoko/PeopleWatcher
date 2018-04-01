#ifndef PEOPLEWATCHER_FFMPEGUTILS_H
#define PEOPLEWATCHER_FFMPEGUTILS_H

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/frame.h"
#include "libavfilter/avfilter.h"
};

void setup_ffmpeg_log();
void av_check_error(int ret);

enum RecordType {
    TestData,
    Record
};

class FFmpegEncoder
{
private:

    static const constexpr AVRational input_time_base = { .num = 1, .den = 1000 * 1000 * 1000 };

    AVCodecContext *video_codec_ctx;
    AVDictionary *video_params;

    AVFormatContext *format_ctx;
    AVStream *video_stream;

    AVFilterGraph *video_filter_graph;
    AVFilterInOut *inputs, *outputs;
    AVFilterContext *video_buffersink_ctx, *video_buffersrc_ctx;

    void flushVideoCodec(void);
    void flushVideoFilters(void);

    void free(void);
public:
    void startRecord(RecordType recordType, int width, int height, const char *filePath);
    void writeFrame(AVFrame* frame);
    void closeRecord(void);

    static void TestMemoryLeak(RecordType recordType, int width, int height, const char *filePath);
};

#endif //PEOPLEWATCHER_FFMPEGUTILS_H
