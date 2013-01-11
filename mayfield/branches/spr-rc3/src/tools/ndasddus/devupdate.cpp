#include "stdafx.h"
#include "devupdate.h"
#include <cfgmgr32.h>
#include <regstr.h>
#include <setupapi.h>
#include <devguid.h>
#define STRSAFE_NO_DEPRECATE
#include <strsafe.h>
#include <xdbgprn.h>

static
LPCTSTR
pGetNextDeviceID(LPCTSTR lpBuffer);

static
BOOL
pInstallDeviceDriver(
	HDEVINFO hDevInfoSet, 
	LPCTSTR devInstID, 
	LPCTSTR szDriverPath);

BOOL
__stdcall
NdasDiInstallDeviceDriver(
	CONST GUID* classGUID, 
	LPCTSTR devInstID, 
	LPCTSTR szDriverPath)
{
	HDEVINFO hDevInfoSet = ::SetupDiCreateDeviceInfoList(classGUID, NULL);
	
	if (INVALID_HANDLE_VALUE == hDevInfoSet)
	{
		DebugPrintLastErr(_T("SetupDiGetClassDevs failed: "));
		return FALSE;
	}

	BOOL fSuccess = pInstallDeviceDriver(hDevInfoSet, devInstID, szDriverPath);

	(VOID) ::SetupDiDestroyDeviceInfoList(hDevInfoSet);

	return fSuccess;
}


#define MAX_KEY_LENGTH 255

BOOL
pNdasDiEnumChildKeys(
	HKEY hParentKey,
	LPCTSTR szParentDevID,
	NDASDIENUMPROC enumProc,
	LPVOID lpContext,
	LPBOOL lpbContinue);

BOOL
pNdasDiCheckEachKey(
	HKEY hKey,
	LPCTSTR szDeviceID,
	NDASDIENUMPROC enumProc,
	LPVOID lpContext,
	LPBOOL lpbContinue)
{
	DebugPrint(1, _T("Device: %s\n"), szDeviceID);

	// If ClassGUID Value exists, it is an instance key
	// otherwise, it's a device key, to enum its child recursively

	LONG lResult = ::RegQueryValueEx(hKey, _T("ClassGUID"), 0, NULL, NULL, NULL);

	if (ERROR_SUCCESS != lResult)
	{
		return pNdasDiEnumChildKeys(hKey, szDeviceID, enumProc, lpContext, lpbContinue);
	}

	DebugPrint(1, _T("Device Instance: %s\n"), szDeviceID);

	DWORD dwConfigFlags = 0;
	ULONG ulConfigFlagsLen = sizeof(dwConfigFlags);

	lResult = ::RegQueryValueEx(
		hKey, _T("ConfigFlags"), NULL, 
		NULL, (BYTE*) &dwConfigFlags, &ulConfigFlagsLen);

	if (CONFIGFLAG_FINISH_INSTALL & dwConfigFlags)
	{
		*lpbContinue = enumProc(szDeviceID, lpContext);
	}
	return TRUE;
}

BOOL
pNdasDiEnumChildKeys(
	HKEY hParentKey,
	LPCTSTR szParentDevID,
	NDASDIENUMPROC enumProc,
	LPVOID lpContext,
	LPBOOL lpbContinue)
{
	BOOL fSuccess;
	LONG lResult;
	HRESULT hr;

	DebugPrint(1, _T("Enumerating: %s\n"), szParentDevID);

	for (DWORD i = 0; ; ++i)
	{
		TCHAR szSubKey[MAX_KEY_LENGTH];
		DWORD cchSubKey = MAX_KEY_LENGTH;
		FILETIME ftLastWritten;
		
		lResult = ::RegEnumKeyEx(
			hParentKey, i,
			szSubKey, &cchSubKey, 
			0, 0, 0, &ftLastWritten);

		if (ERROR_SUCCESS != lResult) break;

		// Device Instance should contain Class, ClassGUID, etc.

		HKEY hSubKey;
		lResult = ::RegOpenKeyEx(
			hParentKey, szSubKey, 0, 
			KEY_READ | KEY_ENUMERATE_SUB_KEYS, &hSubKey);

		if (ERROR_SUCCESS != lResult) continue;

		TCHAR szDeviceID[MAX_DEVICE_ID_LEN];
		hr = ::StringCchPrintf(szDeviceID, MAX_DEVICE_ID_LEN,
			_T("%s\\%s"), szParentDevID, szSubKey);

		_ASSERTE(SUCCEEDED(hr));
		if (FAILED(hr))
		{
			return FALSE;
		}

		fSuccess = pNdasDiCheckEachKey(
			hSubKey, szDeviceID, enumProc, lpContext, lpbContinue);

		::RegCloseKey(hSubKey);

		if (!*lpbContinue) return TRUE;
	}

	return (ERROR_NO_MORE_ITEMS == lResult);
}

