/* 
(C) Copyright 2007-2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 

This header file contains some basic new overloads which call Elephant Memory Manager.  It is in no way a complete replacement but will demonstrate
its use behind the scenes.
*/

#define malloc cMemoryManager::Get().Malloc
#define free cMemoryManager::Get().Free

// New and delete #define method
inline void *_cdecl operator new(size_t cbSize)
{
	void *p = cMemoryManager::Get().Malloc(cbSize, 16, JRSMEMORYFLAG_NEW, "New");  
	return p;
}

inline void _cdecl operator delete(void *pMemory) 
{
	cMemoryManager::Get().Free(pMemory, JRSMEMORYFLAG_NEW);
}

inline void *_cdecl operator new[](size_t cbSize)
{
	void *p = cMemoryManager::Get().Malloc(cbSize, 16, JRSMEMORYFLAG_NEWARRAY, "New");  
	return p;
}

inline void _cdecl operator delete[](void *pMemory) 
{
	cMemoryManager::Get().Free(pMemory, JRSMEMORYFLAG_NEWARRAY);
}

// For debugging
inline void *_cdecl operator new(size_t cbSize, char *pText)
{
	void *p = cMemoryManager::Get().Malloc(cbSize, 16, JRSMEMORYFLAG_NEW, pText);  
	return p;
}

inline void _cdecl operator delete(void *pMemory, char *pText) 
{
	cMemoryManager::Get().Free(pMemory, JRSMEMORYFLAG_NEW, pText);
}

inline void *_cdecl operator new[](size_t cbSize, char *pText)
{
	void *p = cMemoryManager::Get().Malloc(cbSize, 16, JRSMEMORYFLAG_NEWARRAY, pText);  
	return p;
}

inline void _cdecl operator delete[](void *pMemory, char *pText) 
{
	cMemoryManager::Get().Free(pMemory, JRSMEMORYFLAG_NEWARRAY, pText);
}

// heap overloaded new
inline void *_cdecl operator new(size_t cbSize, cHeap *pHeap)
{
	void *p = pHeap->AllocateMemory(cbSize, 16, JRSMEMORYFLAG_NEW, "Heap New");  
	return p;
}

inline void _cdecl operator delete(void *pMemory, cHeap *pHeap) 
{
	cMemoryManager::Get().Free(pMemory, JRSMEMORYFLAG_NEW, "Heap New");
}

inline void *_cdecl operator new[](size_t cbSize, cHeap *pHeap)
{
	void *p = cMemoryManager::Get().Malloc(cbSize, 16, JRSMEMORYFLAG_NEWARRAY, "Heap New[]");  
	return p;
}

inline void _cdecl operator delete[](void *pMemory, cHeap *pHeap) 
{
	cMemoryManager::Get().Free(pMemory, JRSMEMORYFLAG_NEWARRAY, "Heap New[]");
}

// placement new and delete
inline void *_cdecl operator new(size_t cbSize, void* pPlacement)
{
	return pPlacement;
}

inline void *_cdecl operator new[](size_t cbSize, void* pPlacement)
{
	return pPlacement;
}

inline void *_cdecl operator new(size_t cbSize, void* pPlacement, char *pText)
{
	return pPlacement;
}

inline void *_cdecl operator new[](size_t cbSize, void* pPlacement, char *pText)
{
	return pPlacement;
}

inline void operator delete(void* p, void*)
{
	// Here to stop the compiler moaning.  Placement delete does nothing.
}

inline void operator delete[](void* p, void*) 
{
	// Here to stop the compiler moaning.  Placement delete does nothing.
}