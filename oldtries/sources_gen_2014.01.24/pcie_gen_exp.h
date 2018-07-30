#ifndef __PCIE_GEN_H__
#define __PCIE_GEN_H__

#include "includes_gen.h"


#ifndef	_COMMON_DEFINATIONS

//#define		_DRIVER_NAME_				"tamc200"
//#define		DEVNAME						"tamc200"	/* name of device */
//#define		_DRIVER_NAME_				"pcie_gen"
//#define		DEVNAME						"pcie_gen"	/* name of device */
#define		_COMMON_DEFINATIONS
#define		MAX_INTR_NUMB				10

#endif/* #ifndef	_COMMON_DEFINATIONS */

typedef struct SInteruptStr
{
	pid_t				m_nPID;
	const struct cred*	m_pCred;
}SInteruptStr;

typedef struct SignalStruct
{
	pid_t		m_ProgPID;
	u_short		m_usnSpecSig;
	int			m_nSpecSig;
}SignalStruct;


typedef struct MemoryStruct
{
	u32				m_mem_base;
	u32				m_mem_base_end;
	u32				m_mem_base_flag;
	void __iomem *	m_memory_base;
	loff_t			m_rw_off;
}MemoryStruct;



typedef struct Pcie_gen_dev
{
	int					m_dev_num;
	int					m_dev_minor;
	int					m_dev_sts;
	int					m_slot_num;
	struct cdev			m_cdev;     /* Char device structure      */
	struct mutex		m_dev_mut;  /* mutual exclusion semaphore */

    MemoryStruct		m_Memories[NUMBER_OF_BARS];
	
	int					m_nInteruptWaiters;
	struct siginfo		m_info;
	struct SInteruptStr	m_IrqWaiters[MAX_INTR_NUMB];

}Pcie_gen_dev;



struct str_pcie_gen
{
	Pcie_gen_dev				m_PcieGen;
	device_irq_rw2				m_rst_irq[NUMBER_OF_BARS];

	int							m_nSigVal;

	struct pci_dev*				pcie_gen_pci_dev;
	struct work_struct			pcie_gen_work;
	struct workqueue_struct*	pcie_gen_workqueue;
	u16							vendor_id;
	u16							device_id;
	u16							subvendor_id;
	u16							subdevice_id;
	u16							class_code;
	u8							revision;
	spinlock_t					irq_lock;
	int							irq_on;
	u16							irq_mode;
	u16							irq_flag;
	u8							irq_line;
	u8							irq_pin;
	u8							irq_slot;
	u32							pci_dev_irq;
	u32							softint;
	u32							count;
	int							bus_func;
	u32							devfn;
	u32							busNumber;
	u32							devNumber;
	u32							funcNumber;

	int							pcie_gen_all_mems ;
	int							waitFlag;

};


//extern int GetChunkSize1(u_int a_unMode);
extern ssize_t General_Read1(struct Pcie_gen_dev* a_dev, char __user *buf, size_t count);
extern ssize_t General_Write1(struct Pcie_gen_dev* a_dev, const char __user *buf, size_t count);
extern void RemovePID1( struct Pcie_gen_dev* a_dev, pid_t a_nPID );
extern void PcieGen_do_work1(struct work_struct *work_str);
extern long  PcieGen_ioctl1(struct file *filp, unsigned int cmd, unsigned long arg);
extern long  General_ioctl( struct Pcie_gen_dev* a_pDev, unsigned int cmd, unsigned long arg);
extern long  General_ioctl2( struct str_pcie_gen* a_pDev, unsigned int cmd, unsigned long arg);
extern int  General_GainAccess1( struct str_pcie_gen* a_pDev, int a_MAJOR, struct class* a_class, const char* a_DRV_NAME);
extern void Gen_ReleaseAccsess(struct str_pcie_gen* a_pDev, int a_MAJOR, struct class* a_class);

#endif /* __PCIE_GEN_H__ */
