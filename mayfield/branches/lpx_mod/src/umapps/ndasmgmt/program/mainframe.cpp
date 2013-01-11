#include "stdafx.h"
#include "resource.h"
#include "ndasuser.h"
#include <cfgmgr32.h> // CMP_WaitNoPendingInstallEvents
#include <shlwapi.h>

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
#include "appconf.h"
#include "confirmdlg.h"
#include "syshelp.h"
#include "waitdlg.h"
#include "autores.h"

BOOL 
pCheckNoPendingInstall(LPVOID lpContext);

static 
CString& 
pCreateLogicalDeviceString(
	CString& szNameList, 
	NDAS_LOGICALDEVICE_ID logDevId);

static
BOOL 
pRequestSurrenderAccess(
	HWND hWnd, 
	ndas::LogicalDevice* pLogDevice);

class CFootprintWindow :
	public CWindowImpl<
		CFootprintWindow, 
		CWindow, 
		CWinTraits<WS_EX_TOOLWINDOW> >
{
public:
	BEGIN_MSG_MAP(CFootprintWindow)
	END_MSG_MAP()

	CFootprintWindow(HWND hParent = NULL)
	{
		CRect rect(0,0,0,0);
		CString strTitle;
		BOOL fSuccess = strTitle.LoadString(IDS_MAIN_TITLE);
		ATLASSERT(fSuccess);
		HWND hWnd = Create(hParent, rect, strTitle);
		if (NULL == hWnd) {
			ATLTRACE(_T("Failed to create Footprint Window"));
			return;
		}
		UpdateWindow();		
	}

	// called when the window is destroyed
	virtual void OnFinalMessage(HWND /* hWnd */)
	{
		delete this;
	}
};

CMainFrame::CMainFrame() :
	m_fPendingUpdateDeviceList(TRUE),
	m_fPendingUpdateMenuItem(TRUE)
{
	_pDeviceColl = &m_deviceColl;
	_pLogDevColl = &m_logDevColl;
}

LRESULT 
CMainFrame::OnCreate(LPCREATESTRUCT lParam)
{
	DWORD dwThreadId(0);

	::InitializeCriticalSection(&m_csMenu);

	//
	// Subscribe NDAS Event
	//
	BOOL fSuccess = NdasEventSubscribe(m_hWnd);
	ATLASSERT(fSuccess);

	m_hTaskBarDefaultMenu.LoadMenu(IDR_TASKBAR);
	CTaskBarIconExT<CMainFrame>::Install(m_hWnd, 1, IDR_TASKBAR);

	UpdateDeviceList();
	UpdateMenuItems();

	CFootprintWindow* fp = new CFootprintWindow(m_hWnd);

	return 0;
}

LRESULT 
CMainFrame::OnDestroy()
{
	//
	// Unsubscribe NDAS Event
	//
	BOOL fSuccess = NdasEventUnsubscribe();
	ATLASSERT(fSuccess);

	::DeleteCriticalSection(&m_csMenu);
	PostQuitMessage(0);
	return 0;
}

LRESULT 
CMainFrame::OnSize(UINT wParam, CSize size)
{
	// ShowWindow(FALSE);
	SetMsgHandled(FALSE);
	return 0;
}

VOID 
CMainFrame::OnFileExit(UINT /* wNotifyCode */, int /* wID */, HWND /* hWndCtl */)
{
	// PostMessage(WM_CLOSE);
	CString strText, strCaption;

	INT_PTR iResult = pShowMessageBox(
		IDS_CONFIRM_EXIT,
		IDS_MAIN_TITLE,
		m_hWnd,
		_T("DontConfirmExit"),
		IDNO,
		IDYES);

	if (IDYES == iResult) {
		DestroyWindow();
	}
}

VOID 
CMainFrame::ShowWelcome()
{
	ShowBalloonToolTip(
		IDS_NDASMGMT_WELCOME_TOOLTIP,
		IDS_NDASMGMT_WELCOME_TITLE,
		30 * 1000,
		NIIF_INFO);
}

VOID 
CMainFrame::ShowInstPopup()
{
	ShowBalloonToolTip(
		IDS_NDASMGMT_ALREADY_RUNNING_TOOLTIP,
		IDS_NDASMGMT_ALREADY_RUNNING_TITLE,
		5000,
		NIIF_INFO);
}

