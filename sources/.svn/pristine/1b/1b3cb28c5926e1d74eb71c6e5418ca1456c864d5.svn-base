/*
 *	
 *  Multiple DMA transmits
 *
 *  Created on: May 12, 2014
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *	New source file for hess driver interrupt handler
 *  Implementation in this file uses DMA engine
 *  for copying information from device to memory
 *
 */

#if 1

#define	NUMBER_OF_CHANNELS_IN_USE	1

#include <linux/dma-mapping.h>
#include <linux/cdev.h>
#include <asm/dma.h>
#include <linux/dmaengine.h>
#include <linux/interrupt.h>
#include <linux/dmapool.h>
#include <linux/kthread.h>

#include "hessirqhandlers.h"
#include "tools.h"

#ifndef COMPLETE_READ
#error  Scheme with multiple transaction can be compiled if COMPLETE_READ defined!
#endif

/*
 * This function body one can find in file "at_dma_handler.c"
 */
struct dma_async_tx_descriptor *
	atc_prep_dma_memcpyNew(struct dma_chan *chan, int a_nDstSize, dma_addr_t a_dests, dma_addr_t src,
	size_t len, unsigned long flags);

// PIN Definitions
#define TEST_PIN_DMA		AT91_PIN_PB20	// Test Pin on Test Connector, may have spikes if taskit kernel is used (pin used for r/w led?!)
#define TEST_PIN_IRQ		AT91_PIN_PB21	// Test Pin on Test Connector
#define TEST_PIN_3			AT91_PIN_PB22	// Test Pin on Test Connector
#define TEST_PIN_4			AT91_PIN_PB23	// Test Pin on Test Connector

int g_nPossibleDMA = 1;
module_param_named(dma, g_nPossibleDMA, int, S_IRUGO | S_IWUSR);

static int s_nDebugOn = 1;
module_param_named(debug, s_nDebugOn, int, S_IRUGO | S_IWUSR);

// Global Instance of the Control Bus Device
hess_dev_t g_my_device_instance;

IRQDevice_t g_srqDevice;


static struct dma_chan* s_vChannelsB[NUMBER_OF_CHANNELS_IN_USE];
static struct at_dma_slave* s_vAtSlaves[NUMBER_OF_CHANNELS_IN_USE];
dma_addr_t g_dmaDestAddr0 = 0;

//static const dma_addr_t t_dmaSrc = SMC_MEM_START + BASE_NECTAR_ADC + OFFS_EVENT_FIFO;
static const dma_addr_t t_dmaSrc = SMC_MEM_START + BASE_NECTAR_ADC + OFFS_EVENT_FIFO_READ_STROBE;
static const enum dma_ctrl_flags t_flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT | DMA_COMPL_SKIP_DEST_UNMAP | DMA_COMPL_SKIP_SRC_UNMAP;

//static const size_t s_cunDMAsize = HESS_SAMPLE_DATA_SIZE * 2;
//static const size_t s_cunDMAsize = HESS_SAMPLE_DATA_SIZE;
//static const size_t s_cunDMAsize = sizeof(hess_sample_data_named_t) + sizeof(uint16_t);
static const size_t s_cunDMAsize = sizeof(hess_sample_data_named_t);
static int s_event_length = 0;
static uint16_t s_packet_length = 0;
static uint16_t s_words = 0;
static uint16_t s_numberOfEventsInFifo = 0;
extern struct timespec g_time_old__hess;
static hess_sample_data_debug_t *s_last_pointer = NULL;

static int s_nTransmitInProgress = 0;
static uint32_t s_event_number_old = 0;


dma_cookie_t t_cookies[NUMBER_OF_CHANNELS_IN_USE];


