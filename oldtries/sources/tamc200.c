#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/ioport.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/mm.h>
#include <linux/kdev_t.h>
#include <linux/device.h>
#include <asm/scatterlist.h>
#include <asm/io.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>	/* copy_*_user */
#include <asm/page.h>

#include "tamc200_fnc.h"      	/* local definitions */
#include "tamc200_io.h"      	/* local definitions */


MODULE_AUTHOR("Lyudvig Petrosyan");
MODULE_DESCRIPTION("tamc200 driver for TEWS TAMC200 IP carrier");
MODULE_VERSION("1.1.0");
MODULE_LICENSE("Dual BSD/GPL");

static u16 TAMC200_DRV_VER_MAJ=1;
static u16 TAMC200_DRV_VER_MIN=1;

struct tamc200_dev       tamc200_dev[TAMC200_NR_DEVS];	/* allocated in iptimer_init_module */
struct class             *tamc200_class;
static int               tamc200ModuleNum = 0;

static int    tamc200_procinfo(char *, char **, off_t, int, int *,void *);
static struct proc_dir_entry *tamc200_procdir;

static void register_tamc200_proc(int num, struct tamc200_dev *dev)
{
    char prc_entr[11];
    sprintf(prc_entr, "tamc200s%i", num);
    tamc200_procdir = create_proc_entry(prc_entr, S_IFREG | S_IRUGO, 0);
    tamc200_procdir->read_proc = tamc200_procinfo;
    tamc200_procdir->data = dev;
}

static void unregister_tamc200_proc(int num)
{
    char prc_entr[11];
    sprintf(prc_entr, "tamc200s%i", num);
    remove_proc_entry(prc_entr,0);
}


