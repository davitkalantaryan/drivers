/*
 *	File: mtcagen_main.c
 *
 *	Created on: May 12, 2013
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *	This file contain definitions of ...
 *
 */

#define	PCIE_GEN_NR_DEVS		32

//#define	SHOULD_BE_UNDERSTOOD

#include <linux/module.h>
#include "version_dependence.h"
#include "mtcagen_exp.h"
#include "debug_functions.h"
#include <linux/types.h>
#include "mtcagen_io.h"
#include "pci-driver-added-zeuthen.h"
#include "mtcagen_incl.h"

#ifndef WIN32
MODULE_AUTHOR("Davit Kalantaryan");
MODULE_DESCRIPTION("mtcagen_drv General purpose driver for MTCA crates");
MODULE_VERSION("1.1.0");
MODULE_LICENSE("Dual BSD/GPL");
#endif

//#define DEBUG2 ALERTCT

extern struct workqueue_struct* g_pcie_gen_IRQworkqueue;
struct management_dev_struct
{
	struct cdev					m_cdev;     /* Char device structure      */
	struct SCriticalRegionLock	m_criticalReg;
	int							m_dev_minor;
	unsigned long int			m_ulnFlag;
};

static int s_nPrintDebugInfo = 0;
#ifndef WIN32
module_param_named(debug, s_nPrintDebugInfo, int, S_IRUGO | S_IWUSR);
#endif

static const u16					s_PCIE_GEN_DRV_VER_MAJ = 1;
static const u16					s_PCIE_GEN_DRV_VER_MIN = 1;
static struct management_dev_struct	s_mtca_menegment;
static struct mtcapci_dev			s_vPci_gens[PCIE_GEN_NR_DEVS];
//static struct pciedev_dev			s_vPci_gens[PCIE_GEN_NR_DEVS];
struct class*						g_pcie_gen_class = NULL;
int									g_PCIE_GEN_MAJOR = 46;     /*  major by default */

extern int ReleaseAccessToPciDevice_private(struct pci_driver* a_pciDriver, SDeviceParams* a_DeviceToRemove, int a_nIsKernel);
extern int RemoveIDFromDriverByParams_private(struct pci_driver* a_pciDriver, struct SDeviceParams* a_DeviceToRemove, int a_nIsKernel);
extern void FreeAllIDsAndDevices_private(struct pci_driver* a_pciDriver, int a_nIsKernel);


static int pcie_gen_procinfo(char *buf, char **start, off_t fpos, int lenght, int *eof, void *data)
{
	char *p;
	struct management_dev_struct *drv = data;
	p = buf;
	p += sprintf(p, "Driver Version:\t%i.%i\n", s_PCIE_GEN_DRV_VER_MAJ, s_PCIE_GEN_DRV_VER_MIN);
	p += sprintf(p, "Management minor is:\t%i\n", drv->m_dev_minor);
	*eof = 1;
	return p - buf;
}


static int mtcagen_management_open(struct inode *inode, struct file *filp)
{
	int    minor;
	struct management_dev_struct *drv; /* device information */

	minor = iminor(inode);
	drv = container_of(inode->i_cdev, struct management_dev_struct, m_cdev);
	drv->m_dev_minor = minor;
	filp->private_data = drv; /* for other methods */

	ALERTCT("Opening menegment device! Procces is \"%s\" (pid %i) DEV is %d \n",
		current->comm, current->group_leader->pid, minor);

	return 0;
}



