#ifndef _JRSMEMORY_POOLS_H
#define _JRSMEMORY_POOLS_H

#ifndef _JRSMEMORY_H
#include <JRSMemory.h>
#endif

#ifndef _JRSMEMORY_THREADLOCKS_H
#include <JRSMemory_ThreadLocks.h>
#endif

// Elephant Namespace
namespace Elephant
{
	class cHeap;

	// Pool details.
	struct sPoolDetails
	{
		jrs_u32 uAlignment;								// Alignment of each data object.  4 bytes minimum (8 for 64bit). sizeof(jrs_sizet) by default.
		jrs_u32 uBufferAlignment;						// Alignment of the buffer the pool uses.  0 by default to use the heap default.
		cHeap *pOverrunHeap;							// If the pool runs out of space use this heap for storing the overrun details.  NULL by default.  Only valid for intrusive heaps like cMemoryPool.
		jrs_bool bEnableMemoryTracking;					// Enables name and callstack tracking per object.  Default false.  Ignored in MASTER library.
		jrs_bool bEnableSentinel;						// Enables sentinel testing per object.  Default false.  Ignored in MASTER library.
		jrs_bool bThreadSafe;							// Enables thread safe pools.  This is slightly slower than non safe. True by default.
		jrs_bool bAllowDestructionWithAllocations;		// Allows the destruction of the heap with valid allocations.  Default false.
		jrs_bool bAllowNotEnoughSpaceReturn;			// Allows the heap to return null without checking for failure when memory cant fit. Default false.
		jrs_bool bEnableErrors;							// Checks for errors.  Default true.
		jrs_bool bErrorsAsWarnings;						// Disables all errors and turns them into warnings. Default false.

		sPoolDetails() : uAlignment(sizeof(jrs_sizet)), uBufferAlignment(0), pOverrunHeap(NULL), bEnableMemoryTracking(false), bEnableSentinel(false), bThreadSafe(true), 
			bAllowDestructionWithAllocations(false), bAllowNotEnoughSpaceReturn(false), bEnableErrors(true), bErrorsAsWarnings(false) {}
	};

	class JRSMEMORYDLLEXPORT cPoolBase
	{
	protected:

		jrs_i8 m_Name[32];									// Pool name.
		JRSMemory_ThreadLock m_Mutex;						// Thread mutex.
		cPoolBase *m_pNext, *m_pPrev;						// Link to next and prev pools.  
		cHeap *m_pAttachedHeap;								// The heap the pool is attached too.
		jrs_bool m_bAllowDestructionWithAllocations;	
		jrs_bool m_bAllowNotEnoughSpaceReturn;		
		jrs_bool m_bLocked;
		jrs_bool m_bEnableErrors;
		jrs_bool m_bErrorsAsWarnings;
		jrs_u32 m_uPoolID;

		// Functions
		void StackTrace(jrs_sizet *pCallStack);
		void SetAllocatedSentinels(jrs_u32 *pStart, jrs_u32 *pEnd);
		void CheckAllocatedSentinels(jrs_u32 *pStart, jrs_u32 *pEnd);
		void SetFreeSentinels(jrs_u32 *pStart, jrs_u32 *pEnd);
		void CheckFreeSentinels(jrs_u32 *pStart, jrs_u32 *pEnd);

		// friend
		friend class cMemoryManager;	
		friend class cHeap;

		virtual ~cPoolBase();

		// Virtuals
		virtual void Destroy(void) = 0;

	public:

		// Constructor/Destructor
		cPoolBase(cHeap *pPoolMemory, const jrs_i8 *pName);

		// Information functions

		// Get/Set functions.
		cHeap *GetHeap(void) const;
		const jrs_i8 *GetName(void) const;
		jrs_u32 GetUniqueId(void) const { return m_uPoolID; }

		virtual void *GetAddress(void) = 0;
		virtual jrs_sizet GetSize(void) const = 0;

		// Information functions
		jrs_bool IsLocked(void) const;
		jrs_bool AreErrorsEnabled(void) const;
		jrs_bool AreErrorsWarningsOnly(void) const;

		virtual jrs_bool IsAllocatedFromThisPool(void *pMemory) const = 0;
		virtual jrs_u32 GetAllocationSize(void) const = 0;

		// Enable/Disable values
		void EnableLock(jrs_bool bEnableLock);

		virtual void ReportAll(const jrs_i8 *pLogToFile = 0) = 0;
		virtual void ReportStatistics(void) = 0;
		virtual void ReportAllocationsMemoryOrder(const jrs_i8 *pLogToFile = 0, jrs_bool bLogEachAllocation = false) = 0;

		virtual jrs_u32 GetTotalAllocations(void) = 0;
		virtual jrs_u32 GetMaxAllocations(void) = 0;

		virtual jrs_bool HasNameAndCallstackTracing(void) = 0;
		virtual jrs_bool HasSentinels(void) = 0;
	};

	class JRSMEMORYDLLEXPORT cPool : public cPoolBase
	{
		jrs_sizet *m_pBuffer;			// Main data pointer to the buffer.  Passed in by user.  16byte aligned minimum.
		jrs_sizet *m_pFreePtr;			// Current free pointer for the next available allocation.
		jrs_u32 m_uMaxElements;		// Maximum number of elements.
		jrs_u32 m_uElementSize;		// Element size in bytes (sizeof(jrs_sizet) byte multiple).  Rounded up based on object size and alignment.
		jrs_u32 m_uPoolSize;		// The size of the pool.
		jrs_u32 m_uAlignment;		// Alignment of each element, sizeof(jrs_sizet) minimum.
		jrs_bool m_bThreadSafe;		// True if thread safe.
		jrs_bool m_bEnableMemoryTracking;	// Enables name and callstack tracking per object.
		jrs_bool m_bEnableSentinel;			// Enables sentinel testing per object.

