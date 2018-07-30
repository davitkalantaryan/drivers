#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/pci_regs.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/slab.h>


static int scull_major = 0;
static int scull_minor = 0;
static const char device_name[] = "timer_drv_dr";

static const char    g_s_Hello_World_string[] = "Hello world from kernel mode!\n\0";
static const ssize_t g_s_Hello_World_size = sizeof(g_s_Hello_World_string);

struct scull_dev {
    struct scull_qset *data;  /* Pointer to first quantum set */
    int quantum;              /* the current quantum size */
    int qset;                 /* the current array size */
    unsigned long size;       /* amount of data stored here */
    unsigned int access_key;  /* used by sculluid and scullpriv */
    struct semaphore sem;     /* mutual exclusion semaphore     */
    struct cdev m_cdev;     /* Char device structure      */
};



static ssize_t device_file_read(struct file *file_ptr, char __user *user_buffer, 
								size_t count, loff_t *position )
{
        printk( KERN_NOTICE "Simple-driver:Device file is read at offset = %i, read bytes count = %u"
                                , (int)*position, (unsigned int)count );

        /* If position is behind the end of a file we have nothing to read */
        if( *position >= g_s_Hello_World_size )
                return 0;

        /* If a user tries to read more than we have, read only
        as many bytes as we have */
        if( *position + count > g_s_Hello_World_size )
                count = g_s_Hello_World_size - *position;

        if( copy_to_user(user_buffer, g_s_Hello_World_string + *position, count) != 0 )
                return -EFAULT;

        /* Move reading position */
*position += count;
        return count;
}



static ssize_t device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
        printk(KERN_ALERT "Sorry, this operation isn't supported.\n");
        return -EINVAL;
}



struct scull_qset {
    void **data;
    struct scull_qset *next;
};



static int scull_trim(struct scull_dev *dev)
{
#if 1
    struct scull_qset *next, *dptr;
    int qset = dev->qset;   /* "dev" is not-null */
    int i;

    for (dptr = dev->data; dptr; dptr = next) { /* all the list items */
        if (dptr->data) {
            for (i = 0; i < qset; i++)
                kfree(dptr->data[i]);
            kfree(dptr->data);
            dptr->data = NULL;
        }
        next = dptr->next;
        kfree(dptr);
    }
    dev->size = 0;
    //dev->quantum = scull_quantum;
    //dev->qset = scull_qset;
    dev->data = NULL;
#endif
    return dev ? 0 : 1;
}



static int scull_open(struct inode *inode, struct file *filp)
{
    struct scull_dev *dev; /* device information */

    dev = container_of(inode->i_cdev, struct scull_dev, m_cdev);
    filp->private_data = dev; /* for other methods */

    /* now trim to 0 the length of the device if open was write-only */
    if ( (filp->f_flags & O_ACCMODE) == O_WRONLY) {
        scull_trim(dev); /* ignore errors */
    }
    return 0;          /* success */
}



static struct file_operations scull_fops =
{
        .owner   = THIS_MODULE,
		.open = scull_open,
        .read    = device_file_read,
        .write  = device_write,
};



static void scull_setup_cdev(struct scull_dev *dev, int index)
{
    int err, devno = MKDEV(scull_major, scull_minor + index);
    
    cdev_init(&dev->m_cdev, &scull_fops);
    dev->m_cdev.owner = THIS_MODULE;
    dev->m_cdev.ops = &scull_fops;
    err = cdev_add (&dev->m_cdev, devno, 1);
    /* Fail gracefully if need be */
    if (err)
    printk(KERN_NOTICE "Error %d adding scull%d", err, index);
}


void unregister_device(void)
{
        printk( KERN_NOTICE "Simple-driver: unregister_device() is called" );
        if(scull_major != 0)
        {
                unregister_chrdev(scull_major, device_name);
        }
}




static int register_device(void)
{
                int result = 0;

                printk( KERN_NOTICE "Simple-driver: register_device() is called." );

                result = register_chrdev( 0, device_name, &scull_fops );
                if( result < 0 )
                {
                        printk( KERN_WARNING "Simple-driver:  can\'t register character device with errorcode = %i", result );
                        return result;
                }

                scull_major = result;
                printk( KERN_NOTICE "Simple-driver: registered character device with major number = %i and minor numbers 0...255"
                         , scull_major );

                return 0;
}

module_init(register_device);
module_exit(unregister_device);
                   