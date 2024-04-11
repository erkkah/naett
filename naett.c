// Inlined amalgam.h: //
#include "naett.h"
// Inlined naett_core.c: //
// Inlined naett_internal.h: //
#ifndef NAETT_INTERNAL_H
#define NAETT_INTERNAL_H

#ifdef _MSC_VER
    #define strcasecmp _stricmp
    #undef strdup
    #define strdup _strdup
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#ifndef min
    #define min(x,y) ((x) < (y) ? (x) : (y))
#endif
#define __WINDOWS__ 1
#endif

#if __linux__ && !__ANDROID__
#define __LINUX__ 1
#include <curl/curl.h>
#endif

#if __ANDROID__
#include <jni.h>
#include <pthread.h>
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
    int position;
} Buffer;

typedef struct {
    const char* method;
    const char* userAgent;
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
#if __ANDROID__
    jobject urlObject;
#endif
#if __WINDOWS__
    HINTERNET session;
    HINTERNET connection;
    HINTERNET request;
    LPWSTR host;
    LPWSTR resource;
#endif
} InternalRequest;

typedef struct {
    InternalRequest* request;
    int code;
    int complete;
    KVLink* headers;
    Buffer body;
    int contentLength;  // 0 if headers not read, -1 if Content-Length missing.
    int totalBytesRead;
#if __APPLE__
    id session;
#endif
#if __ANDROID__
    pthread_t workerThread;
    int closeRequested;
#endif
#if __LINUX__
    struct curl_slist* headerList;
#endif
#if __WINDOWS__
    char buffer[10240];
    size_t bytesLeft;
#endif
} InternalResponse;

void naettPlatformInit(naettInitData initData);
int naettPlatformInitRequest(InternalRequest* req);
void naettPlatformMakeRequest(InternalResponse* res);
void naettPlatformFreeRequest(InternalRequest* req);
void naettPlatformCloseResponse(InternalResponse* res);

#endif  // NAETT_INTERNAL_H
// End of inlined naett_internal.h //

#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

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
            void (*func)(void);
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

