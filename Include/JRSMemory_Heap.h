#ifndef _JRSMEMORY_HEAP_H
#define _JRSMEMORY_HEAP_H

#ifndef _JRSMEMORY_H
#include <JRSMemory.h>
#endif

#ifndef _JRSMEMORY_THREADLOCKS_H
#include <JRSMemory_ThreadLocks.h>
#endif

#define JRSMEMORYMANAGER_PAGEFREE 0x1
#define JRSMEMORYMANAGER_PAGEALLOCATED 0x2
#define JRSMEMORYMANAGER_PAGESUBALLOC 0x4

// Elephant Namespace
namespace Elephant
{
	class cHeap;
	class cHeapNonIntrusive;

	// User call backs
	typedef void *(*MemoryManagerDefaultAllocator)(jrs_u64 uSize, void *pExtMemoryPtr);
	typedef void (*MemoryManagerDefaultFree)(void *pFree, jrs_u64 uSize);
	typedef jrs_sizet (*MemoryManagerDefaultSystemPageSize)(void);
	typedef void (*HeapSystemCallback)(cHeap *pHeap, void *pAddress, jrs_u64 uSize, jrs_bool bFreeOp);
	typedef void (*HeapSystemNICallback)(cHeapNonIntrusive *pHeap, void *pAddress, jrs_u64 uSize, jrs_bool bFreeOp);

	// Forward declarations
	struct sFreeBlock;
	struct sAllocatedBlock;
	struct sLinkedBlock;
	class cPoolBase;
	class cPool;
	class cPoolNonIntrusive;
	struct sPoolDetails;

	// Elephant cHeap
	class JRSMEMORYDLLEXPORT cHeap
	{
		// Mutex for various elephant functions
		JRSMemory_ThreadLock *m_pThreadLock;
		jrs_bool m_bThreadSafe;

		jrs_sizet m_uMinAllocSize;		// Minimum allocation size
		jrs_sizet m_uMaxAllocSize;		// Maximum allocation size

		jrs_i8 *m_pHeapStartAddress;	// Start address in memory of the heap
		jrs_i8 *m_pHeapEndAddress;		// End address in memory of the heap
		jrs_sizet m_uHeapSize;			// Heap size in bytes

		jrs_bool m_bHeapIsMemoryManagerManaged;		// Flag to determine if the heap is managed by Elephant or not
		jrs_bool m_bHeapIsInUse;					// Enabled to true if in use

		jrs_u32 m_uUniqueAllocCount;				// Unique ID count
		jrs_u32 m_uUniqueFreeCount;					// Unique ID count

		jrs_sizet m_uAllocatedSize;					// Current allocated total
		jrs_u32 m_uAllocatedCount;					// Current allocated count
		jrs_sizet m_uAllocatedSizeMax;				// Max allocated total
		jrs_u32 m_uAllocatedCountMax;				// Max allocated count

		jrs_u32 m_uCallstackDepth;					// Default callstack start depth

		sFreeBlock *m_pMainFreeBlock;				// Floating free block

		jrs_u32 m_uDefaultAlignment;				// Default alignment value	
		jrs_bool m_bReverseFreeOnly;				// Reverse free only
		jrs_bool m_bAllowNullFree;					// Null free
		jrs_bool m_bAllowZeroSizeAllocations;		// 0 size allocation
		jrs_bool m_bAllowDestructionWithAllocations;	// Heap destruction with allocations still existing.
		jrs_bool m_bLocked;							// true if no allocations can take place.
		jrs_bool m_bAllowNotEnoughSpaceReturn;		// Disable errors for allocations that fail
		jrs_bool m_bUseEndAllocationOnly;			// End allocation only

		// The allocated list
		sAllocatedBlock *m_pAllocList;				// Live allocation list

