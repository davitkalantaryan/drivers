/*
 *	File: pcie_gen.c
 *
 *	Created on: May 12, 2013
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *	This file contain definitions of ...
 *
 */

#define USEPCIPROBE

#include "pcie_gen_exp.h"

#include "pcie_gen_fnc.h"      	/* local definitions */
#include "version_dependence.h"

MODULE_AUTHOR("Davit Kalantaryan");
MODULE_DESCRIPTION("pcie_gen_drv General purpose driver ");
MODULE_VERSION("1.1.0");
MODULE_LICENSE("Dual BSD/GPL");

static u16 PCIE_GEN_DRV_VER_MAJ=1;
static u16 PCIE_GEN_DRV_VER_MIN=1;

struct Pcie_gen_dev			g_pcie_gen0;	/* allocated in iptimer_init_module */
struct str_pcie_gen			g_vPci_gens[PCIE_GEN_NR_DEVS];
struct class*				s_pcie_gen_class	= NULL;
static unsigned long int	s_ulnFlag0			= 0;


static int pcie_gen_procinfo(char *buf, char **start, off_t fpos, int lenght, int *eof, void *data)
{
	char *p;
	struct Pcie_gen_dev *dev = data;
	p = buf;
	p += sprintf(p,"Driver Version:\t%i.%i\n", PCIE_GEN_DRV_VER_MAJ, PCIE_GEN_DRV_VER_MIN);
	p += sprintf(p,"STATUS Revision:\t%i\n", dev->m_dev_sts);
	*eof = 1;
	return p - buf;
}


static int PCIE_GEN_MAJOR = 46;     /* static major by default */
static int PCIE_GEN_MINOR = 0;      /* static minor by default */


static int pcie_gen_open( struct inode *inode, struct file *filp )
{
	int    minor;
	struct Pcie_gen_dev *dev2; /* device information */

	minor = iminor(inode);
	dev2 = container_of(inode->i_cdev, struct Pcie_gen_dev, m_cdev);
	dev2->m_dev_minor       = minor;
	filp->private_data   = dev2; /* for other methods */

	PRINT_KERN_ALERT("Open Procces is \"%s\" (pid %i) DEV is %d \n",current->comm,current->group_leader->pid,minor);

	return 0;
}



static int pcie_gen_release(struct inode *inode, struct file *filp)
{
	
	int minor     = 0;
	int d_num     = 0;
	u16 cur_proc  = 0;
	
	struct Pcie_gen_dev      *dev;
	
	dev = filp->private_data;
	
	minor = dev->m_dev_minor;
	d_num = dev->m_dev_num;
	
	cur_proc = current->group_leader->pid;
	
	if (EnterCritRegion(&dev->m_criticalReg))return -ERESTARTSYS;
	
	PRINT_KERN_ALERT("Close Procces is \"%s\" (pid %i)\n", current->comm, current->pid);
	PRINT_KERN_ALERT("Close MINOR = %d, DEV_NUM = %d \n", minor, d_num);
	
	LeaveCritRegion(&dev->m_criticalReg);
	return 0;
}


