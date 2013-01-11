////////////////////////////////////////////////////////////////////////////
//
// Implementation of CNBListViewCtrl class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <stack>
#include "resource.h"
#include "ndasutil.h"
#include "nbuihandler.h"
#include "nblistview.h"

//////////////////////////////////////////////////////////////////////////
// CNBListViewCtrl
//////////////////////////////////////////////////////////////////////////
void CNBListViewCtrl::InitColumn()
{
	// TODO : String resource
	typedef struct _COLUMN_INFO {
		TCHAR	*szColHeading;
		UINT	nWidth;
	} COLUMN_INFO;
	static COLUMN_INFO column_info[] = {	{ _T("Name"), 120 }, 
	{ _T("ID"), 200 },
	{ _T("Size"), 100 } 
	};
	ATLASSERT( m_nColCount <= sizeof(column_info)/sizeof(column_info[0]) );

	UINT i, nColCount;
	// Clear all the column exist
	nColCount = CListViewCtrl::GetHeader().GetItemCount();
	for ( i=0; i < nColCount; i++ )
		CListViewCtrl::DeleteColumn(0);
	// Add new columns
	for ( i=0; i < m_nColCount; i++ )
	{
		CListViewCtrl::InsertColumn(
			i, column_info[i].szColHeading, LVCFMT_LEFT, 
			column_info[i].nWidth, -1 );
	}
}
void CNBListViewCtrl::AddDiskObject(CDiskObjectPtr o)
{
	LVITEM lvItem = { 0 };

	lvItem.mask		= LVIF_TEXT | LVIF_PARAM;
	lvItem.iItem	= GetItemCount();
	lvItem.pszText	= LPSTR_TEXTCALLBACK;
	lvItem.lParam	= static_cast<LPARAM>(o->GetUniqueID());
	CListViewCtrl::InsertItem( &lvItem );
	m_mapObject[o->GetUniqueID()] = o;
}
void CNBListViewCtrl::DeleteDiskObject(CDiskObjectPtr o)
{
	int nItemCount = CListViewCtrl::GetItemCount();
	for (int i=0; i < nItemCount; i++ )
	{
		if ( GetItemData(i) == o->GetUniqueID() )
		{
			CListViewCtrl::DeleteItem( i );
			break;
		}
	}
	m_mapObject.erase( o->GetUniqueID() );
}

void CNBListViewCtrl::AddDiskObjectList(CDiskObjectList disks)
{
	CDiskObjectList::iterator itr;
	for ( itr = disks.begin(); itr != disks.end(); ++itr )
		AddDiskObject( *itr );
	SortItems();
}

void CNBListViewCtrl::DeleteDiskObjectList(CDiskObjectList disks)
{
	CDiskObjectList::iterator itr;
	for ( itr = disks.begin(); itr != disks.end(); ++itr )
		DeleteDiskObject( *itr );
}

void CNBListViewCtrl::SelectDiskObject(CDiskObjectPtr o)
{
	int nItem;
	
	nItem = FindDiskObjectItem(o);
	if ( nItem != -1 )
	{
		CListViewCtrl::SetItemState( nItem, LVIS_SELECTED, LVIS_SELECTED );
	}
}

int CNBListViewCtrl::FindDiskObjectItem(CDiskObjectPtr o)
{
	LVFINDINFO info;

	info.flags = LVFI_PARAM;
	info.lParam = o->GetUniqueID();
	return CListViewCtrl::FindItem(&info, -1);
}
void CNBListViewCtrl::SelectDiskObjectList(CDiskObjectList disks)
{
	CDiskObjectList::iterator itr;
	for ( itr = disks.begin(); itr != disks.end(); ++itr )
		SelectDiskObject( *itr );

}

CDiskObjectList CNBListViewCtrl::GetSelectedDiskObjectList()
{
	int nCount = CListViewCtrl::GetSelectedCount();
	int nItem;
	CDiskObjectList selectDiskList;

	nItem = -1;
	for ( int i=0; i < nCount; i++ )
	{
		nItem = CListViewCtrl::GetNextItem( nItem, LVNI_SELECTED );
		selectDiskList.push_back( m_mapObject[CListViewCtrl::GetItemData(nItem)] );
	}

	return selectDiskList;
}

CDiskObjectList CNBListViewCtrl::GetDiskObjectList()
{
	int nCount = CListViewCtrl::GetItemCount();
	CDiskObjectList diskList;
	for ( int i=0; i < nCount; i++ )
	{
		diskList.push_back( m_mapObject[CListViewCtrl::GetItemData(i)] );
	}
	return diskList;
}

