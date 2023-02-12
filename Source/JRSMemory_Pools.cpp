/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/

// Needed for string functions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <JRSCoreTypes.h>
#include <JRSMemory_Pools.h>

#include "JRSMemory_Internal.h"
#include "JRSMemory_ErrorCodes.h"

// For placement new
#include <new>

// Elephant namespace.  Using Elephant declared in JRSMemory.h.
namespace Elephant
{
	jrs_bool cMemoryManager::InternalCreatePoolBase(jrs_u32 uElementSize, jrs_u32 uMaxElements, const jrs_i8 *pPoolName, sPoolDetails *pDetails, cHeap *pHeap)
	{
		// Some basic checks
		if(uElementSize < sizeof(jrs_sizet) || uMaxElements < 1)
		{
			MemoryWarning(uElementSize >= sizeof(jrs_sizet), JRSMEMORYERROR_POOLCREATEELEMENTSIZE, "Size of Element is to small.  Minimum is sizeof(jrs_sizet).");
			MemoryWarning(uMaxElements >= 1, JRSMEMORYERROR_POOLCREATEELEMENTCOUNT, "Number of Elements is to low.  Minimum is 1");

			return FALSE;
		}

		// The name
		if(!pPoolName)
		{
			MemoryWarning(pPoolName, JRSMEMORYERROR_POOLCREATENAME, "Pool name is NULL.");
			return FALSE;
		}

		return TRUE;
	}

	//  Description:
	//      Creates a memory pool.  It will be intrusive and so could negatively affect performance depending on memory placement.
	//  See Also:
	//      DestroyPool
	//  Arguments:
	//      uElementSize - The size of the Element to be stored.  Minimum size is sizeof(jrs_sizet) and the pool may expand.  See cPool documentation.
	//      uMaxElements - Maximum number of elements to be stored.
	//		pDetails - Pointer to heap details.  NULL will just use the defaults.
	//		pHeap - Pointer to a heap with which to take the Pool memory from. NULL will call cMemoryManager::Malloc and obey allocation there.
	//  Return Value:
	//      Valid cPool.  NULL if there is a failure.
	//  Summary:
	//      Creates a standard memory pool.
	cPool *cMemoryManager::CreatePool(jrs_u32 uElementSize, jrs_u32 uMaxElements, const jrs_i8 *pPoolName, sPoolDetails *pDetails, cHeap *pHeap)
	{
		// Do the basic checks
		if(!InternalCreatePoolBase(uElementSize, uMaxElements, pPoolName, pDetails, pHeap))
			return NULL;

		// Allocate the memory for the cPool
		if(!pHeap)
		{
			pHeap = cMemoryManager::Get().GetDefaultHeap();
			if(!pHeap)
				return FALSE;
		}

		// Check for defaults
		sPoolDetails detailsdefault;
		if(!pDetails)
			pDetails = &detailsdefault;

		// Allocate the memory.
		cPool *pPool = NULL;
		pPool = (cPool *)pHeap->AllocateMemory(sizeof(cPool), 0, JRSMEMORYFLAG_POOL, "Pool Class");
		if(!pPool)
		{
			MemoryWarning(pPool, JRSMEMORYERROR_POOLCREATENOMEMORY, "Failed to allocate memory for Pool");
			return NULL;
		}

		// Create the pool.  Use placement new.  
		pPool = new(pPool) cPool(uElementSize, uMaxElements, pHeap, pPoolName, pDetails);

		// Attach the pool to the heap
		pHeap->AttachPool(pPool);
		cMemoryManager::Get().ContinuousLogging_Operation(cMemoryManager::eContLog_CreatePool, NULL, pPool, 0);

		// Complete
		return pPool;
	}

	//  Description:
	//      Creates a non intrusive memory pool.  With a non intrusive pool the main memory comes from one area and the main headers come from another.  This is 
	//		useful if the memory you want to store data in is particularly slow or not directly accessible.
	//  See Also:
	//      DestroyPool
	//  Arguments:
	//      uElementSize - The size of the Element to be stored.  Minimum size is sizeof(jrs_sizet) and the pool may expand.  See cPool documentation.
	//      uMaxElements - Maximum number of elements to be stored.
	//		pDetails - Pointer to heap details.  NULL will just use the defaults.
	//		pHeap - Pointer to a heap with which to take the Pool memory from. NULL will call cMemoryManager::Malloc and obey allocation there.
	//		pHeaderHeap - Pointer to a heap with which the headers come from.
	//  Return Value:
	//      Valid cPoolNonIntrusive.  NULL if there is a failure.
	//  Summary:
	//      Creates a non intrusive memory pool.
	cPoolNonIntrusive *cMemoryManager::CreatePoolNonIntrusive(jrs_u32 uElementSize, jrs_u32 uMaxElements, const jrs_i8 *pPoolName, void *pDataPointer, jrs_u64 uDataPointerSize, sPoolDetails *pDetails, cHeap *pHeap)
	{
		// Do the basic checks
		if(!InternalCreatePoolBase(uElementSize, uMaxElements, pPoolName, pDetails, pHeap))
			return NULL;

		// Allocate the memory for the cPool
		if(!pHeap)
		{
			pHeap = cMemoryManager::Get().GetDefaultHeap();
			if(!pHeap)
				return FALSE;
		}

		// Check for defaults
		sPoolDetails detailsdefault;
		if(!pDetails)
			pDetails = &detailsdefault;

		// Allocate the memory.sfd
		cPoolNonIntrusive *pPool = NULL;
		pPool = (cPoolNonIntrusive *)pHeap->AllocateMemory(sizeof(cPoolNonIntrusive), 0, JRSMEMORYFLAG_POOL, "Pool Class");
		if(!pPool)
		{
			MemoryWarning(pPool, JRSMEMORYERROR_POOLCREATENOMEMORY, "Failed to allocate memory for Pool");
			return NULL;
		}

		// Create the pool.  Use placement new.  
		pPool = new(pPool) cPoolNonIntrusive(pHeap, pPoolName);
		jrs_bool val = pPool->Create(uElementSize, uMaxElements, pHeap, pDataPointer, uDataPointerSize, pPoolName, pDetails);
		if(!val)
		{
			pHeap->FreeMemory(pPool);
			return NULL;
		}

		// Attach the pool to the heap
		pHeap->AttachPool(pPool);
		cMemoryManager::Get().ContinuousLogging_Operation(cMemoryManager::eContLog_CreatePool, NULL, pPool, 0);

		// Complete
		return pPool;
	}

	//  Description:
	//      Destroys any type of pool created by CreatePool.
	//  See Also:
	//      CreatePool
	//  Arguments:
	//      uElementSize - The size of the Element to be stored.  Minimum size is sizeof(jrs_sizet) and the pool may expand.  See cPool documentation.
	//      uMaxElements - Maximum number of elements to be stored.
	//		pDetails - Pointer to heap details.  NULL will just use the defaults.
	//		pHeap - Pointer to a heap with which to take the Pool memory from. NULL will call cMemoryManager::Malloc and obey allocation there.
	//  Return Value:
	//      None
	//  Summary:
	//      Destroys any type of pool created by CreatePool
	void cMemoryManager::DestroyPool(cPoolBase *pPool)
	{
		// Remove it from the heap
		MemoryWarning(pPool, JRSMEMORYERROR_POOLCREATENOMEMORY, "Pool cannot be NULL.");
		if(pPool)
		{
			cMemoryManager::Get().ContinuousLogging_Operation(cMemoryManager::eContLog_DestroyPool, NULL, pPool, 0);
			pPool->GetHeap()->RemovePool(pPool);
		}
	}

	//  Description:
	//      Attaches a pool to the Heap.  For cPools only.  Internal function.
	//  See Also:
	//      RemovePool
	//  Arguments:
	//      pPool - cPool to attach.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Attaches a pool to the Heap.
	void cHeap::AttachPool(cPoolBase *pPool)
	{
		// Simple linked list to add and remove.
		pPool->m_pNext = m_pAttachedPools;
		if(m_pAttachedPools)
			m_pAttachedPools->m_pPrev = pPool;

		m_pAttachedPools = pPool;
	}

	//  Description:
	//      Removes a pool from the Heap.  For cPools only.  Internal function.
	//  See Also:
	//      AttachPool
	//  Arguments:
	//      pPool - cPool to remove.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Removes a pool from the Heap.
	void cHeap::RemovePool(cPoolBase *pPool)
	{
		cPoolBase *pNext = pPool->m_pNext;
		cPoolBase *pPrev = pPool->m_pPrev;

		if (pNext)
		{
			// Make the next point to any previous
			pNext->m_pPrev = pPrev;
		}

		if (pPrev)
		{
			// Make the previous point to the next
			pPrev->m_pNext = pNext;
		}
		else
		{
			// Else if there is no previous just set it to the next
			m_pAttachedPools = pNext;
		}                
		cPool *pP = (cPool *)pPool;
		pP->Destroy();
		cMemoryManager::Get().Free(pPool, JRSMEMORYFLAG_POOL);
	}

