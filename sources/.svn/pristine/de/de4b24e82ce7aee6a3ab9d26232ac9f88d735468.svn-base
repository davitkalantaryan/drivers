/*
 *	File: mtcagen_io.h 
 *
 *	Created on: Apr 24, 2015
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *	This file contain definitions of ...
 *
 */
#ifndef __mtcagen_io_h__
#define __mtcagen_io_h__

//#define	FIRST_IRQ_TYPE		1
#define	GAINING_ACCESS		0
#define	RELEASING_ACCESS	1
#define	DEVICE_NAME_OFFSET	10 // * sizeof(int)
#define	SIGNAL_TO_INFORM	SIGURG

enum _IRQ_TYPES_{ _NO_IRQ_, _IRQ_TYPE1_, _IRQ_TYPE2_ };

// 1. First 2 for callback data
// 2. Second 2 for IRQ time
// 3. Third 2 for asking old data
#define _SIG_HEADER_OFFSET_INT_	DEVICE_NAME_OFFSET
#define _SIG_HEADER_OFFSET_		sizeof(int)*_SIG_HEADER_OFFSET_INT_

#include <linux/types.h>
#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */
#include "pciedev_io.h"

#define	_NUMBER_OF_RING_BUFFERS_				4
#define	__HEADER_SIZE__							256
#define	__OFSET_TO_BUFFER_INDEX__				4
#define	__OFSET_TO_EVENT_NUMBER__				8

#define	ONE_CHANNEL_SIZE(_a_number_of_samples)	((_a_number_of_samples)*2)
#define	DMA_TRANSFER_LEN(_a_number_of_samples)	(ONE_CHANNEL_SIZE(_a_number_of_samples)*_NUMBER_OF_CHANNELS_)
#define	ONE_BUFFER_SIZE(_a_dma_transfer_len)	((_a_dma_transfer_len) + __HEADER_SIZE__)
#define	WHOLE_MEMORY_SIZE(_a_one_buffer_size)	((_a_one_buffer_size)*_NUMBER_OF_RING_BUFFERS_ )

#define OFFSET_TO_BUFFER(_a_buffer_num, _a_one_buffer_size)				((_a_one_buffer_size)*(_a_buffer_num))
#define	_BUFFER_PTR_(_a_offset_to_buffer,_a_p_shared)					((char*)(_a_p_shared)+(_a_offset_to_buffer))
#define	EVENT_NUMBER_PTR(_a_offset_to_buffer,_a_p_shared)				(_BUFFER_PTR_(_a_offset_to_buffer,_a_p_shared)+__OFSET_TO_EVENT_NUMBER__)
#define	DMA_BUFFER_PTR(_a_offset_to_buffer,_a_p_shared)					(_BUFFER_PTR_(_a_offset_to_buffer,_a_p_shared)+__HEADER_SIZE__)
#define	CHANNEL_BUFFER_PTR(_a_offset_to_buffer,\
						_a_n_channel,_a_channel_size,_a_p_shared)		(DMA_BUFFER_PTR(_a_offset_to_buffer,_a_p_shared)+(a_n_channel)*(a_channel_size))

typedef struct mtcapci_dev
{
	struct pciedev_dev	upciedev;
}mtcapci_dev;


/* This structure is used to tell driver the registers and corresponding
data for interupt reseting */
typedef struct device_irq_reset  {
	u_int8_t	reg_size;		/* mode of rw (RW_D8, RW_D16, RW_D32)										*/
	u_int8_t	rw_access_mode;	/* 										*/
	int16_t		bar_rw;			/* bar of register(s). Range [0-5]. <0 or >5 means las one					*/
	u_int32_t	offset_rw;		/* offset in address														*/
	union
	{
		struct
		{
			u_int32_t	dataW;	/* data to write during interrupt											*/
			u_int32_t	maskW;	/* Mask for selecting needed bits to modify									*/
		};
		union
		{
			struct
			{
				u_int32_t	dataR;	/* do not provide from user space											*/
				u_int32_t	countR;	/* how many register should be read											*/
			};
			size_t			offsetInSigInfo; // do not provide from user space
		};
	};
}device_irq_reset;


