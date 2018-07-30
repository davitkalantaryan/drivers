/*
*	Created on: Jan 16, 2015
*	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
*
*	New source file for hess driver
*   Initializing DMA engine to make several
*   DMA transaction implemented in this source
*
*/

#define	ATC_DEFAULT_CFG		(ATC_FIFOCFG_HALFFIFO)
#define	ATC_DEFAULT_CTRLA	(0)
#define	ATC_DEFAULT_CTRLB	(ATC_SIF(AT_DMA_MEM_IF) \
				|ATC_DIF(AT_DMA_MEM_IF))

#include <asm/dma.h>
#include <linux/dmaengine.h>
#include <linux/interrupt.h>
#include <linux/dmapool.h>
#include <../drivers/dma/at_hdmac_regs.h>
#include <mach/at_hdmac.h>
#include "dma_engine_header.h"


static struct at_desc *atc_desc_getZn(struct at_dma_chan *atchan);
static void atc_desc_chain(struct at_desc **first, struct at_desc **prev, struct at_desc *desc);
static void atc_desc_put(struct at_dma_chan *atchan, struct at_desc *desc);

/**
* atc_prep_dma_memcpyNew - prepare a memcpy operation
* @chan:		the channel to prepare operation on
* @a_nDstSize:	number of DMA transactions should be performed
* @dest:		operation virtual destination address (size=a_nDstSize*len*2)
* @a_src:		operation virtual source address. After each trans. repeated.
* @a_len:		operation length of each transaction
* @a_step:		operation length of each transaction
* @flags:		tx descriptor status flags
*/
struct dma_async_tx_descriptor *
	atc_prep_dma_memcpyNewVect(struct dma_chan *a_chan, int a_nTransCount, SSingleTransfer* a_pTransInfo, unsigned long a_flags)
{
	//static int nIteration = 0;
	struct at_dma_chan	*atchan = to_at_dma_chan(a_chan);
	struct at_desc		*desc = NULL;
	struct at_desc		*first = NULL;
	struct at_desc		*prev = NULL;
	size_t			xfer_count;
	size_t			offset;
	unsigned int		src_width;
	unsigned int		dst_width;
	u32			ctrla;
	u32			ctrlb;
	int i1, i2;
	dma_addr_t step, len;
	dma_addr_t aDest, destMin, destMax;
	dma_addr_t src;

	int nSGcount;

	dev_vdbg(chan2dev(a_chan), "atc_prep_dma_memcpyNewVect: f0x%lx\n", a_flags);

#if 0
	if (unlikely(!a_len))
	{
		dev_dbg(chan2dev(a_chan), "atc_prep_dma_memcpyNewVect: length is zero!\n");
		return NULL;
	}
#endif

	ctrla = ATC_DEFAULT_CTRLA;
	ctrlb = ATC_DEFAULT_CTRLB | ATC_IEN
		| ATC_SRC_ADDR_MODE_INCR
		| ATC_DST_ADDR_MODE_INCR
		| ATC_FC_MEM2MEM;


	for (i1 = 0; i1 < a_nTransCount; ++i1)
	{
		nSGcount = a_pTransInfo[i1].m_nSGcount;
		step = (dma_addr_t)a_pTransInfo[i1].m_dstBuffIncrement;
		len = (dma_addr_t)a_pTransInfo[i1].m_transferLength;
		aDest = a_pTransInfo[i1].m_dest0;
		destMin = a_pTransInfo[i1].m_destMin;
		destMax = a_pTransInfo[i1].m_destMax;
		src = a_pTransInfo[i1].m_src;
		for (i2 = 0; i2 < nSGcount; ++i2, aDest += step)
		{
			if (aDest + len > destMax)aDest = destMin;

			/*
			* We can be a lot more clever here, but this should take care
			* of the most common optimization.
			*/
			if (!((src | aDest | len) & 3)) {
				ctrla |= ATC_SRC_WIDTH_WORD | ATC_DST_WIDTH_WORD;
				src_width = dst_width = 2;
			}
			else if (!((src | aDest | len) & 1)) {
				ctrla |= ATC_SRC_WIDTH_HALFWORD | ATC_DST_WIDTH_HALFWORD;
				src_width = dst_width = 1;
			}
			else {
				ctrla |= ATC_SRC_WIDTH_BYTE | ATC_DST_WIDTH_BYTE;
				src_width = dst_width = 0;
			}


			for (offset = 0; offset < len; offset += xfer_count << src_width) 
			{
				xfer_count = min_t(size_t, (len - offset) >> src_width,ATC_BTSIZE_MAX);

				desc = atc_desc_getZn(atchan);
				if (!desc) goto err_desc_get;

				desc->lli.saddr = src + offset;
				desc->lli.daddr = aDest + offset;
				desc->lli.ctrla = ctrla | xfer_count;
				desc->lli.ctrlb = ctrlb;

				desc->txd.cookie = 0;

				atc_desc_chain(&first, &prev, desc);
			} // for (offset = 0; offset < len; offset += xfer_count << src_width)

		} // for (i = 0; i < a_nDstSize; ++i, aDest += (dma_addr_t)a_step)

	} // for (i1 = 0; i1 < a_nTransCount; ++i1)

	

	/* First descriptor of the chain embedds additional information */
	first->txd.cookie = -EBUSY;
	first->len = a_pTransInfo[0].m_transferLength;

	/* set end-of-link to the last link descriptor of list*/
	set_desc_eol(desc);

	first->txd.flags = a_flags; /* client is in control of this ack */

	return &first->txd;

err_desc_get:
	atc_desc_put(atchan, first);
	return NULL;
}
EXPORT_SYMBOL(atc_prep_dma_memcpyNewVect);



