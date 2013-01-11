//
// NDDrvInst.cpp : Defines the entry point for the DLL application.
//
//#ifdef USE_PCH_XX
//#include "stdafx.h"
//#else
#include <windows.h>
#include <tchar.h>
#include <msiquery.h>
#include <msi.h>
#include <strsafe.h>
#include <stdlib.h>

#include "NDInst.h"
// headers and libraries for NetDisk device and driver install routines
#include "NDSetup.h"
#include "NDDevice.h"
#include "NDNetComp.h"
#include "NDFilter.h"
#include "SvcQuery.h"

#pragma comment(lib, "msi.lib")
#pragma comment(lib, "strsafe.lib")
#pragma comment(lib, "SetupAPI.lib")
#pragma comment(lib, "NDNetComp.lib")
#pragma comment(lib, "NDDevice.lib")
#pragma comment(lib, "NDFilter.lib")
#pragma comment(lib, "NDLog.lib")

#define USE_ACTIVATE_WARNDLG

#include "MsiProgressBar.h"
#include "resource.h"
#include "InstallTipDlg.h"
#include "RebootFlag.h"
#include "ActivateWarnDlg.h"
#include "MultilangRes.h"

#define SET_LANG_ID(HANDLE_INSTALL, LANGUAGE_ID) LANGUAGE_ID = (0 == ((LANGUAGE_ID) = MsiGetLanguage(HANDLE_INSTALL))) ? GetUserDefaultUILanguage() : (LANGUAGE_ID)
#define SAFE_CLOSESERVICEHANDLE(HANDLE_SERVICE) if(NULL != (HANDLE_SERVICE)) {CloseServiceHandle(HANDLE_SERVICE); (HANDLE_SERVICE) = NULL;}

HINSTANCE ghInst;

typedef struct _INSTALLDRIVER_CUSTOMACTIONDATA {
	WORD	wLangID;
	TCHAR	szInstallDir[MAX_PATH];
} INSTALLDRIVER_CUSTOMACTIONDATA;

UINT ProcessInstallError(MSIHANDLE hInstall, int iNdRet, LPCTSTR szCompName, DWORD dwError);
UINT ProcessUninstallError(MSIHANDLE hInstall, int iNdRet, LPCTSTR szCompName, DWORD dwError);

//++
//
// DllMain
//
// Nothing but a debugging purpose.
//

BOOL APIENTRY DllMain( HINSTANCE hInstDll, DWORD  dwReason, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);
    switch (dwReason)
	{
		case DLL_PROCESS_ATTACH:
			DebugPrintf(TEXT(__FILE__) TEXT(": Process Attach\n"));
			ghInst = hInstDll;
			break;
		case DLL_THREAD_ATTACH:
			DebugPrintf(TEXT(__FILE__) TEXT(": Thread Attach\n"));
			break;
		case DLL_THREAD_DETACH:
			DebugPrintf(TEXT(__FILE__) TEXT(": Thread Detach\n"));
			break;
		case DLL_PROCESS_DETACH:
			DebugPrintf(TEXT(__FILE__) TEXT(": Process Detach\n"));
			break;
    }
    return TRUE;
}

//++
//
// IsSystemEqualOrLater
//
// Windows 2000 : 5.0.x
// Windows XP   : 5.1.x
// Windows Server 2003 : 5.2.x
//
//  where x is a service pack version
//

#define IsWindows2000OrLater() IsSystemEqualOrLater(5,0,0)
#define IsWindowsXPOrLater() IsSystemEqualOrLater(5,1,0)
#define IsWindowsServer2003OrLater() IsSystemEqualOrLater(5,2,0)

BOOL IsSystemEqualOrLater(DWORD dwMajorVersion, DWORD dwMinorVersion, WORD wServicePackMajor)
{
	OSVERSIONINFOEX osvi;
	DWORDLONG dwlConditionMask = 0;

	// Initialize the OSVERSIONINFOEX structure.

	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	osvi.dwMajorVersion = dwMajorVersion;
	osvi.dwMinorVersion = dwMinorVersion;
	osvi.wServicePackMajor = wServicePackMajor;

	// Initialize the condition mask.

	VER_SET_CONDITION( dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL );
	VER_SET_CONDITION( dwlConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL );
	VER_SET_CONDITION( dwlConditionMask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL );

	// Perform the test.

	return VerifyVersionInfo(&osvi, 
		VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR,
		dwlConditionMask);
}

//++
//
// LogCompOperation
//
// 

void LogCompOperation(int iNdRetCode, LPCTSTR szCompName)
{
	switch (iNdRetCode)
	{
	case NDS_SUCCESS:
		LogPrintf(TEXT("Operation completed successfully for %s."), szCompName);
		break;
	case NDS_FAIL:
		LogLastError();
		LogPrintf(TEXT("ERROR: Operation failed for %s."), szCompName);
		break;
	case NDS_REBOOT_REQUIRED:
		LogPrintf(TEXT("WARNING: Reboot required for this operation."));
		break;
	case NDS_PREBOOT_REQUIRED:
		LogPrintf(TEXT("ERROR: Preboot required for this operation."));
		break;
	default:
		LogPrintf(TEXT("WARNING: Unknown code returned from the function for the operation with %s."), szCompName);
		break;
	}
	return;
}

//++
//
// Delete Device Service
//
//

int DeleteDeviceService(LPCTSTR szServiceName)
{
	BOOL bRet;
	SC_HANDLE hSCM = NULL, hService = NULL;
	int iReturn = NDS_FAIL;

	hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == hSCM)
	{
		LogPrintfErr(TEXT("Error opening Service Control Manager"));
		goto out;
	}

	hService = OpenService(hSCM, szServiceName, DELETE);
	
	if (NULL == hService)
	{
		LogPrintfErr(TEXT("Error opening service %s"), szServiceName);
		goto out;
	}

	bRet = DeleteService(hService);
	if (NULL == bRet)
	{
		LogPrintfErr(TEXT("Error deleting service %s"), szServiceName);
		goto out;
	}

	iReturn = NDS_SUCCESS;
out:
	SAFE_CLOSESERVICEHANDLE(hService);
	SAFE_CLOSESERVICEHANDLE(hSCM);

	return iReturn;
}

//++
//
// LPX
//
//

NDINST_API int __stdcall InstallLPX(LPTSTR szDrvSrcPath)
{
	int		iRet;
	TCHAR	lpBuffer[MAX_PATH + 1] = {0};

	StringCchPrintf(lpBuffer, MAX_PATH, TEXT("%s\\%s"), szDrvSrcPath, LPX_INF);

	LogPrintf(TEXT("Installing LPX from %s..."), lpBuffer);
	
	iRet = NDInstallNetComp(LPX_NETCOMPID, NDNC_PROTOCOL, lpBuffer);
	
	LogCompOperation(iRet, TEXT("LPX Protocol"));

	return iRet;
}

