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


NTSTATUS
CallbackSecondaryToPrimary (
	IN  PDEVICE_OBJECT	RealDevice
	);


NTSTATUS
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

NTSTATUS
LfsPnpPassThroughCompletion (
	IN PDEVICE_OBJECT	DeviceObject,
	IN PIRP				Irp,
	IN PKEVENT			WaitEvent
	);

NTSTATUS
LfsPassThroughCompletion (
	IN PDEVICE_OBJECT	DeviceObject,
	IN PIRP				Irp,
	IN PKEVENT			WaitEvent
	);


#if __LFS_FLUSH_VOLUME__

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
	PVOID	DGSvrCtx = NULL;
	PVOID	NtcCtx = NULL;

#if DBG
	
	ASSERT( sizeof(UCHAR)	== 1 );
	ASSERT( sizeof(USHORT)	== 2 );
	ASSERT( sizeof(ULONG)	== 4 );
	ASSERT( sizeof(ULONGLONG)== 8 );

	DbgPrint( "KdDebuggerEnabled = %p *KdDebuggerEnabled = %d\n", KdDebuggerEnabled, *KdDebuggerEnabled );
	
	gFileSpyDebugLevel = 0;
	gFileSpyDebugLevel |= LFS_DEBUG_LFS_TRACE;
	gFileSpyDebugLevel |= LFS_DEBUG_LFS_TRACE;
	gFileSpyDebugLevel |= LFS_DEBUG_NETDISK_MANAGER_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_NETDISK_MANAGER_TRACE;
	gFileSpyDebugLevel |= LFS_DEBUG_PRIMARY_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_PRIMARY_INFO;
	gFileSpyDebugLevel |= LFS_DEBUG_SECONDARY_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_SECONDARY_TRACE;
	gFileSpyDebugLevel |= LFS_DEBUG_SECONDARY_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_TABLE_INFO;
	gFileSpyDebugLevel |= LFS_DEBUG_LIB_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_LIB_INFO;

	gFileSpyDebugLevel |= SPYDEBUG_DISPLAY_ATTACHMENT_NAMES;

	gFileSpyDebugLevel = SPYDEBUG_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_LFS_TRACE;
	//gFileSpyDebugLevel |= LFS_DEBUG_NETDISK_MANAGER_TRACE;
	gFileSpyDebugLevel |= LFS_DEBUG_LFS_TRACE;
	gFileSpyDebugLevel |= LFS_DEBUG_PRIMARY_INFO;
	gFileSpyDebugLevel |= LFS_DEBUG_LFS_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_SECONDARY_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_PRIMARY_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_SECONDARY_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_SECONDARY_TRACE;

	gFileSpyDebugLevel = SPYDEBUG_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_LFS_INFO;
	gFileSpyDebugLevel |= LFS_DEBUG_PRIMARY_INFO;
	gFileSpyDebugLevel |= SPYDEBUG_DISPLAY_ATTACHMENT_NAMES;
	gFileSpyDebugLevel |= LFS_DEBUG_NETDISK_MANAGER_TRACE;
	gFileSpyDebugLevel |= LFS_DEBUG_SECONDARY_INFO;
	gFileSpyDebugLevel |= LFS_DEBUG_NETDISK_MANAGER_ERROR;

	gFileSpyDebugLevel = SPYDEBUG_ERROR;
	gFileSpyDebugLevel |= SPYDEBUG_DISPLAY_ATTACHMENT_NAMES;
	gFileSpyDebugLevel |= LFS_DEBUG_SECONDARY_INFO;

	gFileSpyDebugLevel |= LFS_DEBUG_LFS_INFO;
	gFileSpyDebugLevel |= LFS_DEBUG_LFS_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_NETDISK_MANAGER_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_PRIMARY_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_SECONDARY_ERROR;

	gFileSpyDebugLevel |= LFS_DEBUG_NETDISK_MANAGER_TRACE;
	gFileSpyDebugLevel |= LFS_DEBUG_NETDISK_MANAGER_INFO;
	//gFileSpyDebugLevel |= LFS_DEBUG_READONLY_TRACE;
	gFileSpyDebugLevel |= LFS_DEBUG_READONLY_INFO;
	gFileSpyDebugLevel |= LFS_DEBUG_READONLY_ERROR;


	DbgPrint( "Lfs DriverEntry %s %s\n", __DATE__, __TIME__ );
	
	DbgPrint( "sizeof(NDFS_WINXP_REQUEST_HEADER) = %d, sizeof(NDFS_WINXP_REPLY_HEADER) = %d\n",
			   sizeof(NDFS_WINXP_REQUEST_HEADER), sizeof(NDFS_WINXP_REPLY_HEADER) );

	ASSERT( DEFAULT_MAX_DATA_SIZE % 8 == 0 );
	ASSERT( sizeof(NDFS_WINXP_REQUEST_HEADER) % 8 == 0 );
	ASSERT( sizeof(NDFS_WINXP_REPLY_HEADER) % 8 == 0 );

	DbgPrint( "gSpyOsMajorVersion = %x, gSpyOsMinorVersion = %x\n", gSpyOsMajorVersion, gSpyOsMinorVersion );

#endif

	RtlZeroMemory( lfs, sizeof(LFS) );

    ExInitializeFastMutex( &lfs->FastMutex );
	lfs->ReferenceCount			= 1;

	lfs->DriverObject			= DriverObject;
	lfs->RegistryPath			= RegistryPath;
	lfs->ControlDeviceObject	= ControlDeviceObject;

	InitializeListHead( &lfs->LfsDeviceExtQueue );
	KeInitializeSpinLock( &lfs->LfsDeviceExtQSpinLock );

#if __NDAS_NTFS_x32_RW__

	if (IS_WINDOWSXP())
		lfs->NdasNtfsRwSupport	= 1;
	else
		lfs->NdasNtfsRwSupport	= 0;	

#endif

#if __NDAS_NTFS_x64_RW__

	if (IS_WINDOWSXP_OR_LATER())
		lfs->NdasNtfsRwSupport	= 1;
	else
		lfs->NdasNtfsRwSupport	= 0;	

#endif

#if __NDAS_NTFS_x32_RO__

	if (IS_WINDOWSXP())
		lfs->NdasNtfsRoSupport	= 1;
	else
		lfs->NdasNtfsRoSupport	= 0;	

#endif

#if __NDAS_NTFS_x64_RO__

	if (IS_WINDOWSXP_OR_LATER())
		lfs->NdasNtfsRoSupport	= 1;
	else
		lfs->NdasNtfsRoSupport	= 0;	

#endif

#if __NDAS_FAT_RW__
	lfs->NdasFatRwSupport			= 1;
#endif

#if __NDAS_FAT_RO__
	lfs->NdasFatRoSupport			= 1;
#endif

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

	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("LfsDriverUnload: called\n") );

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

	    ExDeleteNPagedLookasideList( &Lfs->EResourceLookasideList );
		ExDeleteNPagedLookasideList( &Lfs->NonPagedFcbLookasideList );

		ASSERT( Lfs->LfsDeviceExtQueue.Flink == Lfs->LfsDeviceExtQueue.Blink );

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
				("---------------------LfsDereference: Lfs is Freed-----------------------\n") );
	}
}


