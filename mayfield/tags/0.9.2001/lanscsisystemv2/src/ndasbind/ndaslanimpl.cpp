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
// CLanUnitDiskObject
///////////////////////////////////////////////////////////////////////////////
CLanUnitDiskObject::CLanUnitDiskObject(
				   CDeviceInfoPtr deviceInfo,
				   UINT nUnitNumber, 
				   CUnitDiskInfoHandler *pHandler
				   )
: 	m_deviceInfo(deviceInfo),
	CUnitDiskObject( 
					deviceInfo->GetName(),
					new CLanDiskLocation(
					deviceInfo->GetDeviceID()->Node, static_cast<unsigned _int8>(nUnitNumber)), 
					pHandler)
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

BOOL CLanUnitDiskObject::CanAccessExclusive()
{
	CSession session;
	const UNIT_DISK_LOCATION *pLocation;
	TARGET_DATA targetData;
	pLocation = m_pLocation->GetUnitDiskLocation();

	try {
		session.Connect( pLocation->MACAddr );
		session.GetTargetData( pLocation, &targetData );
		session.Disconnect();
	}
	catch( CNDASException & )
	{
		// TODO : Is just returning FALSE fine?
		session.Disconnect();
		return FALSE;
	}

	if ( targetData.NRRWHost != 0 )
	{
		return FALSE;
	}
	//
	// TODO : What if there's read connection?
	//
	return TRUE;
}

void CLanUnitDiskObject::OpenExclusive()
{
	m_session.Connect( m_pLocation->GetUnitDiskLocation()->MACAddr );
	m_session.Login( 
		m_pLocation->GetUnitDiskLocation()->UnitNumber,
		TRUE
		);
}

