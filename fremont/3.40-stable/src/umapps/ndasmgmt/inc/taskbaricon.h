#ifndef __TASKBARICON_H__
#define __TASKBARICON_H__

/////////////////////////////////////////////////////////////////////////////
// Task Bar Icon class
//
// Written by Bjarke Viksoe (bjarke@viksoe.dk)
// Copyright (c) 2001 Bjarke Viksoe.
//
// This code may be used in compiled form in any way you desire. This
// file may be redistributed by any means PROVIDING it is 
// not sold for profit without the authors written consent, and 
// providing that this notice and the authors name is included. 
//
// This file is provided "as is" with no expressed or implied warranty.
// The author accepts no liability if it causes any damage to you or your
// computer whatsoever. It's free, so don't hassle me about it.
//
// Beware of bugs.
//

// 
// Modified and Enhanced by Chesong Lee <cslee@ximeta.com>
//
//

#pragma once

#ifndef __cplusplus
   #error WTL requires C++ compilation (use a .cpp suffix)
#endif

#ifndef __ATLWIN_H__
   #error TaskbarIcon.h requires atlwin.h to be included first
#endif

#if (_WIN32_IE < 0x0400)
   #error TaskbarIcon.h requires _WIN32_IE >= 0x0400
#endif


#define TASKBAR_MESSAGE_HANDLER(ti, msg, func) \
   if(uMsg==ti.m_nid.uCallbackMessage && wParam==ti.m_nid.uID && lParam==msg) \
   { \
      bHandled = TRUE; \
      lResult = func(lParam, bHandled); \
      if(bHandled) \
         return TRUE; \
   }
// atlcrack support
#define TASKBAR_MESSAGE_HANDLER_EX(ti, msg, func) \
	if(uMsg==ti.m_nid.uCallbackMessage && wParam==ti.m_nid.uID && lParam==msg) \
	{ \
		SetMsgHandled(TRUE); \
		lResult = 0; \
		func((UINT)lParam); \
		if(IsMsgHandled()) \
			return TRUE; \
	}

#define WM_TASKBARICON_GUID "WM_TASKBARICON_{32AEC36C-6521-4c9c-B063-12EB269C3296}"

template <class T>
class CTaskbarIconT
{
public:
	const UINT WM_TASKBARICON;
	const UINT WM_TASKBAR_RESTART;
	HMENU m_hMenu;
	NOTIFYICONDATA m_nid; 

	CTaskbarIconT() : 
		WM_TASKBARICON(::RegisterWindowMessage(_T(WM_TASKBARICON_GUID))),
		WM_TASKBAR_RESTART(::RegisterWindowMessage(_T("TaskbarCreated"))),
		m_hMenu(NULL)
   {
	    ::ZeroMemory(&m_nid, sizeof(m_nid));
		m_nid.cbSize = sizeof(m_nid); 
		m_nid.uCallbackMessage = WM_TASKBARICON;
   }

	~CTaskbarIconT()
	{
		T* pT = static_cast<T*>(this);
		pT->Uninstall();
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
		HRESULT hr = ::StringCbCopy(m_nid.szTip, sizeof(m_nid.szTip), lpszTip); 
		ATLASSERT(SUCCEEDED(hr));
		m_hMenu = hMenu;
		return pT->AddTaskbarIcon();
	}

