#include "./include/globals.h"
#include "./include/instamsg.h"
#include "./include/httpclient.h"
#include "./include/sg_stdlib.h"
#include "./include/config.h"
#include "./include/upgrade.h"

#include "device_file_system.h"
#include <string.h>

#define HTTP_RESPONSE_STATUS_PREFIX "HTTP/"


static int getNextLine(Socket *socket, char *buf, int *responseCode)
{
    while(1)
    {
        char ch[2] = {0};

        if(socket->read(socket, (unsigned char*)ch, 1, 1) == FAILURE) /* Pseudo-Blocking Call */
        {
            return FAILURE;
        }

        if(ch[0] == '\n')
        {
            if(strncmp(buf, HTTP_RESPONSE_STATUS_PREFIX, strlen(HTTP_RESPONSE_STATUS_PREFIX)) == 0)
            {
                char *saveptr;
                char *firstToken = strtok_r(buf, " ", &saveptr);
                if(firstToken != NULL)
                {
                    char *secondToken = strtok_r(NULL, " ", &saveptr);
                    *responseCode = sg_atoi(secondToken);

                    if(*responseCode != HTTP_FILE_DOWNLOAD_SUCCESS)
                    {
                        sg_sprintf(LOG_GLOBAL_BUFFER, "%s%sResponse-Code is not %d, instead %d", FILE_UPLOAD, FILE_DOWNLOAD,
                                                                                                 HTTP_FILE_DOWNLOAD_SUCCESS, *responseCode);
                        error_log(LOG_GLOBAL_BUFFER);

                        return FAILURE;
                    }
                }
            }

            return SUCCESS;
        }
        else
        {
            if(ch[0] != '\r')
            {
                strcat(buf, ch);
            }
        }
    }

    return FAILURE;
}


static void generateRequest(const char *requestType,
                            const char *url,
                            KeyValuePairs *params,
                            KeyValuePairs *headers,
                            char *buf,
                            int maxLenAllowed,
                            unsigned char addFinalDelimiter)
{
    /*
     * Add the "GET" and "/1.txt"
     */
    sg_sprintf(buf, "%s %s", requestType, url);

    /*
     * Append the parameters (if any).
     */
    if(params != NULL)
    {
        int i = 0;
        while(1)
        {
            if(params[i].key == NULL)
            {
                break;
            }

            if(i == 0)
            {
                strcat(buf, "?");
            }
            else
            {
                strcat(buf, "&");
            }

            strcat(buf, params[i].key);
            strcat(buf, "=");
            strcat(buf, params[i].value);

            i++;
        }
    }

    /*
     * Add the "HTTP/1.0\r\n" part.
     */
    strcat(buf, " HTTP/1.0\r\n");

    /*
     * Add the headers (if any)
     */
    if(headers != NULL)
    {
        int i = 0;
        while(1)
        {
            if(headers[i].key == NULL)
            {
                break;
            }

            strcat(buf, headers[i].key);
            strcat(buf, ": ");
            strcat(buf, headers[i].value);
            strcat(buf, "\r\n");

            i++;
        }
    }

    /*
     * Finally, add the delimiter.
     */
    strcat(buf, "\r\n");
}


static long getBytesIfContentLengthBytes(char *line)
{
    unsigned long numBytes = 0;

    char *saveptr;
    char *headerKey = strtok_r(line, ":", &saveptr);
    char *headerValue = strtok_r(NULL, ":", &saveptr);

    if(headerKey && headerValue)
    {
        if(strcmp(headerKey, CONTENT_LENGTH) == 0)
        {
            numBytes = sg_atoi(headerValue);
        }
    }

    return numBytes;
}


