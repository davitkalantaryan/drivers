

// Kernel Includes
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/file.h>
#include <linux/types.h>
#include <linux/fs.h>		/* for struct file_operations */
#include <linux/io.h>		/* ioremap and friends */
#include <linux/version.h>
#include <linux/time.h>

// irq handling
#include <linux/interrupt.h>
#include <linux/irqflags.h>
#include <linux/irqreturn.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

// machine dependend includes
#include <mach/hardware.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/at91_pio.h>
#include <mach/at91_pmc.h>
#include <mach/at91sam9_smc.h>

//
//#include "stdio.h"
#include "debug.h"
#include "tools.h"
#include "hess1u/common/hal/types.h"
#include "hess1u/common/hal/bits.h"
//#include "hess1u/common/hal/bus.h"
#include "hess1u/common/hal/offsets.h"
#include "print_functions.h"
#include "common_drv_data.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("desy");
MODULE_DESCRIPTION("memory interface for hess1u fpga");

enum _DMA_TYPES_{ _NO_DMA_ = 0, _SLOW_DMA_ = 1, FAST_DMA_ = 2 };
static int s_nDMAtype = _NO_DMA_;
module_param_named(dma, s_nDMAtype, int, S_IRUGO | S_IWUSR);

#ifdef __CDT_PARSER__
#define __init
#define __exit
#define __user
// add to includes: /var/oe/develop/oe-9g45/openembedded-core/build/tmp-eglibc/work/stamp9g45-angstrom-linux-gnueabi/linux-3.0-r1/linux-3.0/include
// add to includes: /var/oe/develop/oe-9g45/openembedded-core/build/tmp-eglibc/work/stamp9g45-angstrom-linux-gnueabi/linux-3.0-r1/linux-3.0/arch/arm/mach-at91/include
// add to includes: /var/oe/develop/oe-9g45/openembedded-core/build/tmp-eglibc/work/stamp9g45-angstrom-linux-gnueabi/linux-3.0-r1/linux-3.0/arch/arm/include
#endif

// Driver Definitions
#define SMC_DEV_NAME "hessbus"
#define DAQ_DEV_NAME "hessdaq"
#define FPGA_DEV_NAME "hessfpga"

#define DRIVER_VERSION "Release 0.3"

#define DRIVER_NAME    "HESS_DRIVER"

#define SMC_DEV_MINOR		0	// For SMC Bus Device
#define SMC_DEV_MINOR_COUNT 1	// For SMC Bus Device
#define DAQ_DEV_MINOR		0	// For DAQ IRQ Device
#define DAQ_DEV_MINOR_COUNT 1	// For DAQ IRQ Device
#define FPGA_DEV_MINOR		0	// For SMC Bus Device
#define FPGA_DEV_MINOR_COUNT 1	// For SMC Bus Device
#define CTRLBUS_CHIPSELECT	2 // which chip select line should get the timing
// CTRLBUS Timing Setup
#define CTRLBUS_SETUP		2 // Bus Cycle Setup Time
#define CTRLBUS_PULSE		5 // Bus Cycle Pulse Time
#define CTRLBUS_CYCLE		(CTRLBUS_SETUP + CTRLBUS_PULSE + 2) // Bus Cycle Length
#define CTRLBUS_SETUP_CS	1
#define CTRLBUS_PULSE_CS	6

#define SMC_MEM_START		AT91_CHIPSELECT_2 //0x30000000	// physical address region of the controlbus bus mapping *is fixed!*
#define SMC_MEM_LEN			0x4000000 // 64M
#define IOMEM_NAME			"bus2fpga"

// PIN Definitions
#define TEST_PIN_1			AT91_PIN_PB20	// Test Pin on Test Connector, may have spikes if taskit kernel is used (pin used for r/w led?!)
#define TEST_PIN_2			AT91_PIN_PB21	// Test Pin on Test Connector
#define TEST_PIN_3			AT91_PIN_PB22	// Test Pin on Test Connector
#define TEST_PIN_4			AT91_PIN_PB23	// Test Pin on Test Connector

#define PDB_FPGA_nCONFIG	AT91_PIN_PD11	// nConfig Pin for Power Distribution Box

#define BOARD_ID_PIN0		AT91_PIN_PB24	// board id pins to identify hardware board
#define BOARD_ID_PIN1		AT91_PIN_PB26	// board id pins to identify hardware board

#define HESS_IRQ_PIN		AT91_PIN_PC1
#define HESS_IRQ_PIN_NUMBER (1 << 1) // pc1
// Further Definitions

#define FPGA_EVENTCOUNTER_MAX 65535

// Control Source code Options
#define TEST_CODE 				0		// Activate Test Code, if set to 0, should never be used with productive release
#define TEST_IRQ				0		// if 1, IRQ TEst Functions are active
#define IRQ_ENABLED				1		// set to 1 to activate IRQ functionality
#define DAQ_DEVICE_ENABLED		1		// if 1, DAQ Device is enabled
#define BUS_DEVICE_ENABLED		1		// if 1, BUS Device is enabled
#define FPGA_DEVICE_ENABLED		1		// if 1, BUS Device is enabled

// Static Instance of the Control Bus Device
hess_dev_t my_device_instance;

// *** Helper Functions for direct SMC bus Access ***
static inline void smc_bus_write32(size_t offset, uint32_t data)
{
	iowrite16(data, VOID_PTR(my_device_instance.mappedIOMem, offset));
	iowrite16(data >> 16, VOID_PTR(my_device_instance.mappedIOMem, offset + 2));
}

static inline uint32_t smc_bus_read32(size_t offset)
{
	return ioread16(VOID_PTR(my_device_instance.mappedIOMem, offset)) | (ioread16(VOID_PTR(my_device_instance.mappedIOMem, offset + 2)) << 16); // ## in der hoffnung ioread16() gibt mindestens 32 bit zurueck
}
static inline uint16_t smc_bus_read16(size_t offset)
{
	return ioread16(VOID_PTR(my_device_instance.mappedIOMem, offset));
}

// *** Include hal device functions (must be after definition of smc access functions)

#include "hess1u/common/hal/smi.h"
#include "hess1u/common/hal/nectaradc.h"

//--- pins ---------------------------------------------------------------------
// initialize test pins
static inline void initTestPins(void)
{
	// configure nCOnfig Pin
	at91_set_gpio_output(TEST_PIN_1, 0);
	at91_set_gpio_output(TEST_PIN_2, 0);
	at91_set_gpio_output(TEST_PIN_3, 0);
	at91_set_gpio_output(TEST_PIN_4, 0);
}

struct smc_config
{
	// Setup register
	uint8_t ncs_read_setup;
	uint8_t nrd_setup;
	uint8_t ncs_write_setup;
	uint8_t nwe_setup;

	// Pulse register
	uint8_t ncs_read_pulse;
	uint8_t nrd_pulse;
	uint8_t ncs_write_pulse;
	uint8_t nwe_pulse;

	// Cycle register
	uint16_t read_cycle;
	uint16_t write_cycle;

	// Mode register
	uint32_t mode;
	uint8_t tdf_cycles : 4;
};

