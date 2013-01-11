#include "stdafx.h"
#include "resource.h"
#include <atlctrls.h>
#include <atlctrlx.h>

#include <shellapi.h>
#include <winver.h>
#include "autores.h"
#include "aboutdlg.h"
#include "ndas/ndupdate.h"
#include "appconf.h"
#include "apperrdlg.h"
#include "waitdlg.h"

extern LANGID _CurrentUILangID;

static 
HFONT 
pGetTitleFont();

static
BOOL
pGetVerFileInfo(LPCTSTR szFilePath, VS_FIXEDFILEINFO& vffi);

static
BOOL
pGetModuleVerFileInfo(VS_FIXEDFILEINFO& vffi);

static 
BOOL
pIsFileFlagSet(VS_FIXEDFILEINFO& vffi, DWORD dwFlag);

static
VOID
pGetVersionStrings(CString& strAppVer, CString& strProdVer);

LRESULT 
CAboutDialog::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	BOOL fSuccess = FALSE;

	CenterWindow(GetParent());

	m_wndHyperLink.SubclassWindow(GetDlgItem(IDC_LINK));

	CString strHyperLink;
	strHyperLink.LoadString(IDS_ABOUTDLG_HYPERLINK);
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

	CString colName[2];
	fSuccess = colName[0].LoadString(IDS_ABOUTDLG_COL_COMPONENT);
	ATLASSERT(fSuccess);
	fSuccess = colName[1].LoadString(IDS_ABOUTDLG_COL_VERSION);
	ATLASSERT(fSuccess);

	wndListView.AddColumn(colName[0], 0);
	wndListView.AddColumn(colName[1], 1);

	wndListView.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);

	CString strAppTitle;
	strAppTitle.LoadString(IDS_MAIN_TITLE);
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
	m_pix.LoadFromResource(
		_Module.GetResourceInstance(), 
		IDB_ABOUT_HEADER, 
		_T("IMAGE"));

	//
	// Enable/Disable Check for update button
	//
	m_wndUpdate.Attach(GetDlgItem(IDC_CHECK_UPDATE));

	AutoHModule hUpdateDLL = ::LoadLibrary(_T("ndupdate.dll"));
	if (NULL != (HMODULE)hUpdateDLL) {
	} else {
		m_wndUpdate.ShowWindow(FALSE);
	}
		
	return TRUE;
}

void 
CAboutDialog::OnPaint(HDC hDC)
{
	//
	// HDC hDC is not a DC
	// bug in atlcrack.h
	//

	CPaintDC dc(m_hWnd);
	SIZE sizePic = {0, 0};
	m_pix.GetSizeInPixels(sizePic);

	CRect rcWnd, rcClient;
	GetClientRect(rcClient);
	GetWindowRect(rcWnd);

	ATLTRACE(_T("Picture Size: %d, %d\n"), sizePic.cx, sizePic.cy);
	ATLTRACE(_T("Client Size: %d, %d\n"), rcClient.Width(), rcClient.Height());
	ATLTRACE(_T("Window Size: %d, %d\n"), rcWnd.Width(), rcWnd.Height());

	//
	// adjust the picture size to the same width of the dialog
	//

	SIZE sizeAdj = 
	{ 
		rcWnd.Width(), 
		MulDiv(sizePic.cy, rcWnd.Width(), sizePic.cx)
	};

	LONG lBaseUnits = GetDialogBaseUnits();
	INT baseX = LOWORD(lBaseUnits);
	INT baseY = HIWORD(lBaseUnits);
	INT tplX = MulDiv(sizePic.cx, 4, baseX);
	INT tplY = MulDiv(sizePic.cy, 4, baseY);
	ATLTRACE(_T("Adjusted Size: %d, %d\n"), sizeAdj.cx, sizeAdj.cy);

	CRect rectPic(CPoint(0,0),sizeAdj);

	CDCHandle dcHandle;
	dcHandle.Attach((HDC)dc);
	m_pix.Render(dcHandle, rectPic);
}

