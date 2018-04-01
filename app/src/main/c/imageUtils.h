#ifndef PEOPLEWATCHER_IMAGEUTILS_H
#define PEOPLEWATCHER_IMAGEUTILS_H

#include <inttypes.h>

#include "libavutil/frame.h"

int dumpBMP24(uint8_t* pixels, int width, int height, const char* filePath);
int dumpBMP8(uint8_t* pixels, int width, int height, const char* filePath);
int dumpYUV420(AVFrame *yuv_frame, const char* filePath);

void convert_yuv420_888_to_yuv420p(uint8_t* dataY, uint8_t* dataU, uint8_t* dataV, int strideY,
                                   int strideU, int strideV, AVFrame *dst);

void benchmark_convert_yuv420_888_to_yuv420p(void);

#endif //PEOPLEWATCHER_IMAGEUTILS_H
