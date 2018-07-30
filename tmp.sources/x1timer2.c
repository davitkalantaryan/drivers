#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/msi.h>
#include <linux/aer.h>
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
#include <linux/swap.h>
#include <asm/scatterlist.h>
#include <asm/io.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>	/* copy_*_user */
#include <asm/page.h>

#include "x1timer_fnc2.h"	
#include "x1timer_io.h"

////////////////////////////////////
/// New added code              ////
////////////////////////////////////
#ifdef _NEW_ADDED_CODE
#include "mtcagen_exp.h"
#include "adc_timer_interface_io.h"
#endif  // #ifdef _NEW_ADDED_CODE
////////////////////////////////////
/// End of New added code       ////
////////////////////////////////////

MODULE_AUTHOR("Lyudvig Petrosyan");
MODULE_DESCRIPTION("AMC x1timer board driver");
MODULE_VERSION("3.6.0");
MODULE_LICENSE("Dual BSD/GPL");
static u_short X1TIMER_DRV_VER_MAJ = 3;
static u_short X1TIMER_DRV_VER_MIN = 6;

int X1TIMER_MAJOR = 48;    /* major by default */
int X1TIMER_MINOR = 0 ;    /* minor by default */

struct x1timer_dev    x1timer_dev[X1TIMER_NR_DEVS + 1];
struct class               *x1timer_class;
static int                   x1timerModuleNum  = 0;

static struct proc_dir_entry *x1timer_procdir;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)

static int x1timer_procinfo(char *buf, char **start, off_t fpos, int lenght, int *eof, void *data)
{
    char *p;
    struct x1timer_dev     *dev;
    dev = data;

    p = buf;
    p += sprintf(p,"Driver Version:\t%i.%i\n", X1TIMER_DRV_VER_MAJ, X1TIMER_DRV_VER_MIN);
    p += sprintf(p,"Board ID;\t%X\n",dev->X1TIMER_BOARD_ID);
    p += sprintf(p,"Board Version:\t%X\n", dev->X1TIMER_BOARD_VERSION);
    p += sprintf(p,"Board Date:\t%X\n",dev->X1TIMER_BOARD_DATE);
    p += sprintf(p,"Board HW Version;\t%X\n",dev->X1TIMER_BOARD_HW_VERSION);
    
    p += sprintf(p,"Proj1 ID:\t%X\n", dev->X1TIMER_PROJ_1_ID);
    p += sprintf(p,"Proj1 Version;\t%X\n",dev->X1TIMER_PROJ_1_VERSION);
    p += sprintf(p,"Proj1 Date:\t%X\n",dev->X1TIMER_PROJ_1_DATE);
    p += sprintf(p,"Proj1 Reserved;\t%X\n",dev->X1TIMER_PROJ_1_RESERVED);
    
    p += sprintf(p,"Proj2 ID:\t%X\n", dev->X1TIMER_PROJ_2_ID);
    p += sprintf(p,"Proj2 Version;\t%X\n",dev->X1TIMER_PROJ_2_VERSION);
    p += sprintf(p,"Proj2 Date:\t%X\n",dev->X1TIMER_PROJ_2_DATE);
    p += sprintf(p,"Proj2 Reserved;\t%X\n",dev->X1TIMER_PROJ_2_RESERVED);
    
    p += sprintf(p,"Proj3 ID:\t%X\n", dev->X1TIMER_PROJ_3_ID);
    p += sprintf(p,"Proj3 Version;\t%X\n",dev->X1TIMER_PROJ_3_VERSION);
    p += sprintf(p,"Proj3 Date:\t%X\n",dev->X1TIMER_PROJ_3_DATE);
    p += sprintf(p,"Proj3 Reserved;\t%X\n",dev->X1TIMER_PROJ_3_RESERVED);
    
    *eof = 1;
    return p - buf;
}

static void register_x1timer_proc(int num, struct x1timer_dev     *x1timerdev)
{
    char prc_entr[32];
    sprintf(prc_entr, "x1timers%i", num);
    x1timer_procdir = create_proc_entry(prc_entr, S_IFREG | S_IRUGO, 0);
    x1timer_procdir->read_proc = x1timer_procinfo;
    x1timer_procdir->data = x1timerdev;
}


#else
    ssize_t x1timer_procinfo(struct file *filp,char *buf,size_t count,loff_t *offp );
    static const struct file_operations x1timer_proc_fops = { 
        .read = x1timer_procinfo,
    }; 
    static void register_x1timer_proc(int num, struct x1timer_dev     *x1timerdev)
    {
        char prc_entr[32];
        sprintf(prc_entr,"x1timers%i", num);
        x1timer_procdir =  proc_create_data(prc_entr, S_IFREG | S_IRUGO, 0, &x1timer_proc_fops, x1timerdev); 
    }
    ssize_t x1timer_procinfo(struct file *filp,char *buf,size_t count,loff_t *offp)
        {
#if 0
            char *p;
            int cnt = 0;
            struct x1timer_dev     *dev;
            dev = PDE_DATA(file_inode(filp));;

            p = buf;
            p += sprintf(p,"Driver Version:\t%i.%i\n", X1TIMER_DRV_VER_MAJ, X1TIMER_DRV_VER_MIN);
            p += sprintf(p,"Board ID;\t%X\n",dev->X1TIMER_BOARD_ID);
            p += sprintf(p,"Board Version:\t%X\n", dev->X1TIMER_BOARD_VERSION);
            p += sprintf(p,"Board Date:\t%X\n",dev->X1TIMER_BOARD_DATE);
            p += sprintf(p,"Board HW Version;\t%X\n",dev->X1TIMER_BOARD_HW_VERSION);

            p += sprintf(p,"Proj1 ID:\t%X\n", dev->X1TIMER_PROJ_1_ID);
            p += sprintf(p,"Proj1 Version;\t%X\n",dev->X1TIMER_PROJ_1_VERSION);
            p += sprintf(p,"Proj1 Date:\t%X\n",dev->X1TIMER_PROJ_1_DATE);
            p += sprintf(p,"Proj1 Reserved;\t%X\n",dev->X1TIMER_PROJ_1_RESERVED);

            p += sprintf(p,"Proj2 ID:\t%X\n", dev->X1TIMER_PROJ_2_ID);
            p += sprintf(p,"Proj2 Version;\t%X\n",dev->X1TIMER_PROJ_2_VERSION);
            p += sprintf(p,"Proj2 Date:\t%X\n",dev->X1TIMER_PROJ_2_DATE);
            p += sprintf(p,"Proj2 Reserved;\t%X\n",dev->X1TIMER_PROJ_2_RESERVED);

            p += sprintf(p,"Proj3 ID:\t%X\n", dev->X1TIMER_PROJ_3_ID);
            p += sprintf(p,"Proj3 Version;\t%X\n",dev->X1TIMER_PROJ_3_VERSION);
            p += sprintf(p,"Proj3 Date:\t%X\n",dev->X1TIMER_PROJ_3_DATE);
            p += sprintf(p,"Proj3 Reserved;\t%X\n",dev->X1TIMER_PROJ_3_RESERVED);

            //p += sprintf(p,"\0");
			p++ = '\0';
            //cnt = strlen(p);
			cnt = (int)((size_t)(p - buf));
            copy_to_user(buf,p, (size_t)cnt);
            return cnt;
#endif
			return 0;
        }
   