NDINST_API int __stdcall StopLPX()
{
	TCHAR	lpBuffer[MAX_PATH + 1] = {0};
	SERVICE_STATUS status;
	SC_HANDLE hSCM = NULL, hLpx = NULL;
	int iReturn = NDS_FAIL;
	BOOL bRet;

	LogPrintf(TEXT("Stopping LPX..."));

	hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == hSCM) 
	{
		LogPrintfErr(TEXT("Error opening Service Control Manager"));
		goto out;
	}

	hLpx = OpenService(hSCM, LPX_SERVICE, SERVICE_STOP);
	if (NULL == hLpx)
	{
		LogPrintfErr(TEXT("Error opening service %s"), LPX_SERVICE);
		goto out;
	}
	
	bRet = ControlService(hLpx, SERVICE_CONTROL_STOP, &status);
	if (NULL != bRet)
	{

		switch (status.dwWin32ExitCode)
		{
		case NO_ERROR:
		case ERROR_SERVICE_NOT_ACTIVE:
			iReturn = NDS_SUCCESS;
			break;
		case ERROR_INVALID_SERVICE_CONTROL:
		case ERROR_SERVICE_CANNOT_ACCEPT_CTRL:
			iReturn = NDS_FAIL;
			break;
		}
	}

out:
	SAFE_CLOSESERVICEHANDLE(hLpx);
	SAFE_CLOSESERVICEHANDLE(hSCM);

	return iReturn;
}

NDINST_API int __stdcall UninstallLPX()
{
	int		iRet;
	TCHAR	lpBuffer[MAX_PATH + 1] = {0};

	LogPrintf(TEXT("Uninstalling LPX..."));

	// just try to stop lpx and ignore if failed to stop it
	iRet = NDUninstallNetComp(LPX_NETCOMPID);
	// iRet = StopLPX();
	
	LogCompOperation(iRet, TEXT("LPX Protocol"));

	return iRet;
}

//++
//
// LANSCSIBusDevice
//
//

NDINST_API int __stdcall InstallLANSCSIBusDevice(LPTSTR szDrvSrcPath, LPTSTR szOEMInf)
{
	int		iRet;
	TCHAR	lpBuffer[MAX_PATH + 1] = {0};

	StringCchPrintf(lpBuffer, MAX_PATH, TEXT("%s\\%s"), szDrvSrcPath, LANSCSIBUS_INF);

	LogPrintf(TEXT("Installing LANSCSIBus OEM INF from %s..."), lpBuffer);

	iRet = NDCopyInf(lpBuffer, szOEMInf);

	LogPrintf(TEXT("LANSCSIBus INF copied to %s"), szOEMInf);
	LogCompOperation(iRet, TEXT("LANSCSIBus INF"));

	// safely ignore the NDCopyInf failure here.

	LogPrintf(TEXT("Installing LANSCSIBus from %s..."), lpBuffer);

	iRet = NDInstallDevice(lpBuffer, LANSCSIBUS_HWID) ;
	
	LogCompOperation(iRet, TEXT("LANSCSIBus"));

	return iRet;
}

NDINST_API int __stdcall RemoveLANSCSIBusDevice()
{
	int		iRet;
	TCHAR	lpBuffer[MAX_PATH + 1] = {0};

	LogPrintf(TEXT("Removing LANSCSIBus Devices..."));

	iRet = NDRemoveDevice(LANSCSIBUS_HWID);
	
	LogCompOperation(iRet, TEXT("LANSCSIBus"));

	return iRet;
}

NDINST_API int __stdcall RemoveLANSCSIBusDriver()
{
	return DeleteDeviceService(LANSCSIBUS_SERVICE);
}

//++
//
// LANSCSIMiniportInf
//
//

NDINST_API int __stdcall InstallLANSCSIMiniportInf(LPTSTR szDrvSrcPath, LPTSTR szOEMInf)
{
	int		iRet = 0;
	TCHAR	lpBuffer[MAX_PATH + 1] = {0};

	StringCchPrintf(lpBuffer, MAX_PATH, TEXT("%s\\%s"), szDrvSrcPath, LANSCSIMINIPORT_INF);

	LogPrintf(TEXT("Installing LANSCSIMiniport INF from %s"), lpBuffer);

	iRet = NDCopyInf(lpBuffer, szOEMInf);

	LogPrintf(TEXT("LANSCSIMiniport INF copied to %s"), szOEMInf);

	// iRet = NDUpdateDriver(lpBuffer, LANSCSIMINIPORT_HWID);

	LogCompOperation(iRet, TEXT("LANSCSIMiniport INF"));

	return iRet;
}

NDINST_API int __stdcall UpdateLANSCSIMiniportDriver(LPTSTR szDrvSrcPath)
{
	int		iRet = 0;
	TCHAR	lpBuffer[MAX_PATH + 1] = {0};

	StringCchPrintf(lpBuffer, MAX_PATH, TEXT("%s\\%s"), szDrvSrcPath, LANSCSIMINIPORT_INF);
	LogPrintf(TEXT("Updating LANSCSIMiniport Device Driver from %s..."), lpBuffer);

	iRet = NDUpdateDriver(lpBuffer, LANSCSIMINIPORT_HWID);

	LogCompOperation(iRet, TEXT("LANSCSIMiniport Driver Update"));

	return iRet;
}

NDINST_API int __stdcall RemoveLANSCSIMiniportDevice()
{
	int		iRet;
	TCHAR	lpBuffer[MAX_PATH + 1] = {0};

	LogPrintf(TEXT("Removing LANSCSIMiniport Device..."));

	iRet = NDRemoveDevice(LANSCSIMINIPORT_HWID);
	
	LogCompOperation(iRet, TEXT("LANSCSIMiniport Devices"));

	return iRet;
}

NDINST_API int __stdcall RemoveLANSCSIMiniportDriver()
{
	return DeleteDeviceService(LANSCSIMINIPORT_SERVICE);
}

//++
//
// ROFilter
//
//

NDINST_API int __stdcall InstallROFilter(LPTSTR szDrvSrcPath)
{
	int		iRet;
	TCHAR	lpBuffer[MAX_PATH + 1] = {0};

	StringCchPrintf(lpBuffer, sizeof(lpBuffer), TEXT("%s\\%s"), szDrvSrcPath, ROFILT_SYS_FILE);
	LogPrintf(TEXT("Installing ROFilter INF from %s"), lpBuffer);

	iRet = LoadAndStartROFilter(lpBuffer);

	LogCompOperation(iRet, TEXT("ROFilter"));

	return iRet;
}

NDINST_API int __stdcall UninstallROFilter()
{
	int		iRet;
	TCHAR	lpBuffer[MAX_PATH + 1] = {0};

	LogPrintf(TEXT("Uninstalling ROFilter..."));

	iRet = UnloadROFilter();

	LogCompOperation(iRet, TEXT("ROFilter"));

	return iRet;
}

