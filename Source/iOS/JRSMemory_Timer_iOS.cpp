/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/
#include <sys/time.h>
#include <stdio.h>
#include "JRSMemory_Timer.h"

namespace Elephant
{
	cJRSTimer::cJRSTimer()
	{
		Start();
	}

	cJRSTimer::~cJRSTimer()
	{
		Stop();
	}

	void cJRSTimer::Start(void)
	{
		Reset();
		m_TimerStarted = true;
	}

	void cJRSTimer::Stop(void)
	{
		m_TimerStarted = false;
	}

	void cJRSTimer::Reset(void)
	{
		gettimeofday(&m_OldTime, NULL);
		m_ElapsedTimeMSSinceLastUpdate = 0;
		m_ElapsedTimeMSTotal = 0;
	}

	void cJRSTimer::Update(void)
	{
		if(m_TimerStarted)
		{
			timeval CurrentTime;
			gettimeofday(&CurrentTime, NULL);
  
			jrs_sizet useconds = (CurrentTime.tv_sec == m_OldTime.tv_sec) ? CurrentTime.tv_usec - m_OldTime.tv_usec : CurrentTime.tv_usec + (1000000 - m_OldTime.tv_usec);    

			m_ElapsedTimeMSSinceLastUpdate = (jrs_u32)(useconds / 1000);
			m_ElapsedTimeMSTotal += m_ElapsedTimeMSSinceLastUpdate;
			m_OldTime = CurrentTime;
        }
	}

	jrs_u32 cJRSTimer::GetElapsedTimeMilliSec(jrs_bool bUpdate)
	{
		if(bUpdate)
		{
			Update();
		}

		return m_ElapsedTimeMSTotal;
	}
}