#ifndef PEOPLEWATCHER_ENCODER_H
#define PEOPLEWATCHER_ENCODER_H

#include "blockingconcurrentqueue.h"
#include "FFmpegUtils.h"

extern "C" {
#include "libavutil/frame.h"
}

using namespace moodycamel;

class Encoder {
public:
    static Encoder& getInstance() {
        static Encoder instance;

        return instance;
    }

    Encoder(Encoder const&) = delete;
    void operator=(Encoder const&)  = delete;
private:
    Encoder(void);

    enum FrameOperationType {
        StartRecord,
        EncodeFrame,
        CloseRecord,
        FinalizeEncoder
    };

    struct EncoderOperation {
        FrameOperationType  operationType;
        AVFrame *frame;
    };

    int initialized;

    std::string rootDir;

    BlockingConcurrentQueue<EncoderOperation> pendingOperations;

    FFmpegEncoder encoder;
    std::string currentRecordFilePath;
    FILE *io_file;
    void* io_buffer;

    pthread_t thread;

    static void* thread_entrypoint(void* opaque);
    void threadLoop(void);

    std::string getFilePathForRecord(void);
    std::string removeInUseFlagFromFilePath(std::string filePath);
    bool removeInUseFlag(std::string filePath);
    void removeAllInUseFlags(void);

    void startEncoding(void);
    void stopEncoding(void);

    AVIOContext* createIO(const char *filePath);
    void closeIO(AVIOContext **pb);

    static void* encoder_callback(RequestType request, const void* param);
    static int io_write_callback(void *opaque, uint8_t *buf, int buf_size);
public:
    static const int WIDTH  = 640;
    static const int HEIGHT = 480;

    void initialize(const char *rootDir);

    void startRecord(void);
    void stopRecord(void);
    bool canAcceptFrame(void);
    void sendFrame(AVFrame* yuvFrame);
    void terminate(void);
};

#endif //PEOPLEWATCHER_ENCODER_H