//++
//
// LFSFilter
//
//
#define LFSFILT_SYS_NAME		TEXT("LfsFilt")
#define LFSFILT_SYS_FILE		TEXT("LfsFilt.sys")
#define LFSFILT_DISP_NAME		TEXT("Lean File Sharing")
#define LFSFILT_LOADORDER_2000	TEXT("filter")
#define LFSFILT_LOADORDER_XP	TEXT("FSFilter Activity Monitor")
#define LFSFILT_DEPENDENCIES	TEXT("lpx\0\0")

NDINST_API int __stdcall InstallLFSFilter(LPTSTR szDrvSrcPath)
{
	int		iRet;
	TCHAR	lpBuffer[MAX_PATH + 1] = {0};

	StringCchPrintf(lpBuffer, sizeof(lpBuffer), TEXT("%s\\%s"), szDrvSrcPath, LFSFILT_SYS_FILE);
	LogPrintf(TEXT("Installing LFSFilter INF from %s"), lpBuffer);


	if (IsWindowsXPOrLater()) {
		iRet = InstallNonPnPDriver(
				lpBuffer,
				LFSFILT_SYS_NAME,
				LFSFILT_SYS_FILE,
				LFSFILT_DISP_NAME,
				SERVICE_SYSTEM_START,
				SERVICE_ERROR_NORMAL,
				LFSFILT_LOADORDER_XP,
				LFSFILT_DEPENDENCIES
			);
	} else {
		iRet = InstallNonPnPDriver(
				lpBuffer,
				LFSFILT_SYS_NAME,
				LFSFILT_SYS_FILE,
				LFSFILT_DISP_NAME,
				SERVICE_SYSTEM_START,
				SERVICE_ERROR_NORMAL,
				LFSFILT_LOADORDER_2000,
				LFSFILT_DEPENDENCIES
			);
	}

	LogCompOperation(iRet, TEXT("LFSFilter"));

	return iRet;
}

NDINST_API int __stdcall UninstallLFSFilter()
{
	int		iRet;
	TCHAR	lpBuffer[MAX_PATH + 1] = {0};

	LogPrintf(TEXT("Uninstalling LFSFilter..."));

//	StopNonPnPDriver(
//		LFSFILT_SYS_NAME
//	) ;

	iRet = UninstallNonPnPDriver(
					LFSFILT_SYS_NAME,
					LFSFILT_SYS_FILE
				);

	LogCompOperation(iRet, TEXT("LFSFilter"));

	return iRet;
}


//++
//
// ProcessInstallError
//

UINT ProcessInstallError(MSIHANDLE hInstall, int iNdRet, LPCTSTR szCompName, DWORD dwError)
{
	UINT iResult;
	PMSIHANDLE hActionRec = MsiCreateRecord(3);

	DebugPrintf(TEXT("ProcessInstallError(%d, %s)\n"), iNdRet, szCompName);

	switch (iNdRet)
	{
	case NDS_SUCCESS:
		iResult = ERROR_SUCCESS;
		break;
	case NDS_REBOOT_REQUIRED:
		// Initiate REBOOT
		// MsiSetProperty does not work in deferred execution actions
		/// MsiSetProperty(hInstall, TEXT("CustomActionData"), TEXT("REBOOT"));
		LogPrintf(TEXT("Reboot required for %s"), szCompName);
		SetRebootFlag(TRUE);
		iResult = ERROR_SUCCESS_REBOOT_REQUIRED;
		break;
	case NDS_PREBOOT_REQUIRED:
		MsiRecordSetString(hActionRec, 3, TEXT("System must be restarted before this operation."));
		iResult = ERROR_INSTALL_FAILURE;
		break;
	case NDS_INVALID_ARGUMENTS:
		MsiRecordSetString(hActionRec, 3, TEXT("Invalid arguments"));
		iResult = ERROR_INSTALL_FAILURE;
		break;
	case NDS_FAIL:
	default:
		MsiRecordSetInteger(hActionRec, 1, 8002);
		MsiRecordSetString(hActionRec, 2, szCompName);
		MsiRecordSetInteger(hActionRec, 3, dwError);

		iResult = MsiProcessMessage(hInstall,
			INSTALLMESSAGE(INSTALLMESSAGE_ERROR|MB_ABORTRETRYIGNORE|MB_ICONWARNING),
			hActionRec);
		
		switch (iResult)
		{
		case IDABORT:
			return ERROR_INSTALL_FAILURE;
		case IDRETRY:
			return IDRETRY;
		case IDIGNORE:
			return ERROR_SUCCESS;
		}
		break;
	}

	return iResult;
}

//++
//
// ProcessUninstallError
//
//

UINT ProcessUninstallError(MSIHANDLE hInstall, int iNdRet, LPCTSTR szCompName, DWORD dwError)
{
	PMSIHANDLE hActionRec = MsiCreateRecord(3);
	UINT	iResult;

	DebugPrintf(TEXT("ProcessUninstallError(%d, %s)\n"), iNdRet, szCompName);

	switch (iNdRet)
	{
	case NDS_SUCCESS:
		iResult = ERROR_SUCCESS;
		break;
	case NDS_REBOOT_REQUIRED:
		// does not work
		// MsiSetProperty(hInstall, TEXT("CustomActionData"), TEXT("REBOOT"));
		// SetRebootFlag(TRUE);
		iResult = ERROR_SUCCESS;
		break;
	case NDS_PREBOOT_REQUIRED:
		MsiRecordSetString(hActionRec, 3, TEXT("System must be restarted before this operation."));
		iResult = ERROR_INSTALL_FAILURE;
		break;
	case NDS_INVALID_ARGUMENTS:
		MsiRecordSetString(hActionRec, 3, TEXT("Invalid arguments"));
		iResult = ERROR_INSTALL_FAILURE;
		break;
	// ignore errors at this time
	case NDS_FAIL:
		iResult = ERROR_INSTALL_FAILURE;
		break;
	default:
		MsiRecordSetInteger(hActionRec, 1, 8001);
		MsiRecordSetString(hActionRec, 2, szCompName);
		MsiRecordSetInteger(hActionRec, 3, dwError);
		MsiProcessMessage(hInstall,
			INSTALLMESSAGE(INSTALLMESSAGE_WARNING|MB_OK|MB_ICONWARNING),
			hActionRec);
		iResult = ERROR_INSTALL_FAILURE;
		break;
	}

	return iResult;
}

