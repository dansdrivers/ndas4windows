#include <atlbase.h>
#include <atlcom.h>
#include <strsafe.h>
#include "autoplayconfig.h"

HRESULT 
GetErrorDescription(
	__in DWORD ErrorCode,
	__out BSTR* Description)
{
	LPWSTR buffer = NULL;
	FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_IGNORE_INSERTS |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_MAX_WIDTH_MASK,
		NULL,
		ErrorCode,
		0,
		(LPWSTR) &buffer,
		0,
		NULL);
	*Description = SysAllocString(buffer);
	return S_OK;
}

HRESULT
CoCreateInstanceAsAdmin(
	HWND hwnd, 
	REFCLSID rclsid, 
	REFIID riid, 
	void ** ppv)
{
	typedef struct tagBIND_OPTS3 : tagBIND_OPTS2 {
		HWND           hwnd;
	} BIND_OPTS3, * LPBIND_OPTS3;

    BIND_OPTS3 bo;
    WCHAR clsid[50];
    WCHAR monikerName[300];

    StringFromGUID2(rclsid, clsid, sizeof(clsid)/sizeof(clsid[0])); 

    HRESULT hr = StringCchPrintf(
		monikerName, 
		sizeof(monikerName)/sizeof(monikerName[0]),
		L"Elevation:Administrator!new:%s", 
		clsid);

    if (FAILED(hr))
	{
        return hr;
	}

    ZeroMemory(&bo, sizeof(bo));
    bo.cbStruct = sizeof(bo);
    bo.hwnd = hwnd;
    bo.dwClassContext = CLSCTX_LOCAL_SERVER;
	
	hr = CoGetObject(monikerName, &bo, riid, ppv);

	return hr;
}

int __cdecl wmain(int argc, WCHAR** argv)
{
	HRESULT hr = CoInitialize(NULL);

	CComPtr<IAutoPlayConfig> pAutoplayConfig;

	//first create non-elevated
	hr =  CoCreateInstance(
		CLSID_CAutoPlayConfig, 
		NULL, 
		CLSCTX_LOCAL_SERVER,
		IID_IAutoPlayConfig, 
		(void**)&pAutoplayConfig);

	if (FAILED(hr))
	{
		BSTR description = NULL;
		GetErrorDescription(hr, &description);
		wprintf(L"CoCreateInstance failed (1), hr=0x%X\n%s\n", 
			hr, description);
		SysFreeString(description);
		return 1;
	}

	DWORD autoPlayValue = 0;
	HKEY keyHandle;
	
	LONG result = RegOpenKeyExW(
		HKEY_CURRENT_USER, NULL, 0, KEY_READ, &keyHandle);

	hr = pAutoplayConfig->GetNoDriveTypeAutoRun(
		(ULONG_PTR) keyHandle, 
		&autoPlayValue);

	RegCloseKey(keyHandle);

	if (FAILED(hr))
	{
		BSTR description = NULL;
		GetErrorDescription(hr, &description);
		wprintf(L"IID_IAutoPlayConfig.GetNoDriveTypeAutoRun() failed, hr=0x%X\n%s\n", 
			hr, description);
		SysFreeString(description);
	}

	wprintf(L"DriveTypeAutoRun=0x%08X\n", autoPlayValue);

	pAutoplayConfig.Release();

	DWORD osVersion = GetVersion();
	DWORD osVersionDef = 
		LOBYTE(LOWORD(osVersion)) * 0x100 + 
		HIBYTE(LOWORD(osVersion));

	if (osVersionDef < 0x600)
	{
		wprintf(L"Elevation is not supported!\n");
		return 0;
	}

	hr = CoCreateInstanceAsAdmin(
		NULL, 
		CLSID_CAutoPlayConfig, 
		IID_IAutoPlayConfig, 
		(void**)&pAutoplayConfig);

	if (FAILED(hr))
	{
		BSTR description = NULL;
		GetErrorDescription(hr, &description);
		wprintf(L"CoGetObject(IID_IAutoPlayConfig) failed, hr=0x%X\n%s\n",
			hr, description);
		SysFreeString(description);

		return 1;
	}

	hr = pAutoplayConfig->SetNoDriveTypeAutoRun(
		(ULONG_PTR) HKEY_CURRENT_USER,
		AutorunFixedDrive,
		0);

	if (FAILED(hr))
	{
		BSTR description = NULL;
		GetErrorDescription(hr, &description);
		wprintf(L"IID_IAutoPlayConfig.SetNoDriveTypeAutoRun failed, hr=0x%X\n%s\n",
			hr, description);
		SysFreeString(description);
	}

	return 0;

}
