////////////////////////////////////////////////////////////////////////////
//
// Implementation of CSelectDiskDlg class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "nbseldiskdlg.h"

LRESULT CSelectDiskDlg::OnInitDialog(HWND /*hwndCtl*/, LPARAM /*lParam*/)
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

void CSelectDiskDlg::OnOK(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	CDiskObjectList listSelected = m_wndListSingle.GetSelectedDiskObjectList();
	if ( listSelected.size() == 0 )
		// TODO : Display message to select
		return;
	ATLASSERT( listSelected.size() == 1 );
	m_pSelectedDisk = boost::dynamic_pointer_cast<CUnitDiskObject>(listSelected.front());
	// Check if the selected disk's size is not smaller than the source disk's size
	EndDialog(IDOK);
}

void CSelectDiskDlg::OnCancel(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	EndDialog(IDCANCEL);
}