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

	CConfirmDlg() : 
		m_fShowDontShow(TRUE), 
		m_fDontShow(FALSE),
		m_iDefaultButton(IDYES),
		m_szMessage(NULL),
		m_szTitle(NULL)
	{}

	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);
	VOID OnResponse(UINT wNotifyCode, int wID, HWND hWndCtl);

	VOID SetMessage(LPCTSTR szMessage);
	VOID SetTitle(LPCTSTR szTitle);

	VOID ShowDontShowOption(BOOL fShow = TRUE);
	BOOL GetDontShow();
	VOID SetDontShow(BOOL fChecked);
	VOID SetDefaultButton(INT_PTR iDefault);
};

INT_PTR 
pShowMessageBox(
	LPCTSTR szMessage, 
	LPCTSTR szTitle = NULL,
	HWND hWnd = ::GetActiveWindow(),
	LPCTSTR szDontShowOptionValueName = NULL,
	INT_PTR iDefaultButton = IDYES,
	INT_PTR iDefaultResponse = IDYES);

INT_PTR 
pShowMessageBox(
	UINT nMessageID,
	UINT nTitleID,
	HWND hWnd = ::GetActiveWindow(),
	LPCTSTR szDontShowOptionValueName = NULL,
	INT_PTR iDefaultButton = IDYES,
	INT_PTR iDefaultResponse = IDYES);


