#ifndef _JRSMEMORY_H
#define _JRSMEMORY_H

// Current elephant version define.  MajorMinorRev
#define ELEPHANT_VERSION 176		

#ifndef _JRSCORETYPES_H
#include <JRSCoreTypes.h>
#endif

#ifndef _JRSMEMORY_THREADLOCKS_H
#include <JRSMemory_ThreadLocks.h>
#endif

#ifndef _JRSMEMORYTHREAD_H_
#include <JRSMemory_Thread.h>
#endif

// Error ranges.  Values between these defines determine the 3 error ranges that Elephant has defined. 
// Between MEMORYMANAGER_ERRORLEVEL_WARNING and MEMORYMANAGER_ERRORLEVEL_POTENTIALERROR are warnings.  These should not interfere with your programs operation.
// Between MEMORYMANAGER_ERRORLEVEL_POTENTIALERROR and MEMORYMANAGER_ERRORLEVEL_FATALERROR are potentially serious errors.  Elephant hasn't had a problem with these but your program might
// as it covers warnings like Out Of Memory conditions (so Elephant returns NULL).  These may cause issues if not handled correctly when not expected but generally are handled gracefully.
// Values >= MEMORYMANAGER_ERRORLEVEL_FATALERROR are serious errors and should not be ignored. Often these are caused by memory corruption to Elephants structure.
#define MEMORYMANAGER_ERRORLEVEL_WARNING ((1 << 16))
#define MEMORYMANAGER_ERRORLEVEL_POTENTIALERROR ((2 << 16))
#define MEMORYMANAGER_ERRORLEVEL_FATALERROR ((3 << 16))

// Callstack depth - should be a multiple of 4.  Otherwise an undefined error could happen
#define JRSMEMORY_CALLSTACKDEPTH 8

// Largest memory size allowed
#define JRSMEMORYINITFLAG_LARGEST 0xffffffffffffffffULL

// Memory allocation type flags
#define JRSMEMORYFLAG_NONE 0
#define JRSMEMORYFLAG_NEW 1
#define JRSMEMORYFLAG_NEWARRAY 2
#define JRSMEMORYFLAG_EDEBUG 3
#define JRSMEMORYFLAG_POOL 4
#define JRSMEMORYFLAG_POOLMEM 5
#define JRSMEMORYFLAG_HARDLINK 6
#define JRSMEMORYFLAG_HEAPNISLAB 7
#define JRSMEMORYFLAG_HEAPDEBUGTAG 8
#define JRSMEMORYFLAG_RESERVED1 9
#define JRSMEMORYFLAG_RESERVED2 10

#ifndef _JRSMEMORY_HEAP_H
#include <JRSMemory_Heap.h>
#endif

// Main Elephant namespace
namespace Elephant
{

// User call backs
typedef void (*MemoryManagerTTYOutputCB)(const jrs_i8 *pString);
typedef void (*MemoryManagerErrorCB)(const jrs_i8 *pError, jrs_u32 uErrorID);
typedef void (*MemoryManagerOutputToFile)(void *pData, int size, const jrs_i8 *pFilePathAndName, jrs_bool bAppend);
typedef void *(*MemoryManagerDefaultAllocator)(jrs_u64 uSize, void *pExtMemoryPtr);
typedef void (*MemoryManagerDefaultFree)(void *pFree, jrs_u64 uSize);
typedef jrs_sizet (*MemoryManagerDefaultSystemPageSize)(void);

// Maximum number of managed heaps defined.  Change this and recompile to increase.
static const jrs_u32 MemoryManager_MaxHeaps = 32;

// Maximum number of self managed heaps defined.  Change this and recompile to increase.
static const jrs_u32 MemoryManager_MaxUserHeaps = 32;

// Maximum number of non intrusive heaps defined.  Change this and recompile to increase.
static const jrs_u32 MemoryManager_MaxNonIntrusiveHeaps = 32;

// Forward declarations
struct sFreeBlock;
struct sAllocatedBlock;
struct sLinkedBlock;
class cPoolBase;
class cPool;
class cPoolNonIntrusive;
struct sPoolDetails;

// User call backs
typedef void (*MemoryManagerUserDetails)(cHeap::sHeapDetails &rDetails);

// Main memory manager class
JRSMEMORYALIGNPRE(128)				// Align the memory manager to 128 bytes.  Important for cache and atomic operations.
class JRSMEMORYDLLEXPORT cMemoryManager
{
	// Thread locks for each potential heap plus the logging heap.
	JRSMemory_ThreadLock g_ThreadLocks[MemoryManager_MaxHeaps + MemoryManager_MaxUserHeaps + MemoryManager_MaxNonIntrusiveHeaps];
	JRSMemory_ThreadLock m_EDThreadLock;
	JRSMemory_ThreadLock m_LVThreadLock;
	JRSMemory_ThreadLock m_ContThreadLock;
	JRSMemory_ThreadLock m_MMThreadLock;

