// SPDX-License-Identifier: GPL-2.0
/*
 * ledbar.c - 선풍기 프로젝트용 8칸 LED 바 그래프 드라이버
 *
 * 대상: Jetson Orin Nano (JetPack 6, Linux 5.15)
 *
 * - LED1(맨 아래) ~ LED8(맨 위)을 GPIO 8개로 제어
 * - /dev/ledbar : write(텍스트 명령) / read(상태) / ioctl(앱용)
 * - /sys/devices/platform/ledbar/{level,pattern,anim} : 시연/디버깅용
 * - 커널 타이머(timer_list)로 BLINK / WAVE / PROGRESS 애니메이션
 *
 * GPIO를 받는 방법은 두 가지:
 *   1) DT 오버레이  : compatible = "team,ledbar", led-gpios = <...>
 *   2) nodt=1 옵션  : 오버레이 없이 gpiochip 라인 번호로 바로 연결 (빠른 테스트용)
 *        sudo insmod ledbar.ko nodt=1
 *        sudo insmod ledbar.ko nodt=1 lines=50,122,126,125,123,124,52,51
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/machine.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/overflow.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/bits.h>
#include <linux/version.h>

#include "ledbar_ioctl.h"

#define DRV_NAME		"ledbar"

#define DEF_BLINK_MS		500
#define DEF_WAVE_MS		60
#define DEF_PROGRESS_MS		375	/* 8칸 x 375ms = 3초 (길게 누르기 표시용) */
#define MIN_PERIOD_MS		10
#define MAX_PERIOD_MS		5000

/* ---- 커널 버전 호환 (JetPack 6 = 5.15 기준, 최신 커널도 빌드되게) ---- */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 16, 0)
#define ledbar_from_timer(var, t, field)	timer_container_of(var, t, field)
#else
#define ledbar_from_timer(var, t, field)	from_timer(var, t, field)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0)
#define timer_delete_sync(t)	del_timer_sync(t)
#endif

struct ledbar {
	struct device		*dev;
	struct gpio_descs	*gpios;
	unsigned int		n;		/* LED 개수 (1~8) */
	u8			mask;		/* n개 LED에 해당하는 비트마스크 */

	spinlock_t		lock;		/* 아래 상태 + GPIO 출력 보호 */
	struct timer_list	timer;
	struct miscdevice	misc;

	int			level;		/* 마지막 레벨, 패턴 직접 설정 시 -1 */
	u8			pattern;	/* 기본 표시 패턴 */
	u8			shown;		/* 실제로 켜진 패턴 */
	int			anim;
	unsigned int		period_ms;
	unsigned int		step;
	bool			blink_on;
};

static const char * const anim_names[] = {
	[LEDBAR_ANIM_NONE]	= "none",
	[LEDBAR_ANIM_BLINK]	= "blink",
	[LEDBAR_ANIM_WAVE]	= "wave",
	[LEDBAR_ANIM_PROGRESS]	= "progress",
};

static int anim_from_name(const char *s)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(anim_names); i++)
		if (sysfs_streq(s, anim_names[i]))
			return i;
	if (sysfs_streq(s, "stop") || sysfs_streq(s, "off"))
		return LEDBAR_ANIM_NONE;
	return -EINVAL;
}

/* 레벨 N -> 아래부터 N칸 켜는 패턴 */
static u8 level_to_pattern(unsigned int level)
{
	return level ? (u8)GENMASK(level - 1, 0) : 0;
}

/* lock을 잡은 상태에서 호출 */
static void ledbar_show(struct ledbar *lb, u8 pat)
{
	unsigned long bits = pat & lb->mask;

	lb->shown = bits;
	gpiod_set_array_value(lb->gpios->ndescs, lb->gpios->desc,
			      lb->gpios->info, &bits);
}

static void ledbar_arm(struct ledbar *lb)
{
	mod_timer(&lb->timer, jiffies + msecs_to_jiffies(lb->period_ms));
}

