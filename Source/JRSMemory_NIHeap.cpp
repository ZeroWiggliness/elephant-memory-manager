// Needed for string functions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <JRSMemory.h>
#include "JRSMemory_Internal.h"
#include "JRSMemory_ErrorCodes.h"

// Defines to force inlining of some components
#define HEAP_THREADLOCK if(m_bThreadSafe) { m_pThreadLock->Lock(); }
#define HEAP_THREADUNLOCK if(m_bThreadSafe) { m_pThreadLock->Unlock(); }

// Elephant Namespace
namespace Elephant
{
	extern jrs_u64 g_uBaseAddressOffsetCalculation;

	// Report heap enabled.
	extern jrs_bool g_ReportHeap;

	// Report heap create.
	extern jrs_bool g_ReportHeapCreate;

	//  Description:
	//		cHeapNonIntrusive constructor.  Private and should not be called.  Use CreateHeap to create a heap.
	//  See Also:
	//		CreateHeap
	//  Arguments:
	//		None.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		cHeapNonIntrusive constructor.
	cHeapNonIntrusive::cHeapNonIntrusive()
	{
		// Not callable
	}

	//  Description:
	//		cHeapNonIntrusive constructor.  Should not be called by the user.  Use CreateHeapNonIntrusive to create a heap.
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
	//		cHeapNonIntrusive constructor.  Use CreateHeapNonIntrusive.
	cHeapNonIntrusive::cHeapNonIntrusive(void *pMemoryAddress, jrs_sizet uSize, cHeap *pHeap, const jrs_i8 *pName, sHeapDetails *pHeapDetails)
	{		
		HeapWarning(pHeap, JRSMEMORYERROR_HEAPINVALID, "Must have a valid standard heap.");

		m_pStandardHeap = pHeap;
		m_uNumSlabs = 0;
		m_uPageSize = 8192;
		m_bSelfManaged = false;
		m_bLocked = false;

		m_uAllocatedSize = 0;					
		m_uAllocatedCount = 0;					
		m_uAllocatedSizeMax = 0;				
		m_uAllocatedCountMax = 0;
		m_uSize = 0;

		m_bEnableErrors = pHeapDetails->bEnableErrors;
		m_bErrorsAsWarnings = pHeapDetails->bErrorsAsWarnings;
		m_bEnableReportsInErrors = TRUE;
		m_bEnableLogging = pHeapDetails->bEnableLogging;
		m_bEnableMemoryTracking = pHeapDetails->bEnableMemoryTracking;
		m_uNumCallStacks = pHeapDetails->uNumCallStacks;
		if(m_uNumCallStacks < 8)
			m_uNumCallStacks = 8;
		m_uCallstackDepth = 1; 
		m_bResizable = pHeapDetails->bResizable;
		m_uResizableSize = pHeapDetails->uResizableSize;

		// Thread safety must be enabled for some modes
		m_bThreadSafe = pHeapDetails->bThreadSafe;
		if(cMemoryManager::Get().m_bEnableLiveView || cMemoryManager::Get().m_bEnhancedDebugging)
			m_bThreadSafe = true;

#ifndef MEMORYMANAGER_MINIMAL
		m_uDebugHeaderSize = 0;
		if(m_bEnableMemoryTracking)
			m_uDebugHeaderSize = MemoryManager_StringLength + (sizeof(void *) * m_uNumCallStacks);
#endif
		// Only resizable 
		if(m_bResizable)
		{
			HeapWarning(m_uResizableSize >= m_uPageSize, JRSMEMORYERROR_INITIALIZEINVALIDSIZE, "Heap resizable size must be larger of equal to page size %dbytes", m_uPageSize);
			HeapWarning(!pMemoryAddress, JRSMEMORYERROR_INVALIDADDRESS, "Heap initialize address for resziable must be null");
			m_uResizableSize = pHeapDetails->uResizableSize;
		}

		m_pHeapStartAddress = pMemoryAddress;
		m_pHeapEndAddress = (void *)((jrs_i8 *)pMemoryAddress + uSize);

		m_bAllowNotEnoughSpaceReturn = pHeapDetails->bAllowNotEnoughSpaceReturn;
		m_bAllowNullFree = pHeapDetails->bAllowNullFree;
		m_bAllowZeroSizeAllocations = pHeapDetails->bAllowZeroSizeAllocations;
		m_bAllowDestructionWithAllocations = pHeapDetails->bAllowDestructionWithAllocations;
		m_uDefaultAlignment = pHeapDetails->uDefaultAlignment >= 64 ? pHeapDetails->uDefaultAlignment : 64;
		m_uMinAllocSize = pHeapDetails->uMinAllocationSize >= 64 ? pHeapDetails->uMinAllocationSize : 64;
		m_uMaxAllocSize = pHeapDetails->uMaxAllocationSize;
		if(m_uMaxAllocSize > 0 && m_uMaxAllocSize < pHeapDetails->uMinAllocationSize)
			m_uMaxAllocSize = pHeapDetails->uMinAllocationSize;

		// System callbacks for allocation
		m_systemAllocator = pHeapDetails->systemAllocator ? pHeapDetails->systemAllocator : cMemoryManager::Get().m_MemoryManagerDefaultAllocator;
		m_systemFree = pHeapDetails->systemAllocator ? pHeapDetails->systemFree : cMemoryManager::Get().m_MemoryManagerDefaultFree;
		m_systemPageSize = pHeapDetails->systemAllocator ? pHeapDetails->systemPageSize : cMemoryManager::Get().m_MemoryManagerDefaultSystemPageSize;
		m_systemOpCallback = pHeapDetails->systemOpCallback;

		// Clear bins
		for(jrs_u32 i = 0; i < m_uMaxNumBins; i++)
		{
			m_pBins[i] = NULL;
		}
		m_uAvailableBins = 0;

		for(jrs_u32 i = 0; i < m_uMaxNumAllocBins; i++)
		{
			m_pAllocBins[i] = NULL;
			m_pFullAllocBins[i] = NULL;
		}
	
		// Create the first slab
		if(!Expand(pMemoryAddress, uSize))
		{
			HeapWarning(0, JRSMEMORYERROR_FATAL, "Could not create slab memory. FATAL");
		}

		HeapWarning(pName, JRSMEMORYERROR_HEAPNAMEINVALID, "Heap name cannot be null.");
		HeapWarning(strlen(pName) < 32, JRSMEMORYERROR_HEAPNAMETOLARGE, "Heap Name is to large");
		strcpy(m_HeapName, pName);

		// Heap set up
		cMemoryManager::Get().ContinuousLogging_NIOperation(cMemoryManager::eContLog_CreateHeap, this, NULL, 0);
	}

	//  Description:
	//		cHeapNonIntrusive destructor.  Should not be called by the user.  Use DestroyHeapNonIntrusive.
	//  See Also:
	//		DestroyHeap
	//  Arguments:
	//		None
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		cHeapNonIntrusive destructor.  Use DestroyHeapNonIntrusive.
	cHeapNonIntrusive::~cHeapNonIntrusive()
	{

	}

