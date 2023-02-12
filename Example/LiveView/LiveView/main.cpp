/* 
(C) Copyright 2007-2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 

This is a simple example showing how to use LiveView.  Creates 3 heaps and does a few small allocations and frees to each.  Press any key to exit.
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

class cFoo
{
	int x;
	float y;
	void *pData;
	int p;
public:

	cFoo()
	{
		x = 0;
		y = 1.0f;
		pData = cMemoryManager::Get().Malloc(55);
		p = 2;
	}

	~cFoo()
	{
		cMemoryManager::Get().Free(pData);
	}
};

void main(void)
{
#ifdef ELEPHANT_TRIAL
	cMemoryManager::InitializeTrial();
#endif

	// Basic Memory manager initialization
	cMemoryManager::InitializeCallbacks(MemoryManagerTTYPrint, MemoryManagerErrorHandle, NULL);

	// Initialize live view
	cMemoryManager::InitializeLiveView();

	// Initialize Elephant with 128MB (or nearest if not possible) with a no heaps.
	cMemoryManager::Get().Initialize(128 * 1024 * 1024, 0);

	// Create some heaps
	cHeap::sHeapDetails details;
	details.bAllowDestructionWithAllocations = true;
	details.bAllowZeroSizeAllocations = true;
	details.bAllowNotEnoughSpaceReturn = true;

	cHeap *pHeap0 = cMemoryManager::Get().CreateHeap(32 * 1024 * 1024, "Main Heap", &details);
	cHeap *pHeap1 = cMemoryManager::Get().CreateHeap(32 * 1024 * 1024, "Small Heap", &details);
	cHeap *pHeap2 = cMemoryManager::Get().CreateHeap(32 * 1024 * 1024, "Other Heap", &details);

	bool allocation = true;

	int heapallocs = 0;
	void *pHeap0Allocations[10];
	void *pHeap1Allocations[10];
	void *pHeap2Allocations[10];

	// Clear
	for(int i = 0; i < 10; i++)
	{
		pHeap0Allocations[i] = NULL;
		pHeap1Allocations[i] = NULL;
		pHeap2Allocations[i] = NULL;
	}

	printf("\n*** Press <Enter> to terminate ***\n");
	while(!GetAsyncKeyState(VK_RETURN))
	{
		if(allocation)
		{
			pHeap0Allocations[heapallocs] = pHeap0->AllocateMemory(3 << 20, 0, 0, "Heap 0 Allocation");
			pHeap1Allocations[heapallocs] = pHeap1->AllocateMemory(1 << 20, 0, 0, "Heap 1 Allocation");
			pHeap2Allocations[heapallocs] = pHeap2->AllocateMemory(65536, 0, 0, "Heap 2 Allocation");
			heapallocs++;
			if(heapallocs == 10)
				allocation = false;
		}
		else
		{
			heapallocs--;
			pHeap0->FreeMemory(pHeap0Allocations[heapallocs], 0, "Heap 0 Free");
			pHeap1->FreeMemory(pHeap1Allocations[heapallocs], 0, "Heap 1 Free");
			pHeap2->FreeMemory(pHeap2Allocations[heapallocs], 0, "Heap 2 Free");
			if(heapallocs == 0)
				allocation = true;
		}

		Sleep(33);
	}
	
	// Note you do not need to call this.  The destructor will call this also but if you want to decide when all the 
	// memory should be freed this will perform that task.
	cMemoryManager::Get().Destroy();
}