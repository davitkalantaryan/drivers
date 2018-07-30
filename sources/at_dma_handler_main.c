/*
*	Created on: Jun 22, 2015
*	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
*
*	New source file for hess driver
*   Initializing DMA engine to make several
*   DMA transaction implemented in this source
*
*/
#include <linux/module.h>
#include "dma_engine_header.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("desy");
MODULE_DESCRIPTION("driver for DMA engine ");



static int __init dma_engine_init(void)
{
	//WARN_ON_ONCE(1);
	return 0;
}

// Deinitialize the Kernel Module
static void __exit dma_engine_exit(void)
{
}

module_init(dma_engine_init);
module_exit(dma_engine_exit);
