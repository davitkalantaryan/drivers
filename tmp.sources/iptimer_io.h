/*
 *	File: iptimer_io.h
 *
 *	Created on: Sep 15, 2015
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *
 */
#ifndef __iptimer_io_h__
#define __iptimer_io_h__

#define TAMC200_VENDOR_ID    0x10B5	/* TEWS vendor ID */
#define TAMC200_DEVICE_ID    0x9030	/* IP carrier board device ID */
#define TAMC200_SUBVENDOR_ID 0x1498	/* TEWS vendor ID */
#define TAMC200_SUBDEVICE_ID 0x80C8	/* IP carrier board device ID */

#include "mtcagen_io.h"


typedef struct STimeTelegram
{
	u_int32_t	gen_event;
	u_int32_t	seconds;	// starting from epoch
	u_int32_t	useconds;	// microseconds starting from epoch
}STimeTelegram;


typedef struct STimingTest
{
	u_int64_t	iterations;
	u_int64_t	total_time;
	u_int64_t	current_time;
}STimingTest;

#define	TAMC200_NR_SLOTS								3
#define	IP_TIMER_BUFFER_HEADER_SIZE						16
#define	IP_TIMER_RING_BUFFERS_COUNT						4
#define	IP_TIMER_ONE_BUFFER_SIZE						16
#define	IP_TIMER_WHOLE_BUFFER_SIZE						(IP_TIMER_BUFFER_HEADER_SIZE + \
														(IP_TIMER_RING_BUFFERS_COUNT*IP_TIMER_ONE_BUFFER_SIZE))

#define	_OFFSET_TO_SPECIFIC_BUFFER_(_a_buffer_)			(IP_TIMER_BUFFER_HEADER_SIZE + \
														(_a_buffer_)*IP_TIMER_ONE_BUFFER_SIZE)
#define	OFFSET_TO_CURRENT_BUFFER(_a_mem_)				_OFFSET_TO_SPECIFIC_BUFFER_(*((int*)(_a_mem_)))
#define	POINTER_TO_CURRENT_BUFFER(_a_mem_)				((char*)_a_mem_ + _OFFSET_TO_SPECIFIC_BUFFER_(*((int*)(_a_mem_))))


#define	TAMC200_IOC										't'

#define	IP_TIMER_ACTIVATE_INTERRUPT_PRVT				52 /// Piti poxvi

//////////////////////////////////////////////////////////////////////////
#define IP_TIMER_TEST1									_IOWR(TAMC200_IOC,51, int)
#define IP_TIMER_TEST_TIMING							_IOR(TAMC200_IOC,53, struct STimingTest)
#define IP_TIMER_ACTIVATE_INTERRUPT						_IOWR(TAMC200_IOC,IP_TIMER_ACTIVATE_INTERRUPT_PRVT, int)
#define IP_TIMER_WAIT_FOR_EVENT_INF						MTCA_WAIT_FOR_IRQ2_INF
#define IP_TIMER_WAIT_FOR_EVENT_TIMEOUT					MTCA_WAIT_FOR_IRQ2_TIMEOUT

#endif  /* #ifndef __iptimer_io_h__ */
