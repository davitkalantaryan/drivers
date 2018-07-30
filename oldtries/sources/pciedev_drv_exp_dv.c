#include <linux/fs.h>				/* alloc_chrdev_region */
#include <linux/interrupt.h>		/* IRQF_SHARED */
#include <linux/sched.h>
//#include <asm/percpu.h>
//#include <linux/compiler.h>

#include "pciedev_drv_exp_dv.h"
#include "pciedev_drv_dv_io.h"



int register_pcie_device_exp(struct pci_dev* a_dev, struct file_operations* a_pciedev_fops, 
							 pciedev_dev2* a_pciedev_p, char *a_dev_name )
{

	int i;
	int devno;			// used to obtain device number for registration
	int result;			// used for errors
	dev_t devt	= 0;	//
	SSingleDev* pSDev;

	u32 res_start;
	u32 res_end;
	u32 res_flag;

	printk( KERN_NOTICE "Registering new device: register_pcie_device_exp() is called.\n" );

	if( !a_pciedev_p->m_nRegionAllocated )
	{
		result = alloc_chrdev_region(&devt, a_pciedev_p->m_cnFirstMinor, a_pciedev_p->m_cnTotalNumber, 
									a_dev_name);

		if( result )
		{
			printk( KERN_NOTICE "Unable to alloc char device region.\n" );
		}

		a_pciedev_p->m_nMajor = MAJOR(devt);
	}
	else if( a_pciedev_p->m_nNDevices == a_pciedev_p->m_cnTotalNumber )
	{
		printk( KERN_NOTICE "Maximum number of devices (%i) riched.\n", a_pciedev_p->m_cnTotalNumber );
		return -1;
	}

	pSDev = &( (a_pciedev_p->m_pDevices)[a_pciedev_p->m_nNDevices] );
	
	pSDev->m_pOwner = a_pciedev_p;
	cdev_init(&(pSDev->m_cdev), a_pciedev_fops);
	devno = MKDEV( a_pciedev_p->m_nMajor, a_pciedev_p->m_cnFirstMinor + ((a_pciedev_p->m_nNDevices)++) );
	result = cdev_add(&(pSDev->m_cdev), devno, 1);

	if (result)
	{
		printk(KERN_NOTICE "Error %d adding devno%d num%d", result, devno, ((a_pciedev_p->m_nNDevices)--));
		return -2;
	}


	if ((result = pci_enable_device(a_dev)))
	{
		printk("Device enabling error: %i\n", result);
		return result;
	}


	/*PLX8311 memory initialisation */
	//pci_write_config_word(dev, PCI_COMMAND, 3);


	/*******SETUP BARs******/
	pci_request_regions(a_dev, a_dev_name);

	for( i = 0; i < NUMBER_OF_PCI_BASE; ++i )
	{
		res_start  = pci_resource_start(a_dev, i);
		res_end    = pci_resource_end(a_dev, i);
		res_flag   = pci_resource_flags(a_dev, i);
		(pSDev->mem_base)[i] = res_start;
		(pSDev->mem_base_end)[i] = res_end;
		(pSDev->mem_base_flag)[i] = res_flag;
		if(res_start)
		{
			(pSDev->memmory_base)[i] = pci_iomap(a_dev, 0, (res_end - res_start));
			printk(KERN_INFO "PCIEDEV_PROBE: mem_region %i address %X  SIZE %X FLAG %X\n",i,
						   res_start, (res_end - res_start), res_flag);
			(pSDev->rw_off)[i] = (res_end - res_start);
			(pSDev->pciedev_all_mems) = (0x1 < i);
		}
		else
		{
			(pSDev->memmory_base)[i] = 0;
			(pSDev->rw_off)[i]		= 0;
			printk(KERN_INFO "PCIEDEV: NO BASE0 address\n");
		}
	}


	a_pciedev_p->m_nRegionAllocated = 1;
	return 0;

}
EXPORT_SYMBOL(register_pcie_device_exp);



