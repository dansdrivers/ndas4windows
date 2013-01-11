#include "stdatl.hpp"
#include <ndas/ndascomm.h>
#include "maindlg.h"

CAppModule _Module;

int 
Run(
	__in LPTSTR /*lpstrCmdLine*/, 
	__in int /*nShowCmd*/)
{
	//
	// Let's roll the main window
	//
	int retCode = 0;

	CMainDlg wndMain;
	retCode = static_cast<int>(wndMain.DoModal());

	return retCode;
}

int 
_tWinMain(
	__in HINSTANCE hInstance, 
	__in_opt HINSTANCE hPrevInstance, 
	__in_opt LPTSTR lpCmdLine,
	__in int nShowCmd )
{
	WORD				wVersionRequested;
	WSADATA				wsaData;

	COMVERIFY(CoInitialize(NULL));

	// this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
	(void) DefWindowProc(NULL, 0, 0, 0L);

	// add flags to support other controls
	ATLVERIFY(AtlInitCommonControls(ICC_WIN95_CLASSES | ICC_INTERNET_CLASSES));	

	COMVERIFY(_Module.Init(NULL, hInstance));

	int retCode = -1;

	wVersionRequested = MAKEWORD( 2, 2 );
	
	retCode = WSAStartup(wVersionRequested, &wsaData);
	if(retCode != 0) {
		retCode = -1;
	} else {
		retCode = Run(lpCmdLine, nShowCmd);

		WSACleanup();
	}
	_Module.Term();

	CoUninitialize();

	return retCode;
}
