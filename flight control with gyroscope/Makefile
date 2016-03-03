KDIR:=/home/esp/SDK/sysroots/i586-poky-linux/usr/src/kernel
#PWD:= $(shell pwd)

CC = i586-poky-linux-gcc
ARCH = x86
CROSS_COMPILE = i586-poky-linux-
SROOT=/home/esp/SDK/sysroots/i586-poky-linux/

APP =main4
all:
	i586-poky-linux-gcc -o $(APP) main4.c kalman.c  -lpthread -lm --sysroot=$(SROOT)
clean:
	rm -f $(APP)
 
