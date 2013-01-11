////////////////////////////////////////////////////////////////////////////
//
// Implementation of CDiskPropertyPage classes
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "nbpropertypages.h"
#include "nbpropertysheet.h"
#include "nbuihandler.h"
#include "ndasexception.h"
#include "ndas/ndasop.h"
#include "apperrdlg.h"
#include "appconf.h"

//////////////////////////////////////////////////////////////////////////
// Page 1
//////////////////////////////////////////////////////////////////////////
LRESULT CDiskPropertyPage1::OnInitDialog(HWND /*hWndFocus*/, LPARAM /*lParam*/)
{
	WTL::CString strCaption;
	strCaption.LoadString(IDS_DISKPROPERTYPAGE_CAPTION);
	GetParent().SetWindowText(strCaption);

	CDiskObjectPtr disk = GetParentSheet()->GetDiskObject();
	const CObjectUIHandler *phandler = CObjectUIHandler::GetUIHandler( disk );
	WTL::CString strText;

	if(disk->IsUnitDisk())
	{
		GetDlgItem(IDC_EDIT_NAME).SetWindowText( disk->GetTitle() );
		GetDlgItem(IDC_EDIT_ID).SetWindowText( 
			phandler->GetStringID(disk) );
	}
	else
	{
		GetDlgItem(IDC_EDIT_NAME).SetWindowText(phandler->GetTitle(disk));
		GetDlgItem(IDC_DEVICE_ID).ShowWindow(SW_HIDE);
		GetDlgItem(IDC_EDIT_ID).ShowWindow(SW_HIDE);
	}

	if ( (disk->GetAccessMask() & GENERIC_WRITE) != 0 )
	{
		strText.LoadString( IDS_DISKPROPERTYPAGE_WRITEKEY_PRESENT );
		GetDlgItem(IDC_EDIT_WRITEKEY).SetWindowText( strText );
	}
	else
	{
		strText.LoadString( IDS_DISKPROPERTYPAGE_WRITEKEY_NOT_PRESENT );
		GetDlgItem(IDC_EDIT_WRITEKEY).SetWindowText( strText );
	}
	WTL::CString strCapacity;
	strCapacity.FormatMessage( 
			IDS_DISKPROPERTYPAGE_SIZE_IN_GB,
			phandler->GetSizeInMB( disk ) / 1024,
			(phandler->GetSizeInMB( disk ) % 1024) / 10 
			);
	GetDlgItem(IDC_EDIT_CAPACITY).SetWindowText( strCapacity );

	//
	// If the object is composite disk with 2 mirrored disk of DIB V1,
	// display 'Migrate' button and message.
	//
	if(!disk->IsUnitDisk() && // bound disk
		disk->GetParent()->IsRoot() && // top disk
		2 == disk->GetDiskCount()) // 2 disks only
	{
		CDiskObjectCompositePtr pDiskObjectComposite = 
            boost::dynamic_pointer_cast<CDiskObjectComposite>(disk);
		if(NMT_MIRROR == pDiskObjectComposite->GetNDASMediaType())
		{
			GetDlgItem(IDC_TEXT_MIGRATE).ShowWindow( SW_SHOW );
			GetDlgItem(IDC_BTN_MIGRATE).ShowWindow( SW_SHOW );
			GetDlgItem(IDC_ST_MIGRATE).ShowWindow( SW_SHOW );
		}
	}

	return 0;
}

