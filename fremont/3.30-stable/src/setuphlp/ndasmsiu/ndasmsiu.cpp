#include "precomp.hpp"
#include <instmsg.hpp>
#include <ndas/ndasportioctl.h>

// copied from src\inc\ndasbusioctl.h
#define NDASMINIPORT_HARDWARE_IDS_A "NDAS\\SCSIAdapter_R01\0"

const LPCTSTR NDASMGMT_INST_UID =  _T("{1A7FAF5F-4D93-42d3-88AB-35590810D61F}");
const WPARAM NDASMGMT_INST_WPARAM_EXIT = 0xFF02;

VOID xMsiLogvPrintf(MSIHANDLE hInstall, LPCTSTR Prefix, LPCTSTR Format, va_list ap);
VOID xMsiLogPrintf(MSIHANDLE hInstall, LPCTSTR Prefix, LPCTSTR Format, ...);

using namespace local;

BOOL
PostToStopNdasmgmt()
{
    UINT uMsgId = GetInstanceMessageId(NDASMGMT_INST_UID);
    if (0 == uMsgId)
    {
        return FALSE;
    }
    return PostInstanceMessage(uMsgId, NDASMGMT_INST_WPARAM_EXIT, 0);
}

extern "C"
UINT
__stdcall
NdasMsiStopNdasmgmt(MSIHANDLE hInstall)
{
    UNREFERENCED_PARAMETER(hInstall);
    (void) PostToStopNdasmgmt();
    return ERROR_SUCCESS;
}

extern "C" UINT __stdcall NdasMsiDetectNdasLogicalUnitInstances(MSIHANDLE hInstall);
extern "C" UINT __stdcall NdasMsiDetectNdasScsiInstances(MSIHANDLE hInstall);

static
HRESULT 
NdasMsipEnsureNoActiveNdasBusLogicalUnitInstances(
	IN MSIHANDLE hInstall,
	IN HWND hwndParent,
	IN LPCTSTR HardwareId,
	IN BOOL PresentOnly)
{
	BOOL Found = FALSE;
	HRESULT hr;
	BOOL fSuccess = FALSE;

	HDEVINFO devInfoSet = INVALID_HANDLE_VALUE;
	SP_DEVINFO_DATA DeviceInfoData = {0};
	DWORD Flags = DIGCF_ALLCLASSES;

	if (PresentOnly) {
		Flags |= DIGCF_PRESENT;
	}

	devInfoSet = SetupDiGetClassDevs(
		NULL, 
		0, 
		hwndParent,
		Flags);

	if (INVALID_HANDLE_VALUE == devInfoSet)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());

		//
		// If there are no ndasport enumerator, 
		// SetupDiGetClassDevs returns ERROR_INVALID_DATA
		// in which case is not an error
		//

		xMsiLogPrintf(
			hInstall, _T("CA_NDAS_DETECT:"), 
			_T("SetupDiGetClassDevs failed, hr=0x%X"), hr);

		if (HRESULT_FROM_SETUPAPI(ERROR_INVALID_DATA) == hr)
		{
			xMsiLogPrintf(
				hInstall, _T("CA_NDAS_DETECT:"), 
				_T("Possibly no such enumerator, where it is okay to skip."));

			return S_OK;
		}

		return hr;
	}
	for (DWORD i = 0; ; ++i) {
		
		DeviceInfoData.cbSize = sizeof(DeviceInfoData);
		fSuccess = SetupDiEnumDeviceInfo(
			devInfoSet,
			i,
			&DeviceInfoData);

		if (!fSuccess) {
			break;
		}

		DWORD DataT;
		LPTSTR buffer = NULL;
		DWORD buffersize = 0;

		//
		// We won't know the size of the HardwareID buffer until we call
		// this function. So call it with a null to begin with, and then 
		// use the required buffer size to Alloc the nessicary space.
		// Keep calling we have success or an unknown failure.
		//
		while (TRUE) {

			fSuccess = SetupDiGetDeviceRegistryProperty(
				devInfoSet,
				&DeviceInfoData,
				SPDRP_HARDWAREID,
				&DataT,
				(PBYTE)buffer,
				buffersize,
				&buffersize);

			if (fSuccess) {
				break;
			}

			if (ERROR_INVALID_DATA == GetLastError()) {
				//
				// May be a Legacy Device with no HardwareID. Continue.
				//
				break;
			} else if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
				//
				// We need to change the buffer size.
				//
				if (buffer) {
					LocalFree(buffer);
				}
				buffer = (LPTSTR) LocalAlloc(LPTR,buffersize);

			} else {
				//
				// Unknown Failure.
				//
				goto cleanup;
			}            
		}

		//
		// May be a Legacy Device with no HardwareID. Continue.
		//
		if (GetLastError() == ERROR_INVALID_DATA) {
			continue;
		}

		//
		// Compare each entry in the buffer multi-sz list with our HardwareID.
		//
		for (LPTSTR p = buffer; 
			*p && (p < & buffer[buffersize]);
			p += lstrlen(p) + 1) 
		{
			if (0 == lstrcmpi(HardwareId,p)) {
				Found = TRUE;
				break;
			}
		}

		if (buffer) {
			LocalFree(buffer);
		}

		if (Found) {
			break;
		}
	}

	if (Found) {
		//
		// there are active instances
		//
		xMsiLogPrintf(
			hInstall, _T("CA_NDAS_DETECT:"), 
			_T("An active instance is found."));
		SetLastError(ERROR_SUCCESS);
		hr = S_FALSE;
	} else {
		//
		// No active instances are found.
		//

		xMsiLogPrintf(
			hInstall, _T("CA_NDAS_DETECT:"), 
			_T("No active instances are found."));
		hr = S_OK;
	}

