/*
*	Created on: Jan 16, 2015
*	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
*
*	New source file for hess driver
*  Initializing DMA engine to make several
*  DMA transaction implemented in this source
*
*/

#if 1
#define		_CREATION_BIT_		0
#define		_SHOULD_STOP_VALUE_	5

#include <linux/module.h>
#include <linux/dmaengine.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <mach/at_hdmac.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include "dma_engine_header.h"


typedef struct SDMAengine
{
	struct dma_chan*	m_pChan;
	void(*m_fpCallback)(void*);
	void*				m_pReserved;
	unsigned int		m_unFlags;
	int					m_nTransmitInProgress;

	int					m_nNumberOfTransfers;
	int					m_nMaxNumberOfTransfer;
	SSingleTransfer*	m_pTransferInfo;

	struct completion	m_cmp;
	struct task_struct	*m_task;
}SDMAengine;


void FreeAtDMAengineFast(void* a_pEngine);

static bool filter(struct dma_chan *chan, void *param)
{
	//static int s_nIteration = 0;
	int nParam = (int)param;
	//DEBUG1("param = %p, nParam = %d, s_nIteration = %d\n", param, nParam, ++s_nIteration);
	switch (nParam)
	{
	case 0:
		return dma_has_cap(DMA_SLAVE, chan->device->cap_mask) && dma_has_cap(DMA_PRIVATE, chan->device->cap_mask) ? true : false;
	default:
		return false;
	}
}


static int dma_thread_func(void *a_data);

void* GetAtDMAengineFast(void)
{
	SDMAengine* pEngine = kzalloc(sizeof(SDMAengine), GFP_KERNEL);
	dma_cap_mask_t mask;
	struct at_dma_slave* pAtSlave;
	unsigned int unMask;

	if (!pEngine)
	{
		ERRB("No memory!\n");
		return NULL;
	}

	dma_cap_zero(mask);
	//dma_cap_set(DMA_SLAVE, mask);
	dma_cap_set(DMA_MEMCPY, mask);

	pEngine->m_pChan = dma_request_channel(mask, filter, NULL);

	if (!pEngine->m_pChan)
	{
		ERRB("No channel available!\n");
		FreeAtDMAengineVeryFast(pEngine);
		return NULL;
	}

	if (!pEngine->m_pChan->private)
	{
		pAtSlave = kzalloc(sizeof(struct at_dma_slave), GFP_KERNEL);

		if (!pAtSlave)
		{
			ERRB("No memory!\n");
			FreeAtDMAengineVeryFast(pEngine);
			return NULL;
		}

		pEngine->m_pChan->private = pAtSlave;
		unMask = 1 << _CREATION_BIT_;
		pEngine->m_unFlags |= unMask;

	} // if (!s_vChannelsB[i]->private)

	pAtSlave = (struct at_dma_slave*)(pEngine->m_pChan->private);
	pAtSlave->dma_dev = pEngine->m_pChan->device->dev;
	pAtSlave->rx_reg = 0;
	pAtSlave->reg_width = AT_DMA_SLAVE_WIDTH_16BIT;
	pAtSlave->cfg = ATC_SRC_REP | ATC_DST_REP;
	pAtSlave->ctrla = ATC_SCSIZE_4 | ATC_DCSIZE_4;

	init_completion(&pEngine->m_cmp);
	//msleep(1);
	pEngine->m_task = kthread_run(dma_thread_func, pEngine, "dmathreadfordmaengine");

	return pEngine;
}
EXPORT_SYMBOL_GPL(GetAtDMAengineFast);


void StopAtDMAengineFast(void* a_pEngine);

void FreeAtDMAengineFast(void* a_pEngine)
{
	SDMAengine* pEngine = (SDMAengine*)a_pEngine;
	unsigned int unMask;

	if (!pEngine)return;

	StopAtDMAengineFast(pEngine);

	unMask = 1 << _CREATION_BIT_;
	if (pEngine->m_pChan && pEngine->m_pChan->private && (pEngine->m_unFlags&unMask))
	{
		kfree(pEngine->m_pChan->private);
		pEngine->m_pChan->private = NULL;
	}

	if (pEngine->m_pChan){ dma_release_channel(pEngine->m_pChan); }

	if (pEngine->m_pTransferInfo)kfree(pEngine->m_pTransferInfo);

	kfree(pEngine);
}
EXPORT_SYMBOL(FreeAtDMAengineFast);


struct device* GetAtDMAdeviceFast(void* a_pEngine)
{
	SDMAengine* pEngine = (SDMAengine*)a_pEngine;
	return pEngine->m_pChan->device->dev;
}
EXPORT_SYMBOL(GetAtDMAdeviceFast);



