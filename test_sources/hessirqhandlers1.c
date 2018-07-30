#define NUMBER_OF_CHANNELS_IN_USE	1

#include "hessirqhandlers.h"

#include <linux/dma-mapping.h>
#include <linux/cdev.h>
#include <asm/dma.h>
#include <linux/dmaengine.h>
#include <linux/interrupt.h>
#include <linux/dmapool.h>

int g_nPossibleDMA = 1;
module_param_named(dma, g_nPossibleDMA, int, S_IRUGO | S_IWUSR);

// Global Instance of the Control Bus Device
hess_dev_t g_my_device_instance;

IRQDevice_t g_srqDevice;


//struct dma_chan *g_chanA0 = NULL;
//struct dma_chan *g_chanA1 = NULL;
static struct dma_chan* s_vChannelsB[NUMBER_OF_CHANNELS_IN_USE];
//struct at_dma_slave* g_pAtSlave0 = NULL;
//struct at_dma_slave* g_pAtSlave1 = NULL;
static struct at_dma_slave* s_vAtSlaves[NUMBER_OF_CHANNELS_IN_USE];
dma_addr_t g_dmaDestAddr = 0;

//static int	s_vnIndexes[NUMBER_OF_CHANNELS_IN_USE];


//static struct dma_async_tx_descriptor *t_txIRQ = NULL;
//static struct dma_async_tx_descriptor *t_tx0 = NULL;
//static struct dma_async_tx_descriptor *t_tx1 = NULL;
static const dma_addr_t t_dmaSrc = SMC_MEM_START + BASE_NECTAR_ADC + OFFS_EVENT_FIFO;
static const enum dma_ctrl_flags t_flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT | DMA_COMPL_SKIP_DEST_UNMAP | DMA_COMPL_SKIP_SRC_UNMAP;

static const size_t s_cunDMAsize = HESS_SAMPLE_DATA_SIZE * 2;
static int s_event_length = 0;
static uint16_t s_packet_length = 0;
static uint32_t s_event_number_new = 0;
static uint32_t s_event_number_old = 0;
static uint16_t s_words = 0;
static uint16_t s_numberOfEventsInFifo = 0;
static struct timespec s_time_old;
static struct timespec s_time_new;


static int s_nTransmitInProgress = 0;
//static uint64_t	s_nTavNanoMult1000000 = 0; // Keep time ~10>-15
static int32_t	s_nIterations = 0; // Keep time ~10>-15


static struct work_struct			s_DMAengine_work;
static struct workqueue_struct*		s_DMAengine_IRQworkqueue = NULL;

dma_cookie_t t_cookies[NUMBER_OF_CHANNELS_IN_USE];

static struct tasklet_struct* s_chanTasklets[NUMBER_OF_CHANNELS_IN_USE];


#if 1
#define CALCULATE_TIME(a_time_new,a_dev) \
{ \
	static struct timespec current_time; \
	int32_t timeDiffMult; \
	getnstimeofday(&current_time); \
	timeDiffMult =	1000000000 * ((int32_t)(current_time.tv_sec - a_time_new.tv_sec)) + \
							((int32_t)(current_time.tv_nsec - a_time_new.tv_nsec)); \
	(a_dev)->debug_data.m_nTavNanoSecs += (timeDiffMult - (a_dev)->debug_data.m_nTavNanoSecs) / (++s_nIterations);\
}
#else
#include <asm/div64.h>
//dev->debug_data.m_nTavNanoMult1000000 = dev->debug_data.m_nTavNanoMult1000000 + (timeDiffMult1000000 - dev->debug_data.m_nTavNanoMult1000000) / (s_nIterations + 1);
//dev->debug_data.m_nTavNanoMult1000000 += (timeDiffMult1000000 - dev->debug_data.m_nTavNanoMult1000000) / (++s_nIterations);
#define CALCULATE_TIME(a_time_new,a_dev) \
{ \
	static struct timespec current_time; \
	int32_t timeDiffMult1000000; \
	getnstimeofday(&current_time); \
	timeDiffMult1000000 =	1000000000LU * ((int32_t)(current_time.tv_sec - a_time_new.tv_sec)) + \
							((int32_t)(current_time.tv_nsec - a_time_new.tv_nsec)); \
	timeDiffMult1000000 -= a_dev->debug_data.m_nTavNanoSecs; \
	/*do_div(timeDiffMult1000000,++s_nIterations);*/ \
	timeDiffMult1000000 /= ++s_nIterations; \
	a_dev->debug_data.m_nTavNanoSecs += timeDiffMult1000000; \
}
#endif

