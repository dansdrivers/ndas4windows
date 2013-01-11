////////////////////////////////////////////////////////////////////////////
//
// Implementation of CUnBindDlg class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "nbdefine.h"
#include "nbunbinddlg.h"
#include "ndasexception.h"

CUnBindDlg::CUnBindDlg()
{
	m_wndListUnbind = CNBListViewCtrl(2);
}

LRESULT CUnBindDlg::OnInitDialog(HWND /*hWndCtl*/, LPARAM /*lParam*/)
{
	CFindIfVisitor<TRUE> unitDiskFinder;
	CDiskObjectList listUnbind;	// List of disks to unbind
	CenterWindow(GetParent());

	m_wndListUnbind.SubclassWindow( GetDlgItem(IDC_LIST_UNBIND) );
	DWORD dwStyle = LVS_EX_FULLROWSELECT; 
	//| LVS_EX_GRIDLINES 
	//| LVS_EX_INFOTIP 
	m_wndListUnbind.SetExtendedListViewStyle( dwStyle );
	m_wndListUnbind.InitColumn();
	
	listUnbind = unitDiskFinder.FindIf( m_pDiskUnbind, IsUnitDisk);
	m_wndListUnbind.AddDiskObjectList(listUnbind);
	DoDataExchange( FALSE );

	return TRUE;
}

void CUnBindDlg::OnOK(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	if ( !m_pDiskUnbind->CanAccessExclusive() )
	{
		// TODO : String resource
		MessageBox( 
			_T("Disks being accessed by other program/computer cannot be unbound."),
			_T(PROGRAM_TITLE),
			MB_OK | MB_ICONWARNING
			);
		return;
	}
	try {
		m_pDiskUnbind->OpenExclusive();
		m_unboundDisks = m_pDiskUnbind->UnBind( m_pDiskUnbind );
		m_pDiskUnbind->CommitDiskInfo();
		m_pDiskUnbind->Close();
	}
	catch( CNDASException & )
	{
		// TODO : ERROR : Fail to unbind
		// Rollback?
		m_pDiskUnbind->Close();
	}
	EndDialog(IDOK);
}

void CUnBindDlg::OnCancel(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	EndDialog(IDCANCEL);
}

