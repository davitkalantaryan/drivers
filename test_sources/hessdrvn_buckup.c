/*
*
*	Created on: May 12, 2013
*	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
*
*	New source file for hess driver
*
*
*
*/

// Driver Definitions
#define SMC_DEV_NAME "hessbus"
#define DAQ_DEV_NAME "hessdaq"
#define FPGA_DEV_NAME "hessfpga"

#define DMA_CHANNEL		5

#define DRIVER_VERSION "Release 0.2"

#define DRIVER_NAME    "HESS_DRIVER"

#define SMC_DEV_MINOR		0	// For SMC Bus Device
#define SMC_DEV_MINOR_COUNT 1	// For SMC Bus Device
#define DAQ_DEV_MINOR		0	// For DAQ IRQ Device
#define DAQ_DEV_MINOR_COUNT 1	// For DAQ IRQ Device
#define FPGA_DEV_MINOR		0	// For SMC Bus Device
#define FPGA_DEV_MINOR_COUNT 1	// For SMC Bus Device
#define CTRLBUS_CHIPSELECT	2 // which chip select line should get the timing
// CTRLBUS Timing Setup
#define CTRLBUS_SETUP		2 // Bus Cycle Setup Time
#define CTRLBUS_PULSE		5 // Bus Cycle Pulse Time
#define CTRLBUS_CYCLE		(CTRLBUS_SETUP + CTRLBUS_PULSE + 2) // Bus Cycle Length
#define CTRLBUS_SETUP_CS	1
#define CTRLBUS_PULSE_CS	6

#define SMC_MEM_START		AT91_CHIPSELECT_2 //0x30000000	// physical address region of the controlbus bus mapping *is fixed!*
#define SMC_MEM_LEN			0x4000000 // 64M
#define IOMEM_NAME			"bus2fpga"

// PIN Definitions
#define TEST_PIN_1			AT91_PIN_PB20	// Test Pin on Test Connector, may have spikes if taskit kernel is used (pin used for r/w led?!)
#define TEST_PIN_2			AT91_PIN_PB21	// Test Pin on Test Connector
#define TEST_PIN_3			AT91_PIN_PB22	// Test Pin on Test Connector
#define TEST_PIN_4			AT91_PIN_PB23	// Test Pin on Test Connector

#define HESS_IRQ_PIN		AT91_PIN_PC1
#define HESS_IRQ_PIN_NUMBER (1 << 1) // pc1
// Further Definitions

#define FPGA_EVENTCOUNTER_MAX 65535

// Control Source code Options
#define TEST_CODE 				0		// Activate Test Code, if set to 0, should never be used with productive release
#define TEST_IRQ				0		// if 1, IRQ TEst Functions are active
#define IRQ_ENABLED				1		// set to 1 to activate IRQ functionality
#define DAQ_DEVICE_ENABLED		1		// if 1, DAQ Device is enabled
#define BUS_DEVICE_ENABLED		1		// if 1, BUS Device is enabled
#define FPGA_DEVICE_ENABLED		1		// if 1, BUS Device is enabled


#include "hess_drv_exp.h"
#include "tools.h"
#include "hessirqhandlers.h"



MODULE_LICENSE("GPL");
MODULE_AUTHOR("desy");
MODULE_DESCRIPTION("memory interface for hess1u fpga");

static struct dentry *s_dirrent;

static int s_filevalue;
static u64 s_intvalue;
static u64 s_hexvalue;

static int s_nPossibleDMA = 1;
module_param_named(dma, s_nPossibleDMA, int, S_IRUGO | S_IWUSR);

//static dma_addr_t		s_DmaAdrress = 0;
//static struct device *	s_device2;


struct smc_config
{
	// Setup register
	uint8_t ncs_read_setup;
	uint8_t nrd_setup;
	uint8_t ncs_write_setup;
	uint8_t nwe_setup;

	// Pulse register
	uint8_t ncs_read_pulse;
	uint8_t nrd_pulse;
	uint8_t ncs_write_pulse;
	uint8_t nwe_pulse;

	// Cycle register
	uint16_t read_cycle;
	uint16_t write_cycle;

	// Mode register
	uint32_t mode;
	uint8_t tdf_cycles : 4;
};

void smc_configure(int cs, struct smc_config* config)
{
	// Setup register
	at91_sys_write(AT91_SMC_SETUP(cs), AT91_SMC_NWESETUP_(config->nwe_setup) | AT91_SMC_NCS_WRSETUP_(config->ncs_write_setup) | AT91_SMC_NRDSETUP_(config->nrd_setup) | AT91_SMC_NCS_RDSETUP_(config->ncs_read_setup));

	// Pulse register
	at91_sys_write(AT91_SMC_PULSE(cs), AT91_SMC_NWEPULSE_(config->nwe_pulse) | AT91_SMC_NCS_WRPULSE_(config->ncs_write_pulse) | AT91_SMC_NRDPULSE_(config->nrd_pulse) | AT91_SMC_NCS_RDPULSE_(config->ncs_read_pulse));

	// Cycle register
	at91_sys_write(AT91_SMC_CYCLE(cs), AT91_SMC_NWECYCLE_(config->write_cycle) | AT91_SMC_NRDCYCLE_(config->read_cycle));

	// Mode register
	at91_sys_write(AT91_SMC_MODE(cs), config->mode | AT91_SMC_TDF_(config->tdf_cycles));
}

// Apply the timing to the SMC
static void ctrlbus_apply_timings(void)
{
	struct smc_config config = {
		// Setup register
		.ncs_read_setup = 2, //CTRLBUS_SETUP_CS,
		.nrd_setup = 2, //CTRLBUS_SETUP,
		.ncs_write_setup = CTRLBUS_SETUP_CS,
		.nwe_setup = CTRLBUS_SETUP,

		// Pulse register
		.ncs_read_pulse = 8, //CTRLBUS_PULSE_CS,
		.nrd_pulse = 8, //CTRLBUS_PULSE,
		.ncs_write_pulse = CTRLBUS_PULSE_CS,
		.nwe_pulse = CTRLBUS_PULSE,

		// Cycle register
		.read_cycle = 8 + 2, //CTRLBUS_CYCLE,
		.write_cycle = CTRLBUS_CYCLE,

		// Mode register
		.mode = AT91_SMC_READMODE | AT91_SMC_WRITEMODE | AT91_SMC_BAT_SELECT | AT91_SMC_DBW_16 | AT91_SMC_EXNWMODE_READY,
		.tdf_cycles = 0 };

	smc_configure(CTRLBUS_CHIPSELECT, &config);

	INFO("SMC Bus Timing: chipselect=%u tdf_cycles=%u mode=0x%x\n   "
		" read: cycle=%u setup=%u pulse=%u cs_setup=%u sc_pulse=%u\n   "
		"write: cycle=%u setup=%u pulse=%u cs_setup=%u sc_pulse=%u\n", CTRLBUS_CHIPSELECT, config.tdf_cycles, config.mode, config.read_cycle, config.nrd_setup, config.nrd_pulse, config.ncs_read_setup, config.ncs_read_pulse, config.write_cycle, config.nwe_setup, config.nwe_pulse, config.ncs_write_setup, config.ncs_write_pulse);

	//	set_memory_timing(chipselect, cycle, setup, pulse, setup_cs, pulse_cs, data_float, use_iordy);

}

