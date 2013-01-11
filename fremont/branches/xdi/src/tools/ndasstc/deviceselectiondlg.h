#pragma once
#include <ndas/ndasuser.h>
#include <ndas/ndascomm.h>
#include "resource.h"

class CDeviceSelectionDlg : public CDialogImpl<CDeviceSelectionDlg>
{
public:

	typedef enum _DLG_PARAM_TYPE {
		DLG_PARAM_UNSPECIFIED,
		DLG_PARAM_MANUAL,
		DLG_PARAM_REGISTERED
	} DLG_PARAM_TYPE;

	typedef struct _DLG_PARAM {
		DWORD Size;
		DLG_PARAM_TYPE Type;
		TCHAR NdasDeviceId[NDAS_DEVICE_STRING_ID_LEN + 1];
		TCHAR NdasDeviceKey[NDAS_DEVICE_WRITE_KEY_LEN + 1];
		TCHAR NdasDeviceName[MAX_NDAS_DEVICE_NAME_LEN + 1];
	} DLG_PARAM, *PDLG_PARAM;

	enum { IDD = IDD_DEVICE_SELECTION };

	BEGIN_MSG_MAP_EX(CDeviceSelectionDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		// COMMAND_HANDLER_EX(IDC_FROM_DEVICE_LIST, BN_CLICKED, OnDeviceListClicked)
		// COMMAND_HANDLER_EX(IDC_FROM_DEVICE_ID, BN_CLICKED, OnDeviceIdClicked)
		COMMAND_HANDLER_EX(IDC_DEVICE_ID, EN_CHANGE, OnDeviceIdChange)
		COMMAND_HANDLER_EX(IDC_DEVICE_LIST, CBN_SETFOCUS, OnDeviceIdSetFocus)
		COMMAND_HANDLER_EX(IDC_DEVICE_ID, EN_SETFOCUS, OnDeviceIdSetFocus)
		// COMMAND_HANDLER_EX(IDC_DEVICE_ID, WM_CHAR, OnDeviceIdChar)
		COMMAND_ID_HANDLER_EX(IDOK, OnCmdOK)
		COMMAND_ID_HANDLER_EX(IDCANCEL, OnCmdCancel)
	END_MSG_MAP()

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);

	INT_PTR DoModal(HWND hWnd, PDLG_PARAM Param)
	{
		return CDialogImpl<CDeviceSelectionDlg>::DoModal(hWnd, reinterpret_cast<LPARAM>(Param));
	}

	void OnCmdOK(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/);
	void OnCmdCancel(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/);
	void OnDeviceListClicked(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/);
	void OnDeviceIdClicked(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/);
	void OnDeviceIdChange(UINT /*wNotifyCode*/, int wID, HWND /*hWndCtl*/);

	void OnDeviceIdSetFocus(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hWndCtl*/);

private:

	bool m_fromDeviceList;
	CComboBox m_deviceListComboBox;
	CEdit m_deviceIdEdit;
	CButton m_deviceListButton;
	CButton m_deviceIdButton;
	PDLG_PARAM m_dlgParam;
	CFont m_fixedBoldFont;

	int m_lastEditLength;

	BOOL OnNdasDeviceEnum(PNDASUSER_DEVICE_ENUM_ENTRY EnumEntry);

	static BOOL CALLBACK OnNdasDeviceEnum(
		PNDASUSER_DEVICE_ENUM_ENTRY EnumEntry, 
		LPVOID Context)
	{
		return static_cast<CDeviceSelectionDlg*>(Context)->OnNdasDeviceEnum(EnumEntry);
	}

	typedef struct _LIST_ENTRY_CONTEXT
	{
		TCHAR NdasDeviceId[NDAS_DEVICE_STRING_ID_LEN + 1];
		ACCESS_MASK GrantedAccess;
	} LIST_ENTRY_CONTEXT, *PLIST_ENTRY_CONTEXT ;
};
