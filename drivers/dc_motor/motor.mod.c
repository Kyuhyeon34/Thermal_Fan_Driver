#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x5063314d, "module_layout" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x7d4e4e48, "cdev_del" },
	{ 0xd5920837, "pwm_free" },
	{ 0xfe990052, "gpio_free" },
	{ 0x55b5ce0f, "pwm_request" },
	{ 0xd4d50f84, "gpiod_direction_output_raw" },
	{ 0x47229b5c, "gpio_request" },
	{ 0x2b25d7a3, "cdev_add" },
	{ 0xb66ffcd7, "cdev_init" },
	{ 0x3fd78f3b, "register_chrdev_region" },
	{ 0x1e49e99f, "cpu_hwcaps" },
	{ 0x12a4e128, "__arch_copy_from_user" },
	{ 0x31909811, "cpu_hwcap_keys" },
	{ 0x14b89635, "arm64_const_caps_ready" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0x4dfa8d4b, "mutex_lock" },
	{ 0x3ea1b6e4, "__stack_chk_fail" },
	{ 0xc098597f, "pwm_apply_state" },
	{ 0xf193393, "gpiod_set_raw_value" },
	{ 0x9a550c00, "gpio_to_desc" },
	{ 0x4b0a3f52, "gic_nonsecure_priorities" },
	{ 0x92997ed8, "_printk" },
	{ 0x1fdc7df2, "_mcount" },
};

MODULE_INFO(depends, "");

