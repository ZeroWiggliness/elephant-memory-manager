// Needed for string functions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// Needed for string functions
#include <JRSMemory.h>
#include <JRSMemory_Pools.h>
#include "JRSMemory_Internal.h"
#include "JRSMemory_ErrorCodes.h"

// Defines to force inlining of some components
#define HEAP_THREADLOCK if(m_bThreadSafe) { m_pThreadLock->Lock(); }
#define HEAP_THREADUNLOCK if(m_bThreadSafe) { m_pThreadLock->Unlock(); }

#define HEAP_FULLSIZE_CALC(x, min) (x > min ? ((x + 0xf) & ~(0xf)) : min)
#define HEAP_FULLSIZE(x) HEAP_FULLSIZE_CALC(x, m_uMinAllocSize)

// Elephant Namespace
namespace Elephant
{
	extern jrs_u64 g_uBaseAddressOffsetCalculation;

	//  Description:
	//		cHeap constructor.  Private and should not be called.  Use CreateHeap to create a heap.
	//  See Also:
	//		CreateHeap
	//  Arguments:
	//		None.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		cHeap constructor.
	cHeap::cHeap()
	{
		// Not callable.
	}

	//  Description:
	//		cHeap constructor.  Should not be called by the user.  Use CreateHeap to create a heap.
	//  See Also:
	//		CreateHeap
	//  Arguments:
	//		pMemoryAddress - Memory address aligned to 16bytes to create the heap too.
	//		uSize - Size in bytes of the heap.  16 byte multiples.
	//		pHeapName - Name of the heap upto 32bytes.
	//		pHeapDetails - Valid pointer to a cHeap::sHeapDetails structure which contains the heap details to 
	//						modify any user parameters.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		cHeap constructor.  Use CreateHeap.
	cHeap::cHeap(void *pMemoryAddress, jrs_sizet uSize, const jrs_i8 *pHeapName, sHeapDetails *pHeapDetails)
	{
		// This doesn't REALLY need this as it comes in before.  But sometimes a heap outside the manager may be required.
		sHeapDetails details;
		if(!pHeapDetails)
			pHeapDetails = &details;

		// Create the heap with the variables
		m_pHeapStartAddress = (jrs_i8 *)pMemoryAddress;
		m_pHeapEndAddress = m_pHeapStartAddress + uSize - sizeof(sFreeBlock);			// - the freeblock.  That means it will always fit in the end without overwrites
		m_uHeapSize = uSize;

		// set up some details
		m_uMinAllocSize = pHeapDetails->uMinAllocationSize < (sizeof(jrs_sizet) << 2) ? (sizeof(jrs_sizet) << 2) : pHeapDetails->uMinAllocationSize;
		m_uMaxAllocSize = pHeapDetails->uMaxAllocationSize;

		m_bHeapIsMemoryManagerManaged = !pHeapDetails->bHeapIsSelfManaged;
		m_bHeapIsInUse = true;

		// Enable logging   
		m_bEnableLogging = pHeapDetails->bEnableLogging;

		// Thread safety. Continuous/live view will always force this to true.
		m_bThreadSafe = pHeapDetails->bThreadSafe;
		if(cMemoryManager::Get().m_bEnableLiveView || cMemoryManager::Get().m_bEnhancedDebugging)
			m_bThreadSafe = true;

		// Default callstack depth.
		m_uCallstackDepth = 2;

		// Clear some blocks
		m_pAllocList = 0;

		// Now we set up one giant free block.
		InitializeMainFreeBlock();

		// Other defaults
		m_uDefaultAlignment = pHeapDetails->uDefaultAlignment;		
		m_bUseEndAllocationOnly = pHeapDetails->bUseEndAllocationOnly;
		m_bReverseFreeOnly = pHeapDetails->bReverseFreeOnly;			
		m_bAllowNullFree = pHeapDetails->bAllowNullFree;			
		m_bAllowZeroSizeAllocations = pHeapDetails->bAllowZeroSizeAllocations;
		m_bAllowDestructionWithAllocations = pHeapDetails->bAllowDestructionWithAllocations;
		m_bLocked = false;
		m_bAllowNotEnoughSpaceReturn = pHeapDetails->bAllowNotEnoughSpaceReturn;

		// Clearing
		m_bHeapClearing = pHeapDetails->bHeapClearing;
		m_uHeapAllocClearValue = pHeapDetails->uHeapAllocClearValue;				// Clear value for allocation.
		m_uHeapFreeClearValue = pHeapDetails->uHeapFreeClearValue;					// Clear value for frees.
		m_bEnableEnhancedDebug = pHeapDetails->bEnableEnhancedDebug;				// Defers all frees to the enhanced debugging thread.

		// Debug set up
		m_bEnableErrors = pHeapDetails->bEnableErrors;				
		m_bErrorsAsWarnings = pHeapDetails->bErrorsAsWarnings;			
		m_bEnableExhaustiveErrorChecking = pHeapDetails->bEnableExhaustiveErrorChecking;
		m_bEnableSentinelChecking = pHeapDetails->bEnableSentinelChecking;	

		// System callbacks for allocation
		m_systemAllocator = pHeapDetails->systemAllocator ? pHeapDetails->systemAllocator : cMemoryManager::Get().m_MemoryManagerDefaultAllocator;
		m_systemFree = pHeapDetails->systemAllocator ? pHeapDetails->systemFree : cMemoryManager::Get().m_MemoryManagerDefaultFree;
		m_systemPageSize = pHeapDetails->systemAllocator ? pHeapDetails->systemPageSize : cMemoryManager::Get().m_MemoryManagerDefaultSystemPageSize;
		m_systemOpCallback = pHeapDetails->systemOpCallback;
		
		m_uUniqueAllocCount = 0;
		m_uUniqueFreeCount = 0;

		m_uAllocatedCount = 0;
		m_uAllocatedSize = 0;
		m_uAllocatedSizeMax = 0;
		m_uAllocatedCountMax = 0;

		// copy the name
		HeapWarning(strlen(pHeapName) < 32, JRSMEMORYERROR_HEAPNAMETOLARGE, "Heap Name is to large");
		strcpy(m_HeapName, pHeapName);
		m_uHeapId = 0;

		// Resizable
		m_bResizable = cMemoryManager::Get().m_bResizeable;
		m_uResizableSizeMin = pHeapDetails->uResizableSize;	
		m_pResizableLink = NULL;
		m_uReclaimSize = pHeapDetails->uReclaimSize < m_uResizableSizeMin ? m_uResizableSizeMin : pHeapDetails->uReclaimSize;
		m_bAllowResizeReclaimation = pHeapDetails->bAllowResizeReclaimation;

		m_uDebugFlags = 0;
		m_uDebugTrapOnFreeNum = 0;
		m_uEDebugPending = 0;

		// Pools
		m_pAttachedPools = NULL;

		// Enable logging in this heap for warnings
		m_bEnableReportsInErrors = true;

		// Log us some info
		cMemoryManager::Get().ContinuousLogging_Operation(cMemoryManager::eContLog_CreateHeap, this, NULL, 0);
	}

	//  Description:
	//		cHeap destructor.  Should not be called by the user.  Use DestroyHeap.
	//  See Also:
	//		DestroyHeap
	//  Arguments:
	//		None
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		cHeap destructor.  Use DestroyHeap.
	cHeap::~cHeap()
	{
	}

	//  Description:
	//		Main call to allocate memory directly to the heap.  This function is called by cMemoryManager::Malloc.  Call this to
	//		avoid the extra overhead of malloc or to redirect to a specific heap.  This function is the main allocation function
	//		to use. 
	//		Flags are set by the user.  It can be one of JRSMEMORYFLAG_xxx or any user specified flags > JRSMEMORYFLAG_RESERVED3
	//		but smaller than or equal to 15, values greater than 15 will be lost and operation of AllocateMemory is undefined.  Input text is
	//		limited to 32 chars including terminator.  Strings longer than this will only store the last 31 chars.
	//		Alignment must be a power of two however a 0 will default to the default allocation size set when the heap was created.
	//  See Also:
	//		FreeMemory, cMemoryManager::Malloc
	//  Arguments:
	//      uSize - Size in bytes. Minimum size will be 16bytes unless the heap settings have set a larger minimum size.
	//		uAlignment - Default alignment is 16bytes unless heap settings have set a larger alignment.
	//					 Any specified alignments must be a power of 2. Setting 0 will default to the minimum requested alignment of the heap.
	//		uFlag - One of JRSMEMORYFLAG_xxx or user defined value.  Default JRSMEMORYFLAG_NONE.  See description for more details.
	//		pName - NULL terminating text string to associate with the allocation. May be NULL.
	//		uExternalId - An identifier to associate with the allocation.
	//  Return Value:
	//      Valid pointer to allocated memory.
	//		NULL otherwise.
	//  Summary:
	//      Allocates memory with additional information.
	void *cHeap::AllocateMemory(jrs_sizet uSize, jrs_u32 uAlignment, jrs_u32 uFlag, const jrs_i8 *pName, const jrs_u32 uExternalId)
	{
#ifndef MEMORYMANAGER_MINIMAL
		if(!cMemoryManager::Get().IsInitialized())
		{
			HeapWarning(cMemoryManager::Get().IsInitialized(), JRSMEMORYERROR_NOTINITIALIZED, "Elephant is not initialized.");
			return 0;
		}
#endif

		// Cannot allocate if the heap is locked.
		if(IsLocked())
		{
			HeapWarning(!IsLocked(), JRSMEMORYERROR_LOCKED, "Heap is locked.  You may not allocate memory.");
			return 0;
		}

		// Select heap default alignment if 0
		if(!uAlignment)
			uAlignment = m_uDefaultAlignment;

		// Check the alignment is power of 2 and 16 or greater
		HeapWarning(!(uAlignment & (uAlignment - 1)), JRSMEMORYERROR_INVALIDALIGN, "Cannot allocate memory because the alignment is non power of 2");

#ifndef MEMORYMANAGER_MINIMAL
		if(uAlignment < m_uDefaultAlignment)
		{
			HeapWarning(uAlignment >= m_uDefaultAlignment, JRSMEMORYERROR_INVALIDALIGN, "Cannot allocate memory because the alignment is smaller than the default alignment (%d bytes)", m_uDefaultAlignment);
			return 0;
		}
#endif

		// Check for a 0 allocation.  This is important.  Sometimes we want to allow this.  
		jrs_sizet uASize = uSize;
		if(!uSize)
		{
			if(m_bAllowZeroSizeAllocations)
				uASize = m_uMinAllocSize;
			else
			{
				HeapWarning(uASize > 0, JRSMEMORYERROR_ZEROBYTEALLOC, "Cannot allocate because we are allocating a 0 byte allocation.  Set the heap 'bAllowZeroSizeAllocations' flag to enable this");
				return 0;
			}
		}

		// We are allocating memory.  Bump the size up to the minimum allowed for the heap.
		uASize = HEAP_FULLSIZE(uASize);

		// Check if the size is larger than the maximum size allowed
#ifndef MEMORYMANAGER_MINIMAL
		if(m_uMaxAllocSize && uASize > m_uMaxAllocSize)
		{
			HeapWarning(uASize <= m_uMaxAllocSize, JRSMEMORYERROR_SIZETOLARGE, "Size requested from the heap (%s) is larger than the maximum size allowed (%d bytes)", m_HeapName, m_uMaxAllocSize);
			return 0;
		}
#endif
		// Safe to lock
		HEAP_THREADLOCK

		// Do a best fit if this heap allows it
		sFreeBlock *pFreeBlock = m_pMainFreeBlock;
		if(!m_bUseEndAllocationOnly)
		{
			// We need to find out if we can fill a free block but only if we have a valid freelist
			pFreeBlock = SearchForFreeBlockBinFit(uASize, uAlignment);
		}

		// Nothing found in the free lists or have first fit only.  Check if the main free block has enough room and split it to fit.
		sAllocatedBlock *pNewBlock = AllocateFromFreeBlock(pFreeBlock, uSize, uAlignment, uFlag);

		// We may need to return null
		if(!pNewBlock)
		{
			// If Elephant is in resize mode then we see if we can resize here and then retry the allocation
			if(cMemoryManager::Get().m_bResizeable && m_bHeapIsMemoryManagerManaged)
			{
				if(cMemoryManager::Get().InternalResizeHeap(this, uASize + uAlignment))
				{
					// Resized Elephant, now try allocating again
					pFreeBlock = m_pMainFreeBlock;
					if(!m_bUseEndAllocationOnly)
					{
						pFreeBlock = SearchForFreeBlockBinFit(uASize, uAlignment);
					}	
					pNewBlock = AllocateFromFreeBlock(pFreeBlock, uSize, uAlignment, uFlag);
				}

				// Try one last time
				if(!pNewBlock)
				{
					HEAP_THREADUNLOCK
						return 0;
				}

				// Success
			}
			else
			{
				// Just fail
				HEAP_THREADUNLOCK
				return 0;
			}
		}

#ifndef MEMORYMANAGER_MINIMAL
#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
		// Set the names etc and if we are on a supported platform get the stack trace
		if(pName && pName[0])
		{
			jrs_u32 uNameM = 0, uNameL = (jrs_u32)strlen(pName);
			const jrs_i8 *pN = &pName[(uNameL > (MemoryManager_StringLength - 1)) ? uNameL - (MemoryManager_StringLength - 1) : 0];
			while(*pN)
			{
				pNewBlock->Name[uNameM] = (*pN == ';') ? '.' : *pN;
				uNameM++;
				pN++;
			}
			pNewBlock->Name[uNameM] = 0;
		}
		else
			strcpy(pNewBlock->Name, "Unknown Allocation");
		pNewBlock->uExternalId = uExternalId;
		pNewBlock->uHeapId = m_uHeapId;
		cMemoryManager::Get().StackTrace(pNewBlock->uCallsStack, m_uCallstackDepth, JRSMEMORY_CALLSTACKDEPTH);
#endif
		if(m_bEnableLogging)
		{
			cMemoryManager::Get().ContinuousLogging_HeapOperation(cMemoryManager::eContLog_Allocate, this, pNewBlock, uAlignment, 0);
		}
#endif

		HEAP_THREADUNLOCK

		// New memory address
		void *pAllocation = (void *)((jrs_i8 *)pNewBlock + sizeof(sAllocatedBlock));

		// Clear the new memory if needed
		if(m_bHeapClearing)
		{
			memset(pAllocation, m_uHeapAllocClearValue, HEAP_FULLSIZE(pNewBlock->uSize));
		}		

		// Return the correct address
		return pAllocation;
	}

	//  Description:
	//		Reallocates memory.  Generally should be avoided if at all possible as most of the time it will just Free and Allocate except
	//		for some circumstances.  If you are constantly reallocating it is recommended to see if there is a better alternative.
	//		Flags are set by the user.  It can be one of JRSMEMORYFLAG_xxx or any user specified flags > JRSMEMORYFLAG_RESERVED3
	//		but smaller than or equal to 15, values greater than 15 will be lost and operation of AllocateMemory is undefined.  Input text is
	//		limited to 32 chars including terminator.  Strings longer than this will only store the last 31 chars.
	//  See Also:
	//		FreeMemory, cMemoryManager::Malloc
	//  Arguments:
	//      uSize - Size in bytes. Minimum size will be 16bytes unless the heap settings have set a larger minimum size.
	//		uAlignment - Default alignment is 16bytes unless heap settings have set a larger alignment.
	//					 Any specified alignments must be a power of 2.  
	//		uFlag - One of JRSMEMORYFLAG_xxx or user defined value.  Default JRSMEMORYFLAG_NONE.  See description for more details.
	//		pName - NULL terminating text string to associate with the allocation. May be NULL.
	//		uExternalId - An Id that to associate with the allocation.  Default 0.
	//  Return Value:
	//      Valid pointer to allocated memory.
	//		NULL otherwise.
	//  Summary:
	//      Allocates memory with additional information.
	void *cHeap::ReAllocateMemory(void *pMemory, jrs_sizet uSize, jrs_u32 uAlignment, jrs_u32 uFlag, const jrs_i8 *pName, const jrs_u32 uExternalId)
	{
		// Note reallocating memory does nothing special atm unless the size fits in the current block.  It simply free's and allocates copying the
		// data over.  This could be improved coping with the cases where it could be resized in place.  Unfortunately most of the time the block immediately after
		// is normally (more than 99% of the time) occupied so it has to be reallocated anyway.  

		// Null memory can just be allocated through the standard approach
		if(!pMemory)
			return AllocateMemory(uSize, uAlignment, uFlag, pName);

		// 0 size just frees memory
		if(!uSize)
		{
			FreeMemory(pMemory, uFlag, pName);
			return 0;
		}

		// Get the block.
		sAllocatedBlock *pBlock = (sAllocatedBlock *)((jrs_sizet)pMemory - sizeof(sAllocatedBlock));

		// Same size doesn't do anything.
		if(HEAP_FULLSIZE(pBlock->uSize) == HEAP_FULLSIZE(uSize))
			return pMemory;

		// Allocated memory has several choices.  Smaller and it can be resized down and a new hole created.  Larger it can just be expanded if there is room OR
		// it will create a new block, copy the data and free the old block returning the new value.

		// Note check the alignment if we do go for full on reallocation status.		

		// Quick but dirty
		if(HEAP_FULLSIZE(pBlock->uSize) > HEAP_FULLSIZE(uSize))
			return pMemory;

		// Larger
		void *pNewMem = AllocateMemory(uSize, uAlignment, uFlag, pName, uExternalId);
		memcpy(pNewMem, pMemory, HEAP_FULLSIZE(pBlock->uSize));
		FreeMemory(pMemory, uFlag, pName, uExternalId);

		// Return the new memory
		return pNewMem;
	}

