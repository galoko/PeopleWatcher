#ifndef PEOPLEWATCHER_ASYNCIO_H
#define PEOPLEWATCHER_ASYNCIO_H

#include <pthread.h>

#include "readerwriterqueue.h"

using namespace moodycamel;

class AsyncIO {
public:
    static AsyncIO &getInstance() {
        static AsyncIO instance;

        return instance;
    }
    
    AsyncIO(AsyncIO const&) = delete;
    void operator=(AsyncIO const&)  = delete;
private:
    AsyncIO(void);

    enum AsyncIOOperationType {
        Write,
        CloseFile,
        FinalizeIO
    };

    struct AsyncIOOperation {
        AsyncIOOperationType operationType;
        FILE *file;
        void *buffer;
        size_t size;
    };

    int initialized;

    BlockingReaderWriterQueue<AsyncIOOperation> pendingOperations;
    BlockingReaderWriterQueue<void*> freeBuffers;

    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_t thread;

    static void* thread_entrypoint(void* opaque);
    void threadLoop(void);

    void allocateBuffers(void);
public:
    static const int IO_BUFFER_SIZE = 4096;

    void initialize(void);

    void write(FILE *f, void* buffer, size_t size);
    void closeFile(FILE **f);

    void terminate(void);
};

#endif //PEOPLEWATCHER_ASYNCIO_H
