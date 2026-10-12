#include "motor.h"

// module_param(gpio_in1, int, 0444);
// module_param(gpio_in2, int, 0444);
// module_param(pwm_id, int, 0444);

// MODULE_PARM_DESC(gpio_in1, "Linux GPIO number for IN1");
// MODULE_PARM_DESC(gpio_in2, "Linux GPIO number for IN2");
// MODULE_PARM_DESC(pwm_id, "Global PWM ID for pin 33");

#if 0
static int gpio_init(void)
{

    int ret;

    // gpio 할당
    ret = gpio_request(GPIO_IN1, "motor_in1");
    if (ret)
    {
        pr_info("gpio_request failed!!!\n");
        return ret;
    }

    ret = gpio_request(GPIO_IN2, "motor_in2");
    if (ret)
    {
        pr_info("gpio_request failed!!!\n");
        goto err_in1;
    }

    // 출력으로 방향 지정, 초깃값 LOW
    ret = gpio_direction_output(GPIO_IN1, 0);
    if (ret)
    {
        pr_info("GPIO direction set failed!!!\n");
        goto err_in2;
    }
    ret = gpio_direction_output(GPIO_IN2, 0);
    if (ret)
    {
        pr_info("GPIO direction set failed!!!\n");
        goto err_in2;
    }

    return 0;

err_in2:
    gpio_free(GPIO_IN2);
err_in1:
    gpio_free(GPIO_IN1);
    return ret;
};
#endif

/* Apply PWM duty through a DT-managed PWM device */
static int motor_pwm_apply(struct motor_dev *motor, bool enable, unsigned int duty_percent)
{
    struct pwm_state state;
    u64 duty_ns;

    if (!motor || !motor->pwm)
        return -EINVAL;

    if (duty_percent > 100)
        return -EINVAL;

    /* Convert percentage to nanoseconds */
    duty_ns = PWM_PERIOD_NS * (u64)duty_percent / 100;

    /* Read the current PWM state */
    pwm_get_state(motor->pwm, &state);

    /* Configure PWM */
    state.period = PWM_PERIOD_NS;
    state.duty_cycle = enable ? duty_ns : 0;
    state.polarity = PWM_POLARITY_NORMAL;
    state.enabled = enable;

    /* Apply the PWM settings */
    return pwm_apply_state(motor->pwm, &state);
};

#if 0
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
};
#endif

/* Stop the motor safely */
static int motor_stop(struct motor_dev *motor)
{
    int ret;

    if (!motor)
        return -EINVAL;

    /* Disable PWM output */
    ret = motor_pwm_apply(motor, false, 0);

    /* Set both direction pins LOW */
    gpiod_set_value(motor->in1, 0);
    gpiod_set_value(motor->in2, 0);

    if (ret)
        dev_err(motor->dev,
                "Failed to disable PWM (%d)\n", ret);
    else
        dev_info(motor->dev, "Motor stopped\n");

    return ret;
};

#if 0
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
};
#endif

/* Control the motor speed using PWM */
static int motor_set_speed(struct motor_dev *motor,
                           unsigned int duty_percent)
{
    int ret;

    if (!motor)
        return -EINVAL;

    if (duty_percent > 100)
        return -EINVAL;

    /* Duty 0% = stop */
    if (duty_percent == 0)
        return motor_stop(motor);

    /* Motor forward direction */
    gpiod_set_value(motor->in1, 1);
    gpiod_set_value(motor->in2, 0);

    /* Apply PWM */
    ret = motor_pwm_apply(motor, true, duty_percent);

    if (ret)
    {
        dev_err(motor->dev,
                "PWM apply failed (%d)\n", ret);

        motor_stop(motor);
        return ret;
    }

    return 0;
};

#if 0
/* 모터 duty 설정, 방향핀(IN1, IN2 EN)*/
static int motor_set_speed(unsigned int duty_percent)
{
    int ret;

    if (duty_percent > 100)
        return -EINVAL;

    if (duty_percent == 0)
        return motor_stop();

    gpio_set_value(GPIO_IN1, 1);
    gpio_set_value(GPIO_IN2, 0);

    ret = motor_pwm_apply(true, duty_percent);

    if (ret) {
        pr_err("motor: PWM apply failed (%d)\n", ret);
        motor_stop();
        return ret;
    }

    return 0;
};
#endif

/* Handle ioctl commands from user space */
static long device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct motor_dev *motor = filp->private_data;
    __u32 duty_percent;
    int ret;

    if (!motor)
        return -ENODEV;

    switch (cmd)
    {

    case MY_IOCTL_CMD_SET_PWM:

        /* Copy the requested duty from user space */
        if (copy_from_user(&duty_percent,
                           (void __user *)arg,
                           sizeof(duty_percent)))
            return -EFAULT;

        if (duty_percent > 100)
            return -EINVAL;

        /* Protect shared motor hardware */
        mutex_lock(&motor->lock);

        ret = motor_set_speed(motor, duty_percent);

        mutex_unlock(&motor->lock);

        if (ret)
        {
            dev_err(motor->dev,
                    "Failed to set PWM duty: %d\n",
                    ret);
            return ret;
        }

        dev_info(motor->dev,
                 "PWM Duty: %u%%\n",
                 duty_percent);

        return 0;

    default:
        return -ENOTTY;
    }
};

