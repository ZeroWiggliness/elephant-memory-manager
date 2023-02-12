/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/

// Needed for string functions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <JRSMemory.h>
#include <JRSMemory_Pools.h>
#include <JRSMemory_Thread.h>

#include "JRSMemory_Timer.h"

// Has sockets?
#ifdef JRSMEMORY_HASSOCKETS

namespace Elephant
{
	// Socket wrapped functions.  This is based on sockets but left open enough to deal with platforms that cannot manage this.  These extern to
	// the platform specific file.
	extern jrs_bool OpenSocket(jrs_socket &s, jrs_u16 uPort);
	extern jrs_socket AcceptSocket(jrs_socket s, jrs_i32 iBufSize);
	extern jrs_i32 CloseSocket(jrs_socket s);
	extern jrs_i32 SelectSocket(jrs_socket nfds, jrs_bool *pRreadfds, jrs_bool *pWritefds, jrs_i32 timeout_usec);
	extern jrs_i32 SendSocket(jrs_socket s, const jrs_i8 *buf, jrs_i32 len);
	extern jrs_i32 RecvSocket(jrs_socket s, jrs_i8 *buf, jrs_i32 len);	
	extern void MemoryManagerPlatformFunctionNameFromAddress(jrs_sizet uAddress, jrs_i8 *pName, jrs_sizet *pFuncStartAdd, jrs_sizet *pFuncSize);
	
	// Forward defs
    jrs_bool JRSMemory_LiveView_CreateNetworkConnection(jrs_socket &rServerSocket);
	void JRSMemory_LiveView_CloseNetworkConnection(jrs_socket &rServerSocket);
	jrs_bool JRSMemory_LiveView_RecvBufferHasData(void);
	jrs_bool JRSMemory_LiveView_RecvInternal(const char *pData, jrs_i32 iSize);
	jrs_bool JRSMemory_LiveView_Send(jrs_socket &rSendSocket, const char *pData, jrs_i32 iSize, jrs_u32 uFlags);
	jrs_bool JRSMemory_LiveView_SendAllocationDetails(void);
	jrs_bool JRSMemory_LiveView_SendHeapDetails(jrs_u32 uHeapID);
	jrs_bool JRSMemory_LiveView_SendInternal(jrs_socket &rSendSocket, const char *pData, jrs_i32 iSize);
	jrs_bool JRSMemory_LiveView_SendOverview(void);
	jrs_bool JRSMemory_LiveView_SendSystemDetails(void);
	jrs_bool JRSMemory_LiveView_SendPoolDetails(jrs_u32 uHeapID, jrs_u32 uPoolID);
	jrs_bool JRSMemory_LiveView_SendMethodInformation(jrs_u64 funcAddu32, jrs_u64 funcAddl32);
    void MemoryManagerLiveViewTransfer(void *pData, int size, const char *pFilePathAndName, jrs_bool bAppend);

	// For the base address calculation
	extern jrs_u64 g_uBaseAddressOffsetCalculation;

	// To let the system know if it can decode the symbols
	extern jrs_bool g_bCanDecodeSymbols;

	// Global client socket.
	static jrs_socket m_ClientSocket;

	// Elapsed time of the live view connection
	static jrs_u32 m_uLVTimeElapsed = 0;

	// Send buffer size for sending data
	static const jrs_i32 m_LVSendBufSize = 16 * 1024;

	// Buffer to store data being sent.
	static char m_LVSendBuf[m_LVSendBufSize];

	// Current location of data in the send buffer
	static jrs_i32 m_LVSendBufCur = 0;

	// Buffer for storing received data
	static char m_LVRecvBuf[m_LVSendBufSize];

	// Current location of data in the recv buffer
	static jrs_i32 m_LVRecvBufCur = 0;

	// Current location of read in the recv buffer
	static jrs_i32 m_LVRecvBufRead = 0;

	// Static type values for sending of data.
	static const jrs_u32 MemoryManager_PoolDetailType = 5;
	static const jrs_u32 MemoryManager_PoolInformationType = 6;
	static const jrs_u32 MemoryManager_MethodInformation = 8;
	static const jrs_u32 MemoryManager_MethodInformationSend = 9;

	// Each packet has this structure.  
	struct sPacket
	{
		jrs_u32 Size;
		jrs_u32 TimeMS;
		jrs_u32 Count;
		jrs_u32 Type;

		// Swaps data on big endian systems only.
		void SwapToLittleEndian(void)
		{
			Size = JRSEndianSwapToLittleEndian(Size);
			TimeMS = JRSEndianSwapToLittleEndian(TimeMS);
			Count = JRSEndianSwapToLittleEndian(Count);
			Type = JRSEndianSwapToLittleEndian(Type);
		}
	};

	// Structure for transferring heap data.
	struct sHeapData
	{
		jrs_u32 Header;
		jrs_u32 Id;
		jrs_u32 TotalAllocations;
		jrs_u32 TotalSize;

		// Swaps data on big endian systems only.
		void SwapToLittleEndian(void)
		{
			Header = JRSEndianSwapToLittleEndian(Header);
			Id = JRSEndianSwapToLittleEndian(Id);
			TotalAllocations = JRSEndianSwapToLittleEndian(TotalAllocations);
			TotalSize = JRSEndianSwapToLittleEndian(TotalSize);
		}
	};

