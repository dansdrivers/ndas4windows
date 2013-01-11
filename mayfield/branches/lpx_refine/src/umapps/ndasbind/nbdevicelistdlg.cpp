////////////////////////////////////////////////////////////////////////////
//
// Implementation of CSelectDiskDlg class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"
#include "nbdevicelistdlg.h"

LRESULT CNBSelectDeviceDlg::OnInitDialog(HWND /*hwndCtl*/, LPARAM /*lParam*/)
{
	CenterWindow();

	WTL::CString strText;

	strText.LoadString(m_nCaptionID);
	SetWindowText(strText);

	CStatic ctlMessage;
	ctlMessage.Attach(GetDlgItem(IDC_STATIC_MESSAGE));
	strText.LoadString(m_nMessageID);
	ctlMessage.SetWindowText(strText);

	m_wndListSingle.SubclassWindow( GetDlgItem(IDC_LIST_DEVICE) );
	if(0 == m_nSelectCount)
	{
		m_wndListSingle.ModifyStyle(NULL, WS_DISABLED);
	}
	else if(1 == m_nSelectCount)
	{
		m_wndListSingle.ModifyStyle(NULL, LVS_SINGLESEL);
	}
	else
	{
		m_wndListSingle.ModifyStyle(LVS_SINGLESEL, NULL);
	}

	DWORD dwExtStyle = LVS_EX_FULLROWSELECT; 
		//| LVS_EX_GRIDLINES 
		//| LVS_EX_INFOTIP 
	m_wndListSingle.SetExtendedListViewStyle( dwExtStyle );
	m_wndListSingle.InitColumn();
	m_wndListSingle.AddDiskObjectList(m_listDevices);

	OnListSelChanged(NULL);

	DoDataExchange(FALSE);
	return TRUE;
}

BOOL CNBSelectDeviceDlg::IsOK()
{
	m_listDevicesSelected.clear();

	m_listDevicesSelected = m_wndListSingle.GetSelectedDiskObjectList();
	if(m_listDevicesSelected.size() != m_nSelectCount)
		return FALSE;

	UINT nID;

	for(NBUnitDevicePtrList::iterator itUnitDevice = m_listDevicesSelected.begin();
		itUnitDevice != m_listDevicesSelected.end(); itUnitDevice++)
	{
		if(!m_fnCallBack((*itUnitDevice), m_hWnd, m_lpContext))
			return FALSE;
	}

	return TRUE;
}

LRESULT CNBSelectDeviceDlg::OnListSelChanged(LPNMHDR /*lpNMHDR*/)
{
	GetDlgItem(IDOK).EnableWindow(IsOK());

	return 0;
}

void CNBSelectDeviceDlg::OnOK(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	if(!IsOK())
		return;

	m_listDevicesSelected.clear();
	m_listDevicesSelected = m_wndListSingle.GetSelectedDiskObjectList();

	EndDialog(IDOK);
}

void CNBSelectDeviceDlg::OnCancel(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	EndDialog(IDCANCEL);
}