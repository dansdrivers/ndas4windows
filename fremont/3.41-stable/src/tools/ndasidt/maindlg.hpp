#pragma once
#include <atlwin.h>
#include "resource.h"
#include "editchained.h"
#include "edithex.h"

class CMainDialog : public ATL::CDialogImpl<CMainDialog>
{
public:
	enum { IDD = IDD_MAINDIALOG };

	BEGIN_MSG_MAP_EX(CMainDialog)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_DESTROY(OnDestroy)
		MSG_WM_CLOSE(OnClose)
		COMMAND_ID_HANDLER_EX(IDOK, OnCmdOK)
		COMMAND_ID_HANDLER_EX(IDC_PARSE, OnCmdParse)
		COMMAND_ID_HANDLER_EX(IDCLOSE, OnCmdClose)
		COMMAND_ID_HANDLER_EX(IDC_SET_DEFAULT_KEYS, OnCmdSetDefaultKeys)
		COMMAND_ID_HANDLER_EX(IDC_SET_DEFAULT_VENDOR, OnCmdSetDefaultVendor)
		COMMAND_HANDLER_EX(IDC_USE_CUSTOMKEYS, BN_CLICKED, OnCustomKeyClick)
		COMMAND_ID_HANDLER_EX(IDC_COPY_NDASID, OnCmdCopyNdasId)
		COMMAND_ID_HANDLER_EX(IDC_COPY_MACADDRESS, OnCmdCopyMacAddress)
		COMMAND_ID_HANDLER_EX(IDC_CLIPBOARD, OnCmdClipboard)		
		COMMAND_CODE_HANDLER_EX(EN_UPDATE, OnEditUpdate)
		COMMAND_CODE_HANDLER_EX(EN_CHANGE, OnEditChange)
	END_MSG_MAP()

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
	void OnDestroy();
	void OnClose();
	void CloseDialog(int retValue);

	void OnCmdOK(UINT wNotifyCode, int wID, HWND hWndCtl);
	void OnCmdParse(UINT wNotifyCode, int wID, HWND hWndCtl);
	void OnCmdClose(UINT wNotifyCode, int wID, HWND hWndCtl);
	void OnCmdSetDefaultKeys(UINT wNotifyCode, int wID, HWND hWndCtl);
	void OnCmdSetDefaultVendor(UINT wNotifyCode, int wID, HWND hWndCtl);

	void OnEditUpdate(UINT wNotifyCode, int wID, HWND hWndCtl);
	void OnEditChange(UINT wNotifyCode, int wID, HWND hWndCtl);

	void OnCustomKeyClick(UINT, int, HWND);

	void OnCmdCopyNdasId(UINT, int, HWND);
	void OnCmdCopyMacAddress(UINT, int, HWND);
	void OnCmdClipboard(UINT, int, HWND);	

private:

	CChainedEdit m_wndAddresses[6];
	CChainedEdit m_wndVID;
	CChainedEdit m_wndReserved[2];
	CChainedEdit m_wndSeed;
	CChainedEdit m_wndKeys1[8];
	CChainedEdit m_wndKeys2[8];
	CChainedEdit m_wndNdasId[4];
	CChainedEdit m_wndWriteKey;
	CEdit m_wndSerialNumberDigit;
	
	CButton m_wndUseCustomKeys;
	CButton m_wndUseDefaultKeys;
	CButton m_wndUseDefaultVendor;

	CEditHexOnly m_hexOnlyEdit[36];


	void _SetDefaultVendor();
	void _SetDefaultKeys();
	void _EnableEditKeys(BOOL fEnable);
	BOOL _GetNdasIdString(CString &s);
};

