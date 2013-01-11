#pragma once
#include "ndassvc.h"
#include "confmgr.h"

class CNdasSystemCfg : public CRegistryCfg {
public:
	CNdasSystemCfg();
	HKEY OpenRootKey();
};

extern CNdasSystemCfg _NdasSystemCfg;

typedef enum _NDASSVC_CONFIG_DWORD_TYPE {

	nscDebugStartupDelay,
	nscAddTargetGap,
	nscPlugInFailToEjectGap,
	nscHeartbeatFailLimit,
	nscUnitDeviceIdentifyRetryMax,
	nscUnitDeviceIdentifyRetryGap,
	nscCommandPipeInstancesMax,
	nscCommandPipeTimeout,
	nscPeriodicEventInterval,
	nscLogicalDeviceReconnectRetryLimit,
	nscLogicalDeviceReconnectInterval,
	nscWriteAccessCheckLimitOnDisconnect,
	nscLUROptions,
	nscSuspendOptions,
	nscMaximumHeartbeatInterval,
	nscWriteShareCheckTimeout,
	nscAutoPnp,
	nscEndOfDWORDType

} NDASSVC_CONFIG_DWORD_TYPE;

typedef enum _NDASSVC_CONFIG_BOOL_TYPE {
	nscDebugInitialBreak,
	nscOverrideReconnectOptions,
	nscDisconnectOnDebug,
	nscDontSupressAlarms,
	nscDontUseWriteShare,
	nscDontForceSafeRemoval,
	nscMountOnReadyForEncryptedOnly,
	nscDisableRAIDWriteShare,
	nscEndOfBOOLType
} NDASSVC_CONFIG_BOOL_TYPE;

typedef enum _NDASSVC_CONFIG_STRING_TYPE {
	nscCommandPipeName,

	nscEndOfSTRINGType
} NDASSVC_CONFIG_STRING_TYPE;

struct NdasServiceConfig
{
	static DWORD Get(NDASSVC_CONFIG_DWORD_TYPE Type);
	static BOOL  Get(NDASSVC_CONFIG_BOOL_TYPE Type);
	static void  Get(NDASSVC_CONFIG_STRING_TYPE Type, LPTSTR Buffer, SIZE_T BufferChars);
	
	static void Set(NDASSVC_CONFIG_DWORD_TYPE Type, DWORD Value);
	static void Set(NDASSVC_CONFIG_BOOL_TYPE Type, BOOL Value);
	static void Set(NDASSVC_CONFIG_STRING_TYPE Type, LPCTSTR Value);
};
