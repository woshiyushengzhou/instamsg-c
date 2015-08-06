/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Allan Stockdill-Mander/Ian Craggs - initial API and implementation and/or initial documentation
 *    Ajay Garg <ajay.garg@sensegrow.com>
 *******************************************************************************/

#include "include/config.h"
#include "include/instamsg.h"
#include "include/httpclient.h"
#include "../../../cJSON/cJSON.h"

#include <string.h>
#include <signal.h>


static void publishQoS2CycleCompleted(MQTTFixedHeaderPlusMsgId *fixedHeaderPlusMsgId)
{
    debug_log("PUBCOMP received for msg-id [%u]", fixedHeaderPlusMsgId->msgId);
}


static void NewMessageData(MessageData* md, MQTTString* aTopicName, MQTTMessage* aMessgage) {
    md->topicName = aTopicName;
    md->message = aMessgage;
}


static int getNextPacketId(InstaMsg *c) {
    int id = c->next_packetid = (c->next_packetid == MAX_PACKET_ID) ? 1 : c->next_packetid + 1;
    return id;
}


void incrementOrDecrementThreadCount(char increment)
{
    threadCountMutex.lock(&threadCountMutex);

    if(increment == 1)
    {
        threadCount++;
    }
    else
    {
        threadCount--;
    }

    threadCountMutex.unlock(&threadCountMutex);
}


void prepareThreadTerminationIfApplicable(const char *threadName)
{
    if(terminateCurrentInstance == 1)
    {
        info_log("Terminating %s", threadName);
        incrementOrDecrementThreadCount(0);
    }
}


static void attachResultHandler(InstaMsg *c, int msgId, unsigned int timeout, void (*resultHandler)(MQTTFixedHeaderPlusMsgId *))
{
    if(resultHandler == NULL)
    {
        return;
    }

    int i;

    (c->resultHandlersMutex).lock(&(c->resultHandlersMutex));
    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
    {
        if (c->resultHandlers[i].msgId == 0)
        {
            c->resultHandlers[i].msgId = msgId;
            c->resultHandlers[i].timeout = timeout;
            c->resultHandlers[i].fp = resultHandler;

            break;
        }
    }
    (c->resultHandlersMutex).unlock(&(c->resultHandlersMutex));
}


static void fireResultHandlerAndRemove(InstaMsg *c, MQTTFixedHeaderPlusMsgId *fixedHeaderPlusMsgId)
{
    int i;

    (c->resultHandlersMutex).lock(&(c->resultHandlersMutex));
    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
    {
        if (c->resultHandlers[i].msgId == fixedHeaderPlusMsgId->msgId)
        {
            c->resultHandlers[i].fp(fixedHeaderPlusMsgId);

            c->resultHandlers[i].msgId = 0;
            break;
                                                                                                                                                        }
    }
    (c->resultHandlersMutex).unlock(&(c->resultHandlersMutex));
}


static int sendPacket(InstaMsg *c, unsigned char *buf, int length, char lock)
{
    int rc = SUCCESS;

    if(lock == 1)
    {
        (c->networkPhysicalMediumMutex).lock(&(c->networkPhysicalMediumMutex));
    }

    if((c->ipstack).write(&(c->ipstack), buf, length) == FAILURE)
    {
        terminateCurrentInstance = 1;
        rc = FAILURE;
    }

    if(lock == 1)
    {
        (c->networkPhysicalMediumMutex).unlock(&(c->networkPhysicalMediumMutex));
    }

    return rc;
}


static int decodePacket(InstaMsg* c, int* value)
{
    unsigned char i;
    int multiplier = 1;
    const int MAX_NO_OF_REMAINING_LENGTH_BYTES = 4;

    *value = 0;
    do
    {
        int rc = MQTTPACKET_READ_ERROR;

        if((c->ipstack).read(&(c->ipstack), &i, 1) == FAILURE)
        {
            terminateCurrentInstance = 1;
            return FAILURE;
        }

        *value += (i & 127) * multiplier;
        multiplier *= 128;
    } while ((i & 128) != 0);

    return SUCCESS;
}


