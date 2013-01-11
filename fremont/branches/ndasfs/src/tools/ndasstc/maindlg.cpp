#include "stdatl.hpp"
#include "maindlg.h"
#include "deviceselectiondlg.h"
#include <ndas/ndascomm.h>
#include <ndas/ndasuser.h>
#include <ndas/ndasmsg.h>

LRESULT
CMainDlg::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	m_selectedDeviceText.Attach(GetDlgItem(IDC_SELECTED));
	m_selectButton.Attach(GetDlgItem(IDC_SELECT));
	m_currentSettingText.Attach(GetDlgItem(IDC_CURRENT));
	m_newStandbyAfterButton.Attach(GetDlgItem(IDC_NEW_STANDBY_AFTER));
	m_newStandbyAfterEdit.Attach(GetDlgItem(IDC_NEW_STANDBY_MIN));
	m_newStandbyAfterUpDown.Attach(GetDlgItem(IDC_NEW_STANDBY_MIN_SPIN));
	m_newStandbyOffButton.Attach(GetDlgItem(IDC_NEW_STANDBY_OFF));
	m_newStandbyChangeButton.Attach(GetDlgItem(IDC_CHANGE));

	EnableChangeButtons(FALSE);

	m_currentSettingText.SetWindowText(_T(""));

	ZeroMemory(&m_sdparam, sizeof(CDeviceSelectionDlg::DLG_PARAM));
	m_sdparam.Size = sizeof(CDeviceSelectionDlg::DLG_PARAM);
	m_sdparam.Type = CDeviceSelectionDlg::DLG_PARAM_UNSPECIFIED;

#if 0
	__try
	{
		NdasEnumDevices(OnNdasDeviceEnum, this);
	}
	__except(0xC06D007E == GetExceptionCode() ? 
		EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		ATLTRACE("ndasuser.dll is not available\n");
		// ATLVERIFY( m_ndasDeviceListWnd.ModifyStyle(0, CBS_DROPDOWN) );
	}
#endif

	return TRUE;
}

void
CMainDlg::OnCmdSelect(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/)
{
	CDeviceSelectionDlg selDlg;
	if (IDOK != selDlg.DoModal(m_hWnd, &m_sdparam))
	{
		return;
	}

	ATLTRACE(_T("Selected DeviceId=%s\n"), m_sdparam.NdasDeviceId);

	m_selectedDeviceText.SetWindowText(m_sdparam.NdasDeviceName);

	RefreshSettings();
}

void 
CMainDlg::OnCmdChange(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/)
{
	ChangeSettings();
	RefreshSettings();
}

void
CMainDlg::OnCmdAbout(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/)
{
	CSimpleDialog<IDD_ABOUT>().DoModal();
}

void 
CMainDlg::OnStandbyMinSetFocus(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hWndCtl*/)
{
	m_newStandbyAfterButton.SetCheck(BST_CHECKED);
	m_newStandbyOffButton.SetCheck(BST_UNCHECKED);
}

void 
CMainDlg::EnableChangeButtons(BOOL Enable)
{
	m_newStandbyAfterEdit.EnableWindow(Enable);
	m_newStandbyAfterButton.EnableWindow(Enable);
	m_newStandbyAfterEdit.EnableWindow(Enable);
	m_newStandbyAfterUpDown.EnableWindow(Enable);
	m_newStandbyOffButton.EnableWindow(Enable);
	m_newStandbyChangeButton.EnableWindow(Enable);
}

