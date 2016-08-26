#ifndef IOEYE_SERIAL
#define IOEYE_SERIAL

#include "../../../common/instamsg/driver/include/globals.h"
#include "../../../common/ioeye/include/globals.h"

typedef struct Serial Serial;
typedef struct SimulatedModbus SimulatedModbus;

struct Serial
{
    /* ============================= THIS SECTION MUST NOT BE TEMPERED ==================================== */
    SERIAL_DEVICE_TYPE deviceType;
    char identifier[50];
    unsigned char assignedSerialNumber;
    int (*send_command_and_read_response_sync)(Serial *serial,
                                               unsigned char *commandBytes,
                                               int commandBytesLength,
                                               unsigned char *responseByteBuffer,
                                               int *responseBytesLength);

    char serialCommands[SERIAL_COMMANDS_BUFFER_SIZE];
    unsigned char commandsLoaded;
    /* ============================= THIS SECTION MUST NOT BE TEMPERED ==================================== */



    /* ============================= ANY EXTRA FIELDS GO HERE ============================================= */
    int fd;
    /* ============================= ANY EXTRA FIELDS GO HERE ============================================= */
};
#endif
