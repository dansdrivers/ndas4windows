#include "stdafx.h"
#include "ndasmgmt.h"
#include "ndastypestr.h"
#include "ndasdeviceaddwritekeydlg.h"

LRESULT 
CNdasDeviceAddWriteKeyDlg::OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
	CWindow wndDeviceName = GetDlgItem(IDC_DEVICE_NAME);
	CWindow wndDeviceId = GetDlgItem(IDC_DEVICE_ID);
	CEdit wndWriteKey = GetDlgItem(IDC_DEVICE_WRITE_KEY);
	
	TCHAR chPassword = _T('*');

	// Temporary edit control to get an effective password character
	{
		CEdit wndPassword;
		wndPassword.Create(m_hWnd, NULL, NULL, WS_CHILD | ES_PASSWORD);
		chPassword = wndPassword.GetPasswordChar();
		wndPassword.DestroyWindow();
	}

	CString strFmtDeviceId;
	pDelimitedDeviceIdString(strFmtDeviceId, m_strDeviceId, chPassword);

	wndDeviceName.SetWindowText(m_strDeviceName);
	wndDeviceId.SetWindowText(strFmtDeviceId);
	wndWriteKey.SetLimitText(NDAS_DEVICE_WRITE_KEY_LEN);

	m_butOK.Attach(GetDlgItem(IDOK));
	m_butOK.EnableWindow(FALSE);

	return TRUE;
}

void
CNdasDeviceAddWriteKeyDlg::SetDeviceName(LPCTSTR szDeviceName)
{
	m_strDeviceName = szDeviceName;
}

void
CNdasDeviceAddWriteKeyDlg::SetDeviceId(LPCTSTR szDeviceId)
{
	m_strDeviceId = szDeviceId;
}

LPCTSTR
CNdasDeviceAddWriteKeyDlg::GetWriteKey()
{
	return m_strWriteKey;
}

void 
CNdasDeviceAddWriteKeyDlg::OnOK(UINT /*wNotifyCode*/, int /* wID */, HWND /*hWndCtl*/)
{
	EndDialog(IDOK);
}

void 
CNdasDeviceAddWriteKeyDlg::OnCancel(UINT /*wNotifyCode*/, int /* wID */, HWND /*hWndCtl*/)
{
	EndDialog(IDCANCEL);
}

void
CNdasDeviceAddWriteKeyDlg::OnWriteKeyChange(UINT /*wNotifyCode*/, int /*wID*/, HWND hWndCtl)
{
	CEdit wndWriteKey = hWndCtl;

	wndWriteKey.GetWindowText(
		m_strWriteKey.GetBuffer(NDAS_DEVICE_WRITE_KEY_LEN + 1),
		NDAS_DEVICE_WRITE_KEY_LEN + 1);

	m_strWriteKey.ReleaseBuffer();

	CButton wndOK = GetDlgItem(IDOK);
	if (m_strWriteKey.GetLength() == 5) 
	{
		BOOL fValid = ::NdasValidateStringIdKey(
			m_strDeviceId, 
			m_strWriteKey);
		wndOK.EnableWindow(fValid);
	}
	else 
	{
		wndOK.EnableWindow(FALSE);
	}

	SetMsgHandled(FALSE);
}
