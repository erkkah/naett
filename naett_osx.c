#include "naett_internal.h"

#ifdef __MACOS__

#include "naett_objc.h"
#include <stdlib.h>
#include <string.h>

void naettPlatformInitRequest(InternalRequest* req) {
    id urlString = objc_msgSend_t(id, const char*)(class("NSString"), sel("stringWithUTF8String:"), req->url);
    id url = objc_msgSend_t(id, id)(class("NSURL"), sel("URLWithString:"), urlString);
    id request = objc_msgSend_t(id, id)(class("NSMutableURLRequest"), sel("requestWithURL:"), url);
    objc_msgSend_t(void, double)(request, sel("setTimeoutInterval:"), (double)(req->options.timeoutMS) / 1000.0);
    id methodString =
        objc_msgSend_t(id, const char*)(class("NSString"), sel("stringWithUTF8String:"), req->options.method);
    objc_msgSend_t(void, id)(request, sel("setHTTPMethod:"), methodString);

    KVLink* header = req->options.headers;
    while (header != NULL) {
        id name = objc_msgSend_t(id, const char*)(class("NSString"), sel("stringWithUTF8String:"), header->key);
        id value = objc_msgSend_t(id, const char*)(class("NSString"), sel("stringWithUTF8String:"), header->value);
        objc_msgSend_t(void, id, id)(request, sel("setValue:forHTTPHeaderField:"), name, value);
        header = header->next;
    }

    if (req->options.body.data) {
        Buffer* body = &req->options.body;

        id bodyData = objc_msgSend_t(id, void*, NSUInteger, BOOL)(
            class("NSData"), sel("dataWithBytesNoCopy:length:freeWhenDone:"), body->data, body->size, NO);
        objc_msgSend_t(void, id)(request, sel("setHTTPBody:"), bodyData);
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

void didComplete(id self, SEL _sel, id session, id dataTask, id error) {
    InternalResponse* res = NULL;
    object_getInstanceVariable(self, "response", (void**)&res);
    res->complete = 1;
}

void naettPlatformMakeRequest(InternalRequest* req, InternalResponse* res) {
    Class TaskDelegateClass = objc_allocateClassPair((Class)objc_getClass("NSObject"), "TaskDelegate", 0);
    addMethod(TaskDelegateClass, "URLSession:dataTask:didReceiveData:", didReceiveData, "v@:@@@");
    addMethod(TaskDelegateClass, "URLSession:task:didCompleteWithError:", didComplete, "v@:@@@");
    addIvar(TaskDelegateClass, "response", sizeof(void*), "^v");

    id config = objc_msgSend_id(class("NSURLSessionConfiguration"), sel("ephemeralSessionConfiguration"));

    id delegate = objc_msgSend_id((id)TaskDelegateClass, sel("alloc"));
    delegate = objc_msgSend_id(delegate, sel("init"));

    id session = objc_msgSend_t(id, id, id, id)(
        class("NSURLSession"), sel("sessionWithConfiguration:delegate:delegateQueue:"), config, delegate, nil);
    id task = objc_msgSend_t(id, id)(session, sel("dataTaskWithRequest:"), req->urlRequest);

    object_setInstanceVariable(delegate, "response", (void*)res);
    objc_msgSend_void(task, sel("resume"));
}

void naettPlatformFreeRequest(InternalRequest* req) {
}

void naettPlatformCloseResponse(InternalResponse* res) {
}

#endif  // __MACOS__
