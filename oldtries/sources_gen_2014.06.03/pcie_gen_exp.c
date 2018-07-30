/*
 *	File: pcie_gen_exp.c
 *
 *	Created on: May 12, 2013
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *	This file contain definitions of all exported functions
 *
 */

#include "pcie_gen_exp.h"

#include <linux/syscalls.h>
//#include <uapi/asm-generic/unistd.h>


#ifndef LINUX_VERSION_CODE
#define LINUX_VERSION_CODE KERNEL_VERSION(3,5,0)
#endif

#if LINUX_VERSION_CODE < 132632
#else
#define		KILL_CRED_DEFINED
#define		__MY_CRED__		cred
#endif


struct workqueue_struct* g_pcie_gen_IRQworkqueue;
EXPORT_SYMBOL(g_pcie_gen_IRQworkqueue);


static int GetChunkSize1(u_int a_unMode)
{
	int		nChunkSize;

	switch(a_unMode)
	{
	case RW_D8: case RW_D8_MASK: case W_D8:
		nChunkSize = 1;
		break;
	//case RW_D16: case RW_D16_MASK:
	//	nChunkSize = 2;
	//	break;
	case RW_D32: case RW_D32_MASK: case W_D32:
		nChunkSize = 4;
		break;
	default:
		nChunkSize = 2;		
		break;
	}

	return nChunkSize;
}



static int GetAddressAndMemSize1(struct Pcie_gen_dev* a_dev,u_int a_tmp_barx,void** a_address,u32* a_mem_size)
{

	a_tmp_barx %= NUMBER_OF_BARS;
	if(!a_dev->m_Memories[a_tmp_barx].m_memory_base )
	{
		PRINT_KERN_ALERT("No memory in base %d\n",(int)a_tmp_barx);
		return -EFAULT;
	}
	*a_address = (void *)(a_dev->m_Memories)[a_tmp_barx].m_memory_base;
	*a_mem_size = a_dev->m_Memories[a_tmp_barx].m_mem_base_end - a_dev->m_Memories[a_tmp_barx].m_mem_base;

	return 0;
}



//ssize_t ReadDevAndCopy1(u_int a_unMode,void* address,void* a_pValueUsr)
static ssize_t ReadDevAndCopy1(u_int a_unMode,void* address,void* a_pValueUsr)
{
	u8		tmp_data_8;
	u16		tmp_data_16;
	u32		tmp_data_32;
	ssize_t	nRetVal;
	void*	pSource;
	//u_int	unData;
	//int		nChunkSize;

	switch(a_unMode)
	{
	case RW_D8:case RW_D8_MASK:
		tmp_data_8        = ioread8(address);
		rmb();
		//(*a_punValue) = tmp_data_8 & 0x0ff;
		//unData = tmp_data_8 & 0x0ff;
		nRetVal = sizeof(u8);
		pSource = &tmp_data_8;

		break;
	case RW_D32:
		tmp_data_32        = ioread32(address);
		rmb();

		nRetVal	= sizeof(u32);
		pSource = &tmp_data_32;
		break;
	default:
		tmp_data_16        = ioread16(address);
		rmb();
		
		//(*a_punValue)   = tmp_data_16 & 0xffff;
		//unData = tmp_data_16 & 0xffff;
		nRetVal = sizeof(u16);
		pSource = &tmp_data_16;
		
		break;
	}


	if( copy_to_user(a_pValueUsr,pSource,nRetVal) )
	{
		nRetVal = -EFAULT;
	}

	return nRetVal;
}



static ssize_t General_Read1_Prvt(Pcie_gen_dev* a_dev, u_int a_nCount, const device_rw2* reading_p)
{

	char*		pcDataBuffer	= 0;// Buffer where read value will be stored
	void*		pData;				// Temporal buffer from user space to store one value
	ssize_t		retval			= 0;
	u_int		tmp_offset		= 0;
	u_int		tmp_mode		= 0;
	u_int		tmp_barx		= 0;
	u_int		tmp_count_rw	= 0;
	u32			mem_size		= 0;
	u_int		i				= 0;
	u_int		tmp_count_final;
	void*		address			= 0;
	int			nTmpOffset;
	ssize_t		nReaded;
	int			nChunkSize;
    
	// To confirm really atomic read use ioctl(PCIE_GEN_IOC_RW)
	//if( mutex_lock_interruptible(&a_dev->m_dev_mut) ) return -ERESTARTSYS;	
    	
	
	tmp_offset		= reading_p->offset_rw;
	tmp_mode		= reading_p->mode_rw;
	tmp_barx		= reading_p->barx_rw;
	tmp_count_rw	= a_nCount;
	pcDataBuffer	= (char*)reading_p->dataPtr;

	nChunkSize = GetChunkSize1(tmp_mode);
	
	if( GetAddressAndMemSize1(a_dev,tmp_barx,&address,&mem_size) )
	{
		//mutex_unlock(&a_dev->m_dev_mut);
		return -EFAULT;
	}

	
	tmp_count_rw	= (tmp_count_rw<0) ? 0 : tmp_count_rw;
	tmp_count_final = (mem_size-tmp_offset)/nChunkSize;
	tmp_count_final = tmp_count_final>0 ? tmp_count_final : 0;
	tmp_count_final = (tmp_count_final<tmp_count_rw) ? tmp_count_final : tmp_count_rw;

	nTmpOffset = 0;

	//for( i = 0; (i<tmp_count_rw) && tmp_offset+nTmpOffset+nChunkSize<(u_int)mem_size; ++i )
	for( i = 0; i<tmp_count_final; ++i )
	{

		pData = pcDataBuffer + nTmpOffset;
		nReaded = ReadDevAndCopy1(tmp_mode,address+tmp_offset+nTmpOffset,pData);

		if( nReaded<0 )
		{
			// This means that readed amount is less than requested
			break;
		}

		retval += nReaded;
		nTmpOffset += nChunkSize;

	}
	
	//mutex_unlock(&a_dev->m_dev_mut);
	return retval;

}



