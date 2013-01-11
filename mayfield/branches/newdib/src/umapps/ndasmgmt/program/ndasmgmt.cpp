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
#include "appconf.h"
#include "autores.h"
#include "muisel.h"
#include "argv.h"
#include "adminprivs.h"
#include "ndasmgmt_appiniterr.h"

CAppModule _Module;
ndas::DeviceColl* _pDeviceColl;
ndas::LogicalDeviceColl* _pLogDevColl;
LANGID _CurrentUILangID = 0;

int Run(LPTSTR lpstrCmdLine = NULL, int nCmdShow = SW_SHOWDEFAULT);

static VOID pAppInitErrMessage(
	UINT nMessageID, UINT nTitleID = IDS_APP_INIT_ERROR_TITLE);

static BOOL pValidateNDASUSER();
static BOOL pLoadResourceModule(LPCTSTR szFileName);
static BOOL pLoadResourceModule(LANGID wPreferred);
static LANGID pGetPreferredLangID(LPCTSTR lpszCmdLine);
static LANGID pGetCmdLineLangID(LPCTSTR lpszCmdLine);

int Run(LPTSTR lpstrCmdLine, int nCmdShow)
{
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	HRESULT hr = S_FALSE;
	BOOL fSuccess = FALSE;
	
	fSuccess = InitAppConfig();
	if (!fSuccess) {
		pAppInitErrMessage(IDS_APP_INIT_ERROR_OUT_OF_MEMORY);
		return ::GetLastError();
	}

	//
	// Check USER API DLL availability
	//
	fSuccess = pValidateNDASUSER();
	if (!fSuccess) {
		return 255;
	}

	// command line override the registry settings
	LANGID wPrefLangID = pGetPreferredLangID(lpstrCmdLine);
	fSuccess = pLoadResourceModule(wPrefLangID);
	if (!fSuccess) {
		return 255;
	}

	CMainFrame wndMain;

#ifndef HWND_MESSAGE
#define HWND_MESSAGE     ((HWND)-3)
#endif

	CRect rcMain(CPoint(0,0),CPoint(0,0));
	HWND hwndMain = wndMain.Create(
		NULL, 
		rcMain, 
		_T("NDAS Device Management"));
	
	if(NULL == hwndMain) {
		ATLTRACE(_T("Main window creation failed!\n"));
		// Showing the message may also fail!
		pAppInitErrMessage(IDS_APP_INIT_ERROR_CREATE_WINDOW);
		return 0;
	}

	// wndMain.ShowWindow(nCmdShow);  // SW_HIDE // nCmdShow;
	wndMain.ShowWindow(SW_HIDE);
	wndMain.UpdateWindow();

	int nRet = theLoop.Run();

	_Module.RemoveMessageLoop();

	return nRet;
}

int 
WINAPI 
_tWinMain(
	HINSTANCE hInstance, 
	HINSTANCE /*hPrevInstance*/, 
	LPTSTR lpstrCmdLine, 
	int nCmdShow)
{

	HRESULT hRes = ::CoInitialize(NULL);
	//
	// ISSUE: 
	// If you call CoInitialize with COINIT_MULTITHREADED,
	// ShellExecute will fail with error SE_ERR_ACCESSDENIED (5).
	// Currently this affects the About dialog for handling the hyperlink,
	// so that the link will not work.
	//
	// REF: http://support.microsoft.com/?kbid=287087
	//
	// If you are running on NT 4.0 or higher you can use the following call instead to 
	// make the EXE free threaded. This means that calls come in on a random RPC thread.
	// HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
	ATLASSERT(SUCCEEDED(hRes));

	// this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
	::DefWindowProc(NULL, 0, 0, 0L);

	AtlInitCommonControls(ICC_COOL_CLASSES | ICC_BAR_CLASSES);	// add flags to support other controls

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	AtlAxWinInit();

	INT nRet = 0;

	LPTSTR *szArgList;
	INT nArgs;

	szArgList = ::CommandLineToArgv(GetCommandLine(), &nArgs);

	if (!CheckForAdminPrivs()) {

		if (nArgs > 1 && ::lstrcmpi(_T("/startup"), szArgList[1]) == 0) {
			//
			// If the application is started from the Programs - Startup 
			// Do not show error message
			//
			// Setup should set this shortcut to run with /startup
			// for this feature
			//
		} else {
			pAppInitErrMessage(IDS_APP_INIT_ERROR_ADMIN_PRIV_REQUIRED);
		}

		::GlobalFree(szArgList);
		_Module.Term();
		::CoUninitialize();
		return 255;
	}


	BOOL bFirstInst = CSingleInstApp::InitInstance(APP_INST_UID);
	if (!bFirstInst) {

		if (nArgs > 1 && ::lstrcmpi(_T("/exit"), szArgList[1]) == 0) {

			CSingleInstApp::PostInstanceMesage(AIMSG_EXIT, 0);
			nRet = 251;

		} else if (nArgs > 1 && ::lstrcmpi(_T("/restart"), szArgList[1]) == 0) {

			CSingleInstApp::PostInstanceMesage(AIMSG_EXIT, 0);
			CSingleInstApp::WaitInstance(APP_INST_UID);
			nRet = Run(lpstrCmdLine, nCmdShow);
			CSingleInstApp::InitInstance(APP_INST_UID);

		} else {

			CSingleInstApp::PostInstanceMesage(AIMSG_POPUP, 0);
			nRet = 250;
		}

	} else {

		nRet = Run(lpstrCmdLine, nCmdShow);

	}

	::GlobalFree(szArgList);

	_Module.Term();
	::CoUninitialize();

	return nRet;
}