	//  Description:
	//		Expands a Non Intrusive heap by adding another slab.  Alignment and page size is important.  For heaps created with a fixed address calling this
	//		twice or more will fail.  Non intrusive heaps do not take their memory from cMemoryManagers global pool and instead will always call
	//		the system allocators if it is resizable.
	//  See Also:
	//		
	//  Arguments:
	//		pMemoryAddress - Address of the memory to use to create the heap.  Must be page aligned pointer.  NULL will refer to the system allocators to get the 
	//						memory.
	//		uSize - Size in bytes of the memory to add.  Must be multiples of the page size.
	//  Return Value:
	//      TRUE if successfully expanded.
	//		FALSE otherwise.
	//  Summary:
	//		Expands a Non Intrusive heap by adding another slab.
	jrs_bool cHeapNonIntrusive::Expand(void *pMemoryAddress, jrs_sizet uSize)
	{		
		HeapWarning(uSize > 0, JRSMEMORYERROR_INITIALIZEINVALIDSIZE, "Size must be multiples of %dk and greater than 0.  Current size %dk.", m_uPageSize >> 10, uSize >> 10);
		HeapWarning(!(uSize & (m_uPageSize - 1)), JRSMEMORYERROR_INVALIDALIGN, "Size is not page aligned (%dk).  Current size %dk.", m_uPageSize >> 10, uSize >> 10);
		HeapWarning((uSize + (m_uPageSize - 1) & ~(m_uPageSize - 1)) == uSize, JRSMEMORYERROR_INVALIDALIGN, "Size is not page aligned (%dk).  Current size %dk and should be %dk.", m_uPageSize >> 10, uSize >> 10, (uSize + (m_uPageSize - 1) & ~(m_uPageSize - 1)) >> 10);
		
		// Check the size
		if(!uSize || (uSize + (m_uPageSize - 1) & ~(m_uPageSize - 1)) != uSize)
		{
			return FALSE;
		}

		// Allocate the memory if null
		if(!pMemoryAddress)
		{
			jrs_sizet pageSize = m_systemPageSize();
			pageSize = pageSize < m_uPageSize ? m_uPageSize : pageSize;

			// Work the size based on the page size of the system and then our requirements
			jrs_sizet allocsize = (uSize + (pageSize - 1)) & ~(pageSize - 1);
			allocsize = (uSize + (m_uPageSize - 1)) & ~(m_uPageSize - 1);

			pMemoryAddress = m_systemAllocator(allocsize, NULL);
			uSize = allocsize;
			if(m_systemOpCallback)
				m_systemOpCallback(this, pMemoryAddress, uSize, false);
		}

		if(!pMemoryAddress || ((jrs_sizet)pMemoryAddress & (m_uPageSize - 1)))
		{
			HeapWarning(pMemoryAddress, JRSMEMORYERROR_INVALIDADDRESS, "Must have a valid memory address or could not allocate.");
			HeapWarning(!((jrs_sizet)pMemoryAddress & (m_uPageSize - 1)), JRSMEMORYERROR_INVALIDALIGN, "Address is not page aligned (%dk).  Current memory address is 0x%p.", m_uPageSize >> 10, pMemoryAddress);
			return FALSE;
		}

		if(m_uNumSlabs >= m_uMaxNumSlabs)
		{
			HeapWarning(m_uNumSlabs < m_uMaxNumSlabs, JRSMEMORYERROR_NOVALIDNIHEAPEXPAND, "No more slabs to expand.");
			return FALSE;
		}

		// Create slab and expand
		sSlab *pSlab = &m_Slabs[m_uNumSlabs];
		m_Slabs[m_uNumSlabs].numBlocks = (jrs_u32)(uSize / m_uPageSize);
		m_Slabs[m_uNumSlabs].pBlocks = (sPageBlock *)(m_pStandardHeap->AllocateMemory((uSize / m_uPageSize) * sizeof(sPageBlock), 1024, JRSMEMORYFLAG_HEAPNISLAB, "Heap Slab"));
		m_Slabs[m_uNumSlabs].uPageBlockSize = (uSize / m_uPageSize) * sizeof(sPageBlock);
		m_Slabs[m_uNumSlabs].uSize = uSize;
		m_Slabs[m_uNumSlabs].pBase = pMemoryAddress;

		// Check for errors
		if(!m_Slabs[m_uNumSlabs].pBlocks)
		{
			HeapWarning(m_Slabs[m_uNumSlabs].pBlocks, JRSMEMORYERROR_INVALIDADDRESS, "Could not allocate slab information block.");
			return FALSE;
		}

		// Increment the count
		m_uNumSlabs++;

		// Initialize the blocks
		for(jrs_u32 i = 0; i < pSlab->numBlocks; i++)
		{
			pSlab->pBlocks[i].Clear();
			pSlab->pBlocks[i].slabNum = m_uNumSlabs - 1;
		}

		// Add to the bins
		AddPagesToBin(pSlab->pBlocks, pSlab->numBlocks);
#ifndef MEMORYMANAGER_MINIMAL
		if(m_bEnableMemoryTracking)
		{			
			// Only one header for these
			pSlab->pBlocks->pDebugInfo = m_pStandardHeap->AllocateMemory(m_uDebugHeaderSize, 0, JRSMEMORYFLAG_HEAPDEBUGTAG, "NIHeapDebug Info");
			UpdateDebugInfo(pSlab->pBlocks->pDebugInfo, "MemMan_Slab");
		}
#endif

		// Change numbers
		if(m_pHeapStartAddress > pMemoryAddress)
			m_pHeapStartAddress = pMemoryAddress;		
		
		if(m_pHeapEndAddress < (void *)((jrs_i8 *)pMemoryAddress + uSize))
			m_pHeapStartAddress = (void *)((jrs_i8 *)pMemoryAddress + uSize);
		m_uSize += uSize;

		// Expanded
		return TRUE;
	}

	//  Description:
	//		Internal.  Adds pages to the relevant sized bin for quicker look up's later.
	//  See Also:
	//		
	//  Arguments:
	//		pFirstPageOfArray - First page of a contiguous block to add to a bin.
	//		numPages - Num of pages to add to a bin.
	//  Return Value:
	//      None
	//  Summary:
	//		Internal.  Adds pages to the relevant sized bin for quicker look up's later.
	void cHeapNonIntrusive::AddPagesToBin(sPageBlock *pFirstPageOfArray, jrs_u32 numPages)
	{
		HeapWarning(numPages > 0, JRSMEMORYERROR_FATAL, "Number of pages is 0.  FATAL.");
		
		jrs_u32 uBin = JRSCountLeadingZero(numPages);
		m_uAvailableBins |= 1 << uBin;

		pFirstPageOfArray->pageFlags = JRSMEMORYMANAGER_PAGEFREE;
		pFirstPageOfArray->numFreePages = numPages;
		pFirstPageOfArray->pNext = m_pBins[uBin];
		pFirstPageOfArray->pPrev = NULL;
		if(pFirstPageOfArray->pNext)
		{
			pFirstPageOfArray->pNext->pPrev = pFirstPageOfArray;
		}
		sPageBlock *pNext = pFirstPageOfArray + numPages;
		if(pNext < m_Slabs[pFirstPageOfArray->slabNum].pBlocks + m_Slabs[pFirstPageOfArray->slabNum].numBlocks)
			pNext->pPrevBlock = pFirstPageOfArray;
		m_pBins[uBin] = pFirstPageOfArray;
	}

	//  Description:
	//		Internal.  Finds pages that can be used for allocations.
	//  See Also:
	//		
	//  Arguments:
	//		numPages - Number of pages in a contiguous block to find.
	//		uAlignment - Alignment of the allocation that we require.
	//  Return Value:
	//      Valid block for allocations.  NULL otherwise.
	//  Summary:
	//		Internal.  Finds pages that can be used for allocations.
	cHeapNonIntrusive::sPageBlock *cHeapNonIntrusive::FindPages(jrs_u32 numPages, jrs_sizet uAlignment)
	{
		// Choose the bin to select the pages from
		jrs_u32 pageBin = JRSCountLeadingZero(numPages);
		jrs_u32 mask = (1 << pageBin) - 1;

		// If there are no pages then we have no space
		// Note: This is a quick way to eliminate page.  Potentially, the the size may still not exist but we need to scan the bin to find that.
		// Fortunately this also allows for alignment later on.
		if(!(m_uAvailableBins & ~mask))
		{
			// Resize?
			if(!m_bResizable)
				return NULL;
			
			jrs_sizet uResizeSize = (m_uResizableSize + (m_uPageSize - 1)) & ~(m_uPageSize - 1);
			uResizeSize += uAlignment > m_uPageSize ? uAlignment : 0;

			if(!Expand(NULL, uResizeSize))
			{
				return NULL;
			}			
		}

		// Space available
		jrs_u32 bin = JRSCountTrailingZero(m_uAvailableBins & ~mask);
		HeapWarning(bin != 32, JRSMEMORYERROR_FATAL, "Serious error.  Please report.");

		// Remove the pages, split if need be and add new bins if need be.
		if(uAlignment <= m_uPageSize)
			uAlignment = 0;					// Clear, its not interesting as we always have this alignment guarenteed.

		// 1. Remove.  If possible we still need to check for out of memory.
		sPageBlock *pBlock = m_pBins[bin];
		HeapWarning(pBlock, JRSMEMORYERROR_BININVALID, "Bins are invalid. FATAL.");
		while(TRUE)
		{
			// Does it fit
			if(pBlock->numFreePages >= numPages)
			{
				// Yes, check alignment
				if(!uAlignment)
					break;

				// Check alignment matches
				jrs_u32 uBlockOffset = (jrs_u32)(pBlock - m_Slabs[pBlock->slabNum].pBlocks);
				jrs_i8 *pMemLocation = (jrs_i8 *)m_Slabs[pBlock->slabNum].pBase + (uBlockOffset * m_uPageSize);

				// Check the alignment to the memory
				jrs_i8 *pAlignMem = (jrs_i8 *)(((jrs_sizet)pMemLocation + (uAlignment - 1)) & ~(uAlignment - 1));
				if(pAlignMem == pMemLocation)
					break;			// matches.

				// See if it fits
				sPageBlock *pAlignBlock = pBlock + ((jrs_sizet)(pAlignMem - pMemLocation) / m_uPageSize);
				jrs_u32 pagesToSkip = (jrs_u32)(pAlignBlock - pBlock);
				if(pBlock->numFreePages - pagesToSkip >= numPages)
				{
					RemoveFromBin(pageBin, pBlock);

					// Add the align block, it will get removed later.  This could be improved.
					AddPagesToBin(pAlignBlock, pBlock->numFreePages - pagesToSkip);
					pAlignBlock->pPrevBlock = pBlock;
#ifndef MEMORYMANAGER_MINIMAL
					if(m_bEnableMemoryTracking)
					{			
						// Only one header for these
						pAlignBlock->pDebugInfo = m_pStandardHeap->AllocateMemory(m_uDebugHeaderSize, 0, JRSMEMORYFLAG_HEAPDEBUGTAG, "NIHeapDebug Info");
						UpdateDebugInfo(pAlignBlock->pDebugInfo, "MemMan_Empty");
					}
#endif

					sPageBlock *pSplit = pBlock;
					AddPagesToBin(pSplit, pagesToSkip);
#ifndef MEMORYMANAGER_MINIMAL
					if(m_bEnableMemoryTracking)
					{			
						// Only one header for these
						pSplit->pDebugInfo = m_pStandardHeap->AllocateMemory(m_uDebugHeaderSize, 0, JRSMEMORYFLAG_HEAPDEBUGTAG, "NIHeapDebug Info");
						UpdateDebugInfo(pSplit->pDebugInfo, "MemMan_Empty");
					}
#endif						
					pBlock = pAlignBlock;
					break;				// Fits
				}
			}

			pBlock = pBlock->pNext;
			if(!pBlock)
			{
				// Find the next bin
				mask = (mask << 1) + 1;
				if(!(m_uAvailableBins & ~mask))
				{
					// Resize?
					if(!m_bResizable)
						return NULL;

					jrs_sizet uResizeSize = (m_uResizableSize + (m_uPageSize - 1)) & ~(m_uPageSize - 1);
					uResizeSize += uAlignment > m_uPageSize ? uAlignment : 0;

					if(!Expand(NULL, uResizeSize))
					{
						return NULL;
					}	
				}

				jrs_u32 bin = JRSCountTrailingZero(m_uAvailableBins & ~mask);
				HeapWarning(bin != 32, JRSMEMORYERROR_FATAL, "Serious error.  Please report.");
				pBlock = m_pBins[bin];
				HeapWarning(pBlock, JRSMEMORYERROR_BININVALID, "Bins are invalid. FATAL.");
			}
		}
		RemoveFromBin(pageBin, pBlock);

		// 2. Split and add back to the bins if needed.
		if(pBlock->numFreePages > numPages)
		{
			sPageBlock *pSplit = pBlock + numPages;
			AddPagesToBin(pSplit, pBlock->numFreePages - numPages);
#ifndef MEMORYMANAGER_MINIMAL
			if(m_bEnableMemoryTracking)
			{			
				// Only one header for these
				pSplit->pDebugInfo = m_pStandardHeap->AllocateMemory(m_uDebugHeaderSize, 0, JRSMEMORYFLAG_HEAPDEBUGTAG, "NIHeapDebug Info");
				UpdateDebugInfo(pSplit->pDebugInfo, "MemMan_Empty");
			}
#endif
		}
		
		// 3. Initialize the page
		pBlock->pNext = NULL;
		pBlock->pPrev = NULL;
		pBlock->numFreePages = numPages;
		pBlock->pageFlags = JRSMEMORYMANAGER_PAGEALLOCATED;
		sPageBlock *pNext = pBlock + numPages;
		if(pNext < m_Slabs[pBlock->slabNum].pBlocks + m_Slabs[pBlock->slabNum].numBlocks)
			pNext->pPrevBlock = pBlock;

		// 4. Return
		return pBlock;
	}
	
