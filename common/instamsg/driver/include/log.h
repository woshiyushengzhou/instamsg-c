#ifndef INSTAMSG_LOGGER
#define INSTAMSG_LOGGER

#include "./serial_logger.h"
#include "./globals.h"

#include "device_file_system.h"

#if FILE_SYSTEM_ENABLED == 1
typedef struct FileLogger FileLogger;
struct FileLogger
{
    FileSystem fs;
};

void init_file_logger(FileLogger *fileLogger, void *arg);
void release_file_logger(FileLogger *fileLogger);

FileLogger fileLogger;
#endif


#define INSTAMSG_LOG_LEVEL_DISABLED 0
#define INSTAMSG_LOG_LEVEL_INFO     1
#define INSTAMSG_LOG_LEVEL_ERROR    2
#define INSTAMSG_LOG_LEVEL_DEBUG    3

typedef int (*LOG_WRITE_FUNC)(void *logger_medium, unsigned char *buffer, int len);
extern int currentLogLevel;



/*
 *********************************************************************************************************************
 **************************************** PUBLIC APIs *****************************************************************
 **********************************************************************************************************************
 */

/*
 * Logging at INFO level.
 */
void info_log(char *log);

/*
 * Logging at ERROR level.
 */
void error_log(char *log);

/*
 * Logging at DEBUG level.
 */
void debug_log(char *log);



#endif
