#ifndef BMP180_H
#define BMP180_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <linux/iio/iio.h>
#include <linux/mutex.h>

/* Chip identification */
#define BMP180_REG_CHIP_ID 0xD0
#define BMP180_CHIP_ID_ADDR 0x55

/* Calibration registers */
// Calibration Coefficients 시작 주소
#define BMP180_REG_CALIB_START 0xAA
// Calibaration Coefficients 영역 크기
#define BMP180_CALIB_LEN 22

#define BMP180_REG_SOFT_RESET 0xE0
#define BMP180_SOFT_RESET_CMD 0xB6

/* Measurement registers */
#define BMP_REG_CTRL_MEAS 0xF4
#define BMP180_REG_OUT_MSB 0xF6

/* Measurement commands */
#define BMP180_CMD_TEMP 0x2E
#define BMP180_CMD_PRESSURE 0x34

/* Oversampling */
#define BMP180_DEFAULT_OSS 0
#define BMP180_MAX_OSS 3

struct bmp180_data
{
    struct i2c_client *client; // Linux Kernel이 BMP180을 식별하고, 통신하기 위한 I2C Client 정보를 가리킨다.

    /* Signed calibration coefficients */
    s16 ac1;
    s16 ac2;
    s16 ac3;

    s16 b1;
    s16 b2;
    s16 mb;
    s16 mc;
    s16 md;

    /* Unsigned calibration coefficients */
    u16 ac4;
    u16 ac5;
    u16 ac6;

    /* Oversampling setting */
    u8 oss;

    /* Protect sensor conversion sequence */
    struct mutex lock;
};

#endif /* BMP180_H */