//spinlock_t s_lock;
//int s_bLocked = 0;

//static inline void dmatest_callback_inline(void *a_pDev, struct dma_chan *a_pChan, dma_async_tx_callback a_callback)
//static inline void dmaengine_callback(void *a_pDev)
void dmaengine_callback(void *a_pDev)
{
	//printk(KERN_ALERT "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!dmatest_callback_inline!\n");
	uint16_t* pType = (uint16_t*)a_pDev;
	uint16_t type = pType[0];

	//spin_lock(&s_lock);
	//s_bLocked = 1;

	if (((type & 0xfc00) == DATATYPE_SAMPLE_CONTROL) || ((type & 0xfc00) == DATATYPE_TESTDATA_FRONTENDFIFOCONTROL))
	{
		// control data
		if (s_event_length == 15) { g_srqDevice.debug_data.frame_15++; }
		else if (s_event_length == 16) { g_srqDevice.debug_data.frame_16++; }
		else if (s_event_length == 17) { g_srqDevice.debug_data.frame_17++; }
		else if (s_event_length == 32) { g_srqDevice.debug_data.frame_32++; }
		else  { g_srqDevice.debug_data.frame_other++; }
		g_srqDevice.debug_data.frame_length = s_event_length;

		if (s_event_length != s_packet_length){ g_srqDevice.debug_data.length_mismatch++; }

		s_packet_length = g_srqDevice.hess_data->write_pointer->as_control.packet_length - 1;
		s_event_length = 0;

		s_event_number_new = (g_srqDevice.hess_data->write_pointer->as_control.event_counter_h << 16) + g_srqDevice.hess_data->write_pointer->as_control.event_counter_l;
		g_srqDevice.debug_data.eventcounter = s_event_number_new;
		if ((uint32_t)(s_event_number_old + 1) != s_event_number_new)
		{
			g_srqDevice.debug_data.eventcounter_mismatch_old = s_event_number_old;
			g_srqDevice.debug_data.eventcounter_mismatch_new = s_event_number_new;
			g_srqDevice.debug_data.eventcounter_mismatch++;
		}
		s_event_number_old = s_event_number_new;
	}
	else if (((type & 0xfc00) == DATATYPE_SAMPLE_WAVEFORM) || ((type & 0xfc00) == DATATYPE_TESTDATA_FRONTENDFIFOWAVEFORM))
	{
		// sample data
		s_event_length++;
	}
	else
	{
		//g_srqDevice.debug_data.unknown_type++;
	}

	s_nTransmitInProgress = 2;
	queue_work(s_DMAengine_IRQworkqueue, &s_DMAengine_work);

}



static const char* s_dma_statuses[] = {
	"DMA_SUCCESS",
	"DMA_IN_PROGRESS",
	"DMA_PAUSED",
	"DMA_ERROR"
};




