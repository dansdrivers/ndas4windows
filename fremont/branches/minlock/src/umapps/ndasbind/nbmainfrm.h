// nbmainfrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atlctrlw.h>
#include "nbtreeview.h"
#include "eventhandler.h"
#include "nbdev.h"
#include "singleinst.h"
#include <map>

class CMainFrame;

typedef struct _MAINFRAME_CONTEXT{
	CMainFrame *pMainFrame;
//	CNBNdasDevice *pNBNdasDevice;
	void *pParameter;
} MAINFRAME_CONTEXT, *PMAINFRAME_CONTEXT;


class CMainFrame : 
	public CFrameWindowImpl<CMainFrame>, 
	public CNdasEventHandler<CMainFrame>,
	public CInterAppMsgImpl<CMainFrame>,
	public CUpdateUI<CMainFrame>,
	public CMessageFilter, 
	public CIdleHandler
{
	static BOOL	EnumDevicesCountCallBack(
		PNDASUSER_DEVICE_ENUM_ENTRY lpEnumEntry,
		LPVOID lpContext);
	static BOOL	EnumDevicesCallBack(
		PNDASUSER_DEVICE_ENUM_ENTRY lpEnumEntry,
		LPVOID lpContext);

protected:
	NBNdasDevicePtrList m_listDevices;
	NBLogicalDevicePtrList m_listLogicalDevices;
	NBNdasDevicePtrList m_listMissingDevices;	// Unit devices to represent non usable RAID members

	HNDASEVENTCALLBACK m_hEventCallback;
public:
	DECLARE_FRAME_WND_CLASS(NULL, IDR_MAINFRAME)

	CNBTreeListView m_viewTreeList;
	CCommandBarCtrl m_CmdBar;
	CToolBarCtrl m_wndToolBar;
	CMultiPaneStatusBarCtrl m_wndStatusBar;
	CProgressBarCtrl m_wndRefreshProgress;

	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual BOOL OnIdle();

	BEGIN_UPDATE_UI_MAP(CMainFrame)
//		UPDATE_ELEMENT(IDM_TOOL_BIND, UPDUI_MENUPOPUP | UPDUI_TOOLBAR) // Always selectable.
		UPDATE_ELEMENT(IDM_TOOL_UNBIND, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(IDM_TOOL_ADDMIRROR, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(IDM_TOOL_MIGRATE, UPDUI_MENUPOPUP | UPDUI_TOOLBAR )
		UPDATE_ELEMENT(IDM_TOOL_REPLACE_DEVICE, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(IDM_TOOL_USE_AS_MEMBER, UPDUI_MENUPOPUP | UPDUI_TOOLBAR )
//		UPDATE_ELEMENT(IDM_TOOL_SINGLE, UPDUI_MENUPOPUP | UPDUI_TOOLBAR )
		UPDATE_ELEMENT(IDM_TOOL_APPEND, UPDUI_MENUPOPUP | UPDUI_TOOLBAR )
		UPDATE_ELEMENT(IDM_TOOL_SPAREADD, UPDUI_MENUPOPUP | UPDUI_TOOLBAR )
		UPDATE_ELEMENT(IDM_TOOL_FIX_RAID_STATE, UPDUI_MENUPOPUP | UPDUI_TOOLBAR )
//		UPDATE_ELEMENT(IDM_TOOL_SPAREREMOVE, UPDUI_MENUPOPUP | UPDUI_TOOLBAR )
		UPDATE_ELEMENT(IDM_TOOL_RESET_BIND_INFO, UPDUI_MENUPOPUP | UPDUI_TOOLBAR )
		UPDATE_ELEMENT(IDM_TOOL_CLEAR_DEFECTIVE, UPDUI_MENUPOPUP | UPDUI_TOOLBAR )
		UPDATE_ELEMENT(IDM_TOOL_REMOVE_FROM_RAID, UPDUI_MENUPOPUP | UPDUI_TOOLBAR )
		UPDATE_ELEMENT(IDM_TOOL_USE_AS_BASIC, UPDUI_MENUPOPUP | UPDUI_TOOLBAR )
	END_UPDATE_UI_MAP()

	BEGIN_MSG_MAP_EX(CMainFrame)
		MSG_WM_CREATE(OnCreate)
		MSG_WM_DESTROY(OnDestroy)
		MSG_WM_CONTEXTMENU(OnContextMenu)
		COMMAND_ID_HANDLER(IDM_EXIT, OnExit)
		COMMAND_ID_HANDLER(IDM_HELP_ABOUT, OnAppAbout)
		COMMAND_ID_HANDLER_EX(IDM_TOOL_BIND, OnBind)
		COMMAND_ID_HANDLER_EX(IDM_TOOL_UNBIND, OnUnBind)
//		COMMAND_ID_HANDLER_EX(IDM_TOOL_SINGLE, OnSingle)
		COMMAND_ID_HANDLER_EX(IDM_TOOL_ADDMIRROR, OnAddMirror)
		COMMAND_ID_HANDLER_EX(IDM_TOOL_MIRROR, OnToolBarClickMirror)
		COMMAND_ID_HANDLER_EX(IDM_TOOL_REFRESH, OnRefreshStatus)
		COMMAND_ID_HANDLER_EX(IDM_TOOL_SYNCHRONIZE, OnCommand)
		COMMAND_ID_HANDLER_EX(IDM_TOOL_PROPERTY, OnCommand)
		COMMAND_ID_HANDLER_EX(IDM_TOOL_CLEARDIB, OnCommand)
		COMMAND_ID_HANDLER_EX(IDM_TOOL_MIGRATE, OnMigrate)
		COMMAND_ID_HANDLER_EX(IDM_TOOL_APPEND, OnAppend)
		COMMAND_ID_HANDLER_EX(IDM_TOOL_SPAREADD, OnSpareAdd)
//		COMMAND_ID_HANDLER_EX(IDM_TOOL_SPAREREMOVE, OnSpareRemove)
		COMMAND_ID_HANDLER_EX(IDM_TOOL_REPLACE_DEVICE, OnReplaceDevice)		
		COMMAND_ID_HANDLER_EX(IDM_TOOL_USE_AS_MEMBER, OnUseAsMember)	
		COMMAND_ID_HANDLER_EX(IDM_TOOL_RESET_BIND_INFO, OnResetBindInfo)
		COMMAND_ID_HANDLER_EX(IDM_TOOL_CLEAR_DEFECTIVE, OnClearDefect)
		COMMAND_ID_HANDLER_EX(IDM_TOOL_REMOVE_FROM_RAID, OnRemoveFromRaid)
		COMMAND_ID_HANDLER_EX(IDM_TOOL_USE_AS_BASIC, OnUseAsBasic)
		COMMAND_ID_HANDLER_EX(IDM_TOOL_FIX_RAID_STATE, OnFixRaidState)

		NOTIFY_CODE_HANDLER_EX(TVN_SELCHANGED, OnTreeSelChanged)
		NOTIFY_CODE_HANDLER_EX(TBN_DROPDOWN, OnToolBarDropDown)
		CHAIN_MSG_MAP(CInterAppMsgImpl<CMainFrame>)
		CHAIN_MSG_MAP(CNdasEventHandler<CMainFrame>)
		CHAIN_MSG_MAP(CUpdateUI<CMainFrame>)
		CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrame>)
	END_MSG_MAP()

	CMainFrame();
	~CMainFrame();

	LRESULT OnCreate(LPCREATESTRUCT /*lParam*/);
	VOID OnDestroy();
	LRESULT OnExit(WORD /*wNotifyCode*/, int /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAppAbout(WORD /*wNotifyCode*/, int /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	void OnContextMenu(HWND hWnd, CPoint point);
	LRESULT OnTreeSelChanged(LPNMHDR lpNMHDR);
	// When the user click the drop button on the toolbar
	LRESULT OnToolBarDropDown(LPNMHDR lpNMHDR);
	// When the user click the toolbar button on the toolbar
	LRESULT OnToolBarClickMirror(UINT wNotifyCode, int wID, HWND hwndCtl);

	void ClearDevices();

	BOOL CheckCommandAvailable(CNBDevice *pDevice, int wID);
	
	BOOL RefreshStatus();
	NBUnitDevicePtrList GetOperatableSingleDevices();

	// Command dispatchers
	void OnBind(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnUnBind(UINT wNotifyCode, int wID, HWND hwndCtl);
//	void OnSingle(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnMigrate(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnAddMirror(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnRefreshStatus(UINT wNotifyCode, int wID, HWND hwndCtl);
	
	void OnAppend(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnSpareAdd(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnSpareRemove(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnReplaceDevice(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnUseAsMember(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnResetBindInfo(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnClearDefect(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnUseAsBasic(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnRemoveFromRaid(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnFixRaidState(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnCommand(UINT wNotifyCode, int wID, HWND hwndCtl);

	void UIEnableForDevice(CNBDevice* pDevice, UINT nMenuID);

	void OnNdasDevEntryChanged();
	void OnNdasLogDevEntryChanged();
//	VOID OnNdasDevStatusChanged(DWORD dwSlotNo);
//	VOID OnNdasLogDevStatusChanged(const NDAS_LOGICALDEVICE_ID& logDevId);

	//
	// InterAppMsg
	//
	void OnInterAppMsg(WPARAM wParam, LPARAM lParam);

	//
	// Threads related
	//
	HANDLE m_hEventThread;
	LONG m_nAssigned;
	static unsigned WINAPI _ThreadInitializeNdasDevice(LPVOID Context)
	{
		MAINFRAME_CONTEXT *pThreadContext = (MAINFRAME_CONTEXT *)Context;
		CNBNdasDevice *pNBNdasDevice = (CNBNdasDevice *)pThreadContext->pParameter;

		unsigned exitcode = pThreadContext->pMainFrame->ThreadInitializeNdasDevice(pNBNdasDevice);

		free(pThreadContext);
		pThreadContext = NULL;

		return exitcode;
	}

	unsigned ThreadInitializeNdasDevice(CNBNdasDevice *pNBNdasDevice);

	static unsigned WINAPI _ThreadCreateNdasDevice(LPVOID Context)
	{
		MAINFRAME_CONTEXT *pThreadContext = (MAINFRAME_CONTEXT *)Context;
		CNBNdasDevice *pNBNdasDevice = (CNBNdasDevice *)pThreadContext->pParameter;

		unsigned exitcode = pThreadContext->pMainFrame->ThreadCreateNdasDevice(pNBNdasDevice);

		free(pThreadContext);
		pThreadContext = NULL;

		return exitcode;
	}

	unsigned ThreadCreateNdasDevice(CNBNdasDevice *pNBNdasDevice);
};
