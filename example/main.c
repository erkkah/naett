#include "naett.h"
#include <unistd.h>
#include <stdio.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Expected URL argument\n");
        return 1;
    }
    const char* URL = argv[1];

    naettInit(NULL);

    naettReq* req = naettRequest(URL, naettMethod("GET"), naettHeader("accept", "*/*"));
    naettRes* res = naettMake(req);

    while (!naettComplete(res)) {
        usleep(100 * 1000);
    }

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
