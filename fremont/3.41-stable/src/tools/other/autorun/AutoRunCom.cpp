#include "stdafx.h"
#include "autorunhelper.h"
#include "autoruncom.h"

#include "autorunhelper_i.c"

HWND hWndMainFrame = NULL;

STDMETHODIMP
CCoAutoRun::Close()
{
	PostMessage(hWndMainFrame, WM_CLOSE, 0, 0);
	return S_OK;
}

STDMETHODIMP
CCoAutoRun::MessageBox(BSTR message, BSTR Caption, int type, int* result)
{
	HMODULE hUser32 = ::LoadLibrary(_T("user32.dll"));
	int r = ::MessageBox(NULL, message, Caption, type);
	if (result) *result = r;
	return S_OK;
}

// SE_ERR_NOASSOC 31
STDMETHODIMP CCoAutoRun::Run(BSTR verb, BSTR program, VARIANT parameter, int* ret)
{
	CComVariant vParam(parameter);
	ATLENSURE_SUCCEEDED( vParam.ChangeType(VT_BSTR) );

	HINSTANCE sret = ::ShellExecute(
		hWndMainFrame, COLE2CT(verb), COLE2CT(program), COLE2CT(vParam.bstrVal), NULL, SW_NORMAL);
	*ret = reinterpret_cast<int>(sret);
	return S_OK;
}

STDMETHODIMP CCoAutoRun::ShellExec(BSTR verb, BSTR program, VARIANT parameter, int* ret)
{
	CComVariant vParam(parameter);
	ATLENSURE_SUCCEEDED( vParam.ChangeType(VT_BSTR) );

	HINSTANCE sret = ::ShellExecute(
		hWndMainFrame, COLE2CT(verb), COLE2CT(program), COLE2CT(vParam.bstrVal), NULL, SW_NORMAL);
	*ret = reinterpret_cast<int>(sret);
	return S_OK;
}

STDMETHODIMP CCoAutoRun::get_DefaultLangID(int* plcid)
{
	typedef LANGID (WINAPI* PFN)(void);
	HMODULE hKernel32 = ::LoadLibrary(_T("kernel32.dll"));
	PFN pfn = (PFN) ::GetProcAddress(hKernel32, "GetUserDefaultUILanguage");
	if (NULL == pfn)
	{
		pfn = (PFN) ::GetProcAddress(hKernel32, "GetUserDefaultLangID");
	}
	if (NULL != pfn)
	{
		*plcid = pfn();
	}
	else
	{
		*plcid = 0;
	}
	
	return S_OK;
}

STDMETHODIMP CCoAutoRun::get_Title(BSTR* title)
{
	TCHAR szTitle[255] = {0};
	::GetWindowText(hWndMainFrame, szTitle, 255);
	CComBSTR bstrTitle(szTitle);
	*title = bstrTitle.Detach();
	return S_OK;
}

STDMETHODIMP CCoAutoRun::put_Title(BSTR title)
{
	::SetWindowText(hWndMainFrame, COLE2CT(title));
	return S_OK;
}

STDMETHODIMP CCoAutoRun::get_OSVersion(int* version)
{
	OSVERSIONINFOEX vi = {0};
	vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	::GetVersionEx(reinterpret_cast<LPOSVERSIONINFO>(&vi));
	DWORD value = vi.dwMajorVersion * 100 + vi.dwMinorVersion;
	*version = value;
	return S_OK;
}

STDMETHODIMP CCoAutoRun::get_OSPlatform(int* platform)
{
	OSVERSIONINFOEX vi = {0};
	vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	::GetVersionEx(reinterpret_cast<LPOSVERSIONINFO>(&vi));
	*platform = vi.dwPlatformId;
	return S_OK;
}