static int defaultBodyReader(void* dest, int bufferSize, void* userData) {
    Buffer* buffer = (Buffer*) userData;

    if (dest == NULL) {
        return buffer->size;
    }

    int bytesToRead = buffer->size - buffer->position;
    if (bytesToRead > bufferSize) {
        bytesToRead = bufferSize;
    }

    const char* source = ((const char*)buffer->data) + buffer->position;
    memcpy(dest, source, bytesToRead);
    buffer->position += bytesToRead;
    return bytesToRead;
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

static int initialized = 0;

static void initRequest(InternalRequest* req, const char* url) {
    assert(initialized);
    req->options.method = strdup("GET");
    req->options.timeoutMS = 5000;
    req->url = strdup(url);
}

static void applyOptionParams(InternalRequest* req, InternalOption* option) {
    for (int j = 0; j < option->numParams; j++) {
        InternalParam* param = option->params + j;
        param->setter(param, req);
    }
}

// Public API

void naettInit(naettInitData initData) {
    assert(!initialized);
    naettPlatformInit(initData);
    initialized = 1;
}

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

naettOption* naettUserAgent(const char* userAgent) {
    naettAlloc(InternalOption, option);
    option->numParams = 1;
    InternalParam* param = &option->params[0];

    param->string = userAgent;
    param->offset = offsetof(RequestOptions, userAgent);
    param->setter = stringSetter;

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

    readerParam->func = (void (*)(void)) reader;
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

    writerParam->func = (void(*)(void)) writer;
    writerParam->offset = offsetof(RequestOptions, bodyWriter);
    writerParam->setter = ptrSetter;

    dataParam->ptr = userData;
    dataParam->offset = offsetof(RequestOptions, bodyWriterData);
    dataParam->setter = ptrSetter;

    return (naettOption*)option;
}

void setupDefaultRW(InternalRequest* req) {
    if (req->options.bodyReader == NULL) {
        req->options.bodyReader = defaultBodyReader;
        req->options.bodyReaderData = (void*) &req->options.body;
    }
    if (req->options.bodyReader == defaultBodyReader) {
        req->options.body.position = 0;
    }
    if (req->options.bodyWriter == NULL) {
        req->options.bodyWriter = defaultBodyWriter;
    }
}

naettReq* naettRequest_va(const char* url, int numArgs, ...) {
    assert(url != NULL);

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

    setupDefaultRW(req);

    if (naettPlatformInitRequest(req)) {
        return (naettReq*)req;
    }

    naettFree((naettReq*) req);
    return NULL;
}

naettReq* naettRequestWithOptions(const char* url, int numOptions, const naettOption** options) {
    assert(url != NULL);
    assert(numOptions == 0 || options != NULL);

    naettAlloc(InternalRequest, req);
    initRequest(req, url);

    for (int i = 0; i < numOptions; i++) {
        InternalOption* option = (InternalOption*)options[i];
        applyOptionParams(req, option);
        free(option);
    }

    setupDefaultRW(req);

    if (naettPlatformInitRequest(req)) {
        return (naettReq*)req;
    }

    naettFree((naettReq*) req);
    return NULL;
}

naettRes* naettMake(naettReq* request) {
    assert(initialized);
    assert(request != NULL);

    InternalRequest* req = (InternalRequest*)request;
    naettAlloc(InternalResponse, res);
    res->request = req;

    if (req->options.bodyWriter == defaultBodyWriter) {
        req->options.bodyWriterData = (void*) &res->body;
    }

    naettPlatformMakeRequest(res);
    return (naettRes*) res;
}

const void* naettGetBody(naettRes* response, int* size) {
    assert(response != NULL);
    assert(size != NULL);

    InternalResponse* res = (InternalResponse*)response;
    *size = res->body.size;
    return res->body.data;
}

int naettGetTotalBytesRead(naettRes* response, int* totalSize) {
    assert(response != NULL);
    assert(totalSize != NULL);

    InternalResponse* res = (InternalResponse*)response;
    *totalSize = res->contentLength;
    return res->totalBytesRead;
}

const char* naettGetHeader(naettRes* response, const char* name) {
    assert(response != NULL);
    assert(name != NULL);

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

void naettListHeaders(naettRes* response, naettHeaderLister lister, void* userData) {
    assert(response != NULL);
    assert(lister != NULL);

    InternalResponse* res = (InternalResponse*)response;
    KVLink* node = res->headers;
    while (node) {
        if (!lister(node->key, node->value, userData)) {
            return;
        }
        node = node->next;
    }
}

naettReq* naettGetRequest(naettRes* response) {
    assert(response != NULL);
    InternalResponse* res = (InternalResponse*)response;
    return (naettReq*) res->request;
}

int naettComplete(const naettRes* response) {
    assert(response != NULL);
    InternalResponse* res = (InternalResponse*)response;
    return res->complete;
}

int naettGetStatus(const naettRes* response) {
    assert(response != NULL);
    InternalResponse* res = (InternalResponse*)response;
    return res->code;
}

static void freeKVList(KVLink* node) {
    while (node != NULL) {
        free((void*) node->key);
        free((void*) node->value);
        KVLink* next = node->next;
        free(node);
        node = next;
    }
}

void naettFree(naettReq* request) {
    assert(request != NULL);

    InternalRequest* req = (InternalRequest*)request;
    naettPlatformFreeRequest(req);
    KVLink* node = req->options.headers;
    freeKVList(node);
    free((void*)req->options.method);
    free((void*)req->url);
    free(request);
}

void naettClose(naettRes* response) {
    assert(response != NULL);

    InternalResponse* res = (InternalResponse*)response;
    res->request = NULL;
    naettPlatformCloseResponse(res);
    KVLink* node = res->headers;
    freeKVList(node);
    free(res->body.data);
    free(response);
}
// End of inlined naett_core.c //

// Inlined naett_osx.c: //
//#include "naett_internal.h"

#ifdef __APPLE__

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
#define makeClass(NAME, SUPER) \
    objc_allocateClassPair((Class)objc_getClass(SUPER), NAME, 0)

// Check here to get the signature right:
// https://nshipster.com/type-encodings/
// https://ko9.org/posts/encode-types/
#define addMethod(CLASS, NAME, IMPL, SIGNATURE) \
    if (!class_addMethod(CLASS, sel(NAME), (IMP) (IMPL), (SIGNATURE))) assert(false)

#define addIvar(CLASS, NAME, SIZE, SIGNATURE) \
    if (!class_addIvar(CLASS, NAME, SIZE, rint(log2(SIZE)), SIGNATURE)) assert(false)

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
#include <stdio.h>

#ifdef DEBUG
static void _showPools(const char* context) {
    fprintf(stderr, "NSAutoreleasePool@%s:\n", context);
    objc_msgSend_void(class("NSAutoreleasePool"), sel("showPools"));
}
#define showPools(x) _showPools((x))
#else
#define showPools(x)
#endif

static id pool(void) {
    return objc_msgSend_id(objc_alloc("NSAutoreleasePool"), sel("init"));
}

static id sessionConfiguration = nil;

void naettPlatformInit(naettInitData initData) {
    id NSThread = class("NSThread");
    SEL isMultiThreaded = sel("isMultiThreaded");

    if (!objc_msgSend_t(bool)(NSThread, isMultiThreaded)) {
        // Make a dummy call from a new thread to kick Cocoa into multi-threaded mode
        objc_msgSend_t(void, SEL, id, id)(
            NSThread, sel("detachNewThreadSelector:toTarget:withObject:"), isMultiThreaded, NSThread, nil);
    }

    sessionConfiguration = objc_msgSend_id(class("NSURLSessionConfiguration"), sel("ephemeralSessionConfiguration"));
    retain(sessionConfiguration);
}

id NSString(const char* string) {
    return objc_msgSend_t(id, const char*)(class("NSString"), sel("stringWithUTF8String:"), string);
}

int naettPlatformInitRequest(InternalRequest* req) {
    id p = pool();

    id urlString = NSString(req->url);
    id url = objc_msgSend_t(id, id)(class("NSURL"), sel("URLWithString:"), urlString);

    id request = objc_msgSend_t(id, id)(class("NSMutableURLRequest"), sel("requestWithURL:"), url);

    objc_msgSend_t(void, double)(request, sel("setTimeoutInterval:"), (double)(req->options.timeoutMS) / 1000.0);
    id methodString = NSString(req->options.method);
    objc_msgSend_t(void, id)(request, sel("setHTTPMethod:"), methodString);

    {
        id name = NSString("User-Agent");
        id value = NSString(req->options.userAgent ? req->options.userAgent : NAETT_UA);
        objc_msgSend_t(void, id, id)(request, sel("setValue:forHTTPHeaderField:"), value, name);
    }

    KVLink* header = req->options.headers;
    while (header != NULL) {
        id name = NSString(header->key);
        id value = NSString(header->value);
        objc_msgSend_t(void, id, id)(request, sel("setValue:forHTTPHeaderField:"), value, name);
        header = header->next;
    }

    char byteBuffer[10240];
    int bytesRead = 0;

    if (req->options.bodyReader != NULL) {
        id bodyData =
            objc_msgSend_t(id, NSUInteger)(class("NSMutableData"), sel("dataWithCapacity:"), sizeof(byteBuffer));

        int totalBytesRead = 0;
        do {
            bytesRead = req->options.bodyReader(byteBuffer, sizeof(byteBuffer), req->options.bodyReaderData);
            totalBytesRead += bytesRead;
            objc_msgSend_t(void, const void*, NSUInteger)(bodyData, sel("appendBytes:length:"), byteBuffer, bytesRead);
        } while (bytesRead > 0);

        if (totalBytesRead > 0) {
            objc_msgSend_t(void, id)(request, sel("setHTTPBody:"), bodyData);
        }
    }

    retain(request);
    req->urlRequest = request;

    release(p);
    return 1;
}

void didReceiveData(id self, SEL _sel, id session, id dataTask, id data) {
    InternalResponse* res = NULL;
    id p = pool();

    object_getInstanceVariable(self, "response", (void**)&res);

    if (res->headers == NULL) {
        id response = objc_msgSend_t(id)(dataTask, sel("response"));
        res->code = objc_msgSend_t(NSInteger)(response, sel("statusCode"));
        id allHeaders = objc_msgSend_t(id)(response, sel("allHeaderFields"));

        NSUInteger headerCount = objc_msgSend_t(NSUInteger)(allHeaders, sel("count"));
        id headerNames[headerCount];
        id headerValues[headerCount];

        objc_msgSend_t(NSInteger, id*, id*, NSUInteger)(
            allHeaders, sel("getObjects:andKeys:count:"), headerValues, headerNames, headerCount);
        KVLink* firstHeader = NULL;
        for (int i = 0; i < headerCount; i++) {
            naettAlloc(KVLink, node);
            node->key = strdup(objc_msgSend_t(const char*)(headerNames[i], sel("UTF8String")));
            node->value = strdup(objc_msgSend_t(const char*)(headerValues[i], sel("UTF8String")));
            node->next = firstHeader;
            firstHeader = node;
        }
        res->headers = firstHeader;

        const char* contentLength = naettGetHeader((naettRes*)res, "Content-Length");
        if (!contentLength || sscanf(contentLength, "%d", &res->contentLength) != 1) {
            res->contentLength = -1;
        }
    }

    const void* bytes = objc_msgSend_t(const void*)(data, sel("bytes"));
    NSUInteger length = objc_msgSend_t(NSUInteger)(data, sel("length"));

    res->request->options.bodyWriter(bytes, length, res->request->options.bodyWriterData);
    res->totalBytesRead += (int)length;

    release(p);
}

static void didComplete(id self, SEL _sel, id session, id dataTask, id error) {
    InternalResponse* res = NULL;
    object_getInstanceVariable(self, "response", (void**)&res);
    if (res != NULL) {
        if (error != nil) {
            res->code = naettConnectionError;
        }
        res->complete = 1;
    }
}

static id createDelegate(void) {
    static Class TaskDelegateClass = nil;

    if (!TaskDelegateClass) {
        TaskDelegateClass = objc_allocateClassPair((Class)objc_getClass("NSObject"), "naettTaskDelegate", 0);
        class_addProtocol(TaskDelegateClass, (Protocol* _Nonnull)objc_getProtocol("NSURLSessionDataDelegate"));

        addMethod(TaskDelegateClass, "URLSession:dataTask:didReceiveData:", didReceiveData, "v@:@@@");
        addMethod(TaskDelegateClass, "URLSession:task:didCompleteWithError:", didComplete, "v@:@@@");
        addIvar(TaskDelegateClass, "response", sizeof(void*), "^v");
    }

    id delegate = objc_msgSend_id((id)TaskDelegateClass, sel("alloc"));
    delegate = objc_msgSend_id(delegate, sel("init"));

    return delegate;
}

void naettPlatformMakeRequest(InternalResponse* res) {
    InternalRequest* req = res->request;
    id p = pool();

    id delegate = createDelegate();
    // delegate will be retained by session below
    autorelease(delegate);
    object_setInstanceVariable(delegate, "response", (void*)res);

    id session = objc_msgSend_t(id, id, id, id)(class("NSURLSession"),
        sel("sessionWithConfiguration:delegate:delegateQueue:"),
        sessionConfiguration,
        delegate,
        nil);

    res->session = session;

    id task = objc_msgSend_t(id, id)(session, sel("dataTaskWithRequest:"), req->urlRequest);
    objc_msgSend_void(task, sel("resume"));

    release(p);
}

void naettPlatformFreeRequest(InternalRequest* req) {
    release(req->urlRequest);
    req->urlRequest = nil;
}

void naettPlatformCloseResponse(InternalResponse* res) {
    objc_msgSend_void(res->session, sel("invalidateAndCancel"));
    res->session = nil;
}

#endif  // __APPLE__
// End of inlined naett_osx.c //

// Inlined naett_linux.c: //
//#include "naett_internal.h"

#if __linux__ && !__ANDROID__

#include <curl/curl.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

static pthread_t workerThread;
static int handleReadFD = 0;
static int handleWriteFD = 0;

static void panic(const char* message) {
    fprintf(stderr, "%s\n", message);
    exit(1);
}

static void* curlWorker(void* data) {
    CURLM* mc = (CURLM*)data;
    int activeHandles = 0;
    int messagesLeft = 1;

    struct curl_waitfd readFd = { handleReadFD, CURL_WAIT_POLLIN };

    union {
        CURL* handle;
        char buf[sizeof(CURL*)];
    } newHandle;

    int newHandlePos = 0;

    while (1) {
        int status = curl_multi_perform(mc, &activeHandles);
        if (status != CURLM_OK) {
            panic("CURL processing failure");
        }

        struct CURLMsg* message = curl_multi_info_read(mc, &messagesLeft);
        if (message && message->msg == CURLMSG_DONE) {
            CURL* handle = message->easy_handle;
            InternalResponse* res = NULL;
            curl_easy_getinfo(handle, CURLINFO_PRIVATE, (char**)&res);
            curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &res->code);
            res->complete = 1;
            curl_easy_cleanup(handle);
        }


        int readyFDs = 0;
        curl_multi_wait(mc, &readFd, 1, 1, &readyFDs);

        if (readyFDs == 0 && activeHandles == 0 && messagesLeft == 0) {
            usleep(100 * 1000);
        }

        int bytesRead = read(handleReadFD, newHandle.buf, sizeof(newHandle.buf) - newHandlePos);
        if (bytesRead > 0) {
            newHandlePos += bytesRead;
        }
        if (newHandlePos == sizeof(newHandle.buf)) {
            curl_multi_add_handle(mc, newHandle.handle);
            newHandlePos = 0;
        }
    }

    return NULL;
}

void naettPlatformInit(naettInitData initData) {
    curl_global_init(CURL_GLOBAL_ALL);
    CURLM* mc = curl_multi_init();
    int fds[2];
    if (pipe(fds) != 0) {
        panic("Failed to open pipe");
    }
    handleReadFD = fds[0];
    handleWriteFD = fds[1];

    int flags = fcntl(handleReadFD, F_GETFL, 0);
    fcntl(handleReadFD, F_SETFL, flags | O_NONBLOCK);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&workerThread, &attr, curlWorker, mc);
}

