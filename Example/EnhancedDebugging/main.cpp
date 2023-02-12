/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/

/*
This example demonstrates the use of Elephants Enhanced Debugging feature.  Elephant runs a separate thread in the background which free's the memory off a bit later
than it normally would.  This can catch memory problems like dangling pointers (i.e memory which has been freed but is used later by accident).  It is especially 
helpful in tracking down multi threaded problems as well.  For example you may have several threads which are using some memory.  Another thread may choose to free some of this memory
while another is potentially writing to it.  

The Enhanced Debugging feature will catch these errors by delaying some writes, filling the memory with a know value and checking shortly before correct deletion.  Please note that this will  
cause slightly different memory allocation patterns than normal use.

This example will show both features.  First it will create a dangling pointer and overwrite the memory.  The second will create several threads and demonstrate the same problem from there.
*/


// Needed for string functions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <conio.h>

#include <JRSMemory.h>

void MemoryManagerPrint(const jrs_i8 *pText)
{
	printf("%s\n", pText);
}

void MemoryManagerErrorHandle(const jrs_i8 *pError, jrs_u32 uErrorID)
{
	__debugbreak();
}

void TestDanglingPointer(void)
{
	printf("Dangling pointer test.  Allocate\n");

	// Allocate the pointer
	void *pData = cMemoryManager::Get().Malloc(65535, 0, 0, "DanglingAlloc");

	// Do some work with it
	memset(pData, 0x12, 65535);

	// Free it!
	printf("Perform test y/n?\n");
	int xkey = _getche();
	cMemoryManager::Get().Free(pData);	
	if(xkey == 'y' || xkey == 'Y')
	{
		memset(pData, 0xdd, 65535);
		printf("Dangling pointer corrupted.  Please wait for Elephant error.\n");
	}
	
}

void main(void)
{
#ifdef ELEPHANT_TRIAL
	cMemoryManager::InitializeTrial();
#endif

	cMemoryManager::InitializeCallbacks(MemoryManagerPrint, MemoryManagerErrorHandle, NULL);
	cMemoryManager::InitializeSmallHeap(4 * 1024 * 1024, 256);
	cMemoryManager::InitializeEnhancedDebugging(true);
	cMemoryManager::Get().Initialize(64 * 1024 * 1024, 0xffffffffffffffff, false);

	TestDanglingPointer();


	cMemoryManager::Get().Destroy();
}