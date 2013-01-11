#include "stdafx.h"
#include "resource.h"
#include "ndasuser.h"

#pragma warning(disable: 4244)
#pragma warning(disable: 4312)
#include "atlctrlxp.h"
#pragma warning(default: 4312)
#pragma warning(default: 4244)

#include "ndascls.h"
#include "optionpsh.h"
#include "devpropsh.h"
#include "aboutdlg.h"

#include "mainframe.h"
#include "nmmenu.h"

#include "devregdlg.h"
#include "devregwiz.h"

VOID CALLBACK 
NdasEventProc(
	DWORD dwError, 
	PNDAS_EVENT_INFO pEventInfo, 
	LPVOID lpContext);

CMainFrame::
CMainFrame()
{
	_pDeviceColl = &m_deviceColl;
	_pLogDevColl = &m_logDevColl;
}

LRESULT 
CMainFrame::
OnCreate(LPCREATESTRUCT lParam)
{
	DWORD dwThreadId(0);

	m_hEventCallback = ::NdasRegisterEventCallback(
		NdasEventProc,
		m_hWnd);

	m_hTaskBarDefaultMenu.LoadMenu(IDR_TASKBAR);
	m_taskBarIcon.Install(m_hWnd, 1, IDR_TASKBAR);

	UpdateDeviceList();
	UpdateMenuItems();

	// Subclass to a flat-looking menu
/*
	XPSTYLE m_xpstyle = {0};
	m_xpstyle.clrFrame = ::GetSysColor(COLOR_WINDOWFRAME);
	m_xpstyle.clrGreyText = ::GetSysColor(COLOR_GRAYTEXT);
	m_xpstyle.clrMenuText = ::GetSysColor(COLOR_WINDOWTEXT);
	m_xpstyle.clrSelMenuText = ::GetSysColor(COLOR_MENUTEXT);
	m_xpstyle.clrButtonText = ::GetSysColor(COLOR_BTNTEXT);
	m_xpstyle.clrSelButtonText = ::GetSysColor(COLOR_MENUTEXT);

	CFlatMenuWindow* wnd = new CFlatMenuWindow(
		10, 
		//		m_rcButton.right - m_rcButton.left, 
		m_xpstyle.clrFrame, 
		m_xpstyle.clrBackground, 
		m_xpstyle.clrMenu);
	wnd->SubclassWindow(m_hWnd);
*/
	return 0;
}

LRESULT 
CMainFrame::
OnDestroy()
{
	::NdasUnregisterEventCallback(m_hEventCallback);

	PostQuitMessage(0);
	return 0;
}

LRESULT 
CMainFrame::
OnSize(UINT wParam, CSize size)
{
	ShowWindow(FALSE);
	return 0;
}

void 
CMainFrame::
OnFileExit(UINT /* wNotifyCode */, int /* wID */, HWND /* hWndCtl */)
{
	// PostMessage(WM_CLOSE);
	DestroyWindow();
}

void 
CMainFrame::
OnAppAbout(UINT /* wNotifyCode */, int /* wID */, HWND /* hWndCtl */)
{
	CAboutDialog dlg;
	m_wndActivePopup.Attach(dlg);
	dlg.DoModal();
	m_wndActivePopup.Detach();
}

void 
CMainFrame::
OnAppOptions(UINT /* wNotifyCode */, int /* wID */, HWND /* hWndCtl */)
{
	opt::COptionsPropSheet psh(IDS_OPTIONDLG_TITLE);
	m_wndActivePopup.Attach(psh);
	psh.DoModal();
	m_wndActivePopup.Detach();
}


void 
CMainFrame::
OnRegisterDevice(UINT /* wNotifyCode */, int /* wID */, HWND /* hWndCtl */)
{

	ndrwiz::CWizard regwiz;
	m_wndActivePopup.Attach(regwiz);
	regwiz.DoModal();
	m_wndActivePopup.Detach();

	CRegisterDeviceDialog dlg;
	INT_PTR nResult = dlg.DoModal(
		::GetActiveWindow(),
		(LPARAM)m_deviceColl.GetDeviceCount());

	if (nResult == IDC_REGISTER) {
		LPCTSTR lpszDeviceName = dlg.GetDeviceName();
		LPCTSTR lpszDeviceStringId = dlg.GetDeviceStringId();
		LPCTSTR lpszDeviceStringKey = dlg.GetDeviceStringKey();

		BOOL fSuccess = ::NdasRegisterDevice(
			lpszDeviceStringId, lpszDeviceStringKey, lpszDeviceName);

		if (!fSuccess) {
			WTL::CString strError;
			strError.Format(TEXT("Registration failure.\nError 0x%08X"), ::GetLastError());
			MessageBox(strError, TEXT("Error"), MB_OK);
		}

	}
}

