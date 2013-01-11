// ndasbind.cpp : main source file for ndasbind.exe
//

#include "stdafx.h"

#include "resource.h"

#include "nbaboutdlg.h"
#include "nbmainfrm.h"
//#include "nbmainfrmsplit.h"
#include "muisel.h"
#include "appconf.h"
#include "adminprivs.h"
#include "argv.h"
#include <strsafe.h>
#include <xtl/xtlautores.h>
#include "ndasbind_appiniterr.h"
#include "ndasbind.h"
#include "singleinst.h"
#include "ndas/ndascomm.h"

CAppModule _Module;

namespace
{
	struct CommandLineInfo
	{
		bool Exit;
		bool Restart;
		LANGID LangId;
	};
	BOOL ParseCommandLine(CommandLineInfo& CmdInfo, LPCTSTR lpCmdLine);

	LANGID GetConfigLangId();
	BOOL IsValidNdasUserDll();
	BOOL IsValidNdasCommDll();
	BOOL LoadResourceModule(LANGID wLangID);
	BOOL LoadResourceModule(LPCTSTR szFileName, LANGID wLangID);
	INT_PTR ShowAppInitErrMessage(UINT nMessageID, UINT nTitleID = IDS_APP_INIT_ERROR_TITLE);

	int Run(LPTSTR lpstrCmdLine = NULL, int nCmdShow = SW_SHOWDEFAULT);
}

int 
WINAPI 
_tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{
	// If you are running on NT 4.0 or higher you can use the following call instead to 
	// make the EXE free threaded. This means that calls come in on a random RPC thread.
	//	HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
	HRESULT hRes = ::CoInitialize(NULL);
	ATLASSERT(SUCCEEDED(hRes));

	// this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
	::DefWindowProc(NULL, 0, 0, 0L);

	::AtlInitCommonControls(ICC_COOL_CLASSES | ICC_BAR_CLASSES);	
	// add flags to support other controls

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	int ret = Run(lpstrCmdLine, nCmdShow);

	_Module.Term();
	::CoUninitialize();

	return ret;
}

namespace
{

int Run(LPTSTR lpstrCmdLine /*= NULL*/, int nCmdShow /*= SW_SHOWDEFAULT*/)
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

	BOOL fSuccess = InitSystemCfg();
	if (!fSuccess) {
		ShowAppInitErrMessage(IDS_APP_INIT_ERROR_OUT_OF_MEMORY);
		return ::GetLastError();
	}

	fSuccess = InitUserCfg();
	if (!fSuccess) {
		ShowAppInitErrMessage(IDS_APP_INIT_ERROR_OUT_OF_MEMORY);
		return ::GetLastError();
	}

	//
	// See if there are other instances in the current session
	//
	CSingletonApp sapp;
	if (!sapp.Initialize(APP_INST_UID))
	{
		ShowAppInitErrMessage(IDS_APP_INIT_ERROR_OUT_OF_MEMORY);
		return ERROR_OUTOFMEMORY; /* 14 */
	}

	if (!sapp.IsFirstInstance())
	{
		CInterAppMessenger appmsg;
		if (!appmsg.Initialize(APP_INST_UID))
		{
			ShowAppInitErrMessage(IDS_APP_INIT_ERROR_OUT_OF_MEMORY);
			return ERROR_OUTOFMEMORY; /* 14 */
		}
		// otherwise, pop up the balloon where he is!
		(void) appmsg.PostMessage(NDASBIND_INSTMSG_POPUP, 0);
		return 0;
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
		ShowAppInitErrMessage(IDS_ERROR_INVALID_NDASUSER);
		return 255;
	}

	//
	// NDASCOMM.DLL Version Check
	//
	if (!IsValidNdasCommDll()) 
	{
		ShowAppInitErrMessage(IDS_ERROR_INVALID_NDASCOMM);
		return 255;
	}

	//
	// not need to initialize each time.
	//
	fSuccess = ::NdasCommInitialize();
	if (!fSuccess) 
	{
		return 254;
	}

	int ret = 1;

	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	CMainFrame wndMain;

	if(wndMain.CreateEx() != NULL)
	{
		wndMain.ShowWindow(nCmdShow);
		wndMain.UpdateWindow();
		ret = theLoop.Run();
	}

	_Module.RemoveMessageLoop();

	fSuccess = ::NdasCommUninitialize();
	ATLASSERT(fSuccess);

	return ret;
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
		if (0 == ::lstrcmpi(_T("/restart"), argv[i]) || 
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
	}
	return TRUE;
}

LANGID 
GetConfigLangId()
{
	//
	// Use the settings of ndasmgmt for now.
	//
	const TCHAR* NDASMGMT_KEY_NAME = _T("Software\\NDAS\\ndasmgmt");

	DWORD configLangId = 0;
	CRegKey regkey;
	LONG ret = regkey.Open(HKEY_CURRENT_USER, NDASMGMT_KEY_NAME, KEY_READ);
	if (ERROR_SUCCESS == ret)
	{
		ret = regkey.QueryDWORDValue(_T("Language"), configLangId);
		if (ERROR_SUCCESS != ret)
		{
			ret = regkey.Open(HKEY_LOCAL_MACHINE, NDASMGMT_KEY_NAME, KEY_READ);
			if (ERROR_SUCCESS == ret)
			{
				ret = regkey.QueryDWORDValue(_T("Language"), configLangId);
			}
			else
			{
				configLangId = 0;
			}
		}
	}
	
	return static_cast<LANGID>(configLangId);
}

BOOL 
IsValidNdasCommDll()
{
	DWORD version = ::NdasCommGetAPIVersion();

	if (NDASCOMM_API_VERSION_MAJOR != LOWORD(version) ||
		NDASCOMM_API_VERSION_MINOR != HIWORD(version)) 
	{
		return FALSE;
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
		ATLTRACE(_T("Available Resource: %d(%08X) %s\n"), 
			pCur->wLangID,
			pCur->wLangID,
			pCur->lpszFilePath);
	}

	XTL::AutoProcessHeap autoAvailLang = ::HeapAlloc(
		::GetProcessHeap(), 
		0,
		sizeof(LANGID) * nAvailLangID);

	if (NULL == (LPVOID) autoAvailLang) 
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
		ATLTRACE(_T("Current User LangID: %d\n"), wUserLangID);
	}
	else 
	{
		wUserLangID = wPreferred;
	}

	LANGID wCurLangID = ::NuiFindMatchingLanguage(
		wUserLangID, 
		nAvailLangID,
		pwAvailLangID);

	ATLTRACE(_T("Current LangID: %d\n"), wCurLangID);

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

	ATLTRACE(_T("Resource DLL: %s\n"), lpszResDll);
	ATLASSERT(i < nAvailLangID);

	return LoadResourceModule(lpszResDll, wCurLangID);
}

}

