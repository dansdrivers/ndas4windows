////////////////////////////////////////////////////////////////////////////
//
// Implementation of CSelectMirDlg class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "nbselremirdlg.h"
#include "nbdefine.h"

LRESULT CSelectMirDlg::OnInitDialog(HWND /*hwndCtl*/, LPARAM /*lParam*/)
{
	ATLASSERT( m_singleDisks.size() > 0 );

	CenterWindow();
	m_wndListSingle.SubclassWindow( GetDlgItem(IDC_LIST_SINGLE) );
	DWORD dwStyle = LVS_EX_FULLROWSELECT; 
	//| LVS_EX_GRIDLINES 
	//| LVS_EX_INFOTIP 
	m_wndListSingle.SetExtendedListViewStyle( dwStyle );
	m_wndListSingle.InitColumn();
	m_wndListSingle.AddDiskObjectList(m_singleDisks);

	DoDataExchange(FALSE);
	return TRUE;
}

void CSelectMirDlg::OnOK(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	CDiskObjectList listSelected = m_wndListSingle.GetSelectedDiskObjectList();
	if ( listSelected.size() == 0 )
		// TODO : Display message to select
		return;
	ATLASSERT( listSelected.size() == 1 );
	m_pSelectedDisk = boost::dynamic_pointer_cast<CUnitDiskObject>(listSelected.front());
	// Check if the selected disk's size is not smaller than the source disk's size
	if ( m_pSelectedDisk->GetInfoHandler()->GetUserSectorCount() 
		< m_pSourceDisk->GetInfoHandler()->GetUserSectorCount() )
	{
		MessageBox(
			_T("A disk cannot be mirrored by a smaller disk."), 
			_T(PROGRAM_TITLE), 
			MB_OK
			);
		return;
	}
	// FIXME : Confirmation would be necessary.
	EndDialog(IDOK);
}

void CSelectMirDlg::OnCancel(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	EndDialog(IDCANCEL);
}