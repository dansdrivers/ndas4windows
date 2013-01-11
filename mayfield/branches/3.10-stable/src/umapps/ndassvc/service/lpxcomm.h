/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#include <winsock2.h>
#include <socketlpx.h>

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
			_ASSERTE(SUCCEEDED(hr));
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
