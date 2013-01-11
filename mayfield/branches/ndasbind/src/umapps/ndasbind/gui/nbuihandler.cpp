////////////////////////////////////////////////////////////////////////////
//
// classes that represent device & disk
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "resource.h"

#include <list>
#include <map>

#include "ndasobject.h"
#include "nbuihandler.h"
#include "nbseldiskdlg.h"
#include "nbpropertysheet.h"
#include "ndasexception.h"
#include "appconf.h"
#include "nbrecoverdlgs.h"
#include "ndas/ndasop.h"
#include "apperrdlg.h"

typedef struct _IDToStr 
{
	UINT nID;
	UINT nStrID;
} IDToStr;
IDToStr IDToStrMap[] = 
{
	{ IDM_AGGR_BIND, IDS_COMMAND_BIND },
	{ IDM_AGGR_UNBIND, IDS_COMMAND_UNBIND },
	{ IDM_AGGR_SINGLE, IDS_COMMAND_SINGLE },
	{ IDM_AGGR_SYNCHRONIZE, IDS_COMMAND_SYNCHRONIZE },
	{ IDM_AGGR_ADDMIRROR, IDS_COMMAND_ADDMIRROR },
	{ IDM_AGGR_CLEARDIB, IDS_COMMAND_CLEARDIB },
	{ IDM_AGGR_MIGRATE, IDS_COMMAND_MIGRATE },
	{ IDM_AGGR_REPAIR, IDS_COMMAND_REPAIR },
};

class CMenuIDToStringMap : public std::map<UINT, WTL::CString>
{
public:
	CMenuIDToStringMap()
	{
		for ( int i=0; i < sizeof(IDToStrMap)/sizeof(IDToStrMap[0]); i++ )
		{
			WTL::CString strMenu;
			strMenu.LoadString( IDToStrMap[i].nStrID );
			insert( std::make_pair(IDToStrMap[i].nID, strMenu) );
		}
	}
};

void CCommandSet::InsertMenu(HMENU hMenu)
{
	const_iterator itr;
	int nItemCount;
	CMenuHandle menu(hMenu);
	static CMenuIDToStringMap mapIDToStr;

	nItemCount = 0;
	for ( itr = begin(); itr!=end(); itr++ )
	{
		menu.InsertMenu(
				nItemCount++,
				MF_BYPOSITION,
				itr->GetID(),
				mapIDToStr[itr->GetID()]
				);
		if ( itr->IsDisabled() )
			menu.EnableMenuItem( itr->GetID(), MF_BYCOMMAND|MF_GRAYED );
	}
}

const UINT CObjectUIHandler::anIconIDs[] = {
			IDI_ND_NOEXIST,
//			IDI_ND_DISABLED,	
//			IDI_ND_RO,		
//			IDI_ND_RW,
			IDI_ND_INUSE,
			IDI_ND_BADKEY,
			IDI_NDAGGR_OK,
			IDI_NDAGGR_BROKEN,
			IDI_ND_SLAVE,
			IDI_NDMIRR_OK,
			IDI_NDMIRR_BROKEN
			};

CImageList CObjectUIHandler::GetImageList()
{
	CImageList imageList;
	imageList.Create( 
					64, 32,
//					16, 16,
					ILC_COLOR8|ILC_MASK,
					sizeof(anIconIDs)/sizeof(anIconIDs[0]),
					1);
	for ( int i=0; i < sizeof(anIconIDs)/sizeof(anIconIDs[0]); i++ )
	{
		HICON hIcon = ::LoadIcon( 
							_Module.GetResourceInstance(), 
							MAKEINTRESOURCE(anIconIDs[i]) 
							);
		/* Uncomment this if you want 32x32 icon.
		HICON hIcon = (HICON)::LoadImage(
								_Module.GetResourceInstance(), 
								MAKEINTRESOURCE(anIconIDs[i]), 
								IMAGE_ICON,
								32, 32, LR_DEFAULTCOLOR
								);
								*/
		imageList.AddIcon( hIcon );
	}
	return imageList;
}
UINT CObjectUIHandler::GetIconIndexFromID(UINT nID)
{
	for ( int i=0; i< sizeof(anIconIDs)/sizeof(anIconIDs[0]); i++ )
	{
		if ( nID == anIconIDs[i] )
			return i;
	}

	return 0;
}

const CObjectUIHandler *CObjectUIHandler::GetUIHandler(CDiskObjectPtr obj)
{
	// TODO : More sophisticated way of determining uihandler based on the
	//		  status of the disks is necessary.
	static CAggrDiskUIHandler aggrUIHandler;
	static CMirDiskUIHandler mirUIHandler;
	static CEmptyDiskUIHandler emptyUIHandler;
	static CRAID4DiskUIHandler raid4UIHandler;
	static CUnitDiskUIHandler unitDiskUIHandler;
	static CUnsupportedDiskUIHandler unsupportedDiskUIHandler;

	if ( dynamic_cast<const CAggrDiskObject*>(obj.get()) != NULL )
		return &aggrUIHandler;
	if ( dynamic_cast<const CMirDiskObject*>(obj.get()) != NULL )
		return &mirUIHandler;
	if ( dynamic_cast<const CRAID4DiskObject*>(obj.get()) != NULL )
		return &raid4UIHandler;
	if ( dynamic_cast<const CEmptyDiskObject*>(obj.get()) != NULL )
		return &emptyUIHandler;
	if ( dynamic_cast<const CUnitDiskObject*>(obj.get()) != NULL )
	{
		CUnitDiskObjectPtr unitDisk =
			boost::dynamic_pointer_cast<CUnitDiskObject>(obj);
		CUnitDiskInfoHandlerPtr infoHandler = unitDisk->GetInfoHandler();
		if ( infoHandler->HasValidInfo() )
			return &unitDiskUIHandler;
		else
			return &unsupportedDiskUIHandler;
	}
	return &unitDiskUIHandler;		
}

