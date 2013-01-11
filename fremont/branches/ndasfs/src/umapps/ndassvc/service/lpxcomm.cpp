/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include "lpxcomm.h"
#include <socketlpx.h>
#include <binparams.h> // FIRST_TARGET_XXX
#include <stdio.h>
#include <ndas/ndasdib.h>

#include "trace.h"
#ifdef RUN_WPP
#include "lpxcomm.tmh"
#endif

/*++

Description:

	GetLpxInterfaceList returns the list of the addresses of interfaces 
	that are bound to the LPX protocol.

	To get the required size of the list, set lpSocketAddress and 
	cbSocketAddress to NULL and 0.

Returns:

	If the function succeeds, the return value is nonzero.

	If the function fails, the return value is zero.
	To get extended error information, call WSAGetLastError.

--*/

BOOL GetLocalLpxAddressList(
	IN SOCKET s,
	IN DWORD cbBuffer,
	OUT LPSOCKET_ADDRESS_LIST lpBuffer,
	OUT LPDWORD pcbBytesReturned)
{
	XTLASSERT(INVALID_SOCKET != s);
	XTLASSERT(!IsBadWritePtr(lpBuffer, cbBuffer));

	INT iError = WSAIoctl(
		s,
		SIO_ADDRESS_LIST_QUERY,
		NULL, 
		0,
		lpBuffer,
		cbBuffer,
		pcbBytesReturned,
		NULL,
		NULL);

	if (iError != 0) {
		XTLTRACE2(NDASSVC_LPXCOMM, TRACE_LEVEL_ERROR,
			"SIO_ADDRESS_LIST_QUERY failed, error=0x%X\n", GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL GetLocalLpxAddressList(
	IN DWORD cbBuffer,
	OUT LPSOCKET_ADDRESS_LIST lpBuffer,
	OUT LPDWORD pcbBytesReturned)
{
	SOCKET sock = ::WSASocket(
		AF_LPX, 
		SOCK_STREAM, 
		LPXPROTO_TCP,
		NULL,
		0,
		0);

	if (INVALID_SOCKET == sock) 
	{
		XTLTRACE2(NDASSVC_LPXCOMM, TRACE_LEVEL_ERROR,
			"WSASocket() failed, error=0x%X\n", GetLastError());
		return FALSE;
	}

	BOOL fSuccess = GetLocalLpxAddressList(
		sock, 
		cbBuffer,
		lpBuffer, 
		pcbBytesReturned);

	//
	// Close socket may shadow last error
	//
	XTL_SAVE_LAST_ERROR();

	XTLVERIFY(SOCKET_ERROR != closesocket(sock));

	return fSuccess;
}

/*++

Description:

	make a LPX stream connection to the remote host
	as LPX is not routable, you have to explicitly specify
	the local address (interface) to use

Returns:

	If the function succeeds, the return value is nonzero.

	If the function fails, the return value is zero.
	To get extended error information, call WSAGetLastError.
	In this case, the value of lpSocketConnected is unspecified.
	
--*/

SOCKET CreateLpxConnection(
	IN const LPX_ADDRESS* pRemoteAddress, 
	IN const LPX_ADDRESS* pLocalAddress,
	IN USHORT usRemotePort)
{
	SOCKADDR_LPX saLocalSockAddr;
	SOCKADDR_LPX saRemoteSockAddr;

	//
	// create a socket
	// 
	SOCKET sock = socket(AF_LPX, SOCK_STREAM, LPXPROTO_TCP);
	if (INVALID_SOCKET == sock) 
	{
		XTLTRACE2(NDASSVC_LPXCOMM, TRACE_LEVEL_ERROR,
			"socket(LPX) failed, error=0x%X\n", GetLastError());

		return INVALID_SOCKET;
	}

	//
	// initialize local address struct
	//
	ZeroMemory(&saLocalSockAddr, sizeof(SOCKADDR_LPX));
	saLocalSockAddr.sin_family = AF_LPX;
	CopyMemory(saLocalSockAddr.LpxAddress.Node, pLocalAddress->Node, 6);

	//
	// bind a socket to the specified local address
	//
	int iResult = bind(
		sock,
		(struct sockaddr *) &saLocalSockAddr,
		sizeof(SOCKADDR_LPX));

	if (0 != iResult) 
	{
		XTL_SAVE_LAST_ERROR();

		XTLTRACE2(NDASSVC_LPXCOMM, TRACE_LEVEL_ERROR,
			"bind() failed, error=0x%X\n", GetLastError());

		XTLVERIFY(SOCKET_ERROR != closesocket(sock));

		return INVALID_SOCKET;
	}

	//
	// initialize remote address struct
	//
	ZeroMemory(&saRemoteSockAddr, sizeof(SOCKADDR_LPX));
	saRemoteSockAddr.sin_family = AF_LPX;
	CopyMemory(saRemoteSockAddr.LpxAddress.Node, pRemoteAddress->Node, 6);
	saRemoteSockAddr.LpxAddress.Port = htons(usRemotePort);

	//
	// connect to the remote host
	//
	iResult = ::connect(
		sock,
		(struct sockaddr *) &saRemoteSockAddr,
		sizeof(SOCKADDR_LPX));

	if (0 != iResult) 
	{
		XTL_SAVE_LAST_ERROR();
		XTLTRACE2(NDASSVC_LPXCOMM, TRACE_LEVEL_ERROR,
			"connect() failed, error=0x%X\n", GetLastError());
		XTLVERIFY(SOCKET_ERROR != closesocket(sock));
		return INVALID_SOCKET;
	}

	return sock;
}

/*++

Description:

	A utility function to create 

Returns:

	If the function succeeds, the return value is nonzero.

	If the function fails, the return value is zero.
	To get extended error information, call GetLastError.
	
--*/
BOOL CreateLpxAddressString(
	IN const PLPX_ADDRESS address,
	IN OUT LPTSTR lpAddress, 
	IN SIZE_T cchAddress)
{
	HRESULT hr = StringCchPrintf(lpAddress, cchAddress, 
		_T("%02X:%02X:%02X:%02X:%02X:%02X"),
		address->Node[0],
		address->Node[1],
		address->Node[2],
		address->Node[3],
		address->Node[4],
		address->Node[5]);
	return SUCCEEDED(hr);
}

BOOL IsEqualLpxAddress(
	const LPX_ADDRESS& lhs, 
	const LPX_ADDRESS& rhs)
{
	return
		lhs.Node[0] == rhs.Node[0] &&
		lhs.Node[1] == rhs.Node[1] &&
		lhs.Node[2] == rhs.Node[2] &&
		lhs.Node[3] == rhs.Node[3] &&
		lhs.Node[4] == rhs.Node[4] &&
		lhs.Node[5] == rhs.Node[5];		
}

BOOL IsEqualLpxAddress(
	const PLPX_ADDRESS lhs, 
	const PLPX_ADDRESS rhs)
{
	return
		lhs->Node[0] == rhs->Node[0] &&
		lhs->Node[1] == rhs->Node[1] &&
		lhs->Node[2] == rhs->Node[2] &&
		lhs->Node[3] == rhs->Node[3] &&
		lhs->Node[4] == rhs->Node[4] &&
		lhs->Node[5] == rhs->Node[5];		
}
