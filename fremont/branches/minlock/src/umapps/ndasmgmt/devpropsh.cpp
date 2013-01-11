#include "stdafx.h"
#include "ndasmgmt.h"
#include "resource.h"
#include <xtl/xtlautores.h>
#include <boost/mem_fn.hpp>

#include "devpropsh.h"
#include "confirmdlg.h"
#include "propertylist.h"
#include "ndastypestr.h"
#include "apperrdlg.h"
#include "waitdlg.h"
#include "exportdlg.h"

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

	bool IsNullNdasDeviceId(const NDAS_DEVICE_ID& deviceId)
	{
		return (deviceId.Node[0] == 0x00 &&
			deviceId.Node[1] == 0x00 &&
			deviceId.Node[2] == 0x00 &&
			deviceId.Node[3] == 0x00 &&
			deviceId.Node[4] == 0x00 &&
			deviceId.Node[5] == 0x00);
	}


struct ShowWindowFunctor : public std::unary_function<int, void>
{
	ShowWindowFunctor(int nCmdShow) : nCmdShow(nCmdShow) {}
	void operator()(HWND hWnd) const
	{
		::ShowWindow(hWnd, nCmdShow);
	}
private:
	const int nCmdShow;
};

struct AddToComboBox : std::unary_function<ndas::UnitDevicePtr, void> {
	AddToComboBox(const CComboBox& wnd) : wnd(wnd) {}
	void operator()(const ndas::UnitDevicePtr& pUnitDevice) {
		NDAS_UNITDEVICE_TYPE type = pUnitDevice->GetType();
		NDAS_UNITDEVICE_SUBTYPE subType = pUnitDevice->GetSubType();
		CString strNumber; 
		strNumber.Format(_T("%d: "), pUnitDevice->GetUnitNo() + 1);
		CString strType;
		pUnitDeviceTypeString(strType, type, subType);
		strNumber += strType;
		wnd.AddString(strNumber);
	}
private:
	CComboBox wnd;
};

} // anonymous namespace

//////////////////////////////////////////////////////////////////////////
//
// Device Property Sheet
//
//////////////////////////////////////////////////////////////////////////

CDevicePropSheet::CDevicePropSheet(
	_U_STRINGorID title,
	UINT uStartPage,
	HWND hWndParent) : 
	CPropertySheetImpl<CDevicePropSheet>(title, uStartPage, hWndParent),
	m_bCentered(FALSE),
	m_pspHostStats(NULL)
{
	m_psh.dwFlags |= PSH_NOAPPLYNOW | PSH_USEPAGELANG;
	AddPage(m_pspGeneral);
}

void 
CDevicePropSheet::SetDevice(ndas::DevicePtr pDevice)
{
	m_pDevice = pDevice;
	m_pspGeneral.SetDevice(pDevice);

	//
	// Hardware Page shows only if the device is connected
	//
	if (NDAS_DEVICE_STATUS_CONNECTED == pDevice->GetStatus()) 
	{
		m_pspHardware.SetDevice(pDevice);
		AddPage(m_pspHardware);
	}

	m_pspHostStats.clear();

	ndas::UnitDeviceVector unitDevices = pDevice->GetUnitDevices();
	if (!unitDevices.empty()) 
	{
		DWORD nPages = unitDevices.size();
		for (DWORD i = 0; i < nPages; ++i) 
		{
			HostStatPagePtr pPage(new CDeviceHostStatPage());
			pPage->SetUnitDevice(unitDevices[i]);
			m_pspHostStats.push_back(pPage);
			AddPage(*pPage.get());
		}
	}

	m_pspAdvanced.SetDevice(pDevice);
	AddPage(m_pspAdvanced);
}

void 
CDevicePropSheet::OnShowWindow(BOOL bShow, UINT nStatus)
{
	ATLASSERT(NULL != m_pDevice.get()); 
	if (bShow && !m_bCentered) 
	{
		// Center Windows only once!
		m_bCentered = TRUE;
		CenterWindow();
	}

	SetMsgHandled(FALSE);
}

//////////////////////////////////////////////////////////////////////////
//
// General Property Page
//
//////////////////////////////////////////////////////////////////////////

CDeviceGeneralPage::CDeviceGeneralPage()
{
}

