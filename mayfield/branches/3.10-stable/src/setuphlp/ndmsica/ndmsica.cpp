#include "stdafx.h"
#include "autores.h"

#include <netcfgx.h>
#include <setupapi.h>
#include "netcomp.h"
#include "pnpdevinst.h"
#include "svcinst.h"
#include "shiconcache.h"
#include "ndas/ndasautoregscope.h"
#include "ndas/ndascntenc.h"

#define XDBG_FILENAME "ndmsica.cpp"
#include "xdebug.h"

#define TFN _T(__FUNCTION__)

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

typedef enum _LOGMSIMESSAGE_TYPE {
	LMM_ERROR,
	LMM_INFO,
	LMM_WARNING
} LOGMSIMESSAGE_TYPE;

static VOID 
pMsiLogMessage(
	MSIHANDLE hInstall,
	LOGMSIMESSAGE_TYPE Type,
	LPCTSTR szSource,
	LPCTSTR szMessage);

static VOID 
pMsiLogError(
	MSIHANDLE hInstall, 
	LPCTSTR szSource, 
	LPCTSTR szCallee,
	DWORD dwError = GetLastError());

static VOID 
pMsiLogMessage(
	MSIHANDLE hInstall,
	LOGMSIMESSAGE_TYPE Type,
	LPCTSTR szSource,
	LPCTSTR szMessage);

static VOID 
pMsiLogMessageEx(
	MSIHANDLE hInstall,
	LOGMSIMESSAGE_TYPE Type,
	LPCTSTR szSource,
	LPCTSTR szFormat,
	...);

static
LPTSTR 
pGetSystemErrorText(DWORD dwError);

static LPCTSTR 
pGetNextToken(
	LPCTSTR lpszStart, 
	CONST TCHAR chToken);

static LPTSTR 
pGetTokensV(
	LPCTSTR szData, 
	CONST TCHAR chToken, 
	va_list ap);

static LPTSTR 
pGetTokens(
	LPCTSTR szData,
	CONST TCHAR chToken, 
	...);

static NetClass 
pGetNetClass(LPCTSTR szNetClass);

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

//
// Send Dummy Action Data to display the ActionText Template
//
int
pMsiDummyActionData(MSIHANDLE hInstall)
{
	PMSIHANDLE pMsiRecord = ::MsiCreateRecord(2);

	UINT uiRet = ::MsiRecordSetString(pMsiRecord, 0, _T(""));
	_ASSERTE(ERROR_SUCCESS == uiRet);
	uiRet = ::MsiRecordSetString(pMsiRecord, 1, _T(""));
	_ASSERTE(ERROR_SUCCESS == uiRet);

	return ::MsiProcessMessage(hInstall, INSTALLMESSAGE_ACTIONDATA, pMsiRecord);
}

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

#define RUN_MODE_TRACE(hInstall)  \
	do { \
	if (::MsiGetMode(hInstall,MSIRUNMODE_ROLLBACK)) { \
		pMsiLogMessage(hInstall, LMM_INFO, TFN, _T("Rollback Action Started")); \
	} else if (::MsiGetMode(hInstall,MSIRUNMODE_COMMIT)) { \
		pMsiLogMessage(hInstall, LMM_INFO, TFN, _T("Commit Action Started")); \
	} else if (::MsiGetMode(hInstall,MSIRUNMODE_SCHEDULED)) { \
		pMsiLogMessage(hInstall, LMM_INFO, TFN, _T("Scheduled Action Started")); \
	} else { \
		pMsiLogMessage(hInstall, LMM_INFO, TFN, _T("Action Started")); \
	} } while(0)