	//  Description:
	//		Main call to allocate memory directly to the heap.  This function is the main allocation function
	//		to use. 
	//		Flags are set by the user.  It can be one of JRSMEMORYFLAG_xxx or any user specified flags > JRSMEMORYFLAG_RESERVED3
	//		but smaller than or equal to 15, values greater than 15 will be lost and operation of AllocateMemory is undefined.  Input text is
	//		limited to 32 chars including terminator.  Strings longer than this will only store the last 31 chars.
	//		Alignment must be a power of two however a 0 will default to the default allocation size set when the heap was created.
	//  See Also:
	//		FreeMemory
	//  Arguments:
	//      uSize - Size in bytes. Minimum size will be 16bytes unless the heap settings have set a larger minimum size.
	//		uAlignment - Default alignment is 64bytes unless heap settings have set a larger alignment.
	//					 Any specified alignments must be a power of 2. Setting 0 will default to the minimum requested alignment of the heap.
	//		uFlag - One of JRSMEMORYFLAG_xxx or user defined value.  Default JRSMEMORYFLAG_NONE.  See description for more details.
	//		pName - NULL terminating text string to associate with the allocation. May be NULL.
	//		uExternalId - An Id that to associate with the allocation.  Default 0.  Not available to NI Heaps.
	//  Return Value:
	//      Valid pointer to allocated memory.
	//		NULL otherwise.
	//  Summary:
	//      Allocates memory with additional information.
	void *cHeapNonIntrusive::AllocateMemory(jrs_sizet uSize, jrs_u32 uAlignment, jrs_u32 uFlag /*= JRSMEMORYFLAG_NONE*/, const jrs_i8 *pName /*= 0*/, const jrs_u32 uExternalId /* = 0*/)
	{
#ifndef MEMORYMANAGER_MINIMAL
		if(!cMemoryManager::Get().IsInitialized())
		{
			HeapWarning(cMemoryManager::Get().IsInitialized(), JRSMEMORYERROR_NOTINITIALIZED, "Elephant is not initialized.");
			return 0;
		}
#endif

		// 0 size allocations
		if(!uSize)
		{
			if(m_bAllowZeroSizeAllocations)
				uSize = m_uMinAllocSize;
			else
			{
				HeapWarning(uSize > 0, JRSMEMORYERROR_ZEROBYTEALLOC, "Cannot allocate because we are allocating a 0 byte allocation.  Set the heap 'bAllowZeroSizeAllocations' flag to enable this");
				return 0;
			}
		}
		else if(uSize < m_uMinAllocSize)
			uSize = m_uMinAllocSize;

		// Check if the size is larger than the maximum size allowed
#ifndef MEMORYMANAGER_MINIMAL
		if(m_uMaxAllocSize && uSize > m_uMaxAllocSize)
		{
			HeapWarning(uSize <= m_uMaxAllocSize, JRSMEMORYERROR_SIZETOLARGE, "Size requested from the heap (%s) is larger than the maximum size allowed (%d bytes)", m_HeapName, m_uMaxAllocSize);
			return 0;
		}
#endif
		jrs_sizet uAlignedSize = uSize;
		void *pMemAddress = NULL;

		// Thread lock
		if(m_bThreadSafe)
			m_pThreadLock->Lock();

		// Cannot allocate if the heap is locked.
		if(IsLocked())
		{
			HeapWarning(!IsLocked(), JRSMEMORYERROR_LOCKED, "Heap is locked.  You may not allocate memory.");
			HEAP_THREADUNLOCK
				return 0;
		}

		// Do we require page size allocations or sub page size allocations
		if(uSize > m_uPageSize >> 1)
		{
			// Find the pages, round size to nearest page
			uAlignedSize = ((uSize + (m_uPageSize - 1)) & ~(m_uPageSize - 1));
			jrs_u32 numPages = (jrs_u32)(uAlignedSize / m_uPageSize);
			sPageBlock *pBlock = FindPages(numPages, uAlignment);

			// Were we successful?
			if(!pBlock)
			{
				// No
				HeapWarning(m_bAllowNotEnoughSpaceReturn, JRSMEMORYERROR_OUTOFMEMORY, "Out of memory, cannot allocate %llu bytes from heap named %s.  Not enough free space.", (jrs_u64)uSize, m_HeapName);
				if(m_bThreadSafe)
					m_pThreadLock->Unlock();
				return 0;
			}

			// Page size or more - do nothing, already handled above
			// Get the address of the page.
			sSlab *pSlab = &m_Slabs[pBlock->slabNum];
			jrs_sizet pageOffsetInSlab = pBlock - pSlab->pBlocks;
			pMemAddress = (void *)((jrs_i8 *)m_Slabs[pBlock->slabNum].pBase + (pageOffsetInSlab * m_uPageSize));
#ifndef MEMORYMANAGER_MINIMAL
			// Debug information
			if(m_bEnableMemoryTracking)
			{
				// Only one header for these
				pBlock->pDebugInfo = m_pStandardHeap->AllocateMemory(m_uDebugHeaderSize, 0, JRSMEMORYFLAG_HEAPDEBUGTAG, "NIHeapDebug Info");
				UpdateDebugInfo(pBlock->pDebugInfo, pName);
			}
#endif
		}
		else
		{
			// Check for power of 2
			if((uSize & (uSize - 1)))
			{
				uSize = (jrs_sizet)(1 << (JRSCountLeadingZero((jrs_u32)uSize) + 1));
			}
			uAlignedSize = uSize;

			// Sub page			
			jrs_u32 bin = JRSCountTrailingZero((jrs_u32)uSize) - 6;
			sPageBlock *pBlock = m_pAllocBins[bin];
			// if the align
			if(uAlignment > uSize)
				pBlock = NULL;
			
			// Allocate a new block
			if(!pBlock)
			{
				// No page, allocate one
				pBlock = FindPages(1, uAlignment);
				
				// Were we successful?
				if(!pBlock)
				{
					// No
					HeapWarning(m_bAllowNotEnoughSpaceReturn, JRSMEMORYERROR_OUTOFMEMORY, "Out of memory, cannot allocate %llu bytes from heap named %s.  Not enough free space.", (jrs_u64)uSize, m_HeapName);
					if(m_bThreadSafe)
						m_pThreadLock->Unlock();
					return 0;
				}

				pBlock->pageFlags |= JRSMEMORYMANAGER_PAGESUBALLOC;
				pBlock->sizeOfSubAllocs = (jrs_u32)uSize;
				
				// Add it to the pages.			
				pBlock->pNext = m_pAllocBins[bin];
				if(pBlock->pNext)
					pBlock->pNext->pPrev = pBlock;
				pBlock->pPrev = NULL;
				m_pAllocBins[bin] = pBlock;
			
				// Were we successful?
				if(!pBlock)
				{
					// No
					if(m_bAllowNotEnoughSpaceReturn)
					{
						if(m_bThreadSafe)
							m_pThreadLock->Unlock();
						return 0;
					}

					HeapWarning(0, JRSMEMORYERROR_OUTOFMEMORY, "Out of memory, cannot allocate %llu bytes from heap named %s.  Not enough free space.", (jrs_u64)uSize, m_HeapName);
					if(m_bThreadSafe)
						m_pThreadLock->Unlock();
					return 0;
				}

#ifndef MEMORYMANAGER_MINIMAL
				// Debug information
				if(m_bEnableMemoryTracking)
				{
					pBlock->pDebugInfo = m_pStandardHeap->AllocateMemory(m_uDebugHeaderSize * (m_uPageSize / pBlock->sizeOfSubAllocs), 0, JRSMEMORYFLAG_HEAPDEBUGTAG, "NIHeapDebug Info");
					
					for(jrs_u32 uDebugBlock = 0; uDebugBlock < (m_uPageSize / pBlock->sizeOfSubAllocs); uDebugBlock++)
					{
						void *pDebug = ((jrs_u8 *)pBlock->pDebugInfo + (m_uDebugHeaderSize * uDebugBlock));
						UpdateDebugInfo(pDebug, "MemMan_Empty");
					}
				}
#endif
			}

			// Allocate from the block
			pMemAddress = AddSubAllocation(pBlock, uSize);

#ifndef MEMORYMANAGER_MINIMAL
			// Set debug details
			if(m_bEnableMemoryTracking)
			{
				// Find the page by getting the base alignment
				jrs_sizet offset = ((jrs_sizet)pMemAddress & (m_uPageSize - 1));
				offset /= pBlock->sizeOfSubAllocs;

				void *pDebug = ((jrs_u8 *)pBlock->pDebugInfo + (m_uDebugHeaderSize * offset));

				// Find the offset 
				UpdateDebugInfo(pDebug, pName);
			}
#endif

			// Check if the block is full, if so remove it from the list with gaps
			if(pBlock->activeAllocs[0] == 0xffffffff &&
				pBlock->activeAllocs[1]  == 0xffffffff &&
				pBlock->activeAllocs[2]  == 0xffffffff &&
				pBlock->activeAllocs[3] == 0xffffffff)
			{
				// Remove the from the active list
				sPageBlock *pPrev = pBlock->pPrev;
				sPageBlock *pNext = pBlock->pNext;
				if(pPrev)
					pPrev->pNext = pNext;
				if(pNext)
					pNext->pPrev = pPrev;
				if(m_pAllocBins[bin] == pBlock)
					m_pAllocBins[bin] = pNext;

				// Move it to the alloc list
				pBlock->pNext = m_pFullAllocBins[bin];
				pBlock->pPrev = NULL;
				if(pBlock->pNext)
					pBlock->pNext->pPrev = pBlock;
				m_pFullAllocBins[bin] = pBlock;				
			}
		}

		// Stats
		m_uAllocatedCount++;
		m_uAllocatedSize += uAlignedSize;

		if(m_uAllocatedCount > m_uAllocatedCountMax)
			m_uAllocatedCountMax = m_uAllocatedCount;

		if(m_uAllocatedSize > m_uAllocatedSizeMax)
			m_uAllocatedSizeMax = m_uAllocatedSize;

		if(m_bThreadSafe)
			m_pThreadLock->Unlock();
		
#ifndef MEMORYMANAGER_MINIMAL
		if(m_bEnableLogging)
		{
			cMemoryManager::Get().ContinuousLogging_HeapNIOperation(cMemoryManager::eContLog_Allocate, this, pMemAddress, uAlignment, uSize, 0);
		}
#endif

		// Return memory
		return pMemAddress;
	}

