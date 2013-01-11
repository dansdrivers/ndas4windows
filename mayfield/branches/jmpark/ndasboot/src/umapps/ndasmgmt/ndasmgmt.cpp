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
#include "muisel.h"
#include "argv.h"
#include "adminprivs.h"
#include "ndasmgmt_appiniterr.h"
#include <xtl/xtlautores.h>

#ifndef HWND_MESSAGE
#define HWND_MESSAGE     ((HWND)-3)
#endif

CAppModule _Module;

namespace ndasmgmt
{
	LANGID CurrentUILangID = 0;
}

namespace
{
	struct CommandLineInfo
	{
		bool Startup;
		bool Exit;
		bool Restart;
		bool Silent;
		LPCTSTR NifFile;
		LANGID LangId;
	};

	LANGID GetConfigLangId();
	BOOL ParseCommandLine(CommandLineInfo& CmdInfo, LPCTSTR lpCmdLine);
	BOOL IsValidNdasUserDll();
	BOOL LoadResourceModule(LANGID wLangID);
	BOOL LoadResourceModule(LPCTSTR szFileName, LANGID wLangID);
	INT_PTR ShowAppInitErrMessage(UINT nMessageID, UINT nTitleID = IDS_APP_INIT_ERROR_TITLE);
	int Run(LPTSTR lpstrCmdLine = NULL, int nCmdShow = SW_SHOWDEFAULT);
}

int 
WINAPI 
_tWinMain(
	HINSTANCE hInstance, 
	HINSTANCE /*hPrevInstance*/, 
	LPTSTR lpstrCmdLine, 
	int nCmdShow)
{
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
	//
	// Get Command Parameters
	//
	CommandLineInfo cmdinfo = {0};
	if (!ParseCommandLine(cmdinfo, lpstrCmdLine))
	{
		ShowAppInitErrMessage(IDS_APP_INIT_ERROR_OUT_OF_MEMORY);
		return ERROR_OUTOFMEMORY; /* 14 */
	}

	//
	// Application Configuration Initialization
	//
	InitAppConfig();

	//
	// See if there are other instances in the current session
	//
	CSingletonApp sapp;
	if (!sapp.Initialize(NDASMGMT_INST_UID))
	{
		ShowAppInitErrMessage(IDS_APP_INIT_ERROR_OUT_OF_MEMORY);
		return ERROR_OUTOFMEMORY; /* 14 */
	}

	if (!sapp.IsFirstInstance())
	{
		CInterAppMessenger appmsg;
		if (!appmsg.Initialize(NDASMGMT_INST_UID))
		{
			ShowAppInitErrMessage(IDS_APP_INIT_ERROR_OUT_OF_MEMORY);
			return ERROR_OUTOFMEMORY; /* 14 */
		}
		if (cmdinfo.Exit || cmdinfo.Restart)
		{
			// wait for ten-seconds
			if (appmsg.PostMessage(INTERAPPMSG_EXIT, 0) &&
				sapp.WaitOtherInstances(10000))
			{
				// the other application is killed by now, 
				// and I have the ownership.
				// returning this application will abandon my ownership
				if (cmdinfo.Exit) return 0;
			}
			else
			{
				// error
				return 1;
			}
		}
		else
		{
			// otherwise, pop up the balloon where he is!
			(void) appmsg.PostMessage(INTERAPPMSG_POPUP, 0);
			return 0;
		}
	}
	else
	{
		if (cmdinfo.Exit)
		{
			return 0;
		}
	}

	//
	// command line override the registry settings
	//
	LANGID wPrefLangID = (cmdinfo.LangId > 0) ? cmdinfo.LangId : GetConfigLangId(); 
	if (!LoadResourceModule(wPrefLangID)) 
	{
		return 255;
	}

	//
	// NDASUSER.DLL Version Check
	//
	if (!IsValidNdasUserDll()) 
	{
		ShowAppInitErrMessage(IDS_ERROR_INVALID_NDASUSER, IDS_MAIN_TITLE);
		return 255;
	}

	//
	// Privilege Check
	//
	if (!CheckForAdminPrivs()) 
	{
		if (cmdinfo.Startup) 
		{
			//
			// If the application is started from the Programs - Startup 
			// Do not show error message
			//
			// Setup should set this shortcut to run with /startup
			// for this feature
			//
		}
		else 
		{
			ShowAppInitErrMessage(IDS_ERROR_ADMIN_PRIV_REQUIRED, IDS_MAIN_TITLE);
		}
		return ERROR_ACCESS_DENIED;
	}

	//
	// Let's roll the main window
	//
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	int nRet = 0;

	CMainFrame wndMain;
	if(NULL != wndMain.Create(NULL, CRect(0,0,0,0), _T("ndasmgmt"))) 
	{
		// wndMain.ShowWindow(nCmdShow);  // SW_HIDE // nCmdShow;
		wndMain.ShowWindow(SW_HIDE);
		wndMain.UpdateWindow();
		nRet = theLoop.Run();
	}
	else
	{
		ATLTRACE("Main window creation failed!\n");
		// Showing the message may also fail!
		ShowAppInitErrMessage(IDS_ERROR_CREATE_WINDOW, IDS_MAIN_TITLE);
		nRet = ERROR_OUTOFMEMORY;
	}

	_Module.RemoveMessageLoop();

	return nRet;
}


