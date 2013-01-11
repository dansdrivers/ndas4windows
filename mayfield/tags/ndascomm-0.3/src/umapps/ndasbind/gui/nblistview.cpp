// nblistview.cpp : implementation of the CNBListView class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "nblistview.h"
#include "nbuihandler.h"

CNBListView::CNBListView()
{
	m_nAction = NDASBINDVIEW_INSERT_OBJECT;
/*
	m_htiParent = TVI_ROOT;
	m_htiLast = TVI_ROOT;
*/
}

BOOL CNBListView::PreTranslateMessage(MSG* pMsg)
{
	pMsg;
	return FALSE;
}

/*
BOOL CNBListView::InsertNBDevice(CNBDevicePtr NBDevice)
{
	if(dynamic_cast<const CNBLogicalDevice *>(NBDevice.get()) != NULL)
	{		
		CNBLogicalDevicePtr NBLogicalDevice = 
			boost::dynamic_pointer_cast<CNBLogicalDevice>(NBDevice);

		return InsertNBLogicalDevice(NBLogicalDevice);
	}
	else if(dynamic_cast<const CNBUnitDevice *>(NBDevice.get()) != NULL)
	{
		CNBUnitDevicePtr NBUnitDevice =
			boost::dynamic_pointer_cast<CNBUnitDevice>(NBDevice);

		return InsertNBUnitDevice(NBUnitDevice);
	}

	return FALSE;
}

*/


typedef struct _COLUMN_INFO {
	UINT nColHeaderID;
	UINT nWidth;
} COLUMN_INFO;

static COLUMN_INFO column_info[] = {	
	{ IDS_LISTVIEW_COL_NAME, 120 }, 
	{ IDS_LISTVIEW_COL_ID, 60 }, 
	{ IDS_LISTVIEW_COL_SIZE, 60 }, 
	{ IDS_LISTVIEW_COL_STATUS, 70 }, 
	{ IDS_LISTVIEW_COL_TYPE, 100 }, 
	{ IDS_LISTVIEW_COL_FAULT, 100 }, 
};

BOOL CNBListView::Initialize()
{
	DeleteAllItems();
	while(DeleteColumn(1));

	SetExtendedListViewStyle ( LVS_EX_FULLROWSELECT);

	int i = 0;
	WTL::CString strHeader;

	for ( i=0; i < countof(column_info); i++ )
	{
		strHeader.LoadString( column_info[i].nColHeaderID );
		InsertColumn(
			i, strHeader, LVCFMT_LEFT, 
			column_info[i].nWidth, -1 );
	}

	return TRUE;
}

int CNBListView::FindListItem(CDiskObjectPtr o)
{
	LVFINDINFO lvfi;
	ZeroMemory(&lvfi, sizeof(LVFINDINFO));
	lvfi.flags = LVFI_PARAM;
	lvfi.lParam = o->GetUniqueID();

	return FindItem(&lvfi, -1);
}

void CNBListView::Visit(CDiskObjectPtr o)
{
	BOOL bRet;
	const CObjectUIHandler *pHandler = CObjectUIHandler::GetUIHandler(o);

	ATLTRACE(_T("CNBListView::Visit(CDiskObjectPtr o)\n"));

//	ATLASSERT(dynamic_cast<const CEmptyDiskObject*>(o.get()) == NULL );

	int idx;
	switch ( m_nAction )
	{
	case NDASBINDVIEW_INSERT_OBJECT:
		{
			idx = InsertItem(
				LVIF_TEXT|LVIF_IMAGE|LVIF_PARAM,
				GetItemCount(),
				pHandler->GetTitle(o),
				0,
				0,
				pHandler->GetIconIndex(o),
				o->GetUniqueID());
		}
		break;
	case NDASBINDVIEW_UPDATE_OBJECT:
		{
			idx = FindListItem(o);
			if(-1 == idx)
				break;

			SetItemText(idx, 0, pHandler->GetTitle(o));
//			SetItemImage(idx, 0, pHandler->GetTitle(o));
		}
	}

	if(-1 == idx)
		return;

	// update all columns
	WTL::CString strItemText;

	// ID
	int col = 1;
	if(o->IsUnitDisk())
	{
		SetItemText(idx, col, pHandler->GetStringID(o) );
	}


	// Capacity
	col++;
	strItemText.FormatMessage( 
		IDS_DISKPROPERTYPAGE_SIZE_IN_GB,
		pHandler->GetSizeInMB( o ) / 1024,
		(pHandler->GetSizeInMB( o ) % 1024) / 10 
		);
	SetItemText(idx, col, strItemText );

	// Status
	col++;

	while(1)
	{
		// check disconnection
		if(0 != o->GetMissingMemberCount())
		{
			// disconnected status is not set ATM.
			strItemText.LoadString(IDS_STATUS_NOT_CONNECTED);
			SetItemText(idx, col, strItemText );
			break;
		}

		// check In use
		int nRW = 0, nRO = 0;
		bRet = o->GetUserCount(&nRW, &nRO);
		if(nRW || nRO)
		{
			strItemText.LoadString(IDS_STATUS_IN_USE);
			SetItemText(idx, col, strItemText );
			break;
		}

		// check write key
		if(!(o->GetAccessMask() & GENERIC_WRITE))
		{
			strItemText.LoadString(IDS_STATUS_READ_ONLY);
			SetItemText(idx, col, strItemText );
			break;
		}

		strItemText.LoadString(IDS_STATUS_FINE);
		SetItemText(idx, col, strItemText );
		break;
	}

	// Type
	col++;


	strItemText = pHandler->GetType(o);

	SetItemText(idx, col, strItemText );

	// Fault Tolerance
	col++;

	strItemText = pHandler->GetFaultTolerance(o);


	SetItemText(idx, col, strItemText );
};
void CNBListView::IncDepth()
{ 
//	m_htiParents.push( m_htiParent );
//	m_htiParent = m_htiLast;
};
void CNBListView::DecDepth()
{ 
//	m_htiParent = m_htiParents.top();
//	m_htiParents.pop();
};

void CNBListView::DeleteDiskObject(CDiskObjectPtr o)
{
	DeleteItem(FindListItem(o));
}

void CNBListView::InsertDiskObject(CDiskObjectPtr o, CDiskObjectPtr parent)
{
	DeleteAllItems();

	m_nAction = NDASBINDVIEW_INSERT_OBJECT;
	o->Accept( o, this );
}

void CNBListView::UpdateDiskObject(CDiskObjectPtr o)
{
	m_nAction = NDASBINDVIEW_UPDATE_OBJECT;
	o->Accept( o, this );
}

void CNBListView::SelectDiskObject(CDiskObjectPtr o)
{
	int idx = FindListItem(o);
	
	if(-1 == idx)
		return;

	SelectItem(idx);	
}

INT CNBListView::GetSelectedItemData()
{
	int idx = GetSelectedIndex();

	if(-1 == idx)
		return -1;

	int itemData = GetItemData(idx);
	return itemData;
}