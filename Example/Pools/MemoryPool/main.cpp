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

void MemoryManagerWriteToFile(void *pData, int size, const char *pFilePathAndName, jrs_bool bAppend)
{
	FILE *fp;

	fp = fopen(pFilePathAndName, (bAppend) ? "ab" : "wb");
	if(fp)
	{
		fwrite(pData, size, 1, fp);
		fclose(fp);
	}
}

void main(void)
{
#ifdef ELEPHANT_TRIAL
	cMemoryManager::InitializeTrial();
#endif

	cMemoryManager::InitializeCallbacks(MemoryManagerPrint, MemoryManagerErrorHandle, MemoryManagerWriteToFile);
	cMemoryManager::Get().Initialize(64 * 1024 * 1024, JRSMEMORYINITFLAG_LARGEST, false);

	// Create a few pools

	// 1. A standard pool that holds 64 32bit numbers.  It uses all the defaults.
	cPool *pPool = cMemoryManager::Get().CreatePool(sizeof(jrs_u32), 64, "32Bit64Array", NULL, NULL);

	// 2. A pool which when full goes into an overrun heap
	sPoolDetails ORPoolDetails;
	ORPoolDetails.pOverrunHeap = cMemoryManager::Get().GetHeap(0);
	cPool *pPoolOR = cMemoryManager::Get().CreatePool(sizeof(jrs_u32), 32, "32Bit32ArrayOR", &ORPoolDetails, NULL);
	
	// 3. A pool which stores the callstack and name of the allocation as well as sentinel checking.  No overflow buffer.
	// Holds a simple pair structure.  Each element must is aligned to 16 bytes.
	struct sPair
	{
		jrs_u64 First;
		jrs_u32 Second;
	};
	sPoolDetails DetPoolDetails;
	DetPoolDetails.bEnableMemoryTracking = TRUE;
	DetPoolDetails.bEnableSentinel = TRUE;
	cPool *pPoolDet = cMemoryManager::Get().CreatePool(sizeof(sPair), 4, "32Bit32ArrayDet", &DetPoolDetails, NULL);

	// Allocate some memory
 	void *p32bData = pPool->AllocateMemory();

	// Note if you try to free memory like the commented out line below you will get an error.
	// cMemoryManager::Get().Free(p32bData);

	// Free it again
	pPool->FreeMemory(p32bData);

	// Allocate an entire array, giving each the name "item".  When the array is larger an error will appear and the overflow will 
	// be used.
	void *p32Bit32Array[64];
	for(int i = 0; i < 64; i++)
	{
		p32Bit32Array[i] = pPoolOR->AllocateMemory("item");
		*(jrs_sizet *)p32Bit32Array[i] = 0xffffffff;
	}
	
	// Free every other one
	for(int i = 0; i < 64; i += 2)
		pPoolOR->FreeMemory(p32Bit32Array[i]);

	cMemoryManager::Get().ReportAll("d:\\pooltest.txt");

	// Report the memory of the heap
	pPoolOR->ReportAll();

	// Free the other data of
	for(int i = 1; i < 64; i += 2)
		pPoolOR->FreeMemory(p32Bit32Array[i]);

 	// Allocate some memory for tracking
	void *p32bDet1 = pPoolDet->AllocateMemory("Hello");
	pPoolDet->FreeMemory(p32bDet1);

	// Destroy the pools
	cMemoryManager::Get().DestroyPool(pPool);
	cMemoryManager::Get().DestroyPool(pPoolDet);
	cMemoryManager::Get().DestroyPool(pPoolOR);

	// Check the memory is clear and exit
	cMemoryManager::Get().ReportAll();
	cMemoryManager::Get().Destroy();
}