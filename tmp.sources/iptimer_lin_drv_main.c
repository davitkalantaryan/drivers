/*
 *	File: iptimer_lin_drv_main.c
 *
 *	Created on: Sep 15, 2015
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *
 */

#define NUMBER_OF_BOARDS		32
#define DEBUGNEW(...)			if(s_nDoDebug){ALERTCT(__VA_ARGS__);}

#define TAMC200_VENDOR_ID		0x10B5	/* TEWS vendor ID */
#define TAMC200_DEVICE_ID		0x9030	/* IP carrier board device ID */
#define TAMC200_SUBVENDOR_ID	0x1498	/* TEWS vendor ID */
#define TAMC200_SUBDEVICE_ID	0x80C8	/* IP carrier board device ID */

#define	TAMC200DEVNAME			"iptimer"

#include <linux/module.h>
#include <linux/delay.h>
#include "mtcagen_exp.h"
#include "debug_functions.h"
#include "sis8300_io.h"
#include "version_dependence.h"

#ifndef WIN32
MODULE_AUTHOR("David Kalantaryan");
MODULE_DESCRIPTION("Driver for struck sis8300 board");
MODULE_VERSION("1.1.0");
MODULE_LICENSE("Dual BSD/GPL");
#endif

static int s_nDoDebug = 0;
module_param_named(debug, s_nDoDebug, int, S_IRUGO | S_IWUSR);

//static int s_nDefaultClockType = 0;
//module_param_named(clock, s_nDefaultClockType, int, S_IRUGO | S_IWUSR);

struct STamc200Zn
{
	struct pciedev_dev* dev;
	int					mmap_usage;
	wait_queue_head_t	waitIRQ;
	int					numberOfIrqDone;
};

static struct file_operations	s_tamc200FileOps /*= g_default_mtcagen_fops_exp*/;

static void Tamc200_vma_open(struct vm_area_struct *a_vma)
{
	struct STamc200Zn* pTamc200 = a_vma->vm_private_data;
	++pTamc200->mmap_usage;
}


static void Tamc200_vma_close(struct vm_area_struct *a_vma)
{
	struct STamc200Zn* pTamc200 = a_vma->vm_private_data;
	--pTamc200->mmap_usage;
}

static struct vm_operations_struct sis8300_mmap_vm_ops =
{
	.open = Tamc200_vma_open,
	.close = Tamc200_vma_close,
	//.fault = daq_vma_fault,
};

static struct STamc200Zn s_vDevices[NUMBER_OF_BOARDS];
static int s_nOtherDeviceInterrupt = 0;


#if LINUX_VERSION_CODE < 0x20613 // irq_handler_t has changed in 2.6.19
static irqreturn_t Tamc200_interrupt(int irq, void *dev_id, struct pt_regs *regs)
#else
static irqreturn_t Tamc200_interrupt(int irq, void *dev_id)
#endif
{
	struct STamc200Zn*	dev = dev_id;
	//struct pciedev_dev*	pciedev = dev->dev;
	//char*				deviceAddress = pciedev->memmory_base[0];
	//uint32_t			intreg;

	++dev->numberOfIrqDone;
	wake_up(&dev->waitIRQ);

	return IRQ_HANDLED;
}



static int PrepareInterrupt(struct STamc200Zn* a_pDev)
{
	struct pciedev_dev* pPcieDev = a_pDev->dev;

	if (pPcieDev->irqData){ return 1; }

	pPcieDev->irqData = kzalloc(sizeof(), GFP_KERNEL);
	return 0;
}