#endif
static void unregister_x1timer_proc(int num)
{
    char prc_entr[32];
    sprintf(prc_entr, "x1timers%i", num);
    remove_proc_entry(prc_entr,0);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
    #define kill_proc(p,s,v) kill_pid(find_vpid(p), s, v)
#endif
/*
 * Feeding the output queue to the device is handled by way of a
 * workqueue.
 */
#if LINUX_VERSION_CODE < 132632
    static void x1timerp_do_work(void *);
#else
    static void x1timerp_do_work(struct work_struct *work_str);
#endif
static struct workqueue_struct *x1timerp_workqueue;

#if LINUX_VERSION_CODE < 0x20613 // irq_handler_t has changed in 2.6.19
    static irqreturn_t x1timer_interrupt(int irq, void *dev_id, struct pt_regs *regs);
#else
    static irqreturn_t x1timer_interrupt(int irq, void *dev_id);
#endif
    
//static struct pci_device_id x1timer_ids[] __devinitdata = {
static struct pci_device_id x1timer_ids[] = {
	{ PCI_DEVICE(X1TIMER_VENDOR_ID, X1TIMER_DEVICE_ID), },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, x1timer_ids);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
    static int x1timer_probe(struct pci_dev *dev, const struct pci_device_id *id)
#else 
    static int __devinit x1timer_probe(struct pci_dev *dev, const struct pci_device_id *id)
#endif

{   
    int err      = 0;
    int i        = 0;
    int    m_brdNum   = 0;
    int result   = 0;
    u16 vendor_id;
    u16 device_id;
    u8  revision;
    u8  irq_line;
    u8  irq_pin;
    u64 res_start;
    u64 res_end;
    u64 res_flag;
    u32 pci_dev_irq;
    int pcie_cap;
    u8  dev_payload;
    u32 tmp_payload_size = 0;
    u32 tmp_slot_cap     = 0;
    int tmp_slot_num     = 0;
    int brd_slot_num     = 0;
    int tmp_bus_func     = 0;
    u16 subvendor_id;
    u16 subdevice_id;
    u16 class_code;
    u32 tmp_devfn;
    u32 busNumber;
    u32 devNumber;
    u32 funcNumber;
    u32 tmp_mask     = 0;
    u32 tmp_data      = 0;
    int tmp_version_min      = 0;
    int tmp_version_maj      = 0;
    
	
    printk(KERN_ALERT "X1TIMER_PCI_INIT CALLED \n");

    /*find free index*/
    for(m_brdNum = 0;m_brdNum <= X1TIMER_NR_DEVS;m_brdNum++){
        if(!x1timer_dev[m_brdNum].binded)
        break;
    }
    if(m_brdNum == X1TIMER_NR_DEVS){
        printk(KERN_ALERT "X1TIMER_PROBE  NO MORE DEVICES is %d\n", m_brdNum);
        return -1;
    }
    printk(KERN_ALERT "X1TIMER_PROBE : BOARD NUM %i\n", m_brdNum);
    x1timer_dev[m_brdNum].binded = 1;
    
    
    err = pci_enable_device(dev);
    if (err ) return err;

    err = pci_request_regions(dev, "x1timer");
    if (err ){
        pci_disable_device(dev);
        pci_set_drvdata(dev, NULL);
        return err;
    }
    pci_set_master(dev);

    tmp_devfn  = (u32)dev->devfn;
    busNumber  = (u32)dev->bus->number;
    devNumber  = (u32)PCI_SLOT(tmp_devfn);
    funcNumber = (u32)PCI_FUNC(tmp_devfn);
    tmp_bus_func = ((busNumber & 0xFF)<<8) + (devNumber & 0xFF);
    printk(KERN_ALERT "X1TIMER_PROBE:DEVFN %X, BUS_NUM %X, DEV_NUM %X, FUNC_NUM %X, BUS_FUNC %X\n",
                                      tmp_devfn, busNumber, devNumber, funcNumber, tmp_bus_func);
                                      
    tmp_devfn  = (u32)dev->bus->self->devfn;
    busNumber  = (u32)dev->bus->self->bus->number;
    devNumber  = (u32)PCI_SLOT(tmp_devfn);
    funcNumber = (u32)PCI_FUNC(tmp_devfn);
    tmp_bus_func = ((busNumber & 0xFF)<<8) + (devNumber & 0xFF);
    printk(KERN_ALERT "X1TIMER_PROBE:DEVFN %X, BUS_NUM %X, DEV_NUM %X, FUNC_NUM %X, BUS_FUNC %X\n",
                                      tmp_devfn, busNumber, devNumber, funcNumber, tmp_bus_func);
                                      
    pcie_cap = pci_find_capability (dev->bus->self, PCI_CAP_ID_EXP);
    printk(KERN_ALERT "X1TIMER_PROBE: PCIE SWITCH CAP address %X\n",pcie_cap);
    
    pci_read_config_dword(dev->bus->self, (pcie_cap + PCI_EXP_SLTCAP), &tmp_slot_cap);
    tmp_slot_num = (tmp_slot_cap >> 19) & 0x3f;
    x1timer_dev[m_brdNum].slot_num        = tmp_slot_num;
    brd_slot_num     = tmp_slot_num;
    printk(KERN_ALERT "X1TIMER_PROBE:SLOT NUM %d DEV NUM%d SLOT_CAP %X\n",tmp_slot_num,m_brdNum,tmp_slot_cap);
    
    tmp_slot_num = m_brdNum;  /* cresy but I do not want change all tmp_slot_num to m_brdNum*/
    x1timer_dev[tmp_slot_num].brd_num         = m_brdNum;
    x1timer_dev[tmp_slot_num].bus_func        = tmp_bus_func;
    x1timer_dev[tmp_slot_num].x1timer_pci_dev = dev;
    x1timer_dev[tmp_slot_num].x1timer_all_mems = 0;

    init_waitqueue_head(&(x1timer_dev[tmp_slot_num].waitDMA));
     
    pci_set_drvdata(dev, (&x1timer_dev[m_brdNum]));

    pcie_cap = pci_find_capability (dev, PCI_CAP_ID_EXP);
    printk(KERN_ALERT "X1TIMER_PROBE: PCIE CAP address %X\n",pcie_cap);
    pci_read_config_byte(dev, (pcie_cap + PCI_EXP_DEVCAP), &dev_payload);
    dev_payload &=0x0003;
    printk(KERN_ALERT "X1TIMER_PROBE: DEVICE CAP  %X\n",dev_payload);

    switch(dev_payload){
        case 0:
                   tmp_payload_size = 128;
                   break;
        case 1:
                   tmp_payload_size = 256;
                   break;
        case 2:
                   tmp_payload_size = 512;
                   break;
        case 3:
                   tmp_payload_size = 1024;	
                   break;
        case 4:
                   tmp_payload_size = 2048;	
                   break;
        case 5:
                   tmp_payload_size = 4096;	
                   break;
    }
    /*tmp_payload_size = 128; */
    printk(KERN_ALERT "X1TIMER_PROBE: DEVICE PAYLOAD  %d\n",tmp_payload_size);

    spin_lock_init(&x1timer_dev[tmp_slot_num].irq_lock);
    spin_lock_init(&x1timer_dev[tmp_slot_num].soft_lock);
    
    for (i = 0; i < X1TIMER_MAX_SERVER; i++) {
        x1timer_dev[tmp_slot_num].server_signal_stack [i].f_ServerPref	= 0;
        x1timer_dev[tmp_slot_num].server_signal_stack [i].f_IrqProc[0]	= 0;
        x1timer_dev[tmp_slot_num].server_signal_stack [i].f_IrqProc[1]	= 0;
        x1timer_dev[tmp_slot_num].server_signal_stack [i].f_IrqProc[2]	= 0;
        x1timer_dev[tmp_slot_num].server_signal_stack [i].f_IrqProc[3]	= 0;
        x1timer_dev[tmp_slot_num].server_signal_stack [i].f_IrqProc[4]	= 0;
    }
    x1timer_dev[tmp_slot_num].irq_status     = 0;
    x1timer_dev[tmp_slot_num].irq_mask       = 0;
    x1timer_dev[tmp_slot_num].irq_flag       = 0;
    x1timer_dev[tmp_slot_num].irq_sec        = 0;
    x1timer_dev[tmp_slot_num].irq_usec       = 0;
    x1timer_dev[tmp_slot_num].irq_status_hi  = 0;
    x1timer_dev[tmp_slot_num].irq_flag_hi    = 0;
    x1timer_dev[tmp_slot_num].irq_mask_hi    = 0;
    x1timer_dev[tmp_slot_num].softint        = 0;
    x1timer_dev[tmp_slot_num].count          = 0;
    x1timer_dev[tmp_slot_num].server_signals = X1TIMER_MAX_SERVER;
    x1timer_dev[tmp_slot_num].event_mask     = X1TIMER_IRQ_MAIN;
    x1timer_dev[tmp_slot_num].timer_offset   = 0;
    x1timer_dev[tmp_slot_num].soft_run         = 0;

    pci_read_config_word(dev, PCI_VENDOR_ID,   &vendor_id);
    pci_read_config_word(dev, PCI_DEVICE_ID,   &device_id);
    pci_read_config_word(dev, PCI_SUBSYSTEM_VENDOR_ID,   &subvendor_id);
    pci_read_config_word(dev, PCI_SUBSYSTEM_ID,   &subdevice_id);
    pci_read_config_word(dev, PCI_CLASS_DEVICE,   &class_code);
    pci_read_config_byte(dev, PCI_REVISION_ID, &revision);
    pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &irq_line);
    pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &irq_pin);
    pci_dev_irq = dev->irq;
    
    printk(KERN_ALERT "X1TIMER_PROBE: VENDOR_ID  %i\n",vendor_id);
    printk(KERN_ALERT "X1TIMER_PROBE: PCI_DEVICE_ID  %i\n",device_id);
    printk(KERN_ALERT "X1TIMER_PROBE: PCI_SUBSYSTEM_VENDOR_ID  %i\n",subvendor_id);
    printk(KERN_ALERT "X1TIMER_PROBE: PCI_SUBSYSTEM_ID  %i\n",subdevice_id);
    printk(KERN_ALERT "X1TIMER_PROBE: PCI_CLASS_DEVICE  %i\n",class_code);

    x1timer_dev[tmp_slot_num].vendor_id      = vendor_id;
    x1timer_dev[tmp_slot_num].device_id      = device_id;
    x1timer_dev[tmp_slot_num].subvendor_id   = subvendor_id;
    x1timer_dev[tmp_slot_num].subdevice_id   = subdevice_id;
    x1timer_dev[tmp_slot_num].class_code     = class_code;
    x1timer_dev[tmp_slot_num].revision       = revision;
    x1timer_dev[tmp_slot_num].irq_line       = irq_line;
    x1timer_dev[tmp_slot_num].irq_pin        = irq_pin;
    x1timer_dev[tmp_slot_num].pci_dev_irq    = pci_dev_irq;
    
    res_start  = pci_resource_start(dev, 0);
    res_end    = pci_resource_end(dev, 0);
    res_flag   = pci_resource_flags(dev, 0);
    
    x1timer_dev[tmp_slot_num].mem_base0       = res_start;
    x1timer_dev[tmp_slot_num].mem_base0_end   = res_end;
    x1timer_dev[tmp_slot_num].mem_base0_flag  = res_flag;
    
    if (res_start) {
        u32 tmp_addr_hi;
        u32 tmp_addr_low;
        if((res_end - res_start)> 0){
	    x1timer_dev[tmp_slot_num].memmory_base0 = pci_iomap(dev, 0, (res_end - res_start));
	    x1timer_dev[tmp_slot_num].rw_off = res_end - res_start;
	    x1timer_dev[tmp_slot_num].x1timer_all_mems++;
	    tmp_addr_hi  = (u32)(((u64)x1timer_dev[tmp_slot_num].memmory_base0 >> 32) & 0xFFFFFFFF);
	    tmp_addr_low = (u32)((u64)x1timer_dev[tmp_slot_num].memmory_base0  & 0xFFFFFFFF);
        }
    } else {
        x1timer_dev[tmp_slot_num].memmory_base0 = 0;
        printk(KERN_ALERT "X1TIMER_PROBE: NO BASE0 address\n");
    }
    if (!x1timer_dev[tmp_slot_num].x1timer_all_mems){
        printk(KERN_ALERT "X1TIMER_PROBE: ERROR NO BASE_MEMs\n");
        x1timer_dev[m_brdNum].binded = 0;
        pci_disable_device(dev);
        pci_set_drvdata(dev, NULL);
        return -ENOMEM;
    }

    x1timer_dev[tmp_slot_num].X1TIMER_IRQ_ENABLE          = 240; /*60 * 4;*/
    x1timer_dev[tmp_slot_num].X1TIMER_IRQ_MASK            = 276; /*69 * 4;*/
    x1timer_dev[tmp_slot_num].X1TIMER_IRQ_FLAGS           = 280; /*70 * 4;*/
    x1timer_dev[tmp_slot_num].X1TIMER_IRQ_CLR_FLAGSE      = 284; /*71 * 4;*/
    x1timer_dev[tmp_slot_num].X1TIMER_MACRO_PULSE_NUM     = 340;
    x1timer_dev[tmp_slot_num].X1TIMER_MACRO_PULSE_NUM_H   = 356;
    x1timer_dev[tmp_slot_num].X1TIMER_BOARD_ID            = 0;
    x1timer_dev[tmp_slot_num].X1TIMER_BOARD_VERSION       = 0;
    x1timer_dev[tmp_slot_num].X1TIMER_BOARD_DATE          = 0;
    x1timer_dev[tmp_slot_num].X1TIMER_BOARD_HW_VERSION    = 0;
    x1timer_dev[tmp_slot_num].X1TIMER_PROJ_1_ID           = 0;
    x1timer_dev[tmp_slot_num].X1TIMER_PROJ_1_VERSION      = 0;
    x1timer_dev[tmp_slot_num].X1TIMER_PROJ_1_DATE         = 0;
    x1timer_dev[tmp_slot_num].X1TIMER_PROJ_1_RESERVED     = 0;
    x1timer_dev[tmp_slot_num].X1TIMER_PROJ_2_ID           = 0;
    x1timer_dev[tmp_slot_num].X1TIMER_PROJ_2_VERSION      = 0;
    x1timer_dev[tmp_slot_num].X1TIMER_PROJ_2_DATE         = 0;
    x1timer_dev[tmp_slot_num].X1TIMER_PROJ_2_RESERVED     = 0;
    x1timer_dev[tmp_slot_num].X1TIMER_PROJ_3_ID           = 0;
    x1timer_dev[tmp_slot_num].X1TIMER_PROJ_3_VERSION      = 0;
    x1timer_dev[tmp_slot_num].X1TIMER_PROJ_3_DATE         = 0;
    x1timer_dev[tmp_slot_num].X1TIMER_PROJ_3_RESERVED     = 0;
    
    if(x1timer_dev[tmp_slot_num].memmory_base0) {
    
        x1timer_dev[tmp_slot_num].X1TIMER_NEW = 0;
        x1timer_dev[tmp_slot_num].X1TIMER_XN  = 1;

        /*Check Module version*/

        tmp_data = ioread32((void *)x1timer_dev[tmp_slot_num].memmory_base0 );
        
        if(tmp_data == ASCII_BOARD_MAGIC_NUM) {
        
            printk(KERN_ALERT "X1TIMER_PROBE: NEW X1TIMER VERSION\n");
            
            x1timer_dev[tmp_slot_num].X1TIMER_NEW               = 1;
            x1timer_dev[tmp_slot_num].X1TIMER_IRQ_ENABLE        = 60;
            x1timer_dev[tmp_slot_num].X1TIMER_IRQ_MASK          = 64;
            x1timer_dev[tmp_slot_num].X1TIMER_IRQ_FLAGS         = 68;
            x1timer_dev[tmp_slot_num].X1TIMER_IRQ_CLR_FLAGSE    = 72;
            x1timer_dev[tmp_slot_num].X1TIMER_MACRO_PULSE_NUM   = 82304;
            x1timer_dev[tmp_slot_num].X1TIMER_MACRO_PULSE_NUM_H = 82308;

            udelay(10);
            
            /*READ INFO*/
            x1timer_dev[tmp_slot_num].X1TIMER_BOARD_ID = ioread32((void *)x1timer_dev[tmp_slot_num].memmory_base0 + 4);
            smp_rmb();
            x1timer_dev[tmp_slot_num].X1TIMER_BOARD_VERSION = ioread32((void *)x1timer_dev[tmp_slot_num].memmory_base0 + 8);
            smp_rmb();
            x1timer_dev[tmp_slot_num].X1TIMER_BOARD_DATE  = ioread32((void *)x1timer_dev[tmp_slot_num].memmory_base0 + 12);
            smp_rmb();
            x1timer_dev[tmp_slot_num].X1TIMER_BOARD_HW_VERSION = ioread32((void *)x1timer_dev[tmp_slot_num].memmory_base0 + 16);
            smp_rmb();
            x1timer_dev[tmp_slot_num].X1TIMER_PROJ_1_ID  = ioread32((void *)x1timer_dev[tmp_slot_num].memmory_base0 + 32);
            smp_rmb();
            x1timer_dev[tmp_slot_num].X1TIMER_PROJ_1_VERSION = ioread32((void *)x1timer_dev[tmp_slot_num].memmory_base0  + 36);
            smp_rmb();
            x1timer_dev[tmp_slot_num].X1TIMER_PROJ_1_DATE  = ioread32((void *)x1timer_dev[tmp_slot_num].memmory_base0 + 40);
            smp_rmb();
            x1timer_dev[tmp_slot_num].X1TIMER_PROJ_1_RESERVED = ioread32((void *)x1timer_dev[tmp_slot_num].memmory_base0 + 44);
            smp_rmb();
            x1timer_dev[tmp_slot_num].X1TIMER_PROJ_2_ID = ioread32((void *)x1timer_dev[tmp_slot_num].memmory_base0 + 65512);
            smp_rmb();
            x1timer_dev[tmp_slot_num].X1TIMER_PROJ_2_VERSION = ioread32((void *)x1timer_dev[tmp_slot_num].memmory_base0 + 65516);
            smp_rmb();
            x1timer_dev[tmp_slot_num].X1TIMER_PROJ_2_DATE = ioread32((void *)x1timer_dev[tmp_slot_num].memmory_base0 + 65520);
            smp_rmb();
            x1timer_dev[tmp_slot_num].X1TIMER_PROJ_2_RESERVED= ioread32((void *)x1timer_dev[tmp_slot_num].memmory_base0 + 65524);
            smp_rmb();
            x1timer_dev[tmp_slot_num].X1TIMER_PROJ_3_ID = ioread32((void *)x1timer_dev[tmp_slot_num].memmory_base0 + 81924);
            smp_rmb();
            x1timer_dev[tmp_slot_num].X1TIMER_PROJ_3_VERSION = ioread32((void *)x1timer_dev[tmp_slot_num].memmory_base0 + 81928);
            smp_rmb();
            x1timer_dev[tmp_slot_num].X1TIMER_PROJ_3_DATE = ioread32((void *)x1timer_dev[tmp_slot_num].memmory_base0 + 81932);
            smp_rmb();
            x1timer_dev[tmp_slot_num].X1TIMER_PROJ_3_RESERVED= ioread32((void *)x1timer_dev[tmp_slot_num].memmory_base0 + 81936);
            smp_rmb();
            
            if (x1timer_dev[tmp_slot_num].X1TIMER_BOARD_ID == 0x0009) {

                printk(KERN_ALERT "X1TIMER_PROBE: X2TIMER VERSION\n");
            
                x1timer_dev[tmp_slot_num].X1TIMER_XN                = 2;
                x1timer_dev[tmp_slot_num].X1TIMER_NEW               = 1;
                x1timer_dev[tmp_slot_num].X1TIMER_IRQ_ENABLE        = 60;
                x1timer_dev[tmp_slot_num].X1TIMER_IRQ_MASK          = 64;
                x1timer_dev[tmp_slot_num].X1TIMER_IRQ_FLAGS         = 68;
                x1timer_dev[tmp_slot_num].X1TIMER_IRQ_CLR_FLAGSE    = 72;
                x1timer_dev[tmp_slot_num].X1TIMER_MACRO_PULSE_NUM   = 120;
                x1timer_dev[tmp_slot_num].X1TIMER_MACRO_PULSE_NUM_H = 124;
                x1timer_dev[tmp_slot_num].timer_offset              = 96;
                x1timer_dev[tmp_slot_num].global_time = 0;
                x1timer_dev[tmp_slot_num].X1TIMER_GLOBAL_SEC   = 0;
                x1timer_dev[tmp_slot_num].X1TIMER_GLOBAL_USEC = 0;
                
                tmp_data      = x1timer_dev[tmp_slot_num].X1TIMER_BOARD_VERSION;
                
                tmp_version_min      = (tmp_data >> 16) & 0xFF;
                tmp_version_maj      = (tmp_data >> 24) & 0xFF;
                if(tmp_version_maj >= 1)
                    if(tmp_version_min >= 19){
                        x1timer_dev[tmp_slot_num].global_time = 1;
                        x1timer_dev[tmp_slot_num].X1TIMER_GLOBAL_SEC   = 140;
                        x1timer_dev[tmp_slot_num].X1TIMER_GLOBAL_USEC = 136;
                    }
            }
            udelay(10);
            
        } else {
            printk(KERN_ALERT "X1TIMER_PROBE: OLD X1TIMER VERSION\n");
            /*READ INFO*/
            x1timer_dev[tmp_slot_num].X1TIMER_BOARD_ID = ioread32((void *)x1timer_dev[tmp_slot_num].memmory_base0 );
            smp_rmb();
            x1timer_dev[tmp_slot_num].X1TIMER_BOARD_VERSION = ioread32((void *)x1timer_dev[tmp_slot_num].memmory_base0 + 4);
            smp_rmb();
            x1timer_dev[tmp_slot_num].X1TIMER_BOARD_DATE = ioread32((void *)x1timer_dev[tmp_slot_num].memmory_base0 + 8);
            smp_rmb();
            x1timer_dev[tmp_slot_num].X1TIMER_BOARD_HW_VERSION = ioread32((void *)x1timer_dev[tmp_slot_num].memmory_base0 + 12);
            smp_rmb();
            udelay(10);
        }
        
        /*DISABLE INTERUPTS*/
        iowrite32(0x0,(x1timer_dev[tmp_slot_num].memmory_base0 +  x1timer_dev[tmp_slot_num].X1TIMER_IRQ_ENABLE));
        smp_wmb();
        iowrite32(0x0,(x1timer_dev[tmp_slot_num].memmory_base0 +  x1timer_dev[tmp_slot_num].X1TIMER_IRQ_MASK));
        smp_wmb();
        iowrite32(0x1,(x1timer_dev[tmp_slot_num].memmory_base0 +  x1timer_dev[tmp_slot_num].X1TIMER_IRQ_CLR_FLAGSE));
        smp_wmb();
        tmp_mask = ioread32(x1timer_dev[tmp_slot_num].memmory_base0 + x1timer_dev[tmp_slot_num].X1TIMER_IRQ_ENABLE);
        smp_rmb();
        tmp_mask = ioread32(x1timer_dev[tmp_slot_num].memmory_base0 + x1timer_dev[tmp_slot_num].X1TIMER_IRQ_MASK);
        smp_rmb();
        /*Enable DMA Interrupt and Main IRQ*/
        iowrite32(0x1,(x1timer_dev[tmp_slot_num].memmory_base0 +  x1timer_dev[tmp_slot_num].X1TIMER_IRQ_ENABLE));
        iowrite32(X1TIMER_DMA_IRQ_ENBL,(x1timer_dev[tmp_slot_num].memmory_base0 +  x1timer_dev[tmp_slot_num].X1TIMER_IRQ_MASK));
        smp_wmb();
        udelay(10);     
    }
    
    /**SETUP IRQ**/
    //x1timer_dev[tmp_slot_num].irq_flag    = IRQF_SHARED | IRQF_DISABLED;
    x1timer_dev[tmp_slot_num].irq_flag    = IRQF_SHARED ;

    #ifdef CONFIG_PCI_MSI
    
       if (pci_enable_msi(dev) == 0) {
            x1timer_dev[tmp_slot_num].msi = 1;
            x1timer_dev[tmp_slot_num].irq_flag &= ~IRQF_SHARED;
            printk(KERN_ALERT "MSI ENABLED\n");
        } else {
            x1timer_dev[tmp_slot_num].msi = 0;
            printk(KERN_ALERT "MSI NOT SUPPORTED\n");
        }
    
