/*
* nectaradc.h
*
*  Created on: May 28, 2014
*      Author: kossatz
*/

#ifndef NECTARADC2_H_
#define NECTARADC2_H_

// nectar adc read
#define BASE_NECTAR_ADC							0x0000

#define OFFS_ISR_HANDSHAKE						0x0000	//  ,ro
#define BIT_ISR_HANDSHAKE_START				0
#define BIT_ISR_HANDSHAKE_STOP					1
#define OFFS_EVENT_FIFO_WORD_COUNT				0x0002	//  ,ro
#define OFFS_NUMBER_OF_SAMPLES_TO_READ			0x0004	//  ,rw
//#define OFFS_BUFFER_DISPLACE					0x0006	//  ,rw  ## ,ro timeoutcounter
#define OFFS_PROC_MODE							0x0008	//  2Bit,rw
#define OFFS_DRAWER_CONTROL_STATE_ACTUAL		0x000a	//  2bit,ro
#define OFFS_DRAWER_CONTROL_STATE_REQUEST		0x000c	//  2Bit,rw
//#define OFFS_NECTAR_TEST_PULSE					0x000e	//  2Bit,rw
#define OFFS_EVENTS_PER_IRQ						0x0010	//  8bit,rw // function disabled if 0x0
#define OFFS_RESET_FIFOS						0x0012	//  2bit,wo
#define OFFS_FAIL_SAVE_TIMEOUT					0x0014	// 16bit,rw
#define OFFS_ADC_VALID_DELAY					0x0016	//  8bit,rw
#define OFFS_FRONT_END_FIFO_WORD_COUNT			0x0018	// 11bit,ro
#define OFFS_IRQ_AT_FRONT_END_FIFO_WORD_COUNT	0x001a	// 16bit,rw // function disabled if 0x0
#define OFFS_IRQ_AT_EVENT_FIFO_WORD_COUNT		0x001c	// 16bit,rw // function disabled if 0x0
#define OFFS_FIFO_FULL_COUNT					0x001e	// 12bit,ro
#define OFFS_TRIGGER_COUNTER_CONTROL			0x0020	// 16bit,rw
#define OFFS_TRIGGER_COUNTER_RESET_TIME			0x0022	// 16bit,rw //  1 tick = 0,524288ms -- 0x773 = 1907 = 1sec
#define TRIGGER_COUNTER_RESET_TIME_SEC	1907 // value
#define OFFS_TRIGGER_COUNTER_L					0x0024	// 16bit,ro
#define OFFS_TRIGGER_COUNTER_H					0x0026	//  8bit,ro

#define OFFS_EVENT_FIFO_READ_STROBE		0x007e		//  ,ro
#define OFFS_EVENT_FIFO					0x0080		//  ,ro
//#define OFFS_EVENT_FIFO_RAW_DATA_BEGIN	0x0080		//  ,ro
#define OFFS_EVENT_FIFO_RAW_DATA_END	0x00c2		//  ,ro (last valid data on 0x00c0)

#define OFFS_FRONT_END_FIFO					0x0080		//  ,ro
#define OFFS_FRONT_END_FIFO_RAW_DATA_BEGIN	0x0082		//  ,ro
#define OFFS_FRONT_END_FIFO_RAW_DATA_END	0x00c2		//  ,ro (last valid data on 0x00c0)
#define OFFS_FRONT_END_FIFO_READ_STROBE		0x00c0		//  ,ro



#endif /* NECTARADC2_H_ */
