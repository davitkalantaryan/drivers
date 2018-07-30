/*
*	File: mtcagen_irq_all.c
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

//enum _IRQ_TYPES_{ _NO_IRQ_, _IRQ_TYPE1_ };
extern void UnRequestInterrupt_type1_exp(struct pciedev_dev* a_pDev);
extern void UnRequestInterrupt_type2_exp(struct pciedev_dev* a_pDev);
extern int RegisterUserForInterrupt_type1(struct pciedev_dev* a_pDev, unsigned long a_arg);
extern void RemoveUserPID_type1(struct pciedev_dev* a_dev, pid_t a_nPID);

void UnRequestInterrupt_all(struct pciedev_dev* a_pDev)
{
	if (!GET_FLAG_VALUE(a_pDev->m_ulnFlag, _IRQ_REQUEST_DONE)){ return; }
	switch (a_pDev->irq_type)
	{
	case _NO_IRQ_:break;
	case _IRQ_TYPE1_:
		UnRequestInterrupt_type1_exp(a_pDev);
		break;
	case _IRQ_TYPE2_:
		UnRequestInterrupt_type2_exp(a_pDev);
		break;
	default:break;
	}
}



int RegisterUserForInterrupt_all(struct pciedev_dev* a_pDev, unsigned long a_arg)
{
	switch (a_pDev->irq_type)
	{
	case _NO_IRQ_:break;
	case _IRQ_TYPE1_:
		DEBUGNEW("\n");
		return RegisterUserForInterrupt_type1(a_pDev, a_arg);
	case _IRQ_TYPE2_: return 0;
	default:break;
	}
	return -1;
}



int UnregisterUserFromInterrupt_all(struct pciedev_dev* a_pDev)
{
	pid_t		cur_proc = current->group_leader->pid;

	if (!GET_FLAG_VALUE(a_pDev->m_ulnFlag, _IRQ_REQUEST_DONE)){ return -1; }

	switch (a_pDev->irq_type)
	{
	case _NO_IRQ_:break;
	case _IRQ_TYPE1_:
		RemoveUserPID_type1(a_pDev, cur_proc);
		return 0;
	case _IRQ_TYPE2_: return 0;
	default:break;
	}
	return -2;
}


#if 0
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
typedef struct device_irq_handling
{
	int16_t					is_irq_bar;
	u_int16_t				is_irq_offset;
	u_int16_t				is_irq_register_size;
	///////////////////////////////////////////////
	int16_t					resetRegsNumber;
	device_irq_reset*		resetRegsInfo;
	///////////////////////////////////////////////
	//u_int64_t				inform_user_type;
}device_irq_handling;
#endif


int Get_Interrupt_Info_global(struct SIRQstruct_all* a_pInfo, size_t a_unSigInfoBuf, int a_register_size,
	const char* a_DRV_NAME,
	struct device_irq_handling* a_reset_info, unsigned long(*a_fpCopy)(void*, const void*, unsigned long))
{
	size_t unOffset, unSize;
	device_irq_handling aDevIRQhandling;
	SirqResetAndList* pIRQresetAndList;
	device_irq_reset* pReset;
	device_irq_reset* __user pResetUser;
	int nNumber, i;

	if ((*a_fpCopy)(&aDevIRQhandling, a_reset_info, sizeof(device_irq_handling)))
	{
		ERRCT("Unable to get info for irq handling!\n");
		return -1;
	}
	else{nNumber = aDevIRQhandling.resetRegsNumber > 0 ? aDevIRQhandling.resetRegsNumber : 0;}

	if (aDevIRQhandling.is_irq_bar < 0 || aDevIRQhandling.is_irq_bar >= NUMBER_OF_BARS)
	{
		a_pInfo->is_line_shared = 0;
	}
	else
	{
		a_pInfo->is_line_shared = 1;
		a_pInfo->is_irq_bar = aDevIRQhandling.is_irq_bar;
		a_pInfo->is_irq_offset = aDevIRQhandling.is_irq_offset;
		a_pInfo->is_irq_register_size = aDevIRQhandling.is_irq_register_size;
		CORRECT_REGISTER_SIZE(a_pInfo->is_irq_register_size, a_register_size);
	}

	DEBUGCT("nNumber = %d\n", nNumber);

	INIT_LIST_HEAD(&a_pInfo->listIRQreset);
	pResetUser = aDevIRQhandling.resetRegsInfo;

	for (i = 0; i < nNumber; ++i)
	{
		pIRQresetAndList = kzalloc(sizeof(SirqResetAndList), GFP_KERNEL);
		if (unlikely(!pIRQresetAndList))
		{
			WARNCT("Is not able to create memory\n");
			break;
		}

		if ((*a_fpCopy)(&(pIRQresetAndList->m_reset), pResetUser, sizeof(device_irq_reset)) ||
			(pIRQresetAndList->m_reset.bar_rw<0 || pIRQresetAndList->m_reset.bar_rw >= NUMBER_OF_BARS))
		{
			kfree(pIRQresetAndList);
			WARNCT("IRQ reset info provided less than expected\n");
			break;
		}

		pReset = &(pIRQresetAndList->m_reset);
		CORRECT_REGISTER_SIZE(pReset->reg_size, a_register_size);

		list_add_tail(&pIRQresetAndList->m_node, &(a_pInfo->listIRQreset));
		++pResetUser;
		DEBUGCT("mode = %d, bar = %d, offset = %d, dataW = %u, %s = %u\n",
			pReset->rw_access_mode, pReset->bar_rw, (int)pReset->offset_rw, pReset->dataW,
			pReset->rw_access_mode > MTCA_SIMPLE_WRITE ? "sizeR" : "maskW", pReset->maskW);
	}
	//pIRQstr->m_nIRQmaxSize = nMaxSize;
	//DEBUGCT("a_pDev->m_nIRQmaxSize = %d\n", nMaxSize);

	i = 0;
	//nNumberOnSiginfo = (SI_PAD_SIZE - _SIG_HEADER_OFFSET_INT_) * sizeof(int) / nMaxSize;
	unOffset = _SIG_HEADER_OFFSET_;

	list_for_each_entry(pIRQresetAndList, &(a_pInfo->listIRQreset), m_node) {

		pReset = &pIRQresetAndList->m_reset;
		unSize = (size_t)__GetRegisterSizeInBytes__(pReset->reg_size);

		if ((unOffset + unSize) <= a_unSigInfoBuf)
		{
			if (pReset->rw_access_mode < MTCA_SIMPLE_WRITE)
			{
				pReset->rw_access_mode = MT_READ_TO_EXTRA_BUFFER;
				pReset->offsetInSigInfo = unOffset;
				unOffset += unSize;
			}
		} // if ((unOffset + unSize)<=cunSigInfoBuf)
		else
		{
			if (pReset->rw_access_mode < MTCA_SIMPLE_WRITE)
			{
				pReset->rw_access_mode = MTCA_SIMPLE_READ;
			}
		}
	}


	return 0;
}
