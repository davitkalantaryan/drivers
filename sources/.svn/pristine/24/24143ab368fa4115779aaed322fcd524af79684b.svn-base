/*
 * SIS8300
 *
 * Interrupt Service Routine
 */

#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/sched.h>

#include "sis8300.h"
#include "sis8300_reg.h"

irqreturn_t sis8300_isr(int irq, void *dev_id){
  uint32_t intreg = 0;
  //unsigned long flags;
  sis8300_dev *sisdevice = (sis8300_dev *)dev_id;

  /* did my card issue an irq? */
  intreg = SIS8300REGREAD(sisdevice, IRQ_STATUS);
  if(intreg == 0){
    /* nope */
    //printk(KERN_INFO "sis8300: IRQ\n");
    return IRQ_NONE;
  }

  //printk(KERN_INFO "sis8300: IRQ on card:%08X\n", intreg);

  // clear irq on card
  SIS8300REGWRITE(sisdevice, IRQ_CLEAR, intreg);

  if(intreg & (1<<DMA_READ_DONE ^ 1<<DMA_WRITE_DONE)){
    sisdevice->intr_flag = 1;
     wake_up_interruptible(&sisdevice->intr_wait);
  }
  
  if(intreg & (1<<USER_IRQ)){
    sisdevice->usr_irq_flag = 1;
    wake_up_interruptible(&sisdevice->usr_irq_wait);
  }
  
  if(intreg & (1<<DAQ_DONE)){
    sisdevice->daq_done_irq_flag = 1;
    wake_up_interruptible(&sisdevice->daq_done_irq_wait);
  }

  return IRQ_HANDLED;
}
