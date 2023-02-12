/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/

/*
This example demonstrates the use of pools in Elephant Memory manager.  Pools are linked to Heaps so that memory can be tracked and also used as an overflow if needed.

Pools are very quick, even the non master versions are significantly quicker than an allocation from the heap.  They are ideal for things like STL also which often perform
many allocations that are very small.  Using pools effectively on small allocations can save you significant memory while cutting down on fragmentation issues.  One of the largest
costs in performance can result from making a pool thread safe.  The locking and unlocking can consume more time than actually allocating and freeing memory so pools allow you
to disable this on a case by case basis.

All these pools are intrusive.  This means that there is no overhead for the allocation and should it be required some debugging features.  Note however it is not possible
to perform all the checks possible that a Heap can catch.  Therefore any potential memory errors should switch to a heap for detailed debugging.
*/


// Needed for string functions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <JRSMemory.h>
#include <JRSMemory_Pools.h>

void MemoryManagerPrint(const jrs_i8 *pText)
{
	OutputDebugString(pText);
	OutputDebugString("\n");
	printf("%s\n", pText);
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

	cMemoryManager::InitializeCallbacks(MemoryManagerPrint, MemoryManagerErrorHandle, NULL);
	cMemoryManager::Get().Initialize(64 * 1024 * 1024, JRSMEMORYINITFLAG_LARGEST, false);

	// Create a few pools.
	struct sImaginary128ByteData
	{
		jrs_i8 img[128];
	};

	// Create some pointers.  This can be anything you like i.e VRAM or memory you cannot access via CPU.  NULL is
	// also be treated as valid in-case the memory area does actually start there.
	void *pExternalMemoryPointer1 = NULL;
	void *pExternalMemoryPointer2 = cMemoryManager::Get().Malloc(2048);
	void *pExternalMemoryPointer3 = cMemoryManager::Get().Malloc(2048);

	// 1. A standard pool that holds 64 32bit numbers.  It uses all the defaults.
	cPoolNonIntrusive *pPool = cMemoryManager::Get().CreatePoolNonIntrusive(sizeof(sImaginary128ByteData), 2048 / sizeof(sImaginary128ByteData), "NonIntrusive1", pExternalMemoryPointer1, 2048, NULL, NULL);

	// 2. A pool which when full goes into an overrun heap
	sPoolDetails ORPoolDetails;
	ORPoolDetails.bEnableMemoryTracking = TRUE;
	cPoolNonIntrusive *pPoolOR = cMemoryManager::Get().CreatePoolNonIntrusive(sizeof(jrs_u32), 32, "NonIntrusive2", pExternalMemoryPointer2, sizeof(jrs_u32) * 32,  &ORPoolDetails, NULL);
	
	// 3. A pool which stores the callstack and name of the allocation as well as sentinel checking.  No overflow buffer.
	// Holds a simple pair structure.  Each element must is aligned to 16 bytes.
	struct sPair
	{
		jrs_u64 First;
		jrs_u32 Second;
	};
	sPoolDetails DetPoolDetails;
	cPoolNonIntrusive *pPoolDet = cMemoryManager::Get().CreatePoolNonIntrusive(sizeof(sImaginary128ByteData), 2048 / sizeof(sImaginary128ByteData), "NonIntrusive3", pExternalMemoryPointer3, 2048, &DetPoolDetails, NULL);
 
 	// Allocate an entire array, giving each the name "item".  When the array is larger an error will appear and the overflow will 
 	// be used.
 	void *p32Bit32Array[32];
 	for(int i = 0; i < 32; i++)
 	{
 		p32Bit32Array[i] = pPoolOR->AllocateMemory("item");
 		*(jrs_sizet *)p32Bit32Array[i] = 0xffffffff;
 	}
 	
 	// Free every other one
 	for(int i = 0; i < 32; i += 2)
 		pPoolOR->FreeMemory(p32Bit32Array[i]);
 
 	// Report the memory of the heap
 	pPoolOR->ReportAll();
 
 	// Free the other data of
 	for(int i = 1; i < 32; i += 2)
 		pPoolOR->FreeMemory(p32Bit32Array[i]);
 
  	// Allocate some memory for tracking
 	void *p32bDet1 = pPoolDet->AllocateMemory("Hello");
 	pPoolDet->FreeMemory(p32bDet1); 

 	// Destroy the pools	
 	cMemoryManager::Get().DestroyPool(pPoolDet);
 	cMemoryManager::Get().DestroyPool(pPoolOR);
	cMemoryManager::Get().DestroyPool(pPool);

	// Free the memory
	cMemoryManager::Get().Free(pExternalMemoryPointer2);
	cMemoryManager::Get().Free(pExternalMemoryPointer3);

	// Check the memory is clear and exit
	cMemoryManager::Get().ReportAll();
	cMemoryManager::Get().Destroy();
}