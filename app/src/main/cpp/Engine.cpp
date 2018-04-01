#include "Engine.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/pixfmt.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"

#include "imageUtils.h"
#include "log.h"
}

#include "FFmpegUtils.h"
#include "exceptionUtils.h"
#include <string>

#define WIDTH 640
#define HEIGHT 480
#define FRAME_BUFFER_SIZE 15

Engine::Engine() : frames(FRAME_BUFFER_SIZE) {

}

void Engine::initialize(const char *sdCardPath) {

    if (this->initialized)
        return;

    setup_ffmpeg_log();

    this->sdCardPath = std::string(sdCardPath);

    this->initialized = 1;
}

void Engine::startRecord(void) {

    this->startTime = 0;

    encoder.startRecord(TestData, WIDTH, HEIGHT, (this->sdCardPath + "/record.mkv").c_str());
}

void Engine::sendFrame(uint8_t* dataY, uint8_t* dataU, uint8_t* dataV,
                       int strideY, int strideU, int strideV, long long timestamp) {

    if (this->startTime == 0)
        this->startTime = timestamp;

    my_assert(timestamp >= this->startTime);

    if (this->frames.size_approx() < FRAME_BUFFER_SIZE) {

        AVFrame *yuv_frame = av_frame_alloc();
        yuv_frame->width = WIDTH;
        yuv_frame->height = HEIGHT;
        yuv_frame->format = AV_PIX_FMT_YUV420P;
        yuv_frame->pts = timestamp;
        av_check_error(av_frame_get_buffer(yuv_frame, 32));

        convert_yuv420_888_to_yuv420p(dataY, dataU, dataV, strideY, strideU, strideV, yuv_frame);

        if (!frames.try_enqueue(yuv_frame)) {

            // why the fuck this happened?

            av_frame_free(&yuv_frame);

            throw new std::runtime_error("Couldn't enqueue YUV frame");
        }
    }
    else {
        print_log(ANDROID_LOG_WARN, "Engine", "YUV frame drop\n");
    }
}

void Engine::workerThreadLoop(JNIEnv* env) {

    while (1) {

        AVFrame* yuv_frame;

        this->frames.wait_dequeue(yuv_frame);

        if (yuv_frame == NULL)
            break;

        dumpYUV420(yuv_frame, (this->sdCardPath + "/dump.bmp").c_str());
    }
}