#include "imageUtils.h"

#include "log.h"
#include <time.h>

#include "libswscale/swscale.h"

#include "generalUtils.h"

int dumpBMP24(uint8_t* pixels, int width, int height, const char* filePath) {

    FILE *bmpFile;
    int fileSize = 54 + 3 * width * height;

    unsigned char bmpfileheader[14] = {'B','M', 0,0,0,0, 0,0, 0,0, 54,0,0,0};
    unsigned char bmpinfoheader[40] = {40,0,0,0, 0,0,0,0, 0,0,0,0, 1,0, 24,0};
    unsigned char bmppad[3] = {0,0,0};

    bmpfileheader[ 2] = (unsigned char)(fileSize    );
    bmpfileheader[ 3] = (unsigned char)(fileSize>> 8);
    bmpfileheader[ 4] = (unsigned char)(fileSize>>16);
    bmpfileheader[ 5] = (unsigned char)(fileSize>>24);

    bmpinfoheader[ 4] = (unsigned char)(       width    );
    bmpinfoheader[ 5] = (unsigned char)(       width>> 8);
    bmpinfoheader[ 6] = (unsigned char)(       width>>16);
    bmpinfoheader[ 7] = (unsigned char)(       width>>24);
    bmpinfoheader[ 8] = (unsigned char)(       height    );
    bmpinfoheader[ 9] = (unsigned char)(       height>> 8);
    bmpinfoheader[10] = (unsigned char)(       height>>16);
    bmpinfoheader[11] = (unsigned char)(       height>>24);

    bmpFile = fopen(filePath, "wb");
    fwrite(bmpfileheader, 1, sizeof(bmpfileheader), bmpFile);
    fwrite(bmpinfoheader, 1, sizeof(bmpinfoheader), bmpFile);
    for(int i=0; i<height; i++)
    {
        fwrite(pixels + (width * (height - i - 1) * 3), 3, (size_t) width, bmpFile);
        fwrite(bmppad, 1, (size_t) ((4 - (width * 3) % 4) % 4),bmpFile);
    }

    fclose(bmpFile);

    return 1;
}

int dumpBMP8(uint8_t* pixels, int width, int height, const char* filePath) {

    uint8_t* pixels24 = malloc((size_t) (width * height * 3));
    if (!pixels24)
        return 0;

    for (int i = 0; i < width * height; i++){
        pixels24[i * 3 + 0] = pixels[i];
        pixels24[i * 3 + 1] = pixels[i];
        pixels24[i * 3 + 2] = pixels[i];
    }

    int res = dumpBMP24(pixels24, width, height, filePath);

    free(pixels24);

    return res;
}

int dumpYUV420(AVFrame *yuv_frame, const char* filePath) {

    int res = 0;

    AVFrame* rgb_frame = av_frame_alloc();
    rgb_frame->width = yuv_frame->width;
    rgb_frame->height = yuv_frame->height;
    rgb_frame->format = AV_PIX_FMT_BGR24;
    if (av_frame_get_buffer(rgb_frame, 32) < 0)
        goto exit;

    struct SwsContext* ctx = sws_getContext(yuv_frame->width, yuv_frame->height, (enum AVPixelFormat) yuv_frame->format,
                                     rgb_frame->width, rgb_frame->height, (enum AVPixelFormat) rgb_frame->format,
                                     SWS_POINT, NULL, NULL, NULL);
    if (!ctx)
        goto exit;

    int ret = sws_scale(ctx, ((const uint8_t *const*) &yuv_frame->data), yuv_frame->linesize, 0,
                        yuv_frame->height, rgb_frame->data, rgb_frame->linesize);
    if (ret < 0)
        goto exit;

    res = dumpBMP24(rgb_frame->data[0], rgb_frame->width, rgb_frame->height, filePath);

    exit:

    if (ctx)
        sws_freeContext(ctx);

    av_frame_free(&rgb_frame);

    return res;
}

void convert_yuv420_888_to_yuv420p(uint8_t* dataY, uint8_t* dataU, uint8_t* dataV, int strideY,
                                   int strideU, int strideV, AVFrame *dst) {

    uint8_t* srcY;
    uint8_t* srcU;
    uint8_t* srcV;
    uint8_t* dstY;
    uint8_t* dstU;
    uint8_t* dstV;

    srcY = dataY;
    srcU = dataU;
    srcV = dataV;

    dstY = dst->data[0];
    dstU = dst->data[1];
    dstV = dst->data[2];

    if (dst->linesize[0] == strideY) {
        memcpy(dstY, srcY, (size_t) strideY * dst->height);
    }
    else {
        for (int y = 0; y < dst->height; y++) {

            memcpy(dstY, srcY, (size_t) dst->width);

            srcY += strideY;
            dstY += dst->linesize[0];
        }
    }

    for (int y = 0; y < dst->height / 2; y++) {

        for (int x = 0; x < dst->width / 2; x++) {

            dstU[x] = srcU[x * 2];
            dstV[x] = srcV[x * 2];
        }

        srcU += strideU;
        srcV += strideV;

        dstU += dst->linesize[1];
        dstV += dst->linesize[2];
    }
}

#define BENCHMARK_TAG "PW_BENCHMARK"

#pragma clang optimize off

#define WIDTH 640
#define HEIGHT 480

void benchmark_convert_yuv420_888_to_yuv420p(void) {

    uint8_t* Y = malloc(WIDTH * HEIGHT);
    uint8_t* U = malloc(WIDTH * HEIGHT);
    uint8_t* V = malloc(WIDTH * HEIGHT);

    AVFrame* yuv_frame = av_frame_alloc();
    yuv_frame->width = WIDTH;
    yuv_frame->height = HEIGHT;
    yuv_frame->format = AV_PIX_FMT_YUV420P;
    if (av_frame_get_buffer(yuv_frame, 32) < 0)
        return;

    for (int times = 0; times < 10; times++) {
        double startTime = getTime();
        for (int counter = 0; counter < 1000; counter++)
            convert_yuv420_888_to_yuv420p(Y, U, V, WIDTH, WIDTH, WIDTH, yuv_frame);

        double elapsed = getTime() - startTime;

        print_log(ANDROID_LOG_INFO, BENCHMARK_TAG, "%f", elapsed);
    }

    av_frame_free(&yuv_frame);

    free(V);
    free(U);
    free(Y);
}

#pragma clang optimize on