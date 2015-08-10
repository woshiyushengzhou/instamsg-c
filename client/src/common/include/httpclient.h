#ifndef INSTAMSG_HTTPCLIENT
#define INSTAMSG_HTTPCLIENT

#include "./globals.h"
#include "./instamsg_vendor_common.h"

typedef struct HTTPResponse HTTPResponse;
struct HTTPResponse
{
    int status;
    char body[MAX_BUFFER_SIZE];
};


HTTPResponse downloadFile(const char *url,
                          const char *downloadedFileName,
                          KeyValuePairs *params,
                          KeyValuePairs *headers,
                          unsigned int timeout);


HTTPResponse uploadFile(const char *url,
                        const char *filename,
                        KeyValuePairs *params,
                        KeyValuePairs *headers,
                        unsigned int timeout);

#endif
