// ndasmgmt.cpp : main source file for ndasmgmt.exe
//

#include "stdafx.h"
#include "resource.h"

#pragma warning(disable: 4244)
#pragma warning(disable: 4312)
#include "atlctrlxp.h"
#pragma warning(default: 4312)
#pragma warning(default: 4244)

#include "ndasmgmt.h"
#include "mainframe.h"
#include "singleinst.h"

CAppModule _Module;
ndas::DeviceColl* _pDeviceColl;
ndas::LogicalDeviceColl* _pLogDevColl;

int Run(LPTSTR /*lpstrCmdLine*/ = NULL, int nCmdShow = SW_SHOWDEFAULT)
{
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);


	//
	// Check USER API DLL availability
	//

	HMODULE hDll = ::LoadLibraryEx(_T("ndasuser.dll"), NULL, 0);
	if (NULL == hDll) {
		(VOID) ::MessageBox(::GetDesktopWindow(), 
			_T("Unable to load NDASUSER.DLL.\n")
			_T("Please check the installation."),
			_T("NDAS Management Error"),
			MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
		return -1;
	}

	BOOL fSuccess = ::FreeLibrary(hDll);
	ATLASSERT(fSuccess);

	CMainFrame wndMain;

#ifndef HWND_MESSAGE
#define HWND_MESSAGE     ((HWND)-3)
#endif

	if(wndMain.Create(NULL, NULL, _T("NDASMGMT")) == NULL)
	{
		ATLTRACE(_T("Main window creation failed!\n"));
		return 0;
	}

	wndMain.ShowWindow(SW_HIDE); // nCmdShow);

	int nRet = theLoop.Run();

	_Module.RemoveMessageLoop();
	return nRet;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{
//	HRESULT hRes = ::CoInitialize(NULL);
// If you are running on NT 4.0 or higher you can use the following call instead to 
// make the EXE free threaded. This means that calls come in on a random RPC thread.
	HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
	ATLASSERT(SUCCEEDED(hRes));

	// this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
	::DefWindowProc(NULL, 0, 0, 0L);

	AtlInitCommonControls(ICC_COOL_CLASSES | ICC_BAR_CLASSES);	// add flags to support other controls

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	AtlAxWinInit();

	int nRet = 0;
	
	BOOL bFirstInst = CSingleInstApp::InitInstance(APP_INST_UID);
	if (!bFirstInst) {

		if (::lstrcmpi(_T("/exit"), lpstrCmdLine) == 0) {

			CSingleInstApp::PostInstanceMesage(AIMSG_EXIT, 0);
			nRet = 251;

		} else if (::lstrcmpi(_T("/restart"), lpstrCmdLine) == 0) {

			CSingleInstApp::PostInstanceMesage(AIMSG_EXIT, 0);
			CSingleInstApp::WaitInstance(APP_INST_UID);
			nRet = Run(lpstrCmdLine, nCmdShow);
			CSingleInstApp::InitInstance(APP_INST_UID);

		} else {

			CSingleInstApp::PostInstanceMesage(AIMSG_POPUP, 0);
			nRet = 250;
		}

		//		ATOM atCmdLine = ::GlobalAddAtom(lpstrCmdLine);
		//		if (NULL != atCmdLine) {
		//		}


	} else {

		nRet = Run(lpstrCmdLine, nCmdShow);

	}

	_Module.Term();
	::CoUninitialize();

	return nRet;
}