ssize_t General_Read1(Pcie_gen_dev* a_dev, char __user *buf, size_t count)
{

	device_rw2	reading;
    
	if(  copy_from_user( &reading, buf, sizeof(device_rw2) )  ) return -EFAULT;

	return General_Read1_Prvt(a_dev,count,&reading);

}
EXPORT_SYMBOL(General_Read1);



static ssize_t WriteDev1_Prvt(u_int a_unMode,void* address,const void* a_cpValue,const void* a_cpMask);

static ssize_t WriteDev1(u_int a_unMode,int a_nChunkSize,void* address,
						 const void* __user a_cpValueUsr,const void* __user a_cpMaskUsr)
{

	u32		tmp_data_32;
	u32		tmp_data_Mask_32;

	if( copy_from_user(&tmp_data_32,a_cpValueUsr,a_nChunkSize) )return -EFAULT;
	if( copy_from_user(&tmp_data_Mask_32,a_cpMaskUsr,a_nChunkSize) )return -EFAULT;

	return WriteDev1_Prvt(a_unMode,address,&tmp_data_32,&tmp_data_Mask_32);
	
}



static ssize_t WriteDev1_Prvt(u_int a_unMode,void* address,const void* a_cpValue,const void* a_cpMask)
{

	int		nChunkSize			= 0;
	u8		tmp_data_8;
	u16		tmp_data_16;
	u32		tmp_data_32;

	u8		tmp_data_Prev_8;
	u16		tmp_data_Prev_16;
	u32		tmp_data_Prev_32;
	
	u8		tmp_data_Mask_8;
	u16		tmp_data_Mask_16;
	u32		tmp_data_Mask_32;
	

	switch(a_unMode)
	{
	case RW_NONE:break;

	case RW_D8:
		nChunkSize = 1;
		tmp_data_8 = *((const u8*)a_cpValue);

		iowrite8(tmp_data_8,address);
		smp_wmb();
		break;

	case RW_D8_MASK:
		nChunkSize = 1;
		tmp_data_8			=  *((const u8*)a_cpValue);

		tmp_data_Mask_8		=  *((const u8*)a_cpMask);

		tmp_data_8			&= tmp_data_Mask_8;				// Keeping fields corresponding to Mask

		tmp_data_Prev_8		=  ioread8(address);
		rmb();
		tmp_data_Mask_8		=  ~tmp_data_Mask_8;			// Inverting Mask
		tmp_data_Prev_8		&= tmp_data_Mask_8;				// Keeping fields corresponding to inverted Mask
		tmp_data_8			|= tmp_data_Prev_8;				// Concatenating old and new datas

		iowrite8(tmp_data_8,address);						// Writing data to device
		smp_wmb();

		break;

	/*case RW_D16:
		nChunkSize = 2;
		tmp_data_16		=  *((const u16*)a_cpValue);
		
		iowrite16(tmp_data_16,address);
		smp_wmb();
		break;*/   // Implemented in default

	case RW_D16_MASK:
		nChunkSize = 2;
		tmp_data_16			=  *((const u16*)a_cpValue);

		tmp_data_Mask_16	=  *((const u16*)a_cpMask);

		tmp_data_16			&= tmp_data_Mask_16;			// Keeping fields corresponding to Mask

		tmp_data_Prev_16	=  ioread16(address);
		rmb();
		tmp_data_Mask_16	=  ~tmp_data_Mask_16;			// Inverting Mask
		tmp_data_Prev_16	&= tmp_data_Mask_16;			// Keeping fields corresponding inverted Mask
		tmp_data_16			|= tmp_data_Prev_16;			// Concatenating old and new datas

		iowrite16(tmp_data_16,address);						// Writing data to device
		smp_wmb();

		break;

	case RW_D32:
		nChunkSize = 4;
		tmp_data_32			=  *((const u32*)a_cpValue);

		iowrite32(tmp_data_32,address);
		smp_wmb();
		break;

	case RW_D32_MASK:
		nChunkSize = 4;
		tmp_data_32			=  *((const u32*)a_cpValue);

		tmp_data_Mask_32	=  *((const u32*)a_cpMask);

		tmp_data_32			&= tmp_data_Mask_32;			// Keeping fields corresponding to Mask

		tmp_data_Prev_32	=  ioread32(address);
		rmb();
		tmp_data_Mask_32	=  ~tmp_data_Mask_32;			// Inverting Mask
		tmp_data_Prev_32	&= tmp_data_Mask_32;			// Keeping fields corresponding inverted Mask
		tmp_data_32			|= tmp_data_Prev_32;			// Concatenating old and new datas

		iowrite32(tmp_data_32,address);						// Writing data to device
		smp_wmb();

		break;

	default:
		nChunkSize = 2;
		tmp_data_16		=  *((const u16*)a_cpValue);
		
		iowrite16(tmp_data_16,address);
		smp_wmb();
		break;
	}

	return (ssize_t)nChunkSize;
}