BOOL 
CMainDlg::RefreshSettings()
{
	NDASCOMM_CONNECTION_INFO ci = {0};
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL;
	ci.UnitNo = 0;
	ci.WriteAccess = FALSE;
	ci.Flags = 0;
	ci.Protocol = NDASCOMM_TRANSPORT_LPX;
	ci.AddressType = NDASCOMM_CIT_NDAS_ID;

	COMVERIFY( StringCchPrintf(
		ci.Address.NdasId.Id, NDAS_DEVICE_STRING_ID_LEN + 1,
		m_sdparam.NdasDeviceId) );

	m_currentSettingText.SetWindowText(_T(""));
	m_newStandbyAfterEdit.SetWindowText(_T(""));
	EnableChangeButtons(FALSE);

	ATLTRACE("Connecting to %ls\n", ci.Address.NdasId.Id);

	HNDAS ndasHandle = NdasCommConnect(&ci);

	if (NULL == ndasHandle)
	{
		DWORD errCode = GetLastError();
		ATLTRACE("NdasCommConnect failed, error=0x%X\n", errCode);

		CString message;
		message.Format(_T("Error 0x%X"), errCode);
		m_currentSettingText.SetWindowText(message);
		return FALSE;
	}

	NDASCOMM_VCMD_PARAM vcmdparam = {0};

	BOOL success = NdasCommVendorCommand(
		ndasHandle,
		ndascomm_vcmd_get_standby_timer,
		&vcmdparam,
		NULL, 0,
		NULL, 0);

	if (!success)
	{
		DWORD errcode = GetLastError();

		ATLTRACE("NdasCommVendorCommand failed, error=0x%X\n", errcode);

		if (NDASCOMM_ERROR_HARDWARE_UNSUPPORTED == errcode)
		{
			ATLTRACE("NDAS device does not support 'STANDBY' feature.\n", errcode);
			CString message = MAKEINTRESOURCE(IDS_STANDBY_NOT_SUPPORTED);
			m_currentSettingText.SetWindowText(message);
		}
		else
		{
			ATLTRACE("General error=0x%X\n", errcode);
			CString message = _T("Error");
			m_currentSettingText.SetWindowText(message);
		}
	}
	else
	{
		ATLTRACE("Enabled=%d, Value=%d min\n", 
			vcmdparam.GET_STANDBY_TIMER.EnableTimer,
			vcmdparam.GET_STANDBY_TIMER.TimeValue);

		if (vcmdparam.GET_STANDBY_TIMER.EnableTimer)
		{
			CString format = MAKEINTRESOURCE(IDS_STANDBY_AFTER_FMT);
			CString message;
			message.FormatMessage(format, vcmdparam.GET_STANDBY_TIMER.TimeValue);
			m_currentSettingText.SetWindowText(message);

			message.Format(_T("%u"), vcmdparam.GET_STANDBY_TIMER.TimeValue);
			m_newStandbyAfterEdit.SetWindowText(message);

			m_newStandbyAfterButton.Click();
		}
		else
		{
			CString message = MAKEINTRESOURCE(IDS_STANDBY_DISABLED);
			m_currentSettingText.SetWindowText(message);
			m_newStandbyOffButton.Click();
		}

		EnableChangeButtons(TRUE);
	}

	ATLVERIFY( NdasCommDisconnect(ndasHandle) );

	return TRUE;
}

const NDAS_OEM_CODE
NDAS_PRIVILEGED_OEM_CODE_DEFAULT = {
	0x1E, 0x13, 0x50, 0x47, 0x1A, 0x32, 0x2B, 0x3E };

BOOL 
pNdasCommDisconnectAfterReset(HNDAS NdasHandle)
{
	__try
	{
		ATLVERIFY( NdasCommDisconnectEx(NdasHandle, NDASCOMM_DF_DONT_LOGOUT) );
	}
	__except(0xC06D007E == GetExceptionCode() ? 
		EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		ATLTRACE("NdasCommDisconnectEx is not available\n");
		ATLVERIFY( NdasCommDisconnect(NdasHandle) );
		// ATLVERIFY( m_ndasDeviceListWnd.ModifyStyle(0, CBS_DROPDOWN) );
	}

	return TRUE;
}

