/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#ifndef _NDASDEVID_H_
#define _NDASDEVID_H_

#include <ndas/ndastypeex.h>
#include <xtl/xtltrace.h>
#define C_ASSERT_EQUALSIZE(x,y) C_ASSERT(sizeof(x) == sizeof(y))

//////////////////////////////////////////////////////////////////////////
//
// NDAS_DEVICE_ID struct
//
//////////////////////////////////////////////////////////////////////////

//
// wrapper for NDAS_DEVICE_ID structure
//
class CNdasDeviceId
{
	static const size_t CCH_BUFFER = 18;
	static const size_t CB_BUFFER = CCH_BUFFER * sizeof(TCHAR);

	TCHAR _szBuffer[CCH_BUFFER];

public:
	NDAS_DEVICE_ID DeviceId;

	CNdasDeviceId(const NDAS_DEVICE_ID& DeviceId) : 
		DeviceId(DeviceId)
	{
		_szBuffer[0] = _T('\0');
	}

	CNdasDeviceId(const CNdasDeviceId& cDeviceId) :
		DeviceId(cDeviceId.DeviceId)
	{
		::CopyMemory(_szBuffer, cDeviceId._szBuffer, CB_BUFFER);
	}

	CNdasDeviceId(UCHAR n0, UCHAR n1, UCHAR n2, UCHAR n3, UCHAR n4, UCHAR n5)
	{
		DeviceId.Node[0] = n0; DeviceId.Node[1] = n1; DeviceId.Node[2] = n2;
		DeviceId.Node[3] = n3; DeviceId.Node[4] = n4; DeviceId.Node[5] = n5;
	}

	CNdasDeviceId(const BYTE* BytePtr)
	{
		XTLASSERT(!IsBadReadPtr(BytePtr, sizeof(DeviceId.Node)));
		::CopyMemory(DeviceId.Node, BytePtr, sizeof(DeviceId.Node));
	}

	CNdasDeviceId(const LPX_ADDRESS& LpxAddress)
	{
		::ZeroMemory(&DeviceId, sizeof(NDAS_DEVICE_ID));
		::CopyMemory(
			DeviceId.Node, 
			LpxAddress.Node, 
			sizeof(FIELD_OFFSET(NDAS_DEVICE_ID,Node)));
		C_ASSERT_EQUALSIZE(
			FIELD_OFFSET(LPX_ADDRESS,Node),
			FIELD_OFFSET(NDAS_DEVICE_ID,Node));
	}

	LPCTSTR ToString()
	{
		HRESULT hr = ::StringCchPrintf(
			_szBuffer, CCH_BUFFER,
			TEXT("%02X%02X%02X:%02X%02X%02X"),
			DeviceId.Node[0], DeviceId.Node[1], DeviceId.Node[2],
			DeviceId.Node[3], DeviceId.Node[4], DeviceId.Node[5]);
		_ASSERT(SUCCEEDED(hr));
		return _szBuffer;
	}

	LPCWSTR ToStringW()
	{
		LPWSTR lpBuffer = reinterpret_cast<LPWSTR>(_szBuffer);
		HRESULT hr = ::StringCchPrintfW(
			lpBuffer, CCH_BUFFER,
			L"%02X%02X%02X:%02X%02X%02X",
			DeviceId.Node[0], DeviceId.Node[1], DeviceId.Node[2],
			DeviceId.Node[3], DeviceId.Node[4], DeviceId.Node[5]);
		_ASSERT(SUCCEEDED(hr));
		return lpBuffer;
	}

	LPCSTR ToStringA()
	{
		LPSTR lpBuffer = reinterpret_cast<LPSTR>(_szBuffer);
		HRESULT hr = ::StringCchPrintfA(
			lpBuffer, CCH_BUFFER,
			"%02X%02X%02X:%02X%02X%02X",
			DeviceId.Node[0], DeviceId.Node[1], DeviceId.Node[2],
			DeviceId.Node[3], DeviceId.Node[4], DeviceId.Node[5]);
		_ASSERT(SUCCEEDED(hr));
		return lpBuffer;
	}

	operator LPCSTR() { return ToStringA(); }
	operator LPCWSTR() { return ToStringW(); }

	operator const NDAS_DEVICE_ID& () 
	{
		return DeviceId;
	}
};