static void ledbar_timer_fn(struct timer_list *t)
{
	struct ledbar *lb = ledbar_from_timer(lb, t, timer);
	unsigned long flags;
	bool again = true;

	spin_lock_irqsave(&lb->lock, flags);

	switch (lb->anim) {
	case LEDBAR_ANIM_BLINK:
		lb->blink_on = !lb->blink_on;
		ledbar_show(lb, lb->blink_on ? lb->pattern : 0);
		break;

	case LEDBAR_ANIM_WAVE:
		/* step 1..n: 채우기, n+1..2n: 비우기, 끝나면 원래 패턴 복귀 */
		lb->step++;
		if (lb->step <= lb->n) {
			ledbar_show(lb, level_to_pattern(lb->step));
		} else if (lb->step <= 2 * lb->n) {
			ledbar_show(lb, level_to_pattern(2 * lb->n - lb->step));
		} else {
			lb->anim = LEDBAR_ANIM_NONE;
			ledbar_show(lb, lb->pattern);
			again = false;
		}
		break;

	case LEDBAR_ANIM_PROGRESS:
		if (lb->step < lb->n) {
			lb->step++;
			ledbar_show(lb, level_to_pattern(lb->step));
		}
		/* 다 차면 그 상태로 유지, 앱이 다음 명령을 줄 때까지 */
		if (lb->step >= lb->n)
			again = false;
		break;

	default:
		again = false;
		break;
	}

	if (again)
		ledbar_arm(lb);

	spin_unlock_irqrestore(&lb->lock, flags);
}

/* 기본 패턴 변경. WAVE/PROGRESS는 중단, BLINK는 새 패턴으로 계속 */
static void ledbar_set_pattern(struct ledbar *lb, u8 pat, int level)
{
	unsigned long flags;

	spin_lock_irqsave(&lb->lock, flags);
	lb->pattern = pat & lb->mask;
	lb->level = level;

	if (lb->anim == LEDBAR_ANIM_WAVE || lb->anim == LEDBAR_ANIM_PROGRESS)
		lb->anim = LEDBAR_ANIM_NONE;

	if (lb->anim != LEDBAR_ANIM_BLINK || lb->blink_on)
		ledbar_show(lb, lb->pattern);
	spin_unlock_irqrestore(&lb->lock, flags);
}

static int ledbar_set_level(struct ledbar *lb, int level)
{
	if (level < 0 || level > lb->n)
		return -EINVAL;
	ledbar_set_pattern(lb, level_to_pattern(level), level);
	return 0;
}

static int ledbar_set_anim(struct ledbar *lb, int anim, unsigned int period_ms)
{
	unsigned long flags;

	if (anim < 0 || anim >= ARRAY_SIZE(anim_names))
		return -EINVAL;

	if (!period_ms) {
		switch (anim) {
		case LEDBAR_ANIM_BLINK:		period_ms = DEF_BLINK_MS; break;
		case LEDBAR_ANIM_WAVE:		period_ms = DEF_WAVE_MS; break;
		case LEDBAR_ANIM_PROGRESS:	period_ms = DEF_PROGRESS_MS; break;
		default:			break;
		}
	} else {
		period_ms = clamp_t(unsigned int, period_ms,
				    MIN_PERIOD_MS, MAX_PERIOD_MS);
	}

	spin_lock_irqsave(&lb->lock, flags);
	lb->anim = anim;
	lb->period_ms = period_ms;
	lb->step = 0;

	switch (anim) {
	case LEDBAR_ANIM_NONE:
		/* 대기 중인 타이머가 있어도 콜백에서 NONE을 보고 그냥 끝난다 */
		ledbar_show(lb, lb->pattern);
		break;
	case LEDBAR_ANIM_BLINK:
		lb->blink_on = true;
		ledbar_show(lb, lb->pattern);
		ledbar_arm(lb);
		break;
	case LEDBAR_ANIM_WAVE:
	case LEDBAR_ANIM_PROGRESS:
		ledbar_show(lb, 0);
		ledbar_arm(lb);
		break;
	}
	spin_unlock_irqrestore(&lb->lock, flags);
	return 0;
}

static void ledbar_get_state(struct ledbar *lb, struct ledbar_state *st)
{
	unsigned long flags;

	memset(st, 0, sizeof(*st));
	spin_lock_irqsave(&lb->lock, flags);
	st->level = lb->level;
	st->pattern = lb->pattern;
	st->shown = lb->shown;
	st->nleds = lb->n;
	st->anim = lb->anim;
	st->period_ms = lb->period_ms;
	spin_unlock_irqrestore(&lb->lock, flags);
}

/* 모든 출력 끄고 애니메이션 정지 (remove / shutdown) */
static void ledbar_all_off(struct ledbar *lb)
{
	unsigned long flags;

	spin_lock_irqsave(&lb->lock, flags);
	lb->anim = LEDBAR_ANIM_NONE;
	spin_unlock_irqrestore(&lb->lock, flags);

	timer_delete_sync(&lb->timer);

	spin_lock_irqsave(&lb->lock, flags);
	lb->pattern = 0;
	lb->level = 0;
	ledbar_show(lb, 0);
	spin_unlock_irqrestore(&lb->lock, flags);
}

