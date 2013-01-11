////////////////////////////////////////////////////////////////////////////
//
// Implementation of CDiskSector class
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ndassector.h"
#include "ndaslanimpl.h"
#include "ndas/ndasop.h"
#include "scrc32.h"

///////////////////////////////////////////////////////////////////////////////
// CDiskMultiSector
///////////////////////////////////////////////////////////////////////////////
CDiskMultiSector::CDiskMultiSector()
	: m_nCount(1)
{
}
CDiskMultiSector::CDiskMultiSector(UINT nSectorCount)
	: m_nCount(static_cast<_int16>(nSectorCount))
{
	// 
	// When m_nCount is 1, member variables are initialized by 
	// the parent class 'CDiskSector'
	if ( m_nCount > 1)
	{
		m_dataExpanded = boost::shared_ptr<_int8>(new _int8[NDAS_BLOCK_SIZE*m_nCount]);
		::ZeroMemory( m_dataExpanded.get(), NDAS_BLOCK_SIZE*m_nCount);
		m_pdata = m_dataExpanded.get();
	}
}
void CDiskMultiSector::Resize(UINT nSectorCount)
{
	if ( nSectorCount == static_cast<UINT>(m_nCount) ) return;
	
	// Allocate a new buffer
	_int8 *pNewData = new _int8[NDAS_BLOCK_SIZE*nSectorCount];
	// Initialize the new buffer and copy data from the old buffer
	::ZeroMemory( pNewData, NDAS_BLOCK_SIZE*nSectorCount );
	if ( nSectorCount > (UINT)m_nCount )
		::CopyMemory( pNewData, m_pdata, NDAS_BLOCK_SIZE*m_nCount);
	else
		::CopyMemory( pNewData, m_pdata, NDAS_BLOCK_SIZE*nSectorCount);
	// Change status variables's values.
	m_nCount = static_cast<_int16>(nSectorCount);
	m_dataExpanded = boost::shared_ptr<_int8>(pNewData);
	m_pdata = m_dataExpanded.get();
}


///////////////////////////////////////////////////////////////////////////////
// CDiskInfoSector
///////////////////////////////////////////////////////////////////////////////
_int64 CDiskInfoSector::GetTotalSectorCount() const
{
	return m_UnitDeviceInfo.SectorCount;

}

///////////////////////////////////////////////////////////////////////////////
// CDIBSector
///////////////////////////////////////////////////////////////////////////////
WTL::CString CDiskInfoSector::GetModelName() const
{
	char buffer[sizeof( m_UnitDeviceInfo.Model )+1];
	::CopyMemory( buffer, m_UnitDeviceInfo.Model, sizeof(m_UnitDeviceInfo.Model) );
	buffer[sizeof(m_UnitDeviceInfo.Model)] = '\0';
	return WTL::CString( buffer );
}

WTL::CString CDiskInfoSector::GetSerialNo() const
{
	char buffer[sizeof( m_UnitDeviceInfo.SerialNo )+1];
	::CopyMemory( buffer, m_UnitDeviceInfo.SerialNo, sizeof(m_UnitDeviceInfo.SerialNo) );
	buffer[sizeof(m_UnitDeviceInfo.SerialNo)] = '\0';

	WTL::CString strSerialNo;
	strSerialNo = buffer;
	return strSerialNo;
}

///////////////////////////////////////////////////////////////////////////////
// CDIBSector
///////////////////////////////////////////////////////////////////////////////

CDIBSector::CDIBSector(const PNDASCOMM_UNIT_DEVICE_INFO pUnitDeviceInfo)
: CTypedDiskInfoSector<NDAS_DIB>(pUnitDeviceInfo)
{
	NDAS_DIB *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB>::get();

	pDIB->Signature = NDAS_DIB_SIGNATURE;
	pDIB->MajorVersion = NDAS_DIB_VERSION_MAJOR;
	pDIB->MinorVersion = NDAS_DIB_VERSION_MINOR;

}

BOOL CDIBSector::IsValid() const
{
	// Check version
	const NDAS_DIB *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB>::get();
	if ( IS_NDAS_DIBV1_WRONG_VERSION( *pDIB ) )
		return FALSE;
	// Check sequence
	if ( pDIB->Sequence > 1 )
		return FALSE;
	// check disk type
	if ( pDIB->DiskType == NDAS_DIB_DISK_TYPE_INVALID )
		return FALSE;
	// check usage type : ignored
	return TRUE;
}

