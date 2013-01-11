#include "LfsProc.h"


LFS				GlobalLfs;
LFS_REGISTRY	LfsRegistry;



VOID
LfsAttachToMountedDeviceWorker (
    IN PLFS_DEVICE_EXTENSION LfsDeviceExt
    );


BOOLEAN
LfsPnpPassThrough(
	IN  PLFS_DEVICE_EXTENSION	LfsDeviceExt,
	IN  PIRP					Irp,
	OUT PNTSTATUS				NtStatus
	);

VOID
CountIrpMajorFunction(
	PLFS_DEVICE_EXTENSION	LfsDeviceExt,
	UCHAR					IrpMajorFunction
	);


NTSTATUS
RegisterNdfsCallback(
	PDEVICE_OBJECT	DeviceObject
	);


NTSTATUS
UnRegisterNdfsCallback(
	PDEVICE_OBJECT	DeviceObject
	);


NTSTATUS
CallbackQueryPartitionInformation (
	IN  PDEVICE_OBJECT					RealDevice,
	OUT PNETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation
	);


NTSTATUS
CallbackQueryPrimaryAddress( 
	IN  PNETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation,
	OUT PLPX_ADDRESS					PrimaryAddress,
	IN  PBOOLEAN						IsLocalAddress	
	);


BOOLEAN
CallbackSecondaryToPrimary (
	IN  PDEVICE_OBJECT	RealDevice
	);


BOOLEAN
CallbackAddWriteRange(
	IN  PDEVICE_OBJECT	RealDevice,
	OUT PBLOCKACE_ID	BlockAceId
	);


VOID
CallbackRemoveWriteRange(
	IN  PDEVICE_OBJECT	RealDevice,
	OUT BLOCKACE_ID		BlockAceId
	);


NTSTATUS
CallbackGetNdasScsiBacl(
	IN	PDEVICE_OBJECT	DiskDeviceObject,
	OUT PNDAS_BLOCK_ACL	NdasBacl,
	IN BOOLEAN			SystemOrUser
	); 


#ifdef __LFS_FLUSH_VOLUME__

VOID
LfsDeviceExtThreadProc(
	IN	PLFS_DEVICE_EXTENSION	LfsDeviceExt
	);

#endif




NTSTATUS
LfsDriverEntry (
    IN PDRIVER_OBJECT	DriverObject,
    IN PUNICODE_STRING	RegistryPath,
	IN PDEVICE_OBJECT	ControlDeviceObject
	)
{
	PLFS	lfs = &GlobalLfs;
    USHORT	maxDepth = 16;
	PVOID	DGSvrCtx;
	PVOID	NtcCtx;

#if DBG
	
	ASSERT( sizeof(UCHAR)	== 1 );
	ASSERT( sizeof(USHORT)	== 2 );
	ASSERT( sizeof(ULONG)	== 4 );
	ASSERT( sizeof(ULONGLONG)== 8 );


	gFileSpyDebugLevel = 0;
	gFileSpyDebugLevel |= LFS_DEBUG_LFS_TRACE;
	gFileSpyDebugLevel |= LFS_DEBUG_LFS_TRACE;
	gFileSpyDebugLevel |= LFS_DEBUG_NETDISK_MANAGER_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_NETDISK_MANAGER_INFO;
	gFileSpyDebugLevel |= LFS_DEBUG_PRIMARY_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_PRIMARY_INFO;
	gFileSpyDebugLevel |= LFS_DEBUG_SECONDARY_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_SECONDARY_INFO;
	gFileSpyDebugLevel |= LFS_DEBUG_SECONDARY_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_TABLE_INFO;
	gFileSpyDebugLevel |= LFS_DEBUG_LIB_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_LIB_INFO;

	gFileSpyDebugLevel |= SPYDEBUG_DISPLAY_ATTACHMENT_NAMES;

	gFileSpyDebugLevel = SPYDEBUG_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_LFS_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_LFS_INFO;
	//gFileSpyDebugLevel |= LFS_DEBUG_NETDISK_MANAGER_INFO;
	gFileSpyDebugLevel |= LFS_DEBUG_LFS_INFO;

	DbgPrint( "Lfs DriverEntry %s %s\n", __DATE__, __TIME__ );
	
	DbgPrint( "sizeof(NDFS_WINXP_REQUEST_HEADER) = %d, sizeof(NDFS_WINXP_REPLY_HEADER) = %d\n",
			   sizeof(NDFS_WINXP_REQUEST_HEADER), sizeof(NDFS_WINXP_REPLY_HEADER) );

	ASSERT( DEFAULT_MAX_DATA_SIZE % 8 == 0 );
	ASSERT( sizeof(NDFS_WINXP_REQUEST_HEADER) % 8 == 0 );
	ASSERT( sizeof(NDFS_WINXP_REPLY_HEADER) % 8 == 0 );

#endif

	RtlZeroMemory( lfs, sizeof(LFS) );

    ExInitializeFastMutex( &lfs->FastMutex );
	lfs->ReferenceCount			= 1;

	lfs->DriverObject			= DriverObject;
	lfs->RegistryPath			= RegistryPath;
	lfs->ControlDeviceObject	= ControlDeviceObject;

	InitializeListHead( &lfs->LfsDeviceExtQueue );
	KeInitializeSpinLock( &lfs->LfsDeviceExtQSpinLock );

	lfs->ShutdownOccured		= FALSE;

	ExInitializeNPagedLookasideList( &lfs->NonPagedFcbLookasideList,
									 NULL,
									 NULL,
									 POOL_RAISE_IF_ALLOCATION_FAILURE,
									 sizeof(NON_PAGED_FCB),
									 LFS_FCB_NONPAGED_TAG,
									 maxDepth );

    ExInitializeNPagedLookasideList( &lfs->EResourceLookasideList,
									 NULL,
									 NULL,
									 POOL_RAISE_IF_ALLOCATION_FAILURE,
									 sizeof(ERESOURCE),
									 LFS_ERESOURCE_TAG,
									 maxDepth );
	
	
	if (CreateFastIoDispatch() == FALSE) {

		ASSERT( LFS_INSUFFICIENT_RESOURCES );
		LfsDriverUnload( DriverObject );
		return STATUS_INSUFFICIENT_RESOURCES;	
	}

	RtlZeroMemory( &lfs->NdfsCallback, sizeof(lfs->NdfsCallback) );
	lfs->NdfsCallback.Size = sizeof(lfs->NdfsCallback);
	lfs->NdfsCallback.QueryPartitionInformation = CallbackQueryPartitionInformation;
	lfs->NdfsCallback.QueryPrimaryAddress = CallbackQueryPrimaryAddress;
	lfs->NdfsCallback.SecondaryToPrimary = CallbackSecondaryToPrimary;
	lfs->NdfsCallback.AddWriteRange = CallbackAddWriteRange;
	lfs->NdfsCallback.RemoveWriteRange = CallbackRemoveWriteRange;
	lfs->NdfsCallback.GetNdasScsiBacl = CallbackGetNdasScsiBacl;


	lfs->NetdiskManager = NetdiskManager_Create(lfs);
	
	if (!lfs->NetdiskManager) {

		ASSERT( LFS_INSUFFICIENT_RESOURCES );
		LfsDriverUnload( DriverObject );
		return STATUS_INSUFFICIENT_RESOURCES;	
	}

	lfs->Primary = Primary_Create(lfs);
	
	if (!lfs->Primary) {

		ASSERT( LFS_INSUFFICIENT_RESOURCES );
		LfsDriverUnload( DriverObject );
		return STATUS_INSUFFICIENT_RESOURCES;	
	}

	lfs->LfsTable = LfsTable_Create(lfs);
	
	if (!lfs->LfsTable) {

		ASSERT( LFS_INSUFFICIENT_RESOURCES );
		LfsDriverUnload( DriverObject );
		return STATUS_INSUFFICIENT_RESOURCES;	
	}

	RdsvrDatagramInit( lfs->LfsTable, &DGSvrCtx, &NtcCtx );

	//
	//	Register Networking event callbacks.
	//	TODO: make it dynamic and more object-oriented.
	//

	lfs->NetEvtCtx.Callbacks[0]			= Primary_NetEvtCallback;
	lfs->NetEvtCtx.CallbackContext[0]	= lfs->Primary;
	lfs->NetEvtCtx.Callbacks[1]			= DgSvr_NetEvtCallback;
	lfs->NetEvtCtx.CallbackContext[1]	= DGSvrCtx;
	lfs->NetEvtCtx.Callbacks[2]			= DgNtc_NetEvtCallback;
	lfs->NetEvtCtx.CallbackContext[2]	= NtcCtx;
	lfs->NetEvtCtx.CallbackCnt			= 3;

	NetEvtInit( &lfs->NetEvtCtx );

	XEvtqInitializeEventQueueManager( &lfs->EvtQueueMgr );

	return STATUS_SUCCESS;
}


VOID
LfsDriverUnload (
    IN PDRIVER_OBJECT	DriverObject
	)
{
	UNREFERENCED_PARAMETER( DriverObject );

	XEvtqDestroyEventQueueManager( &GlobalLfs.EvtQueueMgr );

	NetEvtTerminate( &GlobalLfs.NetEvtCtx );

	if (GlobalLfs.LfsTable) {

		RdsvrDatagramDestroy();
		LfsTable_Close( GlobalLfs.LfsTable );
	}

	GlobalLfs.LfsTable = NULL;

	if (GlobalLfs.Primary)
		Primary_Close( GlobalLfs.Primary );

	GlobalLfs.Primary = NULL;

	if (GlobalLfs.NetdiskManager)
		NetdiskManager_Close( GlobalLfs.NetdiskManager );

	GlobalLfs.NetdiskManager = NULL;

	ASSERT( LfsObjectCounts.LfsDeviceExtCount == 0 );
	
	LfsDereference( &GlobalLfs );

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsDriverUnload: Finished\n") );

	return;
}


VOID
LfsReference (
	PLFS	Lfs
	)
{
    LONG result;
	
    result = InterlockedIncrement( &Lfs->ReferenceCount );
    ASSERT ( result >= 0 );
}


VOID
LfsDereference (
	PLFS	Lfs
	)
{
    LONG result;


    result = InterlockedDecrement( &Lfs->ReferenceCount );
    ASSERT( result >= 0 );

    if (result == 0) {

		CloseFastIoDispatch();

	    ExDeleteNPagedLookasideList( &Lfs->EResourceLookasideList );
		ExDeleteNPagedLookasideList( &Lfs->NonPagedFcbLookasideList );

		ASSERT( Lfs->LfsDeviceExtQueue.Flink == Lfs->LfsDeviceExtQueue.Blink );

		ASSERT( LfsObjectCounts.LfsDeviceExtCount == 0 );
		
		ASSERT( LfsObjectCounts.PrimarySessionCount == 0 );
		ASSERT( LfsObjectCounts.PrimarySessionThreadCount == 0 );
		ASSERT( LfsObjectCounts.OpenFileCount == 0 );
		
		ASSERT( LfsObjectCounts.SecondaryCount == 0 );
		ASSERT( LfsObjectCounts.SecondaryThreadCount == 0 );
		ASSERT( LfsObjectCounts.SecondaryRequestCount == 0 );
		ASSERT( LfsObjectCounts.FcbCount == 0 );
		ASSERT( LfsObjectCounts.FileExtCount == 0 );

		ASSERT( LfsObjectCounts.EnabledNetdiskCount == 0 );
		ASSERT( LfsObjectCounts.NetdiskPartitionCount == 0 );
		ASSERT( LfsObjectCounts.NetdiskManagerRequestCount == 0 );

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
				("---------------------LfsDereference: Lfs is Freed-----------------------\n") );
	}
}


NTSTATUS
LfsInitializeLfsDeviceExt(
	IN PENABLED_NETDISK	EnabledNetdisk,
    IN PDEVICE_OBJECT	FileSpyDeviceObject,
    IN PDEVICE_OBJECT	DiskDeviceObject
	)
{
    PFILESPY_DEVICE_EXTENSION	devExt = FileSpyDeviceObject->DeviceExtension;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt = &devExt->LfsDeviceExt;

	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
				   ("LfsAttachToFileSystemDevice: Entered FileSpyDeviceObject = %p\n", FileSpyDeviceObject));

	ASSERT( lfsDeviceExt->ReferenceCount == 0 );

	LfsReference( &GlobalLfs );

	RtlZeroMemory( lfsDeviceExt, sizeof(LFS_DEVICE_EXTENSION) );

    ExInitializeFastMutex( &lfsDeviceExt->FastMutex );
	lfsDeviceExt->ReferenceCount		= 1;

	InitializeListHead( &lfsDeviceExt->LfsQListEntry );
	InitializeListHead( &lfsDeviceExt->PrimaryQListEntry );

	lfsDeviceExt->Flags = LFS_DEVICE_INITIALIZING;

	lfsDeviceExt->FileSpyDeviceObject	= FileSpyDeviceObject;
	lfsDeviceExt->FileSpyDevExt			= devExt;
	lfsDeviceExt->EnabledNetdisk		= EnabledNetdisk;

	lfsDeviceExt->Purge					= FALSE;
	lfsDeviceExt->HookFastIo			= FALSE;		//Allow FastIo to pass through
	lfsDeviceExt->Filtering				= FALSE;
	lfsDeviceExt->FilteringMode			= LFS_NO_FILTERING;

	lfsDeviceExt->DiskDeviceObject		= DiskDeviceObject;

	ExInterlockedInsertTailList( &GlobalLfs.LfsDeviceExtQueue,
								 &lfsDeviceExt->LfsQListEntry,
								 &GlobalLfs.LfsDeviceExtQSpinLock );

#if DBG
	LfsObjectCounts.LfsDeviceExtCount ++;
#endif

	return STATUS_SUCCESS;
}


VOID
LfsIoDeleteDevice (
    IN PDEVICE_OBJECT DeviceObject
    )
{
    PFILESPY_DEVICE_EXTENSION devExt;
	PLFS_DEVICE_EXTENSION	  lfsDeviceExt;

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsIoDeleteDevice: Entered DeviceObject = %p\n", DeviceObject) );

	if (DeviceObject == GlobalLfs.ControlDeviceObject) {

        IoDeleteDevice(DeviceObject);
		return;
	}

    devExt = DeviceObject->DeviceExtension;
	lfsDeviceExt = &devExt->LfsDeviceExt;

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsIoDeleteDevice: Entered lfsDeviceExt = %p\n", lfsDeviceExt) );

	ASSERT( lfsDeviceExt->ReferenceCount );

	LfsDeviceExt_Dereference( lfsDeviceExt );
}


VOID
LfsDeviceExt_Reference (
	PLFS_DEVICE_EXTENSION	LfsDeviceExt
	)
{
    LONG result;
	
    result = InterlockedIncrement( &LfsDeviceExt->ReferenceCount );

    ASSERT( result >= 0 );
}


VOID
LfsDeviceExt_Dereference (
	PLFS_DEVICE_EXTENSION	LfsDeviceExt
	)
{
    LONG result;


    result = InterlockedDecrement( &LfsDeviceExt->ReferenceCount );
    
	if (result < 0) {

		ASSERT( LFS_BUG );
		return;
	}

    if (result == 0) {

		KIRQL	oldIrql;

		ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
		ASSERT( LfsDeviceExt->PrimaryQListEntry.Flink == LfsDeviceExt->PrimaryQListEntry.Blink );
		ASSERT( LfsDeviceExt->Secondary == NULL );
		
		KeAcquireSpinLock( &GlobalLfs.LfsDeviceExtQSpinLock, &oldIrql );
		RemoveEntryList( &LfsDeviceExt->LfsQListEntry );
		KeReleaseSpinLock( &GlobalLfs.LfsDeviceExtQSpinLock, oldIrql );
		
		if (LfsDeviceExt->Buffer) {

			ExFreePoolWithTag( LfsDeviceExt->Buffer, LFS_ALLOC_TAG );
			LfsDeviceExt->Buffer = NULL;
		}

		if (LfsDeviceExt->FileSpyDeviceObject) {

			IoDeleteDevice( LfsDeviceExt->FileSpyDeviceObject );
		}

#if DBG
		LfsObjectCounts.LfsDeviceExtCount --;
#endif
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
					   ("LfsDevExtDereference: Lfs Device Extension is Freed LfsDeviceExt = %p, LfsObjectCounts.LfsDeviceExtCount = %d\n", 
						 LfsDeviceExt, LfsObjectCounts.LfsDeviceExtCount) );

		LfsDereference( &GlobalLfs );
	}
}


