#ifndef PEOPLEWATCHER_EXCEPTIONUTILS_H
#define PEOPLEWATCHER_EXCEPTIONUTILS_H

#include <jni.h>

inline void assert_no_exception(JNIEnv *env);
void swallow_cpp_exception_and_throw_java(JNIEnv *env);
void my_assert(bool condition);
void pthread_check_error(int ret);

#endif //PEOPLEWATCHER_EXCEPTIONUTILS_H