/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/

#include "../JRSMemory_Timer.h"

namespace Elephant
{
#ifdef JRSMEMORYMICROSOFTPLATFORMS
	cJRSTimer::cJRSTimer()
	{
		QueryPerformanceFrequency(&m_qwFrequency);
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
		QueryPerformanceCounter(&m_qwOldTime);
		m_ElapsedTimeMSSinceLastUpdate = 0;
		m_ElapsedTimeMSTotal = 0;
	}

	void cJRSTimer::Update(void)
	{
		if(m_TimerStarted)
		{
			LARGE_INTEGER qwCurrentTime;

			QueryPerformanceCounter(&qwCurrentTime);
			m_ElapsedTimeMSSinceLastUpdate = (jrs_u32)(((qwCurrentTime.QuadPart - m_qwOldTime.QuadPart) / (float)(m_qwFrequency.QuadPart)) * 1000.0f);
			m_ElapsedTimeMSTotal += m_ElapsedTimeMSSinceLastUpdate;
			m_qwOldTime = qwCurrentTime;			
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
#endif
}