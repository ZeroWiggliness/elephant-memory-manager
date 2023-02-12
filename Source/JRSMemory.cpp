// Needed for string functions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <JRSMemory.h>
#include <JRSMemory_Thread.h>
#include <JRSMemory_Pools.h>
#include "JRSMemory_ErrorCodes.h"
#include "JRSMemory_Internal.h"

// Extern the main thread
extern cJRSThread::jrs_threadout JRSMemory_LiveViewThread(cJRSThread::jrs_threadin pArg);

// Inline defines
#define HEAP_FULLSIZE_CALC(x, min) (x > min ? ((x + 0xf) & ~(0xf)) : min)

// Elephant namespace.  using Elephant declared in JRSMemory.h.
namespace Elephant
{
	// Global memory manager construction.  Put here to enable constructor reordering.
#ifdef JRSMEMORYGLOBALDECLARATION
	// Define and recompile if you want a global somewhere. The reason it isn't is because there
	// is no way to make it initialize first for all compilers.  This means doing it statically
	// is more generic at minimal cost.
	cMemoryManager g_MemoryManager;

	cMemoryManager &cMemoryManager::Get(void)
	{
		return g_MemoryManager;
	}
#endif

	// Global callbacks
	MemoryManagerTTYOutputCB cMemoryManager::m_MemoryManagerTTYOutput = 0;
	MemoryManagerErrorCB  cMemoryManager::m_MemoryManagerError = 0;
	MemoryManagerOutputToFile cMemoryManager::m_MemoryManagerFileOutput = 0;
	MemoryManagerOutputToFile cMemoryManager::m_MemoryManagerLiveViewOutput = MemoryManagerLiveViewTransfer;
	MemoryManagerDefaultAllocator cMemoryManager::m_MemoryManagerDefaultAllocator = MemoryManagerDefaultSystemAllocator;
	MemoryManagerDefaultFree cMemoryManager::m_MemoryManagerDefaultFree = MemoryManagerDefaultSystemFree;
	MemoryManagerDefaultSystemPageSize cMemoryManager::m_MemoryManagerDefaultSystemPageSize = MemoryManagerSystemPageSize;
	MemoryManagerUserDetails cMemoryManager::m_MemoryManagerUserDetails = 0;

	// Small heap size.
	jrs_sizet cMemoryManager::m_uSmallHeapSize = 0;

	// Small heap details.
	cHeap::sHeapDetails m_SmallHeapDetails;

	// Continuous logging enabled or not flag.
	jrs_bool cMemoryManager::m_bEnableContinuousDump = false;

	// Live view enabled or not flag.
	jrs_bool cMemoryManager::m_bEnableLiveView = false;

	// Live view post init flag
	jrs_bool cMemoryManager::m_bEnableLiveViewPostInit = false;

	// Live view port
	jrs_u16 cMemoryManager::m_uLVPort = 7133;

	// Live view is running flag
	volatile jrs_bool cMemoryManager::m_bLiveViewRunning = false;

	// Live view timer wait time
	jrs_i32 cMemoryManager::m_iLiveViewTimeOutMS;

	// Enhanced debugging.
	jrs_bool cMemoryManager::m_bEnhancedDebugging = false;

	// Enhanced debugging post init flag.
	jrs_bool cMemoryManager::m_bEnableEnhancedDebuggingPostInit = false;
	
	// Live view poll time.
	jrs_u32 cMemoryManager::m_uLiveViewPoll = 33;

	// Live view continuous debug storage
	jrs_u32 cMemoryManager::m_uLVMaxPendingContinuousOperations = 0;

	// Collect the live view continuous information
	jrs_bool cMemoryManager::m_bELVContinuousGrab = false;

	// Force live view continuous information from elephant
	jrs_bool cMemoryManager::m_bForceLVContinuousGrab = false;

	// Force live view continuous information from elephant
	jrs_bool cMemoryManager::m_bForceLVOverviewGrab = false;

	// Live view thread
	cJRSThread g_MemoryManagerNetworkThread;	

	// Enhanced debugging thread
	cJRSThread g_MemoryManagerEnhancedDebugThread;

	// Continuous dump file name
	jrs_i8 cMemoryManager::m_ContinuousDumpFile[256];

	// Report heap enabled.
	jrs_bool g_ReportHeap = false;

	// Report heap create.
	jrs_bool g_ReportHeapCreate = true;

	// The base address for map file look up.  By default all libs will have no problem using MemoryManagerPlatformInit as this is a static function.
	// Complications may arise with dll's and system libraries and INCREMENTAL linking.  These will generally be dealt with else where.
	// This is set in the constructor because platforms that use relocatable code cannot create the address at compile time.
	jrs_u64 g_uBaseAddressOffsetCalculation = 0;

	// Set to true when the system can decode symbols from an address. Set in the constructor and transfered when the user.  Should be set in the 
	// platform init function.
	jrs_bool g_bCanDecodeSymbols = false;

	//  Description:
	//      This function accepts C style function pointers to pass back errors from the memory manager. Must be called before Initialize.
	//  See Also:
	//      Initialize
	//  Arguments:
	//      TTYOutput - This callback outputs text to the log TTY of your choice. 
	//      ErrorHandle - This callback executes any error handling.
	//		FileOutput - This callback handles file output.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Initializes Elephants callbacks.
	void cMemoryManager::InitializeCallbacks(MemoryManagerTTYOutputCB TTYOutput, MemoryManagerErrorCB ErrorHandle, MemoryManagerOutputToFile FileOutput)
	{
		MemoryWarning(!cMemoryManager::Get().IsInitialized(), JRSMEMORYERROR_CALLEDAFTERINITIALIZE, "This function should be called before Initialization.");
		cMemoryManager::m_MemoryManagerTTYOutput = TTYOutput;
		cMemoryManager::m_MemoryManagerError = ErrorHandle;
		cMemoryManager::m_MemoryManagerFileOutput = FileOutput;
	}

	//  Description:
	//      This function accepts C style function pointers to change the default system allocator and free for Initialize. Must be called before Initialize.
	//  See Also:
	//      Initialize
	//  Arguments:
	//      DefaultAllocator - Default allocation call.
	//      DefaultFree - Default free call.
	//		DefaultPageSize - Default page size call.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Initializes Elephants allocation and free main pool functions.
	void cMemoryManager::InitializeAllocationCallbacks(MemoryManagerDefaultAllocator DefaultAllocator, MemoryManagerDefaultFree DefaultFree, MemoryManagerDefaultSystemPageSize DefaultPageSize)
	{
		MemoryWarning(!cMemoryManager::Get().IsInitialized(), JRSMEMORYERROR_CALLEDAFTERINITIALIZE, "This function should be called before Initialization.");
		cMemoryManager::m_MemoryManagerDefaultAllocator = DefaultAllocator;
		cMemoryManager::m_MemoryManagerDefaultFree = DefaultFree;
		cMemoryManager::m_MemoryManagerDefaultSystemPageSize = DefaultPageSize;
	}

	//  Description:
	//      This function initializes the small heap which will then automatically be used by Malloc.  Recommended size is atleast
	//		256k.  Must be called before Initialize.
	//  See Also:
	//      Initialize, Malloc
	//  Arguments:
	//      uSmallHeapSize - The size of the heap.
	//      uMaxAllocSize - The maximum allocation size to divert to the small heap.
	//		pDetails - Override the small heap details. NULL will use the defaults. uMaxAllocSize takes precedence the sHeapDetails MaxAllocSize.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Initializes Elephants small heap.
	void cMemoryManager::InitializeSmallHeap(jrs_sizet uSmallHeapSize, jrs_u32 uMaxAllocSize, cHeap::sHeapDetails *pDetails)
	{
		MemoryWarning(!cMemoryManager::Get().IsInitialized(), JRSMEMORYERROR_CALLEDAFTERINITIALIZE, "This function should be called before Initialization.");
		MemoryWarning(uSmallHeapSize >= 1024, JRSMEMORYERROR_HEAPTOSMALL, "Small heap must be larger than 1k. Recommend 64-256k minimum.");
		MemoryWarning(!(uSmallHeapSize & 0xf), JRSMEMORYERROR_INVALIDALIGN, "Small heap must be a 16byte aligned size");

		cHeap::sHeapDetails details;
		m_SmallHeapDetails = details;
		m_uSmallHeapSize = uSmallHeapSize;
		if(pDetails)
			m_SmallHeapDetails = *pDetails;
		m_SmallHeapDetails.uMaxAllocationSize = uMaxAllocSize;
	}

	//  Description:
	//      Initializes the filename of the continuous dump. Must be called before Initialize.  The string will be passed
	//		to your file callback whenever logging is enabled.
	//  See Also:
	//      Initialize
	//  Arguments:
	//      pFileNameAndPath - The full name and path of the file you want as a continuous log file.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Initializes the filename for the continuous dump.
	void cMemoryManager::InitializeContinuousDump(const jrs_i8 *pFileNameAndPath, jrs_bool bDefaultEnable)
	{
		MemoryWarning(!cMemoryManager::Get().IsInitialized(), JRSMEMORYERROR_CALLEDAFTERINITIALIZE, "This function should be called before Initialization.");
		MemoryWarning(pFileNameAndPath, JRSMEMORYERROR_INVALIDFILENAME, "Filename is invalid");

		m_bEnableContinuousDump = bDefaultEnable;
		strcpy(m_ContinuousDumpFile, pFileNameAndPath);
	}

	//  Description:
	//      Initializes the live view network thread.  The thread is of low priority and is polled roughly every uMilliSeconds.  The thread will always be active but do very little if Goldfish isnt running.
	//  See Also:
	//      Initialize
	//  Arguments:
	//		uMilliSeconds - Time to poll in MilliSeconds.  Minimum time is 16.  Any lower time will be capped to this.
	//		uPendingContinuousOperations - Number of continuous operations to hold in a buffer.  16k start time.  If you get significant stalls due to this you should increase the number.
	//		bAllowUserPostInit - Set to true to post initialize this function.  This may be required if memory is required for various OS threads that rely on Elephant.
	//		uExternalConnectionTimeOutMS - Set time in MS to wait for an external connection from Goldfish/LiveView.  Default 0.  -1 for indefinite wait.
	//		uPort - Sets the default port to connect to.  Default 7133.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Initializes the live view network thread.
	void cMemoryManager::InitializeLiveView(jrs_u32 uMilliSeconds, jrs_u32 uPendingContinuousOperations, jrs_bool bAllowUserPostInit, jrs_i32 iExternalConnectionTimeOutMS, jrs_u16 uPort)
	{
		MemoryWarning(!cMemoryManager::Get().IsInitialized(), JRSMEMORYERROR_CALLEDAFTERINITIALIZE, "This function should be called before Initialization.");

#ifdef JRSMEMORY_HASSOCKETS
		m_bEnableLiveView = true;
		m_bEnableLiveViewPostInit = bAllowUserPostInit;
		if(uMilliSeconds < 16)
			uMilliSeconds = 16;
		m_uLiveViewPoll = uMilliSeconds;
		m_uLVMaxPendingContinuousOperations = uPendingContinuousOperations;
		m_iLiveViewTimeOutMS = iExternalConnectionTimeOutMS;
		m_bLiveViewRunning = false;
		m_uLVPort = uPort;
#endif
	}

	//  Description:
	//      Initializes the enhanced debugging thread.  The thread is of low priority and is polled roughly every 16ms.  This thread checks the memory has not been used after it was freed.
	//		It does however mean that some memory will remain 'valid' until it has been checked and confirmed as unused else where. This means that memory consumption will be slightly higher
	//		than normal but is the ultimate debugging aid when tracking down memory corruption, especially over multiple threads.
	//
	//		Must be called before Initialize.
	//  See Also:
	//      Initialize
	//  Arguments:
	//      bEnhancedDebugging - true to enable enhanced debugging features of Elephant.
	//		uDeferredTimeMS - The amount of time in MilliSeconds before Elephant decides to remove the memory (approximate - it may be longer but never shorter). Default 66.
	//		uMaxAllocation - Maximum number of allocations to cache at any one time.  When this limit is reached stalls may occur.  Default is 32k.  
	//		bAllowUserPostInit - Set to true to post initialize this function.  This may be required if memory is required for various OS threads that rely on Elephant.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Initializes the enhanced debugging thread.
	void cMemoryManager::InitializeEnhancedDebugging(jrs_bool bEnhancedDebugging, jrs_u32 uDeferredTimeMS, jrs_u32 uMaxAllocation, jrs_bool bAllowUserPostInit)
	{
		MemoryWarning(!cMemoryManager::Get().IsInitialized(), JRSMEMORYERROR_CALLEDAFTERINITIALIZE, "This function should be called before Initialization.");

		m_bEnableEnhancedDebuggingPostInit = bAllowUserPostInit;
		m_uEDebugPendingTime = uDeferredTimeMS;
		m_bEnhancedDebugging = bEnhancedDebugging;
		m_uEDebugMaxPendingAllocations = uMaxAllocation;
	}

	//  Description:
	//      Private constructor for the memory manager.  May not be called by the user.
	//  See Also:
	//      Initialize, Get
	//  Arguments:
	//      None.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Private constructor for the memory manager.
	cMemoryManager::cMemoryManager() : m_bInitialized(false)
	{
		g_uBaseAddressOffsetCalculation = (jrs_u64)MemoryManagerPlatformInit;
	}

	//  Description:
	//      Destructor for the memory manager.  Elephant must be destroyed with the Destroy function before global destruction.  Elephant will 
	//		warn if this has not happened.
	//  See Also:
	//      Initialize, Destroy
	//  Arguments:
	//      None.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Destructor for the memory manager.
	cMemoryManager::~cMemoryManager()
	{
		if(m_bInitialized)
			Destroy();
	}

