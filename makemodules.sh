#!/bin/bash

make CONFIG_DEBUG_SECTION_MISMATCH=y ARCH=arm CROSS_COMPILE=/home/wrdev/WR_buildroot/buildroot/build_arm/staging_dir/usr/bin/arm-linux-uclibcgnueabi- $1 modules

scp /home/wrdev/WR_buildroot/WR_Switch/NIC_init_sw/nic_init drivers/net/arm/wr-mch.ko loaddriver.sh root@192.168.1.2:/
scp /home/wrdev/WR_buildroot/WR_Switch/NIC_init_sw/nic_init drivers/net/arm/wr-mch.ko loaddriver.sh root@192.168.1.3:/
