/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#include <socketlpx.h>

const USHORT NDAS_DEVICE_LPX_PORT = 10000;

BOOL IsEqualLpxAddress(
	const LPX_ADDRESS& lhs, 
	const LPX_ADDRESS& rhs);

BOOL IsEqualLpxAddress(
	const PLPX_ADDRESS lhs, 
	const PLPX_ADDRESS rhs);

static const SIZE_T LPXADDRESS_STRING_LENGTH = 18;

// wrapper for LPX Address
class CLpxAddress
{
	CHAR m_szBuffer[18];
	LPX_ADDRESS m_lpxAddress;

public:
	CLpxAddress(const LPX_ADDRESS& lpxAddress)
	{
		::ZeroMemory(&m_lpxAddress, sizeof(LPX_ADDRESS));
		::CopyMemory(&m_lpxAddress, &lpxAddress, sizeof(LPX_ADDRESS));
		m_szBuffer[0] = '\0';
	}

	CLpxAddress(const LPX_ADDRESS* pLpxAddress)
	{
		::ZeroMemory(&m_lpxAddress, sizeof(LPX_ADDRESS));
		::CopyMemory(&m_lpxAddress, pLpxAddress, sizeof(LPX_ADDRESS));
		m_szBuffer[0] = '\0';
	}

	operator const LPX_ADDRESS&()
	{
		return m_lpxAddress;
	}

	LPCSTR ToStringA()
	{
		if (m_szBuffer[0] == '\0') {
			// 00:00:00:00:00:00
			HRESULT hr = StringCchPrintfA(
				m_szBuffer, 18, 
				"%02X:%02X:%02X:%02X:%02X:%02X",
				m_lpxAddress.Node[0], m_lpxAddress.Node[1],
				m_lpxAddress.Node[2], m_lpxAddress.Node[3],
				m_lpxAddress.Node[4], m_lpxAddress.Node[5]);
			COMASSERT(SUCCEEDED(hr));
		}
		return m_szBuffer;
	}

	operator LPCSTR()
	{
		return ToStringA();
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

/*++

Convert an LPX address to TA LSTransport Address.

++*/

typedef struct _TA_ADDRESS_NDAS *PTA_NDAS_ADDRESS;

VOID
LpxCommConvertLpxAddressToTaLsTransAddress(
	__in CONST LPX_ADDRESS *	LpxAddress,
	__out PTA_NDAS_ADDRESS		TaLsTransAddress);

