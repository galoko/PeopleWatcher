#ifndef PEOPLEWATCHER_ENGINE_H
#define PEOPLEWATCHER_ENGINE_H

#include <string>
#include <string>
#include <inttypes.h>

class Engine
{
public:
    static Engine& getInstance()
    {
        static Engine instance;

        return instance;
    }
private:

    std::string sdCardPath;

    Engine() {}

    void dumpBMP24(uint8_t* pixels, int width, int height, char* fileName);
    void dumpBMP8(uint8_t* pixels, int width, int height, char* fileName);
public:
    Engine(Engine const&)          = delete;
    void operator=(Engine const&)  = delete;

    void initialize(const char* sdCardPath);
    void startRecord(long long timestamp);
    void sendFrame(uint8_t* dataY, uint8_t* dataU, uint8_t* dataV,
                   int strideY, int strideU, int strideV, long long timestamp);
};

#endif //PEOPLEWATCHER_ENGINE_H