void 
CMainFrame::
OnRefreshStatus(UINT /* wNotifyCode */, int /* wID */, HWND /* hWndCtl */)
{
	UpdateDeviceList();
	UpdateMenuItems();
}

BOOL 
CMainFrame::
UpdateDeviceList()
{
	BOOL fSuccess = m_deviceColl.Update();
	if (!fSuccess) {
		ShowErrorMessage();
	}

	fSuccess = m_logDevColl.Update();
	if (!fSuccess) {
		ShowErrorMessage();
	}

	return TRUE;
}

void 
CMainFrame::
UpdateMenuItems()
{
	BOOL fSuccess(FALSE);

	CMenuHandle hTaskBarDefaultRootMenu;
	hTaskBarDefaultRootMenu.LoadMenu(IDR_TASKBAR);

	CMenuHandle hTaskBarMenu = ::GetSubMenu(hTaskBarDefaultRootMenu,0);

	MENUINFO mi = {0};
	mi.cbSize = sizeof(MENUINFO);
	mi.fMask = MIM_STYLE;
	fSuccess = hTaskBarMenu.GetMenuInfo(&mi);
	ATLASSERT(fSuccess);

	mi.cbSize = sizeof(MENUINFO);
	mi.fMask = MIM_STYLE | MIM_APPLYTOSUBMENUS;
	mi.dwStyle |= MNS_NOTIFYBYPOS | MNS_CHECKORBMP;
	fSuccess = hTaskBarMenu.SetMenuInfo(&mi);
	ATLASSERT(fSuccess);

	if (m_deviceColl.GetDeviceCount() == 0) {
		CMenuItemInfo mii;
		WTL::CString strMenuText;
		strMenuText.LoadString(IDS_NO_DEVICE);
		mii.fMask = MIIM_ID | MIIM_STRING | MIIM_STATE;
		mii.dwTypeData = (LPTSTR)((LPCTSTR)strMenuText);
		// mii.dwTypeData = MAKEINTRESOURCE(IDS_NO_DEVICE);
		mii.wID = ID_NDAS_DEVICE_NONE;
		mii.fState = MFS_DISABLED | MFS_UNHILITE;

		fSuccess = hTaskBarMenu.InsertMenuItem(0, TRUE, &mii);
		ATLASSERT(fSuccess);
	} else {

		CNdasDeviceMenu ndm(m_hWnd);

		for (DWORD i = 0; i < m_deviceColl.GetDeviceCount(); ++i) {
			CMenuItemInfo mii;
			ndas::Device* pDevice = m_deviceColl.GetDevice(i);
			pDevice->UpdateStatus();
			pDevice->UpdateInfo();
			// m_deviceArray[i].UpdateStatus();
			// m_deviceArray[i].CreateMenuItem(i, mii);
			ndm.CreateDeviceMenuItem(pDevice, mii);
			fSuccess = hTaskBarMenu.InsertMenuItem(i, TRUE, &mii);
			ATLASSERT(fSuccess);
			pDevice->Release();
		}

	}

	CMenuHandle hTaskBarOldRootMenu = m_taskBarIcon.m_hMenu;
	m_taskBarIcon.m_hMenu = hTaskBarDefaultRootMenu;
	hTaskBarOldRootMenu.DestroyMenu();

}

