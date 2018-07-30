#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/pci_regs.h>

#include <asm/msr.h>
#include <linux/timex.h>

static int device_file_major_number = 0;
static const char device_name[] = "test_drv";

static const char    g_s_Hello_World_string[] = "Hello world from kernel mode!\n\0";
static const ssize_t g_s_Hello_World_size = sizeof(g_s_Hello_World_string);


void unregister_device(void)
{
	printk( KERN_NOTICE "Simple-driver: unregister_device() is called\n" );
	if(device_file_major_number != 0)
	{
		unregister_chrdev(device_file_major_number, device_name);
	}
}


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



static struct file_operations simple_driver_fops =
{
	.owner   = THIS_MODULE,
	.read    = device_file_read,
	.write	= device_write,
};





static int register_device(void)
{
		int result = 0;

		//int low32,high32;
		//rdtsc(low32,high32);
		//int low32;
		//rdtscl(low32);
		//cycles_t cycles = get_cycles();

		printk( KERN_NOTICE "Simple-driver: register_device() is called.\n" );

		result = register_chrdev( 0, device_name, &simple_driver_fops );
		if( result < 0 )
		{
			printk( KERN_WARNING "Simple-driver:  can\'t register character device with errorcode = %i", result );
			return result;
		}

		device_file_major_number = result;
		printk( KERN_NOTICE "Simple-driver: registered character device with major number = %i and minor numbers 0...255\n"
			 , device_file_major_number );

		return 0;
}

module_init(register_device);
module_exit(unregister_device);
