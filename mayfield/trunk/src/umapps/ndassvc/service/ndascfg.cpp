#include "stdafx.h"
#include "ndascfg.h"
#include "syncobj.h"
#include <ndas/ndassvcparam.h>
#include <regstr.h>

//////////////////////////////////////////////////////////////////////////
//
// Service Configuration Definitions
//
//////////////////////////////////////////////////////////////////////////

static const struct {
	LPCTSTR RegValueName;
	DWORD Default;
	DWORD Min;
	DWORD Max;
} NSCONFIG_DWORD_DEF[] = {
	_T("DebugStartupDelay"),          0, 0, 30000,         // nscDebugStartupDelay
	_T("AddTargetGap"),             500, 0, (DWORD)(-1),   // nscAddTargetGap
	_T("PlugInFailToEjectGap"),     500, 0, (DWORD)(-1),   // nscPlugInFailToEjectGap
	_T("MaxHeartbeatFailure"),       4,  1, (DWORD)(-1),   // nscHeartbeatFailLimit
	_T("MaxUnitDeviceIdentifyFailure"), 4, 1, (DWORD)(-1), // nscUnitDeviceIdentifyRetryMax
	_T("UnitDeviceIdentifyFailureRetryInterval"), 2500, 10, 300000, 
	                                                           // nscUnitDeviceIdentifyRetryGap
	_T("CmdPipeMaxInstances"),       10, 1, 10,            // nscCommandPipeInstancesMax
	_T("CmdPipeTimeout"),          5000, 1, (DWORD)(-1),   // nscCommandPipeTimeout
	_T("EventPeriodicInterval"),  60000, 1000, (DWORD)(-1),// nscPeriodicEventInterval
	_T("LogDevReconnect"),           19, 0, 19,            // nscLogicalDeviceReconnectRetryLimit
	_T("LogDevReconnectInterval"), 3000, 0, 60000,         // nscLogicalDeviceReconnectInterval
	_T("MaxWriteAccessCheck"),       10, 0, 60,            // nscWriteAccessCheckLimitOnDisconnect
	_T("LUROptions"),                 0, 0, (DWORD)(-1),   // nscLUROptions
	_T("Suspend"), NDASSVC_SUSPEND_DENY, 0, (DWORD)(-1),   // nscSuspendOptions
	_T("MaxHeartbeatInterval"),   15000, 0, (DWORD)(-1),   // nscMaximumHeartbeatInterval
	_T("WriteShareCheckTimeout"),  2000, 0, (DWORD)(-1), // nscWriteShareCheckTimeout
	NULL
};

static const struct {
	LPCTSTR RegValueName;
	BOOL Default;
} NSCONFIG_BOOL_DEF[] = {
	_T("DebugInitialBreak"),       FALSE, // nscInitialDebugBreak
	_T("OverrideLogDevReconnect"), FALSE, // nscOverrideReconnectOptions
	_T("DisconnectOnDebug"),       FALSE, // nscDisconnectOnDebug
	_T("DontSupressAlarms"),       FALSE, // nscDontSupressAlarms
	_T("NoPSWriteShare"),          FALSE, // nscDontUseWriteShare
	_T("NoForceSafeRemoval"),      FALSE, // nscDontForceSafeRemoval
	_T("MountOnReadyForEncryptedOnly"), FALSE, // nscMountOnReadyForEncryptedOnly
	_T("DisableRAIDWriteShare"),   FALSE, // nscDisableRAIDWriteShare
	NULL
};

static const struct {
	LPCTSTR RegValueName;
	LPCTSTR Default;
} NSCONFIG_STRING_DEF[] = {
	_T("CmdPipeName"), _T("\\\\.\\pipe\\ndas\\svccmd"), // nscCommandPipeName
	NULL
};

C_ASSERT(RTL_NUMBER_OF(NSCONFIG_DWORD_DEF) == nscEndOfDWORDType + 1);
C_ASSERT(RTL_NUMBER_OF(NSCONFIG_BOOL_DEF) == nscEndOfBOOLType + 1);
C_ASSERT(RTL_NUMBER_OF(NSCONFIG_STRING_DEF) == nscEndOfSTRINGType + 1);

//////////////////////////////////////////////////////////////////////////
//
// Implementations
//
//////////////////////////////////////////////////////////////////////////

