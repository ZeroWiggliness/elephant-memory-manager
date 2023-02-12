/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/
#include <JRSMemory_ThreadLocks.h>

JRSMemory_ThreadLock::JRSMemory_ThreadLock()
{
	InitializeCriticalSection(&m_CriticalSection);
}

JRSMemory_ThreadLock::~JRSMemory_ThreadLock()
{
	DeleteCriticalSection(&m_CriticalSection);
}

void JRSMemory_ThreadLock::Lock()
{
	EnterCriticalSection(&m_CriticalSection);
}

void JRSMemory_ThreadLock::Unlock()
{
	LeaveCriticalSection(&m_CriticalSection);
}