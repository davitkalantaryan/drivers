#include <linux/module.h>
#include <linux/fs.h>	
#include <asm/uaccess.h>

#include "pciedev_ufn.h"
#include "pciedev_io.h"

ssize_t pciedev_read_exp(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    u64        itemsize      = 0;
    ssize_t    retval         = 0;
    int        minor          = 0;
    int        d_num          = 0;
    int        tmp_offset     = 0;
    int        tmp_mode       = 0;
    int        tmp_barx       = 0;
    int        tmp_rsrvd_rw   = 0;
    int        tmp_size_rw    = 0;
    int        tmp_cnt          = 0;
    u8         tmp_revision   = 0;
    u32        mem_tmp        = 0;
    int        i              = 0;
    int        scrtch_check   = 0;
    u8         tmp_data_8;
    u16        tmp_data_16;
    u32        tmp_data_32;
    device_rw  reading;
    void*       address;
    
    struct pciedev_dev *dev = filp->private_data;
    minor = dev->dev_minor;
    d_num = dev->dev_num;
    
    if(!dev->dev_sts){
        printk("PCIEDEV_READ_EXP: NO DEVICE %d\n", dev->dev_num);
        retval = -ENODEV;
        return retval;
    }
    
    
    /*Check if it is pread call*/
/*   
    printk(KERN_ALERT "PCIEDEV_READ_EXP:&F_POS %X &FILE->F_POS %X\n", f_pos, &(filp->f_pos));
    printk(KERN_ALERT "PCIEDEV_READ_EXP:F_POS %X FILE->F_POS %X\n", *f_pos, filp->f_pos);
    
    if ( ((long)f_pos & 0xFF) != ((long)(&filp->f_pos) & 0xFF) ){
	printk(KERN_ALERT "PCIEDEV_READ_EXP:PREAD called same ADDRESS %X:%X\n", ((long)f_pos & 0xFF), ((long)(&filp->f_pos) & 0xFF));
         return retval;
    }
*/
    
    if ( *f_pos != PCIED_FPOS ){
	//printk(KERN_ALERT "PCIEDEV_READ_EXP:PREAD called POS IS 7\n");
         return retval;
    }

    itemsize = sizeof(device_rw);

    if (mutex_lock_interruptible(&dev->dev_mut)){
                    return -ERESTARTSYS;
    }
    if (copy_from_user(&reading, buf, count)) {
            retval = -EIO;
            mutex_unlock(&dev->dev_mut);
            return retval;
    }
      
    tmp_mode     = reading.mode_rw;
    if(tmp_mode == RW_INFO){
        pci_read_config_byte(dev->pciedev_pci_dev, PCI_REVISION_ID, &tmp_revision);
        reading.offset_rw = dev->parent_dev->PCIEDEV_DRV_VER_MIN;
        reading.data_rw   = dev->parent_dev->PCIEDEV_DRV_VER_MAJ;
        reading.mode_rw   = tmp_revision;
        reading.barx_rw   = dev->pciedev_all_mems;
        reading.size_rw   = dev->slot_num; /*SLOT NUM*/
        retval            = itemsize;
        if (copy_to_user(buf, &reading, count)) {
             printk(KERN_ALERT "PCIEDEV_READ_EXP 3\n");
             retval = -EIO;
             mutex_unlock(&dev->dev_mut);
             return retval;
        }

        mutex_unlock(&dev->dev_mut);
        return retval;
    }
    tmp_offset   = reading.offset_rw;
    tmp_barx     = reading.barx_rw%NUMBER_OF_BARS;
    tmp_rsrvd_rw = reading.rsrvd_rw;
    tmp_size_rw  = reading.size_rw;

	if (!dev->memmory_base[tmp_barx]){
		retval = -ENOMEM;
		mutex_unlock(&dev->dev_mut);
		return retval;
	}

	address = (void *)dev->memmory_base[tmp_barx];
	mem_tmp = (dev->mem_base_end[tmp_barx] - 2);

    if(tmp_size_rw < 2){
        if(tmp_offset > (mem_tmp -2) || (!address)){
              reading.data_rw   = 0;
              retval            = 0;
        }else{
              switch(tmp_mode){
                case RW_D8:
                    tmp_offset = (tmp_offset/sizeof(u8))*sizeof(u8);
                    tmp_data_8        = ioread8(address + tmp_offset);
                    rmb();
                    reading.data_rw   = tmp_data_8 & 0xFF;
                    retval = itemsize;
                    if(reading.data_rw == 0xFF){
                        scrtch_check = pciedev_check_scratch(dev, RW_D8 );
                        if (scrtch_check){
                            retval = -ENOMEM;
                        }
                    }
                    break;
                case RW_D16:
                    tmp_offset = (tmp_offset/sizeof(u16))*sizeof(u16);
                    tmp_data_16       = ioread16(address + tmp_offset);
                    rmb();
                    reading.data_rw   = tmp_data_16 & 0xFFFF;
                    retval = itemsize;
                    if(reading.data_rw == 0xFFFF){
                        scrtch_check = pciedev_check_scratch(dev, RW_D16 );
                        if (scrtch_check){
                            retval = -ENOMEM;
                        }
                    }
                    break;
                case RW_D32:
                    tmp_offset = (tmp_offset/sizeof(u32))*sizeof(u32);
                    tmp_data_32       = ioread32(address + tmp_offset);
                    rmb();
                    reading.data_rw   = tmp_data_32 & 0xFFFFFFFF;
                    retval = itemsize;
                    if(reading.data_rw == 0xFFFFFFFF){
                        scrtch_check = pciedev_check_scratch(dev, RW_D32 );
                        if (scrtch_check){
                            retval = -ENOMEM;
                        }
                    }
                    break;
                default:
                    retval = EINVAL;
                    break;
              }
          }

        if (copy_to_user(buf, &reading, count)) {
             retval = -EIO;
             mutex_unlock(&dev->dev_mut);
             return retval;
        }
    }else{
          switch(tmp_mode){
            case RW_D8:
                tmp_offset = (tmp_offset/sizeof(u8))*sizeof(u8);
                if((tmp_offset + tmp_size_rw) > (mem_tmp -2) || (!address)){
                      reading.data_rw   = 0;
                      retval                   = -ENOMEM;
                }else{
                    tmp_cnt    = tmp_size_rw;
                    for(i = 0; i< tmp_cnt; i++){
                        tmp_data_8        = ioread8(address + tmp_offset + i);
                        rmb();
                        reading.data_rw   = tmp_data_8 & 0xFF;
                        retval = itemsize;
                        if (copy_to_user((buf + i), &tmp_data_8, 1)) {
                             retval = -EIO;
                             mutex_unlock(&dev->dev_mut);
                             return retval;
                        }
                    }
                }
                break;
            case RW_D16:
                 tmp_offset = (tmp_offset/sizeof(u16))*sizeof(u16);
                if((tmp_offset + tmp_size_rw*2) > (mem_tmp -2) || (!address)){
                      reading.data_rw   = 0;
                      retval            = -ENOMEM;
                }else{
                    tmp_cnt    = tmp_size_rw;
                    for(i = 0; i< tmp_cnt; i++){
                        tmp_data_16       = ioread16(address + tmp_offset + i*2);
                        rmb();
                        reading.data_rw   = tmp_data_16 & 0xFFFF;
                        retval = itemsize;
                        if (copy_to_user((buf + i*2), &tmp_data_16, 2)) {
                             retval = -EIO;
                             mutex_unlock(&dev->dev_mut);
                             return retval;
                        }
                    }
                }
                break;
            case RW_D32:
                tmp_offset = (tmp_offset/sizeof(u32))*sizeof(u32);
                if((tmp_offset + tmp_size_rw*4) > (mem_tmp -4) || (!address)){
                      reading.data_rw   = 0;
                      retval            = -ENOMEM;
                }else{
                    tmp_cnt    = tmp_size_rw;
                    for(i = 0; i< tmp_cnt; i++){
                        tmp_data_32       = ioread32(address + tmp_offset + i*4);
                        rmb();
                        reading.data_rw   = tmp_data_32 & 0xFFFFFFFF;
                        retval = itemsize;
                        if (copy_to_user((buf + i*4), &tmp_data_32, 4)) {
                             retval = -EIO;
                             mutex_unlock(&dev->dev_mut);
                             return retval;
                        }
                    }                  
                }
                break;
            default:
                retval = -EINVAL;
                break;
          }
    }
    mutex_unlock(&dev->dev_mut);
    return retval;
}
EXPORT_SYMBOL(pciedev_read_exp);