//++
//
// NDMsiInstallDriver
//
//
// Custom Action Return Values
// 
// ERROR_FUNCTION_NOT_CALLED	Action not executed. 
// ERROR_SUCCESS				Completed actions successfully. 
// ERROR_INSTALL_USEREXIT		User terminated prematurely. 
// ERROR_INSTALL_FAILURE		Unrecoverable error occurred. 
// ERROR_NO_MORE_ITEMS			Skip remaining actions, not an error. 
//
// CAUTION:
//
// Because installation script can be executed outside of the installation session 
// in which it was written, the session may no longer exist during execution 
// of the installation script. In this case, the original session handle and properties 
// set during the installation sequence are not available 
// to a deferred execution custom action. 
// Any functions that require a session handle are restricted to a few methods 
// that can retrieve context information, or else properties that are needed during 
// script execution must be written into the installation script.
//
// Reference : MSDN, Obtaining Context Information for Deferred Execution Custom Actions
//
// Here we are using "CustomActionData" property, which is actually
// "ND_InstallDriver" in the MSI Property.
// Passed value is [INSTALLDIR] to retrieve the actual installed directory.
// And the passing value - to use later in the installation script
// is "REBOOT", that will be used to initiate ForceReboot
// by the custom action "ND_SetCustomActionReboot"
// 
BOOL ParseInstallDriverData(INSTALLDRIVER_CUSTOMACTIONDATA* data, LPCTSTR szProperty, DWORD cch)
{
	TCHAR *pch, szBuf[MAX_PATH];
	DWORD	i;
	
	pch = (TCHAR*) &szProperty[0];

	for (i = 0; szProperty[i] != _T(';') && szProperty[i] != _T('\0') && i < cch; i++)
		;
	LogPrintf(TEXT("Property: %s"), szProperty);

	// invalid parameter
	if (szProperty[i] != _T(';'))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	// get language id
	StringCchCopyN(szBuf, MAX_PATH, szProperty, i);
	data->wLangID = (WORD)_ttoi(szBuf);
	LogPrintf(TEXT("Language: %d"), data->wLangID);

	// skip ;
	i++;

	// copy remaining path to szInstallDir
	pch = (TCHAR*) &szProperty[i];
	StringCchCopy(data->szInstallDir, MAX_PATH, pch);
	LogPrintf(TEXT("InstallDir: %s"), data->szInstallDir);

	return TRUE;
}

