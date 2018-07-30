/*
 *	File: dma_test_exp.c
 *
 *	Created on: Oct 22, 2013
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *	This file contain definitions of all exported functions
 *
 */

//#include "dma_test_exp.h"
//#include <linux/syscalls.h>
#include "dma_test_exp.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,28)
#define kill_proc(p,s,v) kill_pid(find_vpid(p), s, v)
#endif

/*
struct semaphore	m_semaphore;
spinlock_t			m_spinlock;

long				m_lnPID;
int					m_nInKernelSpace;
u64					m_ulnJiffiesTmOut;
u64					m_ullnStartJiffi;
*/


void InitNewLock(SCriticalRegion* a_pLock, long a_lnTimeoutMs)
{
	/*a_pLock->m_nInCriticalregion = 0;
	a_pLock->m_nShouldFreeSemaphore = 0;
	a_pLock->m_ulnJiffiesTmOut = msecs_to_jiffies(a_lnTimeoutMs);
	PRINT_KERN_ALERT("jiffies = %ld\n", (long)a_pLock->m_ulnJiffiesTmOut);
	//spin_lock_init(&s_dma_test0.m_spinlock);
	a_pLock->m_lnPID = -1;

	sema_init(&a_pLock->m_semaphore, 1);*/

	memset(a_pLock, 0, sizeof(struct SCriticalRegion));
	
	a_pLock->m_lnPID = -1;
	a_pLock->m_nInCriticalregion = 0;
	a_pLock->m_ulnJiffiesTmOut = msecs_to_jiffies(a_lnTimeoutMs);

	sema_init(&a_pLock->m_semaphore, 1);
	spin_lock_init(&a_pLock->m_spinlock);

}
EXPORT_SYMBOL(InitNewLock);



void DestroyNewLock(SCriticalRegion* lock)
{
}
EXPORT_SYMBOL(DestroyNewLock);



void LeaveCritRegion(SCriticalRegion* a_pLock, int a_nSemaphoreUp)
{

	a_pLock->m_nInCriticalregion = 0;

	if (likely(a_nSemaphoreUp))
	{
		if (likely(a_pLock->m_lnPID != (long)current->group_leader->pid))
		{
			spin_lock(&a_pLock->m_spinlock);
			if (a_pLock->m_nLocked)
			{
				up(&a_pLock->m_semaphore);
				a_pLock->m_nLocked = 0;
			}
			spin_unlock(&a_pLock->m_spinlock);
		}
		
	}
	else if ( likely(a_pLock->m_lnPID == (long)current->group_leader->pid))
	{
		//a_pLock->m_nInCriticalregion = 0;

		if (a_pLock->m_nShouldFreeSemaphore)
		{
			a_pLock->m_lnPID = -1;
			spin_lock(&a_pLock->m_spinlock);
			if (a_pLock->m_nLocked)
			{
				up(&a_pLock->m_semaphore);
				a_pLock->m_nLocked = 0;
			}
			spin_unlock(&a_pLock->m_spinlock);
			kill_proc(a_pLock->m_lnPID, SIGUSR1, 0); // Inform the user space program
		} // end  if (a_pLock->m_nShouldFreeSemaphore)
	}
}
EXPORT_SYMBOL(LeaveCritRegion);



int EnterCritRegion(SCriticalRegion* a_pLock, int a_bLongTime)
{

	long lnPID = (long)current->group_leader->pid;

	if (unlikely(lnPID == a_pLock->m_lnPID))
	{
		u64 ullnCurJiffies = get_jiffies_64();
		if ((ullnCurJiffies - a_pLock->m_ullnStartJiffi) >= a_pLock->m_ulnJiffiesTmOut)
		{
			//PRINT_KERN_ALERT("Timeout!!!!!!\n");
			a_pLock->m_lnPID = -1;
			spin_lock(&a_pLock->m_spinlock);
			if (a_pLock->m_nLocked)
			{
				up(&a_pLock->m_semaphore);
				a_pLock->m_nLocked = 0;
			}
			a_pLock->m_nInCriticalregion = 0;
			spin_unlock(&a_pLock->m_spinlock);
			kill_proc(a_pLock->m_lnPID, SIGUSR1, 0); // Inform the user space program
			return -3;
		}
		a_pLock->m_nInCriticalregion = 1;
		return 0;

	} // if (unlikely(lnPID == a_pLock->m_lnPID))
	else
	{
		int nIter = 0;

		for (; nIter < 10; ++nIter)
		{
			if (unlikely(down_timeout(&a_pLock->m_semaphore, a_pLock->m_ulnJiffiesTmOut)))
			{
				//if ()
				spin_lock(&a_pLock->m_spinlock);
				if (a_pLock->m_nLocked)
				{
					up(&a_pLock->m_semaphore);
					a_pLock->m_nLocked = 0;
				}
				a_pLock->m_nInCriticalregion = 0;
				spin_unlock(&a_pLock->m_spinlock);

				kill_proc(a_pLock->m_lnPID, 9, 0);
				a_pLock->m_lnPID = -1;
			} // if (unlikely(down_timeout(&a_pLock->m_semaphore, a_pLock->m_ulnJiffiesTmOut)))
			else
			{
				if (unlikely(a_bLongTime))
				{
					a_pLock->m_lnPID = lnPID;
					a_pLock->m_ullnStartJiffi = get_jiffies_64();
				}
				spin_lock(&a_pLock->m_spinlock);
				a_pLock->m_nLocked = 1;
				a_pLock->m_nInCriticalregion = 1;
				spin_unlock(&a_pLock->m_spinlock);
				return 0;

			} // else  of  if (unlikely(down_timeout(&a_pLock->m_semaphore, a_pLock->m_ulnJiffiesTmOut)))

		} // for (; nIter < 10; ++nIter)

		return -ETIME;

	} // else of if (unlikely(lnPID == a_pLock->m_lnPID))



	return 0;
}
EXPORT_SYMBOL(EnterCritRegion);
