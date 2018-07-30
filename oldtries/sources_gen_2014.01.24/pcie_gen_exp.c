#include "pcie_gen_exp.h"

#include <linux/syscalls.h>
//#include <uapi/asm-generic/unistd.h>


#ifndef LINUX_VERSION_CODE
#define LINUX_VERSION_CODE KERNEL_VERSION(3,5,0)
#endif

MODULE_AUTHOR("Davit Kalantaryan");
MODULE_DESCRIPTION("pcie_gen_in General purpose driver");
MODULE_VERSION("1.1.0");
MODULE_LICENSE("Dual BSD/GPL");



static int GetChunkSize1(u_int a_unMode)
{
	int		nChunkSize;

	switch(a_unMode)
	{
	case RW_D8: case RW_D8_MASK:
		nChunkSize = 1;
		break;
	//case RW_D16: case RW_D16_MASK:
	//	nChunkSize = 2;
	//	break;
	case RW_D32: case RW_D32_MASK:
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
		printk(KERN_INFO "No memory\n");
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



ssize_t General_Read1(Pcie_gen_dev* a_dev, char __user *buf, size_t count)
{

	char*		pcDataBuffer	= 0;// Buffer where read value will be stored
	void*		pData;				// Temporal buffer from user space to store one value
	ssize_t		retval			= 0;
	int			tmp_offset		= 0;
	int			tmp_mode		= 0;
	int			tmp_barx		= 0;
	int			tmp_count_rw	= 0;
	u32			mem_size		= 0;
	int			i				= 0;
	device_rw2	reading;
	void*		address			= 0;
	int			nTmpOffset;
	ssize_t		nReaded;
	int			nChunkSize;
    

	if( mutex_lock_interruptible(&a_dev->m_dev_mut) ) return -ERESTARTSYS;
	
    
	if(  copy_from_user( &reading, buf, sizeof(device_rw2) )  )
	{
		mutex_unlock(&a_dev->m_dev_mut);
		return -EFAULT;
	}
	
    
	tmp_offset		= reading.offset_rw;
	tmp_mode		= reading.mode_rw;
	tmp_barx		= reading.barx_rw;
	tmp_count_rw	= (int)count;
	pcDataBuffer	= buf + reading.dataPtr;

	nChunkSize = GetChunkSize1(tmp_mode);
	if( GetAddressAndMemSize1(a_dev,tmp_barx,&address,&mem_size) )
	{
		mutex_unlock(&a_dev->m_dev_mut);
		return -EFAULT;
	}

	nTmpOffset = 0;

	for(i = 0; (i<tmp_count_rw) && tmp_offset+nTmpOffset+nChunkSize<(int)mem_size; i++)
	{
		pData = pcDataBuffer + i*nChunkSize;
		nReaded = ReadDevAndCopy1(tmp_mode,address+tmp_offset+nTmpOffset,pData);

		if( nReaded<0 )
		{
			mutex_unlock(&a_dev->m_dev_mut);
			return nReaded;
		}

		retval += nReaded;

		nTmpOffset += nChunkSize;

	}
	
	mutex_unlock(&a_dev->m_dev_mut);
	return retval;

}
EXPORT_SYMBOL(General_Read1);


static ssize_t WriteDev1(u_int a_unMode,int a_nChunkSize,void* address,const void* a_cpValueUsr,const void* a_cpMaskUsr)
//ssize_t WriteDev(u_int a_unMode,int a_nChunkSize,void* address,const void* a_cpValueUsr,const void* a_cpMaskUsr)
{

	u8		tmp_data_8;
	u16		tmp_data_16;
	u32		tmp_data_32;

	u8		tmp_data_Prev_8;
	u16		tmp_data_Prev_16;
	u32		tmp_data_Prev_32;
	
	u8		tmp_data_Mask_8;
	u16		tmp_data_Mask_16;
	u16		tmp_data_Mask_32;
	

	switch(a_unMode)
	{
	case RW_D8:
		//tmp_data_8			=  *((const u8*)a_cpValueUsr);
		if( copy_from_user(&tmp_data_8,a_cpValueUsr,a_nChunkSize) )return -EFAULT;

		iowrite8(tmp_data_8,address);
		smp_wmb();
		break;

	case RW_D8_MASK:
		//tmp_data_8			=  *((const u8*)a_cpValueUsr);
		if( copy_from_user(&tmp_data_8,a_cpValueUsr,a_nChunkSize) )return -EFAULT;

		//tmp_data_Mask_8		=  *((const u8*)a_cpMaskUsr);
		if( copy_from_user(&tmp_data_Mask_8,a_cpMaskUsr,a_nChunkSize) )return -EFAULT;

		tmp_data_8			&= tmp_data_Mask_8;				// Keeping fields corresponding to Mask

		tmp_data_Prev_8		=  ioread8(address);
		rmb();
		tmp_data_Mask_8		=  ~tmp_data_Mask_8;			// Inverting Mask
		tmp_data_Prev_8		&= tmp_data_Mask_8;				// Keeping fields corresponding to inverted Mask
		tmp_data_8			|= tmp_data_Prev_8;				// Concatenating old and new datas

		iowrite8(tmp_data_8,address);						// Writing data to device
		smp_wmb();

		break;

	//case RW_D16://Implemented in default

	case RW_D16_MASK:
		//tmp_data_16			=  *((const u16*)a_cpValueUsr);
		if( copy_from_user(&tmp_data_16,a_cpValueUsr,a_nChunkSize) )return -EFAULT;

		//tmp_data_Mask_16	=  *((const u16*)a_cpMaskUsr);
		if( copy_from_user(&tmp_data_Mask_16,a_cpMaskUsr,a_nChunkSize) )return -EFAULT;

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
		//tmp_data_32			=  *((const u32*)a_cpValueUsr);
		if( copy_from_user(&tmp_data_32,a_cpValueUsr,a_nChunkSize) )return -EFAULT;

		iowrite32(tmp_data_32,address);
		smp_wmb();
		break;

	case RW_D32_MASK:
		//tmp_data_32			=  *((const u32*)a_cpValueUsr);
		if( copy_from_user(&tmp_data_32,a_cpValueUsr,a_nChunkSize) )return -EFAULT;

		//tmp_data_Mask_32	=  *((const u32*)a_cpMaskUsr);
		if( copy_from_user(&tmp_data_Mask_32,a_cpMaskUsr,a_nChunkSize) )return -EFAULT;

		tmp_data_32			&= tmp_data_Mask_32;			// Keeping fields corresponding to Mask

		tmp_data_Prev_32	=  ioread32(address);
		rmb();
		tmp_data_Mask_32	=  ~tmp_data_Mask_32;			// Inverting Mask
		tmp_data_Prev_32	&= tmp_data_Mask_32;			// Keeping fields corresponding inverted Mask
		tmp_data_32			|= tmp_data_Prev_32;			// Concatenating old and new datas

		iowrite32(tmp_data_32,address);						// Writing data to device
		smp_wmb();

		break;

	//case RW_D16:// Implemented in default
	default:
		//tmp_data_16		=  *((const u16*)a_cpValueUsr);
		if( copy_from_user(&tmp_data_16,a_cpValueUsr,a_nChunkSize) )return -EFAULT;
		
		iowrite16(tmp_data_16,address);
		smp_wmb();
		
		break;
	}

	return (ssize_t)a_nChunkSize;
}



static ssize_t WriteDev1_Prvt(u_int a_unMode,void* address,const void* a_cpValue,const void* a_cpMask)
//ssize_t WriteDev(u_int a_unMode,int a_nChunkSize,void* address,const void* a_cpValueUsr,const void* a_cpMaskUsr)
{

	int		nChunkSize;
	u8		tmp_data_8;
	u16		tmp_data_16;
	u32		tmp_data_32;

	u8		tmp_data_Prev_8;
	u16		tmp_data_Prev_16;
	u32		tmp_data_Prev_32;
	
	u8		tmp_data_Mask_8;
	u16		tmp_data_Mask_16;
	u16		tmp_data_Mask_32;
	

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

	case RW_D16:
		nChunkSize = 2;
		tmp_data_16		=  *((const u16*)a_cpValue);
		
		iowrite16(tmp_data_16,address);
		smp_wmb();
		break;

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

	default:break;
	}

	return (ssize_t)nChunkSize;
}




ssize_t General_Write1(Pcie_gen_dev* a_dev, const char __user *buf, size_t count)
{

	const char*	cpcDataBuffer	= 0;// Buffer where read value will be stored
	const char*	cpcMaskBuffer	= 0;// Buffer where read value will be stored
	ssize_t		nSingleCall;		// Result for single iteration
	int			i;
	int			nTmpOffset		= 0;
	int			nChunkSize;
	const void*	cpData;
	const void*	cpMask;

	ssize_t		retval;
	int			tmp_offset		= 0;
	int			tmp_mode		= 0;
	int			tmp_barx		= 0;
	int			tmp_size_rw		= 0;
	u32			mem_tmp			= 0;
	device_rw2	reading;
	void*		address			= 0;


	//printk(KERN_ALERT "g_nNumInterupts = %d\n", g_nNumInterupts);

	if( mutex_lock_interruptible(&a_dev->m_dev_mut) )
	{
		return -ERESTARTSYS;
	}

	if( copy_from_user(&reading,buf,sizeof(device_rw2))  )
	{
		retval = -EFAULT;
		mutex_unlock(&a_dev->m_dev_mut);
		return retval;
	}


	tmp_mode     = reading.mode_rw;
	tmp_offset   = reading.offset_rw;
	tmp_barx     = reading.barx_rw;
	tmp_size_rw  = (int)count;


	if( GetAddressAndMemSize1(a_dev,tmp_barx,&address,&mem_tmp) )
	{
		mutex_unlock(&a_dev->m_dev_mut);
		return -EFAULT;
	}


	//nChunkSize = (tmp_mode==RW_D8 || tmp_mode==RW_BITES8) ? 1 : 2;
	nChunkSize = GetChunkSize1(tmp_mode);
	printk(KERN_ALERT "\nWriting:  nChunkSize = %d\n\n",nChunkSize);

	tmp_size_rw = (tmp_size_rw<=0) ? 1 : tmp_size_rw;

	printk(KERN_ALERT "\nlnDiff = %lx\n",reading.dataPtr);
	cpcDataBuffer = buf + reading.dataPtr;

	printk(KERN_ALERT "\nlnDiffMask = %lx\n",reading.maskPtr);
	cpcMaskBuffer = buf + reading.maskPtr;


	printk(KERN_ALERT "General_Write: address = %ld, mem_size = %d, tmp_barx = %d\n", 
					(long)address, (int)mem_tmp, (int)tmp_barx );


	retval = 0;
	/////////////////////////////////////////////////////////////////////////
	for(i = 0; (i<tmp_size_rw) && ((tmp_offset+nTmpOffset+nChunkSize)<mem_tmp); i++)
	{

		cpData = (const void*)( cpcDataBuffer + nChunkSize*i );
		cpMask = (const void*)( cpcMaskBuffer + nChunkSize*i );

		nSingleCall = WriteDev1(tmp_mode,nChunkSize,address+tmp_offset+nTmpOffset,cpData,cpMask);
		printk(KERN_ALERT "\nWriting:  i = %d, nSingleCall = %d, retval = %d\n\n",i,(int)nSingleCall,(int)retval);

		if( nSingleCall < 0 )
		{
			mutex_unlock(&a_dev->m_dev_mut);
			return nSingleCall;
		}

		retval += nSingleCall;
		nTmpOffset += nChunkSize;

	}


	mutex_unlock(&a_dev->m_dev_mut);
	return retval;
}
EXPORT_SYMBOL(General_Write1);



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

	int							i;
	int							err			= 0;
	pid_t						cur_proc;

	printk(KERN_ALERT "General_ioctl: IOCTRL CMD %u\n", cmd);

	if(!a_pDev->m_dev_sts)
	{
		printk(KERN_ALERT "General_ioctl:NO DEVICE %d\n", a_pDev->m_dev_num);
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
		printk( KERN_ALERT "General_ioctl:  access_ok Error\n");
		return -EFAULT;
	}
	
	//printk("TAMC200S_CTRL: 1 IOCTRL CMD %i\n", cmd);
	if (mutex_lock_interruptible(&a_pDev->m_dev_mut))return -ERESTARTSYS;


	switch(cmd)
	{
	case PCIE_GEN_REGISTER_FOR_IRQ:
		printk( KERN_ALERT "General_ioctl:  case PCIEGEN_REGISTER_FOR_IRQ\n");

		if( a_pDev->m_nInteruptWaiters < MAX_INTR_NUMB )
		{
			for( i = 0; i <  a_pDev->m_nInteruptWaiters; ++i )
			{
				if( cur_proc ==  a_pDev->m_IrqWaiters[i].m_nPID )
				{
					mutex_unlock(&a_pDev->m_dev_mut);
					return 0;
				}
			}

			a_pDev->m_IrqWaiters[ a_pDev->m_nInteruptWaiters].m_nPID = cur_proc;
			a_pDev->m_IrqWaiters[ a_pDev->m_nInteruptWaiters].m_pCred = current->cred;

			++a_pDev->m_nInteruptWaiters;
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

	const char*					cpcFileName;
	int							i;
	int							err			= 0;
	pid_t						cur_proc;
	Pcie_gen_dev*				dev = &a_pDev->m_PcieGen;

	cpcFileName = strrchr(__FILE__,'/') + 1;
	cpcFileName = cpcFileName ? cpcFileName : "Unknown File";
	printk(KERN_ALERT "\"%s\":\"General_ioctl2\":%d, cmd = %d,  arg = %ld\n",cpcFileName,__LINE__,(int)cmd,(long)arg);

	if(!dev->m_dev_sts)
	{
		printk(KERN_ALERT "General_ioctl2:%d NO DEVICE %d\n",__LINE__,dev->m_dev_num);
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
		printk( KERN_ALERT "General_ioctl:  access_ok Error\n");
		return -EFAULT;
	}
	
	//printk("TAMC200S_CTRL: 1 IOCTRL CMD %i\n", cmd);
	if (mutex_lock_interruptible(&dev->m_dev_mut))return -ERESTARTSYS;


	switch(cmd)
	{
	case PCIE_GEN_REGISTER_FOR_IRQ:
		printk( KERN_ALERT "General_ioctl2:  case PCIEGEN_REGISTER_FOR_IRQ\n");

		if( dev->m_nInteruptWaiters < MAX_INTR_NUMB )
		{
			for( i = 0; i <  dev->m_nInteruptWaiters; ++i )
			{
				if( cur_proc ==  dev->m_IrqWaiters[i].m_nPID )
				{
					mutex_unlock(&dev->m_dev_mut);
					return 0;
				}
			}

			dev->m_IrqWaiters[ dev->m_nInteruptWaiters].m_nPID = cur_proc;
			dev->m_IrqWaiters[ dev->m_nInteruptWaiters].m_pCred = current->cred;

			++dev->m_nInteruptWaiters;
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

	default:
		mutex_unlock(&dev->m_dev_mut);
		return -ENOTTY;
	}

	
	mutex_unlock(&dev->m_dev_mut);

	return 0;

}
EXPORT_SYMBOL(General_ioctl2);



//#if LINUX_VERSION_CODE < 132632 
//static void pcie_gen_do_work(void *tamc200dev)
//#else
//static void pcie_gen_do_work(struct work_struct *work_str)
void pcie_gen_do_work(struct work_struct *work_str)
//#endif 
{

	static int nInterupt = 0;

	int i;
	struct task_struct *p;


	struct str_pcie_gen *dev   =  container_of(work_str, struct str_pcie_gen, pcie_gen_work);
	
	if(!(dev->m_PcieGen.m_dev_sts)) return;

	++nInterupt;

	memset(&dev->m_PcieGen.m_info, 0, sizeof(struct siginfo));
	dev->m_PcieGen.m_info.si_signo = SIGUSR1;/// Piti poxvi
	dev->m_PcieGen.m_info.si_code = SI_QUEUE;

	if(dev->m_nSigVal == -1 )
	{
		dev->m_PcieGen.m_info.si_value.sival_int = nInterupt;
	}
	else
	{
		dev->m_PcieGen.m_info.si_value.sival_int = dev->m_nSigVal;
	}


	if( nInterupt / 1000 == 0 )
	{
		printk(KERN_ALERT "==========pcie_gen_do_work, sizeof(g_info.si_code) = %d, g_info.si_code = %d\n", 
		           (int)sizeof(dev->m_PcieGen.m_info.si_code), dev->m_PcieGen.m_info.si_code );
		printk(KERN_ALERT "InteruptWaiters = %d\n\n",dev->m_PcieGen.m_nInteruptWaiters);
	}

	for( i = 0; i < dev->m_PcieGen.m_nInteruptWaiters; ++i )
	{
		p = pid_task(find_vpid(dev->m_PcieGen.m_IrqWaiters[i].m_nPID), PIDTYPE_PID);

		if( !p )
		{
			RemovePID1( &dev->m_PcieGen, dev->m_PcieGen.m_IrqWaiters[i].m_nPID );
			continue;
		}

		if( kill_pid_info_as_cred(SIGUSR1,&dev->m_PcieGen.m_info,
									find_vpid(dev->m_PcieGen.m_IrqWaiters[i].m_nPID),
									dev->m_PcieGen.m_IrqWaiters[i].m_pCred,0) )
		{
			RemovePID1(  &dev->m_PcieGen,  dev->m_PcieGen.m_IrqWaiters[i].m_nPID );
		}
	}

	dev->m_nSigVal = -1;


}




/*
 * The top-half interrupt handler.
 */
//#if LINUX_VERSION_CODE < 0x20613 // irq_handler_t has changed in 2.6.19
//static irqreturn_t pcie_gen_interrupt(int irq, void *dev_id, struct pt_regs *regs)
//#else
//static irqreturn_t pcie_gen_interrupt(int irq, void *dev_id)
irqreturn_t pcie_gen_interrupt(int irq, void *dev_id)
//#endif
{

	int i;
	u_int unMask = 0xffffffff;
	struct str_pcie_gen * dev = dev_id;

	if( !dev->m_PcieGen.m_dev_sts )return IRQ_NONE;

	queue_work(dev->pcie_gen_workqueue , &(dev->pcie_gen_work));

	for( i = 0; i < NUMBER_OF_BARS; ++i )
	{
		WriteDev1_Prvt(	dev->m_rst_irq[i].mode_rw,
						(char*)dev->m_PcieGen.m_Memories[i].m_memory_base + dev->m_rst_irq[i].offset_rw,
						&dev->m_rst_irq[i].data_rw, &unMask );
	}

	//iowrite16(0xFFFF,  ((void *)dev->m_PcieGen.m_Memories[2].m_memory_base +0xC));
    //smp_wmb();

	//iowrite16(0xFFFF,  ((void *)dev->m_PcieGen.m_Memories[3].m_memory_base +0x3A));
    //smp_wmb();

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
		printk(KERN_ALERT "Unable to enable pci device\n");
		return err;
	}




	if(a_pDev->irq_on)
	{
		printk(KERN_ALERT "PCIE_GEN_GAIN_ACCESS: Initing work\n");
#if LINUX_VERSION_CODE < 132632 
		INIT_WORK(&(a_pDev->pcie_gen_work),pcie_gen_do_work, &g_vPci_gens[i]);
#else
		INIT_WORK(&(a_pDev->pcie_gen_work),pcie_gen_do_work);    
#endif
		a_pDev->pcie_gen_workqueue = create_workqueue(a_DRV_NAME);
	}




	pci_set_master(dev);

	tmp_devfn    = (u32)dev->devfn;
	busNumber    = (u32)dev->bus->number;
	devNumber    = (u32)PCI_SLOT(tmp_devfn);
	funcNumber   = (u32)PCI_FUNC(tmp_devfn);
	tmp_bus_func = ((busNumber & 0xFF)<<8) + (devNumber & 0xFF);
	printk(KERN_ALERT "PCIE_GEN_GAIN_ACCESS:DEVFN %X, BUS_NUM %X, DEV_NUM %X, FUNC_NUM %X, BUS_FUNC %x\n",
		tmp_devfn, busNumber, devNumber, funcNumber, tmp_bus_func);

	tmp_devfn  = (u32)dev->bus->parent->self->devfn;
	busNumber  = (u32)dev->bus->parent->self->bus->number;
	devNumber  = (u32)PCI_SLOT(tmp_devfn);
	funcNumber = (u32)PCI_FUNC(tmp_devfn);
	printk(KERN_ALERT "PCIE_GEN_GAIN_ACCESS:DEVFN %X, BUS_NUM %X, DEV_NUM %X, FUNC_NUM %X\n",
		tmp_devfn, busNumber, devNumber, funcNumber);

	pcie_cap = pci_find_capability (dev->bus->parent->self, PCI_CAP_ID_EXP);
	printk(KERN_INFO "PCIE_GEN_GAIN_ACCESS: PCIE SWITCH CAP address %X\n",pcie_cap);

	pci_read_config_dword(dev->bus->parent->self, (pcie_cap + PCI_EXP_SLTCAP), &tmp_slot_cap);
	tmp_slot_num = (tmp_slot_cap >> 19);
	tmp_dev_num  = tmp_slot_num;
	printk(KERN_ALERT "TAMC200_PROBE:SLOT NUM %d DEV NUM%d SLOT_CAP %X\n",
		tmp_slot_num,tmp_dev_num,tmp_slot_cap);    

	a_pDev->m_PcieGen.m_slot_num = tmp_slot_num;
	a_pDev->m_PcieGen.m_dev_num  = tmp_slot_num;
	a_pDev->bus_func = tmp_bus_func;
	a_pDev->pcie_gen_pci_dev = dev;
	dev_set_drvdata(&(dev->dev), a_pDev);
	a_pDev->pcie_gen_all_mems = 0;


	spin_lock_init(&a_pDev->irq_lock);
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
	printk(KERN_ALERT "TAMC200_PROBE:GET_PCI_CONF VENDOR %X DEVICE %X REV %X\n", 
		vendor_id, device_id, revision);
	printk(KERN_ALERT "TAMC200_PROBE:GET_PCI_CONF SUBVENDOR %X SUBDEVICE %X CLASS %X\n", 
		subvendor_id, subdevice_id, class_code);
	a_pDev->vendor_id      = vendor_id;
	a_pDev->device_id      = device_id;
	a_pDev->subvendor_id   = subvendor_id;
	a_pDev->subdevice_id   = subdevice_id;
	a_pDev->class_code     = class_code;
	a_pDev->revision       = revision;
	a_pDev->irq_line       = irq_line;
	a_pDev->irq_pin        = irq_pin;
	a_pDev->pci_dev_irq    = pci_dev_irq;

	printk(KERN_ALERT "PCIE_GEN_GAIN_ACCESS:GET_PCI_CONF1 VENDOR %X DEVICE %X REV %X\n", 
		a_pDev->vendor_id, 
		a_pDev->device_id, a_pDev->revision);
	printk(KERN_ALERT "PCIE_GEN_GAIN_ACCESS:GET_PCI_CONF1 SUBVENDOR %X SUBDEVICE %X CLASS %X\n", 
		a_pDev->subvendor_id, 
		a_pDev->subdevice_id, a_pDev->class_code);

	if( EBUSY == pci_request_regions(dev, a_DRV_NAME ) )
	{
		printk(KERN_ALERT "EBUSY\n");
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
			printk(KERN_INFO "PCIE_GEN_GAIN_ACCESS: mem_region %d address %lX remaped address %lX\n", j, 
					(unsigned long int)res_start, 
					(unsigned long int)a_pDev->m_PcieGen.m_Memories[j].m_memory_base);
			a_pDev->m_PcieGen.m_Memories[j].m_rw_off = (res_end - res_start);
			printk(KERN_INFO "================= memorySize[%d] = %d\n",j,(int)(res_end - res_start));
			a_pDev->pcie_gen_all_mems++;
		}
		else
		{
			a_pDev->m_PcieGen.m_Memories[j].m_memory_base = 0;
			a_pDev->m_PcieGen.m_Memories[j].m_rw_off = 0;
			printk(KERN_INFO "PCIE_GEN_GAIN_ACCESS: NO BASE_%d address\n",j);
		}
	}


	printk(KERN_ALERT "PCIE_GEN_GAIN_ACCESS: MAJOR = %d, MINOR = %d\n",a_MAJOR, a_pDev->m_PcieGen.m_dev_minor);
	
	a_pDev->m_PcieGen.m_dev_sts   = 1;
	device_create(a_class, NULL, MKDEV(a_MAJOR, a_pDev->m_PcieGen.m_dev_minor),
		&a_pDev->pcie_gen_pci_dev->dev, "pcie_gens%d", tmp_slot_num);

	//sys_chmod("/dev/aaa", 0x00000666 );
	//sys_gettid();






	if( a_pDev->irq_on )
	{

		printk(KERN_ALERT "==============PCIE_GEN_PROBE: Requestin interupt\n");

		a_pDev->irq_flag    = IRQF_SHARED | IRQF_DISABLED;
		a_pDev->pci_dev_irq = dev->irq;
		a_pDev->irq_mode    = 1;
		a_pDev->irq_on      = 1;
		result = request_irq(a_pDev->pci_dev_irq, pcie_gen_interrupt, 
			a_pDev->irq_flag, a_DRV_NAME, a_pDev);

		if (result) 
		{
			printk(KERN_ALERT "PCIE_GEN_PROBE: can't get assigned irq %i\n", a_pDev->irq_line);
			a_pDev->irq_mode = 0;
		}
		else
		{
			printk(KERN_ALERT "PCIE_GEN_PROBE:  assigned IRQ %i RESULT %i\n",
				a_pDev->pci_dev_irq, result);
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
	printk(KERN_ALERT "PCIE_GEN_REMOVE:REMOVE CALLED\n");

	//tamc200_dev = dev_get_drvdata(&(dev->dev));
	tmp_dev_num  = a_pDev->m_PcieGen.m_dev_num;
	tmp_slot_num = a_pDev->m_PcieGen.m_slot_num;
	printk(KERN_ALERT "PCIE_GEN_REMOVE: SLOT %d DEV %d\n", tmp_slot_num, tmp_dev_num);

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
		printk(KERN_ALERT "PCIE_GEN_REMOVE: UNMAPPING MEMORY %d\n",j);
		if(a_pDev->m_PcieGen.m_Memories[j].m_memory_base)
		{
			pci_iounmap(dev, a_pDev->m_PcieGen.m_Memories[j].m_memory_base);
		}
		a_pDev->m_PcieGen.m_Memories[j].m_memory_base = 0;
	}

	printk(KERN_ALERT "PCIE_GEN_REMOVE: MEMORYs UNMAPPED\n");
	pci_release_regions((a_pDev->pcie_gen_pci_dev));


	mutex_unlock(&(a_pDev->m_PcieGen.m_dev_mut));
	printk(KERN_ALERT "PCIE_GEN_REMOVE: MAJOR = %d, MINOR = %d\n",a_MAJOR, a_pDev->m_PcieGen.m_dev_minor);
	device_destroy(a_class,  MKDEV(a_MAJOR, a_pDev->m_PcieGen.m_dev_minor));
	//unregister_tamc200_proc(tmp_slot_num);
	a_pDev->m_PcieGen.m_dev_sts   = 0;
	//tamc200ModuleNum --;
	pci_disable_device(a_pDev->pcie_gen_pci_dev);
}
EXPORT_SYMBOL(Gen_ReleaseAccsess);
