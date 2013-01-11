#include "stdafx.h"
#include "ndasmgmt.h"
#include "resource.h"

#include "devpropsh.h"
#include "confirmdlg.h"
#include "propertylist.h"
#include "ndastypestr.h"

namespace devprop {

//////////////////////////////////////////////////////////////////////////
//
// Device Hardware Property Page
//
//////////////////////////////////////////////////////////////////////////

CHardwarePage::CHardwarePage() :
	m_pDevice(NULL)
{
}

LRESULT 
CHardwarePage::OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
	ATLASSERT(NULL != m_pDevice);

	if (NULL == m_pDevice) {
		return 0;
	}

	m_propList.SubclassWindow(GetDlgItem(IDC_PROPLIST));
	m_propList.SetExtendedListStyle(PLS_EX_CATEGORIZED);

	CString strType, strValue;

	//
	// Device Hardware Information
	//
	const NDAS_DEVICE_HW_INFORMATION* pDevHWInfo = m_pDevice->GetHwInfo();

	strValue.LoadString(IDS_DEVPROP_CATEGORY_HARDWARE);
	m_propList.AddItem(PropCreateCategory(strValue));

	if (NULL == pDevHWInfo) {
		m_propList.AddItem(PropCreateSimple(_T("Not available."), _T("")));
		return 0;
	}

	// Version
	strType.LoadString(IDS_DEVPROP_HW_VERSION);
	pHWVersionString(strValue, pDevHWInfo->dwHwVersion);
	m_propList.AddItem(PropCreateSimple(strType, strValue));

	// Max Request Blocks
	strType.LoadString(IDS_DEVPROP_HW_MAX_REQUEST_BLOCKS);
	strValue.Format(_T("%d"), pDevHWInfo->nMaxRequestBlocks);
	m_propList.AddItem(PropCreateSimple(strType, strValue));

	// Slots
	strType.LoadString(IDS_DEVPROP_HW_SLOT_COUNT);
	strValue.Format(_T("%d"), pDevHWInfo->nSlots);
	m_propList.AddItem(PropCreateSimple(strType, strValue));

	// Targets
	strType.LoadString(IDS_DEVPROP_HW_TARGET_COUNT);
	strValue.Format(_T("%d"), pDevHWInfo->nTargets);
	m_propList.AddItem(PropCreateSimple(strType, strValue));

	// Max Targets
	strType.LoadString(IDS_DEVPROP_HW_MAX_TARGET_COUNT);
	strValue.Format(_T("%d"), pDevHWInfo->nMaxTargets);
	m_propList.AddItem(PropCreateSimple(strType, strValue));

	// Max LUs
	strType.LoadString(IDS_DEVPROP_HW_MAX_LU_COUNT);
	strValue.Format(_T("%d"), pDevHWInfo->nMaxLUs);
	m_propList.AddItem(PropCreateSimple(strType, strValue));

	//
	// Unit Device Hardware Information
	//