VOID 
CMainFrame::OnAppAbout(UINT /* wNotifyCode */, int /* wID */, HWND /* hWndCtl */)
{
	CAboutDialog dlg;
	m_wndActivePopup.Attach(dlg);
	dlg.DoModal();
	m_wndActivePopup.Detach();
}

VOID 
CMainFrame::OnAppOptions(UINT /* wNotifyCode */, int /* wID */, HWND /* hWndCtl */)
{
	CString strTitle;

	CAppOptPropSheet psh;
	strTitle.LoadString(IDS_OPTIONDLG_TITLE);
	psh.SetTitle(strTitle);
	m_wndActivePopup.Attach(psh);
	psh.DoModal();
	m_wndActivePopup.Detach();
}


VOID 
CMainFrame::OnRegisterDevice(UINT /* wNotifyCode */, int /* wID */, HWND /* hWndCtl */)
{

	BOOL fSuccess;
	BOOL fUseWizard;

	//
	// default: use wizard
	//

	fSuccess = pGetAppConfigValue(_T("UseRegWizard"),&fUseWizard);
	if (fSuccess && !fUseWizard) {

		CRegisterDeviceDlg dlg;
		m_wndActivePopup.Attach(dlg);
		(VOID) dlg.DoModal();
		m_wndActivePopup.Detach();

	} else {

		ndrwiz::CWizard regwiz;
		m_wndActivePopup.Attach(regwiz);
		(VOID) regwiz.DoModal();
		m_wndActivePopup.Detach();

	}

}

VOID 
CMainFrame::OnRefreshStatus(UINT /* wNotifyCode */, int /* wID */, HWND /* hWndCtl */)
{
	BOOL fSuccess;
	fSuccess = UpdateDeviceList();
	if (!fSuccess) {
		ShowErrorMessageBox(IDS_ERROR_UPDATE_DEVICE_LIST);
	}
	UpdateMenuItems();
}

BOOL 
CMainFrame::UpdateDeviceList()
{
	BOOL fSuccess = m_deviceColl.Update();
	BOOL fLogDevSuccess = m_logDevColl.Update();

	if (!(fSuccess && fLogDevSuccess)) {
		return FALSE;
	}

	return TRUE;
}

VOID 
CMainFrame::UpdateMenuItems()
{
	::EnterCriticalSection(&m_csMenu);

	BOOL fSuccess(FALSE);

	BOOL fLogDevSuccess = m_logDevColl.Update();

	CMenuHandle hTaskBarDefaultRootMenu;
	hTaskBarDefaultRootMenu.LoadMenu(IDR_TASKBAR);

	BOOL fShowUnmountAll = FALSE;
	fSuccess = pGetAppConfigValue(_T("ShowUnmountAll"), &fShowUnmountAll);
	if (fSuccess && fShowUnmountAll) {
		// do not remove menu
	} else {
		// otherwise remove menu
		hTaskBarDefaultRootMenu.RemoveMenu(IDR_UNMOUNT_ALL, MF_BYCOMMAND);
	}

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
		CString strMenuText;
		strMenuText.LoadString(IDS_NO_DEVICE);
		mii.fMask = MIIM_ID | MIIM_STRING | MIIM_STATE;
		mii.dwTypeData = (LPTSTR)((LPCTSTR)strMenuText);
		// mii.dwTypeData = MAKEINTRESOURCE(IDS_NO_DEVICE);
		mii.wID = IDR_NDAS_DEVICE_NONE;
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
			ndm.CreateDeviceMenuItem(pDevice, mii);
			fSuccess = hTaskBarMenu.InsertMenuItem(i, TRUE, &mii);
			ATLASSERT(fSuccess);
			pDevice->Release();
		}

	}

	CMenuHandle hTaskBarOldRootMenu = CTaskBarIconT<CMainFrame>::m_hMenu;
	CTaskBarIconT<CMainFrame>::m_hMenu = hTaskBarDefaultRootMenu;
	hTaskBarOldRootMenu.DestroyMenu();

	::LeaveCriticalSection(&m_csMenu);
}