#define	METHOD__	2
static inline int GetSlotNumber(struct pci_dev *a_pPciDev)
{

#if METHOD__ == 0

	int pcie_cap;
	u32 tmp_slot_cap = 0;
	struct pci_dev* pCurDev = a_pPciDev, *pCurDevTemp;
	struct pci_bus  *pCurBus = a_pPciDev ? a_pPciDev->bus : NULL;

	int nDebug = 0;

	while (pCurBus && nDebug<100)
	{
		pCurBus = pCurBus->parent;
		pCurDevTemp = pCurBus ? pCurBus->self : NULL;

		printk(KERN_ALERT "+++++++++++%d  pCurDevTemp = %p,  pCurBus = %p", nDebug++, pCurDevTemp, pCurBus);
		if (pCurDevTemp)
		{
			pCurDev = pCurDevTemp;
			pcie_cap = pci_find_capability(pCurDevTemp, PCI_CAP_ID_EXP);
			pci_read_config_dword(pCurDev, (pcie_cap + PCI_EXP_SLTCAP), &tmp_slot_cap);
			printk(KERN_CONT ", slot = %d\n", (int)((tmp_slot_cap >> 19) % PCIE_GEN_NR_DEVS));
		}
		else
		{
			printk(KERN_CONT "\n");
		}

		/*if (!pCurDevTemp)
		{
			pcie_cap = pci_find_capability(pCurDev, PCI_CAP_ID_EXP);
			pci_read_config_dword(pCurDev, (pcie_cap + PCI_EXP_SLTCAP), &tmp_slot_cap);
			printk(KERN_ALERT "++++++++++%d   slot = %d", nDebug,(int)((tmp_slot_cap >> 19) % PCIE_GEN_NR_DEVS));
			return (tmp_slot_cap >> 19) % PCIE_GEN_NR_DEVS;
		}*/

		pCurDev = pCurDevTemp;
	}

	if (pCurDev)
	{
		pcie_cap = pci_find_capability(pCurDev, PCI_CAP_ID_EXP);
		pci_read_config_dword(pCurDev, (pcie_cap + PCI_EXP_SLTCAP), &tmp_slot_cap);
		printk(KERN_ALERT "++++++++++   slot = %d", (int)((tmp_slot_cap >> 19) % PCIE_GEN_NR_DEVS));
		return (tmp_slot_cap >> 19) % PCIE_GEN_NR_DEVS;
	}

	return 0;

#elif METHOD__ == 1

	int pcie_cap2 = (int)a_pPciDev->pcie_cap;
	int pcie_cap;
	u32 tmp_slot_cap = 0;
	u32 tmp_slot_cap2 = 0;

	if (a_pPciDev->bus)
	{
		if (a_pPciDev->bus->parent)
		{
			if (a_pPciDev->bus->parent->self)
			{
				pcie_cap = pci_find_capability(a_pPciDev->bus->parent->self, PCI_CAP_ID_EXP);
				//pcie_cap = pci_find_capability(a_pPciDev->bus->parent->self, PCI_CAP_ID_EXP);
				pci_read_config_dword(a_pPciDev->bus->parent->self, (pcie_cap + PCI_EXP_SLTCAP), &tmp_slot_cap);
				pci_read_config_dword(a_pPciDev->bus->parent->self, (pcie_cap2 + PCI_EXP_SLTCAP), &tmp_slot_cap2);
				if (a_pPciDev->slot)
				{
					printk(KERN_ALERT "+-+-+-+- pcie_cap1=%d,  pcie_cap2=%d, tslot1=%d, tslot2=%d, slot1=%d, slot2=%d slot3=%d\n",
						pcie_cap, pcie_cap2,
						(int)tmp_slot_cap, (int)tmp_slot_cap2,
						(int)((tmp_slot_cap >> 19) % PCIE_GEN_NR_DEVS), (int)((tmp_slot_cap2 >> 19) % PCIE_GEN_NR_DEVS),
						(int)a_pPciDev->slot->number);
				}
				else
				{
					printk(KERN_ALERT "+-+-+-+- pcie_cap1=%d,  pcie_cap2=%d, tslot1=%d, tslot2=%d, slot1=%d, slot2=%d NoSlot3\n",
						pcie_cap, pcie_cap2,
						(int)tmp_slot_cap, (int)tmp_slot_cap2,
						(int)((tmp_slot_cap >> 19) % PCIE_GEN_NR_DEVS), (int)((tmp_slot_cap2 >> 19) % PCIE_GEN_NR_DEVS) );
				}
				return (tmp_slot_cap >> 19) % PCIE_GEN_NR_DEVS;
			}

		} // if (a_pPciDev->bus->parent)

	} // if (a_pPciDev->bus)

	return -1;

#elif METHOD__ == 2
	int pcie_cap;
	u32 tmp_slot_cap = 0;

	if (a_pPciDev->bus)
	{
		if (a_pPciDev->bus->parent)
		{
			if (a_pPciDev->bus->parent->self)
			{

				pcie_cap = pci_find_capability(a_pPciDev->bus->parent->self, PCI_CAP_ID_EXP);

				//pci_read_config_dword(dev->bus->parent->self, (pcie_cap + PCI_EXP_SLTCAP), &tmp_slot_cap);
				//tmp_slot_num = (tmp_slot_cap >> 19);

				//pcie_cap = pci_find_capability(a_pPciDev->bus->parent->self, PCI_CAP_ID_EXP);
				pci_read_config_dword(a_pPciDev->bus->parent->self, (pcie_cap + PCI_EXP_SLTCAP), &tmp_slot_cap);
				return (tmp_slot_cap >> 19) % PCIE_GEN_NR_DEVS;
			}

		} // if (a_pPciDev->bus->parent)

	} // if (a_pPciDev->bus)

	return -1;
#else

#error "Not correct method"

#endif
}



static struct pci_dev* FindDevice(struct SDeviceParams* a_pActiveDev, int* a_tmp_slot_num_ptr)
{
	int nIteration						= 0;
	struct pci_dev *pPciTmp				= NULL;
	struct pci_dev *pPcieRet			= NULL;

	if(a_pActiveDev->vendor_id<0) a_pActiveDev->vendor_id = PCI_ANY_ID;
	if(a_pActiveDev->device_id<0) a_pActiveDev->device_id = PCI_ANY_ID;
	if (a_pActiveDev->subvendor_id<0) a_pActiveDev->subvendor_id = PCI_ANY_ID;
	if (a_pActiveDev->subdevice_id<0) a_pActiveDev->subdevice_id = PCI_ANY_ID;

	PRINT_KERN_ALERT("vendor_id = %d, device_id = %d, subvendor_id = %d, subdevice_id = %d\n",
		a_pActiveDev->vendor_id, a_pActiveDev->device_id, a_pActiveDev->subvendor_id, a_pActiveDev->subdevice_id);


	while( (a_pActiveDev->slot_num>=0)||(nIteration==0) )
	{
		//pPcieRet = pci_get_device( a_pActiveDev->vendor_id,a_pActiveDev->device_id,pPciTmp );
		pPcieRet = pci_get_subsys(a_pActiveDev->vendor_id, a_pActiveDev->device_id, 
									a_pActiveDev->subvendor_id, a_pActiveDev->subdevice_id,pPciTmp);

		if(!pPcieRet)break; // This means that end of global pci devices list reached

		if (((*a_tmp_slot_num_ptr = GetSlotNumber(pPcieRet)) >= 0))
		{
			if (*a_tmp_slot_num_ptr == a_pActiveDev->slot_num || a_pActiveDev->slot_num<0)
			{
				return pPcieRet;
			}
			++nIteration;
		}

		pPciTmp = pPcieRet;	
	}


	return NULL;
}