static inline ssize_t General_Write1_Prvt(Pcie_gen_dev* a_dev, u_int a_nCount, const device_rw2* reading_ptr)
{

	const char*	cpcDataBuffer	= 0;// Buffer where read value will be stored
	const char*	cpcMaskBuffer	= 0;// Buffer where read value will be stored
	ssize_t		nSingleCall;		// Result for single iteration
	u_int		i;
	u_int		tmp_count_final;
	int			nTmpOffset;
	int			nChunkSize;
	const void*	cpData;
	const void*	cpMask;

	ssize_t		retval;
	u_int		tmp_offset		= 0;
	u_int		tmp_mode		= 0;
	u_int		tmp_barx		= 0;
	u_int		tmp_count_rw	= 0;
	u32			mem_size		= 0;
	void*		address			= 0;


	if( mutex_lock_interruptible(&a_dev->m_dev_mut) )return -ERESTARTSYS;


	tmp_mode     = reading_ptr->mode_rw;
	tmp_offset   = reading_ptr->offset_rw;
	tmp_barx     = reading_ptr->barx_rw;
	tmp_count_rw = a_nCount;


	if( GetAddressAndMemSize1(a_dev,tmp_barx,&address,&mem_size) )
	{
		mutex_unlock(&a_dev->m_dev_mut);
		return -EFAULT;
	}


	nChunkSize = GetChunkSize1(tmp_mode);

	cpcDataBuffer = (const char*)reading_ptr->dataPtr;
	cpcMaskBuffer = (const char*)reading_ptr->maskPtr;


	//PRINT_KERN_ALERT("PCIE_GEN: address = %ld, mem_size = %d, tmp_barx = %d\n", 
	//				(long)address, (int)mem_size, (int)tmp_barx );

	tmp_count_rw	= (tmp_count_rw<0) ? 0 : tmp_count_rw;
	tmp_count_final = (mem_size-tmp_offset)/nChunkSize;
	tmp_count_final = tmp_count_final>0 ? tmp_count_final : 0;
	tmp_count_final = (tmp_count_final<tmp_count_rw) ? tmp_count_final : tmp_count_rw;


	retval		= 0;
	nTmpOffset	= 0;
	/////////////////////////////////////////////////////////////////////////
	//for( i = 0; (i<tmp_size_rw) && ((tmp_offset+nTmpOffset+nChunkSize)<(u_int)mem_tmp); ++i )
	for( i = 0; i<tmp_count_final; ++i )
	{

		cpData = (const void*)( cpcDataBuffer + nTmpOffset );
		cpMask = (const void*)( cpcMaskBuffer + nTmpOffset );

		nSingleCall = WriteDev1(tmp_mode,nChunkSize,address+tmp_offset+nTmpOffset,cpData,cpMask);

		if( nSingleCall < 0 )
		{
			// This means that actually readed data is less than requested
			break;
		}

		retval += nSingleCall;
		nTmpOffset += nChunkSize;

	}


	mutex_unlock(&a_dev->m_dev_mut);
	return retval;
}




ssize_t General_Write1(Pcie_gen_dev* a_dev, const char __user *buf, size_t count)
{

	device_rw2	reading;

	if( copy_from_user(&reading,buf,sizeof(device_rw2))  )return -EFAULT;

	return General_Write1_Prvt(a_dev,count,&reading);


}
EXPORT_SYMBOL(General_Write1);



/*
 * This function should be modified to safe one (but without syncronization objects)
 */
void RemovePID1( struct Pcie_gen_dev* a_dev, pid_t a_nPID )
{
	//RemovePID( pid_t a_nPID );

	int i;
	int nIndex = -1;

	for( i = 0; i < a_dev->m_nInteruptWaiters; ++i )
	{
		if( a_nPID == a_dev->m_IrqWaiters[i].m_nPID )
		{
			nIndex = i;
			break;
		}
	}

	if( nIndex >= 0 )
	{
		if( nIndex<a_dev->m_nInteruptWaiters-1 )
			memmove( a_dev->m_IrqWaiters+nIndex, a_dev->m_IrqWaiters+nIndex+1,(a_dev->m_nInteruptWaiters-nIndex-1)*sizeof(SInteruptStr) );

		a_dev->m_nInteruptWaiters--;
	}
}
EXPORT_SYMBOL(RemovePID1);