cleanup:

	DWORD err = GetLastError();

	if (INVALID_HANDLE_VALUE != devInfoSet) {
		(VOID) SetupDiDestroyDeviceInfoList(devInfoSet);
	}

	SetLastError(err);
	return hr;
}
//
// Find any instances using NDASPORT enumerator
//
// Return codes:
// S_OK    : there are no active instances
// S_FALSE : there are active instances
// E_XXX   : for all other failures
//

HRESULT NdasMsipEnsureNoActiveNdasPortLogicalUnitInstances(MSIHANDLE hInstall)
{
	HRESULT hr;

	HDEVINFO devInfoSet = SetupDiGetClassDevs(
		NULL, 
		NDASPORT_ENUMERATOR, 
		NULL, 
		DIGCF_ALLCLASSES | DIGCF_PRESENT);

	if (INVALID_HANDLE_VALUE == devInfoSet)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());

		//
		// If there are no ndasport enumerator, 
		// SetupDiGetClassDevs returns ERROR_INVALID_DATA
		// in which case is not an error
		//

		xMsiLogPrintf(
			hInstall, _T("CA_NDAS_DETECT:"), 
			_T("SetupDiGetClassDevs failed, hr=0x%X"), hr);

		if (HRESULT_FROM_SETUPAPI(ERROR_INVALID_DATA) == hr)
		{
			xMsiLogPrintf(
				hInstall, _T("CA_NDAS_DETECT:"), 
				_T("Possibly no such enumerator, where it is okay to skip."));

			return S_OK;
		}

		return hr;
	}

	SP_DEVINFO_DATA devInfoData = { sizeof(SP_DEVINFO_DATA) };
	BOOL success = SetupDiEnumDeviceInfo(devInfoSet, 0, &devInfoData);

	if (success)
	{
		//
		// there are active instances
		//
		xMsiLogPrintf(
			hInstall, _T("CA_NDAS_DETECT:"), 
			_T("An active instance is found."));

		XTLVERIFY( SetupDiDestroyDeviceInfoList(devInfoSet) );
		return S_FALSE;
	}
	else if (ERROR_NO_MORE_ITEMS != GetLastError())
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());

		xMsiLogPrintf(
			hInstall, _T("CA_NDAS_DETECT:"), 
			_T("SetupDiEnumDeviceInfo failed, hr=0x%X"), hr);

		XTLVERIFY( SetupDiDestroyDeviceInfoList(devInfoSet) );
		return hr;
	}
	//
	// No active instances are found.
	//

	xMsiLogPrintf(
		hInstall, _T("CA_NDAS_DETECT:"), 
		_T("No active instances are found."));

	XTLVERIFY(SetupDiDestroyDeviceInfoList(devInfoSet));
	return S_OK;
}

