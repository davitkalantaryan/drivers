/*
 *	File: timer_drv_main.c
 *
 *	Created on: Oct 15, 2013, Modified 2015 Oct. 5
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *
 */

#define	TAMC200_NR_DEVS		16

#define DEVNAME				"tamc200"	/* name of device */
#define DRV_NAME			"timer_drv"	/* name of device */

//#define	IP_TIMER_WHOLE_BUFFER_SIZE_ALL	IP_TIMER_WHOLE_BUFFER_SIZE
#define	IP_TIMER_WHOLE_BUFFER_SIZE_ALL	1024
#if 0
#define	_ALLOC_MEMORY_(_a_flag_)		kzalloc(IP_TIMER_WHOLE_BUFFER_SIZE_ALL,(_a_flag_))
#define	_DEALLOC_MEMORY_(_a_memory_)	kfree((_a_memory_))
#else
#define	_ALLOC_MEMORY_(_a_flag_)		(void*)get_zeroed_page((_a_flag_))
#define	_DEALLOC_MEMORY_(_a_memory_)	free_page((unsigned long int)(_a_memory_))
#endif


#include <linux/module.h>
#include <linux/delay.h>
//#include <linux/kthread.h>
#include "mtcagen_exp.h"
#include "debug_functions.h"
#include "version_dependence.h"
#include "adc_timer_interface_io.h"
#include "iptimer_io.h"



#ifndef WIN32
MODULE_AUTHOR("Davit Kalantaryan");
MODULE_DESCRIPTION("Driver for TEWS TAMC200 IP carrier");
MODULE_VERSION("1.1.0");
MODULE_LICENSE("Dual BSD/GPL");
#endif


struct STamc200
{
	struct pciedev_dev*			dev;
	void*						sharedAddress;
	unsigned int				istimer[TAMC200_NR_SLOTS] /*: TAMC200_NR_SLOTS*/;
	struct SIrqWaiterStruct		irqWaiterStruct;
	long						event_num;
	int							added_event : 2;
	int							added_wait_irq : 2;
	int							deviceIrqAddress2;
};

static struct file_operations	s_tamc200FileOps /*= g_default_mtcagen_fops_exp*/;


#define		DEBUG_TIMING
#ifdef DEBUG_TIMING
#define NSEC_FROM_TIMESPEC(_a_timespec_,_a_timespec_end_)	\
	((u_int64_t)((u_int64_t)((_a_timespec_end_).tv_sec-(_a_timespec_).tv_sec)*1000000000L+\
	(u_int64_t)((_a_timespec_end_).tv_nsec - (_a_timespec_).tv_nsec)))
//#define	MS_FROM_TIMEVAL(_a_timeval_)			(((u_int64_t)(_a_timeval_).tv_sec)*1000L + ((u_int64_t)(_a_timeval_).tv_usec)/1000L )
//#define	REMNANT_MS_FROM_TIMEVAL(_a_timeval_)	(((u_int64_t)(_a_timeval_).tv_usec)%1000L)

struct STimingTest	s_TimingTest = {
	.iterations = 0L,
	.total_time = 0L };
#endif

struct STamc200       s_vTamc200_dev[TAMC200_NR_DEVS];	/* allocated in iptimer_init_module */
static int s_ips_irq_vec[3] = { 0xFC, 0xFD, 0xFE };

