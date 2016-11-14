#!/bin/sh

move_file()
{
    mv /overlay/home/sensegrow/at_temp /overlay/home/sensegrow/$1
}

counter=0

while true
do
    PID=`ps | grep instamsg | grep -v grep | grep -v tail | sed -e 's/^ *//g' | cut -d\  -f 1`

    if [ -z "${PID}" ]
    then
	    echo "Binary not running"
        cd /overlay/home/sensegrow
        chmod 777 instamsg

        sleep 3
	    ./instamsg &
    else
	    echo "Binary running fine"
    fi

    /usr/bin/sendat /dev/ttyACM4 "at+csq" 2 | grep -v csq | grep -v OK | grep -v "^$" | head -1 | cut -d: -f 2 | cut -d, -f 1 > /overlay/home/sensegrow/at_temp
    move_file "signal"

    /usr/bin/sendat /dev/ttyACM4 "at+cgsn" 2 | grep -v cgsn | grep -v OK | grep -v "^$" > /overlay/home/sensegrow/at_temp
    move_file "imei"

    /usr/bin/sendat /dev/ttyACM4 "at+cimi" 2 | grep -v cimi | grep -v OK | grep -v "^$" > /overlay/home/sensegrow/at_temp
    move_file "imsi"

    sleep 60

    counter=$(($counter+60))
    if [ "$counter" -gt "3600" ]
    then
        /sbin/reboot
        sleep 2
        /sbin/reboot
        sleep 2
        /sbin/reboot
    fi
done