int naettPlatformInitRequest(InternalRequest* req) {
    return 1;
}

static size_t readCallback(char* buffer, size_t size, size_t numItems, void* userData) {
    InternalResponse* res = (InternalResponse*)userData;
    InternalRequest* req = res->request;
    return req->options.bodyReader(buffer, size * numItems, req->options.bodyReaderData);
}

static size_t writeCallback(char* ptr, size_t size, size_t numItems, void* userData) {
    InternalResponse* res = (InternalResponse*)userData;
    InternalRequest* req = res->request;
    size_t bytesWritten = req->options.bodyWriter(ptr, size * numItems, req->options.bodyWriterData);
    res->totalBytesRead += bytesWritten;
    return bytesWritten;
}

#define METHOD(A, B, C) (((A) << 16) | ((B) << 8) | (C))

static void setupMethod(CURL* curl, const char* method) {
    if (strlen(method) < 3) {
        return;
    }

    int methodCode = (method[0] << 16) | (method[1] << 8) | method[2];

    switch (methodCode) {
        case METHOD('G', 'E', 'T'):
        case METHOD('C', 'O', 'N'):
        case METHOD('O', 'P', 'T'):
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
            break;
        case METHOD('P', 'O', 'S'):
        case METHOD('P', 'A', 'T'):
        case METHOD('D', 'E', 'L'):
            curl_easy_setopt(curl, CURLOPT_POST, 1);
            break;
        case METHOD('P', 'U', 'T'):
            curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
            break;
        case METHOD('H', 'E', 'A'):
        case METHOD('T', 'R', 'A'):
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
            break;
    }

    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
}