// Deferred Custom Action
UINT 
__stdcall 
NDMsiInstallNetComponent(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	BOOL fSuccess = FALSE;
	UINT uiReturn = 0;

	(VOID) pMsiDummyActionData(hInstall);

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
		pMsiLogMessage(hInstall, LMM_ERROR, TFN, _T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	AutoHLocal autoTokens = (HLOCAL) lpTokens;

	INT iErrorDialog = _ttoi(pszErrorDialog);

	NetClass nc = pGetNetClass(pszNetClass);
	if (NC_Unknown == nc) {
		pMsiLogMessageEx(hInstall, LMM_ERROR, TFN, 
			_T("Invalid Net Class: %s"), pszNetClass);
		return ERROR_INSTALL_FAILURE;
	}

	if (_T('\0') == pszCompId) {
		pMsiLogMessage(hInstall, LMM_ERROR, TFN, _T("Component ID not specified"));
		return ERROR_INSTALL_FAILURE;
	}

	if (_T('\0') == pszInfPath) {
		pszInfPath = NULL;
	}

	pMsiLogMessageEx(hInstall, LMM_INFO, TFN,
		_T("Net Class: %s, Component Id: %s, InfPath: %s, RegPath: %s, RegValue %s"),
		pszNetClass, pszCompId, pszInfPath, pszRegPath, pszRegValue);

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
		pMsiLogError(hInstall, TFN, _T("pSetInstalledPath"));
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

	UINT uiReturn = 0;

	(VOID) pMsiDummyActionData(hInstall);

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
		pMsiLogMessage(hInstall, LMM_ERROR, TFN, _T("Cannot parse tokens"));
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

			pMsiLogError(hInstall, TFN, _T("NdasDiCopyOEMInf"));

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

		pMsiLogMessageEx(hInstall, LMM_INFO, TFN, 
			_T("INF Copied %s to %s"), pszInfPath, szInstalledInf);

		//
		// set the registry value
		//
		fSuccess = pSetInstalledPath(
			HKEY_LOCAL_MACHINE, 
			pszRegPath, 
			pszRegValue, 
			szInstalledInf);

		if (!fSuccess) {
			pMsiLogError(hInstall, TFN, _T("pSetInstalledPath"));
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

	UINT uiReturn = 0;

	(VOID) pMsiDummyActionData(hInstall);

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
		pMsiLogMessage(hInstall, LMM_ERROR, TFN, _T("Cannot parse tokens"));
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

			pMsiLogError(hInstall, TFN, _T("NdasDiInstallRootEnumeratedDevice"));

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
				pMsiLogError(hInstall, TFN, _T("pSetInstalledPath"));
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

	(VOID) pMsiDummyActionData(hInstall);

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
		pMsiLogMessage(hInstall, LMM_ERROR, TFN, _T("Cannot parse tokens"));
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
		pMsiLogMessage(hInstall, LMM_ERROR, TFN, _T("Invalid Service Type"));
		return ERROR_INSTALL_FAILURE;
	}

	TCHAR szDependencies[250] = {0};
	// substitute ; with NULL for dependency
	{
		::StringCchCopy(szDependencies, 250, pszDependency);
		PTCHAR p = szDependencies;
		for (PTCHAR p = szDependencies; *p != _T('\0'); ++p) {
			if (_T(';') == *p) {
				*p = _T('\0');
			}
		}
	}


	while (TRUE) {

		fSuccess = NdasDiInstallDriverService(
			pszBinaryPath,
			pszServiceName,
			pszDisplayName,
			pszDescription,
			dwServiceType,
			dwStartType,
			dwErrorControl,
			pszLoadOrderGroup,
			NULL,
			szDependencies);

		if (!fSuccess) {

			pMsiLogError(hInstall, TFN, _T("NdasDiInstallRootEnumeratedDevice"));

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
		pMsiLogMessage(hInstall, LMM_ERROR, TFN, _T("Cannot parse tokens"));
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
	RUN_MODE_TRACE(hInstall);

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
		pMsiLogMessage(hInstall, LMM_ERROR, TFN, _T("Cannot parse tokens"));
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
	RUN_MODE_TRACE(hInstall);

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
		pMsiLogMessage(hInstall, LMM_ERROR, TFN, _T("Cannot parse tokens"));
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
	RUN_MODE_TRACE(hInstall);

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
		pMsiLogMessage(hInstall, LMM_ERROR, TFN, _T("Cannot parse tokens"));
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
	RUN_MODE_TRACE(hInstall);

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
		pMsiLogMessage(hInstall, LMM_ERROR, TFN, _T("Cannot parse tokens"));
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
	pMsiSchedulePreboot(hInstall);
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
		pMsiLogMessageEx(hInstall, LMM_INFO, TFN, _T("No deferred reboot."));
		return FALSE;
	}

	pMsiLogMessageEx(hInstall, LMM_INFO, TFN, _T("Deferred reboot is scheduled."));
	return TRUE;
}

UINT 
__stdcall
NDMsiStartServiceOnNoDeferredReboot(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	if (pIsDeferredRebootScheduled(hInstall))
	{
		pMsiLogMessage(hInstall, LMM_INFO, TFN, _T("Skipped this action."));
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
		pMsiLogMessage(hInstall, LMM_ERROR, TFN, _T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	pMsiLogMessageEx(hInstall, LMM_INFO, TFN, _T("ServiceName: %s"), lpServiceName);

	AutoHLocal autoLocal = (HLOCAL) lpTokens;

	AutoSCHandle hSCM = ::OpenSCManager(
		NULL, 
		SERVICES_ACTIVE_DATABASE, 
		GENERIC_READ | SC_MANAGER_CONNECT);

	if (NULL == (SC_HANDLE) hSCM)
	{
		DWORD dwLastError = ::GetLastError();
		pMsiLogMessageEx(hInstall, LMM_ERROR, TFN _T(":OpenSCManager"), 
			_T("Error %u(%08X)"), dwLastError, dwLastError);
		return ERROR_INSTALL_FAILURE;
	}

	AutoSCHandle hService = ::OpenService(
		hSCM, 
		lpServiceName, 
		SERVICE_START);

	if (NULL == (SC_HANDLE)hService)
	{
		DWORD dwLastError = ::GetLastError();
		pMsiLogMessageEx(hInstall, LMM_ERROR, TFN _T(":OpenService"), 
			_T("Error %u(%08X)"), dwLastError, dwLastError);
		return ERROR_INSTALL_FAILURE;
	}

	BOOL fSuccess = ::StartService(hService, 0, NULL);
	if (!fSuccess)
	{
		DWORD dwLastError = ::GetLastError();
		if (ERROR_SERVICE_ALREADY_RUNNING == dwLastError)
		{
			pMsiLogMessageEx(hInstall, LMM_ERROR, TFN _T(":OpenService"), 
				_T("Service is already running. Ignored."));
		} 
		else 
		{
			pMsiLogMessageEx(hInstall, LMM_ERROR, TFN _T(":StartService"), 
				_T("Error %u(%08X)"), dwLastError, dwLastError);
			return ERROR_INSTALL_FAILURE;
		}
	}

	pMsiLogMessageEx(hInstall, LMM_ERROR, 
		TFN _T("Started a service %s successfully."), 
		lpServiceName);

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

	if (pIsDeferredRebootScheduled(hInstall))
	{
		// Reboot Scheduled!
		UINT uiRet = ::MsiSetMode(hInstall, MSIRUNMODE_REBOOTATEND, TRUE);
		if (ERROR_SUCCESS != uiRet)
		{
			// issue warning
			pMsiLogMessageEx(hInstall, LMM_WARNING, TFN, 
				_T("MsiSetMode to REBOOTATEND failed with error: %08X."), uiRet);
		}

		return ERROR_SUCCESS;

	} 
	else 
	{
		pMsiLogMessage(hInstall, LMM_INFO, TFN, _T("Skipped this action."));
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
		return ERROR_INSTALL_FAILURE;
	}

	AutoHLocal autoTokens = (HLOCAL) lpTokens;

	INT iErrorDialog = _ttoi(pszErrorDialog);

	DBGPRT_INFO(_FT("Component Id: %s, Service Name: %s, Driver File Name: %s"), 
		pszCompId, 
		pszServiceName,
		pszDriverFileName);

	pMsiLogMessageEx(hInstall, LMM_INFO, TFN,
		_T("Component Id: %s, Service Name: %s, Driver File Name: %s"), 
		pszCompId, pszServiceName, pszDriverFileName);

	if (_T('\0') == pszCompId) {
		pMsiLogMessage(hInstall, LMM_ERROR, TFN, _T("Component ID not specified"));
		return ERROR_INSTALL_FAILURE;
	}

	HRESULT hr = E_FAIL;
	BOOL fRebootRequired = FALSE;

	while (TRUE) {

		hr = HrIsNetComponentInstalled(pszCompId);
		if (S_OK != hr) {
			break;
		}

		hr = HrUninstallNetComponent(pszCompId);

		if (FAILED(hr)) {
			if (0 == iErrorDialog) {
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
	RUN_MODE_TRACE(hInstall);

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
		pMsiLogMessage(hInstall, LMM_ERROR, TFN, _T("Cannot parse tokens"));
		return ERROR_INSTALL_FAILURE;
	}

	AutoHLocal autoTokens = (HLOCAL) lpTokens;

	INT iErrorDialog = _ttoi(pszErrorDialog);

	DBGPRT_INFO(_FT("Hardware Id: %s, Service Name: %s, Driver File Name: %s"), 
		pszHardwareId, 
		pszServiceName,
		pszDriverFileName);

	pMsiLogMessageEx(hInstall, LMM_INFO, TFN,
		_T("Hardware Id: %s, Service Name: %s, Driver File Name: %s"), 
		pszHardwareId, pszServiceName, pszDriverFileName);

	if (_T('\0') == pszHardwareId) {
		pMsiLogMessage(hInstall, LMM_ERROR, TFN, _T("Hardware ID not specified"));
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

	DBGPRT_INFO(_FT("NdasDiRemoveDevice removed %s (count: %d)\n"),
		pszHardwareId, cRemoved);

	pMsiLogMessageEx(hInstall, LMM_INFO, TFN,
		_T("Removed %d instances"), cRemoved);

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
		pMsiLogMessage(hInstall, LMM_ERROR, TFN, _T("Cannot parse tokens"));
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
	RUN_MODE_TRACE(hInstall);

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
		pMsiLogMessage(hInstall, LMM_ERROR, TFN, _T("Cannot parse tokens"));
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
	RUN_MODE_TRACE(hInstall);

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
	RUN_MODE_TRACE(hInstall);

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

VOID 
pMsiLogMessage(
	MSIHANDLE hInstall,
	LOGMSIMESSAGE_TYPE Type,
	LPCTSTR szSource,
	LPCTSTR szMessage)
{
	// non-invasive to last error
	DWORD err = GetLastError();

	PMSIHANDLE hRecord = MsiCreateRecord(2);

	TCHAR *fmtString = NULL;
	switch (Type) {
	case LMM_INFO:
		fmtString = _T("NDMSICA Info: Source: [1] Message: [2]");
		break;
	case LMM_ERROR:
		fmtString = _T("NDMSICA Error: Source: [1] Message: [2]");
		break;
	case LMM_WARNING:
		fmtString = _T("NDMSICA Warning: Source: [1] Message: [2]");
		break;
	default:
		fmtString = _T("NDMSICA: Source: [1] Message: [2]");
	}

	MsiRecordSetString(hRecord, 0, fmtString);
	MsiRecordSetString(hRecord, 1, (szSource) ? szSource : _T("(none)"));
	MsiRecordSetString(hRecord, 2, (szMessage) ? szMessage : _T("(none"));

	MsiProcessMessage(hInstall, INSTALLMESSAGE_INFO, hRecord);

	SetLastError(err);
}

VOID 
pMsiLogMessageEx(
	MSIHANDLE hInstall,
	LOGMSIMESSAGE_TYPE Type,
	LPCTSTR szSource,
	LPCTSTR szFormat,
	...)
{
	TCHAR szMessage[512]; // maximum 512 chars
	va_list ap;
	va_start(ap,szFormat);

	HRESULT hr = StringCchVPrintf(szMessage, 512, szFormat, ap);
	_ASSERTE(SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr);

	pMsiLogMessage(hInstall, Type, szSource, szMessage);
	va_end(ap);
}

LPTSTR
pGetSystemErrorText(DWORD dwError)
{
	DWORD err = GetLastError();
	LPTSTR lpMsgBuf = NULL;
	DWORD cchMsgBuf = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		dwError,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // We can only read English!!!
		(LPTSTR) &lpMsgBuf,
		0,
		NULL);
	if (cchMsgBuf > 0) {
		SetLastError(err);
		return lpMsgBuf;
	} else {
		if (lpMsgBuf) {
			LocalFree((HLOCAL)lpMsgBuf);
		}
		SetLastError(err);
		return NULL;
	}
}

VOID 
pMsiLogError(
	MSIHANDLE hInstall, 
	LPCTSTR szSource, 
	LPCTSTR szCallee,
	DWORD dwError)
{
	// non-invasive to last error
	DWORD err = GetLastError();

	PMSIHANDLE hRecord = MsiCreateRecord(4);
	TCHAR* fmtString = 
		_T("NDMSICA Error: Source: [1] Callee: [2] Error: [3] [4]");

	MsiRecordSetString(hRecord, 0, fmtString);

	MsiRecordSetString(hRecord, 1, (szSource) ? szSource : _T("(none)"));
	MsiRecordSetString(hRecord, 2, (szCallee) ? szCallee : _T("(none)"));
	MsiRecordSetInteger(hRecord, 3, dwError);

	LPTSTR lpMsgBuf = NULL;
	DWORD cchMsgBuf = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		dwError,
		MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), // We can only read English!!!
		(LPTSTR) &lpMsgBuf,
		0,
		NULL);

	if (cchMsgBuf > 0) {
		MsiRecordSetString(hRecord, 4, lpMsgBuf);
	}

	MsiProcessMessage(hInstall, INSTALLMESSAGE_INFO, hRecord);

	if (NULL != lpMsgBuf) {
		LocalFree(lpMsgBuf);
	}

	SetLastError(err);
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

static CONST LPCTSTR FORCE_REBOOT_REG_KEY = 
	_T("Software\\NDASSetup\\PrebootScheduled");

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

	::MsiSetProperty(hInstall, _T("PrebootScheduled"), _T("1"));

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

LPCTSTR 
pGetNextToken(
	LPCTSTR lpszStart, 
	CONST TCHAR chToken)
{
	LPCTSTR pch = lpszStart;
	while (chToken != *pch && _T('\0') != *pch) {
		++pch;
	}
	return pch;
}

LPTSTR 
pGetTokensV(
	LPCTSTR szData, 
	CONST TCHAR chToken,
	va_list ap)
{
	size_t cch = 0;
	HRESULT hr = ::StringCchLength(szData, STRSAFE_MAX_CCH, &cch);
	if (FAILED(hr)) {
		return NULL;
	}

	size_t cbOutput = (cch + 1) * sizeof(TCHAR);

	LPTSTR lpOutput = (LPTSTR) ::LocalAlloc(
		LPTR, cbOutput);

	if (NULL == lpOutput) {
		return NULL;
	}

	::CopyMemory(lpOutput, szData, cbOutput);

	LPTSTR lpStart = lpOutput;
	LPTSTR lpNext = lpStart;

	while (TRUE) {

		TCHAR** ppszToken = va_arg(ap, TCHAR**);

		if (NULL == ppszToken) {
			break;
		}

		if (_T('\0') != *lpStart) {

			lpNext = (LPTSTR) pGetNextToken(lpStart, chToken);

			if (_T('\0') == *lpNext) {
				*ppszToken = lpStart;
				lpStart = lpNext;
			} else {
				*lpNext = _T('\0');
				*ppszToken = lpStart;
				lpStart = lpNext + 1;
			}

		} else {
			*ppszToken = lpStart; 
		}

	}

	return lpOutput;
}

LPTSTR 
pGetTokens(
	LPCTSTR szData, 
	CONST TCHAR chToken, ...)
{
	va_list ap;
	va_start(ap, chToken);
	LPTSTR lp = pGetTokensV(szData, chToken, ap);
	va_end(ap);
	return lp;
}

NetClass 
pGetNetClass(LPCTSTR szNetClass)
{
	static const struct { LPCTSTR szClass; NetClass nc; } 
	nctable[] = {
		{_T("protocol"), NC_NetProtocol},
		{_T("adapter"), NC_NetAdapter},
		{_T("service"), NC_NetService},
		{_T("client"), NC_NetClient}
	};
	
	size_t nEntries = RTL_NUMBER_OF(nctable);

	for (size_t i = 0; i < nEntries; ++i) {
		if (0 == lstrcmpi(szNetClass, nctable[i].szClass)) {
			return nctable[i].nc;
		}
	}

	return NC_Unknown;
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
		pMsiLogMessage(hInstall, LMM_ERROR, TFN, _T("Cannot parse tokens"));
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
	RUN_MODE_TRACE(hInstall);

	return pMsiEnableCancelButton(hInstall, TRUE);
}

UINT
__stdcall
NDMsiDisableCancelButton(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	return pMsiEnableCancelButton(hInstall, FALSE);
}

