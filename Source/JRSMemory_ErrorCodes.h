/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/

#ifndef _JRSMEMORY_ERRORCODES_H
#define _JRSMEMORY_ERRORCODES_H

// Basic defines and error checking macros.
#ifdef MEMORYMANAGER_MINIMAL
#if defined(JRSMEMORYMWERKSPLATFORMS)
#define HeapWarning(CHECK, ERRCODE, ERRORTEXT, args...) 
#define HeapWarningExtern(HEAP, CHECK, ERRCODE, ERRORTEXT, args...)
#define MemoryWarning(CHECK, ERRCODE, ERRORTEXT, args...) 
#define PoolWarning(CHECK, ERRCODE, ERRORTEXT, args...)
#else
#define HeapWarning(CHECK, ERRCODE, ERRORTEXT, ...) 
#define HeapWarningExtern(HEAP, CHECK, ERRCODE, ERRORTEXT, ...)
#define MemoryWarning(CHECK, ERRCODE, ERRORTEXT, ...) 
#define PoolWarning(CHECK, ERRCODE, ERRORTEXT, ...)
#endif
#if defined(MEMORYMANAGER_ENABLESENTINELCHECKS) || defined(MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS)
// WE MUST NEVER COMPILE WITH THE ABOVE DEFINED WHILE MINIMAL IS DEFINED.  DEFINES MUST BE PLACED IN A FILE THAT WILL 
// AFFECT ALL JRSMemoryX FILES OR ERRORS MAY OCCUR.  BEST PLACE TO DEFINE THESE IS IN THE GLOBAL PREPROCESSOR LIST
#error 
#endif

#undef MEMORYMANAGER_ENABLESENTINELCHECKS
#undef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
#else
// CW needs the args to be defined different
#if defined(JRSMEMORYMWERKSPLATFORMS)
#define HeapWarning(CHECK, ERRCODE, ERRORTEXT, args...) if( (CHECK) ? false : true ) cMemoryManager::DebugWarning(this, 0, ERRCODE, __FUNCTION__, ERRORTEXT, ##args)
#define HeapWarningExtern(HEAP, CHECK, ERRCODE, ERRORTEXT, args...) if( (CHECK) ? false : true ) cMemoryManager::DebugWarning(HEAP, 0, ERRCODE, __FUNCTION__, ERRORTEXT, ##args)
#define MemoryWarning(CHECK, ERRCODE, ERRORTEXT, args...) if( (CHECK) ? false : true ) cMemoryManager::DebugWarning(0, 0, ERRCODE, __FUNCTION__, ERRORTEXT, ##args)
#define PoolWarning(CHECK, ERRCODE, ERRORTEXT, args...) if((CHECK) ? false : true) cMemoryManager::DebugWarning(0, this, ERRCODE, __FUNCTION__, ERRORTEXT, ##args)
#else
#define HeapWarning(CHECK, ERRCODE, ERRORTEXT, ...) if( (CHECK) ? false : true ) cMemoryManager::DebugWarning(this, 0, ERRCODE, __FUNCTION__, ERRORTEXT, ##__VA_ARGS__)
#define HeapWarningExtern(HEAP, CHECK, ERRCODE, ERRORTEXT, ...) if( (CHECK) ? false : true ) cMemoryManager::DebugWarning(HEAP, 0, ERRCODE, __FUNCTION__, ERRORTEXT, ##__VA_ARGS__)
#define MemoryWarning(CHECK, ERRCODE, ERRORTEXT, ...) if( (CHECK) ? false : true ) cMemoryManager::DebugWarning((cHeap *)0, 0, ERRCODE, __FUNCTION__, ERRORTEXT, ##__VA_ARGS__)
#define PoolWarning(CHECK, ERRCODE, ERRORTEXT, ...) if((CHECK) ? false : true) cMemoryManager::DebugWarning((cHeap *)0, this, ERRCODE, __FUNCTION__, ERRORTEXT, ##__VA_ARGS__)
#endif // args... macro defs
#endif

// Error codes
#define JRSMAKEERROR(MAJOR, MINOR)  ((MAJOR << 16) | MINOR)

