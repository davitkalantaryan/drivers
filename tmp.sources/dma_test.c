/*
 *	File: dma_test.c
 *
 *	Created on: Oct 22, 2014
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *	...
 *
 */


#define DRV_NAME	"dma_test"

#define	_PROC_ENTRY_CREATED			0
#define	_DEVC_ENTRY_CREATED			1
#define	_MUTEX_CREATED				2
//alloc_chrdev_region
#define _CHAR_REG_ALLOCATED			3
#define _CDEV_ADDED					4
#define _PCI_DRV_REG				5
#define	_IRQ_REQUEST_DONE			6
#define	_PCI_DEVICE_ENABLED			7
#define	_PCI_REGIONS_REQUESTED		8

#define	SET_FLAG_VALUE(flag,field,value) \
do{ \
	unsigned long ulMask = ~(1<<field); \
	(flag) &= ulMask & (flag); \
	ulMask = (value)<<(field); \
	(flag) |= ulMask; \
}while(0)

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include "dma_test_kernel_user.h"

#ifdef WIN32
//#pragma warning(disable : 4996) 
#define _LAST_CHAR_		'\\'
#else
#define _LAST_CHAR_		'/'
#endif  /*  #ifdef WIN32 */

#define KERN_LOG1(loglevel,...) {printk(loglevel "fl=\"%s\";ln=%d;fnc=%s:  ",strrchr(__FILE__,_LAST_CHAR_)+1,__LINE__,__FUNCTION__); \
	printk(KERN_CONT __VA_ARGS__); }

#define DEBUG1(...) KERN_LOG1(KERN_ALERT,__VA_ARGS__)

MODULE_AUTHOR("Davit Kalantaryan");
MODULE_DESCRIPTION("DMA test driver ");
MODULE_VERSION("1.1.0");
MODULE_LICENSE("Dual BSD/GPL");



static unsigned long int s_ulnFlag = 0;
static int s_nIteration = 0;

static struct class*		s_pcie_gen_class;
static struct cdev			s_cdev;     /* Char device structure      */

static int PCIE_GEN_MAJOR = 46;     /* static major by default */
static int PCIE_GEN_MINOR = 0;      /* static minor by default */

static ssize_t test_dma_read1(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t test_dma_read2(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t test_dma_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
static long  test_dma_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int test_dma_open(struct inode *inode, struct file *filp);
static int test_dma_release(struct inode *inode, struct file *filp);

static struct file_operations s_pcie_gen_fops = {
	.owner			= THIS_MODULE,
	.read			= test_dma_read1,
	.write			= test_dma_write,
	//.ioctl		= pcie_gen_ioctl,
	.unlocked_ioctl = test_dma_ioctl,
	.compat_ioctl	= test_dma_ioctl,
	.open			= test_dma_open,
	.release		= test_dma_release,
};



static ssize_t test_dma_read1(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	//struct tamc200_dev  *dev = filp->private_data;
	DEBUG1("READ1: filep=%p,buf=%p,count=%d,pos=%p\n", filp, buf, (int)count, f_pos);
	return count;
}



static ssize_t test_dma_read2(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	//struct tamc200_dev  *dev = filp->private_data;
	DEBUG1("READ2: filep=%p,buf=%p,count=%d,pos=%p\n", filp, buf, (int)count, f_pos);
	return count;
}



static ssize_t test_dma_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	//struct tamc200_dev  *dev = filp->private_data;
	DEBUG1("filep=%p,buf=%p,count=%d,pos=%p\n", filp, buf, (int)count, f_pos);
	return count;
}


static struct device*			device0;


static long  test_dma_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	//struct tamc200_dev  *dev = filp->private_data;
	int nReturn = 0;
	printk(KERN_ALERT "filep=%p,cmd=%d,arg=%ld\n", filp, (int)cmd, (long)arg);

	if (_IOC_TYPE(cmd) != DMA_TEST_GEN_IOC) return -ENOTTY;
	if (_IOC_NR(cmd) > PCIE_GEN_IOC_MAXNR) return -ENOTTY;

	switch (cmd)
	{
	case CHANGE_DEV_NAME:
		DEBUG1( "pcie_gen_ioctl: CHANGE_DEV_NAME\n");
		//kobject_set_name(&device0->kobj, "%s%d", DRV_NAME, s_nIteration++);
		//dev_set_name(device0, "%s%d", DRV_NAME, s_nIteration++);
		kobject_set_name(&s_cdev.kobj, "%s%d", DRV_NAME, s_nIteration++);
		break;

	case GET_DEV_NAME:
		DEBUG1("pcie_gen_ioctl: GET_DEV_NAME. kobject_name(&s_cdev.kobj) = \"%s\"; kobject_name(&device0->kobj) = \"%s\"\n",
			kobject_name(&s_cdev.kobj) ? kobject_name(&s_cdev.kobj) : "NULL",
			kobject_name(&device0->kobj) ? kobject_name(&device0->kobj) : "NULL");
		break;

	case CHANGE_READ_FNC:
		s_pcie_gen_fops.read = s_pcie_gen_fops.read == &test_dma_read1 ? test_dma_read2 : test_dma_read1;
		break;

	default:
		nReturn = -1;
		break;
	}

	return 0;


}