/* ------------------------------------------------------------------ */
/* /dev/ledbar                                                         */
/* ------------------------------------------------------------------ */

static struct ledbar *file_to_ledbar(struct file *file)
{
	/* misc_open()이 private_data에 miscdevice 포인터를 넣어준다 */
	return container_of(file->private_data, struct ledbar, misc);
}

/*
 * 텍스트 명령:
 *   "0" ~ "8"            레벨
 *   "level N"            레벨
 *   "p 0xA5" / "pattern 0xA5"
 *   "blink [ms]" / "wave [ms]" / "progress [ms]"
 *   "stop" / "none" / "off"
 */
static ssize_t ledbar_write(struct file *file, const char __user *ubuf,
			    size_t count, loff_t *ppos)
{
	struct ledbar *lb = file_to_ledbar(file);
	char buf[32], cmd[16];
	size_t len = min(count, sizeof(buf) - 1);
	int arg = 0, nargs, anim, ret;
	char *s;

	if (copy_from_user(buf, ubuf, len))
		return -EFAULT;
	buf[len] = '\0';
	s = strim(buf);

	nargs = sscanf(s, "%15s %i", cmd, &arg);
	if (nargs < 1)
		return -EINVAL;

	if (!strcmp(cmd, "level")) {
		ret = nargs == 2 ? ledbar_set_level(lb, arg) : -EINVAL;
	} else if (!strcmp(cmd, "p") || !strcmp(cmd, "pattern")) {
		if (nargs != 2 || arg < 0 || arg > 0xff)
			ret = -EINVAL;
		else {
			ledbar_set_pattern(lb, arg, -1);
			ret = 0;
		}
	} else if ((anim = anim_from_name(cmd)) >= 0) {
		ret = ledbar_set_anim(lb, anim,
				      nargs == 2 && arg > 0 ? arg : 0);
	} else {
		int level;

		ret = kstrtoint(cmd, 10, &level);
		if (!ret)
			ret = ledbar_set_level(lb, level);
	}

	return ret ? ret : count;
}

static ssize_t ledbar_read(struct file *file, char __user *ubuf,
			   size_t count, loff_t *ppos)
{
	struct ledbar *lb = file_to_ledbar(file);
	struct ledbar_state st;
	char buf[128];
	int len;

	ledbar_get_state(lb, &st);
	len = scnprintf(buf, sizeof(buf),
			"level=%d pattern=0x%02x shown=0x%02x anim=%s period_ms=%u nleds=%u\n",
			st.level, st.pattern, st.shown, anim_names[st.anim],
			st.period_ms, st.nleds);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static long ledbar_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct ledbar *lb = file_to_ledbar(file);
	void __user *uarg = (void __user *)arg;

	switch (cmd) {
	case LEDBAR_SET_LEVEL: {
		__s32 level;

		if (get_user(level, (__s32 __user *)uarg))
			return -EFAULT;
		return ledbar_set_level(lb, level);
	}
	case LEDBAR_SET_PATTERN: {
		__u8 pat;

		if (get_user(pat, (__u8 __user *)uarg))
			return -EFAULT;
		ledbar_set_pattern(lb, pat, -1);
		return 0;
	}
	case LEDBAR_SET_ANIM: {
		struct ledbar_anim a;

		if (copy_from_user(&a, uarg, sizeof(a)))
			return -EFAULT;
		return ledbar_set_anim(lb, a.anim, a.period_ms);
	}
	case LEDBAR_GET_STATE: {
		struct ledbar_state st;

		ledbar_get_state(lb, &st);
		if (copy_to_user(uarg, &st, sizeof(st)))
			return -EFAULT;
		return 0;
	}
	default:
		return -ENOTTY;
	}
}

static const struct file_operations ledbar_fops = {
	.owner		= THIS_MODULE,
	.write		= ledbar_write,
	.read		= ledbar_read,
	.unlocked_ioctl	= ledbar_ioctl,
	.compat_ioctl	= compat_ptr_ioctl,
};

/* ------------------------------------------------------------------ */
/* sysfs: /sys/devices/platform/ledbar/{level,pattern,anim}           */
/* ------------------------------------------------------------------ */

