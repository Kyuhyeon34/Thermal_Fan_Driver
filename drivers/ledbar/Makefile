obj-m := ledbar.o
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all: modules
modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
dtbo: ledbar-overlay.dts
	dtc -@ -I dts -O dtb -o ledbar.dtbo $<
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	rm -f ledbar.dtbo
.PHONY: all modules dtbo clean