static int s_nOtherDeviceInterrupt = 0;
/*
* The top-half interrupt handler.
*/
#if LINUX_VERSION_CODE < 0x20613 // irq_handler_t has changed in 2.6.19
static irqreturn_t tamc200_interrupt(int irq, void *dev_id, struct pt_regs *regs)
#else
static irqreturn_t tamc200_interrupt(int irq, void *dev_id)
#endif
{
	struct STamc200 * pTamc200 = dev_id;
	int* pnBufferIndex = (int*)pTamc200->sharedAddress;
	//int nNextBufferIndex = (*pnBufferIndex + 1) % IP_TIMER_RING_BUFFERS_COUNT;
	int nNextBufferIndex ;
	char* deviceBar2Address = (char*)pTamc200->dev->memmory_base[2];
	char* deviceBar3Address = (char*)pTamc200->dev->memmory_base[3] + pTamc200->deviceIrqAddress2;
	//char* deviceBar3Address = pTamc200->deviceIrqAddress;
	char* ip_base_addres;
	struct STimeTelegram* pTimeTelegram ;
	struct timeval	aTv;
	u16 uEvLow;
	u16 uEvHigh;
#ifdef DEBUG_TIMING
	struct timespec	aTvEnd, aTvBeg;
	getnstimeofday(&aTvBeg);
#endif

	//if(dev->vendor_id  != TAMC200_VENDOR_ID)   return IRQ_NONE;
	//if(dev->device_id  != TAMC200_DEVICE_ID)   return IRQ_NONE;
	// make shure, this is correct interrupt

	do_gettimeofday(&aTv);

#if 1
	uEvLow = ioread16(deviceBar2Address + 0xC);
	smp_rmb();
	if (uEvLow == 0){ s_nOtherDeviceInterrupt++; return IRQ_NONE; }
#endif

	/// Piti nayvi Should be modified
	ip_base_addres = deviceBar3Address;

	uEvLow = ioread16(ip_base_addres + 0x40);///? should be modified
	//uEvLow = ioread16(ip_base_addres + 0x240);
	smp_rmb();
	if (pTamc200->dev->swap){ uEvLow=UPCIEDEV_SWAPS(uEvLow); }

	uEvHigh = ioread16(ip_base_addres + 0x42);///? should be modified
	//uEvHigh = ioread16(ip_base_addres + 0x242);
	smp_rmb();
	if (pTamc200->dev->swap){ uEvHigh=UPCIEDEV_SWAPS(uEvHigh); }

	pTamc200->event_num = (long)uEvHigh;
	pTamc200->event_num <<= 16;
	pTamc200->event_num |= (long)uEvLow;

	nNextBufferIndex = pTamc200->event_num % IP_TIMER_RING_BUFFERS_COUNT;
	//nNextBufferIndex = (nNextBufferIndex + 1) % IP_TIMER_RING_BUFFERS_COUNT;
	//nNextBufferIndex = snBufferIndex % IP_TIMER_RING_BUFFERS_COUNT;
	pTimeTelegram = (struct STimeTelegram*)((char*)pTamc200->sharedAddress + _OFFSET_TO_SPECIFIC_BUFFER_(nNextBufferIndex));

	pTimeTelegram->gen_event = (int)pTamc200->event_num;
	pTimeTelegram->seconds = aTv.tv_sec;
	pTimeTelegram->useconds = aTv.tv_usec;
	//*((int*)((char*)pTamc200->sharedAddress + _OFFSET_TO_SPECIFIC_BUFFER_(nNextBufferIndex))) = pTamc200->event_num;

	*pnBufferIndex = nNextBufferIndex;
#ifndef DEBUG_TIMING
	++pTamc200->irqWaiterStruct.numberOfIRQs;
	wake_up(&pTamc200->irqWaiterStruct.waitIRQ);
#endif

	/// Piti nayvi Should be modified
	iowrite16(0xFFFF, deviceBar2Address + 0xC);
	smp_wmb();

	/// Piti nayvi
	///ip_base_addres = deviceBar3Address;///? should be modified
	//ip_base_addres = deviceBar3Address + 0x200;
	iowrite16(0xFFFF, ip_base_addres + 0x3A);
	smp_wmb();

#ifdef DEBUG_TIMING
	getnstimeofday(&aTvEnd);
	//do_gettimeofday(&aTvEnd);
	s_TimingTest.iterations++;
	s_TimingTest.current_time = NSEC_FROM_TIMESPEC(aTvBeg, aTvEnd);
	s_TimingTest.total_time += s_TimingTest.current_time;

	++pTamc200->irqWaiterStruct.numberOfIRQs;
	wake_up(&pTamc200->irqWaiterStruct.waitIRQ);
#endif

	return IRQ_HANDLED;
}



