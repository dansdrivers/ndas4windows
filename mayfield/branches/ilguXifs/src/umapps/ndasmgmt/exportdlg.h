#pragma once
#include <atlcrack.h>
#include "resource.h"
#include "ndascls.h"

class CExportDlg : public CDialogImpl<CExportDlg>
{
public:
	enum { IDD = IDD_EXPORT };
	
	BEGIN_MSG_MAP_EX(CExportDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER_EX(IDOK, OnCmdSave)
		COMMAND_ID_HANDLER_EX(IDCLOSE, OnCmdClose)
		COMMAND_ID_HANDLER_EX(IDCANCEL, OnCmdClose)
		COMMAND_HANDLER_EX(IDC_DEVICE_NAME_LIST,CBN_SELCHANGE,OnSelChange)
		COMMAND_HANDLER_EX(IDC_NO_WRITE_KEY,BN_CLICKED,OnNoWriteKeyClick)
		COMMAND_HANDLER_EX(IDC_DEVICEID_4,EN_UPDATE,OnDeviceIdChange)
		COMMAND_HANDLER_EX(IDC_WRITE_KEY,EN_UPDATE,OnDeviceIdChange)
	END_MSG_MAP()

	LRESULT OnInitDialog(HWND hWndCtl, LPARAM lParam);
	void OnCmdSave(UINT, int, HWND);
	void OnCmdClose(UINT, int, HWND);
	void OnSelChange(UINT, int, HWND);
	void OnNoWriteKeyClick(UINT, int, HWND);
	void OnDeviceIdChange(UINT, int, HWND);

private:
	CComboBox m_wndNameList;
	CEdit m_wndDeviceId[4];
	CEdit m_wndWriteKey;
	CEdit m_wndDescription;
	CButton m_wndNoWriteKey;
	CButton m_wndSave;

	ndas::DeviceVector m_devices;
	ndas::DevicePtr m_pDevice;

	TCHAR m_szDeviceId[21];
	TCHAR m_szWriteKey[6];

	void _ResetSelected();
	void _UpdateStates();

	LPCTSTR _GetDeviceId();
	LPCTSTR _GetWriteKey();
};

BOOL 
pExportGetSaveFileName(
	HWND hWndOwner,
	LPTSTR FilePath, 
	DWORD MaxChars);

void 
pNormalizePath(
	LPTSTR FilePath, 
	DWORD MaxChars,
	TCHAR Substitute = _T('_'));
