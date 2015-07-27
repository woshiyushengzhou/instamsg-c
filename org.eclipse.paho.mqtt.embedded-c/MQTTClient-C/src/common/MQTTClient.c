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
 *******************************************************************************/

#include "./include/MQTTClient.h"
#include <string.h>

#define SEND_BUFFER_SIZE 100

void* clientTimerThread(Client *c)
{
    while(1)
    {
        unsigned int sleepIntervalSeconds = 1;
        thread_sleep(sleepIntervalSeconds);

        int i;

        c->resultHandlersMutex->lock(c->resultHandlersMutex);
        for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
        {
            if (c->resultHandlers[i].msgId > 0)
            {
                if(c->resultHandlers[i].timeout > (sleepIntervalSeconds * 1000))
                {
                    c->resultHandlers[i].timeout = c->resultHandlers[i].timeout - (sleepIntervalSeconds * 1000);
                }
                else
                {
                    printf("No result obtained for msgId [%u] in the specified period\n", c->resultHandlers[i].msgId);
                    c->resultHandlers[i].msgId = 0;
                }

                break;
            }
        }
        c->resultHandlersMutex->unlock(c->resultHandlersMutex);
    }

}

void* keepAliveThread(Client *c)
{
    while(1)
    {
        unsigned char buf[1000];
        int len = MQTTSerialize_pingreq(buf, 1000);
        if (len > 0)
        {
            sendPacket(c, buf, len);
        }

        thread_sleep(c->keepAliveInterval);
    }
}

void NewMessageData(MessageData* md, MQTTString* aTopicName, MQTTMessage* aMessgage) {
    md->topicName = aTopicName;
    md->message = aMessgage;
}


int getNextPacketId(Client *c) {
    int id = c->next_packetid = (c->next_packetid == MAX_PACKET_ID) ? 1 : c->next_packetid + 1;
    return id;
}

void attachResultHandler(Client *c, int msgId, unsigned int timeout, void (*resultHandler)(MQTTFixedHeaderPlusMsgId *))
{
    int i;

    c->resultHandlersMutex->lock(c->resultHandlersMutex);
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
    c->resultHandlersMutex->unlock(c->resultHandlersMutex);
}

void fireResultHandlerAndRemove(Client *c, MQTTFixedHeaderPlusMsgId *fixedHeaderPlusMsgId)
{
    int i;

    c->resultHandlersMutex->lock(c->resultHandlersMutex);
    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
    {
        if (c->resultHandlers[i].msgId == fixedHeaderPlusMsgId->msgId)
        {
            c->resultHandlers[i].fp(fixedHeaderPlusMsgId);

            c->resultHandlers[i].msgId = 0;
            break;
                                                                                                                                                        }
    }
    c->resultHandlersMutex->unlock(c->resultHandlersMutex);
}

int sendPacket(Client *c, unsigned char *buf, int length)
{
    c->sendPacketMutex->lock(c->sendPacketMutex);

    int rc = FAILURE,
        sent = 0;

    while (sent < length)
    {
        rc = c->ipstack->mqttwrite(c->ipstack, &(buf[sent]), length);
        if (rc < 0)  // there was an error writing the data
        {
            break;
        }

        sent += rc;
    }

    if (sent == length)
    {
        rc = SUCCESS;
    }
    else
    {
        rc = FAILURE;
    }

    c->sendPacketMutex->unlock(c->sendPacketMutex);
    return rc;
}


void MQTTClient(Client* c, Network* network, unsigned int command_timeout_ms,
                unsigned char* readbuf, size_t readbuf_size,
                int (*onConnect)())
{
    int i;
    c->ipstack = network;

    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
    {
        c->messageHandlers[i].msgId = 0;
        c->messageHandlers[i].topicFilter = 0;

        c->resultHandlers[i].msgId = 0;
        c->resultHandlers[i].timeout = 0;
    }

    c->command_timeout_ms = command_timeout_ms;
    c->readbuf = readbuf;
    c->readbuf_size = readbuf_size;
    c->isconnected = 0;
    c->keepAliveInterval = 0;
    c->defaultMessageHandler = NULL;
    c->next_packetid = MAX_PACKET_ID;
    c->onConnectCallback = onConnect;

    c->sendPacketMutex = get_new_mutex();
    c->messageHandlersMutex = get_new_mutex();
    c->resultHandlersMutex = get_new_mutex();

}


int decodePacket(Client* c, int* value)
{
    unsigned char i;
    int multiplier = 1;
    int len = 0;
    const int MAX_NO_OF_REMAINING_LENGTH_BYTES = 4;

    *value = 0;
    do
    {
        int rc = MQTTPACKET_READ_ERROR;

        if (++len > MAX_NO_OF_REMAINING_LENGTH_BYTES)
        {
            rc = MQTTPACKET_READ_ERROR; /* bad data */
            goto exit;
        }
        rc = c->ipstack->mqttread(c->ipstack, &i, 1);
        if (rc != 1)
            goto exit;
        *value += (i & 127) * multiplier;
        multiplier *= 128;
    } while ((i & 128) != 0);
exit:
    return len;
}


