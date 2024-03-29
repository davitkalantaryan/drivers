#include <linux/module.h>
#include <linux/fs.h>	
#include <linux/sched.h>
#include <asm/uaccess.h>

#include "pciedev_ufn.h"
#include "pciedev_io.h"
#include "read_write_inline.h"


long     pciedev_ioctl_exp(struct file *filp, unsigned int *cmd_p, unsigned long *arg_p, struct pciedev_cdev * pciedev_cdev_m)
{
	unsigned int    cmd;
	unsigned long  arg;
	pid_t                cur_proc = 0;
	int                    minor = 0;
	int                    d_num = 0;
	int                    retval = 0;
	int                    err = 0;
	int				nUserValue;
	struct pci_dev* pdev;
	u_int                tmp_offset;
	u_int                tmp_data;
	u_int                tmp_cmd;
	u_int                tmp_reserved;
	int                   io_size;

	void                *address;
	u8                  tmp_data_8;
	u16                tmp_data_16;
	u32                tmp_data_32;

	device_ioc_rw  aIocRW;
	device_vector_rw	aVectorRW;
	device_ioc_rw* __user	pUserRWdata;
	int i, nMaxNumRW;
	const size_t unSizeOfRW = sizeof(device_ioc_rw);

	device_ioctrl_data  data;
	struct pciedev_dev       *dev = filp->private_data;

	cmd = *cmd_p;
	arg = *arg_p;
	io_size = sizeof(device_ioctrl_data);
	minor = dev->dev_minor;
	d_num = dev->dev_num;
	cur_proc = current->group_leader->pid;
	pdev = (dev->pciedev_pci_dev);

	if (!dev->dev_sts){
		printk("PCIEDEV_IOCTRL: NO DEVICE %d\n", dev->dev_num);
		retval = -EFAULT;
		return retval;
	}

	DEBUGNEW("\n");

	/*
	* the direction is a bitmask, and VERIFY_WRITE catches R/W
	* transfers. `Type' is user-oriented, while
	* access_ok is kernel-oriented, so the concept of "read" and
	* "write" is reversed
	*/
	if (_IOC_DIR(cmd) & _IOC_WRITE)
	{
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
		if (err){ ERRCT("Bad pointer!\n"); }
	}
	else if (_IOC_DIR(cmd) & _IOC_READ)
	{
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
		if (err){ ERRCT("Bad pointer!\n"); }
	}
	if (err) return -EFAULT;

	//if (mutex_lock_interruptible(&dev->dev_mut))return -ERESTARTSYS;
	//if (EnterCritRegion(&dev->dev_mut)){ return -ERESTARTSYS; } /// new  // removed to the cases

	switch (cmd) {
	case PCIEDEV_PHYSICAL_SLOT:
		if (EnterCritRegion(&dev->dev_mut)){ return -ERESTARTSYS; }
		retval = 0;
		if (copy_from_user(&data, (device_ioctrl_data*)arg, (size_t)io_size)) {
			retval = -EFAULT;
			//mutex_unlock(&dev->dev_mut);
			LeaveCritRegion(&dev->dev_mut); /// new
			return retval;
		}
		tmp_offset = data.offset;
		tmp_data = data.data;
		tmp_cmd = data.cmd;
		tmp_reserved = data.reserved;
		data.data = dev->slot_num;
		data.cmd = PCIEDEV_PHYSICAL_SLOT;
		if (copy_to_user((device_ioctrl_data*)arg, &data, (size_t)io_size)) {
			retval = -EFAULT;
			//mutex_unlock(&dev->dev_mut);
			LeaveCritRegion(&dev->dev_mut); /// new
			return retval;
		}
		LeaveCritRegion(&dev->dev_mut);
		break;
	case PCIEDEV_DRIVER_VERSION:
		if (EnterCritRegion(&dev->dev_mut)){ return -ERESTARTSYS; }
		data.data = pciedev_cdev_m->PCIEDEV_DRV_VER_MAJ;
		data.offset = pciedev_cdev_m->PCIEDEV_DRV_VER_MIN;
		if (copy_to_user((device_ioctrl_data*)arg, &data, (size_t)io_size)) {
			retval = -EFAULT;
			//mutex_unlock(&dev->dev_mut);
			LeaveCritRegion(&dev->dev_mut); /// new
			return retval;
		}
		LeaveCritRegion(&dev->dev_mut);
		break;
	case PCIEDEV_FIRMWARE_VERSION:
		if (EnterCritRegion(&dev->dev_mut)){ return -ERESTARTSYS; }
		data.data = dev->brd_info_list.PCIEDEV_BOARD_VERSION;
		if (copy_to_user((device_ioctrl_data*)arg, &data, (size_t)io_size)) {
			retval = -EFAULT;
			//mutex_unlock(&dev->dev_mut);
			LeaveCritRegion(&dev->dev_mut); /// new
			return retval;
		}
		LeaveCritRegion(&dev->dev_mut);
		break;
	case PCIEDEV_GET_STATUS:
		if (EnterCritRegion(&dev->dev_mut)){ return -ERESTARTSYS; }
		data.data = dev->dev_sts;
		if (copy_to_user((device_ioctrl_data*)arg, &data, (size_t)io_size)) {
			retval = -EFAULT;
			//mutex_unlock(&dev->dev_mut);
			LeaveCritRegion(&dev->dev_mut); /// new
			return retval;
		}
		LeaveCritRegion(&dev->dev_mut);
		break;
	case PCIEDEV_SCRATCH_REG:
		if (EnterCritRegion(&dev->dev_mut)){ return -ERESTARTSYS; }
		retval = 0;
		if (copy_from_user(&data, (device_ioctrl_data*)arg, (size_t)io_size)) {
			retval = -EFAULT;
			//mutex_unlock(&dev->dev_mut);
			LeaveCritRegion(&dev->dev_mut); /// new
			return retval;
		}
		tmp_offset = data.offset;
		tmp_data = data.data;
		tmp_cmd = data.cmd;
		if (tmp_cmd == 1){
			data.data = dev->scratch_bar;
			data.offset = dev->scratch_offset;
		}
		if (tmp_cmd == 0){
			dev->scratch_bar = data.data;
			dev->scratch_offset = data.offset;
		}
		if (tmp_cmd == 2){
			//address = (void *)dev->memmory_base[dev->scratch_bar];
			address = pciedev_get_baraddress(dev->scratch_bar, dev);
			if (!address){
				retval = -EFAULT;
				//mutex_unlock(&dev->dev_mut);
				LeaveCritRegion(&dev->dev_mut); /// new
				return retval;
			}

			switch (data.offset){
			case RW_D8:
				tmp_data_8 = ioread8(address + dev->scratch_offset);
				data.data = tmp_data_8;
				break;
			case RW_D16:
				tmp_data_16 = ioread16(address + dev->scratch_offset);
				data.data = tmp_data_16;
				break;
			case RW_D32:
				tmp_data_32 = ioread32(address + dev->scratch_offset);
				data.data = tmp_data_32;
				break;
			}
		}
		if (copy_to_user((device_ioctrl_data*)arg, &data, (size_t)io_size)) {
			retval = -EFAULT;
			//mutex_unlock(&dev->dev_mut);
			LeaveCritRegion(&dev->dev_mut); /// new
			return retval;
		}
		LeaveCritRegion(&dev->dev_mut);
		break;
	case PCIEDEV_SET_SWAP:
		if (EnterCritRegion(&dev->dev_mut)){ return -ERESTARTSYS; }
		retval = 0;
		if (copy_from_user(&data, (device_ioctrl_data*)arg, (size_t)io_size)) {
			retval = -EFAULT;
			//mutex_unlock(&dev->dev_mut);
			LeaveCritRegion(&dev->dev_mut); /// new
			return retval;
		}
		dev->swap = data.data;
		LeaveCritRegion(&dev->dev_mut);
		break;

	case PCIEDEV_SET_SWAP2:
		if (EnterCritRegion(&dev->dev_mut)){ return -ERESTARTSYS; }
		if (copy_from_user(&nUserValue, (int*)arg, sizeof(int))) {
			//mutex_unlock(&dev->dev_mut);
			LeaveCritRegion(&dev->dev_mut); /// new
			return -EFAULT;
		}
		dev->swap = nUserValue ? 1 : 0;
		LeaveCritRegion(&dev->dev_mut);
		break;

	case PCIEDEV_GET_SWAP2:
		return dev->swap;


	/////////////////////////////////////////////////////////////////////////////////////
	////////////////////////  New ioctl calls  //////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////
	case PCIEDEV_LOCK_DEVICE:
		retval = LongLockOfCriticalRegion(&dev->dev_mut);
		break;

	case PCIEDEV_UNLOCK_DEVICE:
		retval = UnlockLongCritRegion(&dev->dev_mut);
		break;

	case PCIEDEV_SET_BITS:
		if (EnterCritRegion(&dev->dev_mut)){ return -ERESTARTSYS; }
		if (copy_from_user(&aIocRW, (device_ioc_rw*)arg, sizeof(device_ioc_rw)))
		{
			LeaveCritRegion(&dev->dev_mut);
			return -EFAULT;
		}
		//aIocRW.mode_rw = W_BITS_INITIAL + (aIocRW.mode_rw&_STEP_FOR_NEXT_MODE2_);
		CORRECT_REGISTER_SIZE(aIocRW.register_size, dev->register_size);
		//retval = pciedev_write_inline(dev, aIocRW.register_size, MTCA_SET_BITS, aIocRW.barx_rw, aIocRW.offset_rw,
		//	(const char*)aIocRW.dataPtr, (const char*)aIocRW.maskPtr, aIocRW.count_rw);
		retval = (*(dev->write))(dev, aIocRW.register_size, MTCA_SET_BITS, aIocRW.barx_rw, aIocRW.offset_rw,
			(const char*)aIocRW.dataPtr, (const char*)aIocRW.maskPtr, aIocRW.count_rw);
		LeaveCritRegion(&dev->dev_mut);
		break;

	case PCIEDEV_SWAP_BITS:
		if (EnterCritRegion(&dev->dev_mut)){ return -ERESTARTSYS; }
		if (copy_from_user(&aIocRW, (device_ioc_rw*)arg, sizeof(device_ioc_rw)))
		{
			LeaveCritRegion(&dev->dev_mut);
			return -EFAULT;
		}
		CORRECT_REGISTER_SIZE(aIocRW.register_size, dev->register_size);
		ALERTCT("!!!!!!!!!!!!!!!! mode=%s\n", ModeString(aIocRW.register_size));
		//retval = pciedev_write_inline(dev, aIocRW.register_size, MTCA_SWAP_BITS,aIocRW.barx_rw, aIocRW.offset_rw,
		//	NULL, (const char*)aIocRW.maskPtr, aIocRW.count_rw);
		retval = (*(dev->write))(dev, aIocRW.register_size, MTCA_SWAP_BITS, aIocRW.barx_rw, aIocRW.offset_rw,
			NULL, (const char*)aIocRW.maskPtr, aIocRW.count_rw);
		LeaveCritRegion(&dev->dev_mut);
		break;

	case PCIEDEV_LOCKED_READ:
		if (copy_from_user(&aIocRW, (device_ioc_rw*)arg, unSizeOfRW)){ return -EFAULT; }
		CORRECT_REGISTER_SIZE(aIocRW.register_size, dev->register_size);
		if (EnterCritRegion(&dev->dev_mut)){ return -ERESTARTSYS; }
		retval = pciedev_read_inline(dev, aIocRW.register_size, MTCA_LOCKED_READ,aIocRW.barx_rw, aIocRW.offset_rw, (char*)aIocRW.dataPtr, aIocRW.count_rw);
		LeaveCritRegion(&dev->dev_mut);
		break;

	case PCIEDEV_VECTOR_RW:
		if (copy_from_user(&aVectorRW, (device_vector_rw*)arg, sizeof(device_vector_rw))){return -EFAULT;}
		nMaxNumRW = (int)aVectorRW.number_of_rw;
		pUserRWdata = (device_ioc_rw* __user)((void*)aVectorRW.device_ioc_rw_ptr);
		retval = 0;

		if (EnterCritRegion(&dev->dev_mut)){ return -ERESTARTSYS; }
		//for (i = 0; (i < nMaxNumRW) && (!copy_from_user(&aIocRW, pUserRWdata, unSizeOfRW)); ++i, pUserRWdata += sizeof(device_ioc_rw))
		for (i = 0; (i < nMaxNumRW) && (!copy_from_user(&aIocRW, pUserRWdata, unSizeOfRW)); ++i, ++pUserRWdata)
		{
			CORRECT_REGISTER_SIZE(aIocRW.register_size, dev->register_size);
			if (aIocRW.rw_access_mode<MTCA_SIMPLE_WRITE)
			{
				retval += pciedev_read_inline(dev, aIocRW.register_size, aIocRW.rw_access_mode,
					aIocRW.barx_rw,aIocRW.offset_rw, (char*)aIocRW.dataPtr, aIocRW.count_rw);
			}
			else
			{
				//retval += pciedev_write_inline(dev, aIocRW.register_size, aIocRW.rw_access_mode, aIocRW.barx_rw, 
				//	aIocRW.offset_rw,(const char*)aIocRW.dataPtr, (const char*)aIocRW.maskPtr, aIocRW.count_rw);
				retval += (*(dev->write))(dev, aIocRW.register_size, aIocRW.rw_access_mode, aIocRW.barx_rw,
					aIocRW.offset_rw, (const char*)aIocRW.dataPtr, (const char*)aIocRW.maskPtr, aIocRW.count_rw);
			}
		}
		LeaveCritRegion(&dev->dev_mut);
		break;

	case PCIEDEV_GET_REGISTER_SIZE:
		DEBUGNEW("!!!PCIEDEV_GET_REGISTER_SIZE reg_size=%d\n", (int)dev->register_size);
		return dev->register_size;

	case PCIEDEV_SET_REGISTER_SIZE:
		if (copy_from_user(&nUserValue, (int*)arg, sizeof(int))) {
			return -EFAULT;
		}
		CORRECT_REGISTER_SIZE(nUserValue, RW_D32);
		dev->register_size = nUserValue;
		break;

	case PCIEDEV_SINGLE_IOC_ACCESS:
		if (EnterCritRegion(&dev->dev_mut)){ return -ERESTARTSYS; } //if (aIocRW.rw_access_mode == MTCA_LOCKED_READ)
		if (copy_from_user(&aIocRW, (device_ioc_rw*)arg, sizeof(device_ioc_rw)))
		{
			LeaveCritRegion(&dev->dev_mut);
			return -EFAULT;
		}
		CORRECT_REGISTER_SIZE(aIocRW.register_size, dev->register_size);
		if (aIocRW.rw_access_mode<MTCA_SIMPLE_WRITE)
		{
			//printk(KERN_ALERT "rw_access_mode=%d, MTCA_LOCKED_READ=%d\n", (int)aIocRW.rw_access_mode, MTCA_LOCKED_READ);
			//if (aIocRW.rw_access_mode == MTCA_LOCKED_READ){ if (EnterCritRegion(&dev->dev_mut)){ return -ERESTARTSYS; } }
			retval += pciedev_read_inline(dev, aIocRW.register_size, aIocRW.rw_access_mode,
				aIocRW.barx_rw, aIocRW.offset_rw, (char*)aIocRW.dataPtr, aIocRW.count_rw);
			//if (aIocRW.rw_access_mode == MTCA_LOCKED_READ){ LeaveCritRegion(&dev->dev_mut); } // Error prone
			LeaveCritRegion(&dev->dev_mut);
		}
		else
		{
			//if (EnterCritRegion(&dev->dev_mut)){ return -ERESTARTSYS; }  /// Error prone
			retval += pciedev_write_inline(dev, aIocRW.register_size, aIocRW.rw_access_mode, aIocRW.barx_rw,
				aIocRW.offset_rw, (const char*)aIocRW.dataPtr, (const char*)aIocRW.maskPtr, aIocRW.count_rw);
			//retval += (*(dev->write))(dev, aIocRW.register_size, aIocRW.rw_access_mode, aIocRW.barx_rw,
			//	aIocRW.offset_rw, (const char*)aIocRW.dataPtr, (const char*)aIocRW.maskPtr, aIocRW.count_rw);
			LeaveCritRegion(&dev->dev_mut);
		}
		break;

	case PCIEDEV_GET_SLOT_NUMBER:
		return dev->slot_num;
		break;
	/////////////////////////////////////////////////////////////////////////////////////
	////////////////////////  End new ioctl calls  //////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////////

	default:
		return -ENOTTY;
		break;
	}
	//mutex_unlock(&dev->dev_mut);
	//LeaveCritRegion(&dev->dev_mut); /// new  // removed to the cases
	return retval;

}
EXPORT_SYMBOL(pciedev_ioctl_exp);