	//  Description:
	//      Initializes Elephant. This function will automatically create the heap if needed and try to 
	//      automatically get as much memory as requested if the maximum size isn't available by reducing the 
	//		size by 64k until it can be achieved.  By default Initialize will try and use all the memory allocated
	//		for one heap.  Any default heap will be called "DefaultHeap".  bFindMaxClosestToSize defaults to true.
	//		
	//		pMemory is only required if you wish Elephant to initialize to an area of memory defined by yourself.  For some
	//		systems this may be beneficial or the only method possible if the system allocation routines are not possible at
	//		this stage. pMemory must be the or larger than uMemorySize and bFindMaxClosestToSize must be false.  
	//
	//		Actual size	of memory available to the user may not be uMemorySize after initialization as Elephant consumes a small
	//		amount for itself.
	//  See Also:
	//      Destroy, CreateHeap
	//  Arguments:
	//      uMemorySize - The total size in bytes.  16byte multiple.
	//      uDefaultHeapSize - The size of the default heap. Set to JRSMEMORYINITFLAG_LARGEST to use all of the available
	//							memory. Set to 0 if you want no default heap created.
	//      bFindMaxClosestToSize - TRUE to find the largest amount of memory available if it it cannot allocate uMemorySize.
	//								FALSE to fail if uMemorySize cannot be allocated.
	//		pMemory - A memory pointer with which use by Elephant.  This overrides the default allocators.
	//  Return Value:
	//      TRUE if successfully created.
	//		FALSE for failure.
	//  Summary:
	//      Initializes Elephant Memory Manager.
	jrs_bool cMemoryManager::Initialize(jrs_u64 uMemorySize, jrs_u64 uDefaultHeapSize, jrs_bool bFindMaxClosestToSize, void *pMemory)
	{
		// Check if initialized
		if(m_bInitialized)
		{
			MemoryWarning(!m_bInitialized, JRSMEMORYERROR_NOTINITIALIZED, "Memory manager is already initialized.");
			return true;
		}	

		// Some basic tests
		MemoryWarning(32 == JRSCountTrailingZero(0), JRSMEMORYERROR_CTZIMPLEMENTATIONFAIL, "Count Trailing Zero failed.  For 0 answer should be 32.");
		MemoryWarning(0 == JRSCountTrailingZero(1), JRSMEMORYERROR_CTZIMPLEMENTATIONFAIL, "Count Trailing Zero failed.  For 1 answer should be 0.");
		MemoryWarning(1 == JRSCountTrailingZero(2), JRSMEMORYERROR_CTZIMPLEMENTATIONFAIL, "Count Trailing Zero failed.  For 2 answer should be 1.");
		MemoryWarning(31 == JRSCountTrailingZero(0x80000000), JRSMEMORYERROR_CTZIMPLEMENTATIONFAIL, "Count Trailing Zero failed.  For 0x80000000 answer should be 32.");

		MemoryWarning(0 == JRSCountLeadingZero(0), JRSMEMORYERROR_CLZIMPLEMENTATIONFAIL, "Count Leading Zero failed.  For 0 answer should be 0.");
		MemoryWarning(0 == JRSCountLeadingZero(1), JRSMEMORYERROR_CLZIMPLEMENTATIONFAIL, "Count Leading Zero failed.  For 1 answer should be 0.");
		MemoryWarning(1 == JRSCountLeadingZero(2), JRSMEMORYERROR_CLZIMPLEMENTATIONFAIL, "Count Leading Zero failed.  For 2 answer should be 1.");
		MemoryWarning(31 == JRSCountLeadingZero(0x80000000), JRSMEMORYERROR_CLZIMPLEMENTATIONFAIL, "Count Leading Zero failed.  For 0x80000000 answer should be 31.");
		
		// Clear all the heaps
		for(jrs_u32 i = 0; i < MemoryManager_MaxHeaps; i++)
			m_pHeaps[i] = 0;

		for(jrs_u32 i = 0; i < MemoryManager_MaxUserHeaps; i++)
			m_pUserHeaps[i] = 0;

		for(jrs_u32 i = 0; i < MemoryManager_MaxNonIntrusiveHeaps; i++)
			m_pNonInstrusiveHeaps[i] = 0;

		// Clear the count
		m_uHeapNum = m_uUserHeapNum = m_uNonIntrusiveHeapNum = m_uHeapIdInfo = m_uPoolIdInfo = 0;

		// Init the data to the actual sizes that we can actually use
		const jrs_u32 HeapSizes = (((sizeof(cHeap) * (MemoryManager_MaxHeaps + MemoryManager_MaxUserHeaps)) + (sizeof(cHeapNonIntrusive) * MemoryManager_MaxNonIntrusiveHeaps)) + 0xf) & ~0xf;		// Size is aligned to 16bytes
		const jrs_u32 EDebugSize = ((sizeof(sEDebug) * m_uEDebugMaxPendingAllocations) + 0xf) & ~0xf;
		const jrs_u32 ELVDebugSize = ((jrs_u32)SizeofFreeBlock() + sizeof(sLVOperation)) * (m_uLVMaxPendingContinuousOperations);
		jrs_u32 ResizableSystemStore = 0;		

		// Default page size of 64k
		m_uSystemPageSize = m_MemoryManagerDefaultSystemPageSize();		
		MemoryWarning(((m_uSystemPageSize != 0) && !(m_uSystemPageSize & (m_uSystemPageSize - 1))) && (m_uSystemPageSize >= 4096), JRSMEMORYERROR_INVALIDALIGN, "System page size is not aligned to a power of 2 or is 0 or is smaller than 4096bytes.  Errors may occur later.");

		// A memory size of JRSMEMORYINITFLAG_LARGEST puts Elephant into resize mode.  Not all platforms support this.
		m_bResizeable = FALSE;
		m_uResizableCount = 0;
		m_pResizableSystemAllocs = NULL;
		if(uMemorySize == JRSMEMORYINITFLAG_LARGEST)
		{
			if(bFindMaxClosestToSize)
			{
				MemoryWarning(!bFindMaxClosestToSize, JRSMEMORYERROR_INVALIDARGUMENTS, "Elephant cannot be initialized with the size set to TRUE in resizable mode.");
				return FALSE;
			}

			// Check if we can work in this mode
			if(pMemory)
			{
				MemoryWarning(pMemory, JRSMEMORYERROR_USERMEMORYADDRESSBUTRESIZEMODE, "Elephant cannot run in resize mode if the main pool is a user supplied address.");
				return FALSE;
			}

			// Memory size only needs to be the size for data handling in elephant.  Heaps will handle the rest.
			// Determine a size to hold all the systems ptrs used in resizable mode.  Set it to 256GB into 32MB chunks. 32MB being the smallest we allow heaps to resize.
			m_bResizeable = TRUE;
			ResizableSystemStore = sizeof(jrs_sizet) * 2 * ((256 * 1024) / 32);	
			uMemorySize = (HeapSizes + EDebugSize + ELVDebugSize + ResizableSystemStore) + 128;	// for padding later
			uMemorySize = (uMemorySize + (m_uSystemPageSize - 1)) & ~(m_uSystemPageSize - 1);
		}
		
		// Check the memory size isnt too large.  Some compilers cannot set the values correctly.
		// i.e (jrs_u64)(1024 * 1024 * 1024 * 3) gets converted to a negative.
		if(uMemorySize >= 0x800000000000LL)
		{
			MemoryWarning(uMemorySize < 0x800000000000LL, JRSMEMORYERROR_INITIALIZEINVALIDSIZE, "uMemorySize is larger than 0x800000000000 (131GB).  This most often occurs due to a 32bit compiler converting a signed 32bit variable to an unsigned 64bit variable. Input uMemorySize is 0x%llu.", uMemorySize);
			return FALSE;
		}

		if(uMemorySize < (HeapSizes + EDebugSize + ELVDebugSize + ResizableSystemStore))
		{
			// Check the size requested will fit within MemoryManager_MaxHeaps
			MemoryWarning(uMemorySize >= (HeapSizes + EDebugSize + ELVDebugSize + ResizableSystemStore), JRSMEMORYERROR_INITIALIZESIZETOSMALL, "uMemorySize must be initialized with at least %dk", (HeapSizes + EDebugSize + ELVDebugSize + ELVDebugSize + ResizableSystemStore + 1024) >> 10);
			return FALSE;
		}
		
		// Create the memory block for use as our heap
		m_bCustomMemoryDefined = FALSE;
		if(bFindMaxClosestToSize)
		{
			// Not if we want out own memory pool with this configuration
			if(pMemory)
			{
				MemoryWarning(!pMemory, JRSMEMORYERROR_INVALIDARGUMENTS, "Cannot initialize Elephant with bFindMaxClosestToSize = true and a valid pMemory pointer.");
				return FALSE;
			}

			// Loops through in page size chunks
			m_pAllocatedMemoryBlock = 0;
			while(!m_pAllocatedMemoryBlock)
			{
				m_pAllocatedMemoryBlock = m_MemoryManagerDefaultAllocator(uMemorySize, NULL);
				m_uAllocatedMemorySize = uMemorySize;

				uMemorySize -= m_uSystemPageSize;
			}
		}
		else
		{
			//Allocate the block of memory here
			if(!pMemory)
				m_pAllocatedMemoryBlock = m_MemoryManagerDefaultAllocator(uMemorySize, NULL);
			else
			{
				m_bCustomMemoryDefined = TRUE;
				m_pAllocatedMemoryBlock = pMemory;
			}

			if(!m_pAllocatedMemoryBlock)
			{
				MemoryWarning(0, JRSMEMORYERROR_INITIALIZEOOM, "Cannot Allocate MemoryManager size of 0x%llx bytes.  Please make smaller.", uMemorySize);
				return false;
			}

			// Set the size
			m_uAllocatedMemorySize = uMemorySize;
		}

		// Create the start and align it to 128/16 bytes just to be sure.
		void *pAlignedUsableStart = (void *)(((jrs_sizet)m_pAllocatedMemoryBlock + 0x7f) & ~0x7f);

		m_pUseableMemoryStart = (void *)((jrs_i8 *)pAlignedUsableStart + HeapSizes + EDebugSize + ELVDebugSize + ResizableSystemStore);
		m_pUseableMemoryStart = (void *)(((jrs_sizet)m_pUseableMemoryStart + 0xf) & ~0xf);
		m_pUseableMemoryEnd = (void *)((jrs_i8 *)m_pUseableMemoryStart + (m_uAllocatedMemorySize - HeapSizes - EDebugSize - ELVDebugSize - ResizableSystemStore));

		// Set the heap starting memory
		m_pUsableHeapMemoryStart = m_pUseableMemoryStart;

		// Heaps come from the start of this block
		m_pMemoryHeaps = (cHeap *)m_pAllocatedMemoryBlock;
		m_pMemoryUserHeaps = m_pMemoryHeaps + MemoryManager_MaxHeaps;
		m_pMemoryNonIntrusiveHeaps = (cHeapNonIntrusive *)(m_pMemoryUserHeaps + MemoryManager_MaxUserHeaps);
		
		// Create the small heap if needed
		m_pMemorySmallHeap = 0;

		// Default malloc heap
		m_pDefaultMallocHeap = 0;
		m_bOverrideMallocHeap = false;

		// Initialize any thing for platform specifics.
		MemoryManagerPlatformInit();

		// Its now initialized
		m_bInitialized = true;

		// Initialize the live view thread if needed
#ifndef MEMORYMANAGER_MINIMAL

#ifdef JRSMEMORY_HASSOCKETS
		m_pELVDebugBuffer = NULL;
		m_uELVDBRead = m_uELVDBWrite = m_uELVDBWriteEnd = 0;
		m_bLiveViewRunning = false;
		if(m_bEnableLiveView)
		{
			// We use X amount of memory to store the pointers to free the memory
			m_pELVDebugBuffer = (jrs_u8 *)((jrs_i8 *)m_pMemoryHeaps + HeapSizes + EDebugSize);
			m_pELVDebugBufferEnd = (m_pELVDebugBuffer + ELVDebugSize);
			m_uELVDebugBufferSize = ELVDebugSize;

			// Only for non post initialized builds
			if(!m_bEnableLiveViewPostInit)
			{
				g_MemoryManagerNetworkThread.Create(JRSMemory_LiveViewThread, &m_bEnableLiveView, cJRSThread::eJRSPriority_Low, 4, 32 * 1024);
				g_MemoryManagerNetworkThread.Start();

				if(m_iLiveViewTimeOutMS != 0)
				{
					DebugOutput("Elephant Memory Manager waiting for LiveViewConnection.");
				
					// Wait for connection
					while(m_iLiveViewTimeOutMS == -1 || m_iLiveViewTimeOutMS > 0)
					{
						// Is it running yet
						if(m_bLiveViewRunning)
							break;

						// No, sleep
						JRSThread::SleepMilliSecond(10);
						if(m_iLiveViewTimeOutMS != -1)
							m_iLiveViewTimeOutMS -= m_iLiveViewTimeOutMS > 10 ? 10 : m_iLiveViewTimeOutMS;
					}

					// Wait a bit longer, LiveView does some handshaking
					JRSThread::SleepMilliSecond(m_uLiveViewPoll * 100);
				}
			}
		}
#endif

		// Enhanced debugging thread if active
		m_pEDebugBuffer = NULL;
		m_EDebugBufStart = 0;
		m_EDebugBufEnd = 0;
		if(m_bEnhancedDebugging)
		{
			// We use X amount of memory to store the pointers to free the memory
			m_pEDebugBuffer = (sEDebug *)((jrs_i8 *)m_pMemoryHeaps + HeapSizes);

			// Initialize and start the thread
			if(!m_bEnableEnhancedDebuggingPostInit)
			{
				g_MemoryManagerEnhancedDebugThread.Create(JRSMemory_EnhancedDebuggingThread, &m_bEnhancedDebugging, cJRSThread::eJRSPriority_Low, 4, 32 * 1024);
				g_MemoryManagerEnhancedDebugThread.Start();
			}
		}
#endif
		// Set the resizable buffer up
		if(m_bResizeable)
		{
			m_pResizableSystemAllocs = (jrs_u64 *)((jrs_i8 *)m_pMemoryHeaps + HeapSizes + EDebugSize + ELVDebugSize);
		}

		int versionRev = (ELEPHANT_VERSION % 10);
		int versionMin = (ELEPHANT_VERSION % 100) - versionRev;
		int versionMaj = ELEPHANT_VERSION - versionMin - versionRev;	

		DebugOutput("Elephant Memory Manager Initialized with %dMB", (jrs_u32)(m_uAllocatedMemorySize / (1024 * 1024)));
		DebugOutput("Elephant Memory Manager Name And Callstack Base Address is 0x%llx", g_uBaseAddressOffsetCalculation);
		DebugOutput("Elephant Memory Manager Mode: %s, Has small heap: %s, %s, Version: %d.%d.%d", m_bResizeable ? "Resizable Mode" : "Fixed Size Mode", m_uSmallHeapSize > 0 ? "Yes" : "No", sizeof(jrs_sizet) > 4 ? "64bit" : "32Bit", versionMaj / 100, versionMin / 10, versionRev);

		// Do continuous logging
		ContinuousLogging_Operation(eContLog_StartLogging, NULL, NULL, 0);

		// Create the small heap
		if(m_uSmallHeapSize)
		{
			cHeap::sHeapDetails SmallHeapDetails = m_SmallHeapDetails;
			m_pMemorySmallHeap = CreateHeap(m_uSmallHeapSize, "SmallHeap", &SmallHeapDetails);
		}

		//Sometimes we want the heap to swallow up everything
		if(!uDefaultHeapSize)
		{
			// Create non heap
			//	uDefaultHeapSize = GetFreeUsableMemory();
		}
		else if(uDefaultHeapSize == 0xffffffffffffffffLL)
		{
			uDefaultHeapSize = m_bResizeable ? (32 << 20) : GetFreeUsableMemory();
			CreateHeap(uDefaultHeapSize, "DefaultHeap", 0);
		}
		else
		{
			//Create the default heap. 
			CreateHeap(uDefaultHeapSize, "DefaultHeap", 0);
		}
		
		//Memory manager completed successfully
		return true;
	}

	//  Description:
	//		Internal call used to resize the heap.  Private.
	//  See Also:
	//		
	//  Arguments:
	//		pHeap - The heap to resize.
	//		uSize - Size in bytes of the heap to extend.  This may be made larger due to page sizes.
	//  Return Value:
	//      TRUE if successful, FALSE otherwise.
	//  Summary:
	//      Resizes a heap in resizable mode.  May interleave memory.
	jrs_bool cMemoryManager::InternalResizeHeap(cHeap *pHeap, jrs_u64 uSize)
	{
		if(!m_bResizeable)
			return FALSE;

		jrs_sizet uSystemPageSize = pHeap->m_systemPageSize();
		MemoryWarning(((uSystemPageSize != 0) && !(uSystemPageSize & (uSystemPageSize - 1))) && (uSystemPageSize >= 4096), JRSMEMORYERROR_INVALIDALIGN, "System page size is not aligned to a power of 2 or is 0 or is smaller than 4096bytes.  Errors may occur later.");

		jrs_u64 uSingleSize = uSize;
		uSingleSize = uSingleSize < pHeap->GetResizableSize() ? pHeap->GetResizableSize() : uSingleSize + uSystemPageSize; // Add an extra system page to cover the free block
		uSingleSize = ((uSingleSize + (uSystemPageSize - 1)) & ~(uSystemPageSize - 1));	

		void *pSAdd = (void *)((jrs_u8 *)pHeap->GetAddress() - uSingleSize);
		void *pEAdd = (void *)((jrs_u8 *)pHeap->GetAddressEnd());

		// Try and allocate
		void *pMemStartAdd = pHeap->m_systemAllocator(uSingleSize, pSAdd);
		if(!pMemStartAdd)
		{
			pMemStartAdd = pHeap->m_systemAllocator(uSingleSize, pEAdd);
			if(!pMemStartAdd)
			{
				pMemStartAdd = pHeap->m_systemAllocator(uSingleSize, NULL);
				if(!pMemStartAdd)
					return FALSE;			// Failed to allocate
			}
		}
		void *pMemEndAdd = (void *)((jrs_sizet)((jrs_sizet)pMemStartAdd + uSingleSize));

		// Memory must be aligned to the page size also.
		MemoryWarning(!((jrs_u64)pMemStartAdd & (uSystemPageSize - 1)), JRSMEMORYERROR_RESIZEOFELEPHANTFAILED, "Returned address is not system page size aligned. Errors may occur.");

		// We have to add the expansion to our pool for later
		m_MMThreadLock.Lock();
		m_pResizableSystemAllocs[m_uResizableCount] = (jrs_u64)pMemStartAdd;
		m_pResizableSystemAllocs[m_uResizableCount + 1] = uSingleSize;
		m_uResizableCount += 2;
		m_MMThreadLock.Unlock();
		
		// Resize the heap with this memory address
		pHeap->ResizeInternal(pMemStartAdd, pMemEndAdd);

		return TRUE;
	}

	//  Description:
	//      Post enables features like LiveView and EnhancedDebugging for when it wasn't possible to initialize with the Initialize call.  This
	//		may happen on some systems that require memory for threads or networking.
	//  See Also:
	//      Initialize, Destroy
	//  Arguments:
	//      None.
	//  Return Value:
	//      TRUE if successfully initialized.
	//		FALSE for failure.
	//  Summary:
	//      Post enables features like LiveView and EnhancedDebugging 
	jrs_bool cMemoryManager::UserInitializePostFeatures(void)
	{
#ifndef MEMORYMANAGER_MINIMAL
#ifdef JRSMEMORY_HASSOCKETS
		if(m_bEnableLiveView && m_bEnableLiveViewPostInit)
		{
			g_MemoryManagerNetworkThread.Create(JRSMemory_LiveViewThread, &m_bEnableLiveView, cJRSThread::eJRSPriority_Low, 4, 32 * 1024);
			g_MemoryManagerNetworkThread.Start();
			m_bEnableLiveViewPostInit = false;
		}
#endif
		// Enhanced debugging thread if active
		if(m_bEnhancedDebugging && m_bEnableEnhancedDebuggingPostInit)
		{
			// Initialize and start the thread
			g_MemoryManagerEnhancedDebugThread.Create(JRSMemory_EnhancedDebuggingThread, &m_bEnhancedDebugging, cJRSThread::eJRSPriority_Low, 4, 32 * 1024);
			g_MemoryManagerEnhancedDebugThread.Start();
			m_bEnableEnhancedDebuggingPostInit = false;
		}
#endif
		return TRUE;
	}

	//  Description:
	//      Destroys Elephant. This will close all heaps which may or may not check for memory leaks depending
	//		on the creation of that heap.
	//  See Also:
	//      Initialize, DestroyHeap
	//  Arguments:
	//      None.
	//  Return Value:
	//      TRUE if successfully destroyed.
	//		FALSE for failure.
	//  Summary:
	//      Destroys Elephant Memory Manager.
	jrs_bool cMemoryManager::Destroy(void)
	{
		// Check if initialized
		if(!m_bInitialized)
		{
			MemoryWarning(m_bInitialized, JRSMEMORYERROR_NOTINITIALIZED, "Memory manager is not initialized.");
			return true;
		}	

		// Destroy the live view thread if needed
#ifndef MEMORYMANAGER_MINIMAL
#ifdef JRSMEMORY_HASSOCKETS
		if(m_bEnableLiveView)
		{
			// Extern the main thread
			m_bEnableLiveView = false;
			g_MemoryManagerNetworkThread.Destroy();
		}
#endif

		// Destroy the enhanced debugging thread
		if(m_bEnhancedDebugging)
		{
			m_bEnhancedDebugging = false;
			g_MemoryManagerEnhancedDebugThread.Destroy();
		}
#endif

		// Destroy any thing for platform specifics.
		MemoryManagerPlatformDestroy();

		// Remove all the non intrusive user heaps.
		for(jrs_u32 i = 0; i < MemoryManager_MaxNonIntrusiveHeaps; i++)
		{
			if(m_pNonInstrusiveHeaps[i])
				DestroyNonIntrusiveHeap(m_pNonInstrusiveHeaps[i]);
		}

		// Remove all the user heaps.
		for(jrs_u32 i = 0; i < MemoryManager_MaxUserHeaps; i++)
		{
			if(m_pUserHeaps[i])
				DestroyHeap(m_pUserHeaps[i]);
		}

		// Clear all the heaps
		for(jrs_u32 i = MemoryManager_MaxHeaps; i > 0; i--)
		{
			if(m_pHeaps[i - 1])
				DestroyHeap(m_pHeaps[i - 1]);
		}

		// End the logging
		ContinuousLogging_Operation(eContLog_StopLogging, NULL, NULL, 0);

		// Disable continuous logging
		m_bEnableContinuousDump = false;

		// Free the rest of the memory only if the system is not using a provided memory pointer.
		if(!m_bCustomMemoryDefined)
			m_MemoryManagerDefaultFree(m_pAllocatedMemoryBlock, m_uAllocatedMemorySize);

		// Clear other internal values
		m_pAllocatedMemoryBlock = 0;
		m_pUseableMemoryStart = m_pUseableMemoryEnd = 0;
		m_pUsableHeapMemoryStart = 0;
		m_pMemorySmallHeap = 0;

		// Clear the count
		m_uHeapNum = m_uUserHeapNum = 0;

		// Completed
		m_bInitialized = false;
		return true;
	}

	//  Description:
	//      Retrieves the  number of managed heaps Elephant is managing.
	//  See Also:
	//      GetMaxNumUserHeaps
	//  Arguments:
	//      None.
	//  Return Value:
	//      The number of heaps.
	//  Summary:
	//      Gets the count of managed heaps.
	jrs_u32 cMemoryManager::GetNumHeaps(void) const 
	{ 
		return m_uHeapNum; 
	}