int readPacket(Client* c, MQTTFixedHeader *fixedHeader)
{
    int rc = FAILURE;
    MQTTHeader header = {0};
    int len = 0;
    int rem_len = 0;

    /* 1. read the header byte.  This has the packet type in it */
    if (c->ipstack->mqttread(c->ipstack, c->readbuf, 1) != 1)
        goto exit;

    len = 1;
    /* 2. read the remaining length.  This is variable in itself */
    decodePacket(c, &rem_len);
    len += MQTTPacket_encode(c->readbuf + 1, rem_len); /* put the original remaining length back into the buffer */

    /* 3. read the rest of the buffer using a callback to supply the rest of the data */
    if (rem_len > 0 && (c->ipstack->mqttread(c->ipstack, c->readbuf + len, rem_len) != rem_len))
        goto exit;

    header.byte = c->readbuf[0];
    fillFixedHeaderFieldsFromPacketHeader(fixedHeader, &header);

    rc = SUCCESS;

exit:
    return rc;
}


// assume topic filter and name is in correct format
// # can only be at end
// + and # can only be next to separator
char isTopicMatched(char* topicFilter, MQTTString* topicName)
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


int deliverMessage(Client* c, MQTTString* topicName, MQTTMessage* message)
{
    int i;
    int rc = FAILURE;

    // we have to find the right message handler - indexed by topic
    c->messageHandlersMutex->lock(c->messageHandlersMutex);
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
    c->messageHandlersMutex->unlock(c->messageHandlersMutex);

    if (rc == FAILURE && c->defaultMessageHandler != NULL)
    {
        MessageData md;
        NewMessageData(&md, topicName, message);
        c->defaultMessageHandler(&md);
        rc = SUCCESS;
    }

    return rc;
}

int fireResultHandlerUsingMsgIdAsTheKey(Client *c)
{
    int msgId = -1;

    MQTTFixedHeaderPlusMsgId fixedHeaderPlusMsgId;
    if (MQTTDeserialize_FixedHeaderAndMsgId(&fixedHeaderPlusMsgId, c->readbuf, c->readbuf_size) == SUCCESS)
    {
        msgId = fixedHeaderPlusMsgId.msgId;
        fireResultHandlerAndRemove(c, &fixedHeaderPlusMsgId);
    }

    return msgId;
}