void FindDeviceForRelase(struct SDeviceParams* a_pActiveDev, int* a_tmp_slot_num_ptr)
{

	int i;

	*a_tmp_slot_num_ptr = -1;

	if(a_pActiveDev->slot_num>=0)
	{
		a_pActiveDev->slot_num %= PCIE_GEN_NR_DEVS;

		if(g_vPci_gens[a_pActiveDev->slot_num].m_PcieGen.m_dev_sts)
		{
			*a_tmp_slot_num_ptr = a_pActiveDev->slot_num;
		}

		return;
	}


	if( a_pActiveDev->vendor_id<0 && a_pActiveDev->device_id<0 )return;

	
	for( i = 0; i < PCIE_GEN_NR_DEVS; ++i )
	{
		if(g_vPci_gens[i].m_PcieGen.m_dev_sts)
		{
			if( (a_pActiveDev->vendor_id<0 || a_pActiveDev->vendor_id==g_vPci_gens[i].m_mtcapice_dev->vendor) &&
				(a_pActiveDev->device_id<0 || a_pActiveDev->device_id==g_vPci_gens[i].m_mtcapice_dev->device) &&
				(a_pActiveDev->subvendor_id<0 || a_pActiveDev->subvendor_id == g_vPci_gens[i].m_mtcapice_dev->subsystem_vendor) &&
				(a_pActiveDev->subdevice_id<0 || a_pActiveDev->subdevice_id == g_vPci_gens[i].m_mtcapice_dev->subsystem_device))
			{
				*a_tmp_slot_num_ptr = g_vPci_gens[i].m_PcieGen.m_slot_num;
				return;
			}
		}
	}

	
}



static inline void ReleaseAccessToDevicePCI1privateBySlot(int a_tmp_slot_num)
{

	if (g_vPci_gens[a_tmp_slot_num].m_PcieGen.m_dev_sts)
	{
		(*pci_bus_type.remove)(&(g_vPci_gens[a_tmp_slot_num].m_mtcapice_dev->dev));
		Gen_ReleaseAccsess(&g_vPci_gens[a_tmp_slot_num], PCIE_GEN_MAJOR, s_pcie_gen_class);
		printk(KERN_ALERT "!!!!!!!! Putting device1!!!\n");
		pci_dev_put(g_vPci_gens[a_tmp_slot_num].m_mtcapice_dev);
	}
}



static inline int ReleaseAccessToDevicePCI1private(SDeviceParams* a_DeviceToRemove)
{
	int tmp_slot_num = 0;

	FindDeviceForRelase(a_DeviceToRemove, &tmp_slot_num);

	if (tmp_slot_num<0)
	{
		PRINT_KERN_ALERT("Couldn't find device\n");
		return -EFAULT;
	}

	ReleaseAccessToDevicePCI1privateBySlot(tmp_slot_num);
	return 0;
}



static int __devinit Mtcapciegen_probe(struct pci_dev *dev, const struct pci_device_id *id)
{

#ifdef USEPCIPROBE

	int tmp_slot_num = (int)(id->driver_data);
	static int nIteration = 0;

	printk(KERN_ALERT "================================================ Mtcapciegen_probe %d\n", ++nIteration);

	ALERT("dev->vendor = %d\n", dev->vendor);

	if (tmp_slot_num < 0) tmp_slot_num = GetSlotNumber(dev) % PCIE_GEN_NR_DEVS; // % PCIE_GEN_NR_DEVS done in GetSlotNumber

	if (g_vPci_gens[tmp_slot_num].m_PcieGen.m_dev_sts)
	{
		ERR("Slot already in use!\n");
		return -1;
	}

	dev_set_drvdata(&(dev->dev), (&g_vPci_gens[tmp_slot_num]));

	g_vPci_gens[tmp_slot_num].m_mtcapice_dev = dev;
	g_vPci_gens[tmp_slot_num].m_PcieGen.m_dev_sts = 1;
	g_vPci_gens[tmp_slot_num].m_PcieGen.m_dev_minor = PCIE_GEN_MINOR + tmp_slot_num + 1;

	ALERT("1. before General_GainAccess1\n");
	if (General_GainAccess1(&g_vPci_gens[tmp_slot_num], PCIE_GEN_MAJOR, s_pcie_gen_class, "pcie_gen_drv"))
	{
		g_vPci_gens[tmp_slot_num].m_PcieGen.m_dev_sts = 0;
		return -1;
	}

#endif

	return 0;
}



static void __devexit Mtcapciegen_remove(struct pci_dev *a_dev)
{
	struct str_pcie_gen* mpdev = dev_get_drvdata(&(a_dev->dev));
	int tmp_slot_num = mpdev->m_PcieGen.m_slot_num;

	DEBUG1("!!!!!!!!!!!!!!!!!!!!++++++++\n");

	ReleaseAccessToDevicePCI1privateBySlot(tmp_slot_num);
}


static struct pci_device_id s_mtcapciegen_table[] __devinitdata = { { 0, } };
MODULE_DEVICE_TABLE(pci, s_mtcapciegen_table);


static struct pci_driver s_mtcapciegen_driver = {
	.name = DRV_NAME,
	.id_table = s_mtcapciegen_table,
	.probe = Mtcapciegen_probe,
	.remove = __devexit_p(Mtcapciegen_remove),
};



