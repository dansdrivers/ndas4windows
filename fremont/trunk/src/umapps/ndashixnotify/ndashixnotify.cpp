#include "stdafx.h"
#include <objbase.h>
#include <ndas/ndashixnotify.h>
#include <algorithm>
#include <xtl/xtltrace.h>
#include "ndasdevid.h"
#include "ndashix.h"
#include "ndashixnotifyutil.h"

#include "lpxcs.h"
#include "ws2tcpip.h"

class CLpxDatagramMultiClient;

//
// Base class for HIX clients
//
class CNdasHIXClient
{
protected:
	GUID m_hostGuid;
	CNdasHIXClient(LPCGUID lpHostGuid = NULL);
};

class CNdasHIXChangeNotify :
	public CNdasHIXClient
{
	CLpxDatagramMultiClient m_bcaster;

public:
	CNdasHIXChangeNotify(LPCGUID lpHostGuid = NULL) :
	  CNdasHIXClient(lpHostGuid) 
	  {}

	  BOOL Initialize();
	  //
	  // Notification of Unit Device Change
	  //
	  BOOL Notify(CONST NDAS_UNITDEVICE_ID& unitDeviceId);
	  BOOL Notify(CONST NDAS_DEVICE_ID& deviceId);
};


extern "C" 
BOOL
WINAPI
NdasHixNotifyUnitDeviceChange(CONST NDAS_UNITDEVICE_ID* pUnitDeviceId)
{
	CNdasHIXChangeNotify notify;
	BOOL fSuccess = notify.Initialize();
	if (!fSuccess) {
		return FALSE;
	}

	fSuccess = notify.Notify(*pUnitDeviceId);
	if (!fSuccess) {
		return FALSE;
	}
	return TRUE;
}

extern "C" 
BOOL
WINAPI
NdasHixNotifyDeviceChange(CONST NDAS_DEVICE_ID* pDeviceId)
{
	CNdasHIXChangeNotify notify;
	BOOL fSuccess = notify.Initialize();
	if (!fSuccess) {
		return FALSE;
	}

	fSuccess = notify.Notify(*pDeviceId);
	if (!fSuccess) {
		return FALSE;
	}
	return TRUE;
}

CNdasHIXClient::CNdasHIXClient(LPCGUID lpHostGuid)
{
	if (NULL == lpHostGuid) {
		HRESULT hr = ::CoCreateGuid(&m_hostGuid);
		_ASSERTE(SUCCEEDED(hr));
	} else {
		m_hostGuid = *lpHostGuid;
	}
}

BOOL
CNdasHIXChangeNotify::Initialize()
{
	BOOL fSuccess = m_bcaster.Initialize();
	if (!fSuccess) {
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"CLpxDatagramMultiClient init failed, error=0x%X", GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL 
CNdasHIXChangeNotify::Notify(
	CONST NDAS_UNITDEVICE_ID& unitDeviceId)
{
	NDAS_HIX::UNITDEVICE_CHANGE::NOTIFY notify = {0};
	NDAS_HIX::UNITDEVICE_CHANGE::PNOTIFY pNotify = &notify;
	NDAS_HIX::PHEADER pHeader = &pNotify->Header;

	_ASSERTE(sizeof(pNotify->DeviceId == unitDeviceId.DeviceId.Node));
	::CopyMemory(
		pNotify->DeviceId,
		unitDeviceId.DeviceId.Node,
		sizeof(pNotify->DeviceId));
	pNotify->UnitNo = (UCHAR) unitDeviceId.UnitNo;

	DWORD cbLength = sizeof(NDAS_HIX::UNITDEVICE_CHANGE::NOTIFY);
	pBuildNHIXRequestHeader(pHeader, &m_hostGuid, NHIX_TYPE_UNITDEVICE_CHANGE, static_cast<USHORT>(cbLength));
	pNHIXHeaderToNetwork(pHeader);

	BOOL fSuccess = m_bcaster.Broadcast(
		NDAS_HIX_LISTEN_PORT, 
		cbLength, 
		(CONST BYTE*) pNotify);

	if (!fSuccess) {
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Broadcast failed, error=0x%X", GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL 
CNdasHIXChangeNotify::Notify(
	CONST NDAS_DEVICE_ID& deviceId)
{
	NDAS_HIX::DEVICE_CHANGE::NOTIFY notify = {0};
	NDAS_HIX::DEVICE_CHANGE::PNOTIFY pNotify = &notify;
	NDAS_HIX::PHEADER pHeader = &pNotify->Header;

	_ASSERTE(sizeof(pNotify->DeviceId) == sizeof(deviceId.Node));
	::CopyMemory(
		pNotify->DeviceId,
		deviceId.Node,
		sizeof(pNotify->DeviceId));

	DWORD cbLength = sizeof(NDAS_HIX::DEVICE_CHANGE::NOTIFY);
	pBuildNHIXRequestHeader(pHeader, &m_hostGuid, NHIX_TYPE_DEVICE_CHANGE, static_cast<USHORT>(cbLength));
	pNHIXHeaderToNetwork(pHeader);

	BOOL fSuccess = m_bcaster.Broadcast(
		NDAS_HIX_LISTEN_PORT, 
		cbLength, 
		(CONST BYTE*) pNotify);

	if (!fSuccess) {
		XTLTRACE1(TRACE_LEVEL_ERROR,
			"Broadcast failed, error=0x%X", GetLastError());
		return FALSE;
	}

	return TRUE;
}