//
//	Query NDAS usage status by going through all lfs filter extensions.
//

NTSTATUS
LfsDeviceExt_QueryNDASUsage(
	IN  UINT32	SlotNo,
	OUT PUINT32	NdasUsageFlags,
	OUT PUINT32	MountedFSVolumeCount
	)
{
	PLIST_ENTRY				listHead = &GlobalLfs.LfsDeviceExtQueue;
	PLIST_ENTRY				listEntry;
	KIRQL					oldIrql;
	PLFS_DEVICE_EXTENSION	lfsDeviceExt;
	LONG					cnt;
	LONG					attachedVolCnt;

	if (NdasUsageFlags == NULL) {

		return STATUS_INVALID_PARAMETER;
	}

	*NdasUsageFlags = 0;
	cnt = 0;
	attachedVolCnt = 0;

	KeAcquireSpinLock( &GlobalLfs.LfsDeviceExtQSpinLock, &oldIrql );
	
	for (listEntry = listHead->Flink; listEntry != listHead; listEntry = listEntry->Flink) {

		lfsDeviceExt = CONTAINING_RECORD( listEntry, LFS_DEVICE_EXTENSION, LfsQListEntry );

		if (lfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.SlotNo == SlotNo) {

			cnt++;

			//
			//	Determine operation mode
			//

			if (lfsDeviceExt->FilteringMode == LFS_SECONDARY_TO_PRIMARY || lfsDeviceExt->FilteringMode == LFS_PRIMARY) {

				*NdasUsageFlags |= LFS_NDASUSAGE_FLAG_PRIMARY;
				attachedVolCnt++;

			} else if (lfsDeviceExt->FilteringMode == LFS_SECONDARY) {

				*NdasUsageFlags |= LFS_NDASUSAGE_FLAG_SECONDARY;
				attachedVolCnt++;

				//
				//	NOTE: workaround for FAT32
				//		FAT32 does not seem to create primary volumes
				//		beside secondary volume.
				//		Check the secondary volume's state to see if this is
				//		acting primary host.
				//

				if(lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_FAT &&
				lfsDeviceExt->SecondaryState == CONNECT_TO_LOCAL_STATE) {

					*NdasUsageFlags |= LFS_NDASUSAGE_FLAG_PRIMARY;
				}

			} else if (lfsDeviceExt->FilteringMode == LFS_READ_ONLY) {

				*NdasUsageFlags |= LFS_NDASUSAGE_FLAG_READONLY;
				attachedVolCnt++;

			}

			//
			//	Determine if this volume is locked
			//

			if (lfsDeviceExt->VolumeLocked) {

				*NdasUsageFlags |= LFS_NDASUSAGE_FLAG_LOCKED;
			}
		}
	}

	//
	//	Determine if LFSFilter is attached to a disk volume.
	//

	if (attachedVolCnt) {

		*NdasUsageFlags |= LFS_NDASUSAGE_FLAG_ATTACHED;
	
	} else {

		*NdasUsageFlags |= LFS_NDASUSAGE_FLAG_NODISKVOL;
	}

	KeReleaseSpinLock( &GlobalLfs.LfsDeviceExtQSpinLock, oldIrql );

	*MountedFSVolumeCount = cnt;

	return STATUS_SUCCESS;
}


NTSTATUS (*LfsOrginalNtfsAcquireFileForModWrite)(PFILE_OBJECT	FileObject,
												 PLARGE_INTEGER	EndingOffset,
												 PERESOURCE		*ResourceToRelease,
												 PDEVICE_OBJECT DeviceObject) = NULL;

NTSTATUS
LfsNtfsAcquireFileForModWrite (
    IN PFILE_OBJECT		FileObject,
    IN PLARGE_INTEGER	EndingOffset,
    OUT PERESOURCE		*ResourceToRelease,
    IN PDEVICE_OBJECT	DeviceObject
    )
{
	PDEVICE_OBJECT attachedDeviceObject;

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsAcquireFileForModWrite: Entered DeviceObject = %p\n", DeviceObject) );

	if (SpyIsAttachedToDevice(DeviceObject, &attachedDeviceObject)) {

		PLFS_DEVICE_EXTENSION	lfsDeviceExt = &((PFILESPY_DEVICE_EXTENSION)(attachedDeviceObject->DeviceExtension))->LfsDeviceExt;

		if (lfsDeviceExt &&
			lfsDeviceExt->FilteringMode == LFS_SECONDARY &&
			lfsDeviceExt->Secondary &&
			Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, FileObject) != NULL) {

			ObDereferenceObject( attachedDeviceObject );
			*ResourceToRelease = NULL;
			
			return STATUS_SUCCESS;
		}

		ObDereferenceObject( attachedDeviceObject );
		
		return LfsOrginalNtfsAcquireFileForModWrite( FileObject, EndingOffset, ResourceToRelease, DeviceObject );
	}

	return LfsOrginalNtfsAcquireFileForModWrite( FileObject, EndingOffset, ResourceToRelease, DeviceObject );
}


NTSTATUS
LfsAttachToFileSystemDevice (
    IN PDEVICE_OBJECT	DeviceObject,
    IN PDEVICE_OBJECT	FileSpyDeviceObject,
    IN PUNICODE_STRING	DeviceName
    )
{
	PLFS_DEVICE_EXTENSION	lfsDeviceExt = &((PFILESPY_DEVICE_EXTENSION)(FileSpyDeviceObject->DeviceExtension))->LfsDeviceExt;
	UNICODE_STRING			fsName;
	BOOLEAN					result;


	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsAttachToFileSystemDevice: Entered lfsDeviceExt = %p\n", lfsDeviceExt) );

	
	ASSERT( lfsDeviceExt->ReferenceCount == 1 );

	lfsDeviceExt->FilteringMode	= LFS_FILE_CONTROL;

	lfsDeviceExt->Vpb						= NULL;
	lfsDeviceExt->DiskDeviceObject			= NULL;
	lfsDeviceExt->BaseVolumeDeviceObject	= DeviceObject;
	lfsDeviceExt->AttachedToDeviceObject	= ((PFILESPY_DEVICE_EXTENSION)(FileSpyDeviceObject->DeviceExtension))->AttachedToDeviceObject;

#if (WINVER >= 0x0501 && DBG)

	if(IS_WINDOWSXP_OR_LATER()) {

		PDEVICE_OBJECT baseVolumeDeviceObject;
	
		baseVolumeDeviceObject = (gSpyDynamicFunctions.GetDeviceAttachmentBaseRef)(lfsDeviceExt->BaseVolumeDeviceObject );
		ASSERT( lfsDeviceExt->BaseVolumeDeviceObject == baseVolumeDeviceObject );
		ObDereferenceObject( baseVolumeDeviceObject );
	}

#endif

    RtlInitEmptyUnicodeString( &lfsDeviceExt->FileSystemVolumeName,
							   lfsDeviceExt->FileSystemVolumeNameBuffer,
							   sizeof(lfsDeviceExt->FileSystemVolumeNameBuffer) );

	RtlCopyUnicodeString( &lfsDeviceExt->FileSystemVolumeName, DeviceName );	

	do {
	
		RtlInitUnicodeString( &fsName, L"\\Ntfs" );

		result = RtlEqualUnicodeString(	&lfsDeviceExt->FileSystemVolumeName,
										&fsName,
										TRUE );
								
		if (result == 1) {

	        PFAST_IO_DISPATCH	fastIoDispatch;

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsAttachToFileSystemDevice: Name compare %d %wZ %wZ\n",
											     result, &lfsDeviceExt->FileSystemVolumeName, &fsName) );

		    FsRtlEnterFileSystem();

			fastIoDispatch = lfsDeviceExt->BaseVolumeDeviceObject->DriverObject->FastIoDispatch;

			// Prevent Direct I/O

			if (fastIoDispatch->AcquireForModWrite) {

				LfsOrginalNtfsAcquireFileForModWrite = fastIoDispatch->AcquireForModWrite;
				fastIoDispatch->AcquireForModWrite = LfsNtfsAcquireFileForModWrite;
			}

			FsRtlExitFileSystem();

			lfsDeviceExt->Filtering = TRUE;
			lfsDeviceExt->FileSystemType = LFS_FILE_SYSTEM_NTFS;

			break;
		} 

		RtlInitUnicodeString( &fsName, L"\\Fat" );

		result = RtlEqualUnicodeString( &lfsDeviceExt->FileSystemVolumeName,
										&fsName,
										TRUE );

		if (result == 1) {

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsAttachToFileSystemDevice: Name compare %d %wZ %wZ\n",
											     result, &lfsDeviceExt->FileSystemVolumeName, &fsName) );

			lfsDeviceExt->Filtering = TRUE;
			lfsDeviceExt->FileSystemType = LFS_FILE_SYSTEM_FAT;

			break;
		} 

		RtlInitUnicodeString( &fsName, L"\\NdFat" );

		result = RtlEqualUnicodeString( &lfsDeviceExt->FileSystemVolumeName,
										&fsName,
										TRUE );

		if (result == 1) {

			NTSTATUS	status;

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsAttachToFileSystemDevice: Name compare %d %wZ %wZ\n",
											     result, &lfsDeviceExt->FileSystemVolumeName, &fsName) );

			lfsDeviceExt->FileSystemType = LFS_FILE_SYSTEM_NDFAT;

			status = RegisterNdfsCallback( DeviceObject );

			if (status == STATUS_SUCCESS) {

				SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_REGISTERED );
				lfsDeviceExt->Filtering = TRUE;
			
			} else {

				lfsDeviceExt->Filtering = FALSE;
			}

			break;
		} 

		RtlInitUnicodeString( &fsName, L"\\NdNtfs" );

		result = RtlEqualUnicodeString( &lfsDeviceExt->FileSystemVolumeName,
										&fsName,
										TRUE );

		if (result == 1) {

			NTSTATUS	status;

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsAttachToFileSystemDevice: Name compare %d %wZ %wZ\n",
												 result, &lfsDeviceExt->FileSystemVolumeName, &fsName) );

			lfsDeviceExt->FileSystemType = LFS_FILE_SYSTEM_NDNTFS;

			status = RegisterNdfsCallback( DeviceObject );

			if (status == STATUS_SUCCESS) {

				SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_REGISTERED );
				lfsDeviceExt->Filtering = TRUE;

			} else {

				lfsDeviceExt->Filtering = FALSE;
			}

			break;
		} 
		
		lfsDeviceExt->FileSystemType = LFS_FILE_SYSTEM_OTHER;
	
	} while (0);
	
	ClearFlag( lfsDeviceExt->Flags, LFS_DEVICE_INITIALIZING );
	SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_START );
	
	return STATUS_SUCCESS;
}


VOID
LfsDetachFromFileSystemDevice (
    IN PDEVICE_OBJECT FileSpyDeviceObject
    )
{
	PLFS_DEVICE_EXTENSION	lfsDeviceExt = &((PFILESPY_DEVICE_EXTENSION)(FileSpyDeviceObject->DeviceExtension))->LfsDeviceExt;
	PFAST_IO_DISPATCH		fastIoDispatch;

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsDetachFromFileSystemDevice: Entered lfsDeviceExt = %p\n", lfsDeviceExt) );

	ASSERT( lfsDeviceExt->FilteringMode == LFS_FILE_CONTROL );

    FsRtlEnterFileSystem();
    
	fastIoDispatch = lfsDeviceExt->BaseVolumeDeviceObject->DriverObject->FastIoDispatch;

	// Prevent Direct I/O
	//lfsDeviceExt->OriginalAcquireForModWrite = fastIoDispatch->AcquireForModWrite;

	if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NTFS && fastIoDispatch->AcquireForModWrite && LfsOrginalNtfsAcquireFileForModWrite) {
		
		fastIoDispatch->AcquireForModWrite = LfsOrginalNtfsAcquireFileForModWrite;
		LfsOrginalNtfsAcquireFileForModWrite = NULL;	
	} 
	
	FsRtlExitFileSystem();

#ifdef __NDFS__
	if (FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_REGISTERED)) {

		UnRegisterNdfsCallback( lfsDeviceExt->BaseVolumeDeviceObject );
		ClearFlag( lfsDeviceExt->Flags, LFS_DEVICE_REGISTERED );
	}
#endif

	ASSERT( lfsDeviceExt->ReferenceCount != 0 );
	SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_STOP );
		
	return;
}


NTSTATUS
LfsAttachToMountedDevice(
    IN PDEVICE_OBJECT	FsDeviceObject,
    IN PDEVICE_OBJECT	VolumeDeviceObject,
    IN PDEVICE_OBJECT	NewDeviceObject
    )
{
	PLFS_DEVICE_EXTENSION	fsLfsDeviceExt = &((PFILESPY_DEVICE_EXTENSION)(FsDeviceObject->DeviceExtension))->LfsDeviceExt;
	PLFS_DEVICE_EXTENSION	newLfsDeviceExt = &((PFILESPY_DEVICE_EXTENSION)(NewDeviceObject->DeviceExtension))->LfsDeviceExt;
	NTSTATUS				getStatus;
    PDEVICE_OBJECT			diskDeviceObject;


	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,("LfsAttachToMountedDevice: Entered newLfsDeviceExt = %p\n", newLfsDeviceExt) );	
	ASSERT( fsLfsDeviceExt );
	ASSERT( newLfsDeviceExt->ReferenceCount == 1 );

	if (fsLfsDeviceExt->Filtering != TRUE) {

		newLfsDeviceExt->Filtering = FALSE;
		return STATUS_UNSUCCESSFUL;
	}

	getStatus = (gSpyDynamicFunctions.GetDiskDeviceObject)( VolumeDeviceObject, &diskDeviceObject );
	
	if (!NT_SUCCESS(getStatus)) {

		ASSERT( LFS_UNEXPECTED );
		newLfsDeviceExt->Filtering = FALSE;
		return STATUS_UNSUCCESSFUL;
	}

	newLfsDeviceExt->FileSystemType			= fsLfsDeviceExt->FileSystemType;

	newLfsDeviceExt->Vpb					= diskDeviceObject->Vpb;
	newLfsDeviceExt->DiskDeviceObject		= newLfsDeviceExt->Vpb->RealDevice;
	newLfsDeviceExt->BaseVolumeDeviceObject	= newLfsDeviceExt->Vpb->DeviceObject;
	newLfsDeviceExt->AttachedToDeviceObject	= ((PFILESPY_DEVICE_EXTENSION)(NewDeviceObject->DeviceExtension))->AttachedToDeviceObject;
	
#if WINVER >= 0x0501
	if(IS_WINDOWSXP_OR_LATER()) {

		PDEVICE_OBJECT baseVolumeDeviceObject;
	
		baseVolumeDeviceObject = (gSpyDynamicFunctions.GetDeviceAttachmentBaseRef)(newLfsDeviceExt->Vpb->DeviceObject);
		ASSERT( newLfsDeviceExt->BaseVolumeDeviceObject == baseVolumeDeviceObject );
		ObDereferenceObject( baseVolumeDeviceObject );
	}