/**
* atc_prep_dma_memcpyNew - prepare a memcpy operation
* @chan:		the channel to prepare operation on
* @a_nDstSize:	number of DMA transactions should be performed
* @dest:		operation virtual destination address (size=a_nDstSize*len*2)
* @a_src:		operation virtual source address. After each trans. repeated.
* @a_len:		operation length of each transaction
* @a_step:		operation length of each transaction
* @flags:		tx descriptor status flags
*/
struct dma_async_tx_descriptor *
	atc_prep_dma_memcpyFifoNew(struct dma_chan *a_chan, int a_nTransfersCount,
	dma_addr_t a_dest, dma_addr_t a_destMin, dma_addr_t a_destMax, dma_addr_t a_FifoSwitchDst,
	dma_addr_t a_src, dma_addr_t a_FifoSwitchSrc,size_t a_len, size_t a_step, size_t a_fifoSwitchLen, unsigned long a_flags)
{
	struct at_dma_chan	*atchan = to_at_dma_chan(a_chan);
	struct at_desc		*desc = NULL;
	struct at_desc		*first = NULL;
	struct at_desc		*prev = NULL;
	size_t			xfer_count;
	size_t			offset;
	unsigned int		src_width;
	unsigned int		dst_width;
	u32			ctrla;
	u32			ctrlb;
	int i;
	dma_addr_t aDest = a_dest;

	dev_vdbg(chan2dev(a_chan), "atc_prep_dma_memcpyFifoNew: d0x%x s0x%x l0x%zx f0x%lx\n",
		a_dest, a_src, a_len, a_flags);

	if (unlikely(!a_len)) {
		dev_dbg(chan2dev(a_chan), "atc_prep_dma_memcpyFifoNew: length is zero!\n");
		return NULL;
	}

	ctrla = ATC_DEFAULT_CTRLA;
	ctrlb = ATC_DEFAULT_CTRLB | ATC_IEN
		| ATC_SRC_ADDR_MODE_INCR
		| ATC_DST_ADDR_MODE_INCR
		| ATC_FC_MEM2MEM;


	for (i = 0; i < a_nTransfersCount; ++i, aDest += (dma_addr_t)a_step)
	{

		/*
		* We can be a lot more clever here, but this should take care
		* of the most common optimization.
		*/
		if (!((a_FifoSwitchSrc | a_FifoSwitchDst | a_fifoSwitchLen) & 3)) {
			ctrla |= ATC_SRC_WIDTH_WORD | ATC_DST_WIDTH_WORD;
			src_width = dst_width = 2;
		}
		else if (!((a_FifoSwitchSrc | a_FifoSwitchDst | a_fifoSwitchLen) & 1)) {
			ctrla |= ATC_SRC_WIDTH_HALFWORD | ATC_DST_WIDTH_HALFWORD;
			src_width = dst_width = 1;
		}
		else {
			ctrla |= ATC_SRC_WIDTH_BYTE | ATC_DST_WIDTH_BYTE;
			src_width = dst_width = 0;
		}


		for (offset = 0; offset < a_fifoSwitchLen; offset += xfer_count << src_width)
		{
			xfer_count = min_t(size_t, (a_fifoSwitchLen - offset) >> src_width,
				ATC_BTSIZE_MAX);

			desc = atc_desc_getZn(atchan);
			if (!desc) goto err_desc_get;

			desc->lli.saddr = a_FifoSwitchSrc + offset;
			desc->lli.daddr = a_FifoSwitchDst + offset;
			desc->lli.ctrla = ctrla | xfer_count;
			desc->lli.ctrlb = ctrlb;

			desc->txd.cookie = 0;

			atc_desc_chain(&first, &prev, desc);
		}


		///////////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////
		if (aDest + a_len > a_destMax)aDest = a_destMin;

		/*
		* We can be a lot more clever here, but this should take care
		* of the most common optimization.
		*/
		if (!((a_src | aDest | a_len) & 3)) {
			ctrla |= ATC_SRC_WIDTH_WORD | ATC_DST_WIDTH_WORD;
			src_width = dst_width = 2;
		}
		else if (!((a_src | aDest | a_len) & 1)) {
			ctrla |= ATC_SRC_WIDTH_HALFWORD | ATC_DST_WIDTH_HALFWORD;
			src_width = dst_width = 1;
		}
		else {
			ctrla |= ATC_SRC_WIDTH_BYTE | ATC_DST_WIDTH_BYTE;
			src_width = dst_width = 0;
		}


		for (offset = 0; offset < a_len; offset += xfer_count << src_width)
		{
			xfer_count = min_t(size_t, (a_len - offset) >> src_width,ATC_BTSIZE_MAX);

			desc = atc_desc_getZn(atchan);
			if (!desc)goto err_desc_get;

			desc->lli.saddr = a_src + offset;
			desc->lli.daddr = aDest + offset;
			desc->lli.ctrla = ctrla | xfer_count;
			desc->lli.ctrlb = ctrlb;

			desc->txd.cookie = 0;

			atc_desc_chain(&first, &prev, desc);
		}

	}

	/* First descriptor of the chain embedds additional information */
	first->txd.cookie = -EBUSY;
	first->len = a_fifoSwitchLen;

	/* set end-of-link to the last link descriptor of list*/
	set_desc_eol(desc);

	first->txd.flags = a_flags; /* client is in control of this ack */

	return &first->txd;

err_desc_get:
	atc_desc_put(atchan, first);
	return NULL;
}
EXPORT_SYMBOL(atc_prep_dma_memcpyFifoNew);