	// Structure to receive method
	struct sMethodData
	{
		jrs_u64 Address;
		jrs_u32 Size;
		jrs_u32 StrLen;
		jrs_u64 FuncAddress;
		jrs_i8 Name[128];	

		// Swaps data on big endian systems only.
		void SwapToLittleEndian(void)
		{
			Address = JRSEndianSwapToLittleEndian(Address);
			Size = JRSEndianSwapToLittleEndian(Size);
			StrLen = JRSEndianSwapToLittleEndian(StrLen);
			FuncAddress = JRSEndianSwapToLittleEndian(FuncAddress);
		}
	};

	// Structure for transferring pool data.
	struct sPoolData
	{
		jrs_u32 PoolId;
		jrs_u32 HeapId;
		jrs_u32 TotalAllocations;
		jrs_u32 MaxAllocations;

		// Swaps data on big endian systems only.
		void SwapToLittleEndian(void)
		{
			PoolId = JRSEndianSwapToLittleEndian(PoolId);
			HeapId = JRSEndianSwapToLittleEndian(HeapId);
			TotalAllocations = JRSEndianSwapToLittleEndian(TotalAllocations);
			MaxAllocations = JRSEndianSwapToLittleEndian(MaxAllocations);
		}
	};
	
	// Transfers the heap details.
	struct sHeapDetailTransfer
	{
		jrs_u64 uAddress;
		jrs_u64 uSize;
		jrs_u64 uNameLen;
		jrs_u64 uAllocSize;
		jrs_i8 iName[128];

		// Swaps data on big endian systems only.
		void SwapToLittleEndian(void)
		{
			uAddress = JRSEndianSwapToLittleEndian(uAddress);
			uSize = JRSEndianSwapToLittleEndian(uSize);
			uNameLen = JRSEndianSwapToLittleEndian(uNameLen);
			uAllocSize = JRSEndianSwapToLittleEndian(uAllocSize);
		}
	};

	// Transfers the pool details.
	struct sPoolDetailTransfer
	{
		jrs_u64 uAddress;
		jrs_u64 uSize;
		jrs_u64 uNameLen;
		jrs_u64 uHeapId;
		jrs_i8 iName[128];

		// Swaps data on big endian systems only.
		void SwapToLittleEndian(void)
		{
			uAddress = JRSEndianSwapToLittleEndian(uAddress);
			uSize = JRSEndianSwapToLittleEndian(uSize);
			uNameLen = JRSEndianSwapToLittleEndian(uNameLen);
			uHeapId = JRSEndianSwapToLittleEndian(uHeapId);
		}
	};

	

	//  Description:
	//      Closes the network connection.  Internal only.
	//  See Also:
	//      JRSMemory_LiveView_CreateNetworkConnection
	//  Arguments:
	//     rServerSocket - Server socket to close.
	//  Return Value:
	//     Nothing.
	//  Summary:
	//      Closes the network connection.
	void JRSMemory_LiveView_CloseNetworkConnection(jrs_socket &rServerSocket)
	{
		CloseSocket(rServerSocket);
	}

	//  Description:
	//      Creates the network connection.  Internal only.
	//  See Also:
	//      JRSMemory_LiveView_CloseNetworkConnection
	//  Arguments:
	//     rServerSocket - Server socket to close.
	//  Return Value:
	//     Nothing.
	//  Summary:
	//      Creates the network connection.
	jrs_bool JRSMemory_LiveView_CreateNetworkConnection(jrs_socket &rServerSocket)
	{
		return OpenSocket(rServerSocket, cMemoryManager::Get().GetLVPortNumber());
	}

	//  Description:
	//      Checks if there is any remaining data to process in the recv buffer.  Internal only.
	//  See Also:
	//      
	//  Arguments:
	//      None
	//  Return Value:
	//      TRUE if there is still data to process.  FALSE otherwise.
	//  Summary:
	//      Checks if there is any remaining data to process in the recv buffer.
	jrs_bool JRSMemory_LiveView_RecvBufferHasData(void)
	{
		if(m_LVRecvBufCur < m_LVRecvBufRead)
			return TRUE;

		return FALSE;
	}

	//  Description:
	//      The main recv function which buffers internally the data. Internal only.
	//  See Also:
	//      
	//  Arguments:
	//      pData - Buffer to copy the data requested into.
	//		iSize - Size of the data in bytes requested.
	//  Return Value:
	//      TRUE for correctly recv'd data.  FALSE for an error.
	//  Summary:
	//		The main recv function which buffers internally the data.
	jrs_bool JRSMemory_LiveView_RecvInternal(const char *pData, jrs_i32 iSize)
	{
		while(true)
		{
			if(iSize <= (m_LVRecvBufRead - m_LVRecvBufCur))
			{
				// Copy the data into the buffer
				memcpy((void *)pData, &m_LVRecvBuf[m_LVRecvBufCur], iSize);
				m_LVRecvBufCur += iSize;

				return TRUE;
			}
			else
			{
				// Reset the data
				for(jrs_i32 i = 1; i < m_LVRecvBufCur; i++)
				{
					m_LVRecvBuf[i - 1] = m_LVRecvBuf[i];
				}
				m_LVRecvBufRead -= m_LVRecvBufCur;
				m_LVRecvBufCur = 0;

				// Need to read in more data
				int err = RecvSocket(m_ClientSocket, &m_LVRecvBuf[m_LVRecvBufRead], m_LVSendBufSize - m_LVRecvBufRead);
				if(err < 0)
				{
					CloseSocket(m_ClientSocket);
					return FALSE;
				}

				m_LVRecvBufRead += err;
			}
		
		}
	}