#endif

	LfsDeviceExt_Reference( newLfsDeviceExt );

	ExInitializeWorkItem( &newLfsDeviceExt->WorkItem,
						  LfsAttachToMountedDeviceWorker,
						  newLfsDeviceExt );

	ExQueueWorkItem( &newLfsDeviceExt->WorkItem, DelayedWorkQueue );

	return STATUS_SUCCESS;
}


VOID
LfsAttachToMountedDeviceWorker (
    IN PLFS_DEVICE_EXTENSION LfsDeviceExt
    )
{
	UNREFERENCED_PARAMETER(LfsDeviceExt);

	LfsDeviceExt->Filtering = FALSE;
	ClearFlag( LfsDeviceExt->Flags, LFS_DEVICE_INITIALIZING );
	SetFlag( LfsDeviceExt->Flags, LFS_DEVICE_START );
	LfsDeviceExt_Dereference( LfsDeviceExt );
	
	return;
}


NTSTATUS
LfsFsControlMountVolumeComplete(
    IN PDEVICE_OBJECT	FsDeviceObject,
    IN PIRP				Irp,
    IN PDEVICE_OBJECT	NewDeviceObject
    )
{
	NTSTATUS				status;
	PLFS_DEVICE_EXTENSION	fsLfsDeviceExt = &((PFILESPY_DEVICE_EXTENSION)(FsDeviceObject->DeviceExtension))->LfsDeviceExt;
	PLFS_DEVICE_EXTENSION	newLfsDeviceExt = &((PFILESPY_DEVICE_EXTENSION)(NewDeviceObject->DeviceExtension))->LfsDeviceExt;


	UNREFERENCED_PARAMETER( Irp );

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsFsControlMountVolumeComplete: Entered newLfsDeviceExt = %p %wZ\n", 
										 newLfsDeviceExt, &newLfsDeviceExt->NetdiskPartitionInformation.VolumeName) );

	if (fsLfsDeviceExt->Filtering != TRUE) {

		newLfsDeviceExt->Filtering = FALSE;
		return STATUS_UNSUCCESSFUL;
	}

	ASSERT( NT_SUCCESS(Irp->IoStatus.Status)				   ||
		    Irp->IoStatus.Status == STATUS_UNRECOGNIZED_VOLUME || 
			Irp->IoStatus.Status == STATUS_FILE_CORRUPT_ERROR  ||
			Irp->IoStatus.Status == STATUS_NO_MEDIA_IN_DEVICE  ||
			Irp->IoStatus.Status == STATUS_DEVICE_NOT_READY    ||
			Irp->IoStatus.Status == STATUS_NO_SUCH_DEVICE      ||
			Irp->IoStatus.Status == STATUS_DISK_CORRUPT_ERROR ); 

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
					   ("newLfsDeviceExt = %p %wZ Irp->IoStatus.Status = %x, vpb->RealDevice = %p, vpb->DeviceObject = %p\n", 
						 newLfsDeviceExt, &newLfsDeviceExt->NetdiskPartitionInformation.VolumeName,
						 Irp->IoStatus.Status, newLfsDeviceExt->DiskDeviceObject, 
						 newLfsDeviceExt->DiskDeviceObject->Vpb->DeviceObject) );

	if (!NT_SUCCESS(Irp->IoStatus.Status)) {

		NetdiskManager_MountVolumeComplete( GlobalLfs.NetdiskManager,
											newLfsDeviceExt->EnabledNetdisk,
											newLfsDeviceExt,
											Irp->IoStatus.Status );

		return Irp->IoStatus.Status;
	}
	
	ASSERT( newLfsDeviceExt->DiskDeviceObject->Vpb->DeviceObject );
	ASSERT( newLfsDeviceExt->ReferenceCount == 2 );

	SetFlag( newLfsDeviceExt->Flags, LFS_DEVICE_MOUNTING );

	newLfsDeviceExt->FileSystemType			= fsLfsDeviceExt->FileSystemType;
	newLfsDeviceExt->Vpb					= newLfsDeviceExt->DiskDeviceObject->Vpb; // Vpb will be changed in IrpSp Why ?
	newLfsDeviceExt->BaseVolumeDeviceObject	= newLfsDeviceExt->Vpb->DeviceObject;
	
#if WINVER >= 0x0501

	if (IS_WINDOWSXP_OR_LATER()) {

		PDEVICE_OBJECT baseVolumeDeviceObject;
	
		baseVolumeDeviceObject = (gSpyDynamicFunctions.GetDeviceAttachmentBaseRef)(newLfsDeviceExt->BaseVolumeDeviceObject);
		ASSERT( newLfsDeviceExt->BaseVolumeDeviceObject == baseVolumeDeviceObject );
		ObDereferenceObject( baseVolumeDeviceObject );
	}

#endif

	newLfsDeviceExt->BufferLength	= FS_BUFFER_SIZE;
	newLfsDeviceExt->Buffer			= ExAllocatePoolWithTag( NonPagedPool, FS_BUFFER_SIZE, LFS_ALLOC_TAG );

	if (newLfsDeviceExt->Buffer == NULL) {

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("Failed to allocate FS_BUFFER\n") );
		RtlZeroMemory( newLfsDeviceExt, sizeof(LFS_DEVICE_EXTENSION) );

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	switch (newLfsDeviceExt->FilteringMode) {

	case LFS_READ_ONLY: {

		ClearFlag( newLfsDeviceExt->Flags, LFS_DEVICE_INITIALIZING|LFS_DEVICE_MOUNTING );
		SetFlag( newLfsDeviceExt->Flags, LFS_DEVICE_START );
		
		//
		//	We don't support cache purging yet.
		//

		newLfsDeviceExt->Purge = FALSE;
		
		newLfsDeviceExt->Filtering = TRUE;
		newLfsDeviceExt->HookFastIo = TRUE;		// disallow FastIo to pass through the next device.

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsFsControlMountVolumeComplete: READONLY PRIMARY Filtering:%d HookFastIo:%d Purge:%d.\n",
											 newLfsDeviceExt->Filtering, newLfsDeviceExt->HookFastIo, newLfsDeviceExt->Purge));
		
		status = STATUS_SUCCESS;
		break;
	}

	case LFS_PRIMARY: {

		newLfsDeviceExt->Filtering = TRUE;
		newLfsDeviceExt->HookFastIo = FALSE;		// allow FastIo to pass through the next device.
		ClearFlag( newLfsDeviceExt->Flags, LFS_DEVICE_INITIALIZING|LFS_DEVICE_MOUNTING );
		SetFlag( newLfsDeviceExt->Flags, LFS_DEVICE_START );

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
					   ("LfsFsControlMountVolumeComplete: READWRITE PRIMARY Filtering:%d HookFastIo:%d Purge:%d.\n",
						 newLfsDeviceExt->Filtering, newLfsDeviceExt->HookFastIo, newLfsDeviceExt->Purge) );

		status = STATUS_SUCCESS;
		break;
	}

	case LFS_SECONDARY: {

		ULONG		tryCreateSecondary;

#ifdef __LFS_NDAS_NTFS__
		if (newLfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT ||
			newLfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDNTFS) {
#else
		if (newLfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT) {
#endif

			newLfsDeviceExt->Filtering = TRUE;
			newLfsDeviceExt->HookFastIo = FALSE;

			ClearFlag( newLfsDeviceExt->Flags, LFS_DEVICE_INITIALIZING|LFS_DEVICE_MOUNTING );
			SetFlag( newLfsDeviceExt->Flags, LFS_DEVICE_START );

			status = STATUS_SUCCESS;
			break;
		}

		//
		//	Try to connect to the primary
		//

//#define TRY_COUNT	120   // about 10 minutes!!
//#define TRY_COUNT	 24   // about  2 minutues  
#define TRY_COUNT	 12   // about  1 minutues

		for (tryCreateSecondary = 0; tryCreateSecondary<TRY_COUNT; tryCreateSecondary++) {

			PSECONDARY				secondary;
			BOOLEAN					upgradeResult;		


			secondary = Secondary_Create( newLfsDeviceExt );
		
			if (secondary) {

				newLfsDeviceExt->Secondary = secondary;
				break;
			}

			upgradeResult = NetdiskManager_Secondary2Primary( GlobalLfs.NetdiskManager, 
															  newLfsDeviceExt->EnabledNetdisk, 
															  newLfsDeviceExt );

			if (upgradeResult != TRUE) {

				continue;
			}

			newLfsDeviceExt->FilteringMode = LFS_SECONDARY_TO_PRIMARY;

			break;
		}

		if (tryCreateSecondary == TRY_COUNT) { // Fail
		
			ASSERT( LFS_REQUIRED );
			//
			//	going to READONLY_MODE
			//
			newLfsDeviceExt->FilteringMode = LFS_READ_ONLY;

			//
			//	Queue an event to notify user applications
			//

			XevtQueueVolumeInvalidOrLocked( -1,
											newLfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.SlotNo,
											newLfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.UnitDiskNo );

			//
			//	Try to unplug
			//

			status = NDCtrlUnplug( newLfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.SlotNo );
			ASSERT( NT_SUCCESS(status) );

			newLfsDeviceExt->Filtering = FALSE;

			status = STATUS_UNSUCCESSFUL;
			break;
		}

		ClearFlag( newLfsDeviceExt->Flags, LFS_DEVICE_INITIALIZING|LFS_DEVICE_MOUNTING );
		SetFlag( newLfsDeviceExt->Flags, LFS_DEVICE_START );

		newLfsDeviceExt->Filtering = TRUE;

		if (newLfsDeviceExt->FilteringMode == LFS_SECONDARY) {

			newLfsDeviceExt->HookFastIo = TRUE;		// disallow FastIo to pass through the next device.
			newLfsDeviceExt->PurgeVolumeSafe = TRUE;

			if (newLfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NTFS)
				newLfsDeviceExt->SecondaryState = WAIT_PURGE_SAFE_STATE;
			else if (newLfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_FAT)
				newLfsDeviceExt->SecondaryState = SECONDARY_STATE;
			else
				ASSERT( LFS_BUG );

			RtlInitUnicodeString( &newLfsDeviceExt->MountMgr, L"\\:$MountMgrRemoteDatabase" );
			RtlInitUnicodeString( &newLfsDeviceExt->ExtendPlus, L"\\$Extend\\$Reparse:$R:$INDEX_ALLOCATION" );	
		}

#if DBG
		if (newLfsDeviceExt->FilteringMode == LFS_SECONDARY || newLfsDeviceExt->FilteringMode == LFS_SECONDARY_TO_PRIMARY) {

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
						   ("LfsFsControlMountVolumeComplete: READWRITE SECONDARY Filtering:%d HookFastIo:%d Purge:%d.\n",
						   newLfsDeviceExt->Filtering, newLfsDeviceExt->HookFastIo, newLfsDeviceExt->Purge) );
		
		} else {

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
						   ("LfsFsControlMountVolumeComplete: READONLY SECONDARY Filtering:%d HookFastIo:%d Purge:%d.\n",
							 newLfsDeviceExt->Filtering, newLfsDeviceExt->HookFastIo, newLfsDeviceExt->Purge) );
		}
#endif
		
		status = STATUS_SUCCESS;
		break;
	}

	default:

		ASSERT( LFS_BUG );
		status = STATUS_SUCCESS;

		break;
	}

	return status;
}


VOID
LfsPostAttachToMountedDevice (
    IN PDEVICE_OBJECT	DeviceObject,
    IN PDEVICE_OBJECT	FilespyDeviceObject,
	IN PIRP				Irp
    )
{
	PLFS_DEVICE_EXTENSION	lfsDeviceExt = &((PFILESPY_DEVICE_EXTENSION)(FilespyDeviceObject->DeviceExtension))->LfsDeviceExt;


	UNREFERENCED_PARAMETER( DeviceObject );

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
				   ("SpyAttachToMountedDevice: Entered lfsDeviceExt = %p\n", lfsDeviceExt) );

	
	ASSERT( lfsDeviceExt->ReferenceCount > 0 );

	if (lfsDeviceExt->Filtering != TRUE) {

		return;
	}

	lfsDeviceExt->AttachedToDeviceObject = ((PFILESPY_DEVICE_EXTENSION)(FilespyDeviceObject->DeviceExtension))->AttachedToDeviceObject;

	NetdiskManager_MountVolumeComplete( GlobalLfs.NetdiskManager,
										lfsDeviceExt->EnabledNetdisk,
										lfsDeviceExt,
										Irp->IoStatus.Status );
	
	if (lfsDeviceExt->FilteringMode == LFS_SECONDARY_TO_PRIMARY) {
		
		NetdiskManager_ChangeMode( GlobalLfs.NetdiskManager,
								   lfsDeviceExt->EnabledNetdisk,
								   lfsDeviceExt );
	}

#ifdef __LFS_FLUSH_VOLUME__

	if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NTFS &&
		(lfsDeviceExt->FilteringMode == LFS_PRIMARY || lfsDeviceExt->FilteringMode == LFS_SECONDARY_TO_PRIMARY)) {

		do {

			NTSTATUS			status;
			OBJECT_ATTRIBUTES	objectAttributes;
			LARGE_INTEGER		timeOut;

			KeInitializeEvent( &lfsDeviceExt->ReadyEvent, NotificationEvent, FALSE );
			KeInitializeEvent( &lfsDeviceExt->RequestEvent, NotificationEvent, FALSE );

			KeQuerySystemTime( &lfsDeviceExt->TryFlushOrPurgeTime );

			InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

			status = PsCreateSystemThread( &lfsDeviceExt->ThreadHandle,
										   THREAD_ALL_ACCESS,
										   &objectAttributes,
										   NULL,
										   NULL,
										   LfsDeviceExtThreadProc,
										   lfsDeviceExt );

			if (!NT_SUCCESS(status)) {

				break;
			}

			status = ObReferenceObjectByHandle( lfsDeviceExt->ThreadHandle,
												FILE_READ_DATA,
												NULL,
												KernelMode,
												&lfsDeviceExt->ThreadObject,
												NULL );

			if (!NT_SUCCESS(status)) {

				break;
			}

			timeOut.QuadPart = -LFS_TIME_OUT;		
			status = KeWaitForSingleObject( &lfsDeviceExt->ReadyEvent,
											Executive,
											KernelMode,
											FALSE,
											&timeOut );

			if (!NT_SUCCESS(status)) {

				break;
			}

			KeClearEvent( &lfsDeviceExt->ReadyEvent );
		
		} while (0);
	}

#endif
		
	return;
}


VOID
LfsCleanupMountedDevice (
    IN PDEVICE_OBJECT DeviceObject
    )
{
	PLFS_DEVICE_EXTENSION	lfsDeviceExt = &((PFILESPY_DEVICE_EXTENSION)(DeviceObject->DeviceExtension))->LfsDeviceExt;

#ifdef __LFS_FLUSH_VOLUME__

	NTSTATUS		Status;
	LARGE_INTEGER	timeOut;

	if (lfsDeviceExt->ThreadHandle != NULL) {
	
		ExAcquireFastMutex( &lfsDeviceExt->FastMutex );

		if (FlagOn(lfsDeviceExt->Thread.Flags, LFS_DEVICE_EXTENTION_THREAD_FLAG_START) &&
			!FlagOn(lfsDeviceExt->Thread.Flags, LFS_DEVICE_EXTENTION_THREAD_FLAG_STOPED)) {

			KeSetEvent( &lfsDeviceExt->RequestEvent, IO_DISK_INCREMENT, FALSE );
		} 

		ExReleaseFastMutex( &lfsDeviceExt->FastMutex );

		timeOut.QuadPart = -LFS_TIME_OUT;

		Status = KeWaitForSingleObject( lfsDeviceExt->ThreadObject,
										Executive,
										KernelMode,
										FALSE,
										&timeOut );

		if (Status == STATUS_SUCCESS) {
	   
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("FatDeleteVcb: thread stoped VolDo = %p\n", lfsDeviceExt) );

			ObDereferenceObject( lfsDeviceExt->ThreadObject );

		} else {

			ASSERT( FALSE );
		}

		lfsDeviceExt->ThreadHandle = NULL;
		lfsDeviceExt->ThreadObject = NULL;
		
	}

