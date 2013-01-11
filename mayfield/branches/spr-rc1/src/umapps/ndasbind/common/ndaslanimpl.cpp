////////////////////////////////////////////////////////////////////////////
//
// Implementation of NDAS object for LAN environment
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include <map>
#include <algorithm> // for copy function

#include "windows.h"
#include "ndaslanimpl.h"

#include "ndasutil.h"
#include "ndashelper.h"
#include "ndasexception.h"

///////////////////////////////////////////////////////////////////////////////
// CLanDiskLocation
///////////////////////////////////////////////////////////////////////////////
BOOL CLanDiskLocation::Equal(const CDiskLocationPtr ref) const
{
	ATLASSERT( ref.get() != NULL );

	const UNIT_DISK_LOCATION *pLanRef;

	pLanRef = dynamic_cast<const CLanDiskLocation*>(ref.get());
	if ( pLanRef == NULL )
		return FALSE;	// Different location type

	return ( ::memcmp( MACAddr, pLanRef->MACAddr, sizeof(MACAddr) ) == 0 )
		&& UnitNumber == pLanRef->UnitNumber;
}

///////////////////////////////////////////////////////////////////////////////
// CLanUnitDiskObject
///////////////////////////////////////////////////////////////////////////////
CLanUnitDiskObject::CLanUnitDiskObject(
				   CDeviceInfoPtr deviceInfo,
				   unsigned _int8 nSlotNumber, 
				   CUnitDiskInfoHandler *pHandler
				   )
: 	m_deviceInfo(deviceInfo),
	m_session( 
		deviceInfo->GetDeviceID()->Node, 
		nSlotNumber
		),
	CUnitDiskObject( 
					deviceInfo->GetName(),
					new CLanDiskLocation(
					deviceInfo->GetDeviceID()->Node, nSlotNumber ), 
					pHandler
					)
{
}

WTL::CString CLanUnitDiskObject::GetStringDeviceID() const 
{ 
	return m_deviceInfo->GetStringDeviceID(); 
}

ACCESS_MASK CLanUnitDiskObject::GetAccessMask() const
{
	return m_deviceInfo->GetAccessMask();
}

BOOL CLanUnitDiskObject::CanAccessExclusive(BOOL bAllowRead)
{
	if ( m_session.IsLoggedIn(TRUE) )
		return TRUE;
	const UNIT_DISK_LOCATION *pLocation;
	pLocation = m_pLocation->GetUnitDiskLocation();

	ConnectedHosts hosts;
	CLanSession session(pLocation->MACAddr, pLocation->UnitNumber);

	try {
		session.Connect(FALSE);
		hosts = session.GetHostCount();
		session.Disconnect();
	}
	catch( CNDASException & )
	{
		session.Disconnect();
		return FALSE;
	}

	if ( hosts.nRWHosts > 0)
	{
		return FALSE;
	}

	if ( !bAllowRead && hosts.nRWHosts > 0 )
	{
		return FALSE;
	}

	return TRUE;
}

void CLanUnitDiskObject::Open(BOOL bWrite)
{
	ATLASSERT( !m_session.IsLoggedIn(FALSE) ); // Multiple login to a disk is prohibited
	m_session.Connect(bWrite);
}

void CLanUnitDiskObject::CommitDiskInfo(BOOL bSaveToDisk)
{
	BOOL bSessionCreated = FALSE;
	CLanSession session;
	if ( !m_session.IsLoggedIn(bSaveToDisk) )
	{
		if ( !bSaveToDisk )
		{
			// For read operation, create a session if session does
			// not exist.
			session.SetAddress( m_pLocation->GetUnitDiskLocation()->MACAddr );
			session.SetSlotNumber( m_pLocation->GetUnitDiskLocation()->UnitNumber );
			session.Connect(FALSE);
			bSessionCreated = TRUE;
		}
		else
		{
			ATLASSERT(FALSE);	// For write operation, session must be
								// created in advance.
		}
	}

	if ( bSessionCreated )
	{
		m_pHandler->CommitDiskInfo(&session, bSaveToDisk);
	}
	else
	{
		m_pHandler->CommitDiskInfo(&m_session, bSaveToDisk);
	}

	if ( bSessionCreated )
	{
		session.Disconnect();
	}
}