	//  Description:
	//      Traverses all the pools in a heap iterator style.  
	//  See Also:
	//      AttachPool, RemovePool
	//  Arguments:
	//      pPool - NULL to get the first pool.  Otherwise the pool value previously returned to get the next.
	//  Return Value:
	//      A valid pool if there was a connected pool.  NULL otherwise.
	//  Summary:
	//      Removes a pool from the Heap.
	cPoolBase *cHeap::GetPool(cPoolBase *pPool)
	{
		if(!pPool)
			return m_pAttachedPools;

		return pPool->m_pNext;
	}

	//  Description:
	//      Checks if the memory allocation actually comes from a pool instead.
	//  See Also:
	//      
	//  Arguments:
	//      pMemory - Memory address to check.
	//  Return Value:
	//      TRUE if memory comes from a pool.  FALSE otherwise.
	//  Summary:
	//      Checks if the memory comes from a pool.
	jrs_bool cHeap::IsAllocatedFromAttachedPool(void *pMemory)
	{
		// Check the cPools
		cPoolBase *pPool = m_pAttachedPools;
		while(pPool)
		{
			cPool *pA = (cPool *)pPool;
			if(pA->IsAllocatedFromThisPool(pMemory))
				return TRUE;

			// Next
			pPool = pPool->m_pNext;
		}
		return FALSE;
	}

	//  Description:
	//      Returns the pool the memory pointer comes from.
	//  See Also:
	//      
	//  Arguments:
	//      pMemory - Memory address to check.
	//  Return Value:
	//      Pointer to the pBasePool. 
	//		NULL if not allocated from this pool.
	//  Summary:
	//      Returns the pool the memory pointer comes from.
	cPoolBase *cHeap::GetPoolFromAllocatedMemory(void *pMemory)
	{
		// Check the cPools
		cPoolBase *pPool = m_pAttachedPools;
		while(pPool)
		{
			cPool *pA = (cPool *)pPool;
			if(pA->IsAllocatedFromThisPool(pMemory))
				return pA;

			// Next
			pPool = pPool->m_pNext;
		}

		return NULL;
	}

	struct sMemPoolSentinel
	{
		jrs_u32 Sentinel[4];
	};

	struct sMemPoolTracking
	{
		jrs_i8 Name[MemoryManager_StringLength];
		jrs_sizet Callstack[8];
	};

	//  Description:
	//      Constructor for cPoolBase.  Provides the generic defaults.
	//  See Also:
	//      
	//  Arguments:
	//      pPoolMemory - Heap the pool is attached too.
	//		pName - Name of the pool.  31 char MAX.
	//  Return Value:
	//      None
	//  Summary:
	//      cPoolBase constructor.
	cPoolBase::cPoolBase(cHeap *pPoolMemory, const jrs_i8 *pName)
	{
		// name copied
		MemoryWarning(pName, JRSMEMORYERROR_HEAPNAME, "Name is invalid");
		MemoryWarning(strlen(pName) < 32, JRSMEMORYERROR_HEAPNAME, "Name is too long.");
		strcpy(m_Name, pName);
		m_pNext = m_pPrev = NULL;
		m_pAttachedHeap = pPoolMemory;

		m_bAllowDestructionWithAllocations = FALSE;
		m_bAllowNotEnoughSpaceReturn = FALSE;		
		m_bLocked = FALSE;
		m_bEnableErrors = TRUE;
		m_bErrorsAsWarnings = FALSE;

		m_uPoolID = cMemoryManager::Get().m_uPoolIdInfo++;
	}

	//  Description:
	//      Destructor for cPoolBase.  Leave empty.
	//  See Also:
	//      
	//  Arguments:
	//		None
	//  Return Value:
	//      None
	//  Summary:
	//      cPoolBase destructor.
	cPoolBase::~cPoolBase()
	{

	}

	//  Description:
	//      Returns the cHeap that the Pool is attached too.  This will always be valid.
	//  See Also:
	//      
	//  Arguments:
	//		None
	//  Return Value:
	//      Heap the pool is attached too. 
	//  Summary:
	//      Returns the Heap the pool is attached too.
	cHeap *cPoolBase::GetHeap(void) const
	{
		return m_pAttachedHeap;
	}

	//  Description:
	//      Returns the name of the Pool.
	//  See Also:
	//      
	//  Arguments:
	//		None
	//  Return Value:
	//      The name of the pool. 
	//  Summary:
	//      Returns the Pool name.
	const jrs_i8 *cPoolBase::GetName(void) const
	{
		return m_Name;
	}

	//  Description:
	//      Returns if the pool will allow allocations or not.
	//  See Also:
	//      
	//  Arguments:
	//		None
	//  Return Value:
	//      TRUE if the pool is locked.  FALSE if allocations can occur. 
	//  Summary:
	//      Returns if the pool will allow allocations or not.
	jrs_bool cPoolBase::IsLocked(void) const
	{
		return m_bLocked;
	}

	//  Description:
	//		Enables locking and unlocking of the pool to prevent or allow dynamic allocation.
	//  See Also:
	//		IsLocked
	//  Arguments:
	//		bEnableLock - TRUE to lock the heap.  FALSE otherwise.
	//  Return Value:
	//      None
	//  Summary:
	//      Enables locking and unlocking of the pool to prevent or allow dynamic allocation.
	void cPoolBase::EnableLock(jrs_bool bEnableLock)
	{
		m_bLocked = bEnableLock;
	}
	
	//  Description:
	//		Sets the sentinel values on an allocated block.  Private.
	//  See Also:
	//		
	//  Arguments:
	//		pStart - Start address of the pool allocation.
	//		pEnd - End address of the allocation.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Sets the allocated block Sentinels.
	void cPoolBase::SetAllocatedSentinels(jrs_u32 *pStart, jrs_u32 *pEnd)
	{
		pStart[0] = pStart[1] = pStart[2] = pStart[3] = MemoryManager_SentinelValueAllocatedBlock;
		pEnd[0] = pEnd[1] = pEnd[2] = pEnd[3] = MemoryManager_SentinelValueAllocatedBlock;
	}

	//  Description:
	//		Checks the sentinel values on an allocated block.  Private.
	//  See Also:
	//		
	//  Arguments:
	//		pStart - Start address of the pool allocation.
	//		pEnd - End address of the allocation.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Checks the sentinel values on an allocated block.
	void cPoolBase::CheckAllocatedSentinels(jrs_u32 *pStart, jrs_u32 *pEnd)
	{
		PoolWarning(pStart[0] == MemoryManager_SentinelValueAllocatedBlock, JRSMEMORYERROR_SENTINELALLOCCORRUPT, "Pool Allocated Sentinel has been corrupted at location 0x%p", &pStart[0]);
		PoolWarning(pStart[1] == MemoryManager_SentinelValueAllocatedBlock, JRSMEMORYERROR_SENTINELALLOCCORRUPT, "Pool Allocated Sentinel has been corrupted at location 0x%p", &pStart[1]);
		PoolWarning(pStart[2] == MemoryManager_SentinelValueAllocatedBlock, JRSMEMORYERROR_SENTINELALLOCCORRUPT, "Pool Allocated Sentinel has been corrupted at location 0x%p", &pStart[2]);
		PoolWarning(pStart[3] == MemoryManager_SentinelValueAllocatedBlock, JRSMEMORYERROR_SENTINELALLOCCORRUPT, "Pool Allocated Sentinel has been corrupted at location 0x%p", &pStart[3]);

		PoolWarning(pEnd[0] == MemoryManager_SentinelValueAllocatedBlock, JRSMEMORYERROR_SENTINELALLOCCORRUPT, "Pool Allocated Sentinel has been corrupted at location 0x%p", &pEnd[0]);
		PoolWarning(pEnd[1] == MemoryManager_SentinelValueAllocatedBlock, JRSMEMORYERROR_SENTINELALLOCCORRUPT, "Pool Allocated Sentinel has been corrupted at location 0x%p", &pEnd[1]);
		PoolWarning(pEnd[2] == MemoryManager_SentinelValueAllocatedBlock, JRSMEMORYERROR_SENTINELALLOCCORRUPT, "Pool Allocated Sentinel has been corrupted at location 0x%p", &pEnd[2]);
		PoolWarning(pEnd[3] == MemoryManager_SentinelValueAllocatedBlock, JRSMEMORYERROR_SENTINELALLOCCORRUPT, "Pool Allocated Sentinel has been corrupted at location 0x%p", &pEnd[3]);
	}

