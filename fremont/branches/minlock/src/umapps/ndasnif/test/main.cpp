
#ifndef _UNICODE
#define _UNICODE
#endif

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <tchar.h>
#include <objbase.h>
#include <ndas/ndasnif.h>

int __cdecl _tmain(int argc, TCHAR** argv)
{
    ::CoInitialize(NULL);
    for (int i = 1; i < argc; ++i)
    {
        NDAS_NIF_V1_ENTRYW* pEntry;
        DWORD EntryCount;
        HRESULT hr = NdasNifImport(argv[i], &EntryCount, &pEntry);
        if (SUCCEEDED(hr))
        {
			_tprintf(_T("Count=%d\n"), EntryCount);
            for (DWORD n = 0; n < EntryCount; ++n)
            {
                _tprintf(_T("Name       : %s\n"), pEntry[n].Name);
                _tprintf(_T("DeviceID   : %s\n"), pEntry[n].DeviceId);
                _tprintf(_T("WriteKey   : %s\n"), pEntry[n].WriteKey);
                _tprintf(_T("Description: %s\n"), pEntry[n].Description);
            }
            ::LocalFree(pEntry);
        }
    }
	::CoUninitialize();

	return 0;
}

