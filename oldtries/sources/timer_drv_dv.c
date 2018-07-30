//#include <linux/init.h>
//#include <linux/fs.h>				/*  */

#include "pciedev_drv_exp_dv.h"
#include "pciedev_drv_dv_io.h"


MODULE_AUTHOR("Davit Kalantaryan");
MODULE_DESCRIPTION("DESY timer AMC-PCIE board driver");
MODULE_VERSION("1.5.0");
MODULE_LICENSE("Dual BSD/GPL");


#define DEVNAME					"timer_drv_dvm"/* name of device */
//#define PCIEDEV_VENDOR_ID		0x10EE			/* XILINX vendor ID */
#define PCIEDEV_VENDOR_ID		PCI_ANY_ID			/* XILINX vendor ID */
#define PCIEDEV_DEVICE_ID		0x0088			/* PCIEDEV dev board device ID or 0x0088 */
#define PCIEDEV_SUBVENDOR_ID	PCI_ANY_ID		/* ESDADIO vendor ID */
#define PCIEDEV_SUBDEVICE_ID	PCI_ANY_ID		/* ESDADIO device ID */


struct pciedev_cdev*		pciedev_cdev_m = 0;
struct module_dev*			module_dev_pp;
struct module_dev*			module_dev_p[PCIEDEV_NR_DEVS];



static struct pci_device_id pciedev_ids[] __devinitdata = {
	{ PCIEDEV_VENDOR_ID, PCIEDEV_DEVICE_ID, PCIEDEV_SUBVENDOR_ID, PCIEDEV_SUBDEVICE_ID, 0, 0, 0},
    { 0, }
};
MODULE_DEVICE_TABLE(pci, pciedev_ids);



static ssize_t pciedev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	ssize_t    retval         = 0;
	retval  = pciedev_read_exp(filp, buf, count, f_pos);
	return retval;
}



static ssize_t pciedev_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	ssize_t         retval = 0;
	retval = pciedev_write_exp(filp, buf, count, f_pos);
	return retval;
}



static long  pciedev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long result = 0;

	//if (_IOC_TYPE(cmd) != PCIEDOOCS_IOC) return -ENOTTY; /*For future check if cmd is right*/
	/*if (_IOC_TYPE(cmd) == PCIEDOOCS_IOC){
		//if (_IOC_NR(cmd) <= PCIEDOOCS_IOC_MAXNR && _IOC_NR(cmd) >= PCIEDOOCS_IOC_MINNR) {
			result = pciedev_ioctl_exp(filp, &cmd, &arg, pciedev_cdev_m);
		//}else{
		//	result = pciedev_ioctl_dma(filp, &cmd, &arg, pciedev_cdev_m);
		//}
	}else{
		return -ENOTTY;
	}*/
	return result;
}



static int pciedev_open( struct inode *inode, struct file *filp )
{
	int    result = 0;
	result = pciedev_open_exp( inode, filp );
	return result;
}



static int pciedev_release(struct inode *inode, struct file *filp)
{
	int result            = 0;
	result = pciedev_release_exp(inode, filp);
	return result;
}



static struct file_operations pciedev_fops = {
	.owner                  =  THIS_MODULE,
	.read                     =  pciedev_read,
	.write                    =  pciedev_write,
	.unlocked_ioctl    =  pciedev_ioctl,
	.open                    =  pciedev_open,
	.release                =  pciedev_release,
};



static int __devinit pciedev_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	int result      = 0;
	int tmp_brd_num = -1;
	
	printk(KERN_ALERT "PCIEDEV_PROBE CALLED \n");
	result = register_pcie_device_exp(dev, &pciedev_fops, &pciedev_cdev_m, DEVNAME, &tmp_brd_num);
	printk(KERN_ALERT "PCIEDEV_PROBE_EXP CALLED  FOR BOARD %i\n", tmp_brd_num);
	
	/*if board has created we will create our structure and pass it to pcedev_dev*/
	if(!result)
	{
		printk(KERN_ALERT "PCIEDEV_PROBE_EXP CREATING CURRENT STRUCTURE FOR BOARD %i\n", tmp_brd_num);
		module_dev_pp = kzalloc(sizeof(module_dev), GFP_KERNEL);
		if(!module_dev_pp){return -ENOMEM;}
		printk(KERN_ALERT "PCIEDEV_PROBE CALLED; CURRENT STRUCTURE CREATED \n");
		module_dev_p[tmp_brd_num] = module_dev_pp;
		module_dev_pp->brd_num      = tmp_brd_num;
		module_dev_pp->parent_dev  = pciedev_cdev_m->pciedev_dev_m[tmp_brd_num];
		pciedev_set_drvdata(pciedev_cdev_m->pciedev_dev_m[tmp_brd_num], module_dev_p[tmp_brd_num]);
	}
	
	return result;
}



