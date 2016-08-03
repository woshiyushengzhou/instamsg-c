#ifndef INSTAMSG_GPS_COMMON
#define INSTAMSG_GPS_COMMON

#include "./time.h"

void fill_in_gps_nmea_blob_until_buffer_fills_or_time_expires(unsigned char *buffer, int bufferLength, int maxTime);

void trim_buffer_to_contain_only_first_GPRMC_sentence(unsigned char *buffer, int bufferLength);
int fill_in_time_coordinates_from_GPRMC_sentence(char *buffer, DateParams *dateParams);

#endif
