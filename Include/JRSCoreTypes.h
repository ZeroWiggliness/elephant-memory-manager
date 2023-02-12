/* 
(C) Copyright 2010-2011 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/

#ifndef _JRSCORETYPES_H
#define _JRSCORETYPES_H

// Define Sockets - sockets are not available for all platforms.  Specific platforms will disable them.
#define JRSMEMORY_HASSOCKETS

// Microsoft
#ifdef _MSC_VER
#if defined(_WIN32) || defined(_WIN64)
#ifdef _XBOX
#define JRSMEMORYXBOX360PLATFORM
#else
#define JRSMEMORYPCPLATFORM
#endif
#define JRSMEMORYMICROSOFTPLATFORMS
#define JRSMEMORYALIGNPRE(value) __declspec(align(value))
#define JRSMEMORYALIGNPOST(value)
#define JRSMEMORYLOCALALIGN(var, value) __declspec(align(value)) var

#ifndef JRSMEMORYLIB
#define JRSMEMORYDLL
#define JRSMEMORYDLLEXPORT _declspec(dllexport)
#else
#define JRSMEMORYDLLEXPORT
#endif

#ifdef JRSMEMORYDLL
#define JRSMEMORYGLOBALDECLARATION
#endif

#endif
// GCC/SN
#elif defined(__GNUC__)
#define JRSMEMORYDLLEXPORT
#ifdef __llvm__
#define JRSMEMORYLLVMPLATFORMS
#endif
#define JRSMEMORYGCCPLATFORMS
#if defined(__CELLOS_LV2__)
#ifdef __SNC__
#define JRSMEMORYCTZNOTAVAILABLE
#endif
#define JRSMEMORYSONYPS3PLATFORM
#elif defined(__APPLE__)
#define JRSMEMORYAPPLE
#elif defined(__ANDROID__)
#define JRSANDROIDPLATFORM
#else
#define JRSUNIXPLATFORM
#endif

#define JRSMEMORYALIGNPRE(value)
#define JRSMEMORYALIGNPOST(value) __attribute__ ((aligned (value)))
#define JRSMEMORYLOCALALIGN(var, value) var __attribute__ ((aligned (value)))

#elif defined(__CWCC__)
#define JRSMEMORYDLLEXPORT
#define JRSMEMORYMWERKSPLATFORMS

#define JRSMEMORYALIGNPRE(value)
#define JRSMEMORYALIGNPOST(value) __attribute__ ((aligned (value)))
#define JRSMEMORYLOCALALIGN(var, value) var __attribute__ ((aligned (value)))

#if defined(RVL) 
// Nintendo Wii
#define JRSMEMORYNINTENDOWII
#undef JRSMEMORY_HASSOCKETS			// Not available
#endif

#elif defined(__CC_ARM)
#define JRSMEMORYDLLEXPORT
#define JRSMEMORYARMCCPLATFORMS

#define JRSMEMORYALIGNPRE(value)
#define JRSMEMORYALIGNPOST(value) 
#define JRSMEMORYLOCALALIGN(var, value) var

#if defined(NINTENDO3DS)
#define JRSMEMORYNINTENDO3DS
#else 
#define JRSMEMORYARMGENERIC
#undef JRSMEMORY_HASSOCKETS			// Not available
#endif

// Other platforms
#else
#error
#define JRSMEMORYALIGN(value)
#define JRSMEMORYDLLEXPORT

#define JRSMEMORYALIGNPRE(value)
#define JRSMEMORYALIGNPOST(value)
#endif

#ifdef JRSMEMORYPCPLATFORM
#include <windows.h>
#endif


// Endianness
#if defined(JRSMEMORYXBOX360PLATFORM) || defined(JRSMEMORYSONYPS3PLATFORM)
#define JRSMEMORYBIGENDIAN
#endif

// Standard pthread implementation
#if defined(JRSMEMORYAPPLE) || defined(JRSANDROIDPLATFORM) || defined(JRSUNIXPLATFORM)
#define JRSMEMORY_HASPTHREADS
#endif

// Types
#ifndef JRSTYPES
typedef char jrs_bool;
typedef char jrs_i8;
typedef unsigned char jrs_u8;
typedef signed short jrs_i16;
typedef unsigned short jrs_u16;
typedef unsigned int jrs_u32;
typedef signed int jrs_i32;
typedef float jrs_f32;

#if defined(_M_IA64) || defined(__IA64__) ||  defined(_M_X64) || defined(__amd64__)
#ifdef _MSC_VER
typedef __int64 jrs_i64;
typedef unsigned __int64 jrs_u64;
#else
typedef signed long long jrs_i64;
typedef unsigned long long jrs_u64;
#endif
#define JRS64BIT
#else
typedef signed long long jrs_i64;
typedef unsigned long long jrs_u64;
#endif

#ifdef JRS64BIT
typedef jrs_u64 jrs_sizet;
#else
typedef jrs_u32 jrs_sizet;
#endif

typedef jrs_i64 jrs_socket;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL 0
#endif

#define JRSTYPES
#endif


template<typename T>
T JRSEndianSwap(const T &rSwap)
{
	if(sizeof(T) == 2)
	{
		return ((rSwap >> 8) | (rSwap << 8));
	}
	else if(sizeof(T) == 4)
	{
		return (((rSwap & 0xff000000) >> 24) | 
				((rSwap & 0x00ff0000) >> 8) | 
				((rSwap & 0x0000ff00) << 8) | 
				((rSwap & 0x000000ff) << 24));
	}
	else if(sizeof(T) == 8)
	{
		return (((rSwap & 0xff00000000000000ULL) >> 56) | 
				((rSwap & 0x00ff000000000000ULL) >> 40) |  
				((rSwap & 0x0000ff0000000000ULL) >> 24) | 
				((rSwap & 0x000000ff00000000ULL) >> 8) | 
				((rSwap & 0x00000000ff000000ULL) << 8) | 
				((rSwap & 0x0000000000ff0000ULL) << 24) | 
				((rSwap & 0x000000000000ff00ULL) << 40) | 
				((rSwap & 0x00000000000000ffULL) << 56));
	}
	else
		return rSwap;
}

template<typename T>
T JRSEndianSwapToLittleEndian(const T &rSwap)
{
#ifdef JRSMEMORYBIGENDIAN
	return JRSEndianSwap(rSwap);
#else
	return rSwap;
#endif
}

template<typename T>
T JRSEndianSwapToBigEndian(const T &rSwap)
{
#ifdef JRSMEMORYBIGENDIAN
	return rSwap;
#else
	return JRSEndianSwap(rSwap);
#endif
}

inline jrs_u32 JRSCountTrailingZero(jrs_u32 val)
{
#if defined(JRSMEMORYPCPLATFORM)
	DWORD outv; 
	jrs_u32 topSetBit;

	// Puts it into the 31st bit if its way to big
	char ret = _BitScanForward(&outv, val);
	topSetBit = ret != 0 ? (jrs_u32)outv : 32;
	return topSetBit;
#elif defined(JRSMEMORYGCCPLATFORMS) && !defined(JRSMEMORYCTZNOTAVAILABLE)
	if(!val)
		return 32;
	return __builtin_ctz(val);
#else
	// Naive approach.
	if(val)
	{
		jrs_u32 uZeroCount = 0;
		val = (val ^ (val - 1)) >> 1;
		for(; val; uZeroCount++)
			val >>= 1;
		return uZeroCount;
	}
	else
		return 32;
#endif
}

inline jrs_u32 JRSCountLeadingZero(jrs_u32 val)
{
#if defined(JRSMEMORYPCPLATFORM)
	// val == 1 = 0
	// val == 2 = 1
	// val == 3 = 1
	// val == 4 = 2
	if(!val)
		return 0;

	DWORD outv; 
	jrs_u32 topSetBit;

	// Puts it into the 31st bit if its way to big
	char ret = _BitScanReverse(&outv, val);
	topSetBit = (jrs_u32)outv;
	return topSetBit;
#elif defined(JRSMEMORYGCCPLATFORMS) && !defined(JRSMEMORYCTZNOTAVAILABLE)
	if(!val)
		return 0;
	return 31 - __builtin_clz(val);
#else
	if(!val)
		return 0;

	// From the awesome hackers delight (not quite so naive approach).
	jrs_u32 uZeroCount = 0;
	if(val <= 0x0000FFFF) {uZeroCount = uZeroCount + 16; val = val <<16;}
	if(val <= 0x00FFFFFF) {uZeroCount = uZeroCount + 8; val = val << 8;}
	if(val <= 0x0FFFFFFF) {uZeroCount = uZeroCount + 4; val = val << 4;}
	if(val <= 0x3FFFFFFF) {uZeroCount = uZeroCount + 2; val = val << 2;}
	if(val <= 0x7FFFFFFF) {uZeroCount = uZeroCount + 1;}
	
	return 31 - uZeroCount;
#endif
}


#endif	// _JRSCORETYPES_H