NDINST_API UINT __stdcall NDMsiInstallDriver(MSIHANDLE hInstall)
{
	TCHAR szCustomActionData[MAX_PATH + 1] = {0};
	TCHAR szDrvSrcPath[MAX_PATH + 1] = {0};
	TCHAR szLANSCSIBusOEMInf[MAX_PATH + 1] = {0};
	TCHAR szLANSCSIMiniportOEMInf[MAX_PATH + 1] = {0};
	LANGID langID;

#ifdef USE_ACTIVATE_WARNDLG
	HANDLE hThread;
	DWORD dwThreadId;
#endif /* USE_ACTIVATE_WARNDLG */

	int iNdRet;
	UINT iResult;
	HANDLE hDlgThread;
	DWORD  dwDlgThreadId;
	TCHAR	szBuffer[256] = {0};

	DWORD cchBuf = MAX_PATH;

	MsiProgressBar pb(hInstall, 8000, 1000);

	PMSIHANDLE hActionRec = MsiCreateRecord(3);

	// Messages are encoded as UTF-16
	WCHAR wszBuf[256];

	int offset;

	BOOL bParsed;
	BOOL	bInstalledLPX = FALSE,
			bInstalledLanscsiBus = FALSE;

	UINT uiReturn = ERROR_INSTALL_FAILURE;

	DebugPrintf(TEXT("MsiInstallDriver()\n"));
	DebugPrintf(TEXT("MsiGetLanguage() = %d\n"), MsiGetLanguage(hInstall));

	// Ensures that custom action called from install script execution
	if (TRUE != MsiGetMode(hInstall,MSIRUNMODE_SCHEDULED))
	{
		uiReturn = ERROR_SUCCESS;
		goto out;
	}

	// LANGID : 1033, ...
	// INSTALLDIR : usually [ProgramFilesFolder]XIMETA\NetDisk
	// MsiSetProperty(hMSI, "ND_InstallDriver", "LANGID;INSTALLDIR")
	// in InstallDriver_SetData (in Setup.rul) 
	// Generates path where drivers will be installed to

	// LANGID is set from the immediate action,
	// because MsiGetLanguage() function always returns 0
	// in a deferred custom action

	INSTALLDRIVER_CUSTOMACTIONDATA caData;

	// parse CustomActionData to get LANGID and INSTALLDIR
	iResult = MsiGetProperty(hInstall, TEXT("CustomActionData"), szCustomActionData, &cchBuf);

	DebugPrintf(_T("CustomActionData : %s\n"), szCustomActionData);

	bParsed = ParseInstallDriverData(&caData, szCustomActionData, cchBuf);
	if (!bParsed)
	{
		// invalid parameter from CustomActionData
		return ERROR_INSTALL_FAILURE;
	}

	DebugPrintf(_T("Parsed: LANGID=%d, INSTALLDIR=%s\n"), caData.wLangID, caData.szInstallDir);

	// e.g: caData.szDrvSrcPath == "C:\\Program Files\\XIMETA\\NetDisk\\DRIVERS"
	//      caData.wLangId = 1033

	StringCchPrintf(szDrvSrcPath, MAX_PATH, TEXT("%sDRIVERS"), caData.szInstallDir);
	DebugPrintf(TEXT("Driver Path is set to %d, %s\n"), iResult, szDrvSrcPath);

	langID = caData.wLangID;
	// Make language ID (if langID is still 0, to set langID from GetDefaultUILanguage()
	SET_LANG_ID(hInstall, langID);

	DebugPrintf(TEXT("LangId is set to %d (CustomActionData %d)\n"), langID, caData.wLangID);
	
	// Launch thread 2

	// AING_TO_DO : change to modeless dialog
	itipdlg::THREADPARAM itipParam;
	itipParam.hInstance = ghInst;
	itipParam.wLang = langID;

	hDlgThread = CreateThread(NULL, 0, itipdlg::InstallTipThreadProc, (LPVOID) &itipParam, 0, &dwDlgThreadId);
	
	LogStart();

	pb.Reset();

	// Installer is executing the installation script. Set up a
	// record specifying appropriate templates and text for
	// messages that will inform the user about what the custom
	// action is doing. Tell the installer to use this template and 
	// text in progress messages.

	offset = GetLanguageResourceOffset(langID);
	LoadString(ghInst, IDS_INSTALL + offset, wszBuf, 255);

	// ms-help://MS.MSDNQTR.2003OCT.1033/msi/setup/msiprocessmessage.htm
	MsiRecordSetString(hActionRec, 1, TEXT("NDInstallDriver")); // : action name
	MsiRecordSetString(hActionRec, 2, wszBuf); // "Installing Device Drivers..." : description	
	MsiRecordSetString(hActionRec, 3, TEXT("Installing [2]")); // : template for ACTIONDATA messages
	iResult = MsiProcessMessage(hInstall, INSTALLMESSAGE_ACTIONSTART, hActionRec); // Progress: start of action

	pb.Step();

	//
	// LPX Protocol
	//

	// Action data. Record fields correspond to the template of ACTIONSTART message.
	MsiRecordSetString(hActionRec, 1, TEXT("LPX Protocol"));
	iResult = MsiProcessMessage(hInstall, INSTALLMESSAGE_ACTIONDATA, hActionRec);
	
	do {
		iNdRet = InstallLPX(szDrvSrcPath);
		if(ERROR_SUCCESS == iNdRet)
		{
			bInstalledLPX = TRUE;
			break;
		}

		// error
		iResult = ProcessInstallError(hInstall, iNdRet, TEXT("LPX Protocol"), GetLastError());

		switch(iResult)
		{
		case ERROR_INSTALL_FAILURE:
			goto out;
		case IDRETRY:
			continue;
		case ERROR_SUCCESS: // ignore
		default: 
			break;
		}
	} while (ERROR_SUCCESS != iResult);
	
	pb.Step();

	//
	// LANSCSIBusDevice
	//

	MsiRecordSetString(hActionRec, 1, TEXT("LANSCSI Bus"));
	iResult = MsiProcessMessage(hInstall, INSTALLMESSAGE_ACTIONDATA, hActionRec);
	
	do {
		iNdRet = InstallLANSCSIBusDevice(szDrvSrcPath, szLANSCSIBusOEMInf);
		if(ERROR_SUCCESS == iNdRet)
		{
			bInstalledLanscsiBus = TRUE;
			break;
		}

		// error
		iResult = ProcessInstallError(hInstall, iNdRet, TEXT("LANSCSI Bus"), GetLastError());

		if(ERROR_INSTALL_FAILURE == iResult)
		{
			goto out;
		}
	} while (IDRETRY == iResult);  // exit loop if ignore
	
	pb.Step();

	//
	// LANSCSIMiniportDevice
	//

	MsiRecordSetString(hActionRec, 1, TEXT("LANSCSI Miniport"));
	iResult = MsiProcessMessage(hInstall, INSTALLMESSAGE_ACTIONDATA, hActionRec);

	do {
#ifdef USE_ACTIVATE_WARNDLG
		hThread = CreateThread(NULL, 0, awdlgproc::ActivateWarnDlgProc, NULL, 0, &dwThreadId);
#endif /* USE_ACTIVATE_WARNDLG */

		iNdRet = InstallLANSCSIMiniportInf(szDrvSrcPath, szLANSCSIMiniportOEMInf);

#ifdef USE_ACTIVATE_WARNDLG
		InterlockedIncrement(&awdlgproc::lThreadCount);
		WaitForSingleObject(hThread, 60000);
		CloseHandle(hThread);
#endif /* USE_ACTIVATE_WARNDLG */

		if(ERROR_SUCCESS == iNdRet)
		{
			break;
		}

		// error
		iResult = ProcessInstallError(hInstall, iNdRet, TEXT("LANSCSI Miniport"), GetLastError());

		if(ERROR_INSTALL_FAILURE == iResult)
		{
			goto out;
		}
	} while (IDRETRY == iResult);  // exit loop if ignore

	pb.Step();

 		//
		// LFSFilter
		//

		MsiRecordSetString(hActionRec, 1, TEXT("LFSFilter"));
		iResult = MsiProcessMessage(hInstall, INSTALLMESSAGE_ACTIONDATA, hActionRec);

		do {
			iNdRet = InstallLFSFilter(szDrvSrcPath);
			//
			//	force to reboot at this time
			//
			if(NDS_SUCCESS == iNdRet)
				iNdRet = NDS_REBOOT_REQUIRED ;

			iResult = ProcessInstallError(hInstall, iNdRet, TEXT("LFSFilter"), GetLastError());

			if (ERROR_INSTALL_FAILURE == iResult)
			{
				goto out;
			}

		} while (IDRETRY == iResult);

/*
		pb.Step();

 	//
	// ROFilter
	//
	// skips for Windows XP or later
	//

	if (!IsWindowsXPOrLater())
	{
		MsiRecordSetString(hActionRec, 1, TEXT("ROFilter"));
		iResult = MsiProcessMessage(hInstall, INSTALLMESSAGE_ACTIONDATA, hActionRec);

		do {
			iNdRet = InstallROFilter(szDrvSrcPath);

			if(ERROR_SUCCESS == iNdRet)
			{
				break;
			}

			iResult = ProcessInstallError(hInstall, iNdRet, TEXT("ROFilter"), GetLastError());

			if (ERROR_INSTALL_FAILURE == iResult)
			{
				goto out;
			}
		} while (IDRETRY == iResult);  // exit loop if ignore

	}

*/
//
//	<-- hootch 12162003
//
		pb.Step();

		//
		// Start LDServ
		//

/*
		SC_HANDLE hScm, hService;
		if ((hScm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)) != NULL)
		{
			if ((hService = OpenService(hScm, LDSERV_SERVICE, SERVICE_START)) != NULL)
			{
				if (!StartService(hService, 0, NULL))
					LogPrintf(TEXT("Error starting LDServ"));
			}
			else
			{
				LogPrintfErr(TEXT("Cannot open service %s"), LDSERV_SERVICE);
			}
		}
		else
		{
			LogPrintfErr(TEXT("Failed to open Service Control Manager"));
		}
*/
		pb.Step();

		//
		// Update LanScsiMiniport Driver
		//
	// AING : Does not handle error.
		
		MsiRecordSetString(hActionRec, 1, TEXT("LANSCSIMiniportDriver"));
		iResult = MsiProcessMessage(hInstall, INSTALLMESSAGE_ACTIONDATA, hActionRec);
		
		iNdRet = UpdateLANSCSIMiniportDriver(szDrvSrcPath);
	ProcessInstallError(hInstall, iNdRet, TEXT("LANSCSIMiniportDriver"), GetLastError());

		pb.Step();
	
		LogEnd();

	// fall back of other modes
	uiReturn = ERROR_SUCCESS;
out:

	// Closes tip dialog
	// AING_TO_DO : change to modeless dialog
		/// DestroyWindow(ghWndInstallTipDlg);
	if(IsWindow(itipdlg::hWndDlg))
	{
		SendMessage(itipdlg::hWndDlg, WM_CLOSE, NULL, NULL);
		WaitForSingleObject(hDlgThread, 10000);
		CloseHandle(hDlgThread);
	}
		// WaitForSingleObject(hDlgThread, INFINITE);

	if(ERROR_INSTALL_FAILURE == uiReturn)
	{
		if(bInstalledLPX)
		{
			UninstallLPX();
		}
		if(bInstalledLanscsiBus)
		{
			RemoveLANSCSIBusDevice();
			RemoveLANSCSIBusDriver();
		}
    }

	return uiReturn;
}

