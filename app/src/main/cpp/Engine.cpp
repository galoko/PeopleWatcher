#include "Engine.h"

#include "log.h"

extern "C" {
#include "imageUtils.h"
}

#include "Encoder.h"
#include "AsyncIO.h"
#include "MotionDetector.h"

#define ENGINE_TAG "PW_ENGINE"

Engine::Engine(void) {
}

void Engine::initialize(const char *sdCardPath) {

    if (this->initialized)
        return;

    AsyncIO::getInstance().initialize();
    Encoder::getInstance().initialize(sdCardPath);
    MotionDetector::getInstance().initialize(sdCardPath, motionDetectorCallback);

    this->initialized = 1;
}

void Engine::finalize(void) {

    MotionDetector::getInstance().terminate();
    Encoder::getInstance().terminate();
    AsyncIO::getInstance().terminate();

    print_log(ANDROID_LOG_INFO, ENGINE_TAG, "Engine is finalized");
}

void Engine::startRecord(void) {

    Encoder::getInstance().startRecord();
}

void Engine::stopRecord(void) {

    Encoder::getInstance().stopRecord();
}

void Engine::sendFrame(uint8_t* dataY, uint8_t* dataU, uint8_t* dataV,
                       int strideY, int strideU, int strideV, long long timestamp) {

    if (MotionDetector::getInstance().canAcceptFrame()) {

        AVFrame *yuvFrame = av_frame_alloc();
        yuvFrame->width = Encoder::WIDTH;
        yuvFrame->height = Encoder::HEIGHT;
        yuvFrame->format = AV_PIX_FMT_YUV420P;
        yuvFrame->pts = timestamp;
        av_check_error(av_frame_get_buffer(yuvFrame, 32));

        convert_yuv420_888_to_yuv420p(dataY, dataU, dataV, strideY, strideU, strideV, yuvFrame);

        MotionDetector::getInstance().sendFrame(yuvFrame);
    } else {
        print_log(ANDROID_LOG_WARN, ENGINE_TAG, "Frame drop");
    }
}

void Engine::motionDetected(AVFrame* yuvFrame) {

    av_frame_free(&yuvFrame);

    // Encoder::getInstance().sendFrame(yuvFrame);
}

void Engine::motionDetectorCallback(AVFrame *yuvFrame) {
    Engine::getInstance().motionDetected(yuvFrame);
}