	//  Description:
	//      Retrieves the number of self managed heaps Elephant is managing.
	//  See Also:
	//      GetMaxNumHeaps
	//  Arguments:
	//      None.
	//  Return Value:
	//      The number of user heaps.
	//  Summary:
	//      Gets the number self managed heaps.
	jrs_u32 cMemoryManager::GetNumUserHeaps(void) const 
	{ 
		return m_uUserHeapNum; 
	}

	//  Description:
	//      Retrieves the maximum number of managed heaps Elephant can handle.
	//  See Also:
	//      GetMaxNumUserHeaps
	//  Arguments:
	//      None.
	//  Return Value:
	//      The maximum number of heaps.
	//  Summary:
	//      Gets the maximum allowed managed heaps.
	jrs_u32 cMemoryManager::GetMaxNumHeaps(void) const 
	{ 
		return MemoryManager_MaxHeaps; 
	}

	//  Description:
	//      Retrieves the maximum number of NI heaps Elephant can handle.
	//  See Also:
	//      GetMaxNumUserHeaps
	//  Arguments:
	//      None.
	//  Return Value:
	//      The maximum number of NI heaps.
	//  Summary:
	//      Gets the maximum allowed NI managed heaps.
	jrs_u32 cMemoryManager::GetMaxNumNIHeaps(void) const 
	{ 
		return MemoryManager_MaxHeaps; 
	}

	//  Description:
	//      Retrieves the maximum number of self managed heaps Elephant can handle.
	//  See Also:
	//      GetMaxNumHeaps
	//  Arguments:
	//      None.
	//  Return Value:
	//      The maximum number of heaps.
	//  Summary:
	//      Gets the maximum allowed self managed heaps.
	jrs_u32 cMemoryManager::GetMaxNumUserHeaps(void) const 
	{ 
		return MemoryManager_MaxUserHeaps; 
	}

	//  Description:
	//      Creates a managed heap only, self managed heaps will fail creation.  A unique name may be specified and it will automatically
	//		come out of Elephants memory created in Initialize. Use the details to customize the operation of the heap for example
	//		to allow zero size allocations.
	//  See Also:
	//      Initialize, DestroyHeap, cHeap::sHeapDetails
	//  Arguments:
	//      uHeapSize - The total size in bytes of the heap.  16byte multiple.
	//      pHeapName - Null terminated string for the heap name. Smaller than 32bytes.
	//      pHeapDetails - A valid pointer to heap details which can be created locally on the stack.  Its lifetime is
	//					  only the length of CreateHeap function.  NULL will use the sHeapDetails values.
	//  Return Value:
	//      Valid cHeap if successful.
	//		NULL for failure.
	//  Summary:
	//      Creates a managed heap.
	cHeap *cMemoryManager::CreateHeap(jrs_u64 uHeapSize, const jrs_i8 *pHeapName, cHeap::sHeapDetails *pHeapDetails)
	{
		// Check if initialized
		if(!m_bInitialized)
		{
			MemoryWarning(m_bInitialized, JRSMEMORYERROR_NOTINITIALIZED, "Memory manager is not initialized.");
			return 0;
		}	

		m_MMThreadLock.Lock();
		cHeap::sHeapDetails defaultdetails;

		// Create details
		if(!pHeapDetails)
			pHeapDetails = &defaultdetails;

		// Do we need to set defaults
		if(pHeapDetails->systemAllocator == NULL || pHeapDetails->systemFree == NULL || pHeapDetails->systemPageSize == NULL)
		{
			pHeapDetails->systemAllocator = m_MemoryManagerDefaultAllocator;
			pHeapDetails->systemFree = m_MemoryManagerDefaultFree;
			pHeapDetails->systemPageSize = m_MemoryManagerDefaultSystemPageSize;
		}
		else if(!m_bResizeable)
		{
			// Non resizable heaps default to cMemoryManagers
			pHeapDetails->systemAllocator = m_MemoryManagerDefaultAllocator;
			pHeapDetails->systemFree = m_MemoryManagerDefaultFree;
			pHeapDetails->systemPageSize = m_MemoryManagerDefaultSystemPageSize;
		}

		if(pHeapDetails->bHeapIsSelfManaged)
		{
			MemoryWarning(!pHeapDetails->bHeapIsSelfManaged, JRSMEMORYERROR_HEAPSELFMANAGED, "Cannot create a self managed heap.  Use cMemoryManager::CreateHeap(void *pMemoryAddress, u32 uHeapSize, i8 *pHeapName, cHeap::sHeapDetails *pHeapDetails)");
			return 0;
		}
		
#ifdef JRS64BIT
		// No need to check for 64bit overflows.  Not for a few years anyway.
#else
		MemoryWarning(uHeapSize <= 0xffffffff, JRSMEMORYERROR_HEAPTOBIG, "Heap size can only be a maximum size of 4gb (2^32)");
#endif
		// Here we create the memory address and hand it to the creation function, a bit like user managed but we handle internally.
		m_bAllowHeapCreationFromAddress = true;		// Enable creation from a specified address

		jrs_sizet uSystemPageSize = m_uSystemPageSize;
		if(pHeapDetails->systemPageSize)
			uSystemPageSize = pHeapDetails->systemPageSize();

		// Resizable mode functions slightly differently
		if(m_bResizeable)
		{
			// Align the memory size to the page size but only for resizable heaps
			uHeapSize = ((uHeapSize + (uSystemPageSize - 1)) & ~(uSystemPageSize - 1));

			void *pMemory = pHeapDetails->systemAllocator(uHeapSize, NULL);
			
			// Increase count			
			m_pResizableSystemAllocs[m_uResizableCount] = (jrs_u64)pMemory;
			m_pResizableSystemAllocs[m_uResizableCount + 1] = uHeapSize;
			m_uResizableCount += 2;			

			cHeap *pHeap = CreateHeap(pMemory, (jrs_sizet)uHeapSize, pHeapName, pHeapDetails);
			
			// Call the system op callback if one exist.
			if(pHeapDetails->systemOpCallback)
				pHeapDetails->systemOpCallback(pHeap, pMemory, uHeapSize, FALSE);

			m_bAllowHeapCreationFromAddress = false;			// Disable
			m_MMThreadLock.Unlock();			
			return pHeap;
		}

		//If the last heap is still null we need to create one from the start other wise we tag it on the end
		cHeap *pHeap = CreateHeap(m_pUsableHeapMemoryStart, (jrs_sizet)uHeapSize, pHeapName, pHeapDetails);
		if(pHeap)
		{
			//Increment the size of the heap
			m_pUsableHeapMemoryStart = (void *)((jrs_i8 *)m_pUsableHeapMemoryStart + uHeapSize);
		}
		m_bAllowHeapCreationFromAddress = false;			// Disable

		//Return the Heap
		m_MMThreadLock.Unlock();
		return pHeap;
	}

	//  Description:
	//      Creates a managed or self managed heap.  A unique name may be specified and it will automatically
	//		come out of Elephants memory created in Initialize. Use the details to customize the operation of the heap for example
	//		to allow zero size allocations.  Use this type of create heap to direct to other predefined areas of the system for 
	//		memory types such as VRAM.
	//  See Also:
	//      Initialize, DestroyHeap, cHeap::sHeapDetails
	//  Arguments:
	//		pMemoryAddress - The memory address to create the heap on.  May be other CPU accessible areas 
	//						like VRAM. 
	//      uHeapSize - The total size in bytes of the heap.  16byte multiple.
	//      pHeapName - Null terminated string for the heap name. Smaller than 32bytes.
	//      pHeapDetails - A valid pointer to heap details which can be created locally on the stack.  Its lifetime is
	//					  only the length of CreateHeap function.
	//  Return Value:
	//      Valid cHeap if successful.
	//		NULL for failure.
	//  Summary:
	//      Creates a managed heap.
	cHeap *cMemoryManager::CreateHeap(void *pMemoryAddress, jrs_u64 uHeapSize, const jrs_i8 *pHeapName, cHeap::sHeapDetails *pHeapDetails)
	{
		MemoryWarning(pHeapDetails, JRSMEMORYERROR_INVALIDARGUMENTS, "pHeapDetails must be valid for this function");
		MemoryWarning(pHeapDetails->uDefaultAlignment >= 16, JRSMEMORYERROR_INVALIDALIGN, "Default alignment must be >= 16");
		MemoryWarning(!(pHeapDetails->uDefaultAlignment & (pHeapDetails->uDefaultAlignment - 1)), JRSMEMORYERROR_INVALIDALIGN, "Cannot create heap because the alignment is non power of 2");
		MemoryWarning(uHeapSize >= (sizeof(sFreeBlock) + sizeof(sAllocatedBlock) + pHeapDetails->uMinAllocationSize), JRSMEMORYERROR_HEAPTOSMALL, "Heap size must be a minimum of %d bytes.  WARNING You probably want this to be atleast 128k or bigger to be useful.", sizeof(sFreeBlock) + sizeof(sAllocatedBlock) + pHeapDetails->uMinAllocationSize);
		MemoryWarning(!(uHeapSize & 0xf), JRSMEMORYERROR_INVALIDALIGN, "Heap size must be 16byte aligned.");

		// Must be a valid memory address
		if(!pMemoryAddress)
		{
			MemoryWarning(pMemoryAddress, JRSMEMORYERROR_HEAPCREATEINVALIDPOINTER, "0 Memory address.  Cannot create Heap.");
			return 0;
		}

		// Check if initialized
		if(!m_bInitialized)
		{
			MemoryWarning(m_bInitialized,  JRSMEMORYERROR_NOTINITIALIZED, "Memory manager is not initialized.");
			return 0;
		}	

		// Do we need to set defaults
		if(pHeapDetails->systemAllocator == NULL || pHeapDetails->systemFree == NULL || pHeapDetails->systemPageSize == NULL)
		{
			MemoryWarning(pHeapDetails->systemAllocator, JRSMEMORYERROR_HEAPSYSTEMALLOCATOR, "The heap detail system allocator is not null.  It will be overridden.");
			MemoryWarning(pHeapDetails->systemFree, JRSMEMORYERROR_HEAPSYSTEMALLOCATOR, "The heap detail system allocator is not null.  It will be overridden.");
			MemoryWarning(pHeapDetails->systemPageSize, JRSMEMORYERROR_HEAPSYSTEMALLOCATOR, "The heap detail system allocator is not null.  It will be overridden.");

			pHeapDetails->systemAllocator = m_MemoryManagerDefaultAllocator;
			pHeapDetails->systemFree = m_MemoryManagerDefaultFree;
			pHeapDetails->systemPageSize = m_MemoryManagerDefaultSystemPageSize;
		}

		// Lock
		m_MMThreadLock.Lock();

		// Check if the heap with the same name has already been created.
		cHeap *pHeap = FindHeap(pHeapName);
		if(pHeap)
		{
			MemoryWarning(0, JRSMEMORYERROR_HEAPNAME, "Memory manager cannot create two heaps with the same name.");
			// UnLock
			m_MMThreadLock.Unlock();
			return 0;
		}

		// Check if we can create it
		if(!pHeapDetails->bHeapIsSelfManaged && !m_bAllowHeapCreationFromAddress)
		{
			MemoryWarning(!(!pHeapDetails->bHeapIsSelfManaged && !m_bAllowHeapCreationFromAddress), JRSMEMORYERROR_HEAPSELFMANAGED, "Cannot create a self managed heap.  See cHeap::sHeapDetails::bHeapIsSelfManaged flag for details.");
			return 0;
		}
		
		// Is the heap memory manager micromanaged?
		if(!pHeapDetails->bHeapIsSelfManaged)
		{
			// Set the correct heap up			
			for(jrs_u32 uHeap = 0; uHeap < MemoryManager_MaxHeaps; uHeap++)
			{
				// Find an empty heap
				if(!m_pHeaps[uHeap])
				{
					jrs_u32 HeapNumber = uHeap;

					// Found
					m_uHeapNum++;

					// Ensure the pointer we are setting is 0.
					MemoryWarning(!m_pHeaps[HeapNumber], JRSMEMORYERROR_FATAL, "Pointer must be 0.  Fatal error.");

					// If the heap size is 0 then it will expand to the whole buffer (resizable doesnt need this check as we pass it in).
					if(!m_bResizeable)
					{
						MemoryWarning(GetFreeUsableMemory() >= uHeapSize, JRSMEMORYERROR_ELEPHANTOOM, "Cannot allocate the heap.  Out of memory.");
					}

					// Now create it.
					m_pMemoryHeaps[HeapNumber] = cHeap(pMemoryAddress, (jrs_sizet)uHeapSize, pHeapName, pHeapDetails);	
					m_pMemoryHeaps[HeapNumber].m_pThreadLock = &g_ThreadLocks[HeapNumber];
					m_pHeaps[HeapNumber] = &m_pMemoryHeaps[HeapNumber];

					// Set the unique id
					m_pHeaps[HeapNumber]->m_uHeapId = m_uHeapIdInfo++;

					// UnLock
					m_MMThreadLock.Unlock();

					return m_pHeaps[HeapNumber];
				}
			}

			MemoryWarning(0, JRSMEMORYERROR_NOTENOUGHHEAPS, "Out of free heaps");
			m_MMThreadLock.Unlock();
				
			return 0;			
		}
		else
		{
			// Cannot init a user heap thats smaller than the minimum
			if((sizeof(sFreeBlock) + sizeof(sAllocatedBlock) + pHeapDetails->uMinAllocationSize) > uHeapSize)
			{
				MemoryWarning((sizeof(sFreeBlock) + sizeof(sAllocatedBlock) + pHeapDetails->uMinAllocationSize) < uHeapSize, JRSMEMORYERROR_HEAPTOSMALL, "User heap size is to small.  Must be %d bytes.", (sizeof(sFreeBlock) + sizeof(sAllocatedBlock) + pHeapDetails->uMinAllocationSize));
				// UnLock
				m_MMThreadLock.Unlock();
				return 0;
			}

			// The heap is user managed but we will register with the memory manager for debugging reasons.
			// Set the correct heap up
			jrs_u32 HeapNumber = m_uUserHeapNum;
			if(HeapNumber >= MemoryManager_MaxUserHeaps)
			{
				MemoryWarning(HeapNumber < MemoryManager_MaxUserHeaps, JRSMEMORYERROR_NOTENOUGHUSERHEAPS, "Out of user heaps");
				// UnLock
				m_MMThreadLock.Unlock();
				return 0;
			}
			m_uUserHeapNum++;

			// Find the first free one.  This is because user heaps can be freed in any order.
			for(jrs_u32 i = 0; i < MemoryManager_MaxUserHeaps; i++)
			{
				if(!m_pUserHeaps[i])
				{
					HeapNumber = i;
					break;
				}
			}

			// Now create it.
			MemoryWarning(!m_pUserHeaps[HeapNumber], JRSMEMORYERROR_FATAL, "Fatal error in user heap allocation.  Report a bug.");
			m_pMemoryUserHeaps[HeapNumber] = cHeap(pMemoryAddress, (jrs_sizet)uHeapSize, pHeapName, pHeapDetails);		m_pMemoryHeaps[MemoryManager_MaxHeaps + HeapNumber].m_pThreadLock = &g_ThreadLocks[MemoryManager_MaxHeaps + HeapNumber];
			m_pUserHeaps[HeapNumber] = &m_pMemoryUserHeaps[HeapNumber];

			// Set the unique id
			m_pUserHeaps[HeapNumber]->m_uHeapId = m_uHeapIdInfo++;

			// UnLock
			m_MMThreadLock.Unlock();
			return m_pUserHeaps[HeapNumber];
		}
	}

	//  Description:
	//      Creates a non intrusive heap. Use this type of create heap to direct to other predefined areas of the system for 
	//		memory types such as VRAM that the CPU has limited access to.
	//  See Also:
	//      Initialize, DestroyHeap, cHeap::sHeapDetails
	//  Arguments:
	//		pMemoryAddress - The memory address to create the heap on.  May be other addressable areas not accessible via CPU like
	//						like VRAM. 
	//      uHeapSize - The total size in bytes of the heap.  16byte multiple.
	//      pHeapName - Null terminated string for the heap name. Smaller than 32bytes.
	//      pHeapDetails - A valid pointer to heap details which can be created locally on the stack.  Its lifetime is
	//					  only the length of CreateHeap function.
	//  Return Value:
	//      Valid cHeap if successful.
	//		NULL for failure.
	//  Summary:
	//      Creates a NI non resizable intrusive heap.
	cHeapNonIntrusive *cMemoryManager::CreateNonIntrusiveHeap(void *pMemoryAddress, jrs_sizet uHeapSize, cHeap *pHeap, const jrs_i8 *pHeapName, cHeapNonIntrusive::sHeapDetails *pHeapDetails)
	{
		MemoryWarning(pHeap, JRSMEMORYERROR_HEAPINVALID, "Heap must be valid");

		if(!pMemoryAddress)
		{
			MemoryWarning(pMemoryAddress, JRSMEMORYERROR_INVALIDADDRESS, "pMemoryAddress must be a pointer to a valid memory block.");
			return NULL;
		}
		
		cHeapNonIntrusive::sHeapDetails details;
		if(!pHeapDetails)
			pHeapDetails = &details;

		if(pHeapDetails->bResizable)
		{
			MemoryWarning(!pHeapDetails->bResizable, JRSMEMORYERROR_INVALIDARGUMENTS, "Cannot create this style heap with bResizable set.");
			return NULL;
		}

		// Lock
		m_MMThreadLock.Lock();

		// Check if the heap with the same name has already been created.
		cHeapNonIntrusive *pHeapNI = FindNonIntrusiveHeap(pHeapName);
		if(pHeapNI)
		{
			MemoryWarning(0, JRSMEMORYERROR_HEAPNAME, "Memory manager cannot create two heaps with the same name.");
			// UnLock
			m_MMThreadLock.Unlock();
			return 0;
		}

		// Check we can create a heap
		if(m_uNonIntrusiveHeapNum >= MemoryManager_MaxNonIntrusiveHeaps)
		{
			MemoryWarning(m_uNonIntrusiveHeapNum < MemoryManager_MaxNonIntrusiveHeaps, JRSMEMORYERROR_NOTENOUGHNONINTRUSIVEHEAPS, "Out of non intrusive heaps");
			// UnLock
			m_MMThreadLock.Unlock();
			return NULL;
		}

		// Find an empty heap space
		for(jrs_u32 i = 0; i < MemoryManager_MaxNonIntrusiveHeaps; i++)
		{
			if(!m_pNonInstrusiveHeaps[i])
			{
				// Create it
				m_pMemoryNonIntrusiveHeaps[i] = cHeapNonIntrusive(pMemoryAddress, uHeapSize, pHeap, pHeapName, pHeapDetails);		
				m_pMemoryNonIntrusiveHeaps[i].m_pThreadLock = &g_ThreadLocks[MemoryManager_MaxHeaps + MemoryManager_MaxUserHeaps + i];
				m_pMemoryNonIntrusiveHeaps[i].m_bSelfManaged = true;
				m_pMemoryNonIntrusiveHeaps[i].m_uHeapId = m_uHeapIdInfo++;
				m_pNonInstrusiveHeaps[i] = &m_pMemoryNonIntrusiveHeaps[i];

				m_uNonIntrusiveHeapNum++;

				// Unlock
				m_MMThreadLock.Unlock();

				return m_pNonInstrusiveHeaps[i];
			}
		}

		// Unlock
		m_MMThreadLock.Unlock();

		return NULL;
	}

