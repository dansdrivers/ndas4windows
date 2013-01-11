////////////////////////////////////////////////////////////////////////////
//
// Implementation of CDiskObjectBuilder class
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ndasobjectbuilder.h"
#include "ndasutil.h"
#include "ndashelper.h"
#include "ndasexception.h"
#include "ndaslanimpl.h"

CUnitDiskObjectPtr
CDiskObjectBuilder::CreateDiskObject(const CDeviceInfoPtr deviceInfo, unsigned _int8 nSlotNumber)
{
	// For now, only management of disks connected by LAN is implemented.
	CUnitDiskObjectPtr diskObject;
//	TARGET_DATA	targetData;
	NDAS_UNIT_DEVICE_INFO UnitDeviceInfo;

	CLanSession session( deviceInfo->GetDeviceID()->Node, nSlotNumber );

	session.Connect();

    session.GetTargetData( &UnitDeviceInfo );

	if ( UnitDeviceInfo.MediaType != MEDIA_TYPE_BLOCK_DEVICE )
	{
		// Types other than HDD is not supported
		// TODO : Throw exception
		NDAS_THROW_EXCEPTION( 
			CDiskException, 
			CDiskException::ERROR_UNSUPPORTED_DISK_TYPE 
			);
	}

	CDiskInfoSectorPtr dibv2(new CDIBV2Sector(&UnitDeviceInfo));
	CDiskInfoSectorPtr dib(new CDIBSector(&UnitDeviceInfo));
	
	// First get DIB_V2
	dibv2->ReadAccept( &session );

	if ( dibv2->IsValidSignature() )
	{
		diskObject = CUnitDiskObjectPtr( 
						new CLanUnitDiskObject( 
								deviceInfo,
								nSlotNumber,
								new CHDDDiskInfoHandler( dibv2 )
								)
						);
	}
	else
	{
		dib->ReadAccept( &session );
		if ( dib->IsValidSignature() && dib->IsValid() )
		{
			diskObject = CUnitDiskObjectPtr(
							new CLanUnitDiskObject(
								deviceInfo,
								nSlotNumber,
								new CHDDDiskInfoHandler( dib )
								)
							);
		}
		else
		{
			// No DIB information : single disk
			CDiskInfoSectorPtr newDIB = 
				CDiskInfoSectorPtr( new CDIBV2Sector(&UnitDeviceInfo) );
			diskObject = CUnitDiskObjectPtr(
							new CLanUnitDiskObject(
								deviceInfo,
								nSlotNumber,
								new CHDDDiskInfoHandler( newDIB )
								)
							);
		}
	}
	session.Disconnect();
	return diskObject;
}

CUnitDiskObjectList 
CDiskObjectBuilder::BuildDiskObjectList(const CDeviceInfoList listDevice)
{
	CUnitDiskObjectList listDiskObj;
	CDeviceInfoList::const_iterator itr;
	for ( itr = listDevice.begin(); itr != listDevice.end(); ++itr )
	{
		CUnitDiskObjectPtr diskObj;
		UINT nDiskCount;
		CLanSession session( (*itr)->GetDeviceID()->Node );

		if(NDAS_DEVICE_STATUS_DISCONNECTED == (*itr)->GetDeviceStatus())
			continue;
		
		// Get number of disks in the device
		try {
			session.Connect();
			nDiskCount = session.GetDiskCount();
			session.Disconnect();
		}
		catch( CNDASException &e )
		{
			(*itr)->SetDeviceStatus(NDAS_DEVICE_STATUS_DISCONNECTED);
			e.PrintStackTrace();
			continue;
		}

		// Create disk object by retrieving information from the device.
		for ( UINT i=0; i < nDiskCount; i++ )
		{
			try {
				diskObj = CreateDiskObject( *itr, static_cast<unsigned _int8>(i) );
				listDiskObj.push_back( diskObj );
			}
			catch( CNDASException &e )
			{
				e.PrintStackTrace();
				continue;
			}
		}
	}
	return listDiskObj;
}