NTSTATUS
LfsInitializeLfsDeviceExt(
    IN PDEVICE_OBJECT	FileSpyDeviceObject,
    IN PDEVICE_OBJECT	DiskDeviceObject,
	IN PDEVICE_OBJECT	MountVolumeDeviceObject
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

	lfsDeviceExt->Flags = LFS_DEVICE_INITIALIZING;

	lfsDeviceExt->FileSpyDeviceObject	= FileSpyDeviceObject;
	lfsDeviceExt->FileSpyDevExt			= devExt;

	lfsDeviceExt->Purge					= FALSE;
	lfsDeviceExt->HookFastIo			= FALSE;		//Allow FastIo to pass through
	lfsDeviceExt->Filtering				= FALSE;
	lfsDeviceExt->FilteringMode			= LFS_NO_FILTERING;

	lfsDeviceExt->DiskDeviceObject			= DiskDeviceObject;
	lfsDeviceExt->MountVolumeDeviceObject	= MountVolumeDeviceObject;

	ExInterlockedInsertTailList( &GlobalLfs.LfsDeviceExtQueue,
								 &lfsDeviceExt->LfsQListEntry,
								 &GlobalLfs.LfsDeviceExtQSpinLock );

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
		ASSERT( LfsDeviceExt->Secondary == NULL );
		
		KeAcquireSpinLock( &GlobalLfs.LfsDeviceExtQSpinLock, &oldIrql );
		RemoveEntryList( &LfsDeviceExt->LfsQListEntry );
		KeReleaseSpinLock( &GlobalLfs.LfsDeviceExtQSpinLock, oldIrql );
		
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

			} else if (lfsDeviceExt->FilteringMode == LFS_READONLY) {

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

		RtlInitUnicodeString( &fsName, NDAS_FAT_DEVICE_NAME );

		result = RtlEqualUnicodeString( &lfsDeviceExt->FileSystemVolumeName,
										&fsName,
										TRUE );

		if (result == 1) {

			NTSTATUS	status;

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsAttachToFileSystemDevice: Name compare %d %wZ %wZ\n",
											     result, &lfsDeviceExt->FileSystemVolumeName, &fsName) );

			lfsDeviceExt->FileSystemType = LFS_FILE_SYSTEM_NDAS_FAT;

			status = RegisterNdfsCallback( DeviceObject );

			if (status == STATUS_SUCCESS) {

				SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_REGISTERED );
				lfsDeviceExt->Filtering = TRUE;
			
			} else {

				lfsDeviceExt->Filtering = FALSE;
			}

			break;
		} 

		RtlInitUnicodeString( &fsName, NDAS_NTFS_DEVICE_NAME );

		result = RtlEqualUnicodeString( &lfsDeviceExt->FileSystemVolumeName,
										&fsName,
										TRUE );

		if (result == 1) {

			NTSTATUS	status;

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsAttachToFileSystemDevice: Name compare %d %wZ %wZ\n",
												 result, &lfsDeviceExt->FileSystemVolumeName, &fsName) );

			lfsDeviceExt->FileSystemType = LFS_FILE_SYSTEM_NDAS_NTFS;

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

	if (FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_REGISTERED)) {

		UnRegisterNdfsCallback( lfsDeviceExt->BaseVolumeDeviceObject );
		ClearFlag( lfsDeviceExt->Flags, LFS_DEVICE_REGISTERED );
	}

	ASSERT( lfsDeviceExt->ReferenceCount != 0 );
	SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_STOP );
		
	return;
}


VOID
LfsCleanupMountedDevice (
    IN PDEVICE_OBJECT DeviceObject
    )
{
	PLFS_DEVICE_EXTENSION	lfsDeviceExt = &((PFILESPY_DEVICE_EXTENSION)(DeviceObject->DeviceExtension))->LfsDeviceExt;

#if __LFS_FLUSH_VOLUME__

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

	if (FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_STOPPING))
		return;

	SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_STOP );

	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("LfsCleanupMountedDevice: Entered lfsDeviceExt = %p\n", lfsDeviceExt) );

	ASSERT( lfsDeviceExt->ReferenceCount );

	if (lfsDeviceExt->NetdiskPartition) {

		NetdiskManager_DisMountVolume( GlobalLfs.NetdiskManager,
									   lfsDeviceExt->NetdiskPartition,
									   lfsDeviceExt->NetdiskEnabledMode );

		lfsDeviceExt->NetdiskPartition = NULL;
	}

	if (lfsDeviceExt->Filtering != TRUE) {

		return;
	}

	switch (lfsDeviceExt->FilteringMode) {

	case LFS_READONLY:

		if (lfsDeviceExt->Readonly)
			Readonly_Close( lfsDeviceExt->Readonly );

		lfsDeviceExt->Readonly = NULL;
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

		if (FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_REGISTERED)) {

			UnRegisterNdfsCallback( lfsDeviceExt->BaseVolumeDeviceObject );
			ClearFlag( lfsDeviceExt->Flags, LFS_DEVICE_REGISTERED );
		}

		FsRtlEnterFileSystem();
    
		fastIoDispatch = lfsDeviceExt->BaseVolumeDeviceObject->DriverObject->FastIoDispatch;

		// Prevent Direct I/O
		//lfsDeviceExt->OriginalAcquireForModWrite = fastIoDispatch->AcquireForModWrite;

		if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NTFS && fastIoDispatch->AcquireForModWrite && LfsOrginalNtfsAcquireFileForModWrite) {
		
			fastIoDispatch->AcquireForModWrite = LfsOrginalNtfsAcquireFileForModWrite;
			LfsOrginalNtfsAcquireFileForModWrite = NULL;	
		} 

		FsRtlExitFileSystem();

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
		
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
					   ("LfsPassthrough: DevObj=%p, LfsExt=%p, Enabled_Netdisk=%p, DeleteFile on close (FileName= %wZ)\n",
						 DeviceObject, lfsDeviceExt, lfsDeviceExt->EnabledNetdisk, &fileObject->FileName) );
	}
#else
	{ 
		// dump all IRP
		
	    CHAR				irpMajorString[OPERATION_NAME_BUFFER_SIZE];
	    CHAR				irpMinorString[OPERATION_NAME_BUFFER_SIZE];


	    GetIrpName( irpSp->MajorFunction,
	                irpSp->MinorFunction,
	                irpSp->Parameters.FileSystemControl.FsControlCode,
	                irpMajorString,irpMinorString );
		
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsPassThrough: Thread=%p, %s %s File=%wZ, LfsExt=%p\n",
											  KeGetCurrentThread(), irpMajorString, irpMinorString, &fileObject->FileName, lfsDeviceExt) );

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

			if (volumeLfsDeviceExt->FilteringMode == LFS_FILE_CONTROL			&& 
				volumeLfsDeviceExt->Filtering == TRUE							&& 
				(volumeLfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDAS_FAT ||
				 volumeLfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDAS_NTFS)) {

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

		NetdiskManager_FileSystemShutdown( GlobalLfs.NetdiskManager );
		Primary_FileSystemShutdown( GlobalLfs.Primary );

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
	PrintIrp( LFS_DEBUG_LFS_TRACE, __FUNCTION__, lfsDeviceExt, Irp );
#endif

#if DBG
	if ((FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_START) && FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_STOP)) || 
		(IS_WINDOWS2K() && lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NTFS && 
		 FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_START) && BooleanFlagOn(lfsDeviceExt->Flags, LFS_DEVICE_STOPPING))) {

		PrintIrp( LFS_DEBUG_LFS_TRACE, FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_STOP) ? "LFS_DEVICE_STOP" : "LFS_DEVICE_STOP",
									  lfsDeviceExt, Irp );
	}
