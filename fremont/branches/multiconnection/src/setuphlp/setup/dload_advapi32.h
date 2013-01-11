#include "dload.h"

class Advapi32Dll :
	public DelayedLoader<Advapi32Dll>
{
public:
	Advapi32Dll(HMODULE hModule) : DelayedLoader<Advapi32Dll>(hModule)
	{}

	static LPCTSTR GetModuleName() throw()
	{ 
		return _T("advapi32.dll"); 
	}

	BOOL CheckTokenMembership(HANDLE TokenHandle, PSID SidToCheck, PBOOL IsMember)
	{
		return Invoke<HANDLE, PSID, PBOOL, BOOL>("CheckTokenMembership", TokenHandle, SidToCheck, IsMember);
	}

	BOOL AdjustTokenPrivileges(
		HANDLE TokenHandle, 
		BOOL DisableAllPrivileges,
		PTOKEN_PRIVILEGES NewState, 
		DWORD BufferLength, 
		PTOKEN_PRIVILEGES PreviousState, 
		PDWORD ReturnLength)
	{
		return Invoke<HANDLE, BOOL, PTOKEN_PRIVILEGES, DWORD, PTOKEN_PRIVILEGES, PDWORD, BOOL>(
			"AdjustTokenPrivileges", 
			TokenHandle, DisableAllPrivileges, NewState, BufferLength, PreviousState, ReturnLength);
	}

	BOOL OpenProcessToken(HANDLE ProcessHandle, DWORD DesiredAccess, PHANDLE TokenHandle)
	{
		return Invoke<HANDLE, DWORD, PHANDLE, BOOL>("OpenProcessToken", ProcessHandle, DesiredAccess, TokenHandle);
	}

	BOOL LookupPrivilegeValueA(LPCSTR lpSystemName, LPCSTR lpName, PLUID lpLuid)
	{
		return Invoke<LPCSTR, LPCSTR, PLUID, BOOL>("LookupPrivilegeValueA", lpSystemName, lpName, lpLuid);
	}

	BOOL LookupPrivilegeValueW(LPCWSTR lpSystemName, LPCWSTR lpName, PLUID lpLuid)
	{
		return Invoke<LPCWSTR, LPCWSTR, PLUID, BOOL>("LookupPrivilegeValueW", lpSystemName, lpName, lpLuid);
	}

	BOOL AllocateAndInitializeSid(
		PSID_IDENTIFIER_AUTHORITY pIdentifierAuthority, BYTE nSubAuthorityCount,
		DWORD dwSubAuthority0, DWORD dwSubAuthority1, DWORD dwSubAuthority2, DWORD dwSubAuthority3,
		DWORD dwSubAuthority4, DWORD dwSubAuthority5, DWORD dwSubAuthority6, DWORD dwSubAuthority7,
		PSID* pSid)
	{
		return Invoke<PSID_IDENTIFIER_AUTHORITY, BYTE, 
			DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, PSID*,
			BOOL>(
				"AllocateAndInitializeSid",
				pIdentifierAuthority, nSubAuthorityCount, 
				dwSubAuthority0, dwSubAuthority1, dwSubAuthority2, dwSubAuthority3,
				dwSubAuthority4, dwSubAuthority5, dwSubAuthority6, dwSubAuthority7,
				pSid);
	}

	PVOID FreeSid(PSID pSid)
	{
		return Invoke<PSID, PVOID>("FreeSid", pSid);
	}
};