/*
NDINST_API UINT _stdcall NDMsiUpdateDriver(MSIHANDLE hInstall)
{
	TCHAR szInstallDir[MAX_PATH + 1] = {0};
	TCHAR szDrvSrcPath[MAX_PATH + 1] = {0};
	TCHAR szLANSCSIBusOEMInf[MAX_PATH + 1] = {0};
	TCHAR szLANSCSIMiniportOEMInf[MAX_PATH + 1] = {0};

	if (MsiGetMode(hInstall,MSIRUNMODE_SCHEDULED) == TRUE)
	{
		int iRet;
		DWORD cchValueBuf = MAX_PATH;

		hDlgThread = CreateThread(NULL, 0, InstallTipThreadProc, NULL, 0, &dwDlgThreadId);

		LogStart();

		MsiProgressBar pb = MsiProgressBar(hInstall, 6000, 1000);
		pb.Reset();

		PMSIHANDLE hActionRec = MsiCreateRecord(3);

		// Installer is executing the installation script. Set up a
		// record specifying appropriate templates and text for
		// messages that will inform the user about what the custom
		// action is doing. Tell the installer to use this template and 
		// text in progress messages.

		MsiRecordSetString(hActionRec, 1, TEXT("NDUpdateDriver"));
		// Update Device Drivers...
		LoadString(ghInst, IDS_UPDATE, szBuffer, 255);
		MsiRecordSetString(hActionRec, 2, szBuffer);
		MsiRecordSetString(hActionRec, 3, TEXT("Updating [2]"));
		iResult = MsiProcessMessage(hInstall, INSTALLMESSAGE_ACTIONSTART, hActionRec);
 

		iRet = MsiGetProperty(hInstall, TEXT("CustomActionData"), szInstallDir, &cchValueBuf);
		StringCchPrintf(szDrvSrcPath, MAX_PATH, TEXT("%sDRIVERS"), szInstallDir);
		DebugPrintf(TEXT("Driver Path is set to %d, %s"), iRet, szDrvSrcPath);

		StringCchPrintf(szLANSCSIMiniportOEMInf, MAX_PATH, TEXT("%s\\%s"), szDrvSrcPath, LANSCSIMINIPORT_INF);
		LogPrintf(TEXT("Updating LANSCSIMiniport Drivers from %s"), szLANSCSIMiniportOEMInf);
		iRet = NDUpdateDriver(szLANSCSIMiniportOEMInf, LANSCSIMINIPORT_HWID);

		LogCompOperation(iRet, TEXT("Update LANSCSIMiniport Driver"));

		SendMessage(ghWndInstallTipDlg, WM_CLOSE, NULL, NULL);
		WaitForSingleObject(hDlgThread, 10000);
		CloseHandle(hDlgThread);

		LogEnd();
	}

	return ERROR_SUCCESS;
}
*/

//++
//
// NDMsiUninstallDriver
//
//

NDINST_API UINT __stdcall NDMsiUninstallDriver(MSIHANDLE hInstall)
{
	int iNdRet;
	TCHAR	szBuffer[256] = {0};
	MsiProgressBar pb = MsiProgressBar(hInstall, 5000, 1000);
	PMSIHANDLE hActionRec;
	LANGID langID;
	WCHAR wszBuf[256];
	int offset;
	UINT iResult;

	UINT uiReturn = ERROR_INSTALL_FAILURE;
	
	DebugPrintf(TEXT("MsiUninstallDriver()...\n"));

	LogStart();

	if (TRUE != MsiGetMode(hInstall,MSIRUNMODE_SCHEDULED))
	{
		uiReturn = ERROR_SUCCESS;
		goto out;
	}

	pb.Reset();

	hActionRec = MsiCreateRecord(3);

	// Installer is executing the installation script. Set up a
	// record specifying appropriate templates and text for
	// messages that will inform the user about what the custom
	// action is doing. Tell the installer to use this template and 
	// text in progress messages.


	// Uninstalling Device Drivers...
	SET_LANG_ID(hInstall, langID);
	offset = GetLanguageResourceOffset(langID);
	LoadString(ghInst, IDS_UNINSTALL + offset, wszBuf, 255);

	MsiRecordSetString(hActionRec, 1, TEXT("NDInstallDriver"));
	MsiRecordSetString(hActionRec, 2, wszBuf);
	MsiRecordSetString(hActionRec, 3, TEXT("Uninstalling [1]"));
	iResult = MsiProcessMessage(hInstall, INSTALLMESSAGE_ACTIONSTART, hActionRec);

	pb.Step();

	//////////////////////////////////////////////////////////////////////////
	//
	//		LFSFilter
	//

	DebugPrintf(TEXT("LFSFilter\n"));
	MsiRecordSetString(hActionRec, 1, TEXT("LFSFilter"));
	iResult = MsiProcessMessage(hInstall, INSTALLMESSAGE_ACTIONDATA, hActionRec);

	iNdRet = UninstallLFSFilter();
	ProcessUninstallError(hInstall, iNdRet, TEXT("LFSFilter"), GetLastError());

/*
	//
	// ROFilter
	//
	// Actually ROFilter is not installed on Windows XP or later
	// However, products prior to 2.1.4 has installed it.
	// Thus, still try to uninstall ROFilter here.
	//

	DebugPrintf(TEXT("ROFilter"));
	MsiRecordSetString(hActionRec, 1, TEXT("ROFilter"));
	iResult = MsiProcessMessage(hInstall, INSTALLMESSAGE_ACTIONDATA, hActionRec);

	iNdRet = UninstallROFilter();
	ProcessUninstallError(hInstall, iNdRet, TEXT("ROFilter"), GetLastError());
*/
//
//
//////////////////////////////////////////////////////////////////////////

		pb.Step();

		//
		// LANSCSIMiniportDevice
		//

		DebugPrintf(TEXT("LANSCSIMiniportDevice\n"));
		MsiRecordSetString(hActionRec, 1, TEXT("LANSCSIMiniportDevice"));
		iResult = MsiProcessMessage(hInstall, INSTALLMESSAGE_ACTIONDATA, hActionRec);

		iNdRet = RemoveLANSCSIMiniportDevice();
	ProcessUninstallError(hInstall, iNdRet, TEXT("LANSCSIMiniportDevice"), GetLastError());
		RemoveLANSCSIMiniportDriver();
		pb.Step();

		//
		// LANSCSIBusDevice
		//

		DebugPrintf(TEXT("LANSCSIBusDevice\n"));
		MsiRecordSetString(hActionRec, 1, TEXT("LANSCSIBusDevice"));
		iResult = MsiProcessMessage(hInstall, INSTALLMESSAGE_ACTIONDATA, hActionRec);
		
		iNdRet = RemoveLANSCSIBusDevice();
	ProcessUninstallError(hInstall, iNdRet, TEXT("LANSCSIBusDevice"), GetLastError());
		RemoveLANSCSIBusDriver();
		pb.Step();

		//
		// LPX Protocol
		//

		DebugPrintf(TEXT("LPX Protocol\n"));
		MsiRecordSetString(hActionRec, 1, TEXT("LPX Protocol"));
		iResult = MsiProcessMessage(hInstall, INSTALLMESSAGE_ACTIONDATA, hActionRec);

		iNdRet = UninstallLPX();
	ProcessUninstallError(hInstall, iNdRet, TEXT("LPX Protocol"), GetLastError());
		pb.Step();
 
		// Driver Services are not actually removed without a reboot.
		// Initiate the system reboot forcibly here.
		
		// ** Uninstall always requires reboot, which is set from InstallShield ND_ForceReboot
		// SetRebootFlag(TRUE);

	uiReturn = ERROR_SUCCESS;

out:
		LogEnd();

	return uiReturn;
}

