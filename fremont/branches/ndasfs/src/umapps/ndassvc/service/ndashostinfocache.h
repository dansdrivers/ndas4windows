#pragma once
#include "stdutils.h"
#include "ndas/ndashostinfo.h"
#include <socketlpx.h>
#include <map>

class CNdasHostInfoCache
{
	const DWORD m_dwExpireInterval;
	DWORD m_dwLastUpdate;

	BOOL Update(DWORD dwTimeout);

	VOID ClearHostInfoMap();

	typedef struct _HOST_INFO_CACHE_DATA {
		PNDAS_HOST_INFO pHostInfo;
		SOCKADDR_LPX boundAddr;
		SOCKADDR_LPX remoteAddr;
	} HOST_INFO_CACHE_DATA, *PHOST_INFO_CACHE_DATA;

	typedef std::map<GUID,HOST_INFO_CACHE_DATA,less_GUID > HostInfoMap;

	HostInfoMap m_hostInfoMap;

public:
	CNdasHostInfoCache(DWORD dwExpireInterval = 3000000);
	virtual ~CNdasHostInfoCache();
	CONST NDAS_HOST_INFO* GetHostInfo(LPCGUID lpHostGuid);
	BOOL GetHostNetworkInfo(
		LPCGUID lpHostGuid, 
		PSOCKADDR_LPX lpBoundAddr,
		PSOCKADDR_LPX lpRemoteAddr);
};
