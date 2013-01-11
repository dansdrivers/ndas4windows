#include "stdafx.h"
#include "resource.h"
#include "devregdlg.h"
#include "ndasmgmt.h"

CRegisterDeviceDlg::CRegisterDeviceDlg() :
	m_bWritableRegister(FALSE),
	m_bValidDeviceName(FALSE),
	m_bValidId(FALSE)
{
}

LRESULT 
CRegisterDeviceDlg::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	DWORD nDeviceCount = _pDeviceColl->GetDeviceCount();

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
	m_wndPasteChains[3].Attach(m_hWnd, m_wndStringIDs[i], m_wndStringKey);
	m_wndPasteChains[4].Attach(m_hWnd, m_wndStringKey, NULL);

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
	CString strNewDeviceName;

	for (DWORD i = 1; ; ++i)
	{
		strNewDeviceName.FormatMessage(IDS_NEW_DEVICE_NAME_TEMPLATE, nDeviceCount + i);
		ndas::Device* pExistingDevice = _pDeviceColl->FindDeviceByName(strNewDeviceName);
		if (NULL != pExistingDevice)
		{
			pExistingDevice->Release();
			continue;
		}
		break;
	}

	m_wndName.SetWindowText(strNewDeviceName);
	m_wndName.SetSelAll();
	m_wndName.SetFocus();

	//
	// Enable on register button
	//
	m_wndEnableOnRegister.Attach(GetDlgItem(IDC_ENABLE_DEVICE));
	m_wndEnableOnRegister.SetCheck(BST_CHECKED);

	//
	// Centering
	//
	CenterWindow();

	SetMsgHandled(FALSE);
	return 0;
}

BOOL
CRegisterDeviceDlg::IsValidDeviceStringIdKey()
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

	CString strDeviceID;
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

BOOL
CRegisterDeviceDlg::UpdateRegisterButton()
{
	if (m_bValidDeviceName && m_bValidId) {
		m_wndRegister.EnableWindow(TRUE);
		return TRUE;
	} else {
		m_wndRegister.EnableWindow(FALSE);
		return FALSE;
	}
}

LRESULT 
CRegisterDeviceDlg::OnDeviceNameChange(UINT uCode, int nCtrlID, HWND hwndCtrl)
{
	if (m_wndName.GetWindowTextLength() > 0) {
		m_bValidDeviceName = TRUE;
	} else {
		m_bValidDeviceName = FALSE;
	}
	UpdateRegisterButton();
	return 0;
}

LRESULT 
CRegisterDeviceDlg::OnDeviceIdChange(UINT uCode, int nCtrlID, HWND hwndCtrl)
{
	CEdit wndCurrentEdit(hwndCtrl);

	BOOL bNextDlgCtrl(FALSE);

	if (m_wndStringIDs[0].GetWindowTextLength() == 5 &&
		m_wndStringIDs[1].GetWindowTextLength() == 5 &&
		m_wndStringIDs[2].GetWindowTextLength() == 5 &&
		m_wndStringIDs[3].GetWindowTextLength() == 5 &&
		(m_wndStringKey.GetWindowTextLength() == 0 || 
		m_wndStringKey.GetWindowTextLength() == 5))
	{

		BOOL bValid = IsValidDeviceStringIdKey();

		if (!bValid) {

			CString strText, strTitle;
			strTitle.LoadString(IDS_INVALID_DEVICE_ID_TOOLTIP_TITLE);
			strText.LoadString(IDS_INVALID_DEVICE_ID_TOOLTIP_TEXT);
			EDITBALLOONTIP tip;
			tip.cbStruct = sizeof(tip);
			tip.pszTitle = strTitle;
			tip.pszText = strText;
			tip.ttiIcon = TTI_WARNING;
			wndCurrentEdit.ShowBalloonTip(&tip);

			m_bValidId = FALSE;
			bNextDlgCtrl = FALSE;

		} else {

			m_bValidId = TRUE;
			bNextDlgCtrl = TRUE;

		}

	} else {
		if (wndCurrentEdit.GetWindowTextLength() >= 5) {
			bNextDlgCtrl = TRUE;
		}
		m_bValidId = FALSE;
	}

	UpdateRegisterButton();

	if (bNextDlgCtrl) NextDlgCtrl();

	return 0;
}