static void DisableAllInterrupts(struct STamc200* a_pTamc200)
{

	ALERTCT("\n");

	if (a_pTamc200->irqWaiterStruct.isIrqActive)
	{
		//free_irq(tamc200_dev->pci_dev_irq, tamc200_dev);
		free_irq(a_pTamc200->dev->pci_dev_irq, a_pTamc200);
		if (a_pTamc200->added_wait_irq)RemoveEntryFromGlobalContainer(IPTIMER_WAITERS);
		if (a_pTamc200->added_event)RemoveEntryFromGlobalContainer(IPTIMER_GENEVNT);
		//kfree(a_pTamc200->sharedAddress);
		_DEALLOC_MEMORY_(a_pTamc200->sharedAddress);
		a_pTamc200->irqWaiterStruct.isIrqActive = 0;
	}
	a_pTamc200->dev->irq_type = _NO_IRQ_;
}



static int EnableInterrupt(struct STamc200* a_pTamc200, int a_IpModule)
{
	char*	deviceBar2Address = (char*)(a_pTamc200->dev->memmory_base[2]);
	char*	deviceBar3Address = (char*)(a_pTamc200->dev->memmory_base[3]);
	char*	ip_base_addres = deviceBar3Address + 0x100 * a_IpModule;
	int nReturn = 0;
	u16 tmp_slot_cntrl = 0;

	ALERTCT("\n");

	a_IpModule = a_IpModule > 2 ? 2 : (a_IpModule < 0 ? 0 : a_IpModule);

	//if (tamc200_dev[tmp_slot_num].ip_s[k].irq_on)
	{
		if (a_pTamc200->irqWaiterStruct.isIrqActive == 0)
		{
			ALERTCT(KERN_ALERT "IRQ FOR IP MODULE %i ENABLED\n", a_IpModule);
			//if(tamc200_dev[tmp_slot_num].ip_s[k].irq_lev)
			{
				//    tmp_slot_cntrl   |= 0x0080;
				//}else{
				//    tmp_slot_cntrl   |= 0x0040;
				//}
				tmp_slot_cntrl |= (1 & 0x3) << 6;
			}
			//if(tamc200_dev[tmp_slot_num].ip_s[k].irq_sens)
			if (1)
			{
				tmp_slot_cntrl |= 0x0020;
			}
			else
			{
				tmp_slot_cntrl |= 0x0010;
			}

			//a_pTamc200->sharedAddress = kzalloc(IP_TIMER_WHOLE_BUFFER_SIZE, GFP_KERNEL);
			//a_pTamc200->sharedAddress = kzalloc(IP_TIMER_WHOLE_BUFFER_SIZE_ALL, GFP_KERNEL);
			a_pTamc200->sharedAddress = _ALLOC_MEMORY_(GFP_KERNEL);

			if (!a_pTamc200->sharedAddress)
			{
				ERRCT("No memory!\n");
				return -1;
			}
			*((int*)a_pTamc200->sharedAddress) = IP_TIMER_RING_BUFFERS_COUNT-1;

			/// New
			a_pTamc200->deviceIrqAddress2 = a_IpModule * 0x100;

			init_waitqueue_head(&a_pTamc200->irqWaiterStruct.waitIRQ);
			//nReturn = request_irq(a_pTamc200->dev->pci_dev_irq, &tamc200_interrupt, IRQF_SHARED | IRQF_DISABLED, DRV_NAME, a_pTamc200);
			//IRQF_DISABLED - keep irqs disabled when calling the action handler.
			//                 DEPRECATED.This flag is a NOOP and scheduled to be removed
			//nReturn = request_irq(a_pTamc200->dev->pci_dev_irq, &tamc200_interrupt, IRQF_SHARED | IRQF_DISABLED, DRV_NAME, a_pTamc200);
			nReturn = request_irq(a_pTamc200->dev->pci_dev_irq, &tamc200_interrupt, IRQF_SHARED , DRV_NAME, a_pTamc200);
			if (nReturn)
			{
				ERRCT("Unable to activate interrupt for pin %d\n", a_pTamc200->dev->pci_dev_irq);
				return nReturn;
			}

			a_pTamc200->irqWaiterStruct.isIrqActive = 1;
			a_pTamc200->irqWaiterStruct.numberOfIRQs = 0;
#if 0
		added_event : 2;
			int							added_wait_irq;
#endif
			ALERTCT("\n");
			if (AddNewEntryToGlobalContainer(&a_pTamc200->event_num, IPTIMER_GENEVNT) == 0){ a_pTamc200->added_event = 1; }
			ALERTCT("\n");
			if (AddNewEntryToGlobalContainer(&a_pTamc200->irqWaiterStruct, IPTIMER_WAITERS) == 0){ a_pTamc200->added_wait_irq = 1; }
			ALERTCT("\n");
			a_pTamc200->dev->irq_type = _IRQ_TYPE2_;
		}

		//printk(KERN_ALERT "TAMC200_PROBE:  SLOT %i CNTRL %X\n", k, tmp_slot_cntrl);
		iowrite16(tmp_slot_cntrl, deviceBar2Address + 0x2 * (a_IpModule + 1));
		smp_wmb();
		iowrite16(s_ips_irq_vec[a_IpModule], (ip_base_addres + 0x2E));
		smp_wmb();

	}

	return 0;
}


