// MainDlg.cpp : implementation of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"
#define NO_XDEBUG
#include "xdebug.h"

#include "MainDlg.h"

#define XDF_LPXCOMM			0x00000001
#define XDF_INSTMAN			0x00000002
#define XDF_EVENTPUB		0x00000004
#define XDF_EVENTMON		0x00000008
#define XDF_NDASLOGDEV		0x00000010
#define XDF_NDASLOGDEVMAN	0x00000020
#define XDF_PNP             0x00000040
#define XDF_SERVICE			0x00000080
#define XDF_CMDPROC         0x00000100
#define XDF_CMDSERVER       0x00000200
#define XDF_NDASDEV         0x00000400
#define XDF_NDASDEVHB       0x00000800
#define XDF_NDASDEVREG		0x00001000
#define XDF_DRVMATCH        0x00002000
#define XDF_NDASIX			0x00004000
#define XDF_MAIN            0xF0000000

static DWORD ParseHex(LPCTSTR szText, DWORD cbText);

LRESULT CMainDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	// center the dialog on the screen
	CenterWindow();

	// set icons
	HICON hIcon = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
	SetIcon(hIcon, TRUE);
	HICON hIconSmall = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	SetIcon(hIconSmall, FALSE);

	m_lvModules.SubclassWindow(GetDlgItem(IDC_MODULES));
	m_lvModules.AddColumn(_T("Module Name"), 0);

	m_wndAlways.Attach(GetDlgItem(IDC_ALWAYS));
	m_wndError.Attach(GetDlgItem(IDC_ERROR));
	m_wndWarning.Attach(GetDlgItem(IDC_WARNING));
	m_wndInfo.Attach(GetDlgItem(IDC_INFO));
	m_wndTrace.Attach(GetDlgItem(IDC_TRACE));
	m_wndNoise.Attach(GetDlgItem(IDC_NOISE));
	m_wndCustom.Attach(GetDlgItem(IDC_CUSTOM));
	m_wndCustomValue.Attach(GetDlgItem(IDC_CUSTOM_LEVEL_VALUE));

	m_wndCustomValue.SetLimitText(8);

	m_dwFlags = 0;
	UpdateModuleList(0);

	CRect rect;
	m_lvModules.GetClientRect(rect);
	m_lvModules.SetColumnWidth(0, rect.Width());

	OnRefresh(0,0,0);

	return TRUE;
}

LRESULT CMainDlg::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CSimpleDialog<IDD_ABOUTBOX, FALSE> dlg;
	dlg.DoModal();
	return 0;
}

LRESULT CMainDlg::OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	// TODO: Add validation code 
	EndDialog(wID);
	return 0;
}

LRESULT CMainDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(wID);
	return 0;
}

void CMainDlg::OnApply(UINT uMsg, int wID, HWND hWndCtl)
{
	m_dwFlags = GetModuleCheckList();
	m_dwLevel = GetOutputLevelButtonStatus();
	SetRegValues();
}

void CMainDlg::OnRefresh(UINT uMsg, int wID, HWND hWndCtl)
{
	GetRegValues();
	SetModuleCheckList(m_dwFlags);
	SetOutputLevelButtonStatus(m_dwLevel);
}

void CMainDlg::GetRegValues()
{
	HKEY hKey;
	
	LONG lResult = ::RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		REGSTR_PATH_SERVICES _T("\\ndashelper\\parameters"),
		0,
		KEY_QUERY_VALUE,
		&hKey);

	if (ERROR_SUCCESS != lResult) {
		m_dwLevel = 0xFFFFFFFF;
		m_dwFlags = 0x0;
		MessageBox(_T("No value defined. Default value loaded."));
		return;
	}

	DWORD dwType, cbData = sizeof(DWORD);

	lResult = ::RegQueryValueEx(
		hKey, 
		_T("DebugLevel"), 
		NULL, 
		&dwType, 
		(LPBYTE)&m_dwLevel, 
		&cbData);

	if (ERROR_SUCCESS != lResult) {
		m_dwLevel = 0xFFFFFFFF;
		MessageBox(_T("DebugLevel not defined. Default value is loaded."));
	}

	lResult = ::RegQueryValueEx(
		hKey, 
		_T("DebugFlags"), 
		0, 
		&dwType, 
		(LPBYTE) &m_dwFlags, 
		&cbData);

	if (ERROR_SUCCESS != lResult) {
		m_dwFlags = 0x0;
		MessageBox(_T("DebugFlags not defined. Default value is loaded."));
	}

	::RegCloseKey(hKey);
}

