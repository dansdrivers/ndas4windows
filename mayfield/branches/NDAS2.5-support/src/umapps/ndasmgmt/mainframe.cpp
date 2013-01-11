#include "stdafx.h"
#include "resource.h"
#include <ndas/ndasnif.h>
#include <ndas/ndasuser.h>
#include <ndas/ndastype.h>

#pragma warning(disable: 4244)
#pragma warning(disable: 4312)
#include "atlctrlxp.h"
#pragma warning(default: 4312)
#pragma warning(default: 4244)

#include "ndascls.h"
#include "optionpsh.h"
#include "devpropsh.h"
#include "aboutdlg.h"
#include "ndasmgmt.h"

#include "mainframe.h"
#include "nmmenu.h"
#include "devregwiz.h"
#include "appconf.h"
#include "confirmdlg.h"
#include "syshelp.h"
#include "waitdlg.h"
#include "runtimeinfo.h"
#include "apperrdlg.h"
#include "ddecmdparser.h"

namespace
{

//////////////////////////////////////////////////////////////////////////
//
// Predicate
//
//////////////////////////////////////////////////////////////////////////

struct EjectMounted : std::unary_function<ndas::LogicalDevicePtr, void> {
	void operator()(ndas::LogicalDevicePtr pLogDevice) const {
		if (NDAS_LOGICALDEVICE_STATUS_MOUNTED == pLogDevice->GetStatus()) {
			if (!pLogDevice->Eject()) {
				ATLTRACE("Eject failed at %d\n", pLogDevice->GetLogicalDeviceId());
			}
		}
	}
};

struct NdasDeviceMenuItemCreator : std::unary_function<ndas::DevicePtr, void> {
	NdasDeviceMenuItemCreator(CNdasDeviceMenu& ndasdm, CMenuHandle& menu) :
		ndasdm(ndasdm), menu(menu), pos(0) 
	{}
	void operator()(ndas::DevicePtr pDevice) {
		CMenuItemInfo mii;
		pDevice->UpdateStatus();
		pDevice->UpdateInfo();
		ndasdm.CreateDeviceMenuItem(pDevice, mii);
		ATLVERIFY(menu.InsertMenuItem(pos++, TRUE, &mii));
	}
private:
	CNdasDeviceMenu& ndasdm;
	CMenuHandle& menu;
	int pos;
};

//////////////////////////////////////////////////////////////////////////
//
// Local Functions
//
//////////////////////////////////////////////////////////////////////////

CString& pLogicalDeviceString(CString& szNameList, NDAS_LOGICALDEVICE_ID logDevId);
BOOL pRequestSurrenderAccess(HWND hWnd, ndas::LogicalDevicePtr pLogDevice);

} // namespace


//////////////////////////////////////////////////////////////////////////
//
// CMainFrame implementaion
//
//////////////////////////////////////////////////////////////////////////

CMainFrame::CMainFrame() :
	CInterAppMsgImpl<CMainFrame>(NDASMGMT_INST_UID),
	m_fPendingUpdateDeviceList(TRUE),
	m_fPendingUpdateMenuItem(TRUE),
	m_fHotKeyRegistered(FALSE),
	m_curBalloonType(UnknownBalloon),
	m_NdasDevices(ndas::GetDevices()),
	m_NdasLogDevices(ndas::GetLogicalDevices()),
	NDASMGMT_POPUP_HOTKEY(0xFFFF),
	WM_QUERYCANCELAUTOPLAY(0)
{
}

LRESULT 
CMainFrame::OnCreate(LPCREATESTRUCT lParam)
{
	::InitializeCriticalSection(&m_csMenu);

#ifdef NDASMGMT_USE_QUERYCANCELAUTOPLAY
	WM_QUERYCANCELAUTOPLAY = ::RegisterWindowMessage(_T("QueryCancelAutoplay"));
	ATLASSERT(WM_QUERYCANCELAUTOPLAY);
#endif

	int cxSmall = ::GetSystemMetrics(SM_CXSMICON);
	int cySmall = ::GetSystemMetrics(SM_CYSMICON);
	int cxLarge = ::GetSystemMetrics(SM_CXICON);
	int cyLarge = ::GetSystemMetrics(SM_CYICON);

	HICON hAppIcon = AtlLoadIconImage(
		IDR_MAINFRAME, LR_DEFAULTCOLOR | LR_DEFAULTSIZE, cxLarge, cyLarge);

	HICON hAppIconSmall = AtlLoadIconImage(
		IDR_MAINFRAME, LR_DEFAULTCOLOR | LR_DEFAULTSIZE, cxSmall, cySmall);

	ATLASSERT(hAppIcon && hAppIconSmall);

	SetIcon(hAppIcon, TRUE);
	SetIcon(hAppIconSmall, FALSE);

	m_hTaskbarDefaultMenu.LoadMenu(IDR_TASKBAR);
	
	m_taskbarIcon.Install(m_hWnd, 1, IDR_TASKBAR);

	// Create Taskbar Icon Image List

	// Only shell 6.0 or later can handle ILC_COLOR32

	DWORD imageListFlags = ILC_MASK;
	imageListFlags |= (GetShellVersion() >= PackVersion(6,0)) ? ILC_COLOR32 : ILC_COLOR8;
	ATLVERIFY(m_taskbarImageList.Create(cxSmall, cySmall, imageListFlags , 2, 1));
	ATLVERIFY(m_hTaskbarIconBase = AtlLoadIconImage(IDR_TASKBAR, LR_DEFAULTCOLOR, cxSmall, cySmall));
	ATLVERIFY(m_hTaskbarFailOverlay = AtlLoadIconImage(IDR_TASKBAR_FAIL, LR_DEFAULTCOLOR, cxSmall, cySmall));

	m_nTaskbarNormal = m_taskbarImageList.AddIcon(m_hTaskbarIconBase);
	m_nTaskbarFailureOverlay = m_taskbarImageList.AddIcon(m_hTaskbarFailOverlay);
	ATLVERIFY(m_taskbarImageList.SetOverlayImage(m_nTaskbarFailureOverlay, 1));

	_UpdateDeviceList();
	_UpdateMenuItems();

	if (pGetAppConfigBOOL(_T("UseHotKey"), FALSE))
	{
		NDASMGMT_POPUP_HOTKEY = ::GlobalAddAtom(ndasmgmt::NDASMGMT_ATOM_HOTKEY);
		ATLASSERT(NDASMGMT_POPUP_HOTKEY >= 0xC000 && NDASMGMT_POPUP_HOTKEY <= 0xFFFF);
		BOOL fSuccess = ::RegisterHotKey(
			m_hWnd, 
			NDASMGMT_POPUP_HOTKEY, 
			MOD_WIN, 
			'N' /* VK_N */);
		if (fSuccess)
		{
			m_fHotKeyRegistered = TRUE;
		}
		else
		{
			ATLVERIFY(0 == ::GlobalDeleteAtom(static_cast<ATOM>(NDASMGMT_POPUP_HOTKEY)));
			NDASMGMT_POPUP_HOTKEY = 0xFFFF;
		}
	}
	
	ATLVERIFY( m_wndFootprint.Create(::GetDesktopWindow()) );
	m_wndFootprint.ShowWindow(SW_NORMAL);
	m_wndFootprint.UpdateWindow();

	if (pGetAppConfigBOOL(_T("FirstRun"), TRUE))
	{
		_ShowWelcome();
		ATLVERIFY(pSetAppConfigValueBOOL(_T("FirstRun"), FALSE));
	}

	ATLVERIFY(m_bitmapHandler.Initialize());

	// Subscribe NDAS Event
	ATLVERIFY(NdasEventSubscribe(m_hWnd));

	return 0;
}

