#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/kfifo.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/spinlock.h>

#include <linux/gpio/machine.h>

#include "ec11_ioctl.h"

#define DEVICE_NAME "ec11"

/*
 * 1: bind to the "smartfan,ec11" device tree node (a-gpios, b-gpios, key-gpios)
 * 0: the module registers the platform device itself, pins are set below
 */
#define CONF_DT 0

#if CONF_DT
#else
#define GPIOCHIP_LABEL	"tegra234-gpio"
#define GPIO_PIN_A	105	/* header pin 29, PQ.05, EC11 S1 */
#define GPIO_PIN_B	106	/* header pin 31, PQ.06, EC11 S2 */
#define GPIO_PIN_KEY	53	/* header pin 35, PI.02, EC11 KEY (active low) */
#endif
#define EVENT_FIFO_SIZE 64	/* number of events, must be a power of 2 */
#define READ_MAX_EVENTS 16

static int steps_per_detent = 4;
module_param(steps_per_detent, int, 0444);
MODULE_PARM_DESC(steps_per_detent, "quadrature transitions per detent (1, 2 or 4)");

static bool reverse;
module_param(reverse, bool, 0644);
MODULE_PARM_DESC(reverse, "swap CW/CCW direction");

static int debounce_ms = 20;
module_param(debounce_ms, int, 0644);
MODULE_PARM_DESC(debounce_ms, "key debounce time in ms");

static int long_press_ms = 2000;
module_param(long_press_ms, int, 0644);
MODULE_PARM_DESC(long_press_ms, "key long press time in ms");

struct ec11_dev {
	struct gpio_desc *gpio_a;
	struct gpio_desc *gpio_b;
	struct gpio_desc *gpio_key;
	int irq_a;
	int irq_b;
	int irq_key;

	spinlock_t lock;	/* protects everything below and the fifo */

	/* rotation */
	int ab_state;		/* (A << 1) | B */
	int rest_state;		/* A/B state at a detent */
	int acc;		/* transitions accumulated since the last detent */
	int count;

	/* key */
	int key_pressed;	/* debounced key state */
	int long_fired;
	struct timer_list debounce_timer;
	struct timer_list long_timer;

	DECLARE_KFIFO(events, struct ec11_event, EVENT_FIFO_SIZE);
	wait_queue_head_t wq;

	dev_t devt;
	struct cdev cdev;
	struct class *class;
};

/* index: (old_state << 2) | new_state, invalid transitions (both bits changed) give 0 */
static const signed char quad_table[16] = {
	 0, -1,  1,  0,
	 1,  0,  0, -1,
	-1,  0,  0,  1,
	 0,  1, -1,  0,
};

/* must be called with ec->lock held */
static void ec11_push_event(struct ec11_dev *ec, int type, int delta)
{
	struct ec11_event ev = {
		.type = type,
		.delta = delta,
		.count = ec->count,
		.state = ((ec->ab_state >> 1) & 1 ? EC11_STATE_S1 : 0) |
			 (ec->ab_state & 1 ? EC11_STATE_S2 : 0) |
			 (ec->key_pressed ? EC11_STATE_KEY : 0),
	};

	if (!kfifo_put(&ec->events, ev))
		printk_ratelimited("ec11: event fifo full, event dropped\n");
	wake_up_interruptible(&ec->wq);
}

static bool ec11_is_rest_state(struct ec11_dev *ec, int state)
{
	if (steps_per_detent == 4)
		return state == ec->rest_state;
	if (steps_per_detent == 2)	/* detents at both 00 and 11 */
		return state == ec->rest_state || state == (ec->rest_state ^ 0x3);
	return true;
}

static irqreturn_t ec11_rotary_isr(int irq, void *dev_id)
{
	struct ec11_dev *ec = dev_id;
	unsigned long flags;
	int state, dir = 0, count = 0;

	state = (gpiod_get_value(ec->gpio_a) << 1) | gpiod_get_value(ec->gpio_b);

	spin_lock_irqsave(&ec->lock, flags);
	ec->acc += quad_table[(ec->ab_state << 2) | state];
	if (state != ec->ab_state) {
		ec->ab_state = state;
		ec11_push_event(ec, EC11_EV_STATE, 0);
	}

	/*
	 * Decide the step only when the encoder settles at a detent.
	 * Contact bounce moves back and forth (+1, -1) and cancels out in acc.
	 */
	if (ec11_is_rest_state(ec, state)) {
		int threshold = steps_per_detent > 1 ? steps_per_detent / 2 : 1;

		if (ec->acc >= threshold)
			dir = 1;
		else if (ec->acc <= -threshold)
			dir = -1;
		ec->acc = 0;

		if (dir) {
			if (reverse)
				dir = -dir;
			ec->count += dir;
			count = ec->count;
			ec11_push_event(ec, EC11_EV_ROTATE, dir);
		}
	}
	spin_unlock_irqrestore(&ec->lock, flags);

	if (dir)
		printk("ec11: rotate %s count = %d\n", dir > 0 ? "CW " : "CCW", count);

	return IRQ_HANDLED;
}