void smc_configure(int cs, struct smc_config* config)
{
	// Setup register
	at91_sys_write(AT91_SMC_SETUP(cs), AT91_SMC_NWESETUP_(config->nwe_setup) | AT91_SMC_NCS_WRSETUP_(config->ncs_write_setup) | AT91_SMC_NRDSETUP_(config->nrd_setup) | AT91_SMC_NCS_RDSETUP_(config->ncs_read_setup));

	// Pulse register
	at91_sys_write(AT91_SMC_PULSE(cs), AT91_SMC_NWEPULSE_(config->nwe_pulse) | AT91_SMC_NCS_WRPULSE_(config->ncs_write_pulse) | AT91_SMC_NRDPULSE_(config->nrd_pulse) | AT91_SMC_NCS_RDPULSE_(config->ncs_read_pulse));

	// Cycle register
	at91_sys_write(AT91_SMC_CYCLE(cs), AT91_SMC_NWECYCLE_(config->write_cycle) | AT91_SMC_NRDCYCLE_(config->read_cycle));

	// Mode register
	at91_sys_write(AT91_SMC_MODE(cs), config->mode | AT91_SMC_TDF_(config->tdf_cycles));
}

// Apply the timing to the SMC
static void ctrlbus_apply_timings(void)
{
	struct smc_config config = {
		// Setup register
		.ncs_read_setup = 2, //CTRLBUS_SETUP_CS,
		.nrd_setup = 2, //CTRLBUS_SETUP,
		.ncs_write_setup = CTRLBUS_SETUP_CS,
		.nwe_setup = CTRLBUS_SETUP,

		// Pulse register
		.ncs_read_pulse = 8, //CTRLBUS_PULSE_CS,
		.nrd_pulse = 8, //CTRLBUS_PULSE,
		.ncs_write_pulse = CTRLBUS_PULSE_CS,
		.nwe_pulse = CTRLBUS_PULSE,

		// Cycle register
		.read_cycle = 8 + 2, //CTRLBUS_CYCLE,
		.write_cycle = CTRLBUS_CYCLE,

		// Mode register
		.mode = AT91_SMC_READMODE | AT91_SMC_WRITEMODE | AT91_SMC_BAT_SELECT | AT91_SMC_DBW_16 | AT91_SMC_EXNWMODE_READY,
		.tdf_cycles = 0 };

	smc_configure(CTRLBUS_CHIPSELECT, &config);

	INFO("SMC Bus Timing: chipselect=%u tdf_cycles=%u mode=0x%x\n   "
		" read: cycle=%u setup=%u pulse=%u cs_setup=%u sc_pulse=%u\n   "
		"write: cycle=%u setup=%u pulse=%u cs_setup=%u sc_pulse=%u\n", CTRLBUS_CHIPSELECT, config.tdf_cycles, config.mode, config.read_cycle, config.nrd_setup, config.nrd_pulse, config.ncs_read_setup, config.ncs_read_pulse, config.write_cycle, config.nwe_setup, config.nwe_pulse, config.ncs_write_setup, config.ncs_write_pulse);

	//	set_memory_timing(chipselect, cycle, setup, pulse, setup_cs, pulse_cs, data_float, use_iordy);

}


static IRQDevice_t g_srqDevice;

// returns the current event count
// may be locking is not needed here
static uint32_t readIrqCount(IRQDevice_t* dev)
{
	uint32_t i;
	read_lock_irq(&dev->accessLock);
	i = dev->debug_data.total_irq_count;
	read_unlock_irq(&dev->accessLock);
	return i;
}

// ------------------------------------------ IRQ Handling --------------
static inline void clear_irq(void)
{
	uint16_t temp;
	temp = smc_bus_read16(OFFS_CONTROL) & (~(1 << BIT_CONTROL_TRIGGER_IRQ)); // ## hier und ueberall besser das macro von atmel?, sollte es nicht immer base + offset sein?!
	smc_bus_write16(OFFS_CONTROL, temp);
}

// todo: this funktion should be in the hal
static hess1u_system_enum get_hess1u_device_type(void)
{
	return hess1u_smi_getSystemType();
	/*
	uint16_t id;
	id = smc_bus_read16(0x312);

	switch(id)
	{
	case HESS1U_MODNAME_DRAWER: return HESS1U_SYSTEM_DRAWER;
	case HESS1U_MODNAME_DRAWERINTERFACEBOX: return HESS1U_SYSTEM_DRAWER_INTERFACE_BOX;
	case HESS1U_MODNAME_POWERDISTRIBUTIONBOX: return HESS1U_SYSTEM_POWER_DISTRIBUTION_BOX;
	case HESS1U_MODNAME_TESTMODE: return HESS1U_SYSTEM_TESTMODE;
	default: return HESS1U_SYSTEM_UNKNOWN;
	}
	return HESS1U_SYSTEM_UNKNOWN;
	*/
}
static const char* get_hess1u_device_type_name(hess1u_system_enum _type)
{
	return hess1u_smi_getSystemTypeAsString(_type);
	/*
	switch(_type)
	{
	case HESS1U_SYSTEM_DRAWER: return "DRAWER";
	//case HESS1U_SYSTEM_DRAWER_INTERFACE_BOX: return "DRAWERINTERFACEBOX";
	case HESS1U_SYSTEM_POWER_DISTRIBUTION_BOX: return "POWERDISTRIBUTIONBOX";
	case HESS1U_SYSTEM_TESTMODE: return "TESTMODE";
	default: return "UNKNOWN";
	}
	return "UNKNOWN";
	*/
}