long  General_ioctl( struct Pcie_gen_dev* a_pDev, unsigned int cmd, unsigned long arg)
{

	int							nIndex;
	int							nTmp;
	int							i;
	int							err			= 0;
	pid_t						cur_proc;
	str_for_ioc_rw				aStrRWAll;
	device_rw2					aStrRW;
	const char*					cpcData;

	PRINT_KERN_ALERT("IOCTRL CMD %u\n", cmd);

	if(!a_pDev->m_dev_sts)
	{
		PRINT_KERN_ALERT("NO DEVICE %d\n", a_pDev->m_dev_num);
		return -EFAULT;
	}

	cur_proc = current->group_leader->pid;
	
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != PCIE_GEN_IOC)    return -ENOTTY;
	if (_IOC_NR(cmd) > PCIE_GEN_IOC_MAXNR) return -ENOTTY;

	/*
	 * the direction is a bitmask, and VERIFY_WRITE catches R/W
	 * transfers. `Type' is user-oriented, while
	 * access_ok is kernel-oriented, so the concept of "read" and
	 * "write" is reversed
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	
	if( err )
	{
		PRINT_KERN_ALERT( "access_ok Error\n");
		return -EFAULT;
	}
	

	if (mutex_lock_interruptible(&a_pDev->m_dev_mut))return -ERESTARTSYS;


	switch(cmd)
	{
	case PCIE_GEN_REGISTER_FOR_IRQ:
		PRINT_KERN_ALERT( "case PCIEGEN_REGISTER_FOR_IRQ\n");

		if( a_pDev->m_nInteruptWaiters < MAX_INTR_NUMB-1 )
		{
			for( i = 0; i <  a_pDev->m_nInteruptWaiters; ++i )
			{
				if( cur_proc ==  a_pDev->m_IrqWaiters[i].m_nPID )
				{
					mutex_unlock(&a_pDev->m_dev_mut);
					return 0;
				}
			}

			nIndex = a_pDev->m_nInteruptWaiters++;

			a_pDev->m_IrqWaiters[ nIndex ].m_nPID = cur_proc;
#ifdef KILL_CRED_DEFINED
			a_pDev->m_IrqWaiters[ nIndex ].m_pCred = current->__MY_CRED__;
#endif

			if( copy_from_user(&nTmp,(const char*)arg,sizeof(int))  )
			{
				//printk(KERN_ALERT);
				// additional info not provided
				a_pDev->m_IrqWaiters[ nIndex ].m_nSigNum = SIGUSR1;
				a_pDev->m_IrqWaiters[ nIndex ].m_nAdditionCode = 0;
			}
			else
			{
				//a_pDev->m_IrqWaiters[ nIndex ].m_nSigNum = nTmp == -1 ? a_pDev->pci_dev_irq : nTmp;
				a_pDev->m_IrqWaiters[ nIndex ].m_nSigNum = nTmp == -1 ? SIGUSR2 : nTmp;
				if( copy_from_user(&nTmp,(const char*)(arg+sizeof(int)),sizeof(int))  )
				{
					// CallBack int not provided
					a_pDev->m_IrqWaiters[ nIndex ].m_nAdditionCode = 0;
				}
				else
				{
					a_pDev->m_IrqWaiters[ nIndex ].m_nAdditionCode = nTmp;
				}
			}

		}
		else
		{
			mutex_unlock(&a_pDev->m_dev_mut);
			return -1;
		}
		break;

	case PCIE_GEN_UNREGISTER_FROM_IRQ:
		RemovePID1(a_pDev,cur_proc);
		break;

	case PCIE_GEN_PHYSICAL_SLOT:
		copy_to_user((int __user *)arg,&a_pDev->m_slot_num,sizeof(int));
		break;

	//case PCIE_GEN_VENDOR_ID:
	//	copy_to_user((int __user *)arg,&a_pDev->m_ve,sizeof(int));
	//	break;
	case PCIE_GEN_IOC_RW:
		if( copy_from_user(&aStrRWAll.dataPtr,(const char*)arg,sizeof(u_int64_t))  )
		{
			mutex_unlock(&a_pDev->m_dev_mut);
			return -EFAULT;
		}
		if( copy_from_user(&aStrRWAll.m_nIterations,(const char*)(arg+sizeof(u_int64_t)),sizeof(int))  )
		{
			aStrRWAll.m_nIterations = -1;
		}
		i=0;
		cpcData = (const char*)aStrRWAll.dataPtr;
		while(!copy_from_user(&aStrRW,cpcData,sizeof(device_rw2)))
		{
			if(aStrRW.mode_rw>WRITE_INIT)
			{
				General_Write1_Prvt(a_pDev,aStrRW.count_rw,&aStrRW);
			}
			else
			{
				General_Read1_Prvt(a_pDev,aStrRW.count_rw,&aStrRW);
			}

			if(aStrRWAll.m_nIterations>=0 && i>=aStrRWAll.m_nIterations){break;}
			++i;
		}
		break;

	default:
		mutex_unlock(&a_pDev->m_dev_mut);
		return -ENOTTY;
	}

	
	mutex_unlock(&a_pDev->m_dev_mut);
	return 0;
}
EXPORT_SYMBOL(General_ioctl);



long  General_ioctl2( struct str_pcie_gen* a_pDev, unsigned int cmd, unsigned long arg)
{
	int							nIndex;
	int							nTmp;

	const char*					cpcFileName;
	int							i;
	int							err			= 0;
	pid_t						cur_proc;
	Pcie_gen_dev*				dev = &a_pDev->m_PcieGen;

	cpcFileName = strrchr(__FILE__,'/') + 1;
	cpcFileName = cpcFileName ? cpcFileName : "Unknown File";
	PRINT_KERN_ALERT("cmd = %d,  arg = 0x%.16lX\n",(int)cmd,(long)arg);

	if(!dev->m_dev_sts)
	{
		PRINT_KERN_ALERT("NO DEVICE %d\n",dev->m_dev_num);
		return -EFAULT;
	}

	cur_proc = current->group_leader->pid;
	
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != PCIE_GEN_IOC)    return -ENOTTY;
	if (_IOC_NR(cmd) > PCIE_GEN_IOC_MAXNR) return -ENOTTY;

	/*
	 * the direction is a bitmask, and VERIFY_WRITE catches R/W
	 * transfers. `Type' is user-oriented, while
	 * access_ok is kernel-oriented, so the concept of "read" and
	 * "write" is reversed
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	
	if( err )
	{
		PRINT_KERN_ALERT( "access_ok Error\n");
		return -EFAULT;
	}
	
	if (mutex_lock_interruptible(&dev->m_dev_mut))return -ERESTARTSYS;


	switch(cmd)
	{
	case PCIE_GEN_REGISTER_FOR_IRQ:
		PRINT_KERN_ALERT( "case PCIEGEN_REGISTER_FOR_IRQ\n");

		if( dev->m_nInteruptWaiters < MAX_INTR_NUMB-1 )
		{
			for( i = 0; i <  dev->m_nInteruptWaiters; ++i )
			{
				if( cur_proc ==  dev->m_IrqWaiters[i].m_nPID )
				{
					mutex_unlock(&dev->m_dev_mut);
					return 0;
				}
			}

			nIndex = dev->m_nInteruptWaiters++;

			dev->m_IrqWaiters[ nIndex ].m_nPID = cur_proc;
#ifdef KILL_CRED_DEFINED
			dev->m_IrqWaiters[ nIndex ].m_pCred = current->__MY_CRED__;
#endif

			if( copy_from_user(&nTmp,(const char*)arg,sizeof(int))  )
			{
				// additional info not provided
				dev->m_IrqWaiters[ nIndex ].m_nSigNum = SIGUSR1;
				dev->m_IrqWaiters[ nIndex ].m_nAdditionCode = 0;
			}
			else
			{
				dev->m_IrqWaiters[ nIndex ].m_nSigNum = nTmp == -1 ? a_pDev->pci_dev_irq : nTmp;
				//dev->m_IrqWaiters[ nIndex ].m_nSigNum = nTmp == -1 ? SIGUSR2 : nTmp;
				if( copy_from_user(&nTmp,(const char*)(arg+sizeof(int)),sizeof(int))  )
				{
					// CallBack int not provided
					dev->m_IrqWaiters[ nIndex ].m_nAdditionCode = 0;
				}
				else
				{
					dev->m_IrqWaiters[ nIndex ].m_nAdditionCode = nTmp;
				}
			}

		}
		else
		{
			mutex_unlock(&dev->m_dev_mut);
			return -1;
		}
		break;

	case PCIE_GEN_UNREGISTER_FROM_IRQ:
		RemovePID1(dev,cur_proc);
		break;

	case PCIE_GEN_PHYSICAL_SLOT:
		copy_to_user((int __user *)arg,&dev->m_slot_num,sizeof(int));
		break;

	case PCIE_GEN_VENDOR_ID:
		copy_to_user((int __user *)arg,&a_pDev->vendor_id,sizeof(int));
		break;

	case PCIE_GEN_DEVICE_ID:
		copy_to_user((int __user *)arg,&a_pDev->device_id,sizeof(int));
		break;

	case PCIE_GEN_DRIVER_VERSION:
		break;

	case PCIE_GEN_FIRMWARE_VERSION:
		break;

	case PCIE_GEN_TEST_IOC:

		/*if( copy_from_user(&aActiveDev.dev_pars,(SDeviceParams*)arg,sizeof(SDeviceParams))  )
		{
			mutex_unlock(&dev->m_dev_mut);
			return -EFAULT;
		}

		fpFunc = (TYPE_FUNC)arg;
		printk(KERN_ALERT "fpFunc = %ld\n",(long)fpFunc);*/

		//if( fpFunc )(*fpFunc)();
		break;

	default:
		mutex_unlock(&dev->m_dev_mut);
		return -ENOTTY;
	}

	
	mutex_unlock(&dev->m_dev_mut);

	return 0;

}
EXPORT_SYMBOL(General_ioctl2);



