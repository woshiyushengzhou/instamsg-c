/*******************************************************************************
 * Contributors:
 *
 *      Ajay Garg <ajay.garg@sensegrow.com>
 *
 *******************************************************************************/

#include "./serial_logger.h"
#include "../../common/include/globals.h"

static int serial_logger_write(SerialLoggerInterface* serialLoggerInterface, unsigned char* buffer, int len)
{
    return SUCCESS;
}


void init_serial_logger_interface(SerialLoggerInterface *serialLoggerInterface, void *arg)
{
    /*
     * The init-logic goes here.
     */

    /* Register write-callback. */
	serialLoggerInterface->write = serial_logger_write;
}


void release_serial_logger_interface(SerialLoggerInterface *serialLoggerInterface)
{
}