#include "dload.h"

struct Kernel32Dll :
	public DelayedLoader<Kernel32Dll>
{
	Kernel32Dll(HMODULE hModule) : DelayedLoader<Kernel32Dll>(hModule)
	{}

	static LPCTSTR GetModuleName() throw()
	{ 
		return _T("kernel32.dll"); 
	}

	BOOL VerifyVersionInfoW(
		LPOSVERSIONINFOEXW lpVersionInfo,
		DWORD dwTypeMask,
		DWORDLONG dwlConditionMask)
	{
		return Invoke<LPOSVERSIONINFOEXW, DWORD, DWORDLONG, BOOL>(
			"VerifyVersionInfoW", lpVersionInfo, dwTypeMask, dwlConditionMask);
	}	

	BOOL VerifyVersionInfoA(
		LPOSVERSIONINFOEXA lpVersionInfo,
		DWORD dwTypeMask,
		DWORDLONG dwlConditionMask)
	{
		return Invoke<LPOSVERSIONINFOEXA, DWORD, DWORDLONG, BOOL>(
			"VerifyVersionInfoA", lpVersionInfo, dwTypeMask, dwlConditionMask);
	}	

	ULONGLONG VerSetConditionMask(
		ULONGLONG dwlConditionMask,
		DWORD dwTypeBitMask,
		BYTE dwConditionMask)
	{
		return Invoke<ULONGLONG, DWORD, BYTE, ULONGLONG>(
			"VerSetConditionMask", dwlConditionMask, dwTypeBitMask, dwConditionMask);
	}

	void GetNativeSystemInfo(
		LPSYSTEM_INFO lpSystemInfo
		)
	{
		InvokeVoid<LPSYSTEM_INFO>(
			"GetNativeSystemInfo", lpSystemInfo);
	}


	LANGID GetUserDefaultUILanguage()
	{
		return Invoke<LANGID>("GetUserDefaultUILanguage");
	}

	LANGID GetSystemDefaultUILanguage()
	{
		return Invoke<LANGID>("GetSystemDefaultUILanguage");
	}

};
