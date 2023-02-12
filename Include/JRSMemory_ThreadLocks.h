/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/

#ifndef _JRSMEMORY_THREADLOCKS_H
#define _JRSMEMORY_THREADLOCKS_H

#ifndef _JRSCORETYPES_H
#include "JRSCoreTypes.h"
#endif

#ifdef JRSMEMORYPCPLATFORM
#include <Windows.h>
#endif

#ifdef JRSMEMORYXBOX360PLATFORM
#ifndef _XTL_
#include <Xtl.h>
#endif
#endif

#ifdef JRSMEMORYSONYPS3PLATFORM
#ifndef __SYS_SYS_SYNCHRONIZATION_H__
#include <sys/synchronization.h>
#endif
#endif

#ifdef JRSMEMORYNINTENDOWII
#ifndef __REVOLUTION_H__
#include <revolution.h>
#endif
#endif

#ifdef JRSMEMORYNINTENDO3DS
#error
#endif

#ifdef JRSMEMORYARMGENERIC
//#error
#endif

#if defined(JRSMEMORY_HASPTHREADS)			// Android/Apple/Linux/most GCC based libs
#ifndef _PTHREAD_H
#include <pthread.h>
#endif
#endif

JRSMEMORYALIGNPRE(128)			// Align the memory manager to 128 bytes.  Important for cache and atomic operations.
class JRSMEMORYDLLEXPORT JRSMemory_ThreadLock
{
#ifdef JRSMEMORYMICROSOFTPLATFORMS
	CRITICAL_SECTION m_CriticalSection;
#elif defined(JRSMEMORYSONYPS3PLATFORM)
	sys_lwmutex_t m_LwMutex;
#elif defined(JRSMEMORYNINTENDOWII)
	OSMutex m_Mutex;
#elif defined(JRSMEMORYNINTENDO3DS)
	#error
#elif  defined(JRSMEMORYARMGENERIC)
	//#error
#elif defined(JRSMEMORY_HASPTHREADS)			// Android/Apple/Linux/most GCC based libs
	pthread_mutex_t  m_Mutex;
#endif

public:
	
	JRSMemory_ThreadLock();
	~JRSMemory_ThreadLock();

	void Lock();
	void Unlock();
}
JRSMEMORYALIGNPOST(128)
;

#endif
