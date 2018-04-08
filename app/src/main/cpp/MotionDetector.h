#ifndef PEOPLEWATCHER_MOTIONDETECTOR_H
#define PEOPLEWATCHER_MOTIONDETECTOR_H

#include <queue>

#include "blockingconcurrentqueue.h"

extern "C" {
#include "libavutil/frame.h"
#include "libswscale/swscale.h"

#include "thpool.h"
}

#include "Encoder.h"

using namespace moodycamel;

typedef void (*MotionDetectorCallback)(AVFrame* yuvFrameWithMotion);

class MotionDetector {
public:
    static MotionDetector& getInstance() {
        static MotionDetector instance;

        return instance;
    }

    MotionDetector(MotionDetector const&) = delete;
    void operator=(MotionDetector const&)  = delete;
private:
    MotionDetector(void);

    enum DetectorOperationType {
        MotionDetected,
        ResetDetector,
        FinalizeDetector
    };

    struct DetectionRequest {
        AVFrame *frame, *nextFrame;
        long long sequenceNum;
        bool haveMotion;
    };

    struct DetectorOperation {
        DetectorOperationType operationType;
        DetectionRequest *request;
    };

    static const int MOTION_PROPAGATION_TIME = 150 * 1000 * 1000; // 150 ms in nanonseconds

    static const int OFFSET_Y         = 200;

    static const int INPUT_WIDTH      = Encoder::WIDTH;
    static const int INPUT_HEIGHT     = Encoder::HEIGHT - OFFSET_Y;

    static const int DOWNSCALE_WIDTH  = INPUT_WIDTH  / 2;
    static const int DOWNSCALE_HEIGHT = INPUT_HEIGHT / 2;

    int initialized;

    std::string sdCardPath;

    MotionDetectorCallback callback;

    // variables for generating detection requests in sendFrame

    AVFrame *frame;
    long long nextSequenceNum;

    // thread pool stuff

    threadpool pool;
    std::atomic_int scheduledCount;

    // separate thread variables

    BlockingConcurrentQueue<DetectorOperation> pendingOperations;
    pthread_t thread;

    long long currentSequenceNum, lastMotionTime;
    std::vector<DetectionRequest*> sequentialOperations;
    std::queue<AVFrame*> bufferedFrames;

    static void* thread_entrypoint(void* opaque);
    void threadLoop(void);

    static void pool_worker(void* opaque);
    void PoolWorker(DetectionRequest *request);

    AVFrame* generateGrayDownscaleCrop(AVFrame *yuvFrame);
    bool detectMotion(AVFrame *frame, AVFrame *nextFrame);
    void processDetectedMotion(DetectionRequest *request);
    void processFrame(AVFrame *frame, bool haveMotion);

    static bool request_comparer(const DetectionRequest *left, const DetectionRequest *right);
    static void free_detection_request(DetectionRequest **request);
public:
    void initialize(const char *sdCardPath, MotionDetectorCallback callback);

    bool canAcceptFrame(void);
    void sendFrame(AVFrame* yuvFrame);
    void resetDetector(void);
    void terminate(void);
};

#endif //PEOPLEWATCHER_MOTIONDETECTOR_H