		sLinkedBlock *m_pResizableLink;				// Pointer to the first resizable link
		jrs_bool m_bResizable;						// TRUE if resizable by Elephant.
		jrs_sizet m_uResizableSizeMin;				// Minimum size to resize.  Multiple of page size.  Minimum 32MB.
		jrs_u64 m_uReclaimSize;						// Minimum size to reclaim.  Larger is faster and minimum is m_uResizableSizeMin.
		jrs_bool m_bAllowResizeReclaimation;		// Allow reclamation.

		// Debug bits and pieces
		jrs_bool m_bEnableErrors;					// Error enable
		jrs_bool m_bErrorsAsWarnings;				// Errors as warnings
		jrs_bool m_bEnableExhaustiveErrorChecking;	// Exhaustive error checking
		jrs_bool m_bEnableSentinelChecking;			// Quick sentinel checking
		jrs_bool m_bEnableReportsInErrors;			// Enable reporting in errors.
		jrs_bool m_bEnableLogging;					// Enables logging.  True by default.

		jrs_i8 m_HeapName[32];						// Heap name
		jrs_u32 m_uHeapId;							// Heap Id

		jrs_bool m_bHeapClearing;					// Enables heap clearing
		jrs_u8 m_uHeapAllocClearValue;				// Clear value for allocation
		jrs_u8 m_uHeapFreeClearValue;					// Clear value for frees
		jrs_bool m_bEnableEnhancedDebug;				// Defers free operations and does full checks when freeing.

		// Debug flag constants to mark for tracking
		static const jrs_u32 m_uDebugFlag_TrapFreeNumber = 1;			
		static const jrs_u32 m_uDebugFlag_TrapFreeAddress = 2;
		static const jrs_u32 m_uDebugFlag_TrapAllocatedNumber = 4;
		static const jrs_u32 m_uDebugFlag_TrapAllocatedAddress = 8;

		jrs_u32 m_uDebugFlags;						// Set debug flags
		jrs_u32 m_uDebugTrapOnFreeNum;				// Free number tracking
		void *m_pDebugTrapOnFreeAddress;			// Free address tracking
		jrs_u32 m_uDebugTrapOnAllocatedNum;			// Allocated number tracking
		void *m_pDebugTrapOnAllocatedAddress;		// Allocated memory address tracking
		jrs_u32 m_uEDebugPending;					// Pending number of allocations remaining to be freed for enhanced debugging.

		// Bins.
		static const jrs_u32 m_uBinCount = 64;
		sFreeBlock *m_pBins[m_uBinCount];

		// Attached pools
		cPoolBase *m_pAttachedPools;

		// System callbacks for allocation
		MemoryManagerDefaultAllocator m_systemAllocator;				// Allocates the heap during creation and resizing. Default NULL (uses cMemoryManager defaults).
		MemoryManagerDefaultFree m_systemFree;						// Frees any memory for the heap during reclaiming or destruction.  Default NULL (uses cMemoryManager defaults).
		MemoryManagerDefaultSystemPageSize m_systemPageSize;			// Page size required for system allocation.  Default NULL (uses cMemoryManager defaults).
		HeapSystemCallback m_systemOpCallback;						// Callback called everytime it calls a system function.

		// Private functions
		cHeap();
		void DestroyLinkedMemory(void);

		// Start block initialize
		void InitializeMainFreeBlock(void);			

		// Block functions
		sAllocatedBlock *AllocateFromFreeBlock(sFreeBlock *pFreeBlock, jrs_sizet uSize, jrs_u32 uAlignment, jrs_u32 uFlag);		
		void InternalFreeMemory(void *pMemory, jrs_u32 uFlag, const jrs_i8 *pName, const jrs_u32 uExternalId);
		sFreeBlock *SearchForFreeBlockBinFit(jrs_sizet uSize, jrs_u32 uAlignment);		

		// Bin Management
		void RemoveBinAllocation(sFreeBlock *pFreeBlock);
		void CreateBinAllocation(jrs_sizet uSize, sFreeBlock *pNewFreeBlock, sFreeBlock **pFBPrevBin, sFreeBlock **pFBNextBin);
		void StackTrace(jrs_sizet *pCallStack);			// Stack tracing

