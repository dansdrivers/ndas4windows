#pragma once

BOOL 
pRequestEject(
	DWORD SlotNo,
	CONFIGRET* pConfigRet, 
	PPNP_VETO_TYPE pVetoType, 
	LPTSTR pszVetoName, 
	DWORD nNameLength);

BOOL
pIsWindowsXPOrLater();