static void __devexit pciedev_remove(struct pci_dev *dev)
{
     int result               = 0;
     int tmp_slot_num = -1;
     int tmp_brd_num = -1;
     printk(KERN_ALERT "REMOVE CALLED\n");
     tmp_brd_num =pciedev_get_brdnum(dev);
     printk(KERN_ALERT "REMOVE CALLED FOR BOARD %i\n", tmp_brd_num);
     /* clean up any allocated resources and stuff here */
     kfree(module_dev_p[tmp_brd_num]);
     /*now we can call pciedev_remove_exp to clean all standard allocated resources
      will clean all interrupts if it seted 
      */
     result = pciedev_remove_exp(dev,  &pciedev_cdev_m, DEVNAME, &tmp_slot_num);
     printk(KERN_ALERT "PCIEDEV_REMOVE_EXP CALLED  FOR SLOT %i\n", tmp_slot_num);
}



static struct pci_driver pci_pciedev_driver = {
    .name       = DEVNAME,
    .id_table   = pciedev_ids,
    .probe      = pciedev_probe,
    .remove     = __devexit_p(pciedev_remove),
};



static void __exit pciedev_cleanup_module(void)
{
	printk(KERN_NOTICE "PCIEDEV_CLEANUP_MODULE CALLED\n");
	pci_unregister_driver(&pci_pciedev_driver);
	printk(KERN_NOTICE "PCIEDEV_CLEANUP_MODULE: PCI DRIVER UNREGISTERED\n");
}



static int __init pciedev_init_module(void)
{


	
	int   tmp_brd_num, result  = 0;

	u16 vendor_id;//
	u16 device_id;//
	u8  revision;//
	u8  irq_line;//
	u8  irq_pin;//
	u16 subvendor_id;//
	u16 subdevice_id;//
	u16 class_code;//
	int i = 0;

	struct pci_dev *pPCI_tmp, *pPCI;
	
	printk(KERN_ALERT "AFTER_INIT:REGISTERING PCI DRIVER\n");
	result = pci_register_driver(&pci_pciedev_driver);
	printk(KERN_ALERT "AFTER_INIT:REGISTERING PCI DRIVER RESUALT %d\n", result);
	if( result )return result;

	pPCI = pci_get_device( PCI_ANY_ID, PCI_ANY_ID,NULL);
	while( pPCI )
	{
		pci_read_config_word(pPCI, PCI_VENDOR_ID,   &vendor_id);
		pci_read_config_word(pPCI, PCI_DEVICE_ID,   &device_id);
		pci_read_config_word(pPCI, PCI_SUBSYSTEM_VENDOR_ID,   &subvendor_id);
		pci_read_config_word(pPCI, PCI_SUBSYSTEM_ID,   &subdevice_id);
		pci_read_config_word(pPCI, PCI_CLASS_DEVICE,   &class_code);
		pci_read_config_byte(pPCI, PCI_REVISION_ID, &revision);
		pci_read_config_byte(pPCI, PCI_INTERRUPT_LINE, &irq_line);
		pci_read_config_byte(pPCI, PCI_INTERRUPT_PIN, &irq_pin);

		/*printk("%i.    =================================\n",++i);
		printk("vendor_id    = %i\n",vendor_id);
		printk("device_id    = %i\n",device_id);
		printk("subvendor_id = %i\n",subvendor_id);
		printk("subdevice_id = %i\n",subdevice_id);
		printk("class_code   = %i\n",class_code);
		printk("revision     = %i\n",revision);
		printk("irq_line     = %i\n",irq_line);
		printk("irq_pin      = %i\n\n",irq_pin);*/

		if( ++i == 32 )
		{
			printk(KERN_ALERT "PCIEDEV_PROBE CALLED \n");
			result = register_pcie_device_exp(pPCI, &pciedev_fops, &pciedev_cdev_m, DEVNAME, &tmp_brd_num);
			printk(KERN_ALERT "PCIEDEV_PROBE_EXP CALLED  FOR BOARD %i\n", tmp_brd_num);
			
			/*if board has created we will create our structure and pass it to pcedev_dev*/
			if(!result)
			{
				printk(KERN_ALERT "PCIEDEV_PROBE_EXP CREATING CURRENT STRUCTURE FOR BOARD %i\n", tmp_brd_num);
				module_dev_pp = kzalloc(sizeof(module_dev), GFP_KERNEL);
				if(!module_dev_pp){return -ENOMEM;}
				printk(KERN_ALERT "PCIEDEV_PROBE CALLED; CURRENT STRUCTURE CREATED \n");
				module_dev_p[tmp_brd_num] = module_dev_pp;
				module_dev_pp->brd_num      = tmp_brd_num;
				module_dev_pp->parent_dev  = pciedev_cdev_m->pciedev_dev_m[tmp_brd_num];
				pciedev_set_drvdata(pciedev_cdev_m->pciedev_dev_m[tmp_brd_num], module_dev_p[tmp_brd_num]);
			}
		}

		pci_dev_put(pPCI);
		pPCI_tmp= pci_get_device( PCI_ANY_ID, PCI_ANY_ID,pPCI );
		pPCI = pPCI_tmp;
	}

	return 0; /* succeed */
}



module_init(pciedev_init_module);
module_exit(pciedev_cleanup_module);
