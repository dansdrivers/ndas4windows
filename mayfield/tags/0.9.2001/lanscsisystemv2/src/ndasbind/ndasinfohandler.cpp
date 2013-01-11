////////////////////////////////////////////////////////////////////////////
//
// Helper class that handles information of disk information
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include <map>
#include <algorithm> // for copy function

#include "windows.h"
#include "nbdefine.h"
#include "ndasutil.h"
#include "ndasinfohandler.h"
#include "ndasobject.h"
#include "ndasinterface.h"
#include "ndaslanimpl.h"
///////////////////////////////////////////////////////////////////////////////
// CUnitDiskInfoHandler
///////////////////////////////////////////////////////////////////////////////
/*
// function object used with 'find_if' and 'bind1st' 
// to find whether a disk object's location matches the location provided
class CDiskLocationEqual : public std::binary_function<CDiskLocation*, CDiskObjectPtr, BOOL>
{
public:
	BOOL operator() (first_argument_type first, second_argument_type second) const
	{
		ATLASSERT ( dynamic_cast<CUnitDiskObject*>(second.get()) != NULL );

		return first->Equal( 
				dynamic_cast<CUnitDiskObject*>(second.get())->GetLocation() );
	}
};

// function object used with 'find_if' and 'bind1st' 
// to find whether the location of the first disk in bind matches the location provided
class CFirstDiskLocationEqual : public std::binary_function<CDiskLocation*, CDiskObjectPtr, BOOL>
{
public:
	BOOL operator() (first_argument_type first, second_argument_type second) const
	{
		CUnitDiskObject *pObject;
		CUnitDiskInfoHandler *pHandler;
		ATLASSERT ( dynamic_cast<CUnitDiskObject*>(second.get()) != NULL );
		pObject = dynamic_cast<CUnitDiskObject*>(second.get());
		pHandler = dynamic_cast<CUnitDiskInfoHandler*>(pObject->GetInfoHandler());

		if ( !pHandler->IsAggregated() )
			return FALSE;
		return first->Equal( pHandler->GetFirstDiskLocationInAggr().get() );
	}
};

CDiskObjectList CUnitDiskInfoHandler::GetAggregatedDiskList(const CDiskObjectList listAllDisk) const
{
	CDiskObjectList listAggregated;
	CDiskObjectList::const_iterator found;

	found = listAllDisk.begin();
	do {
		found = std::find_if ( found, listAllDisk.end(), 
			std::bind1st( CFirstDiskLocationEqual(), GetFirstDiskLocationInAggr().get() ) 
			);
		if ( found != listAllDisk.end() )
		{
			listAggregated.push_back( *found );
			found++;
		}
	} while ( found != listAllDisk.end() );

	return listAggregated;
}

CDiskObjectPtr CUnitDiskInfoHandler::FindMirrorDisk(const CDiskObjectList listAllDisk) const
{
	ATLASSERT( IsMirrored() );
	CDiskObjectList::const_iterator found;

	found = std::find_if ( listAllDisk.begin(), listAllDisk.end(),
		std::bind1st( CDiskLocationEqual(), GetPeerLocation().get() )
		);
	if ( found == listAllDisk.end() )
	{
		return CDiskObjectPtr( new CNoExistDiskObject() );
	}
	return *found;
}
*/
///////////////////////////////////////////////////////////////////////////////
// CHDDDiskInfoHandler
///////////////////////////////////////////////////////////////////////////////
_int32 CHDDDiskInfoHandler::GetSectorsPerBit() const
{
	ATLASSERT( m_pDiskSector->IsBlockV2() );
	CDIBV2Sector *pDIBV2Sector;
	pDIBV2Sector = dynamic_cast<CDIBV2Sector*>(m_pDiskSector.get());
	return pDIBV2Sector->GetSectorsPerBit();
}

void CHDDDiskInfoHandler::Bind(CDiskObjectVector bindDisks, UINT nIndex, int nBindType)
{
	CDIBV2Sector *pDIBV2Sector;
	if ( m_pDiskSector->IsBlockV2() )
	{
		pDIBV2Sector = dynamic_cast<CDIBV2Sector*>(m_pDiskSector.get());
		pDIBV2Sector->Bind(bindDisks, nIndex, nBindType);
	}
	else
	{
		CDIBSector *pDIBSector = 
			dynamic_cast<CDIBSector*>(m_pDiskSector.get());
		pDIBV2Sector = new CDIBV2Sector( pDIBSector );
		m_pDiskSector = CDiskInfoSectorPtr( pDIBV2Sector );
		pDIBV2Sector->Bind(bindDisks, nIndex, nBindType);
	}
}
void CHDDDiskInfoHandler::UnBind()
{
	m_pDiskSector->UnBind();
	// TODO : We need to clean MBR if it's aggregated disk
}

void CHDDDiskInfoHandler::Rebind(CUnitDiskObjectPtr newDisk, UINT nIndex)
{
	// Block version 1 cannnot be rebound
	ATLASSERT( m_pDiskSector->IsBlockV2() );
	CDIBV2Sector *pDIBV2Sector;
	pDIBV2Sector = dynamic_cast<CDIBV2Sector*>(m_pDiskSector.get());
	pDIBV2Sector->Rebind(newDisk, nIndex);
}

void CHDDDiskInfoHandler::Mirror(CUnitDiskObjectPtr sourceDisk)
{
	CDIBV2Sector *pDIBV2Sector;
	if ( m_pDiskSector->IsBlockV2() )
	{
		pDIBV2Sector = dynamic_cast<CDIBV2Sector*>(m_pDiskSector.get());
	}
	else
	{
		CDIBSector *pDIBSector = 
			dynamic_cast<CDIBSector*>(m_pDiskSector.get());
		pDIBV2Sector = new CDIBV2Sector( pDIBSector );
		m_pDiskSector = CDiskInfoSectorPtr( pDIBV2Sector );
	}

	CHDDDiskInfoHandler *pSourceHandler = 
		dynamic_cast<CHDDDiskInfoHandler*>(sourceDisk->GetInfoHandler().get());
	ATLASSERT ( pSourceHandler->m_pDiskSector->IsBlockV2() );

	pDIBV2Sector->Mirror(
					dynamic_cast<CDIBV2Sector*>(pSourceHandler->m_pDiskSector.get())
					);
}

void CHDDDiskInfoHandler::SetDirty(BOOL bDirty)
{
	m_pDiskSector->SetDirty(bDirty);
}
void CHDDDiskInfoHandler::CommitDiskInfo(CSession *pSession, BOOL bSaveToDisk)
{
	if ( bSaveToDisk )
	{
		m_pDiskSector->WriteAccept( pSession );
	}
	else
	{
		m_pDiskSector->ReadAccept( pSession );
	}
}

