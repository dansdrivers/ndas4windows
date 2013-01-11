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

int __cdecl _tmain(int argc, TCHAR** argv)
{
	TCHAR* lpPath;
	TCHAR mountPoint[MAX_PATH];
	TCHAR volumeName[100];
	LPTSTR lpVolumeDevicePath;
	HANDLE hVolume;

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

	if (NdasIsNdasPath(lpVolumeDevicePath))
	{
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
					NDAS_SCSI_LOCATION loc;
					if (NdasGetNdasScsiLocationForVolume(hVolume,&loc))
					{
						_tprintf(_T("NDAS_SCSI_LOCATION=(%d,%d,%d)"), 
							loc.SlotNo, loc.TargetID, loc.LUN);
					}
					else
					{
						_tprintf(_T("NdasGetNdasScsiLocationForVolume failed.\n"));
						_tprintf(_T("Error %u(%X)\n"), GetLastError(), GetLastError());
					}
					CloseHandle(hVolume);
				}
				else
				{
					_tprintf(_T("Opening the volume %s failed.\n"), volumeName);
					_tprintf(_T("Error %u(%X)\n"), GetLastError(), GetLastError());
				}
			}
			else
			{
				_tprintf(_T("GetVolumeNameForVolumeMountPoint %s failed.\n"), mountPoint);
				_tprintf(_T("Error %u(%X)\n"), GetLastError(), GetLastError());
			}
		}
		else
		{
			_tprintf(_T("GetVolumePathName %s failed.\n"), lpVolumeDevicePath);
			_tprintf(_T("Error %u(%X)\n"), GetLastError(), GetLastError());
		}
	}
	else
	{
		_tprintf(_T("Error %u(%X)\n"), GetLastError(), GetLastError());
	}
	return 0;
}
