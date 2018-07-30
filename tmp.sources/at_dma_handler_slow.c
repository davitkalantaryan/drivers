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
#define		_NUMBER_OF_CHANNELS_	2
#define		_SHOULD_STOP_VALUE_		5

#include <linux/module.h>
#include <linux/dmaengine.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <mach/at_hdmac.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include "dma_engine_header.h"

/*
typedef struct SSingleTransfer
{
	int			m_nSGcount;
	dma_addr_t	m_dest0;
	dma_addr_t	m_destMin;
	dma_addr_t	m_destMax;
	dma_addr_t	m_src;
	size_t		m_transferLength;
	size_t		m_dstBuffIncrement;
}SSingleTransfer;
*/

typedef struct SDMAengine
{
	struct dma_chan*	m_vpChan[_NUMBER_OF_CHANNELS_];
	void(*m_fpCallback)(void*);
	int					m_nCurChanIndex;
	/*dma_addr_t			m_dstAddress;
	dma_addr_t			m_dstMin;
	dma_addr_t			m_dstMax;
	dma_addr_t			m_srcAddress;
	int					m_wordsRemaining;
	size_t				m_transferLen;
	size_t				m_destBufIncrement;*/
	int					m_nNumberOfTransfers;
	int					m_nIndexOfTransfer;
	int					m_nMaxNumberOfTransfer;
	SSingleTransfer*	m_pTransferInfo;
	void*				m_pReserved;
	unsigned int		m_unCreationFlags;
	struct completion	m_cmp;
	struct task_struct	*m_task;
	int					m_nTransmitInProgress;
}SDMAengine;


void FreeAtDMAengineSlow(void* a_pEngine);

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

void* GetAtDMAengineSlow(void)
{
	SDMAengine* pEngine = kzalloc(sizeof(SDMAengine), GFP_KERNEL);
	int i = 0;
	dma_cap_mask_t mask;
	struct at_dma_slave* pAtSlave;
	unsigned int unMask;

	if (!pEngine)
	{
		ERRB("No memory!\n");
		return NULL;
	}

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);
	dma_cap_set(DMA_PRIVATE, mask);

	for (; i < _NUMBER_OF_CHANNELS_; ++i)
	{
		pEngine->m_vpChan[i] = dma_request_channel(mask, filter, NULL);

		if (!pEngine->m_vpChan[i])
		{
			ERRB("No channel available!\n");
			FreeAtDMAengineSlow(pEngine);
			return NULL;
		}

		if (!pEngine->m_vpChan[i]->private)
		{
			pAtSlave = kzalloc(sizeof(struct at_dma_slave), GFP_KERNEL);
			
			if (!pAtSlave)
			{
				ERRB("No memory!\n");
				FreeAtDMAengineSlow(pEngine);
				return NULL;
			}

			pEngine->m_vpChan[i]->private = pAtSlave;
			unMask = 1 << i;
			pEngine->m_unCreationFlags |= unMask;

		} // if (!s_vChannelsB[i]->private)

		pAtSlave = (struct at_dma_slave*)(pEngine->m_vpChan[i]->private);
		pAtSlave->dma_dev = pEngine->m_vpChan[i]->device->dev;
		pAtSlave->rx_reg = 0;
		pAtSlave->reg_width = AT_DMA_SLAVE_WIDTH_16BIT;
		pAtSlave->cfg = ATC_SRC_REP | ATC_DST_REP;
		pAtSlave->ctrla = ATC_SCSIZE_4 | ATC_DCSIZE_4;

	} // for (; i < _NUMBER_OF_CHANNELS_; ++i)

	init_completion(&pEngine->m_cmp);
	//msleep(1);
	pEngine->m_task = kthread_run(dma_thread_func, pEngine, "dmathreadfordmaengine");

	return pEngine;
}
EXPORT_SYMBOL_GPL(GetAtDMAengineSlow);


void StopAtDMAengineSlow(void* a_pEngine);

void FreeAtDMAengineSlow(void* a_pEngine)
{
	SDMAengine* pEngine = (SDMAengine*)a_pEngine;
	int i;
	unsigned int unMask;

	if (!pEngine)return;

	StopAtDMAengineSlow(pEngine);

	for (i=0; i < _NUMBER_OF_CHANNELS_; ++i)
	{
		unMask = 1 << i;
		if (pEngine->m_vpChan[i] && pEngine->m_vpChan[i]->private && (pEngine->m_unCreationFlags&unMask))
		{
			kfree(pEngine->m_vpChan[i]->private);
			pEngine->m_vpChan[i]->private = NULL;
		}

		if (pEngine->m_vpChan[i])
		{
			dma_release_channel(pEngine->m_vpChan[i]);
			//s_vChannelsB[i] = NULL;
		}

	} // for (; i < _NUMBER_OF_CHANNELS_; ++i)

	if (pEngine->m_pTransferInfo)kfree(pEngine->m_pTransferInfo);

	kfree(pEngine);
}
EXPORT_SYMBOL(FreeAtDMAengineSlow);


