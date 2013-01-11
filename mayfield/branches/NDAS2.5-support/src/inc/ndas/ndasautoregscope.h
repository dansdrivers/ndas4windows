#pragma once
#include <windows.h>
#include <vector>
#include "ndas/ndastypeex.h"

#include <pshpack4.h>
typedef struct _NDAS_AUTOREG_SCOPE {
	BYTE rBegin[8]; // 6 + 2 (reserved)
	BYTE rEnd[8];	// 6 + 2 (reserved)
	ACCESS_MASK grantedAccess;
	BYTE reserved[4];
} NDAS_AUTOREG_SCOPE, *PNDAS_AUTOREG_SCOPE; //  24 bytes per entry
#include <poppack.h>

class CNdasAutoRegScopeData {

public:

	CNdasAutoRegScopeData();
	~CNdasAutoRegScopeData();

	BOOL AddScope(
		CONST NDAS_DEVICE_ID& rBegin, 
		CONST NDAS_DEVICE_ID& rEnd, 
		ACCESS_MASK access);

	DWORD GetCount();
	BOOL RemoveScope(DWORD dwIndex);
	BOOL GetScope(
		DWORD dwIndex, 
		NDAS_DEVICE_ID& rBegin,
		NDAS_DEVICE_ID& rEnd,
		ACCESS_MASK& access);

	BOOL SaveToSystem();
	BOOL LoadFromSystem();
	BOOL LoadFromFile(LPCTSTR lpFileName);

	ACCESS_MASK GetAutoRegAccess(CONST NDAS_DEVICE_ID& deviceID);

protected:

	std::vector<NDAS_AUTOREG_SCOPE> m_scopes;
};

