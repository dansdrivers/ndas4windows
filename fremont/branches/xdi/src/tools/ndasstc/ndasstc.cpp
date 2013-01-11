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

BOOL
VerifyNdasComm()
{
	HMODULE ndasCommModuleHandle = LoadLibrary(_T("ndascomm.dll"));
	if (NULL == ndasCommModuleHandle)
	{
		return FALSE;
	}
	FreeLibrary(ndasCommModuleHandle);
	return TRUE;
}

int 
_tWinMain(
	__in HINSTANCE hInstance, 
	__in_opt HINSTANCE hPrevInstance, 
	__in_opt LPTSTR lpCmdLine,
	__in int nShowCmd )
{
	COMVERIFY(CoInitialize(NULL));

	// this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
	(void) DefWindowProc(NULL, 0, 0, 0L);

	// add flags to support other controls
	ATLVERIFY(AtlInitCommonControls(ICC_WIN95_CLASSES | ICC_INTERNET_CLASSES));	

	COMVERIFY(_Module.Init(NULL, hInstance));

	int retCode = -1;

	if (!VerifyNdasComm())
	{
		AtlMessageBox(
			NULL, 
			IDS_NDASCOMM_NOT_AVAILABLE, 
			IDS_APP_ERROR_TITLE, 
			MB_OK | MB_ICONSTOP);
	}
	else
	{
		ATLVERIFY( NdasCommInitialize() );
		retCode = Run(lpCmdLine, nShowCmd);
		ATLVERIFY( NdasCommUninitialize() );
	}

	_Module.Term();

	CoUninitialize();

	return retCode;
}
