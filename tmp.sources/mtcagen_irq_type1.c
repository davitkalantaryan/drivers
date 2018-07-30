/*
 *	File: mtcagen_irq.c
 *
 *	Created on: Jul 13, 2015
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

struct workqueue_struct* g_pcie_gen_IRQworkqueue = NULL;
//EXPORT_SYMBOL(g_pcie_gen_IRQworkqueue);

/*
* This function should be modified to safe one (but without syncronization objects)
*/
void RemoveUserPID_type1(struct pciedev_dev* a_dev, pid_t a_nPID)
{
	int i;
	int nIndex = -1;
	struct SIRQstruct1* pIRQstr = (struct SIRQstruct1*)a_dev->irqData;

	if (!GET_FLAG_VALUE(a_dev->m_ulnFlag, _IRQ_REQUEST_DONE) || a_dev->irq_type != _IRQ_TYPE1_){ return; }

	//if (EnterCritRegion(&a_dev->m_PciGen.m_DrvStuff.m_criticalReg)){ ERR("Unable to lock!\n"); return; }

	for (i = 0; i < pIRQstr->m_nInteruptWaiters; ++i)
	{
		if (a_nPID == pIRQstr->m_IrqWaiters[i].m_nPID)
		{
			nIndex = i;
			break;
		}
	}

	if (nIndex >= 0)
	{
		if (nIndex<pIRQstr->m_nInteruptWaiters - 1)
			memmove(pIRQstr->m_IrqWaiters + nIndex, pIRQstr->m_IrqWaiters + nIndex + 1, (pIRQstr->m_nInteruptWaiters - nIndex - 1)*sizeof(SUserInteruptStr));

		pIRQstr->m_nInteruptWaiters--;
	}

	//LeaveCritRegion(&a_dev->m_PciGen.m_DrvStuff.m_criticalReg);
}



#if LINUX_VERSION_CODE < 132632 
static void pcie_gen_do_work(void *pciegendev)
#else
static void pcie_gen_do_work(struct work_struct *work_str)
#endif 
{
#if LINUX_VERSION_CODE < 132632
	struct pciedev_dev *dev = (struct str_pcie_gen*)pciegendev;
	struct SIRQstruct1* pIRQstr = (struct SIRQstruct1*)dev->irqData;
#else
	struct SIRQstruct1* pIRQstr = container_of(work_str, struct SIRQstruct1, pcie_gen_work);
	struct pciedev_dev *dev = pIRQstr->m_pDev;
#endif
	struct siginfo* pSigInfo = &(pIRQstr->m_vSigInfos[pIRQstr->m_nCurrentIndexOfSigInfo]);
	uint64_t* pCallbackData = (uint64_t*)((void*)pSigInfo->_sifields._pad);
	struct SUserInteruptStr aProcTask;
	int i;

	//if (unlikely(!dev->m_PciGen.m_dev_sts)) return; // should be removed at all

	if (pIRQstr->m_nNumberOfIRQtopOld == pIRQstr->m_nNumberOfIRQtop4)
	{
		static int nNumber = 0;
		//WARNCT("!!! Handler called twice %d times!\n", nNumber);
		//DEBUGNEW("  exiting!!!\n");
		if (nNumber++ % 10000 == 0){ WARNCT("!!! Handler called twice %d times!\n", nNumber); }
		return;
	}


	for (i = 0; i < pIRQstr->m_nInteruptWaiters; ++i)
	{
		aProcTask = pIRQstr->m_IrqWaiters[i];
		*pCallbackData = aProcTask.m_unCalbackData;

		if (KillPidWithInfo(aProcTask.m_nPID, aProcTask.m_nSigNum, pSigInfo))
		{
			RemoveUserPID_type1(dev, aProcTask.m_nPID);
		}
	}

	pIRQstr->m_nNumberOfIRQtopOld = pIRQstr->m_nNumberOfIRQtop4;
}



/*
 * The top-half interrupt handler.
 */
