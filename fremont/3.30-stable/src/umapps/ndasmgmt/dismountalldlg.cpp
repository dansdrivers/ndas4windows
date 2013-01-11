#include "stdafx.h"
#include <vector>
#include <algorithm>
#include <ndas/ndasuser.h>
#include "ndascls.h"
#include "resource.h"
#include "progressbarctrlex.h"
#include "dismountalldlg.h"

namespace
{
	void AdjustFinalHeaderColumn(HWND hWndListView)
	{
		CListViewCtrl wndListView = hWndListView;
		CHeaderCtrl wndHeader = wndListView.GetHeader();

		int nColumns = wndHeader.GetItemCount();
		int nWidthSum = 0;
		int i = 0;

		for (i = 0; i < nColumns - 1; ++i)
		{
			nWidthSum += wndListView.GetColumnWidth(i);
		}

		// The width of the last column is the remaining width.
		CRect rcListView;
		wndListView.GetClientRect(rcListView);
		wndListView.SetColumnWidth(i, rcListView.Width() - nWidthSum);
	}
}

void 
CDismountAllDialog::PrepareEjectWorkItemCallback(ndas::LogicalDevicePtr LogDevice)
{
	NDAS_LOGICALDEVICE_ID logDeviceId = LogDevice->GetLogicalDeviceId();

	WORK_ITEM_PARAM p = {0};
	p.DialogInstance = this;
	p.Index = m_paramVector.size();
	p.EjectParam.Size = sizeof(NDAS_LOGICALDEVICE_EJECT_PARAM);
	p.EjectParam.LogicalDeviceId = logDeviceId;
	p.DisplayName;

	NDAS_LOGICALDEVICE_STATUS status = LogDevice->GetStatus();
	if (NDAS_LOGICALDEVICE_STATUS_MOUNTED != status)
	{
		return;
	}

	NDASUSER_LOGICALDEVICE_INFORMATION ldinfo = {0};

	BOOL success = NdasQueryLogicalDeviceInformation(logDeviceId, &ldinfo);
	if (!success)
	{
		ATLTRACE("NdasQueryLogicalDeviceInformation failed, logDeviceId=%d, error=0x%X\n",
			logDeviceId, GetLastError());
		return;
	}

	NDASUSER_LOGICALDEVICE_MEMBER_ENTRY& unitDeviceEntry = ldinfo.FirstUnitDevice; 
	NDASUSER_DEVICE_INFORMATION dinfo = {0};

	success = NdasQueryDeviceInformationById(unitDeviceEntry.szDeviceStringId, &dinfo);
	if (!success)
	{
		ATLTRACE("NdasQueryDeviceInformationById failed, deviceId=%ls, error=0x%X\n",
			unitDeviceEntry.szDeviceStringId, GetLastError());
		return;
	}

	if (0 == ldinfo.FirstUnitDevice.UnitNo)
	{
		p.DisplayName.Format(_T("%s"), dinfo.szDeviceName);
	}
	else
	{
		// When displaying the unit number, it uses 1-based index.
		p.DisplayName.Format(_T("%s:%d"), 
			dinfo.szDeviceName, 
			ldinfo.FirstUnitDevice.UnitNo + 1);
	}

	try
	{
		m_paramVector.push_back(p);
	}
	catch (...)
	{
		ATLTRACE("out of memory\n");
	}
}

void 
CDismountAllDialog::QueueEjectWorkItemCallback(WORK_ITEM_PARAM& WiParam)
{
	int itemIndex = m_deviceListView.GetItemCount();

	m_deviceListView.InsertItem(itemIndex, _T("Dismounting"));
	m_deviceListView.AddItem(itemIndex, 1, WiParam.DisplayName);

	BOOL success = QueueUserWorkItem(WorkerThreadStart, &WiParam, WT_EXECUTEDEFAULT);
	if (!success)
	{
		ATLTRACE("QueueUserWorkItem failed, error=0x%X\n", GetLastError());
		--m_workItemCount;
		ATLASSERT(FALSE && "QueueUserWorkItem failed");
	}
}

LRESULT 
CDismountAllDialog::OnInitDialog(HWND hWnd, LPARAM lParam)
{
	m_logDevices = reinterpret_cast<ndas::LogicalDeviceVector*>(lParam);
	ATLASSERT(NULL != m_logDevices);

	m_deviceListView.Attach(GetDlgItem(IDC_DEVICE_LIST));
	m_retryButton.Attach(GetDlgItem(IDRETRY));
	m_cancelButton.Attach(GetDlgItem(IDCANCEL));
	m_progressBarCtl.SubclassWindow(GetDlgItem(IDC_PROGRESS1));

	m_progressBarCtl.SetMarquee(TRUE, 50);
	m_progressBarCtl.ShowWindow(SW_SHOW);

	m_retryButton.ShowWindow(SW_HIDE);
	m_retryButton.EnableWindow(FALSE);

	m_cancelButton.ShowWindow(SW_HIDE);
	m_cancelButton.EnableWindow(FALSE);

	CString statusColumn = MAKEINTRESOURCE(IDS_DISMOUNT_STATUS);
	CString deviceColumn = MAKEINTRESOURCE(IDS_DISMOUNT_DEVICENAME);
	m_deviceListView.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);
	m_deviceListView.InsertColumn(0, statusColumn, LVCFMT_LEFT, 100, 0);
	m_deviceListView.InsertColumn(1, deviceColumn, LVCFMT_LEFT, 400, 1);

	m_messageText.Attach(GetDlgItem(IDC_WAIT_UNMOUNT_MESSAGE));
	m_messageText.GetWindowText(m_staticMessage);

	AdjustFinalHeaderColumn(m_deviceListView);

	CenterWindow();

	m_closeQueued = false;
	m_hWaitCursor.LoadSysCursor(IDC_WAIT);

	pStartWorkItems();

	return TRUE;
}