WTL::CString CObjectUIHandler::GetTitle(CDiskObjectPtr obj) const
{
	WTL::CString strTitle;

	if(obj->IsUnitDisk())
	{
		if(dynamic_cast<CEmptyDiskObject*>(obj.get()) != NULL)
		{
			strTitle.LoadString(IDS_UNIDEV_TYPE_DISK_EMPTY);
		}
		else
		{
			strTitle = obj->GetTitle();
		}
	}
	else
	{
		strTitle = GetType(obj);
	}

	return strTitle;
}

WTL::CString CObjectUIHandler::GetType(CDiskObjectPtr obj) const
{
	WTL::CString strTitle;

	if(obj->IsUnitDisk())
	{
		if(dynamic_cast<CEmptyDiskObject*>(obj.get()) != NULL)
		{
			strTitle.LoadString(IDS_UNIDEV_TYPE_DISK_EMPTY);
		}
		else
		{
			strTitle.LoadString(IDS_LOGDEV_TYPE_SINGLE_DISK);
		}
	}
	else
	{
		CDiskObjectCompositePtr pDiskObjectComposite = 
			boost::dynamic_pointer_cast<CDiskObjectComposite>(obj);


		switch(pDiskObjectComposite->GetNDASMediaType())
		{
		case NMT_SINGLE: strTitle.LoadString(IDS_LOGDEV_TYPE_SINGLE_DISK); break;
		case NMT_AGGREGATE: strTitle.LoadString(IDS_LOGDEV_TYPE_AGGREGATED_DISK); break;
		case NMT_MIRROR: strTitle.LoadString(IDS_LOGDEV_TYPE_MIRRORED_DISK); break;
		case NMT_RAID0: strTitle.LoadString(IDS_LOGDEV_TYPE_DISK_RAID0); break;
		case NMT_RAID1: strTitle.LoadString(IDS_LOGDEV_TYPE_DISK_RAID1); break;
		case NMT_RAID4: strTitle.LoadString(IDS_LOGDEV_TYPE_DISK_RAID4); break;
		case NMT_CDROM: strTitle.LoadString(IDS_LOGDEV_TYPE_DVD_DRIVE); break;
		case NMT_OPMEM: strTitle.LoadString(IDS_LOGDEV_TYPE_MO_DRIVE); break;
		case NMT_FLASH: strTitle.LoadString(IDS_LOGDEV_TYPE_CF_DRIVE); break;
		default:
			strTitle.FormatMessage(IDS_LOGDEV_TYPE_UNKNOWN_FMT, 
				pDiskObjectComposite->GetNDASMediaType());
		}
	}

	return strTitle;
}

WTL::CString CObjectUIHandler::GetFaultTolerance(CDiskObjectPtr obj) const
{
	WTL::CString strTitle;
	strTitle = _T("");

	if(obj->IsUnitDisk())
	{
		strTitle.LoadString(IDS_FT_UNIT);
	}
	else
	{
		CDiskObjectCompositePtr pDiskObjectComposite = 
			boost::dynamic_pointer_cast<CDiskObjectComposite>(obj);

		// check missing member
		if(pDiskObjectComposite->IsBroken())
		{
			strTitle.LoadString(IDS_FT_MISSING);
		}
		else
		{
			switch(pDiskObjectComposite->GetNDASMediaType())
			{
			case NMT_AGGREGATE:
			case NMT_RAID0:
			case NMT_MIRROR:
				if(pDiskObjectComposite->IsBroken())
					strTitle.LoadString(IDS_FT_MISSING);
				else
					strTitle.LoadString(IDS_FT_NOT_FAULT_TOLERANT);
				break;
			case NMT_RAID1:
				{
					CMirDiskObjectPtr pMirDiskObject = 
						boost::dynamic_pointer_cast<CMirDiskObject>(pDiskObjectComposite);
					if(pMirDiskObject->IsBroken())
						strTitle.LoadString(IDS_FT_NEED_REPAIR);
					else if(pMirDiskObject->IsDirty())
						strTitle.LoadString(IDS_FT_DIRTY);
					else
						strTitle.LoadString(IDS_FT_FAULT_TOLERANT);
				}
				break;
			case NMT_RAID4:
				{
					CRAID4DiskObjectPtr pRAID4DiskObject = 
						boost::dynamic_pointer_cast<CRAID4DiskObject>(pDiskObjectComposite);
					if(pRAID4DiskObject->IsBroken())
						strTitle.LoadString(IDS_FT_NEED_REPAIR);
					else if(pRAID4DiskObject->IsDirty())
						strTitle.LoadString(IDS_FT_DIRTY);
					else
						strTitle.LoadString(IDS_FT_FAULT_TOLERANT);
				}
				break;
			default:
				break;
			}
		}
	}

	return strTitle;
}

void CObjectUIHandler::InsertMenu(CDiskObjectPtr obj, HMENU hMenu) const
{
	GetCommandSet(obj).InsertMenu(hMenu);
}