		sLinkedBlock *ResizeInsertLink(jrs_u8 *pEndPtrForNewLink, jrs_u8 *pStartPtrOfNextBlock, sAllocatedBlock *pPrevAlloc, sAllocatedBlock *pNextAlloc, sLinkedBlock *pPrevLink, sLinkedBlock *pNextLink);
		void ResizeInsertFreeBlock(sAllocatedBlock *pLastAllocBefore, sAllocatedBlock *pNextAllocPtr, jrs_u8 *pFreeBlockStartAddress);
		void ResizeSafeInsertFreeBlock(sAllocatedBlock *pLastAllocBefore, sAllocatedBlock *pNextAllocPtr, jrs_u8 *pFreeBlockStartAddress);
		jrs_bool ResizeInternal(void *pNewSBlock, void *pNewEBlock);
		void InternalReclaimMemory(void);
		sFreeBlock *GetResizeLargestFragment(jrs_sizet uLargerThan, sFreeBlock **pNextFreeBlock, sFreeBlock **pLoopBlock) const;

		// Sentinel tracking
		void SetSentinelsFreeBlock(sFreeBlock *pBlock);
		void SetSentinelsAllocatedBlock(sAllocatedBlock *pBlock);
		void CheckFreeBlockSentinels(sFreeBlock *pBlock);
		void CheckAllocatedBlockSentinels(sAllocatedBlock *pBlock);

		// Finds the bin size related to the allocation size wanted.
		jrs_sizet GetBinLookupBasedOnSize(jrs_sizet uSize) const;

		// Pools
		void AttachPool(cPoolBase *pPool);
		void RemovePool(cPoolBase *pPool);

		// friend
		friend class cMemoryManager;
		friend class cPoolBase;


	public:

		struct sHeapDetails
		{
			jrs_u32 uDefaultAlignment;			// Minimum of 16bytes.  Must be power of two multiple. Default 16.
			jrs_sizet uMinAllocationSize;		// Minimum allocation size. Must be a multiple of 16. Default 16 (32 for 64bit).
			jrs_sizet uMaxAllocationSize;		// Maximum allocation size. Must be larger than uMinAllocationSize except for 0 which specifies use maximum. Default 0.
			jrs_bool bUseEndAllocationOnly;		// Memory holes will not be used. Default false
			jrs_bool bReverseFreeOnly;			// Reverse free only.  Default false.
			jrs_bool bAllowNullFree;			// Allow free's of NULL to occur.  Default false.
			jrs_bool bAllowZeroSizeAllocations;	// Allow 0 size allocations.  Will actually allocate 16bytes.  Default false.
			jrs_bool bAllowDestructionWithAllocations;	// Allows the destruction of the heap with valid allocations.  Default false.
			jrs_bool bAllowNotEnoughSpaceReturn;		// Allows the heap to return null without checking for failure when memory cant fit. Default false.
			jrs_bool bHeapIsSelfManaged;		// Self managed heap.  Memory manager will not take the memory out of its default pool. Default false.
			jrs_bool bThreadSafe;				// Heap is thread safe when this is enabled.  Default true.
			jrs_sizet uResizableSize;			// Minimum size to resize the heap each time in resizable mode.  Larger sizes can create excessive wastage but will perform better. Default and minimum 32MB.
			jrs_sizet uReclaimSize;				// Size to reclaim.  Will try and reclaim all blocks larger or equal to this size and return to the OS.  Minimum size is uResizableSize. Default 128MB.
			jrs_bool bAllowResizeReclaimation;	// Gives memory back to OS.  Performance hit may occur but will help.  Only for resizable heaps. Default false.
			jrs_bool bEnableLogging;			// Enables logging for this heap.  Default true.

			// Memory clearing and enhanced debugging.
			jrs_bool bHeapClearing;				// Enable this to clear the allocations and frees with set values when the operation takes place.  Default false.
			jrs_u8 uHeapAllocClearValue;		// Clear value of an allocation.  0 is not advised.  Default 0xad.
			jrs_u8 uHeapFreeClearValue;			// Clear value of an free.  0 is not advised.  Default 0xbc.

