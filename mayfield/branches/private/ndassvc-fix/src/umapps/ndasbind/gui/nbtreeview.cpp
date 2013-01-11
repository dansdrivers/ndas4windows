// nbtreeview.cpp : implementation of the CNBTreeView class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "nbtreeview.h"
#include <ndas/ndasop.h>

CNBTreeListView::CNBTreeListView()
{
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

const UINT anIconIDs[] = {
	IDI_FAIL,
	IDI_BASIC,
	IDI_AGGR ,
	IDI_RAID0,
	IDI_RAID1,
	IDI_RAID4,
	IDI_DVD,
	IDI_VDVD,
	IDI_FLASH,
	IDI_MO,
};


BOOL CNBTreeListView::Initialize()
{
	// Initialize Column
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


	// Image List
	CImageList imageList;
	//	imageList.Create(64, 32, ILC_COLOR8|ILC_MASK, sizeof(anIconIDs)/sizeof(anIconIDs[0]), 1);
	imageList.Create(32, 32, ILC_COLOR8|ILC_MASK, sizeof(anIconIDs)/sizeof(anIconIDs[0]), 1);
	for ( int i=0; i < sizeof(anIconIDs)/sizeof(anIconIDs[0]); i++ )
	{
		HICON hIcon = ::LoadIcon(_Module.GetResourceInstance(), MAKEINTRESOURCE(anIconIDs[i]) );
		// Uncomment this if you want 32x32 icon.
		// HICON hIcon = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(anIconIDs[i]), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
		imageList.AddIcon( hIcon );
	}

	GetTreeControl().SetImageList( imageList, LVSIL_NORMAL);

	return TRUE;
}

CTreeItem CNBTreeListView::SetDevice(CTreeItem tiParent, CNBDevice *pDevice)
{
	CTreeItem tiDevice;

	if(!pDevice)
	{
		// set empty device
		WTL::CString strText;
		strText.LoadString(IDS_UNIDEV_TYPE_DISK_EMPTY);
		tiDevice = GetTreeControl().InsertItem(
			strText,
			0, // IDI_FAIL
			0, // IDI_FAIL
			tiParent,
			TVI_LAST);

		tiDevice.SetData((DWORD_PTR)NULL);

		return tiDevice;
	}

	tiDevice = GetTreeControl().InsertItem(
		pDevice->GetName(),
		pDevice->GetIconIndex((UINT *)anIconIDs, sizeof(anIconIDs)/sizeof(anIconIDs[0])),
		pDevice->GetSelectIconIndex((UINT *)anIconIDs, sizeof(anIconIDs)/sizeof(anIconIDs[0])),
		tiParent,
		TVI_LAST);

	tiDevice.SetData((DWORD_PTR)pDevice);

	// ID
	int col = 1;

	SetSubItemText(tiDevice, col, pDevice->GetIDString());

	// Capacity
	col++;
	SetSubItemText(tiDevice, col, pDevice->GetCapacityString());

	// Status
	col++;
	SetSubItemText(tiDevice, col, pDevice->GetStatusString());

	// Type
	col++;
	SetSubItemText(tiDevice, col, pDevice->GetTypeString());

	// Fault Tolerance
	col++;
	SetSubItemText(tiDevice, col, pDevice->GetFaultToleranceString());

	return tiDevice;
}

void CNBTreeListView::SetDevices(NBLogicalDevicePtrList *pListLogicalDevice)
{
	ATLASSERT(pListLogicalDevice);
	if(!pListLogicalDevice)
		return;

	CTreeItem tiLogicalDevice, tiUnitDevice;

	for(NBLogicalDevicePtrList::iterator itLogicalDevice = pListLogicalDevice->begin();
		itLogicalDevice != pListLogicalDevice->end(); itLogicalDevice++)
	{
		// register logical device
		tiLogicalDevice = SetDevice(TVI_ROOT, *itLogicalDevice);
		if((*itLogicalDevice)->IsGroup())
		{
			// add children
			for(UINT32 i = 0; i < (*itLogicalDevice)->DevicesTotal(); i++)
			{
				tiUnitDevice = SetDevice(tiLogicalDevice, (*(*itLogicalDevice))[i]);
			}

			GetTreeControl().Expand(tiLogicalDevice, TVE_EXPAND);
		}
	}
}

CNBDevice *CNBTreeListView::GetSelectedDevice()
{
	CTreeItem itemSelected = GetTreeControl().GetSelectedItem();
	if ( !itemSelected.IsNull() )
	{
		return (CNBDevice *)itemSelected.GetData();
	}

	return NULL;
}