	//  Description:
	//      Sends data internally.  Handles platforms for that may not be able to deal with large data packets in one go.  Internal only.
	//  See Also:
	//      
	//  Arguments:
	//		rSendSocket - Socket to send data on.
	//		pData - Data to send.
	//		iSize - Size of data in bytes to send.
	//  Return Value:
	//      TRUE if sent successfully.  FALSE otherwise.
	//  Summary:
	//      Sends data internally.
	jrs_bool JRSMemory_LiveView_SendInternal(jrs_socket &rSendSocket, const char *pData, jrs_i32 iSize)
	{
		jrs_i32 size = 0;
		jrs_i32 dataleft = iSize;
		while(size < iSize)
		{
			int err = SendSocket(rSendSocket, &pData[size], dataleft);
			if(err < 0)
			{
				CloseSocket(rSendSocket);
				return FALSE;
			}

			size += err;
			dataleft -= err;
		}

		return TRUE;
	}

	//  Description:
	//      Sends data.  Internal only.
	//  See Also:
	//      
	//  Arguments:
	//		rSendSocket - Socket to send data on.
	//		pData - Data to send.
	//		iSize - Size of data in bytes to send.
	//		uFlags - Flags to send data.
	//  Return Value:
	//      TRUE if data sent successfully. FALSE if an error occured and the sockets was closed.
	//  Summary:
	//     Sends data.
	jrs_bool JRSMemory_LiveView_Send(jrs_socket &rSendSocket, const char *pData, jrs_i32 iSize, jrs_u32 uFlags)
	{
		jrs_bool err = TRUE;
		if(uFlags == 1)
		{
			if(m_LVSendBufCur + iSize >= m_LVSendBufSize)
			{
				err = JRSMemory_LiveView_SendInternal(rSendSocket, m_LVSendBuf, m_LVSendBufCur);
				m_LVSendBufCur = 0;
			}

			memcpy(&m_LVSendBuf[m_LVSendBufCur], pData, iSize);
			m_LVSendBufCur += iSize;

			return err;
		}
		else if(m_LVSendBufCur)
		{
			err = JRSMemory_LiveView_SendInternal(rSendSocket, m_LVSendBuf, m_LVSendBufCur);
			if(err == FALSE)
			{
				return FALSE;
			}
			m_LVSendBufCur = 0;
		}

		static int failcount;
		jrs_i32 timeout = 0;
		failcount = 0;

		int ret = 0;
		do
		{
			jrs_bool bWrite = FALSE;
			ret = SelectSocket(rSendSocket, NULL, &bWrite, timeout);
			if (ret > 0) 
			{
				if(bWrite) 
				{
					err = JRSMemory_LiveView_SendInternal(rSendSocket, pData, iSize);
					if(err == FALSE)
					{
						return FALSE;
					}
				}
			}
			else if(ret < 0)
			{
				CloseSocket(rSendSocket);
				return FALSE;
			}
			else
			{
				// Possibly disconnected
				timeout = 50000;
				if(failcount == 75)
				{
					CloseSocket(rSendSocket);
					return FALSE;
				}
			}
		}while(!ret);

		return TRUE;
	}

	//  Description:
	//      Sends the current memory manager status. Internal only.
	//  See Also:
	//      
	//  Arguments:
	//     None
	//  Return Value:
	//     TRUE if successful.  False otherwise.
	//  Summary:
	//     ends the current memory manager status.
	jrs_bool JRSMemory_LiveView_SendSystemDetails(void)
	{
		jrs_u64 systemdata[4];

		sPacket packet;
		packet.TimeMS = 0;
		packet.Count = 0;
		packet.Type = 32;
		packet.Size = sizeof(jrs_u64) * 4;
		packet.SwapToLittleEndian();

		if(!JRSMemory_LiveView_Send(m_ClientSocket, (const char *)&packet, sizeof(sPacket), 0)) 
			return FALSE;

		// Set the data up
		// systemdata[0] = bit 0: 1 for 64bit, 0 for 32bit.  bit 1: 1 for big endian, 0 for little. bit 2: 0 for function base address, 1 for module base address
		// systemdata[1] = 1 for sentinels on free/alloc, 0 for no sentinels
		// systemdata[2] = 1 for name/callstack, 0 none.
		// systemdata[3] = offset for callstack fixing address
		systemdata[0] = 0;
#ifdef JRS64BIT
		systemdata[0] = 1;
#endif
#ifdef JRSMEMORYBIGENDIAN
		systemdata[0] |= 2;
#endif
#ifdef JRSMEMORYMICROSOFTPLATFORMS
		systemdata[0] |= 4;
#endif
		if(g_bCanDecodeSymbols)
			systemdata[0] |= 8;

		systemdata[1] = 0;
#ifdef MEMORYMANAGER_ENABLESENTINELCHECKS
		systemdata[1] = 1;
#endif
#ifdef MEMORYMANAGER_ENABLENAMEANDSTACKCHECKS
		systemdata[1] |= 2;
#endif
		
		systemdata[2] = JRSMEMORY_CALLSTACKDEPTH;
		systemdata[3] = g_uBaseAddressOffsetCalculation;	

		systemdata[0] = JRSEndianSwapToLittleEndian(systemdata[0]);
		systemdata[1] = JRSEndianSwapToLittleEndian(systemdata[1]);
		systemdata[2] = JRSEndianSwapToLittleEndian(systemdata[2]);
		systemdata[3] = JRSEndianSwapToLittleEndian(systemdata[3]);
		if(!JRSMemory_LiveView_Send(m_ClientSocket, (const char *)systemdata, sizeof(jrs_u64) * 4, 0)) 
			return FALSE;

		return TRUE;
	}