static int ProbeFunction(struct pciedev_dev* a_dev, void* a_pData)
{
	char*	deviceAddress;

	int nReturn = 0;
	int device = a_dev->pciedev_pci_dev->device;
	int brdNum = a_dev->brd_num % NUMBER_OF_BOARDS;

	if (s_vDevices[brdNum].dev) return -1; // Already in use

	ALERTCT("\n");

	nReturn = Mtcagen_GainAccess_exp(a_dev, 0, NULL, &s_tamc200FileOps,TAMC200DEVNAME, "%ss%d", TAMC200DEVNAME, a_dev->brd_num);
	if (nReturn)
	{
		ERRCT("nReturn = %d\n", nReturn);
		return nReturn;
	}

	memset(&s_vDevices[brdNum], 0, sizeof(struct STamc200Zn));
	s_vDevices[brdNum].dev = a_dev;
	a_dev->parent = &s_vDevices[brdNum];

	//nReturn = SetNewDmaSizeAndOffset(&(s_vDevices[brdNum]), _INITIAL_OFFSET_, _INITIAL_SIZE_);
	nReturn = SetNumberOfSamplings(&(s_vDevices[brdNum]), _INITIAL_NUMBER_OF_SAMPLES_);
	//s_vDevices[brdNum].dmaOffset = _INITIAL_OFFSET_;
	if (nReturn)
	{
		Mtcagen_remove_exp(a_dev, 0, NULL);
		s_vDevices[brdNum].dev = NULL;
		ERRCT("nReturn = %d\n", nReturn);
		return nReturn;
	}

	init_waitqueue_head(&s_vDevices[brdNum].waitDMA);
	nReturn = request_irq(a_dev->pci_dev_irq, &sis8300_interrupt, IRQF_SHARED | IRQF_DISABLED, SIS8300DEVNAME, &(s_vDevices[brdNum]));
	if (nReturn)
	{
		FreeDmaPages(&(s_vDevices[brdNum]));
		Mtcagen_remove_exp(a_dev, 0, NULL);
		s_vDevices[brdNum].dev = NULL;
	}

	deviceAddress = a_dev->memmory_base[0];

	/// disable all IRQs  // erevi sa petq request_irq -ic araj anel
	iowrite32(0xffff0000, ((void*)(deviceAddress + IRQ_ENABLE * 4)));
	smp_wmb();

	/// enable dma read interrupt
	iowrite32((1 << DMA_READ_DONE), ((void*)(deviceAddress + IRQ_ENABLE * 4)));
	smp_wmb();

	return nReturn;
}


static void RemoveFunction(struct pciedev_dev* a_dev, void* a_pData)
{
	struct SSIS8300Zn*		pSis8300 = a_dev->parent;
	char*	deviceAddress = pSis8300->dev->memmory_base[0];

	DEBUGCT("\n");

	/// Disabling all interrupts
	iowrite32(0xFFFF0000, (void*)(deviceAddress + IRQ_ENABLE * 4));
	smp_wmb();

	free_irq(pSis8300->dev->pci_dev_irq, pSis8300);

	FreeDmaPages(pSis8300);
	pSis8300->dev = NULL;
	Mtcagen_remove_exp(a_dev, 0, NULL);
}



static void FreeDmaPages(struct SSIS8300Zn* a_pSis8300)
{
	//u_int32_t unBytes = _WHOLE_BUF_SIZE_(a_pSis8300->numberOfSamples);
	//u_int32_t unBytes = 2*a_pSis8300->numberOfSamples;
	u_int32_t unBytes = _WHOLE_MEMORY_SIZE0_(a_pSis8300->numberOfSamples);

	ALERTCT("!!!!!!!!!!!!!!!!! unBytess=%u, a_pSis8300->destAddress=%p, a_pSis8300->sharedBusAddress=%d\n",
		unBytes, a_pSis8300->destAddress, (int)a_pSis8300->sharedBusAddress);

	if (a_pSis8300->sharedBusAddress && a_pSis8300->destAddress && a_pSis8300->numberOfSamples)
	{
		pci_free_consistent(a_pSis8300->dev->pciedev_pci_dev, unBytes, a_pSis8300->destAddress, a_pSis8300->sharedBusAddress);
	}
	a_pSis8300->sharedBusAddress = 0;
	a_pSis8300->destAddress = NULL;
	a_pSis8300->numberOfSamples = 0;
	a_pSis8300->dmaTransferLen = 0;
	a_pSis8300->oneBufLen = 0;
}


//#define SIS8300_KERNEL_DMA_BLOCK_SIZE 131072 // 128kByte