VOID
CMainFrame::OnMenuCommand(WPARAM wParam, HMENU hMenuHandle)
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

	if (IDR_SHOW_DEVICE_PROPERTIES == mii.wID) {

		DWORD dwSlotNo = static_cast<DWORD>(LOWORD(mii.dwItemData));

		OnShowDeviceProperties(dwSlotNo);

	} else if (IDR_ENABLE_DEVICE == mii.wID) {

		DWORD dwSlotNo = static_cast<DWORD>(LOWORD(mii.dwItemData));

		OnEnableDevice(dwSlotNo);

	} else if (IDR_DISABLE_DEVICE == mii.wID) {
		
		DWORD dwSlotNo = static_cast<DWORD>(LOWORD(mii.dwItemData));

		OnDisableDevice(dwSlotNo);

	} else if (IDR_UNREGISTER_DEVICE == mii.wID) {

		DWORD dwSlotNo = static_cast<DWORD>(LOWORD(mii.dwItemData));

		OnUnregisterDevice(dwSlotNo);

	} else if (IDR_NDD_MOUNT_RW == mii.wID) {

		NDAS_LOGICALDEVICE_ID logDevId = mii.dwItemData;

		OnMountDeviceRW(logDevId);

	} else if (IDR_NDD_MOUNT_RO == mii.wID) {

		NDAS_LOGICALDEVICE_ID logDevId = mii.dwItemData;

		OnMountDeviceRO(logDevId);

	} else if (IDR_NDD_UNMOUNT == mii.wID) {

		NDAS_LOGICALDEVICE_ID logDevId = mii.dwItemData;

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

VOID 
CMainFrame::OnEnableDevice(DWORD dwSlotNo)
{
	ndas::Device* pDevice = m_deviceColl.FindDevice(dwSlotNo);
	if (NULL == pDevice) {
		ATLTRACE(_T("Invalid slot no: %d\n"), dwSlotNo);
		return;
	}

    BOOL fSuccess = pDevice->Enable(true);
	if (!fSuccess) {
		ShowErrorMessageBox(IDS_ERROR_ENABLE_DEVICE);
	}

	pDevice->Release();
}

VOID 
CMainFrame::OnDisableDevice(DWORD dwSlotNo)
{
	ndas::Device* pDevice = m_deviceColl.FindDevice(dwSlotNo);
	if (NULL == pDevice) {
		ATLTRACE(_T("Invalid slot no: %d\n"), dwSlotNo);
		return;
	}

	BOOL fSuccess = pDevice->Enable(false);
	if (!fSuccess) {
		ShowErrorMessageBox(IDS_ERROR_DISABLE_DEVICE);
	}


	pDevice->Release();
}

VOID 
CMainFrame::OnMountDeviceRO(NDAS_LOGICALDEVICE_ID logDevId)
{
	ndas::LogicalDevice* pLogDevice = _pLogDevColl->FindLogicalDevice(logDevId);
	if (NULL == pLogDevice) {
		ATLTRACE(_T("Invalid logical device id specified: %d\n"), logDevId);
		return;
	}

	BOOL fSuccess = pLogDevice->PlugIn(FALSE);
	if (!fSuccess) {
		ShowErrorMessageBox(IDS_ERROR_MOUNT_DEVICE_RO);
	}

	pLogDevice->Release();

	if (fSuccess) {
		if (!pCheckNoPendingInstall(NULL)) {
 			CWaitDialog(
				IDS_WAIT_MOUNT, 
				IDS_MAIN_TITLE, 
				1000, 
				pCheckNoPendingInstall, 
				NULL).DoModal();
		}
	}	
}

VOID
CMainFrame::OnMountDeviceRW(NDAS_LOGICALDEVICE_ID logDevId)
{
	ndas::LogicalDevice* pLogDevice = _pLogDevColl->FindLogicalDevice(logDevId);
	if (NULL == pLogDevice) {
		ATLTRACE(_T("Invalid logical device id specified: %d\n"), logDevId);
		return;
	}

	BOOL fSuccess = pLogDevice->PlugIn(TRUE);
	if (!fSuccess && 
		NDASHLPSVC_ERROR_MULTIPLE_WRITE_ACCESS_NOT_ALLOWED == ::GetLastError())
	{
		CString strMsg, strTitle;
		strTitle.LoadString(IDS_MAIN_TITLE);
		strMsg.LoadString(IDS_REQUEST_SURRENDER_RW_ACCESS);
		INT_PTR iResult = MessageBox(strMsg,strTitle,MB_YESNO | MB_ICONEXCLAMATION);
		if (IDYES == iResult) {
			pRequestSurrenderAccess(m_hWnd,pLogDevice);
		}
	} else if (!fSuccess) {
		ShowErrorMessageBox(IDS_ERROR_MOUNT_DEVICE_RW);
	}

	pLogDevice->Release();

	if (fSuccess) {
		if (!pCheckNoPendingInstall(NULL)) {
 			CWaitDialog(
				IDS_WAIT_MOUNT, 
				IDS_MAIN_TITLE, 
				1000, 
				pCheckNoPendingInstall, 
				NULL).DoModal();
		}
	}
}

VOID
CMainFrame::OnUnmountDevice(NDAS_LOGICALDEVICE_ID logDevId)
{
	ndas::LogicalDevice* pLogDevice = _pLogDevColl->FindLogicalDevice(logDevId);
	if (NULL == pLogDevice) {
		ATLTRACE(_T("Invalid logical device id specified: %d\n"), logDevId);
		return;
	}

	INT_PTR iResult = pShowMessageBox(
		IDS_CONFIRM_UNMOUNT,
		IDS_MAIN_TITLE,
		::GetActiveWindow(),
		_T("DontConfirmUnmount"),
		IDNO,
		IDYES);

	if (IDYES != iResult) {
		return;
	}


	BOOL fSuccess = pLogDevice->Eject();
	if (!fSuccess) {
		ShowErrorMessageBox(IDS_ERROR_UNMOUNT_DEVICE);
	}

	pLogDevice->Release();

	if (fSuccess) {
		if (!pCheckNoPendingInstall(NULL)) {
	 		CWaitDialog(
				IDS_WAIT_UNMOUNT, 
				IDS_MAIN_TITLE, 
				1000, 
				pCheckNoPendingInstall, 
				NULL).DoModal();
		}
	}
}

VOID 
CMainFrame::OnUnmountAll(UINT wNotifyCode, int wID, HWND hWndCtl)
{
	INT_PTR iResult = pShowMessageBox(
		IDS_CONFIRM_UNMOUNT_ALL,
		IDS_MAIN_TITLE,
		::GetActiveWindow(),
		_T("DontConfirmUnmountAll"),
		IDNO,
		IDYES);

	if (IDYES != iResult) {
		return;
	}

	DWORD nLogDevices = _pLogDevColl->GetLogicalDeviceCount();
	for (DWORD i = 0; i < nLogDevices; ++i) 
	{
		ndas::LogicalDevice* pLogDevice = _pLogDevColl->GetLogicalDevice(i);
		ATLASSERT(NULL != pLogDevice);

		if (NULL == pLogDevice) 
		{
			continue;
		}

		if (NDAS_LOGICALDEVICE_STATUS_MOUNTED == pLogDevice->GetStatus())
		{
			pLogDevice->Eject();
		}

		pLogDevice->Release();
	}
}

VOID
CMainFrame::OnUnregisterDevice(DWORD dwSlotNo)
{
	ndas::Device* pDevice = _pDeviceColl->FindDevice(dwSlotNo);
	if (NULL == pDevice) {
		return;
	}

	CString strMessage, strTitle;
	
	BOOL fSuccess = strMessage.FormatMessage(
		IDS_CONFIRM_UNREGISTER_FMT,
		pDevice->GetName());

	strTitle.LoadString(IDS_MAIN_TITLE);

	INT_PTR iResult = pShowMessageBox(
		strMessage,
		strTitle,
		::GetActiveWindow(),
		_T("DontConfirmUnregister"),
		IDNO,
		IDYES);

	if (IDYES != iResult) {
		return;
	}

	if (NDAS_DEVICE_STATUS_DISABLED != pDevice->GetStatus()) {
		fSuccess = pDevice->Enable(FALSE);
		if (!fSuccess) {
			ShowErrorMessageBox(IDS_ERROR_UNREGISTER_DEVICE);
			return;
		}
		pDevice->Release();
	}

	fSuccess = _pDeviceColl->Unregister(dwSlotNo);
	if (!fSuccess) {
		ShowErrorMessageBox(IDS_ERROR_UNREGISTER_DEVICE);
	} else {
	}
}

VOID
CMainFrame::OnShowDeviceProperties(DWORD dwSlotNo)
{
	ndas::Device* pDevice = m_deviceColl.FindDevice(dwSlotNo);
	if (NULL == pDevice) {
		ATLTRACE(_T("Invalid slot no: %d\n"), dwSlotNo);
		return;
	}

	CString strTitle;
	strTitle.FormatMessage(IDS_DEVICE_PROP_TITLE, pDevice->GetName());
	devprop::CDevicePropSheet psh(ATL::_U_STRINGorID(strTitle), 0, m_hWnd);
	psh.SetDevice(pDevice);
	psh.DoModal();

	pDevice->Release();
}

VOID
CMainFrame::OnNdasDevEntryChanged()
{
	ATLTRACE(_T("Device Entry changed.\n"));

	m_fPendingUpdateDeviceList = TRUE;
	m_fPendingUpdateMenuItem = TRUE;
}

VOID 
CMainFrame::OnNdasDevPropChanged(DWORD dwSlotNo)
{
	ATLTRACE(_T("Device(%d) property changed.\n"), dwSlotNo);

	m_fPendingUpdateMenuItem = TRUE;
}

VOID 
CMainFrame::OnNdasUnitDevPropChanged(
	DWORD dwSlotNo, 
	DWORD dwUnitNo)
{
	ATLTRACE(_T("Unit Device(%d,%d) property changed.\n"), dwSlotNo, dwUnitNo);

	m_fPendingUpdateMenuItem = TRUE;
}

VOID 
CMainFrame::OnNdasDevStatusChanged(
	DWORD dwSlotNo,
	NDAS_DEVICE_STATUS oldStatus,
	NDAS_DEVICE_STATUS newStatus)
{
	ATLTRACE(_T("Device(%d) status changed: %08X->%08X.\n"), 
		dwSlotNo, oldStatus, newStatus);

	m_fPendingUpdateMenuItem = TRUE;
}

VOID 
CMainFrame::OnNdasLogDevEntryChanged()
{
	ATLTRACE(_T("Logical Device Entry changed.\n"));
	m_fPendingUpdateMenuItem = TRUE;
}

VOID 
CMainFrame::OnNdasLogDevStatusChanged(
	NDAS_LOGICALDEVICE_ID logDevId,
	NDAS_LOGICALDEVICE_STATUS oldStatus,
	NDAS_LOGICALDEVICE_STATUS newStatus)
{
	ATLTRACE(_T("Logical Device(%d) Status changed: %08X->%08X.\n"),
		logDevId, oldStatus, newStatus);
	m_fPendingUpdateMenuItem = TRUE;
}

VOID 
CMainFrame::OnNdasLogDevPropertyChanged(
	NDAS_LOGICALDEVICE_ID logDevId)
{
	ATLTRACE(_T("Logical Device(%d) Property changed.\n"), logDevId);
	m_fPendingUpdateMenuItem = TRUE;
}

VOID 
CMainFrame::OnNdasLogDevRelationChanged(
	NDAS_LOGICALDEVICE_ID logDevId)
{
	ATLTRACE(_T("Logical Device(%d) Relation changed.\n"), logDevId);
	m_fPendingUpdateMenuItem = TRUE;
}

VOID 
CMainFrame::OnNdasLogDevDisconnected(
	NDAS_LOGICALDEVICE_ID logDevId)
{
	ATLTRACE(_T("Logical Device(%d) disconnected.\n"), logDevId);
	UpdateMenuItems();

	CString szDevices = _T("Unknown");

	BOOL fUseDisconnectAlert = TRUE;
	BOOL fSuccess = pGetAppConfigValue(
		_T("UseDisconnectAlert"), 
		&fUseDisconnectAlert);

	if (fSuccess && !fUseDisconnectAlert) {
		return;
	}

	pCreateLogicalDeviceString(szDevices, logDevId);
	CString strInfoFmt, strInfo, strInfoTitle;
	strInfoFmt.LoadString(IDS_BT_DISCONNECTED_INFO);
	strInfoTitle.LoadString(IDS_BT_DISCONNECTED_INFO_TITLE);
	strInfo.Format(strInfoFmt, szDevices);

	ShowBalloonToolTip(strInfo, strInfoTitle, 5000, NIIF_ERROR);
}

VOID 
CMainFrame::OnNdasLogDevEmergency(
									 NDAS_LOGICALDEVICE_ID logDevId)
{
	ATLTRACE(_T("Logical Device(%d) is being reconnected.\n"), logDevId);

	UpdateMenuItems();

	CString szDevices = _T("Unknown");

	BOOL fUseEmergencyAlert = TRUE;
	BOOL fSuccess = pGetAppConfigValue(
		_T("UseEmergencyAlert"), 
		&fUseEmergencyAlert);

	if (fSuccess && !fUseEmergencyAlert) {
		return;
	}

	pCreateLogicalDeviceString(szDevices, logDevId);
	CString strInfoFmt, strInfo, strInfoTitle;
	strInfoFmt.LoadString(IDS_BT_EMERGENCY_INFO);
	strInfoTitle.LoadString(IDS_BT_EMERGENCY_INFO_TITLE);
	strInfo.Format(strInfoFmt, szDevices);

	ShowBalloonToolTip(strInfo, strInfoTitle, 5000, NIIF_WARNING);
}

VOID 
CMainFrame::OnNdasLogDevReconnecting(
									 NDAS_LOGICALDEVICE_ID logDevId)
{
	ATLTRACE(_T("Logical Device(%d) is being reconnected.\n"), logDevId);

	UpdateMenuItems();

	CString szDevices = _T("Unknown");

	BOOL fUseReconnectAlert = TRUE;
	BOOL fSuccess = pGetAppConfigValue(
		_T("UseReconnectAlert"), 
		&fUseReconnectAlert);

	if (fSuccess && !fUseReconnectAlert) {
		return;
	}

	pCreateLogicalDeviceString(szDevices, logDevId);
	CString strInfoFmt, strInfo, strInfoTitle;
	strInfoFmt.LoadString(IDS_BT_RECONNECTING_INFO);
	strInfoTitle.LoadString(IDS_BT_RECONNECTING_INFO_TITLE);
	strInfo.Format(strInfoFmt, szDevices);

	ShowBalloonToolTip(strInfo, strInfoTitle, 5000, NIIF_WARNING);
}

VOID 
CMainFrame::OnNdasLogDevReconnected(NDAS_LOGICALDEVICE_ID logDevId)
{
	ATLTRACE(_T("Logical Device(%d) is reconnected.\n"), logDevId);

	UpdateMenuItems();

	CString szDevices = _T("Unknown");

	BOOL fUseReconnectAlert = TRUE;
	BOOL fSuccess = pGetAppConfigValue(
		_T("UseReconnectAlert"), 
		&fUseReconnectAlert);

	if (fSuccess && !fUseReconnectAlert) {
		return;
	}

	pCreateLogicalDeviceString(szDevices, logDevId);
	CString strInfoFmt, strInfo, strInfoTitle;
	strInfoFmt.LoadString(IDS_BT_RECONNECTED_INFO);
	strInfoTitle.LoadString(IDS_BT_RECONNECTED_INFO_TITLE);
	strInfo.Format(strInfoFmt, szDevices);

	ShowBalloonToolTip(strInfo, strInfoTitle, 5000, NIIF_INFO);
}

VOID CMainFrame::OnNdasSurrenderAccessRequest(
	DWORD dwSlotNo, 
	DWORD dwUnitNo, 
	UCHAR requestFlags, 
	LPCGUID requestHostGuid)
{
	ATLTRACE(_T("Surrender Request for Unit Device(%d,%d) FLAGS=%02X.\n"), 
		dwSlotNo, dwUnitNo, requestFlags);

	UpdateDeviceList();
	UpdateMenuItems();

	ndas::Device* pDevice = _pDeviceColl->FindDevice(dwSlotNo);
	if (NULL == pDevice) 
	{
		ATLTRACE(_T("Requested device (%d.%d) not found. Ignored. (%08X)\n"),
			dwSlotNo, 
			dwUnitNo,
			::GetLastError());
		return;
	}

	ndas::UnitDevice *pUnitDevice = pDevice->FindUnitDevice(dwUnitNo);
	if (NULL == pDevice) 
	{
		ATLTRACE(_T("Requested unit device (%d.%d) not found. Ignored. (%08X)\n"),
			dwSlotNo, 
			dwUnitNo,
			::GetLastError());
		pDevice->Release();
		return;
	}

	NDAS_HOST_INFO hostInfo;
	BOOL fSuccess = ::NdasQueryHostInfo(requestHostGuid,&hostInfo);

	CString strHostname, strDeviceName;

	if (!fSuccess) 
	{
		strHostname.LoadString(IDS_KNOWN_NDAS_HOST);
	} 
	else 
	{
		strHostname = hostInfo.szHostname;
	}

	CString strMsg;
	// The host (%1!s!) requests to give up Write Access to %2!s!.\r\n
	// Do you want to accept it and unmount related drives?
	strMsg.FormatMessage(
		IDS_ASK_SURRENDER_REQUEST_FMT,
		strHostname, pDevice->GetName());

	CString strTitle;
	strTitle.LoadString(IDS_MAIN_TITLE);

	INT_PTR iResult = MessageBox(
		strMsg, 
		strTitle,
		MB_YESNO | MB_ICONQUESTION | MB_APPLMODAL);

	if (IDYES == iResult) 
	{
		NDAS_LOGICALDEVICE_ID logDevId = pUnitDevice->GetLogicalDeviceId();
		ndas::LogicalDevice* pLogDevice = _pLogDevColl->FindLogicalDevice(logDevId);
		pLogDevice->Eject();
		pLogDevice->Release();
	}
	pUnitDevice->Release();
	pDevice->Release();

}

VOID 
CMainFrame::OnNdasServiceConnectRetry()
{
}

VOID 
CMainFrame::OnNdasServiceConnectFailed()
{
}

VOID 
CMainFrame::OnNdasServiceConnectConnected()
{
	m_fPendingUpdateDeviceList = TRUE;
	m_fPendingUpdateMenuItem = TRUE;
}

VOID 
CMainFrame::OnNdasServiceTerminating()
{
}

VOID 
CMainFrame::OnEnterMenuLoop(BOOL bIsTrackPopupMenu)
{
	// prevent the menu update during the menu loop
	::EnterCriticalSection(&m_csMenu);
}

VOID 
CMainFrame::OnExitMenuLoop(BOOL bIsTrackPopupMenu)
{
	// unlock the menu update 
	::LeaveCriticalSection(&m_csMenu);
}

LRESULT CMainFrame::OnTaskBarMenu(LPARAM uMsg, BOOL& bHandled)
{
	CTaskBarIconT<CMainFrame>::m_hMenu;
	if (m_fPendingUpdateDeviceList) {
		UpdateDeviceList();
		m_fPendingUpdateDeviceList = FALSE;
	}

	if (m_fPendingUpdateMenuItem) {
		UpdateMenuItems();
		m_fPendingUpdateMenuItem = FALSE;
	}

	return CTaskBarIconExT<CMainFrame>::OnContextMenu(uMsg, bHandled);
}

VOID 
CMainFrame::OnAnotherInstanceMessage(WPARAM wParam, LPARAM lParam)
{
	ATLTRACE(_T("MyAnotherInstMsg: WPARAM %d, LPARAM %d"), wParam, lParam);
	//		TCHAR szCmdLine[_MAX_PATH + 1];
	//		::GlobalGetAtomName((ATOM)wParam, szCmdLine, _MAX_PATH);
	//		::GlobalDeleteAtom((ATOM)wParam);
	switch (wParam) {
		case AIMSG_POPUP:
			ATLTRACE(_T("AIM_POPUP\n"));
			ShowInstPopup();
			return;
		case AIMSG_EXIT:
			ATLTRACE(_T("AIMSG_EXIT\n"));
			DestroyWindow();
			return;
		default:
			ATLTRACE(_T("AIMSG_???: %d\n"), wParam);
			return;
	}
}

static 
CString& 
pCreateLogicalDeviceString(
	CString& szNameList, 
	NDAS_LOGICALDEVICE_ID logDevId)
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
				CString szLine;
				// 0x00B7 - middle dot
				szLine.Format(_T(" %c %s\r\n"), 0x00B7, pDevice->GetName());
				szNameList += szLine;
				pDevice->Release();
			}
		}

		pLogDev->Release();
	}

	return szNameList;
}