#endif

	if (FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_STOP))
		return;

	SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_STOP );

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsCleanupMountedDevice: Entered lfsDeviceExt = %p\n", lfsDeviceExt) );

	ASSERT( lfsDeviceExt->ReferenceCount );

	if (lfsDeviceExt->EnabledNetdisk) {

		NetdiskManager_DisMountVolume( GlobalLfs.NetdiskManager,
									   lfsDeviceExt->EnabledNetdisk,
									   lfsDeviceExt );

		lfsDeviceExt->EnabledNetdisk = NULL;
	}

	if (lfsDeviceExt->Filtering != TRUE) {

		return;
	}

	switch (lfsDeviceExt->FilteringMode) {

	case LFS_READ_ONLY:

		lfsDeviceExt->Vpb = NULL;
		break;

	case LFS_PRIMARY:
	case LFS_SECONDARY_TO_PRIMARY: {

		lfsDeviceExt->Vpb = NULL;
		break;
	}

	case LFS_SECONDARY: {

		if (lfsDeviceExt->Secondary)
			Secondary_Close( lfsDeviceExt->Secondary );

		lfsDeviceExt->Secondary = NULL;
		lfsDeviceExt->Vpb = NULL;

		break;
	}
	
	case LFS_FILE_CONTROL: {
	
		PFAST_IO_DISPATCH	fastIoDispatch;

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsDetachFromFileSystemDevice: Entered lfsDeviceExt = %p\n", lfsDeviceExt) );

#ifdef __NDFS__

		if (FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_REGISTERED)) {

			UnRegisterNdfsCallback( lfsDeviceExt->BaseVolumeDeviceObject );
			ClearFlag( lfsDeviceExt->Flags, LFS_DEVICE_REGISTERED );
		}

#endif

		FsRtlEnterFileSystem();
    
		fastIoDispatch = lfsDeviceExt->BaseVolumeDeviceObject->DriverObject->FastIoDispatch;

		// Prevent Direct I/O
		//lfsDeviceExt->OriginalAcquireForModWrite = fastIoDispatch->AcquireForModWrite;

		if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NTFS && fastIoDispatch->AcquireForModWrite && LfsOrginalNtfsAcquireFileForModWrite) {
		
			fastIoDispatch->AcquireForModWrite = LfsOrginalNtfsAcquireFileForModWrite;
			LfsOrginalNtfsAcquireFileForModWrite = NULL;
		
		} 

		break;
	}

	default:

		ASSERT( LFS_BUG );
		break;
	}
	
	return;
}


BOOLEAN
LfsPassThrough (
    IN  PDEVICE_OBJECT				DeviceObject,
    IN  PIRP						Irp,
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	OUT PNTSTATUS					NtStatus
	)
{
	BOOLEAN					result = FALSE;
	PLFS_DEVICE_EXTENSION	lfsDeviceExt = &DevExt->LfsDeviceExt;
    PIO_STACK_LOCATION		irpSp = IoGetCurrentIrpStackLocation( Irp );
	PFILE_OBJECT			fileObject = irpSp->FileObject;


	UNREFERENCED_PARAMETER( DeviceObject );

	LfsDeviceExt_Reference( lfsDeviceExt );	
	ASSERT( lfsDeviceExt->ReferenceCount );

#if DBG

	if (fileObject && fileObject->FileName.Length == 0)
		PrintIrp( LFS_DEBUG_LFS_TRACE, "LfsPassThrough", lfsDeviceExt, Irp );

#endif

	
#if DBG	 
#if 0 
	if ((irpSp->MajorFunction == IRP_MJ_SET_INFORMATION) && 
		(irpSp->Parameters.SetFile.FileInformationClass == FileDispositionInformation) &&
		fileObject && 
		fileObject->FileName.Length != 0) {
		
		CHAR NameBuf[256] = {0};
		int j;
		
		// Print of unicode is available only PASSIVE_LEVEL. 
		// So print in ASCII assuming file name is always ascii range.
		
		for (j=0;fileObject->FileName.Buffer[j]!=0 && j<sizeof(NameBuf)-1;j++) {
			NameBuf[j]= (CHAR)fileObject->FileName.Buffer[j];
		}

		NameBuf[j] = 0;
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
					   ("LfsPassthrough: DevObj=%p, LfsExt=%p, Enabled_Netdisk=%p, DeleteFile on close (FileName= %s)\n",
						 DeviceObject, lfsDeviceExt, lfsDeviceExt->EnabledNetdisk, NameBuf) );
	}
#else
	{ 
		// dump all IRP
		
		CHAR				NameBuf[256] = {0};
	    CHAR				irpMajorString[OPERATION_NAME_BUFFER_SIZE];
	    CHAR				irpMinorString[OPERATION_NAME_BUFFER_SIZE];


	    GetIrpName( irpSp->MajorFunction,
	                irpSp->MinorFunction,
	                irpSp->Parameters.FileSystemControl.FsControlCode,
	                irpMajorString,irpMinorString );
		
		if (fileObject && fileObject->FileName.Length != 0) {
			int j;

			// Print of unicode is available only PASSIVE_LEVEL. 
			// So print in ASCII assuming file name is always ascii range.
			
			for (j=0;fileObject->FileName.Buffer[j]!=0 && j<fileObject->FileName.Length-1;j++) {
				
				NameBuf[j]= (CHAR)fileObject->FileName.Buffer[j];
			}

			NameBuf[j] = 0;
		}

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsPassThrough: Thread=%p, %s %s File=%s, LfsExt=%p\n",
											  KeGetCurrentThread(), irpMajorString, irpMinorString, NameBuf, lfsDeviceExt) );

#if 0	// print directory query option

		if (irpSp->MajorFunction == IRP_MJ_DIRECTORY_CONTROL &&
			irpSp->MinorFunction == IRP_MN_QUERY_DIRECTORY) {

			if (irpSp->Flags & SL_INDEX_SPECIFIED) {
				
				SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,("Query SL_INDEX_SPECIFIED\n"));
			}

			if (irpSp->Flags & SL_RESTART_SCAN) {
				
				SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,("Query SL_RESTART_SCAN\n"));
			
			} else {
			
				SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,("Query SL_RESTART_SCAN is not set\n"));
			}

			if (irpSp->Flags & SL_RETURN_SINGLE_ENTRY) {
				SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,("Query SL_RETURN_SINGLE_ENTRY\n"));
			}
		}

#endif		
	}
#endif
#endif	

	ExAcquireFastMutex( &GlobalLfs.FastMutex );

	if (irpSp->MajorFunction == IRP_MJ_SHUTDOWN && GlobalLfs.ShutdownOccured == FALSE) {

		KIRQL			oldIrql;
		PLIST_ENTRY		lfsDeviceExtListEntry;
		LARGE_INTEGER	interval;

		
		GlobalLfs.ShutdownOccured = TRUE;
		ExReleaseFastMutex( &GlobalLfs.FastMutex );

#if DBG
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("file system type = %d\n", lfsDeviceExt->FileSystemType) );
		PrintIrp( LFS_DEBUG_LFS_TRACE, "ShutdownOccured", lfsDeviceExt, Irp );
#endif

		KeAcquireSpinLock( &GlobalLfs.LfsDeviceExtQSpinLock, &oldIrql );

		for (lfsDeviceExtListEntry = GlobalLfs.LfsDeviceExtQueue.Flink;
			 lfsDeviceExtListEntry != &GlobalLfs.LfsDeviceExtQueue;
			 lfsDeviceExtListEntry = lfsDeviceExtListEntry->Flink) {

			PLFS_DEVICE_EXTENSION volumeLfsDeviceExt;

			volumeLfsDeviceExt = CONTAINING_RECORD( lfsDeviceExtListEntry, LFS_DEVICE_EXTENSION, LfsQListEntry );
			SetFlag( volumeLfsDeviceExt->Flags, LFS_DEVICE_SHUTDOWN );

#ifdef __LFS_NDAS_NTFS__
			if (volumeLfsDeviceExt->FilteringMode == LFS_FILE_CONTROL			&& 
				volumeLfsDeviceExt->Filtering == TRUE							&& 
				(volumeLfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT ||
				 volumeLfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDNTFS)) {
#else
			if (volumeLfsDeviceExt->FilteringMode == LFS_FILE_CONTROL	&& 
				volumeLfsDeviceExt->Filtering == TRUE					&& 
				volumeLfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT) {
#endif
				NTSTATUS				deviceControlStatus;
				IO_STATUS_BLOCK			ioStatusBlock;
				ULONG					ioControlCode;
				ULONG					inputBufferLength;
				ULONG					outputBufferLength;


				ioControlCode		= IOCTL_SHUTDOWN; 
				inputBufferLength	= 0;
				outputBufferLength	= 0;

				RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

				KeReleaseSpinLock( &GlobalLfs.LfsDeviceExtQSpinLock, oldIrql );

				deviceControlStatus = LfsFilterDeviceIoControl( volumeLfsDeviceExt->BaseVolumeDeviceObject,
																ioControlCode,
																NULL,
																inputBufferLength,
																NULL,
																outputBufferLength,
																NULL );

				KeAcquireSpinLock(&GlobalLfs.LfsDeviceExtQSpinLock, &oldIrql);
			}
		}

		KeReleaseSpinLock( &GlobalLfs.LfsDeviceExtQSpinLock, oldIrql );

		MountManager_FileSystemShutdown( GlobalLfs.NetdiskManager );

	    interval.QuadPart = (2 * DELAY_ONE_SECOND);      //for primarySession to close files 
		KeDelayExecutionThread( KernelMode, FALSE, &interval );
		

		LfsDeviceExt_Dereference( lfsDeviceExt );	
		return FALSE;
	
	} else {

		ExReleaseFastMutex( &GlobalLfs.FastMutex );
	}

#if DBG

	if (irpSp->MajorFunction == IRP_MJ_PNP || irpSp->MajorFunction == IRP_MJ_SHUTDOWN) {

		PrintIrp( LFS_DEBUG_LFS_TRACE, NULL, lfsDeviceExt, Irp );
	
		if (irpSp->MajorFunction == IRP_MJ_PNP)
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsPassThrough lfsDeviceExt: %p IRP_MJ_PNP %x QueryDeviceRelations.Type = %d, lfsDeviceExt->SecondaryState = %d\n",
												 lfsDeviceExt, irpSp->MinorFunction, irpSp->Parameters.QueryDeviceRelations.Type, lfsDeviceExt->SecondaryState));
	}

#endif

	if (lfsDeviceExt->Filtering != TRUE) {

		LfsDeviceExt_Dereference( lfsDeviceExt );
		return FALSE;
	}


#if DBG
	PrintIrp( LFS_DEBUG_LFS_TRACE, "LfsPassThrough", lfsDeviceExt, Irp );
#endif

#if DBG
	if ((FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_START) && FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_STOP)) || 
		(IS_WINDOWS2K() && lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NTFS && 
		 FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_START) && BooleanFlagOn(lfsDeviceExt->Flags, LFS_DEVICE_STOPPING))) {

		PrintIrp( LFS_DEBUG_LFS_TRACE, FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_STOP) ? "LFS_DEVICE_STOP" : "LFS_DEVICE_STOP",
									  lfsDeviceExt, Irp );
	}
#endif

	if (IS_WINDOWS2K() &&
		lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NTFS &&
		FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_START) && FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_STOPPING)) {

		if (lfsDeviceExt->Secondary) {
		
			if (Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject) == NULL) {

				SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
							   ("LfsPassThrough: Win2K: BYPASSING. a request comes down during LFS_DEVICE_STOPPING. IRP :%x(%x)\n", irpSp->MajorFunction, irpSp->MinorFunction));

				LfsDeviceExt_Dereference( lfsDeviceExt );
				return FALSE;

			} else {
				
				SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
							   ("LfsPassThrough: Win2K: PROCESSING. a request comes down during LFS_DEVICE_STOPPING. IRP :%x(%x)\n", irpSp->MajorFunction, irpSp->MinorFunction));
			}

		} else {

			if (lfsDeviceExt->FilteringMode == LFS_SECONDARY) {
			
				SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
							   ("LfsPassThrough: Win2K: COMPLETING with access denied: a request comes down during LFS_DEVICE_STOPPING. IRP :%x(%x)\n", irpSp->MajorFunction, irpSp->MinorFunction));

				LfsDeviceExt_Dereference( lfsDeviceExt );
				*NtStatus = Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest( Irp, IO_DISK_INCREMENT );
				
				return TRUE;
			}
		}
	}

	if (!(FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_START) && !FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_STOP))) {

		if (!(IS_WINDOWS2K() && lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NTFS))
			ASSERT( LFS_UNEXPECTED );
		
		ASSERT( lfsDeviceExt->Secondary == NULL || Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject) == NULL );
		LfsDeviceExt_Dereference( lfsDeviceExt );
		return FALSE;
	}

#if DBG

	if (lfsDeviceExt->Vpb && lfsDeviceExt->Vpb->RealDevice && lfsDeviceExt->BaseVolumeDeviceObject) {

		SPY_LOG_PRINT( LFS_DEBUG_LFS_NOISE,
					   ("LfsPassThrough: Entered Dev:%p Ref:%d VpbFlag:%08lx VpbRef:%d RealDev:%p Ref:%d Flag:%08lx Irp = %p,"
						"Base:%p Ref:%d\n",
						 DeviceObject, DeviceObject->ReferenceCount, lfsDeviceExt->Vpb->Flags, lfsDeviceExt->Vpb->ReferenceCount,
						lfsDeviceExt->Vpb->RealDevice,
						lfsDeviceExt->Vpb->RealDevice->ReferenceCount,
						lfsDeviceExt->Vpb->RealDevice->Flags,
						Irp,
						lfsDeviceExt->BaseVolumeDeviceObject,
						lfsDeviceExt->BaseVolumeDeviceObject->ReferenceCount
						));
	
	} else {

		SPY_LOG_PRINT( LFS_DEBUG_LFS_NOISE,
					   ("LfsPassThrough: Entered Dev:%p Ref:%d, Irp = %p\n",
						 DeviceObject, DeviceObject->ReferenceCount, Irp) );
	}

	PrintIrp( LFS_DEBUG_LFS_TRACE, "LfsPassThrough", lfsDeviceExt, Irp );