//static int SetNewDmaSizeAndOffset2(struct SSIS8300Zn* a_pSis8300, u_int32_t a_unSize,u_int32_t a_offset=0)
static int SetNumberOfSamplings(struct SSIS8300Zn* a_pSis8300, u_int32_t a_unNumberOfSamplings)
{
#if 1
	u_int32_t	unNumberOfSamples0 = a_pSis8300->numberOfSamples;
	u_int32_t unBytes;
	int* ringIndexPtr;

	ALERTCT("\n");
	if (a_unNumberOfSamplings <= 0 || a_unNumberOfSamplings>(SIS8300_MEM_MAX_SIZE / 2 / _NUMBER_OF_CHANNELS_))return 0;
	a_unNumberOfSamplings = (a_unNumberOfSamplings % 16) ? (16 + a_unNumberOfSamplings - (a_unNumberOfSamplings % 16)) : a_unNumberOfSamplings;
	if (unNumberOfSamples0 != a_unNumberOfSamplings)
	{
		FreeDmaPages(a_pSis8300);
		unBytes = _WHOLE_MEMORY_SIZE0_(a_unNumberOfSamplings);
		a_pSis8300->destAddress = pci_alloc_consistent(a_pSis8300->dev->pciedev_pci_dev, unBytes, &a_pSis8300->sharedBusAddress);

		ALERTCT("!!!!!!!!!!!!! unBytess=%u, a_pSis8300->destAddress=%p, a_pSis8300->sharedBusAddress=%d, __pa(addr)=%d\n",
			unBytes, a_pSis8300->destAddress, (int)a_pSis8300->sharedBusAddress, (int)__pa(a_pSis8300->destAddress));

		if (!a_pSis8300->destAddress || !a_pSis8300->sharedBusAddress)
		{
			ERRCT("Unable to locate memory!\n");
			FreeDmaPages(a_pSis8300);
			return -ENOMEM;
		}

		a_pSis8300->numberOfSamples = a_unNumberOfSamplings;
		a_pSis8300->dmaTransferLen = _DMA_TRANSFER_LEN3_(a_unNumberOfSamplings);
		a_pSis8300->oneBufLen = _ONE_BUFFER_SIZE3_(a_pSis8300->dmaTransferLen);
	}
	ringIndexPtr = a_pSis8300->destAddress;
	*ringIndexPtr = (_NUMBER_OF_RING_BUFFERS_ - 1);

	return 0;
#else
	u_int32_t	unBytes = a_unNumberOfSamplings * 2;
	u_int32_t	unNumberOfSamples0 = a_pSis8300->numberOfSamples;
	//size_t	unDmaMemSize = _NUMBER_OF_CHANNELS_;
	char*		deviceAddress = a_pSis8300->dev->memmory_base[0];
	int*		ringIndexPtr;
	//int			i;
	//u_int32_t	offset;
	u32			regValue, irqBackup = ioread32((void*)(deviceAddress + IRQ_ENABLE * 4));
	smp_rmb();

	/// Disable all interrupts
	iowrite32(0xFFFF0000, (void*)(deviceAddress + IRQ_ENABLE * 4));
	smp_wmb();

	// correction of number of samples
	if (unBytes%SIS8300_DMA_SYZE){ unBytes += (SIS8300_DMA_SYZE - (unBytes%SIS8300_DMA_SYZE)); }
	unBytes = unBytes == 0 ? SIS8300_DMA_SYZE : unBytes;

	/// size correction
	//if ((a_pSis8300->dmaOffset + unBytes*_NUMBER_OF_CHANNELS_) > SIS8300_MEM_MAX_SIZE)
	if ((unBytes*_NUMBER_OF_CHANNELS_) > SIS8300_MEM_MAX_SIZE)
	{
		//unBytes = (SIS8300_MEM_MAX_SIZE - a_pSis8300->dmaOffset) / _NUMBER_OF_CHANNELS_; 
		unBytes = SIS8300_MEM_MAX_SIZE / _NUMBER_OF_CHANNELS_;
	}
	a_unNumberOfSamplings = unBytes / 2;

	ALERTCT("unNumberOfSamples0=%u, a_unNumberOfSamplings=%u\n", unNumberOfSamples0, a_unNumberOfSamplings);

	if (unNumberOfSamples0 != a_unNumberOfSamplings)
	{
		u32 dmaTransferLen;
		FreeDmaPages(a_pSis8300);
		a_pSis8300->dmaTransferLenPlus4 = a_unNumberOfSamplings*_NUMBER_OF_CHANNELS_ * 2 + sizeof(int);
		unBytes = _WHOLE_BUF_SIZE_(a_unNumberOfSamplings);

		a_pSis8300->destAddress = pci_alloc_consistent(a_pSis8300->dev->pciedev_pci_dev, unBytes, &a_pSis8300->sharedBusAddress);

		ALERTCT("!!!!!!!!!!!!! unBytess=%u, a_pSis8300->destAddress=%p, a_pSis8300->sharedBusAddress=%d\n",
			unBytes, a_pSis8300->destAddress, (int)a_pSis8300->sharedBusAddress);

		if (!a_pSis8300->destAddress || !a_pSis8300->sharedBusAddress)
		{
			ERRCT("Unable to locate memory!\n");
			FreeDmaPages(a_pSis8300);
			return -ENOMEM;
		}
		a_pSis8300->numberOfSamples = a_unNumberOfSamplings;

		dmaTransferLen = a_pSis8300->dmaTransferLenPlus4 - sizeof(int);
		printk(KERN_ALERT "!!!!!!!!!!!! dmaTransferLen=%u\t", dmaTransferLen);

		/// set dma read len
		iowrite32(dmaTransferLen, ((void*)(deviceAddress + DMA_READ_LEN * 4)));
		smp_wmb();

	}

	ringIndexPtr = a_pSis8300->destAddress;
	*ringIndexPtr = (_NUMBER_OF_RING_BUFFERS_ - 1);

	/// Setting sampling number
	//regValue = (a_unNumberOfSamplings / 16) -1; // // 16->0x0  ... Number of samples
	regValue = (a_unNumberOfSamplings / 16); // // 16->0x0  ... Number of samples

	iowrite32(regValue, (void*)(deviceAddress + SIS8300_SAMPLE_LENGTH_REG * 4));
	smp_wmb();

	/// Backup old interrupts
	iowrite32(irqBackup, (void*)(deviceAddress + IRQ_ENABLE * 4));
	smp_wmb();
#endif

	return 0;
}