	//  Description:
	//		Main memory free function.  Internal only.
	//  See Also:
	//		AllocateMemory
	//  Arguments:
	//      pMemory - Valid memory pointer. A null pointer may only be passed in if bAllowNullFree is set in the heap CreateHeap details.
	//		uFlag - One of JRSMEMORYFLAG_xxx or user defined value.
	//		pName - NULL terminating text string to associate with the allocation. May be NULL.
	//		uExternalId - External id.  Default 0.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Frees memory from within a heap.
	void cHeap::InternalFreeMemory(void *pMemory, jrs_u32 uFlag, const jrs_i8 *pName, const jrs_u32 uExternalId)
	{
		// Heap size calculation
		// Total heap size always includes a sizeof(sFreeBlock) and a minimum allocation size and a sAllocatedBlock
		// Free size of a free block is always the end - start - sizeof(sAllocatedBlock).

		// Increment the count
		m_uUniqueFreeCount++;

		// Get the block pointer
		sAllocatedBlock *pBlock = (sAllocatedBlock *)((jrs_i8*)pMemory - sizeof(sAllocatedBlock));
		sAllocatedBlock *pPrev = pBlock->pPrev;
		sAllocatedBlock *pNext = pBlock->pNext;

		// Clear the new memory if needed
		if(m_bHeapClearing)
		{
			memset(pMemory, m_uHeapFreeClearValue, HEAP_FULLSIZE(pBlock->uSize));
		}

		// Decrease the allocated amount
#ifndef MEMORYMANAGER_MINIMAL
		jrs_sizet blocksize = HEAP_FULLSIZE(pBlock->uSize);
#endif
		m_uAllocatedSize -= pBlock->uSize;
		m_uAllocatedCount--;

		// If both previous and next are null then it was the only allocated one.  We just reset the free block to the initial value.
		if(!pPrev && !pNext)
		{
			InitializeMainFreeBlock();
			m_pAllocList = 0;

#ifndef MEMORYMANAGER_MINIMAL
#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
			// Set the names etc and if we are on a supported platform get the stack trace
			if(pName && pName[0])
			{
				jrs_u32 uNameM = 0, uNameL = (jrs_u32)strlen(pName);
				const jrs_i8 *pN = &pName[(uNameL > (MemoryManager_StringLength - 1)) ? uNameL - (MemoryManager_StringLength - 1) : 0];
				while(*pN)
				{
					m_pMainFreeBlock->Name[uNameM] = (*pN == ';') ? '.' : *pN;
					uNameM++;
					pN++;
				}
				m_pMainFreeBlock->Name[uNameM] = 0;
			}
			else
				strcpy(m_pMainFreeBlock->Name, "Unknown Free");
			m_pMainFreeBlock->uExternalId = uExternalId;
			m_pMainFreeBlock->uHeapId = m_uHeapId;
			cMemoryManager::Get().StackTrace(m_pMainFreeBlock->uCallsStack, m_uCallstackDepth, JRSMEMORY_CALLSTACKDEPTH);
#endif
			if(m_bEnableLogging)
			{
				cMemoryManager::Get().ContinuousLogging_HeapOperation(cMemoryManager::eContLog_Free, this, m_pMainFreeBlock, 0, blocksize);
			}
#endif
			return;
		}

		// With any size of memory allocation the extra padding can be ignored when going back.  Their for we can work out the exact position that,
		// potentially due to alignment may sometimes save some memory.  This block is then recalculated in the if(pPrev) block below.
		sFreeBlock *pNewFreeBlock = (sFreeBlock *)pBlock;
		sFreeBlock *pFBPrev = 0;
		sFreeBlock *pFBNext = 0;	
		sFreeBlock *pFBPrevBin = 0;
		sFreeBlock *pFBNextBin = 0;

		// Memory size may be incorrect in some situations.  When a memory block is resized it may not align correctly.  We use the next pointer to 
		// work this out along with minimum size etc.

		// Check if there is a free block after this one.  If so we can consolidate it.
		if(pNext)
		{
			// Remove this block from the list by changing the pNext pointers.
			pNext->pPrev = pPrev;

			// Base 
			sAllocatedBlock *pActualNext = (sAllocatedBlock *)((jrs_i8 *)pBlock + HEAP_FULLSIZE(pBlock->uSize) + sizeof(sAllocatedBlock));
			jrs_sizet uSizeBetween = (jrs_sizet)((jrs_i8 *)pNext - (jrs_i8 *)pActualNext);

			if(uSizeBetween >= sizeof(sAllocatedBlock) + m_uMinAllocSize)
			{
				pFBNext = (sFreeBlock *)pActualNext;
				MemoryWarning(pFBNext->uMarker == MemoryManager_FreeBlockValue, JRSMEMORYERROR_UNKNOWNCORRUPTION, "pNext isnt a free block.  Precalculating back size failed.");
			}		
		}

		// Check if there is an allocation before this
		if(pPrev)
		{
			// Remove this block from the list by changing the pPrev pointers.
			pPrev->pNext = pNext;

			// Use the next pointer to recalculate
			sAllocatedBlock *pActualNext = (sAllocatedBlock *)((jrs_i8 *)pPrev + HEAP_FULLSIZE(pPrev->uSize) + sizeof(sAllocatedBlock));
			jrs_sizet uSizeBetween = (jrs_sizet)((jrs_i8 *)pBlock - (jrs_i8 *)pActualNext);

			// Previous allocation.  Check if there is a free block between the prev and this block
			if(uSizeBetween >= sizeof(sAllocatedBlock) + m_uMinAllocSize)
			{
				pFBPrev = (sFreeBlock *)pActualNext;
				MemoryWarning(pFBPrev->uMarker == MemoryManager_FreeBlockValue, JRSMEMORYERROR_UNKNOWNCORRUPTION, "pPrev isnt a free block.  Precalculating back size failed.");
			}
			else			// However if it was padded we need to move it back more.
				pNewFreeBlock = (sFreeBlock *)pActualNext;
		}

		// Consolidate
		jrs_bool bProcessLostChunk = false;

		// We have one of several situations here (where x is the free block we are freeing.  A = Allocated, F = Free block.
		// 1.  AAFxFAA .  Both pFBPrev and pFBNext are valid.  
		// 2.  AAxAA.	Both pFBPrev and pFBNext are null.  We slot it in at the end with the main free block.
		// 3.  AAFxAA.	There pFBPrev
		// 4.  AAxFAA.
		// 5.  xAAAAA.
		// 6.  FxAAAA.
		// 7.  xFAAAA.
		// 8.  AAAAAAx.
		// 9.  AAAAAFx.
		// Condition 5&6 needs a special check too. As it happens once it does this it can be operated on by conditions 2,3 & 4.
		// m_pAllocList will always be the first allocation so if the block matches this one then its the first in the list.
		if(pBlock == m_pAllocList)
		{
			//Performance warning:  If the alignment is not 16 then free blocks may be split into none aligned pointers. Ensure the minimum allocation size is same as the alignment for maximum speed.

			// Check if there is an earlier free block (ie condition 6)
			sFreeBlock *pPotentialFirst = (sFreeBlock *)(m_pHeapStartAddress + sizeof(sAllocatedBlock) + m_uMinAllocSize);

			// This tests condition 5
			if((jrs_i8 *)m_pHeapStartAddress == (jrs_i8 *)m_pAllocList)
			{
				// It does nothing.  It will be handled correctly below
			}
			else if((jrs_i8 *)pPotentialFirst <= (jrs_i8 *)m_pAllocList)
			{
				// We have an earlier block.
				pFBPrev = (sFreeBlock *)m_pHeapStartAddress;
				MemoryWarning(pFBPrev->uMarker == MemoryManager_FreeBlockValue, JRSMEMORYERROR_UNKNOWNCORRUPTION, "Large error.  The free block does not start here.");
			}
			else
			{
				// We have a block that has just been moved on and has 'vanished'.  Perform extra calculations to cope with this case later on.
				bProcessLostChunk = true;
			}

			// Set the new alloc list pointer
			m_pAllocList = pNext;
		}

		// This traps conditions 8 & 9
		if(pPrev && !pPrev->pNext)
		{
			// pFBPrev calculation is not needed - this is calculated above.
			if(pFBPrev && pFBPrev < (sFreeBlock *)m_pMainFreeBlock->pPrevAlloc)
			{
				// Move it back to cope with condition 9.  pFBPrev will also have a bin connected to it so we must remove that.
				pNewFreeBlock = pFBPrev;

				// Clean up the bins
				RemoveBinAllocation(pFBPrev);
			}

			// Set it.  Note this will only ever be the end block.
			pNewFreeBlock->uMarker = MemoryManager_FreeBlockEndValue;
			pNewFreeBlock->pNextAlloc = pNext;
			pNewFreeBlock->pPrevAlloc = pPrev;
			pNewFreeBlock->pNextBin = 0;			// End block is always 0 here.
			pNewFreeBlock->pPrevBin = 0;
			pNewFreeBlock->uFlags = m_uUniqueFreeCount;
			pNewFreeBlock->uPad2 = MemoryManager_FreeBlockPadValue;
			pNewFreeBlock->uSize = (jrs_sizet)((jrs_i8 *)m_pHeapEndAddress - (jrs_i8 *)pNewFreeBlock);

			// Move the main free block back
			m_pMainFreeBlock = pNewFreeBlock;
		}
		else

			// Condition 1.
			if(pFBPrev && pFBNext)
			{
				RemoveBinAllocation(pFBPrev);
				RemoveBinAllocation(pFBNext);

				// We have a AAfxfAA situation.  The hardest one to deal with.  We must remove the next and expand the first.  Sometimes the next can look at the prev and
				// simple removal can cause the free list to create a circular dependency.
				pNewFreeBlock = pFBPrev;
				CreateBinAllocation((jrs_sizet)((jrs_i8 *)pNext - (jrs_i8 *)pNewFreeBlock), pNewFreeBlock, &pFBPrevBin, &pFBNextBin);
				pNewFreeBlock->uMarker = MemoryManager_FreeBlockValue;	
				pNewFreeBlock->pNextAlloc = pNext;
				pNewFreeBlock->pPrevAlloc = pPrev;
				pNewFreeBlock->pNextBin = pFBNextBin;
				pNewFreeBlock->pPrevBin = pFBPrevBin;
				pNewFreeBlock->uFlags = m_uUniqueFreeCount;
				pNewFreeBlock->uPad2 = MemoryManager_FreeBlockPadValue;
				pNewFreeBlock->uSize = (jrs_sizet)((jrs_i8 *)pNext - (jrs_i8 *)pNewFreeBlock);
			}
			// Condition 2
			else if(!pFBPrev && !pFBNext)
			{		
				// Find the bin and link it in where possible
				CreateBinAllocation((jrs_sizet)((jrs_i8 *)pNext - (jrs_i8 *)pNewFreeBlock), pNewFreeBlock, &pFBPrevBin, &pFBNextBin);

				// We have a AAxAA situation.  Easy to deal with we just plug it in at the end.
				pNewFreeBlock->uMarker = MemoryManager_FreeBlockValue;	
				pNewFreeBlock->pNextAlloc = pNext;
				pNewFreeBlock->pPrevAlloc = pPrev;
				pNewFreeBlock->pNextBin = pFBNextBin;
				pNewFreeBlock->pPrevBin = pFBPrevBin;
				pNewFreeBlock->uFlags = m_uUniqueFreeCount;
				pNewFreeBlock->uPad2 = MemoryManager_FreeBlockPadValue;
				pNewFreeBlock->uSize = (jrs_sizet)((jrs_i8 *)pNext - (jrs_i8 *)pNewFreeBlock);
			}
			// Condition 3
			else if(pFBPrev && !pFBNext)
			{
				// The bins here will need consolidating into a larger block.  In the mean time just remove them.
				RemoveBinAllocation(pFBPrev);	

				// We have a AAfxAA situation.  Easy to deal with we just move it all around.
				pNewFreeBlock = pFBPrev;
				CreateBinAllocation((jrs_sizet)((jrs_i8 *)pNext - (jrs_i8 *)pNewFreeBlock), pNewFreeBlock, &pFBPrevBin, &pFBNextBin);
				pNewFreeBlock->uMarker = MemoryManager_FreeBlockValue;	
				pNewFreeBlock->pNextAlloc = pNext;
				pNewFreeBlock->pPrevAlloc = pPrev;
				pNewFreeBlock->pNextBin = pFBNextBin;
				pNewFreeBlock->pPrevBin = pFBPrevBin;
				pNewFreeBlock->uFlags = m_uUniqueFreeCount;
				pNewFreeBlock->uPad2 = MemoryManager_FreeBlockPadValue;
				pNewFreeBlock->uSize = (jrs_sizet)((jrs_i8 *)pNext - (jrs_i8 *)pNewFreeBlock);
			}
			// condition 4
			else if(!pFBPrev && pFBNext)
			{
				// The bins here will need consolidating into a larger block.  In the mean time just remove them.
				RemoveBinAllocation(pFBNext);
				CreateBinAllocation((jrs_sizet)((jrs_i8 *)pNext - (jrs_i8 *)pNewFreeBlock), pNewFreeBlock, &pFBPrevBin, &pFBNextBin);

				// We have a AAxfAA situation.  Easy to deal with we just move it all back.
				pNewFreeBlock->uMarker = MemoryManager_FreeBlockValue;	
				pNewFreeBlock->pNextAlloc = pNext;
				pNewFreeBlock->pPrevAlloc = pPrev;
				pNewFreeBlock->pNextBin = pFBNextBin;
				pNewFreeBlock->pPrevBin = pFBPrevBin;
				pNewFreeBlock->uFlags = m_uUniqueFreeCount;
				pNewFreeBlock->uPad2 = MemoryManager_FreeBlockPadValue;
				pNewFreeBlock->uSize = (jrs_sizet)((jrs_i8 *)pNext - (jrs_i8 *)pNewFreeBlock);
			}
			else
			{
				cMemoryManager::DebugError(__LINE__, "Should never reach here.  FATAL ERROR.");
			}

			// Potentially we will have a lost chunk that would of been processed by 1 of 3 above.  This deals with it.
			if(bProcessLostChunk)
			{
				MemoryWarning(pNewFreeBlock != (sFreeBlock *)m_pHeapStartAddress, JRSMEMORYERROR_UNKNOWNCORRUPTION, "Heap start address should not be the same as the free block.  There is an error somewhere.");

				RemoveBinAllocation(pNewFreeBlock);

				// Move it back a bit.
				pNewFreeBlock = (sFreeBlock *)m_pHeapStartAddress;
				CreateBinAllocation((jrs_sizet)((jrs_i8 *)pNext - (jrs_i8 *)pNewFreeBlock), pNewFreeBlock, &pFBPrevBin, &pFBNextBin);
				pNewFreeBlock->uMarker = MemoryManager_FreeBlockValue;	
				pNewFreeBlock->pNextAlloc = pNext;
				pNewFreeBlock->pPrevAlloc = pPrev;
				pNewFreeBlock->pNextBin = pFBNextBin;
				pNewFreeBlock->pPrevBin = pFBPrevBin;
				pNewFreeBlock->uFlags = m_uUniqueFreeCount;
				pNewFreeBlock->uPad2 = MemoryManager_FreeBlockPadValue;
				pNewFreeBlock->uSize = (jrs_sizet)((jrs_i8 *)pNext - (jrs_i8 *)pNewFreeBlock);
			}

			// Adjust the bin pointers but only if they don't point to the same block.  This is because we run a circular buffer of pointers
			if(pFBNextBin && pFBNextBin != pNewFreeBlock)
			{
				pFBNextBin->pPrevBin = pNewFreeBlock;
				pFBPrevBin->pNextBin = pNewFreeBlock;
			}

			// Set the Sentinels
#ifndef MEMORYMANAGER_MINIMAL
#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
			SetSentinelsFreeBlock(pNewFreeBlock);
#endif

#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
			// Set the names etc and if we are on a supported platform get the stack trace
			if(pName && pName[0])
			{
				jrs_u32 uNameM = 0, uNameL = (jrs_u32)strlen(pName);
				const jrs_i8 *pN = &pName[(uNameL > (MemoryManager_StringLength - 1)) ? uNameL - (MemoryManager_StringLength - 1) : 0];
				while(*pN)
				{
					pNewFreeBlock->Name[uNameM] = (*pN == ';') ? '.' : *pN;
					uNameM++;
					pN++;
				}
				pNewFreeBlock->Name[uNameM] = 0;
			}
			else
				strcpy(pNewFreeBlock->Name, "Unknown Free");
			pNewFreeBlock->uExternalId = uExternalId;
			pNewFreeBlock->uHeapId = m_uHeapId;
			cMemoryManager::Get().StackTrace(pNewFreeBlock->uCallsStack, m_uCallstackDepth, JRSMEMORY_CALLSTACKDEPTH);
#endif
#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
			CheckFreeBlockSentinels(pNewFreeBlock);
			if(m_bEnableExhaustiveErrorChecking)
				CheckForErrors();
#endif
			if(m_bEnableLogging)
			{
				cMemoryManager::Get().ContinuousLogging_HeapOperation(cMemoryManager::eContLog_Free, this, pNewFreeBlock, 0, blocksize);
			}
#endif

			// Handles reclamation
			if(m_bResizable && m_bAllowResizeReclaimation)
			{
				InternalReclaimMemory();
			}
	}


	//  Description:
	//		Frees memory allocated from AllocateMemory or Malloc.  If the allocation does not come from this heap FreeMemory will fail and/or warn first
	//		depending on heap settings. FreeMemory will free the memory that matches the flag of the allocation. Text is
	//		limited to 32 chars including terminator.  Strings longer than this will only store the last 31 chars.
	//		In certain situations FreeMemory may free memory from a heap which has been allocated from with in other heaps. In this rare situation
	//		memory corruption may occur.
	//  See Also:
	//		AllocateMemory
	//  Arguments:
	//      pMemory - Valid memory pointer. A null pointer may only be passed in if bAllowNullFree is set in the heap CreateHeap details.
	//		uFlag - One of JRSMEMORYFLAG_xxx or user defined value.  Default JRSMEMORYFLAG_NONE.  Must match the flag set at allocation time.
	//		pName - NULL terminating text string to associate with the allocation. May be NULL.
	//		uExternalId - An Id that to associate with the allocation.  Default 0.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Frees memory from within a heap.
	void cHeap::FreeMemory(void *pMemory, jrs_u32 uFlag, const jrs_i8 *pName, const jrs_u32 uExternalId)
	{
#ifndef MEMORYMANAGER_MINIMAL
		if(!cMemoryManager::Get().IsInitialized())
		{
			HeapWarning(cMemoryManager::Get().IsInitialized(), JRSMEMORYERROR_NOTINITIALIZED, "Elephant is not initialized.");
			return;
		}

		// Check if the memory address comes from a pool.  If so passing it to the heap could cause problems.
		if(IsAllocatedFromAttachedPool(pMemory))
		{
			HeapWarning(!IsAllocatedFromAttachedPool(pMemory), JRSMEMORYERROR_MEMORYADDRESSFROMPOOL, "Memory address 0x%p being freed is from a pool. Call cPoolX::FreeMemory to release this.", pMemory);
			return;
		}
#endif
		// Cannot free if the heap is locked.
		if(IsLocked())
		{
			HeapWarning(!IsLocked(), JRSMEMORYERROR_LOCKED, "Heap is locked. You may not free memory.");
			return;
		}

		// Do null check
		if(!pMemory)
		{
			if(!m_bAllowNullFree)
			{
				HeapWarning(pMemory, JRSMEMORYERROR_NULLPTR, "Cannot free null memory pointer. Set bAllowNullFree when creating the heap.");
			}

			return;
		}


#ifndef MEMORYMANAGER_MINIMAL
		// Check the allocation comes from this heap
		if(!IsAllocatedFromThisHeap(pMemory))
		{
			const jrs_i8 *pName = "Unknown";
			cHeap *pHeapF = cMemoryManager::Get().FindHeapFromMemoryAddress(pMemory);
			if(pHeapF)
				pName = pHeapF->GetName();

			HeapWarning(IsAllocatedFromThisHeap(pMemory), JRSMEMORYERROR_WRONGHEAP, "Memory allocation did not come from this heap %s.  Comes from %s.", GetName(), pName);
			return;
		}


#endif

		// Lock the heap to prevent modification
		HEAP_THREADLOCK

#ifndef MEMORYMANAGER_MINIMAL
		// Check that the memory being freed is infact an allocated block of memory.  
		// We can do several things.  First check if the size is 16 bytes or less
		// then check if the block plus the size is > the end (or if wrapped around the start)
		// check if the pPrev and pNext are also in range (or null and match the start or end)

		// Check if it has been freed already or not
		sFreeBlock *pFreeBlock = (sFreeBlock *)((jrs_i8*)pMemory - sizeof(sAllocatedBlock));
		if(pFreeBlock->uMarker == MemoryManager_FreeBlockValue || pFreeBlock->uMarker == MemoryManager_FreeBlockEndValue)
		{
			HeapWarning(pFreeBlock->uMarker != MemoryManager_FreeBlockValue && pFreeBlock->uMarker != MemoryManager_FreeBlockEndValue, JRSMEMORYERROR_ALREADYFREED, "Memory has at 0x%p already been freed.", pMemory);
			HEAP_THREADUNLOCK
			return;
		}// Ensure it is a free block

		sAllocatedBlock *pChBlock = (sAllocatedBlock *)((jrs_i8*)pMemory - sizeof(sAllocatedBlock));
		jrs_i8 *pChNext = (jrs_i8 *)pChBlock + HEAP_FULLSIZE(pChBlock->uSize);
		if(HEAP_FULLSIZE(pChBlock->uSize) > m_uHeapSize)
		{
			HeapWarning(HEAP_FULLSIZE(pChBlock->uSize) < m_uHeapSize, JRSMEMORYERROR_INVALIDADDRESS, "Memory address (0x%p) appears to not be a valid allocated address.  2 possible reasons, memory is either corrupted or the wrong pointer has been passed in.", pMemory);
			HEAP_THREADUNLOCK
			return;
		}

		jrs_bool pa = !((pChBlock->pNext == NULL && m_pMainFreeBlock->pPrevAlloc == pChBlock->pNext) || IsAllocatedFromThisHeap(pChNext));
		jrs_bool pb = !((pChBlock->pPrev == NULL && m_pAllocList == pChBlock) || IsAllocatedFromThisHeap(pChBlock->pPrev));
		if(pa || pb)
		{
			HeapWarning(!pa && !pb, JRSMEMORYERROR_INVALIDADDRESS, "Memory address appears to not be a valid allocated address.  2 possible reasons, memory is either corrupted or the wrong pointer has been passed in.");
			HEAP_THREADUNLOCK
			return;
		}

#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
		// Name and callstack versions store the heap id it was allocated from.  This can trap heap within a heap errors.  MUST BE PERFORMED THREAD SAFE.
		sAllocatedBlock *pABlock = (sAllocatedBlock *)((jrs_i8*)pMemory - sizeof(sAllocatedBlock));
		if(m_uHeapId != pABlock->uHeapId)
		{
			HeapWarning(m_uHeapId == pABlock->uHeapId, JRSMEMORYERROR_WRONGHEAP, "It appears that the allocation you are trying to free was not allocated by the heap that is trying to free it.");
			HEAP_THREADUNLOCK
			return;
		}
#endif
		// Debug checks
		if(m_uDebugFlags & (m_uDebugFlag_TrapFreeNumber | m_uDebugFlag_TrapFreeAddress))
		{
			if((m_uDebugFlags & m_uDebugFlag_TrapFreeNumber) && m_uDebugTrapOnFreeNum == m_uUniqueFreeCount)
			{
				MemoryWarning(0, JRSMEMORYERROR_DEBUGALLOCATEDADDTRAP, "Debug trap hit");
			}

			if((m_uDebugFlags & m_uDebugFlag_TrapFreeAddress) && m_pDebugTrapOnFreeAddress == pMemory)
			{
				MemoryWarning(0, JRSMEMORYERROR_DEBUGFREEDADDTRAP, "Debug trap hit");
			}
		}

		// Check if we have a cap on reverse freeing only.
		if(m_bReverseFreeOnly)
		{
			if(m_pMainFreeBlock->pPrevAlloc != (sAllocatedBlock *)pMemory)
			{
				HeapWarning(m_pMainFreeBlock->pPrevAlloc == (sAllocatedBlock *)pMemory, JRSMEMORYERROR_NOTLASTALLOC, "Allocation being freed is not the last one allocated.");
				HEAP_THREADUNLOCK
					return;
			}
		}


		// Get the block pointer
		sAllocatedBlock *pBlock = (sAllocatedBlock *)((jrs_i8*)pMemory - sizeof(sAllocatedBlock));

		// Check if the flag matches
		HeapWarning((uFlag & 0xf) == (pBlock->uFlagAndUniqueAllocNumber & 0xf), JRSMEMORYERROR_INVALIDFLAG, "Flag type doesnt match for allocation at 0x%p", pMemory);

		// Check the block for memory overwrites
#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
		CheckAllocatedBlockSentinels(pBlock);
#endif		

		// If we are using the enhanced debugging thread we do some other processing here. 
		if(cMemoryManager::Get().m_bEnhancedDebugging && m_bEnableEnhancedDebug)
		{
			// If it is post initialized but this hasn't been called yet free as per normal or we will end up getting blocked
			if(!cMemoryManager::Get().m_bEnableEnhancedDebuggingPostInit)
			{
				// Enhanced debugging places this into a separate list.  This is then removed later by Elephant from a different thread.

				// Clear the memory to a known here.  Always 0xee.  Anyone using this should receive errors.  Note on some platforms this 
				// will still be a valid address.
				memset(pMemory, MemoryManager_EDebugClearValue, HEAP_FULLSIZE(pBlock->uSize));

				// Just to be double sure we change the flag of the memory address. We can potentially still free this memory
				// again for a little while and it wont get caught.  Changing the flag to a known will prevent that.
				pBlock->uFlagAndUniqueAllocNumber = (pBlock->uFlagAndUniqueAllocNumber & ~0xf) | JRSMEMORYFLAG_EDEBUG;

				// Increment pending and unlock the thread. We unlock just incase the buffer ends up being full and we wait for the other thread 
				// to finish what it is doing.  Otherwise if we add this to the list before we can end up with a deadlock.
				m_uEDebugPending++;
				HEAP_THREADUNLOCK

					// Next add it to the list
					cMemoryManager::Get().AddEDebugAllocation(pMemory);

				return;
			}			
		}
#endif

		// Main internal free.
		InternalFreeMemory(pMemory, uFlag, pName, uExternalId);

		// Unlock
		HEAP_THREADUNLOCK
	}

	//  Description:
	//		Determines which Bin to locate the memory from.  Private.
	//  See Also:
	//		
	//  Arguments:
	//      uSize - Size of memory requested.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Finds the bin size related to the allocation size wanted.
	jrs_sizet cHeap::GetBinLookupBasedOnSize(jrs_sizet uSize) const
	{
		// Set the bins up
		jrs_sizet uBinSelect = uSize - sizeof(sAllocatedBlock);
		MemoryWarning(uBinSelect >= 16, JRSMEMORYERROR_UNKNOWNCORRUPTION, "Size must be atleast 16 bytes.  Check input size is not decrementing the sizeof the allocation block.");

		// The first bins go in to exact 16 size buckets up to 128 bytes then in to larger buckets that can be split later.
		// Go into 16 byte buckets for maximum fitting.
		if(uBinSelect <= 512)
		{
			uBinSelect = (uBinSelect >> 4) - 1;
		}
		else
		{
#ifdef JRS64BIT
			jrs_sizet uMaxSize = 0xfffffffful;
			if(uBinSelect >= uMaxSize)
				uBinSelect = m_uBinCount - 1;
			else
#endif
			uBinSelect = (JRSCountLeadingZero((jrs_u32)uBinSelect) - 9) + 31; // 9 is 1 << 9 = 512 and we start on bin 32 currently. We say 31 so 1024 will go into block 32 - TODO make dynamic
			if(uBinSelect >= m_uBinCount)
				uBinSelect = m_uBinCount - 1;
		}

		return uBinSelect;
	}

	//  Description:
	//		Removes the bin allocation links. Private.
	//  See Also:
	//		
	//  Arguments:
	//      pFreeBlock - Free block to remove from the free list.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Removes free block from the bin.
	void cHeap::RemoveBinAllocation(sFreeBlock *pFreeBlock)
	{
		// Get the bin size
		jrs_sizet uBinSelect = GetBinLookupBasedOnSize(pFreeBlock->uSize);

		// We need to totally remove the bins from this if they point to each other making a circular dependency. 
		if(pFreeBlock->pNextBin == pFreeBlock && pFreeBlock->pNextBin == pFreeBlock->pPrevBin)
		{
			m_pBins[uBinSelect] = 0;
		}
		else
		{
			// Close the link list
			sFreeBlock *pPB = pFreeBlock->pPrevBin;
			sFreeBlock *pNB = pFreeBlock->pNextBin;

			pNB->pPrevBin = pPB;
			pPB->pNextBin = pNB;

			// The bin may point to this now redundant block.  This moves it on to the next one.
			if(m_pBins[uBinSelect] == pFreeBlock)
				m_pBins[uBinSelect] = pPB;
		}
	}

