/*
 *	File: write_and_inform.c
 *
 *	Created on: Aug 11, 2016
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *	This file contain definitions of ...
 *
 */

#include <linux/init.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include "read_write_inline.h"

#include "mtcagen_exp.h"
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/module.h>
#include "mtcagen_io.h"
#include "pciedev_ufn.h"
#include "debug_functions.h"

static struct task_struct*	s_task		= NULL;
static struct semaphore		s_semaphore;
static volatile int			s_nDoWork;

static int write_and_inform_thread_func(void *data);


int pcie_gen_init_write_and_inform(void)
{
	s_nDoWork = 1;
	sema_init(&s_semaphore, 0);
	s_task = kthread_run(write_and_inform_thread_func, NULL, "write_and_inform_thread");
	return 0;
}


void pcie_gen_cleanup_write_and_inform(void)
{
	s_nDoWork = 0;
	up(&s_semaphore);
	kthread_stop(s_task);
	s_task = NULL;
	msleep(1);
}


/*
* pciedev_write_and_inform_exp:  write data from user space buffer to a_count number of device registers
*
* Parameters:
*   a_dev:           device structure to handle
*   a_mode:          register access mode (read or write and register len (8,16 or 32 bit))
*   a_bar:           bar to access (0, 1, 2, 3, 4 or 5)
*   a_offset:        first register offset
*   a_pcUserBuf:     pointer to user buffer for data
*   a_pcUserMask:    pointer to user buffer for mask (in the case of bitwise write)
*   a_count:         number of device register to write
*
* Return (int):
*   < 0:             error (details on all errors on documentation)
*   other:           number of Bytes read from device registers
*/
ssize_t pciedev_write_and_inform_exp(void* a_a_dev,
	u_int16_t a_register_size, u_int16_t a_rw_access_mode, u_int32_t a_bar, u_int32_t a_offset,
	const char __user *a_pcUserData0, const char __user *a_pcUserMask0, size_t a_count)
{
	ssize_t retval = pciedev_write_inline2(a_a_dev, a_register_size, a_rw_access_mode, 
		a_bar, a_offset,a_pcUserData0,a_pcUserMask0,a_count);

	return retval;
}
EXPORT_SYMBOL(pciedev_write_and_inform_exp);


static int write_and_inform_thread_func(void *a_data)
{
	while (s_nDoWork)
	{
		if (down_interruptible(&s_semaphore) == 0)
		{
		}
	}
	return (int)((long long int)a_data);
}
