#include "AsyncIO.h"

#include "log.h"
#include "exceptionUtils.h"

#include <cstring>

#define ASYNC_IO_TAG "PW_ASYNC_IO"

#define IO_BUFFERS_COUNT 256

AsyncIO::AsyncIO(void) :
    pendingOperations(IO_BUFFERS_COUNT * 2),
    freeBuffers(IO_BUFFERS_COUNT) {
}

void AsyncIO::initialize(void) {

    if (this->initialized)
        return;

    pthread_check_error(pthread_mutex_init(&mutex, NULL));
    pthread_check_error(pthread_cond_init(&cond, NULL));

    pthread_check_error(pthread_mutex_lock(&mutex));
    pthread_check_error(pthread_create(&thread, NULL, thread_entrypoint, NULL));
    pthread_check_error(pthread_cond_wait(&cond, &mutex));
    pthread_check_error(pthread_mutex_unlock(&mutex));

    this->initialized = 1;
}

void AsyncIO::allocateBuffers(void) {

    for (int counter = 0; counter < IO_BUFFERS_COUNT; counter++) {

        uint8_t* buffer = (uint8_t*) malloc(IO_BUFFER_SIZE);

        freeBuffers.enqueue(buffer);
    }
}

// thread

void* AsyncIO::thread_entrypoint(void* opaque) {

    AsyncIO::getInstance().threadLoop();
    return NULL;
}

void AsyncIO::threadLoop(void) {

    pthread_check_error(pthread_mutex_lock(&mutex));
    allocateBuffers();
    pthread_check_error(pthread_cond_signal(&cond));
    pthread_check_error(pthread_mutex_unlock(&mutex));

    while (true) {

        AsyncIOOperation operation;

        this->pendingOperations.wait_dequeue(operation);

        if (operation.operationType == Write) {

            size_t ret = fwrite(operation.buffer, 1, operation.size, operation.file);
            if (ret != operation.size)
                throw new std::runtime_error("Async IO write failed");

            print_log(ANDROID_LOG_DEBUG, ASYNC_IO_TAG, "written: %d", ret);

            this->freeBuffers.enqueue(operation.buffer);

        } else if (operation.operationType == CloseFile) {

            fclose(operation.file);

        } else if (operation.operationType == FinalizeIO)
            break;
    }

    if (this->pendingOperations.size_approx() > 0)
        print_log(ANDROID_LOG_WARN, ASYNC_IO_TAG, "Async IO have pending operations after finalization");
}

// async API, they send commands to the encoder thread

void AsyncIO::write(FILE *f, void* buffer, size_t size) {

    if (size > IO_BUFFER_SIZE)
        throw new std::runtime_error("Size sent to write operation is larger than buffer size");

    void* currentBuffer;
    if (!this->freeBuffers.try_dequeue(currentBuffer)) {

        print_log(ANDROID_LOG_WARN, ASYNC_IO_TAG, "Ran out of async buffers, have to wait");

        this->freeBuffers.wait_dequeue(currentBuffer);
    }

    memcpy(currentBuffer, buffer, size);

    AsyncIOOperation operation = { };
    operation.operationType = Write;
    operation.file = f;
    operation.buffer = currentBuffer;
    operation.size = size;

    pendingOperations.enqueue(operation);
}

void AsyncIO::closeFile(FILE **f) {

    if (*f) {
        AsyncIOOperation operation = {};
        operation.operationType = CloseFile;
        operation.file = *f;

        pendingOperations.enqueue(operation);

        *f = NULL;
    }
}

void AsyncIO::terminate(void) {

    AsyncIOOperation operation = { };
    operation.operationType = FinalizeIO;

    pendingOperations.enqueue(operation);

    pthread_check_error(pthread_join(thread, NULL));
}