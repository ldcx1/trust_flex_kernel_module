CC=gcc
INC=-I/usr/include/libusb-1.0 
LIB=-L/lib/$(arch)-linux-gnu/libusb-1.0.so.0

.PHONY: all clean archive

obj-m += trust_flex.o
 
all:
	$(CC) detach_usbhid.c $(INC) $(LIB) -lusb-1.0 -o detach_usbhid
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
 
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm detach_usbhid

archive:
	tar f - --exclude=.git -C ../ -c trust_flex | gzip -c9 > ../trust_flex-`date +%Y%m%d`.tgz

install:
	cp ./trust_flex.ko /lib/modules/$(shell uname -r)/kernel/drivers/input/tablet/
	echo trust_flex >> /etc/modules
	depmod
	cp ./load_trust_flex.sh /usr/local/bin
	cp ./detach_usbhid /usr/local/bin
	cp ./load_trust_flex.rules /etc/udev/rules.d
	/sbin/udevadm control --reload
	modprobe trust_flex

uninstall:
	rm /usr/local/bin/load_trust_flex.sh
	rm /usr/local/bin/detach_usbhid
	rm /etc/udev/rules.d/load_trust_flex.rules
	/sbin/udevadm control --reload
	rm /lib/modules/$(shell uname -r)/kernel/drivers/input/tablet/trust_flex.ko
	sed -i '/trust_flex/d' /etc/modules
	depmod
	rmmod trust_flex
