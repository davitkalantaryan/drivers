#define HESS_DEBUG_KERNEL_BUFFER_LENGTH 800

struct dentry *dirret;
struct dentry *fileret;
struct dentry *u64int;
struct dentry *u64hex;

char ker_buf[HESS_DEBUG_KERNEL_BUFFER_LENGTH];
int filevalue;
u64 intvalue;
u64 hexvalue;

static ssize_t debug_stats_read(struct file *fp, char __user *user_buffer, size_t count, loff_t *position)
{
	long diff_mHz;
	if (g_srqDevice.debug_data.diff_us > 0) { diff_mHz = 1000000000 / g_srqDevice.debug_data.diff_us; }
	else { diff_mHz = 0; }

	long triggerRate_mHz;
	if (g_srqDevice.debug_data.triggerRate > 0) { triggerRate_mHz = 1000000000 / g_srqDevice.debug_data.triggerRate; }
	else { triggerRate_mHz = 0; }

	long unsigned trigger_counter = smc_bus_read16(BASE_HESS1U_DRAWER_READOUT + OFFS_HESS1U_DRAWER_READOUT_TRIGGERACCEPTCOUNTERHIGH) * 65536 + smc_bus_read16(BASE_HESS1U_DRAWER_READOUT + OFFS_HESS1U_DRAWER_READOUT_TRIGGERACCEPTCOUNTERLOW);
	u16 resetTime = smc_bus_read16(OFFS_HESS1U_DRAWER_READOUT_TRIGGERACCEPTCOUNTERRESETTIME);
	long unsigned rate_scale = 0;
	long unsigned irq_rate_mHz = 0;
	long unsigned irq_rate_us = 0;
	long unsigned irq_rate_Hz = 0;
	long unsigned averagingTime = 0;
	if (resetTime > 0)
	{
		rate_scale = HESS1U_DRAWER_READOUT_TRIGGERCOUNTERRESETTIMESEC * 1000 / resetTime;
		if (rate_scale > 0)
		{
			averagingTime = 1000000 / rate_scale;
			irq_rate_mHz = trigger_counter * rate_scale;
			if (irq_rate_mHz > 0)
			{
				irq_rate_us = 1000000000 / irq_rate_mHz;
				irq_rate_Hz = irq_rate_mHz / 1000;
			}
		}
	}

	size_t len = 0;
	len += sprintf(ker_buf + len, "irq rate:                 %luus == %lu,%03luHz\n", g_srqDevice.debug_data.diff_us, diff_mHz / 1000, diff_mHz % 1000);
	len += sprintf(ker_buf + len, "trigger rate software:    %luus == %lu,%03luHz\n", g_srqDevice.debug_data.triggerRate, triggerRate_mHz / 1000, triggerRate_mHz % 1000);
	len += sprintf(ker_buf + len, "trigger rate hardware:    %luus == %lu,%03luHz (avg@%lums)\n", irq_rate_us, irq_rate_Hz, irq_rate_mHz % 1000, averagingTime);
	len += sprintf(ker_buf + len, "total_irqs:               %u\n", g_srqDevice.debug_data.total_irq_count);
	len += sprintf(ker_buf + len, "max words in fifo:        %u\n", g_srqDevice.debug_data.max_fifo_count);
	len += sprintf(ker_buf + len, "words in fifo:            %u\n", g_srqDevice.debug_data.fifo_count);
	len += sprintf(ker_buf + len, "words in fifo:            %u\n", smc_bus_read16(BASE_HESS1U_DRAWER_READOUT + OFFS_HESS1U_DRAWER_READOUT_EVENTFIFOWORDCOUNT));
	len += sprintf(ker_buf + len, "eventcounter mismatches:  %u \t%u +1 != %u\n", g_srqDevice.debug_data.eventcounter_mismatch, g_srqDevice.debug_data.eventcounter_mismatch_old, g_srqDevice.debug_data.eventcounter_mismatch_new);
	//	len += sprintf(ker_buf + len, "length_mismatch:          %u\n", g_srqDevice.debug_data.length_mismatch);
	len += sprintf(ker_buf + len, "times sync losts:         %u\n", g_srqDevice.debug_data.sync_lost);
	len += sprintf(ker_buf + len, "fifo unexpected empty:    %u\n", g_srqDevice.debug_data.fifo_empty);
	len += sprintf(ker_buf + len, "bytes droped:             %u\n", g_srqDevice.debug_data.bytes_droped);
	len += sprintf(ker_buf + len, "current eventcounter:     %u\n", g_srqDevice.debug_data.eventcounter);
	len += sprintf(ker_buf + len, "samples lost (fifo full): %u\n", smc_bus_read16(BASE_HESS1U_DRAWER_READOUT + OFFS_HESS1U_DRAWER_READOUT_FIFOFULLCOUNT));
	len += sprintf(ker_buf + len, "samples 0:                %u\n", g_srqDevice.debug_data.samples_0);
	len += sprintf(ker_buf + len, "samples 16:               %u\n", g_srqDevice.debug_data.samples_16);
	len += sprintf(ker_buf + len, "samples 32:               %u\n", g_srqDevice.debug_data.samples_32);
	len += sprintf(ker_buf + len, "samples other:            %u\n", g_srqDevice.debug_data.samples_other);
	len += sprintf(ker_buf + len, "last frame length was:    %u\n", g_srqDevice.debug_data.frame_length);
	len += sprintf(ker_buf + len, "unknown_type:             %u\n", g_srqDevice.debug_data.unknown_type);
	len += sprintf(ker_buf + len, "chars left:~%u\n", HESS_DEBUG_KERNEL_BUFFER_LENGTH - len - 20);

	if (len > HESS_DEBUG_KERNEL_BUFFER_LENGTH)
	{
		sprintf(ker_buf, "HESS_DEBUG_KERNEL_BUFFER_LENGTH exceeded\n\n");
		ERR("HESS_DEBUG_KERNEL_BUFFER_LENGTH exceeded\n");
	}

	return simple_read_from_buffer(user_buffer, count, position, ker_buf, min((size_t)HESS_DEBUG_KERNEL_BUFFER_LENGTH, len));
}

static ssize_t debug_stats_write(struct file *fp, const char __user *user_buffer, size_t count, loff_t *position)
{
	if (count > HESS_DEBUG_KERNEL_BUFFER_LENGTH)
		return -EINVAL;

	return simple_write_to_buffer(ker_buf, HESS_DEBUG_KERNEL_BUFFER_LENGTH, position, user_buffer, count);
}

static const struct file_operations fops_debug_stats =
{
	.read = debug_stats_read,
	.write = debug_stats_write,
};

int hess_debugfs_init(void)
{
	//printk("\n\nhess_debugfs_init()\n\n");
	dirret = debugfs_create_dir("hess", NULL); // create a directory by the name dell in /sys/kernel/debugfs
	fileret = debugfs_create_file("stats", 0644, dirret, &filevalue, &fops_debug_stats);// create a file in the above directory This requires read and write file operations
	if (!fileret)
	{
		printk("error creating stats file");
		return (-ENODEV);
	}

	u64int = debugfs_create_u64("number", 0644, dirret, &intvalue);// create a file which takes in a int(64) value
	if (!u64int)
	{
		printk("error creating int file");
		return (-ENODEV);
	}

	u64hex = debugfs_create_x64("hexnum", 0644, dirret, &hexvalue); // takes a hex decimal value
	if (!u64hex)
	{
		printk("error creating hex file");
		return (-ENODEV);
	}

	return 0;
}

void hess_debugfs_remove(void)
{
	debugfs_remove_recursive(dirret);
}