LRESULT 
CDeviceGeneralPage::OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
	ATLASSERT(m_pDevice != 0);

	m_hCursor = AtlLoadSysCursor(IDC_ARROW);

	m_wndDeviceName.Attach(GetDlgItem(IDC_DEVICE_NAME));
	m_wndDeviceId.Attach(GetDlgItem(IDC_DEVICE_ID));
	m_wndDeviceStatus.Attach(GetDlgItem(IDC_DEVICE_STATUS));
	m_wndDeviceWriteKey.Attach(GetDlgItem(IDC_DEVICE_WRITE_KEY));
	m_wndAddRemoveWriteKey.Attach(GetDlgItem(IDC_ADD_WRITE_KEY));

	m_wndUnitDeviceGroup.Attach(GetDlgItem(IDC_UNITDEVICE_GROUP));
	m_wndUnitDeviceIcon.Attach(GetDlgItem(IDC_UNITDEVICE_TYPE_ICON));
	m_wndUnitDeviceType.Attach(GetDlgItem(IDC_UNITDEVICE_TYPE));
	m_wndUnitDeviceStatus.Attach(GetDlgItem(IDC_UNITDEVICE_STATUS));
	m_wndUnitDeviceCapacity.Attach(GetDlgItem(IDC_UNITDEVICE_CAPACITY));
	m_wndUnitDeviceROHosts.Attach(GetDlgItem(IDC_UNITDEVICE_RO_HOSTS));
	m_wndUnitDeviceRWHosts.Attach(GetDlgItem(IDC_UNITDEVICE_RW_HOSTS));
	m_wndLogDeviceTree.Attach(GetDlgItem(IDC_LOGDEV_TREE));

	m_wndUnitDeviceList = GetDlgItem(IDC_UNITDEVICE_LIST);

	// Temporary edit control to get an effective password character
	{
		CEdit wndPassword;
		wndPassword.Create(m_hWnd, NULL, NULL, WS_CHILD | ES_PASSWORD);
		m_chConcealed = wndPassword.GetPasswordChar();
		wndPassword.DestroyWindow();
	}

	BOOL fSuccess = m_imageList.CreateFromImage(
		IDB_UNITDEVICES, 
		32, 
		1, 
		CLR_DEFAULT, 
		IMAGE_BITMAP,
		LR_CREATEDIBSECTION | LR_DEFAULTCOLOR | LR_DEFAULTSIZE);
	ATLASSERT(fSuccess && "Loading IDB_UNITDEVICES failed");

	_GrabUnitDeviceControls();

	// get the bold font
	CFontHandle boldFont;
	{
		CFontHandle dlgFont = GetFont();
		LOGFONT logFont;
		dlgFont.GetLogFont(&logFont);
		logFont.lfWeight = FW_BOLD;
		ATLVERIFY(boldFont.CreateFontIndirect(&logFont));
	}

	m_wndUnitDeviceType.SetFont(boldFont);

	// Cover up control, be sure to create this after FillUnitDeviceControls()
	{
		CRect rect;
		m_wndUnitDeviceGroup.GetClientRect(&rect);
		::MapWindowPoints(m_wndUnitDeviceGroup, HWND_DESKTOP, reinterpret_cast<LPPOINT>(&rect), 2);
		::MapWindowPoints(HWND_DESKTOP, m_hWnd, reinterpret_cast<LPPOINT>(&rect), 2);
		rect.DeflateRect(10,50,10,10);
		m_wndNA.Create(m_hWnd, rect, NULL, WS_CHILD | SS_CENTER);
		CString str = MAKEINTRESOURCE(IDS_UNITDEVICE_NONE);
		ATLTRACE("NA: %ws\n", str);
		m_wndNA.SetWindowText(str);
		m_wndNA.SetFont(GetFont());
		m_wndNA.EnableWindow(FALSE);
	}
	{
		NDAS_UNITDEVICE_ERROR_HDD_READ_FAILURE;
		CRect rect;
		m_wndLogDeviceTree.GetWindowRect(&rect);
		::MapWindowPoints(HWND_DESKTOP, m_hWnd, reinterpret_cast<LPPOINT>(&rect), 2);
		// rect.DeflateRect(10,10,10,10);
		CString str = MAKEINTRESOURCE(IDS_LOGDEV_INFO_UNAVAILABLE);
		m_wndLogDeviceNA.Create(m_hWnd, rect, str, 
			WS_CHILD | WS_DISABLED | 
			BS_FLAT | BS_CENTER | BS_VCENTER | BS_TEXT);
		// m_wndLogDeviceNA.Create( Create(m_hWnd, rect, NULL, WS_CHILD | SS_CENTER, WS_EX_TRANSPARENT);
		ATLTRACE(_T("LogDevice N/A: %s"), str);
		// m_wndLogDeviceNA.SetWindowText(str);
		m_wndLogDeviceNA.SetFont(GetFont());
		m_wndLogDeviceNA.EnableWindow(FALSE);
	}

	_InitData();

	return 0;
}

BOOL 
CDeviceGeneralPage::OnEnumChild(HWND hWnd)
{
	m_unitDeviceControls.push_back(hWnd);
	return TRUE;
}

void 
CDeviceGeneralPage::OnUnitDeviceSelChange(UINT, int, HWND)
{
	int n = m_wndUnitDeviceList.GetCurSel();
	if (CB_ERR != n)
	{
		_SetCurUnitDevice(n);
	}
}

// Worker Thread Start Routine
DWORD 
CDeviceGeneralPage::OnWorkerThreadStart(LPVOID lpParameter)
{
	UINT i = PtrToUlong(lpParameter);

	ATLASSERT(i < RTL_NUMBER_OF(m_pUnitDevices));
	if (i >= RTL_NUMBER_OF(m_pUnitDevices))
	{
		return 255;
	}

	ndas::UnitDevicePtr pUnitDevice = m_pUnitDevices[i];
	if (NULL == pUnitDevice.get())
	{
		return 1;
	}

	(void) pUnitDevice->UpdateHostStats();

	if (::IsWindow(m_hWnd))
	{
		ATLVERIFY(PostMessage(WM_USER_DONE, 0, static_cast<LPARAM>(i)));
	}

	return 0;
}

LRESULT 
CDeviceGeneralPage::OnWorkDone(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	UINT i = static_cast<UINT>(lParam);

	ATLASSERT(i < RTL_NUMBER_OF(m_pUnitDevices));
	if (i >= RTL_NUMBER_OF(m_pUnitDevices))
	{
		m_hCursor = AtlLoadSysCursor(IDC_ARROW);
		::SetCursor(m_hCursor);
		return 255;
	}

	ndas::UnitDevicePtr pUnitDevice = m_pUnitDevices[i];

	if (NULL == pUnitDevice.get())
	{
		ATLASSERT(FALSE);
		m_hCursor = AtlLoadSysCursor(IDC_ARROW);
		::SetCursor(m_hCursor);
		return TRUE;
	}

	m_unitDeviceUpdated[i] = true;

	if (m_curUnitDevice == i)
	{
		CString str;
		str.Format(_T("%d"), pUnitDevice->GetROHostCount());
		if (m_wndUnitDeviceROHosts.IsWindow())
		{
			m_wndUnitDeviceROHosts.SetWindowText(str);
		}

		str.Format(_T("%d"), pUnitDevice->GetRWHostCount());
		if (m_wndUnitDeviceRWHosts.IsWindow())
		{
			m_wndUnitDeviceRWHosts.SetWindowText(str);
		}
	}

	m_hCursor = AtlLoadSysCursor(IDC_ARROW);
	::SetCursor(m_hCursor);

	return TRUE;
}

void 
CDeviceGeneralPage::SetDevice(ndas::DevicePtr pDevice)
{
	m_pDevice = pDevice;
}

