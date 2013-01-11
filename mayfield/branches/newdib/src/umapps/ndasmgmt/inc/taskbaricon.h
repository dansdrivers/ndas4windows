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
// Modified by Chesong Lee <cslee@ximeta.com>
//
// FIX: Changed to handle WM_RBUTTONUP message instead of WM_RBUTTONDOWN
//

#pragma once

#ifndef __cplusplus
   #error WTL requires C++ compilation (use a .cpp suffix)
#endif

#ifndef __ATLWIN_H__
   #error TaskBarIcon.h requires atlwin.h to be included first
#endif

#if (_WIN32_IE < 0x0400)
   #error TaskBarIcon.h requires _WIN32_IE >= 0x0400
#endif


#ifndef WM_TASKBARICON
#define WM_TASKBARICON WM_APP+423
#endif

#define TASKBAR_MESSAGE_HANDLER(ti, msg, func) \
   if(uMsg==ti.m_nid.uCallbackMessage && wParam==ti.m_nid.uID && lParam==msg) \
   { \
      bHandled = TRUE; \
      lResult = func(lParam, bHandled); \
      if(bHandled) \
         return TRUE; \
   }

template <class T>
class CTaskBarIconT
{
public:
   NOTIFYICONDATA m_nid; 
   HMENU m_hMenu;
   UINT m_iTaskbarRestartMsg;

   BEGIN_MSG_MAP(CTaskBarIconT<T>)
	   MESSAGE_HANDLER(m_iTaskbarRestartMsg, OnRestart)
	   // WM_RBUTTONDOWN may create TaskBar's own popup menu
	   //	  TASKBAR_MESSAGE_HANDLER((*this), WM_RBUTTONDOWN, OnContextMenu)
	   TASKBAR_MESSAGE_HANDLER((*this), WM_RBUTTONUP, OnContextMenu)
   END_MSG_MAP()

   CTaskBarIconT() : m_hMenu(NULL), m_iTaskbarRestartMsg(0)
   {
		T* pT = static_cast<T*>(this);
	    ::ZeroMemory(&m_nid, sizeof(m_nid));
		m_nid.cbSize = sizeof(m_nid); 
		m_nid.uCallbackMessage = WM_TASKBARICON;
		m_iTaskbarRestartMsg = ::RegisterWindowMessage(TEXT("TaskbarCreated"));
   }
   ~CTaskBarIconT()
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
		::lstrcpyn(m_nid.szTip, (lpszTip!=NULL ? lpszTip : _T("")), sizeof(m_nid.szTip)/sizeof(TCHAR)); 
		m_hMenu = hMenu;
		return pT->AddTaskBarIcon();
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
		return pT->AddTaskBarIcon();
   }
   BOOL Uninstall()
   {
		T* pT = static_cast<T*>(this);
		BOOL res = TRUE;
		if( m_nid.hWnd!=NULL ) res = pT->DeleteTaskBarIcon();
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
   BOOL AddTaskBarIcon()
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
   BOOL DeleteTaskBarIcon() 
   { 
		return ::Shell_NotifyIcon(NIM_DELETE, &m_nid); 
   } 

   // Message handlers

	LRESULT OnRestart(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
	{
		T* pT = static_cast<T*>(this);
		pT->AddTaskBarIcon();
		bHandled = FALSE;
		return 0;
	}

	LRESULT OnContextMenu(LPARAM /*uMsg*/, BOOL& bHandled)
	{
		if( !::IsMenu(m_hMenu) ) {
			bHandled = FALSE;
			return 0;
		}
		HMENU hSubMenu = ::GetSubMenu(m_hMenu, 0);   
		ATLASSERT(::IsMenu(hSubMenu));
		// Make first menu-item the default (bold font)      
		// ::SetMenuDefaultItem(hSubMenu, 0, TRUE); 
		// Display the menu at the current mouse location.
		POINT pt;
		::GetCursorPos(&pt);      
		::SetForegroundWindow(m_nid.hWnd);
		::TrackPopupMenu(hSubMenu, 0, pt.x, pt.y, 0, m_nid.hWnd, NULL);
		::PostMessage(m_nid.hWnd, WM_NULL, 0,0); // Fixes Win95 bug
		return 0;
	}
};

class CTaskBarIcon : public CTaskBarIconT<CTaskBarIcon>
{
};


#endif // __TASKBARICON_H__
