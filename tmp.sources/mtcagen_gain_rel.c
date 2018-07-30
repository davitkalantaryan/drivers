/*
 *	File: mtcagen_dma.c
 *
 *	Created on: Jun 06, 2015
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *  Handling of DMA activity of MTCA devices should be implemented here
 *
 */


#include "mtcagen_exp.h"

#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include "mtcagen_io.h"
#include "pciedev_ufn.h"
#include "debug_functions.h"
#include "pci-driver-added-zeuthen.h"
#include "mtcagen_incl.h"


#define DEBUG2 ALERTCT

static void upciedev_pciedev_dev_init1_to_be_discussed(pciedev_dev* a_dev, pciedev_cdev* a_pciedev_cdev_p, int a_nBrdNum)
{
	//devno = MKDEV(pciedev_cdev_p->PCIEDEV_MAJOR, pciedev_cdev_p->PCIEDEV_MINOR + i);
	//pciedev_cdev_p->pciedev_dev_m[i]->dev_num = devno;
	//pciedev_cdev_p->pciedev_dev_m[i]->dev_minor = (pciedev_cdev_p->PCIEDEV_MINOR + i);
	//cdev_init(&(pciedev_cdev_p->pciedev_dev_m[i]->cdev), pciedev_fops);
	//a_dev->cdev.owner = THIS_MODULE;
	//pciedev_cdev_p->pciedev_dev_m[i]->cdev.ops = pciedev_fops;
	//result = cdev_add(&(pciedev_cdev_p->pciedev_dev_m[i]->cdev), devno, 1);
	//if (result)
	//{
	//	printk(KERN_NOTICE "Error %d adding devno%d num%d\n", result, devno, i);
	//	return 1;
	//}
	a_dev->parent_dev = a_pciedev_cdev_p;
	INIT_LIST_HEAD(&(a_dev->prj_info_list.prj_list));
	INIT_LIST_HEAD(&(a_dev->dev_file_list.node_file_list));
	InitCritRegionLock(&(a_dev->dev_mut), _LOCK_TIMEOUT_MS_);/// new
	a_dev->dev_sts = 0;
	a_dev->dev_file_ref = 0;
	a_dev->irq_mode = 0;
	a_dev->msi = 0;
	a_dev->dev_dma_64mask = 0;
	a_dev->pciedev_all_mems = 0;
	a_dev->brd_num = a_nBrdNum;
	a_dev->binded = 0;
	a_dev->dev_file_list.file_cnt = 0;
	a_dev->null_dev = 0;

}

const struct file_operations g_default_mtcagen_fops_exp = {
	.owner			= THIS_MODULE,
	.read			= pciedev_read_exp,
	.write			= pciedev_write_exp,
	//.ioctl		= mtcagen_ioctl,
	.unlocked_ioctl	= mtcagen_ioctl_exp,
	.compat_ioctl	= mtcagen_ioctl_exp,
	.open			= pciedev_open_exp,
	.release		= mtcagen_release_exp,
};
EXPORT_SYMBOL(g_default_mtcagen_fops_exp);

extern struct pci_driver	g_mtcapciegen_driver;

extern struct class*		g_pcie_gen_class;
extern int					g_PCIE_GEN_MAJOR;		/* major by default */

extern void UnRequestInterrupt_all(struct pciedev_dev* a_pDev);
static int DefaultProbe(struct pciedev_dev* a_dev,void* callbackData);
static void DefaultRemove(struct pciedev_dev* a_dev, void* a_pCallbackData);
static int RegisterDeviceForControlStat(struct SDeviceParams* a_pDevParams, struct pci_driver* a_pDrv,
										void* a_pCallbackData, ...);
static void Mtcagen_cdev_delete(struct pciedev_dev* dev);