//++
//
// NDMsiCheckDriver
//
//
//
// not implemented yet
//

NDINST_API UINT __stdcall NDMsiCheckDriver(MSIHANDLE hInstall)
{
	DebugPrintf(TEXT("MsiCheckDriver called\n"));
	// MessageBox(GetForegroundWindow(), TEXT("MsiCheckDriver"), TEXT(""), MB_OK);
	return ERROR_SUCCESS;
}

//++
//
// NDMsiSetReboot
//
// reads the registry key to determine whether the reboot is required.
// value is set by the prior routines.
// after use the value, clean the key property
//
// key   : HKLM\Software\XiMeta\NetDisk\Setup
// value : RebootRequired (REG_DWORD)
// data  : 1
//

NDINST_API int __stdcall NDMsiSetReboot(MSIHANDLE hInstall)
{
	BOOL bReboot;
	DebugPrintf(TEXT("Entering NDMsiSetReboot()...\n"));
	GetRebootFlag(&bReboot);
	DebugPrintf(TEXT("Reboot Flags is %d\n"), bReboot);
	ClearRebootFlag();
	if (bReboot)
	{
		MsiSetProperty(hInstall, TEXT("REBOOT"), TEXT("Force"));
	}
	DebugPrintf(TEXT("Leaving NDMsiSetReboot()...\n"));
	return ERROR_SUCCESS;
}

//++
//
// NDMsiCheckSystemStatus
//
//

BOOL IsDeleteMarkedService(LPCTSTR szServiceKey)
{
	TCHAR szKey[MAX_PATH + 1] = {0};
	HKEY hServiceKey;
	DWORD dwFlag, dwFlagSize;

	StringCchPrintf(szKey, MAX_PATH, TEXT("System\\CurrentControlSet\\Services\\%s"), szServiceKey);
	dwFlagSize = sizeof(dwFlag);
	if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKey, 0, KEY_QUERY_VALUE, &hServiceKey) &&
		ERROR_SUCCESS == RegQueryValueEx(hServiceKey, TEXT("DeleteFlag"), 0, NULL, (LPBYTE) &dwFlag, &dwFlagSize) &&
		dwFlag == 1)
		return TRUE;
	return FALSE;			
}


/****************************************************************************
*
*    FUNCTION: StartDriver( IN SC_HANDLE, IN LPCTSTR)
*
*    PURPOSE: Starts the driver service.
*
****************************************************************************/
BOOL
IsServiceRunning(
		IN LPCTSTR ServiceName
	) {
	SC_HANDLE schSCManager;
    SC_HANDLE  schService;
    BOOL       ret;
	SERVICE_STATUS	ServiceStatus ;

	schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
    if ( schSCManager == NULL ) {
		DebugPrintf(TEXT(__FILE__) TEXT("[NDInst] IsServiceRunning: OpenSCManager() Failed. ErrCode:%d\n"), GetLastError());
        return FALSE;
    }

    schService = OpenService( schSCManager,
                              ServiceName,
                              SERVICE_ALL_ACCESS
                              );
    if ( schService == NULL ) {
	 	CloseServiceHandle( schSCManager );
		DebugPrintf(TEXT(__FILE__) TEXT("[NDInst] IsServiceRunning: OpenService() Failed. ErrCode:%d\n"), GetLastError());
        return FALSE;
	}

    ret = QueryServiceStatus( schService, &ServiceStatus ) ;
	if(ret == FALSE) {
		DebugPrintf(TEXT(__FILE__) TEXT("[NDInst] IsServiceRunning: QueryServiceStatus() Failed. ErrCode:%d\n"), GetLastError());
		goto cleanup ;
	}

	if(ServiceStatus.dwCurrentState == SERVICE_RUNNING)
		ret = TRUE ;
	else {
		DebugPrintf(TEXT(__FILE__) TEXT("[NDInst] IsServiceRunning: Service is not running.\n"));
		ret = FALSE ;
	}

cleanup:
    CloseServiceHandle( schService );
 	CloseServiceHandle( schSCManager );

    return ret;
}


