#include "MotionDetector.h"

#include "log.h"
#include "exceptionUtils.h"

extern "C" {
#include "imageUtils.h"
}

#define MOTION_DETECTOR_TAG "PW_MOTION_DETECTOR"

#define FRAME_BUFFER_SIZE (20 * 1) // 20 fps for 3 seconds (~29 MB buffer)
#define MAX_SCHEDULED_DETECTIONS (FRAME_BUFFER_SIZE / 2)
#define THREADS_IN_THREAD_POOL 3

MotionDetector::MotionDetector(void) : pendingOperations(FRAME_BUFFER_SIZE,
                                                         // one for Java frames send thread
                                                         1 + THREADS_IN_THREAD_POOL,
                                                         1 + THREADS_IN_THREAD_POOL) {
}

void MotionDetector::initialize(const char *rootDir, MotionDetectorCallback callback) {

    if (this->initialized)
        return;

    this->rootDir = std::string(rootDir);

    this->callback = callback;

    pool = thpool_init(THREADS_IN_THREAD_POOL);
    if (pool == NULL)
        throw new std::runtime_error("Couldn't allocate thread pool");

    pthread_check_error(pthread_mutex_init(&mutex, NULL));
    pthread_check_error(pthread_cond_init(&cond, NULL));

    pthread_check_error(pthread_create(&thread, NULL, thread_entrypoint, NULL));

    this->initialized = 1;
}

bool MotionDetector::canAcceptFrame(void) {

    return scheduledCount < MAX_SCHEDULED_DETECTIONS;
}

// send frame directly to the thread pool
void MotionDetector::sendFrame(AVFrame* yuvFrame) {

    DetectorOperation operation = { };
    operation.operationType = FrameSent;
    operation.frame = yuvFrame;

    if (!pendingOperations.try_enqueue(operation)) {

        print_log(ANDROID_LOG_WARN, MOTION_DETECTOR_TAG, "Frame drop at sendFrame");

        av_frame_free(&yuvFrame);
    }
}

void MotionDetector::flush(void) {

    thpool_wait(pool);

    print_log(ANDROID_LOG_DEBUG, MOTION_DETECTOR_TAG, "flush: wait reset enter");

    pthread_check_error(pthread_mutex_lock(&mutex));
    resetDetector();
    pthread_check_error(pthread_cond_wait(&cond, &mutex));
    pthread_check_error(pthread_mutex_unlock(&mutex));

    print_log(ANDROID_LOG_DEBUG, MOTION_DETECTOR_TAG, "flush: wait reset exit");
}

void MotionDetector::resetDetector(void) {

    DetectorOperation operation = { };
    operation.operationType = ResetDetector;

    pendingOperations.enqueue(operation);
}

void MotionDetector::terminate(void) {

    flush();

    thpool_destroy(pool);
    pool = NULL;

    DetectorOperation operation = { };
    operation.operationType = FinalizeDetector;

    pendingOperations.enqueue(operation);

    pthread_check_error(pthread_join(thread, NULL));
}

// processing

void MotionDetector::addFrameToRequests(AVFrame *yuvFrame) {

    my_assert(yuvFrame->opaque == NULL);

    if (lastFrameTime != 0) {

        long long frameTime = yuvFrame->pts - lastFrameTime;
        my_assert(frameTime >= 0 && frameTime <= ULONG_MAX);

        yuvFrame->opaque = (void*) frameTime;

        print_log(ANDROID_LOG_DEBUG, MOTION_DETECTOR_TAG, "frame time: %lld ms", frameTime / (1000 * 1000));
    } else {
        yuvFrame->opaque = (void*) 0;
    }

    lastFrameTime = yuvFrame->pts;

    if (pool == NULL) {

        print_log(ANDROID_LOG_ERROR, MOTION_DETECTOR_TAG, "sendFrame call after finalization");

        av_frame_free(&yuvFrame);
        return;
    }

    if (scheduledCount >= MAX_SCHEDULED_DETECTIONS) {

        print_log(ANDROID_LOG_WARN, MOTION_DETECTOR_TAG, "Frame drop at schedule");

        av_frame_free(&yuvFrame);
        return;
    }

    // we need three frames in order to perform motion detection

    if (this->prevFrame == NULL) {

        this->prevFrame = yuvFrame;
        return;
    }

    if (this->frame == NULL) {

        this->frame = yuvFrame;
        return;
    }

    DetectionRequest *request = new DetectionRequest();
    request->prevFrame = this->prevFrame;
    request->frame = this->frame;
    request->nextFrame = yuvFrame;
    request->sequenceNum = nextSequenceNum;

    this->prevFrame = NULL;
    this->frame = NULL;

    scheduledCount++;

    print_log(ANDROID_LOG_DEBUG, MOTION_DETECTOR_TAG, "added task to thread pool, scheduled: %d",
              (int) scheduledCount);

    int ret = thpool_add_work(this->pool, pool_worker, (void*) request);
    if (ret != 0) {

        print_log(ANDROID_LOG_WARN, MOTION_DETECTOR_TAG, "Frame drop at thread pool");

        // delete request and do rollback

        free_detection_request(&request);

        scheduledCount--;

        return;
    }

    nextSequenceNum++;
}

