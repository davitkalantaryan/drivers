/*
 *  read_write_inline.h
 *
 *  Created on: Jul 07, 2015
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *  Device register read-write functions are implemented in this file.
 *  These functions can be used to implemet read/write/ioctl system calls
 *  Function "Read_Write_Private" can be used also in interrupt context
 *
 *
 */
#ifndef __read_write_inline_h__
#define __read_write_inline_h__

#include "pciedev_io.h"
#include "pciedev_ufn.h"
//#include <asm-generic/io.h>
#include "debug_functions.h"

#define		DEBUG_TIMING

#ifdef DEBUG_TIMING
//#define	_SMB_RMB_	smp_rmb
//#define	_SMB_WMB_	smp_wmb
#endif


/*
 * Read_Write_Private:  read or write one register
 *                      safe to call from interrupt contex
 *
 * Parameters:
 *   a_unMode:        register access mode (read or write and register len (8,16 or 32 bit))
 *   a_addressDev:    io mapped device register address
 *   a_pValueKernel:  address of variable to write to device or store register value
 *   a_pMaskW:        address of mask for biswise write operations
 *   a_pExtraR:       kernel space buffer address, that is used only in the case
 *                    of interrupt handling (for details see documentation)
 * Return (void):
 *  no return!!!
 */
static inline void Read_Write_Private(u_int16_t a_usnRegSize, u_int16_t a_usnAccessMode, void* a_addressDev,
							void* a_pValueKernel, void* a_pMaskW, void* a_pExtraR)
{
	u32		tmp_data;
	u32		tmp_data_Prev;
	u32		tmp_data_Mask;

#if 0
	// All possible device access types
#define	MTCA_SIMLE_READ			0
#define	MTCA_LOCKED_READ		1
#define	MT_READ_TO_EXTRA_BUFFER	2
	////////////////////////////////////
#define	MTCA_SIMPLE_WRITE		5
#define	MTCA_SET_BITS			6
#define	MTCA_SWAP_BITS			7
#endif

	switch (a_usnRegSize)
	{
	case RW_D08:
		switch (a_usnAccessMode)
		{
		case MTCA_SIMPLE_WRITE:
			iowrite8(*((const u8*)a_pValueKernel), a_addressDev);
			smp_wmb();
			return;
		case MTCA_SET_BITS:
			tmp_data = (u32)(*((const u8*)a_pValueKernel));
			tmp_data_Mask = (u32)(*((const u8*)a_pMaskW));

			tmp_data &= tmp_data_Mask;				// Keeping fields corresponding to Mask

			tmp_data_Prev = ioread8(a_addressDev);
			smp_rmb();
			tmp_data_Mask = ~tmp_data_Mask;			// Inverting Mask
			tmp_data_Prev &= tmp_data_Mask;			// Keeping fields corresponding to inverted Mask
			tmp_data |= tmp_data_Prev;				// Concatenating old and new datas

			iowrite8((u8)tmp_data, a_addressDev);	// Writing data to device
			smp_wmb();
			return;
		case MTCA_SWAP_BITS:
			tmp_data_Prev = ioread8(a_addressDev);
			smp_rmb();
			tmp_data = ~tmp_data_Prev;

			tmp_data_Mask = (u32)(*((const u8*)a_pMaskW));
			tmp_data &= tmp_data_Mask;				// Keeping fields corresponding to Mask

			tmp_data_Mask = ~tmp_data_Mask;			// Inverting Mask
			tmp_data_Prev &= tmp_data_Mask;			// Keeping fields corresponding to inverted Mask
			tmp_data |= tmp_data_Prev;				// Concatenating old and new datas

			iowrite8((u8)tmp_data, a_addressDev);	// Writing data to device
			smp_wmb();
			return;
		case MT_READ_TO_EXTRA_BUFFER:
			a_pValueKernel = a_pExtraR;
		default:
			*((u8*)a_pValueKernel) = ioread8(a_addressDev);
			smp_rmb();
			return;
		}
		break;
	case RW_D16:
		switch (a_usnAccessMode)
		{
		case MTCA_SIMPLE_WRITE:
			iowrite16(*((const u16*)a_pValueKernel), a_addressDev);
#ifndef _SMB_WMB_
			smp_wmb();
#endif
			return;
		case MTCA_SET_BITS:
			tmp_data = (u32)(*((const u16*)a_pValueKernel));
			tmp_data_Mask = (u32)(*((const u16*)a_pMaskW));

			tmp_data &= tmp_data_Mask;				// Keeping fields corresponding to Mask

			tmp_data_Prev = ioread16(a_addressDev);
			smp_rmb();
			tmp_data_Mask = ~tmp_data_Mask;			// Inverting Mask
			tmp_data_Prev &= tmp_data_Mask;			// Keeping fields corresponding to inverted Mask
			tmp_data |= tmp_data_Prev;				// Concatenating old and new datas

			iowrite16((u16)tmp_data, a_addressDev);	// Writing data to device
			smp_wmb();
			return;
		case MTCA_SWAP_BITS:
			tmp_data_Prev = ioread16(a_addressDev);
			smp_rmb();
			tmp_data = ~tmp_data_Prev;

			tmp_data_Mask = (u32)(*((const u16*)a_pMaskW));
			tmp_data &= tmp_data_Mask;				// Keeping fields corresponding to Mask

			tmp_data_Mask = ~tmp_data_Mask;			// Inverting Mask
			tmp_data_Prev &= tmp_data_Mask;			// Keeping fields corresponding to inverted Mask
			tmp_data |= tmp_data_Prev;				// Concatenating old and new datas

			iowrite16((u16)tmp_data, a_addressDev);	// Writing data to device
			smp_wmb();
			return;
		case MT_READ_TO_EXTRA_BUFFER:
			a_pValueKernel = a_pExtraR;
		default:
			*((u16*)a_pValueKernel) = ioread16(a_addressDev);
#ifndef _SMB_RMB_
			smp_rmb();
#endif
			return;
		}
		break;
	default:
		switch (a_usnAccessMode)
		{
		case MTCA_SIMPLE_WRITE:
			iowrite32(*((const u32*)a_pValueKernel), a_addressDev);
			smp_wmb();
			return;
		case MTCA_SET_BITS:
			tmp_data = (u32)(*((const u32*)a_pValueKernel));
			tmp_data_Mask = (u32)(*((const u32*)a_pMaskW));

			tmp_data &= tmp_data_Mask;				// Keeping fields corresponding to Mask

			tmp_data_Prev = ioread32(a_addressDev);
			smp_rmb();
			tmp_data_Mask = ~tmp_data_Mask;			// Inverting Mask
			tmp_data_Prev &= tmp_data_Mask;			// Keeping fields corresponding to inverted Mask
			tmp_data |= tmp_data_Prev;				// Concatenating old and new datas

			iowrite32((u32)tmp_data, a_addressDev);	// Writing data to device
			smp_wmb();
			return;
		case MTCA_SWAP_BITS:
			tmp_data_Prev = ioread32(a_addressDev);
			smp_rmb();
			tmp_data = ~tmp_data_Prev;

			tmp_data_Mask = (u32)(*((const u32*)a_pMaskW));
			tmp_data &= tmp_data_Mask;				// Keeping fields corresponding to Mask

			tmp_data_Mask = ~tmp_data_Mask;			// Inverting Mask
			tmp_data_Prev &= tmp_data_Mask;			// Keeping fields corresponding to inverted Mask
			tmp_data |= tmp_data_Prev;				// Concatenating old and new datas

			iowrite32((u32)tmp_data, a_addressDev);	// Writing data to device
			smp_wmb();
			return;
		case MT_READ_TO_EXTRA_BUFFER:
			a_pValueKernel = a_pExtraR;
		default:
			*((u32*)a_pValueKernel) = ioread32(a_addressDev);
			smp_rmb();
			return;
		}//switch (a_usnAccessMode)
		break;
	}//switch (a_usnRegSize)
}