void 
CDeviceGeneralPage::OnRename(UINT, int, HWND)
{
	CDeviceRenameDialog wndRename;
	wndRename.SetName(m_pDevice->GetName());
	
	INT_PTR iResult = wndRename.DoModal();
	if (IDOK == iResult) 
	{
		CString strNewName = wndRename.GetName();
		if (0 != strNewName.Compare(m_pDevice->GetName())) 
		{
			if (!m_pDevice->SetName(strNewName)) 
			{
				ErrorMessageBox(m_hWnd, IDS_ERROR_RENAME_DEVICE);
			} 
			else 
			{
				m_wndDeviceName.SetWindowText(strNewName);

				CString strTitle;
				strTitle.FormatMessage(IDS_DEVICE_PROP_TITLE, strNewName);
				GetParent().SetWindowText(strTitle);
				_UpdateDeviceData();
			}
		}
	}
}


void 
CDeviceGeneralPage::OnModifyWriteKey(UINT, int, HWND)
{
	ACCESS_MASK access = m_pDevice->GetGrantedAccess();
	
	if (access & GENERIC_WRITE) 
	{
		int response = pTaskDialogVerify(
			m_hWnd,
			IDS_REMOVE_WRITE_KEY_CONFIRM_TITLE,
			IDS_REMOVE_WRITE_KEY_CONFIRM,
			IDS_REMOVE_WRITE_KEY_CONFIRM_DESC,
			_T("DontConfirmRemoveWriteKey"),
			TDCBF_YES_BUTTON | TDCBF_NO_BUTTON,
			IDNO, 
			IDYES);

		if (IDYES == response) 
		{
			BOOL fSuccess = m_pDevice->SetAsReadOnly();
			if (!fSuccess) 
			{
				ErrorMessageBox(m_hWnd, IDS_ERROR_REMOVE_WRITE_KEY);
			}
			else 
			{
				_UpdateDeviceData();
			}
		}
	}
	else 
	{
		CDeviceAddWriteKeyDlg dlg;
		dlg.SetDeviceId(m_pDevice->GetStringId());
		dlg.SetDeviceName(m_pDevice->GetName());
		INT_PTR iResult = dlg.DoModal();

		ATLTRACE("iResult = %d\n", iResult);

		if (iResult == IDOK) 
		{
			BOOL fSuccess = m_pDevice->SetAsReadWrite(dlg.GetWriteKey());
			if (!fSuccess) 
			{
				ErrorMessageBox(m_hWnd, IDS_ERROR_ADD_WRITE_KEY);
			}
			else 
			{
				_UpdateDeviceData();
			}
		}
		else 
		{
			;
		}
	}
}

void
CDeviceGeneralPage::OnReset()
{
	ATLTRACE(__FUNCTION__ "\n");
	m_imageList.Destroy();
	
	if (!m_hWorkerThread[0].IsInvalid())
	{
		ATLTRACE("Worker thread is still running! Waiting...\n");
		DWORD waitResult = ::WaitForSingleObject(m_hWorkerThread[0], 30000);
		if (WAIT_OBJECT_0 != waitResult)
		{
			ATLTRACE("Wait failed. Forcibly terminating the thread...\n");
			(void) ::TerminateThread(m_hWorkerThread[0], 0);
		}
		else
		{
			ATLTRACE("Worker thread stopped...\n");
		}
	}
	if (!m_hWorkerThread[1].IsInvalid())
	{
		ATLTRACE("Worker thread 2 is still running! Waiting...\n");
		DWORD waitResult = ::WaitForSingleObject(m_hWorkerThread[1], 30000);
		if (WAIT_OBJECT_0 != waitResult)
		{
			ATLTRACE("Wait failed. Forcibly terminating the thread 2...\n");
			(void) ::TerminateThread(m_hWorkerThread[1], 0);
		}
		else
		{
			ATLTRACE("Worker thread 2 stopped...\n");
		}
	}
}

LRESULT 
CDeviceGeneralPage::OnSetCursor(HWND hWnd, UINT hit, UINT msg)
{
	if (!IsIconic())
	{
		SetCursor(m_hCursor);
	}
	return TRUE;
}

void 
CDeviceGeneralPage::_GrabUnitDeviceControls()
{
	m_unitDeviceControls.clear();
	EnumChildWindows(m_hWnd, EnumChildProc, reinterpret_cast<LPARAM>(this));
	std::vector<HWND>::iterator itr = std::find(
		m_unitDeviceControls.begin(),
		m_unitDeviceControls.end(),
		m_wndUnitDeviceType.m_hWnd);
	m_unitDeviceControls.erase(
		m_unitDeviceControls.begin(), 
		++itr);
}

void 
CDeviceGeneralPage::_ShowUnitDeviceControls(int nCmdShow)
{
	std::for_each(
		m_unitDeviceControls.begin(),
		m_unitDeviceControls.end(),
		ShowWindowFunctor(nCmdShow));
}

void
CDeviceGeneralPage::_InitData()
{
	//
	// Device Properties
	//
	_UpdateDeviceData();

	//
	// Unit Device Properties
	//
	m_pUnitDevices = m_pDevice->GetUnitDevices();
	DWORD nUnitDevices = m_pUnitDevices.size();

	if (0 == nUnitDevices) 
	{
		m_wndUnitDeviceType.SetWindowText(_T(""));
		_ShowUnitDeviceControls(SW_HIDE);
		m_wndNA.ShowWindow(SW_SHOW);
		m_wndUnitDeviceList.ShowWindow(SW_HIDE);
	}
	else
	{
		m_wndNA.ShowWindow(SW_HIDE);
		_ShowUnitDeviceControls(SW_SHOW);
		m_wndUnitDeviceList.ShowWindow((nUnitDevices == 1) ? SW_HIDE : SW_SHOW);
		m_wndUnitDeviceType.ShowWindow((nUnitDevices == 1) ? SW_SHOW : SW_HIDE);
		// Initiate Unit Device Information Update (async)
		m_unitDeviceUpdated.resize(m_pUnitDevices.size(), false);

		std::for_each(
			m_pUnitDevices.begin(),
			m_pUnitDevices.end(),
			UpdateUnitDeviceData(this));

		std::for_each(
			m_pUnitDevices.begin(),
			m_pUnitDevices.end(),
			AddToComboBox(m_wndUnitDeviceList));

		m_wndUnitDeviceList.SetCurSel(0);
		_SetCurUnitDevice(0);
	}
}