class CWorkInProgressDlg :
	public CDialogImpl<CWorkInProgressDlg>,
	public CMessageFilter
{
	CStatic m_wndMessage;
	CString m_szMessageText;

	BEGIN_MSG_MAP_EX(CWorkInProgressDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
	END_MSG_MAP()

public:
	enum { IDD = IDD_WORKING };

	CWorkInProgressDlg();

	virtual BOOL PreTranslateMessage(MSG* pMsg)
	{return IsDialogMessage(pMsg);}

	BOOL SetMessageText(LPCTSTR szMessage);	
	BOOL SetMessageText(UINT nID);	

	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);
};

CWorkInProgressDlg::CWorkInProgressDlg()
{
}

LRESULT 
CWorkInProgressDlg::OnInitDialog(HWND hwndFocus, LPARAM lParam)
{
	m_wndMessage.Attach(GetDlgItem(IDC_PROGRESS_MESSAGE));
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	return TRUE;
}

BOOL
CWorkInProgressDlg::SetMessageText(LPCTSTR szMessage)
{
	ATLASSERT(NULL != m_hWnd);
	return m_wndMessage.SetWindowText(szMessage);
}

BOOL
CWorkInProgressDlg::SetMessageText(UINT nID)
{
	CString strBuffer;
	INT iChars = strBuffer.LoadString(nID);
	if (0 == iChars) {
		return FALSE;
	}
	return SetMessageText(strBuffer);
}

