/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/
#include <JRSMemory_ThreadLocks.h>

JRSMemory_ThreadLock::JRSMemory_ThreadLock()
{
#ifdef _MSC_VER
#if defined(_WIN32) || defined(_WIN64)
	InitializeCriticalSection(&m_CriticalSection);
#endif
#endif
}

JRSMemory_ThreadLock::~JRSMemory_ThreadLock()
{
#ifdef _MSC_VER
#if defined(_WIN32) || defined(_WIN64)
	DeleteCriticalSection(&m_CriticalSection);
#endif
#endif
}

void JRSMemory_ThreadLock::Lock()
{
#ifdef _MSC_VER
#if defined(_WIN32) || defined(_WIN64)
	EnterCriticalSection(&m_CriticalSection);
#endif
#endif
}

void JRSMemory_ThreadLock::Unlock()
{
#ifdef _MSC_VER
#if defined(_WIN32) || defined(_WIN64)
	LeaveCriticalSection(&m_CriticalSection);
#endif
#endif
}