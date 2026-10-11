#ifndef FAN_APP_H
#define FAN_APP_H

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

/* 실제 IIO 디바이스 번호에 맞게 수정 */
#define BMP180_TEMP_PATH \
    "/sys/bus/iio/devices/iio:device0/in_temp_input"

#endif /* FAN_APP_H */