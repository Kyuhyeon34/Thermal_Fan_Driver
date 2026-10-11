#ifndef FAN_APP_H
#define FAN_APP_H

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/ioctl.h>

#include "motor_ioctl.h"

#define MOTOR_DEV_PATH "/dev/mydev"

/* Example path: replace with the actual BMP180 IIO device */
#define TEMP_PATH \
    "/sys/bus/iio/devices/iio:device0/in_temp_input"

#define SAMPLE_INTERVAL_SEC 1

#define FAN_ON_TEMP_MC   26000
#define FAN_OFF_TEMP_MC  24000
#define MIN_DUTY_PERCENT 40
#define MAX_DUTY_PERCENT 100

typedef struct {
    int current_temp_mc;
    unsigned int duty_percent;
    int fan_on;
    int temp_valid;
} fan_state_t;

#endif /* FAN_APP_H */