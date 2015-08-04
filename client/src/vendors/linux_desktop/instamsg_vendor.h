#ifndef INSTAMSG_VENDOR
#define INSTAMSG_VENDOR

#include "../../common/include/instamsg_vendor_common.h"

struct Network
{
    int socket;
    COMMUNICATION_INTERFACE(Network)
};


#include <stdio.h>
struct FileSystem
{
    FILE *fp;
    COMMUNICATION_INTERFACE(FileSystem)
};


struct Serial
{
    COMMUNICATION_INTERFACE(Serial)
};


struct Command
{
    COMMUNICATION_INTERFACE(Command)
};


#include <pthread.h>
struct Mutex
{
    pthread_mutex_t mtx;

    void (*lock)(Mutex *mutex);
    void (*unlock)(Mutex *mutex);
};


#include <sys/time.h>
struct Timer
{
    struct timeval end_time;
    TIMER_INTERFACE
};


struct System
{
    SYSTEM_INTERFACE
};

#endif
