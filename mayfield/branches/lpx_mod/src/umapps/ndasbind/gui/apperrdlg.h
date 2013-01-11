#pragma once
#include "resource.h"

class CAppErrorDlg :
	public CDialogImpl<CAppErrorDlg>
{
	BEGIN_MSG_MAP_EX(CAppErrorDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_CLOSE(OnClose)
		COMMAND_ID_HANDLER_EX(IDOK,OnOK)
	END_MSG_MAP()

	WTL::CString m_strMessage;
	WTL::CString m_strDescription;
	WTL::CString m_strTitle;

	CStatic m_wndMessage;
	CEdit m_wndDescription;
	CStatic m_wndIcon;

public:
	enum { IDD = IDD_ERROR };

	CAppErrorDlg(
		LPCTSTR szMessage, 
		LPCTSTR szDescription,
		LPCTSTR strTitle = NULL);

	LRESULT OnInitDialog(HWND hWndFocus, LPARAM lParam);
	VOID OnOK(UINT wNotifyCode, INT wID, HWND hWndCtl);
	VOID OnClose() { EndDialog(IDCLOSE); }
};

INT_PTR 
ShowErrorMessageBox(
	UINT uMessageID,
	UINT uTitleID = 0,
	HWND hWnd = ::GetActiveWindow(), 
	DWORD dwError = ::GetLastError());

INT_PTR 
ShowErrorMessageBox(
	LPCTSTR szMessage,
	LPCTSTR szTitle = NULL,
	HWND hWnd = ::GetActiveWindow(), 
	DWORD dwError = ::GetLastError());
