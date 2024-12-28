#include "naett.h"
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#define delay_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define delay_ms(ms) usleep((ms) * 1000)
#endif

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Expected URL argument\n");
        return 1;
    }
    const char* URL = argv[1];

    naettInit(NULL);

    naettReq* req = naettRequest(URL, naettMethod("GET"), naettHeader("accept", "*/*"));
    naettRes* res = naettMake(req);

    while (!naettComplete(res)) delay_ms(100);

    int status = naettGetStatus(res);

    if (status < 0) {
        printf("Request failed: %d\n", status);
        return 1;
    }

    int bodyLength = 0;
    const char* body = naettGetBody(res, &bodyLength);
    printf("Got a %d, %d bytes of type '%s':\n\n", naettGetStatus(res), bodyLength, naettGetHeader(res, "Content-Type"));
    printf("%.100s\n...\n", body);

    naettClose(res);
    naettFree(req);
}
