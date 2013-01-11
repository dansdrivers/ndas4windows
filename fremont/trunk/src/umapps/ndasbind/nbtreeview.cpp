// nbtreeview.cpp : implementation of the CNBTreeView class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "nbmainfrm.h"
#include "nbtreeview.h"

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


const UINT ImageListIcons[] = { 
	IDI_DEVICE_FAIL, 
	IDI_DEVICE_BASIC, 
	IDI_DEVICE_BOUND,
	IDI_DEVICE_WARN,
	IDI_DEVICE_INVALID,
};

static COLUMN_INFO column_info[] = {	
	{ IDS_LISTVIEW_COL_NAME, 300 }, 
//	{ IDS_LISTVIEW_COL_ID, 180 }, 
	{ IDS_LISTVIEW_COL_SIZE, 60 }, 
	{ IDS_LISTVIEW_COL_STATUS, 100 }, 
	{ IDS_LISTVIEW_COL_RAID_STATUS, 120 },
	{ IDS_LISTVIEW_COL_COMMENT, 500}, 
};

LRESULT 
CNBTreeListView::OnCreate(LPCREATESTRUCT lpcs)
{
	//
	// Cache the password character
	//
	CEdit wnd;
	HWND hWnd = wnd.Create(m_hWnd, NULL, NULL, ES_PASSWORD);
	ATLASSERT(NULL != hWnd);
	m_chHidden = wnd.GetPasswordChar();
	BOOL fSuccess = wnd.DestroyWindow();
	ATLASSERT(fSuccess);

	// To call WM_CREATE message handler for CTreeListViewImpl
	SetMsgHandled(FALSE);
	return TRUE;
}


BOOL CNBTreeListView::Initialize()
{
	// Initialize Column
	HDITEM col = { 0 };
	col.mask = HDI_FORMAT | HDI_TEXT | HDI_WIDTH;
	col.fmt = HDF_LEFT;

//	SetExtendedListViewStyle ( LVS_EX_FULLROWSELECT);

	int i = 0;
	CString strHeader;

	for ( i=0; i < RTL_NUMBER_OF(column_info); i++ )
	{
		strHeader.LoadString( column_info[i].nColHeaderID );
		col.cxy = column_info[i].nWidth;
		col.pszText = strHeader.LockBuffer();
		GetHeaderControl().InsertItem(i, &col);
		CHeaderCtrl wndHeader = GetHeaderControl();
	}

	// Image List
	CImageList imageList;

	imageList.Create(32, 32, ILC_COLOR8 | ILC_MASK, RTL_NUMBER_OF(ImageListIcons), 1);
	for (size_t i = 0; i < RTL_NUMBER_OF(ImageListIcons); ++i)
	{
		HICON hIcon = ::LoadIcon(_Module.GetResourceInstance(), MAKEINTRESOURCE(ImageListIcons[i]) );
		// Uncomment this if you want 32x32 icon.
		// HICON hIcon = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(ImageListIcons[i]), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
		imageList.AddIcon( hIcon );
	}

	GetTreeControl().SetImageList( imageList, LVSIL_NORMAL);

	return TRUE;
}


CTreeItem CNBTreeListView::SetDevice(CTreeItem tiParent, CNBLogicalDev *LogicalDev)
{
	CTreeItem tiDevice;

	if (!LogicalDev) {

		// set empty device. Not happen anymore.
		
		CString strText;
		
		strText.LoadString(IDS_UNIDEV_TYPE_DISK_EMPTY);
		
		tiDevice = GetTreeControl().InsertItem( strText, 0, /*IDI_FAIL*/ 0, /*IDI_FAIL*/ tiParent, TVI_LAST );

		tiDevice.SetData((DWORD_PTR)NULL);

		return tiDevice;
	}

	tiDevice = GetTreeControl().InsertItem( CMainFrame::GetName(LogicalDev),
											LogicalDev->GetIconIndex((UINT *) ImageListIcons, RTL_NUMBER_OF(ImageListIcons)),
											LogicalDev->GetSelectIconIndex((UINT *)ImageListIcons, RTL_NUMBER_OF(ImageListIcons)),
											tiParent,
											TVI_LAST );

	tiDevice.SetData((DWORD_PTR)LogicalDev);

	// ID

	int col = 1;

	//	SetSubItemText(tiDevice, col, LogicalDev->GetIDString(m_chHidden));

	// Capacity
	//	col++;

	SetSubItemText( tiDevice, col, CMainFrame::GetCapacityString(LogicalDev->LogicalCapacityInByte()) );

	// Status

	col++;

	CString statusString;
	
	statusString.LoadString( LogicalDev->GetStatusId() );

	SetSubItemText( tiDevice, col, statusString );

	// Type

	col++;
	
	SetSubItemText( tiDevice, col, CMainFrame::GetRaidStatusString(LogicalDev) );

	// Fault Tolerance

	col++;
	
	SetSubItemText( tiDevice, col, CMainFrame::GetCommentString(LogicalDev) );

	return tiDevice;
}