static inline void GetIRQinfoFromUser(const struct SActiveDev* a_pActiveDev, int a_nIndex)
{
	if (!g_vPci_gens[a_nIndex].m_PcieGen.m_dev_sts)
	{
		struct device_irq_reset* pIRQreset;
		int i = 0;
		
		g_vPci_gens[a_nIndex].irq_type = a_pActiveDev->irq_type;
		g_vPci_gens[a_nIndex].m_nNumberOfIrqRW = 0;

		if (g_vPci_gens[a_nIndex].irq_type == FIRST_IRQ_TYPE)
		{
			//memcpy( g_vPci_gens[tmp_slot_num].m_rst_irq,aActiveDev.reset_irq,sizeof(aActiveDev.reset_irq) );
			pIRQreset = (device_irq_reset*)a_pActiveDev->irq_info;

			printk(KERN_ALERT "before for\n");

			for (i = 0; i < NUMBER_OF_BARS; ++i)
			{
				printk(KERN_ALERT "copiing reset info for %d\n", i);
				if (copy_from_user(&g_vPci_gens[a_nIndex].m_rst_irq[i], pIRQreset++, sizeof(device_irq_reset)))
				{
					printk(KERN_ALERT "i = %d, Unable to copy_from_user\n", i);
					break;
				}


				if (g_vPci_gens[a_nIndex].m_rst_irq[i].bar_rw < 0 ||
					g_vPci_gens[a_nIndex].m_rst_irq[i].bar_rw >= NUMBER_OF_BARS)
				{
					printk(KERN_ALERT "i = %d, bar_rw = %d\n", i, (int)g_vPci_gens[a_nIndex].m_rst_irq[i].bar_rw);
					break;
				}

				++(g_vPci_gens[a_nIndex].m_nNumberOfIrqRW);

			} // end  for (i = 0; i < NUMBER_OF_BARS; ++i)
		
		} // if (g_vPci_gens[a_nIndex].irq_type == FIRST_IRQ_TYPE)

	} // if (!g_vPci_gens[a_nIndex].m_PcieGen.m_dev_sts)
}



#ifdef USEPCIPROBE

//#define TEST_STORE

static int GainAccessToDevicePCI1(struct SActiveDev* a_pActiveDev)
{

#ifdef TEST_STORE
	char vcBuffer[64];
	int nLen;
	const struct attribute_group **ppDrv_groups = pci_bus_type.drv_groups;
	const struct attribute_group *pDrv_groups = *ppDrv_groups;
	struct attribute ** ppAttrs = pDrv_groups->attrs;
	struct driver_attribute* pDrvAttr = container_of(ppAttrs[0], struct driver_attribute, attr);
#endif



	struct pci_dev *pPCI;
	int result, tmp_slot_num = 0;
	int i = 0;

	//if (a_pActiveDev->dev_pars.vendor_id < 0 && a_pActiveDev->dev_pars.device_id < 0 &&
	//	a_pActiveDev->dev_pars.subvendor_id < 0 && a_pActiveDev->dev_pars.subdevice_id < 0)
	{
		pPCI = FindDevice(&a_pActiveDev->dev_pars, &tmp_slot_num);
		if (!pPCI)
		{
			printk(KERN_ALERT "pPCI = %p\n", pPCI);
			return -EFAULT;
		}

		a_pActiveDev->dev_pars.vendor_id = pPCI->vendor;;
		a_pActiveDev->dev_pars.device_id = pPCI->device;
		a_pActiveDev->dev_pars.subvendor_id = pPCI->subsystem_vendor;
		a_pActiveDev->dev_pars.subdevice_id = pPCI->subsystem_device;
		//a_pActiveDev->dev_pars.slot_num = g_vPci_gens[tmp_slot_num].m_PcieGen.m_slot_num;
		a_pActiveDev->dev_pars.slot_num = GetSlotNumber(pPCI);
		printk(KERN_ALERT "++++ a_pActiveDev->dev_pars.slot_num = %d\n", (int)a_pActiveDev->dev_pars.slot_num);

	}

	for (; i < PCIE_GEN_NR_DEVS; ++i){ GetIRQinfoFromUser(a_pActiveDev, i); }

	printk(KERN_ALERT "+++++++++++++++GainAccessToDevicePCI1\n");

	pci_dev_put(pPCI);
	
#ifndef TEST_STORE
	result = pci_add_dynid(&s_mtcapciegen_driver, a_pActiveDev->dev_pars.vendor_id, a_pActiveDev->dev_pars.device_id, 
		a_pActiveDev->dev_pars.subvendor_id, a_pActiveDev->dev_pars.subdevice_id, 0, 0, (kernel_ulong_t)tmp_slot_num);
#else
	

	nLen = snprintf(vcBuffer, 64, "%x %x %x %x %x %x %lx", (__u32)a_pActiveDev->dev_pars.vendor_id, 
			(__u32)a_pActiveDev->dev_pars.device_id,
			(__u32)a_pActiveDev->dev_pars.subvendor_id, (__u32)a_pActiveDev->dev_pars.subdevice_id, 0, 0, (unsigned long)tmp_slot_num);

	printk(KERN_ALERT "nLen = %d\n", nLen);
	printk(KERN_ALERT "ppDrv_groups = %p\n", ppDrv_groups);
	printk(KERN_ALERT "pDrv_groups = %p\n", pDrv_groups);
	printk(KERN_ALERT "ppAttrs = %p\n", ppAttrs);
	printk(KERN_ALERT "ppAttrs[0] = %p\n", ppAttrs[0]);
	printk(KERN_ALERT "pDrvAttr = %p\n", pDrvAttr);
	printk(KERN_ALERT "pDrvAttr->store = %p\n", pDrvAttr->store);
	printk(KERN_ALERT "vcBuffer = \"%s\"\n",vcBuffer);
	result = pDrvAttr->store(&(s_mtcapciegen_driver.driver), vcBuffer, nLen);
	printk(KERN_ALERT "+++++result = %d(%s)\n", result, result == -ENODEV ? "-ENODEV" : result == -EINVAL ? "-EINVAL":"Unknown");
	
	printk(KERN_ALERT "!!!!!!!!!!!result = %d\n", result);

	result = result > 0 ? 0 : result;

#endif
	return result;
}


