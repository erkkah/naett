#include "naett_internal.h"

#ifdef __WINDOWS__

#include <stdlib.h>
#include <string.h>

int naettPlatformInitRequest(InternalRequest* req) {
}

void naettPlatformMakeRequest(InternalRequest* req, InternalResponse* res) {
}

void naettPlatformFreeRequest(InternalRequest* req) {
}

void naettPlatformCloseResponse(InternalResponse* res) {
}

#endif  // __WINDOWS__