static int readPacket(InstaMsg* c, MQTTFixedHeader *fixedHeader)
{
    MQTTHeader header = {0};
    int rc = FAILURE;
    int len = 0;
    int rem_len = 0;


    /* 1. read the header byte.  This has the packet type in it
     *    (note that this function is guaranteed to succeed, since "ensure_guarantee has been passed as 1
     */
    if((c->ipstack).read(&(c->ipstack), c->readbuf, 1) == FAILURE)
    {
        terminateCurrentInstance = 1;
        return FAILURE;
    }

    len = 1;
    /* 2. read the remaining length.  This is variable in itself
     */
    if(decodePacket(c, &rem_len) == FAILURE)
    {
        goto exit;
    }

    len += MQTTPacket_encode(c->readbuf + 1, rem_len); /* put the original remaining length back into the buffer */

    /* 3. read the rest of the buffer using a callback to supply the rest of the data */
    if (rem_len > 0)
    {
        if((c->ipstack).read(&(c->ipstack), c->readbuf + len, rem_len) == FAILURE)
        {
            terminateCurrentInstance = 1;
            return FAILURE;
        }
    }

    header.byte = c->readbuf[0];
    fillFixedHeaderFieldsFromPacketHeader(fixedHeader, &header);

    rc = SUCCESS;

exit:
    return rc;
}


// assume topic filter and name is in correct format
// # can only be at end
// + and # can only be next to separator
static char isTopicMatched(char* topicFilter, MQTTString* topicName)
{
    char* curf = topicFilter;
    char* curn = topicName->lenstring.data;
    char* curn_end = curn + topicName->lenstring.len;

    while (*curf && curn < curn_end)
    {
        if (*curn == '/' && *curf != '/')
            break;
        if (*curf != '+' && *curf != '#' && *curf != *curn)
            break;
        if (*curf == '+')
        {   // skip until we meet the next separator, or end of string
            char* nextpos = curn + 1;
            while (nextpos < curn_end && *nextpos != '/')
                nextpos = ++curn + 1;
        }
        else if (*curf == '#')
            curn = curn_end - 1;    // skip until end of string
        curf++;
        curn++;
    };

    return (curn == curn_end) && (*curf == '\0');
}


static int deliverMessageToSelf(InstaMsg* c, MQTTString* topicName, MQTTMessage* message)
{
    int i;
    int rc = FAILURE;

    // we have to find the right message handler - indexed by topic
    (c->messageHandlersMutex).lock(&(c->messageHandlersMutex));
    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
    {
        if (c->messageHandlers[i].topicFilter != 0 && (MQTTPacket_equals(topicName, (char*)c->messageHandlers[i].topicFilter) ||
                isTopicMatched((char*)c->messageHandlers[i].topicFilter, topicName)))
        {
            if (c->messageHandlers[i].fp != NULL)
            {
                MessageData md;
                NewMessageData(&md, topicName, message);
                c->messageHandlers[i].fp(&md);
                rc = SUCCESS;
            }
        }
    }
    (c->messageHandlersMutex).unlock(&(c->messageHandlersMutex));

    if (rc == FAILURE && c->defaultMessageHandler != NULL)
    {
        MessageData md;
        NewMessageData(&md, topicName, message);
        c->defaultMessageHandler(&md);
        rc = SUCCESS;
    }

    /*
     * Send the ACK to the server too, if applicable
     */
    enum QoS qos = (message->fixedHeaderPlusMsgId).fixedHeader.qos;
    if (qos != QOS0)
    {
        char buf[MAX_BUFFER_SIZE];
        int len;

        if (qos == QOS1)
        {
            len = MQTTSerialize_ack(buf, MAX_BUFFER_SIZE, PUBACK, 0, (message->fixedHeaderPlusMsgId).msgId);
        }
        else if (qos == QOS2)
        {
            len = MQTTSerialize_ack(buf, MAX_BUFFER_SIZE, PUBREC, 0, (message->fixedHeaderPlusMsgId).msgId);
        }

        if (len > 0)
        {
            rc = sendPacket(c, buf, len, 1);
        }
    }

    return rc;
}


