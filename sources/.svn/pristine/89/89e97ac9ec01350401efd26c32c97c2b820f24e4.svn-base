/*
 * SIS8300
 *
 * ioctl handling
 */

#ifndef SIS8300_IOCTL_H_
#define SIS8300_IOCTL_H_

/*
 * Prototypes
 */

/*
 * IOCTL entry point
 * branches to following functions
 */
long sis8300_ioctl(struct file *filp,
		  unsigned int cmd,
		  unsigned long arg);

/*
 * ioctl workhorses
 */
int _sis8300_register_read(sis8300_dev *device, sis8300_reg *reg);
int _sis8300_register_write(sis8300_dev *device, sis8300_reg *reg);
int _sis8300_wait_user_irq(sis8300_dev *device, sis8300_usr_irq *irq);
int _sis8300_wait_daq_irq(sis8300_dev *device, sis8300_daq_irq *irq);
int _sis8300_daq_dma_chain(sis8300_dev *device, sis8300_daq_dma *irq);

#endif
