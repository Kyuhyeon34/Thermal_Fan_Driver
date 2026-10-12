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
	{ 0xecf645d5, "param_ops_charp" },
	{ 0x91053684, "param_array_ops" },
	{ 0xfbac37cf, "param_ops_uint" },
	{ 0x8eba4c55, "compat_ptr_ioctl" },
	{ 0xf9b9c6b5, "platform_device_unregister" },
	{ 0x46d3d6e4, "platform_driver_unregister" },
	{ 0x92997ed8, "_printk" },
	{ 0x37a0cba, "kfree" },
	{ 0xff81487d, "gpiod_remove_lookup_table" },
	{ 0x80abe1d6, "platform_device_register_full" },
	{ 0x38aa1397, "gpiod_add_lookup_table" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0xa955d91, "__platform_driver_register" },
	{ 0x6cbbfc54, "__arch_copy_to_user" },
	{ 0xbcab6ee6, "sscanf" },
	{ 0x77bc13a0, "strim" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0xc38c83b8, "mod_timer" },
	{ 0x15ba50a6, "jiffies" },
	{ 0x7f02188f, "__msecs_to_jiffies" },
	{ 0x8c8569cb, "kstrtoint" },
	{ 0x3b6c41ea, "kstrtouint" },
	{ 0x287f6da0, "misc_deregister" },
	{ 0x266bace9, "gpiod_set_array_value" },
	{ 0x82ee90dc, "timer_delete_sync" },
	{ 0xdcb764ad, "memset" },
	{ 0x12a4e128, "__arch_copy_from_user" },
	{ 0x1e49e99f, "cpu_hwcaps" },
	{ 0x31909811, "cpu_hwcap_keys" },
	{ 0x14b89635, "arm64_const_caps_ready" },
	{ 0x6881d240, "_dev_info" },
	{ 0x29c1c1c4, "dev_err_probe" },
	{ 0x6188fcf, "_dev_err" },
	{ 0xac49e8e8, "misc_register" },
	{ 0xc6f46339, "init_timer_key" },
	{ 0x70006287, "gpiod_cansleep" },
	{ 0x701f0bed, "devm_gpiod_get_array" },
	{ 0xd8d38480, "devm_kmalloc" },
	{ 0x619cb7dd, "simple_read_from_buffer" },
	{ 0x96848186, "scnprintf" },
	{ 0x4b0a3f52, "gic_nonsecure_priorities" },
	{ 0x3ea1b6e4, "__stack_chk_fail" },
	{ 0xe783e261, "sysfs_emit" },
	{ 0xd35cce70, "_raw_spin_unlock_irqrestore" },
	{ 0x34db050b, "_raw_spin_lock_irqsave" },
	{ 0x222e7ce2, "sysfs_streq" },
	{ 0x1fdc7df2, "_mcount" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("of:N*T*Cteam,ledbar");
MODULE_ALIAS("of:N*T*Cteam,ledbarC*");
