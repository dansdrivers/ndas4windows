#include "stdafx.h"
#include <cfgmgr32.h>
#include <setupapi.h>
#include <cfg.h>
#include <xtl/xtlautores.h>

#include "trace.h"
#ifdef RUN_WPP
#include "misc.tmh"
#endif

struct AutoDeviceInfoSetConfig {
	static HDEVINFO GetInvalidValue() {
		return (HDEVINFO) INVALID_HANDLE_VALUE; 
	}
	static void Release(HDEVINFO h) {
		DWORD dwError = ::GetLastError();
		XTLVERIFY(::SetupDiDestroyDeviceInfoList(h));
		::SetLastError(dwError);
	}
};

typedef XTL::AutoResourceT<HDEVINFO,AutoDeviceInfoSetConfig> AutoDeviceInfoSet;

DWORD 
pGetDeviceUINumber(
	HDEVINFO DeviceInfoSet, 
	SP_DEVINFO_DATA* DeviceInfoData)
{
	DWORD uiNumber;
	BOOL fSuccess = ::SetupDiGetDeviceRegistryProperty(
		DeviceInfoSet,
		DeviceInfoData,
		SPDRP_UI_NUMBER,
		NULL,
		reinterpret_cast<PBYTE>(&uiNumber),
		sizeof(uiNumber),
		NULL);

	if (!fSuccess)
	{
		return 0;
	}

	return uiNumber;
}

BOOL 
pRequestEject(
	DWORD SlotNo,
	CONFIGRET* pConfigRet, 
	PPNP_VETO_TYPE pVetoType, 
	LPTSTR pszVetoName, 
	DWORD nNameLength)
{
	// Get devices under Enum\NDAS
	AutoDeviceInfoSet deviceInfoSet = ::SetupDiGetClassDevs(
		NULL,  
		_T("NDAS"),
		NULL, 
		DIGCF_ALLCLASSES | DIGCF_PRESENT);
	if (deviceInfoSet.IsInvalid())
	{
		return FALSE;
	}

	for (DWORD i = 0; ; ++i)
	{
		SP_DEVINFO_DATA deviceInfoData = {0};
		deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
		BOOL fSuccess = ::SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData);
		if (!fSuccess)
		{
			break;
		}
		DWORD uiNumber = pGetDeviceUINumber(deviceInfoSet, &deviceInfoData);
		if (SlotNo == uiNumber)
		{
			//*pConfigRet = ::CM_Query_And_Remove_SubTree(
			//	deviceInfoData.DevInst,
			//	pVetoType,
			//	pszVetoName,
			//	nNameLength,
			//	0);
			*pConfigRet = ::CM_Request_Device_Eject(
				deviceInfoData.DevInst,
				pVetoType,
				pszVetoName,
				nNameLength,
				0);
			return TRUE;
		}
	}
	return FALSE;
}

BOOL
pIsWindowsXPOrLater()
{
	// Initialize the OSVERSIONINFOEX structure.
	OSVERSIONINFOEX osvi = {0};
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	osvi.dwMajorVersion = 5;
	osvi.dwMinorVersion = 1;
	osvi.wServicePackMajor = 0;

	// Initialize the condition mask.
	DWORDLONG dwlConditionMask = 0;
	VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL );
	VER_SET_CONDITION( dwlConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL );
	VER_SET_CONDITION( dwlConditionMask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL );

	// Perform the test.
	return VerifyVersionInfo(&osvi, 
		VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR,
		dwlConditionMask);
}
