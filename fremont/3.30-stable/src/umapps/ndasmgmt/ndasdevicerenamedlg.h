#pragma once

class CNdasDeviceRenameDlg :
	public CDialogImpl<CNdasDeviceRenameDlg>
{

public:
	enum { IDD = IDD_RENAME };

	BEGIN_MSG_MAP_EX(CNdasDeviceRenameDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnClose)
		COMMAND_ID_HANDLER(IDCANCEL, OnClose)
		COMMAND_HANDLER_EX(IDC_DEVICE_NAME, EN_CHANGE, OnDeviceNameChange)
	END_MSG_MAP()

	CNdasDeviceRenameDlg();

	void SetName(LPCTSTR szName);
	LPCTSTR GetName();

	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);
	LRESULT OnClose(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDeviceNameChange(UINT uCode, int nCtrlID, HWND hwndCtrl);

private:
	static const MAX_NAME_LEN = MAX_NDAS_DEVICE_NAME_LEN;
	TCHAR m_szName[MAX_NAME_LEN + 1];
	TCHAR m_szOldName[MAX_NAME_LEN + 1];
	CEdit m_wndName;
	CButton m_wndOK;
};
