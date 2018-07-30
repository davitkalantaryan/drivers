#ifndef __dma_test_kernel_user_h__
#define __dma_test_kernel_user_h__

#include <linux/types.h>
#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */

#define	CHANGE_DEV_NAME_PRVT			59
#define	GET_DEV_NAME_PRVT				60
#define	CHANGE_READ_FNC_PRVT			61

/* Use 'p' as magic number */
#define	DMA_TEST_GEN_IOC				'p'

#define CHANGE_DEV_NAME					_IOWR(DMA_TEST_GEN_IOC, CHANGE_DEV_NAME_PRVT, int)
#define GET_DEV_NAME					_IOWR(DMA_TEST_GEN_IOC, GET_DEV_NAME_PRVT, int)
#define CHANGE_READ_FNC					_IOWR(DMA_TEST_GEN_IOC, CHANGE_READ_FNC_PRVT, int)

#define PCIE_GEN_IOC_MINNR				40
#define PCIE_GEN_IOC_MAXNR				93


#endif  /* #ifndef __dma_test_kernel_user_h__ */
