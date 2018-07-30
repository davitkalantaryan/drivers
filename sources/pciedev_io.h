#ifndef _PCIEDEV_IO_H
#define _PCIEDEV_IO_H

#include <linux/types.h>
#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */

#define	IRQ_INF_RING_SIZE	4

#ifndef NUMBER_OF_BARS
#define NUMBER_OF_BARS 6
#endif

#define	UPCIEDEV_SWAPL(x) ((((x) >> 24) & 0x000000FF) | (((x) >> 8) & 0x0000FF00) | (((x) << 8) & 0x00FF0000) | (((x) << 24) & 0xFF000000))
#define	UPCIEDEV_SWAPS(x) ((((x) >> 8) & 0x00FF) | (((x) << 8) & 0xFF00))
#define UPCIEDEV_SWAP_TOT(size,x) ((size)==1) ? (x) : (((size)==2) ? UPCIEDEV_SWAPS((x)) : UPCIEDEV_SWAPL((x)))

#define		PRW_REG_SIZE_MASK	(int64_t)0xf000000000000000
#define		PRW_BAR_MASK		(int64_t)0x0f00000000000000
#define		PRW_OFFSET_MASK		(int64_t)0x00ffffffffffffff
#define		PRW_REG_SIZE_SHIFT	60
#define		PRW_BAR_SHIFT		56

#define	WRONG_MODE				119

// Register size
#define RW_D8					0x0
#define RW_D08					RW_D8
#define RW_D16					0x1
#define RW_D32					0x2
#define MTCA_MAX_RW				3
#define RW_DMA					0x3
#define RW_INFO					0x4

#define	__GetRegisterSizeInBytes__(_a_value_)	(1<<(_a_value_))

// All possible device access types
#define	MTCA_SIMPLE_READ		0
#define	MTCA_LOCKED_READ		1
#define	MT_READ_TO_EXTRA_BUFFER	2
////////////////////////////////////
#define	MTCA_SIMPLE_WRITE		5
#define	MTCA_SET_BITS			6
#define	MTCA_SWAP_BITS			7

#define DMA_DATA_OFFSET			6 
#define DMA_DATA_OFFSET_BYTE	24
#define PCIEDEV_DMA_SYZE		4096
#define PCIEDEV_DMA_MIN_SYZE	128

#define IOCTRL_R				0x00
#define IOCTRL_W				0x01
#define IOCTRL_ALL				0x02

typedef u_int64_t pointer_type;

#define	CORRECT_REGISTER_SIZE(_a_rw_mode_,_a_default_rw_mode_) \
{ \
	if((_a_rw_mode_)<0 || (_a_rw_mode_)>= MTCA_MAX_RW){(_a_rw_mode_)=(_a_default_rw_mode_);}\
}


/*
 * GetRegisterSizeInBytes:  gets register size from RW mode
 *                          safe to call from interrupt contex
 *
 * Parameters:
 *   a_unMode:        register access mode (read or write and register len (8,16 or 32 bit))
 *
 * Return (int):
 *   WRONG_MODE:      error (not correct mode provided)
 *   other:           register length in Bytes
 */
static inline int	GetRegisterSizeInBytes(int16_t a_unMode, int16_t a_default_rw_mode)
{
	CORRECT_REGISTER_SIZE(a_unMode, a_default_rw_mode);
	return __GetRegisterSizeInBytes__(a_unMode);
}



/* generic register access */
struct device_rw  {
	u_int32_t				offset_rw;	/* offset in address                       */
	u_int32_t				data_rw;	/* data to set or returned read data       */
	union
	{
		u_int32_t			mode_rw;	/* mode of rw (RW_D8, RW_D16, RW_D32)      */
		u_int32_t			register_size; /* (RW_D8, RW_D16, RW_D32)      */
	};
	u_int32_t				barx_rw;	/* BARx (0, 1, 2, 3, 4, 5)                 */
	union
	{
		struct
		{
			u_int32_t		size_rw;	// !!! transfer size should not be providefd by this field.
										// This field is there for backward compatibility.
										//
										// Transfer size should be provided with read/write 3-rd
										// argument (count)
										// read(int fd, void *buf, size_t count);
										// ssize_t write(int fd, const void *buf, size_t count);
			u_int32_t		rsrvd_rw;
		};
		pointer_type		dataPtr;	// In the case of more than one register access
										// this fields shows pointer to user data
	};
};
typedef struct device_rw device_rw;


/* generic register access from ioctl system call*/
typedef struct device_ioc_rw 
{
	u_int16_t		register_size;	/* (RW_D8, RW_D16, RW_D32)      */
	u_int16_t		rw_access_mode;	/* mode of rw (MTCA_SIMLE_READ,...)      */
	u_int32_t		barx_rw;	/* BARx (0, 1, 2, 3, 4, 5)                 */
	u_int32_t		offset_rw;	/* offset in address                       */
	u_int32_t		count_rw;	/* number of register to read or write     */
	pointer_type	dataPtr;	// In the case of more than one register access
	pointer_type	maskPtr;	// mask for bitwise write operations
}device_ioc_rw;


/* generic vector register access from ioctl system call*/
typedef struct device_vector_rw
{
	u_int64_t		number_of_rw;		/* Number of read or write operations to perform     */
	pointer_type	device_ioc_rw_ptr;	// User space pointer to read or write data    
										// In kernel space this field should be casted to (device_ioc_rw* __user)
}device_vector_rw;


