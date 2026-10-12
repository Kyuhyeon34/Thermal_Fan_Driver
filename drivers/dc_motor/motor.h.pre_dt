#ifndef MOTOR_H
#define MOTOR_H

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/pwm.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include "motor_ioctl.h"

#define DEVICE_NAME "mydev"
#define DEVICE_MAJOR 120
#define DEVICE_MINOR 0

#define PWM_PERIOD_NS 1000000ULL  // 1kHz

#define GPIO_IN1 492
#define GPIO_IN2 433
#define PWM_ID 2

#endif /* MOTOR_H */