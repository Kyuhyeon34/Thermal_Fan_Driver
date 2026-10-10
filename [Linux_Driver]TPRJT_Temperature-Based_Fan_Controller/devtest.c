
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

#include "my_ioctl.h"

#define DEVICE_NAME "mydev"
#define DEVICE_MAJOR 120
#define DEVICE_MINOR 0

#define PWM_PERIOD_NS 1000000ULL  // 1kHz

#define GPIO_IN1 492
#define GPIO_IN2 433
#define PWM_ID 2

static dev_t devt;
static struct cdev my_cdev;
static struct pwm_device *motor_pwm;

// module_param(gpio_in1, int, 0444);
// module_param(gpio_in2, int, 0444);
// module_param(pwm_id, int, 0444);

// MODULE_PARM_DESC(gpio_in1, "Linux GPIO number for IN1");
// MODULE_PARM_DESC(gpio_in2, "Linux GPIO number for IN2");
// MODULE_PARM_DESC(pwm_id, "Global PWM ID for pin 33");

static int gpio_init(void){

    int ret;

    //gpio 할당
    ret = gpio_request(GPIO_IN1, "motor_in1");
    if (ret){
        pr_info("gpio_request failed!!!\n");
        return ret;
    }

    ret = gpio_request(GPIO_IN2, "motor_in2");
    if (ret){
        pr_info("gpio_request failed!!!\n");
        goto err_in1;
    }

    //출력으로 방향 지정, 초깃값 LOW
    ret = gpio_direction_output(GPIO_IN1, 0);
    if (ret){
        pr_info("GPIO direction set failed!!!\n");
        goto err_in2;
    }
    ret = gpio_direction_output(GPIO_IN2, 0);
    if (ret){
        pr_info("GPIO direction set failed!!!\n");
        goto err_in1;
    }

    return 0;

err_in2:
    gpio_free(GPIO_IN2);
err_in1:
    gpio_free(GPIO_IN1);
    return ret;

}

/* PWM 상태 변경 */
static int motor_pwm_apply(bool enable, unsigned int duty_percent)
{
    struct pwm_state state;
    u32 duty_ns;

    if (duty_percent > 100)
        return -EINVAL;

    duty_ns = PWM_PERIOD_NS * duty_percent / 100;
    
    pwm_get_state(motor_pwm, &state);
    
    state.period = PWM_PERIOD_NS;
    state.duty_cycle = enable ? duty_ns : 0;
    state.polarity = PWM_POLARITY_NORMAL;
    state.enabled = enable;
    
    return pwm_apply_state(motor_pwm, &state);
}

/* 출력 정지 */
static int motor_stop(void)
{
    int ret;

    gpio_set_value(GPIO_IN1, 0);
    gpio_set_value(GPIO_IN2, 0);

    ret = motor_pwm_apply(false, 0);
    if (ret)
        pr_err("devtest: failed to disable PWM (%d)\n", ret);
    else
        pr_info("devtest: outputs stopped\n");

    return ret;
}

/* 모터 duty 설정, 방향핀(IN1, IN2 EN)*/
static int motor_set_speed(unsigned int duty_percent)
{
    if (duty_percent == 0)
        return motor_stop();

    gpio_set_value(GPIO_IN1, 1);    
    gpio_set_value(GPIO_IN2, 0);

    return motor_pwm_apply(true, duty_percent);
}    

/* ioctl 명령 처리 */
static long device_ioctl(struct file *filp,
                         unsigned int cmd,
                         unsigned long arg)
{
    unsigned int duty_percent;
    int ret = 0;

    switch (cmd) {

    case MY_IOCTL_CMD_SET_PWM:
        if(copy_from_user(&duty_percent, (void __user *)arg, sizeof(duty_percent))){
            return -EFAULT;
        }

        if(duty_percent > 100)
            return -EINVAL;

        return motor_set_speed(duty_percent);

    //ioctl case 추가: get count

    default:
        ret = -ENOTTY;
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
    int ret = motor_stop();

    pr_info("devtest: device_release\n");
    return ret;
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

    ret = gpio_init();
    if(ret)
        goto err_cdev;

    /* 4. PWM 요청 */
    motor_pwm = pwm_request(PWM_ID, "motor_pwm");
    if (IS_ERR(motor_pwm)) {
        ret = PTR_ERR(motor_pwm);
        goto err_gpio;
    }

    /* 초기 상태: PWM 비활성화 */
    ret = motor_pwm_apply(false, 0);
    if (ret)
        goto err_pwm;

    pr_info("devtest: GPIO initialized\n");
    pr_info("devtest: PWM initialized (1kHz)\n");
    pr_info("devtest: major=%d minor=%d\n",
            DEVICE_MAJOR, DEVICE_MINOR);

    return 0;

err_pwm:
    pwm_free(motor_pwm);
err_gpio:
    gpio_free(GPIO_IN2);
    gpio_free(GPIO_IN1);
err_cdev:
    cdev_del(&my_cdev);
err_chrdev:
    unregister_chrdev_region(devt, 1);

    return ret;
}

/* 커널 모듈 해제 */
static void __exit device_exit(void)
{
    motor_stop();

    pwm_free(motor_pwm);

    gpio_free(GPIO_IN2);
    gpio_free(GPIO_IN1);

    cdev_del(&my_cdev);
    unregister_chrdev_region(devt, 1);

    pr_info("devtest: device_exit\n");
}

module_init(device_init);
module_exit(device_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Jetson Orin Nano GPIO PWM test driver");
