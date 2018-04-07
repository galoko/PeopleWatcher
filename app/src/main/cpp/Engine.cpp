#include "Engine.h"

#include "log.h"

#include "Encoder.h"
#include "AsyncIO.h"

#define ENGINE_TAG "PW_ENGINE"

Engine::Engine(void) {
}

void Engine::initialize(const char *sdCardPath) {

    if (this->initialized)
        return;

    AsyncIO::getInstance().initialize();
    Encoder::getInstance().initialize(sdCardPath);

    this->initialized = 1;
}

void Engine::finalize(void) {

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

    Encoder::getInstance().sendFrame(dataY, dataU, dataV, strideY, strideU, strideV, timestamp);
}