#define NUMBER_MIGRATE_DISK 2
void CDiskPropertyPage1::OnMigrate(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	UINT32 BindResult;
	CDiskObjectPtr disk = GetParentSheet()->GetDiskObject();
	NDASCOMM_CONNECTION_INFO pConnectionInfo[NUMBER_MIGRATE_DISK];
	UINT32 i;
	
	WTL::CString strTitle;
	strTitle.LoadString(IDS_APPLICATION);

	// Find aggregation root
	if (NUMBER_MIGRATE_DISK != disk->GetDiskCount())
	{
		WTL::CString strMsg;
		strMsg.LoadString( IDS_DISKPROPERTYPAGE_MIGRATE_DISK_NOT_EXIST );
		MessageBox(
			strMsg,
			strTitle,
			MB_OK | MB_ICONWARNING
			);
		return;
	}

	WTL::CString strConfirmMsg;
	strConfirmMsg.LoadString( IDS_DISKPROPERTYPAGE_MIGRATE_CONFIRM );
	if ( IDYES != MessageBox( 
					strConfirmMsg,
					strTitle,
					MB_YESNO | MB_ICONWARNING
					) 
		)
	{
		return;
	}

	if ( !disk->CanAccessExclusive() )
	{
		WTL::CString strMsg;
		strMsg.LoadString( IDS_FAIL_TO_ACCESS_EXCLUSIVELY );
		MessageBox( 
			strMsg,
			strTitle,
			MB_OK | MB_ICONWARNING
			);
		return;
	}

	CDiskObjectCompositePtr pDiskObjectComposite = 
		boost::dynamic_pointer_cast<CDiskObjectComposite>(disk);

	CDiskObjectComposite::const_iterator itr;
	for (itr = pDiskObjectComposite->begin(), i = 0; itr != pDiskObjectComposite->end(); ++itr, ++i)
	{
		CUnitDiskObjectPtr unitDisk = 
			boost::dynamic_pointer_cast<CUnitDiskObject>(*itr);
		
		pConnectionInfo[i].address_type = NDASCOMM_CONNECTION_INFO_TYPE_ADDR_LPX;
		pConnectionInfo[i].login_type = NDASCOMM_LOGIN_TYPE_NORMAL;
		pConnectionInfo[i].UnitNo = unitDisk->GetLocation()->GetUnitDiskLocation()->UnitNumber;
		pConnectionInfo[i].bWriteAccess = TRUE;
		pConnectionInfo[i].ui64OEMCode = NULL;
		pConnectionInfo[i].protocol = NDASCOMM_TRANSPORT_LPX;
		CopyMemory(pConnectionInfo[i].AddressLPX, 
			unitDisk->GetLocation()->GetUnitDiskLocation()->MACAddr,
			LPXADDR_NODE_LENGTH);
	}

	BindResult = NdasOpBind(NUMBER_MIGRATE_DISK, pConnectionInfo, NMT_RAID1);

	WTL :: CString strMsg;
	if(NUMBER_MIGRATE_DISK == BindResult)
	{
		strMsg.LoadString(IDS_DISKPROPERTYPAGE_MIGRATE_SUCCESS);
		WTL::CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);
		MessageBox(
			strMsg,
			strTitle,
			MB_OK|MB_ICONINFORMATION
			);

		GetDlgItem(IDC_TEXT_MIGRATE).ShowWindow( SW_HIDE );
		GetDlgItem(IDC_BTN_MIGRATE).ShowWindow( SW_HIDE );
		GetDlgItem(IDC_ST_MIGRATE).ShowWindow( SW_HIDE );
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
		case NDASOP_ERROR_NOT_BOUND_DISK: // does not return this error
			for (itr = pDiskObjectComposite->begin(), i = 0; itr != pDiskObjectComposite->end(); ++itr, ++i)
			{
				CUnitDiskObjectPtr unitDisk = 
					boost::dynamic_pointer_cast<CUnitDiskObject>(*itr);
				if(BindResult == i)
					strMsg.FormatMessage(IDS_DISKPROPERTYPAGE_MIGRATE_FAIL_AT_FMT, unitDisk->GetTitle());
			}

			break;
		default:
			strMsg.LoadString(IDS_DISKPROPERTYPAGE_MIGRATE_FAIL);
			break;
		}

		ShowErrorMessageBox(strMsg);
	}

	CNdasHIXChangeNotify HixChangeNotify(pGetNdasHostGuid());
	BOOL bResults = HixChangeNotify.Initialize();
	if(bResults)
	{
		for(i = 0; i < NUMBER_MIGRATE_DISK; i++)
		{
			NDAS_UNITDEVICE_ID unitDeviceId;
			CopyMemory(unitDeviceId.DeviceId.Node, pConnectionInfo[i].AddressLPX, 
				sizeof(unitDeviceId.DeviceId.Node));
			unitDeviceId.UnitNo = pConnectionInfo[i].UnitNo;
			HixChangeNotify.Notify(unitDeviceId);
		}
	}
}
//////////////////////////////////////////////////////////////////////////
// Page 2
//////////////////////////////////////////////////////////////////////////
LRESULT CDiskPropertyPage2::OnInitDialog(HWND /*hWndFocus*/, LPARAM /*lParam*/)
{
	WTL::CString strCaption;
	strCaption.LoadString(IDS_DISKPROPERTYPAGE_CAPTION);
	GetParentSheet()->SetWindowText(strCaption);

	CDiskObjectPtr disk = GetParentSheet()->GetDiskObject();

	m_listProperty.SubclassWindow( GetDlgItem(IDC_LIST_PROPERTY) );
	DWORD dwStyle = LVS_EX_FULLROWSELECT; 
	//| LVS_EX_GRIDLINES 
	//| LVS_EX_INFOTIP 
	m_listProperty.SetExtendedListViewStyle( dwStyle, dwStyle );

	WTL::CString strCol[2];
	strCol[0].LoadString( IDS_DISKPROPERTYPAGE_LIST_COL_NAME );
	strCol[1].LoadString( IDS_DISKPROPERTYPAGE_LIST_COL_VALUE );
	m_listProperty.InsertColumn( 0, strCol[0], LVCFMT_LEFT, 130, -1 );
	m_listProperty.InsertColumn( 1, strCol[1], LVCFMT_LEFT, 200, -1 );

	const CObjectUIHandler *phandler = CObjectUIHandler::GetUIHandler( disk );

	PropertyList propList = phandler->GetPropertyList( disk );
	PropertyList::iterator itr;
	for ( itr = propList.begin(); itr != propList.end(); ++itr )
	{
		m_listProperty.InsertItem( itr->strName, itr->strValue, itr->strToolTip );
	}

	return 0;
}

int CToolTipListCtrl::InsertItem(LPCTSTR szCol1, LPCTSTR szCol2, LPCTSTR szToolTip)
{
	int nItem;
	nItem = CWindowImpl<CToolTipListCtrl,CListViewCtrl>::InsertItem( 
				CWindowImpl<CToolTipListCtrl,CListViewCtrl>::GetItemCount(),
				szCol1
				);
	CWindowImpl<CToolTipListCtrl,CListViewCtrl>::SetItemText( nItem, 1, szCol2 );
	m_vtToolTip.push_back(WTL::CString(szToolTip));
	return nItem;
}

BOOL CToolTipListCtrl::SubclassWindow(HWND hWnd)
{
	BOOL bRet;
	bRet = CWindowImpl<CToolTipListCtrl,CListViewCtrl>::SubclassWindow(hWnd);
	SetExtendedListViewStyle( 
		LVS_EX_INFOTIP, LVS_EX_INFOTIP 
		);
	return bRet;
}

LRESULT CToolTipListCtrl::OnGetInfoTip(LPNMHDR lParam)
{
	NMLVGETINFOTIP *pnmlv = reinterpret_cast<NMLVGETINFOTIP*>(lParam);

	_tcsncpy(
			pnmlv->pszText,
			m_vtToolTip[pnmlv->iItem],
			pnmlv->cchTextMax-1
			);
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// Page 3
//////////////////////////////////////////////////////////////////////////
