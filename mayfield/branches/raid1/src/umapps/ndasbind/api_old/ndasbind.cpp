/*++

	NDAS BIND API source

	Copyright (C)2002-2004 XIMETA, Inc.
	All rights reserved.

--*/

#include "stdafx.h"
#include "ndasbind.h"
#include "ndasuser.h"

#include "ndascommon.h"
#include "ndasdevice.h"
#include "ndasobjectbuilder.h"
#include "ndasid.h"
#include "ndasexception.h"

#include "nbsyncobserver.h"

CUnitDiskObjectPtr
NdasCreateUnitDiskObject(PNDAS_UNITDEVICE pUnitDeviceId)
{
	BOOL bResult;
	NDASUSER_DEVICE_INFORMATION deviceInfo;
	NDAS_DEVICE_ID deviceID;

	bResult = NdasQueryDeviceInformation( pUnitDeviceId->dwSlotNo, &deviceInfo );
	if ( !bResult )
	{
		// ERROR : Fail to get information from the service.
		return CUnitDiskObjectPtr();
	}

	bResult = ConvertStringIdToRealId( deviceInfo.szDeviceId, &deviceID );
	if ( !bResult )
	{
		// ERROR : Invalid device id(problem with service)
		return CUnitDiskObjectPtr();
	}

	CDeviceInfoPtr device = 
		CDeviceInfoPtr( new CDeviceInfo(deviceInfo.szDeviceName, deviceInfo.szDeviceId, deviceID) );

	CUnitDiskObjectPtr diskObject;
	try {
		diskObject = CDiskObjectBuilder::CreateDiskObject( 
						device, 
						static_cast<unsigned char>(pUnitDeviceId->dwUnitNo) 
						);
	}
	catch( CNDASException & )
	{
		// ERROR : Fail to connect disk or invalid disk
		return CUnitDiskObjectPtr();
	}

	return diskObject;
}

BOOL
WINAPI
NdasBindUnitDevices(PNDAS_UNITDEVICE_GROUP pUnitDeviceIdGroup, NDAS_UNITDEVICE_DISK_TYPE diskType)
{
	int nBindType;
	BOOL bResult;

	_ASSERT(FALSE);
	//
	// Basic validation of parameters
	//
	switch( diskType )
	{
	case NDAS_UNITDEVICE_DISK_TYPE_AGGREGATED:
		nBindType = NMT_AGGREGATE;
		break;
	case NDAS_UNITDEVICE_DISK_TYPE_RAID1:
		if ( (pUnitDeviceIdGroup->cUnitDevices % 2) != 0 )
			return FALSE;	// ERROR : Number of disks must be even.
		nBindType = NMT_RAID1;
		break;
	// TODO : diskType that has been changed by aingoppa should be applied here.
	default:
		// ERROR : Invalid parameters
		return FALSE;
	}
	
	bResult = IsValidBindDiskCount(nBindType, pUnitDeviceIdGroup->cUnitDevices);
	if ( !bResult )
	{
		// ERROR : Invalid number of disks.
		return FALSE;
	}
	//
	// Create disk objects
	//
	CUnitDiskObjectVector vtUnitDisks;
	CUnitDiskObjectPtr unitDisk;
	UINT i;
	for ( i=0; i < pUnitDeviceIdGroup->cUnitDevices; i++ )
	{
		unitDisk = NdasCreateUnitDiskObject( &pUnitDeviceIdGroup->aUnitDevices[i] );
		if ( unitDisk.get() == NULL )
			return FALSE;
		//
		// Validation of disks
		//
		if ( unitDisk->GetInfoHandler()->IsBound() )
			return FALSE;	// ERROR : disk is already bound.
		vtUnitDisks.push_back(unitDisk);
	}

	//
	// Bind disks
	//
	for ( i=0; i < vtUnitDisks.size(); i++ )
	{
		if ( !vtUnitDisks[i]->CanAccessExclusive() )
		{
			// ERROR : cannot access disk exclusively
			return FALSE;
		}
	}

	try {
		for ( i=0; i < vtUnitDisks.size(); i++ )
		{
			vtUnitDisks[i]->Open( TRUE );
		}
	}
	catch ( CNDASException & )
	{
		for ( i=0; i < vtUnitDisks.size(); i++ )
		{
			vtUnitDisks[i]->Close();
		}
		// ERROR : Fail to connect to disk
		return FALSE;
	}

	for ( i=0; i < vtUnitDisks.size(); i++ )
	{
		vtUnitDisks[i]->Bind( vtUnitDisks, i, nBindType );
	}

	try {
		for ( i=0; i < vtUnitDisks.size(); i++ )
		{
			vtUnitDisks[i]->CommitDiskInfo();
		}
	}
	catch ( CNDASException & )
	{
		for ( i=0; i < vtUnitDisks.size(); i++ )
		{
			vtUnitDisks[i]->Close();
		}
		// ERROR : Unexpected error while writing
		return FALSE;
	}
	for ( i=0; i < vtUnitDisks.size(); i++ )
	{
		vtUnitDisks[i]->Close();
	}
	return TRUE;
}


