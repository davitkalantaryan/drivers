#ifndef __pciedev_drv_exp_dv_h__
#define __pciedev_drv_exp_dv_h__

#include <linux/module.h>			/* u16, u32, .., MODULE_AUTHOR(), MODULE_DESCRIPTION(), ... */
#include <linux/cdev.h>				/* struct cdev */
#include <linux/pci.h>				/* pci_device_id  */
#include <linux/proc_fs.h>			/* proc_dir_entry */


#define NUMBER_OF_PCI_BASE		6
#define PCIEDEV_NR_DEVS			15				/* pciedev0 through pciedev11 */


struct SSingleDev;

typedef struct pciedev_dev2
{
	int					m_nRegionAllocated;
	int					m_nMajor;
	int					m_cnFirstMinor;
	int					m_nNDevices;
	int					m_cnTotalNumber;
	struct SSingleDev*	m_pDevices;
}pciedev_dev2;


typedef struct SSingleDev
{
	void*			m_pOwner;
	struct cdev		m_cdev;						/* Char device structure      */
	struct pci_dev*	pciedev_pci_dev;
	int				m_dev_minor;

	u32				mem_base[NUMBER_OF_PCI_BASE];
	u32				mem_base_end[NUMBER_OF_PCI_BASE];
	u32				mem_base_flag[NUMBER_OF_PCI_BASE];
	void __iomem*	memmory_base[NUMBER_OF_PCI_BASE];
	loff_t			rw_off[NUMBER_OF_PCI_BASE];
	int				pciedev_all_mems ;
}SSingleDev;




typedef struct module_dev {
	int                          brd_num;
	//    spinlock_t            irq_lock;
	struct timeval     dma_start_time;
	struct timeval     dma_stop_time;
	int                         waitFlag;
	u32                       dev_dma_size;
	u32                       dma_page_num;
	int                         dma_offset;
	int                         dma_order;
	//    wait_queue_head_t  waitDMA;
	struct pciedev_dev *parent_dev;
}module_dev;



extern int     register_pcie_device_exp(struct pci_dev* a_dev, struct file_operations* a_pciedev_fops,
										pciedev_dev2* a_pciedev_p, char *a_dev_name );

extern ssize_t pciedev_read_exp(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);

extern ssize_t pciedev_write_exp(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);

/*extern long    pciedev_ioctl_exp(struct file *filp, unsigned int *cmd_p, unsigned long *arg_p, 
								 pciedev_cdev * pciedev_cdev_m);*/

/*extern long    pciedev_ioctl_dma(struct file *filp, unsigned int *cmd_p, unsigned long *arg_p, 
								 pciedev_cdev * pciedev_cdev_m);*/

extern int     pciedev_open_exp( struct inode *inode, struct file *filp );

extern int     pciedev_release_exp(struct inode *inode, struct file *filp);

/*extern int     pciedev_remove_exp(struct pci_dev *dev, pciedev_cdev  **pciedev_cdev_p, 
								  char *dev_name, int * brd_num);*/

//extern int     pciedev_set_drvdata(struct pciedev_dev *dev, void *data);

//extern int     pciedev_get_brdnum(struct pci_dev *dev);

//extern int     pciedev_procinfo(char *buf, char **start, off_t fpos, int lenght, int *eof, void *data);



#endif/* #ifndef __pciedev_drv_exp_dv_h__ */