void CMainDlg::SetRegValues()
{
	HKEY hKey;
	
	LONG lResult = ::RegCreateKeyEx(
		HKEY_LOCAL_MACHINE,
		REGSTR_PATH_SERVICES _T("\\ndashelper\\parameters"),
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_SET_VALUE,
		NULL,
		&hKey,
		NULL);

	if (ERROR_SUCCESS != lResult) {
		MessageBox(_T("Cannot open the parameters key to set values."));
		return;
	}

	lResult = ::RegSetValueEx(
		hKey, 
		_T("DebugLevel"), 
		0, 
		REG_DWORD, 
		(const BYTE*) &m_dwLevel, 
		sizeof(DWORD));

	if (ERROR_SUCCESS != lResult) {
		MessageBox(_T("Setting DebugLevel failed."));
	}

	lResult = ::RegSetValueEx(
		hKey, 
		_T("DebugFlags"), 
		0, 
		REG_DWORD, 
		(const BYTE*) &m_dwFlags, 
		sizeof(DWORD));

	if (ERROR_SUCCESS != lResult) {
		m_dwLevel = 0x0;
		MessageBox(_T("Setting DebugFlags failed."));
	}

	::RegCloseKey(hKey);
}


typedef struct _MODULE_DEF {
	DWORD dwFlag;
	LPCTSTR szDisplayName;
} MODULE_DEF, *PMODULE_DEF;

#define MODULE_DEF_ENTRY(x) {x, _T(#x)}

const MODULE_DEF ModuleDefs[] = {
	MODULE_DEF_ENTRY(XDF_INSTMAN),
	MODULE_DEF_ENTRY(XDF_EVENTPUB),
	MODULE_DEF_ENTRY(XDF_EVENTMON),
	MODULE_DEF_ENTRY(XDF_NDASLOGDEV),
	MODULE_DEF_ENTRY(XDF_NDASLOGDEVMAN),
	MODULE_DEF_ENTRY(XDF_PNP),
	MODULE_DEF_ENTRY(XDF_SERVICE),
	MODULE_DEF_ENTRY(XDF_CMDPROC),
	MODULE_DEF_ENTRY(XDF_CMDSERVER),
	MODULE_DEF_ENTRY(XDF_NDASDEV),
	MODULE_DEF_ENTRY(XDF_NDASDEVHB),
	MODULE_DEF_ENTRY(XDF_NDASDEVREG),
	MODULE_DEF_ENTRY(XDF_DRVMATCH),
	MODULE_DEF_ENTRY(XDF_NDASIX),
	MODULE_DEF_ENTRY(XDF_MAIN)
};

static const DWORD ModuleDefsCount = sizeof(ModuleDefs) / sizeof(ModuleDefs[0]);

void CMainDlg::UpdateModuleList(DWORD dwFlags)
{
	for (DWORD i = 0; i < ModuleDefsCount; ++i) {
		DWORD nItem = m_lvModules.InsertItem(i, ModuleDefs[i].szDisplayName);
		if (dwFlags & ModuleDefs[i].dwFlag) {
			m_lvModules.SetCheckState(nItem, TRUE);
		} else {
			m_lvModules.SetCheckState(nItem, FALSE);
		}
		m_lvModules.SetItemData(nItem, ModuleDefs[i].dwFlag);
	}

}

void CMainDlg::SetModuleCheckList(DWORD dwFlags)
{
	for (int i = 0; i < m_lvModules.GetItemCount(); ++i) {
		DWORD dwFlag = (DWORD) m_lvModules.GetItemData(i);
		if (dwFlag & dwFlags) {
			m_lvModules.SetCheckState(i, TRUE);
		} else {
			m_lvModules.SetCheckState(i, FALSE);
		}
	}
}