#if FILE_SYSTEM_ENABLED == 1
/*
 * BYTE-LEVEL-REQUEST ::
 * ======================
 *
 * POST /api/beta/clients/00125580-e29a-11e4-ace1-bc764e102b63/files HTTP/1.0
 * Authorization: password
 * ClientId: 00125580-e29a-11e4-ace1-bc764e102b63
 * Content-Type: multipart/form-data; boundary=-----------ThIs_Is_tHe_bouNdaRY_78564$!@
 * Content-Length: 340
 *
 * -------------ThIs_Is_tHe_bouNdaRY_78564$!@
 *  Content-Disposition: form-data; name="file"; filename="filetester.sh"
 *  Content-Type: application/octet-stream
 *
 *  ./stdoutsub listener_topic --qos 2 --clientid 00125580-e29a-11e4-ace1-bc764e102b63 --password password --log /home/ajay/filetester --sub
 *
 *  -------------ThIs_Is_tHe_bouNdaRY_78564$!@--
 *
 *
 * BYTE-LEVEL-RESPONSE ::
 * =======================
 *
 * HTTP/1.0 200 OK
 * Content-Length: 89
 *
 * http://platform.instamsg.io:8081/files/1325d1f4-a585-4dbd-84e7-d4c6cfa6fd9d.filetester.sh
 */