int  mtcagen_release_exp(struct inode *inode, struct file *filp)
{
	struct pciedev_dev *dev;
	u16 cur_proc = 0;
	upciedev_file_list *tmp_file_list;
	struct list_head *pos, *q;

	dev = filp->private_data;

	//if (mutex_lock_interruptible(&dev->dev_mut)){return -ERESTARTSYS;}
	///if (EnterCritRegion(&dev->dev_mut)){ return -ERESTARTSYS; } /// new
	dev->dev_file_ref--;
	cur_proc = current->group_leader->pid;
	list_for_each_safe(pos, q, &(dev->dev_file_list.node_file_list)){
		tmp_file_list = list_entry(pos, upciedev_file_list, node_file_list);
		if (tmp_file_list->filp == filp){
			list_del(pos);
			kfree(tmp_file_list);
		}
	}
	//mutex_unlock(&dev->dev_mut);
	///LeaveCritRegion(&dev->dev_mut); /// new

	if (!dev->dev_sts)
	{
		Mtcagen_cdev_delete(dev);
	}

	return 0;
}
EXPORT_SYMBOL(mtcagen_release_exp);

int RegisterDeviceForControl_exp(struct pci_driver* a_pciDriver, struct SDeviceParams* a_pDevParams,
								PROBETYPE a_probe, REMOVETYPE a_remove, void* a_pCallbackData)
{
	if (!a_pciDriver)a_pciDriver = &g_mtcapciegen_driver;
	if (!a_probe)a_probe = &DefaultProbe;
	if (!a_remove)a_remove = &DefaultRemove;
	return RegisterDeviceForControlStat(a_pDevParams, a_pciDriver, a_pCallbackData, a_probe, a_remove);
}
EXPORT_SYMBOL(RegisterDeviceForControl_exp);



static void Mtcagen_ReleaseAccsess(struct pciedev_dev* a_pDev)
{
	struct pci_dev*  dev = a_pDev->pciedev_pci_dev;
	int j;

	ALERTCT("Release CALLED for device in slot %d\n", (int)a_pDev->slot_num);
	SET_FLAG_VALUE(a_pDev->m_ulnFlag, _CREATED_ADDED_BY_USER, 0);

	UnRequestInterrupt_all(a_pDev);

	///if (GET_FLAG_VALUE(a_pDev->m_ulnFlag, _MUTEX_CREATED))EnterCritRegion(&(a_pDev->dev_mut));

	for (j = 0; j < NUMBER_OF_BARS; ++j)
	{
		ALERTCT("UNMAPPING MEMORY %d\n", j);
		if (a_pDev->memmory_base[j])
		{
			pci_iounmap(dev, a_pDev->memmory_base[j]);
		}
		a_pDev->memmory_base[j] = 0;
	}
	ALERTCT("MEMORYs UNMAPPED\n");

	if (GET_FLAG_VALUE(a_pDev->m_ulnFlag, _PCI_REGIONS_REQUESTED))
	{
		pci_release_regions((a_pDev->pciedev_pci_dev));
		SET_FLAG_VALUE(a_pDev->m_ulnFlag, _PCI_REGIONS_REQUESTED, 0);
	}

	///if (GET_FLAG_VALUE(a_pDev->m_ulnFlag, _MUTEX_CREATED))LeaveCritRegion(&(a_pDev->dev_mut));

	if (GET_FLAG_VALUE(a_pDev->m_ulnFlag, _MUTEX_CREATED))
	{
		DestroyCritRegionLock(&(a_pDev->dev_mut));
		SET_FLAG_VALUE(a_pDev->m_ulnFlag, _MUTEX_CREATED, 0);
	}

	if (GET_FLAG_VALUE(a_pDev->m_ulnFlag, _PCI_DEVICE_ENABLED))
	{
		pci_disable_device(a_pDev->pciedev_pci_dev);
		SET_FLAG_VALUE(a_pDev->m_ulnFlag, _PCI_DEVICE_ENABLED, 0);
	}
}



static void Mtcagen_cdev_delete(struct pciedev_dev* a_pDev)
{
	if (GET_FLAG_VALUE(a_pDev->m_ulnFlag, _CDEV_ADDED))
	{
		cdev_del(&a_pDev->cdev);
		SET_FLAG_VALUE(a_pDev->m_ulnFlag, _CDEV_ADDED, 0);
	}
}



