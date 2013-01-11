#include "stdafx.h"
#include <ndas/ndasnif.h>
#include "exportdlg.h"
#include "apperrdlg.h"
namespace
{

struct AddToListBox : std::unary_function<ndas::DevicePtr,void> {
	AddToListBox(CComboBox& wndComboBox, const ndas::DevicePtr& pSelected) : 
		m_pSelected(pSelected), m_wndComboBox(wndComboBox) {}
	void operator()(const ndas::DevicePtr& pDevice) {
		int n = m_wndComboBox.AddString(pDevice->GetName());
		m_wndComboBox.SetItemDataPtr(n, (void*)&pDevice);
		if (pDevice == m_pSelected)
		{
			m_wndComboBox.SetCurSel(n);
		}
	}
private:
	const ndas::DevicePtr& m_pSelected;
	CComboBox m_wndComboBox;
};

} // anonymous namespace

LRESULT 
CExportDlg::OnInitDialog(HWND hWndCtl, LPARAM lParam)
{
	m_pDevice = *reinterpret_cast<ndas::DevicePtr*>(lParam);
	m_wndNameList.Attach(GetDlgItem(IDC_DEVICE_NAME_LIST));
	m_wndDeviceId[0].Attach(GetDlgItem(IDC_DEVICEID_1));
	m_wndDeviceId[1].Attach(GetDlgItem(IDC_DEVICEID_2));
	m_wndDeviceId[2].Attach(GetDlgItem(IDC_DEVICEID_3));
	m_wndDeviceId[3].Attach(GetDlgItem(IDC_DEVICEID_4));
	m_wndWriteKey.Attach(GetDlgItem(IDC_WRITE_KEY));
	m_wndNoWriteKey.Attach(GetDlgItem(IDC_NO_WRITE_KEY));
	m_wndDescription.Attach(GetDlgItem(IDC_DESCRIPTION));
	m_wndSave.Attach(GetDlgItem(IDOK));

	m_wndSave.EnableWindow(FALSE);

	m_wndDeviceId[3].SetLimitText(5);
	m_wndWriteKey.SetLimitText(5);
	m_wndDescription.SetLimitText(255);

	m_devices = ndas::GetDevices();

	std::for_each(
		m_devices.begin(),m_devices.end(),
		AddToListBox(m_wndNameList, m_pDevice));

	_ResetSelected();

	m_wndDeviceId[3].SetFocus();

	return FALSE;
}

void
CExportDlg::_ResetSelected()
{
	CString strDeviceId = m_pDevice->GetStringId();
	m_wndDeviceId[0].SetWindowText(strDeviceId.Mid(0,5));
	m_wndDeviceId[1].SetWindowText(strDeviceId.Mid(5,5));
	m_wndDeviceId[2].SetWindowText(strDeviceId.Mid(10,5));

	if (m_pDevice->GetGrantedAccess() & GENERIC_WRITE)
	{
		m_wndWriteKey.EnableWindow(TRUE);
		m_wndNoWriteKey.SetCheck(BST_UNCHECKED);
	}
	else
	{
		m_wndWriteKey.EnableWindow(FALSE);
		m_wndNoWriteKey.SetCheck(BST_CHECKED);
	}
}

BOOL 
pExportLoadFilters(LPTSTR Filters, DWORD MaxChars)
{
	LPTSTR lpNextFilter = &Filters[0];
	size_t remaining = MaxChars;

	const UINT nFilterIDs[] = {
		IDS_NIF_FILTER, IDS_NIF_FILTER_EXT,
		IDS_NIF_FILTER_2, IDS_NIF_FILTER_2_EXT,
		IDS_NIF_FILTER_ALL, IDS_NIF_FILTER_ALL_EXT};

	CString strFilter;
	for (DWORD i = 0; i < RTL_NUMBER_OF(nFilterIDs); ++i)
	{
		ATLVERIFY(strFilter.LoadString(nFilterIDs[i]));
		HRESULT hr = ::StringCchCopyEx(
			lpNextFilter, remaining, 
			static_cast<LPCTSTR>(strFilter), 
			&lpNextFilter, &remaining, 0);
		ATLASSERT(SUCCEEDED(hr));
		if (FAILED(hr))
		{
			return FALSE;
		}
		--remaining;
		++lpNextFilter;
		ATLASSERT(lpNextFilter < &Filters[MaxChars]);
	}

	return TRUE;
}

BOOL 
pExportGetSaveFileName(
	HWND hWndOwner,
	LPTSTR FilePath, 
	DWORD MaxChars)
{
	// TCHAR FilePath[MAX_PATH + 1] = {0};
	TCHAR Filters[MAX_PATH + 1] = {0};

	pExportLoadFilters(Filters, MAX_PATH);

	OPENFILENAME ofn = {0};
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hWndOwner;
	ofn.hInstance;
	ofn.lpstrFilter = Filters;
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = FilePath;
	ofn.nMaxFile = MaxChars;
	ofn.lpstrFileTitle;
	ofn.nMaxFileTitle;
	ofn.lpstrInitialDir = NULL;
	ofn.lpstrTitle = NULL;
	ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = _T("ndas");

	BOOL fSuccess = ::GetSaveFileName(&ofn);

	if (!fSuccess)
	{
		DWORD cderr = ::CommDlgExtendedError();
		ATLTRACE(_T("CDErr=%x\n"), cderr);
		::SetLastError(cderr);
		return FALSE;
	}

	return TRUE;
}

