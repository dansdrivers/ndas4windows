// MainFrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once
#include <dde.h>
#include "ndasmgmt.h"
#include "ndascls.h"
#include "ndasdevice.h"
#include "nmmenu.h"
#include "menubitmap.h"
#include "eventhandler.h"
#include "taskbaricon.h"
#include "singleinst.h"
#include "importdlg.h"

typedef CWinTraits<
	WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 
	WS_EX_TOOLWINDOW> CMainFrameWinTraits;

class CMainFrame : 
	public CFrameWindowImpl<CMainFrame,CWindow,CMainFrameWinTraits>,
	public CNdasEventMessageMap<CMainFrame>,
	public CInterAppMsgImpl<CMainFrame>
{
	typedef CTaskbarIconT<CMainFrame> CTaskbarBase;

	enum BalloonType
	{
		UnknownBalloon,
		FirstRunBalloon,
		ExistingInstanceBalloon
	};

	const ndas::DeviceVector& m_NdasDevices;
	const ndas::LogicalDeviceVector& m_NdasLogDevices;

	CNdasTaskBarMenu m_taskbarMenu;
	CNdasMenuBitmapHandler m_bitmapHandler;
	// CWindow m_wndActivePopup;
	CMenuHandle m_hTaskbarDefaultMenu;

	bool m_fPendingUpdateDeviceList;
	bool m_fPendingUpdateMenuItem;
	bool m_fHotKeyRegistered;

	BalloonType m_curBalloonType;

	HICON m_hTaskbarIconBase;
	HICON m_hTaskbarFailOverlay;

	CTaskbarIcon m_taskbarIcon;
	CImageList m_taskbarImageList;

	int NDASMGMT_POPUP_HOTKEY;
	UINT WM_QUERYCANCELAUTOPLAY;

	//ATOM m_atDDEApp;
	//ATOM m_atDDEDefaultTopic;

	LONG m_RawDiskEventCount;

public:

	typedef CFrameWindowImpl<CMainFrame,CWindow,CMainFrameWinTraits> BaseFrameWindowImpl;

	BEGIN_MSG_MAP_EX(CMainFrame)

		MSG_WM_CREATE(OnCreate)
		MSG_WM_DESTROY(OnDestroy)
		MSG_WM_MENUCOMMAND(OnMenuCommand)
		MSG_WM_HOTKEY(OnHotKey)
		MSG_WM_PARENTNOTIFY(OnParentNotify)

#ifdef NDASMGMT_USE_QUERYCANCELAUTOPLAY
		MESSAGE_HANDLER_EX(WM_QUERYCANCELAUTOPLAY, OnQueryCancelAutoPlay);
#endif
		MESSAGE_HANDLER_EX(WM_DDE_INITIATE, OnDDEInitiate)
		MESSAGE_HANDLER_EX(WM_DDE_EXECUTE, OnDDEExecute)
		MESSAGE_HANDLER_EX(WM_DDE_TERMINATE, OnDDETerminate)

		COMMAND_ID_HANDLER_EX(ID_APP_EXIT, OnCmdFileExit)
		COMMAND_ID_HANDLER_EX(ID_APP_ABOUT, OnCmdAppAbout)
		COMMAND_ID_HANDLER_EX(IDR_APP_OPTION, OnCmdAppOptions)
		COMMAND_ID_HANDLER_EX(IDR_REFRESH_STATUS, OnCmdRefreshStatus)
		COMMAND_ID_HANDLER_EX(IDR_UNMOUNT_ALL, OnCmdDismountAllLogicalUnits)
		COMMAND_ID_HANDLER_EX(IDR_REGISTER_DEVICE, OnCmdRegisterDevice)

		TASKBAR_MESSAGE_HANDLER_EX(m_taskbarIcon, WM_LBUTTONDOWN, OnTaskbarMenu)
		TASKBAR_MESSAGE_HANDLER_EX(m_taskbarIcon, WM_RBUTTONDOWN, OnTaskbarMenu)
		TASKBAR_MESSAGE_HANDLER_EX(m_taskbarIcon, NIN_BALLOONUSERCLICK, OnBalloonUserClick)

		CHAIN_MSG_MAP_MEMBER(m_bitmapHandler)
		CHAIN_MSG_MAP(CInterAppMsgImpl<CMainFrame>)
		CHAIN_MSG_MAP_MEMBER(m_taskbarIcon)
		CHAIN_MSG_MAP(CNdasEventMessageMap<CMainFrame>)
		// The following line will emit an error during preprocessing
		// because of comma in template parameters
		// We aliased it as BaseFrameWindowImpl
		// CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrame,CWindow,CMainFrameWinTraits>)
		CHAIN_MSG_MAP(BaseFrameWindowImpl)

	END_MSG_MAP()

	CMainFrame();

	virtual void OnFinalMessage(HWND hWnd);

	LRESULT OnCreate(LPCREATESTRUCT lpcs);
	void OnDestroy();
	void OnInterAppMsg(WPARAM wParam, LPARAM lParam);

	LRESULT OnQueryCancelAutoPlay(UINT uMsg, WPARAM wParam, LPARAM lParam);

	void OnHotKey(int id, UINT fsModifier, UINT vk);
	void OnTaskbarMenu(UINT /*nMsg*/);
	void OnBalloonUserClick(UINT /*uMsg*/);
	void OnMenuCommand(WPARAM wParam, HMENU hMenu);
	void OnParentNotify(UINT nEvent, UINT nID, LPARAM lParam);
	LRESULT OnDDEInitiate(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnDDEExecute(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnDDETerminate(UINT uMsg, WPARAM wParam, LPARAM lParam);

	void OnCmdFileExit(UINT wNotifyCode, int wID, HWND hWndCtl);
	void OnCmdAppAbout(UINT wNotifyCode, int wID, HWND hWndCtl);
	void OnCmdAppOptions(UINT wNotifyCode, int wID, HWND hWndCtl);
	void OnCmdRegisterDevice(UINT wNotifyCode, int wID, HWND hWndCtl);
	void OnCmdRefreshStatus(UINT wNotifyCode, int wID, HWND hWndCtl);
	void OnCmdDismountAllLogicalUnits(UINT wNotifyCode, int wID, HWND hWndCtl);

	void OnCmdShowDeviceProperties(DWORD dwSlotNo);
	void OnCmdEnableDevice(DWORD dwSlotNo);
	void OnCmdDisableDevice(DWORD dwSlotNo);
	void OnCmdMountLogicalUnit(NDAS_LOGICALDEVICE_ID logDeviceId, BOOL WriteAccess);
	void OnCmdDismountLogicalUnit(NDAS_LOGICALDEVICE_ID logDeviceId);
	void OnCmdUnregisterDevice(DWORD dwSlotNo);
	void OnCmdResetDevice(DWORD dwSlotNo);

	void OnNdasDeviceEntryChanged();
	void OnNdasDeviceStatusChanged(
		DWORD dwSlotNo,
		NDAS_DEVICE_STATUS oldStatus,
		NDAS_DEVICE_STATUS newStatus);

	void OnNdasDevicePropChanged(DWORD dwSlotNo);
	void OnNdasUnitDevicePropChanged(DWORD dwSlotNo, DWORD dwUnitNo);
	void OnNdasLogDeviceEntryChanged();

	void OnNdasLogDeviceStatusChanged(
		NDAS_LOGICALDEVICE_ID logDevId,
		NDAS_LOGICALDEVICE_STATUS oldStatus,
		NDAS_LOGICALDEVICE_STATUS newStatus);

	void OnNdasLogDeviceDisconnected(NDAS_LOGICALDEVICE_ID logDevId);
	void OnNdasLogDeviceAlarmed(NDAS_LOGICALDEVICE_ID logDevId, ULONG AdapterStatus);
	void OnNdasLogDevicePropertyChanged(NDAS_LOGICALDEVICE_ID logDevId);
	void OnNdasLogDeviceRelationChanged(NDAS_LOGICALDEVICE_ID logDevId);

	void OnNdasSurrenderAccessRequest(
		DWORD dwSlotNo, 
		DWORD dwUnitNo, 
		UCHAR requestFlags, 
		LPCGUID requestHostGuid);

	void OnNdasServiceConnectRetry();
	void OnNdasServiceConnectFailed();
	void OnNdasServiceConnectConnected();
	void OnNdasServiceTerminating();
	void OnNdasServiceRejectedSuspend();

private:

	void _ShowWelcome();
	void _ShowInstPopup();
	HICON _CreateTaskbarIcon(bool NormalOrFailure = true);
	BOOL _UpdateDeviceList();
	void _UpdateMenuItems();
	void ErrorMessageBox(
		ATL::_U_STRINGorID Message, 
		ATL::_U_STRINGorID Title = IDS_ERROR_TITLE,
		DWORD ErrorCode = ::GetLastError());
	void _ImportRegistration(LPCTSTR szFileName);
	bool _ProcessRegistration(
		LPCTSTR lpszDeviceName,
		LPCTSTR lpszDeviceId,
		LPCTSTR lpszWriteKey);

	void _ChangeTaskBarIcon(bool Normal);

	int m_nTaskbarNormal;
	int m_nTaskbarFailureOverlay;

	CImportDlg m_wndImportDlg;

	static HRESULT pDismountTaskDialogCallback(
		HWND hWnd, UINT Notification, 
		WPARAM wParam, LPARAM lParam, 
		LONG_PTR RefData);
};