static void RemoveDeviceEntry(struct pciedev_dev* a_pDev, int a_MAJOR, struct class* a_class)
{
	if (GET_FLAG_VALUE(a_pDev->m_ulnFlag, _DEVC_ENTRY_CREATED))
	{
		device_destroy(a_class, MKDEV(a_MAJOR, a_pDev->dev_minor));
		SET_FLAG_VALUE(a_pDev->m_ulnFlag, _DEVC_ENTRY_CREATED, 0);
	}a_pDev->deviceFl = NULL;
}



void Mtcagen_remove_exp(struct pciedev_dev* a_pDev, int a_MAJOR, struct class* a_class)
{
	if (a_pDev->dev_sts == 0)
	{
		WARNCT("Device already released!\n");
		return;
	}
	a_pDev->dev_sts = 0;
	if (a_MAJOR <= 0)a_MAJOR = g_PCIE_GEN_MAJOR;
	if (a_class == NULL)a_class = g_pcie_gen_class;
	RemoveDeviceEntry(a_pDev,a_MAJOR, a_class);
	msleep(1);
	if (a_pDev->dev_file_ref){ msleep(100); }

	Mtcagen_ReleaseAccsess(a_pDev);

	if (a_pDev->dev_file_ref == 0){ Mtcagen_cdev_delete(a_pDev); }
}
EXPORT_SYMBOL(Mtcagen_remove_exp);



