LOGIN="root@192.168.1.1"
SSH_COMMAND="ssh ${LOGIN}"

HOME_DIRECTORY="/overlay/home/sensegrow"

${SSH_COMMAND} "sed -i 's/^[ :\t]*\/etc\/init.d\/event_sms reload/#\/etc\/init.d\/event_sms reload/g' /usr/sbin/eventtrack.sh"
${SSH_COMMAND} "sed -i 's/^[ :\t]*\/etc\/init.d\/event_sms stop/#\/etc\/init.d\/event_sms stop/g' /usr/sbin/eventtrack.sh"
${SSH_COMMAND} "sed -i 's/^[ :\t]*\/etc\/init.d\/event_sms start/#\/etc\/init.d\/event_sms start/g' /usr/sbin/eventtrack.sh"

${SSH_COMMAND} "sed -i 's/^[ :\t]*\/usr\/sbin\/event_sms.sh/#\/usr\/sbin\/event_sms.sh/g' /etc/init.d/event_sms"

${SSH_COMMAND} "killall instamsg"

${SSH_COMMAND} "mkdir -p ${HOME_DIRECTORY}"
${SSH_COMMAND} "chmod -R 777 ${HOME_DIRECTORY}"
scp $1  ${LOGIN}:${HOME_DIRECTORY}/instamsg

scp monitor.sh  ${LOGIN}:${HOME_DIRECTORY}
${SSH_COMMAND} "chmod 777 ${HOME_DIRECTORY}/monitor.sh"

scp cron  ${LOGIN}:${HOME_DIRECTORY}
${SSH_COMMAND} "chmod 777 ${HOME_DIRECTORY}/cron"

${SSH_COMMAND} "cat ${HOME_DIRECTORY}/cron | crontab -"
${SSH_COMMAND} rm ${HOME_DIRECTORY}/cron

${SSH_COMMAND} touch ${HOME_DIRECTORY}/data.txt
${SSH_COMMAND} chmod 777 ${HOME_DIRECTORY}/data.txt

${SSH_COMMAND} "echo test > ${HOME_DIRECTORY}/prov.txt"