	//  Description:
	//      Creates a non intrusive heap. Use this type of create heap to direct to other predefined areas of the system for 
	//		memory types such as VRAM that the CPU has limited access to.
	//  See Also:
	//      Initialize, DestroyHeap, cHeap::sHeapDetails
	//  Arguments:
	//		pMemoryAddress - The memory address to create the heap on.  May be other addressable areas not accessible via CPU like
	//						like VRAM. 
	//      uHeapSize - The total size in bytes of the heap.  16byte multiple.
	//      pHeapName - Null terminated string for the heap name. Smaller than 32bytes.
	//      pHeapDetails - A valid pointer to heap details which can be created locally on the stack.  Its lifetime is
	//					  only the length of CreateHeap function.
	//  Return Value:
	//      Valid cHeap if successful.
	//		NULL for failure.
	//  Summary:
	//      Creates a NI non intrusive heap.
	cHeapNonIntrusive *cMemoryManager::CreateNonIntrusiveHeap(jrs_sizet uHeapSize, cHeap *pHeap, const jrs_i8 *pHeapName, cHeapNonIntrusive::sHeapDetails *pHeapDetails)
	{
		MemoryWarning(pHeap, JRSMEMORYERROR_HEAPINVALID, "Heap must be valid");

		cHeapNonIntrusive::sHeapDetails details;
		if(!pHeapDetails)
			pHeapDetails = &details;

		// Lock
		m_MMThreadLock.Lock();

		// Check if the heap with the same name has already been created.
		cHeapNonIntrusive *pHeapNI = FindNonIntrusiveHeap(pHeapName);
		if(pHeapNI)
		{
			MemoryWarning(0, JRSMEMORYERROR_HEAPNAME, "Memory manager cannot create two heaps with the same name.");
			// UnLock
			m_MMThreadLock.Unlock();
			return 0;
		}

		// Check we can create a heap
		if(m_uNonIntrusiveHeapNum >= MemoryManager_MaxNonIntrusiveHeaps)
		{
			MemoryWarning(m_uNonIntrusiveHeapNum < MemoryManager_MaxNonIntrusiveHeaps, JRSMEMORYERROR_NOTENOUGHNONINTRUSIVEHEAPS, "Out of non intrusive heaps");
			// UnLock
			m_MMThreadLock.Unlock();
			return NULL;
		}

		// Find an empty heap space
		for(jrs_u32 i = 0; i < MemoryManager_MaxNonIntrusiveHeaps; i++)
		{
			if(!m_pNonInstrusiveHeaps[i])
			{
				// Create it
				m_pMemoryNonIntrusiveHeaps[i] = cHeapNonIntrusive(NULL, uHeapSize, pHeap, pHeapName, pHeapDetails);		
				m_pMemoryNonIntrusiveHeaps[i].m_pThreadLock = &g_ThreadLocks[MemoryManager_MaxHeaps + MemoryManager_MaxUserHeaps + i];
				m_pMemoryNonIntrusiveHeaps[i].m_uHeapId = m_uHeapIdInfo++;
				m_pNonInstrusiveHeaps[i] = &m_pMemoryNonIntrusiveHeaps[i];
				m_uNonIntrusiveHeapNum++;

				// Unlock
				m_MMThreadLock.Unlock();

				return m_pNonInstrusiveHeaps[i];
			}
		}

		// Unlock
		m_MMThreadLock.Unlock();

		return NULL;
	}


	//  Description:
	//      Resizes a heap to the size you require.  For self managed heaps you must be very careful when resizing that the expansion does not trample other memory.  
	//		Elephant has no way of detecting this.  Elephant will disallow resizing of managed heaps if it is not the last on the stack.
	//  See Also:
	//      CreateHeap, DestroyHeap, cHeap::sHeapDetails
	//  Arguments:
	//		pHeap - Valid pointer to a cHeap.
	//      uSize - The total size in bytes to resize the heap.  16byte multiple.  
	//  Return Value:
	//      TRUE if successful.
	//		FALSE for failure.
	//  Summary:
	//      Resizes a heap.
	jrs_bool cMemoryManager::ResizeHeap(cHeap *pHeap, jrs_u64 uSize)
	{
		// Can only resize a heap if its at the end.  Its is up to the user to deal with unmanaged heaps.
		MemoryWarning(pHeap, JRSMEMORYERROR_HEAPINVALID, "Heap must be a valid pointer.");

		// Check if initialized
		if(!m_bInitialized)
		{
			MemoryWarning(m_bInitialized,  JRSMEMORYERROR_NOTINITIALIZED, "Memory manager is not initialized.");
			return false;
		}	

		// Self managed heap
		if(!pHeap->IsMemoryManagerManaged())
		{
			MemoryWarning(pHeap->IsMemoryManagerManaged(), JRSMEMORYERROR_HEAPINVALIDCALL, "Heap cannot be resized with this function as it is managed by yourself.  Use cHeap::Resize().");
			return false;
		}

		// Lock
		m_MMThreadLock.Lock();

		// Check the end block
		if((void *)((jrs_i8 *)pHeap->m_pHeapEndAddress + sizeof(sFreeBlock)) != m_pUsableHeapMemoryStart)
		{
			MemoryWarning((void *)((jrs_i8 *)pHeap->m_pHeapEndAddress + sizeof(sFreeBlock)) == m_pUsableHeapMemoryStart, JRSMEMORYERROR_HEAPINVALIDFREE, "The heap is not the last heap that is self managed in the memory manager.  It cannot be resized using this method.");
			// UnLock
			m_MMThreadLock.Unlock();
			return false;
		}

		// Resizing to the same size does nothing
		if(pHeap->m_uHeapSize == uSize)
		{
			// UnLock
			m_MMThreadLock.Unlock();
			return true;
		}

		// Work out the minimum size of the heap
		jrs_sizet MinSize = sizeof(sFreeBlock) + sizeof(sAllocatedBlock) + pHeap->m_uMinAllocSize;
		if(uSize < MinSize)
		{
			MemoryWarning(uSize >= MinSize, JRSMEMORYERROR_HEAPTOSMALL, "Cannot resize the heap to %d bytes.  Minimum size to resize to is %d bytes.", uSize, MinSize);
			// UnLock
			m_MMThreadLock.Unlock();
			return false;
		}

		// Check it will fit within the main memory manager.
		jrs_i8 *pNewPointer = pHeap->m_pHeapStartAddress + uSize;
		if(pNewPointer > (jrs_i8 *)m_pUseableMemoryEnd)
		{
			MemoryWarning(pNewPointer <= (jrs_i8 *)m_pUseableMemoryEnd, JRSMEMORYERROR_HEAPTOBIG, "The heap is being resized past the size of the memory managers usable memory area.  Resize has failed.");
			// UnLock
			m_MMThreadLock.Unlock();
			return false;
		}

		// Move the new pointer back
		m_pUsableHeapMemoryStart = pNewPointer;

		// Resize
		jrs_bool bPassed = pHeap->Resize((jrs_sizet)uSize);

		// UnLock
		m_MMThreadLock.Unlock();
		return bPassed;
	}

	//  Description:
	//      Resizes a heap to the end of the greatest allocated block.  Elephant will disallow resizing of self managed heaps.  Use cHeap::Resize instead.
	//  See Also:
	//      CreateHeap, DestroyHeap, ResizeHeap, cHeap::Resize, cHeap::sHeapDetails
	//  Arguments:
	//		pHeap - Valid pointer to a cHeap.
	//  Return Value:
	//      TRUE if successful.
	//		FALSE for failure.
	//  Summary:
	//      Resizes a heap to the last allocation.
	jrs_bool cMemoryManager::ResizeHeapToLastAllocation(cHeap *pHeap)
	{
		MemoryWarning(pHeap, JRSMEMORYERROR_HEAPCREATEINVALIDPOINTER, "Heap must be a valid pointer.");
		MemoryWarning(pHeap->IsMemoryManagerManaged(), JRSMEMORYERROR_HEAPSELFMANAGED, "Heap cannot be resized with this function as it is managed by yourself.  Use cHeap::Resize().");
		MemoryWarning((void *)((jrs_i8 *)pHeap->m_pHeapEndAddress + sizeof(sFreeBlock)) == m_pUsableHeapMemoryStart, JRSMEMORYERROR_HEAPINVALIDFREE, "The heap is not the last heap that is self managed in the memory manager.  It cannot be resized using this method.");

		// Lock
		m_MMThreadLock.Lock();

		// Work out the memory address we can resize too.
		void *pEndPointer = (void *)((jrs_i8 *)pHeap->m_pMainFreeBlock + sizeof(sFreeBlock));

		// Just make sure we havent overrun the heap.  Unlikely but you never know.
		MemoryWarning(pEndPointer <= m_pUsableHeapMemoryStart, JRSMEMORYERROR_FATAL, "Heap has overrun the end pointer.  Serious error.");

		// Check that we dont need to resize it if it fits exactly
		if(pEndPointer == m_pUsableHeapMemoryStart)
		{
			// No need to size here
			// UnLock
			m_MMThreadLock.Unlock();
			return true;
		}

		// Heap can be resized.  Resize the heap to this size.  It may be 0.
		jrs_u64 uNewSize = (jrs_i8 *)pEndPointer - pHeap->m_pHeapStartAddress;
		jrs_u64 uMinSize = sizeof(sFreeBlock) + sizeof(sAllocatedBlock) + pHeap->m_uMinAllocSize;
		if(uNewSize < uMinSize)
			uNewSize = uMinSize;

		// And finally do it.
		jrs_bool bPass = ResizeHeap(pHeap, uNewSize);

		// UnLock
		m_MMThreadLock.Unlock();
		return bPass;
	}

	//  Description:
	//      Finds a heap based on its name.
	//  See Also:
	//      GetHeap, CreateHeap
	//  Arguments:
	//		pHeapName - Null terminated string with the heap's name.
	//  Return Value:
	//      Valid cHeap if successful.
	//		NULL if heap couldn't be found.
	//  Summary:
	//      Finds a heap based on its name.
	cHeap *cMemoryManager::FindHeap(const jrs_i8 *pHeapName)
	{
		// Check if initialized
		if(!m_bInitialized)
		{
			MemoryWarning(m_bInitialized,  JRSMEMORYERROR_NOTINITIALIZED, "Memory manager is not initialized.");
			return 0;
		}	

		// Scan all the heaps.
		for(jrs_u32 i = 0; i < MemoryManager_MaxHeaps; i++)
		{
			// Only if valid
			if(m_pHeaps[i])
			{
				if(/*strlen(m_pHeaps[i]->m_HeapName) == strlen(pHeapName) && */!strcmp(pHeapName, m_pHeaps[i]->m_HeapName))
				{
					return m_pHeaps[i];
				}
			}
		}

		// Scan all the user heaps.
		for(jrs_u32 i = 0; i < MemoryManager_MaxUserHeaps; i++)
		{
			// Only if valid
			if(m_pUserHeaps[i])
			{
				if(/*strlen(m_pHeaps[i]->m_HeapName) == strlen(pHeapName) && */!strcmp(pHeapName, m_pUserHeaps[i]->m_HeapName))
				{
					return m_pUserHeaps[i];
				}
			}
		}

		return 0;
	}

	//  Description:
	//      Finds a non intrusive heap based on its name.
	//  See Also:
	//      GetHeap, CreateHeap
	//  Arguments:
	//		pHeapName - Null terminated string with the heap's name.
	//  Return Value:
	//      Valid cHeap if successful.
	//		NULL if heap couldn't be found.
	//  Summary:
	//      Finds a non intrusive heap based on its name.
	cHeapNonIntrusive *cMemoryManager::FindNonIntrusiveHeap(const jrs_i8 *pHeapName)
	{
		// Scan all the user heaps.
		for(jrs_u32 i = 0; i < MemoryManager_MaxNonIntrusiveHeaps; i++)
		{
			// Only if valid
			if(m_pNonInstrusiveHeaps[i])
			{
				if(!strcmp(pHeapName, m_pNonInstrusiveHeaps[i]->GetName()))
				{
					return m_pNonInstrusiveHeaps[i];
				}
			}
		}

		return NULL;
	}

	//  Description:
	//      Gets a managed heap from an index
	//  See Also:
	//      GetHeap, CreateHeap
	//  Arguments:
	//		iIndex - Index of heap, starting at 0.
	//  Return Value:
	//      Valid cHeap if successful.
	//		NULL if heap couldn't be found.
	//  Summary:
	//      Gets a managed heap by index.
	cHeap *cMemoryManager::GetHeap(jrs_u32 iIndex)
	{
		// Check if initialized
		if(!m_bInitialized)
		{
			MemoryWarning(m_bInitialized,  JRSMEMORYERROR_NOTINITIALIZED, "Memory manager is not initialized.");
			return 0;
		}	

		MemoryWarning(iIndex < MemoryManager_MaxHeaps, JRSMEMORYERROR_NOTENOUGHHEAPS, "Heap index exceeds the number of heaps");

		return m_pHeaps[iIndex];
	}

	//  Description:
	//      Gets a self managed heap from an index.
	//  See Also:
	//      GetHeap, CreateHeap
	//  Arguments:
	//		iIndex - Index of heap, starting at 0.
	//  Return Value:
	//      Valid cHeap if successful.
	//		NULL if heap couldn't be found.
	//  Summary:
	//      Gets a managed heap by index.
	cHeap *cMemoryManager::GetUserHeap(jrs_u32 iIndex)
	{
		// Check if initialized
		if(!m_bInitialized)
		{
			MemoryWarning(m_bInitialized,  JRSMEMORYERROR_NOTINITIALIZED, "Memory manager is not initialized.");
			return 0;
		}	

		MemoryWarning(iIndex < MemoryManager_MaxUserHeaps, JRSMEMORYERROR_NOTENOUGHUSERHEAPS, "Heap index exceeds the number of heaps");

		return m_pUserHeaps[iIndex];
	}

	//  Description:
	//      Gets a non intrusive heap from an index.
	//  See Also:
	//      GetHeap, CreateHeap
	//  Arguments:
	//		iIndex - Index of heap, starting at 0.
	//  Return Value:
	//      Valid cHeap if successful.
	//		NULL if heap couldn't be found.
	//  Summary:
	//      Gets a Non intrusive heap by index.
	cHeapNonIntrusive *cMemoryManager::GetNIHeap(jrs_u32 iIndex)
	{
		// Check if initialized
		if(!m_bInitialized)
		{
			MemoryWarning(m_bInitialized,  JRSMEMORYERROR_NOTINITIALIZED, "Memory manager is not initialized.");
			return 0;
		}	

		MemoryWarning(iIndex < MemoryManager_MaxNonIntrusiveHeaps, JRSMEMORYERROR_NOTENOUGHNONINTRUSIVEHEAPS, "Heap index exceeds the number of heaps");

		return m_pNonInstrusiveHeaps[iIndex];
	}

	//  Description:
	//      Destroys the specified heap. The heap can be managed or self managed. On destruction the heap may warn
	//		you if there are any allocations remaining depending on the settings specified at creation time.
	//  See Also:
	//      GetHeap, CreateHeap
	//  Arguments:
	//		pHeap - Valid cHeap.
	//  Return Value:
	//      TRUE if successfully destroyed.
	//		FALSE otherwise.
	//  Summary:
	//      Destroys a heap
	jrs_bool cMemoryManager::DestroyHeap(cHeap *pHeap)
	{
		// Check if initialized
		if(!m_bInitialized || !pHeap)
		{
			MemoryWarning(pHeap, JRSMEMORYERROR_HEAPINVALID, "Heap is not valid");
			MemoryWarning(m_bInitialized,  JRSMEMORYERROR_NOTINITIALIZED, "Memory manager is not initialized.");
			return false;
		}	

		// Lock
		m_MMThreadLock.Lock();

		// If we are using enhanced debugging we may have pending operations so we wait for those to clear.  Warn the user.
		if(m_bEnhancedDebugging && pHeap->m_bEnableEnhancedDebug && pHeap->m_uEDebugPending)
		{
			DebugOutput("cMemoryManager::DestroyHeap - Enhanced Debugging is waiting for some pending allocations to be cleared.");

			volatile jrs_u32 *pPending = &pHeap->m_uEDebugPending;
			while(*pPending)
			{
				JRSThread::SleepMilliSecond(16);
			}
		}
		
		// We can only destroy a heap with allocations still valid if we are allowed.  Check that here.
		if(!pHeap->m_bAllowDestructionWithAllocations)
		{
			if(pHeap->GetNumberOfAllocations())
			{
				MemoryWarning(pHeap->GetNumberOfAllocations() == 0, JRSMEMORYERROR_HEAPWITHVALIDALLOCATIONS, "Cannot free heap %s as it still has valid allocations. Set Heap flag bAllowDestructionWithAllocations to true.", pHeap->m_HeapName);
				// UnLock
				m_MMThreadLock.Unlock();
				return false;
			}

			if(pHeap->m_pAttachedPools)
			{
				MemoryWarning(!pHeap->m_pAttachedPools, JRSMEMORYERROR_HEAPWITHVALIDPOOLS, "Cannot free heap %s as it still has valid pools. Set Heap flag bAllowDestructionWithAllocations to true.", pHeap->m_HeapName);
			}
		}

		// Heaps have to be destroyed with different methods depending on if they are managed or not.
		if(pHeap->IsMemoryManagerManaged())
		{
			MemoryWarning(m_uHeapNum, JRSMEMORYERROR_HEAPINVALID, "No heap to remove");

			// Find the heap
			jrs_u32 uHeap = 0;
			cHeap *pFHeap = NULL;
			for(; uHeap < MemoryManager_MaxHeaps; uHeap++)
			{
				if(pHeap == m_pHeaps[uHeap])
				{
					pFHeap = pHeap;
					break;
				}
			}
			
			// Did we match one?
			if(!pFHeap)
			{
				MemoryWarning(pFHeap, JRSMEMORYERROR_HEAPINVALIDFREE, "Heap not found, could not be freed.");
				return false;
			}

			// Now free it
			if(m_bResizeable)
			{
				// Free the memory
				pFHeap->DestroyLinkedMemory();
			}
			else
			{
				// Cannot free a managed heap if its not the last one.
				if((void *)((jrs_i8 *)pFHeap->m_pHeapEndAddress + sizeof(sFreeBlock)) != m_pUsableHeapMemoryStart)
				{
					MemoryWarning((void *)((jrs_i8 *)pFHeap->m_pHeapEndAddress + sizeof(sFreeBlock)) == m_pUsableHeapMemoryStart, JRSMEMORYERROR_HEAPINVALIDFREE, "The heap is not the last heap that is self managed in the memory manager.  It cannot be removed using this method. Heap may also be a user managed heap without the user managed flag set.");
					// UnLock
					m_MMThreadLock.Unlock();
					return false;
				}

				// Reduce the number of heaps
				m_pUsableHeapMemoryStart = (jrs_i8 *)pFHeap->m_pHeapStartAddress;
			}
			m_uHeapNum--;

			cMemoryManager::Get().ContinuousLogging_Operation(cMemoryManager::eContLog_DestroyHeap, pFHeap, NULL, 0);

			// Set it to 0
			m_pHeaps[uHeap] = 0;
		}
		else
		{
			// User heap.  Destroy it here.  A bit more complex and the user still has to deal with the memory used to create this heap.
			MemoryWarning(m_uUserHeapNum, JRSMEMORYERROR_HEAPINVALID, "No heap to remove");

			// Find the heap in the user list
			jrs_u32 uHeap;
			for(uHeap = 0; uHeap < MemoryManager_MaxUserHeaps; uHeap++)
			{
				if(pHeap == m_pUserHeaps[uHeap])
					break;
			}

			// Check if we found it or not
			if(uHeap == MemoryManager_MaxUserHeaps)
			{
				MemoryWarning(m_uUserHeapNum, JRSMEMORYERROR_HEAPINVALID, "No heap to remove");

				// UnLock
				m_MMThreadLock.Unlock();
				return false;
			}

			cMemoryManager::Get().ContinuousLogging_Operation(cMemoryManager::eContLog_DestroyHeap, m_pUserHeaps[m_uHeapNum], NULL, 0);

			// Remove the heap
			m_pUserHeaps[uHeap] = 0;
			m_uUserHeapNum--;
		}

		// UnLock
		m_MMThreadLock.Unlock();

		// Removed.
		return true;
	}