BOOL 
CMainDlg::ChangeSettings()
{
	NDASCOMM_CONNECTION_INFO ci = {0};
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL;
	ci.UnitNo = 0;
	ci.WriteAccess = FALSE;
	ci.Protocol = NDASCOMM_TRANSPORT_LPX;
	ci.AddressType = NDASCOMM_CIT_NDAS_ID;
	/* Requires privileged access to change this setting */
	ci.PrivilegedOEMCode = NDAS_PRIVILEGED_OEM_CODE_DEFAULT;
	/* Privileged Connection cannot use lock commands */
	ci.Flags = NDASCOMM_CNF_DISABLE_LOCK_CLEANUP_ON_CONNECT;

	ATLTRACE("Connecting to %ls\n", ci.Address.NdasId.Id);

	COMVERIFY( StringCchPrintf(
		ci.Address.NdasId.Id, NDAS_DEVICE_STRING_ID_LEN + 1,
		m_sdparam.NdasDeviceId) );

	// m_currentSettingText.SetWindowText(_T(""));
	// m_newStandbyAfterEdit.SetWindowText(_T(""));

	HNDAS ndasHandle = NdasCommConnect(&ci);

	if (NULL == ndasHandle)
	{
		DWORD errCode = GetLastError();
		ATLTRACE("NdasCommConnect failed, error=0x%X\n", errCode);

		CString message;
		message.Format(_T("Error 0x%X"), errCode);
		AtlMessageBox(m_hWnd, static_cast<LPCTSTR>(message));
		return FALSE;
	}

	NDASCOMM_VCMD_PARAM vcmdparam = {0};

	if (BST_CHECKED == m_newStandbyOffButton.GetCheck())
	{
		vcmdparam.SET_STANDBY_TIMER.EnableTimer = 0;
		vcmdparam.SET_STANDBY_TIMER.TimeValue = 0;
	}
	else
	{
		TCHAR s[36] = {0};
		ATLVERIFY( m_newStandbyAfterEdit.GetWindowText(s, 36) );

		ATLTRACE("m_newStandbyAfterEdit: %ls\n", s);

		vcmdparam.SET_STANDBY_TIMER.EnableTimer = 1;
		vcmdparam.SET_STANDBY_TIMER.TimeValue = _ttoi(s);
		// TimeValue must not be zero (0) if enabled
		if (0 == vcmdparam.SET_STANDBY_TIMER.TimeValue)
		{
			vcmdparam.SET_STANDBY_TIMER.TimeValue = 1;
		}
	}

	ATLTRACE("SET_STANDBY_TIMER Enabled=%d, Value=%d min\n", 
		vcmdparam.GET_STANDBY_TIMER.EnableTimer,
		vcmdparam.GET_STANDBY_TIMER.TimeValue);

	BOOL success = NdasCommVendorCommand(
		ndasHandle,
		ndascomm_vcmd_set_standby_timer,
		&vcmdparam,
		NULL, 0,
		NULL, 0);

	if (!success)
	{
		DWORD errcode = GetLastError();

		ATLTRACE("NdasCommVendorCommand failed, error=0x%X\n", errcode);

		if (NDASCOMM_ERROR_HARDWARE_UNSUPPORTED == errcode)
		{
			ATLTRACE("NDAS device does not support 'STANDBY' feature.\n", errcode);
			CString message = MAKEINTRESOURCE(IDS_STANDBY_NOT_SUPPORTED);
			m_currentSettingText.SetWindowText(message);
		}
		else
		{
			ATLTRACE("General error=0x%X\n", errcode);
			CString message = _T("Error");
			m_currentSettingText.SetWindowText(message);
		}
	}
	else
	{
		if (vcmdparam.GET_STANDBY_TIMER.EnableTimer)
		{
			CString format = MAKEINTRESOURCE(IDS_STANDBY_AFTER_FMT);
			CString message;
			message.FormatMessage(format, vcmdparam.GET_STANDBY_TIMER.TimeValue);
			m_currentSettingText.SetWindowText(message);

			message.Format(_T("%u"), vcmdparam.GET_STANDBY_TIMER.TimeValue);
			m_newStandbyAfterEdit.SetWindowText(message);

			m_newStandbyAfterButton.Click();
		}
		else
		{
			CString message = MAKEINTRESOURCE(IDS_STANDBY_DISABLED);
			m_currentSettingText.SetWindowText(message);
			m_newStandbyOffButton.Click();
		}
	}

	ZeroMemory(&vcmdparam, sizeof(NDASCOMM_VCMD_PARAM));

	ATLTRACE("Resetting the NDAS device...\n");

	success = NdasCommVendorCommand(
		ndasHandle, 
		ndascomm_vcmd_reset, 
		&vcmdparam, 
		NULL, 0, 
		NULL, 0);

	ATLTRACE("VCMD_RESET returns %d\n", success);

	ATLVERIFY( pNdasCommDisconnectAfterReset(ndasHandle) );

	return TRUE;
}