VOID 
CAboutDialog::OnCheckUpdate(UINT /* wNotifyCode */, int /* wID */, HWND /* hWndCtl */)
{
	NDUPDATE_SYSTEM_INFO sysInfo = {0};
	NDUPDATE_UPDATE_INFO_V2 updateInfo = {0};
	sysInfo.dwLanguageSet = 0;
	sysInfo.dwPlatform = NDAS_PLATFORM_WIN2K;
	sysInfo.dwVendor = 0;

	VS_FIXEDFILEINFO vffi = {0};
	BOOL fSuccess = pGetModuleVerFileInfo(vffi);
	if (!fSuccess) {
		//
		// This is a fallback
		//
		ATLASSERT(FALSE);
		sysInfo.ProductVersion.wMajor = 3;
		sysInfo.ProductVersion.wMinor = 10;
		sysInfo.ProductVersion.wBuild = 0;
		sysInfo.ProductVersion.wPrivate = 0;
	} else {
		sysInfo.ProductVersion.wMajor = HIWORD(vffi.dwProductVersionMS);
		sysInfo.ProductVersion.wMinor = LOWORD(vffi.dwProductVersionMS);
		sysInfo.ProductVersion.wBuild = HIWORD(vffi.dwProductVersionLS);
		sysInfo.ProductVersion.wPrivate = LOWORD(vffi.dwProductVersionLS);
	}

	TCHAR szUpdateBaseURL[NDUPDATE_MAX_URL] = {0};

	fSuccess = pGetAppConfigValue(
		_T("UpdateURL"), 
		szUpdateBaseURL, 
		NDUPDATE_MAX_URL);

	if (!fSuccess) {
		HRESULT hr = ::StringCchCopy(
			szUpdateBaseURL, 
			NDUPDATE_MAX_URL, 
			_T("http://updates.ximeta.com/update/"));
		ATLASSERT(SUCCEEDED(hr));
	}

	AutoHModule hUpdateDLL = ::LoadLibrary(_T("ndupdate.dll"));
	if (NULL == (HMODULE)hUpdateDLL) {
		return;
	}

	typedef BOOL (WINAPI* NDUPDATE_NdasUpdateDoUpdate)(HWND, LPCTSTR, PNDUPDATE_SYSTEM_INFO);
	NDUPDATE_NdasUpdateDoUpdate pfnDoUpdate = (NDUPDATE_NdasUpdateDoUpdate)
		::GetProcAddress(hUpdateDLL, "NdasUpdateDoUpdate");

	LCID lcid = ::GetThreadLocale();
	fSuccess = ::SetThreadLocale(MAKELCID(_CurrentUILangID,SORT_DEFAULT));
	ATLASSERT(fSuccess);

	fSuccess = pfnDoUpdate(m_hWnd, szUpdateBaseURL, &sysInfo);
	if (!fSuccess) {
		ATLTRACE(_T("Update function failed: %08X"), ::GetLastError());
	}

	fSuccess = ::SetThreadLocale(lcid);
	ATLASSERT(fSuccess);
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

static
BOOL
pGetVerFileInfo(LPCTSTR szFilePath, VS_FIXEDFILEINFO& vffi)
{

	DWORD dwFVHandle;
	DWORD dwFVISize = ::GetFileVersionInfoSize(szFilePath, &dwFVHandle);
	if (0 == dwFVISize) {
		return FALSE;
	}

	PVOID pfvi = ::HeapAlloc(
		::GetProcessHeap(), 
		HEAP_ZERO_MEMORY, 
		dwFVISize);

	if (NULL == pfvi) {
		return FALSE;
	}

	AutoProcessHeap autoPfvi = pfvi;

	BOOL fSuccess = ::GetFileVersionInfo(
		szFilePath, 
		dwFVHandle, 
		dwFVISize, 
		pfvi);

	if (!fSuccess) {
		return FALSE;
	}

	VS_FIXEDFILEINFO* pffi;
	UINT pffiLen;
	fSuccess = ::VerQueryValue(pfvi, _T("\\"), (LPVOID*)&pffi, &pffiLen);
	if (!fSuccess || pffiLen != sizeof(VS_FIXEDFILEINFO)) {
		return FALSE;
	}

	::CopyMemory(
		&vffi,
		pffi,
		pffiLen);

	return TRUE;
}

static
BOOL
pGetModuleVerFileInfo(VS_FIXEDFILEINFO& vffi)
{
	// Does not support very long file name at this time
	TCHAR szModuleName[MAX_PATH] = {0};
	DWORD nch = ::GetModuleFileName(NULL, szModuleName, MAX_PATH);
	_ASSERTE(0 != nch && MAX_PATH != nch);
	if (0 == nch || nch >= MAX_PATH) {
		return FALSE;
	}

	return pGetVerFileInfo(szModuleName, vffi);
}

static 
BOOL
pIsFileFlagSet(VS_FIXEDFILEINFO& vffi, DWORD dwFlag)
{
	return (vffi.dwFileFlagsMask & dwFlag) &&
		(vffi.dwFileFlags & dwFlag);
}

static
VOID
pGetVersionStrings(CString& strAppVer, CString& strProdVer)
{
	VS_FIXEDFILEINFO vffi = {0};
	BOOL fSuccess = pGetModuleVerFileInfo(vffi);
	if (fSuccess) {
		strAppVer.Format(_T("%d.%d.%d%s%s%s"),
			HIWORD(vffi.dwFileVersionMS),
			LOWORD(vffi.dwFileVersionMS),
			HIWORD(vffi.dwFileVersionLS),
			pIsFileFlagSet(vffi, VS_FF_PRIVATEBUILD) ? _T(" PRIV") : _T(""),
			pIsFileFlagSet(vffi, VS_FF_PRERELEASE) ? _T(" PRERELEASE") : _T(""),
			pIsFileFlagSet(vffi, VS_FF_DEBUG) ? _T(" DEBUG") : _T(""));
		if (LOWORD(vffi.dwProductVersionLS) == 0) {
			strProdVer.Format(_T("%d.%d.%d"),
				HIWORD(vffi.dwProductVersionMS),
				LOWORD(vffi.dwProductVersionMS),
				HIWORD(vffi.dwProductVersionLS));
		} else {
			strProdVer.Format(_T("%d.%d.%d.%d"),
				HIWORD(vffi.dwProductVersionMS),
				LOWORD(vffi.dwProductVersionMS),
				HIWORD(vffi.dwProductVersionLS),
				LOWORD(vffi.dwProductVersionLS));
		}
	} else {
		strAppVer = _T("(unavailable)");
		strProdVer = _T("(unavailable)");
	}
}
