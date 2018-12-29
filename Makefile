# Raspbian
# LINUX_PATH := /usr/src/linux

# Ubuntu
LINUX_PATH := /lib/modules/`uname -r`/build

obj-m := kadai1.o

kadai1.ko: kadai1.c
	make -C $(LINUX_PATH) M=`pwd` V=1 modules

clean:
	make -C $(LINUX_PATH) M=`pwd` V=1 clean
