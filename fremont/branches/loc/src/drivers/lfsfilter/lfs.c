#include "LfsProc.h"

#if DBG
BOOLEAN NdasTestBug = 1;
#else
BOOLEAN	NdasTestBug = 0;
#endif

LFS									GlobalLfs;
LFS_REGISTRY						LfsRegistry;
PFN_FSRTLTEARDOWNPERSTREAMCONTEXTS  LfsFsRtlTeardownPerStreamContexts;


NTSTATUS
LfsPassThroughCompletion (
	IN PDEVICE_OBJECT	DeviceObject,
	IN PIRP				Irp,
	IN PKEVENT			WaitEvent
	);

#if __NDAS_FS_FLUSH_VOLUME__

VOID
LfsDeviceExtThreadProc (
	IN	PLFS_DEVICE_EXTENSION	LfsDeviceExt
	);

#endif

NTSTATUS 
(*LfsOrginalNtfsAcquireFileForModWrite) (
	PFILE_OBJECT	FileObject,
	PLARGE_INTEGER	EndingOffset,
	PERESOURCE		*ResourceToRelease,
	PDEVICE_OBJECT DeviceObject) = NULL;


NTSTATUS
LfsDriverEntry (
    IN PDRIVER_OBJECT	DriverObject,
    IN PUNICODE_STRING	RegistryPath,
	IN PDEVICE_OBJECT	ControlDeviceObject
	)
{
	PLFS			lfs = &GlobalLfs;
    USHORT			maxDepth = 16;
	PVOID			DGSvrCtx = NULL;
	PVOID			NtcCtx = NULL;
	UNICODE_STRING	tempUnicode;


#if DBG
	
	ASSERT( sizeof(UCHAR)	== 1 );
	ASSERT( sizeof(USHORT)	== 2 );
	ASSERT( sizeof(ULONG)	== 4 );
	ASSERT( sizeof(ULONGLONG)== 8 );

	DbgPrint( "KdDebuggerEnabled = %p *KdDebuggerEnabled = %d\n", KdDebuggerEnabled, *KdDebuggerEnabled );
	
	//*KdDebuggerNotPresent = TRUE;
	//*KdDebuggerEnabled = FALSE;

	DbgPrint( "sizeof(L""\\$Extend"") = %d\n", sizeof(L"\\$Extend") );

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

	gFileSpyDebugLevel |= LFS_DEBUG_LFS_TRACE;
	//gFileSpyDebugLevel |= LFS_DEBUG_NETDISK_MANAGER_TRACE;
	gFileSpyDebugLevel |= LFS_DEBUG_LFS_TRACE;
	gFileSpyDebugLevel |= LFS_DEBUG_PRIMARY_INFO;
	gFileSpyDebugLevel |= LFS_DEBUG_LFS_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_SECONDARY_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_PRIMARY_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_SECONDARY_ERROR;
	gFileSpyDebugLevel |= LFS_DEBUG_SECONDARY_TRACE;

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
	gFileSpyDebugLevel |= LFS_DEBUG_LIB_INFO;
	gFileSpyDebugLevel |= LFS_DEBUG_PRIMARY_INFO;


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

#if (__NDAS_FS_MINI__ && __NDAS_FS_MINI_MODE__)

	lfs->NdasFsMiniMode = TRUE;

#else
	
	lfs->NdasFsMiniMode = FALSE;

#endif

	if (IS_WINDOWSXP_OR_LATER()) {

#if __NDAS_FS_NTFS_RW__

		lfs->NdasNtfsRwSupport	= TRUE;

#if __NDAS_FS_NTFS_RW_INDIRECT__

		lfs->NdasNtfsRwIndirect	= TRUE;

#endif

#endif

#if __NDAS_FS_NTFS_RO__

		lfs->NdasNtfsRoSupport	= TRUE;

#endif

	}

#if __NDAS_FS_FAT_RW__

	lfs->NdasFatRwSupport	= TRUE;

#if __NDAS_FS_FAT_RW_INDIRECT__
	lfs->NdasFatRwIndirect	= TRUE;
#endif

#endif

#if __NDAS_FS_FAT_RO__

	lfs->NdasFatRoSupport	= TRUE;

#endif

	lfs->ShutdownOccured = FALSE;


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

	RtlInitEmptyUnicodeString( &lfs->MountMgrRemoteDatabase, 
							   lfs->MountMgrRemoteDatabaseBuffer,
							   sizeof(lfs->MountMgrRemoteDatabaseBuffer) );

	RtlInitUnicodeString( &tempUnicode, L"\\:$MountMgrRemoteDatabase" );
	RtlCopyUnicodeString( &lfs->MountMgrRemoteDatabase, &tempUnicode );

	RtlInitEmptyUnicodeString( &lfs->ExtendReparse, 
							   lfs->ExtendReparseBuffer,
							   sizeof(lfs->ExtendReparseBuffer) );

	RtlInitUnicodeString( &tempUnicode, L"\\$Extend\\$Reparse:$R:$INDEX_ALLOCATION" );
	RtlCopyUnicodeString( &lfs->ExtendReparse, &tempUnicode );

	RtlInitEmptyUnicodeString( &lfs->MountPointManagerRemoteDatabase, 
							   lfs->MountPointManagerRemoteDatabaseBuffer,
							   sizeof(lfs->MountPointManagerRemoteDatabaseBuffer) );

	RtlInitUnicodeString( &tempUnicode, L"\\System Volume Information\\MountPointManagerRemoteDatabase" );
	RtlCopyUnicodeString( &lfs->MountPointManagerRemoteDatabase, &tempUnicode );


	lfs->LfsTable = LfsTable_Create(lfs);

	if (!lfs->LfsTable) {

		NDAS_BUGON( NDAS_BUGON_INSUFFICIENT_RESOURCES );
		LfsDriverUnload( DriverObject );

		return STATUS_INSUFFICIENT_RESOURCES;	
	}

	lfs->NetdiskManager = NetdiskManager_Create(lfs);

	if (!lfs->NetdiskManager) {

		NDAS_BUGON( NDAS_BUGON_INSUFFICIENT_RESOURCES );
		LfsDriverUnload( DriverObject );

		return STATUS_INSUFFICIENT_RESOURCES;	
	}

	lfs->Primary = Primary_Create(lfs);
	
	if (!lfs->Primary) {

		NDAS_BUGON( NDAS_BUGON_INSUFFICIENT_RESOURCES );
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

#if __NDAS_FS__

	RtlInitUnicodeString( &tempUnicode, L"FsRtlTeardownPerStreamContexts" );
	LfsFsRtlTeardownPerStreamContexts = MmGetSystemRoutineAddress( &tempUnicode );

	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("LfsFsRtlTeardownPerStreamContexts = %x\n", LfsFsRtlTeardownPerStreamContexts) );

#endif

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

	if (GlobalLfs.Primary)
		Primary_Close( GlobalLfs.Primary );

	GlobalLfs.Primary = NULL;

	if (GlobalLfs.NetdiskManager)
		NetdiskManager_Close( GlobalLfs.NetdiskManager );

	GlobalLfs.NetdiskManager = NULL;

	if (GlobalLfs.LfsTable) {

		RdsvrDatagramDestroy();
		LfsTable_Close( GlobalLfs.LfsTable );
	}

	GlobalLfs.LfsTable = NULL;

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

			ObDereferenceObject( LfsDeviceExt->FileSpyDeviceObject );
		}

