/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#include <winsock2.h>
#include <socketlpx.h>
#include <ndas/ndastype.h>
#include <xtl/xtltrace.h>

const USHORT NDAS_DEVICE_LPX_PORT = 10000;

BOOL GetLocalLpxAddressList(
	IN DWORD cbOutBuffer,
	OUT LPSOCKET_ADDRESS_LIST lpOutBuffer,
	OUT LPDWORD pcbBytesReturned);

BOOL GetLocalLpxAddressList(
	IN SOCKET s,
	IN DWORD cbOutBuffer,
	OUT LPSOCKET_ADDRESS_LIST lpOutBuffer,
	OUT LPDWORD pcbBytesReturned);

SOCKET
CreateLpxConnection(
	IN const LPX_ADDRESS* pRemoteAddress, 
	IN const LPX_ADDRESS* pLocalAddress,
	IN USHORT usRemotePort = NDAS_DEVICE_LPX_PORT);

BOOL IsEqualLpxAddress(
	const LPX_ADDRESS& lhs, 
	const LPX_ADDRESS& rhs);

BOOL IsEqualLpxAddress(
	const PLPX_ADDRESS lhs, 
	const PLPX_ADDRESS rhs);


static const SIZE_T LPXADDRESS_STRING_LENGTH = 18;

BOOL 
CreateLpxAddressString(
	IN const PLPX_ADDRESS pLpxAddress,
	IN OUT LPTSTR lpszAddress, 
	IN SIZE_T cchAddress);

// wrapper for LPX Address
class CLpxAddress
{
	TCHAR m_szBuffer[18];
	LPX_ADDRESS m_lpxAddress;

public:
	CLpxAddress(const LPX_ADDRESS& lpxAddress)
	{
		::ZeroMemory(&m_lpxAddress, sizeof(LPX_ADDRESS));
		::CopyMemory(&m_lpxAddress, &lpxAddress, sizeof(LPX_ADDRESS));
		m_szBuffer[0] = TEXT('\0');
	}

	CLpxAddress(const LPX_ADDRESS* pLpxAddress)
	{
		::ZeroMemory(&m_lpxAddress, sizeof(LPX_ADDRESS));
		::CopyMemory(&m_lpxAddress, pLpxAddress, sizeof(LPX_ADDRESS));
		m_szBuffer[0] = TEXT('\0');
	}

	operator const LPX_ADDRESS&()
	{
		return m_lpxAddress;
	}

	LPCTSTR ToString()
	{
		if (m_szBuffer[0] == TEXT('\0')) {
			// 00:00:00:00:00:00
			HRESULT hr = StringCchPrintf(m_szBuffer, 18, 
				_T("%02X:%02X:%02X:%02X:%02X:%02X"),
				m_lpxAddress.Node[0], m_lpxAddress.Node[1],
				m_lpxAddress.Node[2], m_lpxAddress.Node[3],
				m_lpxAddress.Node[4], m_lpxAddress.Node[5]);
			XTLASSERT(SUCCEEDED(hr));
		}
		return m_szBuffer;
	}

	operator LPCTSTR()
	{
		return ToString();
	}

	//
	// only compares the Node
	//
	BOOL IsEqualAddress(const LPX_ADDRESS& rhs)
	{
		return (m_lpxAddress.Node[0] == rhs.Node[0]) &&
			(m_lpxAddress.Node[1] == rhs.Node[1]) &&
			(m_lpxAddress.Node[2] == rhs.Node[2]) &&
			(m_lpxAddress.Node[3] == rhs.Node[3]) &&
			(m_lpxAddress.Node[4] == rhs.Node[4]) &&
			(m_lpxAddress.Node[5] == rhs.Node[5]);
	}

	BOOL IsEqualAddress(const CLpxAddress& rhs)
	{
		return IsEqualAddress(rhs.m_lpxAddress);
	}

	BOOL IsEqualAddress(const LPX_ADDRESS* rhs)
	{
		return IsEqualAddress(*rhs);
	}

};

//
// Utility Functions
//

