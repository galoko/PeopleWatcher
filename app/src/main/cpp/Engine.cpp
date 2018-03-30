#include "Engine.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/pixfmt.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
}

#include "ffmpegUtils.h"
#include "imageUtils.h"

#define WIDTH 640
#define HEIGHT 480

void Engine::initialize(const char *sdCardPath) {

    if (this->initialized)
        return;

    this->sdCardPath = std::string(sdCardPath);
    this->frames = ReaderWriterQueue<AVFrame*>(15);

    this->initialized = 1;
}

void Engine::startRecord(long long timestamp) {

}

void Engine::sendFrame(uint8_t* dataY, uint8_t* dataU, uint8_t* dataV,
                       int strideY, int strideU, int strideV, long long timestamp) {

    AVFrame* yuv_frame = av_frame_alloc();
    yuv_frame->width = WIDTH;
    yuv_frame->height = HEIGHT;
    yuv_frame->format = AV_PIX_FMT_YUV420P;
    av_check_error(av_frame_get_buffer(yuv_frame, 32));

    convert_yuv420_888_to_yuv420p(dataY, dataU, dataV, strideY, strideU, strideV, yuv_frame);

    // addFrameQueue(yuv_frame);

    /*
    AVFrame* rgb_frame = av_frame_alloc();
    rgb_frame->width = WIDTH;
    rgb_frame->height = HEIGHT;
    rgb_frame->format = AV_PIX_FMT_BGR24;
    av_check_error(av_frame_get_buffer(rgb_frame, 32));



    SwsContext* ctx = sws_getContext(yuv_frame->width, yuv_frame->height, (AVPixelFormat) yuv_frame->format,
                                     rgb_frame->width, rgb_frame->height, (AVPixelFormat) rgb_frame->format,
                                     SWS_POINT, NULL, NULL, NULL);

    int ret = sws_scale(ctx, ((const uint8_t *const*) &yuv_frame->data), yuv_frame->linesize, 0,
                        HEIGHT, rgb_frame->data, rgb_frame->linesize);

    dumpBMP24(rgb_frame->data[0], rgb_frame->width, rgb_frame->height, "dump.bmp");
    */
}

void Engine::addFrameQueue(AVFrame* frame) {

    if (frames.try_enqueue(frame)) {

    } else {

    }
}