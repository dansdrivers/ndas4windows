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
#include "ndasutil.h"
#include "ndasinfohandler.h"
#include "ndasobject.h"
#include "ndaslanimpl.h"

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

void CHDDDiskInfoHandler::Bind(CUnitDiskObjectVector bindDisks, UINT nIndex, int nBindType)
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
void CHDDDiskInfoHandler::UnBind(CUnitDiskObjectPtr disk)
{
	m_pDiskSector->UnBind(disk);
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

void CHDDDiskInfoHandler::Initialize(CUnitDiskObjectPtr disk)
{
	if ( !m_pDiskSector->IsBlockV2() )
	{
		m_pDiskSector = 
			CDiskInfoSectorPtr( new CDIBV2Sector( (PNDASCOMM_UNIT_DEVICE_INFO)NULL ) );
	}
	CDIBV2Sector *pDIBV2Sector;
	pDIBV2Sector = dynamic_cast<CDIBV2Sector*>(m_pDiskSector.get());
	pDIBV2Sector->Initialize(disk);
}

BOOL CHDDDiskInfoHandler::HasBlockV2() const
{
	return m_pDiskSector->IsBlockV2();
}

WTL::CString CHDDDiskInfoHandler::GetModelName() const
{
	return m_pDiskSector->GetModelName();
}

WTL::CString CHDDDiskInfoHandler::GetSerialNo() const
{
	return m_pDiskSector->GetSerialNo();
}
