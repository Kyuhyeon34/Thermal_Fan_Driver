#ifndef MOTOR_IOCTL_H
#define MOTOR_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define MY_IOCTL_MAGIC     'M'

#define MY_IOCTL_CMD_SET_PWM _IOW(MY_IOCTL_MAGIC, 0, __u32)
// #define MY_IOCTL_PWM_TEST  _IO(MY_IOCTL_MAGIC, 1)
// #define MY_IOCTL_STOP      _IO(MY_IOCTL_MAGIC, 2)

#endif /* MOTOR_IOCTL_H */