struct device_ioctrl_data  {
	u_int32_t    offset;
	u_int32_t    data;
	u_int32_t    cmd;
	u_int32_t    reserved;
};
typedef struct device_ioctrl_data device_ioctrl_data;

struct device_ioctrl_dma  {
	u_int32_t    dma_offset;
	u_int32_t    dma_size;
	u_int32_t    dma_cmd;          // value to DMA Control register
	u_int32_t    dma_pattern;     // DMA BAR num
	u_int32_t    dma_reserved1; // DMA Control register offset (31:16) DMA Length register offset (15:0)
	u_int32_t    dma_reserved2; // DMA Read/Write Source register offset (31:16) Destination register offset (15:0)
};
typedef struct device_ioctrl_dma device_ioctrl_dma;

struct device_ioctrl_time  {
	struct timeval   start_time;
	struct timeval   stop_time;
};
typedef struct device_ioctrl_time device_ioctrl_time;

struct device_i2c_rw  {
	u_int32_t            busNum; /* I2C Bus num*/
	u_int32_t            devAddr;   /* I2C device address on the current bus */
	u_int32_t            regAddr;   /* I2C register address on the current device */
	u_int32_t            size;   /* number of bytes to  read/write*/
	u_int32_t            done;  /* read done*/
	u_int32_t            timeout;   /* transfer timeout usec */
	u_int32_t            status;  /* status */
	u_int32_t            data[256];  /* data */
};
typedef struct device_i2c_rw device_i2c_rw;

/* Use 'o' as magic number */
#define PCIEDOOCS_IOC                               '0'
#define PCIEDEV_PHYSICAL_SLOT            _IOWR(PCIEDOOCS_IOC, 60, int)
#define PCIEDEV_DRIVER_VERSION          _IOWR(PCIEDOOCS_IOC, 61, int)
#define PCIEDEV_FIRMWARE_VERSION    _IOWR(PCIEDOOCS_IOC, 62, int)
#define PCIEDEV_SCRATCH_REG              _IOWR(PCIEDOOCS_IOC, 63, int)
#define PCIEDEV_GET_STATUS                 _IOWR(PCIEDOOCS_IOC, 64, int)
#define PCIEDEV_SET_SWAP                     _IOWR(PCIEDOOCS_IOC, 65, int)
#define PCIEDEV_SET_SWAP2                     _IOWR(PCIEDOOCS_IOC, 66, int)
#define PCIEDEV_GET_SWAP2                     _IOWR(PCIEDOOCS_IOC, 67, int)
#define PCIEDEV_GET_DMA_TIME             _IOWR(PCIEDOOCS_IOC, 70, int)
#define PCIEDEV_WRITE_DMA                  _IOWR(PCIEDOOCS_IOC, 71, int)
#define PCIEDEV_READ_DMA                    _IOWR(PCIEDOOCS_IOC, 72, int)
#define PCIEDEV_SET_IRQ                         _IOWR(PCIEDOOCS_IOC, 73, int)
#define PCIEDEV_I2C_READ                     _IOWR(PCIEDOOCS_IOC, 74, int)
#define PCIEDEV_I2C_WRITE                   _IOWR(PCIEDOOCS_IOC, 75, int)


/////////////////////////////////////////////////////////////////////////////////////
////////////////////////  New ioctl calls  //////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
#ifndef WIN32
#define	_IO2	_IO
#define	_IOR2	_IOR
#define	_IOW2	_IOW
#endif

#define	PCIE_GEN_IOC					PCIEDOOCS_IOC

#define PCIEDEV_LOCK_DEVICE				_IO2(PCIE_GEN_IOC, 30)
#define PCIEDEV_UNLOCK_DEVICE			_IO2(PCIE_GEN_IOC, 29)
#define PCIEDEV_SET_BITS				_IOW2(PCIE_GEN_IOC, 31, struct device_ioc_rw)
#define PCIEDEV_SWAP_BITS				_IOW2(PCIE_GEN_IOC, 34, struct device_ioc_rw)
#define PCIEDEV_LOCKED_READ				_IOWR(PCIE_GEN_IOC, 32, struct device_ioc_rw)
#define PCIEDEV_VECTOR_RW				_IOWR(PCIE_GEN_IOC, 33, struct device_vector_rw)
#define PCIEDEV_SET_REGISTER_SIZE		_IOW2(PCIE_GEN_IOC, 35, int)
#define PCIEDEV_GET_REGISTER_SIZE		_IO2(PCIE_GEN_IOC, 36)
#define PCIEDEV_SINGLE_IOC_ACCESS		_IOWR(PCIE_GEN_IOC, 37, struct device_ioc_rw)
#define PCIEDEV_GET_SLOT_NUMBER			_IO2(PCIE_GEN_IOC, 38)
/////////////////////////////////////////////////////////////////////////////////////
////////////////////////  End new ioctl calls  //////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

#define PCIEDOOCS_IOC_MINNR			28
#define PCIEDOOCS_IOC_MAXNR			66

#define PCIEDEV_IOC_COMMON_MINNR	70
#define PCIEDEV_IOC_COMMON_MAXNR	76
#define PCIEDOOCS_IOC_DMA_MINNR		70
#define PCIEDOOCS_IOC_DMA_MAXNR		76

#endif
