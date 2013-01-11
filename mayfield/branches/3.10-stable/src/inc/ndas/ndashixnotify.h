#pragma once
#include "ndastypeex.h"
#include "ndashix.h"

#ifdef __cplusplus
extern "C" {
#endif

BOOL
WINAPI
NdasHixNotifyUnitDeviceChange(CONST NDAS_UNITDEVICE_ID* pUnitDeviceId);

BOOL
WINAPI
NdasHixNotifyDeviceChange(CONST NDAS_DEVICE_ID* pDeviceId);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

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


#endif
