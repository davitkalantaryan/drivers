#ifndef __common_drv_data_h__
#define __common_drv_data_h__

#include <linux/cdev.h>
#include <linux/semaphore.h>
#include "hess1u/common/hal/types.h"

//--- irq ---------------------------------------------------------------------
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
	uint32_t fifo_empty;
	uint32_t samples_0;
	uint32_t samples_16;
	uint32_t samples_32;
	uint32_t samples_other;
	uint32_t frame_length;
	uint32_t unknown_type;
	uint32_t length_mismatch;
} IRQDevice_debug_data_t;

typedef struct
{
	char devid;
	int dum;
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


#define VOID_PTR(PTR, OFFS) ((void*)(((char*)(PTR)) + (OFFS)))

extern hess_dev_t my_device_instance;

static inline void smc_bus_write16(size_t offset, uint16_t data)
{
	iowrite16(data, VOID_PTR(my_device_instance.mappedIOMem, offset));
}

#endif  /* #ifndef __common_drv_data_h__ */
