#ifndef JNIGLUE_H
#define JNIGLUE_H

#include "log.h"

class JNIGlue {
   public:
    JNIGlue(JavaVM* vm) {
        vm->GetEnv((void**)&_env, JNI_VERSION_1_6);
        _env->PushLocalFrame(100);
    }

    ~JNIGlue() {
        _env->PopLocalFrame(NULL);
    }

    jobject callObjectMethod(jclass clazz, const char* method, const char* signature, jobject instance, ...) {
        jmethodID methodID = _env->GetMethodID(clazz, method, signature);
        va_list args;
        va_start(args, instance);
        jobject result = _env->CallObjectMethodV(instance, methodID, args);
        va_end(args);
        checkException();
        return result;
    }

    jobject callObjectMethod(const char* className, const char* method, const char* signature, jobject instance, ...) {
        jmethodID methodID = getMethod(className, method, signature);
        va_list args;
        va_start(args, instance);
        jobject result = _env->CallObjectMethodV(instance, methodID, args);
        va_end(args);
        checkException();
        return result;
    }

    int callIntMethod(const char* className, const char* method, const char* signature, jobject instance, ...) {
        jmethodID methodID = getMethod(className, method, signature);
        va_list args;
        va_start(args, instance);
        int result = _env->CallIntMethodV(instance, methodID, args);
        va_end(args);
        checkException();
        return result;
    }

    void callVoidMethod(jobject instance, const char* method, const char* signature, ...) {
        jclass clazz = _env->GetObjectClass(instance);
        jmethodID methodID = _env->GetMethodID(clazz, method, signature);
        va_list args;
        va_start(args, signature);
        _env->CallVoidMethodV(instance, methodID, args);
        va_end(args);
        checkException();
    }

    void callVoidMethod(const char* className, const char* method, const char* signature, jobject instance, ...) {
        jmethodID methodID = getMethod(className, method, signature);
        va_list args;
        va_start(args, instance);
        _env->CallVoidMethodV(instance, methodID, args);
        va_end(args);
        checkException();
    }

    void callStaticVoidMethod(jclass clazz, const char* method, const char* signature, ...) {
        jmethodID methodID = _env->GetMethodID(clazz, method, signature);
        va_list args;
        va_start(args, signature);
        _env->CallStaticVoidMethodV(clazz, methodID, args);
        va_end(args);
        checkException();
    }

    int getStaticIntField(const char* className, const char* fieldName) {
        jclass clazz = _env->FindClass(className);
        jfieldID fieldID = _env->GetStaticFieldID(clazz, fieldName, "I");
        return _env->GetStaticIntField(clazz, fieldID);
    }

    void throwException(const char* message) {
        jclass clazz = _env->FindClass("java/lang/Exception");
        _env->ThrowNew(clazz, message);
    }

   private:
    void checkException() {
        if (_env->ExceptionCheck()) {
            jthrowable ex = _env->ExceptionOccurred();
            _env->ExceptionClear();
            jclass clazz = _env->GetObjectClass(ex);
            jmethodID getMessage = _env->GetMethodID(clazz, "getMessage", "()Ljava/lang/String;");
            jstring message = (jstring)_env->CallObjectMethod(ex, getMessage);
            const char* messageChars = _env->GetStringUTFChars(message, NULL);
            LOGE("JNI Exception: %s", messageChars);
            _env->ReleaseStringUTFChars(message, messageChars);
        }
    }

    jmethodID getMethod(const char* className, const char* method, const char* signature) {
        jclass clazz = _env->FindClass(className);
        jmethodID methodID = _env->GetMethodID(clazz, method, signature);
        return methodID;
    }

    jfieldID getStaticField(const char* className, const char* fieldName, const char* signature) {
        jclass clazz = _env->FindClass(className);
        jfieldID fieldID = _env->GetStaticFieldID(clazz, fieldName, signature);
        return fieldID;
    }

    JNIEnv* _env;
};

#endif  // JNIGLUE_H