HICON 
CMainFrame::_CreateTaskbarIcon(bool NormalOrFailure /* = true */)
{
	return m_taskbarImageList.GetIcon(
		m_nTaskbarNormal, 
		NormalOrFailure ? ILD_NORMAL : INDEXTOOVERLAYMASK(1));
}

void
CMainFrame::OnDestroy()
{
	// Unsubscribe NDAS Event
	ATLVERIFY(NdasEventUnsubscribe());

	m_bitmapHandler.Cleanup();

	if (m_fHotKeyRegistered)
	{
		ATLVERIFY(::UnregisterHotKey(m_hWnd, NDASMGMT_POPUP_HOTKEY));
		// The function always returns (ATOM) 0. 
#if _DEBUG
		::SetLastError(ERROR_SUCCESS);
#endif
		ATLVERIFY(0 == ::GlobalDeleteAtom(static_cast<ATOM>(NDASMGMT_POPUP_HOTKEY)));
		ATLASSERT(::GetLastError() == ERROR_SUCCESS);
		NDASMGMT_POPUP_HOTKEY = 0xFFFF;
	}

	::DeleteCriticalSection(&m_csMenu);
	PostQuitMessage(0);
}

LRESULT
CMainFrame::OnQueryCancelAutoPlay(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ATLTRACE("%s\n", __FUNCTION__);
	return TRUE;
}

void 
CMainFrame::OnCmdFileExit(UINT /* wNotifyCode */, int /* wID */, HWND /* hWndCtl */)
{
	// PostMessage(WM_CLOSE);
	CString strText, strCaption;

	int response = ConfirmMessageBox(
		m_hWnd,
		IDS_CONFIRM_EXIT,
		IDS_MAIN_TITLE,
		_T("DontConfirmExit"),
		IDNO,
		IDYES);

	if (IDYES == response)
	{
		DestroyWindow();
	}
}

void 
CMainFrame::_ShowWelcome()
{
	//
	// We use different message based on the version of the shell,
	// as NIN_BALLOONUSERCLICK is only available in shell 6.0 (Windows XP) or later
	// 
	UINT nMsgId = (GetShellVersion() >= PackVersion(6,0)) ?
		IDS_NDASMGMT_WELCOME_TOOLTIP : 
		IDS_NDASMGMT_WELCOME_TOOLTIP_SHELL50;

	m_taskbarIcon.ShowBalloonToolTip(
		nMsgId,
		IDS_NDASMGMT_WELCOME_TITLE,
		NIIF_INFO,
		30 * 1000);

	m_curBalloonType = FirstRunBalloon;
}

void 
CMainFrame::_ShowInstPopup()
{
	m_taskbarIcon.ShowBalloonToolTip(
		IDS_NDASMGMT_ALREADY_RUNNING_TOOLTIP,
		IDS_NDASMGMT_ALREADY_RUNNING_TITLE,
		NIIF_INFO,
		5000);

	m_curBalloonType = ExistingInstanceBalloon;
}

void 
CMainFrame::OnCmdAppAbout(UINT /* wNotifyCode */, int /* wID */, HWND /* hWndCtl */)
{
	CAboutDialog dlg;
	dlg.DoModal();
}

void 
CMainFrame::OnCmdAppOptions(UINT /* wNotifyCode */, int /* wID */, HWND /* hWndCtl */)
{
	CAppOptPropSheet psh;
	CString strTitle = MAKEINTRESOURCE(IDS_OPTIONDLG_TITLE);
	psh.SetTitle(strTitle);
	psh.DoModal(m_hWnd);
}


void 
CMainFrame::OnCmdRegisterDevice(UINT /* wNotifyCode */, int /* wID */, HWND /* hWndCtl */)
{
	  CRegWizard regwiz;
	  (void) regwiz.DoModal(m_hWnd);
}

void 
CMainFrame::OnCmdRefreshStatus(UINT /* wNotifyCode */, int /* wID */, HWND /* hWndCtl */)
{
	BOOL fSuccess;
	fSuccess = _UpdateDeviceList();
	if (!fSuccess) 
	{
		ErrorMessageBox(IDS_ERROR_UPDATE_DEVICE_LIST);
	}
	_UpdateMenuItems();
}

BOOL 
CMainFrame::_UpdateDeviceList()
{
	if (!ndas::UpdateDeviceList())
	{
		return FALSE;
	}
	if (!ndas::UpdateLogicalDeviceList())
	{
		return FALSE;
	}
	return TRUE;
}

