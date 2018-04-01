#include "generalUtils.h"

#include <time.h>

#define BILLION 1E9

double getTime(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_sec + time.tv_nsec / BILLION;
}