class CNdasUnitDeviceId
{
	static const size_t CCH_BUFFER = 20;
	static const size_t CB_BUFFER = CCH_BUFFER * sizeof(TCHAR);

	TCHAR _szBuffer[CCH_BUFFER];

public:

	NDAS_UNITDEVICE_ID UnitDeviceId;

	CNdasUnitDeviceId(const NDAS_UNITDEVICE_ID& unitDeviceId) :
		UnitDeviceId(unitDeviceId)
	{
	}

	CNdasUnitDeviceId(const CNdasUnitDeviceId& cUnitDeviceId) :
		UnitDeviceId(cUnitDeviceId.UnitDeviceId)
	{
		::CopyMemory(_szBuffer, cUnitDeviceId._szBuffer, CB_BUFFER);
	}

	CNdasUnitDeviceId(const NDAS_DEVICE_ID& deviceId, DWORD UnitNo)
	{
		UnitDeviceId.DeviceId = deviceId;
		UnitDeviceId.UnitNo = UnitNo;
	}

	operator const NDAS_UNITDEVICE_ID& ()
	{
		return UnitDeviceId;
	}

	LPCTSTR ToString()
	{
		// 000DB0:00AA01:02
		HRESULT hr = ::StringCchPrintf(
			_szBuffer, 
			CCH_BUFFER, 
			TEXT("%02X%02X%02X:%02X%02X%02X:%02X"),
			UnitDeviceId.DeviceId.Node[0], UnitDeviceId.DeviceId.Node[1], 
			UnitDeviceId.DeviceId.Node[2], UnitDeviceId.DeviceId.Node[3], 
			UnitDeviceId.DeviceId.Node[4], UnitDeviceId.DeviceId.Node[5],
			UnitDeviceId.UnitNo);
		_ASSERT(SUCCEEDED(hr));
		return _szBuffer;
	}

	LPCSTR ToStringA()
	{
		LPSTR lpBuffer = reinterpret_cast<LPSTR>(_szBuffer);
		HRESULT hr = ::StringCchPrintfA(
			lpBuffer, 
			CCH_BUFFER, 
			"%02X%02X%02X:%02X%02X%02X:%02X",
			UnitDeviceId.DeviceId.Node[0], UnitDeviceId.DeviceId.Node[1], 
			UnitDeviceId.DeviceId.Node[2], UnitDeviceId.DeviceId.Node[3], 
			UnitDeviceId.DeviceId.Node[4], UnitDeviceId.DeviceId.Node[5],
			UnitDeviceId.UnitNo);
		_ASSERT(SUCCEEDED(hr));
		return lpBuffer;
	}

	LPCWSTR ToStringW()
	{
		LPWSTR lpBuffer = reinterpret_cast<LPWSTR>(_szBuffer);
		HRESULT hr = ::StringCchPrintfW(
			lpBuffer, 
			CCH_BUFFER, 
			L"%02X%02X%02X:%02X%02X%02X:%02X",
			UnitDeviceId.DeviceId.Node[0], UnitDeviceId.DeviceId.Node[1], 
			UnitDeviceId.DeviceId.Node[2], UnitDeviceId.DeviceId.Node[3], 
			UnitDeviceId.DeviceId.Node[4], UnitDeviceId.DeviceId.Node[5],
			UnitDeviceId.UnitNo);
		_ASSERT(SUCCEEDED(hr));
		return lpBuffer;
	}

	operator LPCSTR()
	{
		return ToStringA();
	}

	operator LPCWSTR()
	{
		return ToStringW();
	}
};

inline bool operator ==(const NDAS_DEVICE_ID& lhs, const NDAS_DEVICE_ID& rhs)
{
	return ((lhs.Node[0] == rhs.Node[0]) && 
		(lhs.Node[1] == rhs.Node[1]) &&
		(lhs.Node[2] == rhs.Node[2]) && 
		(lhs.Node[3] == rhs.Node[3]) &&
		(lhs.Node[4] == rhs.Node[4]) && 
		(lhs.Node[5] == rhs.Node[5]));
}

inline bool operator !=(const NDAS_DEVICE_ID& lhs, const NDAS_DEVICE_ID& rhs)
{
	return !(lhs == rhs);
}