#endif


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

	case LFS_READONLY: {

		if (!(FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_START) && !FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_STOP))) {

			ASSERT( lfsDeviceExt->Secondary == NULL || Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject) == NULL );
			LfsDeviceExt_Dereference( lfsDeviceExt );
			return FALSE;
		}

		result = ReadOnlyPassThrough( DeviceObject, Irp, irpSp, lfsDeviceExt, NtStatus );

		if (result == TRUE) {

			break;
		}

		if (!(FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_START) && !FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_STOP))) {

			if (lfsDeviceExt->Secondary != NULL && 
				Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject) != NULL) {
				
				LfsDbgBreakPoint();

				*NtStatus = Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest( Irp, IO_DISK_INCREMENT );

				LfsDeviceExt_Dereference( lfsDeviceExt );
				return TRUE;
			}

			LfsDeviceExt_Dereference( lfsDeviceExt );
			return FALSE;
		}

		if (irpSp->MajorFunction == IRP_MJ_PNP) {

			//PrintIrp( LFS_DEBUG_LFS_INFO, __FUNCTION__, lfsDeviceExt, Irp );

			if (irpSp->MinorFunction == IRP_MN_SURPRISE_REMOVAL) {

				if (lfsDeviceExt->NetdiskPartition == NULL) {

					LfsDbgBreakPoint();
				
				} else {

					NetdiskManager_PrimarySessionStopping( GlobalLfs.NetdiskManager,
														   lfsDeviceExt->NetdiskPartition,
														   lfsDeviceExt->NetdiskEnabledMode	);

					NetdiskManager_PrimarySessionDisconnect( GlobalLfs.NetdiskManager,
														     lfsDeviceExt->NetdiskPartition,
														     lfsDeviceExt->NetdiskEnabledMode );

					NetdiskManager_SurpriseRemoval( GlobalLfs.NetdiskManager,
													lfsDeviceExt->NetdiskPartition,
													lfsDeviceExt->NetdiskEnabledMode );
				}

				lfsDeviceExt->NetdiskPartition = NULL;			
				SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_SURPRISE_REMOVAL );

				result = FALSE;
				break;

			}

			// Need to test much whether this is okay..
	
			if (irpSp->MinorFunction == IRP_MN_CANCEL_REMOVE_DEVICE || irpSp->MinorFunction == IRP_MN_REMOVE_DEVICE) {

				result = FALSE;
				break;
			
			} else if (irpSp->MinorFunction == IRP_MN_QUERY_REMOVE_DEVICE) {

				KEVENT				waitEvent;


				if (lfsDeviceExt->AttachedToDeviceObject == NULL) {

					LfsDbgBreakPoint();

					result = FALSE;
					break;
				}

				SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_STOPPING );

				NetdiskManager_PrimarySessionStopping( GlobalLfs.NetdiskManager,
													   lfsDeviceExt->NetdiskPartition,
													   lfsDeviceExt->NetdiskEnabledMode	);

				IoCopyCurrentIrpStackLocationToNext( Irp );
				KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

				IoSetCompletionRoutine( Irp,
										LfsPassThroughCompletion,
										&waitEvent,
										TRUE,
										TRUE,
										TRUE );

				*NtStatus = IoCallDriver( lfsDeviceExt->AttachedToDeviceObject, Irp );

				if (*NtStatus == STATUS_PENDING) {

					*NtStatus = KeWaitForSingleObject( &waitEvent, Executive, KernelMode, FALSE, NULL );
					ASSERT( *NtStatus == STATUS_SUCCESS );
				}

				ASSERT( KeReadStateEvent(&waitEvent) || !NT_SUCCESS(Irp->IoStatus.Status) );
	
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO, 
							   ("%s: lfsDeviceExt = %p, IRP_MN_QUERY_REMOVE_DEVICE Irp->IoStatus.Status = %x\n",
								__FUNCTION__, lfsDeviceExt, Irp->IoStatus.Status) );

				if (NT_SUCCESS(Irp->IoStatus.Status)) {

					NetdiskManager_PrimarySessionDisconnect( GlobalLfs.NetdiskManager,
															 lfsDeviceExt->NetdiskPartition,
															 lfsDeviceExt->NetdiskEnabledMode );

				} else {
					
					NetdiskManager_PrimarySessionCancelStopping( GlobalLfs.NetdiskManager,
																 lfsDeviceExt->NetdiskPartition,
																 lfsDeviceExt->NetdiskEnabledMode );
				}

				ClearFlag( lfsDeviceExt->Flags, LFS_DEVICE_STOPPING );

				if (NT_SUCCESS(Irp->IoStatus.Status)) {

					ASSERT( !FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_STOP) );
					LfsCleanupMountedDevice( lfsDeviceExt->FileSpyDeviceObject );
				}

				*NtStatus = Irp->IoStatus.Status;
				IoCompleteRequest( Irp, IO_NO_INCREMENT );

				result = TRUE;
				break;
			}

			result = FALSE;
			break;
		}

#if 0

		if (irpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) {

			if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDAS_FAT ||
				lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDAS_NTFS) {

				if (!(irpSp->MinorFunction == IRP_MN_USER_FS_REQUEST || irpSp->MinorFunction == IRP_MN_KERNEL_CALL)) {

					result = FALSE;
					break;
				}
										
				if (irpSp->Parameters.FileSystemControl.FsControlCode != FSCTL_DISMOUNT_VOLUME) {

					result = FALSE;
					break;
				}
			}
			
			//PrintIrp( LFS_DEBUG_LFS_INFO, __FUNCTION__, lfsDeviceExt, Irp );

			if (irpSp->MinorFunction == IRP_MN_USER_FS_REQUEST || irpSp->MinorFunction == IRP_MN_KERNEL_CALL) {

				//	Do not allow exclusive access to the volume and dismount volume to protect format
				//	We allow exclusive access if secondaries are connected locally.
				//

				if (irpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_LOCK_VOLUME) {
						
					if (NetdiskManager_ThisVolumeHasSecondary(GlobalLfs.NetdiskManager,
															  lfsDeviceExt->NetdiskPartition,
															  lfsDeviceExt->NetdiskEnabledMode,
															  FALSE) ) {

						SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
									   ("Primary_PassThrough, IRP_MJ_FILE_SYSTEM_CONTROL: Trying to acquire the volume exclusively. \n") );

						ASSERT( fileObject && fileObject->FileName.Length == 0 && fileObject->RelatedFileObject == NULL ); 

						*NtStatus = Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
						Irp->IoStatus.Information = 0;
						IoCompleteRequest( Irp, IO_DISK_INCREMENT );

						result = TRUE;
						break;
					}
					
				} else if (irpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_UNLOCK_VOLUME) {

					result = FALSE;
					break;
	
				} else if (irpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_DISMOUNT_VOLUME) {
						
					KEVENT				waitEvent;

					if (NetdiskManager_ThisVolumeHasSecondary( GlobalLfs.NetdiskManager,
															   lfsDeviceExt->NetdiskPartition,
															   lfsDeviceExt->NetdiskEnabledMode,
															   FALSE) ) {

						ASSERT ( FALSE );

						SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
									   ("Primary_PassThrough, IRP_MJ_FILE_SYSTEM_CONTROL: Trying to acquire the volume exclusively. \n") );

						ASSERT( fileObject && fileObject->FileName.Length == 0 && fileObject->RelatedFileObject == NULL ); 

						*NtStatus = Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
						Irp->IoStatus.Information = 0;
						IoCompleteRequest( Irp, IO_DISK_INCREMENT );

						result = TRUE;
						break;
					}
	
					SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_STOPPING );

					IoCopyCurrentIrpStackLocationToNext( Irp );
					KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

					IoSetCompletionRoutine( Irp,
											LfsPassThroughCompletion,
											&waitEvent,
											TRUE,
											TRUE,
											TRUE );

					*NtStatus = IoCallDriver( lfsDeviceExt->AttachedToDeviceObject, Irp );

					if (*NtStatus == STATUS_PENDING) {

						*NtStatus = KeWaitForSingleObject( &waitEvent, Executive, KernelMode, FALSE, NULL );
						ASSERT( *NtStatus == STATUS_SUCCESS );
					}

					ASSERT( KeReadStateEvent(&waitEvent) || !NT_SUCCESS(Irp->IoStatus.Status) );

					ClearFlag( lfsDeviceExt->Flags, LFS_DEVICE_STOPPING );
	
					if (NT_SUCCESS(Irp->IoStatus.Status)) {

						ASSERT( !FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_STOP) );
						LfsCleanupMountedDevice( lfsDeviceExt->FileSpyDeviceObject );
					}

					*NtStatus = Irp->IoStatus.Status;
					IoCompleteRequest( Irp, IO_NO_INCREMENT );

					SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, 
								   ("%s: FSCTL_DISMOUNT_VOLUME Irp->IoStatus.Status = %x\n",
									__FUNCTION__, Irp->IoStatus.Status) );

					result = TRUE;
					break;

				} else if (irpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_SET_ENCRYPTION) {

					//
					//	Do not support encryption.
					//

					SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
								   ("Primary_PassThrough, IRP_MJ_FILE_SYSTEM_CONTROL: Setting encryption denied.\n") );

					*NtStatus = Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
					Irp->IoStatus.Information = 0;
					IoCompleteRequest( Irp, IO_DISK_INCREMENT );

					result = TRUE;
					break;
				}
			}
		}