void CLanUnitDiskObject::CommitDiskInfo(BOOL bSaveToDisk)
{
	BOOL bSessionCreated = FALSE;
	CSession session;
	if ( !m_session.IsLoggedIn(bSaveToDisk) )
	{
		if ( !bSaveToDisk )
		{
			session.Connect( m_pLocation->GetUnitDiskLocation()->MACAddr );
			session.Login( 
				m_pLocation->GetUnitDiskLocation()->UnitNumber,
				FALSE
				);
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
		session.Logout();
		session.Disconnect();
	}
}

void CLanUnitDiskObject::Close()
{
	m_session.Logout();
	m_session.Disconnect();
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
		bitmap.GetCount() * BLOCK_SIZE, 
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
	m_pHandler->SetDirty();
	try { 
	m_pHandler->CommitDiskInfo( &m_session );
	}
	catch( CNDASException &e )
	{
		NDAS_THROW_EXCEPTION_CHAIN_STR(
			CDiskException,
			CDiskException::ERROR_FAIL_TO_MARK_BITMAP,
			_T("Fail to update DISK_INFORMATION_BLOCK"),
			e);
	}
}

///////////////////////////////////////////////////////////////////////////////
// CLanUnitDiskObjectFactory
///////////////////////////////////////////////////////////////////////////////
CUnitDiskObjectPtr CLanUnitDiskObjectFactory::Create(
												CDeviceInfoPtr deviceInfo,
												UINT nUnitNumber, 
												TARGET_DATA *pTargetData
												)
{
	ATLASSERT( pTargetData != NULL );
	if ( pTargetData->MediaType != MEDIA_TYPE_BLOCK_DEVICE )
	{
		// Types other than HDD is not handled here.
		return CUnitDiskObjectPtr( new CLanUnitDiskObject(
										deviceInfo,
										nUnitNumber, 
										new CDefaultDiskInfoHandler()
										)
							 );
	}

	// 
	// Get information about the HDD and create CDiskObject based on the information
	//
	// CTypedDiskSector<DISK_INFORMATION_BLOCK_V2> diskInfoBlockV2;
	CSession session;
	CUnitDiskObjectPtr newDisk;
	CDiskInfoSectorPtr pDIBV2(new CDIBV2Sector(pTargetData));
	CDiskInfoSectorPtr pDIB(new CDIBSector(pTargetData));

	session.Connect( deviceInfo->GetDeviceID()->Node );
	session.Login();
	pDIBV2->ReadAccept( &session );
	
	if ( dynamic_cast<CDIBV2Sector*>(pDIBV2.get())->IsValid() )
	{
		newDisk = CUnitDiskObjectPtr( 
					new CLanUnitDiskObject( 
							deviceInfo,
							nUnitNumber, 
							new CHDDDiskInfoHandler( pDIBV2 )
							)
					);
	}
	else
	{
		pDIB->ReadAccept( &session );
		if ( dynamic_cast<CDIBSector*>(pDIB.get())->IsValid() )
		{
			newDisk = CUnitDiskObjectPtr(
						new CLanUnitDiskObject(
							deviceInfo,
							nUnitNumber,
							new CHDDDiskInfoHandler( pDIB )
							)
						);
		}
		else
		{
			// Single disk
			CDiskInfoSectorPtr pNewDIB = CDiskInfoSectorPtr( new CDIBV2Sector(pTargetData) );
			newDisk = CUnitDiskObjectPtr(
						new CLanUnitDiskObject(
							deviceInfo,
							nUnitNumber,
							new CHDDDiskInfoHandler( pNewDIB )
							)
						);
		}
	}

	session.Logout();
	session.Disconnect();

	return newDisk;
}

///////////////////////////////////////////////////////////////////////////////
// CLanDeviceObject
///////////////////////////////////////////////////////////////////////////////
void CLanDeviceObject::Init()
{
	int iErrorCode;
	CUnitDiskObjectPtr diskObject;
	try {
		CSession::Connect(m_info->GetDeviceID()->Node);

		// Get information about NDAS device
		m_path.iUserID = 0x0001;
		m_path.iPassword = _GetHWPassword( m_info->GetDeviceID()->Node );
		m_path.iSessionPhase = LOGOUT_PHASE;

		iErrorCode = ::Discovery( &m_path );
		if ( iErrorCode != 0 )
		{
			// ERROR : fail to discover NDAS information
			NDAS_THROW_EXCEPTION( CDiskException, CDiskException::ERROR_FAIL_TO_DISCOVER_DEVICEINFO );
		}

		// Get information about each units in the device
		CSession::Login();
		for ( int i=0; i < m_path.iNRTargets; i++ )
		{
			if ( !m_path.PerTarget[i].bPresent )
				continue;
			// This function fills in the information of 
			// PerTarget fields in the LANSCSI_PATH.
			iErrorCode = ::GetDiskInfo( &m_path, i );
			if ( iErrorCode != 0 )
			{
				// ERROR : fail to get disk information
				NDAS_THROW_EXCEPTION( CDiskException, CDiskException::ERROR_FAIL_TO_GET_DISKINFO );
			}
			diskObject = 
				CLanUnitDiskObjectFactory::Create( 
					m_info, i, &m_path.PerTarget[i]
					);
			m_listDisk.push_back( diskObject );
		}
		CSession::Logout();
		CSession::Disconnect();
	}
	catch( CNDASException &e )
	{
		NDAS_THROW_EXCEPTION_CHAIN(
			CDiskException,
			CDiskException::ERROR_FAIL_TO_INITIALIZE,
			e
			);
	}
}

///////////////////////////////////////////////////////////////////////////////
// CLanDeviceObjectBuilder
///////////////////////////////////////////////////////////////////////////////
static CLanDeviceObjectBuilder g_deviceBuilder; // singleton object.
CDeviceObjectList CLanDeviceObjectBuilder::Build(const CDeviceInfoList listDeviceInfo) const
{
	CLanDeviceObject *pNewDevice;
	CDeviceObjectList listDevice;
	CDeviceInfoList::const_iterator itr;

	for ( itr = listDeviceInfo.begin(); itr != listDeviceInfo.end(); ++itr )
	{
		pNewDevice = new CLanDeviceObject( *itr );
		try {
			pNewDevice->Init();
			listDevice.push_back( CDeviceObjectPtr(pNewDevice) );
		} catch( CNDASException &e )
		{
			// ERROR : Fail to initialize device
			e.PrintStackTrace();
		}
	}
	return listDevice;
}

static CLanDiskObjectBuilder g_objectBuilder; 	// Singleton object.

CUnitDiskObjectList 
CLanDiskObjectBuilder::BuildDiskObjectList(const CDeviceInfoList listDeviceInfo) const
{
	CDeviceObjectList listDeviceObj;
	CUnitDiskObjectList listDiskObj;

	// Get the list of all devices
	listDeviceObj = CDeviceObjectBuilder::GetInstance()->Build( listDeviceInfo );
	
	// Make the list of all disks attached to the devices
	CDeviceObjectList::const_iterator itr;

	for ( itr = listDeviceObj.begin(); itr != listDeviceObj.end(); ++itr )
	{
		CUnitDiskObjectList listSubDisks;
		listSubDisks = (*itr)->GetDiskObjectList();
		listDiskObj.insert( 
						listDiskObj.end(), 
						listSubDisks.begin(), listSubDisks.end()
						);
	}
	return listDiskObj;
}

CDiskObjectPtr CLanDiskObjectBuilder::BuildFromDeviceInfo(const CDeviceInfoList listDevice) const
{
	CDiskObjectCompositePtr root;
	//
	// Build list of unit disks from the device list
	//
	CUnitDiskObjectList listDeviceDisk = BuildDiskObjectList(listDevice);

	root = CDiskObjectCompositePtr(new CRootDiskObject());
	//
	// Construct structure of disks including aggregation and mirroring
	//
	while ( !listDeviceDisk.empty() )
	{
		CUnitDiskObjectPtr disk;
		disk = listDeviceDisk.front();

		CUnitDiskInfoHandlerPtr pInfoHandler = disk->GetInfoHandler();
			
		if ( !pInfoHandler->IsHDD() )
		{
			listDeviceDisk.pop_front();
			continue; // Only HDD type disk is supported.(This may be changed later)
		}
		if ( pInfoHandler->IsBound() )
		{
			if ( pInfoHandler->IsAggregated() )
			{
				CDiskObjectCompositePtr aggrDisks = 
					CDiskObjectCompositePtr(new CAggrDiskObject());
				CDiskLocationVector vtLocation =
					pInfoHandler->GetBoundDiskLocations();

				if ( pInfoHandler->IsMirrored() )
				{
					for ( UINT i=0; i < vtLocation.size(); i++ )
					{
						CMirDiskObjectPtr mirDisks =
							CMirDiskObjectPtr( new CMirDiskObject() );
						CUnitDiskObjectList::const_iterator found;
						found = std::find_if( 
								listDeviceDisk.begin(), 
								listDeviceDisk.end(),
								std::bind1st(CDiskLocationEqual(), vtLocation[i])
								);
						if ( found != listDeviceDisk.end() 
							&& ::HasSameBoundDiskList(*found, disk) )
						{
							mirDisks->AddChild( mirDisks, *found );
							listDeviceDisk.remove( *found );
						}
						found = std::find_if( 
							listDeviceDisk.begin(), 
							listDeviceDisk.end(),
							std::bind1st(CDiskLocationEqual(), vtLocation[i+1])
							);
						if ( found != listDeviceDisk.end() 
							&& ::HasSameBoundDiskList(*found, disk) )
						{
							mirDisks->AddChild( mirDisks, *found );
							listDeviceDisk.remove( *found );
						}
						if ( mirDisks->size() != 0 )
							aggrDisks->AddChild( aggrDisks, mirDisks );
					}
				}
				else
				{
					for ( UINT i=0; i < vtLocation.size(); i++ )
					{
						CUnitDiskObjectList::const_iterator found;
						found = std::find_if( 
							listDeviceDisk.begin(), 
							listDeviceDisk.end(),
							std::bind1st(CDiskLocationEqual(), vtLocation[i])
							);
						if ( found != listDeviceDisk.end()
							&& ::HasSameBoundDiskList(*found, disk) )
						{
							aggrDisks->AddChild( aggrDisks, *found );
							listDeviceDisk.remove( *found );
						}
					}

				}
				root->AddChild( root, aggrDisks );
			}
			else if ( pInfoHandler->IsMirrored() )
			{
				CMirDiskObjectPtr mirDisks = 
					CMirDiskObjectPtr(new CMirDiskObject());
				CUnitDiskObjectPtr mirDisk = 
					::FindMirrorDisk(disk, listDeviceDisk);

				mirDisks->AddChild( mirDisks, disk );
				listDeviceDisk.pop_front();
				mirDisks->AddChild( mirDisks, mirDisk );
				listDeviceDisk.remove( mirDisk );
				root->AddChild( root, mirDisks );
			}
		}
		else // pDiskInfoHander->IsBound()
		{
			listDeviceDisk.pop_front();
			root->AddChild( root, disk );
		}
	}	// while ( !listDeviceDisk.empty() )
	return root;
}

///////////////////////////////////////////////////////////////////////////////
// CLanDiskLocation
///////////////////////////////////////////////////////////////////////////////
BOOL CLanDiskLocation::Equal(const CDiskLocation *pRef) const
{
	ATLASSERT( pRef != NULL );

	const UNIT_DISK_LOCATION *pLanRef;

	pLanRef = dynamic_cast<const CLanDiskLocation*>(pRef);
	if ( pLanRef == NULL )
		return FALSE;	// Different location type

	return ( ::memcmp( MACAddr, pLanRef->MACAddr, sizeof(MACAddr) ) == 0 )
		&& UnitNumber == pLanRef->UnitNumber;
}