	//  Description:
	//      Sends the current status of each heap. Internal only.
	//  See Also:
	//      
	//  Arguments:
	//     None
	//  Return Value:
	//     TRUE if successful.  False otherwise.
	//  Summary:
	//     Sends the current status of each heap.
	jrs_bool JRSMemory_LiveView_SendAllocationDetails(void)
	{
		jrs_i32 heapc = 0;
		sHeapData data[MemoryManager_MaxHeaps + MemoryManager_MaxUserHeaps + MemoryManager_MaxNonIntrusiveHeaps];	

		for(jrs_u32 i = 0; i < cMemoryManager::Get().GetMaxNumHeaps(); i++)
		{
			cHeap *pHeap = cMemoryManager::Get().GetHeap(i);
			if(pHeap)
			{
				data[heapc].Header = 1;
				data[heapc].Id = pHeap->GetUniqueId();
				data[heapc].TotalAllocations = pHeap->GetNumberOfAllocations();
				data[heapc].TotalSize = (jrs_u32)pHeap->GetMemoryUsed();
				data[heapc].SwapToLittleEndian();
				heapc++;
			}
		}

		for(jrs_u32 i = 0; i < cMemoryManager::Get().GetMaxNumUserHeaps(); i++)
		{
			cHeap *pHeap = cMemoryManager::Get().GetUserHeap(i);
			if(pHeap)
			{
				data[heapc].Header = 1;
				data[heapc].Id = pHeap->GetUniqueId();
				data[heapc].TotalAllocations = pHeap->GetNumberOfAllocations();
				data[heapc].TotalSize = (jrs_u32)pHeap->GetMemoryUsed();
				data[heapc].SwapToLittleEndian();
				heapc++;
			}
		}

		for(jrs_u32 i = 0; i < cMemoryManager::Get().GetMaxNumNIHeaps(); i++)
		{
			cHeapNonIntrusive *pHeap = cMemoryManager::Get().GetNIHeap(i);
			if(pHeap)
			{
				data[heapc].Header = 1;
				data[heapc].Id = pHeap->GetUniqueId();
				data[heapc].TotalAllocations = pHeap->GetNumberOfAllocations();
				data[heapc].TotalSize = (jrs_u32)pHeap->GetMemoryUsed();
				data[heapc].SwapToLittleEndian();
				heapc++;
			}
		}

		sPacket packet;
		packet.TimeMS = m_uLVTimeElapsed;
		packet.Count = heapc;
		packet.Type = 1;
		packet.Size = sizeof(sHeapData) * heapc;
		packet.SwapToLittleEndian();

		if(!JRSMemory_LiveView_Send(m_ClientSocket, (const char *)&packet, sizeof(sPacket), 0)) return FALSE;
		if(!JRSMemory_LiveView_Send(m_ClientSocket, (const char *)data, sizeof(sHeapData) * heapc, 0)) return FALSE;

		// Send the pool details
		for(jrs_u32 i = 0; i < cMemoryManager::Get().GetMaxNumHeaps(); i++)
		{
			cHeap *pHeap = cMemoryManager::Get().GetHeap(i);
			if(pHeap)
			{
				cPoolBase *pPool = pHeap->GetPool(NULL);
				while(pPool)
				{
					// Create the packet details
					packet.Count = 1;
					packet.Type = MemoryManager_PoolDetailType;
					packet.Size = sizeof(sPoolData);
					packet.SwapToLittleEndian();

					sPoolData pdata;
					pdata.PoolId = pPool->GetUniqueId();
					pdata.HeapId = pHeap->GetUniqueId();
					pdata.TotalAllocations = pPool->GetTotalAllocations();
					pdata.MaxAllocations = pPool->GetMaxAllocations();
					pdata.SwapToLittleEndian();

					if(!JRSMemory_LiveView_Send(m_ClientSocket, (const char *)&packet, sizeof(sPacket), 0)) return FALSE;
					if(!JRSMemory_LiveView_Send(m_ClientSocket, (const char *)&pdata, sizeof(sPoolData), 0)) return FALSE;

					// Next pool
					pPool = pHeap->GetPool(pPool);
				}
			}
		}


		return TRUE;
	}

