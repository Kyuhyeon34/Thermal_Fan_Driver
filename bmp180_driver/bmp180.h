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

#define BMP180_REG_CHIP_ID      0xD0
#define BMP180_CHIP_ID_ADDR     0x55

// Calibration Coefficients 시작 주소
#define BMP180_REG_CALIB_START  0xAA
// Calibaration Coefficients 영역 크기
#define BMP180_CALIB_LEN        22

#define BMP180_REG_SOFT_RESET   0xE0
#define BMP180_SOFT_RESET_CMD   0xB6

#define BMP_REG_CTRL_MEAS       0xF4
#define BMP180_REG_OUT_MSB      0xF6



#endif /* BMP180_H */