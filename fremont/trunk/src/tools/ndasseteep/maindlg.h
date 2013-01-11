#pragma once
#include "resource.h"
#include <ndas/ndascomm.h>
#include <ndas/ndashear.h>
#include <hash_set>

using namespace std;
using namespace stdext;

class CMainDlg : public CDialogImpl<CMainDlg>
{
public:

	enum { IDD = IDD_MAINDLG };

	BEGIN_MSG_MAP_EX(CMainDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_CLOSE(OnClose)
		COMMAND_ID_HANDLER_EX(IDC_LOAD, OnCmdLoad)
		COMMAND_ID_HANDLER_EX(IDC_ABOUT, OnCmdAbout)
		COMMAND_ID_HANDLER_EX(IDC_UPDATE, OnCmdUpdate)
		COMMAND_ID_HANDLER_EX(IDC_CLEAR_ALL, OnCmdClearAll)
		COMMAND_ID_HANDLER_EX(IDC_CLEAR_SELECTED, OnCmdClearSelected)
		COMMAND_ID_HANDLER_EX(IDC_CHECK_USE_AUTO_DETECTED, OnCmdCheckUseAutoDetected)
		COMMAND_RANGE_HANDLER_EX(IDOK, IDNO, OnCloseCmd)
	END_MSG_MAP()

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
	void OnClose();
	void OnCloseCmd(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/)
	{
		if (INVALID_HANDLE_VALUE != m_logFile) {
			CloseHandle(m_logFile);
			if (!m_logWritten) {
				DeleteFile(m_logFileName);
			}
		}
		NdasHeartbeatUnregisterNotification(m_NdasHearHandle);
		NdasHeartbeatUninitialize();
		EndDialog(wID);
	}

	void OnCmdCheckUseAutoDetected(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/);
	void OnCmdClearAll(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/);
	void OnCmdClearSelected(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/);
	void OnCmdLoad(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/);

	void OnCmdUpdate(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/);
	void OnCmdAbout(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/);


	CListBox m_listBox;

private:

	HMODULE m_NdasUserModule;

	HANDLE m_NdasHearHandle;

	CButton m_autoDetectCheck;
	CButton m_loadButton;
	CEdit m_currentMacText[6];
	CEdit m_currentSuperPasswordText;
	CEdit m_currentUserPasswordText;
	CEdit m_eepromPathText;
	CEdit m_newMacText[6];

	CButton m_dontChangeMacButton;
	CButton m_macIncButton;
	CButton m_udpateButton;
	CEdit m_logText;
	BOOL m_logWritten;

	CEdit m_pnPrefix;
	CEdit m_pnBegin;
	CButton m_pnAutoIncrease;
	CEdit m_pnPostFix;


	const static int m_eepromSize = 1* 1024;
	const static DWORDLONG m_defaultUserPassword = 0x1f4a50731530eabb; // 64 bit value
	const static DWORDLONG m_defaultSuperPassword = 0x3E2B321A4750131E;
//	const static unsigned _int64 m_defaultSeagatePassword = 0x99a26ebc46274152;

	/*
	record all used mac address to prevent mac address duplication
	*/
	hash_set <UINT64> m_usedMacs;
	HANDLE	m_usedMacsFile;


	BYTE	m_eepromImage[m_eepromSize];
	BOOL	m_imageLoaded;
	
	HANDLE	m_logFile;
	TCHAR m_logFileName[256];

	BOOL GetHexFromControl(CEdit& Ctrl, BYTE* value);
	BOOL GetHexFromControl(CListBox& Ctrl, BYTE* value);
	BOOL GetHexFromControl(CEdit& Ctrl, ULONG64* value);

	BOOL UpdateEeprom(BYTE* curMac, BOOL keepMac, BYTE* newMac, ULONG64 superpw, ULONG64 userpw, LPCTSTR szProductNumber);
	VOID WriteLogText(LPCTSTR str);
	VOID WriteLogFile(LPCTSTR str);
	VOID WriteNewIdLog(BYTE* mac);
//	BOOL RefreshSettings();
//	BOOL ChangeSettings();
};