	//  Description:
	//      Sends the heap details (name, address etc). Internal only.
	//  See Also:
	//      
	//  Arguments:
	//      uHeapID - Id of the heap to send.
	//  Return Value:
	//      TRUE if successful.
	//  Summary:
	//      Sends the heap details (name, address etc).
	jrs_bool JRSMemory_LiveView_SendHeapDetails(jrs_u32 uHeapID)
	{
		cHeap *pFHeap = NULL;
		for(jrs_u32 i = 0; i < cMemoryManager::Get().GetMaxNumHeaps(); i++)
		{
			cHeap *pHeap = cMemoryManager::Get().GetHeap(i);
			if(pHeap && pHeap->GetUniqueId() == uHeapID)
			{
				pFHeap = pHeap;
				break;
			}
		}

		if(!pFHeap)
		{
			for(jrs_u32 i = 0; i < cMemoryManager::Get().GetMaxNumUserHeaps(); i++)
			{
				cHeap *pHeap = cMemoryManager::Get().GetUserHeap(i);
				if(pHeap && pHeap->GetUniqueId() == uHeapID)
				{
					pFHeap = pHeap;
					break;
				}
			}
		}

		if(!pFHeap)
		{
			for(jrs_u32 i = 0; i < cMemoryManager::Get().GetMaxNumNIHeaps(); i++)
			{
				cHeapNonIntrusive *pHeap = cMemoryManager::Get().GetNIHeap(i);
				if(pHeap && pHeap->GetUniqueId() == uHeapID)
				{
					sHeapDetailTransfer details;
					strcpy(details.iName, pHeap->GetName());
					if(strlen(details.iName) + strlen("(NI)") < 32)
						strcat(details.iName, "(NI)");
					details.uNameLen = strlen(details.iName);
					details.uAddress = (jrs_sizet)pHeap->GetAddress();
					details.uSize = (jrs_sizet)pHeap->GetSize();
					details.uAllocSize = (jrs_sizet)cMemoryManager::SizeofAllocatedBlock();
					details.SwapToLittleEndian();

					sPacket packet;
					packet.TimeMS = m_uLVTimeElapsed;
					packet.Count = pHeap->GetUniqueId();
					packet.Type = 2;
					packet.Size = sizeof(sHeapDetailTransfer);
					packet.SwapToLittleEndian();
					JRSMemory_LiveView_Send(m_ClientSocket, (const char *)&packet, sizeof(sPacket), 0);
					JRSMemory_LiveView_Send(m_ClientSocket, (const char *)&details, sizeof(sHeapDetailTransfer), 0);

					return TRUE;
				}
			}
		}

		if(!pFHeap)
			return TRUE;

		sHeapDetailTransfer details;
		strcpy(details.iName, pFHeap->GetName());
		details.uNameLen = strlen(details.iName);
		details.uAddress = (jrs_sizet)pFHeap->GetAddress();
		details.uSize = (jrs_sizet)pFHeap->GetSize();
		details.uAllocSize = (jrs_sizet)cMemoryManager::SizeofAllocatedBlock();
		details.SwapToLittleEndian();
	
		sPacket packet;
		packet.TimeMS = m_uLVTimeElapsed;
		packet.Count = pFHeap->GetUniqueId();
		packet.Type = 2;
		packet.Size = sizeof(sHeapDetailTransfer);
		packet.SwapToLittleEndian();
		JRSMemory_LiveView_Send(m_ClientSocket, (const char *)&packet, sizeof(sPacket), 0);
		JRSMemory_LiveView_Send(m_ClientSocket, (const char *)&details, sizeof(sHeapDetailTransfer), 0);

		return TRUE;
	}

	//  Description:
	//      Sends the method information. Internal only.
	//  See Also:
	//      
	//  Arguments:
	//      funcAdd - address of the function
	//  Return Value:
	//      TRUE if successful.
	//  Summary:
	//      Sends the method information (name, address etc).
	jrs_bool JRSMemory_LiveView_SendMethodInformation(jrs_u64 funcAddu32, jrs_u64 funcAddl32)
	{
		jrs_u64 add = funcAddu32;
		add <<= 32;
		add |= funcAddl32;

		sMethodData method;
		jrs_sizet uFuncStartAdd = 0;
		jrs_sizet uFuncSize = 0;
		strcpy(method.Name, "Unknown");
		MemoryManagerPlatformFunctionNameFromAddress((jrs_sizet)add, method.Name, &uFuncStartAdd, &uFuncSize);
		method.Address = (jrs_u64)add;	
		method.FuncAddress = (jrs_u64)uFuncStartAdd;	
		method.StrLen = (jrs_u32)strlen(method.Name);
		method.Size = (jrs_u32)uFuncSize;

		sPacket packet;
		packet.TimeMS = 0;
		packet.Count = 0;
		packet.Type = MemoryManager_MethodInformation;
		packet.Size = sizeof(sMethodData);
		packet.SwapToLittleEndian();

		
		method.SwapToLittleEndian();
		JRSMemory_LiveView_Send(m_ClientSocket, (const char *)&packet, sizeof(sPacket), 0);
		JRSMemory_LiveView_Send(m_ClientSocket, (const char *)&method, sizeof(sMethodData), 1);

		return TRUE;
	}

