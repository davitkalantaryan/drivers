/*
 *	File: mtcagen_ioctl.c
 *
 *	Created on: May 12, 2013
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *	This file contain definitions of all exported functions
 *
 */

#include "mtcagen_exp.h"

#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/module.h>
#include "mtcagen_io.h"
#include "pciedev_ufn.h"
#include "debug_functions.h"

extern int RequestInterrupt_type1(struct pciedev_dev* a_pDev, const char* a_DRV_NAME, unsigned long a_arg);
extern void UnRequestInterrupt_all(struct pciedev_dev* a_pDev);
extern int RegisterUserForInterrupt_all(struct pciedev_dev* a_pDev, unsigned long a_arg);
extern int UnregisterUserFromInterrupt_all(struct pciedev_dev* a_pDev);


long mtcagen_ioctl_exp(struct file *filp, unsigned int a_cmd, unsigned long a_arg)
{
	struct pciedev_dev*			dev = filp->private_data;
	int							nReturn = 0;

	if (unlikely(!dev->dev_sts))
	{
		//PRINT_KERN_ALERT("NO DEVICE in slot %d\n", a_pDev->m_slot_num);
		return -EFAULT;
	}

	DEBUGNEW("\n");

	switch (a_cmd)
	{
	case GEN_REQUEST_IRQ1_FOR_DEV:
		nReturn = RequestInterrupt_type1(dev, DRV_AND_DEV_NAME, a_arg);
		break;

	case GEN_UNREQUEST_IRQ_FOR_DEV:
		DEBUGCT("PCIE_GEN_UNREQUEST_IRQ\n");
		UnRequestInterrupt_all(dev);
		break;

	case GEN_REGISTER_USER_FOR_IRQ:
		DEBUGCT("case PCIEGEN_REGISTER_FOR_IRQ\n");
		return RegisterUserForInterrupt_all(dev, a_arg);

	case GEN_UNREGISTER_USER_FROM_IRQ:
		DEBUGCT("case PCIE_GEN_UNREGISTER_FROM_IRQ2\n");
		UnregisterUserFromInterrupt_all(dev);
		break;

	case MTCA_GET_IRQ_TYPE:
		return dev->irq_type;

	default:
		DEBUGNEW("default:\n");
		return pciedev_ioctl_exp(filp, &a_cmd, &a_arg, dev->parent_dev);
	}

	return nReturn;
}
EXPORT_SYMBOL(mtcagen_ioctl_exp);
