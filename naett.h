#ifndef LIBNAETT_H
#define LIBNAETT_H

#ifdef __cplusplus
extern "C" {
#endif

void naettInit(void* initThing);

typedef struct naettReq naettReq;
typedef struct naettRes naettRes;
typedef struct naettOption naettOption;
typedef int (*naettReadFunc)(void* dest, int bufferSize, void* userData);
typedef int (*naettWriteFunc)(const void* source, int bytes, void* userData);

naettOption* naettMethod(const char* method);
naettOption* naettHeader(const char* name, const char* value);
naettOption* naettBody(const char* body, int size);
naettOption* naettBodyReader(naettReadFunc reader, void* userData);
naettOption* naettBodyWriter(naettWriteFunc writer, void* userData);
naettOption* naettTimeout(int milliSeconds);

#define naettRequest(url, ...) naettRequest_va(url, countOptions(__VA_ARGS__), ##__VA_ARGS__)
naettReq* naettRequestWithOptions(const char* url, int numOptions, const naettOption** options);
void naettFree(naettReq* request);
naettRes* naettMake(naettReq* request);

int naettComplete(const naettRes* response);
enum {
    naettConnectionError = -1,
    naettProtocolError = -2,
    naettReadError = -3,
    naettConnecting = 0,
};
int naettGetStatus(const naettRes* response);
const void* naettGetBody(naettRes* response, int* size);
const char* naettGetHeader(naettRes* response, const char* name);
void naettClose(naettRes* response);

naettReq* naettRequest_va(const char* url, int numOptions, ...);
#define countOptions(...) ((sizeof((void*[]){ __VA_ARGS__ }) / sizeof(void*)))

#ifdef __cplusplus
}
#endif

#endif  // LIBNAETT_H