	//  Description:
	//		Creates a bin allocation.  Private.
	//  See Also:
	//		
	//  Arguments:
	//		uSize - Size of the allocation added.
	//      pNewFreeBlock - Free block to be adding to the list.
	//		pFBPrevBin - Previous bin.
	//		pFBNextBin - Next bin.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Adds a bin to the list.
	void cHeap::CreateBinAllocation(jrs_sizet uSize, sFreeBlock *pNewFreeBlock /*ie the block we will be creating*/, sFreeBlock **pFBPrevBin, sFreeBlock **pFBNextBin)
	{
		jrs_sizet uBin = GetBinLookupBasedOnSize(uSize);

		if(m_pBins[uBin])
		{
			// Put between this one and the next.
			*pFBPrevBin = m_pBins[uBin];
			*pFBNextBin = m_pBins[uBin]->pNextBin;
		}
		else
		{
			// A bin will always loop to itself if needed.  This makes hunting for free blocks simple and cheap.
			*pFBPrevBin = pNewFreeBlock;
			*pFBNextBin = pNewFreeBlock;
			m_pBins[uBin] = pNewFreeBlock;
		}
	}

#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
	//  Description:
	//		Sets the sentinel values on a free block.  Private.
	//  See Also:
	//		
	//  Arguments:
	//		pBlock - Size of the allocation added.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Sets the free block Sentinels.
	void cHeap::SetSentinelsFreeBlock(sFreeBlock *pBlock)
	{
		pBlock->SentinelEnd[0] = pBlock->SentinelEnd[1] = pBlock->SentinelEnd[2] = pBlock->SentinelEnd[3] = MemoryManager_SentinelValueFreeBlock;
		pBlock->SentinelStart[0] = pBlock->SentinelStart[1] = pBlock->SentinelStart[2] = pBlock->SentinelStart[3] = MemoryManager_SentinelValueFreeBlock;
	}

	//  Description:
	//		Checks the sentinel values on a free block.  Private.
	//  See Also:
	//		
	//  Arguments:
	//		pBlock - Size of the allocation added.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Checks the free block sentinels.
	void cHeap::CheckFreeBlockSentinels(sFreeBlock *pBlock)
	{
		// Checking can take time.  Disable it per heap.
		if(!m_bEnableSentinelChecking)
			return;

		if(pBlock != m_pMainFreeBlock)
		{
			jrs_u32 *pSentinels = (jrs_u32 *)((jrs_i8 *)pBlock + pBlock->uSize);
			if((sAllocatedBlock *)pSentinels == pBlock->pNextAlloc)
			{
				HeapWarning(pSentinels[0] == MemoryManager_SentinelValueAllocatedBlock, JRSMEMORYERROR_SENTINELFREECORRUPT, "Freeblock Sentinel has been corrupted in freeblock 0x%p at location 0x%p", pBlock, &pSentinels[0]);
				HeapWarning(pSentinels[1] == MemoryManager_SentinelValueAllocatedBlock, JRSMEMORYERROR_SENTINELFREECORRUPT, "Freeblock Sentinel has been corrupted in freeblock 0x%p at location 0x%p", pBlock, &pSentinels[1]);
				HeapWarning(pSentinels[2] == MemoryManager_SentinelValueAllocatedBlock, JRSMEMORYERROR_SENTINELFREECORRUPT, "Freeblock Sentinel has been corrupted in freeblock 0x%p at location 0x%p", pBlock, &pSentinels[2]);
				HeapWarning(pSentinels[3] == MemoryManager_SentinelValueAllocatedBlock, JRSMEMORYERROR_SENTINELFREECORRUPT, "Freeblock Sentinel has been corrupted in freeblock 0x%p at location 0x%p", pBlock, &pSentinels[3]);
			}
			else
			{
				HeapWarning(pSentinels[0] == MemoryManager_SentinelValueFreeBlock, JRSMEMORYERROR_SENTINELFREECORRUPT, "Freeblock Sentinel has been corrupted in freeblock 0x%p at location 0x%p", pBlock, &pSentinels[0]);
				HeapWarning(pSentinels[1] == MemoryManager_SentinelValueFreeBlock, JRSMEMORYERROR_SENTINELFREECORRUPT, "Freeblock Sentinel has been corrupted in freeblock 0x%p at location 0x%p", pBlock, &pSentinels[1]);
				HeapWarning(pSentinels[2] == MemoryManager_SentinelValueFreeBlock, JRSMEMORYERROR_SENTINELFREECORRUPT, "Freeblock Sentinel has been corrupted in freeblock 0x%p at location 0x%p", pBlock, &pSentinels[2]);
				HeapWarning(pSentinels[3] == MemoryManager_SentinelValueFreeBlock, JRSMEMORYERROR_SENTINELFREECORRUPT, "Freeblock Sentinel has been corrupted in freeblock 0x%p at location 0x%p", pBlock, &pSentinels[3]);
			}
		}

		HeapWarning(pBlock->SentinelStart[0] == MemoryManager_SentinelValueFreeBlock, JRSMEMORYERROR_SENTINELFREECORRUPT, "Freeblock Start Sentinel has been corrupted in freeblock 0x%p at location 0x%p", pBlock, &pBlock->SentinelStart[0]);
		HeapWarning(pBlock->SentinelStart[1] == MemoryManager_SentinelValueFreeBlock, JRSMEMORYERROR_SENTINELFREECORRUPT, "Freeblock Start Sentinel has been corrupted in freeblock 0x%p at location 0x%p", pBlock, &pBlock->SentinelStart[1]);
		HeapWarning(pBlock->SentinelStart[2] == MemoryManager_SentinelValueFreeBlock, JRSMEMORYERROR_SENTINELFREECORRUPT, "Freeblock Start Sentinel has been corrupted in freeblock 0x%p at location 0x%p", pBlock, &pBlock->SentinelStart[2]);
		HeapWarning(pBlock->SentinelStart[3] == MemoryManager_SentinelValueFreeBlock, JRSMEMORYERROR_SENTINELFREECORRUPT, "Freeblock Start Sentinel has been corrupted in freeblock 0x%p at location 0x%p", pBlock, &pBlock->SentinelStart[3]);
	}

	//  Description:
	//		Sets the sentinel values on an allocated block.  Private.
	//  See Also:
	//		
	//  Arguments:
	//		pBlock - Size of the allocation added.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Sets the allocated block sentinels.
	void cHeap::SetSentinelsAllocatedBlock(sAllocatedBlock *pBlock)
	{
		pBlock->SentinelEnd[0] = pBlock->SentinelEnd[1] = pBlock->SentinelEnd[2] = pBlock->SentinelEnd[3] = MemoryManager_SentinelValueAllocatedBlock;
		pBlock->SentinelStart[0] = pBlock->SentinelStart[1] = pBlock->SentinelStart[2] = pBlock->SentinelStart[3] = MemoryManager_SentinelValueAllocatedBlock;
	}

	//  Description:
	//		Checks the sentinel values on an allocated block.  Private.
	//  See Also:
	//		
	//  Arguments:
	//		pBlock - Size of the allocation added.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Checks the allocated block sentinels.
	void cHeap::CheckAllocatedBlockSentinels(sAllocatedBlock *pBlock)
	{
		// Checking can take time.  Disable it per heap.
		if(!m_bEnableSentinelChecking)
			return;

		sAllocatedBlock *pActualNext = (sAllocatedBlock *)((jrs_i8 *)pBlock + HEAP_FULLSIZE(pBlock->uSize) + sizeof(sAllocatedBlock));
		jrs_sizet uSizeBetween = (jrs_sizet)((jrs_i8 *)pBlock->pNext - (jrs_i8 *)pActualNext);

		if(uSizeBetween >= sizeof(sAllocatedBlock) + m_uMinAllocSize)
		{
			// We have a free block
			jrs_u32 *pSentinels = (jrs_u32 *)pActualNext;
			HeapWarning(pSentinels[0] == MemoryManager_SentinelValueFreeBlock, JRSMEMORYERROR_SENTINELALLOCCORRUPT, "Memory Allocation Sentinel has been been corrupted.  Looks like allocation at 0x%p has overrun at location 0x%p.", (jrs_sizet)((jrs_i8 *)pBlock + sizeof(sAllocatedBlock)), &pSentinels[0]);
			HeapWarning(pSentinels[1] == MemoryManager_SentinelValueFreeBlock, JRSMEMORYERROR_SENTINELALLOCCORRUPT, "Memory Allocation Sentinel has been been corrupted.  Looks like allocation at 0x%p has overrun at location 0x%p.", (jrs_sizet)((jrs_i8 *)pBlock + sizeof(sAllocatedBlock)), &pSentinels[1]);
			HeapWarning(pSentinels[2] == MemoryManager_SentinelValueFreeBlock, JRSMEMORYERROR_SENTINELALLOCCORRUPT, "Memory Allocation Sentinel has been been corrupted.  Looks like allocation at 0x%p has overrun at location 0x%p.", (jrs_sizet)((jrs_i8 *)pBlock + sizeof(sAllocatedBlock)), &pSentinels[2]);
			HeapWarning(pSentinels[3] == MemoryManager_SentinelValueFreeBlock, JRSMEMORYERROR_SENTINELALLOCCORRUPT, "Memory Allocation Sentinel has been been corrupted.  Looks like allocation at 0x%p has overrun at location 0x%p.", (jrs_sizet)((jrs_i8 *)pBlock + sizeof(sAllocatedBlock)), &pSentinels[3]);
		}	
		else
		{
			// The next block is an allocated one.  We can potentially deduce different types of errors from this.
			jrs_u32 *pSentinels = (jrs_u32 *)pBlock->pNext;
			HeapWarning(pSentinels[0] == MemoryManager_SentinelValueAllocatedBlock, JRSMEMORYERROR_SENTINELALLOCCORRUPT, "Memory Allocation Sentinel has been been corrupted.  Looks like allocation at 0x%p has overrun at location 0x%p.", (jrs_sizet)((jrs_i8 *)pBlock + sizeof(sAllocatedBlock)), &pSentinels[0]);
			HeapWarning(pSentinels[1] == MemoryManager_SentinelValueAllocatedBlock, JRSMEMORYERROR_SENTINELALLOCCORRUPT, "Memory Allocation Sentinel has been been corrupted.  Looks like allocation at 0x%p has overrun at location 0x%p.", (jrs_sizet)((jrs_i8 *)pBlock + sizeof(sAllocatedBlock)), &pSentinels[1]);
			HeapWarning(pSentinels[2] == MemoryManager_SentinelValueAllocatedBlock, JRSMEMORYERROR_SENTINELALLOCCORRUPT, "Memory Allocation Sentinel has been been corrupted.  Looks like allocation at 0x%p has overrun at location 0x%p.", (jrs_sizet)((jrs_i8 *)pBlock + sizeof(sAllocatedBlock)), &pSentinels[2]);
			HeapWarning(pSentinels[3] == MemoryManager_SentinelValueAllocatedBlock, JRSMEMORYERROR_SENTINELALLOCCORRUPT, "Memory Allocation Sentinel has been been corrupted.  Looks like allocation at 0x%p has overrun at location 0x%p.", (jrs_sizet)((jrs_i8 *)pBlock + sizeof(sAllocatedBlock)), &pSentinels[3]);
		}

		HeapWarning(pBlock->SentinelStart[0] == MemoryManager_SentinelValueAllocatedBlock, JRSMEMORYERROR_SENTINELALLOCCORRUPT, "Memory Allocation Sentinel has been been corrupted.  Looks like previous allocation at 0x%p may have overrun. Use more exhaustive error checking to track the overrun.", pBlock->pPrev);
		HeapWarning(pBlock->SentinelStart[1] == MemoryManager_SentinelValueAllocatedBlock, JRSMEMORYERROR_SENTINELALLOCCORRUPT, "Memory Allocation Sentinel has been been corrupted.  Looks like previous allocation at 0x%p may have overrun. Use more exhaustive error checking to track the overrun.", pBlock->pPrev);
		HeapWarning(pBlock->SentinelStart[2] == MemoryManager_SentinelValueAllocatedBlock, JRSMEMORYERROR_SENTINELALLOCCORRUPT, "Memory Allocation Sentinel has been been corrupted.  Looks like previous allocation at 0x%p may have overrun. Use more exhaustive error checking to track the overrun.", pBlock->pPrev);
		HeapWarning(pBlock->SentinelStart[3] == MemoryManager_SentinelValueAllocatedBlock, JRSMEMORYERROR_SENTINELALLOCCORRUPT, "Memory Allocation Sentinel has been been corrupted.  Looks like previous allocation at 0x%p may have overrun. Use more exhaustive error checking to track the overrun.", pBlock->pPrev);
	}
#endif

	//  Description:
	//		Resizes the heap.  It is up to the user to ensure the size being enlarged is valid other wise memory overruns could occur.  It is recommended to avoid
	//		problems to use cMemoryManager::ResizeHeap instead as this performs more safety checks.
	//  See Also:
	//		cMemoryManager::ResizeHeap, CreateHeap, DestroyHeap
	//  Arguments:
	//		uSize - Size to resize the heap too.  16byte multiples.  
	//  Return Value:
	//      TRUE if successful.
	//		FALSE for failure.
	//  Summary:
	//      Resizes the heap.
	jrs_bool cHeap::Resize(jrs_sizet uSize)
	{	
		// Resize not available
		if(cMemoryManager::Get().m_bResizeable)
		{
			HeapWarning(!cMemoryManager::Get().m_bResizeable, JRSMEMORYERROR_RESIZEFAIL, "Cannot resize the heap in resizable mode.");
			return FALSE;
		}

		HEAP_THREADLOCK

			jrs_u64 CurSize = (jrs_u64)((jrs_i8 *)m_pMainFreeBlock - m_pHeapStartAddress);
		jrs_u64 MinSize = sizeof(sFreeBlock) + sizeof(sAllocatedBlock) + m_uMinAllocSize;

		// Cannot pass when the last allocated block (the rest may be fragmented) is larger than the size requested.
		if(CurSize > uSize)
		{
			HeapWarning(0, JRSMEMORYERROR_RESIZEFAIL, "Cannot resize the heap.  Current heap size is %d bytes ", CurSize);
			HEAP_THREADUNLOCK
				return false;
		}

		// Do some checks.  Returns true because its correctly passed.
		if(uSize == m_uHeapSize)
		{
			HEAP_THREADUNLOCK
				return true;
		}

		// Cannot size smaller than the minimum size.
		if(MinSize > uSize)
		{
			HeapWarning(uSize >= MinSize, JRSMEMORYERROR_RESIZEFAIL, "Cannot resize the heap to %d bytes.  Minimum size to resize too is %d bytes.", uSize, MinSize);
			HEAP_THREADUNLOCK
				return false;
		}

		// Resizing the heap.  No need to take the freeblock into account. Could cause problems for the user managed heaps if we did.
		m_uHeapSize = uSize;
		m_pHeapEndAddress = m_pHeapStartAddress + m_uHeapSize - sizeof(sFreeBlock);

		// NOTE:  It is up to the user to ensure the size being enlarged is valid other wise memory overruns could occur.

		// Redo the main freeblock size.  The block hasnt moved so there is no need to change the pointers only the size.
		m_pMainFreeBlock->uSize = (jrs_sizet)((jrs_i8 *)m_pHeapEndAddress - ((jrs_i8 *)m_pMainFreeBlock));

		cMemoryManager::Get().ContinuousLogging_Operation(cMemoryManager::eContLog_ResizeHeap, this, NULL, 0);
		HEAP_THREADUNLOCK
			return true;
	}

	//  Description:
	//		Resizes a heap reclaiming memory and may cause interleaving of memory where memory is not contiguous.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      None
	//  Summary:
	//      Function to resize a heap dynamically giving memory back to the OS.
	void cHeap::Reclaim()
	{
		// Only reclaim when the heap allows it.
		if(!(cMemoryManager::Get().m_bResizeable && m_bHeapIsMemoryManagerManaged))
			return;			

		HEAP_THREADLOCK

		// We always search for the free links first of all.  These are the easiest to remove.
		FreeAllEmptyLinkBlocks();

		// Find the largest free space
		sFreeBlock *pFreeBlockToStartFrom = NULL;
		sFreeBlock *pLoopMarker = NULL;
		
		// Try and free m_uResizableSizeMin or a chunk (they may be bigger due to other reasons.
		// Find the largest free gap
		while(1)
		{
			sFreeBlock *pBlock = GetResizeLargestFragment(m_uResizableSizeMin, &pFreeBlockToStartFrom, &pLoopMarker);
			if(!pBlock)
			{
				HEAP_THREADUNLOCK
				return;
			}

			// Not null, see if we can free it
			jrs_i8 *pStartAddress = (jrs_i8 *)pBlock;
			jrs_i8 *pEndAddress = pStartAddress + pBlock->uSize;

			// Special case when the end is the last one.
			if(pBlock == m_pMainFreeBlock)
			{
				// It isn't quite the end
				pEndAddress += sizeof(sFreeBlock);

				// Counter an additional problem if the free block is all at the very start (i.e the whole heap is empty)
				if(m_pHeapStartAddress == (jrs_i8 *)m_pMainFreeBlock)
				{
					// Moving it on will resize to the minimum
					pStartAddress += sizeof(sFreeBlock);
				}
			}

			// Special case when the end is a linked block
			jrs_i8 *pBlockNextIsLink = (jrs_i8 *)pBlock + pBlock->uSize;
			sLinkedBlock *pBlockNextLink = NULL;
			if(pBlockNextIsLink == (jrs_i8 *)pBlock->pNextAlloc && pBlock->pNextAlloc && (pBlock->pNextAlloc->uFlagAndUniqueAllocNumber & 0xf) == JRSMEMORYFLAG_HARDLINK)
			{
				pEndAddress += sizeof(sLinkedBlock);
				pBlockNextLink = (sLinkedBlock *)pEndAddress;
			}

			// Check if any can be freed
			jrs_i8 *pStartOfFreeAddress = NULL;
			jrs_i8 *pEndOfFreeAddress = NULL;
			jrs_bool bCanBeFreed = CanReclaimBetweenMemoryAddresses(pStartAddress, pEndAddress, &pStartOfFreeAddress, &pEndOfFreeAddress);
			if(bCanBeFreed)
			{
				HeapWarning(pStartOfFreeAddress && pStartOfFreeAddress >= pStartAddress, JRSMEMORYERROR_INVALIDRECLAIMPTR, "Start pointer is invalid for reclaiming.");
				HeapWarning(pEndOfFreeAddress && pEndOfFreeAddress <= pEndAddress, JRSMEMORYERROR_INVALIDRECLAIMPTR, "End pointer is invalid for reclaiming.");

				// Free
				// 1. Resize pBlock to pStartOfFreeAddress only if it isnt the start of the heap
				// 2. Insert free block if there is a gap between pEndOfFreeAddress and pEndAddress.
				// 2a. If so insert link block
				// 2b. Else see if main free block needs to move.

				// 1.  Resize.
				sAllocatedBlock *pNewLink = NULL;
				sAllocatedBlock *pPrevA = pBlock->pPrevAlloc;
				sAllocatedBlock *pNextA = pBlock->pNextAlloc;
				jrs_bool bNeedsEndCheck = true;
				if(pStartOfFreeAddress == m_pHeapStartAddress)
				{
					RemoveBinAllocation((sFreeBlock *)pBlock);
					m_pHeapStartAddress = pEndOfFreeAddress;
					bNeedsEndCheck = false;
					ResizeSafeInsertFreeBlock(NULL, pNextA, (jrs_u8 *)pEndOfFreeAddress);
				}
				else
				{
					if(pBlock == m_pMainFreeBlock)
					{
						// Do not remove this block.  It will potentially get resized but nothing more.
					}			
					else
					{
						// Check if it is a linked block
						if(pPrevA && (pPrevA->uFlagAndUniqueAllocNumber & 0xf) == JRSMEMORYFLAG_HARDLINK)
						{
							RemoveBinAllocation((sFreeBlock *)pBlock);						

							// No end address check is needed, we just expand the link size
							bNeedsEndCheck = false;

							// Make the link larger
							pPrevA->uSize = (jrs_sizet)((jrs_u8 *)pEndOfFreeAddress - ((jrs_u8 *)pPrevA + sizeof(sAllocatedBlock)));
							ResizeSafeInsertFreeBlock(pPrevA, pNextA, (jrs_u8 *)pEndOfFreeAddress);
						}						
						else
						{
							// What we must check is that we allow enough room for the linked block to be inserted.  If not we cannot reclaim this area.
							jrs_u8 *pPotentialLinkPtrAdd = (jrs_u8 *)(pStartOfFreeAddress - sizeof(sLinkedBlock));
							if((jrs_u8 *)pStartAddress > pPotentialLinkPtrAdd)
							{
								continue;
							}

							// Resize freeblock
							RemoveBinAllocation((sFreeBlock *)pBlock);
							pNewLink = (sAllocatedBlock *)(pStartOfFreeAddress - sizeof(sLinkedBlock));
							ResizeSafeInsertFreeBlock(pPrevA, pNewLink, (jrs_u8 *)pBlock);
						}
					}				
				}

				// 2. Check if the block is the last block or not
				if(bNeedsEndCheck)
				{
					if(pEndOfFreeAddress == m_pHeapEndAddress + sizeof(sFreeBlock))
					{
						// No free block
						HeapWarning(m_pMainFreeBlock == pBlock, JRSMEMORYERROR_INVALIDRECLAIMPTR, "End pointer is not the last block");

						// We check if the address we are removing can still fit
						jrs_i8 *pNewMainEndAddress = pStartAddress + sizeof(sFreeBlock);
						if(pNewMainEndAddress <= pStartOfFreeAddress)
						{
							m_pMainFreeBlock->uSize = (jrs_sizet)(pStartOfFreeAddress - pNewMainEndAddress);
							m_pHeapEndAddress = pStartOfFreeAddress - sizeof(sFreeBlock);
						}
					}
					else if(pBlockNextLink)
					{
						// Block has been resized correctly.  We need to resize the link.
						HeapWarning((pNextA->uFlagAndUniqueAllocNumber & 0xf) == JRSMEMORYFLAG_HARDLINK, JRSMEMORYERROR_INVALIDRECLAIMPTR, "Pointer is not a heap link pointer");
						sLinkedBlock *pLB = (sLinkedBlock *)pNextA;

						// Remove the link block
						if(pLB->pLinkedPrev) pLB->pLinkedPrev->pLinkedNext = pLB->pLinkedNext;
						if(pLB->pLinkedNext) pLB->pLinkedNext->pLinkedPrev = pLB->pLinkedPrev;
						if(!pLB->pLinkedPrev && !pLB->pLinkedNext)	m_pResizableLink = NULL;

						jrs_u8 *pEndOfOldLink = (jrs_u8 *)pLB + pLB->uSize + sizeof(sAllocatedBlock);
						sLinkedBlock *pNewL = ResizeInsertLink((jrs_u8 *)pStartOfFreeAddress, pEndOfOldLink, pPrevA, pLB->pNext, pLB->pLinkedPrev, pLB->pLinkedNext);

						// If there is a free block, we change the pointer back
						pEndOfOldLink = (jrs_u8 *)pLB + pLB->uSize + sizeof(sAllocatedBlock);
						jrs_u8 *pPotentialFirst = (jrs_u8 *)(pEndOfOldLink + sizeof(sAllocatedBlock) + m_uMinAllocSize);
						if(pLB->pNext && (jrs_u8 *)pLB->pNext >= pPotentialFirst)
						{
							sFreeBlock *pFBAdjust = (sFreeBlock *)pEndOfOldLink;
							HeapWarning(pFBAdjust->pPrevAlloc == (sAllocatedBlock *)pLB, JRSMEMORYERROR_INVALIDRECLAIMPTR, "Pointer is not the previous link block");
							pFBAdjust->pPrevAlloc = (sAllocatedBlock *)pNewL;
						}
					}
					else
					{
						// Do we need to insert a link at all?

						// Find the previous free link to insert.
						sLinkedBlock *pPL = NULL;
						sLinkedBlock *pNL = m_pResizableLink;
						while(pNL)
						{
							if((jrs_i8 *)pNL > pStartOfFreeAddress)
								break;

							pPL = pNL;
							pNL = pNL->pLinkedNext;
						}
							
						// Insert a link
						ResizeInsertLink((jrs_u8 *)pStartOfFreeAddress, (jrs_u8 *)pEndOfFreeAddress, pPrevA, pNextA, pPL, pNL);
					
						// Insert a free block
						// Note there should always be an allocated block next in this situation.
						HeapWarning(pNextA, JRSMEMORYERROR_NONEXTBLOCKFORRECLAIM, "There must always be a next block in this situation. Potential corruption of heap.");
						ResizeSafeInsertFreeBlock(pNewLink, pNextA, (jrs_u8 *)pEndOfFreeAddress);
					}
				}

				// Give the memory back
				ReclaimBetweenMemoryAddresses(pStartOfFreeAddress, pEndOfFreeAddress);		
			}
		}		
	}