	//  Description:
	//		Sets the sentinel values on a free block.  Private.
	//  See Also:
	//		
	//  Arguments:
	//		pStart - Start address of the pool allocation.
	//		pEnd - End address of the allocation.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Sets the free block Sentinels.
	void cPoolBase::SetFreeSentinels(jrs_u32 *pStart, jrs_u32 *pEnd)
	{
		pStart[0] = pStart[1] = pStart[2] = pStart[3] = MemoryManager_SentinelValueFreeBlock;
		pEnd[0] = pEnd[1] = pEnd[2] = pEnd[3] = MemoryManager_SentinelValueFreeBlock;
	}

	//  Description:
	//		Checks the sentinel values on a free block.  Private.
	//  See Also:
	//		
	//  Arguments:
	//		pStart - Start address of the pool allocation.
	//		pEnd - End address of the allocation.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Checks the sentinel values on a free block.
	void cPoolBase::CheckFreeSentinels(jrs_u32 *pStart, jrs_u32 *pEnd)
	{
		PoolWarning(pStart[0] == MemoryManager_SentinelValueFreeBlock, JRSMEMORYERROR_SENTINELFREECORRUPT, "Pool Free Sentinel has been corrupted at location 0x%p", &pStart[0]);
		PoolWarning(pStart[1] == MemoryManager_SentinelValueFreeBlock, JRSMEMORYERROR_SENTINELFREECORRUPT, "Pool Free Sentinel has been corrupted at location 0x%p", &pStart[1]);
		PoolWarning(pStart[2] == MemoryManager_SentinelValueFreeBlock, JRSMEMORYERROR_SENTINELFREECORRUPT, "Pool Free Sentinel has been corrupted at location 0x%p", &pStart[2]);
		PoolWarning(pStart[3] == MemoryManager_SentinelValueFreeBlock, JRSMEMORYERROR_SENTINELFREECORRUPT, "Pool Free Sentinel has been corrupted at location 0x%p", &pStart[3]);

		PoolWarning(pEnd[0] == MemoryManager_SentinelValueFreeBlock, JRSMEMORYERROR_SENTINELFREECORRUPT, "Pool Free Sentinel has been corrupted at location 0x%p", &pEnd[0]);
		PoolWarning(pEnd[1] == MemoryManager_SentinelValueFreeBlock, JRSMEMORYERROR_SENTINELFREECORRUPT, "Pool Free Sentinel has been corrupted at location 0x%p", &pEnd[1]);
		PoolWarning(pEnd[2] == MemoryManager_SentinelValueFreeBlock, JRSMEMORYERROR_SENTINELFREECORRUPT, "Pool Free Sentinel has been corrupted at location 0x%p", &pEnd[2]);
		PoolWarning(pEnd[3] == MemoryManager_SentinelValueFreeBlock, JRSMEMORYERROR_SENTINELFREECORRUPT, "Pool Free Sentinel has been corrupted at location 0x%p", &pEnd[3]);
	}

	//  Description:
	//		Returns if error checking is enabled or not for the pool.  This is determined by the bEnableErrors flag of sPoolDetails when creating the heap.
	//  See Also:
	//		AreErrorsWarningsOnly, CreatePool
	//  Arguments:
	//		None
	//  Return Value:
	//      TRUE if errors are enabled.
	//		FALSE otherwise.
	//  Summary:
	//      Returns if error checking is enabled or not for the pool.
	jrs_bool cPoolBase::AreErrorsEnabled(void) const 
	{ 
		return m_bEnableErrors; 
	}				

	//  Description:
	//		Returns if all errors have been demoted to just warnings for the pool.  This is determined by the bErrorsAsWarnings flag of sPoolDetails when creating the heap.  If error checking
	//		is enabled this will be ignored internally.
	//  See Also:
	//		AreErrorsEnabled, CreatePool
	//  Arguments:
	//		None
	//  Return Value:
	//      TRUE if errors are demoted to warnings.
	//		FALSE otherwise.
	//  Summary:
	//      Returns if all errors have been demoted to just warnings for the pool.
	jrs_bool cPoolBase::AreErrorsWarningsOnly(void) const 
	{ 
		return m_bErrorsAsWarnings; 
	}

	//  Description:
	//		Constructor for intrusive pools.  Called internally.
	//  See Also:
	//		CreatePool
	//  Arguments:
	//		uElementSize - Size of the element to allocate each time.
	//		uMaxElements - Maximum number of elements that the pool can hold.
	//		pPoolMemory - The Heap that the pool memory gets allocated from.
	//		pName - The name of the pool.
	//		pDetails - Details pointer.  Can be null for default values.
	//  Return Value:
	//      None
	//  Summary:
	//      Constructor for intrusive pools.
	cPool::cPool(jrs_u32 uElementSize, jrs_u32 uMaxElements, cHeap *pPoolMemory, const jrs_i8 *pName, const sPoolDetails *pDetails) : cPoolBase(pPoolMemory, pName)
	{
		m_uMaxElements = uMaxElements;
		m_uAlignment = pDetails->uAlignment;
		m_bThreadSafe = pDetails->bThreadSafe;
		m_bEnableMemoryTracking = pDetails->bEnableMemoryTracking;
		m_bEnableSentinel = pDetails->bEnableSentinel;
		m_bEnableOverrun = (pDetails->pOverrunHeap) ? true : false;	
		m_pOverrunHeap = pDetails->pOverrunHeap;
		m_bAllowDestructionWithAllocations = pDetails->bAllowDestructionWithAllocations;
		m_bAllowNotEnoughSpaceReturn = pDetails->bAllowNotEnoughSpaceReturn;		
		m_bEnableErrors = pDetails->bEnableErrors;
		m_bErrorsAsWarnings = pDetails->bErrorsAsWarnings;

		// Minimum size must be at least size_t.  If the alignment is large we enlarge the size of the objects to match that.
		jrs_u32 minsize = uElementSize < sizeof(jrs_sizet) ? sizeof(jrs_sizet) : uElementSize;
		minsize = minsize < m_uAlignment ? m_uAlignment : minsize;

		jrs_u32 uElementPad = minsize;

		// Add data for any debug tracking
		m_uTrackingOffset = uElementPad;
		m_uStartSentinelOffset = uElementPad;
		m_uPointerOffset = 0;
		if(m_bEnableMemoryTracking)
		{
			minsize += sizeof(sMemPoolTracking);		// size for name and 8 deep callstack.
			m_uStartSentinelOffset = m_uTrackingOffset + sizeof(sMemPoolTracking);
		}

		if(m_bEnableSentinel)
		{
			minsize += (sizeof(sMemPoolSentinel) * 2);			// Sentinel checks.  4 u32s either side of the pool.
			m_uPointerOffset += sizeof(sMemPoolSentinel);
			m_uTrackingOffset += sizeof(sMemPoolSentinel);
			m_uStartSentinelOffset += (sizeof(sMemPoolSentinel));
		}

		// Adjust for sizet
		m_uPointerOffset /= sizeof(jrs_sizet);				// 2 offsets in 64bit/4 in 32bit
		m_uTrackingOffset /= sizeof(jrs_sizet);
		m_uStartSentinelOffset /= sizeof(jrs_sizet);

		// Final size
		m_uElementSize = minsize;

		// get the size
		m_uPoolSize = m_uElementSize * m_uMaxElements;

		// Allocate from the pool unless it is null, then just take it from the last memory heap.
		m_pBuffer = (jrs_sizet *)pPoolMemory->AllocateMemory(m_uPoolSize, pDetails->uBufferAlignment, JRSMEMORYFLAG_POOL, m_Name);
		if(!m_pBuffer)
		{
			PoolWarning(m_pBuffer, JRSMEMORYERROR_NOPOOLMEM, "Not enough memory to allocate the pool buffer.");
		}

		// Clear the memory read for allocations
#ifndef MEMORYMANAGER_MINIMAL
		// Less optimal version.  This covers the name and call stack if required.  Means we have a few extra instructions but in general
		// allows for easier use and debugging while not being a noticeable amount slower.
		jrs_sizet *pBuf = m_pFreePtr = m_pBuffer;
		for(jrs_u32 i = 0; i < m_uMaxElements; i++)
		{
			// Do name and callstack
			if(m_bEnableMemoryTracking)
			{
				StackTrace(&pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet))]);
				if(pName)
					strcpy((char *)&pBuf[m_uTrackingOffset], pName);
				else
					strcpy((char *)&pBuf[m_uTrackingOffset], "Unknown");
			}

			// Mark the sentinels
			if(m_bEnableSentinel)
			{
				SetFreeSentinels((jrs_u32 *)&pBuf[0], (jrs_u32 *)&pBuf[m_uStartSentinelOffset]);
			}

			pBuf += m_uElementSize / sizeof(jrs_sizet);
			m_pBuffer[((m_uElementSize / sizeof(jrs_sizet)) * i) + m_uPointerOffset] = (jrs_sizet)pBuf;		
		}

		// Set the last one to NULL
		jrs_u32 Offset = ((m_uElementSize / sizeof(jrs_sizet)) * (m_uMaxElements - 1));
		m_pBuffer[Offset + m_uPointerOffset ] = 0;
