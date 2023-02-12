/* 
(C) Copyright 2007-2011 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 

This is a simple example showing 'Resizable Memory Mode'.  This is the same method you would know from standard allocators.  You do not need to worry about
how much memory you are consuming.  Elephant will resize the heaps as you consume memory and release it back when you destroy the heap.
*/

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <windows.h>

#include "JRSMemory.h"
#include "MMgr.h"

// Basic memory manager overloading
void MemoryManagerTTYPrint(const jrs_i8 *pText)
{
	printf("%s\n", pText);			// Output to console window for example
	OutputDebugString(pText);		// Output to TTY.
	OutputDebugString("\n");
}

void MemoryManagerErrorHandle(const jrs_i8 *pError, jrs_u32 uErrorID)
{
	__debugbreak();
}

void CustomElasticHeapSettings(cHeap::sHeapDetails &rDetails)
{
	// As an example we allow destruction with allocations.
	rDetails.bAllowDestructionWithAllocations = true;
}

void main(void)
{
#ifdef ELEPHANT_TRIAL
	cMemoryManager::InitializeTrial();
#endif

	// Basic Memory manager initialization
	cMemoryManager::InitializeCallbacks(MemoryManagerTTYPrint, MemoryManagerErrorHandle, NULL);

	// Initialize live view
	cMemoryManager::InitializeLiveView();
	
	// Elephant needs a bit of memory to hold some of its structure information.  This depends highly on what features are being used.
	// Normally this isn't a problem.  For Elastic heaps it is good to set this to reasonable amount while developing. Setting the input
	// to 0 will give an error warning you how much Elephant needs.
	cMemoryManager::Get().Initialize(JRSMEMORYINITFLAG_LARGEST, JRSMEMORYINITFLAG_LARGEST, FALSE);

	// With resizable memory

	void *pMem = cMemoryManager::Get().Malloc(1024 * 1024 * 24);
	//void *pMem = cMemoryManager::Get().Malloc(1024 * 1024 * 24);

	// Write out all the details before we end
	cMemoryManager::Get().ReportAll();

	// Note you do not need to call this.  The destructor will call this also but if you want to decide when all the 
	// memory should be freed this will perform that task.
	cMemoryManager::Get().Destroy();
}