// Memory errors
#define JRSMEMORYERROR_NOTINITIALIZED					JRSMAKEERROR(1, 1)			// Warning - Elephant is not initialized.
#define JRSMEMORYERROR_HEAPTOSMALL						JRSMAKEERROR(1, 2)			// Warning - Heap is to small to create or resize.
#define JRSMEMORYERROR_UNKNOWNWARNING					JRSMAKEERROR(1, 3)			// Warning - An unknown warning.  Generally for logging information
#define JRSMEMORYERROR_INVALIDMARKER					JRSMAKEERROR(1, 4)			// Warning - The LogMemoryMarker name is invalid.
#define JRSMEMORYERROR_UNKNOWNADDRESS					JRSMAKEERROR(1, 5)			// Warning - Unknown address has been passed in and could not be found by Elephant (i.e off the stack and not allcoated)
#define JRSMEMORYERROR_NOVALIDHEAP						JRSMAKEERROR(1, 6)			// Warning - No heap has been created so no allocation can occur.
#define JRSMEMORYERROR_DEBUGALLOCATEDADDTRAP			JRSMAKEERROR(1, 7)			// Warning - Debug allocation on address trap.
#define JRSMEMORYERROR_DEBUGFREEDADDTRAP				JRSMAKEERROR(1, 8)			// Warning - Debug free on address trap.
#define JRSMEMORYERROR_DEBUGALLOCATEDNUMBERTRAP			JRSMAKEERROR(1, 9)			// Warning - Debug allocation on specific number trap.
#define JRSMEMORYERROR_DEBUGFREEDNUMBERTRAP				JRSMAKEERROR(1, 10)			// Warning - Debug free on specific number trap.
#define JRSMEMORYERROR_HEAPINVALIDCALL					JRSMAKEERROR(1, 11)			// Warning - Function call at this time was wrong or not allowed.
#define JRSMEMORYERROR_LOCKED							JRSMAKEERROR(1, 12)			// Warning - Heap is locked and prevented from allocations.
#define JRSMEMORYERROR_ZEROBYTEALLOC					JRSMAKEERROR(1, 13)			// Warning - Zero byte allocation detected.
#define JRSMEMORYERROR_SIZETOLARGE						JRSMAKEERROR(1, 14)			// Warning - Heap maximum allowed size is to big.
#define JRSMEMORYERROR_NULLPTR							JRSMAKEERROR(1, 15)			// Warning - Null pointer trying to be freed.
#define JRSMEMORYERROR_NOTLASTALLOC						JRSMAKEERROR(1, 16)			// Warning - Allocation has not been freed in reverse order as heap flag specified.
#define JRSMEMORYERROR_HEAPTOLINKINVALID				JRSMAKEERROR(1, 17)			// Warning - Linked heap is not NULL.
#define JRSMEMORYERROR_NOPOOLMEM						JRSMAKEERROR(1, 18)			// Warning - Out of memory for pool creation buffer.
#define JRSMEMORYERROR_OUTOFSPACE						JRSMAKEERROR(1, 19)			// Warning - Pool is out of memory.
#define JRSMEMORYERROR_NOVALIDNIHEAPEXPAND				JRSMAKEERROR(1, 20)			// Warning - Out of slabs.
#define JRSMEMORYERROR_HEAPSYSTEMALLOCATOR				JRSMAKEERROR(1, 21)			// Warning - System allocator/free/pagesize is invalid