NDINST_API UINT __stdcall NDMsiCheckSystemStatus(MSIHANDLE hInstall)
{
	HWND hDlg;
	int i, j, iRet, nFound = 0;
	ULONG	NDEnabled ;
	PMSIHANDLE hRecord;
	struct { TCHAR szExecutable[256]; TCHAR szCaption[256]; BOOL bFound; } 
	ndWindows[] = { 
			{ TEXT("Admin.exe"), TEXT("NetDisk Administrator"), FALSE },
			{ TEXT("AggrMirUI.EXE"), TEXT("NetDisk Aggregation & Mirroring"), FALSE },
			// added for Gennetworks OEM
			{ TEXT("Admin.exe"), TEXT("GenDisk Administrator"), FALSE },
			{ TEXT("AggrMirUI.EXE"), TEXT("GenDisk Aggregation & Mirroring"), FALSE},
			// added for Iomega OEM
			{ TEXT("Admin.exe"), TEXT("Network Hard Drive Administrator"), FALSE },
			{ TEXT("AggrMirUI.EXE"), TEXT("Network Hard Drive Aggregation & Mirroring"), FALSE},
			// added for Logitec OEM
			{ TEXT("Admin.exe"), TEXT("LHD-LU2 Administrator"), FALSE },
			{ TEXT("AggrMirUI.EXE"), TEXT("LHD-LU2 Aggregation & Mirroring"), FALSE},
			// added for Moritani OEM
			{ TEXT("Admin.exe"), TEXT("Eoseed Administrator"), FALSE },
			{ TEXT("AggrMirUI.EXE"), TEXT("Eoseed Aggregation & Mirroring"), FALSE}
	};
	const int nNdWindows = sizeof(ndWindows) / sizeof(ndWindows[0]);
	BOOL	BRet ;

	WORD wLangId;

	DebugPrintf(TEXT(__FILE__) TEXT("[NDInst] NDMsiCheckSystemStatus: Entered.\n"));

	//
	// Check services
	// 
	// * Remarks
	// If services are "Marked for deletion",
	// the system should be restarted before continuing installation.
	// That flag can be checked at the registry key:
	//
	// Key:   HKLM\System\CurrentControlSet\Services\[ServiceName]
	// Value: DeleteFlag
	// Data:  0x0001 (REG_DWORD)
	//
	
	// Check ROFILT
	// Check LPX
	// Check NetDisk_Service
	// Check LANSCSIBus
	// Check LANSCSIMiniport
	if (IsDeleteMarkedService(TEXT("ROFILT")) ||
		IsDeleteMarkedService(TEXT("LPX")) ||
		IsDeleteMarkedService(TEXT("NetDisk_Service")) || // 2.1.3 or before
		IsDeleteMarkedService(TEXT("LANSCSIHelper")) || // 2.1.4 or later
		IsDeleteMarkedService(TEXT("LANSCSIBus")) ||
		IsDeleteMarkedService(TEXT("LANSCSIMiniport")))
	{
		PMSIHANDLE hRecord = MsiCreateRecord(1);
		MsiRecordSetInteger(hRecord, 1, 8003);
		MsiProcessMessage(hInstall, INSTALLMESSAGE_ERROR, hRecord);
		// MsiProcessMessage(hInstall, INSTALLMESSAGE_FATALEXIT, NULL);
		return ERROR_INSTALL_USEREXIT;
	}

	//
	//	Don't allow to uninstall with NetDisk enabled.
	//	If the function fails, just skip to the next step.
	//
	//	added by hootch 03242004
	//
	BRet = IsServiceRunning(TEXT("LANSCSIHelper")) ;
	if(BRet) {
		iRet = IDRETRY;

		while (iRet == IDRETRY)
		{
			iRet = CheckIfAnyEnabledND(
						&NDEnabled
				) ;
			if(ERROR_SUCCESS == iRet) {
				if(NDEnabled) {
					PMSIHANDLE hRecord = MsiCreateRecord(1);
					MsiRecordSetInteger(hRecord, 1, 8004);
					iRet = MsiProcessMessage(hInstall, INSTALLMESSAGE(INSTALLMESSAGE_ERROR|MB_ABORTRETRYIGNORE|MB_ICONWARNING), hRecord);
					DebugPrintf(TEXT(__FILE__) TEXT("[NDInst] NDMsiCheckSystemStatus: MsiProcessMessage() Ret:%d\n"), iRet);
					if (IDCANCEL == iRet)
						return ERROR_INSTALL_USEREXIT;	
				}
			} else {
				DebugPrintf(TEXT(__FILE__) TEXT("[NDInst] NDMsiCheckSystemStatus: CheckIfAnyEnabledND() Failed. ErrCode:%d\n"), GetLastError());
				break ;
			}
		}
	}

	//
	// Check in-use files
	//

	// * Remarks
	// "Admin.exe" doesn't lock itself when it starts up.
	// So the installer cannot automatically check the process is running
	// without locks. This is a manual way to provide the user
	// to exit the process.
	//
	// admin.exe and aggrmirui.exe has the following captions:
	// 
	// Netdisk Administrator
	// NetDisk Aggregation & Mirroring
	//
	// If the localized versions have the different caption,
	// their entry can be added at the following ndWindows structure.
	//
	//
	SET_LANG_ID(hInstall, wLangId);

	// try to exit programs fist
	for (i = 0; i < nNdWindows; i++)
	{
		hDlg = FindWindowEx(NULL, NULL, TEXT("#32770"), ndWindows[i].szCaption);
		if (NULL != hDlg)
		{
			SendMessage(hDlg, WM_CLOSE, NULL, NULL);
			// SendMessage(hDlg, WM_QUIT, 0, NULL);
		}
	}
	

	iRet = IDRETRY;
	while (iRet == IDRETRY)
	{
		nFound = 0;
		for (i = 0; i < nNdWindows; i++)
		{
			hDlg = FindWindowEx(NULL, NULL, TEXT("#32770"), ndWindows[i].szCaption);
			ndWindows[i].bFound = (hDlg == NULL) ? FALSE : TRUE;
			if (NULL != hDlg) nFound++;
		}
		if (nFound > 0)
		{
			hRecord = MsiCreateRecord(1 + nFound * 2);
			TCHAR szMessage[256];

			LoadString(ghInst, IDS_FILE_IN_USE + GetLanguageResourceOffset(wLangId), szMessage, 256);
			MsiRecordSetString(hRecord, 0, szMessage);

			for (i = 0, j = 0; i < nNdWindows; i++)
			{
				if (TRUE == ndWindows[i].bFound)
				{
					MsiRecordSetString(hRecord, j * 2 + 1, ndWindows[i].szExecutable);
					MsiRecordSetString(hRecord, j * 2 + 2, ndWindows[i].szCaption);
				}
			}
			iRet = MsiProcessMessage(hInstall, INSTALLMESSAGE_FILESINUSE, hRecord);
			DebugPrintf(TEXT("FileInUse returned %d\n"), iRet);

			if (IDCANCEL == iRet)
				return ERROR_INSTALL_USEREXIT;	
		}
		else 
		{
			return ERROR_SUCCESS;
		}
	}

	return ERROR_SUCCESS;
}

#ifdef _DEBUG
void CALLBACK ShowDlgDebug(
  HWND hwnd,        // handle to owner window
  HINSTANCE hinst,  // instance handle for the DLL
  LPSTR lpCmdLine, // string the DLL will parse
  int nCmdShow      // show state
)
{
	HANDLE hDlgThread;
	DWORD  dwDlgThreadId;
	TCHAR  szCmdLine[MAX_PATH];

#ifdef UNICODE
	MultiByteToWideChar(CP_ACP, 0, lpCmdLine, strlen(lpCmdLine) + 1, szCmdLine, MAX_PATH);
#else
	StringCchCopy(szCmdLine, MAX_PATH, lpCmdLine);
#endif

	MessageBox(NULL, szCmdLine, TEXT(""), MB_OK);

	LANGID langID = _ttoi(szCmdLine);

	itipdlg::THREADPARAM thParam;
	thParam.wLang = langID;
	thParam.hInstance = hinst;

	hDlgThread = CreateThread(NULL, 0, itipdlg::InstallTipThreadProc, (LPVOID) &thParam, 0, &dwDlgThreadId);

	Sleep(10000);

	SendMessage(itipdlg::hWndDlg, WM_CLOSE, NULL, NULL);
	WaitForSingleObject(hDlgThread, 10000);
	CloseHandle(hDlgThread);

}

#endif
