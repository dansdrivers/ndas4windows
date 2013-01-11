#include <windows.h>
#include <stdio.h>
#include <ndas/ndasfsctl.h>

HRESULT geterrdesc(DWORD errcode, LPWSTR buf, DWORD buflen)
{
	DWORD n = FormatMessageW(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		errcode,
		0,
		buf,
		buflen,
		NULL);
	if (0 == n)
	{
		buf[0] = 0;
		return HRESULT_FROM_WIN32(GetLastError());
	}
	return S_OK;
}

int __cdecl wmain(int argc, WCHAR** argv)
{
	HRESULT hr;

	if (argc < 2)
	{
		wprintf(L"usage: ndasfscmd shutdown <ndasfat|ndasntfs|ndasrofs>\n");
		return 1;
	}
	
	if (0 == lstrcmpiW(argv[1], L"shutdown"))
	{
		if (argc < 3)
		{
			wprintf(L"usage: ndasfscmd shutdown <ndasfat|ndasntfs|ndasrofs>\n");
			return 1;
		}
		if (0 == lstrcmpiW(L"ndasfat", argv[2]))
		{
			hr = NdasFsCtlShutdown(NdasFatControlDeviceName);
		}
		else if (0 == lstrcmpiW(L"ndasntfs", argv[2]))
		{
			hr = NdasFsCtlShutdown(NdasNtfsControlDeviceName);
		}
		else if (0 == lstrcmpiW(L"ndasrofs", argv[2]))
		{
			hr = NdasFsCtlShutdown(NdasRofsControlDeviceName);
		}
		else
		{
			wprintf(L"usage: ndasfscmd shutdown <ndasfat|ndasntfs|ndasrofs>\n");
			return 1;
		}
	}
	else
	{
		wprintf(L"usage: ndasfscmd shutdown <ndasfat|ndasntfs>\n");
		return 1;
	}

	if (FAILED(hr))
	{
		WCHAR desc[512];
		wprintf(L"Operation failed, hr=0x%X\n", hr);
		
		if (SUCCEEDED(geterrdesc(hr, desc, 512)))
		{
			wprintf(L"%s", desc);
		}

		return 2;
	}

	wprintf(L"Shutdown completed successfully.\n");
	
	return 0;
}

