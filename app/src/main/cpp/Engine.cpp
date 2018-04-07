#include "Engine.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/pixfmt.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"

#include "imageUtils.h"
#include "log.h"
}

#include "FFmpegUtils.h"
#include "exceptionUtils.h"
#include <string>
#include <unistd.h>

#define ENGINE_TAG "PW_ENGINE"

#define WIDTH 640
#define HEIGHT 480
#define FRAME_BUFFER_SIZE 20 * 8 // 20 fps for 8 seconds (~73 MB buffer)

#define IO_BUFFER_SIZE 4096
#define IO_BUFFERS_COUNT 256

Engine::Engine() : pendingEncoderOperations(FRAME_BUFFER_SIZE), pendingIOOperations(IO_BUFFERS_COUNT * 2), freeBuffers(IO_BUFFERS_COUNT) {

}

void* Engine::encoder_thread_entrypoint(void* opaque) {

    Engine::getInstance().encoderThreadLoop();
    return NULL;
}

void* Engine::async_io_thread_entrypoint(void* opaque) {

    Engine::getInstance().asyncIOThreadLoop();
    return NULL;
}

void Engine::initialize(const char *sdCardPath) {

    if (this->initialized)
        return;

    setup_ffmpeg_log();

    avfilter_register_all();

    this->sdCardPath = std::string(sdCardPath);

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);

    pthread_create(&encoderThread, NULL, encoder_thread_entrypoint, NULL);

    pthread_mutex_lock(&mutex);
    pthread_create(&asyncIOThread, NULL, async_io_thread_entrypoint, NULL);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);

    this->initialized = 1;
}

void Engine::finalize(void) {

    encoderTerminate();

    asyncIOTerminate();

    print_log(ANDROID_LOG_INFO, ENGINE_TAG, "Engine is finalized");
}

void Engine::startRecord(void) {

    // for now - doing nothing for this method
}

void Engine::stopRecord(void) {

    FrameOperation operation = { };
    operation.operationType = CloseRecord;

    pendingEncoderOperations.enqueue(operation);
}

void Engine::encoderTerminate(void) {

    FrameOperation operation = { };
    operation.operationType = FinalizeEncoder;

    pendingEncoderOperations.enqueue(operation);

    pthread_join(encoderThread, NULL);
}

void Engine::sendFrame(uint8_t* dataY, uint8_t* dataU, uint8_t* dataV,
                       int strideY, int strideU, int strideV, long long timestamp) {

    if (this->pendingEncoderOperations.size_approx() < FRAME_BUFFER_SIZE) {

        AVFrame *yuv_frame = av_frame_alloc();
        yuv_frame->width = WIDTH;
        yuv_frame->height = HEIGHT;
        yuv_frame->format = AV_PIX_FMT_YUV420P;
        yuv_frame->pts = timestamp;
        av_check_error(av_frame_get_buffer(yuv_frame, 32));

        convert_yuv420_888_to_yuv420p(dataY, dataU, dataV, strideY, strideU, strideV, yuv_frame);

        FrameOperation operation = { };
        operation.frame = yuv_frame;
        operation.operationType = EncodeFrame;

        if (!pendingEncoderOperations.try_enqueue(operation)) {

            // why the fuck this happened?

            av_frame_free(&yuv_frame);

            throw new std::runtime_error("Couldn't enqueue YUV frame");
        }
    }
    else {
        print_log(ANDROID_LOG_WARN, ENGINE_TAG, "YUV frame drop");
    }
}

// Encoder Thread

void Engine::encoderThreadLoop(void) {

    long long startTime = 0;

    while (true) {

        FrameOperation operation;

        this->pendingEncoderOperations.wait_dequeue(operation);

        if (operation.operationType == EncodeFrame) {

            AVFrame *yuv_frame = operation.frame;

            // auto start
            if (startTime == 0) {

                startTime = yuv_frame->pts;

                startEncoding();
            }

            my_assert(yuv_frame->pts >= startTime);
            yuv_frame->pts -= startTime;

            encoder.writeFrame(yuv_frame);

        } else if (operation.operationType == CloseFile || operation.operationType == FinalizeEncoder) {

            if (startTime > 0) {

                stopEncoding();
                startTime = 0;
            }

            if (operation.operationType == FinalizeEncoder)
                break;
        }
    }

    if (this->pendingEncoderOperations.size_approx() > 0)
        print_log(ANDROID_LOG_WARN, ENGINE_TAG, "Encoder: Still have pending operations after finalization");

    if (this->io_buffer != NULL || this->io_file != NULL)
        print_log(ANDROID_LOG_WARN, ENGINE_TAG, "Encoder: IO Context wasn't closed after finalization");
}