static irqreturn_t srqIrqHandler(int irq_number, void *p_dev)
{
	// todo: ringbufferverwaltung auslagern -> inline

	//	setTestPin(TEST_PIN_1,SET);
	static uint16_t first = 1;
	static uint16_t testPattern = 0;

	IRQDevice_t* dev = (IRQDevice_t*)p_dev;
	uint16_t words = 0;
	uint16_t numberOfEventsInFifo = 0;
	uint16_t* data_pointer = NULL;
	int k = 0;
	static int frame_length = 0;
	size_t fifo_offset;

	static struct timespec time_old;
	static struct timespec time_new;
	static uint32_t event_number_old = 0;
	static uint32_t event_number_new = 0;

	static uint16_t packet_length = 1;
	static uint16_t sample_length = 0;
	static hess1u_system_enum hess1u_device_type = HESS1U_SYSTEM_UNKNOWN;
	//	static hess1u_system_enum hess1u_system_type = HESS1U_SYSTEM_UNKNOWN;

	write_lock(&dev->accessLock);

	switch (hess1u_device_type)
	{
	case HESS1U_SYSTEM_DRAWER: smc_bus_write16(BASE_HESS1U_DRAWER_READOUT + OFFS_HESS1U_DRAWER_READOUT_ISRHANDSHAKE, bitValue16(BIT_HESS1U_DRAWER_READOUT_ISRHANDSHAKE_START)); break;
	case HESS1U_SYSTEM_DRAWER_INTERFACE_BOX: smc_bus_write16(ADDR_DRAWERINTERFACEBOX_IRQ_HANDSHAKE, bitValue16(BIT_HESS1U_DRAWER_READOUT_ISRHANDSHAKE_START)); break;
	case HESS1U_SYSTEM_TESTMODE: smc_bus_write16(BASE_HESS1U_DRAWER_READOUT + OFFS_HESS1U_DRAWER_READOUT_ISRHANDSHAKE, bitValue16(BIT_HESS1U_DRAWER_READOUT_ISRHANDSHAKE_START)); break;
	case HESS1U_SYSTEM_POWER_DISTRIBUTION_BOX: break;
	case HESS1U_SYSTEM_UNKNOWN: break;
	default: break;
	}

	dev->debug_data.total_irq_count++;
	getnstimeofday(&time_new);

	if (first == 1)
	{
		getnstimeofday(&time_old);

		hess1u_device_type = get_hess1u_device_type();
		DBG("hess1u_device_type == %s\n", get_hess1u_device_type_name(hess1u_device_type));


		first = 0;
	}
	else
	{
		hess1u_system_enum temp = get_hess1u_device_type();
		if (temp != hess1u_device_type)
		{
			hess1u_device_type = temp;
			DBG("hess1u_device_type == %s\n", get_hess1u_device_type_name(hess1u_device_type));
		}
	}

	//	DBG("srqIrqHandler(...) called \n");
	//	int temp = at91_sys_read(AT91_PIOC + PIO_PDSR);
	//	if((temp & 0x2) == 0) // ## ja, das ist PortC Pin 1... sehr unschoen aber es gibt noch kein gutes macro
	//	{
	//		DBG("srqIrqHandler(...) falling [%x]\n",temp); // ## wenn der irq sehr kurz ist landet man immer hier
	////		goto end;
	//	}
	//	else {DBG("srqIrqHandler(...) rising [%x]\n",temp);}

	switch (hess1u_device_type)
	{
	case HESS1U_SYSTEM_DRAWER:
		numberOfEventsInFifo = smc_bus_read16(BASE_HESS1U_DRAWER_READOUT + OFFS_HESS1U_DRAWER_READOUT_EVENTSPERIRQ); // ## kommt spaeter aus dem fifo durch parsen ?!
		words = smc_bus_read16(BASE_HESS1U_DRAWER_READOUT + OFFS_HESS1U_DRAWER_READOUT_EVENTFIFOWORDCOUNT);
		//			DBG("srqIrqHandler(...) words: [%x]\n",words);
		if (words == 0x0)
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
			smc_bus_read16(BASE_HESS1U_DRAWER_READOUT + OFFS_HESS1U_DRAWER_READOUT_EVENTFIFOREADSTROBE); // ## move to start of isr

			//				data_pointer = ringbuffer_get_write_pointer();
			data_pointer = &(dev->hess_data->write_pointer->as_raw_u16[0]);

			fifo_offset = BASE_HESS1U_DRAWER_READOUT + OFFS_HESS1U_DRAWER_READOUT_EVENTFIFO;
			for (k = 0; k < (HESS_SAMPLE_DATA_SIZE / 2); ++k) // ## ist die frage nach was man sich richten sollte... sizeof(structs)?, offsets?, noch mehr defines?
			{
				*(data_pointer++) = smc_bus_read16(fifo_offset); // ## unrolling bringt hier was...
				fifo_offset += 2;
			}

			words--;

			uint16_t type = dev->hess_data->write_pointer->as_raw_u16[0];

			//				if((dev->hess_data->write_pointer->as_control.data_type.u16_[0] == 0x80a) && (dev->hess_data->write_pointer->as_sample.low_gain[6] != 0xb38)) {dev->debug_data.unknown_type++;}

			if (((type & DATATYPE_MASK) == DATATYPE_SAMPLE_CONTROL) || ((type & DATATYPE_MASK) == DATATYPE_TESTDATA_FRONTENDFIFOCONTROL))
			{
				// control data
				if (sample_length == 16) { dev->debug_data.samples_16++; }
				else if (sample_length == 32) { dev->debug_data.samples_32++; }
				else if (sample_length == 0) { dev->debug_data.samples_0++; }
				else  { dev->debug_data.samples_other++; }
				dev->debug_data.frame_length = frame_length;

				if (frame_length != packet_length)
				{
					dev->debug_data.length_mismatch++;
				}

				packet_length = dev->hess_data->write_pointer->as_control.packet_length;
				frame_length = 1;
				sample_length = 0;

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
			else if ((type & DATATYPE_MASK) == DATATYPE_SAMPLE_WAVEFORM)
			{
				sample_length++;
				frame_length++;
			}
			else if (((type & DATATYPE_MASK) == DATATYPE_SAMPLE_CHARGE)
				|| ((type & DATATYPE_MASK) == DATATYPE_SAMPLE_TXT)
				|| ((type & DATATYPE_MASK) == DATATYPE_SAMPLE_CHARGESLIDINGWINDOW))
			{
				frame_length++;
			}
			else
			{
				dev->debug_data.unknown_type++;
			}

			hess_sample_data_debug_t *temp_write_pointer;
			temp_write_pointer = dev->hess_data->write_pointer;
			temp_write_pointer++;
			if (temp_write_pointer == dev->hess_data->end)
			{
				temp_write_pointer = dev->hess_data->begin;
			}
			dev->hess_data->write_pointer = temp_write_pointer;

			if (words == 0) { goto out; }
		}
		break;

	case HESS1U_SYSTEM_DRAWER_INTERFACE_BOX:
		numberOfEventsInFifo = smc_bus_read16(ADDR_DRAWERINTERFACEBOX_EVENTS_PER_IRQ); // ## kommt spaeter aus dem fifo durch parsen ?!
		words = smc_bus_read16(BASE_DRAWERINTERFACEBOX_TRIGGER + OFFS_DRAWERINTERFACEBOX_TRIGGER_FIFO_WORD_COUNT);
		if (words == 0x0)
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
			//				smc_bus_read16(BASE_DRAWERINTERFACEBOX + OFFS_DRAWERINTERFACEBOX_FIFO_READ_STROBE);

			data_pointer = &(dev->hess_data->write_pointer->as_raw_u16[0]);

			fifo_offset = BASE_DRAWERINTERFACEBOX_TRIGGER + OFFS_DRAWERINTERFACEBOX_TRIGGER_FIFO; // ## first read is also fifo read strobe
			for (k = 0; k < (HESS_TRIGGER_DATA_SIZE / 2); ++k)
			{
				*(data_pointer++) = smc_bus_read16(fifo_offset); // ## unrolling bringt hier was...
				fifo_offset += 2;
			}
			for (k = 0; k < ((HESS_SAMPLE_DATA_SIZE - HESS_TRIGGER_DATA_SIZE) / 2); ++k)
			{
				*(data_pointer++) = 0x0;
			}

			words--;

			uint16_t type = dev->hess_data->write_pointer->as_raw_u16[0];

			if ((type & DATATYPE_MASK) == DATATYPE_SAMPLE_TRIGGER)
			{
				event_number_new = (dev->hess_data->write_pointer->as_trigger.event_counter_h << 16) + dev->hess_data->write_pointer->as_trigger.event_counter_l;
				dev->debug_data.eventcounter = event_number_new;
				if ((uint32_t)(event_number_old + 1) != event_number_new)
				{
					dev->debug_data.eventcounter_mismatch_old = event_number_old;
					dev->debug_data.eventcounter_mismatch_new = event_number_new;
					dev->debug_data.eventcounter_mismatch++;
				}
				event_number_old = event_number_new;
			}
			else
			{
				dev->debug_data.unknown_type++;
			}

			hess_sample_data_debug_t *temp_write_pointer;
			temp_write_pointer = dev->hess_data->write_pointer;
			temp_write_pointer++;
			if (temp_write_pointer == dev->hess_data->end)
			{
				temp_write_pointer = dev->hess_data->begin;
			}
			dev->hess_data->write_pointer = temp_write_pointer;

			if (words == 0) { goto out; }
		}
		break;

	case HESS1U_SYSTEM_TESTMODE:
		numberOfEventsInFifo = smc_bus_read16(BASE_HESS1U_DRAWER_READOUT + OFFS_HESS1U_DRAWER_READOUT_EVENTSPERIRQ); // ## kommt spaeter aus dem fifo durch parsen ?!
		words = smc_bus_read16(BASE_HESS1U_DRAWER_READOUT + OFFS_HESS1U_DRAWER_READOUT_EVENTFIFOWORDCOUNT);
		if (words == 0x0)
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
			smc_bus_read16(BASE_HESS1U_DRAWER_READOUT + OFFS_HESS1U_DRAWER_READOUT_EVENTFIFOREADSTROBE);
			data_pointer = &(dev->hess_data->write_pointer->as_raw_u16[0]);
			fifo_offset = BASE_HESS1U_DRAWER_READOUT + OFFS_HESS1U_DRAWER_READOUT_EVENTFIFO;

			for (k = 0; k < (HESS_SAMPLE_DATA_SIZE / 2); ++k) // ## ist die frage nach was man sich richten sollte... sizeof(structs)?, offsets?, noch mehr defines?
			{
				if (k == 0) { *(data_pointer++) = smc_bus_read16(fifo_offset); } // type
				else
				{
					*(data_pointer++) = testPattern;
					testPattern = (uint16_t)testPattern + 1;
				}

				//					fifo_offset += 2;
			}

			words--;

			uint16_t type = dev->hess_data->write_pointer->as_raw_u16[0];

			//				if((dev->hess_data->write_pointer->as_control.data_type.u16_[0] == 0x80a) && (dev->hess_data->write_pointer->as_sample.low_gain[6] != 0xb38)) {dev->debug_data.unknown_type++;}

			if (((type & DATATYPE_MASK) == DATATYPE_SAMPLE_CONTROL) || ((type & DATATYPE_MASK) == DATATYPE_TESTDATA_FRONTENDFIFOCONTROL))
			{
				// control data
				if (sample_length == 16) { dev->debug_data.samples_16++; }
				else if (sample_length == 32) { dev->debug_data.samples_32++; }
				else if (sample_length == 0) { dev->debug_data.samples_0++; }
				else  { dev->debug_data.samples_other++; }
				dev->debug_data.frame_length = frame_length;

				if (frame_length != packet_length)
				{
					dev->debug_data.length_mismatch++;
				}

				packet_length = dev->hess_data->write_pointer->as_control.packet_length;
				frame_length = 1;
				sample_length = 0;

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
			else if ((type & DATATYPE_MASK) == DATATYPE_SAMPLE_WAVEFORM)
			{
				sample_length++;
				frame_length++;
			}
			else if (((type & DATATYPE_MASK) == DATATYPE_SAMPLE_CHARGE)
				|| ((type & DATATYPE_MASK) == DATATYPE_SAMPLE_TXT)
				|| ((type & DATATYPE_MASK) == DATATYPE_SAMPLE_CHARGESLIDINGWINDOW))
			{
				frame_length++;
			}
			else
			{
				dev->debug_data.unknown_type++;
			}

			hess_sample_data_debug_t *temp_write_pointer;
			temp_write_pointer = dev->hess_data->write_pointer;
			temp_write_pointer++;
			if (temp_write_pointer == dev->hess_data->end)
			{
				temp_write_pointer = dev->hess_data->begin;
			}
			dev->hess_data->write_pointer = temp_write_pointer;

			if (words == 0) { goto out; }
		}
		break;

	case HESS1U_SYSTEM_POWER_DISTRIBUTION_BOX:
		break;

	default:
	case HESS1U_SYSTEM_UNKNOWN:
		DBG("isr: HESS1U_SYSTEM_UNKNOWN: nothing to do...\n");
		break;
	}