	// Statics and singleton values for the memory manager
	static jrs_sizet m_uSmallHeapSize;
	static jrs_u32 m_uSmallHeapMaxAllocSize;
	static jrs_bool m_bEnableContinuousDump;
	static jrs_i8 m_ContinuousDumpFile[256];
	static jrs_bool m_bEnableLiveView;
	static jrs_bool m_bEnableLiveViewPostInit;
	static jrs_i32 m_iLiveViewTimeOutMS;
	volatile static jrs_bool m_bLiveViewRunning;
	static jrs_bool m_bEnhancedDebugging;
	static jrs_bool m_bEnableEnhancedDebuggingPostInit;
	static jrs_u32 m_uLiveViewPoll;
	static jrs_u32 m_uLVMaxPendingContinuousOperations;
	static jrs_bool m_bELVContinuousGrab;
	static jrs_bool m_bForceLVContinuousGrab;
	static jrs_bool m_bForceLVOverviewGrab;
	static jrs_u32 m_uEDebugTime;
	static jrs_u32 m_uEDebugPendingTime;
	static jrs_u32 m_uEDebugMaxPendingAllocations;

	jrs_bool m_bInitialized;					// True if initialized

	// Memory manager details
	jrs_u64 m_uAllocatedMemorySize;				//Allocated Memory Size
	void *m_pAllocatedMemoryBlock;			//Pointer to the allocated memory
	void *m_pUseableMemoryStart;
	void *m_pUseableMemoryEnd;
	void *m_pUsableHeapMemoryStart;
	jrs_bool m_bCustomMemoryDefined;		// TRUE if memory has been initialized by the user.
	jrs_sizet m_uSystemPageSize;

	jrs_u32 m_uHeapNum;
	cHeap *m_pHeaps[MemoryManager_MaxHeaps];
	jrs_u32 m_uUserHeapNum;
	cHeap *m_pUserHeaps[MemoryManager_MaxUserHeaps];
	jrs_u32 m_uHeapIdInfo;
	
	jrs_u32 m_uNonIntrusiveHeapNum;
	cHeapNonIntrusive *m_pNonInstrusiveHeaps[MemoryManager_MaxNonIntrusiveHeaps];

	jrs_u32 m_uPoolIdInfo;

	// Flags to handle dynamic resizing
	jrs_bool m_bResizeable;		
	jrs_u64 *m_pResizableSystemAllocs;
	jrs_u32 m_uResizableCount;

	// Enhanced debugging information
	struct sEDebug
	{
		void *pPtr;
		jrs_sizet uTime;
	};

	sEDebug *m_pEDebugBuffer;
	JRSMEMORYLOCALALIGN(volatile jrs_u32 m_EDebugBufStart, 128);
	JRSMEMORYLOCALALIGN(volatile jrs_u32 m_EDebugBufEnd, 128);

	// Live view buffer for continuous data
	struct sLVOperation
	{
		jrs_u64 address;
		jrs_u32 alignment;
		jrs_u16 idofheappool;
		jrs_u8 type;
		jrs_u8 sizeofopdata;

		jrs_u64 extraInfo0;
		jrs_u64 extraInfo1;
		jrs_u64 extraInfo2;
		jrs_u64 extraInfo3;
	};

	jrs_u8 *m_pELVDebugBuffer;
	jrs_u8 *m_pELVDebugBufferEnd;
	jrs_u32 m_uELVDebugBufferSize;
	static jrs_u16 m_uLVPort;

