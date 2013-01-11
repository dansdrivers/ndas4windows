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
#include "ndasinterface.h"

typedef struct _IDToStr 
{
	UINT nID;
	const TCHAR *szStr;
} IDToStr;
IDToStr IDToStrMap[] = 
{
	{ IDM_AGGR_BIND, _T("&Bind") },
	{ IDM_AGGR_UNBIND, _T("&Unbind") },
	{ IDM_AGGR_SYNCHRONIZE, _T("&Syncronize") },
	{ IDM_AGGR_REMIRROR, _T("&Reestablish mirror") },
	{ IDM_AGGR_ADDMIRROR, _T("&Add mirror") }
};

class CMenuIDToStringMap : public std::map<UINT, WTL::CString>
{
public:
	CMenuIDToStringMap()
	{
		// TODO : Change string table to string resource
		for ( int i=0; i < sizeof(IDToStrMap)/sizeof(IDToStrMap[0]); i++ )
		{
			insert( std::make_pair(IDToStrMap[i].nID, IDToStrMap[i].szStr) );
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
			IDI_ND_DISABLED,	
			IDI_ND_RO,		
			IDI_ND_RW,
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
					ILC_COLOR8|ILC_MASK,
					sizeof(anIconIDs)/sizeof(anIconIDs[0]),
					1);
	for ( int i=0; i < sizeof(anIconIDs)/sizeof(anIconIDs[0]); i++ )
	{
		HICON hIcon = ::LoadIcon( 
							_Module.GetResourceInstance(), 
							MAKEINTRESOURCE(anIconIDs[i]) 
							);
		/*
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

const CObjectUIHandler *CObjectUIHandler::GetUIHandler(const CDiskObject *o)
{
	// TODO : More sophisticated way of determining uihandler based on the
	//		  status of the disks is necessary.
	static CAggrDiskUIHandler aggrUIHandler;
	static CMirDiskUIHandler mirUIHandler;
	static CUnitDiskUIHandler unitDiskUIHandler;
	if ( dynamic_cast<const CAggrDiskObject*>(o) != NULL )
		return &aggrUIHandler;
	if ( dynamic_cast<const CMirDiskObject*>(o) != NULL )
		return &mirUIHandler;
	if ( dynamic_cast<const CUnitDiskObject*>(o) != NULL )
		return &unitDiskUIHandler;
	return &unitDiskUIHandler;		
}

WTL::CString CObjectUIHandler::GetTitle(CDiskObject *o) const
{
	return o->GetTitle();
}

void CObjectUIHandler::InsertMenu(CDiskObject *o, HMENU hMenu) const
{
	GetCommandSet(o).InsertMenu(hMenu);
}

UINT CObjectUIHandler::GetSizeInMB(CDiskObject *o) const
{
	_int64 nSize;
	nSize = o->GetUserSectorCount() / ( 1024 / BLOCK_SIZE )  / 1024;
	/* KB per sector */		/* MB per KB */
	return static_cast<UINT>(nSize);
}

WTL::CString CObjectUIHandler::GetStringID(CDiskObject *o) const
{
	WTL::CString strID = o->GetStringDeviceID();
	WTL::CString strDashedID;
	strID.Remove(_T('-'));

	int i;
	for ( i=0; i < strID.GetLength()-5; i+=5 )
	{
		strDashedID += strID.Mid(i, 5) + _T("-");
	}
	strDashedID += strID.Mid(i, strID.GetLength()-i);

	return strDashedID;

}
///////////////////////////////////////////////////////////////////////////////
// CAggrDiskUIHandler
///////////////////////////////////////////////////////////////////////////////
UINT CAggrDiskUIHandler::GetIconID(CDiskObject *o) const
{
	ATLASSERT( dynamic_cast<const CAggrDiskObject*>(o) != NULL );
	const CAggrDiskObject *pobj = dynamic_cast<const CAggrDiskObject*>(o);
	if ( pobj->IsUsable() )
	{
		return IDI_NDAGGR_OK;
	}
	else
	{
		return IDI_NDAGGR_BROKEN;
	}
}

CCommandSet CAggrDiskUIHandler::GetCommandSet(CDiskObject * o) const
{
	CCommandSet setCommand;

	setCommand.push_back( 
		CCommand(IDM_AGGR_UNBIND,o->GetAccessMask() & GENERIC_WRITE)
		);
	return setCommand;
}

///////////////////////////////////////////////////////////////////////////////
// CMirDiskUIHandler
///////////////////////////////////////////////////////////////////////////////
UINT CMirDiskUIHandler::GetIconID(CDiskObject *o) const
{
	ATLASSERT( dynamic_cast<const CMirDiskObject*>(o) != NULL );
	const CMirDiskObject *pobj = dynamic_cast<const CMirDiskObject*>(o);

	if ( pobj->IsBroken() )
	{
		return IDI_NDMIRR_BROKEN;
	}
	else
	{
		return IDI_NDMIRR_OK;
	}
}

CCommandSet CMirDiskUIHandler::GetCommandSet(CDiskObject *o) const
{
	ATLASSERT( dynamic_cast<const CMirDiskObject*>(o) != NULL );
	CCommandSet setCommand;
	BOOL bCanWrite;
	const CMirDiskObject *pobj = dynamic_cast<const CMirDiskObject*>(o);
	bCanWrite = o->GetAccessMask() & GENERIC_WRITE;
	setCommand.push_back( CCommand( IDM_AGGR_UNBIND, bCanWrite ) );
	setCommand.push_back( CCommand( IDM_AGGR_SYNCHRONIZE, 
							bCanWrite && pobj->IsDirty() && !pobj->IsBroken()) );
	setCommand.push_back( CCommand( IDM_AGGR_REMIRROR, 
							bCanWrite && pobj->IsBroken()) );
	return setCommand;
}

///////////////////////////////////////////////////////////////////////////////
// CUnitDiskUIHandler
///////////////////////////////////////////////////////////////////////////////
UINT CUnitDiskUIHandler::GetIconID(CDiskObject *o) const
{
	ATLASSERT( dynamic_cast<CUnitDiskObject*>(o) != NULL);
	CUnitDiskObject *pUnitDisk = dynamic_cast<CUnitDiskObject*>(o);
	CUnitDiskInfoHandlerPtr handler = pUnitDisk->GetInfoHandler();

	if ( !o->IsUsable() )
		return IDI_ND_NOEXIST;

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

CCommandSet CUnitDiskUIHandler::GetCommandSet(CDiskObject *o) const
{
	ATLASSERT( dynamic_cast<CUnitDiskObject*>(o) != NULL);
	CCommandSet setCommand;
	CUnitDiskObject *pUnitDisk = dynamic_cast<CUnitDiskObject*>(o);
	CUnitDiskInfoHandlerPtr handler = pUnitDisk->GetInfoHandler();
	BOOL bCanWrite;

	if ( handler->IsBound() )
	{
		CDiskObjectPtr aggrRoot = o->GetParent();
		while ( aggrRoot->GetParent()->IsRoot() )
			aggrRoot = aggrRoot->GetParent();
		// To Unbind, we should have write privilege to all the disks in bind
		setCommand.push_back( CCommand(IDM_AGGR_UNBIND, 
								aggrRoot->GetAccessMask() & GENERIC_WRITE) );
	}
	else
	{
		bCanWrite =  o->GetAccessMask() & GENERIC_WRITE;
		setCommand.push_back( CCommand(IDM_AGGR_ADDMIRROR, bCanWrite) );
	}
	if ( handler->IsMirrored() )
	{
		const CMirDiskObject *pParent = 
			dynamic_cast<const CMirDiskObject*>(o->GetParent().get());
		ATLASSERT( pParent != NULL );

		// To synchronize, we have write privilege to the two disks in mirroring
		setCommand.push_back( CCommand(IDM_AGGR_SYNCHRONIZE, 
								(pParent->GetAccessMask() & GENERIC_WRITE)
								&& handler->IsDirty() && !pParent->IsBroken() 
								) 
							);

		CDiskObjectPtr aggrRoot = o->GetParent();
		while ( aggrRoot->GetParent()->IsRoot() )
			aggrRoot = aggrRoot->GetParent();
		// To rebind, we should have write privilege to all the disks in bind
		setCommand.push_back( CCommand(IDM_AGGR_REMIRROR, 
								(aggrRoot->GetAccessMask() & GENERIC_WRITE)
								 && pParent->IsBroken()
								) 
							);
	}
	return setCommand;
}

