#ifndef PEOPLEWATCHER_MOTIONDETECTOR_H
#define PEOPLEWATCHER_MOTIONDETECTOR_H

#include <queue>

#include "opencv2/highgui.hpp"
#include <opencv2/optflow.hpp>

using namespace cv;

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
        FrameSent,
        MotionDetected,
        ResetDetector,
        FinalizeDetector
    };

    struct DetectionRequest {
        AVFrame *prevFrame, *frame, *nextFrame;
        long long sequenceNum;
        bool haveMotion;
    };

    struct DetectorOperation {
        DetectorOperationType operationType;
        DetectionRequest *request;
        AVFrame* frame;
    };

    static const int MOTION_PROPAGATION_TIME = 525 * 1000 * 1000; // 525 ms in nanonseconds

    static const int OFFSET_Y         = 200;

    static const int INPUT_WIDTH      = Encoder::WIDTH;
    static const int INPUT_HEIGHT     = Encoder::HEIGHT - OFFSET_Y;

    static const int DOWNSCALE_WIDTH  = INPUT_WIDTH  / 2;
    static const int DOWNSCALE_HEIGHT = INPUT_HEIGHT / 2;

    int initialized;

    std::string rootDir;

    MotionDetectorCallback callback;

    // thread pool stuff

    threadpool pool;
    std::atomic_int scheduledCount;

    // separate thread variables

    BlockingConcurrentQueue<DetectorOperation> pendingOperations;

    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_t thread;

    AVFrame *prevFrame, *frame;
    long long currentSequenceNum, nextSequenceNum;

    long long lastFrameTime, lastMotionTime, lastFrameWithMotionTime;
    std::vector<DetectionRequest*> sequentialOperations;
    std::queue<AVFrame*> bufferedFrames;

    static void* thread_entrypoint(void* opaque);
    void threadLoop(void);

    static void pool_worker(void* opaque);
    void PoolWorker(DetectionRequest *request);

    void addFrameToRequests(AVFrame *yuvFrame);
    void resetDetector(void);

    AVFrame* generateGrayDownscaleCrop(AVFrame *yuvFrame);
    static void convertFlowToImage(Mat* flow, Mat* image, double minLen);
    static uint8_t getGrayscaleMeanLuminace(AVFrame *frame);

    bool detectMotion(AVFrame *frame, AVFrame *nextFrame);
    void processDetectedMotion(DetectionRequest *request);
    void processFrame(AVFrame *frame, bool haveMotion);
    void correctTimestamp(AVFrame *frame);

    static bool request_comparer(const DetectionRequest *left, const DetectionRequest *right);
    static void free_detection_request(DetectionRequest **request);
public:
    void initialize(const char *rootDir, MotionDetectorCallback callback);

    bool canAcceptFrame(void);
    void sendFrame(AVFrame* yuvFrame);
    void flush(void);
    void terminate(void);
};

#endif //PEOPLEWATCHER_MOTIONDETECTOR_H