int pciedev_open_exp( struct inode *inode, struct file *filp )
{
	int    minor;
	struct SSingleDev *pDev;

	minor = iminor(inode);
	pDev = container_of(inode->i_cdev, struct SSingleDev, m_cdev);
	pDev->m_dev_minor     = minor;
	filp->private_data  = pDev; 
	printk(KERN_ALERT "Open Procces is \"%s\" (pid %i) DEV is %d \n", current->comm, current->pid, minor);
	return 0;
}
EXPORT_SYMBOL(pciedev_open_exp);



ssize_t pciedev_read_exp(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	size_t		itemsize		= 0;
	size_t		count_temp;
	ssize_t		retval			= 0;
	int			tmp_mode		= 0;
	u8			tmp_revision	= 0;
	int			tmp_offset		= 0;
	int			tmp_rsrvd_rw	= 0;
	int			tmp_barx		= 0;
	int			tmp_size_rw		= 0;
	u8			tmp_data_8;
	u16			tmp_data_16;
	u32			tmp_data_32;
	u32			mem_tmp			= 0;
	int			i				= 0;
	device_rw  reading;
	void       *address;
	
	struct SSingleDev *pDev = filp->private_data;
	pciedev_dev2* pParent = pDev->m_pOwner;
	//minor = dev->dev_minor;
	
	itemsize = sizeof(device_rw);

	count_temp = count > itemsize ? itemsize : count;
	
	if( copy_from_user(&reading, buf, count_temp) )
	{
		retval = -EFAULT;
		return retval;
	}
	
	tmp_mode     = reading.mode_rw;
	if(tmp_mode == RW_INFO)
	{
		pci_read_config_byte(pDev->pciedev_pci_dev, PCI_REVISION_ID, &tmp_revision);
		//reading.offset_rw = pDev->parent_dev->PCIEDEV_DRV_VER_MIN;
		reading.data_rw   = pParent->m_nMajor;
		reading.mode_rw   = tmp_revision;
		reading.barx_rw   = pDev->pciedev_all_mems;
		//reading.size_rw   = pDev->slot_num; /*SLOT NUM*/
        retval            = itemsize;
		
		if( copy_to_user(buf, &reading, count))
		{
			printk(KERN_ALERT "PCIEDEV_READ_EXP 3\n");
			retval = -EFAULT;
			return retval;
		}
		
		return retval;
	}
	tmp_offset   = reading.offset_rw;
	tmp_barx     = reading.barx_rw;
	tmp_rsrvd_rw = reading.rsrvd_rw;
	tmp_size_rw  = reading.size_rw;

	tmp_barx %= NUMBER_OF_PCI_BASE;

	if(!(pDev->memmory_base)[tmp_barx])
	{
		retval = -EFAULT;
		return retval;
	}
	//address = (void *) pDev->memmory_base0;
	//mem_tmp = (pDev->mem_base0_end -2);
	address = (void *) (pDev->memmory_base)[tmp_barx];
	mem_tmp = ((pDev->mem_base_end)[tmp_barx] - 2);

	if((tmp_offset + tmp_size_rw) > (mem_tmp -2) || (!address))
	{
		reading.data_rw   = 0;
		retval            = 0;
		return retval;
	}
	
	if(tmp_size_rw < 2)
	{
		switch(tmp_mode)
		{
		case RW_D8:
			tmp_data_8        = ioread8(address + tmp_offset);
			rmb();
			reading.data_rw   = tmp_data_8 & 0xFF;
			retval = itemsize;
			break;
		case RW_D16:
			tmp_data_16       = ioread16(address + tmp_offset);
			rmb();
			reading.data_rw   = tmp_data_16 & 0xFFFF;
			retval = itemsize;
			break;
		case RW_D32:
			tmp_data_32       = 0xABCD;
			tmp_data_32       = ioread32(address + tmp_offset);
			rmb();
			reading.data_rw   = tmp_data_32 & 0xFFFFFFFF;
			retval = itemsize;
			break;
		default:
			tmp_data_16       = ioread16(address + tmp_offset);
			rmb();
			reading.data_rw   = tmp_data_16 & 0xFFFF;
			retval = itemsize;
			break;
		}

		if( copy_to_user(buf, &reading, count) )
		{
			retval = -EFAULT;
			retval = 0;
			return retval;
		}
	
	}
	else/* if(tmp_size_rw < 2) */
	{
		retval = 0;
		switch(tmp_mode)
		{
		case RW_D8:
			for(i = 0; i< tmp_size_rw; i++)
			{
				tmp_data_8        = ioread8(address + tmp_offset + i);
				rmb();
				reading.data_rw   = tmp_data_8 & 0xFF;
				if (copy_to_user((buf + i), &reading, 1))
				{
					retval = -EFAULT;
					return retval;
				}
				retval += 1;
			}
			break;
		case RW_D16:
			for(i = 0; i< tmp_size_rw; i++)
			{
				tmp_data_16       = ioread16(address + tmp_offset);
				rmb();
				reading.data_rw   = tmp_data_16 & 0xFFFF;
				if( copy_to_user((buf + i*2), &reading, 2) )
				{
					retval = -EFAULT;
					return retval;
				}
				retval += 2;
			}
			break;
		case RW_D32:
			for(i = 0; i< tmp_size_rw; i++)
			{
				tmp_data_32       = 0xABCD;
				tmp_data_32       = ioread32(address + tmp_offset);
				rmb();
				reading.data_rw   = tmp_data_32 & 0xFFFFFFFF;
				if( copy_to_user((buf + i*4), &reading, 4) )
				{
					retval = -EFAULT;
					return retval;
				}
				retval += 4;
			}
			break;
		default:
			for(i = 0; i< tmp_size_rw; i++)
			{
				tmp_data_16       = ioread16(address + tmp_offset);
				rmb();
				reading.data_rw   = tmp_data_16 & 0xFFFF;
				if( copy_to_user((buf + i*2), &reading, 2) )
				{
					retval = -EFAULT;
					return retval;
				}
				retval += 2;
			}
			break;
		}//switch
    }//else

    return retval;
}
EXPORT_SYMBOL(pciedev_read_exp);