void PcieGen_do_work1(struct Pcie_gen_dev *dev)
{

	int* pnAditCod;
	SInteruptStr aProcTask;

	static int nInterupt = 0;
	int i;
#ifdef KILL_CRED_DEFINED
	struct task_struct *p;
#endif
	
	if(!(dev->m_dev_sts)) return;

	++nInterupt;

	memset(&dev->m_info, 0, sizeof(struct siginfo));
	dev->m_info.si_code = SI_QUEUE;

	//dev->m_info.si_value.sival_ptr = 0;

	if(dev->m_nSigVal == -1 )
	{
		dev->m_info.si_value.sival_int = nInterupt;
	}
	else
	{
		dev->m_info.si_value.sival_int = dev->m_nSigVal;
	}

	pnAditCod = (int*)((void*)&dev->m_info.si_value.sival_ptr);
	++pnAditCod;

	if( nInterupt % 36000 == 0 )
	{
		PRINT_KERN_ALERT("g_info.si_value.sival_int = %d\n",dev->m_info.si_value.sival_int );
		PRINT_KERN_ALERT("InteruptWaiters = %d\n\n",dev->m_nInteruptWaiters);
	}

	for( i = 0; i < dev->m_nInteruptWaiters; ++i )
	{
		aProcTask = dev->m_IrqWaiters[i];

#ifdef KILL_CRED_DEFINED
		p = pid_task(find_vpid(aProcTask.m_nPID), PIDTYPE_PID);

		if( !p )
		{
			RemovePID1( dev, aProcTask.m_nPID );
			continue;
		}

		dev->m_info.si_signo = aProcTask.m_nSigNum;
		*pnAditCod =aProcTask.m_nAdditionCode;

		if( kill_pid_info_as_cred(	aProcTask.m_nSigNum,&dev->m_info,
									find_vpid(aProcTask.m_nPID),
									aProcTask.m_pCred,0) )
		{
			RemovePID1(  dev,  aProcTask.m_nPID );
		}
#endif
	}

	dev->m_nSigVal = -1;
}
EXPORT_SYMBOL(PcieGen_do_work1);