static int MAY_BE_DELETED GetEventNumber(void)
{
	static int nEventNumber = 0;
	return ++nEventNumber;
}


static int prepare_dma_read_prvt(struct SSIS8300Zn* a_pSis8300, dma_addr_t a_sysMemAddress,
	u_int32_t a_device_offset, u_int32_t a_count);


static int StartDma2(struct SSIS8300Zn* a_pSis8300)
{
	int nBufferIndex = (1 + *((int*)a_pSis8300->destAddress)) % _NUMBER_OF_RING_BUFFERS_;
	int nOffsetToEventNumber = _OFFSET_TO_EVENT_NUMBER3_(nBufferIndex, a_pSis8300->oneBufLen);
	int* pnEventNumber = (int*)(a_pSis8300->destAddress + nOffsetToEventNumber);
	dma_addr_t sysMemorForDma = a_pSis8300->sharedBusAddress + _OFFSET_TO_DMA_BUFFER3_(nOffsetToEventNumber);

	*pnEventNumber = GetEventNumber();
	a_pSis8300->debug = 0;
	return prepare_dma_read_prvt(a_pSis8300, sysMemorForDma, 0, a_pSis8300->dmaTransferLen);
}



static int sis8300_mmap(struct file *a_filp, struct vm_area_struct *a_vma)
{

	struct pciedev_dev*		dev = a_filp->private_data;
	struct SSIS8300Zn*		pSis8300 = dev->parent;
	unsigned long sizeFrUser = a_vma->vm_end - a_vma->vm_start;
	unsigned long sizeOrig = _WHOLE_MEMORY_SIZE0_(pSis8300->numberOfSamples);
	//unsigned long sizeOrig = 2 * pSis8300->numberOfSamples;
	//unsigned int size = size > sizeof(hess_ring_buffer_t) ? sizeof(hess_ring_buffer_t) : size;
	unsigned int size = sizeFrUser>sizeOrig ? sizeOrig : sizeFrUser;

	if (remap_pfn_range(a_vma, a_vma->vm_start, virt_to_phys((void *)pSis8300->destAddress) >> PAGE_SHIFT, size, a_vma->vm_page_prot) < 0)
	{
		ERRCT("remap_pfn_range failed\n");
		return -EIO;
	}

	a_vma->vm_private_data = pSis8300;
	a_vma->vm_ops = &sis8300_mmap_vm_ops;
	sis8300_vma_open(a_vma);

	return 0;
}


#define SIS8300REGWRITE(a_device,a_register_num,a_value) \
{ \
	char*		deviceAddress = (a_device)->dev->memmory_base[0]; \
	iowrite32((a_value),((void*)(deviceAddress + (a_register_num) * 4))); \
	smp_wmb(); \
}



