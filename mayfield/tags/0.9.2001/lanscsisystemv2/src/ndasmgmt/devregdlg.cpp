#include "stdafx.h"
#include "resource.h"
#include "devregdlg.h"
#include "ndasmgmt.h"

CRegisterDeviceDialog::
CRegisterDeviceDialog() :
	m_bWritableRegister(FALSE)
{
}

LRESULT 
CRegisterDeviceDialog::
OnInitDialog(HWND hWnd, LPARAM lParam)
{
	DWORD nDeviceCount = static_cast<DWORD>(lParam);

	//
	// Attach controls
	//
	m_wndStringIDs[0] = CEdit(GetDlgItem(IDC_DEV_ID_1));
	m_wndStringIDs[1] = CEdit(GetDlgItem(IDC_DEV_ID_2));
	m_wndStringIDs[2] = CEdit(GetDlgItem(IDC_DEV_ID_3));
	m_wndStringIDs[3] = CEdit(GetDlgItem(IDC_DEV_ID_4));

	m_wndStringKey = GetDlgItem(IDC_DEV_KEY);
	m_wndName = GetDlgItem(IDC_DEV_NAME);
	m_wndRegister = GetDlgItem(IDC_REGISTER);

	//
	// chaining paste support
	//
	DWORD i(0);
	for (; i < 3; ++i) {
		m_wndPasteChains[i].Attach(m_hWnd, m_wndStringIDs[i], m_wndStringIDs[i+1]);
	}
	m_wndPasteChains[i].Attach(m_hWnd, m_wndStringIDs[i], NULL);

	//
	// limit the maximum text
	//
	m_wndStringIDs[0].SetLimitText(5);
	m_wndStringIDs[1].SetLimitText(5);
	m_wndStringIDs[2].SetLimitText(5);
	m_wndStringIDs[3].SetLimitText(5);
	m_wndStringKey.SetLimitText(5);
	m_wndName.SetLimitText(32);

	//
	// default device name
	//
	WTL::CString strNewDeviceName;

	strNewDeviceName.Format(IDS_NEW_DEVICE_NAME_TEMPLATE, nDeviceCount + 1);
	m_wndName.SetWindowText(strNewDeviceName);
	m_wndName.SetSelAll();
	m_wndName.SetFocus();

	//
	// Centering
	//
	CenterWindow();

	SetMsgHandled(FALSE);
	return 0;
}

BOOL
CRegisterDeviceDialog::IsValidDeviceStringIdKey()
{
	// Transfer data from the controls to member variables
	DoDataExchange(TRUE);

	if (!(m_strDeviceIDs[0].GetLength() == 5 &&
		m_strDeviceIDs[1].GetLength() == 5 &&
		m_strDeviceIDs[2].GetLength() == 5 &&
		m_strDeviceIDs[3].GetLength() == 5 &&
		(m_strDeviceKey.GetLength() == 0 || m_strDeviceKey.GetLength() == 5)))
	{
		return FALSE;
	}

	WTL::CString strDeviceID;
	strDeviceID.Format(TEXT("%s%s%s%s"), 
		m_strDeviceIDs[0], m_strDeviceIDs[1], 
		m_strDeviceIDs[2], m_strDeviceIDs[3]);

	strDeviceID.MakeUpper();

	if (m_strDeviceKey.GetLength() > 0) {
		m_strDeviceKey.MakeUpper();
		return ::NdasValidateStringIdKeyW(strDeviceID, m_strDeviceKey);
	} else {
		return ::NdasValidateStringIdKeyW(strDeviceID, NULL);
	}
}

LRESULT 
CRegisterDeviceDialog::
OnDeviceIdChange(UINT uCode, int nCtrlID, HWND hwndCtrl)
{
	CEdit wndCurrentEdit(hwndCtrl);

	BOOL bEnableRegister(FALSE);
	BOOL bNextDlgCtrl(FALSE);

	if (m_wndStringIDs[0].GetWindowTextLength() == 5 &&
		m_wndStringIDs[1].GetWindowTextLength() == 5 &&
		m_wndStringIDs[2].GetWindowTextLength() == 5 &&
		m_wndStringIDs[3].GetWindowTextLength() == 5 &&
		(m_wndStringKey.GetWindowTextLength() == 0 || m_wndStringKey.GetWindowTextLength() == 5))
	{

		BOOL bValid = IsValidDeviceStringIdKey();

		if (!bValid) {

			EDITBALLOONTIP tip;
			tip.cbStruct = sizeof(tip);
			tip.pszText = TEXT("Device ID is composed of alphabet characters and numbers only!\nIt's 20 characters.\n");
			tip.pszTitle = TEXT("Invalid device ID?");
			tip.ttiIcon = TTI_WARNING;
			wndCurrentEdit.ShowBalloonTip(&tip);

			bEnableRegister = FALSE;
			bNextDlgCtrl = FALSE;

		} else {

			bEnableRegister = TRUE;
			bNextDlgCtrl = TRUE;

		}

	} else {
		if (wndCurrentEdit.GetWindowTextLength() >= 5) {
			bNextDlgCtrl = TRUE;
		}
	}
	
	m_wndRegister.EnableWindow(bEnableRegister);
	if (bNextDlgCtrl) NextDlgCtrl();

	return 0;
}

void 
CRegisterDeviceDialog::
OnChar_DeviceId(TCHAR ch, UINT nRepCnt, UINT nFlags)
{
	if (((TEXT('a') <= ch) && (ch <= TEXT('z'))) ||
		((TEXT('A') <= ch) && (ch <= TEXT('Z'))) ||
		((TEXT('0') <= ch) && (ch <= TEXT('9'))))
	{
		SetMsgHandled(FALSE);
		return;
	}

	::MessageBeep(0xFFFFFFFF);
	SetMsgHandled(TRUE);
}

LRESULT 
CRegisterDeviceDialog::
OnRegister(WORD, WORD wID, HWND, BOOL&)
{
	DoDataExchange(TRUE);
	EndDialog(wID);
	return 0;
}

LRESULT 
CRegisterDeviceDialog::
OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(wID);
	return 0;
}

LPCTSTR 
CRegisterDeviceDialog::
GetDeviceStringId()
{
	m_strDeviceId.Format(TEXT("%s%s%s%s"), 
		m_strDeviceIDs[0], m_strDeviceIDs[1],
		m_strDeviceIDs[2], m_strDeviceIDs[3]);
	return m_strDeviceId;
}

LPCTSTR 
CRegisterDeviceDialog::
GetDeviceStringKey()
{
	LPCTSTR lpszDeviceStringKey = (m_strDeviceKey.GetLength() == 0) ? 
		NULL : (LPCTSTR) m_strDeviceKey;
	return lpszDeviceStringKey;
}

LPCTSTR
CRegisterDeviceDialog::
GetDeviceName()
{
	return m_strDeviceName;
}
