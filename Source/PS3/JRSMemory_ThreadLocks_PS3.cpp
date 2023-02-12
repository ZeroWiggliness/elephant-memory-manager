/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/
#include <JRSMemory_ThreadLocks.h>

JRSMemory_ThreadLock::JRSMemory_ThreadLock()
{
	sys_lwmutex_attribute_t attr;

	attr.attr_protocol = SYS_SYNC_PRIORITY; 
	attr.attr_recursive =  SYS_SYNC_RECURSIVE;
	attr.name[0] = '\0'; 
	sys_lwmutex_create(&m_LwMutex, &attr);
}

JRSMemory_ThreadLock::~JRSMemory_ThreadLock()
{
	sys_lwmutex_destroy(&m_LwMutex);
}

void JRSMemory_ThreadLock::Lock()
{
	sys_lwmutex_lock(&m_LwMutex, 0);
}

void JRSMemory_ThreadLock::Unlock()
{
	sys_lwmutex_unlock(&m_LwMutex);
}