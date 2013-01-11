// nbtreeview.h : interface of the CNBTreeView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once
#include <stack>
#include <map>
#include "ndasobject.h"

class CNBTreeView : 
				public CWindowImpl<CNBTreeView, CTreeViewCtrlEx>,
				public CDiskObjectVisitorT<TRUE>
{
private:
	static const UINT NDASBINDVIEW_INSERT_OBJECT = 1;
	static const UINT NDASBINDVIEW_UPDATE_OBJECT = 2;
	UINT m_nAction;

	std::stack<HTREEITEM> m_htiParents;
	CTreeItem m_htiParent;
	CTreeItem m_htiLast;
	std::map<UINT, CTreeItem> m_mapIDToTreeItem;

	//
	// Methods for iterating disks
	// these methods are used to manipulate treectrl
	//
	virtual void Visit(CDiskObjectPtr o);
	virtual void IncDepth();
	virtual void DecDepth();
public:
	
	CNBTreeView();

	DECLARE_WND_SUPERCLASS(NULL, CTreeViewCtrl::GetWndClassName())

	BOOL PreTranslateMessage(MSG* pMsg);

	BEGIN_MSG_MAP_EX(CNBTreeView)
	END_MSG_MAP()


	// Delete a disk object from the tree
	// If the disk object has child objects, they will also be deleted
	void DeleteDiskObject(CDiskObjectPtr o);

	// Insert disk below the parent.
	void InsertDiskObject(CDiskObjectPtr o, CDiskObjectPtr parent = CDiskObjectPtr());

	// Update a disk object 
	// If the disk object has child objects, they will also be updated
	void UpdateDiskObject(CDiskObjectPtr o);

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
};