#if __NDAS_FS_MINI__
		if (LfsDeviceExt->InstanceContext) {

			FltReleaseContext( LfsDeviceExt->InstanceContext );
		}
#endif

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
					   ("LfsDevExtDereference: Lfs Device Extension is Freed LfsDeviceExt = %p\n", 
						 LfsDeviceExt) );

		LfsDereference( &GlobalLfs );
	}
}


//
//	Query NDAS usage status by going through all lfs filter extensions.
//

NTSTATUS
LfsDeviceExt_QueryNDASUsage (
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

				if (((FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_FLAG_INDIRECT) && lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDAS_FAT) ||
					 lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_FAT) &&
					lfsDeviceExt->SecondaryState == CONNECTED_TO_LOCAL_STATE) {

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
			Secondary_LookUpCcb(lfsDeviceExt->Secondary, FileObject) != NULL) {

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
	lfsDeviceExt->AttachedToDeviceObject	= ((PFILESPY_DEVICE_EXTENSION)(FileSpyDeviceObject->DeviceExtension))->NLExtHeader.AttachedToDeviceObject;

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

			lfsDeviceExt->FileSystemType = LFS_FILE_SYSTEM_NTFS;
	
			ClearFlag( lfsDeviceExt->Flags, LFS_DEVICE_FLAG_INITIALIZING );
			SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_FLAG_MOUNTED );

			break;
		} 

		RtlInitUnicodeString( &fsName, L"\\Fat" );

		result = RtlEqualUnicodeString( &lfsDeviceExt->FileSystemVolumeName,
										&fsName,
										TRUE );

		if (result == 1) {

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsAttachToFileSystemDevice: Name compare %d %wZ %wZ\n",
											     result, &lfsDeviceExt->FileSystemVolumeName, &fsName) );

			lfsDeviceExt->FileSystemType = LFS_FILE_SYSTEM_FAT;

			ClearFlag( lfsDeviceExt->Flags, LFS_DEVICE_FLAG_INITIALIZING );
			SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_FLAG_MOUNTED );

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

			ClearFlag( lfsDeviceExt->Flags, LFS_DEVICE_FLAG_INITIALIZING );

			if (status == STATUS_SUCCESS) {

				SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_FLAG_MOUNTED );
				SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_FLAG_REGISTERED );			
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

			ClearFlag( lfsDeviceExt->Flags, LFS_DEVICE_FLAG_INITIALIZING );

			if (status == STATUS_SUCCESS) {

				SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_FLAG_MOUNTED );
				SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_FLAG_REGISTERED );			
			}

			break;
		} 
		
		lfsDeviceExt->FileSystemType = LFS_FILE_SYSTEM_OTHER;
		ClearFlag( lfsDeviceExt->Flags, LFS_DEVICE_FLAG_INITIALIZING );
		SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_FLAG_MOUNTED );
	
	} while (0);
		
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

	if (FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_FLAG_REGISTERED)) {

		UnRegisterNdfsCallback( lfsDeviceExt->BaseVolumeDeviceObject );
		ClearFlag( lfsDeviceExt->Flags, LFS_DEVICE_FLAG_REGISTERED );
	}

	ASSERT( lfsDeviceExt->ReferenceCount != 0 );
	SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_FLAG_DISMOUNTED );
		
	return;
}


VOID
LfsCleanupMountedDevice (
    IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
    )
{
	PLFS_DEVICE_EXTENSION	lfsDeviceExt = LfsDeviceExt;

	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, 
				   ("LfsCleanupMountedDevice: Entered lfsDeviceExt = %p, LfsDeviceExt->ReferenceCount = %d\n", 
					lfsDeviceExt, LfsDeviceExt->ReferenceCount) );

	//NDAS_BUGON( LfsDeviceExt->ReferenceCount == 1 );

#if !(__NDAS_FS_MINI__ && __NDAS_FS_MINI_MODE__)

	NDAS_BUGON( LfsDeviceExt->FileSpyDeviceObject );

#else

	NDAS_BUGON( LfsDeviceExt->InstanceContext || LfsDeviceExt->FileSpyDeviceObject );

#endif

