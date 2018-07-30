/*
 *
 *	Created on: Jan 16, 2015
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *	New source file for hess driver
 *
 *
 *
 */

#define	ATC_DEFAULT_CTRLA	(0)


#include <asm/dma.h>
#include <linux/dmaengine.h>
#include <linux/interrupt.h>
#include <linux/dmapool.h>
//#include <../drivers/dma/at_hdmac_regs.h>
#include <mach/at_hdmac.h>


/////////////////////////////////////////////////////////////////////////
///// Testing part
/////////////////////////////////////////////////////////////////////////
#define SMC_MEM_START		AT91_CHIPSELECT_2 //0x30000000	// physical address region of the controlbus bus mapping *is fixed!*
#define SMC_MEM_LEN			0x4000000 // 64M
#define BASE_NECTAR_ADC							0x0000
#define IOMEM_NAME			"bus2fpga"
//#define OFFS_EVENT_FIFO_WORD_COUNT				0x0002	//  ,ro
#define OFFS_EVENT_FIFO					0x0080		//  ,ro
#define DMA_TEST_SIZE 16
#define ROW_SIZE	8

#include "hess_drv_exp.h"

static int s_nChannelNumber2 = 1;
module_param_named(channel2, s_nChannelNumber2, int, S_IRUGO | S_IWUSR);

static int s_nRealDevice = 1;
module_param_named(real, s_nRealDevice, int, S_IRUGO | S_IWUSR);


static const char* s_dma_statuses[] = {
	"DMA_SUCCESS",
	"DMA_IN_PROGRESS",
	"DMA_PAUSED",
	"DMA_ERROR"
};

static const dma_addr_t t_dmaSrc =    SMC_MEM_START + BASE_NECTAR_ADC + OFFS_EVENT_FIFO;
									//SMC_MEM_START + BASE_NECTAR_ADC + OFFS_EVENT_FIFO;


static bool filter2222(struct dma_chan *chan, void *param)
{

#if 1
	//struct at_dma	*atdma = to_at_dma(chan->device);

	printk(KERN_ALERT "chan=%p, param=%p, device_prep_dma_cyclic = %p\n", chan, param, chan->device->device_prep_dma_cyclic);

	/*
	//if (!dma_has_cap(DMA_CYCLIC, atdma->dma_common.cap_mask))
	//if (!dma_has_cap(DMA_SG, atdma->dma_common.cap_mask))
	if (!chan->device->device_prep_dma_cyclic)
	{
		printk(KERN_ALERT "No cyclic capability!\n");
		return false;
	}
	*/

	if (chan)
	{
		struct at_dma_slave* pSlave = (struct at_dma_slave*)kzalloc(sizeof(struct at_dma_slave), GFP_KERNEL);
		//pSlave->
		pSlave->dma_dev = chan->device->dev;
		pSlave->rx_reg = t_dmaSrc;
		pSlave->reg_width = AT_DMA_SLAVE_WIDTH_16BIT;
		pSlave->cfg = ATC_SRC_REP | ATC_DST_REP;
		pSlave->ctrla = ATC_SCSIZE_4 | ATC_DCSIZE_4;
		chan->private = pSlave;
	}
	return true;
#else
	int nParam = (int)param;

	printk(KERN_ALERT "param = %p, nParam = %d, s_nIteration = %d\n", param, nParam, s_nIteration);

	if (s_nChannelNumber2 != s_nIteration++)
		return false;

	switch (nParam)
	{
	case 0:
		return dma_has_cap(DMA_SLAVE, chan->device->cap_mask) && dma_has_cap(DMA_PRIVATE, chan->device->cap_mask) ? true : false;
	default:
		return false;
	}
#endif

}

static char *s_pDestination = NULL;

static void dmatest_callback(void *completion)
{
	//complete(completion);
	printk(KERN_ALERT "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!completed!\n");
	PRINT_MEMORY(s_pDestination, 16);
}

#define		_NUMBER_OF_BUFFERS_	32

static dma_addr_t s_dmaDsts = 0;
static struct dma_chan *s_chan = NULL;

struct dma_async_tx_descriptor *
atc_prep_dma_memcpyNew(struct dma_chan *chan, int a_nDstSize, dma_addr_t a_dests, dma_addr_t src,
size_t len, unsigned long flags);