static irqreturn_t ec11_key_isr(int irq, void *dev_id)
{
	struct ec11_dev *ec = dev_id;

	/* restart the debounce timer on every edge, the key is read once it is stable */
	mod_timer(&ec->debounce_timer, jiffies + msecs_to_jiffies(debounce_ms));

	return IRQ_HANDLED;
}

static void ec11_debounce_timer_fn(struct timer_list *t)
{
	struct ec11_dev *ec = from_timer(ec, t, debounce_timer);
	unsigned long flags;
	int pressed, clicked = 0, count;

	pressed = gpiod_get_value(ec->gpio_key);

	spin_lock_irqsave(&ec->lock, flags);
	if (pressed != ec->key_pressed) {
		ec->key_pressed = pressed;
		ec11_push_event(ec, EC11_EV_STATE, 0);
		if (pressed) {
			ec->long_fired = 0;
			mod_timer(&ec->long_timer, jiffies + msecs_to_jiffies(long_press_ms));
		} else {
			del_timer(&ec->long_timer);
			if (!ec->long_fired) {
				clicked = 1;
				ec11_push_event(ec, EC11_EV_CLICK, 0);
			}
		}
	}
	count = ec->count;
	spin_unlock_irqrestore(&ec->lock, flags);

	if (clicked)
		printk("ec11: click (count = %d)\n", count);
}

static void ec11_long_timer_fn(struct timer_list *t)
{
	struct ec11_dev *ec = from_timer(ec, t, long_timer);
	unsigned long flags;
	int fired = 0;

	spin_lock_irqsave(&ec->lock, flags);
	if (ec->key_pressed && !ec->long_fired) {
		ec->long_fired = 1;
		fired = 1;
		ec11_push_event(ec, EC11_EV_LONG_PRESS, 0);
	}
	spin_unlock_irqrestore(&ec->lock, flags);

	if (fired)
		printk("ec11: long press (%d ms)\n", long_press_ms);
}

static long device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct ec11_dev *ec = filp->private_data;
	unsigned long flags;
	int ret = 0, data;

	switch(cmd) {
		case EC11_IOCTL_GET_COUNT:
			spin_lock_irqsave(&ec->lock, flags);
			data = ec->count;
			spin_unlock_irqrestore(&ec->lock, flags);
			if(copy_to_user((int __user *)arg, &data, sizeof(int))) {
				return -EFAULT;
			}
			break;
		case EC11_IOCTL_RESET_COUNT:
			printk("ec11: EC11_IOCTL_RESET_COUNT\n");
			spin_lock_irqsave(&ec->lock, flags);
			ec->count = 0;
			ec->acc = 0;
			spin_unlock_irqrestore(&ec->lock, flags);
			break;
		default:
			printk("ec11: unknown command\n");
			ret = -EINVAL;
			break;
	}

	return ret;
}

static bool ec11_fifo_empty(struct ec11_dev *ec)
{
	unsigned long flags;
	bool empty;

	spin_lock_irqsave(&ec->lock, flags);
	empty = kfifo_is_empty(&ec->events);
	spin_unlock_irqrestore(&ec->lock, flags);

	return empty;
}

/* blocks until at least one event is available, returns whole struct ec11_event records */
static ssize_t device_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct ec11_dev *ec = filp->private_data;
	struct ec11_event evs[READ_MAX_EVENTS];
	unsigned long flags;
	unsigned int n, max;
	int ret;

	if(count < sizeof(struct ec11_event)) {
		return -EINVAL;
	}
	max = min_t(size_t, count / sizeof(struct ec11_event), READ_MAX_EVENTS);

	do {
		if(ec11_fifo_empty(ec)) {
			if(filp->f_flags & O_NONBLOCK) {
				return -EAGAIN;
			}
			ret = wait_event_interruptible(ec->wq, !ec11_fifo_empty(ec));
			if(ret) {
				return ret;
			}
		}
		spin_lock_irqsave(&ec->lock, flags);
		n = kfifo_out(&ec->events, evs, max);
		spin_unlock_irqrestore(&ec->lock, flags);
	} while(n == 0);	/* another reader took the events first */

	if(copy_to_user(buf, evs, n * sizeof(struct ec11_event))) {
		return -EFAULT;
	}

	return n * sizeof(struct ec11_event);
}