#if __NDAS_FS_FLUSH_VOLUME__

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

	switch (lfsDeviceExt->FilteringMode) {

	case LFS_READONLY:

		break;

	case LFS_PRIMARY:
	case LFS_SECONDARY_TO_PRIMARY: {

		break;
	}

	case LFS_SECONDARY: {

		break;
	}
	
	case LFS_FILE_CONTROL: {
	
		PFAST_IO_DISPATCH	fastIoDispatch;

		if (FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_FLAG_REGISTERED)) {

			UnRegisterNdfsCallback( lfsDeviceExt->BaseVolumeDeviceObject );
			ClearFlag( lfsDeviceExt->Flags, LFS_DEVICE_FLAG_REGISTERED );
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

		NDAS_BUGON( LFS_BUG );
		break;
	}

	NDAS_BUGON( !FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_FLAG_CLNEANUPED) );
	SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_FLAG_CLNEANUPED );

	LfsDeviceExt_Dereference( lfsDeviceExt );

	return;
}

VOID
LfsDismountVolume (
    IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
    )
{
	PLFS_DEVICE_EXTENSION	lfsDeviceExt = LfsDeviceExt;


	ASSERT( LfsDeviceExt );

	if (FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_FLAG_DISMOUNTED)) {

		NDAS_BUGON( FALSE );
		return;
	}

	if (FlagOn(lfsDeviceExt->Flags, LFS_DEVICE_FLAG_DISMOUNTING)) {

		NDAS_BUGON( FALSE );
		return;
	}

	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("LfsDismountVolume: Entered lfsDeviceExt = %p\n", lfsDeviceExt) );

	ASSERT( lfsDeviceExt->ReferenceCount );

	ExAcquireFastMutex( &lfsDeviceExt->FastMutex );

	SetFlag( lfsDeviceExt->Flags, LFS_DEVICE_FLAG_DISMOUNTED );

	if (lfsDeviceExt->NetdiskPartition) {

		PNETDISK_PARTITION	netdiskPartition2;

		netdiskPartition2 = lfsDeviceExt->NetdiskPartition;
		lfsDeviceExt->NetdiskPartition = NULL;

		ExReleaseFastMutex( &lfsDeviceExt->FastMutex );

		NetdiskManager_DisMountVolume( GlobalLfs.NetdiskManager,
									   netdiskPartition2,
									   lfsDeviceExt->NetdiskEnabledMode );
	
	} else {

		ExReleaseFastMutex( &lfsDeviceExt->FastMutex );
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
	
	default:

		NDAS_BUGON( LFS_BUG );
		break;
	}

	return;
}


BOOLEAN
LfsSecondaryPassThrough (
    IN  PDEVICE_OBJECT				DeviceObject,
    IN  PIRP						Irp,
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	OUT PNTSTATUS					NtStatus
	)
{
	NTSTATUS			status = STATUS_SUCCESS;
	BOOLEAN				result = FALSE;
    PIO_STACK_LOCATION	irpSp = IoGetCurrentIrpStackLocation( Irp );
	PFILE_OBJECT		fileObject = irpSp->FileObject;
	BOOLEAN				fastMutexAcquired = FALSE;

	UNREFERENCED_PARAMETER( DeviceObject );

	ASSERT( KeGetCurrentIrql() <= APC_LEVEL );

	PrintIrp( LFS_DEBUG_SECONDARY_TRACE, "LfsSecondaryPassThrough", &DevExt->LfsDeviceExt, Irp );

	LfsDeviceExt_Reference( &DevExt->LfsDeviceExt );	

	InterlockedIncrement( &DevExt->LfsDeviceExt.ProcessingIrpCount );

	ExAcquireFastMutex( &DevExt->LfsDeviceExt.FastMutex );
	fastMutexAcquired = TRUE;

	NDAS_BUGON( !FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_CLNEANUPED) );
	
	try {

		if (irpSp->MajorFunction == IRP_MJ_WRITE) {

			if (DevExt->LfsDeviceExt.Secondary == NULL ||
				Secondary_LookUpCcb(DevExt->LfsDeviceExt.Secondary, fileObject) == NULL) {

				SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsPassThrough Fake Write %wZ\n", &fileObject->FileName) );

				status = Irp->IoStatus.Status = STATUS_SUCCESS;
				Irp->IoStatus.Information = irpSp->Parameters.Write.Length;
				IoCompleteRequest( Irp, IO_DISK_INCREMENT );

				result = TRUE;
				leave;
			}
		}

		if (irpSp->MajorFunction == IRP_MJ_CREATE) {

			//UNICODE_STRING	Extend = { 16, 16, L"\\$Extend" };
			//UNICODE_STRING	tempUnicode;

			if (IsMetaFile(&fileObject->FileName)) {

				result = FALSE;
				leave;
			}

#if 0
			if (fileObject->FileName.Length >= 16) {

				tempUnicode = fileObject->FileName;
				tempUnicode.Length = 16;

				if (RtlEqualUnicodeString( &Extend, &tempUnicode, TRUE)) {

					SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, 
								  ("LfsPassThrough: %wZ returned with aaa STATUS_OBJECT_NAME_NOT_FOUND\n", &fileObject->FileName) );

					NDAS_BUGON( FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTED) );

					if (FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTED)) {

						status = Irp->IoStatus.Status = STATUS_OBJECT_NAME_NOT_FOUND;
						Irp->IoStatus.Information = 0;
						IoCompleteRequest(Irp, IO_DISK_INCREMENT);

						result = TRUE;
				
					} else {

						result = FALSE;
					}

					leave;
				}
			}
