#ifndef INSTAMSG_GLOBALS
#define INSTAMSG_GLOBALS

#define MAX_BUFFER_SIZE 1000

enum returnCode
{
    SOCKET_READ_TIMEOUT = -3,
    BUFFER_OVERFLOW = -2,
    FAILURE = -1,
    SUCCESS = 0
};

enum ValueType
{
    STRING = 0,
    INTEGER
};


typedef struct KeyValuePairs KeyValuePairs;
struct KeyValuePairs
{
    char *key;
    char *value;
};


#define HTTP_FILE_DOWNLOAD_SUCCESS 200
#define HTTP_FILE_UPLOAD_SUCCESS 200

#define CONTENT_LENGTH "Content-Length"
#define POST_BOUNDARY "-----------ThIs_Is_tHe_bouNdaRY_78564$!@"

#define SOCKET_READ     "[SOCKET-READ] "
#define SOCKET_WRITE    "[SOCKET-WRITE] "



////////////////////////////////////////////////////////////////////////////////
#define INSTAMSG_HOST       "localhost"
#define INSTAMSG_PORT       1883
#define INSTAMSG_HTTP_HOST  "localhost"
#define INSTAMSG_HTTP_PORT  80
#define LOG_LEVEL           2
#define USE_SERIAL_LOGGER   0

#define NETWORK_READ_TIMEOUT_SECS 1
#define MAX_TRIES_ALLOWED_WHILE_READING_FROM_NETWORK_MEDIUM 5
///////////////////////////////////////////////////////////////////////////////

#endif
