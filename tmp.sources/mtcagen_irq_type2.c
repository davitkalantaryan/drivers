/*
 *	File: mtcagen_irq_type2.c
 *
 *	Created on: Nov 29, 2015
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *	This file contain definitions of ...
 *
 */

#include <linux/export.h>
#include "pciedev_ufn.h"
#include "mtcagen_io.h"
#include "debug_functions.h"
#include "read_write_inline.h"
#include "mtcagen_exp.h"
#include "version_dependence.h"
#include "mtcagen_irq_all.h"

/*
* The top-half interrupt handler.
*/
#if LINUX_VERSION_CODE < 0x20613 // irq_handler_t has changed in 2.6.19
static irqreturn_t pcie_gen_interrupt(int irq, void *dev_id, struct pt_regs *regs)
#else
static irqreturn_t pcie_gen_interrupt(int irq, void *dev_id)
#endif
{
#if 0
	struct pciedev_dev * dev = dev_id;
	struct SIRQstruct1* pIRQstr = dev->irqData;
	int nSigInfoIndex = (pIRQstr->m_IRQ_common.numberOfIRQtop) % IRQ_INF_RING_SIZE;
	struct siginfo* pSigInfo = &(pIRQstr->m_vSigInfos[nSigInfoIndex]);
	char*const pcBuffer = (char*)((pSigInfo->_sifields._pad));

	//pSigInfo->_sifields._pad[4] = pIRQstr->m_nNumberOfIRQtop4++;

	// int pcie_gen_interrupt_inline(struct pciedev_dev * a_dev, char* a_pcInfoBuffer, struct SIRQstruct_all* a_pIrqStruct)
	if (pcie_gen_interrupt_inline(dev, pcBuffer, &pIRQstr->m_IRQ_common))
	{
		pIRQstr->m_nCurrentIndexOfSigInfo = nSigInfoIndex;
		queue_work(g_pcie_gen_IRQworkqueue, &(pIRQstr->pcie_gen_work));
		return IRQ_HANDLED;
	}
#endif

	return IRQ_NONE;
}


static int RequestInterrupt_type2_static(struct pciedev_dev* a_pDev, const char* a_DRV_NAME,
	struct device_irq_handling* a_reset_info, unsigned long(*a_fpCopy)(void*, const void*, unsigned long))
{
	struct pci_dev*  dev = a_pDev->pciedev_pci_dev;
	const size_t cunSigInfoBuf = SI_PAD_SIZE * sizeof(int);
	int result = 0;
	struct SIRQstruct2* pIRQstr2;

	if (GET_FLAG_VALUE(a_pDev->m_ulnFlag, _IRQ_REQUEST_DONE)){ return -2; }

	a_pDev->irq_type = _IRQ_TYPE2_;

	pIRQstr2 = a_pDev->irqData = kzalloc(sizeof(struct SIRQstruct2), GFP_KERNEL);
	if (!pIRQstr2)
	{
		ERRCT("ERROR: Unabble to create memory!\n");
		a_pDev->irq_type = _NO_IRQ_;
		return -ENOMEM;
	}

	if (Get_Interrupt_Info_global(&(pIRQstr2->irq_common), cunSigInfoBuf, a_pDev->register_size,
		a_DRV_NAME, a_reset_info, a_fpCopy) < 0)
	{
		a_pDev->irq_type = _NO_IRQ_;
		a_pDev->irqData = NULL;
		kfree(pIRQstr2);
		ERRCT("ERROR: Error during copying IRQ handling info!\n");
		return -1;
	}

	ALERTCT("Requesting interupt\n");
	//a_pDev->irq_flag = IRQF_SHARED | IRQF_DISABLED;
	//IRQF_DISABLED - keep irqs disabled when calling the action handler.
	//                 DEPRECATED.This flag is a NOOP and scheduled to be removed
	//a_pDev->irq_flag = IRQF_SHARED | IRQF_DISABLED;
	a_pDev->irq_flag = IRQF_SHARED;
	a_pDev->pci_dev_irq = dev->irq;
	a_pDev->irq_mode = 1;

	result = request_irq(a_pDev->pci_dev_irq, pcie_gen_interrupt,
		a_pDev->irq_flag, a_DRV_NAME, a_pDev);

	if (result)
	{
		/// Stex memory leak ka, piti dzvi
		kfree(pIRQstr2);
		a_pDev->irqData = NULL;
		WARNCT("can't get assigned irq %i\n", a_pDev->irq_line);
		a_pDev->irq_mode = 0;
	}
	else
	{
		ALERTCT("assigned IRQ %i RESULT %i\n", a_pDev->pci_dev_irq, result);
		SET_FLAG_VALUE(a_pDev->m_ulnFlag, _IRQ_REQUEST_DONE, 1);
	}

	SET_FLAG_VALUE(a_pDev->m_ulnFlag, _IRQ_REQUEST_DONE, 1);

	return result;
}


int RequestInterrupt_type2(struct pciedev_dev* a_pDev, const char* a_DRV_NAME, unsigned long a_arg)
{
	return RequestInterrupt_type2_static(a_pDev, a_DRV_NAME, (struct device_irq_handling*)a_arg, copy_from_user);
}


static unsigned long CopyKernelData(void* a_to, const void* a_from, unsigned long a_n)
{
	memcpy(a_to, a_from, a_n);
	return 0;
}

int RequestInterrupt_type2_exp(struct pciedev_dev* a_pDev, const char* a_DRV_NAME, struct device_irq_handling* a_reset_info)
{
	return RequestInterrupt_type2_static(a_pDev, a_DRV_NAME, a_reset_info, CopyKernelData);
}
EXPORT_SYMBOL(RequestInterrupt_type2_exp);



void UnRequestInterrupt_type2_exp(struct pciedev_dev* a_pDev)
{
#if 0
	struct SirqResetAndList *dynid, *n;
	struct SIRQstruct1* pIRQstr = a_pDev->irqData;

	if (GET_FLAG_VALUE(a_pDev->m_ulnFlag, _IRQ_REQUEST_DONE))
	{
		ALERTCT("Freeing Irq Number %d\n", (int)a_pDev->pci_dev_irq);
		free_irq(a_pDev->pci_dev_irq, a_pDev);

		if (g_pcie_gen_IRQworkqueue)flush_workqueue(g_pcie_gen_IRQworkqueue); // flush all pending
		//usleep(1);

		list_for_each_entry_safe(dynid, n, &pIRQstr->m_IRQ_common.listIRQreset, m_node) {
			list_del(&dynid->m_node);
			kfree(dynid);
		}

		SET_FLAG_VALUE(a_pDev->m_ulnFlag, _IRQ_REQUEST_DONE, 0);

		kfree(pIRQstr);
		a_pDev->irqData = NULL;
	}
#endif
}
EXPORT_SYMBOL(UnRequestInterrupt_type2_exp);
