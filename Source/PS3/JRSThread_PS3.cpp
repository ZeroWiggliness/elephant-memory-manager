/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/

#include <JRSMemory_Thread.h>
#include <sys/ppu_thread.h>
#include <sys/timer.h>
#include <cell/atomic.h>

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
 		jrs_u64 Arg = (jrs_u64)pArg;
		sys_ppu_thread_create(&m_Thread, Entry, Arg, 1001, uStackSize, SYS_PPU_THREAD_CREATE_JOINABLE, "JRSThread");
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
		jrs_u64 data;
		sys_ppu_thread_join(m_Thread, &data);
	}

	namespace JRSThread
	{
		void SleepMilliSecond(jrs_i32 iMs)
		{
			sys_timer_usleep(iMs * 1000);
		}

		void SleepMicroSecond(jrs_i32 iMicS)
		{
			sys_timer_usleep(iMicS);
		}

		void YieldThread(void)
		{
			sys_ppu_thread_yield();
		}

		jrs_sizet CurrentID(void)
		{
			sys_ppu_thread_t ppu_thread_id;
			sys_ppu_thread_get_id(&ppu_thread_id);

			return (jrs_sizet)ppu_thread_id;
		}
	}
}// Namespace