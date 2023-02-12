/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/
#include <JRSMemory_ThreadLocks.h>

JRSMemory_ThreadLock::JRSMemory_ThreadLock()
{
	/* Pthread implementation
	pthread_mutexattr_t   mta;

	pthread_mutexattr_init(&mta);	
	pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&m_Mutex, &mta);
	*/
}

JRSMemory_ThreadLock::~JRSMemory_ThreadLock()
{
	/* Pthread implementation
	pthread_mutex_destroy(&m_Mutex);
	*/
}

void JRSMemory_ThreadLock::Lock()
{
	/* Pthread implementation
	pthread_mutex_lock(&m_Mutex);
	*/
}

void JRSMemory_ThreadLock::Unlock()
{
	/* Pthread implementation
	pthread_mutex_unlock(&m_Mutex);
	*/
}