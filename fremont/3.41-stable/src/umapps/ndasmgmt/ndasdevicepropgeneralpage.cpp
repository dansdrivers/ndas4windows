#include "stdafx.h"
#include "ndasmgmt.h"

#include "confirmdlg.h"
#include "ndastypestr.h"
#include "apperrdlg.h"
#include "ndasdevicerenamedlg.h"
#include "ndasdeviceaddwritekeydlg.h"

#include "ndasdevicepropgeneralpage.h"

LONG DbgLevelMagmPage = DBG_LEVEL_MAGM_PAGE;

#define NdasUiDbgCall(l,x,...) do {							\
    if (l <= DbgLevelMagmPage) {							\
        ATLTRACE("|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
		ATLTRACE (x,__VA_ARGS__);							\
    } 														\
} while(0)

namespace 
{
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

	struct AddToComboBox : std::unary_function<ndas::UnitDevicePtr, void> 
	{
		AddToComboBox(const CComboBox& wnd) : wnd(wnd) {}
		void operator()(const ndas::UnitDevicePtr& pUnitDevice) 
		{
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
}

CNdasDevicePropGeneralPage::CNdasDevicePropGeneralPage() :
	m_ThreadCompleted(CreateEvent(NULL, TRUE, TRUE, FALSE)),
	m_ThreadCount(0)
{
}

LRESULT 
CNdasDevicePropGeneralPage::OnInitDialog(HWND hwndFocus, LPARAM lParam)
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

	// Support F5 to refresh
	ACCEL accel = {0};
	accel.fVirt = FVIRTKEY;
	accel.key = VK_F5;
	accel.cmd = IDC_REFRESH_HOST;

	m_hAccel = ::CreateAcceleratorTable(&accel, 1);
	ATLASSERT(NULL != m_hAccel);