			// Extra debug structures.		These are available in debug only builds.
			jrs_bool bEnableEnhancedDebug;		// If Enhanced debugging is available this will defer all allocations to help track corruption.  Default true (and only runs if Enhanced Debugging is enabled).
			jrs_bool bEnableErrors;				// Checks for errors.  Default true.
			jrs_bool bErrorsAsWarnings;			// Disables all errors and turns them into warnings. Default false.
			jrs_bool bEnableExhaustiveErrorChecking;	// Enables exhaustive error checking.  Calls the CheckForErrors function at the same frequency as bEnableActiveErrorChecking.  Time consuming.  Default false.
			jrs_bool bEnableSentinelChecking;		// Enables sentinel checking on for the blocking being allocated or freed. Default true.

			// System callbacks for allocation
			MemoryManagerDefaultAllocator systemAllocator;				// Allocates the heap during creation and resizing. Default NULL (uses cMemoryManager defaults).
			MemoryManagerDefaultFree systemFree;						// Frees any memory for the heap during reclaiming or destruction.  Default NULL (uses cMemoryManager defaults).
			MemoryManagerDefaultSystemPageSize systemPageSize;			// Page size required for system allocation.  Default NULL (uses cMemoryManager defaults).
			HeapSystemCallback systemOpCallback;						// Callback when a system op occurs.  Default NULL.

			sHeapDetails() : uDefaultAlignment(16), uMinAllocationSize(16), uMaxAllocationSize(0), bUseEndAllocationOnly(false), bReverseFreeOnly(false),
				bAllowNullFree(false), bAllowZeroSizeAllocations(false), bAllowDestructionWithAllocations(false), bAllowNotEnoughSpaceReturn(false), 
				bHeapIsSelfManaged(false), bThreadSafe(true), uResizableSize(32 << 20), uReclaimSize(128 << 20), bAllowResizeReclaimation(false), bEnableLogging(true), 
				bHeapClearing(false), uHeapAllocClearValue(0xad), uHeapFreeClearValue(0xbc), bEnableEnhancedDebug(true),
				bEnableErrors(true), bErrorsAsWarnings(false), bEnableExhaustiveErrorChecking(false), bEnableSentinelChecking(true),
				systemAllocator(NULL), systemFree(NULL), systemPageSize(NULL), systemOpCallback(NULL)
			{};
		};

		// Constructor/Destructor.  Do not use.  Use CreateHeap instead.
		cHeap(void *pMemoryAddress, jrs_sizet uSize, const jrs_i8 *pHeapName, sHeapDetails *pHeapDetails);
		~cHeap();

		// Memory allocation/deallocation functions
		void *AllocateMemory(jrs_sizet uSize, jrs_u32 uAlignment, jrs_u32 uFlag = JRSMEMORYFLAG_NONE, const jrs_i8 *pName = 0, const jrs_u32 uExternalId = 0);
		void FreeMemory(void *pMemory, jrs_u32 uFlag = JRSMEMORYFLAG_NONE, const jrs_i8 *pName  = 0, const jrs_u32 uExternalId = 0);
		void *ReAllocateMemory(void *pMemory, jrs_sizet uSize, jrs_u32 uAlignment, jrs_u32 uFlag = JRSMEMORYFLAG_NONE, const jrs_i8 *pName = 0, const jrs_u32 uExternalId = 0);

		// Heap sizing
		jrs_bool Resize(jrs_sizet uSize);
		void Reclaim();

		// Information functions
		jrs_sizet GetMaxAllocationSize(void) const;
		jrs_sizet GetMinAllocationSize(void) const;
		jrs_u32 GetDefaultAlignment(void) const;