	//  Description:
	//      Sends the pool details (name, address etc). Internal only.
	//  See Also:
	//      
	//  Arguments:
	//      uHeapID - Id of the heap to send.
	//		uPoolID - Id of the pool to send.
	//  Return Value:
	//      TRUE if successful.
	//  Summary:
	//      Sends the pool details (name, address etc).
	jrs_bool JRSMemory_LiveView_SendPoolDetails(jrs_u32 uHeapID, jrs_u32 uPoolID)
	{
		cHeap *pFHeap = NULL;
		for(jrs_u32 i = 0; i < cMemoryManager::Get().GetMaxNumHeaps(); i++)
		{
			cHeap *pHeap = cMemoryManager::Get().GetHeap(i);
			if(pHeap && pHeap->GetUniqueId() == uHeapID)
			{
				pFHeap = pHeap;
				break;
			}
		}

		if(!pFHeap)
		{
			for(jrs_u32 i = 0; i < cMemoryManager::Get().GetMaxNumUserHeaps(); i++)
			{
				cHeap *pHeap = cMemoryManager::Get().GetUserHeap(i);
				if(pHeap && pHeap->GetUniqueId() == uHeapID)
				{
					pFHeap = pHeap;
					break;
				}
			}
		}

		if(!pFHeap)
			return TRUE;

		// Find the pool
		cPoolBase *pPool = pFHeap->GetPool(NULL);
		while(pPool)
		{
			if(pPool->GetUniqueId() == uPoolID)
				break;

			pPool = pFHeap->GetPool(pPool);
		}

		if(!pPool)
			return TRUE;

		sPoolDetailTransfer details;
		strcpy(details.iName, pPool->GetName());
		details.uNameLen = strlen(details.iName);
		details.uAddress = (jrs_sizet)pPool->GetAddress();
		details.uSize = (jrs_sizet)pPool->GetSize();
		details.uHeapId = uHeapID;
		details.SwapToLittleEndian();

		sPacket packet;
		packet.TimeMS = m_uLVTimeElapsed;
		packet.Count = pPool->GetUniqueId();
		packet.Type = MemoryManager_PoolInformationType;
		packet.Size = sizeof(sPoolDetailTransfer);
		packet.SwapToLittleEndian();
		JRSMemory_LiveView_Send(m_ClientSocket, (const char *)&packet, sizeof(sPacket), 0);
		JRSMemory_LiveView_Send(m_ClientSocket, (const char *)&details, sizeof(sPoolDetailTransfer), 0);

		return TRUE;
	}

	//  Description:
	//      Internal only.
	//  See Also:
	//      
	//  Arguments:
	//		pData - Incoming data to send.
	//		size - Size in bytes of data to send.
	//		pFilePathAndName - Ignored.
	//		bAppend - Ignored.
	//  Return Value:
	//      None.
	//  Summary:
	//      Main function for transferring data to the client.
	void MemoryManagerLiveViewTransfer(void *pData, int size, const char *pFilePathAndName, jrs_bool bAppend)
	{
		char buffer[2048];
		sPacket *pPacket = (sPacket *)buffer;
		memcpy(&buffer[sizeof(sPacket)], pData, size);

		pPacket->TimeMS = m_uLVTimeElapsed;
		pPacket->Count = 0;
		pPacket->Type = 3;
		pPacket->Size = size;
		pPacket->SwapToLittleEndian();
		if(!JRSMemory_LiveView_Send(m_ClientSocket, (const char *)pPacket, size + sizeof(sPacket), 1))
		{
			return;
		}
	}



	//  Description:
	//      Sends an the operation data. Internal only.
	//  See Also:
	//      
	//  Arguments:
	//		None
	//  Return Value:
	//      TRUE if data was sent.  FALSE for error.
	//  Summary:
	//      Sends the operation data .
	jrs_bool cMemoryManager::JRSMemory_LiveView_SendOperations(void *pBuffer, JRSMemory_ThreadLock *pThreadLock)
	{
		pThreadLock->Lock();
		jrs_u32 uWritePtr = cMemoryManager::Get().m_uELVDBWrite;
		jrs_u32 uReadPtr = cMemoryManager::Get().m_uELVDBRead;
		jrs_u32 uWrapPtr = cMemoryManager::Get().m_uELVDBWriteEnd;
		pThreadLock->Unlock();
		
		// Get the size
		if(uReadPtr == uWritePtr)
			return TRUE;

		// Packet size - Send it in 16k chunks
		sPacket packet;
		const jrs_u32 DataSize = 1024 * 16;
		jrs_u32 size = 0;		

		jrs_u32 uNewWrap = uWrapPtr;
		jrs_u32 uReadMsb = uReadPtr & 1;
		jrs_u32 uWriteMsb = uWritePtr & 1;

		uReadPtr &= ~1;
		uWritePtr &= ~1;

		// Work out how much to send
		jrs_u8 *pReadPtr = cMemoryManager::Get().m_pELVDebugBuffer + uReadPtr;
		if(uReadMsb == uWriteMsb)
		{
			// Easy, just send a big batch - uWritePtr is larger than uReadptr
			packet.TimeMS = m_uLVTimeElapsed;
			packet.Type = 4;
			size = packet.Size = (jrs_u32)(uWritePtr - uReadPtr);
			packet.Count = 0;
			packet.SwapToLittleEndian();

			if(!JRSMemory_LiveView_Send(m_ClientSocket, (const char *)&packet, sizeof(sPacket), 1))
			{
				return FALSE;
			}

			while(size)
			{
				jrs_u32 cop = size > DataSize ? DataSize : size;
				if(!JRSMemory_LiveView_Send(m_ClientSocket, (const char *)pReadPtr, cop, 1))
				{
					return FALSE;
				}

				uReadPtr += cop;
				pReadPtr += cop;
				size -= cop;
			}

			uReadPtr = uReadPtr | (uReadMsb);
		}
		else
		{
			//uReadPtr is in the previous ring.  Just transfer to the end.
			packet.TimeMS = m_uLVTimeElapsed;
			packet.Type = 4;
			size = packet.Size = (jrs_u32)(uWrapPtr - uReadPtr);
			packet.Count = 0;
			packet.SwapToLittleEndian();

			// It is possible to get a 0 size
			if(size != 0)
			{				
				if(!JRSMemory_LiveView_Send(m_ClientSocket, (const char *)&packet, sizeof(sPacket), 1))
				{
					return FALSE;
				}

				while(size)
				{
					jrs_u32 cop = size > DataSize ? DataSize : size;
					if(!JRSMemory_LiveView_Send(m_ClientSocket, (const char *)pReadPtr, cop, 1))
					{
						return FALSE;
					}

					uReadPtr += cop;
					pReadPtr += cop;
					size -= cop;
				}
			}

			uNewWrap = 0;
			uReadPtr = 0 | (uReadMsb ^ 1);
		}

		// Increment the next end value
		pThreadLock->Lock();
		cMemoryManager::Get().m_uELVDBRead = uReadPtr;

		if(uWrapPtr && !uNewWrap)
		{
			cMemoryManager::Get().m_uELVDBWriteEnd = uNewWrap;
		}
		pThreadLock->Unlock();

		return TRUE;
	}

