#include "stdafx.h"
#include "msilog.h"
#include "fstrbuf.h"
#include "misc.h"
#include <setupapi.h>
#include "pnpdevinst.h"
#include "procdlgact.hpp"

#include "trace.h"
#ifdef RUN_WPP
#include "ddinstca.tmh"
#endif

//
// Deferred Custom Action
//
//
// CustomActionData: 
// <ierrordialog>|<hardware_id>|<inf_path>|<regpath>|<regvalue>
//
//        
NC_DLLSPEC 
UINT 
NC_CALLSPEC 
NcaMsiUpdateOrInstallPnpDevice(MSIHANDLE hInstall)
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

	TCHAR szInstalledInf[MAX_PATH] = {0};
	BOOL bRebootRequired = FALSE;
	BOOL fSuccess = FALSE;
	BOOL fUpdateExisting = FALSE;

	while (TRUE) 
	{
		bRebootRequired = FALSE;
		//
		// Specifying INSTALLFLAG_FORCE will overwrite the
		// existing device driver with the current one
		// irrespective of existence of higher version
		//
		fUpdateExisting = NdasDiFindExistingDevice(
			NULL, 
			pszHardwareID, 
			FALSE);

		if (fUpdateExisting)
		{
			fSuccess = NdasDiUpdateDeviceDriver(
				NULL, 
				pszInfPath, 
				pszHardwareID, 
				INSTALLFLAG_FORCE, 
				&bRebootRequired);

			if (!fSuccess)
			{
				MSILOGERR_PROC(_T("NdasDiUpdateDeviceDriver"));
			}

		}
		else
		{
			MSILOGERR_PROC(_T("NdasDiFindExistingDevice"));

			fSuccess = NdasDiInstallRootEnumeratedDevice(
				NULL,
				pszInfPath,
				pszHardwareID,
				INSTALLFLAG_FORCE,
				&bRebootRequired);

			if (!fSuccess)
			{
				MSILOGERR_PROC(_T("NdasDiInstallRootEnumeratedDevice"));
			}

		}

		if (!fSuccess) 
		{

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

	if (bRebootRequired)
	{
		(void) pMsiScheduleRebootForDeferredCA(hInstall);
	}

	//
	// The following actions are non-critical ones
	//

	if (fSuccess) 
	{
		//
		// set the registry value
		//
		if (!fUpdateExisting && _T('\0') != szInstalledInf[0]) 
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
// Deferred Custom Action
//
//
// CustomActionData: 
// <ierrordialog>|<hardware_id>|<inf_path>|<regpath>|<regvalue>
//
//        
NC_DLLSPEC 
UINT 
NC_CALLSPEC 
NcaMsiUpdateOrPreinstallPnpDevice(MSIHANDLE hInstall)
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

	TCHAR szInstalledInf[MAX_PATH] = {0};
	BOOL bRebootRequired = FALSE;
	BOOL fSuccess = FALSE;
	BOOL fUpdateExisting = FALSE;

	while (TRUE) 
	{
		bRebootRequired = FALSE;

		BOOL fUpdateExisting = NdasDiFindExistingDevice(
			NULL, 
			pszHardwareID, 
			FALSE);

		if (fUpdateExisting)
		{
			fSuccess = NdasDiUpdateDeviceDriver(
				NULL, 
				pszInfPath, 
				pszHardwareID, 
				INSTALLFLAG_FORCE, 
				&bRebootRequired);

			if (!fSuccess)
			{
				MSILOGERR_PROC(_T("NdasDiUpdateDeviceDriver"));
			}
		}
		else
		{
			MSILOGERR_PROC(_T("NdasDiFindExistingDevice"));

			//
			// This will show the digital signature warning dialog
			//
			CProcessDialogActivator pdact;
			pdact.Start(10000);

			fSuccess = NdasDiCopyOEMInf(
				pszInfPath, 
				0, 
				szInstalledInf, 
				MAX_PATH, 
				NULL, 
				NULL);

			{
				RESERVE_LAST_ERROR();
				pdact.Stop();
			}

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
			}
		}

		if (!fSuccess) 
		{
			if (0 == iErrorDlg) 
			{
				return ERROR_INSTALL_FAILURE;
			}

			UINT uiResponse = pMsiErrorMessageBox(hInstall, iErrorDlg);
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

	if (bRebootRequired)
	{
		(void) pMsiScheduleRebootForDeferredCA(hInstall);
	}

	//
	// The following actions are non-critical ones
	//

	if (fSuccess) 
	{
		//
		// set the registry value
		//
		if (!fUpdateExisting && _T('\0') != szInstalledInf[0]) 
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
