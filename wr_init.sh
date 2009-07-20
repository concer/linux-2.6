#!/bin/sh

WR_IP=192.168.100.2

bootfpgas.sh
sleep 1
cd /
./nic_init 1>/dev/null &
sleep 1
insmod wr-mch.ko debug=16
sleep 1
ifconfig eth1 up $WR_IP

echo "FPGA's booted, nic_init running and wr-mch driver installed OK."
echo "wr-mch running on eth1, ip=$WR_IP"

exit 0
