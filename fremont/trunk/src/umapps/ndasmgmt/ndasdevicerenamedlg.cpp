#include "stdafx.h"
#include "ndasmgmt.h"
#include "ndasdevicerenamedlg.h"

CNdasDeviceRenameDlg::CNdasDeviceRenameDlg()
{
}

LRESULT 
CNdasDeviceRenameDlg::OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
	m_wndName.Attach(GetDlgItem(IDC_DEVICE_NAME));
	m_wndOK.Attach(GetDlgItem(IDOK));

	m_wndName.SetLimitText(MAX_NAME_LEN - 1);
	m_wndName.SetWindowText(m_szName);
	m_wndName.SetFocus();
	m_wndName.SetSelAll();

	return 0;
}

void 
CNdasDeviceRenameDlg::SetName(LPCTSTR szName)
{
	HRESULT hr = ::StringCchCopyN(
		m_szName, 
		MAX_NAME_LEN + 1, 
		szName, 
		MAX_NAME_LEN);
	ATLASSERT(SUCCEEDED(hr));
	hr = ::StringCchCopyN(
		m_szOldName,
		MAX_NAME_LEN + 1,
		szName,
		MAX_NAME_LEN);
	ATLASSERT(SUCCEEDED(hr));
}

LPCTSTR 
CNdasDeviceRenameDlg::GetName()
{
	return m_szName;
}

LRESULT 
CNdasDeviceRenameDlg::OnClose(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CEdit wndName(GetDlgItem(IDC_DEVICE_NAME));
	wndName.GetWindowText(m_szName, MAX_NAME_LEN + 1);

	if (IDOK == wID && 0 != ::lstrcmp(m_szOldName, m_szName))
	{
		//
		// name is changed! check duplicate
		//
		ndas::DevicePtr pExistingDevice;
		if (ndas::FindDeviceByName(pExistingDevice, m_szName))
		{

			AtlTaskDialogEx(
				m_hWnd,
				IDS_MAIN_TITLE,
				0U,
				IDS_ERROR_DUPLICATE_NAME,
				TDCBF_OK_BUTTON,
				TD_ERROR_ICON);

			m_wndName.SetFocus();
			m_wndName.SetSel(0, -1);
			return 0;
		}
	}

	EndDialog(wID);
	return 0;
}

LRESULT
CNdasDeviceRenameDlg::OnDeviceNameChange(UINT uCode, int nCtrlID, HWND hwndCtrl)
{
	_ASSERTE(hwndCtrl == m_wndName.m_hWnd);

	INT iLen = m_wndName.GetWindowTextLength();
	if (iLen == 0) 
	{
		m_wndOK.EnableWindow(FALSE);
	}
	else 
	{
		m_wndOK.EnableWindow(TRUE);
	}
	return 0;
}

