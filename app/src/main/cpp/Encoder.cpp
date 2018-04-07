#include "Encoder.h"

extern "C" {
#include "imageUtils.h"
}

#include "log.h"
#include "exceptionUtils.h"

#include "AsyncIO.h"

#define ENCODER_TAG "PW_ENCODER"

#define WIDTH 640
#define HEIGHT 480
#define FRAME_BUFFER_SIZE 20 * 8 // 20 fps for 8 seconds (~73 MB buffer)

Encoder::Encoder(void) : pendingEncoderOperations(FRAME_BUFFER_SIZE) {
}

void Encoder::initialize(const char *sdCardPath) {

    if (this->initialized)
        return;

    this->sdCardPath = std::string(sdCardPath);

    setup_ffmpeg_log();

    avfilter_register_all();

    pthread_create(&thread, NULL, thread_entrypoint, NULL);

    this->initialized = 1;
}

// async API, they send commands to the encoder thread

void Encoder::startRecord(void) {

    // for now - doing nothing for this method
}

void Encoder::stopRecord(void) {

    FrameOperation operation = { };
    operation.operationType = CloseRecord;

    pendingEncoderOperations.enqueue(operation);
}

void Encoder::sendFrame(uint8_t* dataY, uint8_t* dataU, uint8_t* dataV,
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
        print_log(ANDROID_LOG_WARN, ENCODER_TAG, "YUV frame drop");
    }
}

void Encoder::terminate(void) {

    FrameOperation operation = { };
    operation.operationType = FinalizeEncoder;

    pendingEncoderOperations.enqueue(operation);

    pthread_join(thread, NULL);
}

// start/close implementations
// TODO make real name for records. name could consist of something like current date plus flags

void Encoder::startEncoding(void) {

    encoder.startRecord(TestData, x264, WIDTH, HEIGHT, (this->sdCardPath + "/record.flv").c_str(),
                        io_callback_create);
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

        } else if (operation.operationType == CloseRecord || operation.operationType == FinalizeEncoder) {

            if (startTime > 0) {

                stopEncoding();
                startTime = 0;
            }

            if (operation.operationType == FinalizeEncoder)
                break;
        }
    }

    if (this->pendingEncoderOperations.size_approx() > 0)
        print_log(ANDROID_LOG_WARN, ENCODER_TAG, "Encoder: Still have pending operations after finalization");

    if (this->io_buffer != NULL || this->io_file != NULL)
        print_log(ANDROID_LOG_WARN, ENCODER_TAG, "Encoder: IO Context wasn't closed after finalization");
}

// async IO part

void Encoder::createIO(const char *filePath, AVIOContext **pb) {

    if (filePath != NULL) {

        my_assert(io_buffer == NULL);

        io_file = fopen(filePath, "w");
        if (io_file == NULL) {
            closeIO();
            return;
        }

        size_t io_buffer_size = AsyncIO::IO_BUFFER_SIZE;

        io_buffer = av_mallocz(io_buffer_size + AV_INPUT_BUFFER_PADDING_SIZE);
        if (io_buffer == NULL) {
            closeIO();
            return;
        }

        *pb = avio_alloc_context((uint8_t *) io_buffer, io_buffer_size, 1, io_file, NULL,
                                 io_callback_write, NULL);

        if (*pb == NULL) {
            closeIO();
            return;
        }
    } else {
        closeIO();
        avio_context_free(pb);
    }
}

void Encoder::closeIO(void) {
    // free buffer
    av_freep(&this->io_buffer);

    // schedule file close
    AsyncIO::getInstance().closeFile(&io_file);
}

// static C-like callbacks

void Encoder::io_callback_create(const char *filePath, AVIOContext **pb) {

    Encoder::getInstance().createIO(filePath, pb);
}

int Encoder::io_callback_write(void *opaque, uint8_t *buf, int buf_size) {

    FILE *f = (FILE*) opaque;

    AsyncIO::getInstance().write(f, (void*) buf, (size_t) buf_size);

    return buf_size;
}