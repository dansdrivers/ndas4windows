////////////////////////////////////////////////////////////////////////////
//
// Implementation of CBindPage classes
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "nbbindpages.h"
#include "nbbindsheet.h"
#include "ndasexception.h"
#include "nbuihandler.h"
#include "ndas/ndasop.h"
#include "apperrdlg.h"
#include "appconf.h"

//////////////////////////////////////////////////////////////////////////
// Page 1
//////////////////////////////////////////////////////////////////////////
LRESULT CBindPage1::OnInitDialog(HWND /*hWndFocus*/, LPARAM /*lParam*/)
{
	WTL::CString strCaption;
	strCaption.LoadString(IDS_BINDPAGE_CAPTION);
	GetParentSheet()->SetWindowText(strCaption);

	RefreshDiskCountComboBox(BIND_TYPE_AGGR);

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

	// Check whether the number of disks is valid for the selected type of binding.
	// AING_TO_DO : edit box -> spin

	BOOL bValidDiskCount = TRUE;

	switch(m_nType)
	{
	case BIND_TYPE_AGGR :
		bValidDiskCount = CAggrDiskUIHandler::IsValidDiskCount(m_nDiskCount);
		break;
	case BIND_TYPE_RAID0:
		bValidDiskCount = CRAID0DiskUIHandler::IsValidDiskCount(m_nDiskCount);
		break;
	case BIND_TYPE_RAID1:
		bValidDiskCount = CMirDiskUIHandler::IsValidDiskCount(m_nDiskCount);
		break;
	case BIND_TYPE_RAID4:
		bValidDiskCount = CRAID4DiskUIHandler::IsValidDiskCount(m_nDiskCount);
		break;
	default:
		bValidDiskCount = FALSE;
		return FALSE;
	}

	if(!bValidDiskCount)
	{
		// number in combo box must NOT fail
		_ASSERT(FALSE);

		return FALSE;
	}

	pSheet->SetBindType(static_cast<UINT>(m_nType));
	pSheet->SetDiskCount( m_nDiskCount );
	return TRUE;
}

VOID CBindPage1::RefreshDiskCountComboBox(_BIND_TYPE nType)
{
	HWND hWndComboBox = GetDlgItem(IDC_COMBO_DISKCOUNT);
	if(!hWndComboBox)
		return;

	SendMessage(hWndComboBox, CB_RESETCONTENT, 0, 0);
	
	switch(nType)
	{
	case BIND_TYPE_AGGR :
		SendMessage(hWndComboBox, CB_INSERTSTRING, -1, (LPARAM)_T("2"));
		SendMessage(hWndComboBox, CB_INSERTSTRING, -1, (LPARAM)_T("3"));
		SendMessage(hWndComboBox, CB_INSERTSTRING, -1, (LPARAM)_T("4"));
		SendMessage(hWndComboBox, CB_INSERTSTRING, -1, (LPARAM)_T("5"));
		SendMessage(hWndComboBox, CB_INSERTSTRING, -1, (LPARAM)_T("6"));
		SendMessage(hWndComboBox, CB_INSERTSTRING, -1, (LPARAM)_T("7"));
		SendMessage(hWndComboBox, CB_INSERTSTRING, -1, (LPARAM)_T("8"));
		break;
	case BIND_TYPE_RAID0 :
		SendMessage(hWndComboBox, CB_INSERTSTRING, -1, (LPARAM)_T("2"));
		SendMessage(hWndComboBox, CB_INSERTSTRING, -1, (LPARAM)_T("4"));
		SendMessage(hWndComboBox, CB_INSERTSTRING, -1, (LPARAM)_T("8"));
		break;
	case BIND_TYPE_RAID1 :
		SendMessage(hWndComboBox, CB_INSERTSTRING, -1, (LPARAM)_T("2"));
		break;
	case BIND_TYPE_RAID4 :
		SendMessage(hWndComboBox, CB_INSERTSTRING, -1, (LPARAM)_T("3"));
		SendMessage(hWndComboBox, CB_INSERTSTRING, -1, (LPARAM)_T("5"));
		SendMessage(hWndComboBox, CB_INSERTSTRING, -1, (LPARAM)_T("9"));
		break;
	default:
		break;
	}
	SendMessage(hWndComboBox, CB_SETCURSEL, 0, 0);
}

