#ifndef PEOPLEWATCHER_IMAGEUTILS_H
#define PEOPLEWATCHER_IMAGEUTILS_H

#include <inttypes.h>

extern "C" {
#include "libavutil/frame.h"
};

void dumpBMP24(uint8_t* pixels, int width, int height, char* filePath);

void convert_yuv420_888_to_yuv420p(uint8_t* dataY, uint8_t* dataU, uint8_t* dataV, int strideY,
                                   int strideU, int strideV, AVFrame *dst);

void benchmark_convert_yuv420_888_to_yuv420p(void);

#endif //PEOPLEWATCHER_IMAGEUTILS_H
