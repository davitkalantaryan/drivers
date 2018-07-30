#ifndef __pciedev_drv_dv_io_h__
#define __pciedev_drv_dv_io_h__

#include <linux/types.h>
#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */

#define PCIEDOOCS_IOC				'0'
#define PCIEDEV_PHYSICAL_SLOT		_IOWR(PCIEDOOCS_IOC, 60, int)
#define PCIEDEV_DRIVER_VERSION		_IOWR(PCIEDOOCS_IOC, 61, int)
#define PCIEDEV_FIRMWARE_VERSION	_IOWR(PCIEDOOCS_IOC, 62, int)
#define PCIEDEV_GET_DMA_TIME		_IOWR(PCIEDOOCS_IOC, 70, int)
#define PCIEDEV_READ_DMA			_IOWR(PCIEDOOCS_IOC, 72, int)

#define RW_INFO						0x4
#define RW_D8						0x0
#define RW_D16						0x1
#define RW_D32						0x2
#define PCIEDEV_DMA_SYZE			4096

/* generic register access */
typedef struct device_rw 
{
	u_int            offset_rw; /* offset in address                       */
	u_int            data_rw;   /* data to set or returned read data       */
	u_int            mode_rw;   /* mode of rw (RW_D8, RW_D16, RW_D32)      */
	u_int            barx_rw;   /* BARx (0, 1, 2, 3, 4, 5)                 */
	u_int            size_rw;   /* transfer size in bytes                  */             
	u_int            rsrvd_rw;  /* transfer size in bytes                  */
}device_rw;



typedef struct device_ioctrl_data 
{
	u_int    offset;
	u_int    data;
	u_int    cmd;
	u_int    reserved;
}device_ioctrl_data;



typedef struct device_ioctrl_time
{
	struct timeval   start_time;
	struct timeval   stop_time;
}device_ioctrl_time;



typedef struct device_ioctrl_dma
{
	u_int    dma_offset;
	u_int    dma_size;
	u_int    dma_cmd;          // value to DMA Control register
	u_int    dma_pattern;     // DMA BAR num
	u_int    dma_reserved1; // DMA Control register offset (31:16) DMA Length register offset (15:0)
	u_int    dma_reserved2; // DMA Read/Write Source register offset (31:16) Destination register offset (15:0)
}device_ioctrl_dma;


#endif/* #ifndef __pciedev_drv_dv_io_h__ */
