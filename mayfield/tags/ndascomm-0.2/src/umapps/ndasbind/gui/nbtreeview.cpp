// nbtreeview.cpp : implementation of the CNBTreeView class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "nbtreeview.h"
#include "nbuihandler.h"

CNBTreeListView::CNBTreeListView()
{
	m_nAction = NDASBINDVIEW_INSERT_OBJECT;
	m_htiParent = TVI_ROOT;
	m_htiLast = TVI_ROOT;
}

BOOL CNBTreeListView::PreTranslateMessage(MSG* pMsg)
{
	pMsg;
	return FALSE;
}

typedef struct _COLUMN_INFO {
	UINT nColHeaderID;
	UINT nWidth;
} COLUMN_INFO;

static COLUMN_INFO column_info[] = {	
	{ IDS_LISTVIEW_COL_NAME, 180 }, 
	{ IDS_LISTVIEW_COL_ID, 60 }, 
	{ IDS_LISTVIEW_COL_SIZE, 60 }, 
	{ IDS_LISTVIEW_COL_STATUS, 70 }, 
	{ IDS_LISTVIEW_COL_TYPE, 100 }, 
	{ IDS_LISTVIEW_COL_FAULT, 100 }, 
};

BOOL CNBTreeListView::Initialize()
{
//	DeleteAllItems();
//	while(DeleteColumn(1));

	HDITEM col = { 0 };
	col.mask = HDI_FORMAT | HDI_TEXT | HDI_WIDTH;
	col.fmt = HDF_LEFT;

//	SetExtendedListViewStyle ( LVS_EX_FULLROWSELECT);

	int i = 0;
	WTL::CString strHeader;

	for ( i=0; i < countof(column_info); i++ )
	{
		strHeader.LoadString( column_info[i].nColHeaderID );
		col.cxy = column_info[i].nWidth;
		col.pszText = strHeader.LockBuffer();
		GetHeaderControl().InsertItem(i, &col);
	}

	return TRUE;
}

void CNBTreeListView::Visit(CDiskObjectPtr o)
{
	BOOL bRet;
	const CObjectUIHandler *pHandler = CObjectUIHandler::GetUIHandler(o);

	switch ( m_nAction )
	{
	case NDASBINDVIEW_INSERT_OBJECT:
		{
			m_htiLast = GetTreeControl().InsertItem( 
				pHandler->GetTitle(o), 
				pHandler->GetIconIndex(o),
				pHandler->GetSelectedIconIndex(o),
				m_htiParent,
				TVI_LAST 
				);
			m_htiLast.SetData( o->GetUniqueID() );
			m_mapIDToTreeItem[o->GetUniqueID()] = m_htiLast;
			GetTreeControl().Expand(m_htiParent, TVE_EXPAND);
		}
		break;
	case NDASBINDVIEW_UPDATE_OBJECT:
		{
			CTreeItem htiUpdate = m_mapIDToTreeItem[o->GetUniqueID()];
			htiUpdate.SetText( pHandler->GetTitle(o) );
			htiUpdate.SetImage(
				pHandler->GetIconIndex(o),
				pHandler->GetSelectedIconIndex(o)
				);
		}
	}

	// update all columns
	WTL::CString strItemText;

	// ID
	int col = 1;
	if(o->IsUnitDisk())
	{
		SetSubItemText(m_htiLast, col, pHandler->GetStringID(o) );
	}


	// Capacity
	col++;
	strItemText.FormatMessage( 
		IDS_DISKPROPERTYPAGE_SIZE_IN_GB,
		pHandler->GetSizeInMB( o ) / 1000,
		(pHandler->GetSizeInMB( o ) % 1000) 
		);
	SetSubItemText(m_htiLast, col, strItemText );

	// Status
	col++;

	while(1)
	{
		// check disconnection
		if(0 != o->GetMissingMemberCount())
		{
			// disconnected status is not set ATM.
			strItemText.LoadString(IDS_STATUS_NOT_CONNECTED);
			SetSubItemText(m_htiLast, col, strItemText );
			break;
		}

		// check In use
		int nRW = 0, nRO = 0;
		bRet = o->GetUserCount(&nRW, &nRO);
		if(nRW || nRO)
		{
			strItemText.LoadString(IDS_STATUS_IN_USE);
			SetSubItemText(m_htiLast, col, strItemText );
			break;
		}

		// check write key
		if(!(o->GetAccessMask() & GENERIC_WRITE))
		{
			strItemText.LoadString(IDS_STATUS_READ_ONLY);
			SetSubItemText(m_htiLast, col, strItemText );
			break;
		}

		strItemText.LoadString(IDS_STATUS_FINE);
		SetSubItemText(m_htiLast, col, strItemText );
		break;
	}

	// Type
	col++;


	strItemText = pHandler->GetType(o);

	SetSubItemText(m_htiLast, col, strItemText );

	// Fault Tolerance
	col++;

	strItemText = pHandler->GetFaultTolerance(o);


	SetSubItemText(m_htiLast, col, strItemText );
};
void CNBTreeListView::IncDepth()
{ 
	m_htiParents.push( m_htiParent );
	m_htiParent = m_htiLast;
};
void CNBTreeListView::DecDepth()
{ 
	m_htiParent = m_htiParents.top();
	m_htiParents.pop();
};

void CNBTreeListView::DeleteDiskObject(CDiskObjectPtr o)
{
	CTreeItem htiDelete = m_mapIDToTreeItem[o->GetUniqueID()];

	GetTreeControl().DeleteItem( htiDelete );
}

