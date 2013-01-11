#include "stdafx.h"
#include "ndasmgmt.h"

#include "devpropsh.h"
#include "confirmdlg.h"
#include "propertylist.h"
#include "ndastypestr.h"
#include "apperrdlg.h"
#include "waitdlg.h"
#include "exportdlg.h"
#include "ndasdevicerenamedlg.h"
#include "ndasdeviceaddwritekeydlg.h"

#include "ndasdeviceprophoststatpage.h"

namespace
{
	//
	// List Column Header Width Adjustment
	//
	void AdjustHeaderWidth(HWND hWndListView)
	{
		CListViewCtrl wndListView = hWndListView;
		CHeaderCtrl wndHeader = wndListView.GetHeader();

		int nColumns = wndHeader.GetItemCount();
		int nWidthSum = 0;
		int i = 0;

		for (i = 0; i < nColumns - 1; ++i)
		{
			wndListView.SetColumnWidth(i, LVSCW_AUTOSIZE_USEHEADER);
			nWidthSum += wndListView.GetColumnWidth(i);
		}

		// The width of the last column is the remaining width.
		CRect rcListView;
		wndListView.GetClientRect(rcListView);
		wndListView.SetColumnWidth(i, rcListView.Width() - nWidthSum);
	}

} // anonymous namespace

CNdasDevicePropHostStatPage::CNdasDevicePropHostStatPage() :
	m_hAccel(NULL),
	m_ThreadCompletionEvent(CreateEvent(NULL, TRUE, TRUE, NULL))
{
}

CNdasDevicePropHostStatPage::CNdasDevicePropHostStatPage(ndas::UnitDevicePtr pUnitDevice) :
	m_pUnitDevice(pUnitDevice),
	m_ThreadCompletionEvent(CreateEvent(NULL, TRUE, TRUE, NULL))
{
}

void
CNdasDevicePropHostStatPage::SetUnitDevice(ndas::UnitDevicePtr pUnitDevice)
{
	ATLASSERT(NULL != pUnitDevice.get());
	m_pUnitDevice = pUnitDevice;
}

LRESULT
CNdasDevicePropHostStatPage::OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
	m_wndListView.Attach(GetDlgItem(IDC_HOST_LIST));

	CString strBuffer;

	strBuffer.LoadString(IDS_HOSTSTAT_ACCESS);
	m_wndListView.AddColumn(strBuffer, 0);

	strBuffer.LoadString(IDS_HOSTSTAT_HOSTNAME);
	m_wndListView.AddColumn(strBuffer, 1);

	strBuffer.LoadString(IDS_HOSTSTAT_NETWORK_ADDRESS);
	m_wndListView.AddColumn(strBuffer, 2);
	m_wndListView.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);

	AdjustHeaderWidth(m_wndListView);

	// Link Control
	m_wndRefreshLink.SetHyperLinkExtendedStyle(HLINK_COMMANDBUTTON);
	m_wndRefreshLink.SubclassWindow(GetDlgItem(IDC_REFRESH_HOST));

	// List View Background Color Brush
	m_brushListViewBk.CreateSolidBrush(m_wndListView.GetBkColor());

	// Create Animate Control
	CRect rect;
	m_wndListView.GetWindowRect(&rect);
	
	CRect rectHeader;
	m_wndListView.GetHeader().GetWindowRect(&rectHeader);
	rect.DeflateRect(5,rectHeader.Height()+5,5,5);
	::MapWindowPoints(HWND_DESKTOP, m_hWnd, (LPPOINT)&rect, 2);

	ATLTRACE("Rect=(%d,%d)-(%d,%d)\n", rect.left, rect.top, rect.right, rect.bottom);

	HWND hWnd = m_findHostsAnimate.Create(
		m_hWnd, rect, NULL, WS_CHILD | ACS_CENTER | ACS_TRANSPARENT);
	if (NULL == hWnd)
	{
		ATLASSERT(FALSE && "Create animate control failed");
	}

	//
	// CAnimateControl.Open cannot access the Resource Instance
	// Do not use: m_findHostsAnimate.Open(IDA_FINDHOSTS);
	//
	BOOL fSuccess = Animate_OpenEx(
		m_findHostsAnimate, 
		ATL::_AtlBaseModule.GetResourceInstance(), 
		MAKEINTRESOURCE(IDA_FINDHOSTS));
	if (!fSuccess)
	{
		ATLASSERT(FALSE && "Animate_OpenEx(IDA_FINDHOSTS) failed");
	}

	// Support F5 to refresh
	ACCEL accel = {0};
	accel.fVirt = FVIRTKEY;
	accel.key = VK_F5;
	accel.cmd = IDC_REFRESH_HOST;

	m_hAccel = ::CreateAcceleratorTable(&accel, 1);
	ATLASSERT(NULL != m_hAccel);

	// Begin Refresh
	BeginRefreshHosts();

	return TRUE;
}

