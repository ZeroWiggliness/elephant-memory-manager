/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/

#ifndef _JRSMEMORYTHREAD_H_
#define _JRSMEMORYTHREAD_H_

#ifndef _JRSCORETYPES_H
#include <JRSCoreTypes.h>
#endif

#ifdef JRSMEMORYSONYPS3PLATFORM
#ifndef	__SYS_SYS_PPU_THREAD_H__
#include <sys/ppu_thread.h>
#endif
#endif

#ifdef JRSMEMORYNINTENDOWII
#ifndef __REVOLUTION_H__
#include <revolution.h>
#endif
#endif

#ifdef JRSMEMORYNINTENDO3DS
#error // Need include here
#endif

#ifdef JRSMEMORYARMGENERIC
//#error // Need include here
#endif

#if defined(JRSMEMORY_HASPTHREADS)			// Android/Apple/Linux/most GCC based libs
#ifndef _PTHREAD_H
#include <pthread.h>
#endif
#endif

// These define the returns needed for a generic returning of values from a thread.  Elephant
// doesn't require anything to be returned.  This is only for each platform to terminate the threads
// successfully.
#ifdef JRSMEMORYMICROSOFTPLATFORMS					// Windows/Xbox
typedef void * HANDLE;
#define JRSThreadReturn(X) return X
#elif defined(JRSMEMORYSONYPS3PLATFORM)				// PS3
#define JRSThreadReturn(X) sys_ppu_thread_exit(X)
#elif defined(JRSMEMORYNINTENDOWII)					// Wii
#define JRSThreadReturn(X) return (void *)X
#elif defined(JRSMEMORYNINTENDO3DS)					// 3DS
//#error
#define JRSThreadReturn(X) return (void *)X
#elif defined(JRSMEMORYARMGENERIC)
#define JRSThreadReturn(X) return (void *)X
#elif defined(JRSMEMORY_HASPTHREADS)			// Android/Apple/Linux/most GCC based libs
#define JRSThreadReturn(X) pthread_exit(0)
#else
#error undefed platfrom
#endif

namespace Elephant
{

class cJRSThread
{
public:
	#ifdef JRSMEMORYMICROSOFTPLATFORMS
	typedef void * jrs_threadin;
	typedef jrs_u32 jrs_threadout;
	#elif defined(JRSMEMORYSONYPS3PLATFORM)
	typedef jrs_u64 jrs_threadin;
	typedef void jrs_threadout;
	#elif defined(JRSMEMORYNINTENDOWII)
	typedef void * jrs_threadin;
	typedef void * jrs_threadout;
	#elif defined(JRSMEMORYNINTENDO3DS)
	#error
	// Most probably
	typedef void * jrs_threadin;
	typedef void * jrs_threadout;
	#elif defined(JRSMEMORYARMGENERIC)
	typedef void * jrs_threadin;
	typedef void * jrs_threadout;
	#elif defined(JRSMEMORY_HASPTHREADS)			// Android/Apple/Linux/most GCC based libs
	typedef void * jrs_threadin;
	typedef void * jrs_threadout;
	#endif

private:
#ifdef JRSMEMORYMICROSOFTPLATFORMS
	HANDLE m_hThread;
	typedef jrs_u32	(*ThreadEntryJob)(void *pArg, void *pJobSpecific);
	jrs_u32 m_ThreadID;
#elif defined(JRSMEMORYSONYPS3PLATFORM)
	sys_ppu_thread_t m_Thread;
#elif defined(JRSMEMORYNINTENDOWII)
	OSThread m_Thread;
	jrs_u8 m_ThreadStack[32 * 1024];
#elif defined(JRSMEMORYNINTENDO3DS)
	#error
	// You need the thread variable to be defined here.
#elif defined(JRSMEMORYARMGENERIC)
#elif defined(JRSMEMORY_HASPTHREADS)
	pthread_t m_Thread;
#endif

	typedef jrs_threadout (*ThreadEntry)(jrs_threadin pArg); 
	ThreadEntry	m_ThreadFunc;

public:
	
	enum eJRSPriority
	{
		eJRSPriority_Highest,
		eJRSPriority_High,
		eJRSPriority_MedHigh,
		eJRSPriority_Med,
		eJRSPriority_MedLow,
		eJRSPriority_Low,
		eJRSPriority_Lowest,
		eJRSPriority_DWord = 0x7fffffff
	};


	cJRSThread();
	~cJRSThread();

	void Create(ThreadEntry Entry, void *pArg, eJRSPriority ePriority, jrs_u32 uProcessor, jrs_u32 uStackSize);
	void Destroy(void);

	void Start(void);
	void WaitForFinish(void);
};

namespace JRSThread
{
	void SleepMilliSecond(jrs_i32 iMs);
	void SleepMicroSecond(jrs_i32 iMicS);
	void YieldThread(void);
	jrs_sizet CurrentID(void);
}

}	// Elephant namespace


#endif // _JRSMEMORYTHREAD_H_