struct pci_dynid {
	struct list_head node;
	struct pci_device_id id;
};


static int pci_free_dynidMy(struct pci_driver *pdrv, const struct pci_device_id *id)
{
	struct pci_dynid *dynid, *n;
	__u32 vendor, device, subvendor = PCI_ANY_ID,
		subdevice = PCI_ANY_ID, class = 0, class_mask = 0;
	int retval = -ENODEV;

	vendor = id->vendor;
	device = id->device;
	subvendor = id->subvendor;
	subdevice = id->subdevice;
	class = id->class;
	class_mask = id->class_mask;

	spin_lock(&pdrv->dynids.lock);
	list_for_each_entry_safe(dynid, n, &pdrv->dynids.list, node) {
		struct pci_device_id *id = &dynid->id;
		if ((id->vendor == vendor) &&
			(id->device == device) &&
			(subvendor == PCI_ANY_ID || id->subvendor == subvendor) &&
			(subdevice == PCI_ANY_ID || id->subdevice == subdevice) &&
			!((id->class ^ class) & class_mask)) {
			printk(KERN_ALERT "!!!!!!!!!!!!!!!!!!!!!!!found for delete!\n");
			list_del(&dynid->node);
			kfree(dynid);
			retval = 0;
			break;
		}
	}
	spin_unlock(&pdrv->dynids.lock);

	if (retval)
		return retval;
	return 0;

}


static int ReleaseAccessToDevicePCI1(SDeviceParams* a_DeviceToRemove)
{

#if 1
	int nRet;
	struct pci_device_id aID = {
		.vendor = a_DeviceToRemove->vendor_id,
		.device = a_DeviceToRemove->device_id,
		.subvendor = a_DeviceToRemove->subvendor_id,
		.subdevice = a_DeviceToRemove->subdevice_id,
		.class = 0,
		.class_mask = 0,
		.driver_data = 0,
	};
	nRet = pci_free_dynidMy(&s_mtcapciegen_driver,&aID);

	ALERT("+++nRet = %d, a_DeviceToRemove->slot_num = %d\n", nRet, (int)a_DeviceToRemove->slot_num);

	if(!nRet)ReleaseAccessToDevicePCI1privateBySlot(a_DeviceToRemove->slot_num);

	return nRet;
#else

	char vcBuffer[64];
	const struct attribute_group **ppDrv_groups = pci_bus_type.drv_groups;
	const struct attribute_group *pDrv_groups = *ppDrv_groups;
	struct attribute ** ppAttrs = pDrv_groups->attrs;
	struct driver_attribute* pDrvAttr = container_of(ppAttrs[1], struct driver_attribute, attr);

	unsigned int vendor_id = a_DeviceToRemove->vendor_id > 0 ? a_DeviceToRemove->vendor_id : PCI_ANY_ID;
	unsigned int device_id = a_DeviceToRemove->device_id > 0 ? a_DeviceToRemove->device_id : PCI_ANY_ID;
	unsigned int subvendor_id = a_DeviceToRemove->subvendor_id > 0 ? a_DeviceToRemove->subvendor_id : PCI_ANY_ID;
	unsigned int subdevice_id = a_DeviceToRemove->subdevice_id > 0 ? a_DeviceToRemove->subdevice_id : PCI_ANY_ID;

	//printk(KERN_ALERT "---tamc200_table[0].class = %d, tamc200_table[0].class = %d, tamc200_table[0].class = %d\n",
	//	(int)tamc200_table[0].class, (int)tamc200_table[0].class_mask, (int)tamc200_table[0].driver_data);
	int result;
	int nLen = snprintf(vcBuffer, 64, "%x %x %x %x %x %x", vendor_id, device_id, subvendor_id, subdevice_id, 0, 0);

	printk(KERN_ALERT "ppDrv_groups = %p\n", ppDrv_groups);
	printk(KERN_ALERT "pDrv_groups = %p\n", pDrv_groups);
	printk(KERN_ALERT "ppAttrs = %p\n", ppAttrs);
	printk(KERN_ALERT "ppAttrs[1] = %p\n", ppAttrs[1]);
	printk(KERN_ALERT "pDrvAttr = %p\n", pDrvAttr);
	printk(KERN_ALERT "pDrvAttr->store = %p\n", pDrvAttr->store);
	result = pDrvAttr->store(&(s_mtcapciegen_driver.driver), vcBuffer, nLen);
	printk(KERN_ALERT "+++++result = %d(%s)\n", result, result == -ENODEV ? "-ENODEV" : "Unknown");

	return result;
#endif

}

#else  // #ifdef USEPCIPROBE

