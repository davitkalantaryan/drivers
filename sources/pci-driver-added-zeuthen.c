/*
 *	File: pcie-driver-add-zeuthen.c
 *
 *	Created on: Apr 24, 2015
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *	This file contain definitions of ...
 *
 */


//#define DEBUG2 ALERTCT

#define _COMPARE_2_IDS2_(_id1_,_id2_) \
	( \
	((_id1_)->vendor    == PCI_ANY_ID || (_id2_)->vendor    == PCI_ANY_ID || (_id1_)->vendor    == (_id2_)->vendor)    && \
	((_id1_)->device    == PCI_ANY_ID || (_id2_)->device    == PCI_ANY_ID || (_id1_)->device    == (_id2_)->device)    && \
	((_id1_)->subvendor == PCI_ANY_ID || (_id2_)->subvendor == PCI_ANY_ID || (_id1_)->subvendor == (_id2_)->subvendor) && \
	((_id1_)->subdevice == PCI_ANY_ID || (_id2_)->subdevice == PCI_ANY_ID || (_id1_)->subdevice == (_id2_)->subdevice) && \
	!(((_id2_)->class ^ (_id1_)->class) & (_id1_)->class_mask) && \
	!(((_id1_)->class ^ (_id2_)->class) & (_id2_)->class_mask) \
	) ? 1 : 0


#include <acpi/platform/acenv.h>
#include <linux/version.h>
#include "debug_functions.h"
#include "pci-driver-added-zeuthen.h"

struct pci_dynid {
	struct list_head node;
	struct pci_device_id id;
};

static inline int LIST_SIZE(struct list_head* a_pListHead)
{
	struct list_head* p;
	int i = 0;
	list_for_each(p, a_pListHead){++i;}
	return i;
}


int pci_add_dynid_zeuthen2(struct pci_driver *a_pdrv,
	unsigned int a_vendor, unsigned int a_device,
	unsigned int a_subvendor, unsigned int a_subdevice,
	unsigned int a_class, unsigned int a_class_mask,
	unsigned long a_slot, void* a_pProbe,void* a_pRelease, void* a_pCallbackData)
{
	struct pci_dynid *dynid;
	struct pci_device_id *idOld = NULL;
	int retval;

	int nSlotNew;

	int nDrvDataDelete = 0;
	int bFound = 0;

	struct list_head* pHeadNew;
	struct SDriverData* dvrDataNew;

	struct pci_dynid *dynidNew = kzalloc(sizeof(*dynidNew), GFP_KERNEL);
	
	if (!dynidNew){ ERRCT("No memory"); return -ENOMEM; }

	dvrDataNew = kzalloc(sizeof(*dvrDataNew), GFP_KERNEL);
	if (!dvrDataNew)
	{
		kfree(dynidNew);
		ERRCT("No memory");
		return -ENOMEM;
	}

	pHeadNew = kzalloc(sizeof(*pHeadNew), GFP_KERNEL);
	if (!pHeadNew)
	{
		kfree(dvrDataNew);
		kfree(dynidNew);
		ERRCT("No memory");
		return -ENOMEM;
	}

	dynidNew->id.vendor = a_vendor;
	dynidNew->id.device = a_device;
	dynidNew->id.subvendor = a_subvendor;
	dynidNew->id.subdevice = a_subdevice;
	dynidNew->id.class = a_class;
	dynidNew->id.class_mask = a_class_mask;
	//dynidNew->id.driver_data = a_driver_data;

	nSlotNew = a_slot;
	dvrDataNew->m_slot_num = nSlotNew;
	dvrDataNew->probe = a_pProbe;
	dvrDataNew->remove = a_pRelease;
	dvrDataNew->callbackData = a_pCallbackData;

	spin_lock(&a_pdrv->dynids.lock);

	list_for_each_entry(dynid, &a_pdrv->dynids.list, node)
	{
		idOld = &dynid->id;
		if (_COMPARE_2_IDS2_(&dynidNew->id, idOld))
		{
			struct list_head* pHead = (struct list_head*)((void*)((char*)idOld->driver_data));
			struct SDriverData *pDrvDataOld;

			if (a_vendor == PCI_ANY_ID){ idOld->vendor = PCI_ANY_ID; }
			if (a_device == PCI_ANY_ID){ idOld->device = PCI_ANY_ID; }
			if (a_subvendor == PCI_ANY_ID){ idOld->subvendor = PCI_ANY_ID; }
			if (a_subvendor == PCI_ANY_ID){ idOld->subvendor = PCI_ANY_ID; }

			list_for_each_entry(pDrvDataOld, pHead, m_node)
			{
				if (nSlotNew < 0 || pDrvDataOld->m_slot_num < 0 || nSlotNew == pDrvDataOld->m_slot_num)
				{
					if (nSlotNew < 0){ pDrvDataOld->m_slot_num = -1; }
					bFound = 1;
					nDrvDataDelete = 1;
					break;
				}
			} // 2.  list_for_each_entry(pDrvDataOld, pHead, m_node)


			if (!bFound){ list_add_tail(&dvrDataNew->m_node, pHead); }

			bFound = 1;
			break;
		} // if (_COMPARE_3_IDS_(&dynidNew->id, idOld))
	} // list_for_each_entry(dynid, &a_pdrv->dynids.list, node)

	//list_add_tail(&dynid->node, &a_pdrv->dynids.list);
	//list_add(&dynid->node, &a_pdrv->dynids.list);
	if (!bFound)
	{
		dynidNew->id.driver_data = (kernel_ulong_t)((char*)pHeadNew);
		//DEBUG2("pHead(%p), pHead->prev=%p, pHead->next=%p\n", pHeadNew, pHeadNew->prev, pHeadNew->next);
		INIT_LIST_HEAD(pHeadNew);
		//DEBUG2("pHead(%p), size = %d, pHead->prev=%p, pHead->next=%p\n", pHeadNew, LIST_SIZE(pHeadNew), pHeadNew->prev, pHeadNew->next);
		list_add_tail(&dvrDataNew->m_node, pHeadNew);
		//DEBUG2("pHead(%p), size = %d, pHead->prev=%p, pHead->next=%p\n", pHeadNew, LIST_SIZE(pHeadNew), pHeadNew->prev, pHeadNew->next);
		list_add_tail(&dynidNew->node, &a_pdrv->dynids.list);
	}
	spin_unlock(&a_pdrv->dynids.lock);

	//DEBUG2("bFound = %d, nDrvDataDelete = %d\n", bFound, nDrvDataDelete);

	if (bFound) kfree(pHeadNew);
	if (nDrvDataDelete) kfree(dvrDataNew);
	if (bFound) kfree(dynidNew);

	retval = driver_attach(&a_pdrv->driver);

	return retval;
}


