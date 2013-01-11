#pragma once
#include "resource.h"
#include "ndasmgmt.h"

class CAppErrorDlg :
	public CDialogImpl<CAppErrorDlg>
{
	BEGIN_MSG_MAP_EX(CAppErrorDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_RANGE_HANDLER_EX(IDOK,IDNO,OnCloseCmd)
	END_MSG_MAP()

	CString m_strMessage;
	CString m_strDescription;
	CString m_strTitle;

	CStatic m_wndMessage;
	CEdit m_wndDescription;
	CStatic m_wndIcon;

public:

	enum { IDD = IDD_ERROR };

	CAppErrorDlg(
		ATL::_U_STRINGorID szMessage, 
		ATL::_U_STRINGorID szDescription,
		ATL::_U_STRINGorID szTitle);

	CAppErrorDlg(
		const CString& strMessage, 
		const CString& strDescription,
		const CString& strTitle);

	LRESULT OnInitDialog(HWND hWndFocus, LPARAM lParam);
	void OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/)
	{
		EndDialog(wID);
	}
};

INT_PTR 
ErrorMessageBox(
	HWND hWndOwner,
	ATL::_U_STRINGorID Message,
	ATL::_U_STRINGorID Title = IDS_ERROR_TITLE,
	LANGID LanguageId = ndasmgmt::CurrentUILangID,
	DWORD ErrorCode = ::GetLastError());
