#include "pcie_gen_exp.h"

#include "pcie_gen_fnc.h"      	/* local definitions */


#ifndef LINUX_VERSION_CODE
#define LINUX_VERSION_CODE KERNEL_VERSION(3,5,0)
#endif

MODULE_AUTHOR("Davit Kalantaryan");
MODULE_DESCRIPTION("pcie_gen_in General purpose driver ");
MODULE_VERSION("1.1.0");
MODULE_LICENSE("Dual BSD/GPL");

static u16 PCIE_GEN_DRV_VER_MAJ=1;
static u16 PCIE_GEN_DRV_VER_MIN=1;

int						g_nTmpMinor		= 1;
//struct str_pcie_gen       str_pcie_gen[PCIE_GEN_NR_DEVS];	/* allocated in iptimer_init_module */
struct Pcie_gen_dev		g_pcie_gen0;	/* allocated in iptimer_init_module */
struct str_pcie_gen		g_vPci_gens[PCIE_GEN_NR_DEVS];
struct class*			pcie_gen_class;

static int    pcie_gen_procinfo(char *, char **, off_t, int, int *,void *);
static struct proc_dir_entry *pcie_gen_procdir;


static int pcie_gen_procinfo(char *buf, char **start, off_t fpos, int lenght, int *eof, void *data)
{
	char *p;
	struct Pcie_gen_dev *dev = data;

	p = buf;
	p += sprintf(p,"Driver Version:\t%i.%i\n", PCIE_GEN_DRV_VER_MAJ, PCIE_GEN_DRV_VER_MIN);
	p += sprintf(p,"STATUS Revision:\t%i\n", dev->m_dev_sts);

	*eof = 1;
	return p - buf;
}



int PCIE_GEN_MAJOR = 46;     /* static major by default */
int PCIE_GEN_MINOR = 0;      /* static minor by default */



static int pcie_gen_open( struct inode *inode, struct file *filp )
{
    int    minor;
	struct Pcie_gen_dev *dev2; /* device information */
	
    minor = iminor(inode);
    dev2 = container_of(inode->i_cdev, struct Pcie_gen_dev, m_cdev);
    dev2->m_dev_minor       = minor;
    filp->private_data   = dev2; /* for other methods */

    printk(KERN_ALERT "Open Procces is \"%s\" (pid %i) DEV is %d \n", current->comm, current->group_leader->pid, minor);

    return 0;
}



static int pcie_gen_release(struct inode *inode, struct file *filp)
{
    int minor     = 0;
    int d_num     = 0;
    u16 cur_proc  = 0;

    struct Pcie_gen_dev      *dev;
    
    dev = filp->private_data;

    minor = dev->m_dev_minor;
    d_num = dev->m_dev_num;
    //cur_proc = current->pid;
    cur_proc = current->group_leader->pid;
    if (mutex_lock_interruptible(&dev->m_dev_mut))
                    return -ERESTARTSYS;
    printk(KERN_ALERT "PCIE_GEN_SLOT RELEAASE: Close Procces is \"%s\" (pid %i)\n", current->comm, current->pid);
    printk(KERN_ALERT "PCIE_GEN_SLOT RELEAASE: Close MINOR %d DEV_NUM %d \n", minor, d_num);
    mutex_unlock(&dev->m_dev_mut);
    return 0;
}



struct pci_dev* FindDevice(struct SDeviceParams* a_pActiveDev, int* a_tmp_slot_num_ptr)
{
	int nIteration						= 0;
	u32 tmp_slot_cap					= 0;
	int pcie_cap						= 0;
	struct pci_dev *pPciTmp				= NULL;
	struct pci_dev *pPcieRet			= NULL;

	if(a_pActiveDev->vendor_id<0)
	{
		a_pActiveDev->vendor_id = PCI_ANY_ID;
	}

	if(a_pActiveDev->device_id<0)
	{
		a_pActiveDev->device_id = PCI_ANY_ID;
	}

