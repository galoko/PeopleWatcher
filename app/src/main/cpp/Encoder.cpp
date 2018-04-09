#include "Encoder.h"

#include "log.h"
#include "exceptionUtils.h"

#include "AsyncIO.h"

#define ENCODER_TAG "PW_ENCODER"

#define FRAME_BUFFER_SIZE 20 * 3 // 20 fps for 3 seconds (~28 MB buffer)

Encoder::Encoder(void) : pendingOperations(FRAME_BUFFER_SIZE) {
}

void Encoder::initialize(const char *sdCardPath) {

    if (this->initialized)
        return;

    this->sdCardPath = std::string(sdCardPath);

    setup_ffmpeg_log();

    avfilter_register_all();

    pthread_check_error(pthread_create(&thread, NULL, thread_entrypoint, NULL));

    this->initialized = 1;
}

// async API, they send commands to the encoder thread

void Encoder::startRecord(void) {

    EncoderOperation operation = { };
    operation.operationType = StartRecord;

    pendingOperations.enqueue(operation);
}

void Encoder::stopRecord(void) {

    EncoderOperation operation = { };
    operation.operationType = CloseRecord;

    pendingOperations.enqueue(operation);

    // print_log(ANDROID_LOG_INFO, ENCODER_TAG, "CloseRecord operation sent");
}

bool Encoder::canAcceptFrame(void) {

    return this->pendingOperations.size_approx() < FRAME_BUFFER_SIZE;
}

void Encoder::sendFrame(AVFrame* yuvFrame) {

    EncoderOperation operation = { };
    operation.operationType = EncodeFrame;
    operation.frame = yuvFrame;

    if (!pendingOperations.try_enqueue(operation)) {

        print_log(ANDROID_LOG_WARN, ENCODER_TAG, "Frame drop");

        av_frame_free(&yuvFrame);
    } else {
        // print_log(ANDROID_LOG_INFO, ENCODER_TAG, "EncodeFrame operation sent");
    }
}

void Encoder::terminate(void) {

    EncoderOperation operation = { };
    operation.operationType = FinalizeEncoder;

    pendingOperations.enqueue(operation);

    // print_log(ANDROID_LOG_INFO, ENCODER_TAG, "FinalizeEncoder operation sent");

    pthread_check_error(pthread_join(thread, NULL));
}

// start/close implementations
// TODO make real name for records. name could consist of something like current date plus flags

void Encoder::startEncoding(void) {

    encoder.startRecord(Record, MediaCodec, WIDTH, HEIGHT,
                        (this->sdCardPath + "/record.flv").c_str(),
                        encoder_callback);
}

void Encoder::stopEncoding(void) {

    encoder.closeRecord();
}

// thread

void* Encoder::thread_entrypoint(void* opaque) {

    Encoder::getInstance().threadLoop();
    return NULL;
}

void Encoder::threadLoop(void) {

    long long startTime = 0;
    bool recordStarted = false;

    while (true) {

        EncoderOperation operation;

        this->pendingOperations.wait_dequeue(operation);

        /*
        if (operation.operationType == EncodeFrame) {
            print_log(ANDROID_LOG_INFO, ENCODER_TAG, "EncodeFrame operation recved");
        } else if (operation.operationType == CloseRecord) {
            print_log(ANDROID_LOG_INFO, ENCODER_TAG, "CloseRecord operation recved");
        } else if (operation.operationType == FinalizeEncoder) {
            print_log(ANDROID_LOG_INFO, ENCODER_TAG, "FinalizeEncoder operation recved");
        }
         */

        if (operation.operationType == StartRecord) {

            if (!recordStarted) {
                my_assert(startTime == 0);

                startEncoding();

                recordStarted = true;
            } else {
                print_log(ANDROID_LOG_WARN, ENCODER_TAG, "Record is already started");
            }

        } else if (operation.operationType == EncodeFrame) {

            AVFrame *yuvFrame = operation.frame;

            if (recordStarted) {

                if (startTime == 0)
                    startTime = yuvFrame->pts;

                my_assert(yuvFrame->pts >= startTime);
                yuvFrame->pts -= startTime;

                encoder.writeFrame(yuvFrame);
            } else {

                print_log(ANDROID_LOG_WARN, ENCODER_TAG, "Framed drop because record isn't started");

                av_frame_free(&yuvFrame);
            }

        } else if (operation.operationType == CloseRecord || operation.operationType == FinalizeEncoder) {

            if (recordStarted) {

                stopEncoding();

                startTime = 0;
                recordStarted = false;
            }

            if (operation.operationType == FinalizeEncoder)
                break;
        }
    }

    if (this->pendingOperations.size_approx() > 0)
        print_log(ANDROID_LOG_WARN, ENCODER_TAG, "Encoder: Still have pending operations after finalization");

    if (this->io_buffer != NULL || this->io_file != NULL)
        print_log(ANDROID_LOG_WARN, ENCODER_TAG, "Encoder: IO Context wasn't closed after finalization");
}

// async IO part

AVIOContext* Encoder::createIO(const char *filePath) {

    print_log(ANDROID_LOG_INFO, ENCODER_TAG, "creating io");

    my_assert(io_buffer == NULL);

    io_file = fopen(filePath, "w");
    if (io_file == NULL) {
        closeIO(NULL);
        return NULL;
    }

    size_t io_buffer_size = AsyncIO::IO_BUFFER_SIZE;

    io_buffer = av_mallocz(io_buffer_size + AV_INPUT_BUFFER_PADDING_SIZE);
    if (io_buffer == NULL) {
        closeIO(NULL);
        return NULL;
    }

    AVIOContext* pb = avio_alloc_context((uint8_t *) io_buffer, io_buffer_size, 1, io_file, NULL,
                             io_write_callback, NULL);

    if (pb == NULL) {
        closeIO(NULL);
        return pb;
    }

    print_log(ANDROID_LOG_INFO, ENCODER_TAG, "io created");

    return pb;
}

void Encoder::closeIO(AVIOContext **pb) {

    print_log(ANDROID_LOG_INFO, ENCODER_TAG, "closing io");

    // free context
    if (pb != NULL)
        avio_context_free(pb);

    // free buffer
    av_freep(&this->io_buffer);

    // schedule file close
    AsyncIO::getInstance().closeFile(&io_file);

    print_log(ANDROID_LOG_INFO, ENCODER_TAG, "io closed");
}

// static C-like callbacks


void* Encoder::encoder_callback(RequestType request, const void* param) {

    switch (request) {
        case CreateIO: {
            return Encoder::getInstance().createIO((const char*) param);
        };
        case CloseIO: {
            Encoder::getInstance().closeIO((AVIOContext**) param);
            return NULL;
        };
    }
}

int Encoder::io_write_callback(void *opaque, uint8_t *buf, int buf_size) {

    FILE *f = (FILE*) opaque;

    AsyncIO::getInstance().write(f, (void*) buf, (size_t) buf_size);

    return buf_size;
}