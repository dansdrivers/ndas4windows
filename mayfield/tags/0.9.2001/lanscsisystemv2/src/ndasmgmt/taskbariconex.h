#ifndef _TASKBARICON_EX_H_
#define _TASKBARICON_EX_H_
#include "taskbaricon.h"

#ifndef __cplusplus
#error WTL requires C++ compilation (use a .cpp suffix)
#endif

#ifndef _STRSAFE_H_INCLUDED_
#error TaskBarIconEx.h requires strsafe.h to be included first
#endif

#ifndef __ATLCRACK_H__
#error TaskBarIconEx.h requires atlcrack.h to be included first
#endif

#pragma once

template <typename T>
class CTaskBarIconExT :
	public CTaskBarIconT<T>
{
public:
	BEGIN_MSG_MAP_EX(CTaskBarIconExT<T>)
#if _WIN32_IE >= 0x500
//		MSG_WM_CONTEXTMENU(OnContextMenu2)
#endif
		MESSAGE_HANDLER_EX(WM_TASKBARICON, OnTaskBarNotify)
//		MESSAGE_HANDLER(m_iTaskbarRestartMsg, OnRestart)
		// Added LButtonUp also
		TASKBAR_MESSAGE_HANDLER((*this), WM_LBUTTONUP, OnContextMenu)
		CHAIN_MSG_MAP(CTaskBarIconT<T>)
	END_MSG_MAP()

	void OnContextMenu2(HWND hWnd, CPoint pt)
	{
		ATLTRACE(_T("ContextMenu2\n"));
		if( !::IsMenu(m_hMenu) ) {
			SetMsgHandled(FALSE);
			return;
		}
		HMENU hSubMenu = ::GetSubMenu(m_hMenu, 0);   
		ATLASSERT(::IsMenu(hSubMenu));
		// Make first menu-item the default (bold font)      
		// ::SetMenuDefaultItem(hSubMenu, 0, TRUE); 
		// Display the menu at the current mouse location.
		// POINT pt;
		// ::GetCursorPos(&pt);
		::SetForegroundWindow(m_nid.hWnd);
		::TrackPopupMenu(hSubMenu, 0, pt.x, pt.y, 0, m_nid.hWnd, NULL);
		::PostMessage(m_nid.hWnd, WM_NULL, 0,0); // Fixes Win95 bug
		return;
	}

	BOOL Install(HWND hWnd, UINT iID, HICON hIcon, HMENU hMenu, LPTSTR lpszTip)
	{
		T* pT = static_cast<T*>(this);
		ATLASSERT(::IsWindow(hWnd));
		ATLASSERT(m_hMenu==NULL);
		ATLASSERT(m_nid.hIcon==NULL);
		m_nid.hWnd = hWnd;
		m_nid.uID = iID;
		m_nid.hIcon = hIcon; 
		::lstrcpyn(m_nid.szTip, (lpszTip!=NULL ? lpszTip : _T("")), sizeof(m_nid.szTip)/sizeof(TCHAR)); 
		m_hMenu = hMenu;
		m_nid.uVersion = NOTIFYICON_VERSION;
		::Shell_NotifyIcon(NIM_SETVERSION, &m_nid);
		return pT->AddTaskBarIcon();;
	}

	BOOL Install(HWND hWnd, UINT iID, UINT nRes)
	{
		T* pT = static_cast<T*>(this);
		ATLASSERT(::IsWindow(hWnd));
		ATLASSERT(m_hMenu==NULL);
		ATLASSERT(m_nid.hIcon==NULL);
		m_nid.hWnd = hWnd;
		m_nid.uID = iID;
		m_nid.hIcon = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(nRes), IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
		m_nid.szTip[0] = '\0';
		::LoadString(_Module.GetResourceInstance(), nRes, m_nid.szTip, sizeof(m_nid.szTip)/sizeof(TCHAR));
		m_hMenu = ::LoadMenu(_Module.GetResourceInstance(), MAKEINTRESOURCE(nRes));
		m_nid.uVersion = NOTIFYICON_VERSION;
		::Shell_NotifyIcon(NIM_SETVERSION, &m_nid);
		return pT->AddTaskBarIcon();
	}
#if (_WIN32_IE < 0x0501)
#define NIN_BALLOONSHOW     (WM_USER + 2)
#define NIN_BALLOONHIDE     (WM_USER + 3)
#define NIN_BALLOONTIMEOUT  (WM_USER + 4)
#define NIN_BALLOONUSERCLICK (WM_USER + 5)
#endif

	LRESULT OnTaskBarNotify(UINT /* uMsg */, WPARAM wParam, LPARAM lParam)
	{
		switch (lParam) {
		case NIN_BALLOONSHOW: 
			ATLTRACE(_T("NIN_BALLOONSHOW\n")); break;
		case NIN_BALLOONHIDE: 
			ATLTRACE(_T("NIN_BALLOONHIDE\n")); break;
		case NIN_BALLOONTIMEOUT: 
			ATLTRACE(_T("NIN_BALLOONTIMEOUT\n")); break;
		case NIN_BALLOONUSERCLICK: 
			ATLTRACE(_T("NIN_BALLOONUSERCLICK\n")); break;
		case NIN_SELECT:
			ATLTRACE(_T("NIN_SELECT\n"));
		case NIN_KEYSELECT:
			ATLTRACE(_T("NIN_KEYSELECT\n"));
			{
				BOOL bHandled = TRUE;
				OnContextMenu(0, bHandled);
				SetMsgHandled(bHandled);
			}
			break;
		default:
			SetMsgHandled(FALSE);
			break;
		}
		return 0;
	}

	BOOL SetToolTipText(UINT nRes)
	{
		ATLASSERT(::IsWindow(m_nid.hWnd));
		int iChars = ::LoadString(_Module.GetResourceInstance(), 
			nRes, m_nid.szTip, sizeof(m_nid.szTip)/sizeof(TCHAR));
		if (0 == iChars) {
			return FALSE;
		}
		m_nid.uFlags = NIF_TIP;
		return Shell_NotifyIcon(NIM_MODIFY, &m_nid);
	}

	BOOL SetToolTipText(LPCTSTR szTooltip)
	{
		ATLASSERT(::IsWindow(m_nid.hWnd));
		m_nid.uFlags = NIF_TIP;
		HRESULT hr = ::StringCchCopy(
			m_nid.szTip, sizeof(m_nid.szTip)/sizeof(TCHAR), szTooltip);
		ATLASSERT(SUCCEEDED(hr) || hr == STRSAFE_E_INSUFFICIENT_BUFFER);
		return Shell_NotifyIcon(NIM_MODIFY, &m_nid);
	}

	LPCTSTR GetToolTipText()
	{
		return m_nid.szTip;
	}

	BOOL ShowBalloonToolTip(
		UINT nResInfo, 
		UINT nResInfoTitle = 0, 
		UINT uTimeout = 5000, 
		DWORD dwInfoFlags = 0)
	{
		m_nid.uFlags = NIF_INFO;

		int iChars = ::LoadString(_Module.GetResourceInstance(), 
			nResInfo, m_nid.szInfo, sizeof(m_nid.szInfo)/sizeof(TCHAR));
		if (0 == iChars) { return FALSE; }

		if (nResInfoTitle == 0) {
			m_nid.szInfoTitle[0] = _T('\0');
		} else {
			int iChars = ::LoadString(_Module.GetResourceInstance(), 
				nResInfoTitle, m_nid.szInfoTitle, sizeof(m_nid.szInfoTitle)/sizeof(TCHAR));
			if (0 == iChars) { return FALSE; }
		} 

		m_nid.uTimeout = uTimeout;
		m_nid.dwInfoFlags = dwInfoFlags;
		return Shell_NotifyIcon(NIM_MODIFY, &m_nid);
	}

	BOOL ShowBalloonToolTip(
		LPCTSTR szInfo, 
		LPCTSTR szInfoTitle = 0, 
		UINT uTimeout = 5000, 
		DWORD dwInfoFlags = 0)
	{
		m_nid.uFlags = NIF_INFO;
		HRESULT hr = ::StringCchCopy(
			m_nid.szInfo, sizeof(m_nid.szInfo)/sizeof(TCHAR), szInfo);
		ATLASSERT(SUCCEEDED(hr) || hr == STRSAFE_E_INSUFFICIENT_BUFFER);
		m_nid.uTimeout = uTimeout;
		hr = ::StringCchCopy(
			m_nid.szInfoTitle, sizeof(m_nid.szInfoTitle)/sizeof(TCHAR), szInfoTitle);
		m_nid.dwInfoFlags = dwInfoFlags;
		return Shell_NotifyIcon(NIM_MODIFY, &m_nid);
	}

};

class CTaskBarIconEx : public CTaskBarIconExT<CTaskBarIconEx>
{
};

#endif