	printk(KERN_ALERT "FindDevice: vendor_id = %d,  device_id = %d\n",a_pActiveDev->vendor_id,a_pActiveDev->device_id);


	while( (*a_tmp_slot_num_ptr != a_pActiveDev->slot_num) && ((a_pActiveDev->slot_num>=0)||(nIteration==0)))
	{
		pPcieRet = pci_get_device( a_pActiveDev->vendor_id,a_pActiveDev->device_id,pPciTmp );
		
		if(!pPcieRet)break;


		if(pPcieRet->bus)
		{

			if(pPcieRet->bus->parent)
			{
				if( pPcieRet->bus->parent->self )
				{
					pcie_cap = pci_find_capability (pPcieRet->bus->parent->self, PCI_CAP_ID_EXP);
					pci_read_config_dword(pPcieRet->bus->parent->self, (pcie_cap + PCI_EXP_SLTCAP), &tmp_slot_cap);
					*a_tmp_slot_num_ptr = (tmp_slot_cap >> 19);
					++nIteration;
				}
			}
		}

		pPciTmp = pPcieRet;
		
	}


	return pPcieRet;
}



void FindDeviceForRelase(struct SDeviceParams* a_pActiveDev, int* a_tmp_slot_num_ptr)
{

	int i;

	*a_tmp_slot_num_ptr = -1;


	if(a_pActiveDev->slot_num>=0)
	{
		a_pActiveDev->slot_num %= PCIE_GEN_NR_DEVS;

		if(g_vPci_gens[a_pActiveDev->slot_num].m_PcieGen.m_dev_sts)
		{
			*a_tmp_slot_num_ptr = a_pActiveDev->slot_num;
		}

		return;
	}


	if( a_pActiveDev->vendor_id<0 && a_pActiveDev->device_id<0 )return;

	
	for( i = 0; i < PCIE_GEN_NR_DEVS; ++i )
	{
		if(g_vPci_gens[i].m_PcieGen.m_dev_sts)
		{
			if( (a_pActiveDev->vendor_id<0 || a_pActiveDev->vendor_id==g_vPci_gens[i].vendor_id) &&
				(a_pActiveDev->device_id<0 || a_pActiveDev->device_id==g_vPci_gens[i].device_id))
			{
				*a_tmp_slot_num_ptr = g_vPci_gens[i].m_PcieGen.m_slot_num;
				return;
			}
		}
	}

	
}



