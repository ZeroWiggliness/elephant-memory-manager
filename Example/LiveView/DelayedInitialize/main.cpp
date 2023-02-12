/* 
(C) Copyright 2007-2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 

This is a simple example showing how to use use the delayed initialization of Live View.
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
	cMemoryManager::InitializeLiveView(33, 16384, true);

	// Initialize Elephant with 96MB (or nearest if not possible) with a 32MB heap.
	cMemoryManager::Get().Initialize(96 * 1024 * 1024, 32 * 1024 * 1024);

	// Testing the allocator.
	int *newmemtest = new(__FILE__) int;
	int *newmemtestarray = new int[50];
	cFoo *pFoo = new cFoo;
	cFoo *pFooArray = new cFoo[4];

	delete newmemtest;
	delete []newmemtestarray;
	delete pFoo;
	delete []pFooArray;	

	// using Elephant as malloc - this is also overloaded in MMGr.h
	void *mVoid = malloc(16);
	free(mVoid);

	// Now init live view (and other post initialized features)
	cMemoryManager::Get().UserInitializePostFeatures();

	// Create a new heap with 32mb and store some memory there.  Note the sHeapDetails controls the heap and how it handles errors.
	// This heap will allow 0 byte allocations and to be destroyed with the allocation not freed.  Refer to the documentation 
	// about customization.
	cHeap::sHeapDetails details;
	details.bAllowDestructionWithAllocations = true;
	details.bAllowZeroSizeAllocations = true;
	cHeap *pCustomHeap = cMemoryManager::Get().CreateHeap(32 * 1024 * 1024, "Custom Heap", &details);
	
	// Direct allocation
	void *pData1 = pCustomHeap->AllocateMemory(0, 4096, JRSMEMORYFLAG_NONE, "Heap allocation");

	// Using heap overloaded new
	cFoo *pCustomHeapAllocation = new(pCustomHeap) cFoo;

	// Write out all the details before we end
	cMemoryManager::Get().ReportAll();

	// Note you do not need to call this.  The destructor will call this also but if you want to decide when all the 
	// memory should be freed this will perform that task.
	cMemoryManager::Get().Destroy();
}