void 
CDeviceGeneralPage::_SetCurUnitDevice(int n)
{
	ATLASSERT(static_cast<size_t>(n) < m_pUnitDevices.size());
	if (static_cast<size_t>(n) < m_pUnitDevices.size())
	{
		ndas::UnitDevicePtr pUnitDevice = m_pUnitDevices.at(n);
		m_pUnitDevices[n] = pUnitDevice;
		m_curUnitDevice = n;

		// Type
		if (1 == m_pUnitDevices.size())
		{
			NDAS_UNITDEVICE_TYPE type = pUnitDevice->GetType();
			NDAS_UNITDEVICE_SUBTYPE subType = pUnitDevice->GetSubType();

			CString strType;
			pUnitDeviceTypeString(strType, type, subType);
			m_wndUnitDeviceType.SetWindowText(strType);			
		}
		else
		{
			// m_wndUnitDeviceList will show this
			m_wndUnitDeviceType.SetWindowText(_T(""));
		}

		// Status
		NDAS_UNITDEVICE_STATUS status = pUnitDevice->GetStatus();
		NDAS_UNITDEVICE_ERROR error = pUnitDevice->GetLastError();

		CString strStatus;
		pUnitDeviceStatusString(strStatus, status, error);
		m_wndUnitDeviceStatus.SetWindowText(strStatus);

		// Capacity
		const NDAS_UNITDEVICE_HARDWARE_INFO* pudinfo = pUnitDevice->GetHWInfo();
		if (NULL != pudinfo) 
		{
			CString str = pBlockCapacityString(
				pudinfo->SectorCount.LowPart, 
				pudinfo->SectorCount.HighPart);
			m_wndUnitDeviceCapacity.SetWindowText(str);
		}

		//
		// Image List contains the following images
		//
		// 0: Disk
		// 1: DVD
		// 2: CD-ROM
		//
		NDAS_UNITDEVICE_TYPE type = pUnitDevice->GetType();
		if (NDAS_UNITDEVICE_TYPE_DISK == type) 
		{
			m_wndUnitDeviceIcon.SetIcon(m_imageList.GetIcon(0));
		}
		else if (NDAS_UNITDEVICE_TYPE_CDROM == type)
		{
			m_wndUnitDeviceIcon.SetIcon(m_imageList.GetIcon(2));
		}
		else
		{
			// no icon will show
		}

		if (m_unitDeviceUpdated[n])
		{
			CString str;
			str.Format(_T("%d"), pUnitDevice->GetROHostCount());
			if (m_wndUnitDeviceROHosts.IsWindow())
			{
				m_wndUnitDeviceROHosts.SetWindowText(str);
			}

			str.Format(_T("%d"), pUnitDevice->GetRWHostCount());
			if (m_wndUnitDeviceRWHosts.IsWindow())
			{
				m_wndUnitDeviceRWHosts.SetWindowText(str);
			}
		}

		_UpdateLogDeviceData(pUnitDevice);
	}
}

void
CDeviceGeneralPage::_UpdateDeviceData()
{
	m_pDevice->UpdateStatus();
	m_pDevice->UpdateInfo();

	m_wndDeviceName.SetWindowText(m_pDevice->GetName());

	CString strDevId = m_pDevice->GetStringId();
	CString strFmtDevId = pDelimitedDeviceIdString(strDevId, m_chConcealed);
	m_wndDeviceId.SetWindowText(strFmtDevId);

	if (GENERIC_WRITE & m_pDevice->GetGrantedAccess()) 
	{

		CString str = MAKEINTRESOURCE(IDS_WRITE_KEY_PRESENT);
		m_wndDeviceWriteKey.SetWindowText(str);

		CString strButton = MAKEINTRESOURCE(IDS_REMOVE_WRITE_KEY);
		m_wndAddRemoveWriteKey.SetWindowText(strButton);

	}
	else 
	{
		CString str = MAKEINTRESOURCE(IDS_WRITE_KEY_NONE);
		m_wndDeviceWriteKey.SetWindowText(str);

		CString strButton = MAKEINTRESOURCE(IDS_ADD_WRITE_KEY);
		m_wndAddRemoveWriteKey.SetWindowText(strButton);
	}

	CString strStatus = pDeviceStatusString(
		m_pDevice->GetStatus(),
		m_pDevice->GetLastError());

	m_wndDeviceStatus.SetWindowText(strStatus);
}

void
CDeviceGeneralPage::_UpdateUnitDeviceData(
	ndas::UnitDevicePtr pUnitDevice)
{
	pUnitDevice->UpdateStatus();
	pUnitDevice->UpdateInfo();

	m_hCursor = AtlLoadSysCursor(IDC_APPSTARTING);
	SetCursor(m_hCursor);

	int index = pUnitDevice->GetUnitNo();
	ATLASSERT(index < RTL_NUMBER_OF(m_hWorkerThread));
	m_hWorkerThread[index] = CreateWorkerThreadParam(ULongToPtr(index));
}