#if LINUX_VERSION_CODE < 132632 
static void pcie_gen_do_work(void *pciegendev)
#else
static void pcie_gen_do_work(struct work_struct *work_str)
#endif 
{
#if LINUX_VERSION_CODE < 132632
	struct str_pcie_gen *dev = (struct str_pcie_gen*)pciegendev;
#else
	struct str_pcie_gen *dev   =  container_of(work_str, struct str_pcie_gen, pcie_gen_work);
#endif
	PcieGen_do_work1(&dev->m_PcieGen);
}



/*
 * The top-half interrupt handler.
 */
#if LINUX_VERSION_CODE < 0x20613 // irq_handler_t has changed in 2.6.19
static irqreturn_t pcie_gen_interrupt(int irq, void *dev_id, struct pt_regs *regs)
#else
static irqreturn_t pcie_gen_interrupt(int irq, void *dev_id)
#endif
{

	int i;
	u_int unMask = 0xffffffff;
	struct str_pcie_gen * dev = dev_id;

	if( !dev->m_PcieGen.m_dev_sts )return IRQ_NONE;

	queue_work(g_pcie_gen_IRQworkqueue , &(dev->pcie_gen_work));

	for( i = 0; i < NUMBER_OF_BARS; ++i )
	{
		WriteDev1_Prvt(	dev->m_rst_irq[i].mode_rw,
						(char*)dev->m_PcieGen.m_Memories[i].m_memory_base + dev->m_rst_irq[i].offset_rw,
						&dev->m_rst_irq[i].data_rw, &unMask );
	}

	return IRQ_HANDLED;
}




