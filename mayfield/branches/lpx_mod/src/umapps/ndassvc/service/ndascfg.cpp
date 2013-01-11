#include "stdafx.h"
#include "ndascfg.h"
#include <regstr.h>

static LPCTSTR NDAS_CFG_SUBKEY = TEXT("Software\\NDAS");

CNdasServiceCfg _NdasServiceCfg;
CNdasSystemCfg _NdasSystemCfg;

static CNdasServiceCfgDefaultValue _NdasServiceCfgDefaultValue;
static CNdasSystemCfgDefaultValue _NdasSystemCfgDefaultValue;

enum DEFAULT_VALUE_TYPE {
	DF_STRING,
	DF_DWORD,
	DF_BOOL
};

struct DEFAULT_VALUE {
	LPCTSTR Name;
	DWORD Type;
	union {
        DWORD DwordValue;
		BOOL BoolValue;
	};
	LPCTSTR StringValue;
};

typedef DEFAULT_VALUE* PDEFAULT_VALUE;

#define DWORD_VAL(v) DF_DWORD, (v), NULL
#define STRING_VAL(s) DF_STRING, 0, _T(s)
#define BOOL_VAL(v) DF_BOOL, (v), NULL
#define DEFINE_NDAS_CFG(name, val) {name, val},
#define BEGIN_NDAS_CFG_DEF(var) static const DEFAULT_VALUE var[] = {
#define END_NDAS_CFG_DEF() {NULL, DF_DWORD, 0, NULL} };

BEGIN_NDAS_CFG_DEF(ServiceCfgDefaultValues)
	DEFINE_NDAS_CFG(CFG_HEARTBEAT_TIMEOUT,	DWORD_VAL(15 * 1000))
	DEFINE_NDAS_CFG(CFG_MAX_COMM_FAILURE,	DWORD_VAL(3))
	DEFINE_NDAS_CFG(CFG_CMDPIPE_NAME,			STRING_VAL("\\\\.\\pipe\\ndas\\svccmd"))
	DEFINE_NDAS_CFG(CFG_CMDPIPE_MAX_INSTANCE,	DWORD_VAL(10))
	DEFINE_NDAS_CFG(CFG_CMDPIPE_TIMEOUT,		DWORD_VAL(5 * 1000))
	DEFINE_NDAS_CFG(CFG_EVENTPUB_PERIODIC_INTERVAL, DWORD_VAL(60 * 1000))
END_NDAS_CFG_DEF()

#undef DEFINE_NDAS_CFG
#undef BEGIN_NDAS_CFG_DEF
#undef END_NDAS_CFG_DEF
#undef DWORD_VAL
#undef STRING_VAL
#undef BOOL_VAL


BOOL 
CNdasServiceCfgDefaultValue::
GetDefaultValue(
	LPCTSTR szContainer, LPCTSTR szValueName,
	LPVOID lpOutValue, DWORD cbOutValue, LPDWORD lpcbUsed)
{
	for (const DEFAULT_VALUE* pv = &ServiceCfgDefaultValues[0]; pv->Name != NULL; ++pv) {
		if (lstrcmpi(szValueName, pv->Name) == 0) {
			switch (pv->Type) {
			case DF_DWORD:
				return ReturnValue(pv->DwordValue, lpOutValue, cbOutValue, lpcbUsed);
			case DF_BOOL:
				return ReturnValue(pv->BoolValue, lpOutValue, cbOutValue, lpcbUsed);
			case DF_STRING:
				return ReturnValue(pv->StringValue, 0, lpOutValue, cbOutValue, lpcbUsed);
			default:
				_ASSERT(FALSE && "Illegal definition in Default Values");
			}
		}
	}

	return FALSE;
}

BOOL 
CNdasSystemCfgDefaultValue::
GetDefaultValue(
	LPCTSTR szContainer, LPCTSTR szValueName,
	LPVOID lpOutValue, DWORD cbOutValue, LPDWORD lpcbUsed)
{
	return FALSE;
}

BOOL
CNdasUserCfgDefaultValue::
GetDefaultValue(
	LPCTSTR szContainer, LPCTSTR szValueName,
	LPVOID lpOutValue, DWORD cbOutValue, LPDWORD lpcbUsed)
{
	return FALSE;
}

// {5D303CCE-0F53-4351-A881-D1CC828F47AE}
static const GUID SystemCfgGuid  = 
{ 0x5d303cce, 0xf53, 0x4351, { 0xa8, 0x81, 0xd1, 0xcc, 0x82, 0x8f, 0x47, 0xae } };

// {6DE95DAB-63FF-49ec-B03F-DCBBD886186F}
static const GUID ServiceCfgGuid = 
{ 0x6de95dab, 0x63ff, 0x49ec, { 0xb0, 0x3f, 0xdc, 0xbb, 0xd8, 0x86, 0x18, 0x6f } };

CNdasSystemCfg::
CNdasSystemCfg() :
	CRegistryCfg(HKEY_LOCAL_MACHINE, NDAS_CFG_SUBKEY, &_NdasSystemCfgDefaultValue)
{
	BOOL fSuccess = SetEntropy((LPBYTE)&SystemCfgGuid, sizeof(GUID));
	_ASSERT(fSuccess);
}

CNdasServiceCfg::
CNdasServiceCfg(LPCTSTR szServiceName) :
	CRegistryCfg(HKEY_LOCAL_MACHINE, NULL, &_NdasServiceCfgDefaultValue)
{
	TCHAR szBuffer[_MAX_PATH + 1];
	HRESULT hr = ::StringCchPrintf(
		szBuffer, _MAX_PATH + 1,
		REGSTR_PATH_SERVICES TEXT("\\%s\\Parameters"),
		szServiceName);
	CRegistryCfg::SetCfgRegKey(szBuffer);
	BOOL fSuccess = SetEntropy((LPBYTE)&ServiceCfgGuid, sizeof(GUID));
	_ASSERT(fSuccess);
}

CNdasUserCfg::
CNdasUserCfg() :
	CRegistryCfg(HKEY_CURRENT_USER, NDAS_CFG_SUBKEY, &CNdasUserCfgDefaultValue())
{
}