void 
CMainFrame::_UpdateMenuItems()
{
	::EnterCriticalSection(&m_csMenu);

	BOOL fSuccess(FALSE);

	if (!ndas::UpdateDeviceList())
	{
		// warning device list update failed.
	}

	if (!ndas::UpdateLogicalDeviceList())
	{
		// warning logical device list update failed.
	}

	CMenuHandle taskBarRootMenu;
	taskBarRootMenu.LoadMenu(IDR_TASKBAR);

	if (pGetAppConfigBOOL(_T("ShowUnmountAll"), FALSE)) 
	{
		ndas::LogicalDeviceConstIterator itr = std::find_if(
			m_NdasLogDevices.begin(),
			m_NdasLogDevices.end(),
			ndas::LogicalDeviceMounted());

		if (m_NdasLogDevices.end() != itr)
		{
			// There is a mounted logical device 
			taskBarRootMenu.EnableMenuItem(
				IDR_UNMOUNT_ALL, 
				MF_BYCOMMAND | MFS_ENABLED);
		}
		else
		{
			// There is no mounted logical device
			taskBarRootMenu.EnableMenuItem(
				IDR_UNMOUNT_ALL, 
				MF_BYCOMMAND | MFS_DISABLED);
		}

		// do not remove menu
	}
	else 
	{
		// otherwise remove menu
		taskBarRootMenu.RemoveMenu(IDR_UNMOUNT_ALL, MF_BYCOMMAND);
	}

	CMenuHandle taskBarMenu = ::GetSubMenu(taskBarRootMenu,0);

	MENUINFO mi = {0};
	mi.cbSize = sizeof(MENUINFO);
	mi.fMask = MIM_STYLE;
	fSuccess = taskBarMenu.GetMenuInfo(&mi);
	ATLASSERT(fSuccess);

	mi.cbSize = sizeof(MENUINFO);
	mi.fMask = MIM_STYLE | MIM_APPLYTOSUBMENUS;
	mi.dwStyle |= MNS_NOTIFYBYPOS | MNS_CHECKORBMP;
	fSuccess = taskBarMenu.SetMenuInfo(&mi);
	ATLASSERT(fSuccess);

	if (m_NdasDevices.empty()) 
	{
		CMenuItemInfo mii;
		CString strMenuText = MAKEINTRESOURCE(IDS_NO_DEVICE);
		mii.fMask = MIIM_ID | MIIM_STRING | MIIM_STATE;
		mii.dwTypeData = (LPTSTR)((LPCTSTR)strMenuText);
		mii.wID = IDR_NDAS_DEVICE_NONE;
		mii.fState = MFS_DISABLED | MFS_UNHILITE;

		fSuccess = taskBarMenu.InsertMenuItem(0, TRUE, &mii);
		ATLASSERT(fSuccess);
	} 
	else 
	{
		CNdasDeviceMenu ndm(m_hWnd);
		NdasDeviceMenuItemCreator creator(ndm, taskBarMenu);
		std::for_each(
			m_NdasDevices.begin(),
			m_NdasDevices.end(), 
			creator);
	}

	CMenu existingTaskbarMenu = m_taskbarIcon.m_hMenu;
	m_taskbarIcon.m_hMenu = taskBarRootMenu;

	::LeaveCriticalSection(&m_csMenu);
}

void
CMainFrame::OnMenuCommand(WPARAM wParam, HMENU hMenuHandle)
{
	if (!::IsMenu(hMenuHandle)) 
	{
		ATLTRACE("Invalid menu handle: %p.\n", hMenuHandle);
		return;
	}

	CMenuHandle hMenu(hMenuHandle);
	INT nPos = (INT) wParam;

	CMenuItemInfo mii;
	mii.fMask = MIIM_DATA | MIIM_ID;

	BOOL fSuccess = hMenu.GetMenuItemInfo(nPos, TRUE, &mii);
	if (!fSuccess) 
	{
		ATLTRACE("OnMenuCommand: GetMenuItemInfo failed.\n");
		return;
	}

	if (IDR_SHOW_DEVICE_PROPERTIES == mii.wID) {

		DWORD dwSlotNo = static_cast<DWORD>(LOWORD(mii.dwItemData));

		OnCmdShowDeviceProperties(dwSlotNo);

	} else if (IDR_ENABLE_DEVICE == mii.wID) {

		DWORD dwSlotNo = static_cast<DWORD>(LOWORD(mii.dwItemData));

		OnCmdEnableDevice(dwSlotNo);

	} else if (IDR_DISABLE_DEVICE == mii.wID) {
		
		DWORD dwSlotNo = static_cast<DWORD>(LOWORD(mii.dwItemData));

		OnCmdDisableDevice(dwSlotNo);

	} else if (IDR_UNREGISTER_DEVICE == mii.wID) {

		DWORD dwSlotNo = static_cast<DWORD>(LOWORD(mii.dwItemData));

		OnCmdUnregisterDevice(dwSlotNo);

	} else if (IDR_NDD_MOUNT_RW == mii.wID) {

		NDAS_LOGICALDEVICE_ID logDevId = 
			static_cast<NDAS_LOGICALDEVICE_ID>(mii.dwItemData);

		OnCmdMountDeviceRW(logDevId);

	} else if (IDR_NDD_MOUNT_RO == mii.wID) {

		NDAS_LOGICALDEVICE_ID logDevId = 
			static_cast<NDAS_LOGICALDEVICE_ID>(mii.dwItemData);

		OnCmdMountDeviceRO(logDevId);

	} else if (IDR_NDD_UNMOUNT == mii.wID) {

		NDAS_LOGICALDEVICE_ID logDevId = 
			static_cast<NDAS_LOGICALDEVICE_ID>(mii.dwItemData);

		OnCmdUnmountDevice(logDevId);

	} else {

		//
		// Reroute the message
		//

		PostMessage(WM_COMMAND, mii.wID, NULL);

	}

// for Windows 98/Me
//	int nPos = HIWORD(wParam);
	

//	UINT wId = hMenu.GetMenuItemID(nPos);
//	ATLTRACE("Menu ID = %d\n");
//	PostMessage(WM_COMMAND, MAKEWORD(0, wId), NULL);
//	SetMsgHandled(TRUE);

}

void 
CMainFrame::ErrorMessageBox(
	ATL::_U_STRINGorID Message,
	ATL::_U_STRINGorID Title,
	DWORD ErrorCode)
{
	(void) ::ErrorMessageBox(m_hWnd, Message, Title, ndasmgmt::CurrentUILangID, ErrorCode);
}

void 
CMainFrame::OnCmdEnableDevice(DWORD dwSlotNo)
{
	ndas::DevicePtr pDevice;
	if (!ndas::FindDeviceBySlotNumber(pDevice, dwSlotNo))
	{
		ATLTRACE("Invalid slot no: %d\n", dwSlotNo);
		return;
	}

	if (!pDevice->Enable(TRUE)) 
	{
		ErrorMessageBox(IDS_ERROR_ENABLE_DEVICE);
	}
}