static
BOOL 
pRequestSurrenderAccess(HWND hWnd, ndas::LogicalDevice* pLogDevice)
{
//	CWorkInProgressDlg dlg;
//	dlg.Create(hWnd);
//	dlg.SetMessageText(IDS_SURRENDER_REQUEST_CONNECTING);
//	dlg.ShowWindow(SW_SHOW);

	CONST ndas::LogicalDevice::UNITDEVICE_INFO& udi = 
		pLogDevice->GetUnitDeviceInfo(0);

	ndas::Device* pDevice = _pDeviceColl->FindDevice(udi.DeviceId);
	if (NULL == pDevice) {
		return FALSE;
	}

	ndas::UnitDevice* pUnitDevice = pDevice->FindUnitDevice(udi.UnitNo);
	if (NULL == pUnitDevice) {
		return FALSE;
	}

	BOOL fSuccess = pUnitDevice->UpdateHostInfo();
	if (!fSuccess) {
		return FALSE;
	}
	
	DWORD nHosts = pUnitDevice->GetHostInfoCount();
	for (DWORD i = 0; i < nHosts; ++i) {
		ACCESS_MASK access;
		GUID hostGuid;
		CONST NDAS_HOST_INFO* pHostInfo = pUnitDevice->GetHostInfo(i, &access,&hostGuid);
		ATLASSERT(NULL != pHostInfo);
		if (NULL == pHostInfo) {
			return FALSE;
		}
		if (GENERIC_WRITE & access) {
			BOOL fSuccess = ::NdasRequestSurrenderAccess(&hostGuid,
				pUnitDevice->GetSlotNo(),
				pUnitDevice->GetUnitNo(),
				GENERIC_WRITE);
			DWORD err = ::GetLastError();
			ATLTRACE(_T("NdasRequestSurrenderAccess returned %d (%08X)\n"),
				fSuccess,
				::GetLastError());
			::SetLastError(err);
		}
	}

//	::Sleep(5000);
//	dlg.ShowWindow(SW_HIDE);
//	dlg.DestroyWindow();

	return TRUE;
}

BOOL 
pCheckNoPendingInstall(LPVOID lpContext)
{
	DWORD dwWait = CMP_WaitNoPendingInstallEvents(0);
	if (WAIT_TIMEOUT == dwWait) {
		return FALSE;
	}

	if (WAIT_OBJECT_0 == dwWait ||
		WAIT_FAILED  == dwWait) 
	{
		return TRUE;
	}
	
	return TRUE;
}