#endif


    x1timer_dev[tmp_slot_num].pci_dev_irq = dev->irq;
    x1timer_dev[tmp_slot_num].irq_mode    = 1;
    x1timer_dev[tmp_slot_num].irq_on         = 1;
    
    result = request_irq(x1timer_dev[tmp_slot_num].pci_dev_irq, x1timer_interrupt,
                         x1timer_dev[tmp_slot_num].irq_flag, "x1timer", &x1timer_dev[tmp_slot_num]);
    if (result) {
        printk(KERN_ALERT "x1timer_PROBE: can't get assigned irq %i\n", x1timer_dev[tmp_slot_num].pci_dev_irq);
        x1timer_dev[tmp_slot_num].irq_mode = 0;
    }else{
        printk(KERN_ALERT "x1timer_PROBE:  assigned IRQ %i RESULT %i\n",
               x1timer_dev[tmp_slot_num].pci_dev_irq, result);
    }
    
    if(x1timer_dev[tmp_slot_num].memmory_base0){
        iowrite32(0x1,(x1timer_dev[tmp_slot_num].memmory_base0 +  x1timer_dev[tmp_slot_num].X1TIMER_IRQ_CLR_FLAGSE));
        smp_wmb();
        iowrite32(0x0,(x1timer_dev[tmp_slot_num].memmory_base0 +  x1timer_dev[tmp_slot_num].X1TIMER_IRQ_CLR_FLAGSE));
        smp_wmb();
    }
    udelay(10);

    if(x1timer_dev[tmp_slot_num].X1TIMER_XN == 2) {

        device_create(x1timer_class, NULL, MKDEV(X1TIMER_MAJOR, (X1TIMER_MINOR + tmp_slot_num)),
                            &x1timer_dev[tmp_slot_num].x1timer_pci_dev->dev, "x2timers%d", brd_slot_num );

    }else{

        device_create(x1timer_class, NULL, MKDEV(X1TIMER_MAJOR, (X1TIMER_MINOR + tmp_slot_num) ),
                            &x1timer_dev[tmp_slot_num].x1timer_pci_dev->dev, "x1timers%d", brd_slot_num );
    }
    
    register_x1timer_proc(brd_slot_num, &x1timer_dev[tmp_slot_num]);
    x1timer_dev[tmp_slot_num].dev_sts   = 1;
    x1timer_dev[tmp_slot_num].dev_err_detected = 0;

	////////////////////////////////////
	/// New added code              ////
	////////////////////////////////////
#ifdef _NEW_ADDED_CODE
	//#define	_ENTRY_NAME_WAITERS_	"timer_waiters_for"
	//#define	_ENTRY_NAME_GEN_EVENT_	"gen_event_entry_name"
	init_waitqueue_head(&x1timer_dev[tmp_slot_num].irqWaiterStruct.waitIRQ);
	x1timer_dev[tmp_slot_num].irqWaiterStruct.numberOfIRQs = 0;
	x1timer_dev[tmp_slot_num].irqWaiterStruct.isIrqActive = 1;
	x1timer_dev[tmp_slot_num].added_event = 0;
	x1timer_dev[tmp_slot_num].added_wait_irq = 0;
	if (AddNewEntryToGlobalContainer(&(x1timer_dev[tmp_slot_num].gen_event), X2TIMER_GENEVNT) == 0){ x1timer_dev[tmp_slot_num].added_event = 1; }
	if (AddNewEntryToGlobalContainer(&(x1timer_dev[tmp_slot_num].irqWaiterStruct), X2TIMER_WAITERS) == 0){ x1timer_dev[tmp_slot_num].added_wait_irq = 1; }
#endif // #ifdef _NEW_ADDED_CODE
	////////////////////////////////////
	/// End of New added code       ////
	////////////////////////////////////

    x1timerModuleNum++;

    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
    static void x1timer_remove(struct pci_dev *dev)
#else 
    static void __devexit x1timer_remove(struct pci_dev *dev)
#endif

