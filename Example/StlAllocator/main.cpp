/* 
(C) Copyright 2007-2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 

This is a simple example showing how to use Elephant as a custom allocator for the STL containers. The STL containers
can be memory intensive so we show you two examples.  One that allows an Elephant Heap and another that shows you
how to use an Elephant pool with an std::list. 

Note that its not 100% robust so if you use this code you may need to improve on it.  It is only an example.
*/

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <windows.h>
#include <list>

#include "JRSMemory.h"
#include "ElephantHeapSTLAllocator.h"
#include "ElephantHeapSTLPoolAllocator.h"

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

	// Initialize Elephant with 96MB (or nearest if not possible) with a 32MB heap.
	cMemoryManager::Get().Initialize(96 * 1024 * 1024, 0);
	cHeap::sHeapDetails details;
	details.bAllowDestructionWithAllocations = true;
	cMemoryManager::Get().CreateHeap(32 * 1024 * 1024, "Default", &details);

	// Create a scope.  When the list gets destroyed it will be destroyed AFTER we have destroyed Elephant.
	// The scope just ensures that its correct.
	{
		// Create a list and fill with data
		const int HeapNumber = 0;				// References the default created heap.
		std::list<int, ElephantHeapSTLAllocator<int, HeapNumber> > heaplist;

		// NOTE: THE POOL ALLOCATOR DOES NOT REMOVE ITSELF FROM ELEPHANT!
		std::list<int, ElephantHeapSTLPoolAllocator<int, 20> > poollist;

		// Push some random bits onto the stack and push things off again
		heaplist.push_back(18);
		heaplist.push_back(1348);
		heaplist.push_back(118);
		heaplist.push_back(85);
		heaplist.push_back(54);
		heaplist.push_back(52);
		heaplist.pop_front();
		heaplist.pop_back();

		poollist.push_back(18);
		poollist.push_back(1348);
		poollist.push_back(118);
		poollist.push_back(85);

		// Check we have allocated
		cMemoryManager::Get().ReportAll();

		heaplist.pop_front();
		heaplist.pop_front();
		heaplist.pop_back();
		heaplist.pop_back();

		poollist.pop_front();
		poollist.pop_back();
		poollist.pop_back();
		poollist.pop_back();
	}

	// Note you do not need to call this.  The destructor will call this also but if you want to decide when all the 
	// memory should be freed this will perform that task.
	cMemoryManager::Get().Destroy();
}