long  pcie_gen_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	const char* cpcFileName;
	int tmp_slot_num     = 0;
	//int pcie_cap;
	//u32 tmp_slot_cap     = 0;
	int err = 0;
	SActiveDev	aActiveDev;
	struct pci_dev *pPCI;
	struct Pcie_gen_dev*	dev = filp->private_data;

	cpcFileName = strrchr(__FILE__,'/') + 1;
	cpcFileName = cpcFileName ? cpcFileName : "Unknown File";
	printk(KERN_ALERT "\"%s\":\"pcie_gen_ioctl\":%d, cmd = %d,  arg = %ld\n",cpcFileName,__LINE__,(int)cmd,(long)arg);

	if( !dev->m_dev_sts )
	{
		printk(KERN_ALERT "pcie_gen_ioctl: No device!\n");
		return -EFAULT;
	}


	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != PCIE_GEN_IOC)    return -ENOTTY;
	if (_IOC_NR(cmd) > PCIE_GEN_IOC_MAXNR) return -ENOTTY;

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

	
	if( err )
	{
		printk( KERN_ALERT "pcie_gen_ioctl:%d  access_ok Error\n",__LINE__);
		return -EFAULT;
	}
	
	if (mutex_lock_interruptible(&dev->m_dev_mut))return -ERESTARTSYS;

	switch(cmd)
	{
	case PCIE_GEN_GAIN_ACCESS:
		if( copy_from_user(&aActiveDev,(SActiveDev*)arg,sizeof(SActiveDev))  )
		{
			mutex_unlock(&dev->m_dev_mut);
			return -EFAULT;
		}
		
		
		pPCI = FindDevice(&aActiveDev.dev_pars,&tmp_slot_num);

		if(!pPCI)
		{
			printk(KERN_ALERT "pcie_gen_ioctl:%d. Couldn't find device\n",__LINE__);
			mutex_unlock(&dev->m_dev_mut);
			return -EFAULT;
		}

		g_vPci_gens[tmp_slot_num].pcie_gen_pci_dev = pPCI;
		g_vPci_gens[tmp_slot_num].m_PcieGen.m_dev_sts = 1;
		g_vPci_gens[tmp_slot_num].m_PcieGen.m_dev_minor = PCIE_GEN_MINOR + tmp_slot_num + 1;
		
		g_vPci_gens[tmp_slot_num].irq_on = aActiveDev.irq_on;

		if(g_vPci_gens[tmp_slot_num].irq_on)
		{
			memcpy( g_vPci_gens[tmp_slot_num].m_rst_irq,aActiveDev.reset_irq,sizeof(aActiveDev.reset_irq) );
		}

		General_GainAccess1(&g_vPci_gens[tmp_slot_num], PCIE_GEN_MAJOR,pcie_gen_class, "pcie_gen_drv" );
	
		break;

	case PCIE_GEN_RELASE_ACCESS:
		if( copy_from_user(&aActiveDev.dev_pars,(SDeviceParams*)arg,sizeof(SDeviceParams))  )
		{
			mutex_unlock(&dev->m_dev_mut);
			return -EFAULT;
		}
		
		FindDeviceForRelase(&aActiveDev.dev_pars,&tmp_slot_num);

		if(tmp_slot_num<0)
		{
			printk(KERN_ALERT "pcie_gen_ioctl:%d. Couldn't find device\n",__LINE__);
			mutex_unlock(&dev->m_dev_mut);
			return -EFAULT;
		}

		Gen_ReleaseAccsess( &g_vPci_gens[tmp_slot_num], PCIE_GEN_MAJOR, pcie_gen_class );
		//pci_dev_put( g_vPci_gens[tmp_slot_num].pcie_gen_pci_dev );
		break;

	default:
		break;
	}

	mutex_unlock(&dev->m_dev_mut);
	
	return 0;
}




static int pcie_gen_open_tot( struct inode *inode, struct file *filp )
{
    int    minor;
    struct str_pcie_gen *dev; /* device information */
	struct Pcie_gen_dev *dev2; /* device information */
	
    minor = iminor(inode);
    dev2 = container_of(inode->i_cdev, struct Pcie_gen_dev, m_cdev);
	dev = container_of(dev2, struct str_pcie_gen, m_PcieGen);
    dev->m_PcieGen.m_dev_minor       = minor;
    filp->private_data   = dev; /* for other methods */

    printk(KERN_ALERT "Open Procces is \"%s\" (pid %i) DEV is %d \n", current->comm, current->group_leader->pid, minor);

    return 0;
}



static int pcie_gen_release_tot(struct inode *inode, struct file *filp)
{
    int minor     = 0;
    int d_num     = 0;
    u16 cur_proc  = 0;

    struct str_pcie_gen      *dev;
    
    dev = filp->private_data;

    minor = dev->m_PcieGen.m_dev_minor;
    d_num = dev->m_PcieGen.m_dev_num;
    //cur_proc = current->pid;
    cur_proc = current->group_leader->pid;
    if (mutex_lock_interruptible(&dev->m_PcieGen.m_dev_mut))
                    return -ERESTARTSYS;
    printk(KERN_ALERT "TAMC200_SLOT RELEAASE: Close Procces is \"%s\" (pid %i)\n", current->comm, current->pid);
    printk(KERN_ALERT "TAMC200_SLOT RELEAASE: Close MINOR %d DEV_NUM %d \n", minor, d_num);
    mutex_unlock(&dev->m_PcieGen.m_dev_mut);
    return 0;
}