static LPCTSTR NDAS_CFG_SUBKEY = TEXT("Software\\NDAS");

CNdasSystemCfg _NdasSystemCfg;

// {5D303CCE-0F53-4351-A881-D1CC828F47AE}
static const GUID SystemCfgGuid  = 
{ 0x5d303cce, 0xf53, 0x4351, { 0xa8, 0x81, 0xd1, 0xcc, 0x82, 0x8f, 0x47, 0xae } };

//// {6DE95DAB-63FF-49ec-B03F-DCBBD886186F}
//static const GUID ServiceCfgGuid = 
//{ 0x6de95dab, 0x63ff, 0x49ec, { 0xb0, 0x3f, 0xdc, 0xbb, 0xd8, 0x86, 0x18, 0x6f } };

CNdasSystemCfg::
CNdasSystemCfg() :
	CRegistryCfg(HKEY_LOCAL_MACHINE, NDAS_CFG_SUBKEY, NULL)
{
	BOOL fSuccess = SetEntropy((LPBYTE)&SystemCfgGuid, sizeof(GUID));
	_ASSERT(fSuccess);
}

//CNdasServiceCfg::
//CNdasServiceCfg(LPCTSTR szServiceName) :
//	CRegistryCfg(HKEY_LOCAL_MACHINE, NULL, NULL)
//{
//	TCHAR szBuffer[_MAX_PATH + 1];
//	HRESULT hr = ::StringCchPrintf(
//		szBuffer, _MAX_PATH + 1,
//		REGSTR_PATH_SERVICES TEXT("\\%s\\Parameters"),
//		szServiceName);
//	CRegistryCfg::SetCfgRegKey(szBuffer);
//	BOOL fSuccess = SetEntropy((LPBYTE)&ServiceCfgGuid, sizeof(GUID));
//	_ASSERT(fSuccess);
//}

template <typename T>
class MeyerSingleton
{
public:
	static T& Instance()
	{
		static T instance;
		return instance;
	}
};

class CNdasServiceConfigBase : 
	public ximeta::CReadWriteLock // We are using coarse-grained lock
{
protected:
	static LPCTSTR SubContainer;
};

LPCTSTR CNdasServiceConfigBase::SubContainer = _T("ndassvc");

class CNdasServiceDwordConfig : 
	public CNdasServiceConfigBase,
	public MeyerSingleton<CNdasServiceDwordConfig>
{
	static const SIZE_T DEF_SIZE = RTL_NUMBER_OF(NSCONFIG_DWORD_DEF) - 1;

	DWORD m_data[DEF_SIZE];
	bool m_cached[DEF_SIZE];

public:

	DWORD Get(NDASSVC_CONFIG_DWORD_TYPE Type);
	void Set(NDASSVC_CONFIG_DWORD_TYPE Type, DWORD Value);
};

class CNdasServiceBoolConfig : 
	public CNdasServiceConfigBase,
	public MeyerSingleton<CNdasServiceBoolConfig>
{
	static const SIZE_T DEF_SIZE = RTL_NUMBER_OF(NSCONFIG_BOOL_DEF) - 1;

	BOOL m_data[DEF_SIZE];
	bool m_cached[DEF_SIZE];

public:

	BOOL Get(NDASSVC_CONFIG_BOOL_TYPE Type);
	void Set(NDASSVC_CONFIG_BOOL_TYPE Type, BOOL Value);
};

class CNdasServiceStringConfig : 
	public CNdasServiceConfigBase,
	public MeyerSingleton<CNdasServiceStringConfig>
{
	static const SIZE_T DEF_SIZE = RTL_NUMBER_OF(NSCONFIG_STRING_DEF) - 1;
	static const SIZE_T MAX_STRING_VALUE_LEN = 256;

	TCHAR m_data[DEF_SIZE][MAX_STRING_VALUE_LEN];
	bool m_cached[DEF_SIZE];

public:

	void Get(NDASSVC_CONFIG_STRING_TYPE Type, LPTSTR Buffer, SIZE_T BufferChars);
	void Set(NDASSVC_CONFIG_STRING_TYPE Type, LPCTSTR Value);
};

