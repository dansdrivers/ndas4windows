#pragma once
#include "resource.h"
#include <ndas/ndascomm.h>
#include <ndas/ndasuser.h>
#include "deviceselectiondlg.h"

class CMainDlg : public CDialogImpl<CMainDlg>
{
public:

	enum { IDD = IDD_MAINDLG };

	BEGIN_MSG_MAP_EX(CMainDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER_EX(IDC_SELECT, OnCmdSelect)
		COMMAND_ID_HANDLER_EX(IDC_ABOUT, OnCmdAbout)
		COMMAND_ID_HANDLER_EX(IDC_CHANGE, OnCmdChange)
		COMMAND_HANDLER_EX(IDC_NEW_STANDBY_MIN, EN_SETFOCUS, OnStandbyMinSetFocus)
		COMMAND_RANGE_HANDLER_EX(IDOK, IDNO, OnCloseCmd)
	END_MSG_MAP()

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
	void OnCloseCmd(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/)
	{
		EndDialog(wID);
	}

	void OnCmdSelect(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/);
	void OnCmdChange(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/);
	void OnCmdAbout(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/);
	void OnStandbyMinSetFocus(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hWndCtl*/);

private:

	HMODULE m_NdasUserModule;
	CEdit m_selectedDeviceText;
	CButton m_selectButton;
	CStatic m_currentSettingText;
	CButton m_newStandbyAfterButton;
	CEdit m_newStandbyAfterEdit;
	CUpDownCtrl m_newStandbyAfterUpDown;
	CButton m_newStandbyOffButton;
	CButton m_newStandbyChangeButton;

	CDeviceSelectionDlg::DLG_PARAM m_sdparam;

	void EnableChangeButtons(BOOL Enable);
	BOOL RefreshSettings();
	BOOL ChangeSettings();
};