#if 0
/* ioctl 명령 처리 */
static long device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    unsigned int duty_percent;
    int ret = 0;

    switch (cmd)
    {

    case MY_IOCTL_CMD_SET_PWM:
        if (copy_from_user(&duty_percent, (void __user *)arg, sizeof(duty_percent)))
        {
            return -EFAULT;
        }

        if (duty_percent > 100)
            return -EINVAL;

        mutex_lock(&motor_lock);

        printk("duty: %u\n", duty_percent);        

        ret = motor_set_speed(duty_percent);
        mutex_unlock(&motor_lock);

        break;

        // ioctl case 추가: get count

    default:
        ret = -ENOTTY;
        break;
    }

    return ret;
};
#endif

/* Called when /dev/mydev is opened */
static int device_open(struct inode *inode, struct file *filp)
{
    struct miscdevice *misc = filp->private_data;

    struct motor_dev *motor =
        container_of(misc, struct motor_dev, miscdev);

    filp->private_data = motor;

    dev_info(motor->dev, "Motor device opened\n");

    return 0;
};

#if 0
static int device_open(struct inode *inode,
                       struct file *filp)
{
    pr_info("devtest: device_open\n");
    return 0;
};
#endif /* device_open_old */

/* Called when /dev/mydev is closed */
static int device_release(struct inode *inode, struct file *filp)
{
    struct motor_dev *motor = filp->private_data;
    int ret;

    if (!motor)
        return -ENODEV;

    mutex_lock(&motor->lock);

    ret = motor_stop(motor);

    mutex_unlock(&motor->lock);

    dev_info(motor->dev, "Motor device closed\n");

    return ret;
};

#if 0
static int device_release(struct inode *inode,
                          struct file *filp)
{
    int ret;
    mutex_lock(&motor_lock);

    ret = motor_stop();

    mutex_unlock(&motor_lock);

    pr_info("devtest: device_release\n");
    return ret;
};
#endif /* device_release_old */

static const struct file_operations my_fops = {
    .owner = THIS_MODULE,
    .open = device_open,
    .release = device_release,
    .unlocked_ioctl = device_ioctl,
};

/* Probe a motor device described by Device Tree */
static int motor_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct motor_dev *motor;
    int ret;

    /* 1. Allocate per-device private data */
    motor = devm_kzalloc(dev, sizeof(*motor), GFP_KERNEL);
    if (!motor)
        return -ENOMEM;

    motor->dev = dev;
    mutex_init(&motor->lock);

    /* 2. Acquire IN1 GPIO */
    motor->in1 = devm_gpiod_get(
        dev, "in1", GPIOD_OUT_LOW);

    if (IS_ERR(motor->in1))
        return dev_err_probe(
            dev, PTR_ERR(motor->in1),
            "Failed to get IN1 GPIO\n");

    /* 3. Acquire IN2 GPIO */
    motor->in2 = devm_gpiod_get(
        dev, "in2", GPIOD_OUT_LOW);

    if (IS_ERR(motor->in2))
        return dev_err_probe(
            dev, PTR_ERR(motor->in2),
            "Failed to get IN2 GPIO\n");

    /* 4. Acquire PWM */
    motor->pwm = devm_pwm_get(dev, "motor");

    if (IS_ERR(motor->pwm))
        return dev_err_probe(
            dev, PTR_ERR(motor->pwm),
            "Failed to get motor PWM\n");

    /* 5. Initialize PWM in stopped state */
    ret = motor_pwm_apply(motor, false, 0);

    if (ret)
        return dev_err_probe(
            dev, ret,
            "Failed to initialize PWM\n");

    /* 6. Register misc character device */
    motor->miscdev.minor = MISC_DYNAMIC_MINOR;
    motor->miscdev.name = DEVICE_NAME;
    motor->miscdev.fops = &my_fops;
    motor->miscdev.parent = dev;

    ret = misc_register(&motor->miscdev);

    if (ret)
        return dev_err_probe(
            dev, ret,
            "Failed to register motor misc device\n");

    /* 7. Save driver private data */
    platform_set_drvdata(pdev, motor);

    dev_info(dev,
             "Motor driver probed successfully\n");

    return 0;
};

static int motor_remove(struct platform_device *pdev)
{
    struct motor_dev *motor =
        platform_get_drvdata(pdev);

    misc_deregister(&motor->miscdev);

    mutex_lock(&motor->lock);
    motor_stop(motor);
    mutex_unlock(&motor->lock);

    dev_info(&pdev->dev, "Motor driver removed\n");

    return 0;
};

#if 0
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
    if (ret)
        goto err_cdev;

    /* 4. PWM 요청 */
    motor_pwm = pwm_request(PWM_ID, "motor_pwm");
    if (IS_ERR(motor_pwm))
    {
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
};

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
};

module_init(device_init);
module_exit(device_exit);
#endif /* device_init + device_exit */

static const struct of_device_id motor_of_match[] = {
    {
        .compatible = MOTOR_COMPATIBLE,
    },
    {

    }
};

MODULE_DEVICE_TABLE(of, motor_of_match);

static struct platform_driver motor_driver = {
    .probe = motor_probe,
    .remove = motor_remove,
    .driver = {
        .name = "smartfan-motor",
        .of_match_table = motor_of_match,
    },
};

module_platform_driver(motor_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Jetson Orin Nano GPIO PWM test driver");