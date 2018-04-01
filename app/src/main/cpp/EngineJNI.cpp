#include <jni.h>

#include "coffeecatch.h"
#include "coffeejni.h"

#include "exceptionUtils.h"

#include "Engine.h"

extern "C" JNIEXPORT void JNICALL Java_com_galover_media_peoplewatcher_EngineManager_initialize(
        JNIEnv *env, jobject /*this*/, jstring sdCardPath) {

    try {
        COFFEE_TRY() {

            const char *sdCardPathStr = env->GetStringUTFChars(sdCardPath, JNI_FALSE);

            Engine::getInstance().initialize(sdCardPathStr);

            env->ReleaseStringUTFChars(sdCardPath, sdCardPathStr);

        } COFFEE_CATCH() {
            coffeecatch_throw_exception(env);
        } COFFEE_END();
    } catch(...) {
        swallow_cpp_exception_and_throw_java(env);
    }
}

extern "C" JNIEXPORT void JNICALL Java_com_galover_media_peoplewatcher_EngineManager_startRecord(
        JNIEnv *env, jobject /*this*/) {
    try {
        COFFEE_TRY() {

            Engine::getInstance().startRecord();

        } COFFEE_CATCH() {
            coffeecatch_throw_exception(env);
        } COFFEE_END();
    } catch(...) {
        swallow_cpp_exception_and_throw_java(env);
    }
}

extern "C" JNIEXPORT void JNICALL Java_com_galover_media_peoplewatcher_EngineManager_sendFrame(
        JNIEnv *env, jobject /*this*/,
        jobject Y, jobject U, jobject V,
        jint strideY, jint strideU, jint strideV,
        jlong timestamp) {

    try {
        COFFEE_TRY() {

            uint8_t* dataY = (uint8_t *) env->GetDirectBufferAddress(Y);
            uint8_t* dataU = (uint8_t *) env->GetDirectBufferAddress(U);
            uint8_t* dataV = (uint8_t *) env->GetDirectBufferAddress(V);

            Engine::getInstance().sendFrame(dataY, dataU, dataV, strideY, strideU, strideV, timestamp);

        } COFFEE_CATCH() {
            coffeecatch_throw_exception(env);
        } COFFEE_END();
    } catch(...) {
        swallow_cpp_exception_and_throw_java(env);
    }
}

extern "C" JNIEXPORT void JNICALL Java_com_galover_media_peoplewatcher_EngineManager_workerThreadLoop(
        JNIEnv *env, jobject /*this*/) {
    try {
        COFFEE_TRY() {

            Engine::getInstance().workerThreadLoop(env);

        } COFFEE_CATCH() {
            coffeecatch_throw_exception(env);
        } COFFEE_END();
    } catch(...) {
        swallow_cpp_exception_and_throw_java(env);
    }
}