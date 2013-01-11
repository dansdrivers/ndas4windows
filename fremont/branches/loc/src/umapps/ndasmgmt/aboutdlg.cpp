#include "stdafx.h"
#include "resource.h"
#include <atlctrls.h>
#include <atlctrlx.h>

#include <shellapi.h>
#include <winver.h>
#include <xtl/xtlautores.h>
#include "aboutdlg.h"
#include <ndas/ndupdate.h>
#include "appconf.h"
#include "apperrdlg.h"
#include "waitdlg.h"
#include "ndasmgmt.h"
#include "fileversioninfo.h"

static 
HFONT 
pGetTitleFont();

static
void
pGetVersionStrings(CString& strAppVer, CString& strProdVer);

LRESULT 
CAboutDialog::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	BOOL fSuccess = FALSE;

	CenterWindow(GetParent());

	m_wndHyperLink.SubclassWindow(GetDlgItem(IDC_LINK));

	CString strHyperLink = (LPCTSTR) IDS_ABOUTDLG_HYPERLINK;
	m_wndHyperLink.SetHyperLink(strHyperLink);

	CString strAppVer, strProdVer;

	pGetVersionStrings(strAppVer, strProdVer);

	CString strProdVerText;
	TCHAR szProdVerFmt[256] = {0};

	CStatic wndProdVer;
	wndProdVer.Attach(GetDlgItem(IDC_PRODVER));
	wndProdVer.GetWindowText(szProdVerFmt, 256);
	strProdVerText.FormatMessage(szProdVerFmt, strProdVer);
	wndProdVer.SetWindowText(strProdVerText);

	CStatic wndProdName;
	wndProdName.Attach(GetDlgItem(IDC_PRODNAME));
	wndProdName.SetFont(pGetTitleFont());

	CListViewCtrl wndListView(GetDlgItem(IDC_COMPVER));

	CString colName[2] = {
		CString((LPCTSTR) IDS_ABOUTDLG_COL_COMPONENT),
		CString((LPCTSTR) IDS_ABOUTDLG_COL_VERSION)
	};

	wndListView.AddColumn(colName[0], 0);
	wndListView.AddColumn(colName[1], 1);

	wndListView.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);

	CString strAppTitle = (LPCTSTR) IDS_MAIN_TITLE;
	wndListView.AddItem(0, 0, strAppTitle);
	wndListView.SetItemText(0, 1, strAppVer);

	CRect rcListView;
	wndListView.GetClientRect(rcListView);
	wndListView.SetColumnWidth(0, LVSCW_AUTOSIZE);
	wndListView.SetColumnWidth(1, 
		rcListView.Width() - wndListView.GetColumnWidth(0));

	//
	// Image Header
	//
	m_wndImage.SetImage(IDB_ABOUT_HEADER);
	m_wndImage.SubclassWindow(GetDlgItem(IDC_HEADER));

	//
	// Enable/Disable Check for update button
	//
	m_wndUpdate.Attach(GetDlgItem(IDC_CHECK_UPDATE));

	m_hUpdateDll = ::LoadLibrary(_T("ndupdate.dll"));
	if (m_hUpdateDll.IsInvalid()) 
	{
		m_wndUpdate.ShowWindow(FALSE);
	}

	m_pfnDoUpdate = (NDUPDATE_NdasUpdateDoUpdate)
		::GetProcAddress(m_hUpdateDll, "NdasUpdateDoUpdate");
	if (NULL == m_pfnDoUpdate)
	{
		m_wndUpdate.ShowWindow(FALSE);
	}

	return TRUE;
}