static int GainAccessToDevicePCI1(struct SActiveDev* a_pActiveDev)
{
	int tmp_slot_num = 0;
	
	struct pci_dev *pPCI = FindDevice(&a_pActiveDev->dev_pars, &tmp_slot_num);
	if (!pPCI)
	{
		PRINT_KERN_ALERT("Couldn't find device\n");
		return -EFAULT;
	}

	if(g_vPci_gens[tmp_slot_num].m_PcieGen.m_dev_sts)
	{
		ERR("Device in this slot already under control!!!\n");
		return -1;
	}

	a_pActiveDev->dev_pars.vendor_id = pPCI->vendor;
	a_pActiveDev->dev_pars.device_id = pPCI->device;
	a_pActiveDev->dev_pars.subvendor_id = pPCI->subsystem_vendor;
	a_pActiveDev->dev_pars.subdevice_id = pPCI->subsystem_device;
	//a_pActiveDev->dev_pars.slot_num = g_vPci_gens[tmp_slot_num].m_PcieGen.m_slot_num;
	a_pActiveDev->dev_pars.slot_num = tmp_slot_num;

	g_vPci_gens[tmp_slot_num].m_mtcapice_dev = pPCI;
	g_vPci_gens[tmp_slot_num].m_PcieGen.m_dev_minor = PCIE_GEN_MINOR + tmp_slot_num + 1;

	GetIRQinfoFromUser(a_pActiveDev, tmp_slot_num);
	g_vPci_gens[tmp_slot_num].m_PcieGen.m_dev_sts = 1;

	printk(KERN_ALERT "before General_GainAccess1\n");
	if( General_GainAccess1(&g_vPci_gens[tmp_slot_num], PCIE_GEN_MAJOR, s_pcie_gen_class, "pcie_gen_drv") )
	{
		g_vPci_gens[tmp_slot_num].m_PcieGen.m_dev_sts = 0;
		return -1;
	}

	return 0;
}



static int ReleaseAccessToDevicePCI1(SDeviceParams* a_DeviceToRemove)
{
	return ReleaseAccessToDevicePCI1private(a_DeviceToRemove);
}


#endif // #ifdef USEPCIPROBE



long  pcie_gen_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	int err = 0;
	SActiveDev	aActiveDev;
	struct Pcie_gen_dev*	dev = filp->private_data;

	PRINT_KERN_ALERT("cmd = %d,  arg = 0x%.16lX\n",(int)cmd,(long)arg);

	if( !dev->m_dev_sts )
	{
		PRINT_KERN_ALERT("No device!\n");
		return -EFAULT;
	}


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
		err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	
	if( err )
	{
		PRINT_KERN_ALERT( "access_ok Error\n");
		return -EFAULT;
	}
	
	if (EnterCritRegion(&dev->m_criticalReg))return -ERESTARTSYS;

	printk(KERN_ALERT "before switch\n");

	switch(cmd)
	{
	case PCIE_GEN_GAIN_ACCESS:
		printk(KERN_ALERT "PCIE_GEN_GAIN_ACCESS\n");
		if( copy_from_user(&aActiveDev,(SActiveDev*)arg,sizeof(SActiveDev))  )
		{
			PRINT_KERN_ALERT("Info for device is not provided\n");
			LeaveCritRegion(&dev->m_criticalReg);
			return -EFAULT;
		}

		GainAccessToDevicePCI1(&aActiveDev);

		copy_to_user( &((SActiveDev*)arg)->dev_pars,&aActiveDev.dev_pars,sizeof(SDeviceParams));
	
		break;

	case PCIE_GEN_RELASE_ACCESS:
		if( copy_from_user(&aActiveDev.dev_pars,(SDeviceParams*)arg,sizeof(SDeviceParams))  )
		{
			printk(KERN_ALERT "unable to get buffer from user!\n");
			LeaveCritRegion(&dev->m_criticalReg);
			return -EFAULT;
		}

		err = ReleaseAccessToDevicePCI1(&aActiveDev.dev_pars);
		break;

	default:
		break;
	}

	LeaveCritRegion(&dev->m_criticalReg);
	
	return err;
}




static int pcie_gen_open_tot( struct inode *inode, struct file *filp )
{
	int    minor;
	struct str_pcie_gen *dev; /* device information */
	struct Pcie_gen_dev *dev2; /* device information */

	minor = iminor(inode);
	dev2 = container_of(inode->i_cdev, struct Pcie_gen_dev, m_cdev);
	dev = container_of(dev2, struct str_pcie_gen, m_PcieGen);
	dev->m_PcieGen.m_dev_minor       = minor;
	filp->private_data   = dev; /* for other methods */

	PRINT_KERN_ALERT("Open Procces is \"%s\" (pid %i) DEV is %d \n", current->comm, current->group_leader->pid, minor);

	return 0;
}



static int pcie_gen_release_tot(struct inode *inode, struct file *filp)
{
	int minor     = 0;
	int d_num     = 0;
	u16 cur_proc  = 0;

	struct str_pcie_gen      *dev;

	dev = filp->private_data;

	minor = dev->m_PcieGen.m_dev_minor;
	d_num = dev->m_PcieGen.m_dev_num;
	cur_proc = current->group_leader->pid;
	
	if (EnterCritRegion(&dev->m_PcieGen.m_criticalReg))return -ERESTARTSYS;

	PRINT_KERN_ALERT("Close Procces is \"%s\" (pid %i)\n", current->comm, current->pid);
	PRINT_KERN_ALERT("Close MINOR %d DEV_NUM %d \n", minor, d_num);
	
	LeaveCritRegion(&dev->m_PcieGen.m_criticalReg);

	return 0;
}



static ssize_t pcie_gen_read_tot(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct str_pcie_gen  *dev = filp->private_data;
	return General_Read_Fops(&dev->m_PcieGen, buf, count);
}



static ssize_t pcie_gen_write_tot(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct str_pcie_gen  *dev = filp->private_data;
	return General_Write_Fops(&dev->m_PcieGen,buf,count);
}


static long  pcie_gen_ioctl_tot(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct str_pcie_gen*	dev = filp->private_data;
	return General_ioctl_mini(dev,cmd,arg);
}