CDiskObjectPtr 
CDiskObjectBuilder::Build(const CDeviceInfoList listDevice)
{
	CUnitDiskObjectList listDiskObj;
	CDiskObjectCompositePtr root;

	//
	// Build list of unit disks from the device list
	//
	listDiskObj = BuildDiskObjectList(listDevice);

	//
	// Construct structure of disks including aggregation and mirroring
	//
	root = CDiskObjectCompositePtr(new CRootDiskObject());

	while ( !listDiskObj.empty() )
	{
		CUnitDiskObjectPtr disk;
		disk = listDiskObj.front();

		CUnitDiskInfoHandlerPtr pInfoHandler = disk->GetInfoHandler();
			
		if ( !pInfoHandler->IsHDD() )
		{
			listDiskObj.pop_front();
			continue; // Only HDD type disk is supported.(This may be changed later)
		}
		if ( pInfoHandler->IsBound() )
		{
			CDiskLocationVector vtLocation =
				pInfoHandler->GetBoundDiskLocations(disk->GetLocation());

			if ( pInfoHandler->IsBoundAndNotSingleMirrored() )
			{
				CDiskObjectCompositePtr aggrDisks = 
					CDiskObjectCompositePtr(new CAggrDiskObject());

				if ( pInfoHandler->IsMirrored() ) // double tree
				{
					for ( UINT i=0; i < vtLocation.size(); i+= 2 )
					{
						CMirDiskObjectPtr mirDisks =
							CMirDiskObjectPtr( new CMirDiskObject() );
						CUnitDiskObjectList::const_iterator found;

						// find first of the pair
						found = std::find_if( 
								listDiskObj.begin(), 
								listDiskObj.end(),
								std::bind1st(CDiskLocationEqual(), vtLocation[i])
								);
						if ( found != listDiskObj.end() 
							&& ::HasSameBoundDiskList(*found, disk) )
						{
							mirDisks->AddChild( mirDisks, *found );
							listDiskObj.remove( *found );
						}
						else
						{
							// create empty unit disk
							mirDisks->AddChild( mirDisks, ::CreateEmptyDiskObject());
						}

						// find second of the pair
						found = std::find_if( 
							listDiskObj.begin(), 
							listDiskObj.end(),
							std::bind1st(CDiskLocationEqual(), vtLocation[i+1])
							);
						if ( found != listDiskObj.end() 
							&& ::HasSameBoundDiskList(*found, disk) )
						{
							mirDisks->AddChild( mirDisks, *found );
							listDiskObj.remove( *found );
						}
						else
						{
							// create empty unit disk
							mirDisks->AddChild( mirDisks, ::CreateEmptyDiskObject());
						}

						if ( mirDisks->size() != 0 ) // always 2 including empty disks
							aggrDisks->AddChild( aggrDisks, mirDisks );
					}
				}
				else
				{
					for ( UINT i=0; i < vtLocation.size(); i++ )
					{
						CDiskLocationPtr p = vtLocation[i];
						CUnitDiskObjectList::const_iterator found;
						found = std::find_if( 
							listDiskObj.begin(), 
							listDiskObj.end(),
							std::bind1st(CDiskLocationEqual(), vtLocation[i])
							);
						if ( found != listDiskObj.end()
							&& ::HasSameBoundDiskList(*found, disk) )
						{
							aggrDisks->AddChild( aggrDisks, *found );
							listDiskObj.remove( *found );
						}
						else
						{
							// create empty unit disk
							aggrDisks->AddChild( aggrDisks, ::CreateEmptyDiskObject());
						}
					}
				}
				root->AddChild( root, aggrDisks );
			}
			else if ( pInfoHandler->IsMirrored() )
			{
				CMirDiskObjectPtr mirDisks =
					CMirDiskObjectPtr( new CMirDiskObject() );
				CUnitDiskObjectList::const_iterator found;

				UINT i = 0;

				// find first of the pair
				found = std::find_if( 
					listDiskObj.begin(), 
					listDiskObj.end(),
					std::bind1st(CDiskLocationEqual(), vtLocation[i])
					);
				if ( found != listDiskObj.end() 
					&& ::HasSameBoundDiskList(*found, disk) )
				{
					mirDisks->AddChild( mirDisks, *found );
					listDiskObj.remove( *found );
				}
				else
				{
					// create empty unit disk
					mirDisks->AddChild( mirDisks, ::CreateEmptyDiskObject());
				}

				// find second of the pair
				found = std::find_if( 
					listDiskObj.begin(), 
					listDiskObj.end(),
					std::bind1st(CDiskLocationEqual(), vtLocation[i+1])
					);
				if ( found != listDiskObj.end() 
					&& ::HasSameBoundDiskList(*found, disk) )
				{
					mirDisks->AddChild( mirDisks, *found );
					listDiskObj.remove( *found );
				}
				else
				{
					// create empty unit disk
					mirDisks->AddChild( mirDisks, ::CreateEmptyDiskObject());
				}
				
				root->AddChild(root, mirDisks);
			}
		}
		else // pDiskInfoHander->IsBound()
		{
			listDiskObj.pop_front();
			root->AddChild( root, disk );
		}
	}	// while ( !listDiskObj.empty() )
	return root;
}