#include "naett.h"

extern "C" {

int runTests(const char* endpoint);

JNIEXPORT
int JNICALL Java_naett_test_NaettTests_runTests(JNIEnv* env, jobject obj) {
    JavaVM* vm = nullptr;
    env->GetJavaVM(&vm);
    naettInit(vm);
    return runTests("http://10.0.2.2:4711");
}
}
