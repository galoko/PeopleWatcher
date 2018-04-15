#include "Engine.h"

#include <unistd.h>

#include "log.h"

#include "opencv2/highgui.hpp"

extern "C" {
#include "imageUtils.h"
}

#include "Encoder.h"
#include "AsyncIO.h"
#include "MotionDetector.h"

#define ENGINE_TAG "PW_ENGINE"

using namespace cv;

Engine::Engine(void) {
}

void Engine::initialize(const char *rootDir) {

    if (this->initialized)
        return;

    AsyncIO::getInstance().initialize();
    Encoder::getInstance().initialize(rootDir);
    MotionDetector::getInstance().initialize(rootDir, motionDetectorCallback);

    nice(-20);

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

    MotionDetector::getInstance().flush();

    Encoder::getInstance().stopRecord();
}

void Engine::restartRecordIfFramesTooFarApart(long long realtimeTimestamp) {

    if (lastMotionRealtimeTimestamp != 0) {

        long long RECORD_SPLIT_TIME = (long long) 6 * 60 * 60 * 1000 * 1000 * 1000;
        long long elapsed = realtimeTimestamp - lastMotionRealtimeTimestamp;

        if (elapsed >= RECORD_SPLIT_TIME) {

            print_log(ANDROID_LOG_INFO, ENGINE_TAG, "restarting record");

            lastMotionRealtimeTimestamp = 0;

            stopRecord();
            startRecord();
        }
    }
}

void Engine::sendFrame(uint8_t* dataY, uint8_t* dataU, uint8_t* dataV,
                       int strideY, int strideU, int strideV, long long timestamp) {

    restartRecordIfFramesTooFarApart(timestamp);

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

void Engine::motionDetected(AVFrame* yuvFrame, long long realtimeTimestamp) {

    lastMotionRealtimeTimestamp = realtimeTimestamp;

    Encoder::getInstance().sendFrame(yuvFrame);
}

void Engine::motionDetectorCallback(AVFrame *yuvFrame, long long realtimeTimestamp) {
    
    Engine::getInstance().motionDetected(yuvFrame, realtimeTimestamp);
}