static inline int pciedev_check_scratch(struct pciedev_dev* dev);

/*
 * pciedev_read_inline:  reads data from a_count device registers to user buffer
 *
 * Parameters:
 *   a_dev:           device structure to handle
 *   a_mode:          register access mode (read or write and register len (8,16 or 32 bit))
 *   a_bar:           bar to access (0, 1, 2, 3, 4 or 5)
 *   a_offset:        first register offset
 *   a_pcUserBuf:     pointer to user buffer for storing data
 *   a_count:         number of device register to read
 *
 * Return (int):
 *   < 0:             error (details on all errors on documentation)
 *   other:           number of Bytes read from device registers
 */
static inline ssize_t pciedev_read_inline(struct pciedev_dev* a_dev, u_int16_t a_register_size, u_int16_t a_rw_access_mode, 
				u_int32_t a_bar, u_int32_t a_offset, char __user *a_pcUserBuf0, size_t a_count)
{
	char __user*		pcUserBuf = a_pcUserBuf0;
	ssize_t				retval = 0;
	u32					total_size_rw;
	u32					size_after_offset_rw;
	u32					cnt_after_offset_rw;
	int					i;
	int					scrtch_check = 0;
	u32					tmp_data_for_rw;
	u32					scrachTest;
	/*const */char*		addressDev;
	u_int32_t			unRegisterSize;

	unRegisterSize = GetRegisterSizeInBytes(a_register_size,a_dev->register_size);
	scrachTest = unRegisterSize == 1 ? 0xff : (unRegisterSize == 2 ? 0xffff : 0xffffffff);
	//maskForScrach = scrachTest;

	if (!a_dev->memmory_base[a_bar] || a_offset%unRegisterSize)
	{
		ERRCT("ERROR: memor_base[%u]=%p, offset=%u, registerSize=%u\n", a_bar, a_dev->memmory_base[a_bar], a_offset, unRegisterSize);
		return -EFAULT;
	}

	total_size_rw = a_dev->mem_base_end[a_bar] - a_dev->mem_base[a_bar];
	size_after_offset_rw = (total_size_rw>a_offset) ? total_size_rw - a_offset : 0;
	cnt_after_offset_rw = size_after_offset_rw / unRegisterSize;
	a_count = a_count < cnt_after_offset_rw ? a_count : cnt_after_offset_rw;
	addressDev = (char*)a_dev->memmory_base[a_bar] + a_offset;

	for (i = 0; i < a_count; ++i, pcUserBuf += unRegisterSize, addressDev += unRegisterSize)
	{
		Read_Write_Private(a_register_size, a_rw_access_mode, addressDev, &tmp_data_for_rw, /*NULL*/&tmp_data_for_rw, /*NULL*/&tmp_data_for_rw);
		if (g_nPrintDebugInfo){ printk(KERN_ALERT "!!!!!!! scrachTest=%x, readed=%x\n", (unsigned)scrachTest,(unsigned)tmp_data_for_rw); }
		if ((tmp_data_for_rw&scrachTest) == scrachTest)
		{
			if (g_nPrintDebugInfo){ printk(KERN_ALERT "!!!!!!! Scrach test started\n"); }
			scrtch_check = pciedev_check_scratch(a_dev);
			if (scrtch_check)
			{
				if (!GET_FLAG_VALUE(a_dev->error_report_status,PCIE_ERROR_BIT))
				{
					ERRCT("ERROR: PCIE error!\n");
					SET_FLAG_VALUE(a_dev->error_report_status, PCIE_ERROR_BIT, 1);
				}
				retval = -ENOMEM; break;
			}
			else
			{
				// Reset PCIe error bit
				if (GET_FLAG_VALUE(a_dev->error_report_status, PCIE_ERROR_BIT))
				{
					SET_FLAG_VALUE(a_dev->error_report_status, PCIE_ERROR_BIT, 0);
				}
			}
		}
		else
		{
			// Reset PCIe error bit
			if (GET_FLAG_VALUE(a_dev->error_report_status, PCIE_ERROR_BIT))
			{
				SET_FLAG_VALUE(a_dev->error_report_status, PCIE_ERROR_BIT, 0);
			}
		}
		if (a_dev->swap) { tmp_data_for_rw = UPCIEDEV_SWAP_TOT(unRegisterSize, tmp_data_for_rw); }
		if (a_pcUserBuf0 && copy_to_user(pcUserBuf, &tmp_data_for_rw, unRegisterSize)) { break; }
		retval += unRegisterSize;
	}

	return retval;

}



