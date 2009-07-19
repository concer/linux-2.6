#!/bin/sh

insmod wr-mch.ko debug=16
ifconfig eth1 up 192.168.100.3
