#include "stdafx.h"
#include "singleinst.h"

namespace
{
	BOOL EnableTokenPrivilege(LPCTSTR lpszSystemName, BOOL bEnable = TRUE);
}

BOOL 
CInterAppMessenger::Initialize(LPCTSTR ApplicationId)
{
	// If two different applications register the same message string, 
	// the applications return the same message value. The message 
	// remains registered until the session ends. If the message is 
	// successfully registered, the return value is a message identifier 
	// in the range 0xC000 through 0xFFFF.
	ATLASSERT(0 == m_AppMsgId); // Only once
	m_AppMsgId = ::RegisterWindowMessage(ApplicationId);
	ATLASSERT(m_AppMsgId >= 0xC000 && m_AppMsgId <= 0xFFFF);
	return m_AppMsgId != 0;
}

BOOL 
CInterAppMessenger::PostMessage(WPARAM wParam, LPARAM lParam)
{
	// One process can terminate the other process by broadcasting 
	// the private message using the BroadcastSystemMessage function 
	// as follows:

	DWORD dwReceipents = BSM_APPLICATIONS;

	if (EnableTokenPrivilege(SE_TCB_NAME))
	{
		dwReceipents |= BSM_ALLDESKTOPS;
	}

	// Send the message to all other instances.
	// If the function succeeds, the return value is a positive value.
	// If the function is unable to broadcast the message, the return value is –1.

	LONG ret = ::BroadcastSystemMessage(
		BSF_IGNORECURRENTTASK | BSF_FORCEIFHUNG | BSF_POSTMESSAGE, 
		&dwReceipents, 
		m_AppMsgId, 
		wParam, 
		lParam);

	return (BOOL)( ret != -1 );
}

CSingletonApp::~CSingletonApp()
{
	if (NULL != m_hMutex)
	{
		if (m_fOtherInstance)
		{
			::ReleaseMutex(m_hMutex);
		}
		::CloseHandle(m_hMutex);
	}
}

BOOL 
CSingletonApp::Initialize(LPCTSTR SingletonId)
{
	m_hMutex = ::OpenMutex(MUTEX_ALL_ACCESS, FALSE, SingletonId);
	if (NULL == m_hMutex && ERROR_FILE_NOT_FOUND == ::GetLastError())
	{
		// New instance, create and request ownership
		m_hMutex = ::CreateMutex(NULL, TRUE, SingletonId);
		m_fOtherInstance = FALSE;
	}
	else
	{
		// Other instances are running
		m_fOtherInstance = TRUE;
	}
	if (NULL == m_hMutex)
	{
		return FALSE;
	}
	return TRUE;
}

BOOL
CSingletonApp::IsFirstInstance()
{
	return !m_fOtherInstance;
}

BOOL 
CSingletonApp::WaitOtherInstances(DWORD Timeout /* = INFINITE */)
{
	ATLASSERT(NULL != m_hMutex);
	if (NULL == m_hMutex)
	{
		return FALSE;
	}
	if (!m_fOtherInstance)
	{
		return TRUE;
	}
	// Request ownership of mutex.
	DWORD waitResult = ::WaitForSingleObject(m_hMutex, Timeout);
	switch (waitResult)
	{
	case WAIT_OBJECT_0:
		// The thread got mutex ownership.
		return TRUE;
	case WAIT_ABANDONED:
		// Got ownership of the abandoned mutex object.
		m_fOtherInstance = FALSE;
		return TRUE;
	case WAIT_TIMEOUT:
		return FALSE;
	}
	return FALSE;
}

namespace
{

BOOL
EnableTokenPrivilege(
	LPCTSTR lpszSystemName, 
	BOOL    bEnable /*TRUE*/ )
{
	ATLASSERT(NULL != lpszSystemName);

	BOOL fSuccess = FALSE;
	if (::GetVersion() < 0x80000000) // NT40/2K/XP // afxData ???
	{
		HANDLE hToken = NULL;		
		if (::OpenProcessToken(
				::GetCurrentProcess(), 
				TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, 
				&hToken))
		{
			TOKEN_PRIVILEGES tp = { 0 };
			if (::LookupPrivilegeValue(NULL, lpszSystemName, &tp.Privileges[0].Luid ))
			{
				tp.PrivilegeCount = 1;
				tp.Privileges[0].Attributes = ( bEnable ? SE_PRIVILEGE_ENABLED : 0 );

				// To determine whether the function adjusted all of the 
				// specified privileges, call GetLastError:
				if (::AdjustTokenPrivileges(
						hToken, FALSE, &tp, sizeof( TOKEN_PRIVILEGES ), 
						(PTOKEN_PRIVILEGES)NULL, NULL))
				{				
					fSuccess = (::GetLastError() == ERROR_SUCCESS);
				}
			}
			::CloseHandle( hToken );
		}
	}
	return fSuccess;
}

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
