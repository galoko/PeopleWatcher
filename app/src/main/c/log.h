#ifndef PEOPLEWATCHER_LOG_H
#define PEOPLEWATCHER_LOG_H

#include <android/log.h>

#define print_log(level, tag, ...) __android_log_print(level, tag, __VA_ARGS__);

#endif //PEOPLEWATCHER_LOG_H
