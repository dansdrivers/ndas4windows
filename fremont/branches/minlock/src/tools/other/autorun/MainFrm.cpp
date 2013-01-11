// MainFrm.cpp : implmentation of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "AutoRunView.h"
#include "MainFrm.h"
#include <strsafe.h>

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg)
{
	if(WindowImplBase::PreTranslateMessage(pMsg))
		return TRUE;

	return m_view.PreTranslateMessage(pMsg);
}

BOOL CMainFrame::OnIdle()
{
	return FALSE;
}

extern HWND hWndMainFrame;

LRESULT CMainFrame::OnCreate(LPCREATESTRUCT lpcs)
{
	hWndMainFrame = this->m_hWnd;

	TCHAR moduleDir[MAX_PATH] = {0};
	TCHAR fileurl[MAX_PATH] = _T("file://");
	LPTSTR pathpart = fileurl + ::lstrlen(fileurl);
	DWORD pathpartlen = MAX_PATH - ::lstrlen(fileurl);

	// Get the autorun.exe's path
	ATLENSURE( ::GetModuleFileName(NULL, moduleDir, MAX_PATH) );
	::PathRemoveFileSpec(moduleDir);

	TCHAR autoRunInfPath[MAX_PATH] = {0};
	ATLENSURE_SUCCEEDED( ::StringCchCopy(autoRunInfPath, MAX_PATH, moduleDir) );
	::PathAppend(autoRunInfPath, _T("AUTORUN.INF"));

	// Read [HTML] HTML=...
	TCHAR home[MAX_PATH] = {0};
	GetPrivateProfileString(_T("HTML"), _T("HOME"), _T(""),  home, MAX_PATH, autoRunInfPath);
	if (::lstrlen(home) == 0)
	{
		PostMessage(WM_CLOSE);
		::ShellExecute(NULL, _T("open"), _T("."), NULL, NULL, SW_SHOW);
		return TRUE;
	}


	LPCTSTR url = _T("");
	if (home[0] == _T('r') && 
		home[1] == _T('e') && 
		home[2] == _T('s') && 
		home[3] == _T(':') &&
		home[4] == _T('/') &&
		home[5] == _T('/'))
	{
		url = home;
	}
	else
	{
		::StringCchCopy(pathpart, pathpartlen, moduleDir);
		::PathAppend(pathpart, home);
		url = fileurl;
	}

	//TODO: Replace with a URL of your choice
	m_hWndClient = m_view.Create(
		m_hWnd, 
		rcDefault, 
		_T(""), 
		WS_BORDER | WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 
		0,
		0U, 
		const_cast<LPTSTR>(url));

	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	int width = GetPrivateProfileInt(_T("HTML"), _T("WIDTH"), 600, autoRunInfPath);
	int height = GetPrivateProfileInt(_T("HTML"), _T("HEIGHT"), 450, autoRunInfPath);

	if (width < 1) width = 600;
	if (height < 1) height = 450;

	ResizeClient(width, height);

	CenterWindow();

	return 0;
}

LRESULT CMainFrame::OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	PostMessage(WM_CLOSE);
	return 0;
}

void CMainFrame::OnClose()
{
	DestroyWindow();
}

void CMainFrame::OnDestroy()
{
	PostQuitMessage(0);
}