static int tamc200_procinfo(char *buf, char **start, off_t fpos, int lenght, int *eof, void *data)
{
	char *p;
         struct tamc200_dev *dev = data;

	p = buf;
	p += sprintf(p,"Driver Version:\t%i.%i\n", TAMC200_DRV_VER_MAJ, TAMC200_DRV_VER_MIN);
        p += sprintf(p,"FPGA Revision:\t%i\n", dev->TAMC200_PPGA_REV);
	p += sprintf(p,"Slot A ON:\t%i\n", dev->TAMC200_A_ON);
        p += sprintf(p,"Slot A type;\t%i\n",dev->TAMC200_A_TYPE);
        p += sprintf(p,"Slot A IP_ID;\t%i\n",dev->TAMC200_A_ID);
        p += sprintf(p,"Slot B ON:\t%i\n",dev->TAMC200_B_ON);
	p += sprintf(p,"Slot B type:\t%i\n",dev->TAMC200_B_TYPE);
        p += sprintf(p,"Slot B IP_ID:\t%i\n",dev->TAMC200_B_ID);
        p += sprintf(p,"Slot C ON:\t%i\n", dev->TAMC200_C_ON);
        p += sprintf(p,"Slot C type:\t%i\n", dev->TAMC200_C_TYPE);
        p += sprintf(p,"Slot C IP_ID:\t%i\n", dev->TAMC200_C_ID);

	*eof = 1;
	return p - buf;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
    #define kill_proc(p,s,v) kill_pid(find_vpid(p), s, v)
#endif

int TAMC200_MAJOR = 46;     /* static major by default */
int TAMC200_MINOR = 0;      /* static major by default */

/*ON/OFF, TYPE, IRQ-ON/OFF, IRQ_LEVEL, IRQ_VECTOR*/
static int ips_on[3]          = {1, 1, 1};
static int ips_type[3]        = {0, 0, 0};
static int ips_irq[3]         = {1, 1, 1};
static int ips_irq_lev[3]     = {0, 0, 1};
static int ips_irq_vec[3]     = {0xFC, 0xFD, 0xFE};
static int ips_irq_sens[3]    = {0, 1, 1};
static int ips_on_argc        = 0;
static int ips_type_argc      = 0;
static int ips_irq_argc       = 0;
static int ips_irq_lev_argc   = 0;
static int ips_irq_vec_argc   = 0;
static int ips_irq_sens_argc  = 0;
module_param_array(ips_on,       int, &ips_on_argc,      S_IRUGO|S_IWUSR);
module_param_array(ips_type,     int, &ips_type_argc,    S_IRUGO|S_IWUSR);
module_param_array(ips_irq,      int, &ips_irq_argc,     S_IRUGO|S_IWUSR);
module_param_array(ips_irq_lev,  int, &ips_irq_lev_argc, S_IRUGO|S_IWUSR);
module_param_array(ips_irq_vec,  int, &ips_irq_vec_argc, S_IRUGO|S_IWUSR);
module_param_array(ips_irq_sens, int, &ips_irq_sens_argc,S_IRUGO|S_IWUSR);

/*
 * Feeding the output queue to the device is handled by way of a
 * workqueue.
 */
#if LINUX_VERSION_CODE < 132632 
    static void tamc200_do_work(void *);
#else
    static void tamc200_do_work(struct work_struct *work_str);
#endif 
static struct workqueue_struct *tamc200_workqueue;

static struct pci_device_id tamc200_table[] __devinitdata = {
	{ TAMC200_VENDOR_ID, TAMC200_DEVICE_ID,
          TAMC200_SUBVENDOR_ID, TAMC200_SUBDEVICE_ID, 0, 0, 0},
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, tamc200_table);

#if LINUX_VERSION_CODE < 0x20613 // irq_handler_t has changed in 2.6.19
static irqreturn_t tamc200_interrupt(int irq, void *dev_id, struct pt_regs *regs);
#else
static irqreturn_t tamc200_interrupt(int irq, void *dev_id);
#endif

static int __devinit tamc200_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
    int err      = 0;
    int i        = 0;
    int k        = 0;
    int result   = 0;
    u16 vendor_id;
    u16 device_id;
    u8  revision;
    u8  irq_line;
    u8  irq_pin;
    u32 res_start;
    u32 res_end;
    u32 res_flag;
    u32 pci_dev_irq;
    int pcie_cap;
    u16 subvendor_id;
    u16 subdevice_id;
    u16 class_code;
    u32 tmp_devfn;
    u32 busNumber;
    u32 devNumber;
    u32 funcNumber;
    
    u32 tmp_slot_cap     = 0;
    u32 tmp_module_id;
    u16 tmp_data_16;
    
    int tmp_slot_num     = 0;
    int tmp_dev_num      = 0;
    int tmp_bus_func     = 0;
    u16 tmp_slot_cntrl   = 0;
    
    void *base_addres;
	
    /* Do probing type stuff here.  
     * Like calling request_region();
     */
    printk(KERN_ALERT "TAMC200_PROBE CALLED \n");
    if ((err = pci_enable_device(dev)))
        return err;

    pci_set_master(dev);
    
    tmp_devfn    = (u32)dev->devfn;
    busNumber    = (u32)dev->bus->number;
    devNumber    = (u32)PCI_SLOT(tmp_devfn);
    funcNumber   = (u32)PCI_FUNC(tmp_devfn);
    tmp_bus_func = ((busNumber & 0xFF)<<8) + (devNumber & 0xFF);
    printk(KERN_ALERT "TAMC200_PROBE:DEVFN %X, BUS_NUM %X, DEV_NUM %X, FUNC_NUM %X, BUS_FUNC %x\n",
                                      tmp_devfn, busNumber, devNumber, funcNumber, tmp_bus_func);
    
    tmp_devfn  = (u32)dev->bus->parent->self->devfn;
    busNumber  = (u32)dev->bus->parent->self->bus->number;
    devNumber  = (u32)PCI_SLOT(tmp_devfn);
    funcNumber = (u32)PCI_FUNC(tmp_devfn);
    printk(KERN_ALERT "TAMC200_PROBE:DEVFN %X, BUS_NUM %X, DEV_NUM %X, FUNC_NUM %X\n",
                                      tmp_devfn, busNumber, devNumber, funcNumber);
    
    pcie_cap = pci_find_capability (dev->bus->parent->self, PCI_CAP_ID_EXP);
    printk(KERN_INFO "TAMC200_PROBE: PCIE SWITCH CAP address %X\n",pcie_cap);
    
    pci_read_config_dword(dev->bus->parent->self, (pcie_cap +PCI_EXP_SLTCAP), &tmp_slot_cap);
    tmp_slot_num = (tmp_slot_cap >> 19);
    tmp_dev_num  = tmp_slot_num;
    printk(KERN_ALERT "TAMC200_PROBE:SLOT NUM %d DEV NUM%d SLOT_CAP %X\n",
                                   tmp_slot_num,tmp_dev_num,tmp_slot_cap);    
    
    tamc200_dev[tmp_slot_num].slot_num = tmp_slot_num;
    tamc200_dev[tmp_slot_num].dev_num  = tmp_slot_num;
    tamc200_dev[tmp_slot_num].bus_func = tmp_bus_func;
    tamc200_dev[tmp_slot_num].tamc200_pci_dev = dev;
    dev_set_drvdata(&(dev->dev), (&tamc200_dev[tmp_slot_num]));
    tamc200_dev[tmp_slot_num].tamc200_all_mems = 0;
    
    
    spin_lock_init(&tamc200_dev[tmp_slot_num].irq_lock);
    tamc200_dev[tmp_slot_num].softint        = 0;
    tamc200_dev[tmp_slot_num].count          = 0;
    tamc200_dev[tmp_slot_num].irq_mode       = 0;
    tamc200_dev[tmp_slot_num].irq_on         = 0;
    tamc200_dev[tmp_slot_num].ip_on          = 0;
    
    pci_read_config_word(dev, PCI_VENDOR_ID,   &vendor_id);
    pci_read_config_word(dev, PCI_DEVICE_ID,   &device_id);
    pci_read_config_word(dev, PCI_SUBSYSTEM_VENDOR_ID,   &subvendor_id);
    pci_read_config_word(dev, PCI_SUBSYSTEM_ID,   &subdevice_id);
    pci_read_config_word(dev, PCI_CLASS_DEVICE,   &class_code);
    pci_read_config_byte(dev, PCI_REVISION_ID, &revision);
    pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &irq_line);
    pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &irq_pin);

    pci_dev_irq = dev->irq;
    printk(KERN_ALERT "TAMC200_PROBE:GET_PCI_CONF VENDOR %X DEVICE %X REV %X\n", 
                                  vendor_id, device_id, revision);
    printk(KERN_ALERT "TAMC200_PROBE:GET_PCI_CONF SUBVENDOR %X SUBDEVICE %X CLASS %X\n", 
                                   subvendor_id, subdevice_id, class_code);
    tamc200_dev[tmp_slot_num].vendor_id      = vendor_id;
    tamc200_dev[tmp_slot_num].device_id      = device_id;
    tamc200_dev[tmp_slot_num].subvendor_id   = subvendor_id;
    tamc200_dev[tmp_slot_num].subdevice_id   = subdevice_id;
    tamc200_dev[tmp_slot_num].class_code     = class_code;
    tamc200_dev[tmp_slot_num].revision       = revision;
    tamc200_dev[tmp_slot_num].irq_line       = irq_line;
    tamc200_dev[tmp_slot_num].irq_pin        = irq_pin;
    tamc200_dev[tmp_slot_num].pci_dev_irq    = pci_dev_irq;

    printk(KERN_ALERT "TAMC200_PROBE:GET_PCI_CONF1 VENDOR %X DEVICE %X REV %X\n", 
             tamc200_dev[tmp_slot_num].vendor_id, 
             tamc200_dev[tmp_slot_num].device_id, tamc200_dev[tmp_slot_num].revision);
    printk(KERN_ALERT "TAMC200_PROBE:GET_PCI_CONF1 SUBVENDOR %X SUBDEVICE %X CLASS %X\n", 
             tamc200_dev[tmp_slot_num].subvendor_id, 
             tamc200_dev[tmp_slot_num].subdevice_id, tamc200_dev[tmp_slot_num].class_code);

    pci_request_regions(dev, "tamc200");
		
    res_start = pci_resource_start(dev, 0);
    res_end   = pci_resource_end(dev, 0);
    res_flag  = pci_resource_flags(dev, 0);
    tamc200_dev[tmp_slot_num].mem_base0       = res_start;
    tamc200_dev[tmp_slot_num].mem_base0_end   = res_end;
    tamc200_dev[tmp_slot_num].mem_base0_flag  = res_flag;

    if(res_start){
       tamc200_dev[tmp_slot_num].memmory_base0 = pci_iomap(dev, 0, (res_end - res_start));
       printk(KERN_INFO "TAMC200_PROBE: mem_region 0 address %X remaped address %X\n", 
                       res_start, tamc200_dev[tmp_slot_num].memmory_base0);
        tamc200_dev[tmp_slot_num].rw_off0 = (res_end - res_start);
        tamc200_dev[tmp_slot_num].tamc200_all_mems++;
    }
    else{
      tamc200_dev[tmp_slot_num].memmory_base0 = 0;
      tamc200_dev[tmp_slot_num].rw_off0 = 0;
      printk(KERN_INFO "TAMC200_PROBE: NO BASE0 address\n");
    }
    
    res_start = pci_resource_start(dev, 2);
    res_end   = pci_resource_end(dev, 2);
    res_flag  = pci_resource_flags(dev, 2);
    tamc200_dev[tmp_slot_num].mem_base2       = res_start;
    tamc200_dev[tmp_slot_num].mem_base2_end   = res_end;
    tamc200_dev[tmp_slot_num].mem_base2_flag  = res_flag;

    if(res_start){
       tamc200_dev[tmp_slot_num].memmory_base2 = pci_iomap(dev, 2, (res_end - res_start));
       printk(KERN_INFO "TAMC200_PROBE: mem_region 2 address %X remaped address %X\n", 
                       res_start, tamc200_dev[tmp_slot_num].memmory_base2);
        tamc200_dev[tmp_slot_num].rw_off2 = (res_end - res_start);
        tamc200_dev[tmp_slot_num].tamc200_all_mems++;
    }
    else{
      tamc200_dev[tmp_slot_num].memmory_base2 = 0;
      tamc200_dev[tmp_slot_num].rw_off2 = 0;
      printk(KERN_INFO "TAMC200_PROBE: NO BASE2 address\n");
    }
    
    res_start = pci_resource_start(dev, 3);
    res_end   = pci_resource_end(dev, 3);
    res_flag  = pci_resource_flags(dev, 3);
    tamc200_dev[tmp_slot_num].mem_base3       = res_start;
    tamc200_dev[tmp_slot_num].mem_base3_end   = res_end;
    tamc200_dev[tmp_slot_num].mem_base3_flag  = res_flag;

    if(res_start){
       tamc200_dev[tmp_slot_num].memmory_base3 = pci_iomap(dev, 3, (res_end - res_start));
       printk(KERN_INFO "TAMC200_PROBE: mem_region 3 address %X remaped address %X\n", 
                       res_start, tamc200_dev[tmp_slot_num].memmory_base3);
        tamc200_dev[tmp_slot_num].rw_off3 = (res_end - res_start);
        tamc200_dev[tmp_slot_num].tamc200_all_mems++;
    }
    else{
      tamc200_dev[tmp_slot_num].memmory_base3 = 0;
      tamc200_dev[tmp_slot_num].rw_off3 = 0;
      printk(KERN_INFO "TAMC200_PROBE: NO BASE3 address\n");
    }
    res_start = pci_resource_start(dev, 4);
    res_end   = pci_resource_end(dev, 4);
    res_flag  = pci_resource_flags(dev, 4);
    tamc200_dev[tmp_slot_num].mem_base4       = res_start;
    tamc200_dev[tmp_slot_num].mem_base4_end   = res_end;
    tamc200_dev[tmp_slot_num].mem_base4_flag  = res_flag;

    if(res_start){
       tamc200_dev[tmp_slot_num].memmory_base4 = pci_iomap(dev, 4, (res_end - res_start));
       printk(KERN_INFO "TAMC200_PROBE: mem_region 4 address %X remaped address %X\n", 
                       res_start, tamc200_dev[tmp_slot_num].memmory_base4);
        tamc200_dev[tmp_slot_num].rw_off4 = (res_end - res_start);
        tamc200_dev[tmp_slot_num].tamc200_all_mems++;
    }
    else{
      tamc200_dev[tmp_slot_num].memmory_base4 = 0;
      tamc200_dev[tmp_slot_num].rw_off4 = 0;
      printk(KERN_INFO "TAMC200_PROBE: NO BASE4 address\n");
    }
    
    res_start = pci_resource_start(dev, 5);
    res_end   = pci_resource_end(dev, 5);
    res_flag  = pci_resource_flags(dev, 5);
    tamc200_dev[tmp_slot_num].mem_base5       = res_start;
    tamc200_dev[tmp_slot_num].mem_base5_end   = res_end;
    tamc200_dev[tmp_slot_num].mem_base5_flag  = res_flag;

    if(res_start){
       tamc200_dev[tmp_slot_num].memmory_base5 = pci_iomap(dev, 5, (res_end - res_start));
       printk(KERN_INFO "TAMC200_PROBE: mem_region 5 address %X remaped address %X\n", 
                       res_start, tamc200_dev[tmp_slot_num].memmory_base5);
        tamc200_dev[tmp_slot_num].rw_off5 = (res_end - res_start);
        tamc200_dev[tmp_slot_num].tamc200_all_mems++;
    }
    else{
      tamc200_dev[tmp_slot_num].memmory_base5 = 0;
      tamc200_dev[tmp_slot_num].rw_off5 = 0;
      printk(KERN_INFO "TAMC200_PROBE: NO BASE5 address\n");
    }
    if(!tamc200_dev[tmp_slot_num].tamc200_all_mems){
        printk(KERN_ALERT "TAMC200_PROBE: ERROR NO BASE_MEMs\n");
    }
    
     /*iptimer_slot_dev[tmp_slot_num].irq_flag    = IRQF_SHARED;*/
     tamc200_dev[tmp_slot_num].irq_flag    = IRQF_SHARED | IRQF_DISABLED;
     tamc200_dev[tmp_slot_num].pci_dev_irq = dev->irq;
     tamc200_dev[tmp_slot_num].irq_mode    = 1;
     tamc200_dev[tmp_slot_num].irq_on      = 1;
     result = request_irq(tamc200_dev[tmp_slot_num].pci_dev_irq, tamc200_interrupt, 
                         tamc200_dev[tmp_slot_num].irq_flag, "tamc200", &tamc200_dev[tmp_slot_num]);
     if (result) {
         printk(KERN_ALERT "TAMC200_PROBE: can't get assigned irq %i\n", tamc200_dev[tmp_slot_num].irq_line);
         tamc200_dev[tmp_slot_num].irq_mode = 0;
     }else{
         printk(KERN_ALERT "TAMC200_PROBE:  assigned IRQ %i RESULT %i\n",
               tamc200_dev[tmp_slot_num].pci_dev_irq, result);
     }
     
     if(tamc200_dev[tmp_slot_num].dev_sts){
         tamc200_dev[tmp_slot_num].dev_sts   = 0;
         device_destroy(tamc200_class,  MKDEV(TAMC200_MAJOR, tamc200_dev[tmp_slot_num].dev_minor));
     }
     tamc200_dev[tmp_slot_num].dev_sts   = 1;
     device_create(tamc200_class, NULL, MKDEV(TAMC200_MAJOR, tamc200_dev[tmp_slot_num].dev_minor),
                  &tamc200_dev[tmp_slot_num].tamc200_pci_dev->dev, "tamc200s%d", tmp_slot_num);
     
     /*PLX8311 memory initialisation*/ 
     //address = (void *) tamc200_dev[tmp_slot_num].memmory_local;
     //iowrite16(0x0120, (address + 0xA));
     //smp_wmb();
     //iowrite16(0x0980, (address + 0x68));
     //smp_wmb();
 
    for(k = 0; k < TAMC200_NR_SLOTS; k++){
        tmp_slot_cntrl   = 0;
        for (i = 0; i < IPTIMER_MAX_SERVER; i++) {
            tamc200_dev[tmp_slot_num].ip_s[k].server_signal_stack [i].f_ServerPref	= 0;
            tamc200_dev[tmp_slot_num].ip_s[k].server_signal_stack [i].f_IrqProc[0]	= 0;
            tamc200_dev[tmp_slot_num].ip_s[k].server_signal_stack [i].f_IrqProc[1]	= 0;
            tamc200_dev[tmp_slot_num].ip_s[k].server_signal_stack [i].f_IrqProc[2]	= 0;
            tamc200_dev[tmp_slot_num].ip_s[k].server_signal_stack [i].f_IrqProc[3]	= 0;
            tamc200_dev[tmp_slot_num].ip_s[k].server_signal_stack [i].f_IrqProc[4]	= 0;
        }
        tamc200_dev[tmp_slot_num].ip_s[k].irq_status     = 0;
        tamc200_dev[tmp_slot_num].ip_s[k].irq_mask       = 0;
        tamc200_dev[tmp_slot_num].ip_s[k].irq_flag       = 0;
        tamc200_dev[tmp_slot_num].ip_s[k].irq_sec        = 0;
        tamc200_dev[tmp_slot_num].ip_s[k].irq_usec       = 0;
        tamc200_dev[tmp_slot_num].ip_s[k].irq_status_hi  = 0;
        tamc200_dev[tmp_slot_num].ip_s[k].irq_flag_hi    = 0;
        tamc200_dev[tmp_slot_num].ip_s[k].irq_mask_hi    = 0;
        tamc200_dev[tmp_slot_num].ip_s[k].server_signals = IPTIMER_MAX_SERVER;
        tamc200_dev[tmp_slot_num].ip_s[k].event_mask     = IPTIMER_IRQ_CH3;
        
        if(tamc200_dev[tmp_slot_num].ip_s[k].ip_on){
            printk(KERN_ALERT "TAMC200_PROBE:  SLOT %i ENABLED\n",k);
            tamc200_dev[tmp_slot_num].ip_s[k].ip_io  = (tamc200_dev[tmp_slot_num].memmory_base3 + 0x100*k);
            tamc200_dev[tmp_slot_num].ip_s[k].ip_id   = (tamc200_dev[tmp_slot_num].memmory_base3 + 0x100*k + 80);
            tamc200_dev[tmp_slot_num].ip_s[k].ip_mem  = (tamc200_dev[tmp_slot_num].memmory_base4 + 0x800000*k);
            tamc200_dev[tmp_slot_num].ip_s[k].ip_mem8 = (tamc200_dev[tmp_slot_num].memmory_base5 + 0x400000*k);
            
            base_addres= (void *) tamc200_dev[tmp_slot_num].ip_s[k].ip_io;
            tmp_data_16 = ioread16(base_addres + 0x80);
            smp_rmb();
            tmp_module_id = tmp_data_16 <<24;
            tmp_data_16 = ioread16(base_addres + 0x82);
            smp_rmb();
            tmp_module_id +=( tmp_data_16 <<16);
            tmp_data_16  = ioread16(base_addres + 0x84);
            smp_rmb();
            tmp_module_id += tmp_data_16 <<8;
            tmp_data_16 = ioread16(base_addres + 0x86);
            smp_rmb();
            tmp_module_id  += tmp_data_16 ;
            printk(KERN_ALERT "TAMC200_PROBE: MODULE ID %X\n", tmp_module_id);
            tamc200_dev[tmp_slot_num].ip_s[k].ip_module_id = tmp_module_id;
            if(tamc200_dev[tmp_slot_num].ip_s[k].ip_module_type == IPTIMER){
                iowrite16(0, (base_addres + 0x2A));
                smp_wmb();
                iowrite16(0xFFFF, (base_addres + 0x2A));
                smp_wmb();
                iowrite16(0xFFFF, (base_addres + 0x20));
                smp_wmb();
                iowrite16(0xFFFF, (base_addres + 0x22));
                smp_wmb();
                iowrite16(0xFFFF, (base_addres + 0x24));
                smp_wmb();
                iowrite16(0xFFFF, (base_addres + 0x26));
                smp_wmb();
                iowrite16(0x0,    (base_addres + 0x28));
                smp_wmb();
                iowrite16(0xFFFF, (base_addres + 0x2C));
                smp_wmb();
            }
            if(tamc200_dev[tmp_slot_num].ip_s[k].irq_on){
                printk(KERN_ALERT "TAMC200_PROBE:  IRQ FOR SLOT %i ENABLED\n",k);
                if(tamc200_dev[tmp_slot_num].ip_s[k].irq_lev){
                //    tmp_slot_cntrl   |= 0x0080;
                //}else{
                //    tmp_slot_cntrl   |= 0x0040;
                //}
                    tmp_slot_cntrl   |= (tamc200_dev[tmp_slot_num].ip_s[k].irq_lev & 0x3)<< 6;
                }
                if(tamc200_dev[tmp_slot_num].ip_s[k].irq_sens){
                    tmp_slot_cntrl   |= 0x0020;
                }else{
                    tmp_slot_cntrl   |= 0x0010;
                }
                printk(KERN_ALERT "TAMC200_PROBE:  SLOT %i CNTRL %X\n", k, tmp_slot_cntrl);
                iowrite16(tmp_slot_cntrl,((void*)(tamc200_dev[tmp_slot_num].memmory_base2) + 0x2*(k + 1)));
                smp_wmb();
                iowrite16(tamc200_dev[tmp_slot_num].ip_s[k].irq_vec,(base_addres + 0x2E));
                smp_wmb();
                tamc200_dev[tmp_slot_num].ip_s[k].reg_map.wr16 [23] = tamc200_dev[tmp_slot_num].ip_s[k].irq_vec;
            }
            if(tamc200_dev[tmp_slot_num].ip_s[k].dev_sts){
                tamc200_dev[tmp_slot_num].ip_s[k].dev_sts   = 0;
                device_destroy(tamc200_class,  MKDEV(TAMC200_MAJOR, tamc200_dev[tmp_slot_num].ip_s[k].dev_minor));
            }
            tamc200_dev[tmp_slot_num].ip_s[k].dev_sts   = 1;
            switch(k){
                case 0:
                    device_create(tamc200_class, NULL, MKDEV(TAMC200_MAJOR, tamc200_dev[tmp_slot_num].ip_s[k].dev_minor),
                                &tamc200_dev[tmp_slot_num].tamc200_pci_dev->dev, "tamc200s%d%s", tmp_slot_num, "a");
                    break;
                case 1:
                    device_create(tamc200_class, NULL, MKDEV(TAMC200_MAJOR, tamc200_dev[tmp_slot_num].ip_s[k].dev_minor),
                                &tamc200_dev[tmp_slot_num].tamc200_pci_dev->dev, "tamc200s%d%s", tmp_slot_num, "b");
                    break;
                case 2:
                    device_create(tamc200_class, NULL, MKDEV(TAMC200_MAJOR, tamc200_dev[tmp_slot_num].ip_s[k].dev_minor),
                                &tamc200_dev[tmp_slot_num].tamc200_pci_dev->dev, "tamc200s%d%s", tmp_slot_num, "c");
                    break;
            }
        }
    }
    
    register_tamc200_proc(tmp_slot_num, &tamc200_dev[tmp_slot_num]);
    tamc200ModuleNum ++;
    return 0;
}