static ssize_t level_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct ledbar_state st;

	ledbar_get_state(dev_get_drvdata(dev), &st);
	return sysfs_emit(buf, "%d\n", st.level);
}

static ssize_t level_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	int level, ret;

	ret = kstrtoint(buf, 10, &level);
	if (ret)
		return ret;
	ret = ledbar_set_level(dev_get_drvdata(dev), level);
	return ret ? ret : count;
}
static DEVICE_ATTR_RW(level);

static ssize_t pattern_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct ledbar_state st;

	ledbar_get_state(dev_get_drvdata(dev), &st);
	return sysfs_emit(buf, "0x%02x\n", st.pattern);
}

static ssize_t pattern_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	unsigned int pat;
	int ret;

	ret = kstrtouint(buf, 0, &pat);
	if (ret)
		return ret;
	if (pat > 0xff)
		return -EINVAL;
	ledbar_set_pattern(dev_get_drvdata(dev), pat, -1);
	return count;
}
static DEVICE_ATTR_RW(pattern);

static ssize_t anim_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct ledbar_state st;

	ledbar_get_state(dev_get_drvdata(dev), &st);
	return sysfs_emit(buf, "%s\n", anim_names[st.anim]);
}

static ssize_t anim_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	int anim, ret;

	anim = anim_from_name(buf);
	if (anim < 0)
		return anim;
	ret = ledbar_set_anim(dev_get_drvdata(dev), anim, 0);
	return ret ? ret : count;
}
static DEVICE_ATTR_RW(anim);

static struct attribute *ledbar_attrs[] = {
	&dev_attr_level.attr,
	&dev_attr_pattern.attr,
	&dev_attr_anim.attr,
	NULL,
};
ATTRIBUTE_GROUPS(ledbar);

/* ------------------------------------------------------------------ */
/* platform driver                                                     */
/* ------------------------------------------------------------------ */

static int ledbar_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ledbar *lb;
	unsigned int i;
	int ret;

	lb = devm_kzalloc(dev, sizeof(*lb), GFP_KERNEL);
	if (!lb)
		return -ENOMEM;
	lb->dev = dev;

	/* DT의 led-gpios 또는 nodt 모드의 lookup table에서 "led" GPIO 배열을 받는다 */
	lb->gpios = devm_gpiod_get_array(dev, "led", GPIOD_OUT_LOW);
	if (IS_ERR(lb->gpios))
		return dev_err_probe(dev, PTR_ERR(lb->gpios),
				     "led-gpios를 가져오지 못함\n");

	lb->n = lb->gpios->ndescs;
	if (lb->n < 1 || lb->n > LEDBAR_MAX_LEDS) {
		dev_err(dev, "LED 개수가 잘못됨: %u (1~%d)\n",
			lb->n, LEDBAR_MAX_LEDS);
		return -EINVAL;
	}
	lb->mask = level_to_pattern(lb->n);

	/* 타이머 콜백(softirq)에서 GPIO를 건드리므로 sleep 하는 GPIO는 쓸 수 없다 */
	for (i = 0; i < lb->n; i++) {
		if (gpiod_cansleep(lb->gpios->desc[i])) {
			dev_err(dev, "LED%u GPIO가 sleep 가능한 컨트롤러에 있음 (지원 안 함)\n",
				i + 1);
			return -EINVAL;
		}
	}

	spin_lock_init(&lb->lock);
	timer_setup(&lb->timer, ledbar_timer_fn, 0);
	lb->anim = LEDBAR_ANIM_NONE;
	lb->level = 0;

	platform_set_drvdata(pdev, lb);

	lb->misc.minor = MISC_DYNAMIC_MINOR;
	lb->misc.name = DRV_NAME;
	lb->misc.fops = &ledbar_fops;
	lb->misc.parent = dev;
	lb->misc.mode = 0666;	/* 일반 사용자 앱에서도 열 수 있게 */

	ret = misc_register(&lb->misc);
	if (ret) {
		dev_err(dev, "misc_register 실패: %d\n", ret);
		return ret;
	}

	dev_info(dev, "LED 바 %u칸 준비 완료 (/dev/%s)\n", lb->n, DRV_NAME);
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void ledbar_remove(struct platform_device *pdev)
#else
static int ledbar_remove(struct platform_device *pdev)
#endif
{
	struct ledbar *lb = platform_get_drvdata(pdev);

	misc_deregister(&lb->misc);
	ledbar_all_off(lb);
	dev_info(&pdev->dev, "LED 바 해제\n");
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 11, 0)
	return 0;