	//  Description:
	//		Resizes a heap reclaiming memory and may cause interleaving of memory where memory is not contiguous.  Private.
	//  See Also:
	//		ResizeInternal
	//  Arguments:
	//		None
	//  Return Value:
	//      None
	//  Summary:
	//      Function to resize a heap dynamically.
	void cHeap::InternalReclaimMemory(void) 
	{
		// Currently empty for performance reasons.
	}

	sFreeBlock *cHeap::GetResizeLargestFragment(jrs_sizet uLargerThan, sFreeBlock **pFreeBlock, sFreeBlock **pLoopBlock) const
	{
		sFreeBlock *endNextPtr = (sFreeBlock *)0xffffffff;
		const jrs_u64 uSizeToFree = m_uReclaimSize;
		sFreeBlock *pNextFreeBlock = *pFreeBlock;
		jrs_sizet minBin = GetBinLookupBasedOnSize((jrs_sizet)m_uReclaimSize);


		// Terminate early if we have scanned the list
		if(*pFreeBlock == endNextPtr)
			return NULL;

		// Check for the largest free blocks.  Because we typically allocate in blocks of 
		// Get the free block as the starting size.  This is often the biggest, so optimize for that case.
		jrs_sizet MaxSize = m_pMainFreeBlock->uSize;
		if(MaxSize >= uSizeToFree && !pNextFreeBlock)
		{
			// Mark the next free block so we don't come back
			*pFreeBlock = m_pMainFreeBlock;

			// Check this block first
			return m_pMainFreeBlock;
		}
		else if(pNextFreeBlock == m_pMainFreeBlock)	
		{
			pNextFreeBlock = NULL;
			*pFreeBlock = NULL;
		}

		// Check all the other blocks.  This could be slow if there are lots of fragments.
		jrs_sizet startBin = pNextFreeBlock ? GetBinLookupBasedOnSize(pNextFreeBlock->uSize) : m_uBinCount - 1;

		// Try and free
		for(jrs_sizet i = startBin; i >= minBin; i--)
		{
			// Check if we have an allocation
			if(m_pBins[i])
			{
				if(!(*pLoopBlock))
					*pLoopBlock = m_pBins[i]->pPrevBin;
				sFreeBlock *pBin = pNextFreeBlock ? pNextFreeBlock : m_pBins[i];
				sFreeBlock *pList = pBin;
				sFreeBlock *pBN = pBin->pNextBin;

				// Check the size
				do
				{
					jrs_sizet uSize = pBin->uSize;

					// Check if the block next to it is a link block
					jrs_i8 *pNextBlock = (jrs_i8 *)pBin + pBin->uSize;
					if(pNextBlock == (jrs_i8 *)pBin->pNextAlloc && pBin->pNextAlloc && (pBin->pNextAlloc->uFlagAndUniqueAllocNumber & 0xf) == JRSMEMORYFLAG_HARDLINK)
						uSize += sizeof(sLinkedBlock);

					// Now see if we can free it
					if(uSize >= uSizeToFree)
					{						
						// We must find out IF we can reclaim memory between the two addresses. Just removing
						// will potentially remove headers we don't/cant move.			
						*pFreeBlock = pBin != *pLoopBlock ? pBin->pNextBin : NULL;					

						// Check smaller bins if needed
						if(!(*pFreeBlock))
						{
							*pLoopBlock = NULL;
							startBin = i - 1;
							while(startBin >= minBin && !m_pBins[startBin])
								startBin--;

							*pFreeBlock = m_pBins[startBin];
							if(startBin <= minBin)
								*pFreeBlock = endNextPtr;
						}

						// Test this
						return pBin;
					}

					// Move on
					pBin = pBN;
					pBN = pBN->pNextBin;
				}
				while(pBin != pList);

				// End.
				*pFreeBlock = endNextPtr;
				return NULL;
			}
		}

		// End
		*pFreeBlock = endNextPtr;
		return NULL;
	}

	//  Description:
	//		 Resizes a heap and may cause interleaving of memory where memory is not contiguous.  Private.
	//  See Also:
	//		InternalReclaimMemory
	//  Arguments:
	//		pNewSBlock - Valid free block to set the allocation too.
	//		pNewEBlock - Size in bytes of memory requested.	
	//  Return Value:
	//      Valid allocation block.  
	//		NULL otherwise.
	//  Summary:
	//      Function to resize a heap dynamically.
	jrs_bool cHeap::ResizeInternal(void *pNewSBlock, void *pNewEBlock)
	{
		jrs_u8 *pHeapSAdd = (jrs_u8 *)m_pHeapStartAddress;
		jrs_u8 *pHeapEAdd = (jrs_u8 *)m_pHeapEndAddress;

		jrs_u8 *pNHS = (jrs_u8 *)pNewSBlock;
		jrs_u8 *pNHE = (jrs_u8 *)pNewEBlock;
		jrs_sizet newSize = pNHE - pNHS;

		// Call the system op callback if one exist.
		if(m_systemOpCallback)
			m_systemOpCallback(this, pNewSBlock, newSize, FALSE);

		// Check if the new block are out of range
		jrs_bool bHeapStartAddMustChange = false;
		jrs_bool bHeapEndAddMustChange = false;
		if(pNHS < pHeapSAdd)
		{
			bHeapStartAddMustChange = true;
		}
		else if(pNHE > pHeapEAdd)
		{
			bHeapEndAddMustChange = true;
		}
		
		// Find the allocations, ptrs and everything between them.
		jrs_u8 *pNextBlockStartAddress = NULL;
		jrs_u8 *pPrevBlockEndAddress = NULL;
		sAllocatedBlock *pPrevBlockLastAlloc = NULL;
		sAllocatedBlock *pNextBlockFirstAlloc = NULL;
		sLinkedBlock *pPrevBlockEndLink = NULL;

		sLinkedBlock *pL = m_pResizableLink;
		if(pL && !bHeapStartAddMustChange)
		{		
			sLinkedBlock *pOldL = NULL;
			while(pL)
			{
				pPrevBlockEndAddress = (jrs_u8 *)pL + sizeof(sLinkedBlock);
				pNextBlockStartAddress = (jrs_u8 *)pL + sizeof(sAllocatedBlock) + HEAP_FULLSIZE(pL->uSize);
				if(pNextBlockStartAddress > pNHS)
				{
					// Find some other info out
					pPrevBlockEndLink = pL;
					pPrevBlockLastAlloc = pL->pPrev;
					pNextBlockFirstAlloc = pL->pNext;

					break;
				}

				pOldL = pL;
				pL = pL->pLinkedNext;
			}

			// Catch final increment size.  Should only occur when bHeapEndAddMustChange is true.
			if(!pL)
			{
				HeapWarning(bHeapEndAddMustChange, JRSMEMORYERROR_FATAL, "Link blocks appear corrupted.");
				pPrevBlockEndAddress = (jrs_u8 *)m_pHeapEndAddress + sizeof(sFreeBlock);
				pNextBlockStartAddress = NULL;
				pPrevBlockEndLink = pOldL;
				pPrevBlockLastAlloc = m_pMainFreeBlock->pPrevAlloc;
				pNextBlockFirstAlloc = NULL;
			}
		}
		else
		{
			if(bHeapStartAddMustChange)
			{
				pPrevBlockEndAddress = NULL;
				pNextBlockStartAddress = (jrs_u8 *)m_pHeapStartAddress;
				pPrevBlockEndLink = NULL;
				pPrevBlockLastAlloc = NULL;
				pNextBlockFirstAlloc = m_pAllocList;
			}
			else
			{
				// Note:  If the system allocator returns an address that has been used (as can happen if the user returns set addresses) this is the most
				// likely situation to break on.
				HeapWarning(bHeapEndAddMustChange, JRSMEMORYERROR_FATAL, "Cannot resize due to invalid values.  This can happen if the system allocator returns an address that has already been used.");
				pPrevBlockEndAddress = (jrs_u8 *)m_pHeapEndAddress + sizeof(sFreeBlock);
				pNextBlockStartAddress = NULL;
				pPrevBlockEndLink = NULL;
				pPrevBlockLastAlloc = m_pMainFreeBlock->pPrevAlloc;
				pNextBlockFirstAlloc = NULL;
			}
		}

		// We have 6 situations		
		// 1 - |-New-| Gap |-----|
		// 2 - |-----| Gap |-New-|
		// 3 - |-----||-New-||-----|		
		// 4 - |-----||-New-| Gap |-----|
		// 5 - |-----| Gap |-New-||-----|
		// 6 - |-----| Gap |-New-| Gap |-----|

		if(bHeapStartAddMustChange)
		{
			// 1 - |-New-| Gap |-----| or |-New-||-----|
			HeapWarning(!pPrevBlockEndAddress && pNextBlockStartAddress, JRSMEMORYERROR_FATAL, "Cannot resize due to invalid start pointers.");
			HeapWarning(!pPrevBlockEndLink && !pPrevBlockLastAlloc, JRSMEMORYERROR_FATAL, "Cannot be a valid link or allocation address here");

			// Do we need to create a link?
			if(pNHE < pHeapSAdd)
			{
				// Yes
				sLinkedBlock *pFirstLBlock = m_pResizableLink;
				sLinkedBlock *pNL = ResizeInsertLink(pNHE, pNextBlockStartAddress, NULL, pNextBlockFirstAlloc, pPrevBlockEndLink, NULL);
				ResizeInsertFreeBlock(NULL, (sAllocatedBlock *)pNL, pNHS);

				// See if there is a freeblock
				jrs_u8 *pPotentialFirst = (jrs_u8 *)(pNextBlockStartAddress + sizeof(sAllocatedBlock) + m_uMinAllocSize);
				if((jrs_u8 *)pNextBlockFirstAlloc >= pPotentialFirst)
				{
					RemoveBinAllocation((sFreeBlock *)pNextBlockStartAddress);
					ResizeInsertFreeBlock((sAllocatedBlock *)pNL, (sAllocatedBlock *)pNextBlockFirstAlloc, pNextBlockStartAddress);
				}

				// Link into the blocks
				HeapWarning(pNL == m_pResizableLink, JRSMEMORYERROR_FATAL, "Resizable link ptr must point to this link.");
				if(pFirstLBlock)
				{
					pFirstLBlock->pLinkedPrev = pNL;
					pNL->pLinkedNext = pFirstLBlock;
				}

				// Do we need to move the first alloc
				if(!m_pAllocList || (sAllocatedBlock *)pNL < m_pAllocList)
					m_pAllocList = (sAllocatedBlock *)pNL;
			}
			else
			{
				// No, just move the ptrs
				if(!pNextBlockFirstAlloc)
				{
					m_pHeapStartAddress = (jrs_i8 *)pNHS;
					InitializeMainFreeBlock();
				}
				else
				{
					// See if there is a freeblock
					jrs_u8 *pPotentialFirst = (jrs_u8 *)(pNextBlockStartAddress + sizeof(sAllocatedBlock) + m_uMinAllocSize);
					if((jrs_u8 *)pNextBlockFirstAlloc >= pPotentialFirst)
					{
						RemoveBinAllocation((sFreeBlock *)pNextBlockStartAddress);
					}

					// Insert a new free block
					ResizeInsertFreeBlock(NULL, (sAllocatedBlock *)pNextBlockFirstAlloc, pNHS);
				}
			}

			// Move the start
			m_pHeapStartAddress = (jrs_i8 *)pNHS;			
		}
		else if(bHeapEndAddMustChange)
		{
			// 2 - |-----| Gap |-New-| or |-----||-New-|
			HeapWarning(pPrevBlockEndAddress && !pNextBlockStartAddress, JRSMEMORYERROR_FATAL, "Cannot resize due to invalid end pointers.");

			// Do we need to create a link?
			if(pNHS > pHeapEAdd + sizeof(sFreeBlock))
			{
				// Update the main freeblock to pNHS
				jrs_u8 *pOldMainFreeBlock = (jrs_u8 *)m_pMainFreeBlock;

				m_uUniqueFreeCount++;
				m_pMainFreeBlock = (sFreeBlock *)pNHS;				
				m_pMainFreeBlock->uMarker = MemoryManager_FreeBlockEndValue;
				m_pMainFreeBlock->pNextAlloc = NULL;
				//m_pMainFreeBlock->pPrevAlloc = (sAllocatedBlock *)pNL;		Set in ResizeInsertLink.
				m_pMainFreeBlock->pNextBin = 0;			// End block is always 0 here.
				m_pMainFreeBlock->pPrevBin = 0;
				m_pMainFreeBlock->uFlags = m_uUniqueFreeCount;
				m_pMainFreeBlock->uPad2 = MemoryManager_FreeBlockPadValue;
				m_pMainFreeBlock->uSize = (jrs_sizet)((jrs_i8 *)pNHE - (jrs_i8 *)m_pMainFreeBlock) - sizeof(sFreeBlock);
#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
				SetSentinelsFreeBlock(m_pMainFreeBlock);
#endif

				HeapWarning(!pNextBlockFirstAlloc, JRSMEMORYERROR_FATAL, "Cannot be a next allocation.");
				sLinkedBlock *pNL = ResizeInsertLink(pPrevBlockEndAddress, pNHS, pPrevBlockLastAlloc, pNextBlockFirstAlloc, pPrevBlockEndLink, NULL);
				HeapWarning(m_pMainFreeBlock->pPrevAlloc == (sAllocatedBlock *)pNL, JRSMEMORYERROR_FATAL, "End free block must reference new link.");

				// Do we need to insert a free block?  Handle null spaces.
				jrs_u8 *pNewFreePos = pPrevBlockLastAlloc ? (jrs_u8 *)pPrevBlockLastAlloc + HEAP_FULLSIZE(pPrevBlockLastAlloc->uSize) + sizeof(sAllocatedBlock) : pOldMainFreeBlock;
				jrs_u8 *pPotentialFirst = (jrs_u8 *)(pNewFreePos + sizeof(sAllocatedBlock) + m_uMinAllocSize);
				if((jrs_u8 *)pNL >= pPotentialFirst)
				{					
					ResizeInsertFreeBlock(pPrevBlockLastAlloc, (sAllocatedBlock *)pNL, pNewFreePos);
				}
			}
			else
			{
				// Update the main freeblock to just resize				
				m_pMainFreeBlock->uMarker = MemoryManager_FreeBlockEndValue;
				m_pMainFreeBlock->pNextAlloc = NULL;
				m_pMainFreeBlock->pPrevAlloc = (sAllocatedBlock *)pPrevBlockLastAlloc;
				m_pMainFreeBlock->pNextBin = 0;			// End block is always 0 here.
				m_pMainFreeBlock->pPrevBin = 0;
				m_pMainFreeBlock->uFlags = m_uUniqueFreeCount;
				m_pMainFreeBlock->uPad2 = MemoryManager_FreeBlockPadValue;
				m_pMainFreeBlock->uSize = (jrs_sizet)((jrs_i8 *)pNHE - (jrs_i8 *)m_pMainFreeBlock) - sizeof(sFreeBlock);

#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
				SetSentinelsFreeBlock(m_pMainFreeBlock);
#endif
				m_uUniqueFreeCount++;			
			}

			// Set the new end ptr
			m_pHeapEndAddress = (jrs_i8 *)(pNHE - sizeof(sFreeBlock));

		}
		else if(pNHS == pPrevBlockEndAddress && pNHE == pNextBlockStartAddress)
		{
			// 3 - |-----||-New-||-----|	
			sLinkedBlock *pPrevL = pPrevBlockEndLink->pLinkedPrev;
			sLinkedBlock *pNextL = pPrevBlockEndLink->pLinkedNext;

			// No need to do much, just remove the link.  We do need to ensure the counts dont decrement though as the links are not a real allocation.
			// We could do it inside the function but it adds time for a very common call.
			m_uAllocatedSize += pPrevBlockEndLink->uSize;
			m_uAllocatedCount++;
			InternalFreeMemory((void *)((jrs_i8 *)pPrevBlockEndLink + sizeof(sAllocatedBlock)), JRSMEMORYFLAG_HARDLINK, "Link Block Consolidated", 0);

			// Deal with the removal of the link
			if(pPrevL)
				pPrevL->pLinkedNext = pNextL;
			else
				m_pResizableLink = pNextL;
			if(pNextL)
				pNextL->pLinkedPrev = pPrevL;
		}
		else if(pNHS == pPrevBlockEndAddress && pNHE < pNextBlockStartAddress)
		{
			// 4 - |-----||-New-| Gap |-----|
			// We must move the link block to the end of pNHE, we cant do that by directly removing it as it may cause problems, so we add a new link block,
			// resize the old, then remove the old.  It is more convoluted but it is safer.
			sLinkedBlock *pNL = ResizeInsertLink(pNHE, pNextBlockStartAddress, (sAllocatedBlock *)pPrevBlockEndLink, pNextBlockFirstAlloc, pPrevBlockEndLink, NULL);

			// We must resize the prev so it can be removed correctly.
			pPrevBlockEndLink->uSize = (jrs_u8 *)pNL - ((jrs_u8 *)pPrevBlockEndLink + sizeof(sAllocatedBlock));
			HeapWarning((jrs_u8 *)pPrevBlockEndLink + sizeof(sAllocatedBlock) + pPrevBlockEndLink->uSize == (jrs_u8 *)pNL, JRSMEMORYERROR_FATAL, "The prev link should now point to the next Link block.");

			// See if there is a freeblock
			jrs_u8 *pPotentialFirst = (jrs_u8 *)(pNextBlockStartAddress + sizeof(sAllocatedBlock) + m_uMinAllocSize);
			if((jrs_u8 *)pNextBlockFirstAlloc >= pPotentialFirst)
			{
				RemoveBinAllocation((sFreeBlock *)pNextBlockStartAddress);
				ResizeInsertFreeBlock((sAllocatedBlock *)pNL, (sAllocatedBlock *)pNextBlockFirstAlloc, pNextBlockStartAddress);
			}

			// All done, simply remove 
			sLinkedBlock *pPrevL = pPrevBlockEndLink->pLinkedPrev;
			sLinkedBlock *pNextL = pPrevBlockEndLink->pLinkedNext;
			HeapWarning(pNL->pLinkedPrev == pPrevBlockEndLink, JRSMEMORYERROR_FATAL, "Link blocks are out of sync.");

			// No need to do much, just remove the link.  We do need to ensure the counts dont decrement though as the links are not a real allocation.
			// We could do it inside the function but it adds time for a very common call.
			m_uAllocatedSize += pPrevBlockEndLink->uSize;
			m_uAllocatedCount++;
			InternalFreeMemory((void *)((jrs_i8 *)pPrevBlockEndLink + sizeof(sAllocatedBlock)), JRSMEMORYFLAG_HARDLINK, "Link Block Consolidated", 0);

			// Deal with the removal of the link, next doesn't need dealing with.
			pNL->pLinkedPrev = pPrevL;
			if(pPrevL)
				pPrevL->pLinkedNext = pNextL;
			else
				m_pResizableLink = pNextL;			
		}
		else if(pNHS > pPrevBlockEndAddress && pNHE == pNextBlockStartAddress)
		{
			// 5 - |-----| Gap |-New-||-----|
			// See if there is a free block AFTER the next link and remove it
			jrs_u8 *pFreeBlock = (jrs_u8 *)pPrevBlockEndLink + HEAP_FULLSIZE(pPrevBlockEndLink->uSize) + sizeof(sAllocatedBlock);
			HeapWarning(pNextBlockStartAddress == pFreeBlock, JRSMEMORYERROR_FATAL, "Situation 5 pointers must match.");

			// Change the prev link block size - we can do this simply by resizing the value
			pPrevBlockEndLink->uSize = pNHS - ((jrs_u8 *)pPrevBlockEndLink + sizeof(sAllocatedBlock));
			HeapWarning((jrs_u8 *)pPrevBlockEndLink + sizeof(sAllocatedBlock) + pPrevBlockEndLink->uSize == pNHS, JRSMEMORYERROR_FATAL, "The size to the next freeblock should be equal to the start of the next linked block.");

			jrs_u8 *pNewFreeBlock = (jrs_u8 *)pPrevBlockEndLink + HEAP_FULLSIZE(pPrevBlockEndLink->uSize) + sizeof(sAllocatedBlock);

			// See if we need to move the end block to the new free block
			if(pFreeBlock == (jrs_u8 *)m_pMainFreeBlock)
			{
				// Update the main freeblock to just resize
				m_pMainFreeBlock = (sFreeBlock *)pNHS;
				m_pMainFreeBlock->uMarker = MemoryManager_FreeBlockEndValue;
				m_pMainFreeBlock->pNextAlloc = NULL;
				m_pMainFreeBlock->pPrevAlloc = (sAllocatedBlock *)pPrevBlockEndLink;
				m_pMainFreeBlock->pNextBin = 0;			// End block is always 0 here.
				m_pMainFreeBlock->pPrevBin = 0;
				m_pMainFreeBlock->uFlags = m_uUniqueFreeCount;
				m_pMainFreeBlock->uPad2 = MemoryManager_FreeBlockPadValue;
				m_pMainFreeBlock->uSize = (jrs_sizet)(((jrs_i8 *)m_pHeapEndAddress + sizeof(sFreeBlock)) - (jrs_i8 *)m_pMainFreeBlock) - sizeof(sFreeBlock);

#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
				SetSentinelsFreeBlock(m_pMainFreeBlock);
#endif
				m_uUniqueFreeCount++;

				HeapWarning((jrs_u8 *)m_pMainFreeBlock + m_pMainFreeBlock->uSize == (jrs_u8 *)m_pHeapEndAddress, JRSMEMORYERROR_FATAL, "Main free block size doesnt match the end address.");
			}
			else
			{
				// No, that means we have an allocation and we must put a free block between the Link we have resized and the allocation.
				HeapWarning(pNextBlockFirstAlloc, JRSMEMORYERROR_FATAL, "Next pointer is invalid and resize cannot take place.");

				// Check for a previous allocation, handling the null case and remove.
				jrs_u8 *pPotentialFirst = (jrs_u8 *)(pFreeBlock + sizeof(sAllocatedBlock) + m_uMinAllocSize);
				if((jrs_u8 *)pNextBlockFirstAlloc >= pPotentialFirst)
				{
					// Remove the freeblock
					RemoveBinAllocation((sFreeBlock *)pFreeBlock);
				}

				HeapWarning(pNewFreeBlock == pNHS, JRSMEMORYERROR_FATAL, "The pointer to the next freeblock should be equal to the start of the next linked block.");
				ResizeInsertFreeBlock((sAllocatedBlock *)pPrevBlockEndLink, pNextBlockFirstAlloc, pNewFreeBlock);
			}
		}
		else
		{
			// 6 - |-----| Gap |-New-| Gap |-----|
			// This is fairly simple, we resize the prevlink to point to pNHS, insert a new link at pNHE and insert a freeblock.
			HeapWarning(pPrevBlockEndLink, JRSMEMORYERROR_FATAL, "Previous pointers in situation 6 are invalid");

			// Resize the prev
			pPrevBlockEndLink->uSize = pNHS - ((jrs_u8 *)pPrevBlockEndLink + sizeof(sAllocatedBlock));
			HeapWarning(pPrevBlockEndAddress - sizeof(sLinkedBlock) + sizeof(sAllocatedBlock) + pPrevBlockEndLink->uSize == pNHS, JRSMEMORYERROR_FATAL, "The size to the next freeblock should be equal in situation 6. to the start of the next linked block.");

			// Insert the new link
			sLinkedBlock *pNL = ResizeInsertLink(pNHE, pNextBlockStartAddress, (sAllocatedBlock *)pPrevBlockEndLink, pNextBlockFirstAlloc, pPrevBlockEndLink, NULL);
			HeapWarning(pNL->pLinkedPrev == pPrevBlockEndLink && pPrevBlockEndLink->pLinkedNext == pNL, JRSMEMORYERROR_FATAL, "New block doesnt link back to itself.");

			// See if there is a freeblock
			jrs_u8 *pPotentialFirst = (jrs_u8 *)(pNextBlockStartAddress + sizeof(sAllocatedBlock) + m_uMinAllocSize);
			if((jrs_u8 *)pNextBlockFirstAlloc >= pPotentialFirst)
			{
				RemoveBinAllocation((sFreeBlock *)pNextBlockStartAddress);
				ResizeInsertFreeBlock((sAllocatedBlock *)pNL, (sAllocatedBlock *)pNextBlockFirstAlloc, pNextBlockStartAddress);
			}

			// Insert the freeblock
			ResizeInsertFreeBlock((sAllocatedBlock *)pPrevBlockEndLink, (sAllocatedBlock *)pNL, NULL);
		}

		// Increase the size
		m_uHeapSize += newSize;

		return TRUE;
	}

