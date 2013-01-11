////////////////////////////////////////////////////////////////////////////
//
// Implementation of CBindSheet class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "nbbindpages.h"
#include "nbbindsheet.h"
#include "nbdefine.h"
#include "ndasexception.h"


//////////////////////////////////////////////////////////////////////////
// Page 1
//////////////////////////////////////////////////////////////////////////
LRESULT CBindPage1::OnInitDialog(HWND /*hWndFocus*/, LPARAM /*lParam*/)
{
	DoDataExchange(FALSE);
	return 0;
}

BOOL CBindPage1::OnSetActive()
{
	GetParentSheet()->SetWizardButtons( PSWIZB_NEXT );
	return TRUE;
}
BOOL CBindPage1::OnKillActive()
{
	DoDataExchange(TRUE);
	CBindSheet *pSheet;
	pSheet = GetParentSheet();

	if ( m_nDiskCount < 2 )
	{
		// TODO : String resource
		MessageBox(
			_T("Number of disks must be larger than or equal to 2"),
			_T(PROGRAM_TITLE),
			MB_OK|MB_ICONWARNING
			);
		::SetFocus( GetDlgItem(IDC_EDIT_DISKCOUNT) );
		return FALSE;
	}
	if ( m_nDiskCount > pSheet->GetSingleDisks().size() )
	{
		// TODO : String resource
		WTL::CString strMsg;
		strMsg.Format( 
			_T("There are only %d disks available"), 
			pSheet->GetSingleDisks().size()
			);
		MessageBox( strMsg, _T(PROGRAM_TITLE), MB_OK|MB_ICONWARNING );
		::SetFocus( GetDlgItem(IDC_EDIT_DISKCOUNT) );
		return FALSE;
	}
	if ( m_nType == BIND_TYPE_RAID1 && (m_nDiskCount%2) != 0 )
	{
		// TODO : String resource
		MessageBox(
			_T("Number of disks must be even to use mirroring"),
			_T(PROGRAM_TITLE),
			MB_OK|MB_ICONWARNING
			);
		::SetFocus( GetDlgItem(IDC_EDIT_DISKCOUNT) );
		return FALSE;
	}

	pSheet->SetBindType(static_cast<UINT>(m_nType));
	pSheet->SetDiskCount( m_nDiskCount );
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
// Page 2
//////////////////////////////////////////////////////////////////////////
LRESULT CBindPage2::OnInitDialog(HWND /*hWndFocus*/, LPARAM /*lParam*/)
{
	m_btnAdd.SubclassWindow( GetDlgItem(IDC_BTN_ADD) );
	m_btnRemove.SubclassWindow( GetDlgItem(IDC_BTN_REMOVE) );
	m_btnUp.SubclassWindow( GetDlgItem(IDC_BTN_UP) );
	m_btnDown.SubclassWindow( GetDlgItem(IDC_BTN_DOWN) );

	m_wndDiskList.SubclassWindow( GetDlgItem(IDC_DISKLIST) );

	m_wndListSingle.SubclassWindow( GetDlgItem(IDC_LIST_SINGLE) );
	m_wndListBind.SubclassWindow( GetDlgItem(IDC_LIST_BIND) );
	DWORD dwStyle = LVS_EX_FULLROWSELECT; 
	//| LVS_EX_GRIDLINES 
	//| LVS_EX_INFOTIP 
	m_wndListSingle.SetExtendedListViewStyle( dwStyle );
	m_wndListBind.SetExtendedListViewStyle( dwStyle );
	m_wndListSingle.InitColumn();
	m_wndListBind.InitColumn();

	m_wndListSingle.AddDiskObjectList( 
		GetParentSheet()->GetSingleDisks()
		);
	m_imgDrag.Create(16, 16, ILC_COLOR8|ILC_MASK, 0, 1);
	CBitmapHandle bitmapDrag;
	bitmapDrag.LoadBitmap( IDB_DRAG );
	m_imgDrag.Add(bitmapDrag, bitmapDrag);
	return 0;
}

BOOL CBindPage2::OnKillActive()
{
	return TRUE;
}

BOOL CBindPage2::OnSetActive()
{
	GetParentSheet()->SetWizardButtons( PSWIZB_BACK | PSWIZB_DISABLEDFINISH );
	CBindSheet *pSheet = GetParentSheet();
	m_nDiskCount = pSheet->GetDiskCount();
	m_nType = pSheet->GetBindType();

	m_wndDiskList.SetMaxItemCount( m_nDiskCount );
	if ( m_nType == BIND_TYPE_RAID1 )
		m_wndDiskList.SetItemDepth(2);
	else
		m_wndDiskList.SetItemDepth(1);

	m_wndListBind.SetMaxItemCount( m_nDiskCount );
	// TODO : Change image as the type selected on the previous page.
	UpdateControls();
	return TRUE;
}
void CBindPage2::OnClickAddRemove(UINT /*wNotifyCode*/, int wID, HWND /*hwndCtl*/)
{
	MoveBetweenLists( (wID == IDC_BTN_ADD) );
}

void CBindPage2::OnClickMove(UINT /*wNotifyCode*/, int wID, HWND /*hwndCtl*/)
{
	switch( wID )
	{
	case IDC_BTN_UP:
		m_wndListBind.MoveSelectedItemUp();		
		break;
	case IDC_BTN_DOWN:
	default:
		m_wndListBind.MoveSelectedItemDown();
		break;
	}
}
BOOL CBindPage2::CanAddDisksToBindList(CDiskObjectList listDisks)
{
	return (m_wndListBind.GetItemCount() + listDisks.size()) <= (int)m_nDiskCount;
}
void CBindPage2::UpdateControls()
{
	GetDlgItem(IDC_BTN_ADD).EnableWindow( 
		CanAddDisksToBindList(m_wndListSingle.GetSelectedDiskObjectList())
		);
	GetDlgItem(IDC_BTN_REMOVE).EnableWindow( m_wndListBind.GetItemCount() > 0 );
	GetDlgItem(IDC_BTN_UP).EnableWindow( m_wndListBind.IsItemMovable(TRUE) );
	GetDlgItem(IDC_BTN_DOWN).EnableWindow( m_wndListBind.IsItemMovable(FALSE) );
	if ( m_wndListBind.GetDiskObjectList().size() == m_nDiskCount )
	{
		GetParentSheet()->SetWizardButtons( PSWIZB_BACK | PSWIZB_FINISH );
	}
	else
	{
		GetParentSheet()->SetWizardButtons( PSWIZB_BACK | PSWIZB_DISABLEDFINISH );
	}
}

void CBindPage2::MoveBetweenLists(BOOL bFromSingle)
{
	CNBListViewCtrl *pWndFrom, *pWndTo;
	if ( bFromSingle )
	{
		pWndFrom = &m_wndListSingle;
		pWndTo = &m_wndListBind;
	}
	else
	{
		pWndFrom = &m_wndListBind;
		pWndTo = &m_wndListSingle;
	}

	CDiskObjectList selectedDisks = pWndFrom->GetSelectedDiskObjectList();
	if ( selectedDisks.size() == 0 )
		return;
	pWndFrom->DeleteDiskObjectList( selectedDisks );
	pWndTo->AddDiskObjectList( selectedDisks );
	pWndTo->SelectDiskObjectList( selectedDisks );

	for ( UINT i=0; i < selectedDisks.size(); i++ )
	{
		if ( bFromSingle )
			m_wndDiskList.InsertItem();
		else
			m_wndDiskList.DeleteItem();
	}
	UpdateControls();	
}
LRESULT CBindPage2::OnListSelChanged(LPNMHDR /*lpNMHDR*/)
{
	//LPNMLISTVIEW pnmv = reinterpret_cast<LPNMLISTVIEW>(lpNMHDR);
	UpdateControls();
	return 0;
}

LRESULT CBindPage2::OnListBeginDrag(LPNMHDR lpNMHDR)
{
	LPNMLISTVIEW pnmv = reinterpret_cast<LPNMLISTVIEW>(lpNMHDR);
	CPoint ptOffset(-10, -10), ptAction(pnmv->ptAction);
	m_imgDrag.BeginDrag(0, ptOffset);
	m_imgDrag.DragEnter( ::GetDesktopWindow(), ptAction );

	m_bDragging = TRUE;
	SetCapture();

	if ( lpNMHDR->idFrom == IDC_LIST_SINGLE )
	{
		m_bDragFromBindList = FALSE;
		m_bPtInBindList = FALSE;
		m_listDrag = m_wndListSingle.GetSelectedDiskObjectList();
	}
	else
	{
		m_bDragFromBindList = TRUE;
		m_bPtInBindList = TRUE;
		m_listDrag = m_wndListBind.GetSelectedDiskObjectList();
	}

	return 0;
}

void CBindPage2::OnMouseMove(UINT /*nFlags*/, CPoint point)
{
	if ( m_bDragging )
	{
		ClientToScreen( &point );
		m_imgDrag.DragMove( point );
		// If the selected disks cannot be added to bind list,
		// change the icon to show that.
		if ( !m_bDragFromBindList && !CanAddDisksToBindList(m_listDrag) )
		{
			CRect rtWindow;
			CPoint ptOffset(-10, -10);
			m_wndListBind.GetWindowRect( rtWindow );
			if ( rtWindow.PtInRect(point) && !m_bPtInBindList )
			{
				m_imgDrag.DragShowNolock(FALSE);
				m_imgDrag.DragLeave( ::GetDesktopWindow() );
				m_imgDrag.EndDrag();
				m_imgDrag.BeginDrag(1, ptOffset);
				m_imgDrag.DragEnter( ::GetDesktopWindow(), point );
				m_bPtInBindList = TRUE;
				m_imgDrag.DragShowNolock(TRUE);
			}
			else if ( !rtWindow.PtInRect(point) && m_bPtInBindList )
			{
				m_imgDrag.DragShowNolock(FALSE);
				m_imgDrag.DragLeave( ::GetDesktopWindow() );
				m_imgDrag.EndDrag();
				m_imgDrag.BeginDrag(0, ptOffset);
				m_imgDrag.DragEnter( ::GetDesktopWindow(), point );
				m_bPtInBindList = FALSE;
				m_imgDrag.DragShowNolock(TRUE);
			}
		}
	}
}

void CBindPage2::OnLButtonUp(UINT /*nFlags*/, CPoint point)
{
	if ( m_bDragging )
	{
		m_imgDrag.DragLeave( ::GetDesktopWindow() );
		m_imgDrag.EndDrag();
		ReleaseCapture();
	}

	if ( m_bDragFromBindList )
	{
		CRect rtWindow;
		ClientToScreen( &point );
		m_wndListSingle.GetWindowRect( rtWindow );
		if ( rtWindow.PtInRect(point) )
		{
			// Move disks from bind disks' list to single disks' list
			MoveBetweenLists( FALSE );
		}
	}
	else
	{
		CRect rtWindow;
		ClientToScreen( &point );
		m_wndListBind.GetWindowRect( rtWindow );
		if ( rtWindow.PtInRect(point) )
		{
			if ( !CanAddDisksToBindList(m_listDrag) )
				return;
			// Move disks from single disks' list to bind disks' list
			MoveBetweenLists( TRUE );
		}
	}
	UpdateControls();
}

BOOL CBindPage2::OnWizardFinish()
{
	ATLASSERT( m_wndListBind.GetItemCount() == (int)m_nDiskCount );
	CDiskObjectList listBind;
	CDiskObjectVector vtBind;
	int nBindType;
	unsigned int i;

	listBind = m_wndListBind.GetDiskObjectList();
	CDiskObjectList::const_iterator itr;
	for ( itr = listBind.begin(); itr != listBind.end(); ++itr )
	{
		vtBind.push_back( *itr );
	}
	nBindType = GetParentSheet()->GetBindType();
	for ( i=0; i < vtBind.size(); i++ )
	{
		if ( !vtBind[i]->CanAccessExclusive() )
		{
			// TODO : String resource
			WTL::CString strMsg;
			strMsg.Format( 
				_T("Cannot write to disk %s."), 
				vtBind[i]->GetTitle()
				);
			MessageBox( 
				strMsg,
				_T(PROGRAM_TITLE),
				MB_OK | MB_ICONWARNING
				);
			return FALSE;
		}
	}

	try {
		for ( i=0; i < vtBind.size(); i++ )
		{
			vtBind[i]->OpenExclusive();
		}
		for ( i=0; i < vtBind.size(); i++ )
		{
			vtBind[i]->Bind( vtBind, i, (nBindType != BIND_TYPE_AGGR_ONLY) );
			vtBind[i]->CommitDiskInfo();
		}
		for ( i=0; i < vtBind.size(); i++ )
		{
			vtBind[i]->Close();
		}
	}
	catch ( CNDASException & )
	{
		// TODO : ERROR : Fail to unbind
		// Rollback?
		for ( i=0; i < vtBind.size(); i++ )
		{
			vtBind[i]->Close();
		}
		return FALSE;
	}
	GetParentSheet()->SetBoundDisks( vtBind );

	return TRUE;
}