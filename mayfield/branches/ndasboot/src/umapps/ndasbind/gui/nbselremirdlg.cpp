////////////////////////////////////////////////////////////////////////////
//
// Implementation of CSelectMirDlg class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "nbselremirdlg.h"

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
	{
		// TODO : String resource
		WTL::CString strMsg;
		strMsg.LoadString( IDS_SELECTMIRDLG_NO_DISK_SELECTED );
		WTL::CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);
		MessageBox(
			strMsg,
			strTitle,
			MB_OK|MB_ICONWARNING
			);
		return;
	}
	ATLASSERT( listSelected.size() == 1 );
	m_pSelectedDisk = boost::dynamic_pointer_cast<CUnitDiskObject>(listSelected.front());
	// Check if the selected disk's size is not smaller than the source disk's size
	if ( m_pSelectedDisk->GetInfoHandler()->GetUserSectorCount() 
		< m_pSourceDisk->GetInfoHandler()->GetUserSectorCount() )
	{
		WTL::CString strMsg;
		strMsg.LoadString( IDS_SELECTMIRDLG_SMALLER_DISK );
		WTL::CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);
		MessageBox(
			strMsg,
			strTitle, 
			MB_OK | MB_ICONWARNING
			);
		return;
	}

	{
		WTL::CString strMsg;
		strMsg.LoadString(IDS_WARNING_ADD_MIRROR);
		WTL::CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);

		int ret = MessageBox( 
			strMsg,
			strTitle,
			MB_YESNO | MB_ICONWARNING
			);

		if(IDYES != ret)
		{
			EndDialog(IDCANCEL);
		}
	}

	EndDialog(IDOK);
}

void CSelectMirDlg::OnCancel(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	EndDialog(IDCANCEL);
}