HTTPResponse uploadFile(const char *url,
                        const char *filename,
                        KeyValuePairs *params,
                        KeyValuePairs *headers,
                        unsigned int timeout)
{

    int i = 0;
    unsigned int numBytes = 0;

    Socket socket;
    HTTPResponse response = {0};
    FileSystem fs;

    unsigned int totalLength;
    char *request, *secondLevel, *fourthLevel;

    request = (char *) sg_malloc(MAX_BUFFER_SIZE);
    secondLevel = (char *) sg_malloc(MAX_BUFFER_SIZE);
    fourthLevel = (char *) sg_malloc(MAX_BUFFER_SIZE);

    if((request == NULL) || (secondLevel == NULL) || (fourthLevel == NULL))
    {
        sg_sprintf(LOG_GLOBAL_BUFFER, PROSTR("%sFailure in memory allocation for uploadFile"), FILE_UPLOAD);
        error_log(LOG_GLOBAL_BUFFER);

        goto exit;
    }
    memset(request, 0, MAX_BUFFER_SIZE);
    memset(secondLevel, 0, MAX_BUFFER_SIZE);
    memset(fourthLevel, 0, MAX_BUFFER_SIZE);

	init_socket(&socket, INSTAMSG_HTTP_HOST, INSTAMSG_HTTP_PORT);

    /* Now, generate the second-level (form) data
     * Please consult ::
     *
     *          http://stackoverflow.com/questions/8659808/how-does-http-file-upload-work
     */
    sg_sprintf(secondLevel, "--%s"                                                                   \
                         "\r\n"                                                                 \
                         "Content-Disposition: form-data; name=\"file\"; filename=\"%s\""       \
                         "\r\n"                                                                 \
                         "Content-Type: application/octet-stream"                               \
                         "\r\n\r\n", POST_BOUNDARY, filename);

    sg_sprintf(fourthLevel, "\r\n--%s--", POST_BOUNDARY);

    /*
     * Add the "Content-Length header
     */
    numBytes = instaMsg.singletonUtilityFs.getFileSize(&(instaMsg.singletonUtilityFs), filename);
    totalLength = strlen(secondLevel) + numBytes + strlen(fourthLevel);
    i = 0;

    {
        RESET_GLOBAL_BUFFER;

        while(1)
        {
            if(headers[i].key == NULL)
            {
                break;
            }

            if(strcmp(headers[i].key, CONTENT_LENGTH) == 0)
            {
                sg_sprintf((char*)GLOBAL_BUFFER, "%u", totalLength);
                headers[i].value = (char*)GLOBAL_BUFFER;
            }

            i++;
        }

        generateRequest("POST", url, params, headers, request, MAX_BUFFER_SIZE, 0);

        sg_sprintf(LOG_GLOBAL_BUFFER, PROSTR("%sFirst-stage URL that will be hit : [%s]"), FILE_UPLOAD, request);
        info_log(LOG_GLOBAL_BUFFER);
    }

    if(socket.write(&socket, (unsigned char*)request, strlen(request)) == FAILURE)
    {
        sg_sprintf(LOG_GLOBAL_BUFFER, PROSTR("%sError occurred while uploading POST data (FIRST LEVEL) for [%s]"), FILE_UPLOAD, filename);
        error_log(LOG_GLOBAL_BUFFER);

        goto exit;
    }

    if(socket.write(&socket, (unsigned char*)secondLevel, strlen(secondLevel)) == FAILURE)
    {
        sg_sprintf(LOG_GLOBAL_BUFFER, PROSTR("%sError occurred while uploading POST data (SECOND LEVEL) for [%s]"), FILE_UPLOAD, filename);
        error_log(LOG_GLOBAL_BUFFER);

        goto exit;
    }

    /*
     * Now, upload the actual file-data
     */
    init_file_system(&fs, (void *)filename);

    for(i = 0; i < numBytes; i++)
    {
        char ch[2] = {0};

        fs.read(&fs, (unsigned char*)ch, 1, 1);
        if(socket.write(&socket, (unsigned char*)ch, 1) == FAILURE)
        {
            sg_sprintf(LOG_GLOBAL_BUFFER, PROSTR("%sError occurred while uploading POST data (THIRD LEVEL) for [%s]"), FILE_UPLOAD, filename);
            error_log(LOG_GLOBAL_BUFFER);

            release_file_system(&fs);
            goto exit;
        }
    }

    sg_sprintf(LOG_GLOBAL_BUFFER, PROSTR("%sFile [%s] successfully uploaded worth [%u] bytes"), FILE_UPLOAD, filename, numBytes);
    info_log(LOG_GLOBAL_BUFFER);

    release_file_system(&fs);
    if(socket.write(&socket, (unsigned char*)fourthLevel, strlen(fourthLevel)) == FAILURE)
    {
        sg_sprintf(LOG_GLOBAL_BUFFER, PROSTR("%sError occurred while uploading POST data (FOURTH LEVEL) for [%s]"), FILE_UPLOAD, filename);
        error_log(LOG_GLOBAL_BUFFER);

        goto exit;
    }


    numBytes = 0;
    while(1)
    {
        char beginPayloadDownload = 0;
        char *newLine;

        RESET_GLOBAL_BUFFER;

        newLine = (char*)GLOBAL_BUFFER;
        strcpy(newLine, "");

        if(getNextLine(&socket, newLine, &(response.status)) == FAILURE)
        {
            sg_sprintf(LOG_GLOBAL_BUFFER, PROSTR("%sSocket error while reading URL-payload for uploaded file [%s] (stage 1)"),
                                          FILE_UPLOAD, filename);
            error_log(LOG_GLOBAL_BUFFER);

            goto exit;
        }

        /*
         * The actual payload begins after we receive an empty line.
         * Here, the payload contains the URL that needs to be passed to the peer.
         */
        if(strlen(newLine) == 0)
        {
            beginPayloadDownload = 1;
        }

        if(numBytes == 0)
        {
            numBytes = getBytesIfContentLengthBytes(newLine);
        }

        if(beginPayloadDownload == 1)
        {
            if(socket.read(&socket, (unsigned char*)response.body, numBytes, 1) == FAILURE) /* Pseudo-Blocking Call */
            {
                sg_sprintf(LOG_GLOBAL_BUFFER, PROSTR("%sSocket error while reading URL-payload for uploaded file [%s] (stage 2)"),
                                              FILE_UPLOAD, filename);
                error_log(LOG_GLOBAL_BUFFER);

                goto exit;
            }
            else
            {
                sg_sprintf(LOG_GLOBAL_BUFFER,
                           PROSTR("%sURL being provided to peer for uploaded file [%s] is [%s]"), FILE_UPLOAD, filename, response.body);
                info_log(LOG_GLOBAL_BUFFER);

                break;
            }
        }
    }

exit:

    if(fourthLevel)
        sg_free(fourthLevel);

    if(secondLevel)
        sg_free(secondLevel);

    if(request)
        sg_free(request);

    release_socket(&socket);

    sg_sprintf(LOG_GLOBAL_BUFFER, PROSTR("%sHTTP-Response Status = [%d]"), FILE_UPLOAD, response.status);
    info_log(LOG_GLOBAL_BUFFER);

    return response;
}
#endif


/*
 * Either of the URLs form work ::
 *
 *      http://platform.instamsg.io:8081/files/d2f9d9e7-e98b-4777-989e-605073a55efd.0003-Missed-a-path-export.patch
 *      /files/d2f9d9e7-e98b-4777-989e-605073a55efd.0003-Missed-a-path-export.patch
 */


