/*
*	File: pcie_gen.c
*
*	Created on: May 12, 2013
*	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
*
*	This file contain definitions of ...
*
*/

#include "pcie_gen_exp2.h"

#include "pcie_gen_fnc.h"      	/* local definitions */
#include "version_dependence.h"

MODULE_AUTHOR("Davit Kalantaryan");
MODULE_DESCRIPTION("pcie_gen_drv General purpose driver ");
MODULE_VERSION("1.1.0");
MODULE_LICENSE("Dual BSD/GPL");

static const u16 PCIE_GEN_DRV_VER_MAJ = 1;
static const u16 PCIE_GEN_DRV_VER_MIN = 1;

static struct ZnDrv_gen		s_mtca_menegment;	/* ... */
static struct str_pcie_gen	s_vPci_gens[PCIE_GEN_NR_DEVS];
static struct class*		s_pcie_gen_class = NULL;
static int					PCIE_GEN_MAJOR = 46;     /* static major by default */


static int pcie_gen_procinfo(char *buf, char **start, off_t fpos, int lenght, int *eof, void *data)
{
	char *p;
	struct ZnDrv_gen *drv = data;
	p = buf;
	p += sprintf(p, "Driver Version:\t%i.%i\n", PCIE_GEN_DRV_VER_MAJ, PCIE_GEN_DRV_VER_MIN);
	p += sprintf(p, "Management minor is:\t%i\n", drv->m_dev_minor);
	*eof = 1;
	return p - buf;
}



static int mtcagen_management_open(struct inode *inode, struct file *filp)
{
	int    minor;
	struct ZnDrv_gen *drv; /* device information */

	minor = iminor(inode);
	drv = container_of(inode->i_cdev, struct ZnDrv_gen, m_cdev);
	drv->m_dev_minor = minor;
	filp->private_data = drv; /* for other methods */

	ALERT("Opening menegment device! Procces is \"%s\" (pid %i) DEV is %d \n", current->comm, current->group_leader->pid, minor);

	return 0;
}



static int mtcagen_managent_release(struct inode *inode, struct file *filp)
{
	struct ZnDrv_gen	*drv = filp->private_data;
	int minor = drv->m_dev_minor;
	u16 cur_proc = current->group_leader->pid;
	//if (EnterCritRegion(&dev->m_DrvStuff.m_criticalReg))return -ERESTARTSYS;
	ALERT("Closing management device. Procces is \"%s\" (pid %d), current->pid = %i, ",
		current->comm, (int)cur_proc, current->pid);
	printk(KERN_CONT "MINOR = %d\n", minor);
	//LeaveCritRegion(&dev->m_DrvStuff.m_criticalReg);
	return 0;
}

#define BIT_N_VALUE_PRVT(__a_u32__,__a_N__) ((__a_u32__) & (1<<(__a_N__))) ? 1 : 0

#define PRINT_BITS_FR_INT_PRVT(__a_name__,__a_u32__,__a_start__,__a_fin__) \
{\
	int __nStart__ = (__a_start__)<0 ? 0 : ((__a_start__)>31 ? 31 : (__a_start__)); \
	int __nFin__ = (__a_fin__)<__nStart__ ? __nStart__ : ((__a_fin__)>31 ? 31 : (__a_fin__));\
	int __i__ = __nFin__; \
	printk(KERN_CONT "%s[%d-%d] = {",__a_name__,__nFin__,__nStart__); \
	for(;__i__>__nStart__;--__i__) printk(KERN_CONT "%d,",BIT_N_VALUE_PRVT(__a_u32__,__i__)); \
	printk(KERN_CONT "%d}",BIT_N_VALUE_PRVT(__a_u32__,__i__));\
}