#if LINUX_VERSION_CODE < 0x20613 // irq_handler_t has changed in 2.6.19
static irqreturn_t pcie_gen_interrupt(int irq, void *dev_id, struct pt_regs *regs)
#else
static irqreturn_t pcie_gen_interrupt(int irq, void *dev_id)
#endif
{
	struct pciedev_dev * dev = dev_id;
	struct SIRQstruct1* pIRQstr = dev->irqData;
	int nSigInfoIndex = (pIRQstr->m_nNumberOfIRQtop4) % IRQ_INF_RING_SIZE;
	struct siginfo* pSigInfo = &(pIRQstr->m_vSigInfos[nSigInfoIndex]);
	char*const pcBuffer = (char*)((pSigInfo->_sifields._pad));

	pSigInfo->_sifields._pad[4] = pIRQstr->m_nNumberOfIRQtop4++;

	// int pcie_gen_interrupt_inline(struct pciedev_dev * a_dev, char* a_pcInfoBuffer, struct SIRQstruct_all* a_pIrqStruct)
	if (pcie_gen_interrupt_inline(dev, pcBuffer, &pIRQstr->irq_common))
	{
		pIRQstr->m_nCurrentIndexOfSigInfo = nSigInfoIndex;
		queue_work(g_pcie_gen_IRQworkqueue, &(pIRQstr->pcie_gen_work));
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}


static int RequestInterrupt_type1_static(struct pciedev_dev* a_pDev, const char* a_DRV_NAME, 
		struct device_irq_handling* a_reset_info, unsigned long(*a_fpCopy)(void*, const void*, unsigned long))
{
	struct pci_dev*  dev = a_pDev->pciedev_pci_dev;
	const size_t cunSigInfoBuf = SI_PAD_SIZE * sizeof(int);
	int result = 0;
	struct SIRQstruct1* pIRQstr;

	if (GET_FLAG_VALUE(a_pDev->m_ulnFlag, _IRQ_REQUEST_DONE)){ return -2; }

	a_pDev->irq_type = _IRQ_TYPE1_;

	pIRQstr = a_pDev->irqData = kzalloc(sizeof(struct SIRQstruct1), GFP_KERNEL);
	if (!pIRQstr)
	{
		ERRCT("ERROR: Unabble to create memory!\n");
		a_pDev->irq_type = _NO_IRQ_;
		return -ENOMEM;
	}

	if (Get_Interrupt_Info_global(&(pIRQstr->irq_common), cunSigInfoBuf, a_pDev->register_size,
		a_DRV_NAME, a_reset_info, a_fpCopy) < 0)
	{
		a_pDev->irq_type = _NO_IRQ_;
		a_pDev->irqData = NULL;
		kfree(pIRQstr);
		ERRCT("ERROR: Error during copying IRQ handling info!\n");
		return -1;
	}

	pIRQstr->m_pDev = a_pDev;

	ALERTCT("Initing work\n");
	vers_INIT_WORK(&(pIRQstr->pcie_gen_work), pcie_gen_do_work, a_pDev);
	ALERTCT("Requesting interupt\n");
	//IRQF_DISABLED - keep irqs disabled when calling the action handler.
	//                 DEPRECATED.This flag is a NOOP and scheduled to be removed
	//a_pDev->irq_flag = IRQF_SHARED | IRQF_DISABLED;
	a_pDev->irq_flag = IRQF_SHARED ;
	a_pDev->pci_dev_irq = dev->irq;
	a_pDev->irq_mode = 1;

	result = request_irq(a_pDev->pci_dev_irq, pcie_gen_interrupt,
		a_pDev->irq_flag, a_DRV_NAME, a_pDev);

	if (result)
	{
		/// Stex memory leak ka, piti dzvi
		kfree(pIRQstr);
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


int RequestInterrupt_type1(struct pciedev_dev* a_pDev, const char* a_DRV_NAME, unsigned long a_arg)
{
	return RequestInterrupt_type1_static(a_pDev, a_DRV_NAME, (struct device_irq_handling*)a_arg, copy_from_user);
}


static unsigned long CopyKernelData(void* a_to, const void* a_from, unsigned long a_n)
{
	memcpy(a_to,a_from,a_n);
	return 0;
}

int RequestInterrupt_type1_exp(struct pciedev_dev* a_pDev, const char* a_DRV_NAME, struct device_irq_handling* a_reset_info)
{
	return RequestInterrupt_type1_static(a_pDev, a_DRV_NAME, a_reset_info, CopyKernelData);
}
EXPORT_SYMBOL(RequestInterrupt_type1_exp);



void UnRequestInterrupt_type1_exp(struct pciedev_dev* a_pDev)
{
	struct SirqResetAndList *dynid, *n;
	struct SIRQstruct1* pIRQstr = a_pDev->irqData;

	if (GET_FLAG_VALUE(a_pDev->m_ulnFlag, _IRQ_REQUEST_DONE))
	{
		ALERTCT("Freeing Irq Number %d\n", (int)a_pDev->pci_dev_irq);
		free_irq(a_pDev->pci_dev_irq, a_pDev);

		if (g_pcie_gen_IRQworkqueue)flush_workqueue(g_pcie_gen_IRQworkqueue); // flush all pending
		//usleep(1);

		list_for_each_entry_safe(dynid, n, &pIRQstr->irq_common.listIRQreset, m_node) {
			list_del(&dynid->m_node);
			kfree(dynid);
		}

		SET_FLAG_VALUE(a_pDev->m_ulnFlag, _IRQ_REQUEST_DONE, 0);

		kfree(pIRQstr);
		a_pDev->irqData = NULL;
	}
}
EXPORT_SYMBOL(UnRequestInterrupt_type1_exp);



int RegisterUserForInterrupt_type1(struct pciedev_dev* a_pDev, unsigned long a_arg)
{
	struct SIRQstruct1*	pIRQstr		= a_pDev->irqData;
	uint64_t			ullnTmp2;
	int64_t				nTmp2;
	pid_t				cur_proc	= current->group_leader->pid;
	int					i, nIndex;

	if (!GET_FLAG_VALUE(a_pDev->m_ulnFlag, _IRQ_REQUEST_DONE) || a_pDev->irq_type != _IRQ_TYPE1_){ return -1; }
	if (EnterCritRegion(&a_pDev->dev_mut))return -ERESTARTSYS;

	if (pIRQstr->m_nInteruptWaiters < MAX_INTR_NUMB - 1)
	{
		for (i = 0; i < pIRQstr->m_nInteruptWaiters; ++i)
		{
			if (cur_proc == pIRQstr->m_IrqWaiters[i].m_nPID)
			{
				//mutex_unlock(&a_pDev->m_dev_mut);
				LeaveCritRegion(&a_pDev->dev_mut);
				return 0;
			}
		}

		nIndex = pIRQstr->m_nInteruptWaiters++;

		pIRQstr->m_IrqWaiters[nIndex].m_nPID = cur_proc;

		if (copy_from_user(&nTmp2, (const char*)a_arg, sizeof(int64_t)))
		{
			// additional info not provided
			pIRQstr->m_IrqWaiters[nIndex].m_nSigNum = SIGUSR1;
			pIRQstr->m_IrqWaiters[nIndex].m_unCalbackData = 0;
		}
		else
		{
			pIRQstr->m_IrqWaiters[nIndex].m_nSigNum = nTmp2 < 0 ? SIGUSR2 : (int)nTmp2;
			if (copy_from_user(&ullnTmp2, (const char*)(a_arg + sizeof(int64_t)), sizeof(uint64_t)))
			{
				// CallBack not provided
				pIRQstr->m_IrqWaiters[nIndex].m_unCalbackData = 0;
			}
			else
			{
				pIRQstr->m_IrqWaiters[nIndex].m_unCalbackData = ullnTmp2;
			}
		}

	}
	else // if( a_pDev->m_nInteruptWaiters < MAX_INTR_NUMB-1 )
	{
		//mutex_unlock(&a_pDev->m_dev_mut);
		LeaveCritRegion(&a_pDev->dev_mut);
		return -1;
	}
	LeaveCritRegion(&a_pDev->dev_mut);

	return 0;
}