ssize_t pciedev_write_exp(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	
	device_rw		reading;
	size_t			itemsize		= 0;
	ssize_t			retval			= 0;
	int				tmp_mode		= 0;
	int				tmp_offset		= 0;
	int				tmp_barx		= 0;
	int				tmp_rsrvd_rw	= 0;
	int				tmp_size_rw		= 0;
	u32				mem_tmp			= 0;
	u8				tmp_data_8;
    u16				tmp_data_16;
	u32				tmp_data_32;

	//int				minor			= 0;
    /*int             d_num          = 0;
    */
	void            *address ;
	
	struct SSingleDev       *pDev = filp->private_data;
	
	itemsize = sizeof(device_rw);
    
    
	if( copy_from_user(&reading, buf, count) )
	{
		retval = -EFAULT;
		return retval;
	}
    
	tmp_mode     = reading.mode_rw;
	tmp_offset   = reading.offset_rw;
	tmp_barx     = reading.barx_rw;
	tmp_data_32  = reading.data_rw & 0xFFFFFFFF;
	tmp_rsrvd_rw = reading.rsrvd_rw;
	tmp_size_rw  = reading.size_rw;

	tmp_barx %= NUMBER_OF_PCI_BASE;
	printk(KERN_ALERT "NO MEM UNDER BAR%i \n",tmp_barx);

	if(!(pDev->memmory_base)[tmp_barx])
	{
		retval = -EFAULT;
		return retval;
	}

	address    = (void *)(pDev->memmory_base)[tmp_barx];
	mem_tmp = ((pDev->mem_base_end)[tmp_barx] - 2);

	if(tmp_offset > (mem_tmp -2) || (!address))
	{
		reading.data_rw   = 0;
		retval            = 0;
		return retval;
	}


	switch(tmp_mode)
	{
	case RW_D8:
		tmp_data_8 = tmp_data_32 & 0xFF;
		iowrite8(tmp_data_8, ((void*)(address + tmp_offset)));
		wmb();
		retval = itemsize;
		break;
	case RW_D16:
		tmp_data_16 = tmp_data_32 & 0xFFFF;
		iowrite16(tmp_data_16, ((void*)(address + tmp_offset)));
		wmb();
		retval = itemsize;
		break;
	case RW_D32:
		iowrite32(tmp_data_32, ((void*)(address + tmp_offset)));
		wmb();
		retval = itemsize;                
		break;
	default:
		tmp_data_16 = tmp_data_32 & 0xFFFF;
		iowrite16(tmp_data_16, ((void*)(address + tmp_offset)));
		wmb();
		retval = itemsize;
		break;
	}
	
	return retval;

}
EXPORT_SYMBOL(pciedev_write_exp);