inline
void
LpxAddressToSockAddrLpx(
	SOCKADDR_LPX* pLpxSockAddress,
	const LPX_ADDRESS* pLpxAddress)
{
	XTLASSERT(!::IsBadWritePtr(pLpxSockAddress, sizeof(SOCKADDR_LPX)));
	XTLASSERT(!::IsBadReadPtr(pLpxAddress, sizeof(LPX_ADDRESS)));

	::ZeroMemory(
		pLpxSockAddress,
		sizeof(SOCKADDR_LPX));

	::CopyMemory(
		&pLpxSockAddress->LpxAddress,
		pLpxAddress, 
		sizeof(LPX_ADDRESS));

	pLpxSockAddress->sin_family = AF_LPX;
	pLpxSockAddress->LpxAddress.Port = 0;
}

inline
void
NdasDeviceToSockAddrLpx(
	SOCKADDR_LPX* pLpxSockAddress, 
	const NDAS_DEVICE_ID* pDeviceId)
{
	XTLASSERT(!::IsBadWritePtr(pLpxSockAddress, sizeof(SOCKADDR_LPX)));
	XTLASSERT(!::IsBadReadPtr(pDeviceId, sizeof(NDAS_DEVICE_ID)));

	::ZeroMemory(
		pLpxSockAddress,
		sizeof(SOCKADDR_LPX));

	C_ASSERT(
		sizeof(pLpxSockAddress->LpxAddress.Node) == 
		sizeof(pDeviceId->Node));

	::CopyMemory(
		pLpxSockAddress->LpxAddress.Node, 
		pDeviceId->Node, 
		sizeof(pLpxSockAddress->LpxAddress.Node));

	pLpxSockAddress->sin_family = AF_LPX;
	pLpxSockAddress->LpxAddress.Port = 0;
}

inline 
void
LpxAddressToNdasDeviceId(
	NDAS_DEVICE_ID* pDeviceId,
	const LPX_ADDRESS* pLpxAddress)
{
	::ZeroMemory(pDeviceId, sizeof(NDAS_DEVICE_ID));
	C_ASSERT(sizeof(pDeviceId->Node) == sizeof(pLpxAddress->Node));
	::CopyMemory(pDeviceId->Node, pLpxAddress->Node, sizeof(pDeviceId->Node));
}

//
// Creates a SOCKET_ADDRESS_LIST with a single SOCKADDR_LPX entry
// in the process heap. Non-NULL returned pointer should be freed with
// HeapFree(GetProcessHeap(),...).
//
inline
LPSOCKET_ADDRESS_LIST
CreateLpxSocketAddressList(const SOCKADDR_LPX* pLpxSockAddress)
{
	SIZE_T RequiredSize = 
		sizeof(SOCKETLPX_ADDRESS_LIST) + 
		sizeof(SOCKADDR_LPX);

	LPVOID Buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, RequiredSize);
	if (NULL == Buffer)
	{
		SetLastError(ERROR_OUTOFMEMORY);
		return NULL;
	}

	LPSOCKET_ADDRESS_LIST pSockAddrList = reinterpret_cast<LPSOCKET_ADDRESS_LIST>(Buffer);
	SOCKADDR_LPX* pLpxSockAddrEntry = 
		reinterpret_cast<SOCKADDR_LPX*>(
			reinterpret_cast<LPBYTE>(pSockAddrList) + 
			sizeof(SOCKET_ADDRESS_LIST));
	pSockAddrList->iAddressCount = 1;
	pSockAddrList->Address[0].iSockaddrLength = sizeof(SOCKADDR_LPX);
	pSockAddrList->Address[0].lpSockaddr = reinterpret_cast<LPSOCKADDR>(pLpxSockAddrEntry);

	::CopyMemory(pLpxSockAddrEntry, pLpxSockAddress, sizeof(SOCKADDR_LPX));

	return pSockAddrList;
}

inline
LPSOCKET_ADDRESS_LIST
CreateLpxSocketAddressList(const LPX_ADDRESS* pLpxAddress)
{
	SOCKADDR_LPX saLpx;
	LpxAddressToSockAddrLpx(&saLpx, pLpxAddress);
	return CreateLpxSocketAddressList(&saLpx);
}