#endif


	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );
	ASSERT( lfsDeviceExt->AttachedToDeviceObject == DevExt->AttachedToDeviceObject );
	
	switch (lfsDeviceExt->FilteringMode) {

	case LFS_READ_ONLY: {

		if (irpSp->MajorFunction == IRP_MJ_PNP && irpSp->MinorFunction == IRP_MN_SURPRISE_REMOVAL) {

			ASSERT( lfsDeviceExt->EnabledNetdisk );
			NetdiskManager_SurpriseRemoval( GlobalLfs.NetdiskManager,
										    lfsDeviceExt->EnabledNetdisk,
											lfsDeviceExt );

			lfsDeviceExt->EnabledNetdisk = NULL;			
			SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_SURPRISE_REMOVAL );
		}

#ifdef __LFS_NDAS_NTFS__
		if (lfsDeviceExt->FileSystemType != LFS_FILE_SYSTEM_NDFAT &&
			lfsDeviceExt->FileSystemType != LFS_FILE_SYSTEM_NDNTFS) {
#else
		if (lfsDeviceExt->FileSystemType != LFS_FILE_SYSTEM_NDFAT) {
#endif

			result = ReadOnlyPassThrough( DeviceObject, Irp, irpSp, lfsDeviceExt, NtStatus );
		}

		if (IS_WINDOWS2K() && 
			lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NTFS && result == FALSE && irpSp->MajorFunction == IRP_MJ_PNP) {

			UCHAR	minorFunction = irpSp->MinorFunction;

			SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_STOPPING );
			result = LfsPnpPassThrough( lfsDeviceExt, Irp, NtStatus );
			
			if (result == TRUE && *NtStatus == STATUS_SUCCESS && minorFunction == IRP_MN_QUERY_REMOVE_DEVICE) {

				ASSERT( !FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_STOP) );

				LfsCleanupMountedDevice( lfsDeviceExt->FileSpyDeviceObject );

				SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_STOP );
			}

			ClearFlag( lfsDeviceExt->Flags, LFS_DEVICE_STOPPING );
		}		
		
		break;
	}

	case LFS_PRIMARY: 
	case LFS_SECONDARY_TO_PRIMARY: {

#ifdef __LFS_FLUSH_VOLUME__

		//KeQuerySystemTime( &lfsDeviceExt->CommandReceiveTime );

		if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NTFS) {

			switch (irpSp->MajorFunction) {

			case IRP_MJ_CREATE: {

				ULONG	options;
				CHAR	disposition;

				options = irpSp->Parameters.Create.Options & 0xFFFFFF;
				disposition = (CHAR)(((irpSp->Parameters.Create.Options) >> 24) & 0xFF);

				if (options & FILE_DELETE_ON_CLOSE	||
					disposition == FILE_CREATE		||
					disposition == FILE_SUPERSEDE	||
					disposition == FILE_OVERWRITE	||
					disposition == FILE_OVERWRITE_IF) {

					lfsDeviceExt->ReceiveWriteCommand = TRUE;
					KeQuerySystemTime( &lfsDeviceExt->CommandReceiveTime );
					SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, 
								   ("lfsDeviceExt->CommandReceiveTime.QuadPart = %I64d\n", lfsDeviceExt->CommandReceiveTime.QuadPart) );

			
					break;
				}

				break;
			}

			case IRP_MJ_WRITE:
			case IRP_MJ_SET_INFORMATION:
			case IRP_MJ_SET_EA:
	        case IRP_MJ_SET_VOLUME_INFORMATION:
		    case IRP_MJ_SET_SECURITY:
			case IRP_MJ_SET_QUOTA:

				lfsDeviceExt->ReceiveWriteCommand = TRUE;
				KeQuerySystemTime( &lfsDeviceExt->CommandReceiveTime );
				SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, 
							   ("lfsDeviceExt->CommandReceiveTime.QuadPart = %I64d\n", lfsDeviceExt->CommandReceiveTime.QuadPart) );
				break;

			case IRP_MJ_FILE_SYSTEM_CONTROL:

				if (irpSp->Parameters.DeviceIoControl.IoControlCode == FSCTL_MOVE_FILE) {

					lfsDeviceExt->ReceiveWriteCommand = TRUE;
					KeQuerySystemTime( &lfsDeviceExt->CommandReceiveTime );
					SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, 
								   ("lfsDeviceExt->CommandReceiveTime.QuadPart = %I64d\n", lfsDeviceExt->CommandReceiveTime.QuadPart) );
				}

				break;

			default:

				break;
			}
		}

#endif

		if (irpSp->MajorFunction == IRP_MJ_PNP && irpSp->MinorFunction == IRP_MN_SURPRISE_REMOVAL) {

			ASSERT( lfsDeviceExt->EnabledNetdisk );

			NetdiskManager_SurpriseRemoval( GlobalLfs.NetdiskManager,
											lfsDeviceExt->EnabledNetdisk,
											lfsDeviceExt );

			lfsDeviceExt->EnabledNetdisk = NULL;
			
			SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_SURPRISE_REMOVAL );
		}

#ifdef __LFS_NDAS_NTFS__
		if (lfsDeviceExt->FileSystemType != LFS_FILE_SYSTEM_NDFAT &&
			lfsDeviceExt->FileSystemType != LFS_FILE_SYSTEM_NDNTFS) {
#else
		if (lfsDeviceExt->FileSystemType != LFS_FILE_SYSTEM_NDFAT) {
#endif

			result = Primary_PassThrough( lfsDeviceExt, Irp, NtStatus );
		}

		if (IS_WINDOWS2K() && 
			lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NTFS && result == FALSE && irpSp->MajorFunction == IRP_MJ_PNP) {

			UCHAR	minorFunction = irpSp->MinorFunction;
			
			SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_STOPPING );
			result = LfsPnpPassThrough( lfsDeviceExt, Irp, NtStatus );
			
			if (result == TRUE && *NtStatus == STATUS_SUCCESS && minorFunction == IRP_MN_QUERY_REMOVE_DEVICE) {

				ASSERT( !FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_STOP) );

				LfsCleanupMountedDevice( lfsDeviceExt->FileSpyDeviceObject );
			
				SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_STOP );
			}
			ClearFlag( lfsDeviceExt->Flags, LFS_DEVICE_STOPPING );
		}

		break;
	}

	case LFS_SECONDARY: {

#if DBG
		CountIrpMajorFunction( lfsDeviceExt, irpSp->MajorFunction );

		if (fileObject && 
			(irpSp->MajorFunction == IRP_MJ_CREATE || irpSp->MajorFunction == IRP_MJ_CLOSE || irpSp->MajorFunction == IRP_MJ_CLEANUP))
			PrintIrp( LFS_DEBUG_LFS_TRACE, NULL, lfsDeviceExt, Irp );
#endif

#ifdef __LFS_NDAS_NTFS__
		if (lfsDeviceExt->FileSystemType != LFS_FILE_SYSTEM_NDFAT &&
			lfsDeviceExt->FileSystemType != LFS_FILE_SYSTEM_NDNTFS) {
#else
		if (lfsDeviceExt->FileSystemType != LFS_FILE_SYSTEM_NDFAT) {
#endif

			ASSERT(lfsDeviceExt->Secondary);
		}

		if (fileObject && 
			fileObject->FileName.Length == (sizeof(REMOUNT_VOLUME_FILE_NAME)-sizeof(WCHAR)) && 
			RtlEqualMemory(fileObject->FileName.Buffer, REMOUNT_VOLUME_FILE_NAME, fileObject->FileName.Length)
			||
		    fileObject && 
			fileObject->FileName.Length == (sizeof(TOUCH_VOLUME_FILE_NAME)-sizeof(WCHAR)) && 
			RtlEqualMemory(fileObject->FileName.Buffer, TOUCH_VOLUME_FILE_NAME, fileObject->FileName.Length)) {

#if DBG
			PrintIrp( LFS_DEBUG_LFS_TRACE, "TOUCH/REMOUNT_VOLUME_FILE_NAME", lfsDeviceExt, Irp );
//			if(fileObject->FileName.Length == (sizeof(REMOUNT_VOLUME_FILE_NAME)-sizeof(WCHAR)))
//				ASSERT(LFS_UNEXPECTED);
#endif

			*NtStatus = Irp->IoStatus.Status = STATUS_OBJECT_NAME_NOT_FOUND;
			Irp->IoStatus.Information = 0;
			IoCompleteRequest(Irp, IO_DISK_INCREMENT);

			result = TRUE;
			break;
		}

		//
		//	Pass some PNP Irps through
		//

		if (irpSp->MajorFunction == IRP_MJ_PNP) {

			if (lfsDeviceExt->FileSystemType != LFS_FILE_SYSTEM_NDFAT && IS_WINDOWS2K()) {

				ASSERT( fileObject == NULL || 
					    fileObject->FileName.Length == 0 || 
						Secondary_LookUpFileExtension( lfsDeviceExt->Secondary, fileObject) == NULL );
#if DBG
				if (fileObject && fileObject->FileName.Length)
					PrintIrp( LFS_DEBUG_LFS_TRACE, "IRP_MJ_PNP", lfsDeviceExt, Irp );
#endif
			} else {

				//ASSERT(fileObject == NULL || fileObject->FileName.Length == 0); // fails some cases. Do not assert.
			}

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
						   ("LfsPassThrough lfsDeviceExt: %p fileObject = %p IRP_MJ_PNP MinorFunction = %d lfsDeviceExt->SecondaryState = %d\n",
							 lfsDeviceExt, fileObject, irpSp->MinorFunction, lfsDeviceExt->SecondaryState));

			if (irpSp->MinorFunction == IRP_MN_QUERY_DEVICE_RELATIONS		||
				irpSp->MinorFunction == IRP_MN_QUERY_INTERFACE				||
				irpSp->MinorFunction == IRP_MN_QUERY_CAPABILITIES			||
				irpSp->MinorFunction == IRP_MN_QUERY_RESOURCES				||
				irpSp->MinorFunction == IRP_MN_QUERY_RESOURCE_REQUIREMENTS	||
				irpSp->MinorFunction == IRP_MN_QUERY_DEVICE_TEXT			||
				irpSp->MinorFunction == IRP_MN_QUERY_PNP_DEVICE_STATE		||
				irpSp->MinorFunction == IRP_MN_QUERY_BUS_INFORMATION		||
				irpSp->MinorFunction == IRP_MN_DEVICE_USAGE_NOTIFICATION	||
				irpSp->MinorFunction == IRP_MN_QUERY_LEGACY_BUS_INFORMATION ||
				irpSp->MinorFunction == IRP_MN_QUERY_ID) {

				SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
							   ("LfsPassThrough lfsDeviceExt:IRP_MN_QUERY_DEVICE_RELATIONS:"
								"%p fileObject = %p IRP_MJ_PNP MinorFunction = %d lfsDeviceExt->SecondaryState = %d\n",
								lfsDeviceExt, fileObject, irpSp->MinorFunction, lfsDeviceExt->SecondaryState) );

				//
				//	hand over to the lower layer.
				//

				result = FALSE;
				break;
			}
		}

		//
		//	Check surprise flag
		//

		if (FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_SURPRISE_REMOVAL)) {

			*NtStatus = Irp->IoStatus.Status = STATUS_DEVICE_REMOVED;
			Irp->IoStatus.Information = 0;
			IoCompleteRequest(Irp, IO_DISK_INCREMENT);

			result = TRUE;
			break;
		}

		//
		//	Pass surprise removal through and set flag to the device ext
		//

		if (irpSp->MajorFunction == IRP_MJ_PNP && irpSp->MinorFunction == IRP_MN_SURPRISE_REMOVAL) {

			ASSERT( lfsDeviceExt->EnabledNetdisk );
			NetdiskManager_SurpriseRemoval(	GlobalLfs.NetdiskManager,
											lfsDeviceExt->EnabledNetdisk,
											lfsDeviceExt );

			lfsDeviceExt->EnabledNetdisk = NULL;

			SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_SURPRISE_REMOVAL );

			result = FALSE;
			break;
		}

#ifdef __LFS_NDAS_NTFS__
		if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT ||
			lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDNTFS) {
#else
		if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT) {
#endif

			result = FALSE;
			break;
		}

		if (!(lfsDeviceExt->FileSystemType  == LFS_FILE_SYSTEM_FAT && lfsDeviceExt->FilteringMode == CONNECT_TO_LOCAL_STATE)) {

			if (irpSp->MajorFunction == IRP_MJ_WRITE && Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject) == NULL) {

				struct Write *write =(struct Write *)&(irpSp->Parameters.Write);

				SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsPassThrough Fake Write %wZ\n", &fileObject->FileName) );

				*NtStatus = Irp->IoStatus.Status = STATUS_SUCCESS;
				Irp->IoStatus.Information = write->Length;
				IoCompleteRequest( Irp, IO_DISK_INCREMENT );

				result = TRUE;
				break;
			}
		}

		if (irpSp->MajorFunction == IRP_MJ_CREATE &&
			(fileObject && RtlEqualUnicodeString(&lfsDeviceExt->MountMgr, &fileObject->FileName, TRUE) || 
			fileObject && RtlEqualUnicodeString(&lfsDeviceExt->ExtendPlus, &fileObject->FileName, TRUE))) {

			ASSERT( irpSp->MajorFunction == IRP_MJ_CREATE );

			*NtStatus = Irp->IoStatus.Status = STATUS_OBJECT_NAME_NOT_FOUND;
			Irp->IoStatus.Information = 0;
			IoCompleteRequest(Irp, IO_DISK_INCREMENT);

			result = TRUE;
			break;
		}

		ExAcquireFastMutex( &lfsDeviceExt->FastMutex );
		
		//
		//
		//	Do not allow exclusive access to the volume in secondary mode.
		//	But, in purging process, allow.
		//

		if (irpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL && 
			(irpSp->MinorFunction == IRP_MN_USER_FS_REQUEST || irpSp->MinorFunction == IRP_MN_KERNEL_CALL)) {

			struct FileSystemControl	*fileSystemControl =(struct FileSystemControl *)&(irpSp->Parameters.FileSystemControl);

			fileObject = irpSp->FileObject;

			//
			//	Do not allow exclusive access to the volume
			//

			if (lfsDeviceExt->SecondaryState != VOLUME_PURGE_STATE && (fileSystemControl->FsControlCode == FSCTL_LOCK_VOLUME)) {
				
				ExReleaseFastMutex( &lfsDeviceExt->FastMutex );

				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
							   ("LfsPassThrough, IRP_MJ_FILE_SYSTEM_CONTROL: Return failure of acquiring the volume exclusively.\n") );

				ASSERT( fileObject && fileObject->FileName.Length == 0 && fileObject->RelatedFileObject == NULL );

				*NtStatus = Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest( Irp, IO_DISK_INCREMENT );

				result = TRUE;
				break;
			}
		}

		if (lfsDeviceExt->SecondaryState == WAIT_PURGE_SAFE_STATE) {

		    //
		    //	In purging
		    //

			if (Irp->RequestorMode == UserMode && fileObject && fileObject->FileName.Length) {

				if (irpSp->MajorFunction == IRP_MJ_CREATE) {

					lfsDeviceExt->SecondaryState = SECONDARY_STATE;
				
				} else {

					ASSERT( LFS_UNEXPECTED );
					result = FALSE;
					ExReleaseFastMutex( &lfsDeviceExt->FastMutex );
					goto PASS_THROUGH_OUT;
				}

				ExReleaseFastMutex( &lfsDeviceExt->FastMutex );
			
			} else {

				ExReleaseFastMutex( &lfsDeviceExt->FastMutex );

				if (irpSp->MajorFunction == IRP_MJ_CREATE) {
#if DBG
					PrintIrp( LFS_DEBUG_LFS_TRACE, "LfsPassThrough WAIT_PURGE_SAFE_STATE", lfsDeviceExt, Irp );
#endif
					if (fileObject->FileName.Length == (sizeof(WAKEUP_VOLUME_FILE_NAME)-sizeof(WCHAR)) && 
						RtlEqualMemory(fileObject->FileName.Buffer, WAKEUP_VOLUME_FILE_NAME, fileObject->FileName.Length)
					    || 
						fileObject->FileName.Length == (sizeof(TOUCH_VOLUME_FILE_NAME)-sizeof(WCHAR)) && 
						RtlEqualMemory(fileObject->FileName.Buffer, TOUCH_VOLUME_FILE_NAME, fileObject->FileName.Length)) {

						*NtStatus = Irp->IoStatus.Status = STATUS_OBJECT_NAME_NOT_FOUND;
						Irp->IoStatus.Information = 0;
						IoCompleteRequest( Irp, IO_DISK_INCREMENT );

						result = TRUE;
						break;	
					}
				}

				result = FALSE;
				goto PASS_THROUGH_OUT;
			}
		
		} else {

				ExReleaseFastMutex( &lfsDeviceExt->FastMutex );
		}

		//
		//	Take actions depending on the current secondary state
		//

