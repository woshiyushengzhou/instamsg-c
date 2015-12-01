#ifndef IOEYE_MODBUS
#define IOEYE_MODBUS

#include "../../../common/instamsg/driver/include/globals.h"
#include "../../../common/ioeye/include/globals.h"

typedef struct Modbus Modbus;

struct Modbus
{
    /* ============================= THIS SECTION MUST NOT BE TEMPERED ==================================== */
    MODBUS_DEVICE_TYPE deviceType;
    const char *identifier;
    int (*send_command_and_read_response_sync)(Modbus *modbus,
                                               unsigned char *commandBytes,
                                               int commandBytesLength,
                                               unsigned char *responseByteBuffer,
                                               int responseBytesLength);

    char modbusCommands[MAX_BUFFER_SIZE];
    /* ============================= THIS SECTION MUST NOT BE TEMPERED ==================================== */



    /* ============================= ANY EXTRA FIELDS GO HERE ============================================= */
    /* ============================= ANY EXTRA FIELDS GO HERE ============================================= */
};

#endif