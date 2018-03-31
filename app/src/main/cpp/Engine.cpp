#include "Engine.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/pixfmt.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
}

#include "ffmpegUtils.h"
#include "imageUtils.h"
#include "exceptionUtils.h"

#define WIDTH 640
#define HEIGHT 480
#define FRAME_BUFFER_SIZE 15

struct frame_info {
    long long timestamp;
};

void Engine::initialize(const char *sdCardPath) {

    if (this->initialized)
        return;

    this->sdCardPath = std::string(sdCardPath);
    this->frames = ReaderWriterQueue<AVFrame*>(FRAME_BUFFER_SIZE);

    this->initialized = 1;
}

void Engine::startRecord(long long timestamp) {

}

void Engine::sendFrame(uint8_t* dataY, uint8_t* dataU, uint8_t* dataV,
                       int strideY, int strideU, int strideV, long long timestamp) {

    if (this->frames.size_approx() < FRAME_BUFFER_SIZE) {

        frame_info* info = new frame_info;
        info->timestamp = timestamp;

        AVFrame *yuv_frame = av_frame_alloc();
        yuv_frame->width = WIDTH;
        yuv_frame->height = HEIGHT;
        yuv_frame->format = AV_PIX_FMT_YUV420P;
        yuv_frame->opaque = info;
        av_check_error(av_frame_get_buffer(yuv_frame, 32));

        convert_yuv420_888_to_yuv420p(dataY, dataU, dataV, strideY, strideU, strideV, yuv_frame);

        if (!frames.try_enqueue(yuv_frame)) {

            // why the fuck this happened?

            delete (frame_info*) yuv_frame->opaque;
            yuv_frame->opaque = NULL;

            av_frame_free(&yuv_frame);

            throw new std::runtime_error("Couldn't enqueue YUV frame");
        }
    }
}