void 
CMainFrame::OnCmdDisableDevice(DWORD dwSlotNo)
{
	ndas::DevicePtr pDevice;
	if (!ndas::FindDeviceBySlotNumber(pDevice, dwSlotNo))
	{
		ATLTRACE("Invalid slot no: %d\n", dwSlotNo);
		return;
	}
	if (!pDevice->Enable(FALSE))
	{
		ErrorMessageBox(IDS_ERROR_DISABLE_DEVICE);
	}
}

void 
CMainFrame::OnCmdMountDeviceRO(NDAS_LOGICALDEVICE_ID logDeviceId)
{
	ndas::LogicalDevicePtr pLogDevice;
	if (!ndas::FindLogicalDevice(pLogDevice, logDeviceId))
	{
		ATLTRACE("Invalid logical device id specified: %d\n", logDeviceId);
		return;
	}

	if (!pLogDevice->PlugIn(FALSE))
	{
		ErrorMessageBox(IDS_ERROR_MOUNT_DEVICE_RO);
		return;
	}

	if (!pLogDevice->UpdateStatus())
	{
		// Service Communication failure?
		return;
	}
	if (NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == pLogDevice->GetStatus())
	{
		CWaitMountDialog().DoModal(m_hWnd, pLogDevice);
	}
}

void
CMainFrame::OnCmdMountDeviceRW(NDAS_LOGICALDEVICE_ID logDeviceId)
{
	ndas::LogicalDevicePtr pLogDevice;
	if (!ndas::FindLogicalDevice(pLogDevice, logDeviceId))
	{
		ATLTRACE("Invalid logical device id specified: %d\n", logDeviceId);
		return;
	}
	if (!pLogDevice->PlugIn(TRUE))
	{
		if (NDASSVC_ERROR_MULTIPLE_WRITE_ACCESS_NOT_ALLOWED == ::GetLastError())
		{
			int response = AtlMessageBox(
				m_hWnd, 
				IDS_REQUEST_SURRENDER_RW_ACCESS, 
				IDS_MAIN_TITLE,
				MB_YESNO | MB_ICONEXCLAMATION);
			if (IDYES == response) 
			{
				pRequestSurrenderAccess(m_hWnd,pLogDevice);
			}
		}
		else
		{
			ErrorMessageBox(IDS_ERROR_MOUNT_DEVICE_RW);
		}
		return;
	}

	//
	// Wait Mount Dialog
	//
	if (!pLogDevice->UpdateStatus())
	{
		// Service Communication failure?
		return;
	}
	if (NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING == pLogDevice->GetStatus())
	{
		CWaitMountDialog().DoModal(m_hWnd, pLogDevice);
	}
}

void
CMainFrame::OnCmdUnmountDevice(NDAS_LOGICALDEVICE_ID logDeviceId)
{
	ndas::LogicalDevicePtr pLogDevice;
	if (!ndas::FindLogicalDevice(pLogDevice, logDeviceId))
	{
		ATLTRACE("Invalid logical device id specified: %d\n", logDeviceId);
		return;
	}

	int response = ConfirmMessageBox(
		m_hWnd,
		IDS_CONFIRM_UNMOUNT,
		IDS_MAIN_TITLE,
		_T("DontConfirmUnmount"), 
		IDNO, IDYES);
	if (IDYES != response)
	{
		ATLTRACE("User canceled unmount request!\n");
		return;
	}

	if (!pLogDevice->Eject()) 
	{
		ErrorMessageBox(IDS_ERROR_UNMOUNT_DEVICE);
		return;
	}

	if (!pLogDevice->UpdateStatus())
	{
		// Service Communication failure?
		return;
	}
	if (NDAS_LOGICALDEVICE_STATUS_UNMOUNT_PENDING == pLogDevice->GetStatus())
	{
		CWaitUnmountDialog().DoModal(m_hWnd, pLogDevice);
	}
}

void 
CMainFrame::OnCmdUnmountAll(UINT wNotifyCode, int wID, HWND hWndCtl)
{
	int response = ConfirmMessageBox(
		m_hWnd,
		IDS_CONFIRM_UNMOUNT_ALL,
		IDS_MAIN_TITLE,
		_T("DontConfirmUnmountAll"),
		IDNO,
		IDYES);

	if (IDYES != response) 
	{
		return;
	}

	std::for_each(
		m_NdasLogDevices.begin(), 
		m_NdasLogDevices.end(),
		EjectMounted());
}

void
CMainFrame::OnCmdUnregisterDevice(DWORD dwSlotNo)
{
	ndas::DevicePtr pDevice;
	if (!ndas::FindDeviceBySlotNumber(pDevice, dwSlotNo))
	{
		return;
	}

	CString strMessage;

	strMessage.FormatMessage(
		IDS_CONFIRM_UNREGISTER_FMT,
		pDevice->GetName());

	INT_PTR response = ConfirmMessageBox(
		m_hWnd,
		static_cast<LPCTSTR>(strMessage),
		IDS_MAIN_TITLE,
		_T("DontConfirmUnregister"),
		IDNO,
		IDYES);

	if (IDYES != response) 
	{
		return;
	}

	if (NDAS_DEVICE_STATUS_DISABLED != pDevice->GetStatus()) 
	{
		if (!pDevice->Enable(FALSE)) 
		{
			ErrorMessageBox(IDS_ERROR_UNREGISTER_DEVICE);
			ndas::UpdateDeviceList();
			return;
		}
	}

	if (!::NdasUnregisterDevice(dwSlotNo))
	{
		ErrorMessageBox(IDS_ERROR_UNREGISTER_DEVICE);
	}

	ndas::UpdateDeviceList();
}

void
CMainFrame::OnCmdShowDeviceProperties(DWORD dwSlotNo)
{
	ndas::DevicePtr pDevice;
	if (!ndas::FindDeviceBySlotNumber(pDevice, dwSlotNo))
	{
		ATLTRACE("Invalid slot no: %d\n", dwSlotNo);
		return;
	}

	CString strTitle;
	strTitle.FormatMessage(IDS_DEVICE_PROP_TITLE, pDevice->GetName());

	CDevicePropSheet psh(static_cast<LPCTSTR>(strTitle), 0, m_hWnd);
	psh.SetDevice(pDevice);
	psh.DoModal();

}

void
CMainFrame::OnNdasDeviceEntryChanged()
{
	ATLTRACE("Device Entry changed.\n");

	m_fPendingUpdateDeviceList = TRUE;
	m_fPendingUpdateMenuItem = TRUE;
}