out:
	write_unlock(&dev->accessLock);
	wake_up(&dev->waitQueue);

	if (1)
	{
		__kernel_time_t sec = time_new.tv_sec - time_old.tv_sec;
		long diff_us = (1000000 * sec) + (time_new.tv_nsec - time_old.tv_nsec) / 1000;

		dev->debug_data.diff_us = diff_us;
		if (numberOfEventsInFifo > 0) { dev->debug_data.triggerRate = diff_us / numberOfEventsInFifo; }
		else { dev->debug_data.triggerRate = 0; }

		time_old = time_new;
	}

	switch (hess1u_device_type)
	{
	case HESS1U_SYSTEM_DRAWER: smc_bus_write16(BASE_HESS1U_DRAWER_READOUT + OFFS_HESS1U_DRAWER_READOUT_ISRHANDSHAKE, bitValue16(BIT_HESS1U_DRAWER_READOUT_ISRHANDSHAKE_STOP)); break;
	case HESS1U_SYSTEM_DRAWER_INTERFACE_BOX: smc_bus_write16(ADDR_DRAWERINTERFACEBOX_IRQ_HANDSHAKE, bitValue16(BIT_HESS1U_DRAWER_READOUT_ISRHANDSHAKE_STOP)); break;
	case HESS1U_SYSTEM_TESTMODE: smc_bus_write16(BASE_HESS1U_DRAWER_READOUT + OFFS_HESS1U_DRAWER_READOUT_ISRHANDSHAKE, bitValue16(BIT_HESS1U_DRAWER_READOUT_ISRHANDSHAKE_STOP)); break;
	case HESS1U_SYSTEM_POWER_DISTRIBUTION_BOX: break;
	case HESS1U_SYSTEM_UNKNOWN: break;
	default: break;
	}

	return IRQ_HANDLED;
}

