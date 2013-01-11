#pragma once

class CMyTaskBarIcon : 
	public CTaskBarIconExT<CMyTaskBarIcon>
{
public:
	CCommandBarXPCtrl m_xpCmdBar;
	HWND m_hWndActiveDialog;
	// CCommandBarCtrl m_xpCmdBar;

	BEGIN_MSG_MAP(CMyTaskBarIcon)
		TASKBAR_MESSAGE_HANDLER((*this), WM_RBUTTONUP, OnContextMenu)
		CHAIN_MSG_MAP(CTaskBarIconExT<CMyTaskBarIcon>)
	END_MSG_MAP()

	CMyTaskBarIcon()
	{
	}

	HWND SetActiveDialog(HWND hWnd)
	{
		HWND hWndOld = m_hWndActiveDialog;
		m_hWndActiveDialog = hWnd;
		return hWndOld;
	}

	BOOL AddTaskBarIcon()
	{
		HWND hWndCmdBar = m_xpCmdBar.Create(m_nid.hWnd, CRect(0,0,0,0), NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE);
		m_xpCmdBar.Prepare();
		m_xpCmdBar.AttachMenu(m_hMenu);
		return CTaskBarIconExT<CMyTaskBarIcon>::AddTaskBarIcon();
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
		//
		// If there are any active dialog, 
		// do not show the menu but just pop it up
		//
		if (NULL != m_hWndActiveDialog && ::IsWindow(m_hWndActiveDialog)) {
			if (!::IsWindow(m_hWndActiveDialog)) {
				m_hWndActiveDialog = NULL;
			}
			SetForegroundWindow(m_hWndActiveDialog);
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


		return CTaskBarIconExT<CMyTaskBarIcon>::OnContextMenu(uMsg, bHandled);
	}

};