UINT CObjectUIHandler::GetSizeInMB(CDiskObjectPtr obj) const
{
	_int64 nSize;
	nSize = obj->GetUserSectorCount() / ( 1024 / NDAS_BLOCK_SIZE )  / 1024;
	                                   /* KB per sector */		/* MB per KB */
	return static_cast<UINT>(nSize);
}

WTL::CString CObjectUIHandler::GetStringID(CDiskObjectPtr obj) const
{
	WTL::CString strID = obj->GetStringDeviceID();
	WTL::CString strDashedID;
	strID.Remove(_T('-'));

	strDashedID += 
		strID.Mid(0, 5) + _T("-") +
		strID.Mid(5, 5) + _T("-") +
		strID.Mid(10, 5) + _T("-") + _T("*****");
//		strID.Mid(0, 5) + _T("-") +

	return strDashedID;

}

///////////////////////////////////////////////////////////////////////////////
// CAggrDiskUIHandler
///////////////////////////////////////////////////////////////////////////////
UINT CAggrDiskUIHandler::GetIconID(CDiskObjectPtr obj) const
{
	ATLASSERT( dynamic_cast<const CAggrDiskObject*>(obj.get()) != NULL );
	CAggrDiskObjectPtr aggrDisk = 
		boost::dynamic_pointer_cast<CAggrDiskObject>(obj);
	if ( aggrDisk->IsUsable() )
	{
		return IDI_NDAGGR_OK;
	}
	else
	{
		return IDI_NDAGGR_BROKEN;
	}
}

CCommandSet CAggrDiskUIHandler::GetCommandSet(CDiskObjectPtr obj) const
{
	CCommandSet setCommand;

	setCommand.push_back( 
		CCommand(IDM_AGGR_UNBIND, obj->GetAccessMask() & GENERIC_WRITE)
		);
	return setCommand;
}

PropertyList CAggrDiskUIHandler::GetPropertyList(CDiskObjectPtr obj) const
{
	PropertyList propList;
	PropertyListItem propItem;
	WTL::CString strBuffer;

	//propItem[0] = _T("Binding type");
	propItem.strName.LoadString( IDS_UIHANDLER_PROPERTY_NUM_BOUND_DISK );
	strBuffer.Format( _T("%d"), obj->GetDiskCountInBind() );
	propItem.strValue = strBuffer;
	propItem.strToolTip.LoadString( IDS_UIHANDLER_PROPERTY_NUM_BOUND_DISK_TOOLTIP );
	propList.push_back( propItem );
	return propList;	
}

BOOL CAggrDiskUIHandler::OnCommand(CDiskObjectPtr obj, UINT nCommandID) const
{
	ATLASSERT( dynamic_cast<CAggrDiskObject*>(obj.get()) != NULL );
	switch( nCommandID )
	{
	case IDM_AGGR_PROPERTY:
		return OnProperty( obj );
	}
	return FALSE;
}

BOOL CAggrDiskUIHandler::OnProperty(CDiskObjectPtr obj) const
{
	CDiskPropertySheet sheet;
	sheet.SetDiskObject( obj );
	sheet.DoModal();
	return TRUE;
}

BOOL CAggrDiskUIHandler::IsValidDiskCount(UINT nDiskCount)
{
	return (nDiskCount >= 2 && nDiskCount <= NDAS_MAX_UNITS_IN_BIND);

}

///////////////////////////////////////////////////////////////////////////////
// CRAID0DiskUIHandler
///////////////////////////////////////////////////////////////////////////////
UINT CRAID0DiskUIHandler::GetIconID(CDiskObjectPtr obj) const
{
	ATLASSERT( dynamic_cast<const CRAID0DiskObject*>(obj.get()) != NULL );
	CRAID0DiskObjectPtr disk = 
		boost::dynamic_pointer_cast<CRAID0DiskObject>(obj);
	if ( disk->IsUsable() )
	{
		// AING_TO_DO : add IDI_BIND_OK, IDI_BIND_BROKEN
		return IDI_NDAGGR_OK;
	}
	else
	{
		return IDI_NDAGGR_BROKEN;
	}
}

CCommandSet CRAID0DiskUIHandler::GetCommandSet(CDiskObjectPtr obj) const
{
	CCommandSet setCommand;

	// AING_TO_DO : add IDM_RAID0_UNBIND
	setCommand.push_back( 
		CCommand(IDM_AGGR_UNBIND, obj->GetAccessMask() & GENERIC_WRITE)
		);
	return setCommand;
}

PropertyList CRAID0DiskUIHandler::GetPropertyList(CDiskObjectPtr obj) const
{
	PropertyList propList;
	PropertyListItem propItem;
	WTL::CString strBuffer;

	//propItem[0] = _T("Binding type");
	propItem.strName.LoadString( IDS_UIHANDLER_PROPERTY_NUM_BOUND_DISK );
	strBuffer.Format( _T("%d"), obj->GetDiskCountInBind() );
	propItem.strValue = strBuffer;
	propItem.strToolTip.LoadString( IDS_UIHANDLER_PROPERTY_NUM_BOUND_DISK_TOOLTIP );
	propList.push_back( propItem );
	return propList;	
}

BOOL CRAID0DiskUIHandler::OnCommand(CDiskObjectPtr obj, UINT nCommandID) const
{
	ATLASSERT( dynamic_cast<CAggrDiskObject*>(obj.get()) != NULL );
	switch( nCommandID )
	{
		// AING_TO_DO : add IDM_BIND_PROPERTY
	case IDM_AGGR_PROPERTY:
		return OnProperty( obj );
	}
	return FALSE;
}