static int installSRQHandler(void)
{
	//	INFO("Install IRQ Handler: ");
	int irqRequestResult;
	memset(&g_srqDevice, 0, sizeof(IRQDevice_t));
	rwlock_init(&(g_srqDevice.accessLock));
	init_waitqueue_head(&(g_srqDevice.waitQueue));

	g_srqDevice.hess_data = (hess_ring_buffer_t*)vmalloc(sizeof(hess_ring_buffer_t)); // ## kmalloc can be an option too
	memset(g_srqDevice.hess_data, 0x0, sizeof(hess_ring_buffer_t));
	//	g_srqDevice.hess_data = (hess_ring_buffer_t*) __get_free_pages(GFP_USER, 0); // ## kmalloc can be an option too
	//	memset(g_srqDevice.hess_data, 0, PAGE_SIZE << 0);

	if (!g_srqDevice.hess_data)
	{
		ERR("Install IRQ Handler: unable to allocate memory for ring buffer\n");
		return 1;
	}
	else
	{
		INFO("Install IRQ Handler: %u bytes allocated for ring buffer\n", sizeof(hess_ring_buffer_t));
	}

	g_srqDevice.hess_data->begin = &(g_srqDevice.hess_data->raw_buffer[0]);
	//	g_srqDevice.hess_data->end = &(g_srqDevice.hess_data->raw_buffer[(sizeof(raw_buffer_t)/sizeof(hess_event_data_t))]);
	g_srqDevice.hess_data->end = &(g_srqDevice.hess_data->raw_buffer[HESS_RING_BUFFER_COUNT]);
	g_srqDevice.hess_data->write_pointer = g_srqDevice.hess_data->begin;

	//	g_srqDevice.hess_data->begin = &(g_srqDevice.hess_data->raw_buffer[0]);
	//	g_srqDevice.hess_data->end = &(g_srqDevice.hess_data->raw_buffer[HESS_RING_BUFFER_COUNT]);
	//	g_srqDevice.hess_data->write_pointer = g_srqDevice.hess_data->begin;

	// Activate PIOC Clock (s.441 30.3.4)
	at91_sys_write(AT91_PMC_PCER, (1 << AT91SAM9G45_ID_PIOC)); // PORT_C ID == 4

	//msleep(100); // ## ?

	at91_set_GPIO_periph(HESS_IRQ_PIN, 0);
	at91_set_gpio_input(HESS_IRQ_PIN, 0); // 0 == no pullup
	at91_set_deglitch(HESS_IRQ_PIN, 1); // 1 == deglitch activated

	//msleep(100); // ## ?

	// Configure IRQ Capture Edge //enum _DMA_TYPES_{_NO_DMA_=0,_SLOW_DMA_=1,FAST_DMA_=2};
	switch (s_nDMAtype)
	{
	case _NO_DMA_:
		irqRequestResult = request_irq(HESS_IRQ_PIN, srqIrqHandler, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "hess_fpga", &g_srqDevice);
		break;
	case _SLOW_DMA_:
		irqRequestResult = request_irq(HESS_IRQ_PIN, srqIrqHandler, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "hess_fpga", &g_srqDevice);
		break;
	case FAST_DMA_:
		irqRequestResult = request_irq(HESS_IRQ_PIN, srqIrqHandler, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "hess_fpga", &g_srqDevice);
		break;
	default:
		ERROR2("Wrong DMA type, types are 0, 1, 2\n");
		return -1;
	}

	if (irqRequestResult != 0)
	{
		ERR("Install IRQ Handler: problem installing srq line irq handler %d\n", irqRequestResult);
		return 1; // error
	}

	return 0;
}

static void uninstallSRQHandler(void)
{
	at91_sys_write(AT91_PIOC + PIO_IDR, HESS_IRQ_PIN_NUMBER); // PIO_IDR == interrupt disable register

	free_irq(HESS_IRQ_PIN, &g_srqDevice);

	//	free_pages((unsigned long)(g_srqDevice.hess_data),0);
	vfree(g_srqDevice.hess_data);
}
//-----------------------------------------------------------------------------
//--- irq end -----------------------------------------------------------------

//--- daq ---------------------------------------------------------------------
//-----------------------------------------------------------------------------
#include "hess1u/common/hal/commands.h"

// ----------------------------------- Character Device Driver Interface ------------------

// Char Driver Implementation

typedef struct
{
	int usage_count;
	IRQDevice_t* irqDevice;
} daq_private_data_t;
static daq_private_data_t g_daq_private_data =
{
	.usage_count = 0,
	.irqDevice = &g_srqDevice
};

static int daq_chrdrv_open(struct inode * node, struct file *f)
{
	//	DBG("daq user_open\n");

	hess_dev_t* dev;

	dev = container_of(node->i_cdev, hess_dev_t, daq_driver.cdev);
	f->private_data = dev;

	return 0;
}

static int daq_chrdrv_release(struct inode *node, struct file *f)
{
	//	DBG("daq user_release\n");
	return 0;
}

static ssize_t daq_chrdrv_read(struct file *f, char __user *buf, size_t size, loff_t *offs)
//static ssize_t daq_chrdrv_read(struct file *f, char *buf, size_t size, loff_t *offs)
{
	ssize_t retVal = 0;

	hess_dev_t* dev = f->private_data;
	unsigned int myoffs = dev->offset;

	//	DBG("daq user_read offs: 0x%p + 0x%x + 0x%x = 0x%x size:%d\n",dev->mappedIOMem, dev->offset, (int)*offs, myoffs, size);

	// lock the resource
	if (down_interruptible(&dev->sem)) { return -ERESTART; }

	// Check File Size
	if ((myoffs) >= dev->IOMemSize) { goto out; }

	// Limit Count
	if ((myoffs + size) >= dev->IOMemSize) { size = dev->IOMemSize - myoffs; }

	retVal = copy_from_io_to_user_16bit(buf, VOID_PTR(dev->mappedIOMem, myoffs), size);

	if (retVal<0) { goto out; }

	*offs += retVal; // Is this realy needed?

out:
	up(&dev->sem); // free the resource

	return retVal;
}

static ssize_t daq_chrdrv_write(struct file *f, const char __user *buf, size_t size, loff_t *offs)
//static ssize_t daq_chrdrv_write(struct file *f, const char *buf, size_t size, loff_t *offs)
{
	ssize_t retVal = 0;
	hess_dev_t* dev = f->private_data;

	//unsigned int myoffs=dev->offset + *offs;
	unsigned int myoffs = dev->offset;
	//	DBG("daq user_write offs: 0x%x  size:0x%d\n",myoffs, size);

	// lock the resource
	if (down_interruptible(&dev->sem)) { return -ERESTART; }

	// Check File Size
	//        if ((myoffs + dev->offset) >= dev->IOMemSize) goto out;
	if ((myoffs) >= dev->IOMemSize) { goto out; }

	// Limit Count
	if ((myoffs + size) >= dev->IOMemSize) size = dev->IOMemSize - myoffs;

	retVal = copy_from_user_to_io_16bit(VOID_PTR(dev->mappedIOMem, myoffs), buf, size);
	if (retVal<0) { goto out; } // fault occured, write was not completed

	*offs += retVal;

out:
	up(&dev->sem);// free the resource

	return retVal;
}

