/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/

/*
This example demonstrates the use of pools in Elephant Memory manager to default small allocations to.
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

// Create pools globals
const jrs_u32 NumPools = 5;
cPool *g_pPool[NumPools];
cHeap *g_pDefault;

// Allocate from the memory.  THis will allocate from the right size pool first.
void *AllocateMemory(jrs_sizet uSize)
{
	if(!uSize)
		return NULL;

	if(uSize <= 4)
		return g_pPool[0]->AllocateMemory();
	else if(uSize <= 8)
		return g_pPool[1]->AllocateMemory();
	if(uSize <= 16)
		return g_pPool[2]->AllocateMemory();
	if(uSize <= 32)
		return g_pPool[3]->AllocateMemory();
	if(uSize <= 128)
		return g_pPool[4]->AllocateMemory();

	return g_pDefault->AllocateMemory(uSize, 0);
}

// This frees the memory. We get the allocation flag because the allocation from a pool overrun is set as belonging to a pool.
void FreeMemory(void *pMem)
{
	cPool *pPool = (cPool *)g_pDefault->GetPoolFromAllocatedMemory(pMem);
	if(pPool)
	{
		pPool->FreeMemory(pMem);
		return;
	}

	g_pDefault->FreeMemory(pMem, cMemoryManager::Get().GetAllocationFlag(pMem));
}

void main(void)
{
#ifdef ELEPHANT_TRIAL
	cMemoryManager::InitializeTrial();
#endif

	cMemoryManager::InitializeCallbacks(MemoryManagerPrint, MemoryManagerErrorHandle);
	cMemoryManager::Get().Initialize(64 * 1024 * 1024, 0, false);

	// Create a default heap
	g_pDefault = cMemoryManager::Get().CreateHeap(cMemoryManager::Get().GetFreeUsableMemory(), "Default Heap", NULL);

	// Create a the pools
	sPoolDetails PoolDetails;
	PoolDetails.pOverrunHeap = g_pDefault;			// Set the heap to allocate overruns in
	PoolDetails.bAllowNotEnoughSpaceReturn = true;
	g_pPool[0] = cMemoryManager::Get().CreatePool(4, 32, "4Byte", &PoolDetails, g_pDefault);
	g_pPool[1] = cMemoryManager::Get().CreatePool(8, 2, "8Byte", &PoolDetails, g_pDefault);				// Create a small pool to test overrun
	g_pPool[2] = cMemoryManager::Get().CreatePool(16, 32, "16Byte", &PoolDetails, g_pDefault);
	g_pPool[3] = cMemoryManager::Get().CreatePool(32, 32, "32Byte", &PoolDetails, g_pDefault);
	g_pPool[4] = cMemoryManager::Get().CreatePool(128, 32, "128Byte", &PoolDetails, g_pDefault);

	// Allocate random amounts of memory
	void *p1 = AllocateMemory(3);
	void *p2 = AllocateMemory(128);
	void *p3 = AllocateMemory(48);
	void *p4 = AllocateMemory(129);

	void *p8_1 = AllocateMemory(8);
	void *p8_2 = AllocateMemory(8);
	void *p8_3 = AllocateMemory(8);
	void *p8_4 = AllocateMemory(8);

	// Report it
	cMemoryManager::Get().ReportAll();

	// Free the memory
	FreeMemory(p8_1);
	FreeMemory(p8_2);
	FreeMemory(p8_3);
	FreeMemory(p8_4);
	
	FreeMemory(p1);
	FreeMemory(p2);
	FreeMemory(p3);
	FreeMemory(p4);

	cMemoryManager::Get().ReportAll();

	// Destroy the pools
	cMemoryManager::Get().DestroyPool(g_pPool[0]);
	cMemoryManager::Get().DestroyPool(g_pPool[1]);
	cMemoryManager::Get().DestroyPool(g_pPool[2]);
	cMemoryManager::Get().DestroyPool(g_pPool[3]);
	cMemoryManager::Get().DestroyPool(g_pPool[4]);

	// Check the memory is clear and exit
	cMemoryManager::Get().Destroy();
}