void Engine::startEncoding(void) {

    encoder.startRecord(TestData, x264, WIDTH, HEIGHT, (this->sdCardPath + "/record.flv").c_str(),
                        asyncCreateIOCallback);
}

void Engine::stopEncoding(void) {

    encoder.closeRecord();
}

void Engine::asyncCreateIOCallback(const char* filePath, AVIOContext **pb) {

    Engine::getInstance().asyncCreateIO(filePath, pb);
}

void Engine::asyncCreateIO(const char* filePath, AVIOContext **pb) {

    if (filePath != NULL) {

        my_assert(io_buffer == NULL);

        io_file = fopen(filePath, "w");
        if (io_file == NULL) {
            asyncCloseIO();
            return;
        }

        io_buffer = av_mallocz(IO_BUFFER_SIZE + AV_INPUT_BUFFER_PADDING_SIZE);
        if (io_buffer == NULL) {
            asyncCloseIO();
            return;
        }

        *pb = avio_alloc_context((uint8_t *) io_buffer, IO_BUFFER_SIZE, 1, io_file, NULL,
                                 async_write_io, NULL);

        if (*pb == NULL) {
            asyncCloseIO();
            return;
        }
    } else {
        asyncCloseIO();
        avio_context_free(pb);
    }
}

void Engine::asyncCloseIO(void) {
    // free buffer
    av_freep(&this->io_buffer);

    // schedule file close
    if (io_file != NULL) {
        asyncCloseFile(io_file);
        io_file = NULL;
    }
}

int Engine::async_write_io(void *opaque, uint8_t *buf, int buf_size) {

    FILE *f = (FILE*) opaque;

    Engine::getInstance().asyncWrite(f, (void*) buf, (size_t) buf_size);

    return buf_size;
}

// Async I/O

void Engine::asyncWrite(FILE *f, void* buffer, size_t size) {

    // print_log(ANDROID_LOG_INFO, ENGINE_TAG, "async write %d", size);

    void* currentBuffer;
    if (!this->freeBuffers.try_dequeue(currentBuffer)) {

        print_log(ANDROID_LOG_WARN, ENGINE_TAG, "Ran out of async buffers, have to wait");

        this->freeBuffers.wait_dequeue(currentBuffer);
    }

    memcpy(currentBuffer, buffer, size);

    AsyncIOOperation operation = { };
    operation.operationType = Write;
    operation.file = f;
    operation.buffer = currentBuffer;
    operation.size = size;

    pendingIOOperations.enqueue(operation);
}

void Engine::asyncCloseFile(FILE *f) {

    AsyncIOOperation operation = { };
    operation.operationType = CloseFile;
    operation.file = f;

    pendingIOOperations.enqueue(operation);
}

void Engine::asyncIOTerminate(void) {

    AsyncIOOperation operation = { };
    operation.operationType = FinalizeIO;

    pendingIOOperations.enqueue(operation);

    pthread_join(asyncIOThread, NULL);
}

void Engine::allocateBuffers(void) {

    for (int counter = 0; counter < IO_BUFFERS_COUNT; counter++) {

        uint8_t* buffer = (uint8_t*) malloc(IO_BUFFER_SIZE);

        freeBuffers.enqueue(buffer);
    }
}

void Engine::asyncIOThreadLoop(void) {

    pthread_mutex_lock(&mutex);
    allocateBuffers();
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);

    while (true) {

        AsyncIOOperation operation;

        this->pendingIOOperations.wait_dequeue(operation);

        if (operation.operationType == Write) {

            size_t ret = fwrite(operation.buffer, 1, operation.size, operation.file);
            if (ret != operation.size)
                throw new std::runtime_error("Async IO write failed");

            this->freeBuffers.enqueue(operation.buffer);

        } else if (operation.operationType == CloseFile) {

            fclose(operation.file);

        } else if (operation.operationType == FinalizeIO)
            break;
    }

    if (this->pendingIOOperations.size_approx() > 0)
        print_log(ANDROID_LOG_WARN, ENGINE_TAG, "Async IO have pending operations after finalization");
}