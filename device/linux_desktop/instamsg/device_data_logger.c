#include <stdio.h>
#include <string.h>

#include "../../../common/instamsg/driver/include/data_logger.h"
#include "../../../common/instamsg/driver/include/globals.h"
#include "../../../common/instamsg/driver/include/log.h"

#define DATA_FILE_NAME      "/home/sensegrow/data.txt"
#define TEMP_FILE_NAME      "/home/sensegrow/temp"

static char tempBuffer[1024];

/*
 * This method initializes the data-logger-interface for the device.
 */
void init_data_logger()
{
}


/*
 * This method saves the record on the device.
 *
 * If and when the device-storage becomes full, the device MUST delete the oldest record, and instead replace
 * it with the current record. That way, we will maintain a rolling-data-logger.
 */
void save_record_to_persistent_storage(char *record)
{
    sg_appendLine(DATA_FILE_NAME, record);
}


/*
 * The method returns the next available record.
 * If a record is available, following must be done ::
 *
 * 1)
 * The record must be deleted from the storage-medium (so as not to duplicate-fetch this record later).
 *
 * 2)
 * Then actually return the record.
 *
 * Obviously, there is a rare chance that step 1) is finished, but step 2) could not run to completion.
 * That would result in a data-loss, but we are ok with it, because we don't want to send duplicate-records to InstaMsg-Server.
 *
 * We could have done step 2) first and then step 1), but in that scenario, we could have landed in a scenario where step 2)
 * was done but step 1) could not be completed. That could have caused duplicate-data on InstaMsg-Server, but we don't want
 * that.
 *
 *
 * One of the following statuses must be returned ::
 *
 * a)
 * SUCCESS, if a record is successfully returned.
 *
 * b)
 * FAILURE, if no record is available.
 */
int get_next_record_from_persistent_storage(char *buffer, int maxLength)
{
    int rc = FAILURE;
    unsigned char lineRead = 0;

    FILE *fp = fopen(DATA_FILE_NAME, "r");
    if(fp == NULL)
    {
        sg_sprintf(LOG_GLOBAL_BUFFER, "%sCould not open data-log-file [%s] for reading", DATA_LOGGING_ERROR, DATA_FILE_NAME);
        error_log(LOG_GLOBAL_BUFFER);

        goto exit;
    }

    sg_createEmptyFile(TEMP_FILE_NAME);

    while(1)
    {

        memset(tempBuffer, 0, sizeof(tempBuffer));
        sg_readLine(fp, tempBuffer, sizeof(tempBuffer));

        if(strlen(tempBuffer) == 0)
        {
            /*
             * Nothing else was left to read.
             */
            goto exit;
        }
        else
        {
            if(lineRead == 0)
            {
                /*
                 * This is the data we want.
                 */
                memcpy(buffer, tempBuffer, strlen(tempBuffer));
                lineRead = 1;

                rc = SUCCESS;
            }
            else
            {
                /*
                 * Copy to temp-file.
                 */
                sg_appendLine(TEMP_FILE_NAME, tempBuffer);
            }
        }
    }

exit:
    if(fp != NULL)
    {
        fclose(fp);
    }

    if(1)
    {
        if(renameFile(NULL, TEMP_FILE_NAME, DATA_FILE_NAME) != 0)
        {
            sg_sprintf(LOG_GLOBAL_BUFFER, "%sCould not move file from [%s] to [%s]", DATA_LOGGING_ERROR, TEMP_FILE_NAME, DATA_FILE_NAME);
            error_log(LOG_GLOBAL_BUFFER);
        }
    }

    return rc;
}