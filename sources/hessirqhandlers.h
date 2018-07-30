/*
*	File: hess_drv_exp.h
*
*	Created on: Jan 02, 2015
*	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
*
*
*
*/
#ifndef __hessirqhandlers_h__
#define __hessirqhandlers_h__

#define SMC_MEM_START		AT91_CHIPSELECT_2 //0x30000000	// physical address region of the controlbus bus mapping *is fixed!*

#include "hess_drv_exp.h"

#include "hess1u/common/hal/types.h"
#include "hess1u/common/hal/bits.h"
#include "hess1u/common/hal/offsets.h"
#include "hess1u/common/hal/nectaradc.h"

#include <linux/dma-mapping.h>
#include <linux/cdev.h>
#include <asm/dma.h>
#include <linux/dmaengine.h>
#include <linux/interrupt.h>
#include <linux/dmapool.h>
#include <mach/at_hdmac.h>

irqreturn_t IrqHandlerNoDma(int irq_number, void *p_dev);
irqreturn_t IrqHandlerDma(int irq_number, void *p_dev);

void StopDMAchannelActivity(void);
void RemoveDmaChannelsGlb(void);
int PrepareDmaChannelsGlb(void);
void FirstDMAInitGlb(void);
struct device * GetDMAdevice(void);

extern dma_addr_t g_dmaDestAddr0 ;
extern int g_nPossibleDMA ;

typedef struct
{
	uint32_t total_irq_count;
	int daq_errors;

	long diff_us;
	long triggerRate;
	//	uint32_t irq_counts;
	uint32_t sync_lost;
	uint32_t eventcounter_mismatch;
	uint32_t eventcounter_mismatch_old;
	uint32_t eventcounter_mismatch_new;
	uint32_t eventcounter;
	uint32_t bytes_droped;
	uint32_t max_fifo_count;
	uint32_t fifo_count;
	uint32_t type;
	uint32_t fifo_empty;
	uint32_t frame_15;
	uint32_t frame_16;
	uint32_t frame_17;
	uint32_t frame_32;
	uint32_t frame_other;
	uint32_t frame_length;
	uint32_t unknown_type;
	uint32_t length_mismatch;
	//	uint16_t event_number_old;
	//	uint16_t event_number_new;

	int		notCompleted;
} IRQDevice_debug_data_t;

typedef struct
{
	char devid;
	int dum;
	int m_nDma;
	//int m_nChannelIndex;
	//irqreturn_t(*m_fpIrqHandler)(int, void*);
	irq_handler_t m_fpIrqHandler;
	rwlock_t accessLock;
	wait_queue_head_t waitQueue;
	//	int irqEnable;

	// individual data
	IRQDevice_debug_data_t debug_data;
	hess_ring_buffer_t* hess_data;
} IRQDevice_t; // kernel only

typedef struct
{
	dev_t devno; // Major Minor Device Number
	struct cdev cdev; // Character device
	struct device *device;
} chrdrv_t;


// Device Structure
typedef struct
{
	chrdrv_t bus_driver;
	chrdrv_t fpga_driver;
	chrdrv_t daq_driver;

	struct class *sysfs_class; // Sysfs class

	struct resource *iomemregion;
	struct semaphore sem;
	size_t IOMemSize;
	size_t offset;
	void* mappedIOMem; // Mapped IO Memory to Physical memory
} hess_dev_t;

extern hess_dev_t g_my_device_instance;
extern IRQDevice_t g_srqDevice;

#define VOID_PTR(PTR, OFFS) ((void*)(((char*)(PTR)) + (OFFS)))

// *** Helper Functions for direct SMC bus Access ***
static inline void smc_bus_write32(size_t offset, uint32_t data)
{
	iowrite16(data, VOID_PTR(g_my_device_instance.mappedIOMem, offset));
	iowrite16(data >> 16, VOID_PTR(g_my_device_instance.mappedIOMem, offset + 2));
}
static inline void smc_bus_write16(size_t offset, uint16_t data)
{
	iowrite16(data, VOID_PTR(g_my_device_instance.mappedIOMem, offset));
}
static inline uint32_t smc_bus_read32(size_t offset)
{
	return ioread16(VOID_PTR(g_my_device_instance.mappedIOMem, offset)) | (ioread16(VOID_PTR(g_my_device_instance.mappedIOMem, offset + 2)) << 16); // ## in der hoffnung ioread16() gibt mindestens 32 bit zurueck
}
static inline uint16_t smc_bus_read16(size_t offset)
{
	return ioread16(VOID_PTR(g_my_device_instance.mappedIOMem, offset));
}


#endif  /* #ifndef __hessirqhandlers_h__ */