void 
CMainFrame::OnNdasDevicePropChanged(DWORD dwSlotNo)
{
	ATLTRACE("Device(%d) property changed.\n", dwSlotNo);

	m_fPendingUpdateMenuItem = TRUE;
}

void 
CMainFrame::OnNdasUnitDevicePropChanged(
	DWORD dwSlotNo, 
	DWORD dwUnitNo)
{
	ATLTRACE("Unit Device(%d,%d) property changed.\n", dwSlotNo, dwUnitNo);

	m_fPendingUpdateMenuItem = TRUE;
}

void 
CMainFrame::OnNdasDeviceStatusChanged(
	DWORD dwSlotNo,
	NDAS_DEVICE_STATUS oldStatus,
	NDAS_DEVICE_STATUS newStatus)
{
	ATLTRACE("Device(%d) status changed: %08X->%08X.\n", 
		dwSlotNo, oldStatus, newStatus);

	m_fPendingUpdateMenuItem = TRUE;
}

void 
CMainFrame::OnNdasLogDeviceEntryChanged()
{
	ATLTRACE("Logical Device Entry changed.\n");
	m_fPendingUpdateMenuItem = TRUE;
}

void 
CMainFrame::OnNdasLogDeviceStatusChanged(
	NDAS_LOGICALDEVICE_ID logDevId,
	NDAS_LOGICALDEVICE_STATUS oldStatus,
	NDAS_LOGICALDEVICE_STATUS newStatus)
{
	ATLTRACE("Logical Device(%d) Status changed: %08X->%08X.\n",
		logDevId, oldStatus, newStatus);
	m_fPendingUpdateMenuItem = TRUE;
}

void 
CMainFrame::OnNdasLogDevicePropertyChanged(
	NDAS_LOGICALDEVICE_ID logDevId)
{
	ATLTRACE("Logical Device(%d) Property changed.\n", logDevId);
	m_fPendingUpdateMenuItem = TRUE;
}

void 
CMainFrame::OnNdasLogDeviceRelationChanged(
	NDAS_LOGICALDEVICE_ID logDevId)
{
	ATLTRACE("Logical Device(%d) Relation changed.\n", logDevId);
	m_fPendingUpdateMenuItem = TRUE;
}

void 
CMainFrame::OnNdasLogDeviceDisconnected(
	NDAS_LOGICALDEVICE_ID logDevId)
{
	ATLTRACE("Logical Device(%d) disconnected.\n", logDevId);
	_UpdateMenuItems();

	CString szDevices = MAKEINTRESOURCE(IDS_DEVICE_UNKNOWN);

	BOOL fUseDisconnectAlert = pGetAppConfigBOOL(_T("UseDisconnectAlert"), TRUE);
	if (!fUseDisconnectAlert) 
	{
		return;
	}

	pLogicalDeviceString(szDevices, logDevId);

	CString strMessage;
	strMessage.FormatMessage(IDS_BT_DISCONNECTED_INFO, szDevices);

	m_taskbarIcon.ShowBalloonToolTip(
		static_cast<LPCTSTR>(strMessage), 
		IDS_BT_DISCONNECTED_INFO_TITLE, 
		NIIF_ERROR, 
		5000);
}

void 
CMainFrame::OnNdasLogDeviceAlarmed(
	NDAS_LOGICALDEVICE_ID logDevId,
	ULONG AdapterStatus)
{
	ATLTRACE("Logical Device(%d) is alarmed(%08lx).\n", logDevId, AdapterStatus);

	_UpdateMenuItems();

	// Default String
	CString szDevices = MAKEINTRESOURCE(IDS_DEVICE_UNKNOWN);

	pLogicalDeviceString(szDevices, logDevId);

	BOOL fUseEmergencyAlert = pGetAppConfigBOOL(_T("UseReconnectAlert"), TRUE);
	if (!fUseEmergencyAlert) 
	{
		return;
	}

	CString strTitle = MAKEINTRESOURCE(IDS_BT_ALARMED_INFO_TITLE);
	UINT nFormatID;

	if(NDAS_DEVICE_ALARM_RECONNECTING == AdapterStatus)
	{
		nFormatID = IDS_BT_ALARMED_RECONNECTING;
	}
	else if(NDAS_DEVICE_ALARM_RECONNECTED == AdapterStatus)
	{
		nFormatID = IDS_BT_ALARMED_RECONNECTED;
	}
	else if(NDAS_DEVICE_ALARM_MEMBER_FAULT == AdapterStatus)
	{
		nFormatID = IDS_BT_ALARMED_MEMBER_FAULT;
	}
	else if(NDAS_DEVICE_ALARM_RECOVERING == AdapterStatus)
	{
		nFormatID = IDS_BT_ALARMED_RECOVERING;
	}
	else if(NDAS_DEVICE_ALARM_RECOVERRED == AdapterStatus)
	{
		nFormatID = IDS_BT_ALARMED_RECOVERRED;
	}
	else // if(NDAS_DEVICE_ALARM_NORMAL == AdapterStatus)
	{
//		strInfoFmt.LoadString(IDS_BT_NORMAL_INFO);
		return;
	}

	CString strMessage;
	strMessage.FormatMessage(nFormatID, szDevices);

	m_taskbarIcon.ShowBalloonToolTip(
		static_cast<LPCTSTR>(strMessage), 
		IDS_BT_ALARMED_INFO_TITLE, 
		NIIF_WARNING, 
		5000);
}

