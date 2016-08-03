/*******************************************************************************
 * Contributors:
 *
 *      Ajay Garg <ajay.garg@sensegrow.com>
 *
 *******************************************************************************/

#include "../driver/include/socket.h"
#include "../driver/include/time.h"

#include <time.h>



/*
 * This method does the global-level-initialization for time (if any).
 */
void init_global_timer()
{
}


/*
 * This method returns the minimum-delay achievable via this device.
 */
unsigned long getMinimumDelayPossibleInMicroSeconds()
{
    return 500;
}


/*
 * This method ACTUALLY causes the current-device to go to sleep for the minimum-delay possible.
 */
void minimumDelay()
{
    usleep(500);
}


/*
 * This method returns the current-tick/timestamp.
 */
unsigned long getCurrentTick()
{
    return time(NULL);
}


int fill_in_time_coordinates_from_gps(DateParams *dateParams)
{
    sg_sprintf(LOG_GLOBAL_BUFFER,
              "%sTime-Syncing at grass-root-level not required for this device, so returning pseudo-success for GPS-Time-Sync", CLOCK);
    info_log(LOG_GLOBAL_BUFFER);

    return SUCCESS;
}


#if GPS_TIME_SYNC_PRESENT == 1
/*
 * Fills in the time-coordinates from GPRMC-sentence, as per http://aprs.gids.nl/nmea/#rmc
 *
 * In particular, following fields need to be filled
 *
 *      dateParams->tm_year  // year    in YY
 *      dateParams->tm_mon;  // month   in MM    (01-12)
 *      dateParams->tm_mday; // day     in DD    (01-31)
 *      dateParams->tm_hour; // hour    in hh    (00-23)
 *      dateParams->tm_min;  // minute  in mm    (00-59)
 *      dateParams->tm_sec;  // second  in ss    (00-59)
 *
 *
 * Returns SUCCESS on successful fetching of all time-coordinates.
 * Else returns FAILURE.
 */
int fill_in_time_coordinates_from_GPRMC_sentence(char *buffer, DateParams *dateParams)
{
    return FAILURE;
}
#endif


#if GSM_TIME_SYNC_PRESENT == 1
/*
 * Returns the current-timestamp, the original of which was returned via GSM. *
 * Returns 0 in case no informaton is received from GSM (yet).
 */
unsigned long get_GSM_timestamp()
{
    return 0;
}
#endif


/*
 * Syncs the system-clock.
 *
 * Returns SUCCESS on successful-syncing.
 * Else returns FAILURE.
 */
int sync_system_clock(DateParams *dateParams)
{
    return SUCCESS;
}