LRESULT CBindPage1::OnCommand(UINT msg, int nID, HWND hWnd)
{	
	if(BN_CLICKED == msg)
	{
		switch(nID)
		{
		case IDC_BIND_TYPE_AGGR:
			RefreshDiskCountComboBox(BIND_TYPE_AGGR);
			break;
		case IDC_BIND_TYPE_RAID0:
			RefreshDiskCountComboBox(BIND_TYPE_RAID0);
			break;
		case IDC_BIND_TYPE_RAID1:
			RefreshDiskCountComboBox(BIND_TYPE_RAID1);
			break;
		case IDC_BIND_TYPE_RAID4:
			RefreshDiskCountComboBox(BIND_TYPE_RAID4);
			break;
		default:
			break;
		}
	}
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
// Page 2
//////////////////////////////////////////////////////////////////////////
LRESULT CBindPage2::OnInitDialog(HWND /*hWndFocus*/, LPARAM /*lParam*/)
{
	WTL::CString strCaption;
	strCaption.LoadString(IDS_BINDPAGE_CAPTION);
	GetParentSheet()->SetWindowText(strCaption);

	m_btnAdd.SubclassWindow( GetDlgItem(IDC_BTN_ADD) );
	m_btnRemove.SubclassWindow( GetDlgItem(IDC_BTN_REMOVE) );
//	m_btnUp.SubclassWindow( GetDlgItem(IDC_BTN_UP) );
//	m_btnDown.SubclassWindow( GetDlgItem(IDC_BTN_DOWN) );

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
	m_imgDrag.Create(16, 12, ILC_COLOR8|ILC_MASK, 0, 1);
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
//	GetDlgItem(IDC_BTN_UP).EnableWindow( m_wndListBind.IsItemMovable(TRUE) );
//	GetDlgItem(IDC_BTN_DOWN).EnableWindow( m_wndListBind.IsItemMovable(FALSE) );
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
		m_nItemHit = -1;
	}
	else
	{
		m_bDragFromBindList = TRUE;
		m_bPtInBindList = TRUE;
		m_listDrag = m_wndListBind.GetSelectedDiskObjectList();
		UINT flags;
		m_nItemHit = m_wndListBind.HitTest( ptAction, &flags );
	}

	return 0;
}

