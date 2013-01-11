#include "stdafx.h"
#include "autores.h"

#include <netcfgx.h>
#include <setupapi.h>
#include "netcomp.h"
#include "pnpdevinst.h"
#include "svcinst.h"
#include "shiconcache.h"
#include "fstrbuf.h"
#include "misc.h"
#include "msilog.h"

#define XDBG_FILENAME "ndmsica.cpp"
#include "xdebug.h"


//
// Custom actions can return the following values:
//
// - ERROR_FUNCTION_NOT_CALLED
// - ERROR_SUCCESS
// - ERROR_INSTALL_USEREXIT
// - ERROR_INSTALL_FAILURE
// - ERROR_NO_MORE_ITEMS
//

static BOOL WINAPI
pFindAndDeleteDriverFile(LPCTSTR szDriverFileName, LPBOOL lpbReboot);

static BOOL WINAPI
pFindAndDeleteFile(LPCTSTR szFilePath, LPBOOL lpbReboot);

static BOOL WINAPI
pFileExists(LPCTSTR szFilePath);

static BOOL WINAPI
pFindAndDeleteService(LPCTSTR szServiceName, LPBOOL lpbReboot);

static CONST DWORD FLG_MSI_SCHEDULE_PREBOOT = 0x0001;
static CONST DWORD FLG_MSI_SCHEDULE_REBOOT = 0x0002;

struct AutoFindFileHandleConfig
{
	static HANDLE GetInvalidValue() throw()
	{ 
		return (HANDLE)INVALID_HANDLE_VALUE; 
	}
	static void Release(HANDLE h) throw()
	{ 
		DWORD dwError = ::GetLastError();
		BOOL fSuccess = ::FindClose(h);
		_ASSERTE(fSuccess);
		::SetLastError(dwError);
	}
};

typedef AutoResourceT<HANDLE,AutoFindFileHandleConfig> AutoFindFileHandle;

//
// CustomActionData: 
//
// <ierrordialog>|<netclass>|<compid>|<inf_path>|<regkey>|<regvalue>
//
// ierrordialog: 25001
// netclass: protocol
// compid: nkc_lpx
// inf_path: c:\program files\ximeta\netdisk\drivers\netlpx.inf
// regkey: Software\NDAS\Setup\InstallPath
// regvalue: LPXINF <- oemXXX.inf
//
// e.g. 25001|protocol|nkc_lpx|...
//	   c:\program files\ximeta\netdisk\drivers\netlpx.inf|...
//     Software\NDAS\Setup\InstallPath|LPXINF
//        
// Deferred Custom Action
//

UINT 
__stdcall 
NDMsiInstallNetComponent(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);
	pMsiIncrementProgressBar(hInstall, 10);

	BOOL fSuccess = FALSE;
	UINT uiReturn = 0;

	//
	// Parse Custom Action Data
	//

	INT  iErrorDialog = 0;
	LPCTSTR pszNetClass = NULL;
	LPCTSTR pszCompId = NULL;
	LPCTSTR pszInfPath = NULL;
	LPCTSTR pszRegPath = NULL;
	LPCTSTR pszRegValue = NULL;

	CADParser parser;
	parser.AddToken<INT>(iErrorDialog);
	parser.AddToken<LPCTSTR>(pszNetClass);
	parser.AddToken<LPCTSTR>(pszCompId);
	parser.AddToken<LPCTSTR>(pszInfPath);
	parser.AddToken<LPCTSTR>(pszRegPath);
	parser.AddToken<LPCTSTR>(pszRegValue);
	DWORD nTokens = parser.Parse(hInstall);

	if (parser.GetTokenCount() != nTokens) 
	{
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	NetClass nc = pGetNetClass(pszNetClass);
	if (NC_Unknown == nc) 
	{
		MSILOGERR(FSB256(_T("Invalid Net Class: %s"), pszNetClass));
		return ERROR_INSTALL_FAILURE;
	}

	if (_T('\0') == pszCompId[0]) 
	{
		MSILOGERR(_T("Component ID not specified"));
		return ERROR_INSTALL_FAILURE;
	}

	if (_T('\0') == pszInfPath[0]) 
	{
		pszInfPath = NULL;
	}

	MSILOGINFO(
		FSB256(_T("Net Class: %s, Component Id: %s, InfPath: %s, RegPath: %s, RegValue %s"),
		pszNetClass, pszCompId, pszInfPath, pszRegPath, pszRegValue));

	HRESULT hr;
	TCHAR szInstalledInf[MAX_PATH];

	while (TRUE)
	{

		hr = HrInstallNetComponent(
			pszCompId,
			nc,
			pszInfPath,
			szInstalledInf,
			MAX_PATH,
			NULL,
			NULL);

		if (FAILED(hr)) 
		{

			MSILOGERR_PROC2(_T("HrInstallNetComponent"), hr);

			if (0 == iErrorDialog) 
			{
				return ERROR_INSTALL_FAILURE;
			}

			UINT uiResponse = pMsiErrorMessageBox(hInstall, iErrorDialog, hr);
			// 0, IDABORT, IDRETRY, IDIGNORE
			if (IDABORT == uiResponse) 
			{
				return ERROR_INSTALL_FAILURE;
			}
			else if (IDRETRY == uiResponse) 
			{
				continue;
			}
			else if (IDIGNORE == uiResponse) 
			{
			}
		}

		break;
	}

	//
	// The followings actions are not critical ones
	// Just log errors if any and return ERROR_SUCCESS
	//

	if (NETCFG_S_REBOOT == hr) 
	{
		pMsiScheduleRebootForDeferredCA(hInstall);
	}

	//
	// set the registry value
	//
	fSuccess = pSetInstalledPath(
		hInstall,
		HKEY_LOCAL_MACHINE, 
		pszRegPath, 
		pszRegValue, 
		szInstalledInf);

	if (!fSuccess) 
	{
		MSILOGERR_PROC(_T("pSetInstalledPath"));
	}

	return ERROR_SUCCESS;

}

//
// CustomActionData: 
// <ierrordialog>|<inf_path>|<regpath>|<regvalue>
//
//        

UINT 
__stdcall
NDMsiCopyOEMInf(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);
	pMsiIncrementProgressBar(hInstall, 10);

	UINT uiReturn = 0;

	INT  iErrorDialog = 0;
	LPCTSTR pszInfPath = NULL;
	LPCTSTR pszRegPath = NULL;
	LPCTSTR pszRegValue = NULL;

	CADParser parser;
	parser.AddToken<INT>(iErrorDialog);
	parser.AddToken<LPCTSTR>(pszInfPath);
	parser.AddToken<LPCTSTR>(pszRegPath);
	parser.AddToken<LPCTSTR>(pszRegValue);
	DWORD nTokens = parser.Parse(hInstall);

	if (parser.GetTokenCount() != nTokens) 
	{
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	TCHAR szInstalledInf[MAX_PATH] = {0};

	BOOL fSuccess = FALSE;

	while (TRUE) 
	{

		fSuccess = NdasDiCopyOEMInf(
			pszInfPath, 
			0, 
			szInstalledInf, 
			MAX_PATH, 
			NULL, 
			NULL);

		//
		// Actual error for Error 87 The parameter is incorrect is
		// 0x800B0100 No signature was present in the subject. 
		// 

		if (87 == ::GetLastError())
		{
			::SetLastError(0x800B0100);
		}

		if (!fSuccess) 
		{

			MSILOGERR_PROC(_T("NdasDiCopyOEMInf"));

			if (0 == iErrorDialog) 
			{
				return ERROR_INSTALL_FAILURE;
			}

			UINT uiResponse = pMsiErrorMessageBox(hInstall, iErrorDialog);
			if (IDABORT == uiResponse) 
			{
				return ERROR_INSTALL_FAILURE;
			} 
			else if (IDRETRY == uiResponse) 
			{
				continue;
			} 
			else if (IDIGNORE == uiResponse) 
			{
			}
		}

		break;
	}

	//
	// The following actions are non-critical ones
	//

	if (fSuccess) 
	{

		MSILOGINFO(FSB256(_T("INF Copied %s to %s"), pszInfPath, szInstalledInf));

		//
		// set the registry value
		//
		fSuccess = pSetInstalledPath(
			hInstall,
			HKEY_LOCAL_MACHINE, 
			pszRegPath, 
			pszRegValue, 
			szInstalledInf);

		if (!fSuccess) 
		{
			MSILOGERR_PROC(_T("pSetInstalledPath"));
		}

	}

	return ERROR_SUCCESS;
}