int  General_GainAccess1( struct str_pcie_gen* a_pDev, int a_MAJOR, struct class* a_class, const char* a_DRV_NAME)
{

	
	struct pci_dev*  dev = a_pDev->pcie_gen_pci_dev;

	int err      = 0;
	//int i        = 0;
	//int k        = 0;
	int result   = 0;
	int j;
	u16 vendor_id;
	u16 device_id;
	u8  revision;
	u8  irq_line;
	u8  irq_pin;
	u32 res_start;
	u32 res_end;
	u32 res_flag;
	u32 pci_dev_irq;
	int pcie_cap;
	u16 subvendor_id;
	u16 subdevice_id;
	u16 class_code;
	u32 tmp_devfn;
	u32 busNumber;
	u32 devNumber;
	u32 funcNumber;

	u32 tmp_slot_cap     = 0;
	//u32 tmp_module_id;
	//u16 tmp_data_16;

	int tmp_slot_num     = 0;
	int tmp_dev_num      = 0;
	int tmp_bus_func     = 0;
	//u16 tmp_slot_cntrl   = 0;

	//void *base_addres;

	if ((err = pci_enable_device(dev)))
	{
		PRINT_KERN_ALERT("Unable to enable pci device\n");
		return err;
	}




	if(a_pDev->irq_on)
	{
		PRINT_KERN_ALERT("Initing work\n");
#if LINUX_VERSION_CODE < 132632 
		INIT_WORK(&(a_pDev->pcie_gen_work),pcie_gen_do_work, a_pDev);
#else
		INIT_WORK(&(a_pDev->pcie_gen_work),pcie_gen_do_work);    
#endif
	}


	pci_set_master(dev);

	tmp_devfn    = (u32)dev->devfn;
	busNumber    = (u32)dev->bus->number;
	devNumber    = (u32)PCI_SLOT(tmp_devfn);
	funcNumber   = (u32)PCI_FUNC(tmp_devfn);
	tmp_bus_func = ((busNumber & 0xFF)<<8) + (devNumber & 0xFF);
	PRINT_KERN_ALERT("DEVFN %X, BUS_NUM %X, DEV_NUM %X, FUNC_NUM %X, BUS_FUNC %x\n",
		tmp_devfn, busNumber, devNumber, funcNumber, tmp_bus_func);

	tmp_devfn  = (u32)dev->bus->parent->self->devfn;
	busNumber  = (u32)dev->bus->parent->self->bus->number;
	devNumber  = (u32)PCI_SLOT(tmp_devfn);
	funcNumber = (u32)PCI_FUNC(tmp_devfn);
	PRINT_KERN_ALERT("GAIN_ACCESS(dev->bus->parent->self):DEVFN %X, BUS_NUM %X, DEV_NUM %X, FUNC_NUM %X\n",
		tmp_devfn, busNumber, devNumber, funcNumber);

	pcie_cap = pci_find_capability (dev->bus->parent->self, PCI_CAP_ID_EXP);
	PRINT_KERNEL_MSG(KERN_INFO,"PCIE SWITCH CAP address %X\n",pcie_cap);

	pci_read_config_dword(dev->bus->parent->self, (pcie_cap + PCI_EXP_SLTCAP), &tmp_slot_cap);
	tmp_slot_num = (tmp_slot_cap >> 19);
	tmp_dev_num  = tmp_slot_num;
	PRINT_KERN_ALERT("SLOT NUM %d DEV NUM%d SLOT_CAP %X\n",tmp_slot_num,tmp_dev_num,tmp_slot_cap);    

	a_pDev->m_PcieGen.m_slot_num = tmp_slot_num;
	a_pDev->m_PcieGen.m_dev_num  = tmp_slot_num;
	a_pDev->bus_func = tmp_bus_func;
	a_pDev->pcie_gen_pci_dev = dev;
	dev_set_drvdata(&(dev->dev), a_pDev);
	a_pDev->pcie_gen_all_mems = 0;


	//spin_lock_init(&a_pDev->irq_lock);
	a_pDev->softint        = 0;
	a_pDev->count          = 0;
	a_pDev->irq_mode       = 0;
	//a_pDev->irq_on         = 0;

	pci_read_config_word(dev, PCI_VENDOR_ID,   &vendor_id);
	pci_read_config_word(dev, PCI_DEVICE_ID,   &device_id);
	pci_read_config_word(dev, PCI_SUBSYSTEM_VENDOR_ID,   &subvendor_id);
	pci_read_config_word(dev, PCI_SUBSYSTEM_ID,   &subdevice_id);
	pci_read_config_word(dev, PCI_CLASS_DEVICE,   &class_code);
	pci_read_config_byte(dev, PCI_REVISION_ID, &revision);
	pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &irq_line);
	pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &irq_pin);

	pci_dev_irq = dev->irq;
	PRINT_KERN_ALERT("GET_PCI_CONF VENDOR %X DEVICE %X REV %X\n", vendor_id, device_id, revision);
	PRINT_KERN_ALERT("GET_PCI_CONF SUBVENDOR %X SUBDEVICE %X CLASS %X\n",subvendor_id,subdevice_id,class_code);
	a_pDev->vendor_id      = vendor_id;
	a_pDev->device_id      = device_id;
	a_pDev->subvendor_id   = subvendor_id;
	a_pDev->subdevice_id   = subdevice_id;
	a_pDev->class_code     = class_code;
	a_pDev->revision       = revision;
	a_pDev->irq_line       = irq_line;
	a_pDev->irq_pin        = irq_pin;
	a_pDev->pci_dev_irq    = pci_dev_irq;

	PRINT_KERN_ALERT("GET_PCI_CONF1 VENDOR %X DEVICE %X REV %X\n",a_pDev->vendor_id,a_pDev->device_id,a_pDev->revision);
	PRINT_KERN_ALERT("GET_PCI_CONF1 SUBVENDOR %X SUBDEVICE %X CLASS %X\n",a_pDev->subvendor_id,a_pDev->subdevice_id,a_pDev->class_code);

	if( EBUSY == pci_request_regions(dev, a_DRV_NAME ) )
	{
		PRINT_KERN_ALERT("EBUSY\n");
		return EBUSY;
	}


	//////////////////////////////////////////////////////////////////
	
	////////////////////////////////////////////////////////////////////////////
	for( j = 0; j < NUMBER_OF_BARS; ++j )
	{
		res_start = pci_resource_start(dev, j);
		res_end   = pci_resource_end(dev, j);
		res_flag  = pci_resource_flags(dev, j);
		a_pDev->m_PcieGen.m_Memories[j].m_mem_base       = res_start;
		a_pDev->m_PcieGen.m_Memories[j].m_mem_base_end   = res_end;
		a_pDev->m_PcieGen.m_Memories[j].m_mem_base_flag  = res_flag;

		if(res_start)
		{
			a_pDev->m_PcieGen.m_Memories[j].m_memory_base = pci_iomap(dev, j, (res_end - res_start));
			PRINT_KERNEL_MSG(KERN_INFO, "mem_region %d address %lX remaped address %lX\n", j, 
					(unsigned long int)res_start, 
					(unsigned long int)a_pDev->m_PcieGen.m_Memories[j].m_memory_base);
			a_pDev->m_PcieGen.m_Memories[j].m_rw_off = (res_end - res_start);
			PRINT_KERNEL_MSG(KERN_INFO," memorySize[%d] = %d\n",j,(int)(res_end - res_start));
			a_pDev->pcie_gen_all_mems++;
		}
		else
		{
			a_pDev->m_PcieGen.m_Memories[j].m_memory_base = 0;
			a_pDev->m_PcieGen.m_Memories[j].m_rw_off = 0;
			PRINT_KERNEL_MSG(KERN_INFO,"NO BASE_%d address\n",j);
		}
	}


	PRINT_KERN_ALERT("MAJOR = %d, MINOR = %d\n",a_MAJOR, a_pDev->m_PcieGen.m_dev_minor);
	
	a_pDev->m_PcieGen.m_dev_sts   = 1;
	device_create(a_class, NULL, MKDEV(a_MAJOR, a_pDev->m_PcieGen.m_dev_minor),
		(void*)&a_pDev->pcie_gen_pci_dev->dev, "pcie_gens%d", tmp_slot_num);

	//sys_chmod("/dev/aaa", 0x00000666 );
	//sys_gettid();



	if( a_pDev->irq_on )
	{

		PRINT_KERN_ALERT("Requestin interupt\n");

		a_pDev->irq_flag    = IRQF_SHARED | IRQF_DISABLED;
		a_pDev->pci_dev_irq = dev->irq;
		a_pDev->irq_mode    = 1;
		a_pDev->irq_on      = 1;
		result = request_irq(a_pDev->pci_dev_irq, pcie_gen_interrupt, 
			a_pDev->irq_flag, a_DRV_NAME, a_pDev);

		if (result) 
		{
			PRINT_KERN_ALERT("can't get assigned irq %i\n", a_pDev->irq_line);
			a_pDev->irq_mode = 0;
		}
		else
		{
			PRINT_KERN_ALERT("assigned IRQ %i RESULT %i\n",a_pDev->pci_dev_irq, result);
		}

	}



	return 0;
}
EXPORT_SYMBOL(General_GainAccess1);



