/*
 *	File: version_dependence_dma.h
 *
 *	Created on: Oct 22, 2014
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *
 *
 */
#ifndef __version_dependence_dma_h__
#define __version_dependence_dma_h__

#ifdef WIN32
#include "windows_headers_for_lk/windows_definations_for_lk.h"
#endif

#include "includes_dma_test.h"

#ifndef KERNEL_VERSION
#define KERNEL_VERSION(x,y,z)	(x)+(y)+(z)
#endif
#ifndef LINUX_VERSION_CODE
#define LINUX_VERSION_CODE KERNEL_VERSION(3,5,0)
#endif


#if		(LINUX_VERSION_CODE>=100)	&&(LINUX_VERSION_CODE<197895)	// kernel 2.6.18


static inline struct proc_dir_entry *vers_create_proc_entry(
	const char* a_name, umode_t a_mode,void* a_pData,
	int(*a_procinfo)(char*,char**,off_t,int,int*,void*)
	)
{
	struct proc_dir_entry * pRet = create_proc_entry(a_name,a_mode, 0);
	pRet->read_proc = a_procinfo;
	pRet->data = a_pData;

	return pRet;
}


#elif	(LINUX_VERSION_CODE>=197895)	&&(LINUX_VERSION_CODE<199945)	// kernel 3.5.0


static inline struct proc_dir_entry *vers_create_proc_entry(
	const char* a_name, umode_t a_mode,void* a_pData,
	int(*a_procinfo)(char*,char**,off_t,int,int*,void*)
	)
{
	struct proc_dir_entry * pRet = create_proc_entry(a_name,a_mode, 0);
	pRet->read_proc = a_procinfo;
	pRet->data = a_pData;

	return pRet;
}



#else																	// kernel 3.13.0


#define		__devinitdata
#define		__devinit
#define		__devexit
#define		__devexit_p(x)		x

//static int pcie_gen_procinfo(char *buf, char **start, off_t fpos, int lenght, int *eof, void *data)
/*
static int pcie_gen_procinfo(char *buf, char **start, off_t fpos, int lenght, int *eof, void *data)
{
	char *p;
	struct Pcie_gen_dev *dev = data;

	p = buf;
	p += sprintf(p,"Driver Version:\t%i.%i\n", PCIE_GEN_DRV_VER_MAJ, PCIE_GEN_DRV_VER_MIN);
	p += sprintf(p,"STATUS Revision:\t%i\n", dev->m_dev_sts);

	*eof = 1;
	return p - buf;
}*/

static ssize_t test_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	int nCountRem = 17-((int)*f_pos);
	int nCount2 = (int)count<nCountRem ? count : nCountRem;
	if(nCount2<=0)return 0;

	copy_to_user( buf,"Not implemented\n"+(*f_pos),nCount2);
	*f_pos += nCount2;
	return nCount2;
} 


static struct file_operations proc_fops = {
	.owner			=	THIS_MODULE,
	.read			=	test_read,
	//.write		=	test_write,
	//.ioctl		=	pcie_gen_ioctl,
	//.unlocked_ioctl	=	pcie_gen_ioctl,
	//.compat_ioctl	=	pcie_gen_ioctl,
	//.open			=	pcie_gen_open,
	//.release		=	pcie_gen_release,
};



static inline struct proc_dir_entry *vers_create_proc_entry(
	const char* a_name, umode_t a_mode,void* a_pData,
	int(*a_procinfo)(char*,char**,off_t,int,int*,void*)
	)
{
	struct proc_dir_entry * pRet = proc_create_data(a_name,a_mode,NULL,&proc_fops,a_pData);
	return pRet;
}

#endif



#endif  /* #ifndef __version_dependence_dma_h__ */