#else
		// Optimized version for full speed.
		jrs_sizet *pBuf = m_pFreePtr = m_pBuffer;
		for(jrs_u32 i = 0; i < m_uMaxElements - 1; i++)
		{
			pBuf += m_uElementSize / sizeof(jrs_sizet);
			m_pBuffer[(m_uElementSize / sizeof(jrs_sizet)) * i] = (jrs_sizet)pBuf;
		}
		m_pBuffer[(m_uElementSize / sizeof(jrs_sizet)) * (m_uMaxElements - 1)] = 0;
#endif
		m_uUsedElements = 0;
	}

	//  Description:
	//		Destructor for intrusive pools.  Called internally.
	//  See Also:
	//		CreatePool, DestroyPool
	//  Arguments:
	//		None
	//  Return Value:
	//      None
	//  Summary:
	//      Destructor for intrusive pools.
	cPool::~cPool()
	{
		// Does nothing
	}

	//  Description:
	//		Allocates a block of memory from the pool.
	//  See Also:
	//		FreeMemory
	//  Arguments:
	//		None
	//  Return Value:
	//      Valid memory address.  NULL otherwise.
	//  Summary:
	//      Allocates a block of memory from the pool.
	void *cPool::AllocateMemory(void)
	{
		return AllocateMemory(NULL);
	}

	//  Description:
	//		Allocates a block of memory from the pool but allows you to give a name to the memory address if tracking is enabled for the pool.
	//  See Also:
	//		FreeMemory
	//  Arguments:
	//		pName - Name of the allocation.  31 chars not including null terminator.
	//  Return Value:
	//      Valid memory address.  NULL otherwise.
	//  Summary:
	//      Allocates a block of memory from the pool but allows you to give a name to the memory address if tracking is enabled for the pool.
	void *cPool::AllocateMemory(const jrs_i8 *pName)
	{
		PoolWarning(!IsLocked(), JRSMEMORYERROR_LOCKED, "Cannot allocate as the pool is locked.");
		if(IsLocked())
			return NULL;

		// Allocating inplace pools is easy and quick.  We do need to check for threading and if we want to do that however.
		if(m_bThreadSafe)
			m_Mutex.Lock();

		// Take some memory out of the pool.
		if(!m_pFreePtr)
		{
			// Release the lock here - the heaps will deal with it.
			if(m_bThreadSafe)
				m_Mutex.Unlock();

			// We are out of memory.  Do we allocate or do we free
			if(m_bEnableOverrun)
			{
				// Warn just the once
				if(m_bEnableOverrun == 1)
				{
					cMemoryManager::Get().DebugOutput("Memory Pool: %s has overrun.  Using Heap %s.", m_Name, m_pOverrunHeap->GetName());
					m_bEnableOverrun = 2;		// Yes, I know this shouldn't be set to this but JRS defines this as a char so we use it as such.
				}

				return m_pOverrunHeap->AllocateMemory(m_uElementSize, 0, JRSMEMORYFLAG_POOLMEM, (jrs_i8 *)pName);
			}
			else if(m_bAllowNotEnoughSpaceReturn)
				return NULL;

			PoolWarning(NULL, JRSMEMORYERROR_OUTOFSPACE, "Pool has run out of space and there is no overrun buffer.");
			return NULL;
		}

		// We have memory, time to take it from the list
#ifndef MEMORYMANAGER_MINIMAL
		// Some extra bits.  Still quick but not as fast
		jrs_sizet *pMemory = m_pFreePtr;

		// Check/Mark the sentinels
		if(m_bEnableSentinel)
		{
			CheckFreeSentinels((jrs_u32 *)&pMemory[0], (jrs_u32 *)&pMemory[m_uStartSentinelOffset]);
			SetAllocatedSentinels((jrs_u32 *)&pMemory[0], (jrs_u32 *)&pMemory[m_uStartSentinelOffset]);
		}

		// Do name and callstack
		if(m_bEnableMemoryTracking)
		{
 			StackTrace(&pMemory[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet))]);
			if(pName)
				strcpy((char *)&pMemory[m_uTrackingOffset], pName);
			else
				strcpy((char *)&pMemory[m_uTrackingOffset], "Unknown");
		}

		// Get the next pointer
		m_pFreePtr = (jrs_sizet *)(pMemory[m_uPointerOffset]);
		
		// Move the address on to the next
		pMemory = &pMemory[m_uPointerOffset];
#else
		// Very quick
		void *pMemory = m_pFreePtr;
		m_pFreePtr = (jrs_sizet *)(*m_pFreePtr);
#endif

		// Increase the count
		m_uUsedElements++;

		// unlock the memory
		if(m_bThreadSafe)
			m_Mutex.Unlock();

		// Log it
#ifndef MEMORYMANAGER_MINIMAL
		if(GetHeap()->IsLoggingEnabled())
			cMemoryManager::Get().ContinuousLogging_PoolOperation(cMemoryManager::eContLog_AllocatePool, this, pMemory, 0); 
#endif

		return pMemory;
	}

	//  Description:
	//		Frees a block of memory from the pool that was allocated with AllocateMemory.  Allows a name to specified if memory tracking for the pool is enabled.
	//  See Also:
	//		AllocateMemory
	//  Arguments:
	//		pMemory - Memory address previously allocated with AllocateMemory.
	//		pName - Name of the allocation.  31 chars not including null terminator.
	//  Return Value:
	//      None
	//  Summary:
	//      Frees a block of memory from the pool that was allocated with AllocateMemory.
	void cPool::FreeMemory(void *pMemory, const jrs_i8 *pName)
	{
		// not if there is a lock
		PoolWarning(!IsLocked(), JRSMEMORYERROR_LOCKED, "Cannot allocate as the pool is locked.");
		if(IsLocked())
			return;

		// Check the memory came from the overrun heap or not.
		if(m_bEnableOverrun)
		{
			// We dont do this check thread safe.  If the memory has already been freed and comes from the heap that
			// will always be thread safe.  These values do no change so it wont be a problem.

			// Check it came from the memory or not.
			if(pMemory < m_pBuffer || pMemory >= ((jrs_i8 *)m_pBuffer + m_uPoolSize))
			{
				// It did so free it from the heap.
				if(m_pOverrunHeap)
				{
					m_pOverrunHeap->FreeMemory(pMemory, JRSMEMORYFLAG_POOLMEM, pName);
					return;
				}

				// Allow fall through
			}
		}

#ifndef MEMORYMANAGER_MINIMAL
		// Check its valid
		if(!IsAllocatedFromThisPool(pMemory))
		{
			// Freed so exit.
			PoolWarning(NULL, JRSMEMORYERROR_ADDRESSNOTFROMPOOL, "Memory address is not from this pool (%s)", GetName());
			return;
		}
#endif
		// Free it from the pool quickly.
		if(m_bThreadSafe)
			m_Mutex.Lock();

		// Simple as getting the pointer and putting it back into the list
#ifndef MEMORYMANAGER_MINIMAL
		jrs_sizet *pBuf = (jrs_sizet *)pMemory - m_uPointerOffset;

		// Check/Mark the sentinels
		if(m_bEnableSentinel)
		{
			CheckAllocatedSentinels((jrs_u32 *)&pBuf[0], (jrs_u32 *)&pBuf[m_uStartSentinelOffset]);
			SetFreeSentinels((jrs_u32 *)&pBuf[0], (jrs_u32 *)&pBuf[m_uStartSentinelOffset]);
		}

		// Do name and callstack
		if(m_bEnableMemoryTracking)
		{
			StackTrace(&pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet))]);
			if(pName)
				strcpy((char *)&pBuf[m_uTrackingOffset], pName);
			else
				strcpy((char *)&pBuf[m_uTrackingOffset], "Unknown");
		}

		// Revert the pointer
		pBuf[m_uPointerOffset] = (jrs_sizet)m_pFreePtr;
		m_pFreePtr = (jrs_sizet *)pBuf;