static __poll_t device_poll(struct file *filp, poll_table *wait)
{
	struct ec11_dev *ec = filp->private_data;

	poll_wait(filp, &ec->wq, wait);

	return ec11_fifo_empty(ec) ? 0 : (EPOLLIN | EPOLLRDNORM);
}

static int device_open(struct inode *inode, struct file *filp)
{
	struct ec11_dev *ec = container_of(inode->i_cdev, struct ec11_dev, cdev);
	unsigned long flags;

	printk("ec11: device_open\n");
	filp->private_data = ec;

	/* drop stale events queued while nobody was reading */
	spin_lock_irqsave(&ec->lock, flags);
	kfifo_reset(&ec->events);
	spin_unlock_irqrestore(&ec->lock, flags);

	return 0;
}

static int device_release(struct inode *inode, struct file *filp)
{
	printk("ec11: device_release\n");
	return 0;
}

static const struct file_operations my_fops = {
	.owner = THIS_MODULE,
	.open = device_open,
	.release = device_release,
	.read = device_read,
	.poll = device_poll,
	.unlocked_ioctl = device_ioctl,
};

#if CONF_DT
static const struct of_device_id ec11_of_match[] = {
	{ .compatible = "smartfan,ec11", },
	{ },
};
MODULE_DEVICE_TABLE(of, ec11_of_match);
#else
static struct gpiod_lookup_table ec11_gpio_table = {
	.dev_id = DEVICE_NAME,
	.table = {
		GPIO_LOOKUP(GPIOCHIP_LABEL, GPIO_PIN_A, "a", GPIO_ACTIVE_HIGH),
		GPIO_LOOKUP(GPIOCHIP_LABEL, GPIO_PIN_B, "b", GPIO_ACTIVE_HIGH),
		GPIO_LOOKUP(GPIOCHIP_LABEL, GPIO_PIN_KEY, "key", GPIO_ACTIVE_LOW),
		{ },
	},
};

static struct platform_device *ec11_pdev;
#endif

static int ec11_get_gpio_irq(struct device *dev, const char *con_id,
			     struct gpio_desc **gpio, int *irq)
{
	*gpio = devm_gpiod_get(dev, con_id, GPIOD_IN);
	if (IS_ERR(*gpio)) {
		printk("ec11: can't get %s-gpios\n", con_id);
		return PTR_ERR(*gpio);
	}

	*irq = gpiod_to_irq(*gpio);
	if (*irq <= 0) {
		printk("ec11: can't find IRQ for %s-gpios\n", con_id);
		return *irq ? *irq : -ENODEV;
	}

	return 0;
}

static int driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ec11_dev *ec;
	struct device *cdev_device;
	int ret;

	printk("ec11: driver_probe\n");

	ec = devm_kzalloc(dev, sizeof(*ec), GFP_KERNEL);
	if (!ec)
		return -ENOMEM;

	spin_lock_init(&ec->lock);
	init_waitqueue_head(&ec->wq);
	INIT_KFIFO(ec->events);
	timer_setup(&ec->debounce_timer, ec11_debounce_timer_fn, 0);
	timer_setup(&ec->long_timer, ec11_long_timer_fn, 0);

	ret = ec11_get_gpio_irq(dev, "a", &ec->gpio_a, &ec->irq_a);
	if (ret)
		return ret;
	ret = ec11_get_gpio_irq(dev, "b", &ec->gpio_b, &ec->irq_b);
	if (ret)
		return ret;
	ret = ec11_get_gpio_irq(dev, "key", &ec->gpio_key, &ec->irq_key);
	if (ret)
		return ret;

	/* the encoder is resting at a detent when the driver is loaded */
	ec->ab_state = (gpiod_get_value(ec->gpio_a) << 1) | gpiod_get_value(ec->gpio_b);
	ec->rest_state = ec->ab_state;
	if (ec->rest_state == 0x1 || ec->rest_state == 0x2) {
		printk("ec11: A/B not at a detent (state = %d), assuming 3\n", ec->rest_state);
		ec->rest_state = 0x3;
	}
	ec->key_pressed = gpiod_get_value(ec->gpio_key);
	printk("ec11: initial A/B state = %d, key = %d\n", ec->ab_state, ec->key_pressed);

	/* char device: /dev/ec11 is created automatically */
	ret = alloc_chrdev_region(&ec->devt, 0, 1, DEVICE_NAME);
	if (ret < 0) {
		printk("ec11: can't allocate chrdev region\n");
		goto err0;
	}

	cdev_init(&ec->cdev, &my_fops);
	ec->cdev.owner = THIS_MODULE;
	ret = cdev_add(&ec->cdev, ec->devt, 1);
	if (ret) {
		printk("ec11: can't add device %d\n", ec->devt);
		goto err1;
	}

	ec->class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(ec->class)) {
		ret = PTR_ERR(ec->class);
		goto err2;
	}

	cdev_device = device_create(ec->class, dev, ec->devt, NULL, DEVICE_NAME);
	if (IS_ERR(cdev_device)) {
		ret = PTR_ERR(cdev_device);
		goto err3;
	}

	platform_set_drvdata(pdev, ec);

	/* key IRQ last: it arms the timers, so nothing can fail after it */
	ret = devm_request_irq(dev, ec->irq_a, ec11_rotary_isr,
			       IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "ec11_a", ec);
	if (ret) {
		printk("ec11: IRQ %d is not free\n", ec->irq_a);
		goto err4;
	}
	ret = devm_request_irq(dev, ec->irq_b, ec11_rotary_isr,
			       IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "ec11_b", ec);
	if (ret) {
		printk("ec11: IRQ %d is not free\n", ec->irq_b);
		goto err4;
	}
	ret = devm_request_irq(dev, ec->irq_key, ec11_key_isr,
			       IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "ec11_key", ec);
	if (ret) {
		printk("ec11: IRQ %d is not free\n", ec->irq_key);
		goto err4;
	}
	printk("ec11: IRQ a = %d, b = %d, key = %d enabled\n", ec->irq_a, ec->irq_b, ec->irq_key);

	return 0;