#endif

		break;
	}

	case LFS_PRIMARY: 
	case LFS_SECONDARY_TO_PRIMARY: {

PRIMARY_PASS:

		if (!(FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_START) && !FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_STOP))) {

			if (lfsDeviceExt->Secondary != NULL && 
				Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject) != NULL) {
				
				LfsDbgBreakPoint();

				*NtStatus = Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest( Irp, IO_DISK_INCREMENT );

				LfsDeviceExt_Dereference( lfsDeviceExt );
				return TRUE;
			}

			LfsDeviceExt_Dereference( lfsDeviceExt );
			return FALSE;
		}

#if __LFS_FLUSH_VOLUME__

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
		//PrintIrp( LFS_DEBUG_LFS_INFO, __FUNCTION__, lfsDeviceExt, Irp );

		if (irpSp->MajorFunction == IRP_MJ_PNP) {

			//PrintIrp( LFS_DEBUG_LFS_INFO, __FUNCTION__, lfsDeviceExt, Irp );

			if (irpSp->MinorFunction == IRP_MN_SURPRISE_REMOVAL) {

				if (lfsDeviceExt->NetdiskPartition == NULL) {

					LfsDbgBreakPoint();
				
				} else {

					NetdiskManager_PrimarySessionStopping( GlobalLfs.NetdiskManager,
														   lfsDeviceExt->NetdiskPartition,
														   lfsDeviceExt->NetdiskEnabledMode	);

					NetdiskManager_PrimarySessionDisconnect( GlobalLfs.NetdiskManager,
														     lfsDeviceExt->NetdiskPartition,
														     lfsDeviceExt->NetdiskEnabledMode );

					NetdiskManager_SurpriseRemoval( GlobalLfs.NetdiskManager,
													lfsDeviceExt->NetdiskPartition,
													lfsDeviceExt->NetdiskEnabledMode );
				}

				lfsDeviceExt->NetdiskPartition = NULL;			
				SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_SURPRISE_REMOVAL );

				result = FALSE;
				break;

			}

			// Need to test much whether this is okay..
	
			if (irpSp->MinorFunction == IRP_MN_CANCEL_REMOVE_DEVICE || irpSp->MinorFunction == IRP_MN_REMOVE_DEVICE) {

				result = FALSE;
				break;
			
			} else if (irpSp->MinorFunction == IRP_MN_QUERY_REMOVE_DEVICE) {

				KEVENT				waitEvent;


				if (lfsDeviceExt->AttachedToDeviceObject == NULL) {

					LfsDbgBreakPoint();

					result = FALSE;
					break;
				}

				SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_STOPPING );

				NetdiskManager_PrimarySessionStopping( GlobalLfs.NetdiskManager,
													   lfsDeviceExt->NetdiskPartition,
													   lfsDeviceExt->NetdiskEnabledMode	);

				IoCopyCurrentIrpStackLocationToNext( Irp );
				KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

				IoSetCompletionRoutine( Irp,
										LfsPassThroughCompletion,
										&waitEvent,
										TRUE,
										TRUE,
										TRUE );

				*NtStatus = IoCallDriver( lfsDeviceExt->AttachedToDeviceObject, Irp );

				if (*NtStatus == STATUS_PENDING) {

					*NtStatus = KeWaitForSingleObject( &waitEvent, Executive, KernelMode, FALSE, NULL );
					ASSERT( *NtStatus == STATUS_SUCCESS );
				}

				ASSERT( KeReadStateEvent(&waitEvent) || !NT_SUCCESS(Irp->IoStatus.Status) );
	
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, 
							   ("%s: lfsDeviceExt = %p, IRP_MN_QUERY_REMOVE_DEVICE Irp->IoStatus.Status = %x\n",
								__FUNCTION__, lfsDeviceExt, Irp->IoStatus.Status) );

				if (NT_SUCCESS(Irp->IoStatus.Status)) {

					NetdiskManager_PrimarySessionDisconnect( GlobalLfs.NetdiskManager,
															 lfsDeviceExt->NetdiskPartition,
															 lfsDeviceExt->NetdiskEnabledMode );

				} else {
					
					NetdiskManager_PrimarySessionCancelStopping( GlobalLfs.NetdiskManager,
																 lfsDeviceExt->NetdiskPartition,
																 lfsDeviceExt->NetdiskEnabledMode );
				}

				ClearFlag( lfsDeviceExt->Flags, LFS_DEVICE_STOPPING );

				if (NT_SUCCESS(Irp->IoStatus.Status)) {

					ASSERT( !FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_STOP) );
					LfsCleanupMountedDevice( lfsDeviceExt->FileSpyDeviceObject );
				}

				*NtStatus = Irp->IoStatus.Status;
				IoCompleteRequest( Irp, IO_NO_INCREMENT );

				result = TRUE;
				break;
			}

			result = FALSE;
			break;
		}

		if (irpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) {

			if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDAS_FAT ||
				lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDAS_NTFS) {

				if (!(irpSp->MinorFunction == IRP_MN_USER_FS_REQUEST || irpSp->MinorFunction == IRP_MN_KERNEL_CALL)) {

					result = FALSE;
					break;
				}
										
				//if (irpSp->Parameters.FileSystemControl.FsControlCode != FSCTL_DISMOUNT_VOLUME) {

				//	result = FALSE;
				//	break;
				//}
			}
			
			//PrintIrp( LFS_DEBUG_LFS_INFO, __FUNCTION__, lfsDeviceExt, Irp );

			if (irpSp->MinorFunction == IRP_MN_USER_FS_REQUEST || irpSp->MinorFunction == IRP_MN_KERNEL_CALL) {

				//	Do not allow exclusive access to the volume and dismount volume to protect format
				//	We allow exclusive access if secondaries are connected locally.
				//

				if (irpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_LOCK_VOLUME) {
						
					if (NetdiskManager_ThisVolumeHasSecondary(GlobalLfs.NetdiskManager,
															  lfsDeviceExt->NetdiskPartition,
															  lfsDeviceExt->NetdiskEnabledMode,
															  FALSE) ) {

						SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
									   ("Primary_PassThrough, IRP_MJ_FILE_SYSTEM_CONTROL: Trying to acquire the volume exclusively. \n") );

						ASSERT( fileObject && fileObject->FileName.Length == 0 && fileObject->RelatedFileObject == NULL ); 

						*NtStatus = Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
						Irp->IoStatus.Information = 0;
						IoCompleteRequest( Irp, IO_DISK_INCREMENT );

						result = TRUE;
						break;
					}
					
				} else if (irpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_UNLOCK_VOLUME) {

					result = FALSE;
					break;
	
				} else if (irpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_DISMOUNT_VOLUME) {
						
					KEVENT				waitEvent;

					if (NetdiskManager_ThisVolumeHasSecondary( GlobalLfs.NetdiskManager,
															   lfsDeviceExt->NetdiskPartition,
															   lfsDeviceExt->NetdiskEnabledMode,
															   FALSE) ) {

						ASSERT ( FALSE );

						SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
									   ("Primary_PassThrough, IRP_MJ_FILE_SYSTEM_CONTROL: Trying to acquire the volume exclusively. \n") );

						ASSERT( fileObject && fileObject->FileName.Length == 0 && fileObject->RelatedFileObject == NULL ); 

						*NtStatus = Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
						Irp->IoStatus.Information = 0;
						IoCompleteRequest( Irp, IO_DISK_INCREMENT );

						result = TRUE;
						break;
					}
	
					SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_STOPPING );

					IoCopyCurrentIrpStackLocationToNext( Irp );
					KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

					IoSetCompletionRoutine( Irp,
											LfsPassThroughCompletion,
											&waitEvent,
											TRUE,
											TRUE,
											TRUE );

					*NtStatus = IoCallDriver( lfsDeviceExt->AttachedToDeviceObject, Irp );

					if (*NtStatus == STATUS_PENDING) {

						*NtStatus = KeWaitForSingleObject( &waitEvent, Executive, KernelMode, FALSE, NULL );
						ASSERT( *NtStatus == STATUS_SUCCESS );
					}

					ASSERT( KeReadStateEvent(&waitEvent) || !NT_SUCCESS(Irp->IoStatus.Status) );

					ClearFlag( lfsDeviceExt->Flags, LFS_DEVICE_STOPPING );
	
					if (NT_SUCCESS(Irp->IoStatus.Status)) {

						ASSERT( !FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_STOP) );
						LfsCleanupMountedDevice( lfsDeviceExt->FileSpyDeviceObject );
					}

					*NtStatus = Irp->IoStatus.Status;
					IoCompleteRequest( Irp, IO_NO_INCREMENT );

					SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, 
								   ("%s: FSCTL_DISMOUNT_VOLUME Irp->IoStatus.Status = %x\n",
									__FUNCTION__, Irp->IoStatus.Status) );

					result = TRUE;
					break;

				} else if (irpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_SET_ENCRYPTION) {

					//
					//	Do not support encryption.
					//

					SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
								   ("Primary_PassThrough, IRP_MJ_FILE_SYSTEM_CONTROL: Setting encryption denied.\n") );

					*NtStatus = Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
					Irp->IoStatus.Information = 0;
					IoCompleteRequest( Irp, IO_DISK_INCREMENT );

					result = TRUE;
					break;
				}
			}
		}

		break;
	}

	case LFS_SECONDARY: {

		if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDAS_FAT ||
			lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDAS_NTFS) {

			goto PRIMARY_PASS;
		}

		if (!(FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_START) && !FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_STOP))) {

			if (lfsDeviceExt->Secondary != NULL && 
				Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject) != NULL) {

				ASSERT( FALSE );

				if (irpSp->MajorFunction != IRP_MJ_CLOSE) {
		
					*NtStatus = Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
					Irp->IoStatus.Information = 0;
					IoCompleteRequest( Irp, IO_DISK_INCREMENT );

					LfsDeviceExt_Dereference( lfsDeviceExt );
					return TRUE;
				}
			
			} else {

				LfsDeviceExt_Dereference( lfsDeviceExt );
				return FALSE;
			}
		}

		if (FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_START) && FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_STOPPING)) {

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

				SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
							   ("LfsPassThrough: Win2K: COMPLETING with access denied: a request comes down during LFS_DEVICE_STOPPING. IRP :%x(%x)\n", irpSp->MajorFunction, irpSp->MinorFunction));

				LfsDeviceExt_Dereference( lfsDeviceExt );
				*NtStatus = Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest( Irp, IO_DISK_INCREMENT );
				
				return TRUE;
			}
		}