long pciedev_ioctl_exp(struct file *filp, unsigned int *cmd_p, unsigned long *arg_p, SSingleDev * pciedev_cdev_m)
{

#if 0
	unsigned int    cmd;
    unsigned long arg;
    pid_t                cur_proc = 0;
    int                    minor    = 0;
    int                    d_num    = 0;
    int                    retval   = 0;
    int                    err      = 0;
    struct pci_dev* pdev;
    u8              tmp_revision = 0;
    u_int           tmp_offset;
    u_int           tmp_data;
    u_int           tmp_cmd;
    u_int           tmp_reserved;
    int io_size;
    device_ioctrl_data  data;
    struct pciedev_dev       *dev  = filp->private_data;

    cmd              = *cmd_p;
    arg                = *arg_p;
    io_size           = sizeof(device_ioctrl_data);
    minor           = dev->dev_minor;
    d_num         = dev->dev_num;	
    cur_proc     = current->group_leader->pid;
    pdev            = (dev->pciedev_pci_dev);




	/// Jnjelu
	printk("PCIEDEV_IOCTRL DEVICE %d\n", dev->dev_num);
	return 0;


    
    if(!dev->dev_sts){
        printk("PCIEDEV_IOCTRL: NO DEVICE %d\n", dev->dev_num);
        retval = -EFAULT;
        return retval;
    }
        
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
     if (err) return -EFAULT;

    if (mutex_lock_interruptible(&dev->dev_mut))
                    return -ERESTARTSYS;

    switch (cmd) {
        case PCIEDEV_PHYSICAL_SLOT:
            retval = 0;
            if (copy_from_user(&data, (device_ioctrl_data*)arg, (size_t)io_size)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            tmp_offset   = data.offset;
            tmp_data     = data.data;
            tmp_cmd      = data.cmd;
            tmp_reserved = data.reserved;
            data.data    = dev->slot_num;
            data.cmd     = PCIEDEV_PHYSICAL_SLOT;
            if (copy_to_user((device_ioctrl_data*)arg, &data, (size_t)io_size)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            break;
        case PCIEDEV_DRIVER_VERSION:
            data.data   =  pciedev_cdev_m->PCIEDEV_DRV_VER_MAJ;
            data.offset =  pciedev_cdev_m->PCIEDEV_DRV_VER_MIN;
            if (copy_to_user((device_ioctrl_data*)arg, &data, (size_t)io_size)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            break;
        case PCIEDEV_FIRMWARE_VERSION:
            pci_read_config_byte(dev->pciedev_pci_dev, PCI_REVISION_ID, &tmp_revision);
            data.data   = tmp_revision;
            data.offset = dev->revision;
            if (copy_to_user((device_ioctrl_data*)arg, &data, (size_t)io_size)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            break;
        
        default:
            return -ENOTTY;
            break;
    }
    mutex_unlock(&dev->dev_mut);
    return retval;
#endif

	return 0;
}
EXPORT_SYMBOL(pciedev_ioctl_exp);



long pciedev_ioctl_dma(struct file *filp, unsigned int *cmd_p, unsigned long *arg_p, SSingleDev * pciedev_cdev_m)
{

#if 0
	unsigned int    cmd;
    unsigned long arg;
    pid_t                cur_proc = 0;
    int                    minor    = 0;
    int                    d_num    = 0;
    int                    retval   = 0;
    int                    err      = 0;
    struct pci_dev* pdev;
    ulong           value;
    u_int	    tmp_dma_size;
    u_int	    tmp_dma_trns_size;
    u_int	    tmp_dma_offset;
    void*           pWriteBuf           = 0;
    int             tmp_order           = 0;
    unsigned long   length              = 0;
    dma_addr_t      pTmpDmaHandle;
    u32             dma_sys_addr ;
    int               tmp_source_address  = 0;
    u_int           tmp_dma_control_reg = 0;
    u_int           tmp_dma_len_reg     = 0;
    u_int           tmp_dma_src_reg     = 0;
    u_int           tmp_dma_dest_reg    = 0;
    u_int           tmp_dma_cmd         = 0;

    int size_time;
    int io_dma_size;
    device_ioctrl_time  time_data;
    device_ioctrl_dma   dma_data;

    module_dev       *module_dev_pp;
    pciedev_dev       *dev  = filp->private_data;
    module_dev_pp = (module_dev*)(dev->dev_str);

    cmd              = *cmd_p;
    arg                = *arg_p;
    size_time      = sizeof (device_ioctrl_time);
    io_dma_size = sizeof(device_ioctrl_dma);
    minor           = dev->dev_minor;
    d_num         = dev->dev_num;	
    cur_proc     = current->group_leader->pid;
    pdev            = (dev->pciedev_pci_dev);



	/// Jnjelu
	printk("PCIEDEV_IOCTRL (DMA) DEVICE %d\n", dev->dev_num);
	return 0;


    
    if(!dev->dev_sts){
        printk(KERN_ALERT "PCIEDEV_IOCTRL: NO DEVICE %d\n", dev->dev_num);
        retval = -EFAULT;
        return retval;
    }
        
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
     if (err) return -EFAULT;

    if (mutex_lock_interruptible(&dev->dev_mut))
                    return -ERESTARTSYS;

    switch (cmd) {
        case PCIEDEV_GET_DMA_TIME:
            retval = 0;
            
            module_dev_pp->dma_start_time.tv_sec+= (long)dev->slot_num;
            module_dev_pp->dma_stop_time.tv_sec  += (long)dev->slot_num;
            module_dev_pp->dma_start_time.tv_usec+= (long)dev->brd_num;
            module_dev_pp->dma_stop_time.tv_usec  += (long)dev->brd_num;
            time_data.start_time = module_dev_pp->dma_start_time;
            time_data.stop_time  = module_dev_pp->dma_stop_time;
            if (copy_to_user((device_ioctrl_time*)arg, &time_data, (size_t)size_time)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            break;
        case PCIEDEV_READ_DMA:
            retval = 0;
            if (copy_from_user(&dma_data, (device_ioctrl_dma*)arg, (size_t)io_dma_size)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }

            tmp_dma_control_reg = (dma_data.dma_reserved1 >> 16) & 0xFFFF;
            tmp_dma_len_reg     = dma_data.dma_reserved1 & 0xFFFF;
            tmp_dma_src_reg     = (dma_data.dma_reserved2 >> 16) & 0xFFFF;
            tmp_dma_dest_reg    = dma_data.dma_reserved2 & 0xFFFF;
            tmp_dma_cmd         = dma_data.dma_cmd;
            tmp_dma_size        = dma_data.dma_size;
            tmp_dma_offset      = dma_data.dma_offset;

            printk (KERN_ALERT "PCIEDEV_READ_DMA: tmp_dma_control_reg %X, tmp_dma_len_reg %X\n",
                   tmp_dma_control_reg, tmp_dma_len_reg);
            printk (KERN_ALERT "PCIEDEV_READ_DMA: tmp_dma_src_reg %X, tmp_dma_dest_reg %X\n",
                   tmp_dma_src_reg, tmp_dma_dest_reg);
            printk (KERN_ALERT "PCIEDEV_READ_DMA: tmp_dma_cmd %X, tmp_dma_size %X\n",
                   tmp_dma_cmd, tmp_dma_size);

            break; //for testing
            
            module_dev_pp->dev_dma_size     = tmp_dma_size;
             if(tmp_dma_size <= 0){
                 mutex_unlock(&dev->dev_mut);
                 return EFAULT;
            }
            tmp_dma_trns_size    = tmp_dma_size;
            if((tmp_dma_size%PCIEDEV_DMA_SYZE)){
                tmp_dma_trns_size    = tmp_dma_size + (tmp_dma_size%PCIEDEV_DMA_SYZE);
            }
            value    = HZ/1; /* value is given in jiffies*/
            length   = tmp_dma_size;
            tmp_order = get_order(tmp_dma_trns_size);
            module_dev_pp->dma_order = tmp_order;
            pWriteBuf = (void *)__get_free_pages(GFP_KERNEL | __GFP_DMA, tmp_order);
            pTmpDmaHandle      = pci_map_single(pdev, pWriteBuf, tmp_dma_trns_size, PCI_DMA_FROMDEVICE);

            tmp_source_address = tmp_dma_offset;
            dma_sys_addr       = (u32)(pTmpDmaHandle & 0xFFFFFFFF);
            /* MAKE DMA TRANSFER*/
            pci_unmap_single(pdev, pTmpDmaHandle, tmp_dma_trns_size, PCI_DMA_FROMDEVICE);
             if (copy_to_user ((void *)arg, pWriteBuf, tmp_dma_size)) {
                retval = -EFAULT;
            }
            free_pages((ulong)pWriteBuf, tmp_order);
            break;
        default:
            return -ENOTTY;
            break;
    }
    mutex_unlock(&dev->dev_mut);
    return retval;

#endif

	return 0;

}
EXPORT_SYMBOL(pciedev_ioctl_dma);




int    pciedev_release_exp(struct inode *inode, struct file *filp)
{
	/*int minor            = 0;
	int d_num           = 0;
	u16 cur_proc     = 0;
	struct pciedev_dev2 *dev   = filp->private_data;
	minor     = dev->dev_minor;
	d_num   = dev->dev_num;
	cur_proc = current->group_leader->pid;
	return 0;*/


	pciedev_dev2* pPCI_dev;
	dev_t              devno ;
	SSingleDev* pDev;

	printk( KERN_NOTICE "Device rejection\n" );

	//pDev = container_of(inode->i_cdev, struct pciedev_dev2, cdev);
	pDev = filp->private_data;
	pPCI_dev = pDev->m_pOwner;

	if( pPCI_dev->m_nMajor != 0)
	{
		//cdev_del(my_cdevp);
		cdev_del(&(pDev->m_cdev));
		devno = MKDEV(pPCI_dev->m_nMajor, pDev->m_dev_minor);
		unregister_chrdev_region(devno, 1);
	}

	return 0;
}
EXPORT_SYMBOL(pciedev_release_exp);



int pciedev_remove_exp(struct pci_dev *dev, SSingleDev  **pciedev_cdev_p, char *dev_name, int * brd_num)
{

#if 0
	pciedev_dev                *pciedevdev;
     pciedev_cdev              *pciedev_cdev_m;
     int                    tmp_dev_num  = 0;
     int                    tmp_slot_num  = 0;
     int                    brdNum            = 0;
     int                    brdCnt              = 0;
     int                    m_brdNum      = 0;
     char                f_name[64];
     char                prc_entr[11];
     dev_t              devno ;
     printk(KERN_ALERT "PCIEDEV_REMOVE_EXP CALLED\n");
    
    pciedev_cdev_m = *pciedev_cdev_p;
    devno = MKDEV(pciedev_cdev_m->PCIEDEV_MAJOR, pciedev_cdev_m->PCIEDEV_MINOR);
    pciedevdev = dev_get_drvdata(&(dev->dev));
    tmp_dev_num  = pciedevdev->dev_num;
    tmp_slot_num  = pciedevdev->slot_num;
    m_brdNum       = pciedevdev->brd_num;
    * brd_num        = tmp_slot_num;
    sprintf(f_name, "%ss%d", dev_name, tmp_slot_num);
    sprintf(prc_entr, "%ss%d", dev_name, tmp_slot_num);
    printk(KERN_ALERT "PCIEDEV_REMOVE: SLOT %d DEV %d BOARD %i\n", tmp_slot_num, tmp_dev_num, m_brdNum);
    
    
    printk(KERN_ALERT "REMOVING IRQ_MODE %d\n", pciedevdev->irq_mode);

	/// Jnjelu
	return 0;




    if(pciedevdev->irq_mode){
       printk(KERN_ALERT "FREE IRQ\n");
       free_irq(pciedevdev->pci_dev_irq, pciedevdev);
       printk(KERN_ALERT "REMOVING IRQ\n");
       if(pciedevdev->msi){
           printk(KERN_ALERT "DISABLE MSI\n");
           pci_disable_msi((pciedevdev->pciedev_pci_dev));
       }
    }else{
        if(pciedevdev->msi){
           printk(KERN_ALERT "DISABLE MSI\n");
           pci_disable_msi((pciedevdev->pciedev_pci_dev));
       }
    }
     
    printk(KERN_ALERT "REMOVE: UNMAPPING MEMORYs\n");
    mutex_lock_interruptible(&pciedevdev->dev_mut);
                    
    if(pciedevdev->memmory_base0){
       pci_iounmap(dev, pciedevdev->memmory_base0);
       pciedevdev->memmory_base0  = 0;
       pciedevdev->mem_base0      = 0;
       pciedevdev->mem_base0_end  = 0;
       pciedevdev->mem_base0_flag = 0;
       pciedevdev->rw_off0         = 0;
    }
    if(pciedevdev->memmory_base1){
       pci_iounmap(dev, pciedevdev->memmory_base1);
       pciedevdev->memmory_base1  = 0;
       pciedevdev->mem_base1      = 0;
       pciedevdev->mem_base1_end  = 0;
       pciedevdev->mem_base1_flag = 0;
       pciedevdev->rw_off1        = 0;
    }
    if(pciedevdev->memmory_base2){
       pci_iounmap(dev, pciedevdev->memmory_base2);
       pciedevdev->memmory_base2  = 0;
       pciedevdev->mem_base2      = 0;
       pciedevdev->mem_base2_end  = 0;
       pciedevdev->mem_base2_flag = 0;
       pciedevdev->rw_off2        = 0;
    }
    if(pciedevdev->memmory_base3){
       pci_iounmap(dev, pciedevdev->memmory_base3);
       pciedevdev->memmory_base3  = 0;
       pciedevdev->mem_base3      = 0;
       pciedevdev->mem_base3_end  = 0;
       pciedevdev->mem_base3_flag = 0;
       pciedevdev->rw_off3         = 0;
    }
    if(pciedevdev->memmory_base4){
       pci_iounmap(dev, pciedevdev->memmory_base4);
       pciedevdev->memmory_base4  = 0;
       pciedevdev->mem_base4      = 0;
       pciedevdev->mem_base4_end  = 0;
       pciedevdev->mem_base4_flag = 0;
       pciedevdev->rw_off4        = 0;
    }
    if(pciedevdev->memmory_base5){
       pci_iounmap(dev, pciedevdev->memmory_base5);
       pciedevdev->memmory_base5  = 0;
       pciedevdev->mem_base5      = 0;
       pciedevdev->mem_base5_end  = 0;
       pciedevdev->mem_base5_flag = 0;
       pciedevdev->rw_off5        = 0;
    }
    pci_release_regions((pciedevdev->pciedev_pci_dev));
    mutex_unlock(&pciedevdev->dev_mut);
    printk(KERN_INFO "PCIEDEV_REMOVE:  DESTROY DEVICE MAJOR %i MINOR %i\n",
               pciedev_cdev_m->PCIEDEV_MAJOR, (pciedev_cdev_m->PCIEDEV_MINOR + pciedevdev->brd_num));
    device_destroy(pciedev_cdev_m->pciedev_class,  MKDEV(pciedev_cdev_m->PCIEDEV_MAJOR, 
                                                  pciedev_cdev_m->PCIEDEV_MINOR + pciedevdev->brd_num));
    remove_proc_entry(prc_entr,0);
    
    pciedevdev->dev_sts   = 0;
    pciedev_cdev_m->pciedevModuleNum --;
    pci_disable_device(dev);
    mutex_destroy(&pciedevdev->dev_mut);
    cdev_del(&pciedevdev->cdev);
    brdNum = pciedevdev->brd_num;
    kfree(pciedev_cdev_m->pciedev_dev_m[brdNum]);
    pciedev_cdev_m->pciedev_dev_m[brdNum] = 0;
    
    /*Check if no more modules free main chrdev structure */
    brdCnt             = 0;
    if(pciedev_cdev_m){
        for(brdNum = 0;brdNum < PCIEDEV_NR_DEVS;brdNum++){
            if(pciedev_cdev_m->pciedev_dev_m[brdNum]){
                printk(KERN_INFO "PCIEDEV_REMOVE:  EXIST BOARD %i\n", brdNum);
                brdCnt++;
            }
        }
        if(!brdCnt){
            printk(KERN_INFO "PCIEDEV_REMOVE:  NO MORE BOARDS\n");
            unregister_chrdev_region(devno, PCIEDEV_NR_DEVS);
            class_destroy(pciedev_cdev_m->pciedev_class);
            kfree(pciedev_cdev_m);
            pciedev_cdev_m = 0;
        }
    }
#endif
    return 0;
}
EXPORT_SYMBOL(pciedev_remove_exp);


/*
int pciedev_set_drvdata(struct pciedev_dev *dev, void *data)
{
    if(!dev)
        return 1;
    dev->dev_str = data;
    return 0;
}
EXPORT_SYMBOL(pciedev_set_drvdata);*/



int pciedev_procinfo(char *buf, char **start, off_t fpos, int lenght, int *eof, void *data)
{

#if 0
    char *p;
    pciedev_dev     *pciedev_dev_m ;

    pciedev_dev_m = (pciedev_dev*)data;
    p = buf;
    p += sprintf(p,"Driver Version:\t%i.%i\n", pciedev_dev_m->parent_dev->PCIEDEV_DRV_VER_MAJ, pciedev_dev_m->parent_dev->PCIEDEV_DRV_VER_MIN);
    p += sprintf(p,"Board NUM:\t%i\n", pciedev_dev_m->brd_num);
    p += sprintf(p,"Slot    NUM:\t%i\n", pciedev_dev_m->slot_num);
 /*
    p += sprintf(p,"Bord ID:\t%i\n", PCIEDEV_BORD_ID);
    p += sprintf(p,"Bord Version;\t%i\n",PCIEDEV_BORD_VR);
    p += sprintf(p,"Bord Date:\t%i\n",PCIEDEV_BORD_DT);
    p += sprintf(p,"Bord HW Ver:\t%i\n",PCIEDEV_BORD_HW_VR);
    p += sprintf(p,"Number of Proj:\t%i\n", PCIEDEV_PROJ_NM);
    p += sprintf(p,"Proj ID:\t%i\n", PCIEDEV_PROJ_ID);
    p += sprintf(p,"Proj Version;\t%i\n",PCIEDEV_PROJ_VR);
    p += sprintf(p,"Proj Date:\t%i\n",PCIEDEV_PROJ_DT);
*/

    *eof = 1;
    return p - buf;
#endif

	return 0;
}
EXPORT_SYMBOL(pciedev_procinfo);
