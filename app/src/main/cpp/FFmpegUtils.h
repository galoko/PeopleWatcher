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
    AVCodecContext* video_codec_ctx;
    AVStream* video_stream;

    AVFilterContext *video_buffersink_ctx, *video_buffersrc_ctx;

    void free(void);
public:
    void startRecord(RecordType recordType, int width, int height, const char *filePath);
    void writeFrame(AVFrame* frame);
    void closeRecord(void);
};

#endif //PEOPLEWATCHER_FFMPEGUTILS_H
