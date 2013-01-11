#pragma once

class CErrorDlg : 
	public CDialogImpl<CErrorDlg>
{
public:

	struct DialogParam 
	{
		DWORD ErrorCode;
		LPCTSTR Message;
	};

	enum { IDD = IDD_ERROR };

	BEGIN_MSG_MAP_EX(CErrorDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_RANGE_HANDLER_EX(IDOK, IDNO, OnCloseCmd)
	END_MSG_MAP()

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
	LRESULT OnCloseCmd(UINT nNotifyCode, int nID, HWND hWndCtl);

private:

	CEdit m_wndMessage;
	CStatic m_wndIcon;
};

inline
LRESULT 
CErrorDlg::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	CenterWindow();
	m_wndMessage.Attach(GetDlgItem(IDC_MESSAGE));
	m_wndIcon.Attach(GetDlgItem(IDC_ERROR_ICON));

	HICON hIcon = ::LoadIcon(NULL, IDI_ERROR);
	m_wndIcon.SetIcon(hIcon);

	DialogParam* param = reinterpret_cast<DialogParam*>(lParam);
	LPCTSTR lpMessage = param->Message;
	m_wndMessage.SetWindowText(lpMessage);

	CString strCode; 
	strCode.Format(_T("\r\nError Code: %d (0x%X)"), param->ErrorCode, param->ErrorCode);
	m_wndMessage.AppendText(strCode);

	return TRUE;
}

inline
LRESULT
CErrorDlg::OnCloseCmd(UINT, int nID, HWND)
{
	EndDialog(nID);
	return 0;
}
