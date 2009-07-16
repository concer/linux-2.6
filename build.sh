#!/bin/sh
# build current tree
# take the config from myconfig

#make clean
#make mrproper
cp myconfig .config
make CONFIG_DEBUG_SECTION_MISMATCH=y ARCH=arm CROSS_COMPILE=/home/wrdev/WR_buildroot/buildroot/build_arm/staging_dir/usr/bin/arm-linux-uclibcgnueabi- $1
mkimage -A arm -O linux -C none -T kernel -a 20008000 -e 20008000 -n linux-2.6 -d arch/arm/boot/zImage uImage