static size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userData) {
    InternalResponse* res = (InternalResponse*) userData;
    size_t headerSize = size * nitems;

    char* headerName = strndup(buffer, headerSize);
    char* split = strchr(headerName, ':');
    if (split) {
        *split = 0;
        split++;
        while (*split == ' ') {
            split++;
        }
        char* headerValue = strdup(split);

        char* cr = strchr(headerValue, 13);
        if (cr) {
            *cr = 0;
        }

        char* lf = strchr(headerValue, 10);
        if (lf) {
            *lf = 0;
        }

        naettAlloc(KVLink, node);
        node->next = res->headers;
        node->key = headerName;
        node->value = headerValue;
        res->headers = node;
    }

    return headerSize;
}

void naettPlatformMakeRequest(InternalResponse* res) {
    InternalRequest* req = res->request;

    CURL* c = curl_easy_init();
    curl_easy_setopt(c, CURLOPT_URL, req->url);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT_MS, req->options.timeoutMS);

    curl_easy_setopt(c, CURLOPT_READFUNCTION, readCallback);
    curl_easy_setopt(c, CURLOPT_READDATA, res);

    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, res);

    curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(c, CURLOPT_HEADERDATA, res);

    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1);

    int bodySize = res->request->options.bodyReader(NULL, 0, res->request->options.bodyReaderData);
    curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, bodySize);

    setupMethod(c, req->options.method);

    struct curl_slist* headerList = NULL;
    char uaBuf[512];
    snprintf(uaBuf, sizeof(uaBuf), "User-Agent: %s", req->options.userAgent ? req->options.userAgent : NAETT_UA);
    headerList = curl_slist_append(headerList, uaBuf);

    KVLink* header = req->options.headers;
    size_t bufferSize = 0;
    char* buffer = NULL;
    while (header) {
        size_t headerLength = strlen(header->key) + strlen(header->value) + 1 + 1;  // colon + null
        if (headerLength > bufferSize) {
            bufferSize = headerLength;
            buffer = (char*)realloc(buffer, bufferSize);
        }
        snprintf(buffer, bufferSize, "%s:%s", header->key, header->value);
        headerList = curl_slist_append(headerList, buffer);
        header = header->next;
    }
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, headerList);
    free(buffer);
    res->headerList = headerList;

    curl_easy_setopt(c, CURLOPT_PRIVATE, res);

    write(handleWriteFD, &c, sizeof(c));
}

