////////////////////////////////////////////////////////////////////////////
//
// Implementation of CBindDlg class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////


#include "stdafx.h"
#include "ndasutil.h"
#include "nbuihandler.h"
#include "nbbinddlg.h"

CBindDlg::CBindDlg()
	: m_bUseMirror(FALSE)
{

}
LRESULT CBindDlg::OnInitDialog(HWND /*hWndCtl*/, LPARAM /*lParam*/)
{
	CenterWindow(GetParent());

	m_wndListSingle.SubclassWindow( GetDlgItem(IDC_LIST_SINGLE) );
	m_wndListPrimary.SubclassWindow( GetDlgItem(IDC_LIST_PRIMARY) );
	m_wndListMirror.SubclassWindow( GetDlgItem(IDC_LIST_MIRROR) );
	m_btnToPrimary.SubclassWindow( GetDlgItem(IDC_BTN_TO_PRIMARY) );
	m_btnFromPrimary.SubclassWindow( GetDlgItem(IDC_BTN_FROM_PRIMARY) );
	m_btnToMirror.SubclassWindow( GetDlgItem(IDC_BTN_TO_MIRROR) );
	m_btnFromMirror.SubclassWindow( GetDlgItem(IDC_BTN_FROM_MIRROR) );


	// Initialize lists
	DWORD dwStyle = LVS_EX_FULLROWSELECT; 
				//| LVS_EX_GRIDLINES 
				//| LVS_EX_INFOTIP 
	m_wndListSingle.SetExtendedListViewStyle( dwStyle );
	m_wndListPrimary.SetExtendedListViewStyle( dwStyle );
	m_wndListMirror.SetExtendedListViewStyle( dwStyle );
	m_wndListSingle.InitColumn();
	m_wndListPrimary.InitColumn();
	m_wndListMirror.InitColumn();
	m_wndListSingle.AddDiskObjectList( m_singleDisks );

	DoDataExchange(FALSE);

	AdjustControls(m_bUseMirror);
	UpdateControls();
	return TRUE;
}

void CBindDlg::AdjustControls(BOOL bMirror)
{
	int nWidthExpand;
	CRect rectSingle, rectMirror;
	CRect rectControl;

	m_wndListSingle.GetWindowRect( rectSingle );
	m_wndListMirror.GetWindowRect( rectMirror );
	nWidthExpand  = rectSingle.Width() - rectMirror.Width();
	if ( bMirror ) nWidthExpand *= -1;

	m_wndListPrimary.GetWindowRect( rectControl );
	rectControl.InflateRect(0, 0, nWidthExpand, 0);
	CDialogImpl<CBindDlg>::ScreenToClient( rectControl );
	m_wndListPrimary.MoveWindow( rectControl );

	m_btnToPrimary.GetWindowRect( rectControl );
	rectControl.OffsetRect( nWidthExpand/2, 0);
	CDialogImpl<CBindDlg>::ScreenToClient( rectControl );
	m_btnToPrimary.MoveWindow( rectControl );
	
	m_btnFromPrimary.GetWindowRect( rectControl );
	rectControl.OffsetRect( nWidthExpand/2, 0);
	CDialogImpl<CBindDlg>::ScreenToClient( rectControl );
	m_btnFromPrimary.MoveWindow( rectControl );

	int nCmdShow =  bMirror ? SW_SHOW : SW_HIDE;
	m_wndListMirror.ShowWindow( nCmdShow );
	m_btnToMirror.ShowWindow( nCmdShow );
	m_btnFromMirror.ShowWindow( nCmdShow );
}

void CBindDlg::UpdateControls()
{
	CWindow btnOK = GetDlgItem(IDOK);
	if ( m_bUseMirror )
	{
		if ( m_wndListPrimary.GetItemCount() > 0 
			&& m_wndListPrimary.GetItemCount() == m_wndListMirror.GetItemCount() )
		{
			btnOK.EnableWindow();
		}
		else
		{
			btnOK.EnableWindow(FALSE);
		}
	}
	else
	{
		btnOK.EnableWindow( m_wndListPrimary.GetItemCount() > 1 );
	}

	m_btnToPrimary.EnableWindow( m_wndListSingle.GetItemCount() > 0 );
	m_btnFromPrimary.EnableWindow( m_wndListPrimary.GetItemCount() > 0 );
	m_btnToMirror.EnableWindow( m_wndListSingle.GetItemCount() > 0 );
	m_btnFromMirror.EnableWindow( m_wndListMirror.GetItemCount() > 0 );
}
void CBindDlg::OnOK(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	m_boundDisks.clear();
	if ( m_bUseMirror )
	{
		CDiskObjectList listPrimary, listMirror;
		listPrimary = m_wndListPrimary.GetDiskObjectList();
		listMirror = m_wndListMirror.GetDiskObjectList();
		while ( !listPrimary.empty() )
		{
			ATLASSERT( !listMirror.empty() );
			m_boundDisks.push_back( listPrimary.front() );
			m_boundDisks.push_back( listMirror.front() );
			listPrimary.pop_front();
			listMirror.pop_front();
		}
	}
	else
	{
		CDiskObjectList listDisks = m_wndListPrimary.GetDiskObjectList();
		m_boundDisks.resize( listDisks.size() );
		std::copy( listDisks.begin(), listDisks.end(), m_boundDisks.begin() );
	}

	for ( unsigned int i=0; i < m_boundDisks.size(); i++ )
	{
		m_boundDisks[i]->Bind( m_boundDisks, i, m_bUseMirror );
		m_boundDisks[i]->UpdateData();
	}

	EndDialog(IDOK);
}
void CBindDlg::OnCancel(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	EndDialog(IDCANCEL);
}

void CBindDlg::OnClickUseMirror(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	DoDataExchange(TRUE);
	AdjustControls(m_bUseMirror);
}

void CBindDlg::OnClickMove(UINT /*wNotifyCode*/, int wID, HWND /*hwndCtl*/)
{
	CNBListViewCtrl *pWndFrom, *pWndTo;
	switch( wID )
	{
	case IDC_BTN_FROM_MIRROR:
		pWndFrom = &m_wndListMirror;
		pWndTo = &m_wndListSingle;
		break;
	case IDC_BTN_TO_MIRROR:
		pWndFrom = &m_wndListSingle;
		pWndTo = &m_wndListMirror;
		break;
	case IDC_BTN_FROM_PRIMARY:
		pWndFrom = &m_wndListPrimary;
		pWndTo = &m_wndListSingle;
		break;
	case IDC_BTN_TO_PRIMARY:
	default:
		pWndFrom = &m_wndListSingle;
		pWndTo = &m_wndListPrimary;
		break;
	}

	CDiskObjectList selectedDisks = pWndFrom->GetSelectedDiskObjectList();
	if ( selectedDisks.size() == 0 )
		return;

	pWndTo->AddDiskObjectList(selectedDisks);
	pWndFrom->DeleteDiskObjectList(selectedDisks);
	UpdateControls();
}



