#include <windows.h>
#include <crtdbg.h>
#include <tchar.h>
#include <strsafe.h>
#include "xixfsctl.h"

void usage()
{
	_tprintf(TEXT("xixfscmd [version | unload]\n"));
	_tprintf(TEXT("  - unload : make Xixfs ready to unload\n"));
}


int __cdecl _tmain(int argc, LPTSTR* argv)
{
	BOOL	fSuccess(FALSE);
	BOOL	bret;

	if(argc < 2) {
		usage();
		return 1;
	}

	if (lstrcmpi(argv[1],TEXT("version")) == 0) {
		WORD VersionMajor;
		WORD VersionMinor;
		WORD VersionBuild;
		WORD VersionPrivate;
		WORD NdfsMajor;
		WORD NdfsMinor;

		bret = XixfsCtlGetVersion(
					&VersionMajor,
					&VersionMinor,
					&VersionBuild,
					&VersionPrivate,
					&NdfsMajor,
					&NdfsMinor
				);
		if(bret == FALSE) {
			_tprintf(TEXT("Xixfs control failed. LastError:%lu\n"), GetLastError());
		} else {
			_tprintf(TEXT("- Xixfs version\n"));
			_tprintf(	TEXT("Major   : %u\n")
						TEXT("Minor   : %u\n")
						TEXT("Build   : %u\n")
						TEXT("Private : %u\n")
						TEXT("NDFS Maj: %u\n")
						TEXT("NDFS Min: %u\n"),
						VersionMajor, VersionMinor, VersionBuild, VersionPrivate,
						NdfsMajor, NdfsMinor);
		}

	} else if (lstrcmpi(argv[1],TEXT("unload")) == 0) {
		bret = XixfsCtlReadyForUnload();
		if(bret == FALSE) {
			_tprintf(TEXT("Xixfs control failed. LastError:%lu\n"), GetLastError());
		} else {
			_tprintf(TEXT("- Xixfs is ready to unload\n"));
		}
	} else {
		usage();
	}


	return fSuccess ? 0 : 1;
}
