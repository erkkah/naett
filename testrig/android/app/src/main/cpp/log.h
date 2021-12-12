#ifndef LOG_H
#define LOG_H

#include <android/log.h>

#ifndef NDEBUG
#   define LOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, "timogr", __VA_ARGS__))
#else
#   define LOGD(...)  ((void)0)
#endif
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "timogr", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "timogr", __VA_ARGS__))

#endif  // LOG_H
