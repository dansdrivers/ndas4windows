#include <windows.h>
#include <crtdbg.h>
#include <tchar.h>
#include <strsafe.h>
#include <xixfsctl.h>

void usage()
{
	_tprintf(TEXT("xixfscmd [version | shutdown]\n"));
	_tprintf(TEXT("  - shutdown : shutdown the xixfs to unload.\n"));
}

int __cdecl _tmain(int argc, LPTSTR* argv)
{
	HRESULT hr;

	if(argc < 2) 
	{
		usage();
		return 2;
	}

	if (lstrcmpi(argv[1],TEXT("version")) == 0) 
	{
		WORD VersionMajor;
		WORD VersionMinor;
		WORD VersionBuild;
		WORD VersionPrivate;
		WORD NdfsMajor;
		WORD NdfsMinor;

		hr = XixfsCtlGetVersion(
			&VersionMajor,
			&VersionMinor,
			&VersionBuild,
			&VersionPrivate,
			&NdfsMajor,
			&NdfsMinor);

		if (FAILED(hr)) 
		{
			_tprintf(TEXT("XixfsCtlGetVersion failed, error=0x%X\n"), hr);
			return 1;
		} 

		_tprintf(TEXT("Xixfs Version %u.%u.%u.%u (Compatibility Version %u.%u)\n"),
			VersionMajor, VersionMinor, VersionBuild, VersionPrivate,
			NdfsMajor, NdfsMinor);
	}
	else if (0 == lstrcmpi(argv[1],TEXT("unload")) ||
		0 == lstrcmpi(argv[1], TEXT("shutdown")))
	{
		hr = XixfsCtlShutdown();

		if (FAILED(hr))
		{
			_tprintf(TEXT("XixfsCtlShutdown failed, error=0x%X\n"), hr);
			return 1;
		}

		_tprintf(TEXT("Xixfs is now ready to unload.\n"));
	} 
	else 
	{
		usage();
		return 2;
	}

	return 0;
}