static int mtcagen_managent_release(struct inode *inode, struct file *filp)
{
	struct management_dev_struct	*drv = filp->private_data;
	int minor = drv->m_dev_minor;
	u16 cur_proc = current->group_leader->pid;
	//if (EnterCritRegion(&dev->m_DrvStuff.m_criticalReg))return -ERESTARTSYS;
	ALERTCT("Closing management device. Procces is \"%s\" (pid %d), current->pid = %i, ",
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

#define PRINT_BITS_FR_INT3_ALL(__a_str__,__a_u32__) \
	printk(KERN_ALERT "%s",(__a_str__)); \
	PRINT_BITS_FR_INT_PRVT(#__a_u32__,__a_u32__,19,31); \
	printk(KERN_CONT ";");\
	PRINT_BITS_FR_INT_PRVT(#__a_u32__,__a_u32__,0,18); \
	printk(KERN_CONT "\n");

#define _METHOD_	2

static inline int GetSlotNumber(struct pci_dev *a_pPciDev) // This function should be improved
{
	int pcie_cap;
	u32 tmp_slot_cap = 0;

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
				//PRINT_BITS_FR_INT(tmp_slot_cap, 0, 19);
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
			//PRINT_BITS_FR_INT(tmp_slot_cap, 0, 19);
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
{ \
	int i=0; \
	*(tmp_slot_numM_ptr) = 	(tmp_slot_numM); \
	while(s_vPci_gens[*(tmp_slot_numM_ptr)].upciedev.dev_sts && i<PCIE_GEN_NR_DEVS) \
	{ \
		WARNCT("Slot %d already in use!\n",(int)(*(tmp_slot_numM_ptr))); \
		*(tmp_slot_numM_ptr) = ((*(tmp_slot_numM_ptr))+1)%PCIE_GEN_NR_DEVS; \
		if((*(tmp_slot_numM_ptr)) == (tmp_slot_numM)) {*(tmp_slot_numM_ptr) = -1;break;} \
		++i; \
	} \
	if(i>=PCIE_GEN_NR_DEVS){*(tmp_slot_numM_ptr) = -1;} \
}


#include <linux/list.h>
#include <linux/mutex.h>
#define _COMPARE_2_IDS3_(_uinfo_,_vendor_,_device_,_subvendor_,_subdevice_,_class_,_class_mask_) \
	( \
	((_uinfo_)->vendor           < 0 || (_uinfo_)->vendor    == (_vendor_))    && \
	((_uinfo_)->device           < 0 || (_uinfo_)->device    == (_device_))    && \
	((_uinfo_)->subsystem_vendor < 0 || (_uinfo_)->subsystem_vendor == (_subvendor_)) && \
	((_uinfo_)->subsystem_device < 0 || (_uinfo_)->subsystem_device == (_subdevice_)) && \
	!(((_class_) ^ (_uinfo_)->m_class) & (_uinfo_)->class_mask) && \
	!(((_uinfo_)->m_class ^ (_class_)) & (_class_mask_)) \
	) ? 1 : 0

#define _COMPARE_2_IDS6_(_uinfo_,_pcidev_,_class_,_class_mask_)	\
	_COMPARE_2_IDS3_(_uinfo_,(_pcidev_)->vendor,(_pcidev_)->device,(_pcidev_)->subsystem_vendor,(_pcidev_)->subsystem_device,(_class_),(_class_mask_))

//#define _PID_FOR_INFO_	(pid_t)current->current->group_leader->pid
#define _PID_FOR_INFO_	(pid_t)current->pid

static struct list_head	s_list_for_user_progs;
struct SUserProgs
{
	struct list_head		node_list2;
	struct SDeviceParams	device_params2;
	pid_t					user_pid2;
	pointer_type			user_callback;
};

static struct mutex s_mutex_for_list;

static void PrepareSigInfo(struct siginfo* a_pInfo, struct pciedev_dev* a_dev)
{
	const size_t cunSigInfoBuf = SI_PAD_SIZE * sizeof(int);
	size_t unBufferOffset = DEVICE_NAME_OFFSET*sizeof(int);
	char*	pcBufferForName = (char*)a_pInfo->_sifields._pad + unBufferOffset;
	const char* cpcName = a_dev->cdev.kobj.name ? a_dev->cdev.kobj.name : "Unknown";

	//a_pInfo->_sifields._pad[0] = a_gain_or_remove;
	a_pInfo->_sifields._pad[5] = a_dev->slot_num;
	a_pInfo->_sifields._pad[6] = a_dev->pciedev_pci_dev->vendor;
	a_pInfo->_sifields._pad[7] = a_dev->pciedev_pci_dev->device;
	a_pInfo->_sifields._pad[8] = a_dev->pciedev_pci_dev->subsystem_vendor;
	a_pInfo->_sifields._pad[9] = a_dev->pciedev_pci_dev->subsystem_device;
	strncpy(pcBufferForName, cpcName, (cunSigInfoBuf - unBufferOffset - 1));
}


#define	SET_CALLBACK(_a_sig_info_ptr_,_a_callback_)\
{ \
	pointer_type* _pCallBack_ = (pointer_type*)(_a_sig_info_ptr_)->_sifields._pad;\
	*_pCallBack_ = (_a_callback_); \
}


#define	SET_ACCESS_TYPE(_a_sig_info_ptr_,_a_access_) {*((_a_sig_info_ptr_)->_sifields._pad + 4) = (_a_access_);}



static int AddUserProgram(struct SDeviceParams* a_params,pointer_type a_user_callback)
{
	int i;
	struct siginfo aInfo;
	pid_t lnPID = _PID_FOR_INFO_;

	struct SUserProgs* pNewEntry = (struct SUserProgs*)kzalloc(sizeof(struct SUserProgs), GFP_KERNEL);
	if (!pNewEntry) return -ENOMEM;

	DEBUGNEW("!!!!!!! a_params=%p, user_callback=%p\n", a_params, (void*)a_user_callback);

	pNewEntry->user_pid2 = lnPID;
	memcpy(&pNewEntry->device_params2, a_params, sizeof(struct SDeviceParams));
	pNewEntry->user_callback = a_user_callback;
	
	SET_ACCESS_TYPE(&aInfo, GAINING_ACCESS);

	if (mutex_lock_interruptible(&s_mutex_for_list)){ return -ERESTARTSYS; }

	//s_vPci_gens[PCIE_GEN_NR_DEVS]
	for (i = 0; i < PCIE_GEN_NR_DEVS; ++i)
	{
		//DEBUGNEW("!!!!!!!dev_sts=%d, pci_dev=%p\n", (int)s_vPci_gens[i].dev_sts,s_vPci_gens[i].pciedev_pci_dev);
		if (s_vPci_gens[i].upciedev.dev_sts>0 && 
			_COMPARE_2_IDS6_(a_params, s_vPci_gens[i].upciedev.pciedev_pci_dev, s_vPci_gens[i].upciedev.pciedev_pci_dev->class, 0))
		{
			DEBUGNEW("!!!!!!!!!!!!!!!!! Found!\n");
			PrepareSigInfo(&aInfo, &s_vPci_gens[i].upciedev);
			SET_CALLBACK(&aInfo, a_user_callback);
			KillPidWithInfo(lnPID, SIGNAL_TO_INFORM, &aInfo);
		}
	}
	DEBUGNEW("for finished! pNewEntry=%p\n", pNewEntry);

	list_add_tail(&pNewEntry->node_list2, &s_list_for_user_progs);

	DEBUGNEW("list add done!\n");

	mutex_unlock(&s_mutex_for_list);

	DEBUGNEW("mutex unlock done!\n");

	return 0;
}


static void RemoveUserProgram_private(struct SDeviceParams* a_params,pointer_type a_user_callback,int a_check_info, int a_check_pid, int a_check_callback)
{
	pid_t lnPID = _PID_FOR_INFO_;

	struct SUserProgs *pEntryToDelete, *pEntryNextTemp;

	while (mutex_lock_interruptible(&s_mutex_for_list));

	list_for_each_entry_safe(pEntryToDelete, pEntryNextTemp, &s_list_for_user_progs, node_list2)
	{
		if ((!a_check_info || _COMPARE_2_IDS6_(&pEntryToDelete->device_params2, a_params, a_params->m_class,a_params->class_mask)) &&
			(!a_check_pid || lnPID == pEntryToDelete->user_pid2) && 
			(!a_check_callback || a_user_callback == pEntryToDelete->user_callback))
		{
			list_del(&pEntryToDelete->node_list2); 
			kfree(pEntryToDelete);
		}
	}

	mutex_unlock(&s_mutex_for_list);
}


static void RemoveUserProgramForOneID(struct SDeviceParams* a_params, pointer_type a_user_callback)
{
	RemoveUserProgram_private(a_params, a_user_callback,1, 1, 1);
}


static void RemoveUserProgramAllIDs(void)
{
	RemoveUserProgram_private(NULL,0,0, 1, 0);
}



static int InformAllUserPrograms2(struct pciedev_dev* a_dev, int a_gain_or_remove)
{
	struct siginfo aInfo;
	struct SUserProgs* pEntry;

	//aInfo._sifields._pad[0] = a_gain_or_remove;
	SET_ACCESS_TYPE(&aInfo, a_gain_or_remove);
	PrepareSigInfo(&aInfo, a_dev);
	
	if (mutex_lock_interruptible(&s_mutex_for_list)){ return -ERESTARTSYS; }

	list_for_each_entry(pEntry, &s_list_for_user_progs, node_list2)
	{
		//#define _COMPARE_2_IDS3_(_uinfo_,_vendor_,_device_,_subvendor_,_subdevice_,_class_,_class_mask_)
		if (_COMPARE_2_IDS6_(&pEntry->device_params2, a_dev->pciedev_pci_dev, a_dev->pciedev_pci_dev->class,0) )
		{
			SET_CALLBACK(&aInfo, pEntry->user_callback);
			KillPidWithInfo(pEntry->user_pid2, SIGNAL_TO_INFORM, &aInfo);
		}
	}

	mutex_unlock(&s_mutex_for_list);
	return 0;
}


//int  General_GainAccess(struct pciedev_dev* a_pDev, int a_MAJOR, struct class* a_class, const char* a_DRV_NAME, const struct file_operations *a_pciedev_fops)
//typedef int(*CALLBACKTYPE)(struct pciedev_dev*, int, struct class*, const char*, const struct file_operations*);
//CALLBACKTYPE	callback;

static int __devinit Mtcapciegen_probe(struct pci_dev *a_dev, const struct pci_device_id *a_id)
{
	int tmp_slot_num0, tmp_slot_numF;
	struct list_head* pHead = (struct list_head*)((void*)((char*)a_id->driver_data));
	struct SDriverData* pDrvData;
	PROBETYPE fpProbe;

	//static int nIteration = 0;
	//printk(KERN_ALERT "=============================================== Mtcapciegen_probe %d.  id = %p\n", ++nIteration,a_id);

	ALERTCT("++++++++++++++++dev:{vendor(%d),device(%d),subvendor(%d),subdevice(%d)}\n",
		(int)a_dev->vendor, (int)a_dev->device, (int)a_dev->subsystem_vendor, (int)a_dev->subsystem_device);
	ALERTCT("id: {vendor(%d),device(%d),subvendor(%d),subdevice(%d),driver_data(0x%p),class(%d),class_mask(%d)}\n",
		(int)a_id->vendor, (int)a_id->device,
		(int)a_id->subvendor, (int)a_id->subdevice, (char*)a_id->driver_data, (int)a_id->class, (int)a_id->class_mask);

#ifdef SHOULD_BE_UNDERSTOOD
	if (a_dev->subsystem_vendor == 0 && a_dev->subsystem_device==0)
	{
		WARNCT("Should be understood!\n");
		return -2;
	}
#endif

	tmp_slot_num0 = GetSlotNumber(a_dev); // % PCIE_GEN_NR_DEVS done in GetSlotNumber
	ALERTCT("+++tmp_slot_num = %d\n", tmp_slot_num0);
	if (unlikely(tmp_slot_num0 < 0)) return -4;

	list_for_each_entry(pDrvData, pHead, m_node)
	{
		//DEBUG2("pHead(%p); pDrvData(%p)\n", pHead, pDrvData);
		ALERTCT("pDrvData->m_slot_num=%d, tmp_slot_num0=%d\n", (int)pDrvData->m_slot_num, (int)tmp_slot_num0);

		if (pDrvData->m_slot_num < 0 || pDrvData->m_slot_num == tmp_slot_num0)
		{

			FIND_FREE_ENTRY(tmp_slot_num0, &tmp_slot_numF);
			if (tmp_slot_numF<0){ ERRCT("All entries in use!\n"); return -2; }
			//DEBUG1("!!!!!!!!!!!! tmp_slot_numF = %d\n", tmp_slot_numF);

			s_vPci_gens[tmp_slot_numF].upciedev.pciedev_pci_dev = a_dev;
			s_vPci_gens[tmp_slot_numF].upciedev.irq_type = /*pDrvData->m_irq_type*/0;
			s_vPci_gens[tmp_slot_numF].upciedev.slot_num = tmp_slot_num0;
			s_vPci_gens[tmp_slot_numF].upciedev.dev_minor = PCIE_GEN_MINOR + tmp_slot_numF + 1;
			s_vPci_gens[tmp_slot_numF].upciedev.m_id = a_id;
			s_vPci_gens[tmp_slot_numF].upciedev.brd_num = tmp_slot_numF;
			fpProbe = (PROBETYPE)pDrvData->probe;

			if ((*fpProbe)(&s_vPci_gens[tmp_slot_numF].upciedev, pDrvData->callbackData))
			{
				s_vPci_gens[tmp_slot_numF].upciedev.dev_sts = 0;
				return -3;
			}
			ALERTCT("------------------------ dev:{slot(%d),vendor(%d),device(%d),subvendor(%d),subdevice(%d)}\n",
				tmp_slot_num0, (int)a_dev->vendor, (int)a_dev->device, (int)a_dev->subsystem_vendor, (int)a_dev->subsystem_device);
			//DEBUGNEW("+++++++++++++++++++++ tmp_slot_num0=%d,vendor=%d,device=%d,sub_ven=%d,sub_dev=%d\n", 
			//	(int)tmp_slot_num0);
			s_vPci_gens[tmp_slot_numF].upciedev.remove = pDrvData->remove;
			s_vPci_gens[tmp_slot_numF].upciedev.deviceData = pDrvData->callbackData;
			//s_vPci_gens[tmp_slot_numF].slot_num = tmp_slot_num0;
			//s_vPci_gens[tmp_slot_numF].m_id = a_id;

			dev_set_drvdata(&(a_dev->dev), (&s_vPci_gens[tmp_slot_numF].upciedev));

			//aInfo._sifields._pad[4] = s_vPci_gens[tmp_slot_numF].m_PciGen.m_DrvStuff.m_dev_minor;
			///KillPidWithInfoZn(pDrvData->m_unPID,SIGURG, &aInfo);
			//KillPidWithInfo(pDrvData->m_unPID, SIGURG, &aInfo);
			InformAllUserPrograms2(&s_vPci_gens[tmp_slot_numF].upciedev, GAINING_ACCESS);

			return 0;
		} // if (nSlotNumFromDrvData < 0 || nSlotNumFromDrvData == tmp_slot_num)
	} // list_for_each_entry(pDrvData, pHead, m_node)

	return -1;
}



static void __devexit Mtcapciegen_remove(struct pci_dev *a_dev)
{
	struct pciedev_dev* pPcieStr = dev_get_drvdata(&a_dev->dev);
	if (pPcieStr)
	{
		REMOVETYPE fpRemove;
		InformAllUserPrograms2(pPcieStr, RELEASING_ACCESS);
		fpRemove = (REMOVETYPE)pPcieStr->remove;
		(*fpRemove)(pPcieStr, pPcieStr->deviceData);
		pPcieStr->m_id = NULL;
		pPcieStr->dev_sts = 0;
		pPcieStr->binded = 0;
	}
}


static struct pci_device_id s_mtcapciegen_table[] __devinitdata = { { 0, } };
MODULE_DEVICE_TABLE(pci, s_mtcapciegen_table);


struct pci_driver g_mtcapciegen_driver = {
	.name = DRV_AND_DEV_NAME,
	.id_table = s_mtcapciegen_table,
	.probe = Mtcapciegen_probe,
	.remove = __devexit_p(Mtcapciegen_remove),
};


static long  mtcagen_management_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	int err = 0;
	//SActiveDev	aActiveDev;
	SDeviceParams aDevicePars;
	struct SHotPlugRegister aHotPlug;
	struct management_dev_struct*	drv = filp->private_data;
	//int nMagic = MAGIC_VALUE;
	//struct pci_dev* pDev;
	//int nIndex;

	ALERTCT("cmd = %u,  arg = 0x%.16lX\n", cmd, arg);

	/*
	* extract the type and number bitfields, and don't decode
	* wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	*/
	if (_IOC_TYPE(cmd) != MTCAMANAGEMENT_IOC)   return -ENOTTY;
	if (_IOC_NR(cmd) > MTCAMAN_IOC_MAXNR)		return -ENOTTY;

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
		ERRCT("access_ok Error\n");
		return -EFAULT;
	}

	if (EnterCritRegion(&drv->m_criticalReg))return -ERESTARTSYS;

	switch(cmd)
	{
	case MTCA_GAIN_ACCESS:
		ALERTCT("PCIE_GEN_GAIN_ACCESS\n");
		if (copy_from_user(&aDevicePars, (SDeviceParams*)arg, sizeof(struct SDeviceParams)))
		{
			ALERTCT("Info for device is not provided\n");
			LeaveCritRegion(&drv->m_criticalReg);
			return -EFAULT;
		}
		RegisterDeviceForControl_exp(&g_mtcapciegen_driver, &aDevicePars, NULL, NULL, (void*)((long)MAGIC_VALUE2));
		break;

	case MTCA_RELASE_ACCESS: case MTCA_REMOVE_PCI_ID:
		DEBUGNEW("MTCA_RELASE_ACCESS");
		if (copy_from_user(&aDevicePars, (SDeviceParams*)arg, sizeof(struct SDeviceParams)))
		{
			WARNCT("unable to get buffer from user (\"%s\", %u)!\n", current->comm, (unsigned)current->group_leader->pid);
			LeaveCritRegion(&drv->m_criticalReg);
			return -EFAULT;
		}
		//err = ReleaseAccessToPciDevice_private(NULL, &aDevicePars,0);
		err = RemoveIDFromDriverByParams_private(&g_mtcapciegen_driver, &aDevicePars, 0);
		break;

	case MTCA_REMOVE_ALL:
		FreeAllIDsAndDevices_private(&g_mtcapciegen_driver,0);
		break;

	case MTCA_REGISTER_FOR_HOT_PLUG:
		if (copy_from_user(&aHotPlug, (struct SHotPlugRegister*)arg, sizeof(struct SHotPlugRegister)))
		{
			ALERTCT("Info for device is not provided\n");
			LeaveCritRegion(&drv->m_criticalReg);
			return -EFAULT;
		}
		AddUserProgram(&aHotPlug.device, aHotPlug.user_callback);
		break;

	case MTCA_UNREGISTER_FROM_HOT_PLUG:
		if (copy_from_user(&aHotPlug, (struct SHotPlugRegister*)arg, sizeof(struct SHotPlugRegister)))
		{
			ALERTCT("Info for device is not provided\n");
			LeaveCritRegion(&drv->m_criticalReg);
			return -EFAULT;
		}
		RemoveUserProgramForOneID(&aHotPlug.device, aHotPlug.user_callback);
		break;

	case MTCA_UNREGISTER_FROM_ALL:
		RemoveUserProgramAllIDs();
		break;

	default:
		break;
	}

	LeaveCritRegion(&drv->m_criticalReg);

	return err;
}



static struct file_operations s_management_fops = {
	.owner			= THIS_MODULE,
	//.ioctl		=	mtcagen_management_ioctl,
	.unlocked_ioctl = mtcagen_management_ioctl,
	.compat_ioctl	= mtcagen_management_ioctl,
	.open			= mtcagen_management_open,
	.release		= mtcagen_managent_release,
};



static void ClearModule(void)
{
	dev_t devno = MKDEV(g_PCIE_GEN_MAJOR, PCIE_GEN_MINOR);

	if (GET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _PCI_DRV_REG))
	{
		FreeAllIDsAndDevices_private(&g_mtcapciegen_driver,1);
		pci_unregister_driver(&g_mtcapciegen_driver);
		SET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _PCI_DRV_REG, 0);
	}

	if (GET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _PROC_ENTRY_CREATED))
	{
		remove_proc_entry("pcie_gen_fd", 0);
		SET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _PROC_ENTRY_CREATED, 0);
	}

	DEBUGCT("DEVICE_ENTRY_CREATED = %d\n", GET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _DEVC_ENTRY_CREATED) ? 1 : 0);
	if (GET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _DEVC_ENTRY_CREATED))
	{
		device_destroy(g_pcie_gen_class, MKDEV(g_PCIE_GEN_MAJOR, s_mtca_menegment.m_dev_minor));
		SET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _DEVC_ENTRY_CREATED, 0);
	}

	if (GET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _MUTEX_CREATED))
	{
		//mutex_destroy(&g_pcie_gen0.m_dev_mut);
		DestroyCritRegionLock(&s_mtca_menegment.m_criticalReg);
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

	if (g_pcie_gen_class)
	{
		class_destroy(g_pcie_gen_class);
		g_pcie_gen_class = NULL;
	}

	if (g_pcie_gen_IRQworkqueue)
	{
		flush_workqueue(g_pcie_gen_IRQworkqueue);
		destroy_workqueue(g_pcie_gen_IRQworkqueue);
		g_pcie_gen_IRQworkqueue = NULL;
	}
}

