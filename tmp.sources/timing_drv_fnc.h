/*
 *	File: tamc200_fnc.h
 *
 *	Created on: Dec 10, 2013
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 */
#ifndef __timing_drv_fnc_h__
#define __timing_drv_fnc_h__

#define	DEVICE_NAME		"tamc200"

#ifndef TAMC200_NR_DEVS
#define TAMC200_NR_DEVS 20
#endif
#ifndef TAMC200_NR_SLOTS
#define TAMC200_NR_SLOTS 3   
#endif
#ifndef TAMC200_NR_FDEVS
#define TAMC200_NR_FDEVS (TAMC200_NR_DEVS * TAMC200_NR_SLOTS +TAMC200_NR_DEVS)   
#endif

//#define DEVNAME "tamc200"	/* name of device */
#define DEVNAME "tamc200"	/* name of device */
#define DRV_NAME "timer_drv"	/* name of device */

#define TAMC200_VENDOR_ID    0x10B5	/* TEWS vendor ID */
#define TAMC200_DEVICE_ID    0x9030	/* IP carrier board device ID */
#define TAMC200_SUBVENDOR_ID 0x1498	/* TEWS vendor ID */
#define TAMC200_SUBDEVICE_ID 0x80C8	/* IP carrier board device ID */

 /* IP module types */
#define IPTIMER 1  /*IP8TIMER mdoule*/
#define IPTIMER_IRQ_MODE_OFF	 	0
#define IPTIMER_IRQ_MODE_ON	 	1
#define IPTIMER_IRQ_MODE_DEFAULT  	IPTIMER_IRQ_MODE_OFF
#define IPTIMER_MAX_SERVER	 	0x25
#define IPTIMER_ID	 	        0x43425049
#define IPTIMER_IRQ_VEC	 	        0xF0

#define IPTIMER_IRQ_CH3       		0x0008


struct tamc200_dev;

struct rw_iptimer {
    volatile u_short        wr16 [64];
};
typedef struct rw_iptimer rw_iptimer;

struct server_signal{
   pid_t           f_ServerPref;
   u_short	   f_IrqProc[5];
   u_short	   f_IrqFlag;
   u_short	   f_CurSig ;
};
typedef struct server_signal t_ServerSignal;


struct tamc200_slot_dev
{
	Pcie_gen_dev		m_PcieGen;

	struct tamc200_dev*	ptamc200_dev;


	u32                ip_module_id;
	u32                ip_module_type;
	int                ip_on;

	void __iomem *	ip_id; //?????????????

	int                irq_on;
	int                irq_lev;
	int                irq_vec;
	int                irq_sens;

	rw_iptimer         reg_map;
	int                server_signals;
	t_ServerSignal     server_signal_stack [IPTIMER_MAX_SERVER];
	u32                irq_status;
	u16                irq_mask;
	u16                irq_flag;
	u32                irq_sec;
	u32                irq_usec;
	u32                irq_sec_hi;
	u32                irq_usec_hi;
	u32                irq_status_hi;
	u32                irq_delay;
	u32                irq_delay_mask;
	u16                irq_flag_hi;
	u16                irq_mask_hi;
	u16                irq_vect_hi;
	u32                event_mask;
	u32                event_value;
	u16                gen_event_low;
	u16                gen_event_hi;
	u16                irq_jitter_test;
    
};



struct tamc200_dev
{
	Pcie_gen_dev	m_PcieGen;
    
	struct pci_dev     *tamc200_pci_dev;
	struct work_struct tamc200_work;
	u16                vendor_id;
	u16                device_id;
	u16                subvendor_id;
	u16                subdevice_id;
	u16                class_code;
	u8                 revision;
	int                ip_on;
	spinlock_t         irq_lock;
	int                irq_on;
	u16                irq_mode;
	u16                irq_flag;
	u8                 irq_line;
	u8                 irq_pin;
	u8                 irq_slot;
	u32                pci_dev_irq;
	u32                softint;
	u32                count;
	int                bus_func;
	u32                devfn;
	u32                busNumber;
	u32                devNumber;
	u32                funcNumber;

	int                tamc200_all_mems ;
	int                waitFlag;


	struct tamc200_slot_dev ip_s[TAMC200_NR_SLOTS];
	u_int TAMC200_PPGA_REV ;
	u_int TAMC200_A_ON     ;
	u_int TAMC200_B_ON     ;
	u_int TAMC200_C_ON     ;
	u_int TAMC200_A_TYPE   ;
	u_int TAMC200_B_TYPE   ;
	u_int TAMC200_C_TYPE   ;
	u_int TAMC200_A_ID     ;
	u_int TAMC200_B_ID     ;
	u_int TAMC200_C_ID     ;

	u32                irq_sec_hi;
	u32                irq_usec_hi;
	u32                irq_status_hi;
	u16                irq_flag_hi;
	u16                irq_mask_hi;
	u16                irq_vect_hi;

};

#endif /* __timing_drv_fnc_h__ */
