#include <jni.h>

#include "coffeecatch.h"
#include "coffeejni.h"

#include "exceptionUtils.h"

#include "Engine.h"

extern "C" JNIEXPORT void JNICALL Java_com_galover_media_peoplewatcher_EngineManager_initializeEngine(
        JNIEnv *env, jobject /*this*/, jstring rootDir) {

    try {
        COFFEE_TRY() {

            const char *rootDirStr = env->GetStringUTFChars(rootDir, JNI_FALSE);

            Engine::getInstance().initialize(rootDirStr);

            env->ReleaseStringUTFChars(rootDir, rootDirStr);

        } COFFEE_CATCH() {
            coffeecatch_throw_exception(env);
        } COFFEE_END();
    } catch(...) {
        swallow_cpp_exception_and_throw_java(env);
    }
}

extern "C" JNIEXPORT void JNICALL Java_com_galover_media_peoplewatcher_EngineManager_finalizeEngine(
        JNIEnv *env, jobject /*this*/) {

    try {
        COFFEE_TRY() {

            Engine::getInstance().finalize();

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

extern "C" JNIEXPORT void JNICALL Java_com_galover_media_peoplewatcher_EngineManager_stopRecord(
        JNIEnv *env, jobject /*this*/) {
    try {
        COFFEE_TRY() {

            Engine::getInstance().stopRecord();

        } COFFEE_CATCH() {
            coffeecatch_throw_exception(env);
        } COFFEE_END();
    } catch(...) {
        swallow_cpp_exception_and_throw_java(env);
    }
}