LANGID 
GetConfigLangId()
{
	DWORD configLangId = 0;
	BOOL fSuccess = pGetAppConfigValue(_T("Language"), &configLangId);
	if (fSuccess) 
	{
		return static_cast<LANGID>(configLangId);
	}
	return 0;
}

BOOL
ParseCommandLine(CommandLineInfo& CmdInfo, LPCTSTR lpCmdLine)
{
	int argc = 0;
	LPTSTR *argv = ::CommandLineToArgv(GetCommandLine(), &argc);
	if (NULL == argv)
	{
		return FALSE;
	}
	XTL::AutoLocalHandle auto_argv = argv;
	for (int i = 0; i < argc; ++i)
	{
		if (0 == ::lstrcmpi(_T("/startup"), argv[i]) || 
			0 == ::lstrcmpi(_T("-startup"), argv[i]))
		{
			CmdInfo.Startup = true;
		}
		else if (0 == ::lstrcmpi(_T("/restart"), argv[i]) || 
			0 == ::lstrcmpi(_T("-restart"), argv[i]))
		{
			CmdInfo.Restart = true;
		}
		else if (0 == ::lstrcmpi(_T("/exit"), argv[i]) || 
			0 == ::lstrcmpi(_T("-exit"), argv[i]))
		{
			CmdInfo.Exit = true;
		}
		else if (0 == ::lstrcmpi(_T("/l"), argv[i]) || 
			0 == ::lstrcmpi(_T("-l"), argv[i]) ||
			0 == ::lstrcmpi(_T("/lang"), argv[i]) || 
			0 == ::lstrcmpi(_T("-lang"), argv[i]) ||
			0 == ::lstrcmpi(_T("/language"), argv[i]) ||
			0 == ::lstrcmpi(_T("-language"), argv[i]))
		{
			if (i + 1 < argc)
			{
				++i;
				int value = 0;
				if (::StrToIntEx(argv[i], STIF_SUPPORT_HEX, &value))
				{
					CmdInfo.LangId = static_cast<LANGID>(value);
				}
			}
		}
		else if (0 == ::lstrcmpi(_T("/s"), argv[i]) ||
			0 == ::lstrcmpi(_T("-s"), argv[i]))
		{
			CmdInfo.Silent = true;
		}
		else if (0 == ::lstrcmpi(_T("/import"), argv[i]) ||
			0 == ::lstrcmpi(_T("-import"), argv[i]))
		{
			if (i + 1 < argc)
			{
				++i;
				CmdInfo.NifFile = argv[i];
			}
		}
	}
	return TRUE;
}


BOOL
IsValidNdasUserDll()
{
	DWORD version = ::NdasGetAPIVersion();

	if (NDASUSER_API_VERSION_MAJOR != LOWORD(version) ||
		NDASUSER_API_VERSION_MINOR != HIWORD(version)) 
	{
		return FALSE;
	}

	return TRUE;
}