/*typedef struct device_irq_reset_whole
{
	int32_t				m_nNumber;
	device_irq_reset*	m_pResetInfo;
}device_irq_reset_whole;*/


typedef struct device_irq_handling
{
	int16_t					is_irq_bar;
	u_int16_t				is_irq_offset;
	u_int16_t				is_irq_register_size;
	///////////////////////////////////////////////
	int16_t					resetRegsNumber;
	device_irq_reset*		resetRegsInfo;
	///////////////////////////////////////////////
	//u_int64_t				inform_user_type;
}device_irq_handling;


typedef struct SDeviceParams
{
	int32_t	vendor, device;
	int32_t	subsystem_vendor, subsystem_device;
	int32_t m_class, class_mask;

	int32_t		slot_num;
	char		temp[sizeof(int32_t)]; // For correct padding 
}SDeviceParams;
typedef SDeviceParams DriverIDparams;

typedef struct SHotPlugRegister
{
	struct SDeviceParams	device;
	pointer_type			user_callback;
}SHotPlugRegister;


///////////////////////////////////////////////////////////////
#define	MTCAGEN_IOC							'p'

//#define GEN_REQUEST_IRQ_FOR_DEV				_IOW(MTCAGEN_IOC,20, struct device_irq_handling)
#define ADC_NUMBER_OF_SAMPLES				_IO2('s',56)
#define ADC_WAIT_FOR_DMA_INF				_IO2('s',57)
#define ADC_WAIT_FOR_DMA_TIMEOUT			_IOW2('s',58, int)
#define GEN_REQUEST_IRQ1_FOR_DEV			_IOW(MTCAGEN_IOC,21, struct device_irq_handling)
#define GEN_REQUEST_IRQ2_FOR_DEV			_IOW(MTCAGEN_IOC,22, struct device_irq_handling)
#define GEN_UNREQUEST_IRQ_FOR_DEV			_IO(MTCAGEN_IOC,30)
#define GEN_REGISTER_USER_FOR_IRQ			_IOW(MTCAGEN_IOC,31, int)
#define GEN_UNREGISTER_USER_FROM_IRQ		_IO(MTCAGEN_IOC,32)
#define MTCA_WAIT_FOR_IRQ2_INF				_IO(MTCAGEN_IOC,33)
#define MTCA_WAIT_FOR_IRQ2_TIMEOUT			_IOW(MTCAGEN_IOC,34, int)
#define MTCA_GET_IRQ_TYPE					_IO2(MTCAGEN_IOC,35)
#define MTCA_SET_ADC_TIMER					_IOW2(MTCAGEN_IOC,36,const char*)
#define MTCA_SET_ADC_GEN_EVNT_SRC			_IOW2(MTCAGEN_IOC,37,const char*)

#define MTCAGEN_IOC_MINNR					20
#define MTCAGEN_IOC_MAXNR					50


////////////////////////////////////////////////////////////////////////////////////////////////
#define	MTCAMANAGEMENT_IOC					'm'

#define MTCA_GAIN_ACCESS				_IOW(MTCAMANAGEMENT_IOC,51, struct SDeviceParams)
#define MTCA_RELASE_ACCESS				_IOW(MTCAMANAGEMENT_IOC,52, struct SDeviceParams)
#define MTCA_REMOVE_PCI_ID				_IOW(MTCAMANAGEMENT_IOC,53, struct SDeviceParams)
#define MTCA_REMOVE_ALL					_IO(MTCAMANAGEMENT_IOC,54)
#define MTCA_REGISTER_FOR_HOT_PLUG		_IOW(MTCAMANAGEMENT_IOC,55, struct SHotPlugRegister)
#define MTCA_UNREGISTER_FROM_HOT_PLUG	_IOW(MTCAMANAGEMENT_IOC,56, struct SDeviceParams)
#define MTCA_UNREGISTER_FROM_ALL		_IO(MTCAMANAGEMENT_IOC,58)

#define MTCAMAN_IOC_MINNR				50
#define MTCAMAN_IOC_MAXNR				80



#endif  /* #ifndef __mtcagen_io_h__ */
