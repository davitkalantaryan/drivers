#include <linux/module.h>

#include "pciedev_ufn.h"
#include "pciedev_io.h"

MODULE_AUTHOR("Ludwig Petrosyan, David Kalantaryan");
MODULE_DESCRIPTION("DESY PCIE universal driver");
MODULE_VERSION("5.1.0");
MODULE_LICENSE("Dual BSD/GPL");

int g_nPrintDebugInfo = 0;
#ifndef WIN32
module_param_named(debug, g_nPrintDebugInfo, int, S_IRUGO | S_IWUSR);
EXPORT_SYMBOL(g_nPrintDebugInfo);
#endif

static void __exit pciedev_cleanup_module(void)
{
	printk(KERN_NOTICE "UPCIEDEV_CLEANUP_MODULE CALLED\n");
}

#include "debug_functions.h"

void UpciedevTestFunction(const char* a_string)
{
	NOTICERT("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!%s\n", a_string);
}
EXPORT_SYMBOL(UpciedevTestFunction);

static int __init pciedev_init_module(void)
{
	int   result = 0;

#ifdef CONFIG_PRINTK
	//ALERTRT("CONFIG_PRINTK defined!!!!!!!!!!!!!!!!\n");
#endif
	UpciedevTestFunction("UPCIEDEV");
	printk(KERN_ALERT "UPCIEDEV_INIT:REGISTERING PCI DRIVER sizeof(loff_t)=%d, __USER_CS=0x%x\n", (int)sizeof(loff_t), (int)__USER_CS);
	return result; /* succeed */
}

module_init(pciedev_init_module);
module_exit(pciedev_cleanup_module);
