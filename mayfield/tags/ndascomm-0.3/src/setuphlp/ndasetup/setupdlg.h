// setupdlg.h : interface of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once
#include "setupui.h"
#include "menubtn.h"
#include "setuptask.h"
#include "ndas/ndupdate.h"

class CSetupTask;

static CONST INT_PTR SETUPDLG_RET_EXECUTEINSTALLER = 0xC0000001;
static CONST INT_PTR SETUPDLG_RET_EXECUTEUPDATER = 0xC0000002;

#define WM_TASK_DONE	(WM_USER + 0xCFE0)

class CSetupInitDlg : 
	public CDialogImpl<CSetupInitDlg>,
	public ISetupUI
{
	WTLEX::CMenuButton m_wndLangMenuButton;

	CButton m_wndCheckUpdate;
	CButton m_wndOK;
	CButton m_wndCancel;

	CProgressBarCtrl m_wndProgress;
	CStatic m_wndActionText;
	CStatic m_wndBannerText;

	CString m_szInstallCmdLine;

	CSetupTask* m_pCurrentTask;

	enum SetupTasks {
		ST_NONE,
		ST_INIT,
		ST_CHECK_UPDATE,
		ST_CHECK_MSI,
		ST_CHECK_UPGRADE,
		ST_CACHE_MSI,
		ST_INSTALL
	};


	SetupTasks m_CurrentSetupTask;

	TCHAR m_szPackagePath[MAX_PATH];
	TCHAR m_szCachedPackagePath[MAX_PATH];
	TCHAR m_szLogFilePath[MAX_PATH];
	TCHAR m_szPostExecFile[MAX_PATH];

	BOOL m_fDisableCheckUpdate;
	BOOL m_fCheckUpdate;
	TCHAR m_szUpdateURL[MAX_PATH];
	NDUPDATE_SYSTEM_INFO m_SysInfo;

	BOOL cpGetSettings();

public:
	enum { IDD = IDD_INIT_SETUP };

	BEGIN_MSG_MAP_EX(CSetupInitDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER_EX(IDOK, OnOK)
		COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER_EX(ID_INSTALL_ENU, OnChangeLanguage)
		COMMAND_ID_HANDLER_EX(ID_INSTALL_KOR, OnChangeLanguage)
		MESSAGE_HANDLER_EX(WM_TASK_DONE,OnTaskDone)
		REFLECT_NOTIFICATIONS()
	END_MSG_MAP()

	CSetupInitDlg();

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
	VOID OnOK(UINT uNotifyCode, INT wID, HWND hWndCtrl);
	VOID OnCancel(UINT uNotifyCode, INT wID, HWND hWndCtrl);

	VOID OnChangeLanguage(UINT uNotifyCode, INT wID, HWND hWndCtrl);

	LRESULT OnTaskDone(UINT uMsg, WPARAM wParam, LPARAM lParam);

	VOID DoInit();
	VOID DoCheckMsi();
	VOID DoCheckUpdate();
	VOID DoCheckUpgrade();
	VOID DoCacheMsi();

	UINT_PTR ExecuteInstaller();
	UINT_PTR ExecuteUpdater();

	LPCTSTR GetPackagePath();
	LPCTSTR GetCachedPackagePath();
	LPCTSTR GetLogFilePath();
	BOOL SetCachedPackagePath(LPCTSTR szPath);

	//
	// ISetupUI Interface
	//
	BOOL HasUserCanceled();
	VOID SetActionText(LPCTSTR szActionText);
	VOID SetActionText(UINT uiActionTextID);
	VOID SetBannerText(LPCTSTR szBannerText);
	VOID SetBannerText(UINT uiBannerTextID);
	VOID InitProgressBar(ULONG ulProgressMax);
	VOID SetProgressBar(ULONG ulProgress);
	HWND GetCurrentWindow();
	VOID ShowProgressBar(BOOL fShow = TRUE);
	VOID NotifyTaskDone(UINT uiRetCode = TASKRET_CONTINUE);
	VOID NotifyFatalExit(UINT uiRetCode);
	VOID SetPostExecuteFile(LPCTSTR szFileName);

	virtual INT_PTR PostMessageBox(LPCTSTR szText, UINT uiType);
	virtual INT_PTR PostMessageBox(UINT uiTextID, UINT uiType);
	virtual INT_PTR PostErrorMessageBox(DWORD dwError, UINT uiErrorID, UINT uiType = MB_OK | MB_ICONERROR);
	virtual INT_PTR PostErrorMessageBox(DWORD dwError, LPCTSTR szText, UINT uiType = MB_OK | MB_ICONERROR);
};
