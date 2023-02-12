/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/

// Needed for string functions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <JRSMemory.h>
#include <JRSMemory_Thread.h>

#include "JRSMemory_Internal.h"	
#include "JRSMemory_ErrorCodes.h"
#include "JRSMemory_Timer.h"

namespace Elephant
{

	// Time since last process of the enhanced debugging thread.
	jrs_u32 cMemoryManager::m_uEDebugTime = 0;

	// Time before Elephant purges the memory allocation in MilliSeconds.
	jrs_u32 cMemoryManager::m_uEDebugPendingTime = 66;

	// Ring buffer size for the number of allocations.
	jrs_u32 cMemoryManager::m_uEDebugMaxPendingAllocations = 0;

	//  Description:
	//		Internal function.  Adds a pending free address to the circular buffer so it can be freed by the enhanced debugging thread.
	//  See Also:
	//		
	//  Arguments:
	//		pMemoryAddress - Memory address to free.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Adds a pending free address to the circular buffer so it can be freed by the enhanced debugging thread.
	void cMemoryManager::AddEDebugAllocation(void *pMemoryAddress)
	{
		// Add it to the buffer.  We may need to wait if the buffer is full until some get cleared.  Generally we keep them around for 
		// a second or two.
		while(1)
		{
			m_EDThreadLock.Lock();
			jrs_u32 EDebugBufEnd = m_EDebugBufEnd;
			jrs_u32 uNewEnd = (EDebugBufEnd + 1) % m_uEDebugMaxPendingAllocations;
			if(uNewEnd != m_EDebugBufStart)
			{
				m_pEDebugBuffer[EDebugBufEnd].pPtr = pMemoryAddress;
				m_pEDebugBuffer[EDebugBufEnd].uTime = m_uEDebugTime;
				m_EDebugBufEnd = uNewEnd;
				m_EDThreadLock.Unlock();
				return;
			}
			m_EDThreadLock.Unlock();
		}	
	}

	//  Description:
	//		Internal function.  Main Enhanced Debugging thread function.
	//  See Also:
	//		
	//  Arguments:
	//		pArg - Argument passed to the thread.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Main Enhanced Debugging thread function.
	cJRSThread::jrs_threadout cMemoryManager::JRSMemory_EnhancedDebuggingThread(cJRSThread::jrs_threadin pArg)
	{
		jrs_bool *pActiveEnhancedDebugging = (jrs_bool *)((jrs_sizet)pArg);
		jrs_bool PerformSleep = true;		// When we max the buffer out we will hit a snag as it will cause significant wait times when freeing large number of allocs.  This tries to prevent that.

		// Create the timer
		cJRSTimer EDebugTimer;
		while(*pActiveEnhancedDebugging && cMemoryManager::Get().IsInitialized())
		{
			sEDebug *pEDebug = cMemoryManager::Get().m_pEDebugBuffer;

			// Get the elapsed time		
			m_uEDebugTime = EDebugTimer.GetElapsedTimeMilliSec(true);
			
			// Lock the thread just to be safe
			cMemoryManager::Get().m_EDThreadLock.Lock();

			// Scan the list and see if any allocations no fall behind this.
			PerformSleep = true;

			jrs_u32 EDebugBufStart = cMemoryManager::Get().m_EDebugBufStart;
			jrs_u32 EDebugBufEnd = cMemoryManager::Get().m_EDebugBufEnd;
			while(EDebugBufStart != EDebugBufEnd)
			{
				jrs_u32 uStartNext = (EDebugBufStart + 1) % m_uEDebugMaxPendingAllocations;

 				// Scan forward until we the time is > inserted time + x amount.
				if((pEDebug[EDebugBufStart].uTime + m_uEDebugPendingTime) > m_uEDebugTime )
				{
					// no more to do
					PerformSleep = true;
					break;
				}

				// We have a valid allocation to clear. 
				jrs_u32 *pMemory = (jrs_u32 *)pEDebug[EDebugBufStart].pPtr;

				// Find the heap
				cHeap *pHeap = cMemoryManager::Get().FindHeapFromMemoryAddress(pMemory);
				MemoryWarning(pHeap, JRSMEMORYERROR_ENHANCEDINVALIDADDRESS, "Invalid memory address found in Enhanced Debug buffer (0x%p). FATAL ERROR.", pMemory);

				// 1. Check that it hasn't been corrupted.
				sAllocatedBlock *pBlock = (sAllocatedBlock *)((jrs_i8 *)pMemory - sizeof(sAllocatedBlock));
				for(jrs_u32 i = 0; i < pBlock->uSize >> 2; i++)
				{
					// Corruption found. 
					HeapWarningExtern(pHeap, pMemory[i] == MemoryManager_EDebugClearValue, JRSMEMORYERROR_POSTFREECORRUPTION, "Memory corruption found in post freed memory at 0x%p (corrupted location at 0x%p). Further errors may occur.", pMemory, &pMemory[i]);					
				}

	#ifdef _DEBUG
				// Clear the value in the buffer so we know we have done it
				pEDebug[EDebugBufStart].pPtr = 0;
				pEDebug[EDebugBufStart].uTime = 0xffffffff;
	#endif

				// 2. Free it. Free it.  All the checks were valid when we first passed the test so we can free safely.
				pHeap->m_pThreadLock->Lock();
				pHeap->InternalFreeMemory(pMemory, 0, "Enhanced Free", 0);
			
				// 3. Decrement the pending count.
				pHeap->m_uEDebugPending--;
				pHeap->m_pThreadLock->Unlock();

				// Increment the next
				cMemoryManager::Get().m_EDebugBufStart = uStartNext;
				EDebugBufStart = uStartNext;

				EDebugBufEnd = cMemoryManager::Get().m_EDebugBufEnd;
			}

			// Unlock
			cMemoryManager::Get().m_EDThreadLock.Unlock();

			// Sleep
			if(PerformSleep)
				JRSThread::SleepMilliSecond(16);
		}

		JRSThreadReturn(1);
	}
}
