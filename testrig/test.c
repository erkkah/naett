#include "../naett.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#if __ANDROID__
#include <android/log.h>
#define LOG(...) ((void)__android_log_print(ANDROID_LOG_INFO, "naett", __VA_ARGS__))
#else
#define LOG(...) printf(__VA_ARGS__)
#endif  // __ANDROID__

int fail(const char* where, const char* message) {
    LOG("%s: FAIL - %s\n", where, message);
    return 0;
}

void trace(const char* where, const char* message) {
    LOG("%s: %s\n", where, message);
}

int verifyBody(naettRes* res, const char* expected) {
    int bodyLength = 0;
    const char* body = naettGetBody(res, &bodyLength);

    if (body != NULL && strncmp(body, expected, bodyLength) != 0) {
        LOG("Expected body: [\n%s\n], got body of length %d: [\n%.*s]\n", expected, bodyLength, bodyLength, body);
        return fail(__func__, "");
    }

    if (strlen(expected) != bodyLength) {
        LOG("Body length (%d) does not match expected length (%lu)", bodyLength, (unsigned long)strlen(expected));
        return fail(__func__, "");
    }

    const char* lengthString = naettGetHeader(res, "Content-Length");
    if (lengthString == NULL) {
        return fail(__func__, "Expected 'Content-Length' header");
    }
    const int expectedLength = atoi(lengthString);
    if (bodyLength != expectedLength) {
        LOG("Received body (%d) and 'Content-Length' (%d) mismatch.", bodyLength, expectedLength);
        return fail(__func__, "");
    }

    return 1;
}

int runGETTest(const char* endpoint) {
    trace(__func__, "begin");

    char testURL[512];
    snprintf(testURL, sizeof(testURL), "%s/get", endpoint);

    naettReq* req = naettRequest(testURL, naettMethod("GET"), naettHeader("accept", "naett/testresult"));
    if (req == NULL) {
        return fail(__func__, "Failed to create request");
    }

    naettRes* res = naettMake(req);
    if (res == NULL) {
        return fail(__func__, "Failed to make request");
    }

    while (!naettComplete(res)) {
        usleep(100 * 1000);
    }

    int status = naettGetStatus(res);

    if (status < 0) {
        return fail(__func__, "Connection failed");
    }

    if (!verifyBody(res, "OK")) {
        return 0;
    }

    if (naettGetStatus(res) != 200) {
        return fail(__func__, "Expected 200");
    }

    naettClose(res);
    naettFree(req);

    trace(__func__, "end");

    return 1;
}

int runPOSTTest(const char* endpoint) {
    trace(__func__, "begin");

    char testURL[512];
    snprintf(testURL, sizeof(testURL), "%s/post", endpoint);

    naettReq* req = naettRequest(
        testURL, naettMethod("POST"), naettHeader("accept", "naett/testresult"), naettBody("TestRequest!", 12));
    if (req == NULL) {
        return fail(__func__, "Failed to create request");
    }

    naettRes* res = naettMake(req);
    if (res == NULL) {
        return fail(__func__, "Failed to make request");
    }

    while (!naettComplete(res)) {
        usleep(100 * 1000);
    }

    if (naettGetStatus(res) < 0) {
        return fail(__func__, "Connection failed");
    }

    if (!verifyBody(res, "OK")) {
        return 0;
    }

    if (naettGetStatus(res) != 200) {
        return fail(__func__, "Expected 200");
    }

    naettClose(res);
    naettFree(req);

    trace(__func__, "end");

    return 1;
}

int runRedirectTest(const char* endpoint) {
    trace(__func__, "begin");

    char testURL[512];
    snprintf(testURL, sizeof(testURL), "%s/redirect", endpoint);

    trace(__func__, "Creating request");
    naettReq* req = naettRequest(testURL, naettMethod("GET"));
    if (req == NULL) {
        return fail(__func__, "Failed to create request");
    }

    trace(__func__, "Making request");
    naettRes* res = naettMake(req);
    if (res == NULL) {
        return fail(__func__, "Failed to make request");
    }

    trace(__func__, "Waiting for completion");
    while (!naettComplete(res)) {
        usleep(100 * 1000);
    }

    if (naettGetStatus(res) < 0) {
        return fail(__func__, "Connection failed");
    }

    trace(__func__, "Verifying body");
    if (!verifyBody(res, "Redirected")) {
        return 0;
    }

    if (naettGetStatus(res) != 200) {
        return fail(__func__, "Expected 200");
    }

    naettClose(res);
    naettFree(req);

    trace(__func__, "end");

    return 1;
}

int runStressTest(const char* endpoint) {
    trace(__func__, "begin");

    char testURL[512];
    snprintf(testURL, sizeof(testURL), "%s/stress", endpoint);

    naettReq* req = naettRequest(testURL, naettMethod("GET"), naettHeader("accept", "naett/testresult"));
    if (req == NULL) {
        return fail(__func__, "Failed to create request");
    }

    for (int i = 0; i < 8000; i++) {
        naettRes* res = naettMake(req);
        if (res == NULL) {
            return fail(__func__, "Failed to make request");
        }

        while (!naettComplete(res)) {
            usleep(1 * 1000);
        }

        int status = naettGetStatus(res);

        if (status < 0) {
            return fail(__func__, "Connection failed");
        }

        if (!verifyBody(res, "OK")) {
            return 0;
        }

        if (naettGetStatus(res) != 200) {
            return fail(__func__, "Expected 200");
        }

        naettClose(res);
    }

    naettFree(req);

    trace(__func__, "end");

    return 1;
}

int runTests(const char* endpoint) {
    if (!runGETTest(endpoint)) {
        return 0;
    }
    if (!runPOSTTest(endpoint)) {
        return 0;
    }
    if (!runRedirectTest(endpoint)) {
        return 0;
    }
    if (!runStressTest(endpoint)) {
        return 0;
    }
    return 1;
}

#if INCLUDE_MAIN

int main(int argc, char** argv) {
    const char* endpoint = "http://localhost:4711";

    if (argc >= 2) {
        endpoint = argv[1];
    }

    printf("Running tests using %s\n", endpoint);

    naettInit(NULL);
    if (runTests(endpoint)) {
        printf("All tests pass OK\n");
        return 0;
    } else {
        printf("Tests failed!\n");
        return 1;
    }
}

#endif  // INCLUDE_MAIN