#if DBG
		CountIrpMajorFunction( lfsDeviceExt, irpSp->MajorFunction );

		if (fileObject && 
			(irpSp->MajorFunction == IRP_MJ_CREATE || irpSp->MajorFunction == IRP_MJ_CLOSE || irpSp->MajorFunction == IRP_MJ_CLEANUP))
			PrintIrp( LFS_DEBUG_LFS_TRACE, NULL, lfsDeviceExt, Irp );
#endif

		ASSERT( lfsDeviceExt->Secondary );

		//
		//	Check surprise flag
		//

		if (FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_SURPRISE_REMOVAL)) {

			if (irpSp->MajorFunction == IRP_MJ_PNP) {

				if (fileObject && Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject)) {

					*NtStatus = Irp->IoStatus.Status = STATUS_SUCCESS;
					Irp->IoStatus.Information = 0;
					IoCompleteRequest( Irp, IO_DISK_INCREMENT );

					result = TRUE;
				
				} else {
				
					result = FALSE;
					break;
				}
			
			} else {

				*NtStatus = Irp->IoStatus.Status = STATUS_DEVICE_REMOVED;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest(Irp, IO_DISK_INCREMENT);

				result = TRUE;
				break;
			}
		}

		if (irpSp->MajorFunction == IRP_MJ_PNP) {

			if (lfsDeviceExt->SecondaryState == VOLUME_PURGE_STATE) {

				if (irpSp->MinorFunction != IRP_MN_SURPRISE_REMOVAL) {

					result = FALSE;
					break;
				}
			}

			if (irpSp->MinorFunction == IRP_MN_SURPRISE_REMOVAL) {

				ASSERT( lfsDeviceExt->NetdiskPartition );
				NetdiskManager_SurpriseRemoval( GlobalLfs.NetdiskManager,
												lfsDeviceExt->NetdiskPartition,
												lfsDeviceExt->NetdiskEnabledMode );

				lfsDeviceExt->NetdiskPartition = NULL;			
				SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_SURPRISE_REMOVAL );

				result = FALSE;

			} else if (irpSp->MinorFunction == IRP_MN_QUERY_REMOVE_DEVICE) {
				
				if (FlagOn(lfsDeviceExt->Secondary->Flags, SECONDARY_FLAG_RECONNECTING)) {  // While Disabling and disconnected
	
					*NtStatus = Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
					Irp->IoStatus.Information = 0;
					IoCompleteRequest( Irp, IO_DISK_INCREMENT );
	
					result = TRUE;
					break;
				}

				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("Secondary_PassThrough: IRP_MN_QUERY_REMOVE_DEVICE Entered\n") );
		
				Secondary_TryCloseFilExts( lfsDeviceExt->Secondary );
			
				if (!IsListEmpty(&lfsDeviceExt->Secondary->FcbQueue)) {

					LARGE_INTEGER interval;
				
					// Wait all files closed
					interval.QuadPart = (1 * DELAY_ONE_SECOND);      //delay 1 seconds
					KeDelayExecutionThread( KernelMode, FALSE, &interval );
				}
			
				if (!IsListEmpty(&lfsDeviceExt->Secondary->FcbQueue)) {

					*NtStatus = Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
					Irp->IoStatus.Information = 0;
					IoCompleteRequest( Irp, IO_DISK_INCREMENT );
				
					result = TRUE;
				
				} else {

					result = FALSE;
				}

			} else if (irpSp->MinorFunction == IRP_MN_REMOVE_DEVICE) {

				if (!IsListEmpty(&lfsDeviceExt->Secondary->FcbQueue)) {

					ASSERT( LFS_BUG );
					*NtStatus = Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
					Irp->IoStatus.Information = 0;
					IoCompleteRequest( Irp, IO_DISK_INCREMENT );
					
					result = TRUE;
				
				} else {

					result = FALSE;			
				}

			} else {

				result = FALSE;
				break;

				if (fileObject && Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject)) {

					*NtStatus = Irp->IoStatus.Status = STATUS_SUCCESS;
					Irp->IoStatus.Information = 0;
					IoCompleteRequest( Irp, IO_DISK_INCREMENT );
									
					result = TRUE;
				
				} else {

					result = FALSE;
					break;
				}
			}

			if (result == TRUE) {

				break;
			}

			if (irpSp->MinorFunction == IRP_MN_QUERY_REMOVE_DEVICE) {

				KEVENT				waitEvent;


				if (lfsDeviceExt->AttachedToDeviceObject == NULL) {

					LfsDbgBreakPoint();

					result = FALSE;
					break;
				}

				SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_STOPPING );

				IoCopyCurrentIrpStackLocationToNext( Irp );
				KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

				IoSetCompletionRoutine( Irp,
										LfsPassThroughCompletion,
										&waitEvent,
										TRUE,
										TRUE,
										TRUE );

				*NtStatus = IoCallDriver( lfsDeviceExt->AttachedToDeviceObject, Irp );

				if (*NtStatus == STATUS_PENDING) {

					*NtStatus = KeWaitForSingleObject( &waitEvent, Executive, KernelMode, FALSE, NULL );
					ASSERT( *NtStatus == STATUS_SUCCESS );
				}

				ASSERT( KeReadStateEvent(&waitEvent) || !NT_SUCCESS(Irp->IoStatus.Status) );
	
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO, 
							   ("%s: lfsDeviceExt = %p, IRP_MN_QUERY_REMOVE_DEVICE Irp->IoStatus.Status = %x\n",
								__FUNCTION__, lfsDeviceExt, Irp->IoStatus.Status) );

				ClearFlag( lfsDeviceExt->Flags, LFS_DEVICE_STOPPING );

				if (NT_SUCCESS(Irp->IoStatus.Status)) {

					ASSERT( !FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_STOP) );
					LfsCleanupMountedDevice( lfsDeviceExt->FileSpyDeviceObject );
				}

				*NtStatus = Irp->IoStatus.Status;
				IoCompleteRequest( Irp, IO_NO_INCREMENT );

				result = TRUE;
				break;
			}

			result = FALSE;
			break;
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

		if (irpSp->MajorFunction == IRP_MJ_WRITE) {

			if (Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject) == NULL) {

				if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NTFS ||
					lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_FAT && (lfsDeviceExt->FilteringMode != CONNECT_TO_LOCAL_STATE)) {
				
					SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsPassThrough Fake Write %wZ\n", &fileObject->FileName) );

					*NtStatus = Irp->IoStatus.Status = STATUS_SUCCESS;
					Irp->IoStatus.Information = irpSp->Parameters.Write.Length;
					IoCompleteRequest( Irp, IO_DISK_INCREMENT );

					result = TRUE;
					break;
				}
			}
		}

		if (irpSp->MajorFunction == IRP_MJ_CREATE) {
			
			if (fileObject && RtlEqualUnicodeString(&lfsDeviceExt->MountMgr, &fileObject->FileName, TRUE) || 
				fileObject && RtlEqualUnicodeString(&lfsDeviceExt->ExtendPlus, &fileObject->FileName, TRUE)) {

				SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, 
							  ("LfsPassThrough: %wZ returned with STATUS_OBJECT_NAME_NOT_FOUND\n", &fileObject->FileName) );

				*NtStatus = Irp->IoStatus.Status = STATUS_OBJECT_NAME_NOT_FOUND;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest(Irp, IO_DISK_INCREMENT);

				result = TRUE;
				break;
			}
		}

		//
		//
		//	Do not allow exclusive access to the volume in secondary mode.
		//	But, in purging process, allow.
		//

		ExAcquireFastMutex( &lfsDeviceExt->FastMutex );
		
		if (irpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) {

			if (irpSp->MinorFunction == IRP_MN_USER_FS_REQUEST || irpSp->MinorFunction == IRP_MN_KERNEL_CALL) {

				struct FileSystemControl	*fileSystemControl =(struct FileSystemControl *)&(irpSp->Parameters.FileSystemControl);

				//
				//	Do not allow exclusive access to the volume
				//

				if (fileSystemControl->FsControlCode == FSCTL_LOCK_VOLUME) {

					if (lfsDeviceExt->SecondaryState != VOLUME_PURGE_STATE) {
				
						ExReleaseFastMutex( &lfsDeviceExt->FastMutex );

						SPY_LOG_PRINT( LFS_DEBUG_LFS_ERROR,
									   ("LfsPassThrough, IRP_MJ_FILE_SYSTEM_CONTROL: Return failure of acquiring the volume exclusively.\n") );

						ASSERT( fileObject && fileObject->FileName.Length == 0 && fileObject->RelatedFileObject == NULL );

						*NtStatus = Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
						Irp->IoStatus.Information = 0;
						IoCompleteRequest( Irp, IO_DISK_INCREMENT );

						result = TRUE;
						break;
					}
				}
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

					break;
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
				break;
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

		ASSERT( fileObject );

		if (lfsDeviceExt->SecondaryState == SECONDARY_STATE) {
		
			if (irpSp->MajorFunction != IRP_MJ_CREATE && 
				Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject) == NULL) {

				result = FALSE;
				break;
			} 
		
		} else if (lfsDeviceExt->SecondaryState == VOLUME_PURGE_STATE/* && irpSp->MajorFunction != IRP_MJ_DEVICE_CONTROL*/) {

#if DBG
			if (irpSp->MajorFunction == IRP_MJ_CREATE && fileObject->FileName.Length)
				PrintIrp( LFS_DEBUG_LFS_TRACE, "LfsPassThrough VOLUME_PURGE_STATE", lfsDeviceExt, Irp );
#endif

			if (fileObject != NULL &&
				Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject) == NULL && 
				(fileObject->RelatedFileObject == NULL ||
				Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject->RelatedFileObject) == NULL)) {

				 result = FALSE;
				 break;
			}
				
