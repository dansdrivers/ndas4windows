#include "stdafx.h"
#include "autores.h"

#include <netcfgx.h>
#include <setupapi.h>
#include "netcomp.h"
#include "pnpdevinst.h"
#include "svcinst.h"
#include "shiconcache.h"
#include "fstrbuf.h"
#include "ndas/ndasautoregscope.h"
#include "ndas/ndascntenc.h"

#define XDBG_FILENAME "ndmsica.cpp"
#include "xdebug.h"

#include "misc.h"
#include "msilog.h"


//
// Custom actions can return the following values:
//
// - ERROR_FUNCTION_NOT_CALLED
// - ERROR_SUCCESS
// - ERROR_INSTALL_USEREXIT
// - ERROR_INSTALL_FAILURE
// - ERROR_NO_MORE_ITEMS
//

static LPCTSTR CA_PROPERTY_NAME = _T("CustomActionData");
static const TCHAR CA_TOKEN = _T('|');
static const TCHAR PROP_DELIMITER = _T('|');

static BOOL WINAPI
pFindAndDeleteDriverFile(LPCTSTR szDriverFileName, LPBOOL lpbReboot);

static BOOL WINAPI
pFindAndDeleteFile(LPCTSTR szFilePath, LPBOOL lpbReboot);

static BOOL WINAPI
pFileExists(LPCTSTR szFilePath);

static BOOL WINAPI
pFindAndDeleteService(LPCTSTR szServiceName, LPBOOL lpbReboot);

static BOOL WINAPI
pMsiSchedulePreboot(MSIHANDLE hInstall);

static BOOL WINAPI
pIsPrebootScheduled();

static BOOL WINAPI
pSetInstalledPath(
	HKEY hKeyRoot, 
	LPCTSTR szRegPath, 
	LPCTSTR szValue, 
	LPCTSTR szData);

//
// This macro assumes that MSIHANDLE is hInstall for convenience.
// (ONLY for LOCAL use!)
//

static LPTSTR 
pGetMsiProperty(
	MSIHANDLE hInstall, 
	LPCTSTR szPropName, 
	LPDWORD pcchData);

static LPTSTR
pParseMsiProperty(
	MSIHANDLE hInstall, 
	LPCTSTR szPropName, 
	CONST TCHAR cchDelimiter, 
	...);

static UINT 
pMsiErrorMessageBox(
	MSIHANDLE hInstall, 
	INT iErrorDialog, 
	DWORD dwError,
	LPCTSTR szErrorText);

static UINT 
pMsiErrorMessageBox(
	MSIHANDLE hInstall, 
	INT iErrorDialog, 
	DWORD dwError = GetLastError());

static UINT 
pMsiMessageBox(
	MSIHANDLE,
	INT iErrorDialog,
	INSTALLMESSAGE Flags,
	LPCTSTR szText);

static UINT
pMsiScheduleRebootForDeferredCA(MSIHANDLE hInstall);

static UINT
pMsiScheduleReboot(MSIHANDLE hInstall);

static CONST DWORD FLG_MSI_SCHEDULE_PREBOOT = 0x0001;
static CONST DWORD FLG_MSI_SCHEDULE_REBOOT = 0x0002;

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


#define TFN _T(__FUNCTION__)

#define MSILOGINFO(msg) pMsiLogMessage(hInstall, LMM_INFO, TFN, msg);
#define MSILOGERR(msg) pMsiLogMessage(hInstall, LMM_ERROR, TFN, msg);
#define MSILOGERR_PROC(proc) pMsiLogError(hInstall, TFN, proc);
#define MSILOGERR_PROC2(proc, err) pMsiLogError(hInstall, TFN, proc, err);

#define RUN_MODE_TRACE()  \
	do { \
	if (::MsiGetMode(hInstall,MSIRUNMODE_ROLLBACK)) { \
		MSILOGINFO(_T("Rollback Action Started")); \
	} else if (::MsiGetMode(hInstall,MSIRUNMODE_COMMIT)) { \
		MSILOGINFO(_T("Commit Action Started")); \
	} else if (::MsiGetMode(hInstall,MSIRUNMODE_SCHEDULED)) { \
		MSILOGINFO(_T("Scheduled Action Started")); \
	} else { \
		MSILOGINFO(_T("Action Started")); \
	} } while(0)

