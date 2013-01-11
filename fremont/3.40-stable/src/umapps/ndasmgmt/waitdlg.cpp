#include "stdafx.h"
#include "waitdlg.h"

INT_PTR 
CDismountDialog::DoModal(
	HWND hWndParent, 
	ndas::LogicalDevicePtr pLogDevice)
{
	return CDialogImpl<CDismountDialog>::DoModal(
		hWndParent, 
		reinterpret_cast<LPARAM>(&pLogDevice));
}

LRESULT 
CDismountDialog::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	m_closeQueued = FALSE;
	m_WorkItemFinished = CreateEvent(NULL, TRUE, TRUE, FALSE);
	ATLASSERT(NULL != m_WorkItemFinished);

	m_pLogDevice = *reinterpret_cast<ndas::LogicalDevicePtr*>(lParam);
	CenterWindow();

	m_messageText.Attach(GetDlgItem(IDC_WAIT_UNMOUNT_MESSAGE));
	m_messageText.GetWindowText(m_staticMessage, 256);

	// m_progressBarCtl.Attach(GetDlgItem(IDC_PROGRESS1));
	m_progressBarCtl.SubclassWindow(GetDlgItem(IDC_PROGRESS1));
	m_cancelButton.Attach(GetDlgItem(IDCANCEL));
	m_retryButton.Attach(GetDlgItem(IDRETRY));

	m_hWaitCursor = LoadCursor(NULL, IDC_WAIT);

	//
	// Start the work item
	//
	m_progressBarCtl.SetMarquee(TRUE, 50);
	m_cancelButton.ShowWindow(SW_HIDE);
	m_retryButton.ShowWindow(SW_HIDE);

	ATLVERIFY( ResetEvent(m_WorkItemFinished) );
	BOOL success = QueueUserWorkItem(WorkItemStart, this, WT_EXECUTEDEFAULT);
	if (!success)
	{
		ATLTRACE("QueueUserWorkItem failed, error=0x%X\n", GetLastError());
		EndDialog(IDCANCEL);
		return TRUE;
	}

	return TRUE;
}

BOOL 
CDismountDialog::OnSetCursor(HWND, UINT HitTestCode, UINT MouseMsgId)
{
	if (m_closeQueued)
	{
		SetCursor(m_hWaitCursor);
		return TRUE;
	}
	else
	{
		SetMsgHandled(FALSE);
	}
	return FALSE;
}

void 
CDismountDialog::OnDestroy()
{
	ATLVERIFY( CloseHandle(m_WorkItemFinished) );
}

void 
CDismountDialog::OnClose()
{
	if (WAIT_OBJECT_0 == WaitForSingleObject(m_WorkItemFinished, 0))
	{
		SetMsgHandled(FALSE);
	}
	else
	{
		//
		// At first WM_CLOSE, disable the close menu and icon,
		// and queue the close command at work item completion
		//
		if (!m_closeQueued)
		{
			CMenuHandle sysMenu = GetSystemMenu(FALSE);
			sysMenu.ModifyMenu(SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
		}
		m_closeQueued = TRUE;
		SetMsgHandled(TRUE);
	}
}

void 
CDismountDialog::OnCmdCancel(UINT NotifyCode, int ID, HWND hWndCtl)
{
	ATLASSERT(WAIT_OBJECT_0 == WaitForSingleObject(m_WorkItemFinished, 0));
	EndDialog(IDCANCEL);
}

void CDismountDialog::OnCmdRetry(UINT NotifyCode, int ID, HWND hWndCtl)
{
	ATLASSERT(WAIT_OBJECT_0 == WaitForSingleObject(m_WorkItemFinished, 0));

	//
	// Start the work item
	//

	m_cancelButton.ShowWindow(SW_HIDE);
	m_retryButton.ShowWindow(SW_HIDE);
	m_progressBarCtl.SetMarquee(TRUE, 50);
	m_progressBarCtl.ShowWindow(SW_SHOW);
	m_messageText.SetWindowText(m_staticMessage);

	ResetEvent(m_WorkItemFinished);
	BOOL success = QueueUserWorkItem(WorkItemStart, this, WT_EXECUTEDEFAULT);
	if (!success)
	{
		ATLTRACE("QueueUserWorkItem failed, error=0x%X\n", GetLastError());
		EndDialog(IDCANCEL);
	}
}

DWORD
CDismountDialog::WorkItemStart()
{
	m_EjectError = 0;

	BOOL success = m_pLogDevice->Eject(&m_EjectParam);
	if (!success)
	{
		m_EjectError = GetLastError();
		ATLTRACE("Eject failed, error=0x%X\n", m_EjectError);
	}
	else
	{
		if (m_EjectParam.ConfigRet != CR_SUCCESS)
		{
			ATLTRACE("Eject failed from configuration manager, configRet=%d\n",
				m_EjectParam.ConfigRet);
			ATLTRACE("VetoType=%d,VetoName=%ls\n",
				m_EjectParam.VetoType,
				m_EjectParam.VetoName);
		}
	}

	ATLVERIFY( PostMessage(WM_WORKITEM_COMPLETED) );

	ATLVERIFY( SetEvent(m_WorkItemFinished) );

	return 0;
}

LRESULT 
CDismountDialog::OnWorkItemCompleted(UINT Msg, WPARAM wParam, LPARAM lParam)
{
	//
	// Make sure that the work item is really done.
	//
	ATLVERIFY(WAIT_OBJECT_0 == WaitForSingleObject(m_WorkItemFinished, INFINITE));

	//
	// If the close command is queued, close the dialog without further
	// processing
	//
	if (m_closeQueued)
	{
		EndDialog(IDCLOSE);
		return TRUE;
	}

	//
	// If the operation is completed without an error,
	// we closes the dialog.
	//
	if (0 == m_EjectError && CR_SUCCESS == m_EjectParam.ConfigRet)
	{
		EndDialog(IDOK);
		return TRUE;
	}

	//
	// Otherwise, shows the error message and asks if we should retry
	//
	m_cancelButton.ShowWindow(SW_SHOW);
	m_retryButton.ShowWindow(SW_SHOW);
	m_progressBarCtl.SetMarquee(FALSE, 0);
	m_progressBarCtl.ShowWindow(SW_HIDE);

	CString errorMessage = MAKEINTRESOURCE(IDS_DISMOUNT_FAILURE);
	m_messageText.SetWindowText(errorMessage);

	return TRUE;
}
