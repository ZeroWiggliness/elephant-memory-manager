/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/

#ifndef _JRSMEMORY_INTERNAL_H
#define _JRSMEMORY_INTERNAL_H

#ifndef _JRSMEMORY_H
#include <JRSMemory.h>
#endif

namespace Elephant
{
	// Enhanced debugging clear value
	static const jrs_u32 MemoryManager_EDebugClearValue = 0xeeeeeeee; 

	// String length
	static const jrs_u32 MemoryManager_StringLength = 40;

	// Sentinel block values
	static const jrs_u32 MemoryManager_SentinelValueFreeBlock = 0xfdffdfdd;
	static const jrs_u32 MemoryManager_SentinelValueAllocatedBlock = 0xfacceedd;

	// Padding values and other end values
	static const jrs_u32 MemoryManager_FreeBlockPadValue = 0xabcdefab;
	static const jrs_u32 MemoryManager_FreeBlockEndValue = 0xacdcfefe;
	static const jrs_u32 MemoryManager_FreeBlockValue = 0xfefefefe;

	// Heap reporting globals
	extern jrs_bool g_ReportHeap;
	extern jrs_bool g_ReportHeapCreate;

	// Must always be 16bytes smaller than sFreeBlock block
	struct sAllocatedBlock
	{
#ifndef MEMORYMANAGER_MINIMAL
	#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
		jrs_u32 SentinelEnd[4];				// Sentinels
	#endif
#endif
		sAllocatedBlock *pNext;
		sAllocatedBlock *pPrev;
		jrs_sizet uSize;
		jrs_sizet uFlagAndUniqueAllocNumber;
#ifndef MEMORYMANAGER_MINIMAL
	#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
		jrs_i8 Name[MemoryManager_StringLength];			// Text of the allocation block.
		jrs_u32 uExternalId;								// External Id
		jrs_u32 uHeapId;									// Id check of the heap that allocated this block
		jrs_sizet uCallsStack[JRSMEMORY_CALLSTACKDEPTH];	// Callstacks.
	#endif

	#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
		jrs_u32 SentinelStart[4];			// Sentinels
	#endif
#endif
	};

	// The free block must be at least 16 bytes larger than the allocated block
	struct sFreeBlock
	{
#ifndef MEMORYMANAGER_MINIMAL
	#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
		jrs_u32 SentinelEnd[4];					// Sentinels
	#endif
#endif
		jrs_sizet uMarker;	
		jrs_sizet uSize;
		jrs_sizet uFlags, uPad2;

		sFreeBlock *pPrevBin, *pNextBin;
		sAllocatedBlock *pPrevAlloc, *pNextAlloc;
#ifndef MEMORYMANAGER_MINIMAL
	#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
		jrs_i8 Name[MemoryManager_StringLength];
		jrs_u32 uExternalId;								// External Id
		jrs_u32 uHeapId;									// Id check of the heap that allocated this block
		jrs_sizet uCallsStack[JRSMEMORY_CALLSTACKDEPTH];
	#endif

	#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS		
		jrs_u32 SentinelStart[4];				// Sentinels
	#endif
#endif
	};

	// Structure to link blocks. Identical to allocated block with extra ptrs to other linked blocks
	struct sLinkedBlock
	{
#ifndef MEMORYMANAGER_MINIMAL
#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
		jrs_u32 SentinelEnd[4];				// Sentinels
#endif
#endif
		sAllocatedBlock *pNext;
		sAllocatedBlock *pPrev;
		jrs_sizet uSize;
		jrs_sizet uFlagAndUniqueAllocNumber;
#ifndef MEMORYMANAGER_MINIMAL
#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
		jrs_i8 Name[MemoryManager_StringLength];			// Text of the allocation block.
		jrs_u32 uExternalId;								// External Id
		jrs_u32 uHeapId;									// Id check of the heap that allocated this block
		jrs_sizet uCallsStack[JRSMEMORY_CALLSTACKDEPTH];	// Callstacks.
#endif

#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
		jrs_u32 SentinelStart[4];			// Sentinels
#endif
#endif
		sLinkedBlock *pLinkedNext;
		sLinkedBlock *pLinkedPrev;
		jrs_sizet uPad1, uPad2;
	};

	// extern the default allocators.
	extern void *MemoryManagerDefaultSystemAllocator(jrs_u64 uSize, void *pExtMemoryPtr);
	extern void MemoryManagerDefaultSystemFree(void *pFree, jrs_u64 uSize);
	extern void MemoryManagerLiveViewTransfer(void *pData, int size, const char *pFilePathAndName, jrs_bool bAppend);
	extern void MemoryManagerPlatformInit(void);
	extern void MemoryManagerPlatformDestroy(void);
	extern jrs_sizet MemoryManagerPlatformAddressToBaseAddress(jrs_sizet uAddress);
	extern void MemoryManagerPlatformFunctionNameFromAddress(jrs_sizet uAddress, jrs_i8 *pName, jrs_sizet *pFuncStartAdd, jrs_sizet *pFuncSize);
	extern jrs_sizet MemoryManagerSystemPageSize(void);

} // Namespace

#endif	// _JRSMEMORY_INTERNAL_H
