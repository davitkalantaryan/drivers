/*
 * SIS8300 Typedefines
 *
 * everything userland needs to compile (hopefully)
 */

#ifndef SIS8300_DEFS_H_
#define SIS8300_DEFS_H_

/*
 * ioctl structs which will be passed as parameters
 * according to the called function
 * e.g.
 * REGISTER READ/WRITE
 * SERNUM READ
 * etc
 */

/* generic register access */
typedef struct t_sis8300_reg{
	uint32_t offset; /* offset from bar0 */
	uint32_t data;   /* data which will be read / written */
}sis8300_reg;

/* user irq with timeout */
typedef struct t_sis8300_usr_irq{
  uint32_t timeout;
  uint32_t status;
}sis8300_usr_irq;

/* daq irq with timeout */
typedef struct t_sis8300_daq_irq{
  uint32_t timeout;
  uint32_t status;
}sis8300_daq_irq;

/* daq done dma chain */
typedef struct t_sis8300_daq_dma{
	uint32_t daq_timeout;
	uint32_t read_len;
	char *read_buf;
	uint32_t status;
}sis8300_daq_dma;

/*
 * macro magic to retrieve the device struct from a file struct inside
 * read, write, ioctl and release
 */

#define SIS8300_DEV_FILE(filp) ((sis8300_dev *)(filp)->private_data)

/*
 * calls need to pass an unique identifier as a command to ioctl
 * these are defined here with macros
 */

#define SIS8300_MAGIC 's'

/* generic register read/write */
#define SIS8300_REG_READ			_IOWR(SIS8300_MAGIC, 0, sis8300_reg)
#define SIS8300_REG_WRITE			_IOWR(SIS8300_MAGIC, 1, sis8300_reg)
#define SIS8300_WAIT_USER_IRQ			_IOWR(SIS8300_MAGIC, 2, sis8300_usr_irq)
#define SIS8300_WAIT_DAQ_IRQ			_IOWR(SIS8300_MAGIC, 3, sis8300_daq_irq)
#define SIS8300_WAIT_DAQ_DMA_IRQ		_IOWR(SIS8300_MAGIC, 4, sis8300_daq_dma)

#define SIS8300_MAXNR			5

// the maximum amount of cards currently supported
#define SIS8300_MAXCARDS 16

#endif
