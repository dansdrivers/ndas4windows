#include <windows.h>
#include "osverify.h"

BOOL WINAPI VerifyWindowsVersion(
	BYTE dwCondMask, 
	DWORD dwMajorVersion, 
	DWORD dwMinorVersion, 
	WORD wServicePackMajor)
{
	OSVERSIONINFOEX osvi = {0};
	DWORDLONG dwlCondMask = 0;
	DWORD dwTypeMask = 0;

	osvi.dwOSVersionInfoSize = sizeof(osvi);
	osvi.dwMajorVersion = dwMajorVersion;
	osvi.dwMinorVersion = dwMinorVersion;

	if (wServicePackMajor < 0xFFFF) {
		osvi.wServicePackMajor = wServicePackMajor;
	}

	VER_SET_CONDITION(dwlCondMask, VER_MAJORVERSION, dwCondMask);
	dwTypeMask |= VER_MAJORVERSION;

	VER_SET_CONDITION(dwlCondMask, VER_MINORVERSION, dwCondMask);
	dwTypeMask |= VER_MINORVERSION;

	if (wServicePackMajor < 0xFFFF) {
		VER_SET_CONDITION(dwlCondMask, VER_SERVICEPACKMAJOR, dwCondMask);
		dwTypeMask |= VER_SERVICEPACKMAJOR;
	}

	return VerifyVersionInfo(
		&osvi, 
		dwTypeMask,
		dwlCondMask);
}

BOOL WINAPI
IsUserAdmin(VOID)
/*++

Routine Description:

This routine returns TRUE if the caller's process is a
member of the Administrators local group.

Caller is NOT expected to be impersonating anyone and IS
expected to be able to open their own process and process
token.

Arguments:

None.

Return Value:

TRUE - Caller has Administrators local group.

FALSE - Caller does not have Administrators local group.

--*/

{
	HANDLE Token;
	DWORD BytesRequired;
	PTOKEN_GROUPS Groups;
	BOOL b;
	DWORD i;
	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
	PSID AdministratorsGroup;

	//
	// Open the process token.
	//
	if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&Token)) {
		return(GetLastError() == ERROR_CALL_NOT_IMPLEMENTED);
	}

	b = FALSE;
	Groups = NULL;

	//
	// Get group information.
	//
	if(!GetTokenInformation(Token,TokenGroups,NULL,0,&BytesRequired)
		&& (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		&& (Groups = (PTOKEN_GROUPS)LocalAlloc(LPTR,BytesRequired))
		&& GetTokenInformation(Token,TokenGroups,Groups,BytesRequired,&BytesRequired)) 
	{

		b = AllocateAndInitializeSid(
			&NtAuthority,
			2,
			SECURITY_BUILTIN_DOMAIN_RID,
			DOMAIN_ALIAS_RID_ADMINS,
			0, 0, 0, 0, 0, 0,
			&AdministratorsGroup
			);

		if(b) {

			//
			// See if the user has the administrator group.
			//
			b = FALSE;
			for(i=0; i<Groups->GroupCount; i++) {
				if(EqualSid(Groups->Groups[i].Sid,AdministratorsGroup)) {
					b = TRUE;
					break;
				}
			}

			FreeSid(AdministratorsGroup);
		}
	}

	//
	// Clean up and return.
	//

	if(Groups) {
		LocalFree((HLOCAL)Groups);
	}

	CloseHandle(Token);

	return(b);
}