static int fireResultHandlerUsingMsgIdAsTheKey(InstaMsg *c)
{
    int msgId = -1;

    MQTTFixedHeaderPlusMsgId fixedHeaderPlusMsgId;
    if (MQTTDeserialize_FixedHeaderAndMsgId(&fixedHeaderPlusMsgId, c->readbuf, MAX_BUFFER_SIZE) == SUCCESS)
    {
        msgId = fixedHeaderPlusMsgId.msgId;
        fireResultHandlerAndRemove(c, &fixedHeaderPlusMsgId);
    }

    return msgId;
}


void* clientTimerThread(InstaMsg *c)
{
    while(1)
    {
        if(terminateCurrentInstance == 1)
        {
            prepareThreadTerminationIfApplicable("clientTimerThread");
            return;
        }

        unsigned int sleepIntervalSeconds = 1;
        thread_sleep(sleepIntervalSeconds);

        int i;

        (c->resultHandlersMutex).lock(&(c->resultHandlersMutex));
        for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
        {
            if (c->resultHandlers[i].msgId > 0)
            {
                if(c->resultHandlers[i].timeout > (sleepIntervalSeconds))
                {
                    c->resultHandlers[i].timeout = c->resultHandlers[i].timeout - (sleepIntervalSeconds);
                }
                else
                {
                    info_log("No result obtained for msgId [%u] in the specified period", c->resultHandlers[i].msgId);
                    c->resultHandlers[i].msgId = 0;
                }

                break;
            }
        }
        (c->resultHandlersMutex).unlock(&(c->resultHandlersMutex));
    }

}


void* keepAliveThread(InstaMsg *c)
{
    while(1)
    {
        if(terminateCurrentInstance == 1)
        {
            prepareThreadTerminationIfApplicable("keepAliveThread");
            return;
        }

        unsigned char buf[1000];
        int len = MQTTSerialize_pingreq(buf, 1000);
        if (len > 0)
        {
            sendPacket(c, buf, len, 1);
        }

        thread_sleep(KEEP_ALIVE_INTERVAL_SECS);
    }
}


void initInstaMsg(InstaMsg* c,
                  char *clientId,
                  char *authKey,
                  int (*connectHandler)(),
                  int (*disconnectHandler)(),
                  int (*oneToOneMessageHandler)(),
                  struct opts_struct *opts)
{
    int i;
    char clientIdMachine[MAX_BUFFER_SIZE] = {0};
    char username[MAX_BUFFER_SIZE] = {0};

    // VERY IMPORTANT: If this is not done, the "write" on an invalid socket will cause program-crash
    signal(SIGPIPE,SIG_IGN);

    readConfig(&config, "LOG_LEVEL", INTEGER, &currentLogLevel);

    {
        readConfig(&config, "USE_SERIAL_LOGGER", INTEGER, &serialLoggerEnabled);

        if(serialLoggerEnabled == 1)
        {
            init_serial_logger(&serialLogger, opts->logFilePath);

            logger_write_func = (void *) &(serialLogger.serial.write);
            logger_medium = &(serialLogger.serial);
        }
        else
        {
            init_file_logger(&fileLogger, opts->logFilePath);

            logger_write_func = (void *) &(fileLogger.fs.write);
            logger_medium = &(fileLogger.fs);
        }
    }

    {
        NetworkParameters networkParametrs;
        readConfig(&config, "INSTAMSG_HOST", STRING, &(networkParametrs.hostName));
        readConfig(&config, "INSTAMSG_PORT", INTEGER, &(networkParametrs.port));

	    init_network(&(c->ipstack), &networkParametrs);
    }

    {
        NetworkParameters networkParametrs;
        readConfig(&config, "INSTAMSG_HTTP_HOST", STRING, &(networkParametrs.hostName));
        readConfig(&config, "INSTAMSG_HTTP_PORT", INTEGER, &(networkParametrs.port));

	    init_network(&(c->httpClient), &networkParametrs);
    }

    init_system_utils(&(c->systemUtils), NULL);

    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
    {
        c->messageHandlers[i].msgId = 0;
        c->messageHandlers[i].topicFilter = 0;

        c->resultHandlers[i].msgId = 0;
        c->resultHandlers[i].timeout = 0;
    }