static void RemoveFunction(struct pciedev_dev* a_dev, void* a_pData)
{
	struct STamc200*		pTamc200 = a_dev->parent;
	char* deviceBar2Address = (char*)pTamc200->dev->memmory_base[2];
	char* deviceBar3Address = (char*)pTamc200->dev->memmory_base[3];
	char* ip_base_addres;
	int                    k = 0;
    
	ALERTCT( "SLOT %d BOARD %d\n", a_dev->slot_num, a_dev->brd_num);

	/*DISABLING INTERRUPTS ON THE MODULE*/
	iowrite16(0x0000, deviceBar2Address + 0x2);
	iowrite16(0x0000, deviceBar2Address + 0x4);
	iowrite16(0x0000, deviceBar2Address + 0x6); 
	smp_wmb();

	DisableAllInterrupts(pTamc200);
        
	for(k = 0; k < TAMC200_NR_SLOTS; k++)
	{
		//if(tamc200_dev->ip_s[k].ip_on)
		{
			ip_base_addres = (char *)deviceBar3Address + 0x100*k;
			//if(tamc200_dev->ip_s[k].ip_module_type == IPTIMER)
			if (pTamc200->istimer[k])
			{
				iowrite16(0, (ip_base_addres + 0x2A));
				smp_wmb();
				iowrite16(0xFFFF, (ip_base_addres + 0x2A));
				smp_wmb();
				iowrite16(0xFFFF, (ip_base_addres + 0x20));
				smp_wmb();
				iowrite16(0xFFFF, (ip_base_addres + 0x22));
				smp_wmb();
				iowrite16(0xFFFF, (ip_base_addres + 0x24));
				smp_wmb();
				iowrite16(0xFFFF, (ip_base_addres + 0x26));
				smp_wmb();
				iowrite16(0x0, (ip_base_addres + 0x28));
				smp_wmb();
				iowrite16(0xFFFF, (ip_base_addres + 0x2C));
				smp_wmb();
			} // if (pTamc200->istimer[k])
		}
	} // for(k = 0; k < TAMC200_NR_SLOTS; k++)

	memset(pTamc200, 0, sizeof(struct STamc200));
	Mtcagen_remove_exp(a_dev, 0, NULL);
    
	////////////////////////////////////////////////////////////////////
        
}