int  Mtcagen_GainAccess_exp(struct pciedev_dev* a_pDev, int a_major, struct class* a_class, 
	const struct file_operations *a_pciedev_fops, const char* a_DRV_NAME2, const char* a_drv_name_fmt, ...)
{
	struct pci_dev*  dev = a_pDev->pciedev_pci_dev;
	va_list vargs;
	int result = 0;
	int j, nToAdd;
	u8  revision;
	u8  irq_line;
	u8  irq_pin;
	u32 res_start;
	u32 res_end;
	u32 res_flag;
	u32 pci_dev_irq;
	u16 class_code;
	int devno;

	if (a_pDev->dev_sts)
	{
		ERRCT("Device in slot %d already in use!\n", a_pDev->slot_num);
		return -1;
	}

	if (a_major <= 0)a_major = g_PCIE_GEN_MAJOR;
	if (a_class == NULL)a_class = g_pcie_gen_class;

	upciedev_pciedev_dev_init1_to_be_discussed(a_pDev, NULL, a_pDev->dev_minor);

	if ((result = pci_enable_device(dev)))
	{
		ERRCT("Unable to enable pci device in slot %d\n", a_pDev->slot_num);
		return result;
	}
	SET_FLAG_VALUE(a_pDev->m_ulnFlag, _PCI_DEVICE_ENABLED, 1);

	
	a_pDev->cdev.owner = THIS_MODULE;

	if (GET_FLAG_VALUE(a_pDev->m_ulnFlag, _CDEV_ADDED)==0)
	{
		devno = MKDEV(a_major, a_pDev->dev_minor);
		cdev_init(&a_pDev->cdev, a_pciedev_fops);
		result = cdev_add(&a_pDev->cdev, devno, 1);
		if (result)
		{
			ERRCT("Adding error(%d) devno:%d for slot:%dd\n", result, devno, a_pDev->slot_num);
			goto errorOut;
		}
		SET_FLAG_VALUE(a_pDev->m_ulnFlag, _CDEV_ADDED, 1);
	}
	a_pDev->cdev.ops = a_pciedev_fops;

	InitCritRegionLock(&(a_pDev->dev_mut), DEFAULT_LOCK_TIMEOUT);
	SET_FLAG_VALUE(a_pDev->m_ulnFlag, _MUTEX_CREATED, 1);

	pci_set_master(dev);

	dev_set_drvdata(&(dev->dev), a_pDev);
	a_pDev->pciedev_all_mems = 0;

	//spin_lock_init(&a_pDev->irq_lock);
	a_pDev->irq_mode = 0;

	pci_read_config_word(dev, PCI_CLASS_DEVICE, &class_code);
	pci_read_config_byte(dev, PCI_REVISION_ID, &revision);
	pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &irq_line);
	pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &irq_pin);

	pci_dev_irq = dev->irq;
	ALERTCT("GET_PCI_CONF VENDOR %X DEVICE %X REV %X\n", dev->vendor, dev->device, revision);
	ALERTCT("GET_PCI_CONF SUBVENDOR %X SUBDEVICE %X CLASS %X\n", dev->subsystem_vendor, dev->subsystem_device, class_code);

	a_pDev->class_code = class_code;
	a_pDev->revision = revision;
	a_pDev->irq_line = irq_line;
	a_pDev->irq_pin = irq_pin;
	a_pDev->pci_dev_irq = pci_dev_irq;

	if (EBUSY == pci_request_regions(dev, a_DRV_NAME2))
	{
		ERRCT("EBUSY\n");
		result = EBUSY < 0 ? EBUSY : (EBUSY == 0 ? -1 : -EBUSY);
		goto errorOut;
	}
	SET_FLAG_VALUE(a_pDev->m_ulnFlag, _PCI_REGIONS_REQUESTED, 1);


	for (j = 0, nToAdd = 1; j < NUMBER_OF_BARS; ++j, nToAdd *= 2)
	{
		res_start = pci_resource_start(dev, j);
		res_end = pci_resource_end(dev, j);
		res_flag = pci_resource_flags(dev, j);
		a_pDev->mem_base[j] = res_start;
		a_pDev->mem_base_end[j] = res_end;
		a_pDev->mem_base_flag[j] = res_flag;

		if (res_start)
		{
			a_pDev->memmory_base[j] = pci_iomap(dev, j, (res_end - res_start));
			INFOCT("mem_region %d address %lX remaped address %lX\n", j,
				(unsigned long int)res_start,
				(unsigned long int)a_pDev->memmory_base[j]);
			a_pDev->rw_off[j] = (res_end - res_start);
			INFOCT(" memorySize[%d] = %d\n", j, (int)(res_end - res_start));
			a_pDev->pciedev_all_mems += nToAdd;
		}
		else
		{
			a_pDev->memmory_base[j] = 0;
			a_pDev->rw_off[j] = 0;
			INFOCT("NO BASE_%d address\n", j);
		}
	}


	ALERTCT("MAJOR = %d, MINOR = %d\n", a_major, a_pDev->dev_minor);

	va_start(vargs, a_drv_name_fmt);
	a_pDev->deviceFl = device_create_vargs(a_class, NULL, MKDEV(a_major, a_pDev->dev_minor), (void*)&a_pDev->pciedev_pci_dev->dev,
		a_drv_name_fmt, vargs);
	va_end(vargs);
	if (IS_ERR(a_pDev->deviceFl))
	{
		ERRCT("Device creation error!\n");
		goto errorOut;
	}
	ALERTCT("Device create done!\n");
	SET_FLAG_VALUE(a_pDev->m_ulnFlag, _DEVC_ENTRY_CREATED, 1);

	a_pDev->dev_sts = 1;
	a_pDev->register_size = RW_D32;

	dev_set_drvdata(&dev->dev, a_pDev);
	a_pDev->error_report_status = 0;

	return 0;

errorOut:
	a_pDev->dev_sts = 0;
	Mtcagen_remove_exp(a_pDev, a_major, a_class);
	return result < 0 ? result : -1;
}
EXPORT_SYMBOL(Mtcagen_GainAccess_exp);


///////////////////////////////////////////////////////////////////////////////////////
struct SDeleteStruct
{
	int	isKernel;
	union
	{
		struct pci_device_id* id;
		int slot_num;
	};
};