	//  Description:
	//		 Inserts an sLinkedBlock into a resizable heap where contiguous memory isn't possible  Private.
	//  See Also:
	//		
	//  Arguments:
	//		pEndPtrForNewLink - End of previous block.
	//		pStartPtrOfNextBlock - Next block pointer.
	//		pPrevAlloc - Previous allocation.  May be null.
	//		pNextAlloc - Next allocation. May be null.
	//		pPrevLink - Previous link block.
	//		pNextLink - Next link block.  May be null in some circumstances.  Only used from Reclaim.
	//  Return Value:
	//      Valid link block ptr.
	//  Summary:
	//      Inserts a link block
	sLinkedBlock *cHeap::ResizeInsertLink(jrs_u8 *pEndPtrForNewLink, jrs_u8 *pStartPtrOfNextBlock, sAllocatedBlock *pPrevAlloc, sAllocatedBlock *pNextAlloc, sLinkedBlock *pPrevLink, sLinkedBlock *pNextLink)
	{
		sLinkedBlock *pNewL = (sLinkedBlock *)(pEndPtrForNewLink - sizeof(sLinkedBlock));
		pNewL->pPrev = pPrevAlloc;
		if(pPrevAlloc)		
			pPrevAlloc->pNext = (sAllocatedBlock *)pNewL;
		pNewL->pNext = pNextAlloc;
		if(pNextAlloc)
			pNextAlloc->pPrev = (sAllocatedBlock *)pNewL;
		pNewL->uFlagAndUniqueAllocNumber = (JRSMEMORYFLAG_HARDLINK & 15) | ((m_uUniqueAllocCount & 0xfffffff) << 4);
		pNewL->uSize = (jrs_sizet)(pStartPtrOfNextBlock - ((jrs_u8 *)pNewL + sizeof(sAllocatedBlock)));

		if(pNextLink)
		{
			// Reclaim only path
			sLinkedBlock *pNextPrev = pNextLink->pLinkedPrev;
			pNewL->pLinkedNext = pNextLink;
			if(pNewL->pLinkedNext)
				pNewL->pLinkedNext->pLinkedPrev = pNewL;

			pNewL->pLinkedPrev = pNextPrev;
			if(pNewL->pLinkedPrev)
				pNewL->pLinkedPrev->pLinkedNext = pNewL;
		}
		else
		{
			// Default path
			pNewL->pLinkedNext = pPrevLink ? pPrevLink->pLinkedNext : NULL;
			if(pNewL->pLinkedNext)
				pNewL->pLinkedNext->pLinkedPrev = pNewL;

			pNewL->pLinkedPrev = pPrevLink;
			if(pNewL->pLinkedPrev)
				pNewL->pLinkedPrev->pLinkedNext = pNewL;
		}

#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
		SetSentinelsAllocatedBlock((sAllocatedBlock *)pNewL);
#endif

#ifndef MEMORYMANAGER_MINIMAL
#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
		// Set the names etc and if we are on a supported platform get the stack trace
		strcpy(pNewL->Name, "Heap Linking Block");
		cMemoryManager::Get().StackTrace(pNewL->uCallsStack, m_uCallstackDepth, JRSMEMORY_CALLSTACKDEPTH);
#endif
		if(m_bEnableLogging)
		{
			cMemoryManager::Get().ContinuousLogging_HeapOperation(cMemoryManager::eContLog_Allocate, this, (sAllocatedBlock *)pNewL, 16, 0);
		}
#endif

		if(!m_pResizableLink || pNewL < m_pResizableLink)
			m_pResizableLink = pNewL;

		// Point to the end block
		if(!pNextAlloc)
		{
#ifndef MEMORYMANAGER_MINIMAL
			jrs_u8 *pNextP = (jrs_u8 *)pNewL + sizeof(sAllocatedBlock) + HEAP_FULLSIZE(pNewL->uSize);
			MemoryWarning(pNextP <= (jrs_u8 *)m_pMainFreeBlock, JRSMEMORYERROR_FATAL, "Link block to be created is too small.  Must be at least %llubytes (currently %llubytes).", (jrs_u64)m_uMinAllocSize, (jrs_u64)m_pMainFreeBlock - (jrs_u64)pEndPtrForNewLink);
			MemoryWarning(pNextP == (jrs_u8 *)m_pMainFreeBlock, JRSMEMORYERROR_FATAL, "Pointer doesnt equal end block. Pointer may be null NULL.");
#endif

			// Check if the end block is pointed to by the new link
			if(pStartPtrOfNextBlock == (jrs_u8 *)m_pMainFreeBlock)
				m_pMainFreeBlock->pPrevAlloc = (sAllocatedBlock *)pNewL;
		}

		// Increase stats
		m_uUniqueAllocCount++;

		return pNewL;
	}

	//  Description:
	//		 Inserts a free block but checks if it can be inserted first.  Private.
	//  See Also:
	//		
	//  Arguments:
	//		pLastAllocBefore - Previous allocation.  May be NULL if pFreeBlockStartAddress is valid.
	//		pNextAllocPtr - Memory address to allocate up to.
	//		pFreeBlockStartAddress - Pointer to start with. May be NULL if pLastAllocBefore is valid.
	//  Return Value:
	//      Valid allocation block.  
	//		NULL otherwise.
	//  Summary:
	//      Function to safely insert a free block for resizable heaps..
	void cHeap::ResizeSafeInsertFreeBlock(sAllocatedBlock *pLastAllocBefore, sAllocatedBlock *pNextAllocPtr, jrs_u8 *pFreeBlockStartAddress)
	{
		// See if there is room for a freeblock
		jrs_u8 *pPotentialFirst = (jrs_u8 *)(pFreeBlockStartAddress + sizeof(sAllocatedBlock) + m_uMinAllocSize);
		if((jrs_u8 *)pNextAllocPtr >= pPotentialFirst)
			ResizeInsertFreeBlock(pLastAllocBefore, pNextAllocPtr, pFreeBlockStartAddress);
	}

	//  Description:
	//		 Inserts a free block.  Private.
	//  See Also:
	//		
	//  Arguments:
	//		pLastAllocBefore - Previous allocation.  May be NULL if pFreeBlockStartAddress is valid.
	//		pNextAllocPtr - Memory address to allocate up to.
	//		pFreeBlockStartAddress - Pointer to start with. May be NULL if pLastAllocBefore is valid.
	//  Return Value:
	//      Valid allocation block.  
	//		NULL otherwise.
	//  Summary:
	//      Function to insert a free block for resizable heaps..
	void cHeap::ResizeInsertFreeBlock(sAllocatedBlock *pLastAllocBefore, sAllocatedBlock *pNextAllocPtr, jrs_u8 *pFreeBlockStartAddress)
	{
		HeapWarning(pLastAllocBefore || pFreeBlockStartAddress, JRSMEMORYERROR_FATAL, "One or the other pointer must be valid to create free");
		if(pLastAllocBefore)
		{
			pFreeBlockStartAddress = (jrs_u8 *)pLastAllocBefore + HEAP_FULLSIZE(pLastAllocBefore->uSize) + sizeof(sAllocatedBlock);
		}

		sFreeBlock *pNewBlock = (sFreeBlock *)(pFreeBlockStartAddress);
		jrs_sizet newFreeSpace = (jrs_sizet)((jrs_u8 *)pNextAllocPtr - pFreeBlockStartAddress);

		sFreeBlock *pFBPrevBin, *pFBNextBin;
		CreateBinAllocation(newFreeSpace, pNewBlock, &pFBPrevBin, &pFBNextBin);
		pNewBlock->pNextBin = pFBNextBin;
		pNewBlock->pNextAlloc = (sAllocatedBlock *)pNextAllocPtr;
		pNewBlock->uFlags = m_uUniqueFreeCount;
		pNewBlock->uPad2 = MemoryManager_FreeBlockPadValue;
		pNewBlock->pPrevBin = pFBPrevBin;
		pNewBlock->pPrevAlloc = pLastAllocBefore;
		pNewBlock->uMarker = MemoryManager_FreeBlockValue;
		pNewBlock->uSize = newFreeSpace;

		// Adjust the bin pointers but only if they don't point to the same block.  This is because we run a circular buffer of pointers
		if(pFBNextBin && pFBNextBin != pNewBlock)
		{
			pFBNextBin->pPrevBin = pNewBlock;
			pFBPrevBin->pNextBin = pNewBlock;
		}
#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
		strcpy(pNewBlock->Name, "MemMan_NewResizedBlock");
		cMemoryManager::Get().StackTrace(pNewBlock->uCallsStack, m_uCallstackDepth, JRSMEMORY_CALLSTACKDEPTH);
#endif
#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
		SetSentinelsFreeBlock(pNewBlock);
#endif
	}

	//  Description:
	//		Checks if the memory pointer was allocated from this heap by checking the heaps memory range.  May get confused if memory is located within other heaps.
	//  See Also:
	//		CreateHeap, DestroyHeap
	//  Arguments:
	//		pMemory - Valid memory pointer to a block of memory. 
	//  Return Value:
	//      TRUE if memory belongs to heap.
	//		FALSE otherwise.
	//  Summary:
	//      Checks if the memory pointer was allocated from this heap by checking the heaps memory range.
	jrs_bool cHeap::IsAllocatedFromThisHeap(void *pMemory) const
	{
		if((jrs_i8 *)pMemory >= m_pHeapStartAddress && (jrs_i8 *)pMemory < m_pHeapEndAddress)
		{
			// Locking only needed in resizable mode
			if(cMemoryManager::Get().m_bResizeable)
			{
				HEAP_THREADLOCK
			}

			// It is but we need to check if it falls outside of a re-sizable block
			if(m_pResizableLink)
			{
				sLinkedBlock *pL = m_pResizableLink;
				jrs_u8 *pS = (jrs_u8 *)m_pHeapStartAddress;
				jrs_u8 *pE = NULL;
				while(pL)
				{
					pE = (jrs_u8 *)pL + sizeof(sLinkedBlock);
					if((jrs_u8 *)pMemory >= pS && (jrs_u8 *)pMemory <= pE)
					{
						HEAP_THREADUNLOCK
						return true;
					}

					pS = (jrs_u8 *)pL + sizeof(sAllocatedBlock) + pL->uSize;
					pL = pL->pLinkedNext;
				}

				// Final check
				pE = (jrs_u8 *)m_pHeapEndAddress;
				if((jrs_u8 *)pMemory >= pS && (jrs_u8 *)pMemory <= pE)
				{
					HEAP_THREADUNLOCK
					return true;
				}

				HEAP_THREADUNLOCK
				// Not found.
				return false;
			}

			if(cMemoryManager::Get().m_bResizeable)
			{
				HEAP_THREADUNLOCK
			}

			return true;
		}

		return false;
	}

	//  Description:
	//		Returns if error checking is enabled or not for the heap.  This is determined by the bEnableErrors flag of sHeapDetails when creating the heap.
	//  See Also:
	//		AreErrorsWarningsOnly, CreateHeap
	//  Arguments:
	//		None
	//  Return Value:
	//      TRUE if errors are enabled.
	//		FALSE otherwise.
	//  Summary:
	//      Returns if error checking is enabled or not for the heap.
	jrs_bool cHeap::AreErrorsEnabled(void) const 
	{ 
		return m_bEnableErrors; 
	}				

	//  Description:
	//		Returns if all errors have been demoted to just warnings for the heap.  This is determined by the bErrorsAsWarnings flag of sHeapDetails when creating the heap.  If error checking
	//		is enabled this will be ignored internally.
	//  See Also:
	//		AreErrorsEnabled, CreateHeap
	//  Arguments:
	//		None
	//  Return Value:
	//      TRUE if errors are demoted to warnings.
	//		FALSE otherwise.
	//  Summary:
	//      Returns if all errors have been demoted to just warnings for the heap.
	jrs_bool cHeap::AreErrorsWarningsOnly(void) const 
	{ 
		return m_bErrorsAsWarnings; 
	}

	//  Description:
	//		Returns if this heap is managed by Elephant or if it is self managed.
	//  See Also:
	//		CreateHeap
	//  Arguments:
	//		None
	//  Return Value:
	//      TRUE if managed by Elephant.
	//		FALSE if managed by the user.
	//  Summary:
	//      Returns if this heap is managed by Elephant or if it is self managed.
	jrs_bool cHeap::IsMemoryManagerManaged(void) const 
	{ 
		return m_bHeapIsMemoryManagerManaged; 
	}

	//  Description:
	//		Return the maximum allocation size allowed by the heap.  This is set by the uMaxAllocationSize flag of sHeapDetails when creating the heap.
	//  See Also:
	//		GetMinAllocationSize
	//  Arguments:
	//		None
	//  Return Value:
	//      Maximum allowed allocation size.
	//  Summary:
	//      Return the maximum allocation size allowed by the heap.
	jrs_sizet cHeap::GetMaxAllocationSize(void) const 
	{ 
		return m_uMaxAllocSize; 
	}

	//  Description:
	//		Return the minimum allocation size allowed by the heap.  This is set by the uMinAllocationSize flag of sHeapDetails when creating the heap.  The heap
	//		will automatically resize the minimum size to match this which is different functionality to the maximum size which will fail.  This is because some systems
	//		may require a minimum size internally for some memory items due to page or boundary sizes.
	//  See Also:
	//		GetMaxAllocationSize
	//  Arguments:
	//		None
	//  Return Value:
	//      Minimum allocation size.
	//  Summary:
	//      Return the minimum allocation size allowed by the heap.
	jrs_sizet cHeap::GetMinAllocationSize(void) const 
	{ 
		return m_uMinAllocSize; 
	}

	//  Description:
	//		Return the default alignment allowed by the heap.  This is set by the uDefaultAlignment flag of sHeapDetails when creating the heap. 
	//  See Also:
	//		CreateHeap
	//  Arguments:
	//		None
	//  Return Value:
	//      Size of the default alignment.
	//  Summary:
	//      Return the default alignment allowed by the heap.
	jrs_u32 cHeap::GetDefaultAlignment(void) const 
	{ 
		return m_uDefaultAlignment; 
	}

	//  Description:
	//		Returns the name of the heap created when calling CreateHeap.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      NULL terminated string of the heap name.
	//  Summary:
	//      Returns the name of the heap.
	const jrs_i8 *cHeap::GetName(void) const 
	{ 
		return m_HeapName; 
	}

	//  Description:
	//		Returns the unique id of the heap created when calling CreateHeap.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      jrs_u32 id of the heap.
	//  Summary:
	//      Returns the unique id of the heap.
	jrs_u32 cHeap::GetUniqueId(void) const
	{
		return m_uHeapId;
	}

	//  Description:
	//		Returns the size in bytes of the heap.
	//  See Also:
	//		GetAddress, GetName
	//  Arguments:
	//		None
	//  Return Value:
	//      Size in bytes of the heap.
	//  Summary:
	//      Returns the size in bytes of the heap.
	jrs_sizet cHeap::GetSize(void) const 
	{ 
		return m_uHeapSize; 
	}

	//  Description:
	//		Returns the start address of the heap.  For managed heaps this will be one created by Elephant or it will be the user requested one.
	//  See Also:
	//		GetSize, GetName, GetAddressEnd
	//  Arguments:
	//		None
	//  Return Value:
	//      Memory address the heap works from.
	//  Summary:
	//      Returns the start address of the heap.
	void *cHeap::GetAddress(void) const 
	{ 
		return (void *)m_pHeapStartAddress; 
	}

	//  Description:
	//		Returns the end address of the heap.  For managed heaps this will be one created by Elephant or it will be the user requested one.
	//  See Also:
	//		GetSize, GetName, GetAddress
	//  Arguments:
	//		None
	//  Return Value:
	//      Memory address the heap works from.
	//  Summary:
	//      Returns the end address of the heap.
	void *cHeap::GetAddressEnd(void) const 
	{ 
		return (void *)m_pHeapEndAddress; 
	}

	//  Description:
	//		Returns the amount of memory used in allocations of the heap.  This does not include allocation overhead but does include free block fragmentation.  Just because
	//		a heap may have the memory free does not mean it is one contiguous block of memory so allocating this amount may fail.
	//  See Also:
	//		GetSize, GetNumberOfAllocations
	//  Arguments:
	//		None
	//  Return Value:
	//      Size of memory allocated in bytes.
	//  Summary:
	//      Returns the amount of memory used in allocations of the heap.
	jrs_sizet cHeap::GetMemoryUsed(void) const 
	{ 
		return m_uAllocatedSize; 
	}

	//  Description:
	//		Returns the total number of hard links for a resizable heap.
	//  See Also:
	//		GetNumberOfAllocations
	//  Arguments:
	//		None
	//  Return Value:
	//      Number of links.
	//  Summary:
	//      Returns the total number of hard links for a resizable heap.
	jrs_u32 cHeap::GetNumberOfLinks(void) const
	{
		jrs_u32 uNumLinks = 0;
		sLinkedBlock *pBlock = m_pResizableLink;
		while(pBlock)
		{
			uNumLinks++;
			pBlock = pBlock->pLinkedNext;
		}

		return uNumLinks;
	}

	//  Description:
	//		Returns the total number of active allocations in the heap.  Multiply this value with cMemoryManager::SizeofAllocatedBlock to get the total overhead.
	//  See Also:
	//		GetMemoryUsedMaximum
	//  Arguments:
	//  Return Value:
	//      Total number of allocations in the heap.
	//  Summary:
	//      Returns the total number of active allocations in the heap.
	jrs_u32 cHeap::GetNumberOfAllocations(void) const 
	{
		return m_uAllocatedCount; 
	}

	//  Description:
	//		Returns the maximum amount of allocated memory the heap has had to manage.  The actual value may be the same or lower but never higher than the current.  Use ResetHeapStatistics
	//		to set this to the current.
	//  See Also:
	//		GetMemoryUsed, ResetHeapStatistics
	//  Arguments:
	//		None
	//  Return Value:
	//      Total memory allocated in bytes at its peak.
	//  Summary:
	//      Returns the maximum amount of allocated memory the heap has had to manage.
	jrs_sizet cHeap::GetMemoryUsedMaximum(void) const 
	{ 
		return m_uAllocatedSizeMax; 
	}

	//  Description:
	//		Returns the maximum number of allocations the heap has dealt with.  The actual value may be the same or lower but never higher than the current.  Use ResetHeapStatistics
	//		to set this to the current.
	//  See Also:
	//		ResetHeapStatistics, GetNumberOfAllocations
	//  Arguments:
	//		None
	//  Return Value:
	//      Number of allocations managed by this heap at its peak.
	//  Summary:
	//      Returns the maximum number of allocations the heap has dealt with. 
	jrs_u32 cHeap::GetNumberOfAllocationsMaximum(void) const 
	{ 
		return m_uAllocatedCountMax; 
	}

	//  Description:
	//		Finds the largest free block of memory in the Heap.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      Size in bytes of the free memory block this Heap.
	//  Summary:
	//      Finds the largest free block of memory in the system.
	jrs_sizet cHeap::GetSizeOfLargestFragment(void) const
	{
		// Get the free block as the starting size.  This is often the biggest, so optimize for that case.
		jrs_sizet MaxSize = m_pMainFreeBlock->uSize;

		// Check all the other blocks.  This could be slow if there are lots of fragments.
		for(jrs_i32 i = m_uBinCount - 1; i >= 0; i--)
		{
			// Check if we have an allocation
			if(m_pBins[i])
			{
				// Check this bin is bigger than the actual size
				if(i < 8)
				{
					if(MaxSize > (jrs_sizet)(16 * i))
						return MaxSize - cMemoryManager::Get().SizeofAllocatedBlock();
				}
				// Early out
				else if(MaxSize > (jrs_sizet)(1 << i))
					return MaxSize - cMemoryManager::Get().SizeofAllocatedBlock();

				sFreeBlock *pBin = m_pBins[i];
				sFreeBlock *pList = pBin;

				// Check the size
				do
				{
					if(pBin->uSize > MaxSize)
						MaxSize = pBin->uSize;
					pBin = pBin->pNextBin;
				}
				while(pBin != pList);

				return MaxSize - cMemoryManager::Get().SizeofAllocatedBlock();
			}
		}

		// Largest free block size (minus overhead)
		return MaxSize ? MaxSize - cMemoryManager::Get().SizeofAllocatedBlock() : 0; 		
	}

	//  Description:
	//		Returns the total free memory.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      Size in bytes of the free memory of this heap.
	//  Summary:
	//      Returns the total free memory.
	jrs_sizet cHeap::GetTotalFreeMemory(void) const
	{
		// Calculation works as follows.
		// GetSize returns the total heap size but that includes one free block at all times so - cMemoryManager::Get().SizeofFreeBlock()
		// Total memory used does not take into account the overhead for each.  So multiply that by the size of the allocated block.
		return GetSize() - (GetMemoryUsed() + (GetNumberOfAllocations() * cMemoryManager::Get().SizeofAllocatedBlock())) - cMemoryManager::Get().SizeofFreeBlock();
	}

	//  Description:
	//		Returns if the heap is locked and thus prevented from allocating any more memory.  
	//  See Also:
	//		SetLocked
	//  Arguments:
	//		None
	//  Return Value:
	//      TRUE if locked.
	//		FALSE otherwise.
	//  Summary:
	//      Returns if the heap is locked and thus prevented from allocating any more memory.  
	jrs_bool cHeap::IsLocked(void) const 
	{ 
		return m_bLocked; 
	}

