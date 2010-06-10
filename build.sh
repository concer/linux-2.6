#!/bin/sh
. ../settings

cp myconfig .config
make CONFIG_DEBUG_SECTION_MISMATCH=y ARCH=arm CROSS_COMPILE=$CC_CPU $1
$MK_KERNEL_IMAGE -A arm -O linux -C none -T kernel -a 20008000 -e 20008000 -n linux-2.6 -d arch/arm/boot/zImage $KERNEL_IMAGE
