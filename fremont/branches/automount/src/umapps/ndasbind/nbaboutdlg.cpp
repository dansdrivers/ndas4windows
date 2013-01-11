// nbaboutdlg.cpp : implementation of the CAboutDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"
#include <xtl/xtlautores.h>
#include "nbaboutdlg.h"

static 
BOOL
pIsFileFlagSet(VS_FIXEDFILEINFO& vffi, DWORD dwFlag);

static
BOOL
pGetModuleVerFileInfo(VS_FIXEDFILEINFO& vffi);

static
BOOL
pGetVerFileInfo(LPCTSTR szFilePath, VS_FIXEDFILEINFO& vffi);

static
VOID
pGetVersionStrings(CString& strAppVer, CString& strProdVer);

static 
HFONT 
pGetTitleFont();

LRESULT 
CAboutDlg::OnInitDialog(HWND hWndCtrl, LPARAM lParam)
{
	CenterWindow(GetParent());

	CString strProductVer;

	CString strAppVerStr, strProdVerStr;
	pGetVersionStrings(strAppVerStr, strProdVerStr);

	CString strProdVerFmt;

	CStatic wndAppVer, wndAppName;
	wndAppName.Attach(GetDlgItem(IDC_APPNAME));
	wndAppName.SetFont(pGetTitleFont());
	
	wndAppVer.Attach(GetDlgItem(IDC_APPVER));
	wndAppVer.GetWindowText(strProdVerFmt.GetBuffer(256), 256);
	strProductVer.FormatMessage((LPCTSTR)strProdVerFmt, strProdVerStr);
	wndAppVer.SetWindowText(strProductVer);

	m_wndLink.SubclassWindow( GetDlgItem(IDC_LINK) );
	m_wndImage.SetImage( IDB_ABOUT_HEADER );
	m_wndImage.SubclassWindow( GetDlgItem(IDC_IMAGE) );

	CListViewCtrl wndListCtrl;
	CString colName[2];
	colName[0].LoadString(IDS_ABOUTDLG_COL_COMPONENT);
	colName[1].LoadString(IDS_ABOUTDLG_COL_VERSION);

	wndListCtrl.Attach( GetDlgItem( IDC_COMPVER ) );
	wndListCtrl.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);

	wndListCtrl.AddColumn(colName[0], 0);
	wndListCtrl.AddColumn(colName[1], 1);
	CString strAppName;
	strAppName.LoadString(IDS_ABOUTDLG_PRODUCT_NAME);
	wndListCtrl.AddItem(0, 0, strAppName);
	wndListCtrl.AddItem(0, 1, strAppVerStr);

	CRect rcListView;
	wndListCtrl.GetClientRect(rcListView);
	wndListCtrl.SetColumnWidth(0, LVSCW_AUTOSIZE);
	wndListCtrl.SetColumnWidth(1, 
		rcListView.Width() - wndListCtrl.GetColumnWidth(0));

	return TRUE;
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

	XTL::AutoProcessHeap autoPfvi = pfvi;

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
	INT TitleFontSize = 11; //::StrToInt(strFontSize);

	HDC hdc = ::GetDC(NULL);
	TitleLogFont.lfHeight = 0 - 
		::GetDeviceCaps(hdc,LOGPIXELSY) * TitleFontSize / 72;

	hTitleFont = ::CreateFontIndirect(&TitleLogFont);
	::ReleaseDC(NULL, hdc);

	return hTitleFont;
}
