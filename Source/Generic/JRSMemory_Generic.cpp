/* 
(C) Copyright 2010 Jury Rig Software Limited. All Rights Reserved. 

Use of this software is subject to the terms of an end user license agreement.
This software contains code, techniques and know-how which is confidential and proprietary to Jury Rig Software Ltd.
Not for disclosure or distribution without Jury Rig Software Ltd's prior written consent. 
*/
#include <JRSMemory.h>
#include <JRSMemory_Pools.h>

// Typical includes needed for sockets.  Note that most implementations of sockets are identical but may change the name of the select and close functions.
// #include <sys/types.h>
// #include <sys/socket.h>
// #include <sys/select.h>
// #include <netinet/in.h>
// #include <netinet/tcp.h>
// #include <sys/time.h>
// #include <unistd.h>
// 
// typedef int SOCKET;


namespace Elephant
{
	// Forward declarations to avoid compiler errors on some platforms
	void *MemoryManagerDefaultSystemAllocator(jrs_u64 uSize, void *pExtMemoryPtr);
	void MemoryManagerDefaultSystemFree(void *pFree, jrs_u64 uSize);
	void MemoryManagerPlatformInit(void);
	void MemoryManagerPlatformDestroy(void);
	jrs_sizet MemoryManagerPlatformAddressToBaseAddress(jrs_sizet uAddress);
	void MemoryManagerPlatformFunctionNameFromAddress(jrs_sizet uAddress, jrs_i8 *pName, jrs_sizet *pFuncStartAdd, jrs_sizet *pFuncSize);
	jrs_sizet MemoryManagerSystemPageSize(void);

	// Socket wrappers
	jrs_bool OpenSocket(jrs_socket &s, jrs_u16 uPort);
	jrs_socket AcceptSocket(jrs_socket s, jrs_i32 iBufSize);
	jrs_i32 CloseSocket(jrs_socket s);
	jrs_i32 SelectSocket(jrs_socket nfds, jrs_bool *pRreadfds, jrs_bool *pWritefds, jrs_i32 timeout_usec);
	jrs_i32 SendSocket(jrs_socket s, const jrs_i8 *buf, jrs_i32 len);
	jrs_i32 RecvSocket(jrs_socket s, jrs_i8 *buf, jrs_i32 len);	

	void *MemoryManagerDefaultSystemAllocator(jrs_u64 uSize, void *pExtMemoryPtr)
	{
		return malloc((size_t)uSize);
	}

	void MemoryManagerDefaultSystemFree(void *pFree, jrs_u64 uSize)
	{
		free(pFree);
	}

	jrs_sizet MemoryManagerSystemPageSize(void)
	{
		return 1024 * 64;
	}

	void MemoryManagerPlatformInit(void)
	{

	}

	void MemoryManagerPlatformDestroy(void)
	{
	}

	jrs_sizet MemoryManagerPlatformAddressToBaseAddress(jrs_sizet uAddress)
	{
		return uAddress;
	}

	void MemoryManagerPlatformFunctionNameFromAddress(jrs_sizet uAddress, jrs_i8 *pName, jrs_sizet *pFuncStartAdd, jrs_sizet *pFuncSize)
	{

	}

	void cMemoryManager::StackTrace(jrs_sizet *pCallStack, jrs_u32 uCallstackDepth, jrs_u32 uCallStackCount)
	{
		memset(pCallStack, 0, uCallStackCount * sizeof(jrs_sizet));
	}

	void cPoolBase::StackTrace(jrs_sizet *pCallStack)
	{
		memset(pCallStack, 0, JRSMEMORY_CALLSTACKDEPTH * sizeof(jrs_sizet));
	}

	// Socket wrappers per platform.  This doesn't have to be sockets but must function the same.  Although we call it a socket it could just be an id that is passed around.

	//  Description:
	//      Accepts a client connection from the input server connection.  Then sets the network send buffer to the buffer size.  Some platforms will send a signal (if usinf sockets)
	//		on an error which we disable.  This is generally on BSD implementations. Internal only.
	//  See Also:
	//      
	//  Arguments:
	//      s - Server id/socket.
	//		iBufSize - The size of the send buffer.
	//  Return Value:
	//      The client socket id
	//  Summary:
	//      Accepts a client connection.
	jrs_socket AcceptSocket(jrs_socket s, jrs_i32 iBufSize)
	{
//		*** Socket code - works on most platforms (dont forget the includes)***
// 		// Accept the socket
// 		SOCKET ClientSocket = accept((SOCKET)s, NULL, NULL);
// 
// 		// Set the buffer size.
// 		setsockopt(ClientSocket, SOL_SOCKET, SO_SNDBUF, (const char *)&iBufSize, sizeof(jrs_i32));
// 
// 		// Potentially disable socket signals
// #ifdef SO_NOSIGPIPE
// 		jrs_u32 setnosig = 1;
// 		setsockopt(ClientSocket, SOL_SOCKET, SO_NOSIGPIPE, (void *)&setnosig, sizeof(int));
// #endif
// 
// 		return ClientSocket;
		return 0;
	}

