
#ifndef MY_IOCTL_H
#define MY_IOCTL_H

#include <linux/ioctl.h>

#define MY_IOCTL_MAGIC     'F'

#define MY_IOCTL_GPIO_TEST _IO(MY_IOCTL_MAGIC, 0)
#define MY_IOCTL_PWM_TEST  _IO(MY_IOCTL_MAGIC, 1)
#define MY_IOCTL_STOP      _IO(MY_IOCTL_MAGIC, 2)

#endif