#if DBG
			PrintIrp( LFS_DEBUG_LFS_TRACE, "LfsPassThrough VOLUME_PURGE_STATE", lfsDeviceExt, Irp );
#endif
			ASSERT( fileObject->DeviceObject == lfsDeviceExt->DiskDeviceObject );			

		} else if (lfsDeviceExt->SecondaryState == CONNECT_TO_LOCAL_STATE) {

#if DBG
			PrintIrp( LFS_DEBUG_LFS_TRACE, "LfsPassThrough CONNECT_TO_LOCAL_STATE", lfsDeviceExt, Irp );
#endif
				
			if (fileObject != NULL &&
				Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject) == NULL && 
				(fileObject->RelatedFileObject == NULL ||
				Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject->RelatedFileObject) == NULL)) {

				result = FALSE;
				break;
			
			} 
		
		} else {

			LfsDbgBreakPoint();
		}

		if (irpSp->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL	|| 	// 0x0f
		   irpSp->MajorFunction == IRP_MJ_SHUTDOWN					||  // 0x10
		   irpSp->MajorFunction == IRP_MJ_CREATE_MAILSLOT			||  // 0x13
		   irpSp->MajorFunction == IRP_MJ_POWER						||  // 0x16
		   irpSp->MajorFunction == IRP_MJ_SYSTEM_CONTROL			||  // 0x17
		   irpSp->MajorFunction == IRP_MJ_DEVICE_CHANGE) {				// 0x18
	
			ASSERT( LFS_REQUIRED );
			PrintIrp( LFS_DEBUG_SECONDARY_TRACE, __FUNCTION__, lfsDeviceExt, Irp );

			*NtStatus = Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
			Irp->IoStatus.Information = 0;
			IoCompleteRequest( Irp, IO_DISK_INCREMENT );

			result = TRUE;
		
		} else if (irpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL) {

			PFILE_EXTENTION	fileExt = NULL;

			fileExt = Secondary_LookUpFileExtension( lfsDeviceExt->Secondary, fileObject );
		
			if (fileExt == NULL || fileExt->TypeOfOpen != UserVolumeOpen) {

				Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
				*NtStatus = STATUS_INVALID_PARAMETER;
				IoCompleteRequest( Irp, IO_DISK_INCREMENT );
				
				result = TRUE;
				break;
			}

			result = Secondary_Ioctl( lfsDeviceExt->Secondary, Irp, irpSp, NtStatus );
			
		} else {

			result = Secondary_PassThrough( lfsDeviceExt->Secondary, Irp, NtStatus );
		}

		if (result == FALSE) {

			if (lfsDeviceExt->SecondaryState == CONNECT_TO_LOCAL_STATE) {

				if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_FAT) {

					goto PRIMARY_PASS;
					//result = Primary_PassThrough( lfsDeviceExt, Irp, NtStatus );
				}
			}
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
LfsPassThroughCompletion (
	IN PDEVICE_OBJECT	DeviceObject,
	IN PIRP				Irp,
	IN PKEVENT			WaitEvent
	)
{
	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("%s: called\n", __FUNCTION__) );

	UNREFERENCED_PARAMETER( DeviceObject );
	UNREFERENCED_PARAMETER( Irp );

	KeSetEvent( WaitEvent, IO_NO_INCREMENT, FALSE );

	return STATUS_MORE_PROCESSING_REQUIRED;
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
#if __LFSDBG_MEMORY__
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

#if __LFSDBG_MEMORY__
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

