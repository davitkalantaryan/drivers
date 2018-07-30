/*
*	Created on: Jun 22, 2015
*	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
*
*	New source file for hess driver
*   Initializing DMA engine to make several
*   DMA transaction implemented in this source
*
*/
#ifndef __dma_engine_header_h__
#define __dma_engine_header_h__

#include <linux/printk.h>
#include <linux/module.h>
#include <linux/dmaengine.h>

typedef struct SSingleTransfer
{
	int			m_nSGcount;
	dma_addr_t	m_dest0;
	dma_addr_t	m_destMin;
	dma_addr_t	m_destMax;
	dma_addr_t	m_src;
	size_t		m_transferLength;
	size_t		m_dstBuffIncrement;

	dma_addr_t	m_FifoSwitchDst;
	dma_addr_t	m_FifoSwitchSrc;
	size_t		m_FifoSwtchLen;
}SSingleTransfer;

#if 0
dma_addr_t a_dest, dma_addr_t a_destMin, dma_addr_t a_destMax, dma_addr_t a_FifoSwitchDst,
dma_addr_t a_src, dma_addr_t a_FifoSwitchSrc
#endif


extern int AsyncAtDMAreadFifoFast(void* dmaEngine, int transfersCount,
	dma_addr_t dest, dma_addr_t destMin, dma_addr_t destMax, dma_addr_t fifoSwitchDst,
	dma_addr_t src, dma_addr_t fifoSwitchSrc, size_t len, size_t step, size_t fifoSwitchLen,
	void* pCallbackData, void(*fpCallback)(void*));

extern void* GetAtDMAengineFast(void);

extern void FreeAtDMAengineFast(void* dmaEngine);

extern struct device* GetAtDMAdeviceFast(void* dmaEngine);

extern void StopAtDMAengineFast(void* a_pEngine);

extern int AsyncAtDMAreadFast(void* dmaEngine, int rwNumber,
						dma_addr_t dests, dma_addr_t destMin, dma_addr_t destMax, dma_addr_t srcDev,
						size_t transferLen, size_t destBufIncrement, void* pCallbackData, void(*fpCallback)(void*));

extern int AsyncAtDMAreadVectFast(void* dmaEngine, int transferNumber, SSingleTransfer* pTransferInfo,
								void* pCallbackData, void(*fpCallback)(void*));

extern int AsyncAtDMAreadFifoFast(void* a_dmaEngine, int a_nTransfersCount,
	dma_addr_t a_dest, dma_addr_t a_destMin, dma_addr_t a_destMax, dma_addr_t a_FifoSwitchDst,
	dma_addr_t a_src, dma_addr_t a_FifoSwitchSrc, 
	size_t a_len, size_t a_step, size_t a_fifoSwitchLen,
	void* a_pCallbackData, void(*a_fpCallback)(void*));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern void* GetAtDMAengineSlow(void);

extern void FreeAtDMAengineSlow(void* a_pEngine);

extern struct device* GetAtDMAdeviceSlow(void* a_pEngine);

extern void StopAtDMAengineSlow(void* a_pEngine);

extern int AsyncAtDMAreadSlow(void* dmaEngine, int rwNumber,
	dma_addr_t dests, dma_addr_t destMin, dma_addr_t destMax, dma_addr_t srcDev,
	size_t transferLen, size_t destBufIncrement, void* pCallbackData, void(*fpCallback)(void*));

extern int AsyncAtDMAreadVectSlow(void* dmaEngine, int transferNumber, SSingleTransfer* pTransferInfo,
	void* pCallbackData, void(*fpCallback)(void*));

extern int AsyncAtDMAreadFifoSlow(void* a_dmaEngine, int a_nTransfersCount,
	dma_addr_t a_dest, dma_addr_t a_destMin, dma_addr_t a_destMax, dma_addr_t a_FifoSwitchDst,
	dma_addr_t a_src, dma_addr_t a_FifoSwitchSrc, size_t a_len, size_t a_step, size_t a_fifoSwitchLen,
	void* a_pCallbackData, void(*a_fpCallback)(void*));


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern void* GetAtDMAengineVeryFast(void);

extern void FreeAtDMAengineVeryFast(void* dmaEngine);

extern struct device* GetAtDMAdeviceVeryFast(void* dmaEngine);

extern void StopAtDMAengineVeryFast(void* a_pEngine);

extern int AsyncAtDMAreadVeryFast(void* dmaEngine, int rwNumber,
	dma_addr_t dests, dma_addr_t destMin, dma_addr_t destMax, dma_addr_t srcDev,
	size_t transferLen, size_t destBufIncrement, void* pCallbackData, void(*fpCallback)(void*));

extern int AsyncAtDMAreadVectVeryFast(void* dmaEngine, int transferNumber, SSingleTransfer* pTransferInfo,
	void* pCallbackData, void(*fpCallback)(void*));

extern int AsyncAtDMAreadFifoVeryFast(void* a_dmaEngine, int a_nTransfersCount,
	dma_addr_t a_dest, dma_addr_t a_destMin, dma_addr_t a_destMax, dma_addr_t a_FifoSwitchDst,
	dma_addr_t a_src, dma_addr_t a_FifoSwitchSrc, size_t a_len, size_t a_step, size_t a_fifoSwitchLen,
	void* a_pCallbackData, void(*a_fpCallback)(void*));


#define _INIT_FORM_COMMON_	"Fl:\"%s\";Fn:\"%s\";L:%d. "
#define _INIT_FORM_  "Md:\"%s\"" _INIT_FORM_COMMON_

#ifdef WIN32
//#pragma warning(disable : 4996) 
#define dma_addr_t int
#define _LAST_CHAR_		'\\'
#else
#define _LAST_CHAR_		'/'
#endif  /*  #ifdef WIN32 */

#define		PRINT_REP_ERR_K_1(func,arg1,...) \
		{ \
		func(arg1 _INIT_FORM_,THIS_MODULE->name, \
				strrchr(__FILE__,_LAST_CHAR_)?strrchr(__FILE__,_LAST_CHAR_)+1:"Unknown file", \
				__FUNCTION__,__LINE__); \
		func(KERN_CONT __VA_ARGS__); \
		}

#define		PRINT_KERNEL_MSG(arg1,...) PRINT_REP_ERR_K_1(printk,arg1,__VA_ARGS__)

#define		INFOB(...) PRINT_KERNEL_MSG(KERN_INFO,__VA_ARGS__)
#define		ALERTB(...) PRINT_KERNEL_MSG(KERN_ALERT,__VA_ARGS__)
#define		DEBUGB1(...) PRINT_KERNEL_MSG(KERN_DEBUG,__VA_ARGS__)
#define		ERRB(...)  PRINT_KERNEL_MSG(KERN_ERR,__VA_ARGS__)
#define		DEBUGB2(...) \
do{ERR("++++++++++++++++++++++++++++++++++++++++");printk(KERN_CONT __VA_ARGS__);}while(0)
#define		WARNINGB(...)  PRINT_KERNEL_MSG(KERN_WARNING,__VA_ARGS__)
#define		DEBUGB3(...)  do{if(g_nPrintDebugInfo){ALERT(__VA_ARGS__);}}while(0)
#define		ERR_IRQB(str)


#endif  /* #ifndef __dma_engine_header_h__ */
