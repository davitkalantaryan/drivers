/*
*	Created on: Jan 16, 2015
*	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
*
*	New source file for hess driver
*  Initializing DMA engine to make several
*  DMA transaction implemented in this source
*
*/

#define		_CREATION_BIT_		0
//#define		_DMA_MAPPING_FLAG_	DMA_BIDIRECTIONAL

#include <linux/module.h>
#include <linux/dmaengine.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <mach/at_hdmac.h>
#include "dma_engine_header.h"

typedef struct SDMAengine
{
	struct dma_chan*	m_pChan;
	void(*m_fpCallback)(void*);
	void*				m_pReserved;
	//dma_addr_t			m_dstAddress;
	//size_t				m_unDstSize;
	unsigned int		m_unFlags;
	int					m_nTransmitInProgress;
}SDMAengine;


void FreeAtDMAengineVeryFast(void* a_pEngine);

#if 1
static bool filter(struct dma_chan *chan, void *param)
{
	return dma_has_cap(DMA_MEMCPY, chan->device->cap_mask) ? true : false;
}
#else
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
#endif



void* GetAtDMAengineVeryFast(void)
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

	return pEngine;
}
EXPORT_SYMBOL(GetAtDMAengineVeryFast);



void StopAtDMAengineVeryFast(void* a_pEngine);

void FreeAtDMAengineVeryFast(void* a_pEngine)
{
	SDMAengine* pEngine = (SDMAengine*)a_pEngine;
	unsigned int unMask;

	if (!pEngine)return;

	StopAtDMAengineVeryFast(pEngine);

	unMask = 1 << _CREATION_BIT_;
	if (pEngine->m_pChan && pEngine->m_pChan->private && (pEngine->m_unFlags&unMask))
	{
		kfree(pEngine->m_pChan->private);
		pEngine->m_pChan->private = NULL;
	}

	if (pEngine->m_pChan){ dma_release_channel(pEngine->m_pChan); }

	kfree(pEngine);
}
EXPORT_SYMBOL(FreeAtDMAengineVeryFast);


struct device* GetAtDMAdeviceVeryFast(void* a_pEngine)
{
	SDMAengine* pEngine = (SDMAengine*)a_pEngine;
	return pEngine->m_pChan->device->dev;
}
EXPORT_SYMBOL(GetAtDMAdeviceVeryFast);


void StopAtDMAengineVeryFast(void* a_pEngine)
{
	SDMAengine* pEngine = (SDMAengine*)a_pEngine;
	if (pEngine && pEngine->m_pChan){ dmaengine_terminate_all(pEngine->m_pChan); }
}
EXPORT_SYMBOL(StopAtDMAengineVeryFast);

struct dma_async_tx_descriptor *
	atc_prep_dma_memcpyNew(struct dma_chan *a_chan, int a_nDstSize,
	dma_addr_t a_dests, dma_addr_t a_destMin, dma_addr_t a_destMax,
	dma_addr_t a_src, size_t a_len, size_t a_step, unsigned long a_flags);

struct dma_async_tx_descriptor *
	atc_prep_dma_memcpyNewVect(struct dma_chan *a_chan, int a_nTransCount, SSingleTransfer* a_pTransInfo, unsigned long a_flags);

static inline void dmaengine_callback(void *a_pEngine);

static const enum dma_ctrl_flags s_flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT | DMA_COMPL_SKIP_DEST_UNMAP | DMA_COMPL_SKIP_SRC_UNMAP;

struct dma_async_tx_descriptor *
	atc_prep_dma_memcpyFifoNew(struct dma_chan *a_chan, int a_nTransfersCount,
	dma_addr_t a_dest, dma_addr_t a_destMin, dma_addr_t a_destMax, dma_addr_t a_FifoSwitchDst,
	dma_addr_t a_src, dma_addr_t a_FifoSwitchSrc, size_t a_len, size_t a_step, size_t a_fifoSwitchLen, unsigned long a_flags);