    c->defaultMessageHandler = NULL;
    c->next_packetid = MAX_PACKET_ID;
    c->onConnectCallback = connectHandler;
    c->onDisconnectCallback = disconnectHandler;
    c->oneToOneMessageCallback = oneToOneMessageHandler;

    init_mutex(&(c->networkPhysicalMediumMutex));
    init_mutex(&(c->messageHandlersMutex));
    init_mutex(&(c->resultHandlersMutex));

    memset(c->filesTopic, 0, MAX_BUFFER_SIZE);
    sprintf(c->filesTopic, "instamsg/clients/%s/files", clientId);

    memset(c->rebootTopic, 0, MAX_BUFFER_SIZE);
    sprintf(c->rebootTopic, "instamsg/clients/%s/reboot", clientId);

	c->connectOptions.willFlag = 0;
	c->connectOptions.MQTTVersion = 3;

    strncpy(clientIdMachine, clientId, 23);
	c->connectOptions.clientID.cstring = clientIdMachine;

    strcpy(username, clientId + 24);
	c->connectOptions.username.cstring = username;

	c->connectOptions.password.cstring = authKey;
	c->connectOptions.cleansession = 1;

    MQTTConnect(c);
}


void cleanInstaMsgObject(InstaMsg *c)
{
    /*
     * Releasing the resources in the reverse-order in which they were initiated.
     */
    release_system_utils(&(c->systemUtils));

    release_network(&(c->httpClient));
    release_network(&(c->ipstack));

    release_mutex(&(c->resultHandlersMutex));
    release_mutex(&(c->messageHandlersMutex));
    release_mutex(&(c->networkPhysicalMediumMutex));

    if(serialLoggerEnabled == 1)
    {
        release_serial_logger(&serialLogger);
    }
    else
    {
        release_file_logger(&fileLogger);
    }
}


static void getStringFromInstamsgJSON(cJSON *json, const char *key, const char **value)
{
    cJSON *objectItem = cJSON_GetObjectItem(json, key);
    if(objectItem != NULL)
    {
        *value = objectItem->valuestring;
    }
}


static const char* getValueFromParsedJSONStuff(JSONParseStuff *jsonStuff, int items, const char *key)
{
    int i;

    for(i = 0; i < items; i++)
    {
        if(strcmp(jsonStuff[i].key, key) == 0)
        {
            return jsonStuff[i].value;
        }
    }

    return NULL;
}


