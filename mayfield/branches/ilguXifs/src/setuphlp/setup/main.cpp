#include "stdafx.h"
#include <shlwapi.h>
#include <xtl/xtlautores.h>
#include "resource.h"
#include "langdlg.h"
#include "preparedlg.h"
#include "winutil.h"
#include "errordlg.h"
#include "sdf.h"
#define DLOAD_USE_SEH
#include "dload_msi.h"

CMyAppModule _Module;

UINT
InstallProduct(
	const SetupDefinition& sdf)
{
	XTL::AutoModuleHandle hMsiDll = LoadLibrary(_T("msi.dll"));
	if (hMsiDll.IsInvalid())
	{
		ATLTRACE(_T("Unable to load msi.dll\n"));
		return ERROR_MOD_NOT_FOUND;
	}

	MsiDll msidll(hMsiDll);

	__try
	{
		INSTALLUILEVEL uilevel = msidll.MsiSetInternalUI(INSTALLUILEVEL_FULL, NULL);
		ATLTRACE("MsiSetInternalUI returned %d (UILEVEL)\n", uilevel);

		if (sdf.UseLog)
		{
			UINT ret = msidll.MsiEnableLog(
				sdf.LogMode, 
				sdf.LogFile, 
				sdf.LogAttribute);
			// Ignore MsiEnableLog error
			ATLTRACE("MsiEnableLog returned %d\n", ret);
		}

		UINT ret = msidll.MsiInstallProduct(
			sdf.GetMsiDatabase(),
			sdf.GetMsiCommandLine());
		ATLTRACE("MsiInstallProduct returned %d\n", ret);

		if (ERROR_SUCCESS != ret &&
			ERROR_SUCCESS_REBOOT_INITIATED != ret &&
			ERROR_SUCCESS_REBOOT_REQUIRED != ret &&
			ERROR_INSTALL_USEREXIT != ret &&
			ERROR_INSTALL_FAILURE != ret)
		{
			LPTSTR lpszError = GetErrorMessage(ret, _Module.m_wResLangId);
			if (NULL != lpszError)
			{
				CErrorDlg::DialogParam param = { ret, lpszError };
				CErrorDlg errorDlg;
				errorDlg.DoModal(
					GetDesktopWindow(), 
					reinterpret_cast<LPARAM>(&param));
				(void) LocalFree(lpszError);
			}
		}

		return ret;
	}
	__except (
		GetExceptionCode() == DLOAD_EXCEPTION_PROC_NOT_FOUND ? 
		EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		ATLTRACE("DLOAD Exception: %08X\n", GetExceptionCode());
		return -1;
	}
}

LPCTSTR
GetResourceFileName(
	LANGID wLangID)
{
	const struct {
		LANGID LangID;
		LPCTSTR ResFileName;
	} Table[] = {
		1042, _T("setupkor.dll"),
		1041, _T("setupjpn.dll"),
		1033, _T("setupenu.dll")
	};
	for (size_t i = 0; i < RTL_NUMBER_OF(Table); ++i)
	{
		if (Table[i].LangID == wLangID)
		{
			return Table[i].ResFileName;
		}
	}
	return NULL;
}

BOOL
GetSdfFileName(
	LPTSTR Buffer, 
	DWORD cchBuffer)
{
	DWORD n = GetModuleFileName(NULL, Buffer, cchBuffer);
	if (0 == n)
	{
		_ASSERTE(FALSE && "GetModuleFileName failed.");
		return FALSE;
	}
	PathRenameExtension(Buffer, _T(".inf"));
	return TRUE;
}

int 
Run(
	LPCTSTR lpstrCmdLine, 
	int nCmdShow)
{
	TCHAR sdfFileName[MAX_PATH] = {0};
	ATLVERIFY( GetSdfFileName(sdfFileName, MAX_PATH) );

	SetupDefinition sdf;
	sdf.Load(sdfFileName);

	LANGID selLangID;
	if (sdf.UseMUI && sdf.ConfirmLanguage)
	{
		CLanguageSelectionDlg langDlg;
		CSimpleArray<LANGID> langIdArray;
		sdf.GetLanguages(langIdArray);
		langDlg.SetLanguages(langIdArray);
		INT_PTR nRet = langDlg.DoModal();
		ATLTRACE("LangSel returned %d\n", nRet);
		if (IDOK != nRet)
		{
			return -1;
		}
		selLangID = langDlg.GetSelectedLangID();
	}
	else 
	{
		selLangID = GetUserDefaultUILanguage();
	}

	if (sdf.UseMUI)
	{
		sdf.SetActiveLanguage(selLangID);
	}

	ATLTRACE("Language %d is selected.\n", selLangID);

	LPCTSTR lpszResFileName = GetResourceFileName(selLangID);

	if (NULL == lpszResFileName)
	{
		_Module.SetResourceInstance(_Module.GetModuleInstance());
	}
	else
	{
		HMODULE hResModule = LoadLibrary(lpszResFileName);
		if (NULL != hResModule)
		{
			HMODULE hOldResModule = _Module.SetResourceInstance(hResModule);
			if (hOldResModule != GetModuleHandle(NULL))
			{
				FreeLibrary(hOldResModule);
			}
			_Module.m_wResLangId = selLangID;
		}
	}

	ULONG RequiredMsiVer = sdf.RequiredMsiVersion;
	if (IsMsiUpgradeNecessary(RequiredMsiVer))
	{
		CPrepareDlg prepDlg;
		INT_PTR nRet = prepDlg.DoModal(
			GetActiveWindow(), 
			reinterpret_cast<LPARAM>(&sdf));
		ATLTRACE("PrepareDlg returned %d\n", nRet);

		if (IDOK != nRet)
		{
		}
	}

	return InstallProduct(sdf);
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
	// This setup boot-strapper requires Windows 2000 or higher
	//
	if (!IsPlatform(VER_PLATFORM_WIN32_NT) ||
		!MinimumWindowsPlatform(5,0,0))
	{
		char szMessage[255], szCaption[255];
		int iChars = LoadStringA(hInstance, IDS_ERR_REQUIRES_WINDOWS2000, szMessage, 255);
		ATLASSERT(iChars > 0);
		iChars = LoadStringA(hInstance, IDS_SETUP, szCaption, 255);
		ATLASSERT(iChars > 0);
		(void) MessageBoxA(NULL, szMessage, szCaption, MB_OK | MB_ICONERROR);
		return -1;
	}

	HRESULT hr = CoInitialize(NULL);
	ATLASSERT(SUCCEEDED(hr));

	// this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
	DefWindowProc(NULL, 0, 0, 0L);

	AtlInitCommonControls(ICC_BAR_CLASSES);	// add flags to support other controls

	hr = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hr));

	int nRet = Run(lpstrCmdLine, nCmdShow);

	_Module.Term();

	CoUninitialize();

	return nRet;
}


