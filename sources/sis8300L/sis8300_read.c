/*
 * SIS8300: read calls
 */

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <asm/uaccess.h>
#include <linux/delay.h>

#include "sis8300.h"
#include "sis8300_defs.h"
#include "sis8300_reg.h"
#include "sis8300_read.h"

ssize_t sub_sis8300_read(struct file *filp, char __user *buff, size_t count, loff_t offp);

ssize_t sis8300_read(struct file *filp, char __user *buff, size_t count, loff_t *offp){
  
  size_t len_remaining, len_local;
  loff_t offset;
  
  len_remaining = count;
  offset = 0;
  
  // slice transfers
  
  if(len_remaining == 0)
    return 0;
  
  do{
    
    if(len_remaining > SIS8300_KERNEL_DMA_BLOCK_SIZE){
      len_local = SIS8300_KERNEL_DMA_BLOCK_SIZE;
    }else{
      len_local = len_remaining;
    }
    
    sub_sis8300_read(filp, buff, len_local, offset);
    
    len_remaining -= len_local;
    offset += len_local;
    
  }while(len_remaining > 0); 
  
  return 0;
}

ssize_t sub_sis8300_read(struct file *filp, char __user *buff, size_t count, loff_t offp){
  
  uint64_t dmaspace_addr;
  uint32_t temp;
  int ret;
  
  sis8300_dev *sisdevice = SIS8300_DEV_FILE(filp);
    
  dmaspace_addr = (uint64_t)sisdevice->dmablock.dma_addr;

  // set destination address
  temp = (uint32_t)dmaspace_addr;
  SIS8300REGWRITE(sisdevice, DMA_READ_DST_ADR_LO32, temp);
  
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
  temp = (uint32_t)(dmaspace_addr >> 32);
#else
  temp = 0;  
#endif
  SIS8300REGWRITE(sisdevice, DMA_READ_DST_ADR_HI32, temp);
  
  // set transfer len
  SIS8300REGWRITE(sisdevice, DMA_READ_LEN, (uint32_t)count);
  
  // disable interrupt
  SIS8300REGWRITE(sisdevice, IRQ_ENABLE, 0xFFFF0000);
  
  // enable interrupt
  SIS8300REGWRITE(sisdevice, IRQ_ENABLE, (1<<DMA_READ_DONE));
  
  sisdevice->intr_flag = 0;
  // start
  SIS8300REGWRITE(sisdevice, DMA_READ_CTRL, (1<<DMA_READ_START));

  // wait for interrupt
  wait_event_interruptible(sisdevice->intr_wait, sisdevice->intr_flag != 0);
  
  ret = copy_to_user(buff + offp, sisdevice->dmablock.data, count);  
  
  return 0;
}



