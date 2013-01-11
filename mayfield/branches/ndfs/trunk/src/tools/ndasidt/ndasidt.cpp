#include "precomp.hpp"
#include "resource.h"

#include "maindlg.hpp"

CAppModule _Module;

namespace
{
	int Run(LPTSTR lpstrCmdLine, int nCmdShow);
}

int 
WINAPI 
_tWinMain(
	HINSTANCE hInstance, 
	HINSTANCE /*hPrevInstance*/, 
	LPTSTR lpstrCmdLine, 
	int nCmdShow)
{
	HRESULT hr = ::CoInitialize(NULL);
	ATLASSERT(SUCCEEDED(hr));

	// this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
	(void) ::DefWindowProc(NULL, 0, 0, 0L);

	// add flags to support other controls
	BOOL fSuccess = ::AtlInitCommonControls(ICC_WIN95_CLASSES | ICC_INTERNET_CLASSES);	
	ATLASSERT(fSuccess);

	hr = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hr));

	int nRet = Run(lpstrCmdLine, nCmdShow);

	_Module.Term();
	
	::CoUninitialize();

	return nRet;
}

//////////////////////////////////////////////////////////////////////////
//
// anonymous namespace
//
//////////////////////////////////////////////////////////////////////////
namespace
{

int Run(LPTSTR lpstrCmdLine, int nCmdShow)
{
	int nRet = 0;

#define USE_MODAL_DIALOG
#ifdef USE_MODAL_DIALOG

	CMainDialog wndMain;
	nRet = static_cast<int>(wndMain.DoModal());

#else

	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	CMainDialog wndMain;
	wndMain.Create(NULL);
	wndMain.ShowWindow(nCmdShow);

	nRet = theLoop.Run();

	_Module.RemoveMessageLoop();

#endif

	return nRet;
}

}
