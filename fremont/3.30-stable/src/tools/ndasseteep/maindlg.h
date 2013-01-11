#pragma once
#include "resource.h"
#include <ndas/ndascomm.h>
#include <ndas/ndasuser.h>

class CMainDlg : public CDialogImpl<CMainDlg>
{
public:

	enum { IDD = IDD_MAINDLG };

	BEGIN_MSG_MAP_EX(CMainDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER_EX(IDC_LOAD, OnCmdLoad)
		COMMAND_ID_HANDLER_EX(IDC_ABOUT, OnCmdAbout)
		COMMAND_ID_HANDLER_EX(IDC_UPDATE, OnCmdUpdate)
		COMMAND_RANGE_HANDLER_EX(IDOK, IDNO, OnCloseCmd)
	END_MSG_MAP()

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
	void OnCloseCmd(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/)
	{
		if (INVALID_HANDLE_VALUE != m_logFile) {
			CloseHandle(m_logFile);
		}
		EndDialog(wID);
	}

	void OnCmdLoad(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/);

	void OnCmdUpdate(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/);
	void OnCmdAbout(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/);

private:

	HMODULE m_NdasUserModule;

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

	const static int m_eepromSize = 1* 1024;
	const static DWORDLONG m_defaultPassword = 0x1f4a50731530eabb; // 64 bit value
	const static DWORDLONG m_defaultSuperPassword = 0x3E2B321A4750131E;
//	const static unsigned _int64 m_defaultSeagatePassword = 0x99a26ebc46274152;

	BYTE	m_eepromImage[m_eepromSize];
	BOOL	m_imageLoaded;
	
	HANDLE	m_logFile;
	
	BOOL GetHexFromControl(CWnd& Ctrl, BYTE* value);
	BOOL GetHexFromControl(CWnd& Ctrl, ULONG64* value);

	BOOL UpdateEeprom(BYTE* curMac, BOOL keepMac, BYTE* newMac, ULONG64 superpw, ULONG64 userpw);

//	BOOL RefreshSettings();
//	BOOL ChangeSettings();
};

