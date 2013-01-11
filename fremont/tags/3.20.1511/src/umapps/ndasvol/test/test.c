#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <strsafe.h>
#include <ndas/ndasvol.h>

__inline BOOL
XtlSetTraceDll(LPCTSTR DllName, DWORD Flags, DWORD Category, DWORD Level)
{
	typedef BOOL (WINAPI *XTLSETTRACEPROC)(DWORD, DWORD, DWORD);
	static LPCSTR XtlSetTraceProcName = "SetTrace";
	HMODULE hModule = GetModuleHandle(DllName);
	if (hModule)
	{
		XTLSETTRACEPROC pfn = (XTLSETTRACEPROC) GetProcAddress(hModule, XtlSetTraceProcName);
		if (pfn)
		{
			pfn(Flags, Category, Level);
			FreeLibrary(hModule);
			return TRUE;
		}
		FreeLibrary(hModule);
	}
	return FALSE;
}

__forceinline BOOL
NdasVolSetTrace(DWORD Flags, DWORD Category, DWORD Level)
{
	return XtlSetTraceDll(_T("ndasvol.dll"), Flags, Category, Level);
}

#ifndef RTL_NUMBER_OF
#define RTL_NUMBER_OF(A) (sizeof(A)/sizeof((A)[0]))
#endif

void ReportError(LPCTSTR Description, DWORD ErrorCode)
{
	LPTSTR errorDescription = NULL;
	DWORD n = FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_IGNORE_INSERTS |
		FORMAT_MESSAGE_MAX_WIDTH_MASK,
		NULL,
		ErrorCode,
		0,
		(LPTSTR)&errorDescription,
		0,
		NULL);
	if (0 == n)
	{
		_tprintf(_T("%sError Code: %u(0x%X)\n"), Description, ErrorCode, ErrorCode);
		return;
	}
	_tprintf(_T("%sError Code: %u(0x%X) - %s\n"), 
		Description, ErrorCode, ErrorCode, errorDescription);
	return;
}

int __cdecl _tmain(int argc, TCHAR** argv)
{
	TCHAR* lpPath;
	TCHAR mountPoint[MAX_PATH];
	TCHAR volumeName[100];
	LPTSTR lpVolumeDevicePath;
	HANDLE hVolume;
	HRESULT hr;

	if (argc > 1)
	{
		if (0 == lstrcmpi(_T("/trace"), argv[1]) ||
			0 == lstrcmpi(_T("-trace"), argv[1]))
		{
			NdasVolSetTrace(0x0000ffff, 0xffffffff, 5);
			--argc;
			++argv;
		}
	}

	lpPath = (argc < 2) ? _T("C:") : argv[1]; 
	lpVolumeDevicePath = lpPath;

	_tprintf(_T("Device Name: %s\n"), lpVolumeDevicePath);

	hr = NdasIsNdasPath(lpVolumeDevicePath);
	if (FAILED(hr))
	{
		ReportError(_T("NdasIsNdasPath failed.\n"), hr);
		return 1;
	}

	_tprintf(_T("%s is on the NDAS device.\n"), lpVolumeDevicePath );

	if (GetVolumePathName(lpVolumeDevicePath, mountPoint, RTL_NUMBER_OF(mountPoint)))
	{
		if (GetVolumeNameForVolumeMountPoint(
			mountPoint, 
			volumeName, RTL_NUMBER_OF(volumeName)))
		{
			int len = lstrlen(volumeName);
			// _tprintf(_T("VolumeName=%s\n"), volumeName);
			// remove trailing backslash
			if (len > 0 && _T('\\') == volumeName[len-1])
			{
				volumeName[len-1] = _T('\0');
			}
			// replace \\?\Volume... to \\.\Volume...
			if (_T('\\') == volumeName[0] && _T('\\') == volumeName[1] &&
				_T('?') == volumeName[2] && _T('\\') == volumeName[3])
			{
				volumeName[2] = _T('.');
			}
			// _tprintf(_T("VolumeName=%s\n"), volumeName);
			hVolume = CreateFile(volumeName, GENERIC_READ, 
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL, OPEN_EXISTING, 0, NULL);
			if (INVALID_HANDLE_VALUE != hVolume)
			{
				NDAS_LOCATION location;

				hr = NdasGetNdasLocationForVolume(hVolume, &location);
				
				if (FAILED(hr))
				{
					ReportError(_T("NdasGetNdasLocationForVolume failed.\n"), hr);
				}
				else if (S_FALSE == hr)
				{
					_tprintf(_T("Not an NDAS Device\n"));
				}
				else // S_OK
				{
					_tprintf(_T("NDAS_LOCATION=%d"), location);
				}

				CloseHandle(hVolume);
			}
			else
			{
				_tprintf(_T("Opening the volume %s failed.\n"), volumeName);
				ReportError(_T("CreateFile failed.\n"), hr);
			}
		}
		else
		{
			_tprintf(_T("GetVolumeNameForVolumeMountPoint %s failed.\n"), mountPoint);
			ReportError(_T("GetVolumeNameForVolumeMountPoint failed.\n"), hr);
		}
	}
	else
	{
		_tprintf(_T("GetVolumePathName %s failed.\n"), lpVolumeDevicePath);
		ReportError(_T("GetVolumePathName failed.\n"), GetLastError());
	}

	return 0;
}
