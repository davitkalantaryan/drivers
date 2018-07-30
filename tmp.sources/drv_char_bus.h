/***************************************************************************** 
 * File		  smc_chrdrv.h
 * created on 09.02.2012
 *****************************************************************************
 * Author:	M.Eng. Dipl.-Ing(FH) Marek Penno, EL/1L23, Tel:033762/77275 marekp
 * Email:	marek.penno@desy.de
 * Mail:	DESY, Platanenallee 6, 15738 Zeuthen
 *****************************************************************************
 * Description
 * 
 ****************************************************************************/

#ifndef SMC_CHRDRV_H_
#define SMC_CHRDRV_H_

#include <linux/mm.h>
#include <linux/version.h>

// ----------------------------------- Character Device Driver Interface ------------------

// Char Driver Implementation

static int smc_chrdrv_open(struct inode * node, struct file *f)
{
//	DBG("user_open\n");

	hess_dev_t* dev;

	dev = container_of(node->i_cdev, hess_dev_t, bus_driver.cdev);
	f->private_data = dev;

	return 0;
}

static int smc_chrdrv_release(struct inode *node, struct file *f)
{
//	DBG("user_release\n");
	return 0;
}

static ssize_t smc_chrdrv_read(struct file *f, char __user *buf, size_t size, loff_t *offs)
{
	ssize_t retVal=0;

	hess_dev_t* dev=f->private_data;
	unsigned int myoffs=dev->offset;

//	DBG("user_read offs: 0x%p + 0x%x + 0x%x = 0x%x size:%d\n",dev->mappedIOMem, dev->offset, (int)*offs, myoffs,size);

	// lock the resource
	if (down_interruptible(&dev->sem)) return -ERESTART;

	// Check File Size
	if ((myoffs) >= dev->IOMemSize) goto out;

	// Limit Count
	if ((myoffs + size) >= dev->IOMemSize ) size = dev->IOMemSize - myoffs;

	retVal=copy_from_io_to_user_16bit(buf, VOID_PTR(dev->mappedIOMem, myoffs), size);
	if (retVal<0)
	{
		goto out;
	}

	*offs+=retVal; // Is this realy needed?

	out:
	// free the resource
	up(&dev->sem);
	return retVal;
}

static ssize_t smc_chrdrv_write(struct file *f, const char __user *buf, size_t size, loff_t *offs)
{
	ssize_t retVal=0;
	hess_dev_t* dev=f->private_data;

	//unsigned int myoffs=dev->offset + *offs;
	unsigned int myoffs=dev->offset;
	//DBG("user_write offs: 0x%x  size:0x%d\n",myoffs, size);

	// lock the resource
	if (down_interruptible(&dev->sem)) return -ERESTART;

	// Check File Size
//        if ((myoffs + dev->offset) >= dev->IOMemSize) goto out;
	if ((myoffs) >= dev->IOMemSize) goto out;

	// Limit Count
	if ((myoffs + size) >= dev->IOMemSize ) size = dev->IOMemSize - myoffs;

	retVal=copy_from_user_to_io_16bit(VOID_PTR(dev->mappedIOMem, myoffs), buf, size);
	if (retVal<0)
	{
		// fault occured, write was not completed
		goto out;
	}

	*offs+=retVal;
	out:
	// free the resource
	up(&dev->sem);
	return retVal;
}

