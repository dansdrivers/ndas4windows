#pragma once

class CNdasDeviceAddWriteKeyDlg :
	public CDialogImpl<CNdasDeviceAddWriteKeyDlg>
{
	CString m_strWriteKey;
	CString m_strDeviceName;
	CString m_strDeviceId;

	CButton m_butOK;

public:
	enum { IDD = IDD_ADD_WRITE_KEY };

	BEGIN_MSG_MAP_EX(CNdasDeviceAddWriteKeyDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER_EX(IDC_DEVICE_WRITE_KEY,EN_CHANGE,OnWriteKeyChange)
		COMMAND_ID_HANDLER_EX(IDOK, OnOK)
		COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
	END_MSG_MAP()

	void SetDeviceName(LPCTSTR szName);
	void SetDeviceId(LPCTSTR szDeviceId);
	LPCTSTR GetWriteKey();

	// Message Handlers
	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);
	void OnWriteKeyChange(UINT, int, HWND);
	void OnOK(UINT /*wNotifyCode*/, int /* wID */, HWND /*hWndCtl*/);
	void OnCancel(UINT /*wNotifyCode*/, int /* wID */, HWND /*hWndCtl*/);
};
