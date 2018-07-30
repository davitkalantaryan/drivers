/*
 *	File: mtcagen_exp.h
 *
 *	Created on: May 12, 2013
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *	This file contain definitions of all exported functions
 *
 */
#ifndef __mtcagen_exp_h__
#define __mtcagen_exp_h__

#define	DRV_AND_DEV_NAME		"pcimtcagen_drv"
#define DEFAULT_LOCK_TIMEOUT	50 //ms

#ifndef SET_FLAG_VALUE
#define	SET_FLAG_VALUE(flag,field,value) \
do{ \
	unsigned long ulMask = ~(1<<field); \
	(flag) &= ulMask & (flag); \
	ulMask = (value)<<(field); \
	(flag) |= ulMask; \
}while(0)
#define	GET_FLAG_VALUE(flag,field) ( (flag) & (1<<(field)) )
#endif

#define DEVICE_PARAM_RESET(_deviceParam_) \
{ \
	(_deviceParam_)->vendor = -1; \
	(_deviceParam_)->device = -1; \
	(_deviceParam_)->subsystem_vendor = -1; \
	(_deviceParam_)->subsystem_device = -1; \
	(_deviceParam_)->m_class = 0; \
	(_deviceParam_)->class_mask = 0; \
	(_deviceParam_)->slot_num = -1; \
}

#define	PCIE_GEN_MINOR		0

#include <linux/types.h>
#include <linux/module.h>
#include "pciedev_ufn.h"
#include "mtcagen_io.h"
#include <linux/wait.h>

typedef struct SIrqWaiterStruct
{
	wait_queue_head_t	waitIRQ;
	long				numberOfIRQs;
	int					isIrqActive;
}SIrqWaiterStruct;

typedef int(*PROBETYPE)(struct pciedev_dev*,void*);
typedef void(*REMOVETYPE)(struct pciedev_dev*,void*);

typedef struct SUserInteruptStr
{
	int32_t				m_nPID;
	int32_t				m_nSigNum;
	uint64_t			m_unCalbackData;
}SUserInteruptStr;

typedef struct SIRQstruct_all
{
	u_int16_t				is_line_shared : 4;
	u_int16_t				is_irq_bar : 12;
	u_int16_t				is_irq_offset;
	u_int16_t				is_irq_register_size;
	////////////////////////////////////////////
	u_int16_t				resetRegsNumber;
	struct list_head		listIRQreset;
}SIRQstruct_all;


typedef struct SIRQstruct1
{
#define	MAX_INTR_NUMB				16
	//spinlock_t				irq_lock;
	struct pciedev_dev*			m_pDev;
	struct work_struct			pcie_gen_work;
	//struct list_head			m_ListIRQreset;
	struct SIRQstruct_all		irq_common;
	struct siginfo				m_vSigInfos[IRQ_INF_RING_SIZE];
	int							m_nNumberOfIRQtop4;
	int							m_nNumberOfIRQtopOld;
	int							m_nCurrentIndexOfSigInfo;
	int							m_nInteruptWaiters : 5;
	SUserInteruptStr			m_IrqWaiters[MAX_INTR_NUMB];
	//int							m_nIRQmaxSize : 3;
}SIRQstruct1;


typedef struct SIRQstruct2
{
	struct SIRQstruct_all		irq_common;
	struct SIrqWaiterStruct		waiterIRQ;
}SIRQstruct2;

typedef struct SRegisterChangeUser
{
	uint64_t			m_unCalbackData;
}SRegisterChangeUser;

typedef struct mtcapci_dev
{
	struct pciedev_dev	upciedev;
}mtcapci_dev;


extern const struct file_operations g_default_mtcagen_fops_exp;
int RegisterDeviceForControl_exp(struct pci_driver* pciDriver, struct SDeviceParams* deviceToHandle,
								PROBETYPE probeFunc, REMOVETYPE removeFunc, void* dataForProbeAndRemove);
int  Mtcagen_GainAccess_exp(struct pciedev_dev* dev, int major, struct class* a_class,
	const struct file_operations *pciedev_fops, const char* driver_name, const char* drv_name_fmt, ...);
void Mtcagen_remove_exp(struct pciedev_dev* dev, int major, struct class* a_class);
int ReleaseAccessToPciDevice2_exp(struct pci_driver* driver, SDeviceParams* deviceParams);
int RemoveIDFromDriver2_exp(struct pci_driver* driver, struct pci_device_id* id);
int RemoveIDFromDriverByParams2_exp(struct pci_driver* driver, struct SDeviceParams* deviceParams);
long mtcagen_ioctl_exp(struct file *filp, unsigned int a_cmd, unsigned long a_arg);
int  mtcagen_release_exp(struct inode *inode, struct file *filp);
ssize_t pciedev_write_and_inform_exp(void* a_a_dev,
	u_int16_t a_register_size, u_int16_t a_rw_access_mode, u_int32_t a_bar, u_int32_t a_offset,
	const char __user *a_pcUserData0, const char __user *a_pcUserMask0, size_t a_count);

int AddNewEntryToGlobalContainer_prvt(struct module* owner,const void* entry, const char* format, ...);
void RemoveEntryFromGlobalContainer(const char* keyName);
int vRemoveEntryFromGlobalContainer(const char* format, ...);
void* FindAndUseEntryFromGlobalContainer(const char* keyName);
void* vFindAndUseEntryFromGlobalContainer(const char* format, ...);
void PutEntryToGlobalContainer(const char* keyName);

#define	AddNewEntryToGlobalContainer(__entry__,__format__,...)	AddNewEntryToGlobalContainer_prvt(THIS_MODULE,(__entry__),(__format__))


#define	registerDeviceToMainDriver(dvcParam,probe,remove,data)	RegisterDeviceForControl_exp(NULL,(dvcParam),(probe),(remove),(data))
#define	releaseAccessToDeviceFromMainDriver(deviceParams)		ReleaseAccessToPciDevice2_exp(NULL,(deviceParams))
#define	removeIDfromMainDriver(id)								RemoveIDFromDriver2_exp(NULL,(id))
#define	removeIDfromMainDriverByParam(deviceParams)				RemoveIDFromDriverByParams2_exp(NULL,(deviceParams))
//#define	freeAllDevicesAndIdsFromMainDriver(...)					FreeAllIDsAndDevices_exp(NULL)


int RequestInterrupt_type1_exp(struct pciedev_dev* dev, const char* dev_name, struct device_irq_handling* reset_info);
int RequestInterrupt_type2_exp(struct pciedev_dev* dev, const char* dev_name, struct device_irq_handling* reset_info);
void UnRequestInterrupt_type1_exp(struct pciedev_dev* dev);
void UnRequestInterrupt_type2_exp(struct pciedev_dev* dev);

#endif  /* #ifndef __mtcagen_exp_h__ */