	JRSMEMORYLOCALALIGN(volatile jrs_u32 m_uELVDBRead, 128);
	JRSMEMORYLOCALALIGN(volatile jrs_u32 m_uELVDBWrite, 128);
	JRSMEMORYLOCALALIGN(volatile jrs_u32 m_uELVDBWriteEnd, 128);

	void AddEDebugAllocation(void *pMemoryAddress);

	// Heaps come from the start of this block
	cHeap *m_pMemoryHeaps;
	cHeap *m_pMemoryUserHeaps;
	cHeap *m_pMemorySmallHeap;
	cHeapNonIntrusive *m_pMemoryNonIntrusiveHeaps;

	// Default malloc heap
	cHeap *m_pDefaultMallocHeap;
	bool m_bOverrideMallocHeap;

	// Prevention techniques
	bool m_bAllowHeapCreationFromAddress;

	// Continuous logging enums.  Covers all the items logged
	enum eContLog
	{
		eContLog_Allocate,
		eContLog_Free,
		eContLog_ReAlloc,
		eContLog_CreateHeap,
		eContLog_ResizeHeap,
		eContLog_DestroyHeap,
		eContLog_StartLogging,
		eContLog_StopLogging,
		eContLog_Marker,
		eContLog_CreatePool,
		eContLog_DestroyPool,
		eContLog_AllocatePool,
		eContLog_FreePool,
		eContLog_Unknown = 0x7fffffff
	};

	// Callbacks for the user to fill in
	static MemoryManagerDefaultAllocator m_MemoryManagerDefaultAllocator;
	static MemoryManagerDefaultFree m_MemoryManagerDefaultFree;
	static MemoryManagerDefaultSystemPageSize m_MemoryManagerDefaultSystemPageSize;
	static MemoryManagerTTYOutputCB m_MemoryManagerTTYOutput;
	static MemoryManagerErrorCB m_MemoryManagerError;
	static MemoryManagerOutputToFile m_MemoryManagerFileOutput;
	static MemoryManagerOutputToFile m_MemoryManagerLiveViewOutput;
	static MemoryManagerUserDetails m_MemoryManagerUserDetails;

	// Static debug functions
	static void DebugOutput(const jrs_i8 *pText, ...);
	static void DebugOutputFile(const jrs_i8 *pFile, jrs_bool bCreate, const jrs_i8 *pText, ...);
	static void DebugError(jrs_u32 uErrorCode, const jrs_i8 *pText);
	static void DebugWarning(cHeap *pHeap, cPoolBase *pPool, jrs_u32 uErrorCode, const jrs_i8 *pFunction, const jrs_i8 *pText, ...);
	static void DebugWarning(cHeapNonIntrusive *pHeap, cPoolBase *pPool, jrs_u32 uErrorCode, const jrs_i8 *pFunction, const jrs_i8 *pText, ...);

	// Thread functions
	static jrs_bool JRSMemory_LiveView_SendOperations(void *pBuffer, JRSMemory_ThreadLock *pThreadLock);
	static cJRSThread::jrs_threadout JRSMemory_LiveViewThread(cJRSThread::jrs_threadin pArg);
	static cJRSThread::jrs_threadout JRSMemory_EnhancedDebuggingThread(cJRSThread::jrs_threadin pArg);

	// Private functions	
	jrs_bool InternalCreatePoolBase(jrs_u32 uElementSize, jrs_u32 uMaxElements, const jrs_i8 *pHeapName, sPoolDetails *pDetails = NULL, cHeap *pHeap = NULL);

	jrs_bool ContinuousLog_CanLog(cHeap *pHeap);
	jrs_bool ContinuousLog_CanLog(cHeapNonIntrusive *pHeap);
	void ContinuousLog_AddToBuffer(sLVOperation &rOp, void *pData);
	void ContinuousLogging_Operation(eContLog eType, cHeap *pHeap, cPoolBase *pPool, jrs_u64 uMisc);
	void ContinuousLogging_NIOperation(eContLog eType, cHeapNonIntrusive *pHeap, cPoolBase *pPool, jrs_u64 uMisc);
	void ContinuousLogging_HeapOperation(eContLog eType, cHeap *pHeap, void *pHeaderAdd, jrs_u32 uAlignment, jrs_u64 uMisc);
	void ContinuousLogging_HeapNIOperation(eContLog eType, cHeapNonIntrusive *pHeap, void *pHeaderAdd, jrs_u32 uAlignment, jrs_u64 uSize, jrs_u64 uMisc);
	void ContinuousLogging_PoolOperation(eContLog eType, cPoolBase *pPool, void *pAddress, jrs_u64 uMisc);

