#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/pci_regs.h>
#include <linux/pci.h>				/* pci_device_id  */


static int device_file_major_number = 0;
static const char device_name[] = "test_drv";

static const char    g_s_Hello_World_string[] = "Hello world from kernel mode!\n\0";
static const ssize_t g_s_Hello_World_size = sizeof(g_s_Hello_World_string);


void unregister_device(void)
{
	printk( KERN_NOTICE "Simple-driver: unregister_device() is called" );
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

	u16 vendor_id;//
	u16 device_id;//
	u8  revision;//
	u8  irq_line;//
	u8  irq_pin;//
	u16 subvendor_id;//
	u16 subdevice_id;//
	u16 class_code;//
	int i = 0;

	struct pci_dev *pPCI_tmp, *pPCI = pci_get_device( PCI_ANY_ID, PCI_ANY_ID,NULL);

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

		printk("%i.    =================================\n",++i);
		printk("vendor_id    = %i\n",vendor_id);
		printk("device_id    = %i\n",device_id);
		printk("subvendor_id = %i\n",subvendor_id);
		printk("subdevice_id = %i\n",subdevice_id);
		printk("class_code   = %i\n",class_code);
		printk("revision     = %i\n",revision);
		printk("irq_line     = %i\n",irq_line);
		printk("irq_pin      = %i\n\n",irq_pin);

		if(0)
		{
			break;
		}

		pci_dev_put(pPCI);
		pPCI_tmp= pci_get_device( PCI_ANY_ID, PCI_ANY_ID,pPCI );
		pPCI = pPCI_tmp;
	}

	/*int result = 0;

	//int low32,high32;
	//rdtsc(low32,high32);
	int low32;
	rdtscl(low32);
	cycles_t cycles = get_cycles();

	printk( KERN_NOTICE "Simple-driver: register_device() is called." );

	result = register_chrdev( 0, device_name, &simple_driver_fops );
	if( result < 0 )
	{
		printk( KERN_WARNING "Simple-driver:  can\'t register character device with errorcode = %i", result );
		return result;
	}

	device_file_major_number = result;
	printk( KERN_NOTICE "Simple-driver: registered character device with major number = %i and minor numbers 0...255"
		 , device_file_major_number );*/

	return 0;
}

module_init(register_device);
module_exit(unregister_device);