	//  Description:
	//      Destroys the specified non intrusive heap. On destruction the heap may warn
	//		you if there are any allocations remaining depending on the settings specified at creation time.
	//  See Also:
	//      GetHeap, FindHeap
	//  Arguments:
	//		pHeap - Valid cHeapNonIntrusive.
	//  Return Value:
	//      TRUE if successfully destroyed.
	//		FALSE otherwise.
	//  Summary:
	//      Destroys a heap
	jrs_bool cMemoryManager::DestroyNonIntrusiveHeap(cHeapNonIntrusive *pHeap)
	{
		// Check if initialized
		if(!m_bInitialized || !pHeap)
		{
			MemoryWarning(pHeap, JRSMEMORYERROR_HEAPINVALID, "Heap is not valid");
			MemoryWarning(m_bInitialized,  JRSMEMORYERROR_NOTINITIALIZED, "Memory manager is not initialized.");
			return false;
		}	

		// Lock
		m_MMThreadLock.Lock();

		// User heap.  Destroy it here.  A bit more complex and the user still has to deal with the memory used to create this heap.
		MemoryWarning(m_uNonIntrusiveHeapNum, JRSMEMORYERROR_HEAPINVALID, "No heap to remove");

		// Find the heap in the user list
		jrs_u32 uHeap;
		for(uHeap = 0; uHeap < MemoryManager_MaxNonIntrusiveHeaps; uHeap++)
		{
			if(pHeap == m_pNonInstrusiveHeaps[uHeap])
				break;
		}

		// Check if we found it or not
		if(uHeap == MemoryManager_MaxNonIntrusiveHeaps)
		{
			MemoryWarning(m_uNonIntrusiveHeapNum, JRSMEMORYERROR_HEAPINVALID, "No heap to remove");

			// UnLock
			m_MMThreadLock.Unlock();
			return false;
		}

		// We can only destroy a heap with allocations still valid if we are allowed.  Check that here.
		if(!pHeap->m_bAllowDestructionWithAllocations)
		{
			if(pHeap->GetNumberOfAllocations())
			{
				MemoryWarning(pHeap->GetNumberOfAllocations() == 0, JRSMEMORYERROR_HEAPWITHVALIDALLOCATIONS, "Cannot free heap %s as it still has valid allocations. Set Heap flag bAllowDestructionWithAllocations to true.", pHeap->m_HeapName);
				// UnLock
				m_MMThreadLock.Unlock();
				return false;
			}
		}
		cMemoryManager::Get().ContinuousLogging_NIOperation(cMemoryManager::eContLog_DestroyHeap, pHeap, NULL, 0);

		// Remove the allocations
		pHeap->Destroy();

		// Remove the heap
		m_pNonInstrusiveHeaps[uHeap] = NULL;
		m_uNonIntrusiveHeapNum--;

		m_MMThreadLock.Unlock();
		return TRUE;
	}

	//  Description:
	//      Returns the heap where the memory address lies.  Can be useful for finding out general information of where memory came from.  Does not need to be an allocated address,
	//		any will do.
	//  See Also:
	//      
	//  Arguments:
	//		pMemory - Memory address to search heaps against.
	//  Return Value:
	//      A valid cHeap pointer if a heap contains the memory location.  NULL otherwise.
	//  Summary:
	//      Returns a valid Heap where the memory address lies.
	cHeap *cMemoryManager::FindHeapFromMemoryAddress(void *pMemory) const
	{
		// Do the user heaps first.  Sounds odd but the user heaps MAY come from the main heap.  Then any data that gets free'd through this function
		// will corrupt that main heap as it will be detected from that heap.
		for(jrs_u32 i = MemoryManager_MaxUserHeaps; i > 0; i--)
		{
			if(m_pUserHeaps[i - 1] && m_pUserHeaps[i - 1]->IsAllocatedFromThisHeap(pMemory))
			{
				return m_pUserHeaps[i - 1];
			}
		}
		
		// Find out what heap it is from.  Done in reverse because typically they will be done on the last heap using these methods.  This will
		// also catch the small heap.
		for(jrs_u32 i = MemoryManager_MaxHeaps; i > 0; i--)
		{
			if(m_pHeaps[i - 1] && m_pHeaps[i - 1]->IsAllocatedFromThisHeap(pMemory))
			{
				return m_pHeaps[i - 1];
			}
		}

		return NULL;
	}

	//  Description:
	//      Returns the heap where the memory address lies.  Can be useful for finding out general information of where memory came from.  Does not need to be an allocated address,
	//		any will do.
	//  See Also:
	//      
	//  Arguments:
	//		pMemory - Memory address to search heaps against.
	//  Return Value:
	//      A valid cHeapNonIntrusive pointer if a heap contains the memory location.  NULL otherwise.
	//  Summary:
	//      Returns a valid NI Heap where the memory address lies.
	cHeapNonIntrusive *cMemoryManager::FindNIHeapFromMemoryAddress(void *pMemory) const
	{
		for(jrs_u32 i = MemoryManager_MaxNonIntrusiveHeaps; i > 0; i--)
		{
			if(m_pNonInstrusiveHeaps[i - 1] && m_pNonInstrusiveHeaps[i - 1]->IsAllocatedFromThisHeap(pMemory))
			{
				return m_pNonInstrusiveHeaps[i - 1];
			}
		}

		return NULL;
	}

	//  Description:
	//      Gets the default heap allocations will go into when calling cMemoryManager::Malloc.
	//  See Also:
	//		SetMallocDefaultHeap
	//  Arguments:
	//      None
	//  Return Value:
	//      Valid cHeap pointer or NULL.
	//  Summary:	
	//		Gets the default heap allocations will go into when calling cMemoryManager::Malloc.
	cHeap *cMemoryManager::GetDefaultHeap(void)
	{
		// Check to ensure valid heap
		if(!m_uHeapNum)
		{
			MemoryWarning(m_uHeapNum, JRSMEMORYERROR_NOVALIDHEAP, "No valid heap. Use CreateHeap to create one.");
			return NULL;
		}

		// Use the user set heap
		if(m_bOverrideMallocHeap)
			return m_pDefaultMallocHeap;

		return &m_pMemoryHeaps[m_uHeapNum - 1];
	}

	//  Description:
	//      Resets any heap statistics that have accrued since either Initialization or the last time this function was called.
	//  See Also:
	//      cHeap::ResetStatistics
	//  Arguments:
	//		None.
	//  Return Value:
	//      Nothing.
	//  Summary:
	//      Resets all heap statistics.
	void cMemoryManager::ResetHeapStatistics(void)
	{
		for(jrs_u32 i = MemoryManager_MaxUserHeaps; i > 0; i--)
		{
			if(m_pUserHeaps[i - 1])
			{
				m_pUserHeaps[i - 1]->ResetStatistics();
			}
		}

		for(jrs_u32 i = MemoryManager_MaxNonIntrusiveHeaps; i > 0; i--)
		{
			if(m_pNonInstrusiveHeaps[i - 1])
			{
				m_pNonInstrusiveHeaps[i - 1]->ResetStatistics();
			}
		}

		for(jrs_u32 i = MemoryManager_MaxHeaps; i > 0; i--)
		{
			if(m_pHeaps[i - 1])
			{
				m_pHeaps[i - 1]->ResetStatistics();
			}
		}		
	}

	//  Description:
	//      Replaces standard system malloc but allows for more advanced allocation parameters such as alignment. Malloc will
	//		automatically send the allocation to the last created heap, unless a small heap is specified.  If a small heap is
	//		specified and the allocation size is <= the maximum allocation size of the small heap the small heap will try to allocate.
	//		Flags are set by the user.  It can be one of JRSMEMORYFLAG_xxx or any user specified flags > JRSMEMORYFLAG_RESERVED3
	//		but smaller than or equal to 15, values greater than 15 will be lost and operation of Malloc is undefined.  Input text is
	//		limited to 32 chars including terminator.  Strings longer than this will only store the last 31 chars.
	//  See Also:
	//		Free
	//  Arguments:
	//      uSizeInBytes - Size in bytes. Minimum size will be 16bytes unless the heap settings have set a larger minimum size.
	//		uAlignment - Default alignment is 16bytes unless heap settings have set a larger alignment.
	//					 Any specified alignments must be a power of 2. Passing 0 will set it to the correct alignment for the heap.
	//		uFlag - One of JRSMEMORYFLAG_xxx or user defined value.  Default JRSMEMORYFLAG_NONE.  See description for more details.
	//		pText - NULL terminating text string to associate with the allocation.
	//		uExternalId - An external id to associate with the allocation.  Default 0.
	//  Return Value:
	//      Valid pointer to allocated memory.
	//		NULL otherwise.
	//  Summary:
	//      Allocates memory with additional information.
	void *cMemoryManager::Malloc(jrs_sizet uSizeInBytes, jrs_u32 uAlignment, jrs_u32 uFlag, const jrs_i8 *pText, const jrs_u32 uExternalId /* = 0*/)
	{
		// Check to ensure valid heap
		if(!m_uHeapNum)
		{
			MemoryWarning(m_uHeapNum, JRSMEMORYERROR_NOVALIDHEAP, "No valid heap to allocate from.  Use CreateHeap to create one before calling Malloc.");
			return NULL;
		}

		// Do we need to allocate from the small heap
		if(m_pMemorySmallHeap && m_pMemorySmallHeap->GetMaxAllocationSize() >= uSizeInBytes)
		{
			if(m_pMemorySmallHeap->GetDefaultAlignment() == uAlignment || uAlignment == 0)
			{
				// Warn once with an warning when the heap overflows.
#ifndef MEMORYMANAGER_MINIMAL
				static jrs_bool bSmallHeapWarnOverflowOnce = false;
#endif
				void *pMem = m_pMemorySmallHeap->AllocateMemory(uSizeInBytes, uAlignment, uFlag, pText, uExternalId);
				if(pMem)
					return pMem;

				// Warn
#ifndef MEMORYMANAGER_MINIMAL
				// Can only continue if sdf
				if(!m_pMemorySmallHeap->IsOutOfMemoryReturnEnabled())
					return NULL;

				if(!bSmallHeapWarnOverflowOnce)
				{
					DebugOutput("Small heap memory hes been exhausted. Memory will use the default Malloc heap.  You will see this warning only once.");
					bSmallHeapWarnOverflowOnce = true;
				}
#endif
			}
				
		}

		// No just allocate
		return GetDefaultHeap()->AllocateMemory(uSizeInBytes, uAlignment, uFlag, pText, uExternalId);
	}

	//  Description:
	//      Replaces standard system malloc but allows for more advanced allocation parameters such as alignment. Malloc will
	//		automatically send the allocation to the last created heap, unless a small heap is specified.  If a small heap is
	//		specified and the allocation size is <= the maximum allocation size of the small heap the small heap will try to allocate.
	//		Allocation flag defaults to JRSMEMORYFLAG_NONE and passed in string is NULL.  In NAC or NACS libraries the string will
	//		be given a default value of 'Unknown'.
	//  See Also:
	//		Free, Realloc
	//  Arguments:
	//      uSizeInBytes - Size in bytes. Minimum size will be 16bytes unless the heap settings have set a larger minimum size.
	//		uAlignment - Default alignment is 16bytes unless heap settings have set a larger alignment.
	//					 Any specified alignments must be a power of 2. Passing 0 will set it to the correct alignment for the heap.
	//  Return Value:
	//      Valid pointer to allocated memory.
	//		NULL otherwise.
	//  Summary:	
	//		Allocates memory with standard values.
	void *cMemoryManager::Malloc(jrs_sizet uSizeInBytes, jrs_u32 uAlignment)
	{
		// No just allocate
		return Malloc(uSizeInBytes, uAlignment, JRSMEMORYFLAG_NONE, NULL);
	}

	//  Description:
	//      Frees allocated memory. Elephant will automatically search for the heap the allocation was allocated from (See note).
	//		DeAllocation flag defaults to JRSMEMORYFLAG_NONE.  In NAC or NACS libraries the string associated with the free will
	//		be given a default value.
	//		Note: If heaps are created from memory allocated within other heaps this function may corrupt memory. If you are allocating
	//		self managed heaps or heaps with addresses from memory allocated by Elephant in Initialize it is recommended that you do not use this
	//		function to avoid potential errors.
	//  See Also:
	//		Free, Malloc, Realloc
	//  Arguments:
	//      pMemory - Valid memory address.  A NULL value may cause a warning depending on the settings of the heap.
	//		uFlag - Defaults to JRSMEMORYFLAG_NONE but can be other user specified values.
	//  Return Value:
	//      Nothing.
	//  Summary:	
	//		Frees some previously allocated memory.
	void cMemoryManager::Free(void *pMemory, jrs_u32 uFlag)
	{
		Free(pMemory, uFlag, NULL);
	}

	//  Description:
	//      Reallocates a block of memory. This works just like standard realloc.  Passing in pMemory as NULL will perform a standard malloc.  Passing 0 
	//		as a size will free the memory.  Any other size will resize the allocation.  In most situations this may be performed as a malloc, copy, free
	//		operation.
	//  See Also:
	//		Free, Malloc
	//  Arguments:
	//      pMemory - Valid memory address.  A NULL value will cause a standard allocation to the default heap to be performed.
	//		uSizeInBytes - Size in bytes of the memory to resize.  0 causes a free operation to be performed.
	//		uFlag - Defaults to JRSMEMORYFLAG_NONE but can be other user specified values.
	//		pText - 32char NULL terminated text string to associate with the memory.
	//  Return Value:
	//      Valid memory pointer if memory is valid.  NULL otherwise.
	//  Summary:	
	//		Reallocates a block of memory. 
	void *cMemoryManager::Realloc(void *pMemory, jrs_sizet uSizeInBytes, jrs_u32 uAlignment, jrs_u32 uFlag, const jrs_i8 *pText)
	{
		// If memory is null then we just allocate as normal
		if(!pMemory)
			return Malloc(uSizeInBytes, uAlignment, uFlag, pText);

		// Memory isn't null, it can be reallocated.  We reallocate to the same heap it came from.
		cHeap *pHeap = FindHeapFromMemoryAddress(pMemory);
		if(!pHeap)
		{
			MemoryWarning(pHeap, JRSMEMORYERROR_UNKNOWNADDRESS, "Heap could not be found for memory allocation. Realloc has failed");
			return NULL;
		}

		// Do we have a small heap?
		if(m_pMemorySmallHeap && pHeap == m_pMemorySmallHeap)
		{
			// Just free and recall malloc
			Free(pMemory);
			return Malloc(uSizeInBytes, uAlignment, uFlag, pText);
		}

		// Reallocate it
		return pHeap->ReAllocateMemory(pMemory, uSizeInBytes, uAlignment, uFlag, pText);
	}

	//  Description:
	//      Frees allocated memory. Elephant will automatically search for the heap the allocation was allocated from (See note).
	//		DeAllocation flag defaults to JRSMEMORYFLAG_NONE.  In NAC or NACS libraries the string associated with the free will
	//		be given a default value otherwise a custom value may be assigned.  See Malloc for futher flag values.  This function is identical to the other cMemoryManager::Free
	//		except that it allows a string to be associated with the free.
	//		Note: If heaps are created from memory allocated within other heaps this function may corrupt memory. If you are allocating
	//		self managed heaps or heaps with addresses from memory allocated by Elephant in Initialize it is recommended that you do not use this
	//		function to avoid potential errors.
	//  See Also:
	//		Free, Malloc
	//  Arguments:
	//      pMemory - Valid memory address.  A NULL value may cause a warning depending on the settings of the heap.
	//		uFlag - Defaults to JRSMEMORYFLAG_NONE but can be other user specified values.
	//		pText - 32 byte including terminator value string to be associated with the free. 
	//  Return Value:
	//      Nothing.
	//  Summary:	
	//		Frees some previously allocated memory.
	void cMemoryManager::Free(void *pMemory, jrs_u32 uFlag, const jrs_i8 *pText)
	{
		// Null frees just get returned
		if(!pMemory)
			return;

		// Do the user heaps first.  Sounds odd but the user heaps MAY come from the main heap.  Then any data that gets free'd through this function
		// will corrupt that main heap as it will be detected from that heap.
		for(jrs_u32 i = MemoryManager_MaxUserHeaps; i > 0; i--)
		{
			if(m_pUserHeaps[i - 1] && m_pUserHeaps[i - 1]->IsAllocatedFromThisHeap(pMemory))
			{
				m_pUserHeaps[i - 1]->FreeMemory(pMemory, uFlag, pText);
				return;
			}
		}

		// Find out what heap it is from.  Done in reverse because typically they will be done on the last heap using these methods.  This will
		// also catch the small heap.
		for(jrs_u32 i = m_uHeapNum; i > 0; i--)
		{
			if(m_pHeaps[i - 1]->IsAllocatedFromThisHeap(pMemory))
			{
				m_pHeaps[i - 1]->FreeMemory(pMemory, uFlag, pText);
				return;
			}
		}

		MemoryWarning(0, JRSMEMORYERROR_UNKNOWNADDRESS, "Memory could not be found allocated from any of the memory managers heaps.");
	}

