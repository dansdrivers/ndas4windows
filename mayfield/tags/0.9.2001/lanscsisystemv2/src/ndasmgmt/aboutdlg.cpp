#include "stdafx.h"
#include "resource.h"
#include <atlctrls.h>
#include <atlctrlx.h>

#include <shellapi.h>
#include <winver.h>

#include "aboutdlg.h"

#if 0
struct CCompVer
{
	typedef enum _COMP_LOCATION {
		CMPL_LOADLIBRARY,
		CMPL_DRIVERPATH,
		CMPL_SERVICEENTRY,
		CMPL_USERFN
	} COMP_LOCATION;
	
	struct COMP_DATA {
		COMP_LOCATION cloc;
		WORD wCompTitleId;
		LPCTSTR szModuleName;

		BOOL LocateFile(CString& szFilePath)
		{
			switch (this->cloc) {
			case CMPL_LOADLIBRARY:
				return LocateFileFromSearchPath(this->szModuleName, szFilePath);
			case CMPL_DRIVERPATH:
				if (0 < ::GetSystemDirectory(szFilePath.GetBuffer(_MAX_PATH),_MAX_PATH))
					return TRUE;
				else
					return FALSE;
			}
		}

		
		BOOL GetModuleVerInfoString(LPCTSTR szSubBlock, CString& szData)
		{
			CString szFilePath;
			return LocateFile(this, szFilePath) &&
				GetFileVerInfoString(szFilePath, szSubBlock, szData);
		}

	};

	typedef COMP_DATA* PCOMP_DATA;

	static COMP_DATA cmps[];

	static BOOL LocateFileFromSearchPath(LPCTSTR szModuleName, CString& szFilePath)
	{
		PTCHAR lpFilePart;
		return ::SearchPath(
			NULL, 
			szModuleName, 
			NULL, 
			_MAX_PATH, 
			szFilePath.GetBuffer(_MAX_PATH), &lpFilePart);
	}

	static BOOL GetFileVerInfoString(LPCTSTR szFilePath, LPCTSTR szSubBlock, CString& szData)
	{
		DWORD dwHandle, dwSize;
		BOOL fSuccess(FALSE);

		dwSize = ::GetFileVersionInfoSize(szFilePath, &dwHandle);
		if (0 == dwSize)
			return FALSE;

		LPVOID lpData = new BYTE[dwSize];
		if (NULL == lpData)
			return FALSE;

		fSuccess = ::GetFileVersionInfo(szFilePath, dwHandle, dwSize, lpData);
		if (!fSuccess)
			return FALSE;

		LPVOID lpValue;
		UINT uiValueLen;
		fSuccess = ::VerQueryValue(lpData, (LPTSTR)szSubBlock, &lpValue, &uiValueLen);
		if (!fSuccess)
			return FALSE;

		szData = (LPCTSTR) lpValue;
		return TRUE;
	}

	static BOOL GetModuleVerInfoString(LPCTSTR szModuleName, LPCTSTR szSubBlock, CString& szData)
	{
		CString szFilePath;
		return LocateFileFromSearchPath(szModuleName, szFilePath) &&
			GetFileVerInfoString(szFilePath, szSubBlock, szData);
	}

};

#define ID_COMP_NDASMGMT 501
#define ID_COMP_LPX	502
#define ID_COMP_NDASHLPSVC 503
#define ID_COMP_NDASBUSENUM 504
#define ID_COMP_NDASFSFLT 505
#define ID_COMP_NDASPORT 506
#define ID_COMP_NDASUIRES 507

CCompVer::COMP_DATA CCompVer::cmps[] = {
	{ CMPL_LOADLIBRARY, ID_COMP_NDASMGMT, TEXT("ndasmgmt.exe")},
	{ CMPL_DRIVERPATH, ID_COMP_LPX, TEXT("lpx.sys")},
	{ CMPL_SERVICEENTRY, ID_COMP_NDASHLPSVC, TEXT("ndashlpsvc.exe")}
};
#endif

LRESULT 
CAboutDialog::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	CenterWindow(GetParent());

	m_wndHyperLink.SubclassWindow(GetDlgItem(IDC_LINK));

	CListViewCtrl wndListView(GetDlgItem(IDC_COMPVER));

	wndListView.AddColumn(TEXT("Component"), 0);
	wndListView.AddColumn(TEXT("Version"), 1);
	wndListView.SetColumnWidth(0, 200);
	wndListView.SetColumnWidth(1, 100);

	CString szVer(TEXT("2.10.0 QFE 1"));

	CStatic wndAppName;
	wndAppName.Attach(GetDlgItem(IDC_APPNAME));
//	CFont hFont;
//	hFont.Attach(wndAppName.GetFont());
//	LOGFONT logFont;
//	hFont.GetLogFont(&logFont);
//	logFont.lfHeight = -MulDiv(14, GetDeviceCaps(CWindowDC(m_hWnd), LOGPIXELSY), 72);
//	hFont.GetLogFont(&logFont);
//	hFont.Detach();
//	wndAppName.SetFont(hFont);

	wndListView.AddItem(0, 0, TEXT("NDAS Device Management"));
	wndListView.SetItemText(0, 1, szVer);

	CString szAppVer;
	TCHAR szAppVerFmt[256];

	GetDlgItemText(IDC_APPVER, szAppVerFmt, 256);
	szAppVer.Format(szAppVerFmt, szVer);
	SetDlgItemText(IDC_APPVER, szAppVer);

//	m_dibHeader.LoadBitmap(IDB_HEADER);

//	m_bmpHeader.LoadBitmap(IDB_HEADER);

	m_pix.LoadFromResource(
		_Module.GetResourceInstance(), 
		IDB_ABOUT_HEADER, 
		_T("IMAGE"));

	return TRUE;
}

void CAboutDialog::OnPaint(HDC hDC)
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