#define JRSMEMORYERROR_CALLEDAFTERINITIALIZE			JRSMAKEERROR(2, 1)			// Potential fatal - Function should be called before Initialize.
#define JRSMEMORYERROR_INVALIDFILENAME					JRSMAKEERROR(2, 2)			// Potential fatal - Invalid file name for output specified.
#define JRSMEMORYERROR_INITIALIZEINVALIDSIZE			JRSMAKEERROR(2, 3)			// Potential fatal - Initialize is being created with an invalid size.
#define JRSMEMORYERROR_INITIALIZESIZETOSMALL			JRSMAKEERROR(2, 4)			// Potential fatal - Initialize cannot initialize with such a small size.
#define JRSMEMORYERROR_INVALIDARGUMENTS					JRSMAKEERROR(2, 5)			// Potential fatal - Invalid arguments were passed to the function.
#define JRSMEMORYERROR_HEAPINVALIDFREE					JRSMAKEERROR(2, 6)			// Potential fatal - Heap must be freed in reverse creation order.
#define JRSMEMORYERROR_HEAPSELFMANAGED					JRSMAKEERROR(2, 7)			// Potential fatal - Heap is marked as managed or selfmanaged when the opposite is required.
#define JRSMEMORYERROR_HEAPTOBIG						JRSMAKEERROR(2, 8)			// Potential fatal - Heap being created is to big.
#define JRSMEMORYERROR_HEAPNAME							JRSMAKEERROR(2, 9)			// Potential fatal - Heap name is identical to others or invalid.
#define JRSMEMORYERROR_HEAPINVALID						JRSMAKEERROR(2, 10)			// Potential fatal - No valid heap to perform operations on.
#define JRSMEMORYERROR_HEAPCREATEINVALIDPOINTER			JRSMAKEERROR(2, 12)			// Potential fatal - Invalid memory address passed to pointer.
#define JRSMEMORYERROR_HEAPWITHVALIDALLOCATIONS			JRSMAKEERROR(2, 13)			// Potential fatal - Heap still has valid memory allocations active.
#define JRSMEMORYERROR_HEAPWITHVALIDPOOLS				JRSMAKEERROR(2, 14)			// Potential fatal - Heap still has valid pools allocated.
#define JRSMEMORYERROR_POOLCREATEELEMENTSIZE			JRSMAKEERROR(2, 15)			// Potential fatal - Pool element size is incorrect.
#define JRSMEMORYERROR_POOLCREATEELEMENTCOUNT			JRSMAKEERROR(2, 16)			// Potential fatal - Pool element count is invalid.
#define JRSMEMORYERROR_POOLCREATENAME					JRSMAKEERROR(2, 17)			// Potential fatal - Pool name is invalid.
#define JRSMEMORYERROR_POOLDESTROYINVALID				JRSMAKEERROR(2, 18)			// Potential fatal - Pool destruction error.
#define JRSMEMORYERROR_POOLCREATENOMEMORY				JRSMAKEERROR(2, 19)			// Potential fatal - No memory to create pool.
#define JRSMEMORYERROR_ELEPHANTOOM						JRSMAKEERROR(2, 20)			// Potential fatal - Elephant cannot create a block of memory to requested size to function.
#define JRSMEMORYERROR_INVALIDALIGN						JRSMAKEERROR(2, 21)			// Potential fatal - Invalid alignment specified.
#define JRSMEMORYERROR_WRONGHEAP						JRSMAKEERROR(2, 22)			// Potential fatal - Allocation didn't come from this heap.
#define JRSMEMORYERROR_ADDRESSNOTFROMPOOL				JRSMAKEERROR(2, 25)			// Potential fatal - Address is not within this pool's range.
#define JRSMEMORYERROR_VALIDALLOCATIONS					JRSMAKEERROR(2, 26)			// Potential fatal - Pool still has active allocations.
#define JRSMEMORYERROR_NONINTRUSIVEDATASIZE				JRSMAKEERROR(2, 27)			// Potential fatal - Non intrusive pool size is incorrect.
#define JRSMEMORYERROR_NONINTRUSIVEDATAALIGNMENT		JRSMAKEERROR(2, 28)			// Potential fatal - Non intrusive pool alignment is incorrect.
#define JRSMEMORYERROR_OUTOFMEMORY						JRSMAKEERROR(2, 29)			// Potential fatal - Heap/Pool count not allocate due to running out of memory.
#define JRSMEMORYERROR_INVALIDFLAG						JRSMAKEERROR(2, 30)			// Potential fatal - Flag does not match.  I.e new[] called with delete.
#define JRSMEMORYERROR_RESIZEFAIL						JRSMAKEERROR(2, 31)			// Potential fatal - Resizing operation failed.
#define JRSMEMORYERROR_MEMORYADDRESSFROMPOOL			JRSMAKEERROR(2, 33)			// Potential fatal - Memory address comes from pool not heap.
#define JRSMEMORYERROR_RESIZESIZETOSMALL				JRSMAKEERROR(2, 34)			// Potential fatal - Minimum resize size is to small.