/**
* atc_prep_dma_memcpyNew - prepare a memcpy operation
* @chan:		the channel to prepare operation on
* @a_nDstSize:	number of DMA transactions should be performed
* @dest:		operation virtual destination address (size=a_nDstSize*len*2)
* @a_src:		operation virtual source address. After each trans. repeated.
* @a_len:		operation length of each transaction
* @a_step:		operation length of each transaction
* @flags:		tx descriptor status flags
*/
struct dma_async_tx_descriptor *
	atc_prep_dma_memcpyNew(struct dma_chan *a_chan, int a_nDstSize,
	dma_addr_t a_dests, dma_addr_t a_destMin, dma_addr_t a_destMax,
	dma_addr_t a_src, size_t a_len, size_t a_step, unsigned long a_flags)
{
	struct at_dma_chan	*atchan = to_at_dma_chan(a_chan);
	struct at_desc		*desc = NULL;
	struct at_desc		*first = NULL;
	struct at_desc		*prev = NULL;
	size_t			xfer_count;
	size_t			offset;
	unsigned int		src_width;
	unsigned int		dst_width;
	u32			ctrla;
	u32			ctrlb;
	int i;
	dma_addr_t aDest = a_dests;

	dev_vdbg(chan2dev(a_chan), "atc_prep_dma_memcpyNew: d0x%x s0x%x l0x%zx f0x%lx\n",
		a_dests, a_src, a_len, a_flags);

	if (unlikely(!a_len)) {
		dev_dbg(chan2dev(a_chan), "atc_prep_dma_memcpyNew: length is zero!\n");
		return NULL;
	}

	ctrla = ATC_DEFAULT_CTRLA;
	ctrlb = ATC_DEFAULT_CTRLB | ATC_IEN
		| ATC_SRC_ADDR_MODE_INCR
		| ATC_DST_ADDR_MODE_INCR
		| ATC_FC_MEM2MEM;


	for (i = 0; i < a_nDstSize; ++i, aDest += (dma_addr_t)a_step)
	{

		if (aDest + a_len > a_destMax)aDest = a_destMin;

		/*
		* We can be a lot more clever here, but this should take care
		* of the most common optimization.
		*/
		if (!((a_src | aDest | a_len) & 3)) {
			ctrla |= ATC_SRC_WIDTH_WORD | ATC_DST_WIDTH_WORD;
			src_width = dst_width = 2;
		}
		else if (!((a_src | aDest | a_len) & 1)) {
			ctrla |= ATC_SRC_WIDTH_HALFWORD | ATC_DST_WIDTH_HALFWORD;
			src_width = dst_width = 1;
		}
		else {
			ctrla |= ATC_SRC_WIDTH_BYTE | ATC_DST_WIDTH_BYTE;
			src_width = dst_width = 0;
		}


		for (offset = 0; offset < a_len; offset += xfer_count << src_width) {
			xfer_count = min_t(size_t, (a_len - offset) >> src_width,
				ATC_BTSIZE_MAX);

			desc = atc_desc_getZn(atchan);
			if (!desc)
				goto err_desc_get;

			desc->lli.saddr = a_src + offset;
			desc->lli.daddr = aDest + offset;
			desc->lli.ctrla = ctrla | xfer_count;
			desc->lli.ctrlb = ctrlb;

			desc->txd.cookie = 0;

			atc_desc_chain(&first, &prev, desc);
		}

	}

	/* First descriptor of the chain embedds additional information */
	first->txd.cookie = -EBUSY;
	first->len = a_len;

	/* set end-of-link to the last link descriptor of list*/
	set_desc_eol(desc);

	first->txd.flags = a_flags; /* client is in control of this ack */

	return &first->txd;

err_desc_get:
	atc_desc_put(atchan, first);
	return NULL;
}
EXPORT_SYMBOL(atc_prep_dma_memcpyNew);



