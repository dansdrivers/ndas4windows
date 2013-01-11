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

#include <vector>
#include "dismountalldlg.h"

#include "ndastaskdlgs.hpp"

namespace
{

//////////////////////////////////////////////////////////////////////////
//
// Predicate
//
//////////////////////////////////////////////////////////////////////////

struct EjectMounted : std::unary_function<ndas::LogicalDevicePtr, void> 
{
	void operator()(ndas::LogicalDevicePtr pLogDevice) const 
	{
		if (NDAS_LOGICALDEVICE_STATUS_MOUNTED == pLogDevice->GetStatus()) 
		{
			if (!pLogDevice->Eject()) 
			{
				ATLTRACE("Eject failed at %d\n", pLogDevice->GetLogicalDeviceId());
			}
		}
	}
};

struct NdasDeviceMenuItemCreator : std::unary_function<ndas::DevicePtr, void> 
{
	NdasDeviceMenuItemCreator(CNdasTaskBarMenu& ndasdm, CMenuHandle& menu) :
		ndasdm(ndasdm), menu(menu), pos(0) 
	{
	}
	void operator()(ndas::DevicePtr pDevice) 
	{
		CMenuItemInfo mii;
		pDevice->UpdateStatus();
		pDevice->UpdateInfo();
		ndasdm.CreateDeviceMenuItem(pDevice, mii);
		ATLVERIFY(menu.InsertMenuItem(pos++, TRUE, &mii));
	}
private:
	CNdasTaskBarMenu& ndasdm;
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
	WM_QUERYCANCELAUTOPLAY(0),
	m_RawDiskEventCount(0)
{
}

LRESULT 
CMainFrame::OnCreate(LPCREATESTRUCT lParam)
{
	xTaskDialogInitialize();

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

	// Create Task Bar Icon Image List

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
	
	if (pGetAppConfigBOOL(_T("FirstRun"), TRUE))
	{
		_ShowWelcome();
		ATLVERIFY(pSetAppConfigValueBOOL(_T("FirstRun"), FALSE));
	}

	m_taskbarMenu.Init(m_hWnd);
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

	xTaskDialogUninitialize();

	SetMsgHandled(FALSE);
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

	int response = pTaskDialogVerify(
		m_hWnd,
		IDS_MAIN_TITLE,
		IDS_CONFIRM_EXIT,
		static_cast<LPCTSTR>(NULL),
		_T("DontConfirmExit"),
		TDCBF_YES_BUTTON | TDCBF_NO_BUTTON,
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

	_UpdateMenuItems();
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
	BOOL fSuccess(FALSE);

	if (!ndas::UpdateDeviceList())
	{
		// warning device list update failed.
	}

	if (!ndas::UpdateLogicalDeviceList())
	{
		// warning logical device list update failed.
	}

	m_taskbarMenu.ClearItemStringData();

	BOOL showText = TRUE;
	pGetAppConfigValue(_T("ShowDeviceStatusText"), &showText);
	m_taskbarMenu.ShowDeviceStatusText(showText);

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
		std::for_each(
			m_NdasDevices.begin(),
			m_NdasDevices.end(), 
			NdasDeviceMenuItemCreator(m_taskbarMenu, taskBarMenu));
	}

	CMenu existingTaskbarMenu = m_taskbarIcon.m_hMenu;
	m_taskbarIcon.m_hMenu = taskBarRootMenu;
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

	DWORD slotNo = static_cast<DWORD>(LOWORD(mii.dwItemData));
	NDAS_LOGICALDEVICE_ID lun = 
		static_cast<NDAS_LOGICALDEVICE_ID>(mii.dwItemData);

	switch (mii.wID)
	{
	case IDR_SHOW_DEVICE_PROPERTIES:
		OnCmdShowDeviceProperties(slotNo);
		break;
	case IDR_ENABLE_DEVICE:
		OnCmdEnableDevice(slotNo);
		break;
	case IDR_DISABLE_DEVICE:
		OnCmdDisableDevice(slotNo);
		break;
	case IDR_UNREGISTER_DEVICE:
		OnCmdUnregisterDevice(slotNo);
		break;
	case IDR_RESET_DEVICE:
		OnCmdResetDevice(slotNo);
		break;

	case IDR_NDD_MOUNT_RW:
		OnCmdMountLogicalUnit(lun, TRUE);
		break;
	case IDR_NDD_MOUNT_RO:
		OnCmdMountLogicalUnit(lun, FALSE);
		break;
	case IDR_NDD_UNMOUNT:
		OnCmdDismountLogicalUnit(lun);
		break;
	default:
		//
		// Reroute the message
		//
		PostMessage(WM_COMMAND, mii.wID, NULL);
		break;
	}
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
CMainFrame::OnCmdMountLogicalUnit(NDAS_LOGICALDEVICE_ID logDeviceId, BOOL WriteAccess)
{
	TASKDIALOGCONFIG taskConfig;

	ndas::LogicalDevicePtr pLogDevice;
	if (!ndas::FindLogicalDevice(pLogDevice, logDeviceId))
	{
		ATLTRACE("Invalid logical device id specified: %d\n", logDeviceId);
		return;
	}

	if (pLogDevice->GetLastError() == NDAS_LOGICALDEVICE_ERROR_DEGRADED_MODE) 
	{
		int response = pTaskDialogVerify(
			m_hWnd,
			IDS_MAIN_TITLE,
			IDS_CONFIRM_DEGRADED_MOUNT,
			IDS_CONFIRM_DEGRADED_MOUNT_DESC,
			_T("DontConfirmDegradedMode"), 
			TDCBF_YES_BUTTON | TDCBF_NO_BUTTON,
			IDNO, 
			IDYES);

		if (IDNO == response)
		{
			return;
		}
	}

	CMountTaskDialog dlg(pLogDevice, WriteAccess);

retry:

	dlg.DoModal(GetDesktopWindow());

	HRESULT hr = dlg.GetTaskResult();

	if (NDASSVC_ERROR_MULTIPLE_WRITE_ACCESS_NOT_ALLOWED == hr)
	{
		int response = pTaskDialogVerify(
			m_hWnd,
			IDS_MAIN_TITLE,
			IDS_REQUEST_SURRENDER_RW_ACCESS,
			IDS_REQUEST_SURRENDER_RW_ACCESS_DESC,
			static_cast<LPCTSTR>(NULL),
			TDCBF_YES_BUTTON | TDCBF_NO_BUTTON,
			IDYES,
			IDYES);

		if (IDYES == response) 
		{
			pRequestSurrenderAccess(m_hWnd,pLogDevice);
		}
	}
	else if (FAILED(hr))
	{
		CString formattedMessage = pFormatErrorString(hr);

		CTaskDialogEx taskDialog(m_hWnd);
		taskDialog.SetCommonButtons(TDCBF_RETRY_BUTTON | TDCBF_CANCEL_BUTTON);
		taskDialog.SetWindowTitle(IDS_MAIN_TITLE);
		taskDialog.SetMainIcon(IDR_MAINFRAME);
		if (WriteAccess)
		{
			taskDialog.SetMainInstructionText(IDS_ERROR_MOUNT_DEVICE_RW);
		}
		else
		{
			taskDialog.SetMainInstructionText(IDS_ERROR_MOUNT_DEVICE_RO);			
		}
		taskDialog.SetContentText(static_cast<LPCTSTR>(formattedMessage));

		int selected;
		HRESULT hr = taskDialog.DoModal(GetDesktopWindow(), &selected);
		ATLASSERT(SUCCEEDED(hr));

		if (SUCCEEDED(hr) && IDRETRY == selected)
		{
			goto retry;
		}

		// ErrorMessageBox(IDS_ERROR_MOUNT_DEVICE_RW);
	}

	return;
}

void
CMainFrame::OnCmdDismountLogicalUnit(NDAS_LOGICALDEVICE_ID logDeviceId)
{
	ndas::LogicalDevicePtr pLogDevice;
	if (!ndas::FindLogicalDevice(pLogDevice, logDeviceId))
	{
		ATLTRACE("Invalid logical device id specified: %d\n", logDeviceId);
		return;
	}

	int response = pTaskDialogVerify(
		m_hWnd,
		IDS_MAIN_TITLE,
		IDS_CONFIRM_UNMOUNT,
		IDS_CONFIRM_UNMOUNT_DESC,
		_T("DontConfirmUnmount"), 
		TDCBF_YES_BUTTON | TDCBF_NO_BUTTON,
		IDNO, 
		IDYES);

	if (IDYES != response)
	{
		ATLTRACE("User canceled unmount request!\n");
		return;
	}

	CDismountTaskDialog dismountTaskDialog(pLogDevice);

retry:

	dismountTaskDialog.DoModal(GetDesktopWindow());

	HRESULT hr = dismountTaskDialog.GetTaskResult();

	if (FAILED(hr) ||
		NDAS_LOGICALUNIT_STATUS_MOUNTED == pLogDevice->GetStatus())
	{
		CTaskDialogEx errorTaskDialog(GetDesktopWindow());

		// errorTaskDialog.ModifyFlags(0, TDF_USE_COMMAND_LINKS);
		errorTaskDialog.SetWindowTitle(IDS_MAIN_TITLE);
		errorTaskDialog.SetMainIcon(IDR_MAINFRAME);
		errorTaskDialog.SetCommonButtons(TDCBF_RETRY_BUTTON | TDCBF_CANCEL_BUTTON);
		errorTaskDialog.SetMainInstructionText(IDS_DISMOUNT_FAILURE);
		errorTaskDialog.SetContentText(IDS_DISMOUNT_FAILURE_DESC);
		errorTaskDialog.SetDefaultButton(IDRETRY);

		int selected;
		HRESULT hr = errorTaskDialog.DoModal(NULL, &selected);

		if (FAILED(hr))
		{
			ATLTRACE("ErrorTaskDialog failed, hr=0x%X\n", hr);
			ATLASSERT(FALSE);
		}
		else if (selected == IDRETRY)
		{
			goto retry;
		}
	}

}

void 
CMainFrame::OnCmdDismountAllLogicalUnits(UINT wNotifyCode, int wID, HWND hWndCtl)
{
	int response = pTaskDialogVerify(
		m_hWnd,
		IDS_MAIN_TITLE,
		IDS_CONFIRM_UNMOUNT_ALL,
		IDS_CONFIRM_UNMOUNT_DESC,
		_T("DontConfirmUnmountAll"),
		TDCBF_YES_BUTTON | TDCBF_NO_BUTTON,
		IDNO, 
		IDYES);

	if (IDYES != response) 
	{
		return;
	}

	CDismountAllDialog dlg;
	dlg.DoModal(
		m_hWnd, 
		reinterpret_cast<LPARAM>(&m_NdasLogDevices));
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

	int response = pTaskDialogVerify(
		m_hWnd,
		IDS_MAIN_TITLE,
		static_cast<LPCTSTR>(strMessage),
		static_cast<LPCTSTR>(NULL),
		_T("DontConfirmUnregister"),
		TDCBF_YES_BUTTON | TDCBF_NO_BUTTON,
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
CMainFrame::OnCmdResetDevice(DWORD dwSlotNo)
{
	ndas::DevicePtr pDevice;
	if (!ndas::FindDeviceBySlotNumber(pDevice, dwSlotNo))
	{
		return;
	}

	if (!pDevice->Enable(FALSE)) 
	{
		ErrorMessageBox(IDS_ERROR_RESET_DEVICE);
		return;
	}

	::SetCursor(AtlLoadSysCursor(IDC_WAIT));

	CSimpleWaitDlg().DoModal(m_hWnd, 2000);

	::SetCursor(AtlLoadSysCursor(IDC_ARROW));

	if (!pDevice->Enable(TRUE)) 
	{
		ErrorMessageBox(IDS_ERROR_RESET_DEVICE);
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

//void 
//CMainFrame::OnNdasRawDiskDetected(
//	NDAS_LOGICALDEVICE_ID LogicalDeviceId)
//{
//	ATLTRACE("Logical Device does not have partitions.\n");
//
//	LONG dlgCount = InterlockedIncrement(&m_RawDiskEventCount);
//	ATLASSERT(dlgCount > 0);
//	if (1 == dlgCount)
//	{
//		TASKDIALOG_BUTTON buttons[] = {
//			IDOK, MAKEINTRESOURCE(IDS_LAUNCH_DISKMGMT),
//		};
//
//		TASKDIALOGCONFIG config = {0};
//		config.cbSize = sizeof(TASKDIALOGCONFIG);
//		config.hwndParent = m_hWnd;
//		config.hInstance = _AtlBaseModule.GetResourceInstance();
//		config.dwFlags = 
//			TDF_ALLOW_DIALOG_CANCELLATION |
//			TDF_USE_COMMAND_LINKS;
//		config.dwCommonButtons = TDCBF_CLOSE_BUTTON;
//		config.pszWindowTitle = MAKEINTRESOURCE(IDS_MAIN_TITLE);
//		config.pszMainIcon = TD_INFORMATION_ICON;
//		config.pszMainInstruction;
//		config.pszContent = MAKEINTRESOURCE(IDS_MOUNTED_DISK_WITH_NO_PARTITION);
//		config.cButtons = RTL_NUMBER_OF(buttons);
//		config.pButtons = buttons;
//		config.nDefaultButton = IDOK;
//
//		int selected;
//		BOOL verify;
//		HRESULT hr = xTaskDialogIndirect(&config, &selected, NULL, &verify);
//		if (FAILED(hr))
//		{
//			ATLTRACE("xTaskDialogIndirect failed, hr=0x%X\n", hr);
//		}
//		else
//		{
//			if (IDOK == selected)
//			{
//				int ret = PtrToLong(ShellExecute(
//					m_hWnd,
//					_T("open"),
//					_T("diskmgmt.msc"),
//					NULL,
//					NULL,
//					SW_SHOWNORMAL));
//				if (ret <= 32)
//				{
//					ATLTRACE("ShellExecute(diskmgmt.msc) failed, ret=%d\n", ret);
//				}
//			}
//		}
//	}
//	dlgCount = InterlockedDecrement(&m_RawDiskEventCount);
//	ATLASSERT(dlgCount >= 0);
//}

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
	else if(NDAS_DEVICE_ALARM_RECOVERED == AdapterStatus)
	{
		nFormatID = IDS_BT_ALARMED_RECOVERRED;
	}
	else if(NDAS_DEVICE_ALARM_RAID_FAILURE == AdapterStatus)
	{	
		nFormatID = IDS_BT_ALARMED_RAID_FAILURE;
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
CMainFrame::_ChangeTaskBarIcon(bool Normal)
{
	if (Normal)
	{
		ATLVERIFY(m_taskbarIcon.ChangeIcon(_CreateTaskbarIcon(true)));
		ATLVERIFY( m_taskbarIcon.SetToolTipText(IDR_TASKBAR) );
	}
	else
	{
		ATLVERIFY(m_taskbarIcon.ChangeIcon(_CreateTaskbarIcon(false)));
		ATLVERIFY( m_taskbarIcon.SetToolTipText(IDR_TASKBAR_FAIL) );
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

	_ChangeTaskBarIcon(false);
}

void 
CMainFrame::OnNdasServiceConnectConnected()
{
	ATLTRACE("NDAS Service connection established.\n");

	_ChangeTaskBarIcon(true);

	m_fPendingUpdateDeviceList = TRUE;
	m_fPendingUpdateMenuItem = TRUE;
}

void 
CMainFrame::OnNdasServiceTerminating()
{
	ATLTRACE("NDAS Service is terminating.\n");

	_ChangeTaskBarIcon(false);
}

void 
CMainFrame::OnNdasServiceRejectedSuspend()
{
	CString message = MAKEINTRESOURCE(IDS_SUSPEND_REJECTED);

	m_taskbarIcon.ShowBalloonToolTip(
		static_cast<LPCTSTR>(message), 
		IDS_MAIN_TITLE, 
		NIIF_WARNING,
		30 * 1000);
}

void
CMainFrame::OnTaskbarMenu(UINT)
{
	// if there are any pop-up windows, do not handle this!

	HWND hWnd = ::GetLastActivePopup(m_hWnd);
	if (hWnd != m_hWnd)
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

	BOOL updated = FALSE;
	if (m_fPendingUpdateDeviceList) 
	{
		updated = _UpdateDeviceList();
		m_fPendingUpdateDeviceList = FALSE;
	}

	if (m_fPendingUpdateMenuItem) 
	{
		_UpdateMenuItems();
		m_fPendingUpdateMenuItem = FALSE;
	}

	if (updated)
	{
		_ChangeTaskBarIcon(true);
	}

	HMENU hSubMenu = ::GetSubMenu(m_taskbarIcon.m_hMenu, 0); 
	ATLASSERT(::IsMenu(hSubMenu));
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
			ATLTRACE(_T("%d/%d: %s\n"), i+1, count, pEntry->Name);
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
		AtlTaskDialogEx(
			m_hWnd, 
			IDS_MAIN_TITLE,
			0U,
			IDS_ERROR_DUPLICATE_ENTRY,
			TDCBF_OK_BUTTON,
			TD_WARNING_ICON);

		return false;
	}

	CDeviceRenameDialog renameDlg;
	while (ndas::FindDeviceByName(pExistingDevice, lpszDeviceName))
	{
		int response = AtlTaskDialogEx(
			m_hWnd, 
			IDS_MAIN_TITLE,
			0U,
			IDS_ERROR_DUPLICATE_NAME,
			TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON,
			TD_WARNING_ICON);

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
	
	AtlTaskDialogEx(
		m_hWnd, 
		IDS_MAIN_TITLE,
		0U,
		static_cast<LPCTSTR>(strMessage),
		TDCBF_OK_BUTTON,
		TD_INFORMATION_ICON);

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
		// \x25CF - black circle
		strLine.Format(_T(" \x25CF %s\r\n"), pDevice->GetName());
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