/*
 * pciedev_write_inline:  write data from user space buffer to a_count number of device registers
 *
 * Parameters:
 *   a_dev:           device structure to handle
 *   a_mode:          register access mode (read or write and register len (8,16 or 32 bit))
 *   a_bar:           bar to access (0, 1, 2, 3, 4 or 5)
 *   a_offset:        first register offset
 *   a_pcUserBuf:     pointer to user buffer for data
 *   a_pcUserMask:    pointer to user buffer for mask (in the case of bitwise write)
 *   a_count:         number of device register to write
 *
 * Return (int):
 *   < 0:             error (details on all errors on documentation)
 *   other:           number of Bytes read from device registers
 */
static inline ssize_t pciedev_write_inline2(void* a_a_dev, 
	u_int16_t a_register_size, u_int16_t a_rw_access_mode, u_int32_t a_bar, u_int32_t a_offset,
	const char __user *a_pcUserData0, const char __user *a_pcUserMask0, size_t a_count)
{
	struct pciedev_dev* a_dev = (struct pciedev_dev*)a_a_dev;
	const char __user*	pcUserData = a_pcUserData0;
	const char __user*	pcUserMask = a_pcUserMask0;
	ssize_t				retval = 0;
	u32					total_size_rw;
	u32					size_after_offset_rw;
	u32					cnt_after_offset_rw;
	int					i;
	u32					tmp_data_for_rw;
	u32					tmp_mask_for_w = 0xffffffff;
	char*				addressDev;
	u_int32_t			unRegisterSize;

	unRegisterSize = GetRegisterSizeInBytes(a_register_size,a_dev->register_size);

	if (!a_dev->memmory_base[a_bar] || a_offset%unRegisterSize)
	{
		ERRCT("ERROR: memor_base[%u]=%p, offset=%u, registerSize=%u\n", a_bar, a_dev->memmory_base[a_bar], a_offset, unRegisterSize);
		return -EFAULT;
	}

	total_size_rw = a_dev->mem_base_end[a_bar] - a_dev->mem_base[a_bar];
	size_after_offset_rw = (total_size_rw>a_offset) ? total_size_rw - a_offset : 0;
	cnt_after_offset_rw = size_after_offset_rw / unRegisterSize;
	a_count = a_count < cnt_after_offset_rw ? a_count : cnt_after_offset_rw;
	addressDev = (char*)a_dev->memmory_base[a_bar] + a_offset;

	for (i = 0; i < a_count; ++i, pcUserData += unRegisterSize, pcUserMask += unRegisterSize, addressDev += unRegisterSize)
	{
		if (a_pcUserData0 && copy_from_user(&tmp_data_for_rw, pcUserData, unRegisterSize))break;
		if (a_pcUserMask0 && copy_from_user(&tmp_mask_for_w, pcUserMask, unRegisterSize))break;
		if (a_dev->swap) { tmp_data_for_rw = UPCIEDEV_SWAP_TOT(unRegisterSize, tmp_data_for_rw); tmp_mask_for_w = UPCIEDEV_SWAP_TOT(unRegisterSize, tmp_mask_for_w); }
		Read_Write_Private(a_register_size,a_rw_access_mode, addressDev, &tmp_data_for_rw, &tmp_mask_for_w,/*NULL*/&tmp_mask_for_w);
		retval += unRegisterSize;
	}

	return retval;
}



