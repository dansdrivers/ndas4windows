#pragma once
#include <commctrl.h>
#include "ndasuser.h"
#include "editchainpaste.h"

class CRegisterDeviceDlg : 
	public CDialogImpl<CRegisterDeviceDlg>,
	public CWinDataExchange<CRegisterDeviceDlg>
{
	CEdit m_wndName;
	CEdit m_wndStringIDs[4];
	CEdit m_wndStringKey;
	CButton m_wndEnableOnRegister;
	CButton m_wndRegister;

	INT m_iEnableDevice;
	BOOL m_bWritableRegister;

	BOOL m_bValidId;
	BOOL m_bValidDeviceName;

	CEditChainPaste m_wndPasteChains[5];

	CString m_strDeviceIDs[4];

	CString m_strDeviceName;
	CString m_strDeviceId;
	CString m_strDeviceKey;

	// internal implementations
	BOOL IsValidDeviceStringIdKey();

public:
	enum { IDD = IDD_REGISTER_DEVICE };

	LPCTSTR GetDeviceStringId();
	LPCTSTR GetDeviceStringKey();
	LPCTSTR GetDeviceName();

	CRegisterDeviceDlg();

	BEGIN_DDX_MAP(CRegisterDeviceDlg)
		DDX_TEXT(IDC_DEV_ID_1,m_strDeviceIDs[0])
		DDX_TEXT(IDC_DEV_ID_2,m_strDeviceIDs[1])
		DDX_TEXT(IDC_DEV_ID_3,m_strDeviceIDs[2])
		DDX_TEXT(IDC_DEV_ID_4,m_strDeviceIDs[3])
		DDX_TEXT(IDC_DEV_KEY,m_strDeviceKey)
		DDX_TEXT(IDC_DEV_NAME, m_strDeviceName)
		DDX_CHECK(IDC_ENABLE_DEVICE,m_iEnableDevice)
	END_DDX_MAP()

	BEGIN_MSG_MAP_EX(CRegisterDeviceDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER_EX(IDC_DEV_ID_1,EN_CHANGE, OnDeviceIdChange)
		COMMAND_HANDLER_EX(IDC_DEV_ID_2,EN_CHANGE, OnDeviceIdChange)
		COMMAND_HANDLER_EX(IDC_DEV_ID_3,EN_CHANGE, OnDeviceIdChange)
		COMMAND_HANDLER_EX(IDC_DEV_ID_4,EN_CHANGE, OnDeviceIdChange)
		COMMAND_HANDLER_EX(IDC_DEV_KEY,EN_CHANGE, OnDeviceIdChange)
		COMMAND_HANDLER_EX(IDC_DEV_NAME, EN_CHANGE, OnDeviceNameChange)
		COMMAND_ID_HANDLER(IDC_REGISTER, OnRegister)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		CHAIN_MSG_MAP_MEMBER(m_wndPasteChains[0])
		CHAIN_MSG_MAP_MEMBER(m_wndPasteChains[1])
		CHAIN_MSG_MAP_MEMBER(m_wndPasteChains[2])
		CHAIN_MSG_MAP_MEMBER(m_wndPasteChains[3])
		CHAIN_MSG_MAP_MEMBER(m_wndPasteChains[4])
	END_MSG_MAP()

	LRESULT OnKeyDown(UINT uCode, int nCtrlID, HWND hwndCtrl)
	{
		ATLTRACE(TEXT("OnKeyDown uCode = %d, nCtrlID = %d, hwndCtrl = %d\n"), 
			uCode, nCtrlID, hwndCtrl);
		SetMsgHandled(FALSE);
		return 0;
	}

	LRESULT OnDeviceNameChange(UINT uCode, int nCtrlID, HWND hwndCtrl);
	LRESULT OnDeviceIdChange(UINT uCode, int nCtrlID, HWND hwndCtrl);
	VOID OnChar_DeviceId(TCHAR ch, UINT nRepCnt, UINT nFlags);
	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
	LRESULT OnRegister(WORD, WORD wID, HWND, BOOL&);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	BOOL UpdateRegisterButton();
	BOOL GetEnableOnRegister();
};