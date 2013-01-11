// nbtreeview.h : interface of the CNBTreeView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once
#include "TreeListView.h"
#include <stack>
#include <map>
#include "nbdev.h"

class CNBTreeListView :
	public CTreeListViewImpl<CNBTreeListView>
{
	TCHAR m_chHidden;

public:
	
	CNBTreeListView();

	DECLARE_WND_SUPERCLASS(NULL, CTreeViewCtrl::GetWndClassName())

	BOOL PreTranslateMessage(MSG* pMsg);

	BEGIN_MSG_MAP_EX(CNBTreeListView)
		MSG_WM_CREATE(OnCreate)
		CHAIN_MSG_MAP(CTreeListViewImpl<CNBTreeListView>)
		ALT_MSG_MAP(1) // Tree Ctrl
		CHAIN_MSG_MAP_ALT(CTreeListViewImpl<CNBTreeListView>, 1)
		ALT_MSG_MAP(2) // List Ctrl
		CHAIN_MSG_MAP_ALT(CTreeListViewImpl<CNBTreeListView>, 2)
//		CHAIN_MSG_MAP_ALT(CContainedWindowT< CTreeViewCtrl >, 1)
//		CHAIN_MSG_MAP_ALT(CContainedWindowT< CHeaderCtrl >, 2)
	END_MSG_MAP()

	LRESULT OnCreate(LPCREATESTRUCT lpcs);

	CNBDevice *GetSelectedDevice();
	BOOL Initialize();
	void SetDevices(NBLogicalDevicePtrList *pListLogicalDevice);

	CTreeItem SetDevice(CTreeItem tiParent, CNBDevice *pDevice);

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)}

	void DrawTreeItem(LPNMTVCUSTOMDRAW lptvcd, UINT iState, const RECT& rcItem);

};