static loff_t smc_chrdrv_llseek(struct file *f, loff_t offs, int whence)
{
	hess_dev_t* dev = f->private_data;
	unsigned int myoffs = offs;
//	DBG("user_llseek offs: 0x%x  whence: %d\n", myoffs, whence);
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

typedef struct
{
	int usage_count;
	hess_dev_t* mydev;
} smc_private_data_t;

//static void smc_vma_open(struct vm_area_struct *vma)
//{
//	smc_private_data_t* data = vma->vm_private_data;
//	data->usage_count++;
//}
//
//static void smc_vma_close(struct vm_area_struct *vma)
//{
//	smc_private_data_t* data = vma->vm_private_data;
//	data->usage_count--;
//}
//
//static int smc_vma_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
//{
//	DBG("handling fault\n");
//	smc_private_data_t* data = vma->vm_private_data;
//	char* pagePtr = NULL;
//	struct page *page;
//	unsigned long offset;
//
//	offset = (((unsigned long) vmf->virtual_address) - vma->vm_start) + (vma->vm_pgoff << PAGE_SHIFT);
//
//	DBG(" virtua.addr: 0x%x offset: 0x%x  vm_start:0x%x  vm_pgoff:%d\n", (unsigned int) vmf->virtual_address, (unsigned int) offset, (unsigned int) vma->vm_start, (unsigned int) vma->vm_pgoff);
//
//	if (offset >= SMC_MEM_LEN) return VM_FAULT_SIGBUS;
//
//	DBG(" virtua.addr bus: 0x%x \n", (unsigned int) data->mydev->mappedIOMem);
//	pagePtr = (void*) (((addr_t) data->mydev->mappedIOMem) + offset);
//
//	DBG("getting page\n");
////	return VM_FAULT_SIGBUS;
//
//	page = virt_to_page(pagePtr);
////	page = vmalloc_to_page(__va(pagePtr));
//	get_page(page);
//	vmf->page = page;
//
//	return 0;
//}
//
//static struct vm_operations_struct smc_remap_vm_ops =
//	{ .open = smc_vma_open,
//	  .close = smc_vma_close,
//	  .fault = smc_vma_fault };
//
//static smc_private_data_t smc_private_data =
//	{ .usage_count = 0,
//	  .mydev = &my_device_instance };
//
//static int smc_chrdrv_mmap(struct file *f, struct vm_area_struct *vma)
//{
//	unsigned long off = vma->vm_pgoff << PAGE_SHIFT;
//	unsigned long physical = SMC_MEM_START + off;
//	unsigned long vsize = vma->vm_end - vma->vm_start;
//	unsigned long psize = SMC_MEM_LEN - off;
//
//	DBG("user_mmap: phys:0x%x vsize:%d  psize:%d  off:%d\n", (unsigned int) physical, (unsigned int) vsize, (unsigned int) psize, (unsigned int) off);
//
//	if (vsize > psize)
//	{
//		return -EINVAL; // spans to high
//	}
//
//	// do not map here, let no page do the mapping
//	vma->vm_ops = &smc_remap_vm_ops;
//	vma->vm_flags |= VM_RESERVED;
//	vma->vm_private_data = &smc_private_data;
//
//	smc_vma_open(vma);
//
////	/* Remap-pfn-range will mark the range VM_IO and VM_RESERVED */
////	if (remap_pfn_range(vma,
////			    vma->vm_start,
////			    physical,
////			    vsize,
////			    vma->vm_page_prot)) {
////		ERR("Failed to remap memory!\n");
////		return -EAGAIN;
////	}
//
//	return 0;
//}

// ----------------------------------- IOCTL Interface ---------------------
// the char device support some special function via the ioctl command
// The current supported functions are:
//  - reboot controller fpga
//  - read back nCRC Flags of the system (corruption of the FPGA configuration)
// Further commands could be:
//  - enable/disable irq
//  - readback irq frequency
//  - enable snapshoot functions
//  - set memory regions of control bus with write protection

// ----------------------- Read Back Status IOCTL Command ------------------
#include "../../libs/hal/hess1u/common/hal/commands.h"

// ----------------------- Reboot FPGA IOCTL Command ------------------

// read back the status
//static int smc_ioctl_cmd_reboot_fgpa(unsigned long arg)
//{
//	smc_command_reboot_fpga_t cmd;
//
//	DBG("SMC IOCTL cmd reboot fpga\n");
//
//	if (copy_from_user(&cmd, (void*) arg, sizeof(cmd)))
//	{
//		// fault occured, write was not completed
//		return -EFAULT;
//	}
//
//	// check command size
//	if (cmd.size != sizeof(cmd))
//	{
//		ERR("invalid structure size %d", cmd.size);
//		return -EINVAL;
//	}
//	// check command id
//	if (cmd.command != SMC_COMMAND_REBOOT_FPGA)
//	{
//		ERR("invalid command id %d", cmd.command);
//		return -EINVAL;
//	}
//	// check command secure id
//	if (cmd.id != REBOOT_FPGA_CODE)
//	{
//		ERR("invalid security id 0x%x", cmd.id);
//		return -EINVAL;
//	}
//
//	// reboot controller fpga
//	// toggle nConfig Pin
//	at91_set_gpio_output(FPGA_nCONFIG, 0);
//	at91_set_gpio_output(FPGA_nCONFIG, 1);
//
//	return 0;
//}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
static int smc_chrdrv_ioctl(struct inode *node, struct file *f, unsigned int cmd, unsigned long arg)
#else
static long smc_chrdrv_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
#endif
{
	//intlk_dev_t* dev=f->private_data;
	switch (cmd)
	{
	case SMC_COMMAND_REBOOT_FPGA:
		//return smc_ioctl_cmd_reboot_fgpa(arg);
	default:
		ERR("invalid ioctl cmd: %d\n", cmd);
		return -EFAULT;
	}
	return 0;
}

// **************************************** Character Device Instance Definiton

// Char Driver File Operations
static struct file_operations smc_user_fops =
	{	.open = smc_chrdrv_open, // handle opening the file-node
		.release = smc_chrdrv_release, // handle closing the file-node
		.read = smc_chrdrv_read, // handle reading
		.write = smc_chrdrv_write, // handle writing
		.llseek = smc_chrdrv_llseek, // handle seeking in the file
		.unlocked_ioctl = smc_chrdrv_ioctl, // handle special i/o operations
//        .mmap = smc_chrdrv_mmap
		};

// Initialization of the Character Device
static int smc_chrdrv_create(hess_dev_t* mydev)
{
	int err = 0;
	char name[30];
	chrdrv_t* chrdrv = &mydev->bus_driver;

	INFO("Install Character Device\n");
	if (!mydev)
	{
		ERR("Null pointer argument!\n");
		goto err_dev;
	}

	/* Allocate major and minor numbers for the driver */
	err = alloc_chrdev_region(&chrdrv->devno, SMC_DEV_MINOR, SMC_DEV_MINOR_COUNT, SMC_DEV_NAME);
	if (err)
	{
		ERR("Error allocating Major Number for driver.\n");
		goto err_region;
	}

	DBG("Major Number: %d\n", MAJOR(chrdrv->devno));

	/* Register the driver as a char device */
	cdev_init(&chrdrv->cdev, &smc_user_fops);
	chrdrv->cdev.owner = THIS_MODULE;
//	DBG("char device allocated 0x%x\n",(unsigned int)chrdrv->cdev);
	err = cdev_add(&chrdrv->cdev, chrdrv->devno, SMC_DEV_MINOR_COUNT);
	if (err)
	{
		ERR("cdev_all failed\n");
		goto err_char;
	}
	DBG("Char device added\n");

	sprintf(name, "%s0", SMC_DEV_NAME);

	// create devices
	chrdrv->device = device_create(mydev->sysfs_class, NULL, MKDEV(MAJOR(chrdrv->devno), 0), NULL, name, 0);

	if (IS_ERR(chrdrv->device))
	{
		ERR("%s: Error creating sysfs device\n", DRIVER_NAME);
		err = PTR_ERR(chrdrv->device);
		goto err_class;
	}

	return 0;

	err_class: cdev_del(&chrdrv->cdev);
	err_char: unregister_chrdev_region(chrdrv->devno, SMC_DEV_MINOR_COUNT);
	err_region:

	err_dev: return err;
}

// Initialization of the Character Device
static void smc_chrdrv_destroy(hess_dev_t* mydev)
{
	chrdrv_t* chrdrv = &mydev->bus_driver;
	device_destroy(mydev->sysfs_class, MKDEV(MAJOR(chrdrv->devno), 0));

	/* Unregister device driver */
	cdev_del(&chrdrv->cdev);

	/* Unregiser the major and minor device numbers */
	unregister_chrdev_region(chrdrv->devno, SMC_DEV_MINOR_COUNT);
}

#endif /* SMC_CHRDRV_H_ */