DWORD
CNdasServiceDwordConfig::Get(
	NDASSVC_CONFIG_DWORD_TYPE Type)
{
	SIZE_T i = (SIZE_T) Type;
	XTLASSERT(i < DEF_SIZE);
	if (i >= DEF_SIZE) return 0;

	ReadLock();

	if (m_cached[i])
	{
		DWORD data = m_data[i];
		ReadUnlock();
		return data;
	}

	ReadUnlock();

	WriteLock();

	DWORD value;
	BOOL fSuccess = _NdasSystemCfg.GetValueEx(
		SubContainer,
		NSCONFIG_DWORD_DEF[i].RegValueName,
		&value);

	if (fSuccess && value >= NSCONFIG_DWORD_DEF[i].Min &&
		value <= NSCONFIG_DWORD_DEF[i].Max)
	{
		m_data[i] = value;
	}
	else
	{
		m_data[i] = NSCONFIG_DWORD_DEF[i].Default;
	}

	m_cached[i] = true;
	DWORD data = m_data[i];

	WriteUnlock();

	return data;
}

BOOL
CNdasServiceBoolConfig::Get(
	NDASSVC_CONFIG_BOOL_TYPE Type)
{
	SIZE_T i = (SIZE_T) Type;
	XTLASSERT(i < DEF_SIZE);
	if (i >= DEF_SIZE) return 0;

	ReadLock();

	if (m_cached[i])
	{
		BOOL data = m_data[i];
		ReadUnlock();
		return data;
	}

	ReadUnlock();

	WriteLock();

	BOOL value;
	BOOL fSuccess = _NdasSystemCfg.GetValueEx(
		SubContainer,
		NSCONFIG_BOOL_DEF[i].RegValueName,
		&value);

	if (fSuccess)
	{
		m_data[i] = value ? TRUE : FALSE;
	}
	else
	{
		m_data[i] = NSCONFIG_BOOL_DEF[i].Default;
	}

	m_cached[i] = true;
	BOOL data = m_data[i];

	WriteUnlock();

	return data;
}

void
CNdasServiceStringConfig::Get(
	NDASSVC_CONFIG_STRING_TYPE Type,
	LPTSTR Buffer,
	SIZE_T BufferChars)
{
	SIZE_T i = (SIZE_T) Type;
	XTLASSERT(i < DEF_SIZE);
	if (i >= DEF_SIZE) return;

	ReadLock();

	if (m_cached[i])
	{
		HRESULT hr = ::StringCchCopyN(
			Buffer, BufferChars, 
			m_data[i], MAX_STRING_VALUE_LEN);
		XTLASSERT(SUCCEEDED(hr));
		ReadUnlock();
		return;
	}

	ReadUnlock();

	WriteLock();

	TCHAR value[MAX_STRING_VALUE_LEN];
	BOOL fSuccess = _NdasSystemCfg.GetValueEx(
		SubContainer,
		NSCONFIG_STRING_DEF[i].RegValueName,
		value, 
		sizeof(value));

	if (fSuccess)
	{
		HRESULT hr = ::StringCchCopy(
			m_data[i],
			MAX_STRING_VALUE_LEN,
			value);
		XTLASSERT(SUCCEEDED(hr));
	}
	else
	{
		HRESULT hr = ::StringCchCopy(
			m_data[i],
			MAX_STRING_VALUE_LEN,
			NSCONFIG_STRING_DEF[i].Default);
		XTLASSERT(SUCCEEDED(hr));
	}

	m_cached[i] = true;

	HRESULT hr = ::StringCchCopyN(
		Buffer, BufferChars, 
		m_data[i], MAX_STRING_VALUE_LEN);
	XTLASSERT(SUCCEEDED(hr));

	WriteUnlock();

	return;
}

void
CNdasServiceDwordConfig::Set(
	NDASSVC_CONFIG_DWORD_TYPE Type, 
	DWORD Value)
{
	SIZE_T i = (SIZE_T) Type;
	XTLASSERT(i < DEF_SIZE);
	if (i >= DEF_SIZE) return;

	WriteLock();

	LPCTSTR RegValueName = NSCONFIG_DWORD_DEF[i].RegValueName;
	BOOL fSuccess = _NdasSystemCfg.SetValueEx(
		SubContainer, 
		RegValueName, 
		Value);
	XTLASSERT(fSuccess);

	m_data[i] = Value;
	if (!m_cached[i])
	{
		m_cached[i] = true;
	}

	WriteUnlock();

	return;
}

