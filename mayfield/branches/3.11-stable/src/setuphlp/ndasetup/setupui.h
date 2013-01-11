#pragma once

static UINT TASKRET_CONTINUE = 0;
static UINT TASKRET_EXIT = 1;

struct ISetupUI
{
	virtual BOOL HasUserCanceled() = 0;
	virtual VOID SetActionText(LPCTSTR szActionText) = 0;
	virtual VOID SetActionText(UINT uStringID) = 0;
	virtual VOID SetBannerText(LPCTSTR szBannerText) = 0;
	virtual VOID SetBannerText(UINT uStringID) = 0;
	virtual VOID InitProgressBar(ULONG ulProgressMax) = 0;
	virtual VOID SetProgressBar(ULONG ulProgress) = 0;
	virtual HWND GetCurrentWindow() = 0;
	virtual VOID ShowProgressBar(BOOL fShow = TRUE) = 0;
	virtual VOID NotifyTaskDone(UINT uiRetCode = TASKRET_CONTINUE) = 0;
	virtual VOID NotifyFatalExit(UINT uiRetCode) = 0;

	virtual VOID SetPostExecuteFile(LPCTSTR szFileName) = 0;

	virtual INT_PTR PostMessageBox(LPCTSTR szText, UINT uiType) = 0;
	virtual INT_PTR PostMessageBox(UINT uiTextID, UINT uiType) = 0;
	virtual INT_PTR PostErrorMessageBox(DWORD dwError, UINT uiErrorID, UINT uiType = MB_OK | MB_ICONERROR) = 0;
	virtual INT_PTR PostErrorMessageBox(DWORD dwError, LPCTSTR szText, UINT uiType = MB_OK | MB_ICONERROR) = 0;
};
