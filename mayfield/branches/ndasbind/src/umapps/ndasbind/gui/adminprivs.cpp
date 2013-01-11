#include "stdafx.h"

BOOL
CheckForAdminPrivs()
{
	BOOL bIsAdministrator = FALSE;
	DWORD dwInfoBufferSize = 0;
	PSID psidAdministrators;
	SID_IDENTIFIER_AUTHORITY NTAuthority = SECURITY_NT_AUTHORITY;

	BOOL fSuccess = ::AllocateAndInitializeSid(
		&NTAuthority, 
		2,
		SECURITY_BUILTIN_DOMAIN_RID, 
		DOMAIN_ALIAS_RID_ADMINS,
		0, 
		0, 
		0, 
		0, 
		0, 
		0, 
		&psidAdministrators);

	if (fSuccess) {

		fSuccess = ::CheckTokenMembership(0, psidAdministrators, &bIsAdministrator);
		_ASSERTE(fSuccess);

		::FreeSid(psidAdministrators);
	}
	else {

	}

	return bIsAdministrator;
}

