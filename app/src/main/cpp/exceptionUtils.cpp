#include "exceptionUtils.h"

#include <stdexcept>
#include <ios>
#include <jni.h>

#include "coffeecatch.h"
#include "coffeejni.h"

//This is how we represent a Java exception already in progress
struct ThrownJavaException : std::runtime_error {
    ThrownJavaException() :std::runtime_error("") {}
    ThrownJavaException(const std::string& msg ) :std::runtime_error(msg) {}
};

//used to throw a new Java exception. use full paths like:
//"java/lang/NoSuchFieldException"
//"java/lang/NullPointerException"
//"java/security/InvalidParameterException"
struct NewJavaException : public ThrownJavaException{
    NewJavaException(JNIEnv * env, const char* type="", const char* message="")
            : ThrownJavaException(type + std::string(" ") + message)
    {
        jclass newExcCls = env->FindClass(type);
        if (newExcCls != NULL)
            env->ThrowNew(newExcCls, message);
        //if it is null, a NoClassDefFoundError was already thrown
    }
};

void assert_no_exception(JNIEnv *env) {

    if (env->ExceptionCheck()==JNI_TRUE)
        throw ThrownJavaException("assert_no_exception");
}

void swallow_cpp_exception_and_throw_java(JNIEnv *env) {

    try {
        throw;
    } catch(const ThrownJavaException&) {
        //already reported to Java, ignore
    } catch(const std::bad_alloc& rhs) {
        //translate OOM C++ exception to a Java exception
        NewJavaException(env, "java/lang/OutOfMemoryError", rhs.what());
    } catch(const std::ios_base::failure& rhs) { //sample translation
        //translate IO C++ exception to a Java exception
        NewJavaException(env, "java/io/IOException", rhs.what());

        //TRANSLATE ANY OTHER C++ EXCEPTIONS TO JAVA EXCEPTIONS HERE

    } catch(const std::exception& e) {
        //translate unknown C++ exception to a Java exception
        NewJavaException(env, "java/lang/Error", e.what());
    } catch(...) {
        //translate unknown C++ exception to a Java exception
        NewJavaException(env, "java/lang/Error", "Unknown exception type");
    }
}

void my_assert(bool condition) {

    if (!condition)
        raise(SIGABRT);
}