#if DBG
		if (lfsDeviceExt->SecondaryState != CONNECT_TO_LOCAL_STATE && irpSp->MajorFunction == IRP_MJ_CREATE && Irp->RequestorMode == KernelMode)
			PrintIrp( LFS_DEBUG_LFS_TRACE, "LfsPassThrough KernelMode", lfsDeviceExt, Irp );
#endif

		if (lfsDeviceExt->SecondaryState == SECONDARY_STATE) {
		
			if (!(fileObject == NULL || irpSp->MajorFunction == IRP_MJ_CREATE || 
				  Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject) != NULL)) {

				result = FALSE;
			
			} else {

				result = Secondary_PassThrough( lfsDeviceExt->Secondary, Irp, NtStatus );
			}	
		
		} else if (lfsDeviceExt->SecondaryState == VOLUME_PURGE_STATE) {

#if DBG
			if (irpSp->MajorFunction == IRP_MJ_CREATE && fileObject->FileName.Length)
				PrintIrp( LFS_DEBUG_LFS_TRACE, "LfsPassThrough VOLUME_PURGE_STATE", lfsDeviceExt, Irp );
#endif
			if (irpSp->MajorFunction == IRP_MJ_PNP) {

				result = FALSE;
				break;
			
			} else if (!(fileObject == NULL || 
				         Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject) != NULL || 
						 (fileObject->RelatedFileObject && Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject->RelatedFileObject) != NULL))) {

				UNICODE_STRING		Mft;
				UNICODE_STRING		MftMirr;
				UNICODE_STRING		LogFile;
				UNICODE_STRING		Directory;
				UNICODE_STRING		BitMap;
	

				RtlInitUnicodeString( &Mft, L"\\$Mft" );
				RtlInitUnicodeString( &MftMirr, L"\\$MftMirr" );
				RtlInitUnicodeString( &LogFile, L"\\$LogFile" );
				RtlInitUnicodeString( &Directory, L"\\$Directory" );
				RtlInitUnicodeString( &BitMap, L"\\$BitMap" );

				/*if (irpSp->MajorFunction != IRP_MJ_CREATE) */ {
					result = FALSE;
					break;
				}
				
				if (fileObject->FileName.Length == (sizeof(WAKEUP_VOLUME_FILE_NAME)-sizeof(WCHAR)) && 
					RtlEqualMemory(fileObject->FileName.Buffer, WAKEUP_VOLUME_FILE_NAME, fileObject->FileName.Length)
				    || 
					fileObject->FileName.Length == (sizeof(TOUCH_VOLUME_FILE_NAME)-sizeof(WCHAR))&& 
					RtlEqualMemory(fileObject->FileName.Buffer, TOUCH_VOLUME_FILE_NAME, fileObject->FileName.Length)) {

					*NtStatus = Irp->IoStatus.Status = STATUS_OBJECT_NAME_NOT_FOUND;
					Irp->IoStatus.Information = 0;
					IoCompleteRequest( Irp, IO_DISK_INCREMENT );

					result = TRUE;
					break;	
				}
				
				if (Irp->RequestorMode != UserMode) {

					result = FALSE;
					break;
				}
		
				if (RtlEqualUnicodeString(&Mft, &fileObject->FileName, TRUE)	 || 
					RtlEqualUnicodeString(&MftMirr, &fileObject->FileName, TRUE) || 
					RtlEqualUnicodeString(&LogFile, &fileObject->FileName, TRUE) || 
					RtlEqualUnicodeString(&Directory, &fileObject->FileName, TRUE))
				{
#if DBG
					PrintIrp( LFS_DEBUG_LFS_TRACE, "LfsPassThrough VOLUME_PURGE_STATE metafile", lfsDeviceExt, Irp );
#endif
					result = FALSE;
					break;
				}
			}	
#if DBG
			PrintIrp( LFS_DEBUG_LFS_TRACE, "LfsPassThrough VOLUME_PURGE_STATE", lfsDeviceExt, Irp );
#endif
			ASSERT( fileObject == NULL || fileObject->DeviceObject == lfsDeviceExt->DiskDeviceObject );			
			result = Secondary_PassThrough( lfsDeviceExt->Secondary, Irp, NtStatus );
		
		} else if (lfsDeviceExt->SecondaryState == CONNECT_TO_LOCAL_STATE) {

#if DBG
			PrintIrp( LFS_DEBUG_LFS_TRACE, "LfsPassThrough CONNECT_TO_LOCAL_STATE", lfsDeviceExt, Irp );
#endif
				
			if (!(fileObject == NULL || 
				  Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject) != NULL || 
				  (fileObject->RelatedFileObject && Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject->RelatedFileObject) != NULL))) {

				if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NTFS) {

					if (irpSp->MajorFunction == IRP_MJ_CREATE)
						ASSERT( LFS_UNEXPECTED );
				}

				result = FALSE;
			
			} else {

				result = Secondary_PassThrough( lfsDeviceExt->Secondary, Irp, NtStatus );
				
				if(result == FALSE) {

					if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_FAT)
						result = Primary_PassThrough( lfsDeviceExt, Irp, NtStatus );
				}
			}
		
		} else {

			ASSERT( FALSE );
		}

PASS_THROUGH_OUT:

#if DBG
		if (result == FALSE) {

			ASSERT( Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject) == NULL );
			//ASSERT( fileObject == NULL || fileObject->RelatedFileObject == NULL || 
			//	    Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject->RelatedFileObject) == NULL );
		}
#endif
		if (IS_WINDOWS2K() && 
			lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NTFS && 
			result == FALSE && irpSp->MajorFunction == IRP_MJ_PNP) {

			UCHAR	minorFunction = irpSp->MinorFunction;

			SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_STOPPING );
			result = LfsPnpPassThrough( lfsDeviceExt, Irp, NtStatus );
			
			if (result == TRUE && *NtStatus == STATUS_SUCCESS && minorFunction == IRP_MN_QUERY_REMOVE_DEVICE) {

				ASSERT( !FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_STOP) );

				LfsCleanupMountedDevice( lfsDeviceExt->FileSpyDeviceObject );

				SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_STOP );
			}

			ClearFlag( lfsDeviceExt->Flags, LFS_DEVICE_STOPPING );
		}
				
		break;
	}

	case LFS_FILE_CONTROL: {
		
#if DBG
		PrintIrp( LFS_DEBUG_LFS_TRACE, "LFS_FILE_CONTROL", lfsDeviceExt, Irp );
#endif

		result = FALSE;
		break;
	}	

	default:

		ASSERT( LFS_BUG );		
		result = FALSE;

		break;
	}
	
	LfsDeviceExt_Dereference( lfsDeviceExt );

	return result;
}


NTSTATUS
LfsPnpPassThroughCompletion (
	IN PDEVICE_OBJECT	DeviceObject,
	IN PIRP				Irp,
	IN PKEVENT			WaitEvent
)
{
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO, ("LfsPnpPassThroughCompletion: called\n") );

	UNREFERENCED_PARAMETER( DeviceObject );
	UNREFERENCED_PARAMETER( Irp );

	KeSetEvent( WaitEvent, IO_NO_INCREMENT, FALSE );

	return STATUS_MORE_PROCESSING_REQUIRED;
}


BOOLEAN
LfsPnpPassThrough(
	IN  PLFS_DEVICE_EXTENSION	LfsDeviceExt,
	IN  PIRP					Irp,
	OUT PNTSTATUS				NtStatus
	)
{
    PIO_STACK_LOCATION	irpSp = IoGetCurrentIrpStackLocation(Irp);
    KEVENT				waitEvent;
	NTSTATUS			status;


	if (LfsDeviceExt->AttachedToDeviceObject == NULL) {

		ASSERT( LFS_BUG );
		return FALSE;
	}

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
				   ("LfsPnpPassThrough: LfsDeviceExt = %p, irpSp->MajorFunction = %x, irpSp->MinorFunction = %x\n", 
					 LfsDeviceExt, irpSp->MajorFunction, irpSp->MinorFunction) );

	if (irpSp->MinorFunction != IRP_MN_QUERY_REMOVE_DEVICE)
		return FALSE;


	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
				   ("Open files from secondary: LfsDeviceExt = %p, irpSp->MajorFunction = %x, irpSp->MinorFunction = %x\n", 
					 LfsDeviceExt, irpSp->MajorFunction, irpSp->MinorFunction) );


	IoCopyCurrentIrpStackLocationToNext( Irp );
	KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

	IoSetCompletionRoutine( Irp,
							LfsPnpPassThroughCompletion,
							&waitEvent,
							TRUE,
							TRUE,
							TRUE );

	status = IoCallDriver( LfsDeviceExt->AttachedToDeviceObject, Irp );

	if (status == STATUS_PENDING) {

		status = KeWaitForSingleObject( &waitEvent,
										Executive,
										KernelMode,
										FALSE,
										NULL );

		ASSERT( status == STATUS_SUCCESS );
	}

	ASSERT( KeReadStateEvent(&waitEvent) || !NT_SUCCESS(Irp->IoStatus.Status) );
	
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO, ("LfsPnpPassThrough: Irp->IoStatus.Status = %x\n", Irp->IoStatus.Status) );

	*NtStatus = Irp->IoStatus.Status;
	IoCompleteRequest( Irp, IO_NO_INCREMENT );

	return TRUE;
}


#if DBG

VOID
CountIrpMajorFunction(
	PLFS_DEVICE_EXTENSION	LfsDeviceExt,
	UCHAR					IrpMajorFunction
	)
{
	if (++LfsDeviceExt->IrpMajorFunctionCount[IrpMajorFunction] % 10 == 0 && 
		IrpMajorFunction != IRP_MJ_CREATE && 
		IrpMajorFunction != IRP_MJ_CLOSE && 
		IrpMajorFunction != IRP_MJ_CLEANUP && 
		IrpMajorFunction != IRP_MJ_DIRECTORY_CONTROL) {

		ULONG	i;

		for (i=0; i<IRP_MJ_MAXIMUM_FUNCTION; i++)
				SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("%d ", LfsDeviceExt->IrpMajorFunctionCount[i]) );
		SPY_LOG_PRINT(LFS_DEBUG_LFS_TRACE, ("\n") );		
	}

	return;
}

#endif

PNON_PAGED_FCB
LfsAllocateNonPagedFcb (
	VOID
    )
{
#ifdef __LFSDBG_MEMORY__
    return (PNON_PAGED_FCB) ExAllocatePoolWithTag(NonPagedPool, sizeof(NON_PAGED_FCB), LFS_FCB_NONPAGED_TAG);
#else
    return (PNON_PAGED_FCB) ExAllocateFromNPagedLookasideList(&GlobalLfs.NonPagedFcbLookasideList);
#endif
}


VOID
LfsFreeNonPagedFcb (
    IN  PNON_PAGED_FCB NonPagedFcb
    )
{
 	if(NonPagedFcb == NULL) {

		ASSERT( LFS_BUG );
		return;
	}

#ifdef __LFSDBG_MEMORY__
	ExFreePoolWithTag( NonPagedFcb, LFS_FCB_NONPAGED_TAG );
#else
	ExFreeToNPagedLookasideList( &GlobalLfs.NonPagedFcbLookasideList, (PVOID) NonPagedFcb );
#endif
}


PERESOURCE
LfsAllocateResource (
	VOID
    )
{
    PERESOURCE Resource;

#ifdef __LFSDBG_MEMORY__
    Resource = (PERESOURCE) ExAllocatePoolWithTag(NonPagedPool, sizeof(ERESOURCE), LFS_ERESOURCE_TAG);
#else
    Resource = (PERESOURCE) ExAllocateFromNPagedLookasideList(&GlobalLfs.EResourceLookasideList);
#endif

    if(Resource)
		ExInitializeResourceLite(Resource);
	else
		ASSERT(LFS_UNEXPECTED);

    return Resource;
}

VOID
LfsFreeResource (
    IN PERESOURCE Resource
    )
{
	if(Resource == NULL)
	{
		ASSERT(LFS_BUG);
		return;
	}

    ExDeleteResourceLite(Resource);

#ifdef __LFSDBG_MEMORY__
	ExFreePoolWithTag(Resource, LFS_ERESOURCE_TAG);
#else
    ExFreeToNPagedLookasideList(&GlobalLfs.EResourceLookasideList, (PVOID) Resource);
#endif
}


/*************************************************************************************************/

/* Only LfsDeviceExt_SecondaryToPrimary() can call SecondaryToPrimaryThreadProc() and PurgeVolume() */

VOID
SecondaryToPrimaryThreadProc(
	IN PLFS_DEVICE_EXTENSION LfsDeviceExt
	);

NTSTATUS
PurgeVolume(
	IN PLFS_DEVICE_EXTENSION LfsDeviceExt
	);

/*************************************************************************************************/


BOOLEAN
LfsDeviceExt_SecondaryToPrimary(
	IN PLFS_DEVICE_EXTENSION LfsDeviceExt
	)
{
	OBJECT_ATTRIBUTES	objectAttributes;
	NTSTATUS			waitStatus;
	LARGE_INTEGER		timeOut;

	HANDLE				threadHandle;
	PVOID				threadObject;


	LfsDeviceExt_Reference(LfsDeviceExt);

	if (LfsDeviceExt->SecondaryState == CONNECT_TO_LOCAL_STATE) {

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
					   ("LfsDeviceExt_SecondaryToPrimary: LfsDeviceExt:%p already connected to the local\n", LfsDeviceExt));
		LfsDeviceExt_Dereference( LfsDeviceExt );
		
		return TRUE;
	}

	InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );
	
	waitStatus = PsCreateSystemThread( &threadHandle,
						               THREAD_ALL_ACCESS,
						               &objectAttributes,
						               NULL,
						               NULL,
						               SecondaryToPrimaryThreadProc,
						               LfsDeviceExt );

	if (!NT_SUCCESS(waitStatus)) {

		ASSERT(LFS_UNEXPECTED);
		LfsDeviceExt_Dereference( LfsDeviceExt );
		return FALSE;
	}

	waitStatus = ObReferenceObjectByHandle( threadHandle,
											FILE_READ_DATA,
											NULL,
											KernelMode,
											&threadObject,
											NULL );

	if (!NT_SUCCESS(waitStatus)) {

		ASSERT( LFS_INSUFFICIENT_RESOURCES );
		LfsDeviceExt_Dereference( LfsDeviceExt );
		return FALSE;
	}

	timeOut.QuadPart = -10*HZ;		// 60 sec
//	timeOut.QuadPart = - (LFS_CHANGECONNECTION_TIMEOUT);
	waitStatus = KeWaitForSingleObject(	threadObject,
										Executive,
										KernelMode,
										FALSE,
										&timeOut );

	if (waitStatus == STATUS_TIMEOUT) {

		if (GlobalLfs.ShutdownOccured != TRUE) {

			LARGE_INTEGER	currentTime;

			KeQuerySystemTime( &currentTime );
	
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
						   ("*************************************\nLfsDeviceExt_SecondaryToPrimary: LfsDeviceExt = %p Second KeWaitForSingleObject = currentTime = %I64d\n", 
							LfsDeviceExt, currentTime.QuadPart) );
	
			timeOut.QuadPart = -(LFS_TIME_OUT*2);		// 10 sec
			waitStatus = KeWaitForSingleObject( threadObject,
												Executive,
												KernelMode,
												FALSE,
												&timeOut );

			KeQuerySystemTime( &currentTime );

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
						   ("LfsDeviceExt_SecondaryToPrimary: Exit LfsDeviceExt = %p Second KeWaitForSingleObject = currentTime = %I64d waitStatus = %d\n", 
							 LfsDeviceExt, currentTime.QuadPart, waitStatus) );
		}

		if (waitStatus != STATUS_SUCCESS) {

			ASSERT( LFS_REQUIRED );	// occured sometimes. need to handle..
			LfsDeviceExt_Dereference( LfsDeviceExt );
			return FALSE;
		}
	
	} else if (waitStatus != STATUS_SUCCESS) {

		ASSERT( LFS_REQUIRED );
		
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
					   ("LfsDeviceExt_SecondaryToPrimary: NetdiskManager_UnplugNetdisk LfsDeviceExt = %p, waitStatus = %x, LfsDeviceExt->SecondaryState = %d\n",
					     LfsDeviceExt, waitStatus, LfsDeviceExt->SecondaryState) );
	}

	ObDereferenceObject( threadObject );

	//ExReleaseFastMutex(&lfs->Secondary2PrimaryMutex);
	
	LfsDeviceExt_Dereference( LfsDeviceExt );

	return ( LfsDeviceExt->SecondaryState == CONNECT_TO_LOCAL_STATE );
}


