/* 
(C) Copyright 2007-2011 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 

This is a simple example showing 'Resizable Memory Mode'.  This is the same method you would know from standard allocators.  You do not need to worry about
how much memory you are consuming.  Elephant will resize the heaps as you consume memory and release it back when you destroy the heap.
*/

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <windows.h>

#include "JRSMemory.h"
#include "MMgr.h"

// Basic memory manager overloading
void MemoryManagerTTYPrint(const jrs_i8 *pText)
{
	printf("%s\n", pText);			// Output to console window for example
	OutputDebugString(pText);		// Output to TTY.
	OutputDebugString("\n");
}

void MemoryManagerErrorHandle(const jrs_i8 *pError, jrs_u32 uErrorID)
{
	__debugbreak();
}

void CustomElasticHeapSettings(cHeap::sHeapDetails &rDetails)
{
	// As an example we allow destruction with allocations.
	rDetails.bAllowDestructionWithAllocations = true;
}

cHeap *pManaged1;

cJRSThread::jrs_threadout OtherThread(cJRSThread::jrs_threadin pArg)
{
	while (true) {
		int sizes[] = { 1 << 10, 1 << 16, 1 << 20 };
		void * mem[10 * 3] = { 0 };
		for (int j = 0; j < 3; ++j) {
			for (int i = 0; i < 10; ++i) {
				mem[j * 10 + i] = pManaged1->AllocateMemory(sizes[j], 16);
			}
		}

		for (int j = 0; j < 3; ++j) {
			for (int i = 0; i < 10; ++i) {
				pManaged1->FreeMemory(mem[j * 10 + i]);
			}
		}
		Sleep(5);
	}
}


void main(void)
{
#ifdef ELEPHANT_TRIAL
	cMemoryManager::InitializeTrial();
#endif

	// Basic Memory manager initialization
	cMemoryManager::InitializeCallbacks(MemoryManagerTTYPrint, MemoryManagerErrorHandle, NULL);

	// Initialize live view
	//cMemoryManager::InitializeLiveView();
	
	// Elephant needs a bit of memory to hold some of its structure information.  This depends highly on what features are being used.
	// Normally this isn't a problem.  For Elastic heaps it is good to set this to reasonable amount while developing. Setting the input
	// to 0 will give an error warning you how much Elephant needs.
	cMemoryManager::Get().Initialize(JRSMEMORYINITFLAG_LARGEST, 0, FALSE);

	// Create some heaps.  Allow the 2nd to be customized with NULL free warnings disabled.
	cHeap::sHeapDetails managed2details;
	//managed2details. = true;

	pManaged1 = cMemoryManager::Get().CreateHeap(32 * 1024 * 1024, "Managed1", &managed2details);


	cJRSThread thread;
	
	//thread.Create(OtherThread, 0, cJRSThread::eJRSPriority_Low, 4, 32 * 1024);
	//thread.Start();
	Sleep(5);

	while (true)
	{
		void * mem[10] = { 0 };
		for (int i = 0; i < 10; ++i) {
			mem[i] = pManaged1->AllocateMemory(85 * 1024 * 1024, 16);
		}

		void * foo = pManaged1->AllocateMemory(10 * 1024, 16);

		for (int i = 0; i < 10; ++i) {
			pManaged1->FreeMemory(mem[i]);
		}

		//CSpinLockPtr& lock = elephant.Lock();
		//	lock->Lock();
		static int t = 0;
		if (t == 14)
		{
			pManaged1->ReportAll(0, true);
			pManaged1->Reclaim();
		}
		else 
			pManaged1->Reclaim();

		if (t == 14)
		{
			pManaged1->ReportAll(0, true);
		//	pManaged1->ReportAll();
			MemoryManagerTTYPrint("Reclaim");
		}
		t++;
		//	lock->Unlock();

		void * bar = pManaged1->AllocateMemory(10 * 1024, 16);
		pManaged1->FreeMemory(foo);
		pManaged1->FreeMemory(bar);
	}

	//	AllocatorFreeAligned_(elephant, foo);
	//	AllocatorFreeAligned_(elephant, bar);

//	static CElephantAllocator elephant;
//	static nflag bonce = true;

//	vpointer allocatormem = &elephant;
//	vpointer lockmem = elephant.Lock().Pointer();

//	static SElephantHeapSettings texsettings;
//	texsettings.allowresize = true;
//	texsettings.allowoom = true;

//	if (bonce) {
//		CreateElephantAllocator(allocatormem, 2 MB_, eSystemHeap, "TestHeap", lockmem, texsettings);
//		static CTestThread thread(elephant);
//		thread.Fork();
//		bonce = false;
//	}

	// --
//	vpointer mem[10] = { 0 };
//	for (unint i = 0; i < 10; ++i) {
//		mem[i] = AllocatorAllocAligned_(elephant, 16, 2 MB_);
//	}

//	vpointer foo = AllocatorAllocAligned_(elephant, 16, 10 KB_);
//
//	for (unint i = 0; i < 10; ++i) {
//		AllocatorFreeAligned_(elephant, mem[i]);
//	}
//	CSpinLockPtr& lock = elephant.Lock();
//	lock->Lock();
//	elephant.Heap()->Reclaim();
//	lock->Unlock();

//	vpointer bar = AllocatorAllocAligned_(elephant, 16, 10 KB_);

//	AllocatorFreeAligned_(elephant, foo);
//	AllocatorFreeAligned_(elephant, bar);





	// Note you do not need to call this.  The destructor will call this also but if you want to decide when all the 
	// memory should be freed this will perform that task.
	cMemoryManager::Get().Destroy();
}
/*
 *
     virtual void Main() override {
        while(true) {
            unint sizes[] = { 1 << 10, 1 << 16, 1 << 20 };
            vpointer mem[10 * 3] = { 0 };
            for(int j = 0; j < 3; ++j) {
                for(unint i = 0; i < 10; ++i) {
                    mem[j * 10 + i] = AllocatorAllocAligned_(elephant, 16, sizes[j]);
                }
            }

            for(int j = 0; j < 3; ++j) {
                for(unint i = 0; i < 10; ++i) {
                    AllocatorFreeAligned_(elephant, mem[j * 10 + i]);
                }
            }
            Sleep(5);
        }
    }

//////////////////// main thread

    while(true) {
        static CElephantAllocator elephant;
        static nflag bonce = true;

        vpointer allocatormem = &elephant;
        vpointer lockmem = elephant.Lock().Pointer();

        static SElephantHeapSettings texsettings;
        texsettings.allowresize = true;
        texsettings.allowoom = true;

        if(bonce) {
            CreateElephantAllocator(allocatormem, 2 MB_, eSystemHeap, "TestHeap", lockmem, texsettings);
            static CTestThread thread(elephant);
            thread.Fork();
            bonce = false;
        }

        // --
        vpointer mem[10] = { 0 };
        for(unint i = 0; i < 10; ++i) {
            mem[i] = AllocatorAllocAligned_(elephant, 16, 2 MB_);
        }

        vpointer foo = AllocatorAllocAligned_(elephant, 16, 10 KB_);

        for(unint i = 0; i < 10; ++i) {
            AllocatorFreeAligned_(elephant, mem[i]);
        }
        CSpinLockPtr& lock = elephant.Lock();
        lock->Lock();
        elephant.Heap()->Reclaim();
        lock->Unlock();

        vpointer bar = AllocatorAllocAligned_(elephant, 16, 10 KB_);

        AllocatorFreeAligned_(elephant, foo);
        AllocatorFreeAligned_(elephant, bar);

        break;
    }

 *
 */