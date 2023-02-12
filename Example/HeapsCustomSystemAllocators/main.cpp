/* 
(C) Copyright 2007-2011 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 

This is a simple example showing you can create custom allocators for heaps.  By default Elephant uses the standard system allocator to create it's pool (VirtualAlloc or mmap). 
Some systems allow for many different types of memory depending on what is required.  I.e no caching or 1MB pages.  This functionality allows you to create heaps with different
types and allows resizable heaps if needed.  A callback is also available that is called whenever elephant calls them.

Example based on the Non intrusive heap sample
*/

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <windows.h>

#include "JRSMemory.h"

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

// Create the custom allocators.  We use a mix of both malloc and virtualalloc calls.  The NonIntrusive heap will use malloc.  In reality this
// may use some system aligned one or other method to retrieve valid pointers.
void *VirtualAllocSystemAllocator(jrs_u64 uSize, void *pExtMemoryPtr)
{
	return VirtualAlloc(pExtMemoryPtr, (SIZE_T)uSize + (1024 * 1024 * 8), MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN, PAGE_READWRITE);
}

void VirtualAllocSystemFree(void *pFree, jrs_u64 uSize)
{
	VirtualFree(pFree, 0, MEM_RELEASE);
}

jrs_sizet VirtualAllocSystemPageSize(void)
{
	return 8192;
}

void NonIntrusiveHeapSystemCB(cHeapNonIntrusive *pHeap, void *pAddress, jrs_u64 uSize, jrs_bool bFreeOp)
{
	OutputDebugString("NonIntrusiveHeapSystemCB\n");
}

void HeapSystemCB(cHeap *pHeap, void *pAddress, jrs_u64 uSize, jrs_bool bFreeOp)
{
	OutputDebugString("HeapSystemCB\n");
}

void *MallocSystemAllocator(jrs_u64 uSize, void *pExtMemoryPtr)
{
	return malloc((SIZE_T)uSize + (1024 * 1024 * 8));
}

void MallocSystemFree(void *pFree, jrs_u64 uSize)
{
	free(pFree);
}

jrs_sizet MallocSystemPageSize(void)
{
	return 8192;
}

void main(void)
{
#ifdef ELEPHANT_TRIAL
	cMemoryManager::InitializeTrial();
#endif
	// Basic Memory manager initialization
	cMemoryManager::InitializeCallbacks(MemoryManagerTTYPrint, MemoryManagerErrorHandle, NULL);

	// This will override the defaults.  When set on a heap it will override those as well
	cMemoryManager::InitializeAllocationCallbacks(VirtualAllocSystemAllocator, VirtualAllocSystemFree, VirtualAllocSystemPageSize);

	// Initialize live view
	cMemoryManager::InitializeLiveView();

	// Initialize Elephant with into resizable mode, default heap.  Must not be closest.
	cMemoryManager::Get().Initialize(JRSMEMORYINITFLAG_LARGEST, 0, FALSE);

	// Create some heaps.  Allow the 2nd to be customized with NULL free warnings disabled.  Set the default allocators and callbacks to use.
	cHeap::sHeapDetails detail;
	detail.systemAllocator = MallocSystemAllocator;
	detail.systemFree = MallocSystemFree;
	detail.systemPageSize = MallocSystemPageSize;
	detail.systemOpCallback = HeapSystemCB;
	cHeap *pDefault = cMemoryManager::Get().CreateHeap(32 * 1024 * 1024, "Custom Allocators", &detail);

	// Create the Non Intrusive heap.  Set the default allocators and callbacks to use.
	cHeapNonIntrusive::sHeapDetails details;
	details.systemAllocator = VirtualAllocSystemAllocator;
	details.systemFree = VirtualAllocSystemFree;
	details.systemPageSize = VirtualAllocSystemPageSize;
	details.systemOpCallback = NonIntrusiveHeapSystemCB;
	cHeapNonIntrusive *pHeapNI = cMemoryManager::Get().CreateNonIntrusiveHeap(32 * 1024 * 1024, pDefault, "Non Intrusive", &details);

	// Allocate some memory.
	void *pImaginarySystemPointer1 = pHeapNI->AllocateMemory(2 * 1024 * 1024, 0 /* use heap default alignment */);
	void *pImaginarySystemPointer2 = pHeapNI->AllocateMemory(64, 0 /* use heap default alignment */);

	// Write out all the details
	cMemoryManager::Get().ReportAll();

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