void
CAboutDialog::OnCheckUpdate(UINT /* wNotifyCode */, int /* wID */, HWND /* hWndCtl */)
{
	NDUPDATE_SYSTEM_INFO sysInfo = {0};
	NDUPDATE_UPDATE_INFO_V2 updateInfo = {0};
	sysInfo.dwLanguageSet = 0;
	sysInfo.dwPlatform = NDAS_PLATFORM_WIN2K;
	sysInfo.dwVendor = 0;

	VS_FIXEDFILEINFO fileInfo = {0};
	HRESULT hr = FvReadFileVersionFixedFileInfoByHandle(NULL, &fileInfo);
	if (FAILED(hr)) 
	{
		//
		// This is a fallback
		//
		ATLASSERT(FALSE);
		sysInfo.ProductVersion.wMajor = 3;
		sysInfo.ProductVersion.wMinor = 10;
		sysInfo.ProductVersion.wBuild = 0;
		sysInfo.ProductVersion.wPrivate = 0;
	}
	else 
	{
		sysInfo.ProductVersion.wMajor = HIWORD(fileInfo.dwProductVersionMS);
		sysInfo.ProductVersion.wMinor = LOWORD(fileInfo.dwProductVersionMS);
		sysInfo.ProductVersion.wBuild = HIWORD(fileInfo.dwProductVersionLS);
		sysInfo.ProductVersion.wPrivate = LOWORD(fileInfo.dwProductVersionLS);
	}

	TCHAR szUpdateBaseURL[NDUPDATE_MAX_URL] = {0};

	BOOL success = pGetAppConfigValue(
		_T("UpdateURL"), 
		szUpdateBaseURL, 
		NDUPDATE_MAX_URL);

	if (!success) 
	{
		HRESULT hr = ::StringCchCopy(
			szUpdateBaseURL, 
			NDUPDATE_MAX_URL, 
			_T("http://updates.ximeta.com/update/"));
		ATLASSERT(SUCCEEDED(hr));
	}

	if (m_hUpdateDll.IsInvalid() || NULL == m_pfnDoUpdate) 
	{
		return;
	}

	//LCID lcid = ::GetThreadLocale();
	//fSuccess = ::SetThreadLocale(MAKELCID(ndasmgmt::CurrentUILangID,SORT_DEFAULT));
	//ATLASSERT(fSuccess);

	success = m_pfnDoUpdate(m_hWnd, szUpdateBaseURL, &sysInfo);
	if (!success) 
	{
		ATLTRACE("Update function failed: %08X\n", ::GetLastError());
	}

	//fSuccess = ::SetThreadLocale(lcid);
	// ATLASSERT(fSuccess);
}

static 
HFONT 
pGetTitleFont()
{
	BOOL fSuccess = FALSE;
	static HFONT hTitleFont = NULL;
	if (NULL != hTitleFont) {
		return hTitleFont;
	}

	NONCLIENTMETRICS ncm = {0};
	ncm.cbSize = sizeof(NONCLIENTMETRICS);
	fSuccess = ::SystemParametersInfo(
		SPI_GETNONCLIENTMETRICS, 
		sizeof(NONCLIENTMETRICS), 
		&ncm, 
		0);
	ATLASSERT(fSuccess);

	LOGFONT TitleLogFont = ncm.lfMessageFont;
	// TitleLogFont.lfWeight = FW_BOLD;


	INT TitleFontSize = 11; //::StrToInt(strFontSize);

	HDC hdc = ::GetDC(NULL);
	TitleLogFont.lfHeight = 0 - 
		::GetDeviceCaps(hdc,LOGPIXELSY) * TitleFontSize / 72;

	hTitleFont = ::CreateFontIndirect(&TitleLogFont);
	::ReleaseDC(NULL, hdc);

	return hTitleFont;
}

HRESULT
pGetFileVersionInfoByHandle(
	__in_opt HANDLE ModuleHandle,
	__deref_out PVOID* FileVersionInfo);

HRESULT
pGetFileVersionInfoByFileName(
	__in LPCTSTR FileName,
	__deref_out PVOID* FileVersionInfo);

inline BOOL pIsFileFlagSet(const VS_FIXEDFILEINFO* FileInfo, DWORD Flag)
{
	return (FileInfo->dwFileFlagsMask & Flag) &&
		(FileInfo->dwFileFlags & Flag);
}

void
pGetVersionStrings(CString& strAppVer, CString& strProdVer)
{
	HRESULT hr;

	CHeapPtr<void> fileVersionInfo;
	hr = FvReadFileVersionInfoByHandle(NULL, &fileVersionInfo);

	if (FAILED(hr))
	{
		strAppVer = _T("(unavailable)");
		strProdVer = _T("(unavailable)");
		return;
	}

	UINT length;
	LPCTSTR fileVersion;

	hr = FvGetFileVersionInformationString(
		fileVersionInfo,
		FVI_FileVersion,
		&fileVersion,
		&length);

	if (FAILED(hr))
	{
		strAppVer = _T("(unavailable)");
	}
	else
	{
		strAppVer = fileVersion;
	}

	LPCTSTR productVersion;

	hr = FvGetFileVersionInformationString(
		fileVersionInfo,
		FVI_FileVersion,
		&productVersion,
		&length);

	if (FAILED(hr))
	{
		strProdVer = _T("(unavailable)");
	}
	else
	{
		strProdVer = productVersion;
	}

	const VS_FIXEDFILEINFO* fileInfo;
	fileInfo = FvGetFixedFileInfo(fileVersionInfo);

	if (NULL != fileInfo)
	{
		if (pIsFileFlagSet(fileInfo, VS_FF_PRIVATEBUILD))
		{
			strAppVer += _T(" PRIVATE");
		}
		if (pIsFileFlagSet(fileInfo, VS_FF_PRERELEASE))
		{
			strAppVer += _T(" PRERELEASE");
		}
		if (pIsFileFlagSet(fileInfo, VS_FF_DEBUG))
		{
			strAppVer += _T(" DEBUG");
		}
	}
}
