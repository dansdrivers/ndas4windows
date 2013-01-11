// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once
#include "ndasmgmt.h"
#include "ndasdevice.h"
#include "nmmenu.h"
#include "menubitmap.h"
#include "eventhandler.h"
#include "taskbariconex.h"
#include "singleinst.h"

#define WM_APP_SHOW_EXISTING_INSTANCE (WM_APP + 0x401)

class CMainFrame : 
	public CFrameWindowImpl<CMainFrame>,
	public CNdasEventHandler<CMainFrame>,
	public CSingleInstWnd<CMainFrame>,
	public CTaskBarIconExT<CMainFrame>
{
	CNdasMenuBitmapHandler m_bitmapHandler;

	ndas::DeviceColl m_deviceColl;
	ndas::LogicalDeviceColl m_logDevColl;

	CWindow m_wndActivePopup;

	CRITICAL_SECTION m_csMenu;

	BOOL m_fPendingUpdateDeviceList;
	BOOL m_fPendingUpdateMenuItem;
	CMenuHandle m_hTaskBarDefaultMenu;

public:

	DECLARE_WND_CLASS(_T("NDASMGMT Window Class"))

	BEGIN_MSG_MAP_EX(CMainFrame)

		MSG_WM_CREATE(OnCreate)
		MSG_WM_DESTROY(OnDestroy)
		MSG_WM_SIZE(OnSize)
		MSG_WM_MENUCOMMAND(OnMenuCommand)
		MSG_WM_ENTERMENULOOP(OnEnterMenuLoop)
		MSG_WM_EXITMENULOOP(OnExitMenuLoop)

		COMMAND_ID_HANDLER_EX(ID_APP_EXIT, OnFileExit)
		COMMAND_ID_HANDLER_EX(ID_APP_ABOUT, OnAppAbout)
		COMMAND_ID_HANDLER_EX(IDR_APP_OPTION, OnAppOptions)
		COMMAND_ID_HANDLER_EX(IDR_REFRESH_STATUS, OnRefreshStatus)
		COMMAND_ID_HANDLER_EX(IDR_UNMOUNT_ALL, OnUnmountAll)
		COMMAND_ID_HANDLER_EX(IDR_REGISTER_DEVICE, OnRegisterDevice)

		TASKBAR_MESSAGE_HANDLER((*this), WM_LBUTTONUP, OnTaskBarMenu)
		TASKBAR_MESSAGE_HANDLER((*this), WM_RBUTTONUP, OnTaskBarMenu)

		CHAIN_MSG_MAP_MEMBER(m_bitmapHandler)
		CHAIN_MSG_MAP(CTaskBarIconExT<CMainFrame>)
		CHAIN_MSG_MAP(CNdasEventHandler<CMainFrame>)
		CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrame>)
		CHAIN_MSG_MAP(CSingleInstWnd<CMainFrame>)
	END_MSG_MAP()

	CMainFrame();

	VOID OnAnotherInstanceMessage(WPARAM wParam, LPARAM lParam);

	VOID ShowWelcome();
	VOID ShowInstPopup();

	LRESULT OnCreate(LPCREATESTRUCT lParam);
	LRESULT OnDestroy();
	LRESULT OnSize(UINT wParam, CSize size);

	LRESULT OnTaskBarMenu(LPARAM uMsg, BOOL& bHandled);
	VOID OnMenuCommand(WPARAM wParam, HMENU hMenu);

	VOID OnEnterMenuLoop(BOOL bIsTrackPopupMenu);
	VOID OnExitMenuLoop(BOOL bIsTrackPopupMenu);

	VOID OnFileExit(UINT wNotifyCode, int wID, HWND hWndCtl);
	VOID OnAppAbout(UINT wNotifyCode, int wID, HWND hWndCtl);
	VOID OnAppOptions(UINT wNotifyCode, int wID, HWND hWndCtl);
	VOID OnRegisterDevice(UINT wNotifyCode, int wID, HWND hWndCtl);
	VOID OnRefreshStatus(UINT wNotifyCode, int wID, HWND hWndCtl);
	VOID OnUnmountAll(UINT wNotifyCode, int wID, HWND hWndCtl);

	BOOL UpdateDeviceList();
	VOID UpdateMenuItems();

	VOID OnShowDeviceProperties(DWORD dwSlotNo);
	VOID OnEnableDevice(DWORD dwSlotNo);
	VOID OnDisableDevice(DWORD dwSlotNo);
	VOID OnMountDeviceRW(NDAS_LOGICALDEVICE_ID logDevId);
	VOID OnMountDeviceRO(NDAS_LOGICALDEVICE_ID logDevId);
	VOID OnUnmountDevice(NDAS_LOGICALDEVICE_ID logDevId);
	VOID OnUnregisterDevice(DWORD dwSlotNo);

	VOID OnNdasDevEntryChanged();
	VOID OnNdasDevStatusChanged(
		DWORD dwSlotNo,
		NDAS_DEVICE_STATUS oldStatus,
		NDAS_DEVICE_STATUS newStatus);

	VOID OnNdasDevPropChanged(DWORD dwSlotNo);
	VOID OnNdasUnitDevPropChanged(DWORD dwSlotNo, DWORD dwUnitNo);
	VOID OnNdasLogDevEntryChanged();

	VOID OnNdasLogDevStatusChanged(
		NDAS_LOGICALDEVICE_ID logDevId,
		NDAS_LOGICALDEVICE_STATUS oldStatus,
		NDAS_LOGICALDEVICE_STATUS newStatus);

	VOID OnNdasLogDevDisconnected(NDAS_LOGICALDEVICE_ID logDevId);
	VOID OnNdasLogDevReconnecting(NDAS_LOGICALDEVICE_ID logDevId);
	VOID OnNdasLogDevReconnected(NDAS_LOGICALDEVICE_ID logDevId);
	VOID OnNdasLogDevEmergency(NDAS_LOGICALDEVICE_ID logDevId);	
	VOID OnNdasLogDevAlarmed(NDAS_LOGICALDEVICE_ID logDevId, ULONG AdapterStatus);
	VOID OnNdasLogDevPropertyChanged(NDAS_LOGICALDEVICE_ID logDevId);
	VOID OnNdasLogDevRelationChanged(NDAS_LOGICALDEVICE_ID logDevId);

	VOID OnNdasSurrenderAccessRequest(
		DWORD dwSlotNo, 
		DWORD dwUnitNo, 
		UCHAR requestFlags, 
		LPCGUID requestHostGuid);

	VOID OnNdasServiceConnectRetry();
	VOID OnNdasServiceConnectFailed();
	VOID OnNdasServiceConnectConnected();
	VOID OnNdasServiceTerminating();

};
