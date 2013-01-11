#pragma once
#include "ndascls.h"
#include "resource.h"
#include "progressbarctrlex.h"

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

	void OnTimer(UINT_PTR uTimer)
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

	void OnTimer(UINT uTimer, TIMERPROC proc)
	{
		OnTimer(uTimer);
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

class CDismountDialog :
	public CDialogImpl<CDismountDialog>
{
public:
	enum { IDD = IDD_WAIT_UNMOUNT };

	enum { WM_WORKITEM_COMPLETED = WM_APP + 0xC00};

	BEGIN_MSG_MAP_EX(CDismountDialog)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_DESTROY(OnDestroy)
		MSG_WM_SETCURSOR(OnSetCursor)
		MSG_WM_CLOSE(OnClose)
		MESSAGE_HANDLER_EX(WM_WORKITEM_COMPLETED,OnWorkItemCompleted)
		COMMAND_ID_HANDLER_EX(IDRETRY, OnCmdRetry)
		COMMAND_ID_HANDLER_EX(IDCANCEL, OnCmdCancel)
	END_MSG_MAP()

	CDismountDialog() : m_Interval(1000), m_closeQueued(FALSE) {}

	INT_PTR DoModal(HWND hWndParent, ndas::LogicalDevicePtr pLogDevice);
	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam);
	BOOL OnSetCursor(HWND, UINT HitTestCode, UINT MouseMsgId);
	void OnClose();
	void OnDestroy();
	void OnCmdCancel(UINT NotifyCode, int ID, HWND hWndCtl);
	void OnCmdRetry(UINT NotifyCode, int ID, HWND hWndCtl);
	LRESULT OnWorkItemCompleted(UINT Msg, WPARAM wParam, LPARAM lParam);

private:

	const UINT m_Interval;
	UINT_PTR m_uiTimer;
	ndas::LogicalDevicePtr m_pLogDevice;

	TCHAR m_staticMessage[256];
	CStatic m_messageText;
	CButton m_cancelButton;
	CButton m_retryButton;
	CMarqueeProgressBarCtrl m_progressBarCtl;

	HCURSOR m_hWaitCursor;

	BOOL m_closeQueued;
	DWORD m_EjectError;
	NDAS_LOGICALDEVICE_EJECT_PARAM m_EjectParam;
	HANDLE m_WorkItemFinished;

	static DWORD WorkItemStart(LPVOID Context)
	{
		return static_cast<CDismountDialog*>(Context)->WorkItemStart();
	}

	DWORD WorkItemStart();
};

class CSimpleWaitDlg : 
	public CDialogImpl<CSimpleWaitDlg>
{
public:
	enum { IDD = IDD_NULL };

	BEGIN_MSG_MAP_EX(CSimpleWaitDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_TIMER(OnTimer)
		MSG_WM_DESTROY(OnDestroy)
		MSG_WM_SETCURSOR(OnSetCursor)
	END_MSG_MAP()

	CSimpleWaitDlg() : m_uiTimer(0)
	{
	}

	LRESULT OnInitDialog(HWND hWnd, LPARAM lParam)
	{
		UINT Elapse = static_cast<UINT>(lParam);
		m_uiTimer = SetTimer(100, Elapse, NULL);
		m_waitCursor = AtlLoadSysCursor(IDC_WAIT);
		::SetCursor(m_waitCursor);
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

	void OnTimer(UINT_PTR uTimer)
	{
		EndDialog(IDCLOSE);
	}

	void OnTimer(UINT uTimer, TIMERPROC proc)
	{
		OnTimer(uTimer);
	}

	BOOL OnSetCursor(HWND, UINT HitTestCode, UINT MouseMsgId)
	{
		SetCursor(m_waitCursor);
		return TRUE;
	}

private:
	UINT_PTR m_uiTimer;
	HCURSOR m_waitCursor;
};