BOOL CDIBSector::IsValidSignature() const
{
	return get()->Signature == NDAS_DIB_V2_SIGNATURE;
}


BOOL CDIBSector::IsBound() const 
{
	return IsBoundAndNotSingleMirrored() || IsMirrored();
}

BOOL CDIBSector::IsMirrored() const
{
	const NDAS_DIB *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB>::get();
	BOOL bMirrored = FALSE;
	// For NDAS_DIB, 
	// we need to check disk type to see whether it is mirrored.
	switch( pDIB->DiskType )
	{
	case NDAS_DIB_DISK_TYPE_MIRROR_MASTER:
	case NDAS_DIB_DISK_TYPE_MIRROR_SLAVE:
		bMirrored = TRUE;
		break;
	default:
		break;
	}
	return bMirrored;
}

BOOL CDIBSector::IsBoundAndNotSingleMirrored() const
{
	const NDAS_DIB *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB>::get();
	BOOL bAggr = FALSE;
	// For NDAS_DIB, 
	// we need to check disk type to see whether it is mirrored.
	switch( pDIB->DiskType )
	{
	case NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST:
	case NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND:
	case NDAS_DIB_DISK_TYPE_AGGREGATION_THIRD:
	case NDAS_DIB_DISK_TYPE_AGGREGATION_FOURTH:
		bAggr = TRUE;
		break;
	default:
		break;
	}
	return bAggr;
}

BOOL CDIBSector::IsMaster() const
{
	const NDAS_DIB *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB>::get();
	if ( IsBoundAndNotSingleMirrored() )
		return (pDIB->DiskType == NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST);
	else if ( IsMirrored() )
		return (pDIB->DiskType == NDAS_DIB_DISK_TYPE_MIRROR_MASTER);
	else
		return TRUE;
}

UINT32 CDIBSector::GetNDASMediaType() const
{
	const NDAS_DIB *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB>::get();

	switch(pDIB->DiskType)
	{
	case NDAS_DIB_DISK_TYPE_MIRROR_MASTER:
	case NDAS_DIB_DISK_TYPE_MIRROR_SLAVE:
		return NMT_MIRROR;
	case NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST:
	case NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND:
	case NDAS_DIB_DISK_TYPE_AGGREGATION_THIRD:
	case NDAS_DIB_DISK_TYPE_AGGREGATION_FOURTH:
		return NMT_AGGREGATE;
	case NDAS_DIB_DISK_TYPE_SINGLE:
	case NDAS_DIB_DISK_TYPE_VDVD:
	case NDAS_DIB_DISK_TYPE_INVALID:
	case NDAS_DIB_DISK_TYPE_BIND:
	default:
		return NMT_SINGLE;
	}
}

CDiskLocationVector CDIBSector::GetBoundDiskLocations(CDiskLocationPtr thisLocation) const
{
	//
	// NOTE : Previous version which uses NDAS_DIB(ver1)
	//		allows only 2 disks to be bound.
	//		( NDAS_DIB_DISK_TYPE_AGGREGATION_THIRD,
	//		  NDAS_DIB_DISK_TYPE_AGGREGATION_FOURTH are not used.)
	//
	const NDAS_DIB *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB>::get();
	CDiskLocationVector vtLocation;
	UNIT_DISK_LOCATION udlPeer;
	::ZeroMemory( &udlPeer, sizeof(UNIT_DISK_LOCATION) );

	ATLASSERT( IsBound() );

	::CopyMemory( &udlPeer.MACAddr, pDIB->PeerAddress, sizeof(udlPeer.MACAddr) );
	udlPeer.UnitNumber = pDIB->PeerUnitNumber;
	switch ( pDIB->DiskType )
	{
	case NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST:
	case NDAS_DIB_DISK_TYPE_MIRROR_MASTER:
		vtLocation.push_back( thisLocation );
		vtLocation.push_back( CDiskLocationPtr(new CLanDiskLocation(&udlPeer)) );
		break;
	case NDAS_DIB_DISK_TYPE_MIRROR_SLAVE:
	case NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND:
	case NDAS_DIB_DISK_TYPE_AGGREGATION_THIRD:
	case NDAS_DIB_DISK_TYPE_AGGREGATION_FOURTH:
	default:
		vtLocation.push_back( CDiskLocationPtr(new CLanDiskLocation(&udlPeer)) );
		vtLocation.push_back( thisLocation );
		break;
	}

	return vtLocation;
}
CDiskLocationPtr CDIBSector::GetPeerLocation() const
{
	const NDAS_DIB *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB>::get();
	//
	// Peer disk's location is stored in PeerAddress field
	//
	UNIT_DISK_LOCATION udlPeer;
	::ZeroMemory( &udlPeer, sizeof(UNIT_DISK_LOCATION) );
	::CopyMemory( &udlPeer.MACAddr, pDIB->PeerAddress, sizeof(udlPeer.MACAddr) );
	udlPeer.UnitNumber = pDIB->PeerUnitNumber;

	return CDiskLocationPtr( new CLanDiskLocation( &udlPeer ) );
}