static void handleFileTransfer(InstaMsg *c, MQTTMessage *msg)
{
    const char *REPLY_TOPIC = "reply_to";
    const char *MESSAGE_ID = "message_id";
    const char *METHOD = "method";
    const char *URL = "url";
    const char *FILENAME = "filename";

    cJSON *json = cJSON_Parse(msg->payload);
    if(json == NULL)
    {
        error_log("Payload [%s] could not be parsed successfully :( ... not doing anything further", msg->payload);
        return;
    }

    JSONParseStuff jsonStuff[] = \
                        {
                            {
                                REPLY_TOPIC,
                                NULL,
                                1
                            },
                            {
                                MESSAGE_ID,
                                 NULL,
                                 1
                            },
                            {
                                METHOD,
                                NULL,
                                1
                            },
                            {
                                URL,
                                NULL,
                                0
                            },
                            {
                                FILENAME,
                                NULL,
                                0
                            },

                        };

    int jsonStuffLength = sizeof(jsonStuff) / sizeof(jsonStuff[0]);
    int i;

    for(i = 0; i < jsonStuffLength; i++)
    {
        getStringFromInstamsgJSON(json, jsonStuff[i].key, &(jsonStuff[i].value));
        if(jsonStuff[i].value == NULL)
        {
            if(jsonStuff[i].mandatory == 1)
            {
                error_log("Could not find field [%s] in the JSON-payload [%s] ... not proceeding further", jsonStuff[i].key, msg->payload);
                return;
            }
            else
            {
                debug_log("Could not find field [%s] in the JSON-payload [%s]", jsonStuff[i].key, msg->payload);
            }
        }
        else
        {
            debug_log("Found value ::  Key = [%s], Value = [%s] in the JSON-payload [%s]", jsonStuff[i].key, jsonStuff[i].value, msg->payload);
        }
    }


    /*
     * If we reach till here, we have successfully parsed the parameters.
     */

    /*
     * TODO: Right now following functionalities are handled ::
     *
     *      File-Download
     *
     * Following are present in Instamsg-Python... integrate in Instamsg-C too
     *
     *      File-Upload
     *      File-Listings
     *      File-Deletion
     */

    const char *replyTopic = getValueFromParsedJSONStuff(jsonStuff, jsonStuffLength, REPLY_TOPIC);
    const char *messageId = getValueFromParsedJSONStuff(jsonStuff, jsonStuffLength, MESSAGE_ID);
    const char *method =  getValueFromParsedJSONStuff(jsonStuff, jsonStuffLength, METHOD);
    const char *filename = getValueFromParsedJSONStuff(jsonStuff, jsonStuffLength, FILENAME);
    const char *url = getValueFromParsedJSONStuff(jsonStuff, jsonStuffLength, URL);


    unsigned char ackMessage[MAX_BUFFER_SIZE] = {0};

    if( (   (strcmp(method, "POST") == 0) || (strcmp(method, "PUT") == 0)   ) &&
            (filename != NULL) &&
            (url != NULL)   )
    {
        int ackStatus = 0;

        /*
         * Behaviour of File-Download Status-Notification to user
         * (as per the scenario tested, when a browser-client uploads file, and a C-client downloads the file).
         * ====================================================================================================
         *
         * While browser-client uploads the file to server, "Uploading %" is shown.
         *
         * Once the upload is complete, the C-client starts downloading, and the browser-client sees a "Waiting .."
         * note .. (in the browser-lower panel).
         *
         * Now, following scenarios arise ::
         *
         * a)
         * C-client finishes the downloading, returns status 200 and the ACK-message is sent to server
         * with status 1.
         *
         * In this case, the "Waiting .." message disappears (as expected), and an additional ioEYE-message
         * "File uploaded successfully" is displayed to browser-client.
         *
         *
         * b)
         * C-client might or might not finish downloading, but it returns a status other than 200, and the ACK-message
         * is sent to server with status 0.
         *
         * In this case, the "Waiting .." message disappears (as expected), but no additional ioEYE message is displayed.
         * (MAY BE, SOME ERROR-NOTIFICATION SHOULD BE SHOWN TO THE BROWSER-CLIENT).
         *
         *
         * c)
         * C-client might or might not finish downloading, but no ACK-message is sent to the server whatsoever.
         *
         * In this case, the "Waiting .." message is kept showing on the browser-client (posssibly timing out after
         * a long time).
         *
         *
         * ALL IN ALL, IF THE "Waiting .." MESSAGE DISAPPEARS, AND THE "File uploaded succcessfully" MESSAGE IS SEEN,
         * IT MEANS THE FILE-TRANSFER COMPLETED, AND THAT TOO PERFECTLY SUCCESSFULLY.
         *
         */
        int status = downloadFile(&(c->httpClient), url, filename, 10);
        if(status == HTTP_FILE_DOWNLOAD_SUCCESS)
        {
            ackStatus = 1;
        }
        sprintf(ackMessage, "{\"response_id\": \"%s\", \"status\": %d}", messageId, ackStatus);

    }
    else if( (strcmp(method, "GET") == 0) && (filename == NULL))
    {
        unsigned char fileList[MAX_BUFFER_SIZE] = {0};
        (c->systemUtils).getFileListing(&(c->systemUtils), fileList, MAX_BUFFER_SIZE, ".");

        info_log("File-Listing :::::: [%s]", fileList);

        sprintf(ackMessage, "{\"response_id\": \"%s\", \"status\": 1, \"files\": %s}", messageId, fileList);
    }


    /*
     * Send the acknowledgement, along with the ackStatus (success/failure).
     */
    MQTTPublish(c,
                replyTopic,
                ackMessage,
                (msg->fixedHeaderPlusMsgId).fixedHeader.qos,
                (msg->fixedHeaderPlusMsgId).fixedHeader.dup,
                NULL,
                MQTT_RESULT_HANDLER_TIMEOUT,
                0,
                1);

    cJSON_Delete(json); // IMPORTANT, else there will be memory-leak.
}