static int test_dma_open(struct inode *inode, struct file *filp)
{
	int    minor;

	minor = iminor(inode);
	DEBUG1("Open Procces is \"%s\" (pid %i) DEV is %d \n", current->comm, current->group_leader->pid, minor);

	return 0;
}



static int test_dma_release(struct inode *inode, struct file *filp)
{
	DEBUG1("inode=%p,filp=%p\n", inode, filp);
	return 0;
}


static void __exit dma_test_cleanup_module(void)
{
	dev_t devno = MKDEV(PCIE_GEN_MAJOR, PCIE_GEN_MINOR);
	KERN_LOG1(KERN_WARNING, "pcie_gen_cleanup_module CALLED\n");
	device_destroy(s_pcie_gen_class, devno);
	cdev_del(&s_cdev);
	unregister_chrdev_region(devno, 1);
	class_destroy(s_pcie_gen_class);
	//DestroyNewLock(&s_dma_test0.m_DevLock);

}


static int __init dma_test_init_module(void)
{
	int   result = 0;
	int   devno = 0;
	dev_t dev;

	result = alloc_chrdev_region(&dev, PCIE_GEN_MINOR, 1, DRV_NAME);
	if (result)
	{
		KERN_LOG1(KERN_ERR,"Error allocating Major Number for mtcapcie driver.\n");
		goto errorOut;
	}
	SET_FLAG_VALUE(s_ulnFlag, _CHAR_REG_ALLOCATED, 1);
	PCIE_GEN_MAJOR = MAJOR(dev);
	DEBUG1("AFTER_INIT DRV_MAJOR is %d\n", PCIE_GEN_MAJOR);

	s_pcie_gen_class = class_create(THIS_MODULE, DRV_NAME);
	if (IS_ERR(s_pcie_gen_class))
	{
		result = PTR_ERR(s_pcie_gen_class);
		KERN_LOG1(KERN_ERR,"Error creating mtcapcie class err = %d. class = %p\n", result, s_pcie_gen_class);
		s_pcie_gen_class = NULL;
		goto errorOut;
	}

	cdev_init(&s_cdev, &s_pcie_gen_fops);
	s_cdev.owner = THIS_MODULE;
	s_cdev.ops = &s_pcie_gen_fops;

	devno = MKDEV(PCIE_GEN_MAJOR, PCIE_GEN_MINOR);

	result = cdev_add(&s_cdev, devno, 1);
	if (result)
	{
		KERN_LOG1(KERN_ERR,"Error %d adding devno%d num%d", result, devno, 0);
		goto errorOut;
	}
	SET_FLAG_VALUE(s_ulnFlag, _CDEV_ADDED, 1);

	device0 = device_create(s_pcie_gen_class, NULL, devno, NULL, "%s%d",DRV_NAME,s_nIteration++);
	if (IS_ERR(device0))
	{
		KERN_LOG1(KERN_ERR, "Device creation error!\n");
		goto errorOut;
	}
	SET_FLAG_VALUE(s_ulnFlag, _DEVC_ENTRY_CREATED, 1);
	
	return 0;

errorOut:
	return 1;
}

module_init(dma_test_init_module);
module_exit(dma_test_cleanup_module);
