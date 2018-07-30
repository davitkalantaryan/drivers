/*
*	File: pciegen_io_base.h
*
*	Created on: May 12, 2013
*	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
*
*	This file contain definitions of all exported functions
*
*/
#ifndef __print_functions_h__
#define __print_functions_h__

#ifndef __pciegen_io_base_h__
#if defined(WIN32) && defined(KERN_DEV)
#include "windows_headers_for_lk/windows_definations_for_lk.h"
#endif

#ifdef WIN32
#define	snprintf _snprintf
#ifndef _IOWR
#define _IOWR(x1,x2,x3)	(x2)
#endif

#ifndef int8_t
typedef char int8_t;
#define int8_t int8_t
#endif //int8_t
#ifndef u_int8_t
typedef unsigned char u_int8_t;
#define u_int8_t u_int8_t
#endif //u_int8_t

#ifndef int16_t
typedef short int int16_t;
#define int16_t int16_t
#endif //int16_t
#ifndef u_int16_t
typedef unsigned short int u_int16_t;
#define u_int16_t u_int16_t
#endif //u_int16_t

#ifndef int32_t
typedef int int32_t;
#define int32_t int32_t
#endif //int32_t
#ifndef u_int32_t
typedef unsigned int u_int32_t;
#define u_int32_t u_int32_t
#endif //u_int32_t

#ifdef _MSC_VER
#define	_int_type_64_t_ __int64
#else
#define _int_type_64_t_ long long int
#endif   // #ifdef _MSC_VER

#ifndef int64_t
typedef _int_type_64_t_ int64_t;
#define int64_t int64_t
#endif //int64_t
#ifndef u_int64_t
typedef unsigned _int_type_64_t_ u_int64_t;
#define u_int64_t u_int64_t
#endif //u_int64_t

#else	/* #ifdef WIN32 */
#include <linux/types.h>
#include <linux/ioctl.h> /* needed for the _IOW etc stuff */
#endif	/* #ifdef WIN32 */

#ifdef KERNEL_VERSION
#include <linux/string.h>
#include <linux/kernel.h>
#else/* #ifdef KERNEL_VERSION */
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#ifdef WIN32
#include <WINDOWS.H>
#ifndef _WINSOCK2API_
#ifndef U_INT_DEFINED
typedef unsigned char   u_char;
typedef unsigned short  u_short;
typedef unsigned int    u_int;
typedef unsigned long   u_long;
#define U_INT_DEFINED
#endif	/* #ifndef U_INT_DEFINED */
#endif	/* #ifndef _WINSOCK2API_ */
#else	/* #ifdef WIN32 */
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#endif	/* #ifdef WIN32 */
#endif/* #ifdef KERNEL_VERSION */


#ifdef _WIN64
#ifndef WIN32
#define WIN32
#endif
#endif

/*#if !defined(WIN32) | (defined(_MSC_VER) & (_MSC_VER >= 1400))
#define VARIADIC_MACROS_DEFINED
#endif*/
#ifdef WIN32
#ifdef _MSC_VER
#if _MSC_VER >= 1400
#define VARIADIC_MACROS_DEFINED
#else
#undef VARIADIC_MACROS_DEFINED
#endif
#else
#define VARIADIC_MACROS_DEFINED
#endif
#else  /* #ifdef WIN32 */
#define VARIADIC_MACROS_DEFINED
#endif  /* #ifdef WIN32 */

#define _INIT_FORM_COMMON_	"Fl:\"%s\";Fn:\"%s\";L:%d. "

#ifdef WIN32
//#pragma warning(disable : 4996) 
#define _LAST_CHAR_		'\\'
#else
#define _LAST_CHAR_		'/'
#endif  /*  #ifdef WIN32 */


#ifdef VARIADIC_MACROS_DEFINED

#define ARG_N_PRVT_(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,N,...) N
#define NARG_PRVT_(...) ARG_N_PRVT_(__VA_ARGS__)
#define _NARG__PRVT(...) NARG_PRVT_(__VA_ARGS__,10,9,8,7,6,5,4,3,2,1,0)


#ifdef KERNEL_VERSION

#include <linux/module.h>
#define _INIT_FORM_  "Md:\"%s\"" _INIT_FORM_COMMON_


/* General macros for Kernel space */
#define		PRINT_REP_ERR_K_1(func,arg1,...) \
	{ \
		func(arg1 _INIT_FORM_,THIS_MODULE->name, \
				strrchr(__FILE__,_LAST_CHAR_)?strrchr(__FILE__,_LAST_CHAR_)+1:"Unknown file", \
				__FUNCTION__,__LINE__); \
		func(KERN_CONT __VA_ARGS__); \
	}


/* Macros for kernel space, that uses printk function for outputs */
#define		PRINT_KERNEL_MSG(arg1,...) PRINT_REP_ERR_K_1(printk,arg1,__VA_ARGS__)


/* Macros for printing kernel alerts */
#define		PRINT_KERN_ALERT(...) PRINT_KERNEL_MSG(KERN_ALERT,__VA_ARGS__)

#define		INFO2(...) PRINT_KERNEL_MSG(KERN_INFO,__VA_ARGS__)
#define		ALERT2(...) PRINT_KERNEL_MSG(KERN_ALERT,__VA_ARGS__)
#define		DEBUG1(...) PRINT_KERNEL_MSG(KERN_DEBUG,__VA_ARGS__)
#define		ERROR2(...)  PRINT_KERNEL_MSG(KERN_ERR,__VA_ARGS__)
#define		DEBUG2(...) \
do{ERR("++++++++++++++++++++++++++++++++++++++++");printk(KERN_CONT __VA_ARGS__);}while(0)
#define		DBG2(...)  PRINT_KERNEL_MSG(KERN_DEBUG,__VA_ARGS__)
#define		WARNING2(...)  PRINT_KERNEL_MSG(KERN_WARNING,__VA_ARGS__)
#define		DEBUG3(...)  do{if(g_nPrintDebugInfo){ALERT(__VA_ARGS__);}}while(0)

#else  /* #ifdef KERNEL_VERSION */

/*  Macroses for user space										*/
/* General macroses for user space, last digit shows number of arguments, before formatting string */
#define		PRINT_REP_ERR_U_1(func,arg1,...) \
	{ \
		func(arg1, "\"%s\": \"%s\": %d. ",strrchr(__FILE__,_LAST_CHAR_)?strrchr(__FILE__,_LAST_CHAR_)+1:"Unknown file",__FUNCTION__,__LINE__); \
		func(arg1, __VA_ARGS__); \
	}

#define		PRINT_REP_ERR_U_0(func,...) \
		{ \
		func("\"%s\": \"%s\": %d. ",strrchr(__FILE__,_LAST_CHAR_)?strrchr(__FILE__,_LAST_CHAR_)+1:"Unknown file",__FUNCTION__,__LINE__); \
		func(__VA_ARGS__); \
		}

#define		PRINT_REP_ERR_U_2(func,arg1,arg2,...) \
		{ \
		func(arg1, arg2,"\"%s\": \"%s\": %d. ",strrchr(__FILE__,_LAST_CHAR_)?strrchr(__FILE__,_LAST_CHAR_)+1:"Unknown file",__FUNCTION__,__LINE__); \
		func(arg1, arg2,__VA_ARGS__); \
		}

#endif  /* #ifdef KERNEL_VERSION */

#else/* #ifdef VARIADIC_MACROS_DEFINED */

#endif/* #ifdef VARIADIC_MACROS_DEFINED */

#endif /* #ifndef __pciegen_io_base_h__ */


#endif  /* #ifndef __print_functions_h__ */
