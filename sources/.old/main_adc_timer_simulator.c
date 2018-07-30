/*
*	File: main_adc_timer_simulator.c
*
*	Created on: Sep 29, 2015
*	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
*
*
*/

#define	MAY_BE_DELETED
//#define	MAY_BE_DELETED	__attribute__((deprecated))

#define	ADC_TIMER_SIMUL_MINOR		0

#define ADC_SIMUL_DRV_NAME	"adc_timer_simulator"

#define DEBUGNEW(...)	if(s_nDoDebug){ALERTCT(__VA_ARGS__);}

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include "mtcagen_exp.h"
#include "debug_functions.h"
#include "adc_timer_simulator_io.h"
#include "version_dependence.h"
#include "timer_adc_header.h"

static int s_nDoDebug = 1;
module_param_named(debug, s_nDoDebug, int, S_IRUGO | S_IWUSR);

#ifndef WIN32
MODULE_AUTHOR("David Kalantaryan");
MODULE_DESCRIPTION("Driver for struck sis8300 board");
MODULE_VERSION("1.1.0");
MODULE_LICENSE("Dual BSD/GPL");
#endif

static struct SIrqWaiterStruct s_waitStruct;
static int s_gen_event = 0;

static long  Adc_timer_simul_ioctl(struct file *a_filp, unsigned int a_cmd, unsigned long a_arg)
{
	switch (a_cmd)
	{
	case ADC_TIMER_SIMUL_DO_DMA:
		++s_gen_event;
		++s_waitStruct.numberOfIRQs;
		break;
	default:
		DEBUGNEW("default\n");
		return -1;
	}

	return 0;
}



static struct file_operations s_adc_timer_simul_fops = { 
	.unlocked_ioctl = Adc_timer_simul_ioctl };

static struct class*	s_fops_class = NULL;
static int s_MAJOR = 0;
static struct cdev	s_cdev;
static struct device*			s_device0 = NULL;
static unsigned long int s_ulnFlag = 0;


static void CleanDriver(void)
{
	dev_t devno = MKDEV(s_MAJOR, ADC_TIMER_SIMUL_MINOR);

	ALERTCT("\n");

	if (s_device0)
	{
		device_destroy(s_fops_class, devno);
		s_device0 = NULL;
	}

	if (GET_FLAG_VALUE(s_ulnFlag, _CDEV_ADDED))
	{
		cdev_del(&s_cdev);
		SET_FLAG_VALUE(s_ulnFlag, _CDEV_ADDED, 0);
	}

	if (GET_FLAG_VALUE(s_ulnFlag, _CHAR_REG_ALLOCATED))
	{
		unregister_chrdev_region(devno, 1);
		SET_FLAG_VALUE(s_ulnFlag, _CHAR_REG_ALLOCATED, 0);
	}

	if (s_fops_class)
	{
		class_destroy(s_fops_class);
		s_fops_class = NULL;
	}

	RemoveEntryFromGlobalContainer(_ENTRY_NAME_WAITERS_TEST_);
	RemoveEntryFromGlobalContainer(_ENTRY_NAME_GEN_EVENT_TEST_);

}



static void __exit Adc_timer_simul_cleanup_module(void)
{
	ALERTCT("\n");

	CleanDriver();
}



static int __init Adc_timer_simul_init_module(void)
{
	int result;
	int   devno;
	dev_t dev;

	ALERTCT("============= version 1:\n");

	init_waitqueue_head(&s_waitStruct.waitIRQ);
	s_waitStruct.isIrqActive = 1;
	s_waitStruct.numberOfIRQs = 0;
	AddNewEntryToGlobalContainer(&s_gen_event, _ENTRY_NAME_GEN_EVENT_TEST_);
	AddNewEntryToGlobalContainer(&s_waitStruct, _ENTRY_NAME_WAITERS_TEST_);

	result = alloc_chrdev_region(&dev, ADC_TIMER_SIMUL_MINOR, 1, ADC_SIMUL_DRV_NAME);
	if (result)
	{
		ERRCT("Error allocating Major Number for adc_timer_simulator driver.\n");
		goto errorOut;
	}
	SET_FLAG_VALUE(s_ulnFlag, _CHAR_REG_ALLOCATED, 1);
	s_MAJOR = MAJOR(dev);

	s_fops_class = class_create(THIS_MODULE, ADC_SIMUL_DRV_NAME);
	if (IS_ERR(s_fops_class))
	{
		result = PTR_ERR(s_fops_class);
		ERRCT("Error creating mtcapcie class err = %d. class = %p\n", result, s_fops_class);
		s_fops_class = NULL;
		goto errorOut;
	}

	devno = MKDEV(s_MAJOR, ADC_TIMER_SIMUL_MINOR);

	cdev_init(&s_cdev, &s_adc_timer_simul_fops);
	s_cdev.owner = THIS_MODULE;
	s_cdev.ops = &s_adc_timer_simul_fops;

	result = cdev_add(&s_cdev, devno, 1);
	if (result)
	{
		ERRCT("Error %d adding devno%d num%d", result, devno, 0);
		goto errorOut;
	}
	SET_FLAG_VALUE(s_ulnFlag, _CDEV_ADDED, 1);

	s_device0 = device_create(s_fops_class, NULL, MKDEV(s_MAJOR, ADC_TIMER_SIMUL_MINOR), NULL, ADC_SIMUL_DRV_NAME);
	if (IS_ERR(s_device0))
	{
		ERRCT("Device creation error!\n");
		goto errorOut;
	}

	return 0; /* succeed */

errorOut:
	CleanDriver();
	return result;
}

module_init(Adc_timer_simul_init_module);
module_exit(Adc_timer_simul_cleanup_module);