BOOL
pNdasDiEnumUnconfiguredDevicesUsingRegistry(
	LPCTSTR szFilter, 
	NDASDIENUMPROC enumProc, 
	LPVOID lpContext)
{
	HKEY hKey;
	TCHAR szKey[MAX_PATH];

	HRESULT hr = ::StringCchPrintf(
		szKey, MAX_PATH, 
		_T("System\\CurrentControlSet\\Enum%s%s"), 
		szFilter ? _T("\\") : _T(""), 
		szFilter ? szFilter : _T(""));

	_ASSERTE(SUCCEEDED(hr));
	if (FAILED(hr))
	{
		::SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	ULONG lResult = ::RegOpenKeyEx(
		HKEY_LOCAL_MACHINE, szKey, 
		0, KEY_READ | KEY_ENUMERATE_SUB_KEYS , &hKey);

	if (ERROR_SUCCESS != lResult) 
	{
		::SetLastError(lResult);
		return FALSE;
	}

	BOOL fCont = TRUE;
	BOOL fSuccess = pNdasDiCheckEachKey(
		hKey, 
		szFilter ? szFilter : _T(""), 
		enumProc, 
		lpContext,
		&fCont);

	::RegCloseKey(hKey);
	return fSuccess;
}

BOOL
__stdcall
NdasDiEnumUnconfigedDevices(
	LPCTSTR szFilter, 
	NDASDIENUMPROC enumProc, 
	LPVOID lpContext)
{

#define ENUM_WITH_REGISTRY
#ifdef ENUM_WITH_REGISTRY
	return pNdasDiEnumUnconfiguredDevicesUsingRegistry(szFilter, enumProc, lpContext);
#else

	ULONG ulBufferSize = 0;
	CONFIGRET cfgRet = ::CM_Get_Device_ID_List_Size(
		&ulBufferSize,
		szFilter,
		CM_GETIDLIST_FILTER_ENUMERATOR);

	if (CR_SUCCESS != cfgRet)
	{
		DebugPrintErr(cfgRet, _T("CM_Get_Device_ID_List_Size failed: "));
		return FALSE;
	}

	TCHAR* lpBuffer = (TCHAR*) ::LocalAlloc(GPTR, ulBufferSize);
	if (NULL == lpBuffer)
	{
		DebugPrint(1, _T("Unable to alloc %d bytes.\n"), ulBufferSize);
		return FALSE;
	}

	cfgRet = ::CM_Get_Device_ID_List(
		szFilter,
		lpBuffer,
		ulBufferSize,
		CM_GETIDLIST_FILTER_ENUMERATOR);

	if (CR_SUCCESS != cfgRet)
	{
		DebugPrintErr(cfgRet,_T("CM_Get_Device_ID_List failed: "));
		::LocalFree(lpBuffer);
		return FALSE;
	}

	//
	// iterate each device
	//
	for (LPCTSTR lpCur = lpBuffer; *lpCur; lpCur = pGetNextDeviceID(lpCur))
	{
		DEVINST dnDevInst; // INVALID_HANDLE_VALUE;
		DEVINSTID dnDevInstID = const_cast<DEVINSTID>(lpCur);
		//
		// include phantom devices
		//
		CONFIGRET cfgRet = ::CM_Locate_DevNode(
			&dnDevInst,
			dnDevInstID,
			CM_LOCATE_DEVNODE_PHANTOM); 
		if (CR_SUCCESS != cfgRet)
		{
			DebugPrintErr(cfgRet, _T("CM_Locate_DevNode(%s) failed: "), lpCur);
			continue;
		}

		DWORD dwConfigFlags = 0;
		ULONG ulConfigFlagsLen = sizeof(dwConfigFlags);
		cfgRet = ::CM_Get_DevNode_Registry_Property(
			dnDevInst,
			CM_DRP_CONFIGFLAGS,
			NULL,
			&dwConfigFlags,
			&ulConfigFlagsLen,
			0);

		if (CR_SUCCESS != cfgRet)
		{
			DebugPrintErr(cfgRet, _T("CM_Get_DevNode_Registry_Property(%s) failed: "), lpCur);
			continue;
		}

		DebugPrint(1, _T("%s: %s\n"), lpCur,
			(CONFIGFLAG_FINISH_INSTALL & dwConfigFlags) ? 
			_T("To install") : _T("Complete"));

		if (CONFIGFLAG_FINISH_INSTALL & dwConfigFlags)
		{
			BOOL fCont = enumProc(lpCur, lpContext);
			if (!fCont) break;
		}
	}

	return TRUE;

#endif

}

LPCTSTR 
pGetNextDeviceID(LPCTSTR lpBuffer)
{
	LPCTSTR ptr = lpBuffer;
	while (*ptr) ++ptr;
	return (ptr + 1);
}

BOOL
pInstallDeviceDriver(
	HDEVINFO hDevInfoSet, 
	LPCTSTR devInstID, 
	LPCTSTR szDriverPath)
{
	SP_DEVINFO_DATA devInfoData = {0};
	devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	BOOL fSuccess = ::SetupDiOpenDeviceInfo(
		hDevInfoSet,
		devInstID,
		NULL,
		0,
		&devInfoData);
	if (!fSuccess)
	{
		DebugPrintLastErr(_T("SetupDiOpenDeviceInfo failed: "));
		return FALSE;
	}

	SP_DRVINFO_DATA drvInfoData = {0};
	drvInfoData.cbSize = sizeof(drvInfoData);

	SP_DEVINSTALL_PARAMS devInstParams = {0};
	devInstParams.cbSize = sizeof(devInstParams);

	fSuccess = ::SetupDiGetDeviceInstallParams(
		hDevInfoSet, 
		&devInfoData, 
		&devInstParams);
	if (!fSuccess)
	{
		DebugPrintLastErr(_T("SetupDiGetDeviceInstallParams failed: "));
		return FALSE;
	}

	devInstParams.Flags |= ( DI_ENUMSINGLEINF | DI_QUIETINSTALL );
	::lstrcpy(devInstParams.DriverPath, szDriverPath);

	fSuccess = ::SetupDiSetDeviceInstallParams(
		hDevInfoSet, 
		&devInfoData, 
		&devInstParams);
	if (!fSuccess)
	{
		DebugPrintLastErr(_T("SetupDiSetDeviceInstallParams failed: "));
		return FALSE;
	}

	fSuccess = ::SetupDiBuildDriverInfoList(
		hDevInfoSet, 
		&devInfoData, 
		SPDIT_COMPATDRIVER);
	if (!fSuccess)
	{
		DebugPrintLastErr(_T("SetupDiBuildDriverInfoList failed: "));
		return FALSE;
	}

	fSuccess = ::SetupDiCallClassInstaller(
		DIF_SELECTBESTCOMPATDRV,
		hDevInfoSet,
		&devInfoData);
	if (!fSuccess)
	{
		DebugPrintLastErr(_T("SetupDiCallClassInstaller DIF_SELECTBESTCOMPATDRV failed: "));
		return FALSE;
	}

	fSuccess = ::SetupDiCallClassInstaller(
		DIF_INSTALLDEVICE,
		hDevInfoSet,
		&devInfoData);
	if (!fSuccess)
	{
		DebugPrintLastErr(_T("SetupDiCallClassInstaller DIF_INSTALLDEVICE failed: "));
		return FALSE;
	}

	DebugPrint(1, _T("pInstallDeviceDriver done successfully.\n"));

	return TRUE;
}
