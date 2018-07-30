/*
 *	File: mtcagen_irq_all.h
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


typedef struct SirqResetAndList
{
	struct list_head	m_node;
	device_irq_reset	m_reset;
}SirqResetAndList;


int Get_Interrupt_Info_global(struct SIRQstruct_all* a_pInfo, size_t a_unSigInfoBuf, int a_register_size,
	const char* a_DRV_NAME,
	struct device_irq_handling* a_reset_info, unsigned long(*a_fpCopy)(void*, const void*, unsigned long));

/*
 * The top-half interrupt handler.
 */
static inline int pcie_gen_interrupt_inline(struct pciedev_dev * a_dev, char* a_pcInfoBuffer, struct SIRQstruct_all* a_pIrqStruct)
{
	int* pnInts = (int*)a_pcInfoBuffer;
	device_irq_reset* pReset;
	SirqResetAndList* pIRQresetAndList;
	struct timeval	aTm;

	do_gettimeofday(&aTm);

	// try to understand if interrupt is done by our device

	pnInts[2] = aTm.tv_sec;
	pnInts[3] = aTm.tv_usec;

	//pnInts[4] = pIRQstr->m_nNumberOfIRQtop4++;
	//pnInts[4] = a_pIrqStruct->numberOfIRQtop++;

	list_for_each_entry(pIRQresetAndList, &a_pIrqStruct->listIRQreset, m_node) {
		pReset = &pIRQresetAndList->m_reset;
		Read_Write_Private(pReset->reg_size, pReset->rw_access_mode,
			(char*)a_dev->memmory_base[pReset->bar_rw] +
			pReset->offset_rw,
			&pReset->dataW, &pReset->maskW, a_pcInfoBuffer + pReset->offsetInSigInfo);

	}

	return 1;
}
