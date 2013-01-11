#include "stdafx.h"
#include "ndasmgmt.h"
#include "apperrdlg.h"
#include "waitdlg.h"
#include "exportdlg.h"
#include "ndasdevicepropadvancedpage.h"

CNdasDevicePropAdvancedPage::CNdasDevicePropAdvancedPage()
{
}

CNdasDevicePropAdvancedPage::CNdasDevicePropAdvancedPage(ndas::DevicePtr pDevice) :
	m_pDevice(pDevice)
{
}

void
CNdasDevicePropAdvancedPage::SetDevice(ndas::DevicePtr pDevice)
{
	ATLASSERT(NULL != pDevice.get());
	m_pDevice = pDevice;
}

LRESULT 
CNdasDevicePropAdvancedPage::OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
	m_wndDeactivate.Attach(GetDlgItem(IDC_DEACTIVATE_DEVICE));
	m_wndReset.Attach(GetDlgItem(IDC_RESET_DEVICE));

	ATLASSERT(NULL != m_pDevice.get());

	if (NDAS_DEVICE_STATUS_DISABLED == m_pDevice->GetStatus() ||
		m_pDevice->IsAnyUnitDeviceMounted()) 
	{
		m_wndDeactivate.EnableWindow(FALSE);
		m_wndReset.EnableWindow(FALSE);
	}

	m_waiting = false;
	m_waitCursor.LoadSysCursor(IDC_WAIT);

	// Set initial focus
	return 1;
}

void
CNdasDevicePropAdvancedPage::OnDeactivateDevice(UINT uCode, int nCtrlID, HWND hwndCtrl)
{
	ATLASSERT(NULL != m_pDevice.get());

	int response = AtlTaskDialogEx(
		m_hWnd,
		IDS_MAIN_TITLE,
		IDS_CONFIRM_DEACTIVATE_DEVICE,
		IDS_CONFIRM_DEACTIVATE_DEVICE_DESC,
		TDCBF_YES_BUTTON | TDCBF_NO_BUTTON,
		TD_INFORMATION_ICON);

	if (IDYES != response) 
	{
		return;
	}

	if (!m_pDevice->Enable(FALSE)) 
	{
		ErrorMessageBox(m_hWnd, IDS_ERROR_DISABLE_DEVICE);
	}

	m_wndDeactivate.EnableWindow(FALSE);
	m_wndReset.EnableWindow(FALSE);
}

void
CNdasDevicePropAdvancedPage::OnResetDevice(UINT uCode, int nCtrlID, HWND hwndCtrl)
{
	ATLASSERT(NULL != m_pDevice.get());

	int response = AtlTaskDialogEx(
		m_hWnd,
		IDS_MAIN_TITLE,
		IDS_CONFIRM_RESET_DEVICE,
		0U,
		TDCBF_YES_BUTTON | TDCBF_NO_BUTTON,
		TD_INFORMATION_ICON);

	if (IDYES != response) 
	{
		return;
	}

	if (!m_pDevice->Enable(FALSE)) 
	{
		ErrorMessageBox(m_hWnd, IDS_ERROR_RESET_DEVICE);
		return;
	}

	m_waiting = true;
	SetCursor(m_waitCursor);

	CSimpleWaitDlg().DoModal(m_hWnd, 2000);

	m_waiting = false;
	SetCursor(AtlLoadSysCursor(IDC_ARROW));

	if (!m_pDevice->Enable(TRUE)) 
	{
		ErrorMessageBox(m_hWnd, IDS_ERROR_RESET_DEVICE);
	}
	else
	{
		::PostMessage(GetParent(), WM_CLOSE, 0, 0);
	}
}

void 
CNdasDevicePropAdvancedPage::OnCmdExport(UINT uCode, int nCtrlID, HWND hwndCtrl)
{
	CExportDlg exportDlg;
	exportDlg.DoModal(m_hWnd, reinterpret_cast<LPARAM>(&m_pDevice));
}

BOOL 
CNdasDevicePropAdvancedPage::OnSetCursor(HWND, UINT HitTestCode, UINT MouseMsgId)
{
	if (!m_waiting)
	{
		SetMsgHandled(FALSE);
		return TRUE;
	}

	SetCursor(m_waitCursor);
	return TRUE;
}
