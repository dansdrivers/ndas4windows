// nblistview.h : interface of the CNBListView class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once
#include <stack>
#include <map>
#include "ndasobject.h"

class CNBListView :
	public CWindowImpl<CNBListView, CListViewCtrl>,
	public CDiskObjectVisitorT<TRUE>

{
private:
	static const UINT NDASBINDVIEW_INSERT_OBJECT = 1;
	static const UINT NDASBINDVIEW_UPDATE_OBJECT = 2;
	UINT m_nAction;

	int FindListItem(CDiskObjectPtr o);
	virtual void Visit(CDiskObjectPtr o);
	virtual void IncDepth();
	virtual void DecDepth();

public:
	
	CNBListView();

	DECLARE_WND_SUPERCLASS(NULL, CListViewCtrl::GetWndClassName())

	BOOL PreTranslateMessage(MSG* pMsg);

	BEGIN_MSG_MAP_EX(CNBListView)
	END_MSG_MAP()

	// Message handlers

	BOOL Initialize();

	// Delete a disk object from the tree
	// If the disk object has child objects, they will also be deleted
	void DeleteDiskObject(CDiskObjectPtr o);

	// Insert disk below the parent.
	void InsertDiskObject(CDiskObjectPtr o, CDiskObjectPtr parent = CDiskObjectPtr());

	// Update a disk object 
	// If the disk object has child objects, they will also be updated
	void UpdateDiskObject(CDiskObjectPtr o);

	void SelectDiskObject(CDiskObjectPtr o);
	INT GetSelectedItemData();
};