UINT NdasMsiDetectNdasLogicalUnitInstances(MSIHANDLE hInstall)
{
	UINT ret;

	TCHAR dialogNumberString[32];
	DWORD dialogNumberStringLength = RTL_NUMBER_OF(dialogNumberString);
	LPCTSTR propertyName = TEXT("NdasMsiDetectNdasLogicalUnitInstances");

	if (MsiGetMode(hInstall, MSIRUNMODE_SCHEDULED))
	{
		propertyName = IPROPNAME_CUSTOMACTIONDATA;
	}

	ret = MsiGetProperty(
		hInstall, 
		propertyName, 
		dialogNumberString, 
		&dialogNumberStringLength);

	if (ERROR_SUCCESS != ret)
	{
		xMsiLogPrintf(
			hInstall, _T("CA_NDAS_DETECT:"), 
			_T("MsiGetProperty failed, property=%s, ret=%d"),
			propertyName, ret);

		// return ERROR_INSTALL_FAILURE;
		return ERROR_SUCCESS;
	}

	int dialogNumber = _ttoi(dialogNumberString);

	while (TRUE)
	{
		HRESULT hrPort = NdasMsipEnsureNoActiveNdasPortLogicalUnitInstances(hInstall);

		if (FAILED(hrPort))
		{
			xMsiLogPrintf(
				hInstall, _T("CA_NDAS_DETECT:"), 
				_T("NdasMsipEnsureNoActiveNdasPortLogicalUnitInstances failed, hr=0x%X"),
				hrPort);

			// return ERROR_INSTALL_FAILURE;
			return ERROR_SUCCESS;
		}

		HRESULT hrScsi = NdasMsipEnsureNoActiveNdasBusLogicalUnitInstances(
			hInstall,
			NULL,
			_T(NDASMINIPORT_HARDWARE_IDS_A),
			TRUE);

		if (FAILED(hrScsi))
		{
			xMsiLogPrintf(
				hInstall, _T("CA_NDAS_DETECT:"), 
				_T("NdasMsipEnsureNoActiveNdasBusLogicalUnitInstances failed, hr=0x%X"),
				hrScsi);

			// return ERROR_INSTALL_FAILURE;
			return ERROR_SUCCESS;
		}

		if (S_OK == hrPort && S_OK == hrScsi)
		{
			return ERROR_SUCCESS;
		}

		PMSIHANDLE hRecord = MsiCreateRecord(4);

		if (NULL == hRecord)
		{
			xMsiLogPrintf(
				hInstall, _T("CA_NDAS_DETECT:"), 
				_T("MsiCreateRecord(4) failed."));

			// return ERROR_INSTALL_FAILURE;
			return ERROR_SUCCESS;
		}

		//
		// (Workaround)
		// Send [4] as "\n" for more pretty text format.
		// Error Dialog Text property cannot contain "\n".
		// 

		ret = MsiRecordSetInteger(hRecord, 1, dialogNumber);
		XTLASSERT(ERROR_SUCCESS == ret);

		ret = MsiRecordSetString(hRecord, 4, _T("\n"));
		XTLASSERT(ERROR_SUCCESS == ret);

		ret = MsiProcessMessage(
			hInstall, 
			static_cast<INSTALLMESSAGE>(
				INSTALLMESSAGE_USER | 
				MB_ABORTRETRYIGNORE |
				MB_ICONWARNING),
			hRecord);

		if (IDRETRY == ret)
		{
			xMsiLogPrintf(
				hInstall, _T("CA_NDAS_DETECT:"), 
				_T("MsiProcessMessage returned %d (IDRETRY)"), ret);

			continue;
		}
		else if (IDABORT == ret)
		{
			xMsiLogPrintf(
				hInstall, _T("CA_NDAS_DETECT:"), 
				_T("MsiProcessMessage returned %d (IDABORT)"), ret);

			return ERROR_INSTALL_USEREXIT;
		}
		else if (IDIGNORE == ret)
		{
			xMsiLogPrintf(
				hInstall, _T("CA_NDAS_DETECT:"), 
				_T("MsiProcessMessage returned %d (IDIGNORE)"), ret);

			return ERROR_SUCCESS;
		}

		xMsiLogPrintf(
			hInstall, _T("CA_NDAS_DETECT:"), 
			_T("MsiProcessMessage returned %d (FAILURE)"), ret);

		// return ERROR_INSTALL_FAILURE;
		return ERROR_SUCCESS;
	}
}

UINT NdasMsiDetectNdasScsiInstances(MSIHANDLE hInstall)
{
	return ERROR_SUCCESS;
}

VOID xMsiLogvPrintf(MSIHANDLE hInstall, LPCTSTR Prefix, LPCTSTR Format, va_list ap)
{
	TCHAR buffer[256];

	PMSIHANDLE hRecord = MsiCreateRecord(2);

	if (NULL == hRecord) return;

	StringCchVPrintf(buffer, RTL_NUMBER_OF(buffer), Format, ap);

	MsiRecordSetString(hRecord, 0, _T("[1] [2]"));
	MsiRecordSetString(hRecord, 1, Prefix);
	MsiRecordSetString(hRecord, 2, buffer);

	MsiProcessMessage(hInstall, INSTALLMESSAGE_INFO, hRecord);
}

VOID xMsiLogPrintf(MSIHANDLE hInstall, LPCTSTR Prefix, LPCTSTR Format, ...)
{
	va_list ap;
	va_start(ap, Format);
	xMsiLogvPrintf(hInstall, Prefix, Format, ap);
	va_end(ap);
}
