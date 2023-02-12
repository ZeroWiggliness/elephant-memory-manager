/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/

#ifndef _JRSMEMORY_TIMER_H
#define _JRSMEMORY_TIMER_H

#ifndef _JRSCORETYPES_H
#include <JRSCoreTypes.h>
#endif

#ifdef JRSMEMORYPCPLATFORM
#include <Windows.h>
#elif defined(JRSMEMORYXBOX360PLATFORM)
#ifndef _XTL_
#include <Xtl.h>
#endif
#elif defined(JRSMEMORYSONYPS3PLATFORM)
#include <sys/sys_time.h>
#include <sys/time_util.h>
#elif defined(JRSMEMORYNINTENDOWII)
#ifndef __REVOLUTION_H__
#include <revolution.h>
#endif
#elif defined(JRSMEMORYNINTENDO3DS)
#error // Need include here
#elif defined(JRSMEMORYARMGENERIC)
#elif defined(JRSMEMORY_HASPTHREADS)	// Android/Apple/Linux/most GCC based libs
#ifndef _SYS_TIME_H_
#include <sys/time.h>
#endif
#endif

namespace Elephant
{
	class cJRSTimer
	{
		
#ifdef JRSMEMORYMICROSOFTPLATFORMS
		LARGE_INTEGER m_qwFrequency;
		LARGE_INTEGER m_qwOldTime;
#elif defined(JRSMEMORYSONYPS3PLATFORM)
		jrs_u64 m_qwFrequency;
		jrs_u64 m_qwOldTime;
#elif defined(JRSMEMORYNINTENDOWII)
		OSTime m_qwOldTime;
#elif defined(JRSMEMORYNINTENDO3DS)
#error
#elif defined(JRSMEMORYARMGENERIC)
//#error
#elif defined(JRSMEMORY_HASPTHREADS)	// Android/Apple/Linux/most GCC based libs
		timeval m_OldTime;
#else
#error
#endif
		jrs_u32 m_ElapsedTimeMSSinceLastUpdate;
		jrs_u32 m_ElapsedTimeMSTotal;
		jrs_bool m_TimerStarted;

	public:
		cJRSTimer();
		~cJRSTimer();

		void Start(void);
		void Stop(void);
		void Reset(void);
		void Update(void);

		jrs_u32 GetElapsedTimeMilliSec(jrs_bool bUpdate = false);
	};
}

#endif	// _JRSMEMORY_TIMER_H