	//  Description:
	//      Opens a standard server point of entry for the networking. Internal only.
	//  See Also:
	//      
	//  Arguments:
	//      s - Returns the server id/socket via reference.
	//  Return Value:
	//      TRUE if a point of entry/socket could be created.  FALSE otherwise.
	//  Summary:
	//      Opens a server connection.
	jrs_bool OpenSocket(jrs_socket &s, jrs_u16 uPort)
	{
//		*** Socket code - works on most platforms (dont forget the includes)***
// 
// 		// Standard connection
// 		SOCKET rServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // Create socket
// 		if(rServerSocket < 0)
// 		{
// 			return FALSE; //Don't continue if we couldn't create a socket!!
// 		}
// 
// 		// Set the connection up
// 		sockaddr_in addr; // The address structure for a TCP socket
// 		addr.sin_family = AF_INET;      // Address family
// 		addr.sin_port = htons(uPort); 
// 		addr.sin_addr.s_addr = INADDR_ANY;
// 		if(bind(rServerSocket, (sockaddr *)&addr, sizeof(addr)) < 0)
// 		{
// 			//We couldn't bind
// 			CloseSocket(rServerSocket);
// 			return FALSE;
// 		}
// 
// 		//Now we can start listening
// 		listen(rServerSocket, 1);
// 		s = rServerSocket;
// 		return TRUE;
		return FALSE;
	}

	//  Description:
	//      Closes the specified network connection. Internal only.
	//  See Also:
	//      
	//  Arguments:
	//      s - Id/socket to close.
	//  Return Value:
	//      0 if no error occurred.
	//  Summary:
	//      Closes a server connection.
	jrs_i32 CloseSocket(jrs_socket s)
	{
//		*** Socket code - works on most platforms (dont forget the includes)***
//		return close((SOCKET)s);
		return 0;
	}

	//  Description:
	//      Works just like select would for sockets only hides it behind an interface. Internal only.
	//  See Also:
	//      
	//  Arguments:
	//      jrs_nfds - Id/socket to test against.
	//		pRreadfds - Pointer to a jrs_bool.  This value is used to check if any data has been received.  NULL to ignore any read information checks.  This will be set to TRUE if data can be read else FALSE.
	//		pWritefds - Pointer to a jrs_bool.  This value is used to check if any data can be written.  NULL to ignore any write information checks. This will be set to TRUE if data can be written else FALSE.
	//		timeout_usec - Time value in usec to wait.  1000 is 1milisecond.  0 to continue.
	//  Return Value:
	//      0 if a time out happened, <0 if an error occured. >0 if succeeded.
	//  Summary:
	//      Closes a server connection.
	jrs_i32 SelectSocket(jrs_socket jrs_nfds, jrs_bool *pRreadfds, jrs_bool *pWritefds, jrs_i32 timeout_usec)
	{
//		*** Socket code - works on most platforms (dont forget the includes)***
//
// 		fd_set readfds;
// 		fd_set wrfds;
// 		int ret = 0;
// 		int nfds = (int)jrs_nfds;
// 
// 		timeval tv = { 0, 0 };
// 		tv.tv_sec = 0;
// 		tv.tv_usec = timeout_usec;
// 
// 		if(pRreadfds)
// 		{
// 			FD_ZERO(&readfds);
// 			FD_SET(nfds, &readfds);
// 		}
// 
// 		if(pWritefds)
// 		{
// 			FD_ZERO(&wrfds);
// 			FD_SET(nfds, &wrfds);
// 		}
// 
// 		// Check the data.
// 		ret = select((int)nfds + 1, pRreadfds != NULL ? &readfds : NULL, pWritefds != NULL ? &wrfds : NULL, NULL, &tv);
// 		if (ret > 0) 
// 		{
// 			if(pRreadfds)
// 			{
// 				*pRreadfds = FD_ISSET(nfds, &readfds) ? TRUE : FALSE;
// 			}
// 
// 			if(pWritefds)
// 			{
// 				*pWritefds = FD_ISSET(nfds, &wrfds) ? TRUE : FALSE;
// 			}
// 		}
// 
// 		return ret;
		return 0;
	}

	//  Description:
	//      Sends data to the specified network connection. Internal only.
	//  See Also:
	//      
	//  Arguments:
	//      s - Id/socket to send data to.
	//		buf - Pointer to the buffer of data to send.
	//		len - Amount of data in bytes to send.
	//  Return Value:
	//      <0 for error otherwise number of bytes sent which may not be the same size of len.
	//  Summary:
	//      Sends data to the specified network connection.
	jrs_i32 SendSocket(jrs_socket s, const jrs_i8 *buf, jrs_i32 len)
	{
//		*** Socket code - works on most platforms (dont forget the includes)***
//
//		return send((SOCKET)s, buf, len, 0);
		return 0;
	}

	//  Description:
	//      Receives data from the specified network connection. Internal only.
	//  See Also:
	//      
	//  Arguments:
	//      s - Id/socket to receive data from.
	//		buf - Pointer to the buffer of data to store the received information.
	//		len - Amount of data in bytes to read.
	//  Return Value:
	//      <0 for error otherwise number of bytes recieved which may not be the same size of len.
	//  Summary:
	//      Receives data from the specified network connection.
	jrs_i32 RecvSocket(jrs_socket s, jrs_i8 *buf, jrs_i32 len)
	{
//		*** Socket code - works on most platforms (dont forget the includes)***
//
//		return recv((SOCKET)s, buf, len, 0);
		return 0;
	}
}