void MotionDetector::processDetectedMotion(DetectionRequest *request) {

    // enforce sequentiality for all requests

    std::vector<DetectionRequest*>::iterator insertion_point;

    insertion_point = std::lower_bound(sequentialOperations.begin(), sequentialOperations.end(), request, request_comparer);

    sequentialOperations.insert(insertion_point, request);

    std::vector<DetectionRequest*>::iterator it = sequentialOperations.begin();
    for( ; it != sequentialOperations.end();)
    {
        DetectionRequest* currentRequest = *it;

        if (currentRequest->sequenceNum == currentSequenceNum) {

            it = sequentialOperations.erase(it);

            currentSequenceNum++;

            processFrame(currentRequest->prevFrame, false);
            processFrame(currentRequest->frame, currentRequest->haveMotion);
            processFrame(currentRequest->nextFrame, false);

            currentRequest->prevFrame = NULL;
            currentRequest->frame = NULL;
            currentRequest->nextFrame = NULL;

            free_detection_request(&currentRequest);
        }
        else
            break;
    }
}

void MotionDetector::correctTimestamp(AVFrame *frame) {

    if (lastFrameWithMotionTime == 0) {

        lastFrameWithMotionTime = frame->pts;
        return;
    }

    unsigned long int frameTime = (unsigned long int) frame->opaque;
    lastFrameWithMotionTime += frameTime;

    frame->pts = lastFrameWithMotionTime;
}

void MotionDetector::processFrame(AVFrame *frame, bool haveMotion) {

    if (!bufferedFrames.empty()) {

        AVFrame* prevFrame = bufferedFrames.back();

        long long currentTime = prevFrame->pts;

        while (!bufferedFrames.empty()) {

            AVFrame* latestFrame = bufferedFrames.front();

            long long latestTime = latestFrame->pts;

            bool tooManyFramesBuffered = bufferedFrames.size() >= FRAME_BUFFER_SIZE;
            bool frameExpired = latestTime + MOTION_PROPAGATION_TIME < currentTime;

            bool frameHavePropagatedMotion = lastMotionTime + MOTION_PROPAGATION_TIME >= latestTime;
            bool frameHaveMotion = frameHavePropagatedMotion || (!frameExpired && haveMotion);

            if (frameHaveMotion || frameExpired || tooManyFramesBuffered) {

                bufferedFrames.pop();

                if (frameHaveMotion && callback != NULL) {

                    long long realTimeTimestamp = latestFrame->pts;
                    correctTimestamp(latestFrame);

                    print_log(ANDROID_LOG_DEBUG, MOTION_DETECTOR_TAG, "frame with motion send to callback");
                    callback(latestFrame, realTimeTimestamp);
                }
                else
                    av_frame_free(&latestFrame);
            } else
                break;
        }

        if (haveMotion)
            lastMotionTime = currentTime;
    }

    bufferedFrames.push(frame);
}

static thread_local SwsContext* tls_downscaler;

