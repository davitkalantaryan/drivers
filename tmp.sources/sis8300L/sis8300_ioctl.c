/*
 * SIS8300
 *
 * ioctl handling
 */

#include <linux/fs.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <asm/uaccess.h>
#include <linux/sched.h>

#include "sis8300.h"
#include "sis8300_defs.h"
#include "sis8300_ioctl.h"
#include "sis8300_reg.h"

union{
	sis8300_reg		reg;
	sis8300_usr_irq		irq;
}all_params;

#define PARAMS_MAX_SIZE (sizeof(all_params))

int _sub_daq_dma_chain(sis8300_dev *device, char __user *buff, size_t count, loff_t offp);

/***************************************************************/

long sis8300_ioctl(struct file *filp,
		  unsigned int cmd,
		  unsigned long arg){
  
	long ret = 0;
	sis8300_dev *sisdevice = SIS8300_DEV_FILE(filp);
	uint8_t data[_IOC_SIZE(cmd)];
	
	if(_IOC_TYPE(cmd) != SIS8300_MAGIC){
		printk(KERN_INFO "sis8300: ioctl: unknown command:%08x, %X, %c\n", cmd, _IOC_TYPE(cmd), _IOC_TYPE(cmd));
		return -EINVAL;
	}
	if(_IOC_NR(cmd) > SIS8300_MAXNR){
		printk(KERN_INFO "sis8300: ioctl: command nr too high: %08x\n", cmd);
		return -EINVAL;
	}

	/*
	 * is this a write ioctl call?
	 * if yes, i need to get the data from userland first
	 */

	if(cmd&IOC_IN){
		if (copy_from_user(&data, (void *)arg, _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	switch(cmd){
	case SIS8300_REG_READ:

		ret = _sis8300_register_read(sisdevice, (sis8300_reg *)data);
		break;

	case SIS8300_REG_WRITE:

		ret = _sis8300_register_write(sisdevice, (sis8300_reg *)data);
		break;

	case SIS8300_WAIT_USER_IRQ:
	  
		ret = _sis8300_wait_user_irq(sisdevice, (sis8300_usr_irq *)data);
		break;
		
	case SIS8300_WAIT_DAQ_IRQ:
	  ret = _sis8300_wait_daq_irq(sisdevice, (sis8300_daq_irq *)data);
	  break;
	  
	case SIS8300_WAIT_DAQ_DMA_IRQ:
	  ret = _sis8300_daq_dma_chain(sisdevice, (sis8300_daq_dma *)data);
	  break;
		
	default:
	  break;
  }

  /*
    * is this a read ioctl call?
    * if yes, i need to copy the data back to userland before i return
    */

  if(cmd&IOC_OUT){
    if (copy_to_user((void *)arg, &data, _IOC_SIZE(cmd)))
    return -EFAULT;
  }

  return ret;
}

/***************************************************************/

int _sis8300_register_read(sis8300_dev *device, sis8300_reg *reg){

  reg->data = ioread32((device->bar0) + reg->offset);
  return 0;
}

/***************************************************************/

int _sis8300_register_write(sis8300_dev *device, sis8300_reg *reg){

  iowrite32(reg->data, ((device->bar0) + reg->offset));
  return 0;
}

/***************************************************************/

int _sis8300_wait_user_irq(sis8300_dev *device, sis8300_usr_irq *irq){
  unsigned long jiffies;
  unsigned long ret;
  
  /* check args */
  if(irq->timeout == 0){
    return -1;
  }
  
  /* enable user irq */
  device->usr_irq_flag = 0;
  SIS8300REGWRITE(device, IRQ_ENABLE, 1<<USER_IRQ);
  
  /* wait for interrupt */
  jiffies = irq->timeout * HZ / 1000;
  
  ret = wait_event_interruptible_timeout(device->usr_irq_wait, device->usr_irq_flag != 0, jiffies);
  
  switch(ret){
  case 0:
    // timeout elapsed
    irq->status = 1;
    break;
  case -ERESTARTSYS:
    // error
    irq->status = 2;
    return ret;
    break;
  default:
    // irq happened
    irq->status = 0;
    break;
  }
  
  return 0;
}

/***************************************************************/

int _sis8300_wait_daq_irq(sis8300_dev *device, sis8300_daq_irq *irq){
  unsigned long jiffies;
  unsigned long ret;
  
  /* check args */
  if(irq->timeout == 0){
    return -1;
  }

  /* enable daq done irq */
  device->daq_done_irq_flag = 0;
  SIS8300REGWRITE(device, IRQ_ENABLE, 1<<DAQ_DONE);
  
  /* wait for interrupt */
  jiffies = irq->timeout * HZ / 1000;
  
  ret = wait_event_interruptible_timeout(device->daq_done_irq_wait, device->daq_done_irq_flag != 0, jiffies);
  
  switch(ret){
  case 0:
    // timeout elapsed
    irq->status = 1;
    break;
  case -ERESTARTSYS:
    // error
    irq->status = 2;
    return ret;
    break;
  default:
    // irq happened
    irq->status = 0;
    break;
  }
  
  /* disable daq done irq */
  SIS8300REGWRITE(device, IRQ_ENABLE, (1<<DAQ_DONE)<<16);
  
  return 0;
}

/***************************************************************/

int _sis8300_daq_dma_chain(sis8300_dev *device, sis8300_daq_dma *irq){
  size_t len_remaining, len_local;
  loff_t offset;
  uint64_t dmaspace_addr;
  uint32_t temp;
  size_t count;
  int ret;
  
  //printk(KERN_INFO "sis8300: _sis8300_daq_dma_chain: entry\n");
  /* setup dma */
  len_remaining = irq->read_len;
  offset = 0;
  
  if(len_remaining == 0)
    return 0;
  
  //printk(KERN_INFO "sis8300: _sis8300_daq_dma_chain: len: %d\n", len_remaining);
  
  /* prepare first dma block */    
  dmaspace_addr = (uint64_t)device->dmablock.dma_addr;
  // set destination address
  temp = (uint32_t)dmaspace_addr;
  SIS8300REGWRITE(device, DMA_READ_DST_ADR_LO32, temp);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
  temp = (uint32_t)(dmaspace_addr >> 32);
#else
  temp = 0;  
#endif
  SIS8300REGWRITE(device, DMA_READ_DST_ADR_HI32, temp);
  // set transfer len
  if(len_remaining > SIS8300_KERNEL_DMA_BLOCK_SIZE){
    count = SIS8300_KERNEL_DMA_BLOCK_SIZE;
  }else{
    count = len_remaining;
  }
  SIS8300REGWRITE(device, DMA_READ_LEN, (uint32_t)count);
  // disable interrupt
  SIS8300REGWRITE(device, IRQ_ENABLE, 0xFFFF0000);
  // enable interrupt
  SIS8300REGWRITE(device, IRQ_ENABLE, (1<<DMA_READ_DONE));
  device->intr_flag = 0;

  /* enable chain + arm sampling */
  SIS8300REGWRITE(device, DAQ_DMA_CHAIN, 1<<DAQ_DMA_CHAIN_ENABLE);
  
  SIS8300REGWRITE(device, SIS8300_ACQUISITION_CONTROL_STATUS_REG, 0x2);
  
  //printk(KERN_INFO "sis8300: _sis8300_daq_dma_chain: wait for first packet\n");
  
  // TH Test SIS8300REGWRITE(device, DMA_READ_CTRL, (1<<DMA_READ_START));

  // wait for interrupt
  device->intr_flag = 0; 
  wait_event_interruptible(device->intr_wait, device->intr_flag != 0);
  // copy first block
  ret = copy_to_user(irq->read_buf + offset, device->dmablock.data, count);
    
  len_remaining -= count;
  offset += count;
  
  //printk(KERN_INFO "sis8300: _sis8300_daq_dma_chain: got for first packet\n");
  //printk(KERN_INFO "sis8300: _sis8300_daq_dma_chain: remaining: %d\n", len_remaining);
  
  /* loop over data blocks */
  if(len_remaining){
    do{
      if(len_remaining > SIS8300_KERNEL_DMA_BLOCK_SIZE){
	len_local = SIS8300_KERNEL_DMA_BLOCK_SIZE;
      }else{
	len_local = len_remaining;
      }
      
      _sub_daq_dma_chain(device, irq->read_buf, len_local, offset);
      
      len_remaining -= len_local;
      offset += len_local;
      
    }while(len_remaining > 0);    
  }
  
  SIS8300REGWRITE(device, DAQ_DMA_CHAIN, 0);  
  
  return 0;
}

/***************************************************************/

int _sub_daq_dma_chain(sis8300_dev *device, char __user *buff, size_t count, loff_t offp){
  uint64_t dmaspace_addr;
  uint32_t temp;
  int ret;
    
  dmaspace_addr = (uint64_t)device->dmablock.dma_addr;

  // set destination address
  temp = (uint32_t)dmaspace_addr;
  SIS8300REGWRITE(device, DMA_READ_DST_ADR_LO32, temp);
  
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
  temp = (uint32_t)(dmaspace_addr >> 32);
#else
  temp = 0;  
#endif
  SIS8300REGWRITE(device, DMA_READ_DST_ADR_HI32, temp);
  
  // set transfer len
  SIS8300REGWRITE(device, DMA_READ_LEN, (uint32_t)count);
  
  // disable interrupt
  SIS8300REGWRITE(device, IRQ_ENABLE, 0xFFFF0000);
  
  // enable interrupt
  SIS8300REGWRITE(device, IRQ_ENABLE, (1<<DMA_READ_DONE));
  
  device->intr_flag = 0;
  // start
  SIS8300REGWRITE(device, DMA_READ_CTRL, (1<<DMA_READ_START));

  // wait for interrupt
  wait_event_interruptible(device->intr_wait, device->intr_flag != 0);
  
  ret = copy_to_user(buff + offp, device->dmablock.data, count);    
  
  return 0;
}