#else
		jrs_sizet *pBuf = (jrs_sizet *)pMemory;
		*pBuf = (jrs_sizet)m_pFreePtr;
		m_pFreePtr = (jrs_sizet *)pMemory;
#endif

		// Decrease the count
		m_uUsedElements--;

		// Unlock
		if(m_bThreadSafe)
			m_Mutex.Unlock();

		// Log it
#ifndef MEMORYMANAGER_MINIMAL
		if(GetHeap()->IsLoggingEnabled())
			cMemoryManager::Get().ContinuousLogging_PoolOperation(cMemoryManager::eContLog_FreePool, this, pMemory, 0); 
#endif
	}

	//  Description:
	//		Gets the number of allocations currently allocated.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      Number of currently allocated elements in the pool.
	//  Summary:
	//      Gets the number of allocations currently allocated.
	jrs_u32 cPool::GetNumberOfAllocations(void) const
	{
		return m_uUsedElements;
	}

	//  Description:
	//		Gets the size of each allocation the pool returns.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      The size of each pool allocation.
	//  Summary:
	//      Gets the size of each allocation the pool returns.
	jrs_u32 cPool::GetAllocationSize(void) const
	{
		return m_uElementSize;
	}

// 	jrs_u32 cPool::GetNumberOfAllocationsMaximum(void) const
// 	{
// 		return 0;
// 	}

	//  Description:
	//		Checks if the memory allocated was allocated from within this pool.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      TRUE if allocated by the pool. FALSE otherwise.
	//  Summary:
	//      Checks if the memory allocated was allocated from within this pool.
	jrs_bool cPool::IsAllocatedFromThisPool(void *pMemory) const
	{
		if(pMemory >= m_pBuffer && pMemory < (jrs_i8 *)m_pBuffer + m_uPoolSize)
			return TRUE;

		return FALSE;
	}

	//  Description:
	//		Returns the size of the pool in bytes.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      Size of the pool in bytes.
	//  Summary:
	//      Returns the size of the pool in bytes.
	jrs_sizet cPool::GetSize(void) const 
	{ 
		return m_uPoolSize; 
	}

	//  Description:
	//		Returns the main memory address of the pool.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      Main memory address the pool allocates into.
	//  Summary:
	//      Returns the main memory address of the pool.
	void *cPool::GetAddress(void)
	{
		return m_pBuffer;
	}

	//  Description:
	//		Does a report on the pool (statistics and allocations in memory order) to the user TTY callback.  If you want to report these to a file
	//		include full path and file name to the function.  
	//  See Also:
	//		ReportStatistics, ReportAllocationsMemoryOrder
	//  Arguments:
	//      pLogToFile - Full path and file name null terminated string. NULL if you do not wish to generate a file.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Reports on a pool and can generate an Overview file for Goldfish.
	void cPool::ReportAll(const jrs_i8 *pLogToFile)
	{
		ReportStatistics();
		ReportAllocationsMemoryOrder(pLogToFile);
	}

	//  Description:
	//		Reports basic statistics about the pool to the user TTY callback. Use this to get quick information on 
	//		when need that is more detailed that simple heap memory real time calls.
	//  See Also:
	//		ReportAll, ReportAllocationsMemoryOrder
	//  Arguments:
	//		None
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Reports basic statistics about the pool to the user TTY callback. 
	void cPool::ReportStatistics(void)
	{
		// Incase we need thread safety
		if(m_bThreadSafe)
			m_Mutex.Lock();

		// Logging
		cMemoryManager::DebugOutput("---------------------------------------------------------------------------------------------");
		cMemoryManager::DebugOutput("Logging Pool: %s", GetName());
		cMemoryManager::DebugOutput("Pool Start Address: 0x%016x", m_pBuffer);
		cMemoryManager::DebugOutput("Pool End Address: 0x%016x", (jrs_i8 *)m_pBuffer + m_uPoolSize);
		cMemoryManager::DebugOutput("Pool Size: %dk (bytes %d (0x%x))", m_uPoolSize >> 10, m_uPoolSize, m_uPoolSize);
		cMemoryManager::DebugOutput("Max Elements: %d (Element size %d)", m_uMaxElements, m_uElementSize);
		cMemoryManager::DebugOutput("Used Allocations: %d (%.2f%%)", m_uUsedElements, ((jrs_f32)m_uUsedElements / (jrs_f32)m_uMaxElements) * 100.0f);
		cMemoryManager::DebugOutput("Using Name and Callstack: %s", m_bEnableMemoryTracking ? "Yes" : "No");
		cMemoryManager::DebugOutput("Using Sentinel Checking: %s", m_bEnableSentinel ? "Yes" : "No");
		if(m_bEnableOverrun)
			cMemoryManager::DebugOutput("Using Overrun Heap: %s", m_pOverrunHeap->GetName());
		else
			cMemoryManager::DebugOutput("No Overrun Allowed");
		cMemoryManager::DebugOutput("---------------------------------------------------------------------------------------------");

		// Incase we need thread safety
		if(m_bThreadSafe)
			m_Mutex.Unlock();
	}

	//  Description:
	//		Reports on all the allocations in the pool.  This function is like ReportAll but doesn't output the statistics.  Calling
	//		this function with a valid file name will create an Overview file for use in Goldfish.
	//  See Also:
	//		ReportAll, ReportStatistics
	//  Arguments:
	//		pLogToFile - Full path and file name null terminated string. NULL if you do not wish to generate a file.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Reports all allocations from the pool to the user TTY callback and/or a file.
	void cPool::ReportAllocationsMemoryOrder(const jrs_i8 *pLogToFile, jrs_bool bLogEachAllocation)
	{
		// Incase we need thread safety
		if(m_bThreadSafe)
			m_Mutex.Lock();

		// Because pools have no memory header its much harder to report the allocations correctly as we dont know which one
		// is allocated and not allocated without searching the list which may or may not be used.  This means this function could end up being
		// quite time consuming.  Any marker we put on the area that points to the next is irrelevant because it could actually fall within
		// the memory area due to virtual address and such.  So we do it the safe way and traverse multiple times.
		cMemoryManager::DebugOutput("---------------------------------------------------------------------------------------------");
		if(m_bEnableMemoryTracking)
			cMemoryManager::DebugOutput("Pool       (Text                            ) - Address    (HeaderAddr)");
		else
			cMemoryManager::DebugOutput("Pool       - Address");

		// Log the header
		cMemoryManager::DebugOutputFile(pLogToFile, g_ReportHeapCreate, "_PoolHeadMarker_; %s; %u; %u; %u; %u; %u; %u; 0; 0; %u; 32; %u; %u", m_Name, m_uPoolSize, 0, m_uMaxElements, m_uElementSize, m_uUsedElements, 0, 0, 0, 0);	

		// Log each allocation
		if(bLogEachAllocation)
		{
			jrs_sizet *pBuf = m_pBuffer;
			while((jrs_i8 *)pBuf < ((jrs_i8 *)m_pBuffer + m_uPoolSize))
			{
				// Check if the pBuf address is in the allocated list
				jrs_bool bAlloc = TRUE;
				jrs_sizet *pAlloc = m_pFreePtr;
				while(pAlloc)
				{
					// If it matches then is a free block.
					if(pAlloc == pBuf)
					{
						bAlloc = FALSE;
						break;
					}

					// No match yet, continue.
					pAlloc = (jrs_sizet *)pAlloc[m_uPointerOffset];
				}

				// CSV Main block		 = Marker (Allocation/Free), PoolName, Address, Size, CallStack4, CS3, CS3, CS2, CS1, Text (if one), (line if one)

				// Was it allocated or not?
				if(bAlloc)
				{
					if(m_bEnableMemoryTracking)
					{
						cMemoryManager::DebugOutput("Allocation (%-32s) - 0x%016x (0x%016x)", &pBuf[m_uTrackingOffset], &pBuf[m_uPointerOffset], pBuf);
						cMemoryManager::DebugOutputFile(pLogToFile, g_ReportHeapCreate, "_PoolAlloc_; %u; %u; %s; %u; %u; %u; %u; %u; %u; 0; 0; 0; 0", 
							&pBuf[m_uPointerOffset], m_uElementSize, &pBuf[m_uTrackingOffset], 
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (0 * sizeof(jrs_sizet))]), 
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (1 * sizeof(jrs_sizet))]),
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (2 * sizeof(jrs_sizet))]),
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (3 * sizeof(jrs_sizet))]),
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (4 * sizeof(jrs_sizet))]),
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (5 * sizeof(jrs_sizet))]),
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (6 * sizeof(jrs_sizet))])); 
					}
					else
					{
						cMemoryManager::DebugOutput("Allocation - 0x%016x", pBuf);
						cMemoryManager::DebugOutputFile(pLogToFile, g_ReportHeapCreate, "_PoolAlloc_; %u; %u; %s; %u; %u; %u; %u; %u; %u; 0; 0; 0; 0", 
							&pBuf[m_uPointerOffset], m_uElementSize, "Unknown", 0, 0, 0, 0, 0, 0, 0); 
					}
				}
				else
				{
					if(m_bEnableMemoryTracking)
					{
						cMemoryManager::DebugOutput("Free       (%-32s) - 0x%016x (0x%016x)", &pBuf[m_uTrackingOffset], &pBuf[m_uPointerOffset], pBuf);
						cMemoryManager::DebugOutputFile(pLogToFile, g_ReportHeapCreate, "_PoolFree_; %u; %u; %s; %u; %u; %u; %u; %u; %u; 0; 0; 0; 0", 
							&pBuf[m_uPointerOffset], m_uElementSize, &pBuf[m_uTrackingOffset], 
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (0 * sizeof(jrs_sizet))]), 
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (1 * sizeof(jrs_sizet))]),
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (2 * sizeof(jrs_sizet))]),
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (3 * sizeof(jrs_sizet))]),
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (4 * sizeof(jrs_sizet))]),
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (5 * sizeof(jrs_sizet))]),
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (6 * sizeof(jrs_sizet))])); 

					}
					else
					{
						cMemoryManager::DebugOutput("Free       - 0x%016x", pBuf);
						cMemoryManager::DebugOutputFile(pLogToFile, g_ReportHeapCreate, "_PoolFree_; %u; %u; %s; %u; %u; %u; %u; %u; %u; 0; 0; 0; 0", 
							&pBuf[m_uPointerOffset], m_uElementSize, "Unknown", 0, 0, 0, 0, 0, 0, 0); 
					}
			
				}

				// Next pointer 
				pBuf += m_uElementSize / sizeof(jrs_sizet);
			}
		}
		else
		{
			cMemoryManager::DebugOutput("Individual Element logging is disabled for speed");
		}

		// Close output
		cMemoryManager::DebugOutput("---------------------------------------------------------------------------------------------");

		// Incase we need thread safety
		if(m_bThreadSafe)
			m_Mutex.Unlock();
	}

	//  Description:
	//		Destroys the pool and any internals that the pool may have acquired.  Private function.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Destroys the pool and any internals that the pool may have acquired.
	void cPool::Destroy(void)
	{
		// No destruction with valid allocations?
		if(!m_bAllowDestructionWithAllocations && m_uUsedElements)
		{
			PoolWarning(m_bAllowDestructionWithAllocations && m_uUsedElements, JRSMEMORYERROR_VALIDALLOCATIONS, "Pool still has valid allocations.");
		}

		// A heap cant free this with out saying 'it comes from pool'.  We clear that error here by just setting it to null.
		void *pPoolBuffer = m_pBuffer;
		m_pBuffer = NULL;

		// Free the main pool off.
		cMemoryManager::Get().Free(pPoolBuffer, JRSMEMORYFLAG_POOL, "Pool Free");
	}
	
	// Non intrusive pool
	//  Description:
	//		Constructor for intrusive pools.  Called internally.
	//  See Also:
	//		CreatePool
	//  Arguments:
	//		uElementSize - Size of the element to allocate each time.
	//		uMaxElements - Maximum number of elements that the pool can hold.
	//		pPoolMemory - The Heap that the pool memory gets allocated from.
	//		pName - The name of the pool.
	//		pDetails - Details pointer.  Can be null for default values.
	//  Return Value:
	//      None
	//  Summary:
	//      Constructor for intrusive pools.
	cPoolNonIntrusive::cPoolNonIntrusive(cHeap *pPoolMemory, const jrs_i8 *pName) : cPoolBase(pPoolMemory, pName)
	{
		// Empty
	}

	//  Description:
	//		Destructor for intrusive pools.  Called internally.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      None
	//  Summary:
	//      Destructor for intrusive pools.
	cPoolNonIntrusive::~cPoolNonIntrusive()
	{
		// Empty
	}

	//  Description:
	//		Allocates a block of memory from the pool.
	//  See Also:
	//		FreeMemory
	//  Arguments:
	//		None
	//  Return Value:
	//      Valid memory address.  NULL otherwise.
	//  Summary:
	//      Allocates a block of memory from the pool.
	void *cPoolNonIntrusive::AllocateMemory(void)
	{
		return AllocateMemory(NULL);
	}

	//  Description:
	//		Allocates a block of memory from the pool but allows you to give a name to the memory address if tracking is enabled for the pool.
	//  See Also:
	//		FreeMemory
	//  Arguments:
	//		pName - Name of the allocation.  31 chars not including null terminator.
	//  Return Value:
	//      Valid memory address.  NULL otherwise.
	//  Summary:
	//      Allocates a block of memory from the pool but allows you to give a name to the memory address if tracking is enabled for the pool.
	void *cPoolNonIntrusive::AllocateMemory(const jrs_i8 *pName)
	{
		PoolWarning(!IsLocked(), JRSMEMORYERROR_LOCKED, "Cannot allocate as the pool is locked.");
		if(IsLocked())
			return NULL;

		// Allocating inplace pools is easy and quick.  We do need to check for threading and if we want to do that however.
		if(m_bThreadSafe)
			m_Mutex.Lock();

		// Check if we can continue to allocate
		if(!m_pFreePtr)
		{
			PoolWarning(m_uUsedElements < m_uMaxElements, JRSMEMORYERROR_OUTOFSPACE, "Pool cannot allocate any more elements.");
			if(m_bThreadSafe)
				m_Mutex.Unlock();
			return NULL;
		}

		// We have memory, time to take it from the list
		jrs_i8 *pOutMemory = (jrs_i8 *)m_pDataBuffer + ((((jrs_sizet)((jrs_i8 *)m_pFreePtr - (jrs_i8 *)m_pBuffer)) / m_uHeaderSize) * m_uElementSize);
#ifndef MEMORYMANAGER_MINIMAL
		jrs_sizet *pMemory = m_pFreePtr;

		// Some extra bits.  Still quick but not as fast
		
		// Do name and callstack
		if(m_bEnableMemoryTracking)
		{
			StackTrace(&pMemory[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet))]);
			if(pName)
				strcpy((char *)&pMemory[m_uTrackingOffset], pName);
			else
				strcpy((char *)&pMemory[m_uTrackingOffset], "Unknown");
		}
