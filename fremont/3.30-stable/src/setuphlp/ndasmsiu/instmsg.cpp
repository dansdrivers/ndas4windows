#include "precomp.hpp"

namespace local 
{
    
UINT
GetInstanceMessageId(
    LPCTSTR szUID)
{
	UINT nMsg = ::RegisterWindowMessage( szUID );
	_ASSERTE( nMsg >= 0xC000 && nMsg <= 0xFFFF );
    return nMsg;
}

BOOL
EnableTokenPrivilege(
	LPCTSTR lpszSystemName, 
	BOOL    bEnable /*TRUE*/ )
{
	_ASSERTE( lpszSystemName != NULL );
	BOOL bRetVal = FALSE;

	if (::GetVersion() < 0x80000000 ) // NT40/2K/XP // afxData ???
	{
		HANDLE hToken = NULL;		
		if (::OpenProcessToken( ::GetCurrentProcess(), 
			TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken ) )
		{
			TOKEN_PRIVILEGES tp = { 0 };
			if (::LookupPrivilegeValue( NULL, lpszSystemName, &tp.Privileges[0].Luid ) )
			{
				tp.PrivilegeCount = 1;
				tp.Privileges[0].Attributes = ( bEnable ? SE_PRIVILEGE_ENABLED : 0 );

				// To determine whether the function adjusted all of the 
				// specified privileges, call GetLastError:

				if (::AdjustTokenPrivileges(hToken, FALSE, &tp, 
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
PostInstanceMessage(
    UINT nMsg,
    WPARAM wParam, 
    LPARAM lParam)
{

	// One process can terminate the other process by broadcasting 
	// the private message using the BroadcastSystemMessage function 
	// as follows:

	DWORD dwReceipents = BSM_APPLICATIONS;

	if (EnableTokenPrivilege(SE_TCB_NAME, TRUE))
    {
		dwReceipents |= BSM_ALLDESKTOPS;
    }

	// Send the message to all other instances.
	// If the function succeeds, the return value is a positive value.
	// If the function is unable to broadcast the message, the return value is ?.

	LONG ret = ::BroadcastSystemMessage(
        BSF_IGNORECURRENTTASK | BSF_FORCEIFHUNG |
		BSF_POSTMESSAGE, 
        &dwReceipents, 
        nMsg, wParam, lParam );

	return (BOOL)( ret != -1 );
}

}