static int prepare_dma_read_prvt(struct SSIS8300Zn* a_pSis8300, dma_addr_t a_sysMemAddress,
	u_int32_t a_device_offset, u_int32_t a_count)
{
	uint64_t			dmaspace_addr;
	uint32_t			temp, aquision_state;
	struct SSIS8300Zn*	sisdevice = a_pSis8300;
	char*				deviceAddress = a_pSis8300->dev->memmory_base[0];
	//dma_addr_t			sysMemAddress = a_sharedBusAddress + a_sys_mem_offset;

	printk(KERN_ALERT "!!!!!!!!!! count=%u\n", a_count);

	aquision_state = ioread32((void*)(deviceAddress + SIS8300_ACQUISITION_CONTROL_STATUS_REG * 4));//deviceAddress
	smp_rmb();
	if (aquision_state & 0x00000001)
	{
		// some more reporting
		printk(KERN_WARNING "!!!!!!!!!!!!!!!!!!+++++++++++ Sampling busy!!!\n");
		return -EBUSY;
	}

	SIS8300REGWRITE(sisdevice, DMA_READ_SRC_ADR_LO32, a_device_offset);

	// set destination address
	dmaspace_addr = (uint64_t)a_sysMemAddress;
	temp = (uint32_t)dmaspace_addr;
	SIS8300REGWRITE(sisdevice, DMA_READ_DST_ADR_LO32, temp);

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	temp = (uint32_t)(dmaspace_addr >> 32);
#else
	temp = 0;
#endif
	SIS8300REGWRITE(sisdevice, DMA_READ_DST_ADR_HI32, temp);

	// set transfer len
	SIS8300REGWRITE(sisdevice, DMA_READ_LEN, a_count);

	// disable interrupt
	SIS8300REGWRITE(sisdevice, IRQ_ENABLE, 0xFFFF0000);

	// enable interrupt
	SIS8300REGWRITE(sisdevice, IRQ_ENABLE, (1 << DMA_READ_DONE));

	//sisdevice->intr_flag = 0;
	// start
	SIS8300REGWRITE(sisdevice, DMA_READ_CTRL, (1 << DMA_READ_START));

	return 0;
}


#define TEST_STREAMING_DMA
//void*                  pWriteBuf = 0;

