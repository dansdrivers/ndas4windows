#pragma once
#ifndef _XTASKDLG_H_
#define _XTASKDLG_H_

#include <commctrl.h>

#ifdef __cplusplus
extern "C" {
#endif

VOID
WINAPI
xTaskDialogInitialize();

VOID
WINAPI
xTaskDialogUninitialize();

extern
HRESULT
(WINAPI *xTaskDialogIndirect)(
	const TASKDIALOGCONFIG* pTaskConfig, 
	__out_opt int* pnButton, 
	__out_opt int* pnRadioButton, 
	__out_opt BOOL* pfVerificationFlagChecked);

extern
HRESULT 
(WINAPI *xTaskDialog)(
	__in_opt HWND hWndParent, 
	__in_opt HINSTANCE hInstance, 
	__in_opt PCWSTR pszWindowTitle, 
	__in_opt PCWSTR pszMainInstruction, 
	__in_opt PCWSTR pszContent, 
	TASKDIALOG_COMMON_BUTTON_FLAGS dwCommonButtons, 
	__in_opt PCWSTR pszIcon, 
	__out_opt int* pnButton);

HRESULT
WINAPI
xTaskDialogIndirectImp(
	const TASKDIALOGCONFIG* pTaskConfig, 
	__out_opt int* pnButton, 
	__out_opt int* pnRadioButton, 
	__out_opt BOOL* pfVerificationFlagChecked);

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
	__out_opt int* pnButton);

#ifdef __cplusplus
}
#endif

#endif
