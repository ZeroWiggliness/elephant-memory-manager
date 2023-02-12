/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/

#include "../JRSMemory_Timer.h"

namespace Elephant
{
	cJRSTimer::cJRSTimer()
	{
		m_qwFrequency = sys_time_get_timebase_frequency();
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
		SYS_TIMEBASE_GET(m_qwOldTime);
		m_ElapsedTimeMSSinceLastUpdate = 0;
		m_ElapsedTimeMSTotal = 0;
	}

	void cJRSTimer::Update(void)
	{
		if(m_TimerStarted)
		{
			uint64_t qwCurrentTime;

			SYS_TIMEBASE_GET( qwCurrentTime );

			m_ElapsedTimeMSSinceLastUpdate = (jrs_u32)(((qwCurrentTime - m_qwOldTime) / (float)m_qwFrequency) * 1000.0f);
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
}