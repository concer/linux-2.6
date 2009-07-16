#!/bin/sh
# deploy current kernel to White Rabbit

WR_TFTP=/home/wrdev/WR_buildroot/WR_Switch/tftpboot

# if test -f $WR_TFTP/uImage
# then
    # cp $WR_TFTP/uImage $WR_TFTP/uImage.backup
# fi

cp uImage $WR_TFTP