#if __LFSDBG_MEMORY__
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

#if __LFSDBG_MEMORY__
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

	//timeOut.QuadPart = -60*HZ;		// 60 sec
	timeOut.QuadPart = - (LFS_CHANGECONNECTION_TIMEOUT);
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
	NTSTATUS	status;
	NTSTATUS	purgeStatus;


	ASSERT( LfsDeviceExt->SecondaryState == SECONDARY_STATE );

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("SecondaryToPrimaryThreadProc: LfsDeviceExt = %p, LfsDeviceExt->SecondaryState = %d\n",
										 LfsDeviceExt, LfsDeviceExt->SecondaryState) );

	status = NetdiskManager_Secondary2Primary( GlobalLfs.NetdiskManager, 
											   LfsDeviceExt->NetdiskPartition, 
											   LfsDeviceExt->NetdiskEnabledMode );

	if (status != STATUS_SUCCESS) {
	
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
	NTSTATUS				status;

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

	
	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
				   ("PurgeVolume: LfsDeviceExt = %p, LfsDeviceExt->FileSystemType = %d LfsDeviceExt->Vpb->ReferenceCount = %d\n", 
					 LfsDeviceExt, LfsDeviceExt->FileSystemType, LfsDeviceExt->Vpb->ReferenceCount) );

	if (LfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_FAT) {

#ifndef _WIN64

		if(IS_WINDOWS2K()) {

			W2kFatPurgeVolume( NULL, LfsDeviceExt->BaseVolumeDeviceObject );			
		
		} else 
		if(IS_WINDOWSXP()) {

			WxpFatPurgeVolume( NULL, LfsDeviceExt->BaseVolumeDeviceObject );
		
		} else
#endif
		if(IS_WINDOWSNET()) {

			WnetFatPurgeVolume( NULL, LfsDeviceExt->BaseVolumeDeviceObject );
		
		}else {

			ASSERT( LFS_REQUIRED );
			return STATUS_UNSUCCESSFUL;
		}

		NetdiskManager_ChangeMode( GlobalLfs.NetdiskManager,
								   LfsDeviceExt->NetdiskPartition,
								   &LfsDeviceExt->NetdiskEnabledMode );

		return STATUS_SUCCESS;
	}

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

	status = ZwCreateFile( &fileHandle,
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
				   ("PurgeVolume: LfsDeviceExt = %p ZwCreateFile fileHandle =%p, status = %X, ioStatusBlock = %X\n",
					 LfsDeviceExt, fileHandle, status, ioStatusBlock.Information) );

	if (status != STATUS_SUCCESS) {
	
		return status;
	
	} else {

		ASSERT( ioStatusBlock.Information == FILE_OPENED );
	}

	do {

#if 0

		status = ZwCreateEvent( &eventHandle,
								GENERIC_READ,
								NULL,
								SynchronizationEvent,
								FALSE );

		if (status != STATUS_SUCCESS) {

			ASSERT( LFS_UNEXPECTED );
			break;
		}
		
		RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );
		
		status = ZwFsControlFile( fileHandle,
								  eventHandle,
								  NULL,
								  NULL,
								  &ioStatusBlock,
								  FSCTL_DISMOUNT_VOLUME,
								  NULL,
								  0,
								  NULL,
								  0 );

		if (status == STATUS_PENDING) {

			status = ZwWaitForSingleObject( eventHandle, TRUE, NULL );

			if (status != STATUS_SUCCESS)
				LfsDbgBreakPoint();
			
			status = ioStatusBlock.Status;
		}

		ZwClose( eventHandle );

		if (status != STATUS_SUCCESS) {

			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
							("PurgeVolume: LfsDeviceExt = %p ZwFsControlFile FSCTL_DISMOUNT_VOLUME fileHandle =%p, status = %X, ioStatusBlock = %X\n",
							LfsDeviceExt, fileHandle, status, ioStatusBlock.Information) );
		
			break;
		}

		break;

#endif

		status = ZwCreateEvent( &eventHandle,
								GENERIC_READ,
								NULL,
								SynchronizationEvent,
								FALSE );

		if (status != STATUS_SUCCESS) {

			ASSERT( LFS_UNEXPECTED );
			break;
		}
		
		RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );
		
		status = ZwFsControlFile( fileHandle,
								  eventHandle,
								  NULL,
								  NULL,
								  &ioStatusBlock,
								  FSCTL_LOCK_VOLUME,
								  NULL,
								  0,
								  NULL,
								  0 );

		if (status == STATUS_PENDING) {

			status = ZwWaitForSingleObject( eventHandle, TRUE, NULL );

			if (status != STATUS_SUCCESS)
				LfsDbgBreakPoint();
			
			status = ioStatusBlock.Status;
		}

		ZwClose( eventHandle );

		if (status != STATUS_SUCCESS) {

			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
							("PurgeVolume: LfsDeviceExt = %p ZwFsControlFile FSCTL_LOCK_VOLUME fileHandle =%p, status = %X, ioStatusBlock = %X\n",
							LfsDeviceExt, fileHandle, status, ioStatusBlock.Information) );
		
			break;
		}

		status = ZwCreateEvent( &eventHandle,
								GENERIC_READ,
								NULL,
								SynchronizationEvent,
								FALSE );

		if (status != STATUS_SUCCESS) {

			ASSERT( LFS_UNEXPECTED );
			break;
		}
		
		RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );
		
		status = ZwFsControlFile( fileHandle,
								  eventHandle,
								  NULL,
								  NULL,
								  &ioStatusBlock,
								  FSCTL_UNLOCK_VOLUME,
								  NULL,
								  0,
								  NULL,
								  0 );

		if (status == STATUS_PENDING) {

			status = ZwWaitForSingleObject( eventHandle, TRUE, NULL );

			if (status != STATUS_SUCCESS) {
				
				LfsDbgBreakPoint();
				break;
			}

			status = ioStatusBlock.Status;
		}

		ZwClose( eventHandle );

		if (status != STATUS_SUCCESS) {

			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
							("PurgeVolume: LfsDeviceExt = %p ZwFsControlFile FSCTL_UNLOCK_VOLUME fileHandle =%p, status = %X, ioStatusBlock = %X\n",
							LfsDeviceExt, fileHandle, status, ioStatusBlock.Information) );
		
			break;
		}
	
	} while(0);

	ZwClose( fileHandle );

	if (status != STATUS_SUCCESS)
		return status;

	do {

		UNICODE_STRING			fileName;
		PWCHAR					fileNameBuffer;

		fileNameBuffer = ExAllocatePoolWithTag(PagedPool,NDFS_MAX_PATH, LFS_ALLOC_TAG);
		if(fileNameBuffer == NULL) {
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		if(GlobalLfs.ShutdownOccured == TRUE)
			break;

		RtlInitEmptyUnicodeString( &fileName,
								   fileNameBuffer,
								   NDFS_MAX_PATH );
        
		RtlCopyUnicodeString( &fileName, &LfsDeviceExt->NetdiskPartitionInformation.VolumeName );

		ioStatusBlock.Information = 0;

		status = RtlAppendUnicodeToString( &fileName, REMOUNT_VOLUME_FILE_NAME );

		if (status != STATUS_SUCCESS) {

			ExFreePool( fileNameBuffer );
			ASSERT( LFS_UNEXPECTED );

			status = STATUS_SUCCESS;
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
		
		status = ZwCreateFile( &fileHandle,
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
					   ("PurgeVolume:	New Volume Create ? %p %wZ, status = %x, ioStatusBlock.Information = %d\n",
			             LfsDeviceExt, &LfsDeviceExt->NetdiskPartitionInformation.VolumeName, status, ioStatusBlock.Information) );

		if (status == STATUS_SUCCESS) {

			ASSERT( ioStatusBlock.Information == FILE_OPENED );
			ZwClose( fileHandle );
		
		} else {

			//if (!FlagOn(LfsDeviceExt->Flags, LFS_DEVICE_STOP))
			//	LfsDbgBreakPoint();
		}

		status = STATUS_SUCCESS;

		ExFreePool( fileNameBuffer );

	} while(0);

	return status;
}


BOOLEAN
Lfs_IsLocalAddress(
	PLPX_ADDRESS	LpxAddress
	)
{
	LONG		idx_enabled;

	KeEnterCriticalRegion();
	ExAcquireFastMutex( &GlobalLfs.Primary->FastMutex );

	for (idx_enabled = 0; idx_enabled < MAX_SOCKETLPX_INTERFACE; idx_enabled ++) {

		if (GlobalLfs.Primary->Agent.ListenSocket[idx_enabled].Active &&
			RtlEqualMemory( LpxAddress->Node,
							GlobalLfs.Primary->Agent.ListenSocket[idx_enabled].NICAddress.Node,
							ETHER_ADDR_LENGTH)) {

			ExReleaseFastMutex( &GlobalLfs.Primary->FastMutex );
			KeLeaveCriticalRegion();
			return TRUE;
		}
	}

	ExReleaseFastMutex( &GlobalLfs.Primary->FastMutex );
	KeLeaveCriticalRegion();
	
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
	
#if __LFS_NDSC_ID__
	RtlCopyMemory( netdiskPartitionInfo.NdscId,
				   NetdiskPartitionInformation->NetdiskInformation.NdscId,
				   NDSC_ID_LENGTH );
#endif

	netdiskPartitionInfo.StartingOffset.QuadPart = 0;
	

	status = LfsTable_QueryPrimaryAddress( GlobalLfs.LfsTable,
										   &netdiskPartitionInfo,
										   PrimaryAddress );
	
	if (status == STATUS_SUCCESS && IsLocalAddress) 
		*IsLocalAddress = Lfs_IsLocalAddress( PrimaryAddress );
    
	return status;
}


NTSTATUS
CallbackSecondaryToPrimary(
	IN  PDEVICE_OBJECT	DiskDeviceObject
	)
{
	NTSTATUS			status;
	PNETDISK_PARTITION	netdiskPartition;

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, 
				   ("CallbackSecondaryToPrimary: Entered DiskDeviceObject = %p\n", DiskDeviceObject) );

	netdiskPartition = NetdiskManager_QueryNetdiskPartition( GlobalLfs.NetdiskManager, DiskDeviceObject );

	if (netdiskPartition == NULL) {

		LfsDbgBreakPoint();
		return STATUS_UNSUCCESSFUL;
	}

	status = NetdiskManager_Secondary2Primary( GlobalLfs.NetdiskManager, 
											   netdiskPartition,
											   NETDISK_SECONDARY );

	if (status == STATUS_SUCCESS) {

		NetdiskManager_ChangeMode( GlobalLfs.NetdiskManager,
								   netdiskPartition,
								   NULL );
	}

	NetdiskPartition_Dereference( netdiskPartition );

	return status;
}