/**
* atc_prep_dma_memcpyVect - prepare a memcpy operation
* @chan:		the channel to prepare operation on
* @a_nDstSize:	number of DMA transactions should be performed
* @a_vDests:	operation virtual destination addresses (vector)
* @a_src:		operation virtual source address. After each trans. repeated.
* @len:		operation length of each transaction
* @flags:		tx descriptor status flags
*/
struct dma_async_tx_descriptor *
	atc_prep_dma_memcpyVect(struct dma_chan *a_chan, int a_nDstSize, dma_addr_t a_vDests[], dma_addr_t a_src,
	size_t a_len, unsigned long a_flags)
{
	struct at_dma_chan	*atchan = to_at_dma_chan(a_chan);
	struct at_desc		*desc = NULL;
	struct at_desc		*first = NULL;
	struct at_desc		*prev = NULL;
	size_t			xfer_count;
	size_t			offset;
	unsigned int		src_width;
	unsigned int		dst_width;
	u32			ctrla;
	u32			ctrlb;
	int i;

	dev_vdbg(chan2dev(a_chan), "atc_prep_dma_memcpyVect: d0x%x s0x%x l0x%zx f0x%lx\n",
		a_vDests[0], a_src, a_len, a_flags);

	if (unlikely(!a_len)) {
		dev_dbg(chan2dev(a_chan), "atc_prep_dma_memcpyVect: length is zero!\n");
		return NULL;
	}

	ctrla = ATC_DEFAULT_CTRLA;
	ctrlb = ATC_DEFAULT_CTRLB | ATC_IEN
		| ATC_SRC_ADDR_MODE_INCR
		| ATC_DST_ADDR_MODE_INCR
		| ATC_FC_MEM2MEM;


	for (i = 0; i < a_nDstSize; ++i)
	{

		/*
		* We can be a lot more clever here, but this should take care
		* of the most common optimization.
		*/
		if (!((a_src | a_vDests[i] | a_len) & 3)) {
			ctrla |= ATC_SRC_WIDTH_WORD | ATC_DST_WIDTH_WORD;
			src_width = dst_width = 2;
		}
		else if (!((a_src | a_vDests[i] | a_len) & 1)) {
			ctrla |= ATC_SRC_WIDTH_HALFWORD | ATC_DST_WIDTH_HALFWORD;
			src_width = dst_width = 1;
		}
		else {
			ctrla |= ATC_SRC_WIDTH_BYTE | ATC_DST_WIDTH_BYTE;
			src_width = dst_width = 0;
		}


		for (offset = 0; offset < a_len; offset += xfer_count << src_width) {
			xfer_count = min_t(size_t, (a_len - offset) >> src_width,
				ATC_BTSIZE_MAX);

			desc = atc_desc_getZn(atchan);
			if (!desc)
				goto err_desc_get;

			desc->lli.saddr = a_src + offset;
			desc->lli.daddr = a_vDests[i] + offset;
			desc->lli.ctrla = ctrla | xfer_count;
			desc->lli.ctrlb = ctrlb;

			desc->txd.cookie = 0;

			atc_desc_chain(&first, &prev, desc);
		}

	}

	/* First descriptor of the chain embedds additional information */
	first->txd.cookie = -EBUSY;
	first->len = a_len;

	/* set end-of-link to the last link descriptor of list*/
	set_desc_eol(desc);

	first->txd.flags = a_flags; /* client is in control of this ack */

	return &first->txd;

err_desc_get:
	atc_desc_put(atchan, first);
	return NULL;
}
EXPORT_SYMBOL(atc_prep_dma_memcpyVect);