#define PRINT_BITS_FR_INT1(__a_u32__,__a_start__,__a_fin__) \
	PRINT_BITS_FR_INT_PRVT(#__a_u32__,__a_u32__,__a_start__,__a_fin__)

#define PRINT_BITS_FR_INT2(__a_str__,__a_u32__,__a_start__,__a_fin__) \
	printk(KERN_ALERT "%s",(__a_str__)); \
	PRINT_BITS_FR_INT_PRVT(#__a_u32__,__a_u32__,__a_start__,__a_fin__);printk(KERN_CONT "\n");

#if 1
#define PRINT_BITS_FR_INT3_ALL(__a_str__,__a_u32__) \
	printk(KERN_ALERT "%s",(__a_str__)); \
	PRINT_BITS_FR_INT_PRVT(#__a_u32__,__a_u32__,19,31); \
	printk(KERN_CONT ";");\
	PRINT_BITS_FR_INT_PRVT(#__a_u32__,__a_u32__,0,18); \
	printk(KERN_CONT "\n");
#else
static inline void  PRINT_BITS_FR_INT3_ALL(const char* __a_str__, u32 __a_u32__)
{
	printk(KERN_ALERT "%s", (__a_str__));
	PRINT_BITS_FR_INT_PRVT("tmp_cap", __a_u32__, 19, 31);
	printk(KERN_CONT ";");
	PRINT_BITS_FR_INT_PRVT("tmp_cap", __a_u32__, 0, 18);
	printk(KERN_CONT "\n");
}
#endif

#define _METHOD_	4

static inline int GetSlotNumber(struct pci_dev *a_pPciDev) // This function should be improved
{
	int pcie_cap;
	u32 tmp_slot_cap = 0;

	//u32 toDebug;

#if _METHOD_==1
	if (a_pPciDev->bus)
	{
		if (a_pPciDev->bus->parent)
		{
			if (a_pPciDev->bus->parent->self)
			{
				pcie_cap = pci_find_capability(a_pPciDev->bus->parent->self, PCI_CAP_ID_EXP);
				pci_read_config_dword(a_pPciDev->bus->parent->self, (pcie_cap + PCI_EXP_SLTCAP), &tmp_slot_cap);
				//pci_read_config_dword(a_pPciDev->bus->parent->self, (pcie_cap + PCI_EXP_SLTCAP), &tmp_slot_cap);
				PRINT_BITS_FR_INT(tmp_slot_cap, 0, 19);
				return (tmp_slot_cap >> 19) % PCIE_GEN_NR_DEVS;
			}

		} // if (a_pPciDev->bus->parent)

	} // if (a_pPciDev->bus)
#elif _METHOD_==2
	if (a_pPciDev->bus)
	{
		if (a_pPciDev->bus->self)
		{
			pcie_cap = pci_find_capability(a_pPciDev->bus->self, PCI_CAP_ID_EXP);
			pci_read_config_dword(a_pPciDev->bus->self, (pcie_cap + PCI_EXP_SLTCAP), &tmp_slot_cap);
			PRINT_BITS_FR_INT(tmp_slot_cap, 0, 19);
			return (tmp_slot_cap >> 19) % PCIE_GEN_NR_DEVS;
		}

	} // if (a_pPciDev->bus)
#elif _METHOD_==3
	struct pci_bus* pCurBus = a_pPciDev ? a_pPciDev->bus : NULL;
	struct pci_dev* pCurDev = pCurBus ? pCurBus->self : NULL;

	while (pCurDev)
	{
		pcie_cap = pci_find_capability(pCurDev, PCI_CAP_ID_EXP);
		pci_read_config_dword(pCurDev, (pcie_cap + PCI_EXP_SLTCAP), &tmp_slot_cap);

		PRINT_BITS_FR_INT3_ALL("Trying:", tmp_slot_cap);

		if (tmp_slot_cap & PCI_EXP_FLAGS_SLOT)
		{
			PRINT_BITS_FR_INT3_ALL("Found!:", tmp_slot_cap);
			return (tmp_slot_cap >> 19) % PCIE_GEN_NR_DEVS;
		}

		pCurBus = pCurBus ? pCurBus->parent : NULL;
		pCurDev = pCurBus ? pCurBus->self : NULL;
	}
#elif _METHOD_==4
	struct pci_bus* pCurBus = a_pPciDev ? a_pPciDev->bus : NULL;
	struct pci_dev* pCurDev = pCurBus ? pCurBus->self : NULL;

	while (pCurDev)
	{
		pcie_cap = pci_find_capability(pCurDev, PCI_CAP_ID_EXP);
		pci_read_config_dword(pCurDev, (pcie_cap + PCI_EXP_SLTCAP), &tmp_slot_cap);

		PRINT_BITS_FR_INT3_ALL("Trying:", tmp_slot_cap);

		if ((tmp_slot_cap & 1) && (tmp_slot_cap & 2))
		{
			PRINT_BITS_FR_INT3_ALL("Found!:", tmp_slot_cap);
			return (tmp_slot_cap >> 19) % PCIE_GEN_NR_DEVS;
		}

		pCurBus = pCurBus ? pCurBus->parent : NULL;
		pCurDev = pCurBus ? pCurBus->self : NULL;
	}
#else
#error wrong method
#endif

	return -1;
}


#define		FIND_FREE_ENTRY(tmp_slot_numM,tmp_slot_numM_ptr) \
	*(tmp_slot_numM_ptr) = 	(tmp_slot_numM); \
		while(s_vPci_gens[*(tmp_slot_numM_ptr)].m_PciGen.m_dev_sts) \
			{ \
		WARNING("Slot %d already in use!\n",(int)(*(tmp_slot_numM_ptr))); \
		*(tmp_slot_numM_ptr) = ((*(tmp_slot_numM_ptr))+1)%PCIE_GEN_NR_DEVS; \
		if((*(tmp_slot_numM_ptr)) == (tmp_slot_numM)) {*(tmp_slot_numM_ptr) = -1;break;} \
			}


extern inline int KillPidWithInfoZn(pid_t a_unPID, int a_nSignal, struct siginfo* a_pInfo);

static int __devinit Mtcapciegen_probe(struct pci_dev *a_dev, const struct pci_device_id *a_id)
{
	int tmp_slot_num0, tmp_slot_numF;
	struct list_head* pHead = (struct list_head*)((void*)((char*)a_id->driver_data));
	struct SDriverData* pDrvData;

	//static int nIteration = 0;
	//printk(KERN_ALERT "=============================================== Mtcapciegen_probe %d.  id = %p\n", ++nIteration,a_id);

	ALERT("dev:{vendor(%d),device(%d),subvendor(%d),subdevice(%d)}\n",
		(int)a_dev->vendor, (int)a_dev->device, (int)a_dev->subsystem_vendor, (int)a_dev->subsystem_device);
	ALERT("id: {vendor(%d),device(%d),subvendor(%d),subdevice(%d),driver_data(0x%p),class(%d),class_mask(%d)}\n",
		(int)a_id->vendor, (int)a_id->device,
		(int)a_id->subvendor, (int)a_id->subdevice, (char*)a_id->driver_data, (int)a_id->class, (int)a_id->class_mask);

	tmp_slot_num0 = GetSlotNumber(a_dev); // % PCIE_GEN_NR_DEVS done in GetSlotNumber
	ALERT("+++tmp_slot_num = %d\n", tmp_slot_num0);
	if (unlikely(tmp_slot_num0 < 0)) return -4;

	list_for_each_entry(pDrvData, pHead, m_node)
	{
		DEBUG2("pHead(%p); pDrvData(%p)\n", pHead, pDrvData);

		if (pDrvData->m_slot_num < 0 || pDrvData->m_slot_num == tmp_slot_num0)
		{
			struct siginfo aInfo;

			FIND_FREE_ENTRY(tmp_slot_num0, &tmp_slot_numF);
			if (tmp_slot_numF<0){ ERR("All entries in use!\n"); return -2; }
			DEBUG1("!!!!!!!!!!!! tmp_slot_numF = %d\n", tmp_slot_numF);

			s_vPci_gens[tmp_slot_numF].m_mtcapice_dev = a_dev;
			s_vPci_gens[tmp_slot_numF].irq_type = /*pDrvData->m_irq_type*/0;
			s_vPci_gens[tmp_slot_numF].m_PciGen.m_slot_num = tmp_slot_num0;
			s_vPci_gens[tmp_slot_numF].m_PciGen.m_DrvStuff.m_dev_minor = PCIE_GEN_MINOR + tmp_slot_numF + 1;

			if (General_GainAccess1(&s_vPci_gens[tmp_slot_numF], PCIE_GEN_MAJOR, s_pcie_gen_class, "pcie_gen_drv"))
			{
				s_vPci_gens[tmp_slot_numF].m_PciGen.m_dev_sts = 0;
				return -3;
			}
			s_vPci_gens[tmp_slot_numF].m_PciGen.m_slot_num = tmp_slot_num0;

			dev_set_drvdata(&(a_dev->dev), (&s_vPci_gens[tmp_slot_numF]));
			s_vPci_gens[tmp_slot_numF].m_id = a_id;

			aInfo._sifields._pad[0] = s_vPci_gens[tmp_slot_numF].m_mtcapice_dev->vendor;
			aInfo._sifields._pad[1] = s_vPci_gens[tmp_slot_numF].m_mtcapice_dev->device;
			aInfo._sifields._pad[2] = s_vPci_gens[tmp_slot_numF].m_mtcapice_dev->subsystem_vendor;
			aInfo._sifields._pad[3] = s_vPci_gens[tmp_slot_numF].m_mtcapice_dev->subsystem_device;
			aInfo._sifields._pad[4] = s_vPci_gens[tmp_slot_numF].m_PciGen.m_slot_num;
			//aInfo._sifields._pad[4] = s_vPci_gens[tmp_slot_numF].m_PciGen.m_DrvStuff.m_dev_minor;
			KillPidWithInfoZn(pDrvData->m_unPID, SIGURG, &aInfo);

			return 0;
		} // if (nSlotNumFromDrvData < 0 || nSlotNumFromDrvData == tmp_slot_num)
	} // list_for_each_entry(pDrvData, pHead, m_node)

	return -1;
}



static void __devexit Mtcapciegen_remove(struct pci_dev *a_dev)
{
	struct str_pcie_gen* pPcieStr = dev_get_drvdata(&a_dev->dev);
	if (pPcieStr)Gen_ReleaseAccsess(pPcieStr, PCIE_GEN_MAJOR, s_pcie_gen_class);
	pPcieStr->m_id = NULL;
}


static struct pci_device_id s_mtcapciegen_table[] __devinitdata = { { 0, } };
MODULE_DEVICE_TABLE(pci, s_mtcapciegen_table);


static struct pci_driver s_mtcapciegen_driver = {
	.name = DRV_NAME,
	.id_table = s_mtcapciegen_table,
	.probe = Mtcapciegen_probe,
	.remove = __devexit_p(Mtcapciegen_remove),
};



static int GainAccessToDevicePCI1(struct SDeviceParams* a_pDevParams)
{
	kernel_ulong_t ullnDrvData;
	pid_t unPID = current->group_leader->pid;
	int result;

	SET_SLOT_AND_PID(&ullnDrvData, a_pDevParams->slot_num, unPID);

	if (a_pDevParams->vendor_id<0) a_pDevParams->vendor_id = PCI_ANY_ID;
	if (a_pDevParams->device_id<0) a_pDevParams->device_id = PCI_ANY_ID;
	if (a_pDevParams->subvendor_id<0) a_pDevParams->subvendor_id = PCI_ANY_ID;
	if (a_pDevParams->subdevice_id<0) a_pDevParams->subdevice_id = PCI_ANY_ID;

	DEBUG2("Calling pci_add_dynidMy\n");

	result = pci_add_dynid_zeuthen(&s_mtcapciegen_driver, a_pDevParams->vendor_id, a_pDevParams->device_id,
		a_pDevParams->subvendor_id, a_pDevParams->subdevice_id, 0, 0, ullnDrvData);

	//wait_for_device_probe();	// Waiting is not done, if user space needs, then information will
	//							// be provided by interrupt
	return result;
}



static inline int RemoveDevicesForID(struct pci_device_id* a_id)
{
	int nRet = ENODEV, i = 0;

	for (; i < PCIE_GEN_NR_DEVS; ++i)
	{
		if (s_vPci_gens[i].m_id && ((!a_id) || _COMPARE_2_IDS1_(a_id, s_vPci_gens[i].m_id)))
		{
			device_release_driver(&(s_vPci_gens[i].m_mtcapice_dev->dev));
			s_vPci_gens[i].m_id = NULL; // kara ev chlini
			nRet = 0;
		}
	}
	return nRet;
}



static int ReleaseAccessToDevicePCI1(SDeviceParams* a_DeviceToRemove)
{
	int tmp_slot_num = a_DeviceToRemove->slot_num;

	if (tmp_slot_num < 0 || tmp_slot_num >= PCIE_GEN_NR_DEVS)
	{
		struct pci_device_id aID = {
			.vendor = a_DeviceToRemove->vendor_id < 0 ? PCI_ANY_ID : a_DeviceToRemove->vendor_id,
			.device = a_DeviceToRemove->device_id < 0 ? PCI_ANY_ID : a_DeviceToRemove->device_id,
			.subvendor = a_DeviceToRemove->subvendor_id < 0 ? PCI_ANY_ID : a_DeviceToRemove->subvendor_id,
			.subdevice = a_DeviceToRemove->subdevice_id < 0 ? PCI_ANY_ID : a_DeviceToRemove->subdevice_id,
			.class = 0,
			.class_mask = 0,
			.driver_data = 0,
		};
		return RemoveDevicesForID(&aID);
	}

	if (!s_vPci_gens[tmp_slot_num].m_PciGen.m_dev_sts) return -1;

	device_release_driver(&(s_vPci_gens[tmp_slot_num].m_mtcapice_dev->dev));
	ALERT("Removed device slot number is %d)\n", (int)s_vPci_gens[tmp_slot_num].m_PciGen.m_slot_num);
	return 0;
}



static int RemoveIDfromDriver(DriverIDparams* a_IDtoRemove)
{
	struct pci_device_id aID = {
		.vendor = a_IDtoRemove->vendor_id    < 0 ? PCI_ANY_ID : a_IDtoRemove->vendor_id,
		.device = a_IDtoRemove->device_id    < 0 ? PCI_ANY_ID : a_IDtoRemove->device_id,
		.subvendor = a_IDtoRemove->subvendor_id < 0 ? PCI_ANY_ID : a_IDtoRemove->subvendor_id,
		.subdevice = a_IDtoRemove->subdevice_id < 0 ? PCI_ANY_ID : a_IDtoRemove->subdevice_id,
		.class = 0,
		.class_mask = 0,
		.driver_data = 0,
	};

	RemoveDevicesForID(&aID);
	return pci_free_dynid_zeuthen(&s_mtcapciegen_driver, &aID);
}



static void FreeAllIDsAndDevices(void)
{
	RemoveDevicesForID(NULL);
	FreeAllIDsAndDrvDatas(&s_mtcapciegen_driver);
}


//#include <linux/pci.h>


static long  mtcagen_management_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	int err = 0;
	//SActiveDev	aActiveDev;
	SDeviceParams aDevicePars;
	struct ZnDrv_gen*	drv = filp->private_data;
	//struct pci_dev* pDev;
	//int nIndex;

	ALERT("cmd = %u,  arg = 0x%.16lX\n", cmd, arg);

	/*
	* extract the type and number bitfields, and don't decode
	* wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	*/
	if (_IOC_TYPE(cmd) != PCIE_GEN_IOC)    return -ENOTTY;
	if (_IOC_NR(cmd) > PCIE_GEN_IOC_MAXNR) return -ENOTTY;

	/*
	* the direction is a bitmask, and VERIFY_WRITE catches R/W
	* transfers. `Type' is user-oriented, while
	* access_ok is kernel-oriented, so the concept of "read" and
	* "write" is reversed
	*/
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));


	if (err)
	{
		PRINT_KERN_ALERT("access_ok Error\n");
		return -EFAULT;
	}

	if (EnterCritRegion(&drv->m_criticalReg))return -ERESTARTSYS;

	switch (cmd)
	{
	case PCIE_GEN_GAIN_ACCESS:
		printk(KERN_ALERT "PCIE_GEN_GAIN_ACCESS\n");
		if (copy_from_user(&aDevicePars, (SDeviceParams*)arg, sizeof(SDeviceParams)))
		{
			PRINT_KERN_ALERT("Info for device is not provided\n");
			LeaveCritRegion(&drv->m_criticalReg);
			return -EFAULT;
		}

		printk(KERN_ALERT "calling GainAccessToDevicePCI1\n");
		GainAccessToDevicePCI1(&aDevicePars);

		//copy_to_user( &((SActiveDev*)arg)->dev_pars,&aActiveDev.dev_pars,sizeof(SDeviceParams));

		break;

	case PCIE_GEN_RELASE_ACCESS:
		if (copy_from_user(&aDevicePars, (SDeviceParams*)arg, sizeof(SDeviceParams)))
		{
			WARNING("unable to get buffer from user (\"%s\", %u)!\n", current->comm, (unsigned)current->group_leader->pid);
			LeaveCritRegion(&drv->m_criticalReg);
			return -EFAULT;
		}
		err = ReleaseAccessToDevicePCI1(&aDevicePars);
		break;

	case PCIE_GEN_DRIVER_TYPE:
		return MANAGEMENT;
		break;

	case PCIE_GEN_IS_ZEUTHEN_DRV:
		return 1;

	case PCIE_GEN_REMOVE_PCI_ID:
		if (copy_from_user(&aDevicePars, (SDeviceParams*)arg, sizeof(SDeviceParams)))
		{
			WARNING("unable to get buffer from user (\"%s\", %u)!\n", current->comm, (unsigned)current->group_leader->pid);
			LeaveCritRegion(&drv->m_criticalReg);
			return -EFAULT;
		}

		err = RemoveIDfromDriver(&aDevicePars);
		break;

	case PCIE_GEN_REMOVE_ALL:
		FreeAllIDsAndDevices();
		break;

#if 0
	case PCIE_GEN_NEW_DEVICE_INFO:
		if (copy_from_user(&nIndex, (int*)arg, sizeof(int)) || nIndex <0 || nIndex >= PCIE_GEN_NR_DEVS)
		{
			LeaveCritRegion(&drv->m_criticalReg);
			ERR("Wrong index!\n");
			return -EFAULT;
		}

		pDev = s_vPci_gens[nIndex].m_mtcapice_dev;
		if (!pDev)
		{
			LeaveCritRegion(&drv->m_criticalReg);
			ERR("!\n");
			return -EFAULT;
		}
		aDevicePars.vendor_id = pDev->vendor;
		aDevicePars.device_id = pDev->device;
		aDevicePars.subvendor_id = pDev->subsystem_vendor;
		aDevicePars.subdevice_id = pDev->subsystem_device;
		aDevicePars.slot_num = s_vPci_gens[nIndex].m_PciGen.m_slot_num;

		err = copy_to_user((struct SDeviceParams*)arg, &aDevicePars, sizeof(SDeviceParams));
		break;
#endif

	default:
		break;
	}

	LeaveCritRegion(&drv->m_criticalReg);

	return err;
}



static struct file_operations s_mtcagen_management_fops = {
	.owner = THIS_MODULE,
	//.ioctl		=	mtcagen_management_ioctl,
	.unlocked_ioctl = mtcagen_management_ioctl,
	.compat_ioctl = mtcagen_management_ioctl,
	.open = mtcagen_management_open,
	.release = mtcagen_managent_release,
};



inline void ClearModule(void)
{
	dev_t devno = MKDEV(PCIE_GEN_MAJOR, PCIE_GEN_MINOR);

	if (GET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _PCI_DRV_REG))
	{
		FreeAllIDsAndDevices();
		pci_unregister_driver(&s_mtcapciegen_driver);
		SET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _PCI_DRV_REG, 0);
	}

	if (g_pcie_gen_IRQworkqueue)
	{
		flush_workqueue(g_pcie_gen_IRQworkqueue);
		destroy_workqueue(g_pcie_gen_IRQworkqueue);
		g_pcie_gen_IRQworkqueue = NULL;
	}

	if (GET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _PROC_ENTRY_CREATED))
	{
		remove_proc_entry("pcie_gen_fd", 0);
		SET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _PROC_ENTRY_CREATED, 0);
	}

	DEBUG1("DEVICE_ENTRY_CREATED = %d\n", GET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _DEVC_ENTRY_CREATED) ? 1 : 0);
	if (GET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _DEVC_ENTRY_CREATED))
	{
		device_destroy(s_pcie_gen_class, MKDEV(PCIE_GEN_MAJOR, s_mtca_menegment.m_dev_minor));
		SET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _DEVC_ENTRY_CREATED, 0);
	}

	if (GET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _MUTEX_CREATED))
	{
		//mutex_destroy(&g_pcie_gen0.m_dev_mut);
		DestroyCritRegion(&s_mtca_menegment.m_criticalReg);
		SET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _MUTEX_CREATED, 0);
	}

	if (GET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _CDEV_ADDED))
	{
		cdev_del(&s_mtca_menegment.m_cdev);
		SET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _CDEV_ADDED, 0);
	}

	if (GET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _CHAR_REG_ALLOCATED))
	{
		unregister_chrdev_region(devno, PCIE_GEN_NR_DEVS + 1);
		SET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _CHAR_REG_ALLOCATED, 0);
	}

	if (s_pcie_gen_class)
	{
		class_destroy(s_pcie_gen_class);
		s_pcie_gen_class = NULL;
	}
}