// before calling this function lock should be performed
static inline void pci_id_free_drvDatas(const struct pci_device_id *a_id)
{
	int nSubIDs = 0;
	struct SDriverData *pDrvData, *pDrvDataNext;
	struct list_head* pHead = (struct list_head*)((void*)((char*)a_id->driver_data));
	struct pci_device_id *id = (struct pci_device_id*)a_id;
	if (pHead)
	{
		list_for_each_entry_safe(pDrvData, pDrvDataNext, pHead, m_node)
		{
			++nSubIDs;
			list_del(&pDrvData->m_node);
			kfree(pDrvData);
		}
		kfree(pHead);
		id->driver_data = 0;
	}

	//DEBUG1("pHead = %p. Number of sub ids = %d\n", pHead, nSubIDs);
}



int pci_free_dynid_zeuthen(struct pci_driver *a_pdrv, const struct pci_device_id *a_id)
{
	struct pci_dynid *dynid, *n;
	int retval = -ENODEV;
	void* pDeleted = NULL;

	DEBUGNEW("pciedev=%p, a_id=%p", a_pdrv, a_id);

	spin_lock(&a_pdrv->dynids.lock);
	list_for_each_entry_safe(dynid, n, &a_pdrv->dynids.list, node)
	{
		struct pci_device_id *id = &dynid->id;
		if (!a_id || _COMPARE_2_IDS1_(a_id, id))
		{
			pci_id_free_drvDatas(id);
			pDeleted = id;
			list_del(&dynid->node);
			kfree(dynid);
			retval = 0;
			break;
		}
	}
	spin_unlock(&a_pdrv->dynids.lock);

	//DEBUG1("Deleted %p id from MTCA general driver!\n", pDeleted);

	return retval;
}



void FreeAllIDsAndDrvDatas(struct pci_driver *a_pdrv)
{
	struct pci_dynid *dynid, *n;

	spin_lock(&a_pdrv->dynids.lock);
	list_for_each_entry_safe(dynid, n, &a_pdrv->dynids.list, node)
	{
		struct pci_device_id *id = &dynid->id;
		pci_id_free_drvDatas(id);
		list_del(&dynid->node);
		kfree(dynid);
	}
	spin_unlock(&a_pdrv->dynids.lock);
}
