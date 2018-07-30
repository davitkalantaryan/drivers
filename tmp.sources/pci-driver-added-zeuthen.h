/*
 *	File: pcie-driver-add-zeuthen.h
 *
 *	Created on: Apr 24, 2015
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *	This file contain definitions of ...
 *
 */

#ifndef __pci_driver_added_zeuthen_h__
#define __pci_driver_added_zeuthen_h__

#define	_TYPE_BIT_LEN_				2
#define	_SLOT_NUM_LEN_				5
#define	_IRQ_TYPE_LEN2_				3


//#define		_MASK_FOR_DRV_DATA_	0x00000000000000f8
#define _COMPARE_2_IDS1_(pid1,pid2) \
	( \
	((pid1)->vendor    == (pid2)->vendor)    && \
	((pid1)->device    == (pid2)->device)    && \
	((pid1)->subvendor == (pid2)->subvendor) && \
	((pid1)->subdevice == (pid2)->subdevice) && \
	!(((pid2)->class ^ (pid1)->class) & (pid1)->class_mask) && \
	!(((pid1)->class ^ (pid2)->class) & (pid2)->class_mask) \
	) ? 1 : 0
//(((pid1)->driver_data&_MASK_FOR_DRV_DATA_) == ((pid2)->driver_data&_MASK_FOR_DRV_DATA_)) && 
#if 0
#define	SET_SLOT_AND_PID(driverData_ptr,slot_,pid_) \
	do{ \
		slot_ = (slot_<0) ? ((1<<_SLOT_NUM_LEN_)-1) : slot_; \
		slot_ &= ((1 << _SLOT_NUM_LEN_) - 1); \
		*(driverData_ptr) = (kernel_ulong_t)pid_; \
		*(driverData_ptr) <<= _SLOT_NUM_LEN_; \
		*(driverData_ptr) |= slot_; \
		*(driverData_ptr) <<= _TYPE_BIT_LEN_; \
		}while(0)
#endif

#include <linux/pci.h>

struct SDriverData
{
	void*				probe;
	void*				remove;
	void*				callbackData;
	struct list_head	m_node;
	int					m_slot_num : _SLOT_NUM_LEN_;
	//int				m_irq_type : _IRQ_TYPE_LEN_;
	//int					m_nProbeDone : 1;
	//pid_t				m_unPID		/*	: (32 - _SLOT_NUM_LEN_ - _IRQ_TYPE_LEN_ - 1)*/;
};

int pci_add_dynid_zeuthen2(struct pci_driver *pdrv,
	unsigned int vendor, unsigned int device,
	unsigned int subvendor, unsigned int subdevice,
	unsigned int class, unsigned int class_mask,
	unsigned long driver_data, void* probe, void* remove,void* callbackData);

int pci_free_dynid_zeuthen(struct pci_driver *drv, const struct pci_device_id *id);

void FreeAllIDsAndDrvDatas(struct pci_driver *a_pdrv);

#endif  /* #ifndef __pci_driver_added_zeuthen_h__ */
