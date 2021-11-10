#include "naett_internal.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

typedef struct InternalOption* InternalOptionPtr;
typedef void (*OptionSetter)(InternalOptionPtr option, InternalRequest* req);

typedef struct InternalOption {
    OptionSetter setter;
    int offset;
    union {
        int integer;
        const char* string;
        struct {
            const char* key;
            const char* value;
        } kv;
        void* ptr;
        Buffer buffer;
    };
} InternalOption;

void stringSetter(InternalOptionPtr option, InternalRequest* req) {
    char* stringCopy = strdup(option->string);
    char* opaque = (char*) &req->options;
    char** stringField = (char**) (opaque + option->offset);
    if (*stringField) {
        free(*stringField);
    }
    *stringField = stringCopy;
}

void intSetter(InternalOptionPtr option, InternalRequest* req) {
    char* opaque = (char*) &req->options;
    int* intField = (int*) (opaque + option->offset);
    *intField = option->integer;
}

void ptrSetter(InternalOptionPtr option, InternalRequest* req) {
    char* opaque = (char*) &req->options;
    void** ptrField = (void**) (opaque + option->offset);
    *ptrField = option->ptr;
}

void kvSetter(InternalOptionPtr option, InternalRequest* req) {
    char* opaque = (char*) &req->options;
    KVLink** kvField = (KVLink**) (opaque + option->offset);
    
    naettAlloc(KVLink, newNode);
    newNode->key = strdup(option->kv.key);
    newNode->value = strdup(option->kv.value);
    newNode->next = *kvField;
    *kvField = newNode;
}

void bufferSetter(InternalOptionPtr option, InternalRequest* req) {
    char* opaque = (char*) &req->options;
    Buffer* bufferField = (Buffer*) (opaque + option->offset);
    
    bufferField->data = option->buffer.data;
    bufferField->size = option->buffer.size;
}

void initRequest(InternalRequest* req, const char* url) {
    req->options.method = strdup("GET");
    req->url = strdup(url);
}

// Public API

naettOption* naettMethod(const char* method) {
    naettAlloc(InternalOption, option);
    option->string = method;
    option->offset = offsetof(RequestOptions, method);
    option->setter = stringSetter;
    return (naettOption*) option;
}

naettOption* naettHeader(const char* name, const char* value) {
    naettAlloc(InternalOption, option);
    option->kv.key = name;
    option->kv.value = value;
    option->offset = offsetof(RequestOptions, headers);
    option->setter = kvSetter;
    return (naettOption*) option;
}

naettOption* naettTimeout(int timeoutMS) {
    naettAlloc(InternalOption, option);
    option->integer = timeoutMS;
    option->offset = offsetof(RequestOptions, timeoutMS);
    option->setter = intSetter;
    return (naettOption*) option;
}

naettOption* naettBody(const char* body, int size) {
    naettAlloc(InternalOption, option);
    option->buffer.data = (void*) body;
    option->buffer.size = size;
    option->offset = offsetof(RequestOptions, body);
    option->setter = bufferSetter;
    return (naettOption*) option;
}

naettOption* naettBodyReader(naettReadFunc reader) {
    naettAlloc(InternalOption, option);
    option->ptr = reader;
    option->offset = offsetof(RequestOptions, bodyReader);
    option->setter = ptrSetter;
    return (naettOption*) option;
}

naettOption* naettBodyWriter(naettWriteFunc writer) {
    naettAlloc(InternalOption, option);
    option->ptr = writer;
    option->offset = offsetof(RequestOptions, bodyWriter);
    option->setter = ptrSetter;
    return (naettOption*) option;
}

naettReq* naettRequest_va(const char* url, int numArgs, ...) {
    va_list args;
    InternalOption* option;
    naettAlloc(InternalRequest, req);
    initRequest(req, url);

    va_start(args, numArgs);
    for (int i = 0; i < numArgs; i++) {
        option = va_arg(args, InternalOption*);
        option->setter(option, req);
        free(option);
    }
    va_end(args);

    naettPlatformInitRequest(req);
    return (naettReq*) req;
}

naettReq* naettRequestWithOptions(const char* url, int numOptions, const naettOption** options) {
    naettAlloc(InternalRequest, req);
    initRequest(req, url);

    for (int i = 0; i < numOptions; i++) {
        InternalOption* option = (InternalOption*) options[i];
        option->setter(option, req);
        free(option);
    }

    naettPlatformInitRequest(req);
    return (naettReq*) req;
}

int naettComplete(const naettRes* response) {
    InternalResponse* res = (InternalResponse*) response;
    return res->complete;
}