void readPacketThread(InstaMsg* c)
{
    while(1)
    {
        if(terminateCurrentInstance == 1)
        {
            prepareThreadTerminationIfApplicable("readPacketThread");
            return;
        }

        int len = 0;

        MQTTFixedHeader fixedHeader;
        readPacket(c, &fixedHeader);

        switch (fixedHeader.packetType)
        {
            case CONNACK:
            {
                unsigned char connack_rc = 255;
                char sessionPresent = 0;
                if (MQTTDeserialize_connack((unsigned char*)&sessionPresent, &connack_rc, c->readbuf, MAX_BUFFER_SIZE) == 1)
                {
                    if(connack_rc == 0x00)  // Connection Accepted
                    {
                        c->onConnectCallback();
                    }
                    else
                    {
                        info_log("Client-Connection failed with code [%d]", connack_rc);
                    }
                }

                break;
            }

            case PUBACK:
            {
                fireResultHandlerUsingMsgIdAsTheKey(c);
                break;
            }

            case SUBACK:
            {
                fireResultHandlerUsingMsgIdAsTheKey(c);

                /*
                * Remove the message-handlers, if the server was unable to process the subscription-request.
                */
                int count = 0, grantedQoS = -1;
                unsigned short msgId;

                if (MQTTDeserialize_suback(&msgId, 1, &count, &grantedQoS, c->readbuf, MAX_BUFFER_SIZE) != 1)
                {
                    goto exit;
                }

                if (grantedQoS == 0x80)
                {
                    int i;

                    (c->messageHandlersMutex).lock(&(c->messageHandlersMutex));
                    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
                    {
                        if (c->messageHandlers[i].msgId == msgId)
                        {
                            c->messageHandlers[i].topicFilter = 0;
                            break;
                        }
                    }
                    (c->messageHandlersMutex).unlock(&(c->messageHandlersMutex));
                }

                break;
            }

            case PUBLISH:
            {
                MQTTString topicPlusPayload;
                MQTTMessage msg;
                if (MQTTDeserialize_publish(&(msg.fixedHeaderPlusMsgId),
                                            &topicPlusPayload,
                                            (unsigned char**)&msg.payload,
                                            (int*)&msg.payloadlen,
                                            c->readbuf,
                                            MAX_BUFFER_SIZE) != SUCCESS)
                {
                    goto exit;
                }

                /*
                 * At this point, "msg.payload" contains the real-stuff that is passed from the peer ....
                 */
                char topicName[MAX_BUFFER_SIZE] = {0};
                snprintf(topicName, strlen(topicPlusPayload.lenstring.data) - strlen(msg.payload) + 1, "%s", topicPlusPayload.lenstring.data);

                debug_log("PUBLISH-topic = [%s]", topicName);
                debug_log("PUBLISH-message-payload = [%s]", msg.payload);

                if(topicName != NULL)
                {
                    if(strcmp(topicName, c->filesTopic) == 0)
                    {
                        handleFileTransfer(c, &msg);
                        break;
                    }
                    else if(strcmp(topicName, c->rebootTopic) == 0)
                    {
                        (c->systemUtils).rebootDevice(&(c->systemUtils));
                        break;
                    }
                }

                /*
                 * This is the last-ditch effort... if we reach till here... call this method
                 */
                deliverMessageToSelf(c, &topicPlusPayload, &msg);

                break;
            }

            case PUBREC:
            {
                int msgId = fireResultHandlerUsingMsgIdAsTheKey(c);

                char buf[MAX_BUFFER_SIZE];
                if ((len = MQTTSerialize_ack(buf, MAX_BUFFER_SIZE, PUBREL, 0, msgId)) <= 0)
                {
                    goto exit;
                }

                attachResultHandler(c, msgId, MQTT_RESULT_HANDLER_TIMEOUT, publishQoS2CycleCompleted);
                sendPacket(c, buf, len, 1); // send the PUBREL packet

                break;
            }

            case PUBCOMP:
            {
                fireResultHandlerUsingMsgIdAsTheKey(c);
                break;
            }

            case PINGRESP:
            {
                debug_log("PINGRESP received... relations are intact !!");
                break;
            }
        }

exit:
        continue;
    }
}