/**
* atc_desc_put - move a descriptor, including any children, to the free list
* @atchan: channel we work on
* @desc: descriptor, at the head of a chain, to move to free list
*/
static void atc_desc_put(struct at_dma_chan *atchan, struct at_desc *desc)
{
	if (desc) {
		struct at_desc *child;

		spin_lock_bh(&atchan->lock);
		list_for_each_entry(child, &desc->tx_list, desc_node)
			dev_vdbg(chan2dev(&atchan->chan_common),
			"moving child desc %p to freelist\n",
			child);
		list_splice_init(&desc->tx_list, &atchan->free_list);
		dev_vdbg(chan2dev(&atchan->chan_common),
			"moving desc %p to freelist\n", desc);
		list_add(&desc->desc_node, &atchan->free_list);
		spin_unlock_bh(&atchan->lock);
	}
}



/**
* atc_desc_chain - build chain adding a descripor
* @first: address of first descripor of the chain
* @prev: address of previous descripor of the chain
* @desc: descriptor to queue
*
* Called from prep_* functions
*/
static void atc_desc_chain(struct at_desc **first, struct at_desc **prev,
struct at_desc *desc)
{
	if (!(*first)) {
		*first = desc;
	}
	else {
		/* inform the HW lli about chaining */
		(*prev)->lli.dscr = desc->txd.phys;
		/* insert the link descriptor to the LD ring */
		list_add_tail(&desc->desc_node,
			&(*first)->tx_list);
	}
	*prev = desc;
}



/**
* atc_dostart - starts the DMA engine for real
* @atchan: the channel we want to start
* @first: first descriptor in the list we want to begin with
*
* Called with atchan->lock held and bh disabled
*/
static void atc_dostart(struct at_dma_chan *atchan, struct at_desc *first)
{
	struct at_dma	*atdma = to_at_dma(atchan->chan_common.device);

	/* ASSERT:  channel is idle */
	if (atc_chan_is_enabled(atchan)) {
		dev_err(chan2dev(&atchan->chan_common),
			"BUG: Attempted to start non-idle channel\n");
		dev_err(chan2dev(&atchan->chan_common),
			"  channel: s0x%x d0x%x ctrl0x%x:0x%x l0x%x\n",
			channel_readl(atchan, SADDR),
			channel_readl(atchan, DADDR),
			channel_readl(atchan, CTRLA),
			channel_readl(atchan, CTRLB),
			channel_readl(atchan, DSCR));

		/* The tasklet will hopefully advance the queue... */
		return;
	}

	vdbg_dump_regs(atchan);

	/* clear any pending interrupt */
	while (dma_readl(atdma, EBCISR))
		cpu_relax();

	channel_writel(atchan, SADDR, 0);
	channel_writel(atchan, DADDR, 0);
	channel_writel(atchan, CTRLA, 0);
	channel_writel(atchan, CTRLB, 0);
	channel_writel(atchan, DSCR, first->txd.phys);
	dma_writel(atdma, CHER, atchan->mask);

	vdbg_dump_regs(atchan);
}



