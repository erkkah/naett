#include "../naett.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>


void fail(const char* where, const char* message) {
    printf("%s: FAIL - %s\n", where, message);
    fflush(stdout);
    exit(1);
}

void trace(const char* where, const char* message) {
    printf("%s: %s\n", where, message);
    fflush(stdout);
}

void verifyBody(naettRes* res, const char* expected) {
    int bodyLength = 0;
    const char* body = naettGetBody(res, &bodyLength);

    if (strlen(expected) != bodyLength) {
        fail(__func__, "Body length does not match expected length");
    }

    if (body != NULL && strncmp(body, expected, bodyLength) != 0) {
        printf("Expected body: [\n%s\n], got: [\n%s]\n", expected, body);
        fail(__func__, "");
    }

    const char* lengthString = naettGetHeader(res, "Content-Length");
    if (lengthString == NULL) {
        fail(__func__, "Expected 'Content-Length' header");
    }
    const int expectedLength = atoi(lengthString);
    if (bodyLength != expectedLength) {
        printf("Received body (%d) and 'Content-Length' (%d) mismatch.", bodyLength, expectedLength);
        fail(__func__, "");
    }
}

void runGETTest(const char* endpoint) {
    trace(__func__, "begin");

    char testURL[512];
    snprintf(testURL, sizeof(testURL), "%s/get", endpoint);

    naettReq* req = naettRequest(testURL, naettMethod("GET"), naettHeader("accept", "naett/testresult"));
    if (req == NULL) {
        fail(__func__, "Failed to create request");
    }

    naettRes* res = naettMake(req);
    if (res == NULL) {
        fail(__func__, "Failed to make request");
    }

    while (!naettComplete(res)) {
        usleep(100 * 1000);
    }

    int status = naettGetStatus(res);

    if (status < 0) {
        fail(__func__, "Connection failed");
    }

    verifyBody(res, "OK");

    if (naettGetStatus(res) != 200) {
        fail(__func__, "Expected 200");
    }

    naettClose(res);
    naettFree(req);

    trace(__func__, "end");
}

void runPOSTTest(const char* endpoint) {
    trace(__func__, "begin");

    char testURL[512];
    snprintf(testURL, sizeof(testURL), "%s/post", endpoint);

    naettReq* req = naettRequest(testURL, naettMethod("POST"), naettHeader("accept", "naett/testresult"), naettBody("TestRequest!", 12));
    if (req == NULL) {
        fail(__func__, "Failed to create request");
    }

    naettRes* res = naettMake(req);
    if (res == NULL) {
        fail(__func__, "Failed to make request");
    }

    while (!naettComplete(res)) {
        usleep(100 * 1000);
    }

    if (naettGetStatus(res) < 0) {
        fail(__func__, "Connection failed");
    }

    verifyBody(res, "OK");

    if (naettGetStatus(res) != 200) {
        fail(__func__, "Expected 200");
    }

    naettClose(res);
    naettFree(req);

    trace(__func__, "end");
}

void runRedirectTest(const char* endpoint) {
    trace(__func__, "begin");

    char testURL[512];
    snprintf(testURL, sizeof(testURL), "%s/redirect", endpoint);

    trace(__func__, "Creating request");
    naettReq* req = naettRequest(testURL, naettMethod("GET"));
    if (req == NULL) {
        fail(__func__, "Failed to create request");
    }

    trace(__func__, "Making request");
    naettRes* res = naettMake(req);
    if (res == NULL) {
        fail(__func__, "Failed to make request");
    }

    trace(__func__, "Waiting for completion");
    while (!naettComplete(res)) {
        usleep(100 * 1000);
    }

    if (naettGetStatus(res) < 0) {
        fail(__func__, "Connection failed");
    }

    trace(__func__, "Verifying body");
    verifyBody(res, "Redirected");

    if (naettGetStatus(res) != 200) {
        fail(__func__, "Expected 200");
    }

    naettClose(res);
    naettFree(req);

    trace(__func__, "end");
}

void runTests(const char* endpoint) {
    naettInit(NULL);
    runGETTest(endpoint);
    runPOSTTest(endpoint);
    runRedirectTest(endpoint);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Expected test endpoint argument\n");
        return 1;
    }

    const char* endpoint = argv[1];
    printf("Running tests using %s\n", endpoint);

    runTests(endpoint);
    printf("All tests pass OK\n");
    fflush(stdout);
    return 0;
}
