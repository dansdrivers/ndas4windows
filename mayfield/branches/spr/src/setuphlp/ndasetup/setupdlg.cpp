//	setupdlg.cpp : implementation of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "winutil.h"
#include "msiproc.h"
#include "setupdlg.h"
#include "setuptask.h"

#define SETUP_REQUIRED_MSI_VERSION 200

CSetupInitDlg::CSetupInitDlg() :
	m_fCheckUpdate(FALSE),
	m_fDisableCheckUpdate(FALSE)
{
	m_szUpdateURL[0] = _T('\0');
	::ZeroMemory(&m_SysInfo, sizeof(NDUPDATE_SYSTEM_INFO));
}

LRESULT 
CSetupInitDlg::OnInitDialog(HWND hWnd, LPARAM /*lParam*/)
{
	cpGetSettings();

	m_wndOK.Attach(GetDlgItem(IDOK));
	m_wndCancel.Attach(GetDlgItem(IDCANCEL));

	m_wndBannerText.Attach(GetDlgItem(IDC_BANNER_TEXT));
	m_wndActionText.Attach(GetDlgItem(IDC_ACTION_TEXT));

	m_wndCheckUpdate.Attach(GetDlgItem(IDC_CHECK_UPDATE));

	m_wndProgress.Attach(GetDlgItem(IDC_PROGRESS));

	// center the dialog on the screen
	CenterWindow();

	// set icons
	HICON hIcon = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
	SetIcon(hIcon, TRUE);
	HICON hIconSmall = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
		IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
	SetIcon(hIconSmall, FALSE);

//	m_wndLangMenuButton = GetDlgItem(IDOK);
//	m_wndLangMenuButton.SetMenuID(IDR_INSTALL_BUTTON_MENU);

	ShowProgressBar(FALSE);

	if (m_fDisableCheckUpdate) {
		m_wndCheckUpdate.SetCheck(BST_UNCHECKED);
		m_wndCheckUpdate.ShowWindow(SW_HIDE);
	} else {
		m_wndCheckUpdate.SetCheck(
			m_fCheckUpdate ? BST_CHECKED : BST_UNCHECKED);
	}

	m_wndActionText.SetWindowText(_T(""));

	CString strCancel; strCancel.LoadString(IDS_CANCEL);
	m_wndCancel.SetWindowText(strCancel);

	m_wndCancel.EnableWindow(FALSE);
	m_wndOK.EnableWindow(FALSE);


	m_pCurrentTask = NULL;
	m_CurrentSetupTask = ST_NONE;

	DoInit();

	return TRUE;
}

VOID
CSetupInitDlg::OnOK(UINT uNotifyCode, INT wID, HWND hWndCtrl)
{
	//
	// Check update
	//
	ShowProgressBar();
	m_wndOK.ShowWindow(SW_HIDE);
	m_wndCheckUpdate.EnableWindow(FALSE);

	CString strCancel; strCancel.LoadString(IDS_CANCEL);
	m_wndCancel.SetWindowText(strCancel);

	if (BST_CHECKED == m_wndCheckUpdate.GetCheck()) {
		DoCheckUpdate();
	} else {
		DoCheckMsi();
	}


}

VOID
CSetupInitDlg::OnCancel(UINT uNotifyCode, INT wID, HWND hWndCtrl)
{
	if (NULL != m_pCurrentTask) {
		m_pCurrentTask->Cancel();
	} else {
		EndDialog(wID);
	}
}

VOID
CSetupInitDlg::OnChangeLanguage(UINT uNotifyCode, INT wID, HWND hWndCtrl)
{
	if (ID_INSTALL_ENU == wID) {
		m_wndOK.SetWindowText(_T("&Install (English)"));
	} else if (ID_INSTALL_KOR == wID) {
		m_wndOK.SetWindowText(_T("&Install (Korean)"));
	}
}

LRESULT 
CSetupInitDlg::OnTaskDone(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (NULL != m_pCurrentTask) {
		m_pCurrentTask->WaitForStop(30000);
		delete m_pCurrentTask;
		m_pCurrentTask = NULL;
	}

	switch (m_CurrentSetupTask)
	{
	case ST_NONE:
		return TRUE;
	case ST_INIT:
		{
			m_wndOK.EnableWindow(TRUE);
			m_wndCancel.EnableWindow(TRUE);

			SetActionText(_T(""));
			ShowProgressBar(FALSE);
			return TRUE;
		}
	case ST_CHECK_UPDATE:
		{
			if (TASKRET_EXIT == (UINT)wParam) {
				EndDialog(SETUPDLG_RET_EXECUTEUPDATER);
				return TRUE;
			}
			DoCheckMsi();
			return TRUE;
		}
	case ST_CHECK_MSI:
		{
			DoCheckUpgrade();
			return TRUE;
		}
	case ST_CHECK_UPGRADE:
		{
			DoCacheMsi();
			return TRUE;
		}
	case ST_CACHE_MSI:
		{
			EndDialog(SETUPDLG_RET_EXECUTEINSTALLER);
			return TRUE;
		}
	case ST_INSTALL:
		{
			EndDialog(ERROR_SUCCESS);
			return TRUE;
		}
	default:
		ATLASSERT(FALSE);
	}

	return TRUE;
}

