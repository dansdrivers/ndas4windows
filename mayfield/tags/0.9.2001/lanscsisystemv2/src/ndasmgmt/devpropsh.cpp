#include "stdafx.h"
#include "ndasmgmt.h"
#include "resource.h"
#include "devpropsh.h"

namespace devprop {

//
// Local functions
//
static void CapacityString(
	WTL::CString& str, 
	DWORD lowPart, 
	DWORD highPart);

CHardwarePage::
CHardwarePage() :
	m_pDevice(NULL)
{
}

LRESULT 
CHardwarePage::
OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
	ATLASSERT(NULL != m_pDevice);

	CListViewCtrl wndListView(GetDlgItem(IDC_HWINFO_LIST));

	wndListView.AddColumn(TEXT("Property"), 0);
	wndListView.AddColumn(TEXT("Value"), 1);
	wndListView.SetColumnWidth(0, 200);
	wndListView.SetColumnWidth(1, 100);

	const NDAS_DEVICE_HW_INFORMATION hwinfo = 
		m_pDevice->GetHwInfo();

	wndListView.AddItem(0, 0, TEXT("Hardware Type"));
	wndListView.SetItemText(0, 1, TEXT("NDAS Device"));

	wndListView.AddItem(1, 0, TEXT("Hardware Version"));
	WTL::CString str;
	switch (hwinfo.dwHwVersion) {
			case 0:
				wndListView.SetItemText(1, 1, _T("1.0"));
				break;
			case 1:
				wndListView.SetItemText(1, 1, _T("1.1"));
				break;
			case 2:
				wndListView.SetItemText(1, 1, _T("2.0"));
				break;
			default:
				str.Format(_T("Unrecognized version (%d)"), hwinfo.dwHwVersion);
				wndListView.SetItemText(1, 1, str);
				break;
	}

	DWORD i = 2;
	wndListView.AddItem(i, 0, _T("Max Request Blocks"));
	str.Format(_T("%d"), hwinfo.nMaxRequestBlocks);
	wndListView.SetItemText(i, 1, str);
	++i;

	wndListView.AddItem(i, 0, _T("Maximum Targets"));
	str.Format(_T("%d"), hwinfo.nMaxTargets);
	wndListView.SetItemText(i, 1, str);
	++i;

	wndListView.AddItem(i, 0, _T("Maximum LUs"));
	str.Format(_T("%d"), hwinfo.nMaxLUs);
	wndListView.SetItemText(i, 1, str);
	++i;

	wndListView.AddItem(i, 0, _T("Number of Targets"));
	str.Format(_T("%d"), hwinfo.nTargets);
	wndListView.SetItemText(i, 1, str);
	++i;

	wndListView.AddItem(i, 0, _T("Number of Slots"));
	str.Format(_T("%d"), hwinfo.nSlots);
	wndListView.SetItemText(i, 1, str);
	++i;

//		wndListView.AddItem(0, 0, TEXT("Unavailable"));

	return 0;
}

void 
CHardwarePage::
SetDevice(ndas::Device* pDevice)
{
	m_pDevice = pDevice;
}


CRenameDialog::
CRenameDialog()
{}

LRESULT 
CRenameDialog::
OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
	CEdit wndDeviceName(GetDlgItem(IDC_DEVICE_NAME));
	ATLASSERT(NULL != wndDeviceName.m_hWnd);
	wndDeviceName.SetLimitText(MAX_NAME_LEN);
	wndDeviceName.SetWindowText(m_szName);
	wndDeviceName.SetFocus();
	wndDeviceName.SetSelAll();
	return 0;
}

void 
CRenameDialog::
SetName(LPCTSTR szName)
{
	HRESULT hr = ::StringCchCopyN(
		m_szName, 
		MAX_NAME_LEN + 1, 
		szName, 
		MAX_NAME_LEN);
	ATLASSERT(SUCCEEDED(hr));
}

LPCTSTR 
CRenameDialog::
GetName()
{
	return m_szName;
}

LRESULT 
CRenameDialog::
OnClose(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CEdit wndName(GetDlgItem(IDC_DEVICE_NAME));
	_ASSERTE(NULL != wndName.m_hWnd);

	wndName.GetWindowText(m_szName, MAX_NAME_LEN + 1);
	EndDialog(wID);
	return 0;
}