BOOL CRAID0DiskUIHandler::OnProperty(CDiskObjectPtr obj) const
{
	CDiskPropertySheet sheet;
	sheet.SetDiskObject( obj );
	sheet.DoModal();
	return TRUE;
}

BOOL CRAID0DiskUIHandler::IsValidDiskCount(UINT nDiskCount)
{
	switch(nDiskCount)
	{
	case 2:
	case 4:
	case 8:
		return TRUE;
	default:
		return FALSE;
	}

	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
// CMirDiskUIHandler
///////////////////////////////////////////////////////////////////////////////
UINT CMirDiskUIHandler::GetIconID(CDiskObjectPtr obj) const
{
	ATLASSERT( dynamic_cast<const CMirDiskObject*>(obj.get()) != NULL );
	CMirDiskObjectPtr mirDisk = 
		boost::dynamic_pointer_cast<CMirDiskObject>(obj);

	if ( mirDisk->IsBroken() )
	{
		return IDI_NDMIRR_BROKEN;
	}
	else
	{
		return IDI_NDMIRR_OK;
	}
}

CCommandSet CMirDiskUIHandler::GetCommandSet(CDiskObjectPtr obj) const
{
	ATLASSERT( dynamic_cast<const CMirDiskObject*>(obj.get()) != NULL );
	CCommandSet setCommand;
	BOOL bCanWrite;
	CMirDiskObjectPtr mirDisk = 
		boost::dynamic_pointer_cast<CMirDiskObject>(obj);
	bCanWrite = mirDisk->GetAccessMask() & GENERIC_WRITE;
	setCommand.push_back( CCommand( IDM_AGGR_UNBIND, bCanWrite ) );
	if(NMT_MIRROR == mirDisk->GetNDASMediaType())
	{
		// migrate : mirror -> RAID 1
		setCommand.push_back( CCommand( IDM_AGGR_MIGRATE,
			!mirDisk->IsBroken() && mirDisk->HasWriteAccess()));
	}
	else
	{
		// do not check IsDirty here (cause it takes some time)
		setCommand.push_back( CCommand( IDM_AGGR_SYNCHRONIZE, 
			/* bCanWrite && */ mirDisk->IsDirty() && !mirDisk->IsBroken() && mirDisk->HasWriteAccess()));
	}
	return setCommand;
}

BOOL CMirDiskUIHandler::OnCommand(CDiskObjectPtr obj, UINT nCommandID) const
{
	ATLASSERT( dynamic_cast<CMirDiskObject*>(obj.get()) != NULL );
	switch ( nCommandID )
	{
	case IDM_AGGR_SYNCHRONIZE:
		return OnSynchronize( obj );
	case IDM_AGGR_PROPERTY:
		return OnProperty( obj );
	case IDM_AGGR_MIGRATE:
		return OnMigrate( obj );
	}
	return FALSE;
}

#define NUMBER_MIGRATE_DISK 2
BOOL CMirDiskUIHandler::OnMigrate(CDiskObjectPtr obj ) const
{
	CMirDiskObjectPtr mirDisk = 
		boost::dynamic_pointer_cast<CMirDiskObject>(obj);
	if(NMT_MIRROR != mirDisk->GetNDASMediaType())
		return FALSE;

	UINT32 BindResult;
	CDiskObjectPtr disk = obj;
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
			::GetFocus(),
			strMsg,
			strTitle,
			MB_OK | MB_ICONWARNING
			);
		return FALSE;
	}

	WTL::CString strConfirmMsg;
	strConfirmMsg.LoadString( IDS_DISKPROPERTYPAGE_MIGRATE_CONFIRM );
	if ( IDYES != MessageBox( 
					::GetFocus(),
					strConfirmMsg,
					strTitle,
					MB_YESNO | MB_ICONWARNING
					) 
		)
	{
		return FALSE;
	}

	if ( !disk->CanAccessExclusive() )
	{
		WTL::CString strMsg;
		strMsg.LoadString( IDS_FAIL_TO_ACCESS_EXCLUSIVELY );
		MessageBox( 
			::GetFocus(),
			strMsg,
			strTitle,
			MB_OK | MB_ICONWARNING
			);
		return FALSE;
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
			::GetFocus(),
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
	return TRUE;
}