void 
CExportDlg::OnCmdSave(UINT, int, HWND)
{
	CString strName;
	m_wndNameList.GetLBText(m_wndNameList.GetCurSel(), strName);

	TCHAR FilePath[MAX_PATH];
	ATLVERIFY(SUCCEEDED(::StringCchCopy(FilePath, MAX_PATH, strName)));
	pNormalizePath(FilePath, MAX_PATH);

	if (!pExportGetSaveFileName(m_hWnd, FilePath, MAX_PATH))
	{
		if (ERROR_SUCCESS != ::GetLastError())
		{
			ErrorMessageBox(m_hWnd, IDS_ERROR_EXPORT);
		}
		return;
	}

	NDAS_NIF_V1_ENTRY nif = {0};

	nif.Flags; /* reserved */

	CString strDescription;
	m_wndDescription.GetWindowText(strDescription.GetBuffer(256), 256);

	nif.Name = const_cast<LPTSTR>(static_cast<LPCTSTR>(strName));
	nif.DeviceId = const_cast<LPTSTR>(_GetDeviceId());
	nif.WriteKey = const_cast<LPTSTR>(_GetWriteKey());
	nif.Description = const_cast<LPTSTR>(static_cast<LPCTSTR>(strDescription));
	
	HRESULT hr = ::NdasNifExport(FilePath, 1, &nif);

	if (FAILED(hr))
	{
		ErrorMessageBox(
			m_hWnd,
			IDS_ERROR_EXPORT,
			IDS_ERROR_TITLE,
			ndasmgmt::CurrentUILangID,
			hr);
	}
	else
	{
		EndDialog(IDOK);
	}
}

void 
CExportDlg::OnCmdClose(UINT nID, int, HWND)
{
	EndDialog(nID);
}

void 
CExportDlg::OnSelChange(UINT, int, HWND)
{
	int n = m_wndNameList.GetCurSel();
	m_pDevice = *reinterpret_cast<ndas::DevicePtr*>(
		m_wndNameList.GetItemDataPtr(n));
	_ResetSelected();
	_UpdateStates();
}

void 
CExportDlg::OnNoWriteKeyClick(UINT, int, HWND)
{
	int check = m_wndNoWriteKey.GetCheck();
	m_wndWriteKey.EnableWindow(!(BST_CHECKED == check));
	_UpdateStates();
}

void 
CExportDlg::OnDeviceIdChange(UINT, int, HWND)
{
	SetMsgHandled(FALSE);
	_UpdateStates();
	return;
}

void 
CExportDlg::_UpdateStates()
{
	LPCTSTR lpszDeviceId = NULL;
	LPCTSTR lpszWriteKey = NULL;
	bool NoWriteKey = (BST_CHECKED == m_wndNoWriteKey.GetCheck());
	BOOL IsValid = FALSE;
	if (!NoWriteKey)
	{
		if (m_wndWriteKey.GetWindowTextLength() != 5)
		{
			m_wndSave.EnableWindow(IsValid);
			return;
		}
		else
		{
			lpszWriteKey = _GetWriteKey();
		}
	}
	if (m_wndDeviceId[3].GetWindowTextLength() != 5)
	{
		m_wndSave.EnableWindow(IsValid);
		return;
	}

	lpszDeviceId = _GetDeviceId();

	IsValid = ::NdasValidateStringIdKey(lpszDeviceId, lpszWriteKey);

	m_wndSave.EnableWindow(IsValid);
}

LPCTSTR 
CExportDlg::_GetDeviceId()
{
	::ZeroMemory(m_szDeviceId, RTL_NUMBER_OF(m_szDeviceId));
	m_wndDeviceId[0].GetWindowText(&m_szDeviceId[0], 6);
	m_wndDeviceId[1].GetWindowText(&m_szDeviceId[5], 6);
	m_wndDeviceId[2].GetWindowText(&m_szDeviceId[10], 6);
	m_wndDeviceId[3].GetWindowText(&m_szDeviceId[15], 6);
	return m_szDeviceId;
}

LPCTSTR 
CExportDlg::_GetWriteKey()
{
	bool NoWriteKey = (BST_CHECKED == m_wndNoWriteKey.GetCheck());
	if (NoWriteKey)
	{
		return NULL;
	}
	::ZeroMemory(m_szWriteKey, RTL_NUMBER_OF(m_szWriteKey));
	m_wndWriteKey.GetWindowText(m_szWriteKey, 6);
	return m_szWriteKey;
}

void 
pNormalizePath(
	LPTSTR FilePath, 
	DWORD MaxChars,
	TCHAR Substitute)
{
	const TCHAR INVALID_CHARS[] = _T("<>:\"/\\|");
	const LPCTSTR INVALID_NAMES[] = {
		_T("CON"), _T("PRN"), _T("AUX"), _T("NUL"),
		_T("COM1"), _T("COM2"), _T("COM3"), _T("COM4"), _T("COM5"), 
		_T("COM6"), _T("COM7"), _T("COM8"), _T("COM9"), 
		_T("LPT1"), _T("LPT2"), _T("LPT3"), _T("LPT4"), _T("LPT5"),
		_T("LPT6"), _T("LPT7"), _T("LPT8"), _T("LPT9"),
		_T("CLOCK$"), _T("..")
	};

	for (DWORD i = 0; i < RTL_NUMBER_OF(INVALID_NAMES); ++i)
	{
		if (0 == ::lstrcmpi(INVALID_NAMES[i], FilePath))
		{
			FilePath[0] = 0;
			return;
		}
	}
	for (DWORD i = 0; i < MaxChars && FilePath[i] != 0; ++i)
	{
		if (FilePath[i] >= 0 && FilePath[i] <= 31)
		{
			FilePath[i] = Substitute;
			continue;
		}
		for (const TCHAR* pch = &INVALID_CHARS[0]; *pch != 0; ++pch)
		{
			if (*pch == FilePath[i])
			{
				FilePath[i] = Substitute;
				break;
			}
		}		
	}
}
