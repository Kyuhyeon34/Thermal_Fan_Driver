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
	{ 0x8fded1ef, "param_ops_bool" },
	{ 0xb4ba4bcd, "param_ops_int" },
	{ 0xf9b9c6b5, "platform_device_unregister" },
	{ 0x46d3d6e4, "platform_driver_unregister" },
	{ 0x80abe1d6, "platform_device_register_full" },
	{ 0xdcb764ad, "memset" },
	{ 0xff81487d, "gpiod_remove_lookup_table" },
	{ 0xa955d91, "__platform_driver_register" },
	{ 0x38aa1397, "gpiod_add_lookup_table" },
	{ 0x56470118, "__warn_printk" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x92540fbf, "finish_wait" },
	{ 0x8c26d495, "prepare_to_wait_event" },
	{ 0x1000e51, "schedule" },
	{ 0xfe487975, "init_wait_entry" },
	{ 0x13d0adf7, "__kfifo_out" },
	{ 0x3ea1b6e4, "__stack_chk_fail" },
	{ 0x6cbbfc54, "__arch_copy_to_user" },
	{ 0x1e49e99f, "cpu_hwcaps" },
	{ 0x31909811, "cpu_hwcap_keys" },
	{ 0x14b89635, "arm64_const_caps_ready" },
	{ 0x9fcec377, "devm_request_threaded_irq" },
	{ 0x257b3dd1, "device_create" },
	{ 0xc75c9cad, "__class_create" },
	{ 0x2b25d7a3, "cdev_add" },
	{ 0xb66ffcd7, "cdev_init" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0xc6f46339, "init_timer_key" },
	{ 0xd9a5ea54, "__init_waitqueue_head" },
	{ 0xd8d38480, "devm_kmalloc" },
	{ 0x5b04e4a6, "gpiod_to_irq" },
	{ 0xf078870e, "devm_gpiod_get" },
	{ 0xdc0e4855, "timer_delete" },
	{ 0x4b0a3f52, "gic_nonsecure_priorities" },
	{ 0xdd763ff8, "gpiod_get_value" },
	{ 0x3eeb2322, "__wake_up" },
	{ 0x1d24c881, "___ratelimit" },
	{ 0xc38c83b8, "mod_timer" },
	{ 0x15ba50a6, "jiffies" },
	{ 0x7f02188f, "__msecs_to_jiffies" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x7d4e4e48, "cdev_del" },
	{ 0x4720199c, "class_destroy" },
	{ 0xc21fdfb0, "device_destroy" },
	{ 0x82ee90dc, "timer_delete_sync" },
	{ 0x3ce4ca6f, "disable_irq" },
	{ 0x92997ed8, "_printk" },
	{ 0xd35cce70, "_raw_spin_unlock_irqrestore" },
	{ 0x34db050b, "_raw_spin_lock_irqsave" },
	{ 0x1fdc7df2, "_mcount" },
};

MODULE_INFO(depends, "");