void CNBTreeListView::SetDevices(NBLogicalDevPtrList *pListLogicalDevice)
{

	ATLASSERT(pListLogicalDevice);
	if(!pListLogicalDevice)
		return;

	CTreeItem tiLogicalDevice, tiUnitDevice;

	for(NBLogicalDevPtrList::iterator itLogicalDevice = pListLogicalDevice->begin();
		itLogicalDevice != pListLogicalDevice->end(); itLogicalDevice++)
	{
		// register logical device
		tiLogicalDevice = SetDevice(TVI_ROOT, *itLogicalDevice);
		
		if (!(*itLogicalDevice)->IsLeaf()) {

			// add children
			
			for (UINT32 i = 0; i < (*itLogicalDevice)->NumberOfChild(); i++) {

				tiUnitDevice = SetDevice(tiLogicalDevice, (*itLogicalDevice)->Child(i));
			}

			GetTreeControl().Expand(tiLogicalDevice, TVE_EXPAND);
		}
	}
}

CNBLogicalDev *CNBTreeListView::GetSelectedDevice()
{
	CTreeItem itemSelected = GetTreeControl().GetSelectedItem();
	if ( !itemSelected.IsNull() )
	{
		return (CNBLogicalDev *)itemSelected.GetData();
	}

	return NULL;
}