//static ssize_t tamc200_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
ssize_t pcie_gen_read_tot(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{

	struct str_pcie_gen  *dev = filp->private_data;
	printk( KERN_ALERT "dev->irq_slot = %d\n", dev->irq_slot );
	return General_Read1(&dev->m_PcieGen, buf, count);
}



// Piti nayvi
//static ssize_t tamc200_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
ssize_t pcie_gen_write_tot(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct str_pcie_gen  *dev = filp->private_data;
	printk( KERN_ALERT "dev_sts = %d\n", dev->m_PcieGen.m_dev_sts );
	return General_Write1(&dev->m_PcieGen,buf,count);
}


long  pcie_gen_ioctl_tot(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct str_pcie_gen*	dev = filp->private_data;
	//return General_ioctl(&dev->m_PcieGen,cmd,arg);
	return General_ioctl2(dev,cmd,arg);
}


struct file_operations pcie_gen_fops = {
	.owner   =  THIS_MODULE,
	//.read    =  tamc200_read,
	//.write   =  tamc200_write,
	//.ioctl   =  pcie_gen_ioctl,
	.unlocked_ioctl   =  pcie_gen_ioctl,
	.open    =  pcie_gen_open,
	.release =  pcie_gen_release,
};



struct file_operations pcie_gen_fops_tot = {
	.owner   =  THIS_MODULE,
	.read    =  pcie_gen_read_tot,
	.write   =  pcie_gen_write_tot,
	//.ioctl   =  pcie_gen_ioctl,
	.unlocked_ioctl   =  pcie_gen_ioctl_tot,
	.open    =  pcie_gen_open_tot,
	.release =  pcie_gen_release_tot,
};




void __exit pcie_gen_cleanup_module(void)
{

	int i;

    dev_t devno = MKDEV(PCIE_GEN_MAJOR, PCIE_GEN_MINOR);

	printk(KERN_WARNING "PCIE_GEN: pcie_gen_cleanup_module CALLED\n");

	for( i = 0; i < PCIE_GEN_NR_DEVS; ++i )
	{
		if( g_vPci_gens[i].m_PcieGen.m_dev_sts )
		{
			Gen_ReleaseAccsess( &g_vPci_gens[i], PCIE_GEN_MAJOR, pcie_gen_class );
			//printk(KERN_ALERT "Trying to put device\n");
			//pci_dev_put( g_vPci_gens[i].pcie_gen_pci_dev );
			//printk(KERN_ALERT "put device done\n");
		}
	}

	remove_proc_entry("pcie_gen_fd",0);

	device_destroy(pcie_gen_class,  MKDEV(PCIE_GEN_MAJOR, g_pcie_gen0.m_dev_minor));

	//flush_workqueue(tamc200_workqueue);
    //destroy_workqueue(tamc200_workqueue);

	mutex_destroy(&g_pcie_gen0.m_dev_mut);

	cdev_del(&g_pcie_gen0.m_cdev);

	unregister_chrdev_region(devno, PCIE_GEN_NR_DEVS+1);
	class_destroy(pcie_gen_class);
        
}



//static int __init pcie_gen_init_module(void)
int __init pcie_gen_init_module(void)
{
	int   result    = 0;
	dev_t dev       = 0;
	int   devno     = 0;
	int i;
	int j;


	printk(KERN_WARNING "PCIE_GEN: pcie_gen_init_module CALLED\n");

	result = alloc_chrdev_region(&dev, PCIE_GEN_MINOR, PCIE_GEN_NR_DEVS+1, DEVNAME);
	PCIE_GEN_MAJOR = MAJOR(dev);
	printk(KERN_ALERT "PCIE_GEN: AFTER_INIT DRV_MAJOR is %d\n", PCIE_GEN_MAJOR);

	pcie_gen_class = class_create(THIS_MODULE, DRV_NAME);

	devno = MKDEV(PCIE_GEN_MAJOR, PCIE_GEN_MINOR);
	cdev_init(&g_pcie_gen0.m_cdev, &pcie_gen_fops);
	g_pcie_gen0.m_cdev.owner = THIS_MODULE;
	g_pcie_gen0.m_cdev.ops = &pcie_gen_fops;
	result = cdev_add(&g_pcie_gen0.m_cdev, devno, 1);
	if (result)
	{
		printk(KERN_NOTICE "TAMC200: Error %d adding devno%d num%d", result, devno, 0);
		return result;
	}
	mutex_init(&g_pcie_gen0.m_dev_mut);
	g_pcie_gen0.m_dev_num   = 0;
	g_pcie_gen0.m_dev_minor = PCIE_GEN_MINOR;
	g_pcie_gen0.m_dev_sts   = 0;
	g_pcie_gen0.m_slot_num  = 0;

	for( j = 0; j < NUMBER_OF_BARS; ++j ){g_pcie_gen0.m_Memories[j].m_memory_base = 0;}


	device_create(pcie_gen_class, NULL, MKDEV(PCIE_GEN_MAJOR, g_pcie_gen0.m_dev_minor),
	/*&g_pcie_gen.tamc200_pci_dev->dev*/NULL, "pcie_gen_fd");

	pcie_gen_procdir = create_proc_entry("pcie_gen_fd", S_IFREG | S_IRUGO, 0);
	pcie_gen_procdir->read_proc = pcie_gen_procinfo;
	pcie_gen_procdir->data = &g_pcie_gen0;

	g_pcie_gen0.m_dev_sts = 1;


	for( i = 0; i < PCIE_GEN_NR_DEVS; ++i )
	{
		g_vPci_gens[i].m_PcieGen.m_dev_sts = 0;
		g_vPci_gens[i].m_PcieGen.m_dev_minor = (PCIE_GEN_MINOR + i+1);

		devno = MKDEV(PCIE_GEN_MAJOR, PCIE_GEN_MINOR + i+1); 
		cdev_init(&g_vPci_gens[i].m_PcieGen.m_cdev, &pcie_gen_fops_tot);
		g_vPci_gens[i].m_PcieGen.m_cdev.owner = THIS_MODULE;
		g_vPci_gens[i].m_PcieGen.m_cdev.ops = &pcie_gen_fops_tot;
		result = cdev_add(&g_vPci_gens[i].m_PcieGen.m_cdev, devno, 1);
		if (result)
		{
			printk(KERN_NOTICE "PCIE_GEN: Error %d adding devno%d num%d", result, devno, i);
			return result;
		}
		mutex_init(&g_vPci_gens[i].m_PcieGen.m_dev_mut);
		g_vPci_gens[i].m_PcieGen.m_dev_num   = i+1;
		g_vPci_gens[i].m_PcieGen.m_dev_minor = (PCIE_GEN_MINOR + i+1);
		g_vPci_gens[i].m_PcieGen.m_dev_sts   = 0;
		g_vPci_gens[i].m_PcieGen.m_slot_num  = 0;

#if LINUX_VERSION_CODE < 132632 
		//INIT_WORK(&(g_vPci_gens[i].tamc200_work),tamc200_do_work, &g_vPci_gens[i]);
#else
		//INIT_WORK(&(g_vPci_gens[i].tamc200_work),tamc200_do_work);    
#endif
		
		for( j = 0; j < NUMBER_OF_BARS; ++j ){g_vPci_gens[i].m_PcieGen.m_Memories[j].m_memory_base = 0;}
	}

	return 0;
	
}

module_init(pcie_gen_init_module);
module_exit(pcie_gen_cleanup_module);
