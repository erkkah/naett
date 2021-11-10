// Inlined amalgam.h: //
#include "naett.h"
// Inlined naett_core.c: //
// Inlined naett_internal.h: //
#ifndef NAETT_INTERNAL_H
#define NAETT_INTERNAL_H

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
    int capacity;
} Buffer;

typedef struct {
    const char* method;
    int timeoutMS;
    naettReadFunc bodyReader;
    void* bodyReaderData;
    naettWriteFunc bodyWriter;
    void* bodyWriterData;
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
    KVLink* headers;
    Buffer body;
#if __APPLE__
    id session;
#endif
} InternalResponse;

void naettPlatformInitRequest(InternalRequest* req);
void naettPlatformMakeRequest(InternalRequest* req, InternalResponse* res);
void naettPlatformFreeRequest(InternalRequest* req);
void naettPlatformCloseResponse(InternalResponse* res);

#endif  // NAETT_INTERNAL_H
// End of inlined naett_internal.h //

#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

typedef struct InternalParam* InternalParamPtr;
typedef void (*ParamSetter)(InternalParamPtr param, InternalRequest* req);

typedef struct InternalParam {
        ParamSetter setter;
        int offset;
        union {
            int integer;
            const char* string;
            struct {
                const char* key;
                const char* value;
            } kv;
            void* ptr;
        };
} InternalParam;

typedef struct InternalOption {
    #define maxParams 2
    int numParams;
    InternalParam params[maxParams];
} InternalOption;

static void stringSetter(InternalParamPtr param, InternalRequest* req) {
    char* stringCopy = strdup(param->string);
    char* opaque = (char*)&req->options;
    char** stringField = (char**)(opaque + param->offset);
    if (*stringField) {
        free(*stringField);
    }
    *stringField = stringCopy;
}

static void intSetter(InternalParamPtr param, InternalRequest* req) {
    char* opaque = (char*)&req->options;
    int* intField = (int*)(opaque + param->offset);
    *intField = param->integer;
}

static void ptrSetter(InternalParamPtr param, InternalRequest* req) {
    char* opaque = (char*)&req->options;
    void** ptrField = (void**)(opaque + param->offset);
    *ptrField = param->ptr;
}

static void kvSetter(InternalParamPtr param, InternalRequest* req) {
    char* opaque = (char*)&req->options;
    KVLink** kvField = (KVLink**)(opaque + param->offset);

    naettAlloc(KVLink, newNode);
    newNode->key = strdup(param->kv.key);
    newNode->value = strdup(param->kv.value);
    newNode->next = *kvField;

    *kvField = newNode;
}

static int defaultBodyWriter(const void* source, int bytes, void* userData) {
    Buffer* buffer = (Buffer*) userData;
    int newCapacity = buffer->capacity;
    if (newCapacity == 0) {
        newCapacity = bytes;
    }
    while (newCapacity - buffer->size < bytes) {
        newCapacity *= 2;
    }
    if (newCapacity != buffer->capacity) {
        buffer->data = realloc(buffer->data, newCapacity);
        buffer->capacity = newCapacity;
    }
    char* dest = ((char*)buffer->data) + buffer->size;
    memcpy(dest, source, bytes);
    buffer->size += bytes;
    return bytes;
}

static void initRequest(InternalRequest* req, const char* url) {
    req->options.method = strdup("GET");
    req->url = strdup(url);
}

static void applyOptionParams(InternalRequest* req, InternalOption* option) {
    for (int j = 0; j < option->numParams; j++) {
        InternalParam* param = option->params + j;
        param->setter(param, req);
    }
}

// Public API

naettOption* naettMethod(const char* method) {
    naettAlloc(InternalOption, option);
    option->numParams = 1;
    InternalParam* param = &option->params[0];

    param->string = method;
    param->offset = offsetof(RequestOptions, method);
    param->setter = stringSetter;

    return (naettOption*)option;
}

naettOption* naettHeader(const char* name, const char* value) {
    naettAlloc(InternalOption, option);
    option->numParams = 1;
    InternalParam* param = &option->params[0];

    param->kv.key = name;
    param->kv.value = value;
    param->offset = offsetof(RequestOptions, headers);
    param->setter = kvSetter;

    return (naettOption*)option;
}