void 
CNBTreeListView::DrawTreeItem(LPNMTVCUSTOMDRAW lptvcd, UINT iState, const RECT& rcItem)
{
	CDCHandle dc = lptvcd->nmcd.hdc;
	HTREEITEM hItem = (HTREEITEM) lptvcd->nmcd.dwItemSpec;

	tMapItem* pVal = m_mapItems.Lookup(hItem);
	if( pVal == NULL ) return;

	// NOTE: Having an ImageList attached to the TreeView control seems
	//       to produce some extra WM_ERASEBKGND msgs, which we can use to
	//       optimize the painting...
	CImageList il = m_ctrlTree.GetImageList(TVSIL_NORMAL);

	// If the item had focus then draw it
	// NOTE: Only when images are used (see note above)
	// FIX-BY-PATRIA: DrawFocusRect should be done later
	//if( (iState & CDIS_FOCUS) != 0 && !il.IsNull() ) {
	//	RECT rcFocus = rcItem;
	//	rcFocus.left = 1;
	//	dc.SetTextColor(::GetSysColor(COLOR_BTNTEXT));
	//	dc.DrawFocusRect(&rcFocus);
	//}

	// If it's selected, paint the selection background
	if( (iState & CDIS_SELECTED)  && (iState & CDIS_FOCUS)) {
		RECT rcHigh = rcItem;
		dc.FillSolidRect(&rcHigh, ::GetSysColor(COLOR_HIGHLIGHT));
	}
	else if( il.IsNull() ) {
		RECT rcHigh = rcItem;
		dc.FillSolidRect(&rcHigh, lptvcd->clrTextBk);
	}

	// Always write text with background
	dc.SetBkMode(OPAQUE);
	dc.SetBkColor(::GetSysColor((iState & CDIS_SELECTED) != 0 ? COLOR_HIGHLIGHT : COLOR_WINDOW));

	// Draw all columns of the item
	RECT rc = rcItem;
	int cnt = pVal->GetSize();
	for( int i = 0; i < cnt; i++ ) {
		LPTLVITEM pItem = (*pVal)[i];
		ATLASSERT(pItem);

		if( i != 0 ) rc.left = m_rcColumns[i].left;
		rc.right = m_rcColumns[i].right;

		if( pItem->mask & TLVIF_IMAGE ) {
			ATLASSERT(!il.IsNull());
			int cx, cy;
			il.GetIconSize(cx, cy);
			il.DrawEx(
				pItem->iImage, 
				dc, 
				rc.left, rc.top, 
				MIN(cx, rc.right - rc.left), cy,
				CLR_NONE, CLR_NONE,
				ILD_TRANSPARENT);
			rc.left += cx;
		}

		if( pItem->mask & TLVIF_TEXT ) {

			rc.left += 2;

			COLORREF clrText = lptvcd->clrText;
			if( pItem->mask & TLVIF_TEXTCOLOR ) clrText = pItem->clrText;
			if( (iState & CDIS_SELECTED) && (iState & CDIS_FOCUS) ) 
			{
				clrText = ::GetSysColor(COLOR_HIGHLIGHTTEXT);
			}
			dc.SetTextColor(clrText);

			CFont font;
			HFONT hOldFont = NULL;
			if( pItem->mask & TLVIF_STATE ) {
				LOGFONT lf;
				::GetObject(m_ctrlTree.GetFont(), sizeof(LOGFONT), &lf);
				if( pItem->state & TLVIS_BOLD ) lf.lfWeight += FW_BOLD - FW_NORMAL;
				if( pItem->state & TLVIS_ITALIC ) lf.lfItalic = TRUE;
				if( pItem->state & TLVIS_UNDERLINE ) lf.lfUnderline = TRUE;
				if( pItem->state & TLVIS_STRIKEOUT ) lf.lfStrikeOut = TRUE;
				font.CreateFontIndirect(&lf);
				ATLASSERT(!font.IsNull());
				hOldFont = dc.SelectFont(font);
			}

			UINT format = pItem->mask & TLVIF_FORMAT ? pItem->format : 0;

			if (0 == i)
			{
				CNBLogicalDev *pDevice = (CNBLogicalDev*) m_ctrlTree.GetItemData(hItem);
				if (NULL != pDevice && CMainFrame::GetIDString(pDevice, _T('*')).GetLength() > 0)
				{
					CString strBottom = CMainFrame::GetIDString(pDevice, m_chHidden);

					CRect rcTop = rc; rcTop.DeflateRect(0, 0, 0, rcTop.Height() / 2);
					CRect rcBottom = rc; rcBottom.top = rcTop.bottom;
					
					dc.FillSolidRect(&rc, lptvcd->clrTextBk);

					LOGFONT lf;
					CFontHandle fontHandle = dc.GetCurrentFont();
					fontHandle.GetLogFont(&lf);
					lf.lfWeight = FW_BOLD;
					CFont font; 
					font.CreateFontIndirect(&lf);
					ATLASSERT(!font.IsNull());

					HFONT hOldFont = dc.SelectFont(font);
					int oldBkMode = dc.SetBkMode(TRANSPARENT);
					dc.DrawText(pItem->pszText, -1, &rcTop, DT_VCENTER | DT_SINGLELINE | DT_WORD_ELLIPSIS | format);
					dc.SelectFont(hOldFont);

					// Trying to dim the text.
					// This causes a weird look when 
					//COLORREF clrText = dc.GetTextColor();
					//clrText = RGB(
					//	(GetRValue(clrText) + 0x40) & 0xFF, 
					//	(GetGValue(clrText) + 0x40) & 0xFF,
					//	(GetBValue(clrText) + 0x40) & 0xFF);
					//clrText = dc.SetTextColor(clrText);
					dc.DrawText(strBottom, -1, &rcBottom, DT_VCENTER | DT_SINGLELINE | DT_WORD_ELLIPSIS | format);
					//dc.SetTextColor(clrText);
					dc.SetBkMode(oldBkMode);
				}
				else
				{
					dc.FillSolidRect(&rc, lptvcd->clrTextBk);

					LOGFONT lf;
					CFontHandle fontHandle = dc.GetCurrentFont();
					fontHandle.GetLogFont(&lf);
					lf.lfWeight = FW_BOLD;
					CFont font; 
					font.CreateFontIndirect(&lf);
					ATLASSERT(!font.IsNull());
					HFONT hOldFont = dc.SelectFont(font);
					int oldBkMode = dc.SetBkMode(TRANSPARENT);
					dc.DrawText(pItem->pszText, 
						-1, 
						&rc, 
						DT_VCENTER | DT_SINGLELINE | DT_WORD_ELLIPSIS | format);
					dc.SetBkMode(oldBkMode);
					dc.SelectFont(hOldFont);
				}
			}
			else
			{
				UINT textFormat;
				size_t newlineindex = _tcscspn(pItem->pszText, _T("\n"));
				if (newlineindex == _tcslen(pItem->pszText)) {
					textFormat = DT_VCENTER | DT_SINGLELINE | DT_WORD_ELLIPSIS | format;
				} else {
					textFormat = DT_WORD_ELLIPSIS | format;
				}
				int oldBkMode = dc.SetBkMode(TRANSPARENT);
				dc.DrawText(pItem->pszText, 
					-1, 
					&rc, 
					textFormat);
				dc.SetBkMode(oldBkMode);
			}

			if( pItem->mask & TLVIF_STATE ) dc.SelectFont(hOldFont);
		}
	}
	// FIX-BY-PATRIA: DrawFocusRect should be done here
	if( (iState & CDIS_FOCUS) != 0 && !il.IsNull() ) {
		RECT rcFocus = rcItem;
		rcFocus.left = 1;
		dc.SetTextColor(::GetSysColor(COLOR_BTNTEXT));
		dc.DrawFocusRect(&rcFocus);
	}

}
