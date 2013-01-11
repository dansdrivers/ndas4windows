// ndasmgmt.h
#pragma once
#include "ndascls.h"
#include "ndasuser.h"

#define APP_INST_UID _T("{1A7FAF5F-4D93-42d3-88AB-35590810D61F}")

#define WMSZ_APP_EXIT		(APP_INST_UID _T("_EXIT"))
#define WMSZ_APP_WELCOME	(APP_INST_UID _T("_WELCOME"))
#define WMSZ_APP_POPUP		(APP_INST_UID _T("_POPUP"))

extern UINT WM_APP_POPUP;
extern UINT WM_APP_WELCOME;
extern UINT WM_APP_EXIT;

#define AIMSG_POPUP		0xFF00
#define AIMSG_WELCOME	0xFF01
#define AIMSG_EXIT		0xFF02
#define AIMSG_REGDEV	0xFF03

typedef struct _APPINSTMSG {
	WORD aiMsg;
} APPINSTMSG, *PAPPINSTMSG;

#define WM_APP_NDAS_DEVICE_STATUS_CHANGED		(WM_APP + 0x980)
#define WM_APP_NDAS_LOGICALDEVICE_STATUS_CHANGED	(WM_APP + 0x981)
#define WM_APP_NDAS_DEVICE_ENTRY_CHANGED		(WM_APP + 0x982)
#define WM_APP_NDAS_LOGICALDEVICE_ENTRY_CHANGED	(WM_APP + 0x983)
#define WM_APP_NDAS_LOGICALDEVICE_DISCONNECTED	(WM_APP + 0x990)
#define WM_APP_NDAS_LOGICALDEVICE_RECONNECTING	(WM_APP + 0x991)
#define WM_APP_NDAS_LOGICALDEVICE_RECONNECTED	(WM_APP + 0x992)
#define WM_APP_NDAS_SERVICE_TERMINATING (WM_APP + 0x993)

extern ndas::DeviceColl* _pDeviceColl;
extern ndas::LogicalDeviceColl* _pLogDevColl;

inline INT_PTR ShowErrorMessage(HWND hWnd = NULL, DWORD dwError = ::GetLastError())
{
	WTL::CString strText;
	WTL::CString strCaption = TEXT("Error");

	hWnd = ::GetActiveWindow();

	if (dwError & APPLICATION_ERROR_MASK) {

		HMODULE hModule = NULL;

		hModule = ::LoadLibraryEx(
			_T("ndasmsg.dll"), 
			NULL, 
			LOAD_LIBRARY_AS_DATAFILE);

		if (NULL == hModule) {

			strText.Format(
				_T("NDAS User Error \nError Code: %d (0x%08X)"), dwError, dwError);

		} else {

			LPTSTR lpszErrorMessage;
			BOOL fSuccess = ::FormatMessage(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE,
				hModule,
				dwError,
				0,
				(LPTSTR) &lpszErrorMessage,
				0,
				NULL);

			if (fSuccess) {

				strText.Format(
					_T("NDAS User Error\nError Code: %d (0x%08X)\n%s"),
					dwError, dwError, lpszErrorMessage);

				::LocalFree(lpszErrorMessage);
			} else {
			}
		}

		::FreeLibrary(hModule);


	} else {
		LPTSTR lpszErrorMessage;
		BOOL fSuccess = ::FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, 
			dwError, 
			0, 
			(LPTSTR) &lpszErrorMessage, 
			0, 
			NULL);

		strText.Format(_T("Error from the system\nError Code: %d (0x%08X)\n\n%s"),
			dwError, dwError, 
			(fSuccess) ? lpszErrorMessage : _T("Error Description Not Available"));

		if (fSuccess) {
			::LocalFree(lpszErrorMessage);
		}
	}

	return MessageBox(hWnd, strText, strCaption, MB_OK | MB_ICONERROR);
}