	//  Description:
	//		Internal. Updates the debug information of an allocation.  
	//  See Also:
	//		
	//  Arguments:
	//		pDebugInfo - Pointer to the start of the block that stores debug information.
	//		pName - Debug text to associate with the allocation/free block.
	//  Return Value:
	//      None
	//  Summary:
	//		Internal. Updates the debug information of an allocation.  
	void cHeapNonIntrusive::UpdateDebugInfo(void *pDebugInfo, const jrs_i8 *pName)
	{
#ifndef MEMORYMANAGER_MINIMAL
		jrs_i8 *pText = (jrs_i8 *)pDebugInfo;
		jrs_sizet *puCallStack = (jrs_sizet *)(pText + MemoryManager_StringLength);

		memset(pDebugInfo, 0, m_uDebugHeaderSize);
		// Set the names etc and if we are on a supported platform get the stack trace
		if(pName && pName[0])
		{
			jrs_u32 uNameM = 0, uNameL = (jrs_u32)strlen(pName);
			const jrs_i8 *pN = &pName[(uNameL > (MemoryManager_StringLength - 1)) ? uNameL - (MemoryManager_StringLength - 1) : 0];
			while(*pN)
			{
				pText[uNameM] = (*pN == ';') ? '.' : *pN;
				uNameM++;
				pN++;
			}
			pText[uNameM] = 0;
		}
		else
			strcpy(pText, "Unknown Allocation");
		cMemoryManager::Get().StackTrace(puCallStack, m_uCallstackDepth, m_uNumCallStacks);
#endif
	}

