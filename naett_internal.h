#ifndef NAETT_INTERNAL_H
#define NAETT_INTERNAL_H

#include "naett.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define __WINDOWS__ 1
#endif

#if __linux__ && !__ANDROID__
#define __LINUX__ 1
#endif

#ifdef __APPLE__
#include "TargetConditionals.h"
#include <objc/objc.h>
#if TARGET_OS_IPHONE
#define __IOS__ 1
#else
#define __MACOS__ 1
#endif
#endif

#define naettAlloc(TYPE, VAR) TYPE* VAR = (TYPE*)calloc(1, sizeof(TYPE))

typedef struct KVLink {
    const char* key;
    const char* value;
    struct KVLink* next;
} KVLink;

typedef struct Buffer {
    void* data;
    int size;
} Buffer;

typedef struct {
    const char* method;
    int timeoutMS;
    naettReadFunc bodyReader;
    naettWriteFunc bodyWriter;
    KVLink* headers;
    Buffer body;
} RequestOptions;

typedef struct {
    RequestOptions options;
    const char* url;
#if __APPLE__
    id urlRequest;
#endif
} InternalRequest;

typedef struct {
    InternalRequest* request;
    int complete;
    int code;
    Buffer body;
} InternalResponse;

void naettPlatformInitRequest(InternalRequest* req);

#endif  // NAETT_INTERNAL_H