DWORD 
CNdasDevicePropHostStatPage::pHostInfoUpdateThreadStart(LPVOID Context)
{
	CNdasDevicePropHostStatPage* instance = 
		static_cast<CNdasDevicePropHostStatPage*>(Context);
	DWORD ret = instance->pHostInfoUpdateThreadStart();
	return ret;
}

DWORD
CNdasDevicePropHostStatPage::pHostInfoUpdateThreadStart()
{
	BOOL fSuccess = m_pUnitDevice->UpdateHostInfo();
	if (!fSuccess) 
	{
		//		m_wndListView.AddItem(0,0,_T("Unavailable"));
		PostMessage(WM_THREADED_WORK_COMPLETED);
		return 1;
	}

	DWORD nHostInfo = m_pUnitDevice->GetHostInfoCount();
	for (DWORD i = 0; i < nHostInfo; ++i) 
	{
		ACCESS_MASK access = 0;
		const NDAS_HOST_INFO* pHostInfo = m_pUnitDevice->GetHostInfo(i,&access);
		ATLASSERT(NULL != pHostInfo); // index is correct, then it must succeeded
		HostInfoData data = {access, *pHostInfo };
		m_hostInfoDataArray.Add(data);
	}

	::Sleep(500);

	PostMessage(WM_THREADED_WORK_COMPLETED);

	return 0;
}

BOOL 
CNdasDevicePropHostStatPage::OnTranslateAccelerator(LPMSG lpMsg)
{
	int ret = ::TranslateAccelerator(m_hWnd, m_hAccel, lpMsg);
	return ret ? PSNRET_MESSAGEHANDLED : PSNRET_NOERROR;
}

void
CNdasDevicePropHostStatPage::OnReset()
{
	ATLTRACE(__FUNCTION__ "\n");

	ATLVERIFY(AtlWaitWithMessageLoop(m_ThreadCompletionEvent));

	(void) m_findHostsAnimate.Close(); // always returns FALSE
	
	if (m_hAccel && !::DestroyAcceleratorTable(m_hAccel))
	{
		ATLASSERT(FALSE && "DestroyAcceleratorTable failed\n");
	}
}

HBRUSH 
CNdasDevicePropHostStatPage::OnCtlColorStatic(HDC hCtlDC, HWND hWndCtl)
{
	if (hWndCtl == m_findHostsAnimate.m_hWnd)
	{
		::SetBkColor(hCtlDC, m_wndListView.GetBkColor());
		::SetBkMode(hCtlDC, OPAQUE);
		return m_brushListViewBk;
	}
	else
	{
		SetMsgHandled(FALSE);
		return 0;
	}
}