void readPacketThread(Client* c)
{
    while(1)
    {
        int len = 0;

        MQTTFixedHeader fixedHeader;
        int rc = readPacket(c, &fixedHeader);

        switch (fixedHeader.packetType)
        {
            case CONNACK:
            {
                unsigned char connack_rc = 255;
                char sessionPresent = 0;
                if (MQTTDeserialize_connack((unsigned char*)&sessionPresent, &connack_rc, c->readbuf, c->readbuf_size) == 1)
                {
                    if(connack_rc == 0x00)  // Connection Accepted
                    {
                        c->isconnected = 1;
                        c->onConnectCallback();
                    }
                    else
                    {
                        printf("Client-Connection failed with code [%d]\n", connack_rc);
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

                if (MQTTDeserialize_suback(&msgId, 1, &count, &grantedQoS, c->readbuf, c->readbuf_size) == 1)
                    rc = grantedQoS; // 0, 1, 2 or 0x80

                if (rc == 0x80)
                {
                    int i;

                    c->messageHandlersMutex->lock(c->messageHandlersMutex);
                    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
                    {
                        if (c->messageHandlers[i].msgId == msgId)
                        {
                            c->messageHandlers[i].topicFilter = 0;
                            break;
                        }
                    }
                    c->messageHandlersMutex->unlock(c->messageHandlersMutex);
                }

                break;
            }

            case PUBLISH:
            {
                MQTTString topicName;
                MQTTMessage msg;
                if (MQTTDeserialize_publish(&(msg.fixedHeaderPlusMsgId),
                                            &topicName,
                                            (unsigned char**)&msg.payload,
                                            (int*)&msg.payloadlen,
                                            c->readbuf,
                                            c->readbuf_size) != SUCCESS)
                {
                    goto exit;
                }

                deliverMessage(c, &topicName, &msg);

                enum QoS qos = msg.fixedHeaderPlusMsgId.fixedHeader.qos;
                if (qos != QOS0)
                {
                    char buf[SEND_BUFFER_SIZE];

                    if (qos == QOS1)
                        len = MQTTSerialize_ack(buf, SEND_BUFFER_SIZE, PUBACK, 0, msg.fixedHeaderPlusMsgId.msgId);
                    else if (qos == QOS2)
                        len = MQTTSerialize_ack(buf, SEND_BUFFER_SIZE, PUBREC, 0, msg.fixedHeaderPlusMsgId.msgId);
                    if (len <= 0)
                        rc = FAILURE;
                    else
                        rc = sendPacket(c, buf, len);

                    if (rc == FAILURE)
                        goto exit; // there was a problem
                }

                break;
            }

            case PUBREC:
            {
                int msgId = fireResultHandlerUsingMsgIdAsTheKey(c);

                char buf[SEND_BUFFER_SIZE];
                if ((len = MQTTSerialize_ack(buf, SEND_BUFFER_SIZE, PUBREL, 0, msgId)) <= 0)
                {
                    rc = FAILURE;
                }
                else if ((rc = sendPacket(c, buf, len)) != SUCCESS) // send the PUBREL packet
                {
                    rc = FAILURE; // there was a problem
                }

                if (rc == FAILURE)
                    goto exit; // there was a problem

                break;
            }

            case PUBCOMP:

                break;

            case PINGRESP:
            {
                printf("PINGRESP received... relations are intact !!\n");

                break;
            }
        }

exit:

    continue;
    }
}


int MQTTConnect(Client* c, MQTTPacket_connectData* options)
{
    int rc = FAILURE;
    char buf[SEND_BUFFER_SIZE];

    MQTTPacket_connectData default_options = MQTTPacket_connectData_initializer;
    int len = 0;

    if (c->isconnected) // don't send connect packet again if we are already connected
        goto exit;

    if (options == 0)
        options = &default_options; // set default options if none were supplied

    c->keepAliveInterval = options->keepAliveInterval;

    if ((len = MQTTSerialize_connect(buf, SEND_BUFFER_SIZE, options)) <= 0)
        goto exit;
    if ((rc = sendPacket(c, buf, len)) != SUCCESS)  // send the connect packet
        goto exit; // there was a problem


exit:
    return rc;
}


int MQTTSubscribe(Client* c,
                  const char* topicName,
                  const enum QoS qos,
                  messageHandler messageHandler,
                  void (*resultHandler)(MQTTFixedHeaderPlusMsgId *),
                  unsigned int resultHandlerTimeout)
{
    int rc = FAILURE;
    int len = 0;
    int id;
    char buf[SEND_BUFFER_SIZE];

    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicName;

    if (!c->isconnected)
        goto exit;

    id = getNextPacketId(c);
    len = MQTTSerialize_subscribe(buf, SEND_BUFFER_SIZE, 0, id, 1, &topic, (int*)&qos);
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

        c->messageHandlersMutex->lock(c->messageHandlersMutex);
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
        c->messageHandlersMutex->unlock(c->messageHandlersMutex);
    }

    if ((rc = sendPacket(c, buf, len)) != SUCCESS) // send the subscribe packet
        goto exit;             // there was a problem

exit:
    return rc;
}


int MQTTUnsubscribe(Client* c, const char* topicFilter)
{
    int rc = FAILURE;
    char buf[SEND_BUFFER_SIZE];

    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicFilter;
    int len = 0;

    if (!c->isconnected)
        goto exit;

    if ((len = MQTTSerialize_unsubscribe(buf, SEND_BUFFER_SIZE, 0, getNextPacketId(c), 1, &topic)) <= 0)
        goto exit;
    if ((rc = sendPacket(c, buf, len)) != SUCCESS) // send the subscribe packet
        goto exit; // there was a problem

exit:
    return rc;
}


int MQTTPublish(Client* c,
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
    char buf[SEND_BUFFER_SIZE];

    MQTTString topic = MQTTString_initializer;
    topic.cstring = (char *)topicName;
    int len = 0;
    int id;

    if (!c->isconnected)
        goto exit;

    if (qos == QOS1 || qos == QOS2)
    {
        id = getNextPacketId(c);

        /*
         * We will get PUBACK from server only for QOS1 and QOS2.
         * So, it makes sense to lodge the result-handler only for these cases.
         */
        attachResultHandler(c, id, resultHandlerTimeout, resultHandler);
    }

    len = MQTTSerialize_publish(buf, SEND_BUFFER_SIZE, 0, qos, retain, id, topic, (unsigned char*)payload, strlen(payload) + 1);
    if (len <= 0)
        goto exit;
    if ((rc = sendPacket(c, buf, len)) != SUCCESS) // send the subscribe packet
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


int MQTTDisconnect(Client* c)
{
    int rc = FAILURE;
    char buf[SEND_BUFFER_SIZE];

    int len = MQTTSerialize_disconnect(buf, SEND_BUFFER_SIZE);
    if (len > 0)
        rc = sendPacket(c, buf, len);            // send the disconnect packet

    c->isconnected = 0;

    return rc;
}