static long  sis8300_ioctl(struct file *a_filp, unsigned int a_cmd, unsigned long a_arg)
{
	struct pciedev_dev*		dev = a_filp->private_data;
	struct SSIS8300Zn*		pSis8300 = dev->parent;
	u64						ulnJiffiesTmOut;
	int						nReturn = 0;
	int						nSharedmemSize;
	int32_t					nUserValue;
	int						nNextNumberOfDmaDone = pSis8300->numberOfdmaDone + 1;
	int nDmaLen;
	int nSisMemOffset;
	int nDeviceCardOffset;
	int nTimeout;
#ifdef TEST_STREAMING_DMA
	void*                  pWriteBuf = NULL;
	int* pnEventNumber;
	dma_addr_t      pTmpDmaHandle;
	int                      tmp_order = 0;
	int nBufferIndex, nOffsetToEventNumber;
#endif

	if (unlikely(!dev->dev_sts))
	{
		WARNCT("device has been taken out!\n");
		return -ENODEV;
	}

	switch (a_cmd)
	{
	case SIS8300_TEST1:
		DEBUGNEW("SIS8300_TEST1\n");
		break;

	case SIS8300_TEST_DMA:
		DEBUGNEW("SIS8300_TEST_DMA\n");
		nReturn = StartDma2(pSis8300);
		break;

#ifdef TEST_STREAMING_DMA
	case SIS8300_TEST_STREAMING_DMA:
		DEBUGNEW("SIS8300_TEST_STREAMING_DMA\n");
		tmp_order = get_order(pSis8300->dmaTransferLen);
		pWriteBuf = (void *)__get_free_pages(GFP_KERNEL | __GFP_DMA, tmp_order);
		pTmpDmaHandle = pci_map_single(dev->pciedev_pci_dev, pWriteBuf, pSis8300->dmaTransferLen, PCI_DMA_FROMDEVICE);

		nBufferIndex = (1 + *((int*)pSis8300->destAddress)) % _NUMBER_OF_RING_BUFFERS_;
		nOffsetToEventNumber = _OFFSET_TO_EVENT_NUMBER3_(nBufferIndex, pSis8300->oneBufLen);
		pnEventNumber = (int*)(pSis8300->destAddress + nOffsetToEventNumber);
		*pnEventNumber = GetEventNumber();
		pSis8300->debug = 0;
		nReturn = prepare_dma_read_prvt(pSis8300, pTmpDmaHandle, 0, pSis8300->dmaTransferLen);
		// Should be checked
		wait_event_interruptible_timeout(pSis8300->waitDMA, nNextNumberOfDmaDone <= pSis8300->numberOfdmaDone, 10000);

		pci_unmap_single(dev->pciedev_pci_dev, pTmpDmaHandle, pSis8300->dmaTransferLen, PCI_DMA_FROMDEVICE);
		copy_to_user((void *)a_arg, pWriteBuf, pSis8300->dmaTransferLen);
		free_pages((ulong)pWriteBuf, tmp_order);
		break;

	case SIS8300_TEST_STREAMING_DMA2:
		DEBUGNEW("SIS8300_TEST_STREAMING_DMA\n");
		tmp_order = get_order(pSis8300->dmaTransferLen);
		pWriteBuf = (void *)__get_free_pages(GFP_KERNEL | __GFP_DMA, tmp_order);
		pTmpDmaHandle = pci_map_single(dev->pciedev_pci_dev, pWriteBuf, pSis8300->dmaTransferLen, PCI_DMA_FROMDEVICE);

		nBufferIndex = (1 + *((int*)pSis8300->destAddress)) % _NUMBER_OF_RING_BUFFERS_;
		nOffsetToEventNumber = _OFFSET_TO_EVENT_NUMBER3_(nBufferIndex, pSis8300->oneBufLen);
		pnEventNumber = (int*)(pSis8300->destAddress + nOffsetToEventNumber);
		*pnEventNumber = GetEventNumber();
		pSis8300->debug = 0;
		nReturn = prepare_dma_read_prvt(pSis8300, pTmpDmaHandle, 0, pSis8300->dmaTransferLen);
		// Should be checked
		wait_event_interruptible_timeout(pSis8300->waitDMA, nNextNumberOfDmaDone <= pSis8300->numberOfdmaDone, 10000);

		pci_unmap_single(dev->pciedev_pci_dev, pTmpDmaHandle, pSis8300->dmaTransferLen, PCI_DMA_FROMDEVICE);
		//copy_to_user((void *)a_arg, pWriteBuf, pSis8300->dmaTransferLen);
		free_pages((ulong)pWriteBuf, tmp_order);
		break;
#endif
#if 0
	case SIS8300_TEST_SAMPLING:
		DEBUGNEW("SIS8300_TEST_SAMPLING\n");
		StartSampling2(pSis8300);
		break;
#endif

	case SIS8300_SHARED_MEMORY_SIZE:
		DEBUGNEW("SIS8300_SHARED_MEMORY_SIZE\n");
		nSharedmemSize = _WHOLE_MEMORY_SIZE0_(pSis8300->numberOfSamples);
#if 1
		nReturn = nSharedmemSize;
#else
		nReturn = copy_to_user((int*)a_arg, &nSharedmemSize, sizeof(int));
#endif
		break;

	case SIS8300_NUMBER_OF_SAMPLES:
		DEBUGNEW("SIS8300_NUMBER_OF_SAMPLES\n");
		nReturn = pSis8300->numberOfSamples;
		break;

	case SIS8300_WAIT_FOR_DMA_INF:
		DEBUGNEW("SIS8300_WAIT_FOR_DMA_INF\n");
		nReturn = wait_event_interruptible(pSis8300->waitDMA, nNextNumberOfDmaDone <= pSis8300->numberOfdmaDone);
		break;

	case SIS8300_WAIT_FOR_DMA_TIMEOUT:
		DEBUGNEW("SIS8300_WAIT_FOR_DMA_TIMEOUT\n");
		if (copy_from_user(&nUserValue, (int32_t*)a_arg, sizeof(int32_t)))
		{
			nReturn = -EFAULT;
			goto returnPoint;
		}
		ulnJiffiesTmOut = msecs_to_jiffies(nUserValue);
		nReturn = wait_event_interruptible_timeout(pSis8300->waitDMA, nNextNumberOfDmaDone <= pSis8300->numberOfdmaDone, ulnJiffiesTmOut);
		break;

	case SIS8300_SET_NUMBER_OF_SAMPLES:
		DEBUGNEW("SIS8300_SET_NUMBER_OF_SAMPLES\n");
		if (pSis8300->mmap_usage != 0) return -EBUSY;

		if (copy_from_user(&nUserValue, (int32_t*)a_arg, sizeof(int32_t)))
		{
			nReturn = -EFAULT;
			goto returnPoint;
		}

		if (nUserValue>0)SetNumberOfSamplings(pSis8300, nUserValue);
		break;

	case SIS8300_MAKE_DMA_TEST:
		if (copy_from_user(&nDmaLen, (int*)a_arg, 4))
			return -EFAULT;
		if (copy_from_user(&nSisMemOffset, (int*)(a_arg + 4), 4))
			nSisMemOffset = 0;
		if (copy_from_user(&nDeviceCardOffset, (int*)(a_arg + 8), 4))
			nDeviceCardOffset = 0;//nTimeout
		if (copy_from_user(&nTimeout, (int*)(a_arg + 12), 4))
			nTimeout = 0;
		//nDmaLen = nDmaLen % SIS8300_KERNEL_DMA_BLOCK_SIZE;
		nDmaLen %= pSis8300->dmaTransferLen;
		//nSisMemOffset %= 
		printk(KERN_ALERT "!!!!!!!!!!dmaSize=%d, sys_offset=%d, cardOffset=%d, timeout=%d\n",
			nDmaLen, nSisMemOffset, nDeviceCardOffset, nTimeout);
		ulnJiffiesTmOut = msecs_to_jiffies(nTimeout);
		pSis8300->debug = 1;
		nReturn = prepare_dma_read_prvt(pSis8300, pSis8300->sharedBusAddress + nSisMemOffset, nDeviceCardOffset, nDmaLen);
		if (nTimeout > 0)wait_event_interruptible_timeout(pSis8300->waitDMA, nNextNumberOfDmaDone <= pSis8300->numberOfdmaDone, ulnJiffiesTmOut);
		else if (nTimeout<0)wait_event_interruptible(pSis8300->waitDMA, nNextNumberOfDmaDone <= pSis8300->numberOfdmaDone);;
		break;

	default:
		DEBUGNEW("default\n");
		return mtcagen_ioctl_exp(a_filp, a_cmd, a_arg);
	}

returnPoint:
	return nReturn;
}