/*
 * pciedev_check_scratch:  write data from user space buffer to a_count number of device registers
 *
 * Parameters:
 *   a_dev:           device structure to handle
 *
 * Return (int):
 *   0:               no pci error
 *   1:               pci error accured
 */
static inline int pciedev_check_scratch(struct pciedev_dev * a_dev)
{
	char*	address = (char*)a_dev->memmory_base[a_dev->scratch_bar] + a_dev->scratch_offset;
	u32		scrachTest;
	u32		tmp_data_for_rw;

	if (!a_dev->memmory_base[a_dev->scratch_bar])
	{
		WARNCT("NO MEM UNDER BAR%d \n", a_dev->scratch_bar);
		return 1;
	}

	scrachTest = a_dev->register_size == RW_D8 ? 0xff : (a_dev->register_size == RW_D16 ? 0xffff : 0xffffffff);
	Read_Write_Private(a_dev->register_size,MTCA_SIMPLE_READ,address,&tmp_data_for_rw,NULL,NULL);

	if ((tmp_data_for_rw&scrachTest) == scrachTest){ return 1; }

	return 0;
}



/*
 * ModeString:    get string for corresponding mode (for debugging)
 *                safe to call from interrupt contex
 *
 * Parameters:
 *   a_unMode:        register access mode (read or write and register len (8,16 or 32 bit))
 *
 * Return (const char*):
 *   "UnknownRW":     error (not correct mode provided)
 *   other:           corresponding string for provided mode
 */
static inline const char* ModeString(u_int16_t a_unMode)
{
	switch (a_unMode)
	{
	case RW_D08:return "RW_D08";
	case RW_D16:return "RW_D16";
	case RW_D32:return "RW_D32";
	default:break;
	}

	return "UnknownRW";
}

#endif  /* #ifndef __read_write_inline_h__ */
