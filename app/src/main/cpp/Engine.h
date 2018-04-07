#ifndef PEOPLEWATCHER_ENGINE_H
#define PEOPLEWATCHER_ENGINE_H

#include <string>
#include <inttypes.h>

class Engine {
public:
    static Engine& getInstance() {
        static Engine instance;

        return instance;
    }

    Engine(Engine const&)          = delete;
    void operator=(Engine const&)  = delete;
private:
    Engine(void);

    int initialized;
public:
    void initialize(const char* sdCardPathStr);
    void finalize(void);

    void startRecord(void);
    void stopRecord(void);
    void sendFrame(uint8_t* dataY, uint8_t* dataU, uint8_t* dataV,
                   int strideY, int strideU, int strideV, long long timestamp);
};

#endif //PEOPLEWATCHER_ENGINE_H