void naettPlatformFreeRequest(InternalRequest* req) {
}

void naettPlatformCloseResponse(InternalResponse* res) {
    curl_slist_free_all(res->headerList);
}

#endif
// End of inlined naett_linux.c //

// Inlined naett_win.c: //
//#include "naett_internal.h"

#ifdef __WINDOWS__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <winhttp.h>
#include <assert.h>
#include <tchar.h>

void naettPlatformInit(naettInitData initData) {
}

static char* winToUTF8(LPWSTR source) {
    int length = WideCharToMultiByte(CP_UTF8, 0, source, -1, NULL, 0, NULL, NULL);
    char* chars = (char*)malloc(length);
    int result = WideCharToMultiByte(CP_UTF8, 0, source, -1, chars, length, NULL, NULL);
    if (!result) {
        free(chars);
        return NULL;
    }
    return chars;
}

static LPWSTR winFromUTF8(const char* source) {
    int length = MultiByteToWideChar(CP_UTF8, 0, source, -1, NULL, 0);
    LPWSTR chars = (LPWSTR)malloc(length * sizeof(WCHAR));
    int result = MultiByteToWideChar(CP_UTF8, 0, source, -1, chars, length);
    if (!result) {
        free(chars);
        return NULL;
    }
    return chars;
}

#define ASPRINTF(result, fmt, ...)                        \
    {                                                     \
        size_t len = snprintf(NULL, 0, fmt, __VA_ARGS__); \
        *(result) = (char*)malloc(len + 1);               \
        snprintf(*(result), len + 1, fmt, __VA_ARGS__);   \
    }

static LPWSTR wcsndup(LPCWSTR str, size_t len) {
    LPWSTR result = calloc(1, sizeof(WCHAR) * (len + 1));
    wcsncpy(result, str, len);
    return result;
}

static LPCWSTR packHeaders(InternalRequest* req) {
    char* packed = strdup("");

    KVLink* node = req->options.headers;
    while (node != NULL) {
        char* update;
        ASPRINTF(&update, "%s%s:%s%s", packed, node->key, node->value, node->next ? "\r\n" : "");
        free(packed);
        packed = update;
        node = node->next;
    }

    LPCWSTR winHeaders = winFromUTF8(packed);
    free(packed);
    return winHeaders;
}

static void unpackHeaders(InternalResponse* res, LPWSTR packed) {
    size_t len = 0;
    KVLink* firstHeader = NULL;
    while ((len = wcslen(packed)) != 0) {
        char* header = winToUTF8(packed);
        char* split = strchr(header, ':');
        if (split) {
            *split = 0;
            split++;
            while (*split == ' ') {
                split++;
            }
            naettAlloc(KVLink, node);
            node->key = strdup(header);
            node->value = strdup(split);
            node->next = firstHeader;
            firstHeader = node;
        }
        free(header);
        packed += len + 1;
    }
    res->headers = firstHeader;
}

