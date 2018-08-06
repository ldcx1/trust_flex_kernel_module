#!/bin/sh

# Script called by udev to initialise trust_flex module during usb hotplug event.

logger udev rule triggered to load trust_flex
#Make sure mono module is not loaded
rmmod trust_flex
logger removed trust_flex driver module from kernel

#Detach the usbhid module
/usr/local/bin/detach_usbhid
logger detached usbhid driver module from mono device

#Now insert Bosto module
insmod /lib/modules/$(uname -r)/kernel/drivers/input/tablet/trust_flex.ko