void
CMainFrame::
OnMenuCommand(WPARAM wParam, HMENU hMenuHandle)
{
	if (! ::IsMenu(hMenuHandle)) {
		ATLTRACE(_T("Invalid menu handle: %p.\n"), hMenuHandle);
		return;
	}

	CMenuHandle hMenu(hMenuHandle);
	INT nPos = (INT) wParam;

	CMenuItemInfo mii;
	mii.fMask = MIIM_DATA | MIIM_ID;

	BOOL fSuccess = hMenu.GetMenuItemInfo(nPos, TRUE, &mii);
	if (!fSuccess) {
		ATLTRACE(_T("OnMenuCommand: GetMenuItemInfo failed.\n"));
		return;
	}

	if (ID_SHOW_DEVICE_PROPERTIES == mii.wID) {

		DWORD dwSlotNo = static_cast<DWORD>(LOWORD(mii.dwItemData));

		OnShowDeviceProperties(dwSlotNo);

	} else if (ID_ENABLE_DEVICE == mii.wID) {

		DWORD dwSlotNo = static_cast<DWORD>(LOWORD(mii.dwItemData));

		OnEnableDevice(dwSlotNo);

	} else if (ID_DISABLE_DEVICE == mii.wID) {
		
		DWORD dwSlotNo = static_cast<DWORD>(LOWORD(mii.dwItemData));

		OnDisableDevice(dwSlotNo);

	} else if (ID_UNREGISTER_DEVICE == mii.wID) {

		DWORD dwSlotNo = static_cast<DWORD>(LOWORD(mii.dwItemData));

		OnUnregisterDevice(dwSlotNo);

	} else if (ID_NDD_MOUNT_RW == mii.wID) {

		NDAS_LOGICALDEVICE_ID logDevId = 
			LOGDEV_ULONG_PTR_TO_LDID(mii.dwItemData);

		OnMountDeviceRW(logDevId);

	} else if (ID_NDD_MOUNT_RO == mii.wID) {

		NDAS_LOGICALDEVICE_ID logDevId = 
			LOGDEV_ULONG_PTR_TO_LDID(mii.dwItemData);

		OnMountDeviceRO(logDevId);

	} else if (ID_NDD_UNMOUNT == mii.wID) {

		NDAS_LOGICALDEVICE_ID logDevId = 
			LOGDEV_ULONG_PTR_TO_LDID(mii.dwItemData);

		OnUnmountDevice(logDevId);

	} else {

		//
		// Reroute the message
		//

		PostMessage(WM_COMMAND, mii.wID, NULL);

	}

// for Windows 98/Me
//	int nPos = HIWORD(wParam);
	

//	UINT wId = hMenu.GetMenuItemID(nPos);
//	ATLTRACE(_T("Menu ID = %d\n"));
//	PostMessage(WM_COMMAND, MAKEWORD(0, wId), NULL);
//	SetMsgHandled(TRUE);

}

void 
CMainFrame::
OnEnableDevice(DWORD dwSlotNo)
{
	ndas::Device* pDevice = m_deviceColl.FindDevice(dwSlotNo);
	if (NULL == pDevice) {
		ATLTRACE(_T("Invalid slot no: %d\n"), dwSlotNo);
		return;
	}

    BOOL fSuccess = pDevice->Enable(true);
	if (!fSuccess) {
		ShowErrorMessage();
	}

	pDevice->Release();
}

void 
CMainFrame::
OnDisableDevice(DWORD dwSlotNo)
{
	ndas::Device* pDevice = m_deviceColl.FindDevice(dwSlotNo);
	if (NULL == pDevice) {
		ATLTRACE(_T("Invalid slot no: %d\n"), dwSlotNo);
		return;
	}

	BOOL fSuccess = pDevice->Enable(false);
	if (!fSuccess) {
		ShowErrorMessage();
	}


	pDevice->Release();
}

void 
CMainFrame::
OnMountDeviceRO(const NDAS_LOGICALDEVICE_ID& logDevId)
{
	ndas::LogicalDevice* pLogDevice = _pLogDevColl->FindLogicalDevice(logDevId);
	if (NULL == pLogDevice) {
		ATLTRACE(_T("Invalid logical device id specified: %d,%d,%d\n"),
			logDevId.SlotNo, logDevId.TargetId, logDevId.LUN);
		return;
	}

	BOOL fSuccess = pLogDevice->PlugIn(FALSE);
	if (!fSuccess) {
		ShowErrorMessage();
	}

	pLogDevice->Release();
}

void 
CMainFrame::
OnMountDeviceRW(const NDAS_LOGICALDEVICE_ID& logDevId)
{
	ndas::LogicalDevice* pLogDevice = _pLogDevColl->FindLogicalDevice(logDevId);
	if (NULL == pLogDevice) {
		ATLTRACE(_T("Invalid logical device id specified: %d,%d,%d\n"),
			logDevId.SlotNo, logDevId.TargetId, logDevId.LUN);
		return;
	}

	BOOL fSuccess = pLogDevice->PlugIn(TRUE);
	if (!fSuccess) {
		ShowErrorMessage();
	}

	pLogDevice->Release();
}

