#ifndef _TIMER_IO_H
#define _TIMER_IO_H

#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */

#define IPTIMER_IRQ_ALL       		0xFFFF
#define IPTIMER_IRQ_EVA       		0x0100
#define IPTIMER_IRQ_EVB       		0x0200
#define IPTIMER_IRQ_EVC       		0x0400
#define IPTIMER_IRQ_EVD       		0x0800
#define IPTIMER_IRQ_EVE       		0x1000
#define IPTIMER_IRQ_EVF       		0x2000
#define IPTIMER_IRQ_EVG       		0x4000
#define IPTIMER_IRQ_EVH       		0x8000
#define IPTIMER_IRQ_CH0       		0x0001
#define IPTIMER_IRQ_CH1       		0x0002
#define IPTIMER_IRQ_CH2       		0x0004
#define IPTIMER_IRQ_CH3       		0x0008
#define IPTIMER_IRQ_CH4       		0x0010
#define IPTIMER_IRQ_CH5       		0x0020
#define IPTIMER_IRQ_CH6       		0x0040
#define IPTIMER_IRQ_CH7       		0x0080
#define SIG1        0
#define SIG2        1
#define URG         2
#define MSK_WANT    3
#define CUR_SIG     4

#define MAX_LINES   20
#define IPTIMER_OK	0

struct iptimer_inout{
       u_int    	board;
       u_int    	element_nr;
       u_short  	data;
};
typedef struct iptimer_inout iptimer_inout; 

struct iptimer_reg_data{
       u_short        data [64];
};
typedef struct iptimer_reg_data t_iptimer_reg_data; 

struct iptimer_ioctl_data{
       u_short		f_Irq_Mask;
       u_short		f_Irq_Sig;
       u_short		f_Status;
       
};
typedef struct iptimer_ioctl_data t_iptimer_ioctl_data;

struct iptimer_ioctl_time{
       int             f_Irq_Sec;
       int             f_Irq_uSec;
       u_short         f_Irq_Mask;
       u_short         f_Irq_Flag;
       u_short         f_Irq_Sts;
       
};
typedef struct iptimer_ioctl_time t_iptimer_ioctl_time;

struct iptimer_ioctl_event  {
       int              f_Irq_Sec;
       int              f_Irq_uSec;
       int              f_Irq_Delay;
       u_short		f_Irq_Mask;
       u_short		f_Irq_Flag;
       u_short		f_Irq_Sts;
       u_short		f_event_low;
       u_short		f_event_high;
};
typedef struct iptimer_ioctl_event t_iptimer_ioctl_event;

struct iptimer_ioctl_rw{
       u_short		io_chn;
       u_short		io_data;
       u_short		io_rw;
};
typedef struct iptimer_ioctl_rw t_iptimer_ioctl_rw;

/*
 * Ioctl definitions
 */

/* Use 'k' as magic number */
#define IPTIMER_IOC                       'k'

#define IPTIMER_REFRESH 		_IO(IPTIMER_IOC, 0)
#define IPTIMER_ADD_SERVER_SIGNAL	_IOWR(IPTIMER_IOC, 1,  int)
#define IPTIMER_DEL_SERVER_SIGNAL	_IOWR(IPTIMER_IOC, 2,  int)
#define IPTIMER_GET_STATUS		_IOWR(IPTIMER_IOC, 3,  int)
#define IPTIMER_READ_IRQ 		_IOWR(IPTIMER_IOC, 4,  int)
#define IPTIMER_SET_MASK 		_IOWR(IPTIMER_IOC, 5,  int)
#define IPTIMER_SET_IRQ 		_IOWR(IPTIMER_IOC, 6,  int)
#define IPTIMER_DELAY 			_IOWR(IPTIMER_IOC, 7,  int)
#define IPTIMER_EVENT 			_IOWR(IPTIMER_IOC, 8,  int)
#define IPTIMER_DAISY_CHAIN 		_IOWR(IPTIMER_IOC, 9,  int)
#define IPTIMER_TIME_BASE 		_IOWR(IPTIMER_IOC, 10, int)
#define IPTIMER_PULSE_LENGTH 		_IOWR(IPTIMER_IOC, 11, int)
#define IPTIMER_OUT 			_IOWR(IPTIMER_IOC, 12, int)
#define IPTIMER_VARCLOCK 		_IOWR(IPTIMER_IOC, 13, int)
#define IPTIMER_GET_REG 		_IOWR(IPTIMER_IOC, 14, int)
#define IPTIMER_READ_IRQ_TM 		_IOWR(IPTIMER_IOC, 15, int)
#define IPTIMER_GET_CLCK_STS 		_IOWR(IPTIMER_IOC, 16, int)
#define IPTIMER_GET_LOC_REG 		_IOWR(IPTIMER_IOC, 17, int)
#define IPTIMER_CHECK_IRQ 		_IOWR(IPTIMER_IOC, 18, int)
#define IPTIMER_CHECK_SVR 		_IOWR(IPTIMER_IOC, 19, int)
#define IPTIMER_REM_SVR 		_IOWR(IPTIMER_IOC, 20, int)
#define IPTIMER_RAM_SET 		_IOWR(IPTIMER_IOC, 21, int)
#define IPTIMER_GET_EVENT 		_IOWR(IPTIMER_IOC, 22, int)
#define IPTIMER_EVENT_CHECK 	        _IOWR(IPTIMER_IOC, 23, int)
#define IPTIMER_EVENT_CHANNEL 	        _IOWR(IPTIMER_IOC, 24, int)
#define IPTIMER_JITTER_TEST 	        _IOWR(IPTIMER_IOC, 25, int)
#define IPTIMER_EVENT_INFO              _IOWR(IPTIMER_IOC, 26, int)
#define IPTIMER_DRIVER_VERSION          _IOWR(IPTIMER_IOC, 27, int)
#define IPTIMER_IRQ_DEALY               _IOWR(IPTIMER_IOC, 28, int)
#define IPTIMER_IOC_MAXNR               28

#endif