	for (DWORD i = 0; i < m_pDevice->GetUnitDeviceCount(); ++i) {

		ndas::UnitDevice* pUnitDevice = m_pDevice->GetUnitDevice(i);

		const NDAS_UNITDEVICE_HW_INFORMATION* pHWI = pUnitDevice->GetHWInfo();

		strValue.FormatMessage(IDS_DEVPROP_UNITDEV_TITLE_FMT, i + 1);

		m_propList.AddItem(PropCreateCategory(strValue));

		if (NULL == pHWI) {
			m_propList.AddItem(PropCreateSimple(_T("Not available"), _T("")));
			continue;
		}

		// Media Type
		strType.LoadString(IDS_DEVPROP_UNITDEV_DEVICE_TYPE);
		pUnitDeviceMediaTypeString(strValue, pHWI->MediaType);
		m_propList.AddItem(PropCreateSimple(strType, strValue));

		// Transfer Mode
		strType.LoadString(IDS_DEVPROP_UNITDEV_TRANSFER_MODE);
		strValue = _T("");

		if (pHWI->bPIO) strValue += _T("PIO, ");
		if (pHWI->bDMA) strValue += _T("DMA, ");
		if (pHWI->bUDMA) strValue += _T("UDMA, ");
		if (strValue.GetLength() > 2) {
			// Trimming ", "
			strValue = strValue.Left(strValue.GetLength() - 2);
		}
		m_propList.AddItem(PropCreateSimple(strType, strValue));

		// LBA support?
		strType.LoadString(IDS_DEVPROP_UNITDEV_LBA_MODE);
		strValue = _T("");
		if (pHWI->bLBA) strValue += _T("LBA, ");
		if (pHWI->bLBA48) strValue += _T("LBA48, ");
		if (strValue.GetLength() > 2) {
			// Trimming ", "
			strValue = strValue.Left(strValue.GetLength() - 2);
		}
		m_propList.AddItem(PropCreateSimple(strType, strValue));

		// Model
		strType.LoadString(IDS_DEVPROP_UNITDEV_MODEL);
		strValue = pHWI->szModel;
		strValue.TrimRight(); 
		m_propList.AddItem(PropCreateSimple(strType, strValue));

		// FWRev
		strType.LoadString(IDS_DEVPROP_UNITDEV_FWREV);
		strValue = pHWI->szFwRev;
		strValue.TrimRight(); 
		m_propList.AddItem(PropCreateSimple(strType, strValue));

		// Serial No
		strType.LoadString(IDS_DEVPROP_UNITDEV_SERIALNO);
		strValue = pHWI->szSerialNo;
		strValue.TrimRight(); 
		m_propList.AddItem(PropCreateSimple(strType, strValue));

		pUnitDevice->Release();
	}

	return 0;
}

void 
CHardwarePage::SetDevice(ndas::Device* pDevice)
{
	m_pDevice = pDevice;
}

//////////////////////////////////////////////////////////////////////////
//
// Rename Device Dialog
// Invoked from General Page
//
//////////////////////////////////////////////////////////////////////////

CRenameDialog::CRenameDialog()
{}

LRESULT 
CRenameDialog::OnInitDialog(HWND hwndFocus, LPARAM lParam)
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
CRenameDialog::SetName(LPCTSTR szName)
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
CRenameDialog::GetName()
{
	return m_szName;
}

LRESULT 
CRenameDialog::OnClose(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CEdit wndName(GetDlgItem(IDC_DEVICE_NAME));
	_ASSERTE(NULL != wndName.m_hWnd);

	wndName.GetWindowText(m_szName, MAX_NAME_LEN + 1);

	if (IDOK == wID && 0 != ::lstrcmp(m_szOldName, m_szName))
	{
		// name is changed!
		// check duplicate
		ndas::Device* pExistingDevice = 
			_pDeviceColl->FindDeviceByName(m_szName);
		if (NULL != pExistingDevice)
		{
			pExistingDevice->Release();
			AtlMessageBox(
				m_hWnd, 
				IDS_ERROR_DUPLICATE_NAME, 
				IDS_MAIN_TITLE, 
				MB_OK | MB_ICONWARNING);

			m_wndName.SetFocus();
			m_wndName.SetSel(0, -1);
			return 0;
		}
	}

	EndDialog(wID);
	return 0;
}

