/*
 *	File: global_pointer_container.c
 *
 *	Created on: Sep 15, 2015
 *	Author: Davit Kalantaryan (Email: davit.kalantaryan@desy.de)
 *
 *  ...
 *
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/slab.h>
#include "debug_functions.h"

#define DEBUG_PRINT(...) {printk(KERN_ALERT "!!!!!!!!!!!!!!!!!! ln=%d: ",__LINE__); printk(KERN_CONT __VA_ARGS__);}

struct SSingleEntry
{
	char*					key_name;
	void*					private_data;
	struct module*			owner;
	int						usage;
	struct SSingleEntry*	next_ptr;
};


// No data is asociated with head
static struct SSingleEntry s_ContainerHead = {
	.key_name = "",
	.private_data = NULL,
	.next_ptr = NULL,
};

static struct mutex s_mutex;


static  struct SSingleEntry* FindEntryFromContainerPrivate(const char* a_cpcKeyName);

void RemoveEntryFromGlobalContainer(const char* a_cpcKeyName)
{
	int nWarning = 0;
	struct SSingleEntry *pToDelete, *pPrevToDelete;
	
	while (mutex_lock_interruptible(&s_mutex));

	pPrevToDelete = FindEntryFromContainerPrivate(a_cpcKeyName);
	if (pPrevToDelete)
	{
		pToDelete = pPrevToDelete->next_ptr;
		pPrevToDelete->next_ptr = pToDelete->next_ptr;
		nWarning = pToDelete->usage;
	}

	mutex_unlock(&s_mutex);

	if (pPrevToDelete)
	{
		kfree(pToDelete->key_name);
		kfree(pToDelete);
	}

	if (nWarning)
	{
		WARNRT("usage of entry is: %d\n", nWarning);
	}
}
EXPORT_SYMBOL(RemoveEntryFromGlobalContainer);

int vRemoveEntryFromGlobalContainer(const char* a_cpcFormat, ...)
{
	char* pcKeyName;
	va_list ap;

	va_start(ap, a_cpcFormat);
	pcKeyName = kvasprintf(GFP_KERNEL, a_cpcFormat, ap);
	va_end(ap);

	if (!pcKeyName){return -ENOMEM;}

	RemoveEntryFromGlobalContainer(pcKeyName);	
	kfree(pcKeyName);
	return 0;

}
EXPORT_SYMBOL(vRemoveEntryFromGlobalContainer);


void PutEntryToGlobalContainer(const char* a_cpcKeyName)
{
	struct SSingleEntry *pPreviousEntry;

	while (mutex_lock_interruptible(&s_mutex));

	pPreviousEntry = FindEntryFromContainerPrivate(a_cpcKeyName);
	if (pPreviousEntry)
	{
		module_put(pPreviousEntry->owner);
		pPreviousEntry->usage--;
	}

	mutex_unlock(&s_mutex);
}
EXPORT_SYMBOL(PutEntryToGlobalContainer);


void* FindAndUseEntryFromGlobalContainer(const char* a_cpcKeyName)
{
	struct SSingleEntry *pPreviousEntry;

	if (mutex_lock_interruptible(&s_mutex))
	{
		//return -ERESTARTSYS;
		return NULL;
	}

	pPreviousEntry = FindEntryFromContainerPrivate(a_cpcKeyName);
	if (pPreviousEntry)
	{
		bool bRet = try_module_get(pPreviousEntry->owner);
		if (!bRet){ return NULL; }
		pPreviousEntry->usage++;
	}

	mutex_unlock(&s_mutex);
	return pPreviousEntry&&pPreviousEntry->next_ptr ? pPreviousEntry->next_ptr->private_data : NULL;
}
EXPORT_SYMBOL(FindAndUseEntryFromGlobalContainer);



void* vFindAndUseEntryFromGlobalContainer(const char* a_cpcFormat, ...)
{
	char* pcKeyName;
	void* pRet;
	va_list ap;

	va_start(ap, a_cpcFormat);
	pcKeyName = kvasprintf(GFP_KERNEL, a_cpcFormat, ap);
	va_end(ap);

	if (!pcKeyName)
	{
		//return -ENOMEM;
		return NULL;
	}

	pRet = FindAndUseEntryFromGlobalContainer(pcKeyName);
	kfree(pcKeyName);
	return pRet;
}
EXPORT_SYMBOL(vFindAndUseEntryFromGlobalContainer);



int AddNewEntryToGlobalContainer_prvt(struct module* a_owner,const void* a_pEntry, const char* a_cpcFormat,...)
{
	struct SSingleEntry* pPrevElement = &s_ContainerHead;
	struct SSingleEntry* pCurElement = s_ContainerHead.next_ptr;
	struct SSingleEntry* pContElement = (struct SSingleEntry*)kzalloc(sizeof(struct SSingleEntry), GFP_KERNEL);
	va_list ap;

	if (!pContElement) return -ENOMEM;

	va_start(ap, a_cpcFormat);
	pContElement->key_name = kvasprintf(GFP_KERNEL, a_cpcFormat, ap);
	va_end(ap);

	if (!pContElement->key_name)
	{
		kfree(pContElement);
		return -ENOMEM;
	}
	DEBUG_PRINT("key_name=\"%s\"\n", pContElement->key_name);

	pContElement->private_data = (void*)a_pEntry;

	if (mutex_lock_interruptible(&s_mutex))
	{
		kfree(pContElement->key_name); 
		kfree(pContElement); 
		return -ERESTARTSYS;
	}
	
	while (pCurElement)
	{
		if (strcmp(pContElement->key_name, pCurElement->key_name) == 0)
		{
			mutex_unlock(&s_mutex);
			kfree(pContElement->key_name);
			kfree(pContElement);
			ERRRT("ERROR:  entry already exist!\n");
			return -1;
		}
		pPrevElement = pCurElement;
		pCurElement = pPrevElement->next_ptr;
	}

	pContElement->owner = a_owner;
	pContElement->usage = 0;
	pPrevElement->next_ptr = pContElement;

	mutex_unlock(&s_mutex);
	
	return 0;
}
EXPORT_SYMBOL(AddNewEntryToGlobalContainer_prvt);



int InitGlobalContainerInternal(void)
{
	mutex_init(&s_mutex);
	return 0;
}


void DestroyGlobalContainerInternal(void)
{
	struct SSingleEntry* pPrevElement = &s_ContainerHead;
	struct SSingleEntry* pCurElement = s_ContainerHead.next_ptr;

	while (mutex_lock_interruptible(&s_mutex));

	while (pCurElement)
	{
		DEBUG_PRINT("Deleting: key_name=\"%s\"\n", pCurElement->key_name);

		pPrevElement = pCurElement;
		pCurElement = pPrevElement->next_ptr;

		kfree(pPrevElement->key_name);
		kfree(pPrevElement);
	}

	mutex_unlock(&s_mutex);
}


static  struct SSingleEntry* FindEntryFromContainerPrivate(const char* a_cpcKeyName)
{
	struct SSingleEntry* pPrevElement = &s_ContainerHead;
	struct SSingleEntry* pCurElement = s_ContainerHead.next_ptr;

	while (pCurElement)
	{
		if (strcmp(pCurElement->key_name, a_cpcKeyName) == 0)
		{ 
			DEBUG_PRINT("Found: key_name=\"%s\"\n", pCurElement->key_name);
			return pPrevElement; 
		}
		pPrevElement = pCurElement;
		pCurElement = pPrevElement->next_ptr;
	}

	return NULL;
}
