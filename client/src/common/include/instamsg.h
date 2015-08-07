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

#ifndef INSTAMSG
#define INSTAMSG

#include "./globals.h"
#include "./log.h"
#include "../../../../MQTTPacket/src/MQTTPacket.h"

#include "stdio.h"

#define MAX_PACKET_ID 30000
#define MAX_MESSAGE_HANDLERS 5
#define KEEP_ALIVE_INTERVAL_SECS 10
#define MQTT_RESULT_HANDLER_TIMEOUT 10



unsigned char terminateCurrentInstance;
unsigned int threadCount;
Mutex threadCountMutex;

void NewTimer(Timer*);

typedef struct MQTTMessage MQTTMessage;
typedef struct MessageData MessageData;
typedef struct InstaMsg InstaMsg;


struct MQTTMessage
{
    MQTTFixedHeaderPlusMsgId fixedHeaderPlusMsgId;
    void *payload;
    size_t payloadlen;
};


struct MessageData
{
    InstaMsg *c;
    MQTTMessage* message;
    MQTTString* topicName;
};


typedef void (*messageHandler)(MessageData*);

struct opts_struct
{
	char* clientid;
	int nodelimiter;
	char* delimiter;
	enum QoS qos;
	char* password;
	char* host;
	int port;
	int showtopics;

	int subscribe;
	int publish;
	char msg[100];
    char *logFilePath;
};

struct InstaMsg {
    unsigned int next_packetid;
    unsigned char readbuf[MAX_BUFFER_SIZE];

    struct MessageHandlers
    {
        int msgId;
        const char* topicFilter;
        void (*fp) (MessageData*);
    } messageHandlers[MAX_MESSAGE_HANDLERS];      // Message handlers are indexed by subscription topic

    struct ResultHandlers
    {
        int msgId;
        unsigned int timeout;
        void (*fp) (MQTTFixedHeaderPlusMsgId*);
    } resultHandlers[MAX_MESSAGE_HANDLERS];      // Message handlers are indexed by subscription topic

    void (*defaultMessageHandler) (MessageData*);
    int (*onConnectCallback)();
    int (*onDisconnectCallback)();
    int (*oneToOneMessageCallback)();

    struct Mutex networkPhysicalMediumMutex;
    struct Mutex messageHandlersMutex;
    struct Mutex resultHandlersMutex;

    unsigned char filesTopic[MAX_BUFFER_SIZE];
    unsigned char rebootTopic[MAX_BUFFER_SIZE];
    unsigned char enableServerLoggingTopic[MAX_BUFFER_SIZE];
    unsigned char serverLogsTopic[MAX_BUFFER_SIZE];

    unsigned char serverLoggingEnabled;

    Network ipstack;
    Network httpClient;

    System systemUtils;

    MQTTPacket_connectData connectOptions;
};

InstaMsg instaMsg;

void* MQTTConnect(void* arg);

int MQTTPublish(InstaMsg *c,
                const char *topicName,
                const char *payload,
                const enum QoS qos,
                const char dup,
                void (*resultHandler)(MQTTFixedHeaderPlusMsgId *),
                unsigned int resultHandlerTimeout,
                const char retain,
                const char logging);


int MQTTSubscribe(InstaMsg *c,
                  const char *topicName,
                  const enum QoS qos,
                  messageHandler messageHandler,
                  void (*resultHandler)(MQTTFixedHeaderPlusMsgId *),
                  unsigned int resultHandlerTimeout);

int MQTTUnsubscribe (InstaMsg*, const char*);
int MQTTDisconnect (InstaMsg*);
void setDefaultMessageHandler(InstaMsg*, messageHandler);

void initInstaMsg(InstaMsg* c,
                  char *clientId,
                  char *authKey,
                  int (*connectHandler)(),
                  int (*disconnectHandler)(),
                  int (*oneToOneMessageHandler)(),
                  struct opts_struct *opts);

void cleanInstaMsgObject(InstaMsg *c);

void* clientTimerThread(InstaMsg *c);
void* keepAliveThread(InstaMsg *c);
void readPacketThread(InstaMsg *c);

void prepareThreadTerminationIfApplicable(const char *threadName);
void incrementOrDecrementThreadCount(char increment);

typedef struct JSONParseStuff JSONParseStuff;
struct JSONParseStuff
{
    const char *key;
    void *value;
    unsigned int mandatory;
    unsigned char type;
};

char serialLoggerEnabled;

void messageArrived(MessageData* md);
void subscribeAckReceived(MQTTFixedHeaderPlusMsgId *fixedHeaderPlusMsgId);

#define SERVER_LOGGING "[SERVER-LOGGING] "
#define FILE_TRANSFER "[FILE-TRANSFER] "

#define DefaultClient {0, 0, 0, 0, NULL, NULL, 0, 0, 0}
#endif
