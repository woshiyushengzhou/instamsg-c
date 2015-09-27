#ifndef IOEYE_METADATA
#define IOEYE_METADATA

#include <string.h>

#include "../../../../instamsg/driver/include/globals.h"
#include "../../../../instamsg/driver/include/log.h"
#include "../../../../instamsg/driver/include/watchdog.h"

#include "../common/telit.h"

static void addKeyValue(char *buffer, const char *key, const char *splitter, const char *defaultValue, const char *command)
{
    strcat(buffer, ", '");
    strcat(buffer, key);
    strcat(buffer, "' : ");

    RESET_GLOBAL_BUFFER;
    run_simple_at_command_and_get_output(command, (char*)GLOBAL_BUFFER);

    if(strstr((char*)GLOBAL_BUFFER, splitter) != NULL)
    {
        strcat(buffer, strstr((char*)GLOBAL_BUFFER, splitter) + 1);
    }
    else
    {
        strcat(buffer, defaultValue);
    }
}


/*
 * This method returns the client-session-data, in simple JSON form, of type ::
 *
 * {key1 : value1, key2 : value2 .....}
 */
void get_client_session_data(char *messageBuffer, int maxBufferLength)
{
    watchdog_reset_and_enable(60, "get_client_session_data");


    /*
     * Start the JSON-Dict.
     */
    strcat(messageBuffer, "{");

    strcat(messageBuffer, "'method' : 'GPRS'");
    addKeyValue(messageBuffer, "ip_address", ",", "\"\"", "AT+CGPADDR=\r\n");
    addKeyValue(messageBuffer, "antina_status", ":", " -1", "AT#GSMAD=3\r\n");

    strcat(messageBuffer, ", 'signal_strength' : ");
    RESET_GLOBAL_BUFFER;
    run_simple_at_command_and_get_output("AT+CSQ\r\n", (char*)GLOBAL_BUFFER);
    strcat(messageBuffer, "\"[");
    {
        char *starter = (char*)GLOBAL_BUFFER + strlen("+CSQ: ");
        while(1)
        {
            char *finder = strstr(starter, ",");
            if(finder != NULL)
            {
                strcat(messageBuffer, "'");
                strncat(messageBuffer, starter, finder - starter);
                strcat(messageBuffer, "', ");
                starter = finder + 1;
            }
            else
            {
                strcat(messageBuffer, "'");
                strcat(messageBuffer, starter);
                strcat(messageBuffer, "'");
                break;
            }
        }
    }
    strcat(messageBuffer, "]\"");

    /*
     * Terminate the JSON-Dict.
     */
    strcat(messageBuffer, "}");

    info_log("Client-Session-Data = [%s]", messageBuffer);
    watchdog_disable();
}


/*
 * This method returns the client-metadata, in simple JSON form, of type ::
 *
 * {key1 : value1, key2 : value2 .....}
 */
void get_client_metadata(char *messageBuffer, int maxBufferLength)
{
}


#endif
