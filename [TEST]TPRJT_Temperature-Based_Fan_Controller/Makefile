obj-m := devtest.o

KERNELDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
CC := aarch64-linux-gnu-gcc

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
	$(CC) -Wall -o apptest apptest.c

clean:
	rm -f *.o *.ko *.mod *.mod.c modules.order Module.symvers
	rm -f .*.*.cmd .*.*.*.cmd
	rm -rf .tmp_versions
	rm -f apptest