	//  Description:
	//		Returns if the heap is allowed to free NULL values passed to it.  Set bAllowNullFree of sHeapDetails when creating or use SetNullFreeEnable.
	//  See Also:
	//		SetNullFreeEnable, CreateHeap
	//  Arguments:
	//		None
	//  Return Value:
	//      TRUE if NULL free memory is enabled.
	//		FALSE otherwise.
	//  Summary:
	//      Returns if the heap is allowed to free NULL values passed to it.
	jrs_bool cHeap::IsNullFreeEnabled(void) const 
	{ 
		return m_bAllowNullFree; 
	}

	//  Description:
	//		Returns if the heap is allowed to allocate memory of 0 size.  Set bAllowNullFree of sHeapDetails when creating or use SetZeroAllocationEnable.  Standard system
	//		malloc allows for 0 size memory and sometimes their may be a requirement for this.  Note that Elephant will allocate the minimum sized memory and it must still
	//		be freed.
	//  See Also:
	//		EnableZeroAllocation, CreateHeap
	//  Arguments:
	//		None
	//  Return Value:
	//      TRUE if 0 size memory allocation is allowed.
	//		FALSE otherwise.
	//  Summary:
	//      Returns if the heap is allowed to allocate memory of 0 size.
	jrs_bool cHeap::IsZeroAllocationEnabled(void) const 
	{ 
		return m_bAllowZeroSizeAllocations; 
	}

	//  Description:
	//		Returns if the heap was created with the bAllowNotEnoughSpaceReturn set to true.  Set bAllowNotEnoughSpaceReturn of sHeapDetails when creating.  Note that Elephant will allocate the minimum sized memory and it must still
	//		be freed.
	//  See Also:
	//		CreateHeap
	//  Arguments:
	//		None
	//  Return Value:
	//      TRUE if Out of memory conditions are allowed.
	//		FALSE otherwise.
	//  Summary:
	//      Returns if the heap was created with the bAllowNotEnoughSpaceReturn set to true.
	jrs_bool cHeap::IsOutOfMemoryReturnEnabled(void) const
	{
		return m_bAllowNotEnoughSpaceReturn;
	}

	//  Description:
	//		Enables locking and unlocking of the heap to prevent or allow dynamic allocation.
	//  See Also:
	//		IsLocked
	//  Arguments:
	//		bEnableLock - TRUE to lock the heap.  FALSE otherwise.
	//  Return Value:
	//      None
	//  Summary:
	//      Enables locking and unlocking of the heap to prevent or allow dynamic allocation.
	void cHeap::EnableLock(jrs_bool bEnableLock)
	{ 
		m_bLocked = bEnableLock;
	}

	//  Description:
	//		Enables freeing of NULL pointers.  Set bAllowNullFree of sHeapDetails when creating the heap.
	//  See Also:
	//		IsNullFreeEnable
	//  Arguments:
	//		bEnableNullFree - TRUE to enable freeing of NULL memory pointers.  FALSE otherwise.
	//  Return Value:
	//      None
	//  Summary:
	//      Enables freeing of NULL pointers.
	void cHeap::EnableNullFree(jrs_bool bEnableNullFree) 
	{ 
		m_bAllowNullFree = bEnableNullFree; 
	}

	//  Description:
	//		Enables allocation of 0 size memory allocations.  Sometimes it may be needed to allocate 0 byte allocations and emulate malloc.  Set bAllowNullFree of sHeapDetails when creating the heap.
	//  See Also:
	//		CreateHeap
	//  Arguments:
	//		bEnableZeroAllocation - TRUE to enable zero size allocations.  FALSE to disable.
	//  Return Value:
	//      None
	//  Summary:
	//      Enables allocation of 0 size memory allocations.
	void cHeap::EnableZeroAllocation(jrs_bool bEnableZeroAllocation) 
	{ 
		m_bAllowZeroSizeAllocations = bEnableZeroAllocation; 
	}

	//  Description:
	//		Enables sentinel checking of the heap.  This is the quick, basic form of checking and will enable the current block of memory checked when an allocation or free
	//		is performed.  Must be using the S or NACS library.
	//  See Also:
	//		IsSentinelCheckingEnabled
	//  Arguments:
	//		bEnable - TRUE to enable quick sentinel checking.  FALSE to disable.
	//  Return Value:
	//      None
	//  Summary:
	//      Enables sentinel checking of the heap.
	void cHeap::EnableSentinelChecking(jrs_bool bEnable)	
	{ 
		m_bEnableSentinelChecking = bEnable; 
	}

	//  Description:
	//		Checks if the heap has sentinel checking enabled or not.
	//  See Also:
	//		EnableSentinelChecking
	//  Arguments:
	//		None
	//  Return Value:
	//      TRUE if sentinel checking is enabled.
	//		FALSE otherwise.
	//  Summary:
	//      Checks if the heap has sentinel checking enabled or not.
	jrs_bool cHeap::IsSentinelCheckingEnabled(void) const	
	{ 
		return m_bEnableSentinelChecking; 
	}

	//  Description:
	//		Enables full error checking of the heap every allocation or free.  This is time consuming and you must be using the S or NACS lib.
	//  See Also:
	//		IsExhaustiveSentinelCheckingEnabled
	//  Arguments:
	//		bEnable - TRUE to enable.  FALSE to disable.
	//  Return Value:
	//     None
	//  Summary:
	//      Enables full error checking of the heap every allocation or free. 
	void cHeap::EnableExhaustiveSentinelChecking(jrs_bool bEnable)	
	{ 
		m_bEnableExhaustiveErrorChecking = bEnable; 
	}

	//  Description:
	//		Checks if exhaustive sentinel checking is enabled or not.
	//  See Also:
	//		EnableExhaustiveSentinelChecking
	//  Arguments:
	//		None
	//  Return Value:
	//      TRUE if enabled.
	//		FALSE otherwise.
	//  Summary:
	//      Checks if exhaustive sentinel checking is enabled or not.
	jrs_bool cHeap::IsExhaustiveSentinelCheckingEnabled(void) const	
	{ 
		return m_bEnableExhaustiveErrorChecking; 
	}

	//  Description:
	//		Returns if continuous logging for this heap is enabled or not.  By default it is enabled.  Using this can remove results from heaps you do not
	//		want and improve performance.
	//  See Also:
	//		EnableLogging
	//  Arguments:
	//		None
	//  Return Value:
	//      TRUE if enabled.
	//		FALSE otherwise.
	//  Summary:
	//      Returns if continuous logging for this heap is enabled or not.
	jrs_bool cHeap::IsLoggingEnabled(void) const 
	{ 
		return m_bEnableLogging; 
	}

	//  Description:
	//		Enables or disables continuous logging for the heap.
	//  See Also:
	//		IsLoggingEnabled
	//  Arguments:
	//		bEnable - TRUE to enable logging or FALSE to disable.
	//  Return Value:
	//      None.
	//  Summary:
	//      Enables or disables continuous logging for the heap.
	void cHeap::EnableLogging(jrs_bool bEnable) 
	{ 
		m_bEnableLogging = bEnable; 
	}

	//  Description:
	//		Sets the callstack depth to start storing its addresses from N from the actual internal Elephant allocation. By default Elephant reports a callstack 
	//		from its main allocation function.  However sometimes the top level callstack isn't high enough to capture exactly where the call came from.  
	//		This is typical in middleware solutions where most of the callstacks will be identical internally and you cannot see where in your code the allocation originated.  
	//		The default value is 2.
	//  See Also:
	//		
	//  Arguments:
	//		uDepth - Depth of the callstack to start with.
	//  Return Value:
	//      None.
	//  Summary:
	//      Sets the callstack depth to start reporting from.
	void cHeap::SetCallstackDepth(jrs_u32 uDepth)
	{
		m_uCallstackDepth = uDepth;
	}

	//  Description:
	//		Searches the free list for an empty block to give to an allocation.  Private.
	//  See Also:
	//		
	//  Arguments:
	//		uSize - Size in bytes of memory requested. No checks are performed on validity - earlier functions guarantee this.
	//		uAlignment - Alignment of memory requested. No checks are performed on validity - earlier functions guarantee this.
	//  Return Value:
	//      Valid cFreeBlock if found, never NULL.
	//  Summary:
	//      Searches the free list for an empty block to give to an allocation.
	sFreeBlock *cHeap::SearchForFreeBlockBinFit(jrs_sizet uSize, jrs_u32 uAlignment)
	{
		jrs_sizet uBin = GetBinLookupBasedOnSize((uSize + sizeof(sAllocatedBlock)));	

		// Note we you get better memory use if a 16 byte allocation skips 32 and 48 and goes right to 64byte.  It will split that block rather
		// than waste the remainder of memory.  JRS will check this and change if better general results, result.

		// The bin search starts with a simple search to look for the bin based on the current size.  If it cant find a bin that matches that size it scans
		// based on m_uBinDepthSearch.  When that fails it allocates from the main bin.
		for(; uBin < m_uBinCount; uBin++)
		{
			// Its important to add the allocation block size here.  The look up function subtracts that off giving us the wrong bin potentially.
			sFreeBlock *pFb = m_pBins[uBin];
			if(pFb)
			{
				sFreeBlock *pStart = pFb;

				// Loop until we find the same pointer again.  Its a circular list.
				do 
				{
					jrs_u64 uFreeSize = pFb->uSize - sizeof(sAllocatedBlock);
					if(uFreeSize >= uSize)
					{
						// Get the aligned address and size
						jrs_i8 *pAlignedAddress = (jrs_i8 *)(((jrs_sizet)pFb + sizeof(sAllocatedBlock) + ((jrs_sizet)uAlignment - 1)) & ~((jrs_sizet)uAlignment - 1));
						jrs_i8 *pFBEndAddress = (jrs_i8 *)pFb + pFb->uSize;
						if((pAlignedAddress + uSize) <= pFBEndAddress)
						{
							// We have a fit!
							return pFb;
						}
					}

					// Get the next pointer.
					pFb = pFb->pNextBin;
				}while(pStart != pFb);
			}
		}

		// Just return the main free block if we reach here
		return m_pMainFreeBlock;
	}

	//  Description:
	//		Main allocation function to allocate, setup and return a new allocation.  This function is the one that does the hard work.  Private.
	//  See Also:
	//		
	//  Arguments:
	//		pFreeBlock - Valid free block to set the allocation too.
	//		uSize - Size in bytes of memory requested.
	//		uAlignment - Alignment of memory requested.
	//		uFlag - Valid JRSMEMORY_XXX flag or user up to and including 15.
	//  Return Value:
	//      Valid allocation block.  
	//		NULL otherwise.
	//  Summary:
	//      Main allocation function to allocate, setup and return a new allocation.
	sAllocatedBlock *cHeap::AllocateFromFreeBlock(sFreeBlock *pFreeBlock, jrs_sizet uSize, jrs_u32 uAlignment, jrs_u32 uFlag)
	{
		jrs_sizet uFreeBytesBetweenAligned;

		jrs_i8 *pEndMemory;
		jrs_i8 *pFBEndMemory;
		jrs_i8 *pStartMemory;
		jrs_i8 *pAlignedMemory;

#ifndef MEMORYMANAGER_MINIMAL
		// Check that the free block is valid and also checks the Sentinels
		HeapWarning(pFreeBlock->uMarker == MemoryManager_FreeBlockValue || pFreeBlock->uMarker == MemoryManager_FreeBlockEndValue, JRSMEMORYERROR_INVALIDFREEBLOCK, "Not a valid free block at 0x%p.  It has probably been corrupted.", pFreeBlock);
#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
		CheckFreeBlockSentinels(pFreeBlock);
		if(m_bEnableExhaustiveErrorChecking)
			CheckForErrors();
#endif
		// Debug checks
		if(m_uDebugFlags & (m_uDebugFlag_TrapAllocatedNumber | m_uDebugFlag_TrapAllocatedAddress))
		{
			if((m_uDebugFlags & m_uDebugFlag_TrapAllocatedNumber) && m_uDebugTrapOnAllocatedNum == m_uUniqueAllocCount)
			{
				MemoryWarning(0, JRSMEMORYERROR_DEBUGALLOCATEDNUMBERTRAP, "Allocated Number debug trap hit");
			}

			if((m_uDebugFlags & m_uDebugFlag_TrapAllocatedAddress) && m_pDebugTrapOnAllocatedAddress == pFreeBlock)
			{
				MemoryWarning(0, JRSMEMORYERROR_DEBUGALLOCATEDADDTRAP, "Allocated Address Debug trap hit");
			}
		}
#endif

		jrs_sizet uASize = HEAP_FULLSIZE(uSize);

		// Get the aligned pointers
		pStartMemory = (jrs_i8 *)pFreeBlock;
		pAlignedMemory = (jrs_i8 *)(((jrs_sizet)pStartMemory + sizeof(sAllocatedBlock) + (uAlignment - 1)) & ~((jrs_sizet)uAlignment - 1));
		pEndMemory = (jrs_i8 *)pAlignedMemory + uASize;
		pFBEndMemory = (jrs_i8 *)pFreeBlock + pFreeBlock->uSize;

		// Get the actual sizes
		uFreeBytesBetweenAligned = (jrs_sizet)(pAlignedMemory - pStartMemory) - sizeof(sAllocatedBlock);

		// Create the new block
		sAllocatedBlock *pAllocBlock = (sAllocatedBlock *)(pAlignedMemory - sizeof(sAllocatedBlock));

		// When we start filling in this block we hit a problem because the free block will start being overwritten.  We need to back the values up.
		sAllocatedBlock *pAllocPrev = pFreeBlock->pPrevAlloc;
		sAllocatedBlock *pAllocNext = pFreeBlock->pNextAlloc;
		sFreeBlock *pNextFB = 0;

		// Check if the data will fit.  The < check accounts for memory mapped systems where the end memory may have overflowed and will then be smaller than
		// the starting block.
		if(pEndMemory > pFBEndMemory || pEndMemory < pAlignedMemory)
		{
			// No, return 0. We are out of space if we hit this one.  Sometimes we want to be able to fail on a heap.
			if(m_bAllowNotEnoughSpaceReturn || m_bResizable)
				return 0;

			HeapWarning(0, JRSMEMORYERROR_OUTOFMEMORY, "Out of memory, cannot allocate %llu bytes from heap named %s.  Not enough free space", (jrs_u64)uSize, m_HeapName);
			return 0;
		}
		// If the free block is == the main free block we can speed this up
		else if(pFreeBlock == m_pMainFreeBlock)
		{
			// The free block is the same as the main free block.  We must move the main free block to the end.
			m_pMainFreeBlock = (sFreeBlock *)pEndMemory;

			// No need to remove the bins.  Since this free block will have no bins or shouldnt (The end free block never has bins).
			HeapWarning(pFreeBlock->uMarker == MemoryManager_FreeBlockEndValue, JRSMEMORYERROR_FATAL, "Not the last free block. FATAL ERROR");
			HeapWarning(!pFreeBlock->pPrevBin && !pFreeBlock->pNextBin, JRSMEMORYERROR_FATAL, "Free bin is not null.  FATAL ERROR");

			m_pMainFreeBlock->uFlags = m_uUniqueFreeCount;
			m_pMainFreeBlock->uPad2 = MemoryManager_FreeBlockPadValue;
			m_pMainFreeBlock->pNextBin = 0;
			m_pMainFreeBlock->pPrevBin = 0;
			m_pMainFreeBlock->pNextAlloc = 0;
			m_pMainFreeBlock->pPrevAlloc = pAllocBlock;
			m_pMainFreeBlock->uMarker = pFreeBlock->uMarker;
			m_pMainFreeBlock->uSize = (jrs_sizet)((jrs_i8 *)m_pHeapEndAddress - (jrs_i8 *)m_pMainFreeBlock);

			// Set the next free block to this one. Enables us to slot a free slot in in order
			pNextFB = m_pMainFreeBlock;

#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
			SetSentinelsFreeBlock(m_pMainFreeBlock);
#endif
		}
		// Yes it does.  We need to either split it or totally remove this block.  We can only split it IF there is enough room for a 
		// m_MinAllocationSize allocation + header
		else if(pEndMemory <= pFBEndMemory)
		{
			jrs_u64 BytesBetweenAllocEndAndFreeBlockEnd = (jrs_u64)(pFBEndMemory - pEndMemory);

			// Check if moving it 'up' in memory leaves enough free space for another allocation
			if(BytesBetweenAllocEndAndFreeBlockEnd >= (sizeof(sAllocatedBlock) + m_uMinAllocSize))
			{
				// Enough room to add a new free block
				sFreeBlock *pNewBlock = (sFreeBlock *)pEndMemory;

				// Remove the bins and create new ones.  Moving this memory up will adjust the current bin sizes and pointers to alternative blocks.
				RemoveBinAllocation(pFreeBlock);
				sFreeBlock *pFBPrevBin, *pFBNextBin;
				CreateBinAllocation((jrs_sizet)((jrs_i8 *)pAllocNext - (jrs_i8 *)pNewBlock), pNewBlock, &pFBPrevBin, &pFBNextBin);

				pNewBlock->uFlags = m_uUniqueFreeCount;
				pNewBlock->uPad2 = MemoryManager_FreeBlockPadValue;
				pNewBlock->pNextBin = pFBNextBin;
				pNewBlock->pPrevBin = pFBPrevBin;
				pNewBlock->pNextAlloc = pAllocNext;
				pNewBlock->pPrevAlloc = pAllocBlock;
				pNewBlock->uMarker = MemoryManager_FreeBlockValue;
				pNewBlock->uSize = (jrs_sizet)((jrs_i8 *)pAllocNext - (jrs_i8 *)pNewBlock);

#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
				// Set the names etc and if we are on a supported platform get the stack trace
				memset(pNewBlock->Name, 0, sizeof(pNewBlock->Name));
				memcpy(pNewBlock->Name, "MemMan_Filler", strlen("MemMan_Filler"));
				cMemoryManager::Get().StackTrace(pNewBlock->uCallsStack, m_uCallstackDepth, JRSMEMORY_CALLSTACKDEPTH);
#endif

				// Adjust the bin pointers but only if they don't point to the same block.  This is because we run a circular buffer of pointers
				if(pFBNextBin && pFBNextBin != pNewBlock)
				{
					pFBNextBin->pPrevBin = pNewBlock;
					pFBPrevBin->pNextBin = pNewBlock;
				}

#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
				SetSentinelsFreeBlock(pNewBlock);
#endif

				pNextFB = pNewBlock;
			}
			else
			{
				// Its to small.  Remove the free block and expand the size of the allocation
				RemoveBinAllocation(pFreeBlock);
				uASize += (jrs_sizet)BytesBetweenAllocEndAndFreeBlockEnd;

				// If we have removed the block we may still have a null next.  This may cause an issue with later so just slot it before the end block
				if(!pNextFB)
					pNextFB = m_pMainFreeBlock;

			}
		}
		else		// Free block fits EXACTLY!
		{
			cMemoryManager::DebugError(__LINE__, "Should never reach here.  FATAL ERROR.");
		}

		// Fill in the new block
		HeapWarning(uFlag <= 15, JRSMEMORYERROR_INVALIDFLAG, "The flag passed in is of a value greater than 15.  Extra information will be lost.");

		pAllocBlock->uSize = uSize;
		pAllocBlock->uFlagAndUniqueAllocNumber = (uFlag & 15) | ((m_uUniqueAllocCount & 0xfffffff) << 4);			// 36874500
		pAllocBlock->pPrev = pAllocPrev;
		pAllocBlock->pNext = pAllocNext;
		m_uUniqueAllocCount++;

		// The previous block must have the next updated to the new address
		if(pAllocBlock->pPrev)
		{
			pAllocBlock->pPrev->pNext = pAllocBlock;
		}

		// The same for the next block
		if(pAllocBlock->pNext)
		{
			pAllocBlock->pNext->pPrev = pAllocBlock;
		}

		// Set the Sentinels
#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
		SetSentinelsAllocatedBlock(pAllocBlock);
#endif
		// Increase the allocated amount
		m_uAllocatedSize += uSize;
		if(m_uAllocatedSize > m_uAllocatedSizeMax)
			m_uAllocatedSizeMax = m_uAllocatedSize;

		m_uAllocatedCount++;
		if(m_uAllocatedCount > m_uAllocatedCountMax)
			m_uAllocatedCountMax = m_uAllocatedCount;

		// Set the main allocated list to this one if it is null.
		if(!m_pAllocList || m_pAllocList > pAllocBlock)
		{
			m_pAllocList = pAllocBlock;

			// When this is null the main block will also need to point to this
			HeapWarning(m_pMainFreeBlock->pPrevAlloc, JRSMEMORYERROR_FATAL, "Main Heap points to 0.  There is a serious error.");
		}

		// We have to check if the free space between the allocations will fit a new free block
		// If uFreeBytesBetweenAligned is 0 then no gap needs to be filled!
		if(uFreeBytesBetweenAligned)
		{
			if(uFreeBytesBetweenAligned >= sizeof(sAllocatedBlock) + m_uMinAllocSize)
			{
				// Increment the free count
				m_uUniqueFreeCount++;

				HeapWarning(pNextFB, JRSMEMORYERROR_FATAL, "Next. FATAL ERROR");

				// Yes it will fit BUT we have to slot it in.  Easiest way is just to put it right in before the main free block.
				sFreeBlock *pNewBlock = (sFreeBlock *)(pStartMemory);

				sFreeBlock *pFBPrevBin, *pFBNextBin;
				CreateBinAllocation(uFreeBytesBetweenAligned, pNewBlock, &pFBPrevBin, &pFBNextBin);
				pNewBlock->pNextBin = pFBNextBin;
				pNewBlock->pNextAlloc = pAllocBlock;
				pNewBlock->uFlags = m_uUniqueFreeCount;
				pNewBlock->uPad2 = MemoryManager_FreeBlockPadValue;
				pNewBlock->pPrevBin = pFBPrevBin;
				pNewBlock->pPrevAlloc = pAllocBlock->pPrev;
				pNewBlock->uMarker = MemoryManager_FreeBlockValue;
				pNewBlock->uSize = uFreeBytesBetweenAligned;
#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
				strcpy(pNewBlock->Name, "MemMan_Filler_X");
				cMemoryManager::Get().StackTrace(pNewBlock->uCallsStack, m_uCallstackDepth, JRSMEMORY_CALLSTACKDEPTH);
#endif
				// Adjust the bin pointers but only if they don't point to the same block.  This is because we run a circular buffer of pointers
				if(pFBNextBin && pFBNextBin != pNewBlock)
				{
					//MemoryWarning(pFBPrevBin != pNewFreeBlock)
					pFBNextBin->pPrevBin = pNewBlock;
					pFBPrevBin->pNextBin = pNewBlock;
				}

#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
				SetSentinelsFreeBlock(pNewBlock);
#endif
			}
			else
			{
				// Not enough room to squeeze one in.  2 situations can occur here.  1 is when there is an allocated block before it which we just expand.
				// We do not have to touch any free bins because we are adjusting an allocation.
				if(pAllocBlock->pPrev)
				{
#ifndef MEMORYMANAGER_MINIMAL
					// In this case and we are running the enhanced debugging thread we need to ensure that we fill this 'extra' data.  Otherwise we hit a problem - 
					// it will determine that there is junk and fail the test.  We only need to do this if the flag is equal to JRSMEMORYFLAG_EDEBUG. 
					if((pAllocBlock->pPrev->uFlagAndUniqueAllocNumber & 0xf) == JRSMEMORYFLAG_EDEBUG)
					{
						jrs_i8 *pFill = (jrs_i8 *)pAllocBlock->pPrev + HEAP_FULLSIZE(pAllocBlock->pPrev->uSize) + sizeof(sAllocatedBlock);
						memset(pFill, MemoryManager_EDebugClearValue, uFreeBytesBetweenAligned);
					}
#endif
				}
				// or 2 is when its right at the start.
				else
				{
					// At which point we do nothing.
				}
			}
		}

		// Return
		return pAllocBlock;
	}