BOOL CMirDiskUIHandler::OnSynchronize(CDiskObjectPtr obj) const
{
	ATLASSERT( dynamic_cast<CMirDiskObject*>(obj.get()) != NULL );
	
	CMirDiskObjectPtr mirDisk = 
		boost::dynamic_pointer_cast<CMirDiskObject>(obj);
	CUnitDiskObjectPtr sourceDisk, destDisk;
	BOOL bFirstDefected, bSecondDefected;
	BOOL bResults;
	
	bResults = mirDisk->GetDirtyDiskStatus(&bFirstDefected, &bSecondDefected);
	if(!bResults)
		return FALSE;

	// check dirty status here
//	ATLASSERT( mirDisk->GetDirtyDiskCount() > 0 );
	if ( !bFirstDefected && !bSecondDefected)
	{
		WTL::CString strMsg;
		strMsg.LoadString(IDS_WARNING_NOT_NEED_RECOVER );
		WTL::CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);
		MessageBox( 
			::GetFocus(),
			strMsg,
			strTitle,
			MB_OK | MB_ICONERROR
			);
		return TRUE;
	}
	
	//
	// Select the source disk and the dest disk
	//
	if ( bFirstDefected && bSecondDefected)
	{
		CSelectDiskDlg dlgSelect(IDD_SELSOURCE);
		CDiskObjectList listDisks;
		listDisks.push_back( mirDisk->front() );
		listDisks.push_back( mirDisk->back() );
		dlgSelect.SetSingleDisks( listDisks );
		if ( dlgSelect.DoModal() != IDOK )
			return TRUE;
		sourceDisk = dlgSelect.GetSelectedDisk();
		if ( sourceDisk == mirDisk->front() )
			destDisk = 
				boost::dynamic_pointer_cast<CUnitDiskObject>(mirDisk->back());
		else
			destDisk = 
				boost::dynamic_pointer_cast<CUnitDiskObject>(mirDisk->front());
	}
	else
	{
		if (bFirstDefected)
		{
			sourceDisk = 
				/*boost::dynamic_pointer_cast<CUnitDiskObject>*/ (mirDisk->SecondDisk());
			destDisk = 
			/*boost::dynamic_pointer_cast<CUnitDiskObject>*/ (mirDisk->FirstDisk());
		}
		else if (bSecondDefected )
		{
			sourceDisk = 
				/*boost::dynamic_pointer_cast<CUnitDiskObject>*/ (mirDisk->FirstDisk());
			destDisk = 
				/*boost::dynamic_pointer_cast<CUnitDiskObject>*/ (mirDisk->SecondDisk());
		}
		else
		{
			return FALSE;
		}
	}

	//
	// Synchronize
	//

	CRecoverDlg dlgRecover(FALSE, IDS_LOGDEV_TYPE_DISK_RAID1, IDS_RECOVERDLG_TASK_RECOVER);

	dlgRecover.SetMemberDevice(destDisk);
	dlgRecover.DoModal();

	CNdasHIXChangeNotify HixChangeNotify(pGetNdasHostGuid());
	bResults = HixChangeNotify.Initialize();
	if(bResults)
	{
		NDAS_UNITDEVICE_ID unitDeviceId;

		CopyMemory(unitDeviceId.DeviceId.Node,
			sourceDisk->GetLocation()->GetUnitDiskLocation()->MACAddr,
			sizeof(unitDeviceId.DeviceId.Node));
		unitDeviceId.UnitNo = 
			sourceDisk->GetLocation()->GetUnitDiskLocation()->UnitNumber;
		HixChangeNotify.Notify(unitDeviceId);

		CopyMemory(unitDeviceId.DeviceId.Node,
			destDisk->GetLocation()->GetUnitDiskLocation()->MACAddr,
			sizeof(unitDeviceId.DeviceId.Node));
		unitDeviceId.UnitNo = 
			destDisk->GetLocation()->GetUnitDiskLocation()->UnitNumber;
		HixChangeNotify.Notify(unitDeviceId);
	}
	

	return TRUE;
}

BOOL CMirDiskUIHandler::OnProperty(CDiskObjectPtr obj) const
{
	CDiskPropertySheet sheet;
	sheet.SetDiskObject( obj );
	sheet.DoModal();
	return TRUE;
}

PropertyList CMirDiskUIHandler::GetPropertyList(CDiskObjectPtr obj) const
{
	PropertyList propList;
	PropertyListItem propItem;
	WTL::CString strBuffer;
	CMirDiskObjectPtr mirDisk = 
		boost::dynamic_pointer_cast<CMirDiskObject>(obj);

	propItem.strName.LoadString( IDS_UIHANDLER_PROPERTY_DIRTY );
	if ( mirDisk->IsDirty() )
        propItem.strValue.LoadString( IDS_UIHANDLER_PROPERTY_DIRTY_TRUE );
	else
		propItem.strValue.LoadString( IDS_UIHANDLER_PROPERTY_DIRTY_FALSE );
	propItem.strToolTip.LoadString( IDS_UIHANDLER_PROPERTY_DIRTY_TOOLTIP );
	propList.push_back( propItem );
	return propList;	
}

BOOL CMirDiskUIHandler::IsValidDiskCount(UINT nDiskCount)
{
	return (nDiskCount >= 2 && nDiskCount <= NDAS_MAX_UNITS_IN_BIND && !(nDiskCount %2));

}

///////////////////////////////////////////////////////////////////////////////
// CRAID4DiskUIHandler
///////////////////////////////////////////////////////////////////////////////
UINT CRAID4DiskUIHandler::GetIconID(CDiskObjectPtr obj) const
{
	ATLASSERT( dynamic_cast<const CRAID4DiskObject*>(obj.get()) != NULL );
	CRAID4DiskObjectPtr disk = 
		boost::dynamic_pointer_cast<CRAID4DiskObject>(obj);
	if ( disk->IsUsable() )
	{
		// AING_TO_DO : add IDI_BIND_OK, IDI_BIND_BROKEN
		return IDI_NDAGGR_OK;
	}
	else
	{
		return IDI_NDAGGR_BROKEN;
	}
}

CCommandSet CRAID4DiskUIHandler::GetCommandSet(CDiskObjectPtr obj) const
{
	CCommandSet setCommand;
	BOOL bCanWrite;

	CRAID4DiskObjectPtr disk = 
		boost::dynamic_pointer_cast<CRAID4DiskObject>(obj);

	setCommand.push_back( 
		CCommand(IDM_AGGR_UNBIND, obj->GetAccessMask() & GENERIC_WRITE)
		);
	setCommand.push_back(
		CCommand(IDM_AGGR_SYNCHRONIZE, disk->IsDirty() && disk->HasWriteAccess()));

	setCommand.push_back(
		CCommand(IDM_AGGR_REPAIR, 1 == disk->GetMissingMemberCount()));

	return setCommand;
}