	//  Description:
	//		Internal.  Adds allocations smaller than the page size to a block.
	//  See Also:
	//		
	//  Arguments:
	//		pBlock - Valid block to allocate the sub allocation from.
	//		uSize - Size of the sub allocation to allocate.
	//  Return Value:
	//      Valid pointer to the allocation.
	//  Summary:
	//		Internal.  Adds allocations smaller than the page size to a block.
	void *cHeapNonIntrusive::AddSubAllocation(sPageBlock *pBlock, jrs_sizet uSize)
	{
		// Find the first free space in the block.
		jrs_u32 index = 0;
		jrs_u32 offset = JRSCountTrailingZero(~pBlock->activeAllocs[0]);
		if(offset >= 32) 
		{
			index++;
			offset = JRSCountTrailingZero(~pBlock->activeAllocs[1]);
			if(offset >= 32) 
			{
				index++;
				offset = JRSCountTrailingZero(~pBlock->activeAllocs[2]);
				if(offset >= 32)
				{
					index++;
					offset = JRSCountTrailingZero(~pBlock->activeAllocs[3]);
				}
			}
		}
		
		// Mark it as taken
		jrs_u32 bitSize = (jrs_u32)(uSize >> 6);
		jrs_u32 byteOffset = 0;
		if(bitSize <= 32)
		{
			jrs_u32 bitsToSet = (2 << (bitSize - 1)) - 1;
			byteOffset = (offset * 64) + (index * 2048);	
			pBlock->activeAllocs[index]	|= bitsToSet << offset;
		}
		else
		{
			// special case 4k allocs
			HeapWarning(index <= 2, JRSMEMORYERROR_SIZETOLARGE, "Index is not 0 or 2");
			byteOffset = (index * 2048);		// offset totally
			pBlock->activeAllocs[index]	= 0xffffffff;
			pBlock->activeAllocs[index + 1]	= 0xffffffff;
		}		

		// Return the memory
		sSlab *pSlab = &m_Slabs[pBlock->slabNum];
		jrs_sizet pageOffsetInSlab = pBlock - pSlab->pBlocks;
		jrs_i8 *pMemAddress = ((jrs_i8 *)m_Slabs[pBlock->slabNum].pBase + (pageOffsetInSlab * m_uPageSize));
		pMemAddress += byteOffset;
		return pMemAddress;
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
	//		uExternalId - An Id that to associate with the allocation.  Default 0.  Not available to NI Heaps.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Frees memory from within a heap.   
	void cHeapNonIntrusive::FreeMemory(void *pMemory, jrs_u32 uFlag /*= JRSMEMORYFLAG_NONE*/, const jrs_i8 *pName /*= 0*/, const jrs_u32 uExternalId /*=0*/)
	{
#ifndef MEMORYMANAGER_MINIMAL
		if(!cMemoryManager::Get().IsInitialized())
		{
			HeapWarning(cMemoryManager::Get().IsInitialized(), JRSMEMORYERROR_NOTINITIALIZED, "Elephant is not initialized.");
			return;
		}
#endif

		if(!pMemory)
		{
			// Null free
			if(m_bAllowNullFree)
				return;

			HeapWarning(pMemory, JRSMEMORYERROR_NULLPTR, "Trying to free NULL memory address.");
			return;
		}

		// Find the page by getting the base alignment
		jrs_i8 *pPageAdd = (jrs_i8 *)((jrs_sizet)pMemory & ~(m_uPageSize - 1));
		
		if(m_bThreadSafe)
			m_pThreadLock->Lock();

		// Cannot free if the heap is locked.
		if(IsLocked())
		{
			HeapWarning(!IsLocked(), JRSMEMORYERROR_LOCKED, "Heap is locked.  You may not free memory.");
			HEAP_THREADUNLOCK
			return;
		}

		// Find the slab it came from
		sSlab *pSlab = NULL;
		for(jrs_u32 i = 0; i < m_uNumSlabs; i++)
		{
			if(pPageAdd >= (jrs_i8 *)m_Slabs[i].pBase && pPageAdd < ((jrs_i8 *)m_Slabs[i].pBase + m_Slabs[i].uSize))
			{
				pSlab = &m_Slabs[i];
				break;
			}
		}

		// Slab not found
		if(!pSlab)
		{
			HeapWarning(pSlab, JRSMEMORYERROR_HEAPINVALID, "Memory doesnt appear to come from this heap.");
			if(m_bThreadSafe)
				m_pThreadLock->Unlock();
			return;
		}

		jrs_sizet pageIndex = ((jrs_sizet)pPageAdd - (jrs_sizet)pSlab->pBase) / m_uPageSize;
		sPageBlock *pBlock = &pSlab->pBlocks[pageIndex];
		HeapWarning(pBlock->pageFlags & JRSMEMORYMANAGER_PAGEALLOCATED, JRSMEMORYERROR_INVALIDADDRESS, "Memory address 0x%p has already been freed", pBlock);

#ifndef MEMORYMANAGER_MINIMAL
		if(m_bEnableLogging)
		{
			cMemoryManager::Get().ContinuousLogging_HeapNIOperation(cMemoryManager::eContLog_Allocate, this, pMemory, 0, pBlock->pageFlags & JRSMEMORYMANAGER_PAGESUBALLOC ? pBlock->sizeOfSubAllocs : pBlock->numFreePages * m_uPageSize, 0);
		}
#endif

		sPageBlock *pBlockPrev = pBlock->pPrevBlock;
		sPageBlock *pBlockNext = pBlock + pBlock->numFreePages;
		if(pBlockNext >= m_Slabs[pBlock->slabNum].pBlocks + m_Slabs[pBlock->slabNum].numBlocks)
			pBlockNext = NULL;

		// Is it a small allocation
		if(pBlock->pageFlags & JRSMEMORYMANAGER_PAGESUBALLOC)
		{
			jrs_u32 bitSize = (pBlock->sizeOfSubAllocs >> 6);
			jrs_u32 bitsToSet = (2 << (bitSize - 1)) - 1;// << offset;
			jrs_sizet offset = ((jrs_sizet)pMemory & (m_uPageSize - 1));
			jrs_u32 index = (jrs_u32)(offset / 2048);		// one 32 bit coverage page is 2k
			jrs_u32 offsetInBits = (jrs_u32)((offset >> 6) - (index * 32));

			// If its a full page we need to remove it from the full list and insert it into the free list
			if(pBlock->activeAllocs[0] == 0xffffffff &&
				pBlock->activeAllocs[1] == 0xffffffff &&
				pBlock->activeAllocs[2] == 0xffffffff &&
				pBlock->activeAllocs[3] == 0xffffffff)
			{
				// Remove the from the full list
				jrs_u32 bin = JRSCountTrailingZero(pBlock->sizeOfSubAllocs) - 6;
				sPageBlock *pPrev = pBlock->pPrev;
				sPageBlock *pNext = pBlock->pNext;
				if(pPrev)
					pPrev->pNext = pNext;
				if(pNext)
					pNext->pPrev = pPrev;
				if(m_pFullAllocBins[bin] == pBlock)
					m_pFullAllocBins[bin] = pNext;

				// Move it to the active list
				pBlock->pNext = m_pAllocBins[bin];
				pBlock->pPrev = NULL;
				if(pBlock->pNext)
					pBlock->pNext->pPrev = pBlock;
				m_pAllocBins[bin] = pBlock;		
			}

			// clear the flags
			if(bitSize <= 32)
				pBlock->activeAllocs[index] &= ~(bitsToSet << offsetInBits);
			else
			{
				pBlock->activeAllocs[index] = 0;
				pBlock->activeAllocs[index + 1] = 0;
			}

#ifndef MEMORYMANAGER_MINIMAL
			// Clear the debug flags
			if(m_bEnableMemoryTracking)
			{			
				jrs_sizet offsetDebug = offset / pBlock->sizeOfSubAllocs;
				void *pDebug = ((jrs_u8 *)pBlock->pDebugInfo + (m_uDebugHeaderSize * offsetDebug));
				
				UpdateDebugInfo(pDebug, pName);
			}
#endif

			// Update stats
			m_uAllocatedCount--;
			m_uAllocatedSize -= pBlock->sizeOfSubAllocs;

			// Do we need to free the page?
			if(pBlock->activeAllocs[0] | pBlock->activeAllocs[1] | pBlock->activeAllocs[2] | pBlock->activeAllocs[3])
			{
				if(m_bThreadSafe)
					m_pThreadLock->Unlock();
				return;		// No need to free the page
			}

			// Remove the from the active list
			jrs_u32 bin = JRSCountTrailingZero(pBlock->sizeOfSubAllocs) - 6;
			sPageBlock *pPrev = pBlock->pPrev;
			sPageBlock *pNext = pBlock->pNext;
			if(pPrev)
				pPrev->pNext = pNext;
			if(pNext)
				pNext->pPrev = pPrev;
			if(m_pAllocBins[bin] == pBlock)
				m_pAllocBins[bin] = pNext;
		}
		else
		{
			// Update stats
			m_uAllocatedCount--;
			m_uAllocatedSize -= pBlock->numFreePages * m_uPageSize;
		}
		// Free the memory, add it back into the main pool

#ifndef MEMORYMANAGER_MINIMAL
		// Debug information
		if(m_bEnableMemoryTracking)
		{
			// Only one header for these
			m_pStandardHeap->FreeMemory(pBlock->pDebugInfo, JRSMEMORYFLAG_HEAPDEBUGTAG, "NIHeap Debug Free");
			pBlock->pDebugInfo = NULL;
		}
#endif

		// See if the previous or next can be consolidated
		jrs_u32 uNumPages = pBlock->numFreePages;
		jrs_bool bUpdatePointers = FALSE;
		if(pBlockNext && pBlockNext->pageFlags & JRSMEMORYMANAGER_PAGEFREE)
		{
			bUpdatePointers = TRUE;
			uNumPages += pBlockNext->numFreePages;
			sPageBlock *pPotentialNext = pBlockNext + pBlockNext->numFreePages;
			RemoveFromBin(pBlockNext);

			// Move to the next
			pBlockNext = pPotentialNext;
			if(pBlockNext >= m_Slabs[pBlock->slabNum].pBlocks + m_Slabs[pBlock->slabNum].numBlocks)
				pBlockNext = NULL;
		}

		if(pBlockPrev && pBlockPrev->pageFlags & JRSMEMORYMANAGER_PAGEFREE && (pBlockPrev + pBlockPrev->numFreePages) == pBlock)
		{
			bUpdatePointers = TRUE;
			uNumPages += pBlockPrev->numFreePages;
			RemoveFromBin(pBlockPrev);

			pBlock = pBlockPrev;
		}
		
		// Remove the allocated block from the list

		// Add the free block back
		AddPagesToBin(pBlock, uNumPages);
#ifndef MEMORYMANAGER_MINIMAL
		if(m_bEnableMemoryTracking)
		{			
			// Only one header for these
			pBlock->pDebugInfo = m_pStandardHeap->AllocateMemory(m_uDebugHeaderSize, 0, JRSMEMORYFLAG_HEAPDEBUGTAG, "NIHeapDebug Info");
			UpdateDebugInfo(pBlock->pDebugInfo, "MemMan_Empty");
		}
#endif

		// Finalize
		pBlock->numFreePages = uNumPages;
		pBlock->pageFlags = JRSMEMORYMANAGER_PAGEFREE;
		if(bUpdatePointers && pBlockNext)
		{
			pBlockNext->pPrevBlock = pBlock;
		}

		if(m_bThreadSafe)
			m_pThreadLock->Unlock();
	}

	//  Description:
	//		Internal.  Clears The page blocks to known values.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      None
	//  Summary:
	//		Internal.  Clears The page blocks to known values.
	void cHeapNonIntrusive::sPageBlock::Clear(void)
	{
		activeAllocs[0] = activeAllocs[1] = activeAllocs[2] = activeAllocs[3] = 0;
		pPrevBlock = pNext = pPrev = NULL;
		numFreePages = 0;
		pageFlags = JRSMEMORYMANAGER_PAGEFREE;
		sizeOfSubAllocs = 0;
		pDebugInfo = NULL;
	}

	//  Description:
	//		Internal. Removes blocks from a bin.
	//  See Also:
	//		
	//  Arguments:
	//		uBin - Bin to remove from (not used).
	//		pBlock - Block to remove from a bin.
	//  Return Value:
	//      None
	//  Summary:
	//		Internal. Removes blocks from a bin.
	void cHeapNonIntrusive::RemoveFromBin(jrs_u32 uBin, cHeapNonIntrusive::sPageBlock *pBlock)
	{
		jrs_u32 uBinP = JRSCountLeadingZero(pBlock->numFreePages);
	
		if(pBlock->pNext)
			pBlock->pNext->pPrev = pBlock->pPrev;
		if(pBlock->pPrev)
			pBlock->pPrev->pNext = pBlock->pNext;

		// Set the link to the next if needed
		if(m_pBins[uBinP] == pBlock)
			m_pBins[uBinP] = pBlock->pNext;
		
		// Clear the flags if needed.
		if(!m_pBins[uBinP])
		{			
			m_uAvailableBins &= ~(1 << uBinP);
		}

#ifndef MEMORYMANAGER_MINIMAL
		if(m_bEnableMemoryTracking)
		{			
			// Only one header for these
			m_pStandardHeap->FreeMemory(pBlock->pDebugInfo, JRSMEMORYFLAG_HEAPDEBUGTAG, "MemMan_Removed");
			pBlock->pDebugInfo = NULL;
		}
#endif
	}

	//  Description:
	//		Internal. Removes blocks from a bin.
	//  See Also:
	//		
	//  Arguments:
	//		pBlock - Block to remove from a bin.
	//  Return Value:
	//      None
	//  Summary:
	//		Internal. Removes blocks from a bin.
	void cHeapNonIntrusive::RemoveFromBin(cHeapNonIntrusive::sPageBlock *pBlock)
	{
		jrs_u32 pageBin = JRSCountLeadingZero(pBlock->numFreePages);
		RemoveFromBin(pageBin, pBlock);
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
	void cHeapNonIntrusive::ReportAll(const jrs_i8 *pLogToFile, jrs_bool includeFreeBlocks, jrs_bool displayCallStack)
	{
		ReportStatistics();
		ReportAllocationsMemoryOrder(pLogToFile);
	}

	//  Description:
	//		Resets the internal statistics used by the heap.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      None
	//  Summary:
	//      Resets the internal statistics used by the heap.
	void cHeapNonIntrusive::ResetStatistics(void)
	{
		m_uAllocatedCountMax = m_uAllocatedCount;
		m_uAllocatedSizeMax = m_uAllocatedSize;
	}

	//  Description:
	//		Reports basic statistics about the heap to the user TTY callback. Use this to get quick information on 
	//		when need that is more detailed that simple heap memory real time calls.
	//  See Also:
	//		ReportAll, ReportAllocationsMemoryOrder, ReportAllocationsUniqueTotals
	//  Arguments:
	//		bAdvanced - TRUE to display extra statistics.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Reports basic statistics about all heaps to the user TTY callback. 
	void cHeapNonIntrusive::ReportStatistics(jrs_bool bAdvanced)
	{
		HEAP_THREADLOCK
		m_bEnableReportsInErrors = false;

		cMemoryManager::DebugOutput("---------------------------------------------------------------------------------------------");

		jrs_i8 *pName = m_HeapName;
		jrs_sizet uHeapSize = GetSize();

		// Report
		cMemoryManager::DebugOutput("Logging Non Intrusive Heap: %s", pName);
		cMemoryManager::DebugOutput("Number of Slabs: %d", m_uNumSlabs);
		cMemoryManager::DebugOutput("Heap Size: %dMB (%dk (0x%llxk))", uHeapSize >> 20, uHeapSize >> 10, (jrs_u64)uHeapSize >> 10);

		// Work out some other stats
		jrs_u32 AllocCount = 0;
		jrs_u64 TotalFreeMemory = 0;
		jrs_u32 TotalFreeCount = 0;
		jrs_u64 uAllocatedSize = m_uAllocatedSize;

		// Loop for linked heaps
		jrs_sizet AllocSize = 0;
		jrs_sizet FreeCount = 0;
		jrs_sizet FreeSize = 0;

		// Go through all the blocks		
		for(jrs_u32 i = 0; i < m_uNumSlabs; i++)
		{
			sPageBlock *pEndBlock = m_Slabs[i].pBlocks + m_Slabs[i].numBlocks;
			sPageBlock *pBlock = m_Slabs[i].pBlocks;

			HeapWarning(!pBlock->pPrevBlock, JRSMEMORYERROR_INVALIDADDRESS, "Previous address should be NULL.  Address is 0x%p.", pBlock->pPrevBlock);
			while(pBlock)
			{
				if(pBlock->pageFlags & JRSMEMORYMANAGER_PAGEALLOCATED)
				{
					if(pBlock->pageFlags & JRSMEMORYMANAGER_PAGESUBALLOC)
					{
						// Find the set bits
						jrs_u32 totalBits = 0; 
						for(jrs_u32 ind = 0; ind < 4; ind++)
						{
							jrs_u32 val = pBlock->activeAllocs[ind];
							for (jrs_u32 c = 0; val; c++, totalBits++) 
								val &= val - 1;
						}
						totalBits /= pBlock->sizeOfSubAllocs >> 6;
						AllocSize += totalBits * pBlock->sizeOfSubAllocs;
						AllocCount += totalBits;
					}
					else
					{
						AllocSize += pBlock->numFreePages * m_uPageSize;
						AllocCount++;
					}
				}
				else
				{
					// Check pointers points to correct size block
					HeapWarning(!pBlock->pNext || pBlock->pNext->pageFlags & JRSMEMORYMANAGER_PAGEFREE, JRSMEMORYERROR_INVALIDADDRESS, "Free links are invalid.  Errors may occur.");
					HeapWarning(!pBlock->pPrev || pBlock->pPrev->pageFlags & JRSMEMORYMANAGER_PAGEFREE, JRSMEMORYERROR_INVALIDADDRESS, "Free links are invalid.  Errors may occur.");

					FreeCount++;
					FreeSize += pBlock->numFreePages * m_uPageSize;
				}

				// Next
				sPageBlock *pBlockNext = pBlock + pBlock->numFreePages;
				if(pBlockNext >= pEndBlock)
				{
					break;
				}

				HeapWarning(pBlockNext->pPrevBlock == pBlock, JRSMEMORYERROR_INVALIDADDRESS, "Previous address is invalid.  Address is 0x%p but should be 0x%p.", pBlockNext->pPrevBlock, pBlock);
				pBlock = pBlockNext;
			}
		}
		
		HeapWarning(m_uAllocatedCount == AllocCount, JRSMEMORYERROR_FATAL, "Allocated counts do not match. Detected %d but recorded %lld", AllocCount, (jrs_u64)m_uAllocatedCount);
		HeapWarning(uAllocatedSize == AllocSize, JRSMEMORYERROR_FATAL, "Allocated sizes do not match. Detected %lld but recorded %lld", AllocSize, uAllocatedSize);

		cMemoryManager::DebugOutput("Total Allocations: %d", AllocCount);
		cMemoryManager::DebugOutput("Total Memory Used: %lluk (%llu bytes)", uAllocatedSize >> 10, uAllocatedSize);
	
		cMemoryManager::DebugOutput("Total Fragments: %d", TotalFreeCount);
		cMemoryManager::DebugOutput("Total Fragmented Memory: %lluk", TotalFreeMemory >> 10);
	
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
	//		includeFreeBlocks - Reports free blocks to the output also.
	//		displayCallStack - Logs any the callstacks as well.  May be slow.  Default false.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Reports all allocations from all heaps to the user TTY callback and/or a file.
	void cHeapNonIntrusive::ReportAllocationsMemoryOrder(const jrs_i8 *pLogToFile, jrs_bool includeFreeBlocks, jrs_bool displayCallStack)
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
			m_HeapName, (jrs_u64)GetSize(), FALSE, 
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
#ifndef MEMORYMANAGER_MINIMAL
		if(m_bEnableMemoryTracking)
			cMemoryManager::DebugOutput("Allocation Number (UniqueId             Text                  Line) - Address             (HeaderAddr) Size ");
		else
#endif
			cMemoryManager::DebugOutput("Allocation Number (UniqueId) - Address            (HeaderAddr) Size ");

		jrs_sizet AllocCount = 0;
		jrs_sizet FreeCount = 0;

		// Go through all the blocks
		for(jrs_u32 i = 0; i < m_uNumSlabs; i++)
		{
			sPageBlock *pBlock = m_Slabs[i].pBlocks;
			jrs_i8 logtext[8192];

			while(pBlock)
			{
				jrs_i8 *pMemoryLocation = (jrs_i8 *)m_Slabs[i].pBase + (((((jrs_i8 *)pBlock - (jrs_i8 *)m_Slabs[i].pBlocks) / sizeof(sPageBlock)) * m_uPageSize));
				
				const jrs_i8 *pText = "Unknown";
				jrs_sizet *puCallStack = NULL;
#ifndef MEMORYMANAGER_MINIMAL
				if(m_bEnableMemoryTracking && pBlock->pDebugInfo)
				{
					pText = (jrs_i8 *)pBlock->pDebugInfo;
					puCallStack = (jrs_sizet *)(pText + MemoryManager_StringLength);
				}				
#endif

				if(pBlock->pageFlags & JRSMEMORYMANAGER_PAGEALLOCATED)
				{
					// Output some info		
					if(pBlock->pageFlags & JRSMEMORYMANAGER_PAGESUBALLOC)
					{
						// Find the set bits
						jrs_u32 bitSize = (pBlock->sizeOfSubAllocs >> 6);
						jrs_u32 bitsToSet = (2 << (bitSize - 1)) - 1;
						jrs_u32 allocsPerBlock = (32 / bitSize);
						jrs_u32 indStep = 1;
						// Handle larger than 2k blocks, 64bit machines will be better here.
						if(bitSize > 32)
						{
							allocsPerBlock = 1;
							bitsToSet = 0xffffffff;
							indStep = 2;
						}

						for(jrs_u32 ind = 0; ind < 4; ind += indStep)
						{
							jrs_u32 offset = 0;
							jrs_u32 val = pBlock->activeAllocs[ind];
							for (jrs_u32 c = 0; c < allocsPerBlock; c++) 
							{
								if(((val >> offset) & (bitsToSet)) == bitsToSet)
								{
									sprintf(logtext, "%s %6d (%8d %-32s %5d) - 0x%p (0x%p) %u", "Allocation",
										(jrs_u32)AllocCount,
										pBlock->pageFlags,
										pText,
										0,
										pMemoryLocation,
										pBlock,
										pBlock->sizeOfSubAllocs);

									if(displayCallStack)
										cMemoryManager::StackToString(&logtext[strlen(logtext)], puCallStack, m_uNumCallStacks);
									cMemoryManager::DebugOutput(logtext);

 									sprintf(logtext, "_Alloc_; %s; %llu; %llu; %llu; %llu; ",
										m_HeapName, (jrs_u64)pMemoryLocation, (jrs_u64)pBlock->sizeOfSubAllocs, 
 										(jrs_u64)(pBlock->pageFlags), (jrs_u64)0);
									// Write the callstacks
									strcat(logtext, pText);
									for(jrs_u32 cs = 0; cs < m_uNumCallStacks; cs++)
									{
										jrs_i8 temp[32];
										sprintf(temp, "; 0x%llx", puCallStack ? (jrs_u64)puCallStack[cs] : 0LL);
										strcat(logtext, temp);
									}
									
									AllocCount++;
								}
								else
								{
									if(includeFreeBlocks)
									{
										sprintf(logtext, "%s %6d (%8d %-32s %5d) - 0x%p (0x%p) %u", "Free Block",
											(jrs_u32)AllocCount,
											pBlock->pageFlags,
											pText,
											0,
											pMemoryLocation,
											pBlock,
											pBlock->sizeOfSubAllocs);
										
										if (displayCallStack)
											cMemoryManager::StackToString(&logtext[strlen(logtext)], puCallStack, m_uNumCallStacks);
										cMemoryManager::DebugOutput(logtext);
									}

									sprintf(logtext, "_Free_; %s; %llu; %llu; %u; %llu; ",						
										m_HeapName, (jrs_u64)((jrs_i8 *)pMemoryLocation), (jrs_u64)pBlock->sizeOfSubAllocs, 
										0, (jrs_u64)pBlock->pageFlags);

									strcat(logtext, pText);
									for(jrs_u32 cs = 0; cs < m_uNumCallStacks; cs++)
									{
										jrs_i8 temp[32];
										sprintf(temp, "; 0x%llx", puCallStack ? (jrs_u64)puCallStack[cs] : 0LL);
										strcat(logtext, temp);
									}

									FreeCount++;
								}

								offset += bitSize;
								pMemoryLocation += pBlock->sizeOfSubAllocs;
#ifndef MEMORYMANAGER_MINIMAL
								if(m_bEnableMemoryTracking)
								{
									pText += m_uDebugHeaderSize;
									puCallStack = (jrs_sizet *)((jrs_i8 *)puCallStack + m_uDebugHeaderSize);
								}	
#endif
							}
						}
					}
					else
					{
						sprintf(logtext, "%s %6d (%8d %-32s %5d) - 0x%p (0x%p) %llu %u", "Allocation", (jrs_u32)AllocCount, pBlock->pageFlags, pText, 0, pMemoryLocation, pBlock, (jrs_u64)pBlock->numFreePages * m_uPageSize, pBlock->numFreePages);
						if (displayCallStack)
							cMemoryManager::StackToString(&logtext[strlen(logtext)], puCallStack, m_uNumCallStacks);
						cMemoryManager::DebugOutput(logtext);

						sprintf(logtext, "_Alloc_; %s; %llu; %llu; %llu; %llu; ",
							m_HeapName, (jrs_u64)pMemoryLocation, (jrs_u64)(pBlock->numFreePages * m_uPageSize), 
							(jrs_u64)(pBlock->pageFlags), 0LL);
						// Write the callstacks
						strcat(logtext, pText);
						for(jrs_u32 cs = 0; cs < m_uNumCallStacks; cs++)
						{
							jrs_i8 temp[32];
							sprintf(temp, "; 0x%llx", puCallStack ? (jrs_u64)puCallStack[cs] : 0LL);
							strcat(logtext, temp);
						}

						AllocCount++;
					}
				}
				else
				{
					// We have a free block.
					if(includeFreeBlocks)
					{
						sprintf(logtext, "Free Block %6d (%8d %-32s %5d) - 0x%p (0x%p) %llu %u", (jrs_u32)FreeCount, pBlock->pageFlags, pText, 0, pMemoryLocation, pBlock, (jrs_u64)pBlock->numFreePages * m_uPageSize, pBlock->numFreePages);
						if (displayCallStack)
							cMemoryManager::StackToString(&logtext[strlen(logtext)], puCallStack, m_uNumCallStacks);
						cMemoryManager::DebugOutput(logtext);
					}
					sprintf(logtext, "_Free_; %s; %llu; %llu; %u; %llu; ",						
						m_HeapName, (jrs_u64)((jrs_i8 *)pMemoryLocation), (jrs_u64)pBlock->numFreePages * m_uPageSize, 
						0, (jrs_u64)pBlock->pageFlags);

					strcat(logtext, pText);
					for(jrs_u32 cs = 0; cs < m_uNumCallStacks; cs++)
					{
						jrs_i8 temp[32];
						sprintf(temp, "; 0x%llx", puCallStack ? (jrs_u64)puCallStack[cs] : 0LL);
						strcat(logtext, temp);
					}

					FreeCount++;
				}

				cMemoryManager::DebugOutputFile(pLogToFile, false, logtext);

				// Next
				pBlock = pBlock + pBlock->numFreePages;
				if(pBlock >= m_Slabs[i].pBlocks + m_Slabs[i].numBlocks)
					pBlock = NULL;
			}
		}

		cMemoryManager::DebugOutput("---------------------------------------------------------------------------------------------");
		m_bEnableReportsInErrors = true;
		HEAP_THREADUNLOCK
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
	void cHeapNonIntrusive::CheckForErrors(void)
	{
#ifndef MEMORYMANAGER_MINIMAL
		// Run through the bins and check they fit
		for(jrs_u32 bins = 0; bins < m_uMaxNumBins; bins++)
		{
			jrs_u32 uMaxSize = 1 << (bins + 1);
			jrs_u32 uMinSize = 1 << (bins);

			sPageBlock *pPage = m_pBins[bins];
			while(pPage)
			{
				jrs_u32 uBin = JRSCountLeadingZero(pPage->numFreePages);
				HeapWarning(pPage->numFreePages >= uMinSize && pPage->numFreePages < uMaxSize, JRSMEMORYERROR_WRONGBIN, "Page 0x%p has page count %d and is in the wrong bin.  Should be bin %d but is in bin %d", pPage, pPage->numFreePages, uBin, bins);
				if(pPage->pNext)
				{
					HeapWarning(pPage->pNext->pPrev == pPage, JRSMEMORYERROR_INVALIDLINK, "The previous pointer is invalid for page 0x%p", pPage);
				}
				
				// Check pointers points to correct size block
				HeapWarning(!pPage->pNext || pPage->pNext->pageFlags & JRSMEMORYMANAGER_PAGEFREE, JRSMEMORYERROR_INVALIDLINK, "Free links are invalid.  Errors may occur.");
				HeapWarning(!pPage->pPrev || pPage->pPrev->pageFlags & JRSMEMORYMANAGER_PAGEFREE, JRSMEMORYERROR_INVALIDLINK, "Free links are invalid.  Errors may occur.");

				pPage = pPage->pNext;
			}
		}

		// Check the block
		for(jrs_u32 i = 0; i < m_uNumSlabs; i++)
		{
			sPageBlock *pEndBlock = m_Slabs[i].pBlocks + m_Slabs[i].numBlocks;
			sPageBlock *pBlock = m_Slabs[i].pBlocks;

			while(pBlock)
			{
				sPageBlock *pBlockNext = pBlock + pBlock->numFreePages;
				if(pBlockNext >= pEndBlock)
				{
					break;
				}
				HeapWarning(pBlockNext->pPrevBlock == pBlock, JRSMEMORYERROR_INVALIDADDRESS, "Previous address is invalid.  Address is 0x%p but should be 0x%p in block 0x%p", pBlockNext->pPrevBlock, pBlock, pBlockNext);
				pBlock = pBlockNext;
			}
		}
#endif
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
	jrs_sizet cHeapNonIntrusive::GetSize(void) const
	{
		jrs_sizet uHeapSize = 0;
		for(jrs_u32 i = 0; i < m_uNumSlabs; i++)
		{
			uHeapSize += m_Slabs[i].uSize;
		}

		return uHeapSize;
	}
	
	//  Description:
	//		Destroys any memory that was created by the heap.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      None
	//  Summary:
	//		Destroys any memory that was created by the heap.
	void cHeapNonIntrusive::Destroy(void)
	{
		for(jrs_u32 i = 0; i < m_uNumSlabs; i++)
		{
			// When we destroy with allocations we dont bother with these, its left to the user
			if(!m_bAllowDestructionWithAllocations)
			{
#ifndef MEMORYMANAGER_MINIMAL
				if(m_bEnableMemoryTracking)
					m_pStandardHeap->FreeMemory(m_Slabs[i].pBlocks->pDebugInfo, JRSMEMORYFLAG_HEAPDEBUGTAG, "MemMan_SlabDebugFree");
				m_Slabs[i].pBlocks->pDebugInfo = NULL;
#endif
			}

			if(!m_bSelfManaged)
			{
				m_systemFree(m_Slabs[i].pBase, m_Slabs[i].uSize);
				if(m_systemOpCallback)
					m_systemOpCallback(this, m_Slabs[i].pBase, m_Slabs[i].uSize, true);
			}
			m_pStandardHeap->FreeMemory(m_Slabs[i].pBlocks, JRSMEMORYFLAG_HEAPNISLAB, "NI Heap Blocks");
		}
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
	jrs_sizet cHeapNonIntrusive::GetMemoryUsed(void) const
	{
		return m_uAllocatedSize;
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
	jrs_u32 cHeapNonIntrusive::GetNumberOfAllocations(void) const
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
	jrs_sizet cHeapNonIntrusive::GetMemoryUsedMaximum(void) const
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
	jrs_u32 cHeapNonIntrusive::GetNumberOfAllocationsMaximum(void) const
	{
		return m_uAllocatedCountMax;
	}

	//  Description:
	//		Gets the size in bytes of the largest free contiguous block of memory in the Heap.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      Size in bytes of the largest free contiguous block.
	//  Summary:
	//		Gets the size in bytes of the largest free contiguous block of memory in the Heap.
	jrs_sizet cHeapNonIntrusive::GetSizeOfLargestFragment(void) const
	{
		sPageBlock *pLargestFreeBlock = NULL;
		for(jrs_u32 i = 0; i < m_uNumSlabs; i++)
		{
			sPageBlock *pBlock = m_Slabs[i].pBlocks;

			// Scan all slab blocks
			while(pBlock)
			{
				if(!(pBlock->pageFlags & JRSMEMORYMANAGER_PAGEALLOCATED))
				{
					if(!pLargestFreeBlock)
						pLargestFreeBlock = pBlock;
					else if(pBlock->numFreePages > pLargestFreeBlock->numFreePages)
						pLargestFreeBlock = pBlock;
				}

				// Next
				pBlock = pBlock + pBlock->numFreePages;
				if(pBlock >= m_Slabs[i].pBlocks + m_Slabs[i].numBlocks)
					pBlock = NULL;
			}
		}

		// Finish
		if(!pLargestFreeBlock)
			return 0;

		return pLargestFreeBlock->numFreePages * m_uPageSize;
	}

	//  Description:
	//		Returns the amount of free memory available to be allocated.  This is the number of free pages * page size and does
	//		not include any free space available in partly used pages.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      Size in bytes of available free memory.
	//  Summary:
	//      Returns the amount of free memory available to be allocated.
	jrs_sizet cHeapNonIntrusive::GetTotalFreeMemory(void) const
	{
		return m_uSize - m_uAllocatedSize;
	}

	//  Description:
	//		Returns if error checking is enabled or not for the heap.  This is determined by the bEnableErrors flag of sHeapDetails when creating the non intrusive heap.
	//  See Also:
	//		AreErrorsWarningsOnly, CreateHeap
	//  Arguments:
	//		None
	//  Return Value:
	//      TRUE if errors are enabled.
	//		FALSE otherwise.
	//  Summary:
	//      Returns if error checking is enabled or not for the non intrusive heap.
	jrs_bool cHeapNonIntrusive::AreErrorsEnabled(void) const
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
	jrs_bool cHeapNonIntrusive::AreErrorsWarningsOnly(void) const
	{
		return m_bErrorsAsWarnings; 
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
	void cHeapNonIntrusive::SetCallstackDepth(jrs_u32 uDepth)
	{
		m_uCallstackDepth = uDepth;
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
	jrs_u32 cHeapNonIntrusive::GetUniqueId(void) const
	{
		return m_uHeapId;
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
	jrs_bool cHeapNonIntrusive::IsLoggingEnabled(void) const 
	{ 
		return m_bEnableLogging; 
	}

	//  Description:
	//		Returns if memory tracking is enabled.
	//  See Also:
	//  Arguments:
	//		None
	//  Return Value:
	//      TRUE if enabled.
	//		FALSE otherwise.
	//  Summary:
	//      Returns if memory logging is enabled.
	jrs_bool cHeapNonIntrusive::IsMemoryTrackingEnabled(void) const
	{
		return m_bEnableMemoryTracking;
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
	void *cHeapNonIntrusive::GetAddress(void) const 
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
	void *cHeapNonIntrusive::GetAddressEnd(void) const 
	{ 
		return (void *)m_pHeapEndAddress; 
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
	jrs_sizet cHeapNonIntrusive::GetMaxAllocationSize(void) const 
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
	jrs_sizet cHeapNonIntrusive::GetMinAllocationSize(void) const 
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
	jrs_sizet cHeapNonIntrusive::GetDefaultAlignment(void) const 
	{ 
		return m_uDefaultAlignment; 
	}

	//  Description:
	//		Gets the size of the page the Non Intrusive heap uses internally.  Default is 8k.
	//  See Also:
	//		GetSize
	//  Arguments:
	//		None
	//  Return Value:
	//      Size in bytes of the page size.  Default 8k.
	//  Summary:
	//      Gets the Non Intrusive Heap page size.
	jrs_sizet cHeapNonIntrusive::GetPageSize(void) const
	{
		return m_uPageSize;
	}

	//  Description:
	//		Gets the size of the debug header.  This is the size that contains the callstack and name information.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      Size in bytes of the debug header.
	//  Summary:
	//      Gets the size of the debug header.
	jrs_sizet cHeapNonIntrusive::GetDebugHeaderSize(void) const
	{
#ifndef MEMORYMANAGER_MINIMAL
		return m_uDebugHeaderSize;
#else
		return 0;
#endif
	}

	//  Description:
	//		Finds a slab based on the memory address.
	//  See Also:
	//		AllocateMemory, FreeMemory
	//  Arguments:
	//		pMemory - Memory address to find.
	//  Return Value:
	//      sSlap pointer if memory exists.  NULL otherwise.
	//  Summary:
	//      Finds a valid slab based on a memory address.
	cHeapNonIntrusive::sSlab *cHeapNonIntrusive::FindSlabFromMemory(jrs_i8 *pMemory)
	{
		// Find the slab it came from
		for(jrs_u32 i = 0; i < m_uNumSlabs; i++)
		{
			if(pMemory >= (jrs_i8 *)m_Slabs[i].pBase && pMemory < ((jrs_i8 *)m_Slabs[i].pBase + m_Slabs[i].uSize))
			{
				return &m_Slabs[i];
			}
		}

		return NULL;
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
	jrs_bool cHeapNonIntrusive::IsAllocatedFromThisHeap(void *pMemory) const
	{
		// Use the slab checker
		for(jrs_u32 i = 0; i < m_uNumSlabs; i++)
		{
			if(pMemory >= (jrs_i8 *)m_Slabs[i].pBase && pMemory < ((jrs_i8 *)m_Slabs[i].pBase + m_Slabs[i].uSize))
			{
				return true;
			}
		}

		return false;
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
	jrs_bool cHeapNonIntrusive::IsLocked(void) const 
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
	jrs_bool cHeapNonIntrusive::IsNullFreeEnabled(void) const 
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
	jrs_bool cHeapNonIntrusive::IsZeroAllocationEnabled(void) const 
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
	jrs_bool cHeapNonIntrusive::IsOutOfMemoryReturnEnabled(void) const
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
	void cHeapNonIntrusive::EnableLock(jrs_bool bEnableLock)
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
	void cHeapNonIntrusive::EnableNullFree(jrs_bool bEnableNullFree) 
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
	void cHeapNonIntrusive::EnableZeroAllocation(jrs_bool bEnableZeroAllocation) 
	{ 
		m_bAllowZeroSizeAllocations = bEnableZeroAllocation; 
	}
}