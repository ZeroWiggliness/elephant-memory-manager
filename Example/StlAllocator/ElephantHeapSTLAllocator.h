/* 
(C) Copyright 2007-2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 

This is a simple example showing how to use Elephant as a custom allocator for the STL containers. The STL containers
can be memory intensive so we show you two examples.  One that allows an Elephant Heap and another that shows you
how to use an Elephant pool with an std::list.  
*/

#include <JRSMemory.h>

template <typename T> 
class ElephantHeapSTLAllocatorBase
{
public:

	// Heap to take the allocations from
	cHeap *m_pHeap;

	// type definitions
	typedef T        value_type;
	typedef T*       pointer;
	typedef const T* const_pointer;
	typedef T&       reference;
	typedef const T& const_reference;
	typedef std::size_t    size_type;
	typedef std::ptrdiff_t difference_type;

	template<typename U> 
	struct rebind
	{
		// rebinding will create a new tracked allocator which sustains the memory pool of the original tracked allocator
		typedef ElephantHeapSTLAllocatorBase<U> other;
	};

	ElephantHeapSTLAllocatorBase()
	{

	}

	template <class U>
	ElephantHeapSTLAllocatorBase(const ElephantHeapSTLAllocatorBase<U>& rBase) 
	{
		m_pHeap = rBase.m_pHeap;
	}

	~ElephantHeapSTLAllocatorBase()
	{

	}

	pointer address(reference ref) const
	{
		return &ref;
	}

	const_pointer address(const_reference ref) const
	{
		return &ref;
	}

	size_type max_size() const
	{
		// This isn't entirely correct.  Its the MAXIMUM amount the heap could hold but overheads etc will soon take that up depending
		// on the stl container used.
		return m_pHeap->GetSize() / sizeof(T);
	}

	pointer allocate(size_type numelements) const
	{
		return static_cast<pointer>(m_pHeap->AllocateMemory(numelements * sizeof(T), 0, 0, "STLAlloc"));
	}

	void deallocate(pointer ptr, size_type numelements) const
	{
		m_pHeap->FreeMemory(ptr, 0, "STLFree");
	}

	void construct(pointer ptr, const_reference value) const
	{
		::new(static_cast<void*>(ptr)) T(value);
	}

	void destroy(pointer ptr) const
	{
		(ptr)->~T();
	}

};

// return that all specializations of this allocator are interchangeable
template <class T1, class T2>
bool operator== (const ElephantHeapSTLAllocatorBase<T1>&, const ElephantHeapSTLAllocatorBase<T2>&) throw() 
{
	return true;
}

template <class T1, class T2>
bool operator!= (const ElephantHeapSTLAllocatorBase<T1>&, const ElephantHeapSTLAllocatorBase<T2>&) throw() 
{
	return false;
}

template <typename T, int Heap>
class ElephantHeapSTLAllocator : public ElephantHeapSTLAllocatorBase<T>
{
public:
	ElephantHeapSTLAllocator()
	{
		m_pHeap = cMemoryManager::Get().GetHeap(Heap);
	}

	~ElephantHeapSTLAllocator()
	{

	}
};