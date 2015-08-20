/*******************************************************************************
 * Contributors:
 *
 *    Ajay Garg <ajay.garg@sensegrow.com>
 *******************************************************************************/


#include <stdio.h>
#include <string.h>

#include "instamsg_vendor.h"

static void getManufacturer(System *system, char *buf, int maxValueLenAllowed)
{
    sg_sprintf(buf, "HP");
}


static void getSerialNumber(System *system, char *buf, int maxValueLenAllowed)
{
    sg_sprintf(buf, "123456789");
}


static void rebootDevice(System *sys)
{
    info_log("Rebooting the system.");
    system("/sbin/reboot");
}


void init_system_utils(System *system, void *arg)
{
    system->getManufacturer = getManufacturer;
    system->getSerialNumber = getSerialNumber;
    system->rebootDevice = rebootDevice;
}


void release_system_utils(System *system)
{
}


void SYSTEM_GLOBAL_INIT()
{
}