int AsyncAtDMAreadFifoVeryFast(void* a_dmaEngine, int a_nTransfersCount,
	dma_addr_t a_dest, dma_addr_t a_destMin, dma_addr_t a_destMax, dma_addr_t a_FifoSwitchDst,
	dma_addr_t a_src, dma_addr_t a_FifoSwitchSrc, size_t a_len, size_t a_step, size_t a_fifoSwitchLen,
	void* a_pCallbackData, void(*a_fpCallback)(void*))
{
	struct dma_async_tx_descriptor *tx;
	dma_cookie_t aCookie;
	SDMAengine* pEngine = (SDMAengine*)a_dmaEngine;

	if (pEngine->m_nTransmitInProgress)return -1;

	pEngine->m_nTransmitInProgress = 1;

	pEngine->m_fpCallback = a_fpCallback;
	pEngine->m_pReserved = a_pCallbackData;

	tx = atc_prep_dma_memcpyFifoNew(pEngine->m_pChan, a_nTransfersCount, a_dest, a_destMin, a_destMax,
		a_FifoSwitchDst, a_src, a_FifoSwitchSrc, a_len, a_step, a_fifoSwitchLen, s_flags);

	if (unlikely(!tx))
	{
		ERR_IRQB("Preparing DMA error!\n");
		goto out;
	}

	tx->callback = dmaengine_callback;
	tx->callback_param = pEngine;

	aCookie = dmaengine_submit(tx);
	if (dma_submit_error(aCookie))
	{
		//// Error handling
		ERR_IRQB("DMA submit error!\n");
		goto out;
	}

	dma_async_issue_pending(pEngine->m_pChan);

out:
	return 0;
}
EXPORT_SYMBOL(AsyncAtDMAreadFifoVeryFast);



int AsyncAtDMAreadVectVeryFast(void* a_dmaEngine, int a_transferNumber, SSingleTransfer* a_pTransferInfo,
	void* a_pCallbackData, void(*a_fpCallback)(void*))
{
	struct dma_async_tx_descriptor *tx;
	dma_cookie_t aCookie;
	SDMAengine* pEngine = (SDMAengine*)a_dmaEngine;

	if (pEngine->m_nTransmitInProgress)return -1;

	//pEngine->m_unDstSize = a_step*a_nRWnumber;
	//pEngine->m_dstAddress = dma_map_single(pEngine->m_pChan->device->dev, a_dests, pEngine->m_unDstSize, _DMA_MAPPING_FLAG_);

	pEngine->m_nTransmitInProgress = 1;

	pEngine->m_fpCallback = a_fpCallback;
	pEngine->m_pReserved = a_pCallbackData;

	tx = atc_prep_dma_memcpyNewVect(pEngine->m_pChan, a_transferNumber, a_pTransferInfo, s_flags);

	if (unlikely(!tx))
	{
		ERR_IRQB("Preparing DMA error!\n");
		goto out;
	}

	tx->callback = dmaengine_callback;
	tx->callback_param = pEngine;

	aCookie = dmaengine_submit(tx);
	if (dma_submit_error(aCookie))
	{
		//// Error handling
		ERR_IRQB("DMA submit error!\n");
		goto out;
	}

	dma_async_issue_pending(pEngine->m_pChan);

out:
	return 0;
}
EXPORT_SYMBOL(AsyncAtDMAreadVectVeryFast);


int AsyncAtDMAreadVeryFast(void* a_dmaEngine, int a_nRWnumber,
	dma_addr_t a_dests, dma_addr_t a_destMin, dma_addr_t a_destMax, dma_addr_t a_srcDev,
	size_t a_len, size_t a_step, void* a_callbackData, void(*a_fpCallback)(void*))
{
	struct dma_async_tx_descriptor *tx;
	dma_cookie_t aCookie;
	SDMAengine* pEngine = (SDMAengine*)a_dmaEngine;

	if (pEngine->m_nTransmitInProgress)return -1;

	//pEngine->m_unDstSize = a_step*a_nRWnumber;
	//pEngine->m_dstAddress = dma_map_single(pEngine->m_pChan->device->dev, a_dests, pEngine->m_unDstSize, _DMA_MAPPING_FLAG_);

	pEngine->m_nTransmitInProgress = 1;

	pEngine->m_fpCallback = a_fpCallback;
	pEngine->m_pReserved = a_callbackData;

	tx = atc_prep_dma_memcpyNew(pEngine->m_pChan, a_nRWnumber, a_dests, a_destMin, a_destMax, a_srcDev, a_len, a_step, s_flags);

	if (unlikely(!tx))
	{
		ERR_IRQB("Preparing DMA error!\n");
		goto out;
	}

	tx->callback = dmaengine_callback;
	tx->callback_param = pEngine;

	aCookie = dmaengine_submit(tx);
	if (dma_submit_error(aCookie))
	{
		//// Error handling
		ERR_IRQB("DMA submit error!\n");
		goto out;
	}

	dma_async_issue_pending(pEngine->m_pChan);

out:
	return 0;
}
EXPORT_SYMBOL(AsyncAtDMAreadVeryFast);



static inline void dmaengine_callback(void *a_pEngine)
{
	SDMAengine* pEngine = (SDMAengine*)a_pEngine;

	pEngine->m_nTransmitInProgress = 0;

	if (pEngine->m_fpCallback) (*pEngine->m_fpCallback)(pEngine->m_pReserved);

	//dma_unmap_single(pEngine->m_pChan->device->dev, pEngine->m_dstAddress, pEngine->m_unDstSize,_DMA_MAPPING_FLAG_);
}