#if LINUX_VERSION_CODE < 132632 
static void DMAengine_do_work(void *pciegendev)
#else
static void DMAengine_do_work(struct work_struct *work_str)
#endif 
{
#if LINUX_VERSION_CODE < 132632
	//struct str_pcie_gen *dev = (struct str_pcie_gen*)pciegendev;
#else
	//struct str_pcie_gen *dev = container_of(work_str, struct str_pcie_gen, pcie_gen_work);
#endif

#if 1
	//static int s_snFirst = 0;

	static int snChannelIndex = 0;
	struct dma_async_tx_descriptor *tx;
	hess_sample_data_debug_t *temp_write_pointer;
	struct dma_chan* pChan = s_vChannelsB[snChannelIndex];
	//dma_cookie_t cookie = t_cookies[snChannelIndex];
	dma_addr_t dmaDst;
	enum dma_status		status = DMA_SUCCESS;

	if (likely(t_cookies[snChannelIndex]))
	{
		//dmaengine_terminate_all(pChan);
		status = dma_async_is_tx_complete(pChan, t_cookies[snChannelIndex], NULL, NULL);
	}

	if (status != DMA_SUCCESS)
	{
		printk(KERN_ALERT "+++++++++++++++status = %s\n", s_dma_statuses[status]);
		ERR("!!!!!!!!!!!!!!!!! status != DMA_SUCCESS\n");
		goto out;
	}

	/*if (s_snFirst++ >= NUMBER_OF_CHANNELS_IN_USE)
	{
	s_snFirst = 0;
	goto out;
	}*/

	//spin_lock_init(&s_lock);
	//if(s_bLocked)spin_lock(&s_lock);

	//goto out;
	//printk(KERN_ALERT "snChannelIndex = %d\n", snChannelIndex);

	if (s_nTransmitInProgress == 2)
	{
		temp_write_pointer = g_srqDevice.hess_data->write_pointer;
		temp_write_pointer++;
		if (temp_write_pointer == g_srqDevice.hess_data->end)
		{
			temp_write_pointer = g_srqDevice.hess_data->begin;
		}
		g_srqDevice.hess_data->write_pointer = temp_write_pointer;

		--s_words;

		if (s_words == 0){ goto out; }
	}

	//tasklet_unlock_wait(s_chanTasklets[snChannelIndex]);
	//smp_mb();
	tasklet_disable(s_chanTasklets[snChannelIndex]);

	dmaDst = g_dmaDestAddr + (dma_addr_t)((char*)(&(g_srqDevice.hess_data->write_pointer->as_raw_u16[0])) - (char*)g_srqDevice.hess_data);

	smc_bus_read16(BASE_NECTAR_ADC + OFFS_EVENT_FIFO_READ_STROBE);

	tx = pChan->device->device_prep_dma_memcpy(pChan, dmaDst, t_dmaSrc, s_cunDMAsize, t_flags);
	if (unlikely(!tx))
	{
		ERR("Preparing DMA error!\n");
		return;
	}
	tx->callback = dmaengine_callback;
	//tx->callback = NULL;
	tx->callback_param = g_srqDevice.hess_data->write_pointer->as_raw_u16;

	t_cookies[snChannelIndex] = dmaengine_submit(tx);
	if (dma_submit_error(t_cookies[snChannelIndex]))
	{
		//// Error handling
		ERR("DMA submit error!\n");
		return;
	}

	dma_async_issue_pending(pChan);

	tasklet_enable(s_chanTasklets[snChannelIndex]);

	snChannelIndex = (snChannelIndex + 1) % NUMBER_OF_CHANNELS_IN_USE;

	return;

#else

	printk(KERN_ALERT "++++++++++++++++++++++\n");
	smc_bus_write16(OFFS_ISR_HANDSHAKE, bitValue16(BIT_ISR_HANDSHAKE_STOP));
	return;

	goto out;

#endif

out:

	wake_up(&g_srqDevice.waitQueue);

	if (likely(1))
	{
		__kernel_time_t sec = s_time_new.tv_sec - s_time_old.tv_sec;
		long diff_us = (1000000 * sec) + (s_time_new.tv_nsec - s_time_old.tv_nsec) / 1000;

		CALCULATE_TIME(s_time_new, &g_srqDevice);
		//g_srqDevice.debug_data.m_nTavNanoMult1000000 += (timeDiffMult1000000 - dev->debug_data.m_nTavNanoMult1000000) / (++s_nIterations);

		//if (s_nIterations % 10 == 0){printk(KERN_ALERT "!!!!!!!!!!!!!!!!!! dmatest_callback_inline!!!\n");}

		g_srqDevice.debug_data.diff_us = diff_us;
		if (s_numberOfEventsInFifo > 0) { g_srqDevice.debug_data.triggerRate = diff_us / s_numberOfEventsInFifo; }
		else { g_srqDevice.debug_data.triggerRate = 0; }

		s_time_old = s_time_new;
	}

	s_nTransmitInProgress = 0;
	smc_bus_write16(OFFS_ISR_HANDSHAKE, bitValue16(BIT_ISR_HANDSHAKE_STOP));

}