LRESULT
CRenameDialog::OnDeviceNameChange(UINT uCode, int nCtrlID, HWND hwndCtrl)
{
	_ASSERTE(hwndCtrl == m_wndName.m_hWnd);

	INT iLen = m_wndName.GetWindowTextLength();
	if (iLen == 0) {
		m_wndOK.EnableWindow(FALSE);
	} else {
		m_wndOK.EnableWindow(TRUE);
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
//
// General Property Page
//
//////////////////////////////////////////////////////////////////////////

CGeneralPage::CGeneralPage() :
	m_pDevice(NULL)
{
}

LRESULT 
CGeneralPage::OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
	ATLASSERT(NULL != m_pDevice);

	m_edtDevName = GetDlgItem(IDC_DEVICE_NAME);
	m_edtDevId = GetDlgItem(IDC_DEVICE_ID);
	m_edtDevStatus = GetDlgItem(IDC_DEVICE_STATUS);
	m_edtDevWriteKey = GetDlgItem(IDC_DEVICE_WRITE_KEY);
	m_butAddRemoveDevWriteKey = GetDlgItem(IDC_ADD_WRITE_KEY);

	m_edtUnitDevStatus = GetDlgItem(IDC_UNITDEVICE_STATUS);
	m_edtUnitDevCapacity = GetDlgItem(IDC_UNITDEVICE_CAPACITY);
	m_edtUnitDevROHosts = GetDlgItem(IDC_UNITDEVICE_RO_HOSTS);
	m_edtUnitDevRWHosts = GetDlgItem(IDC_UNITDEVICE_RW_HOSTS);

	m_edtUnitDevType = GetDlgItem(IDC_UNITDEVICE_TYPE);

	m_wndUnitDevIcon = GetDlgItem(IDC_UNITDEVICE_TYPE_ICON);
	m_tvLogDev.Attach(GetDlgItem(IDC_LOGDEV_TREE));

	{
		// Temporary edit control to get an effective password character
		CEdit wndConceal;
		wndConceal.Create(m_hWnd, NULL, NULL, WS_CHILD | ES_PASSWORD);
		m_chConcealed = wndConceal.GetPasswordChar();
		wndConceal.DestroyWindow();
	}

	UpdateData();

	CancelToClose();

	return 0;
}

VOID
CGeneralPage::UpdateData()
{
	m_pDevice->UpdateStatus();
	m_pDevice->UpdateInfo();

	HCURSOR hWaitCursor = AtlLoadSysCursor(IDC_WAIT);
	HCURSOR hSavedCursor = SetCursor(hWaitCursor);

	m_edtDevName.SetWindowText(m_pDevice->GetName());
	CString strDevId = m_pDevice->GetStringId();

	CString strFmtDevId;
	m_edtDevId.SetWindowText(
		pCreateDelimitedDeviceId(
			strFmtDevId, 
			strDevId, 
			m_chConcealed));

	if (GENERIC_WRITE & m_pDevice->GetGrantedAccess()) {

		CString str;
		str.LoadString(IDS_WRITE_KEY_PRESENT);
		m_edtDevWriteKey.SetWindowText(str);

		CString strButton;
		strButton.LoadString(IDS_REMOVE_WRITE_KEY);
		m_butAddRemoveDevWriteKey.SetWindowText(strButton);

	} else {

		CString str;
		str.LoadString(IDS_WRITE_KEY_NONE);
		m_edtDevWriteKey.SetWindowText(str);

		CString strButton;
		strButton.LoadString(IDS_ADD_WRITE_KEY);
		m_butAddRemoveDevWriteKey.SetWindowText(strButton);
	}

	CString strStatus;

	pDeviceStatusString(strStatus, 
		m_pDevice->GetStatus(),
		m_pDevice->GetLastError());

	m_edtDevStatus.SetWindowText(strStatus);

	DWORD nUnitDevs = m_pDevice->GetUnitDeviceCount();
	if (0 == nUnitDevs) {

		CEdit wndEdit(GetDlgItem(IDC_UNITDEVICE_TYPE));
		CString str;
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
		if (NULL != pUnitDev)
		{
			UpdateUnitDeviceData(pUnitDev);
			GenerateLogDevTree(pUnitDev);
			pUnitDev->Release();
		}

		for (DWORD i = 1; i < nUnitDevs; ++i)
		{
			ndas::UnitDevice* pUnitDev = m_pDevice->GetUnitDevice(i);
			if (NULL != pUnitDev)
			{
				pUnitDev->UpdateStatus();
				pUnitDev->UpdateInfo();
				pUnitDev->UpdateHostStats();
				pUnitDev->Release();
			}
		}

	}

	SetCursor(hSavedCursor);
}

void
CGeneralPage::UpdateUnitDeviceData(ndas::UnitDevice* pUnitDevice)
{
	pUnitDevice->UpdateStatus();
	pUnitDevice->UpdateInfo();
	pUnitDevice->UpdateHostStats();

	const NDAS_UNITDEVICE_HW_INFORMATION* pHWI = pUnitDevice->GetHWInfo();

	CString str;

	str.Format(_T("%d"), pUnitDevice->GetROHostCount()); // hwi.nROHosts);
	m_edtUnitDevROHosts.SetWindowText(str);

	str.Format(_T("%d"), pUnitDevice->GetRWHostCount()); // hwi.nRWHosts);
	m_edtUnitDevRWHosts.SetWindowText(str);

	if (NULL != pHWI) {
		pCapacityString(str, pHWI->SectorCountLowPart, pHWI->SectorCountHighPart);
		m_edtUnitDevCapacity.SetWindowText(str);
	}

	NDAS_UNITDEVICE_TYPE type = pUnitDevice->GetType();
	NDAS_UNITDEVICE_SUBTYPE subType = pUnitDevice->GetSubType();
	
	CString strType;
	pUnitDeviceTypeString(strType, type, subType);
	m_edtUnitDevType.SetWindowText(strType);

	if (NDAS_UNITDEVICE_TYPE_CDROM == type) {
		m_hUnitDevIcon = LoadIcon(
			_Module.GetResourceInstance(), 
			MAKEINTRESOURCE(IDI_CD_DRIVE));
		m_wndUnitDevIcon.SetIcon(m_hUnitDevIcon);
	} else if (NDAS_UNITDEVICE_TYPE_DISK == type) {
		m_hUnitDevIcon = LoadIcon(
			_Module.GetResourceInstance(), 
			MAKEINTRESOURCE(IDI_DISK_DRIVE));
		m_wndUnitDevIcon.SetIcon(m_hUnitDevIcon);
	} else {
		// no icon will show
	}

	NDAS_UNITDEVICE_STATUS status = pUnitDevice->GetStatus();

	CString strStatus;
	pUnitDeviceStatusString(strStatus, status);
	m_edtUnitDevStatus.SetWindowText(strStatus);

}

void
CGeneralPage::GenerateLogDevTree(
	ndas::UnitDevice* pUnitDevice)
{
	CString strRootNode;

	NDAS_LOGICALDEVICE_ID logDevId = pUnitDevice->GetLogicalDeviceId();

	ndas::LogicalDevice* pLogDev = _pLogDevColl->FindLogicalDevice(logDevId);

	if (NULL == pLogDev) {

		strRootNode.LoadString(IDS_LOGDEV_INFO_UNAVAILABLE);
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

	NDAS_LOGICALDEVICE_TYPE logDevType = pLogDev->GetType();
	NDAS_LOGICALDEVICE_STATUS logDevStatus = pLogDev->GetStatus();

	CString strType, strLocation, strStatus;

	pLogicalDeviceStatusString(
		strStatus, 
		logDevStatus, 
		pLogDev->GetMountedAccess());

	pLogicalDeviceTypeString(strType, logDevType);

	//	Slot %1!d!, Target ID %2!d!, LUN %3!d!
	//strLocation.FormatMessage(
	//	IDS_LOGDEV_LOCATION_FMT,
	//	pLogDev->GetLogicalDeviceId());

	strRootNode.Format(_T("%s - %s"), strType, strStatus);

	m_tvLogDev.DeleteAllItems();

	CTreeItem tiRoot = m_tvLogDev.InsertItem(
		strRootNode, 
		TVI_ROOT, 
		TVI_LAST);

	CString strNodeText;
	CString strLocationCaption;
//	strLocationCaption.LoadString(IDS_LOGDEV_LOCATION);
//	strNodeText.Format(_T("%s %s"), strLocationCaption, strLocation);
	strNodeText.Format(_T("%s"), strLocation);
//	CTreeItem tiLocation = tiRoot.AddTail(strNodeText, 0);
//	tiLocation.Expand();

	CTreeItem tiMember = tiRoot; // .AddTail(_T("Members"), 0);

	for (DWORD i = 0; i < pLogDev->GetUnitDeviceInfoCount(); ++i) {

		CString strNode;
		ndas::LogicalDevice::UNITDEVICE_INFO ui = 
			pLogDev->GetUnitDeviceInfo(i);

		ndas::Device* pMemberDevice = 
			_pDeviceColl->FindDevice(ui.DeviceId);

		if (NULL == pMemberDevice) {
			// "[%1!d!] Missing Entry"
			strNode.FormatMessage(
				IDS_LOGICALDEVICE_ENTRY_MISSING_FMT, 
				ui.Index + 1);
			tiMember.AddTail(strNode, 0);
			continue;
		}

		ndas::UnitDevice* pMemberUnitDevice = 
			pMemberDevice->FindUnitDevice(ui.UnitNo);

		if (NULL == pMemberUnitDevice) {
			// "[%1!d!] Unavailable (%2!s!:%3!d!)"
			strNode.FormatMessage(IDS_LOGICALDEVICE_ENTRY_UNAVAILABLE_FMT, 
				ui.Index + 1,
				pMemberDevice->GetName(), 
				ui.UnitNo);
			tiMember.AddTail(strNode, 0);
			pMemberDevice->Release();
			continue;
		}

		// 	"[%1!d!] %2!s!:%3!d! "
		strNode.FormatMessage(IDS_LOGICALDEVICE_ENTRY_FMT, 
			ui.Index + 1, 
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
CGeneralPage::SetDevice(ndas::Device* pDevice)
{
	m_pDevice = pDevice;
}

void 
CGeneralPage::OnRename(UINT, int, HWND)
{
	CRenameDialog wndRename;
	wndRename.SetName(m_pDevice->GetName());
	INT_PTR iResult = wndRename.DoModal();

	if (IDOK == iResult) {
		CString strNewName = wndRename.GetName();
		if (0 != strNewName.Compare(m_pDevice->GetName())) {

			BOOL fSuccess = m_pDevice->SetName(strNewName);
			if (!fSuccess) {
				ShowErrorMessageBox(IDS_ERROR_RENAME_DEVICE);
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

		INT_PTR iResult = pShowMessageBox(
			IDS_REMOVE_WRITE_KEY_CONFIRM,
			IDS_REMOVE_WRITE_KEY_CONFIRM_TITLE,
			m_hWnd,
			_T("DontConfirmRemoveWriteKey"),
			IDNO,
			IDYES);

		if (IDYES == iResult) {
			BOOL fSuccess = m_pDevice->SetAsReadOnly();
			if (!fSuccess) {
				ShowErrorMessageBox(IDS_ERROR_REMOVE_WRITE_KEY);
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
				ShowErrorMessageBox(IDS_ERROR_ADD_WRITE_KEY);
			} else {
				UpdateData();
			}
		} else {
			;
		}
	}
}

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
	m_pDevice(NULL),
	m_pspHostStats(NULL)
{
	m_psh.dwFlags |= PSH_NOAPPLYNOW | PSH_USEPAGELANG;

	AddPage(m_pspGeneral);
	
}

CDevicePropSheet::~CDevicePropSheet()
{
	if (NULL != m_pDevice) {
		m_pDevice->Release();
	}
	if (NULL != m_pspHostStats) {
		delete [] m_pspHostStats;
	}
}

void 
CDevicePropSheet::SetDevice(ndas::Device* pDevice)
{
	m_pDevice = pDevice;
	m_pDevice->AddRef();
	m_pspGeneral.SetDevice(pDevice);

	//
	// Hardware Page shows only if the device is connected
	//
	if (NDAS_DEVICE_STATUS_CONNECTED == pDevice->GetStatus()) {
		AddPage(m_pspHardware);
		m_pspHardware.SetDevice(pDevice);
	}

	if (pDevice->GetUnitDeviceCount() > 0) {
		DWORD nPages = pDevice->GetUnitDeviceCount();
		m_pspHostStats = new CHostStatPage[nPages];
		for (DWORD i = 0; i < nPages; ++i) {
			ndas::UnitDevice* pUnitDevice = pDevice->GetUnitDevice(i);
			m_pspHostStats[i].SetUnitDevice(pUnitDevice);
			AddPage(m_pspHostStats[i]);
		}
	}

	m_pspAdvanced.SetDevice(pDevice);
	AddPage(m_pspAdvanced);
}

void 
CDevicePropSheet::OnShowWindow(BOOL bShow, UINT nStatus)
{
	ATLASSERT(NULL != m_pDevice); 
	if (bShow && !m_bCentered) {
		// Center Windows only once!
		m_bCentered = TRUE;
		CenterWindow();
	}

	SetMsgHandled(FALSE);
}

//////////////////////////////////////////////////////////////////////////
//
// Add Write Key Dialog
// Invoked from General Page
//
//////////////////////////////////////////////////////////////////////////

LRESULT 
CAddWriteKeyDlg::OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
	CWindow wndDeviceName, wndDeviceId;
	
	wndDeviceName.Attach(GetDlgItem(IDC_DEVICE_NAME));
	wndDeviceName.SetWindowText(m_strDeviceName);

	CString strFmtDeviceId;
	wndDeviceId.Attach(GetDlgItem(IDC_DEVICE_ID));
	wndDeviceId.SetWindowText(
		pCreateDelimitedDeviceId(strFmtDeviceId, m_strDeviceId, _T('*')));

	CEdit wndWriteKey(GetDlgItem(IDC_DEVICE_WRITE_KEY));
	wndWriteKey.SetLimitText(NDAS_DEVICE_WRITE_KEY_LEN);
	m_butOK.Attach(GetDlgItem(IDOK));
	m_butOK.EnableWindow(FALSE);

	return TRUE;
}

void
CAddWriteKeyDlg::SetDeviceName(LPCTSTR szDeviceName)
{
	m_strDeviceName = szDeviceName;
}

void
CAddWriteKeyDlg::SetDeviceId(LPCTSTR szDeviceId)
{
	m_strDeviceId = szDeviceId;
}

LPCTSTR
CAddWriteKeyDlg::GetWriteKey()
{
	return m_strWriteKey;
}

void 
CAddWriteKeyDlg::OnOK(UINT /*wNotifyCode*/, int /* wID */, HWND /*hWndCtl*/)
{
	EndDialog(IDOK);
}

void 
CAddWriteKeyDlg::OnCancel(UINT /*wNotifyCode*/, int /* wID */, HWND /*hWndCtl*/)
{
	EndDialog(IDCANCEL);
}

void
CAddWriteKeyDlg::OnWriteKeyChange(UINT /*wNotifyCode*/, int /*wID*/, HWND hWndCtl)
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

//////////////////////////////////////////////////////////////////////////
//
// Host Statistics Page
//
//////////////////////////////////////////////////////////////////////////

CHostStatPage::CHostStatPage() :
	m_pUnitDevice(NULL)
{
}

CHostStatPage::CHostStatPage(ndas::UnitDevice* pUnitDevice) :
	m_pUnitDevice(pUnitDevice)
{
	m_pUnitDevice->AddRef();
}

VOID
CHostStatPage::SetUnitDevice(ndas::UnitDevice* pUnitDevice)
{
	ATLASSERT(NULL != pUnitDevice);

	pUnitDevice->AddRef();
	if (NULL != m_pUnitDevice) {
		m_pUnitDevice->Release();
	}
	m_pUnitDevice = pUnitDevice;
}

CHostStatPage::~CHostStatPage()
{
	if (NULL != m_pUnitDevice) {
		m_pUnitDevice->Release();
	}
}

LRESULT
CHostStatPage::OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
	CString strBuffer;

	m_wndListView.Attach(GetDlgItem(IDC_HOST_LIST));
	strBuffer.LoadString(IDS_HOSTSTAT_ACCESS);
	m_wndListView.AddColumn(strBuffer, 0);
	strBuffer.LoadString(IDS_HOSTSTAT_HOSTNAME);
	m_wndListView.AddColumn(strBuffer, 1);
	strBuffer.LoadString(IDS_HOSTSTAT_NETWORK_ADDRESS);
	m_wndListView.AddColumn(strBuffer, 2);
	m_wndListView.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);

	BOOL fSuccess = m_pUnitDevice->UpdateHostInfo();
	if (!fSuccess) {
		m_wndListView.AddItem(0,0,_T("Unavailable"));
	}

	DWORD nHostInfo = m_pUnitDevice->GetHostInfoCount();
	for (DWORD i = 0; i < nHostInfo; ++i) {
		ACCESS_MASK access = 0;
		CONST NDAS_HOST_INFO* pHostInfo = m_pUnitDevice->GetHostInfo(i,&access);
		ATLASSERT(NULL != pHostInfo); // index is correct, then it must succeeded

		(VOID) pAddressString(
			strBuffer,
			&pHostInfo->LPXAddrs,
			&pHostInfo->IPV4Addrs);

		m_wndListView.AddItem(i, 0, (access & GENERIC_WRITE) ? _T("RW") : _T("RO"));
		m_wndListView.SetItemText(i, 1, pHostInfo->szHostname);
		m_wndListView.SetItemText(i, 2, strBuffer);
	}

	CRect rcListView;
	m_wndListView.GetClientRect(rcListView);
//	m_wndListView.SetColumnWidth(0, 55);
//	m_wndListView.SetColumnWidth(1, 110);
	m_wndListView.SetColumnWidth(0, LVSCW_AUTOSIZE_USEHEADER);
	m_wndListView.SetColumnWidth(1, LVSCW_AUTOSIZE_USEHEADER);
	m_wndListView.SetColumnWidth(2, LVSCW_AUTOSIZE_USEHEADER);
//	m_wndListView.SetColumnWidth(1, 
//		rcListView.Width() - wndListView.GetColumnWidth(0));


	return 1;
}

//////////////////////////////////////////////////////////////////////////
//
// Advanced Page
//
//////////////////////////////////////////////////////////////////////////

CAdvancedPage::CAdvancedPage() :
	m_pDevice(NULL)
{
}

CAdvancedPage::CAdvancedPage(ndas::Device* pDevice) :
	m_pDevice(pDevice)
{
	m_pDevice->AddRef();
}

CAdvancedPage::~CAdvancedPage()
{
	if (NULL != m_pDevice) {
		m_pDevice->Release();
	}
}

VOID
CAdvancedPage::SetDevice(ndas::Device* pDevice)
{
	ATLASSERT(NULL != pDevice);

	pDevice->AddRef();
	if (NULL != m_pDevice) {
		m_pDevice->Release();
	}
	m_pDevice = pDevice;
}

LRESULT 
CAdvancedPage::OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
	m_wndDeactivate.Attach(GetDlgItem(IDC_DEACTIVATE_DEVICE));
	m_wndReset.Attach(GetDlgItem(IDC_RESET_DEVICE));

	ATLASSERT(NULL != m_pDevice);

	if (NDAS_DEVICE_STATUS_DISABLED == m_pDevice->GetStatus() ||
		m_pDevice->IsAnyUnitDeviceMounted()) 
	{
		m_wndDeactivate.EnableWindow(FALSE);
		m_wndReset.EnableWindow(FALSE);
	}

	// Set initial focus
	return 1;
}

VOID
CAdvancedPage::OnDeactivateDevice(UINT uCode, int nCtrlID, HWND hwndCtrl)
{
	ATLASSERT(NULL != m_pDevice);

	CString strMessage, strTitle;
	BOOL fSuccess = strMessage.LoadString(IDS_CONFIRM_DEACTIVATE_DEVICE);
	ATLASSERT(fSuccess);

	strTitle.LoadString(IDS_MAIN_TITLE);
	ATLASSERT(fSuccess);

	INT_PTR iResponse = MessageBox(
		strMessage, 
		strTitle, 
		MB_YESNO | MB_ICONQUESTION);

	if (IDYES != iResponse) {
		return;
	}

	fSuccess = m_pDevice->Enable(FALSE);
	if (!fSuccess) {
		ShowErrorMessageBox(IDS_ERROR_DISABLE_DEVICE);
	}

	m_wndDeactivate.EnableWindow(FALSE);
	m_wndReset.EnableWindow(FALSE);
}

VOID
CAdvancedPage::OnResetDevice(UINT uCode, int nCtrlID, HWND hwndCtrl)
{
	ATLASSERT(NULL != m_pDevice);

	CString strMessage, strTitle;
	BOOL fSuccess = strMessage.LoadString(IDS_CONFIRM_RESET_DEVICE);
	ATLASSERT(fSuccess);

	strTitle.LoadString(IDS_MAIN_TITLE);
	ATLASSERT(fSuccess);

	INT_PTR iResponse = MessageBox(
		strMessage, 
		strTitle, 
		MB_YESNO | MB_ICONQUESTION);

	if (IDYES != iResponse) {
		return;
	}

	fSuccess = m_pDevice->Enable(FALSE);
	if (!fSuccess) {
		ShowErrorMessageBox(IDS_ERROR_RESET_DEVICE);
		return;
	}

	::Sleep(2000);

	fSuccess = m_pDevice->Enable(TRUE);
	if (!fSuccess) {
		ShowErrorMessageBox(IDS_ERROR_RESET_DEVICE);
	}

	::PostMessage(GetParent(), IDCLOSE, 0, 0);
}

//////////////////////////////////////////////////////////////////////////
//
// Utility functions
//
//////////////////////////////////////////////////////////////////////////

} // namespace