	//  Description:
	//      Logs a memory marker to the continuous logging information.  Goldfish will use this value as a marker also for its continuous
	//		views.
	//  See Also:
	//		
	//  Arguments:
	//      pMarkerName - NULL terminated string of the name of the marker to log.
	//  Return Value:
	//      Nothing.
	//  Summary:	
	//		Adds a marker to the log.
	void cMemoryManager::LogMemoryMarker(const jrs_i8 *pMarkerName)
	{
		MemoryWarning(pMarkerName, JRSMEMORYERROR_INVALIDMARKER, "Marker name is invalid");
		ContinuousLogging_Operation(eContLog_Marker, (cHeap *)pMarkerName, NULL, 0);
	}

	//  Description:
	//      Returns the size of the allocated memory allocated with Malloc or one of the heap allocation functions.
	//  See Also:
	//		Malloc
	//  Arguments:
	//      pMemory - Valid memory address.  A NULL value may cause a warning depending on the settings of the heap.
	//  Return Value:
	//      Size in bytes of the allocation.
	//  Summary:	
	//		Gets the size in bytes of the allocated block of memory.
	jrs_sizet cMemoryManager::SizeofAllocation(void *pMemory) const
	{
		MemoryWarning(pMemory, JRSMEMORYERROR_UNKNOWNADDRESS, "Not a valid allocation.");
		sAllocatedBlock *pB = (sAllocatedBlock *)pMemory - 1;
		return pB->uSize;
	}

	//  Description:
	//      Returns the size of the allocated memory allocated with Malloc or one of the heap allocation functions.  This size is the actual size of the allocated
	//		block which may be larger than the size returned by SizeofAllocation.
	//  See Also:
	//		Malloc, SizeofAllocation
	//  Arguments:
	//      pMemory - Valid memory address.  A NULL value may cause a warning depending on the settings of the heap.
	//  Return Value:
	//      Size in bytes of the allocation.
	//  Summary:	
	//		Gets the size in bytes of the allocated block of memory.
	jrs_sizet cMemoryManager::SizeofAllocationAligned(void *pMemory) const
	{
		MemoryWarning(pMemory, JRSMEMORYERROR_UNKNOWNADDRESS, "Not a valid allocation.");
		cHeap *pHeap = FindHeapFromMemoryAddress(pMemory);
		sAllocatedBlock *pB = (sAllocatedBlock *)pMemory - 1;
		
		return (jrs_sizet)(HEAP_FULLSIZE_CALC(pB->uSize, pHeap->GetMinAllocationSize()));
	}

	//  Description:
	//      Returns the size of the allocation header.  This will be 16 bytes (32 for 64bit) normally but different library configurations
	//		may change this.
	//  See Also:
	//		SizeofAllocation, SizeofFreeBlock
	//  Arguments:
	//      None
	//  Return Value:
	//      Size in bytes of the allocation header.
	//  Summary:	
	//		Gets the size in bytes of the allocation header.
	jrs_sizet cMemoryManager::SizeofAllocatedBlock(void)
	{
		return sizeof(sAllocatedBlock);
	}

	//  Description:
	//      Returns the size of the free header.  This is typically always 16bytes larger than the allocation header.
	//  See Also:
	//		SizeofAllocation, SizeofAllocationBlock
	//  Arguments:
	//      None
	//  Return Value:
	//      Size in bytes of the free header.
	//  Summary:	
	//		Gets the size in bytes of the free header.
	jrs_sizet cMemoryManager::SizeofFreeBlock(void)
	{
		return sizeof(sFreeBlock);
	}

	//  Description:
	//      Returns the flag attached to the allocation.
	//  See Also:
	//		
	//  Arguments:
	//      None
	//  Return Value:
	//      The flag attached to the allocation.
	//  Summary:	
	//		Returns the flag attached to the allocation.
	jrs_u32 cMemoryManager::GetAllocationFlag(void *pMemory) const
	{
		MemoryWarning(pMemory, JRSMEMORYERROR_INVALIDALLOCBLOCK, "Not a valid allocation.");
		sAllocatedBlock *pB = (sAllocatedBlock *)pMemory - 1;
		return pB->uFlagAndUniqueAllocNumber & 15;
	}

	//  Description:
	//      Sets the default heap for malloc to use.  Otherwise it uses the last created user heap.
	//  See Also:
	//		GetDefaultHeap
	//  Arguments:
	//      pHeap - Valid heap to force allocations to allocated to.  NULL to revert to the last created heap.
	//  Return Value:
	//      None
	//  Summary:	
	//		Sets the default heap for malloc to use.  Otherwise it uses the last created user heap.
	void cMemoryManager::SetMallocDefaultHeap(cHeap *pHeap)
	{
		if(!pHeap)
		{
			m_bOverrideMallocHeap = false;
			m_pDefaultMallocHeap = GetDefaultHeap();
		}
		else
		{
			m_bOverrideMallocHeap = true;
			m_pDefaultMallocHeap = pHeap;
		}
	}

	//  Description:
	//		Runs a full check on all heaps to see if their are any errors like overruns.  Must be using NACS or S libraries 
	//		for this to be performed.  It is thorough but not a quick operation.
	//  See Also:
	//		
	//  Arguments:
	//      None
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Checks all heaps for any errors.
	void cMemoryManager::CheckForErrors(void)
	{
		for(jrs_u32 i = 0; i < m_uHeapNum; i++)
		{
			if(m_pHeaps[i])
				m_pHeaps[i]->CheckForErrors();
		}

		for(jrs_u32 i = 0; i < MemoryManager_MaxUserHeaps; i++)
		{
			if(m_pUserHeaps[i])
			{
				m_pUserHeaps[i]->CheckForErrors();
			}
		}
	}

	//  Description:
	//		Does a full memory report (statistics and allocations in memory order) to the user TTY callback.  If you want to report these to a file
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
	void cMemoryManager::ReportAll(const jrs_i8 *pLogToFile, jrs_bool includeFreeBlocks, jrs_bool displayCallStack)
	{
		// Used to determine where the reporting comes from
		g_ReportHeap = true;
		g_ReportHeapCreate = true;

		for(jrs_u32 i = 0; i < m_uHeapNum; i++)
		{
			if(m_pHeaps[i])
				m_pHeaps[i]->ReportAll(pLogToFile, includeFreeBlocks, displayCallStack);
		}

		// And the user heap
		for(jrs_u32 i = 0; i < MemoryManager_MaxUserHeaps; i++)
		{
			if(m_pUserHeaps[i])
			{
				// Report
				m_pUserHeaps[i]->ReportAll(pLogToFile, includeFreeBlocks, displayCallStack);
			}
		}

		// And the NI heap
		for(jrs_u32 i = 0; i < MemoryManager_MaxNonIntrusiveHeaps; i++)
		{
			if(m_pNonInstrusiveHeaps[i])
			{
				// Report
				m_pNonInstrusiveHeaps[i]->ReportAll(pLogToFile, includeFreeBlocks, displayCallStack);
			}
		}

		// Rest
		g_ReportHeap = false;
		g_ReportHeapCreate = true;
	}

	//  Description:
	//		Reports basic statistics about all heaps to the user TTY callback. Use this to get quick information on 
	//		when need that is more detailed that simple heap memory realtime calls.
	//  See Also:
	//		ReportAll, ReportAllocationsMemoryOrder
	//  Arguments:
	//		bAdvanced - When true displays bins and other counts.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Reports basic statistics about all heaps to the user TTY callback. 
	void cMemoryManager::ReportStatistics(jrs_bool bAdvanced)
	{
		for(jrs_u32 i = 0; i < MemoryManager_MaxHeaps; i++)
		{
			if(m_pHeaps[i])
			{
				// Report
				m_pHeaps[i]->ReportStatistics(bAdvanced);
			}
		}

		for(jrs_u32 i = 0; i < MemoryManager_MaxUserHeaps; i++)
		{
			if(m_pUserHeaps[i])
			{
				// Report
				m_pUserHeaps[i]->ReportStatistics(bAdvanced);
			}
		}

		for(jrs_u32 i = 0; i < MemoryManager_MaxNonIntrusiveHeaps; i++)
		{
			if(m_pNonInstrusiveHeaps[i])
			{
				// Report
				m_pNonInstrusiveHeaps[i]->ReportStatistics(bAdvanced);
			}
		}
	}

	//  Description:
	//		Reports on all the allocations in every heap.  This function is like ReportAll but doesnt output the statistics.  Calling
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
	void cMemoryManager::ReportAllocationsMemoryOrder(const jrs_i8 *pLogToFile, jrs_bool includeFreeBlocks, jrs_bool displayCallStack)
	{
		for(jrs_u32 i = 0; i < MemoryManager_MaxHeaps; i++)
		{
			if(m_pHeaps[i])
			{
				m_pHeaps[i]->ReportAllocationsMemoryOrder(pLogToFile, includeFreeBlocks, displayCallStack);
			}
		}

		for(jrs_u32 i = 0; i < MemoryManager_MaxUserHeaps; i++)
		{
			if(m_pUserHeaps[i])
			{
				m_pUserHeaps[i]->ReportAllocationsMemoryOrder(pLogToFile, includeFreeBlocks, displayCallStack);
			}
		}

		for(jrs_u32 i = 0; i < MemoryManager_MaxNonIntrusiveHeaps; i++)
		{
			if(m_pNonInstrusiveHeaps[i])
			{
				m_pNonInstrusiveHeaps[i]->ReportAllocationsMemoryOrder(pLogToFile, includeFreeBlocks, displayCallStack);
			}
		}
	}
	
	//  Description:
	//		Forces a ReportAll to be sent to Goldfish.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Forces a ReportAll to be sent to Goldfish.
	void cMemoryManager::ReportAllToGoldfish(void)
	{
		if(m_bLiveViewRunning && !m_bForceLVOverviewGrab)
		{
			m_bForceLVOverviewGrab = TRUE;

			// Wait a little while so it can go
			JRSThread::SleepMilliSecond(100);
		}
	}

	//  Description:
	//		Starts a continuous grab to be sent to Goldfish.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Starts a continuous grab to be sent to Goldfish.
	void cMemoryManager::ReportContinuousStartToGoldfish(void)
	{
		if(m_bLiveViewRunning && !m_bForceLVContinuousGrab)
		{
			m_bForceLVContinuousGrab = TRUE;
		}		
	}

	//  Description:
	//		Stops a previously started continuous grab being sent to Goldfish.
	//  See Also:
	//		
	//  Arguments:
	//		None
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Stops a previously started continuous grab being sent to Goldfish.
	void cMemoryManager::ReportContinuousStopToGoldfish(void)
	{
		if(m_bLiveViewRunning && m_bELVContinuousGrab)
		{
			m_bForceLVContinuousGrab = TRUE;
		}		
	}

	//  Description:
	//		Standard Elephant debug output generator to pass to user TTY callback.
	//  See Also:
	//		
	//  Arguments:
	//		pText - Standard text string. 2k - 1 max length.
	//		... - variable args.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Internal debug output generation.
	void cMemoryManager::DebugOutput(const jrs_i8 *pText, ...)
	{
		// Only if debugging is valid
		if(cMemoryManager::m_MemoryManagerTTYOutput)
		{
			jrs_i8 logtext[2048];
			va_list args;

			// Create the string
			va_start(args, pText);
			vsnprintf(logtext, 2047, pText, args);
			va_end(args);  

			cMemoryManager::m_MemoryManagerTTYOutput(logtext);
		}
	}

	//  Description:
	//		Standard Elephant debug output to file generator to pass to user TTY callback.
	//  See Also:
	//		
	//  Arguments:
	//		pFile - File name of output.
	//		bCreate - TRUE if the file is to be recreated/cleared.  FALSE to append.
	//		pText - Standard text string. 2k - 1 max length.
	//		... - variable args.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Internal debug output generation.
	void cMemoryManager::DebugOutputFile(const jrs_i8 *pFile, jrs_bool bCreate, const jrs_i8 *pText, ...)
	{
		// Can we write the file?
		if(m_MemoryManagerFileOutput || m_MemoryManagerLiveViewOutput)
		{
			jrs_i8 OutputText[2048];

			// Write the file
			va_list args;

			// Create the string
			va_start(args, pText);
			vsnprintf(OutputText, 2046, pText, args);
			va_end(args); 

			strcat(OutputText, "\n");

			if(m_MemoryManagerFileOutput && pFile)
				m_MemoryManagerFileOutput(OutputText, (int)strlen(OutputText), pFile, !bCreate);

			if(m_MemoryManagerLiveViewOutput)
				m_MemoryManagerLiveViewOutput(OutputText, (int)strlen(OutputText), pFile, !bCreate);
		}
	}

	//  Description:
	//		Standard Elephant debug error generator.
	//  See Also:
	//		
	//  Arguments:
	//		uErrorCode - Error code.  Normally just line number.
	//		pText - Standard text string. 2k - 1 max length.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Internal debug output generation.
	void cMemoryManager::DebugError(jrs_u32 uErrorCode, const jrs_i8 *pText)
	{
		if(cMemoryManager::m_MemoryManagerError)
		{
			if(m_bLiveViewRunning)
			{
				// Flush any pending
				JRSThread::SleepMilliSecond(m_uLiveViewPoll * 100);
			}

			cMemoryManager::m_MemoryManagerError(pText, uErrorCode);
		}
	}

	//  Description:
	//		Standard Elephant debug error generator.
	//  See Also:
	//		
	//  Arguments:
	//		pHeap - Valid pHeap for heap error or NULL for manager error.
	//		pPool - Valid pPool for pool error or NULL for manager error.
	//		uErrorCode - Error code.  Normally just line number.
	//		pFunction - Name of the function.
	//		pText - Standard text string. 2k - 1 max length.
	//		... - variable args.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Internal debug output generation.
	void cMemoryManager::DebugWarning(cHeap *pHeap, cPoolBase *pPool, jrs_u32 uErrorCode, const jrs_i8 *pFunction, const jrs_i8 *pText, ...)
	{
		// We always do this for warnings.  We will print them out.  
		jrs_i8 logtext[2048];
		va_list args;

		sprintf(logtext, "Error 0x%x: ", uErrorCode);

		// Create the string
		va_start(args, pText);
		vsnprintf(&logtext[strlen(logtext)], 2047, pText, args);
		va_end(args);  

		if(cMemoryManager::m_MemoryManagerTTYOutput)
		{
			if(pHeap && pHeap->AreErrorsEnabled())
			{
				cMemoryManager::m_MemoryManagerTTYOutput(logtext);
				if(!pHeap->AreErrorsWarningsOnly())
				{
					if(pHeap->m_bEnableReportsInErrors)
						pHeap->ReportStatistics(false);
					DebugError(uErrorCode, logtext);
				}
			}
			else if(pPool && pPool->AreErrorsEnabled())
			{
				cMemoryManager::m_MemoryManagerTTYOutput(logtext);
				if(!pPool->AreErrorsWarningsOnly())
				{
					DebugError(uErrorCode, logtext);
				}
			}
			else
			{
				cMemoryManager::m_MemoryManagerTTYOutput(logtext);
				DebugError(uErrorCode, logtext);
			}
		}
	}

	//  Description:
	//		Standard Elephant debug error generator.
	//  See Also:
	//		
	//  Arguments:
	//		pHeap - Valid pHeap for heap error or NULL for manager error.
	//		pPool - Valid pPool for pool error or NULL for manager error.
	//		uErrorCode - Error code.  Normally just line number.
	//		pFunction - Name of the function.
	//		pText - Standard text string. 2k - 1 max length.
	//		... - variable args.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Internal debug output generation.
	void cMemoryManager::DebugWarning(cHeapNonIntrusive *pHeap, cPoolBase *pPool, jrs_u32 uErrorCode, const jrs_i8 *pFunction, const jrs_i8 *pText, ...)
	{
		// We always do this for warnings.  We will print them out.  
		jrs_i8 logtext[2048];
		va_list args;

		sprintf(logtext, "Error 0x%x: ", uErrorCode);

		// Create the string
		va_start(args, pText);
		vsnprintf(&logtext[strlen(logtext)], 2047, pText, args);
		va_end(args);  

		if(cMemoryManager::m_MemoryManagerTTYOutput)
		{
			if(pHeap && pHeap->AreErrorsEnabled())
			{
				cMemoryManager::m_MemoryManagerTTYOutput(logtext);
				if(!pHeap->AreErrorsWarningsOnly())
				{
					if(pHeap->m_bEnableReportsInErrors)
						pHeap->ReportStatistics(false);
					DebugError(uErrorCode, logtext);
				}
			}
			else if(pPool && pPool->AreErrorsEnabled())
			{
				cMemoryManager::m_MemoryManagerTTYOutput(logtext);
				if(!pPool->AreErrorsWarningsOnly())
				{
					DebugError(uErrorCode, logtext);
				}
			}
			else
			{
				cMemoryManager::m_MemoryManagerTTYOutput(logtext);
				DebugError(uErrorCode, logtext);
			}
		}
	}

	//  Description:
	//		Returns the amount of free usable memory that Elephant has to allocate managed heaps from.  This does not include 
	//		memory taken with self managed heaps.
	//  See Also:
	//		Initialize
	//  Arguments:
	//		Nothing
	//  Return Value:
	//      The amount of remaining memory available to Elephant for managed heaps.
	//  Summary:	
	//		Returns the amount of free usable memory that Elephant has to allocate managed heaps from.
	jrs_u64 cMemoryManager::GetFreeUsableMemory(void) const 
	{ 
		return (jrs_u64)((jrs_i8 *)m_pUseableMemoryEnd - (jrs_i8 *)m_pUsableHeapMemoryStart); 
	}

	//  Description:
	//		Returns the system page size.
	//  See Also:
	//		Initialize
	//  Arguments:
	//		Nothing
	//  Return Value:
	//      System page size.
	//  Summary:	
	//		Returns the system page size.
	jrs_sizet cMemoryManager::GetSystemPageSize(void) const
	{
		return m_uSystemPageSize;
	}

	//  Description:
	//		Returns the system page size.
	//  See Also:
	//		Initialize
	//  Arguments:
	//		Nothing
	//  Return Value:
	//      System page size.
	//  Summary:	
	//		Returns the system page size.
	jrs_u16 cMemoryManager::GetLVPortNumber(void) const
	{
		return m_uLVPort;
	}

	//  Description:
	//		Used to determine if the memory manager is initialized or not.
	//  See Also:
	//		Initialize
	//  Arguments:
	//		Nothing
	//  Return Value:
	//      TRUE if initialized.
	//		FALSE otherwise.
	//  Summary:	
	//		Determines if the memory manager is initialized or not.
	jrs_bool cMemoryManager::IsInitialized(void) const 
	{ 
		return m_bInitialized; 
	}

