/*
 *	File: includes_dma_test.h
 *
 *	Created on: Oct 22, 2014
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *	
 *
 */
#ifndef __includes_dma_test_h__
#define __includes_dma_test_h__

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/ioport.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/mm.h>
#include <linux/kdev_t.h>
#include <linux/device.h>
#include <asm/scatterlist.h>
#include <asm/io.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>	/* copy_*_user */
#include <asm/page.h>

#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */
#include "testheaders/kern_user_test_iface.h"


#endif/* #ifndef __includes_dma_test_h__ */
