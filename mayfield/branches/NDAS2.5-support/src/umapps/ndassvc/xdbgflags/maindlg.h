// MainDlg.h : interface of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

class CMainDlg : public CDialogImpl<CMainDlg>
{
	CCheckListViewCtrl m_lvModules;

	CButton m_wndAlways;
	CButton m_wndError;
	CButton m_wndWarning;
	CButton m_wndInfo;
	CButton m_wndTrace;
	CButton m_wndNoise;
	CButton m_wndCustom;
	CEdit m_wndCustomValue;

	DWORD m_dwLevel;
	DWORD m_dwFlags;

public:
	enum { IDD = IDD_MAINDLG };

	BEGIN_MSG_MAP_EX(CMainDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(ID_APP_ABOUT, OnAppAbout)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER_EX(ID_APPLY, OnApply)
		COMMAND_ID_HANDLER_EX(ID_REFRESH, OnRefresh)

	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	void OnApply(UINT uMsg, int wID, HWND hWndCtl);
	void OnRefresh(UINT uMsg, int wID, HWND hWndCtl);

	void UpdateModuleList(DWORD dwFlags);
	
	void SetOutputLevelButtonStatus(DWORD dwValue);
	DWORD GetOutputLevelButtonStatus();
	void SetModuleCheckList(DWORD dwFlags);
	DWORD GetModuleCheckList();

	void GetRegValues();
	void SetRegValues();
};