int CNBListViewCtrl::CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	UINT uid1, uid2;
	CDiskObjectPtr obj1, obj2;
	CNBListViewCtrl *_this = reinterpret_cast<CNBListViewCtrl*>(lParamSort);

	uid1 = static_cast<UINT>(lParam1);
	uid2 = static_cast<UINT>(lParam2);
	obj1 = _this->m_mapObject[uid1];
	obj2 = _this->m_mapObject[uid2];

	ATLASSERT( (obj1.get() != NULL) && (obj2.get() != NULL) );

	return _this->CompareItems(obj1, obj2);
}
int CNBListViewCtrl::CompareItems(CDiskObjectPtr obj1, CDiskObjectPtr obj2)
{
	int signAsc = m_abSortAsc[m_iColSort]? 1 : -1;

	switch(m_iColSort)
	{
	case 0:
		return signAsc * obj1->GetTitle().Compare( obj2->GetTitle() );
		break;
	case 1:
		return signAsc * obj1->GetStringDeviceID().Compare( obj2->GetStringDeviceID() );
		break;
	case 2:
		{
			_int64 size1, size2;
			size1 = obj1->GetUserSectorCount();
			size2 = obj2->GetUserSectorCount();
			if ( size1 > size2 )
				return signAsc;
			else if ( size1 == size2 )
				return 0;
			else
				return (-1) * signAsc;
		}
		break;
	default:
		break;
	}
	return 0;
}

void CNBListViewCtrl::SortItems()
{
	CListViewCtrl::SortItems( CompareFunc, reinterpret_cast<LPARAM>(this) );
}

LPARAM CNBListViewCtrl::OnGetDispInfo(LPNMHDR lParam)
{
	NMLVDISPINFO *pDispInfo = reinterpret_cast<NMLVDISPINFO*>(lParam);
	CDiskObjectPtr obj;
	const CObjectUIHandler *phandler;

	obj = m_mapObject[static_cast<UINT>(pDispInfo->item.lParam)];
	ATLASSERT( obj.get() != NULL );

	phandler = CObjectUIHandler::GetUIHandler( obj.get() );

	pDispInfo->item.mask |= LVIF_DI_SETITEM;
	// TODO : String resources
	switch ( pDispInfo->item.iSubItem )
	{
	case 0:	// Name
		::_tcsncpy( 
			pDispInfo->item.pszText,  
			obj->GetTitle(),
			pDispInfo->item.cchTextMax-1
			);
		break;
	case 1:	// ID
		::_tcsncpy(
			pDispInfo->item.pszText,  
			phandler->GetStringID( obj.get() ),
			pDispInfo->item.cchTextMax-1
			);
		break;
	case 2: // Size
		::_sntprintf(
			pDispInfo->item.pszText,
			pDispInfo->item.cchTextMax-1,
			_T("%3d.%02dGB"), 
			phandler->GetSizeInMB( obj.get() ) / 1024,
			(phandler->GetSizeInMB( obj.get() ) % 1024) / 10
			);
	default:
		break;
	}
	pDispInfo->item.pszText[pDispInfo->item.cchTextMax-1] = '\0';
	return 0;
}