static void __devexit tamc200_remove(struct pci_dev *dev)
{
    int                    k = 0;
    struct tamc200_dev     *tamc200_dev;
    int                    tmp_dev_num  = 0;
    int                    tmp_slot_num = 0;
    unsigned long          flags;
    void                   *base_addres;
    printk(KERN_ALERT "TAMC200_REMOVE:REMOVE CALLED\n");
     
    tamc200_dev = dev_get_drvdata(&(dev->dev));
    tmp_dev_num  = tamc200_dev->dev_num;
    tmp_slot_num = tamc200_dev->slot_num;
    printk(KERN_ALERT "TAMC200_REMOVE: SLOT %d DEV %d\n", tmp_slot_num, tmp_dev_num);
    /*DISABLING INTERRUPTS ON THE MODULE*/
    spin_lock_irqsave(&tamc200_dev->irq_lock, flags);
    printk(KERN_ALERT "TAMC200_REMOVE: DISABLING INTERRUPTS ON THE MODULE\n");
    iowrite16(0x0000,((void *)tamc200_dev->memmory_base2 + 0x2));
    iowrite16(0x0000,((void *)tamc200_dev->memmory_base2 + 0x4));
    iowrite16(0x0000,((void *)tamc200_dev->memmory_base2 + 0x6));
    printk(KERN_ALERT "TAMC200_REMOVE: END DISABLING INTERRUPTS ON THE MODULE\n");
    spin_unlock_irqrestore(&tamc200_dev->irq_lock, flags);
    
    printk(KERN_ALERT "TAMC200_REMOVE: flush_workqueue\n");
    printk(KERN_ALERT "TAMC200_REMOVE:REMOVING IRQ_MODE %d\n", tamc200_dev->irq_mode);
    if(tamc200_dev->irq_mode){
       printk(KERN_ALERT "TAMC200_REMOVE:FREE IRQ\n");
       free_irq(tamc200_dev->pci_dev_irq, tamc200_dev);
    }
    for(k = 0; k < TAMC200_NR_SLOTS; k++){
        if(tamc200_dev->ip_s[k].ip_on){
            base_addres= (void *) tamc200_dev->ip_s[k].ip_io;
            mutex_lock_interruptible(&(tamc200_dev->ip_s[k].dev_mut));
                tamc200_dev->ip_s[k].dev_sts   = 0;
                if(tamc200_dev->ip_s[k].ip_module_type == IPTIMER){
                    iowrite16(0, (base_addres + 0x2A));
                    smp_wmb();
                    iowrite16(0xFFFF, (base_addres + 0x2A));
                    smp_wmb();
                    iowrite16(0xFFFF, (base_addres + 0x20));
                    smp_wmb();
                    iowrite16(0xFFFF, (base_addres + 0x22));
                    smp_wmb();
                    iowrite16(0xFFFF, (base_addres + 0x24));
                    smp_wmb();
                    iowrite16(0xFFFF, (base_addres + 0x26));
                    smp_wmb();
                    iowrite16(0x0,    (base_addres + 0x28));
                    smp_wmb();
                    iowrite16(0xFFFF, (base_addres + 0x2C));
                    smp_wmb();
                }
                tamc200_dev->ip_s[k].ip_on   = 0;
                tamc200_dev->ip_s[k].ip_id   = 0;
                tamc200_dev->ip_s[k].ip_io   = 0;
                tamc200_dev->ip_s[k].ip_mem  = 0;
                tamc200_dev->ip_s[k].ip_mem8 = 0;
            mutex_unlock(&(tamc200_dev->ip_s[k].dev_mut));
            device_destroy(tamc200_class,  MKDEV(TAMC200_MAJOR, tamc200_dev->ip_s[k].dev_minor));
        }
    }
    mutex_lock_interruptible(&(tamc200_dev->dev_mut));
    
        printk(KERN_ALERT "TAMC200_REMOVE: UNMAPPING MEMORY 0\n");
        if(tamc200_dev->memmory_base0){
           pci_iounmap(dev, tamc200_dev->memmory_base0);
        }
        printk(KERN_ALERT "TAMC200_REMOVE: UNMAPPING MEMORY 2\n");
        if(tamc200_dev->memmory_base2){
           pci_iounmap(dev, tamc200_dev->memmory_base2);
        }
        printk(KERN_ALERT "TAMC200_REMOVE: UNMAPPING MEMORY 3\n");
        if(tamc200_dev->memmory_base3){
           pci_iounmap(dev, tamc200_dev->memmory_base3);
        }
        printk(KERN_ALERT "TAMC200_REMOVE: UNMAPPING MEMORY 4\n");
        if(tamc200_dev->memmory_base4){
           pci_iounmap(dev, tamc200_dev->memmory_base4);
        }
        printk(KERN_ALERT "TAMC200_REMOVE: UNMAPPING MEMORY 5\n");
        if(tamc200_dev->memmory_base5){
           pci_iounmap(dev, tamc200_dev->memmory_base5);
        }
        printk(KERN_ALERT "TAMC200_REMOVE: MEMORYs UNMAPPED\n");
        pci_release_regions((tamc200_dev->tamc200_pci_dev));
        
        tamc200_dev->memmory_base0 = 0;
        tamc200_dev->memmory_base2 = 0;
        tamc200_dev->memmory_base3 = 0;
        tamc200_dev->memmory_base4 = 0;
        tamc200_dev->memmory_base5 = 0;
    
    mutex_unlock(&(tamc200_dev->dev_mut));
    device_destroy(tamc200_class,  MKDEV(TAMC200_MAJOR, tamc200_dev->dev_minor));
    unregister_tamc200_proc(tmp_slot_num);
    tamc200_dev->dev_sts   = 0;
    tamc200ModuleNum --;
    pci_disable_device(dev);
}

static struct pci_driver pci_tamc200_driver = {
	.name     = "tamc200",
	.id_table = tamc200_table,
	.probe    = tamc200_probe,
	.remove   = __devexit_p(tamc200_remove),
};

#if LINUX_VERSION_CODE < 132632 
    static void tamc200_do_work(void *tamc200dev)
#else
    static void tamc200_do_work(struct work_struct *work_str)
#endif 
{
    //printk(KERN_ALERT "TAMC200 DO WORK: \n");
    unsigned long flags;
    u16 tmp_mask     = 0;
    u16 tmp_flag     = 0;
    u16 tmp_event    = 0;
    u32 tmp_sec;
    u32 tmp_usec;
    u32 status       = 0;
    u16 gen_mask     = 0;
    int i            = 0;
    u16 tmp_irq_sl   = 0;
    t_ServerSignal   *ss;
    u16 minor;
    void *address;
    
    struct tamc200_slot_dev *sdev;
    #if LINUX_VERSION_CODE < 132632 
    struct tamc200_dev *dev   = tamc200dev;
    #else
    struct tamc200_dev *dev   =  container_of(work_str, struct tamc200_dev, tamc200_work);
    #endif 
        
    if(!(dev->dev_sts)) return;
      
    tmp_irq_sl   = dev->irq_slot;
    address = (void *) (dev->ip_s[dev->irq_slot].ip_io);
    sdev = &(dev->ip_s[dev->irq_slot]);
    spin_lock_irqsave(&dev->irq_lock, flags);
       tmp_mask = dev->irq_mask_hi;
       tmp_flag = dev->irq_flag_hi;
       tmp_sec  = dev->irq_sec_hi;
       tmp_usec = dev->irq_usec_hi;
    spin_unlock_irqrestore(&dev->irq_lock, flags);

    minor = sdev->dev_minor;			   
    if (mutex_lock_interruptible(&sdev->dev_mut))
                    return ;
					   
	sdev->irq_flag   = tmp_flag;
	sdev->irq_status = (tmp_flag & tmp_mask);
	status          = (tmp_flag & tmp_mask);
	sdev->irq_sec    = tmp_sec;
	sdev->irq_usec   = tmp_usec;
        tmp_event = ioread16((address +0x2C));
	smp_rmb();
	sdev->event_value = tmp_event;
	if(status & sdev->event_mask){
           sdev->gen_event_low = ioread16(address + 0x40);
	   smp_rmb();
	   sdev->gen_event_hi = ioread16(address + 0x42);
	   smp_rmb();
	   iowrite16(0xFFFF,(address +  0x2C));
	   smp_wmb();
	}	
  	for (i = 0; i < sdev->server_signals; i++) {
            ss = &sdev->server_signal_stack [i];
            if (ss->f_ServerPref == 0) continue;
            if (status & ss->f_IrqProc[0]) {
               if (kill_proc(ss->f_ServerPref, SIGUSR1, 0)) {
                   ss->f_IrqProc[0]    = 0;
                   ss->f_IrqProc[1]    = 0;
                   ss->f_IrqProc[2]    = 0;
                   ss->f_ServerPref    = 0;
               }
            }
            if (status & ss->f_IrqProc[1]) {
              if (kill_proc (ss->f_ServerPref, SIGUSR2, 0)) {
                  ss->f_IrqProc[0]    = 0;
                  ss->f_IrqProc[1]    = 0;
                  ss->f_IrqProc[2]    = 0;
                  ss->f_ServerPref    = 0;
              }
            }
            if (status & ss->f_IrqProc[2]) {
               if (kill_proc (ss->f_ServerPref, SIGURG, 0)) {
                   ss->f_IrqProc[0]    = 0;
                   ss->f_IrqProc[1]    = 0;
                   ss->f_IrqProc[2]    = 0;
                   ss->f_ServerPref    = 0;
               }
            }
            if (status & ss->f_IrqProc[3]) {
                  ss->f_IrqFlag |= tmp_flag;
            }
           if (status & ss->f_IrqProc[4]) {
               if (kill_proc (ss->f_ServerPref, ss->f_CurSig, 0)) {
                   ss->f_IrqProc[0]    = 0;
                   ss->f_IrqProc[1]    = 0;
                   ss->f_IrqProc[2]    = 0;
                   ss->f_ServerPref    = 0;
               }
          }
         gen_mask |= sdev->server_signal_stack [i].f_IrqProc[0];
         gen_mask |= sdev->server_signal_stack [i].f_IrqProc[1];
         gen_mask |= sdev->server_signal_stack [i].f_IrqProc[2];
         gen_mask |= sdev->server_signal_stack [i].f_IrqProc[3];
         gen_mask |= sdev->server_signal_stack [i].f_IrqProc[4];
	}
	sdev->reg_map.wr16 [20]  = gen_mask;
	sdev->irq_mask 	        = gen_mask;
	iowrite16(gen_mask,(address +  0x28));
	smp_wmb();
	iowrite16(0x0,(address +  0x2A));
	smp_wmb();
	iowrite16(0xFFFF,(address +  0x2A));
	smp_wmb();
	
    mutex_unlock(&sdev->dev_mut);

}

/*
 * The top-half interrupt handler.
 */
#if LINUX_VERSION_CODE < 0x20613 // irq_handler_t has changed in 2.6.19
static irqreturn_t tamc200_interrupt(int irq, void *dev_id, struct pt_regs *regs)
#else
static irqreturn_t tamc200_interrupt(int irq, void *dev_id)
#endif
{
    struct timeval     tv;
    u16                tmp_data_16  = 0;
    u16                tmp_vect     = 0;
    int                tmp_dev_num  = 33;
    u16                tmp_mask     = 0;
    u16                tmp_flag     = 0;
    u16                tmp_irq_flag = 0;
    u16                tmp_irq_sl   = 0;
    
    void               *address = 0;
	
    struct tamc200_dev * dev = dev_id;
    tmp_dev_num = dev->dev_num;
    if(dev->vendor_id  != TAMC200_VENDOR_ID)   return IRQ_NONE;
    if(dev->device_id  != TAMC200_DEVICE_ID)   return IRQ_NONE;

    do_gettimeofday(&tv);
    
    //printk(KERN_ALERT "TAMC200 INTERRUPT\n");
    
    tmp_data_16 = ioread16(((void *)dev->memmory_base2+0xC));
    smp_rmb();
    tmp_irq_flag = tmp_data_16 & 0xFF;
    if(!tmp_irq_flag) return IRQ_NONE;
    tmp_irq_sl   = tmp_irq_flag & 3;
    if(tmp_irq_sl){
        dev->irq_slot = 0;
    }else{
        tmp_irq_sl   = (tmp_irq_flag >> 2) & 3;
        if(tmp_irq_sl){
            dev->irq_slot = 1;
        }else{
            tmp_irq_sl   = (tmp_irq_flag >> 4) & 3;
            if(tmp_irq_sl){
                dev->irq_slot = 2;
            }
        }
    }
    address = ((void *)dev->memmory_base3 +0x100*dev->irq_slot);
    iowrite16(0xFFFF,  ((void *)dev->memmory_base2 +0xC));
    //printk(KERN_ALERT "TAMC200 INTERRUPT: SLOT %i ADDRESS %X \n", dev->irq_slot, address);
    smp_wmb();
    if(dev->ip_s[dev->irq_slot].ip_module_type == IPTIMER){
        //printk(KERN_ALERT "TAMC200 INTERRUPT: SLOT %i is IPTIMER \n", dev->irq_slot);
        tmp_mask = ioread16(address + 0x28);
        smp_rmb();
        tmp_flag = ioread16(address + 0x2A);
        smp_rmb();
        iowrite16(0,address + 0x2A);
        smp_wmb();
        iowrite16(0xFFFF,address + 0x2A);
        smp_wmb();
    }
    spin_lock(&dev->irq_lock);
        dev->irq_mask_hi   = tmp_mask;
        dev->irq_flag_hi   = tmp_flag;
        dev->irq_sec_hi    = tv.tv_sec;
        dev->irq_usec_hi   = tv.tv_usec;
        dev->irq_status_hi = tmp_data_16;
        dev->irq_vect_hi   = tmp_vect;
    spin_unlock(&dev->irq_lock);
    queue_work(tamc200_workqueue, &(dev->tamc200_work));
    return IRQ_HANDLED;
}

static int tamc200_open( struct inode *inode, struct file *filp )
{
    int    minor;
    struct tamc200_dev *dev; /* device information */
	
    minor = iminor(inode);
    dev = container_of(inode->i_cdev, struct tamc200_dev, cdev);
    dev->dev_minor       = minor;
    filp->private_data   = dev; /* for other methods */

    printk(KERN_ALERT "Open Procces is \"%s\" (pid %i) DEV is %d \n", current->comm, current->group_leader->pid, minor);

    return 0;
}

