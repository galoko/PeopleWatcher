#ifndef PEOPLEWATCHER_ENGINE_H
#define PEOPLEWATCHER_ENGINE_H

#include <string>
#include <inttypes.h>
#include <jni.h>

#include "readerwriterqueue.h"
#include "FFmpegUtils.h"

extern "C" {
#include "libavutil/frame.h"
#include "libavformat/avio.h"
}

using namespace moodycamel;

enum AsyncIOOperationType {
    Write,
    CloseFile,
    FinalizeIO
};

struct AsyncIOOperation {
    FILE *file;
    AsyncIOOperationType operationType;
    void *buffer;
    size_t size;
};

enum FrameOperationType {
    EncodeFrame,
    CloseRecord,
    FinalizeEncoder
};

struct FrameOperation {
    AVFrame *frame;
    FrameOperationType  operationType;
};

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

    BlockingReaderWriterQueue<FrameOperation> pendingEncoderOperations;

    BlockingReaderWriterQueue<AsyncIOOperation> pendingIOOperations;
    BlockingReaderWriterQueue<void*> freeBuffers;

    FFmpegEncoder encoder;
    FILE *io_file;
    void* io_buffer;

    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_t encoderThread, asyncIOThread;

    void startEncoding(void);
    void stopEncoding(void);

    static void* encoder_thread_entrypoint(void* opaque);
    static void* async_io_thread_entrypoint(void* opaque);

    void encoderThreadLoop(void);
    void encoderTerminate(void);

    static void asyncCreateIOCallback(const char* filePath, AVIOContext **pb);
    void asyncCreateIO(const char* filePath, AVIOContext **pb);
    void asyncCloseIO(void);
    static int async_write_io(void *opaque, uint8_t *buf, int buf_size);

    void allocateBuffers(void);
    void asyncIOThreadLoop(void);
    void asyncWrite(FILE *f, void* buffer, size_t size);
    void asyncCloseFile(FILE *f);
    void asyncIOTerminate(void);

    Engine();
public:
    Engine(Engine const&)          = delete;
    void operator=(Engine const&)  = delete;

    void initialize(const char* sdCardPathStr);
    void finalize(void);

    void startRecord(void);
    void stopRecord(void);
    void sendFrame(uint8_t* dataY, uint8_t* dataU, uint8_t* dataV,
                   int strideY, int strideU, int strideV, long long timestamp);
};

#endif //PEOPLEWATCHER_ENGINE_H