#endif

			if (RtlEqualUnicodeString(&GlobalLfs.MountMgrRemoteDatabase, &fileObject->FileName, TRUE)	|| 
				RtlEqualUnicodeString(&GlobalLfs.ExtendReparse, &fileObject->FileName, TRUE)			||
				RtlEqualUnicodeString(&GlobalLfs.MountPointManagerRemoteDatabase, &fileObject->FileName, TRUE)) {

				SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, 
							  ("LfsPassThrough: %wZ returned with STATUS_OBJECT_NAME_NOT_FOUND\n", &fileObject->FileName) );

				NDAS_BUGON( FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTED) );

				if (FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTED)) {

					status = Irp->IoStatus.Status = STATUS_OBJECT_NAME_NOT_FOUND;
					Irp->IoStatus.Information = 0;
					IoCompleteRequest(Irp, IO_DISK_INCREMENT);

					result = TRUE;
				
				} else {

					result = FALSE;
				}

				leave;
			}

			if ((fileObject->FileName.Length == (sizeof(REMOUNT_VOLUME_FILE_NAME)-sizeof(WCHAR)) && 
				 RtlEqualMemory(fileObject->FileName.Buffer, REMOUNT_VOLUME_FILE_NAME, fileObject->FileName.Length)) ||
				(fileObject->FileName.Length == (sizeof(TOUCH_VOLUME_FILE_NAME)-sizeof(WCHAR)) && 
				 RtlEqualMemory(fileObject->FileName.Buffer, TOUCH_VOLUME_FILE_NAME, fileObject->FileName.Length))) {

				PrintIrp( LFS_DEBUG_LFS_TRACE, "TOUCH/REMOUNT_VOLUME_FILE_NAME", &DevExt->LfsDeviceExt, Irp );

				status = Irp->IoStatus.Status = STATUS_OBJECT_NAME_NOT_FOUND;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest( Irp, IO_DISK_INCREMENT );

				result = TRUE;
				leave;
			}

			PrintIrp( LFS_DEBUG_LFS_NOISE, __FUNCTION__, &DevExt->LfsDeviceExt, Irp );
		}

		if (!FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTED)) {

			result = FALSE;
			leave;
		}

		if (FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED)) {

			result = FALSE;
			leave;
		}

		if (FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING)) {

			NDAS_BUGON( DevExt->LfsDeviceExt.Secondary == NULL || 
						   Secondary_LookUpCcb(DevExt->LfsDeviceExt.Secondary, fileObject) == NULL );

			if (irpSp->MajorFunction == IRP_MJ_CREATE) {

				status = Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest( Irp, IO_DISK_INCREMENT );

				result = TRUE;
				leave;							
			
			} else if (irpSp->MajorFunction == IRP_MJ_PNP) {

			} else {

				result = FALSE;
				leave;
			}
		}

		NDAS_BUGON( FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTED) && !FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED) );

		if (FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_SURPRISE_REMOVED)) {

			if (Secondary_LookUpCcb(DevExt->LfsDeviceExt.Secondary, fileObject)) {

				if (irpSp->MajorFunction == IRP_MJ_CLOSE) {

					SecondaryFileObjectClose( DevExt->LfsDeviceExt.Secondary, fileObject );
		
					status = Irp->IoStatus.Status = STATUS_SUCCESS;
					Irp->IoStatus.Information = 0;
					IoCompleteRequest( Irp, IO_DISK_INCREMENT );

					result = TRUE;
					leave;				
				} 
				
				if (irpSp->MajorFunction == IRP_MJ_PNP) {

					result = FALSE;
					leave;

				} 

				if (irpSp->MajorFunction == IRP_MJ_CLEANUP) {

					InterlockedDecrement( &((PLFS_FCB)irpSp->FileObject->FsContext)->UncleanCount );
					SetFlag( fileObject->Flags, FO_CLEANUP_COMPLETE );

					status = Irp->IoStatus.Status = STATUS_SUCCESS;
				
				} else {
				
					status = Irp->IoStatus.Status = STATUS_DEVICE_REMOVED;
				}

				Irp->IoStatus.Information = 0;
				IoCompleteRequest(Irp, IO_DISK_INCREMENT);

				result = TRUE;
				leave;
			}

			result = FALSE;
			leave;
		}

		if (irpSp->MajorFunction == IRP_MJ_PNP) {

			switch (irpSp->MinorFunction) {

			case IRP_MN_SURPRISE_REMOVAL: {
				
				PNETDISK_PARTITION	netdiskPartition2;

				NDAS_BUGON( DevExt->LfsDeviceExt.NetdiskPartition );
				
				netdiskPartition2 = DevExt->LfsDeviceExt.NetdiskPartition;
				DevExt->LfsDeviceExt.NetdiskPartition = NULL;

				SetFlag( DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_SURPRISE_REMOVED );

				ExReleaseFastMutex( &DevExt->LfsDeviceExt.FastMutex );
				fastMutexAcquired = FALSE;

				NetdiskManager_SurpriseRemoval( GlobalLfs.NetdiskManager,
												netdiskPartition2,
												DevExt->LfsDeviceExt.NetdiskEnabledMode );

				result = FALSE;
				leave;
			} 
		
			case IRP_MN_QUERY_REMOVE_DEVICE: {
							
				KEVENT				waitEvent;

				if (DevExt->LfsDeviceExt.ProcessingIrpCount != 1) {

					status = Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
					Irp->IoStatus.Information = 0;
					IoCompleteRequest( Irp, IO_DISK_INCREMENT );

					result = TRUE;
					leave;
				}

				if (DevExt->LfsDeviceExt.SecondaryState == VOLUME_PURGE_STATE) {
					
					status = Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
					Irp->IoStatus.Information = 0;
					IoCompleteRequest( Irp, IO_DISK_INCREMENT );

					result = TRUE;
					leave;
				}
			
				if (FlagOn(DevExt->LfsDeviceExt.Secondary->Flags, SECONDARY_FLAG_RECONNECTING)) {  // While Disabling and disconnected
	
					status = Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
					Irp->IoStatus.Information = 0;
					IoCompleteRequest( Irp, IO_DISK_INCREMENT );
	
					result = TRUE;
					leave;
				}

				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("Secondary_PassThrough: IRP_MN_QUERY_REMOVE_DEVICE Entered\n") );

				ExReleaseFastMutex( &DevExt->LfsDeviceExt.FastMutex );
				fastMutexAcquired = FALSE;

				Secondary_TryCloseCcbs( DevExt->LfsDeviceExt.Secondary );
			
				if (!IsListEmpty(&DevExt->LfsDeviceExt.Secondary->FcbQueue)) {

					LARGE_INTEGER interval;
				
					// Wait all files closed
					interval.QuadPart = (1 * DELAY_ONE_SECOND);      //delay 1 seconds
					KeDelayExecutionThread( KernelMode, FALSE, &interval );
				}
						
				ExAcquireFastMutex( &DevExt->LfsDeviceExt.FastMutex );
				fastMutexAcquired = TRUE;

				NDAS_BUGON( DevExt->LfsDeviceExt.AttachedToDeviceObject );

				if (!IsListEmpty(&DevExt->LfsDeviceExt.Secondary->FcbQueue)) {

					status = Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
					Irp->IoStatus.Information = 0;
					IoCompleteRequest( Irp, IO_DISK_INCREMENT );
				
					result = TRUE;
					leave;
				} 

				if (DevExt->LfsDeviceExt.ProcessingIrpCount != 1) {

					status = Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
					Irp->IoStatus.Information = 0;
					IoCompleteRequest( Irp, IO_DISK_INCREMENT );

					result = TRUE;
					leave;
				}

				SetFlag( DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );

				ExReleaseFastMutex( &DevExt->LfsDeviceExt.FastMutex );
				fastMutexAcquired = FALSE;

				IoCopyCurrentIrpStackLocationToNext( Irp );
				KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

				IoSetCompletionRoutine( Irp,
										LfsPassThroughCompletion,
										&waitEvent,
										TRUE,
										TRUE,
										TRUE );

				status = IoCallDriver( DevExt->LfsDeviceExt.AttachedToDeviceObject, Irp );

				if (status == STATUS_PENDING) {

					status = KeWaitForSingleObject( &waitEvent, Executive, KernelMode, FALSE, NULL );
					ASSERT( status == STATUS_SUCCESS );
				}

				ASSERT( KeReadStateEvent(&waitEvent) || !NT_SUCCESS(Irp->IoStatus.Status) );
	
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO, 
							   ("%s: lfsDeviceExt = %p, IRP_MN_QUERY_REMOVE_DEVICE Irp->IoStatus.Status = %x\n",
								__FUNCTION__, &DevExt->LfsDeviceExt, Irp->IoStatus.Status) );

				if (NT_SUCCESS(Irp->IoStatus.Status)) {

#if 1
					if (IS_VISTA_OR_LATER() && DevExt->LfsDeviceExt.FileSystemType == LFS_FILE_SYSTEM_NTFS) {

						NetdiskManager_QueryRemoveMountVolume( GlobalLfs.NetdiskManager,
															   DevExt->LfsDeviceExt.NetdiskPartition,
															   DevExt->LfsDeviceExt.NetdiskEnabledMode );

						SetFlag( DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_QUERY_REMOVE );

					} else {
#else
					{
#endif

						ASSERT( !FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED) );

						ClearFlag( DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );
						LfsDismountVolume( &DevExt->LfsDeviceExt );
					}			
				
				} else { 

					ExAcquireFastMutex( &DevExt->LfsDeviceExt.FastMutex );
					fastMutexAcquired = TRUE;

					ClearFlag( DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );
					NDAS_BUGON( DevExt->LfsDeviceExt.ProcessingIrpCount == 1 );

					ExReleaseFastMutex( &DevExt->LfsDeviceExt.FastMutex );
					fastMutexAcquired = FALSE;
				}

				status = Irp->IoStatus.Status;
				IoCompleteRequest( Irp, IO_NO_INCREMENT );

				result = TRUE;
				leave;
			}

			case IRP_MN_REMOVE_DEVICE:
			case IRP_MN_CANCEL_REMOVE_DEVICE: {

				KEVENT				waitEvent;

				if (!FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_QUERY_REMOVE)) {

					NDAS_BUGON( irpSp->MinorFunction == IRP_MN_CANCEL_REMOVE_DEVICE );

					result = FALSE;
					leave;
				}
				
				ClearFlag( DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_QUERY_REMOVE );

				if (irpSp->MinorFunction == IRP_MN_CANCEL_REMOVE_DEVICE) {

					NDAS_BUGON( FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING) );
				}

				ExReleaseFastMutex( &DevExt->LfsDeviceExt.FastMutex );
				fastMutexAcquired = FALSE;

				if (irpSp->MinorFunction == IRP_MN_CANCEL_REMOVE_DEVICE) {

					SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO, 
								   ("%s: lfsDeviceExt = %p, IRP_MN_CANCEL_REMOVE_DEVICE Irp->IoStatus.Status = %x\n",
									__FUNCTION__, &DevExt->LfsDeviceExt, Irp->IoStatus.Status) );

					NetdiskManager_CancelRemoveMountVolume( GlobalLfs.NetdiskManager,
														   DevExt->LfsDeviceExt.NetdiskPartition,
														   DevExt->LfsDeviceExt.NetdiskEnabledMode );

					ClearFlag( DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );

				} else {

					SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO, 
								   ("%s: lfsDeviceExt = %p, IRP_MN_REMOVE_DEVICE Irp->IoStatus.Status = %x\n",
									__FUNCTION__, &DevExt->LfsDeviceExt, Irp->IoStatus.Status) );

					ASSERT( !FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED) );

					ClearFlag( DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );
					LfsDismountVolume( &DevExt->LfsDeviceExt );
				}

				IoCopyCurrentIrpStackLocationToNext( Irp );
				KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

				IoSetCompletionRoutine( Irp,
										LfsPassThroughCompletion,
										&waitEvent,
										TRUE,
										TRUE,
										TRUE );

				status = IoCallDriver( DevExt->LfsDeviceExt.AttachedToDeviceObject, Irp );

				if (status == STATUS_PENDING) {

					status = KeWaitForSingleObject( &waitEvent, Executive, KernelMode, FALSE, NULL );
					ASSERT( status == STATUS_SUCCESS );
				}

				ASSERT( KeReadStateEvent(&waitEvent) || !NT_SUCCESS(Irp->IoStatus.Status) );
		
				NDAS_BUGON( NT_SUCCESS(Irp->IoStatus.Status) );

				status = Irp->IoStatus.Status;
				IoCompleteRequest( Irp, IO_NO_INCREMENT );

				result = TRUE;
				leave;
			}

			default:

				result = FALSE;
				leave;
			}
		}
		
		if (irpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) {

			if (irpSp->MinorFunction == IRP_MN_USER_FS_REQUEST || irpSp->MinorFunction == IRP_MN_KERNEL_CALL) {

				switch (irpSp->Parameters.FileSystemControl.FsControlCode) {

				case FSCTL_LOCK_VOLUME: 
					
					if (DevExt->LfsDeviceExt.SecondaryState == VOLUME_PURGE_STATE) {

						result = FALSE;
						leave;
					}

					SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
								   ("LfsPassThrough, IRP_MJ_FILE_SYSTEM_CONTROL: Return failure of acquiring the volume exclusively.\n") );

					NDAS_BUGON( fileObject && fileObject->FileName.Length == 0 && fileObject->RelatedFileObject == NULL );

					status = Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
					Irp->IoStatus.Information = 0;
					IoCompleteRequest( Irp, IO_DISK_INCREMENT );

					result = TRUE;
					leave;				
									
				case FSCTL_DISMOUNT_VOLUME: {

					KEVENT				waitEvent;

					ASSERT( IS_WINDOWSVISTA_OR_LATER() );

					ExReleaseFastMutex( &DevExt->LfsDeviceExt.FastMutex );
					fastMutexAcquired = FALSE;

					Secondary_TryCloseCcbs( DevExt->LfsDeviceExt.Secondary );
		
					if (!IsListEmpty(&DevExt->LfsDeviceExt.Secondary->FcbQueue)) {

						LARGE_INTEGER interval;
				
						// Wait all files closed
						interval.QuadPart = (2 * DELAY_ONE_SECOND);      //delay 1 seconds
						KeDelayExecutionThread( KernelMode, FALSE, &interval );
					}
			
					if (!IsListEmpty(&DevExt->LfsDeviceExt.Secondary->FcbQueue)) {

						ASSERT( FALSE );

						status = Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
						Irp->IoStatus.Information = 0;
						IoCompleteRequest( Irp, IO_DISK_INCREMENT );
				
						result = TRUE;
						leave;
					} 

					SetFlag( DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );

					IoCopyCurrentIrpStackLocationToNext( Irp );
					KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

					IoSetCompletionRoutine( Irp,
											LfsPassThroughCompletion,
											&waitEvent,
											TRUE,
											TRUE,
											TRUE );

					status = IoCallDriver( DevExt->LfsDeviceExt.AttachedToDeviceObject, Irp );

					if (status == STATUS_PENDING) {

						status = KeWaitForSingleObject( &waitEvent, Executive, KernelMode, FALSE, NULL );
						ASSERT( status == STATUS_SUCCESS );
					}

					ASSERT( KeReadStateEvent(&waitEvent) || !NT_SUCCESS(Irp->IoStatus.Status) );

					ClearFlag( DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );
	
					if (NT_SUCCESS(Irp->IoStatus.Status)) {

						ASSERT( !FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED) );
						LfsDismountVolume( &DevExt->LfsDeviceExt );
					}

					status = Irp->IoStatus.Status;
					IoCompleteRequest( Irp, IO_NO_INCREMENT );

					SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, 
								   ("%s: FSCTL_DISMOUNT_VOLUME Irp->IoStatus.Status = %x\n",
									__FUNCTION__, Irp->IoStatus.Status) );

					result = TRUE;
					leave;	
				}

				default:

					break;
				}
			}
		}

		if (irpSp->MajorFunction != IRP_MJ_CREATE && 
			Secondary_LookUpCcb(DevExt->LfsDeviceExt.Secondary, fileObject) == NULL) {

			result = FALSE;
			leave;
		} 

		if (DevExt->LfsDeviceExt.SecondaryState == WAIT_PURGE_SAFE_STATE) {

			NDAS_BUGON( irpSp->MajorFunction == IRP_MJ_CREATE );

			if ((fileObject->FileName.Length == (sizeof(WAKEUP_VOLUME_FILE_NAME)-sizeof(WCHAR)) && 
				 RtlEqualMemory(fileObject->FileName.Buffer, WAKEUP_VOLUME_FILE_NAME, fileObject->FileName.Length)) || 
				(fileObject->FileName.Length == (sizeof(TOUCH_VOLUME_FILE_NAME)-sizeof(WCHAR)) && 
				 RtlEqualMemory(fileObject->FileName.Buffer, TOUCH_VOLUME_FILE_NAME, fileObject->FileName.Length))) {

				 PrintIrp( LFS_DEBUG_LFS_INFO, "LfsPassThrough WAIT_PURGE_SAFE_STATE", &DevExt->LfsDeviceExt, Irp );

				 status = Irp->IoStatus.Status = STATUS_OBJECT_NAME_NOT_FOUND;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest( Irp, IO_DISK_INCREMENT );

				result = TRUE;
				leave;	
			}		

			PrintIrp( LFS_DEBUG_LFS_INFO, "LfsPassThrough WAIT_PURGE_SAFE_STATE", &DevExt->LfsDeviceExt, Irp );

			if (Irp->RequestorMode == UserMode && fileObject && fileObject->FileName.Length) {

				DevExt->LfsDeviceExt.SecondaryState = SECONDARY_STATE;
			
			} else {

				result = FALSE;
				leave;
			}
		}

		if (DevExt->LfsDeviceExt.SecondaryState == VOLUME_PURGE_STATE) {

			if (irpSp->MajorFunction == IRP_MJ_CREATE) {

				if (fileObject->FileName.Length == (sizeof(WAKEUP_VOLUME_FILE_NAME)-sizeof(WCHAR)) && 
					RtlEqualMemory(fileObject->FileName.Buffer, WAKEUP_VOLUME_FILE_NAME, fileObject->FileName.Length)) {

					PrintIrp( LFS_DEBUG_LFS_TRACE, "WAKEUP_VOLUME_FILE_NAME", &DevExt->LfsDeviceExt, Irp );

					status = Irp->IoStatus.Status = STATUS_OBJECT_NAME_NOT_FOUND;
					Irp->IoStatus.Information = 0;
					IoCompleteRequest( Irp, IO_DISK_INCREMENT );

					result = TRUE;
					leave;
				}

				if (Irp->RequestorMode == KernelMode &&
					(fileObject->RelatedFileObject == NULL ||
					 Secondary_LookUpCcb(DevExt->LfsDeviceExt.Secondary, fileObject->RelatedFileObject) == NULL)) {

					PrintIrp( LFS_DEBUG_LFS_INFO, "LfsPassThrough VOLUME_PURGE_STATE", &DevExt->LfsDeviceExt, Irp );
					//ASSERT( fileObject->FileName.Length == 0 );

					result = FALSE;
					leave;
				}
			}
		}

		ExReleaseFastMutex( &DevExt->LfsDeviceExt.FastMutex );
		fastMutexAcquired = FALSE;

		NDAS_BUGON( fileObject );

		if ((FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_INDIRECT) && DevExt->LfsDeviceExt.FileSystemType == LFS_FILE_SYSTEM_NDAS_FAT) ||
			DevExt->LfsDeviceExt.FileSystemType == LFS_FILE_SYSTEM_FAT) {

			if (DevExt->LfsDeviceExt.SecondaryState == CONNECTED_TO_LOCAL_STATE) {

				if (fileObject != NULL &&
					Secondary_LookUpCcb(DevExt->LfsDeviceExt.Secondary, fileObject) == NULL && 
					(fileObject->RelatedFileObject == NULL ||
					Secondary_LookUpCcb(DevExt->LfsDeviceExt.Secondary, fileObject->RelatedFileObject) == NULL)) {

					result = FALSE;
					leave;
				} 
			}
		}

		result = SecondaryPassThrough( DevExt->LfsDeviceExt.Secondary, Irp, &status );

	} finally {

		InterlockedDecrement( &DevExt->LfsDeviceExt.ProcessingIrpCount );

		if (fastMutexAcquired)
			ExReleaseFastMutex( &DevExt->LfsDeviceExt.FastMutex );

		LfsDeviceExt_Dereference( &DevExt->LfsDeviceExt );
		*NtStatus = status;
	}

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



