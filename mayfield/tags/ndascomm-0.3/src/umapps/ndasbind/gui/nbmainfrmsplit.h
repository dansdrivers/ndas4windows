// nbmainfrmsplit.h : interface of the CMainFrameSplit class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ndasobject.h"
#include "nbtreeview.h"
#include "eventhandler.h"

#include <map>

class CMainFrameSplit : 
		public CFrameWindowImpl<CMainFrameSplit>, 
		public CNdasEventHandler<CMainFrameSplit>,
		public CUpdateUI<CMainFrameSplit>,
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
	CNBTreeView m_viewList;
	CNBTreeView m_viewTree;
	CToolBarCtrl m_wndToolBar;

	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual BOOL OnIdle();

	BEGIN_UPDATE_UI_MAP(CMainFrameSplit)
		UPDATE_ELEMENT(IDM_AGGR_BIND, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(IDM_AGGR_UNBIND, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(IDM_AGGR_SYNCHRONIZE, UPDUI_MENUPOPUP | UPDUI_TOOLBAR)
		UPDATE_ELEMENT(IDM_AGGR_ADDMIRROR, UPDUI_MENUPOPUP | UPDUI_TOOLBAR )
	END_UPDATE_UI_MAP()

	BEGIN_MSG_MAP_EX(CMainFrameSplit)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MSG_WM_DESTROY(OnDestroy)
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
		COMMAND_ID_HANDLER_EX(IDM_AGGR_PROPERTY, OnCommand)
		COMMAND_ID_HANDLER_EX(IDM_AGGR_CLEARDIB, OnCommand)
		MSG_WM_CONTEXTMENU(OnContextMenu)
		NOTIFY_CODE_HANDLER_EX(TVN_SELCHANGED, OnSelChanged)
		NOTIFY_CODE_HANDLER_EX(TBN_DROPDOWN, OnToolBarDropDown)
		CHAIN_MSG_MAP(CNdasEventHandler<CMainFrameSplit>)
		CHAIN_MSG_MAP(CUpdateUI<CMainFrameSplit>)
		CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrameSplit>)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	VOID OnDestroy();
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnExit(WORD /*wNotifyCode*/, int /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAppAbout(WORD /*wNotifyCode*/, int /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	void OnContextMenu(HWND hWnd, CPoint point);
	LRESULT OnSelChanged(LPNMHDR lpNMHDR);
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

	// Command dispatchers
	void OnBind(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnUnBind(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnSingle(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnAddMirror(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnRefreshStatus(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnCommand(UINT wNotifyCode, int wID, HWND hwndCtl);

	VOID OnNdasDevEntryChanged();
	VOID OnNdasLogDevEntryChanged();
};