struct device* GetAtDMAdeviceSlow(void* a_pEngine)
{
	SDMAengine* pEngine = (SDMAengine*)a_pEngine;
	return pEngine->m_vpChan[0]->device->dev;
}
EXPORT_SYMBOL(GetAtDMAdeviceSlow);



void StopAtDMAengineSlow(void* a_pEngine)
{
	SDMAengine* pEngine = (SDMAengine*)a_pEngine;
	int i = 0;
	if (!pEngine)return;
	for (; i < _NUMBER_OF_CHANNELS_; ++i)
	{
		if (pEngine->m_vpChan[i])
		{
			dmaengine_terminate_all(pEngine->m_vpChan[i]);
		}

	} // for (; i < _NUMBER_OF_CHANNELS_; ++i)

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
EXPORT_SYMBOL(StopAtDMAengineSlow);

#if 0
typedef struct SDMAengine
{
	struct dma_chan*	m_vpChan[_NUMBER_OF_CHANNELS_];
	void(*m_fpCallback)(void*);
	int					m_nCurChanIndex;
	/*dma_addr_t			m_dstAddress;
	dma_addr_t			m_dstMin;
	dma_addr_t			m_dstMax;
	dma_addr_t			m_srcAddress;
	int					m_wordsRemaining;
	size_t				m_transferLen;
	size_t				m_destBufIncrement;*/
	int					m_nNumberOfTransfersRemaining;
	int					m_nMaxNumberOfTransfer;
	SSingleTransfer*	m_pTransferInfo;
	void*				m_pReserved;
	unsigned int		m_unCreationFlags;
	struct completion	m_cmp;
	struct task_struct	*m_task;
	int					m_nTransmitInProgress;
}SDMAengine;
#endif

int AsyncAtDMAreadFifoSlow(void* a_dmaEngine, int a_nTransfersCount,
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
EXPORT_SYMBOL(AsyncAtDMAreadFifoSlow);


int AsyncAtDMAreadVectSlow(void* a_dmaEngine, int a_transferNumber, SSingleTransfer* a_pTransferInfo,
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
	pEngine->m_nIndexOfTransfer = 0;
	memcpy(pEngine->m_pTransferInfo, a_pTransferInfo, sizeof(SSingleTransfer)*a_transferNumber);
	//pEngine->m_nCurChanIndex = 0; //???
	pEngine->m_nTransmitInProgress = 1;
	pEngine->m_fpCallback = a_fpCallback;
	pEngine->m_pReserved = a_pCallbackData;
	//pEngine->m_nCurChanIndex = 0; //???

	complete(&pEngine->m_cmp);

	return 0;
}
EXPORT_SYMBOL(AsyncAtDMAreadVectSlow);


#if 0
typedef struct SSingleTransfer
{
	int			m_nSGcount;
	dma_addr_t	m_dest0;
	dma_addr_t	m_destMin;
	dma_addr_t	m_destMax;
	dma_addr_t	m_src;
	size_t		m_transferLength;
	size_t		m_dstBuffIncrement;
}SSingleTransfer;
#endif

int AsyncAtDMAreadSlow(void* a_dmaEngine, int a_nRWnumber,
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
	return AsyncAtDMAreadVectSlow(a_dmaEngine, 1, &aTransferInfo, a_callbackData, a_fpCallback);
}
EXPORT_SYMBOL(AsyncAtDMAreadSlow);



static inline void dmaengine_callback(void *a_pEngine);

static const enum dma_ctrl_flags s_flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT | DMA_COMPL_SKIP_DEST_UNMAP | DMA_COMPL_SKIP_SRC_UNMAP;

#if 0
typedef struct SDMAengine
{
	struct dma_chan*	m_vpChan[_NUMBER_OF_CHANNELS_];
	void(*m_fpCallback)(void*);
	int					m_nCurChanIndex;
	/*dma_addr_t			m_dstAddress;
	dma_addr_t			m_dstMin;
	dma_addr_t			m_dstMax;
	dma_addr_t			m_srcAddress;
	int					m_wordsRemaining;
	size_t				m_transferLen;
	size_t				m_destBufIncrement;*/
	int					m_nNumberOfTransfers;
	int					m_nIndexOfTransfer;
	int					m_nMaxNumberOfTransfer;
	SSingleTransfer*	m_pTransferInfo;
	void*				m_pReserved;
	unsigned int		m_unCreationFlags;
	struct completion	m_cmp;
	struct task_struct	*m_task;
	int					m_nTransmitInProgress;
}SDMAengine;
#endif

static int dma_thread_func(void *a_data)
{
	SDMAengine* pEngine = (SDMAengine*)a_data;
	struct dma_async_tx_descriptor *tx = NULL;
	struct dma_chan* pChan = NULL;
	//int timeout = 3000; // 3 seconds
	//unsigned long tmo = msecs_to_jiffies(timeout);
	dma_cookie_t aCookie;
	SSingleTransfer* pCurTransInfo;

	while (!kthread_should_stop())
	{
		//wait_for_completion_timeout(&pEngine->m_cmp, tmo);
		//printk(KERN_ALERT "++++++++++++ &pEngine->m_cmp=%p\n", (&pEngine->m_cmp));
		wait_for_completion(&pEngine->m_cmp);
		//printk(KERN_ALERT "+++++++++++completed!\n");

		if (unlikely(pEngine->m_nTransmitInProgress == 0)) continue;
		if (unlikely(pEngine->m_nTransmitInProgress == _SHOULD_STOP_VALUE_)) break;

		pChan = pEngine->m_vpChan[pEngine->m_nCurChanIndex];

		if (pEngine->m_nNumberOfTransfers > 0)
		{
			if (pEngine->m_nTransmitInProgress == 2 && --pEngine->m_pTransferInfo[pEngine->m_nIndexOfTransfer].m_nSGcount <= 0)
			{
				if ((++pEngine->m_nIndexOfTransfer) >= pEngine->m_nNumberOfTransfers)
				{
					goto out;
				} // end if (pEngine->m_nTransmitInProgress == 2 &&...)
			} // end if (pEngine->m_nTransmitInProgress == 2 &&...)

			pCurTransInfo = &(pEngine->m_pTransferInfo[pEngine->m_nIndexOfTransfer]);
			tx = pChan->device->device_prep_dma_memcpy(pChan, pCurTransInfo->m_dest0, pCurTransInfo->m_src,
				pCurTransInfo->m_transferLength, s_flags);
		}
		else
		{
			pCurTransInfo = pEngine->m_pTransferInfo;
			if (pEngine->m_nTransmitInProgress && pCurTransInfo->m_nSGcount == 0) goto out;
			if (pEngine->m_nTransmitInProgress == 1)
			{
				tx = pChan->device->device_prep_dma_memcpy(pChan, pCurTransInfo->m_FifoSwitchDst, 
					pCurTransInfo->m_FifoSwitchSrc,pCurTransInfo->m_FifoSwtchLen, s_flags);
			}
			else
			{
				tx = pChan->device->device_prep_dma_memcpy(pChan, pCurTransInfo->m_dest0, pCurTransInfo->m_src,
					pCurTransInfo->m_transferLength, s_flags);
			}
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

		dma_async_issue_pending(pChan);

		//tasklet_enable(s_chanTasklets[snChannelIndex]);
		pEngine->m_nCurChanIndex = (pEngine->m_nCurChanIndex + 1) % _NUMBER_OF_CHANNELS_;

		continue;

	out:
		pEngine->m_nTransmitInProgress = 0;
		if (pEngine->m_fpCallback) (*pEngine->m_fpCallback)(pEngine->m_pReserved);

	} // end while (!kthread_should_stop())

	return 0;
}


#if 0
typedef struct SSingleTransfer
{
	int			m_nSGcount;
	dma_addr_t	m_dest0;
	dma_addr_t	m_destMin;
	dma_addr_t	m_destMax;
	dma_addr_t	m_src;
	size_t		m_transferLength;
	size_t		m_dstBuffIncrement;
}SSingleTransfer;
#endif


static inline void dmaengine_callback(void *a_pEngine)
{
	SDMAengine* pEngine = (SDMAengine*)a_pEngine;
	SSingleTransfer* pCurTransInfo = &(pEngine->m_pTransferInfo[pEngine->m_nIndexOfTransfer]);

	//dma_unmap_single(pEngine->m_pChan->device->dev, pEngine->m_dstAddress, pEngine->m_unDstSize,_DMA_MAPPING_FLAG_);
	if (pEngine->m_nNumberOfTransfers < 0)
	{
		if (pEngine->m_nTransmitInProgress == 2)
		{
			pCurTransInfo->m_dest0 += pCurTransInfo->m_dstBuffIncrement;
			if (pCurTransInfo->m_dest0 + pCurTransInfo->m_transferLength > pCurTransInfo->m_destMax)pCurTransInfo->m_dest0 = pCurTransInfo->m_destMin;
			--pEngine->m_pTransferInfo->m_nSGcount;
			pEngine->m_nTransmitInProgress = 1;
		}
		else
		{
			pEngine->m_nTransmitInProgress = 2;
		}
	}
	else
	{
		pCurTransInfo->m_dest0 += pCurTransInfo->m_dstBuffIncrement;
		if (pCurTransInfo->m_dest0 + pCurTransInfo->m_transferLength > pCurTransInfo->m_destMax)pCurTransInfo->m_dest0 = pCurTransInfo->m_destMin;
		pEngine->m_nTransmitInProgress = 2;
	}
	complete(&pEngine->m_cmp);
}



#endif