/*************************************************************************************************/

/* Only LfsDeviceExt_SecondaryToPrimary() can call SecondaryToPrimaryThreadProc() and PurgeVolume() */

NTSTATUS
PurgeVolume (
	IN PLFS_DEVICE_EXTENSION LfsDeviceExt
	);

/*************************************************************************************************/



NTSTATUS
LfsSecondaryToPrimary (
	IN PLFS_DEVICE_EXTENSION LfsDeviceExt
	)
{
	NTSTATUS	status;

	if (LfsDeviceExt->SecondaryState == CONNECTED_TO_LOCAL_STATE) {

		return STATUS_SUCCESS;
	}

	NDAS_BUGON( LfsDeviceExt->SecondaryState == SECONDARY_STATE );

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsSecondaryToPrimary: LfsDeviceExt = %p, LfsDeviceExt->SecondaryState = %d\n",
										 LfsDeviceExt, LfsDeviceExt->SecondaryState) );

	status = NetdiskManager_Secondary2Primary( GlobalLfs.NetdiskManager, 
											   LfsDeviceExt->NetdiskPartition, 
											   LfsDeviceExt->NetdiskEnabledMode );

	if (status != STATUS_SUCCESS) {
	
		return status;
	}

	SPY_LOG_PRINT(LFS_DEBUG_LFS_TRACE, ("LfsSecondaryToPrimary: LfsDeviceExt->SecondaryState = %d\n",
										LfsDeviceExt->SecondaryState) );

	LfsDeviceExt->SecondaryState = VOLUME_PURGE_STATE;

	status = PurgeVolume( LfsDeviceExt );		

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsSecondaryToPrimary: purgeStatus = %x, LfsDeviceExt->SecondaryState = %d\n",
										 status, LfsDeviceExt->SecondaryState) );

	if(status == STATUS_SUCCESS) {

		LfsDeviceExt->SecondaryState = CONNECTED_TO_LOCAL_STATE;
	
	} else {

		LfsDeviceExt->SecondaryState = SECONDARY_STATE;
	}

	return status;
}