void
CDeviceGeneralPage::_UpdateLogDeviceData(
	ndas::UnitDevicePtr pUnitDevice)
{
	NDAS_LOGICALDEVICE_ID logDeviceId = pUnitDevice->GetLogicalDeviceId();

	ndas::LogicalDevicePtr pLogDevice;
	if (!ndas::FindLogicalDevice(pLogDevice, logDeviceId))
	{
		m_wndLogDeviceTree.DeleteAllItems();
		m_wndLogDeviceTree.ShowWindow(SW_HIDE);
		m_wndLogDeviceNA.ShowWindow(SW_SHOW);
		return;
	}
	else
	{
		m_wndLogDeviceTree.ShowWindow(SW_SHOW);
		m_wndLogDeviceNA.ShowWindow(SW_HIDE);
	}

	//
	// TODO: To handle errors
	//
	(void) pLogDevice->UpdateInfo();
	(void) pLogDevice->UpdateStatus();

	NDAS_LOGICALDEVICE_TYPE logDevType = pLogDevice->GetType();
	NDAS_LOGICALDEVICE_STATUS logDevStatus = pLogDevice->GetStatus();

	CString strStatus = pLogicalDeviceStatusString(
		logDevStatus, 
		pLogDevice->GetLastError(), 
		pLogDevice->GetMountedAccess());

	CString strType = pLogicalDeviceTypeString(logDevType);

	const NDASUSER_LOGICALDEVICE_INFORMATION* pLogDeviceInfo = pLogDevice->GetLogicalDeviceInfo();

	CString strCapacity = pBlockCapacityString(
		pLogDeviceInfo->SubType.LogicalDiskInformation.Blocks);

	CString strRootNode;
	strRootNode.Format(_T("%s (%s) - %s"), strType, strCapacity, strStatus);

	m_wndLogDeviceTree.DeleteAllItems();

	CTreeItem rootItem = m_wndLogDeviceTree.InsertItem(
		strRootNode, 
		TVI_ROOT, 
		TVI_LAST);

	CString strNodeText;
	CString strLocationCaption;
//	strLocationCaption.LoadString(IDS_LOGDEV_LOCATION);
//	strNodeText.Format(_T("%s %s"), strLocationCaption, strLocation);
//	strNodeText.Format(_T("%s"), strLocation);
//	CTreeItem tiLocation = tiRoot.AddTail(strNodeText, 0);
//	tiLocation.Expand();

	CTreeItem memberItem = rootItem; // .AddTail(_T("Members"), 0);

	//
	// Encryption information
	//
	{
		if (NULL != pLogDeviceInfo && 
			IS_NDAS_LOGICALDEVICE_TYPE_DISK(pLogDevice->GetType()) &&
			0 != pLogDeviceInfo->SubType.LogicalDiskInformation.ContentEncrypt.Revision &&
			NDAS_CONTENT_ENCRYPT_TYPE_NONE != pLogDeviceInfo->SubType.LogicalDiskInformation.ContentEncrypt.Type)
		{
			CString strNode = pNdasLogicalDiskEncryptString(pLogDeviceInfo);
			memberItem.AddTail(strNode, 0);
			// m_wndLogDeviceTree.InsertItem(strNode, TVI_ROOT, TVI_LAST);
		}
	}

	for (DWORD i = 0; i < pLogDevice->GetUnitDeviceInfoCount(); ++i) 
	{

		CString strNode;
		ndas::LogicalDevice::UNITDEVICE_INFO ui = 
			pLogDevice->GetUnitDeviceInfo(i);

		ndas::DevicePtr pMemberDevice;
		if (!ndas::FindDeviceByNdasId(pMemberDevice, ui.DeviceId))
		{
			CString strMissingDeviceId = 
				pDelimitedDeviceIdString2(CString(ui.DeviceId), m_chConcealed);

			if (0 == ui.UnitNo)
			{
				// "[%1!d!] %2!s! (Not registered)"
				strNode.FormatMessage(
					IDS_LOGICALDEVICE_ENTRY_MISSING_0_FMT, 
					ui.Index + 1,
					strMissingDeviceId);
			}
			else
			{
				// "[%1!d!] %2!s!:%3!d! (Not registered)"
				strNode.FormatMessage(
					IDS_LOGICALDEVICE_ENTRY_MISSING_0_FMT, 
					ui.Index + 1,
					strMissingDeviceId,
					ui.UnitNo + 1);
			}
			memberItem.AddTail(strNode, 0);
			continue;
		}

		ndas::UnitDevicePtr pMemberUnitDevice;

		if (!pMemberDevice->FindUnitDevice(pMemberUnitDevice, ui.UnitNo))
		{
			if (0 == ui.UnitNo)
			{
				// "[%1!d!] Unavailable (%2!s!)"
				strNode.FormatMessage(IDS_LOGICALDEVICE_ENTRY_UNAVAILABLE_0_FMT, 
					ui.Index + 1,
					pMemberDevice->GetName());
			}
			else
			{
				// "[%1!d!] Unavailable (%2!s!:%3!d!)"
				strNode.FormatMessage(IDS_LOGICALDEVICE_ENTRY_UNAVAILABLE_FMT, 
					ui.Index + 1,
					pMemberDevice->GetName(), 
					ui.UnitNo + 1);
			}
			memberItem.AddTail(strNode, 0);
			continue;
		}

		if (0 == ui.UnitNo)
		{
			// 	"[%1!d!] %2!s!"
			strNode.FormatMessage(IDS_LOGICALDEVICE_ENTRY_0_FMT, 
				ui.Index + 1, 
				pMemberDevice->GetName());
		}
		else
		{
			// 	"[%1!d!] %2!s!:%3!d! "
			strNode.FormatMessage(IDS_LOGICALDEVICE_ENTRY_FMT, 
				ui.Index + 1, 
				pMemberDevice->GetName(), 
				ui.UnitNo + 1);
		}

		memberItem.AddTail(strNode, 0);
	}

	memberItem.Expand();
	rootItem.Expand();

}

//////////////////////////////////////////////////////////////////////////
//
// Rename Device Dialog
// Invoked from General Page
//
//////////////////////////////////////////////////////////////////////////

CDeviceRenameDialog::CDeviceRenameDialog()
{}

LRESULT 
CDeviceRenameDialog::OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
	m_wndName.Attach(GetDlgItem(IDC_DEVICE_NAME));
	m_wndOK.Attach(GetDlgItem(IDOK));

	m_wndName.SetLimitText(MAX_NAME_LEN - 1);
	m_wndName.SetWindowText(m_szName);
	m_wndName.SetFocus();
	m_wndName.SetSelAll();

	return 0;
}

void 
CDeviceRenameDialog::SetName(LPCTSTR szName)
{
	HRESULT hr = ::StringCchCopyN(
		m_szName, 
		MAX_NAME_LEN + 1, 
		szName, 
		MAX_NAME_LEN);
	ATLASSERT(SUCCEEDED(hr));
	hr = ::StringCchCopyN(
		m_szOldName,
		MAX_NAME_LEN + 1,
		szName,
		MAX_NAME_LEN);
	ATLASSERT(SUCCEEDED(hr));
}