BOOL 
CSetupInitDlg::HasUserCanceled()
{
	return FALSE;
}

VOID 
CSetupInitDlg::SetActionText(LPCTSTR szActionText)
{
	m_wndActionText.SetWindowText(szActionText);
}

VOID 
CSetupInitDlg::SetActionText(UINT uiActionTextID)
{
	CString str;
	str.LoadString(uiActionTextID);
	SetActionText((LPCTSTR)str);
}

VOID 
CSetupInitDlg::SetBannerText(LPCTSTR szBannerText)
{
	m_wndBannerText.SetWindowText(szBannerText);
}

VOID 
CSetupInitDlg::SetBannerText(UINT uiBannerTextID)
{
	CString str;
	str.LoadString(uiBannerTextID);
	SetBannerText((LPCTSTR)str);
}

VOID 
CSetupInitDlg::InitProgressBar(ULONG ulProgressMax)
{
	m_wndProgress.SetRange(0, ulProgressMax);
}

VOID 
CSetupInitDlg::SetProgressBar(ULONG ulProgress)
{
	m_wndProgress.SetPos(ulProgress);
}

HWND 
CSetupInitDlg::GetCurrentWindow()
{
	return m_hWnd;
}

VOID 
CSetupInitDlg::ShowProgressBar(BOOL fShow)
{
	m_wndProgress.ShowWindow(fShow ? SW_SHOW : SW_HIDE);
}

VOID
CSetupInitDlg::NotifyTaskDone(UINT uiRetCode)
{
	PostMessage(WM_TASK_DONE, (WPARAM)uiRetCode, 0);
}

VOID
CSetupInitDlg::NotifyFatalExit(UINT uiRetCode)
{
	EndDialog(uiRetCode);
}

VOID
CSetupInitDlg::DoInit()
{
	ATLASSERT(NULL == m_pCurrentTask);

	m_CurrentSetupTask = ST_INIT;
	m_pCurrentTask = new CSetupInitalize(this);
	m_pCurrentTask->Start();
}

VOID
CSetupInitDlg::DoCheckMsi()
{
	ATLASSERT(NULL == m_pCurrentTask);
	m_CurrentSetupTask = ST_CHECK_MSI;

	SetActionText(IDS_ACTION_CHECK_MSI);
	BOOL fUpgrade = IsMsiUpgradeNecessary(SETUP_REQUIRED_MSI_VERSION);
	if (fUpgrade) {
		m_pCurrentTask = new CSetupUpgradeMsi(this);
		if (NULL != m_pCurrentTask) {
			m_pCurrentTask->Start();
		} else {
			NotifyTaskDone();
		}
	} else {
		NotifyTaskDone();
	}
}

VOID
CSetupInitDlg::DoCheckUpdate()
{
	ATLASSERT(NULL == m_pCurrentTask);

	SetActionText(IDS_ACTION_CHECK_UPDATE);
	m_CurrentSetupTask = ST_CHECK_UPDATE;
	
	NDUPDATE_SYSTEM_INFO nsi;
	nsi.dwLanguageSet = 0;
	nsi.dwPlatform = NDAS_PLATFORM_WIN2K;
	nsi.dwVendor = 0;
	nsi.ProductVersion.wMajor;
	nsi.ProductVersion.wMinor;
	nsi.ProductVersion.wBuild;
	nsi.ProductVersion.wPrivate;

	m_pCurrentTask = new CSetupCheckUpdate(this, m_szUpdateURL, m_SysInfo);
	if (NULL != m_pCurrentTask) {
		m_pCurrentTask->Start();
	} else {
		NotifyTaskDone();
	}
}

VOID
CSetupInitDlg::DoCacheMsi()
{
	SetActionText(IDS_ACTION_CACHE_MSI);
	m_CurrentSetupTask = ST_CACHE_MSI;

	UINT uiRet = CacheMsi(
		this, 
		GetPackagePath(),
		m_szCachedPackagePath, 
		MAX_PATH);
	if (ERROR_SUCCESS != uiRet) {
		PostErrorMessageBox(uiRet, IDS_ERR_CACHE_MSI_FAILED);
		NotifyFatalExit(uiRet);
	}

	NotifyTaskDone();
}