UINT CDIBSector::GetPosInBind() const
{
	const NDAS_DIB *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB>::get();
	ATLASSERT( IsBound() );
	switch( pDIB->DiskType )
	{
	case NDAS_DIB_DISK_TYPE_MIRROR_MASTER:
		return 0;
	case NDAS_DIB_DISK_TYPE_MIRROR_SLAVE:
		return 1;
	case NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST:
		return 0;
	case NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND:
		return 1;
	case NDAS_DIB_DISK_TYPE_AGGREGATION_THIRD:
		return 2;
	case NDAS_DIB_DISK_TYPE_AGGREGATION_FOURTH:
		return 3;
	default:
		ATLASSERT(FALSE);
		break;
	}
	return 0;
}

UINT CDIBSector::GetDiskCountInBind() const
{
	ATLASSERT( IsBound() );
	return 2;
}

_int64 CDIBSector::GetUserSectorCount() const
{
	return ::CalcUserSectorCount( GetTotalSectorCount() , MIN_SECTORS_PER_BIT);
}

void CDIBSector::UnBind(CUnitDiskObjectPtr /*disk*/)
{
	NDAS_DIB *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB>::get();
	pDIB->DiskType = NDAS_DIB_DISK_TYPE_SINGLE;
	::ZeroMemory(pDIB->PeerAddress, sizeof(pDIB->PeerAddress));
	pDIB->PeerUnitNumber = 0;
}

///////////////////////////////////////////////////////////////////////////////
// CDIBV2Sector
///////////////////////////////////////////////////////////////////////////////
CDIBV2Sector::CDIBV2Sector(const PNDASCOMM_UNIT_DEVICE_INFO pUnitDeviceInfo)
: CTypedDiskInfoSector<NDAS_DIB_V2>(pUnitDeviceInfo),
  m_nCount(1), m_dataExpanded()
{
	NDAS_DIB_V2 *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();

	pDIB->Signature = NDAS_DIB_V2_SIGNATURE;
	pDIB->MajorVersion = NDAS_DIB_VERSION_MAJOR_V2;
	pDIB->MinorVersion = NDAS_DIB_VERSION_MINOR_V2;
}

CDIBV2Sector::CDIBV2Sector(const CDIBSector *pDIBSector)
: CTypedDiskInfoSector<NDAS_DIB_V2>((PNDASCOMM_UNIT_DEVICE_INFO)&pDIBSector->m_UnitDeviceInfo),
  m_nCount(1), m_dataExpanded()
{
	NDAS_DIB_V2 *pDIBV2 = 
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();

	pDIBV2->Signature = NDAS_DIB_V2_SIGNATURE;
	pDIBV2->MajorVersion = NDAS_DIB_VERSION_MAJOR_V2;
	pDIBV2->MinorVersion = NDAS_DIB_VERSION_MINOR_V2;
}