static int tamc200s_open( struct inode *inode, struct file *filp )
{
    int    minor;
    struct tamc200_slot_dev *dev; /* device information */
	
    minor = iminor(inode);
    dev = container_of(inode->i_cdev, struct tamc200_slot_dev, cdev);
    dev->dev_minor       = minor;
    filp->private_data   = dev; /* for other methods */

    printk(KERN_ALERT "Open Procces is \"%s\" (pid %i) DEV is %d \n", current->comm, current->group_leader->pid, minor);

    return 0;
}

static int tamc200_release(struct inode *inode, struct file *filp)
{
    int minor     = 0;
    int d_num     = 0;
    u16 cur_proc  = 0;

    struct tamc200_dev      *dev;
    
    dev = filp->private_data;

    minor = dev->dev_minor;
    d_num = dev->dev_num;
    //cur_proc = current->pid;
    cur_proc = current->group_leader->pid;
    if (mutex_lock_interruptible(&dev->dev_mut))
                    return -ERESTARTSYS;
    printk(KERN_ALERT "TAMC200_SLOT RELEAASE: Close Procces is \"%s\" (pid %i)\n", current->comm, current->pid);
    printk(KERN_ALERT "TAMC200_SLOT RELEAASE: Close MINOR %d DEV_NUM %d \n", minor, d_num);
    mutex_unlock(&dev->dev_mut);
    return 0;
} 

static int tamc200s_release(struct inode *inode, struct file *filp)
{
    int minor     = 0;
    int d_num     = 0;
    u16 cur_proc  = 0;
    u16 gen_mask  = 0;
    int i         = 0;
    void *address = 0;

    struct tamc200_slot_dev      *dev;
    
    dev = filp->private_data;

    minor = dev->dev_minor;
    d_num = dev->dev_num;
    address = (void *) dev->ip_io ;
    //cur_proc = current->pid;
    cur_proc = current->group_leader->pid;
    if (mutex_lock_interruptible(&dev->dev_mut))
                    return -ERESTARTSYS;

    if(dev->ip_module_type == IPTIMER){
        for (i = 0; i < dev->server_signals; i++) {	
            if (dev->server_signal_stack [i].f_ServerPref == cur_proc) {
                    dev->server_signal_stack [i].f_IrqProc[0] = 0;
                    dev->server_signal_stack [i].f_IrqProc[1] = 0;
                    dev->server_signal_stack [i].f_IrqProc[2] = 0;
                    dev->server_signal_stack [i].f_IrqProc[3] = 0;
                    dev->server_signal_stack [i].f_IrqProc[4] = 0;
                    dev->server_signal_stack [i].f_ServerPref = 0;
            }
            gen_mask |= dev->server_signal_stack [i].f_IrqProc[0];
            gen_mask |= dev->server_signal_stack [i].f_IrqProc[1];
            gen_mask |= dev->server_signal_stack [i].f_IrqProc[2];
            gen_mask |= dev->server_signal_stack [i].f_IrqProc[3];
            gen_mask |= dev->server_signal_stack [i].f_IrqProc[4];
        }
        dev->reg_map.wr16 [20]  = gen_mask;
        dev->irq_mask 	    = gen_mask;
        if(dev->ip_io){
            printk("TAMC200_SLOT RELEAASE: SET GEN_MASK %X\n", dev->ip_io);
            iowrite16(gen_mask,(address +  0x28));
            smp_wmb();
        }
    }
    printk(KERN_ALERT "TAMC200_SLOT RELEAASE: Close Procces is \"%s\" (pid %i)\n", current->comm, current->pid);
    printk(KERN_ALERT "TAMC200_SLOT RELEAASE: Close MINOR %d DEV_NUM %d \n", minor, d_num);
    mutex_unlock(&dev->dev_mut);
    return 0;
} 

