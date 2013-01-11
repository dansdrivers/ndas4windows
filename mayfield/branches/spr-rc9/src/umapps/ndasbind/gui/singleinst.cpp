#include "stdafx.h"
#include "singleinst.h"

UINT CSingleInstApp::uMsgCheckInst = 0;
TCHAR CSingleInstApp::szUID[128] = {0};

UINT
CSingleInstApp::
GetInstanceMessageID()
{
	ATLASSERT(0 != uMsgCheckInst && "InstInstance is not called!");
	return uMsgCheckInst;
}

BOOL 
CSingleInstApp::
InitInstance(LPCTSTR lpszUID)
{
	HRESULT hr = ::StringCchCopy(
		szUID, 
		sizeof(szUID) / sizeof(szUID[0]), 
		lpszUID);

	ATLASSERT(SUCCEEDED(hr));

	// If two different applications register the same message string, 
	// the applications return the same message value. The message 
	// remains registered until the session ends. If the message is 
	// successfully registered, the return value is a message identifier 
	// in the range 0xC000 through 0xFFFF.

	ATLASSERT( uMsgCheckInst == NULL ); // Only once
	uMsgCheckInst = ::RegisterWindowMessage( szUID );
	ATLASSERT( uMsgCheckInst >= 0xC000 && uMsgCheckInst <= 0xFFFF );

	// If another instance is found, pass document file name to it
	// only if command line contains FileNew or FileOpen parameters
	// and exit instance. In other cases such as FilePrint, etc., 
	// do not exit instance because framework will process the commands 
	// itself in invisible mode and will exit.

	BOOL bFirstInstance = IsFirstInstance(szUID);

	if (bFirstInstance) {
		return TRUE;
	} else {
		return FALSE;
	}
}

BOOL
CSingleInstApp::
EnableTokenPrivilege(
	LPCTSTR lpszSystemName, 
	BOOL    bEnable /*TRUE*/ )
{
	ATLASSERT( lpszSystemName != NULL );
	BOOL bRetVal = FALSE;

	if( ::GetVersion() < 0x80000000 ) // NT40/2K/XP // afxData ???
	{
		HANDLE hToken = NULL;		
		if( ::OpenProcessToken( ::GetCurrentProcess(), 
			TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken ) )
		{
			TOKEN_PRIVILEGES tp = { 0 };

			if( ::LookupPrivilegeValue( NULL, lpszSystemName, &tp.Privileges[0].Luid ) )
			{
				tp.PrivilegeCount = 1;
				tp.Privileges[0].Attributes = ( bEnable ? SE_PRIVILEGE_ENABLED : 0 );

				// To determine whether the function adjusted all of the 
				// specified privileges, call GetLastError:

				if( ::AdjustTokenPrivileges( hToken, FALSE, &tp, 
					sizeof( TOKEN_PRIVILEGES ), (PTOKEN_PRIVILEGES)NULL, NULL ) )
				{				
					bRetVal = ( ::GetLastError() == ERROR_SUCCESS );
				}
			}
			::CloseHandle( hToken );
		}
	}

	return bRetVal;
}

BOOL 
CSingleInstApp::
IsFirstInstance(LPCTSTR lpszUID)
{
	// Create a new mutex. If fails means that process already exists:

	HANDLE hMutex  = ::CreateMutex( NULL, FALSE, lpszUID );
	DWORD  dwError = ::GetLastError();	

	if( hMutex != NULL )
	{
		// Close mutex handle
		::ReleaseMutex( hMutex );
		hMutex = NULL;

		// Another instance of application is running:
		if( dwError == ERROR_ALREADY_EXISTS || dwError == ERROR_ACCESS_DENIED )
			return FALSE;
	}

	return TRUE;
}

BOOL
CSingleInstApp::
WaitInstance(LPCTSTR lpszUID, DWORD dwTimeout)
{
	HANDLE hMutex = ::CreateMutex(NULL, FALSE, lpszUID);
	DWORD dwError = ::GetLastError();

	if (hMutex != NULL) {

		DWORD dwWaitResult = ::WaitForSingleObject(hMutex, dwTimeout);
		::ReleaseMutex(hMutex);

		if (WAIT_OBJECT_0 == dwWaitResult) {
			return TRUE;
		} else if (WAIT_TIMEOUT == dwWaitResult) {
			return WAIT_TIMEOUT;
		} else {
			return FALSE;
		}

	}

	return TRUE;
}

BOOL
CSingleInstApp::
PostInstanceMesage(WPARAM wParam, LPARAM lParam)
{
	ATLASSERT( uMsgCheckInst != NULL );

	// One process can terminate the other process by broadcasting 
	// the private message using the BroadcastSystemMessage function 
	// as follows:

	DWORD dwReceipents = BSM_APPLICATIONS;

	if( EnableTokenPrivilege( SE_TCB_NAME ) )
		dwReceipents |= BSM_ALLDESKTOPS;

	// Send the message to all other instances.
	// If the function succeeds, the return value is a positive value.
	// If the function is unable to broadcast the message, the return value is –1.

	LONG lRet = ::BroadcastSystemMessage( BSF_IGNORECURRENTTASK | BSF_FORCEIFHUNG | 
		BSF_POSTMESSAGE, &dwReceipents, uMsgCheckInst, wParam, lParam );

	return (BOOL)( lRet != -1 );
}