		const jrs_i8 *GetName(void) const;
		jrs_u32 GetUniqueId(void) const;
		jrs_sizet GetSize(void) const;
		void *GetAddress(void) const;
		void *GetAddressEnd(void) const;
		jrs_sizet GetResizableSize(void) const;

		jrs_sizet GetMemoryUsed(void) const;
		jrs_u32 GetNumberOfAllocations(void) const;
		jrs_sizet GetMemoryUsedMaximum(void) const;
		jrs_u32 GetNumberOfAllocationsMaximum(void) const;
		jrs_u32 GetNumberOfLinks(void) const;

		jrs_sizet GetSizeOfLargestFragment(void) const;
		jrs_sizet GetTotalFreeMemory(void) const;

		jrs_bool IsMemoryManagerManaged(void) const;
		jrs_bool IsAllocatedFromThisHeap(void *pMemory) const;
		jrs_bool IsLocked(void) const;
		jrs_bool IsNullFreeEnabled(void) const;
		jrs_bool IsZeroAllocationEnabled(void) const;
		jrs_bool IsOutOfMemoryReturnEnabled(void) const;
		jrs_bool IsExhaustiveSentinelCheckingEnabled(void) const;
		jrs_bool IsSentinelCheckingEnabled(void) const;
		jrs_bool AreErrorsEnabled(void) const;				
		jrs_bool AreErrorsWarningsOnly(void) const;
		jrs_bool IsLoggingEnabled(void) const;							//      Returns if continuous logging for this heap is enabled or not.
		jrs_bool IsAllocatedFromAttachedPool(void *pMemory);
		cPoolBase *GetPoolFromAllocatedMemory(void *pMemory);
		cPoolBase *GetPool(cPoolBase *pPool);

		// Enable/Disable values
		void EnableLock(jrs_bool bEnableLock);
		void EnableNullFree(jrs_bool bEnableNullFree);
		void EnableZeroAllocation(jrs_bool bEnableZeroAllocation);
		void EnableSentinelChecking(jrs_bool bEnable);
		void EnableExhaustiveSentinelChecking(jrs_bool bEnable);	
		void EnableLogging(jrs_bool bEnable);						// Enables or disables continuous logging for the heap.
		void SetCallstackDepth(jrs_u32 uDepth);					// Sets the callstack depth for the NAC/NACS libs to start from.  

		// Error checking
		void CheckForErrors(void);		//      Checks the entire heap for errors.

		// Debug functions
		void DebugTrapOnFreeNumber(jrs_u32 uFreeNumber);
		void DebugTrapOnFreeAddress(void *pAddress);
		void DebugTrapOnAllocatedUniqueNumber(jrs_u32 uFreeNumber);
		void DebugTrapOnAllocatedAddress(void *pAddress);

		// Reporting functions
		void ReportAll(const jrs_i8 *pLogToFile = NULL, jrs_bool includeFreeBlocks = FALSE, jrs_bool displayCallStack = FALSE);
		void ReportStatistics(jrs_bool bAdvanced = false);
		void ReportAllocationsMemoryOrder(const jrs_i8 *pLogToFile = 0, jrs_bool includeFreeBlocks = FALSE, jrs_bool displayCallStack = FALSE);

		// Reset statistics
		void ResetStatistics(void);

		void FreeAllEmptyLinkBlocks(void);
		jrs_bool CanReclaimBetweenMemoryAddresses(jrs_i8 *pStartAddress, jrs_i8 *pEndAddress, jrs_i8 **pStartOfFreeAddress, jrs_i8 **pEndOfFreeAddress);
		void ReclaimBetweenMemoryAddresses(jrs_i8 *pStartOfFreeAddress, jrs_i8 *pEndOfFreeAddress);

	};

	class JRSMEMORYDLLEXPORT cHeapNonIntrusive
	{
		static const jrs_u32 m_uMaxNumSlabs = 16;

		// Thread locking
		JRSMemory_ThreadLock *m_pThreadLock;

		// Block headers
		struct sPageBlock
		{
			// Page allocation information
			jrs_u32 activeAllocs[4];