naettOption* naettTimeout(int timeoutMS) {
    naettAlloc(InternalOption, option);
    option->numParams = 1;
    InternalParam* param = &option->params[0];

    param->integer = timeoutMS;
    param->offset = offsetof(RequestOptions, timeoutMS);
    param->setter = intSetter;

    return (naettOption*)option;
}

naettOption* naettBody(const char* body, int size) {
    naettAlloc(InternalOption, option);
    option->numParams = 2;

    InternalParam* bodyParam = &option->params[0];
    InternalParam* sizeParam = &option->params[1];

    bodyParam->ptr = (void*)body;
    bodyParam->offset = offsetof(RequestOptions, body) + offsetof(Buffer, data);
    bodyParam->setter = ptrSetter;

    sizeParam->integer = size;
    sizeParam->offset = offsetof(RequestOptions, body) + offsetof(Buffer, size);
    sizeParam->setter = intSetter;

    return (naettOption*)option;
}

naettOption* naettBodyReader(naettReadFunc reader, void* userData) {
    naettAlloc(InternalOption, option);
    option->numParams = 2;

    InternalParam* readerParam = &option->params[0];
    InternalParam* dataParam = &option->params[1];

    readerParam->ptr = (void*) reader;
    readerParam->offset = offsetof(RequestOptions, bodyReader);
    readerParam->setter = ptrSetter;

    dataParam->ptr = userData;
    dataParam->offset = offsetof(RequestOptions, bodyReaderData);
    dataParam->setter = ptrSetter;

    return (naettOption*)option;
}

naettOption* naettBodyWriter(naettWriteFunc writer, void* userData) {
    naettAlloc(InternalOption, option);
    option->numParams = 2;

    InternalParam* writerParam = &option->params[0];
    InternalParam* dataParam = &option->params[1];

    writerParam->ptr = (void*) writer;
    writerParam->offset = offsetof(RequestOptions, bodyWriter);
    writerParam->setter = ptrSetter;

    dataParam->ptr = userData;
    dataParam->offset = offsetof(RequestOptions, bodyWriterData);
    dataParam->setter = ptrSetter;

    return (naettOption*)option;
}
naettReq* naettRequest_va(const char* url, int numArgs, ...) {
    va_list args;
    InternalOption* option;
    naettAlloc(InternalRequest, req);
    initRequest(req, url);

    va_start(args, numArgs);
    for (int i = 0; i < numArgs; i++) {
        option = va_arg(args, InternalOption*);
        applyOptionParams(req, option);
        free(option);
    }
    va_end(args);

    naettPlatformInitRequest(req);
    return (naettReq*)req;
}

naettReq* naettRequestWithOptions(const char* url, int numOptions, const naettOption** options) {
    naettAlloc(InternalRequest, req);
    initRequest(req, url);

    for (int i = 0; i < numOptions; i++) {
        InternalOption* option = (InternalOption*)options[i];
        applyOptionParams(req, option);
        free(option);
    }

    naettPlatformInitRequest(req);
    return (naettReq*)req;
}

naettRes* naettMake(naettReq* request) {
    InternalRequest* req = (InternalRequest*)request;
    naettAlloc(InternalResponse, res);
    res->request = req;
    if (req->options.bodyWriter == NULL) {
        req->options.bodyWriter = defaultBodyWriter;
        req->options.bodyWriterData = (void*) &res->body;
    }
    naettPlatformMakeRequest(req, res);
    return (naettRes*) res;
}

const void* naettGetBody(naettRes* response, int* size) {
    InternalResponse* res = (InternalResponse*)response;
    *size = res->body.size;
    return res->body.data;
}

const char* naettGetHeader(naettRes* response, const char* name) {
    InternalResponse* res = (InternalResponse*)response;
    KVLink* node = res->headers;
    while (node) {
        if (strcasecmp(name, node->key) == 0) {
            return node->value;
        }
        node = node->next;
    }
    return NULL;
}

int naettComplete(const naettRes* response) {
    InternalResponse* res = (InternalResponse*)response;
    return res->complete;
}

void naettFree(naettReq* request) {
    InternalRequest* req = (InternalRequest*)request;
    naettPlatformFreeRequest(req);
    free(request);
}