NTSTATUS
PurgeVolume (
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

	if (/*(FlagOn(LfsDeviceExt->Flags, LFS_DEVICE_FLAG_INDIRECT) && LfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDAS_FAT) ||*/
		LfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_FAT) {

		NDAS_BUGON( FALSE );

#if 0

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
			//return STATUS_UNSUCCESSFUL;
		}

		NetdiskManager_ChangeMode( GlobalLfs.NetdiskManager,
								   LfsDeviceExt->NetdiskPartition,
								   &LfsDeviceExt->NetdiskEnabledMode );

#endif

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
				NDAS_BUGON( FALSE );
			
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

		if (FlagOn(LfsDeviceExt->Flags, LFS_DEVICE_FLAG_INDIRECT) && LfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDAS_FAT) {

			RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );
		
			status = ZwFsControlFile( fileHandle,
									  eventHandle,
									  NULL,
									  NULL,
									  &ioStatusBlock,
									  FSCTL_NDAS_FS_FLUSH_OR_PURGE,
									  NULL,
									  0,
									  NULL,
									  0 );

			if (status == STATUS_PENDING) {

				status = ZwWaitForSingleObject( eventHandle, TRUE, NULL );

				if (status != STATUS_SUCCESS)
					NDAS_BUGON( FALSE );
			
				status = ioStatusBlock.Status;
			}

			NDAS_BUGON( status == STATUS_SUCCESS );

			ZwClose( eventHandle );

			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
							("PurgeVolume: LfsDeviceExt = %p ZwFsControlFile FSCTL_NDAS_FS_FLUSH_OR_PURGE fileHandle =%p, status = %X, ioStatusBlock = %X\n",
							LfsDeviceExt, fileHandle, status, ioStatusBlock.Information) );

			ZwClose( fileHandle );

			NetdiskManager_ChangeMode( GlobalLfs.NetdiskManager,
									   LfsDeviceExt->NetdiskPartition,
									   &LfsDeviceExt->NetdiskEnabledMode );

			return status;
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
				NDAS_BUGON( FALSE );
			
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
				
				NDAS_BUGON( FALSE );
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
		
		if (fileNameBuffer == NULL) {

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

			//if (!FlagOn(LfsDeviceExt->Flags, LFS_DEVICE_FLAG_MOUNTED))
			//	NDAS_BUGON( FALSE );
		}

		status = STATUS_SUCCESS;

		ExFreePool( fileNameBuffer );

	} while(0);

	return status;
}


