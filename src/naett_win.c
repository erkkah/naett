#include "naett_internal.h"

#ifdef __WINDOWS__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <winhttp.h>

void naettPlatformInit(naettInitData initData) {
}

char* winToUTF8(LPWSTR source) {
    int length = WideCharToMultiByte(CP_UTF8, 0, source, -1, NULL, 0, NULL, NULL);
    char* chars = (char*)malloc(length);
    int result = WideCharToMultiByte(CP_UTF8, 0, source, -1, chars, length, NULL, NULL);
    if (!result) {
        free(chars);
        return NULL;
    }
    return chars;
}

LPWSTR winFromUTF8(const char* source) {
    int length = MultiByteToWideChar(CP_UTF8, 0, source, -1, NULL, 0);
    LPWSTR chars = (LPWSTR)malloc(length * sizeof(WCHAR));
    int result = MultiByteToWideChar(CP_UTF8, 0, source, -1, chars, length);
    if (!result) {
        free(chars);
        return NULL;
    }
    return chars;
}

#define ASPRINTF(result, fmt, ...)                        \
    {                                                     \
        size_t len = snprintf(NULL, 0, fmt, __VA_ARGS__); \
        *(result) = (char*)malloc(len + 1);               \
        snprintf(*(result), len + 1, fmt, __VA_ARGS__);   \
    }

LPCWSTR packHeaders(InternalRequest* req) {
    char* packed = strdup("");

    KVLink* node = req->options.headers;
    while (node != NULL) {
        char* update;
        ASPRINTF(&update, "%s%s=%s%s", packed, node->key, node->value, node->next ? "\r\n" : "");
        free(packed);
        packed = update;
        node = node->next;
    }

    LPCWSTR winHeaders = winFromUTF8(packed);
    free(packed);
    return winHeaders;
}

static void unpackHeaders(InternalResponse* res, LPWSTR packed) {
    int len = 0;
    while ((len = wcslen(packed)) != 0) {
        char* header = winToUTF8(packed);
        char* split = strchr(header, ':');
        if (split) {
            *split = 0;
            split++;
            while (*split == ' ') {
                split++;
            }
            naettAlloc(KVLink, node);
            node->key = strdup(header);
            node->value = strdup(split);
            node->next = res->headers;
            res->headers = node;
        }
        free(header);
        packed += len + 1;
    }
}

