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

    avfilter_register_all();

    this->sdCardPath = std::string(sdCardPath);

    this->initialized = 1;

    // FFmpegEncoder::TestMemoryLeak(TestData, WIDTH, HEIGHT, (this->sdCardPath + "/record.flv").c_str());
}

void Engine::startRecord(void) {

    // for now - doing nothing for this method
}

void Engine::stopRecord(void) {

    if (!frames.try_enqueue(NULL)) {

        throw new std::runtime_error("Couldn't send stop signal to worker thread");
    }
}

void Engine::sendFrame(uint8_t* dataY, uint8_t* dataU, uint8_t* dataV,
                       int strideY, int strideU, int strideV, long long timestamp) {

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

    static volatile bool isEngineWorkerThreadTerminated = false;

    long long startTime = 0;

    while (!isEngineWorkerThreadTerminated) {

        AVFrame* yuv_frame;

        this->frames.wait_dequeue(yuv_frame);

        // null frame stop record
        if (yuv_frame == NULL) {

            if (startTime > 0) {

                encoder.closeRecord();
                startTime = 0;
            }

            continue;
        }

        if (startTime == 0) {

            startTime = yuv_frame->pts;

            encoder.startRecord(TestData, WIDTH, HEIGHT, (this->sdCardPath + "/record.flv").c_str());
        }

        my_assert(yuv_frame->pts >= startTime);
        yuv_frame->pts-= startTime;

        double frame_time = yuv_frame->pts / (1000.0 * 1000.0 * 1000.0);

        // dumpYUV420(yuv_frame, (this->sdCardPath + "/dump.bmp").c_str());

        encoder.writeFrame(yuv_frame);

        yuv_frame = NULL;

        if (frame_time >= 5.0)
            encoder.closeRecord();
    }
}