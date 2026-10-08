obj-m := ledbar.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all: modules dtbo

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

test: ledbar_test

ledbar_test: ledbar_test.c ledbar_ioctl.h
	$(CC) -O2 -Wall -o $@ ledbar_test.c -lm

sim: fan_led_sim

fan_led_sim: fan_led_sim.c ledbar_ctrl.c ledbar_ctrl.h fan_globals.h ledbar_ioctl.h
	$(CC) -O2 -Wall -o $@ fan_led_sim.c ledbar_ctrl.c -lm

dtbo: ledbar.dtbo

ledbar.dtbo: ledbar-overlay.dts
	dtc -@ -I dts -O dtb -o $@ $<

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f ledbar_test fan_led_sim ledbar.dtbo

.PHONY: all modules test sim dtbo clean