LRESULT
CNdasDevicePropHostStatPage::OnThreadCompleted(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// Stop the animation
	m_findHostsAnimate.Stop();
	m_findHostsAnimate.ShowWindow(SW_HIDE);

	// Fill the list view

	CString strBuffer;
	int size = m_hostInfoDataArray.GetSize();
	for (int i = 0; i < size; ++i)
	{
		HostInfoData& data = m_hostInfoDataArray[i];
		const NDAS_HOST_INFO* pHostInfo = &data.HostInfo;
		ACCESS_MASK access = data.Access;

		(void) pAddressString(
			strBuffer,
			&pHostInfo->LPXAddrs,
			&pHostInfo->IPV4Addrs);

		LPCTSTR resID = MAKEINTRESOURCE(IDS_HOST_RO);

		if (access & GENERIC_WRITE)
		{
			resID = MAKEINTRESOURCE(IDS_HOST_RW);
		}
		else
		{
			resID = MAKEINTRESOURCE(IDS_HOST_RO);
		}

		//
		// When NDAS_ACCESS_BIT_EXTENDED is set, 
		// we can tell if the host is write-sharing-enabled host and
		// and if it is primary or secondary in that case.
		//
		if (access & NDAS_ACCESS_BIT_EXTENDED)
		{
			if (access & NDAS_ACCESS_BIT_SHARED_WRITE)
			{
				if (access & NDAS_ACCESS_BIT_SHARED_WRITE_PRIMARY)
				{
					resID = MAKEINTRESOURCE(IDS_HOST_SHARED_RW_P);
				}
				else
				{
					resID = MAKEINTRESOURCE(IDS_HOST_SHARED_RW);
				}
			}
		}
		CString accessText(resID);
		m_wndListView.AddItem(i, 0, accessText);
		m_wndListView.SetItemText(i, 1, pHostInfo->szHostname);
		m_wndListView.SetItemText(i, 2, strBuffer);
	}

	AdjustHeaderWidth(m_wndListView);

	m_wndRefreshLink.EnableWindow(TRUE);

	m_hCursor = AtlLoadSysCursor(IDC_ARROW);
	SetCursor(m_hCursor);

	//
	// Now set the completion event
	//

	ATLVERIFY(SetEvent(m_ThreadCompletionEvent));

	return TRUE;
}

LRESULT 
CNdasDevicePropHostStatPage::OnSetCursor(HWND hWnd, UINT hit, UINT msg)
{
	if (!IsIconic())
	{
		SetCursor(m_hCursor);
	}
	return TRUE;
}

void
CNdasDevicePropHostStatPage::BeginRefreshHosts()
{
	//
	// If a worker thread is running, we ignores this request
	//
	DWORD waitResult = WaitForSingleObject(m_ThreadCompletionEvent, 0);
	if (WAIT_OBJECT_0 != waitResult)
	{
		return;
	}

	//
	// ThreadCompletionEvent is not set - no thread is running
	// Now clear the event to signal the thread is about to run.
	// There is no synchronization between WaitForSingleObject and ResetEvent
	// because this function is called by the message handler
	// and no more than one thread is supposed to invoke the message handler.
	//

	ATLVERIFY(ResetEvent(m_ThreadCompletionEvent));

	m_wndRefreshLink.EnableWindow(FALSE);

	m_findHostsAnimate.ShowWindow(SW_SHOW);
	m_findHostsAnimate.Play(0,-1,-1);

	m_hostInfoDataArray.RemoveAll();
	m_wndListView.DeleteAllItems();

	m_hCursor = AtlLoadSysCursor(IDC_APPSTARTING);
	SetCursor(m_hCursor);

	BOOL success = QueueUserWorkItem(
		pHostInfoUpdateThreadStart, 
		this, 
		WT_EXECUTEDEFAULT);

	if (!success)
	{
		ATLASSERT(FALSE && "QueueUserWorkItem failed");
		PostMessage(WM_THREADED_WORK_COMPLETED);
	}
}

void 
CNdasDevicePropHostStatPage::OnCmdRefresh(UINT, int, HWND)
{
	BeginRefreshHosts();
}