//
// CustomActionData: 
// <ierrordialog>|<hardware_id>|<inf_path>|<regpath>|<regvalue>
//
//        

UINT 
__stdcall 
NDMsiInstallPnpDevice(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);
	pMsiIncrementProgressBar(hInstall, 10);

	UINT uiReturn = 0;

	INT  iErrorDlg = 0;
	LPCTSTR pszHardwareID = NULL;
	LPCTSTR pszInfPath = NULL;
	LPCTSTR pszRegPath = NULL;
	LPCTSTR pszRegValue = NULL;

	CADParser parser;
	parser.AddToken<INT>(iErrorDlg);
	parser.AddToken<LPCTSTR>(pszHardwareID);
	parser.AddToken<LPCTSTR>(pszInfPath);
	parser.AddToken<LPCTSTR>(pszRegPath);
	parser.AddToken<LPCTSTR>(pszRegValue);
	DWORD nTokens = parser.Parse(hInstall);

	if (parser.GetTokenCount() != nTokens) 
	{
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	//
	// We do not update the existing device
	// It's MSI's responsibility to ensure that
	// any existing devices are already removed.
	//

	TCHAR szInstalledInf[MAX_PATH] = {0};
	BOOL bRebootRequired = FALSE;
	BOOL fSuccess = FALSE;

	while (TRUE) 
	{
		//
		// Specifying INSTALLFLAG_FORCE will overwrite the
		// existing device driver with the current one
		// irrespective of existence of higher version
		//
		fSuccess = NdasDiInstallRootEnumeratedDevice(
			NULL,
			pszInfPath,
			pszHardwareID,
			INSTALLFLAG_FORCE,
			&bRebootRequired);

		if (!fSuccess) 
		{

			MSILOGERR_PROC(_T("NdasDiInstallRootEnumeratedDevice"));

			UINT uiResponse = pMsiErrorMessageBox(hInstall, iErrorDlg);

			// 0, IDABORT, IDRETRY, IDIGNORE
			if (IDABORT == uiResponse) 
			{
				return ERROR_INSTALL_FAILURE;
			} 
			else if (IDRETRY == uiResponse) 
			{
				continue;
			}
			else if (IDIGNORE == uiResponse) 
			{
			}
		}

		break;
	}

	//
	// The following actions are non-critical ones
	//

	if (fSuccess) 
	{
		//
		// set the registry value
		//
		if (_T('\0') != szInstalledInf[0]) 
		{
			fSuccess = pSetInstalledPath(
				hInstall,
				HKEY_LOCAL_MACHINE, 
				pszRegPath, 
				pszRegValue, 
				szInstalledInf);

			if (!fSuccess) 
			{
				MSILOGERR_PROC(_T("pSetInstalledPath"));
			}
		}
	}

	return ERROR_SUCCESS;
}

//
// CustomActionData: 
// <ierrordialog>|<source_binary_path>|
// <service_name>|<display_name>|<description>|
// <service_type>|<start_type>|<error_control>|
// <load_order_group>|<dependencies>(semicolon delimited and should be terminated with ;)
//
// Service Type Value
//
// SERVICE_KERNEL_DRIVER 1
// SERVICE_FILE_SYSTEM_DRIVER 2
// (do not use the followings)
// SERVICE_WIN32_OWN_PROCESS 10
// SERVICE_WIN32_SHARE_PROCESS 20
// SERVICE_INTERACTIVE_PROCESS 100
//
// Start Type Values
//
// SERVICE_BOOT_START 0
// SERVICE_SYSTEM_START 1
// SERVICE_AUTO_START 2
// SERVICE_DEMAND_START 3
// SERVICE_DISABLED 4
//
// Error Control Values
//
// SERVICE_ERROR_IGNORE 0
// SERVICE_ERROR_NORMAL 1
// SERVICE_ERROR_SEVERE 2
// SERVICE_ERROR_CRITICAL 3
//

