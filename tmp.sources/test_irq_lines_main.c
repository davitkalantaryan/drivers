/*
 *	File: test_irq_main.c
 *
 *	Created on: Jul 17, 2015
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *
 */

#define SIS8300DEVNAME			"irq_line_test"	                    /* name of device */

#include <linux/module.h>
#include <linux/delay.h>
#include "mtcagen_exp.h"
#include "debug_functions.h"

#ifndef WIN32
MODULE_AUTHOR("David Kalantaryan");
MODULE_DESCRIPTION("Driver for testing irq lines");
MODULE_VERSION("1.1.0");
MODULE_LICENSE("Dual BSD/GPL");
#endif



static int ProbeFunction(struct pciedev_dev* a_dev, void* a_pData)
{
	struct pci_dev* pcidev = a_dev->pciedev_pci_dev;

	printk(KERN_ALERT "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!   slot=%d, irq_line=%d\n", 
		(int)a_dev->slot_num, (int)pcidev->irq);

	return Mtcagen_GainAccess_exp(a_dev, 0, NULL, &g_default_mtcagen_fops_exp,
		SIS8300DEVNAME, "%ss%d", SIS8300DEVNAME, a_dev->brd_num);
}


static void RemoveFunction(struct pciedev_dev* a_dev, void* a_pData)
{
	ALERTCT("a_pData=%p\n", a_pData);
	Mtcagen_remove_exp(a_dev, 0, NULL);
}



static void __exit sis8300_cleanup_module(void)
{
	SDeviceParams aDevParam;
	
	ALERTCT("\n");

	DEVICE_PARAM_RESET(aDevParam);
	removeIDfromMainDriverByParam(&aDevParam);
}



static int __init sis8300_init_module(void)
{
	SDeviceParams aDevParam;

	ALERTCT("\n");

	DEVICE_PARAM_RESET(aDevParam);
	registerDeviceToMainDriver(&aDevParam, &ProbeFunction, &RemoveFunction, NULL);

	return 0; /* succeed */
}

module_init(sis8300_init_module);
module_exit(sis8300_cleanup_module);