//PROCESSOR_ARCHITECTURE_UNKNOWN 0xFFFF
//PROCESSOR_ARCHITECTURE_INTEL 0
//PROCESSOR_ARCHITECTURE_IA64	6
//PROCESSOR_ARCHITECTURE_AMD64 9
STDMETHODIMP CCoAutoRun::get_ProcessorArchitecture(int* arch)
{
	SYSTEM_INFO si = {0};
	typedef VOID (WINAPI* PFN_GETSYSTEMINFO)(LPSYSTEM_INFO);
	HMODULE hKernel32 = ::LoadLibrary(_T("kernel32.dll"));
	PFN_GETSYSTEMINFO pfn = (PFN_GETSYSTEMINFO) ::GetProcAddress(hKernel32, "GetNativeSystemInfo");
	if (NULL == pfn)
	{
		pfn = (PFN_GETSYSTEMINFO) ::GetProcAddress(hKernel32, "GetSystemInfo");
	}
	if (NULL != pfn)
	{
		pfn(&si);
		*arch = si.wProcessorArchitecture;
	}
	else
	{
		*arch = PROCESSOR_ARCHITECTURE_UNKNOWN;
	}
	return S_OK;	
}

STDMETHODIMP CCoAutoRun::ReadINF(BSTR section, BSTR key, VARIANT defaultValue, BSTR* retValue)
{
	TCHAR moduleDir[MAX_PATH] = {0};
	// Get the autorun.exe's path
	ATLENSURE( ::GetModuleFileName(NULL, moduleDir, MAX_PATH) );
	::PathRemoveFileSpec(moduleDir);

	TCHAR autoRunInfPath[MAX_PATH] = {0};
	ATLENSURE_SUCCEEDED( ::StringCchCopy(autoRunInfPath, MAX_PATH, moduleDir) );
	::PathAppend(autoRunInfPath, _T("AUTORUN.INF"));

	CComVariant vDefaultValue(defaultValue);
	ATLENSURE_SUCCEEDED(vDefaultValue.ChangeType(VT_BSTR));

	// Read [HTML] HTML=...
	TCHAR value[255] = {0};
	GetPrivateProfileString(
		COLE2CT(section), COLE2CT(key), COLE2CT(vDefaultValue.bstrVal), 
		value, 255, autoRunInfPath);

	CComBSTR bstrValue(value);
	*retValue = bstrValue.Detach();

	return S_OK;
}

STDMETHODIMP CCoAutoRun::get_AutoRunPath(BSTR* path)
{
	TCHAR moduleDir[MAX_PATH] = {0};
	// Get the autorun.exe's path
	ATLENSURE( ::GetModuleFileName(NULL, moduleDir, MAX_PATH) );
	::PathRemoveFileSpec(moduleDir);

	CComBSTR bstrPath(moduleDir);
	*path = bstrPath.Detach();
	return S_OK;
}

STDMETHODIMP CCoAutoRun::GetLanguageName(
	__in int LocaleId, __deref_out BSTR* Name)
{
	TCHAR buffer[250] = {0};

	int n = GetLocaleInfo(
		static_cast<LCID>(LocaleId), 
		LOCALE_SLANGUAGE,
		buffer,
		RTL_NUMBER_OF(buffer));

	if (0 == n)
	{
		return HRESULT_FROM_WIN32(GetLastError());
	}

	CComBSTR bstrName(buffer);
	*Name = bstrName.Detach();

	return S_OK;
}

STDMETHODIMP CCoAutoRun::GetEnglishLanguageName(
	__in int LocaleId, __deref_out BSTR* Name)
{
	TCHAR buffer[250] = {0};

	int n = GetLocaleInfo(
		static_cast<LCID>(LocaleId), 
		LOCALE_SENGLANGUAGE,
		buffer,
		RTL_NUMBER_OF(buffer));

	if (0 == n)
	{
		return HRESULT_FROM_WIN32(GetLastError());
	}

	CComBSTR bstrName(buffer);
	*Name = bstrName.Detach();

	return S_OK;
}

STDMETHODIMP CCoAutoRun::GetNativeLanguageName(
	__in int LocaleId, __deref_out BSTR* Name)
{
	TCHAR buffer[250] = {0};

	int n = GetLocaleInfo(
		static_cast<LCID>(LocaleId), 
		LOCALE_SNATIVELANGNAME,
		buffer,
		RTL_NUMBER_OF(buffer));

	if (0 == n)
	{
		return HRESULT_FROM_WIN32(GetLastError());
	}

	CComBSTR bstrName(buffer);
	*Name = bstrName.Detach();

	return S_OK;
}