UINT 
__stdcall 
NDMsiInstallLegacyDriver(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);
	pMsiIncrementProgressBar(hInstall, 10);

	// CustomActionData: 
	// <ierrordialog>|<source_binary_path>|
	// <service_name>|<display_name>|<description>|
	// <service_type>|<start_type>|<error_control>|
	// <load_order_group>|<dependencies>; (semicolon delimited and should be terminated with ;)

	INT iErrorDlg = 0;
	LPCTSTR pszBinaryPath = NULL;
	LPCTSTR pszServiceName = NULL;
	LPCTSTR pszDisplayName = NULL;
	LPCTSTR pszDescription = NULL;
	DWORD dwServiceType = 0;
	DWORD dwStartType = 0;
	DWORD dwErrorControl = 0;
	LPCTSTR pszLoadOrderGroup = NULL;
	LPCTSTR pszDependency = NULL;

	CADParser parser;
	parser.AddToken<INT>(iErrorDlg);
	parser.AddToken<LPCTSTR>(pszBinaryPath);
	parser.AddToken<LPCTSTR>(pszServiceName);
	parser.AddToken<LPCTSTR>(pszDisplayName);
	parser.AddToken<LPCTSTR>(pszDescription);
	parser.AddToken<DWORD>(dwServiceType);
	parser.AddToken<DWORD>(dwStartType);
	parser.AddToken<DWORD>(dwErrorControl);
	parser.AddToken<LPCTSTR>(pszLoadOrderGroup);
	parser.AddToken<LPCTSTR>(pszDependency);
	DWORD nTokens = parser.Parse(hInstall);

	if (parser.GetTokenCount() != nTokens) 
	{
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	BOOL fSuccess = FALSE;

	MSILOGINFO(FSB1024(
		_FT("iErrorDlg=%d,BinaryPath=%s,ServiceName=%s,DisplayName=%s")
		_T(",Description=%s,ServiceType=%08X,StartType=%08X,ErrorControl=%08X")
		_T(",LoadOrderGroup=%s,Dependency=%s\n"),
		iErrorDlg,
		pszBinaryPath,
		pszServiceName,
		pszDisplayName,
		pszDescription,
		dwServiceType,
		dwStartType,
		dwErrorControl,
		pszLoadOrderGroup,
		pszDependency));

	if (SERVICE_KERNEL_DRIVER != dwServiceType &&
		SERVICE_FILE_SYSTEM_DRIVER != dwServiceType) 
	{
		MSILOGERR(FSB256(_T("Invalid ServiceType %d"), dwServiceType));
		return ERROR_INSTALL_FAILURE;
	}

	TCHAR szDependencies[250] = {0};
	// substitute ; with NULL for dependency
	{
		HRESULT hr = ::StringCchCopy(szDependencies, 250, pszDependency);
		_ASSERTE(SUCCEEDED(hr));
		PTCHAR p = szDependencies;
		for (PTCHAR p = szDependencies; *p != _T('\0'); ++p) 
		{
			if (_T(';') == *p) 
			{
				*p = _T('\0');
			}
		}
	}

	BOOL bUpdated = FALSE;

	while (TRUE) 
	{

		fSuccess = NdasDiInstallOrUpdateDriverService(
			pszBinaryPath,
			pszServiceName,
			pszDisplayName,
			pszDescription,
			dwServiceType,
			dwStartType,
			dwErrorControl,
			pszLoadOrderGroup,
			NULL,
			szDependencies,
			&bUpdated);

		if (!fSuccess) 
		{

			MSILOGERR_PROC(_T("NdasDiInstallRootEnumeratedDevice"));

			UINT uiResponse = pMsiErrorMessageBox(hInstall, iErrorDlg);

			// 0, IDABORT, IDRETRY, IDIGNORE
			if (IDABORT == uiResponse) 
			{
				return ERROR_INSTALL_FAILURE;
			} 
			else if (IDRETRY == uiResponse) 
			{
				continue;
			} 
			else if (IDIGNORE == uiResponse) 
			{
			}
		}

		break;
	}

	if (bUpdated)
	{
		(VOID) pMsiScheduleRebootForDeferredCA(hInstall);
	}

	return ERROR_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////
//
// Install Legacy Driver and Schedule Reboot
//
//////////////////////////////////////////////////////////////////////////

UINT 
__stdcall 
NDMsiInstallLegacyDriverWithReboot(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	UINT uiRet = ::NDMsiInstallLegacyDriver(hInstall);
	if (ERROR_SUCCESS == uiRet)
	{
		(VOID) pMsiScheduleRebootForDeferredCA(hInstall);
	}
	return uiRet;
}

//
// This is an immediate action
//
// CustomActionData: 
// <propname>|<netcompid>
//

// Immediate Custom Action
UINT 
__stdcall 
NDMsiFindNetComponent(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	BOOL fSuccess = FALSE;
	UINT uiReturn = 0;

	LPCTSTR pszPropName = NULL; 
	LPCTSTR pszNetCompId = NULL;

	CADParser parser;
	parser.AddToken<LPCTSTR>(pszPropName);
	parser.AddToken<LPCTSTR>(pszNetCompId);
	DWORD nTokens = parser.Parse(hInstall);

	if (parser.GetTokenCount() != nTokens)
	{
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	HRESULT hr = HrIsNetComponentInstalled(pszNetCompId);
	if (S_OK == hr) 
	{
		uiReturn = ::MsiSetProperty(hInstall, pszPropName, _T("1"));
		if (ERROR_SUCCESS != uiReturn) 
		{
			return ERROR_INSTALL_FAILURE;
		}
	} 
	else 
	{
		uiReturn = ::MsiSetProperty(hInstall, pszPropName, _T("0"));
		if (ERROR_SUCCESS != uiReturn) 
		{
			return ERROR_INSTALL_FAILURE;
		}
	}

	return ERROR_SUCCESS;
}

//
// This is an immediate action
//
// CustomActionData: 
// <ierrordialog>
//

UINT 
__stdcall
NDMsiQuestion(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	INT iErrorDlg = 0;

	CADParser parser;
	parser.AddToken<INT>(iErrorDlg);
	DWORD nTokens = parser.Parse(hInstall);

	if (parser.GetTokenCount() != nTokens)
	{
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	DBGPRT_INFO(_FT("iErrorDlg=%d\n"), iErrorDlg);

	INSTALLMESSAGE msgFlags =
		INSTALLMESSAGE(INSTALLMESSAGE_USER|MB_YESNO|MB_ICONQUESTION);

	UINT uiResponse = pMsiMessageBox(hInstall, iErrorDlg, msgFlags, _T(""));

	// 0, IDABORT, IDRETRY, IDIGNORE
	if (IDYES == uiResponse) 
	{
		return ERROR_SUCCESS; 
	} 
	else 
	{
		return ERROR_INSTALL_USEREXIT;
	}

}

//
// This is an immediate action
//
// CustomActionData: 
// <ierrordialog>|<hardwareid>
//
UINT 
__stdcall
NDMsiCheckPnpDeviceInstance(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	INT iErrorDlg = 0;
	LPCTSTR pszHardwareID = NULL;

	CADParser parser;
	parser.AddToken<INT>(iErrorDlg);
	parser.AddToken<LPCTSTR>(pszHardwareID);
	DWORD nTokens = parser.Parse(hInstall);

	if (parser.GetTokenCount() != nTokens)
	{
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	DBGPRT_INFO(_FT("iErrorDlg=%d,HardwareID=%s\n"), iErrorDlg, pszHardwareID);

	BOOL fSuccess = FALSE;
	UINT uiReturn = 0;

	while (TRUE) 
	{

		fSuccess = NdasDiFindExistingDevice(
			NULL, 
			pszHardwareID, 
			TRUE);

		if (!fSuccess) 
		{
			break;
		}

		if (0 != iErrorDlg) 
		{

			INSTALLMESSAGE msgFlags =
				INSTALLMESSAGE(INSTALLMESSAGE_USER|MB_RETRYCANCEL|MB_ICONWARNING);

			UINT uiResponse = pMsiMessageBox(hInstall, iErrorDlg, msgFlags, _T(""));

			// 0, IDABORT, IDRETRY, IDIGNORE
			if (IDCANCEL == uiResponse) 
			{
				return ERROR_INSTALL_USEREXIT;
			} 
			else
			{ // IDRETRY == uiResponse
				continue;
			}
		}

		break;

	}

	return ERROR_SUCCESS;
}

//
// This is an immediate action
//
// CustomActionData: 
// <propname>|<hardwareid>|<presentonly>
//
UINT 
__stdcall 
NDMsiFindPnpDevice(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	BOOL fSuccess = FALSE;
	UINT uiReturn = 0;

	LPCTSTR pszPropName = NULL;
	LPCTSTR pszHardwareID = NULL;
	BOOL fPresentOnly = 0;

	CADParser parser;
	parser.AddToken<LPCTSTR>(pszPropName);
	parser.AddToken<LPCTSTR>(pszHardwareID);
	parser.AddToken<BOOL>(fPresentOnly);
	DWORD nTokens = parser.Parse(hInstall);

	if (parser.GetTokenCount() != nTokens)
	{
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	fSuccess = NdasDiFindExistingDevice(
		NULL, 
		pszHardwareID, 
		fPresentOnly);

	if (fSuccess) 
	{
		uiReturn = ::MsiSetProperty(hInstall, pszPropName, _T("1"));
		if (ERROR_SUCCESS != uiReturn) 
		{
			return ERROR_INSTALL_FAILURE;
		}
	} 
	else 
	{
		uiReturn = ::MsiSetProperty(hInstall, pszPropName, _T("0"));
		if (ERROR_SUCCESS != uiReturn) 
		{
			return ERROR_INSTALL_FAILURE;
		}
	}

	return ERROR_SUCCESS;
}


//
// This is an immediate action
//
// CustomActionData: 
// <propname>|<pending_deletion_prop>|<servicename>
//
UINT 
__stdcall 
NDMsiFindService(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	LPCTSTR pszPropName = NULL;
	LPCTSTR pszPendingDeletionPropName = NULL;
	LPCTSTR pszServiceName = NULL;

	CADParser parser;
	parser.AddToken<LPCTSTR>(pszPropName);
	parser.AddToken<LPCTSTR>(pszPendingDeletionPropName);
	parser.AddToken<LPCTSTR>(pszServiceName);
	DWORD nTokens = parser.Parse(hInstall);

	if (parser.GetTokenCount() != nTokens)
	{
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	BOOL fPendingDeletion;
	BOOL fSuccess = NdasDiFindService(pszServiceName, &fPendingDeletion);
	if (fSuccess) 
	{
	
		UINT uiReturn = ::MsiSetProperty(hInstall, pszPropName, _T("1"));

		// This is an optional feature to ignore error
		(VOID) ::MsiSetProperty(hInstall, 
			pszPendingDeletionPropName, 
			fPendingDeletion ? _T("1") : _T("0"));

		if (ERROR_SUCCESS != uiReturn) 
		{
			return ERROR_INSTALL_FAILURE;
		}
	} 
	else 
	{
		UINT uiReturn = ::MsiSetProperty(hInstall, pszPropName, _T("0"));
		
		if (ERROR_SUCCESS != uiReturn) 
		{
			return ERROR_INSTALL_FAILURE;
		}
	}

	return ERROR_SUCCESS;
}

UINT 
__stdcall
NDMsiSchedulePreboot(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	if (!pMsiSchedulePreboot(hInstall))
	{
		MSILOGERR_PROC(_T("pMsiSchedulePreboot"));
		return ERROR_INSTALL_FAILURE;
	}
	return ::MsiSetProperty(hInstall, _T("NdasCAPrebootRequired"), _T("1"));
}

//
// This is an immediate action!
//
UINT
__stdcall
NDMsiSetPrebootScheduled(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	if (pIsPrebootScheduled(hInstall))
	{
		MSILOGINFO(_T("Preboot is scheduled"));
		UINT uiRet = ::MsiSetProperty(
			hInstall, 
			_T("NdasCAPrebootRequired"), _T("1"));
		if (uiRet != ERROR_SUCCESS)
		{
			MSILOGERR_PROC(_T("MsiSetProperty")
				_T("(\"NdasCAPrebootRequired\",\"1\")"));
			return ERROR_INSTALL_FAILURE;
		}

	}
	MSILOGINFO(_T("Preboot is not scheduled"));
	return ERROR_SUCCESS;
}

UINT 
__stdcall
NDMsiStartServiceOnNoDeferredReboot(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);
	pMsiIncrementProgressBar(hInstall, 10);

	if (pIsDeferredRebootScheduled(hInstall))
	{
		MSILOGINFO(_T("Skipped this action."));
		return ERROR_SUCCESS;
	}

	//
	// Parse Custom Action Data
	//

	LPCTSTR lpServiceName = NULL;

	CADParser parser;
	parser.AddToken<LPCTSTR>(lpServiceName);
	DWORD nTokens = parser.Parse(hInstall);

	if (parser.GetTokenCount() != nTokens)
	{
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	MSILOGINFO(FSB256(_T("ServiceName: %s"), lpServiceName));

	AutoSCHandle hSCM = ::OpenSCManager(
		NULL, 
		SERVICES_ACTIVE_DATABASE, 
		GENERIC_READ | SC_MANAGER_CONNECT);

	if (NULL == (SC_HANDLE) hSCM)
	{
		MSILOGERR_PROC(_T("OpenSCManager"));
		return ERROR_INSTALL_FAILURE;
	}

	AutoSCHandle hService = ::OpenService(
		hSCM, 
		lpServiceName, 
		SERVICE_START);

	if (NULL == (SC_HANDLE)hService)
	{
		MSILOGERR_PROC(FSB256(_T("OpenService(%s)"), lpServiceName));
		return ERROR_INSTALL_FAILURE;
	}

	BOOL fSuccess = ::StartService(hService, 0, NULL);
	if (!fSuccess)
	{
		DWORD dwLastError = ::GetLastError();
		if (ERROR_SERVICE_ALREADY_RUNNING == dwLastError)
		{
			MSILOGINFO(FSB256(_T("Service(%s) is already running. Ignored."), lpServiceName));
			return ERROR_SUCCESS;
		} 
		else 
		{
			MSILOGERR_PROC(FSB256(_T("StartService(%s)"), lpServiceName));
			return ERROR_INSTALL_FAILURE;
		}
	}

	MSILOGINFO(FSB256(_T("Service(%s) is started successfully."), lpServiceName));

	return ERROR_SUCCESS;
}

//
// Immediate Action to be execute after InstallFinalize
// to check if there are any pending reboots from Deferred CAs
//

UINT 
__stdcall
NDMsiUpdateScheduledReboot(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);
	pMsiIncrementProgressBar(hInstall, 10);

	if (pIsDeferredRebootScheduled(hInstall))
	{
		// Reboot Scheduled!
		UINT uiRet = ::MsiSetMode(hInstall, MSIRUNMODE_REBOOTATEND, TRUE);
		if (ERROR_SUCCESS != uiRet)
		{
			// issue warning
			MSILOGERR_PROC2(_T("MsiSetMode(REBOOTATEND)"), uiRet);
		}
		return ERROR_SUCCESS;

	} 
	else 
	{
		MSILOGINFO(_T("Skipped this action."));
		return ERROR_SUCCESS;
	}
}

//
// CustomActionData: 
//
// <ierrordialog>|<compid>|<servicename>|<filename>
//
// ierrordialog: 25001
// compid: nkc_lpx
//
// e.g. 25051|nkc_lpx|lpx|lpx.sys
//        
//
// Find the existing instance first
// And remove the instances

BOOL
pNDMsiRemoveNetComponent(MSIHANDLE hInstall, DWORD dwFlags)
{
	RUN_MODE_TRACE(hInstall);
	pMsiIncrementProgressBar(hInstall, 10);

	//
	// Parse Custom Action Data
	//
	INT iErrorDialog = 0;
	LPCTSTR pszCompId = NULL;
	LPCTSTR pszServiceName = NULL;
	LPCTSTR pszDriverFileName = NULL;

	CADParser parser;
	parser.AddToken<INT>(iErrorDialog);
	parser.AddToken<LPCTSTR>(pszCompId);
	parser.AddToken<LPCTSTR>(pszServiceName);
	parser.AddToken<LPCTSTR>(pszDriverFileName);
	DWORD nTokens = parser.Parse(hInstall);

	if (parser.GetTokenCount() != nTokens)
	{
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	MSILOGINFO(FSB256(
		_T("Component Id: %s, Service Name: %s, Driver File Name: %s"), 
		pszCompId, pszServiceName, pszDriverFileName));

	if (_T('\0') == pszCompId) {
		MSILOGERR(_T("Component ID not specified"));
		return ERROR_INSTALL_FAILURE;
	}

	HRESULT hr = E_FAIL;
	BOOL fRebootRequired = FALSE;

	while (TRUE) 
	{

		hr = HrIsNetComponentInstalled(pszCompId);
		if (S_OK != hr) 
		{
			MSILOGINFO(FSB256(_T("NetComp(%s) is not installed, skipped"), pszCompId));
			break;
		}

		hr = HrUninstallNetComponent(pszCompId);

		if (FAILED(hr)) 
		{
			if (0 == iErrorDialog) 
			{
				MSILOGERR_PROC2(FSB256(_T("HrUninstallNetComponent(%s)"), pszCompId), hr);
				return ERROR_INSTALL_FAILURE;
			}
			UINT uiResponse = pMsiErrorMessageBox(hInstall, iErrorDialog);
			if (IDABORT == uiResponse) 
			{
				return ERROR_INSTALL_FAILURE;
			}
			else if (IDRETRY == uiResponse) 
			{
				continue;
			}
			else if (IDIGNORE == uiResponse) 
			{
			}
		}

		if (NETCFG_S_REBOOT == hr) {
			fRebootRequired = TRUE;
		}
		break;
	}

	//
	// Remove Service
	//
	if (_T('\0') != pszServiceName) 
	{
		pFindAndDeleteService(pszServiceName, &fRebootRequired);
	}

	//
	// Delete File
	//
	if (_T('\0') != pszDriverFileName) 
	{
		pFindAndDeleteDriverFile(pszDriverFileName, &fRebootRequired);
	}

	DBGPRT_INFO(_FT("Reboot required: %d\n"), fRebootRequired);

	if (fRebootRequired) 
	{
		if (dwFlags & FLG_MSI_SCHEDULE_PREBOOT) 
		{
			pMsiSchedulePreboot(hInstall);
		} 
		else 
		{
			pMsiScheduleRebootForDeferredCA(hInstall);
		}
	}

	return ERROR_SUCCESS;

}

// Immediate Action
UINT 
__stdcall 
NDMsiPreRemoveNetComponent(MSIHANDLE hInstall)
{
	return pNDMsiRemoveNetComponent(hInstall, FLG_MSI_SCHEDULE_PREBOOT);
}

// Deferred Custom Action
UINT 
__stdcall 
NDMsiRemoveNetComponent(MSIHANDLE hInstall)
{
	return pNDMsiRemoveNetComponent(hInstall, FLG_MSI_SCHEDULE_REBOOT);
}

//
// CustomActionData: 
//
// <ierrordialog>|<hardwareid>|<servicename>|<filename>
//

UINT
pNDMsiRemoveDevice(MSIHANDLE hInstall, DWORD dwFlags)
{
	RUN_MODE_TRACE(hInstall);
	pMsiIncrementProgressBar(hInstall, 10);

	BOOL fSuccess = FALSE;
	UINT uiReturn = 0;

	//
	// Parse Custom Action Data
	//

	INT iErrorDialog = 0;
	LPCTSTR pszHardwareId = NULL;
	LPCTSTR pszServiceName = NULL;
	LPCTSTR pszDriverFileName = NULL;

	CADParser parser;
	parser.AddToken<INT>(iErrorDialog);
	parser.AddToken<LPCTSTR>(pszHardwareId);
	parser.AddToken<LPCTSTR>(pszServiceName);
	parser.AddToken<LPCTSTR>(pszDriverFileName);
	DWORD nTokens = parser.Parse(hInstall);

	if (parser.GetTokenCount() != nTokens)
	{
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	MSILOGINFO(FSB256(
		_T("Hardware Id: %s, Service Name: %s, Driver File Name: %s"), 
		pszHardwareId, 
		pszServiceName,
		pszDriverFileName));

	if (_T('\0') == pszHardwareId) 
	{
		MSILOGERR(_T("Hardware ID not specified"));
		return ERROR_INSTALL_FAILURE;
	}

	DWORD cRemoved;
	BOOL fRebootRequired = FALSE;

	while (TRUE) 
	{

		fSuccess = NdasDiRemoveDevice(
			NULL, 
			pszHardwareId,
			FALSE,
			&cRemoved,
			&fRebootRequired,
			NULL,
			NULL);

		if (!fSuccess && 0 != iErrorDialog) 
		{
			MSILOGERR_PROC(FSB256(_T("NdasDiRemoveDevice(%s)"), pszHardwareId));
			UINT uiResponse = pMsiErrorMessageBox(hInstall, iErrorDialog);
			// 0, IDABORT, IDRETRY, IDIGNORE
			if (IDABORT == uiResponse) 
			{
				return ERROR_INSTALL_FAILURE;
			} 
			else if (IDRETRY == uiResponse) 
			{
				continue;
			} 
			else if (IDIGNORE == uiResponse) 
			{
			}
		}
		break;
	}

	MSILOGINFO(FSB256(_T("Removed %d instances"), cRemoved));

	//
	// Remove Service
	//
	if (_T('\0') != pszServiceName) {
		pFindAndDeleteService(pszServiceName, &fRebootRequired);
	}

	//
	// Delete File
	//
	if (_T('\0') != pszDriverFileName) {
		pFindAndDeleteDriverFile(pszDriverFileName, &fRebootRequired);
	}

	MSILOGINFO(FSB256(_T("Reboot required: %d"), fRebootRequired));

	if (fRebootRequired) {
		if (dwFlags & FLG_MSI_SCHEDULE_PREBOOT) {
			pMsiSchedulePreboot(hInstall);
		} else {
			pMsiScheduleRebootForDeferredCA(hInstall);
		}
	}

	return ERROR_SUCCESS;
}

// Immediate Custom Action
UINT 
__stdcall 
NDMsiPreRemoveDevice(MSIHANDLE hInstall)
{
	return pNDMsiRemoveDevice(hInstall, FLG_MSI_SCHEDULE_PREBOOT);
}

// Deferred Custom Action
UINT 
__stdcall 
NDMsiRemoveDevice(MSIHANDLE hInstall)
{
	return pNDMsiRemoveDevice(hInstall, FLG_MSI_SCHEDULE_REBOOT);
}

//
// Remove Driver Service deletes the service 
// and c:\windows\system32\drivers\<filename> 
//
// ScheduleForceReboot will be set if necessary
//
// CustomActionData: 
//
// <ierrordialog>|<servicename>|<filename>
//
//

static UINT
pNDMsiRemoveDriverService(MSIHANDLE hInstall, DWORD dwFlags);

UINT
pNDMsiRemoveDriverService(MSIHANDLE hInstall, DWORD dwFlags)
{
	RUN_MODE_TRACE(hInstall);
	pMsiIncrementProgressBar(hInstall, 10);

	BOOL fSuccess = FALSE;
	UINT uiReturn = 0;

	//
	// Parse Custom Action Data
	//

	INT iErrorDialog = 0;
	LPCTSTR pszServiceName = NULL;
	LPCTSTR pszDriverFileName = NULL;

	CADParser parser;
	parser.AddToken<INT>(iErrorDialog);
	parser.AddToken<LPCTSTR>(pszServiceName);
	parser.AddToken<LPCTSTR>(pszDriverFileName);
	DWORD nTokens = parser.Parse(hInstall);

	if (parser.GetTokenCount() != nTokens)
	{
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	MSILOGINFO(FSB256(_T("Service Name: %s, Driver File Name: %s"), pszServiceName, pszDriverFileName));

	if (_T('\0') == pszServiceName) 
	{
		MSILOGERR(_T("Service Name not specified"));
		return ERROR_INSTALL_FAILURE;
	}

	DWORD cRemoved;
	BOOL fRebootRequired = FALSE;

	//
	// TODO: Provide Error?
	//

	while (TRUE) 
	{

		//
		// Remove Service
		//
		fSuccess = pFindAndDeleteService(pszServiceName, &fRebootRequired);

		if (!fSuccess) 
		{

			UINT uiResponse = pMsiErrorMessageBox(hInstall, iErrorDialog);

			// 0, IDABORT, IDRETRY, IDIGNORE
			if (IDABORT == uiResponse) 
			{
				return ERROR_INSTALL_FAILURE;
			}
			else if (IDRETRY == uiResponse) 
			{
				continue;
			}
			else if (IDIGNORE == uiResponse) 
			{
			}
		}

		break;
	}

	if (fSuccess) 
	{

		//
		// Delete File
		//
		if (_T('\0') != pszDriverFileName) 
		{
			pFindAndDeleteDriverFile(pszDriverFileName, &fRebootRequired);
		}
	}

	MSILOGINFO(FSB256(_T("Reboot required: %d\n"), fRebootRequired));

	if (fRebootRequired) 
	{
		if (FLG_MSI_SCHEDULE_PREBOOT & dwFlags) 
		{
			pMsiSchedulePreboot(hInstall);
		}
		else 
		{
			pMsiScheduleRebootForDeferredCA(hInstall);
		}
	}


	return ERROR_SUCCESS;
}

UINT 
__stdcall
NDMsiPreRemoveDriverService(MSIHANDLE hInstall)
{
	return pNDMsiRemoveDriverService(hInstall, FLG_MSI_SCHEDULE_PREBOOT);
}

UINT 
__stdcall 
NDMsiRemoveDriverService(MSIHANDLE hInstall)
{
	return pNDMsiRemoveDriverService(hInstall, FLG_MSI_SCHEDULE_REBOOT);
}

UINT 
__stdcall
NDMsiSetMigration(MSIHANDLE hInstall)
{
	HKEY hKey = (HKEY) INVALID_HANDLE_VALUE;
	DWORD dwDisposition;

	LONG lResult = ::RegCreateKeyEx(
		HKEY_LOCAL_MACHINE,
		_T("Software\\NDAS\\Install"),
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_ALL_ACCESS,
		NULL,
		&hKey,
		&dwDisposition);

	if (ERROR_SUCCESS != lResult) {
		pMsiLogError(hInstall, _T("RegCreateKeyEx"), _T("NDMsiSetMigration"), lResult);
		return ERROR_SUCCESS;
	}

	DWORD dwMigrate = 1;
	lResult = ::RegSetValueEx(
		hKey,
		_T("Migrate"),
		0,
		REG_DWORD,
		(CONST BYTE*)&dwMigrate,
		sizeof(dwMigrate));

	if (ERROR_SUCCESS != lResult) {
		pMsiLogError(hInstall, _T("RegSetValueEx"), _T("NDMsiSetMigration"), lResult);
	}

	::RegCloseKey(hKey);
	return ERROR_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////

static 
BOOL 
CALLBACK
pNdasDiDeleteOEMInfCallback(
	DWORD dwError, LPCTSTR szPath, LPCTSTR szFileName, LPVOID lpContext)
{
	UNREFERENCED_PARAMETER(lpContext);

	//
	// It is not safe to cast merely LPVOID into MSIHANDLE
	// (/Wp64 warning)
	//
	MSIHANDLE hInstall = (MSIHANDLE) PtrToUlong(lpContext);

	// logging only, not interactive
	if (ERROR_SUCCESS == dwError) 
	{
		pMsiLogMessageEx(hInstall, LMM_ERROR, TFN, 
			_T("Deleted: %s\\%s"), szPath, szFileName);
	}
	else 
	{
		pMsiLogMessageEx(hInstall, LMM_ERROR, TFN, 
			_T("Deleting %s\\%s failed with error %d (%08X)"),
			szPath, szFileName, dwError, dwError);
	}

	// continue deletion
	return TRUE;
}

//
// CustomActionData: 
//
// <ierrordialog>|<find-spec>|<semicolon-delimited-hardware-id-list>
//
// e.g. 25001|oem*.inf|root\ndasbus;root\lanscsibus;
//

UINT 
__stdcall
NDMsiDeleteOEMInf(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);
	pMsiIncrementProgressBar(hInstall, 10);

	BOOL fSuccess = FALSE;
	UINT uiReturn = 0;

	//
	// Parse Custom Action Data
	//

	INT iErrorDialog = 0;
	LPCTSTR pszFindSpec = NULL;
	LPCTSTR pszHardwareIDList = NULL;

	CADParser parser;
	parser.AddToken<INT>(iErrorDialog);
	parser.AddToken<LPCTSTR>(pszFindSpec);
	parser.AddToken<LPCTSTR>(pszHardwareIDList);
	DWORD nTokens = parser.Parse(hInstall);

	if (parser.GetTokenCount() != nTokens)
	{
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	MSILOGINFO(FSB256(_T("FindSpec: %s, Hardware ID List: %s"), pszFindSpec, pszHardwareIDList));

	if (_T('\0') == pszHardwareIDList) 
	{
		MSILOGINFO(_T("Hardware ID List not specified. Ignored"));
		// Ignore this action in case of no hardware id specified
		return ERROR_SUCCESS;
	}

	// convert hwid1;hwid2;\0 to hwid1\0hwid2\0\0
	TCHAR mszHardwareIDs[512] = {0};
	HRESULT hr = ::StringCchCopy(
		mszHardwareIDs, 
		RTL_NUMBER_OF(mszHardwareIDs), 
		pszHardwareIDList);
	_ASSERTE(SUCCEEDED(hr));
	if (FAILED(hr))
	{
		MSILOGERR_PROC2(_T("Hardware ID List length is too long. Limited to 512 chars."), hr);
		return ERROR_INSTALL_FAILURE;
	}

	for (PTCHAR pch = mszHardwareIDs; _T('\0') != *pch; ++pch)
	{
		if (_T(';') == *pch) 
		{
			*pch = _T('\0');
		}
	}

	while (TRUE) 
	{

		fSuccess = ::NdasDiDeleteOEMInf(
			pszFindSpec,
			mszHardwareIDs,
			0x0001, // SUOI_FORCEDELETE,
			pNdasDiDeleteOEMInfCallback,
			ULongToPtr(hInstall));

		if (!fSuccess) 
		{
			pMsiLogMessageEx(hInstall, LMM_INFO, TFN,
				_T("NdasDiDeleteOEMInf failed: Error Code %08X, error ignored"),
				::GetLastError());

/*
			UINT uiResponse = pMsiErrorMessageBox(hInstall, iErrorDialog);

			// 0, IDABORT, IDRETRY, IDIGNORE
			if (IDABORT == uiResponse) {
				return ERROR_INSTALL_FAILURE;
			} else if (IDRETRY == uiResponse) {
				continue;
			} else if (IDIGNORE == uiResponse) {
			}
*/
		}

		break;
	}

	return ERROR_SUCCESS;
}

UINT 
__stdcall
NDMsiRefreshShellIconCache(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	pMsiLogMessage(hInstall, LMM_INFO, TFN, _T("Started"));
	w32sys::RefreshShellIconCache();
	return ERROR_SUCCESS;
}

//
// (NOT IMPLEMENTED)
//
// Get the package code from the information stream
//
UINT 
__stdcall
NDMsiGetPackageCode(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);
	return ERROR_SUCCESS;
}

//
// (NOT IMPLEMENTED)
//
// Removes the cached package
// e.g. c:\windows\downloaded installation\{PackageCode}\ndas.msi
//
// Custom Action Data contains the above path
UINT 
__stdcall
NDMsiDeleteCachedPackage(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);
	return ERROR_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////
//
// Adding DRIVERS directory to the System Driver Source Directory
//
//////////////////////////////////////////////////////////////////////////

UINT 
__stdcall
NDMsiAddDriverSource(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	TCHAR szProp[MAX_PATH] = {0};
	DWORD cchProp = sizeof(szProp);

	UINT uiRet = ::MsiGetProperty(
		hInstall, 
		_T("CustomActionData"),
		szProp,
		&cchProp);

	if (ERROR_SUCCESS != uiRet) 
	{
		// do not return errors
		pMsiLogMessageEx(hInstall, LMM_INFO, TFN,
			_T("MsiGetProperty failed: Error Code %08X, error ignored"),
			::GetLastError());
		return ERROR_INSTALL_FAILURE;
	}

	// remove trailing backslash(\)
	{
		LPTSTR lp = szProp;
		while (_T('\0') != *lp) ++lp;
		if (lp > szProp && _T('\\') == *(lp-1))
		{
			*(lp-1) = _T('\0');
		}
	}

	BOOL fAdded = ::SetupAddToSourceList(SRCLIST_SYSTEM, szProp);
	if (!fAdded)
	{
		pMsiLogMessageEx(hInstall, LMM_ERROR, TFN,
			_T("SetupAddToSourceList failed: Error=%08X"), ::GetLastError());
		return ERROR_INSTALL_FAILURE;
	}

	pMsiLogMessageEx(hInstall, LMM_INFO, TFN,
		_T("Added to Setup Source List: %s"), szProp);

	return ERROR_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////
//
// Adding DRIVERS directory to the System Driver Source Directory
//
//////////////////////////////////////////////////////////////////////////

UINT 
__stdcall
NDMsiRemoveDriverSource(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	TCHAR szProp[MAX_PATH] = {0};
	DWORD cchProp = sizeof(szProp);

	UINT uiRet = ::MsiGetProperty(
		hInstall, 
		_T("CustomActionData"),
		szProp,
		&cchProp);

	if (ERROR_SUCCESS != uiRet) 
	{
		// do not return errors
		pMsiLogMessageEx(hInstall, LMM_INFO, TFN,
			_T("MsiGetProperty failed: Error Code %08X, error ignored"),
			::GetLastError());
		return ERROR_INSTALL_FAILURE;
	}

	BOOL fRemoved = ::SetupRemoveFromSourceList(SRCLIST_SYSTEM, szProp);
	if (!fRemoved)
	{
		pMsiLogMessageEx(hInstall, LMM_ERROR, TFN,
			_T("SetupRemoveFromSourceList failed: Error=%08X"), ::GetLastError());
		return ERROR_INSTALL_FAILURE;
	}

	pMsiLogMessageEx(hInstall, LMM_INFO, TFN,
		_T("Removed from Setup Source List: %s\n"), szProp);

	return ERROR_SUCCESS;
}

BOOL 
WINAPI
pFileExists(LPCTSTR szFilePath)
{
	HANDLE hFindFile;
	WIN32_FIND_DATA ffData;
	UINT uiOldErrorMode;
	DWORD Error;

	uiOldErrorMode = ::SetErrorMode(SEM_FAILCRITICALERRORS);

	hFindFile = ::FindFirstFile(szFilePath,&ffData);
	if(INVALID_HANDLE_VALUE == hFindFile) {
		Error = GetLastError();
	} else {
		::FindClose(hFindFile);
		Error = ERROR_SUCCESS;
	}

	::SetErrorMode(uiOldErrorMode);

	::SetLastError(Error);
	return (Error == ERROR_SUCCESS);
}

BOOL
__stdcall
NDMsiCheckMarkedForDeletion(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);
	pMsiIncrementProgressBar(hInstall, 10);

	BOOL fSuccess = FALSE;

	//
	// Parse Custom Action Data
	//

	LPCTSTR pszServiceName = NULL;

	CADParser parser;
	parser.AddToken<LPCTSTR>(pszServiceName);
	DWORD nTokens = parser.Parse(hInstall);

	if (parser.GetTokenCount() != nTokens)
	{
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	MSILOGINFO(FSB256(_T("ServiceName: %s"), pszServiceName));

	BOOL fMarked = NdasDiIsServiceMarkedForDeletion(pszServiceName);
	
	if (fMarked)
	{
		MSILOGINFO(_T("Service is marked for deletion. Schedule pre-boot"));
		if (!pMsiSchedulePreboot(hInstall))
		{
			MSILOGERR_PROC(_T("pMsiSchedulePreboot"));
			return ERROR_INSTALL_FAILURE;
		}
	}
	else
	{
		MSILOGINFO(_T("Service is NOT marked for deletion."));
	}

	return ERROR_SUCCESS;
}

BOOL 
WINAPI
pFindAndDeleteService(LPCTSTR szServiceName, LPBOOL lpbReboot)
{
	BOOL fPendingDeletion;
	
	BOOL fSuccess = NdasDiIsServiceMarkedForDeletion(szServiceName);
	if (fSuccess) {
		*lpbReboot = TRUE;
		return TRUE;
	}

	fSuccess = NdasDiFindService(szServiceName, &fPendingDeletion);
	if (fSuccess) {
		if (fPendingDeletion) {
			*lpbReboot = TRUE;
		} else {
			fSuccess = NdasDiDeleteService(szServiceName);
			fSuccess = NdasDiFindService(szServiceName, &fPendingDeletion);
			if (fSuccess && fPendingDeletion) {
				*lpbReboot = TRUE;
			}
		}
	}

	return TRUE;
}

BOOL 
WINAPI
pFindAndDeleteDriverFile(LPCTSTR szDriverFileName, LPBOOL lpbReboot)
{
	TCHAR szDriverFilePath[MAX_PATH] = {0};
	UINT cch = ::GetSystemDirectory(szDriverFilePath, MAX_PATH);
	_ASSERTE(cch > 0);

	HRESULT hr = ::StringCchCat(szDriverFilePath, MAX_PATH, _T("\\DRIVERS\\"));
	_ASSERTE(SUCCEEDED(hr));

	hr = ::StringCchCat(szDriverFilePath, MAX_PATH, szDriverFileName);
	_ASSERTE(SUCCEEDED(hr));

	return pFindAndDeleteFile(szDriverFilePath, lpbReboot);
}

BOOL 
WINAPI
pFindAndDeleteFile(LPCTSTR szFilePath, LPBOOL lpbReboot)
{
	if (pFileExists(szFilePath)) {
		BOOL fSuccess;
		fSuccess = ::DeleteFile(szFilePath);
		if (!fSuccess) {
			fSuccess = ::MoveFileEx(
				szFilePath, 
				NULL, 
				MOVEFILE_DELAY_UNTIL_REBOOT);
			*lpbReboot = TRUE;
		}
	}
	return TRUE;
}

void
pDeleteFiles(MSIHANDLE hInstall, LPCTSTR szSearchSpec, BOOL bRecursive)
{
	TCHAR szBasePath[MAX_PATH] = {0};
	HRESULT hr = ::StringCchCopy(szBasePath, MAX_PATH, szSearchSpec);
	if (FAILED(hr))
	{
		pMsiLogError(hInstall, TFN, FSB1024(_T("BasePathCopy from %s"), szSearchSpec), hr);
		return;
	}
	if (!::PathRemoveFileSpec(szBasePath))
	{
		pMsiLogError(hInstall, TFN, FSB1024(_T("PathRemoveFileSpec %s"), szBasePath));
		return;
	}

	WIN32_FIND_DATA ffData = {0};
	AutoFindFileHandle hFindFile = ::FindFirstFile(szSearchSpec, &ffData);
	if (INVALID_HANDLE_VALUE == (HANDLE) hFindFile)
	{
		if (ERROR_NO_MORE_FILES != ::GetLastError())
		{
			pMsiLogError(hInstall, TFN, FSB1024(_T("FindFirstFile %s"), szSearchSpec));
		}
		return;
	}

	do
	{
		if (0 == ::lstrcmp(_T("."), ffData.cFileName) ||
			0 == ::lstrcmp(_T(".."), ffData.cFileName))
		{
			// ignore . and ..
			continue;
		}

		TCHAR szFilePath[MAX_PATH];
		HRESULT hr = ::StringCchCopy(szFilePath, MAX_PATH, szBasePath);
		if (FAILED(hr))
		{
			// FILE_NAME_TOO_LONG
			// issue warning and continue!
			pMsiLogError(hInstall, TFN, FSB1024(_T("StringCchCopy %s"), szFilePath), hr);
			continue;
		}

		if (!::PathAppend(szFilePath, ffData.cFileName))
		{
			// issue warning and continue!
			pMsiLogError(hInstall, TFN, FSB1024(_T("PathAppend %s"), ffData.cFileName));
			continue;
		}

		if (FILE_ATTRIBUTE_DIRECTORY & ffData.dwFileAttributes)
		{
			if (bRecursive)
			{
				TCHAR szNewSpec[MAX_PATH];
				HRESULT hr = ::StringCchCopy(szNewSpec, MAX_PATH, szFilePath);
				if (FAILED(hr))
				{
					pMsiLogError(hInstall, TFN, FSB1024(_T("StringCchCopy %s"), szFilePath), hr);
					continue;
				}
				if (!::PathAppend(szNewSpec, _T("\\*.*")))
				{
					pMsiLogError(hInstall, TFN, FSB1024(_T("PathAppend %s + \\*.*"), szNewSpec));
					continue;
				}

				pDeleteFiles(hInstall, szNewSpec, TRUE);
			}

			pMsiLogMessage(hInstall, LMM_INFO, TFN, FSB256(_T("Remove Directory %s"), szFilePath));
			if (!::RemoveDirectory(szFilePath))
			{
				// issue warning and continue!
				pMsiLogError(hInstall, TFN, FSB1024(_T("RemoveDirectory %s"), szFilePath));
				continue;
			}
		}

		pMsiLogMessage(hInstall, LMM_INFO, TFN, FSB256(_T("Deleting %s"), szFilePath));
		if (!::DeleteFile(szFilePath))
		{
			// issue warning and continue!
			pMsiLogError(hInstall, TFN, FSB1024(_T("DeleteFile %s"), szFilePath));
			continue;
		}

	} while (::FindNextFile(hFindFile, &ffData));

	if (ERROR_NO_MORE_FILES != ::GetLastError())
	{
		pMsiLogError(hInstall, TFN, FSB1024(_T("FindNextFile %s"), szSearchSpec));
	}

	return;
}

UINT
__stdcall
NDMsiChangeServiceConfig(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);
	pMsiIncrementProgressBar(hInstall, 10);

	//
	// Parse Custom Action Data
	//
	LPCTSTR lpServiceName = NULL;
	DWORD dwServiceType = SERVICE_NO_CHANGE;
	DWORD dwStartType = SERVICE_NO_CHANGE;
	DWORD dwErrorControl = SERVICE_NO_CHANGE;

	CADParser parser;
	parser.AddToken<LPCTSTR>(lpServiceName);
	parser.AddToken<DWORD>(dwServiceType);
	parser.AddToken<DWORD>(dwStartType);
	parser.AddToken<DWORD>(dwErrorControl);
	DWORD nTokens = parser.Parse(hInstall);

	if (parser.GetTokenCount() != nTokens)
	{
		MSILOGERR(_T("CustomActionData parse failed"));
		return ERROR_INSTALL_FAILURE;
	}

	MSILOGINFO(FSB256(
		_T("ServiceName: %s, ServiceType: 0x%X, StartType: 0x%X, ErrorControl: 0x%X"), 
		lpServiceName, dwServiceType, dwStartType, dwErrorControl));

	AutoSCHandle hSCM = ::OpenSCManager(
		NULL, 
		SERVICES_ACTIVE_DATABASE, 
		GENERIC_READ | SC_MANAGER_CONNECT);

	if (NULL == (SC_HANDLE) hSCM)
	{
		MSILOGERR_PROC(_T("OpenSCManager"));
		return ERROR_INSTALL_FAILURE;
	}

	AutoSCHandle hService = ::OpenService(
		hSCM, 
		lpServiceName, 
		SERVICE_CHANGE_CONFIG);

	if (NULL == (SC_HANDLE)hService)
	{
		MSILOGERR_PROC(FSB256(_T("OpenService(%s)"), lpServiceName));
		return ERROR_INSTALL_FAILURE;
	}

	BOOL fSuccess = ::ChangeServiceConfig(
		hService, dwServiceType, dwStartType, dwErrorControl,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL);

	if (!fSuccess)
	{
		MSILOGERR_PROC(FSB256(
			_T("ChangeServiceConfig(%s) failed"), lpServiceName));
		return ERROR_INSTALL_FAILURE;
	}

	MSILOGINFO(FSB256(
		_T("ChangeServiceConfig(%s) completed successfully."), lpServiceName));

	return ERROR_SUCCESS;
}

UINT
__stdcall
NDMsiCleanupSourceDirectory(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	TCHAR szSourceDir[MAX_PATH] = {0};
	DWORD cchSourceDir;

	UINT uiRet = ::MsiGetProperty(
		hInstall, _T("SourceDir"), szSourceDir, &cchSourceDir);
	if (ERROR_SUCCESS != uiRet)
	{
		// ERROR_MORE_DATA if SourceDir is more than MAX_PATH
		// We are not willing to handle such situation.
		return ERROR_INSTALL_FAILURE;
	}

	if (!::PathAppend(szSourceDir, _T("*.*")))
	{
		return ERROR_INSTALL_FAILURE;
	}

	UINT uiOldErrorMode = ::SetErrorMode(SEM_FAILCRITICALERRORS);
	pDeleteFiles(hInstall, szSourceDir, TRUE);
	::SetErrorMode(uiOldErrorMode);

	return ERROR_SUCCESS;
}

NC_DLLSPEC 
UINT 
NC_CALLSPEC 
NcaResetProgressBar(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	INT totalTicks = 100;
	INT direction = 0; // 0 for Forward, 1 for Backward
	INT execType = 0; // 0 for Exec in progress, 1 for creating exec script

	CADParser parser;
	parser.AddToken<INT>(totalTicks);
	parser.AddToken<INT>(direction);
	parser.AddToken<INT>(execType);
	DWORD nTokens = parser.Parse(hInstall);

	if (parser.GetTokenCount() != nTokens)
	{
		MSILOGERR(_T("CustomActionData parse failed"));
		return ERROR_INSTALL_FAILURE;
	}

	return pMsiResetProgressBar(hInstall, totalTicks, direction, execType);
}

NC_DLLSPEC 
UINT 
NC_CALLSPEC 
NcaIncrementProgressBar(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	INT ticks = 10;

	CADParser parser;
	parser.AddToken<INT>(ticks);
	DWORD nTokens = parser.Parse(hInstall);

	if (parser.GetTokenCount() != nTokens)
	{
		MSILOGERR(_T("CustomActionData parse failed"));
		return ERROR_INSTALL_FAILURE;
	}

	return pMsiIncrementProgressBar(hInstall, ticks);
}

NC_DLLSPEC 
UINT 
NC_CALLSPEC 
NcaIncrementProgressBar10(MSIHANDLE hInstall)
{
	INT ticks = 10;
	return pMsiIncrementProgressBar(hInstall, ticks);
}
