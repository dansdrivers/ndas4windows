#pragma once
#include "resource.h"

class CConfirmDlg :
	public CDialogImpl<CConfirmDlg>
{
	CStatic m_wndIcon;
	CStatic m_wndMessage;
	CButton m_wndDontShowAgain;
	CStatic m_wndHidden;

	LPCTSTR m_szMessage;
	LPCTSTR m_szTitle;
	INT_PTR m_iDefaultButton;

	BOOL m_fDontShow;
	BOOL m_fShowDontShow;

public:
	enum { IDD = IDD_CONFIRM };

	BEGIN_MSG_MAP_EX(CConfirmDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER_EX(IDYES,OnResponse)
		COMMAND_ID_HANDLER_EX(IDNO,OnResponse)
		COMMAND_ID_HANDLER_EX(IDCLOSE,OnResponse)
		COMMAND_ID_HANDLER_EX(IDCANCEL,OnResponse)
	END_MSG_MAP()

	CConfirmDlg();

	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);
	void OnResponse(UINT wNotifyCode, int wID, HWND hWndCtl);

	void SetMessage(LPCTSTR szMessage);
	void SetTitle(LPCTSTR szTitle);

	void ShowDontShowOption(BOOL fShow = TRUE);
	BOOL GetDontShow();
	void SetDontShow(BOOL fChecked);
	void SetDefaultButton(INT_PTR iDefault);
};

//int
//pShowMessageBox(
//	LPCTSTR szMessage, 
//	LPCTSTR szTitle = NULL,
//	HWND hWnd = ::GetActiveWindow(),
//	LPCTSTR szDontShowOptionValueName = NULL,
//	INT_PTR iDefaultButton = IDYES,
//	INT_PTR iDefaultResponse = IDYES);
//
//int
//pShowMessageBox(
//	UINT nMessageID,
//	UINT nTitleID,
//	HWND hWnd = ::GetActiveWindow(),
//	LPCTSTR szDontShowOptionValueName = NULL,
//	INT_PTR iDefaultButton = IDYES,
//	INT_PTR iDefaultResponse = IDYES);
//

int
ConfirmMessageBox(
	HWND hWndOwner,
	ATL::_U_STRINGorID Message, 
	ATL::_U_STRINGorID Title = (LPCTSTR)0,
	LPCTSTR szDontShowOptionValueName = NULL,
	int iDefaultButton = IDYES,
	int iDefaultResponse = IDYES);