#endif
		m_pFreePtr = (jrs_sizet *)(*m_pFreePtr);

		// Increase the count
		m_uUsedElements++;

		// unlock the memory
		if(m_bThreadSafe)
			m_Mutex.Unlock();

		// Report it
#ifndef MEMORYMANAGER_MINIMAL
		if(GetHeap()->IsLoggingEnabled())
			cMemoryManager::Get().ContinuousLogging_PoolOperation(cMemoryManager::eContLog_AllocatePool, this, pMemory, 0); 
#endif

		return pOutMemory;
	}

	//  Description:
	//		Frees a block of memory from the pool that was allocated with AllocateMemory.  Allows a name to specified if memory tracking for the pool is enabled.
	//  See Also:
	//		AllocateMemory
	//  Arguments:
	//		pMemory - Memory address previously allocated with AllocateMemory.
	//		pName - Name of the allocation.  31 chars not including null terminator.
	//  Return Value:
	//      None
	//  Summary:
	//      Frees a block of memory from the pool that was allocated with AllocateMemory.
	void cPoolNonIntrusive::FreeMemory(void *pMemory, const jrs_i8 *pName)
	{
		// not if there is a lock
		PoolWarning(!IsLocked(), JRSMEMORYERROR_LOCKED, "Cannot allocate as the pool is locked.");
		if(IsLocked())
			return;

#ifndef MEMORYMANAGER_MINIMAL
		// Check its valid
		if(!IsAllocatedFromThisPool(pMemory))
		{
			// Freed so exit.
			PoolWarning(NULL, JRSMEMORYERROR_ADDRESSNOTFROMPOOL, "Memory address is not from this pool (%s)", GetName());
			return;
		}
#endif
		// Threading
		if(m_bThreadSafe)
			m_Mutex.Lock();

		// Simple as getting the pointer and putting it back into the list but first convert it to the pointer structure
		jrs_sizet uOffset = ((jrs_sizet)((jrs_i8 *)pMemory - (jrs_i8 *)m_pDataBuffer) / m_uElementSize);
		jrs_sizet *pBuf = &m_pBuffer[uOffset * (m_uHeaderSize / sizeof(jrs_sizet))];
#ifndef MEMORYMANAGER_MINIMAL
		// Do name and callstack
		if(m_bEnableMemoryTracking)
		{
			StackTrace(&pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet))]);
			if(pName)
				strcpy((char *)&pBuf[m_uTrackingOffset], pName);
			else
				strcpy((char *)&pBuf[m_uTrackingOffset], "Unknown");
		}
