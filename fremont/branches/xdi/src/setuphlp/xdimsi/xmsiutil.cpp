#include "precomp.hpp"
#include "xmsiutil.h"
#include "xmsitrace.h"

UINT
pxMsiRecordGetString(
	__in MSIHANDLE hRecord,
	__in UINT Field,
	__deref_out LPWSTR* ValueBuffer,
	__out_opt LPDWORD ValueBufferLength)
{
	*ValueBuffer = NULL;
	if (ValueBufferLength) *ValueBufferLength = 0;

	UINT len = MsiRecordDataSize(hRecord, Field);
	++len;
	
	LPWSTR p = (LPWSTR) calloc(len, sizeof(WCHAR));

	if (NULL == p)
	{
		return ERROR_OUTOFMEMORY;
	}
	
	DWORD buflen = len;
	UINT ret = MsiRecordGetStringW(hRecord, Field, p, &buflen);

	if (ERROR_SUCCESS != ret) 
	{
		free(p);
		return ret;
	}

	*ValueBuffer = p;
	if (ValueBufferLength) *ValueBufferLength = buflen;

	return ret;
}


UINT
pxMsiFormatRecord(
	__in MSIHANDLE hInstall,
	__in LPCWSTR Format,
	__deref_inout LPWSTR* Result,
	__out_opt LPDWORD ResultLength)
{
	*Result = NULL;
	if (ResultLength) *ResultLength = 0;

	PMSIHANDLE hRecord = MsiCreateRecord(0);
	if (NULL == hRecord)
	{
		return ERROR_OUTOFMEMORY;
	}

	UINT ret = MsiRecordSetStringW(hRecord, 0, Format);
	
	if (ERROR_SUCCESS != ret)
	{
		return ret;
	}

	DWORD len = 0;
	LPWSTR p = NULL;
	ret = MsiFormatRecordW(hInstall, hRecord, L"", &len);
	if (ERROR_MORE_DATA == ret)
	{
		++len;
		p = (LPWSTR) calloc(len, sizeof(WCHAR));
		if (NULL == p)
		{
			return ERROR_OUTOFMEMORY;
		}
		ret = MsiFormatRecordW(hInstall, hRecord, p, &len);
		if (ERROR_SUCCESS != ret)
		{
			free(p);
			return ret;
		}
	}
	if (ERROR_SUCCESS != ret)
	{
		return ret;
	}

	*Result = p;
	if (ResultLength) *ResultLength = len;

	return ERROR_SUCCESS;
}

LPWSTR
pGetSystemErrorText(
	DWORD ErrorCode)
{
	LPWSTR lpMsgBuf = NULL;
	DWORD cchMsgBuf = FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		ErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // We can only read English!!!
		(LPWSTR) &lpMsgBuf,
		0,
		NULL);
	if (0 == cchMsgBuf)
	{
		if (lpMsgBuf) 
		{
			LocalFree((HLOCAL)lpMsgBuf);
		}
		return NULL;
	}
	return lpMsgBuf;
}

UINT 
pxMsiErrorMessageBox(
	__in MSIHANDLE hInstall, 
	__in INT ErrorDialog, 
	__in DWORD ErrorCode,
	__in LPCWSTR ErrorText)
{
	PMSIHANDLE hActionRec = MsiCreateRecord(4);
	if (NULL == hActionRec)
	{
		return ERROR_OUTOFMEMORY;
	}

	WCHAR errorCodeText[255];

	HRESULT hr = StringCchPrintfW(
		errorCodeText, 
		RTL_NUMBER_OF(errorCodeText), 
		L"0x%08X", 
		ErrorCode);

	MsiRecordSetInteger(hActionRec, 1, ErrorDialog);
	MsiRecordSetStringW(hActionRec, 2, errorCodeText);
	if (NULL != ErrorText) 
	{
		MsiRecordSetStringW(hActionRec, 3, ErrorText);
	} 

	//
	// (Workaround)
	// Send [4] as "\n" for more pretty text format.
	// Error Dialog Text property cannot contain "\n".
	// 
	MsiRecordSetStringW(hActionRec, 4, L"\n");

	UINT response = MsiProcessMessage(
		hInstall, 
		INSTALLMESSAGE(INSTALLMESSAGE_USER|MB_ABORTRETRYIGNORE|MB_ICONWARNING),
		hActionRec);

	return response;
}

UINT 
pxMsiErrorMessageBox(
	__in MSIHANDLE hInstall, 
	__in INT ErrorNumber, 
	__in DWORD ErrorCode)
{
	LPWSTR errorDescription = pGetSystemErrorText(ErrorCode);

	//
	// truncate \r or \n at the end of szErrorText
	//
	INT len = lstrlenW(errorDescription);
	if (len > 0)
	{
		LPWSTR lpTrailing = &errorDescription[len-1];
		if (L'\n' == *lpTrailing || L'\r' == *lpTrailing)
		{
			*lpTrailing = L'\0'; 
		}
		if (len > 1 && 
			(L'\n' == *(lpTrailing-1) || L'\r' == *(lpTrailing-1)))
		{
			*(lpTrailing-1) = L'\0'; 
		}
	}

	UINT response = pxMsiErrorMessageBox(
		hInstall, 
		ErrorNumber, 
		ErrorCode, 
		errorDescription);

	if (errorDescription) 
	{
		LocalFree((HLOCAL)errorDescription);
	}

	return response;
}

