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

extern ndas::DeviceColl* _pDeviceColl;
extern ndas::LogicalDeviceColl* _pLogDevColl;

INT_PTR ShowErrorMessageBox(
	UINT uMessageID,
	UINT uTitleID = 0,
	HWND hWnd = ::GetActiveWindow(), 
	DWORD dwError = ::GetLastError());

INT_PTR ShowErrorMessageBox(
	LPCTSTR szMessage,
	LPCTSTR szTitle = NULL,
	HWND hWnd = ::GetActiveWindow(), 
	DWORD dwError = ::GetLastError());

