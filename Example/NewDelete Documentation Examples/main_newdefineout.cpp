/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent.

Example demonstrates new/delete defined under different names as mentioned in the documentation.

NOTE: This example will warn on exit about the heap still having allocations.
*/

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <windows.h>

#include "JRSMemory.h"

// Basic memory manager overloading
void MemoryManagerTTYPrint(const jrs_i8 *pText)
{
	OutputDebugString(pText);
	OutputDebugString("\n");
}

void MemoryManagerErrorHandle(const jrs_i8 *pError, jrs_u32 uErrorID)
{
	__debugbreak();
}

void MemoryManagerWriteToFile(void *pData, int size, const char *pFilePathAndName, jrs_bool bAppend)
{
	FILE *fp;

	fp = fopen(pFilePathAndName, (bAppend) ? "ab" : "wb");
	if(fp)
	{
		fwrite(pData, size, 1, fp);
		fclose(fp);
	}
}

// New and delete #define method
void *_cdecl operator new(size_t cbSize)
{
	void *p = cMemoryManager::Get().Malloc(cbSize, 16, JRSMEMORYFLAG_NEW, "New");  
	return p;
}

void _cdecl operator delete(void *pMemory) 
{
	 cMemoryManager::Get().Free(pMemory, JRSMEMORYFLAG_NEW);
}

void *_cdecl operator new[](size_t cbSize)
{
	void *p = cMemoryManager::Get().Malloc(cbSize, 16, JRSMEMORYFLAG_NEWARRAY, "New");  
	return p;
}

void _cdecl operator delete[](void *pMemory) 
{
	cMemoryManager::Get().Free(pMemory, JRSMEMORYFLAG_NEWARRAY);
}

// For debugging
void *_cdecl operator new(size_t cbSize, char *pText)
{
	void *p = cMemoryManager::Get().Malloc(cbSize, 16, JRSMEMORYFLAG_NEW, pText);  
	return p;
}

void _cdecl operator delete(void *pMemory, char *pText) 
{
	cMemoryManager::Get().Free(pMemory, JRSMEMORYFLAG_NEW, pText);
}

void *_cdecl operator new[](size_t cbSize, char *pText)
{
	void *p = cMemoryManager::Get().Malloc(cbSize, 16, JRSMEMORYFLAG_NEWARRAY, pText);  
	return p;
}

void _cdecl operator delete[](void *pMemory, char *pText) 
{
	cMemoryManager::Get().Free(pMemory, JRSMEMORYFLAG_NEWARRAY, pText);
}

// placement new and delete
void *_cdecl operator new(size_t cbSize, void* pPlacement)
{
	return pPlacement;
}

void *_cdecl operator new[](size_t cbSize, void* pPlacement)
{
	return pPlacement;
}

void *_cdecl operator new(size_t cbSize, void* pPlacement, char *pText)
{
	return pPlacement;
}

void *_cdecl operator new[](size_t cbSize, void* pPlacement, char *pText)
{
	return pPlacement;
}

void operator delete(void* p, void*)
{
	// Here to stop the compiler moaning.  Placement delete does nothing.
}

void operator delete[](void* p, void*) 
{
	// Here to stop the compiler moaning.  Placement delete does nothing.
}

#ifdef _DEBUG
#define jrs_new new(__FILE__)
#define jrs_delete delete
#else
#define jrs_new new
#define jrs_delete delete
#endif
#define jrs_newplace(placement) new(placement)


class cFoo
{
	int x;
	float y;
	void *pData;
	int p;
public:

	cFoo()
	{
		x = 0;
		y = 1.0f;
		pData = cMemoryManager::Get().Malloc(55);
		p = 2;
	}

	~cFoo()
	{
		cMemoryManager::Get().Free(pData);
	}
};

void main(void)
{
	// Basic Memory manager initialization
	cMemoryManager::InitializeCallbacks(MemoryManagerTTYPrint, MemoryManagerErrorHandle, MemoryManagerWriteToFile);
	cMemoryManager::Get().Initialize(512 * 1024 * 1024);

	int arraymemfootest[16];
	
	// new overload testing
	int *newmemtest = jrs_new int;
	int *newmemtestarray = jrs_new int[50];
	cFoo *pFoo = jrs_new cFoo;
	cFoo *pPlacement;
	cFoo *pFooArray = jrs_new cFoo[4];

	// THIS WILL WORK WITH OVERLOADS (and it will leak since we dont destroy it)
	pPlacement = jrs_newplace(arraymemfootest) cFoo;

	jrs_delete newmemtest;
	jrs_delete []newmemtestarray;
	jrs_delete pFoo;
	jrs_delete []pFooArray;

	cMemoryManager::Get().Destroy();
}