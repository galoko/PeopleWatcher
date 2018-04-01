#ifndef PEOPLEWATCHER_ENGINE_H
#define PEOPLEWATCHER_ENGINE_H

#include <string>
#include <inttypes.h>
#include <jni.h>

#include "readerwriterqueue.h"
#include "FFmpegUtils.h"

extern "C" {
#include "libavutil/frame.h"
}

using namespace moodycamel;

class Engine
{
public:
    static Engine& getInstance()
    {
        static Engine instance;

        return instance;
    }
private:

    int initialized;

    std::string sdCardPath;

    BlockingReaderWriterQueue<AVFrame*> frames;

    FFmpegEncoder encoder;

    Engine();
public:
    Engine(Engine const&)          = delete;
    void operator=(Engine const&)  = delete;

    void initialize(const char* sdCardPathStr);
    void startRecord(void);
    void stopRecord(void);
    void sendFrame(uint8_t* dataY, uint8_t* dataU, uint8_t* dataV,
                   int strideY, int strideU, int strideV, long long timestamp);

    void workerThreadLoop(JNIEnv* env) __attribute__((noreturn));
};

#endif //PEOPLEWATCHER_ENGINE_H