CGeneralPage::
CGeneralPage() :
m_pDevice(NULL)
{
}

WTL::CString MakeDelimitedDeviceId(const WTL::CString& strDevId)
{
	WTL::CString strDelimitedDevId;
	strDelimitedDevId.Format(
		_T("%s-%s-%s-%s"), 
		strDevId.Mid(0, 5),
		strDevId.Mid(5, 5),
		strDevId.Mid(10, 5),
		strDevId.Mid(15, 5));

	return strDelimitedDevId;
}

LRESULT 
CGeneralPage::
OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
	ATLASSERT(NULL != m_pDevice);

	m_edtDevName = GetDlgItem(DEVPROP_IDC_DEVICE_NAME);
	m_edtDevId = GetDlgItem(DEVPROP_IDC_DEVICE_ID);
	m_edtDevStatus = GetDlgItem(DEVPROP_IDC_DEVICE_STATUS);
	m_edtDevWriteKey = GetDlgItem(DEVPROP_IDC_DEVICE_WRITE_KEY);
	m_butAddRemoveDevWriteKey = GetDlgItem(DEVPROP_IDC_ADD_WRITE_KEY);

	m_edtUnitDevStatus = GetDlgItem(IDC_UNITDEVICE_STATUS);
	m_edtUnitDevCapacity = GetDlgItem(IDC_UNITDEVICE_CAPACITY);
	//m_edtUnitDevModel = GetDlgItem(IDC_UNITDEVICE_MODEL);
	//m_edtUnitDevFWRev = GetDlgItem(IDC_UNITDEVICE_FWREV);
	//m_edtUnitDevSerialNo = GetDlgItem(IDC_UNITDEVICE_SERIALNO);
	m_edtUnitDevROHosts = GetDlgItem(IDC_UNITDEVICE_RO_HOSTS);
	m_edtUnitDevRWHosts = GetDlgItem(IDC_UNITDEVICE_RW_HOSTS);

	m_edtUnitDevType = GetDlgItem(IDC_UNITDEVICE_TYPE);

	m_tvLogDev.Attach(GetDlgItem(IDC_LOGDEV_TREE));

	UpdateData();

	return 0;
}

void
CGeneralPage::
UpdateData()
{
	HCURSOR hWaitCursor = AtlLoadSysCursor(IDC_WAIT);
	HCURSOR hSavedCursor = SetCursor(hWaitCursor);

	m_edtDevName.SetWindowText(m_pDevice->GetName());
	WTL::CString strDevId = m_pDevice->GetStringId();

	m_edtDevId.SetWindowText(MakeDelimitedDeviceId(strDevId));

	if (GENERIC_WRITE & m_pDevice->GetGrantedAccess()) {

		WTL::CString str;
		str.LoadString(DEVPROP_IDS_WRITE_KEY_PRESENT);
		m_edtDevWriteKey.SetWindowText(str);

		WTL::CString strButton;
		strButton.LoadString(DEVPROP_IDS_REMOVE_WRITE_KEY);
		m_butAddRemoveDevWriteKey.SetWindowText(strButton);

	} else {

		WTL::CString str;
		str.LoadString(DEVPROP_IDS_WRITE_KEY_NONE);
		m_edtDevWriteKey.SetWindowText(str);

		WTL::CString strButton;
		strButton.LoadString(DEVPROP_IDS_ADD_WRITE_KEY);
		m_butAddRemoveDevWriteKey.SetWindowText(strButton);
	}

	WTL::CString strStatus;
	switch (m_pDevice->GetStatus()) {
		case NDAS_DEVICE_STATUS_CONNECTED:
			strStatus.LoadString(IDS_NDAS_DEVICE_STATUS_CONNECTED);
			break;
		case NDAS_DEVICE_STATUS_DISCONNECTED:
			strStatus.LoadString(IDS_NDAS_DEVICE_STATUS_DISCONNECTED);
			break;
		case NDAS_DEVICE_STATUS_DISABLED:
			strStatus.LoadString(IDS_NDAS_DEVICE_STATUS_DISABLED);
			break;
		case NDAS_DEVICE_STATUS_UNKNOWN:
		default:
			strStatus.LoadString(IDS_NDAS_DEVICE_STATUS_UNKNOWN);
			break;
	}

	m_edtDevStatus.SetWindowText(strStatus);

	DWORD nUnitDevs = m_pDevice->GetUnitDeviceCount();
	if (0 == nUnitDevs) {
		CEdit wndEdit(GetDlgItem(IDC_UNITDEVICE_TYPE));
		WTL::CString str;
		str.LoadString(IDS_UNITDEVICE_NONE);
		wndEdit.SetWindowText(str);
	} else if (1 == nUnitDevs) {

		ndas::UnitDevice* pUnitDev = m_pDevice->GetUnitDevice(0);

		UpdateUnitDeviceData(pUnitDev);

		GenerateLogDevTree(pUnitDev);

		pUnitDev->Release();

	} else {

		//
		// TODO: Handle multiple unit devices
		//

		ndas::UnitDevice* pUnitDev = m_pDevice->GetUnitDevice(0);

		UpdateUnitDeviceData(pUnitDev);

		GenerateLogDevTree(pUnitDev);

		pUnitDev->Release();

	}

	SetCursor(hSavedCursor);
}

