// ndasetup2.cpp : main source file for ndasetup2.exe
//

#include "stdafx.h"
#include "resource.h"
#include "setupdlg.h"
#include "winutil.h"
#define XDBG_MAIN_MODULE
#include "xdebug.h"
CAppModule _Module;

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{
	// Windows 2000 or higher
	if (!IsPlatform(VER_PLATFORM_WIN32_NT) ||
		!MinimumWindowsPlatform(5,0,0))
	{
		CHAR szMessage[255], szCaption[255];
		INT iChars = LoadStringA(hInstance, IDS_ERR_REQUIRES_WINDOWS2000, szMessage, 255);
		ATLASSERT(iChars > 0);
		iChars = LoadStringA(hInstance, IDS_SETUP, szCaption, 255);
		ATLASSERT(iChars > 0);

		(VOID) MessageBoxA(NULL, szMessage, szCaption, MB_OK | MB_ICONERROR);
		return -1;
	}

	if (!IsAdmin()) {
		TCHAR szMessage[255], szCaption[255];
		INT iChars = LoadString(hInstance, IDS_ERR_REQUIRES_ADMIN_PRIV, szMessage, 255);
		ATLASSERT(iChars > 0);
		iChars = LoadString(hInstance, IDS_SETUP, szCaption, 255);
		ATLASSERT(iChars > 0);

		(VOID) MessageBox(NULL, szMessage, szCaption, MB_OK | MB_ICONERROR);
		return -1;
	}



	HRESULT hRes = ::CoInitialize(NULL);
// If you are running on NT 4.0 or higher you can use the following call instead to 
// make the EXE free threaded. This means that calls come in on a random RPC thread.
//	HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
	ATLASSERT(SUCCEEDED(hRes));

	// this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
	::DefWindowProc(NULL, 0, 0, 0L);

	AtlInitCommonControls(ICC_BAR_CLASSES);	// add flags to support other controls

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	INT_PTR nRet = 0;
	// BLOCK: Run application
	{
		CSetupInitDlg dlg;
		nRet = dlg.DoModal();

		if (SETUPDLG_RET_EXECUTEINSTALLER == nRet) {
			dlg.ExecuteInstaller();
		} else if (SETUPDLG_RET_EXECUTEUPDATER == nRet) {
			dlg.ExecuteUpdater();
		}
	}

	_Module.Term();
	::CoUninitialize();

	return nRet;
}