BOOL CDIBV2Sector::IsValid() const
{
	// 
	const NDAS_DIB_V2 *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();
	// Check version
	if ( IS_HIGHER_VERSION_V2(*pDIB) )
		return FALSE;
	// Check iSectorsPerBit
	if ( (pDIB->iSectorsPerBit % 128) != 0 )
	// Check nDiskCount & iSequence
	if ( pDIB->iSequence >= pDIB->nDiskCount + pDIB->nSpareCount)
	{
		return FALSE;
	}
	// TODO : check nDiskCount is 1 when the disk is not bound
	
	return TRUE;
}

BOOL CDIBV2Sector::IsValidSignature() const
{
	const NDAS_DIB_V2 *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();
	return pDIB->Signature == NDAS_DIB_V2_SIGNATURE;
}

void CDIBV2Sector::ReadAccept(CSession *pSession)
{
	UINT32 nDataSize = NDAS_BLOCK_SIZE;
	NdasOpReadDIB(pSession->GetHandle(), (NDAS_DIB_V2 *)m_pdata, &nDataSize);
	return;
}

void CDIBV2Sector::WriteAccept(CSession *pSession)
{
	for ( int i=0; i< m_nCount; i++ )
	{
		pSession->Write( GetLocation()-i, 1, m_pdata );
	}
}

BOOL CDIBV2Sector::IsBound() const 
{
	const NDAS_DIB_V2 *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();
	return ( pDIB->nDiskCount > 1 );
}
BOOL CDIBV2Sector::IsMirrored() const
{
	const NDAS_DIB_V2 *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();
	return ( pDIB->iMediaType == NMT_RAID1 || pDIB->iMediaType == NMT_MIRROR );
}

BOOL CDIBV2Sector::IsBoundAndNotSingleMirrored() const
{
	const NDAS_DIB_V2 *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();
	if ( !IsMirrored() )
	{
		return IsBound();
	}
	else
	{
		return ( pDIB->nDiskCount > 2);
	}
}
BOOL CDIBV2Sector::IsMaster() const
{
	const NDAS_DIB_V2 *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();
	if ( IsMirrored() )
		return ( (pDIB->iSequence & 0x01) == 0);
	else
		return (pDIB->iSequence == 0);
}


UINT32 CDIBV2Sector::GetNDASMediaType() const
{
	const NDAS_DIB_V2 *pDIB =
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();

	return pDIB->iMediaType;
}

BOOL CDIBV2Sector::IsPeerDirty() const
{
	const NDAS_DIB_V2 *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();
	
	CLanSession session(
		pDIB->UnitDisks[pDIB->iSequence].MACAddr, 
		pDIB->UnitDisks[pDIB->iSequence].UnitNumber);

	session.Connect(FALSE);

	BOOL bResults;
	BOOL bIsClean;
	bResults = NdasOpIsBitmapClean(session.GetHandle(), &bIsClean);

	session.Disconnect();

	if(bResults)
		return !bIsClean;
	else
		return FALSE;
}

BOOL CDIBV2Sector::GetLastWrittenSectorInfo(PLAST_WRITTEN_SECTOR pLWS) const
{
	const NDAS_DIB_V2 *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();

	CLanSession session(
		pDIB->UnitDisks[pDIB->iSequence].MACAddr, 
		pDIB->UnitDisks[pDIB->iSequence].UnitNumber);

	session.Connect(FALSE);

	BOOL bResults;
	BOOL bIsClean;
	bResults = NdasOpGetLastWrttenSectorInfo(session.GetHandle(), pLWS);

	session.Disconnect();

	return bResults;
}

BOOL CDIBV2Sector::GetLastWrittenSectorsInfo(PLAST_WRITTEN_SECTORS pLWS) const
{
	const NDAS_DIB_V2 *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();

	CLanSession session(
		pDIB->UnitDisks[pDIB->iSequence].MACAddr, 
		pDIB->UnitDisks[pDIB->iSequence].UnitNumber);

	session.Connect(FALSE);

	BOOL bResults;
	BOOL bIsClean;
	bResults = NdasOpGetLastWrttenSectorsInfo(session.GetHandle(), pLWS);

	session.Disconnect();

	return bResults;
}

void CDIBV2Sector::SetDirty(BOOL bDirty)
{
/*
	NDAS_DIB_V2 *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();
	if ( bDirty )
		pDIB->FlagDirty = NDAS_DIRTY_MIRROR_DIRTY;
	else
		pDIB->FlagDirty = 0;
*/
	return;
}

