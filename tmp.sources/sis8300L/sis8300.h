/*
 * SIS8300 Dev Driver
 */

#ifndef SIS8300_H_
#define SIS8300_H_

#ifndef PCI_VENDOR_FZJZEL
#define PCI_VENDOR_FZJZEL 0x1796
#endif

#ifndef PCI_PRODUCT_SIS8300
#define PCI_PRODUCT_SIS8300 0x0018
#endif

#ifndef PCI_PRODUCT_SIS8300L
#define PCI_PRODUCT_SIS8300L 0x0019
#endif

#define DRIVER_MAJOR 1
#define DRIVER_MINOR 5
#define DRIVER_DATE "2013-02-26"
#define DRIVER_VENDOR "SIS GmbH"

/*
 * register accesses
 */
#define SIS8300REGWRITE(a, b, c) iowrite32(c, (a)->bar0 + (b))
#define SIS8300REGREAD(a, b)     ioread32((a)->bar0 + (b))

/*
 * dma space struct
 */

#define SIS8300_KERNEL_DMA_BLOCK_SIZE 131072 // 128kByte

typedef struct t_dmaspace{
	size_t size;
	uint8_t *data;
	dma_addr_t dma_addr;
}dmaspace;


typedef struct t_sis8300_dev{
  uint32_t __iomem *bar0;
  dev_t drvnum;
  struct class *drvclass;
  struct device *drvdevice;
  struct cdev *drvcdev;
  struct pci_dev *pdev;
  dmaspace dmablock;
  wait_queue_head_t intr_wait;
  wait_queue_head_t usr_irq_wait;
  wait_queue_head_t daq_done_irq_wait;
  int intr_flag;
  int usr_irq_flag;
  int daq_done_irq_flag;
  unsigned int majorNum;
}sis8300_dev;

int sis8300_open(struct inode *, struct file *);
int sis8300_release(struct inode *, struct file *);

#endif // SIS8300_H_