/**
* atc_assign_cookie - compute and assign new cookie
* @atchan: channel we work on
* @desc: descriptor to assign cookie for
*
* Called with atchan->lock held and bh disabled
*/
static dma_cookie_t
atc_assign_cookie(struct at_dma_chan *atchan, struct at_desc *desc)
{
	dma_cookie_t cookie = atchan->chan_common.cookie;

	if (++cookie < 0)
		cookie = 1;

	atchan->chan_common.cookie = cookie;
	desc->txd.cookie = cookie;

	return cookie;
}


/**
* atc_tx_submit - set the prepared descriptor(s) to be executed by the engine
* @desc: descriptor at the head of the transaction chain
*
* Queue chain if DMA engine is working already
*
* Cookie increment and adding to active_list or queue must be atomic
*/
static dma_cookie_t atc_tx_submit(struct dma_async_tx_descriptor *tx)
{
	struct at_desc		*desc = txd_to_at_desc(tx);
	struct at_dma_chan	*atchan = to_at_dma_chan(tx->chan);
	dma_cookie_t		cookie;

	spin_lock_bh(&atchan->lock);
	cookie = atc_assign_cookie(atchan, desc);

	if (list_empty(&atchan->active_list)) {
		dev_vdbg(chan2dev(tx->chan), "tx_submit: started %u\n",
			desc->txd.cookie);
		atc_dostart(atchan, desc);
		list_add_tail(&desc->desc_node, &atchan->active_list);
	}
	else {
		dev_vdbg(chan2dev(tx->chan), "tx_submit: queued %u\n",
			desc->txd.cookie);
		list_add_tail(&desc->desc_node, &atchan->queue);
	}

	spin_unlock_bh(&atchan->lock);

	return cookie;
}


/**
* atc_alloc_descriptor - allocate and return an initialized descriptor
* @chan: the channel to allocate descriptors for
* @gfp_flags: GFP allocation flags
*
* Note: The ack-bit is positioned in the descriptor flag at creation time
*       to make initial allocation more convenient. This bit will be cleared
*       and control will be given to client at usage time (during
*       preparation functions).
*/
static struct at_desc *atc_alloc_descriptor(struct dma_chan *chan,
	gfp_t gfp_flags)
{
	struct at_desc	*desc = NULL;
	struct at_dma	*atdma = to_at_dma(chan->device);
	dma_addr_t phys;

	desc = dma_pool_alloc(atdma->dma_desc_pool, gfp_flags, &phys);
	if (desc) {
		memset(desc, 0, sizeof(struct at_desc));
		INIT_LIST_HEAD(&desc->tx_list);
		dma_async_tx_descriptor_init(&desc->txd, chan);
		/* txd.flags will be overwritten in prep functions */
		desc->txd.flags = DMA_CTRL_ACK;
		desc->txd.tx_submit = atc_tx_submit;
		desc->txd.phys = phys;
	}

	return desc;
}


/**
* atc_desc_get - get an unused descriptor from free_list
* @atchan: channel we want a new descriptor for
*/
static struct at_desc *atc_desc_getZn(struct at_dma_chan *atchan)
{
	struct at_desc *desc, *_desc;
	struct at_desc *ret = NULL;
	unsigned int i = 0;
	LIST_HEAD(tmp_list);

	spin_lock_bh(&atchan->lock);
	list_for_each_entry_safe(desc, _desc, &atchan->free_list, desc_node) {
		i++;
		if (async_tx_test_ack(&desc->txd)) {
			list_del(&desc->desc_node);
			ret = desc;
			break;
		}
		dev_dbg(chan2dev(&atchan->chan_common),
			"desc %p not ACKed\n", desc);
	}
	spin_unlock_bh(&atchan->lock);
	dev_vdbg(chan2dev(&atchan->chan_common),
		"scanned %u descriptors on freelist\n", i);

	/* no more descriptor available in initial pool: create one more */
	if (!ret) {
		ret = atc_alloc_descriptor(&atchan->chan_common, GFP_ATOMIC);
		if (ret) {
			spin_lock_bh(&atchan->lock);
			atchan->descs_allocated++;
			spin_unlock_bh(&atchan->lock);
		}
		else {
			dev_err(chan2dev(&atchan->chan_common),
				"not enough descriptors available\n");
		}
	}

	return ret;
}



void DumpAllStupidWarnings_at_dma_handler(void)
{
	chan2dev(NULL);
	atc_dump_lli(NULL, NULL);
	chan2parent(NULL);
}