PropertyList CRAID4DiskUIHandler::GetPropertyList(CDiskObjectPtr obj) const
{
	PropertyList propList;
	PropertyListItem propItem;
	WTL::CString strBuffer;

	//propItem[0] = _T("Binding type");
	propItem.strName.LoadString( IDS_UIHANDLER_PROPERTY_NUM_BOUND_DISK );
	strBuffer.Format( _T("%d"), obj->GetDiskCountInBind() );
	propItem.strValue = strBuffer;
	propItem.strToolTip.LoadString( IDS_UIHANDLER_PROPERTY_NUM_BOUND_DISK_TOOLTIP );
	propList.push_back( propItem );
	return propList;	
}

BOOL CRAID4DiskUIHandler::OnCommand(CDiskObjectPtr obj, UINT nCommandID) const
{
	ATLASSERT( dynamic_cast<CRAID4DiskObject*>(obj.get()) != NULL );
	switch( nCommandID )
	{
	case IDM_AGGR_SYNCHRONIZE:
		return OnRecover( obj );
	case IDM_AGGR_PROPERTY:
		return OnProperty( obj );
	}
	return FALSE;
}

BOOL CRAID4DiskUIHandler::OnRecover(CDiskObjectPtr obj) const
{
	ATLASSERT( dynamic_cast<CRAID4DiskObject*>(obj.get()) != NULL );
	
	CRAID4DiskObjectPtr raid4Disk = 
		boost::dynamic_pointer_cast<CRAID4DiskObject>(obj);

	if(!raid4Disk->IsDirty())
	{
		WTL::CString strMsg;
		strMsg.LoadString(IDS_WARNING_NOT_NEED_RECOVER );
		WTL::CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);
		MessageBox( 
			::GetFocus(),
			strMsg,
			strTitle,
			MB_OK | MB_ICONERROR
			);
		return TRUE;
	}

	std::list<CDiskObjectPtr>::iterator it;
	CUnitDiskObjectPtr pUnitDisk;

	it = raid4Disk->begin();
	INT32 iDirty;
	for(iDirty = raid4Disk->GetDirtyDisk(); iDirty > 0; iDirty--)
		it++;

	pUnitDisk = boost::dynamic_pointer_cast<CUnitDiskObject>(*it);

	CRecoverDlg dlgRecover(FALSE, IDS_LOGDEV_TYPE_DISK_RAID4, IDS_RECOVERDLG_TASK_RECOVER);

	dlgRecover.SetMemberDevice(pUnitDisk);
	dlgRecover.DoModal();

	CNdasHIXChangeNotify HixChangeNotify(pGetNdasHostGuid());
	BOOL bResults = HixChangeNotify.Initialize();
	if(bResults)
	{
		NDAS_UNITDEVICE_ID unitDeviceId;

		for(it = raid4Disk->begin(); it != raid4Disk->end(); ++it)
		{
			pUnitDisk = boost::dynamic_pointer_cast<CUnitDiskObject>(*it);
			CopyMemory(unitDeviceId.DeviceId.Node,
				pUnitDisk->GetLocation()->GetUnitDiskLocation()->MACAddr,
				sizeof(unitDeviceId.DeviceId.Node));
			unitDeviceId.UnitNo = 
				pUnitDisk->GetLocation()->GetUnitDiskLocation()->UnitNumber;
			HixChangeNotify.Notify(unitDeviceId);
		}
	}

	return TRUE;
}

BOOL CRAID4DiskUIHandler::OnProperty(CDiskObjectPtr obj) const
{
	CDiskPropertySheet sheet;
	sheet.SetDiskObject( obj );
	sheet.DoModal();
	return TRUE;
}

