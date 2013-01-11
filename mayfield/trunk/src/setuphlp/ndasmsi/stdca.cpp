#include "stdafx.h"
#include "msilog.h"

NC_DLLSPEC UINT NC_CALLSPEC NcaMsiInstallExecute(MSIHANDLE hInstall);
NC_DLLSPEC UINT NC_CALLSPEC NcaMsiForceReboot(MSIHANDLE hInstall);
NC_DLLSPEC UINT NC_CALLSPEC NcaMsiInstallExecute(MSIHANDLE hInstall);
NC_DLLSPEC UINT NC_CALLSPEC NcaMsiEnableCancelButton(MSIHANDLE hInstall);
NC_DLLSPEC UINT NC_CALLSPEC NcaMsiDisableCancelButton(MSIHANDLE hInstall);

namespace
{
	// local functions
	UINT pMsiEnableCancelButton(MSIHANDLE hInstall, BOOL bEnable);

}

NC_DLLSPEC 
UINT 
NC_CALLSPEC 
NcaMsiInstallExecute(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);
	return MsiDoAction(hInstall, _T("InstallExecute"));
}

NC_DLLSPEC 
UINT 
NC_CALLSPEC 
NcaMsiForceReboot(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);
	return MsiDoAction(hInstall, _T("ForceReboot"));
}

NC_DLLSPEC 
UINT 
NC_CALLSPEC 
NcaMsiEnableCancelButton(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);
	return pMsiEnableCancelButton(hInstall, TRUE);
}

NC_DLLSPEC 
UINT 
NC_CALLSPEC 
NcaMsiDisableCancelButton(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);
	return pMsiEnableCancelButton(hInstall, FALSE);
}

//
// Local Functions
//
namespace 
{

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

} // namespace