static void __exit sis8300_cleanup_module(void)
{
	SDeviceParams aDevParam;

	ALERTCT("\n");

	DEVICE_PARAM_RESET(aDevParam);
	aDevParam.vendor_id = SIS8300_VENDOR_ID;

	aDevParam.vendor_id = SIS8300_VENDOR_ID;

	aDevParam.device_id = SIS8300_DEVICE_ID;
	removeIDfromMainDriverByParam(&aDevParam);

	aDevParam.device_id = SIS8300L_DEVICE_ID;
	removeIDfromMainDriverByParam(&aDevParam);

}



static int __init sis8300_init_module(void)
{
	SDeviceParams aDevParam;

#if 0
	// __KERNEL__
#ifdef __KERNEL__
	printk(KERN_ALERT "!!!!!!!!!! __KERNEL__ defined!\n");
	return -1;
#endif
#endif

	printk(KERN_ALERT "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! __USER_CS=%d(0x%x), __USER_DS=%d(0x%x), __KERNEL_CS=%d(%d), __KERNEL_DS=%d(%d)\n",
		(int)__USER_CS, (int)__USER_CS, (int)__USER_DS, (int)__USER_DS, (int)__KERNEL_CS, (int)__KERNEL_CS, (int)__KERNEL_DS, (int)__KERNEL_DS);

	ALERTCT("============= version 8:\n");

	DEVICE_PARAM_RESET(aDevParam);
	aDevParam.vendor_id = SIS8300_VENDOR_ID;

	memcpy(&s_sis833FileOps, &g_default_mtcagen_fops_exp, sizeof(struct file_operations));
	s_sis833FileOps.compat_ioctl = s_sis833FileOps.unlocked_ioctl = &sis8300_ioctl;
	s_sis833FileOps.mmap = &sis8300_mmap;

	aDevParam.device_id = SIS8300_DEVICE_ID;
	registerDeviceToMainDriver(&aDevParam, &ProbeFunction, &RemoveFunction, NULL);

	aDevParam.device_id = SIS8300L_DEVICE_ID;
	registerDeviceToMainDriver(&aDevParam, &ProbeFunction, &RemoveFunction, NULL);

	return 0; /* succeed */
}

module_init(sis8300_init_module);
module_exit(sis8300_cleanup_module);
