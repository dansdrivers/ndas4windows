#pragma once

class CNdasDevicePropAdvancedPage :
	public CPropertyPageImpl<CNdasDevicePropAdvancedPage>
{
public:
	enum { IDD = IDD_DEVPROP_ADVANCED };

	BEGIN_MSG_MAP_EX(CNdasDevicePropAdvancedPage)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_SETCURSOR(OnSetCursor)
		COMMAND_ID_HANDLER_EX(IDC_DEACTIVATE_DEVICE, OnDeactivateDevice)
		COMMAND_ID_HANDLER_EX(IDC_RESET_DEVICE, OnResetDevice)
		COMMAND_ID_HANDLER_EX(IDC_EXPORT, OnCmdExport)
	END_MSG_MAP()

	CNdasDevicePropAdvancedPage();
	CNdasDevicePropAdvancedPage(ndas::DevicePtr pDevice);

	void SetDevice(ndas::DevicePtr pDevice);
	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);
	BOOL OnSetCursor(HWND, UINT HitTestCode, UINT MouseMsgId);

	void OnDeactivateDevice(UINT uCode, int nCtrlID, HWND hwndCtrl);
	void OnResetDevice(UINT uCode, int nCtrlID, HWND hwndCtrl);
	void OnCmdExport(UINT uCode, int nCtrlID, HWND hwndCtrl);
private:

	ndas::DevicePtr m_pDevice;

	CButton m_wndDeactivate;
	CButton m_wndReset;
	CCursorHandle m_waitCursor;
	bool m_waiting;
};