void naettClose(naettRes* response) {
    InternalResponse* res = (InternalResponse*)response;
    res->request = NULL;
    naettPlatformCloseResponse(res);
    free(response);
}
// End of inlined naett_core.c //

// Inlined naett_osx.c: //
//#include "naett_internal.h"

#ifdef __MACOS__

// Inlined naett_objc.h: //
#ifndef NAETT_OBJC_H
#define NAETT_OBJC_H

#if defined(__IOS__) || defined (__MACOS__)
#include <assert.h>
#include <math.h>

#include <objc/NSObjCRuntime.h>
#include <objc/message.h>
#include <objc/objc.h>
#include <objc/runtime.h>

#if defined(__OBJC__) && __has_feature(objc_arc)
#error "ARC is not supported"
#endif

// ABI is a bit different between platforms
#ifdef __arm64__
#define abi_objc_msgSend_stret objc_msgSend
#else
#define abi_objc_msgSend_stret objc_msgSend_stret
#endif
#ifdef __i386__
#define abi_objc_msgSend_fpret objc_msgSend_fpret
#else
#define abi_objc_msgSend_fpret objc_msgSend
#endif

#define objc_msgSendSuper_t(RET, ...) ((RET(*)(struct objc_super*, SEL, ##__VA_ARGS__))objc_msgSendSuper)
#define objc_msgSend_t(RET, ...) ((RET(*)(id, SEL, ##__VA_ARGS__))objc_msgSend)
#define objc_msgSend_stret_t(RET, ...) ((RET(*)(id, SEL, ##__VA_ARGS__))abi_objc_msgSend_stret)
#define objc_msgSend_id objc_msgSend_t(id)
#define objc_msgSend_void objc_msgSend_t(void)
#define objc_msgSend_void_id objc_msgSend_t(void, id)
#define objc_msgSend_void_bool objc_msgSend_t(void, bool)

#define sel(NAME) sel_registerName(NAME)
#define class(NAME) ((id)objc_getClass(NAME))
#define makeClass(NAME, SUPER)     objc_allocateClassPair((Class)objc_getClass(SUPER), NAME, 0)

// Check here to get the signature right: https://nshipster.com/type-encodings/
#define addMethod(CLASS, NAME, IMPL, SIGNATURE)     if (!class_addMethod(CLASS, sel(NAME), (IMP) (IMPL), (SIGNATURE))) assert(false)

#define addIvar(CLASS, NAME, SIZE, SIGNATURE)     if (!class_addIvar(CLASS, NAME, SIZE, rint(log2(SIZE)), SIGNATURE)) assert(false)

#define objc_alloc(CLASS) objc_msgSend_id(class(CLASS), sel("alloc"))
#define autorelease(OBJ) objc_msgSend_void(OBJ, sel("autorelease"))
#define retain(OBJ) objc_msgSend_void(OBJ, sel("retain"))
#define release(OBJ) objc_msgSend_void(OBJ, sel("release"))

#if __LP64__ || NS_BUILD_32_LIKE_64
#define NSIntegerEncoding "q"
#define NSUIntegerEncoding "L"
#else
#define NSIntegerEncoding "i"
#define NSUIntegerEncoding "I"
#endif

#endif // defined(__IOS__) || defined (__MACOS__)
#endif // NAETT_OBJC_H
// End of inlined naett_objc.h //

#include <stdlib.h>
#include <string.h>

void naettPlatformInitRequest(InternalRequest* req) {
    id urlString = objc_msgSend_t(id, const char*)(class("NSString"), sel("stringWithUTF8String:"), req->url);
    id url = objc_msgSend_t(id, id)(class("NSURL"), sel("URLWithString:"), urlString);
    release(urlString);

    id request = objc_msgSend_t(id, id)(class("NSMutableURLRequest"), sel("requestWithURL:"), url);
    release(url);

    objc_msgSend_t(void, double)(request, sel("setTimeoutInterval:"), (double)(req->options.timeoutMS) / 1000.0);
    id methodString =
        objc_msgSend_t(id, const char*)(class("NSString"), sel("stringWithUTF8String:"), req->options.method);
    objc_msgSend_t(void, id)(request, sel("setHTTPMethod:"), methodString);
    release(methodString);

    KVLink* header = req->options.headers;
    while (header != NULL) {
        id name = objc_msgSend_t(id, const char*)(class("NSString"), sel("stringWithUTF8String:"), header->key);
        id value = objc_msgSend_t(id, const char*)(class("NSString"), sel("stringWithUTF8String:"), header->value);
        objc_msgSend_t(void, id, id)(request, sel("setValue:forHTTPHeaderField:"), name, value);
        release(name);
        release(value);
        header = header->next;
    }

    if (req->options.body.data) {
        Buffer* body = &req->options.body;

        id bodyData = objc_msgSend_t(id, void*, NSUInteger, BOOL)(
            class("NSData"), sel("dataWithBytesNoCopy:length:freeWhenDone:"), body->data, body->size, NO);
        objc_msgSend_t(void, id)(request, sel("setHTTPBody:"), bodyData);
        release(bodyData);
    }

    req->urlRequest = request;
}

