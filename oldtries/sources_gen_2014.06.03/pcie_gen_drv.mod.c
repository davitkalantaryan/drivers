#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
};

static const struct modversion_info ____versions[]
__attribute_used__
__attribute__((section("__versions"))) = {
	{ 0xf8e3dbd2, "struct_module" },
	{ 0x4f7f1c1a, "cdev_del" },
	{ 0x8b8fd95a, "pci_bus_read_config_byte" },
	{ 0x894c0c4a, "cdev_init" },
	{ 0x36249e28, "pci_disable_device" },
	{ 0x39280472, "remove_proc_entry" },
	{ 0xc978b2b, "device_destroy" },
	{ 0x882efd23, "queue_work" },
	{ 0xd20f583f, "pci_release_regions" },
	{ 0xa464d8ff, "mutex_unlock" },
	{ 0x7485e15e, "unregister_chrdev_region" },
	{ 0x1d26aa98, "sprintf" },
	{ 0xa409da7b, "__create_workqueue" },
	{ 0x490ef5ac, "pci_set_master" },
	{ 0xde0bdcff, "memset" },
	{ 0xf10de535, "ioread8" },
	{ 0xa8f21b0e, "pci_iounmap" },
	{ 0x666cddb1, "mutex_lock_interruptible" },
	{ 0xc16fe12d, "__memcpy" },
	{ 0x86cb9d9f, "__mutex_init" },
	{ 0xdd132261, "printk" },
	{ 0xbe499d81, "copy_to_user" },
	{ 0x30602cac, "destroy_workqueue" },
	{ 0x9af21eb9, "class_create" },
	{ 0x755ac58e, "device_create" },
	{ 0x6725bbd3, "flush_workqueue" },
	{ 0xcad4e8d6, "pci_find_capability" },
	{ 0x5777894f, "cdev_add" },
	{ 0xb11e2cf, "pci_bus_read_config_word" },
	{ 0x65b75503, "pci_bus_read_config_dword" },
	{ 0x5271af5d, "request_irq" },
	{ 0x727c4f3, "iowrite8" },
	{ 0xa1679700, "create_proc_entry" },
	{ 0xa5c89579, "init_timer" },
	{ 0x8c183cbe, "iowrite16" },
	{ 0x4c5b4b9f, "pci_request_regions" },
	{ 0xd3e6b205, "class_destroy" },
	{ 0xc5534d64, "ioread16" },
	{ 0xb04b777a, "pci_get_device" },
	{ 0xa3a5be95, "memmove" },
	{ 0xde2b3eff, "pci_iomap" },
	{ 0x436c2179, "iowrite32" },
	{ 0x399280fc, "pci_enable_device" },
	{ 0x945bc6a7, "copy_from_user" },
	{ 0x29537c9e, "alloc_chrdev_region" },
	{ 0xe484e35f, "ioread32" },
	{ 0xf20dabd8, "free_irq" },
};

static const char __module_depends[]
__attribute_used__
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "D98DE7052280CBD13CA73BF");
