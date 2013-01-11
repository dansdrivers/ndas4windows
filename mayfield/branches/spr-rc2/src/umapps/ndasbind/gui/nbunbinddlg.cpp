////////////////////////////////////////////////////////////////////////////
//
// Implementation of CUnBindDlg class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "nbunbinddlg.h"
#include "ndasexception.h"
#include "ndashelper.h"
#include "ndas/ndasop.h"
#include "apperrdlg.h"
#include "appconf.h"

CUnBindDlg::CUnBindDlg()
{
	IDD = IDD_UNBIND;
	m_wndListUnbind = CNBListViewCtrl(2);
}

LRESULT CUnBindDlg::OnInitDialog(HWND /*hWndCtl*/, LPARAM /*lParam*/)
{
//	CFindIfVisitor<TRUE> unitDiskFinder;
//	CDiskObjectList listUnbind;	// List of disks to unbind
	CenterWindow(GetParent());

	m_wndListUnbind.SubclassWindow( GetDlgItem(IDC_LIST_UNBIND) );
	DWORD dwStyle = LVS_EX_FULLROWSELECT; 
	//| LVS_EX_GRIDLINES 
	//| LVS_EX_INFOTIP 
	m_wndListUnbind.SetExtendedListViewStyle( dwStyle );
	m_wndListUnbind.InitColumn();

	for(UINT32 i = 0; i < m_pLogicalDeviceUnbind->DevicesTotal(); i++)
	{
		if((*m_pLogicalDeviceUnbind)[i])
			m_wndListUnbind.AddDiskObject((*m_pLogicalDeviceUnbind)[i]);
	}
	DoDataExchange( FALSE );

	return TRUE;
}

void CUnBindDlg::OnOK(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	UINT32 i,j;
	NDASCOMM_CONNECTION_INFO *pConnectionInfo;
	WTL::CString strMsg;
	WTL::CString strTitle;
	strTitle.LoadString(IDS_APPLICATION);
	BOOL bUnbindMirror;
	CNBUnitDevice *pUnitDevice;
	NBUnitDevicePtrMap mapUnitDevices;

	BOOL bReadyToUnbind;
	UINT32 BindResult;

	bUnbindMirror = 
		(NMT_RAID1 == m_pLogicalDeviceUnbind->GetType() ||
		NMT_MIRROR == m_pLogicalDeviceUnbind->GetType());

	// warning message
	strMsg.LoadString((bUnbindMirror) ? IDS_WARNING_UNBIND_MIR : IDS_WARNING_UNBIND);
	int id = MessageBox(
		strMsg,
		strTitle,
		MB_YESNO|MB_ICONEXCLAMATION
		);
	if(IDYES != id)
		return;

	for (i = 0, j = 0; i < m_pLogicalDeviceUnbind->DevicesTotal(); i++)
	{
		pUnitDevice = (*m_pLogicalDeviceUnbind)[i];
		if(!pUnitDevice)
			continue;

		if(pUnitDevice->IsOperatable())
			mapUnitDevices[j] = pUnitDevice;

		j++;
	}

	bReadyToUnbind = TRUE;
	pConnectionInfo = new NDASCOMM_CONNECTION_INFO[mapUnitDevices.size()];
	for(i = 0; i < mapUnitDevices.size(); i++)
	{
		if(!mapUnitDevices[i]->InitConnectionInfo(&pConnectionInfo[i], TRUE))
		{
			// "%1!s! does not have a write access privilege. You need to set write key to this NDAS device before this action."
			strMsg.FormatMessage(IDS_ERROR_NOT_REGISTERD_WRITE_FMT,
				mapUnitDevices[i]->GetName()
				);
			MessageBox(
				strMsg,
				strTitle,
				MB_OK|MB_ICONERROR
				);

			bReadyToUnbind = FALSE;
		}
	}

	if(!bReadyToUnbind)
	{
		delete [] pConnectionInfo;
		EndDialog(IDCANCEL);
		return;
	}

	BindResult = NdasOpBind(mapUnitDevices.size(), pConnectionInfo,NMT_SINGLE);

	DWORD dwLastError = ::GetLastError();

	if(i == BindResult)
	{
		strMsg.LoadString(
			(bUnbindMirror) ? IDS_WARNING_UNBIND_AFTER_MIR : 
			IDS_WARNING_UNBIND_AFTER);

		MessageBox(
			strMsg,
			strTitle,
			MB_OK|MB_ICONINFORMATION
			);
	}
	else
	{

		::SetLastError(dwLastError);

		switch(dwLastError)
		{
		case NDASCOMM_ERROR_RW_USER_EXIST:
		case NDASOP_ERROR_ALREADY_USED:
		case NDASOP_ERROR_DEVICE_FAIL:
		case NDASOP_ERROR_NOT_SINGLE_DISK:
		case NDASOP_ERROR_DEVICE_UNSUPPORTED:
		case NDASOP_ERROR_NOT_BOUND_DISK: // does not return this error
			if(mapUnitDevices[BindResult])
				strMsg.FormatMessage(IDS_BIND_FAIL_AT_SINGLE_NDAS_FMT, mapUnitDevices[BindResult]);
			else
				strMsg.LoadString(IDS_BIND_FAIL);
			break;
		default:
			strMsg.LoadString(IDS_BIND_FAIL);
			break;
		}
		ShowErrorMessageBox(IDS_MAINFRAME_SINGLE_ACCESS_FAIL);
 	}

	for(i = 0; i < mapUnitDevices.size(); i++)
	{
		mapUnitDevices[i]->HixChangeNotify(pGetNdasHostGuid());
	}

	delete [] pConnectionInfo;

	EndDialog(IDOK);
}

void CUnBindDlg::OnCancel(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	EndDialog(IDCANCEL);
}

void CUnBindDlg::SetDiskToUnbind(CNBLogicalDevice *obj)
{
	m_pLogicalDeviceUnbind = obj; 

/*
	CFindIfVisitor<TRUE> unitDiskFinder;
	CDiskObjectList listUnbind;	// List of disks to unbind
	listUnbind = unitDiskFinder.FindIf( m_pLogicalDeviceUnbind, IsUnitDisk);
	
	//
	// Data is deleted only when the disks are aggregated
	// Thus, change message if they are not.
	//
	CUnitDiskObjectPtr unitDisk = 
		boost::dynamic_pointer_cast<CUnitDiskObject>(listUnbind.front());
	if ( unitDisk->GetInfoHandler()->IsBoundAndNotSingleMirrored() )
	{
		IDD = IDD_UNBIND_MIRROR;
	}
*/
}