void
CGeneralPage::
UpdateUnitDeviceData(ndas::UnitDevice* pUnitDevice)
{
	NDAS_UNITDEVICE_HW_INFORMATION hwi = pUnitDevice->GetHWInfo();
	NDAS_UNITDEVICE_USAGE_STATS stats = pUnitDevice->GetUsage();

	WTL::CString str;
	str.Format(_T("%d"), stats.nROHosts); // hwi.nROHosts);
	m_edtUnitDevROHosts.SetWindowText(str);

	str.Format(_T("%d"), stats.nRWHosts); // hwi.nRWHosts);
	m_edtUnitDevRWHosts.SetWindowText(str);

	CapacityString(str, hwi.SectorCountLowPart, hwi.SectorCountHighPart);
	m_edtUnitDevCapacity.SetWindowText(str);

	//str = hwi.szFwRev;
	//str.TrimRight();
	//m_edtUnitDevFWRev.SetWindowText(str);

	//str = hwi.szModel;
	//str.TrimRight();
	//m_edtUnitDevModel.SetWindowText(str);

	//str = hwi.szSerialNo;
	//str.TrimRight();
	//m_edtUnitDevSerialNo.SetWindowText(str);

	NDAS_UNITDEVICE_TYPE type = pUnitDevice->GetType();
	NDAS_UNITDEVICE_SUBTYPE subType = pUnitDevice->GetSubType();
	
	WTL::CString strType;
	switch (type) {
	case NDAS_UNITDEVICE_TYPE_DISK:
		{
			strType = _T("Disk Drive");
			switch (subType.DiskDeviceType) {
			case NDAS_UNITDEVICE_DISK_TYPE_SINGLE:
				break;
			case NDAS_UNITDEVICE_DISK_TYPE_AGGREGATED:
				strType += _T(" (Aggregated)");
				break;
			case NDAS_UNITDEVICE_DISK_TYPE_MIRROR_MASTER:
				strType += _T(" (Mirrored Master)");
				break;
			case NDAS_UNITDEVICE_DISK_TYPE_MIRROR_SLAVE:
				strType += _T(" (Mirrored Slave)");
				break;
			case NDAS_UNITDEVICE_DISK_TYPE_UNKNOWN:
			default:
				{
					WTL::CString strTemp;
					strTemp.Format(_T(" (Unknown Type: 0x%04X)"), subType.DiskDeviceType);
					strType += strTemp;
				}
				break;
			}
		}
		break;
	case NDAS_UNITDEVICE_TYPE_CDROM:
		{
			switch (subType.CDROMDeviceType) {
			case NDAS_UNITDEVICE_CDROM_TYPE_CD:
				strType = _T("CD Drive");
				break;
			case NDAS_UNITDEVICE_CDROM_TYPE_DVD:
				strType = _T("DVD Drive");
				break;
			default:
				strType.Format(_T("Unknown CDROM Type: 0x%04X"), subType.CDROMDeviceType);
			}
		}
		break;
	default:
		{
			str.LoadString(IDS_UNKNOWN_UNITDEVICE_TYPE);
			strType.Format(_T("%s (%d)"), str, type);
			m_edtUnitDevType.SetWindowText(strType);
		}
	}
	m_edtUnitDevType.SetWindowText(strType);

	NDAS_UNITDEVICE_STATUS status = pUnitDevice->GetStatus();

	WTL::CString strStatus;
	switch (status) {
	case NDAS_UNITDEVICE_STATUS_MOUNTED:
		strStatus.LoadString(IDS_UNITDEVICE_STATUS_MOUNTED);
		break;
	case NDAS_UNITDEVICE_STATUS_NOT_MOUNTED:
		strStatus.LoadString(IDS_UNITDEVICE_STATUS_NOT_MOUNTED);
		break;
	case NDAS_UNITDEVICE_STATUS_UNKNOWN:
	default:
		{
			str.LoadString(IDS_UNITDEVICE_STATUS_UNKNOWN);
			strStatus.Format(_T("%s (0x%04X)"), str, status);
		}
	}

	m_edtUnitDevStatus.SetWindowText(strStatus);

}