	//  Description:
	//		Initializes the floating free block.  Private.
	//  See Also:
	//		
	//  Arguments:
	//		Nothing.
	//  Return Value:
	//      None
	//  Summary:
	//      Initializes the floating free block.
	void cHeap::DestroyLinkedMemory(void)
	{
		// Lock the whole manager for this part.  
		cMemoryManager::Get().m_MMThreadLock.Lock();

		// First free any linked memory
		sLinkedBlock *pBlock = m_pResizableLink;
		jrs_u8 *pSE = (jrs_u8 *)m_pHeapStartAddress;
		jrs_u8 *pEE = NULL;
		jrs_u8 *pNSE = NULL;
		do
		{
			// Due to the way some OS's allocation memory we have to store the links in a list.
			// We go through that list to see if it will be freeable.
			if(pBlock)
			{
				pEE = (jrs_u8 *)pBlock + sizeof(sLinkedBlock);
				pNSE = (jrs_u8 *)pBlock + sizeof(sAllocatedBlock) + pBlock->uSize;
				pBlock = pBlock ? pBlock->pLinkedNext : NULL;
			}
			else
			{
				pEE = (jrs_u8 *)m_pHeapEndAddress + sizeof(sLinkedBlock);
				pNSE = NULL;
			}

			// Scan the list from the back and see if we can free them.

			jrs_u32 count = cMemoryManager::Get().m_uResizableCount;
			for(jrs_u32 i = count; i > 0; i -= 2)
			{
				jrs_u64 *pRAllocs = cMemoryManager::Get().m_pResizableSystemAllocs;

				jrs_u8 *pMem = (jrs_u8 *)((jrs_sizet)pRAllocs[i - 2]);
				jrs_u64 uSize = pRAllocs[i - 1];
				if(pMem >= pSE && pMem < pEE)
				{
					m_systemFree(pMem, uSize);

					// Call the system op callback if one exist.
					if(m_systemOpCallback)
						m_systemOpCallback(this, pMem, uSize, TRUE);

					pRAllocs[i - 2] = pRAllocs[cMemoryManager::Get().m_uResizableCount - 2];
					pRAllocs[cMemoryManager::Get().m_uResizableCount - 2] = 0;

					pRAllocs[i - 1] = pRAllocs[cMemoryManager::Get().m_uResizableCount - 1];
					pRAllocs[cMemoryManager::Get().m_uResizableCount - 1] = 0;
					cMemoryManager::Get().m_uResizableCount -= 2;
				}
			}

			// Next
			pSE = pNSE;
		}while(pNSE);

		m_pResizableLink = NULL;

		// And unlock
		cMemoryManager::Get().m_MMThreadLock.Unlock();
	}

	//  Description:
	//		Initializes the floating free block.  Private.
	//  See Also:
	//		
	//  Arguments:
	//		Nothing.
	//  Return Value:
	//      None
	//  Summary:
	//      Initializes the floating free block.
	void cHeap::InitializeMainFreeBlock(void)
	{
		m_pMainFreeBlock = (sFreeBlock *)m_pHeapStartAddress;
		m_pMainFreeBlock->uMarker = MemoryManager_FreeBlockEndValue;
		m_pMainFreeBlock->uFlags = m_uUniqueFreeCount;
		m_pMainFreeBlock->uPad2 = MemoryManager_FreeBlockPadValue;
		m_pMainFreeBlock->uSize = (jrs_sizet)((jrs_i8 *)m_pHeapEndAddress - (jrs_i8 *)m_pMainFreeBlock);
		m_pMainFreeBlock->pPrevBin = m_pMainFreeBlock->pNextBin = 0;
		m_pMainFreeBlock->pPrevAlloc = m_pMainFreeBlock->pNextAlloc = 0;

		// Clear bins
		for(jrs_u32 i = 0; i < m_uBinCount; i++)
			m_pBins[i] = 0;

#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
		SetSentinelsFreeBlock(m_pMainFreeBlock);
#endif
#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
		// Set the names etc and if we are on a supported platform get the stack trace
		strcpy(m_pMainFreeBlock->Name, "Main Free");
		m_pMainFreeBlock->uExternalId = 0;
		m_pMainFreeBlock->uHeapId = m_uHeapId;
		cMemoryManager::Get().StackTrace(m_pMainFreeBlock->uCallsStack, m_uCallstackDepth, JRSMEMORY_CALLSTACKDEPTH);
#endif

		// Clear the allocated amount
		m_uAllocatedSize = 0;
		m_uAllocatedCount = 0;
	}

	//  Description:
	//		Checks all the allocations in the heap to see if there are any errors and buffer overruns.  This function can be time consuming but will do a thorough check
	//		and warn on the first error.  When an error is found there is a large chance that a buffer overrun may cause problems with the consistency of the memory further on so
	//		it is recommended you check the first error thoroughly.
	//  See Also:
	//		
	//  Arguments:
	//		None.
	//  Return Value:
	//      None
	//  Summary:
	//      Checks the entire heap for errors.
	void cHeap::CheckForErrors(void)
	{
		// Scans the whole heap for errors
#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
		HEAP_THREADLOCK

			// We turn reports off because we wont be able to traverse the memory if there is an error due to header corruption.
			m_bEnableReportsInErrors = false;
		if(m_pAllocList)
		{
			jrs_u32 AllocCount = 0;
			sAllocatedBlock *pAlloc = m_pAllocList;

			// We report the free blocks to to show fragments.  The first one may be before the first allocation, either because it was freed off first
			// or possibly due to alignment of the first block.
			sFreeBlock *pPotentialFirst = (sFreeBlock *)(m_pHeapStartAddress + sizeof(sAllocatedBlock) + m_uMinAllocSize);
			if((jrs_i8 *)m_pAllocList >= (jrs_i8 *)pPotentialFirst)
			{
				pPotentialFirst = (sFreeBlock *)m_pHeapStartAddress;
				CheckFreeBlockSentinels(pPotentialFirst);
			}

			// Now traverse all the lists
			do 
			{
				// Output some info
				CheckAllocatedBlockSentinels(pAlloc);

				// Check if there is a free block between this and the next
				if(pAlloc->pNext)
				{
					// We now calculate the size between rather than go off the next pointer
					sFreeBlock *pFreeBlock = (sFreeBlock *)((jrs_i8 *)pAlloc + HEAP_FULLSIZE(pAlloc->uSize) + sizeof(sAllocatedBlock));
					jrs_sizet uSizeBetween = (jrs_sizet)((jrs_i8 *)pAlloc->pNext - (jrs_i8 *)pFreeBlock);
					if(uSizeBetween >= sizeof(sAllocatedBlock) + m_uMinAllocSize)
					{
						CheckFreeBlockSentinels(pFreeBlock);
					}
				}

				// Next
				pAlloc = pAlloc->pNext;
				AllocCount++;
			}while(pAlloc);
		}

		HEAP_THREADUNLOCK
#else
		cMemoryManager::DebugOutput("Please enable use Sentinel Checking Enabled Library for this functionality.");
#endif
	}

	//  Description:
	//		Sets a flag to check on the heap if the unique free number matches the one passed to this function.  This functionality can be used to
	//		track when certain allocations are about to be freed in order to help debug.  Will trap in the user error callback.
	//  See Also:
	//		DebugTrapOnFreeAddress, DebugTrapOnAllocatedUniqueNumber, DebugTrapOnAllocatedAddress
	//  Arguments:
	//		uFreeNumber - Unique free number of the allocation to catch.
	//  Return Value:
	//      None
	//  Summary:
	//      Checks if a memory being freed matches the same number and calls the user callback.
	void cHeap::DebugTrapOnFreeNumber(jrs_u32 uFreeNumber)
	{
		m_uDebugFlags |= m_uDebugFlag_TrapFreeNumber;
		m_uDebugTrapOnFreeNum = uFreeNumber;
	}

	//  Description:
	//		Sets a flag to check on the heap if the address matches the allocation being freed.  This functionality can be used to
	//		track when certain allocations are about to be freed in order to help debug.  Will trap in the user error callback.
	//  See Also:
	//		DebugTrapOnFreeNumber, DebugTrapOnAllocatedUniqueNumber, DebugTrapOnAllocatedAddress
	//  Arguments:
	//		pAddress - Address memory allocation to catch.
	//  Return Value:
	//      None
	//  Summary:
	//      Sets a flag to check on the heap if the address matches the allocation being freed and calls the user callback.
	void cHeap::DebugTrapOnFreeAddress(void *pAddress)
	{
		m_uDebugFlags |= m_uDebugFlag_TrapFreeAddress;
		m_pDebugTrapOnFreeAddress = pAddress;
	}

	//  Description:
	//		Sets a flag to check on the heap if the unique allocation number matches the one passed to this function.  This functionality can be used to
	//		track when certain allocations are being allocated in order to help debug.  Will trap in the user error callback.
	//  See Also:
	//		DebugTrapOnFreeNumber, DebugTrapOnFreeAddress, DebugTrapOnAllocatedAddress
	//  Arguments:
	//		uUniqueNumber - Unique number to test allocations against.
	//  Return Value:
	//      None
	//  Summary:
	//      Checks if a memory being allocated matches the same number and calls the user callback.
	void cHeap::DebugTrapOnAllocatedUniqueNumber(jrs_u32 uUniqueNumber)
	{
		m_uDebugFlags |= m_uDebugFlag_TrapAllocatedNumber;
		m_uDebugTrapOnAllocatedNum = uUniqueNumber;
	}

	//  Description:
	//		Sets a flag to check on the heap if the allocation address matches the one passed to this function.  This functionality can be used to
	//		track when certain allocations are about to be allocated in order to help debug.  Will trap in the user error callback.
	//  See Also:
	//		DebugTrapOnFreeNumber, DebugTrapOnFreeAddress, DebugTrapOnAllocatedUniqueNumber
	//  Arguments:
	//		pAddress - Memory address to compare new allocations against.
	//  Return Value:
	//      None
	//  Summary:
	//      Checks if a memory being allocated matched pAddress and calls the user callback.
	void cHeap::DebugTrapOnAllocatedAddress(void *pAddress)
	{
		m_uDebugFlags |= m_uDebugFlag_TrapAllocatedAddress;
		m_pDebugTrapOnAllocatedAddress = pAddress;
	}


	//  Description:
	//		Does a report on the heap (statistics and allocations in memory order) to the user TTY callback.  If you want to report these to a file
	//		include full path and file name to the function.  The generated file is an Overview file for use in Goldfish.  The file is in a 
	//		compatible CSV format for loading into a spread sheet for self analysis.  When connected to a debugger TTY output may cause this
	//		function to take a lot longer than just writing to a log file can take.  Time is proportionate to the number of allocations.
	//  See Also:
	//		ReportStatistics, ReportAllocationsMemoryOrder
	//  Arguments:
	//      pLogToFile - Full path and file name null terminated string. NULL if you do not wish to generate a file.
	//		includeFreeBlocks - Reports free blocks to the output also.
	//		displayCallStack - Logs any the callstacks as well.  May be slow.  Default false.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Reports on all heaps and can generate an Overview file for Goldfish.
	void cHeap::ReportAll(const jrs_i8 *pLogToFile, jrs_bool includeFreeBlocks, jrs_bool displayCallStack)
	{
		ReportStatistics();
		ReportAllocationsMemoryOrder(pLogToFile, includeFreeBlocks, displayCallStack);
	}

	//  Description:
	//		Reports basic statistics about the heap to the user TTY callback. Use this to get quick information on 
	//		when need that is more detailed that simple heap memory real time calls.
	//  See Also:
	//		ReportAll, ReportAllocationsMemoryOrder, ReportAllocationsUniqueTotals
	//  Arguments:
	//		bAdvanced - Displays extra statistics.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Reports basic statistics about all heaps to the user TTY callback. 
	void cHeap::ReportStatistics(jrs_bool bAdvanced)
	{
		HEAP_THREADLOCK
		m_bEnableReportsInErrors = false;

		cMemoryManager::DebugOutput("---------------------------------------------------------------------------------------------");

		jrs_i8 *pName = m_HeapName;
		jrs_i8 *pStartAddress = m_pHeapStartAddress;
		jrs_i8 *pEndAddress = m_pHeapEndAddress;
		jrs_sizet uHeapSize = m_uHeapSize;

		// Report
		cMemoryManager::DebugOutput("Logging Heap: %s", pName);
		cMemoryManager::DebugOutput("Heap Start Address: 0x%p", pStartAddress);
		cMemoryManager::DebugOutput("Heap End Address: 0x%p", pEndAddress);
		cMemoryManager::DebugOutput("Heap Size: %dMB ( %dk (0x%lluk) )", uHeapSize >> 20, uHeapSize >> 10, (jrs_u64)uHeapSize >> 10);

		if(m_pAllocList)
			cMemoryManager::DebugOutput("First Allocated Block: 0x%p", m_pAllocList);

		cMemoryManager::DebugOutput("End Block: 0x%p", m_pMainFreeBlock);

		// Work out some other stats
		jrs_u32 AllocCount = 0;
		jrs_u64 TotalIncHeadersMemory = 0;
		jrs_u64 TotalUsedMemory = 0;
		jrs_u64 TotalUsedMemoryAligned = 0;
		jrs_u64 TotalFreeMemory = 0;
		jrs_u32 TotalFreeCount = 0;
		jrs_u64 LargestFreeBlock = m_pMainFreeBlock->uSize;
		jrs_u64 TotalIncHeadersLinkMemory = 0;
		jrs_u64 TotalUsedLinkMemory = 0;
		jrs_u32 TotalLinks = 0;

		jrs_u64 binCount[m_uBinCount];
		jrs_u64 binSize[m_uBinCount];

		for(jrs_u32 binClear = 0; binClear < m_uBinCount; binClear++)
		{
			binCount[binClear] = binSize[binClear] = 0;
		}

		// Loop for linked heaps
		cHeap *pLinked = this;
		while(pLinked)
		{
			sAllocatedBlock *pAlloc = pLinked->m_pAllocList;
			while(pAlloc) 
			{
				// Do some error checking
				HeapWarning(!((jrs_sizet)pAlloc & 0xf), JRSMEMORYERROR_UNKNOWNCORRUPTION, "Allocation pointer at 0x%p is not 16byte aligned.", pAlloc);
				HeapWarning(!((jrs_sizet)pAlloc->pNext & 0xf), JRSMEMORYERROR_UNKNOWNCORRUPTION, "Next allocation pointer at 0x%p from 0x%p is not 16byte aligned.", pAlloc->pPrev, pAlloc);

				// Work the sizes out for linked blocks
				if((pAlloc->uFlagAndUniqueAllocNumber & 0xf) == JRSMEMORYFLAG_HARDLINK)
				{
					TotalIncHeadersLinkMemory += HEAP_FULLSIZE(pAlloc->uSize) + sizeof(sAllocatedBlock);
					TotalUsedLinkMemory += pAlloc->uSize;
					TotalLinks++;
				}
				else
				{
					TotalIncHeadersMemory += HEAP_FULLSIZE(pAlloc->uSize) + sizeof(sAllocatedBlock);
					TotalUsedMemory += pAlloc->uSize;

					// Work out the aligned size
					TotalUsedMemoryAligned += HEAP_FULLSIZE(pAlloc->uSize);
					AllocCount++;

					// Calculate the bins
					jrs_sizet bin = GetBinLookupBasedOnSize(HEAP_FULLSIZE(pAlloc->uSize) + sizeof(sAllocatedBlock));
					binCount[bin]++;
					binSize[bin] = HEAP_FULLSIZE(pAlloc->uSize);
				}

				// Largest free
				if(pAlloc->pNext)
				{
					sFreeBlock *pFree = (sFreeBlock *)((jrs_i8 *)pAlloc + HEAP_FULLSIZE(pAlloc->uSize) + sizeof(sAllocatedBlock));
					jrs_sizet uSizeBetween = (jrs_sizet)((jrs_i8 *)pAlloc->pNext - (jrs_i8 *)pFree);
					if(uSizeBetween >= sizeof(sAllocatedBlock) + m_uMinAllocSize)
					{
						TotalFreeCount++;
						TotalFreeMemory += pFree->uSize - sizeof(sAllocatedBlock);
						if(LargestFreeBlock < pFree->uSize)
							LargestFreeBlock = pFree->uSize;
					}
				}

				// Next
				pAlloc = pAlloc->pNext;
			}

			pLinked = NULL;
		}	// End linked loop

		HeapWarning(m_uAllocatedCount == AllocCount, JRSMEMORYERROR_FATAL, "Allocated counts do not match. Detected %d but recorded %lld", AllocCount, (jrs_u64)m_uAllocatedCount);
		HeapWarning(m_uAllocatedSize == TotalUsedMemory, JRSMEMORYERROR_FATAL, "Allocated sizes do not match. Detected %lld but recorded %lld", TotalUsedMemory, (jrs_u64)m_uAllocatedSize);

		cMemoryManager::DebugOutput("Total Allocations: %d", AllocCount);
		cMemoryManager::DebugOutput("Total Memory Used: %lluk (%llu bytes)", TotalUsedMemory >> 10, TotalUsedMemory);
		cMemoryManager::DebugOutput("Total Memory Used (With Alignment/Padding): %lluk (%llu bytes)", TotalUsedMemoryAligned >> 10, TotalUsedMemoryAligned);
		cMemoryManager::DebugOutput("Total Memory Used in Headers: %dk (%d bytes)", (AllocCount * sizeof(sAllocatedBlock)) >> 10, AllocCount * sizeof(sAllocatedBlock));
		cMemoryManager::DebugOutput("Total Memory Consumed (Including Headers): %lluk (%llu bytes)", TotalIncHeadersMemory >> 10, TotalIncHeadersMemory);

		cMemoryManager::DebugOutput("Total Fragments: %d", TotalFreeCount);
		cMemoryManager::DebugOutput("Total Fragmented Memory: %lluk", TotalFreeMemory >> 10);
		cMemoryManager::DebugOutput("Largest Free Block: %lluk (%d bytes)", LargestFreeBlock >> 10, LargestFreeBlock);

		// Calculate the stats
		cMemoryManager::DebugOutput("Stats: Free %f%%", 100.0f * (1.0f - (float)TotalIncHeadersMemory / (float)(uHeapSize - sizeof(sFreeBlock))));

		// Fix the values for non resizable
		if(cMemoryManager::Get().m_bResizeable)
		{
			cMemoryManager::DebugOutput("Total Link Count Allocations: %d", TotalLinks);
			cMemoryManager::DebugOutput("Total Link Memory Skipped: %lluk (%llu bytes)", TotalUsedLinkMemory >> 10, TotalUsedLinkMemory);
		}

		cMemoryManager::DebugOutput("Allow Null Free: %s", m_bAllowNullFree ? "Yes" : "No");
		cMemoryManager::DebugOutput("Allow 0 size Alloc: %s", m_bAllowZeroSizeAllocations ? "Yes" : "No");
		cMemoryManager::DebugOutput("Allow destructions with allocations: %s", m_bAllowDestructionWithAllocations ? "Yes" : "No");
		cMemoryManager::DebugOutput("Allow Out of Memory return: %s", m_bAllowNotEnoughSpaceReturn ? "Yes" : "No");
		cMemoryManager::DebugOutput("Reverse free only: %s", m_bReverseFreeOnly ? "Yes" : "No");
		cMemoryManager::DebugOutput("Perform End allocation only: %s", m_bUseEndAllocationOnly ? "Yes" : "No");
		cMemoryManager::DebugOutput("Default alignment: %d", m_uDefaultAlignment);
		cMemoryManager::DebugOutput("Minimum allocation size: %d", m_uMinAllocSize);
		cMemoryManager::DebugOutput("Maximum allocation size: %d", m_uMaxAllocSize);
		cMemoryManager::DebugOutput("Allocation Header Size: %d", cMemoryManager::Get().SizeofAllocatedBlock());


		// Log any pools
		cPoolBase *pPool = m_pAttachedPools;
		while(pPool)
		{
			cMemoryManager::DebugOutput("Pool: %s", pPool->GetName());
			pPool = pPool->m_pNext;
		}

		// List all the bin sizes
		if(bAdvanced)
		{
			cMemoryManager::DebugOutput("Free bins:");
			for(jrs_u32 bins = 0; bins < m_uBinCount; bins++)
			{
				sFreeBlock *pBlock = m_pBins[bins];

				jrs_u64 binInd = (bins + 1) << 4 <= 512 ? (bins + 1) << 4 : 1 << bins;
				if(pBlock)
				{
					jrs_u64 uFreeSize = pBlock->uSize - sizeof(sAllocatedBlock);
					jrs_u64 uFreeCount = 1;
					sFreeBlock *pBlockTrav = pBlock->pNextBin;

					while(pBlockTrav != pBlock)
					{
						uFreeSize += pBlockTrav->uSize - sizeof(sAllocatedBlock);
						uFreeCount++;
						pBlockTrav = pBlockTrav->pNextBin;
					}

					cMemoryManager::DebugOutput("Size %llu%s - Free %lluk in %llu fragments - Allocated %lluk in %llu allocs", 
						binInd < 1024 ? binInd : binInd >> 10, binInd < 1024 ? "bytes" : "k", 
						uFreeSize >> 10, uFreeCount,
						binSize[bins] >> 10, binCount[bins]);
				}
				else
					cMemoryManager::DebugOutput("Size %llu%s - Empty", binInd < 1024 ? binInd : binInd >> 10, binInd < 1024 ? "bytes" : "k");
			}
		}

		// End
		cMemoryManager::DebugOutput("---------------------------------------------------------------------------------------------");
		m_bEnableReportsInErrors = true;
		HEAP_THREADUNLOCK
	}

