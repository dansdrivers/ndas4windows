#pragma once
#include <commctrl.h>
#include "ndasuser.h"
#include "editchainpaste.h"

class CRegisterDeviceDialog : 
	public CDialogImpl<CRegisterDeviceDialog>,
	public CWinDataExchange<CRegisterDeviceDialog>
{
	CEdit m_wndName;
	CEdit m_wndStringIDs[4];
	CEdit m_wndStringKey;
	CButton m_wndRegister;

	BOOL m_bWritableRegister;

	CEditChainPaste m_wndPasteChains[4];

	WTL::CString m_strDeviceIDs[4];

	WTL::CString m_strDeviceName;
	WTL::CString m_strDeviceId;
	WTL::CString m_strDeviceKey;

	// internal implementations
	BOOL IsValidDeviceStringIdKey();

public:
	enum { IDD = IDD_REGISTER_DEVICE };

	LPCTSTR GetDeviceStringId();
	LPCTSTR GetDeviceStringKey();
	LPCTSTR GetDeviceName();

	CRegisterDeviceDialog();

	BEGIN_DDX_MAP(CRegisterDeviceDialog)
		DDX_TEXT(IDC_DEV_ID_1,m_strDeviceIDs[0])
		DDX_TEXT(IDC_DEV_ID_2,m_strDeviceIDs[1])
		DDX_TEXT(IDC_DEV_ID_3,m_strDeviceIDs[2])
		DDX_TEXT(IDC_DEV_ID_4,m_strDeviceIDs[3])
		DDX_TEXT(IDC_DEV_KEY,m_strDeviceKey)
		DDX_TEXT(IDC_DEV_NAME, m_strDeviceName)
	END_DDX_MAP()

	BEGIN_MSG_MAP_EX(CRegisterDeviceDialog)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER_EX(IDC_DEV_ID_1,EN_CHANGE, OnDeviceIdChange)
		COMMAND_HANDLER_EX(IDC_DEV_ID_2,EN_CHANGE, OnDeviceIdChange)
		COMMAND_HANDLER_EX(IDC_DEV_ID_3,EN_CHANGE, OnDeviceIdChange)
		COMMAND_HANDLER_EX(IDC_DEV_ID_4,EN_CHANGE, OnDeviceIdChange)
		COMMAND_HANDLER_EX(IDC_DEV_KEY,EN_CHANGE, OnDeviceIdChange)
		COMMAND_ID_HANDLER(IDC_REGISTER, OnRegister)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		CHAIN_MSG_MAP_MEMBER(m_wndPasteChains[0])
		CHAIN_MSG_MAP_MEMBER(m_wndPasteChains[1])
		CHAIN_MSG_MAP_MEMBER(m_wndPasteChains[2])
		CHAIN_MSG_MAP_MEMBER(m_wndPasteChains[3])
	END_MSG_MAP()

	LRESULT OnKeyDown(UINT uCode, int nCtrlID, HWND hwndCtrl)
	{
		ATLTRACE(TEXT("OnKeyDown uCode = %d, nCtrlID = %d, hwndCtrl = %d\n"), 
			uCode, nCtrlID, hwndCtrl);
		SetMsgHandled(FALSE);
		return 0;
	}

	LRESULT OnDeviceIdChange(UINT uCode, int nCtrlID, HWND hwndCtrl);
	void OnChar_DeviceId(TCHAR ch, UINT nRepCnt, UINT nFlags);
	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
	LRESULT OnRegister(WORD, WORD wID, HWND, BOOL&);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	
};