	BOOL Install(HWND hWnd, UINT iID, UINT nRes)
	{
	   T* pT = static_cast<T*>(this);
		ATLASSERT(::IsWindow(hWnd));
		ATLASSERT(m_hMenu==NULL);
		ATLASSERT(m_nid.hIcon==NULL);
		m_nid.hWnd = hWnd;
		m_nid.uID = iID;
#if (_ATL_VER >= 0x0700)
		m_nid.hIcon = (HICON)::LoadImage(ATL::_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE(nRes), IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
#else //!(_ATL_VER >= 0x0700)
		m_nid.hIcon = (HICON)::LoadImage(ATL::_pModule->GetResourceInstance(), MAKEINTRESOURCE(nRes), IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
#endif //!(_ATL_VER >= 0x0700)
		m_nid.szTip[0] = '\0';
#if (_ATL_VER >= 0x0700)
		int n = ::LoadString(ATL::_AtlBaseModule.GetResourceInstance(), nRes, m_nid.szTip, sizeof(m_nid.szTip)/sizeof(TCHAR));
#else //!(_ATL_VER >= 0x0700)
		int n = ::LoadString(ATL::_pModule->GetResourceInstance(), nRes, m_nid.szTip, sizeof(m_nid.szTip)/sizeof(TCHAR));
#endif //!(_ATL_VER >= 0x0700)
		if (0 == n)
		{
			ATLTRACE2(atlTraceUI, 1, "Loading Text Resource (%d) failed at TaskbarIcon::Install", nRes);
		}
		m_hMenu = ::LoadMenu(_Module.GetResourceInstance(), MAKEINTRESOURCE(nRes));
		if (NULL == m_hMenu)
		{
			ATLTRACE2(atlTraceUI, 1, "Loading Menu Resource (%d) failed at TaskbarIcon::Install", nRes);
		}
		return pT->AddTaskbarIcon();
	}

	BOOL Uninstall()
	{
		T* pT = static_cast<T*>(this);
		BOOL res = TRUE;
		if( m_nid.hWnd!=NULL ) res = pT->DeleteTaskbarIcon();
		m_nid.hWnd=NULL;
		if( m_nid.hIcon ) ::DestroyIcon(m_nid.hIcon);
		m_nid.hIcon = NULL;
		if( m_hMenu ) ::DestroyMenu(m_hMenu);
		m_hMenu = NULL;
		return res;
	}

	BOOL IsInstalled() const
	{
		return m_nid.hWnd!=NULL;
	}

	BOOL SetFocus()
	{
		ATLASSERT(::IsWindow(m_nid.hWnd));
		m_nid.uFlags = 0;
		BOOL res = ::Shell_NotifyIcon(NIM_SETFOCUS, &m_nid);
		return res;
	}
	void SetIcon(HICON hIcon) 
	{ 
		ATLASSERT(m_nid.hIcon==NULL);
		m_nid.hIcon = hIcon; 
	}

	void SetMenu(HMENU hMenu) 
	{ 
		ATLASSERT(m_hMenu==NULL);
		ATLASSERT(::IsMenu(hMenu));
		m_hMenu = hMenu; 
	}

	BOOL AddTaskbarIcon()
	{
		ATLASSERT(::IsWindow(m_nid.hWnd));
		m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP; 
		BOOL res = ::Shell_NotifyIcon(NIM_ADD, &m_nid); 
		return res;
	}

	BOOL ChangeIcon(HICON hIcon)
	{
		// NOTE: The class takes ownership of the icon!
		ATLASSERT(::IsWindow(m_nid.hWnd));
		if( m_nid.hIcon ) ::DestroyIcon(m_nid.hIcon);
		m_nid.uFlags = NIF_ICON; 
		m_nid.hIcon = hIcon;
		BOOL res = ::Shell_NotifyIcon(NIM_MODIFY, &m_nid); 
		return res;
	}

	BOOL DeleteTaskbarIcon() 
	{ 
		return ::Shell_NotifyIcon(NIM_DELETE, &m_nid); 
	} 

	// Shell 5.0 ToolTip and Balloon ToolTip Support
	BOOL SetToolTipText(ATL::_U_STRINGorID message)
	{
		ATLASSERT(::IsWindow(m_nid.hWnd));
		if (IS_INTRESOURCE(message.m_lpstr) && LOWORD(message.m_lpstr) != 0)
		{
#if (_ATL_VER >= 0x0700)
			int nRes = ::LoadString(ATL::_AtlBaseModule.GetResourceInstance(), LOWORD(message.m_lpstr), m_nid.szTip, sizeof(m_nid.szTip)/sizeof(TCHAR));
#else //!(_ATL_VER >= 0x0700)
			int nRes = ::LoadString(ATL::_pModule->GetResourceInstance(), LOWORD(message.m_lpstr), m_nid.szTip, sizeof(m_nid.szTip)/sizeof(TCHAR));
#endif //!(_ATL_VER >= 0x0700)
			if (0 == nRes) 
			{
				ATLTRACE2(atlTraceUI, 1, "Loading Message Resource (%d) failed at SetToolTipText", LOWORD(message.m_lpstr));
			}
		}
		else
		{
			HRESULT hr = ::StringCbCopy(m_nid.szTip, sizeof(m_nid.szTip), message.m_lpstr);
			ATLASSERT(SUCCEEDED(hr) || hr == STRSAFE_E_INSUFFICIENT_BUFFER);
		}
		m_nid.uFlags = NIF_TIP;
		return Shell_NotifyIcon(NIM_MODIFY, &m_nid);
	}

	BOOL ShowBalloonToolTip(
		ATL::_U_STRINGorID message,
		ATL::_U_STRINGorID title,
		DWORD dwInfoFlags = NIIF_NONE,
		UINT uTimeout = 5000)
	{
		ATLASSERT(::IsWindow(m_nid.hWnd));

		// Set Message Text
		if (IS_INTRESOURCE(message.m_lpstr) && LOWORD(message.m_lpstr) != 0)
		{
#if (_ATL_VER >= 0x0700)
			int nRes = ::LoadString(ATL::_AtlBaseModule.GetResourceInstance(), LOWORD(message.m_lpstr), m_nid.szInfo, sizeof(m_nid.szInfo)/sizeof(TCHAR));
#else //!(_ATL_VER >= 0x0700)
			int nRes = ::LoadString(ATL::_pModule->GetResourceInstance(), LOWORD(message.m_lpstr), m_nid.szInfo, sizeof(m_nid.szInfo)/sizeof(TCHAR));
#endif //!(_ATL_VER >= 0x0700)
			if (0 == nRes) 
			{
				return FALSE;
			}
		}
		else
		{
			HRESULT hr = ::StringCbCopy(m_nid.szInfo, sizeof(m_nid.szInfo), message.m_lpstr);
			ATLASSERT(SUCCEEDED(hr) || hr == STRSAFE_E_INSUFFICIENT_BUFFER);
		}

		// Set Title Text
		if (IS_INTRESOURCE(title.m_lpstr) && LOWORD(title.m_lpstr) != 0)
		{
#if (_ATL_VER >= 0x0700)
			int nRes = ::LoadString(ATL::_AtlBaseModule.GetResourceInstance(), LOWORD(title.m_lpstr), m_nid.szInfoTitle, sizeof(m_nid.szInfoTitle)/sizeof(TCHAR));
#else //!(_ATL_VER >= 0x0700)
			int nRes = ::LoadString(ATL::_pModule->GetResourceInstance(), LOWORD(title.m_lpstr), m_nid.szInfoTitle, sizeof(m_nid.szInfoTitle)/sizeof(TCHAR));
#endif //!(_ATL_VER >= 0x0700)
			if (0 == nRes) 
			{
				return FALSE;
			}
		}
		else
		{
			HRESULT hr = ::StringCbCopy(m_nid.szInfoTitle, sizeof(m_nid.szInfoTitle), message.m_lpstr);
			ATLASSERT(SUCCEEDED(hr) || hr == STRSAFE_E_INSUFFICIENT_BUFFER);
		}

		m_nid.uFlags = NIF_INFO;
		m_nid.uTimeout = uTimeout;
		m_nid.dwInfoFlags = dwInfoFlags;
		return Shell_NotifyIcon(NIM_MODIFY, &m_nid);
	}

};

template <class T, class TBase = CTaskbarIconT<T> > class CTaskbarIconImpl;

template <typename T, class TBase /* = CTaskbarIcon<T>*/ >
class ATL_NO_VTABLE CTaskbarIconImpl : public TBase
{
public:
	BEGIN_MSG_MAP(CTaskbarIconImpl)
		MESSAGE_HANDLER(WM_TASKBAR_RESTART, OnRestart)
		TASKBAR_MESSAGE_HANDLER((*this), WM_RBUTTONDOWN, OnTaskbarContextMenu)
	END_MSG_MAP()

	// Message handlers
	LRESULT OnRestart(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		T* pT = static_cast<T*>(this);
		pT->AddTaskbarIcon();
		bHandled = FALSE;
		return 0;
	}

	LRESULT OnTaskbarContextMenu(LPARAM /*uMsg*/, BOOL& bHandled)
	{
		if( !::IsMenu(m_hMenu) ) 
		{
			bHandled = FALSE;
			return 0;
		}
		HMENU hSubMenu = ::GetSubMenu(m_hMenu, 0);   
		ATLASSERT(::IsMenu(hSubMenu));
		// Make first menu-item the default (bold font)      
		::SetMenuDefaultItem(hSubMenu, 0, TRUE);
		// Display the menu at the current mouse location.
		POINT pt;
		::GetCursorPos(&pt);      
		::SetForegroundWindow(m_nid.hWnd);
		::TrackPopupMenu(hSubMenu, 0, pt.x, pt.y, 0, m_nid.hWnd, NULL);
		::PostMessage(m_nid.hWnd, WM_NULL, 0,0); // Fixes Win95 bug
		return 0;
	}
};

class CTaskbarIcon : public CTaskbarIconImpl<CTaskbarIcon>
{
};

#endif // __TASKBARICON_H__