	return 0;
}

BOOL 
CNdasDevicePropGeneralPage::OnEnumChild(HWND hWnd)
{
	m_unitDeviceControls.push_back(hWnd);
	return TRUE;
}

void 
CNdasDevicePropGeneralPage::OnUnitDeviceSelChange(UINT, int, HWND)
{
	int n = m_wndUnitDeviceList.GetCurSel();
	if (CB_ERR != n)
	{
		_SetCurUnitDevice(n);
	}
}

void
CNdasDevicePropGeneralPage::pNotifyThreadCompleted()
{
	m_ThreadCount--;
	ATLASSERT(m_ThreadCount >= 0);
	if (0 == m_ThreadCount)
	{
		ATLVERIFY(SetEvent(m_ThreadCompleted));
	}
}

DWORD 
CNdasDevicePropGeneralPage::spUpdateThreadStart(LPVOID Context)
{
	UpdateThreadParam* param = static_cast<UpdateThreadParam*>(Context);
	DWORD ret = param->Instance->pUpdateThreadStart(param->UnitIndex);
	delete param;
	return ret;
}

DWORD 
CNdasDevicePropGeneralPage::pUpdateThreadStart(DWORD UnitIndex)
{
	ATLASSERT(UnitIndex < RTL_NUMBER_OF(m_pUnitDevices));
	if (UnitIndex >= RTL_NUMBER_OF(m_pUnitDevices))
	{
		ATLVERIFY(PostMessage(
			WM_THREADED_WORK_COMPLETED, 0, static_cast<LPARAM>(UnitIndex)));
		return 255;
	}

	ndas::UnitDevicePtr pUnitDevice = m_pUnitDevices[UnitIndex];
	if (!pUnitDevice)
	{
		ATLVERIFY(PostMessage(
			WM_THREADED_WORK_COMPLETED, 0, static_cast<LPARAM>(UnitIndex)));
		return 1;
	}

	(void) pUnitDevice->UpdateHostStats();
	ATLVERIFY(PostMessage(
		WM_THREADED_WORK_COMPLETED, 0, static_cast<LPARAM>(UnitIndex)));

	return 0;
}

LRESULT 
CNdasDevicePropGeneralPage::OnUpdateThreadCompleted(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	UINT i = static_cast<UINT>(lParam);

	ATLASSERT(i < RTL_NUMBER_OF(m_pUnitDevices));
	if (i >= RTL_NUMBER_OF(m_pUnitDevices))
	{
		m_hCursor = AtlLoadSysCursor(IDC_ARROW);
		::SetCursor(m_hCursor);
		pNotifyThreadCompleted();
		return 255;
	}

	ndas::UnitDevicePtr pUnitDevice = m_pUnitDevices[i];

	if (!pUnitDevice.get())
	{
		ATLASSERT(FALSE);
		m_hCursor = AtlLoadSysCursor(IDC_ARROW);
		::SetCursor(m_hCursor);
		pNotifyThreadCompleted();
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

	pNotifyThreadCompleted();

	return TRUE;
}

void 
CNdasDevicePropGeneralPage::SetDevice(ndas::DevicePtr pDevice)
{
	m_pDevice = pDevice;
}

void 
CNdasDevicePropGeneralPage::OnRename(UINT, int, HWND)
{
	CNdasDeviceRenameDlg wndRename;
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
CNdasDevicePropGeneralPage::OnModifyWriteKey(UINT, int, HWND)
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
		CNdasDeviceAddWriteKeyDlg dlg;
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
CNdasDevicePropGeneralPage::OnReset()
{
	ATLTRACE(__FUNCTION__ "\n");

	ATLVERIFY(AtlWaitWithMessageLoop(m_ThreadCompleted));

	ATLVERIFY(m_imageList.Destroy());
	ATLVERIFY(DestroyAcceleratorTable(m_hAccel));
}

LRESULT 
CNdasDevicePropGeneralPage::OnSetCursor(HWND hWnd, UINT hit, UINT msg)
{
	if (!IsIconic())
	{
		SetCursor(m_hCursor);
	}
	return TRUE;
}

void 
CNdasDevicePropGeneralPage::_GrabUnitDeviceControls()
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
CNdasDevicePropGeneralPage::_ShowUnitDeviceControls(int nCmdShow)
{
	std::for_each(
		m_unitDeviceControls.begin(),
		m_unitDeviceControls.end(),
		ShowWindowFunctor(nCmdShow));
}

void
CNdasDevicePropGeneralPage::_InitData()
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

		ATLASSERT(0 == m_ThreadCount);
		ATLASSERT(WAIT_OBJECT_0 == WaitForSingleObject(m_ThreadCompleted, 0));

		ATLVERIFY(ResetEvent(m_ThreadCompleted));

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
CNdasDevicePropGeneralPage::_SetCurUnitDevice(int n)
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
		NDAS_UNITDEVICE_ERROR error = pUnitDevice->GetNdasUnitError();

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
CNdasDevicePropGeneralPage::_UpdateDeviceData()
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
		m_pDevice->GetNdasDeviceError());

	m_wndDeviceStatus.SetWindowText(strStatus);
}

void
CNdasDevicePropGeneralPage::_UpdateUnitDeviceData(
	ndas::UnitDevicePtr pUnitDevice)
{
	pUnitDevice->UpdateStatus();
	pUnitDevice->UpdateInfo();

	m_hCursor = AtlLoadSysCursor(IDC_APPSTARTING);
	SetCursor(m_hCursor);

	int index = pUnitDevice->GetUnitNo();

	UpdateThreadParam* param = new UpdateThreadParam();
	if (NULL == param)
	{
		return;
	}
	param->Instance = this;
	param->UnitIndex = index;

	m_ThreadCount++;

	BOOL success = QueueUserWorkItem(
		spUpdateThreadStart, 
		param, 
		WT_EXECUTEDEFAULT);

	if (!success)
	{
		delete param;

		ATLVERIFY(PostMessage(
			WM_THREADED_WORK_COMPLETED, 0, static_cast<LPARAM>(index)));

		return;
	}
}

void
CNdasDevicePropGeneralPage::_UpdateLogDeviceData(
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
		pLogDevice->GetNdasLogicalUnitError(), 
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

		NDAS_DEVICE_STAT ndasDeviceStat = {0};

		ndasDeviceStat.Size = sizeof(NDAS_DEVICE_STAT);

		if (NdasQueryDeviceStatsById(ui.DeviceId, &ndasDeviceStat) == FALSE) {

			ATLASSERT(FALSE);

			NdasUiDbgCall( 1, _T("NdasQueryDeviceInformationById failed, ui->DeviceId=%ls, error=0x%X\n"), 
						  ui.DeviceId, GetLastError() );
		}

		NdasUiDbgCall( 2, _T("ui.DeviceId = %s ndasDeviceStat.NumberOfUnitDevices = %d\n"), 
					   ui.DeviceId, ndasDeviceStat.NumberOfUnitDevices );

		if (ndasDeviceStat.NumberOfUnitDevices <= 1) {

			// 	"[%1!d!] %2!s!"
			
			strNode.FormatMessage( IDS_LOGICALDEVICE_ENTRY_0_FMT, 
								   ui.Index + 1, 
								   pMemberDevice->GetName() );
		
		} else {

			// 	"[%1!d!] %2!s!:%3!d! "

			strNode.FormatMessage( IDS_LOGICALDEVICE_ENTRY_FMT, 
								   ui.Index + 1, 
								   pMemberDevice->GetName(), 
								   ui.UnitNo+1 );
		}

		NdasUiDbgCall( 2, _T("strNode = %s\n"), strNode );

		memberItem.AddTail(strNode, 0);
	}

	memberItem.Expand();
	rootItem.Expand();

}

BOOL 
CNdasDevicePropGeneralPage::OnTranslateAccelerator(LPMSG lpMsg)
{
	int ret = ::TranslateAccelerator(m_hWnd, m_hAccel, lpMsg);
	return ret ? PSNRET_MESSAGEHANDLED : PSNRET_NOERROR;
}

void 
CNdasDevicePropGeneralPage::OnCmdRefresh(UINT, int, HWND)
{
	DWORD waitResult = WaitForSingleObject(m_ThreadCompleted, 0);

	if (WAIT_OBJECT_0 != waitResult)
	{
		return;
	}

	ATLASSERT(0 == m_ThreadCount);
	ATLVERIFY(ResetEvent(m_ThreadCompleted));

	m_wndUnitDeviceROHosts.SetWindowText(_T(""));
	m_wndUnitDeviceRWHosts.SetWindowText(_T(""));

	std::for_each(
		m_pUnitDevices.begin(),
		m_pUnitDevices.end(),
		UpdateUnitDeviceData(this));
}
