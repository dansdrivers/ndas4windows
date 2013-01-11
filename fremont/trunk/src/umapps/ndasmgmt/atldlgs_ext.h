#ifndef __ATLDLGS_EXT_H__
#define __ATLDLGS_EXT_H__

#pragma once

#ifndef __cplusplus
#error ATL requires C++ compilation (use a .cpp suffix)
#endif

#ifndef __ATLDLGS_H__
#error atldlgs_ext.h requires atldlgs.h to be included first
#endif

#ifndef _XTASKDLG_H_
#error atldlgs_ext.h requires xtaskdlg.h to be included first
#endif

#if ((_WIN32_WINNT >= 0x0600) || defined(_WTL_TASKDIALOG)) && !defined(_WIN32_WCE)
#else
#error define _WTL_TASKDIALOG and include "atldlgs.h" to use this class
#endif

///////////////////////////////////////////////////////////////////////////////
// Classes in this file:
//
// CTaskDialogExImpl<T>
// CTaskDialogEx
//
// Global functions:
//   AtlTaskDialogEx()
//

namespace WTLEX 
{

template <class T>
class ATL_NO_VTABLE CTaskDialogExImpl : public WTL::CTaskDialogImpl<T>
{
public:
	CTaskDialogExImpl(HWND hWndParent = NULL) : WTL::CTaskDialogImpl<T>(hWndParent)
	{
	}
	HRESULT DoModal(HWND hWndParent = ::GetActiveWindow(), int* pnButton = NULL, int* pnRadioButton = NULL, BOOL* pfVerificationFlagChecked = NULL)
	{
		HRESULT hr = WTL::CTaskDialogImpl<T>::DoModal(hWndParent, pnButton, pnRadioButton, pfVerificationFlagChecked);
		if (E_UNEXPECTED == hr)
		{
			hr = xTaskDialogIndirect(&m_tdc, pnButton, pnRadioButton, pfVerificationFlagChecked);
		}
		return hr;
	}
};

class CTaskDialogEx : public CTaskDialogExImpl<CTaskDialogEx>
{
public:
	CTaskDialogEx(HWND hWndParent = NULL) : CTaskDialogExImpl<CTaskDialogEx>(hWndParent)
	{
		m_tdc.pfCallback = NULL;
	}
};

inline int AtlTaskDialogEx(
	HWND hWndParent, 
	ATL::_U_STRINGorID WindowTitle, ATL::_U_STRINGorID MainInstructionText, ATL::_U_STRINGorID ContentText, 
	TASKDIALOG_COMMON_BUTTON_FLAGS dwCommonButtons = 0U, ATL::_U_STRINGorID Icon = (LPCTSTR)NULL)
{
	int nRet = -1;

	typedef HRESULT (STDAPICALLTYPE *PFN_TaskDialog)(HWND hwndParent, HINSTANCE hInstance, PCWSTR pszWindowTitle, PCWSTR pszMainInstruction, PCWSTR pszContent, TASKDIALOG_COMMON_BUTTON_FLAGS dwCommonButtons, PCWSTR pszIcon, int* pnButton);

	PFN_TaskDialog pfnTaskDialog = xTaskDialog;

	HMODULE m_hCommCtrlDLL = ::LoadLibrary(_T("comctl32.dll"));
	if (m_hCommCtrlDLL != NULL)
	{
		PFN_TaskDialog pfnTaskDialog = (PFN_TaskDialog)::GetProcAddress(m_hCommCtrlDLL, "TaskDialog");
		if (NULL == pfnTaskDialog)
		{
			pfnTaskDialog = xTaskDialog;
		}
	}

	USES_CONVERSION;
	HRESULT hRet = pfnTaskDialog(hWndParent, ModuleHelper::GetResourceInstance(), T2CW(WindowTitle.m_lpstr), T2CW(MainInstructionText.m_lpstr), T2CW(ContentText.m_lpstr), dwCommonButtons, T2CW(Icon.m_lpstr), &nRet);
	ATLVERIFY(SUCCEEDED(hRet));

	if (m_hCommCtrlDLL != NULL)
	{
		::FreeLibrary(m_hCommCtrlDLL);
	}

	return nRet;
}

}

#endif