LRESULT CNBListViewCtrl::OnColumnClick(LPNMHDR lParam)
{
	NMLISTVIEW *pClickInfo = reinterpret_cast<NMLISTVIEW*>(lParam);

	m_iColSort = pClickInfo->iSubItem;
	m_abSortAsc[m_iColSort] = !m_abSortAsc[m_iColSort];
	SortItems();
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// CNBBindListViewCtrl
//////////////////////////////////////////////////////////////////////////
void CNBBindListViewCtrl::InitColumn()
{
	// TODO : String resource
	typedef struct _COLUMN_INFO {
		TCHAR	*szColHeading;
		UINT	nWidth;
	} COLUMN_INFO;
	static COLUMN_INFO column_info[] = {	
		//{ _T("Index"), 45 },
		{ _T(""), 20 },
		{ _T("Name"), 120 }, 
		{ _T("ID"), 200 },
		{ _T("Size"), 100 } 
	};
	ATLASSERT( m_nColCount <= sizeof(column_info)/sizeof(column_info[0]) );

	UINT i, nColCount;
	// Clear all the column exist
	nColCount = CListViewCtrl::GetHeader().GetItemCount();
	for ( i=0; i < nColCount; i++ )
		CListViewCtrl::DeleteColumn(0);
	// Add new columns
	for ( i=0; i < m_nColCount; i++ )
	{
		CListViewCtrl::InsertColumn(
			i, column_info[i].szColHeading, LVCFMT_LEFT, 
			column_info[i].nWidth, -1 );
	}
}

LRESULT CNBBindListViewCtrl::OnGetDispInfo(LPNMHDR lParam)
{
	NMLVDISPINFO *pDispInfo = reinterpret_cast<NMLVDISPINFO*>(lParam);
	CDiskObjectPtr obj;
	const CObjectUIHandler *phandler;

	obj = m_mapObject[static_cast<UINT>(pDispInfo->item.lParam)];
	ATLASSERT( obj.get() != NULL );

	phandler = CObjectUIHandler::GetUIHandler( obj.get() );

	// TODO : String resources
	switch ( pDispInfo->item.iSubItem )
	{
	case 0: // Index
		::_stprintf(
			pDispInfo->item.pszText,
			_T("%d"), 
			pDispInfo->item.iItem
			);
		break;
	case 1:	// Name
		::_tcsncpy( 
			pDispInfo->item.pszText,  
			obj->GetTitle(),
			pDispInfo->item.cchTextMax-1
			);
		break;
	case 2:	// ID
		::_tcsncpy(
			pDispInfo->item.pszText,  
			phandler->GetStringID( obj.get() ),
			pDispInfo->item.cchTextMax-1
			);
		break;
	case 3: // Size
		::_sntprintf(
			pDispInfo->item.pszText,
			pDispInfo->item.cchTextMax-1,
			_T("%3d.%02dGB"), 
			phandler->GetSizeInMB( obj.get() ) / 1024,
			(phandler->GetSizeInMB( obj.get() ) % 1024) / 10
			);
	default:
		break;
	}
	pDispInfo->item.pszText[pDispInfo->item.cchTextMax-1] = '\0';
	return 0;
}

int  CNBBindListViewCtrl::CompareItems(CDiskObjectPtr obj1, CDiskObjectPtr obj2)
{
	// No sorting is supported
	return 0;
}

void CNBBindListViewCtrl::MoveSelectedItemUp()
{
	int nCount = CListViewCtrl::GetSelectedCount();
	int nItem;
	std::list<int> listItem;
	LVITEM lvItem = { 0 };
	CDiskObjectPtr selectedDisk;
	if ( !IsItemMovable(TRUE) )
		return;
	
	nItem = -1;
	lvItem.mask		= LVIF_TEXT | LVIF_PARAM | LVIF_STATE;
	lvItem.pszText	= LPSTR_TEXTCALLBACK;
	for ( int i=0; i < nCount; i++ )
	{
		// Swap lParam value of two items
		nItem = CListViewCtrl::GetNextItem( nItem, LVNI_SELECTED );
		LPARAM lparam = CListViewCtrl::GetItemData(nItem);
		lvItem.iItem = nItem;
		lvItem.lParam = CListViewCtrl::GetItemData( nItem-1 );
		lvItem.state = 0;
		lvItem.stateMask = LVNI_SELECTED;
		CListViewCtrl::SetItem( &lvItem );
		lvItem.iItem = nItem - 1;
		lvItem.lParam = lparam;
		lvItem.state = LVNI_SELECTED;
		lvItem.stateMask = LVNI_SELECTED;
		CListViewCtrl::SetItem( &lvItem );
		CListViewCtrl::RedrawItems( nItem-1, nItem );
	}
}

void CNBBindListViewCtrl::MoveSelectedItemDown()
{
	int nCount = CListViewCtrl::GetSelectedCount();
	int nItem;
	std::list<int> listItem;
	LVITEM lvItem = { 0 };
	CDiskObjectPtr selectedDisk;
	if ( !IsItemMovable(FALSE) )
		return;

	nItem = -1;
	for (int i=0; i < nCount; i++ )
	{
		nItem = CListViewCtrl::GetNextItem( nItem, LVNI_SELECTED);
		listItem.push_front(nItem);
	}
	lvItem.mask		= LVIF_TEXT | LVIF_PARAM | LVIF_STATE;
	lvItem.pszText	= LPSTR_TEXTCALLBACK;
	
	std::list<int>::iterator itr;
	for ( itr = listItem.begin(); itr != listItem.end(); ++itr )
	{
		// Swap lParam value of two items
		LPARAM lparam = CListViewCtrl::GetItemData(*itr);
		lvItem.iItem = (*itr);
		lvItem.lParam = CListViewCtrl::GetItemData( (*itr)+1 );
		lvItem.state = 0;
		lvItem.stateMask = LVNI_SELECTED;
		CListViewCtrl::SetItem( &lvItem );
		lvItem.iItem = (*itr) + 1;
		lvItem.lParam = lparam;
		lvItem.state = LVNI_SELECTED;
		lvItem.stateMask = LVNI_SELECTED;
		CListViewCtrl::SetItem( &lvItem );
		CListViewCtrl::RedrawItems( (*itr), (*itr)+1 );
	}	
}

BOOL CNBBindListViewCtrl::IsItemMovable(BOOL bUp)
{
	int nCount = CListViewCtrl::GetSelectedCount();
	if ( nCount == 0 ) return FALSE;

	int nItem = -1;
	if ( bUp )
	{
		nItem = CListViewCtrl::GetNextItem( nItem, LVNI_SELECTED );
		if ( nItem == -1 || nItem == 0 )
			return FALSE;
	}
	else
	{
		for ( int i=0; i < nCount; i++ )
			nItem = CListViewCtrl::GetNextItem( nItem, LVNI_SELECTED);	// Get last selected item
		if ( nItem == CListViewCtrl::GetItemCount()-1 )
			return FALSE;
	}

	return TRUE;
}

void CNBBindListViewCtrl::SetMaxItemCount(UINT nCount)
{
	m_nMaxCount = nCount;
}

void CCustomStaticCtrl::OnPaint(HDC /*wParam*/)
{
	CPaintDC dc(this->m_hWnd);
	COLORREF clrBg, clrTitle;
	COLORREF clrShadow, clrHighlight;
	clrBg		 = ::GetSysColor(COLOR_3DFACE);//RGB(192, 192, 192);
	clrHighlight = ::GetSysColor(COLOR_3DLIGHT);
	clrShadow	 = ::GetSysColor(COLOR_3DSHADOW);
	clrTitle = RGB(0, 0, 0);
	CRect rtClient;
	TCHAR szTitle[16];
	int nTitleLen;
	CPen penHighlight, penShadow, penOld;
	DWORD dwTextStyle = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
	dc.SetBkColor( clrBg );
	dc.SetTextColor( clrTitle );
	dc.SelectFont( GetParent().GetFont() ); // pja July 1, 2001
	GetClientRect( rtClient );

	dc.FillSolidRect( rtClient, clrBg );
	
	penHighlight.CreatePen( PS_SOLID, 1, clrHighlight );
	penShadow.CreatePen( PS_SOLID, 1, clrShadow );
	penOld = dc.SelectPen( penHighlight );
	dc.MoveTo( rtClient.TopLeft() );
	dc.LineTo( rtClient.right, 0 );
	dc.MoveTo( rtClient.TopLeft() );
	dc.LineTo( 0, rtClient.bottom );
	dc.SelectPen( penShadow );
	dc.MoveTo( rtClient.BottomRight() );
	dc.LineTo( 0, rtClient.bottom );
	dc.MoveTo( rtClient.BottomRight() );
	dc.LineTo( rtClient.right, 0 );
	dc.SelectPen( penOld );
	nTitleLen = GetWindowText( szTitle, sizeof(szTitle)/sizeof(szTitle[0]) );
	dc.DrawText( szTitle, nTitleLen, rtClient, dwTextStyle );
}
void CNBBindListViewCtrl::OnPaint(HDC /*wParam*/)
{
	CRect rtCol, rtHeader, rtSubItem;
	CHeaderCtrl header = GetHeader();
	header.GetWindowRect( rtHeader );
	ScreenToClient( rtHeader );
	header.GetItemRect( 0, rtCol) ;
	rtCol.OffsetRect( rtHeader.TopLeft() );

	while ( !m_vtRowHeader.empty() )
	{
		CCustomStaticCtrl *pWnd = m_vtRowHeader.front();
		m_vtRowHeader.pop_front();
		pWnd->DestroyWindow();
		delete pWnd;
	}

	// For vertical scroll.
	if ( GetItemCount() > 0 )
	{
		GetSubItemRect(0, 0, LVIR_BOUNDS, rtSubItem);
	}
	else
	{
		rtSubItem.top = rtCol.bottom;
	}

	for ( UINT i=0; i < m_nMaxCount; i++ )
	{
		CCustomStaticCtrl *pWnd;
		CStatic staticCtrl;
		pWnd = new CCustomStaticCtrl;
		CRect rtItem;
		static int nHeight = 14;
		rtItem.left = rtCol.left;
		rtItem.right = rtCol.right-2;
		rtItem.top = rtSubItem.top + i * nHeight;
		rtItem.bottom = rtItem.top + nHeight-1;
		
		if ( rtItem.top < rtCol.bottom )
			continue;
		WTL::CString strTitle;
		strTitle.Format(_T("%d"), i);
		staticCtrl.Create( m_hWnd, rtItem, strTitle,
			WS_CHILD | SS_CENTER | WS_DISABLED);
		staticCtrl.ShowWindow( SW_SHOW );
		pWnd->SubclassWindow( staticCtrl.m_hWnd );
		m_vtRowHeader.push_back( pWnd );
	}
	DefWindowProc();
}

CNBBindListViewCtrl::~CNBBindListViewCtrl()
{
	while ( !m_vtRowHeader.empty() )
	{
		delete m_vtRowHeader.front();
		m_vtRowHeader.pop_front();
	}
}