static int MatchDeleteFuncStat1(struct device * a_dev, void * a_pData)
{
	//struct pci_dev* pPciDev = to_pci_dev(a_dev);
	static int snIteration = 0;
	struct pciedev_dev* pMtcaDev = dev_get_drvdata(a_dev);
	struct SDeleteStruct* pDeleteStr = a_pData;
	struct pci_device_id* id = pDeleteStr->id;
	int nIsKernel = pDeleteStr->isKernel;

	DEBUG2("!!!!!!!! iteration=%d,a_dev=%p,pMtcaDev=%p\n", ++snIteration, a_dev, pMtcaDev);

	if (pMtcaDev && pMtcaDev->m_id && ((!id) || _COMPARE_2_IDS1_(id, pMtcaDev->m_id)))
	{
		if (GET_FLAG_VALUE(pMtcaDev->m_ulnFlag, _CREATED_ADDED_BY_USER) || nIsKernel){return 1;}
		WARNCT("Trying to release device by user!\n");
	}
	DEBUG2("!!!!!!!! iteration=%d\n", snIteration);

	return 0;
}


static int MatchDeleteFuncStat2(struct device * a_dev, void * a_pData)
{
	//struct pci_dev* pPciDev = to_pci_dev(a_dev);
	static int snIteration = 0;
	struct pciedev_dev* pMtcaDev = dev_get_drvdata(a_dev);
	struct SDeleteStruct* pDeleteStr = a_pData;
	int tmp_slot_num = pDeleteStr->slot_num;
	int nIsKernel = pDeleteStr->isKernel;

	DEBUG2("!!!!!!!! iteration=%d\n", ++snIteration);

	if ((tmp_slot_num != pMtcaDev->slot_num) || !pMtcaDev->dev_sts) return 0; // Check next

	if (GET_FLAG_VALUE(pMtcaDev->m_ulnFlag, _CREATED_ADDED_BY_USER) || nIsKernel)
	{
		return 1;
		DEBUG2("!!!!!!!! iteration=%d\n", snIteration);
		device_release_driver(a_dev);
		DEBUG2("!!!!!!!! iteration=%d\n", snIteration);
		pMtcaDev->m_id = NULL; // kara ev chlini
	}
	else
	{
		WARNCT("Trying to release device by user!\n");
	}

	return 0; // Stop checking
}
///////////////////////////////////////////////////////////////////////////////////////



static inline void RemoveIDFromDriverInline(struct pci_driver* a_pciDriver, struct pci_device_id* a_id, int a_nIsKernel)
{
	//driver_for_each_device(&a_pciDriver->driver, NULL, &aDeleteStruct, &DeleteFuncStat1);
	// should be understood
	struct SDeleteStruct aDeleteStruct;
	struct device *dev = (void*)1, *devTemp = NULL;
	struct pciedev_dev* pMtcaDev ;

	aDeleteStruct.isKernel = a_nIsKernel;
	aDeleteStruct.id = a_id;

	//DEBUGNEW("a_id=%p, a_pciDriver=%p\n", a_id, a_pciDriver);
	//if (!a_pciDriver)return;

	while(dev)
	{
		//DEBUGNEW("\n");
		dev = driver_find_device(&a_pciDriver->driver, devTemp, &aDeleteStruct, MatchDeleteFuncStat1);
		//dev = driver_find_device(a_pciDriver->driver, devTemp, &aDeleteStruct, MatchDeleteFuncStat1);
		if (devTemp)
		{
			pMtcaDev = dev_get_drvdata(devTemp);
			device_release_driver(devTemp); 
			pMtcaDev->m_id = NULL; // kara ev chlini
		}
		devTemp = dev;
	}

	if (devTemp)
	{
		pMtcaDev = dev_get_drvdata(devTemp);
		device_release_driver(devTemp); 
		pMtcaDev->m_id = NULL; // kara ev chlini
	}
}



int RemoveIDFromDriver_private(struct pci_driver* a_pciDriver, struct pci_device_id* a_id, int a_nIsKernel)
{
	RemoveIDFromDriverInline(a_pciDriver, a_id, a_nIsKernel);
	return pci_free_dynid_zeuthen(a_pciDriver, a_id);
}


