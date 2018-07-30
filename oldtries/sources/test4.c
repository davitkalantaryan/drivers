#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/pci_regs.h>
#include <linux/cdev.h>
#include <linux/device.h>			// For class_create


MODULE_LICENSE("GPL");



typedef struct AScullStr
{
	int			m_nMinor;
	struct cdev	m_cdev;
}AScullStr;


#define					DRIVER_NAME						"test_drv"
#define					TIMER_FIRST_MINOR				0
#define					NUMBER_OF_TM_DEVICES			10
static int				g_device_file_major_number	=	0;


static const char		g_s_Hello_World_string[]	=	"Hello world from kernel mode!\0";
static const ssize_t	g_s_Hello_World_size		=	sizeof(g_s_Hello_World_string);

static AScullStr		g_Structs[NUMBER_OF_TM_DEVICES];

static char				g_vcBuffer[200];


struct class             *test4_class;




static ssize_t device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	printk(KERN_ALERT "Sorry, this operation isn't supported.\n");
	return -EINVAL;
}



static ssize_t device_file_read(
						struct file *file_ptr
					   , char __user *user_buffer
					   , size_t count
					   , loff_t *position)
{

	//printk( KERN_NOTICE "read function\n" );

	//return 0;

	struct AScullStr *pDev = file_ptr->private_data;
	int nLen;

	printk( KERN_NOTICE "Simple-driver:Device file is read at offset = %i, read bytes count = %u"
				, (int)*position, (unsigned int)count );

	/* If position is behind the end of a file we have nothing to read */
	if( *position >= g_s_Hello_World_size )
		return 0;

	/* If a user tries to read more than we have, read only 
	as many bytes as we have */
	nLen = sprintf(g_vcBuffer,"Minor = %d\n%s.\n",pDev->m_nMinor,g_s_Hello_World_string);

	if( *position + count > nLen )
		count = nLen - *position;

	if( copy_to_user(user_buffer, g_vcBuffer + *position, count) != 0 )
		return -EFAULT;	

	/* Move reading position */
	*position += count;
	return count;
}



static int fl_open(struct inode *a_inode, struct file *a_filep)
{

	AScullStr* pDev;

	pDev = container_of(a_inode->i_cdev, struct AScullStr, m_cdev);

	a_filep->private_data = pDev;

	printk( KERN_NOTICE "Minor is %i\n", pDev->m_nMinor );

	//return 1;

	return 0;
}



static struct file_operations simple_driver_fops =
{
	.owner   =  THIS_MODULE,
	.open = fl_open,
//	.close = NULL,
	.owner   = THIS_MODULE,
	.read    = device_file_read,
	.write	= device_write,
};



void unregister_device(void)
{

	int i;

	dev_t              devno ;

	printk( KERN_NOTICE "Simple-driver: unregister_device() is called\n" );

	if( g_device_file_major_number != 0)
	{
		//cdev_del(my_cdevp);
		//cdev_del(&my_cdev);

		for( i = TIMER_FIRST_MINOR; i < TIMER_FIRST_MINOR + NUMBER_OF_TM_DEVICES; ++i )
		{
			cdev_del( &(g_Structs[i].m_cdev) );
			devno = MKDEV(g_device_file_major_number, i);
			unregister_chrdev_region(devno, 1);
		}


		//devno = MKDEV(device_file_major_number, scull_minor);
		//unregister_chrdev_region(devno, scull_nr_devs);
	}
}



static int __init register_device(void)
{

	int i;

	int devno, result = 0;
	dev_t devt              = 0;

	printk( KERN_NOTICE "Simple-driver: register_device() is called.\n" );

	result = alloc_chrdev_region(&devt, TIMER_FIRST_MINOR, NUMBER_OF_TM_DEVICES, DRIVER_NAME);

	printk(KERN_NOTICE "%s\n",DRIVER_NAME);

	//test4_class = class_create(THIS_MODULE, DRIVER_NAME);

	if( result < 0 )
	{
		printk( KERN_WARNING "Simple-driver:  can\'t register character device with errorcode = %i", result );
		return result;
	}
	g_device_file_major_number = MAJOR(devt);

	//// This shoud be tried
	//my_cdevp = cdev_alloc(  );
	//cdev_init(my_cdevp, &simple_driver_fops);
	//devno = MKDEV( device_file_major_number, scull_minor );
	//result = cdev_add(my_cdevp, devno, scull_nr_devs);

	for( i = TIMER_FIRST_MINOR; i < TIMER_FIRST_MINOR + NUMBER_OF_TM_DEVICES; ++i )
	{
		g_Structs[i].m_nMinor = i;
		cdev_init(&g_Structs[i].m_cdev, &simple_driver_fops);
		devno = MKDEV( g_device_file_major_number, i );
		cdev_add(&g_Structs[i].m_cdev, devno, 1);
	}

	//cdev_init(&my_cdev, &simple_driver_fops);
	//devno = MKDEV( device_file_major_number, scull_minor );
	//result = cdev_add(&my_cdev, devno, scull_nr_devs);

	if (result)
	{
		printk(KERN_NOTICE "Error %d adding devno%d", result, devno);
		return 1;
	}

	//test4_class = class_create(THIS_MODULE, DRIVER_NAME);

	printk( KERN_NOTICE "Simple-driver: registered character device with major number = %i and minor numbers 0...%i\n"
		 , g_device_file_major_number, NUMBER_OF_TM_DEVICES );

	//device_create(test4_class, NULL, MKDEV(g_device_file_major_number,TIMER_FIRST_MINOR),
      //            &g_Structs[0].m_cdev, "test4%d", TIMER_FIRST_MINOR);

	return 0;
}


module_init(register_device);
module_exit(unregister_device);