irqreturn_t IrqHandlerDma(int irq_number, void *p_dev)
{
	// todo: ringbufferverwaltung auslagern -> inline
#if 1
	static uint16_t first = 1;

	if (unlikely(first))
	{
		getnstimeofday(&s_time_old);
		first = 0;
		goto out;
	}

	if (s_nTransmitInProgress)
	{
		++(g_srqDevice.debug_data.notCompleted);
		goto out;
	}

	g_srqDevice.debug_data.total_irq_count++;
	getnstimeofday(&s_time_new);

	smc_bus_write16(OFFS_ISR_HANDSHAKE, bitValue16(BIT_ISR_HANDSHAKE_START));

	s_numberOfEventsInFifo = smc_bus_read16(BASE_NECTAR_ADC + OFFS_EVENTS_PER_IRQ); // ## kommt spaeter aus dem fifo durch parsen ?!
	s_words = smc_bus_read16(BASE_NECTAR_ADC + OFFS_EVENT_FIFO_WORD_COUNT);

	if ((s_words == 0xffff) || (s_words == 0x0))
	{
		g_srqDevice.debug_data.fifo_empty++;
		goto out;
	}
	g_srqDevice.debug_data.fifo_count = s_words;
	if (s_words > g_srqDevice.debug_data.max_fifo_count)
	{
		g_srqDevice.debug_data.max_fifo_count = s_words;
	}

	s_nTransmitInProgress = 1;
	queue_work(s_DMAengine_IRQworkqueue, &s_DMAengine_work);

out:

#else
	smc_bus_write16(OFFS_ISR_HANDSHAKE, bitValue16(BIT_ISR_HANDSHAKE_START));
	queue_work(s_DMAengine_IRQworkqueue, &s_DMAengine_work);
#endif

	return IRQ_HANDLED;
}


