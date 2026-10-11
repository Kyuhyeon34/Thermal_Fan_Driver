#include "fan_app.h"

static volatile sig_atomic_t running = 1;

/* Signal handler */
static void signal_handler(int sig)
{
    (void)sig;
    running = 0;
};

/*
 * Read BMP180 temperature via the IIO sysfs interface.
 * Unit: milli-degree Celsius (mC).
 */
static int fan_read_temperature(int *temp_mc)
{
    FILE *fp;
    long value;

    if (temp_mc == NULL)
        return -EINVAL;

    fp = fopen(TEMP_PATH, "r");
    if (fp == NULL)
        return -errno;

    if (fscanf(fp, "%ld", &value) != 1) {
        int err = ferror(fp) ? errno : EIO;
        fclose(fp);
        return -(err ? err : EIO);
    }

    if (fclose(fp) != 0)
        return -errno;

    if (value < -40000 || value > 85000)
        return -ERANGE;

    *temp_mc = (int)value;

    return 0;
};

/* Hysteresis + proportional fan duty control */
static unsigned int fan_calculate_duty(fan_state_t *fan)
{
    int temp = fan->current_temp_mc;

    /* Step 1. Hysteresis ON/OFF */
    if (!fan->fan_on && temp >= FAN_ON_TEMP_MC)
        fan->fan_on = 1;

    else if (fan->fan_on && temp < FAN_OFF_TEMP_MC)
        fan->fan_on = 0;

    if (!fan->fan_on)
        return 0;

    if (temp <= 22000)
        return MIN_DUTY_PERCENT;

    if (temp >= 32000)
        return MAX_DUTY_PERCENT;

    /* 25C ~ 35C : 40% ~ 100% */
    return MIN_DUTY_PERCENT +
           (unsigned int)((temp - 22000) * 60 / 10000);
};

/* Send PWM Duty to the Motor Character Device Driver */
static int fan_motor_set_duty(int motor_fd, unsigned int duty_percent)
{
    __u32 duty = (__u32)duty_percent;

    if (duty_percent > 100)
        return -EINVAL;

    if (ioctl(motor_fd, MY_IOCTL_CMD_SET_PWM, &duty) < 0)
        return -errno;

    return 0;
};

/* Wait for the next sampling interval */
static void fan_wait_interval(void)
{
    struct timespec req = {
        .tv_sec = SAMPLE_INTERVAL_SEC,
        .tv_nsec = 0
    };

    struct timespec rem;

    while (running && nanosleep(&req, &rem) < 0) {
        if (errno != EINTR)
            break;
        req = rem;
    }
};

int main(void)
{
    int motor_fd;
    int ret;
    fan_state_t fan = {0};

    struct sigaction sa = {0};

    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* Open Motor Driver once */
    motor_fd = open(MOTOR_DEV_PATH, O_RDWR);

    if (motor_fd < 0) {
        perror("Motor device open");
        return EXIT_FAILURE;
    }

    /* Initial motor stop */
    ret = fan_motor_set_duty(motor_fd, 0);
    if (ret < 0) {
        fprintf(stderr, "Motor initialization failed: %d\n", ret);
        close(motor_fd);
        return EXIT_FAILURE;
    }

    printf("Automatic Fan Control Started\n");

    while (running) {

        /* 1. Read current temperature */
        ret = fan_read_temperature(&fan.current_temp_mc);

        if (ret < 0) {
            fprintf(stderr, "BMP180 temperature read failed: %d\n", ret);

            fan.temp_valid = 0;
            fan.fan_on = 0;
            fan.duty_percent = 0;

            /* Fail-safe policy: stop the motor */
            if (fan_motor_set_duty(motor_fd, 0) < 0) {
                fprintf(stderr, "Motor stop failed\n");
                break;
            }

            fan_wait_interval();
            continue;
        }

        fan.temp_valid = 1;

        /* 2. Calculate PWM Duty */
        fan.duty_percent = fan_calculate_duty(&fan);

        /* 3. Apply PWM through ioctl */
        ret = fan_motor_set_duty(motor_fd, fan.duty_percent);

        if (ret < 0) {
            fprintf(stderr, "Motor ioctl failed: %d\n", ret);
            break;
        }

        /* 4. Log current status */
        printf("Temp: %.1f C | Fan: %s | Duty: %u%%\n",
               fan.current_temp_mc / 1000.0,
               fan.fan_on ? "ON" : "OFF",
               fan.duty_percent);

        fflush(stdout);

        /* 5. Wait one second */
        fan_wait_interval();
    }

    /* Stop motor before closing */
    if (fan_motor_set_duty(motor_fd, 0) < 0)
        fprintf(stderr, "Final motor stop failed\n");

    close(motor_fd);

    printf("Automatic Fan Control Stopped\n");

    return EXIT_SUCCESS;
};