void 
CMainFrame::OnNdasSurrenderAccessRequest(
	DWORD dwSlotNo, 
	DWORD dwUnitNo, 
	UCHAR requestFlags, 
	LPCGUID requestHostGuid)
{
	ATLTRACE("Surrender Request for Unit Device(%d,%d) FLAGS=%02X.\n", 
		dwSlotNo, dwUnitNo, requestFlags);

	_UpdateDeviceList();
	_UpdateMenuItems();

	ndas::DevicePtr pDevice;
	if (!ndas::FindDeviceBySlotNumber(pDevice, dwSlotNo))
	{
		ATLTRACE("Requested device (%d.%d) not found. Ignored. (%08X)\n",
			dwSlotNo, 
			dwUnitNo,
			::GetLastError());
		return;
	}

	ndas::UnitDevicePtr pUnitDevice;
	if (!pDevice->FindUnitDevice(pUnitDevice, dwUnitNo))
	{
		ATLTRACE("Requested unit device (%d.%d) not found. Ignored. (%08X)\n",
			dwSlotNo, 
			dwUnitNo,
			::GetLastError());
		return;
	}

	CString strHostname;

	NDAS_HOST_INFO hostInfo = {0};
	if (!::NdasQueryHostInfo(requestHostGuid,&hostInfo)) 
	{
		strHostname.LoadString(IDS_KNOWN_NDAS_HOST);
	} 
	else 
	{
		strHostname = hostInfo.szHostname;
	}

	// The host (%1!s!) requests to give up Write Access to %2!s!.\r\n
	// Do you want to accept it and unmount related drives?
	CString strMessage;
	strMessage.FormatMessage(
		IDS_ASK_SURRENDER_REQUEST_FMT,
		strHostname, 
		pDevice->GetName());

	int response = ::AtlMessageBox(
		m_hWnd,
		static_cast<LPCTSTR>(strMessage), 
		IDS_MAIN_TITLE,
		MB_YESNO | MB_ICONQUESTION | MB_APPLMODAL | MB_SETFOREGROUND | MB_TOPMOST);

	if (IDYES == response) 
	{
		NDAS_LOGICALDEVICE_ID logDeviceId = pUnitDevice->GetLogicalDeviceId();
		ndas::LogicalDevicePtr pLogDevice;
		if (!ndas::FindLogicalDevice(pLogDevice, logDeviceId))
		{
			ATLTRACE("Cannot find logical device %d\n", logDeviceId);
		}
		if (!pLogDevice->Eject())
		{
			ATLTRACE("Logical device %d eject failed\n", logDeviceId);
		}
	}
}

void 
CMainFrame::OnNdasServiceConnectRetry()
{
	ATLTRACE("Trying to connect to the NDAS Service.\n");
}

void 
CMainFrame::OnNdasServiceConnectFailed()
{
	ATLTRACE("Connection to the NDAS Service failed.\n");

	ATLVERIFY(m_taskbarIcon.ChangeIcon(_CreateTaskbarIcon(false)));
	ATLVERIFY( m_taskbarIcon.SetToolTipText(IDR_TASKBAR_FAIL) );
}

void 
CMainFrame::OnNdasServiceConnectConnected()
{
	ATLTRACE("NDAS Service connection established.\n");

	ATLVERIFY(m_taskbarIcon.ChangeIcon(_CreateTaskbarIcon()));
	ATLVERIFY(m_taskbarIcon.SetToolTipText(IDR_TASKBAR));

	m_fPendingUpdateDeviceList = TRUE;
	m_fPendingUpdateMenuItem = TRUE;
}

void 
CMainFrame::OnNdasServiceTerminating()
{
	ATLTRACE("NDAS Service is terminating.\n");

	ATLVERIFY(m_taskbarIcon.ChangeIcon(_CreateTaskbarIcon(false)));
	ATLVERIFY(m_taskbarIcon.SetToolTipText(IDR_TASKBAR_FAIL));
}

void 
CMainFrame::OnNdasServiceRejectedSuspend()
{
	AtlMessageBox(m_hWnd, IDS_SUSPEND_REJECTED, IDS_MAIN_TITLE);
}

void 
CMainFrame::OnEnterMenuLoop(BOOL bIsTrackPopupMenu)
{
	// prevent the menu update during the menu loop
	::EnterCriticalSection(&m_csMenu);
}

void 
CMainFrame::OnExitMenuLoop(BOOL bIsTrackPopupMenu)
{
	// unlock the menu update 
	::LeaveCriticalSection(&m_csMenu);
}

//void
//CMainFrame::OnMenuSelect(UINT nID, UINT uFlags, HMENU hMenu)
//{
//	if (0xFFFF == uFlags && NULL == hMenu)
//	{
//		// Returns focus to the taskbar notification area
//		ATLVERIFY(m_taskbarIcon.SetFocus());
//	}
//}

void
CMainFrame::OnTaskbarMenu(UINT)
{
	// if there are any pop-up windows, do not handle this!

	HWND hWnd = ::GetLastActivePopup(m_hWnd);
	if (hWnd != m_hWnd && hWnd != m_wndFootprint.m_hWnd)
	{
		ATLTRACE("Active popup HWND %x exists.\n", hWnd);
		::FlashWindow(hWnd, TRUE);
		::SwitchToThisWindow(hWnd, FALSE);
		return;
	}

	if (m_wndImportDlg.m_hWnd)
	{
		m_wndImportDlg.FlashWindow(TRUE);
		::SwitchToThisWindow(m_wndImportDlg, FALSE);
		return;
	}

	// We will save the position now as updating the list 
	// from the service may take a while and the user may change
	// the mouse position.

	POINT pt;
	::GetCursorPos(&pt);      

	if (m_fPendingUpdateDeviceList) 
	{
		_UpdateDeviceList();
		m_fPendingUpdateDeviceList = FALSE;
	}

	if (m_fPendingUpdateMenuItem) 
	{
		_UpdateMenuItems();
		m_fPendingUpdateMenuItem = FALSE;
	}

	HMENU hSubMenu = ::GetSubMenu(m_taskbarIcon.m_hMenu, 0); 
	ATLASSERT(::IsMenu(hSubMenu));
	// Make first menu-item the default (bold font)      
	// ::SetMenuDefaultItem(hSubMenu, 0, TRUE);
	// Display the menu at the current mouse location.
	::SetForegroundWindow(m_taskbarIcon.m_nid.hWnd);
	BOOL ret = ::TrackPopupMenu(hSubMenu, 0, pt.x, pt.y, 0, m_taskbarIcon.m_nid.hWnd, NULL);
	ATLTRACE("TrackPopupMenu returns %d : Error %d\n", ret, ::GetLastError());
	::PostMessage(m_taskbarIcon.m_nid.hWnd, WM_NULL, 0,0); // Fixes Win95 bug
}

void 
CMainFrame::OnBalloonUserClick(UINT)
{
	if (m_curBalloonType == FirstRunBalloon)
	{
		m_curBalloonType = UnknownBalloon;
		OnCmdRegisterDevice(0,0,0);
	}
}

void 
CMainFrame::OnHotKey(int id, UINT fsModifier, UINT vk)
{
	if (NDASMGMT_POPUP_HOTKEY != id)
	{
		SetMsgHandled(FALSE); 
		return;
	}

	OnTaskbarMenu(0);

	return;
}

