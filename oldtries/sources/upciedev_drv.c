#include <linux/module.h>

#include "pciedev_ufn.h"
#include "pciedev_io.h"

MODULE_AUTHOR("Ludwig Petrosyan");
MODULE_DESCRIPTION("DESY AMC-PCIE board driver");
MODULE_VERSION("4.5.0");
MODULE_LICENSE("Dual BSD/GPL");

static void __exit pciedev_cleanup_module(void)
{
    printk(KERN_NOTICE "UPCIEDEV_CLEANUP_MODULE CALLED\n");
}


static int __init pciedev_init_module(void)
{
    int   result  = 0;

	printk(KERN_ALERT "sizeof(pciedev_dev) = %d\n", (int)sizeof(struct pciedev_dev));
	return -1;
//#ifdef _ASM_X86_MODULE_H
//#endif
    printk(KERN_ALERT "UPCIEDEV_INIT:REGISTERING PCI DRIVER\n");
    return result; /* succeed */
}

module_init(pciedev_init_module);
module_exit(pciedev_cleanup_module);