/*
 * BYTE-LEVEL-REQUEST ::
 * ======================
 *
 * GET /files/1.txt HTTP/1.0\r\n\r\n
 *
 *
 * BYTE-LEVEL-RESPONSE ::
 * =======================
 *
 * HTTP/1.1 200 OK
 * Date: Wed, 05 Aug 2015 09:43:26 GMT
 * Server: Apache/2.4.7 (Ubuntu)
 * Last-Modified: Wed, 05 Aug 2015 09:14:51 GMT
 * ETag: "f-51c8cd5d313d7"
 * Accept-Ranges: bytes
 * Content-Length: 15
 * Connection: close
 * Content-Type: text/plain
 *
 * echo "hi ajay"
*/
HTTPResponse downloadFile(const char *url,
                          const char *filename,
                          KeyValuePairs *params,
                          KeyValuePairs *headers,
                          unsigned int timeout)
{
    Socket socket;
    HTTPResponse response = {0};

    unsigned int numBytes;

	init_socket(&socket, INSTAMSG_HTTP_HOST, INSTAMSG_HTTP_PORT);

    {
        char *urlComplete;

        RESET_GLOBAL_BUFFER;
        urlComplete = (char*) GLOBAL_BUFFER;

        generateRequest("GET", url, params, headers, urlComplete, MAX_BUFFER_SIZE, 1);

        sg_sprintf(LOG_GLOBAL_BUFFER, PROSTR("%sComplete URL that will be hit : [%s]"), FILE_DOWNLOAD, urlComplete);
        info_log(LOG_GLOBAL_BUFFER);

        /*
        * Fire the request-bytes over the socket-medium.
        */
        if(socket.write(&socket, (unsigned char*)urlComplete, strlen(urlComplete)) == FAILURE)
        {
            goto exit;
        }
    }

    numBytes = 0;
    while(1)
    {
        char beginPayloadDownload = 0;

        {
            char *newLine;

            RESET_GLOBAL_BUFFER;
            newLine = (char*)GLOBAL_BUFFER;

            strcpy(newLine, "");
            if(getNextLine(&socket, newLine, &(response.status)) == FAILURE)
            {
                sg_sprintf(LOG_GLOBAL_BUFFER, PROSTR("%sError downloading file-metadata"), FILE_DOWNLOAD);
                info_log(LOG_GLOBAL_BUFFER);

                goto exit;
            }

            /*
            * The actual file-payload begins after we receive an empty line.
            */
            if(strlen(newLine) == 0)
            {
                beginPayloadDownload = 1;
            }

            if(numBytes == 0)
            {
                numBytes = getBytesIfContentLengthBytes(newLine);
            }
        }

        if(beginPayloadDownload == 1)
        {
            long i;

            prepare_for_new_binary_download();

            /* Now, we need to start reading the bytes */
            sg_sprintf(LOG_GLOBAL_BUFFER, PROSTR("%sBeginning downloading worth [%u] bytes"), FILE_DOWNLOAD, numBytes);
            info_log(LOG_GLOBAL_BUFFER);

            for(i = 0; i < numBytes; i++)
            {
                char ch[2] = {0};

                if(socket.read(&socket, (unsigned char*)ch, 1, 1) == FAILURE) /* Pseudo-Blocking Call */
                {
                    tear_down_binary_download();
                    goto exit;
                }

                copy_next_char(ch[0]);
            }

            tear_down_binary_download();


            /*
             * Mark that file was downloaded successfully.
             */
            RESET_GLOBAL_BUFFER;
            generate_config_json((char*)GLOBAL_BUFFER, NEW_FILE_KEY, CONFIG_STRING, NEW_FILE_ARRIVED, "");
            save_config_value_on_persistent_storage(NEW_FILE_KEY, (char*)GLOBAL_BUFFER, 1);

            sg_sprintf(LOG_GLOBAL_BUFFER, PROSTR("%sFile-Download SUCCESS !!!!!!!!!!"), FILE_DOWNLOAD);
            info_log(LOG_GLOBAL_BUFFER);

exit:
            release_socket(&socket);

            sg_sprintf(LOG_GLOBAL_BUFFER, PROSTR("%sHTTP-Response Status = [%d]"), FILE_DOWNLOAD, response.status);
            info_log(LOG_GLOBAL_BUFFER);

            return response;
        }
    }
}