			// Used to track next bin block of free pages.
			sPageBlock *pPrevBlock;
			sPageBlock *pPrev;
			sPageBlock *pNext;
			void *pDebugInfo;				// 16/32 bytes depending on 32/64bit

			jrs_u32 numFreePages;			// Enough for 4billion * 8k bytes per slab
			jrs_u8 slabNum;
			jrs_u8 pageFlags;
			jrs_u16 sizeOfSubAllocs;		// 16 bytes total
											// = 16 + 16/32 + 16 = 48/64 bytes per block.
			void Clear(void);
		};
		
		// Slabs - used to hold the various blocks
		struct sSlab
		{
			sPageBlock *pBlocks;
			void *pBase;
			jrs_sizet uSize;
			jrs_sizet uPageBlockSize;
			jrs_u32 numBlocks;
		};
		sSlab m_Slabs[m_uMaxNumSlabs];
		jrs_u32 m_uNumSlabs;

		cHeap *m_pStandardHeap;
		jrs_i8 m_HeapName[32];
		jrs_sizet m_uPageSize;
		friend class cMemoryManager;

		// Free bins
		static const jrs_u32 m_uMaxNumBins = 32;
		sPageBlock *m_pBins[m_uMaxNumBins];
		jrs_u32 m_uAvailableBins;

		// Allocated bins for small heaps. 64, 128, 256, 512, 1024, 2048, 4096.
		static const jrs_u32 m_uMaxNumAllocBins = 7;
		sPageBlock *m_pAllocBins[m_uMaxNumAllocBins];
		sPageBlock *m_pFullAllocBins[m_uMaxNumAllocBins];

#ifndef MEMORYMANAGER_MINIMAL
		jrs_u32 m_uDebugHeaderSize;
#endif
		// Addresses
		void *m_pHeapStartAddress;
		void *m_pHeapEndAddress;
		jrs_sizet m_uSize;
		jrs_u32 m_uHeapId;

		jrs_u32 m_uUniqueAllocCount;				// Unique ID count
		jrs_u32 m_uUniqueFreeCount;					// Unique ID count

		jrs_sizet m_uAllocatedSize;					// Current allocated total
		jrs_u32 m_uAllocatedCount;					// Current allocated count
		jrs_sizet m_uAllocatedSizeMax;				// Max allocated total
		jrs_u32 m_uAllocatedCountMax;				// Max allocated count

		jrs_bool m_bLocked;
		jrs_bool m_bThreadSafe;
		jrs_bool m_bAllowNullFree;
		jrs_bool m_bAllowZeroSizeAllocations;
		jrs_bool m_bAllowDestructionWithAllocations;
		jrs_bool m_bAllowNotEnoughSpaceReturn;
		jrs_bool m_bReverseFreeOnly;
		jrs_bool m_bUseEndAllocationOnly;
		jrs_sizet m_uDefaultAlignment;
		jrs_sizet m_uMinAllocSize;
		jrs_sizet m_uMaxAllocSize;
		jrs_bool m_bResizable;				// Heap will automatically resize.  Default false.
		jrs_sizet m_uResizableSize;			// Minimum size to resize the heap each time in resizable mode.  Larger sizes can create excessive wastage but will perform better. Default 128MB.

		// Debug bits and pieces
		jrs_bool m_bEnableErrors;					// Error enable
		jrs_bool m_bErrorsAsWarnings;				// Errors as warnings
		jrs_bool m_bEnableReportsInErrors;			// Enable reporting in errors.
		jrs_bool m_bEnableLogging;					// Enables logging. 
		jrs_bool m_bEnableMemoryTracking;				// Enables name and callstack tracking.  Default false.
		jrs_u32 m_uNumCallStacks;	
		jrs_u32 m_uCallstackDepth;					// Default callstack start depth

		jrs_bool m_bSelfManaged;					// Set to true when the heap has a default memory address.

