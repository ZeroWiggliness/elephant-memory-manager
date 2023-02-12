/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/

#include <xtl.h>
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
		m_ThreadFunc = Entry;
		m_hThread = ::CreateThread( NULL, uStackSize, (LPTHREAD_START_ROUTINE)m_ThreadFunc, pArg, CREATE_SUSPENDED, (LPDWORD)&m_ThreadID );    

		XSetThreadProcessor(m_hThread, uProcessor);
	}

	void cJRSThread::Destroy(void)
	{
		CloseHandle(m_hThread);
	}

	void cJRSThread::Start(void)
	{
		ResumeThread( m_hThread );
	}

	void cJRSThread::WaitForFinish(void)
	{
		WaitForSingleObject(m_hThread, INFINITE);
	}

	namespace JRSThread
	{
		void SleepMilliSecond(jrs_i32 iMs)
		{
			Sleep(iMs);
		}

		void SleepMicroSecond(jrs_i32 iMicS)
		{

		}

		void YieldThread(void)
		{
			SwitchToThread();
		}

		jrs_sizet CurrentID(void)
		{
			return (jrs_sizet)GetCurrentThreadId();
		}
	}
} // Namespace
