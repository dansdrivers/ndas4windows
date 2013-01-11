#include "stdafx.h"
#include <objbase.h>
#include "ndasdevid.h"
#include "ndashix.h"
#include "ndashixnotify.h"
#include "ndashixnotifyutil.h"
#include <algorithm>
#include "xdebug.h"

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
		DBGPRT_ERR_EX(_FT("CLpxDatagramMultiClient init failed: "));
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
	pBuildNHIXRequestHeader(pHeader, &m_hostGuid, NHIX_TYPE_UNITDEVICE_CHANGE, cbLength);
	pNHIXHeaderToNetwork(pHeader);

	BOOL fSuccess = m_bcaster.Broadcast(
		NDAS_HIX_LISTEN_PORT, 
		cbLength, 
		(CONST BYTE*) pNotify);

	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Broadcast failed: "));
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
	pBuildNHIXRequestHeader(pHeader, &m_hostGuid, NHIX_TYPE_DEVICE_CHANGE, cbLength);
	pNHIXHeaderToNetwork(pHeader);

	BOOL fSuccess = m_bcaster.Broadcast(
		NDAS_HIX_LISTEN_PORT, 
		cbLength, 
		(CONST BYTE*) pNotify);

	if (!fSuccess) {
		DBGPRT_ERR_EX(_FT("Broadcast failed: "));
		return FALSE;
	}

	return TRUE;
}