int TestDmaEngine(void)
{
	dma_cap_mask_t mask;
	dma_addr_t	dmaSrc = 0;
	struct dma_async_tx_descriptor *tx = NULL;
	enum dma_ctrl_flags 	flags;
	int nRet = 0;
	dma_cookie_t		cookie;
	enum dma_status		status;
	//struct scatterlist sg_rx;
	char *pSrc = NULL;
	//struct dma_slave_config aConfig;
	//struct at_dma		*atdma; //???
	//struct at_dma_chan *atchan;
	struct at_dma_slave* pAtSlaveTemp;

	printk(KERN_ALERT "43;  ENXIO = %d, MAX_DMA_ADDRESS = 0x%x\n", ENXIO, (unsigned int)MAX_DMA_ADDRESS);

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);
	//dma_cap_set(DMA_PRIVATE, mask);
	//dma_cap_set(DMA_CYCLIC, mask);
	//dma_cap_set(DMA_SG,mask);
	//dma_cap_set(DMA_ASYNC_TX, mask);

	if (!s_chan)
	{
		s_chan = dma_request_channel(mask, filter2222, (void*)0);
		printk(KERN_ALERT "chan = %p, chan->private = %p\n", s_chan, s_chan?s_chan->private:NULL);

		if (!s_chan)
		{
			printk(KERN_ERR "No channel!!!\n");
			return -1;
		}
	}

	//chan->device->device_control(chan, DMA_SLAVE_CONFIG, 1);  // This is not implemented for at_hdmac driver, why?
	printk(KERN_ALERT "chan->client_count = %d\n", (int)s_chan->client_count);
	printk(KERN_ALERT "DMA channel name: %s\n", dma_chan_name(s_chan));

	if (s_nRealDevice)
	{
		dmaSrc = SMC_MEM_START + BASE_NECTAR_ADC + OFFS_EVENT_FIFO;
	}
	else
	{
		pSrc = kzalloc(DMA_TEST_SIZE, GFP_DMA | GFP_KERNEL);
		printk(KERN_ALERT "pSrc = %p\n", pSrc);
		if (!pSrc)
		{
			nRet = -2;
			goto functionExit2;
		}
		memset(pSrc, 1, DMA_TEST_SIZE);

		dmaSrc = dma_map_single(s_chan->device->dev, pSrc, DMA_TEST_SIZE, DMA_TO_DEVICE);
		printk(KERN_ALERT "dmaSrc = %lu\n", (unsigned long)dmaSrc);
		if (!dmaSrc)
		{
			nRet = -3;
			goto freeSourceBuf;
		}
	}


	////////////////////////////////////////////////
	if (!s_pDestination)
	{
		s_pDestination = dma_alloc_coherent(s_chan->device->dev, _NUMBER_OF_BUFFERS_*DMA_TEST_SIZE, &s_dmaDsts, GFP_DMA | GFP_KERNEL);
		if (!s_pDestination)
		{
			nRet = -3;
			goto functionExit1;
		}
		memset(s_pDestination, 0, DMA_TEST_SIZE);

		/*for (i = 0; i < _NUMBER_OF_BUFFERS_;++i)
		{
			s_dmaDsts[i] = dmaDst;
			dmaDst += DMA_TEST_SIZE;
		}*/
		

		//sg_init_one(&sg_rx, s_pDestination, DMA_TEST_SIZE);

		PRINT_MEMORY(s_pDestination, _NUMBER_OF_BUFFERS_*DMA_TEST_SIZE);
	}


	///////////////////////////////////////////////////////////////////////////////////////////
	flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT
		| DMA_COMPL_SKIP_DEST_UNMAP | DMA_COMPL_SKIP_SRC_UNMAP;

	//flags = DMA_FROM_DEVICE,
	//	DMA_PREP_INTERRUPT | DMA_CTRL_ACK;

	if (s_nRealDevice)
	{
		//tx = s_chan->device->device_prep_slave_sg(s_chan, &sg_rx, 1, DMA_FROM_DEVICE, flags);
		//tx = s_chan->device->device_prep_dma_memcpy(s_chan, s_dmaDst, dmaSrc, DMA_TEST_SIZE, flags);
		tx = atc_prep_dma_memcpyNew(s_chan, _NUMBER_OF_BUFFERS_, s_dmaDsts, dmaSrc, DMA_TEST_SIZE, flags);
		//printk(KERN_ALERT "++++++++++++++device_prep_slave_sg = %p\n", s_chan->device->device_prep_slave_sg);//device_prep_dma_cyclic
		//printk(KERN_ALERT "++++++++++++++device_prep_dma_cyclic = %p\n", s_chan->device->device_prep_dma_cyclic);//device_prep_dma_cyclic
		//goto functionExit1;
	}
	else
		tx = s_chan->device->device_prep_dma_memcpy(s_chan, s_dmaDsts, dmaSrc, DMA_TEST_SIZE, flags);

	if (!tx)
	{
		nRet = -6;
		printk(KERN_ERR "prep_dma error!\n");
		goto functionExit1;
	}

	tx->callback = dmatest_callback;
	//atdma = to_at_dma(s_chan->device);
	//atchan = to_at_dma_chan(s_chan);
	//printk(KERN_ALERT "atdma = %p, atdma->regs = %p, atchan->mask = %d\n", atdma, atdma->regs, (int)atchan->mask);
	pAtSlaveTemp = s_chan->private;
	printk(KERN_ALERT "chan->private = %p\n", s_chan->private);
	if (pAtSlaveTemp)
	{
		printk(KERN_ALERT "pAtSlaveTemp->ctrla & ATC_SCSIZE_MASK = %d\n", (int)(pAtSlaveTemp->ctrla & ATC_SCSIZE_MASK));
	}
	cookie = dmaengine_submit(tx);
	//cookie = atc_tx_submit2222(tx);

	if (dma_submit_error(cookie)) {
		nRet = -7;
		printk(KERN_ERR "submit error\n");
		goto functionExit1;
	}

	dma_async_issue_pending(s_chan);

	status = dma_async_is_tx_complete(s_chan, cookie, NULL, NULL);
	printk(KERN_ALERT "status is: %s\n", s_dma_statuses[status]);

	if (status != DMA_SUCCESS)
	{
		printk(KERN_ALERT "Waiting 100ms\n");
		msleep(100);
		status = dma_async_is_tx_complete(s_chan, cookie, NULL, NULL);
		printk(KERN_ALERT "status is: %s\n", s_dma_statuses[status]);

		if (status != DMA_SUCCESS)
		{
			printk(KERN_ALERT "Waiting 1000ms\n");
			msleep(1000);
			status = dma_async_is_tx_complete(s_chan, cookie, NULL, NULL);
			printk(KERN_ALERT "status is: %s\n", s_dma_statuses[status]);

			if (status != DMA_SUCCESS)
			{
				printk(KERN_ERR "DMA didn't finish! If it finish latter, then callback will print text\n");;
				nRet = -8;
				goto functionExit1;
			}
		}
	}
	else
	{
		printk(KERN_ALERT "Ready imediatly!\n");
	}

	/////////////////////////////////////////