#endif
}

static void ledbar_shutdown(struct platform_device *pdev)
{
	struct ledbar *lb = platform_get_drvdata(pdev);

	if (lb)
		ledbar_all_off(lb);
}

static const struct of_device_id ledbar_of_match[] = {
	{ .compatible = "team,ledbar" },
	{ }
};
MODULE_DEVICE_TABLE(of, ledbar_of_match);

static struct platform_driver ledbar_driver = {
	.probe		= ledbar_probe,
	.remove		= ledbar_remove,
	.shutdown	= ledbar_shutdown,
	.driver		= {
		.name		= DRV_NAME,
		.of_match_table	= ledbar_of_match,
		.dev_groups	= ledbar_groups,
	},
};

/* ------------------------------------------------------------------ */
/* nodt 모드: DT 오버레이 없이 바로 테스트                               */
/* ------------------------------------------------------------------ */

static bool nodt;
module_param(nodt, bool, 0444);
MODULE_PARM_DESC(nodt, "1이면 DT 오버레이 없이 lines= 라인 번호로 장치를 직접 만든다");

static char *chip = "tegra234-gpio";
module_param(chip, charp, 0444);
MODULE_PARM_DESC(chip, "gpiochip 라벨 (gpiodetect 로 확인, 기본 tegra234-gpio)");

/*
 * LED1(아래) ~ LED8(위) = 헤더 핀 12, 13, 16, 18, 22, 37, 38, 40
 *   12 PH.07=50  13 PY.00=122  16 PY.04=126  18 PY.03=125
 *   22 PY.01=123 37 PY.02=124  38 PI.01=52   40 PI.00=51
 * 반드시 `sudo gpioinfo gpiochip0` 의 라인 이름(PH.07 등)과 대조할 것.
 */
static unsigned int lines[LEDBAR_MAX_LEDS] = { 50, 122, 126, 125, 123, 124, 52, 51 };
static int nlines = LEDBAR_MAX_LEDS;
module_param_array(lines, uint, &nlines, 0444);
MODULE_PARM_DESC(lines, "nodt 모드에서 쓸 gpiochip 라인 번호 (LED1부터 순서대로)");

static struct gpiod_lookup_table *nodt_table;
static struct platform_device *nodt_pdev;

static int nodt_create(void)
{
	int i;

	if (nlines < 1 || nlines > LEDBAR_MAX_LEDS)
		return -EINVAL;

	nodt_table = kzalloc(struct_size(nodt_table, table, nlines + 1),
			     GFP_KERNEL);
	if (!nodt_table)
		return -ENOMEM;

	nodt_table->dev_id = DRV_NAME;	/* platform device 이름과 같아야 함 */
	for (i = 0; i < nlines; i++)
		nodt_table->table[i] = (struct gpiod_lookup)
			GPIO_LOOKUP_IDX(chip, lines[i], "led", i, GPIO_ACTIVE_HIGH);
	gpiod_add_lookup_table(nodt_table);

	nodt_pdev = platform_device_register_simple(DRV_NAME, PLATFORM_DEVID_NONE,
						    NULL, 0);
	if (IS_ERR(nodt_pdev)) {
		int ret = PTR_ERR(nodt_pdev);

		nodt_pdev = NULL;
		gpiod_remove_lookup_table(nodt_table);
		kfree(nodt_table);
		nodt_table = NULL;
		return ret;
	}
	return 0;
}

static void nodt_destroy(void)
{
	if (nodt_pdev)
		platform_device_unregister(nodt_pdev);
	if (nodt_table) {
		gpiod_remove_lookup_table(nodt_table);
		kfree(nodt_table);
	}
}

static int __init ledbar_init(void)
{
	int ret;

	ret = platform_driver_register(&ledbar_driver);
	if (ret || !nodt)
		return ret;

	ret = nodt_create();
	if (ret) {
		pr_err(DRV_NAME ": nodt 장치 생성 실패: %d\n", ret);
		platform_driver_unregister(&ledbar_driver);
	}
	return ret;
}

static void __exit ledbar_exit(void)
{
	nodt_destroy();
	platform_driver_unregister(&ledbar_driver);
}

module_init(ledbar_init);
module_exit(ledbar_exit);

MODULE_AUTHOR("Fan project team");
MODULE_DESCRIPTION("8-segment LED bar graph driver (Jetson Orin Nano)");
MODULE_LICENSE("GPL");