AVFrame* MotionDetector::generateGrayDownscaleCrop(AVFrame *yuvFrame) {

    int ret;

    AVFrame *downscale = av_frame_alloc();
    downscale->width = DOWNSCALE_WIDTH;
    downscale->height = DOWNSCALE_HEIGHT;
    downscale->format = AV_PIX_FMT_GRAY8;

    ret = av_frame_get_buffer(downscale, 32);
    if (ret < 0) {
        av_frame_free(&downscale);
        av_check_error(ret);
        return NULL;
    }

    // crop few top rows by ourselves, because I couldn't get swscale to do the same
    // don't actually know what srcSliceY mean, but it's not cropping rows that's for sure
    const uint8_t *data[AV_NUM_DATA_POINTERS] = { yuvFrame->data[0] + OFFSET_Y * yuvFrame->linesize[0] };
    int linesize[AV_NUM_DATA_POINTERS] = { yuvFrame->linesize[0] };

    SwsContext *downscaler = tls_downscaler;

    if (downscaler == NULL) {
        downscaler = sws_getContext(INPUT_WIDTH, INPUT_HEIGHT, AV_PIX_FMT_GRAY8,
                                    DOWNSCALE_WIDTH, DOWNSCALE_HEIGHT, AV_PIX_FMT_GRAY8, SWS_AREA,
                                    NULL, NULL, NULL);
        if (downscaler == NULL)
            throw new std::runtime_error("Couldn't allocate downscaler");

        tls_downscaler = downscaler;
    }

    ret = sws_scale(downscaler, data, linesize, 0, yuvFrame->height - OFFSET_Y, downscale->data, downscale->linesize);
    if (ret < 0) {
        av_frame_free(&downscale);
        av_check_error(ret);
        return NULL;
    }

    return downscale;
}

void MotionDetector::convertFlowToImage(Mat* flow, Mat* image, double minLen) {

    double minLenSq = minLen * minLen;

    int width = flow->cols;
    int height = flow->rows;

    *image = Mat(height, width, CV_8UC1);

    for (int y = 0; y < height; y++) {

        uchar *pixelRow = image->ptr<uchar>(y);
        Point2f *flowRow = flow->ptr<Point2f>(y);

        for (int x = 0; x < width; x++) {

            Point2f v = flowRow[x];

            double lenSq = v.dot(v);

            pixelRow[x] = (uchar) (lenSq < minLenSq ?  0 : 255);
        }
    }
}

uint8_t MotionDetector::getGrayscaleMeanLuminace(AVFrame *frame) {

    unsigned long int sum = 0;

    int width = frame->width;
    int height = frame->height;

    uchar *pixels = frame->data[0];

    for (int y = 0; y < height; y++) {

        uchar *pixelRow = pixels;

        for (int x = 0; x < width; x++) {

            sum += *pixelRow;
            pixelRow++;
        }

        pixels += frame->linesize[0];
    }

    return (uint8_t) (sum / (width * height));
}