// * = slightly more serious
#define JRSMEMORYERROR_POSTFREECORRUPTION				JRSMAKEERROR(2, 32)			// Potential fatal* - Allocation has been corrupted (Enhanced Debugging)
#define JRSMEMORYERROR_ALREADYFREED						JRSMAKEERROR(2, 24)			// Potential fatal* - Memory has already been freed.
#define JRSMEMORYERROR_RESIZEOFELEPHANTFAILED			JRSMAKEERROR(2, 35)			// Potential fatal* - Resizing of Elephant has failed, this may cause complications with Heap resize.
#define JRSMEMORYERROR_WRONGBIN							JRSMAKEERROR(2, 36)			// Potential fatal* - The internal marker is in the wrong bin.
#define JRSMEMORYERROR_INVALIDLINK						JRSMAKEERROR(2, 37)			// Potential fatal* - The internal pointer isnt referring to the correct pointer.
#define JRSMEMORYERROR_INVALIDRECLAIMPTR				JRSMAKEERROR(2, 38)			// Potential fatal* - Invalid reclaim pointer.

#define JRSMEMORYERROR_NOTENOUGHHEAPS					JRSMAKEERROR(4, 1)			// FATAL - Not enough free heaps.
#define JRSMEMORYERROR_NOTENOUGHUSERHEAPS				JRSMAKEERROR(4, 2)			// FATAL - Not enough free user heaps.
#define JRSMEMORYERROR_ENHANCEDINVALIDADDRESS			JRSMAKEERROR(4, 3)			// FATAL - An invalid memory address has been detected in enhanced debugging. This is not possible other than due to corruption.
#define JRSMEMORYERROR_INITIALIZEOOM					JRSMAKEERROR(4, 4)			// FATAL - Not enough memory to initialize.
#define JRSMEMORYERROR_HEAPNAMETOLARGE					JRSMAKEERROR(4, 5)			// FATAL - Heap name is too large.
#define JRSMEMORYERROR_SENTINELALLOCCORRUPT				JRSMAKEERROR(4, 6)			// FATAL - Sentinel corruption detected. 
#define JRSMEMORYERROR_SENTINELFREECORRUPT				JRSMAKEERROR(4, 7)			// FATAL - Sentinel corruption on free block detected.  
#define JRSMEMORYERROR_INVALIDFREEBLOCK					JRSMAKEERROR(4, 8)			// FATAL - Invalid free block
#define JRSMEMORYERROR_INVALIDALLOCBLOCK				JRSMAKEERROR(4, 9)			// FATAL - Invalid allocated block
#define JRSMEMORYERROR_FATAL							JRSMAKEERROR(4, 10)			// FATAL - Serious fatal.
#define JRSMEMORYERROR_UNKNOWNCORRUPTION				JRSMAKEERROR(4, 11)			// FATAL - Unknown internal corruption.
#define JRSMEMORYERROR_INVALIDADDRESS					JRSMAKEERROR(4, 14)			// FATAL - Internal memory address has been corrupted.  Further operation may cause significant problems.
#define JRSMEMORYERROR_USERMEMORYADDRESSBUTRESIZEMODE	JRSMAKEERROR(4, 15)			// FATAL - user requested resize mode but supplied user memory address.
#define JRSMEMORYERROR_NOTENOUGHNONINTRUSIVEHEAPS		JRSMAKEERROR(4, 16)			// FATAL - Not enough free user heaps.
#define JRSMEMORYERROR_HEAPNAMEINVALID					JRSMAKEERROR(4, 17)			// FATAL - Heap name invalid.
#define JRSMEMORYERROR_BININVALID						JRSMAKEERROR(4, 18)			// FATAL - Bins are invalid.
#define JRSMEMORYERROR_CTZIMPLEMENTATIONFAIL			JRSMAKEERROR(4, 19)			// FATAL - Count Trailing Zero implementation fail.
#define JRSMEMORYERROR_CLZIMPLEMENTATIONFAIL			JRSMAKEERROR(4, 20)			// FATAL - Count Leading Zero implementation fail.
#define JRSMEMORYERROR_NONEXTBLOCKFORRECLAIM			JRSMAKEERROR(4, 21)			// FATAL - There is no valid next block.  Corruption will occur.

#endif	// _JRSMEMORY_ERRORCODES_H