void CLanUnitDiskObject::Close()
{
	if ( m_session.IsLoggedIn() )
		m_session.Disconnect();
}

void CLanUnitDiskObject::Bind(CUnitDiskObjectVector bindDisks, UINT nIndex, int nBindType, BOOL bInit)
{
	if ( bInit )
	{
		// The MBR block of the disk will be cleaned
		// to prevent unexpected behavior of disks after binding.
		CDataSector dataSector( SECTOR_MBR_COUNT );
		dataSector.SetLocation( 0 );
		try{
			dataSector.WriteAccept( &m_session );		
		}
		catch( CNDASException &e )
		{
			NDAS_THROW_EXCEPTION_CHAIN_STR(
				CDiskException,
				CDiskException::ERROR_FAIL_TO_INITIALIZE,
				_T("Fail to clean MBR"),
				e);
		}
	}
	m_pHandler->Bind(bindDisks, nIndex, nBindType);
}

CDiskObjectList CLanUnitDiskObject::UnBind(CDiskObjectPtr _this)
{
	ATLASSERT( this == _this.get() );
	CDiskObjectList listUnbound;

	// If the disk is aggregated, the MBR block of the disk should be cleaned
	// to prevent unexpected behavior of disks after unbinding.
	CDataSector dataSector( SECTOR_MBR_COUNT );
	dataSector.SetLocation( 0 );
	if ( m_pHandler->IsBoundAndNotSingleMirrored() )
	{
		try{
			dataSector.WriteAccept( &m_session );
		}
		catch( CNDASException &e )
		{
			NDAS_THROW_EXCEPTION_CHAIN_STR(
				CDiskException,
				CDiskException::ERROR_FAIL_TO_INITIALIZE,
				_T("Fail to clean MBR"),
				e);
		}
	}

	// Clear bind information
	m_pHandler->UnBind(
		boost::dynamic_pointer_cast<CUnitDiskObject>(_this)
		);

	listUnbound.push_back(_this);
	return listUnbound;
}

void CLanUnitDiskObject::MarkAllBitmap(BOOL bMarkDirty)
{
	// For write operation, session must be
	// created in advance.
	ATLASSERT( m_session.IsLoggedIn(TRUE) != FALSE );
	// TODO : We need to check whether the DISK_INFOMRATION_BLOCK is version 2
	CBitmapSector bitmap;

	::FillMemory( 
		bitmap.GetData(), 
		bitmap.GetCount() * LANSCSI_BLOCK_SIZE, 
		(bMarkDirty)? 0xff : 0x00 
		);
	try{
		bitmap.WriteAccept( &m_session );
	}
	catch( CNDASException &e )
	{
		NDAS_THROW_EXCEPTION_CHAIN_STR(
			CDiskException,
			CDiskException::ERROR_FAIL_TO_MARK_BITMAP,
			_T("Fail to write bitmap"),
			e);
	}
	
	// Mark the disk as dirty
	m_pHandler->SetDirty(bMarkDirty);
	try { 
	m_pHandler->CommitDiskInfo( &m_session );
	}
	catch( CNDASException &e )
	{
		NDAS_THROW_EXCEPTION_CHAIN_STR(
			CDiskException,
			CDiskException::ERROR_FAIL_TO_MARK_BITMAP,
			_T("Fail to update NDAS_DIB"),
			e);
	}
}

CSession *CLanUnitDiskObject::GetSession()
{
	return &m_session;
}


DWORD CLanUnitDiskObject::GetSlotNo() const
{
	return m_deviceInfo->GetSlotNo();
}