	//  Description:
	//		Reports on all the allocations in the heap.  This function is like ReportAll but doesn't output the statistics.  Calling
	//		this function with a valid file name will create an Overview file for use in Goldfish.
	//  See Also:
	//		ReportAll, ReportStatistics
	//  Arguments:
	//		pLogToFile - Full path and file name null terminated string. NULL if you do not wish to generate a file.
	//		includeFreeBlocks - Logs any free block to the output as well.  Default false.
	//		displayCallStack - Logs any the callstacks as well.  May be slow.  Default false.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Reports all allocations from all heaps to the user TTY callback and/or a file.
	void cHeap::ReportAllocationsMemoryOrder(const jrs_i8 *pLogToFile, jrs_bool includeFreeBlocks, jrs_bool displayCallStack)
	{
		HEAP_THREADLOCK
		m_bEnableReportsInErrors = false;

		cMemoryManager::DebugOutput("---------------------------------------------------------------------------------------------");

		// Write the throwaway heap header
		// CSV Heap marker block = Marker, HeapName, Size, Memory/User managed, Default Alignment, Min AllocSize, Max Alloc Size, Start Address, Base address, 1 or 0 (1 base address passed in, 0 for PlatformInitFunction), sizeof allocated block, bits, start memory, end memory, number of callstacks
		// CSV Main block		 = Marker (Allocation/Free), HeapName, Address, Size, CallStack4, CS3, CS3, CS2, CS1, Text (if one), (line if one)
#ifdef JRS64BIT
		const jrs_u32 uBits = 64;
#else
		const jrs_u32 uBits = 32;
#endif

		cMemoryManager::DebugOutputFile(pLogToFile, g_ReportHeapCreate, 
			"_HeapHeadMarker_; %s; %llu; %u; %u; %llu; %llu; %llu; %llu; %u; %llu; %u; %llu; %llu; %u", 
			m_HeapName, (jrs_u64)GetSize(), !m_bHeapIsMemoryManagerManaged, 
			m_uDefaultAlignment, (jrs_u64)m_uMinAllocSize, 
			(jrs_u64)m_uMaxAllocSize, (jrs_u64)m_pHeapStartAddress, 
			g_uBaseAddressOffsetCalculation, 
#ifdef JRSMEMORYMICROSOFTPLATFORMS
			1,
#else
			0,
#endif
			(jrs_u64)cMemoryManager::Get().SizeofAllocatedBlock(), uBits,
			(jrs_u64)((jrs_sizet)cMemoryManager::Get().m_pUseableMemoryStart), (jrs_u64)((jrs_sizet)cMemoryManager::Get().m_pUseableMemoryEnd), JRSMEMORY_CALLSTACKDEPTH);	

		if(g_ReportHeap && g_ReportHeapCreate)
			g_ReportHeapCreate = false;


#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
		cMemoryManager::DebugOutput("Allocation Number (UniqueId             Text                  ExternalId) - Address             (HeaderAddr) Size ");
#else
		cMemoryManager::DebugOutput("Allocation Number (UniqueId) - Address            (HeaderAddr) Size ");
#endif
		jrs_u32 AllocCount = 0;
		jrs_u32 FreeCount = 0;
		cHeap *pLinked = this;
		jrs_i8 logtext[8192];
		while(pLinked)
		{
			// Log all the allocations in order
			if(pLinked->m_pAllocList)
			{
				sAllocatedBlock *pAlloc = pLinked->m_pAllocList;

				// We report the free blocks to to show fragments.  The first one may be before the first allocation, either because it was freed off first
				// or possibly due to alignment of the first block.
				sFreeBlock *pPotentialFirst = (sFreeBlock *)(m_pHeapStartAddress + sizeof(sAllocatedBlock) + m_uMinAllocSize);
				if((jrs_i8 *)pLinked->m_pAllocList >= (jrs_i8 *)pPotentialFirst)
				{
					pPotentialFirst = (sFreeBlock *)m_pHeapStartAddress;
					if(includeFreeBlocks)
					{
#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
						sprintf(logtext, "Free Block %6d (%8d %-32s %5d) - 0x%p (0x%p) %llu", FreeCount++, 0, pPotentialFirst->Name, pPotentialFirst->uExternalId, (jrs_i8 *)pPotentialFirst + sizeof(sAllocatedBlock), pPotentialFirst, (jrs_u64)(pPotentialFirst->uSize - sizeof(sAllocatedBlock)));
						if(displayCallStack)
							cMemoryManager::StackToString(&logtext[strlen(logtext)], pPotentialFirst->uCallsStack, JRSMEMORY_CALLSTACKDEPTH);
						cMemoryManager::DebugOutput(logtext);
#else
					cMemoryManager::DebugOutput("Free Block %6d (%8d) - 0x%p (0x%p) %u", FreeCount++, 0, (jrs_i8 *)pPotentialFirst + sizeof(sAllocatedBlock), pPotentialFirst, pPotentialFirst->uSize - sizeof(sAllocatedBlock));
#endif
					}
					sprintf(logtext, "_Free_; %s; %llu; %llu; %u; %llu; ",						
						m_HeapName, (jrs_u64)((jrs_i8 *)pPotentialFirst + sizeof(sAllocatedBlock)), (jrs_u64)pPotentialFirst->uSize - sizeof(sAllocatedBlock), 
						0, (jrs_u64)pPotentialFirst->uFlags);

#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
					strcat(logtext, pPotentialFirst->Name);
#else
					strcat(logtext, "Unknown");
#endif
					for(jrs_u32 cs = 0; cs < JRSMEMORY_CALLSTACKDEPTH; cs++)
					{
#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
						jrs_i8 temp[32];
						sprintf(temp, "; 0x%llx", (jrs_u64)MemoryManagerPlatformAddressToBaseAddress(pPotentialFirst->uCallsStack[cs]));
						strcat(logtext, temp);
#else
						strcat(logtext, "; 0x0");
#endif
					}

					cMemoryManager::DebugOutputFile(pLogToFile, false, logtext);
				}

				// Now traverse all the lists
				do 
				{
					// Output some info
					jrs_i8 *pMemoryLocation = (jrs_i8 *)pAlloc + sizeof(sAllocatedBlock);
					bool bLinkType = (pAlloc->uFlagAndUniqueAllocNumber & 0xf) == JRSMEMORYFLAG_HARDLINK;
#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
					sprintf(logtext, "%s %6d (%8d %-32s %5d) - 0x%p (0x%p) %llu", bLinkType ? "Link      " : "Allocation", AllocCount, (jrs_u32)pAlloc->uFlagAndUniqueAllocNumber >> 4, pAlloc->Name, pAlloc->uExternalId, pMemoryLocation, pAlloc, (jrs_u64)(HEAP_FULLSIZE(pAlloc->uSize)));
					if (displayCallStack)
						cMemoryManager::StackToString(&logtext[strlen(logtext)], pPotentialFirst->uCallsStack, JRSMEMORY_CALLSTACKDEPTH);
					cMemoryManager::DebugOutput(logtext);
#else
					cMemoryManager::DebugOutput("%s %6d (%8d) - 0x%p (0x%p) %u ", bLinkType ? "Link      " : "Allocation", AllocCount, pAlloc->uFlagAndUniqueAllocNumber >> 4, pMemoryLocation, pAlloc, HEAP_FULLSIZE(pAlloc->uSize));
#endif
					sprintf(logtext, "_Alloc_; %s; %llu; %llu; %llu; %llu; ",
						m_HeapName, (jrs_u64)pMemoryLocation, (jrs_u64)HEAP_FULLSIZE(pAlloc->uSize), 
						(jrs_u64)(pAlloc->uFlagAndUniqueAllocNumber & 0xf), (jrs_u64)(pAlloc->uFlagAndUniqueAllocNumber >> 4));
					// Write the callstacks
#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
					strcat(logtext, pAlloc->Name);
#else
					strcat(logtext, "Unknown");
#endif
					for(jrs_u32 cs = 0; cs < JRSMEMORY_CALLSTACKDEPTH; cs++)
					{
#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
						jrs_i8 temp[32];
						sprintf(temp, "; 0x%llx", (jrs_u64)MemoryManagerPlatformAddressToBaseAddress(pAlloc->uCallsStack[cs]));
						strcat(logtext, temp);
#else
						strcat(logtext, "; 0x0");
#endif
					}

					cMemoryManager::DebugOutputFile(pLogToFile, false, logtext);

					// Check if there is a free block between this and the next
					if(pAlloc->pNext)
					{
						//jrs_sizet uSizeBetween = (jrs_sizet)(((jrs_i8 *)pAlloc->pNext - (jrs_i8 *)pAlloc)) - sizeof(sAllocatedBlock) - HEAP_FULLSIZE(pAlloc->uSize);
						sAllocatedBlock *pActualNext = (sAllocatedBlock *)((jrs_i8 *)pAlloc + HEAP_FULLSIZE(pAlloc->uSize) + sizeof(sAllocatedBlock));
						jrs_sizet uSizeBetween = (jrs_sizet)((jrs_i8 *)pAlloc->pNext - (jrs_i8 *)pActualNext);

						if(uSizeBetween >= sizeof(sAllocatedBlock) + m_uMinAllocSize)
						{
							// We have a free block.
							sFreeBlock *pFreeBlock = (sFreeBlock *)pActualNext;
							if(includeFreeBlocks)
							{
#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
								sprintf(logtext, "Free Block %6d (%8d %-32s %5d) - 0x%p (0x%p) %llu", FreeCount++, (jrs_u32)pFreeBlock->uFlags, pFreeBlock->Name, pFreeBlock->uExternalId, (jrs_i8 *)pFreeBlock + sizeof(sAllocatedBlock), pFreeBlock, (jrs_u64)(pFreeBlock->uSize - sizeof(sAllocatedBlock)));
								if (displayCallStack)
									cMemoryManager::StackToString(&logtext[strlen(logtext)], pPotentialFirst->uCallsStack, JRSMEMORY_CALLSTACKDEPTH);
								cMemoryManager::DebugOutput(logtext);
#else
								cMemoryManager::DebugOutput("Free Block %6d (%8d) - 0x%p (0x%p) %u", FreeCount++, pFreeBlock->uFlags, (jrs_i8 *)pFreeBlock + sizeof(sAllocatedBlock), pFreeBlock, pFreeBlock->uSize - sizeof(sAllocatedBlock));
#endif
							}

							sprintf(logtext, "_Free_; %s; %llu; %llu; %u; %llu; ",						
								m_HeapName, (jrs_u64)((jrs_i8 *)pFreeBlock + sizeof(sAllocatedBlock)), (jrs_u64)pFreeBlock->uSize - sizeof(sAllocatedBlock), 
								0, (jrs_u64)pFreeBlock->uFlags);

#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
							strcat(logtext, pFreeBlock->Name);
#else
							strcat(logtext, "Unknown");
#endif
							for(jrs_u32 cs = 0; cs < JRSMEMORY_CALLSTACKDEPTH; cs++)
							{
#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
								jrs_i8 temp[32];
								sprintf(temp, "; 0x%llx", (jrs_u64)MemoryManagerPlatformAddressToBaseAddress(pFreeBlock->uCallsStack[cs]));
								strcat(logtext, temp);
#else
								strcat(logtext, "; 0x0");
#endif
							}

							cMemoryManager::DebugOutputFile(pLogToFile, false, logtext);
						}
					}

					// Next
					pAlloc = pAlloc->pNext;
					AllocCount++;
				}while(pAlloc);

			}
			else
			{
				cMemoryManager::DebugOutput("No Allocations");
			}

			// next heap
			pLinked = NULL;
		}

		// Log the pools
		cPoolBase *pPool = m_pAttachedPools;
		while(pPool)
		{
			// Report it
			pPool->ReportAllocationsMemoryOrder(pLogToFile);

			// Get the next pool
			pPool = pPool->m_pNext;
		}

		cMemoryManager::DebugOutput("---------------------------------------------------------------------------------------------");
		m_bEnableReportsInErrors = true;
		HEAP_THREADUNLOCK
	}

	//  Description:
	//		Resets the heap statistics to the current values. 
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Resets the heap statistics to the current values.
	void cHeap::ResetStatistics(void)
	{
		m_uAllocatedSizeMax = m_uAllocatedSize;
		m_uAllocatedCountMax = m_uAllocatedCount;
	}

	//  Description:
	//		Returns the minimum size that the heap resizes when a resize action occurs.  Resizable mode only.
	//  See Also:
	//		
	//  Arguments:
	//		
	//  Return Value:
	//      Size the heap resizes when a resize action occurs
	//  Summary:	
	//		Returns the minimum size that the heap resizes when a resize action occurs.  Resizable mode only.
	jrs_sizet cHeap::GetResizableSize(void) const
	{
		return m_uResizableSizeMin;
	}

	void cHeap::FreeAllEmptyLinkBlocks(void)
	{
		sLinkedBlock *pLink = m_pResizableLink;
		if(pLink)
		{
			// Scan 
			while(pLink)
			{
				// Check if it is entirely free
				jrs_i8 *pPrevBlockAdd = (jrs_i8 *)pLink->pPrev;
				jrs_i8 *pStartOfNextLink = (jrs_i8 *)pLink + pLink->uSize + sizeof(sAllocatedBlock);
				jrs_i8 *pStartOfLinkBlock = (jrs_i8 *)pLink + sizeof(sLinkedBlock);

				sLinkedBlock *pPL = pLink->pLinkedPrev;
				sLinkedBlock *pPN = pLink->pLinkedNext;

				// Is it the first?
				if(!pPL)
				{
					// Yes - is there anything to free?
					if(m_pAllocList >= (sAllocatedBlock *)pLink)
					{
						jrs_i8 *pStartOfFreeAddress = NULL;
						jrs_i8 *pEndOfFreeAddress = NULL;
						jrs_bool bCanFree = CanReclaimBetweenMemoryAddresses(m_pHeapStartAddress, pStartOfLinkBlock, &pStartOfFreeAddress, &pEndOfFreeAddress);
						if(bCanFree && pStartOfFreeAddress == m_pHeapStartAddress && pEndOfFreeAddress == pStartOfLinkBlock)
						{
							// Yes, free this block
							// Remove the start block entirely
							RemoveBinAllocation((sFreeBlock *)m_pHeapStartAddress);

							// Remove the link							
							sAllocatedBlock *pNextA = pLink->pNext;
							if(pNextA)
								pNextA->pPrev = NULL;
							if(pPN)
								pPN->pLinkedPrev = NULL;	
							m_pResizableLink = pPN;
							m_pAllocList = pNextA;

							// Adjust the next free
							jrs_sizet uSizeBetween = (jrs_sizet)((jrs_i8 *)pStartOfNextLink - (jrs_i8 *)pNextA);
							if(uSizeBetween >= sizeof(sAllocatedBlock) + m_uMinAllocSize)
							{
								// We have a free block, clear some pointers
								sFreeBlock *pStartFree = (sFreeBlock *)pStartOfNextLink;
								pStartFree->pPrevAlloc = NULL;
							}

							// Adjust the pointers
							m_pHeapStartAddress = pStartOfNextLink;

							// Free the block
							ReclaimBetweenMemoryAddresses(pStartOfFreeAddress, pEndOfFreeAddress);
						}
					}
				}
				else if(pPL)
				{
					// We have a sandwiched block, check if there are any allocations between here and there
					if(pPL->pNext >= (sAllocatedBlock *)pLink)
					{
						jrs_i8 *pStartAdd = (jrs_i8 *)pPL + pPL->uSize + sizeof(sAllocatedBlock);

						// Completely free
						jrs_i8 *pStartOfFreeAddress = NULL;
						jrs_i8 *pEndOfFreeAddress = NULL;
						jrs_bool bCanFree = CanReclaimBetweenMemoryAddresses(pStartAdd, pStartOfLinkBlock, &pStartOfFreeAddress, &pEndOfFreeAddress);
						if(bCanFree && pStartOfFreeAddress == pStartAdd && pEndOfFreeAddress == pStartOfLinkBlock)
						{
							// Yes, free this block
							// Remove the start block entirely
							RemoveBinAllocation((sFreeBlock *)pStartAdd);

							// Remove the link			
							pPL->pLinkedNext = pPN;
							if(pPN)
							{
								pPN->pLinkedPrev = pPL;								
							}

							if(pLink->pNext)
								pLink->pNext->pPrev = (sAllocatedBlock *)pPL;		
							pPL->pNext = (sAllocatedBlock *)pLink->pNext;	
							pPL->uSize = (jrs_sizet)(pStartOfNextLink - ((jrs_sizet)pPL + sizeof(sAllocatedBlock)));

							// Adjust the next free
							if(pLink->pNext)
							{												
								jrs_sizet uSizeBetween = (jrs_sizet)((jrs_i8 *)pLink->pNext - (jrs_i8 *)pStartOfNextLink);
								if(uSizeBetween >= sizeof(sAllocatedBlock) + m_uMinAllocSize)
								{
									// We have a free block, clear some pointers
									sFreeBlock *pStartFree = (sFreeBlock *)pStartOfNextLink;
									pStartFree->pPrevAlloc = (sAllocatedBlock *)pPL;
								}
							}
							else
							{
								// Right at the end, find the previous allocation and move the end block back to there.
								sAllocatedBlock *pLastBlock = pLink->pPrev;

								sFreeBlock *pNewFreeBlock = (sFreeBlock *)((jrs_u8 *)pLastBlock + pLastBlock->uSize + sizeof(sAllocatedBlock));//must POINT to the the end of the previous allocation.
								pNewFreeBlock->uMarker = MemoryManager_FreeBlockEndValue;
								pNewFreeBlock->pNextAlloc = NULL;
								pNewFreeBlock->pPrevAlloc = (sAllocatedBlock *)pPrevBlockAdd;
								pNewFreeBlock->pNextBin = 0;			// End block is always 0 here.
								pNewFreeBlock->pPrevBin = 0;
								pNewFreeBlock->uFlags = ++m_uUniqueFreeCount;
								pNewFreeBlock->uPad2 = MemoryManager_FreeBlockPadValue;
								pNewFreeBlock->uSize = (jrs_sizet)m_pHeapEndAddress - (jrs_sizet)pNewFreeBlock;
#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
								SetSentinelsFreeBlock(pNewFreeBlock);
#endif
								// Move the main free block back but not the end of the heap
								m_pMainFreeBlock = pNewFreeBlock;

								// Adjust some pointers so the !pPN does its job
								pLink = pPL;
								pPL = pLink->pLinkedPrev;								
							}
												
							// Free the block
							ReclaimBetweenMemoryAddresses(pStartOfFreeAddress, pEndOfFreeAddress);
						}
					}
				}

				
				if(!pPN)
				{
					// We are right at the end.  Is there an allocation between here and the main free block?  Resizable link may have been removed above.
					if(pStartOfNextLink == (jrs_i8 *)m_pMainFreeBlock && m_pResizableLink)
					{
						// The end is completely empty.  Remove the link and reset the free block.  The sLinkedBlock is the same size as the freeblock so we
						// know it will fit exactly.

						// Remove link
						if(pPL)
						{
							pPL->pLinkedNext = pPN;
							m_pResizableLink = m_pResizableLink > pPL ? pPL : m_pResizableLink;
						}
						if(pPN)
						{
							pPN->pLinkedPrev = pPL;
							m_pResizableLink = m_pResizableLink > pPN ? pPN : m_pResizableLink;
						}

						// Remove completely if null
						if(!pPN && !pPL)
							m_pResizableLink = NULL;

						// Don't remove the free block, it isn't required as its the main free.
						jrs_i8 *pStartOfFreeAddress = NULL;
						jrs_i8 *pEndOfFreeAddress = NULL;
#ifndef MEMORYMANAGER_MINIMAL
						jrs_bool bCanFree = 
#endif
						CanReclaimBetweenMemoryAddresses(pStartOfNextLink, m_pHeapEndAddress + sizeof(sFreeBlock), &pStartOfFreeAddress, &pEndOfFreeAddress);
						HeapWarning(bCanFree, JRSMEMORYERROR_FATAL, "Must always be able to free this block");

						// Move the main free block back
						sAllocatedBlock *pPrevA = pLink->pPrev;
						jrs_i8 *pEnd = (jrs_i8 *)pPrevA + pPrevA->uSize + sizeof(sAllocatedBlock);
						if(pPrevA)
						{
							pPrevA->pNext = NULL;
						}

						sFreeBlock *pNewFreeBlock = (sFreeBlock *)pEnd;//must POINT to the the end of the previous allocation.

						RemoveBinAllocation(pNewFreeBlock);

						pNewFreeBlock->uMarker = MemoryManager_FreeBlockEndValue;
						pNewFreeBlock->pNextAlloc = NULL;
						pNewFreeBlock->pPrevAlloc = (sAllocatedBlock *)pPrevBlockAdd;
						pNewFreeBlock->pNextBin = 0;			// End block is always 0 here.
						pNewFreeBlock->pPrevBin = 0;
						pNewFreeBlock->uFlags = ++m_uUniqueFreeCount;
						pNewFreeBlock->uPad2 = MemoryManager_FreeBlockPadValue;
						pNewFreeBlock->uSize = sizeof(sFreeBlock);
#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
						SetSentinelsFreeBlock(pNewFreeBlock);
#endif
						// Move the main free block back
						m_pMainFreeBlock = pNewFreeBlock;
						m_pHeapEndAddress = (jrs_i8 *)pLink;

						// Free the block
						ReclaimBetweenMemoryAddresses(pStartOfFreeAddress, pEndOfFreeAddress);
					}
				}

				// The next
				pLink = pPN;
			}
		}
	}

	jrs_bool cHeap::CanReclaimBetweenMemoryAddresses(jrs_i8 *pStartAddress, jrs_i8 *pEndAddress, jrs_i8 **pStartOfFreeAddress, jrs_i8 **pEndOfFreeAddress)
	{
		// First free any linked memory
		jrs_u8 *pMemFreeFirst = NULL;
		jrs_u8 *pMemFreeLast = NULL;

		jrs_u8 *pSE = (jrs_u8 *)pStartAddress;
		jrs_u8 *pEE = (jrs_u8 *)pEndAddress;
		do
		{
			// Scan the list from the back and see if we can free them.
			jrs_u32 count = cMemoryManager::Get().m_uResizableCount;
			jrs_u32 i = count;
			for(; i > 0; i -= 2)
			{
				jrs_u64 *pRAllocs = cMemoryManager::Get().m_pResizableSystemAllocs;

				jrs_u8 *pMem = (jrs_u8 *)((jrs_sizet)pRAllocs[i - 2]);
				jrs_u64 uSize = pRAllocs[i - 1];
				jrs_u8 *pMemEnd = pMem + uSize;

				if(pMem >= pSE && pMemEnd <= pEE)
				{
					if(pMemFreeFirst > pMem || !pMemFreeFirst)
						pMemFreeFirst = pMem;
					if(pMemFreeLast < pMemEnd || !pMemFreeLast)
						pMemFreeLast = pMemEnd;
				}
			}

			// More?
			if(i == 0)
				break;			// No
		}while(1);

		*pStartOfFreeAddress = (jrs_i8 *)pMemFreeFirst;
		*pEndOfFreeAddress = (jrs_i8 *)pMemFreeLast;

		if(!pMemFreeFirst && !pMemFreeLast)
			return FALSE;

		return TRUE;
	}

	void cHeap::ReclaimBetweenMemoryAddresses(jrs_i8 *pStartOfFreeAddress, jrs_i8 *pEndOfFreeAddress)
	{
		jrs_u8 *pSE = (jrs_u8 *)pStartOfFreeAddress;
		jrs_u8 *pEE = (jrs_u8 *)pEndOfFreeAddress;
		do
		{
			// Scan the list from the back and see if we can free them.
			jrs_u32 count = cMemoryManager::Get().m_uResizableCount;
			jrs_u32 i = count;
			for(; i > 0; i -= 2)
			{
				jrs_u64 *pRAllocs = cMemoryManager::Get().m_pResizableSystemAllocs;

				jrs_u8 *pMem = (jrs_u8 *)((jrs_sizet)pRAllocs[i - 2]);
				jrs_u64 uSize = pRAllocs[i - 1];
				jrs_u8 *pMemEnd = pMem + uSize;

				if(pMem >= pSE && pMemEnd <= pEE)
				{
					m_systemFree(pMem, uSize);
					pRAllocs[i - 2] = pRAllocs[cMemoryManager::Get().m_uResizableCount - 2];
					pRAllocs[cMemoryManager::Get().m_uResizableCount - 2] = 0;

					pRAllocs[i - 1] = pRAllocs[cMemoryManager::Get().m_uResizableCount - 1];
					pRAllocs[cMemoryManager::Get().m_uResizableCount - 1] = 0;
					cMemoryManager::Get().m_uResizableCount -= 2;
			
					m_uHeapSize -= (jrs_sizet)uSize;
					break;
				}
			}

			// More?
			if(i == 0)
				break;			// No
		}while(1);
	}

}