BOOLEAN
Lfs_IsLocalAddress (
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
RegisterNdfsCallback (
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
CallbackQueryPartitionInformation (
	IN  PDEVICE_OBJECT					RealDevice,
	OUT PNETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation
	)
{
	
	return NetdiskManager_QueryPartitionInformation( GlobalLfs.NetdiskManager,
													 RealDevice,
													 NetdiskPartitionInformation );
}


NTSTATUS
CallbackQueryPrimaryAddress ( 
	IN  PNETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation,
	OUT PLPX_ADDRESS					PrimaryAddress,
	IN  PBOOLEAN						IsLocalAddress	
	)
{
	NTSTATUS				status;
	NETDISK_PARTITION_INFO	netdiskPartitionInfo;
	
	RtlCopyMemory( &netdiskPartitionInfo.NetdiskAddress,
				   &NetdiskPartitionInformation->NetdiskInformation.NetdiskAddress,
				   sizeof(netdiskPartitionInfo.NetdiskAddress) );

	netdiskPartitionInfo.UnitDiskNo = NetdiskPartitionInformation->NetdiskInformation.UnitDiskNo;
	
	RtlCopyMemory( netdiskPartitionInfo.NdscId,
				   NetdiskPartitionInformation->NetdiskInformation.NdscId,
				   NDSC_ID_LENGTH );

	netdiskPartitionInfo.StartingOffset.QuadPart = 0;
	
	status = LfsTable_QueryPrimaryAddress( GlobalLfs.LfsTable,
										   &netdiskPartitionInfo,
										   PrimaryAddress );
	
	if (status == STATUS_SUCCESS && IsLocalAddress) 
		*IsLocalAddress = Lfs_IsLocalAddress( PrimaryAddress );
    
	return status;
}


NTSTATUS
CallbackSecondaryToPrimary (
	IN  PDEVICE_OBJECT	DiskDeviceObject,
	IN BOOLEAN			ModeChange
	)
{
	NTSTATUS			status;
	PNETDISK_PARTITION	netdiskPartition;

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, 
				   ("CallbackSecondaryToPrimary: Entered DiskDeviceObject = %p\n", DiskDeviceObject) );

	netdiskPartition = NetdiskManager_QueryNetdiskPartition( GlobalLfs.NetdiskManager, DiskDeviceObject );

	if (netdiskPartition == NULL) {

		//NDAS_BUGON( FALSE );
		return STATUS_NO_SUCH_DEVICE;
	}

	if (ModeChange == FALSE) {

		NDAS_BUGON( netdiskPartition->NetdiskVolume[NETDISK_SECONDARY].VolumeState == VolumePreMounting ||
					   netdiskPartition->NetdiskVolume[NETDISK_SECONDARY].VolumeState == VolumeSemiMounted );

		netdiskPartition->NetdiskVolume[NETDISK_SECONDARY].VolumeState = VolumeSemiMounted;
	}

	status = NetdiskManager_Secondary2Primary( GlobalLfs.NetdiskManager, 
											   netdiskPartition,
											   NETDISK_SECONDARY );

	NDAS_BUGON( status != STATUS_NO_SUCH_DEVICE );

	if (status == STATUS_SUCCESS) {

		if (ModeChange == TRUE) {

			NetdiskManager_ChangeMode( GlobalLfs.NetdiskManager,
									   netdiskPartition,
									   NULL );
		}
	}

	NetdiskPartition_Dereference( netdiskPartition );

	return status;
}


NTSTATUS
CallbackAddWriteRange (
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

		NDAS_BUGON( FALSE );
		return STATUS_UNSUCCESSFUL;
	}

	status = NetdiskManager_AddWriteRange( GlobalLfs.NetdiskManager, 
										   netdiskPartition,
										   BlockAceId );

	NetdiskPartition_Dereference( netdiskPartition );

	return status;
}



VOID
CallbackRemoveWriteRange (
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
CallbackGetNdasScsiBacl (
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


#if __NDAS_FS_FLUSH_VOLUME__

VOID
LfsDeviceExtThreadProc (
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

			if (GlobalLfs.ShutdownOccured || !(FlagOn(LfsDeviceExt->Flags, LFS_DEVICE_FLAG_MOUNTED) && !FlagOn(LfsDeviceExt->Flags, LFS_DEVICE_FLAG_DISMOUNTED)))
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
			
						timeOut.QuadPart = -NANO100_PER_SEC;
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
			
						timeOut.QuadPart = -3*NANO100_PER_SEC;
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