VOID
SecondaryToPrimaryThreadProc(
	IN PLFS_DEVICE_EXTENSION LfsDeviceExt
	)
{
	BOOLEAN		upgradeResult;		
	NTSTATUS	purgeStatus;


	ASSERT( LfsDeviceExt->SecondaryState == SECONDARY_STATE );
	ASSERT( LfsDeviceExt->PurgeVolumeSafe == TRUE );

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("SecondaryToPrimaryThreadProc: LfsDeviceExt = %p, LfsDeviceExt->SecondaryState = %d\n",
										 LfsDeviceExt, LfsDeviceExt->SecondaryState) );

	upgradeResult = NetdiskManager_Secondary2Primary( GlobalLfs.NetdiskManager, 
													  LfsDeviceExt->EnabledNetdisk, 
													  LfsDeviceExt );

	if (upgradeResult != TRUE) {
	
		//ASSERT(LFS_BUG);
		PsTerminateSystemThread( STATUS_SUCCESS );
		return;
	}

	SPY_LOG_PRINT(LFS_DEBUG_LFS_TRACE, ("SecondaryToPrimaryThreadProc: LfsDeviceExt->SecondaryState = %d\n",
										LfsDeviceExt->SecondaryState) );

	LfsDeviceExt->SecondaryState = VOLUME_PURGE_STATE;

	purgeStatus = PurgeVolume( LfsDeviceExt );		

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("SecondaryToPrimaryThreadProc: purgeStatus = %x, LfsDeviceExt->SecondaryState = %d\n",
										 purgeStatus, LfsDeviceExt->SecondaryState) );

	if(purgeStatus == STATUS_SUCCESS) {

		LfsDeviceExt->SecondaryState = CONNECT_TO_LOCAL_STATE;
	
	} else {

		LfsDeviceExt->SecondaryState = SECONDARY_STATE;
	}

	PsTerminateSystemThread( STATUS_SUCCESS );

	return;
}


NTSTATUS
PurgeVolume(
	IN PLFS_DEVICE_EXTENSION LfsDeviceExt
	)
{
	HANDLE					eventHandle;

	HANDLE					fileHandle = NULL;
    ACCESS_MASK				desiredAccess;
	ULONG					attributes;
	OBJECT_ATTRIBUTES		objectAttributes;
	IO_STATUS_BLOCK			ioStatusBlock;
	LARGE_INTEGER			allocationSize;
	ULONG					fileAttributes;
    ULONG					shareAccess;
    ULONG					createDisposition;
	ULONG					createOptions;
    PVOID					eaBuffer;
	ULONG					eaLength;

	NTSTATUS				createStatus;
	NTSTATUS				fsControlStatus;

	
	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
				   ("PurgeVolume: LfsDeviceExt = %p, LfsDeviceExt->FileSystemType = %d LfsDeviceExt->Vpb->ReferenceCount = %d\n", 
					 LfsDeviceExt, LfsDeviceExt->FileSystemType, LfsDeviceExt->Vpb->ReferenceCount) );

	desiredAccess = SYNCHRONIZE | READ_CONTROL | FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES;
	desiredAccess |= FILE_WRITE_EA | FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_READ_EA;

	ASSERT( desiredAccess == 0x0012019F );

	attributes  = OBJ_KERNEL_HANDLE;
	attributes |= OBJ_CASE_INSENSITIVE;

	InitializeObjectAttributes( &objectAttributes,
								&LfsDeviceExt->NetdiskPartitionInformation.VolumeName,
								attributes,
								NULL,
								NULL );
		
	allocationSize.LowPart  = 0;
	allocationSize.HighPart = 0;

	fileAttributes	  = 0;		
	shareAccess		  = FILE_SHARE_READ | FILE_SHARE_WRITE;
	createDisposition = FILE_OPEN;
	createOptions     = FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE;
	eaBuffer		  = NULL;
	eaLength		  = 0;

	RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

	createStatus = ZwCreateFile( &fileHandle,
								 desiredAccess,
								 &objectAttributes,
								 &ioStatusBlock,
								 &allocationSize,
								 fileAttributes,
								 shareAccess,
								 createDisposition,
								 createOptions,
								 eaBuffer,
								 eaLength );

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
				   ("PurgeVolume: LfsDeviceExt = %p ZwCreateFile fileHandle =%p, createStatus = %X, ioStatusBlock = %X\n",
					 LfsDeviceExt, fileHandle, createStatus, ioStatusBlock.Information) );

	if (!(createStatus == STATUS_SUCCESS)) {
	
		return STATUS_UNSUCCESSFUL;
	
	}else {

		ASSERT( ioStatusBlock.Information == FILE_OPENED );
	}

	do {
	
		createStatus = ZwCreateEvent( &eventHandle,
									  GENERIC_READ,
									  NULL,
									  SynchronizationEvent,
									  FALSE );

		if (createStatus != STATUS_SUCCESS) {

			ASSERT( LFS_UNEXPECTED );
			ZwClose( fileHandle );
			return STATUS_UNSUCCESSFUL;
		}
		
		RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );
		
		fsControlStatus = ZwFsControlFile( fileHandle,
										   eventHandle,
										   NULL,
										   NULL,
										   &ioStatusBlock,
										   FSCTL_LOCK_VOLUME,
										   NULL,
										   0,
										   NULL,
										   0 );

		if (fsControlStatus == STATUS_PENDING) {

			//ASSERT(LFS_UNEXPECTED);
			fsControlStatus = ZwWaitForSingleObject(eventHandle, TRUE, NULL);
		}

		if (fsControlStatus != STATUS_SUCCESS)
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
							("PurgeVolume: LfsDeviceExt = %p ZwFsControlFile FSCTL_LOCK_VOLUME fileHandle =%p, fsControlStatus = %X, ioStatusBlock = %X\n",
							LfsDeviceExt, fileHandle, fsControlStatus, ioStatusBlock.Information) );
		
		if (!NT_SUCCESS(fsControlStatus))
			break;

		ZwClose( eventHandle );

		createStatus = ZwCreateEvent( &eventHandle, GENERIC_READ, NULL, SynchronizationEvent, FALSE );

		if (createStatus != STATUS_SUCCESS) {
		
			ASSERT( LFS_UNEXPECTED );
			ZwClose( fileHandle );
			return STATUS_UNSUCCESSFUL;
		}
		
		fsControlStatus = ZwFsControlFile( fileHandle,
										   eventHandle, //NULL, //LfsDeviceExt->EventHandle,
										   NULL,
										   NULL,
										   &ioStatusBlock,
										   FSCTL_UNLOCK_VOLUME,
										   NULL,
										   0,
										   NULL,
										   0 );

		if (fsControlStatus == STATUS_PENDING) {

			//ASSERT(LFS_UNEXPECTED);		
			fsControlStatus = ZwWaitForSingleObject( eventHandle, TRUE, NULL );
		}

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
					   ("PurgeVolume: LfsDeviceExt = %p NtUnlockFile fileHandle =%p, fsControlStatus = %X, ioStatusBlock = %X\n",
					     LfsDeviceExt, fileHandle, fsControlStatus, ioStatusBlock.Information) );
	
	} while(0);

	ZwClose( eventHandle );
	ZwClose( fileHandle );

	if (fsControlStatus != STATUS_SUCCESS)
		return fsControlStatus;

	do {

		UNICODE_STRING			fileName;
		PWCHAR					fileNameBuffer;

		fileNameBuffer = ExAllocatePool(PagedPool,NDFS_MAX_PATH);

		if(GlobalLfs.ShutdownOccured == TRUE)
			break;

		RtlInitEmptyUnicodeString( &fileName,
								   fileNameBuffer,
								   NDFS_MAX_PATH );
        
		RtlCopyUnicodeString( &fileName, &LfsDeviceExt->NetdiskPartitionInformation.VolumeName );

		ioStatusBlock.Information = 0;

		createStatus = RtlAppendUnicodeToString( &fileName, REMOUNT_VOLUME_FILE_NAME );

		if (createStatus != STATUS_SUCCESS) {

			ExFreePool( fileNameBuffer );
			ASSERT( LFS_UNEXPECTED );
			break;
		}

		desiredAccess = SYNCHRONIZE | READ_CONTROL | FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES | FILE_WRITE_EA;
		desiredAccess |= FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_READ_EA;

		ASSERT( desiredAccess == 0x0012019F );

		attributes  = OBJ_KERNEL_HANDLE;
		attributes |= OBJ_CASE_INSENSITIVE;

		InitializeObjectAttributes( &objectAttributes,
									&fileName,
									attributes,
									NULL,
									NULL );
		
		allocationSize.LowPart  = 0;
		allocationSize.HighPart = 0;

		fileAttributes	  = 0;		
		shareAccess		  = FILE_SHARE_READ | FILE_SHARE_WRITE;
		createDisposition = FILE_OPEN;
		createOptions     = FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE;
		eaBuffer		  = NULL;
		eaLength		  = 0;
		
		createStatus = ZwCreateFile( &fileHandle,
									 desiredAccess,
									 &objectAttributes,
									 &ioStatusBlock,
									 NULL,
									 fileAttributes,
									 shareAccess,
									 createDisposition,
									 createOptions,
									 NULL,
									 0 );
		
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
					   ("PurgeVolume:	New Volume Create ? %p %wZ, createStatus = %x, ioStatusBlock.Information = %d\n",
			             LfsDeviceExt, &LfsDeviceExt->NetdiskPartitionInformation.VolumeName, createStatus, ioStatusBlock.Information) );

		if (createStatus == STATUS_SUCCESS) {

			ASSERT( ioStatusBlock.Information == FILE_OPENED );
			ZwClose( fileHandle );
		}

		ExFreePool( fileNameBuffer );

	} while(0);

	return fsControlStatus;
}


BOOLEAN
Lfs_IsLocalAddress(
	PLPX_ADDRESS	LpxAddress
	)
{
	KIRQL		oldIrql;
	LONG		idx_enabled;

	KeAcquireSpinLock(&GlobalLfs.Primary->SpinLock, &oldIrql);
	
	for (idx_enabled = 0; idx_enabled < MAX_SOCKETLPX_INTERFACE; idx_enabled ++) {

		if (GlobalLfs.Primary->Agent.ListenSocket[idx_enabled].Active &&
			RtlEqualMemory( LpxAddress->Node,
							GlobalLfs.Primary->Agent.ListenSocket[idx_enabled].NICAddress.Node,
							ETHER_ADDR_LENGTH)) {

			KeReleaseSpinLock( &GlobalLfs.Primary->SpinLock, oldIrql );
			return TRUE;
		}
	}

	KeReleaseSpinLock( &GlobalLfs.Primary->SpinLock, oldIrql );
	
	return FALSE;
}


NTSTATUS
RegisterNdfsCallback(
	PDEVICE_OBJECT	DeviceObject
	)
{
		NTSTATUS		deviceControlStatus;
		IO_STATUS_BLOCK	ioStatusBlock;
		ULONG			ioControlCode;
		PVOID			inputBuffer;
		ULONG			inputBufferLength;
		PVOID			outputBuffer;
		ULONG			outputBufferLength;
 		
		
		ioControlCode		= IOCTL_REGISTER_NDFS_CALLBACK; 
		inputBuffer			= &GlobalLfs.NdfsCallback;
		inputBufferLength	= sizeof(GlobalLfs.NdfsCallback);
		outputBuffer		= NULL;
		outputBufferLength	= 0;

		RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

		deviceControlStatus = LfsFilterDeviceIoControl( DeviceObject,
														ioControlCode,
														inputBuffer,
														inputBufferLength,
														outputBuffer,
														outputBufferLength,
														NULL );

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
					   ("RegisterLfsCallback: deviceControlStatus = %X\n", deviceControlStatus) );
		
		return deviceControlStatus;
}	


NTSTATUS
UnRegisterNdfsCallback(
	PDEVICE_OBJECT	DeviceObject
	)
{
		NTSTATUS		deviceControlStatus;
		IO_STATUS_BLOCK	ioStatusBlock;
		ULONG			ioControlCode;
		PVOID			inputBuffer;
		ULONG			inputBufferLength;
		PVOID			outputBuffer;
		ULONG			outputBufferLength;
 		
		
		ioControlCode		= IOCTL_UNREGISTER_NDFS_CALLBACK; 
		inputBuffer			= NULL;
		inputBufferLength	= 0;
		outputBuffer		= NULL;
		outputBufferLength	= 0;


		RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

		deviceControlStatus = LfsFilterDeviceIoControl( DeviceObject,
														ioControlCode,
														inputBuffer,
														inputBufferLength,
														outputBuffer,
														outputBufferLength,
														NULL );

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
					   ("UnRegisterNdfsCallback: deviceControlStatus = %X\n", deviceControlStatus) );
		
		return deviceControlStatus;
}	


NTSTATUS
CallbackQueryPartitionInformation(
	IN  PDEVICE_OBJECT					RealDevice,
	OUT PNETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation
	)
{
	
	return NetdiskManager_QueryPartitionInformation( GlobalLfs.NetdiskManager,
													 RealDevice,
													 NetdiskPartitionInformation );
}


NTSTATUS
CallbackQueryPrimaryAddress( 
	IN  PNETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation,
	OUT PLPX_ADDRESS					PrimaryAddress,
	IN  PBOOLEAN						IsLocalAddress	
	)
{
	NTSTATUS				status;
	NETDISK_PARTITION_INFO	netdiskPartitionInfo;
	
	RtlCopyMemory( &netdiskPartitionInfo.NetDiskAddress,
				   &NetdiskPartitionInformation->NetdiskInformation.NetDiskAddress,
				   sizeof(netdiskPartitionInfo.NetDiskAddress) );

	netdiskPartitionInfo.UnitDiskNo = NetdiskPartitionInformation->NetdiskInformation.UnitDiskNo;
	netdiskPartitionInfo.StartingOffset.QuadPart = 0;
	
	status = LfsTable_QueryPrimaryAddress( GlobalLfs.LfsTable,
										   &netdiskPartitionInfo,
										   PrimaryAddress );
	
	if (status == STATUS_SUCCESS && IsLocalAddress) 
		*IsLocalAddress = Lfs_IsLocalAddress( PrimaryAddress );
    
	return status;
}


BOOLEAN
CallbackSecondaryToPrimary(
	IN  PDEVICE_OBJECT			RealDevice
	)
{
	PLFS_DEVICE_EXTENSION	lfsDeviceExt;
	BOOLEAN					result;

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("CallbackSecondaryToPrimary: Entered RealDevice = %p\n", RealDevice) );

	lfsDeviceExt = NetdiskManager_QueryLfsDevExt( GlobalLfs.NetdiskManager, RealDevice );

	if (lfsDeviceExt == NULL) {

		ASSERT( FALSE );
		return FALSE;
	}

	result = NetdiskManager_Secondary2Primary( GlobalLfs.NetdiskManager, 
											   lfsDeviceExt->EnabledNetdisk, 
											   lfsDeviceExt );

	if (result == TRUE) {

		lfsDeviceExt->FilteringMode = LFS_SECONDARY_TO_PRIMARY;
		
		NetdiskManager_ChangeMode( GlobalLfs.NetdiskManager,
								   lfsDeviceExt->EnabledNetdisk,
								   lfsDeviceExt );
	}

	return result;
}