ssize_t pciedev_write_exp(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    device_rw       reading;
    int             itemsize       = 0;
    ssize_t         retval         = 0;
    int             minor          = 0;
    int             d_num          = 0;
    int             tmp_offset     = 0;
    int             tmp_mode       = 0;
    int             tmp_barx       = 0;
    int             tmp_cnt          = 0;
    int             i                      = 0;
    u32             mem_tmp        = 0;
    int             tmp_rsrvd_rw   = 0;
    int             tmp_size_rw    = 0;
    u16             tmp_data_8;
    u16             tmp_data_16;
    u32             tmp_data_32;
    void            *address ;
   
    struct pciedev_dev       *dev = filp->private_data;
    minor = dev->dev_minor;
    d_num = dev->dev_num;
    
    if(!dev->dev_sts){
        printk("PCIEDEV_WRITE_EXP: NO DEVICE %d\n", dev->dev_num);
        retval = -EFAULT;
        return retval;
    }
    
     /*Check if it is pwrite call*/
     if ( *f_pos != PCIED_FPOS ){
	//printk(KERN_ALERT "PCIEDEV_READ_EXP:PREAD called POS IS 7\n");
         return retval;
    }
    
    itemsize = sizeof(device_rw);
    
    if (mutex_lock_interruptible(&dev->dev_mut))
        return -ERESTARTSYS;
    
    if (copy_from_user(&reading, buf, count)) {
        retval = -EFAULT;
        mutex_unlock(&dev->dev_mut);
        return retval;
    }
    
    tmp_mode     = reading.mode_rw;
    tmp_offset   = reading.offset_rw;
    tmp_barx     = reading.barx_rw%NUMBER_OF_BARS;
    tmp_data_32  = reading.data_rw & 0xFFFFFFFF;
    tmp_rsrvd_rw = reading.rsrvd_rw;
    tmp_size_rw  = reading.size_rw;

	if (!dev->memmory_base[tmp_barx]){
		printk(KERN_ALERT "NO MEM UNDER BAR%d \n", tmp_barx);
		retval = -EFAULT;
		mutex_unlock(&dev->dev_mut);
		return retval;
	}
	address = (void *)dev->memmory_base[tmp_barx];
	mem_tmp = (dev->mem_base_end[tmp_barx] - 2);
    
    
    if(tmp_size_rw < 2){
        if(tmp_offset > (mem_tmp -2) || (!address)){
            reading.data_rw   = 0;
            retval            = 0;
        }else{
            switch(tmp_mode){
                case RW_D8:
                    tmp_offset = (tmp_offset/sizeof(u8))*sizeof(u8);
                    tmp_data_8 = tmp_data_32 & 0xFF;
                    iowrite8(tmp_data_8, ((void*)(address + tmp_offset)));
                    wmb();
                    retval = itemsize;
                    break;
                case RW_D16:
                    tmp_offset = (tmp_offset/sizeof(u16))*sizeof(u16);
                    tmp_data_16 = tmp_data_32 & 0xFFFF;
                    iowrite16(tmp_data_16, ((void*)(address + tmp_offset)));
                    wmb();
                    retval = itemsize;
                    break;
                case RW_D32:
                    tmp_offset = (tmp_offset/sizeof(u32))*sizeof(u32);
                    iowrite32(tmp_data_32, ((void*)(address + tmp_offset)));
                    wmb();
                    retval = itemsize;                
                    break;
                default:
                    tmp_offset = (tmp_offset/sizeof(u16))*sizeof(u16);
                    tmp_data_16 = tmp_data_32 & 0xFFFF;
                    iowrite16(tmp_data_16, ((void*)(address + tmp_offset)));
                    wmb();
                    retval = itemsize;
                    break;
            }
        }
    }else{
        switch(tmp_mode){
            case RW_D8:
                tmp_offset = (tmp_offset/sizeof(u8))*sizeof(u8);
                if((tmp_offset + tmp_size_rw) > (mem_tmp -2) || (!address)){
                      reading.data_rw   = 0;
                      retval                   = -ENOMEM;
                }else{
                    tmp_cnt    = tmp_size_rw;
                    for(i = 0; i< tmp_cnt; i++){
                        if (copy_from_user(&tmp_data_8, buf + DMA_DATA_OFFSET_BYTE + i, 1)) {
                            retval = -EFAULT;
                            mutex_unlock(&dev->dev_mut);
                            return retval;
                        }
                        tmp_data_8 = tmp_data_32 & 0xFF;
                        iowrite8(tmp_data_8, ((void*)(address + tmp_offset)));
                        wmb();
                        retval = itemsize;
                        
                    }
                }
                break;
            case RW_D16:
                 tmp_offset = (tmp_offset/sizeof(u16))*sizeof(u16);
                if((tmp_offset + tmp_size_rw*2) > (mem_tmp -2) || (!address)){
                      reading.data_rw   = 0;
                      retval            = -ENOMEM;
                }else{
                    tmp_cnt    = tmp_size_rw;
                    for(i = 0; i< tmp_cnt; i++){
                        if (copy_from_user(&tmp_data_16, buf + DMA_DATA_OFFSET_BYTE + i*2, 2)) {
                            retval = -EFAULT;
                            mutex_unlock(&dev->dev_mut);
                            return retval;
                        }
                        tmp_data_16 = tmp_data_32 & 0xFFFF;
                        iowrite16(tmp_data_16, ((void*)(address + tmp_offset)));
                        wmb();
                        retval = itemsize;
                    }
                }
                break;
            case RW_D32:
                tmp_offset = (tmp_offset/sizeof(u32))*sizeof(u32);
                if((tmp_offset + tmp_size_rw*4) > (mem_tmp -4) || (!address)){
                      reading.data_rw   = 0;
                      retval            = -ENOMEM;
                }else{
                    tmp_cnt    = tmp_size_rw;
                    for(i = 0; i< tmp_cnt; i++){
                        if (copy_from_user(&tmp_data_32, buf + DMA_DATA_OFFSET_BYTE + i*4, 4)) {
                            retval = -EFAULT;
                            mutex_unlock(&dev->dev_mut);
                            return retval;
                        }
                        tmp_data_16 = tmp_data_32 & 0xFFFF;
                        iowrite16(tmp_data_16, ((void*)(address + tmp_offset)));
                        wmb();
                        retval = itemsize;
                    }                  
                }
                break;
            default:
                retval = -EINVAL;
                break;
          }
        }
    
    mutex_unlock(&dev->dev_mut);
    return retval;
}
EXPORT_SYMBOL(pciedev_write_exp);