LPCTSTR 
CDeviceRenameDialog::GetName()
{
	return m_szName;
}

LRESULT 
CDeviceRenameDialog::OnClose(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CEdit wndName(GetDlgItem(IDC_DEVICE_NAME));
	wndName.GetWindowText(m_szName, MAX_NAME_LEN + 1);

	if (IDOK == wID && 0 != ::lstrcmp(m_szOldName, m_szName))
	{
		//
		// name is changed! check duplicate
		//
		ndas::DevicePtr pExistingDevice;
		if (ndas::FindDeviceByName(pExistingDevice, m_szName))
		{

			AtlTaskDialogEx(
				m_hWnd,
				IDS_MAIN_TITLE,
				0U,
				IDS_ERROR_DUPLICATE_NAME,
				TDCBF_OK_BUTTON,
				TD_ERROR_ICON);

			m_wndName.SetFocus();
			m_wndName.SetSel(0, -1);
			return 0;
		}
	}

	EndDialog(wID);
	return 0;
}

LRESULT
CDeviceRenameDialog::OnDeviceNameChange(UINT uCode, int nCtrlID, HWND hwndCtrl)
{
	_ASSERTE(hwndCtrl == m_wndName.m_hWnd);

	INT iLen = m_wndName.GetWindowTextLength();
	if (iLen == 0) 
	{
		m_wndOK.EnableWindow(FALSE);
	}
	else 
	{
		m_wndOK.EnableWindow(TRUE);
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
//
// Add Write Key Dialog
// Invoked from General Page
//
//////////////////////////////////////////////////////////////////////////

LRESULT 
CDeviceAddWriteKeyDlg::OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
	CWindow wndDeviceName = GetDlgItem(IDC_DEVICE_NAME);
	CWindow wndDeviceId = GetDlgItem(IDC_DEVICE_ID);
	CEdit wndWriteKey = GetDlgItem(IDC_DEVICE_WRITE_KEY);
	

	TCHAR chPassword = _T('*');

	// Temporary edit control to get an effective password character
	{
		CEdit wndPassword;
		wndPassword.Create(m_hWnd, NULL, NULL, WS_CHILD | ES_PASSWORD);
		chPassword = wndPassword.GetPasswordChar();
		wndPassword.DestroyWindow();
	}

	CString strFmtDeviceId;
	pDelimitedDeviceIdString(strFmtDeviceId, m_strDeviceId, chPassword);

	wndDeviceName.SetWindowText(m_strDeviceName);
	wndDeviceId.SetWindowText(strFmtDeviceId);
	wndWriteKey.SetLimitText(NDAS_DEVICE_WRITE_KEY_LEN);

	m_butOK.Attach(GetDlgItem(IDOK));
	m_butOK.EnableWindow(FALSE);

	return TRUE;
}

void
CDeviceAddWriteKeyDlg::SetDeviceName(LPCTSTR szDeviceName)
{
	m_strDeviceName = szDeviceName;
}

void
CDeviceAddWriteKeyDlg::SetDeviceId(LPCTSTR szDeviceId)
{
	m_strDeviceId = szDeviceId;
}

LPCTSTR
CDeviceAddWriteKeyDlg::GetWriteKey()
{
	return m_strWriteKey;
}

void 
CDeviceAddWriteKeyDlg::OnOK(UINT /*wNotifyCode*/, int /* wID */, HWND /*hWndCtl*/)
{
	EndDialog(IDOK);
}

void 
CDeviceAddWriteKeyDlg::OnCancel(UINT /*wNotifyCode*/, int /* wID */, HWND /*hWndCtl*/)
{
	EndDialog(IDCANCEL);
}

void
CDeviceAddWriteKeyDlg::OnWriteKeyChange(UINT /*wNotifyCode*/, int /*wID*/, HWND hWndCtl)
{
	CEdit wndWriteKey = hWndCtl;

	wndWriteKey.GetWindowText(
		m_strWriteKey.GetBuffer(NDAS_DEVICE_WRITE_KEY_LEN + 1),
		NDAS_DEVICE_WRITE_KEY_LEN + 1);

	m_strWriteKey.ReleaseBuffer();

	CButton wndOK = GetDlgItem(IDOK);
	if (m_strWriteKey.GetLength() == 5) 
	{
		BOOL fValid = ::NdasValidateStringIdKey(
			m_strDeviceId, 
			m_strWriteKey);
		wndOK.EnableWindow(fValid);
	}
	else 
	{
		wndOK.EnableWindow(FALSE);
	}

	SetMsgHandled(FALSE);
}

//////////////////////////////////////////////////////////////////////////
//
// Device Hardware Property Page
//
//////////////////////////////////////////////////////////////////////////


CDeviceHardwarePage::CDeviceHardwarePage()
{
}

LRESULT 
CDeviceHardwarePage::OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
	ATLASSERT(NULL != m_pDevice.get());
	if (NULL == m_pDevice.get()) 
	{
		return 0;
	}

	m_propList.SubclassWindow(GetDlgItem(IDC_PROPLIST));
	m_propList.SetExtendedListStyle(PLS_EX_CATEGORIZED);


	//
	// Device Hardware Information
	//
	boost::shared_ptr<const NDAS_DEVICE_HARDWARE_INFO> pDevHWInfo = 
		m_pDevice->GetHardwareInfo();

	CString strValue = (LPCTSTR) IDS_DEVPROP_CATEGORY_HARDWARE;
	m_propList.AddItem(PropCreateCategory(strValue));

	if (0 == pDevHWInfo.get()) 
	{
		CString str = MAKEINTRESOURCE(IDS_DEVICE_HARDWARE_INFO_NA);
		m_propList.AddItem(PropCreateSimple(str, _T("")));
		return 0;
	}

	CString strType;

	// Version
	strType.LoadString(IDS_DEVPROP_HW_VERSION);
	pHWVersionString(strValue, pDevHWInfo.get());
	m_propList.AddItem(PropCreateSimple(strType, strValue));

	// Revision
	if (pDevHWInfo->HardwareRevision != 0)
	{
		strType.LoadString(IDS_DEVPROP_HW_REVISION);
		strValue.Format(_T("%X"), pDevHWInfo->HardwareRevision);
		m_propList.AddItem(PropCreateSimple(strType, strValue));
	}

	// Max Request Blocks
	strType.LoadString(IDS_DEVPROP_HW_MAX_REQUEST_BLOCKS);
	strValue.Format(_T("%d"), pDevHWInfo->MaximumTransferBlocks);
	m_propList.AddItem(PropCreateSimple(strType, strValue));

	// Slots
	strType.LoadString(IDS_DEVPROP_HW_SLOT_COUNT);
	strValue.Format(_T("%d"), pDevHWInfo->NumberOfCommandProcessingSlots);
	m_propList.AddItem(PropCreateSimple(strType, strValue));

	// Targets
	//strType.LoadString(IDS_DEVPROP_HW_TARGET_COUNT);
	//strValue.Format(_T("%d"), pDevHWInfo->NumberOfTargets);
	//m_propList.AddItem(PropCreateSimple(strType, strValue));

	// Max Targets
	strType.LoadString(IDS_DEVPROP_HW_MAX_TARGET_COUNT);
	strValue.Format(_T("%d"), pDevHWInfo->MaximumNumberOfTargets);
	m_propList.AddItem(PropCreateSimple(strType, strValue));

	// Max LUs
	strType.LoadString(IDS_DEVPROP_HW_MAX_LU_COUNT);
	strValue.Format(_T("%d"), pDevHWInfo->MaximumNumberOfLUs);
	m_propList.AddItem(PropCreateSimple(strType, strValue));

	boost::shared_ptr<const NDAS_DEVICE_HARDWARE_INFO> phwi = m_pDevice->GetHardwareInfo();
	if (NULL != phwi.get() && 
		!IsNullNdasDeviceId(phwi->NdasDeviceId))
	{
		strType.LoadString(IDS_DEVPRO_HW_MAC_ADDRESS);
		strValue.Format(_T("%02X:%02X:%02X:%02X:%02X:%02X"),
			phwi->NdasDeviceId.Node[0],
			phwi->NdasDeviceId.Node[1],
			phwi->NdasDeviceId.Node[2],
			phwi->NdasDeviceId.Node[3],
			phwi->NdasDeviceId.Node[4],
			phwi->NdasDeviceId.Node[5]);
		m_propList.AddItem(PropCreateSimple(strType, strValue));
	}

	//
	// Unit Device Hardware Information
	//

	const ndas::UnitDeviceVector& unitDevices = m_pDevice->GetUnitDevices();
	for (DWORD i = 0; i < unitDevices.size(); ++i) 
	{
		ndas::UnitDevicePtr pUnitDevice = unitDevices.at(i);

		const NDAS_UNITDEVICE_HARDWARE_INFO* pHWI = pUnitDevice->GetHWInfo();

		strValue.FormatMessage(IDS_DEVPROP_UNITDEV_TITLE_FMT, i + 1);

		m_propList.AddItem(PropCreateCategory(strValue));

		if (NULL == pHWI) 
		{
			// "Not available"
			CString str = MAKEINTRESOURCE(IDS_UNITDEVICE_HARDWARE_INFO_NA);
			m_propList.AddItem(PropCreateSimple(str, _T("")));
			continue;
		}

		// Media Type
		strType.LoadString(IDS_DEVPROP_UNITDEV_DEVICE_TYPE);
		pUnitDeviceMediaTypeString(strValue, pHWI->MediaType);
		m_propList.AddItem(PropCreateSimple(strType, strValue));

		CString modeDelimiter(MAKEINTRESOURCE(IDS_MODE_DELIMITER));

		// Transfer Mode
		strType.LoadString(IDS_DEVPROP_UNITDEV_TRANSFER_MODE);
		strValue = _T("");
		{
			const struct { 
				const BOOL& Mode;
				UINT ResID;
			} TransferModeStrings[] = {
				pHWI->PIO, IDS_TRANSFER_MODE_PIO,
				pHWI->DMA, IDS_TRANSFER_MODE_DMA,
				pHWI->UDMA, IDS_TRANSFER_MODE_UDMA
			};

			bool multipleModes = false;
			for (int i = 0; i < RTL_NUMBER_OF(TransferModeStrings); ++i)
			{
				if (TransferModeStrings[i].Mode)
				{
					if (multipleModes) strValue += modeDelimiter;
					strValue += CString(MAKEINTRESOURCE(TransferModeStrings[i].ResID));
					multipleModes = true;
				}
			}
			m_propList.AddItem(PropCreateSimple(strType, strValue));
		}


		// LBA support?
		strType.LoadString(IDS_DEVPROP_UNITDEV_LBA_MODE);
		strValue = _T("");
		{
			const struct { 
				const BOOL& Mode;
				UINT ResID;
			} LbaModeStrings[] = {
				pHWI->LBA, IDS_LBA_MODE_LBA,
				pHWI->LBA48, IDS_LBA_MODE_LBA48
			};

			bool multipleModes = false;
			for (int i = 0; i < RTL_NUMBER_OF(LbaModeStrings); ++i)
			{
				if (LbaModeStrings[i].Mode)
				{
					if (multipleModes) strValue += modeDelimiter;
					strValue += CString(MAKEINTRESOURCE(LbaModeStrings[i].ResID));
					multipleModes = true;
				}
			}
			m_propList.AddItem(PropCreateSimple(strType, strValue));
		}

		// Model
		strType.LoadString(IDS_DEVPROP_UNITDEV_MODEL);
		strValue = pHWI->Model;
		strValue.TrimLeft();
		strValue.TrimRight(); 
		m_propList.AddItem(PropCreateSimple(strType, strValue));

		// FWRev
		strType.LoadString(IDS_DEVPROP_UNITDEV_FWREV);
		strValue = pHWI->FirmwareRevision;
		strValue.TrimLeft();
		strValue.TrimRight(); 
		m_propList.AddItem(PropCreateSimple(strType, strValue));

		// Serial No
		strType.LoadString(IDS_DEVPROP_UNITDEV_SERIALNO);
		strValue = pHWI->SerialNumber;
		strValue.TrimLeft();
		strValue.TrimRight(); 
		m_propList.AddItem(PropCreateSimple(strType, strValue));
	}

	return 0;
}