void DestroyGlobalContainerInternal(void);
int pcie_gen_init_write_and_inform(void);
void pcie_gen_cleanup_write_and_inform(void);

static void __exit pcie_gen_cleanup_module(void)
{
	INFOCT("cleanup called for MTCA general module!\n");
	pcie_gen_cleanup_write_and_inform();
	ClearModule();
	DestroyGlobalContainerInternal();
	RemoveUserProgram_private(NULL,0,0, 0, 0);
}


struct SListTest{ struct list_head m_head; };
int InitGlobalContainerInternal(void);


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

	g_nPrintDebugInfo = s_nPrintDebugInfo;

	InitGlobalContainerInternal();
	mutex_init(&s_mutex_for_list);
	INIT_LIST_HEAD(&s_list_for_user_progs);

	s_mtca_menegment.m_ulnFlag = 0;

	for (i = 0; i < PCIE_GEN_NR_DEVS; ++i) s_vPci_gens[i].upciedev.dev_sts = 0;

	//ALERTRT("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	UpciedevTestFunction("MTCAGEN");

	ALERTCT("Init module called.  LINUX_VERSION_CODE = %d\n", LINUX_VERSION_CODE);

	result = alloc_chrdev_region(&dev, PCIE_GEN_MINOR, PCIE_GEN_NR_DEVS + 1, DRV_AND_DEV_NAME);
	if (result)
	{
		ERRCT("Error allocating Major Number for mtcapcie driver.\n");
		goto errorOut;
	}
	SET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _CHAR_REG_ALLOCATED, 1);

	g_PCIE_GEN_MAJOR = MAJOR(dev);
	ALERTCT("AFTER_INIT DRV_MAJOR is %d\n", g_PCIE_GEN_MAJOR);

	g_pcie_gen_class = class_create(THIS_MODULE, DRV_AND_DEV_NAME);
	if (IS_ERR(g_pcie_gen_class))
	{
		result = PTR_ERR(g_pcie_gen_class);
		ERRCT("Error creating mtcapcie class err = %d. class = %p\n", result, g_pcie_gen_class);
		g_pcie_gen_class = NULL;
		goto errorOut;
	}
	//DEBUG1("\n");

	devno = MKDEV(g_PCIE_GEN_MAJOR, PCIE_GEN_MINOR);

	cdev_init(&s_mtca_menegment.m_cdev, &s_management_fops);
	//DEBUG1("\n");
	s_mtca_menegment.m_cdev.owner = THIS_MODULE;
	s_mtca_menegment.m_cdev.ops = &s_management_fops;

	result = cdev_add(&s_mtca_menegment.m_cdev, devno, 1);
	if (result)
	{
		ERRCT("Error %d adding devno%d num%d", result, devno, 0);
		goto errorOut;
	}
	//DEBUG1("\n");
	SET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _CDEV_ADDED, 1);

	//mutex_init(&g_pcie_gen0.m_dev_mut);
	InitCritRegionLock(&s_mtca_menegment.m_criticalReg, DEFAULT_LOCK_TIMEOUT);
	//DEBUG1("\n");
	SET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _MUTEX_CREATED, 1);

	s_mtca_menegment.m_dev_minor = PCIE_GEN_MINOR;

	device0 = device_create(g_pcie_gen_class, NULL, MKDEV(g_PCIE_GEN_MAJOR, s_mtca_menegment.m_dev_minor), NULL, "pciegen_menegment");
	//DEBUG1("\n");
	if (IS_ERR(device0))
	{
		ERRCT("Device creation error!\n");
		goto errorOut;
	}
	SET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _DEVC_ENTRY_CREATED, 1);

	pcie_gen_procdir = vers_create_proc_entry("pcie_gen_fd", S_IFREG | S_IRUGO, &s_mtca_menegment, pcie_gen_procinfo);
	//DEBUG1("\n");
	if (pcie_gen_procdir)
	{
		SET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _PROC_ENTRY_CREATED, 1);
	}
	else
	{
		WARNCT("proc entry creation error!\n");
	}


	for (i = 0; i < PCIE_GEN_NR_DEVS; ++i)
	{
		//g_vPci_gens[i].m_PcieGen.m_dev_sts = 0;
		s_vPci_gens[i].upciedev.slot_num = 0;
		//s_vPci_gens[i].m_PciGen.m_nCreateInsideBigStruct = 1;
		s_vPci_gens[i].upciedev.m_id = NULL;
		for (j = 0; j < NUMBER_OF_BARS; ++j){ s_vPci_gens[i].upciedev.memmory_base[j] = 0; }
	}

	g_pcie_gen_IRQworkqueue = create_workqueue(DRV_AND_DEV_NAME);
	//DEBUG1("\n");

	result = pci_register_driver(&g_mtcapciegen_driver);
	if (result)
	{
		ERRCT("pci driver couldn't be registered!\n");
		goto errorOut;
	}
	SET_FLAG_VALUE(s_mtca_menegment.m_ulnFlag, _PCI_DRV_REG, 1);
	pcie_gen_init_write_and_inform();

	return 0;

errorOut:
	ClearModule();
	return result > 0 ? -result : (result ? result : -1);
}

module_init(pcie_gen_init_module);
module_exit(pcie_gen_cleanup_module);