void
CGeneralPage::GenerateLogDevTree(ndas::UnitDevice* pUnitDevice)
{
	WTL::CString strRootNode;

	NDAS_LOGICALDEVICE_ID logDevId = pUnitDevice->GetLogicalDeviceId();

	ndas::LogicalDevice* pLogDev = _pLogDevColl->FindLogicalDevice(logDevId);

	if (NULL == pLogDev) {

		strRootNode = _T("Unavailable");
		CTreeItem tiRoot = m_tvLogDev.InsertItem(
			strRootNode, 
			TVI_ROOT, 
			TVI_LAST);

		return;
	}

	//
	// TODO: To handle errors
	//
	(VOID) pLogDev->UpdateInfo();
	(VOID) pLogDev->UpdateStatus();

	DWORD dwSlotNo = pLogDev->GetSlotNo();
	DWORD dwTargetId = pLogDev->GetTargetId();
	DWORD dwLUN = pLogDev->GetLUN();

	NDAS_LOGICALDEVICE_TYPE logDevType = pLogDev->GetType();
	NDAS_LOGICALDEVICE_STATUS logDevStatus = pLogDev->GetStatus();

	WTL::CString strType, strLocation, strStatus;

	switch(logDevStatus)
	{
	case NDAS_LOGICALDEVICE_STATUS_UNMOUNTED:
		strStatus = _T("Unmounted");
		break;
	case NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING:
		strStatus = _T("Mount Pending");
		break;
	case NDAS_LOGICALDEVICE_STATUS_MOUNTED:
		strStatus =  (GENERIC_WRITE & pLogDev->GetMountedAccess()) ?
			_T("Mounted as Read/Write") : _T("Mounted as Read-only");
		break;
	case NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING:
		strStatus = _T("Unmount Pending");
		break;
	case NDAS_LOGICALDEVICE_STATUS_NOT_MOUNTABLE:
		strStatus = _T("Not Mountable");
		break;
	default:
		strStatus.Format(_T("Unknown Status (%08X)"), logDevStatus);
	}

	switch (logDevType) {
	case NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE:
		strType = _T("Disk (Single)");
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED:
		strType = _T("Aggregated Disk");
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_MIRRORED:
		strType = _T("Mirrored Disk");
		break;
	case NDAS_LOGICALDEVICE_TYPE_DISK_RAID_1:
		strType = _T("Disk (RAID 1)");
		break;
	case NDAS_LOGICALDEVICE_TYPE_DVD:
		strType = _T("DVD Drive");
		break;
	case NDAS_LOGICALDEVICE_TYPE_VIRTUAL_DVD:
		strType = _T("DVD Drive (Virtual)");
		break;
	case NDAS_LOGICALDEVICE_TYPE_MO:
		strType = _T("MO Drive");
		break;
	case NDAS_LOGICALDEVICE_TYPE_FLASHCARD:
		strType = _T("Flash Card Reader");
		break;
	default:
		strType.Format(_T("Unknown Type (%08X)"), logDevType);
	}

	strLocation.Format(_T("Slot %d, Target ID %d, LUN %d"),
		dwSlotNo, dwTargetId, dwLUN);

	strRootNode.Format(_T("%s - %s"), strType, strStatus);

	CTreeItem tiRoot = m_tvLogDev.InsertItem(
		strRootNode, 
		TVI_ROOT, 
		TVI_LAST);

	WTL::CString strNodeText;
	strNodeText.Format(_T("Location: %s"), strLocation);
	CTreeItem tiLocation = tiRoot.AddTail(strNodeText, 0);
	tiLocation.Expand();

	CTreeItem tiMember = tiRoot.AddTail(_T("Members"), 0);

	for (DWORD i = 0; i < pLogDev->GetUnitDeviceInfoCount(); ++i) {

		WTL::CString strNode;
		ndas::LogicalDevice::UNITDEVICE_INFO ui = 
			pLogDev->GetUnitDeviceInfo(i);

		ndas::Device* pMemberDevice = _pDeviceColl->FindDevice(ui.DeviceId);
		if (NULL == pMemberDevice) {
			strNode.Format(_T("[%d] Missing Entry (%s:%d)"), 
				ui.Index,
				ui.DeviceId, 
				ui.UnitNo);
			tiMember.AddTail(strNode, 0);
			continue;
		}

		ndas::UnitDevice* pMemberUnitDevice = 
			pMemberDevice->FindUnitDevice(ui.UnitNo);

		if (NULL == pMemberUnitDevice) {
			strNode.Format(_T("[%d] Unavailable (%s:%d)"), 
				ui.Index,
				pMemberDevice->GetName(), 
				ui.UnitNo);
			tiMember.AddTail(strNode, 0);
			pMemberDevice->Release();
			continue;
		}

		strNode.Format(_T("[%d] %s:%d "), 
			ui.Index, 
			pMemberDevice->GetName(), 
			ui.UnitNo);

		tiMember.AddTail(strNode, 0);
		pMemberUnitDevice->Release();
		pMemberDevice->Release();
	}

	tiMember.Expand();
	tiRoot.Expand();

	pLogDev->Release();
}

