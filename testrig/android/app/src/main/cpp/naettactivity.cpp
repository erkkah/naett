#include <android/native_activity.h>

#include "jniglue.h"
#include "log.h"
#include "naett.h"

static void onStart(ANativeActivity* activity) {
    LOGD("onStart");
}

static void onResume(ANativeActivity* activity) {
    LOGD("onResume");
}

static void* onSaveInstanceState(ANativeActivity* activity, size_t* outSize) {
    LOGD(__func__);
    return NULL;
}

static void onPause(ANativeActivity* activity) {
    LOGD("onPause");
}

static void onStop(ANativeActivity* activity) {
    LOGD("onStop");
}

static void onDestroy(ANativeActivity* activity) {
    LOGD("onDestroy");
}

static void onWindowFocusChanged(ANativeActivity* activity, int focused) {
}

static void onNativeWindowCreated(ANativeActivity* activity, ANativeWindow* window) {
    LOGD("onNativeWindowCreated");
}

static void onNativeWindowDestroyed(ANativeActivity* activity, ANativeWindow* window) {
    LOGD("onNativeWindowDestroyed");
}

static void onInputQueueCreated(ANativeActivity* activity, AInputQueue* queue) {
    LOGD(__func__);
}

static void onInputQueueDestroyed(ANativeActivity* activity, AInputQueue* queue) {
    LOGD(__func__);
}

static void onContentRectChanged(ANativeActivity* activity, const ARect* rect) {
    LOGD(__func__);
}

static void onConfigurationChanged(ANativeActivity* activity) {
    LOGD(__func__);
}

static void onLowMemory(ANativeActivity* activity) {
    LOGD(__func__);
}

JNIEXPORT
void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize) {
    LOGD("onCreate");
    activity->callbacks->onStart = onStart;
    activity->callbacks->onResume = onResume;
    activity->callbacks->onSaveInstanceState = onSaveInstanceState;
    activity->callbacks->onPause = onPause;
    activity->callbacks->onStop = onStop;
    activity->callbacks->onDestroy = onDestroy;
    activity->callbacks->onWindowFocusChanged = onWindowFocusChanged;
    activity->callbacks->onNativeWindowCreated = onNativeWindowCreated;
    activity->callbacks->onNativeWindowDestroyed = onNativeWindowDestroyed;
    activity->callbacks->onInputQueueCreated = onInputQueueCreated;
    activity->callbacks->onInputQueueDestroyed = onInputQueueDestroyed;
    activity->callbacks->onContentRectChanged = onContentRectChanged;
    activity->callbacks->onConfigurationChanged = onConfigurationChanged;
    activity->callbacks->onLowMemory = onLowMemory;
    activity->instance = 0;

    JNIEnv* env;
    activity->vm->GetEnv((void**)&env, JNI_VERSION_1_6);
}

extern "C" {

int runTests(const char* endpoint);

JNIEXPORT
int JNICALL Java_com_example_naett_NaettTests_runTests(JNIEnv* env, jobject obj) {
    JavaVM* vm = nullptr;
    env->GetJavaVM(&vm);
    naettInit(vm);
    return runTests("http://10.0.2.2:4711");
}

}