_int64 CDIBV2Sector::GetUserSectorCount() const
{
	const NDAS_DIB_V2 *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();
	if ( pDIB->sizeUserSpace == 0 )
		return ::CalcUserSectorCount( GetTotalSectorCount(), pDIB->iSectorsPerBit);
	else
		return pDIB->sizeUserSpace;
}

_int32 CDIBV2Sector::GetSectorsPerBit() const
{
	const NDAS_DIB_V2 *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();
	return ( pDIB->iSectorsPerBit );
}

CDiskLocationVector CDIBV2Sector::GetBoundDiskLocations(CDiskLocationPtr thisLocation) const
{
	const NDAS_DIB_V2 *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();
	CDiskLocationVector vtLocation;
	ATLASSERT( IsBound() );	

	for ( UINT i=0; i < pDIB->nDiskCount + pDIB->nSpareCount; i++ )
	{
		vtLocation.push_back ( 
			CDiskLocationPtr( 
				new CLanDiskLocation(
					&pDIB->UnitDisks[i]
					)
				)
			);
	}
	return vtLocation;
}
CDiskLocationPtr CDIBV2Sector::GetPeerLocation() const
{
	const NDAS_DIB_V2 *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();
	ATLASSERT( IsMirrored() );
	return CDiskLocationPtr( 
		new CLanDiskLocation(&pDIB->UnitDisks[pDIB->iSequence ^ 0x01]) 
		);
}

UINT CDIBV2Sector::GetPosInBind() const
{
	const NDAS_DIB_V2 *pDIB =
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();

	ATLASSERT( IsBound() );
	return pDIB->iSequence;
}

UINT CDIBV2Sector::GetDiskCountInBind() const
{
	const NDAS_DIB_V2 *pDIB =
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();

	ATLASSERT( IsBound() );
	return pDIB->nDiskCount + pDIB->nSpareCount;
}

void CDIBV2Sector::Bind(CUnitDiskObjectVector bindDisks, UINT nIndex, int nBindType)
{
	NDAS_DIB_V2 *pDIB =
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();
	_int64 nSizeTotal;
	_int64 nSizeSectorMinDisk;
	CUnitDiskObjectPtr pDisk;
	CUnitDiskInfoHandlerPtr pDiskInfoHandler;

	// initialize pDIB	
	pDIB->sizeXArea			= NDAS_BLOCK_SIZE_XAREA; // 2MB
	pDIB->iMediaType		= nBindType;
	pDIB->nDiskCount		= (unsigned int)bindDisks.size();
	pDIB->iSequence			= nIndex;
//	pDIB->FlagDirty			= 0;

	// calculate sectors per bit using total disk size
	nSizeSectorMinDisk = 0;
	CUnitDiskObjectVector::iterator itr = bindDisks.begin();
	while(itr != bindDisks.end())
	{
		pDisk = boost::dynamic_pointer_cast<CUnitDiskObject>(*itr);
		pDiskInfoHandler = pDisk->GetInfoHandler();

		if(0 == nSizeSectorMinDisk)
			nSizeSectorMinDisk = pDiskInfoHandler->GetTotalSectorCount();

		nSizeSectorMinDisk = std::min(nSizeSectorMinDisk,
			pDiskInfoHandler->GetTotalSectorCount());
		itr++;
	}

	// calculate user size
	switch(nBindType)
	{
	case NMT_AGGREGATE:
		pDIB->iSectorsPerBit = 0;
//		pDIB->sizeUserSpace = CDiskInfoSector::GetUserSectorCount();
		pDIB->sizeUserSpace = ::CalcUserSectorCount(CDiskInfoSector::GetTotalSectorCount(), MIN_SECTORS_PER_BIT);
		break;
	case NMT_RAID0:
		pDIB->iSectorsPerBit = 0;
		pDIB->sizeUserSpace = ::CalcUserSectorCount(nSizeSectorMinDisk, MIN_SECTORS_PER_BIT);
		break;
	case NMT_RAID1:
		{
			pDIB->iSectorsPerBit = CalcSectorsPerBit(nSizeSectorMinDisk);
			ATLASSERT( (bindDisks.size() % 2) == 0 );
			CUnitDiskObjectPtr peerDisk = 
				boost::dynamic_pointer_cast<CUnitDiskObject>(bindDisks[nIndex^0x01]);
			ATLASSERT( peerDisk.get() != NULL );
			CUnitDiskInfoHandlerPtr peerInfoHandler = peerDisk->GetInfoHandler();
			nSizeTotal = std::min( 
				CDiskInfoSector::GetTotalSectorCount(),
				peerInfoHandler->GetTotalSectorCount()
				);
		}
		break;
	case NMT_RAID4:
		{
			pDIB->iSectorsPerBit = ::CalcSectorsPerBit(nSizeSectorMinDisk);
			pDIB->sizeUserSpace = ::CalcUserSectorCount(nSizeSectorMinDisk, pDIB->iSectorsPerBit);
		}
		break;
	default:
		ATLASSERT(FALSE);
		return;
	}
	
	if ( CalcRequiredSectorCount(pDIB) > static_cast<UINT>(GetCount()) )
	{
		CDiskMultiSector::Resize( CalcRequiredSectorCount(pDIB) );
		pDIB =
			CTypedDiskInfoSector<NDAS_DIB_V2>::get();
	}

	for ( UINT i=0; i < bindDisks.size(); i++ )
	{
		::CopyMemory( 
			&pDIB->UnitDisks[i], 
			boost::dynamic_pointer_cast<CUnitDiskObject>(bindDisks[i])->
				GetLocation()->GetUnitDiskLocation(),
			sizeof(UNIT_DISK_LOCATION)
			);
	}

	pDIB->crc32 = crc32_calc((unsigned char *)pDIB,
		sizeof(pDIB->bytes_248));
	pDIB->crc32_unitdisks = crc32_calc((unsigned char *)pDIB->UnitDisks,
		sizeof(pDIB->UnitDisks));
}