	//  Description:
	//		Tries to reclaim on all cHeaps.
	//  See Also:
	//		cHeap::Reclaim
	//  Arguments:
	//		Nothing
	//  Return Value:
	//      None
	//  Summary:	
	//		Tries to reclaim on all cHeaps.
	void cMemoryManager::Reclaim(void)
	{
		for(jrs_u32 i = 0; i < MemoryManager_MaxHeaps; i++)
		{
			if(m_pHeaps[i])
			{
				m_pHeaps[i]->Reclaim();
			}
		}
	}

	//  Description:
	//		Enables or disables continuous logging.  This is used to control output to a continuous log file and TYY using the user callback functions.
	//  See Also:
	//		IsLoggingEnabled
	//  Arguments:
	//		bEnable - TRUE to enable logging.  FALSE to disable.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Enables or disables continuous logging.
	void cMemoryManager::EnableLogging(jrs_bool bEnable)
	{
		m_bEnableContinuousDump = bEnable;
		ContinuousLogging_Operation(eContLog_StartLogging, NULL, NULL, 0);
	}

	//  Description:
	//		Determines if logging is currently enabled or not.
	//  See Also:
	//		EnableLogging
	//  Arguments:
	//		None
	//  Return Value:
	//      TRUE if logging is currently enabled.
	//		FALSE otherwise.
	//  Summary:	
	//		Determines if logging is currently enabled or not.
	jrs_bool cMemoryManager::IsLoggingEnabled(void) const 
	{ 
		return m_bEnableContinuousDump; 
	}


	//  Description:
	//		Creates a callstack to a string
	//  See Also:
	//		ReportAll
	//  Arguments:
	//		pOutputBuffer - Pointer to the start of the string to append to.
	//		pCallstack - Pointer to the callstack values
	//		uCallStackCount - Number of callstack values
	//  Return Value:
	//      None
	//  Summary:	
	//		Creates a callstack to a string
	void cMemoryManager::StackToString(jrs_i8 *pOutputBuffer, jrs_sizet *pCallstack, jrs_u32 uCallStackCount)
	{
		char outbuffer[386];
		for(jrs_u32 i = 0; i < uCallStackCount; i++)
		{
			jrs_sizet uFuncStartAdd = 0;
			jrs_sizet uFuncSize = 0;
			outbuffer[0] = ' ';
			outbuffer[1] = '\0';
			MemoryManagerPlatformFunctionNameFromAddress(pCallstack[i], &outbuffer[1], &uFuncStartAdd, &uFuncSize);
			if(uFuncSize == 0)
			{
#ifdef JRS64BIT
				sprintf(outbuffer, " 0x%llu", (jrs_u64)pCallstack[i]);
#else
				sprintf(outbuffer, " 0x%u", (jrs_u32)pCallstack[i]);
#endif
			}
			strcat(pOutputBuffer, outbuffer);
		}
	}

	//  Description:
	//		Internal only.  Inserts the continuous data into the ring buffer.
	//  See Also:
	//		ContinuousLogging_PoolOperation
	//  Arguments:
	//		rOp - Operation details to add to the list.
	//		pData - Data operation to add to the list.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Reports any operations that occur on a specific heap.  These cover Allocations, Frees and Reallocs.  Reallocs may not always show.
	void cMemoryManager::ContinuousLog_AddToBuffer(sLVOperation &rOp, void *pData)
	{
#ifndef MEMORYMANAGER_MINIMAL
		// Only if active
		if(!m_bELVContinuousGrab)
			return;

		// Prevent multiple locks taking place while adding data.
		while(m_bLiveViewRunning && m_bELVContinuousGrab)
		{
			m_LVThreadLock.Lock();

			jrs_u32 uCurRead = m_uELVDBRead;
			jrs_u32 uCurWrite = m_uELVDBWrite;
			jrs_u32 uCurWriteEnd = m_uELVDBWriteEnd;

			jrs_u32 uReadMsb = uCurRead & 1;
			jrs_u32 uWriteMsb = uCurWrite & 1;

			uCurRead &= ~1;
			uCurWrite &= ~1;

			jrs_u32 uSizeToAdd = sizeof(sLVOperation) + rOp.sizeofopdata;
			jrs_u32 uNewWriteEnd = uCurWrite + uSizeToAdd;

			if(uReadMsb == uWriteMsb)
			{
				// Read is ALWAYS behind write.

				// Same ring
				if(uNewWriteEnd > m_uELVDebugBufferSize)
				{
					// If the wrap ptr is valid we cant do anything yet
					if(!uCurWriteEnd)
					{
						// Loop around
						m_uELVDBWriteEnd = uCurWrite;
						m_uELVDBWrite = 0 | (uWriteMsb ^ 1);
					}					
				}
				else
				{
					// We can add
					memcpy((void *)(m_pELVDebugBuffer + uCurWrite), &rOp, sizeof(sLVOperation));
					if(pData)
						memcpy((void *)(m_pELVDebugBuffer + uCurWrite + sizeof(rOp)), pData, rOp.sizeofopdata);	

					m_uELVDBWrite = uNewWriteEnd | (uWriteMsb);
					m_LVThreadLock.Unlock();
					return;				
				}
			}	
			else
			{
				// Read is AHEAD of write
				if(uNewWriteEnd > uCurRead)
				{
					// Wait
				}
				else
				{
					// We can add
					memcpy((void *)(m_pELVDebugBuffer + uCurWrite), &rOp, sizeof(sLVOperation));
					if(pData)
						memcpy((void *)(m_pELVDebugBuffer + uCurWrite + sizeof(rOp)), pData, rOp.sizeofopdata);	

					m_uELVDBWrite = uNewWriteEnd | (uWriteMsb);
					m_LVThreadLock.Unlock();
					return;				
				}
			}

			m_LVThreadLock.Unlock();
		}
#endif
	}

	//  Description:
	//		Internal only.  This function add data to the internal ring buffer of operations to be transmitted.  This specific function deals with 
	//		the heap operations.
	//  See Also:
	//		ContinuousLogging_Operation, ContinuousLogging_PoolOperation
	//  Arguments:
	//		eType - Type of Logging operation.
	//		pHeap - Heap pointer.
	//		pHeaderAdd - Header address of the operation.
	//		uAlignment - Alignment of the operation.
	//		uMisc - Misc information.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Reports any operations that occur on a specific heap.  These cover Allocations, Frees and Reallocs.  Reallocs may not always show.
	void cMemoryManager::ContinuousLogging_HeapOperation(eContLog eType, cHeap *pHeap, void *pHeaderAdd, jrs_u32 uAlignment, jrs_u64 uMisc)
	{
#ifndef MEMORYMANAGER_MINIMAL
		// Only if active
		if(!ContinuousLog_CanLog(pHeap))
			return;

		// Lock the thread writes
		m_ContThreadLock.Lock();

		jrs_u8 flags = 0;
#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
		flags |= 1 << 7;
#endif

#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
		flags |= 1 << 6;
#endif

		jrs_bool bFileAppend = TRUE;
		jrs_i8 ContinuousOutputText[2048];

		jrs_u64 heapSize = (jrs_u64)pHeap->GetSize();
		jrs_u64 heapMemSize = (jrs_u64)pHeap->GetMemoryUsed();
		jrs_u64 heapMemCount = (jrs_u64)pHeap->GetNumberOfAllocations();
		jrs_u64 heapDetails = (((MemoryManager_StringLength << 9) | (0 << 8) | JRSMEMORY_CALLSTACKDEPTH) << 16);	// Always freeblock as thats the largest.

		sLVOperation op;
		if(eType == eContLog_Allocate)
		{
			op.type = (jrs_u8)eContLog_Allocate | flags;
			op.address = (jrs_u64)pHeaderAdd;
			op.sizeofopdata = (jrs_u8)cMemoryManager::Get().SizeofAllocatedBlock();
			op.alignment = uAlignment;
			op.idofheappool = (jrs_u16)pHeap->GetUniqueId();
			op.extraInfo0 = heapMemCount;
			op.extraInfo1 = heapMemSize;
			op.extraInfo2 = heapSize;
			op.extraInfo3 = heapDetails | cMemoryManager::Get().SizeofAllocatedBlock();

			// Log it out if needed
			if(m_bEnableContinuousDump)
			{
				sAllocatedBlock *pBlock = (sAllocatedBlock *)pHeaderAdd;
				sprintf(ContinuousOutputText, "Heap Alloc; %s; 0x%llx; 0x%llx; 0x%x; 0x%x; 0x%x; 0x%llx; 0x%llx; 0x%llx; 0x%llx"
					, pHeap->GetName()
					, (jrs_u64)(((jrs_i8 *)pHeaderAdd) + sizeof(sAllocatedBlock))
					, (jrs_u64)pBlock->uSize
					, uAlignment
					, (jrs_u32)(pBlock->uFlagAndUniqueAllocNumber & 0xf), (jrs_u32)(pBlock->uFlagAndUniqueAllocNumber >> 4)
					, heapMemCount
					, heapMemSize
					, heapSize
					, heapDetails);

#ifndef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
				strcat(ContinuousOutputText, "Unknown");					
#else
				strcat(ContinuousOutputText, pBlock->Name);
#endif
				for(jrs_u32 cs = 0; cs < JRSMEMORY_CALLSTACKDEPTH; cs++)
				{
#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
					jrs_i8 temp[32];
					sprintf(temp, "; 0x%llx", (jrs_u64)MemoryManagerPlatformAddressToBaseAddress(pBlock->uCallsStack[cs]));
					strcat(ContinuousOutputText, temp);
#else
					strcat(ContinuousOutputText, "; 0x0");
#endif
				}
				strcat(ContinuousOutputText, "\n");
			}
		}
		else if(eType == eContLog_Free)
		{
			op.type = (jrs_u8)eContLog_Free | flags;
			op.address = (jrs_u64)pHeaderAdd;
			op.sizeofopdata = (jrs_u8)cMemoryManager::Get().SizeofFreeBlock();
			op.alignment = (jrs_u32)uMisc;
			op.idofheappool = (jrs_u16)pHeap->GetUniqueId();
			op.extraInfo0 = heapMemCount;
			op.extraInfo1 = heapMemSize;
			op.extraInfo2 = heapSize;
			op.extraInfo3 = heapDetails | cMemoryManager::Get().SizeofAllocatedBlock();

			if(m_bEnableContinuousDump)
			{
				sFreeBlock *pBlock = (sFreeBlock *)pHeaderAdd;
				sprintf(ContinuousOutputText, "Heap Free; %s; 0x%llx; 0x%llx; 0x%x; 0x%x; 0x%llx; 0x%llx; 0x%llx; 0x%llx; 0x%llx"
					, pHeap->GetName()
					, (jrs_u64)(((jrs_i8 *)pHeaderAdd) + sizeof(sAllocatedBlock))
					, (jrs_u64)pBlock->uSize
					, uAlignment
					, 0, (jrs_u64)pBlock->uFlags		// flag/id
					, heapMemCount
					, heapMemSize
					, heapSize
					, heapDetails);


#ifndef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
				strcat(ContinuousOutputText, "Unknown");					
#else
				strcat(ContinuousOutputText, pBlock->Name);
#endif
				for(jrs_u32 cs = 0; cs < JRSMEMORY_CALLSTACKDEPTH; cs++)
				{
#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
					jrs_i8 temp[32];
					sprintf(temp, "; 0x%llx", (jrs_u64)MemoryManagerPlatformAddressToBaseAddress(pBlock->uCallsStack[cs]));
					strcat(ContinuousOutputText, temp);
#else
					strcat(ContinuousOutputText, "; 0x0");
#endif
				}
				strcat(ContinuousOutputText, "\n");
			}
		}
		else if(eType == eContLog_ReAlloc)
		{
			op.type = (jrs_u8)eContLog_ReAlloc | flags;
			op.address = (jrs_sizet)pHeaderAdd;
			op.sizeofopdata = (jrs_u8)cMemoryManager::Get().SizeofFreeBlock();
			op.alignment = uAlignment;
			op.idofheappool = pHeap->GetUniqueId();
			op.extraInfo0 = op.extraInfo1 = op.extraInfo2 = op.extraInfo3 = 0;
		}
		else 
		{
			MemoryWarning(0, JRSMEMORYERROR_UNKNOWNWARNING, "Wrong type for ContinuousLogging_HeapOperation.");
		}

		// Write CSV file if need be
		if(m_bEnableContinuousDump && m_MemoryManagerFileOutput)
			m_MemoryManagerFileOutput(ContinuousOutputText, (int)strlen(ContinuousOutputText), m_ContinuousDumpFile, bFileAppend);
				
		// Add the data
		ContinuousLog_AddToBuffer(op, pHeaderAdd);

		// Unlock the continuous writes
		m_ContThreadLock.Unlock();
#endif
	}

	//  Description:
	//		Internal only.  This function add data to the internal ring buffer of operations to be transmitted.  This specific function deals with 
	//		the heap operations.
	//  See Also:
	//		ContinuousLogging_Operation
	//  Arguments:
	//		eType - Type of Logging operation.
	//		pHeap - Heap pointer.
	//		pHeaderAdd - Header address of the operation.
	//		uAlignment - Alignment of the operation.
	//		uMisc - Misc information.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Reports any operations that occur on a specific non intrusive heap.  These cover Allocations, Frees and Reallocs.  Reallocs may not always show.
	void cMemoryManager::ContinuousLogging_HeapNIOperation(eContLog eType, cHeapNonIntrusive *pHeap, void *pHeaderAdd, jrs_u32 uAlignment, jrs_u64 uSize, jrs_u64 uMisc)
	{
#ifndef MEMORYMANAGER_MINIMAL
		// Only if active
		if(!ContinuousLog_CanLog(pHeap))
			return;

		// Lock the thread writes
		m_ContThreadLock.Lock();

		jrs_u8 flags = 0;
		flags |= 1 << 5;						// Size not included in 
		if(pHeap->IsMemoryTrackingEnabled())
			flags |= 1 << 7;

		// Not available for NI heaps
// #ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
// 		flags |= 1 << 6;
// #endif

		jrs_u64 heapSize = pHeap->GetSize();
		jrs_u64 heapMemSize = pHeap->GetMemoryUsed();
		jrs_u64 heapMemCount = pHeap->GetNumberOfAllocations();

		jrs_bool bFileAppend = TRUE;
		jrs_i8 ContinuousOutputText[2048];

		// Find the page by getting the base alignment
		jrs_i8 *pPageAdd = (jrs_i8 *)((jrs_sizet)pHeaderAdd & ~(pHeap->GetPageSize() - 1));
		cHeapNonIntrusive::sSlab *pSlab = pHeap->FindSlabFromMemory(pPageAdd);
		jrs_sizet pageIndex = ((jrs_sizet)pPageAdd - (jrs_sizet)pSlab->pBase) / pHeap->GetPageSize();
		cHeapNonIntrusive::sPageBlock *pBlock = &pSlab->pBlocks[pageIndex];

		jrs_u64 blockSize = pBlock->numFreePages * pHeap->GetPageSize();
		if(pBlock->pageFlags & JRSMEMORYMANAGER_PAGESUBALLOC)
		{
			//offsetDebug = offset / pBlock->sizeOfSubAllocs;
			blockSize = pBlock->sizeOfSubAllocs;
		}
		
		const jrs_i8 *pText = "Unknown";
		jrs_sizet *puCallStack = NULL;
		jrs_u8 transferSize = 0;
#ifndef MEMORYMANAGER_MINIMAL
		if(pBlock->pDebugInfo)
		{
			pText = (jrs_i8 *)pBlock->pDebugInfo;
			puCallStack = (jrs_sizet *)(pText + MemoryManager_StringLength);
			transferSize = (jrs_u8)pHeap->m_uDebugHeaderSize + sizeof(jrs_u64) + sizeof(jrs_u64);
			MemoryWarning(transferSize < 255, JRSMEMORYERROR_UNKNOWNWARNING, "Transfer size is to big. Must be maximum of 256 bytes (32char string + size + flags + callstack depth of 52 (26 in 64bit)");
		}
		else
		{
			// The size and space for details later
			transferSize = sizeof(jrs_u64) + sizeof(jrs_u64);
		}
#endif
		// The 1 << 8 elmininates it from calculations in goldfish
		jrs_u64 heapDetails = (((MemoryManager_StringLength << 9) | (1 << 8) | pHeap->m_uNumCallStacks) << 16) | transferSize;

		sLVOperation op;
		if(eType == eContLog_Allocate)
		{
			op.type = (jrs_u8)eContLog_Allocate | flags;
			op.address = (jrs_sizet)pHeaderAdd;
			op.sizeofopdata = transferSize;
			op.alignment = uAlignment;
			op.idofheappool = pHeap->GetUniqueId();
			op.extraInfo0 = heapMemCount;
			op.extraInfo1 = heapMemSize;
			op.extraInfo2 = heapSize;
			op.extraInfo3 = heapDetails;

			// Log it out if needed
			if(m_bEnableContinuousDump)
			{
				sprintf(ContinuousOutputText, "Heap Alloc; %s; 0x%llx; 0x%llx; 0x%x; 0x%x; 0x%x; 0x%llx; 0x%llx; 0x%llx; 0x%llx"
					, pHeap->GetName()
					, (jrs_u64)(((jrs_i8 *)pHeaderAdd) + sizeof(sAllocatedBlock))
					, blockSize
					, uAlignment
					, 0, 0
					, heapMemCount
					, heapMemSize
					, heapSize
					, heapDetails);
				
				strcat(ContinuousOutputText, pText);	

				for(jrs_u32 cs = 0; cs < pHeap->m_uNumCallStacks; cs++)
				{
					jrs_i8 temp[32];
					sprintf(temp, "; 0x%llx", (jrs_u64)MemoryManagerPlatformAddressToBaseAddress(puCallStack ? puCallStack[cs] : 0));
					strcat(ContinuousOutputText, temp);
				}
				strcat(ContinuousOutputText, "\n");
			}
		}
		else if(eType == eContLog_Free)
		{
			op.type = (jrs_u8)eContLog_Free | flags;
			op.address = (jrs_sizet)pHeaderAdd;
			op.sizeofopdata = transferSize;
			op.alignment = (jrs_u32)uMisc;
			op.idofheappool = pHeap->GetUniqueId();
			op.extraInfo0 = heapMemCount;
			op.extraInfo1 = heapMemSize;
			op.extraInfo2 = heapSize;
			op.extraInfo3 = heapDetails;

			if(m_bEnableContinuousDump)
			{
				sprintf(ContinuousOutputText, "Heap Free; %s; 0x%llx; 0x%llx; 0x%x; 0x%x; 0x%llx; 0x%llx; 0x%llx; 0x%llx; 0x%llx"
					, pHeap->GetName()
					, (jrs_u64)(((jrs_i8 *)pHeaderAdd) + sizeof(sAllocatedBlock))
					, blockSize
					, uAlignment
					, 0, (jrs_u64)0
					, heapMemCount
					, heapMemSize
					, heapSize
					, heapDetails);

				strcat(ContinuousOutputText, pText);			
				for(jrs_u32 cs = 0; cs < pHeap->m_uNumCallStacks; cs++)
				{
					jrs_i8 temp[32];
					sprintf(temp, "; 0x%llx", (jrs_u64)MemoryManagerPlatformAddressToBaseAddress(puCallStack ? puCallStack[cs] : 0));
					strcat(ContinuousOutputText, temp);
				}
				strcat(ContinuousOutputText, "\n");
			}
		}
		else if(eType == eContLog_ReAlloc)
		{
			op.type = (jrs_u8)eContLog_ReAlloc | flags;
			op.address = (jrs_sizet)pHeaderAdd;
			op.sizeofopdata = transferSize;
			op.alignment = uAlignment;
			op.idofheappool = pHeap->GetUniqueId();
			op.extraInfo0 = op.extraInfo1 = op.extraInfo2 = op.extraInfo3 = 0;
		}
		else 
		{
			MemoryWarning(0, JRSMEMORYERROR_UNKNOWNWARNING, "Wrong type for ContinuousLogging_HeapOperation.");
		}

		// Write CSV file if need be
		if(m_bEnableContinuousDump && m_MemoryManagerFileOutput)
			m_MemoryManagerFileOutput(ContinuousOutputText, (int)strlen(ContinuousOutputText), m_ContinuousDumpFile, bFileAppend);

		// Add the data - We cant do this yet
		jrs_u8 dataToTransfer[256];
		if(pBlock->pDebugInfo)
		{
			jrs_u64 zeros = 0;
			memcpy(&dataToTransfer[0], &blockSize, sizeof(jrs_u64));
			memcpy(&dataToTransfer[8], &zeros, sizeof(jrs_u64));
			memcpy(&dataToTransfer[16], pBlock->pDebugInfo, transferSize);
			ContinuousLog_AddToBuffer(op, dataToTransfer);
		}
		else if(transferSize)
		{
			jrs_u64 zeros = 0;
			memcpy(&dataToTransfer[0], &blockSize, sizeof(jrs_u64));
			memcpy(&dataToTransfer[8], &zeros, sizeof(jrs_u64));
			ContinuousLog_AddToBuffer(op, dataToTransfer);
		}

		// Unlock the continuous writes
		m_ContThreadLock.Unlock();
#endif
	}