void StopAtDMAengineFast(void* a_pEngine)
{
	SDMAengine* pEngine = (SDMAengine*)a_pEngine;
	if (!pEngine) return;
	if (pEngine->m_pChan){ dmaengine_terminate_all(pEngine->m_pChan); }

	if (pEngine->m_task)
	{
		pEngine->m_nTransmitInProgress = _SHOULD_STOP_VALUE_;
		complete(&pEngine->m_cmp);
		kthread_stop(pEngine->m_task);
		msleep(2);
		pEngine->m_task = NULL;
		pEngine->m_nTransmitInProgress = 0;
	}
}
EXPORT_SYMBOL(StopAtDMAengineFast);


struct dma_async_tx_descriptor *
	atc_prep_dma_memcpyFifoNew(struct dma_chan *a_chan, int a_nTransfersCount,
	dma_addr_t a_dest, dma_addr_t a_destMin, dma_addr_t a_destMax, dma_addr_t a_FifoSwitchDst,
	dma_addr_t a_src, dma_addr_t a_FifoSwitchSrc, size_t a_len, size_t a_step, size_t a_fifoSwitchLen,
	unsigned long a_flags);

int AsyncAtDMAreadFifoFast(void* a_dmaEngine, int a_nTransfersCount, 
	dma_addr_t a_dest, dma_addr_t a_destMin, dma_addr_t a_destMax, dma_addr_t a_FifoSwitchDst,
	dma_addr_t a_src, dma_addr_t a_FifoSwitchSrc, size_t a_len, size_t a_step, size_t a_fifoSwitchLen,
	void* a_pCallbackData, void(*a_fpCallback)(void*))
{
	SDMAengine* pEngine = (SDMAengine*)a_dmaEngine;
	if (pEngine->m_nTransmitInProgress)return -1;

	if (!pEngine->m_nMaxNumberOfTransfer)
	{
		kfree(pEngine->m_pTransferInfo);
		pEngine->m_pTransferInfo = kzalloc(sizeof(SSingleTransfer), GFP_KERNEL | GFP_ATOMIC);
		if (!pEngine->m_pTransferInfo)
		{
			pEngine->m_nMaxNumberOfTransfer = 0;
			return -1;
		}
		pEngine->m_nMaxNumberOfTransfer = 1;
	}
	pEngine->m_nNumberOfTransfers = -1;
	
	//memcpy(pEngine->m_pTransferInfo, a_pTransferInfo, sizeof(SSingleTransfer)*a_transferNumber);
	//pEngine->m_nCurChanIndex = 0; //???
	pEngine->m_pTransferInfo->m_nSGcount = a_nTransfersCount;
	pEngine->m_pTransferInfo->m_dest0 = a_dest;
	pEngine->m_pTransferInfo->m_destMin = a_destMin;
	pEngine->m_pTransferInfo->m_destMax = a_destMax;
	pEngine->m_pTransferInfo->m_FifoSwitchDst = a_FifoSwitchDst;
	pEngine->m_pTransferInfo->m_src = a_src;
	pEngine->m_pTransferInfo->m_FifoSwitchSrc = a_FifoSwitchSrc;
	pEngine->m_pTransferInfo->m_transferLength = a_len;
	pEngine->m_pTransferInfo->m_dstBuffIncrement = a_step;
	pEngine->m_pTransferInfo->m_FifoSwtchLen = a_fifoSwitchLen;

	pEngine->m_nTransmitInProgress = 1;
	pEngine->m_fpCallback = a_fpCallback;
	pEngine->m_pReserved = a_pCallbackData;

	complete(&pEngine->m_cmp);

	return 0;
}
EXPORT_SYMBOL(AsyncAtDMAreadFifoFast);


int AsyncAtDMAreadVectFast(void* a_dmaEngine, int a_transferNumber, SSingleTransfer* a_pTransferInfo,
	void* a_pCallbackData, void(*a_fpCallback)(void*))
{
	SDMAengine* pEngine = (SDMAengine*)a_dmaEngine;
	if (pEngine->m_nTransmitInProgress)return -1;

	if (a_transferNumber > pEngine->m_nMaxNumberOfTransfer)
	{
		kfree(pEngine->m_pTransferInfo);
		pEngine->m_pTransferInfo = kzalloc(sizeof(SSingleTransfer)*a_transferNumber, GFP_KERNEL | GFP_ATOMIC);
		if (!pEngine->m_pTransferInfo)
		{
			pEngine->m_nMaxNumberOfTransfer = 0;
			return -1;
		}
		pEngine->m_nMaxNumberOfTransfer = a_transferNumber;
	}
	pEngine->m_nNumberOfTransfers = a_transferNumber;
	memcpy(pEngine->m_pTransferInfo, a_pTransferInfo, sizeof(SSingleTransfer)*a_transferNumber);
	//pEngine->m_nCurChanIndex = 0; //???
	pEngine->m_nTransmitInProgress = 1;
	pEngine->m_fpCallback = a_fpCallback;
	pEngine->m_pReserved = a_pCallbackData;

	complete(&pEngine->m_cmp);

	return 0;
}
EXPORT_SYMBOL(AsyncAtDMAreadVectFast);