void CBindPage2::OnMouseMove(UINT /*nFlags*/, CPoint point)
{
	if ( m_bDragging )
	{
		CPoint ptScreen(point);
		ClientToScreen( &ptScreen );
		BOOL bInBindList;
		
		CRect rtWindow;
		m_wndListBind.GetWindowRect( rtWindow );
		bInBindList = rtWindow.PtInRect( ptScreen );

		m_imgDrag.DragMove( ptScreen );
		if ( !m_bDragFromBindList )
		{
			CPoint ptOffset(-10, -10);
			m_imgDrag.DragShowNolock(FALSE);
			if ( bInBindList && !m_bPtInBindList )
			{
				// Enter the bind list

				// If the selected disks cannot be added to bind list,
				// change the icon to show that.
                if ( !CanAddDisksToBindList(m_listDrag) )
				{
					m_imgDrag.DragLeave( ::GetDesktopWindow() );
					m_imgDrag.EndDrag();
					m_imgDrag.BeginDrag(1, ptOffset);
					m_imgDrag.DragEnter( ::GetDesktopWindow(), ptScreen );
				}
			}
			else if ( !bInBindList && m_bPtInBindList )
			{
				// Leave the bind list
				m_imgDrag.DragLeave( ::GetDesktopWindow() );
				m_imgDrag.EndDrag();
				m_imgDrag.BeginDrag(0, ptOffset);
				m_imgDrag.DragEnter( ::GetDesktopWindow(), ptScreen );
			}

			if ( bInBindList )
			{
				int nItem;
				UINT flags;
				CPoint ptList(ptScreen);
				m_wndListBind.ScreenToClient( &ptList );
				nItem = m_wndListBind.HitTest( ptList, &flags );
				
				if ( nItem != -1 && nItem != m_nItemHit )
				{
					m_wndListBind.SetItemState(nItem, 
						LVIS_DROPHILITED, LVIS_DROPHILITED);
					if ( m_nItemHit != -1 )
						m_wndListBind.SetItemState(m_nItemHit,
							0, LVIS_DROPHILITED);
					m_wndListBind.RedrawItems(0, nItem);
					m_wndListBind.UpdateWindow();
					m_nItemHit = nItem;
				}
			}
			else if ( m_bPtInBindList )
			{
				// Leave the bind list
				int nCount = m_wndListBind.GetItemCount();
				for ( int i=0; i < nCount; i++ )
				{
					m_wndListBind.SetItemState(i,
						0, LVIS_DROPHILITED);
					m_wndListBind.RedrawItems(i, m_nItemHit);
					m_wndListBind.UpdateWindow();
				}
			}
			m_bPtInBindList = bInBindList;
			m_imgDrag.DragShowNolock(TRUE);
		}


		// If the mouse is moving over the bind list, 
		// move items.
		if ( m_bDragFromBindList )
		{
			int nItem;
			UINT flags;
			CPoint ptList(ptScreen);
			m_wndListBind.ScreenToClient( &ptList );

			nItem = m_wndListBind.HitTest( ptList, &flags );
			if ( nItem != -1 && nItem != m_nItemHit )
			{
				m_imgDrag.DragShowNolock(FALSE);
				if ( nItem < m_nItemHit )
				{
					m_wndListBind.MoveSelectedItemUp();
					m_nItemHit--;
				}
				else if ( nItem > m_nItemHit )
				{
					m_wndListBind.MoveSelectedItemDown();
					m_nItemHit++;
				}
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
		if ( m_bDragFromBindList )
		{
			//
			// Move items if necessary
			//
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
			//
			// Reset items which are highlighted during draging.
			//
			int nCount = m_wndListBind.GetItemCount();
			for ( int i=0; i < nCount; i++ )
			{
				m_wndListBind.SetItemState(i,
					0, LVIS_DROPHILITED);
				m_wndListBind.RedrawItems(i, m_nItemHit);
				m_wndListBind.UpdateWindow();
			}

			//
			// Move items if necessary
			//
			CRect rtWindow;
			int	nMoveUpCount = 0;
			ClientToScreen( &point );
			m_wndListBind.GetWindowRect( rtWindow );
			if ( rtWindow.PtInRect(point) )
			{
				if ( !CanAddDisksToBindList(m_listDrag) )
					return;
				if ( m_nItemHit != -1 )
					nMoveUpCount = m_wndListBind.GetItemCount() - m_nItemHit;
				// Move disks from single disks' list to bind disks' list
				MoveBetweenLists( TRUE );
				// Change insertion point to the point mouse indicates
				if ( m_nItemHit != -1 )
				{
					for ( int i=0; i < nMoveUpCount; i++ )
						m_wndListBind.MoveSelectedItemUp();
				}

			}
		}
		UpdateControls();
		m_bDragging = FALSE;
	}
}

LRESULT CBindPage2::OnDblClkListSingle(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	MoveBetweenLists(TRUE);

	return TRUE;
}

LRESULT CBindPage2::OnDblClkListBind(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	MoveBetweenLists(FALSE);

	return TRUE;
}

BOOL CBindPage2::OnWizardFinish()
{
	ATLASSERT( m_wndListBind.GetItemCount() == (int)m_nDiskCount );
	CDiskObjectList listBind;
	CUnitDiskObjectVector vtBind;
	int nBindType, nDIBBindType;
	unsigned int i;
	NDASCOMM_CONNECTION_INFO *pConnectionInfo;
	UINT32 BindResult;
	WTL::CString strMsg;
	BOOL bReadyToBind;

	// warning message
	{
		WTL::CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);
		strMsg.LoadString(IDS_WARNING_BIND);
		int id = MessageBox(
			strMsg,
			strTitle,
			MB_YESNO|MB_ICONEXCLAMATION
			);
		if(IDYES != id)
			return FALSE;
	}

	listBind = m_wndListBind.GetDiskObjectList();
	CDiskObjectList::const_iterator itr;
	for ( itr = listBind.begin(); itr != listBind.end(); ++itr )
	{
		vtBind.push_back( 
			boost::dynamic_pointer_cast<CUnitDiskObject>(*itr)
			);
	}
	nBindType = GetParentSheet()->GetBindType();
	switch( nBindType ) 
	{
	case BIND_TYPE_AGGR:
		nDIBBindType = NMT_AGGREGATE;
		break;
	case BIND_TYPE_RAID0:
		nDIBBindType = NMT_RAID0;
		break;
	case BIND_TYPE_RAID1:
		nDIBBindType = NMT_RAID1;
		break;
	case BIND_TYPE_RAID4:
		nDIBBindType = NMT_RAID4;
		break;
	default:
		//
		// You should add more 'case's to support more types 
		//
		ATLASSERT(FALSE);
		nDIBBindType = 0;
		break;
	}

	pConnectionInfo = new NDASCOMM_CONNECTION_INFO[vtBind.size()];
	ZeroMemory(pConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO) * vtBind.size());

	bReadyToBind = TRUE;
	for (i = 0; i < vtBind.size(); i++ )
	{
		
		pConnectionInfo[i].address_type = NDASCOMM_CONNECTION_INFO_TYPE_ADDR_LPX;
		pConnectionInfo[i].login_type = NDASCOMM_LOGIN_TYPE_NORMAL;
		pConnectionInfo[i].UnitNo = vtBind[i]->GetLocation()->GetUnitDiskLocation()->UnitNumber;
		pConnectionInfo[i].bWriteAccess = TRUE;
		pConnectionInfo[i].ui64OEMCode = NULL;
		pConnectionInfo[i].protocol = NDASCOMM_TRANSPORT_LPX;
		CopyMemory(pConnectionInfo[i].AddressLPX, 
			vtBind[i]->GetLocation()->GetUnitDiskLocation()->MACAddr,
			LPXADDR_NODE_LENGTH);

		if(!(vtBind[i]->GetAccessMask() & GENERIC_WRITE))
		{
			// "%1!s! does not have a write access privilege. You need to set write key to this NDAS device before this action."
			strMsg.FormatMessage(IDS_ERROR_NOT_REGISTERD_WRITE_FMT,
				vtBind[i]->GetTitle()
				);
			WTL::CString strTitle;
			strTitle.LoadString(IDS_APPLICATION);
			MessageBox(
				strMsg,
				strTitle,
				MB_OK|MB_ICONERROR
				);

			bReadyToBind = FALSE;
		}
	}

	if(!bReadyToBind)
	{
		delete [] pConnectionInfo;
		return FALSE;
	}

	BindResult = NdasOpBind(vtBind.size(), pConnectionInfo, nDIBBindType);

	if(vtBind.size() == BindResult)
	{
		GetParentSheet()->SetBoundDisks( vtBind );

		WTL::CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);
		strMsg.LoadString(IDS_WARNING_BIND_AFTER);
		MessageBox(
			strMsg,
			strTitle,
			MB_OK|MB_ICONINFORMATION
			);
	}
	else
	{
		DWORD dwLastError = ::GetLastError();

		switch(dwLastError)
		{
		case NDASCOMM_ERROR_RW_USER_EXIST:
		case NDASOP_ERROR_ALREADY_USED:
		case NDASOP_ERROR_DEVICE_FAIL:
		case NDASOP_ERROR_NOT_SINGLE_DISK:
		case NDASOP_ERROR_DEVICE_UNSUPPORTED:
			strMsg.FormatMessage(IDS_BIND_FAIL_AT_SINGLE_NDAS_FMT, vtBind[BindResult]->GetTitle());
			break;
		default:
			strMsg.LoadString(IDS_BIND_FAIL);
			break;
		}

		ShowErrorMessageBox(strMsg);
	}

	CNdasHIXChangeNotify HixChangeNotify(pGetNdasHostGuid());
	BOOL bResults = HixChangeNotify.Initialize();
	if(bResults)
	{
		for(i = 0; i < vtBind.size(); i++)
		{
			NDAS_UNITDEVICE_ID unitDeviceId;
			CopyMemory(unitDeviceId.DeviceId.Node, pConnectionInfo[i].AddressLPX, 
				sizeof(unitDeviceId.DeviceId.Node));
			unitDeviceId.UnitNo = pConnectionInfo[i].UnitNo;
			HixChangeNotify.Notify(unitDeviceId);
		}
	}

	delete [] pConnectionInfo;

	return TRUE;
}