struct file_operations pcie_gen_fops = {
	.owner			=	THIS_MODULE,
	//.read			=	test_read,
	//.write		=	test_write,
	//.ioctl		=	pcie_gen_ioctl,
	.unlocked_ioctl	=	pcie_gen_ioctl,
	.compat_ioctl	=	pcie_gen_ioctl,
	.open			=	pcie_gen_open,
	.release		=	pcie_gen_release,
};



struct file_operations pcie_gen_fops_tot = {
	.owner			=	THIS_MODULE,
	.read			=	pcie_gen_read_tot,
	.write			=	pcie_gen_write_tot,
	//.ioctl		=	pcie_gen_ioctl,
	.unlocked_ioctl	=	pcie_gen_ioctl_tot,
	.compat_ioctl	=	pcie_gen_ioctl,
	.open			=	pcie_gen_open_tot,
	.release		=	pcie_gen_release_tot,
};





inline void ClearModule(void)
{
	int i;

	dev_t devno = MKDEV(PCIE_GEN_MAJOR, PCIE_GEN_MINOR);

	if (g_pcie_gen_IRQworkqueue)
	{
		flush_workqueue(g_pcie_gen_IRQworkqueue);
		destroy_workqueue(g_pcie_gen_IRQworkqueue);
		g_pcie_gen_IRQworkqueue = NULL;
	}

	for (i = 0; i < PCIE_GEN_NR_DEVS; ++i)
	{
		if (g_vPci_gens[i].m_PcieGen.m_dev_sts)
		{
			Gen_ReleaseAccsess(&g_vPci_gens[i], PCIE_GEN_MAJOR, s_pcie_gen_class);
			//printk(KERN_ALERT "Trying to put device\n");
			printk(KERN_ALERT "!!!!!!!! Putting device2!!!\n");
			pci_dev_put(g_vPci_gens[i].m_mtcapice_dev);
			//printk(KERN_ALERT "put device done\n");
			g_vPci_gens[i].m_PcieGen.m_dev_sts = 0;
		}
	}

	if (GET_FLAG_VALUE(s_ulnFlag0, _PROC_ENTRY_CREATED))
	{
		remove_proc_entry("pcie_gen_fd", 0);
		SET_FLAG_VALUE(s_ulnFlag0, _PROC_ENTRY_CREATED, 0);
	}

	if (GET_FLAG_VALUE(s_ulnFlag0, _DEVC_ENTRY_CREATED))
	{
		device_destroy(s_pcie_gen_class, MKDEV(PCIE_GEN_MAJOR, g_pcie_gen0.m_dev_minor));
		SET_FLAG_VALUE(s_ulnFlag0, _DEVC_ENTRY_CREATED, 0);
	}

	if (GET_FLAG_VALUE(s_ulnFlag0, _MUTEX_CREATED))
	{
		//mutex_destroy(&g_pcie_gen0.m_dev_mut);
		DestroyCritRegion(&g_pcie_gen0.m_criticalReg);
		SET_FLAG_VALUE(s_ulnFlag0, _MUTEX_CREATED, 0);
	}

	if (GET_FLAG_VALUE(s_ulnFlag0, _CDEV_ADDED))
	{
		cdev_del(&g_pcie_gen0.m_cdev);
		SET_FLAG_VALUE(s_ulnFlag0, _CDEV_ADDED, 0);
	}

	if (GET_FLAG_VALUE(s_ulnFlag0, _CHAR_REG_ALLOCATED))
	{
		unregister_chrdev_region(devno, PCIE_GEN_NR_DEVS + 1);
		SET_FLAG_VALUE(s_ulnFlag0, _CHAR_REG_ALLOCATED, 0);
	}
	
	if (s_pcie_gen_class)
	{
		//printk(KERN_ALERT "!!!!!!!!!!!!!destroying class!!\n");
		class_destroy(s_pcie_gen_class);
		s_pcie_gen_class = NULL;
	}

	if (GET_FLAG_VALUE(s_ulnFlag0, _PCI_DRV_REG))
	{
		pci_unregister_driver(&s_mtcapciegen_driver);
		SET_FLAG_VALUE(s_ulnFlag0, _PCI_DRV_REG, 0);
	}
}



void __exit pcie_gen_cleanup_module(void)
{
	INFO("pcie_gen_cleanup_module CALLED\n");
	ClearModule();        
}