static inline void dmaengine_callback(void *a_pDev)
{

	IRQDevice_t* dev = a_pDev;
#if 1
	if (s_nDebugOn)
	{
		uint16_t* pType = (uint16_t*)a_pDev;
		uint16_t type = pType[0];
		uint32_t event_numberNew;
		hess_sample_data_debug_t *pointerToRead = dev->hess_data->write_pointer;

		for (pointerToRead = dev->hess_data->write_pointer; pointerToRead != s_last_pointer; ++pointerToRead)
		{
			dev->debug_data.type = type = pointerToRead->as_raw_u16[0];
			if (((type & 0xfc00) == DATATYPE_SAMPLE_CONTROL) || ((type & 0xfc00) == DATATYPE_TESTDATA_FRONTENDFIFOCONTROL))
			{
				// control data
				if (s_event_length == 15) { dev->debug_data.frame_15++; }
				else if (s_event_length == 16) { dev->debug_data.frame_16++; }
				else if (s_event_length == 17) { dev->debug_data.frame_17++; }
				else if (s_event_length == 32) { dev->debug_data.frame_32++; }
				else  { dev->debug_data.frame_other++; }
				dev->debug_data.frame_length = s_event_length;

				if (s_event_length != s_packet_length){ dev->debug_data.length_mismatch++; }

				s_packet_length = pointerToRead->as_control.packet_length - 1;
				s_event_length = 0;

				event_numberNew = (pointerToRead->as_control.event_counter_h << 16) + pointerToRead->as_control.event_counter_l;
				g_srqDevice.debug_data.eventcounter = event_numberNew;
				if ((uint32_t)(s_event_number_old + 1) != event_numberNew)
				{
					dev->debug_data.eventcounter_mismatch_old = s_event_number_old;
					dev->debug_data.eventcounter_mismatch_new = event_numberNew;
					dev->debug_data.eventcounter_mismatch++;
				}
				s_event_number_old = event_numberNew;
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

		} // for (pointerToRead = dev->hess_data->write_pointer; pointerToRead != s_last_pointer; ++pointerToRead)

		//dev->debug_data.number_of_stat = i;

	} // if (s_nDebugOn)

#endif

	s_nTransmitInProgress = 0;
	setTestPin(TEST_PIN_DMA, CLR);
	smc_bus_write16(OFFS_ISR_HANDSHAKE, bitValue16(BIT_ISR_HANDSHAKE_STOP));

	wake_up(&dev->waitQueue);

	if (likely(1))
	{
		struct timespec time_new;
		getnstimeofday(&time_new);
		__kernel_time_t sec = time_new.tv_sec - g_time_old__hess.tv_sec;
		long diff_us = (1000000 * sec) + (time_new.tv_nsec - g_time_old__hess.tv_nsec) / 1000;
		//uint64_t timeDiffMult1000000;

		dev->debug_data.diff_us = diff_us;
		if (s_numberOfEventsInFifo > 0) { dev->debug_data.triggerRate = diff_us / s_numberOfEventsInFifo; }
		else { dev->debug_data.triggerRate = 0; }

		g_time_old__hess = time_new;
	}
}



irqreturn_t IrqHandlerDma(int irq_number, void *p_dev)
{
	// todo: ringbufferverwaltung auslagern -> inline

	struct dma_async_tx_descriptor *tx;
	IRQDevice_t* dev = (IRQDevice_t*)p_dev;
	dma_addr_t dmaDst;
	hess_sample_data_debug_t *temp_write_pointer;
	hess_sample_data_debug_t *temp_last_pointer;

	setTestPin(TEST_PIN_IRQ, SET);

	g_srqDevice.debug_data.total_irq_count++;

	if (s_nTransmitInProgress)
	{
		++(g_srqDevice.debug_data.notCompleted);
		goto out;
	}

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
	
	temp_write_pointer = s_last_pointer ? s_last_pointer : dev->hess_data->write_pointer;
	temp_last_pointer = temp_write_pointer + s_words;
	if (temp_last_pointer >= dev->hess_data->end)
	{
		temp_write_pointer = dev->hess_data->begin;
		s_words = s_words > HESS_RING_BUFFER_COUNT ? HESS_RING_BUFFER_COUNT : s_words;
		s_last_pointer = temp_write_pointer + s_words;
	}
	else
	{
		s_last_pointer = temp_last_pointer;
	}
	dev->hess_data->write_pointer = temp_write_pointer;


	//dmaDst = g_dmaDestAddr0 + (dma_addr_t)((char*)(&(g_srqDevice.hess_data->write_pointer->as_raw_u16[0])) - (char*)g_srqDevice.hess_data);
	dmaDst = g_dmaDestAddr0 + (dma_addr_t)((char*)(&(g_srqDevice.hess_data->write_pointer->as_raw_u16[0])) - (char*)(&(g_srqDevice.hess_data->begin->as_raw_u16[0])));

	setTestPin(TEST_PIN_DMA, SET);
	tx = atc_prep_dma_memcpyNew(s_vChannelsB[0], s_words, dmaDst, t_dmaSrc, s_cunDMAsize, t_flags);
	//tx = s_vChannelsB[0]->device->device_prep_dma_memcpy(s_vChannelsB[0], dmaDst, t_dmaSrc, s_cunDMAsize, t_flags);
	if (unlikely(!tx))
	{
		ERR_IRQ("Preparing DMA error!\n");
		goto out;
	}
	tx->callback = dmaengine_callback;
	//tx->callback_param = g_srqDevice.hess_data->write_pointer->as_raw_u16;//IRQDevice_t
	tx->callback_param = &g_srqDevice;

	t_cookies[0] = dmaengine_submit(tx);
	if (dma_submit_error(t_cookies[0]))
	{
		//// Error handling
		ERR_IRQ("DMA submit error!\n");
		goto out;
	}

	dma_async_issue_pending(s_vChannelsB[0]);

out:

	setTestPin(TEST_PIN_IRQ, CLR);
	return IRQ_HANDLED;
}



irqreturn_t IrqHandlerNoDma(int irq_number, void *p_dev)
{
	// todo: ringbufferverwaltung auslagern -> inline

	IRQDevice_t* dev = (IRQDevice_t*)p_dev;
	uint16_t words = 0;
	uint16_t* data_pionter = NULL;
	uint16_t type;
	int k = 0;
	static int event_length = 0;
	size_t fifo_offset;

	static uint32_t event_number_old = 0;
	static uint32_t event_number_new = 0;

	static uint16_t packet_length = 0;

	hess_sample_data_debug_t *temp_write_pointer;

	setTestPin(TEST_PIN_IRQ, SET);
	smc_bus_write16(OFFS_ISR_HANDSHAKE, bitValue16(BIT_ISR_HANDSHAKE_START));

	//write_lock(&dev->accessLock);

	dev->debug_data.total_irq_count++;

	s_numberOfEventsInFifo = smc_bus_read16(BASE_NECTAR_ADC + OFFS_EVENTS_PER_IRQ); // ## kommt spaeter aus dem fifo durch parsen ?!
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
		//smc_bus_read16(BASE_NECTAR_ADC + OFFS_EVENT_FIFO_READ_STROBE);
		data_pionter = &(dev->hess_data->write_pointer->as_raw_u16[0]);

		//fifo_offset = BASE_NECTAR_ADC + OFFS_EVENT_FIFO;
		fifo_offset = BASE_NECTAR_ADC + OFFS_EVENT_FIFO_READ_STROBE;
		//*(data_pionter++) = smc_bus_read16(fifo_offset-2);
		for (k = 0; k < (HESS_SAMPLE_DATA_SIZE / 2); k++) // ## ist die frage nach was man sich richten sollte... sizeof(structs)?, offsets?, noch mehr defines?
		{
			*(data_pionter++) = smc_bus_read16(fifo_offset); // ## unrolling bringt hier was...
			fifo_offset += 2;
		}

		words--;

		//type = dev->hess_data->write_pointer->as_raw_u16[0];
		dev->debug_data.type = type = dev->hess_data->write_pointer->as_raw_u16[1];

		if (s_nDebugOn)
		{

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
				dev->debug_data.unknown_type++;
			}
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
		struct timespec time_new;
		getnstimeofday(&time_new);
		__kernel_time_t sec = time_new.tv_sec - g_time_old__hess.tv_sec;
		long diff_us = (1000000 * sec) + (time_new.tv_nsec - g_time_old__hess.tv_nsec) / 1000;
		//uint64_t timeDiffMult1000000;

		dev->debug_data.diff_us = diff_us;
		if (s_numberOfEventsInFifo > 0) { dev->debug_data.triggerRate = diff_us / s_numberOfEventsInFifo; }
		else { dev->debug_data.triggerRate = 0; }

		g_time_old__hess = time_new;
	}

	smc_bus_write16(OFFS_ISR_HANDSHAKE, bitValue16(BIT_ISR_HANDSHAKE_STOP));
	setTestPin(TEST_PIN_IRQ, CLR);

	return IRQ_HANDLED;
}



