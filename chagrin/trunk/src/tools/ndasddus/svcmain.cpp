#include "stdafx.h"
#include "svchelp.h"
#include "ndasddus.h"

int start_service(int argc, TCHAR** argv)
{
	// if it doesn't match any of the above parameters
	// the service control manager may be starting the service
	// so we must call StartServiceCtrlDispatcher

	// this is just to be friendly
	_tprintf( _T("%s -install          to install the service\n"), argv[0]);
	_tprintf( _T("%s -remove           to remove the service\n"), argv[0]);
	_tprintf( _T("%s -debug <params>   to run as a console app for debugging\n"), argv[0]);
	_tprintf( _T("\nStartServiceCtrlDispatcher being called.\n"));
	_tprintf( _T("This may take several seconds.  Please wait.\n"));

	CService* pService = new CNdasDDUService();
	if (NULL == pService) return -1;

	SERVICE_TABLE_ENTRY dispatchTable[2] = {
		*(pService->GetDispatchTableEntry()),
		{ NULL, NULL}
	};

	BOOL fSuccess = ::StartServiceCtrlDispatcher(dispatchTable);
	if (!fSuccess) {
		_tprintf(
			_T("StartServiceCtrlDispatcher failed - Error %d\n"), 
			::GetLastError());
	}

	delete pService;
	return 0;
}

int debug_service(int argc, TCHAR** argv)
{
	CService* pService = new CNdasDDUService();
	if (NULL == pService) return -1;

	pService->ServiceDebug(argc, argv);

	delete pService;
	return 0;
}

int uninstall_service(int argc, TCHAR** argv)
{
	UNREFERENCED_PARAMETER(argc);
	UNREFERENCED_PARAMETER(argv);

	BOOL fSuccess = CNdasDDUServiceInstaller::RemoveService(NDASDEVU_SERVICE_NAME);

	if (!fSuccess) {
		_tprintf(
			_T("Service removal failure (%s (%s)) - Error %d\n"),
			NDASDEVU_DISPLAY_NAME,
			NDASDEVU_SERVICE_NAME,
			::GetLastError());
	} else {
		_tprintf(
			_T("%s (%s) service removed successfully.\n"),
			NDASDEVU_DISPLAY_NAME,
			NDASDEVU_SERVICE_NAME);
	}

	return 0;
}

int install_service(int argc, TCHAR** argv)
{
	UNREFERENCED_PARAMETER(argc);
	UNREFERENCED_PARAMETER(argv);

	TCHAR szBinaryPath[MAX_PATH];

	// Retrieve the current process image path
	GetModuleFileName(NULL, szBinaryPath, MAX_PATH);

	// Install the service with current image path
	//
	// NDASDDUS should be interactive to process actual device 
	// driver installations. Otherwise, installation will fail.
	//
	BOOL fSuccess = CNdasDDUServiceInstaller::InstallService(
		NDASDEVU_SERVICE_NAME,
		NDASDEVU_DISPLAY_NAME,
		SERVICE_ALL_ACCESS,
		SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS,
		SERVICE_DEMAND_START,
		SERVICE_ERROR_NORMAL,
		szBinaryPath);

	if (!fSuccess) {
		_tprintf(
			_T("Service install failure (%s (%s)) - Error %d\n"), 
			NDASDEVU_DISPLAY_NAME,
			NDASDEVU_SERVICE_NAME,
			::GetLastError());
	} else {
		_tprintf(
			_T("%s (%s) service installed.\n"),
			NDASDEVU_DISPLAY_NAME,
			NDASDEVU_SERVICE_NAME);
	}

	return 0;
}


int __cdecl _tmain(int argc, TCHAR** argv)
{
	if ( (argc > 1) &&
		((*argv[1] == '-') || (*argv[1] == '/')) )
	{
		typedef int (*CMDPROC)(int argc, TCHAR** argv);
		struct {
			LPCTSTR cmd_text;
			CMDPROC cmd_proc;
		} 
		directive[] = 
		{ 
			{_T("install"), install_service },
			{_T("remove"), uninstall_service },
			{_T("uninstall"), uninstall_service },
			{_T("debug"), debug_service }
		};

		for (int i = 0; i < RTL_NUMBER_OF(directive); ++i)
			if (lstrcmpi(directive[i].cmd_text, argv[1]+1) == 0)
				return directive[i].cmd_proc(argc - 2, argv + 2);
	}

	return start_service(argc, argv);
}