void 
CGeneralPage::
SetDevice(ndas::Device* pDevice)
{
	m_pDevice = pDevice;
}

void 
CGeneralPage::
OnRename(UINT, int, HWND)
{
	CRenameDialog wndRename;
	wndRename.SetName(m_pDevice->GetName());
	INT_PTR iResult = wndRename.DoModal();

	if (IDOK == iResult) {
		WTL::CString strNewName = wndRename.GetName();
		if (0 != strNewName.Compare(m_pDevice->GetName())) {

			BOOL fSuccess = m_pDevice->SetName(strNewName);
			if (!fSuccess) {
				ShowErrorMessage();
			} else {
				m_edtDevName.SetWindowText(strNewName);
				UpdateData();
			}
		}
	}
}


void CGeneralPage::OnModifyWriteKey(UINT, int, HWND)
{
	ACCESS_MASK access = m_pDevice->GetGrantedAccess();
	if (access & GENERIC_WRITE) {
		INT_PTR iResult = MessageBox(_T("Are you sure you want to remove the Write Key?\n")
			_T("It will make you impossible to mount this device as Read/Write"),
			_T("Remove Write Key"),
			MB_ICONEXCLAMATION | MB_YESNO);
		if (iResult == IDYES) {
			BOOL fSuccess = m_pDevice->SetAsReadOnly();
			if (!fSuccess) {
				ShowErrorMessage();
			} else {
				UpdateData();
			}
		}
	} else {
		CAddWriteKeyDlg dlg;
		dlg.SetDeviceId(m_pDevice->GetStringId());
		dlg.SetDeviceName(m_pDevice->GetName());
		INT_PTR iResult = dlg.DoModal();
		ATLTRACE(_T("iResult = %d\n"), iResult);
		if (iResult == IDOK) {
			BOOL fSuccess = m_pDevice->SetAsReadWrite(dlg.GetWriteKey());
			if (!fSuccess) {
				ShowErrorMessage();
			} else {
				UpdateData();
			}
		} else {
			;
		}
	}
}

CDevicePropSheet::
CDevicePropSheet(
	_U_STRINGorID title,
	UINT uStartPage,
	HWND hWndParent) : 
	CPropertySheetImpl<CDevicePropSheet>(title, uStartPage, hWndParent),
	m_bCentered(FALSE),
	m_pDevice(NULL)
{
	m_psh.dwFlags |= PSH_NOAPPLYNOW;

	AddPage(m_pspGeneral);
	AddPage(m_pspHardware);

}