void CDIBV2Sector::UnBind(CUnitDiskObjectPtr disk)
{
	NDAS_DIB_V2 *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();
	UINT nPrevBoundDisks = pDIB->nDiskCount;

	pDIB->iMediaType		= NMT_SINGLE;
	pDIB->nDiskCount		= 1;
	pDIB->iSequence			= 0;
//	pDIB->FlagDirty			= 0;
	::ZeroMemory( pDIB->UnitDisks, sizeof(UNIT_DISK_LOCATION) * nPrevBoundDisks );
	::CopyMemory(
		&pDIB->UnitDisks[0],
		disk->GetLocation()->GetUnitDiskLocation(),
		sizeof(UNIT_DISK_LOCATION)
		);

	pDIB->crc32 = crc32_calc((unsigned char *)pDIB,
		sizeof(pDIB->bytes_248));
	pDIB->crc32_unitdisks = crc32_calc((unsigned char *)pDIB->UnitDisks,
		sizeof(pDIB->UnitDisks));
}

void CDIBV2Sector::Rebind(CUnitDiskObjectPtr newDisk, UINT nIndex)
{
	NDAS_DIB_V2 *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();
	ATLASSERT( nIndex < pDIB->nDiskCount );
	::CopyMemory( 
		&pDIB->UnitDisks[nIndex], 
		newDisk->GetLocation()->GetUnitDiskLocation(),
		sizeof(UNIT_DISK_LOCATION)
		);

	pDIB->crc32 = crc32_calc((unsigned char *)pDIB,
		sizeof(pDIB->bytes_248));
	pDIB->crc32_unitdisks = crc32_calc((unsigned char *)pDIB->UnitDisks,
		sizeof(pDIB->UnitDisks));
}