	void StackTrace(jrs_sizet *pCallStack, jrs_u32 uCallstackDepth, jrs_u32 uCallStackCount);
	static void StackToString(jrs_i8 *pOutputBuffer, jrs_sizet *pCallstack, jrs_u32 uCallStackCount);

	// Resizable calls
	jrs_bool InternalResize(jrs_u64 uMinimumSize);
	jrs_bool InternalResizeHeap(cHeap *pHeap, jrs_u64 uSize);

	// Friend
	friend class cHeap;
	friend class cHeapNonIntrusive;
	friend class cPoolBase;
	friend class cPool;
	friend class cPoolNonIntrusive;

public:

	// Constructor
	cMemoryManager();
	~cMemoryManager();

	// Callback initialize.  Call all before Initialize
	static void InitializeCallbacks(MemoryManagerTTYOutputCB TTYOutput, MemoryManagerErrorCB ErrorHandle, MemoryManagerOutputToFile FileOutput = 0);
	static void InitializeAllocationCallbacks(MemoryManagerDefaultAllocator DefaultAllocator, MemoryManagerDefaultFree DefaultFree, MemoryManagerDefaultSystemPageSize DefaultPageSize);
	static void InitializeSmallHeap(jrs_sizet uSmallHeapSize, jrs_u32 uMaxAllocSize, cHeap::sHeapDetails *pDetails = NULL);
	static void InitializeContinuousDump(const jrs_i8 *pFileNameAndPath, jrs_bool bDefaultEnable = true);
	static void InitializeLiveView(jrs_u32 uMilliSeconds = 33, jrs_u32 uPendingContinuousOperations = 1024, jrs_bool bAllowUserPostInit = false, jrs_i32 iExternalConnectionTimeOutMS = 0, jrs_u16 uPort = 7133);
	static void InitializeEnhancedDebugging(jrs_bool bEnhancedDebugging = false, jrs_u32 uDeferredTimeMS = 66, jrs_u32 uMaxAllocation = 1024 * 32, jrs_bool bAllowUserPostInit = false);

	// Initialize and destroy
	jrs_bool Initialize(jrs_u64 uMemorySize, jrs_u64 uDefaultHeapSize = JRSMEMORYINITFLAG_LARGEST, jrs_bool bFindMaxClosestToSize = true, void *pMemory = NULL);
	jrs_bool UserInitializePostFeatures(void);
	jrs_bool Destroy(void);

	// Heap
	cHeap *CreateHeap(jrs_u64 uHeapSize, const jrs_i8 *pHeapName, cHeap::sHeapDetails *pHeapDetails);
	cHeap *CreateHeap(void *pMemoryAddress, jrs_u64 uHeapSize, const jrs_i8 *pHeapName, cHeap::sHeapDetails *pHeapDetails);
	cHeapNonIntrusive *CreateNonIntrusiveHeap(void *pMemoryAddress, jrs_sizet uHeapSize, cHeap *pHeap, const jrs_i8 *pHeapName, cHeapNonIntrusive::sHeapDetails *pHeapDetails);
	cHeapNonIntrusive *CreateNonIntrusiveHeap(jrs_sizet uHeapSize, cHeap *pHeap, const jrs_i8 *pHeapName, cHeapNonIntrusive::sHeapDetails *pHeapDetails);

