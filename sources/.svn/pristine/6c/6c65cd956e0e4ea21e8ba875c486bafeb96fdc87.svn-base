/*
 *	File: dma_test_exp.h
 *
 *	Created on: Oct 22, 2014
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *	
 *
 */
#ifndef __dma_test_exp_h__
#define __dma_test_exp_h__

#include "version_dependence_dma.h"
#include <linux/spinlock.h>

#define	PCIE_GEN_NR_DEVS	1
#define	DEVNAME				"dma_test0"	/* name of device entry				*/
#define	DRV_NAME			"dma_test0_drv"

typedef struct SCriticalRegion
{
	/*int					m_nInCriticalregion;
	int					m_nShouldFreeSemaphore;
	long				m_lnPID;
	u64					m_ulnJiffiesTmOut;
	u64					m_ullnStartJiffi;*/

	struct semaphore	m_semaphore;
	spinlock_t			m_spinlock;

	long				m_lnPID;
	int					m_nInCriticalregion;
	int					m_nLocked;
	int					m_nShouldFreeSemaphore;
	u64					m_ulnJiffiesTmOut;
	u64					m_ullnStartJiffi;

}SCriticalRegion;

typedef struct Sdma_test0
{
	struct cdev			m_cdev;     /* Char device structure      */
	SCriticalRegion		m_DevLock;
	
}Sdma_test0;


extern void	InitNewLock(SCriticalRegion* lock, long timeoutMs);
extern void	DestroyNewLock(SCriticalRegion* lock);
extern void LeaveCritRegion(SCriticalRegion* lock, int semaphoreUp);
extern int EnterCritRegion(SCriticalRegion* lock, int longTime);


#endif /* __dma_test_exp_h__ */
