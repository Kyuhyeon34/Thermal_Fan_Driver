obj-m := ec11.o

KERNELDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
CC := aarch64-linux-gnu-gcc

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
	$(CC) -Wall -o ec11_test ec11_test.c
	$(CC) -Wall -o fan_ctrl fan_ctrl.c fan_state.c

clean:
	rm -f *.o *.ko *.mod *.mod.c modules.order Module.symvers
	rm -f .*.*.cmd .*.*.*.cmd
	rm -rf .tmp_versions
	rm -f ec11_test fan_ctrl