		// System callbacks for allocation
		MemoryManagerDefaultAllocator m_systemAllocator;				// Allocates the heap during creation and resizing. Default NULL (uses cMemoryManager defaults).
		MemoryManagerDefaultFree m_systemFree;						// Frees any memory for the heap during reclaiming or destruction.  Default NULL (uses cMemoryManager defaults).
		MemoryManagerDefaultSystemPageSize m_systemPageSize;			// Page size required for system allocation.  Default NULL (uses cMemoryManager defaults).
		HeapSystemNICallback m_systemOpCallback;						// Callback called everytime it calls a system function.
		
		// Private methods
		cHeapNonIntrusive();

		// Book keeping
		void AddPagesToBin(sPageBlock *pFirstPageOfArray, jrs_u32 numPages);
		sPageBlock *FindPages(jrs_u32 numPages, jrs_sizet uAlignment);
		void RemoveFromBin(jrs_u32 uBin, sPageBlock *pBlock);
		void RemoveFromBin(sPageBlock *pBlock);

		// Sub block allocation
		void *AddSubAllocation(sPageBlock *pBlock, jrs_sizet uSize);

		// Helpers
		cHeapNonIntrusive::sSlab *FindSlabFromMemory(jrs_i8 *pMemory);
		void UpdateDebugInfo(void *pDebugInfo, const jrs_i8 *pName);

		// Expand the heap
		jrs_bool Expand(void *pMemoryAddress, jrs_sizet uSize);

		// Clean up
		void Destroy(void);

	public:

		struct sHeapDetails
		{
			jrs_u32 uDefaultAlignment;			// Minimum of 16bytes.  Must be power of two multiple. Default 16.
			jrs_sizet uMinAllocationSize;		// Minimum allocation size. Must be a multiple of 16. Default 16 (32 for 64bit).
			jrs_sizet uMaxAllocationSize;		// Maximum allocation size. Must be larger than uMinAllocationSize except for 0 which specifies use maximum. Default 0.

			jrs_bool bAllowNullFree;			// Allow free's of NULL to occur.  Default false.
			jrs_bool bAllowZeroSizeAllocations;	// Allow 0 size allocations.  Will actually allocate 16bytes.  Default false.
			jrs_bool bAllowDestructionWithAllocations;	// Allows the destruction of the heap with valid allocations.  Default false.
			jrs_bool bAllowNotEnoughSpaceReturn;		// Allows the heap to return null without checking for failure when memory cant fit. Default false.
			jrs_bool bThreadSafe;				// Heap is thread safe when this is enabled.  Default true.
			jrs_bool bResizable;				// Heap will automatically resize.  Default false.
			jrs_sizet uResizableSize;			// Minimum size to resize the heap each time in resizable mode.  Larger sizes can create excessive wastage but will perform better. Default 128MB.
			jrs_bool bEnableLogging;			// Enables logging for this heap.  Default true.

			// Extra debug structures.		These are available in debug only builds.
			jrs_bool bEnableErrors;				// Checks for errors.  Default true.
			jrs_bool bErrorsAsWarnings;			// Disables all errors and turns them into warnings. Default false.
			jrs_bool bEnableMemoryTracking;			// Enables name and callstack tracking.  Default false.
			jrs_u32 uNumCallStacks;					// Depth of callstacks to capture.  Default 8.  bEnableDebugFeatures must be true.

			// System callbacks for allocation
			MemoryManagerDefaultAllocator systemAllocator;				// Allocates the heap during creation and resizing. Default NULL (uses cMemoryManager defaults).
			MemoryManagerDefaultFree systemFree;						// Frees any memory for the heap during reclaiming or destruction.  Default NULL (uses cMemoryManager defaults).
			MemoryManagerDefaultSystemPageSize systemPageSize;			// Page size required for system allocation.  Default NULL (uses cMemoryManager defaults).
			HeapSystemNICallback systemOpCallback;						// Callback when a system op occurs.  Default NULL.

