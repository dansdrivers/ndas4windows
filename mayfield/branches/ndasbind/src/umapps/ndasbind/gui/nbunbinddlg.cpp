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
	UINT32 nDiskCount = 0;
	unsigned int i;
	NDASCOMM_CONNECTION_INFO *pConnectionInfo;
	CFindIfVisitor<TRUE> unitDiskFinder;
	CDiskObjectList listUnbind;	// List of disks to unbind
	CDiskObjectList::iterator itr;
	CUnitDiskObjectPtr unitDisk;
	WTL::CString strMsg;
	WTL::CString strTitle;
	strTitle.LoadString(IDS_APPLICATION);
	BOOL bUnbindMirror;

	BOOL bReadyToUnbind;
	UINT32 BindResult;

	bUnbindMirror = (dynamic_cast<const CMirDiskObject*>(m_pDiskUnbind.get()) != NULL);

	// warning message
	strMsg.LoadString((bUnbindMirror) ? IDS_WARNING_UNBIND_MIR : IDS_WARNING_UNBIND);
	int id = MessageBox(
		strMsg,
		strTitle,
		MB_YESNO|MB_ICONEXCLAMATION
		);
	if(IDYES != id)
		return;

	listUnbind = unitDiskFinder.FindIf( m_pDiskUnbind, IsUnitDisk);
	nDiskCount = listUnbind.size();
	pConnectionInfo = new NDASCOMM_CONNECTION_INFO[nDiskCount];
	ZeroMemory(pConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO) * nDiskCount);

	bReadyToUnbind = TRUE;
	for ( itr = listUnbind.begin(), i = 0; itr != listUnbind.end(); ++itr )
	{
		if(!(*itr)->IsUnitDisk())
			continue;

		unitDisk = boost::dynamic_pointer_cast<CUnitDiskObject>(*itr);

		pConnectionInfo[i].address_type = NDASCOMM_CONNECTION_INFO_TYPE_ADDR_LPX;
		pConnectionInfo[i].login_type = NDASCOMM_LOGIN_TYPE_NORMAL;
		pConnectionInfo[i].UnitNo = unitDisk->GetLocation()->GetUnitDiskLocation()->UnitNumber;
		pConnectionInfo[i].bWriteAccess = TRUE;
		pConnectionInfo[i].ui64OEMCode = NULL;
		pConnectionInfo[i].protocol = NDASCOMM_TRANSPORT_LPX;

		CopyMemory(pConnectionInfo[i].AddressLPX, 
			unitDisk->GetLocation()->GetUnitDiskLocation()->MACAddr,
			LPXADDR_NODE_LENGTH);
		
		if(!(unitDisk->GetAccessMask() & GENERIC_WRITE))
		{
			// "%1!s! does not have a write access privilege. You need to set write key to this NDAS device before this action."
			strMsg.FormatMessage(IDS_ERROR_NOT_REGISTERD_WRITE_FMT,
				unitDisk->GetTitle()
				);
			MessageBox(
				strMsg,
				strTitle,
				MB_OK|MB_ICONERROR
				);

			bReadyToUnbind = FALSE;
		}

		i++;
	}

	if(!bReadyToUnbind)
	{
		delete [] pConnectionInfo;
		EndDialog(IDCANCEL);
	}

	BindResult = NdasOpBind(i, pConnectionInfo,NMT_SINGLE);

	DWORD dwLastError = ::GetLastError();

	m_unboundDisks = listUnbind;

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
		for ( itr = listUnbind.begin(); itr != listUnbind.end(); ++itr )
		{
			unitDisk = boost::dynamic_pointer_cast<CUnitDiskObject>(*itr);

			if(!BindResult)
				break;
			BindResult--;
		}

		::SetLastError(dwLastError);

		switch(dwLastError)
		{
		case NDASCOMM_ERROR_RW_USER_EXIST:
		case NDASOP_ERROR_ALREADY_USED:
		case NDASOP_ERROR_DEVICE_FAIL:
		case NDASOP_ERROR_NOT_SINGLE_DISK:
		case NDASOP_ERROR_DEVICE_UNSUPPORTED:
		case NDASOP_ERROR_NOT_BOUND_DISK: // does not return this error
			strMsg.FormatMessage(IDS_BIND_FAIL_AT_SINGLE_NDAS_FMT, unitDisk->GetTitle());
			break;
		default:
			strMsg.LoadString(IDS_BIND_FAIL);
			break;
		}
		ShowErrorMessageBox(IDS_MAINFRAME_SINGLE_ACCESS_FAIL);
 	}

	CNdasHIXChangeNotify HixChangeNotify(pGetNdasHostGuid());
	BOOL bResults = HixChangeNotify.Initialize();
	if(bResults)
	{
		for(i = 0; i < BindResult; i++)
		{
			NDAS_UNITDEVICE_ID unitDeviceId;
			CopyMemory(unitDeviceId.DeviceId.Node, pConnectionInfo[i].AddressLPX, 
				sizeof(unitDeviceId.DeviceId.Node));
			unitDeviceId.UnitNo = pConnectionInfo[i].UnitNo;
			HixChangeNotify.Notify(unitDeviceId);
		}
	}

	delete [] pConnectionInfo;

	EndDialog(IDOK);
}

void CUnBindDlg::OnCancel(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	EndDialog(IDCANCEL);
}

void CUnBindDlg::SetDiskToUnbind(CDiskObjectPtr obj)
{
	 m_pDiskUnbind = obj; 

	CFindIfVisitor<TRUE> unitDiskFinder;
	CDiskObjectList listUnbind;	// List of disks to unbind
	listUnbind = unitDiskFinder.FindIf( m_pDiskUnbind, IsUnitDisk);
	
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
}