UINT
pxMsiScheduleRebootForDeferredCA(
	__in MSIHANDLE hInstall)
{
	HKEY keyHandle = (HKEY) INVALID_HANDLE_VALUE;

	//
	// Create a key for scheduled reboot
	//
	LONG result = RegCreateKeyExW(
		HKEY_LOCAL_MACHINE, 
		L"Software\\NDAS\\DeferredInstallReboot",
		0,
		NULL,
		REG_OPTION_VOLATILE, // volatile key will be deleted after reboot
		KEY_READ,
		NULL,
		&keyHandle,
		NULL);

	if (ERROR_SUCCESS == result)
	{
		RegCloseKey(keyHandle);
	}

	pxMsiTraceW(hInstall, L"ScheduleReboot For Deferred CA completed successfully.");

	return (UINT) result;
}

LPCWSTR ScheduleRebootAtomName = L"{A046D04F-BC09-41D9-95B2-EF8B4CABEC81}";
LPCWSTR ForceRebootAtomName = L"{8588C295-EEE4-4394-8E90-827883F1A529}";

HRESULT
pxClearGlobalAtom(LPCWSTR AtomName)
{
	pxTraceW(L"pxClearGlobalAtom: %ls\n", AtomName);

	DWORD count = 0;

	for (ATOM at = GlobalFindAtomW(AtomName); 
		0 != at;
		at = GlobalFindAtomW(AtomName))
	{
		SetLastError(ERROR_SUCCESS);
		GlobalDeleteAtom(at);

		pxTraceW(L"pxClearGlobalAtom: Count=%d\n", ++count);

		if (ERROR_SUCCESS != GetLastError())
		{
			return HRESULT_FROM_WIN32(GetLastError());
		}
	}
	return S_OK;
}

HRESULT
pxMsiClearScheduleReboot()
{
	return pxClearGlobalAtom(ScheduleRebootAtomName);
}

HRESULT
pxMsiClearForceReboot()
{
	return pxClearGlobalAtom(ForceRebootAtomName);
}

UINT
pxMsiQueueScheduleReboot(
	__in MSIHANDLE hInstall)
{
	if (MsiGetMode(hInstall, MSIRUNMODE_SCHEDULED))
	{
		pxMsiTraceW(hInstall, L"REBOOT: Schedule Reboot to complete the operation\n");

		ATOM rebootScheduled = GlobalFindAtomW(ScheduleRebootAtomName);
		if (0 == rebootScheduled)
		{
			rebootScheduled = GlobalAddAtomW(ScheduleRebootAtomName);
			if (0 == rebootScheduled)
			{
				return GetLastError();
			}
		}
		return ERROR_SUCCESS;
	}
	else if (MsiGetMode(hInstall, MSIRUNMODE_ROLLBACK))
	{
		return ERROR_SUCCESS;
	}
	else
	{
		return MsiDoActionW(hInstall, L"ScheduleReboot");
	}
}

UINT
pxMsiQueueForceReboot(
	__in MSIHANDLE hInstall)
{
	if (MsiGetMode(hInstall, MSIRUNMODE_SCHEDULED))
	{
		pxMsiTraceW(hInstall, L"FORCEREBOOT: ForceReboot is required to complete the operation\n");

		ATOM rebootScheduled = GlobalFindAtomW(ForceRebootAtomName);
		if (0 == rebootScheduled)
		{
			rebootScheduled = GlobalAddAtomW(ForceRebootAtomName);
			if (0 == rebootScheduled)
			{
				return GetLastError();
			}
		}
		return ERROR_SUCCESS;
	}
	else if (MsiGetMode(hInstall, MSIRUNMODE_ROLLBACK))
	{
		return ERROR_SUCCESS;
	}
	else
	{
		pxMsiTraceW(hInstall, L"ForceReboot to complete the operation\n");

		MsiSetMode(hInstall, MSIRUNMODE_REBOOTNOW, TRUE);

		return ERROR_INSTALL_SUSPEND;
	}
}

BOOL
pxMsiIsScheduleRebootQueued(
	__in MSIHANDLE hInstall)
{
	ATOM rebootScheduled = GlobalFindAtomW(ScheduleRebootAtomName);
	return (0 != rebootScheduled);
}

BOOL
pxMsiIsForceRebootQueued(
	__in MSIHANDLE hInstall)
{
	ATOM rebootScheduled = GlobalFindAtomW(ForceRebootAtomName);
	return (0 != rebootScheduled);
}