void 
CRegisterDeviceDlg::OnChar_DeviceId(TCHAR ch, UINT nRepCnt, UINT nFlags)
{
	if (((TEXT('a') <= ch) && (ch <= TEXT('z') && ch != _T('o') && ch != _T('i'))) ||
		((TEXT('A') <= ch) && (ch <= TEXT('Z') && ch != _T('O') && ch != _T('I'))) ||
		((TEXT('0') <= ch) && (ch <= TEXT('9'))))
	{
		SetMsgHandled(FALSE);
		return;
	}

	::MessageBeep(0xFFFFFFFF);
	SetMsgHandled(TRUE);
}

LRESULT 
CRegisterDeviceDlg::OnRegister(WORD, WORD wID, HWND, BOOL&)
{
	DoDataExchange(TRUE);

	LPCTSTR lpszDeviceName = GetDeviceName();
	LPCTSTR lpszDeviceStringId = GetDeviceStringId();
	LPCTSTR lpszDeviceStringKey = GetDeviceStringKey();
	BOOL fEnableOnReg = GetEnableOnRegister();

	ndas::Device* pExistingDevice = 
		_pDeviceColl->FindDeviceByName(lpszDeviceName);
	if (NULL != pExistingDevice)
	{
		pExistingDevice->Release();
		AtlMessageBox(
			m_hWnd, 
			IDS_ERROR_DUPLICATE_NAME, 
			IDS_MAIN_TITLE, 
			MB_OK | MB_ICONWARNING);

		m_wndName.SetFocus();
		m_wndName.SetSel(0, -1);
		// Registration failure will not close dialog
		return 0;
	}

	DWORD dwSlotNo = ::NdasRegisterDevice(
		lpszDeviceStringId, lpszDeviceStringKey, lpszDeviceName);

	// Registration failure will not close dialog
	if (0 == dwSlotNo) {
		ShowErrorMessageBox(IDS_ERROR_REGISTER_DEVICE_FAILURE);
		return 0;
	}

	if (!fEnableOnReg) {
		EndDialog(wID);
		return 0;
	}

	//
	// Enable on register is an optional feature
	// Even if it's failed, still go on to close.
	//
	BOOL fSuccess = _pDeviceColl->Update();
	if (!fSuccess) {
		DWORD err = ::GetLastError();
		ATLTRACE(_T("Enabling device at slot %d failed: "), dwSlotNo);
		::SetLastError(err);
		EndDialog(wID);
		return 0;
	}

	ndas::Device* pDevice = _pDeviceColl->FindDevice(dwSlotNo);
	if (NULL == pDevice) {
		DWORD err = ::GetLastError();
		ATLTRACE(_T("Enabling device at slot %d failed: "), dwSlotNo);
		::SetLastError(err);
		EndDialog(wID);
		return 0;
	}

	fSuccess = pDevice->Enable();
	if (!fSuccess) {
		DWORD err = ::GetLastError();
		ATLTRACE(_T("Enabling device at slot %d failed: "), dwSlotNo);
		::SetLastError(err);
		EndDialog(wID);
		return 0;
	}

	EndDialog(wID);
	return 0;
}

LRESULT 
CRegisterDeviceDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(wID);
	return 0;
}

LPCTSTR 
CRegisterDeviceDlg::GetDeviceStringId()
{
	m_strDeviceId.Format(TEXT("%s%s%s%s"), 
		m_strDeviceIDs[0], m_strDeviceIDs[1],
		m_strDeviceIDs[2], m_strDeviceIDs[3]);
	return m_strDeviceId;
}

LPCTSTR 
CRegisterDeviceDlg::GetDeviceStringKey()
{
	LPCTSTR lpszDeviceStringKey = (m_strDeviceKey.GetLength() == 0) ? 
		NULL : (LPCTSTR) m_strDeviceKey;
	return lpszDeviceStringKey;
}

LPCTSTR
CRegisterDeviceDlg::GetDeviceName()
{
	return m_strDeviceName;
}

BOOL CRegisterDeviceDlg::GetEnableOnRegister()
{
	if (BST_CHECKED == m_iEnableDevice) {
		return TRUE;
	}
	return FALSE;
}
