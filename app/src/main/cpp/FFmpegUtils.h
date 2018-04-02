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

class FFmpegEncoder
{
private:

    AVRational input_time_base;

    AVCodecContext *video_codec_ctx;
    AVDictionary *video_params;

    AVFormatContext *format_ctx;
    AVStream *video_stream;

    AVFilterGraph *video_filter_graph;
    AVFilterInOut *inputs, *outputs;
    AVFilterContext *video_buffersink_ctx, *video_buffersrc_ctx;
    AVFrame *filtered_video_frame;
    long long last_pts;

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