int RemoveIDFromDriver2_exp(struct pci_driver* a_pciDriver, struct pci_device_id* a_id)
{
	if (!a_pciDriver) a_pciDriver = &g_mtcapciegen_driver;
	return RemoveIDFromDriver_private(a_pciDriver, a_id, 1);
}
EXPORT_SYMBOL(RemoveIDFromDriver2_exp);


int RemoveIDFromDriverByParams_private(struct pci_driver* a_pciDriver, struct SDeviceParams* a_DeviceToRemove, int a_nIsKernel)
{
	struct pci_device_id aID = {
		.vendor = a_DeviceToRemove->vendor < 0 ? PCI_ANY_ID : a_DeviceToRemove->vendor,
		.device = a_DeviceToRemove->device < 0 ? PCI_ANY_ID : a_DeviceToRemove->device,
		.subvendor = a_DeviceToRemove->subsystem_vendor < 0 ? PCI_ANY_ID : a_DeviceToRemove->subsystem_vendor,
		.subdevice = a_DeviceToRemove->subsystem_device < 0 ? PCI_ANY_ID : a_DeviceToRemove->subsystem_device,
		.class = 0,
		.class_mask = 0,
		.driver_data = 0,
	};
	return RemoveIDFromDriver_private(a_pciDriver, &aID,a_nIsKernel);
}



int RemoveIDFromDriverByParams2_exp(struct pci_driver* a_pciDriver, struct SDeviceParams* a_DeviceToRemove)
{
	if (!a_pciDriver) a_pciDriver = &g_mtcapciegen_driver;
	return RemoveIDFromDriverByParams_private(a_pciDriver, a_DeviceToRemove, 1);
}
EXPORT_SYMBOL(RemoveIDFromDriverByParams2_exp);



int ReleaseAccessToPciDevice_private(struct pci_driver* a_pciDriver, SDeviceParams* a_DeviceToRemove, int a_nIsKernel)
{
	int tmp_slot_num = a_DeviceToRemove->slot_num;

	if (tmp_slot_num < 0)
	{
		struct pci_device_id aID = {
			.vendor = a_DeviceToRemove->vendor < 0 ? PCI_ANY_ID : a_DeviceToRemove->vendor,
			.device = a_DeviceToRemove->device < 0 ? PCI_ANY_ID : a_DeviceToRemove->device,
			.subvendor = a_DeviceToRemove->subsystem_vendor < 0 ? PCI_ANY_ID : a_DeviceToRemove->subsystem_vendor,
			.subdevice = a_DeviceToRemove->subsystem_device < 0 ? PCI_ANY_ID : a_DeviceToRemove->subsystem_device,
			.class = 0,
			.class_mask = 0,
			.driver_data = 0,
		};
		RemoveIDFromDriverInline(a_pciDriver, &aID, a_nIsKernel);
	}
	else
	{
		struct device *dev = (void*)1, *devTemp = NULL;
		struct SDeleteStruct aDeleteStruct;
		struct pciedev_dev* pMtcaDev;

		aDeleteStruct.isKernel = a_nIsKernel;
		aDeleteStruct.slot_num = tmp_slot_num;

		while (dev)
		{
			dev = driver_find_device(&a_pciDriver->driver, devTemp, &aDeleteStruct, MatchDeleteFuncStat2);
			if (dev)
			{
				pMtcaDev = dev_get_drvdata(dev);
				device_release_driver(dev);
				pMtcaDev->m_id = NULL; // kara ev chlini
				break;
			}
			devTemp = dev;
		}

		//return driver_for_each_device(&a_pciDriver->driver, NULL, &aDeleteStruct, &DeleteFuncStat2);
	}

	return 0;
}



