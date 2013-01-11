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
	public CSingleInstWnd<CMainFrame>
{
	HNDASEVENTCALLBACK m_hEventCallback;

	CNdasMenuBitmapHandler m_bitmapHandler;

	ndas::DeviceColl m_deviceColl;
	ndas::LogicalDeviceColl m_logDevColl;

	CWindow m_wndActivePopup;

public:
	CTaskBarIconEx m_taskBarIcon;
	// CMyTaskBarIcon m_taskBarIcon;
	CMenuHandle m_hTaskBarDefaultMenu;

	BEGIN_MSG_MAP_EX(CMainFrame)

		MSG_WM_CREATE(OnCreate)
		MSG_WM_DESTROY(OnDestroy)
		MSG_WM_SIZE(OnSize)
		MSG_WM_MENUCOMMAND(OnMenuCommand)

//		MESSAGE_HANDLER_EX(WM_APP_SHOW_EXISTING_INSTANCE, OnShowExistInst)
		COMMAND_ID_HANDLER_EX(ID_APP_EXIT, OnFileExit)
		COMMAND_ID_HANDLER_EX(ID_APP_ABOUT, OnAppAbout)
		COMMAND_ID_HANDLER_EX(ID_APP_OPTION, OnAppOptions)
		COMMAND_ID_HANDLER_EX(ID_REFRESH_STATUS, OnRefreshStatus)

		COMMAND_ID_HANDLER_EX(ID_REGISTER_DEVICE, OnRegisterDevice)

		CHAIN_MSG_MAP_MEMBER(m_taskBarIcon)
		CHAIN_MSG_MAP_MEMBER(m_bitmapHandler)
		CHAIN_MSG_MAP(CNdasEventHandler<CMainFrame>)
		CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrame>)
		CHAIN_MSG_MAP(CSingleInstWnd<CMainFrame>)

	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	CMainFrame();

	void OnAnotherInstanceMessage(WPARAM wParam, LPARAM lParam)
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

	void ShowWelcome()
	{
		m_taskBarIcon.ShowBalloonToolTip(
			_T("NDAS Device Management program is running now.\n")
			_T("Click here to register a new NDAS Device to your system.\n"),
			_T("NDAS Device Management is running"),
			30 * 1000,
			NIIF_INFO);
	}

	void ShowInstPopup()
	{
		m_taskBarIcon.ShowBalloonToolTip(
			_T("NDAS Management is already running.\n")
			_T("Click the task bar icon to show the menu so that you can access features."),
			_T("NDAS Management is already running."),
			5000,
			NIIF_INFO);
	}

	LRESULT OnCreate(LPCREATESTRUCT lParam);
	LRESULT OnDestroy();
	LRESULT OnSize(UINT wParam, CSize size);
	void OnMenuCommand(WPARAM wParam, HMENU hMenu);

	void OnFileExit(UINT wNotifyCode, int wID, HWND hWndCtl);
	void OnAppAbout(UINT wNotifyCode, int wID, HWND hWndCtl);
	void OnAppOptions(UINT wNotifyCode, int wID, HWND hWndCtl);
	void OnRegisterDevice(UINT wNotifyCode, int wID, HWND hWndCtl);
	void OnRefreshStatus(UINT wNotifyCode, int wID, HWND hWndCtl);

	BOOL UpdateDeviceList();
	void UpdateMenuItems();

	void OnShowDeviceProperties(DWORD dwSlotNo);
	void OnEnableDevice(DWORD dwSlotNo);
	void OnDisableDevice(DWORD dwSlotNo);
	void OnMountDeviceRW(const NDAS_LOGICALDEVICE_ID& logDevId);
	void OnMountDeviceRO(const NDAS_LOGICALDEVICE_ID& logDevId);
	void OnUnmountDevice(const NDAS_LOGICALDEVICE_ID& logDevId);
	void OnUnregisterDevice(DWORD dwSlotNo);

	void OnNdasDevEntryChanged();
	void OnNdasDevStatusChanged(DWORD dwSlotNo);
	void OnNdasLogDevEntryChanged();
	void OnNdasLogDevStatusChanged(const NDAS_LOGICALDEVICE_ID& logDevId);

	void OnNdasLogDevDisconnected(const NDAS_LOGICALDEVICE_ID& logDevId);
	void OnNdasLogDevReconnecting(const NDAS_LOGICALDEVICE_ID& logDevId);
	void OnNdasLogDevReconnected(const NDAS_LOGICALDEVICE_ID& logDevId);

	//	void OnNdasServiceTerminating();
};
