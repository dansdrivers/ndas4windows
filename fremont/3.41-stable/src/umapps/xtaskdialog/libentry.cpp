#include "stdafx.h"
#include "xtaskdialog.h"
#include "xtaskdlg.h"

// namespace xtaskdlg {

typedef 
HRESULT
WINAPI
TASKDIALOGINDIRECT(
	const TASKDIALOGCONFIG* pTaskConfig, 
	int* pnButton, 
	int* pnRadioButton, 
	BOOL* pfVerificationFlagChecked);

typedef TASKDIALOGINDIRECT *PTASKDIALOGINDIRECT;

typedef
HRESULT 
WINAPI
TASKDIALOG(
	HWND hWndParent, 
	HINSTANCE hInstance, 
	PCWSTR pszWindowTitle, 
	PCWSTR pszMainInstruction, 
	PCWSTR pszContent, 
	TASKDIALOG_COMMON_BUTTON_FLAGS dwCommonButtons, 
	PCWSTR pszIcon, 
	int* pnButton);

typedef TASKDIALOG *PTASKDIALOG;

HMODULE comctl32ModuleHandle = NULL;	
PTASKDIALOGINDIRECT xTaskDialogIndirect = &xTaskDialogIndirectImp;
PTASKDIALOG xTaskDialog = &xTaskDialogImp;

VOID
WINAPI
xTaskDialogInitialize()
{
	comctl32ModuleHandle = LoadLibraryW(L"comctl32.dll");
	if (NULL == comctl32ModuleHandle)
	{
		return;
	}
	PTASKDIALOGINDIRECT pfnTaskDialogIndirect = 
		reinterpret_cast<PTASKDIALOGINDIRECT>(
			GetProcAddress(comctl32ModuleHandle, "TaskDialogIndirect"));

	if (NULL == pfnTaskDialogIndirect)
	{
		FreeLibrary(comctl32ModuleHandle);
		comctl32ModuleHandle = NULL;
		return;
	}

	xTaskDialogIndirect = pfnTaskDialogIndirect;

	PTASKDIALOG pfnTaskDialog = reinterpret_cast<PTASKDIALOG>(
		GetProcAddress(comctl32ModuleHandle, "TaskDialog"));

	ATLASSERT(NULL != pfnTaskDialog);
	
	xTaskDialog = pfnTaskDialog;

	return;
}

VOID
WINAPI
xTaskDialogUninitialize()
{
	if (NULL != comctl32ModuleHandle)
	{
		xTaskDialogIndirect = &xTaskDialogIndirectImp;
		xTaskDialog = &xTaskDialogImp;
		FreeLibrary(comctl32ModuleHandle);
		comctl32ModuleHandle = NULL;
	}
}

HRESULT
WINAPI
xTaskDialogIndirectImp(
	const TASKDIALOGCONFIG* pTaskConfig, 
	__out_opt int* pnButton, 
	__out_opt int* pnRadioButton, 
	__out_opt BOOL* pfVerificationFlagChecked)
{
	CXTaskDialog dlg;
	return dlg.TaskDialogIndirect(
		pTaskConfig, 
		pnButton,
		pnRadioButton, 
		pfVerificationFlagChecked);
}

HRESULT 
WINAPI
xTaskDialogImp(
	__in_opt HWND hWndParent, 
	__in_opt HINSTANCE hInstance, 
	__in_opt PCWSTR pszWindowTitle, 
	__in_opt PCWSTR pszMainInstruction, 
	__in_opt PCWSTR pszContent, 
	TASKDIALOG_COMMON_BUTTON_FLAGS dwCommonButtons, 
	__in_opt PCWSTR pszIcon, 
	__out_opt int* pnButton)
{
	TASKDIALOGCONFIG config;
	ZeroMemory(&config, sizeof(TASKDIALOGCONFIG));
	config.cbSize = sizeof(TASKDIALOGCONFIG);
	config.hwndParent = hWndParent;
	config.hInstance = hInstance;
	config.pszWindowTitle = pszWindowTitle;
	config.pszMainInstruction = pszMainInstruction;
	config.pszContent = pszContent;
	config.dwCommonButtons = dwCommonButtons;
	config.pszMainIcon = pszIcon;
	return xTaskDialogIndirectImp(&config, pnButton, NULL, NULL);
}
