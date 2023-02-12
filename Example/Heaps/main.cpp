/* 
(C) Copyright 2007-2011 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 

This is a simple example showing multiple types of heaps.  In this example we create 2 managed heaps of 32MB and 2 User heaps. By commenting in
line X it will also demonstrate mixing heaps with Elastic heaps.
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

	// Initialize Elephant with into resizable mode, no default heap.  Must not be closest.
	cMemoryManager::Get().Initialize(JRSMEMORYINITFLAG_LARGEST, 0, FALSE);

	// Create some heaps.  Allow the 2nd to be customized with NULL free warnings disabled.
	cHeap::sHeapDetails managed2details;
	managed2details.bAllowNullFree = true;

	cHeap *pManaged1 = cMemoryManager::Get().CreateHeap(32 * 1024 * 1024, "Managed1", NULL);
	cHeap *pManaged2 = cMemoryManager::Get().CreateHeap(32 * 1024 * 1024, "Managed2", &managed2details);

	// Create 2 user heaps (useful for VRAM/Audio memory most often).  For example purposes we will allocate 2x2MB blocks from the managed heaps.
	// You are free to do this if you wish without any problems but more often than not you will want to point it at other memory areas system
	// calls will return to you.
	void *pImaginarySystemPointer1 = pManaged1->AllocateMemory(2 * 1024 * 1024, 0 /* use heap default alignment */);
	void *pImaginarySystemPointer2 = pManaged2->AllocateMemory(2 * 1024 * 1024, 0 /* use heap default alignment */);

	cHeap::sHeapDetails defaultdetails;		
	defaultdetails.bHeapIsSelfManaged = true;		// Lets elephant know these heaps and managed by the user.
	cHeap *pUser1 = cMemoryManager::Get().CreateHeap(pImaginarySystemPointer1, 2 * 1024 * 1024 /*size of the memory block - getting this wrong could result in serious problems*/, "User1", &defaultdetails);
	cHeap *pUser2 = cMemoryManager::Get().CreateHeap(pImaginarySystemPointer2, 2 * 1024 * 1024 /*size of the memory block - getting this wrong could result in serious problems*/, "User2", &defaultdetails);

	// You can now allocate and free away to multiple heaps
	
	// You must destroy any user heaps.  Managed heaps are fine to be left (even with allocations if you have disabled this with the details structure)
	cMemoryManager::Get().DestroyHeap(pUser2);
	cMemoryManager::Get().DestroyHeap(pUser1);

	// Free the memory used by the heaps. You can use either cHeap::FreeMemory or the slightly slower cMemoryManager::Free (it just finds the heap it was allocated from and calls the other function)
	pManaged1->FreeMemory(pImaginarySystemPointer1);
	cMemoryManager::Get().Free(pImaginarySystemPointer2);

	// Write out all the details before we end
	cMemoryManager::Get().ReportAll();

	// Note you do not need to call this.  The destructor will call this also but if you want to decide when all the 
	// memory should be freed this will perform that task.
	cMemoryManager::Get().Destroy();
}