static ssize_t tamc200_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    u64        itemsize       = 0; 
    ssize_t    retval         = 0;
    int        minor          = 0;
    int        d_num          = 0;
    int        tmp_offset     = 0;
    int        tmp_mode       = 0;
    int        tmp_barx       = 0;
    int        tmp_rsrvd_rw   = 0;
    int        tmp_size_rw    = 0;
    u8         tmp_revision   = 0;
    u32        mem_tmp        = 0;
    int        i              = 0;
    u8         tmp_data_8;
    u16        tmp_data_16;
    device_rw  reading;
    void       *address       = 0;
   
    struct tamc200_dev  *dev = filp->private_data;
    minor = dev->dev_minor;
    d_num = dev->dev_num;
    
    //printk(KERN_ALERT "TAMC200_READ 0\n");
    
    if(!dev->dev_sts){
        printk(KERN_ALERT "TAMC200_READ: NO DEVICE %d\n", dev->dev_num);
        retval = -EFAULT;
        return retval;
    }
    
   // printk(KERN_ALERT "TAMC200_READ 1\n");
    
    itemsize = sizeof(device_rw);
    if (mutex_lock_interruptible(&dev->dev_mut)){
        return -ERESTARTSYS;
    }
    
    //printk(KERN_ALERT "TAMC200_READ 2\n");
    
        if (copy_from_user(&reading, buf, count)) {
            retval = -EFAULT;
            mutex_unlock(&dev->dev_mut);
            return retval;
        }
    
    //printk(KERN_ALERT "TAMC200_READ 3\n");
    
	tmp_mode     = reading.mode_rw;
        if(tmp_mode == RW_INFO){
            pci_read_config_byte(dev->tamc200_pci_dev, PCI_REVISION_ID, &tmp_revision);
            reading.offset_rw = TAMC200_DRV_VER_MIN;
            reading.data_rw   = TAMC200_DRV_VER_MAJ;
            reading.mode_rw   = tmp_revision;
            reading.barx_rw   = dev->tamc200_all_mems;
            reading.size_rw   = dev->slot_num; /*SLOT NUM*/
            retval            = itemsize;
            if (copy_to_user(buf, &reading, count)) {
                printk(KERN_ALERT "TAMC200_READ 4\n");
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                retval = 0;
                return retval;
            }

            mutex_unlock(&dev->dev_mut);
            return retval;
        }
        tmp_offset   = reading.offset_rw;
        tmp_barx     = reading.barx_rw;
        tmp_rsrvd_rw = reading.rsrvd_rw;
        tmp_size_rw  = reading.size_rw;
        //printk(KERN_ALERT "offset %x bar %x mode %x rsrvd %x size %x\n", tmp_offset, tmp_barx, tmp_mode, tmp_rsrvd_rw, tmp_size_rw);
	switch(tmp_barx){
            case 0:
            if(!dev->memmory_base0){
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            address = (void *) dev->memmory_base0;
            mem_tmp = (dev->mem_base0_end );
            break;
            case 2:
            if(!dev->memmory_base2){
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
                }
            address = (void *)dev->memmory_base2;
            mem_tmp = (dev->mem_base2_end );
            break;
            case 3:
            if(!dev->memmory_base3){
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            address = (void *) dev->memmory_base3;
            mem_tmp = (dev->mem_base3_end );
            break;
            case 4:
            if(!dev->memmory_base4){
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            address = (void *)dev->memmory_base4;
            mem_tmp = (dev->mem_base4_end );
            break;
            case 5:
            if(!dev->memmory_base5){
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
                }
            address = (void *)dev->memmory_base5;
            mem_tmp = (dev->mem_base5_end );
            break;
            default:
            if(!dev->memmory_base0){
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            address = (void *)dev->memmory_base0;
            mem_tmp = (dev->mem_base0_end );
            break;
        }
         if(tmp_size_rw < 2){
            if(tmp_offset > (mem_tmp -2) || (!address)){
                reading.data_rw   = 0;
                retval            = 0;
            }else{
                switch(tmp_mode){
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
                    default:
                        tmp_data_16       = ioread16(address + tmp_offset);
                        rmb();
                        reading.data_rw   = tmp_data_16 & 0xFFFF;
                        retval = itemsize;
                        break;
                }
            }

            if (copy_to_user(buf, &reading, count)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                retval = 0;
                return retval;
            }
        }else{
            switch(tmp_mode){
                case RW_D8:
                    if((tmp_offset + tmp_size_rw) > (mem_tmp -2) || (!address)){
                        reading.data_rw   = 0;
                        retval            = 0;
                    }else{
                        for(i = 0; i< tmp_size_rw; i++){
                            tmp_data_8        = ioread8(address + tmp_offset + i);
                            rmb();
                            reading.data_rw   = tmp_data_8 & 0xFF;
                            retval = itemsize;
                            if (copy_to_user((buf + i), &reading, 1)) {
                                retval = -EFAULT;
                                mutex_unlock(&dev->dev_mut);
                                retval = 0;
                                return retval;
                            }
                        }
                    }
                    break;
                case RW_D16:
                    if((tmp_offset + tmp_size_rw*2) > (mem_tmp -2) || (!address)){
                        reading.data_rw   = 0;
                        retval            = 0;
                    }else{
                        for(i = 0; i< tmp_size_rw; i++){
                            tmp_data_16       = ioread16(address + tmp_offset);
                            rmb();
                            reading.data_rw   = tmp_data_16 & 0xFFFF;
                            retval = itemsize;
                            if (copy_to_user((buf + i*2), &reading, 2)) {
                                retval = -EFAULT;
                                mutex_unlock(&dev->dev_mut);
                                retval = 0;
                                return retval;
                            }
                        }
                    }
                    break;
                default:
                    if((tmp_offset + tmp_size_rw*2) > (mem_tmp -2) || (!address)){
                        reading.data_rw   = 0;
                        retval            = 0;
                    }else{
                        for(i = 0; i< tmp_size_rw; i++){
                            tmp_data_16       = ioread16(address + tmp_offset);
                            rmb();
                            reading.data_rw   = tmp_data_16 & 0xFFFF;
                            retval = itemsize;
                            if (copy_to_user((buf + i*2), &reading, 2)) {
                                retval = -EFAULT;
                                mutex_unlock(&dev->dev_mut);
                                retval = 0;
                                return retval;
                            }
                        }
                    }
                    break;
            }
        } 
    mutex_unlock(&dev->dev_mut);
    return retval;
}

static ssize_t tamc200s_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    u64        itemsize       = 0; 
    ssize_t    retval         = 0;
    int        minor          = 0;
    int        d_num          = 0;
    int        tmp_offset     = 0;
    int        tmp_mode       = 0;
    int        tmp_barx       = 0;
    int        tmp_rsrvd_rw   = 0;
    int        tmp_size_rw    = 0;
    u8         tmp_revision   = 0;
    u32        mem_tmp        = 0;
    int        i              = 0;
    u8         tmp_data_8;
    u16        tmp_data_16;
    device_rw  reading;
    void       *address       = 0;
   
    struct tamc200_slot_dev  *dev = filp->private_data;
    minor = dev->dev_minor;
    d_num = dev->dev_num;
    
    //printk(KERN_ALERT "TAMC200_READ 0\n");
    
    if(!dev->dev_sts){
        printk(KERN_ALERT "TAMC200_READ: NO DEVICE %d\n", dev->dev_num);
        retval = -EFAULT;
        return retval;
    }
    
    //printk(KERN_ALERT "TAMC200_READ 1\n");
    
    itemsize = sizeof(device_rw);
    if (mutex_lock_interruptible(&dev->dev_mut)){
        return -ERESTARTSYS;
    }
    
    //printk(KERN_ALERT "TAMC200_READ 2\n");
    
        if (copy_from_user(&reading, buf, count)) {
            retval = -EFAULT;
            mutex_unlock(&dev->dev_mut);
            return retval;
        }
    
        //printk(KERN_ALERT "TAMC200_READ 3\n");
        
	tmp_mode     = reading.mode_rw;
        if(tmp_mode == RW_INFO){
            pci_read_config_byte(dev->ptamc200_dev->tamc200_pci_dev, PCI_REVISION_ID, &tmp_revision);
            reading.offset_rw = TAMC200_DRV_VER_MIN;
            reading.data_rw   = TAMC200_DRV_VER_MAJ;
            reading.mode_rw   = tmp_revision;
            reading.barx_rw   = 0;
            reading.size_rw   = dev->ip_slot_num; /*SLOT NUM*/
            retval            = itemsize;
            if (copy_to_user(buf, &reading, count)) {
                printk(KERN_ALERT "TAMC200_READ 43\n");
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                retval = 0;
                return retval;
            }

            mutex_unlock(&dev->dev_mut);
            return retval;
        }
        tmp_offset   = reading.offset_rw;
        tmp_barx     = reading.barx_rw;
        tmp_rsrvd_rw = reading.rsrvd_rw;
        tmp_size_rw  = reading.size_rw;
        //printk(KERN_ALERT "offset %x bar %x mode %x rsrvd %x size %x\n", tmp_offset, tmp_barx, tmp_mode, tmp_rsrvd_rw, tmp_size_rw);
	switch(tmp_barx){
            case 0:
            if(!dev->ip_io){
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            address = (void *) dev->ip_io;
            mem_tmp = 256;
            break;
            case 1:
            if(!dev->ip_mem){
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
                }
            address = (void *)dev->ip_mem;
            mem_tmp = 8388608;
            break;
            case 2:
            if(!dev->ip_mem8){
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            address = (void *) dev->ip_mem8;
            mem_tmp = 4194304;
            break;
            default:
            if(!dev->ip_io){
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            address = (void *)dev->ip_io;
            mem_tmp = 256;
            break;
        }
         if(tmp_size_rw < 2){
            if(tmp_offset > (mem_tmp -2) || (!address)){
                reading.data_rw   = 0;
                retval            = 0;
            }else{
                switch(tmp_mode){
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
                    default:
                        tmp_data_16       = ioread16(address + tmp_offset);
                        rmb();
                        reading.data_rw   = tmp_data_16 & 0xFFFF;
                        retval = itemsize;
                        break;
                }
            }

            if (copy_to_user(buf, &reading, count)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                retval = 0;
                return retval;
            }
        }else{
            switch(tmp_mode){
                case RW_D8:
                    if((tmp_offset + tmp_size_rw) > (mem_tmp -2) || (!address)){
                        reading.data_rw   = 0;
                        retval            = 0;
                    }else{
                        for(i = 0; i< tmp_size_rw; i++){
                            tmp_data_8        = ioread8(address + tmp_offset + i);
                            rmb();
                            reading.data_rw   = tmp_data_8 & 0xFF;
                            retval = itemsize;
                            if (copy_to_user((buf + i), &reading, 1)) {
                                retval = -EFAULT;
                                mutex_unlock(&dev->dev_mut);
                                retval = 0;
                                return retval;
                            }
                        }
                    }
                    break;
                case RW_D16:
                    if((tmp_offset + tmp_size_rw*2) > (mem_tmp -2) || (!address)){
                        reading.data_rw   = 0;
                        retval            = 0;
                    }else{
                        for(i = 0; i< tmp_size_rw; i++){
                            tmp_data_16       = ioread16(address + tmp_offset);
                            rmb();
                            reading.data_rw   = tmp_data_16 & 0xFFFF;
                            retval = itemsize;
                            if (copy_to_user((buf + i*2), &reading, 2)) {
                                retval = -EFAULT;
                                mutex_unlock(&dev->dev_mut);
                                retval = 0;
                                return retval;
                            }
                        }
                    }
                    break;
                default:
                    if((tmp_offset + tmp_size_rw*2) > (mem_tmp -2) || (!address)){
                        reading.data_rw   = 0;
                        retval            = 0;
                    }else{
                        for(i = 0; i< tmp_size_rw; i++){
                            tmp_data_16       = ioread16(address + tmp_offset);
                            rmb();
                            reading.data_rw   = tmp_data_16 & 0xFFFF;
                            retval = itemsize;
                            if (copy_to_user((buf + i*2), &reading, 2)) {
                                retval = -EFAULT;
                                mutex_unlock(&dev->dev_mut);
                                retval = 0;
                                return retval;
                            }
                        }
                    }
                    break;
            }
        } 
    mutex_unlock(&dev->dev_mut);
    return retval;
}

static ssize_t tamc200_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    u64        itemsize       = 0; 
    ssize_t    retval         = 0;
    int        minor          = 0;
    int        d_num          = 0;
    int        tmp_offset     = 0;
    int        tmp_mode       = 0;
    int        tmp_barx       = 0;
    int        tmp_rsrvd_rw   = 0;
    int        tmp_size_rw    = 0;
    u32        mem_tmp        = 0;
    u8         tmp_data_8;
    u16        tmp_data_16;
    u32        tmp_data_32;
    device_rw  reading;
    void       *address       = 0;
   
    struct tamc200_dev  *dev = filp->private_data;
    minor = dev->dev_minor;
    d_num = dev->dev_num;
    
    if(!dev->dev_sts){
        printk("TAMC200_READ: NO DEVICE %d\n", dev->dev_num);
        retval = -EFAULT;
        return retval;
    }
    
    itemsize = sizeof(device_rw);
    if (mutex_lock_interruptible(&dev->dev_mut)){
        return -ERESTARTSYS;
    }
        if (copy_from_user(&reading, buf, count)) {
            retval = -EFAULT;
            mutex_unlock(&dev->dev_mut);
            return retval;
        }
	tmp_mode     = reading.mode_rw;
        tmp_offset   = reading.offset_rw;
        tmp_barx     = reading.barx_rw;
        tmp_data_32  = reading.data_rw & 0xFFFFFFFF;
        tmp_rsrvd_rw = reading.rsrvd_rw;
        tmp_size_rw  = reading.size_rw;
        
         switch(tmp_barx){
            case 0:
                if(!dev->memmory_base0){
                    printk(KERN_ALERT "NO MEM UNDER BAR0 \n");
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                address    = (void *)dev->memmory_base0;
                mem_tmp = (dev->mem_base0_end );
                break;
            case 2:
                if(!dev->memmory_base2){
                    printk(KERN_ALERT "NO MEM UNDER BAR2 \n");
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                address    = (void *)dev->memmory_base2;
                mem_tmp = (dev->mem_base2_end );
                break;
            case 3:
            if(!dev->memmory_base3){
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            address    = (void *) dev->memmory_base3;
            mem_tmp = (dev->mem_base3_end );
            break;
            case 4:
            if(!dev->memmory_base4){
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            address    = (void *)dev->memmory_base4;
            mem_tmp = (dev->mem_base4_end );
            break;
            case 5:
            if(!dev->memmory_base5){
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
                }
            address    = (void *)dev->memmory_base5;
            mem_tmp = (dev->mem_base5_end );
            break;
            default:
                if(!dev->memmory_base0){
                    printk(KERN_ALERT "NO MEM UNDER BAR0 \n");
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                address    = (void *)dev->memmory_base0;
                mem_tmp = (dev->mem_base0_end );
                break;
        }

        if(tmp_offset > (mem_tmp -2) || (!address)){
            reading.data_rw   = 0;
            retval            = 0;
        }else{
            switch(tmp_mode){
                case RW_D8:
                    tmp_data_8 = tmp_data_32 & 0xFF;
                    iowrite8(tmp_data_8, ((void*)(address + tmp_offset)));
                    smp_wmb();
                    retval = itemsize;
                    break;
                case RW_D16:
                    tmp_data_16 = tmp_data_32 & 0xFFFF;
                    iowrite16(tmp_data_16, ((void*)(address + tmp_offset)));
                    smp_wmb();
                    retval = itemsize;
                    break;
                default:
                    tmp_data_16 = tmp_data_32 & 0xFFFF;
                    iowrite16(tmp_data_16, ((void*)(address + tmp_offset)));
                    smp_wmb();
                    retval = itemsize;
                    break;
            }
        }
	
    mutex_unlock(&dev->dev_mut);
    return retval;
}

static ssize_t tamc200s_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    u64        itemsize       = 0; 
    ssize_t    retval         = 0;
    int        minor          = 0;
    int        d_num          = 0;
    int        tmp_offset     = 0;
    int        tmp_mode       = 0;
    int        tmp_barx       = 0;
    int        tmp_rsrvd_rw   = 0;
    int        tmp_size_rw    = 0;
    u32        mem_tmp        = 0;
    u8         tmp_data_8;
    u16        tmp_data_16;
    u32        tmp_data_32;
    device_rw  reading;
    void       *address       = 0;
   
    struct tamc200_slot_dev  *dev = filp->private_data;
    minor = dev->dev_minor;
    d_num = dev->dev_num;
    
    if(!dev->dev_sts){
        printk("TAMC200_READ: NO DEVICE %d\n", dev->dev_num);
        retval = -EFAULT;
        return retval;
    }
    
    itemsize = sizeof(device_rw);
    if (mutex_lock_interruptible(&dev->dev_mut)){
        return -ERESTARTSYS;
    }
        if (copy_from_user(&reading, buf, count)) {
            retval = -EFAULT;
            mutex_unlock(&dev->dev_mut);
            return retval;
        }
	tmp_mode     = reading.mode_rw;
        tmp_offset   = reading.offset_rw;
        tmp_barx     = reading.barx_rw;
        tmp_data_32  = reading.data_rw & 0xFFFFFFFF;
        tmp_rsrvd_rw = reading.rsrvd_rw;
        tmp_size_rw  = reading.size_rw;
        
         switch(tmp_barx){
            case 0:
                if(!dev->ip_io){
                    printk(KERN_ALERT "NO MEM UNDER BAR0 \n");
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                address    = (void *)dev->ip_io;
                mem_tmp = 256;
                break;
            case 1:
                if(!dev->ip_mem){
                    printk(KERN_ALERT "NO MEM UNDER BAR2 \n");
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                address    = (void *)dev->ip_mem;
                mem_tmp = 8388608;
                break;
            case 2:
            if(!dev->ip_mem8){
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            address    = (void *) dev->ip_mem8;
            mem_tmp = 4194304;
            break;
            default:
                if(!dev->ip_io){
                    printk(KERN_ALERT "NO MEM UNDER BAR0 \n");
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                address    = (void *)dev->ip_io;
                mem_tmp = 256;
                break;
        }

        if(tmp_offset > (mem_tmp -2) || (!address)){
            reading.data_rw   = 0;
            retval            = 0;
        }else{
            switch(tmp_mode){
                case RW_D8:
                    tmp_data_8 = tmp_data_32 & 0xFF;
                    iowrite8(tmp_data_8, ((void*)(address + tmp_offset)));
                    smp_wmb();
                    retval = itemsize;
                    break;
                case RW_D16:
                    tmp_data_16 = tmp_data_32 & 0xFFFF;
                    iowrite16(tmp_data_16, ((void*)(address + tmp_offset)));
                    smp_wmb();
                    retval = itemsize;
                    break;
                default:
                    tmp_data_16 = tmp_data_32 & 0xFFFF;
                    iowrite16(tmp_data_16, ((void*)(address + tmp_offset)));
                    smp_wmb();
                    retval = itemsize;
                    break;
            }
        }
	
    mutex_unlock(&dev->dev_mut);
    return retval;
}

//static int  tamc200s_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
static long  tamc200s_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int   err      = 0;
    int   retval   = 0;
    int   minor    = 0;
    int   d_num    = 0;
    u16   tmp_data_16 ;
    int   offset   = 0;
    int   i        = 0;
    u16   new_mask;
    u16   gen_mask = 0;
    pid_t cur_proc = 0;
    u16 tmp_chn    = 0;
    
    u8              tmp_revision = 0;
    u_int           tmp_offset;
    u_int           tmp_data;
    u_int           tmp_cmd;
    u_int           tmp_reserved;
    
    int size;
    int rw_size;
    int reg_size;
    int time_size;
    int event_size;
    
    void *address     = 0;
    struct tamc200_slot_dev      *dev;
    int io_size;
    device_ioctrl_data  io_data;
    
    
    t_iptimer_ioctl_data   	data;
    t_iptimer_ioctl_time   	time_data;
    t_iptimer_ioctl_rw     	rw_data;
    t_iptimer_reg_data   	reg_data;
    t_iptimer_ioctl_event       event_data;
    
    dev = filp->private_data;
    
    //printk("TAMC200S_CTRL: IOCTRL CMD %i\n", cmd);
    
    if(!dev->dev_sts){
        printk("TAMC200S_CTRL:NO DEVICE %d\n", dev->dev_num);
        retval = -EFAULT;
        return retval;
    }
    
    minor = dev->dev_minor;
    d_num = dev->dev_num;
    address = (void *) dev->ip_io ;
    
    io_size    = sizeof(device_ioctrl_data);
    size       = sizeof (t_iptimer_ioctl_data);
    rw_size    = sizeof (t_iptimer_ioctl_rw);
    reg_size   = sizeof (t_iptimer_reg_data);
    time_size  = sizeof (t_iptimer_ioctl_time);
    event_size = sizeof (t_iptimer_ioctl_event);

    //cur_proc = current->pid;
    cur_proc = current->group_leader->pid;
    /*
     * extract the type and number bitfields, and don't decode
     * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
     */
    if (_IOC_TYPE(cmd) != IPTIMER_IOC)    return -ENOTTY;
    if (_IOC_NR(cmd) > PCIEDOOCS_IOC_MAXNR) return -ENOTTY;
    /*
     * the direction is a bitmask, and VERIFY_WRITE catches R/W
     * transfers. `Type' is user-oriented, while
     * access_ok is kernel-oriented, so the concept of "read" and
     * "write" is reversed
     */
    if(cmd != IPTIMER_REFRESH){
       if (_IOC_DIR(cmd) & _IOC_READ)
               err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
       else if (_IOC_DIR(cmd) & _IOC_WRITE)
               err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
       if (err) return -EFAULT;
    }
    //printk("TAMC200S_CTRL: 1 IOCTRL CMD %i\n", cmd);
    if (mutex_lock_interruptible(&dev->dev_mut))
        return -ERESTARTSYS;
	switch (cmd) {
            case PCIEDEV_PHYSICAL_SLOT:
                retval = 0;
                if (copy_from_user(&io_data, (device_ioctrl_data*)arg, (size_t)io_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                tmp_offset   = io_data.offset;
                tmp_data     = io_data.data;
                tmp_cmd      = io_data.cmd;
                tmp_reserved = io_data.reserved;
                io_data.data    = dev->ptamc200_dev->slot_num;
                io_data.cmd     = PCIEDEV_PHYSICAL_SLOT;
                if (copy_to_user((device_ioctrl_data*)arg, &io_data, (size_t)io_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                break;
            case PCIEDEV_DRIVER_VERSION:
                io_data.data   =  TAMC200_DRV_VER_MAJ;
                io_data.offset =  TAMC200_DRV_VER_MIN;
                if (copy_to_user((device_ioctrl_data*)arg, &io_data, (size_t)io_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                break;
            case PCIEDEV_FIRMWARE_VERSION:
                io_data.data   = dev->ptamc200_dev->TAMC200_PPGA_REV;
                io_data.offset = 0;
                if (copy_to_user((device_ioctrl_data*)arg, &io_data, (size_t)io_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                break;
	    case IPTIMER_REFRESH: /*********/
                for (i = 0; i < 24; i++){
                    tmp_data_16 = dev->reg_map.wr16 [i];
                    iowrite16(tmp_data_16, (address + + i*2));
                    smp_wmb();
                }
                break;
	    case IPTIMER_ADD_SERVER_SIGNAL:/* t_iptimer_ioctl_data data******/
                if (copy_from_user(&data, (t_iptimer_ioctl_data*)arg, (size_t)size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                if (!dev->irq_on){
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;	
                }
                if (data.f_Irq_Sig > 4) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                for (i = 0; i < dev->server_signals; i++) {
                    if (dev->server_signal_stack [i].f_ServerPref == 0 ||
                        dev->server_signal_stack [i].f_ServerPref == cur_proc) break;
		}
                if (i == dev->server_signals) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;	
                }
                dev->server_signal_stack [i].f_ServerPref = cur_proc;
                switch(data.f_Irq_Sig){
                    case SIG1:
                    case SIG2:
                    case URG:
                    case MSK_WANT:
                        dev->server_signal_stack [i].f_IrqProc[data.f_Irq_Sig] |= data.f_Irq_Mask;
                        tmp_data_16 = ioread16(address + (offset + 0x28));
                        rmb();
                        tmp_data_16 |= data.f_Irq_Mask;
                        dev->reg_map.wr16 [20]  |= data.f_Irq_Mask;
                        iowrite16(tmp_data_16,  (address + (offset + 0x28)));
                        smp_wmb();
                        dev->irq_mask |= data.f_Irq_Mask;
                        break;
                    case CUR_SIG:
                        dev->server_signal_stack [i].f_IrqProc[data.f_Irq_Sig]	|= data.f_Irq_Mask;
                        dev->server_signal_stack [i].f_CurSig = data.f_Status;
                        tmp_data_16 = ioread16(address + (offset + 0x28));
                        rmb();
                        tmp_data_16             |= data.f_Irq_Mask;
                        dev->reg_map.wr16 [20]  |= data.f_Irq_Mask;
                        iowrite16(tmp_data_16,  (address + (offset + 0x28)));
                        smp_wmb();
                        dev->irq_mask |= data.f_Irq_Mask;
                       break;
                   default:
                       break;
                }
                if (copy_to_user((t_iptimer_ioctl_data*)arg, &data, (size_t)size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                retval = 0;
                break;
            case IPTIMER_DEL_SERVER_SIGNAL:/* t_iptimer_ioctl_data ******/
                if (copy_from_user(&data, (t_iptimer_ioctl_data*)arg, (size_t)size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                if (!dev->irq_on){
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;	
                }
                if (data.f_Irq_Sig > 4) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                new_mask = data.f_Irq_Mask;
                new_mask = ~new_mask;
                for (i = 0; i < dev->server_signals; i++) {	
                    if (dev->server_signal_stack [i].f_ServerPref == cur_proc) {
                        dev->server_signal_stack [i].f_IrqProc[data.f_Irq_Sig] &= new_mask;
                        if (dev->server_signal_stack [i].f_IrqProc[0] == 0 &&
                            dev->server_signal_stack [i].f_IrqProc[1] == 0 &&
                            dev->server_signal_stack [i].f_IrqProc[2] == 0 &&
                            dev->server_signal_stack [i].f_IrqProc[3] == 0 &&
                            dev->server_signal_stack [i].f_IrqProc[4] == 0) {
                                dev->server_signal_stack [i].f_ServerPref = 0;
                        }
                    }
                    gen_mask |= dev->server_signal_stack [i].f_IrqProc[0];
                    gen_mask |= dev->server_signal_stack [i].f_IrqProc[1];
                    gen_mask |= dev->server_signal_stack [i].f_IrqProc[2];
                    gen_mask |= dev->server_signal_stack [i].f_IrqProc[3];
                    gen_mask |= dev->server_signal_stack [i].f_IrqProc[4];
                }
                dev->reg_map.wr16 [20]  = gen_mask;
                dev->irq_mask           = gen_mask;
                iowrite16(gen_mask,(address + (offset + 0x28)));
	        smp_wmb();	 
                if (copy_to_user((t_iptimer_ioctl_data*)arg, &data, (size_t)size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                retval = 0;
		break;
            case IPTIMER_GET_STATUS:/* t_iptimer_ioctl_data ******/
                if (copy_from_user(&data, (t_iptimer_ioctl_data*)arg, (size_t)size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                if (!dev->irq_on){
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;	
                }
                data.f_Status 	= 0;
                data.f_Irq_Sig 	= 0;
                data.f_Irq_Mask	= 0;	
                for (i = 0; i < dev->server_signals; i++) {
                    if (dev->server_signal_stack [i].f_ServerPref == cur_proc) {
                        data.f_Irq_Mask = dev->server_signal_stack [i].f_IrqProc[0];
                        data.f_Irq_Sig  = dev->server_signal_stack [i].f_IrqProc[1];
                        data.f_Status   = dev->server_signal_stack [i].f_IrqProc[3];
                        break;
                    }
                }
                if (copy_to_user((t_iptimer_ioctl_data*)arg, &data, (size_t)size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                retval = 0;
                break;
            case IPTIMER_READ_IRQ:/* t_iptimer_ioctl_data ******/
                if (copy_from_user(&data, (t_iptimer_ioctl_data*)arg, (size_t)size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                if (!dev->irq_on){
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;	
                }
                data.f_Status 	= dev->irq_status;
                data.f_Irq_Sig 	= dev->irq_flag;
                data.f_Irq_Mask	= dev->irq_mask;
                if (copy_to_user((t_iptimer_ioctl_data*)arg, &data, (size_t)size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                retval = 0;
                break;
	    case IPTIMER_SET_MASK:/* t_iptimer_ioctl_data */
                if (copy_from_user(&data, (t_iptimer_ioctl_data*)arg, (size_t)size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
/*
                printk(KERN_ALERT "TAMC200_SET_MASK f_Irq_Mask %x f_Irq_Sig %x f_Status %x \n", 
                                   data.f_Irq_Mask, data.f_Irq_Sig, data.f_Status);
*/
                data.f_Irq_Mask++;
                data.f_Irq_Sig++;
                data.f_Status++;
                if (copy_to_user((t_iptimer_ioctl_data*)arg, &data, (size_t)size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                retval = 0;
                break;
            case IPTIMER_READ_IRQ_TM:/* t_iptimer_ioctl_time ******/
                if (copy_from_user(&time_data, (t_iptimer_ioctl_time*)arg, (size_t)time_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                if (!dev->irq_on){
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;	
                }
                time_data.f_Irq_Sts 	= dev->irq_status;
                time_data.f_Irq_Flag 	= dev->irq_flag;
                time_data.f_Irq_Mask 	= dev->irq_mask;
                time_data.f_Irq_Sec 	= (int)dev->irq_sec;
                time_data.f_Irq_uSec 	= (int)dev->irq_usec;
                if (copy_to_user((t_iptimer_ioctl_time*)arg, &time_data, (size_t)time_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                retval = 0;
                break;
            case IPTIMER_SET_IRQ:/* t_iptimer_ioctl_data ******/
                if (copy_from_user(&data, (t_iptimer_ioctl_data*)arg, (size_t)size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                switch (data.f_Irq_Mask){
                  case IPTIMER_IRQ_EVA:
                      tmp_data_16 = ioread16(address + (offset + 0x20));
                      rmb();
                      tmp_data_16	        &= 0xFF00;
                      tmp_data_16	        |= (data.f_Status & 0xFF);
                      dev->reg_map.wr16[16] = tmp_data_16;
                      iowrite16(tmp_data_16,  (address + (offset + 0x20)));
                      smp_wmb();
                      break;
                  case IPTIMER_IRQ_EVB:
                      tmp_data_16 = ioread16(address + (offset + 0x20));
                      rmb();
                      tmp_data_16 &= 0xFF;
                      tmp_data_16 |= (data.f_Status << 8);
                      dev->reg_map.wr16[16] = tmp_data_16;
                      iowrite16(tmp_data_16,  (address + (offset + 0x20)));
                      smp_wmb();
                      break;
                  case IPTIMER_IRQ_EVC:
                      tmp_data_16 = ioread16(address + (offset + 0x22));
                      rmb();
                      tmp_data_16	        &= 0xFF00;
                      tmp_data_16	        |= (data.f_Status & 0xFF);
                      dev->reg_map.wr16[17] = tmp_data_16;
                      iowrite16(tmp_data_16,  (address + (offset + 0x22)));
                      smp_wmb();
                      break;
                  case IPTIMER_IRQ_EVD:
                      tmp_data_16 = ioread16(address + (offset + 0x22));
                      rmb();
                      tmp_data_16 &= 0xFF;
                      tmp_data_16 |= (data.f_Status << 8);
                      dev->reg_map.wr16[17] = tmp_data_16;
                      iowrite16(tmp_data_16,  (address + (offset + 0x22)));
                      smp_wmb();
                      break;
                  case IPTIMER_IRQ_EVE:
                      tmp_data_16 = ioread16(address + (offset + 0x24));
                      rmb();
                      tmp_data_16	        &= 0xFF00;
                      tmp_data_16	        |= (data.f_Status & 0xFF);
                      dev->reg_map.wr16[18] = tmp_data_16;
                      iowrite16(tmp_data_16,  (address + (offset + 0x24)));
                      smp_wmb();
                      break;
                  case IPTIMER_IRQ_EVF:
                      tmp_data_16 = ioread16(address + (offset + 0x24));
                      rmb();
                      tmp_data_16 &= 0xFF;
                      tmp_data_16 |= (data.f_Status << 8);
                      dev->reg_map.wr16[18] = tmp_data_16;
                      iowrite16(tmp_data_16,  (address + (offset + 0x24)));
                      smp_wmb();
                      break;
                  case IPTIMER_IRQ_EVG:
                      tmp_data_16 = ioread16(address + (offset + 0x26));
                      rmb();
                      tmp_data_16	        &= 0xFF00;
                      tmp_data_16	        |= (data.f_Status & 0xFF);
                      dev->reg_map.wr16[19] = tmp_data_16;
                      iowrite16(tmp_data_16,  (address + (offset + 0x26)));
                      smp_wmb();
                      break;
                  case IPTIMER_IRQ_EVH:
                      tmp_data_16 = ioread16(address + (offset + 0x26));
                      rmb();
                      tmp_data_16 &= 0xFF;
                      tmp_data_16 |= (data.f_Status << 8);
                      dev->reg_map.wr16[19] = tmp_data_16;
                      iowrite16(tmp_data_16,  (address + (offset + 0x26)));
                      smp_wmb();
                      break;
                  case IPTIMER_IRQ_CH0:
                  case IPTIMER_IRQ_CH1:
                  case IPTIMER_IRQ_CH2:
                  case IPTIMER_IRQ_CH3:
                  case IPTIMER_IRQ_CH4:
                  case IPTIMER_IRQ_CH5:
                  case IPTIMER_IRQ_CH6:
                  case IPTIMER_IRQ_CH7:
                          break;
                 default:
                         break;
		}
                if (copy_to_user((t_iptimer_ioctl_data*)arg, &data, (size_t)size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                retval = 0;
                break;
            case IPTIMER_CHECK_IRQ:/* t_iptimer_ioctl_data */
                if (copy_from_user(&data, (t_iptimer_ioctl_data*)arg, (size_t)size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                if (!dev->irq_on){
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                data.f_Irq_Sig 	= dev->irq_flag;
                data.f_Irq_Mask = dev->irq_mask;
                for (i = 0; i < dev->server_signals; i++) {
                    if (dev->server_signal_stack [i].f_ServerPref == cur_proc) {
                        data.f_Status   = dev->server_signal_stack [i].f_IrqFlag;
                        dev->server_signal_stack [i].f_IrqFlag = 0;
                        break;
                    }
                }  
                if (copy_to_user((t_iptimer_ioctl_data*)arg, &data, (size_t)size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                retval = 0;
                break;
            case IPTIMER_CHECK_SVR: /*  t_iptimer_reg_data  */
                if (copy_from_user(&reg_data, (t_iptimer_reg_data*)arg, (size_t)reg_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                printk(KERN_ALERT "TAMC200_CHECK_SVR io_chn %x io_data %x io_rw %x \n", 
                                   reg_data.data[0], reg_data.data[1], reg_data.data[2]++);
                reg_data.data[0]++;
                reg_data.data[1]++;
                reg_data.data[2]++;
                if (copy_to_user((t_iptimer_reg_data*)arg, &reg_data, (size_t)reg_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                retval = 0;
                break;
            case IPTIMER_REM_SVR:/* t_iptimer_ioctl_data */ 
                if (copy_from_user(&data, (t_iptimer_ioctl_data*)arg, (size_t)size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                printk(KERN_ALERT "TAMC200_REM_SVR f_Irq_Mask %x f_Irq_Sig %x f_Status %x \n", 
                                   data.f_Irq_Mask, data.f_Irq_Sig, data.f_Status);
                data.f_Irq_Mask++;
                data.f_Irq_Sig++;
                data.f_Status++;
                if (copy_to_user((t_iptimer_ioctl_data*)arg, &data, (size_t)size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                retval = 0;
                break;
            case IPTIMER_DELAY:/* t_iptimer_ioctl_rw ********/
                if (copy_from_user(&rw_data, (t_iptimer_ioctl_rw*)arg, (size_t)rw_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                tmp_chn = rw_data.io_chn;
                if (tmp_chn >7 ){
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                offset += rw_data.io_chn*2;
                switch (rw_data.io_rw) {
                    case 1:
                        tmp_data_16 = rw_data.io_data;
                        dev->reg_map.wr16 [(rw_data.io_chn )] = rw_data.io_data;
                        iowrite16(tmp_data_16, (address + offset));
                        smp_wmb();
                        if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                                retval = -EFAULT;
                                mutex_unlock(&dev->dev_mut);
                                return retval;
                        }
                        break;
                    case 0:
                        tmp_data_16 = ioread16(address + offset);
                        rmb();
                        rw_data.io_data = tmp_data_16;
                        if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                                retval = -EFAULT;
                                mutex_unlock(&dev->dev_mut);
                                return retval;
                        }
                        break;
                    default:
                       rw_data.io_chn  = 0;
                       rw_data.io_data = 0;
                       rw_data.io_rw   = 0;
                       if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                                retval = -EFAULT;
                                mutex_unlock(&dev->dev_mut);
                                return retval;
                        }
                        retval = -EFAULT;
                        mutex_unlock(&dev->dev_mut);
                        return retval;
                }
                retval = 0;
                break;
            case IPTIMER_EVENT:/* t_iptimer_ioctl_rw ********/
                if (copy_from_user(&rw_data, (t_iptimer_ioctl_rw*)arg, (size_t)rw_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                if (rw_data.io_chn >3 ){
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                offset += 0x10;
                offset += rw_data.io_chn*2;
                switch (rw_data.io_rw) {
                    case 1:
                        tmp_data_16 = ioread16(address + offset);
                        rmb();
                        rw_data.io_data = (rw_data.io_data << 8) ;
                        rw_data.io_data |= 0xFF;
                        tmp_data_16 |= 0xFF00;
                        tmp_data_16 &= rw_data.io_data;
                        tmp_data_16 &= 0xFFFF;
                        dev->reg_map.wr16 [(8 + rw_data.io_chn )] = tmp_data_16;
                        iowrite16(tmp_data_16, (address + offset));
                        smp_wmb();
                        rw_data.io_data = tmp_data_16;
                        if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                                retval = -EFAULT;
                                mutex_unlock(&dev->dev_mut);
                                return retval;
                        }
                        break;
                    case 0:
                        tmp_data_16 = ioread16(address + offset);
                        rmb();
                        tmp_data_16 = tmp_data_16 >> 8;
                        rw_data.io_data = tmp_data_16;
                        if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                                retval = -EFAULT;
                                mutex_unlock(&dev->dev_mut);
                                return retval;
                        }
                        break;
                    default:
                        rw_data.io_chn  = 0;
                        rw_data.io_data = 0;
                        rw_data.io_rw   = 0;
                        if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                                retval = -EFAULT;
                                mutex_unlock(&dev->dev_mut);
                                return retval;
                        }
                        retval = -EFAULT;
                        mutex_unlock(&dev->dev_mut);
                        return retval;
                }
                retval = 0;
                break;
            case IPTIMER_DAISY_CHAIN:/* t_iptimer_ioctl_rw *******/
                if (copy_from_user(&rw_data, (t_iptimer_ioctl_rw*)arg, (size_t)rw_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                if (rw_data.io_chn >5 || rw_data.io_chn == 2 || rw_data.io_chn == 3){
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                offset += 0x10;
                offset += 2*(rw_data.io_chn/4);
                switch (rw_data.io_rw){
                    case 1:
                        tmp_data_16 = ioread16(address + offset);
                        rmb();
                        switch (rw_data.io_chn%2){
                            case 0:
                                tmp_data_16 &= 0xFFBF;
                                rw_data.io_data = rw_data.io_data << 6;
                                tmp_data_16 |= (rw_data.io_data & 0x40);
                                break;
                            case 1:
                                tmp_data_16 &= 0xFF7F;
                                rw_data.io_data = rw_data.io_data << 7;
                                tmp_data_16 |= (rw_data.io_data & 0x80);
                                break;
                            default:
                                break;
                        }
                        dev->reg_map.wr16 [(8 + 2*(rw_data.io_chn/4))] = tmp_data_16;
                        iowrite16(tmp_data_16, (address + offset));
                        smp_wmb();
                        if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                                retval = -EFAULT;
                                mutex_unlock(&dev->dev_mut);
                                return retval;
                        }
                        break;
                    case 0:
                        tmp_data_16 = ioread16(address + offset);
                        rmb();
                        switch (rw_data.io_chn%2){
                            case 0:
                                tmp_data_16     = tmp_data_16 >> 6;
                                rw_data.io_data = tmp_data_16 & 0x1;
                                break;
                            case 1:
                                tmp_data_16     = tmp_data_16 >> 7;
                                rw_data.io_data = tmp_data_16 & 0x1;
                                break;
                            default:
                                break;
                        }
                        if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                            retval = -EFAULT;
                            mutex_unlock(&dev->dev_mut);
                            return retval;
                        }
                        break;
                    default:
                        rw_data.io_chn  = 0;
                        rw_data.io_data = 0;
                        rw_data.io_rw   = 0;
                        if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                            retval = -EFAULT;
                            mutex_unlock(&dev->dev_mut);
                            return retval;
                        }
                        retval = -EFAULT;
                        mutex_unlock(&dev->dev_mut);
                        return retval;
                }
                retval = 0;
                break;
            case IPTIMER_TIME_BASE:/* t_iptimer_ioctl_rw ******/
                if (copy_from_user(&rw_data, (t_iptimer_ioctl_rw*)arg, (size_t)rw_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                if (rw_data.io_chn >3 ){
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                offset += 0x10;
                offset += rw_data.io_chn*2;
                switch (rw_data.io_rw){
                    case 1:
                        tmp_data_16 = ioread16(address + offset);
                        rmb();
                        rw_data.io_data = rw_data.io_data << 5;
                        tmp_data_16 &= 0xFFDF ;
                        tmp_data_16 |= (rw_data.io_data & 0x20);
                        dev->reg_map.wr16 [(8 + rw_data.io_chn )] = tmp_data_16;
                        iowrite16(tmp_data_16, (address + offset));
                        smp_wmb();
                        if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                            retval = -EFAULT;
                            mutex_unlock(&dev->dev_mut);
                            return retval;
                        }
                        break;
                    case 0:
                        tmp_data_16 = ioread16(address + offset);
                        rmb();
                        tmp_data_16 = tmp_data_16 >> 5;
                        tmp_data_16 = tmp_data_16 & 0x1;
                        rw_data.io_data = tmp_data_16;
                        if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                            retval = -EFAULT;
                            mutex_unlock(&dev->dev_mut);
                            return retval;
                        }
                        break;
                    default:
                        rw_data.io_chn  = 0;
                        rw_data.io_data = 0;
                        rw_data.io_rw   = 0;
                        if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                            retval = -EFAULT;
                            mutex_unlock(&dev->dev_mut);
                            return retval;
                        }
                        retval = -EFAULT;
                        mutex_unlock(&dev->dev_mut);
                        return retval;
                }
                retval = 0;
                break;
            case IPTIMER_PULSE_LENGTH:/* t_iptimer_ioctl_rw ******/
                if (copy_from_user(&rw_data, (t_iptimer_ioctl_rw*)arg, (size_t)rw_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                if (rw_data.io_chn >7 ){
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                offset += 0x10;
                offset += (rw_data.io_chn/2)*2;
                switch (rw_data.io_rw){
                    case 1:
                        tmp_data_16 = ioread16(address + offset);
                        rmb();
                        switch (rw_data.io_chn%2){
                            case 0:
                                tmp_data_16 &= 0xFFFB;
                                rw_data.io_data = rw_data.io_data << 2;
                                tmp_data_16 |= (rw_data.io_data & 0x4);
                                break;
                            case 1:
                                rw_data.io_data = rw_data.io_data << 3;
                                tmp_data_16 &= 0xFFF7;
                                tmp_data_16 |= (rw_data.io_data & 0x8);
                                break;
                            default:
                                break;
                        }
                        dev->reg_map.wr16 [(8 + (rw_data.io_chn/2) )] = tmp_data_16;
                        iowrite16(tmp_data_16, (address + offset));
                        smp_wmb();
                        if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                            retval = -EFAULT;
                            mutex_unlock(&dev->dev_mut);
                            return retval;
                        }
                        break;
                    case 0:
                        tmp_data_16 = ioread16(address + offset);
                        rmb();
                        switch (rw_data.io_chn%2){
                            case 0:
                                tmp_data_16 = tmp_data_16 >> 2;
                                rw_data.io_data = tmp_data_16 & 0x1;
                                break;
                            case 1:
                                tmp_data_16 = tmp_data_16 >> 3;
                                rw_data.io_data = tmp_data_16 & 0x1;
                                break;
                            default:
                                break;
                        }
                        if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                            retval = -EFAULT;
                            mutex_unlock(&dev->dev_mut);
                            return retval;
                        }
                        break;
                    default:
                        rw_data.io_chn  = 0;
                        rw_data.io_data = 0;
                        rw_data.io_rw   = 0;
                        if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                            retval = -EFAULT;
                            mutex_unlock(&dev->dev_mut);
                            return retval;
                        }
                        retval = -EFAULT;
                        mutex_unlock(&dev->dev_mut);
                        return retval;
                }
                retval = 0;
                break;
	    case IPTIMER_OUT:/* t_iptimer_ioctl_rw ******/
                if (copy_from_user(&rw_data, (t_iptimer_ioctl_rw*)arg, (size_t)rw_size)) {
                        retval = -EFAULT;
                        mutex_unlock(&dev->dev_mut);
                        return retval;
                }
                if (rw_data.io_chn >7){
                        retval = -EFAULT;
                        mutex_unlock(&dev->dev_mut);
                        return retval;
                }
                offset += 0x10;
                offset += (rw_data.io_chn/2)*2;
                switch (rw_data.io_rw){
                    case 1:
                        tmp_data_16 = ioread16(address + offset);
                        rmb();
                        switch (rw_data.io_chn%2){
                            case 0:
                                tmp_data_16 &= 0xFFFE;
                                tmp_data_16 |= (rw_data.io_data & 0x1);
                                break;
                            case 1:
                                rw_data.io_data = rw_data.io_data << 1;
                                tmp_data_16 &= 0xFFFD;
                                tmp_data_16 |= (rw_data.io_data & 0x2);
                                break;
                            default:
                                break;
                        }
                        dev->reg_map.wr16 [(8 + (rw_data.io_chn/2) )] = tmp_data_16;
                        iowrite16(tmp_data_16, (address + offset));
                        smp_wmb();
                        if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                            retval = -EFAULT;
                            mutex_unlock(&dev->dev_mut);
                            return retval;
                        }
                        break;
                    case 0:
                        tmp_data_16 = ioread16(address + offset);
                        rmb();
                        switch (rw_data.io_chn%2){
                            case 0:
                                rw_data.io_data = tmp_data_16 & 0x1;
                                break;
                            case 1:
                                rw_data.io_data = tmp_data_16 & 0x2;
                                rw_data.io_data = rw_data.io_data > 1;
                                break;
                            default:
                                break;
                        }
                        if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                            retval = -EFAULT;
                            mutex_unlock(&dev->dev_mut);
                            return retval;
                        }
                        break;
                    default:
                        rw_data.io_chn  = 0;
                        rw_data.io_data = 0;
                        rw_data.io_rw   = 0;
                        if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                            retval = -EFAULT;
                            mutex_unlock(&dev->dev_mut);
                            return retval;
                        }
                        retval = -EFAULT;
                        mutex_unlock(&dev->dev_mut);
                        return retval;
                }
                retval = 0;
                break;
            case IPTIMER_VARCLOCK:/* t_iptimer_ioctl_rw ******/
                if (copy_from_user(&rw_data, (t_iptimer_ioctl_rw*)arg, (size_t)rw_size)) {
                        retval = -EFAULT;
                        mutex_unlock(&dev->dev_mut);
                        return retval;
                }
                if (rw_data.io_data >3 ){
                        retval = -EFAULT;
                        mutex_unlock(&dev->dev_mut);
                        return retval;
                }
                offset += 0x16;
                switch (rw_data.io_rw){
                    case 1:
                        tmp_data_16 = ioread16(address + offset);
                        rmb();
                        rw_data.io_data = rw_data.io_data << 6;
                        tmp_data_16 &= 0xFF3F ;
                        tmp_data_16 |= (rw_data.io_data & 0xC0);
                        dev->reg_map.wr16 [11] = tmp_data_16;
                        iowrite16(tmp_data_16, (address + offset));
                        smp_wmb();
                        if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                            retval = -EFAULT;
                            mutex_unlock(&dev->dev_mut);
                            return retval;
                        }
                        break;
                    case 0:
                        tmp_data_16 = ioread16(address + offset);
                        rmb();
                        tmp_data_16 = tmp_data_16 >> 6;
                        tmp_data_16 = tmp_data_16 & 0x3;
                        rw_data.io_data = tmp_data_16;
                        if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                            retval = -EFAULT;
                            mutex_unlock(&dev->dev_mut);
                            return retval;
                        }
                        break;
                    default:
                        rw_data.io_chn  = 0;
                        rw_data.io_data = 0;
                        rw_data.io_rw   = 0;
                        if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                            retval = -EFAULT;
                            mutex_unlock(&dev->dev_mut);
                            return retval;
                        }
                        retval = -EFAULT;
                        mutex_unlock(&dev->dev_mut);
                        return retval;
                }
                retval = 0;
                break;
            case IPTIMER_GET_REG:/* t_iptimer_reg_data ********/
                if (copy_from_user(&reg_data, (t_iptimer_reg_data*)arg, (size_t)reg_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                for (i = 0; i < 64; i++){
                    tmp_data_16 = ioread16(address + (offset + i*2));
                    rmb();
                    reg_data.data[i] = tmp_data_16;
                }
                if (copy_to_user((t_iptimer_reg_data*)arg, &reg_data, (size_t)reg_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                retval = 0;
                break;
	    case IPTIMER_GET_CLCK_STS:/* t_iptimer_ioctl_rw ******/
                if (copy_from_user(&rw_data, (t_iptimer_ioctl_rw*)arg, (size_t)rw_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                offset += 0x12;
                tmp_data_16 = ioread16(address + offset);
                rmb();
                tmp_data_16     = tmp_data_16 >> 6;
                tmp_data_16     = tmp_data_16 & 0x1;
                rw_data.io_data = tmp_data_16;
                if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                retval = 0;
                break;
            case IPTIMER_GET_LOC_REG:/* t_iptimer_reg_data ********/
                if (copy_from_user(&reg_data, (t_iptimer_reg_data*)arg, (size_t)reg_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                for (i = 0; i < 64; i++){
                    reg_data.data[i] = dev->reg_map.wr16 [i];
                }
                if (copy_to_user((t_iptimer_reg_data*)arg, &reg_data, (size_t)reg_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                retval = 0;
                break;
            case IPTIMER_RAM_SET:
                if (copy_from_user(&rw_data, (t_iptimer_ioctl_rw*)arg, (size_t)rw_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                offset += 0x18;
                switch (rw_data.io_rw){
                    case 1:
                        tmp_data_16 = ioread16(address + offset);
                        smp_rmb();
                        tmp_data_16 &= 0xFFFE;
                        tmp_data_16 |= (rw_data.io_data & 0x1);
                        dev->reg_map.wr16 [12] = tmp_data_16;
                        iowrite16(tmp_data_16, (address + offset));
                        smp_wmb();
                        break;
                    case 0:
                        rw_data.io_data = ioread16(address + offset);
                        smp_rmb();
                        rw_data.io_data = rw_data.io_data & 0x1;
                        if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                           retval = -EFAULT;
                           mutex_unlock(&dev->dev_mut);
                           return retval;
                        }
                        break;
                    case 2:
                        tmp_data_16 = ioread16(address + offset);
                        smp_rmb();
                        tmp_data_16 = tmp_data_16 >> 4 ;
                        tmp_data_16 &= 0x1;
                        rw_data.io_data = tmp_data_16;
                        if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                            retval = -EFAULT;
                            mutex_unlock(&dev->dev_mut);
                            return retval;
                        }
                        break;
                    default:
                        retval = -EFAULT;
                        mutex_unlock(&dev->dev_mut);
                        return retval;
                }
                retval = 0;
                break;
            case IPTIMER_EVENT_CHANNEL:
                if (copy_from_user(&rw_data, (t_iptimer_ioctl_rw*)arg, (size_t)rw_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                dev->event_mask = rw_data.io_chn;
                retval = 0;
                break;
            case IPTIMER_GET_EVENT:
                if (copy_from_user(&rw_data, (t_iptimer_ioctl_rw*)arg, (size_t)rw_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                
                offset += 0x40;
                rw_data.io_data = ioread16(address + offset);
	        smp_rmb();
	        offset += 0x2;
	        rw_data.io_chn = ioread16(address + offset);
	        smp_rmb();
                
                rw_data.io_data = dev->gen_event_low;
                rw_data.io_chn  = dev->gen_event_hi;
                
                
                if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                   retval = -EFAULT;
                   mutex_unlock(&dev->dev_mut);
                   return retval;
                }
                retval = 0;
                break;
             case IPTIMER_EVENT_INFO:
                
                event_data.f_event_low  = dev->gen_event_low;
                event_data.f_event_high = dev->gen_event_hi;
                event_data.f_Irq_Sts 	= dev->irq_status;
                event_data.f_Irq_Flag 	= dev->irq_flag;
                event_data.f_Irq_Mask 	= dev->irq_mask;
                event_data.f_Irq_Sec 	= (int)dev->irq_sec;
                event_data.f_Irq_uSec 	= (int)dev->irq_usec;
                event_data.f_Irq_Delay 	= (int)dev->irq_delay;
                
                if (copy_to_user((t_iptimer_ioctl_event*)arg, &event_data, (size_t)event_size)) {
                   retval = -EFAULT;
                   mutex_unlock(&dev->dev_mut);
                   return retval;
                }
                retval = 0;
                break;
            case IPTIMER_EVENT_CHECK:
                if (copy_from_user(&rw_data, (t_iptimer_ioctl_rw*)arg, (size_t)rw_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                offset += 0x2C;
                switch (rw_data.io_rw) {
                    case 1: /*read with reset*/
                        rw_data.io_data = dev->event_value;
                        iowrite16(0xFFFF, (address + offset));
                        smp_wmb();
                        dev->reg_map.wr16 [22] = 0;
                        break;
                    case 0:/*read without reset*/
                        rw_data.io_data  = dev->event_value;
                        dev->reg_map.wr16[22] = dev->event_value;
                        break;
                    default:
                        rw_data.io_data = dev->event_value;
                        dev->reg_map.wr16 [22] = rw_data.io_data;
                        break;
                }
                if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                   retval = -EFAULT;
                   mutex_unlock(&dev->dev_mut);
                   return retval;
                }
                retval = 0;
                break;
            case IPTIMER_DRIVER_VERSION:
                rw_data.io_data = TAMC200_DRV_VER_MAJ;
                rw_data.io_chn  = TAMC200_DRV_VER_MIN;
                if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                   retval = -EFAULT;
                   mutex_unlock(&dev->dev_mut);
                   return retval;
                }
                retval = 0;
                break; 
            case IPTIMER_IRQ_DEALY:
                if (copy_from_user(&rw_data, (t_iptimer_ioctl_rw*)arg, (size_t)rw_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                switch (rw_data.io_rw) {
                    case 1: /*read with reset*/
                        dev->irq_delay_mask = rw_data.io_data;
                        break;
                    case 0:/*read without reset*/
                        rw_data.io_data = dev->irq_delay_mask;
                        break;
                    default:
                        rw_data.io_data = dev->irq_delay_mask;
                        break;
                }
                if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                   retval = -EFAULT;
                   mutex_unlock(&dev->dev_mut);
                   return retval;
                }
                retval = 0;
                break; 
            case IPTIMER_JITTER_TEST:
                if (copy_from_user(&rw_data, (t_iptimer_ioctl_rw*)arg, (size_t)rw_size)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                offset += 0x2C;
                switch (rw_data.io_rw) {
                    case 1: /*set jitter test*/
                        dev->irq_jitter_test = 1;
                        break;
                    case 0:/*reset jitter test*/
                        dev->irq_jitter_test = 0;
                        break;
                    default:
                        dev->irq_jitter_test = 0;
                        break;
                }
                if (copy_to_user((t_iptimer_ioctl_rw*)arg, &rw_data, (size_t)rw_size)) {
                   retval = -EFAULT;
                   mutex_unlock(&dev->dev_mut);
                   return retval;
                }
                retval = 0;
                break;
	    default:
                mutex_unlock(&dev->dev_mut);
                return -ENOTTY;
	}
    mutex_unlock(&dev->dev_mut);
    return retval;
}

struct file_operations tamc200_fops = {
	.owner   =  THIS_MODULE,
	.read    =  tamc200_read,
	.write   =  tamc200_write,
	//.ioctl   =  tamc200_ioctl,
	.open    =  tamc200_open,
	.release =  tamc200_release,
};

struct file_operations tamc200s_fops = {
	.owner   =  THIS_MODULE,
	.read    =  tamc200s_read,
	.write   =  tamc200s_write,
        //.ioctl   =  tamc200s_ioctl,
	.unlocked_ioctl   =  tamc200s_ioctl,
	.open    =  tamc200s_open,
	.release =  tamc200s_release,
};

static void __exit tamc200_cleanup_module(void)
{
    int   i     = 0;
    int   k     = 0;
    dev_t devno;
    
     printk(KERN_ALERT "TAMC200_CLEANUP_MODULE\n");
    
    devno = MKDEV(TAMC200_MAJOR, TAMC200_MINOR);
    flush_workqueue(tamc200_workqueue);
    destroy_workqueue(tamc200_workqueue);
    
    pci_unregister_driver(&pci_tamc200_driver);
    printk(KERN_ALERT "TAMC200_CLEANUP_MODULE: PCI DRIVER UNREGISTERED\n");

    for(i = 0; i < TAMC200_NR_DEVS; i++){
        mutex_destroy(&tamc200_dev[i].dev_mut);
        /* Get rid of our char dev entries */
        cdev_del(&tamc200_dev[i].cdev);
        for(k = 0; k < TAMC200_NR_SLOTS; k++){
                if(tamc200_dev[i].ip_s[k].ip_on){
                mutex_destroy(&tamc200_dev[i].ip_s[k].dev_mut);
                /* Get rid of our char dev entries */
                cdev_del(&tamc200_dev[i].ip_s[k].cdev);
            }
        }
    }
    /* cleanup_module is never called if registering failed */
    unregister_chrdev_region(devno, TAMC200_NR_FDEVS);
    class_destroy(tamc200_class);
        
}

static int __init tamc200_init_module(void)
{
    int   result    = 0;
    dev_t dev       = 0;
    int   devno     = 0;
    int   i         = 0;
    int   k         = 0;
    int   tmp_minor = 0;

    printk(KERN_WARNING "TAMC200: tamc200_init_module CALLED\n");

    result = alloc_chrdev_region(&dev, TAMC200_MINOR, TAMC200_NR_FDEVS, DEVNAME);
    TAMC200_MAJOR = MAJOR(dev);
    printk(KERN_ALERT "TAMC200: AFTER_INIT DRV_MAJOR is %d\n", TAMC200_MAJOR);

    tamc200_class = class_create(THIS_MODULE, "tamc200");
    for(k = 0; k< TAMC200_NR_SLOTS; k++){
    printk(KERN_ALERT "TAMC200: AFTER_INIT SLOT %i ON %i, TYPE %i, IRQ %i, IRQ_LEV %i, IRQ_VEC %i\n", 
                              k, ips_on[k], ips_type[k], ips_irq[k], ips_irq_lev[k], ips_irq_vec[k]);
    }
    printk(KERN_ALERT "TAMC200: AFTER_INIT:GO THRUH DEVs\n");
    for(i = 0; i < TAMC200_NR_DEVS; i++){
        printk(KERN_ALERT "TAMC200: AFTER_INIT:INIT CDEV %i\n", i);
        devno = MKDEV(TAMC200_MAJOR, TAMC200_MINOR + tmp_minor); 
        cdev_init(&tamc200_dev[i].cdev, &tamc200_fops);
        tamc200_dev[i].cdev.owner = THIS_MODULE;
        tamc200_dev[i].cdev.ops = &tamc200_fops;
        result = cdev_add(&tamc200_dev[i].cdev, devno, 1);
        if (result){
            printk(KERN_NOTICE "TAMC200: Error %d adding devno%d num%d", result, devno, i);
            return 1;
        }
        mutex_init(&tamc200_dev[i].dev_mut);
        tamc200_dev[i].dev_num   = tmp_minor;
        tamc200_dev[i].dev_minor = (TAMC200_MINOR + tmp_minor);
        tamc200_dev[i].dev_sts   = 0;
        tamc200_dev[i].slot_num  = 0;

        #if LINUX_VERSION_CODE < 132632 
        INIT_WORK(&(tamc200_dev[i].tamc200_work),tamc200_do_work, &tamc200_dev[i]);
        #else
        INIT_WORK(&(tamc200_dev[i].tamc200_work),tamc200_do_work);    
        #endif     

        tamc200_dev[i].memmory_base0 = 0;
        tamc200_dev[i].memmory_base2 = 0;
        tamc200_dev[i].memmory_base3 = 0;
        tamc200_dev[i].memmory_base4 = 0;
        tamc200_dev[i].memmory_base5 = 0;
        tmp_minor++;
        for(k = 0; k < TAMC200_NR_SLOTS; k++){
            printk(KERN_ALERT "TAMC200: AFTER_INIT:INIT CDEV %i SLOT %i\n", i, k);
            tamc200_dev[i].ip_s[k].ip_on              = ips_on[k];
            tamc200_dev[i].ip_s[k].dev_sts            = 0;
            tamc200_dev[i].ip_s[k].ptamc200_dev       = &tamc200_dev[i];
            if(k== 0){
               tamc200_dev[i].TAMC200_A_ON   = ips_on[k];
               tamc200_dev[i].TAMC200_A_TYPE = ips_type[k];
            }
            if(k== 1){
               tamc200_dev[i].TAMC200_B_ON   = ips_on[k];
               tamc200_dev[i].TAMC200_B_TYPE = ips_type[k];
            }
            if(k== 2){
               tamc200_dev[i].TAMC200_C_ON   = ips_on[k];
               tamc200_dev[i].TAMC200_C_TYPE = ips_type[k];
            }
            if(ips_on[k]){
                tamc200_dev[i].ip_s[k].ip_module_type = ips_type[k];
                tamc200_dev[i].ip_s[k].irq_on         = ips_irq[k];
                tamc200_dev[i].ip_s[k].irq_lev        = ips_irq_lev[k];
                tamc200_dev[i].ip_s[k].irq_vec        = ips_irq_vec[k];
                tamc200_dev[i].ip_s[k].irq_sens       = ips_irq_sens[k];
                devno = MKDEV(TAMC200_MAJOR, TAMC200_MINOR + tmp_minor); 
                cdev_init(&(tamc200_dev[i].ip_s[k].cdev), &tamc200s_fops);
                tamc200_dev[i].ip_s[k].cdev.owner = THIS_MODULE;
                tamc200_dev[i].ip_s[k].cdev.ops = &tamc200s_fops;
                result = cdev_add(&(tamc200_dev[i].ip_s[k].cdev), devno, 1);
                if (result){
                    printk(KERN_NOTICE "TAMC200: Error %d adding devno%d num%d", result, devno, k);
                    return 1;
                }
                mutex_init(&(tamc200_dev[i].ip_s[k].dev_mut));
                tamc200_dev[i].ip_s[k].dev_num      = tmp_minor;
                tamc200_dev[i].ip_s[k].dev_minor    = (TAMC200_MINOR + tmp_minor);
                tamc200_dev[i].ip_s[k].dev_sts      = 0;
                tamc200_dev[i].ip_s[k].ip_slot_num  = 0;
                tamc200_dev[i].ip_s[k].ip_io        = 0;
                tamc200_dev[i].ip_s[k].ip_id        = 0;
                tamc200_dev[i].ip_s[k].ip_mem       = 0;
                tamc200_dev[i].ip_s[k].ip_mem8      = 0;
                tmp_minor++;
            }
        }
    }
    
    tamc200_workqueue   = create_workqueue("tamc200");
    
    printk(KERN_ALERT "TAMC200: AFTER_INIT:REGISTERING PCI DRIVER\n");
    result = pci_register_driver(&pci_tamc200_driver);
    printk(KERN_ALERT "TAMC200: AFTER_INIT:REGISTERING PCI DRIVER RESUALT %d\n", result);
    return 0; /* succeed */
	
}

module_init(tamc200_init_module);
module_exit(tamc200_cleanup_module);