void __exit pcie_gen_cleanup_module(void)
{
	INFO("cleanup called for MTCA general module!\n");
	ClearModule();
}


struct SListTest{ struct list_head m_head; };



static int __init pcie_gen_init_module(void)
{
	struct proc_dir_entry*	pcie_gen_procdir;
	struct device*			device0;
	int   result = 0;
	dev_t dev = 0;
	int   devno = 0;
	int i;
	int j;

#if 0
	struct SListTest* pTest;
	struct SListTest aTest;
	struct SListTest aTest2;
	struct SListTest aTest3;

	printk(KERN_ALERT "sizeof(str_pcie_gen) = %d, sizeof(UIRQstructs) = %d\n", (int)sizeof(struct str_pcie_gen), (int)sizeof(union UIRQstructs));

	return -1;

#if 0
	struct file *f;
	char bufff[128];

	//https://www.howtoforge.com/reading-files-from-the-linux-kernel-space-module-driver-fedora-14
	f = filp_open("/doocs/develop/kalantar/scripts/change_to_13_3_0.bat", O_RDONLY, 0);

	printk(KERN_ALERT "f = %p\n", f);
	if (!f) goto NO_FILE_OK;

	printk(KERN_ALERT "OK!\n");
	bufff[0] = 0;
	f->f_op->read(f, bufff, 127, &f->f_pos); bufff[127] = 0;
	filp_close(f, NULL);

	printk(KERN_ALERT "OK2!\n");
	printk(KERN_ALERT "buf[0] = %d; buf = \"%s\"\n", (int)bufff[0], bufff);
	return -1;
NO_FILE_OK:
#endif

	printk(KERN_ALERT "Test.m_head.next = %p, aTest.m_head.prev = %p\n", aTest.m_head.next, aTest.m_head.prev);
	INIT_LIST_HEAD(&aTest.m_head);
	printk(KERN_ALERT "Test.m_head.next = %p, aTest.m_head.prev = %p\n", aTest.m_head.next, aTest.m_head.prev);
	i = 0;
	list_add_tail(&aTest2.m_head, &aTest.m_head);
	printk(KERN_ALERT "Test.m_head.next = %p, aTest.m_head.prev = %p\n", aTest.m_head.next, aTest.m_head.prev);
	list_add_tail(&aTest3.m_head, &aTest.m_head);
	printk(KERN_ALERT "Test.m_head.next = %p, aTest.m_head.prev = %p\n", aTest.m_head.next, aTest.m_head.prev);
	printk(KERN_ALERT "starting loop\n");
	list_for_each_entry(pTest, &aTest.m_head, m_head) {
		++i;
		printk(KERN_ALERT "i = %d\n", i);
	}
	printk(KERN_ALERT "Number of iter = %d, aTest.m_head.next = %p, aTest.m_head.prev = %p\n", i, aTest.m_head.next, aTest.m_head.prev);

	return -1;
#endif

#if defined(_ASM_X86_INIT_H)
#endif // Correct header is "arch/x86/include/asm/init.h"

	s_mtca_menegment.m_ulnFlag = 0;

	for (i = 0; i < PCIE_GEN_NR_DEVS; ++i) s_vPci_gens[i].m_PciGen.m_dev_sts = 0;

	PRINT_KERN_ALERT("Init module called.  LINUX_VERSION_CODE = %d\n", LINUX_VERSION_CODE);

	result = alloc_chrdev_region(&dev, PCIE_GEN_MINOR, PCIE_GEN_NR_DEVS + 1, DEVNAME);
	if (result)
	{
		ERR("Error allocating Major Number for mtcapcie driver.\n");
		goto errorOut;
	}
	SET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _CHAR_REG_ALLOCATED, 1);

	PCIE_GEN_MAJOR = MAJOR(dev);
	PRINT_KERN_ALERT("AFTER_INIT DRV_MAJOR is %d\n", PCIE_GEN_MAJOR);

	s_pcie_gen_class = class_create(THIS_MODULE, DRV_NAME);
	if (IS_ERR(s_pcie_gen_class))
	{
		result = PTR_ERR(s_pcie_gen_class);
		ERR("Error creating mtcapcie class err = %d. class = %p\n", result, s_pcie_gen_class);
		s_pcie_gen_class = NULL;
		goto errorOut;
	}
	DEBUG1("\n");

	devno = MKDEV(PCIE_GEN_MAJOR, PCIE_GEN_MINOR);

	cdev_init(&s_mtca_menegment.m_cdev, &s_mtcagen_management_fops);
	DEBUG1("\n");
	s_mtca_menegment.m_cdev.owner = THIS_MODULE;
	s_mtca_menegment.m_cdev.ops = &s_mtcagen_management_fops;

	result = cdev_add(&s_mtca_menegment.m_cdev, devno, 1);
	if (result)
	{
		ERR("Error %d adding devno%d num%d", result, devno, 0);
		goto errorOut;
	}
	DEBUG1("\n");
	SET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _CDEV_ADDED, 1);

	//mutex_init(&g_pcie_gen0.m_dev_mut);
	InitCritRegion(&s_mtca_menegment.m_criticalReg, DEFAULT_REG_TIMEOUT);
	DEBUG1("\n");
	SET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _MUTEX_CREATED, 1);

	s_mtca_menegment.m_dev_minor = PCIE_GEN_MINOR;

	device0 = device_create(s_pcie_gen_class, NULL, MKDEV(PCIE_GEN_MAJOR, s_mtca_menegment.m_dev_minor), NULL, "pciegen_menegment");
	DEBUG1("\n");
	if (IS_ERR(device0))
	{
		ERR("Device creation error!\n");
		goto errorOut;
	}
	SET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _DEVC_ENTRY_CREATED, 1);

	pcie_gen_procdir = vers_create_proc_entry("pcie_gen_fd", S_IFREG | S_IRUGO, &s_mtca_menegment, pcie_gen_procinfo);
	DEBUG1("\n");
	if (pcie_gen_procdir)
	{
		SET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _PROC_ENTRY_CREATED, 1);
	}
	else
	{
		WARNING("proc entry creation error!\n");
	}


	for (i = 0; i < PCIE_GEN_NR_DEVS; ++i)
	{
		//g_vPci_gens[i].m_PcieGen.m_dev_sts = 0;
		s_vPci_gens[i].m_PciGen.m_slot_num = 0;
		s_vPci_gens[i].m_PciGen.m_nCreateInsideBigStruct = 1;
		s_vPci_gens[i].m_id = NULL;
		for (j = 0; j < NUMBER_OF_BARS; ++j){ s_vPci_gens[i].m_PciGen.m_Memories[j].m_memory_base = 0; }
	}

	g_pcie_gen_IRQworkqueue = create_workqueue(DRV_NAME);
	DEBUG1("\n");

	result = pci_register_driver(&s_mtcapciegen_driver);
	if (result)
	{
		ERR("pci driver couldn't be registered!\n");
		goto errorOut;
	}
	SET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _PCI_DRV_REG, 1);

	return 0;

errorOut:
	ClearModule();
	return result > 0 ? -result : (result ? result : -1);
}

module_init(pcie_gen_init_module);
module_exit(pcie_gen_cleanup_module);