//#include <asm/init.h>
static int __init pcie_gen_init_module(void)
{
	struct proc_dir_entry*	pcie_gen_procdir;
	struct device*			device0;
	int   result    = 0;
	dev_t dev       = 0;
	int   devno     = 0;
	int i;
	int j;

	const struct attribute_group **ppDrv_groups = pci_bus_type.drv_groups;
	const struct attribute_group *pDrv_groups = *ppDrv_groups;
	struct attribute ** ppAttrs = pDrv_groups->attrs;
	struct driver_attribute* pDrvAttr = container_of(ppAttrs[1], struct driver_attribute, attr);

#if defined(_ASM_X86_INIT_H)
#endif // Correct header is "arch/x86/include/asm/init.h"

	printk(KERN_ALERT "ppDrv_groups = %p\n", ppDrv_groups);
	printk(KERN_ALERT "pDrv_groups = %p\n", pDrv_groups);
	printk(KERN_ALERT "ppAttrs = %p\n", ppAttrs);
	printk(KERN_ALERT "ppAttrs[1] = %p\n", ppAttrs[1]);
	printk(KERN_ALERT "pDrvAttr = %p\n", pDrvAttr);
	printk(KERN_ALERT "pDrvAttr->store = %p\n", pDrvAttr->store);
	result = pDrvAttr->store(NULL," ",1);
	printk(KERN_ALERT "result = %d(%s)\n", result,result == -EINVAL ? "-EINVAL" : "Unknown");


	////////////////////////////////////////////
	result = pci_register_driver(&s_mtcapciegen_driver);
	printk(KERN_ALERT "result = %d\n", result);
	if (result)
	{
		ERR("pci driver couldn't be registered!\n");
		goto errorOut;
	}
	SET_FLAG_VALUE(s_ulnFlag0, _PCI_DRV_REG, 1);

	for (i = 0; i < PCIE_GEN_NR_DEVS; ++i) g_vPci_gens[i].m_PcieGen.m_dev_sts = 0;

	PRINT_KERN_ALERT("2. Init module called.  LINUX_VERSION_CODE = %d\n",LINUX_VERSION_CODE);

	result = alloc_chrdev_region(&dev, PCIE_GEN_MINOR, PCIE_GEN_NR_DEVS+1, DEVNAME);
	if (result)
	{
		ERR("Error allocating Major Number for mtcapcie driver.\n");
		goto errorOut;
	}
	SET_FLAG_VALUE(s_ulnFlag0, _CHAR_REG_ALLOCATED, 1);

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

	devno = MKDEV(PCIE_GEN_MAJOR, PCIE_GEN_MINOR);

	cdev_init(&g_pcie_gen0.m_cdev, &pcie_gen_fops);
	g_pcie_gen0.m_cdev.owner = THIS_MODULE;
	g_pcie_gen0.m_cdev.ops = &pcie_gen_fops;
	
	result = cdev_add(&g_pcie_gen0.m_cdev, devno, 1);
	if (result)
	{
		ERR("Error %d adding devno%d num%d", result, devno, 0);
		goto errorOut;
	}
	SET_FLAG_VALUE(s_ulnFlag0, _CDEV_ADDED, 1);

	//mutex_init(&g_pcie_gen0.m_dev_mut);
	InitCritRegion(&g_pcie_gen0.m_criticalReg, DEFAULT_REG_TIMEOUT);
	SET_FLAG_VALUE(s_ulnFlag0, _MUTEX_CREATED, 1);

	g_pcie_gen0.m_dev_num   = 0;
	g_pcie_gen0.m_dev_minor = PCIE_GEN_MINOR;
	g_pcie_gen0.m_dev_sts   = 0;
	g_pcie_gen0.m_slot_num  = 0;

	for( j = 0; j < NUMBER_OF_BARS; ++j ){g_pcie_gen0.m_Memories[j].m_memory_base = 0;}

	device0 = device_create(s_pcie_gen_class, NULL, MKDEV(PCIE_GEN_MAJOR, g_pcie_gen0.m_dev_minor), NULL, "pcie_gen_init");
	if (IS_ERR(device0))
	{
		ERR("Device creation error!\n");
		goto errorOut;
	}
	SET_FLAG_VALUE(s_ulnFlag0, _DEVC_ENTRY_CREATED, 1);

	pcie_gen_procdir = vers_create_proc_entry("pcie_gen_fd",S_IFREG|S_IRUGO,&g_pcie_gen0,pcie_gen_procinfo);
	printk(KERN_ALERT "+++++++++++++pcie_gen_procdir = %p\n", pcie_gen_procdir);
	if (pcie_gen_procdir)
	{
		SET_FLAG_VALUE(s_ulnFlag0, _PROC_ENTRY_CREATED, 1);
	}
	else
	{
		WARNING("proc entry creation error!\n");
	}

	g_pcie_gen0.m_dev_sts = 1;

	for( i = 0; i < PCIE_GEN_NR_DEVS; ++i )
	{
		//g_vPci_gens[i].m_PcieGen.m_dev_sts = 0;
		g_vPci_gens[i].m_PcieGen.m_dev_minor = (PCIE_GEN_MINOR + i+1);

		devno = MKDEV(PCIE_GEN_MAJOR, PCIE_GEN_MINOR + i+1); 
		cdev_init(&g_vPci_gens[i].m_PcieGen.m_cdev, &pcie_gen_fops_tot);
		g_vPci_gens[i].m_PcieGen.m_cdev.owner = THIS_MODULE;
		g_vPci_gens[i].m_PcieGen.m_cdev.ops = &pcie_gen_fops_tot;
		result = cdev_add(&g_vPci_gens[i].m_PcieGen.m_cdev, devno, 1);
		if (result)
		{
			ERR("Error %d adding devno%d num%d", result, devno, i);
			goto errorOut;
		}
		g_vPci_gens[i].m_PcieGen.m_dev_num   = i+1;
		g_vPci_gens[i].m_PcieGen.m_dev_minor = (PCIE_GEN_MINOR + i+1);
		g_vPci_gens[i].m_PcieGen.m_dev_sts   = 0;
		g_vPci_gens[i].m_PcieGen.m_slot_num  = 0;
		
		for( j = 0; j < NUMBER_OF_BARS; ++j ){g_vPci_gens[i].m_PcieGen.m_Memories[j].m_memory_base = 0;}
	}

	g_pcie_gen_IRQworkqueue = create_workqueue(DRV_NAME);

	return 0;

errorOut:
	ClearModule();

	return result > 0 ? -result : (result ? result : -1);
	
}

module_init(pcie_gen_init_module);
module_exit(pcie_gen_cleanup_module);