	//  Description:
	//      Sends an overview. Internal only.
	//  See Also:
	//      
	//  Arguments:
	//		None
	//  Return Value:
	//      TRUE if data was sent.  FALSE for error.
	//  Summary:
	//      Sends an overview.
	jrs_bool JRSMemory_LiveView_SendOverview(void)
	{
		// Send a packet letting us know what to expect
		sPacket packet;
		packet.TimeMS = m_uLVTimeElapsed;
		packet.Count = 0;
		packet.Type = 3;
		packet.Size = 0;
		packet.SwapToLittleEndian();
		if(!JRSMemory_LiveView_Send(m_ClientSocket, (const char *)&packet, sizeof(sPacket), 1))
		{
			return FALSE;
		}

		// Do the basic reporting and dump the information
		for(jrs_u32 i = 0; i < cMemoryManager::Get().GetMaxNumHeaps(); i++)
		{
			cHeap *pHeap = cMemoryManager::Get().GetHeap(i);
			if(pHeap)
			{
				pHeap->ReportAllocationsMemoryOrder();
			}
		}

		for(jrs_u32 i = 0; i < cMemoryManager::Get().GetMaxNumUserHeaps(); i++)
		{
			cHeap *pHeap = cMemoryManager::Get().GetUserHeap(i);
			if(pHeap)
			{
				pHeap->ReportAllocationsMemoryOrder();
			}
		}

		for(jrs_u32 i = 0; i < cMemoryManager::Get().GetMaxNumNIHeaps(); i++)
		{
			cHeapNonIntrusive *pHeap = cMemoryManager::Get().GetNIHeap(i);
			if(pHeap)
			{
				pHeap->ReportAllocationsMemoryOrder();
			}
		}

		packet.TimeMS = m_uLVTimeElapsed;
		packet.Count = 0;
		packet.Type = 18;
		packet.Size = 0;
		packet.SwapToLittleEndian();
		if(!JRSMemory_LiveView_Send(m_ClientSocket, (const char *)&packet, sizeof(sPacket), 1))
		{
			return FALSE;
		}

		// Do we need to send and recieve the mapfile data?


		return TRUE;
	}