BOOL CRAID4DiskUIHandler::IsValidDiskCount(UINT nDiskCount)
{
	switch(nDiskCount)
	{
	case 3:
	case 5:
	case 9:
		return TRUE;
	default:
		return FALSE;
	}

	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
// CUnitDiskUIHandler
///////////////////////////////////////////////////////////////////////////////
UINT CUnitDiskUIHandler::GetIconID(CDiskObjectPtr obj) const
{
	ATLASSERT(dynamic_cast<CUnitDiskObject*>(obj.get()) != NULL);
	CUnitDiskObjectPtr unitDisk = 
		boost::dynamic_pointer_cast<CUnitDiskObject>(obj);
	CUnitDiskInfoHandlerPtr handler = unitDisk->GetInfoHandler();

	if ( !obj->IsUsable() )
		return IDI_ND_BADKEY;

	if ( handler->IsHDD() )
	{
		if ( handler->IsBound() )
		{
			if ( handler->IsMaster() )
			{
				return IDI_ND_INUSE;
			}
			else
			{
				return IDI_ND_SLAVE;
			}
		}
		else
		{
			return IDI_ND_INUSE;
		}
	}
	else
	{
		// TODO : We need a new icon for this type(DVD, FLASH, MO.. ETC)
		return IDI_ND_INUSE;	
	}
}

CCommandSet CUnitDiskUIHandler::GetCommandSet(CDiskObjectPtr obj) const
{
	ATLASSERT( dynamic_cast<CUnitDiskObject*>(obj.get()) != NULL);
	CCommandSet setCommand;
	CUnitDiskObjectPtr unitDisk =
		boost::dynamic_pointer_cast<CUnitDiskObject>(obj);
	CUnitDiskInfoHandlerPtr handler = unitDisk->GetInfoHandler();
	BOOL bCanWrite;

	if ( handler->IsBound() )
	{
		CDiskObjectPtr aggrRoot = unitDisk->GetParent();
		if ( aggrRoot->IsRoot() )
		{
			// This can occur when the tree is updated just after
			// the disk is bound.
			// This additional if code prevents error.
			setCommand.push_back( CCommand(IDM_AGGR_UNBIND) ); 
		}
		else
		{
			while ( !aggrRoot->GetParent()->IsRoot() )
				aggrRoot = aggrRoot->GetParent();
			// To Unbind, we should have write privilege to all the disks in bind
			setCommand.push_back( CCommand(IDM_AGGR_UNBIND, 
				aggrRoot->GetAccessMask() & GENERIC_WRITE) );
		}
	}
	else
	{
		bCanWrite =  unitDisk->GetAccessMask() & GENERIC_WRITE;
		setCommand.push_back( CCommand(IDM_AGGR_ADDMIRROR, bCanWrite) );
	}

	if ( handler->IsMirrored() )
	{
		CMirDiskObjectPtr parent = 
			boost::dynamic_pointer_cast<CMirDiskObject>(unitDisk->GetParent());
		if ( parent.get() != NULL )
		{
			// To synchronize, we have write privilege to the two disks in mirroring
			setCommand.push_back( CCommand(IDM_AGGR_SYNCHRONIZE, 
									(parent->GetAccessMask() & GENERIC_WRITE) &&
									parent->IsDirty() && !parent->IsBroken() &&
									parent->HasWriteAccess()
									) 
								);
		}
		CDiskObjectPtr aggrRoot = unitDisk->GetParent();
		if ( aggrRoot->IsRoot() )
		{
			// This can occur when the tree is updated just after
			// the disk is bound.
			// This additional if code prevents error.
			setCommand.push_back( CCommand(IDM_AGGR_UNBIND) ); 
		}
		else
		{
			while ( !aggrRoot->GetParent()->IsRoot() )
				aggrRoot = aggrRoot->GetParent();
		}
	}
	else if(NMT_RAID4 == handler->GetNDASMediaType())
	{
		CRAID4DiskObjectPtr parent = 
			boost::dynamic_pointer_cast<CRAID4DiskObject>(unitDisk->GetParent());

		if ( parent.get() != NULL )
		{
			// To synchronize, we have write privilege to the two disks in mirroring
			setCommand.push_back( CCommand(IDM_AGGR_SYNCHRONIZE, 
				(parent->GetAccessMask() & GENERIC_WRITE)
				&& parent->IsDirty() && !parent->IsBroken() 
				) 
				);
		}
	}

	if(unitDisk->IsUnitDisk())
	{
		setCommand.push_back( CCommand(IDM_AGGR_SINGLE) );
	}
	return setCommand;
}

BOOL CUnitDiskUIHandler::OnCommand(CDiskObjectPtr obj, UINT nCommandID) const
{
	ATLASSERT( dynamic_cast<CUnitDiskObject*>(obj.get()) != NULL);

	switch( nCommandID )
	{
	case IDM_AGGR_SYNCHRONIZE:
		{
			// Delegate command to parent
			const CObjectUIHandler *phandler = 
				CObjectUIHandler::GetUIHandler( obj->GetParent() );
			return phandler->OnCommand( obj->GetParent(), nCommandID );
		}
	case IDM_AGGR_PROPERTY:
		return OnProperty( obj );
	}

	return FALSE;
}

BOOL CUnitDiskUIHandler::OnProperty(CDiskObjectPtr obj) const
{

	CDiskPropertySheet sheet;
	sheet.SetDiskObject( obj );
	sheet.DoModal();
	return TRUE;
}

PropertyList CUnitDiskUIHandler::GetPropertyList(CDiskObjectPtr obj) const
{
	PropertyList propList;
	PropertyListItem propItem;
	WTL::CString strBuffer;
	CUnitDiskObjectPtr unitDisk = 
		boost::dynamic_pointer_cast<CUnitDiskObject>(obj);
	CUnitDiskInfoHandlerPtr handler = unitDisk->GetInfoHandler();
	CHDDDiskInfoHandler *pHDDHandler = 
		dynamic_cast<CHDDDiskInfoHandler*>(handler.get());
	if ( pHDDHandler != NULL )
	{
		// TODO : String resources
		propItem.strName.LoadString( IDS_UIHANDLER_PROPERTY_MODEL );
		propItem.strValue = pHDDHandler->GetModelName();
		propItem.strToolTip.LoadString( IDS_UIHANDLER_PROPERTY_MODEL_TOOLTIP );
		propList.push_back( propItem );

		propItem.strName.LoadString( IDS_UIHANDLER_PROPERTY_SERIALNO );
		propItem.strValue = pHDDHandler->GetSerialNo();
		propItem.strToolTip.LoadString( IDS_UIHANDLER_PROPERTY_SERIALNO_TOOLTIP );
		propList.push_back( propItem );
	}
	return propList;
}

BOOL CUnitDiskUIHandler::IsValidDiskCount(UINT nDiskCount)
{
	return (nDiskCount == 1);

}

///////////////////////////////////////////////////////////////////////////////
// CUnsupportedDiskUIHandler
///////////////////////////////////////////////////////////////////////////////
CCommandSet CUnsupportedDiskUIHandler::GetCommandSet(CDiskObjectPtr obj) const
{
	ATLASSERT( dynamic_cast<CUnitDiskObject*>(obj.get()) != NULL);
	CCommandSet setCommand;

	setCommand.push_back ( CCommand( IDM_AGGR_CLEARDIB, 
							(obj->GetAccessMask() & GENERIC_WRITE)
							) 
						 );
	return setCommand;							
}

BOOL CUnsupportedDiskUIHandler::OnProperty(CDiskObjectPtr obj) const
{
	CUnsupportedDiskPropertySheet sheet;
	sheet.DoModal();
	return TRUE;
}

BOOL CUnsupportedDiskUIHandler::OnClearDIB(CDiskObjectPtr obj) const
{
	ATLASSERT( dynamic_cast<CUnitDiskObject*>(obj.get()) != NULL);

	WTL::CString strWarning;
	strWarning.FormatMessage( 
			IDS_UIHANDLER_CLEARDIB_CONFIRM,
			obj->GetTitle()
			);
	WTL::CString strTitle;
	strTitle.LoadString(IDS_APPLICATION);
	if ( MessageBox(::GetFocus(), strWarning, strTitle, MB_YESNO | MB_ICONWARNING) != IDYES )
	{
		return TRUE;
	}
	CUnitDiskObjectPtr unitDisk = 
		boost::dynamic_pointer_cast<CUnitDiskObject>(obj);
	if ( !unitDisk->CanAccessExclusive() )
	{
		WTL::CString strMsg;
		strMsg.LoadString( IDS_FAIL_TO_ACCESS_EXCLUSIVELY );
		MessageBox( 
			::GetFocus(),
			strMsg,
			strTitle,
			MB_OK | MB_ICONWARNING
			);
		return TRUE;
	}
	try {
		unitDisk->Open( TRUE );
        unitDisk->Initialize( unitDisk );
		unitDisk->CommitDiskInfo();
		unitDisk->Close();
	}
	catch( CNDASException & )
	{
		unitDisk->Close();
		WTL::CString strMsg;
		strMsg.LoadString ( IDS_UIHANDLER_CLEARDIB_FAIL );
		MessageBox( 
			::GetFocus(),
			strMsg,
			strTitle,
			MB_OK | MB_ICONERROR
			);
		return TRUE;
	}
	WTL::CString strMsg;
	strMsg.LoadString( IDS_UIHANDLER_CLEARDIB_SUCCESS );
	return TRUE;
}

BOOL CUnsupportedDiskUIHandler::OnCommand(CDiskObjectPtr obj, UINT nCommandID) const
{
	switch( nCommandID )
	{
	case IDM_AGGR_CLEARDIB:
		return OnClearDIB( obj );
	case IDM_AGGR_PROPERTY:
		return OnProperty( obj );
	}
	return FALSE;
}

BOOL CUnsupportedDiskUIHandler::IsValidDiskCount(UINT nDiskCount)
{
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
// CEmptyDiskUIHandler
///////////////////////////////////////////////////////////////////////////////
UINT CEmptyDiskUIHandler::GetIconID(CDiskObjectPtr obj) const
{
	ATLASSERT(dynamic_cast<CEmptyDiskObject*>(obj.get()) != NULL);

	return IDI_ND_BADKEY;
}

CCommandSet CEmptyDiskUIHandler::GetCommandSet(CDiskObjectPtr obj) const
{
	ATLASSERT( dynamic_cast<CEmptyDiskObject*>(obj.get()) != NULL);
	CCommandSet setCommand;
	CEmptyDiskObjectPtr unitDisk =
		boost::dynamic_pointer_cast<CEmptyDiskObject>(obj);

	return setCommand;
}

BOOL CEmptyDiskUIHandler::OnCommand(CDiskObjectPtr obj, UINT nCommandID) const
{
	ATLASSERT( dynamic_cast<CEmptyDiskObject*>(obj.get()) != NULL);

	switch( nCommandID )
	{
	case IDM_AGGR_PROPERTY:
		return OnProperty( obj );
	}

	return FALSE;
}

BOOL CEmptyDiskUIHandler::OnProperty(CDiskObjectPtr obj) const
{

	CDiskPropertySheet sheet;
	sheet.SetDiskObject( obj );
	sheet.DoModal();
	return TRUE;
}

PropertyList CEmptyDiskUIHandler::GetPropertyList(CDiskObjectPtr obj) const
{
	PropertyList propList;
	PropertyListItem propItem;

	// TODO : String resources
	propItem.strName.LoadString( IDS_UIHANDLER_PROPERTY_MODEL );
	propItem.strValue.LoadString(IDS_UNIDEV_TYPE_DISK_EMPTY);
	propItem.strToolTip.LoadString(IDS_UNIDEV_TYPE_DISK_EMPTY);
	propList.push_back( propItem );

	return propList;
}

BOOL CEmptyDiskUIHandler::IsValidDiskCount(UINT nDiskCount)
{
	return TRUE;
}

WTL::CString CEmptyDiskUIHandler::GetStringID(CDiskObjectPtr obj) const
{
	WTL::CString strDashedID;
	strDashedID.LoadString(IDS_UNIDEV_TYPE_DISK_EMPTY);

	return strDashedID;

}