BOOL
WINAPI
NdasUnBindUnitDevices(PNDAS_UNITDEVICE_GROUP pUnitDeviceIdGroup)
{
	UINT i;

	CUnitDiskObjectVector vtUnitDisks;
	CUnitDiskObjectPtr unitDisk;
	for ( i=0; i < pUnitDeviceIdGroup->cUnitDevices; i++ )
	{
		unitDisk = NdasCreateUnitDiskObject( &pUnitDeviceIdGroup->aUnitDevices[i] );
		if ( unitDisk.get() == NULL )
			return FALSE;
		//
		// Validation of disks
		//
		if ( !unitDisk->GetInfoHandler()->IsBound() )
			return FALSE; // ERROR : disk is not bound
		vtUnitDisks.push_back(unitDisk);
	}

	//
	// Unbind disks
	//
	for ( i=0; i < vtUnitDisks.size(); i++ )
	{
		if ( !vtUnitDisks[i]->CanAccessExclusive() )
		{
			// ERROR : cannot access disk exclusively
			return FALSE;
		}
	}

	try {
		for ( i=0; i < vtUnitDisks.size(); i++ )
		{
			vtUnitDisks[i]->Open( TRUE );
		}
	}
	catch ( CNDASException & )
	{
		for ( i=0; i < vtUnitDisks.size(); i++ )
		{
			vtUnitDisks[i]->Close();
		}
		// ERROR : Fail to connect to disk
		return FALSE;
	}

	for ( i=0; i < vtUnitDisks.size(); i++ )
	{
		vtUnitDisks[i]->UnBind( vtUnitDisks[i] );
	}


	try {
		for ( i=0; i < vtUnitDisks.size(); i++ )
		{
			vtUnitDisks[i]->CommitDiskInfo();
		}
	}
	catch ( CNDASException & )
	{
		for ( i=0; i < vtUnitDisks.size(); i++ )
		{
			vtUnitDisks[i]->Close();
		}
		// ERROR : Unexpected error while writing
		return FALSE;
	}
	for ( i=0; i < vtUnitDisks.size(); i++ )
	{
		vtUnitDisks[i]->Close();
	}
	return TRUE;
}

BOOL
WINAPI
NdasAddMirror(DWORD dwMasterSlotNo, DWORD dwMasterUnitNo,
			  DWORD dwMirrorSlotNo, DWORD dwMirrorUnitNo)
{
	NDAS_UNITDEVICE master, mirror;
	CUnitDiskObjectPtr masterUnitDisk, mirrorUnitDisk;
	master.dwSlotNo = dwMasterSlotNo;
	master.dwUnitNo = dwMasterUnitNo;
	mirror.dwSlotNo = dwMirrorSlotNo;
	mirror.dwUnitNo = dwMirrorUnitNo;

	masterUnitDisk = NdasCreateUnitDiskObject( &master );
	if ( masterUnitDisk.get() == NULL )
		return FALSE; // ERROR : Fail to create disk object
	mirrorUnitDisk = NdasCreateUnitDiskObject( &mirror );
	if ( mirrorUnitDisk.get() == NULL )
		return FALSE; // ERROR : Fail to create disk object

	if ( !masterUnitDisk->CanAccessExclusive() 
		|| !mirrorUnitDisk->CanAccessExclusive() )
	{
		// ERRROR : cannnot access disk exclusively
		return FALSE;
	}

	try {
		masterUnitDisk->Open( TRUE );
		mirrorUnitDisk->Open( TRUE );
	}
	catch( CNDASException & )
	{
		// ERROR : Fail to open disks
		masterUnitDisk->Close();
		mirrorUnitDisk->Close();
		return FALSE;
	}

	try { 
		CUnitDiskObjectVector vtUnitDisks;
		vtUnitDisks.push_back( masterUnitDisk );
		vtUnitDisks.push_back( mirrorUnitDisk );
		masterUnitDisk->Bind( vtUnitDisks, 0, NMT_RAID1 );
		mirrorUnitDisk->Bind( vtUnitDisks, 1, NMT_RAID1 );
		masterUnitDisk->SetDirty(TRUE);
		masterUnitDisk->MarkAllBitmap();
		masterUnitDisk->CommitDiskInfo();
		mirrorUnitDisk->CommitDiskInfo();
	}
	catch( CNDASException & )
	{
		// ERROR : Fail to write to disks
		masterUnitDisk->Close();
		mirrorUnitDisk->Close();
		return FALSE;
	}
	
	masterUnitDisk->Close();
	mirrorUnitDisk->Close();

	return TRUE;
}


BOOL
WINAPI
NdasSynchronize(DWORD dwSourceSlotNo, DWORD dwSourceUnitNo,
				DWORD dwMirrorSlotNo, DWORD dwMirrorUnitNo,
				LPSYNC_PROGRESS_ROUTINE lpProgressRoutine,
				LPVOID lpData, 
				LPBOOL pbCancel)
{
	CMirrorWorkThread workThread( NBSYNC_TYPE_SYNC_ONLY );
	CSyncObserver syncObserver;
	BOOL bResult;
	NDAS_UNITDEVICE master, mirror;
	CUnitDiskObjectPtr masterUnitDisk, mirrorUnitDisk;
	master.dwSlotNo = dwSourceSlotNo;
	master.dwUnitNo = dwSourceUnitNo;
	mirror.dwSlotNo = dwMirrorSlotNo;
	mirror.dwUnitNo = dwMirrorUnitNo;

	masterUnitDisk = NdasCreateUnitDiskObject( &master );
	if ( masterUnitDisk.get() == NULL )
		return FALSE; // ERROR : Fail to create disk object
	mirrorUnitDisk = NdasCreateUnitDiskObject( &mirror );
	if ( mirrorUnitDisk.get() == NULL )
		return FALSE; // ERROR : Fail to create disk object

	if ( !masterUnitDisk->CanAccessExclusive() 
		|| !mirrorUnitDisk->CanAccessExclusive() )
	{
		// ERRROR : cannnot access disk exclusively
		return FALSE;
	}

	workThread.SetSource( masterUnitDisk );
	workThread.SetDest( mirrorUnitDisk );
	workThread.Attach( &syncObserver );
	workThread.Execute();

	bResult = syncObserver.WaitForNotification( &workThread, lpProgressRoutine, lpData, pbCancel );
	
	return bResult;
}
