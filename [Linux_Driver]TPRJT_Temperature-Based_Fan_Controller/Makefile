obj-m := devtest.o

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
APP_CC ?= aarch64-linux-gnu-gcc
APP_CFLAGS ?= -Wall

.PHONY: all modules clean

all: modules apptest

modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURDIR) modules

apptest: apptest.c my_ioctl.h
	$(APP_CC) $(APP_CFLAGS) -o $@ apptest.c

clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURDIR) clean
	rm -f apptest