#endif
		*pBuf = (jrs_sizet)m_pFreePtr;
		m_pFreePtr = (jrs_sizet *)pBuf;

		// Decrease the count
		m_uUsedElements--;

		// Unlock
		if(m_bThreadSafe)
			m_Mutex.Unlock();

		// Free pool information
#ifndef MEMORYMANAGER_MINIMAL
		if(GetHeap()->IsLoggingEnabled())
			cMemoryManager::Get().ContinuousLogging_PoolOperation(cMemoryManager::eContLog_AllocatePool, this, pMemory, 0); 
#endif
	}

	//  Description:
	//		Gets the number of allocations currently allocated.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      Number of currently allocated elements in the pool.
	//  Summary:
	//      Gets the number of allocations currently allocated.
	jrs_u32 cPoolNonIntrusive::GetNumberOfAllocations(void) const
	{
		return m_uUsedElements;
	}

	//  Description:
	//		Returns the size of the pool in bytes.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      Size of the pool in bytes.
	//  Summary:
	//      Returns the size of the pool in bytes.
	jrs_sizet cPoolNonIntrusive::GetSize(void) const 
	{ 
		return m_uPoolSize; 
	}

	//  Description:
	//		Returns the main memory address of the pool.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      Main memory address the pool allocates into.
	//  Summary:
	//      Returns the main memory address of the pool.
	void *cPoolNonIntrusive::GetAddress(void)
	{
		return m_pBuffer;
	}

	//  Description:
	//		Checks if the memory allocated was allocated from within this pool.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      TRUE if allocated by the pool. FALSE otherwise.
	//  Summary:
	//      Checks if the memory allocated was allocated from within this pool.
	jrs_bool cPoolNonIntrusive::IsAllocatedFromThisPool(void *pMemory) const
	{
		if(pMemory >= m_pDataBuffer && pMemory < (jrs_i8 *)m_pDataBuffer + m_uPoolSize)
			return TRUE;

		return FALSE;
	}

	//  Description:
	//		Gets the size of each allocation the pool returns.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      The size of each pool allocation.
	//  Summary:
	//      Gets the size of each allocation the pool returns.
	jrs_u32 cPoolNonIntrusive::GetAllocationSize(void) const
	{
		return m_uElementSize;
	}

	//  Description:
	//		Does a report on the pool (statistics and allocations in memory order) to the user TTY callback.  If you want to report these to a file
	//		include full path and file name to the function.  
	//  See Also:
	//		ReportStatistics, ReportAllocationsMemoryOrder
	//  Arguments:
	//      pLogToFile - Full path and file name null terminated string. NULL if you do not wish to generate a file.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Reports on a pool and can generate an Overview file for Goldfish.
	void cPoolNonIntrusive::ReportAll(const jrs_i8 *pLogToFile)
	{
		ReportStatistics();
		ReportAllocationsMemoryOrder(pLogToFile);
	}

	//  Description:
	//		Reports basic statistics about the pool to the user TTY callback. Use this to get quick information on 
	//		when need that is more detailed that simple heap memory real time calls.
	//  See Also:
	//		ReportAll, ReportAllocationsMemoryOrder
	//  Arguments:
	//		None
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Reports basic statistics about the pool to the user TTY callback. 
	void cPoolNonIntrusive::ReportStatistics(void)
	{
		// Incase we need thread safety
		if(m_bThreadSafe)
			m_Mutex.Lock();

		// Logging
		cMemoryManager::DebugOutput("---------------------------------------------------------------------------------------------");
		cMemoryManager::DebugOutput("Logging Pool: %s", GetName());
		cMemoryManager::DebugOutput("Pool Start Address: 0x%016x", m_pDataBuffer);
		cMemoryManager::DebugOutput("Pool End Address: 0x%016x", (jrs_i8 *)m_pDataBuffer + m_uPoolSize);
		cMemoryManager::DebugOutput("Pool Header Address: 0x%016x", (jrs_i8 *)m_pBuffer);
		cMemoryManager::DebugOutput("Pool Size: %dk (bytes %d (0x%x))", m_uPoolSize >> 10, m_uPoolSize, m_uPoolSize);
		cMemoryManager::DebugOutput("Max Elements: %d (Element size %d)", m_uMaxElements, m_uElementSize);
		cMemoryManager::DebugOutput("Used Allocations: %d (%.2f%%)", m_uUsedElements, ((jrs_f32)m_uUsedElements / (jrs_f32)m_uMaxElements) * 100.0f);
		cMemoryManager::DebugOutput("Using Name and Callstack: %s", m_bEnableMemoryTracking ? "Yes" : "No");
		cMemoryManager::DebugOutput("Using Sentinel Checking: %s", "Disabled for Non Intrusive Pools");
		cMemoryManager::DebugOutput("No Overrun Allowed");
		cMemoryManager::DebugOutput("---------------------------------------------------------------------------------------------");

		// Incase we need thread safety
		if(m_bThreadSafe)
			m_Mutex.Unlock();
	}

	//  Description:
	//		Reports on all the allocations in the pool.  This function is like ReportAll but doesn't output the statistics.  Calling
	//		this function with a valid file name will create an Overview file for use in Goldfish.
	//
	//		Logging each allocation to file can be extremely slow.  This is because it is not possible to tell if that allocation is free or not without traversing the
	//		whole list.  Goldfish doesn't need to know about this to function correctly but for debugging it may be needed.
	//  See Also:
	//		ReportAll, ReportStatistics
	//  Arguments:
	//		pLogToFile - Full path and file name null terminated string. NULL if you do not wish to generate a file.
	//		bLogEachAllocation - TRUE to log each allocation.  FALSE by default.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Reports all allocations from the pool to the user TTY callback and/or a file.
	void cPoolNonIntrusive::ReportAllocationsMemoryOrder(const jrs_i8 *pLogToFile, jrs_bool bLogEachAllocation)
	{
		// Incase we need thread safety
		if(m_bThreadSafe)
			m_Mutex.Lock();

		// Because pools have no memory header its much harder to report the allocations correctly as we dont know which one
		// is allocated and not allocated without searching the list which may or may not be used.  This means this function could end up being
		// quite time consuming.  Any marker we put on the area that points to the next is irrelevant because it could actually fall within
		// the memory area due to virtual address and such.  So we do it the safe way and traverse multiple times.  This can also be quite slow.
		cMemoryManager::DebugOutput("---------------------------------------------------------------------------------------------");
		if(m_bEnableMemoryTracking)
			cMemoryManager::DebugOutput("Pool       (Text                            ) - Address    (HeaderAddr)");
		else
			cMemoryManager::DebugOutput("Pool       - Address");

		// Log the header
		cMemoryManager::DebugOutputFile(pLogToFile, g_ReportHeapCreate, "_PoolHeadMarker_; %s; %u; %u; %u; %u; %u; %u; 0; 0; %u; 32; %u; %u", m_Name, m_uPoolSize, 1, m_uMaxElements, m_uElementSize, m_uUsedElements, m_pDataBuffer, 0, 0, 0);	

		// Log each allocation
		if(bLogEachAllocation)
		{
			jrs_sizet *pBuf = m_pBuffer;
			while((jrs_i8 *)pBuf < ((jrs_i8 *)m_pBuffer + (m_uHeaderSize * m_uMaxElements)))
			{
				// Check if the pBuf address is in the allocated list
				jrs_bool bAlloc = TRUE;
				jrs_sizet *pAlloc = m_pFreePtr;
				while(pAlloc)
				{
					// If it matches then is a free block.
					if(pAlloc == pBuf)
					{
						bAlloc = FALSE;
						break;
					}

					// No match yet, continue.
					pAlloc = (jrs_sizet *)pAlloc[0];
				}

				// Was it allocated or not?
				if(bAlloc)
				{
					if(m_bEnableMemoryTracking)
					{
						cMemoryManager::DebugOutput("Allocation (%-32s) - 0x%016x (0x%016x)", &pBuf[m_uTrackingOffset], &pBuf[0], pBuf);
						cMemoryManager::DebugOutputFile(pLogToFile, g_ReportHeapCreate, "_PoolAlloc_; %u; %u; %s; %u; %u; %u; %u; %u; %u; 0; 0; 0; 0", 
							&pBuf[0], m_uElementSize, &pBuf[m_uTrackingOffset], 
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (0 * sizeof(jrs_sizet))]), 
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (1 * sizeof(jrs_sizet))]),
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (2 * sizeof(jrs_sizet))]),
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (3 * sizeof(jrs_sizet))]),
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (4 * sizeof(jrs_sizet))]),
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (5 * sizeof(jrs_sizet))]),
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (6 * sizeof(jrs_sizet))])); 
					}
					else
					{
						cMemoryManager::DebugOutput("Allocation - 0x%016x", pBuf);
						cMemoryManager::DebugOutputFile(pLogToFile, g_ReportHeapCreate, "_PoolAlloc_; %u; %u; %s; %u; %u; %u; %u; %u; %u; 0; 0; 0; 0", 
							&pBuf[0], m_uElementSize, "Unknown", 0, 0, 0, 0, 0, 0, 0); 
					}
				}
				else
				{
					if(m_bEnableMemoryTracking)
					{
						cMemoryManager::DebugOutput("Free       (%-32s) - 0x%016x (0x%016x)", &pBuf[m_uTrackingOffset], &pBuf[0], pBuf);
						cMemoryManager::DebugOutputFile(pLogToFile, g_ReportHeapCreate, "_PoolFree_; %u; %u; %s; %u; %u; %u; %u; %u; %u; 0; 0; 0; 0", 
							&pBuf[0], m_uElementSize, &pBuf[m_uTrackingOffset], 
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (0 * sizeof(jrs_sizet))]), 
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (1 * sizeof(jrs_sizet))]),
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (2 * sizeof(jrs_sizet))]),
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (3 * sizeof(jrs_sizet))]),
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (4 * sizeof(jrs_sizet))]),
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (5 * sizeof(jrs_sizet))]),
							MemoryManagerPlatformAddressToBaseAddress(pBuf[m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)) + (6 * sizeof(jrs_sizet))])); 

					}
					else
					{
						cMemoryManager::DebugOutput("Free       - 0x%016x", pBuf);
						cMemoryManager::DebugOutputFile(pLogToFile, g_ReportHeapCreate, "_PoolFree_; %u; %u; %s; %u; %u; %u; %u; %u; %u; 0; 0; 0; 0", 
							&pBuf[0], m_uElementSize, "Unknown", 0, 0, 0, 0, 0, 0, 0);
					}

				}

				// Next pointer 
				pBuf += m_uHeaderSize / sizeof(jrs_sizet);
			}
		}
		else
		{
			cMemoryManager::DebugOutput("Individual Element logging is disabled for speed");
		}

		// Close output
		cMemoryManager::DebugOutput("---------------------------------------------------------------------------------------------");

		// Incase we need thread safety
		if(m_bThreadSafe)
			m_Mutex.Unlock();
	}

	//  Description:
	//		Destroys the pool and any internals that the pool may have acquired.  Private function.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Destroys the pool and any internals that the pool may have acquired.
	void cPoolNonIntrusive::Destroy(void)
	{
		// No destruction with valid allocations?
		if(!m_bAllowDestructionWithAllocations && m_uUsedElements)
		{
			PoolWarning(m_bAllowDestructionWithAllocations && m_uUsedElements, JRSMEMORYERROR_VALIDALLOCATIONS, "Pool still has valid allocations.");
		}

		// A heap cant free this with out saying 'it comes from pool'.  We clear that error here by just setting it to null.
		void *pPoolBuffer = m_pBuffer;
		m_pBuffer = NULL;

		// Free the main pool off.
		cMemoryManager::Get().Free(pPoolBuffer, JRSMEMORYFLAG_POOL, "Pool Free");
	}


	//  Description:
	//		Creation function for Non Intrusive Heaps.
	//  See Also:
	//		uElementSize - The size of each element to be allocated.
	//		uMaxElements - Maximum number of elements to allocate.
	//		pPoolMemory - Heap that the pool headers get allocated from.
	//		pDataPointer - The data area of the pool.  Pool never directly accesses this.
	//		uDataPointerSize - Size of the data area of the pool.  This must match the ElementSize * uMaxElements.
	//		pName - Name of the pool.
	//		pDetails - Pool details.
	//  Arguments:
	//		None
	//  Return Value:
	//      TRUE if successful. FALSE otherwise.
	//  Summary:	
	//		Creation function for non intrusive heaps.  
	jrs_bool cPoolNonIntrusive::Create(jrs_u32 uElementSize, jrs_u32 uMaxElements, cHeap *pPoolMemory, void *pDataPointer, jrs_u64 uDataPointerSize, const jrs_i8 *pName, const sPoolDetails *pDetails)
	{
		PoolWarning(!pDetails->bEnableSentinel, JRSMEMORYERROR_INVALIDARGUMENTS, "Sentinels cannot exist for non intrusive memory");
		PoolWarning(!pDetails->pOverrunHeap, JRSMEMORYERROR_INVALIDARGUMENTS, "Overrun cannot exist for non intrusive memory");

		m_uMaxElements = uMaxElements;
		m_uAlignment = pDetails->uAlignment;
		m_bThreadSafe = pDetails->bThreadSafe;
		m_bEnableMemoryTracking = pDetails->bEnableMemoryTracking;
		m_bAllowDestructionWithAllocations = pDetails->bAllowDestructionWithAllocations;
		m_bAllowNotEnoughSpaceReturn = pDetails->bAllowNotEnoughSpaceReturn;		
		m_bEnableErrors = pDetails->bEnableErrors;
		m_bErrorsAsWarnings = pDetails->bErrorsAsWarnings;

		// Minimum size must be at least size_t.  If the alignment is large we enlarge the size of the objects to match that.
		jrs_u32 minsize = uElementSize;
		minsize = minsize < m_uAlignment ? m_uAlignment : minsize;
	
		// Final sizes
		m_uElementSize = minsize;
		m_uTrackingOffset = 0;
		m_uHeaderSize = sizeof(jrs_sizet);			
#ifndef MEMORYMANAGER_MINIMAL
		if(m_bEnableMemoryTracking)
		{
			m_uTrackingOffset = m_uHeaderSize;
			m_uHeaderSize += sizeof(sMemPoolTracking);
		}
		m_uTrackingOffset /= sizeof(jrs_sizet);
#endif
		// Get the size
		m_uPoolSize = m_uElementSize * m_uMaxElements;

		// The main data buffer
		m_pDataBuffer = (jrs_sizet *)pDataPointer;

		// Check if the pool data matches the alignment and size
		if(uDataPointerSize != m_uPoolSize)
		{
			PoolWarning(uDataPointerSize == m_uPoolSize, JRSMEMORYERROR_NONINTRUSIVEDATASIZE, "uDataPointerSize is not the correct size.  Non intrusive pool calculates this as %dbytes", (m_uElementSize * m_uMaxElements));
			return FALSE;
		}

		if(((jrs_sizet)pDataPointer & (m_uAlignment - 1)))
		{
			PoolWarning(!((jrs_sizet)pDataPointer & (m_uAlignment - 1)), JRSMEMORYERROR_NONINTRUSIVEDATAALIGNMENT, "pDataPointer is not aligned to %d", m_uAlignment);
			return FALSE;
		}

		// Allocate from the pool unless it is null, then just take it from the last memory heap.
		m_pBuffer = (jrs_sizet *)pPoolMemory->AllocateMemory(m_uHeaderSize * m_uMaxElements, pDetails->uBufferAlignment, JRSMEMORYFLAG_POOL, m_Name);
		if(!m_pBuffer)
		{
			PoolWarning(m_pBuffer, JRSMEMORYERROR_NOPOOLMEM, "Not enough memory to allocate the pool header buffer.");
		}

		// Clear the memory read for allocations
		jrs_sizet *pBuf = m_pFreePtr = m_pBuffer;
		for(jrs_u32 i = 0; i < m_uMaxElements; i++)
		{
			// Set the data to each of the right bits of data.
			pBuf += m_uHeaderSize  / sizeof(jrs_sizet);

#ifndef MEMORYMANAGER_MINIMAL
			if(m_bEnableMemoryTracking)
			{
				StackTrace(&m_pBuffer[((m_uHeaderSize  / sizeof(jrs_sizet)) * i) + (m_uTrackingOffset + (MemoryManager_StringLength / sizeof(jrs_sizet)))]);
				strcpy((char *)&m_pBuffer[((m_uHeaderSize  / sizeof(jrs_sizet)) * i) + m_uTrackingOffset], "Free");
			}
#endif
			m_pBuffer[((m_uHeaderSize  / sizeof(jrs_sizet)) * i) + 0] = (jrs_sizet)pBuf;
		}
		m_pBuffer[(m_uHeaderSize  / sizeof(jrs_sizet)) * (m_uMaxElements - 1)] = 0;
		m_uUsedElements = 0;

		return TRUE;
	}
}	// Elephant