void 
CDevicePropSheet::
SetDevice(ndas::Device* pDevice)
{
	m_pDevice = pDevice;
	m_pspGeneral.SetDevice(pDevice);
	m_pspHardware.SetDevice(pDevice);
}

void 
CDevicePropSheet::
OnShowWindow(BOOL bShow, UINT nStatus)
{
	ATLASSERT(NULL != m_pDevice); 
	if (bShow && !m_bCentered) {
		// Center Windows only once!
		m_bCentered = TRUE;
		CenterWindow();
	}

	SetMsgHandled(FALSE);
}

LRESULT 
CAddWriteKeyDlg::
OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
	CWindow edtDeviceName, edtDeviceId;
	
	edtDeviceName.Attach(GetDlgItem(IDC_DEVICE_NAME));
	edtDeviceName.SetWindowText(m_strDeviceName);
	
	edtDeviceId.Attach(GetDlgItem(IDC_DEVICE_ID));
	edtDeviceId.SetWindowText(MakeDelimitedDeviceId(m_strDeviceId));

	CEdit editWriteKey(GetDlgItem(IDC_DEVICE_WRITE_KEY));
	editWriteKey.SetLimitText(NDAS_DEVICE_WRITE_KEY_LEN);
	m_butOK.Attach(GetDlgItem(IDOK));
	m_butOK.EnableWindow(FALSE);

	return TRUE;
}

void
CAddWriteKeyDlg::
SetDeviceName(LPCTSTR szDeviceName)
{
	m_strDeviceName = szDeviceName;
}

void
CAddWriteKeyDlg::
SetDeviceId(LPCTSTR szDeviceId)
{
	m_strDeviceId = szDeviceId;
}

LPCTSTR
CAddWriteKeyDlg::
GetWriteKey()
{
	return m_strWriteKey;
}

void 
CAddWriteKeyDlg::
OnOK(UINT /*wNotifyCode*/, int /* wID */, HWND /*hWndCtl*/)
{
	EndDialog(IDOK);
}

void 
CAddWriteKeyDlg::
OnCancel(UINT /*wNotifyCode*/, int /* wID */, HWND /*hWndCtl*/)
{
	EndDialog(IDCANCEL);
}

void
CAddWriteKeyDlg::
OnWriteKeyChange(UINT /*wNotifyCode*/, int /*wID*/, HWND hWndCtl)
{
	CEdit editWriteKey(hWndCtl);
	TCHAR szWriteKey[NDAS_DEVICE_WRITE_KEY_LEN + 1] = {0};

	editWriteKey.GetWindowText(
		szWriteKey,
		NDAS_DEVICE_WRITE_KEY_LEN + 1);

	m_strWriteKey = szWriteKey;

	CButton butOk(GetDlgItem(IDOK));
	if (m_strWriteKey.GetLength() == 5) {
		BOOL fSuccess = ::NdasValidateStringIdKey(
			m_strDeviceId, 
			m_strWriteKey);
		if (!fSuccess) {
			butOk.EnableWindow(FALSE);
		} else {
			butOk.EnableWindow(TRUE);
		}
	} else {
		butOk.EnableWindow(FALSE);
	}

	SetMsgHandled(FALSE);
}

static void CapacityString(WTL::CString& str, DWORD lowPart, DWORD highPart)
{
	//
	// Sector Size = 512 Bytes = 0x200 Bytes
	//
	// 1024 = 0x400
	//
	// sectors = high * 0x1,0000,00000 + low
	// bytes = high * 0x1,0000,0000 + low * 0x0200
	//       = high * 0x200,0000,0000 + low * 0x200
	// kilobytes = high * 0x8000,0000 + low / 0x2
	// megabytes = high * 0x20,0000 + low / 0x800
	// gigabytes = high * 0x800 + low / 0x20,0000
	// terabytes = high * 0x2 + low / 0x8000,0000

	DWORD dwMB = highPart * 0x200000 + lowPart / 0x0002;

	if (dwMB > 1026) {
		DWORD dwGB = highPart * 0x800 + lowPart / 0x200000;
		DWORD dwFrac = (dwMB % 1024) * 10 / 1024;
		str.Format(_T("%d.%01d GB"), dwGB, dwFrac);
	} else {
		str.Format(_T("%d MB"), dwMB);
	}
}

} // namespace
