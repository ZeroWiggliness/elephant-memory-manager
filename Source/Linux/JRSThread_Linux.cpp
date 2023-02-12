/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/

// For usleep
#include <unistd.h>
#include <sched.h>
#include <JRSMemory_Thread.h>

namespace Elephant
{
	cJRSThread::cJRSThread()
	{

	}

	cJRSThread::~cJRSThread()
	{

	}

	void cJRSThread::Create(ThreadEntry Entry, void *pArg, eJRSPriority ePriority, jrs_u32 uProcessor, jrs_u32 uStackSize )
	{
		pthread_create(&m_Thread, NULL, Entry, pArg);
	}

	void cJRSThread::Destroy(void)
	{
		WaitForFinish();
	}

	void cJRSThread::Start(void)
	{
		
	}

	void cJRSThread::WaitForFinish(void)
	{
		pthread_join(m_Thread, NULL);
	}

	namespace JRSThread
	{
		void SleepMilliSecond(jrs_i32 iMs)
		{
			usleep(iMs * 1000);
		}

		void SleepMicroSecond(jrs_i32 iMicS)
		{
			usleep(iMicS);
		}

		void YieldThread(void)
		{
			sched_yield();
		}

		jrs_sizet CurrentID(void)
		{
			return (jrs_sizet)pthread_self();
		}
	}
}	// Namespace