void 
CMainFrame::OnInterAppMsg(WPARAM wParam, LPARAM lParam)
{
	ATLTRACE("InterAppMsg: WPARAM %d, LPARAM %d\n", wParam, lParam);
	switch (wParam) 
	{
	case INTERAPPMSG_POPUP:
		ATLTRACE("INTERAPPMSG_POPUP\n");
		_ShowInstPopup();
		return;
	case INTERAPPMSG_EXIT:
		ATLTRACE("INTERAPPMSG_EXIT\n");
		DestroyWindow();
		return;
	default:
		ATLTRACE("INTERAPPMSG_???: %d\n", wParam);
		return;
	}
}

void 
CMainFrame::OnFinalMessage(HWND hWnd)
{
	if (m_wndFootprint.IsWindow())
	{
		m_wndFootprint.DestroyWindow();
	}
}

LRESULT  
CMainFrame::OnDDEInitiate(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam)
{
	HWND hWndSender = HWND(wParam);

	const ATOM atAppName = ::GlobalAddAtom(ndasmgmt::NDASMGMT_ATOM_DDE_APP);
	const ATOM atTopic = ::GlobalAddAtom(ndasmgmt::NDASMGMT_ATOM_DDE_DEFAULT_TOPIC);

	ATLASSERT(atAppName);
	ATLASSERT(atTopic);

	UINT_PTR atSenderApp;
	UINT_PTR atSenderTopic;

	ATLVERIFY(::UnpackDDElParam(WM_DDE_INITIATE, lParam, &atSenderApp, &atSenderTopic));

	ATLTRACE("OnDDEInitiate: HWND=%x, atSenderAppName=%x, atSenderTopic=%x"
		" atAppName=%x, atTopic=%x\n", 
		hWndSender, atSenderApp, atSenderTopic,
		atAppName, atTopic);
	
	if ((atAppName == atSenderApp || 0 == atSenderApp) && 
		(atTopic == atSenderTopic || 0 == atTopic))
	{
		ATLTRACE("Registered DDE operation\n");
		ATLVERIFY(
		::SendMessage(
			hWndSender, 
			WM_DDE_ACK, 
			WPARAM(HandleToUlong(m_hWnd)),
			::PackDDElParam(WM_DDE_ACK, atAppName, atTopic)));
	}
	else
	{
		ATLTRACE("Non-registered DDE operation\n");
	}

	::GlobalDeleteAtom(atAppName);
	::GlobalDeleteAtom(atTopic);

	return TRUE;
}


LRESULT  
CMainFrame::OnDDEExecute(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam)
{
	HWND hWndSender = (HWND)wParam;
	HGLOBAL hGlobal = (HGLOBAL)lParam;
	LPVOID lpGlobal = ::GlobalLock(hGlobal);

	ATLTRACE("OnDDEExecute: HWND=%x, Global=%p\n", hWndSender, lpGlobal);

	if (::IsWindowUnicode(hWndSender) && ::IsWindowUnicode(m_hWnd))
	{
		ATLTRACE(L"UnicodeCommandString=%s\n", lpGlobal);
		CDdeCommandParser parser;
		LPCTSTR lpNextCmd = static_cast<LPCWSTR>(lpGlobal);
		if (parser.Parse(lpNextCmd, &lpNextCmd))
		{
			ATLTRACE("%ws,%ws", parser.GetOpCode(), parser.GetParam());
			if (0 == lstrcmpi(_T("open"), parser.GetOpCode()))
			{
				LPCTSTR FileName = parser.GetParam();
				_ImportRegistration(FileName);
			}
		}
		else
		{
			ATLTRACE("DDECommandString Parse Error\n");
		}
	}
	else
	{
		ATLTRACE("CommandString=%s\n", lpGlobal);
		CA2W wGlobal(static_cast<LPCSTR>(lpGlobal));
		CDdeCommandParser parser;
		LPCTSTR lpNextCmd = wGlobal;
		if (parser.Parse(lpNextCmd, &lpNextCmd))
		{
			ATLTRACE("%ws,%ws", parser.GetOpCode(), parser.GetParam());
			if (0 == lstrcmpi(_T("open"), parser.GetOpCode()))
			{
				LPCTSTR FileName = parser.GetParam();
				_ImportRegistration(FileName);
			}
		}
		else
		{
			ATLTRACE("DDECommandString Parse Error\n");
		}
	}


	::GlobalUnlock(hGlobal);

	DWORD ddeAckParam = 0;
	DDEACK* pDdeAck = reinterpret_cast<DDEACK*>(&ddeAckParam);
	pDdeAck->bAppReturnCode = TRUE;
	pDdeAck->fAck = TRUE;
	ATLVERIFY(
	::PostMessage(
		hWndSender, 
		WM_DDE_ACK, 
		HandleToUlong(m_hWnd), 
		::PackDDElParam(WM_DDE_ACK, ddeAckParam, HandleToUlong(hGlobal))));

	return TRUE;
}

LRESULT 
CMainFrame::OnDDETerminate(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/)
{
	HWND hWndSender = HWND(wParam);
	ATLTRACE("OnDDETerminate: HWND=%x\n", hWndSender);
	ATLVERIFY(
		::PostMessage(
			hWndSender, 
			WM_DDE_TERMINATE, 
			HandleToUlong(m_hWnd),
			0));
	return TRUE;
}

void
CMainFrame::_ImportRegistration(LPCTSTR szFileName)
{
	DWORD count = 0;
	NDAS_NIF_V1_ENTRY* pEntry;
	HRESULT hr = ::NdasNifImport(szFileName, &count, &pEntry);
	if (SUCCEEDED(hr))
	{
		NifEntryArray entryArray;

		for (DWORD i = 0; i < count; ++i)
		{
			ATLTRACE(_T("%d/%d: %s\n"), pEntry->Name);
			entryArray.Add(NifEntry(pEntry[i]));
		}

		if (NULL == m_wndImportDlg.m_hWnd)
		{
			ATLVERIFY(m_wndImportDlg.Create(m_hWnd, PtrToUlong(&entryArray)));
			m_wndImportDlg.CenterWindow(::GetDesktopWindow());
			m_wndImportDlg.ShowWindow(SW_SHOWNORMAL);
		}
		else
		{
			m_wndImportDlg.AddNifEntry(entryArray);
			m_wndImportDlg.SetActiveWindow();
		}

		ATLVERIFY(NULL == ::LocalFree(pEntry));
	}
	else
	{
		ATLTRACE("NdasNifImport Error hr=%x\n", hr);
		CString strError;
		TCHAR szCompactPath[50];
		LPCTSTR lpFileName;
		if (::PathCompactPathEx(szCompactPath, szFileName, 50, 0))
		{
			lpFileName = szCompactPath;
		}
		else
		{
			lpFileName = szFileName;
		}
		strError.FormatMessage(IDS_ERROR_IMPORT_FMT, lpFileName);
		ErrorMessageBox(static_cast<LPCTSTR>(strError), IDS_MAIN_TITLE, hr);
	}
}