void
CNdasServiceBoolConfig::Set(
	NDASSVC_CONFIG_BOOL_TYPE Type, 
	BOOL Value)
{
	SIZE_T i = (SIZE_T) Type;
	XTLASSERT(i < DEF_SIZE);
	if (i >= DEF_SIZE) return;

	WriteLock();

	LPCTSTR RegValueName = NSCONFIG_BOOL_DEF[i].RegValueName;
	BOOL fSuccess = _NdasSystemCfg.SetValueEx(
		SubContainer, 
		RegValueName, 
		Value);
	XTLASSERT(fSuccess);

	m_data[i] = Value;
	if (!m_cached[i])
	{
		m_cached[i] = true;
	}

	WriteUnlock();

	return;
}

void
CNdasServiceStringConfig::Set(
	NDASSVC_CONFIG_STRING_TYPE Type, 
	LPCTSTR Value)
{
	SIZE_T i = (SIZE_T) Type;
	XTLASSERT(i < DEF_SIZE);
	if (i >= DEF_SIZE) return;

	WriteLock();

	LPCTSTR RegValueName = NSCONFIG_STRING_DEF[i].RegValueName;
	BOOL fSuccess = _NdasSystemCfg.SetValueEx(
		SubContainer,
		RegValueName, 
		Value);
	XTLASSERT(fSuccess);

	HRESULT hr = ::StringCchCopyN(
		m_data[i], MAX_STRING_VALUE_LEN, 
		Value, MAX_STRING_VALUE_LEN);
	if (!m_cached[i])
	{
		m_cached[i] = true;
	}

	WriteUnlock();

	return;
}

void
NdasServiceConfig::Get(
	NDASSVC_CONFIG_STRING_TYPE Type,
	LPTSTR Buffer,
	SIZE_T BufferChars)
{
	CNdasServiceStringConfig::Instance().Get(Type, Buffer, BufferChars);
}

DWORD
NdasServiceConfig::Get(NDASSVC_CONFIG_DWORD_TYPE Type)
{
	return CNdasServiceDwordConfig::Instance().Get(Type);
}

BOOL
NdasServiceConfig::Get(NDASSVC_CONFIG_BOOL_TYPE Type)
{
	return CNdasServiceBoolConfig::Instance().Get(Type);
}

void 
NdasServiceConfig::Set(NDASSVC_CONFIG_DWORD_TYPE Type, DWORD Value)
{
	CNdasServiceDwordConfig::Instance().Set(Type, Value);
}

void 
NdasServiceConfig::Set(NDASSVC_CONFIG_BOOL_TYPE Type, BOOL Value)
{
	CNdasServiceBoolConfig::Instance().Set(Type, Value);
}

void 
NdasServiceConfig::Set(NDASSVC_CONFIG_STRING_TYPE Type, LPCTSTR Value)
{
	CNdasServiceStringConfig::Instance().Set(Type, Value);
}

//
// Not Implemented yet
//

typedef enum _NDASSYS_CONFIG_BOOL
{
} NDASSYS_CONFIG_BOOL;

typedef enum _NDASSYS_CONFIG_STRING
{
} NDASSYS_CONFIG_STRING;

typedef enum _NDASSYS_CONFIG_DWORD
{
} NDASSYS_CONFIG_DWORD;

struct NdasSystemConfig
{
	DWORD   Get(LPCTSTR Container, NDASSYS_CONFIG_DWORD Type);
	BOOL    Get(LPCTSTR Container, NDASSYS_CONFIG_BOOL Type);
	LPCTSTR Get(LPCTSTR Container, NDASSYS_CONFIG_STRING Type);

	DWORD   GetSecure(LPCTSTR Container, NDASSYS_CONFIG_DWORD Type);
	BOOL    GetSecure(LPCTSTR Container, NDASSYS_CONFIG_BOOL Type);
	LPCTSTR GetSecure(LPCTSTR Container, NDASSYS_CONFIG_STRING Type);

	void Set(LPCTSTR Container, NDASSYS_CONFIG_DWORD Type, DWORD Value, BOOL Volatile);
	void Set(LPCTSTR Container, NDASSYS_CONFIG_BOOL Type, BOOL Value, BOOL Volatile);
	void Set(LPCTSTR Container, NDASSYS_CONFIG_STRING Type, LPCTSTR Value, BOOL Volatile);
};