void CDIBV2Sector::Mirror(CDIBV2Sector *pSourceSector)
{
	ATLASSERT( 
		GetUserSectorCount() >= pSourceSector->GetUserSectorCount() );
	CDiskMultiSector::Resize( pSourceSector->GetCount() );
	NDAS_DIB_V2 *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();
	NDAS_DIB_V2 *pSourceDIB = pSourceSector->get();
	pDIB->sizeXArea			= pSourceDIB->sizeXArea;
	pDIB->sizeUserSpace		= pSourceDIB->sizeUserSpace;
	pDIB->iSectorsPerBit	= pSourceDIB->iSectorsPerBit;
	pDIB->iMediaType		= NMT_RAID1;
	pDIB->nDiskCount		= pSourceDIB->nDiskCount;
	pDIB->iSequence	= pSourceDIB->iSequence ^ 0x01;
//	pDIB->FlagDirty			= 0;
	
	::CopyMemory( 
		pDIB->UnitDisks, 
		pSourceDIB->UnitDisks, 
		sizeof(UNIT_DISK_LOCATION) * pDIB->nDiskCount 
		);

	pDIB->crc32 = crc32_calc((unsigned char *)pDIB,
		sizeof(pDIB->bytes_248));
	pDIB->crc32_unitdisks = crc32_calc((unsigned char *)pDIB->UnitDisks,
		sizeof(pDIB->UnitDisks));
}

void CDIBV2Sector::Initialize(CUnitDiskObjectPtr disk)
{
	NDAS_DIB_V2 *pDIB = 
		CTypedDiskInfoSector<NDAS_DIB_V2>::get();

	::ZeroMemory( pDIB, sizeof(NDAS_DIB_V2) );

	pDIB->Signature = NDAS_DIB_V2_SIGNATURE;
	pDIB->MajorVersion = NDAS_DIB_VERSION_MAJOR_V2;
	pDIB->MinorVersion = NDAS_DIB_VERSION_MINOR_V2;
	pDIB->iMediaType		= NMT_SINGLE;
	pDIB->nDiskCount		= 1;
	::CopyMemory(
		&pDIB->UnitDisks[0],
		disk->GetLocation()->GetUnitDiskLocation(),
		sizeof(UNIT_DISK_LOCATION)
		);

	pDIB->crc32 = crc32_calc((unsigned char *)pDIB,
		sizeof(pDIB->bytes_248));
	pDIB->crc32_unitdisks = crc32_calc((unsigned char *)pDIB->UnitDisks,
		sizeof(pDIB->UnitDisks));
}

///////////////////////////////////////////////////////////////////////////////
// CBitmapSector
///////////////////////////////////////////////////////////////////////////////
CBitmapSector::CBitmapSector()
: CDiskMultiSector(NDAS_BLOCK_SIZE_BITMAP)
{
}
_int64 CBitmapSector::GetLocation()
{
	return NDAS_BLOCK_LOCATION_BITMAP;	
}

void CBitmapSector::Merge(CBitmapSector *sec)
{
	_int8 *pbBitmap1, *pbBitmap2;
	int i, j;
	
	pbBitmap1 = this->GetData();
	pbBitmap2 = sec->GetData();

	for ( i=0; i < GetCount(); i++ )
	{
		for ( j=0; j < NDAS_BLOCK_SIZE; j++ )
		{
			pbBitmap1[i*NDAS_BLOCK_SIZE+j] = 
				pbBitmap1[i*NDAS_BLOCK_SIZE+j] | pbBitmap2[i*NDAS_BLOCK_SIZE+j];
		}
	}

	return;
}

///////////////////////////////////////////////////////////////////////////////
// Helper functions
///////////////////////////////////////////////////////////////////////////////
_int64 CalcUserSectorCount(_int64 nTotalSectorCount, _int32 nSectorsPerBit)
{
	_int64 nSize;

//	ATLASSERT(nSectorsPerBit != 0);
	nSize = nTotalSectorCount- NDAS_BLOCK_SIZE_XAREA;	// Subtracts area reserved
//	nSize = nSize - (nSize % nSectorsPerBit);

	return nSize;
}

UINT CalcSectorsPerBit(_int64 nTotalSectorCount)
{
	int nMinSectorPerBit =  static_cast<int>( (nTotalSectorCount)/MAX_BITS_IN_BITMAP );
	int nSectorPerBit;

	if ( nMinSectorPerBit < MIN_SECTORS_PER_BIT )
	{
		return MIN_SECTORS_PER_BIT;
	}
	else
	{
		nSectorPerBit = 256;
		while (1)
		{
			ATLASSERT( nSectorPerBit < 1024*1024 ); // In case for bug.
			if ( nMinSectorPerBit < nSectorPerBit )
				return nSectorPerBit;
			nSectorPerBit *= 2;
		}
	}
}