void didReceiveData(id self, SEL _sel, id session, id dataTask, id data) {
    InternalResponse* res = NULL;
    object_getInstanceVariable(self, "response", (void**)&res);

    if (res->headers == NULL) {
        id response = objc_msgSend_t(id)(dataTask, sel("response"));
        id allHeaders = objc_msgSend_t(id)(response, sel("allHeaderFields"));

        NSUInteger headerCount = objc_msgSend_t(NSUInteger)(allHeaders, sel("count"));
        id headerNames[headerCount];
        id headerValues[headerCount];

        objc_msgSend_t(NSInteger, id*, id*, NSUInteger)(
            allHeaders, sel("getObjects:andKeys:count:"), headerValues, headerNames, headerCount);
        for (int i = 0; i < headerCount; i++) {
            naettAlloc(KVLink, node);
            node->key = strdup(objc_msgSend_t(const char*)(headerNames[i], sel("UTF8String")));
            node->value = strdup(objc_msgSend_t(const char*)(headerValues[i], sel("UTF8String")));
            node->next = res->headers;
            res->headers = node;
        }
    }

    const void* bytes = objc_msgSend_t(const void*)(data, sel("bytes"));
    NSUInteger length = objc_msgSend_t(NSUInteger)(data, sel("length"));
    res->request->options.bodyWriter(bytes, length, res->request->options.bodyWriterData);
}

static void didComplete(id self, SEL _sel, id session, id dataTask, id error) {
    InternalResponse* res = NULL;
    object_getInstanceVariable(self, "response", (void**)&res);
    res->complete = 1;
}

static id createDelegate() {
    Class TaskDelegateClass = nil;

    if (!TaskDelegateClass) {
        TaskDelegateClass = objc_allocateClassPair((Class)objc_getClass("NSObject"), "naettTaskDelegate", 0);
        addMethod(TaskDelegateClass, "URLSession:dataTask:didReceiveData:", didReceiveData, "v@:@@@");
        addMethod(TaskDelegateClass, "URLSession:task:didCompleteWithError:", didComplete, "v@:@@@");
        addIvar(TaskDelegateClass, "response", sizeof(void*), "^v");
    }

    id delegate = objc_msgSend_id((id)TaskDelegateClass, sel("alloc"));
    delegate = objc_msgSend_id(delegate, sel("init"));

    return delegate;
}

void naettPlatformMakeRequest(InternalRequest* req, InternalResponse* res) {
    id config = objc_msgSend_id(class("NSURLSessionConfiguration"), sel("ephemeralSessionConfiguration"));
    id delegate = createDelegate();

    id session = objc_msgSend_t(id, id, id, id)(
        class("NSURLSession"), sel("sessionWithConfiguration:delegate:delegateQueue:"), config, delegate, nil);

    res->session = session;
    release(delegate);

    id task = objc_msgSend_t(id, id)(session, sel("dataTaskWithRequest:"), req->urlRequest);
    object_setInstanceVariable(delegate, "response", (void*)res);
    objc_msgSend_void(task, sel("resume"));
    release(task);
}

void naettPlatformFreeRequest(InternalRequest* req) {
    release(req->urlRequest);
    req->urlRequest = nil;
}

void naettPlatformCloseResponse(InternalResponse* res) {
    release(res->session);
}

#endif  // __MACOS__
// End of inlined naett_osx.c //

// End of inlined amalgam.h //