VOID 
pAppInitErrMessage(UINT nMessageID, UINT nTitleID)
{
	TCHAR szMessage[APP_INIT_MAX_MESSAGE_LEN] = {0};
	TCHAR szTitle[APP_INIT_MAX_TITLE_LEN] = {0};
	HINSTANCE hAppInst = ::GetModuleHandle(NULL);

	INT iMessageStr = ::LoadString(
		hAppInst, 
		nMessageID, 
		szMessage, 
		RTL_NUMBER_OF(szMessage));

	ATLASSERT(iMessageStr > 0);

	INT iTitleStr = ::LoadString(
		hAppInst, 
		nTitleID, 
		szTitle, 
		RTL_NUMBER_OF(szTitle));

	ATLASSERT(iTitleStr > 0);

	(VOID) ::MessageBox(
		::GetDesktopWindow(),
		szMessage,
		szTitle,
		MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
}


BOOL
pValidateNDASUSER()
{
	AutoHModule hDll = ::LoadLibraryEx(_T("ndasuser.dll"), NULL, 0);
	if (NULL == (HMODULE) hDll) {
		pAppInitErrMessage(IDS_APP_INIT_ERROR_NO_NDASUSER);
		return FALSE;
	}

	FARPROC pProc = ::GetProcAddress(hDll, "_NdasGetAPIVersion@0");
	if (NULL == pProc) {
		pAppInitErrMessage(IDS_APP_INIT_ERROR_INVALID_NDASUSER);
		return FALSE;
	}

	DWORD dwApiVer = ::NdasGetAPIVersion();

	if (NDASUSER_API_VERSION_MAJOR != LOWORD(dwApiVer) ||
		NDASUSER_API_VERSION_MINOR != HIWORD(dwApiVer)) 
	{
		pAppInitErrMessage(IDS_APP_INIT_ERROR_INVALID_NDASUSER);
		return FALSE;
	}

	return TRUE;
}

BOOL
pLoadResourceModule(LPCTSTR szFileName, LANGID wLangID)
{
	HINSTANCE hResInst = ::LoadLibrary(szFileName);

	if (NULL == hResInst) {
		pAppInitErrMessage(IDS_APP_INIT_ERROR_RESOURCE_LOAD);
		return FALSE;
	}

	HINSTANCE hPrevResInst = _Module.SetResourceInstance(hResInst);
	::InitMUILanguage(wLangID);
	_CurrentUILangID = wLangID;

	return TRUE;
}

BOOL
pLoadResourceModule(LANGID wPreferred)
{
	//
	// First to find the available resources
	// from the application directory
	//
		
	DWORD_PTR cbResDlls = ::NuiCreateAvailResourceDLLsInModuleDirectory(
		NULL, 
		NULL, 
		0, 
		NULL);

	if (cbResDlls == 0) {
		pAppInitErrMessage(IDS_APP_INIT_ERROR_NO_RESOURCE);
		return FALSE;
	}

	LPVOID lpResDlls = ::HeapAlloc(
		::GetProcessHeap(),
		HEAP_ZERO_MEMORY,
		cbResDlls);

	if (NULL == lpResDlls) {
		pAppInitErrMessage(IDS_APP_INIT_ERROR_OUT_OF_MEMORY);
		return FALSE;
	}

	AutoProcessHeap autoResDlls = lpResDlls;
	PNUI_RESDLL_INFO pResDlls = static_cast<PNUI_RESDLL_INFO>(lpResDlls);

	cbResDlls = ::NuiCreateAvailResourceDLLsInModuleDirectory(
		NULL, 
		NULL, 
		cbResDlls, 
		pResDlls);

	if (0 == cbResDlls) {
		pAppInitErrMessage(IDS_APP_INIT_ERROR_NO_RESOURCE);
		return FALSE;
	}

	DWORD nAvailLangID = 0;
	for (PNUI_RESDLL_INFO pCur = pResDlls; 
		NULL != pCur;
		pCur = pCur->pNext, ++nAvailLangID)
	{
		ATLTRACE(_T("Available Resource: %d(%08X) %s\n"), 
			pCur->wLangID,
			pCur->wLangID,
			pCur->lpszFilePath);
	}

	AutoProcessHeap autoAvailLang = ::HeapAlloc(
		::GetProcessHeap(), 
		0,
		sizeof(LANGID) * nAvailLangID);

	if (NULL == (LPVOID) autoAvailLang) {
		pAppInitErrMessage(IDS_APP_INIT_ERROR_OUT_OF_MEMORY);
		return FALSE;
	}

	LANGID* pwAvailLangID = (LANGID*)(LPVOID)autoAvailLang;

	DWORD i = 0;
	for (PNUI_RESDLL_INFO pCur = pResDlls; 
		NULL != pCur;
		pCur = pCur->pNext, ++i)
	{
		pwAvailLangID[i] = pCur->wLangID;
	}

	LANGID wUserLangID = 0;
	if (0 == wPreferred) {
		wUserLangID = ::NuiGetCurrentUserUILanguage();
		ATLTRACE(_T("Current User LangID: %d\n"), wUserLangID);
	} else {
		wUserLangID = wPreferred;
	}

	LANGID wCurLangID = ::NuiFindMatchingLanguage(
		wUserLangID, 
		nAvailLangID,
		pwAvailLangID);

	ATLTRACE(_T("Current LangID: %d\n"), wCurLangID);

	// If cannot find the match, try to find English Resource.
	static CONST LANGID EnglishLANGID = 0x409;
	if (0 == wCurLangID) {
		wCurLangID = ::NuiFindMatchingLanguage(
			EnglishLANGID,
			nAvailLangID,
			pwAvailLangID);
	}

	// If still fails, load the first language
	if (0 == wCurLangID) {
		wCurLangID = pwAvailLangID[0];
	}

	LPTSTR lpszResDll = NULL;
	i = 0;
	for (PNUI_RESDLL_INFO pCur = pResDlls; 
		NULL != pCur;
		pCur = pCur->pNext, ++i)
	{
		if (pCur->wLangID == wCurLangID) {
			lpszResDll = pCur->lpszFilePath;
			break;
		}
	}

	ATLTRACE(_T("Resource DLL: %s\n"), lpszResDll);
	ATLASSERT(i < nAvailLangID);

	return pLoadResourceModule(lpszResDll, wCurLangID);
}

LANGID 
pGetCmdLineLangID(LPCTSTR lpszCmdLine)
{
	BOOL fSuccess;
	LPTSTR* szArgs = NULL;
	INT nArgs;

	LANGID cmdLangID = 0;

	szArgs = CommandLineToArgv(lpszCmdLine, &nArgs);
	for (INT i = 0; i < nArgs; ++i) {
		if (0 == ::lstrcmpi(_T("/l"),szArgs[i]) ||
			0 == ::lstrcmpi(_T("/lang"), szArgs[i]) ||
			0 == ::lstrcmpi(_T("/language"), szArgs[i]) ||
			0 == ::lstrcmpi(_T("-l"), szArgs[i]) ||
			0 == ::lstrcmpi(_T("-lang"), szArgs[i]) ||
			0 == ::lstrcmpi(_T("-language"), szArgs[i])) 
		{
			if (i + 1 < nArgs) {
				INT iCmdLangID;
				fSuccess = ::StrToIntEx(
					szArgs[i+1], 
					STIF_SUPPORT_HEX, 
					&iCmdLangID);
				if (fSuccess) {
					cmdLangID = (LANGID) iCmdLangID;
				}
				break;
			}
		}
	}
	if (NULL != szArgs) {
		HGLOBAL hNull = ::GlobalFree((HGLOBAL)szArgs);
		ATLASSERT(NULL == hNull);
	}

	return cmdLangID;
}

LANGID 
pGetPreferredLangID(LPCTSTR lpszCmdLine)
{
	LANGID wPref = 0;
	wPref = pGetCmdLineLangID(lpszCmdLine);
	if (0 == wPref) {
		DWORD dwConfigLangID = 0;
		BOOL fSuccess = pGetAppConfigValue(_T("Language"), &dwConfigLangID);
		if (fSuccess) {
			wPref = (LANGID) dwConfigLangID;
		}
	}
	return wPref;
}
