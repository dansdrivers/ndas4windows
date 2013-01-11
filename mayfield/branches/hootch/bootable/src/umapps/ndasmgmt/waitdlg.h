#pragma once
#include "ndascls.h"
#include "resource.h"

class CWaitMountDialog :
	public CDialogImpl<CWaitMountDialog>
{
public:
	enum { IDD = IDD_WAIT_MOUNT };
	
	BEGIN_MSG_MAP_EX(CWaitMountDialog)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_TIMER(OnTimer)
		MSG_WM_DESTROY(OnDestroy)
		COMMAND_RANGE_HANDLER_EX(IDOK,IDNO,OnCmdClose)
	END_MSG_MAP()

	CWaitMountDialog() : m_Interval(1000) {}

	INT_PTR DoModal(
		HWND hWndParent, 
		ndas::LogicalDevicePtr pLogDevice)
	{
		return CDialogImpl<CWaitMountDialog>::DoModal(
			hWndParent, 
			reinterpret_cast<LPARAM>(&pLogDevice));
	}

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam)
	{
		m_pLogDevice = *reinterpret_cast<ndas::LogicalDevicePtr*>(lParam);
		m_uiTimer = SetTimer(100, m_Interval, NULL);
		CenterWindow();
		return TRUE;
	}

	void OnDestroy()
	{
		if (0 != m_uiTimer)
		{
			KillTimer(100);
		}
	}

	void OnTimer(UINT uTimer, TIMERPROC proc)
	{
		if (!m_pLogDevice->UpdateStatus())
		{
			EndDialog(IDCLOSE);
			return;
		}
		NDAS_LOGICALDEVICE_STATUS status = m_pLogDevice->GetStatus();
		if (NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING != status)
		{
			EndDialog(IDCLOSE);
			return;
		}
	}

	void OnCmdClose(UINT wNotifyCode, int wID, HWND hWndCtl)
	{
		EndDialog(wID);
	}

private:

	const UINT m_Interval;
	UINT_PTR m_uiTimer;
	ndas::LogicalDevicePtr m_pLogDevice;

};

class CWaitUnmountDialog :
	public CDialogImpl<CWaitUnmountDialog>
{
public:
	enum { IDD = IDD_WAIT_UNMOUNT };
	
	BEGIN_MSG_MAP_EX(CWaitUnmountDialog)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_TIMER(OnTimer)
		MSG_WM_DESTROY(OnDestroy)
		COMMAND_RANGE_HANDLER_EX(IDOK,IDNO,OnCmdClose)
	END_MSG_MAP()

	CWaitUnmountDialog() : m_Interval(1000) {}

	INT_PTR DoModal(
		HWND hWndParent, 
		ndas::LogicalDevicePtr pLogDevice)
	{
		return CDialogImpl<CWaitUnmountDialog>::DoModal(
			hWndParent, 
			reinterpret_cast<LPARAM>(&pLogDevice));
	}

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam)
	{
		m_pLogDevice = *reinterpret_cast<ndas::LogicalDevicePtr*>(lParam);
		m_uiTimer = SetTimer(100, m_Interval, NULL);
		CenterWindow();
		return TRUE;
	}

	void OnDestroy()
	{
		if (0 != m_uiTimer)
		{
			KillTimer(100);
		}
	}

	void OnTimer(UINT uTimer, TIMERPROC proc)
	{
		if (!m_pLogDevice->UpdateStatus())
		{
			EndDialog(IDCLOSE);
			return;
		}
		NDAS_LOGICALDEVICE_STATUS status = m_pLogDevice->GetStatus();
		if (NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING != status)
		{
			EndDialog(IDCLOSE);
			return;
		}
	}

	void OnCmdClose(UINT wNotifyCode, int wID, HWND hWndCtl)
	{
		EndDialog(wID);
	}

private:

	const UINT m_Interval;
	UINT_PTR m_uiTimer;
	ndas::LogicalDevicePtr m_pLogDevice;

};

class CSimpleWaitDlg : 
	public CDialogImpl<CSimpleWaitDlg>
{
public:
	enum { IDD = IDD_NULL };

	BEGIN_MSG_MAP_EX(CWaitUnmountDialog)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_TIMER(OnTimer)
		MSG_WM_DESTROY(OnDestroy)
	END_MSG_MAP()

	CSimpleWaitDlg() : m_uiTimer(0)
	{
	}

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam)
	{
		UINT Elapse = static_cast<UINT>(lParam);
		m_uiTimer = SetTimer(100, Elapse, NULL);
		::SetCursor(AtlLoadSysCursor(IDC_WAIT));
		return FALSE;
	}

	void OnDestroy()
	{
		if (0 != m_uiTimer)
		{
			KillTimer(100);
		}
		::SetCursor(AtlLoadSysCursor(IDC_ARROW));
	}

	void OnTimer(UINT uTimer, TIMERPROC proc)
	{
		EndDialog(IDCLOSE);
	}

private:
	UINT_PTR m_uiTimer;
};