BOOLEAN
CallbackAddWriteRange(
	IN  PDEVICE_OBJECT	RealDevice,
	OUT PBLOCKACE_ID	BlockAceId
	)
{
	PLFS_DEVICE_EXTENSION	lfsDeviceExt;
	BOOLEAN					result;

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("CallbackAddWriteRange: Entered RealDevice = %p\n", RealDevice) );

	lfsDeviceExt = NetdiskManager_QueryLfsDevExt( GlobalLfs.NetdiskManager, RealDevice );

	if (lfsDeviceExt == NULL) {

		ASSERT( FALSE );
		return FALSE;
	}

	result = NetdiskManager_AddWriteRange( GlobalLfs.NetdiskManager, 
										   lfsDeviceExt,
										   BlockAceId );

	return result;
}



VOID
CallbackRemoveWriteRange(
	IN  PDEVICE_OBJECT	RealDevice,
	OUT BLOCKACE_ID		BlockAceId
	)
{
	PLFS_DEVICE_EXTENSION	lfsDeviceExt;


	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("CallbackAddWriteRange: Entered RealDevice = %p\n", RealDevice) );

	lfsDeviceExt = NetdiskManager_QueryLfsDevExt( GlobalLfs.NetdiskManager, RealDevice );

	if (lfsDeviceExt == NULL) {

		ASSERT( FALSE );
		return;
	}

	NetdiskManager_RemoveWriteRange( GlobalLfs.NetdiskManager, 
								     lfsDeviceExt,
									 BlockAceId );

	return;
}


NTSTATUS
CallbackGetNdasScsiBacl(
	IN	PDEVICE_OBJECT	DiskDeviceObject,
	OUT PNDAS_BLOCK_ACL	NdasBacl,
	IN BOOLEAN			SystemOrUser
	) 
{
	NTSTATUS	queryStatus;

	queryStatus = GetNdasScsiBacl( DiskDeviceObject, NdasBacl, SystemOrUser );
		
	ASSERT( queryStatus == STATUS_SUCCESS || queryStatus == STATUS_BUFFER_OVERFLOW );

	return queryStatus;
}


#ifdef __LFS_FLUSH_VOLUME__

VOID
LfsDeviceExtThreadProc(
	IN	PLFS_DEVICE_EXTENSION	LfsDeviceExt
	)
{
	BOOLEAN		volDoThreadTerminate = FALSE;


	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsDeviceExtThreadProc: Start LfsDeviceExt = %p\n", LfsDeviceExt) );
	
	LfsDeviceExt_Reference( LfsDeviceExt );
	
	LfsDeviceExt->Thread.Flags = LFS_DEVICE_EXTENTION_THREAD_FLAG_INITIALIZING;

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsDeviceExtThreadProc: Start LfsDeviceExt = %p\n", LfsDeviceExt) );

	ExAcquireFastMutex( &LfsDeviceExt->FastMutex );		
	ClearFlag( LfsDeviceExt->Thread.Flags, LFS_DEVICE_EXTENTION_THREAD_FLAG_INITIALIZING );
	SetFlag( LfsDeviceExt->Thread.Flags, LFS_DEVICE_EXTENTION_THREAD_FLAG_START );
	ExReleaseFastMutex( &LfsDeviceExt->FastMutex );
			
	KeSetEvent( &LfsDeviceExt->ReadyEvent, IO_DISK_INCREMENT, FALSE );

	volDoThreadTerminate = FALSE;
	
	while (volDoThreadTerminate == FALSE) {

		PKEVENT			events[2];
		LONG			eventCount;
		NTSTATUS		eventStatus;
		LARGE_INTEGER	timeOut;
		

		ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

		eventCount = 0;
		events[eventCount++] = &LfsDeviceExt->RequestEvent;

		timeOut.QuadPart = -LFS_DEVICE_EXTENTION_THREAD_FLAG_TIME_OUT;

		eventStatus = KeWaitForMultipleObjects(	eventCount,
												events,
												WaitAny,
												Executive,
												KernelMode,
												TRUE,
												&timeOut,
												NULL );


		if (eventStatus == STATUS_TIMEOUT) {

			LARGE_INTEGER	currentTime;

			if (GlobalLfs.ShutdownOccured || !(FlagOn(LfsDeviceExt->Flags, LFS_DEVICE_START) && !FlagOn(LfsDeviceExt->Flags, LFS_DEVICE_STOP)))
				continue;

			KeQuerySystemTime( &currentTime );

			if (LfsDeviceExt->ReceiveWriteCommand == TRUE && 
				(LfsDeviceExt->TryFlushOrPurgeTime.QuadPart > currentTime.QuadPart || 
				(currentTime.QuadPart - LfsDeviceExt->TryFlushOrPurgeTime.QuadPart) >= LFS_DEVICE_EXTENTION_TRY_FLUSH_OR_PURGE_DURATION)) {

				if ((currentTime.QuadPart - LfsDeviceExt->CommandReceiveTime.QuadPart) <=  LFS_DEVICE_EXTENTION_TRY_FLUSH_OR_PURGE_DURATION /*&& 
					(currentTime.QuadPart - LfsDeviceExt->TryFlushOrPurgeTime.QuadPart) <= (100*LFS_DEVICE_EXTENTION_TRY_FLUSH_OR_PURGE_DURATION)*/) {

					continue;
				}

				do {
				
					HANDLE					eventHandle = NULL;

					HANDLE					fileHandle = NULL;
					ACCESS_MASK				desiredAccess;
					ULONG					attributes;
					OBJECT_ATTRIBUTES		objectAttributes;
					IO_STATUS_BLOCK			ioStatusBlock;
					LARGE_INTEGER			allocationSize;
					ULONG					fileAttributes;
					ULONG					shareAccess;
				    ULONG					createDisposition;
					ULONG					createOptions;
				    PVOID					eaBuffer;
					ULONG					eaLength;

					NTSTATUS				createStatus;
					NTSTATUS				fileSystemControlStatus;


					//PIRP					topLevelIrp;
					//PRIMARY_REQUEST_INFO	primaryRequestInfo;


					ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

					if (LfsDeviceExt->FilteringMode == LFS_SECONDARY)
						break;

					SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsDeviceExtThreadProc: Flush Volume LfsDeviceExt = %p, LfsDeviceExt->NetdiskPartitionInformation.VolumeName = %wZ\n", 
											LfsDeviceExt, &LfsDeviceExt->NetdiskPartitionInformation.VolumeName) );

					desiredAccess = SYNCHRONIZE | READ_CONTROL | FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES | FILE_WRITE_EA 
									| FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_READ_EA;

					ASSERT( desiredAccess == 0x0012019F );

					attributes  = OBJ_KERNEL_HANDLE;
					attributes |= OBJ_CASE_INSENSITIVE;

					InitializeObjectAttributes( &objectAttributes,
											    &LfsDeviceExt->NetdiskPartitionInformation.VolumeName,
												attributes,
												NULL,
												NULL );
		
					allocationSize.LowPart  = 0;
					allocationSize.HighPart = 0;

					fileAttributes	  = 0;		
					shareAccess		  = FILE_SHARE_READ | FILE_SHARE_WRITE;
					createDisposition = FILE_OPEN;
					createOptions     = FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE;
					eaBuffer		  = NULL;
					eaLength		  = 0;

					RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

					createStatus = ZwCreateFile( &fileHandle,
												 desiredAccess,
												 &objectAttributes,
												 &ioStatusBlock,
												 &allocationSize,
												 fileAttributes,
												 shareAccess,
												 createDisposition,
												 createOptions,
												 eaBuffer,
												 eaLength );

					SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsDeviceExtThreadProc: LfsDeviceExt = %p, createStatus = %x, ioStatusBlock = %x\n", LfsDeviceExt, createStatus, ioStatusBlock.Information) );


					if (createStatus != STATUS_SUCCESS) 
						break;
						
					ASSERT( ioStatusBlock.Information == FILE_OPENED);
	
					createStatus = ZwCreateEvent( &eventHandle,
												  GENERIC_READ,
												  NULL,
												  SynchronizationEvent,
												  FALSE );

					if (createStatus != STATUS_SUCCESS) {

						ASSERT( FALSE );
						ZwClose( fileHandle );
						break;
					}


					//primaryRequestInfo.PrimaryTag				= 0xe2027482;
					//primaryRequestInfo.PrimarySession			= NULL;
					//primaryRequestInfo.NdfsWinxpRequestHeader	= NULL;

					//topLevelIrp = IoGetTopLevelIrp();
					//ASSERT( topLevelIrp == NULL );
					//IoSetTopLevelIrp( (PIRP)&primaryRequestInfo );

					RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

					fileSystemControlStatus = ZwDeviceIoControlFile( fileHandle,
																	 &eventHandle,
																	 NULL,
																	 NULL,
																	 &ioStatusBlock,
																	 IOCTL_VOLSNAP_FLUSH_AND_HOLD_WRITES,
																	 NULL,
																	 0,
																	 NULL,
																	 0 );

					if (fileSystemControlStatus == STATUS_PENDING) {

						LARGE_INTEGER			timeOut;
			
						timeOut.QuadPart = -HZ;
						//ASSERT(LFS_UNEXPECTED);
						fileSystemControlStatus = ZwWaitForSingleObject( eventHandle, TRUE, &timeOut );
					}

#define IOCTL_VOLSNAP_RELEASE_WRITES CTL_CODE(VOLSNAPCONTROLTYPE, 1, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

					fileSystemControlStatus = ZwDeviceIoControlFile( fileHandle,
																	 &eventHandle,
																	 NULL,
																	 NULL,
																	 &ioStatusBlock,
																	 IOCTL_VOLSNAP_RELEASE_WRITES,
																	 NULL,
																	 0,
																	 NULL,
																	 0 );

					if (fileSystemControlStatus == STATUS_PENDING) {

						LARGE_INTEGER			timeOut;
			
						timeOut.QuadPart = -3*HZ;
						//ASSERT(LFS_UNEXPECTED);
						fileSystemControlStatus = ZwWaitForSingleObject( eventHandle, TRUE, &timeOut );
					}

					//IoSetTopLevelIrp( topLevelIrp );

					if (fileSystemControlStatus != STATUS_SUCCESS)
						SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsDeviceExtThreadProc: LfsDeviceExt = %p, fileSystemControlStatus = %x, ioStatusBlock = %x\n", 
												LfsDeviceExt, fileSystemControlStatus, ioStatusBlock.Information) );

					ZwClose( eventHandle );
					ZwClose( fileHandle );
					
					SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsDeviceExtThreadProc: Flush Volume End LfsDeviceExt = %p, LfsDeviceExt->NetdiskPartitionInformation.VolumeName = %wZ\n", 
											LfsDeviceExt, &LfsDeviceExt->NetdiskPartitionInformation.VolumeName) );
					break;
				
				} while (0);
			}

			KeQuerySystemTime( &LfsDeviceExt->TryFlushOrPurgeTime );
			LfsDeviceExt->ReceiveWriteCommand = FALSE;

			continue;
		}
		
		ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
		ASSERT( eventCount < THREAD_WAIT_OBJECTS );
		
		if (!NT_SUCCESS( eventStatus ) || eventStatus >= eventCount) {

			ASSERT( FALSE );
			SetFlag( LfsDeviceExt->Thread.Flags, LFS_DEVICE_EXTENTION_THREAD_FLAG_ERROR );
			volDoThreadTerminate = TRUE;
			continue;
		}
		
		KeClearEvent( events[eventStatus] );

		if (eventStatus == 0) {

			volDoThreadTerminate = TRUE;
			break;
		
		} else
			ASSERT( FALSE );
	}

	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	ExAcquireFastMutex( &LfsDeviceExt->FastMutex );

	SetFlag( LfsDeviceExt->Thread.Flags, LFS_DEVICE_EXTENTION_THREAD_FLAG_STOPED );

	ExReleaseFastMutex( &LfsDeviceExt->FastMutex );

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsDeviceExtThreadProc: PsTerminateSystemThread LfsDeviceExt = %p\n", LfsDeviceExt) );
	
	ExAcquireFastMutex( &LfsDeviceExt->FastMutex );
	SetFlag( LfsDeviceExt->Thread.Flags, LFS_DEVICE_EXTENTION_THREAD_FLAG_TERMINATED );
	ExReleaseFastMutex( &LfsDeviceExt->FastMutex );
	
	LfsDeviceExt_Dereference( LfsDeviceExt );

	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	
	PsTerminateSystemThread( STATUS_SUCCESS );
}

#endif


VOID
PrintIrp(
	ULONG					DebugLevel,
	PCHAR					Where,
	PLFS_DEVICE_EXTENSION	LfsDeviceExt,
	PIRP					Irp
	)
{
#if DBG
	
	PIO_STACK_LOCATION  irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT		fileObject = irpSp->FileObject;
	UNICODE_STRING		nullName;
	UCHAR				minorFunction;
    CHAR				irpMajorString[OPERATION_NAME_BUFFER_SIZE];
    CHAR				irpMinorString[OPERATION_NAME_BUFFER_SIZE];

	GetIrpName (
		irpSp->MajorFunction,
		irpSp->MinorFunction,
		irpSp->Parameters.FileSystemControl.FsControlCode,
		irpMajorString,
		irpMinorString
		);

	RtlInitUnicodeString(&nullName, L"fileObject == NULL");
	
	if(irpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL && irpSp->MinorFunction == IRP_MN_USER_FS_REQUEST) 
		minorFunction = (UCHAR)((irpSp->Parameters.FileSystemControl.FsControlCode & 0x00003FFC) >> 2);
	else if(irpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL && irpSp->MinorFunction == 0)
		minorFunction = (UCHAR)((irpSp->Parameters.DeviceIoControl.IoControlCode & 0x00003FFC) >> 2);
	else
		minorFunction = irpSp->MinorFunction;

	ASSERT(Irp->RequestorMode == KernelMode || Irp->RequestorMode == UserMode);

	SPY_LOG_PRINT(DebugLevel,
		("%s Lfs: %p Irql:%d Irp:%p %s %s (%u:%u) %08x %02x ",
			(Where) ? Where : "",
			LfsDeviceExt, KeGetCurrentIrql(), 
			Irp, irpMajorString, irpMinorString, irpSp->MajorFunction, minorFunction,
			Irp->Flags, irpSp->Flags));
			/*"%s %c%c%c%c%c ", */
			/*(Irp->RequestorMode == KernelMode) ? "KernelMode" : "UserMode",
	        (Irp->Flags & IRP_PAGING_IO) ? '*' : ' ',
			(Irp->Flags & IRP_SYNCHRONOUS_PAGING_IO) ? '+' : ' ',
			(Irp->Flags & IRP_SYNCHRONOUS_API) ? 'A' : ' ',
			BooleanFlagOn(Irp->Flags,IRP_NOCACHE) ? 'N' : ' ',
			(fileObject && fileObject->Flags & FO_SYNCHRONOUS_IO) ? '&':' ',*/
	SPY_LOG_PRINT(DebugLevel,
		("file: %p  %08x %p %wZ %d\n",
			fileObject, 
			fileObject ? fileObject->Flags : 0,
			fileObject ? fileObject->RelatedFileObject : NULL,
			fileObject ? &fileObject->FileName : &nullName,
			fileObject ? fileObject->FileName.Length : 0
			));

#else
	
	UNREFERENCED_PARAMETER(DebugLevel);
	UNREFERENCED_PARAMETER(Where);
	UNREFERENCED_PARAMETER(LfsDeviceExt);
	UNREFERENCED_PARAMETER(Irp);

#endif

	return;
}