//--- irq ---------------------------------------------------------------------

// returns the current event count
// may be locking is not needed here
static uint32_t readIrqCount(IRQDevice_t* dev)
{
	uint32_t i;
	read_lock_irq(&dev->accessLock);
	i = dev->debug_data.total_irq_count;
	read_unlock_irq(&dev->accessLock);
	return i;
}

// ------------------------------------------ IRQ Handling --------------
static inline void clear_irq(void)
{
	uint16_t temp;
	temp = smc_bus_read16(OFFS_CONTROL) & (~(1 << BIT_CONTROL_TRIGGER_IRQ)); // ## hier und ueberall besser das macro von atmel?
	smc_bus_write16(OFFS_CONTROL, temp);
}


static irqreturn_t srqIrqHandler(int irq_number, void *p_dev)
{
	// todo: ringbufferverwaltung auslagern -> inline
	IRQDevice_t* dev = (IRQDevice_t*)p_dev;

	write_lock(&dev->accessLock);

	irqreturn_t tRet = (*dev->m_fpIrqHandler)(irq_number, p_dev);

	write_unlock(&dev->accessLock);

	return tRet;
}

static int installSRQHandler(void)
{
	//	INFO("Install IRQ Handler: ");
	memset(&g_srqDevice, 0, sizeof(IRQDevice_t));
	rwlock_init(&(g_srqDevice.accessLock));
	init_waitqueue_head(&(g_srqDevice.waitQueue));
	g_srqDevice.m_nDma = 0;
	g_srqDevice.m_fpIrqHandler = &IrqHandlerNoDma;

	if (likely(s_nPossibleDMA))
	{
		//g_srqDevice.hess_data = (hess_ring_buffer_t*)kzalloc(sizeof(hess_ring_buffer_t), GFP_DMA);

		int nPages = sizeof(hess_ring_buffer_t) / PAGE_SIZE + 1;
		printk(KERN_ALERT "------- nPages = %d\n", nPages);
		g_srqDevice.hess_data = alloc_pages_exact(nPages, GFP_DMA | GFP_KERNEL | __GFP_ZERO);
	}
	else
	{
		g_srqDevice.hess_data = (hess_ring_buffer_t*)vmalloc(sizeof(hess_ring_buffer_t)); // ## kmalloc can be an option too
		if (g_srqDevice.hess_data){ memset(g_srqDevice.hess_data, 0x0, sizeof(hess_ring_buffer_t)); }
	}
	//	g_srqDevice.hess_data = (hess_ring_buffer_t*) __get_free_pages(GFP_USER, 0); // ## kmalloc can be an option too
	//	memset(g_srqDevice.hess_data, 0, PAGE_SIZE << 0);


	if (!g_srqDevice.hess_data)
	{
		ERR("Install IRQ Handler: unable to allocate memory for ring buffer\n");
		return 1;
	}
	else
	{
		INFO("Install IRQ Handler: %u bytes allocated for ring buffer\n", sizeof(hess_ring_buffer_t));
	}

	g_srqDevice.hess_data->begin = &(g_srqDevice.hess_data->raw_buffer[0]);
	//	g_srqDevice.hess_data->end = &(g_srqDevice.hess_data->raw_buffer[(sizeof(raw_buffer_t)/sizeof(hess_event_data_t))]);
	g_srqDevice.hess_data->end = &(g_srqDevice.hess_data->raw_buffer[HESS_RING_BUFFER_COUNT]);
	g_srqDevice.hess_data->write_pointer = g_srqDevice.hess_data->begin;

	//	g_srqDevice.hess_data->begin = &(g_srqDevice.hess_data->raw_buffer[0]);
	//	g_srqDevice.hess_data->end = &(g_srqDevice.hess_data->raw_buffer[HESS_RING_BUFFER_COUNT]);
	//	g_srqDevice.hess_data->write_pointer = g_srqDevice.hess_data->begin;

	// Activate PIOC Clock (s.441 30.3.4)
	at91_sys_write(AT91_PMC_PCER, (1 << AT91SAM9G45_ID_PIOC)); // PORT_C ID == 4

	//msleep(100); // ## ?

	at91_set_GPIO_periph(HESS_IRQ_PIN, 0);
	at91_set_gpio_input(HESS_IRQ_PIN, 0); // 0 == no pullup
	at91_set_deglitch(HESS_IRQ_PIN, 1); // 1 == deglitch activated

	//msleep(100); // ## ?

	// Configure IRQ Capture Edge
	int irqRequestResult = request_irq(HESS_IRQ_PIN, srqIrqHandler, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "hess_fpga", &g_srqDevice);
	if (irqRequestResult != 0)
	{
		ERR("Install IRQ Handler: problem installing srq line irq handler %d\n", irqRequestResult);
		return 1; // error
	}

	return 0;
}

static void uninstallSRQHandler(void)
{
	at91_sys_write(AT91_PIOC + PIO_IDR, HESS_IRQ_PIN_NUMBER); // PIO_IDR == interrupt disable register

	free_irq(HESS_IRQ_PIN, &g_srqDevice);

	//	free_pages((unsigned long)(g_srqDevice.hess_data),0);
	if (likely(s_nPossibleDMA))
	{
		//kfree(g_srqDevice.hess_data);

		int nPages = sizeof(hess_ring_buffer_t) / PAGE_SIZE + 1;
		free_pages_exact((void *)g_srqDevice.hess_data, nPages);
	}
	else
		vfree(g_srqDevice.hess_data);
}
//-----------------------------------------------------------------------------
//--- irq end -----------------------------------------------------------------

//--- daq ---------------------------------------------------------------------
//-----------------------------------------------------------------------------
#include "hess1u/common/hal/commands.h"

// ----------------------------------- Character Device Driver Interface ------------------

// Char Driver Implementation

typedef struct
{
	int usage_count;
	IRQDevice_t* irqDevice;
} daq_private_data_t;
static daq_private_data_t g_daq_private_data =
{
	.usage_count = 0,
	.irqDevice = &g_srqDevice
};

static int daq_chrdrv_open(struct inode * node, struct file *f)
{
	//	DBG("daq user_open\n");

	hess_dev_t* dev;

	dev = container_of(node->i_cdev, hess_dev_t, daq_driver.cdev);
	f->private_data = dev;

	return 0;
}

static int daq_chrdrv_release(struct inode *node, struct file *f)
{
	//	DBG("daq user_release\n");
	return 0;
}

static ssize_t daq_chrdrv_read(struct file *f, char __user *buf, size_t size, loff_t *offs)
//static ssize_t daq_chrdrv_read(struct file *f, char *buf, size_t size, loff_t *offs)
{
	ssize_t retVal = 0;

	hess_dev_t* dev = f->private_data;
	unsigned int myoffs = dev->offset;

	//	DBG("daq user_read offs: 0x%p + 0x%x + 0x%x = 0x%x size:%d\n",dev->mappedIOMem, dev->offset, (int)*offs, myoffs, size);

	// lock the resource
	if (down_interruptible(&dev->sem)) { return -ERESTART; }

	// Check File Size
	if ((myoffs) >= dev->IOMemSize) { goto out; }

	// Limit Count
	if ((myoffs + size) >= dev->IOMemSize) { size = dev->IOMemSize - myoffs; }

	retVal = copy_from_io_to_user_16bit(buf, VOID_PTR(dev->mappedIOMem, myoffs), size);

	if (retVal<0) { goto out; }

	*offs += retVal; // Is this realy needed?

out:
	up(&dev->sem); // free the resource

	return retVal;
}