void Gen_ReleaseAccsess(struct str_pcie_gen* a_pDev, int a_MAJOR, struct class* a_class)
{
	struct pci_dev*  dev = a_pDev->pcie_gen_pci_dev;
	//int                    k = 0;
	int j;
	//struct tamc200_dev     *tamc200_dev;
	int                    tmp_dev_num  = 0;
	int                    tmp_slot_num = 0;
	//unsigned long          flags;
	//void                   *base_addres;
	PRINT_KERN_ALERT("REMOVE CALLED\n");

	//tamc200_dev = dev_get_drvdata(&(dev->dev));
	tmp_dev_num  = a_pDev->m_PcieGen.m_dev_num;
	tmp_slot_num = a_pDev->m_PcieGen.m_slot_num;
	PRINT_KERN_ALERT("SLOT %d DEV %d\n", tmp_slot_num, tmp_dev_num);

	if( a_pDev->irq_on )
	{
		PRINT_KERN_ALERT("==============FreeIrq\n");
		free_irq(a_pDev->pci_dev_irq, a_pDev);
	}

	/*DISABLING INTERRUPTS ON THE MODULE*/
	//spin_lock_irqsave(&a_pDev->irq_lock, flags);
	//spin_unlock_irqrestore(&a_pDev->irq_lock, flags);

	//printk(KERN_ALERT "TAMC200_REMOVE: flush_workqueue\n");
	//printk(KERN_ALERT "TAMC200_REMOVE:REMOVING IRQ_MODE %d\n", a_pDev->irq_mode);
	/*if(a_pDev->irq_mode){
	printk(KERN_ALERT "TAMC200_REMOVE:FREE IRQ\n");
	free_irq(a_pDev->pci_dev_irq, a_pDev);
	}*/

	mutex_lock_interruptible(&(a_pDev->m_PcieGen.m_dev_mut));

	////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////
	for( j = 0; j < NUMBER_OF_BARS; ++j )
	{
		PRINT_KERN_ALERT("UNMAPPING MEMORY %d\n",j);
		if(a_pDev->m_PcieGen.m_Memories[j].m_memory_base)
		{
			pci_iounmap(dev, a_pDev->m_PcieGen.m_Memories[j].m_memory_base);
		}
		a_pDev->m_PcieGen.m_Memories[j].m_memory_base = 0;
	}

	PRINT_KERN_ALERT("MEMORYs UNMAPPED\n");
	pci_release_regions((a_pDev->pcie_gen_pci_dev));


	mutex_unlock(&(a_pDev->m_PcieGen.m_dev_mut));
	PRINT_KERN_ALERT("MAJOR = %d, MINOR = %d\n",a_MAJOR, a_pDev->m_PcieGen.m_dev_minor);
	device_destroy(a_class,  MKDEV(a_MAJOR, a_pDev->m_PcieGen.m_dev_minor));
	//unregister_tamc200_proc(tmp_slot_num);
	a_pDev->m_PcieGen.m_dev_sts   = 0;
	//tamc200ModuleNum --;
	pci_disable_device(a_pDev->pcie_gen_pci_dev);
}
EXPORT_SYMBOL(Gen_ReleaseAccsess);
