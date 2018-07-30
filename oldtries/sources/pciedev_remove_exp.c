#include <linux/module.h>
#include <linux/fs.h>	
#include <linux/proc_fs.h>

#include "pciedev_ufn.h"

int pciedev_remove_exp(struct pci_dev *dev, pciedev_cdev  *pciedev_cdev_m, char *dev_name, int * brd_num)
{
     pciedev_dev                *pciedevdev;
     int                    tmp_dev_num  = 0;
     int                    tmp_slot_num  = 0;
     int                    m_brdNum      = 0;
     char                f_name[64];
     char                prc_entr[64];
	 int i;
     
     struct list_head *pos;
     struct list_head *npos;
     struct pciedev_prj_info  *tmp_prj_info_list;
     
    upciedev_file_list *tmp_file_list;
    //upciedev_file_list *tmp;
    struct list_head *fpos, *q;
     
     printk(KERN_ALERT "PCIEDEV_REMOVE_EXP CALLED\n");
    
    pciedevdev = dev_get_drvdata(&(dev->dev));
    tmp_dev_num  = pciedevdev->dev_num;
    tmp_slot_num  = pciedevdev->slot_num;
    m_brdNum      = pciedevdev->brd_num;
    *brd_num        = tmp_slot_num;
    sprintf(f_name, "%ss%d", dev_name, tmp_slot_num);
    sprintf(prc_entr, "%ss%d", dev_name, tmp_slot_num);
    printk(KERN_ALERT "PCIEDEV_REMOVE: SLOT %d DEV %d BOARD %i\n", tmp_slot_num, tmp_dev_num, m_brdNum);
    
    /* now let's be good and free the proj_info_list items. since we will be removing items
     * off the list using list_del() we need to use a safer version of the list_for_each() 
     * macro aptly named list_for_each_safe(). Note that you MUST use this macro if the loop 
     * involves deletions of items (or moving items from one list to another).
     */
    list_for_each_safe(pos,  npos, &pciedevdev->prj_info_list.prj_list ){
        tmp_prj_info_list = list_entry(pos, struct pciedev_prj_info, prj_list);
        list_del(pos);
        kfree(tmp_prj_info_list);
    }
    
    printk(KERN_ALERT "REMOVING IRQ_MODE %d\n", pciedevdev->irq_mode);
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

	for (i = 0; i < NUMBER_OF_BARS; ++i)
	{
		if (pciedevdev->memmory_base[i]){
			pci_iounmap(dev, pciedevdev->memmory_base[i]);
			pciedevdev->memmory_base[i] = 0;
			pciedevdev->mem_base[i] = 0;
			pciedevdev->mem_base_end[i] = 0;
			pciedevdev->mem_base_flag[i] = 0;
			pciedevdev->rw_off[i] = 0;
		}
	}
    
    pci_release_regions((pciedevdev->pciedev_pci_dev));
    printk(KERN_INFO "PCIEDEV_REMOVE:  DESTROY DEVICE MAJOR %i MINOR %i\n",
               pciedev_cdev_m->PCIEDEV_MAJOR, (pciedev_cdev_m->PCIEDEV_MINOR + pciedevdev->brd_num));
    device_destroy(pciedev_cdev_m->pciedev_class,  MKDEV(pciedev_cdev_m->PCIEDEV_MAJOR, 
                                                  pciedev_cdev_m->PCIEDEV_MINOR + pciedevdev->brd_num));
    
    unregister_upciedev_proc(tmp_slot_num, dev_name);
    
    pciedevdev->dev_sts   = 0;
    pciedevdev->binded   = 0;
    pciedev_cdev_m->pciedevModuleNum --;
    pci_disable_device(dev);
    
    list_for_each_safe(fpos, q, &(pciedevdev->dev_file_list.node_file_list)){
             tmp_file_list = list_entry(fpos, upciedev_file_list, node_file_list);
             //printk(KERN_ALERT "FILE_REF %i FILE_P %p\n", tmp_file_list->file_cnt, tmp_file_list->filp);
             tmp_file_list->filp->private_data  = pciedev_cdev_m->pciedev_dev_m[PCIEDEV_NR_DEVS]; 
             //printk(KERN_ALERT "DELETING FILE_LIST FILE_REF %i FILE_P %p\n", tmp_file_list->file_cnt, tmp_file_list->filp);
             list_del(fpos);
             kfree (tmp_file_list);
             pciedev_cdev_m->pciedev_dev_m[PCIEDEV_NR_DEVS]->dev_file_ref++;
    }

#ifdef _USE_DEPRICATED_
	pciedevdev->mem_base0 = pciedevdev->mem_base[0];
	pciedevdev->mem_base1 = pciedevdev->mem_base[1];
	pciedevdev->mem_base2 = pciedevdev->mem_base[2];
	pciedevdev->mem_base3 = pciedevdev->mem_base[3];
	pciedevdev->mem_base4 = pciedevdev->mem_base[4];
	pciedevdev->mem_base5 = pciedevdev->mem_base[5];

	pciedevdev->mem_base0_end = pciedevdev->mem_base_end[0];
	pciedevdev->mem_base1_end = pciedevdev->mem_base_end[1];
	pciedevdev->mem_base2_end = pciedevdev->mem_base_end[2];
	pciedevdev->mem_base3_end = pciedevdev->mem_base_end[3];
	pciedevdev->mem_base4_end = pciedevdev->mem_base_end[4];
	pciedevdev->mem_base5_end = pciedevdev->mem_base_end[5];

	pciedevdev->mem_base0_flag = pciedevdev->mem_base_flag[0];
	pciedevdev->mem_base1_flag = pciedevdev->mem_base_flag[1];
	pciedevdev->mem_base2_flag = pciedevdev->mem_base_flag[2];
	pciedevdev->mem_base3_flag = pciedevdev->mem_base_flag[3];
	pciedevdev->mem_base4_flag = pciedevdev->mem_base_flag[4];
	pciedevdev->mem_base5_flag = pciedevdev->mem_base_flag[5];

	pciedevdev->rw_off0 = pciedevdev->rw_off[0];
	pciedevdev->rw_off1 = pciedevdev->rw_off[1];
	pciedevdev->rw_off2 = pciedevdev->rw_off[2];
	pciedevdev->rw_off3 = pciedevdev->rw_off[3];
	pciedevdev->rw_off4 = pciedevdev->rw_off[4];
	pciedevdev->rw_off5 = pciedevdev->rw_off[5];

	pciedevdev->memmory_base0 = pciedevdev->memmory_base[0];
	pciedevdev->memmory_base1 = pciedevdev->memmory_base[1];
	pciedevdev->memmory_base2 = pciedevdev->memmory_base[2];
	pciedevdev->memmory_base3 = pciedevdev->memmory_base[3];
	pciedevdev->memmory_base4 = pciedevdev->memmory_base[4];
	pciedevdev->memmory_base5 = pciedevdev->memmory_base[5];
#endif  /* #ifdef _USE_DEPRICATED_ */
    
    mutex_unlock(&pciedevdev->dev_mut);
    return 0;
}
EXPORT_SYMBOL(pciedev_remove_exp);