	jrs_bool ResizeHeap(cHeap *pHeap, jrs_u64 uSize);
	jrs_bool ResizeHeapToLastAllocation(cHeap *pHeap);
	cHeap *FindHeap(const jrs_i8 *pHeapName);
	cHeapNonIntrusive *FindNonIntrusiveHeap(const jrs_i8 * pHeapName);
	cHeap *GetHeap(jrs_u32 iIndex);
	cHeap *GetUserHeap(jrs_u32 iIndex);
	cHeapNonIntrusive *GetNIHeap(jrs_u32 iIndex);
	jrs_u32 GetNumHeaps(void) const;
	jrs_u32 GetNumUserHeaps(void) const;
	jrs_u32 GetMaxNumHeaps(void) const;
	jrs_u32 GetMaxNumUserHeaps(void) const;
	jrs_u32 GetMaxNumNIHeaps(void) const;
	jrs_bool DestroyHeap(cHeap *pHeap);
	jrs_bool DestroyNonIntrusiveHeap(cHeapNonIntrusive *pHeap);
	cHeap *FindHeapFromMemoryAddress(void *pMemory) const;
	cHeapNonIntrusive *FindNIHeapFromMemoryAddress(void *pMemory) const;
	cHeap *GetDefaultHeap(void);

	// Pool
	cPool *CreatePool(jrs_u32 uElementSize, jrs_u32 uMaxElements, const jrs_i8 *pPoolName, sPoolDetails *pDetails = NULL, cHeap *pHeap = NULL);
	cPoolNonIntrusive *CreatePoolNonIntrusive(jrs_u32 uElementSize, jrs_u32 uMaxElements, const jrs_i8 *pPoolName, void *pDataPointer, jrs_u64 uDataPointerSize, sPoolDetails *pDetails = NULL, cHeap *pHeap = NULL);
	void DestroyPool(cPoolBase *pPool);

	// Allocation functions
	void *Malloc(jrs_sizet uSizeInBytes, jrs_u32 uAlignment = 0);
	void *Malloc(jrs_sizet uSizeInBytes, jrs_u32 uAlignment, jrs_u32 uFlag, const jrs_i8 *pText, const jrs_u32 uExternalId = 0);
	void Free(void *pMemory, jrs_u32 uFlag = JRSMEMORYFLAG_NONE);
	void Free(void *pMemory, jrs_u32 uFlag, const jrs_i8 *pText);
	void *Realloc(void *pMemory, jrs_sizet uSizeInBytes, jrs_u32 uAlignment = 0, jrs_u32 uFlag = JRSMEMORYFLAG_NONE, const jrs_i8 *pText = NULL);

	// Reclaiming
	void Reclaim(void);

	// General functions
	jrs_u64 GetFreeUsableMemory(void) const;
	jrs_bool IsInitialized(void) const;
	jrs_sizet SizeofAllocation(void *pMemory) const;
	jrs_sizet SizeofAllocationAligned(void *pMemory) const;
	static jrs_sizet SizeofAllocatedBlock(void);
	static jrs_sizet SizeofFreeBlock(void);
	jrs_u32 GetAllocationFlag(void *pMemory) const;
	void SetMallocDefaultHeap(cHeap *pHeap); 
	jrs_sizet GetSystemPageSize(void) const;
	jrs_u16 GetLVPortNumber(void) const;

	// Logging markers
	void EnableLogging(jrs_bool bEnable);
	jrs_bool IsLoggingEnabled(void) const;
	void LogMemoryMarker(const jrs_i8 *pMarkerName);

	// Check for errors
	void CheckForErrors(void);

	// Report functions
	void ReportAll(const jrs_i8 *pLogToFile = 0, jrs_bool includeFreeBlocks = FALSE, jrs_bool displayCallStack = FALSE);
	void ReportStatistics(jrs_bool bAdvanced = false);
	void ReportAllocationsMemoryOrder(const jrs_i8 *pLogToFile = 0, jrs_bool includeFreeBlocks = FALSE, jrs_bool displayCallStack = FALSE);

	void ReportAllToGoldfish(void);
	void ReportContinuousStartToGoldfish(void);
	void ReportContinuousStopToGoldfish(void);

	// Reset
	void ResetHeapStatistics(void);

	// Singleton get

	// Define and recompile if you want a global somewhere. The reason it isn't is because there
	// is no way to make it initialize first for all compilers.  This means doing it statically
	// is more generic at minimal cost.
#ifdef JRSMEMORYGLOBALDECLARATION
	static cMemoryManager &Get(void);
#else
	static cMemoryManager &Get(void) 
	{ 
		static cMemoryManager sMemoryManager;
		return sMemoryManager;
	}
#endif
}
JRSMEMORYALIGNPOST(128)
;

} // Elephant namespace

// Enable Elephant namespace
using namespace Elephant;

#endif	// _JRSMEMORY_H