	//  Description:
	//		Internal only.  This function add data to the internal ring buffer of operations to be transmitted.  This specific function deals with 
	//		the heap operations.
	//  See Also:
	//		ContinuousLogging_Operation
	//  Arguments:
	//		eType - Type of Logging operation.
	//		pHeap - Heap pointer.
	//		pHeaderAdd - Header address of the operation.
	//		uAlignment - Alignment of the operation.
	//		uMisc - Misc information.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Reports any operations that occur on a specific heap.  These cover Allocations, Frees and Reallocs.  Reallocs may not always show.
	void cMemoryManager::ContinuousLogging_PoolOperation(eContLog eType, cPoolBase *pPool, void *pAddress, jrs_u64 uMisc)
	{
#ifndef MEMORYMANAGER_MINIMAL
		// Only if active
		if(!ContinuousLog_CanLog(pPool->GetHeap()))
			return;

		// Lock the thread writes
		m_ContThreadLock.Lock();

		jrs_bool bFileAppend = TRUE;
		jrs_i8 ContinuousOutputText[512];

		jrs_u8 flags = 0;
		flags |= pPool->HasNameAndCallstackTracing() ? 1 << 7 : 0;
		flags |= pPool->HasSentinels() ? 1 << 6 : 0;

		sLVOperation op;
		if(eType == eContLog_AllocatePool)
		{
			op.type = (jrs_u8)eContLog_AllocatePool | flags;
			op.address = (jrs_sizet)pAddress;
			op.sizeofopdata = 0;
			op.alignment = pPool->GetAllocationSize();
			op.idofheappool = (jrs_u16)pPool->GetUniqueId();
			op.extraInfo0 = op.extraInfo1 = op.extraInfo2 = op.extraInfo3 = 0;

			if(m_bEnableContinuousDump)
			{
				sprintf(ContinuousOutputText, "Pool Alloc; %s; %s; 0x%llx; 0x%x; 0x%x; %s; 0x%llx; 0x%llx; 0x%llx; 0x%llx; 0x%llx; 0x%llx; 0x%llx; 0x%llx\n",
					pPool->GetName(),
					pPool->GetHeap()->GetName(),
					op.address,
					pPool->GetAllocationSize(),
					16,
					"Name",
					(jrs_u64)0,
					(jrs_u64)1,
					(jrs_u64)2,
					(jrs_u64)3,
					(jrs_u64)4,
					(jrs_u64)5,
					(jrs_u64)6,
					(jrs_u64)7
					);
			}
		}
		else if(eType == eContLog_FreePool)
		{
			op.type = (jrs_u8)eContLog_FreePool | flags;
			op.address = (jrs_sizet)pAddress;
			op.sizeofopdata = 0;
			op.alignment = pPool->GetAllocationSize();
			op.idofheappool = (jrs_u16)pPool->GetUniqueId();
			op.extraInfo0 = op.extraInfo1 = op.extraInfo2 = op.extraInfo3 = 0;

			if(m_bEnableContinuousDump)
			{
				sprintf(ContinuousOutputText, "Pool Free; %s; %s; 0x%llx; 0x%x; 0x%x; %s; 0x%llx; 0x%llx; 0x%llx; 0x%llx; 0x%llx; 0x%llx; 0x%llx; 0x%llx\n",
					pPool->GetName(),
					pPool->GetHeap()->GetName(),
					op.address,
					pPool->GetAllocationSize(),
					16,
					"Name",
					(jrs_u64)0,
					(jrs_u64)1,
					(jrs_u64)2,
					(jrs_u64)3,
					(jrs_u64)4,
					(jrs_u64)5,
					(jrs_u64)6,
					(jrs_u64)7
					);
			}
		}
		else 
		{
			MemoryWarning(0, JRSMEMORYERROR_UNKNOWNWARNING, "Wrong type for ContinuousLogging_PoolOperation.");
		}

		// Add the data
		ContinuousLog_AddToBuffer(op, pAddress);

		// Add the data
		// Write CSV file if need be
		if(m_bEnableContinuousDump && m_MemoryManagerFileOutput)
			m_MemoryManagerFileOutput(ContinuousOutputText, (int)strlen(ContinuousOutputText), m_ContinuousDumpFile, bFileAppend);

		// Unlock the continuous writes
		m_ContThreadLock.Unlock();
#endif
	}

	//  Description:
	//		Internal only.  Reports if the heap can log or not.
	//  See Also:
	//		
	//  Arguments:
	//		pHeap - Heap to check.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Checks if the heap can log or not
	jrs_bool cMemoryManager::ContinuousLog_CanLog(cHeap *pHeap)
	{
#ifndef MEMORYMANAGER_MINIMAL
		// Only if active
		if(!pHeap->IsLoggingEnabled())
			return FALSE;

		// Only if continuous data grabbing is enabled or we want continuous output
		if(!m_bELVContinuousGrab && !m_bEnableContinuousDump)
			return FALSE;
#endif
		return TRUE;
	}

	//  Description:
	//		Internal only.  Reports if the NI heap can log or not.
	//  See Also:
	//		
	//  Arguments:
	//		pHeap - Heap to check.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Checks if the NI heap can log or not
	jrs_bool cMemoryManager::ContinuousLog_CanLog(cHeapNonIntrusive *pHeap)
	{
#ifndef MEMORYMANAGER_MINIMAL
		// Only if active
		if(!pHeap->IsLoggingEnabled())
			return FALSE;

		// Only if continuous data grabbing is enabled or we want continuous output
		if(!m_bELVContinuousGrab && !m_bEnableContinuousDump)
			return FALSE;
#endif
		return TRUE;
	}

	//  Description:
	//		Internal only.  This function add data to the internal ring buffer of operations to be transmitted.  This specific function deals with 
	//		the heap operations.
	//  See Also:
	//		
	//  Arguments:
	//		eType - Type of Logging operation.
	//		pHeap - Heap pointer.
	//		pPool - Pool pointer base.
	//		uMisc - Misc information.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Reports any operations that occur on a specific heap.  These cover Allocations, Frees and Reallocs.  Reallocs may not always show.
	void cMemoryManager::ContinuousLogging_Operation(eContLog eType, cHeap *pHeap, cPoolBase *pPool, jrs_u64 uMisc)
	{
#ifndef MEMORYMANAGER_MINIMAL
		// Only if active - we ignore the heap here and only care about if we should be logging or not
		if(!m_bELVContinuousGrab && !m_bEnableContinuousDump)
			return;

		// Lock the thread writes
		m_ContThreadLock.Lock();

		jrs_bool bFileAppend = TRUE;
		jrs_i8 ContinuousOutputText[512];

		sLVOperation op;
		op.type = (jrs_u8)eType;
		op.address = 0;
		op.sizeofopdata = 32;
		op.alignment = 0;
		op.extraInfo0 = op.extraInfo1 = op.extraInfo2 = op.extraInfo3 = 0;
		if(eType == eContLog_CreateHeap || eType == eContLog_DestroyHeap || eType == eContLog_ResizeHeap)
		{	
			struct LVOpHeapInformation
			{
				jrs_u64 uSize;				// 0
				jrs_u32 uDefaultAlignment;  // 8
				jrs_u32 uMinAllocSize;		// 12
				jrs_u32 uMaxAllocSize;		// 16
				jrs_u32 uPad1;				// 20
				jrs_u32 uPad2;				// 24
				jrs_u32 uFlags;				// 28
			};

			op.idofheappool = pHeap->GetUniqueId();
			op.address = (jrs_u64)(pHeap->GetAddress());

			LVOpHeapInformation info;
			info.uSize = pHeap->GetSize();
			info.uDefaultAlignment = pHeap->GetDefaultAlignment();
			info.uMinAllocSize = (jrs_u32)pHeap->GetMinAllocationSize();
			info.uMaxAllocSize = (jrs_u32)pHeap->GetMaxAllocationSize();
			info.uPad1 = info.uPad2 = 0;
			info.uFlags = 0;
#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
			info.uFlags |= 1 << 7;
#endif
#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
			info.uFlags |= 1 << 6;
#endif
			ContinuousLog_AddToBuffer(op, &info);

			if(m_bEnableContinuousDump)
			{
				const jrs_i8 *pOp = "Heap Create";
				if(op.type == eContLog_DestroyHeap)
				{
					pOp = "Heap Destroy";
				}
				else if(op.type == eContLog_ResizeHeap)
				{
					pOp = "Heap Resize";
				}

				sprintf(ContinuousOutputText, "%s; %s; 0x%llx; 0x%llx; 0x%x; 0x%x; 0x%x; 0x0; 0x0; 0x0; 0x0; 0x0; 0x0; 0x0\n",
					pOp,
					pHeap->GetName(),
					op.address,
					info.uSize,
					info.uDefaultAlignment,
					info.uMinAllocSize,
					info.uMaxAllocSize
					);
			}
		}
		else if(eType == eContLog_CreatePool || eType == eContLog_DestroyPool)
		{
			op.idofheappool = 0;
			struct LVOpPoolInformation
			{
				jrs_u64 uSize;				// 8
				jrs_u32 uDefaultAlignment;  // 12
				jrs_u32 uAllocSize;			// 16
				jrs_u32 uPad1;				// 20
				jrs_u32 uPad2;				// 24
				jrs_u32 uFlags;				// 28
			};

			op.idofheappool = (jrs_u16)pPool->GetUniqueId();
			op.address = (jrs_u64)pPool->GetAddress();

			LVOpPoolInformation info;
			info.uSize = pPool->GetSize();
			info.uDefaultAlignment = 0;
			info.uAllocSize = pPool->GetAllocationSize();
			info.uFlags = 0;
			info.uFlags |= pPool->HasNameAndCallstackTracing() ? 1 << 7 : 0;
			info.uFlags |= pPool->HasSentinels() ? 1 << 6 : 0;
			ContinuousLog_AddToBuffer(op, &info);

			if(m_bEnableContinuousDump)
			{
				const jrs_i8 *pOp = "Pool Create";
				if(op.type == eContLog_DestroyPool)
				{
					pOp = "Pool Destroy";
				}

				sprintf(ContinuousOutputText, "%s; %s$$%s; 0x%llx; 0x%llx; 0x%x; 0x0; 0x0; 0x0; 0x0; 0x0; 0x0; 0x0; 0x0; 0x0\n",
					pOp,
					pPool->GetName(), pPool->GetHeap()->GetName(),
					op.address,
					info.uSize,
					info.uAllocSize
					);
			}
		}
		else if(eType == eContLog_Marker)
		{
			jrs_i8 text[32];
			op.idofheappool = eContLog_Marker;
			jrs_u32 size = (jrs_u32)strlen((jrs_i8 *)pHeap);
			size = (size >= 31) ? 30 : size;
			memcpy(text, pHeap, size);
			text[size] = '\0';
			ContinuousLog_AddToBuffer(op, text);

			if(m_bEnableContinuousDump)
			{
				sprintf(ContinuousOutputText, "Marker; %s; 0x0; 0x0; 0x0; 0; 0; 0; 0; 0; 0; 0; 0; 0\n", text);
			}
		}
		else if(eType == eContLog_StartLogging || eType == eContLog_StopLogging)
		{
			op.idofheappool = 0;
			ContinuousLog_AddToBuffer(op, NULL);

			if(m_bEnableContinuousDump)
			{
				if(eType == eContLog_StartLogging)
				{
					bFileAppend = false;
					sprintf(ContinuousOutputText, "Logging Started; %d; 0x%llx; %d; %d; ; ; ; ; ; ; ; ; 0\n", 
#ifdef JRS64BIT
						1,
#else
						0,
#endif
						g_uBaseAddressOffsetCalculation, 
#ifdef JRSMEMORYMICROSOFTPLATFORMS
						0,
#else
						1,
#endif
						this->m_bResizeable ? 1 : 0
						);
				}
				else
				{
					m_ContThreadLock.Unlock();
					return;
				}
			}
		}
		else 
		{
			MemoryWarning(0, JRSMEMORYERROR_UNKNOWNWARNING, "Wrong type for ContinuousLogging_Operation.");
		}

		// Add the data
		// Write CSV file if need be
		if(m_bEnableContinuousDump && m_MemoryManagerFileOutput)
			m_MemoryManagerFileOutput(ContinuousOutputText, (int)strlen(ContinuousOutputText), m_ContinuousDumpFile, bFileAppend);

		// Unlock the continuous writes
		m_ContThreadLock.Unlock();
#endif
	}

	//  Description:
	//		Internal only.  This function add data to the internal ring buffer of operations to be transmitted.  This specific function deals with 
	//		the heap operations.
	//  See Also:
	//		
	//  Arguments:
	//		eType - Type of Logging operation.
	//		pHeap - Heap pointer.
	//		pPool - Pool pointer base.
	//		uMisc - Misc information.
	//  Return Value:
	//      Nothing
	//  Summary:	
	//		Reports any operations that occur on a specific heap.  These cover Allocations, Frees and Reallocs.  Reallocs may not always show.
	void cMemoryManager::ContinuousLogging_NIOperation(eContLog eType, cHeapNonIntrusive *pHeap, cPoolBase *pPool, jrs_u64 uMisc)
	{
#ifndef MEMORYMANAGER_MINIMAL
		// Only if active - we ignore the heap here and only care about if we should be logging or not
		if(!m_bELVContinuousGrab && !m_bEnableContinuousDump)
			return;

		// Lock the thread writes
		m_ContThreadLock.Lock();

		jrs_bool bFileAppend = TRUE;
		jrs_i8 ContinuousOutputText[512];

		sLVOperation op;
		op.type = (jrs_u8)eType;
		op.address = 0;
		op.sizeofopdata = 32;
		op.alignment = 0;
		op.extraInfo0 = op.extraInfo1 = op.extraInfo2 = op.extraInfo3 = 0;
		if(eType == eContLog_CreateHeap || eType == eContLog_DestroyHeap || eType == eContLog_ResizeHeap)
		{	
			struct LVOpHeapInformation
			{
				jrs_u64 uSize;				// 0
				jrs_u32 uDefaultAlignment;  // 8
				jrs_u32 uMinAllocSize;		// 12
				jrs_u32 uMaxAllocSize;		// 16
				jrs_u32 uPad1;				// 20
				jrs_u32 uPad2;				// 24
				jrs_u32 uFlags;				// 28
			};

			op.idofheappool = pHeap->GetUniqueId();
			op.address = (jrs_u64)(pHeap->GetAddress());

			LVOpHeapInformation info;
			info.uSize = pHeap->GetSize();
			info.uDefaultAlignment = (jrs_u32)pHeap->GetDefaultAlignment();
			info.uMinAllocSize = (jrs_u32)pHeap->GetMinAllocationSize();
			info.uMaxAllocSize = (jrs_u32)pHeap->GetMaxAllocationSize();
			info.uPad1 = info.uPad2 = 0;
			info.uFlags = 0;
			
			if(pHeap->IsMemoryTrackingEnabled())
				info.uFlags |= 1 << 7;

// #ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
// 			info.uFlags |= 1 << 6;
// #endif
			ContinuousLog_AddToBuffer(op, &info);

			if(m_bEnableContinuousDump)
			{
				const jrs_i8 *pOp = "Heap Create";
				if(op.type == eContLog_DestroyHeap)
				{
					pOp = "Heap Destroy";
				}
				else if(op.type == eContLog_ResizeHeap)
				{
					pOp = "Heap Resize";
				}

				sprintf(ContinuousOutputText, "%s; %s; 0x%llx; 0x%llx; 0x%x; 0x%x; 0x%x; 0x0; 0x0; 0x0; 0x0; 0x0; 0x0; 0x0\n",
					pOp,
					pHeap->GetName(),
					op.address,
					info.uSize,
					info.uDefaultAlignment,
					info.uMinAllocSize,
					info.uMaxAllocSize
					);
			}
		}
		else 
		{
			MemoryWarning(0, JRSMEMORYERROR_UNKNOWNWARNING, "Wrong type for ContinuousLogging_Operation.");
		}

		// Add the data
		// Write CSV file if need be
		if(m_bEnableContinuousDump && m_MemoryManagerFileOutput)
			m_MemoryManagerFileOutput(ContinuousOutputText, (int)strlen(ContinuousOutputText), m_ContinuousDumpFile, bFileAppend);

		// Unlock the continuous writes
		m_ContThreadLock.Unlock();
#endif
	}
}	// Elephant
