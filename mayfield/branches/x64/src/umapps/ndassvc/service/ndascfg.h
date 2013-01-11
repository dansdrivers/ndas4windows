#pragma once
#include "ndassvc.h"
#include "confmgr.h"

#define CFG_EVENTPUB_PERIODIC_INTERVAL	_T("EventPeriodicInterval")
#define CFG_CMDPIPE_TIMEOUT _T("CmdPipeTimeout")
#define CFG_CMDPIPE_NAME	_T("CmdPipeName")
#define CFG_CMDPIPE_MAX_INSTANCE _T("CmdPipeMaxInstances")
#define CFG_MAX_COMM_FAILURE	_T("MaxCommFailureCount")
#define CFG_HEARTBEAT_TIMEOUT	_T("MaxHeartbeatTimeout")

class CRegistryCfgDefaultValue :
	public IRegistryCfgDefaultValue
{
protected:

	template<typename T>
	BOOL ReturnValue(
		T returnData, 
		LPVOID lpOutValue, DWORD cbOutValue, LPDWORD lpcbUsed)
	{
		return ReturnValue(&returnData, sizeof(T), lpOutValue, cbOutValue, lpcbUsed);
	}

	template<typename T>
	BOOL ReturnValue(
		T* pReturnData, DWORD cbReturnData, 
		LPVOID lpOutValue, DWORD cbOutValue, LPDWORD lpcbUsed)
	{
		if (lpcbUsed != NULL) {
			*lpcbUsed = sizeof(T);
		}
		if (lpOutValue == NULL) {
			return TRUE;
		}
		if (cbOutValue < cbReturnData) {
			::SetLastError(ERROR_INSUFFICIENT_BUFFER);
			return FALSE;
		}
		::CopyMemory(lpOutValue, pReturnData, cbReturnData);
		return TRUE;
	}

	template<>
	BOOL ReturnValue<const TCHAR>(
		const TCHAR* pReturnData, DWORD cbReturnData,
		LPVOID lpOutValue, DWORD cbOutValue, LPDWORD lpcbUsed)
	{
		DWORD cchActual;
		HRESULT hr = ::StringCchLength(pReturnData, STRSAFE_MAX_CCH, (size_t*)&cchActual);
		_ASSERTE(SUCCEEDED(hr));
		DWORD cbActual = (cchActual + 1) * sizeof(TCHAR);
		cbActual = max(cbActual, cbReturnData);
		return ReturnValue((LPBYTE) pReturnData, cbActual, lpOutValue, cbOutValue, lpcbUsed);
	}

};

class CNdasServiceCfgDefaultValue :
	public CRegistryCfgDefaultValue
{
public:
	virtual BOOL GetDefaultValue(
		LPCTSTR szContainer, LPCTSTR szValueName,
		LPVOID lpOutValue, DWORD cbOutValue, LPDWORD lpcbUsed);
};

class CNdasSystemCfgDefaultValue :
	public CRegistryCfgDefaultValue
{
public:
	virtual BOOL GetDefaultValue(
		LPCTSTR szContainer, LPCTSTR szValueName,
		LPVOID lpOutValue, DWORD cbOutValue, LPDWORD lpcbUsed);
};

class CNdasUserCfgDefaultValue :
	public CRegistryCfgDefaultValue
{
public:
	virtual BOOL GetDefaultValue(
		LPCTSTR szContainer, LPCTSTR szValueName,
		LPVOID lpOutValue, DWORD cbOutValue, LPDWORD lpcbUsed);
};

class CNdasServiceCfg : public CRegistryCfg {
public:
	CNdasServiceCfg(LPCTSTR szServiceName = NDAS_SERVICE_NAME);
	HKEY OpenRootKey();
};

class CNdasSystemCfg : public CRegistryCfg {
public:
	CNdasSystemCfg();
	HKEY OpenRootKey();
};

class CNdasUserCfg : public CRegistryCfg {
public:
	CNdasUserCfg();
	HKEY OpenRootKey();
};

extern CNdasServiceCfg _NdasServiceCfg;
extern CNdasSystemCfg _NdasSystemCfg;

namespace ndascfg
{
	namespace sys
	{
		struct MaxHeartbeatFailure { static DWORD GetValue(); };
		struct MaxUnitDeviceIdentifyFailure { static DWORD GetValue(); };
	}
}

