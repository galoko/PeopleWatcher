#ifndef PEOPLEWATCHER_MOTIONDETECTION_H
#define PEOPLEWATCHER_MOTIONDETECTION_H

extern "C" {
#include "libavutil/frame.h"
}

class MotionDetection {
public:
    static MotionDetection& getInstance() {
        static MotionDetection instance;

        return instance;
    }

    MotionDetection(MotionDetection const&) = delete;
    void operator=(MotionDetection const&)  = delete;
private:
    MotionDetection(void);
public:
    void pushFrame(AVFrame* yuvFrame);
    AVFrame* pullFrame(void);
};

#endif //PEOPLEWATCHER_MOTIONDETECTION_H