static void CALLBACK
callback(HINTERNET request, DWORD_PTR context, DWORD status, LPVOID statusInformation, DWORD statusInfoLength) {
    InternalResponse* res = (InternalResponse*)context;

    switch (status) {
        case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE: {
            DWORD bufSize = 0;
            WinHttpQueryHeaders(request,
                WINHTTP_QUERY_RAW_HEADERS,
                WINHTTP_HEADER_NAME_BY_INDEX,
                NULL,
                &bufSize,
                WINHTTP_NO_HEADER_INDEX);
            LPWSTR buffer = (LPWSTR)malloc(bufSize);
            WinHttpQueryHeaders(request,
                WINHTTP_QUERY_RAW_HEADERS,
                WINHTTP_HEADER_NAME_BY_INDEX,
                buffer,
                &bufSize,
                WINHTTP_NO_HEADER_INDEX);
            unpackHeaders(res, buffer);
            free(buffer);

            const char* contentLength = naettGetHeader((naettRes*)res, "Content-Length");
            if (!contentLength || sscanf(contentLength, "%d", &res->contentLength) != 1) {
                res->contentLength = -1;
            }

            DWORD statusCode = 0;
            DWORD statusCodeSize = sizeof(statusCode);

            WinHttpQueryHeaders(request,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &statusCode,
                &statusCodeSize,
                WINHTTP_NO_HEADER_INDEX);
            res->code = statusCode;

            if (!WinHttpQueryDataAvailable(request, NULL)) {
                res->code = naettProtocolError;
                res->complete = 1;
            }
        } break;

        case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE: {
            DWORD* available = (DWORD*)statusInformation;
            res->bytesLeft = *available;
            if (res->bytesLeft == 0) {
                res->complete = 1;
                break;
            }

            size_t bytesToRead = min(res->bytesLeft, sizeof(res->buffer));
            if (!WinHttpReadData(request, res->buffer, (DWORD)bytesToRead, NULL)) {
                res->code = naettReadError;
                res->complete = 1;
            }
        } break;

        case WINHTTP_CALLBACK_STATUS_READ_COMPLETE: {
            size_t bytesRead = statusInfoLength;

            InternalRequest* req = res->request;
            if (req->options.bodyWriter(res->buffer, (int)bytesRead, req->options.bodyWriterData) != bytesRead) {
                res->code = naettReadError;
                res->complete = 1;
            }
            res->totalBytesRead += (int)bytesRead;
            res->bytesLeft -= bytesRead;
            if (res->bytesLeft > 0) {
                size_t bytesToRead = min(res->bytesLeft, sizeof(res->buffer));
                if (!WinHttpReadData(request, res->buffer, (DWORD)bytesToRead, NULL)) {
                    res->code = naettReadError;
                    res->complete = 1;
                }
            } else {
                if (!WinHttpQueryDataAvailable(request, NULL)) {
                    res->code = naettProtocolError;
                    res->complete = 1;
                }
            }
        } break;

        case WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE:
        case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE: {
            int bytesRead = res->request->options.bodyReader(
                res->buffer, sizeof(res->buffer), res->request->options.bodyReaderData);
            if (bytesRead) {
                WinHttpWriteData(request, res->buffer, bytesRead, NULL);
            } else {
                if (!WinHttpReceiveResponse(request, NULL)) {
                    res->code = naettReadError;
                    res->complete = 1;
                }
            }
        } break;

        //
        case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR: {
            WINHTTP_ASYNC_RESULT* result = (WINHTTP_ASYNC_RESULT*)statusInformation;
            switch (result->dwResult) {
                case API_RECEIVE_RESPONSE:
                case API_QUERY_DATA_AVAILABLE:
                case API_READ_DATA:
                    res->code = naettReadError;
                    break;
                case API_WRITE_DATA:
                    res->code = naettWriteError;
                    break;
                case API_SEND_REQUEST:
                    res->code = naettConnectionError;
                    break;
                default:
                    res->code = naettGenericError;
            }

            res->complete = 1;
        } break;
    }
}

int naettPlatformInitRequest(InternalRequest* req) {
    LPWSTR url = winFromUTF8(req->url);

    URL_COMPONENTS components;
    ZeroMemory(&components, sizeof(components));
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = (DWORD)-1;
    components.dwHostNameLength = (DWORD)-1;
    components.dwUrlPathLength = (DWORD)-1;
    components.dwExtraInfoLength = (DWORD)-1;
    BOOL cracked = WinHttpCrackUrl(url, 0, 0, &components);

    if (!cracked) {
        free(url);
        return 0;
    }

    req->host = wcsndup(components.lpszHostName, components.dwHostNameLength);
    req->resource = wcsndup(components.lpszUrlPath, components.dwUrlPathLength + components.dwExtraInfoLength);
    free(url);

    LPWSTR uaBuf = winFromUTF8(req->options.userAgent ? req->options.userAgent : NAETT_UA);
    req->session = WinHttpOpen(uaBuf,
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        WINHTTP_FLAG_ASYNC);
    free(uaBuf);

    if (!req->session) {
        return 0;
    }

    WinHttpSetStatusCallback(req->session, callback, WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS, 0);

    // Set the connect timeout. Leave the other three timeouts at their default values.
    WinHttpSetTimeouts(req->session, 0, req->options.timeoutMS, 30000, 30000);

    req->connection = WinHttpConnect(req->session, req->host, components.nPort, 0);
    if (!req->connection) {
        naettPlatformFreeRequest(req);
        return 0;
    }

    LPWSTR verb = winFromUTF8(req->options.method);
    req->request = WinHttpOpenRequest(req->connection,
        verb,
        req->resource,
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        components.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    free(verb);
    if (!req->request) {
        naettPlatformFreeRequest(req);
        return 0;
    }

    LPCWSTR headers = packHeaders(req);
    if (headers[0] != 0) {
        if (!WinHttpAddRequestHeaders(
                req->request, headers, -1, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE)) {
            naettPlatformFreeRequest(req);
            free((LPWSTR)headers);
            return 0;
        }
    }
    free((LPWSTR)headers);

    return 1;
}

void naettPlatformMakeRequest(InternalResponse* res) {
    InternalRequest* req = res->request;

    LPCWSTR extraHeaders = WINHTTP_NO_ADDITIONAL_HEADERS;
    WCHAR contentLengthHeader[64];

    int contentLength = req->options.bodyReader(NULL, 0, req->options.bodyReaderData);
    if (contentLength > 0) {
        swprintf(contentLengthHeader, 64, L"Content-Length: %d", contentLength);
        extraHeaders = contentLengthHeader;
    }

    if (!WinHttpSendRequest(req->request, extraHeaders, -1, NULL, 0, 0, (DWORD_PTR)res)) {
        res->code = naettConnectionError;
        res->complete = 1;
    }
}

void naettPlatformFreeRequest(InternalRequest* req) {
    assert(req != NULL);

    if (req->request != NULL) {
        WinHttpCloseHandle(req->request);
        req->request = NULL;
    }
    if (req->connection != NULL) {
        WinHttpCloseHandle(req->connection);
        req->connection = NULL;
    }
    if (req->session != NULL) {
        WinHttpCloseHandle(req->session);
        req->session = NULL;
    }
    if (req->host != NULL) {
        free(req->host);
        req->host = NULL;
    }
    if (req->resource != NULL) {
        free(req->resource);
        req->resource = NULL;
    }
}

void naettPlatformCloseResponse(InternalResponse* res) {
}

#endif  // __WINDOWS__
// End of inlined naett_win.c //

// Inlined naett_android.c: //
//#include "naett_internal.h"

#ifdef __ANDROID__

#ifdef __cplusplus
#error "Cannot build in C++ mode"
#endif

#include <stdlib.h>
#include <string.h>
#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <stdarg.h>

#ifndef NDEBUG
#define LOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, "naett", __VA_ARGS__))
#else
#define LOGD(...) ((void)0)
#endif
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "naett", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "naett", __VA_ARGS__))

