#pragma once

class CSingleInstApp
{
protected:

	static BOOL IsFirstInstance(LPCTSTR lpszUID);
	static BOOL EnableTokenPrivilege(
		LPCTSTR lpszSystemName, 
		BOOL    bEnable = TRUE);

public:

	static UINT uMsgCheckInst;
	static TCHAR szUID[128];

	static UINT GetInstanceMessageID();
	static BOOL PostInstanceMesage(WPARAM wParam, LPARAM lParam);
	static BOOL InitInstance(LPCTSTR lpszUID);
	static BOOL WaitInstance(LPCTSTR lpszUID, DWORD dwTimeout = INFINITE);
};

template <typename T>
class ATL_NO_VTABLE CSingleInstWnd :
	public CMessageMap
{
public:

	UINT	m_uMsgCheckInst;

	BEGIN_MSG_MAP_EX(CSingleInstWnd<T>)
		MESSAGE_HANDLER_EX(m_uMsgCheckInst, OnMsgCheckInst)
	END_MSG_MAP()

	CSingleInstWnd() :
		m_uMsgCheckInst(CSingleInstApp::GetInstanceMessageID())
	{}

	LRESULT OnMsgCheckInst(UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		T* pThis = static_cast<T*>(this);
		pThis->OnAnotherInstanceMessage(wParam, lParam);
		return 0;
	}

	void OnAnotherInstanceMessage(WPARAM wParam, LPARAM lParam)
	{
		ATLTRACE(_T("Another Instance Message: wParam=%d, lParam=%d\n"), wParam, lParam);
	}

#if 0
		// Get command line arguments (if any) from new instance.
		BOOL bShellOpen = FALSE;

		if( pMsg->wParam != NULL ) 
		{
			::GlobalGetAtomName( (ATOM)pMsg->wParam, m_lpCmdLine, _MAX_FNAME );			
			::GlobalDeleteAtom(  (ATOM)pMsg->wParam );		
			bShellOpen = TRUE;		
		}


				// Does the main window have any popups ? If has, 
				// bring the main window or its popup to the top
				// before showing:

				CWindow pPopupWnd = m_pMainWnd->GetLastActivePopup();
				pPopupWnd->BringWindowToTop();

				// If window is not visible then show it, else if
				// it is iconic, restore it:

				if( !m_pMainWnd->IsWindowVisible() )
					m_pMainWnd->ShowWindow( SW_SHOWNORMAL ); 
				else if( m_pMainWnd->IsIconic() )
					m_pMainWnd->ShowWindow( SW_RESTORE );

				// And finally, bring to top after showing again:

				pPopupWnd->BringWindowToTop();
				pPopupWnd->SetForegroundWindow(); 
			}
		}

#endif

};