VOID
CSetupInitDlg::DoCheckUpgrade()
{
	ATLASSERT(NULL == m_pCurrentTask);
	m_CurrentSetupTask = ST_CHECK_UPGRADE;
	NotifyTaskDone();
}

UINT_PTR
CSetupInitDlg::ExecuteUpdater()
{
	ATLASSERT(NULL == m_pCurrentTask);

	STARTUPINFO startupinfo = {0};
	PROCESS_INFORMATION processinfo = {0};
	startupinfo.cb = sizeof(startupinfo);

	while (TRUE) {

		BOOL fSuccess = CreateProcess(
			m_szPostExecFile,
			NULL,
			NULL,
			NULL,
			FALSE,
			0,
			NULL,
			NULL,
			&startupinfo,
			&processinfo);

		DWORD dwError = ::GetLastError();

		if (!fSuccess) {

			INT_PTR iResponse = PostErrorMessageBox(
				dwError,
				IDS_ERR_RUN_UPDATE,
				MB_ABORTRETRYIGNORE | MB_ICONERROR);

			if (IDABORT == iResponse) {
				return ERROR_INSTALL_USEREXIT;
			} else if (IDRETRY == iResponse) {
				continue;
			} else if (IDIGNORE == iResponse) {
				return ERROR_SUCCESS;
			}
		} else {
			::CloseHandle(processinfo.hProcess);
		}

		break;
	}

	return ERROR_SUCCESS;
}

UINT_PTR
CSetupInitDlg::ExecuteInstaller()
{
	ATLASSERT(NULL == m_pCurrentTask);
	m_CurrentSetupTask = ST_INSTALL;

	CMsiApi msiApi;
	BOOL fSuccess = msiApi.Initialize();
	if (!fSuccess) {
		PostErrorMessageBox(ERROR_PROC_NOT_FOUND, IDS_ERR_LOADMSI);
	}

	UINT uiRet = msiApi.EnableLog(
		INSTALLLOGMODE_VERBOSE, 
		GetLogFilePath(),
		0);

	if (ERROR_SUCCESS != uiRet) {
		// warning
	}

	INSTALLUILEVEL uiLevel = msiApi.SetInternalUI(
		INSTALLUILEVEL_FULL, 
		NULL);

	ATLASSERT(INSTALLUILEVEL_NOCHANGE != uiLevel);

	m_szInstallCmdLine = _T("");

	uiRet = msiApi.InstallProduct(GetCachedPackagePath(), m_szInstallCmdLine);

	if (ERROR_SUCCESS != uiRet &&
		ERROR_INSTALL_FAILURE != uiRet &&
		ERROR_INSTALL_USEREXIT != uiRet &&
		ERROR_SUCCESS_REBOOT_INITIATED != uiRet &&
		ERROR_SUCCESS_REBOOT_REQUIRED != uiRet) 
	{
		LPTSTR lpszMessage;
		DWORD nChars = ::FormatMessage(
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
			NULL, 
			uiRet, 
			0, 
			(LPTSTR)&lpszMessage,
			0,
			NULL);

		PostMessageBox(
			lpszMessage, 
			MB_ICONERROR);
	}

	return uiRet;
}


VOID
CSetupInitDlg::SetPostExecuteFile(LPCTSTR szFileName)
{
	HRESULT hr = StringCchCopy(m_szPostExecFile, MAX_PATH, szFileName);
	ATLASSERT(SUCCEEDED(hr));
}

INT_PTR 
CSetupInitDlg::PostMessageBox(LPCTSTR szText, UINT uiType)
{
	CString strTitle;
	BOOL fSuccess = strTitle.LoadString(IDS_SETUP);
	ATLASSERT(fSuccess);
	return ::MessageBox(m_hWnd, szText, strTitle, uiType);
}

INT_PTR 
CSetupInitDlg::PostMessageBox(UINT uiTextID, UINT uiType)
{
	CString strText;
	BOOL fSuccess = strText.LoadString(uiTextID);
	ATLASSERT(fSuccess);
	return PostMessageBox((LPCTSTR)strText, uiType);
}

INT_PTR 
CSetupInitDlg::PostErrorMessageBox(DWORD dwError, UINT uiErrorID, UINT uiType)
{
	CString strErrorText;
	BOOL fSuccess = strErrorText.LoadString(uiErrorID);
	ATLASSERT(fSuccess);
	return PostErrorMessageBox(dwError, strErrorText, uiType);
}

INT_PTR 
CSetupInitDlg::PostErrorMessageBox(DWORD dwError, LPCTSTR szText, UINT uiType)
{
	CString strText;
	BOOL fSuccess = strText.FormatMessage(
		IDS_ERR_FORMAT, 
		dwError, 
		(LPCTSTR)szText);
	ATLASSERT(fSuccess);
	return PostMessageBox(strText, uiType);
}