err4:
	device_destroy(ec->class, ec->devt);
err3:
	class_destroy(ec->class);
err2:
	cdev_del(&ec->cdev);
err1:
	unregister_chrdev_region(ec->devt, 1);
err0:
	return ret;
}

static int driver_remove(struct platform_device *pdev)
{
	struct ec11_dev *ec = platform_get_drvdata(pdev);

	printk("ec11: driver_remove\n");

	/* stop the IRQs before the timers so nothing re-arms them (IRQs are freed by devm) */
	disable_irq(ec->irq_key);
	disable_irq(ec->irq_a);
	disable_irq(ec->irq_b);
	del_timer_sync(&ec->debounce_timer);
	del_timer_sync(&ec->long_timer);

	device_destroy(ec->class, ec->devt);
	class_destroy(ec->class);
	cdev_del(&ec->cdev);
	unregister_chrdev_region(ec->devt, 1);

	return 0;
}

static struct platform_driver my_platform_driver = {
	.probe          = driver_probe,
	.remove         = driver_remove,
	.driver         = {
		.name   = DEVICE_NAME,
		.owner  = THIS_MODULE,
#if CONF_DT
		.of_match_table = ec11_of_match,
#endif
	},
};

static int __init platform_device_init(void)
{
	int ret;

	printk("ec11: platform_device_init\n");

	if (steps_per_detent != 1 && steps_per_detent != 2 && steps_per_detent != 4) {
		printk("ec11: steps_per_detent must be 1, 2 or 4\n");
		return -EINVAL;
	}

#if CONF_DT
	ret = platform_driver_register(&my_platform_driver);
	if(ret) {
		printk("platform_driver_register failed (ret=%d)\n", ret);
	}
#else
	gpiod_add_lookup_table(&ec11_gpio_table);

	ret = platform_driver_register(&my_platform_driver);
	if(ret) {
		gpiod_remove_lookup_table(&ec11_gpio_table);
		printk("platform_driver_register failed (ret=%d)\n", ret);
		return ret;
	}

	ec11_pdev = platform_device_register_simple(DEVICE_NAME, PLATFORM_DEVID_NONE, NULL, 0);
	if(IS_ERR(ec11_pdev)) {
		ret = PTR_ERR(ec11_pdev);
		platform_driver_unregister(&my_platform_driver);
		gpiod_remove_lookup_table(&ec11_gpio_table);
		printk("platform_device_register failed (ret=%d)\n", ret);
		return ret;
	}

	/* probe runs inside the registration above, a failed probe leaves the device unbound */
	if(!ec11_pdev->dev.driver) {
		printk("ec11: probe failed, see the messages above\n");
		platform_device_unregister(ec11_pdev);
		platform_driver_unregister(&my_platform_driver);
		gpiod_remove_lookup_table(&ec11_gpio_table);
		return -ENODEV;
	}
#endif

	return ret;
}

static void __exit platform_device_exit(void)
{
	printk("ec11: platform_device_exit\n");

#if CONF_DT
	platform_driver_unregister(&my_platform_driver);
#else
	platform_device_unregister(ec11_pdev);
	platform_driver_unregister(&my_platform_driver);
	gpiod_remove_lookup_table(&ec11_gpio_table);
#endif
}

module_init(platform_device_init);
module_exit(platform_device_exit);

MODULE_LICENSE("GPL");