static void callback(HINTERNET request,
    DWORD_PTR context,
    DWORD status,
    LPVOID statusInformation,
    DWORD statusInfoLength) {
    InternalResponse* res = (InternalResponse*)context;

    switch (status) {
        case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE: {
            DWORD bufSize = 0;
            WinHttpQueryHeaders(request,
                WINHTTP_QUERY_RAW_HEADERS,
                WINHTTP_HEADER_NAME_BY_INDEX,
                NULL,
                &bufSize,
                WINHTTP_NO_HEADER_INDEX);
            LPWSTR buffer = (LPWSTR)malloc(bufSize);
            WinHttpQueryHeaders(request,
                WINHTTP_QUERY_RAW_HEADERS,
                WINHTTP_HEADER_NAME_BY_INDEX,
                buffer,
                &bufSize,
                WINHTTP_NO_HEADER_INDEX);
            unpackHeaders(res, buffer);
            free(buffer);

            DWORD statusCode = 0;
            DWORD statusCodeSize = sizeof(statusCode);

            WinHttpQueryHeaders(request,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &statusCode,
                &statusCodeSize,
                WINHTTP_NO_HEADER_INDEX);
            res->code = statusCode;

            if (!WinHttpQueryDataAvailable(request, NULL)) {
                res->code = naettProtocolError;
                res->complete = 1;
            }
        } break;

        case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE: {
            DWORD* available = (DWORD*)statusInformation;
            res->bytesLeft = *available;
            if (res->bytesLeft == 0) {
                res->complete = 1;
                break;
            }

            size_t bytesToRead = min(res->bytesLeft, sizeof(res->buffer));
            if (!WinHttpReadData(request, res->buffer, bytesToRead, NULL)) {
                res->code = naettReadError;
                res->complete = 1;
            }
        }break;

        case WINHTTP_CALLBACK_STATUS_READ_COMPLETE: {
            size_t bytesRead = statusInfoLength;

            InternalRequest* req = res->request;
            if (req->options.bodyWriter(res->buffer, bytesRead, req->options.bodyWriterData) != bytesRead) {
                res->code = naettReadError;
                res->complete = 1;
            }

            res->bytesLeft -= bytesRead;
            if (res->bytesLeft > 0) {
                size_t bytesToRead = min(res->bytesLeft, sizeof(res->buffer));
                if (!WinHttpReadData(request, res->buffer, bytesToRead, NULL)) {
                    res->code = naettReadError;
                    res->complete = 1;
                }
            } else {
                if (!WinHttpQueryDataAvailable(request, NULL)) {
                    res->code = naettProtocolError;
                    res->complete = 1;
                }
            }
        } break;

        case WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE:
        case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE: {
            int bytesRead = res->request->options.bodyReader(
                res->buffer, sizeof(res->buffer), res->request->options.bodyReaderData);
            if (bytesRead) {
                WinHttpWriteData(request, res->buffer, bytesRead, NULL);
            } else {
                if (!WinHttpReceiveResponse(request, NULL)) {
                    res->code = naettReadError;
                    res->complete = 1;
                }
            }
        } break;

        //
        case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR: {
            WINHTTP_ASYNC_RESULT* result = (WINHTTP_ASYNC_RESULT*)statusInformation;
            switch (result->dwResult) {
                case API_RECEIVE_RESPONSE:
                case API_QUERY_DATA_AVAILABLE:
                case API_READ_DATA:
                    res->code = naettReadError;
                    break;
                case API_WRITE_DATA:
                    res->code = naettWriteError;
                    break;
                case API_SEND_REQUEST:
                    res->code = naettConnectionError;
                    break;
                default:
                    res->code = naettGenericError;
            }

            res->complete = 1;
        } break;
    }
}

int naettPlatformInitRequest(InternalRequest* req) {
    LPWSTR url = winFromUTF8(req->url);

    URL_COMPONENTS components;
    ZeroMemory(&components, sizeof(components));
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = (DWORD)-1;
    components.dwHostNameLength = (DWORD)-1;
    components.dwUrlPathLength = (DWORD)-1;
    components.dwExtraInfoLength = (DWORD)-1;
    BOOL cracked = WinHttpCrackUrl(url, 0, 0, &components);

    if (!cracked) {
        free(url);
        return 0;
    }

    req->host = wcsncat(wcsdup(L""), components.lpszHostName, components.dwHostNameLength);
    req->resource = wcsncat(wcsdup(L""), components.lpszUrlPath, components.dwUrlPathLength);
    free(url);

    req->session = WinHttpOpen(
        L"Naett/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);

    if (!req->session) {
        return 0;
    }

    WinHttpSetStatusCallback(req->session, callback, WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS, 0);

    req->connection = WinHttpConnect(req->session, req->host, components.nPort, 0);
    if (!req->connection) {
        WinHttpCloseHandle(req->session);
        return 0;
    }

    LPWSTR verb = winFromUTF8(req->options.method);
    req->request = WinHttpOpenRequest(req->connection,
        verb,
        req->resource,
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        components.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    free(verb);
    if (!req->request) {
        WinHttpCloseHandle(req->session);
        WinHttpCloseHandle(req->connection);
        return 0;
    }

    LPCWSTR headers = packHeaders(req);
    WinHttpAddRequestHeaders(req->request, headers, 0, WINHTTP_ADDREQ_FLAG_ADD);
    free((LPWSTR)headers);

    return 1;
}

void naettPlatformMakeRequest(InternalResponse* res) {
    if (!WinHttpSendRequest(res->request->request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, (DWORD_PTR)res)) {
        res->code = naettConnectionError;
        res->complete = 1;
    }
}

void naettPlatformFreeRequest(InternalRequest* req) {
    WinHttpCloseHandle(req->session);
    WinHttpCloseHandle(req->connection);
    WinHttpCloseHandle(req->request);
    free(req->host);
    free(req->resource);
}

void naettPlatformCloseResponse(InternalResponse* res) {
}

#endif  // __WINDOWS__
