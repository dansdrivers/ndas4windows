#include "stdafx.h"
#include "runtimeinfo.h"

DWORD 
GetDllVersion(LPCTSTR lpszDllName)
{
    /* For security purposes, LoadLibrary should be provided with a 
       fully-qualified path to the DLL. The lpszDllName variable should be
       tested to ensure that it is a fully qualified path before it is used. */
	HINSTANCE hInst = ::LoadLibrary(lpszDllName);
	DWORD dwVersion = 0;

	if (hInst)
    {
        DLLGETVERSIONPROC pDllGetVersion = 
			reinterpret_cast<DLLGETVERSIONPROC>(
				::GetProcAddress(hInst, "DllGetVersion"));

        /* Because some DLLs might not implement this function, you
        must test for it explicitly. Depending on the particular 
        DLL, the lack of a DllGetVersion function can be a useful
        indicator of the version. */

        if (pDllGetVersion)
        {
			DLLVERSIONINFO dvi = {0};
            dvi.cbSize = sizeof(dvi);

            HRESULT hr = (*pDllGetVersion)(&dvi);
            if(SUCCEEDED(hr))
            {
               dwVersion = PackVersion(
				   static_cast<WORD>(dvi.dwMajorVersion), 
				   static_cast<WORD>(dvi.dwMinorVersion));
            }
        }

        FreeLibrary(hInst);
    }

    return dwVersion;
}