void CNBTreeListView::InsertDiskObject(CDiskObjectPtr o, CDiskObjectPtr parent)
{
	if ( parent.get() == NULL )
	{
		m_htiParent = TVI_ROOT;
		m_htiLast = m_htiParent;
	}
	else 
	{
		m_htiParent = m_mapIDToTreeItem[parent->GetUniqueID()];
		if ( m_htiParent.IsNull() )
			m_htiParent = TVI_ROOT;
		m_htiLast = m_htiParent;
	}

	m_nAction = NDASBINDVIEW_INSERT_OBJECT;
	o->Accept( o, this );
}

void CNBTreeListView::UpdateDiskObject(CDiskObjectPtr o)
{
	m_nAction = NDASBINDVIEW_UPDATE_OBJECT;
	o->Accept( o, this );
}

void CNBTreeListView::SelectItemWithData(int itemData)
{
	HTREEITEM hItemWithData = GetItemWithData(itemData);

	if(hItemWithData)
		GetTreeControl().SelectItem(hItemWithData);
}

HTREEITEM CNBTreeListView::GetItemWithData(int itemData)
{
	HTREEITEM hNextItem;

	hNextItem = GetTreeControl().GetFirstVisibleItem();

	while(hNextItem)
	{
		if(itemData == (int)GetTreeControl().GetItemData(hNextItem))
			return hNextItem;

		hNextItem = GetTreeControl().GetNextVisibleItem(hNextItem);
	}

	return NULL;
}

int CNBTreeListView::GetSelectedItemData()
{

	CTreeItem itemSelected = GetTreeControl().GetSelectedItem();
	if ( !itemSelected.IsNull() )
	{
		return itemSelected.GetData();
	}

	return -1;
}

CNBTreeView::CNBTreeView()
{
	m_nAction = NDASBINDVIEW_INSERT_OBJECT;
	m_htiParent = TVI_ROOT;
	m_htiLast = TVI_ROOT;
}

BOOL CNBTreeView::PreTranslateMessage(MSG* pMsg)
{
	pMsg;
	return FALSE;
}

void CNBTreeView::Visit(CDiskObjectPtr o)
{
	const CObjectUIHandler *pHandler = CObjectUIHandler::GetUIHandler(o);

	switch ( m_nAction )
	{
	case NDASBINDVIEW_INSERT_OBJECT:
		{
			m_htiLast = CTreeViewCtrlEx::InsertItem( 
				pHandler->GetTitle(o), 
				pHandler->GetIconIndex(o),
				pHandler->GetSelectedIconIndex(o),
				m_htiParent,
				TVI_LAST 
				);
			m_htiLast.SetData( o->GetUniqueID() );
			m_mapIDToTreeItem[o->GetUniqueID()] = m_htiLast;
			Expand(m_htiParent, TVE_EXPAND);
		}
		break;
	case NDASBINDVIEW_UPDATE_OBJECT:
		{
			CTreeItem htiUpdate = m_mapIDToTreeItem[o->GetUniqueID()];
			htiUpdate.SetText( pHandler->GetTitle(o) );
			htiUpdate.SetImage(
				pHandler->GetIconIndex(o),
				pHandler->GetSelectedIconIndex(o)
				);
		}
	}
};
void CNBTreeView::IncDepth()
{ 
	m_htiParents.push( m_htiParent );
	m_htiParent = m_htiLast;
};
void CNBTreeView::DecDepth()
{ 
	m_htiParent = m_htiParents.top();
	m_htiParents.pop();
};

void CNBTreeView::DeleteDiskObject(CDiskObjectPtr o)
{
	CTreeItem htiDelete = m_mapIDToTreeItem[o->GetUniqueID()];

	CTreeViewCtrlEx::DeleteItem( htiDelete );
}

void CNBTreeView::InsertDiskObject(CDiskObjectPtr o, CDiskObjectPtr parent)
{
	if ( parent.get() == NULL )
	{
		m_htiParent = TVI_ROOT;
		m_htiLast = m_htiParent;
	}
	else 
	{
		m_htiParent = m_mapIDToTreeItem[parent->GetUniqueID()];
		if ( m_htiParent.IsNull() )
			m_htiParent = TVI_ROOT;
		m_htiLast = m_htiParent;
	}

	m_nAction = NDASBINDVIEW_INSERT_OBJECT;
	o->Accept( o, this );
}

void CNBTreeView::UpdateDiskObject(CDiskObjectPtr o)
{
	m_nAction = NDASBINDVIEW_UPDATE_OBJECT;
	o->Accept( o, this );
}

void CNBTreeView::SelectItemWithData(int itemData)
{
	HTREEITEM hItemWithData = GetItemWithData(itemData);


	if(hItemWithData)
		SelectItem(hItemWithData);
}

HTREEITEM CNBTreeView::GetItemWithData(int itemData)
{
	HTREEITEM hNextItem;

	hNextItem = GetFirstVisibleItem();

	while(hNextItem)
	{
		if(itemData == (int)GetItemData(hNextItem))
			return hNextItem;

		hNextItem = GetNextVisibleItem(hNextItem);
	}

	return NULL;
}

int CNBTreeView::GetSelectedItemData()
{
	CTreeItem itemSelected = GetSelectedItem();
	if ( !itemSelected.IsNull() )
	{
		return itemSelected.GetData();
	}

	return -1;
}