void 
CDeviceHardwarePage::SetDevice(ndas::DevicePtr pDevice)
{
	m_pDevice = pDevice;
}

//////////////////////////////////////////////////////////////////////////
//
// Host Statistics Page
//
//////////////////////////////////////////////////////////////////////////

CDeviceHostStatPage::CDeviceHostStatPage() :
	m_hWorkerThread(NULL),
	m_hAccel(NULL)
{
}

CDeviceHostStatPage::CDeviceHostStatPage(ndas::UnitDevicePtr pUnitDevice) :
	m_pUnitDevice(pUnitDevice),
	m_hWorkerThread(NULL)
{
}

void
CDeviceHostStatPage::SetUnitDevice(ndas::UnitDevicePtr pUnitDevice)
{
	ATLASSERT(NULL != pUnitDevice.get());
	m_pUnitDevice = pUnitDevice;
}

LRESULT
CDeviceHostStatPage::OnInitDialog(HWND hwndFocus, LPARAM lParam)
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
CDeviceHostStatPage::OnWorkerThreadStart()
{
	BOOL fSuccess = m_pUnitDevice->UpdateHostInfo();
	if (!fSuccess) 
	{
		//		m_wndListView.AddItem(0,0,_T("Unavailable"));
		PostMessage(WM_USER_DONE);
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

	PostMessage(WM_USER_DONE);

	return 0;
}

BOOL 
CDeviceHostStatPage::OnTranslateAccelerator(LPMSG lpMsg)
{
	int ret = ::TranslateAccelerator(m_hWnd, m_hAccel, lpMsg);
	return ret ? PSNRET_MESSAGEHANDLED : PSNRET_NOERROR;
}

void
CDeviceHostStatPage::OnReset()
{
	ATLTRACE(__FUNCTION__ "\n");
	if (NULL != m_hWorkerThread)
	{
		DWORD waitResult = ::WaitForSingleObject(m_hWorkerThread, 30000);
		if (WAIT_OBJECT_0 != waitResult)
		{
			ATLTRACE("Wait for worker thread failed. Forcibly terminating the thread...\n");
			(void) ::TerminateThread(m_hWorkerThread, 0);
		}
		::CloseHandle(m_hWorkerThread);
		m_hWorkerThread = NULL;
	}
	
	(void) m_findHostsAnimate.Close(); // always returns FALSE
	
	if (m_hAccel && !::DestroyAcceleratorTable(m_hAccel))
	{
		ATLASSERT(FALSE && "DestroyAcceleratorTable failed\n");
	}
}

HBRUSH 
CDeviceHostStatPage::OnCtlColorStatic(HDC hCtlDC, HWND hWndCtl)
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
CDeviceHostStatPage::OnWorkDone(UINT uMsg, WPARAM wParam, LPARAM lParam)
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

	return TRUE;
}