bool MotionDetector::detectMotion(AVFrame *frame, AVFrame *nextFrame) {

    int64 startTime = getTickCount();

    bool haveMovement = false;

    uint8_t luminance = getGrayscaleMeanLuminace(frame);
    if (luminance > 100) {

        Mat img(frame->height, frame->width, CV_8UC1, frame->data[0],
                (size_t) frame->linesize[0]);
        Mat nextImg(nextFrame->height, nextFrame->width, CV_8UC1, nextFrame->data[0],
                    (size_t) nextFrame->linesize[0]);

        Mat flow;

        img.convertTo(img, -1, 0.75, 0.0);
        nextImg.convertTo(nextImg, -1, 0.75, 0.0);

        GaussianBlur(img, img, Size(21, 21), 0.0);
        GaussianBlur(nextImg, nextImg, Size(21, 21), 0.0);

        calcOpticalFlowFarneback(img, nextImg, flow, 0.5, 1, 25, 1, 5, 1.1, 0);

        Mat flowImg;
        convertFlowToImage(&flow, &flowImg, 0.1);

        // dilate(flowImg, flowImg, Mat(), Point(-1, -1), 3);

        std::vector<std::vector<Point>> contours;
        findContours(flowImg, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

        // loop over the contours
        for (std::vector<Point> contour : contours) {

            if (contourArea(contour) >= 20 * 20) {
                haveMovement = true;
                break;
            }
        }
    }

    int64 endTime = getTickCount();

    double elapsed = (endTime - startTime) / getTickFrequency();

    print_log(ANDROID_LOG_DEBUG, MOTION_DETECTOR_TAG, "Motion detection took %d ms", (int) (elapsed * 1000.0));

    return haveMovement;
}

void MotionDetector::pool_worker(void* opaque) {

    DetectionRequest *request = (DetectionRequest*) opaque;

    MotionDetector::getInstance().PoolWorker(request);
}

void MotionDetector::PoolWorker(DetectionRequest *request) {

    AVFrame *gray, *grayNext;

    gray = generateGrayDownscaleCrop(request->frame);
    grayNext = generateGrayDownscaleCrop(request->nextFrame);

    request->haveMotion = detectMotion(gray, grayNext);

    av_frame_free(&grayNext);
    av_frame_free(&gray);

    this->scheduledCount--;

    DetectorOperation operation = { };
    operation.operationType = MotionDetected;
    operation.request = request;

    if (!pendingOperations.try_enqueue(operation)) {

        print_log(ANDROID_LOG_WARN, MOTION_DETECTOR_TAG, "Frame drop after motion detection");

        free_detection_request(&request);
    }
}

// thread

void* MotionDetector::thread_entrypoint(void* opaque) {

    MotionDetector::getInstance().threadLoop();
    return NULL;
}

void MotionDetector::threadLoop(void) {

    while (true) {

        DetectorOperation operation;

        this->pendingOperations.wait_dequeue(operation);

        if (operation.operationType == FrameSent) {

            addFrameToRequests(operation.frame);

        } else if (operation.operationType == MotionDetected) {

            processDetectedMotion(operation.request);

        } else if (operation.operationType == ResetDetector || operation.operationType == FinalizeDetector) {

            pthread_check_error(pthread_mutex_lock(&mutex));

            if (operation.operationType == ResetDetector)
                print_log(ANDROID_LOG_DEBUG, MOTION_DETECTOR_TAG, "reset start");

            // deleting frames that awaits motion that will not happen at this point
            while (!bufferedFrames.empty()) {

                AVFrame* frame = bufferedFrames.front();
                bufferedFrames.pop();

                av_frame_free(&frame);
            }

            // deleting non sequential requests that awaits other requests in order to become sequential

            /*
            since reset can only occur after flush, i.e. all thread pool tasks are finished at this point,
            then all sequential requests should be sorted into completely sequential stream of requests and
            then be processed fully, so we have no need to destroy any requests in normal circumstances
             */

            for (DetectionRequest *request : sequentialOperations) {

                print_log(ANDROID_LOG_WARN, MOTION_DETECTOR_TAG, "unsorted request found after flush");

                free_detection_request(&request);
            }
            sequentialOperations.clear();

            // deleting frames that awaits to become request

            av_frame_free(&frame);

            // check everything to be empty

            my_assert(sequentialOperations.empty());
            my_assert(bufferedFrames.empty());

            // resetting sequence numbers and time variables

            currentSequenceNum = 0;
            nextSequenceNum = 0;

            lastFrameTime = 0;
            lastMotionTime = 0;
            lastFrameWithMotionTime = 0;

            if (operation.operationType == ResetDetector)
                print_log(ANDROID_LOG_DEBUG, MOTION_DETECTOR_TAG, "reset finish");

            pthread_check_error(pthread_cond_signal(&cond));
            pthread_check_error(pthread_mutex_unlock(&mutex));

            if (operation.operationType == FinalizeDetector)
                break;
        }
    }

    if (this->pendingOperations.size_approx() > 0)
        print_log(ANDROID_LOG_WARN, MOTION_DETECTOR_TAG,
                  "Motion detector: Still have pending operations after finalization");
}

// utils

bool MotionDetector::request_comparer(const DetectionRequest *left, const DetectionRequest *right) {

    return left->sequenceNum < right->sequenceNum;
}

void MotionDetector::free_detection_request(DetectionRequest **request) {

    if (*request != NULL) {
        av_frame_free(&(*request)->prevFrame);
        av_frame_free(&(*request)->frame);
        av_frame_free(&(*request)->nextFrame);
        delete *request;
        *request = NULL;
    }
}