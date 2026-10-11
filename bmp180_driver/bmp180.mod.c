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
	{ 0xffe4cb28, "i2c_del_driver" },
	{ 0x45dad54b, "i2c_register_driver" },
	{ 0xc3055d20, "usleep_range_state" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0x4dfa8d4b, "mutex_lock" },
	{ 0x757ab9ec, "__devm_iio_device_register" },
	{ 0x6881d240, "_dev_info" },
	{ 0x6188fcf, "_dev_err" },
	{ 0xcefb0c9f, "__mutex_init" },
	{ 0x7039b05d, "devm_iio_device_alloc" },
	{ 0x3ea1b6e4, "__stack_chk_fail" },
	{ 0x84ace6f0, "i2c_transfer" },
	{ 0x1fdc7df2, "_mcount" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Ccustom,bmp180");
MODULE_ALIAS("of:N*T*Ccustom,bmp180C*");
MODULE_ALIAS("i2c:bmp180_custom");

MODULE_INFO(srcversion, "F1831478BC94E47FC10FA27");