{
    struct x1timer_dev     *x1timerdev;
     int                    tmp_dev_num  = 0;
     int                    tmp_slot_num = 0;
     int                    tmp_brd_num = 0;
     unsigned long          flags        = 0;
     u32                    tmp_mask     = 0;
     int                    tmp_sts      = 0;
     
     x1timer_file_list *tmp_file_list;
     struct list_head *fpos, *q;
     
     printk(KERN_ALERT "X1TIMER_REMOVE CALLED\n");

     /* clean up any allocated resources and stuff here.
      * like call release_region();
      */
    x1timerdev = pci_get_drvdata(dev);

	////////////////////////////////////
	/// New added code              ////
	////////////////////////////////////
#ifdef _NEW_ADDED_CODE
	if (x1timerdev && x1timerdev->added_wait_irq)RemoveEntryFromGlobalContainer(X2TIMER_WAITERS);
	if (x1timerdev && x1timerdev->added_event)RemoveEntryFromGlobalContainer(X2TIMER_GENEVNT);
#endif  // #ifdef _NEW_ADDED_CODE
	////////////////////////////////////
	/// End of New added code       ////
	////////////////////////////////////

    tmp_dev_num  = x1timerdev->dev_num;
    tmp_slot_num = x1timerdev->slot_num;
    tmp_brd_num = x1timerdev->brd_num;
    printk(KERN_ALERT "X1TIMER_REMOVE: SLOT %d DEV %d\n", tmp_slot_num, tmp_brd_num);
    if(x1timerdev->dev_sts){
        tmp_sts               = 1;
        x1timerdev->dev_sts   = 0;
    }
    /*DISABLING INTERRUPTS ON THE MODULE*/
    spin_lock_irqsave(&x1timerdev->irq_lock, flags);
        printk(KERN_ALERT "X1TIMER_REMOVE: DISABLING INTERRUPTS ON THE MODULE\n");
        if(!(x1timerdev->dev_err_detected)){
            iowrite32(0x0,(x1timerdev->memmory_base0 +  x1timerdev->X1TIMER_IRQ_ENABLE));
            smp_wmb();
            udelay(10);
            iowrite32(0x0,(x1timerdev->memmory_base0 +  x1timerdev->X1TIMER_IRQ_MASK));
            smp_wmb();
            udelay(10);
            tmp_mask = ioread32(x1timerdev->memmory_base0 + x1timerdev->X1TIMER_IRQ_ENABLE);
            smp_rmb();
            tmp_mask = ioread32(x1timerdev->memmory_base0 + x1timerdev->X1TIMER_IRQ_MASK);
            smp_rmb();
            udelay(10);
        }
    spin_unlock_irqrestore(&x1timerdev->irq_lock, flags);
    
    printk(KERN_ALERT "X1TIMER_REMOVE: flush_workqueue\n");
    cancel_work_sync(&(x1timerdev->x1timerp_work));
    flush_workqueue(x1timerp_workqueue);
    printk(KERN_ALERT "X1TIMER_REMOVE:REMOVING IRQ_MODE %d\n", x1timerdev->irq_mode);
    if(x1timerdev->irq_mode){
        printk(KERN_ALERT "X1TIMER_REMOVE:FREE IRQ\n");
        free_irq(x1timerdev->pci_dev_irq, x1timerdev);
        printk(KERN_ALERT "REMOVING IRQ\n");
    }
    
    if(x1timerdev->msi){
       printk(KERN_ALERT "DISABLE MSI\n");
       pci_disable_msi((x1timerdev->x1timer_pci_dev));
    }

    printk(KERN_ALERT "X1TIMER_REMOVE: UNMAPPING MEMORYs\n");
    mutex_lock(&x1timerdev->dev_mut);
        printk(KERN_ALERT "X1TIMER_REMOVE: UNMAPPING MEMORYs MUTEX LOCKED\n");
        if(x1timerdev->memmory_base0){
           pci_iounmap(dev, x1timerdev->memmory_base0);
           x1timerdev->memmory_base0  = 0;
           x1timerdev->mem_base0      = 0;
           x1timerdev->mem_base0_end  = 0;
           x1timerdev->mem_base0_flag = 0;
           x1timerdev->rw_off         = 0;
        }
        pci_release_regions((x1timerdev->x1timer_pci_dev));
    mutex_unlock(&x1timerdev->dev_mut);

    if(tmp_sts){
        printk(KERN_ALERT "X1TIMER_REMOVE: DESTROY DEVICE\n");
        x1timerdev->dev_sts   = 0;
        device_destroy(x1timer_class,  MKDEV(X1TIMER_MAJOR, (X1TIMER_MINOR + tmp_brd_num)));
    }

    unregister_x1timer_proc(tmp_slot_num);
    x1timerdev->dev_sts   = 0;
    x1timerdev->binded   = 0;
    x1timerModuleNum --;
        
    list_for_each_safe(fpos, q, &(x1timerdev->dev_file_list.node_file_list)){
             tmp_file_list = list_entry(fpos, x1timer_file_list, node_file_list);
             tmp_file_list->filp->private_data  = &x1timer_dev[X1TIMER_NR_DEVS]; 
             list_del(fpos);
             kfree (tmp_file_list);
             x1timer_dev[X1TIMER_NR_DEVS].f_cntl++;
    }
    
    
    
    pci_disable_device(dev);
    udelay(10);
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
    static struct pci_driver pci_x1timer_driver = {
    .name        = "x1timer",
    .id_table    = x1timer_ids,
    .probe       = x1timer_probe,
    .remove     = x1timer_remove,
};
#else 
    static struct pci_driver pci_x1timer_driver = {
    .name        = "x1timer",
    .id_table    = x1timer_ids,
    .probe       = x1timer_probe,
    .remove      = __devexit_p(x1timer_remove),
};
#endif


#if LINUX_VERSION_CODE < 132632
    static void x1timerp_do_work(void *x1timerdev)
#else
    static void x1timerp_do_work(struct work_struct *work_str)
#endif
{
    unsigned long flags;
    u32 tmp_mask = 0;
    u32 tmp_flag = 0;
    u32 tmp_sec;
    u32 tmp_usec;
    u32 tmp_event_h = 0;
    u32 tmp_event_l = 0;
    u32 status   = 0;
    u32 gen_mask = 0;
    //u32 tmp_data  = 0;
    int i        = 0;
    t_x1timer_ServerSignal   *ss;
    u16 minor;
    void *address;
    long evnum;
    long tmpwd;

    #if LINUX_VERSION_CODE < 132632
    struct x1timer_dev *dev   = x1timerdev;
    #else
    struct x1timer_dev *dev   =  container_of(work_str, struct x1timer_dev, x1timerp_work);
    #endif

    //printk(KERN_ALERT "X9TIMER_DO_WORK\n");
    if (mutex_lock_interruptible(&dev->dev_mut)) return ;

    if(!(dev->dev_sts)){
        printk(KERN_ALERT "#####X9TIMER_DO_WORK DEV_STS 0\n");
        mutex_unlock(&dev->dev_mut);
        return;
    }
    address = (void *) dev->memmory_base0 ;
    //spin_lock_irqsave(&dev->irq_lock, flags);
    spin_lock_irqsave(&dev->soft_lock, flags);
        tmp_mask = dev->irq_mask_hi;
        tmp_flag = dev->irq_flag_hi;
        tmp_sec  = dev->irq_sec_hi;
        tmp_usec = dev->irq_usec_hi;
        dev->soft_run = 1;
   // spin_unlock_irqrestore(&dev->irq_lock, flags);
    spin_unlock_irqrestore(&dev->soft_lock, flags);
    minor = dev->dev_minor;
    dev->irq_flag    = tmp_flag;
    dev->irq_status  = (tmp_flag & tmp_mask);
    status           = (tmp_flag & tmp_mask);
    dev->irq_sec     = tmp_sec;
    dev->irq_usec    = tmp_usec;  
    /*if (status & dev->event_mask) {*/
    if (tmp_flag & dev->event_mask) {
        if (dev->timer_offset) {
            u32   wd;
            /* read abs. time */
            wd = ioread32 (address + dev->timer_offset + 8); /* drop the word */
            smp_rmb ();
            wd = ioread32 (address + dev->timer_offset + 12); /* drop the word */
            smp_rmb ();
            dev->board_usec = ioread32 (address + dev->timer_offset + 8);
            smp_rmb ();
            dev->board_sec  = ioread32 (address + dev->timer_offset + 12);
            smp_rmb ();
            /* read macro pulse number */
            wd = ioread32 (address + dev->timer_offset + 24); /* drop the word */
            smp_rmb ();
            wd = ioread32 (address + dev->timer_offset + 28); /* drop the word */
            smp_rmb ();
            tmp_event_l = ioread32 (address + dev->timer_offset + 24);
            smp_rmb ();
            tmp_event_h = ioread32 (address + dev->timer_offset + 28);
            smp_rmb ();

        }else{
            /* read macro pulse number */
            tmp_event_l = ioread32 (address + dev->X1TIMER_MACRO_PULSE_NUM);
            smp_rmb ();
            tmp_event_h = ioread32 (address + dev->X1TIMER_MACRO_PULSE_NUM_H);
            smp_rmb ();
        }
        tmpwd = tmp_event_l;
        evnum = tmp_event_h;
        evnum = (evnum << 32) | (tmpwd & 0x0ffffffff);
        dev->gen_event = evnum;
        dev->irq_delay = (tmp_sec - dev->irq_sec)*1000000L + (tmp_usec - dev->irq_usec);
        
        if(dev->global_time){
            tmp_event_l = ioread32 (address + dev->X1TIMER_GLOBAL_SEC);
            smp_rmb ();
            tmp_event_h = ioread32 (address + dev->X1TIMER_GLOBAL_USEC);
            smp_rmb ();
            dev->global_sec    = tmp_event_l;
            dev->global_usec  = tmp_event_h;
        }else{
            dev->global_sec    = 0;
            dev->global_usec  = 0;
        }
    }

	////////////////////////////////////
	/// New added code              ////
	////////////////////////////////////
#ifdef _NEW_ADDED_CODE
	if (dev->gen_event_old != dev->gen_event) // this is not necessary anymore, but I kkep it
												// because there is no check if really this device 
												// makes interrupt, or other device, that shares same interrupt line
	{
		++dev->irqWaiterStruct.numberOfIRQs;
		//printk(KERN_ALERT "!!!!!!!!!!!!!!!!   line=%d, irqs=%d\n", __LINE__, (int)dev->irqWaiterStruct.numberOfIRQs);
		wake_up(&dev->irqWaiterStruct.waitIRQ);
		//goto exitNewCode;

		{
			static int snTest = 0;
			if ((snTest++ % 86400)==0)printk(KERN_ALERT "!!!!!!!!!!!!!!!! iter=%d, event=%ld\n", snTest, dev->gen_event);
		}

		dev->gen_event_old = dev->gen_event;
	}
//exitNewCode:
#endif  // #ifdef _NEW_ADDED_CODE
	////////////////////////////////////
	/// End of New added code       ////
	////////////////////////////////////

    /* send signals to registered processes */
    for (i = 0; i < dev->server_signals; i++) {
        ss = &dev->server_signal_stack [i];
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
        gen_mask |= dev->server_signal_stack [i].f_IrqProc[0];
        gen_mask |= dev->server_signal_stack [i].f_IrqProc[1];
        gen_mask |= dev->server_signal_stack [i].f_IrqProc[2];
        gen_mask |= dev->server_signal_stack [i].f_IrqProc[3];
        gen_mask |= dev->server_signal_stack [i].f_IrqProc[4];
        /*Enable DMA Interrupt*/
        gen_mask |= X1TIMER_DMA_IRQ_ENBL;
    }
    dev->irq_mask = gen_mask;
    iowrite32(gen_mask,(address +  dev->X1TIMER_IRQ_MASK));
    smp_wmb();
/*
        iowrite32(0x1,(address +  dev->X1TIMER_IRQ_CLR_FLAGSE));
        smp_wmb();
        //udelay(3);
        tmp_data = ioread32 (address + dev->X1TIMER_IRQ_CLR_FLAGSE); 
        smp_rmb ();
        iowrite32(0x0,(address +  dev->X1TIMER_IRQ_CLR_FLAGSE));
        smp_wmb();
        //udelay(3);
        tmp_data = ioread32 (address + dev->X1TIMER_IRQ_CLR_FLAGSE); 
        smp_rmb ();
        udelay(3);
        iowrite32(0x1,(address +  dev->X1TIMER_IRQ_ENABLE));
        smp_wmb();
        udelay(10);
*/
/*
    iowrite32(0x0,((void *)dev->memmory_base0 +  dev->X1TIMER_IRQ_CLR_FLAGSE));
    iowrite32(0x1,(dev->memmory_base0 +  dev->X1TIMER_IRQ_ENABLE));
*/
    mutex_unlock(&dev->dev_mut);
    //spin_lock_irqsave(&dev->irq_lock, flags);
    spin_lock_irqsave(&dev->soft_lock, flags);
        dev->soft_run = 0;
    //spin_unlock_irqrestore(&dev->irq_lock, flags);
    spin_unlock_irqrestore(&dev->soft_lock, flags);
}

/*
 * The top-half interrupt handler.
 */
#if LINUX_VERSION_CODE < 0x20613 // irq_handler_t has changed in 2.6.19
static irqreturn_t x1timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
#else
static irqreturn_t x1timer_interrupt(int irq, void *dev_id)
#endif
{
    struct timeval     tv;
    unsigned long    flags;
    int                      tmp_dev_num = 33;
    u32                    tmp_mask       = 0;
    u32                    tmp_flag         = 0;
    u32                    tmp_data        = 0;
    int                      tmp_soft_run  = 0;

    struct x1timer_dev * dev = dev_id;
    tmp_dev_num = dev->dev_num;

    if(dev->vendor_id  != X1TIMER_VENDOR_ID)   return IRQ_NONE;
    if(dev->device_id  != X1TIMER_DEVICE_ID)   return IRQ_NONE;

    do_gettimeofday(&tv);
    if(!dev->dev_sts){
        printk(KERN_ALERT "##############X1TIMER_INTERRUPT: NO DEVICE \n");
        return IRQ_NONE;
    }
    /* Remember the time, and farm off the rest to the workqueue function */
    if(dev->memmory_base0) {
         disable_irq_nosync(dev->pci_dev_irq);
         spin_lock_irqsave(&dev->irq_lock, flags);
            udelay(3);
            tmp_flag   = ioread32((void *)dev->memmory_base0 + dev->X1TIMER_IRQ_FLAGS);
            tmp_mask = ioread32((void *)dev->memmory_base0 + dev->X1TIMER_IRQ_MASK);
            smp_rmb();
/*
            iowrite32(0x0,((void *)dev->memmory_base0 +  dev->X1TIMER_IRQ_MASK));
            smp_wmb();
*/
            udelay(3);
            iowrite32(0x1,((void *)dev->memmory_base0 +  dev->X1TIMER_IRQ_CLR_FLAGSE));
            smp_wmb();
            tmp_data = ioread32 (dev->memmory_base0 +  dev->X1TIMER_IRQ_CLR_FLAGSE); 
            smp_rmb();
            if(!tmp_data){
               // printk(KERN_ALERT "##############X1TIMER_INTERRUPT: CLEAR NOT 1 %X \n", tmp_data);
                iowrite32(0x1,((void *)dev->memmory_base0 +  dev->X1TIMER_IRQ_CLR_FLAGSE));
                smp_wmb();
            }
            //udelay(3);
            iowrite32(0x0,((void *)dev->memmory_base0 +  dev->X1TIMER_IRQ_CLR_FLAGSE));
            smp_wmb();
            tmp_data = ioread32 (dev->memmory_base0 +  dev->X1TIMER_IRQ_CLR_FLAGSE); 
            smp_rmb();
            if(tmp_data){
                //printk(KERN_ALERT "##############X1TIMER_INTERRUPT: CLEAR NOT 0 \n");
                iowrite32(0x0,((void *)dev->memmory_base0 +  dev->X1TIMER_IRQ_CLR_FLAGSE));
                smp_wmb();
            }
/*
            udelay(5);
             iowrite32(tmp_mask,((void *)dev->memmory_base0 +  dev->X1TIMER_IRQ_MASK));
            smp_wmb();
*/
/*
            iowrite32(0x0,(dev->memmory_base0 +  dev->X1TIMER_IRQ_ENABLE));
            smp_wmb();
            tmp_data = ioread32 (dev->memmory_base0 +  dev->X1TIMER_IRQ_ENABLE); 
            smp_rmb ();
            iowrite32(0x1,(dev->memmory_base0 +  dev->X1TIMER_IRQ_ENABLE));
            smp_wmb();
            tmp_data = ioread32 (dev->memmory_base0 +  dev->X1TIMER_IRQ_ENABLE); 
            smp_rmb ();
*/
         spin_unlock_irqrestore(&dev->irq_lock, flags);
         enable_irq(dev->pci_dev_irq);
        if(tmp_flag & X1TIMER_DMA_IRQ_MASK){
            //printk(KERN_ALERT "##############X1TIMER_INTERRUPT: DMA\n");
            dev->waitFlag = 1;
            wake_up_interruptible(&(dev->waitDMA));
            return IRQ_HANDLED;
        }
        if (dev->timer_offset) {
            void       *address;
            address = (void *) dev->memmory_base0;
            /* latch macro pulse number and absolute time in registers */
            iowrite32 (1, address + dev->timer_offset + 32); /* latch abs. time */
            smp_wmb ();
            iowrite32 (1, address + dev->timer_offset + 36); /* latch macro pulse number */
            smp_wmb ();
            ioread32 (address + dev->timer_offset + 32);
            smp_rmb ();
            ioread32 (address + dev->timer_offset + 36);
            smp_rmb ();
        }
        udelay(5);
        if(!tmp_flag){
            printk(KERN_ALERT "________X1TIMER_INTERRUPT IRQ_NONE\n");
            return IRQ_HANDLED;
        }
        //spin_lock(&dev->irq_lock);
        spin_lock(&dev->soft_lock);
            dev->irq_mask_hi   = tmp_mask;
            dev->irq_flag_hi     = tmp_flag;
            dev->irq_sec_hi      = tv.tv_sec;
            dev->irq_usec_hi    = tv.tv_usec;
            dev->irq_status_hi  = (tmp_flag & tmp_mask);
            tmp_soft_run          = dev->soft_run;   
        //spin_unlock(&dev->irq_lock);
        spin_unlock(&dev->soft_lock);
}

	////////////////////////////////////
	/// New added code              ////
	////////////////////////////////////
#ifdef _NEW_ADDED_CODE
	//++dev->irqWaiterStruct.numberOfIRQs;
	//wake_up(&dev->waitDMA);
#endif  // #ifdef _NEW_ADDED_CODE
	////////////////////////////////////
	/// End of New added code       ////
	////////////////////////////////////
    
    if(!tmp_soft_run)
        queue_work(x1timerp_workqueue, &(dev->x1timerp_work));
    return IRQ_HANDLED;
}

static int x1timer_open( struct inode *inode, struct file *filp )
{
    int    minor;
    struct x1timer_dev *dev; 
    x1timer_file_list *tmp_file_list;
    
    minor = iminor(inode);
    dev = container_of(inode->i_cdev, struct x1timer_dev, cdev);
    
    
        dev->f_cntl++;
        filp->private_data = dev; 

        tmp_file_list = kzalloc(sizeof(x1timer_file_list), GFP_KERNEL);
        INIT_LIST_HEAD(&tmp_file_list->node_file_list); 
        tmp_file_list->file_cnt = dev->f_cntl;
        tmp_file_list->filp = filp;
        list_add(&(tmp_file_list->node_file_list), &(dev->dev_file_list.node_file_list));
    mutex_unlock(&dev->dev_mut);
    return 0;
}

static int x1timer_release(struct inode *inode, struct file *filp)
{
    u16 cur_proc     = 0;
    x1timer_file_list *tmp_file_list;
    struct list_head *pos, *q;
    
    
    struct x1timer_dev *dev   = filp->private_data;
    mutex_lock(&dev->dev_mut);
        dev->f_cntl--;
        cur_proc = current->group_leader->pid;
        list_for_each_safe(pos, q, &(dev->dev_file_list.node_file_list)){
            tmp_file_list = list_entry(pos, x1timer_file_list, node_file_list);
            if(tmp_file_list->filp ==filp){
                list_del(pos);
                kfree (tmp_file_list);
            }
        }
    mutex_unlock(&dev->dev_mut);    
    return 0;
} 

static ssize_t x1timer_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    u64        itemsize   = 0; 
    ssize_t    retval     = 0;
    int        minor      = 0;
    int        d_num      = 0;
    int        tmp_offset = 0;
    int        tmp_mode   = 0;
    int        tmp_barx   = 0;
    u8         tmp_data_8;
    u16        tmp_data_16;
    u32        tmp_data_32;
    device_rw  reading;
    u32        mem_tmp = 0;
    void       *address;

    struct x1timer_dev *dev = filp->private_data;
    
/*
    if (mutex_lock_interruptible(&dev->dev_mut)){
        return -ERESTARTSYS;
    }
*/
    mutex_lock(&dev->dev_mut);
    minor = dev->dev_minor;
    d_num = dev->dev_num;

    //printk(KERN_ALERT "X1TIMER_READ: BRD_NUM %i SLOT_NUM %i\n", dev->brd_num, dev->slot_num);
    
    if(!dev->dev_sts){
        printk(KERN_ALERT "X1TIMER_READ: NO DEVICE  BRD/SLOT %di/%in", dev->brd_num, dev->slot_num);
        retval = -EIO;
        mutex_unlock(&dev->dev_mut);
        return retval;
    }
    itemsize = sizeof(device_rw);
    if (copy_from_user(&reading, buf, count)) {
        retval = -EFAULT;
        mutex_unlock(&dev->dev_mut);
        return retval;
    }
    tmp_mode     = reading.mode_rw; 
    tmp_offset   = reading.offset_rw;
    tmp_barx     = reading.barx_rw;

    switch(tmp_barx){
	   case 0:
	        if (!dev->memmory_base0){
	            retval = -EFAULT;
	            mutex_unlock(&dev->dev_mut);
	            return retval;
         }
	        address    = (void *) dev->memmory_base0;
	        mem_tmp = (dev->mem_base0_end -2);
	        break;

	   default:
	        if (!dev->memmory_base0){
	            retval = -EFAULT;
	            mutex_unlock(&dev->dev_mut);
	            return retval;
         }
         address = (void *)dev->memmory_base0;
         mem_tmp = (dev->mem_base0_end -2);
         break;
    }
    if(tmp_offset > (mem_tmp -2) || (!address)){
          reading.data_rw   = 0;
          retval            = 0;
    }else{
          switch(tmp_mode){
            case RW_D8:
                tmp_data_8        = ioread8(address + tmp_offset);
                smp_rmb();
                reading.data_rw   = tmp_data_8 & 0xFF;
                retval = itemsize;
                break;
            case RW_D16:
                 if ((tmp_offset % 2)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                tmp_data_16       = ioread16(address + tmp_offset);
                smp_rmb();
                reading.data_rw   = tmp_data_16 & 0xFFFF;
                retval = itemsize;
                break;
            case RW_D32:
                 if ((tmp_offset % 4)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                tmp_data_32       = ioread32(address + tmp_offset);
                smp_rmb();
                reading.data_rw   = tmp_data_32 & 0xFFFFFFFF;
                retval = itemsize;
                break;
            default:
                 if ((tmp_offset % 2)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                tmp_data_16       = ioread16(address + tmp_offset);
                smp_rmb();
                reading.data_rw   = tmp_data_16 & 0xFFFF;
                retval = itemsize;
                break;
          }	
    }
    udelay(5);
    if(reading.data_rw == 0xFFFFFFFF){
        tmp_data_32       = ioread32(address );
        smp_rmb();
        tmp_data_32   = tmp_data_32 & 0xFFFFFFFF;
        if(tmp_data_32 == 0xFFFFFFFF){
            retval = -EFAULT;
            mutex_unlock(&dev->dev_mut);
            return retval;
        }
    }
    if (copy_to_user(buf, &reading, count)) {
         retval = -EFAULT;
         mutex_unlock(&dev->dev_mut);
         retval = 0;
         return retval;
    }
    mutex_unlock(&dev->dev_mut);
    return retval;
}

static ssize_t x1timer_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) 
{
    device_rw       reading;
    int             itemsize   = 0;
    ssize_t         retval     = 0;
    int             minor      = 0;
    int             d_num      = 0;
    u16             tmp_data_8;
    u16             tmp_data_16;
    u32             tmp_data_32;
    int             tmp_offset = 0;
    int             tmp_mode   = 0;
    int             tmp_barx   = 0;
    u32             mem_tmp    = 0;
    void            *address ;
       
    struct x1timer_dev       *dev = filp->private_data;

/*
    if (mutex_lock_interruptible(&dev->dev_mut)) {
        return -ERESTARTSYS;
    }
*/
     mutex_lock(&dev->dev_mut);
    minor = dev->dev_minor;
    d_num = dev->dev_num;

    if(!dev->dev_sts){
        retval = -EIO;
        mutex_unlock(&dev->dev_mut);
        return retval;
    }
    itemsize = sizeof(device_rw);
    if (copy_from_user(&reading, buf, count)) {
        retval = -EFAULT;
        mutex_unlock(&dev->dev_mut);
        return retval;
    }
    tmp_mode     = reading.mode_rw;
    tmp_offset   = reading.offset_rw;
    tmp_barx     = reading.barx_rw;
    tmp_data_32  = reading.data_rw & 0xFFFFFFFF;
    
    
    if(tmp_offset == 0x3C){
        printk(KERN_ALERT "WRITING TO WORD_IRQ_ENABLE REGISTER %X process %i\n", tmp_data_32, current->group_leader->pid);
    }
    
    switch(tmp_barx){
        case 0:
            if(!dev->memmory_base0){
                printk(KERN_ALERT "NO MEM UNDER BAR0 \n");
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            address    = (void *)dev->memmory_base0;
            mem_tmp = (dev->mem_base0_end -2);
            break;
        default:
            if(!dev->memmory_base0){
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            address    = (void *)dev->memmory_base0;
            mem_tmp = (dev->mem_base0_end -2);
            break;
    }
    if(tmp_offset > (mem_tmp -2) || (!address )){
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
                if ((tmp_offset % 2)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                tmp_data_16 = tmp_data_32 & 0xFFFF;
                iowrite16(tmp_data_16, ((void*)(address + tmp_offset)));
                smp_wmb();
                retval = itemsize;
                break;
            case RW_D32:
                 if ((tmp_offset % 4)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                iowrite32(tmp_data_32, ((void*)(address + tmp_offset)));
                smp_wmb();
                retval = itemsize;                
                break;
            default:
                
                if ((tmp_offset % 2)) {
                    retval = -EFAULT;
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
                tmp_data_16 = tmp_data_32 & 0xFFFF;
                iowrite16(tmp_data_16, ((void*)(address + tmp_offset)));
                smp_wmb();
                retval = itemsize;
                break;
        }
    }
    udelay(10);
    mutex_unlock(&dev->dev_mut);
    return retval;
}

//static int  x1timer_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
static long  x1timer_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int                   i        = 0;
    pid_t                 cur_proc = 0;
    int                   minor    = 0;
    int                   d_num    = 0;
    int                   retval   = 0;
    int                   err      = 0;
    u32                   new_mask;
    u32                   gen_mask = 0;
    u32                   tmp_data_32 ;
    struct pci_dev*       pdev;
    device_ioctrl_data    data;
    t_x1timer_ioctl_data  irq_data;
    t_x1timer_ioctl_event event_data;
    t_x1timer_ioctl_event_ht event_data_ht;
    struct timeval     tv;
    int                   tmp_free_pages = 0;
    void                 *address     = 0;
    u_int	    tmp_dma_size;
    u_int	    tmp_dma_trns_size;
    u_int	    tmp_dma_offset;
    void*           pWriteBuf           = 0;
    void*          dma_reg_address;
    int             tmp_order           = 0;
    unsigned long   length              = 0;
    dma_addr_t      pTmpDmaHandle;
    u32             dma_sys_addr ;
    int               tmp_source_address  = 0;
/*
    u_int           tmp_dma_control_reg = 0;
    u_int           tmp_dma_len_reg     = 0;
    u_int           tmp_dma_src_reg     = 0;
    u_int           tmp_dma_dest_reg    = 0;
*/
    u_int           tmp_dma_cmd         = 0;
    long            timeDMAwait;
    u32             tmp_dma_done ;
     ulong           value;
     int tmp_dma_time_cnt = 0;
    
    int size_time;
    int io_dma_size;
    device_ioctrl_time  time_data;
    device_ioctrl_dma   dma_data;

    int size_data ;
    int irq_size;
    int event_size;
    int event_size_ht;
   

    struct x1timer_dev       *dev      = filp->private_data;
    
    //if (mutex_lock_interruptible(&dev->dev_mut)) return -ERESTARTSYS;
   mutex_lock(&dev->dev_mut);
    //spin_lock_irqsave(&dev->soft_lock, sfflags);
     //printk(KERN_ALERT "X1TIMER_IOCTL:\n");

    size_time      = sizeof (device_ioctrl_time);
    io_dma_size = sizeof(device_ioctrl_dma);
    
    size_data        = sizeof (device_ioctrl_data);
    irq_size          = sizeof (t_x1timer_ioctl_data);
    event_size      = sizeof (t_x1timer_ioctl_event);
    event_size_ht = sizeof (t_x1timer_ioctl_event_ht);
    minor    = dev->dev_minor;
    d_num    = dev->dev_num;
    cur_proc = current->group_leader->pid;
    pdev     = (dev->x1timer_pci_dev);
    address  = (void *) dev->memmory_base0 ;

    //printk(KERN_ALERT "X1TIMER_IOCTL: DEVICE %d\n", dev->dev_num);
    if(!dev->dev_sts){
       printk(KERN_ALERT "X1TIMER_IOCTL: NO DEVICE %d\n", dev->dev_num);
       retval = -EIO;
       mutex_unlock(&dev->dev_mut);
       return retval;
    }

    /*
    * extract the type and number bitfields, and don't decode
    * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
    */
    if (_IOC_TYPE(cmd) != X1TIMER_IOC) {
        mutex_unlock(&dev->dev_mut);
        return -ENOTTY;
    }
    if (_IOC_NR(cmd) > X1TIMER_IOC_MAXNR) {
         mutex_unlock(&dev->dev_mut);
        return -ENOTTY;
    }
    if (_IOC_NR(cmd) < X1TIMER_IOC_MINNR) {
        mutex_unlock(&dev->dev_mut);
        return -ENOTTY;
    }

    /*
    * the direction is a bitmask, and VERIFY_WRITE catches R/W
    * transfers. `Type' is user-oriented, while
    * access_ok is kernel-oriented, so the concept of "read" and
    * "write" is reversed
    */
    if (_IOC_DIR(cmd) & _IOC_READ)
         err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
         err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
    if (err) {
        mutex_unlock(&dev->dev_mut);
        return -EFAULT;
    }

    switch (cmd) {
        case X1TIMER_PHYSICAL_SLOT:
            retval = 0;
            if (copy_from_user(&data, (device_ioctrl_data*)arg, (size_t)size_data)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            data.data    = dev->slot_num;
            data.cmd     = X1TIMER_PHYSICAL_SLOT;
            if (copy_to_user((device_ioctrl_data*)arg, &data, (size_t)size_data)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            break;
        case X1TIMER_DRIVER_VERSION:
            retval = 0;
            data.data   = X1TIMER_DRV_VER_MAJ;
            data.offset = X1TIMER_DRV_VER_MIN;
            if (copy_to_user((device_ioctrl_data*)arg, &data, (size_t)size_data)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            break;
        case X1TIMER_FIRMWARE_VERSION:
            retval = 0;
            data.data   = X1TIMER_DRV_VER_MAJ + 1;
            data.offset = X1TIMER_DRV_VER_MIN + 1;
            if (copy_to_user((device_ioctrl_data*)arg, &data, (size_t)size_data)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            break;
        case X1TIMER_ADD_SERVER_SIGNAL:/* t_iptimer_ioctl_data data******/
            if (copy_from_user(&irq_data, (t_x1timer_ioctl_data*)arg, (size_t)irq_size)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            if (!dev->irq_on){
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            if (irq_data.f_Irq_Sig > 4) {
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
            
            
            switch(irq_data.f_Irq_Sig){
                case SIG1:
                case SIG2:
                case URG:
                case MSK_WANT:
                    dev->server_signal_stack [i].f_IrqProc[irq_data.f_Irq_Sig] |= irq_data.f_Irq_Mask;
                    
                    tmp_data_32 = ioread32(address + dev->X1TIMER_IRQ_MASK);
                    smp_rmb();
                    tmp_data_32 |= irq_data.f_Irq_Mask;
                    iowrite32(tmp_data_32,  (address + dev->X1TIMER_IRQ_MASK));
                    smp_wmb();
                    dev->irq_mask |= irq_data.f_Irq_Mask;
                    udelay(10);
                    break;
                case CUR_SIG:
                    dev->server_signal_stack [i].f_IrqProc[irq_data.f_Irq_Sig] |= irq_data.f_Irq_Mask;
                    dev->server_signal_stack [i].f_CurSig = irq_data.f_Status;
                    
                    tmp_data_32 = ioread32(address + dev->X1TIMER_IRQ_MASK);
                    smp_rmb();
                    tmp_data_32             |= irq_data.f_Irq_Mask;
                    iowrite32(tmp_data_32,  (address + dev->X1TIMER_IRQ_MASK));
                    smp_wmb();
                    udelay(10);
                    dev->irq_mask |= irq_data.f_Irq_Mask;
                   break;
               default:
                   break;
            }
            if (copy_to_user((t_x1timer_ioctl_data*)arg, &irq_data, (size_t)irq_size)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            retval = 0;
            break;
        case X1TIMER_DEL_SERVER_SIGNAL:/* t_iptimer_ioctl_data ******/
            if (copy_from_user(&irq_data, (t_x1timer_ioctl_data*)arg, (size_t)irq_size)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            if (!dev->irq_on){
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            if (irq_data.f_Irq_Sig > 4) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            new_mask = irq_data.f_Irq_Mask;
            new_mask = ~new_mask;
            for (i = 0; i < dev->server_signals; i++) {
                if (dev->server_signal_stack [i].f_ServerPref == cur_proc) {
                    dev->server_signal_stack [i].f_IrqProc[irq_data.f_Irq_Sig] &= new_mask;
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
            gen_mask |= X1TIMER_DMA_IRQ_ENBL;
            dev->irq_mask = gen_mask;
            iowrite32(gen_mask,(address + dev->X1TIMER_IRQ_MASK));
            smp_wmb();
            udelay(10);
            if (copy_to_user((t_x1timer_ioctl_data*)arg, &irq_data, (size_t)irq_size)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            retval = 0;
            break;
        case X1TIMER_EVENT_MASK:/* t_iptimer_ioctl_data ******/
            if (copy_from_user(&irq_data, (t_x1timer_ioctl_data*)arg, (size_t)irq_size)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            if(!irq_data.f_Status){
                dev->event_mask = irq_data.f_Irq_Mask;
            }
            irq_data.f_Irq_Mask = dev->event_mask;
            if (copy_to_user((t_x1timer_ioctl_data*)arg, &irq_data, (size_t)irq_size)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            retval = 0;
            break;            
        case X1TIMER_GET_STATUS:/* t_iptimer_ioctl_data ******/
            if (copy_from_user(&irq_data, (t_x1timer_ioctl_data*)arg, (size_t)irq_size)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            if (!dev->irq_on){
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            irq_data.f_Status 	= 0;
            irq_data.f_Irq_Sig 	= 0;
            irq_data.f_Irq_Mask	= 0;
            for (i = 0; i < dev->server_signals; i++) {
                if (dev->server_signal_stack [i].f_ServerPref == cur_proc) {
                    irq_data.f_Irq_Mask = dev->server_signal_stack [i].f_IrqProc[0];
                    irq_data.f_Irq_Sig  = dev->server_signal_stack [i].f_IrqProc[1];
                    irq_data.f_Status   = dev->server_signal_stack [i].f_IrqProc[3];
                    break;
                }
            }
            if (copy_to_user((t_x1timer_ioctl_data*)arg, &irq_data, (size_t)irq_size)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            retval = 0;
            break;
        case X1TIMER_READ_IRQ:/* t_iptimer_ioctl_data ******/
            if (copy_from_user(&irq_data, (t_x1timer_ioctl_data*)arg, (size_t)irq_size)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            if (!dev->irq_on){
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            irq_data.f_Status 	= dev->irq_status;
            irq_data.f_Irq_Sig 	= dev->irq_flag;
            irq_data.f_Irq_Mask	= dev->irq_mask;
            if (copy_to_user((t_x1timer_ioctl_data*)arg, &irq_data, (size_t)irq_size)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            retval = 0;
            break;
         case X1TIMER_CHECK_IRQ:/* t_iptimer_ioctl_data */
            if (copy_from_user(&irq_data, (t_x1timer_ioctl_data*)arg, (size_t)irq_size)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            if (!dev->irq_on){
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            irq_data.f_Irq_Sig 	= dev->irq_flag;
            irq_data.f_Irq_Mask = dev->irq_mask;
            for (i = 0; i < dev->server_signals; i++) {
                if (dev->server_signal_stack [i].f_ServerPref == cur_proc) {
                    irq_data.f_Status   = dev->server_signal_stack [i].f_IrqFlag;
                    dev->server_signal_stack [i].f_IrqFlag = 0;
                    break;
                }
            }
            if (copy_to_user((t_x1timer_ioctl_data*)arg, &irq_data, (size_t)irq_size)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            retval = 0;
            break;
        case X1TIMER_EVENT_INFO:
                event_data.f_gen_event  = dev->gen_event;
                event_data.f_Irq_Sts 	= dev->irq_status;
                event_data.f_Irq_Flag 	= dev->irq_flag;
                event_data.f_Irq_Mask 	= dev->irq_mask;
                event_data.f_Irq_Sec 	= (int)dev->irq_sec;
                event_data.f_Irq_uSec 	= (int)dev->irq_usec;
                event_data.f_Irq_Delay 	= (int)dev->irq_delay;

                if (copy_to_user((t_x1timer_ioctl_event*)arg, &event_data, (size_t)event_size)) {
                   retval = -EFAULT;
                   mutex_unlock(&dev->dev_mut);
                   return retval;
                }
                retval = 0;
                break;
        case X1TIMER_EVENT_INFO_EXT:
                event_data_ht.f_gen_event  = dev->gen_event;
                event_data_ht.f_Irq_Sts 	  = dev->irq_status;
                event_data_ht.f_Irq_Flag   = dev->irq_flag;
                event_data_ht.f_Irq_Mask   = dev->irq_mask;
                event_data_ht.f_Irq_Sec    = (int)dev->irq_sec;
                event_data_ht.f_Irq_uSec   = (int)dev->irq_usec;
                event_data_ht.f_Irq_Delay  = (int)dev->irq_delay;
                event_data_ht.f_board_Sec  = dev->board_sec;
                event_data_ht.f_board_uSec = dev->board_usec;
                event_data_ht.f_global_Sec   = dev->global_sec;
                event_data_ht.f_global_uSec = dev->global_usec;
                
                if (copy_to_user((t_x1timer_ioctl_event_ht*)arg, &event_data_ht, (size_t)event_size_ht)) {
                   retval = -EFAULT;
                   mutex_unlock(&dev->dev_mut);
                   return retval;
                }
                retval = 0;
                break;

        case X1TIMER_SET_TIMER_OFFSET:
             retval = 0;

             if (copy_from_user(&data, (device_ioctrl_data*)arg, (size_t)size_data)) {
                 retval = -EFAULT;
                 mutex_unlock(&dev->dev_mut);
                 return retval;
             }

             dev->timer_offset = data.data;

             if (copy_to_user((device_ioctrl_data*)arg, &data, (size_t)size_data)) {
                 retval = -EFAULT;
                 mutex_unlock(&dev->dev_mut);
                 return retval;
             }
             break;

        case X1TIMER_SET_TIMER:
            retval = 0;
            if (dev->timer_offset) {
                do_gettimeofday (&tv);
                dev->board_sec =  tv.tv_sec;
                dev->board_usec = tv.tv_usec;
                /* set absolute time */
                tmp_data_32 = dev->board_sec;
                iowrite32 (tmp_data_32, address + dev->timer_offset + 4);
                tmp_data_32 = dev->board_usec;
                iowrite32 (tmp_data_32, address + dev->timer_offset);
                data.offset = dev->board_usec;
                data.data   = dev->board_sec;
            } else {
                dev->board_sec  = 0;
                dev->board_usec = 0;
                data.data       = 0;
                data.offset     = 0;	
            }
            if (copy_to_user((device_ioctrl_data*)arg, &data, (size_t)size_data)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            break;
        case X1TIMER_DMA_TIME:
            retval = 0;
            
            time_data.start_time = dev->dma_start_time;
            time_data.stop_time  = dev->dma_stop_time;
            if (copy_to_user((device_ioctrl_time*)arg, &time_data, (size_t)size_time)) {
                retval = -EIO;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
            break;
        case X1TIMER_READ_DMA:
            retval = 0;
            if (copy_from_user(&dma_data, (device_ioctrl_dma*)arg, (size_t)io_dma_size)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
             if(!dev->memmory_base0){
                printk("X1TIMER_READ_DMA: NO MEMORY\n");
                mutex_unlock(&dev->dev_mut);
                retval = -ENOMEM;
                return retval;
            }
            dma_reg_address = dev->memmory_base0;
    
            tmp_dma_cmd         = dma_data.dma_cmd;
            tmp_dma_size          = dma_data.dma_size;
            tmp_dma_offset      = dma_data.dma_offset;
           
            if((tmp_dma_size + tmp_dma_offset) > dev->rw_off) {
                tmp_dma_size = dev->rw_off - tmp_dma_offset;
            }
            
            dev->dev_dma_size     = tmp_dma_size;
             if(tmp_dma_size <= 0){
                 mutex_unlock(&dev->dev_mut);
                 return -EFAULT;
            }

            tmp_free_pages = nr_free_pages();
            
            //mutex_lock(&dev->dev_dma_mut);
            tmp_dma_trns_size    = tmp_dma_size;
            if((tmp_dma_size%PCIEDEV_DMA_SYZE)){
                tmp_dma_trns_size    = tmp_dma_size + (tmp_dma_size%PCIEDEV_DMA_SYZE);
            }
                 
            if(tmp_dma_trns_size > tmp_free_pages << (PAGE_SHIFT-10)){
                printk (KERN_ALERT "X1TIMER_READ_DMA: NO MEMORY FOR TRANSF_SIZE,  %i MAX AVAILABLE MEMORY %i\n",
                        tmp_dma_trns_size, tmp_free_pages << (PAGE_SHIFT-10));
                //mutex_unlock(&dev->dev_dma_mut);
                mutex_unlock(&dev->dev_mut);
                return -ENOMEM;
            }
            value    = HZ/20; /* value is given in jiffies*/
            length   = tmp_dma_size;
            tmp_order = get_order(tmp_dma_trns_size);
            dev->dma_order = tmp_order;
            pWriteBuf = (void *)__get_free_pages(GFP_KERNEL | __GFP_DMA, tmp_order);
            if (!pWriteBuf){
                printk (KERN_ALERT "X1TIMER_READ_DMA: NO MEMORY FOR SIZE,  %X\n",tmp_dma_size);
                //mutex_unlock(&dev->dev_dma_mut);
                mutex_unlock(&dev->dev_mut);
                return -ENOMEM;
            }
            pTmpDmaHandle      = pci_map_single(pdev, pWriteBuf, tmp_dma_trns_size, PCI_DMA_FROMDEVICE);
            if (!pTmpDmaHandle){
                printk (KERN_ALERT "X1TIMER_READ_DMA: NO PCI_MAP MEMORY FOR SIZE,  %X\n",tmp_dma_size);
                 free_pages((ulong)pWriteBuf, (ulong)dev->dma_order);
                //mutex_unlock(&dev->dev_dma_mut);
                mutex_unlock(&dev->dev_mut);
                return -ENOMEM;
            }

            /* MAKE DMA TRANSFER*/
            iowrite32(0x4, ((void*)(dma_reg_address +X1TIMER_DMA_CTRL)));
            tmp_source_address = tmp_dma_offset;
            dma_sys_addr       = (u32)(pTmpDmaHandle & 0xFFFFFFFF);
            iowrite32(tmp_source_address, ((void*)(dma_reg_address + X1TIMER_DMA_BOARD_ADDR)));
            tmp_data_32         = dma_sys_addr;
            iowrite32(tmp_data_32, ((void*)(dma_reg_address +X1TIMER_DMA_HOST_ADDR)));
            //tmp_data_32         = tmp_dma_trns_size;
            tmp_data_32         = tmp_dma_size;
            iowrite32(tmp_data_32, ((void*)(dma_reg_address +X1TIMER_DMA_SIZE)));
            smp_wmb();
            udelay(5);
            tmp_data_32       = ioread32(dma_reg_address +X1TIMER_DMA_BOARD_ADDR); // be safe all writes are done
            smp_rmb();
            do_gettimeofday(&(dev->dma_start_time));
            dev->waitFlag = 0;
            iowrite32(X1TIMER_DMA_RD_START, ((void*)(dma_reg_address +X1TIMER_DMA_CTRL)));
            timeDMAwait = wait_event_interruptible_timeout( dev->waitDMA, dev->waitFlag != 0, value );
            
            if(!dev->waitFlag){
                printk (KERN_ALERT "X1TIMER_READ_DMA:SLOT NUM %i NO INTERRUPT \n", dev->slot_num);
                tmp_dma_time_cnt = 0;
                tmp_dma_done  = ioread32(dma_reg_address +X1TIMER_DMA_CTRL); 
                tmp_dma_done = ( tmp_dma_done >> 1) & 0x1; 
                dev->waitFlag = 1;
                while(!tmp_dma_done){
                    printk (KERN_ALERT "X1TIMER_READ_DMA:DMA_CNT %i\n", tmp_dma_time_cnt);
                    udelay(5000);
                    tmp_dma_done  = ioread32(dma_reg_address +X1TIMER_DMA_CTRL); 
                    tmp_dma_done = ( tmp_dma_done >> 1) & 0x1; 
                    tmp_dma_time_cnt++;
                    if(tmp_dma_time_cnt > 8){
                        tmp_dma_done = 1;
                        dev->waitFlag = 0;
                    }
                }
            }
            do_gettimeofday(&(dev->dma_stop_time));
             if(!dev->waitFlag){
/*
                     iowrite32(0x1,((void *)dev->memmory_base0 +  dev->X1TIMER_IRQ_CLR_FLAGSE));
                     iowrite32(0x0,((void *)dev->memmory_base0 +  dev->X1TIMER_IRQ_CLR_FLAGSE));
                     smp_wmb();
                     udelay(3);
                     iowrite32(0x1,((void*)(dma_reg_address +  dev->X1TIMER_IRQ_ENABLE)));
*/
/*
                iowrite32(0x0,((void *)dev->memmory_base0 +  dev->X1TIMER_IRQ_CLR_FLAGSE));
                iowrite32(0x1,(dev->memmory_base0 +  dev->X1TIMER_IRQ_ENABLE));
*/
                
                dev->irq_mask |= X1TIMER_DMA_IRQ_ENBL;
                iowrite32(dev->irq_mask,((void*)(dma_reg_address +  dev->X1TIMER_IRQ_MASK)));
                iowrite32(0x4, ((void*)(dma_reg_address +X1TIMER_DMA_CTRL)));
                smp_wmb();
                printk (KERN_ALERT "X1TIMER_READ_DMA:SLOT NUM %i DMA READ ERROR \n", dev->slot_num);
                dev->waitFlag = 1;
                pci_unmap_single(pdev, pTmpDmaHandle, tmp_dma_trns_size, PCI_DMA_FROMDEVICE);
                free_pages((ulong)pWriteBuf, (ulong)dev->dma_order);
                //mutex_unlock(&dev->dev_dma_mut);
                mutex_unlock(&dev->dev_mut);
                return -EFAULT;
            }
            iowrite32(0x4, ((void*)(dma_reg_address +X1TIMER_DMA_CTRL)));
            smp_wmb();
            pci_unmap_single(pdev, pTmpDmaHandle, tmp_dma_trns_size, PCI_DMA_FROMDEVICE);
             if (copy_to_user ((void *)arg, pWriteBuf, tmp_dma_size)) {
                retval = -EFAULT;
            }
            free_pages((ulong)pWriteBuf, (ulong)dev->dma_order);
            //mutex_unlock(&dev->dev_dma_mut);
            break;
        case X1TIMER_WRITE_DMA:
            retval = 0;
            if (copy_from_user(&dma_data, (device_ioctrl_dma*)arg, (size_t)io_dma_size)) {
                retval = -EFAULT;
                mutex_unlock(&dev->dev_mut);
                return retval;
            }
             if(!dev->memmory_base0){
                printk("X1TIMER_WRITE_DMA: NO MEMORY\n");
                mutex_unlock(&dev->dev_mut);
                retval = -ENOMEM;
                return retval;
            }
            dma_reg_address = dev->memmory_base0;
    
            tmp_dma_cmd         = dma_data.dma_cmd;
            tmp_dma_size        = dma_data.dma_size;
            tmp_dma_offset      = dma_data.dma_offset;
            
            if((tmp_dma_size + tmp_dma_offset) > dev->rw_off) {
                tmp_dma_size = dev->rw_off - tmp_dma_offset;
            }
            
            dev->dev_dma_size     = tmp_dma_size;
             if(tmp_dma_size <= 0){
                 mutex_unlock(&dev->dev_mut);
                 return -EFAULT;
            }
            
            //mutex_lock(&dev->dev_dma_mut);
            tmp_dma_trns_size    = tmp_dma_size;
            if((tmp_dma_size%PCIEDEV_DMA_SYZE)){
                tmp_dma_trns_size    = tmp_dma_size + (tmp_dma_size%PCIEDEV_DMA_SYZE);
            }
            tmp_free_pages = nr_free_pages();
            if(tmp_dma_trns_size > tmp_free_pages << (PAGE_SHIFT-10)){
                printk (KERN_ALERT "X1TIMER_WRITE_DMA: NO MEMORY FOR TRANSF_SIZE,  %i MAX AVAILABLE MEMORY %i\n",
                        tmp_dma_trns_size, tmp_free_pages << (PAGE_SHIFT-10));
                //mutex_unlock(&dev->dev_dma_mut);
                mutex_unlock(&dev->dev_mut);
                return -ENOMEM;
            }
                        
            value    = HZ/1; /* value is given in jiffies*/
            length   = tmp_dma_size;
            tmp_order = get_order(tmp_dma_trns_size);
            dev->dma_order = tmp_order;
            pWriteBuf = (void *)__get_free_pages(GFP_KERNEL | __GFP_DMA, tmp_order);
            if (!pWriteBuf){
                printk (KERN_ALERT "X1TIMER_WRITE_DMA: NO MEMORY FOR SIZE,  %X\n",tmp_dma_size);
                //mutex_unlock(&dev->dev_dma_mut);
                mutex_unlock(&dev->dev_mut);
                return -EFAULT;
            }
            
            if (copy_from_user(pWriteBuf, ((int*)arg + DMA_DATA_OFFSET), (size_t)tmp_dma_size)) {
                    printk (KERN_ALERT "X1TIMER_WRITE_DMA: could not read from user buf\n");
                    retval = -EFAULT;
                    free_pages((ulong)pWriteBuf, (ulong)tmp_order);
                    //mutex_unlock(&dev->dev_dma_mut);
                    mutex_unlock(&dev->dev_mut);
                    return retval;
                }
            
            pTmpDmaHandle      = pci_map_single(pdev, pWriteBuf, tmp_dma_trns_size, PCI_DMA_TODEVICE);
            if (!pTmpDmaHandle){
                printk (KERN_ALERT "X1TIMER_WRITE_DMA: NO PCI_MAP MEMORY FOR SIZE,  %X\n",tmp_dma_size);
                 free_pages((ulong)pWriteBuf, (ulong)dev->dma_order);
                //mutex_unlock(&dev->dev_dma_mut);
                mutex_unlock(&dev->dev_mut);
                return -ENOMEM;
            }

            /* MAKE DMA TRANSFER*/
            iowrite32(0x0, ((void*)(dma_reg_address +X1TIMER_DMA_CTRL)));
            tmp_source_address = tmp_dma_offset;
            dma_sys_addr       = (u32)(pTmpDmaHandle & 0xFFFFFFFF);
            iowrite32(tmp_source_address, ((void*)(dma_reg_address + X1TIMER_DMA_BOARD_ADDR)));
            tmp_data_32         = dma_sys_addr;
            iowrite32(tmp_data_32, ((void*)(dma_reg_address +X1TIMER_DMA_HOST_ADDR)));
            //tmp_data_32         = tmp_dma_trns_size;
            tmp_data_32         = tmp_dma_size;
            iowrite32(tmp_data_32, ((void*)(dma_reg_address +X1TIMER_DMA_SIZE)));
            smp_wmb();
            udelay(5);
            tmp_data_32       = ioread32(dma_reg_address +X1TIMER_DMA_BOARD_ADDR); // be safe all writes are done
            smp_rmb();
            do_gettimeofday(&(dev->dma_start_time));
            dev->waitFlag = 0;
            iowrite32(X1TIMER_DMA_WR_START, ((void*)(dma_reg_address +X1TIMER_DMA_CTRL)));
            timeDMAwait = wait_event_interruptible_timeout( dev->waitDMA, dev->waitFlag != 0, value );
            
            if(!dev->waitFlag){
                printk (KERN_ALERT "X1TIMER_WRITE_DMA:SLOT NUM %i NO INTERRUPT \n", dev->slot_num);
                tmp_dma_time_cnt = 0;
                tmp_dma_done  = ioread32(dma_reg_address +X1TIMER_DMA_CTRL); 
                tmp_dma_done = ( tmp_dma_done >> 3) & 0x1; 
                dev->waitFlag = 1;
                while(!tmp_dma_done){
                    printk (KERN_ALERT "X1TIMER_WRITE_DMA:DMA_CNT %i\n", tmp_dma_time_cnt);
                    udelay(5000);
                    tmp_dma_done  = ioread32(dma_reg_address +X1TIMER_DMA_CTRL); 
                    tmp_dma_done = ( tmp_dma_done >> 3) & 0x1; 
                    tmp_dma_time_cnt++;
                    if(tmp_dma_time_cnt > 200){
                        tmp_dma_done = 1;
                        dev->waitFlag = 0;
                    }
                }
            }
           do_gettimeofday(&(dev->dma_stop_time));
           if(!dev->waitFlag){
/*
                    iowrite32(0x1,((void *)dev->memmory_base0 +  dev->X1TIMER_IRQ_CLR_FLAGSE));
                    iowrite32(0x0,((void *)dev->memmory_base0 +  dev->X1TIMER_IRQ_CLR_FLAGSE));
                    smp_wmb();
                    udelay(3);
                    iowrite32(0x1,((void*)(dma_reg_address +  dev->X1TIMER_IRQ_ENABLE)));
*/
/*
                 iowrite32(0x0,((void *)dev->memmory_base0 +  dev->X1TIMER_IRQ_CLR_FLAGSE));
                 iowrite32(0x1,(dev->memmory_base0 +  dev->X1TIMER_IRQ_ENABLE));
*/
                 
                dev->irq_mask |= X1TIMER_DMA_IRQ_ENBL;
                iowrite32(dev->irq_mask,((void*)(dma_reg_address +  dev->X1TIMER_IRQ_MASK)));
                iowrite32(0x4, ((void*)(dma_reg_address +X1TIMER_DMA_CTRL)));
                smp_wmb();
                printk (KERN_ALERT "X1TIMER_WRITE_DMA:SLOT NUM %i DMA WRITE ERROR \n", dev->slot_num);
                dev->waitFlag = 1;
                pci_unmap_single(pdev, pTmpDmaHandle, tmp_dma_trns_size, PCI_DMA_FROMDEVICE);
                free_pages((ulong)pWriteBuf, (ulong)dev->dma_order);
                //mutex_unlock(&dev->dev_dma_mut);
                mutex_unlock(&dev->dev_mut);
                return -EFAULT;
            }
            iowrite32(0x4, ((void*)(dma_reg_address +X1TIMER_DMA_CTRL)));
            smp_wmb();
            pci_unmap_single(pdev, pTmpDmaHandle, tmp_dma_trns_size, PCI_DMA_FROMDEVICE);
            free_pages((ulong)pWriteBuf, (ulong)dev->dma_order);
            //mutex_unlock(&dev->dev_dma_mut);
            break;
        default:
             return -ENOTTY;
             break;
    }
    mutex_unlock(&dev->dev_mut);
    return retval;

}

struct file_operations x1timer_fops = {
      	.owner    =  THIS_MODULE,
      	.read     =  x1timer_read,
      	.write    =  x1timer_write,
      	//.ioctl    =  x1timer_ioctl,
      	.unlocked_ioctl =  x1timer_ioctl,
      	.open     =  x1timer_open,
      	.release  =  x1timer_release,
};

static void __exit x1timer_cleanup_module(void)
{
    int i = 0;
    
    dev_t devno = MKDEV(X1TIMER_MAJOR, X1TIMER_MINOR);
    printk(KERN_NOTICE "X1TIMER_CLEANUP_MODULE\n");

    pci_unregister_driver(&pci_x1timer_driver);

    for(i = 0; i < X1TIMER_NR_DEVS; i++){
    
        cancel_work_sync(&x1timer_dev[i].x1timerp_work);
        mutex_destroy(&x1timer_dev[i].dev_mut);
        mutex_destroy(&x1timer_dev[i].dev_dma_mut);

        /* Get rid of our char dev entries */
        cdev_del (&x1timer_dev[i].cdev);
    }
    
    flush_workqueue(x1timerp_workqueue);
    destroy_workqueue(x1timerp_workqueue);
    /* cleanup_module is never called if registering failed */
    unregister_chrdev_region(devno, X1TIMER_NR_DEVS);
    class_destroy(x1timer_class);
}

static int __init x1timer_init_module(void)
{
    int   result = 0;
    dev_t dev    = 0;
    int   devno  = 0;
    int   i      = 0;
    
    printk(KERN_WARNING "X1TIMER_INIT_MODULE\n");
    
    result = alloc_chrdev_region(&dev, X1TIMER_MINOR, X1TIMER_NR_DEVS, DEVNAME);
    X1TIMER_MAJOR = MAJOR(dev);

    /* Populate sysfs entries */
    x1timer_class = class_create(THIS_MODULE, "x1timer");
    
    for(i = 0; i <= X1TIMER_NR_DEVS; i++){
    
        devno = MKDEV (X1TIMER_MAJOR, X1TIMER_MINOR + i); 
        
        INIT_LIST_HEAD(&(x1timer_dev[i].dev_file_list.node_file_list));
        mutex_init (&x1timer_dev[i].dev_mut);
        mutex_init (&x1timer_dev[i].dev_dma_mut);

        x1timer_dev[i].dev_sts          = 0;
        x1timer_dev[i].binded           = 0;
        x1timer_dev[i].null_dev         = 0;
        x1timer_dev[i].f_cntl            =  0;
        x1timer_dev[i].dev_num          =  devno;
        x1timer_dev[i].slot_num         = 0;
        x1timer_dev[i].dev_minor        = 0;
        x1timer_dev[i].vendor_id        = 0;
        x1timer_dev[i].device_id        = 0;
        x1timer_dev[i].subvendor_id     = 0;
        x1timer_dev[i].subdevice_id     = 0;
        x1timer_dev[i].class_code       = 0;
        x1timer_dev[i].revision         = 0;
        x1timer_dev[i].devfn            = 0;
        x1timer_dev[i].busNumber        = 0;
        x1timer_dev[i].devNumber        = 0;
        x1timer_dev[i].funcNumber       = 0;
        x1timer_dev[i].bus_func         = 0;
        x1timer_dev[i].mem_base0        = 0;
        x1timer_dev[i].mem_base0_end    = 0;
        x1timer_dev[i].mem_base0_flag   = 0;
        x1timer_dev[i].memmory_base0    = 0;
        x1timer_dev[i].rw_off           = 0;
        x1timer_dev[i].x1timer_all_mems = 0;
        x1timer_dev[i].dev_payload_size = 0;
        x1timer_dev[i].timer_offset     = 0;

        #if LINUX_VERSION_CODE < 132632
            INIT_WORK(&(x1timer_dev[i].x1timer_work),x1timerp_do_work, &x1timer_dev[i]);
        #else
            INIT_WORK(&(x1timer_dev[i].x1timerp_work),x1timerp_do_work);
        #endif

        cdev_init(&x1timer_dev[i].cdev, &x1timer_fops);
        x1timer_dev[i].cdev.owner = THIS_MODULE;
        x1timer_dev[i].cdev.ops = &x1timer_fops;

        result = cdev_add(&x1timer_dev[i].cdev, devno, 1);

        if (result) {
            printk(KERN_NOTICE "X1TIMER_INIT_MODULE: Error %d adding devno%d num%d", result, devno, i);
            return 1;
        }
    }
    x1timer_dev[X1TIMER_NR_DEVS].binded        = 1;
    x1timer_dev[X1TIMER_NR_DEVS].null_dev      = 1;
    x1timerp_workqueue = create_workqueue("x1timer");
    
    result = pci_register_driver(&pci_x1timer_driver);

    if (result < 0) return 1;
    return 0; /* succeed */
}

module_init(x1timer_init_module);
module_exit(x1timer_cleanup_module);