INT_PTR
ShowAppInitErrMessage(UINT nMessageID, UINT nTitleID)
{
	TCHAR szMessage[APP_INIT_MAX_MESSAGE_LEN] = {0};
	TCHAR szTitle[APP_INIT_MAX_TITLE_LEN] = {0};

	HINSTANCE hAppInst = ATL::_AtlBaseModule.GetResourceInstance();

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

	return ::MessageBox(
		::GetDesktopWindow(),
		szMessage,
		szTitle,
		MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
}

BOOL
LoadResourceModule(LPCTSTR szFileName, LANGID wLangID)
{
	HINSTANCE hResInst = ::LoadLibrary(szFileName);

	if (NULL == hResInst) 
	{
		return FALSE;
	}

	HINSTANCE hPrevResInst = ATL::_AtlBaseModule.SetResourceInstance(hResInst);
	::InitMUILanguage(wLangID);
	ndasmgmt::CurrentUILangID = wLangID;

	return TRUE;
}

BOOL
LoadResourceModule(LANGID wPreferred)
{
	//
	// First to find the available resources
	// from the application directory
	//

	DWORD_PTR cbResDlls = ::NuiCreateAvailResourceDLLsInModuleDirectory(
		NULL, NULL, 0, NULL);
	if (cbResDlls == 0) 
	{
		ShowAppInitErrMessage(IDS_APP_INIT_ERROR_NO_RESOURCE);
		return FALSE;
	}

	LPVOID lpResDlls = ::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, cbResDlls);
	if (NULL == lpResDlls) 
	{
		ShowAppInitErrMessage(IDS_APP_INIT_ERROR_OUT_OF_MEMORY);
		return FALSE;
	}

	XTL::AutoProcessHeap autoResDlls = lpResDlls;
	PNUI_RESDLL_INFO pResDlls = static_cast<PNUI_RESDLL_INFO>(lpResDlls);

	cbResDlls = ::NuiCreateAvailResourceDLLsInModuleDirectory(
		NULL, NULL, cbResDlls, pResDlls);

	if (0 == cbResDlls) 
	{
		ShowAppInitErrMessage(IDS_APP_INIT_ERROR_NO_RESOURCE);
		return FALSE;
	}

	DWORD nAvailLangID = 0;
	for (PNUI_RESDLL_INFO pCur = pResDlls; 
		NULL != pCur;
		pCur = pCur->pNext, ++nAvailLangID)
	{
		ATLTRACE("Available Resource: %d(%08X) %ws\n", 
			pCur->wLangID,
			pCur->wLangID,
			pCur->lpszFilePath);
	}

	XTL::AutoProcessHeap autoAvailLang = ::HeapAlloc(
		::GetProcessHeap(), 
		0,
		sizeof(LANGID) * nAvailLangID);

	if (autoAvailLang.IsInvalid()) 
	{
		ShowAppInitErrMessage(IDS_APP_INIT_ERROR_OUT_OF_MEMORY);
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
	if (0 == wPreferred) 
	{
		wUserLangID = ::NuiGetCurrentUserUILanguage();
		ATLTRACE("Current User LangID: %d\n", wUserLangID);
	}
	else 
	{
		wUserLangID = wPreferred;
	}

	LANGID wCurLangID = ::NuiFindMatchingLanguage(
		wUserLangID, 
		nAvailLangID,
		pwAvailLangID);

	ATLTRACE("Current LangID: %d\n", wCurLangID);

	// If cannot find the match, try to find English Resource.
	static CONST LANGID EnglishLANGID = 0x409;
	if (0 == wCurLangID) 
	{
		wCurLangID = ::NuiFindMatchingLanguage(
			EnglishLANGID,
			nAvailLangID,
			pwAvailLangID);
	}

	// If still fails, load the first language
	if (0 == wCurLangID) 
	{
		wCurLangID = pwAvailLangID[0];
	}

	LPTSTR lpszResDll = NULL;
	i = 0;
	for (PNUI_RESDLL_INFO pCur = pResDlls; 
		NULL != pCur;
		pCur = pCur->pNext, ++i)
	{
		if (pCur->wLangID == wCurLangID) 
		{
			lpszResDll = pCur->lpszFilePath;
			break;
		}
	}

	ATLTRACE("Resource DLL: %ws\n", lpszResDll);
	ATLASSERT(i < nAvailLangID);

	return LoadResourceModule(lpszResDll, wCurLangID);
}

}
