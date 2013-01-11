#include "stdafx.h"
#include "confirmdlg.h"
#include "appconf.h"

LRESULT 
CConfirmDlg::OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
	m_wndDontShowAgain.Attach(GetDlgItem(IDC_DONT_SHOW_AGAIN));
	m_wndMessage.Attach(GetDlgItem(IDC_CONFIRM_MESSAGE));
	m_wndIcon.Attach(GetDlgItem(IDC_CONFIRM_ICON));
	m_wndHidden.Attach(GetDlgItem(IDC_HIDDEN));

	HICON hIcon = ::LoadIcon(NULL,IDI_QUESTION);
	m_wndIcon.SetIcon(hIcon);

	m_wndMessage.SetWindowText(m_szMessage);
	SetWindowText(m_szTitle);

	if (IDYES == m_iDefaultButton || IDNO == m_iDefaultButton) {
		CButton wnd = GetDlgItem(m_iDefaultButton);
		wnd.SetButtonStyle(BS_DEFPUSHBUTTON);
	}

	m_wndHidden.ShowWindow(SW_HIDE);

	if (!m_fShowDontShow) {
		CRect rect, rectDlg;
		m_wndHidden.GetWindowRect(rect);
		GetWindowRect(rectDlg);
		MoveWindow(rectDlg.left,rectDlg.top,
			rectDlg.Width(),
			rectDlg.Height() - rect.Height());
	}

	if (m_fDontShow) {
		INT iCheck = m_fDontShow ? BST_CHECKED : BST_UNCHECKED;
		m_wndDontShowAgain.SetCheck(iCheck);
	}

	CenterWindow();

	return 1;
}


BOOL
CConfirmDlg::GetDontShow()
{
	return m_fDontShow;
}

VOID
CConfirmDlg::SetDontShow(BOOL fChecked)
{
	m_fDontShow = fChecked;
}

VOID 
CConfirmDlg::OnResponse(UINT wNotifyCode, int wID, HWND hWndCtl)
{
	m_fDontShow = (BST_CHECKED == m_wndDontShowAgain.GetCheck());
	EndDialog(wID);
}

VOID
CConfirmDlg::SetDefaultButton(INT_PTR iDefault)
{
	m_iDefaultButton = iDefault;
}

VOID 
CConfirmDlg::SetMessage(LPCTSTR szMessage)
{
	m_szMessage = szMessage;
}

VOID 
CConfirmDlg::SetTitle(LPCTSTR szTitle)
{
	m_szTitle = szTitle;
}

VOID
CConfirmDlg::ShowDontShowOption(BOOL fShow /* = TRUE */)
{
	m_fShowDontShow = fShow;
}

INT_PTR
pShowMessageBox(
	LPCTSTR szMessage, 
	LPCTSTR szTitle,
	HWND hWnd,
	LPCTSTR szDontShowOptionValueName,
	INT_PTR iDefaultButton,
	INT_PTR iDefaultResponse)
{
	CConfirmDlg dlg;

	BOOL fDontShow = FALSE;
	BOOL fSuccess = pGetAppConfigValue(szDontShowOptionValueName,&fDontShow);
	if (fSuccess && fDontShow) {
		return iDefaultResponse;
	}

	dlg.SetMessage(szMessage);
	dlg.SetTitle(szTitle);
	dlg.SetDefaultButton(iDefaultButton);

	INT_PTR iResult = dlg.DoModal();
	fDontShow = dlg.GetDontShow();
	if (fDontShow) {
		fSuccess = pSetAppConfigValue(szDontShowOptionValueName,(BOOL)TRUE);
	}

	return iResult;
}

INT_PTR
pShowMessageBox(
	UINT nMessageID,
	UINT nTitleID,
	HWND hWnd,
	LPCTSTR szDontShowOptionValueName,
	INT_PTR iDefaultButton,
	INT_PTR iDefaultResponse)
{
	CString strMessage, strTitle;
	strMessage.LoadString(nMessageID);
	strTitle.LoadString(nTitleID);

	return pShowMessageBox(
		strMessage,
		strTitle,
		hWnd,
		szDontShowOptionValueName,
		iDefaultButton,
		iDefaultResponse);
}