functionExit1:

	if (s_chan && pSrc && dmaSrc) dma_unmap_single(s_chan->device->dev, dmaSrc, DMA_TEST_SIZE, DMA_TO_DEVICE);

	if (s_pDestination)
	{
		PRINT_MEMORY(s_pDestination, _NUMBER_OF_BUFFERS_*DMA_TEST_SIZE);
	}

freeSourceBuf:
	if(pSrc) kfree(pSrc);

	/*if (!s_nRealDevice)
	{
		dmaengine_terminate_all(s_chan);
		dma_release_channel(s_chan);
		s_chan = NULL;
	}*/

functionExit2:

	return nRet;
}



void ClearDMAtestStuff(void)
{

	printk(KERN_ALERT "ClearDMAtestStuff 2\n");

	if (s_chan){ dmaengine_terminate_all(s_chan); }

	if(s_nRealDevice)
	{
		if (s_chan && s_chan->private)
		{
			void* pSlave = s_chan->private;
			s_chan->private = NULL;
			kfree(pSlave);
		}
	}


	if (s_pDestination)
	{
		dma_free_coherent(s_chan->device->dev, _NUMBER_OF_BUFFERS_*DMA_TEST_SIZE, s_pDestination, s_dmaDsts);
		s_pDestination = NULL;
		//s_dmaDst = 0;
	}

	//printk(KERN_ALERT "ClearDMAtestStuff 1\n");
	//return;

	if (s_chan)
	{
		//dmaengine_terminate_all(s_chan);
		dma_release_channel(s_chan);
		s_chan = NULL;
	}
}
