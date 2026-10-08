
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

#include "my_ioctl.h"

#define DEV_NAME "/dev/mydev"

int main(int argc, char *argv[])
{
    int fd;
    unsigned long cmd;

    if (argc != 2) {
        printf("Usage: %s gpio|pwm|off\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "gpio") == 0) {
        cmd = MY_IOCTL_GPIO_TEST;
    }
    else if (strcmp(argv[1], "pwm") == 0) {
        cmd = MY_IOCTL_PWM_TEST;
    }
    else if (strcmp(argv[1], "off") == 0) {
        cmd = MY_IOCTL_STOP;
    }
    else {
        printf("Invalid command\n");
        return EXIT_FAILURE;
    }

    fd = open(DEV_NAME, O_RDWR);

    if (fd == -1) {
        perror("apptest: open");
        return EXIT_FAILURE;
    }

    printf("apptest: %s opened\n", DEV_NAME);

    if (ioctl(fd, cmd) == -1) {
        perror("apptest: ioctl");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("apptest: ioctl success\n");

    close(fd);

    printf("apptest: %s closed\n", DEV_NAME);

    return EXIT_SUCCESS;
}