//static int __devinit tamc200_probe(struct pci_dev *dev, const struct pci_device_id *id)
//int __devinit tamc200_probe(struct pci_dev *dev, const struct pci_device_id *id)
static int ProbeFunction(struct pciedev_dev* a_dev, void* a_pData)
{
	char*	deviceBar2Address;
	char*	deviceBar3Address;
	char*  ip_base_addres;
	int nReturn;
	int k;
	int brdNum = a_dev->brd_num % TAMC200_NR_DEVS;
	u32 tmp_module_id;
	u16 tmp_data_16;

	ALERTCT("\n");


	if (unlikely(s_vTamc200_dev[brdNum].dev)) return -1; // Already in use
	s_vTamc200_dev[brdNum].dev = a_dev;

	nReturn = Mtcagen_GainAccess_exp(a_dev, 0, NULL, &s_tamc200FileOps, DRV_NAME, "%ss%d", DEVNAME, a_dev->brd_num);
	if (nReturn)
	{
		ERRCT("nReturn = %d\n", nReturn);
		s_vTamc200_dev[brdNum].dev = NULL;
		return nReturn;
	}
	a_dev->parent = &s_vTamc200_dev[brdNum];

	deviceBar2Address = (char*)s_vTamc200_dev[brdNum].dev->memmory_base[2];
	deviceBar3Address = (char*)s_vTamc200_dev[brdNum].dev->memmory_base[3];


	for (k = 0; k < TAMC200_NR_SLOTS; k++)
	{
		//if(tamc200_dev[tmp_slot_num].ip_s[k].ip_on)
		{
			ALERTCT("TAMC200_PROBE:  SLOT %i ENABLED\n", k);

			ip_base_addres = deviceBar3Address + 0x100 * k;

			tmp_data_16 = ioread16(ip_base_addres + 0x80);
			smp_rmb();
			tmp_module_id = tmp_data_16 << 24;

			tmp_data_16 = ioread16(ip_base_addres + 0x82);
			smp_rmb();
			tmp_module_id += (tmp_data_16 << 16);

			tmp_data_16 = ioread16(ip_base_addres + 0x84);
			smp_rmb();
			tmp_module_id += tmp_data_16 << 8;

			tmp_data_16 = ioread16(ip_base_addres + 0x86);
			smp_rmb();
			tmp_module_id += tmp_data_16;

			ALERTCT("TAMC200_PROBE: MODULE ID %X\n", tmp_module_id);
		}
	} // for(k = 0; k < TAMC200_NR_SLOTS; k++)

	return 0;
}



static void tamc200_vma_open(struct vm_area_struct *a_vma){}
static void tamc200_vma_close(struct vm_area_struct *a_vma){}


static struct vm_operations_struct tamc200_mmap_vm_ops =
{
	.open = tamc200_vma_open,
	.close = tamc200_vma_close,
	//.fault = daq_vma_fault,	// Finally page fault exception should be used to do 
	// page size changing really dynamoc
};


static int tamc200_mmap(struct file *a_filp, struct vm_area_struct *a_vma)
{
	struct pciedev_dev*		dev = a_filp->private_data;
	struct STamc200*		pTamc200 = dev->parent;
	unsigned long sizeFrUser = a_vma->vm_end - a_vma->vm_start;
	unsigned long sizeOrig = IP_TIMER_WHOLE_BUFFER_SIZE_ALL;
	unsigned int size = sizeFrUser>sizeOrig ? sizeOrig : sizeFrUser;

	ALERTCT("\n");

	if (!pTamc200->sharedAddress)
	{
		ERRCT("device is not registered for interrupts\n");
		return -ENODEV;
	}

	if (remap_pfn_range(a_vma, a_vma->vm_start, virt_to_phys((void *)pTamc200->sharedAddress) >> PAGE_SHIFT, size, a_vma->vm_page_prot) < 0)
	{
		ERRCT("remap_pfn_range failed\n");
		return -EIO;
	}

	a_vma->vm_private_data = pTamc200;
	a_vma->vm_ops = &tamc200_mmap_vm_ops;
	tamc200_vma_open(a_vma);

	return 0;
}