irqreturn_t IrqHandlerNoDma(int irq_number, void *p_dev)
{
	// todo: ringbufferverwaltung auslagern -> inline

	static uint16_t first = 1;

	IRQDevice_t* dev = (IRQDevice_t*)p_dev;
	uint16_t words = 0;
	uint16_t numberOfEventsInFifo = 0;
	uint16_t* data_pionter = NULL;
	uint16_t type;
	int k = 0;
	static int event_length = 0;
	size_t fifo_offset;

	static struct timespec time_old;
	static struct timespec time_new;
	static uint32_t event_number_old = 0;
	static uint32_t event_number_new = 0;

	static uint16_t packet_length = 0;

	hess_sample_data_debug_t *temp_write_pointer;

	//	setTestPin(TEST_PIN_1,SET);
	smc_bus_write16(OFFS_ISR_HANDSHAKE, bitValue16(BIT_ISR_HANDSHAKE_START));

	//write_lock(&dev->accessLock);

	dev->debug_data.total_irq_count++;
	getnstimeofday(&time_new);

	if (first == 1)
	{
		getnstimeofday(&time_old);
		first = 0;
	}


	numberOfEventsInFifo = smc_bus_read16(BASE_NECTAR_ADC + OFFS_EVENTS_PER_IRQ); // ## kommt spaeter aus dem fifo durch parsen ?!
	words = smc_bus_read16(BASE_NECTAR_ADC + OFFS_EVENT_FIFO_WORD_COUNT);
	//words = smc_bus_read16(BASE_NECTAR_ADC + OFFS_EVENT_FIFO_WORD_COUNT);

	if ((words == 0xffff) || (words == 0x0))
	{
		dev->debug_data.fifo_empty++;
		goto out;
	}
	dev->debug_data.fifo_count = words;
	if (words > dev->debug_data.max_fifo_count)
	{
		dev->debug_data.max_fifo_count = words;
	}


	while (words > 0)
	{
		smc_bus_read16(BASE_NECTAR_ADC + OFFS_EVENT_FIFO_READ_STROBE);
		data_pionter = &(dev->hess_data->write_pointer->as_raw_u16[0]);

		fifo_offset = BASE_NECTAR_ADC + OFFS_EVENT_FIFO;
		for (k = 0; k < (HESS_SAMPLE_DATA_SIZE / 2); k++) // ## ist die frage nach was man sich richten sollte... sizeof(structs)?, offsets?, noch mehr defines?
		{
			*(data_pionter++) = smc_bus_read16(fifo_offset); // ## unrolling bringt hier was...
			fifo_offset += 2;
		}

		words--;

		type = dev->hess_data->write_pointer->as_raw_u16[0];

		if (((type & 0xfc00) == DATATYPE_SAMPLE_CONTROL) || ((type & 0xfc00) == DATATYPE_TESTDATA_FRONTENDFIFOCONTROL))
		{
			// control data
			if (event_length == 15) { dev->debug_data.frame_15++; }
			else if (event_length == 16) { dev->debug_data.frame_16++; }
			else if (event_length == 17) { dev->debug_data.frame_17++; }
			else if (event_length == 32) { dev->debug_data.frame_32++; }
			else  { dev->debug_data.frame_other++; }
			dev->debug_data.frame_length = event_length;

			if (event_length != packet_length){ dev->debug_data.length_mismatch++; }

			packet_length = dev->hess_data->write_pointer->as_control.packet_length - 1;
			event_length = 0;

			event_number_new = (dev->hess_data->write_pointer->as_control.event_counter_h << 16) + dev->hess_data->write_pointer->as_control.event_counter_l;
			dev->debug_data.eventcounter = event_number_new;
			if ((uint32_t)(event_number_old + 1) != event_number_new)
			{
				dev->debug_data.eventcounter_mismatch_old = event_number_old;
				dev->debug_data.eventcounter_mismatch_new = event_number_new;
				dev->debug_data.eventcounter_mismatch++;
			}
			event_number_old = event_number_new;
		}
		else if (((type & 0xfc00) == DATATYPE_SAMPLE_WAVEFORM) || ((type & 0xfc00) == DATATYPE_TESTDATA_FRONTENDFIFOWAVEFORM))
		{
			// sample data
			event_length++;
		}
		else
		{
			//dev->debug_data.unknown_type++;
		}

		temp_write_pointer = dev->hess_data->write_pointer;
		temp_write_pointer++;
		if (temp_write_pointer == dev->hess_data->end)
		{
			temp_write_pointer = dev->hess_data->begin;
		}
		dev->hess_data->write_pointer = temp_write_pointer;

		if (words == 0) { goto out; }

	} // end while (words > 0)

out:
	//write_unlock(&dev->accessLock);
	wake_up(&dev->waitQueue);

	if (likely(1))
	{
		//static struct timespec current_time;
		__kernel_time_t sec = time_new.tv_sec - time_old.tv_sec;
		long diff_us = (1000000 * sec) + (time_new.tv_nsec - time_old.tv_nsec) / 1000;
		//uint64_t timeDiffMult1000000;

		//getnstimeofday(&current_time);
		//timeDiffMult1000000 = 1000000000000000LLU * ((uint64_t)(current_time.tv_sec - s_time_new.tv_sec)) + 1000000LLU * ((uint64_t)(current_time.tv_nsec - s_time_new.tv_nsec));
		//dev->debug_data.m_nTavNanoMult1000000 += (timeDiffMult1000000 - dev->debug_data.m_nTavNanoMult1000000) / (++s_nIterations);

		CALCULATE_TIME(time_new, dev);

		dev->debug_data.diff_us = diff_us;
		if (numberOfEventsInFifo > 0) { dev->debug_data.triggerRate = diff_us / numberOfEventsInFifo; }
		else { dev->debug_data.triggerRate = 0; }

		time_old = time_new;
	}

	smc_bus_write16(OFFS_ISR_HANDSHAKE, bitValue16(BIT_ISR_HANDSHAKE_STOP));
	return IRQ_HANDLED;
}




void FirstDMAInitGlb(void)
{
	//static struct dma_chan* s_vChannelsB[NUMBER_OF_CHANNELS_IN_USE];
	////////struct at_dma_slave* g_pAtSlave0 = NULL;
	////////struct at_dma_slave* g_pAtSlave1 = NULL;
	//static struct at_dma_slave* s_vAtSlaves[NUMBER_OF_CHANNELS_IN_USE];

	memset(s_vChannelsB, 0, sizeof(struct dma_chan*)*NUMBER_OF_CHANNELS_IN_USE);
	memset(s_vAtSlaves, 0, sizeof(struct at_dma_slave*)*NUMBER_OF_CHANNELS_IN_USE);
	memset(t_cookies, 0, sizeof(dma_cookie_t)*NUMBER_OF_CHANNELS_IN_USE);
}