DWORD CMainDlg::GetModuleCheckList()
{
	DWORD dwFlags = 0;
	for (int i = 0; i < m_lvModules.GetItemCount(); ++i) {
		if (m_lvModules.GetCheckState(i)) {
			dwFlags |=(DWORD) m_lvModules.GetItemData(i);
		}
	}
	return dwFlags;
}

void CMainDlg::SetOutputLevelButtonStatus(DWORD dwValue)
{
	m_wndAlways.SetCheck(BST_UNCHECKED);
	m_wndError.SetCheck(BST_UNCHECKED);
	m_wndWarning.SetCheck(BST_UNCHECKED);
	m_wndInfo.SetCheck(BST_UNCHECKED);
	m_wndTrace.SetCheck(BST_UNCHECKED);
	m_wndNoise.SetCheck(BST_UNCHECKED);
	m_wndCustom.SetCheck(BST_UNCHECKED);

	switch (dwValue) {
	case XDebug::OL_ALWAYS:
		m_wndAlways.SetCheck(BST_CHECKED);
		break;
	case XDebug::OL_ERROR:
		m_wndError.SetCheck(BST_CHECKED);
		break;
	case XDebug::OL_WARNING:
		m_wndWarning.SetCheck(BST_CHECKED);
		break;
	case XDebug::OL_INFO:
		m_wndInfo.SetCheck(BST_CHECKED);
		break;
	case XDebug::OL_TRACE:
		m_wndTrace.SetCheck(BST_CHECKED);
		break;
	case XDebug::OL_NOISE:
		m_wndNoise.SetCheck(BST_CHECKED);
		break;
	case XDebug::OL_NONE:
	default:
		m_wndCustom.SetCheck(BST_CHECKED);
		{
			TCHAR szBuf[9] = {0};
			HRESULT hr = StringCchPrintf(szBuf, 9, _T("%08X"), dwValue);
			m_wndCustomValue.SetWindowText(szBuf);
		}
		break;
	}
}

DWORD CMainDlg::GetOutputLevelButtonStatus()
{
	DWORD dwValue = 0;
	if (BST_CHECKED == m_wndAlways.GetCheck()) {
		dwValue = XDebug::OL_ALWAYS;
	} else if (BST_CHECKED == m_wndError.GetCheck()) {
		dwValue = XDebug::OL_ERROR;
	} else if (BST_CHECKED == m_wndWarning.GetCheck()) {
		dwValue = XDebug::OL_WARNING;
	} else if (BST_CHECKED == m_wndInfo.GetCheck()) {
		dwValue = XDebug::OL_INFO;
	} else if (BST_CHECKED == m_wndTrace.GetCheck()) {
		dwValue = XDebug::OL_TRACE;
	} else if (BST_CHECKED == m_wndNoise.GetCheck()) {
		dwValue = XDebug::OL_NOISE;
	} else {
		TCHAR szValue[9];
		DWORD cch = m_wndCustomValue.GetWindowText(szValue, 9);
		dwValue = ParseHex(szValue, cch);
	}
	return dwValue;
}

static DWORD ParseHex(LPCTSTR szText, DWORD cbText)
{
	DWORD dwValue = 0;
	for (DWORD i = 0; i < cbText; ++i) {
		dwValue <<= 4;
		if (szText[i] == _T('\0')) {
			ATLTRACE(_T("Parsed value 0x%08X\n"), dwValue);
			return dwValue;
		} else if (szText[i] >= _T('A') && szText[i] <= _T('F')) {
			dwValue += DWORD(szText[i] - _T('A')) + 10;
		} else if (szText[i] >= _T('a') && szText[i] <= _T('f')) {
			dwValue += DWORD(szText[i] - _T('a')) + 10;
		} else if (szText[i] >= _T('0') && szText[i] <= _T('9')) {
			dwValue += DWORD(szText[i] - _T('0'));
		} else {
			ATLTRACE(_T("Valid character %s\n"), szText);
			return 0;
		}
	}
	ATLTRACE(_T("Parsed value 0x%08X\n"), dwValue);
	return dwValue;
}