void FirstDMAInitGlb(void)
{
	memset(s_vChannelsB, 0, sizeof(struct dma_chan*)*NUMBER_OF_CHANNELS_IN_USE);
	memset(s_vAtSlaves, 0, sizeof(struct at_dma_slave*)*NUMBER_OF_CHANNELS_IN_USE);
	memset(t_cookies, 0, sizeof(dma_cookie_t)*NUMBER_OF_CHANNELS_IN_USE);
}



struct device * GetDMAdevice(void)
{ 
	return s_vChannelsB[0]->device->dev; 
}


static bool filter(struct dma_chan *chan, void *param)
{
	static int s_nIteration = 0;
	int nParam = (int)param;

	DEBUG1("param = %p, nParam = %d, s_nIteration = %d\n", param, nParam, ++s_nIteration);

	switch (nParam)
	{
	case 0:
		return dma_has_cap(DMA_SLAVE, chan->device->cap_mask) && dma_has_cap(DMA_PRIVATE, chan->device->cap_mask) ? true : false;
	default:
		return false;
	}
}



int PrepareDmaChannelsGlb(void)
{
	dma_cap_mask_t mask;
	struct at_dma_slave* pAtSlave = NULL;
	int i = 0;
	
	if (unlikely(!g_nPossibleDMA)) return 0;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);
	dma_cap_set(DMA_PRIVATE, mask);

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

		pAtSlave = (struct at_dma_slave*)(s_vChannelsB[i]->private);
		pAtSlave->dma_dev = s_vChannelsB[i]->device->dev;
		pAtSlave->rx_reg = t_dmaSrc;
		pAtSlave->reg_width = AT_DMA_SLAVE_WIDTH_16BIT;
		pAtSlave->cfg = ATC_SRC_REP | ATC_DST_REP;
		pAtSlave->ctrla = ATC_SCSIZE_4 | ATC_DCSIZE_4;

	} // for (; i < NUMBER_OF_CHANNELS_IN_USE; ++i)

	return 0;
}



void StopDMAchannelActivity(void)
{
	int i = 0;

	for (; i < NUMBER_OF_CHANNELS_IN_USE; ++i)
	{
		if (s_vChannelsB[i]){dmaengine_terminate_all(s_vChannelsB[i]);}
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



void DumpAllStupidWarnings_hessirqhandlers(void)
{
	copy_from_io_to_user_16bit(NULL, NULL, 0);
	copy_from_user_to_io_16bit(NULL, NULL, 0);
}

#endif