void
CMainFrame::
OnUnmountDevice(const NDAS_LOGICALDEVICE_ID& logDevId)
{
	ndas::LogicalDevice* pLogDevice = _pLogDevColl->FindLogicalDevice(logDevId);
	if (NULL == pLogDevice) {
		ATLTRACE(_T("Invalid logical device id specified: %d,%d,%d\n"),
			logDevId.SlotNo, logDevId.TargetId, logDevId.LUN);
		return;
	}

	BOOL fSuccess = pLogDevice->Eject();
	if (!fSuccess) {
		ShowErrorMessage();
	}

	pLogDevice->Release();
}

void
CMainFrame::
OnUnregisterDevice(DWORD dwSlotNo)
{
	BOOL fSuccess = _pDeviceColl->Unregister(dwSlotNo);
	if (!fSuccess) {
		ShowErrorMessage();
	}
}

void
CMainFrame::
OnShowDeviceProperties(DWORD dwSlotNo)
{
	ndas::Device* pDevice = m_deviceColl.FindDevice(dwSlotNo);
	if (NULL == pDevice) {
		ATLTRACE(_T("Invalid slot no: %d\n"), dwSlotNo);
		return;
	}

	WTL::CString strTitle;
	strTitle.Format(IDS_DEVICE_PROP_TITLE, pDevice->GetName());
	devprop::CDevicePropSheet psh(ATL::_U_STRINGorID(strTitle), 0, m_hWnd);
	psh.SetDevice(pDevice);
	psh.DoModal();

	pDevice->Release();
}


VOID CALLBACK 
NdasEventProc(
	DWORD dwError, 
	PNDAS_EVENT_INFO pEventInfo, 
	LPVOID lpContext)
{
	if (NULL == pEventInfo) {
		ATLTRACE(_T("Event Error %d (0x%08X)\n"), dwError, dwError);
		return;
	}

	HWND hWnd = reinterpret_cast<HWND>(lpContext);

	WPARAM wParam(0);
	LPARAM lParam(0);

	switch (pEventInfo->EventType) {
	case NDAS_EVENT_TYPE_DEVICE_ENTRY_CHANGED:
		::PostMessage(hWnd, WM_APP_NDAS_DEVICE_ENTRY_CHANGED, wParam, lParam);
		break;
	case NDAS_EVENT_TYPE_DEVICE_STATUS_CHANGED:
		lParam = static_cast<LPARAM>(pEventInfo->DeviceInfo.SlotNo);
		::PostMessage(hWnd, WM_APP_NDAS_DEVICE_STATUS_CHANGED, wParam, lParam);
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_ENTRY_CHANGED:
		::PostMessage(hWnd, WM_APP_NDAS_LOGICALDEVICE_ENTRY_CHANGED, wParam, lParam);
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_STATUS_CHANGED:
		wParam = static_cast<WPARAM>(MAKEWPARAM(
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId.TargetId,
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId.LUN));
		lParam = static_cast<LPARAM>(
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId.SlotNo);
		::PostMessage(hWnd, WM_APP_NDAS_LOGICALDEVICE_STATUS_CHANGED, wParam, lParam);
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_RECONNECTING:
		wParam = static_cast<WPARAM>(MAKEWPARAM(
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId.TargetId,
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId.LUN));
		lParam = static_cast<LPARAM>(
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId.SlotNo);
		::PostMessage(hWnd, WM_APP_NDAS_LOGICALDEVICE_RECONNECTING, wParam, lParam);
		break;
	case NDAS_EVENT_TYPE_LOGICALDEVICE_DISCONNECTED:
		wParam = static_cast<WPARAM>(MAKEWPARAM(
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId.TargetId,
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId.LUN));
		lParam = static_cast<LPARAM>(
			pEventInfo->LogicalDeviceInfo.LogicalDeviceId.SlotNo);
		::PostMessage(hWnd, WM_APP_NDAS_LOGICALDEVICE_DISCONNECTED, wParam, lParam);
		break;
	case NDAS_EVENT_TYPE_TERMINATING:
		::PostMessage(hWnd, WM_APP_NDAS_SERVICE_TERMINATING, wParam, lParam);
		break;
	case NDAS_EVENT_TYPE_RETRYING_CONNECTION:
		ATLTRACE(_T("NDAS_EVENT_TYPE_RETRYING_CONNECTION\n"));
		break;
	case NDAS_EVENT_TYPE_CONNECTED:
		ATLTRACE(_T("NDAS_EVENT_TYPE_CONNECTED\n"));
		break;
	case NDAS_EVENT_TYPE_CONNECTION_FAILED:
		ATLTRACE(_T("NDAS_EVENT_TYPE_CONNECTION_FAILED\n"));
		break;
	default:
		;
	}
}