static ssize_t daq_chrdrv_write(struct file *f, const char __user *buf, size_t size, loff_t *offs)
//static ssize_t daq_chrdrv_write(struct file *f, const char *buf, size_t size, loff_t *offs)
{
	ssize_t retVal = 0;
	hess_dev_t* dev = f->private_data;

	//unsigned int myoffs=dev->offset + *offs;
	unsigned int myoffs = dev->offset;
	//	DBG("daq user_write offs: 0x%x  size:0x%d\n",myoffs, size);

	// lock the resource
	if (down_interruptible(&dev->sem)) { return -ERESTART; }

	// Check File Size
	//        if ((myoffs + dev->offset) >= dev->IOMemSize) goto out;
	if ((myoffs) >= dev->IOMemSize) { goto out; }

	// Limit Count
	if ((myoffs + size) >= dev->IOMemSize) size = dev->IOMemSize - myoffs;

	retVal = copy_from_user_to_io_16bit(VOID_PTR(dev->mappedIOMem, myoffs), buf, size);
	if (retVal<0) { goto out; } // fault occured, write was not completed

	*offs += retVal;

out:
	up(&dev->sem);// free the resource

	return retVal;
}

static loff_t daq_chrdrv_llseek(struct file *f, loff_t offs, int whence)
{
	hess_dev_t* dev = f->private_data;
	unsigned int myoffs = offs;
	//	DBG("daq user_llseek offs: 0x%x  whence: %d\n", myoffs, whence);
	switch (whence)
	{
	case SEEK_SET:
		if (offs >= (dev->IOMemSize))
		{
			// out of range seeking
			return -1;
		}
		dev->offset = offs;
		return offs;
		break;

	case SEEK_CUR:
		if ((dev->offset + offs) >= (dev->IOMemSize))
		{
			// out of range seeking
			return -1;
		}
		dev->offset += offs;
		return dev->offset;
		break;

	case SEEK_END:
		if ((dev->offset) >= (dev->IOMemSize))
		{
			// out of range seeking
			return -1;
		}
		dev->offset = dev->IOMemSize - myoffs;
		return dev->offset;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

//--- mmap -------------------------------------------------------------------------
static void daq_vma_open(struct vm_area_struct *vma)
{
	daq_private_data_t* data = vma->vm_private_data;
	data->usage_count++;

	//	vma->vm_private_data->usage_count++;
}

static void daq_vma_close(struct vm_area_struct *vma)
{
	daq_private_data_t* data = vma->vm_private_data;
	data->usage_count--;

	//	vma->vm_private_data->usage_count--;
}

static int daq_vma_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	//	DBG("handling fault, ");
	struct page *page;
	daq_private_data_t* data = vma->vm_private_data;

	if (s_nPossibleDMA)
		page = virt_to_page(((char*)(data->irqDevice->hess_data)) + (vmf->pgoff << PAGE_SHIFT));
	else
		page = vmalloc_to_page(((char*)(data->irqDevice->hess_data)) + (vmf->pgoff << PAGE_SHIFT));

	//	page = virt_to_page(((char*)(data->irqDevice->hess_data)) + (vmf->pgoff << PAGE_SHIFT));
	//page = vmalloc_to_page(((char*)(data->irqDevice->hess_data)) + (vmf->pgoff << PAGE_SHIFT));
	get_page(page);
	vmf->page = page;

	return 0;
}

//static int daq_vma_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
//{
////	DBG("handling fault, ");
//	daq_private_data_t* data = vma->vm_private_data;
//	char* pagePtr = NULL;
//	struct page *page;
//	unsigned long offset;
//
//	offset = (((unsigned long) vmf->virtual_address) - vma->vm_start) + (vma->vm_pgoff << PAGE_SHIFT);
//
////	DBG( " virtua.addr: 0x%x offset: 0x%x  vm_start:0x%x  vm_pgoff:%d\n", (unsigned int) vmf->virtual_address, (unsigned int) offset, (unsigned int) vma->vm_start, (unsigned int) vma->vm_pgoff);
//
//	if (offset >= sizeof(hess_ring_buffer_t))
//	{
//		return VM_FAULT_SIGBUS;
//	}
//
//	pagePtr = (char*) (data->irqDevice->hess_data);
//	pagePtr += offset;
//
////	DBG("getting page\n");
//
////	page = virt_to_page(pagePtr);
//	page = vmalloc_to_page(pagePtr);
//	get_page(page);
//	vmf->page = page;
//
//	return 0;
//}

static struct vm_operations_struct daq_remap_vm_ops =
{
	.open = daq_vma_open,
	.close = daq_vma_close,
	.fault = daq_vma_fault,
};

static int daq_chrdrv_mmap(struct file *f, struct vm_area_struct *vma)
{
	unsigned long off = vma->vm_pgoff << PAGE_SHIFT;
	//unsigned long physical = SMC_MEM_START + off;
	unsigned long vsize = vma->vm_end - vma->vm_start;
	unsigned long psize = SMC_MEM_LEN - off;

	if (vsize > psize)
	{
		return -EINVAL; // spans to high
	}

	// do not map here, let no page do the mapping
	vma->vm_ops = &daq_remap_vm_ops;
	vma->vm_flags |= VM_RESERVED;
	vma->vm_private_data = &g_daq_private_data;

	daq_vma_open(vma);
	return 0;
}
//--- mmap end -------------------------------------------------------------------------

// ----------------------------------- IOCTL Interface ---------------------
static int smc_ioctl_cmd_wait_for_irq(unsigned long arg) // ## name
{
	ioctl_command_wait_for_irq_t cmd;

	//DBG("ioctl wait for irq command\n");

	if (copy_from_user(&cmd, (ioctl_command_wait_for_irq_t*)arg, sizeof(ioctl_command_wait_for_irq_t)))
	{
		ERR("error copying data from userspace");
		return -EACCES;
	}

	//DBG( " timeout %d ms == %d jiffis (1 secound has %d jiffis)\n", cmd.timeout_ms, cmd.timeout_ms * HZ / 1000, HZ);

	// wait for the irq
	uint32_t lastCount = readIrqCount(&g_srqDevice);

	// recalculate timeout into jiffis

	//    setTestPin(1,0);
	//    wait_event(srqDevice.waitQueue, (lastCount!=srqDevice.irq_count));
	int result = wait_event_interruptible_timeout(g_srqDevice.waitQueue, (lastCount != g_srqDevice.debug_data.total_irq_count), cmd.timeout_ms * HZ / 1000);
	if (result == -ERESTARTSYS)
	{
		// System wants to shutdown, so we just exit
		return -ERESTARTSYS;
	}
	if (result == 0)
	{
		//        setTestPin(1,1);
		// Timeout occured, we just return to user process
		return -ETIMEDOUT;
	}
	//    setTestPin(1,1);

	return 0; // ## warum nicht result?
}


#include <linux/dma-mapping.h>
#include <linux/cdev.h>
//#include <asm-generic/dma.h>
//#include <linux/omap-dma.h>

// We should find corresponding functions for theses
//extern int request_dma(unsigned int chan, const char *device_id); 
//extern void free_dma(unsigned int chan);

static int request_dma(int chan, const char* device_id){ return 0; }
static void free_dma(unsigned int chan){ return; }


static int EnableDMAfor(void)
{
	int nErr;

	nErr = request_dma(DMA_CHANNEL, "hessdmatest"); // booking dma channel for our application
	if (nErr)
	{
		PRINT_KERN_ALERT("Unable to allocate dma channel N %d", DMA_CHANNEL);
		return nErr>0 ? -nErr : nErr;
	}

	//s_DmaAdrress = dma_map_single(*s_devicePtr, g_srqDevice.hess_data, sizeof(hess_ring_buffer_t), DMA_FROM_DEVICE);
	//printk(KERN_ALERT "s_DmaAdrress = %lu\n", (unsigned long)s_DmaAdrress);

	return 0;
}


static void DisableDMAfor(void)
{
	free_dma(DMA_CHANNEL);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33)
static int daq_ioctl(struct inode *node, struct file *f, unsigned int cmd, unsigned long arg)
#else
static long daq_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
#endif
{
	switch (cmd)
	{
	case IOCTL_COMMAND_WAIT_FOR_IRQ:
		return smc_ioctl_cmd_wait_for_irq(arg);
		//		case DAQ_COMMAND_WR_CONFIG:
		//			return daq_ioctl_cmd_rdwr_config(arg, 1);
		//		case DAQ_COMMAND_RD_CONFIG:
		//			return daq_ioctl_cmd_rdwr_config(arg, 0);
		//		case DAQ_COMMAND_RD_STATUS:
		//			return daq_ioctl_cmd_rd_status(arg);
		//		case DAQ_COMMAND_WR_IRQ_ENABLE:
		//			return daq_ioctl_cmd_rdwr_irq_enable(arg, 1);
		//		case DAQ_COMMAND_RD_IRQ_ENABLE:
		//			return daq_ioctl_cmd_rdwr_irq_enable(arg, 0);

	case TOGGLE_DMA:
		if (likely(s_nPossibleDMA))
		{
			// Disable IRQ in device side
			write_lock(&g_srqDevice.accessLock);

			if (g_srqDevice.m_nDma)
			{
				DisableDMAfor();
				g_srqDevice.m_nDma = 0;
				g_srqDevice.m_fpIrqHandler = &IrqHandlerNoDma;
			}
			else
			{
				EnableDMAfor();
				g_srqDevice.m_nDma = 1;
				g_srqDevice.m_fpIrqHandler = &IrqHandlerNoDma;
			}

			write_unlock(&g_srqDevice.accessLock);

			// Enable IRQ on device side
		}

		return 0;

	default:
		ERR("invalid smc ioctl cmd: %d\n", cmd);
		return -EFAULT;
	}
	return 0;
}
//-----------------------------------------------------------------------------
//--- daq end -----------------------------------------------------------------

// **************************************** Character Device Instance Definiton

// Char Driver File Operations
static struct file_operations daq_chrdrv_fops = { .open = daq_chrdrv_open, // handle opening the file-node
.release = daq_chrdrv_release, // handle closing the file-node
.read = daq_chrdrv_read, // handle reading
.write = daq_chrdrv_write, // handle writing
.llseek = daq_chrdrv_llseek, // handle seeking in the file
.unlocked_ioctl = daq_ioctl, // handle special i/o operations
.mmap = daq_chrdrv_mmap };

// Initialization of the Character Device
static int daq_chrdrv_create(hess_dev_t* mydev)
{
	int err = -1;
	char name[30];
	chrdrv_t* chrdrv = &mydev->daq_driver;

	INFO("Install DAQ Character Device\n");
	if (!mydev)
	{
		ERR("Null pointer argument!\n");
		goto err_dev;
	}

	/* Allocate major and minor numbers for the driver */
	err = alloc_chrdev_region(&chrdrv->devno, DAQ_DEV_MINOR, DAQ_DEV_MINOR_COUNT, DAQ_DEV_NAME);
	if (err)
	{
		ERR("Error allocating Major Number for daq driver.\n");
		goto err_region;
	}

	DBG("Major Number: %d\n", MAJOR(chrdrv->devno));

	/* Register the driver as a char device */
	cdev_init(&chrdrv->cdev, &daq_chrdrv_fops);
	chrdrv->cdev.owner = THIS_MODULE;
	DBG("char device allocated 0x%x\n", (unsigned int)&chrdrv->cdev);
	err = cdev_add(&chrdrv->cdev, chrdrv->devno, DAQ_DEV_MINOR_COUNT);
	if (err)
	{
		ERR("cdev_all failed\n");
		goto err_char;
	}
	DBG("Char device added\n");

	sprintf(name, "%s0", DAQ_DEV_NAME);

	// create devices
	chrdrv->device = device_create(mydev->sysfs_class, NULL, MKDEV(MAJOR(chrdrv->devno), 0), NULL, name, 0);

	if (IS_ERR(chrdrv->device))
	{
		ERR("%s: Error creating sysfs device\n", name);
		err = PTR_ERR(chrdrv->device);
		goto err_class;
	}

	//s_device2 = &chrdrv->device;

	return 0;

err_class: cdev_del(&chrdrv->cdev);
err_char: unregister_chrdev_region(chrdrv->devno, DAQ_DEV_MINOR_COUNT);
err_region:

err_dev : return err;
}

// Initialization of the Character Device
static void daq_chrdrv_destroy(hess_dev_t* mydev)
{
	chrdrv_t* chrdrv = &mydev->daq_driver;
	device_destroy(mydev->sysfs_class, MKDEV(MAJOR(chrdrv->devno), 0));

	/* Unregister device driver */
	cdev_del(&chrdrv->cdev);

	/* Unregiser the major and minor device numbers */
	unregister_chrdev_region(chrdrv->devno, DAQ_DEV_MINOR_COUNT);
}

// ************************************************  SRQ IRQ Handler Code

#if TEST_CODE==1

// some debug helper code...
static void dumpPio(size_t pio)
{
#define DUMP(NAME, REG) INFO(#REG "\t= 0x%.8x  " NAME "\n", at91_sys_read(pio + REG));
	DUMP("Status Register", PIO_PSR);
	DUMP("Output Status Register", PIO_OSR);
	DUMP("Glitch Input Filter Status", PIO_IFSR);
	DUMP("Output Data Status Register", PIO_ODSR);
	DUMP("Pin Data Status Register", PIO_PDSR);
	DUMP("Interrupt Mask Register", PIO_IMR);
	DUMP("Interrupt Status Register", PIO_ISR);

	DUMP("Multi-driver Status Register", PIO_MDSR);
	DUMP("Pull-up Status Register", PIO_PUSR);

	DUMP("AB Status Register", PIO_ABSR);
	DUMP("Output Write Status Register", PIO_OWSR);
#undef DUMP
}

#endif

// more driver...
#if	BUS_DEVICE_ENABLED==1
#include "drv_char_bus.h"
#endif

#if	FPGA_DEVICE_ENABLED==1
#include "fpga_char_bus.h"
#endif

static int hess_smc_create(hess_dev_t* mydev)
{
	int err;
	//unsigned long csa;
	INFO("Initialize SMC\n");

	/* Create sysfs entries - on udev systems this creates the dev files */
	mydev->sysfs_class = class_create(THIS_MODULE, DRIVER_NAME);
	if (IS_ERR(mydev->sysfs_class))
	{
		err = PTR_ERR(mydev->sysfs_class);
		ERR("Error creating hessdrv class err = %d. class = %p\n", err, mydev->sysfs_class);
		goto err_sysclass;
	}

	// Request Memory Region
	mydev->iomemregion = request_mem_region(SMC_MEM_START, SMC_MEM_LEN, IOMEM_NAME);
	if (!mydev->iomemregion)
	{
		ERR("could not request io-mem region for controlbus\n.");
		err = -1;
		goto err_memregion;
	}

	// Request remap of Memory Region
	mydev->mappedIOMem = ioremap_nocache(SMC_MEM_START, SMC_MEM_LEN);
	if (!mydev->mappedIOMem)
	{
		ERR("could not remap io-mem region for controlbus\n.");
		err = -2;
		goto err_ioremap;
	}
	mydev->IOMemSize = SMC_MEM_LEN;

	// activate smc chip select NCS
	//	at91_set_gpio_output(AT91_PIN_PC11, 1); // Activate as Output
	//	at91_set_B_periph(AT91_PIN_PC11, 0);    // Disable SPI0 CS Function
	//	at91_set_A_periph(AT91_PIN_PC11, 1);    // Activate NCS2 Function
	at91_set_A_periph(AT91_PIN_PC13, 1); // Activate NCS2 Function // HESS

	at91_set_A_periph(AT91_PIN_PC2, 1); // Activate A19 // HESS
	at91_set_A_periph(AT91_PIN_PC3, 1); // Activate A20 // HESS
	at91_set_A_periph(AT91_PIN_PC4, 1); // Activate A21 // HESS
	at91_set_A_periph(AT91_PIN_PC5, 1); // Activate A22 // HESS
	at91_set_A_periph(AT91_PIN_PC6, 1); // Activate A23 // HESS
	at91_set_A_periph(AT91_PIN_PC7, 1); // Activate A24 // HESS
	at91_set_A_periph(AT91_PIN_PC12, 1); // Activate A25 // HESS

	// Activate additional address lines needed for smc
	//    at91_set_gpio_output(AT91_PIN_PC5, 1); // Activate as Output
	//	at91_set_B_periph(AT91_PIN_PC5, 0);    // Disable SPI1_NPCS1 Function
	//	at91_set_A_periph(AT91_PIN_PC5, 1);    // Activate A24 Function

	//	at91_set_gpio_output(AT91_PIN_PC4, 1); // Activate as Output
	//	at91_set_B_periph(AT91_PIN_PC4, 0);    // Disable SPI1_NPCS2 Function
	//	at91_set_A_periph(AT91_PIN_PC4, 1);    // Activate A23 Function

	//	at91_set_gpio_output(AT91_PIN_PC10, 1); // Activate as Output
	//  at91_set_B_periph(AT91_PIN_PC10, 0);    // Disable UART_CTS3 Function
	//    at91_set_A_periph(AT91_PIN_PC10, 1);    // Activate A53 Function

	// activate ARM Bus Access and deactivate NIOS Bus access
	//	at91_set_gpio_output(CTRL_FPGA_ARM_BUS_ENABLE_PIN, 0); // Activate as Output and set level to GND

	// activate ARM Bus Access and deactivate NIOS Bus access

	//	at91_set_gpio_output(AT91_PIN_PB22, 0); // Activate as Output and set level to GND
	//	at91_set_gpio_output(AT91_PIN_PB23, 0); // Activate as Output and set level to GND
	//	at91_set_gpio_output(AT91_PIN_PB24, 0); // Activate as Output and set level to GND
	//	at91_set_gpio_output(AT91_PIN_PB25, 0); // Activate as Output and set level to GND
	//	at91_set_gpio_output(AT91_PIN_PB26, 0); // Activate as Output and set level to GND
	//	at91_set_gpio_output(AT91_PIN_PB27, 0); // Activate as Output and set level to GND
	//	at91_set_gpio_output(AT91_PIN_PB28, 0); // Activate as Output and set level to GND
	//	at91_set_gpio_output(AT91_PIN_PB29, 0); // Activate as Output and set level to GND
	//	at91_set_gpio_output(AT91_PIN_PB30, 0); // Activate as Output and set level to GND
	//	at91_set_gpio_output(AT91_PIN_PB31, 0); // Activate as Output and set level to GND

#if TEST_CODE==1
	// Test Controller Reconfigure
	// configure nCOnfig Pin
	at91_set_gpio_output(FPGA_nCONFIG, 0);// Activate as Output and set level to VCC
#endif
	// configure nCOnfig Pin
	at91_set_gpio_output(FPGA_nCONFIG, 1); // Activate as Output and set level to VCC

	// TODO: PB21-PB31 -> Directly Connected to FPGA, not used yet

	// Configure PB16, nCRC Error of controller (0==CRC ERROR)
	//	at91_set_gpio_input(CTRL_FPGA_NCRC_PIN, 0); // Activate as Input, no pullup

	// Configure PB17, nCRC Error of slaves (or'ed) (0==CRC ERROR)
	//	at91_set_gpio_input(SLAVES_FPGA_NCRC_PIN, 0); // Activate as Input, no pullup

	// apply the timing to
	ctrlbus_apply_timings();

	//	initTestPins();

	// wait until the timing configuration took effect, without waiting control bus operations will fail at this stage
	//msleep(500); // ## ??
	msleep(10); // ## ??

#if IRQ_ENABLED==1
	if (installSRQHandler())
	{
		ERR("could not install irq handler\n.");
		err = -3;
		goto err_irq_install;
	}
#endif
	//msleep(100); // ## ??

	// initialize semaphore for the device access
	sema_init(&mydev->sem, 1);

	return 0;

	// Error Handling
err_irq_install: iounmap(mydev->mappedIOMem);
err_ioremap: release_mem_region(SMC_MEM_START, SMC_MEM_LEN);
err_memregion: class_destroy(mydev->sysfs_class);

err_sysclass:

	return err;
}

static int hess_smc_destroy(hess_dev_t* mydev)
{
#if IRQ_ENABLED==1
	uninstallSRQHandler();
#endif
	iounmap(mydev->mappedIOMem);
	release_mem_region(SMC_MEM_START, SMC_MEM_LEN);
	class_destroy(mydev->sysfs_class);

	return 0;
}

// Initialization of the Character Device
static int hess_drv_create(hess_dev_t* mydev)
{
	int err;

	// initialize test pins
	// configure nCOnfig Pin
	at91_set_gpio_output(TEST_PIN_1, 0);
	at91_set_gpio_output(TEST_PIN_2, 0);
	at91_set_gpio_output(TEST_PIN_3, 0);
	at91_set_gpio_output(TEST_PIN_4, 0);

	if ((err = hess_smc_create(mydev))) return err; // printing will be done in  hess_smc_create

#if	BUS_DEVICE_ENABLED==1
	if ((err = smc_chrdrv_create(mydev))) return err;
#endif

#if	FPGA_DEVICE_ENABLED==1
	err = fpga_chrdrv_create(mydev);
	if (err)
	{
		ERR("error initialzing fpga char device\n");
		hess_smc_destroy(mydev);
		return err;
	}
#endif

#if IRQ_ENABLED==1
#if DAQ_DEVICE_ENABLED==1
	err = daq_chrdrv_create(mydev);
	if (err)
	{
		ERR("error initialzing daq char device\n");

#if	BUS_DEVICE_ENABLED==1
		smc_chrdrv_destroy(mydev);
#endif
		hess_smc_destroy(mydev);
		return err;
	}
#endif  /*#if DAQ_DEVICE_ENABLED==1*/
#endif /*#if IRQ_ENABLED==1*/

	return 0;
}

static int hess_drv_destroy(hess_dev_t* mydev)
{
#if	DAQ_DEVICE_ENABLED==1
	daq_chrdrv_destroy(mydev);
#endif

#if	FPGA_DEVICE_ENABLED==1
	fpga_chrdrv_destroy(mydev);
#endif

#if	BUS_DEVICE_ENABLED==1
	smc_chrdrv_destroy(mydev);
#endif
	hess_smc_destroy(mydev);

	return 0;
}


static ssize_t debug_stats_read(struct file *fp, char __user *user_buffer, size_t count, loff_t *position)
{
	static int nMaxFileSize = 0;
	loff_t pos = *position;
	long diff_mHz;

	if (g_srqDevice.debug_data.diff_us > 0) { diff_mHz = 1000000000 / g_srqDevice.debug_data.diff_us; }
	else { diff_mHz = 0; }

	long triggerRate_mHz;
	if (g_srqDevice.debug_data.triggerRate > 0) { triggerRate_mHz = 1000000000 / g_srqDevice.debug_data.triggerRate; }
	else { triggerRate_mHz = 0; }

	long unsigned trigger_counter = smc_bus_read16(OFFS_TRIGGER_COUNTER_H) * 65536 + smc_bus_read16(OFFS_TRIGGER_COUNTER_L);
	u16 resetTime = smc_bus_read16(OFFS_TRIGGER_COUNTER_RESET_TIME);
	long unsigned rate_scale = 0;
	long unsigned irq_rate_mHz = 0;
	long unsigned irq_rate_us = 0;
	long unsigned irq_rate_Hz = 0;
	long unsigned averagingTime = 0;
	size_t len = 0;

	/*if (nMaxFileSize)
	{
	count = count>
	}*/

	if (nMaxFileSize && *position >= nMaxFileSize)return 0;

	if (resetTime > 0)
	{
		rate_scale = TRIGGER_COUNTER_RESET_TIME_SEC * 1000 / resetTime;
		if (rate_scale > 0)
		{
			averagingTime = 1000000 / rate_scale;
			irq_rate_mHz = trigger_counter * rate_scale;
			if (irq_rate_mHz > 0)
			{
				irq_rate_us = 1000000000 / irq_rate_mHz;
				irq_rate_Hz = irq_rate_mHz / 1000;
			}
		}
	}

	//len += snprintf(s_ker_buf + len, HESS_DEBUG_KERNEL_BUFFER_LENGTH - len, "irq rate:                 %luus == %lu,%03luHz\n", g_srqDevice.debug_data.diff_us, diff_mHz/1000, diff_mHz%1000);
	len += snprintf_to_userGen(user_buffer + len, count - len, "irq rate:                 %luus == %lu,%03luHz\n", g_srqDevice.debug_data.diff_us, diff_mHz / 1000, diff_mHz % 1000);
	len += snprintf_to_userGen(user_buffer + len, count - len, "trigger rate software:    %luus == %lu,%03luHz\n", g_srqDevice.debug_data.triggerRate, triggerRate_mHz / 1000, triggerRate_mHz % 1000);
	len += snprintf_to_userGen(user_buffer + len, count - len, "trigger rate hardware:    %luus == %lu,%03luHz (avg@%lums)\n", irq_rate_us, irq_rate_Hz, irq_rate_mHz % 1000, averagingTime);
	len += snprintf_to_userGen(user_buffer + len, count - len, "total_irqs:               %u\n", g_srqDevice.debug_data.total_irq_count);
	len += snprintf_to_userGen(user_buffer + len, count - len, "max words in fifo:        %u\n", g_srqDevice.debug_data.max_fifo_count);
	len += snprintf_to_userGen(user_buffer + len, count - len, "words in fifo:            %u\n", g_srqDevice.debug_data.fifo_count);
	len += snprintf_to_userGen(user_buffer + len, count - len, "words in fifo:            %u\n", smc_bus_read16(BASE_NECTAR_ADC + OFFS_EVENT_FIFO_WORD_COUNT));
	len += snprintf_to_userGen(user_buffer + len, count - len, "eventcounter mismatches:  %u \t%u +1 != %u\n", g_srqDevice.debug_data.eventcounter_mismatch, g_srqDevice.debug_data.eventcounter_mismatch_old, g_srqDevice.debug_data.eventcounter_mismatch_new);
	//	len += snprintf_to_userGen(user_buffer+len,count-len,"length_mismatch:          %u\n", g_srqDevice.debug_data.length_mismatch);
	len += snprintf_to_userGen(user_buffer + len, count - len, "times sync losts:         %u\n", g_srqDevice.debug_data.sync_lost);
	len += snprintf_to_userGen(user_buffer + len, count - len, "fifo unexpected empty:    %u\n", g_srqDevice.debug_data.fifo_empty);
	len += snprintf_to_userGen(user_buffer + len, count - len, "bytes droped:             %u\n", g_srqDevice.debug_data.bytes_droped);
	len += snprintf_to_userGen(user_buffer + len, count - len, "current eventcounter:     %u\n", g_srqDevice.debug_data.eventcounter);
	len += snprintf_to_userGen(user_buffer + len, count - len, "samples lost (fifo full): %u\n", smc_bus_read16(BASE_NECTAR_ADC + OFFS_FIFO_FULL_COUNT));
	len += snprintf_to_userGen(user_buffer + len, count - len, "frame 15:                 %u\n", g_srqDevice.debug_data.frame_15);
	len += snprintf_to_userGen(user_buffer + len, count - len, "frame 16:                 %u\n", g_srqDevice.debug_data.frame_16);
	len += snprintf_to_userGen(user_buffer + len, count - len, "frame 17:                 %u\n", g_srqDevice.debug_data.frame_17);
	len += snprintf_to_userGen(user_buffer + len, count - len, "frame 32:                 %u\n", g_srqDevice.debug_data.frame_32);
	len += snprintf_to_userGen(user_buffer + len, count - len, "frame other:              %u\n", g_srqDevice.debug_data.frame_other);
	len += snprintf_to_userGen(user_buffer + len, count - len, "last frame length was:    %u\n", g_srqDevice.debug_data.frame_length);
	len += snprintf_to_userGen(user_buffer + len, count - len, "unknown_type:             %u\n", g_srqDevice.debug_data.unknown_type);
	//len += snprintf(s_ker_buf + len, HESS_DEBUG_KERNEL_BUFFER_LENGTH - len, "chars left:~%u\n", HESS_DEBUG_KERNEL_BUFFER_LENGTH-len-20);
	//len += snprintf_to_userGen(user_buffer+len,count-len,"chars left:~%u\n", HESS_DEBUG_KERNEL_BUFFER_LENGTH-len-20);

	if (!nMaxFileSize){ nMaxFileSize = len; }

	*position = pos + count;

	//return simple_read_from_buffer(user_buffer, count, position, s_ker_buf, min((size_t)HESS_DEBUG_KERNEL_BUFFER_LENGTH,len));
	return len;
}


static const struct file_operations fops_debug_stats =
{
	.read = debug_stats_read,
};




static int hess_debugfs_init(void)
{
	struct dentry *fileret;
	struct dentry *u64int;
	struct dentry *u64hex;
	//printk("\n\nhess_debugfs_init()\n\n");
	s_dirrent = debugfs_create_dir("hess", NULL); // create a directory by the name dell in /sys/kernel/debugfs
	fileret = debugfs_create_file("stats", 0644, s_dirrent, &s_filevalue, &fops_debug_stats);// create a file in the above directory This requires read and write file operations
	if (!fileret)
	{
		printk("error creating stats file");
		return (-ENODEV);
	}

	u64int = debugfs_create_u64("number", 0644, s_dirrent, &s_intvalue);// create a file which takes in a int(64) value
	if (!u64int)
	{
		printk("error creating int file");
		return (-ENODEV);
	}

	u64hex = debugfs_create_x64("hexnum", 0644, s_dirrent, &s_hexvalue); // takes a hex decimal value
	if (!u64hex)
	{
		printk("error creating hex file");
		return (-ENODEV);
	}

	return 0;
}


static void hess_debugfs_remove(void)
{
	debugfs_remove_recursive(s_dirrent);
}


#include <asm/dma.h>
#include <linux/dmaengine.h>


/*static*/ bool filter2222(struct dma_chan *chan, void *param)
{
	int nParam = (int)param;

	printk(KERN_ALERT "param = %p, nParam = %d\n", param, nParam);

	//return true;

	switch (nParam)
	{
	case 0:
		//return dma_has_cap(DMA_CYCLIC, chan->device->cap_mask) ? true : false;
		return dma_has_cap(DMA_SLAVE, chan->device->cap_mask) && dma_has_cap(DMA_PRIVATE, chan->device->cap_mask) ? true : false;

	default:
		return false;
	}
}




#define DMA_TEST_SIZE 16
#define ROW_SIZE	8
#define	REAL_DEVICE

#define PRINT_MEMORY(memory,Nsize) \
{ \
	char* pcMemory = (char*)memory; \
	int nSizeMin1 = (int)Nsize - 1; \
	int i = 1; \
	printk(KERN_ALERT "%s[0-%d] = [%d",#memory,nSizeMin1+1,(int)pcMemory[0]); \
	if(nSizeMin1) \
		{ \
		printk(KERN_CONT ", "); \
		} \
	for(;i<nSizeMin1;++i) \
		{ \
		printk(KERN_CONT "%d, ",(int)pcMemory[i]); \
		if(i%ROW_SIZE == 0) \
				{ \
			printk(KERN_CONT "\n"); \
				} \
		} \
	if(nSizeMin1) \
		{ \
		printk(KERN_CONT "%d",(int)pcMemory[nSizeMin1]); \
		} \
	printk(KERN_CONT "]\n"); \
}

static void dmatest_callback(void *completion)
{
	//complete(completion);
}

//struct dma_ops dma_fops;
#define DO_DEBUG
#include <mach/at_hdmac.h>
//#include <../drivers/dma/at_hdmac_regs.h>
extern dma_cookie_t atc_tx_submit2222(struct dma_async_tx_descriptor *tx);
//static struct at_dma_slave aAtSlave;
static struct at_dma_slave* s_pAtSlave;

// Initialize the Kernel Module
static int __init hess_init(void)
{

#ifdef DO_DEBUG

#if defined(__ASM_ARM_DMA_H)
	//printk(KERN_ALERT "__ASM_ARM_DMA_H\n");
	//return -1;
#endif // Correct header is "arch/arm/include/asm/dma.h"

	dma_cap_mask_t mask;
	struct dma_chan *chan;
	char *pDst;
	dma_addr_t	dmaSrc = 0, dmaDst = 0;
	struct dma_async_tx_descriptor *tx = NULL;
	enum dma_ctrl_flags 	flags;
	int nRet = -1;
	dma_cookie_t		cookie;
	enum dma_status		status;
#ifdef REAL_DEVICE
	struct resource *iomemregion;
	void* mappedIOMem;
#else
	char *pSrc;
#endif
	//struct dma_slave_config aConfig;
	struct scatterlist sg_rx;
	//struct at_dma_slave aAtSlave;
	//struct at_dma		*atdma; //???

	printk(KERN_ALERT "43;  ENXIO = %d, MAX_DMA_ADDRESS = 0x%x\n", ENXIO, (unsigned int)MAX_DMA_ADDRESS);

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);
	dma_cap_set(DMA_PRIVATE, mask);

	chan = dma_request_channel(mask, filter2222, NULL);
	printk(KERN_ALERT "chan = %p\n", chan);
	if (!chan)
	{
		printk(KERN_ERR "No channel!!!\n");
		return -1;
	}

	//chan->device->device_control(chan, DMA_SLAVE_CONFIG, 1);
	//printk(KERN_ALERT "dma_has_cap(DMA_CYCLIC, chan->device->cap_mask) = %d\n", dma_has_cap(DMA_CYCLIC, chan->device->cap_mask));
	printk(KERN_ALERT "chan->client_count = %d\n", (int)chan->client_count);
	printk(KERN_ALERT "DMA channel name: %s\n", dma_chan_name(chan));
	printk(KERN_ALERT "chan->device->device_prep_dma_memcpy = %p\n", chan->device->device_prep_dma_memcpy);

#ifdef REAL_DEVICE
	iomemregion = request_mem_region(SMC_MEM_START, SMC_MEM_LEN, IOMEM_NAME);
	if (!iomemregion)
	{
		nRet = -2;
		ERR("could not request io-mem region for controlbus\n.");
		goto errDMArelChannel;
	}

	mappedIOMem = ioremap_nocache(SMC_MEM_START, SMC_MEM_LEN);
	if (!mappedIOMem)
	{
		nRet = -3;
		ERR("could not remap io-mem region for controlbus\n.");
		goto errRelMemRegion;
	}

	//dmaSrc = virt_to_bus((char*)mappedIOMem + BASE_NECTAR_ADC + OFFS_EVENT_FIFO);
	//printk(KERN_ALERT "dmaSrc = %lu, dmaSrc = %lx, SMC_MEM_START = %lu\n", (unsigned long)dmaSrc, (unsigned long)dmaSrc, (unsigned long)SMC_MEM_START);
	dmaSrc = SMC_MEM_START + BASE_NECTAR_ADC + OFFS_EVENT_FIFO;
#else
	pSrc = kzalloc(DMA_TEST_SIZE, GFP_DMA | GFP_KERNEL);
	printk(KERN_ALERT "pSrc = %p\n", pSrc);
	if (!pSrc)
	{
		nRet = -2;
		goto errDMArelChannel;
	}
	memset(pSrc, 1, DMA_TEST_SIZE);

	dmaSrc = dma_map_single(chan->device->dev, pSrc, DMA_TEST_SIZE, DMA_TO_DEVICE);
	printk(KERN_ALERT "dmaSrc = %lu\n", (unsigned long)dmaSrc);
	if (!dmaSrc)
	{
		nRet = -3;
		goto errRelMemRegion;
	}
#endif


	////////////////////////////////////////////////

	pDst = kzalloc(DMA_TEST_SIZE, GFP_DMA | GFP_KERNEL);
	if (!pDst)
	{
		nRet = -3;
		printk(KERN_ERR "No memory!\n");
		goto errIOunmap;
	}

	PRINT_MEMORY(pDst, 16);

	//dmaDst = dma_map_single(chan->device->dev, pDst, DMA_TEST_SIZE, DMA_FROM_DEVICE);
	sg_init_one(&sg_rx, pDst, DMA_TEST_SIZE);
	//sg_init_table(&sg_rx, 1);
	dmaDst = dma_map_single(chan->device->dev, pDst, DMA_TEST_SIZE, DMA_FROM_DEVICE);
	printk(KERN_ALERT "dmaDst = %lu\n", (unsigned long)dmaDst);
	if (!dmaDst)
	{
		nRet = -4;
		goto errKfree;
	}
	sg_dma_address(&sg_rx) = dmaDst;
	sg_dma_len(&sg_rx) = DMA_TEST_SIZE;

	//tx = chan->device->device_prep_dma_memcpy(chan, dmaDst, dmaSrc, DMA_TEST_SIZE, flags);


	///////////////////////////////////////////////////////////////////////////////////////////
	/*memset(&aConfig, 0, sizeof(struct dma_slave_config));
	aConfig.direction = DMA_FROM_DEVICE;
	aConfig.src_addr = dmaSrc;
	aConfig.dst_addr = dmaDst;
	aConfig.src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	aConfig.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	aConfig.src_maxburst = 2;
	aConfig.dst_maxburst = 2;
	nRet = dmaengine_slave_config(chan, &aConfig);
	printk(KERN_ALERT "nRet = %d, chan->device->device_prep_slave_sg = %p\n", nRet, chan->device->device_prep_slave_sg);*/

	s_pAtSlave = kzalloc(sizeof(struct at_dma_slave), GFP_KERNEL);
	//memset(s_pAtSlave, 0, sizeof(struct at_dma_slave));
	s_pAtSlave->dma_dev = chan->device->dev;
	//atdma = to_at_dma(chan->device); //???
	//aAtSlave.dma_dev = atdma->dma_common.dev;
	//printk(KERN_ALERT "atdma->dma_common.dev = %p, chan->device->dev = %p\n", atdma->dma_common.dev,chan->device->dev);
	s_pAtSlave->rx_reg = dmaSrc;
	s_pAtSlave->reg_width = AT_DMA_SLAVE_WIDTH_16BIT;
	//printk(KERN_ALERT "pAtSlave = %p\n", pAtSlave);
	//goto errDmaUnmapSingle;
	chan->private = s_pAtSlave;

	if (!chan->device->device_prep_slave_sg)
	{
		nRet = -5;
		goto errDmaUnmapSingle;
	}

	flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT
		| DMA_COMPL_SKIP_DEST_UNMAP | DMA_COMPL_SKIP_SRC_UNMAP;

	//tx = chan->device->device_prep_slave_sg(chan, &sg_rx, 1, DMA_DEV_TO_MEM, DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	tx = chan->device->device_prep_slave_sg(chan, &sg_rx, 1, DMA_TO_DEVICE, flags);
	printk(KERN_ALERT "tx = %p\n", tx);
	//goto errDmaUnmapSingle;

	if (!tx)
	{
		nRet = -6;
		printk(KERN_ERR "prep_dma_slave_sg error!\n");
		goto errDmaUnmapSingle;
	}

	tx->callback = dmatest_callback;
	//goto errDmaUnmapSingle;
	//cookie = dmaengine_submit(tx);
	//printk(KERN_ALERT "!!!!!!!!!!!!!!chan = %p\n",chan);
	atc_tx_submit2222(tx);
	goto errDmaUnmapSingle;

	if (dma_submit_error(cookie)) {
		nRet = -7;
		printk(KERN_ERR "submit error\n");
		//msleep(100);
		//failed_tests++;
		//continue;
		goto errDmaUnmapSingle;
	}

	dma_async_issue_pending(chan);

	status = dma_async_is_tx_complete(chan, cookie, NULL, NULL);

	if (status != DMA_SUCCESS)
	{
		printk(KERN_ALERT "Waiting 100ms\n");
		msleep(150);
		status = dma_async_is_tx_complete(chan, cookie, NULL, NULL);

		if (status != DMA_SUCCESS)
		{
			dmaengine_terminate_all(chan);
			printk(KERN_ERR "DMA didn't finish!\n");
			nRet = -8;
			goto errDmaUnmapSingle;
		}
	}
	else
	{
		printk(KERN_ALERT "Ready imediatly!\n");
	}

	//PRINT_MEMORY(pDst, 16);

	/////////////////////////////////////////
errDmaUnmapSingle:
	dma_unmap_single(chan->device->dev, dmaDst, DMA_TEST_SIZE, DMA_FROM_DEVICE);
errKfree:
	PRINT_MEMORY(pDst, 16);
	kfree(pDst);
#ifdef REAL_DEVICE
errIOunmap :
	iounmap(mappedIOMem);
errRelMemRegion:
	release_mem_region(SMC_MEM_START, SMC_MEM_LEN);
#else
errIOunmap:
	dma_unmap_single(chan->device->dev, dmaSrc, DMA_TEST_SIZE, DMA_TO_DEVICE);
errRelMemRegion:
	kfree(pSrc);
#endif
errDMArelChannel:
	dma_release_channel(chan);

	return nRet;

#endif  /* #if DO_DEBUG */

	INFO("++++++sizeof(hess_ring_buffer_t) = %d, PAGE_SIZE = %d, sizeof(hess_sample_data_debug_t) = %d\n",
		sizeof(hess_ring_buffer_t), (int)PAGE_SIZE, (int)sizeof(hess_sample_data_debug_t));
	INFO("*** Hess Drawer - Driver Version '" DRIVER_VERSION " compiled at " __DATE__ " Time:" __TIME__ "' loaded ***\n");

	INFO("!!!!!! s_nPossibleDMA = %d\n", s_nPossibleDMA);

#if TEST_CODE==1
	WRN("Test Code Activated! DO NOT USE THIS DRIVER with a productive system\n");
#endif

	int ret = 0;

	ret = hess_debugfs_init();
	if (ret != 0) return ret;

	ret = hess_drv_create(&g_my_device_instance);
	if (ret != 0) return ret;

	INFO("success!\n");

	return ret;
}

// Deinitialize the Kernel Module
static void __exit hess_exit(void)
{
#ifdef DO_DEBUG
	return;
#endif
	hess_debugfs_remove(); // removing the directory recursively which in turn cleans all the files
	hess_drv_destroy(&g_my_device_instance);
	INFO("*** Hess Drawer - Version '" DRIVER_VERSION " compiled at " __DATE__ " Time:" __TIME__ "' unloaded ***\n");
}

module_init(hess_init);
module_exit(hess_exit);