		jrs_u32 m_uPointerOffset;
		jrs_u32 m_uTrackingOffset;			// Offsets for memory tracking.  Do nothing in master.
		jrs_u32 m_uStartSentinelOffset;

		jrs_u32 m_uUsedElements;			// Amount of used elements from the pool.
		jrs_bool m_bEnableOverrun;	// If the memory pool runs out of memory allocate from the heap specified (or just the main heap if null)
		cHeap *m_pOverrunHeap;		// Overrun heap to use if pool runs out of memory.

		// Friends
		friend class cHeap;

	protected:
		
		// Functions
		virtual void Destroy(void);

	public:

		// Constructor/Destructor
		cPool(jrs_u32 uElementSize, jrs_u32 uMaxElements, cHeap *pPoolMemory, const jrs_i8 *pName, const sPoolDetails *pDetails);
		virtual ~cPool();

		void *AllocateMemory(void);
		void *AllocateMemory(const jrs_i8 *pName);
		void FreeMemory(void *pMemory, const jrs_i8 *pName = 0);
	
		// Information functions
		jrs_u32 GetNumberOfAllocations(void) const;
		virtual jrs_bool IsAllocatedFromThisPool(void *pMemory) const;
		virtual jrs_u32 GetAllocationSize(void) const;

		virtual void *GetAddress(void);
		virtual jrs_sizet GetSize(void) const;
		
		// Reporting functions
		virtual void ReportAll(const jrs_i8 *pLogToFile = 0);
		virtual void ReportStatistics(void);
		virtual void ReportAllocationsMemoryOrder(const jrs_i8 *pLogToFile = 0, jrs_bool bLogEachAllocation = false);

		virtual jrs_u32 GetTotalAllocations(void) { return m_uUsedElements; }
		virtual jrs_u32 GetMaxAllocations(void) { return m_uMaxElements; }

		virtual jrs_bool HasNameAndCallstackTracing(void) { return m_bEnableMemoryTracking; }
		virtual jrs_bool HasSentinels(void) { return m_bEnableSentinel; }
	};

	class JRSMEMORYDLLEXPORT cPoolNonIntrusive : public cPoolBase
	{
		jrs_sizet *m_pBuffer;			// Main data pointer to the buffer.  Passed in by user.  16byte aligned minimum.
		jrs_sizet *m_pDataBuffer;		// Data buffer memory.
		jrs_sizet *m_pFreePtr;			// Current free pointer for the next available allocation.
		jrs_u32 m_uMaxElements;			// Maximum number of elements.
		jrs_u32 m_uElementSize;			// Element size in bytes (sizeof(jrs_sizet) byte multiple).  Rounded up based on object size and alignment.
		jrs_u32 m_uHeaderSize;			// Size of each header.  sizeof(jrs_sizet) minimum.
		jrs_u32 m_uPoolSize;			// The size of the pool.
		jrs_u32 m_uAlignment;			// Alignment of each element, sizeof(jrs_sizet) minimum.
		jrs_bool m_bThreadSafe;			// True if thread safe.
		jrs_bool m_bEnableMemoryTracking;	// Enables name and callstack tracking per object.
		jrs_u32 m_uTrackingOffset;			// Offsets for memory tracking.  Do nothing in master.
		jrs_u32 m_uUsedElements;			// Amount of used elements from the pool.

		// Friends
		friend class cHeap;
		friend class cMemoryManager;

	protected:

		// Functions
		virtual void Destroy(void);

		// Create function.  Do not call.
		jrs_bool Create(jrs_u32 uElementSize, jrs_u32 uMaxElements, cHeap *pPoolMemory, void *pDataPointer, jrs_u64 uDataPointerSize, const jrs_i8 *pName, const sPoolDetails *pDetails);

	public:

		// Constructor/Destructor
		cPoolNonIntrusive(cHeap *pPoolMemory, const jrs_i8 *pName);
		virtual ~cPoolNonIntrusive();

		
		void *AllocateMemory(void);
		void *AllocateMemory(const jrs_i8 *pName);
		void FreeMemory(void *pMemory, const jrs_i8 *pName = 0);

		// Information functions
		jrs_u32 GetNumberOfAllocations(void) const;
		virtual jrs_bool IsAllocatedFromThisPool(void *pMemory) const;
		virtual jrs_u32 GetAllocationSize(void) const;

		virtual void *GetAddress(void);
		virtual jrs_sizet GetSize(void) const;

		// Reporting functions
		virtual void ReportAll(const jrs_i8 *pLogToFile = 0);
		virtual void ReportStatistics(void);
		virtual void ReportAllocationsMemoryOrder(const jrs_i8 *pLogToFile = 0, jrs_bool bLogEachAllocation = false);

		virtual jrs_u32 GetTotalAllocations(void) { return m_uUsedElements; }
		virtual jrs_u32 GetMaxAllocations(void) { return m_uMaxElements; }

		virtual jrs_bool HasNameAndCallstackTracing(void) { return m_bEnableMemoryTracking; }
		virtual jrs_bool HasSentinels(void) { return false; }			// No sentinels
	};

}

using namespace Elephant;

#endif
