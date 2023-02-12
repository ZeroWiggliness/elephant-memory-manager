/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/

#include <JJRSMemory_Thread.h>

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
		OSCreateThread(&m_Thread, Entry, pArg, m_ThreadStack, 32 * 1024, 16, 0);
		OSResumeThread(&m_Thread);
	}

	void cJRSThread::Destroy(void)
	{
		WaitForFinish();
		OSDetachThread(&m_Thread);
	}

	void cJRSThread::Start(void)
	{
		
	}

	void cJRSThread::WaitForFinish(void)
	{
		OSJoinThread(&m_Thread, NULL);
	}

	namespace JRSThread
	{
		void SleepMilliSecond(jrs_i32 iMs)
		{
			OSSleepMilliseconds(iMs);
		}

		void SleepMicroSecond(jrs_i32 iMicS)
		{
			OSSleepMicroseconds(iMicS);
		}

		void YieldThread(void)
		{
			OSYieldThread();
		}

		jrs_sizet CurrentID(void)
		{
			return 0;
		}
	}
}	// Namespace
