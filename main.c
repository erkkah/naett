#include "naett.h"
#include <unistd.h>

int main(int argc, char** argv) {
    naettReq* req = naettRequest("http://www.dn.se", naettMethod("GET"), naettHeader("content-type", "application/json"));
    naettRes* res = naettMake(req);
    while (!naettComplete(res)) {
        usleep(100*1000);
    }
}