// Deferred Custom Action
UINT 
__stdcall 
NDMsiInstallNetComponent(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE();

	BOOL fSuccess = FALSE;
	UINT uiReturn = 0;

	//
	// Parse Custom Action Data
	//

	LPTSTR pszErrorDialog = NULL;
	LPTSTR pszNetClass = NULL;
	LPTSTR pszCompId = NULL;
	LPTSTR pszInfPath = NULL;
	LPTSTR pszRegPath = NULL;
	LPTSTR pszRegValue = NULL;

	LPTSTR lpTokens = pParseMsiProperty(
		hInstall,
		CA_PROPERTY_NAME,
		PROP_DELIMITER,
		&pszErrorDialog,
		&pszNetClass,
		&pszCompId,
		&pszInfPath,
		&pszRegPath,
		&pszRegValue,
		NULL);

	if (NULL == lpTokens) {
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	AutoHLocal autoTokens = (HLOCAL) lpTokens;

	INT iErrorDialog = _ttoi(pszErrorDialog);

	NetClass nc = pGetNetClass(pszNetClass);
	if (NC_Unknown == nc) {
		MSILOGERR(FSB256(_T("Invalid Net Class: %s"), pszNetClass));
		return ERROR_INSTALL_FAILURE;
	}

	if (_T('\0') == pszCompId) {
		MSILOGERR(_T("Component ID not specified"));
		return ERROR_INSTALL_FAILURE;
	}

	if (_T('\0') == pszInfPath) {
		pszInfPath = NULL;
	}

	MSILOGINFO(
		FSB256(_T("Net Class: %s, Component Id: %s, InfPath: %s, RegPath: %s, RegValue %s"),
		pszNetClass, pszCompId, pszInfPath, pszRegPath, pszRegValue));

	HRESULT hr;
	TCHAR szInstalledInf[MAX_PATH];

	while (TRUE) {

		hr = HrInstallNetComponent(
			pszCompId,
			nc,
			pszInfPath,
			szInstalledInf,
			MAX_PATH,
			NULL,
			NULL);

		if (FAILED(hr)) {

			MSILOGERR_PROC2(_T("HrInstallNetComponent"), hr);

			if (0 == iErrorDialog) {
				return ERROR_INSTALL_FAILURE;
			}

			UINT uiResponse = pMsiErrorMessageBox(hInstall, iErrorDialog, hr);
			// 0, IDABORT, IDRETRY, IDIGNORE
			if (IDABORT == uiResponse) {
				return ERROR_INSTALL_FAILURE;
			} else if (IDRETRY == uiResponse) {
				continue;
			} else if (IDIGNORE == uiResponse) {
			}
		}

		break;
	}

	//
	// The followings actions are not critical ones
	// Just log errors if any and return ERROR_SUCCESS
	//

	if (NETCFG_S_REBOOT == hr) {
		pMsiScheduleRebootForDeferredCA(hInstall);
	}

	//
	// set the registry value
	//
	fSuccess = pSetInstalledPath(
		HKEY_LOCAL_MACHINE, 
		pszRegPath, 
		pszRegValue, 
		szInstalledInf);

	if (!fSuccess) {
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
	RUN_MODE_TRACE();

	UINT uiReturn = 0;

	LPTSTR pszErrorDlg = NULL;
	LPTSTR pszInfPath = NULL;
	LPTSTR pszRegPath = NULL;
	LPTSTR pszRegValue = NULL;

	LPTSTR lpTokens = pParseMsiProperty(
		hInstall,
		CA_PROPERTY_NAME,
		PROP_DELIMITER,
		&pszErrorDlg,
		&pszInfPath,
		&pszRegPath,
		&pszRegValue,
		NULL);

	if (NULL == lpTokens) {
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	AutoHLocal autoTokens = (HLOCAL) lpTokens;
	INT iErrorDlg = _ttoi(pszErrorDlg);
	TCHAR szInstalledInf[MAX_PATH] = {0};

	BOOL fSuccess = FALSE;

	while (TRUE) {

		fSuccess = NdasDiCopyOEMInf(
			pszInfPath, 
			0, 
			szInstalledInf, 
			MAX_PATH, 
			NULL, 
			NULL);

		if (!fSuccess) {

			MSILOGERR_PROC(_T("NdasDiCopyOEMInf"));

			if (0 == iErrorDlg) {
				return ERROR_INSTALL_FAILURE;
			}

			UINT uiResponse = pMsiErrorMessageBox(hInstall, iErrorDlg);
			if (IDABORT == uiResponse) {
				return ERROR_INSTALL_FAILURE;
			} else if (IDRETRY == uiResponse) {
				continue;
			} else if (IDIGNORE == uiResponse) {
			}
		}

		break;
	}

	//
	// The following actions are non-critical ones
	//

	if (fSuccess) {

		MSILOGINFO(FSB256(_T("INF Copied %s to %s"), pszInfPath, szInstalledInf));

		//
		// set the registry value
		//
		fSuccess = pSetInstalledPath(
			HKEY_LOCAL_MACHINE, 
			pszRegPath, 
			pszRegValue, 
			szInstalledInf);

		if (!fSuccess) {
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
	RUN_MODE_TRACE();

	UINT uiReturn = 0;

	LPTSTR pszErrorDlg = NULL,
		pszHardwareID = NULL,
		pszInfPath = NULL,
		pszRegPath = NULL,
		pszRegValue = NULL;

	LPTSTR lpTokens = pParseMsiProperty(
		hInstall,
		CA_PROPERTY_NAME,
		PROP_DELIMITER,
		&pszErrorDlg,
		&pszHardwareID,
		&pszInfPath,
		&pszRegPath,
		&pszRegValue,
		NULL);

	if (NULL == lpTokens) {
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	AutoHLocal autoTokens = (HLOCAL) lpTokens;

	//
	// We do not update the existing device
	// It's MSI's responsibility to ensure that
	// any existing devices are already removed.
	//

	INT iErrorDlg = _ttoi(pszErrorDlg);
	TCHAR szInstalledInf[MAX_PATH] = {0};
	BOOL bRebootRequired = FALSE;
	BOOL fSuccess = FALSE;

	while (TRUE) {
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

		if (!fSuccess) {

			MSILOGERR_PROC(_T("NdasDiInstallRootEnumeratedDevice"));

			UINT uiResponse = pMsiErrorMessageBox(hInstall, iErrorDlg);

			// 0, IDABORT, IDRETRY, IDIGNORE
			if (IDABORT == uiResponse) {
				return ERROR_INSTALL_FAILURE;
			} else if (IDRETRY == uiResponse) {
				continue;
			} else if (IDIGNORE == uiResponse) {
			}
		}

		break;
	}

	//
	// The following actions are non-critical ones
	//

	if (fSuccess) {
		//
		// set the registry value
		//
		if (_T('\0') != szInstalledInf[0]) {
			fSuccess = pSetInstalledPath(
				HKEY_LOCAL_MACHINE, 
				pszRegPath, 
				pszRegValue, 
				szInstalledInf);

			if (!fSuccess) {
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
	RUN_MODE_TRACE();

	// CustomActionData: 
	// <ierrordialog>|<source_binary_path>|
	// <service_name>|<display_name>|<description>|
	// <service_type>|<start_type>|<error_control>|
	// <load_order_group>|<dependencies>; (semicolon delimited and should be terminated with ;)

	LPTSTR pszErrorDialog = NULL,
		pszBinaryPath = NULL,
		pszServiceName = NULL,
		pszDisplayName = NULL,
		pszDescription = NULL,
		pszServiceType = NULL,
		pszStartType = NULL,
		pszErrorControl = NULL,
		pszLoadOrderGroup = NULL,
		pszDependency = NULL;

	LPTSTR lpTokens = pParseMsiProperty(
		hInstall,
		CA_PROPERTY_NAME,
		PROP_DELIMITER,
		&pszErrorDialog,
		&pszBinaryPath,
		&pszServiceName,
		&pszDisplayName,
		&pszDescription,
		&pszServiceType,
		&pszStartType,
		&pszErrorControl,
		&pszLoadOrderGroup,
		&pszDependency,
		NULL);

	if (NULL == lpTokens) {
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	AutoHLocal autoTokens = (HLOCAL) lpTokens;

	INT iErrorDlg = _ttoi(pszErrorDialog);
	BOOL fSuccess = FALSE;

	DWORD dwServiceType = _ttol(pszServiceType);
	DWORD dwStartType = _ttol(pszStartType);
	DWORD dwErrorControl = _ttol(pszErrorControl);

	DBGPRT_INFO(_FT("lpTokens: %s\n"), lpTokens);
	DBGPRT_INFO(
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
		pszDependency);

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
		for (PTCHAR p = szDependencies; *p != _T('\0'); ++p) {
			if (_T(';') == *p) {
				*p = _T('\0');
			}
		}
	}

	BOOL bUpdated = FALSE;

	while (TRUE) {

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

		if (!fSuccess) {

			MSILOGERR_PROC(_T("NdasDiInstallRootEnumeratedDevice"));

			UINT uiResponse = pMsiErrorMessageBox(hInstall, iErrorDlg);

			// 0, IDABORT, IDRETRY, IDIGNORE
			if (IDABORT == uiResponse) {
				return ERROR_INSTALL_FAILURE;
			} else if (IDRETRY == uiResponse) {
				continue;
			} else if (IDIGNORE == uiResponse) {
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
	RUN_MODE_TRACE();

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
	RUN_MODE_TRACE();

	BOOL fSuccess = FALSE;
	UINT uiReturn = 0;

	LPTSTR pszPropName = NULL, 
		pszNetCompId = NULL;

	LPTSTR lpTokens = pParseMsiProperty(
		hInstall,
		CA_PROPERTY_NAME,
		PROP_DELIMITER,
		&pszPropName,
		&pszNetCompId,
		NULL);

	if (NULL == lpTokens) {
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	AutoHLocal autoTokens = (HLOCAL) lpTokens;

	HRESULT hr = HrIsNetComponentInstalled(pszNetCompId);
	if (S_OK == hr) {
		uiReturn = ::MsiSetProperty(hInstall, pszPropName, _T("1"));
		if (ERROR_SUCCESS != uiReturn) {
			return ERROR_INSTALL_FAILURE;
		}
	} else {
		uiReturn = ::MsiSetProperty(hInstall, pszPropName, _T("0"));
		if (ERROR_SUCCESS != uiReturn) {
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
	RUN_MODE_TRACE();

	BOOL fSuccess = FALSE;
	UINT uiReturn = 0;

	LPTSTR pszErrorDialog = NULL,
		pszOptionFlags = NULL;

	LPTSTR lpTokens = pParseMsiProperty(
		hInstall,
		CA_PROPERTY_NAME,
		PROP_DELIMITER,
		&pszErrorDialog,
		NULL);

	if (NULL == lpTokens) {
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	AutoHLocal autoTokens = (HLOCAL) lpTokens;

	INT iErrorDlg = _ttoi(pszErrorDialog);
	DBGPRT_INFO(_FT("iErrorDlg=%d\n"), iErrorDlg);

	INSTALLMESSAGE msgFlags =
		INSTALLMESSAGE(INSTALLMESSAGE_USER|MB_YESNO|MB_ICONQUESTION);

	UINT uiResponse = pMsiMessageBox(hInstall, iErrorDlg, msgFlags, _T(""));

	// 0, IDABORT, IDRETRY, IDIGNORE
	if (IDYES == uiResponse) {
		return ERROR_SUCCESS; 
	} else {
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
	RUN_MODE_TRACE();

	LPTSTR pszErrorDialog = NULL, 
		pszHardwareID = NULL;

	LPTSTR lpTokens = pParseMsiProperty(
		hInstall,
		CA_PROPERTY_NAME,
		PROP_DELIMITER,
		&pszErrorDialog,
		&pszHardwareID,
		NULL);

	if (NULL == lpTokens) {
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	AutoHLocal autoTokens = (HLOCAL) lpTokens;

	INT iErrorDlg = _ttoi(pszErrorDialog);
	DBGPRT_INFO(_FT("iErrorDlg=%d,HardwareID=%s\n"), iErrorDlg, pszHardwareID);

	BOOL fSuccess = FALSE;
	UINT uiReturn = 0;

	while (TRUE) {

		fSuccess = NdasDiFindExistingDevice(
			NULL, 
			pszHardwareID, 
			TRUE);

		if (!fSuccess) {
			break;
		}

		if (0 != iErrorDlg) {

			INSTALLMESSAGE msgFlags =
				INSTALLMESSAGE(INSTALLMESSAGE_USER|MB_RETRYCANCEL|MB_ICONWARNING);

			UINT uiResponse = pMsiMessageBox(hInstall, iErrorDlg, msgFlags, _T(""));

			// 0, IDABORT, IDRETRY, IDIGNORE
			if (IDCANCEL == uiResponse) {
				return ERROR_INSTALL_USEREXIT;
			} else { // IDRETRY == uiResponse
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
	RUN_MODE_TRACE();

	BOOL fSuccess = FALSE;
	UINT uiReturn = 0;

	LPTSTR pszPropName = NULL, 
		pszHardwareID = NULL,
		pszPresentOnly = NULL;

	LPTSTR lpTokens = pParseMsiProperty(
		hInstall,
		CA_PROPERTY_NAME,
		PROP_DELIMITER,
		&pszPropName,
		&pszHardwareID,
		&pszPresentOnly,
		NULL);

	if (NULL == lpTokens) {
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	AutoHLocal autoTokens = (HLOCAL) lpTokens;

	BOOL fPresentOnly = _ttoi(pszPresentOnly);

	fSuccess = NdasDiFindExistingDevice(
		NULL, 
		pszHardwareID, 
		fPresentOnly);

	if (fSuccess) {
		uiReturn = ::MsiSetProperty(hInstall, pszPropName, _T("1"));
		if (ERROR_SUCCESS != uiReturn) {
			return ERROR_INSTALL_FAILURE;
		}
	} else {
		uiReturn = ::MsiSetProperty(hInstall, pszPropName, _T("0"));
		if (ERROR_SUCCESS != uiReturn) {
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
	RUN_MODE_TRACE();

	BOOL fSuccess = FALSE;
	UINT uiReturn = 0;

	LPTSTR pszPropName = NULL;
	LPTSTR pszPendingDeletionPropName = NULL;
	LPTSTR pszServiceName = NULL;

	LPTSTR lpTokens = pParseMsiProperty(
		hInstall,
		CA_PROPERTY_NAME,
		PROP_DELIMITER,
		&pszPropName,
		&pszPendingDeletionPropName,
		&pszServiceName,
		NULL);

	if (NULL == lpTokens) {
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	AutoHLocal autoTokens = (HLOCAL) lpTokens;

	BOOL fPendingDeletion;
	fSuccess = NdasDiFindService(pszServiceName, &fPendingDeletion);
	if (fSuccess) {
	
		uiReturn = ::MsiSetProperty(hInstall, pszPropName, _T("1"));

		// This is an optional feature to ignore error
		(VOID) ::MsiSetProperty(hInstall, 
			pszPendingDeletionPropName, 
			fPendingDeletion ? _T("1") : _T("0"));

		if (ERROR_SUCCESS != uiReturn) {
			return ERROR_INSTALL_FAILURE;
		}
	} else {
		uiReturn = ::MsiSetProperty(hInstall, pszPropName, _T("0"));
		if (ERROR_SUCCESS != uiReturn) {
			return ERROR_INSTALL_FAILURE;
		}
	}

	return ERROR_SUCCESS;
}

UINT 
__stdcall
NDMsiSchedulePreboot(MSIHANDLE hInstall)
{
	if (!pMsiSchedulePreboot(hInstall))
	{
		MSILOGERR_PROC(_T("pMsiSchedulePreboot"));
		return ERROR_INSTALL_FAILURE;
	}
	return ::MsiSetProperty(hInstall, _T("NDASCustomActionPrebootScheduled"), _T("1"));
}

// This is an immediate action!
UINT
__stdcall
NDMsiSetPrebootScheduled(MSIHANDLE hInstall)
{
	if (pIsPrebootScheduled())
	{
		MSILOGINFO(_T("Preboot is scheduled"));
		UINT uiRet = ::MsiSetProperty(
			hInstall, 
			_T("NdasCustomActionPrebootScheduled"), _T("1"));
		if (uiRet != ERROR_SUCCESS)
		{
			MSILOGERR_PROC(_T("MsiSetProperty")
				_T("(\"NdasCustomActionPrebootScheduled\",\"1\")"));
			return ERROR_INSTALL_FAILURE;
		}

	}
	MSILOGINFO(_T("Preboot is not scheduled"));
	return ERROR_SUCCESS;
}
//
// Deferred Custom Action to start a service if there is no deferred reboot
//

BOOL
pIsDeferredRebootScheduled(MSIHANDLE hInstall)
{
	HKEY hKey = (HKEY) INVALID_HANDLE_VALUE;
	LONG lResult = ::RegOpenKeyEx(
		HKEY_LOCAL_MACHINE, 
		_T("Software\\NDAS\\DeferredInstallReboot"),
		0,
		KEY_READ,
		&hKey);

	if (ERROR_SUCCESS != lResult)
	{
		// Not Scheduled!
		MSILOGINFO(_T("No deferred reboot."));
		return FALSE;
	}

	MSILOGINFO(_T("Deferred reboot is scheduled."));
	return TRUE;
}

UINT 
__stdcall
NDMsiStartServiceOnNoDeferredReboot(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE();

	if (pIsDeferredRebootScheduled(hInstall))
	{
		MSILOGINFO(_T("Skipped this action."));
		return ERROR_SUCCESS;
	}

	//
	// Parse Custom Action Data
	//

	LPCTSTR lpServiceName = NULL;

	LPTSTR lpTokens = pParseMsiProperty(
		hInstall,
		CA_PROPERTY_NAME,
		PROP_DELIMITER,
		&lpServiceName,
		NULL);

	if (NULL == lpTokens) {
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	MSILOGINFO(FSB256(_T("ServiceName: %s"), lpServiceName));

	AutoHLocal autoLocal = (HLOCAL) lpTokens;

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
	RUN_MODE_TRACE();

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
	RUN_MODE_TRACE();

	//
	// Parse Custom Action Data
	//

	LPTSTR pszErrorDialog = NULL;
	LPTSTR pszCompId = NULL,
		pszServiceName = NULL,
		pszDriverFileName = NULL;

	LPTSTR lpTokens = pParseMsiProperty(
		hInstall,
		CA_PROPERTY_NAME,
		PROP_DELIMITER,
		&pszErrorDialog,
		&pszCompId,
		&pszServiceName,
		&pszDriverFileName,
		NULL);

	if (NULL == lpTokens) {
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	AutoHLocal autoTokens = (HLOCAL) lpTokens;

	INT iErrorDialog = _ttoi(pszErrorDialog);

	MSILOGINFO(FSB256(
		_T("Component Id: %s, Service Name: %s, Driver File Name: %s"), 
		pszCompId, pszServiceName, pszDriverFileName));

	if (_T('\0') == pszCompId) {
		MSILOGERR(_T("Component ID not specified"));
		return ERROR_INSTALL_FAILURE;
	}

	HRESULT hr = E_FAIL;
	BOOL fRebootRequired = FALSE;

	while (TRUE) {

		hr = HrIsNetComponentInstalled(pszCompId);
		if (S_OK != hr) {
			MSILOGINFO(FSB256(_T("NetComp(%s) is not installed, skipped"), pszCompId));
			break;
		}

		hr = HrUninstallNetComponent(pszCompId);

		if (FAILED(hr)) {
			if (0 == iErrorDialog) {
				MSILOGERR_PROC2(FSB256(_T("HrUninstallNetComponent(%s)"), pszCompId), hr);
				return ERROR_INSTALL_FAILURE;
			}
			UINT uiResponse = pMsiErrorMessageBox(hInstall, iErrorDialog);
			if (IDABORT == uiResponse) {
				return ERROR_INSTALL_FAILURE;
			} else if (IDRETRY == uiResponse) {
				continue;
			} else if (IDIGNORE == uiResponse) {
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
	if (_T('\0') != pszServiceName) {
		pFindAndDeleteService(pszServiceName, &fRebootRequired);
	}

	//
	// Delete File
	//
	if (_T('\0') != pszDriverFileName) {
		pFindAndDeleteDriverFile(pszDriverFileName, &fRebootRequired);
	}

	DBGPRT_INFO(_FT("Reboot required: %d\n"), fRebootRequired);

	if (fRebootRequired) {
		if (dwFlags & FLG_MSI_SCHEDULE_PREBOOT) {
			pMsiSchedulePreboot(hInstall);
		} else {
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
	RUN_MODE_TRACE();

	BOOL fSuccess = FALSE;
	UINT uiReturn = 0;

	//
	// Parse Custom Action Data
	//

	LPTSTR pszErrorDialog = NULL,
		pszHardwareId = NULL,
		pszServiceName = NULL,
		pszDriverFileName = NULL;

	LPTSTR lpTokens = pParseMsiProperty(
		hInstall,
		CA_PROPERTY_NAME,
		PROP_DELIMITER,
		&pszErrorDialog,
		&pszHardwareId,
		&pszServiceName,
		&pszDriverFileName,
		NULL);

	if (NULL == lpTokens) {
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	AutoHLocal autoTokens = (HLOCAL) lpTokens;

	INT iErrorDialog = _ttoi(pszErrorDialog);

	MSILOGINFO(FSB256(
		_T("Hardware Id: %s, Service Name: %s, Driver File Name: %s"), 
		pszHardwareId, 
		pszServiceName,
		pszDriverFileName));

	if (_T('\0') == pszHardwareId) {
		MSILOGERR(_T("Hardware ID not specified"));
		return ERROR_INSTALL_FAILURE;
	}

	DWORD cRemoved;
	BOOL fRebootRequired = FALSE;

	while (TRUE) {

		fSuccess = NdasDiRemoveDevice(
			NULL, 
			pszHardwareId,
			FALSE,
			&cRemoved,
			&fRebootRequired,
			NULL,
			NULL);

		if (!fSuccess && 0 != iErrorDialog) {
			MSILOGERR_PROC(FSB256(_T("NdasDiRemoveDevice(%s)"), pszHardwareId));
			UINT uiResponse = pMsiErrorMessageBox(hInstall, iErrorDialog);
			// 0, IDABORT, IDRETRY, IDIGNORE
			if (IDABORT == uiResponse) {
				return ERROR_INSTALL_FAILURE;
			} else if (IDRETRY == uiResponse) {
				continue;
			} else if (IDIGNORE == uiResponse) {
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
	RUN_MODE_TRACE();

	BOOL fSuccess = FALSE;
	UINT uiReturn = 0;

	//
	// Parse Custom Action Data
	//

	LPTSTR pszErrorDialog = NULL,
		pszServiceName = NULL,
		pszDriverFileName = NULL;

	LPTSTR lpTokens = pParseMsiProperty(
		hInstall,
		CA_PROPERTY_NAME,
		PROP_DELIMITER,
		&pszErrorDialog,
		&pszServiceName,
		&pszDriverFileName,
		NULL);

	if (NULL == lpTokens) {
		MSILOGERR(_T("Cannnot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	AutoHLocal autoTokens = (HLOCAL) lpTokens;

	INT iErrorDialog = _ttoi(pszErrorDialog);

	DBGPRT_INFO(_FT("Service Name: %s, Driver File Name: %s\n"), 
		pszServiceName,
		pszDriverFileName);

	pMsiLogMessageEx(hInstall, LMM_INFO, TFN,
		_T("Service Name: %s, Driver File Name: %s"), 
		pszServiceName, pszDriverFileName);

	if (_T('\0') == pszServiceName) {
		pMsiLogMessage(hInstall, LMM_ERROR, TFN, _T("Component ID not specified"));
		return ERROR_INSTALL_FAILURE;
	}

	DWORD cRemoved;
	BOOL fRebootRequired = FALSE;

	//
	// TODO: Provide Error?
	//

	while (TRUE) {

		//
		// Remove Service
		//
		fSuccess = pFindAndDeleteService(pszServiceName, &fRebootRequired);

		if (!fSuccess) {

			UINT uiResponse = pMsiErrorMessageBox(hInstall, iErrorDialog);

			// 0, IDABORT, IDRETRY, IDIGNORE
			if (IDABORT == uiResponse) {
				return ERROR_INSTALL_FAILURE;
			} else if (IDRETRY == uiResponse) {
				continue;
			} else if (IDIGNORE == uiResponse) {
			}
		}

		break;
	}

	if (fSuccess) {

		//
		// Delete File
		//
		if (_T('\0') != pszDriverFileName) {
			pFindAndDeleteDriverFile(pszDriverFileName, &fRebootRequired);
		}
	}

	DBGPRT_INFO(_FT("Reboot required: %d\n"), fRebootRequired);

	if (fRebootRequired) {
		if (FLG_MSI_SCHEDULE_PREBOOT & dwFlags) {
			pMsiSchedulePreboot(hInstall);
		} else {
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
	MSIHANDLE* phInstall = reinterpret_cast<MSIHANDLE*>(lpContext);

	// logging only, not interactive
	if (ERROR_SUCCESS == dwError) {
		pMsiLogMessageEx(*phInstall, LMM_ERROR, TFN, 
			_T("Deleted: %s\\%s"), szPath, szFileName);
	} else {
		pMsiLogMessageEx(*phInstall, LMM_ERROR, TFN, 
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
	RUN_MODE_TRACE();

	BOOL fSuccess = FALSE;
	UINT uiReturn = 0;

	//
	// Parse Custom Action Data
	//

	LPTSTR pszErrorDialog = NULL,
		pszFindSpec = NULL,
		pszHardwareIDList = NULL;

	LPTSTR lpTokens = pParseMsiProperty(
		hInstall,
		CA_PROPERTY_NAME,
		PROP_DELIMITER,
		&pszErrorDialog,
		&pszFindSpec,
		&pszHardwareIDList,
		NULL);

	if (NULL == lpTokens) {
		MSILOGERR(_T("Cannnot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	AutoHLocal autoTokens = (HLOCAL) lpTokens;

	INT iErrorDialog = _ttoi(pszErrorDialog);

	DBGPRT_INFO(_FT("FindSpec: %s, Hardware ID List: %s\n"), 
		pszFindSpec,
		pszHardwareIDList);

	pMsiLogMessageEx(hInstall, LMM_INFO, TFN,
		_T("FindSpec: %s, Hardware ID List: %s"), 
		pszFindSpec, 
		pszHardwareIDList);

	if (_T('\0') == pszHardwareIDList) {
		pMsiLogMessage(hInstall, LMM_ERROR, TFN, _T("Hardware ID List not specified"));
		// Ignore this action in case of no hardware id specified
		return ERROR_SUCCESS;
	}

	// convert hwid1;hwid2;\0 to hwid1\0hwid2\0\0
	PTCHAR pch = pszHardwareIDList;
	while (_T('\0') != *pch) {
		if (_T(';') == *pch) {
			*pch = _T('\0');
			++pch;
		}
		++pch;
	}

	while (TRUE) {

		fSuccess = ::NdasDiDeleteOEMInf(
			pszFindSpec,
			pszHardwareIDList,
			0x0001, // SUOI_FORCEDELETE,
			pNdasDiDeleteOEMInfCallback,
			(LPVOID) &hInstall);

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
	RUN_MODE_TRACE();

	pMsiLogMessage(hInstall, LMM_INFO, TFN, _T("Started"));
	w32sys::RefreshShellIconCache();
	return ERROR_SUCCESS;
}

//
// Set Encryption System Key
//
// Commit Custom Action
//
// Custom Action Data contains the path to the key file
// If the file exists, This function tries to import the key file.
// This function always returns ERROR_SUCCESS.
// In case of error, only the log records are generated.
//

UINT 
__stdcall
NDMsiSetECFlags(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE();

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
		return ERROR_SUCCESS;
	}

	pMsiLogMessageEx(hInstall, LMM_INFO, TFN,
		_T("Importing SysKey file from: %s\n"), szProp);

	uiRet = ::NdasEncImportSysKeyFromFile(szProp);

	if (ERROR_SUCCESS != uiRet) 
	{
		pMsiLogMessageEx(hInstall, LMM_INFO, TFN,
			_T("Importing SysKey from %s failed: RetCode %08X, Error Code %08X, error ignored"),
			szProp, uiRet, ::GetLastError());
		return ERROR_SUCCESS;
	}

	pMsiLogMessageEx(hInstall, LMM_INFO, TFN, 
		_T("SysKey imported from %s successfully\n"), szProp);

	return ERROR_SUCCESS;
}

//
// Set the auto registration flag
//
// Commit Custom Action
//
UINT 
__stdcall
NDMsiSetARFlags(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE();

	const static NDAS_DEVICE_ID REDDOTNET_ALL_AR_BEGIN = { 0x00, 0x0B, 0xD0, 0x27, 0x60, 0x00 };
	const static NDAS_DEVICE_ID REDDOTNET_ALL_AR_END =	{ 0x00, 0x0B, 0xD0, 0x27, 0x9F, 0xFF };
	const static ACCESS_MASK REDDOTNET_ALL_AR_ACCESS = 	GENERIC_READ | GENERIC_WRITE;

	const static NDAS_DEVICE_ID REDDOTNET_A_AR_BEGIN =	{ 0x00, 0x0B, 0xD0, 0x27, 0x80, 0x00 };
	const static NDAS_DEVICE_ID REDDOTNET_A_AR_END =	{ 0x00, 0x0B, 0xD0, 0x27, 0x9F, 0xFF };
	const static ACCESS_MASK REDDOTNET_A_AR_ACCESS = 	GENERIC_READ | GENERIC_WRITE;

	const static NDAS_DEVICE_ID REDDOTNET_B_AR_BEGIN =	{ 0x00, 0x0B, 0xD0, 0x27, 0x60, 0x00 };
	const static NDAS_DEVICE_ID REDDOTNET_B_AR_END =	{ 0x00, 0x0B, 0xD0, 0x27, 0x7F, 0xFF };
	const static ACCESS_MASK REDDOTNET_B_AR_ACCESS =	GENERIC_READ | GENERIC_WRITE;

	CNdasAutoRegScopeData arsData;

	TCHAR szProp[30] = {0};
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
		return ERROR_SUCCESS;
	}

	//
	// For setup products, Auto Registration Scopes are hard-coded
	// by the OEM partners to prevent tampering with these values
	//

	NDAS_DEVICE_ID scopeBegin, scopeEnd;
	ACCESS_MASK grantedAccess;

	if (0 == ::lstrcmpi(_T("REDDOTNET_A"), szProp)) 
	{
		scopeBegin = REDDOTNET_A_AR_BEGIN;
		scopeEnd = REDDOTNET_A_AR_END;
		grantedAccess = REDDOTNET_A_AR_ACCESS;
	}
	else if (0 == ::lstrcmpi(_T("REDDOTNET_B"), szProp)) 
	{
		scopeBegin = REDDOTNET_B_AR_BEGIN;
		scopeEnd = REDDOTNET_B_AR_END;
		grantedAccess = REDDOTNET_B_AR_ACCESS;
	}
	else 
	{
		// ignore invalid AR flags
		pMsiLogMessageEx(hInstall, LMM_INFO, TFN,
			_T("Unknown Scope Name: %s ignored."), szProp);
		return ERROR_SUCCESS;
	}

	BOOL fSuccess = arsData.AddScope(scopeBegin, scopeEnd, grantedAccess);

	if (!fSuccess) 
	{
		pMsiLogMessageEx(hInstall, LMM_INFO, TFN,
			_T("arsData.AddScope failed: Error Code %08X, error ignored"),
			::GetLastError());
		return ERROR_SUCCESS;
	}

	fSuccess = arsData.SaveToSystem();
	if (!fSuccess) 
	{
		pMsiLogMessageEx(hInstall, LMM_INFO, TFN,
			_T("arsData.SaveToSystem failed: Error Code %08X, error ignored"),
			::GetLastError());
	}

	pMsiLogMessageEx(hInstall, LMM_INFO, TFN, 
		_T("ARS Data is set to %s successfully\n"), szProp);

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
	RUN_MODE_TRACE();
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
	RUN_MODE_TRACE();
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
	RUN_MODE_TRACE();

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
	RUN_MODE_TRACE();

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

BOOL WINAPI
pSetInstalledPath(
	HKEY hKeyRoot, 
	LPCTSTR szRegPath, 
	LPCTSTR szValue, 
	LPCTSTR szData)
{
	HKEY hKey = NULL;
	DWORD dwDisp;
	LONG lResult = RegCreateKeyEx(
		hKeyRoot, 
		szRegPath, 
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_ALL_ACCESS,
		NULL,
		&hKey,
		&dwDisp);

	//
	// Favor autoresource
	//

	AutoHKey hKeyManaged = hKey;

	if (ERROR_SUCCESS != lResult) {
		DPErrorEx2(lResult, _FT("Creating a reg key %s failed: "), szRegPath);
		return FALSE;
	}

	lResult = RegSetValueEx(
		hKey, 
		szValue, 
		0, 
		REG_SZ, 
		(const BYTE*) szData, 
		(lstrlen(szData) + 1) * sizeof(TCHAR));

	if (ERROR_SUCCESS != lResult) {
		DPErrorEx2(lResult, _FT("Setting a value %s:%s to %s failed: "),
			szRegPath, szValue, szData);
		return FALSE;
	}

	DPInfo(_FT("RegValueSet %s:%s to %s,\n"), szRegPath, szValue, szData);

	return TRUE;
}

BOOL WINAPI
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
	RUN_MODE_TRACE();

	BOOL fSuccess = FALSE;

	//
	// Parse Custom Action Data
	//

	LPTSTR pszServiceName = NULL;

	LPTSTR lpTokens = pParseMsiProperty(
		hInstall,
		CA_PROPERTY_NAME,
		PROP_DELIMITER,
		&pszServiceName,
		NULL);

	if (NULL == lpTokens) {
		MSILOGERR(_T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	AutoHLocal autoTokens = (HLOCAL) lpTokens;

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

	MSILOGINFO(_T("Service is NOT marked for deletion."));

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

UINT
pMsiScheduleRebootForDeferredCA(MSIHANDLE hInstall)
{
	UNREFERENCED_PARAMETER(hInstall);

	HKEY hKey = (HKEY) INVALID_HANDLE_VALUE;
	
	// Create a key for scheduled reboot
	LONG lResult = ::RegCreateKeyEx(
		HKEY_LOCAL_MACHINE, 
		_T("Software\\NDAS\\DeferredInstallReboot"),
		0,
		NULL,
		REG_OPTION_VOLATILE, // volatile key will be deleted after reboot
		KEY_ALL_ACCESS,
		NULL,
		&hKey,
		NULL);

	(VOID) ::RegCloseKey(hKey);

	return (UINT) lResult;
}

UINT
pMsiScheduleReboot(MSIHANDLE hInstall)
{
	DBGPRT_INFO(_FT("pMsiScheduleReboot: 1\n"));

	UINT uiReturn = MsiDoAction(hInstall, _T("ScheduleReboot"));
	if (ERROR_SUCCESS != uiReturn) 
	{
		pMsiLogError(hInstall, TFN, _T("MsiDoAction(ScheduleReboot)"), uiReturn);
	}
	return uiReturn;
}

static CONST LPCTSTR FORCE_REBOOT_REG_KEY = _T("Software\\NDASSetup\\PrebootScheduled");

BOOL 
WINAPI
pMsiSchedulePreboot(MSIHANDLE hInstall)
{
	HKEY hKey;

	//
	// This key is a volatile key, which will be delete on reboot
	//
	LONG lResult = ::RegCreateKeyEx(
		HKEY_LOCAL_MACHINE,
		FORCE_REBOOT_REG_KEY,
		0,
		NULL,
		REG_OPTION_VOLATILE,
		KEY_ALL_ACCESS,
		NULL,
		&hKey,
		NULL);

	if (ERROR_SUCCESS != lResult) {
		DBGPRT_ERR_EX(_FT("Preboot schedule failed: "));
		::SetLastError(lResult);
		return FALSE;
	}

	DBGPRT_INFO(_FT("Preboot scheduled.\n"));

	::RegCloseKey(hKey);
	return TRUE;
}

BOOL 
WINAPI
pIsPrebootScheduled()
{
	HKEY hKey;
	LONG lResult = ::RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		FORCE_REBOOT_REG_KEY,
		0,
		KEY_READ,
		&hKey);

	if (ERROR_SUCCESS != lResult) {
		return FALSE;
	}

	::RegCloseKey(hKey);
	return TRUE;
}


//
// Caller is responsible to free the returned string 
// with LocalFree if not null
//
LPTSTR 
pGetMsiProperty(
	MSIHANDLE hInstall,
	LPCTSTR szPropName,
	LPDWORD pcchData)
{
	UINT uiReturn = 0;
	LPTSTR lpszProperty = NULL;
	DWORD cchProperty = 0;

	uiReturn = MsiGetProperty(
		hInstall, 
		szPropName,
		_T(""),
		&cchProperty);

	switch (uiReturn) {
	case ERROR_INVALID_HANDLE:
	case ERROR_INVALID_PARAMETER:
		pMsiLogError(hInstall, TFN, _T("MsiGetProperty"), uiReturn);
		return NULL;
	}

	// on output does not include terminating null, so add 1
	++cchProperty;

	lpszProperty = (LPTSTR) ::LocalAlloc(
		LPTR, 
		(cchProperty) * sizeof(TCHAR));

	uiReturn = ::MsiGetProperty(
		hInstall, 
		szPropName,
		lpszProperty, 
		&cchProperty);

	switch (uiReturn) {
	case ERROR_INVALID_HANDLE:
	case ERROR_INVALID_PARAMETER:
	case ERROR_MORE_DATA:
		pMsiLogError(hInstall, TFN, _T("MsiGetProperty"), uiReturn);
		return NULL;
	}

	pMsiLogMessageEx(hInstall, LMM_INFO, TFN, 
		_T("Prop %s: %s"), szPropName, lpszProperty);

	*pcchData = cchProperty;
	return lpszProperty;
}

LPTSTR
pParseMsiProperty(
	MSIHANDLE hInstall,
	LPCTSTR szPropName,
	CONST TCHAR cchDelimiter,
	...)
{
	BOOL fSuccess = FALSE;
	UINT uiReturn = 0;
	LPTSTR lpszProperty = NULL;
	DWORD cchProperty = 0;

	lpszProperty = pGetMsiProperty(hInstall, szPropName, &cchProperty);

	if (NULL == lpszProperty) {
		// pGetMsiProperty logs the error
		return NULL;
	}

	// attaching to the auto resource
	AutoHLocal hLocal = (HLOCAL) lpszProperty;

	//
	// Parse Custom Action Data
	//
	va_list ap;
	va_start(ap, cchDelimiter);
	LPTSTR lpTokens = pGetTokensV(
		lpszProperty, cchDelimiter, ap);
	va_end(ap);

	if (NULL == lpTokens) {
		MSILOGERR(_T("Cannnot parse tokens"));
		return NULL;
	}

	return lpTokens;
}

UINT 
pMsiMessageBox(
	MSIHANDLE hInstall,
	INT iErrorDialog,
	INSTALLMESSAGE Flags,
	LPCTSTR szText)
{
	PMSIHANDLE hActionRec = MsiCreateRecord(4);
	MsiRecordSetInteger(hActionRec, 1, iErrorDialog);

	//
	// (Workaround)
	// Send [4] as "\n" for more pretty text format.
	// Error Dialog Text property cannot contain "\n".
	// 

	MsiRecordSetString(hActionRec, 4, _T("\n"));
	MsiRecordSetString(hActionRec, 2, szText);

	UINT uiResponse = MsiProcessMessage(
		hInstall, 
		Flags,
		hActionRec);

	return uiResponse;
}

UINT 
pMsiErrorMessageBox(
	MSIHANDLE hInstall, 
	INT iErrorDialog, 
	DWORD dwError,
	LPCTSTR szErrorText)
{
	TCHAR szErrorCodeText[255];

	HRESULT hr = StringCchPrintf(szErrorCodeText, 255, _T("0x%08X"), dwError);

	PMSIHANDLE hActionRec = MsiCreateRecord(4);
	MsiRecordSetInteger(hActionRec, 1, iErrorDialog);

	//
	// (Workaround)
	// Send [4] as "\n" for more pretty text format.
	// Error Dialog Text property cannot contain "\n".
	// 

	MsiRecordSetString(hActionRec, 4, _T("\n"));
	MsiRecordSetString(hActionRec, 2, szErrorCodeText);

	if (NULL == szErrorText) {
		MsiRecordSetString(hActionRec, 3, _T("(Description not available)"));
	} else {
		MsiRecordSetString(hActionRec, 3, szErrorText);
	}

	UINT uiResponse = MsiProcessMessage(
		hInstall, 
		INSTALLMESSAGE(INSTALLMESSAGE_USER|MB_ABORTRETRYIGNORE|MB_ICONWARNING),
		hActionRec);

	return uiResponse;

}

UINT 
pMsiErrorMessageBox(
	MSIHANDLE hInstall, 
	INT iErrorDialog, 
	DWORD dwError)
{
	LPTSTR szErrorText = pGetSystemErrorText(dwError);

	UINT uiResponse = pMsiErrorMessageBox(
		hInstall, 
		iErrorDialog, 
		dwError, 
		szErrorText);

	if (szErrorText) {
		LocalFree((HLOCAL)szErrorText);
	}

	return uiResponse;
}

static
UINT
pMsiEnableCancelButton(
	MSIHANDLE hInstall,
	BOOL bEnable);

UINT
pMsiEnableCancelButton(
	MSIHANDLE hInstall,
	BOOL bEnable)
{
	PMSIHANDLE hActionRec = ::MsiCreateRecord(2);

	if (NULL == hActionRec)
	{
		return ERROR_INSTALL_FAILURE;
	}

	//
	// 0 : unused
	// 1 : 2 refers to the Cancel button
	// 2 : 1 indicates the Cancel button should be visible
	//     0 indicates the Cancel button should be invisible
	//
	::MsiRecordSetInteger(hActionRec, 1, 2);
	::MsiRecordSetInteger(hActionRec, 2, bEnable ? 1 : 0);
	::MsiProcessMessage(hInstall, INSTALLMESSAGE_COMMONDATA, hActionRec);

	return ERROR_SUCCESS;
}

UINT
__stdcall
NDMsiEnableCancelButton(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE();

	return pMsiEnableCancelButton(hInstall, TRUE);
}

UINT
__stdcall
NDMsiDisableCancelButton(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE();

	return pMsiEnableCancelButton(hInstall, FALSE);
}


struct AutoFindFileHandleConfig
{
	static HANDLE GetInvalidValue() 
	{ 
		return (HANDLE)INVALID_HANDLE_VALUE; 
	}
	static void Release(HANDLE h)
	{ 
		DWORD dwError = ::GetLastError();
		BOOL fSuccess = ::FindClose(h);
		_ASSERTE(fSuccess);
		::SetLastError(dwError);
	}
};

typedef AutoResourceT<HANDLE,AutoFindFileHandleConfig> AutoFindFileHandle;


VOID
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
NDMsiCleanupSourceDirectory(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE();

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

UINT
__stdcall
NDMsiChangeServiceStartType(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE();

	//
	// Parse Custom Action Data
	//
	LPCTSTR 
		lpServiceName = NULL,
		lpServiceStartType = NULL;

	LPTSTR lpTokens = pParseMsiProperty(
		hInstall,
		CA_PROPERTY_NAME,
		PROP_DELIMITER,
		&lpServiceName,
		&lpServiceStartType,
		NULL);

	if (NULL == lpTokens) {
		MSILOGERR(_T("Cannnot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	DWORD dwServiceStartType = 0;
	BOOL fConverted = ::StrToIntEx(
		lpServiceStartType, 
		STIF_SUPPORT_HEX, 
		(int*)&dwServiceStartType);

	if (!fConverted)
	{
		MSILOGERR(FSB256(_T("Invalid ServiceStartType: %s"), lpServiceStartType));
		return ERROR_INSTALL_FAILURE;
	}

	AutoHLocal autoLocal = (HLOCAL) lpTokens;

	MSILOGINFO(FSB256(
		_T("ServiceName: %s, ServiceStartType: %d"), 
		lpServiceName, dwServiceStartType));

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
		hService, SERVICE_NO_CHANGE, dwServiceStartType, SERVICE_NO_CHANGE,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL);

	if (!fSuccess)
	{
		MSILOGERR_PROC(FSB256(
			_T("ChangeServiceConfig(%s,StartType:%d)"), 
			lpServiceName, dwServiceStartType));
		return ERROR_INSTALL_FAILURE;
	}

	MSILOGINFO(FSB256(
		_T("Service(%s) start type is changed to %d"), 
		lpServiceName, dwServiceStartType));

	return ERROR_SUCCESS;
}