static JavaVM* globalVM = NULL;

static JavaVM* getVM() {
    if (globalVM == NULL) {
        LOGE("Panic: No VM configured, exiting.");
        exit(42);
    }
    return globalVM;
}

static JNIEnv* getEnv() {
    JavaVM* vm = getVM();
    JNIEnv* env;
    (*vm)->AttachCurrentThread(vm, &env, NULL);
    return env;
}

static int catch (JNIEnv* env) {
    int thrown = (*env)->ExceptionCheck(env);
    if (thrown) {
        (*env)->ExceptionDescribe(env);
    }
    return thrown;
}

static jmethodID getMethod(JNIEnv* env, jobject instance, const char* method, const char* sig) {
    jclass clazz = (*env)->GetObjectClass(env, instance);
    jmethodID id = (*env)->GetMethodID(env, clazz, method, sig);
    (*env)->DeleteLocalRef(env, clazz);
    return id;
}

static jobject call(JNIEnv* env, jobject instance, const char* method, const char* sig, ...) {
    jmethodID methodID = getMethod(env, instance, method, sig);
    va_list args;
    va_start(args, sig);
    jobject result = (*env)->CallObjectMethodV(env, instance, methodID, args);
    va_end(args);
    return result;
}

static void voidCall(JNIEnv* env, jobject instance, const char* method, const char* sig, ...) {
    jmethodID methodID = getMethod(env, instance, method, sig);
    va_list args;
    va_start(args, sig);
    (*env)->CallVoidMethodV(env, instance, methodID, args);
    va_end(args);
}

static jint intCall(JNIEnv* env, jobject instance, const char* method, const char* sig, ...) {
    jmethodID methodID = getMethod(env, instance, method, sig);
    va_list args;
    va_start(args, sig);
    jint result = (*env)->CallIntMethodV(env, instance, methodID, args);
    va_end(args);
    return result;
}

void naettPlatformInit(naettInitData initData) {
    globalVM = initData;
}

int naettPlatformInitRequest(InternalRequest* req) {
    JNIEnv* env = getEnv();
    (*env)->PushLocalFrame(env, 10);
    jclass URL = (*env)->FindClass(env, "java/net/URL");
    jmethodID newURL = (*env)->GetMethodID(env, URL, "<init>", "(Ljava/lang/String;)V");
    jstring urlString = (*env)->NewStringUTF(env, req->url);
    jobject url = (*env)->NewObject(env, URL, newURL, urlString);
    if (catch (env)) {
        (*env)->PopLocalFrame(env, NULL);
        return 0;
    }
    req->urlObject = (*env)->NewGlobalRef(env, url);
    (*env)->PopLocalFrame(env, NULL);
    return 1;
}