bool
CMainFrame::_ProcessRegistration(
	LPCTSTR lpszDeviceName,
	LPCTSTR lpszDeviceId,
	LPCTSTR lpszWriteKey)
{
	ndas::DevicePtr pExistingDevice;

	if (ndas::FindDeviceByNdasId(pExistingDevice, lpszDeviceId))
	{
		AtlMessageBox(
			m_hWnd,
			IDS_ERROR_DUPLICATE_ENTRY,
			IDS_MAIN_TITLE,
			MB_OK | MB_ICONWARNING);
		return false;
	}

	CDeviceRenameDialog renameDlg;
	while (ndas::FindDeviceByName(pExistingDevice, lpszDeviceName))
	{
		int response = AtlMessageBox(
			m_hWnd, 
			IDS_ERROR_DUPLICATE_NAME, 
			IDS_MAIN_TITLE, 
			MB_OKCANCEL | MB_ICONWARNING);
		if (IDOK == response)
		{
			renameDlg.SetName(lpszDeviceName);
			renameDlg.DoModal(m_hWnd);
			lpszDeviceName = renameDlg.GetName();
		}
		else
		{
			return false;
		}
	}

	DWORD dwSlotNo = ::NdasRegisterDevice(
		lpszDeviceId, lpszWriteKey, 
		lpszDeviceName, NDAS_DEVICE_REG_FLAG_NONE);

	// Registration failure will not close dialog
	if (0 == dwSlotNo) 
	{
		ErrorMessageBox(IDS_ERROR_REGISTER_DEVICE_FAILURE);
		return false;
	}

	//
	// Enable on register is an optional feature
	// Even if it's failed, still go on to close.
	//
	if (!ndas::UpdateDeviceList()) 
	{
		DWORD err = ::GetLastError();
		ATLTRACE("Enabling device at slot %d failed\n", dwSlotNo);
		::SetLastError(err);
		return false;
	}

	ndas::DevicePtr pDevice;
	if (!ndas::FindDeviceBySlotNumber(pDevice, dwSlotNo)) 
	{
		DWORD err = ::GetLastError();
		ATLTRACE("Enabling device at slot %d failed\n", dwSlotNo);
		::SetLastError(err);
		return false;
	}

	if (!pDevice->Enable()) 
	{
		DWORD err = ::GetLastError();
		ATLTRACE("Enabling device at slot %d failed\n", dwSlotNo);
		::SetLastError(err);
		return false;
	}

	CString strMessage;
	strMessage.FormatMessage(IDS_IMPORT_SUCCESS, lpszDeviceName);
	
	AtlMessageBox(m_hWnd, static_cast<LPCTSTR>(strMessage), IDS_MAIN_TITLE);

	return true;
}

void 
CMainFrame::OnParentNotify(UINT nEvent, UINT nID, LPARAM lParam)
{
	ATLTRACE("OnParentNotify: Event=%x, ID=%x, LPARAM=%x\n", nEvent, nID, lParam);
}

//////////////////////////////////////////////////////////////////////////
//
// Local Functions
//
//////////////////////////////////////////////////////////////////////////

namespace
{

CString& 
pLogicalDeviceString(
	CString& strNameList, 
	NDAS_LOGICALDEVICE_ID logDeviceId)
{
	
	ndas::LogicalDevicePtr pLogDevice;
	if (!ndas::FindLogicalDevice(pLogDevice, logDeviceId))
	{
		return strNameList;
	}

	strNameList = _T("");

	for (DWORD i = 0; i < pLogDevice->GetUnitDeviceInfoCount(); ++i) {

		ndas::LogicalDevice::UNITDEVICE_INFO ui = 
			pLogDevice->GetUnitDeviceInfo(i);

		ndas::DevicePtr pDevice;
		if (!ndas::FindDeviceByNdasId(pDevice, ui.DeviceId))
		{
			continue;
		}

		CString strLine;
		// 0x00B7 - middle dot
		strLine.Format(_T(" \0xB7 %s\r\n"), pDevice->GetName());
		strNameList += strLine;
	}

	return strNameList;
}

BOOL 
pRequestSurrenderAccess(
	HWND hWnd, 
	ndas::LogicalDevicePtr pLogDevice)
{
	const ndas::LogicalDevice::UNITDEVICE_INFO& udi = 
		pLogDevice->GetUnitDeviceInfo(0);

	ndas::DevicePtr pDevice;
	if (!ndas::FindDeviceByNdasId(pDevice, udi.DeviceId))
	{
		return FALSE;
	}

	ndas::UnitDevicePtr pUnitDevice;
	if (!pDevice->FindUnitDevice(pUnitDevice, udi.UnitNo))
	{
		return FALSE;
	}

	if (!pUnitDevice->UpdateHostInfo()) 
	{
		return FALSE;
	}
	
	DWORD nHosts = pUnitDevice->GetHostInfoCount();
	for (DWORD i = 0; i < nHosts; ++i) 
	{
		ACCESS_MASK access;
		GUID hostGuid;
		CONST NDAS_HOST_INFO* pHostInfo = pUnitDevice->GetHostInfo(i, &access,&hostGuid);
		ATLASSERT(NULL != pHostInfo);
		if (NULL == pHostInfo) 
		{
			return FALSE;
		}
		if (GENERIC_WRITE & access) 
		{
			BOOL fSuccess = ::NdasRequestSurrenderAccess(&hostGuid,
				pUnitDevice->GetSlotNo(),
				pUnitDevice->GetUnitNo(),
				GENERIC_WRITE);
			DWORD err = ::GetLastError();
			ATLTRACE("NdasRequestSurrenderAccess returned %d (%08X)\n",
				fSuccess,
				::GetLastError());
			::SetLastError(err);
		}
	}

	return TRUE;
}

} // namespace