	//  Description:
	//		Main Live View thread.  Controls communication with the outside world. Internal only.
	//  See Also:
	//      InitializeLiveView
	//  Arguments:
	//		pArg - Pointer value to active variable of the LV thread.
	//  Return Value:
	//      None.
	//  Summary:
	//      Main Live View thread
	cJRSThread::jrs_threadout cMemoryManager::JRSMemory_LiveViewThread(cJRSThread::jrs_threadin pArg)
	{
		jrs_socket ServerSocket;
		jrs_bool *pActiveLiveView = (jrs_bool *)((jrs_sizet)pArg);

		if(!JRSMemory_LiveView_CreateNetworkConnection(ServerSocket))
		{
			*pActiveLiveView = FALSE;
			JRSThreadReturn(1);
		}

		// Create the timer
		cJRSTimer LVTimer;

		jrs_bool NoConnection = TRUE;
		jrs_bool MapRetrieve = FALSE;
		while(*pActiveLiveView && cMemoryManager::Get().IsInitialized())
		{
			// Do we have a connection?
			if(NoConnection)
			{
				// No, get one.
				jrs_bool bRead = FALSE;		
				m_bELVContinuousGrab = false;
				int ret = SelectSocket(ServerSocket, &bRead, NULL, 1000000 - 1);
				if (ret > 0) 
				{
					if(bRead) 
					{
						// Accept incoming connection and add new socket to list
						jrs_u32 sendbuff = 128 * 1024; 
						m_ClientSocket = AcceptSocket(ServerSocket, sendbuff);
						NoConnection = FALSE;
						MapRetrieve = FALSE;
						m_uLVTimeElapsed = 0;			// reset timer
						m_LVSendBufCur = 0;
						m_LVRecvBufCur = 0;
						m_LVRecvBufRead = 0;
						cMemoryManager::Get().m_uELVDBRead = 0;
						cMemoryManager::Get().m_uELVDBWrite = 0;						
						cMemoryManager::Get().m_uELVDBWriteEnd = 0;	
						cMemoryManager::Get().m_bLiveViewRunning = TRUE;	

						// Send some init data to get us going
						JRSMemory_LiveView_SendSystemDetails();
					}
				}			
			}
			else
			{
				// Update the time
				m_uLVTimeElapsed = LVTimer.GetElapsedTimeMilliSec(true);

				// Do not send this data while wating for a map file
				if(!MapRetrieve)
				{
					// Connection established.  Write some data.
					if(JRSMemory_LiveView_SendAllocationDetails() == FALSE)
					{
						cMemoryManager::Get().m_bLiveViewRunning = FALSE;
						NoConnection = TRUE;
						continue;
					}

					// Check if anything has been forced
					if(m_bForceLVOverviewGrab)
					{
						JRSMemory_LiveView_SendOverview();
						m_bForceLVOverviewGrab = FALSE;
					}

					if(m_bForceLVContinuousGrab)
					{
						m_bForceLVContinuousGrab = FALSE;
						sPacket contPacket;
						contPacket.TimeMS = m_uLVTimeElapsed;
						contPacket.Type = 50;
						contPacket.Size = 0;
						contPacket.Count = 0;
						contPacket.SwapToLittleEndian();

						if(!JRSMemory_LiveView_Send(m_ClientSocket, (const char *)&contPacket, sizeof(sPacket), 0))
						{
							cMemoryManager::Get().m_bLiveViewRunning = FALSE;
							NoConnection = TRUE;
						}
					}

					// Send the operation details
					if(m_bELVContinuousGrab && cMemoryManager::JRSMemory_LiveView_SendOperations(cMemoryManager::Get().m_pELVDebugBuffer, &cMemoryManager::Get().m_LVThreadLock) == FALSE)
					{
						cMemoryManager::Get().m_bLiveViewRunning = FALSE;
						NoConnection = TRUE;
						continue;
					}
				}
				
				// Check if we have data waiting to be read
				jrs_bool bReadAvailable = FALSE;
				int ret = SelectSocket(m_ClientSocket, &bReadAvailable, NULL, 0);
				if (ret > 0) 
				{
					if(bReadAvailable) 
					{
						// Accept incoming connection and add new socket to list
						char buffer[2048];
						do
						{
							if(!JRSMemory_LiveView_RecvInternal(buffer, 16))
							{
								cMemoryManager::Get().m_bLiveViewRunning = FALSE;
								NoConnection = TRUE;
								continue;
							}

							sPacket *pPacket = (sPacket *)buffer;
							pPacket->SwapToLittleEndian();
							switch(pPacket->Type)
							{
							case 2:
								// We want to send the names of heap.
								JRSMemory_LiveView_SendHeapDetails(pPacket->Count);
								break;
							case 3:
								// We want to send overview information
								JRSMemory_LiveView_SendOverview();
								break;
							case 4:
								// We want to start/stop continue logging
								m_bELVContinuousGrab = !m_bELVContinuousGrab;
								cMemoryManager::Get().m_uELVDBRead = 0;
								cMemoryManager::Get().m_uELVDBWrite = 0;
								cMemoryManager::Get().m_uELVDBWriteEnd = 0;
							case MemoryManager_PoolInformationType:
								// We want to send the names of the pools.
								JRSMemory_LiveView_SendPoolDetails(pPacket->Size, pPacket->Count);
								break;
							case MemoryManager_MethodInformation:
								// We want to send the names of the pools.
								JRSMemory_LiveView_SendMethodInformation(pPacket->Size, pPacket->Count);
								break;
							case MemoryManager_MethodInformationSend:
								// We will receive heap data
								MapRetrieve = pPacket->Count ? TRUE : FALSE;
								break;
							default:
								break;
							}
						}while(JRSMemory_LiveView_RecvBufferHasData());

					}
				}

				JRSThread::SleepMilliSecond(cMemoryManager::m_uLiveViewPoll);
			}
		}

		// End
		cMemoryManager::Get().m_bLiveViewRunning = FALSE;
		JRSMemory_LiveView_CloseNetworkConnection(ServerSocket);

		JRSThreadReturn(1);
	}

}

#else
// Empty function for the main code to work without change.
namespace Elephant
{
	//  Description:
	//      Internal only.  Stub function for when Live view isnt available.
	//  See Also:
	//      
	//  Arguments:
	//		pData - Incoming data to send.
	//		size - Size in bytes of data to send.
	//		pFilePathAndName - Ignored.
	//		bAppend - Ignored.
	//  Return Value:
	//      None.
	//  Summary:
	//      Main function for transferring data to the client.
	void MemoryManagerLiveViewTransfer(void *pData, int size, const char *pFilePathAndName, jrs_bool bAppend)
	{
		
	}
}

#endif	// JRSMEMORY_HASSOCKETS