void* MQTTConnect(void* arg)
{
    char buf[MAX_BUFFER_SIZE];
    int len = 0;

    InstaMsg *c = (InstaMsg *)arg;
    if ((len = MQTTSerialize_connect(buf, MAX_BUFFER_SIZE, &(c->connectOptions))) <= 0)
    {
        return;
    }

    sendPacket(c, buf, len, 0);

    return NULL;
}


int MQTTSubscribe(InstaMsg* c,
                  const char* topicName,
                  const enum QoS qos,
                  messageHandler messageHandler,
                  void (*resultHandler)(MQTTFixedHeaderPlusMsgId *),
                  unsigned int resultHandlerTimeout)
{
    int rc = FAILURE;
    int len = 0;
    int id;
    char buf[MAX_BUFFER_SIZE];

    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicName;

    id = getNextPacketId(c);
    len = MQTTSerialize_subscribe(buf, MAX_BUFFER_SIZE, 0, id, 1, &topic, (int*)&qos);
    if (len <= 0)
        goto exit;

    attachResultHandler(c, id, resultHandlerTimeout, resultHandler);

    /*
     * We follow optimistic approach, and assume that the subscription will be successful, and accordingly assign the
     * message-handlers.
     *
     * If the subscription is unsuccessful, we would then remove/unsubscribe the topic.
     */
    {
        int i;

        (c->messageHandlersMutex).lock(&(c->messageHandlersMutex));
        for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
        {
            if (c->messageHandlers[i].topicFilter == 0)
            {
                c->messageHandlers[i].msgId = id;
                c->messageHandlers[i].topicFilter = topicName;
                c->messageHandlers[i].fp = messageHandler;

                break;
            }
         }
        (c->messageHandlersMutex).unlock(&(c->messageHandlersMutex));
    }

    if ((rc = sendPacket(c, buf, len, 1)) != SUCCESS) // send the subscribe packet
        goto exit;             // there was a problem

exit:
    return rc;
}


int MQTTUnsubscribe(InstaMsg* c, const char* topicFilter)
{
    int rc = FAILURE;
    char buf[MAX_BUFFER_SIZE];

    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicFilter;
    int len = 0;

    if ((len = MQTTSerialize_unsubscribe(buf, MAX_BUFFER_SIZE, 0, getNextPacketId(c), 1, &topic)) <= 0)
        goto exit;
    if ((rc = sendPacket(c, buf, len, 1)) != SUCCESS) // send the subscribe packet
        goto exit; // there was a problem

exit:
    return rc;
}


int MQTTPublish(InstaMsg* c,
                const char* topicName,
                const char* payload,
                const enum QoS qos,
                const char dup,
                void (*resultHandler)(MQTTFixedHeaderPlusMsgId *),
                unsigned int resultHandlerTimeout,
                const char retain,
                const char logging)
{
    int rc = FAILURE;
    char buf[MAX_BUFFER_SIZE];

    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicName;
    int len = 0;
    int id;

    if (qos == QOS1 || qos == QOS2)
    {
        id = getNextPacketId(c);

        /*
         * We will get PUBACK from server only for QOS1 and QOS2.
         * So, it makes sense to lodge the result-handler only for these cases.
         */
        attachResultHandler(c, id, resultHandlerTimeout, resultHandler);
    }

    len = MQTTSerialize_publish(buf, MAX_BUFFER_SIZE, 0, qos, retain, id, topic, (unsigned char*)payload, strlen(payload) + 1);
    if (len <= 0)
        goto exit;
    if ((rc = sendPacket(c, buf, len, 1)) != SUCCESS) // send the subscribe packet
        goto exit; // there was a problem

    if (qos == QOS1)
    {
    }
    else if (qos == QOS2)
    {
    }

exit:
    return rc;
}


int MQTTDisconnect(InstaMsg* c)
{
    int rc = FAILURE;
    char buf[MAX_BUFFER_SIZE];

    int len = MQTTSerialize_disconnect(buf, MAX_BUFFER_SIZE);
    if (len > 0)
        rc = sendPacket(c, buf, len, 1);            // send the disconnect packet

    c->onDisconnectCallback();

    return rc;
}