			sHeapDetails() : uDefaultAlignment(64), uMinAllocationSize(64), uMaxAllocationSize(0), bAllowNullFree(false), bAllowZeroSizeAllocations(false), bAllowDestructionWithAllocations(false),
				bAllowNotEnoughSpaceReturn(false), bThreadSafe(true), bResizable(false), uResizableSize(128 << 20), bEnableLogging(true),
				bEnableErrors(true), bErrorsAsWarnings(false), bEnableMemoryTracking(false), uNumCallStacks(8),
				systemAllocator(NULL), systemFree(NULL), systemPageSize(NULL), systemOpCallback(NULL)
			{};
		};

		// Constructor/Destructor.  Do not use.  Use CreateHeap instead.
		cHeapNonIntrusive(void *pMemoryAddress, jrs_sizet uSize, cHeap *pHeap, const jrs_i8 *pName, sHeapDetails *pHeapDetails);
		~cHeapNonIntrusive();

		// Memory allocation/deallocation functions
		void *AllocateMemory(jrs_sizet uSize, jrs_u32 uAlignment, jrs_u32 uFlag = JRSMEMORYFLAG_NONE, const jrs_i8 *pName = 0, const jrs_u32 uExternalId = 0);
		void FreeMemory(void *pMemory, jrs_u32 uFlag = JRSMEMORYFLAG_NONE, const jrs_i8 *pName  = 0, const jrs_u32 uExternalId = 0);
		
		// Reporting functions
		void ReportAll(const jrs_i8 *pLogToFile = 0, jrs_bool includeFreeBlocks = FALSE, jrs_bool displayCallStack = FALSE);
		void ResetStatistics(void);
		void ReportStatistics(jrs_bool bAdvanced = false);
		void ReportAllocationsMemoryOrder(const jrs_i8 *pLogToFile = NULL, jrs_bool includeFreeBlocks = FALSE, jrs_bool displayCallStack = FALSE);
		
		// Error checking
		void CheckForErrors(void);		//      Checks the entire heap for errors.
					
		// Helpers
		const jrs_i8 *GetName(void) const { return m_HeapName; }
		jrs_u32 GetUniqueId(void) const;
		jrs_sizet GetSize(void) const;

		jrs_sizet GetMemoryUsed(void) const;
		jrs_u32 GetNumberOfAllocations(void) const;
		jrs_sizet GetMemoryUsedMaximum(void) const;
		jrs_u32 GetNumberOfAllocationsMaximum(void) const;

		jrs_sizet GetSizeOfLargestFragment(void) const;
		jrs_sizet GetTotalFreeMemory(void) const;
		void *GetAddress(void) const;
		void *GetAddressEnd(void) const;
		jrs_sizet GetMaxAllocationSize(void) const;
		jrs_sizet GetMinAllocationSize(void) const;
		jrs_sizet GetDefaultAlignment(void) const;

		jrs_bool IsLocked(void) const;
		jrs_bool IsNullFreeEnabled(void) const;
		jrs_bool IsZeroAllocationEnabled(void) const;
		jrs_bool IsOutOfMemoryReturnEnabled(void) const;
		void EnableLock(jrs_bool bEnableLock);
		void EnableNullFree(jrs_bool bEnableNullFree);
		void EnableZeroAllocation(jrs_bool bEnableZeroAllocation);
		void EnableLogging(jrs_bool bEnable);					
		
		void SetCallstackDepth(jrs_u32 uDepth);					// Sets the callstack depth to start from.  
		
		jrs_bool IsLoggingEnabled(void) const;							//      Returns if continuous logging for this heap is enabled or not.
		jrs_bool IsMemoryTrackingEnabled(void) const;
		jrs_bool IsAllocatedFromThisHeap(void *pMemory) const;

		// Debug functions
		jrs_bool AreErrorsEnabled(void) const;
		jrs_bool AreErrorsWarningsOnly(void) const;
		jrs_sizet GetPageSize(void) const;
		jrs_sizet GetDebugHeaderSize(void) const;
	};
}

#endif