int AsyncAtDMAreadFast(void* a_dmaEngine, int a_nRWnumber,
	dma_addr_t a_dests, dma_addr_t a_destMin, dma_addr_t a_destMax, dma_addr_t a_srcDev,
	size_t a_len, size_t a_step, void* a_callbackData, void(*a_fpCallback)(void*))
{
	SSingleTransfer aTransferInfo;
	aTransferInfo.m_nSGcount = a_nRWnumber;
	aTransferInfo.m_dest0 = a_dests;
	aTransferInfo.m_destMin = a_destMin;
	aTransferInfo.m_destMax = a_destMax;
	aTransferInfo.m_src = a_srcDev;
	aTransferInfo.m_transferLength = a_len;
	aTransferInfo.m_dstBuffIncrement = a_step;
	return AsyncAtDMAreadVectFast(a_dmaEngine, 1, &aTransferInfo, a_callbackData, a_fpCallback);
}
EXPORT_SYMBOL(AsyncAtDMAreadFast);

static inline void dmaengine_callback(void *a_pEngine);

static const enum dma_ctrl_flags s_flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT | DMA_COMPL_SKIP_DEST_UNMAP | DMA_COMPL_SKIP_SRC_UNMAP;

struct dma_async_tx_descriptor *
	atc_prep_dma_memcpyNewVect(struct dma_chan *a_chan, int a_nTransCount, SSingleTransfer* a_pTransInfo, unsigned long a_flags);

static int dma_thread_func(void *a_data)
{
	SDMAengine* pEngine = (SDMAengine*)a_data;
	struct dma_async_tx_descriptor *tx = NULL;
	dma_cookie_t aCookie;

	while (!kthread_should_stop())
	{
		wait_for_completion(&pEngine->m_cmp);

		if (unlikely(pEngine->m_nTransmitInProgress == 0)) continue;
		if (unlikely(pEngine->m_nTransmitInProgress == _SHOULD_STOP_VALUE_)) break;

		pEngine->m_nTransmitInProgress = 2;
		if (pEngine->m_nNumberOfTransfers > 0)
		{
			tx = atc_prep_dma_memcpyNewVect(pEngine->m_pChan, pEngine->m_nNumberOfTransfers, pEngine->m_pTransferInfo, s_flags);
		}
		else
		{
			tx = atc_prep_dma_memcpyFifoNew(pEngine->m_pChan, pEngine->m_pTransferInfo->m_nSGcount,
				pEngine->m_pTransferInfo->m_dest0, pEngine->m_pTransferInfo->m_destMin, pEngine->m_pTransferInfo->m_destMax,
				pEngine->m_pTransferInfo->m_FifoSwitchDst,
				pEngine->m_pTransferInfo->m_src, pEngine->m_pTransferInfo->m_FifoSwitchSrc, 
				pEngine->m_pTransferInfo->m_transferLength, pEngine->m_pTransferInfo->m_dstBuffIncrement,
				pEngine->m_pTransferInfo->m_FifoSwtchLen, s_flags);
		}
		if (unlikely(!tx))
		{
			ERRB("Preparing DMA error!\n");
			continue;
		}
		tx->callback = dmaengine_callback;
		tx->callback_param = pEngine;

		aCookie = dmaengine_submit(tx);
		if (dma_submit_error(aCookie))
		{
			//// Error handling
			ERRB("DMA submit error!\n");
			continue;
		}

		dma_async_issue_pending(pEngine->m_pChan);

	} // end while (!kthread_should_stop())

	return 0;
}


static inline void dmaengine_callback(void *a_pEngine)
{
	SDMAengine* pEngine = (SDMAengine*)a_pEngine;
	pEngine->m_nTransmitInProgress = 0;
	if (pEngine->m_fpCallback) (*pEngine->m_fpCallback)(pEngine->m_pReserved);
}



#endif