static void* processRequest(void* data) {
    const int bufSize = 10240;
    char byteBuffer[bufSize];
    InternalResponse* res = (InternalResponse*)data;
    InternalRequest* req = res->request;

    JNIEnv* env = getEnv();
    (*env)->PushLocalFrame(env, 100);

    jobject connection = call(env, req->urlObject, "openConnection", "()Ljava/net/URLConnection;");
    if (catch (env)) {
        res->code = naettConnectionError;
        goto finally;
    }

    {
        jstring name = (*env)->NewStringUTF(env, "User-Agent");
        jstring value = (*env)->NewStringUTF(env, req->options.userAgent ? req->options.userAgent : NAETT_UA);
        voidCall(env, connection, "addRequestProperty", "(Ljava/lang/String;Ljava/lang/String;)V", name, value);
        (*env)->DeleteLocalRef(env, name);
        (*env)->DeleteLocalRef(env, value);
    }

    KVLink* header = req->options.headers;
    while (header != NULL) {
        jstring name = (*env)->NewStringUTF(env, header->key);
        jstring value = (*env)->NewStringUTF(env, header->value);
        voidCall(env, connection, "addRequestProperty", "(Ljava/lang/String;Ljava/lang/String;)V", name, value);
        (*env)->DeleteLocalRef(env, name);
        (*env)->DeleteLocalRef(env, value);
        header = header->next;
    }

    jobject outputStream = NULL;
    if (strcmp(req->options.method, "POST") == 0 || strcmp(req->options.method, "PUT") == 0 ||
        strcmp(req->options.method, "PATCH") == 0 || strcmp(req->options.method, "DELETE") == 0) {
        voidCall(env, connection, "setDoOutput", "(Z)V", 1);
        outputStream = call(env, connection, "getOutputStream", "()Ljava/io/OutputStream;");
    }
    jobject methodString = (*env)->NewStringUTF(env, req->options.method);
    voidCall(env, connection, "setRequestMethod", "(Ljava/lang/String;)V", methodString);
    voidCall(env, connection, "setConnectTimeout", "(I)V", req->options.timeoutMS);
    voidCall(env, connection, "setInstanceFollowRedirects", "(Z)V", 1);

    voidCall(env, connection, "connect", "()V");
    if (catch (env)) {
        res->code = naettConnectionError;
        goto finally;
    }

    jbyteArray buffer = (*env)->NewByteArray(env, bufSize);

    if (outputStream != NULL) {
        int bytesRead = 0;
        if (req->options.bodyReader != NULL)
            do {
                bytesRead = req->options.bodyReader(byteBuffer, bufSize, req->options.bodyReaderData);
                if (bytesRead > 0) {
                    (*env)->SetByteArrayRegion(env, buffer, 0, bytesRead, (const jbyte*) byteBuffer);
                    voidCall(env, outputStream, "write", "([BII)V", buffer, 0, bytesRead);
                } else {
                    break;
                }
            } while (!res->closeRequested);
        voidCall(env, outputStream, "close", "()V");
    }

    jobject headerMap = call(env, connection, "getHeaderFields", "()Ljava/util/Map;");
    if (catch (env)) {
        res->code = naettProtocolError;
        goto finally;
    }

    jobject headerSet = call(env, headerMap, "keySet", "()Ljava/util/Set;");
    jarray headers = call(env, headerSet, "toArray", "()[Ljava/lang/Object;");
    jsize headerCount = (*env)->GetArrayLength(env, headers);

    KVLink *firstHeader = NULL;
    for (int i = 0; i < headerCount; i++) {
        jstring name = (*env)->GetObjectArrayElement(env, headers, i);
        if (name == NULL) {
            continue;
        }
        const char* nameString = (*env)->GetStringUTFChars(env, name, NULL);

        jobject values = call(env, headerMap, "get", "(Ljava/lang/Object;)Ljava/lang/Object;", name);
        jstring value = call(env, values, "get", "(I)Ljava/lang/Object;", 0);
        const char* valueString = (*env)->GetStringUTFChars(env, value, NULL);

        naettAlloc(KVLink, node);
        node->key = strdup(nameString);
        node->value = strdup(valueString);
        node->next = firstHeader;
        firstHeader = node;

        (*env)->ReleaseStringUTFChars(env, name, nameString);
        (*env)->ReleaseStringUTFChars(env, value, valueString);

        (*env)->DeleteLocalRef(env, name);
        (*env)->DeleteLocalRef(env, value);
        (*env)->DeleteLocalRef(env, values);
    }
    res->headers = firstHeader;

    const char *contentLength = naettGetHeader((naettRes *)res, "Content-Length");
    if (!contentLength || sscanf(contentLength, "%d", &res->contentLength) != 1) {
        res->contentLength = -1;
    }

    int statusCode = intCall(env, connection, "getResponseCode", "()I");

    jobject inputStream = NULL;

    if (statusCode >= 400) {
        inputStream = call(env, connection, "getErrorStream", "()Ljava/io/InputStream;");
    } else {
        inputStream = call(env, connection, "getInputStream", "()Ljava/io/InputStream;");
    }
    if (catch (env)) {
        res->code = naettProtocolError;
        goto finally;
    }

    jint bytesRead = 0;
    do {
        bytesRead = intCall(env, inputStream, "read", "([B)I", buffer);
        if (catch (env)) {
            res->code = naettReadError;
            goto finally;
        }
        if (bytesRead < 0) {
            break;
        } else if (bytesRead > 0) {
            (*env)->GetByteArrayRegion(env, buffer, 0, bytesRead, (jbyte*) byteBuffer);
            req->options.bodyWriter(byteBuffer, bytesRead, req->options.bodyWriterData);
            res->totalBytesRead += bytesRead;
        }
    } while (!res->closeRequested);

    voidCall(env, inputStream, "close", "()V");

    res->code = statusCode;

finally:
    res->complete = 1;
    (*env)->PopLocalFrame(env, NULL);
    JavaVM* vm = getVM();
    (*env)->ExceptionClear(env);
    (*vm)->DetachCurrentThread(vm);
    return NULL;
}

static void startWorkerThread(InternalResponse* res) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&res->workerThread, &attr, processRequest, res);
    pthread_setname_np(res->workerThread, "naett worker thread");
}

void naettPlatformMakeRequest(InternalResponse* res) {
    startWorkerThread(res);
}

void naettPlatformFreeRequest(InternalRequest* req) {
    JNIEnv* env = getEnv();
    (*env)->DeleteGlobalRef(env, req->urlObject);
}

void naettPlatformCloseResponse(InternalResponse* res) {
    res->closeRequested = 1;
    if (res->workerThread != 0) {
        int joinResult = pthread_join(res->workerThread, NULL);
        if (joinResult != 0) {
            LOGE("Failed to join: %s", strerror(joinResult));
        }
    }
}

#endif  // __ANDROID__
// End of inlined naett_android.c //

// End of inlined amalgam.h //
