/* 
(C) Copyright 2007-2011 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 

This is a simple example showing how to create a non intrusive heap.  These heaps are ideal for memory areas that are not suitable for direct CPU access.
They are however more limited in regards to debugging, sizes and features found on standard cHeap.
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

void main(void)
{
#ifdef ELEPHANT_TRIAL
	cMemoryManager::InitializeTrial();
#endif
	// Basic Memory manager initialization
	cMemoryManager::InitializeCallbacks(MemoryManagerTTYPrint, MemoryManagerErrorHandle, NULL);

	// Initialize live view
	cMemoryManager::InitializeLiveView();

	// Initialize Elephant with into resizable mode, default heap.  Must not be closest.
	cMemoryManager::Get().Initialize(JRSMEMORYINITFLAG_LARGEST, 32 * 1024 * 1024, FALSE);

	// Create some heaps.  Allow the 2nd to be customized with NULL free warnings disabled.
	cHeap *pDefault = cMemoryManager::Get().GetDefaultHeap();

	// Create the Non Intrusive heap.  You may pass in a direct pointer to memory here or allocate it like the normal heap.  Debugging is done via sHeapDetails
	// and is available is all builds except MASTER.
	cHeapNonIntrusive::sHeapDetails details;
	details.bEnableMemoryTracking = TRUE;			// Enable memory tracking

	// You must input a heap managed by Elephant.  Note that some memory to hold the memory headers is always required.  Debugging information is also stored
	// in this heap and that can end up being quite large.
	cHeapNonIntrusive *pHeapNI = cMemoryManager::Get().CreateNonIntrusiveHeap(32 * 1024 * 1024, pDefault, "Non Intrusive", &details);

	// Allocate some memory.
	void *pImaginarySystemPointer1 = pHeapNI->AllocateMemory(2 * 1024 * 1024, 0 /* use heap default alignment */);
	void *pImaginarySystemPointer2 = pHeapNI->AllocateMemory(64, 0 /* use heap default alignment */);

	// Write out all the details
	cMemoryManager::Get().ReportAll(NULL);
	
	// list with the callstacks also
	pHeapNI->ReportAllocationsMemoryOrder(NULL, TRUE, TRUE);

	// Free the memory used by the heaps. You can use either cHeap::FreeMemory or the slightly slower cMemoryManager::Free (it just finds the heap it was allocated from and calls the other function)
	pHeapNI->FreeMemory(pImaginarySystemPointer1);
	pHeapNI->FreeMemory(pImaginarySystemPointer2);

	// You must destroy any user heaps.  Managed heaps are fine to be left (even with allocations if you have disabled this with the details structure)
	cMemoryManager::Get().DestroyNonIntrusiveHeap(pHeapNI);
	
	// Write out all the details before we end
	cMemoryManager::Get().ReportAll();

	// Note you do not need to call this.  The destructor will call this also but if you want to decide when all the 
	// memory should be freed this will perform that task.
	cMemoryManager::Get().Destroy();
}