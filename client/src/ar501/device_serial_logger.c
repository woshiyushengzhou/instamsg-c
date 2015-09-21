/*******************************************************************************
 * Contributors:
 *
 *      Ajay Garg <ajay.garg@sensegrow.com>
 *
 *******************************************************************************/


#include "./device_serial_logger.h"
#include "./uart_utils.h"

#include "../common/include/globals.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "inc/hw_memmap.h"
#include "inc/hw_gpio.h"
#include "inc/hw_types.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/uart.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"


/*
 * This method MUST connect the underlying medium (even if it means to retry continuously).
 */
void connect_underlying_serial_logger_medium_guaranteed(SerialLoggerInterface *serialLoggerInterface)
{
    /*
     * UART-initialiazation.
     */
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

    /*
     * Enable TX.
     */
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    ROM_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_1);

    /*
     * Configure UART-clocking.
     */
    ROM_UARTConfigSetExpClk(UART0_BASE, ROM_SysCtlClockGet(), 115200,
                            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                             UART_CONFIG_PAR_NONE));

    UARTEnable(UART0_BASE);
}


/*
 * This method writes first "len" bytes from "buffer" onto the serial-logger-interface.
 *
 * This is a blocking function. So, either of the following must hold true ::
 *
 * a)
 * All "len" bytes are written.
 * In this case, SUCCESS must be returned.
 *
 *                      OR
 * b)
 * An error occurred while writing.
 * In this case, FAILURE must be returned immediately.
 */
int serial_logger_write(SerialLoggerInterface* serialLoggerInterface, unsigned char* buffer, int len)
{
    UARTSend(UART0_BASE, buffer, len);
    return SUCCESS;
}


/*
 * This method MUST release the underlying medium (even if it means to retry continuously).
 * But if it is ok to re-connect without releasing the underlying-system-resource, then this can be left empty.
 */
void release_underlying_serial_logger_medium_guaranteed(SerialLoggerInterface *serialLoggerInterface)
{
}