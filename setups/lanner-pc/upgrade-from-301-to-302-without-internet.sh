set -x
set -e

sed -i 's/GRUB_CMDLINE_LINUX_DEFAULT=.*/GRUB_CMDLINE_LINUX_DEFAULT="quiet splash intel_idle.max_cstate=0 processor.max_cstate=0 idle=poll"/g' /etc/default/grub
update-grub

apt-get -y remove pm-utils

kill -9 `pgrep -x instamsg` || true
cp energy-meter_lanner-pc_3.0.3_3.0.3 /home/sensegrow/instamsg

echo
echo
echo "PLEASE REBOOT THE MACHINE NOW ..."
echo

