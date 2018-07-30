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
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xa8c16cf3, "module_layout" },
	{ 0x1d26dbf2, "cdev_del" },
	{ 0x640a934, "pci_bus_read_config_byte" },
	{ 0x1606bd6d, "cdev_init" },
	{ 0x4c4fef19, "kernel_stack" },
	{ 0xc5ac94d0, "dev_set_drvdata" },
	{ 0x43a53735, "__alloc_workqueue_key" },
	{ 0x2ea9ad56, "kill_pid_info_as_cred" },
	{ 0x255066a5, "find_vpid" },
	{ 0x1f3ca42, "pci_disable_device" },
	{ 0x6f2e6f37, "remove_proc_entry" },
	{ 0x152aeb08, "device_destroy" },
	{ 0x26576139, "queue_work" },
	{ 0x632faaec, "pci_release_regions" },
	{ 0x9297e8ae, "mutex_unlock" },
	{ 0x7485e15e, "unregister_chrdev_region" },
	{ 0x91715312, "sprintf" },
	{ 0x4f8b5ddb, "_copy_to_user" },
	{ 0xe28af57f, "pci_set_master" },
	{ 0xf10de535, "ioread8" },
	{ 0x96b92e3e, "pci_iounmap" },
	{ 0x571ab46f, "current_task" },
	{ 0xbb41cdec, "mutex_lock_interruptible" },
	{ 0x3e18b1c5, "__mutex_init" },
	{ 0x27e1a049, "printk" },
	{ 0xa1c76e0a, "_cond_resched" },
	{ 0xb4390f9a, "mcount" },
	{ 0x6a517590, "device_create" },
	{ 0x55666903, "pid_task" },
	{ 0x2072ee9b, "request_threaded_irq" },
	{ 0xc6fa2947, "pci_find_capability" },
	{ 0xc4556ebd, "cdev_add" },
	{ 0xe660bd22, "pci_bus_read_config_word" },
	{ 0x5acc377, "pci_bus_read_config_dword" },
	{ 0x727c4f3, "iowrite8" },
	{ 0x77ba54e8, "create_proc_entry" },
	{ 0x8c183cbe, "iowrite16" },
	{ 0xc8450627, "pci_request_regions" },
	{ 0x940df3fb, "class_destroy" },
	{ 0xc5534d64, "ioread16" },
	{ 0xecbec80a, "pci_get_device" },
	{ 0xb0e602eb, "memmove" },
	{ 0xa5f09d33, "pci_iomap" },
	{ 0x436c2179, "iowrite32" },
	{ 0xe3e5d565, "pci_enable_device" },
	{ 0x4f6b400b, "_copy_from_user" },
	{ 0x8037428e, "__class_create" },
	{ 0x29537c9e, "alloc_chrdev_region" },
	{ 0xe484e35f, "ioread32" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "365C556652D4FC8DEF4402A");
