#pragma once

class CMyTaskBarIcon : 
	public CTaskBarIconT<CMyTaskBarIcon>
{
	CCommandBarXPCtrl m_xpCmdBar;
	// CCommandBarCtrl m_xpCmdBar;

	BEGIN_MSG_MAP(CMyTaskBarIcon)
		TASKBAR_MESSAGE_HANDLER((*this), WM_RBUTTONUP, OnContextMenu)
	END_MSG_MAP()

	CMyTaskBarIcon()
	{
	}

	BOOL AddTaskBarIcon()
	{
		HWND hWndCmdBar = m_xpCmdBar.Create(m_nid.hWnd, CRect(0,0,0,0), NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE);
		m_xpCmdBar.Prepare();
		m_xpCmdBar.AttachMenu(m_hMenu);
		return CTaskBarIconT<CMyTaskBarIcon>::AddTaskBarIcon();
	}

	LRESULT OnContextMenu(LPARAM uMsg, BOOL& bHandled)
	{
		//		HWND hWndCmdBar = m_xpCmdBar.Create(m_nid.hWnd, CRect(0,0,0,0), NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE);
		//		m_xpCmdBar.Prepare();
		// Attach menu
		//		m_xpCmdBar.AttachMenu(m_hMenu);
		// Load command bar images
		//		m_xpCmdBar.LoadImages(IDR_MAINFRAME);
		// Remove old menu
		//		SetMenu(NULL);

		if( !::IsMenu(m_hMenu) ) {
			bHandled = FALSE;
			return 0;
		}
#if 1
		HMENU hSubMenu = ::GetSubMenu(m_hMenu, 0);   
		ATLASSERT(::IsMenu(hSubMenu));
		// Make first menu-item the default (bold font)      
		// ::SetMenuDefaultItem(hSubMenu, 0, TRUE); 
		// Display the menu at the current mouse location.
		POINT pt;
		::GetCursorPos(&pt);      
		::SetForegroundWindow(m_nid.hWnd);
		m_xpCmdBar.DoTrackPopupMenu(hSubMenu,0,pt.x,pt.y);
		::PostMessage(m_nid.hWnd, WM_NULL, 0,0); // Fixes Win95 bug
#else
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
#endif
		return 0;


		return CTaskBarIconT<CMyTaskBarIcon>::OnContextMenu(uMsg, bHandled);
	}

};
