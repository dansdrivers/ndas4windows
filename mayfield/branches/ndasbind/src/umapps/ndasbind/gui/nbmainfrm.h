// nbmainfrm.h : interface of the CMainFrame class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atlctrlw.h>
#include "ndasobject.h"
#include "nbtreeview.h"
#include "nblistview.h"
#include "eventhandler.h"

#include <map>
class CMainFrame : 
		public CFrameWindowImpl<CMainFrame>, 
		public CNdasEventHandler<CMainFrame>,
		public CUpdateUI<CMainFrame>,
		public CMessageFilter, 
		public CIdleHandler,
		public CDiskObjectVisitorT<TRUE>
{
protected:
	CRootDiskObjectPtr m_pRoot;
	std::map<UINT, CDiskObjectPtr> m_mapObject;

	HNDASEVENTCALLBACK m_hEventCallback;
	CRITICAL_SECTION m_csThreadRefreshStatus;
	BOOL m_bRefreshing;
public:
	DECLARE_FRAME_WND_CLASS(NULL, IDR_MAINFRAME)

	CHorSplitterWindow m_wndHorzSplit;
	CNBListView m_viewList;
	CNBTreeView m_viewTree;
	CNBTreeListView m_viewTreeList;
	CCommandBarCtrl m_CmdBar;
	CToolBarCtrl m_wndToolBar;
	CMultiPaneStatusBarCtrl m_wndStatusBar;
	CProgressBarCtrl m_wndRefreshProgress;

	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual BOOL OnIdle();

	BEGIN_UPDATE_UI_MAP(CMainFrame)
		UPDATE_ELEMENT(IDM_AGGR_BIND, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(IDM_AGGR_UNBIND, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(IDM_AGGR_SYNCHRONIZE, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(IDM_AGGR_ADDMIRROR, UPDUI_MENUPOPUP | UPDUI_TOOLBAR )
		UPDATE_ELEMENT(IDM_AGGR_MIGRATE, UPDUI_MENUPOPUP | UPDUI_TOOLBAR )
		UPDATE_ELEMENT(IDM_AGGR_SINGLE, UPDUI_MENUPOPUP | UPDUI_TOOLBAR )
		UPDATE_ELEMENT(IDM_AGGR_REPAIR, UPDUI_MENUPOPUP | UPDUI_TOOLBAR )
	END_UPDATE_UI_MAP()

	BEGIN_MSG_MAP_EX(CMainFrame)
		MSG_WM_CREATE(OnCreate)
		MSG_WM_DESTROY(OnDestroy)
		MSG_WM_CONTEXTMENU(OnContextMenu)
		COMMAND_ID_HANDLER(ID_APP_EXIT, OnExit)
		COMMAND_ID_HANDLER(IDM_EXIT, OnExit)
		COMMAND_ID_HANDLER(IDM_HELP_ABOUT, OnAppAbout)
		COMMAND_ID_HANDLER_EX(IDM_AGGR_BIND, OnBind)
		COMMAND_ID_HANDLER_EX(IDM_AGGR_UNBIND, OnUnBind)
		COMMAND_ID_HANDLER_EX(IDM_AGGR_SINGLE, OnSingle)
		COMMAND_ID_HANDLER_EX(IDM_AGGR_ADDMIRROR, OnAddMirror)
		COMMAND_ID_HANDLER_EX(IDM_AGGR_MIRROR, OnToolBarClickMirror)
		COMMAND_ID_HANDLER_EX(IDM_AGGR_REFRESH, OnRefreshStatus)
		COMMAND_ID_HANDLER_EX(IDM_AGGR_SYNCHRONIZE, OnCommand)
		COMMAND_ID_HANDLER_EX(IDM_AGGR_REPAIR, OnRepair)
		COMMAND_ID_HANDLER_EX(IDM_AGGR_PROPERTY, OnCommand)
		COMMAND_ID_HANDLER_EX(IDM_AGGR_CLEARDIB, OnCommand)
		COMMAND_ID_HANDLER_EX(IDM_AGGR_MIGRATE, OnCommand)
		NOTIFY_CODE_HANDLER_EX(TVN_SELCHANGED, OnTreeSelChanged)
		NOTIFY_CODE_HANDLER_EX(LVN_ITEMCHANGED, OnListItemChanged)
		NOTIFY_CODE_HANDLER_EX(TBN_DROPDOWN, OnToolBarDropDown)
		CHAIN_MSG_MAP(CNdasEventHandler<CMainFrame>)
		CHAIN_MSG_MAP(CUpdateUI<CMainFrame>)
		CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrame>)
	END_MSG_MAP()

	LRESULT OnCreate(LPCREATESTRUCT /*lParam*/);
	VOID OnDestroy();
	LRESULT OnExit(WORD /*wNotifyCode*/, int /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAppAbout(WORD /*wNotifyCode*/, int /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	void OnContextMenu(HWND hWnd, CPoint point);
	LRESULT OnTreeSelChanged(LPNMHDR lpNMHDR);
	LRESULT OnListItemChanged(LPNMHDR lpNMHDR);
	// When the user click the drop button on the toolbar
	LRESULT OnToolBarDropDown(LPNMHDR lpNMHDR);
	// When the user click the toolbar button on the toolbar
	LRESULT OnToolBarClickMirror(UINT wNotifyCode, int wID, HWND hwndCtl);

	virtual void Visit(CDiskObjectPtr o);
	void BuildObjectMap(CDiskObjectPtr o);
	void AddObjectToMap(CDiskObjectPtr o);
	void StartRefreshStatus();
	BOOL ThreadRefreshStatus();
	void ActivateUI(BOOL bActivate);
	void RefreshAction();

	// Command dispatchers
	void OnBind(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnUnBind(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnSingle(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnAddMirror(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnRepair(UINT wNotifyCode, int wID, HWND hwndCtl);	
	void OnRefreshStatus(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnCommand(UINT wNotifyCode, int wID, HWND hwndCtl);

	VOID OnNdasDevEntryChanged();
	VOID OnNdasLogDevEntryChanged();
};
