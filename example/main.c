#include "naett.h"
#include <unistd.h>
#include <stdio.h>

int main(int argc, char** argv) {
    naettInit(NULL);

    naettReq* req = naettRequest("https://www.dn.se", naettMethod("GET"), naettHeader("accept", "application/json"));
    naettRes* res = naettMake(req);
    while (!naettComplete(res)) {
        usleep(100 * 1000);
    }

    int bodyLength = 0;
    const char* body = naettGetBody(res, &bodyLength);
    printf("Got a %d, %d bytes of type '%s':\n", naettGetStatus(res), bodyLength, naettGetHeader(res, "Content-Type"));
    printf("%.100s\n...\n", body);
}