struct device * GetDMAdevice(void){ return s_vChannelsB[0]->device->dev; }




static bool filter(struct dma_chan *chan, void *param)
{
	static int s_nIteration = 0;
	int nParam = (int)param;

	printk(KERN_ALERT "param = %p, nParam = %d, s_nIteration = %d\n", param, nParam, s_nIteration);

	//if (s_nChannelNumber != s_nIteration++)
	//	return false;

	switch (nParam)
	{
	case 0:
		return dma_has_cap(DMA_SLAVE, chan->device->cap_mask) && dma_has_cap(DMA_PRIVATE, chan->device->cap_mask) ? true : false;
	default:
		return false;
	}

}


#include <../drivers/dma/at_hdmac_regs.h>

int PrepareDmaChannelsGlb(void)
{
	dma_cap_mask_t mask;
	int i = 0;

	struct at_dma_chan	*atchan;
	//spinlock_t lock;


	if (unlikely(!g_nPossibleDMA)) return 0;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);
	dma_cap_set(DMA_PRIVATE, mask);

	s_DMAengine_IRQworkqueue = create_workqueue("hessdrv");

#if LINUX_VERSION_CODE < 132632 
	INIT_WORK(&s_DMAengine_work, DMAengine_do_work, NULL);
#else
	INIT_WORK(&s_DMAengine_work, DMAengine_do_work);
#endif


	for (; i < NUMBER_OF_CHANNELS_IN_USE; ++i)
	{

		if (likely(!s_vChannelsB[i]))
		{
			s_vChannelsB[i] = dma_request_channel(mask, filter, NULL);
			if (!s_vChannelsB[i])
			{
				ERR("No channel available!\n");
				RemoveDmaChannelsGlb();
				g_nPossibleDMA = 0;
				return -1;
			}
		}// if (likely(!s_vChannelsB[i]))

		if (!s_vChannelsB[i]->private)
		{
			if (likely(!s_vAtSlaves[i]))
			{
				s_vAtSlaves[i] = kzalloc(sizeof(struct at_dma_slave), GFP_KERNEL);
				if (!s_vAtSlaves[i])
				{
					ERR("No memory!\n");
					RemoveDmaChannelsGlb();
					g_nPossibleDMA = 0;
					return -2;
				}
			} // if (likely(!s_vAtSlaves[i]))

			s_vChannelsB[i]->private = s_vAtSlaves[i];

		} // if (!s_vChannelsB[i]->private)

		((struct at_dma_slave*)(s_vChannelsB[i]->private))->rx_reg = t_dmaSrc;

		atchan = to_at_dma_chan(s_vChannelsB[i]);
		s_chanTasklets[i] = &atchan->tasklet;

	} // for (; i < NUMBER_OF_CHANNELS_IN_USE; ++i)

	//atchan = to_at_dma_chan(s_vChannelsB[0]);
	//printk(KERN_ALERT "++++++ locking spinlock\n");
	//spin_lock(&atchan->lock);
	//spin_lock(&atchan->lock);
	//spin_lock(&atchan->lock);


	return 0;
}



void StopDMAchannelActivity(void)
{

	int i = 0;

	for (; i < NUMBER_OF_CHANNELS_IN_USE; ++i)
	{
		if (s_vChannelsB[i])
		{
			dmaengine_terminate_all(s_vChannelsB[i]);
		}
	}

	if (s_DMAengine_IRQworkqueue)
	{
		flush_workqueue(s_DMAengine_IRQworkqueue);
		destroy_workqueue(s_DMAengine_IRQworkqueue);
		s_DMAengine_IRQworkqueue = NULL;
	}

}



void RemoveDmaChannelsGlb(void)
{

	int i = 0;

	for (; i < NUMBER_OF_CHANNELS_IN_USE; ++i)
	{
		if (s_vChannelsB[i] && ((char*)s_vChannelsB[i]->private == (char*)s_vAtSlaves[i]))
		{
			s_vChannelsB[i]->private = NULL;
		}

		if (s_vAtSlaves[i])
		{
			kfree(s_vAtSlaves[i]);
			s_vAtSlaves[i] = NULL;
		}

		if (s_vChannelsB[i])
		{
			dma_release_channel(s_vChannelsB[i]);
			s_vChannelsB[i] = NULL;
		}
	}
}