LPCTSTR
CSetupInitDlg::GetPackagePath()
{
	TCHAR szModulePath[MAX_PATH];
	TCHAR* lpFilePart = NULL;
	DWORD nChars = GetModuleFileName(NULL, szModulePath, MAX_PATH);
	ATLASSERT(nChars > 0);
	if (0 == nChars) {
		return NULL;
	}

	nChars = GetFullPathName(szModulePath, MAX_PATH, m_szPackagePath, &lpFilePart);
	ATLASSERT(nChars > 0);
	if (0 == nChars) {
		return NULL;
	}

	if (NULL != lpFilePart) {
		*lpFilePart = _T('\0');
	}

	HRESULT hr = StringCchCat(m_szPackagePath, MAX_PATH, _T("ndas.msi"));
	ATLASSERT(SUCCEEDED(hr));
	if (FAILED(hr)) {
		return NULL;
	}

	return m_szPackagePath;
}

LPCTSTR
CSetupInitDlg::GetLogFilePath()
{
	// DWORD nChars = GetSystemWindowsDirectory(m_szLogFilePath, MAX_PATH);
	DWORD nChars = GetWindowsDirectory(m_szLogFilePath, MAX_PATH);
	ATLASSERT(nChars > 0);
	if (0 == nChars) {
		return NULL;
	}

	HRESULT hr = StringCchCat(m_szLogFilePath, MAX_PATH, _T("\\NDASetup.log"));
	ATLASSERT(SUCCEEDED(hr));
	if (FAILED(hr)) {
		return NULL;
	}

	return m_szLogFilePath;
}

LPCTSTR
CSetupInitDlg::GetCachedPackagePath()
{
	return m_szCachedPackagePath;
}

BOOL
CSetupInitDlg::SetCachedPackagePath(LPCTSTR szPath)
{
	HRESULT hr = StringCchCopy(m_szCachedPackagePath, MAX_PATH, szPath);
	ATLASSERT(SUCCEEDED(hr));
	if (FAILED(hr)) {
		return FALSE;
	}
	return TRUE;
}

BOOL
CSetupInitDlg::cpGetSettings()
{
	TCHAR szINIFile[MAX_PATH];
	DWORD nChars = ::GetModuleFileName(NULL, szINIFile, MAX_PATH);
	if (0 == nChars || nChars < 4) {
		return FALSE;
	}

	// replace .exe with .ini
	szINIFile[nChars - 4] = _T('.');
	szINIFile[nChars - 3] = _T('i');
	szINIFile[nChars - 2] = _T('n');
	szINIFile[nChars - 1] = _T('i');

	m_SysInfo.ProductVersion.wMajor = (WORD) ::GetPrivateProfileInt(
		_T("Product"), 
		_T("VersionMajor"),
		0,
		szINIFile);

	m_SysInfo.ProductVersion.wMinor = (WORD) ::GetPrivateProfileInt(
		_T("Product"),
		_T("VersionMinor"),
		0,
		szINIFile);

	m_SysInfo.ProductVersion.wBuild = (WORD) ::GetPrivateProfileInt(
		_T("Product"),
		_T("VersionBuild"),
		0,
		szINIFile);

	m_SysInfo.ProductVersion.wPrivate = (WORD) ::GetPrivateProfileInt(
		_T("Product"),
		_T("VersionPrivate"),
		0,
		szINIFile);

	m_SysInfo.dwLanguageSet = ::GetPrivateProfileInt(
		_T("Product"),
		_T("LanguageSet"),
		0,
		szINIFile);

	m_SysInfo.dwVendor = ::GetPrivateProfileInt(
		_T("Product"),
		_T("Vendor"),
		0,
		szINIFile);

	m_SysInfo.dwPlatform = NDAS_PLATFORM_WIN2K;

	::GetPrivateProfileString(
		_T("Update"),
		_T("URL"),
		_T(""),
		m_szUpdateURL,
		MAX_PATH, 
		szINIFile);

	m_fCheckUpdate = (0 == ::GetPrivateProfileInt(
		_T("Update"),
		_T("CheckUpdate"),
		0,
		szINIFile)) ? FALSE : TRUE;

	BOOL fDisableCheckUpdate = (0 == ::GetPrivateProfileInt(
		_T("Update"),
		_T("Disable"),
		0,
		szINIFile)) ? FALSE : TRUE;

	if (_T('\0') == m_szUpdateURL[0] || fDisableCheckUpdate) {
		m_fDisableCheckUpdate = TRUE;
	}

	return TRUE;
}

