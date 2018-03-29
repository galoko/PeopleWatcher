#include <jni.h>

#include "coffeecatch.h"
#include "coffeejni.h"

#include "exceptionUtils.h"

#include "Engine.h"

extern "C" JNIEXPORT void JNICALL Java_com_galover_media_peoplewatcher_EngineManager_initialize(
        JNIEnv *env, jclass /*this*/, jstring sdCardPath) {

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
        JNIEnv *env, jclass /*this*/, jlong timestamp) {
    try {
        COFFEE_TRY() {

            Engine::getInstance().startRecord(timestamp);

        } COFFEE_CATCH() {
            coffeecatch_throw_exception(env);
        } COFFEE_END();
    } catch(...) {
        swallow_cpp_exception_and_throw_java(env);
    }
}

extern "C" JNIEXPORT void JNICALL Java_com_galover_media_peoplewatcher_EngineManager_sendFrame(
        JNIEnv *env, jclass /*this*/,
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