LRESULT 
CDeviceHostStatPage::OnSetCursor(HWND hWnd, UINT hit, UINT msg)
{
	if (!IsIconic())
	{
		SetCursor(m_hCursor);
	}
	return TRUE;
}

void
CDeviceHostStatPage::BeginRefreshHosts()
{
	m_wndRefreshLink.EnableWindow(FALSE);

	m_findHostsAnimate.ShowWindow(SW_SHOW);
	m_findHostsAnimate.Play(0,-1,-1);

	m_hostInfoDataArray.RemoveAll();
	m_wndListView.DeleteAllItems();

	m_hWorkerThread = CreateWorkerThread();
	if (NULL == m_hWorkerThread)
	{
		ATLTRACE("Worker Thread creation failed %d\n", ::GetLastError());
	}

	m_hCursor = AtlLoadSysCursor(IDC_APPSTARTING);
	SetCursor(m_hCursor);
}

void 
CDeviceHostStatPage::OnCmdRefresh(UINT, int, HWND)
{
	BeginRefreshHosts();
}

//////////////////////////////////////////////////////////////////////////
//
// Advanced Page
//
//////////////////////////////////////////////////////////////////////////

CDeviceAdvancedPage::CDeviceAdvancedPage()
{
}

CDeviceAdvancedPage::CDeviceAdvancedPage(ndas::DevicePtr pDevice) :
	m_pDevice(pDevice)
{
}

void
CDeviceAdvancedPage::SetDevice(ndas::DevicePtr pDevice)
{
	ATLASSERT(NULL != pDevice.get());
	m_pDevice = pDevice;
}

LRESULT 
CDeviceAdvancedPage::OnInitDialog(HWND hwndFocus, LPARAM lParam)
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

	// Set initial focus
	return 1;
}

void
CDeviceAdvancedPage::OnDeactivateDevice(UINT uCode, int nCtrlID, HWND hwndCtrl)
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
CDeviceAdvancedPage::OnResetDevice(UINT uCode, int nCtrlID, HWND hwndCtrl)
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

	::SetCursor(AtlLoadSysCursor(IDC_WAIT));

	CSimpleWaitDlg().DoModal(m_hWnd, 2000);

	::SetCursor(AtlLoadSysCursor(IDC_ARROW));

	if (!m_pDevice->Enable(TRUE)) 
	{
		ErrorMessageBox(m_hWnd, IDS_ERROR_RESET_DEVICE);
	}

	::PostMessage(GetParent(), IDCLOSE, 0, 0);
}

void 
CDeviceAdvancedPage::OnCmdExport(UINT uCode, int nCtrlID, HWND hwndCtrl)
{
	CExportDlg exportDlg;
	exportDlg.DoModal(m_hWnd, reinterpret_cast<LPARAM>(&m_pDevice));
}
