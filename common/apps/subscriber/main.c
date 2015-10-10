#include "../../instamsg/driver/include/instamsg.h"

static char TOPIC[100];

static void subscribeAckReceived(MQTTFixedHeaderPlusMsgId *fixedHeaderPlusMsgId)
{
    info_log("SUBACK received for msg-id [%u]", fixedHeaderPlusMsgId->msgId);
}


static void messageArrived(MessageData* md)
{
	MQTTMessage* message = md->message;
    info_log("%s", (char*)message->payload);
}


static int onConnectOneTimeOperations()
{
    return MQTTSubscribe(&instaMsg,
                         TOPIC,
                         QOS2,
                         messageArrived,
                         subscribeAckReceived,
                         MQTT_RESULT_HANDLER_TIMEOUT,
                         1);
}


int main(int argc, char** argv)
{
    if(argc < 2)
    {
        printf("\n\nUsage :: ./build/subscriber/<device>/instamsg <topic>\n\n");
        return 1;
    }

    memset(TOPIC, 0, sizeof(TOPIC));
    strcpy(TOPIC, argv[1]);

    {
        char *logFilePath = LOG_FILE_PATH;
#ifdef FILE_SYSTEM_INTERFACE_ENABLED

#ifdef DEBUG_MODE

        if(argc >= 3)
        {
            memset(USER_LOG_FILE_PATH, 0, sizeof(USER_LOG_FILE_PATH));
            strcpy(USER_LOG_FILE_PATH, argv[2]);

            logFilePath = USER_LOG_FILE_PATH;
        }
        if(argc >= 4)
        {
            memset(USER_DEVICE_UUID, 0, sizeof(USER_DEVICE_UUID));
            strcpy(USER_DEVICE_UUID, argv[3]);
        }
#endif  /* DEBUG_MODE */

        globalSystemInit(logFilePath);

#else   /* FILE_SYSTEM_INTERFACE_ENABLED */

        globalSystemInit(NULL);

#endif  /* FILE_SYSTEM_INTERFACE_ENABLED */
    }

    start(onConnectOneTimeOperations, NULL, NULL, NULL, 1);
}