static long  tamc200_ioctl(struct file *a_filp, unsigned int a_cmd, unsigned long a_arg)
{
	struct pciedev_dev*		dev = a_filp->private_data;
	struct STamc200*		pTamc200 = dev->parent;
	struct SIrqWaiterStruct*	psIRQ = &pTamc200->irqWaiterStruct;
	long nReturn = 0;
	long lnNextNumberOfIRQDone = pTamc200->irqWaiterStruct.numberOfIRQs + 1;
	u64						ulnJiffiesTmOut;
	int32_t nUserValue;

	DEBUGNEW("\n");

	if (unlikely(!dev->dev_sts))
	{
		WARNCT("device has been taken out!\n");
		return -ENODEV;
	}

	//if (_IOC_TYPE(cmd) != TIMING_IOC)    return -ENOTTY;
	//if (_IOC_NR(cmd) > PCIE_GEN_IOC_MAXNR) return -ENOTTY;
	//if (mutex_lock_interruptible(&dev->m_PcieGen.m_dev_mut))return -ERESTARTSYS;

	switch (a_cmd)
	{
	case IP_TIMER_TEST1:
		DEBUGNEW("IP_TIMER_TEST1\n");
		return s_nOtherDeviceInterrupt;
		break;

	case GEN_REQUEST_IRQ1_FOR_DEV: case GEN_UNREQUEST_IRQ_FOR_DEV: break; // These are not allowed to do

	case IP_TIMER_TEST_TIMING:
#ifdef DEBUG_TIMING
		copy_to_user((struct STimingTest*)a_arg, &s_TimingTest, sizeof(struct STimingTest));
#endif
		break;

	case IP_TIMER_ACTIVATE_INTERRUPT: case GEN_REQUEST_IRQ2_FOR_DEV:
		DEBUGNEW("IP_TIMER_ACTIVATE_INTERRUPT\n");
		if (psIRQ->isIrqActive) return 0;
		if (copy_from_user(&nUserValue, (int32_t*)a_arg, sizeof(int32_t)))
		{
			nReturn = -EFAULT;
			goto returnPoint;
		}
		nReturn = EnableInterrupt(pTamc200, nUserValue);
		break;

	case IP_TIMER_WAIT_FOR_EVENT_INF:
		DEBUGNEW("IP_TIMER_WAIT_FOR_EVENT_INF\n");
		if (unlikely(!psIRQ->isIrqActive)) return -1;
		nReturn = wait_event_interruptible(psIRQ->waitIRQ, lnNextNumberOfIRQDone <= psIRQ->numberOfIRQs);
		break;

	case IP_TIMER_WAIT_FOR_EVENT_TIMEOUT:
		DEBUGNEW("IP_TIMER_WAIT_FOR_EVENT_TIMEOUT\n");
		if (unlikely(!psIRQ->isIrqActive)) return -1;
		if (copy_from_user(&nUserValue, (int32_t*)a_arg, sizeof(int32_t)))
		{
			nReturn = -EFAULT;
			goto returnPoint;
		}
		ulnJiffiesTmOut = msecs_to_jiffies(nUserValue);
		nReturn = wait_event_interruptible_timeout(psIRQ->waitIRQ, lnNextNumberOfIRQDone <= psIRQ->numberOfIRQs, ulnJiffiesTmOut);
		break;

	default:
		DEBUGNEW("default\n");
		return mtcagen_ioctl_exp(a_filp, a_cmd, a_arg);
	}

returnPoint:

	return nReturn;
}


static struct SDeviceParams s_DevParam;

void __exit tamc200_cleanup_module(void)
{
	ALERTCT("\n");
	removeIDfromMainDriverByParam(&s_DevParam);
}

static int __init tamc200_init_module(void)
{
	ALERTCT("\n");
	memset(s_vTamc200_dev, 0, sizeof(s_vTamc200_dev));

	printk(KERN_ALERT "!!!!!!!!!!!!!!! __USER_CS=%d(0x%x), __USER_DS=%d(0x%x), __KERNEL_CS=%d(%d), __KERNEL_DS=%d(%d)\n",
		(int)__USER_CS, (int)__USER_CS, (int)__USER_DS, (int)__USER_DS, (int)__KERNEL_CS, (int)__KERNEL_CS, (int)__KERNEL_DS, (int)__KERNEL_DS);

	ALERTCT("============= version 9:\n");

	DEVICE_PARAM_RESET(&s_DevParam);
	s_DevParam.vendor = TAMC200_VENDOR_ID;
	s_DevParam.device = TAMC200_DEVICE_ID;
	s_DevParam.subsystem_vendor = TAMC200_SUBVENDOR_ID;
	s_DevParam.subsystem_device = TAMC200_SUBDEVICE_ID;

	memcpy(&s_tamc200FileOps, &g_default_mtcagen_fops_exp, sizeof(struct file_operations));
	s_tamc200FileOps.owner = THIS_MODULE;
	s_tamc200FileOps.compat_ioctl = s_tamc200FileOps.unlocked_ioctl = &tamc200_ioctl;
	s_tamc200FileOps.mmap = &tamc200_mmap;
	
	registerDeviceToMainDriver(&s_DevParam, &ProbeFunction, &RemoveFunction, NULL);

	///?
	printk(KERN_ALERT "!!!!!!!!!!!!!! registerDeviceToMainDriver done!!!\n");

	return 0; /* succeed */
}

module_init(tamc200_init_module);
module_exit(tamc200_cleanup_module);