static loff_t daq_chrdrv_llseek(struct file *f, loff_t offs, int whence)
{
	hess_dev_t* dev = f->private_data;
	unsigned int myoffs = offs;
	//	DBG("daq user_llseek offs: 0x%x  whence: %d\n", myoffs, whence);
	switch (whence)
	{
	case SEEK_SET:
		if (offs >= (dev->IOMemSize))
		{
			// out of range seeking
			return -1;
		}
		dev->offset = offs;
		return offs;
		break;

	case SEEK_CUR:
		if ((dev->offset + offs) >= (dev->IOMemSize))
		{
			// out of range seeking
			return -1;
		}
		dev->offset += offs;
		return dev->offset;
		break;

	case SEEK_END:
		if ((dev->offset) >= (dev->IOMemSize))
		{
			// out of range seeking
			return -1;
		}
		dev->offset = dev->IOMemSize - myoffs;
		return dev->offset;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

//--- mmap -------------------------------------------------------------------------
static void daq_vma_open(struct vm_area_struct *vma)
{
	daq_private_data_t* data = vma->vm_private_data;
	data->usage_count++;

	//	vma->vm_private_data->usage_count++;
}

static void daq_vma_close(struct vm_area_struct *vma)
{
	daq_private_data_t* data = vma->vm_private_data;
	data->usage_count--;

	//	vma->vm_private_data->usage_count--;
}

static int daq_vma_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	//	DBG("handling fault, ");
	struct page *page;
	daq_private_data_t* data = vma->vm_private_data;

	//	page = virt_to_page(((char*)(data->irqDevice->hess_data)) + (vmf->pgoff << PAGE_SHIFT));
	page = vmalloc_to_page(((char*)(data->irqDevice->hess_data)) + (vmf->pgoff << PAGE_SHIFT));
	get_page(page);
	vmf->page = page;

	return 0;
}

//static int daq_vma_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
//{
////	DBG("handling fault, ");
//	daq_private_data_t* data = vma->vm_private_data;
//	char* pagePtr = NULL;
//	struct page *page;
//	unsigned long offset;
//
//	offset = (((unsigned long) vmf->virtual_address) - vma->vm_start) + (vma->vm_pgoff << PAGE_SHIFT);
//
////	DBG( " virtua.addr: 0x%x offset: 0x%x  vm_start:0x%x  vm_pgoff:%d\n", (unsigned int) vmf->virtual_address, (unsigned int) offset, (unsigned int) vma->vm_start, (unsigned int) vma->vm_pgoff);
//
//	if (offset >= sizeof(hess_ring_buffer_t))
//	{
//		return VM_FAULT_SIGBUS;
//	}
//
//	pagePtr = (char*) (data->irqDevice->hess_data);
//	pagePtr += offset;
//
////	DBG("getting page\n");
//
////	page = virt_to_page(pagePtr);
//	page = vmalloc_to_page(pagePtr);
//	get_page(page);
//	vmf->page = page;
//
//	return 0;
//}

static struct vm_operations_struct daq_remap_vm_ops =
{
	.open = daq_vma_open,
	.close = daq_vma_close,
	.fault = daq_vma_fault,
};

static int daq_chrdrv_mmap(struct file *f, struct vm_area_struct *vma)
{
	unsigned long off = vma->vm_pgoff << PAGE_SHIFT;
	//unsigned long physical = SMC_MEM_START + off;
	unsigned long vsize = vma->vm_end - vma->vm_start;
	unsigned long psize = SMC_MEM_LEN - off;

	if (vsize > psize)
	{
		return -EINVAL; // spans to high
	}

	// do not map here, let no page do the mapping
	vma->vm_ops = &daq_remap_vm_ops;
	vma->vm_flags |= VM_RESERVED;
	vma->vm_private_data = &g_daq_private_data;

	daq_vma_open(vma);
	return 0;
}
//--- mmap end -------------------------------------------------------------------------

// ----------------------------------- IOCTL Interface ---------------------
static int smc_ioctl_cmd_wait_for_irq(unsigned long arg) // ## name
{
	ioctl_command_wait_for_irq_t cmd;

	//DBG("ioctl wait for irq command\n");

	if (copy_from_user(&cmd, (ioctl_command_wait_for_irq_t*)arg, sizeof(ioctl_command_wait_for_irq_t)))
	{
		ERR("error copying data from userspace");
		return -EACCES;
	}

	//DBG( " timeout %d ms == %d jiffis (1 secound has %d jiffis)\n", cmd.timeout_ms, cmd.timeout_ms * HZ / 1000, HZ);

	// wait for the irq
	uint32_t lastCount = readIrqCount(&g_srqDevice);

	// recalculate timeout into jiffis

	//    setTestPin(1,0);
	//    wait_event(srqDevice.waitQueue, (lastCount!=srqDevice.irq_count));
	int result = wait_event_interruptible_timeout(g_srqDevice.waitQueue, (lastCount != g_srqDevice.debug_data.total_irq_count), cmd.timeout_ms * HZ / 1000);
	if (result == -ERESTARTSYS)
	{
		// System wants to shutdown, so we just exit
		return -ERESTARTSYS;
	}
	if (result == 0)
	{
		//        setTestPin(1,1);
		// Timeout occured, we just return to user process
		return -ETIMEDOUT;
	}
	//    setTestPin(1,1);

	return 0; // ## warum nicht result?
}

static void smc_ioctl_switchFpgsClockSource(void)
{
	at91_set_gpio_output(TEST_PIN_2, 0);
	at91_set_gpio_output(TEST_PIN_2, 1);
	at91_set_gpio_output(TEST_PIN_2, 1);
	at91_set_gpio_output(TEST_PIN_2, 1);
	at91_set_gpio_output(TEST_PIN_2, 0);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33)
static int daq_ioctl(struct inode *node, struct file *f, unsigned int cmd, unsigned long arg)
#else
static long daq_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
#endif
{
	switch (cmd)
	{
	case IOCTL_COMMAND_WAIT_FOR_IRQ:
		return smc_ioctl_cmd_wait_for_irq(arg);
	case IOCTL_COMMAND_SWITCH_FPGA_CLOCK_SOURCE:
		smc_ioctl_switchFpgsClockSource(); return 0;
		//		case DAQ_COMMAND_WR_CONFIG:
		//			return daq_ioctl_cmd_rdwr_config(arg, 1);
		//		case DAQ_COMMAND_RD_CONFIG:
		//			return daq_ioctl_cmd_rdwr_config(arg, 0);
		//		case DAQ_COMMAND_RD_STATUS:
		//			return daq_ioctl_cmd_rd_status(arg);
		//		case DAQ_COMMAND_WR_IRQ_ENABLE:
		//			return daq_ioctl_cmd_rdwr_irq_enable(arg, 1);
		//		case DAQ_COMMAND_RD_IRQ_ENABLE:
		//			return daq_ioctl_cmd_rdwr_irq_enable(arg, 0);
	default:
		ERR("invalid smc ioctl cmd: %d\n", cmd);
		return -EFAULT;
	}
	return 0;
}
//-----------------------------------------------------------------------------
//--- daq end -----------------------------------------------------------------

// **************************************** Character Device Instance Definiton

// Char Driver File Operations
static struct file_operations daq_chrdrv_fops = { .open = daq_chrdrv_open, // handle opening the file-node
.release = daq_chrdrv_release, // handle closing the file-node
.read = daq_chrdrv_read, // handle reading
.write = daq_chrdrv_write, // handle writing
.llseek = daq_chrdrv_llseek, // handle seeking in the file
.unlocked_ioctl = daq_ioctl, // handle special i/o operations
.mmap = daq_chrdrv_mmap };

// Initialization of the Character Device
static int daq_chrdrv_create(hess_dev_t* mydev)
{
	int err = -1;
	char name[30];
	chrdrv_t* chrdrv = &mydev->daq_driver;

	INFO("Install DAQ Character Device\n");
	if (!mydev)
	{
		ERR("Null pointer argument!\n");
		goto err_dev;
	}

	/* Allocate major and minor numbers for the driver */
	err = alloc_chrdev_region(&chrdrv->devno, DAQ_DEV_MINOR, DAQ_DEV_MINOR_COUNT, DAQ_DEV_NAME);
	if (err)
	{
		ERR("Error allocating Major Number for daq driver.\n");
		goto err_region;
	}

	DBG("Major Number: %d\n", MAJOR(chrdrv->devno));

	/* Register the driver as a char device */
	cdev_init(&chrdrv->cdev, &daq_chrdrv_fops);
	chrdrv->cdev.owner = THIS_MODULE;
	DBG("char device allocated 0x%x\n", (unsigned int)&chrdrv->cdev);
	err = cdev_add(&chrdrv->cdev, chrdrv->devno, DAQ_DEV_MINOR_COUNT);
	if (err)
	{
		ERR("cdev_all failed\n");
		goto err_char;
	}
	DBG("Char device added\n");

	sprintf(name, "%s0", DAQ_DEV_NAME);

	// create devices
	chrdrv->device = device_create(mydev->sysfs_class, NULL, MKDEV(MAJOR(chrdrv->devno), 0), NULL, name, 0);

	if (IS_ERR(chrdrv->device))
	{
		ERR("%s: Error creating sysfs device\n", name);
		err = PTR_ERR(chrdrv->device);
		goto err_class;
	}

	hess1u_system_enum hess1u_device_type = get_hess1u_device_type();
	DBG("first check hess1u_device_type == %s\n", get_hess1u_device_type_name(hess1u_device_type));

	return 0;

err_class: cdev_del(&chrdrv->cdev);
err_char: unregister_chrdev_region(chrdrv->devno, DAQ_DEV_MINOR_COUNT);
err_region:

err_dev : return err;
}

// Initialization of the Character Device
static void daq_chrdrv_destroy(hess_dev_t* mydev)
{
	chrdrv_t* chrdrv = &mydev->daq_driver;
	device_destroy(mydev->sysfs_class, MKDEV(MAJOR(chrdrv->devno), 0));

	/* Unregister device driver */
	cdev_del(&chrdrv->cdev);

	/* Unregiser the major and minor device numbers */
	unregister_chrdev_region(chrdrv->devno, DAQ_DEV_MINOR_COUNT);
}

// ************************************************  SRQ IRQ Handler Code

#if TEST_CODE==1

// some debug helper code...
static void dumpPio(size_t pio)
{
#define DUMP(NAME, REG) INFO(#REG "\t= 0x%.8x  " NAME "\n", at91_sys_read(pio + REG));
	DUMP("Status Register", PIO_PSR);
	DUMP("Output Status Register", PIO_OSR);
	DUMP("Glitch Input Filter Status", PIO_IFSR);
	DUMP("Output Data Status Register", PIO_ODSR);
	DUMP("Pin Data Status Register", PIO_PDSR);
	DUMP("Interrupt Mask Register", PIO_IMR);
	DUMP("Interrupt Status Register", PIO_ISR);

	DUMP("Multi-driver Status Register", PIO_MDSR);
	DUMP("Pull-up Status Register", PIO_PUSR);

	DUMP("AB Status Register", PIO_ABSR);
	DUMP("Output Write Status Register", PIO_OWSR);
#undef DUMP
}

#endif

// fpga must come first because its needed by drv_char
#if	FPGA_DEVICE_ENABLED==1
#include "fpga_char_bus.h"
#endif

// more driver...
#if	BUS_DEVICE_ENABLED==1
#include "drv_char_bus.h"
#endif

static int hess_smc_create(hess_dev_t* mydev)
{
	int err;
	//unsigned long csa;
	INFO("Initialize SMC\n");

	/* Create sysfs entries - on udev systems this creates the dev files */
	mydev->sysfs_class = class_create(THIS_MODULE, DRIVER_NAME);
	if (IS_ERR(mydev->sysfs_class))
	{
		err = PTR_ERR(mydev->sysfs_class);
		ERR("Error creating hessdrv class %d.\n", err);
		goto err_sysclass;
	}

	// Request Memory Region
	mydev->iomemregion = request_mem_region(SMC_MEM_START, SMC_MEM_LEN, IOMEM_NAME);
	if (!mydev->iomemregion)
	{
		ERR("could not request io-mem region for controlbus\n.");
		err = -1;
		goto err_memregion;
	}

	// Request remap of Memory Region
	mydev->mappedIOMem = ioremap_nocache(SMC_MEM_START, SMC_MEM_LEN);
	if (!mydev->mappedIOMem)
	{
		ERR("could not remap io-mem region for controlbus\n.");
		err = -2;
		goto err_ioremap;
	}
	mydev->IOMemSize = SMC_MEM_LEN;

	// activate smc chip select NCS
	//	at91_set_gpio_output(AT91_PIN_PC11, 1); // Activate as Output
	//	at91_set_B_periph(AT91_PIN_PC11, 0);    // Disable SPI0 CS Function
	//	at91_set_A_periph(AT91_PIN_PC11, 1);    // Activate NCS2 Function
	at91_set_A_periph(AT91_PIN_PC13, 1); // Activate NCS2 Function // HESS

	at91_set_A_periph(AT91_PIN_PC2, 1); // Activate A19 // HESS
	at91_set_A_periph(AT91_PIN_PC3, 1); // Activate A20 // HESS
	at91_set_A_periph(AT91_PIN_PC4, 1); // Activate A21 // HESS
	at91_set_A_periph(AT91_PIN_PC5, 1); // Activate A22 // HESS
	at91_set_A_periph(AT91_PIN_PC6, 1); // Activate A23 // HESS
	at91_set_A_periph(AT91_PIN_PC7, 1); // Activate A24 // HESS
	at91_set_A_periph(AT91_PIN_PC12, 1); // Activate A25 // HESS

	// configure board id pins
	at91_set_gpio_input(BOARD_ID_PIN0, 1); // Activate as input, pull up active
	at91_set_gpio_input(BOARD_ID_PIN1, 1); // Activate as input, pull up active

	// setup possible reconfiguration pin for PDB fpga
	at91_set_gpio_output(PDB_FPGA_nCONFIG, 1);
	setTestPin(PDB_FPGA_nCONFIG, SET);

	msleep(10); // ## wait for pullups to charge pin

	// read out board id
	__hess1u_board_id = at91_get_gpio_value(BOARD_ID_PIN0) ? 1 : 0;
	__hess1u_board_id |= at91_get_gpio_value(BOARD_ID_PIN1) ? 2 : 0;

	INFO("Board ID : %d\n", __hess1u_board_id);

	// Activate additional address lines needed for smc
	//    at91_set_gpio_output(AT91_PIN_PC5, 1); // Activate as Output
	//	at91_set_B_periph(AT91_PIN_PC5, 0);    // Disable SPI1_NPCS1 Function
	//	at91_set_A_periph(AT91_PIN_PC5, 1);    // Activate A24 Function

	//	at91_set_gpio_output(AT91_PIN_PC4, 1); // Activate as Output
	//	at91_set_B_periph(AT91_PIN_PC4, 0);    // Disable SPI1_NPCS2 Function
	//	at91_set_A_periph(AT91_PIN_PC4, 1);    // Activate A23 Function

	//	at91_set_gpio_output(AT91_PIN_PC10, 1); // Activate as Output
	//  at91_set_B_periph(AT91_PIN_PC10, 0);    // Disable UART_CTS3 Function
	//    at91_set_A_periph(AT91_PIN_PC10, 1);    // Activate A53 Function

	// activate ARM Bus Access and deactivate NIOS Bus access
	//	at91_set_gpio_output(CTRL_FPGA_ARM_BUS_ENABLE_PIN, 0); // Activate as Output and set level to GND

	// activate ARM Bus Access and deactivate NIOS Bus access

	//	at91_set_gpio_output(AT91_PIN_PB22, 0); // Activate as Output and set level to GND
	//	at91_set_gpio_output(AT91_PIN_PB23, 0); // Activate as Output and set level to GND
	//	at91_set_gpio_output(AT91_PIN_PB24, 0); // Activate as Output and set level to GND
	//	at91_set_gpio_output(AT91_PIN_PB25, 0); // Activate as Output and set level to GND
	//	at91_set_gpio_output(AT91_PIN_PB26, 0); // Activate as Output and set level to GND
	//	at91_set_gpio_output(AT91_PIN_PB27, 0); // Activate as Output and set level to GND
	//	at91_set_gpio_output(AT91_PIN_PB28, 0); // Activate as Output and set level to GND
	//	at91_set_gpio_output(AT91_PIN_PB29, 0); // Activate as Output and set level to GND
	//	at91_set_gpio_output(AT91_PIN_PB30, 0); // Activate as Output and set level to GND
	//	at91_set_gpio_output(AT91_PIN_PB31, 0); // Activate as Output and set level to GND

#if TEST_CODE==1
	// Test Controller Reconfigure
	// configure nCOnfig Pin
	at91_set_gpio_output(FPGA_nCONFIG, 0);// Activate as Output and set level to VCC
#endif
	// configure nCOnfig Pin
	at91_set_gpio_output(FPGA_nCONFIG, 1); // Activate as Output and set level to VCC

	// TODO: PB21-PB31 -> Directly Connected to FPGA, not used yet

	// Configure PB16, nCRC Error of controller (0==CRC ERROR)
	//	at91_set_gpio_input(CTRL_FPGA_NCRC_PIN, 0); // Activate as Input, no pullup

	// Configure PB17, nCRC Error of slaves (or'ed) (0==CRC ERROR)
	//	at91_set_gpio_input(SLAVES_FPGA_NCRC_PIN, 0); // Activate as Input, no pullup

	// apply the timing to
	ctrlbus_apply_timings();

	//	initTestPins();

	// wait until the timing configuration took effect, without waiting control bus operations will fail at this stage
	//msleep(500); // ## ??
	msleep(10); // ## ??

#if IRQ_ENABLED==1
	if (installSRQHandler())
	{
		ERR("could not install irq handler\n.");
		err = -3;
		goto err_irq_install;
	}
#endif
	//msleep(100); // ## ??

	// initialize semaphore for the device access
	sema_init(&mydev->sem, 1);

	return 0;

	// Error Handling
err_irq_install: iounmap(mydev->mappedIOMem);
err_ioremap: release_mem_region(SMC_MEM_START, SMC_MEM_LEN);
err_memregion: class_destroy(mydev->sysfs_class);

err_sysclass:

	return err;
}

static int hess_smc_destroy(hess_dev_t* mydev)
{
#if IRQ_ENABLED==1
	uninstallSRQHandler();
#endif
	iounmap(mydev->mappedIOMem);
	release_mem_region(SMC_MEM_START, SMC_MEM_LEN);
	class_destroy(mydev->sysfs_class);

	return 0;
}

// Initialization of the Character Device
static int hess_drv_create(hess_dev_t* mydev)
{
	int err;

	initTestPins();

	err = hess_smc_create(mydev);
	if (err)
	{
		ERR("error initialzing smc bus\n");
		return err;
	}

#if	BUS_DEVICE_ENABLED==1
	err = smc_chrdrv_create(mydev);
	if (err)
	{
		ERR("error initialzing smc char device\n");
		hess_smc_destroy(mydev);
		return err;
	}
#endif

#if	FPGA_DEVICE_ENABLED==1
	err = fpga_chrdrv_create(mydev);
	if (err)
	{
		ERR("error initialzing fpga char device\n");
		hess_smc_destroy(mydev);
		return err;
	}
#endif

#if IRQ_ENABLED==1
#if DAQ_DEVICE_ENABLED==1
	err = daq_chrdrv_create(mydev);
	if (err)
	{
		ERR("error initialzing daq char device\n");

#if	BUS_DEVICE_ENABLED==1
		smc_chrdrv_destroy(mydev);
#endif
		hess_smc_destroy(mydev);
		return err;
	}
#endif
#endif

	return 0;
}

static int hess_drv_destroy(hess_dev_t* mydev)
{
#if	DAQ_DEVICE_ENABLED==1
	daq_chrdrv_destroy(mydev);
#endif

#if	FPGA_DEVICE_ENABLED==1
	fpga_chrdrv_destroy(mydev);
#endif

#if	BUS_DEVICE_ENABLED==1
	smc_chrdrv_destroy(mydev);
#endif
	hess_smc_destroy(mydev);

	return 0;
}

#include "debugfs.h"

// Initialize the Kernel Module
static int __init hess_init(void)
{
	INFO("*** Hess Drawer - Driver Version '" DRIVER_VERSION " compiled at " __DATE__ " Time:" __TIME__ "' loaded ***\n");
#if TEST_CODE==1
	WRN("Test Code Activated! DO NOT USE THIS DRIVER with a productive system\n");
#endif

	int ret = 0;

	ret = hess_debugfs_init();
	if (ret != 0) return ret;

	ret = hess_drv_create(&my_device_instance);
	if (ret != 0) return ret;

	return ret;
}

// Deinitialize the Kernel Module
static void __exit hess_exit(void)
{
	hess_debugfs_remove(); // removing the directory recursively which in turn cleans all the files
	hess_drv_destroy(&my_device_instance);
	INFO("*** Hess Drawer - Version '" DRIVER_VERSION " compiled at " __DATE__ " Time:" __TIME__ "' unloaded ***\n");
}

module_init(hess_init);
module_exit(hess_exit);
