
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/pwm.h>
#include <linux/ioctl.h>

#include "my_ioctl.h"

#define DEVICE_NAME "mydev"
#define DEVICE_MAJOR 120
#define DEVICE_MINOR 0

#define PWM_PERIOD_NS 1000000ULL  // 1kHz
#define PWM_DUTY_NS   1000000ULL  // 100%

static dev_t devt;
static struct cdev my_cdev;
static struct pwm_device *fan_pwm;

/* 실제 Linux GPIO 번호를 insmod 시 전달 */
static int gpio_in1 = -1;
static int gpio_in2 = -1;
static int pwm_id = -1;

module_param(gpio_in1, int, 0444);
module_param(gpio_in2, int, 0444);
module_param(pwm_id, int, 0444);

MODULE_PARM_DESC(gpio_in1, "Linux GPIO number for IN1");
MODULE_PARM_DESC(gpio_in2, "Linux GPIO number for IN2");
MODULE_PARM_DESC(pwm_id, "Global PWM ID for pin 33");

/* PWM 상태 변경 */
static int fan_pwm_set(bool enable)
{
    struct pwm_state state;

    pwm_get_state(fan_pwm, &state);

    state.period = PWM_PERIOD_NS;
    state.duty_cycle = enable ? PWM_DUTY_NS : 0;
    state.polarity = PWM_POLARITY_NORMAL;
    state.enabled = enable;

    return pwm_apply_state(fan_pwm, &state);
}

/* GPIO 방향 설정: IN1=HIGH, IN2=LOW */
static void fan_gpio_test(void)
{
    gpio_set_value(gpio_in2, 0);
    gpio_set_value(gpio_in1, 1);

    pr_info("devtest: IN1=HIGH, IN2=LOW\n");
}

/* 출력 정지 */
static int fan_stop(void)
{
    int ret;

    ret = fan_pwm_set(false);

    gpio_set_value(gpio_in1, 0);
    gpio_set_value(gpio_in2, 0);

    pr_info("devtest: outputs stopped\n");

    return ret;
}

/* ioctl 명령 처리 */
static long device_ioctl(struct file *filp,
                         unsigned int cmd,
                         unsigned long arg)
{
    int ret = 0;

    switch (cmd) {

    case MY_IOCTL_GPIO_TEST:
        fan_gpio_test();
        break;

    case MY_IOCTL_PWM_TEST:
        ret = fan_pwm_set(true);

        if (ret == 0)
            pr_info("devtest: PWM 1kHz, duty 100%%\n");
        else
            pr_err("devtest: PWM failed (%d)\n", ret);

        break;

    case MY_IOCTL_STOP:
        ret = fan_stop();
        break;

    default:
        ret = -EINVAL;
        break;
    }

    return ret;
}

static int device_open(struct inode *inode,
                       struct file *filp)
{
    pr_info("devtest: device_open\n");
    return 0;
}

static int device_release(struct inode *inode,
                          struct file *filp)
{
    pr_info("devtest: device_release\n");
    return 0;
}

static const struct file_operations my_fops = {
    .owner          = THIS_MODULE,
    .open           = device_open,
    .release        = device_release,
    .unlocked_ioctl = device_ioctl,
};

/* 커널 모듈 초기화 */
static int __init device_init(void)
{
    int ret;

    pr_info("devtest: device_init\n");

    if (gpio_in1 < 0 || gpio_in2 < 0 || pwm_id < 0)
        return -EINVAL;

    if (gpio_in1 == gpio_in2)
        return -EINVAL;

    /* 1. 디바이스 번호 등록 */
    devt = MKDEV(DEVICE_MAJOR, DEVICE_MINOR);

    ret = register_chrdev_region(devt, 1, DEVICE_NAME);
    if (ret)
        return ret;

    /* 2. 문자 디바이스 등록 */
    cdev_init(&my_cdev, &my_fops);
    my_cdev.owner = THIS_MODULE;

    ret = cdev_add(&my_cdev, devt, 1);
    if (ret)
        goto err_chrdev;

    /* 3. GPIO 요청 */
    ret = gpio_request(gpio_in1, "fan_in1");
    if (ret)
        goto err_cdev;

    ret = gpio_request(gpio_in2, "fan_in2");
    if (ret)
        goto err_gpio1;

    /* 초기 상태는 두 핀 모두 LOW */
    ret = gpio_direction_output(gpio_in1, 0);
    if (ret)
        goto err_gpio2;

    ret = gpio_direction_output(gpio_in2, 0);
    if (ret)
        goto err_gpio2;

    /* 4. PWM 요청 */
    fan_pwm = pwm_request(pwm_id, "fan_pwm");
    if (IS_ERR(fan_pwm)) {
        ret = PTR_ERR(fan_pwm);
        goto err_gpio2;
    }

    /* 초기 상태: PWM 비활성화 */
    ret = fan_pwm_set(false);
    if (ret)
        goto err_pwm;

    pr_info("devtest: GPIO initialized\n");
    pr_info("devtest: PWM initialized (1kHz)\n");
    pr_info("devtest: major=%d minor=%d\n",
            DEVICE_MAJOR, DEVICE_MINOR);

    return 0;

err_pwm:
    pwm_free(fan_pwm);
err_gpio2:
    gpio_free(gpio_in2);
err_gpio1:
    gpio_free(gpio_in1);
err_cdev:
    cdev_del(&my_cdev);
err_chrdev:
    unregister_chrdev_region(devt, 1);

    return ret;
}

/* 커널 모듈 해제 */
static void __exit device_exit(void)
{
    fan_stop();

    pwm_free(fan_pwm);

    gpio_free(gpio_in2);
    gpio_free(gpio_in1);

    cdev_del(&my_cdev);
    unregister_chrdev_region(devt, 1);

    pr_info("devtest: device_exit\n");
}

module_init(device_init);
module_exit(device_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Jetson Orin Nano GPIO PWM test driver");