loff_t    pciedev_llseek(struct file *filp, loff_t off, int frm)
{
    ssize_t       retval         = 0;
    int             minor          = 0;
    int             d_num          = 0;
    
    struct pciedev_dev       *dev = filp->private_data;
    minor = dev->dev_minor;
    d_num = dev->dev_num;
        
    filp->f_pos = (loff_t)PCIED_FPOS;
    if(!dev->dev_sts){
        retval = -EFAULT;
        return retval;
    }
    return (loff_t)PCIED_FPOS;
}

int pciedev_check_scratch(struct pciedev_dev * dev, int rmode)
{
    int retval = 0;
    void            *address ;
    int              tmp_offset     = 0;
	u8         tmp_data_8;
	u16         tmp_data_16;
	u16         tmp_data_32;
	//char* mem_tmp;
	//u32             mem_tmp = 0;
    
    tmp_offset = dev->scratch_offset;

	if (!dev->memmory_base[dev->scratch_bar]){
		printk(KERN_ALERT "NO MEM UNDER BAR%d \n", dev->scratch_bar);
		retval = 1;
		mutex_unlock(&dev->dev_mut);
		return retval;
	}
	address = (void *)dev->memmory_base[dev->scratch_bar];
	//mem_tmp = (dev->mem_base_end[dev->scratch_bar] -2);
    
    
    switch(rmode){
        case RW_D8:
            tmp_data_8        = ioread8(address + tmp_offset);
            tmp_data_8 &= 0xFF;
            if(tmp_data_8 == 0xFF){
                retval = 1;
            }else{
                retval = 0;
            }
            break;
        case RW_D16:
            tmp_data_16        = ioread16(address + tmp_offset);
            tmp_data_16 &= 0xFFFF;
            if(tmp_data_16 == 0xFFFF){
                retval = 1;
            }else{
                retval = 0;
            }
            break;
        case RW_D32:
            tmp_data_32        = ioread32(address + tmp_offset);
            tmp_data_32 &= 0xFFFFFFFF;
            if(tmp_data_32 == 0xFFFFFFFF){
                retval = 1;
            }else{
                retval = 0;
            }
            break;
        default:
            retval =1;
        break;
    } 
    return retval;
}