void
CDismountAllDialog::pStartWorkItems()
{
	m_messageText.SetWindowText(m_staticMessage);
	m_progressBarCtl.SetMarquee(TRUE, 50);
	m_progressBarCtl.ShowWindow(SW_SHOW);

	m_retryButton.ShowWindow(SW_HIDE);
	m_retryButton.EnableWindow(FALSE);

	m_cancelButton.ShowWindow(SW_HIDE);
	m_cancelButton.EnableWindow(FALSE);

	m_errorCount = 0;
	m_deviceListView.DeleteAllItems();
	m_paramVector.clear();

	std::for_each(
		m_logDevices->begin(),
		m_logDevices->end(),
		PrepareEjectWorkItem(this));

	m_workItemCount = m_paramVector.size();

	std::for_each(
		m_paramVector.begin(),
		m_paramVector.end(),
		QueueEjectWorkItem(this));

	if (0 == m_workItemCount)
	{
		EndDialog(IDCLOSE);
		return;
	}
}

void 
CDismountAllDialog::OnClose()
{
	if (m_workItemCount > 0)
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
	else
	{
		// If there are no outstanding work items,
		// close the dialog
		SetMsgHandled(FALSE);
	}
}

BOOL 
CDismountAllDialog::OnSetCursor(HWND, UINT HitTestCode, UINT MouseMsgId)
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
CDismountAllDialog::OnCmdCancel(UINT NotifyCode, int ID, HWND hWndCtl)
{
	EndDialog(ID);
}

void 
CDismountAllDialog::OnCmdRetry(UINT NotifyCode, int ID, HWND hWndCtl)
{
	pStartWorkItems();
}

LRESULT 
CDismountAllDialog::OnWorkItemCompleted(UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (m_closeQueued)
	{
		ATLTRACE("EndDialog is queued. Closing a dialog\n");
		EndDialog(IDCLOSE);
		return TRUE;
	}

	PWORK_ITEM_PARAM wiparam = reinterpret_cast<PWORK_ITEM_PARAM>(wParam);

	if (0 == wiparam->ErrorCode && CR_SUCCESS == wiparam->EjectParam.ConfigRet)
	{
		CString statusText = MAKEINTRESOURCE(IDS_DISMOUNT_COMPLETED);
		m_deviceListView.SetItemText(wiparam->Index, 0, statusText);
	}
	else
	{
		CString statusText = MAKEINTRESOURCE(IDS_DISMOUNT_FAILED);
		++m_errorCount;
		m_deviceListView.SetItemText(wiparam->Index, 0, statusText);
	}

	--m_workItemCount;
	ATLASSERT(m_workItemCount >= 0);

	ATLTRACE("Work Item (%d) completed, remaining=%d\n", wiparam->Index, m_workItemCount);

	if (0 == m_workItemCount)
	{
		//
		// All work items are completed here
		//
		m_progressBarCtl.SetMarquee(FALSE, 0);
		m_progressBarCtl.ShowWindow(SW_HIDE);

		if (m_errorCount > 0)
		{
			CString errorMessage = MAKEINTRESOURCE(IDS_DISMOUNT_FAILURE);
			m_messageText.SetWindowText(errorMessage);

			m_retryButton.ShowWindow(SW_SHOW);
			m_retryButton.EnableWindow(TRUE);

			m_cancelButton.ShowWindow(SW_SHOW);
			m_cancelButton.EnableWindow(TRUE);
		}
		else
		{
			//
			// If there is no error, close the dialog
			//
			EndDialog(IDOK);
		}
	}

	return TRUE;
}

DWORD 
CDismountAllDialog::WorkerThreadStart(PWORK_ITEM_PARAM Param)
{
	NDAS_LOGICALUNIT_STATUS status;
	NDAS_LOGICALDEVICE_ERROR error;

	Param->ErrorCode = 0;
	Param->EjectParam.ConfigRet = CR_SUCCESS;

	BOOL success = NdasQueryLogicalDeviceStatus(
		Param->EjectParam.LogicalDeviceId,
		&status,
		&error);

	if (!success)
	{
		Param->ErrorCode = GetLastError();
		WorkItemCompleted(Param);
		return -1;
	}

	if (status == NDAS_LOGICALUNIT_STATUS_MOUNTED)
	{
		success = NdasEjectLogicalDeviceEx(&Param->EjectParam);
		if (!success)
		{
			Param->ErrorCode = GetLastError();
			WorkItemCompleted(Param);
			return -1;
		}
	}

	do
	{
		success = NdasQueryLogicalDeviceStatus(
			Param->EjectParam.LogicalDeviceId,
			&status,
			&error);

		if (!success)
		{
			Param->ErrorCode = GetLastError();
			WorkItemCompleted(Param);
			return -1;
		}

	} while (NDAS_LOGICALUNIT_STATUS_DISMOUNT_PENDING == status);

	WorkItemCompleted(Param);
	return 0;
}

BOOL 
CDismountAllDialog::WorkItemCompleted(PWORK_ITEM_PARAM Param)
{
	return PostMessage(WM_WORKITEM_COMPLETED, reinterpret_cast<WPARAM>(Param));
}