int ReleaseAccessToPciDevice2_exp(struct pci_driver* a_pciDriver,SDeviceParams* a_DeviceToRemove)
{
	if (!a_pciDriver) a_pciDriver = &g_mtcapciegen_driver;
	return ReleaseAccessToPciDevice_private(a_pciDriver, a_DeviceToRemove, 1);
}
EXPORT_SYMBOL(ReleaseAccessToPciDevice2_exp);



void FreeAllIDsAndDevices_private(struct pci_driver* a_pciDriver, int a_nIsKernel)
{
	RemoveIDFromDriver_private(a_pciDriver, NULL, a_nIsKernel);
	FreeAllIDsAndDrvDatas(a_pciDriver);  // This is not needed ?
}


#if 0
void FreeAllIDsAndDevices2_exp(struct pci_driver* a_pciDriver)
{
	FreeAllIDsAndDevices_private(a_pciDriver, 1);
}
EXPORT_SYMBOL(FreeAllIDsAndDevices2_exp);
#endif



static int DefaultProbe(struct pciedev_dev* a_dev,void* a_pCallbackData)
{
	// In the case of no callback provided we set possible to remove 
	int nMagic = (int)((long)a_pCallbackData);
	switch (nMagic)
	{
	case 0: a_pCallbackData = (void*)((long)MAGIC_VALUE2); break;
	case MAGIC_VALUE2: SET_FLAG_VALUE(a_dev->m_ulnFlag, _CREATED_ADDED_BY_USER,1); break;
	default: break;
	}
	
	return Mtcagen_GainAccess_exp(a_dev, g_PCIE_GEN_MAJOR, g_pcie_gen_class, &g_default_mtcagen_fops_exp,
		DRV_AND_DEV_NAME, "%ss%d", DRV_AND_DEV_NAME, a_dev->brd_num);
}



static void DefaultRemove(struct pciedev_dev* a_dev, void* a_pCallbackData)
{
	int nMagic = (int)((long)a_pCallbackData);
	if (nMagic == MAGIC_VALUE2)
	{
		Mtcagen_remove_exp(a_dev, g_PCIE_GEN_MAJOR, g_pcie_gen_class);
	}
	else
	{
		ALERTCT("!!!! vendor_id=%d handled by other driver!\n", (int)a_dev->pciedev_pci_dev->vendor);
	}
}



/*PROBETYPE a_probe, RELEASETYPE a_release*/
static int RegisterDeviceForControlStat(struct SDeviceParams* a_pDevParams, struct pci_driver* a_pDrv,
	void* a_pCallbackData, ...)
{
	va_list vargs;
	void* pProbe;
	void* pRelease;
	//kernel_ulong_t ullnDrvData;
	//kernel_ulong_t ullnSlotNumber = a_pDevParams->slot_num < 0 ? 0 : a_pDevParams->slot_num;
	int result;

	va_start(vargs, a_pCallbackData);
	pProbe = va_arg(vargs, void*);
	pRelease = va_arg(vargs, void*);
	va_end(vargs);

	//SET_SLOT_AND_PID(&ullnDrvData, a_pDevParams->slot_num, unPID);

	///if (a_pDevParams->callback == NULL) a_pDevParams->callback = &General_GainAccess;
	if (a_pDevParams->vendor<0) a_pDevParams->vendor = PCI_ANY_ID;
	if (a_pDevParams->device<0) a_pDevParams->device = PCI_ANY_ID;
	if (a_pDevParams->subsystem_vendor<0) a_pDevParams->subsystem_vendor = PCI_ANY_ID;
	if (a_pDevParams->subsystem_device<0) a_pDevParams->subsystem_device = PCI_ANY_ID;

	//DEBUGCT("Calling pci_add_dynidMy\n");
	result = pci_add_dynid_zeuthen2(a_pDrv, a_pDevParams->vendor, a_pDevParams->device,
		a_pDevParams->subsystem_vendor, a_pDevParams->subsystem_device, 0, 0,
		a_pDevParams->slot_num, pProbe, pRelease, a_pCallbackData);

	//wait_for_device_probe();	// Waiting is not done, if user space needs, then information will
	//							// be provided by interrupt
	return result;
}
