obj-m := ec11.o ledbar.o

KERNELDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
CC := aarch64-linux-gnu-gcc

default: modules apps

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

apps: ec11_test fan_ctrl

ec11_test: ec11_test.c ec11_ioctl.h
	$(CC) -Wall -o $@ ec11_test.c

# main app: EC11 state machine + mode LED + LED bar
fan_ctrl: fan_ctrl.c fan_state.c fan_state.h ledbar_ctrl.c ledbar_ctrl.h fan_globals.h ledbar_ioctl.h ec11_ioctl.h
	$(CC) -O2 -Wall -o $@ fan_ctrl.c fan_state.c ledbar_ctrl.c -lm

# LED bar team code (unchanged)
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
	rm -f *.o *.ko *.mod *.mod.c modules.order Module.symvers
	rm -f .*.*.cmd .*.*.*.cmd
	rm -rf .tmp_versions
	rm -f ec11_test fan_ctrl ledbar_test fan_led_sim ledbar.dtbo

.PHONY: default modules apps test sim dtbo clean