NTSTATUS
CallbackAddWriteRange(
	IN	PDEVICE_OBJECT	DiskDeviceObject,
	OUT PBLOCKACE_ID	BlockAceId
	)
{
	NTSTATUS			status;
	PNETDISK_PARTITION	netdiskPartition;

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, 
					("CallbackAddWriteRange: Entered DiskDeviceObject = %p\n", DiskDeviceObject) );

	netdiskPartition = NetdiskManager_QueryNetdiskPartition( GlobalLfs.NetdiskManager, DiskDeviceObject );

	if (netdiskPartition == NULL) {

		LfsDbgBreakPoint();
		return STATUS_UNSUCCESSFUL;
	}

	status = NetdiskManager_AddWriteRange( GlobalLfs.NetdiskManager, 
										   netdiskPartition,
										   BlockAceId );

	NetdiskPartition_Dereference( netdiskPartition );

	return status;
}



VOID
CallbackRemoveWriteRange(
	IN	PDEVICE_OBJECT	DiskDeviceObject,
	OUT BLOCKACE_ID		BlockAceId
	)
{
	PNETDISK_PARTITION	netdiskPartition;

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, 
					("CallbackRemoveWriteRange: Entered DiskDeviceObject = %p\n", DiskDeviceObject) );

	netdiskPartition = NetdiskManager_QueryNetdiskPartition( GlobalLfs.NetdiskManager, DiskDeviceObject );

	if (netdiskPartition == NULL) {

		ASSERT( FALSE );
		return;
	}

	NetdiskManager_RemoveWriteRange( GlobalLfs.NetdiskManager, 
								     netdiskPartition,
									 BlockAceId );

	NetdiskPartition_Dereference( netdiskPartition );

	return;
}


NTSTATUS
CallbackGetNdasScsiBacl(
	IN	PDEVICE_OBJECT	DiskDeviceObject,
	OUT PNDAS_BLOCK_ACL	NdasBacl,
	IN BOOLEAN			SystemOrUser
	) 
{
	NTSTATUS	status;

	status = GetNdasScsiBacl( DiskDeviceObject, NdasBacl, SystemOrUser );
		
	ASSERT( status == STATUS_SUCCESS || status == STATUS_BUFFER_OVERFLOW );

	return status;
}


#if __LFS_FLUSH_VOLUME__

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

#if DBG

VOID
PrintIrp(
	ULONG					DebugLevel,
	PCHAR					Where,
	PLFS_DEVICE_EXTENSION	LfsDeviceExt,
	PIRP					Irp
	)
{
	PIO_STACK_LOCATION  irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT		fileObject = irpSp->FileObject;
	UNICODE_STRING		nullName;
	UCHAR				minorFunction;
    CHAR				irpMajorString[OPERATION_NAME_BUFFER_SIZE];
    CHAR				irpMinorString[OPERATION_NAME_BUFFER_SIZE];

	GetIrpName ( irpSp->MajorFunction,
				 irpSp->MinorFunction,
				 irpSp->Parameters.FileSystemControl.FsControlCode,
				 irpMajorString,
				 irpMinorString );

	RtlInitUnicodeString( &nullName, L"fileObject == NULL" );
	
	if (irpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL && irpSp->MinorFunction == IRP_MN_USER_FS_REQUEST) 
		minorFunction = (UCHAR)((irpSp->Parameters.FileSystemControl.FsControlCode & 0x00003FFC) >> 2);
	else if(irpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL && irpSp->MinorFunction == 0)
		minorFunction = (UCHAR)((irpSp->Parameters.DeviceIoControl.IoControlCode & 0x00003FFC) >> 2);
	else
		minorFunction = irpSp->MinorFunction;

	ASSERT( Irp->RequestorMode == KernelMode || Irp->RequestorMode == UserMode );

	SPY_LOG_PRINT( DebugLevel,
				   ("%s Lfs: %p Irql:%d Irp:%p %s %s (%u:%u) %08x %02x ",
				   (Where) ? Where : "",
				   LfsDeviceExt, KeGetCurrentIrql(), 
				   Irp, irpMajorString, irpMinorString, irpSp->MajorFunction, minorFunction,
				   Irp->Flags, irpSp->Flags) );
					/*"%s %c%c%c%c%c ", */
					/*(Irp->RequestorMode == KernelMode) ? "KernelMode" : "UserMode",
				   (Irp->Flags & IRP_PAGING_IO) ? '*' : ' ',
				   (Irp->Flags & IRP_SYNCHRONOUS_PAGING_IO) ? '+' : ' ',
				   (Irp->Flags & IRP_SYNCHRONOUS_API) ? 'A' : ' ',
				   BooleanFlagOn(Irp->Flags,IRP_NOCACHE) ? 'N' : ' ',
				   (fileObject && fileObject->Flags & FO_SYNCHRONOUS_IO) ? '&':' ',*/
	
	SPY_LOG_PRINT( DebugLevel,
				   ("file: %p  %08x %p %wZ %d\n",
					fileObject, 
					fileObject ? fileObject->Flags : 0,
					fileObject ? fileObject->RelatedFileObject : NULL,
					fileObject ? &fileObject->FileName : &nullName,
					fileObject ? fileObject->FileName.Length : 0 ));
	return;
}

#endif