template<>
struct std::less <NDAS_DEVICE_ID> : 
	public std::binary_function <NDAS_DEVICE_ID, NDAS_DEVICE_ID, bool> 
{
	bool operator()(const NDAS_DEVICE_ID& lhs, const NDAS_DEVICE_ID& rhs) const
	{
		for (DWORD i = 0; i < 6; ++i) {
			if (lhs.Node[i] < rhs.Node[i]) return true;	// less
			if (lhs.Node[i] > rhs.Node[i]) return false; // greater
		}
		return false; // equal
	}
};

inline bool operator ==(const NDAS_UNITDEVICE_ID& lhs, const NDAS_UNITDEVICE_ID& rhs)
{
	if (lhs.DeviceId == rhs.DeviceId) {
		return lhs.UnitNo == rhs.UnitNo;
	} else {
		return false;
	}
};

inline bool operator !=(const NDAS_UNITDEVICE_ID& lhs, const NDAS_UNITDEVICE_ID& rhs)
{
	return !(lhs == rhs);
};

template <>
struct std::less<NDAS_UNITDEVICE_ID> :
	public std::binary_function<NDAS_UNITDEVICE_ID, NDAS_UNITDEVICE_ID, bool>
{
#ifndef NDAS_DEVICE_ID_COMPARE_DEBUG
	bool operator()(const NDAS_UNITDEVICE_ID& lhs, const NDAS_UNITDEVICE_ID& rhs) const
	{
		if (std::less<NDAS_DEVICE_ID>()(lhs.DeviceId, rhs.DeviceId)) {
			return true;
		} else {
			if (lhs.DeviceId == rhs.DeviceId) {
				if (lhs.UnitNo < rhs.UnitNo) { return true;	} 
				else { return false; }
			}
			return false;
		}
	}
#else
	bool operator()(const NDAS_UNITDEVICE_ID& lhs, const NDAS_UNITDEVICE_ID& rhs) const
	{
		static TCHAR buf[255] = {0};
		CNdasUnitDeviceId x(lhs), y(rhs);
		if (std::less<NDAS_DEVICE_ID>()(lhs.DeviceId, rhs.DeviceId)) {
			StringCchPrintf(buf, 255, _T("CMP: %s <  %s\n"), x.ToString(), y.ToString());
			OutputDebugString(buf);
			return true;
		} else {
			if (lhs.DeviceId == rhs.DeviceId) {
				if (lhs.UnitNo < rhs.UnitNo) {
					StringCchPrintf(buf, 255, _T("CMP: %s <  %s\n"), x.ToString(), y.ToString());
					OutputDebugString(buf);
					return true;
				} else {
					StringCchPrintf(buf, 255, _T("CMP: %s >= %s\n"), x.ToString(), y.ToString());
					OutputDebugString(buf);
					return false;
				}
			}
			StringCchPrintf(buf, 255, _T("CMP: %s >= %s\n"), x.ToString(), y.ToString());
			OutputDebugString(buf);
			return false;
		}
	}
#endif
};

template<>
struct std::less <NDAS_LOGICALDEVICE_GROUP> :
	public std::binary_function<
		NDAS_LOGICALDEVICE_GROUP, 
		NDAS_LOGICALDEVICE_GROUP, 
		bool>
{
	bool operator()(
		const NDAS_LOGICALDEVICE_GROUP& lhs, 
		const NDAS_LOGICALDEVICE_GROUP& rhs) const
	{
		int res;
		if (lhs.Type != rhs.Type) {
			return lhs.Type < rhs.Type;
		}

		if (lhs.nUnitDevices != rhs.nUnitDevices) {
			return lhs.nUnitDevices < rhs.nUnitDevices;
		}
#if 0
		res = ::memcmp(&lhs.RaidSetId, &rhs.RaidSetId, sizeof(GUID));
		if (res)
			return res < 0;
		res = ::memcmp(&lhs.ConfigSetId, &rhs.ConfigSetId, sizeof(GUID));
		if (res)
			return res < 0;
#endif
		return ::memcmp(
			lhs.UnitDevices, 
			rhs.UnitDevices, 
			sizeof(NDAS_UNITDEVICE_ID) * lhs.nUnitDevices) < 0;
	}
};

//////////////////////////////////////////////////////////////////////////

#endif // _NDASDEVID_H_

