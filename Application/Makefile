CC ?= aarch64-linux-gnu-gcc
CFLAGS += -Wall -Wextra -O2
CFLAGS += -I../drivers/dc_motor

all: fan_app

fan_app: fan_app.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f fan_app