void
CMainFrame::
OnNdasDevEntryChanged()
{
	UpdateDeviceList();
	UpdateMenuItems();
}

void 
CMainFrame::
OnNdasDevStatusChanged(DWORD dwSlotNo)
{
	UpdateMenuItems();
}

void 
CMainFrame::
OnNdasLogDevEntryChanged()
{
	UpdateMenuItems();
}

void 
CMainFrame::
OnNdasLogDevStatusChanged(const NDAS_LOGICALDEVICE_ID& logDevId)
{
	UpdateMenuItems();
}

static void MakeDevNameList(
	WTL::CString& szNameList, 
	const NDAS_LOGICALDEVICE_ID& logDevId)
{
	ndas::LogicalDevice* pLogDev = _pLogDevColl->FindLogicalDevice(logDevId);

	if (NULL != pLogDev) {

		szNameList = _T("");

		for (DWORD i = 0; i < pLogDev->GetUnitDeviceInfoCount(); ++i) {

			ndas::LogicalDevice::UNITDEVICE_INFO ui = 
				pLogDev->GetUnitDeviceInfo(i);

			ndas::Device* pDevice = 
				_pDeviceColl->FindDevice(ui.DeviceId);

			if (NULL != pDevice) {
				WTL::CString szLine;
				szLine.Format(_T("- %s\n"), pDevice->GetName);
				szNameList += szLine;
				pDevice->Release();
			}
		}

		pLogDev->Release();
	}
}

void 
CMainFrame::
OnNdasLogDevDisconnected(const NDAS_LOGICALDEVICE_ID& logDevId)
{
	WTL::CString szDevices = _T("Unknown");

	MakeDevNameList(szDevices, logDevId);
	WTL::CString strInfoFmt, strInfo, strInfoTitle;
	strInfoFmt.LoadString(IDS_BT_DISCONNECTED_INFO);
	strInfo.LoadString(IDS_BT_DISCONNECTED_INFO_TITLE);
	strInfo.Format(strInfoFmt, szDevices);
	m_taskBarIcon.ShowBalloonToolTip(
		strInfo, strInfoTitle, 5000, NIIF_ERROR);

	UpdateMenuItems();
}

void 
CMainFrame::
OnNdasLogDevReconnecting(const NDAS_LOGICALDEVICE_ID& logDevId)
{
	WTL::CString szDevices = _T("Unknown");

	MakeDevNameList(szDevices, logDevId);
	WTL::CString strInfoFmt, strInfo, strInfoTitle;
	strInfoFmt.LoadString(IDS_BT_RECONNECTING_INFO);
	strInfo.LoadString(IDS_BT_RECONNECTING_INFO_TITLE);
	strInfo.Format(strInfoFmt, szDevices);

	m_taskBarIcon.ShowBalloonToolTip(
		strInfo, strInfoTitle, 5000, NIIF_WARNING);
}

void 
CMainFrame::
OnNdasLogDevReconnected(const NDAS_LOGICALDEVICE_ID& logDevId)
{
	WTL::CString szDevices = _T("Unknown");

	MakeDevNameList(szDevices, logDevId);
	WTL::CString strInfoFmt, strInfo, strInfoTitle;
	strInfoFmt.LoadString(IDS_BT_RECONNECTED_INFO);
	strInfo.LoadString(IDS_BT_RECONNECTED_INFO_TITLE);
	strInfo.Format(strInfoFmt, szDevices);

	m_taskBarIcon.ShowBalloonToolTip(
		strInfo, strInfoTitle, 5000, NIIF_INFO);
}
