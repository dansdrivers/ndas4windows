#include "dload.h"
#include <msiquery.h>

struct MsiDll :	DelayedLoader<MsiDll>
{
	MsiDll(HMODULE hModule) :
		DelayedLoader<MsiDll>(hModule) 
	{}

	UINT MsiInstallProductA(LPCSTR szPackagePath, LPCSTR szCommandLine)
	{
		return Invoke<LPCSTR, LPCSTR, UINT>("MsiInstallProductA", szPackagePath, szCommandLine);
	}
	UINT MsiInstallProductW(LPCWSTR szPackagePath, LPCWSTR szCommandLine)
	{
		return Invoke<LPCWSTR, LPCWSTR, UINT>("MsiInstallProductW", szPackagePath, szCommandLine);
	}
	UINT MsiEnableLogA(DWORD dwLogMode,  LPCSTR szLogFile,  DWORD dwLogAttributes)
	{
		return Invoke<DWORD, LPCSTR, DWORD, UINT>("MsiEnableLogA", dwLogMode, szLogFile, dwLogAttributes);
	}
	UINT MsiEnableLogW(DWORD dwLogMode,  LPCWSTR szLogFile,  DWORD dwLogAttributes)
	{
		return Invoke<DWORD, LPCWSTR, DWORD, UINT>("MsiEnableLogW", dwLogMode, szLogFile, dwLogAttributes);
	}
	INSTALLUILEVEL MsiSetInternalUI(INSTALLUILEVEL dwUILevel,  HWND *phWnd)
	{
		return Invoke<INSTALLUILEVEL, HWND*, INSTALLUILEVEL>("MsiSetInternalUI", dwUILevel, phWnd);
	}
};
