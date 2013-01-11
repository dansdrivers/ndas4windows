#include "LfsProc.h"


VOID
NetdiskManagerThreadProc (
	IN 	PNETDISK_MANAGER	NetdiskManager
	);


PNETDISK_MANAGER_REQUEST
AllocNetdiskManagerRequest (
	IN	BOOLEAN	Synchronous
	); 


VOID
DereferenceNetdiskManagerRequest (
	IN PNETDISK_MANAGER_REQUEST	NetdiskManagerRequest
	); 


FORCEINLINE
VOID
QueueingNetdiskManagerRequest (
	IN PNETDISK_MANAGER			NetdiskManager,
	IN PNETDISK_MANAGER_REQUEST	NetdiskManagerRequest
	);


PENABLED_NETDISK
EnabledNetdisk_Create (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PDEVICE_OBJECT		VolumeDeviceObject,
	IN PDEVICE_OBJECT		DiskDeviceObject,
	IN PNETDISK_INFORMATION	NetdiskInformation
	);

BOOLEAN
EnabledNetdisk_Update (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PDEVICE_OBJECT		VolumeDeviceObject,
	IN PDEVICE_OBJECT		DiskDeviceObject,
	IN PNETDISK_INFORMATION	NetdiskInformation
	);

VOID
EnabledNetdisk_Close (
	IN PENABLED_NETDISK	EnabledNetdisk
	);

VOID
EnabledNetdisk_Reference (
	PENABLED_NETDISK	EnabledNetdisk
	);

VOID
EnabledNetdisk_Dereference (
	PENABLED_NETDISK	EnabledNetdisk
	);

VOID
EnabledNetdisk_FileSystemShutdown (
	IN PENABLED_NETDISK	EnabledNetdisk
	);

NTSTATUS
EnabledNetdisk_PreMountVolume (
	IN  PENABLED_NETDISK		EnabledNetdisk,
	IN  BOOLEAN					Indirect,
	IN  PDEVICE_OBJECT			VolumeDeviceObject,
	IN  PDEVICE_OBJECT			DiskDeviceObject,
	IN  PDEVICE_OBJECT			*ScsiportAdapterDeviceObject,
	OUT PNETDISK_PARTITION		*NetdiskPartition,
	OUT	PNETDISK_ENABLE_MODE	NetdiskEnableMode
	);

NTSTATUS
EnabledNetdisk_PostMountVolume (
	IN  PENABLED_NETDISK				EnabledNetdisk,
	IN	PNETDISK_PARTITION				NetdiskPartition,
	IN	NETDISK_ENABLE_MODE				NetdiskEnableMode,
	IN  BOOLEAN							Mount,
	IN	LFS_FILE_SYSTEM_TYPE			FileSystemType,
	IN  PLFS_DEVICE_EXTENSION			LfsDeviceExt,
	OUT	PNETDISK_PARTITION_INFORMATION	NetdiskPartionInformation
	);

VOID
EnabledNetdisk_MountVolumeComplete (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode,
	IN NTSTATUS				MountStatus,
	IN PDEVICE_OBJECT		AttachedToDeviceObject
	);

VOID
EnabledNetdisk_ChangeMode (
	IN		PENABLED_NETDISK		EnabledNetdisk,
	IN		PNETDISK_PARTITION		NetdiskPartition,
	IN OUT	PNETDISK_ENABLE_MODE	NetdiskEnableMode
	);

VOID
EnabledNetdisk_SurpriseRemoval (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	);

VOID
EnabledNetdisk_QueryRemoveMountVolume (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	);

VOID
EnabledNetdisk_CancelRemoveMountVolume (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	);

VOID
EnabledNetdisk_DisMountVolume (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	);

NTSTATUS
EnabledNetdisk_Secondary2Primary (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	);

NTSTATUS
EnabledNetdisk_UnplugNetdisk (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	);

BOOLEAN
EnabledNetdisk_ThisVolumeHasSecondary (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode,
	IN BOOLEAN				IncludeLocalSecondary
	);

NTSTATUS
WakeUpVolume (
	PNETDISK_PARTITION	NetdiskPartition
	);

NTSTATUS
EnabledNetdisk_GetPrimaryPartition (
	IN  PENABLED_NETDISK				EnabledNetdisk,
	IN 	PPRIMARY_SESSION				PrimarySession,
	IN  PLPX_ADDRESS					NetDiskAddress,
	IN  USHORT							UnitDiskNo,
	IN  PLARGE_INTEGER					StartingOffset,
	IN  BOOLEAN							LocalSecondary,
	OUT PNETDISK_PARTITION				*NetdiskPartition,
	OUT PNETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation,
	OUT PLFS_FILE_SYSTEM_TYPE			FileSystemType
	);

VOID
EnabledNetdisk_ReturnPrimaryPartition (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PPRIMARY_SESSION		PrimarySession,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN BOOLEAN				LocalSecondary
	);

NTSTATUS
EnabledNetdisk_TakeOver (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN PSESSION_INFORMATION	SessionInformation
	);

VOID
EnabledNetdisk_PrimarySessionStopping (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	);

VOID
EnabledNetdisk_PrimarySessionCancelStopping (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	);

VOID
EnabledNetdisk_PrimarySessionDisconnect (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	);

PENABLED_NETDISK
NetdiskManager_Lookup (
	IN PNETDISK_MANAGER	NetdiskManager,
	IN PLPX_ADDRESS		NetDiskAddress,
	IN USHORT			UnitDiskNo,
	IN PUCHAR			NdscId
	);

BOOLEAN
EnabledNetdisk_IsStoppedNetdisk (
	IN PENABLED_NETDISK	EnabledNetdisk
	);

BOOLEAN
EnabledNetdisk_IsStoppedPrimary (
	IN PENABLED_NETDISK	EnabledNetdisk
	);

PNETDISK_PARTITION
EnabledNetdisk_LookupNetdiskPartition (
	IN PENABLED_NETDISK				EnabledNetdisk,
    IN PPARTITION_INFORMATION_EX	PartitionInformationEx,
	IN PLARGE_INTEGER				StartingOffset,
	IN PDEVICE_OBJECT				DiskDeviceObject
	);

PNETDISK_PARTITION
NetdiskParttion_Allocate (
	IN PENABLED_NETDISK		EnabledNetdisk
	); 

VOID
NetdiskPartition_Reference (
	PNETDISK_PARTITION	NetdiskPartition
	);

VOID
NetdiskPartition_Dereference (
	PNETDISK_PARTITION	NetdiskPartition
	);



PNETDISK_MANAGER
NetdiskManager_Create (
	IN PLFS	Lfs
	)
{
	PNETDISK_MANAGER	netdiskManager;
 	OBJECT_ATTRIBUTES	objectAttributes;
	NTSTATUS			ntStatus;
	LARGE_INTEGER		timeOut;


	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("NetdiskManager_Create: Entered\n") );

	LfsReference( Lfs );

	netdiskManager = ExAllocatePoolWithTag( NonPagedPool, 
											sizeof(NETDISK_MANAGER),
											LFS_ALLOC_TAG );
	
	if (netdiskManager == NULL) {

		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
		LfsDereference( Lfs );

		return NULL;
	}

	RtlZeroMemory( netdiskManager, sizeof(NETDISK_MANAGER) );

	ExInitializeResourceLite( &netdiskManager->Resource );

	//KeInitializeSpinLock( &netdiskManager->SpinLock );
	ExInitializeFastMutex( &netdiskManager->FastMutex );

	netdiskManager->ReferenceCount	= 1;

	netdiskManager->Lfs = Lfs;

	KeInitializeSpinLock( &netdiskManager->EnabledNetdiskQSpinLock );
	InitializeListHead( &netdiskManager->EnabledNetdiskQueue );
	ExInitializeFastMutex( &netdiskManager->NDManFastMutex );

	netdiskManager->Thread.ThreadHandle = 0;
	netdiskManager->Thread.ThreadObject = NULL;

	netdiskManager->Thread.Flags = 0;

	KeInitializeEvent( &netdiskManager->Thread.ReadyEvent, NotificationEvent, FALSE );

	KeInitializeSpinLock( &netdiskManager->Thread.RequestQSpinLock );
	InitializeListHead( &netdiskManager->Thread.RequestQueue );
	KeInitializeEvent( &netdiskManager->Thread.RequestEvent, NotificationEvent, FALSE );

	InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

	ntStatus = PsCreateSystemThread( &netdiskManager->Thread.ThreadHandle,
									 THREAD_ALL_ACCESS,
									 &objectAttributes,
									 NULL,
									 NULL,
									 NetdiskManagerThreadProc,
									 netdiskManager );
	
	if (!NT_SUCCESS(ntStatus)) {

		ASSERT( LFS_UNEXPECTED );
		NetdiskManager_Close( netdiskManager );
		
		return NULL;
	}

	ntStatus = ObReferenceObjectByHandle( netdiskManager->Thread.ThreadHandle,
										  FILE_READ_DATA,
										  NULL,
										  KernelMode,
										  &netdiskManager->Thread.ThreadObject,
										  NULL );

	if (!NT_SUCCESS(ntStatus)) {

		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
		NetdiskManager_Close( netdiskManager );
		
		return NULL;
	}

	timeOut.QuadPart = -LFS_TIME_OUT;
	
	ntStatus = KeWaitForSingleObject( &netdiskManager->Thread.ReadyEvent,
									  Executive,
									  KernelMode,
									  FALSE,
									  &timeOut );

	ASSERT( ntStatus == STATUS_SUCCESS );

	KeClearEvent( &netdiskManager->Thread.ReadyEvent );

	if (ntStatus != STATUS_SUCCESS) {

		ASSERT( LFS_BUG );
		NetdiskManager_Close( netdiskManager );
		
		return NULL;
	}
	
	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("NetdiskManager_Create: netdiskManager = %p\n", netdiskManager) );

	return netdiskManager;
}


VOID
NetdiskManager_Close (
	IN PNETDISK_MANAGER	NetdiskManager
	)
{
	PLIST_ENTRY enabledNetdiskListEntry;


	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("NetdiskManager_Close: Entered NetdiskManager = %p\n", NetdiskManager) );

	while (enabledNetdiskListEntry = ExInterlockedRemoveHeadList(&NetdiskManager->EnabledNetdiskQueue,
																 &NetdiskManager->EnabledNetdiskQSpinLock)) {

		PENABLED_NETDISK	enabledNetdisk;
		
		enabledNetdisk = CONTAINING_RECORD(enabledNetdiskListEntry, ENABLED_NETDISK, ListEntry);
		InitializeListHead( &enabledNetdisk->ListEntry );
		EnabledNetdisk_Close( enabledNetdisk );
	}
	
	if (NetdiskManager->Thread.ThreadHandle == NULL) {

		ASSERT( LFS_BUG );
		NetdiskManager_Dereference( NetdiskManager );

		return;
	}

	ASSERT( NetdiskManager->Thread.ThreadObject != NULL );

	if (NetdiskManager->Thread.Flags & NETDISK_MANAGER_THREAD_TERMINATED) {

		ObDereferenceObject( NetdiskManager->Thread.ThreadObject );

		NetdiskManager->Thread.ThreadHandle = NULL;
		NetdiskManager->Thread.ThreadObject = NULL;

	} else {

		PNETDISK_MANAGER_REQUEST	netdiskManagerRequest;
		NTSTATUS					ntStatus;
		LARGE_INTEGER				timeOut;
	
		
		netdiskManagerRequest = AllocNetdiskManagerRequest( FALSE );
		
		if (netdiskManagerRequest == NULL) {

		    SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_ERROR,
						   ("NetdiskManager_Close: failed to allocate NetDisk manager request\n") );

			ASSERT( LFS_UNEXPECTED );
			NetdiskManager_Dereference( NetdiskManager );
			
			return;
		}

		netdiskManagerRequest->RequestType = NETDISK_MANAGER_REQUEST_DOWN;

		QueueingNetdiskManagerRequest( NetdiskManager, netdiskManagerRequest );

		timeOut.QuadPart = -LFS_TIME_OUT;

		ntStatus = KeWaitForSingleObject( NetdiskManager->Thread.ThreadObject,
										  Executive,
										  KernelMode,
										  FALSE,
										  &timeOut );

		ASSERT( ntStatus == STATUS_SUCCESS );

		if (ntStatus == STATUS_SUCCESS) {

		    SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
						   ("NetdiskManager_Close: thread stoped\n") );

			ObDereferenceObject( NetdiskManager->Thread.ThreadObject );

			NetdiskManager->Thread.ThreadHandle = NULL;
			NetdiskManager->Thread.ThreadObject = NULL;
		
		} else {

			ASSERT( LFS_BUG );
			return;
		}
	}

	NetdiskManager_Dereference( NetdiskManager );

	return;
}


VOID
NetdiskManager_Reference (
	IN PNETDISK_MANAGER	NetdiskManager
	)
{
    LONG result;
	
    result = InterlockedIncrement( &NetdiskManager->ReferenceCount );

    ASSERT( result >= 0 );
}


VOID
NetdiskManager_Dereference (
	IN PNETDISK_MANAGER	NetdiskManager
	)
{
    LONG result;

    result = InterlockedDecrement( &NetdiskManager->ReferenceCount );
    ASSERT( result >= 0 );

    if (result == 0) {

		ASSERT( NetdiskManager->EnabledNetdiskQueue.Flink == NetdiskManager->EnabledNetdiskQueue.Blink );

		ExDeleteResourceLite( &NetdiskManager->Resource );
		
		LfsDereference( NetdiskManager->Lfs );

		ExFreePoolWithTag( NetdiskManager, LFS_ALLOC_TAG );

		SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
					   ("NetdiskManager_Dereference: NetdiskManager is Freed NetdiskManager = %p\n", 
					    NetdiskManager) );
	}
}


VOID
NetdiskManagerThreadProc (
	IN 	PNETDISK_MANAGER	NetdiskManager
	)
{
	BOOLEAN	netdiskManagerThreadExit = FALSE;

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("NetdiskManagerThreadProc: Start\n") );

	NetdiskManager_Reference( NetdiskManager );

	SetFlag( NetdiskManager->Thread.Flags, NETDISK_MANAGER_THREAD_START );				
	KeSetEvent( &NetdiskManager->Thread.ReadyEvent, IO_DISK_INCREMENT, FALSE );


	while (netdiskManagerThreadExit == FALSE) {

		LARGE_INTEGER	timeOut ;
		NTSTATUS		waitStatus;		
		PLIST_ENTRY		netdiskManagerRequestEntry;


		timeOut.QuadPart = -LFS_TIME_OUT;

		waitStatus = KeWaitForSingleObject( &NetdiskManager->Thread.RequestEvent,
											Executive,
											KernelMode,
											FALSE,
											&timeOut );

		if (waitStatus == STATUS_TIMEOUT) {
		
			continue;
		}

		ASSERT( waitStatus == STATUS_SUCCESS );

		KeClearEvent( &NetdiskManager->Thread.RequestEvent );

		while (netdiskManagerThreadExit == FALSE && 
			   (netdiskManagerRequestEntry = ExInterlockedRemoveHeadList( &NetdiskManager->Thread.RequestQueue,
																		  &NetdiskManager->Thread.RequestQSpinLock))) {

			PNETDISK_MANAGER_REQUEST	netdiskManagerRequest;
			
			InitializeListHead( netdiskManagerRequestEntry );

			netdiskManagerRequest = CONTAINING_RECORD(netdiskManagerRequestEntry, NETDISK_MANAGER_REQUEST, ListEntry);

			if (netdiskManagerRequest->RequestType == NETDISK_MANAGER_REQUEST_DOWN) {

				netdiskManagerThreadExit = TRUE;
				ASSERT(IsListEmpty(&NetdiskManager->Thread.RequestQueue));
			
			} else if (netdiskManagerRequest->RequestType == NETDISK_MANAGER_REQUEST_TOUCH_VOLUME) {

				do {
			
					HANDLE			fileHandle = NULL;	
					UNICODE_STRING	fileName;
					PWCHAR			fileNameBuffer;
					NTSTATUS		unicodeCreateStatus;
					LARGE_INTEGER	interval;

					ULONG			retryCount = 5;

					//
					//	Allocate a name buffer
					//
	
					fileNameBuffer = ExAllocatePoolWithTag( NonPagedPool, NDFS_MAX_PATH, LFS_ALLOC_TAG );

					if (fileNameBuffer == NULL) {
		
						ASSERT( LFS_REQUIRED );
						NetdiskPartition_Dereference( netdiskManagerRequest->NetdiskPartition );
						break;
					}

					RtlInitEmptyUnicodeString( &fileName, fileNameBuffer, NDFS_MAX_PATH );

					RtlCopyUnicodeString( &fileName,
										  &netdiskManagerRequest->NetdiskPartition->NetdiskPartitionInformation.VolumeName );
	
					unicodeCreateStatus = RtlAppendUnicodeToString( &fileName, TOUCH_VOLUME_FILE_NAME );

					if (unicodeCreateStatus != STATUS_SUCCESS) {

						ASSERT(LFS_UNEXPECTED);
					}

					interval.QuadPart = ( 60 * DELAY_ONE_SECOND );      //delay 60 seconds
					KeDelayExecutionThread( KernelMode, FALSE, &interval );
					
					while (unicodeCreateStatus == STATUS_SUCCESS && retryCount--) {

						NTSTATUS				createStatus;
						IO_STATUS_BLOCK			ioStatusBlock;
					    LARGE_INTEGER			interval;
						ACCESS_MASK				desiredAccess;
						ULONG					attributes;
						OBJECT_ATTRIBUTES		objectAttributes;
						ULONG					fileAttributes;
						ULONG					shareAccess;
						ULONG					createDisposition;
						ULONG					createOptions;

			    
						ioStatusBlock.Information = 0;

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
		
						//allocationSize.LowPart  = 0;
						//allocationSize.HighPart = 0;

						fileAttributes	  = 0;
						shareAccess		  = FILE_SHARE_READ | FILE_SHARE_WRITE;
						createDisposition = FILE_OPEN_IF;
						createOptions     = FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE | FILE_DELETE_ON_CLOSE;
						//eaBuffer		  = NULL;
						//eaLength		  = 0;

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

						SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
										("NetdiskManagerThreadProc: Touch Volume %wZ, createStatus = %x, ioStatusBlock.Information = %d\n",
						                 &netdiskManagerRequest->NetdiskPartition->NetdiskPartitionInformation.VolumeName, createStatus, ioStatusBlock.Information ));

						if (createStatus == STATUS_SUCCESS) {

							ASSERT( ioStatusBlock.Information == FILE_CREATED );
							ZwClose( fileHandle );
							break;
						}
						interval.QuadPart = (3 * DELAY_ONE_SECOND);      //delay 3 seconds
						KeDelayExecutionThread( KernelMode, FALSE, &interval );

						//if(retryCount == 3)
						//	ASSERT(LFS_REQUIRED);
					}

					//
					// Free a name buffer
					//

					ExFreePool( fileNameBuffer );

					NetdiskPartition_Dereference( netdiskManagerRequest->NetdiskPartition );
			
				} while(0);
			
			} else if (netdiskManagerRequest->RequestType == NETDISK_MANAGER_REQUEST_CREATE_FILE) {

				do {
			
					HANDLE					fileHandle = NULL;	
					UNICODE_STRING			fileName;
					PWCHAR					fileNameBuffer;
					NTSTATUS				unicodeCreateStatus;

					ULONG					retryCount = 5;

					//
					//	Allocate a name buffer
					//
	
					fileNameBuffer = ExAllocatePoolWithTag( NonPagedPool, NDFS_MAX_PATH, LFS_ALLOC_TAG );

					if (fileNameBuffer == NULL) {
		
						ASSERT( LFS_REQUIRED );
						NetdiskPartition_Dereference( netdiskManagerRequest->NetdiskPartition );
						break;
					}

					RtlInitEmptyUnicodeString( &fileName, fileNameBuffer, NDFS_MAX_PATH );

					RtlCopyUnicodeString( &fileName,
										  &netdiskManagerRequest->NetdiskPartition->NetdiskPartitionInformation.VolumeName );
	
					unicodeCreateStatus = RtlAppendUnicodeToString( &fileName, CREATE_FILE_NAME );

					if (unicodeCreateStatus != STATUS_SUCCESS) {

						ASSERT(LFS_UNEXPECTED);
					}
					
					while (unicodeCreateStatus == STATUS_SUCCESS && retryCount--) {

						NTSTATUS				createStatus;
						IO_STATUS_BLOCK			ioStatusBlock;
					    LARGE_INTEGER			interval;
						ACCESS_MASK				desiredAccess;
						ULONG					attributes;
						OBJECT_ATTRIBUTES		objectAttributes;
						ULONG					fileAttributes;
						ULONG					shareAccess;
						ULONG					createDisposition;
						ULONG					createOptions;

			    
						ioStatusBlock.Information = 0;

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
		
						//allocationSize.LowPart  = 0;
						//allocationSize.HighPart = 0;

						fileAttributes	  = FILE_ATTRIBUTE_HIDDEN;
						shareAccess		  = FILE_SHARE_READ | FILE_SHARE_WRITE;
						createDisposition = FILE_OPEN_IF;
						createOptions     = FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE;
						//eaBuffer		  = NULL;
						//eaLength		  = 0;

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

						SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
										("NetdiskManagerThreadProc: Create File %wZ, createStatus = %x, ioStatusBlock.Information = %d\n",
						                 &netdiskManagerRequest->NetdiskPartition->NetdiskPartitionInformation.VolumeName, createStatus, ioStatusBlock.Information ));

						if (createStatus == STATUS_SUCCESS) {

							ASSERT( ioStatusBlock.Information == FILE_CREATED ||  ioStatusBlock.Information == FILE_OPENED );


							createStatus = ZwWriteFile( fileHandle,
														NULL,
														NULL,
														NULL,
														&ioStatusBlock,
														fileName.Buffer,
														fileName.Length,
														NULL,
														NULL );

							ZwClose( fileHandle );
							break;
						}
						interval.QuadPart = (3 * DELAY_ONE_SECOND);      //delay 3 seconds
						KeDelayExecutionThread( KernelMode, FALSE, &interval );

						//if(retryCount == 3)
						//	ASSERT(LFS_REQUIRED);
					}

					//
					// Free a name buffer
					//

					ExFreePool( fileNameBuffer );

					NetdiskPartition_Dereference( netdiskManagerRequest->NetdiskPartition );
			
				} while(0);
			
			} else {

				ASSERT( LFS_BUG );
				ExAcquireFastMutex( &NetdiskManager->FastMutex );
				SetFlag( NetdiskManager->Thread.Flags, NETDISK_MANAGER_THREAD_ERROR );
				ExReleaseFastMutex( &NetdiskManager->FastMutex );
				netdiskManagerThreadExit = TRUE;
			}

			if(netdiskManagerRequest->Synchronous == TRUE)
				KeSetEvent( &netdiskManagerRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
			else
				DereferenceNetdiskManagerRequest( netdiskManagerRequest );
		}
	}

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
				   ("NetdiskManagerThreadProc: PsTerminateSystemThread NetdiskManager = %p\n", NetdiskManager) );

	SetFlag( NetdiskManager->Thread.Flags, NETDISK_MANAGER_THREAD_TERMINATED );

	NetdiskManager_Dereference( NetdiskManager );	
	PsTerminateSystemThread( STATUS_SUCCESS );
}


PENABLED_NETDISK
NetdiskManager_Lookup (
	IN PNETDISK_MANAGER	NetdiskManager,
	IN PLPX_ADDRESS		NetDiskAddress,
	IN USHORT			UnitDiskNo,
	IN PUCHAR			NdscId	
	)
{	
	PENABLED_NETDISK	enabledNetdisk;
    PLIST_ENTRY			listEntry;
	KIRQL				oldIrql;

	KeAcquireSpinLock( &NetdiskManager->EnabledNetdiskQSpinLock, &oldIrql );

    for (enabledNetdisk = NULL, listEntry = NetdiskManager->EnabledNetdiskQueue.Flink;
         listEntry != &NetdiskManager->EnabledNetdiskQueue;
         enabledNetdisk = NULL, listEntry = listEntry->Flink) {

		enabledNetdisk = CONTAINING_RECORD (listEntry, ENABLED_NETDISK, ListEntry);
		ASSERT( enabledNetdisk->ReferenceCount > 0 );

		if (!RtlEqualMemory( &enabledNetdisk->NetdiskInformation.NetDiskAddress.Node,
							 NetDiskAddress->Node,
							 6)) {

			continue;
		}

		if (enabledNetdisk->NetdiskInformation.NetDiskAddress.Port != NetDiskAddress->Port) {

			continue;
		}

		if (enabledNetdisk->NetdiskInformation.UnitDiskNo != UnitDiskNo) {

			continue;
		}

		if (!RtlEqualMemory(enabledNetdisk->NetdiskInformation.NdscId, NdscId,  NDSC_ID_LENGTH)) {
			continue;
		}

		EnabledNetdisk_Reference( enabledNetdisk );	// dereferenced in caller function.
		
		break;
	}

	KeReleaseSpinLock( &NetdiskManager->EnabledNetdiskQSpinLock, oldIrql );

#if DBG
	if (enabledNetdisk == NULL)
		SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_ERROR,
					   ("EnabledNetdisk_Lookup: Enabled NetDisk not found.\n") );
#endif

	return enabledNetdisk;
}


VOID
NetdiskManager_FileSystemShutdown (
	IN PNETDISK_MANAGER		NetdiskManager
	)
{
	PLIST_ENTRY			enabledNetdiskListEntry;
	KIRQL				oldIrql;
	PENABLED_NETDISK	enabledNetdisk;


	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("NetdiskManager_FileSystemShutdown: Entered\n") );

	do {

		KeAcquireSpinLock( &NetdiskManager->EnabledNetdiskQSpinLock, &oldIrql );

		for (enabledNetdisk = NULL, enabledNetdiskListEntry = NetdiskManager->EnabledNetdiskQueue.Flink;
			 enabledNetdiskListEntry != &NetdiskManager->EnabledNetdiskQueue;
			 enabledNetdisk = NULL, enabledNetdiskListEntry = enabledNetdiskListEntry->Flink) {

			
			enabledNetdisk = CONTAINING_RECORD (enabledNetdiskListEntry, ENABLED_NETDISK, ListEntry);
		
			if (!FlagOn(enabledNetdisk->Flags, ENABLED_NETDISK_SHUTDOWN)) {

				EnabledNetdisk_Reference( enabledNetdisk ); // dereferenced after the loop
				break;
			}
		}

		KeReleaseSpinLock( &NetdiskManager->EnabledNetdiskQSpinLock, oldIrql );

		if (enabledNetdisk) {

			EnabledNetdisk_FileSystemShutdown( enabledNetdisk );
			EnabledNetdisk_Dereference( enabledNetdisk ); // referenced in the loop.
		}

	} while (enabledNetdisk);

	return;
}
	

NTSTATUS
NetdiskManager_PreMountVolume (
	IN  PNETDISK_MANAGER		NetdiskManager,
	IN  BOOLEAN					ForceIndirect,
	IN  BOOLEAN					CallFromMiniFilter,
	IN  PDEVICE_OBJECT			VolumeDeviceObject,
	IN  PDEVICE_OBJECT			DiskDeviceObject,
	OUT PNETDISK_PARTITION		*NetdiskPartition,
	OUT PNETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
	NTSTATUS				status = STATUS_UNSUCCESSFUL;

	BOOLEAN					result;
	NETDISK_INFORMATION		netdiskInformation;
	
	PENABLED_NETDISK		enabledNetdisk = NULL;

	BOOLEAN					resourceAcquired = FALSE;
	BOOLEAN					decreaseEnabledNetdisk = FALSE;

	PDEVICE_OBJECT			scsiportAdapterDeviceObject = NULL;
		
	try {

		result = IsNetdiskPartition( DiskDeviceObject, &netdiskInformation, &scsiportAdapterDeviceObject );
	
		if (result == FALSE) {

			status = STATUS_UNSUCCESSFUL;
			leave;
		}

		KeEnterCriticalRegion();
		ExAcquireResourceExclusiveLite( &NetdiskManager->Resource, TRUE );
		resourceAcquired = TRUE;

		enabledNetdisk = NetdiskManager_Lookup( NetdiskManager,
											    &netdiskInformation.NetDiskAddress,
											    netdiskInformation.UnitDiskNo,
											    netdiskInformation.NdscId );

		if (enabledNetdisk) {

			decreaseEnabledNetdisk = TRUE;

			if (enabledNetdisk->NetdiskInformation.EnabledTime.QuadPart != netdiskInformation.EnabledTime.QuadPart) {

				KIRQL	oldIrql;

				KeAcquireSpinLock( &NetdiskManager->EnabledNetdiskQSpinLock, &oldIrql );
				RemoveEntryList( &enabledNetdisk->ListEntry );
				InitializeListHead( &enabledNetdisk->ListEntry );
				KeReleaseSpinLock(&NetdiskManager->EnabledNetdiskQSpinLock, oldIrql);

				ASSERT( EnabledNetdisk_IsStoppedNetdisk(enabledNetdisk) );

				EnabledNetdisk_Close( enabledNetdisk );
				EnabledNetdisk_Dereference( enabledNetdisk ); // referenced by NetdiskManager_Lookup()

				decreaseEnabledNetdisk = FALSE;

				enabledNetdisk = NULL;
			}
		}

		if (enabledNetdisk) {

			BOOLEAN	updateResult;

			updateResult = EnabledNetdisk_Update( enabledNetdisk,
												  VolumeDeviceObject,
												  DiskDeviceObject,
												  &netdiskInformation );
		
			if (updateResult != TRUE) {
	
				ASSERT( FALSE );
				
				status = STATUS_UNSUCCESSFUL;
				leave;
			}
		}

		if (enabledNetdisk == NULL) {

			enabledNetdisk = EnabledNetdisk_Create( NetdiskManager,
													VolumeDeviceObject,
													DiskDeviceObject,
													&netdiskInformation );

			if (enabledNetdisk == NULL) {

				status = STATUS_UNSUCCESSFUL;
				leave;
			}

			ExInterlockedInsertHeadList( &NetdiskManager->EnabledNetdiskQueue,
										 &enabledNetdisk->ListEntry,
										 &NetdiskManager->EnabledNetdiskQSpinLock );
		}

		if (GlobalLfs.NdasFsMiniMode == TRUE) {

			if (CallFromMiniFilter == TRUE) {

				if (enabledNetdisk->NetdiskEnableMode == NETDISK_READ_ONLY) {

					status = STATUS_VOLUME_MOUNTED;
			
				} else {

					status = STATUS_SUCCESS;
				}

			} else {

				if (enabledNetdisk->NetdiskEnableMode != NETDISK_READ_ONLY) {

					status = STATUS_VOLUME_MOUNTED;
			
				} else {

					status = STATUS_SUCCESS;
				}
			}

			if (status == STATUS_VOLUME_MOUNTED) {

				*NetdiskEnableMode = enabledNetdisk->NetdiskEnableMode;
				leave;
			}
		}

		status = EnabledNetdisk_PreMountVolume( enabledNetdisk,
												ForceIndirect,
												VolumeDeviceObject,
												DiskDeviceObject,
												&scsiportAdapterDeviceObject,
												NetdiskPartition,
												NetdiskEnableMode );

	} finally {

		if (scsiportAdapterDeviceObject) {

			ObDereferenceObject( scsiportAdapterDeviceObject );
		}

		if (decreaseEnabledNetdisk == TRUE) {

			EnabledNetdisk_Dereference( enabledNetdisk ); // referenced by NetdiskManager_Lookup()
		}

		if (resourceAcquired == TRUE) {

			ExReleaseResourceLite( &NetdiskManager->Resource );
			KeLeaveCriticalRegion();
		}
	}

	return status;
}


NTSTATUS
NetdiskManager_PostMountVolume (
	IN  PNETDISK_MANAGER				NetdiskManager,
	IN  PNETDISK_PARTITION				NetdiskPartition,
	IN  NETDISK_ENABLE_MODE				NetdiskEnableMode,
	IN  BOOLEAN							Mount,
	IN	LFS_FILE_SYSTEM_TYPE			FileSystemType,
	IN  PLFS_DEVICE_EXTENSION			LfsDeviceExt,
	OUT	PNETDISK_PARTITION_INFORMATION	NetdiskPartionInformation
	)
{
	NTSTATUS			status;


	UNREFERENCED_PARAMETER( NetdiskManager );

	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &NetdiskManager->Resource, TRUE );

	status = EnabledNetdisk_PostMountVolume( NetdiskPartition->EnabledNetdisk,
											 NetdiskPartition,
											 NetdiskEnableMode,
											 Mount,
											 FileSystemType,
											 LfsDeviceExt,
											 NetdiskPartionInformation );

	ExReleaseResourceLite( &NetdiskManager->Resource );
	KeLeaveCriticalRegion();

	return status;
}


VOID
NetdiskManager_MountVolumeComplete (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode,
	IN NTSTATUS				MountStatus,
	IN PDEVICE_OBJECT		AttachedToDeviceObject
	)
{
	UNREFERENCED_PARAMETER( NetdiskManager );

	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &NetdiskManager->Resource, TRUE );

	EnabledNetdisk_MountVolumeComplete( NetdiskPartition->EnabledNetdisk,
										NetdiskPartition,
										NetdiskEnableMode,
										MountStatus,
										AttachedToDeviceObject );

	ExReleaseResourceLite( &NetdiskManager->Resource );
	KeLeaveCriticalRegion();

	return;
}


VOID
NetdiskManager_CreateFile (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition
	)
{
	PNETDISK_MANAGER_REQUEST	netdiskManagerRequest;

	UNREFERENCED_PARAMETER( NetdiskManager );

	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &NetdiskManager->Resource, TRUE );


	netdiskManagerRequest = AllocNetdiskManagerRequest( FALSE );
				
	if (netdiskManagerRequest == NULL) {
				
		SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
					   ("EnabledNetdisk_DisMountVolume: failed to allocate Netdisk manager request.\n") );
	
	} else {

		netdiskManagerRequest->RequestType = NETDISK_MANAGER_REQUEST_CREATE_FILE;
		NetdiskPartition_Reference( NetdiskPartition );
		netdiskManagerRequest->NetdiskPartition = NetdiskPartition;

		QueueingNetdiskManagerRequest( GlobalLfs.NetdiskManager,
									   netdiskManagerRequest );
	}

	ExReleaseResourceLite( &NetdiskManager->Resource );
	KeLeaveCriticalRegion();

	return;
}


VOID
NetdiskManager_ChangeMode (
	IN		PNETDISK_MANAGER		NetdiskManager,
	IN		PNETDISK_PARTITION		NetdiskPartition,
	IN OUT	PNETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
	if (NetdiskPartition == NULL) {

		NDAS_ASSERT( FALSE );
		return;
	}

	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &NetdiskManager->Resource, TRUE );

	EnabledNetdisk_ChangeMode( NetdiskPartition->EnabledNetdisk,
							   NetdiskPartition,
							   NetdiskEnableMode );

	ExReleaseResourceLite( &NetdiskManager->Resource );
	KeLeaveCriticalRegion();

	return;
}


VOID
NetdiskManager_SurpriseRemoval (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
	if (NetdiskPartition == NULL) {

		NDAS_ASSERT( FALSE );
		return;
	}

	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &NetdiskManager->Resource, TRUE );

	EnabledNetdisk_SurpriseRemoval( NetdiskPartition->EnabledNetdisk,
									NetdiskPartition,
									NetdiskEnableMode );

	ExReleaseResourceLite( &NetdiskManager->Resource );
	KeLeaveCriticalRegion();

	return;
}

VOID
NetdiskManager_QueryRemoveMountVolume (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
	if (NetdiskPartition == NULL) {

		NDAS_ASSERT( FALSE );
		return;
	}

	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &NetdiskManager->Resource, TRUE );

	EnabledNetdisk_QueryRemoveMountVolume( NetdiskPartition->EnabledNetdisk, 
										   NetdiskPartition,
										   NetdiskEnableMode );

	ExReleaseResourceLite( &NetdiskManager->Resource );
	KeLeaveCriticalRegion();

	return;
}
	
VOID
NetdiskManager_CancelRemoveMountVolume (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
	if (NetdiskPartition == NULL) {

		NDAS_ASSERT( FALSE );
		return;
	}

	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &NetdiskManager->Resource, TRUE );

	EnabledNetdisk_CancelRemoveMountVolume( NetdiskPartition->EnabledNetdisk, 
										   NetdiskPartition,
										   NetdiskEnableMode );

	ExReleaseResourceLite( &NetdiskManager->Resource );
	KeLeaveCriticalRegion();

	return;
}

VOID
NetdiskManager_DisMountVolume (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
	if (NetdiskPartition == NULL) {

		NDAS_ASSERT( FALSE );
		return;
	}

	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &NetdiskManager->Resource, TRUE );

	EnabledNetdisk_DisMountVolume( NetdiskPartition->EnabledNetdisk, 
								   NetdiskPartition,
								   NetdiskEnableMode );

	ExReleaseResourceLite( &NetdiskManager->Resource );
	KeLeaveCriticalRegion();

	return;
}


NTSTATUS
NetdiskManager_Secondary2Primary (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
	NTSTATUS	status;

	if (NetdiskPartition == NULL) {

		NDAS_ASSERT( FALSE );
		return STATUS_UNSUCCESSFUL;
	}

	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite( &NetdiskManager->Resource, TRUE );

	status = EnabledNetdisk_Secondary2Primary( NetdiskPartition->EnabledNetdisk,
											   NetdiskPartition,
											   NetdiskEnableMode );

	ExReleaseResourceLite( &NetdiskManager->Resource );
	KeLeaveCriticalRegion();

	return status;
}


BOOLEAN
NetdiskManager_ThisVolumeHasSecondary (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode,
	IN BOOLEAN				IncludeLocalSecondary
	)
{
	BOOLEAN	boolSucc;

	if (NetdiskPartition == NULL) {

		NDAS_ASSERT( FALSE );
		return FALSE;
	}

	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite( &NetdiskManager->Resource, TRUE );

	boolSucc = EnabledNetdisk_ThisVolumeHasSecondary( NetdiskPartition->EnabledNetdisk,
													  NetdiskPartition,
													  NetdiskEnableMode,
													  IncludeLocalSecondary );

	ExReleaseResourceLite( &NetdiskManager->Resource );
	KeLeaveCriticalRegion();

	return boolSucc;
}


BOOLEAN
NetdiskManager_IsStoppedNetdisk (
	IN PNETDISK_MANAGER	NetdiskManager,
	IN PENABLED_NETDISK	EnabledNetdisk
	)
{
	BOOLEAN	result;

	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite( &NetdiskManager->Resource, TRUE );

	result = EnabledNetdisk_IsStoppedNetdisk( EnabledNetdisk );
	
	ExReleaseResourceLite( &NetdiskManager->Resource );
	KeLeaveCriticalRegion();

	return result;
}


NTSTATUS
NetdiskManager_UnplugNetdisk (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
	NTSTATUS	status;

	UNREFERENCED_PARAMETER( NetdiskManager );

	//KeEnterCriticalRegion();
	//NmAcquireResourceExclusiveLite( &NetdiskManager->Resource, TRUE );

	//KeEnterCriticalRegion();
	//ExAcquireFastMutexUnsafe( &NetdiskManager->NDManFastMutex );

	status = EnabledNetdisk_UnplugNetdisk( NetdiskPartition->EnabledNetdisk,
										   NetdiskPartition,
										   NetdiskEnableMode );

	//ExReleaseFastMutexUnsafe( &NetdiskManager->NDManFastMutex );
	//KeLeaveCriticalRegion();

	//NmReleaseResourceLite( &NetdiskManager->Resource );
	//KeLeaveCriticalRegion();

	return status;
}


NTSTATUS
NetdiskManager_AddWriteRange (
	IN	PNETDISK_MANAGER	NetdiskManager,
	IN  PNETDISK_PARTITION	NetdiskPartition,
	OUT PBLOCKACE_ID		BlockAceId
	)
{
	NTSTATUS		status;
	NDAS_BLOCK_ACE	ndasBace;

	PSRB_IO_CONTROL	psrbioctl;
	int				outbuff_sz;
	PNDSCIOCTL_ADD_USERBACL	ioctlAddBacl;
	PBLOCKACE_ID	blockAceId;

	UNREFERENCED_PARAMETER( NetdiskManager );
	
	//KeEnterCriticalRegion();
	//ExAcquireResourceSharedLite( &NetdiskManager->Resource, TRUE );

	ndasBace.AccessMode = NBACE_ACCESS_READ | NBACE_ACCESS_WRITE;

	if (IS_WINDOWS2K()) {

		ndasBace.BlockStartAddr = NetdiskPartition->NetdiskPartitionInformation.PartitionInformation.StartingOffset.QuadPart/512;
		ndasBace.BlockEndAddr = (NetdiskPartition->NetdiskPartitionInformation.PartitionInformation.StartingOffset.QuadPart + 
								NetdiskPartition->NetdiskPartitionInformation.PartitionInformation.PartitionLength.QuadPart)/512 - 1;
	} else {

		ndasBace.BlockStartAddr = NetdiskPartition->NetdiskPartitionInformation.PartitionInformationEx.StartingOffset.QuadPart/512;
		ndasBace.BlockEndAddr = (NetdiskPartition->NetdiskPartitionInformation.PartitionInformationEx.StartingOffset.QuadPart + 
								 NetdiskPartition->NetdiskPartitionInformation.PartitionInformationEx.PartitionLength.QuadPart)/512 - 1;
	}

	ASSERT( ndasBace.BlockStartAddr <= ndasBace.BlockEndAddr );


	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	outbuff_sz = sizeof(SRB_IO_CONTROL) + sizeof(NDSCIOCTL_ADD_USERBACL);
	psrbioctl = (PSRB_IO_CONTROL)ExAllocatePoolWithTag(NonPagedPool , outbuff_sz, LFS_ALLOC_TAG);
	
	if(psrbioctl == NULL) {

        SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
						("STATUS_INSUFFICIENT_RESOURCES\n") );

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	memset( psrbioctl, 0, sizeof(*psrbioctl) );
	psrbioctl->HeaderLength = sizeof(SRB_IO_CONTROL);
	memcpy( psrbioctl->Signature, NDASSCSI_IOCTL_SIGNATURE, 8 );
	psrbioctl->Timeout = 10;
	psrbioctl->ControlCode = NDASSCSI_IOCTL_ADD_USERBACL;
	psrbioctl->Length = sizeof(NDSCIOCTL_ADD_USERBACL);

	ioctlAddBacl = (PNDSCIOCTL_ADD_USERBACL)((PUCHAR)psrbioctl + sizeof(SRB_IO_CONTROL));
	blockAceId = (PBLOCKACE_ID)ioctlAddBacl;
	ioctlAddBacl->NdasScsiAddress.NdasScsiAddress = 0;
	RtlCopyMemory( &ioctlAddBacl->NdasBlockAce, &ndasBace, sizeof(NDAS_BLOCK_ACE) );

	//
	// Try with the disk device.
	//

	if(NetdiskPartition->NetdiskPartitionInformation.DiskDeviceObject) {
		status = LfsFilterDeviceIoControl( NetdiskPartition->NetdiskPartitionInformation.DiskDeviceObject,
										   IOCTL_SCSI_MINIPORT,
										   psrbioctl,
										   outbuff_sz,
										   psrbioctl,
										   outbuff_sz,
										   NULL );
	} else {
		status = STATUS_UNSUCCESSFUL;
	}

	if(!NT_SUCCESS(status)) {
		//
		// Try with the SCSI adapter.
		//
		status = LfsFilterDeviceIoControl( NetdiskPartition->ScsiportAdapterDeviceObject,
										   IOCTL_SCSI_MINIPORT,
										   psrbioctl,
										   outbuff_sz,
										   psrbioctl,
										   outbuff_sz,
										   NULL );
	}

	if (status == STATUS_SUCCESS) {
	
		*BlockAceId = *blockAceId;

		KeEnterCriticalRegion();
		ExAcquireResourceExclusiveLite( &NetdiskPartition->EnabledNetdisk->Resource, TRUE );

		NDAS_ASSERT( NetdiskPartition->ScsiportAdapterDeviceObjectReferenceCount );

		NetdiskPartition->ScsiportAdapterDeviceObjectReferenceCount ++;

		ExReleaseResourceLite( &NetdiskPartition->EnabledNetdisk->Resource );
		KeLeaveCriticalRegion();

	} else {

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
					   ("[LFS] AddUserBacl: Not NetDisk. status = %x\n", 
					   status) );
	}

	ExFreePool( psrbioctl );

	return status;
}


VOID
NetdiskManager_RemoveWriteRange (
	IN	PNETDISK_MANAGER	NetdiskManager,
	IN  PNETDISK_PARTITION	NetdiskPartition,
	OUT BLOCKACE_ID			BlockAceId
	)
{
	NTSTATUS		status;
	PSRB_IO_CONTROL	psrbioctl;
	int				outbuff_sz;
	PNDSCIOCTL_REMOVE_USERBACL	ioctlRemoveBacl;


	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("NetdiskManager_RemoveWriteRange: Entered\n") );

	UNREFERENCED_PARAMETER( NetdiskManager );

	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	outbuff_sz = sizeof(SRB_IO_CONTROL) + sizeof(NDASSCSI_QUERY_INFO_DATA) + sizeof(NDSCIOCTL_REMOVE_USERBACL);
	psrbioctl = (PSRB_IO_CONTROL)ExAllocatePoolWithTag(NonPagedPool , outbuff_sz, LFS_ALLOC_TAG);

	if (psrbioctl == NULL) {

        SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
					   ("STATUS_INSUFFICIENT_RESOURCES\n") );

		return;
	}

	memset( psrbioctl, 0, sizeof(*psrbioctl) );
	psrbioctl->HeaderLength = sizeof(SRB_IO_CONTROL);
	memcpy( psrbioctl->Signature, NDASSCSI_IOCTL_SIGNATURE, 8 );
	psrbioctl->Timeout = 10;
	psrbioctl->ControlCode = NDASSCSI_IOCTL_REMOVE_USERBACL;
	psrbioctl->Length = sizeof(NDSCIOCTL_REMOVE_USERBACL);

	ioctlRemoveBacl = (PNDSCIOCTL_REMOVE_USERBACL)((PUCHAR)psrbioctl + sizeof(SRB_IO_CONTROL));
	ioctlRemoveBacl->NdasScsiAddress.NdasScsiAddress = 0;
	ioctlRemoveBacl->NdasBlockAceId = BlockAceId;

	status = STATUS_UNSUCCESSFUL;

#if 0

	// If called with PNP context, It's deadlocked.

	//
	// Try with the disk device
	//

	if (NetdiskPartition->NetdiskPartitionInformation.DiskDeviceObject) {
	
		status = LfsFilterDeviceIoControl( NetdiskPartition->NetdiskPartitionInformation.DiskDeviceObject,
										   IOCTL_SCSI_MINIPORT,
										   psrbioctl,
										   outbuff_sz,
										   psrbioctl,
										   outbuff_sz,
										   NULL );
	}
	
#endif

	//
	// Try with the SCSI adapter
	//

	if (!NT_SUCCESS(status)) {

		status = LfsFilterDeviceIoControl( NetdiskPartition->ScsiportAdapterDeviceObject,
										   IOCTL_SCSI_MINIPORT,
										   psrbioctl,
										   outbuff_sz,
										   psrbioctl,
										   outbuff_sz,
										   NULL );
	}
	
	if (!NT_SUCCESS(status)) {
	
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
					   ("[LFS] GetPrimaryUnitDisk: Not NetDisk. status = %x\n", 
						status) );
	}

	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &NetdiskPartition->EnabledNetdisk->Resource, TRUE );

	NDAS_ASSERT( NetdiskPartition->ScsiportAdapterDeviceObjectReferenceCount );

	NetdiskPartition->ScsiportAdapterDeviceObjectReferenceCount --;

	if (NetdiskPartition->ScsiportAdapterDeviceObjectReferenceCount == 0) {

		ObDereferenceObject( NetdiskPartition->ScsiportAdapterDeviceObject );
		NetdiskPartition->ScsiportAdapterDeviceObject = NULL;

		ObDereferenceObject( NetdiskPartition->NetdiskPartitionInformation.DiskDeviceObject );
		NetdiskPartition->NetdiskPartitionInformation.DiskDeviceObject = NULL;
	}

	ExReleaseResourceLite( &NetdiskPartition->EnabledNetdisk->Resource );
	KeLeaveCriticalRegion();

	ExFreePool( psrbioctl );

	return;
}


NTSTATUS
NetdiskManager_GetPrimaryPartition (
	IN  PNETDISK_MANAGER				NetdiskManager,
	IN  PPRIMARY_SESSION				PrimarySession,
	IN  PLPX_ADDRESS					NetDiskAddress,
	IN  USHORT							UnitDiskNo,
	IN PUCHAR							NdscId,
	IN  PLARGE_INTEGER					StartingOffset,
	IN  BOOLEAN							LocalSecondary,
	OUT PNETDISK_PARTITION				*NetdiskPartition,
	OUT PNETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation,
	OUT PLFS_FILE_SYSTEM_TYPE			FileSystemType
	)
{	
	NTSTATUS			status;
	PENABLED_NETDISK	enabledNetdisk;


	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite( &NetdiskManager->Resource, TRUE );

	enabledNetdisk = NetdiskManager_Lookup( NetdiskManager, NetDiskAddress, UnitDiskNo, NdscId );

	if (enabledNetdisk == NULL) {

		ExReleaseResourceLite( &NetdiskManager->Resource );
		KeLeaveCriticalRegion();

		return STATUS_NO_SUCH_DEVICE;
	}

	ExReleaseResourceLite( &NetdiskManager->Resource );
	KeLeaveCriticalRegion();

	if (FlagOn(enabledNetdisk->Flags, ENABLED_NETDISK_SHUTDOWN)) {

		EnabledNetdisk_Dereference( enabledNetdisk );		// by EnabledNetdisk_Lookup

		return STATUS_SYSTEM_SHUTDOWN;
	}

	status = EnabledNetdisk_GetPrimaryPartition( enabledNetdisk,
												 PrimarySession,
												 NetDiskAddress,
												 UnitDiskNo,
												 StartingOffset,
												 LocalSecondary,
												 NetdiskPartition,
												 NetdiskPartitionInformation,
												 FileSystemType );

	EnabledNetdisk_Dereference( enabledNetdisk );		// by EnabledNetdisk_Lookup

	return status;
}


VOID
NetdiskManager_ReturnPrimaryPartition (
	IN PNETDISK_MANAGER		NetdiskManager, 
	IN PPRIMARY_SESSION		PrimarySession,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN BOOLEAN				LocalSecondary
	)
{
	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite( &NetdiskManager->Resource, TRUE );

	EnabledNetdisk_ReturnPrimaryPartition( NetdiskPartition->EnabledNetdisk,
										   PrimarySession,
										   NetdiskPartition,
										   LocalSecondary );

	ExReleaseResourceLite( &NetdiskManager->Resource );
	KeLeaveCriticalRegion();

	return;
}


NTSTATUS
NetdiskManager_TakeOver (
	IN PNETDISK_MANAGER		NetdiskManager, 
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN PSESSION_INFORMATION	SessionInformation
	)
{
	NTSTATUS	status;

	UNREFERENCED_PARAMETER( NetdiskManager );
	//KeEnterCriticalRegion();
	//ExAcquireResourceSharedLite( &NetdiskManager->Resource, TRUE );

	status = EnabledNetdisk_TakeOver( NetdiskPartition->EnabledNetdisk,
									  NetdiskPartition,
									  SessionInformation );

	//ExReleaseResourceLite( &NetdiskManager->Resource );
	//KeLeaveCriticalRegion();

	return status;
}

VOID
NetdiskManager_PrimarySessionStopping (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite( &NetdiskManager->Resource, TRUE );

	EnabledNetdisk_PrimarySessionStopping( NetdiskPartition->EnabledNetdisk, 
										   NetdiskPartition, 
										   NetdiskEnableMode );

	ExReleaseResourceLite( &NetdiskManager->Resource );
	KeLeaveCriticalRegion();

	return;
}


VOID
NetdiskManager_PrimarySessionCancelStopping (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite( &NetdiskManager->Resource, TRUE );

	EnabledNetdisk_PrimarySessionCancelStopping( NetdiskPartition->EnabledNetdisk, 
												 NetdiskPartition, 
												 NetdiskEnableMode );

	ExReleaseResourceLite( &NetdiskManager->Resource );
	KeLeaveCriticalRegion();

	return;
}


VOID
NetdiskManager_PrimarySessionDisconnect (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite( &NetdiskManager->Resource, TRUE );

	EnabledNetdisk_PrimarySessionDisconnect( NetdiskPartition->EnabledNetdisk, 
											 NetdiskPartition, 
											 NetdiskEnableMode );

	ExReleaseResourceLite( &NetdiskManager->Resource );
	KeLeaveCriticalRegion();

	return;
}


PNETDISK_MANAGER_REQUEST
AllocNetdiskManagerRequest (
	IN	BOOLEAN	Synchronous
) 
{
	PNETDISK_MANAGER_REQUEST	netdiskManagerRequest;


 	netdiskManagerRequest = ExAllocatePoolWithTag( NonPagedPool,
												   sizeof(NETDISK_MANAGER_REQUEST),
												   NETDISK_MANAGER_REQUEST_TAG );

	if (netdiskManagerRequest == NULL) {

		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
		return NULL;
	}

	RtlZeroMemory( netdiskManagerRequest, sizeof(NETDISK_MANAGER_REQUEST) );

	netdiskManagerRequest->ReferenceCount = 1;
	InitializeListHead( &netdiskManagerRequest->ListEntry );
	
	netdiskManagerRequest->Synchronous = Synchronous;
	KeInitializeEvent( &netdiskManagerRequest->CompleteEvent, NotificationEvent, FALSE );

	return netdiskManagerRequest;
}


VOID
DereferenceNetdiskManagerRequest (
	IN PNETDISK_MANAGER_REQUEST	NetdiskManagerRequest
	) 
{
	LONG	result ;

	result = InterlockedDecrement( &NetdiskManagerRequest->ReferenceCount );

	ASSERT( result >= 0 );

	if (0 == result) {

		ExFreePoolWithTag( NetdiskManagerRequest,
						   NETDISK_MANAGER_REQUEST_TAG );

		SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
					   ("DereferenceNetdiskManagerRequest: NetdiskManagerRequest freed\n") );
	}	
}


FORCEINLINE
VOID
QueueingNetdiskManagerRequest (
	IN PNETDISK_MANAGER			NetdiskManager,
	IN PNETDISK_MANAGER_REQUEST	NetdiskManagerRequest
	)
{
	ExInterlockedInsertTailList( &NetdiskManager->Thread.RequestQueue,
								 &NetdiskManagerRequest->ListEntry,
								 &NetdiskManager->Thread.RequestQSpinLock );

	KeSetEvent( &NetdiskManager->Thread.RequestEvent, IO_DISK_INCREMENT, FALSE );
	
	return;
}


NTSTATUS
NetdiskManager_QueryPartitionInformation (
	IN  PNETDISK_MANAGER				NetdiskManager,
	IN  PDEVICE_OBJECT					DiskDeviceObject,
	OUT PNETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation
	)
{
	PNETDISK_PARTITION	netdiskPartition = NULL;
	KIRQL				oldIrql;
	PENABLED_NETDISK	enabledNetdisk;
    PLIST_ENTRY			listEntry;


	KeAcquireSpinLock( &NetdiskManager->EnabledNetdiskQSpinLock, &oldIrql );

    for (enabledNetdisk = NULL, listEntry = NetdiskManager->EnabledNetdiskQueue.Flink;
         listEntry != &NetdiskManager->EnabledNetdiskQueue;
         enabledNetdisk = NULL, listEntry = listEntry->Flink) {

		enabledNetdisk = CONTAINING_RECORD( listEntry, ENABLED_NETDISK, ListEntry );
		ASSERT( enabledNetdisk->ReferenceCount > 0 );

		netdiskPartition = EnabledNetdisk_LookupNetdiskPartition( enabledNetdisk,
																  NULL,
																  NULL,
																  DiskDeviceObject );
		
		if (netdiskPartition != NULL)
			break;			
	}

	KeReleaseSpinLock( &NetdiskManager->EnabledNetdiskQSpinLock, oldIrql );

	if (netdiskPartition) {

		*NetdiskPartitionInformation = netdiskPartition->NetdiskPartitionInformation;

		RtlInitEmptyUnicodeString( &NetdiskPartitionInformation->VolumeName,
								   NetdiskPartitionInformation->VolumeNameBuffer,
								   sizeof(NetdiskPartitionInformation->VolumeNameBuffer) );

		if (RtlAppendUnicodeStringToString( &NetdiskPartitionInformation->VolumeName,
											&netdiskPartition->NetdiskPartitionInformation.VolumeName
											) != STATUS_SUCCESS) {

			ASSERT( LFS_UNEXPECTED );
		}

		NDAS_ASSERT( NetdiskPartitionInformation->VolumeName.Length );

		ASSERT( NetdiskPartitionInformation->VolumeName.Buffer == NetdiskPartitionInformation->VolumeNameBuffer );

		NetdiskPartition_Dereference( netdiskPartition );

		return STATUS_SUCCESS;
	
	} else {

		return STATUS_UNSUCCESSFUL;
	}
}


PNETDISK_PARTITION
NetdiskManager_QueryNetdiskPartition (
	IN  PNETDISK_MANAGER		NetdiskManager,
	IN  PDEVICE_OBJECT			DiskDeviceObject
	)
{
	PNETDISK_PARTITION	netdiskPartition = NULL;
	KIRQL				oldIrql;
	PENABLED_NETDISK	enabledNetdisk;
    PLIST_ENTRY			listEntry;


	KeAcquireSpinLock( &NetdiskManager->EnabledNetdiskQSpinLock, &oldIrql );

    for (enabledNetdisk = NULL, listEntry = NetdiskManager->EnabledNetdiskQueue.Flink;
         listEntry != &NetdiskManager->EnabledNetdiskQueue;
         enabledNetdisk = NULL, listEntry = listEntry->Flink) {

		enabledNetdisk = CONTAINING_RECORD( listEntry, ENABLED_NETDISK, ListEntry );
		ASSERT( enabledNetdisk->ReferenceCount > 0 );

		netdiskPartition = EnabledNetdisk_LookupNetdiskPartition( enabledNetdisk,
																  NULL,
																  NULL,
																  DiskDeviceObject );
		
		if (netdiskPartition != NULL)
			break;			
	}

	KeReleaseSpinLock( &NetdiskManager->EnabledNetdiskQSpinLock, oldIrql );

	return netdiskPartition;
}


//////////////////////////////////////////////////////////////////////////
//
//	Enabled NetDisk management
//

PENABLED_NETDISK
EnabledNetdisk_Create (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PDEVICE_OBJECT		VolumeDeviceObject,
	IN PDEVICE_OBJECT		DiskDeviceObject,
	IN PNETDISK_INFORMATION	NetdiskInformation
	)
{
	PENABLED_NETDISK		enabledNetdisk = NULL;

	NETDISK_ENABLE_MODE		netdiskEnableMode;


	UNREFERENCED_PARAMETER( DiskDeviceObject );

	if (NetdiskInformation->DeviceMode == DEVMODE_SHARED_READWRITE) {

		if (!FlagOn(NetdiskInformation->EnabledFeatures, NDASFEATURE_SECONDARY)) {

			netdiskEnableMode = NETDISK_PRIMARY;

		} else {

			netdiskEnableMode = NETDISK_SECONDARY;
		}

	} else if (NetdiskInformation->DeviceMode == DEVMODE_SHARED_READONLY) {

		netdiskEnableMode = NETDISK_READ_ONLY;

	} else {

		NDAS_ASSERT( FALSE );
		netdiskEnableMode = NETDISK_UNKNOWN_MODE;

		return NULL;
	}

if (IS_WINDOWS2K()) {

	NTSTATUS					ntStatus;
	PDRIVE_LAYOUT_INFORMATION	driveLayoutInformation;
	ULONG						driveLayoutInformationSize = 0;
	ULONG						partitionIndex;


	enabledNetdisk = ExAllocatePoolWithTag( NonPagedPool, 
											sizeof(ENABLED_NETDISK),
											NETDISK_MANAGER_ENABLED_TAG );
	
	if (enabledNetdisk == NULL) {

		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
		return NULL;
	}

	NetdiskManager_Reference( NetdiskManager );

	RtlZeroMemory( enabledNetdisk, sizeof(ENABLED_NETDISK) );
	

	//KeInitializeSpinLock( &enabledNetdisk->SpinLock );
	ExInitializeFastMutex( &enabledNetdisk->FastMutex );
	enabledNetdisk->ReferenceCount = 1;

	ExInitializeResourceLite( &enabledNetdisk->Resource );

	InitializeListHead( &enabledNetdisk->ListEntry );
	enabledNetdisk->NetdiskManager = NetdiskManager;

	KeInitializeSpinLock( &enabledNetdisk->NetdiskPartitionQSpinLock );
	InitializeListHead( &enabledNetdisk->NetdiskPartitionQueue );

	enabledNetdisk->NetdiskInformation = *NetdiskInformation;
	enabledNetdisk->NetdiskEnableMode = netdiskEnableMode;

	while (1) {

		if (driveLayoutInformationSize == 0) {

			driveLayoutInformationSize = sizeof(DRIVE_LAYOUT_INFORMATION) + 
										 sizeof(PARTITION_INFORMATION) * 7;
		
		} else {
	
			driveLayoutInformationSize += sizeof(PARTITION_INFORMATION); 
		}

		driveLayoutInformation = ExAllocatePoolWithTag( NonPagedPool, 
													    driveLayoutInformationSize,
														NETDISK_MANAGER_TAG );

		if (driveLayoutInformation == NULL) {

			NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
			return enabledNetdisk;
		}
		
		ntStatus = LfsFilterDeviceIoControl( VolumeDeviceObject,
											 IOCTL_DISK_GET_DRIVE_LAYOUT,
											 NULL,
											 0,
											 driveLayoutInformation,
											 driveLayoutInformationSize,
											 NULL );

		if (ntStatus == STATUS_SUCCESS) {

			break;
		}
		
		ExFreePoolWithTag( driveLayoutInformation,
						   NETDISK_MANAGER_TAG );

		if (ntStatus == STATUS_BUFFER_TOO_SMALL) {

			continue;
		}

		NDAS_ASSERT( FALSE );
		InitializeListHead( &enabledNetdisk->ListEntry );
		EnabledNetdisk_Close( enabledNetdisk );
		return NULL;
	}

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_Create: driveLayoutInformation->PartitionCount = %d\n", 
				    driveLayoutInformation->PartitionCount) );

	enabledNetdisk->DummyNetdiskPartition.EnabledNetdisk	= enabledNetdisk;
	enabledNetdisk->DummyNetdiskPartition.Flags				= NETDISK_PARTITION_FLAG_ENABLED;
	enabledNetdisk->DummyNetdiskPartition.NetdiskPartitionInformation.NetdiskInformation = enabledNetdisk->NetdiskInformation;

	KeInitializeSpinLock( &enabledNetdisk->DummyNetdiskPartition.PrimarySessionQSpinLock );
	InitializeListHead( &enabledNetdisk->DummyNetdiskPartition.PrimarySessionQueue );

	for (partitionIndex = 0; partitionIndex < driveLayoutInformation->PartitionCount; partitionIndex++) {

		PNETDISK_PARTITION	netdiskPartition;
		NETDISK_ENABLE_MODE	volumeIndex;

		if (driveLayoutInformation->PartitionEntry[partitionIndex].PartitionLength.QuadPart == 0)
			continue;

		SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
					   ("EnabledNetdisk_Create: StartingOffset = %I64x, StartingSector = %I64d, Length = %I64x, SectorLength = %I64d PartitionNumber = %d\n", 
						driveLayoutInformation->PartitionEntry[partitionIndex].StartingOffset.QuadPart,
						driveLayoutInformation->PartitionEntry[partitionIndex].StartingOffset.QuadPart / 512,
						driveLayoutInformation->PartitionEntry[partitionIndex].PartitionLength.QuadPart,
						driveLayoutInformation->PartitionEntry[partitionIndex].PartitionLength.QuadPart / 512,
						driveLayoutInformation->PartitionEntry[partitionIndex].PartitionNumber) );
	
		netdiskPartition = NetdiskParttion_Allocate( enabledNetdisk );

		if (netdiskPartition == NULL) {

			break;
		}

		NetdiskPartition_Reference( netdiskPartition );

		netdiskPartition->Flags = NETDISK_PARTITION_FLAG_ENABLED;

		netdiskPartition->NetdiskPartitionInformation.NetdiskInformation = enabledNetdisk->NetdiskInformation;
		netdiskPartition->NetdiskPartitionInformation.PartitionInformation = driveLayoutInformation->PartitionEntry[partitionIndex];

		for (volumeIndex = 0; volumeIndex < (NETDISK_SECONDARY2PRIMARY+1); volumeIndex++) {

			netdiskPartition->NetdiskVolume[volumeIndex].VolumeState	= VolumeEnabled;
			netdiskPartition->NetdiskVolume[volumeIndex].LfsDeviceExt	= NULL;
		}

		ExInterlockedInsertHeadList( &enabledNetdisk->NetdiskPartitionQueue,
									 &netdiskPartition->ListEntry,
									 &enabledNetdisk->NetdiskPartitionQSpinLock );
	
		NetdiskPartition_Dereference( netdiskPartition );
	}

	InterlockedExchangePointer( &enabledNetdisk->DriveLayoutInformation, driveLayoutInformation );
	
} else {

	NTSTATUS						ntStatus;
	PDRIVE_LAYOUT_INFORMATION_EX	driveLayoutInformationEx;
	ULONG							driveLayoutInformationSize = 0;
	ULONG							partitionIndex;


	enabledNetdisk = ExAllocatePoolWithTag( NonPagedPool, 
											sizeof(ENABLED_NETDISK),
											NETDISK_MANAGER_ENABLED_TAG );
	
	if (enabledNetdisk == NULL) {

		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
		return NULL;
	}

	NetdiskManager_Reference( NetdiskManager );

	RtlZeroMemory( enabledNetdisk, sizeof(ENABLED_NETDISK) );
	

	//KeInitializeSpinLock( &enabledNetdisk->SpinLock );
	ExInitializeFastMutex( &enabledNetdisk->FastMutex );
	enabledNetdisk->ReferenceCount = 1;

	ExInitializeResourceLite( &enabledNetdisk->Resource );

	InitializeListHead( &enabledNetdisk->ListEntry );
	enabledNetdisk->NetdiskManager = NetdiskManager;

	KeInitializeSpinLock( &enabledNetdisk->NetdiskPartitionQSpinLock );
	InitializeListHead( &enabledNetdisk->NetdiskPartitionQueue );

	enabledNetdisk->NetdiskInformation = *NetdiskInformation;
	enabledNetdisk->NetdiskEnableMode = netdiskEnableMode;

	while (1) {

		if (driveLayoutInformationSize == 0) {

			driveLayoutInformationSize = sizeof(DRIVE_LAYOUT_INFORMATION_EX) + 
										 sizeof(PARTITION_INFORMATION_EX) * 7;
		
		} else {
	
			driveLayoutInformationSize += sizeof(PARTITION_INFORMATION_EX); 
		}

		driveLayoutInformationEx = ExAllocatePoolWithTag( NonPagedPool, 
													      driveLayoutInformationSize,
														  NETDISK_MANAGER_TAG );

		if (driveLayoutInformationEx == NULL) {

			NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
			return enabledNetdisk;
		}
		
		ntStatus = LfsFilterDeviceIoControl( VolumeDeviceObject,
											 IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
											 NULL,
											 0,
											 driveLayoutInformationEx,
											 driveLayoutInformationSize,
											 NULL );

		if (ntStatus == STATUS_SUCCESS) {

			break;
		}
		
		ExFreePoolWithTag( driveLayoutInformationEx,
						   NETDISK_MANAGER_TAG );

		if (ntStatus == STATUS_BUFFER_TOO_SMALL) {

			continue;
		}

		NDAS_ASSERT( FALSE );
		InitializeListHead( &enabledNetdisk->ListEntry );
		EnabledNetdisk_Close( enabledNetdisk );
		return NULL;
	}

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_Create: driveLayoutInformation->PartitionCount = %d\n", 
				    driveLayoutInformationEx->PartitionCount) );

	enabledNetdisk->DummyNetdiskPartition.EnabledNetdisk	= enabledNetdisk;
	enabledNetdisk->DummyNetdiskPartition.Flags				= NETDISK_PARTITION_FLAG_ENABLED;
	enabledNetdisk->DummyNetdiskPartition.NetdiskPartitionInformation.NetdiskInformation = 
		enabledNetdisk->NetdiskInformation;

	KeInitializeSpinLock( &enabledNetdisk->DummyNetdiskPartition.PrimarySessionQSpinLock );
	InitializeListHead( &enabledNetdisk->DummyNetdiskPartition.PrimarySessionQueue );

	for (partitionIndex = 0; partitionIndex < driveLayoutInformationEx->PartitionCount; partitionIndex++) {

		PNETDISK_PARTITION	netdiskPartition;
		NETDISK_ENABLE_MODE	volumeIndex;

		if (driveLayoutInformationEx->PartitionEntry[partitionIndex].PartitionLength.QuadPart == 0)
			continue;

		SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
					   ("EnabledNetdisk_Create: StartingOffset = %I64x, StartingSector = %I64d, Length = %I64x, SectorLength = %I64d PartitionNumber = %d\n", 
						driveLayoutInformationEx->PartitionEntry[partitionIndex].StartingOffset.QuadPart,
						driveLayoutInformationEx->PartitionEntry[partitionIndex].StartingOffset.QuadPart / 512,
						driveLayoutInformationEx->PartitionEntry[partitionIndex].PartitionLength.QuadPart,
						driveLayoutInformationEx->PartitionEntry[partitionIndex].PartitionLength.QuadPart / 512,
						driveLayoutInformationEx->PartitionEntry[partitionIndex].PartitionNumber) );
	
		netdiskPartition = NetdiskParttion_Allocate( enabledNetdisk );

		if (netdiskPartition == NULL) {

			break;
		}

		NetdiskPartition_Reference( netdiskPartition );

		netdiskPartition->Flags = NETDISK_PARTITION_FLAG_ENABLED;

		netdiskPartition->NetdiskPartitionInformation.NetdiskInformation
			= enabledNetdisk->NetdiskInformation;
		netdiskPartition->NetdiskPartitionInformation.PartitionInformationEx 
			= driveLayoutInformationEx->PartitionEntry[partitionIndex];

		for (volumeIndex = 0; volumeIndex < (NETDISK_SECONDARY2PRIMARY+1); volumeIndex++) {

			netdiskPartition->NetdiskVolume[volumeIndex].VolumeState	= VolumeEnabled;
			netdiskPartition->NetdiskVolume[volumeIndex].LfsDeviceExt	= NULL;
		}

		ExInterlockedInsertHeadList( &enabledNetdisk->NetdiskPartitionQueue,
									 &netdiskPartition->ListEntry,
									 &enabledNetdisk->NetdiskPartitionQSpinLock );
	
		NetdiskPartition_Dereference( netdiskPartition );
	}

	InterlockedExchangePointer( &enabledNetdisk->DriveLayoutInformation, driveLayoutInformationEx );
}

	if (netdiskEnableMode == NETDISK_SECONDARY) {
	
		NETDISK_PARTITION_INFO	netdiskPartitionInfo;
	
		RtlCopyMemory( &netdiskPartitionInfo.NetDiskAddress,
					   &enabledNetdisk->NetdiskInformation.NetDiskAddress,
					   sizeof(netdiskPartitionInfo.NetDiskAddress) );

		netdiskPartitionInfo.UnitDiskNo = enabledNetdisk->NetdiskInformation.UnitDiskNo;

		RtlCopyMemory( netdiskPartitionInfo.NdscId,
					   enabledNetdisk->NetdiskInformation.NdscId,
					   NDSC_ID_LENGTH );

		netdiskPartitionInfo.StartingOffset.QuadPart = 0;

		LfsTable_InsertNetDiskPartitionInfoUser( GlobalLfs.LfsTable,
												 &netdiskPartitionInfo,
												 &enabledNetdisk->NetdiskInformation.BindAddress,
												 FALSE );

		SetFlag( enabledNetdisk->Flags, ENABLED_NETDISK_INSERT_DISK_INFORMATION );
	} 

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_Create enabledNetdisk = %p\n", 
				     enabledNetdisk) );

	return enabledNetdisk;
}


BOOLEAN
EnabledNetdisk_Update (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PDEVICE_OBJECT		VolumeDeviceObject,
	IN PDEVICE_OBJECT		DiskDeviceObject,
	IN PNETDISK_INFORMATION	NetdiskInformation
	)
{
	NETDISK_ENABLE_MODE	netdiskEnableMode;

	UNREFERENCED_PARAMETER( DiskDeviceObject );

	if (NetdiskInformation->DeviceMode == DEVMODE_SHARED_READWRITE) {

		if (!FlagOn(NetdiskInformation->EnabledFeatures, NDASFEATURE_SECONDARY)) {

			netdiskEnableMode = NETDISK_PRIMARY;

		} else {

			netdiskEnableMode = NETDISK_SECONDARY;
		}

	} else if (NetdiskInformation->DeviceMode == DEVMODE_SHARED_READONLY) {

		netdiskEnableMode = NETDISK_READ_ONLY;

	} else {

		NDAS_ASSERT( FALSE );
		netdiskEnableMode = NETDISK_UNKNOWN_MODE;
	}

if (IS_WINDOWS2K()) {

	NTSTATUS					ntStatus;
	PDRIVE_LAYOUT_INFORMATION	driveLayoutInformation;
	PDRIVE_LAYOUT_INFORMATION	oldDriveLayoutInformation;	
	ULONG						driveLayoutInformationSize;
	ULONG						partitionIndex;


	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &EnabledNetdisk->Resource, TRUE );

	switch (EnabledNetdisk->NetdiskEnableMode) {

	case NETDISK_READ_ONLY:
			
		NDAS_ASSERT( netdiskEnableMode == NETDISK_READ_ONLY );

		break;

	case NETDISK_SECONDARY:
			
		if (netdiskEnableMode == NETDISK_PRIMARY) {

			EnabledNetdisk->NetdiskInformation = *NetdiskInformation;
			EnabledNetdisk->NetdiskEnableMode = NETDISK_SECONDARY2PRIMARY;
		
		} else
			ASSERT( netdiskEnableMode == NETDISK_SECONDARY );

		break;

	case NETDISK_PRIMARY:

		NDAS_ASSERT( netdiskEnableMode == NETDISK_PRIMARY );

		break;

	case NETDISK_SECONDARY2PRIMARY:
			
		NDAS_ASSERT( netdiskEnableMode == NETDISK_PRIMARY || netdiskEnableMode == NETDISK_SECONDARY );

		if (netdiskEnableMode == NETDISK_SECONDARY) { // changed by other volume
		
			*NetdiskInformation = EnabledNetdisk->NetdiskInformation;
			netdiskEnableMode = NETDISK_SECONDARY2PRIMARY;
		}

		break;

	default:

		NDAS_ASSERT( LFS_BUG );
		break;
	}

	if (EnabledNetdisk->DriveLayoutInformation) {

		driveLayoutInformationSize = FIELD_OFFSET(DRIVE_LAYOUT_INFORMATION, PartitionEntry) + 
									 EnabledNetdisk->DriveLayoutInformation->PartitionCount * sizeof(PARTITION_INFORMATION);
	
	} else {
	
		NDAS_ASSERT( FALSE );

		driveLayoutInformationSize = FIELD_OFFSET(DRIVE_LAYOUT_INFORMATION, PartitionEntry)
									 + 8 * sizeof(PARTITION_INFORMATION);
	}

	while (1) {

		driveLayoutInformation = ExAllocatePoolWithTag( NonPagedPool, 
														driveLayoutInformationSize,
														NETDISK_MANAGER_TAG );

		if (driveLayoutInformation == NULL) {

			NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
			ExReleaseResourceLite( &EnabledNetdisk->Resource );
			KeLeaveCriticalRegion();
			return FALSE;
		}
		
		ntStatus = LfsFilterDeviceIoControl( VolumeDeviceObject,
											 IOCTL_DISK_GET_DRIVE_LAYOUT,
											 NULL,
											 0,
											 driveLayoutInformation,
											 driveLayoutInformationSize,
											 NULL );

		if (ntStatus == STATUS_SUCCESS) {

			break;
		}
		
		ExFreePoolWithTag( driveLayoutInformation, NETDISK_MANAGER_TAG );

		if (ntStatus == STATUS_BUFFER_TOO_SMALL) {

			driveLayoutInformationSize += sizeof(PARTITION_INFORMATION); 
			continue;
		}

		NDAS_ASSERT( FALSE );

		ExReleaseResourceLite( &EnabledNetdisk->Resource );
		KeLeaveCriticalRegion();

		return FALSE;
	}

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_Update: driveLayoutInformation->PartitionCount = %d\n", 
					 driveLayoutInformation->PartitionCount) );

	for (partitionIndex = 0; partitionIndex < driveLayoutInformation->PartitionCount; partitionIndex++) {

		PNETDISK_PARTITION		netdiskPartition;
		NETDISK_ENABLE_MODE		volumeIndex;
		PPARTITION_INFORMATION	partitionInformation;
	
	
		partitionInformation = &driveLayoutInformation->PartitionEntry[partitionIndex];

		if (partitionInformation->PartitionLength.QuadPart == 0)
			continue;

		netdiskPartition = EnabledNetdisk_LookupNetdiskPartition( EnabledNetdisk,
																  NULL,
																  &partitionInformation->StartingOffset,
																  NULL );

		if (netdiskPartition) {

			KIRQL				oldIrql;
			//NETDISK_ENABLE_MODE	volumeIndex2;

			if (netdiskPartition->NetdiskPartitionInformation.PartitionInformation.PartitionLength.QuadPart == 
				partitionInformation->PartitionLength.QuadPart) {

				netdiskPartition->NetdiskPartitionInformation.PartitionInformation = *partitionInformation;
				
				NetdiskPartition_Dereference( netdiskPartition );	// by EnabledNetdisk_LookupNetdiskPartition
				continue;
			}

#if 1

			if (netdiskPartition->ScsiportAdapterDeviceObjectReferenceCount) {

				SetFlag( netdiskPartition->Flags, NETDISK_PARTITION_FLAG_ALREADY_DELETED );

				netdiskPartition->NetdiskPartitionInformation.PartitionInformation.StartingOffset.QuadPart = -1;
				NetdiskPartition_Dereference( netdiskPartition );	// by EnabledNetdisk_LookupNetdiskPartition
			
			} else {

				KeAcquireSpinLock( &EnabledNetdisk->NetdiskPartitionQSpinLock, &oldIrql );
				RemoveEntryList( &netdiskPartition->ListEntry );
				KeReleaseSpinLock(&EnabledNetdisk->NetdiskPartitionQSpinLock, oldIrql);
			
				InitializeListHead( &netdiskPartition->ListEntry );

				NDAS_ASSERT( netdiskPartition->ScsiportAdapterDeviceObject == NULL );
				NDAS_ASSERT( netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject == NULL );

				NetdiskPartition_Dereference( netdiskPartition );	// close It
				NetdiskPartition_Dereference( netdiskPartition );	// by EnabledNetdisk_LookupNetdiskPartition
			}

#else

			for (volumeIndex2 = 0; volumeIndex2 < (NETDISK_SECONDARY2PRIMARY+1); volumeIndex2++) {

				if (netdiskPartition->NetdiskVolume[volumeIndex2].LfsDeviceExt) {
					
					if (!(EnabledNetdisk->NetdiskEnableMode == NETDISK_SECONDARY2PRIMARY && volumeIndex2 == NETDISK_SECONDARY)) {

						NDAS_ASSERT( FALSE ); 
					}

					break;

					((PLFS_DEVICE_EXTENSION)(netdiskPartition->NetdiskVolume[volumeIndex2].LfsDeviceExt))->NetdiskPartition = NULL;				

					EnabledNetdisk_SurpriseRemoval( EnabledNetdisk, 
													netdiskPartition,
													volumeIndex2 );

				} 
			}

			if (volumeIndex2 != NETDISK_SECONDARY2PRIMARY) {

				SetFlag( netdiskPartition->Flags, NETDISK_PARTITION_FLAG_ALREADY_DELETED );

				netdiskPartition->NetdiskPartitionInformation.PartitionInformation.StartingOffset.QuadPart = -1;
				NetdiskPartition_Dereference( netdiskPartition );	// by EnabledNetdisk_LookupNetdiskPartition
			
			} else {

				KeAcquireSpinLock( &EnabledNetdisk->NetdiskPartitionQSpinLock, &oldIrql );
				RemoveEntryList( &netdiskPartition->ListEntry );
				KeReleaseSpinLock(&EnabledNetdisk->NetdiskPartitionQSpinLock, oldIrql);
			
				InitializeListHead( &netdiskPartition->ListEntry );

				NDAS_ASSERT( netdiskPartition->ScsiportAdapterDeviceObject == NULL );
				NDAS_ASSERT( netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject == NULL );

				NetdiskPartition_Dereference( netdiskPartition );	// close It
				NetdiskPartition_Dereference( netdiskPartition );	// by EnabledNetdisk_LookupNetdiskPartition
			}

#endif

			netdiskPartition = NULL;
		}

		SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
					   ("EnabledNetdisk_Update: StartingOffset = %I64x, PartitionNumber = %d\n", 
						 driveLayoutInformation->PartitionEntry[partitionIndex].StartingOffset.QuadPart,
						 driveLayoutInformation->PartitionEntry[partitionIndex].PartitionNumber) );

		netdiskPartition = NetdiskParttion_Allocate( EnabledNetdisk );

		if (netdiskPartition == NULL) {

			ExReleaseResourceLite( &EnabledNetdisk->Resource );
			KeLeaveCriticalRegion();
			return FALSE;
		}

		netdiskPartition->Flags = NETDISK_PARTITION_FLAG_ENABLED;

		netdiskPartition->NetdiskPartitionInformation.NetdiskInformation = EnabledNetdisk->NetdiskInformation;
		netdiskPartition->NetdiskPartitionInformation.PartitionInformation = 
			driveLayoutInformation->PartitionEntry[partitionIndex];

		for (volumeIndex = 0; volumeIndex < (NETDISK_SECONDARY2PRIMARY+1); volumeIndex++) {

			netdiskPartition->NetdiskVolume[volumeIndex].VolumeState	= VolumeEnabled;
			netdiskPartition->NetdiskVolume[volumeIndex].LfsDeviceExt	= NULL;
		}

		ExInterlockedInsertHeadList( &EnabledNetdisk->NetdiskPartitionQueue,
									 &netdiskPartition->ListEntry,
									 &EnabledNetdisk->NetdiskPartitionQSpinLock );
	}

	oldDriveLayoutInformation = InterlockedExchangePointer( &EnabledNetdisk->DriveLayoutInformation, 
															driveLayoutInformation );
	if (oldDriveLayoutInformation) {

		ExFreePoolWithTag( oldDriveLayoutInformation, NETDISK_MANAGER_TAG );
	
	} else {
	
		NDAS_ASSERT( FALSE );
	}

	ExReleaseResourceLite( &EnabledNetdisk->Resource );
	KeLeaveCriticalRegion();

	return TRUE;

} else {

	NTSTATUS						ntStatus;
	PDRIVE_LAYOUT_INFORMATION_EX	driveLayoutInformationEx;
	PDRIVE_LAYOUT_INFORMATION_EX	oldDriveLayoutInformationEx;	
	ULONG							driveLayoutInformationSize;
	ULONG							partitionIndex;

	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &EnabledNetdisk->Resource, TRUE );

	switch (EnabledNetdisk->NetdiskEnableMode) {

	case NETDISK_READ_ONLY:
			
		NDAS_ASSERT( netdiskEnableMode == NETDISK_READ_ONLY );

		break;

	case NETDISK_SECONDARY:
			
		if (netdiskEnableMode == NETDISK_PRIMARY) {

			EnabledNetdisk->NetdiskInformation = *NetdiskInformation;
			EnabledNetdisk->NetdiskEnableMode = NETDISK_SECONDARY2PRIMARY;
		
		} else
			ASSERT( netdiskEnableMode == NETDISK_SECONDARY );

		break;

	case NETDISK_PRIMARY:

		NDAS_ASSERT( netdiskEnableMode == NETDISK_PRIMARY );

		break;

	case NETDISK_SECONDARY2PRIMARY:
			
		NDAS_ASSERT( netdiskEnableMode == NETDISK_PRIMARY || netdiskEnableMode == NETDISK_SECONDARY );

		if (netdiskEnableMode == NETDISK_SECONDARY) { // changed by other volume
		
			*NetdiskInformation = EnabledNetdisk->NetdiskInformation;
			netdiskEnableMode = NETDISK_SECONDARY2PRIMARY;
		}

		break;

	default:

		NDAS_ASSERT( LFS_BUG );
		break;
	}

	if (EnabledNetdisk->DriveLayoutInformation) {

		driveLayoutInformationSize = FIELD_OFFSET(DRIVE_LAYOUT_INFORMATION_EX, PartitionEntry) + 
									 EnabledNetdisk->DriveLayoutInformationEx->PartitionCount * sizeof(PARTITION_INFORMATION_EX);
	
	} else {
	
		NDAS_ASSERT( FALSE );

		driveLayoutInformationSize = FIELD_OFFSET(DRIVE_LAYOUT_INFORMATION_EX, PartitionEntry)
									 + 8 * sizeof(PARTITION_INFORMATION_EX);
	}

	while (1) {

		driveLayoutInformationEx = ExAllocatePoolWithTag( NonPagedPool, 
														  driveLayoutInformationSize,
														  NETDISK_MANAGER_TAG );

		if (driveLayoutInformationEx == NULL) {

			NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
			ExReleaseResourceLite( &EnabledNetdisk->Resource );
			KeLeaveCriticalRegion();
			return FALSE;
		}
		
		ntStatus = LfsFilterDeviceIoControl( VolumeDeviceObject,
											 IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
											 NULL,
											 0,
											 driveLayoutInformationEx,
											 driveLayoutInformationSize,
											 NULL );

		if (ntStatus == STATUS_SUCCESS) {

			break;
		}
		
		ExFreePoolWithTag( driveLayoutInformationEx, NETDISK_MANAGER_TAG );

		if (ntStatus == STATUS_BUFFER_TOO_SMALL) {

			driveLayoutInformationSize += sizeof(PARTITION_INFORMATION_EX); 
			continue;
		}

		NDAS_ASSERT( FALSE );

		ExReleaseResourceLite( &EnabledNetdisk->Resource );
		KeLeaveCriticalRegion();

		return FALSE;
	}

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_Update: driveLayoutInformationEx->PartitionCount = %d\n", 
					 driveLayoutInformationEx->PartitionCount) );

	for (partitionIndex = 0; partitionIndex < driveLayoutInformationEx->PartitionCount; partitionIndex++) {

		PNETDISK_PARTITION			netdiskPartition;
		NETDISK_ENABLE_MODE			volumeIndex;
		PPARTITION_INFORMATION_EX	partitionInformationEx;
	
	
		partitionInformationEx = &driveLayoutInformationEx->PartitionEntry[partitionIndex];

		if (partitionInformationEx->PartitionLength.QuadPart == 0)
			continue;

		netdiskPartition = EnabledNetdisk_LookupNetdiskPartition( EnabledNetdisk,
																  NULL,
																  &partitionInformationEx->StartingOffset,
																  NULL );

		if (netdiskPartition) {

			KIRQL				oldIrql;
			//NETDISK_ENABLE_MODE	volumeIndex2;

			if (netdiskPartition->NetdiskPartitionInformation.PartitionInformationEx.PartitionLength.QuadPart == 
				partitionInformationEx->PartitionLength.QuadPart) {

				netdiskPartition->NetdiskPartitionInformation.PartitionInformationEx = *partitionInformationEx;
				
				NetdiskPartition_Dereference( netdiskPartition );	// by EnabledNetdisk_LookupNetdiskPartition
				continue;
			}

#if 1

			if (netdiskPartition->ScsiportAdapterDeviceObjectReferenceCount) {

				SetFlag( netdiskPartition->Flags, NETDISK_PARTITION_FLAG_ALREADY_DELETED );

				netdiskPartition->NetdiskPartitionInformation.PartitionInformation.StartingOffset.QuadPart = -1;
				NetdiskPartition_Dereference( netdiskPartition );	// by EnabledNetdisk_LookupNetdiskPartition
			
			} else {

				KeAcquireSpinLock( &EnabledNetdisk->NetdiskPartitionQSpinLock, &oldIrql );
				RemoveEntryList( &netdiskPartition->ListEntry );
				KeReleaseSpinLock(&EnabledNetdisk->NetdiskPartitionQSpinLock, oldIrql);
			
				InitializeListHead( &netdiskPartition->ListEntry );

				NDAS_ASSERT( netdiskPartition->ScsiportAdapterDeviceObject == NULL );
				NDAS_ASSERT( netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject == NULL );

				NetdiskPartition_Dereference( netdiskPartition );	// close It
				NetdiskPartition_Dereference( netdiskPartition );	// by EnabledNetdisk_LookupNetdiskPartition
			}

#else

			for (volumeIndex2 = 0; volumeIndex2 < (NETDISK_SECONDARY2PRIMARY+1); volumeIndex2++) {

				if (netdiskPartition->NetdiskVolume[volumeIndex2].LfsDeviceExt) {
					
					if (!(EnabledNetdisk->NetdiskEnableMode == NETDISK_SECONDARY2PRIMARY && volumeIndex2 == NETDISK_SECONDARY)) {

						NDAS_ASSERT( FALSE ); 
					}

					break;

					((PLFS_DEVICE_EXTENSION)(netdiskPartition->NetdiskVolume[volumeIndex2].LfsDeviceExt))->NetdiskPartition = NULL;				

					EnabledNetdisk_SurpriseRemoval( EnabledNetdisk, 
													netdiskPartition,
													volumeIndex2 );

				} 
			}

			if (volumeIndex2 != NETDISK_SECONDARY2PRIMARY) {

				SetFlag( netdiskPartition->Flags, NETDISK_PARTITION_FLAG_ALREADY_DELETED );

				netdiskPartition->NetdiskPartitionInformation.PartitionInformation.StartingOffset.QuadPart = -1;
				NetdiskPartition_Dereference( netdiskPartition );	// by EnabledNetdisk_LookupNetdiskPartition
			
			} else {

				KeAcquireSpinLock( &EnabledNetdisk->NetdiskPartitionQSpinLock, &oldIrql );
				RemoveEntryList( &netdiskPartition->ListEntry );
				KeReleaseSpinLock(&EnabledNetdisk->NetdiskPartitionQSpinLock, oldIrql);
			
				InitializeListHead( &netdiskPartition->ListEntry );

				NDAS_ASSERT( netdiskPartition->ScsiportAdapterDeviceObject == NULL );
				NDAS_ASSERT( netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject == NULL );

				NetdiskPartition_Dereference( netdiskPartition );	// close It
				NetdiskPartition_Dereference( netdiskPartition );	// by EnabledNetdisk_LookupNetdiskPartition
			}

#endif

			netdiskPartition = NULL;
		}

		SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
					   ("EnabledNetdisk_Update: StartingOffset = %I64x, PartitionNumber = %d\n", 
						 driveLayoutInformationEx->PartitionEntry[partitionIndex].StartingOffset.QuadPart,
						 driveLayoutInformationEx->PartitionEntry[partitionIndex].PartitionNumber) );

		netdiskPartition = NetdiskParttion_Allocate( EnabledNetdisk );

		if (netdiskPartition == NULL) {

			ExReleaseResourceLite( &EnabledNetdisk->Resource );
			KeLeaveCriticalRegion();
			return FALSE;
		}

		netdiskPartition->Flags = NETDISK_PARTITION_FLAG_ENABLED;

		netdiskPartition->NetdiskPartitionInformation.NetdiskInformation = EnabledNetdisk->NetdiskInformation;
		netdiskPartition->NetdiskPartitionInformation.PartitionInformationEx = 
			driveLayoutInformationEx->PartitionEntry[partitionIndex];

		for (volumeIndex = 0; volumeIndex < (NETDISK_SECONDARY2PRIMARY+1); volumeIndex++) {

			netdiskPartition->NetdiskVolume[volumeIndex].VolumeState	= VolumeEnabled;
			netdiskPartition->NetdiskVolume[volumeIndex].LfsDeviceExt	= NULL;
		}

		ExInterlockedInsertHeadList( &EnabledNetdisk->NetdiskPartitionQueue,
									 &netdiskPartition->ListEntry,
									 &EnabledNetdisk->NetdiskPartitionQSpinLock );
	}

	oldDriveLayoutInformationEx = InterlockedExchangePointer( &EnabledNetdisk->DriveLayoutInformationEx, 
															  driveLayoutInformationEx );
	if (oldDriveLayoutInformationEx) {

		ExFreePoolWithTag( oldDriveLayoutInformationEx, NETDISK_MANAGER_TAG );
	
	} else {
	
		NDAS_ASSERT( FALSE );
	}

	ExReleaseResourceLite( &EnabledNetdisk->Resource );
	KeLeaveCriticalRegion();

	return TRUE;
}
}

VOID
EnabledNetdisk_Close (
	IN PENABLED_NETDISK	EnabledNetdisk
	)
{
	PLIST_ENTRY	netdiskPartitionListEntry;

	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &EnabledNetdisk->Resource, TRUE );

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_Close: EnabledNetdisk = %p\n", EnabledNetdisk) );

	NDAS_ASSERT( EnabledNetdisk->ListEntry.Flink == EnabledNetdisk->ListEntry.Blink );

	if (FlagOn(EnabledNetdisk->Flags, ENABLED_NETDISK_INSERT_DISK_INFORMATION)) {
		
		NETDISK_PARTITION_INFO	netdiskPartitionInfo;

		NDAS_ASSERT( EnabledNetdisk->NetdiskEnableMode == NETDISK_SECONDARY ||
					   EnabledNetdisk->NetdiskEnableMode == NETDISK_SECONDARY2PRIMARY );
			
		RtlCopyMemory( &netdiskPartitionInfo.NetDiskAddress,
					   &EnabledNetdisk->NetdiskInformation.NetDiskAddress,
					   sizeof(netdiskPartitionInfo.NetDiskAddress) );

		netdiskPartitionInfo.UnitDiskNo = EnabledNetdisk->NetdiskInformation.UnitDiskNo;

		RtlCopyMemory( netdiskPartitionInfo.NdscId,
					   EnabledNetdisk->NetdiskInformation.NdscId,
					   NDSC_ID_LENGTH );

		netdiskPartitionInfo.StartingOffset.QuadPart = 0;

		LfsTable_DeleteNetDiskPartitionInfoUser( GlobalLfs.LfsTable,
												 &netdiskPartitionInfo,
												 FALSE );	

		ClearFlag( EnabledNetdisk->Flags, ENABLED_NETDISK_INSERT_DISK_INFORMATION );
	} 

	InitializeListHead( &EnabledNetdisk->ListEntry );

	while (netdiskPartitionListEntry = ExInterlockedRemoveHeadList(&EnabledNetdisk->NetdiskPartitionQueue,
																   &EnabledNetdisk->NetdiskPartitionQSpinLock)) {

		PNETDISK_PARTITION	netdiskPartition;
		
		netdiskPartition = CONTAINING_RECORD(netdiskPartitionListEntry, NETDISK_PARTITION, ListEntry);
		InitializeListHead( &netdiskPartition->ListEntry );
		
		NetdiskPartition_Dereference( netdiskPartition );	// close it
	}
	
	ExReleaseResourceLite( &EnabledNetdisk->Resource );
	KeLeaveCriticalRegion();

	EnabledNetdisk_Dereference( EnabledNetdisk ); // remove creation reference count
}


VOID
EnabledNetdisk_Reference (
	PENABLED_NETDISK	EnabledNetdisk
	)
{
    LONG result;
	
    result = InterlockedIncrement( &EnabledNetdisk->ReferenceCount );
    ASSERT( result >= 0 );
}


VOID
EnabledNetdisk_Dereference (
	PENABLED_NETDISK	EnabledNetdisk
	)
{
    LONG result;


    result = InterlockedDecrement( &EnabledNetdisk->ReferenceCount );
    
	if (result < 0) {

		ASSERT( LFS_BUG );
		return;
	}

    if (result == 0) {

		PNETDISK_MANAGER	netdiskManager = EnabledNetdisk->NetdiskManager;

		if (IS_WINDOWS2K()) {

			PDRIVE_LAYOUT_INFORMATION driveLayoutInfomation;

			ASSERT( EnabledNetdisk->ListEntry.Flink == EnabledNetdisk->ListEntry.Blink );

			driveLayoutInfomation = InterlockedExchangePointer( &EnabledNetdisk->DriveLayoutInformation, NULL );

			if (driveLayoutInfomation) 
				ExFreePoolWithTag( driveLayoutInfomation, NETDISK_MANAGER_TAG );

		} else {

			PDRIVE_LAYOUT_INFORMATION_EX driveLayoutInfomationEx;

			ASSERT( EnabledNetdisk->ListEntry.Flink == EnabledNetdisk->ListEntry.Blink );

			driveLayoutInfomationEx = InterlockedExchangePointer( &EnabledNetdisk->DriveLayoutInformationEx, NULL );

			if (driveLayoutInfomationEx)
				ExFreePoolWithTag( driveLayoutInfomationEx, NETDISK_MANAGER_TAG );
		}

		ExDeleteResourceLite( &EnabledNetdisk->Resource );
		
		ExFreePoolWithTag( EnabledNetdisk, NETDISK_MANAGER_ENABLED_TAG );

		NetdiskManager_Dereference( netdiskManager );

		SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				       ("EnabledNetdisk_Dereference: EnabledNetdisk = %p\n", EnabledNetdisk) );
	}
}


VOID
EnabledNetdisk_FileSystemShutdown (
	IN PENABLED_NETDISK	EnabledNetdisk
	)
{
	PLIST_ENTRY			netdiskPartitionListEntry;
	KIRQL				oldIrql;
	PNETDISK_PARTITION	netdiskPartition;


	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_FileSystemShutdown: Entered\n") );

	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &EnabledNetdisk->Resource, TRUE );

	ExAcquireFastMutex( &EnabledNetdisk->FastMutex );

	if (FlagOn(EnabledNetdisk->Flags, ENABLED_NETDISK_SHUTDOWN)) {

		ExReleaseFastMutex( &EnabledNetdisk->FastMutex );

		ExReleaseResourceLite( &EnabledNetdisk->Resource );
		KeLeaveCriticalRegion();

		return;
	}

	SetFlag( EnabledNetdisk->Flags, ENABLED_NETDISK_SHUTDOWN );
	ExReleaseFastMutex( &EnabledNetdisk->FastMutex );

	do {

		KeAcquireSpinLock( &EnabledNetdisk->NetdiskPartitionQSpinLock, &oldIrql );

		for (netdiskPartition = NULL, netdiskPartitionListEntry = EnabledNetdisk->NetdiskPartitionQueue.Flink;
			 netdiskPartitionListEntry != &EnabledNetdisk->NetdiskPartitionQueue;
			 netdiskPartition = NULL, netdiskPartitionListEntry = netdiskPartitionListEntry->Flink) {

			netdiskPartition = CONTAINING_RECORD (netdiskPartitionListEntry, NETDISK_PARTITION, ListEntry);
		
			if (!FlagOn(netdiskPartition->Flags, NETDISK_PARTITION_FLAG_SHUTDOWN)) {

				NetdiskPartition_Reference( netdiskPartition );
				break;
			}
		}

		KeReleaseSpinLock( &EnabledNetdisk->NetdiskPartitionQSpinLock, oldIrql );

		if (netdiskPartition) {

			ExAcquireFastMutex( &netdiskPartition->FastMutex );
			SetFlag( netdiskPartition->Flags, NETDISK_PARTITION_FLAG_SHUTDOWN );
			ExReleaseFastMutex( &netdiskPartition->FastMutex );

			NetdiskPartition_Dereference( netdiskPartition );
		}

	} while (netdiskPartition);

	ExReleaseResourceLite( &EnabledNetdisk->Resource );
	KeLeaveCriticalRegion();

	return;
}


VOID
SpyGetObjectName (
    IN PVOID Object,
    IN OUT PUNICODE_STRING Name
    )
/*++

Routine Description:

    This routine will return the name of the given object.
    If a name can not be found an empty string will be returned.

Arguments:

    Object - The object whose name we want

    Name - A unicode string that is already initialized with a buffer

Return Value:

    None

--*/
{
    NTSTATUS status;
    CHAR nibuf[512];        //buffer that receives NAME information and name
    POBJECT_NAME_INFORMATION nameInfo = (POBJECT_NAME_INFORMATION)nibuf;
    ULONG retLength;

    PAGED_CODE();

    status = ObQueryNameString( Object, 
                                nameInfo, 
                                sizeof(nibuf), 
                                &retLength );

    //
    //  Init current length, if we have an error a NULL string will be returned
    //

    Name->Length = 0;

    if (NT_SUCCESS( status )) {

        //
        //  Copy what we can of the name string
        //

        RtlCopyUnicodeString( Name, &nameInfo->Name );
    }
}


NTSTATUS
EnabledNetdisk_PreMountVolume (
	IN  PENABLED_NETDISK		EnabledNetdisk,
	IN  BOOLEAN					ForceIndirect,
	IN  PDEVICE_OBJECT			VolumeDeviceObject,
	IN  PDEVICE_OBJECT			DiskDeviceObject,
	IN  PDEVICE_OBJECT			*ScsiportAdapterDeviceObject,
	OUT PNETDISK_PARTITION		*NetdiskPartition,
	OUT	PNETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
if (IS_WINDOWS2K()) {

	NTSTATUS				status;
	PNETDISK_PARTITION		netdiskPartition;
	PARTITION_INFORMATION	partitionInformation;
	NETDISK_ENABLE_MODE		netdiskEnableMode;


	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &EnabledNetdisk->Resource, TRUE );

	status = LfsFilterDeviceIoControl( VolumeDeviceObject,
									   IOCTL_DISK_GET_PARTITION_INFO,
									   NULL,
									   0,
									   &partitionInformation,
									   sizeof(PARTITION_INFORMATION),
									   NULL );

	if (status != STATUS_SUCCESS) {
	
		ASSERT( LFS_UNEXPECTED );
		ExReleaseResourceLite( &EnabledNetdisk->Resource );
		KeLeaveCriticalRegion();

		return status;
	}

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_PreMountVolume DiskDeviceObject = %p, partitionInformation.StartingOffset.QuadPart = %I64x\n", 
					 DiskDeviceObject, partitionInformation.StartingOffset.QuadPart) );

	netdiskPartition = EnabledNetdisk_LookupNetdiskPartition( EnabledNetdisk,
															  (PPARTITION_INFORMATION_EX)&partitionInformation,
															  NULL,
															  NULL );

	if (netdiskPartition == NULL) {

		NDAS_ASSERT( LFS_BUG );
		ExReleaseResourceLite( &EnabledNetdisk->Resource );
		KeLeaveCriticalRegion();

		return STATUS_UNSUCCESSFUL;
	}

	netdiskEnableMode = EnabledNetdisk->NetdiskEnableMode;

	NDAS_ASSERT( netdiskPartition->NetdiskVolume[netdiskEnableMode].VolumeState == VolumeEnabled || 
				   netdiskPartition->NetdiskVolume[netdiskEnableMode].VolumeState == VolumeDismounted );

	netdiskPartition->NetdiskPartitionInformation.NetdiskInformation = EnabledNetdisk->NetdiskInformation;
	netdiskPartition->NetdiskVolume[netdiskEnableMode].VolumeState	= VolumePreMounting;

	if (ForceIndirect) {

		SetFlag( netdiskPartition->NetdiskPartitionInformation.Flags, NETDISK_PARTITION_INFORMATION_FLAG_INDIRECT );

	} else {

		if (FlagOn(netdiskPartition->EnabledNetdisk->NetdiskInformation.EnabledFeatures, NDASFEATURE_SIMULTANEOUS_WRITE)) {

			ClearFlag( netdiskPartition->NetdiskPartitionInformation.Flags, NETDISK_PARTITION_INFORMATION_FLAG_INDIRECT );
		
		} else {

			SetFlag( netdiskPartition->NetdiskPartitionInformation.Flags, NETDISK_PARTITION_INFORMATION_FLAG_INDIRECT );
		}
	} 

	*NetdiskEnableMode = netdiskEnableMode;
	*NetdiskPartition = netdiskPartition;

	netdiskPartition->ScsiportAdapterDeviceObjectReferenceCount ++;

	if (netdiskPartition->ScsiportAdapterDeviceObject) {

		NDAS_ASSERT( *ScsiportAdapterDeviceObject == netdiskPartition->ScsiportAdapterDeviceObject );
		NDAS_ASSERT( DiskDeviceObject == netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject );

		ObDereferenceObject( *ScsiportAdapterDeviceObject );

	} else {

		NDAS_ASSERT( netdiskPartition->ScsiportAdapterDeviceObjectReferenceCount == 1 );
		NDAS_ASSERT( DiskDeviceObject && netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject == NULL );

		NDAS_ASSERT( *ScsiportAdapterDeviceObject );
		netdiskPartition->ScsiportAdapterDeviceObject = *ScsiportAdapterDeviceObject;

		netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject = DiskDeviceObject;
		ObReferenceObject( netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject );

		SpyGetObjectName( netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject, 
						  &netdiskPartition->NetdiskPartitionInformation.VolumeName );

		NDAS_ASSERT( netdiskPartition->NetdiskPartitionInformation.VolumeName.Length );
	}

	*ScsiportAdapterDeviceObject = NULL;

	ClearFlag( netdiskPartition->Flags, NETDISK_PARTITION_FLAG_DISMOUNTED );
	SetFlag( netdiskPartition->Flags, NETDISK_PARTITION_FLAG_PREMOUNTING );

	ExReleaseResourceLite( &EnabledNetdisk->Resource );
	KeLeaveCriticalRegion();

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_PreMountVolume DiskDeviceObject = %p, netdiskPartition = %p, partitionInformation.StartingOffset.QuadPart = %I64x\n", 
					 DiskDeviceObject, netdiskPartition, partitionInformation.StartingOffset.QuadPart) );

	return STATUS_SUCCESS;

} else {

	NTSTATUS					status;
	PNETDISK_PARTITION			netdiskPartition;
	PARTITION_INFORMATION_EX	partitionInformationEx;
	NETDISK_ENABLE_MODE			netdiskEnableMode;


	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &EnabledNetdisk->Resource, TRUE );

	status = LfsFilterDeviceIoControl( VolumeDeviceObject,
									   IOCTL_DISK_GET_PARTITION_INFO_EX,
									   NULL,
									   0,
									   &partitionInformationEx,
									   sizeof(PARTITION_INFORMATION_EX),
									   NULL );

	if (status != STATUS_SUCCESS) {
	
		ASSERT( LFS_UNEXPECTED );
		ExReleaseResourceLite( &EnabledNetdisk->Resource );
		KeLeaveCriticalRegion();

		return status;
	}

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_PreMountVolume DiskDeviceObject = %p, partitionInformation.StartingOffset.QuadPart = %I64x\n", 
					 DiskDeviceObject, partitionInformationEx.StartingOffset.QuadPart) );

	netdiskPartition = EnabledNetdisk_LookupNetdiskPartition( EnabledNetdisk,
															  &partitionInformationEx,
															  NULL,
															  NULL );

	if (netdiskPartition == NULL) {

		ASSERT( LFS_BUG );
		ExReleaseResourceLite( &EnabledNetdisk->Resource );
		KeLeaveCriticalRegion();

		return STATUS_UNSUCCESSFUL;
	}

	netdiskEnableMode = EnabledNetdisk->NetdiskEnableMode;

	NDAS_ASSERT( netdiskPartition->NetdiskVolume[netdiskEnableMode].VolumeState == VolumeEnabled || 
				   netdiskPartition->NetdiskVolume[netdiskEnableMode].VolumeState == VolumeDismounted );

	netdiskPartition->NetdiskPartitionInformation.NetdiskInformation = EnabledNetdisk->NetdiskInformation;
	netdiskPartition->NetdiskVolume[netdiskEnableMode].VolumeState	= VolumePreMounting;

	if (ForceIndirect) {

		SetFlag( netdiskPartition->NetdiskPartitionInformation.Flags, NETDISK_PARTITION_INFORMATION_FLAG_INDIRECT );

	} else {

		if (FlagOn(netdiskPartition->EnabledNetdisk->NetdiskInformation.EnabledFeatures, NDASFEATURE_SIMULTANEOUS_WRITE)) {

			ClearFlag( netdiskPartition->NetdiskPartitionInformation.Flags, NETDISK_PARTITION_INFORMATION_FLAG_INDIRECT );

		} else {

			SetFlag( netdiskPartition->NetdiskPartitionInformation.Flags, NETDISK_PARTITION_INFORMATION_FLAG_INDIRECT );
		}
	} 

	*NetdiskEnableMode = netdiskEnableMode;
	*NetdiskPartition = netdiskPartition;

	netdiskPartition->ScsiportAdapterDeviceObjectReferenceCount ++;

	if (netdiskPartition->ScsiportAdapterDeviceObject) {

		NDAS_ASSERT( *ScsiportAdapterDeviceObject == netdiskPartition->ScsiportAdapterDeviceObject );
		NDAS_ASSERT( DiskDeviceObject == netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject );

		ObDereferenceObject( *ScsiportAdapterDeviceObject );

	} else {

		NDAS_ASSERT( netdiskPartition->ScsiportAdapterDeviceObjectReferenceCount == 1 );
		NDAS_ASSERT( DiskDeviceObject && netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject == NULL );

		NDAS_ASSERT( *ScsiportAdapterDeviceObject );
		netdiskPartition->ScsiportAdapterDeviceObject = *ScsiportAdapterDeviceObject;

		netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject = DiskDeviceObject;
		ObReferenceObject( netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject );

		SpyGetObjectName( netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject, 
						  &netdiskPartition->NetdiskPartitionInformation.VolumeName );

		NDAS_ASSERT( netdiskPartition->NetdiskPartitionInformation.VolumeName.Length );
	}

	*ScsiportAdapterDeviceObject = NULL;

	ClearFlag( netdiskPartition->Flags, NETDISK_PARTITION_FLAG_DISMOUNTED );
	SetFlag( netdiskPartition->Flags, NETDISK_PARTITION_FLAG_PREMOUNTING );

	ExReleaseResourceLite( &EnabledNetdisk->Resource );
	KeLeaveCriticalRegion();

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_PreMountVolume DiskDeviceObject = %p, netdiskPartition = %p, partitionInformation.StartingOffset.QuadPart = %I64x\n", 
					 DiskDeviceObject, netdiskPartition, partitionInformationEx.StartingOffset.QuadPart) );

	return STATUS_SUCCESS;
}
}


NTSTATUS
EnabledNetdisk_PostMountVolume (
	IN  PENABLED_NETDISK				EnabledNetdisk,
	IN	PNETDISK_PARTITION				NetdiskPartition,
	IN	NETDISK_ENABLE_MODE				NetdiskEnableMode,
	IN  BOOLEAN							Mount,
	IN	LFS_FILE_SYSTEM_TYPE			FileSystemType,
	IN  PLFS_DEVICE_EXTENSION			LfsDeviceExt,
	OUT	PNETDISK_PARTITION_INFORMATION	NetdiskPartionInformation
	)
{
	NETDISK_PARTITION_INFO		netdiskPartitionInfo;


	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_PostMountVolume: NetdiskPartition = %p, Mount = %d\n", NetdiskPartition, Mount) );

	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &EnabledNetdisk->Resource, TRUE );

	ClearFlag( NetdiskPartition->Flags, NETDISK_PARTITION_FLAG_PREMOUNTING );

	NDAS_ASSERT( NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState == VolumePreMounting ||
				   NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState == VolumeSemiMounted );

	if (Mount == FALSE) {

		NDAS_ASSERT( NetdiskPartition->ScsiportAdapterDeviceObjectReferenceCount );

		NetdiskPartition->ScsiportAdapterDeviceObjectReferenceCount --;

		if (NetdiskPartition->ScsiportAdapterDeviceObjectReferenceCount == 0) {

			ObDereferenceObject( NetdiskPartition->NetdiskPartitionInformation.DiskDeviceObject );
			NetdiskPartition->NetdiskPartitionInformation.DiskDeviceObject = NULL;

			ObDereferenceObject( NetdiskPartition->ScsiportAdapterDeviceObject );
			NetdiskPartition->ScsiportAdapterDeviceObject = NULL;
		}

		NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState	= VolumeEnabled;
		NetdiskPartition_Dereference( NetdiskPartition );
		ExReleaseResourceLite( &EnabledNetdisk->Resource );
		KeLeaveCriticalRegion();

		return STATUS_UNSUCCESSFUL;
	}
	
	NetdiskPartition->FileSystemType = FileSystemType;

	if (NetdiskPartition->NetdiskVolume[NetdiskEnableMode].LfsDeviceExt != NULL) {

		NDAS_ASSERT( FALSE );
	}

	LfsDeviceExt_Reference( LfsDeviceExt );

	NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState	= VolumePostMounting;
	NetdiskPartition->NetdiskVolume[NetdiskEnableMode].LfsDeviceExt	= LfsDeviceExt;
	NetdiskPartition->NetdiskVolume[NetdiskEnableMode].MountStatus	= STATUS_SUCCESS;

	*NetdiskPartionInformation = NetdiskPartition->NetdiskPartitionInformation;
    RtlInitEmptyUnicodeString( &NetdiskPartionInformation->VolumeName,
							   NetdiskPartionInformation->VolumeNameBuffer,
							   sizeof(NetdiskPartionInformation->VolumeNameBuffer) );

	if (RtlAppendUnicodeStringToString(&NetdiskPartionInformation->VolumeName,
									   &NetdiskPartition->NetdiskPartitionInformation.VolumeName) != STATUS_SUCCESS) {

		ASSERT( LFS_UNEXPECTED );
	}

	RtlCopyMemory( &netdiskPartitionInfo.NetDiskAddress,
				   &EnabledNetdisk->NetdiskInformation.NetDiskAddress,
				   sizeof(netdiskPartitionInfo.NetDiskAddress) );

	netdiskPartitionInfo.UnitDiskNo = EnabledNetdisk->NetdiskInformation.UnitDiskNo;

	RtlCopyMemory( netdiskPartitionInfo.NdscId,
				   EnabledNetdisk->NetdiskInformation.NdscId,
				   NDSC_ID_LENGTH );

	if (IS_WINDOWS2K())
		netdiskPartitionInfo.StartingOffset = NetdiskPartition->NetdiskPartitionInformation.PartitionInformation.StartingOffset;
	else
		netdiskPartitionInfo.StartingOffset = NetdiskPartition->NetdiskPartitionInformation.PartitionInformationEx.StartingOffset;

	if (NetdiskEnableMode == NETDISK_SECONDARY) {

		LfsTable_InsertNetDiskPartitionInfoUser( GlobalLfs.LfsTable,
												 &netdiskPartitionInfo,
												 &EnabledNetdisk->NetdiskInformation.BindAddress,
												 FALSE );
	
	} else if (NetdiskEnableMode == NETDISK_PRIMARY || NetdiskEnableMode == NETDISK_SECONDARY2PRIMARY) {

		LfsTable_InsertNetDiskPartitionInfoUser( GlobalLfs.LfsTable,
												 &netdiskPartitionInfo,
												 &EnabledNetdisk->NetdiskInformation.BindAddress,
												 TRUE );
	}

	SetFlag( NetdiskPartition->Flags, NETDISK_PARTITION_FLAG_POSTMOUNTING );

	ExReleaseResourceLite( &EnabledNetdisk->Resource );
	KeLeaveCriticalRegion();

	return STATUS_SUCCESS;
}

VOID
EnabledNetdisk_MountVolumeComplete (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode,
	IN NTSTATUS				MountStatus,
	IN PDEVICE_OBJECT		AttachedToDeviceObject
	)
{
	UNREFERENCED_PARAMETER( EnabledNetdisk );

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_MountVolumeComplete: NetdiskPartition = %p MountStatus = %x\n", NetdiskPartition, MountStatus) );

	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &EnabledNetdisk->Resource, TRUE );

	ASSERT( NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState == VolumePostMounting );

	NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState				= VolumeMounted;
	NetdiskPartition->NetdiskVolume[NetdiskEnableMode].MountStatus				= MountStatus;
	NetdiskPartition->NetdiskVolume[NetdiskEnableMode].AttachedToDeviceObject	= AttachedToDeviceObject;

	if (AttachedToDeviceObject)
		ObReferenceObject( AttachedToDeviceObject );

	ClearFlag( NetdiskPartition->Flags, NETDISK_PARTITION_FLAG_POSTMOUNTING );

	if (NT_SUCCESS(MountStatus)) {

		ClearFlag( NetdiskPartition->Flags, NETDISK_PARTITION_FLAG_MOUNT_CORRUPTED );
		SetFlag( NetdiskPartition->Flags, NETDISK_PARTITION_FLAG_MOUNT_SUCCESS );
	
	} else if (MountStatus == STATUS_FILE_CORRUPT_ERROR || MountStatus == STATUS_DISK_CORRUPT_ERROR) {

		ClearFlag( NetdiskPartition->Flags, NETDISK_PARTITION_FLAG_MOUNT_SUCCESS );
		SetFlag( NetdiskPartition->Flags, NETDISK_PARTITION_FLAG_MOUNT_CORRUPTED );
	}

	SetFlag( NetdiskPartition->Flags, NETDISK_PARTITION_FLAG_MOUNTED );

	ExReleaseResourceLite( &EnabledNetdisk->Resource );
	KeLeaveCriticalRegion();

	return;
}


VOID
EnabledNetdisk_ChangeMode (
	IN		PENABLED_NETDISK		EnabledNetdisk,
	IN		PNETDISK_PARTITION		NetdiskPartition,
	IN OUT	PNETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
	NETDISK_PARTITION_INFO	netdiskPartitionInfo;

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_ChangeMode: NetdiskPartition = %p\n", NetdiskPartition) );

	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &EnabledNetdisk->Resource, TRUE );

	if (NetdiskPartition->NetdiskVolume[NETDISK_SECONDARY].VolumeState == VolumeEnabled) {

		NDAS_ASSERT( GlobalLfs.NdasFsMiniMode == 1 && NetdiskEnableMode == NULL );	
		NDAS_ASSERT( NetdiskPartition->NetdiskVolume[NETDISK_SECONDARY2PRIMARY].VolumeState == VolumeMounted );

	} else {

		NDAS_ASSERT( NetdiskPartition->NetdiskVolume[NETDISK_SECONDARY2PRIMARY].VolumeState == VolumeEnabled );
		NDAS_ASSERT( NetdiskPartition->NetdiskVolume[NETDISK_SECONDARY2PRIMARY].LfsDeviceExt == NULL );

		if (NetdiskEnableMode) {

			NDAS_ASSERT( NetdiskPartition->NetdiskVolume[NETDISK_SECONDARY].VolumeState == VolumeMounted );
	
		} else {

		// called from NdasFat or NdasNtfs

			NDAS_ASSERT( NetdiskPartition->NetdiskVolume[NETDISK_SECONDARY].VolumeState == VolumePostMounting  || 
						   NetdiskPartition->NetdiskVolume[NETDISK_SECONDARY].VolumeState == VolumeMounted		 || 
						   NetdiskPartition->NetdiskVolume[NETDISK_SECONDARY].VolumeState == VolumeStopping );
		}
	
		NetdiskPartition->NetdiskVolume[NETDISK_SECONDARY2PRIMARY] = NetdiskPartition->NetdiskVolume[NETDISK_SECONDARY];
	
		NetdiskPartition->NetdiskVolume[NETDISK_SECONDARY].VolumeState			  = VolumeEnabled;
		NetdiskPartition->NetdiskVolume[NETDISK_SECONDARY].LfsDeviceExt			  = NULL;
		NetdiskPartition->NetdiskVolume[NETDISK_SECONDARY].MountStatus			  = STATUS_NO_SUCH_DEVICE;
		NetdiskPartition->NetdiskVolume[NETDISK_SECONDARY].AttachedToDeviceObject = NULL;
	
		if (NetdiskEnableMode) {

			*NetdiskEnableMode = NETDISK_SECONDARY2PRIMARY;
	
		} else {

			((PLFS_DEVICE_EXTENSION)(NetdiskPartition->NetdiskVolume[NETDISK_SECONDARY2PRIMARY].LfsDeviceExt))->NetdiskEnabledMode = NETDISK_SECONDARY2PRIMARY;
			((PLFS_DEVICE_EXTENSION)(NetdiskPartition->NetdiskVolume[NETDISK_SECONDARY2PRIMARY].LfsDeviceExt))->FilteringMode = LFS_SECONDARY_TO_PRIMARY;
		}

		RtlCopyMemory( &netdiskPartitionInfo.NetDiskAddress,
					   &EnabledNetdisk->NetdiskInformation.NetDiskAddress,
					   sizeof(netdiskPartitionInfo.NetDiskAddress) );

		netdiskPartitionInfo.UnitDiskNo = EnabledNetdisk->NetdiskInformation.UnitDiskNo;

		RtlCopyMemory( netdiskPartitionInfo.NdscId,
					   EnabledNetdisk->NetdiskInformation.NdscId,
					   NDSC_ID_LENGTH );

		if (IS_WINDOWS2K())
			netdiskPartitionInfo.StartingOffset = NetdiskPartition->NetdiskPartitionInformation.PartitionInformation.StartingOffset;
		else
			netdiskPartitionInfo.StartingOffset = NetdiskPartition->NetdiskPartitionInformation.PartitionInformationEx.StartingOffset;

		LfsTable_DeleteNetDiskPartitionInfoUser( GlobalLfs.LfsTable,
												 &netdiskPartitionInfo,
												 FALSE );

		LfsTable_InsertNetDiskPartitionInfoUser( GlobalLfs.LfsTable,
												 &netdiskPartitionInfo,
												 &EnabledNetdisk->NetdiskInformation.BindAddress,
												 TRUE );
	}

	ExReleaseResourceLite( &EnabledNetdisk->Resource );
	KeLeaveCriticalRegion();

	return;
}


VOID
EnabledNetdisk_SurpriseRemoval (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
	NETDISK_PARTITION_INFO	netdiskPartitionInfo;

	
	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_SurpriseRemoval: NetdiskPartition = %p\n", NetdiskPartition) );

	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &EnabledNetdisk->Resource, TRUE );

	if (NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState != VolumeMounted &&
		NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState != VolumeStopped) {
		
		NDAS_ASSERT( FALSE );
		ExReleaseResourceLite( &EnabledNetdisk->Resource );
		KeLeaveCriticalRegion();
		return;
	}

	NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState = VolumeSurpriseRemoved;
	
	if (NetdiskPartition->NetdiskVolume[NetdiskEnableMode].LfsDeviceExt) {

		LfsDeviceExt_Dereference( NetdiskPartition->NetdiskVolume[NetdiskEnableMode].LfsDeviceExt );
		NetdiskPartition->NetdiskVolume[NetdiskEnableMode].LfsDeviceExt = NULL;

		if (NetdiskPartition->NetdiskVolume[NetdiskEnableMode].AttachedToDeviceObject) {

			ObDereferenceObject( NetdiskPartition->NetdiskVolume[NetdiskEnableMode].AttachedToDeviceObject );
			NetdiskPartition->NetdiskVolume[NetdiskEnableMode].AttachedToDeviceObject = NULL;			
		}
	
	} else {

		NDAS_ASSERT( FALSE );
		ExReleaseResourceLite( &EnabledNetdisk->Resource );
		KeLeaveCriticalRegion();
		return;
	}

	RtlCopyMemory( &netdiskPartitionInfo.NetDiskAddress,
				   &EnabledNetdisk->NetdiskInformation.NetDiskAddress,
				   sizeof(netdiskPartitionInfo.NetDiskAddress) );

	netdiskPartitionInfo.UnitDiskNo = EnabledNetdisk->NetdiskInformation.UnitDiskNo;

	RtlCopyMemory( netdiskPartitionInfo.NdscId,
				   EnabledNetdisk->NetdiskInformation.NdscId,
				   NDSC_ID_LENGTH );

	if (IS_WINDOWS2K())
		netdiskPartitionInfo.StartingOffset = NetdiskPartition->NetdiskPartitionInformation.PartitionInformation.StartingOffset;
	else
		netdiskPartitionInfo.StartingOffset = NetdiskPartition->NetdiskPartitionInformation.PartitionInformationEx.StartingOffset;

	if (NetdiskEnableMode == NETDISK_SECONDARY) {

		LfsTable_DeleteNetDiskPartitionInfoUser( GlobalLfs.LfsTable,
												 &netdiskPartitionInfo,
												 FALSE );
	
	} else if(NetdiskEnableMode == NETDISK_PRIMARY || NetdiskEnableMode == NETDISK_SECONDARY2PRIMARY) {

		LfsTable_DeleteNetDiskPartitionInfoUser( GlobalLfs.LfsTable,
												 &netdiskPartitionInfo,
												 TRUE );
	}
	
	ClearFlag( NetdiskPartition->Flags, NETDISK_PARTITION_FLAG_ENABLED );
	SetFlag( NetdiskPartition->Flags, NETDISK_PARTITION_FLAG_SURPRISE_REMOVED );
	
	NDAS_ASSERT( NetdiskPartition->ScsiportAdapterDeviceObjectReferenceCount );

	NetdiskPartition->ScsiportAdapterDeviceObjectReferenceCount --;

	if (NetdiskPartition->ScsiportAdapterDeviceObjectReferenceCount == 0) {

		ObDereferenceObject( NetdiskPartition->ScsiportAdapterDeviceObject );
		NetdiskPartition->ScsiportAdapterDeviceObject = NULL;

		ObDereferenceObject( NetdiskPartition->NetdiskPartitionInformation.DiskDeviceObject );
		NetdiskPartition->NetdiskPartitionInformation.DiskDeviceObject = NULL;
	}

	ExReleaseResourceLite( &EnabledNetdisk->Resource );
	KeLeaveCriticalRegion();

	NetdiskPartition_Dereference( NetdiskPartition );		// by EnabledNetdisk_PreMountVolume

	return;
}

VOID
EnabledNetdisk_QueryRemoveMountVolume (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_QueryRemoveMountVolume: NetdiskPartition = %p\n", NetdiskPartition) );

	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &EnabledNetdisk->Resource, TRUE );

	if (NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState != VolumeMounted &&
		NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState != VolumeStopping) {

		NDAS_ASSERT( FALSE );
		ExReleaseResourceLite( &EnabledNetdisk->Resource );
		KeLeaveCriticalRegion();

		return;
	}

	NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState = VolumeQueryRemove;

	NDAS_ASSERT( NetdiskPartition->ScsiportAdapterDeviceObjectReferenceCount );

	if (NetdiskPartition->ScsiportAdapterDeviceObjectReferenceCount == 1) {

		ObDereferenceObject( NetdiskPartition->ScsiportAdapterDeviceObject );
		NetdiskPartition->ScsiportAdapterDeviceObject = NULL;

		//ObDereferenceObject( NetdiskPartition->NetdiskPartitionInformation.DiskDeviceObject );
		//NetdiskPartition->NetdiskPartitionInformation.DiskDeviceObject = NULL;

		NetdiskPartition->UnreferenceByQueryRemove = TRUE;
	}

	ExReleaseResourceLite( &EnabledNetdisk->Resource );
	KeLeaveCriticalRegion();

	return;
}

VOID
EnabledNetdisk_CancelRemoveMountVolume (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_CancelRemoveMountVolume: NetdiskPartition = %p\n", NetdiskPartition) );

	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &EnabledNetdisk->Resource, TRUE );

	if (NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState != VolumeMounted		&&
		NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState != VolumeQueryRemove &&
		NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState != VolumeStopping) {

		NDAS_ASSERT( FALSE );
		ExReleaseResourceLite( &EnabledNetdisk->Resource );
		KeLeaveCriticalRegion();

		return;
	}

	NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState = VolumeMounted;

	NDAS_ASSERT( NetdiskPartition->ScsiportAdapterDeviceObjectReferenceCount );

	if (NetdiskPartition->UnreferenceByQueryRemove == TRUE) {

		NTSTATUS	status;

		status = GetScsiportAdapter( NetdiskPartition->NetdiskPartitionInformation.DiskDeviceObject, 
									 &NetdiskPartition->ScsiportAdapterDeviceObject );

		if (status != STATUS_SUCCESS) {

			NDAS_ASSERT( FALSE );
		
		} else {

			NetdiskPartition->UnreferenceByQueryRemove = FALSE;
		}
	}

	ExReleaseResourceLite( &EnabledNetdisk->Resource );
	KeLeaveCriticalRegion();

	return;
}

VOID
EnabledNetdisk_DisMountVolume (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
	NETDISK_PARTITION_INFO	netdiskPartitionInfo;
	NETDISK_ENABLE_MODE		volumeIndex2;

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_DisMountVolume: NetdiskPartition = %p\n", NetdiskPartition) );

	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite( &EnabledNetdisk->Resource, TRUE );

	if (NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState != VolumeMounted		&&
		NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState != VolumeQueryRemove &&
		NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState != VolumeStopped) {

		NDAS_ASSERT( FALSE );
		ExReleaseResourceLite( &EnabledNetdisk->Resource );
		KeLeaveCriticalRegion();

		return;
	}

	NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState = VolumeDismounted;

	if (NetdiskPartition->NetdiskVolume[NetdiskEnableMode].LfsDeviceExt) {

		LfsDeviceExt_Dereference( NetdiskPartition->NetdiskVolume[NetdiskEnableMode].LfsDeviceExt );
		NetdiskPartition->NetdiskVolume[NetdiskEnableMode].LfsDeviceExt = NULL;

		if (NetdiskPartition->NetdiskVolume[NetdiskEnableMode].AttachedToDeviceObject) {

			ObDereferenceObject( NetdiskPartition->NetdiskVolume[NetdiskEnableMode].AttachedToDeviceObject );
			NetdiskPartition->NetdiskVolume[NetdiskEnableMode].AttachedToDeviceObject = NULL;			
		}

	} else {

		NDAS_ASSERT( FALSE );
		ExReleaseResourceLite( &EnabledNetdisk->Resource );
		KeLeaveCriticalRegion();
		return;
	}

	RtlCopyMemory( &netdiskPartitionInfo.NetDiskAddress,
				   &EnabledNetdisk->NetdiskInformation.NetDiskAddress,
				   sizeof(netdiskPartitionInfo.NetDiskAddress) );

	netdiskPartitionInfo.UnitDiskNo = EnabledNetdisk->NetdiskInformation.UnitDiskNo;

	RtlCopyMemory( netdiskPartitionInfo.NdscId,
				   EnabledNetdisk->NetdiskInformation.NdscId,
				   NDSC_ID_LENGTH );

	if (IS_WINDOWS2K())
		netdiskPartitionInfo.StartingOffset = NetdiskPartition->NetdiskPartitionInformation.PartitionInformation.StartingOffset;
	else
		netdiskPartitionInfo.StartingOffset = NetdiskPartition->NetdiskPartitionInformation.PartitionInformationEx.StartingOffset;

	if (NetdiskEnableMode == NETDISK_SECONDARY) {

		LfsTable_DeleteNetDiskPartitionInfoUser( GlobalLfs.LfsTable,
												 &netdiskPartitionInfo,
												 FALSE );
	
	} else if (NetdiskEnableMode == NETDISK_PRIMARY || NetdiskEnableMode == NETDISK_SECONDARY2PRIMARY) {

		LfsTable_DeleteNetDiskPartitionInfoUser( GlobalLfs.LfsTable,
												 &netdiskPartitionInfo,
												 TRUE );
	}

	for (volumeIndex2 = 0; volumeIndex2 < (NETDISK_SECONDARY2PRIMARY+1); volumeIndex2++) {

		if (NetdiskPartition->NetdiskVolume[volumeIndex2].LfsDeviceExt)
			break;
	}

	if (volumeIndex2 == (NETDISK_SECONDARY2PRIMARY+1)) {

		ClearFlag( NetdiskPartition->Flags, NETDISK_PARTITION_FLAG_MOUNTED );
		SetFlag( NetdiskPartition->Flags, NETDISK_PARTITION_FLAG_DISMOUNTED );
	}

	NDAS_ASSERT( NetdiskPartition->ScsiportAdapterDeviceObjectReferenceCount );

	if (NetdiskPartition->ScsiportAdapterDeviceObjectReferenceCount != 0) {

		NetdiskPartition->ScsiportAdapterDeviceObjectReferenceCount --;
	
		if (NetdiskPartition->ScsiportAdapterDeviceObjectReferenceCount == 0) {

			if (NetdiskPartition->UnreferenceByQueryRemove == FALSE) {

				ObDereferenceObject( NetdiskPartition->ScsiportAdapterDeviceObject );
				NetdiskPartition->ScsiportAdapterDeviceObject = NULL;
			}
			
			ObDereferenceObject( NetdiskPartition->NetdiskPartitionInformation.DiskDeviceObject );
			NetdiskPartition->NetdiskPartitionInformation.DiskDeviceObject = NULL;
		}
	}

	//
	// Reference the enabled NetDisk to prevent from being freed
	// by dereferencing the partition.
	//
	EnabledNetdisk_Reference(EnabledNetdisk); // dereferenced before the function returns.

	//
	// This call dereference the enabled NetDisk if the partition reference count goes to zero.
	//
	NetdiskPartition_Dereference( NetdiskPartition ); // by EnabledNetdisk_PreMountVolume

	if (EnabledNetdisk->NetdiskEnableMode == NETDISK_SECONDARY2PRIMARY) {
		
		if (EnabledNetdisk_IsStoppedPrimary(EnabledNetdisk) == TRUE &&
			EnabledNetdisk_IsStoppedNetdisk(EnabledNetdisk) == FALSE) {

			PLIST_ENTRY	netdiskPartitionListEntry;
			KIRQL		oldIrql;

			KeAcquireSpinLock( &EnabledNetdisk->NetdiskPartitionQSpinLock, &oldIrql );
	
			for (netdiskPartitionListEntry = EnabledNetdisk->NetdiskPartitionQueue.Flink;
				 netdiskPartitionListEntry != &EnabledNetdisk->NetdiskPartitionQueue;
				 netdiskPartitionListEntry = netdiskPartitionListEntry->Flink) {

				PNETDISK_PARTITION			netdiskPartition2;
		
				netdiskPartition2 = CONTAINING_RECORD (netdiskPartitionListEntry, NETDISK_PARTITION, ListEntry);
			
				if (netdiskPartition2->NetdiskVolume[NETDISK_SECONDARY].LfsDeviceExt) {

					if (netdiskPartition2->NetdiskVolume[NETDISK_SECONDARY2PRIMARY].LfsDeviceExt == NULL &&
						netdiskPartition2->NetdiskVolume[NETDISK_SECONDARY2PRIMARY].VolumeState != VolumeEnabled) {


						PNETDISK_MANAGER_REQUEST	netdiskManagerRequest;
						
						netdiskManagerRequest = AllocNetdiskManagerRequest( FALSE );
				
						if (netdiskManagerRequest == NULL) {
				
							SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
										   ("EnabledNetdisk_DisMountVolume: failed to allocate Netdisk manager request.\n") );
							break;
						}

						netdiskManagerRequest->RequestType = NETDISK_MANAGER_REQUEST_TOUCH_VOLUME;
						NetdiskPartition_Reference( netdiskPartition2 );
						netdiskManagerRequest->NetdiskPartition = netdiskPartition2;

						QueueingNetdiskManagerRequest( GlobalLfs.NetdiskManager,
													   netdiskManagerRequest );
					}
				}
			}

			KeReleaseSpinLock( &EnabledNetdisk->NetdiskPartitionQSpinLock, oldIrql );
		}
	}

	ExReleaseResourceLite( &EnabledNetdisk->Resource );
	EnabledNetdisk_Dereference(EnabledNetdisk); // referenced before dereferencing the partition.

	KeLeaveCriticalRegion();

	return;
}


NTSTATUS
EnabledNetdisk_Secondary2Primary (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
	NTSTATUS			status = STATUS_UNSUCCESSFUL;

	BOOLEAN				result;
	IO_STATUS_BLOCK		ioStatusBlock;

	NETDISK_INFORMATION	netdiskInformation;
	NETDISK_ENABLE_MODE netdiskEnableMode;

	UNREFERENCED_PARAMETER( NetdiskEnableMode );

	ASSERT( KeGetCurrentIrql() <= PASSIVE_LEVEL );

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_Secondary2Primary: EnabledNetdisk = %p, NetdiskPartition = %p\n", EnabledNetdisk, NetdiskPartition) );

	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite( &EnabledNetdisk->Resource, TRUE );
	
	if (EnabledNetdisk->NetdiskEnableMode == NETDISK_SECONDARY2PRIMARY) {

		ExReleaseResourceLite( &EnabledNetdisk->Resource );
		KeLeaveCriticalRegion();
		return STATUS_SUCCESS;
	}

	NDAS_ASSERT( EnabledNetdisk->NetdiskEnableMode == NETDISK_SECONDARY );

	try {

		PNETDISK_PARTITION	netdiskPartition;
		PLIST_ENTRY			listEntry;
		KIRQL				oldIrql;

		KeAcquireSpinLock( &EnabledNetdisk->NetdiskPartitionQSpinLock, &oldIrql );

	    for (netdiskPartition = NULL, listEntry = EnabledNetdisk->NetdiskPartitionQueue.Flink;
		     listEntry != &EnabledNetdisk->NetdiskPartitionQueue;
			 netdiskPartition = NULL, listEntry = listEntry->Flink) {

			netdiskPartition = CONTAINING_RECORD(listEntry, NETDISK_PARTITION, ListEntry);

			ASSERT( netdiskPartition->ReferenceCount > 0 );

			if (netdiskPartition->NetdiskVolume[NETDISK_SECONDARY].VolumeState == VolumePreMounting || 
				netdiskPartition->NetdiskVolume[NETDISK_SECONDARY].VolumeState == VolumePostMounting) {

				KeReleaseSpinLock( &EnabledNetdisk->NetdiskPartitionQSpinLock, oldIrql );
		
				status = STATUS_UNSUCCESSFUL;
				leave;
			}
		}

		KeReleaseSpinLock( &EnabledNetdisk->NetdiskPartitionQSpinLock, oldIrql );
		status = NdasScsiCtrlUpgradeToWrite(
						NetdiskPartition->NetdiskPartitionInformation.DiskDeviceObject,
						&ioStatusBlock);
		if (!NT_SUCCESS(status)) {
	
			SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_ERROR, 
					       ("[LFS] SecondaryToPrimaryThreadProc: NDCtrlUpgradeToWrite() function failed.\n") );
	
		} else if (!NT_SUCCESS(ioStatusBlock.Status)) {
	
			SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE, 
						   ("[LFS] SecondaryToPrimaryThreadProc: Upgrading failed. NetWorking failed.\n") );
	
		} else {
	
			SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_INFO, 
						   ("[LFS] SecondaryToPrimaryThreadProc: Primary mode.\n") );
		}

		if (NetdiskPartition->ScsiportAdapterDeviceObject == NULL) {

			ASSERT( FlagOn(NetdiskPartition->Flags, NETDISK_PARTITION_FLAG_SURPRISE_REMOVED) );

			status = STATUS_UNSUCCESSFUL;
			leave;
		}

		result = IsNetdiskPartition( NetdiskPartition->NetdiskPartitionInformation.DiskDeviceObject,
									 &netdiskInformation,
									 &NetdiskPartition->ScsiportAdapterDeviceObject );
	
		if (result == FALSE) {

			status = STATUS_UNSUCCESSFUL;
			leave;
		}
	
		if (netdiskInformation.EnabledTime.QuadPart != EnabledNetdisk->NetdiskInformation.EnabledTime.QuadPart) {

			status = STATUS_UNSUCCESSFUL;
			leave;
		}

		if (netdiskInformation.DeviceMode == DEVMODE_SHARED_READWRITE) {

			if (!FlagOn(netdiskInformation.EnabledFeatures, NDASFEATURE_SECONDARY)) {

				netdiskEnableMode = NETDISK_PRIMARY;
			
			} else {

				netdiskEnableMode = NETDISK_SECONDARY;
			}

		} else if (netdiskInformation.DeviceMode == DEVMODE_SHARED_READONLY) {
	
			netdiskEnableMode = NETDISK_READ_ONLY;

		} else {

			NDAS_ASSERT( FALSE );
			netdiskEnableMode = NETDISK_UNKNOWN_MODE;
		}

		if (netdiskEnableMode == NETDISK_PRIMARY) {

			EnabledNetdisk->NetdiskInformation = netdiskInformation;

			EnabledNetdisk->NetdiskEnableMode = NETDISK_SECONDARY2PRIMARY;
			status = STATUS_SUCCESS;
	
		} else {

			status = STATUS_UNSUCCESSFUL;
		} 
	
	} finally {

		ExReleaseResourceLite( &EnabledNetdisk->Resource );
		KeLeaveCriticalRegion();
	}

	return status;
}


NTSTATUS
EnabledNetdisk_UnplugNetdisk (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
	NTSTATUS			status;
	BOOLEAN				result;
	NETDISK_INFORMATION	netdiskInformation;
	

	UNREFERENCED_PARAMETER( NetdiskEnableMode );

	ExAcquireFastMutex( &EnabledNetdisk->FastMutex );	
	
	if (FlagOn(EnabledNetdisk->Flags, ENABLED_NETDISK_UNPLUGING) || 
		FlagOn(EnabledNetdisk->Flags, ENABLED_NETDISK_UNPLUGED)  || 
		FlagOn(EnabledNetdisk->Flags, ENABLED_NETDISK_SURPRISE_REMOVAL)) {

		ExReleaseFastMutex( &EnabledNetdisk->FastMutex );

		return STATUS_UNSUCCESSFUL;
	}

	SetFlag( EnabledNetdisk->Flags, ENABLED_NETDISK_UNPLUGING );
	
	ExReleaseFastMutex( &EnabledNetdisk->FastMutex );

	result = IsNetdiskPartition( NetdiskPartition->NetdiskPartitionInformation.DiskDeviceObject,
								 &netdiskInformation,
								 &NetdiskPartition->ScsiportAdapterDeviceObject );
	
	if (result == TRUE) {

		status = NDCtrlUnplug( EnabledNetdisk->NetdiskInformation.SlotNo );

	} else {

		status = STATUS_UNSUCCESSFUL;
	}

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_ERROR,
				   ("EnabledNetdisk_UnplugNetdisk:	result = %d status = %x\n", result, status) );
	
	ExAcquireFastMutex( &EnabledNetdisk->FastMutex );
	ClearFlag( EnabledNetdisk->Flags, ENABLED_NETDISK_UNPLUGING );
	ExReleaseFastMutex( &EnabledNetdisk->FastMutex );

	return status;
}


BOOLEAN
EnabledNetdisk_ThisVolumeHasSecondary (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode,
	IN BOOLEAN				IncludeLocalSecondary
	)
{
	ULONG					value;


	UNREFERENCED_PARAMETER( NetdiskEnableMode );

	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite( &EnabledNetdisk->Resource, TRUE );

#if !__NDAS_FS_MINI__
	NDAS_ASSERT( NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState == VolumeMounted );
#else
	NDAS_ASSERT( NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState == VolumeMounted ||
				   NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState == VolumeStopping );
#endif

	InterlockedIncrement( &NetdiskPartition->ExportCount );
	value = InterlockedDecrement( &NetdiskPartition->ExportCount );
	
	if (value) {

		ExReleaseResourceLite( &EnabledNetdisk->Resource );
		KeLeaveCriticalRegion();
		return TRUE;
	}

	if (IncludeLocalSecondary) {
	
		InterlockedIncrement( &NetdiskPartition->LocalExportCount );
		value = InterlockedDecrement( &NetdiskPartition->LocalExportCount );
		
		if (value) {
		
			ExReleaseResourceLite( &EnabledNetdisk->Resource );
			KeLeaveCriticalRegion();
			return TRUE;
		}
	}

	ExReleaseResourceLite( &EnabledNetdisk->Resource );
	KeLeaveCriticalRegion();

	return FALSE;
}


NTSTATUS
WakeUpVolume (
	PNETDISK_PARTITION	NetdiskPartition
	)
{
	HANDLE						fileHandle = NULL;	
	UNICODE_STRING				fileName;
	PWCHAR						fileNameBuffer;
	IO_STATUS_BLOCK				ioStatusBlock;
	NTSTATUS					createStatus;

	ACCESS_MASK					desiredAccess;
	ULONG						attributes;
	OBJECT_ATTRIBUTES			objectAttributes;
	LARGE_INTEGER				allocationSize;
	ULONG						fileAttributes;
	ULONG						shareAccess;
	ULONG						createDisposition;
	ULONG						createOptions;
	PFILE_FULL_EA_INFORMATION	eaBuffer;
    ULONG						eaLength;			

	
	//
	//	Allocate a name buffer
	//

	fileNameBuffer = ExAllocatePoolWithTag( NonPagedPool, NDFS_MAX_PATH, LFS_ALLOC_TAG );
	
	if (fileNameBuffer == NULL) {
	
		ASSERT( LFS_REQUIRED );
		return STATUS_INSUFFICIENT_RESOURCES;
	}

    RtlInitEmptyUnicodeString( &fileName,
							   fileNameBuffer,
							   NDFS_MAX_PATH );

	RtlCopyUnicodeString( &fileName,
						  &NetdiskPartition->NetdiskPartitionInformation.VolumeName );

	
	ioStatusBlock.Information = 0;

	createStatus = RtlAppendUnicodeToString( &fileName, WAKEUP_VOLUME_FILE_NAME );

	if (createStatus != STATUS_SUCCESS) {

		ASSERT( LFS_UNEXPECTED );
		ExFreePool( fileNameBuffer );
		return createStatus;
	}

	desiredAccess =	SYNCHRONIZE | READ_CONTROL | FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES | FILE_WRITE_EA; 
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
	createOptions     = FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE | FILE_DELETE_ON_CLOSE;
	eaBuffer		  = NULL;
	eaLength		  = 0;

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
								 0 );
		
	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("WakeUpVolume:	volume %wZ, createStatus = %x, ioStatusBlock.Information = %d\n",
					&NetdiskPartition->NetdiskPartitionInformation.VolumeName, createStatus, ioStatusBlock.Information ));

	if (createStatus == STATUS_SUCCESS) {
	
		ZwClose( fileHandle );
	}

	if (createStatus != STATUS_PORT_DISCONNECTED)
		createStatus = STATUS_SUCCESS;

	ExFreePool( fileNameBuffer );

	return createStatus;
}

//
// Find a NetDisk partition and increments reference count
//

NTSTATUS
EnabledNetdisk_GetPrimaryPartition (
	IN  PENABLED_NETDISK				EnabledNetdisk,
	IN 	PPRIMARY_SESSION				PrimarySession,
	IN  PLPX_ADDRESS					NetDiskAddress,
	IN  USHORT							UnitDiskNo,
	IN  PLARGE_INTEGER					StartingOffset,
	IN  BOOLEAN							LocalSecondary,
	OUT PNETDISK_PARTITION				*NetdiskPartition,
	OUT PNETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation,
	OUT PLFS_FILE_SYSTEM_TYPE			FileSystemType
	)
{
	NTSTATUS			status;
	PNETDISK_PARTITION	netdiskPartition = NULL;
	NTSTATUS			wakeUpVolumeStatus;
	//NETDISK_INFORMATION	netdiskInformation;
	ULONG				waitCount = 0;


	UNREFERENCED_PARAMETER( NetDiskAddress );
	UNREFERENCED_PARAMETER( UnitDiskNo );

	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite( &EnabledNetdisk->Resource, TRUE );

	ASSERT( EnabledNetdisk->NetdiskInformation.UnitDiskNo == UnitDiskNo );


	if (StartingOffset == NULL) {

		*NetdiskPartition = &EnabledNetdisk->DummyNetdiskPartition;

		*FileSystemType = LFS_FILE_SYSTEM_OTHER;
		*NetdiskPartitionInformation = EnabledNetdisk->DummyNetdiskPartition.NetdiskPartitionInformation;

		RtlInitEmptyUnicodeString( &NetdiskPartitionInformation->VolumeName,
								   NetdiskPartitionInformation->VolumeNameBuffer,
								   sizeof(NetdiskPartitionInformation->VolumeNameBuffer) );

		EnabledNetdisk_Reference( EnabledNetdisk ); // dereferenced in EnabledNetdisk_ReturnPrimaryPartition()
		PrimarySession_Reference( PrimarySession );

		ExReleaseResourceLite( &EnabledNetdisk->Resource );
		KeLeaveCriticalRegion();

		return STATUS_SUCCESS;
	}

	netdiskPartition = EnabledNetdisk_LookupNetdiskPartition( EnabledNetdisk,
															  NULL,
															  StartingOffset,
															  NULL );

	if (netdiskPartition == NULL) {

		ExReleaseResourceLite( &EnabledNetdisk->Resource );
		KeLeaveCriticalRegion();

		return STATUS_UNRECOGNIZED_VOLUME;
	}

#if 0
	if (netdiskPartition->NetdiskPartitionInformation.ScsiportAdapterDeviceObject && 
		IsNetdiskPartition(netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject, 
						   &netdiskInformation, 
						   &netdiskPartition->ScsiportAdapterDeviceObject)) {

		if (EnabledNetdisk->NetdiskInformation.EnabledTime.QuadPart != netdiskInformation.EnabledTime.QuadPart) {			
		
			SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_ERROR,
						   ("EnabledNetdisk_GetPrimaryPartition: Enable time mismatch!\n") );

			ASSERT( FALSE );

			ExReleaseResourceLite( &EnabledNetdisk->Resource );
			KeLeaveCriticalRegion();

			NetdiskPartition_Dereference( netdiskPartition );		// by EnabledNetdisk_LookupNetdiskPartition

			return STATUS_NO_SUCH_DEVICE;
		}
	}
#endif

	if (EnabledNetdisk_IsStoppedNetdisk(EnabledNetdisk)) {

		SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
					   ("EnabledNetdisk_GetPrimaryPartition: EnabledNetdisk_IsStoppedNetdisk\n") );

		NetdiskPartition_Dereference( netdiskPartition );		// by EnabledNetdisk_LookupNetdiskPartition
		ExReleaseResourceLite( &EnabledNetdisk->Resource );
		KeLeaveCriticalRegion();

		return STATUS_NO_SUCH_DEVICE;
	}

	if (EnabledNetdisk->NetdiskEnableMode != NETDISK_PRIMARY && 
		EnabledNetdisk->NetdiskEnableMode != NETDISK_SECONDARY2PRIMARY) {

		SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_ERROR,
					   ("EnabledNetdisk_GetPrimaryPartition: not a primary host!\n") );

		//ASSERT( FALSE );

		ExReleaseResourceLite( &EnabledNetdisk->Resource );
		KeLeaveCriticalRegion();

		NetdiskPartition_Dereference( netdiskPartition );		// by EnabledNetdisk_LookupNetdiskPartition

		return STATUS_NO_SUCH_DEVICE;
	}

	ExReleaseResourceLite( &EnabledNetdisk->Resource );
	KeLeaveCriticalRegion();

	while (1) {

		if (EnabledNetdisk_IsStoppedNetdisk(netdiskPartition->EnabledNetdisk)) {

			SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
						   ("EnabledNetdisk_GetPrimaryPartition: EnabledNetdisk_IsStoppedNetdisk\n") );

			NetdiskPartition_Dereference( netdiskPartition );		// by EnabledNetdisk_LookupNetdiskPartition
			netdiskPartition = NULL;

			status = STATUS_NO_SUCH_DEVICE;
			break;
		}
			
		if (FlagOn(netdiskPartition->Flags, NETDISK_PARTITION_FLAG_MOUNT_SUCCESS)) {

			if (netdiskPartition->NetdiskVolume[EnabledNetdisk->NetdiskEnableMode].LfsDeviceExt) {

				status = STATUS_SUCCESS;
				break;
			}

			if (waitCount == 0) {

				LARGE_INTEGER interval;

				interval.QuadPart = (5 * DELAY_ONE_SECOND);      //delay 5 seconds
				KeDelayExecutionThread( KernelMode, FALSE, &interval );

				waitCount ++;
				continue;
			}

			wakeUpVolumeStatus = WakeUpVolume( netdiskPartition );

			if (wakeUpVolumeStatus == STATUS_SUCCESS) {

				if (netdiskPartition->NetdiskVolume[EnabledNetdisk->NetdiskEnableMode].LfsDeviceExt) {

					status = STATUS_SUCCESS;

				} else {

					//NDAS_ASSERT( FALSE );

					SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
								   ("EnabledNetdisk_GetPrimaryPartition: Device extension NULL!!!\n") );

					NetdiskPartition_Dereference( netdiskPartition );		// by EnabledNetdisk_LookupNetdiskPartition
					netdiskPartition = NULL;

					status = STATUS_NO_SUCH_DEVICE;
				}
		
			} else {

				SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_ERROR,
							   ("EnabledNetdisk_GetPrimaryPartition: WakeUpVolume failed\n") );

				NetdiskPartition_Dereference( netdiskPartition );		// by EnabledNetdisk_LookupNetdiskPartition
				netdiskPartition = NULL;

				status = STATUS_NO_SUCH_DEVICE;
			}

		} else if (FlagOn(netdiskPartition->Flags, NETDISK_PARTITION_FLAG_MOUNT_CORRUPTED)) {

			status = STATUS_SUCCESS;
	
		} else {

			SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
						   ("EnabledNetdisk_GetPrimaryPartition: Partition not mounted!!! netdiskPartition->Flags = %x\n",
						    netdiskPartition->Flags) );

			NetdiskPartition_Dereference( netdiskPartition );		// by EnabledNetdisk_LookupNetdiskPartition
			netdiskPartition = NULL;

			status = STATUS_UNRECOGNIZED_VOLUME;
		}

		break;
	} 

	if (status == STATUS_SUCCESS) {

		if (!LocalSecondary) {
			
			InterlockedIncrement( &netdiskPartition->ExportCount );
			
		} else {

			InterlockedIncrement( &netdiskPartition->LocalExportCount );
		}

		*NetdiskPartition = netdiskPartition;

		*FileSystemType = netdiskPartition->FileSystemType;

		*NetdiskPartitionInformation = netdiskPartition->NetdiskPartitionInformation;

		RtlInitEmptyUnicodeString( &NetdiskPartitionInformation->VolumeName,
								   NetdiskPartitionInformation->VolumeNameBuffer,
								   sizeof(NetdiskPartitionInformation->VolumeNameBuffer) );

		if (RtlAppendUnicodeStringToString( &NetdiskPartitionInformation->VolumeName,
											&netdiskPartition->NetdiskPartitionInformation.VolumeName
											) != STATUS_SUCCESS) {

			ASSERT( LFS_UNEXPECTED );
		}

		PrimarySession_Reference( PrimarySession );
		ExInterlockedInsertHeadList( &netdiskPartition->PrimarySessionQueue,
									 &PrimarySession->NetdiskPartitionListEntry,
									 &netdiskPartition->PrimarySessionQSpinLock );
	}

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_GetPrimaryPartition: netdiskPartition = %p, PrimarySession = %p\n",
				   netdiskPartition, PrimarySession) );

	return status;
}


//
// Decrements reference count
//

VOID
EnabledNetdisk_ReturnPrimaryPartition (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PPRIMARY_SESSION		PrimarySession,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN BOOLEAN				LocalSecondary
	)
{
	KIRQL oldIrql;

	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite( &EnabledNetdisk->Resource, TRUE );
		
	if (NetdiskPartition == &EnabledNetdisk->DummyNetdiskPartition) {

		KeAcquireSpinLock( &NetdiskPartition->PrimarySessionQSpinLock, &oldIrql );
		PrimarySession_Dereference( PrimarySession );
		KeReleaseSpinLock( &NetdiskPartition->PrimarySessionQSpinLock, oldIrql );

		ExReleaseResourceLite( &EnabledNetdisk->Resource );
		KeLeaveCriticalRegion();

		EnabledNetdisk_Dereference( EnabledNetdisk ); // referenced by EnabledNetdisk_GetPrimaryPartition()

		return;
	}

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_ReturnPrimaryPartition: NetdiskPartition = %p, PrimarySession = %p\n",
				     NetdiskPartition, PrimarySession) );

	if(!LocalSecondary)
		InterlockedDecrement( &NetdiskPartition->ExportCount );
	else
		InterlockedDecrement( &NetdiskPartition->LocalExportCount );

	KeAcquireSpinLock( &NetdiskPartition->PrimarySessionQSpinLock, &oldIrql );
	RemoveEntryList( &PrimarySession->NetdiskPartitionListEntry );
	//InitializeListHead( &PrimarySession->NetdiskPartitionListEntry );
	PrimarySession_Dereference( PrimarySession );
	KeReleaseSpinLock( &NetdiskPartition->PrimarySessionQSpinLock, oldIrql );

	ExReleaseResourceLite( &EnabledNetdisk->Resource );
	KeLeaveCriticalRegion();

	NetdiskPartition_Dereference( NetdiskPartition );  // by EnabledNetdisk_GetPrimaryPartition

	return;
}


VOID
EnabledNetdisk_PrimarySessionStopping (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
	PLIST_ENTRY	primarySessionListEntry;
	KIRQL		oldIrql;


	UNREFERENCED_PARAMETER( EnabledNetdisk );
	UNREFERENCED_PARAMETER( NetdiskEnableMode );

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_PrimarySessionStopping: NetdiskPartition = %p\n",
				   NetdiskPartition) );

	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite( &EnabledNetdisk->Resource, TRUE );

	ExAcquireFastMutex( &NetdiskPartition->FastMutex );
	NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState = VolumeStopping;
	ExReleaseFastMutex( &NetdiskPartition->FastMutex );

	KeAcquireSpinLock( &NetdiskPartition->PrimarySessionQSpinLock, &oldIrql );

	for (primarySessionListEntry = NetdiskPartition->PrimarySessionQueue.Flink;
		 primarySessionListEntry != &NetdiskPartition->PrimarySessionQueue;
		 ) {

		 PPRIMARY_SESSION	primarySession;
				
		primarySession = CONTAINING_RECORD( primarySessionListEntry, PRIMARY_SESSION, NetdiskPartitionListEntry );
		PrimarySession_Reference( primarySession );
		
		KeReleaseSpinLock( &NetdiskPartition->PrimarySessionQSpinLock, oldIrql );
						
		PrimarySession_Stopping( primarySession );

		KeAcquireSpinLock( &NetdiskPartition->PrimarySessionQSpinLock, &oldIrql );

		primarySessionListEntry = primarySessionListEntry->Flink;
		PrimarySession_Dereference( primarySession );
	}

	KeReleaseSpinLock( &NetdiskPartition->PrimarySessionQSpinLock, oldIrql );

	ExReleaseResourceLite( &EnabledNetdisk->Resource );
	KeLeaveCriticalRegion();

	return;
}


VOID
EnabledNetdisk_PrimarySessionCancelStopping (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
	PLIST_ENTRY	primarySessionListEntry;
	KIRQL		oldIrql;


	UNREFERENCED_PARAMETER( EnabledNetdisk );
	UNREFERENCED_PARAMETER( NetdiskEnableMode );

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_PrimarySessionCancelStopping: NetdiskPartition = %p\n",
				   NetdiskPartition) );

	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite( &EnabledNetdisk->Resource, TRUE );

	ExAcquireFastMutex( &NetdiskPartition->FastMutex );
	NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState = VolumeMounted;
	ExReleaseFastMutex( &NetdiskPartition->FastMutex );

	KeAcquireSpinLock( &NetdiskPartition->PrimarySessionQSpinLock, &oldIrql );

	for (primarySessionListEntry = NetdiskPartition->PrimarySessionQueue.Flink;
		 primarySessionListEntry != &NetdiskPartition->PrimarySessionQueue;
		 ) {

		 PPRIMARY_SESSION	primarySession;
				
		primarySession = CONTAINING_RECORD( primarySessionListEntry, PRIMARY_SESSION, NetdiskPartitionListEntry );
		PrimarySession_Reference( primarySession );
		
		KeReleaseSpinLock( &NetdiskPartition->PrimarySessionQSpinLock, oldIrql );
						
		PrimarySession_CancelStopping( primarySession );

		KeAcquireSpinLock( &NetdiskPartition->PrimarySessionQSpinLock, &oldIrql );

		primarySessionListEntry = primarySessionListEntry->Flink;
		PrimarySession_Dereference( primarySession );
	}

	KeReleaseSpinLock( &NetdiskPartition->PrimarySessionQSpinLock, oldIrql );

	ExReleaseResourceLite( &EnabledNetdisk->Resource );
	KeLeaveCriticalRegion();

	return;
}

VOID
EnabledNetdisk_PrimarySessionDisconnect (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	)
{
	PLIST_ENTRY	primarySessionListEntry;
	KIRQL		oldIrql;


	UNREFERENCED_PARAMETER( EnabledNetdisk );
	UNREFERENCED_PARAMETER( NetdiskEnableMode );

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_PrimarySessionDisconnect: NetdiskPartition = %p\n",
				   NetdiskPartition) );

	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite( &EnabledNetdisk->Resource, TRUE );

	ExAcquireFastMutex( &NetdiskPartition->FastMutex );
	NetdiskPartition->NetdiskVolume[NetdiskEnableMode].VolumeState = VolumeStopped;
	ExReleaseFastMutex( &NetdiskPartition->FastMutex );

	KeAcquireSpinLock( &NetdiskPartition->PrimarySessionQSpinLock, &oldIrql );

	for (primarySessionListEntry = NetdiskPartition->PrimarySessionQueue.Flink;
		 primarySessionListEntry != &NetdiskPartition->PrimarySessionQueue;
		 ) {

		 PPRIMARY_SESSION	primarySession;
				
		primarySession = CONTAINING_RECORD( primarySessionListEntry, PRIMARY_SESSION, NetdiskPartitionListEntry );
		PrimarySession_Reference( primarySession );
		
		KeReleaseSpinLock( &NetdiskPartition->PrimarySessionQSpinLock, oldIrql );
						
		PrimarySession_Disconnect( primarySession );

		KeAcquireSpinLock( &NetdiskPartition->PrimarySessionQSpinLock, &oldIrql );

		primarySessionListEntry = primarySessionListEntry->Flink;
		PrimarySession_Dereference( primarySession );
	}

	KeReleaseSpinLock( &NetdiskPartition->PrimarySessionQSpinLock, oldIrql );

	ExReleaseResourceLite( &EnabledNetdisk->Resource );
	KeLeaveCriticalRegion();

	return;
}


NTSTATUS
EnabledNetdisk_TakeOver (
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN PSESSION_INFORMATION	SessionInformation
	)
{
	NTSTATUS				status = STATUS_INVALID_DEVICE_REQUEST;
	IO_STATUS_BLOCK			ioStatusBlock;
	ULONG					ioControlCode;
	ULONG					inputBufferLength;
	PVOID					handle = NULL;
	ULONG					outputBufferLength;


	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
				   ("EnabledNetdisk_TakeOver: NetdiskPartition = %p\n", NetdiskPartition) );

	UNREFERENCED_PARAMETER( EnabledNetdisk );

	//KeEnterCriticalRegion();
	//ExAcquireResourceSharedLite( &EnabledNetdisk->Resource, TRUE );
 		
	ioControlCode		= IOCTL_INSERT_PRIMARY_SESSION; 
	inputBufferLength	= sizeof( SESSION_INFORMATION );
	outputBufferLength	= sizeof( handle );

	RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

	if (NetdiskPartition->FileSystemType == LFS_FILE_SYSTEM_NDAS_FAT ||
		NetdiskPartition->FileSystemType == LFS_FILE_SYSTEM_NDAS_NTFS) {
			
		if (NetdiskPartition->NetdiskVolume[NETDISK_PRIMARY].VolumeState == VolumeMounted && 
			NT_SUCCESS(NetdiskPartition->NetdiskVolume[NETDISK_PRIMARY].MountStatus)) {
			
			if (NetdiskPartition->NetdiskVolume[NETDISK_PRIMARY].AttachedToDeviceObject == NULL) {
				
				ASSERT( FALSE );
				status = STATUS_UNSUCCESSFUL;
			
			} else {

				status = LfsFilterDeviceIoControl( NetdiskPartition->NetdiskVolume[NETDISK_PRIMARY].AttachedToDeviceObject,
												   ioControlCode,
												   SessionInformation,
												   inputBufferLength,
												   &handle,
												   outputBufferLength,
												   NULL );

				if (GlobalLfs.NdasFatRwIndirect == TRUE && NetdiskPartition->FileSystemType == LFS_FILE_SYSTEM_NDAS_FAT ||
					GlobalLfs.NdasNtfsRwIndirect == TRUE && NetdiskPartition->FileSystemType == LFS_FILE_SYSTEM_NDAS_NTFS) {

					NDAS_ASSERT( status == STATUS_INVALID_DEVICE_REQUEST );

				} else {

					NDAS_ASSERT( status == STATUS_SUCCESS );
				}
			}

		} else if (NetdiskPartition->NetdiskVolume[NETDISK_SECONDARY2PRIMARY].VolumeState == VolumeMounted &&
				   NT_SUCCESS(NetdiskPartition->NetdiskVolume[NETDISK_SECONDARY2PRIMARY].MountStatus)) {
			
			if (NetdiskPartition->NetdiskVolume[NETDISK_SECONDARY2PRIMARY].AttachedToDeviceObject == NULL) {
				
				ASSERT( FALSE );
				status = STATUS_UNSUCCESSFUL;
			
			} else {

				status = LfsFilterDeviceIoControl( NetdiskPartition->NetdiskVolume[NETDISK_SECONDARY2PRIMARY].AttachedToDeviceObject,
												   ioControlCode,
												   SessionInformation,
												   inputBufferLength,
												   &handle,
												   outputBufferLength,
												   NULL );

				if (GlobalLfs.NdasFatRwIndirect == TRUE && NetdiskPartition->FileSystemType == LFS_FILE_SYSTEM_NDAS_FAT ||
					GlobalLfs.NdasNtfsRwIndirect == TRUE && NetdiskPartition->FileSystemType == LFS_FILE_SYSTEM_NDAS_NTFS) {

					NDAS_ASSERT( status == STATUS_INVALID_DEVICE_REQUEST || status == STATUS_INSUFFICIENT_RESOURCES );

				} else {

					NDAS_ASSERT( status == STATUS_SUCCESS || status == STATUS_INSUFFICIENT_RESOURCES );
				}
			}

		} else {

			status = STATUS_UNSUCCESSFUL;
		}
	
	} else {

		status = STATUS_INVALID_DEVICE_REQUEST;
	}

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE, 
				  ("EnabledNetdisk_TakeOver: handle = %p, deviceControlStatus = %X\n", handle, status) );

	//ExReleaseResourceLite( &EnabledNetdisk->Resource );
	//KeLeaveCriticalRegion();

	return status;
}


BOOLEAN
EnabledNetdisk_IsStoppedNetdisk (
	IN PENABLED_NETDISK	EnabledNetdisk
	)
{
	PNETDISK_PARTITION	netdiskPartition;
    PLIST_ENTRY			listEntry;
	KIRQL				oldIrql;

	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite( &EnabledNetdisk->Resource, TRUE );

	KeAcquireSpinLock( &EnabledNetdisk->NetdiskPartitionQSpinLock, &oldIrql );

    for (netdiskPartition = NULL, listEntry = EnabledNetdisk->NetdiskPartitionQueue.Flink;
         listEntry != &EnabledNetdisk->NetdiskPartitionQueue;
         netdiskPartition = NULL, listEntry = listEntry->Flink) {

		//NETDISK_ENABLE_MODE	volumeIndex;

		netdiskPartition = CONTAINING_RECORD (listEntry, NETDISK_PARTITION, ListEntry);

		ASSERT( netdiskPartition->ReferenceCount > 0 );

#if 1 

		if (netdiskPartition->ScsiportAdapterDeviceObjectReferenceCount) {

			KeReleaseSpinLock( &EnabledNetdisk->NetdiskPartitionQSpinLock, oldIrql );
			ExReleaseResourceLite( &EnabledNetdisk->Resource );
			KeLeaveCriticalRegion();

			return FALSE;
		}

#else

		for (volumeIndex = 0; volumeIndex < (NETDISK_SECONDARY2PRIMARY+1); volumeIndex++) {

			if (netdiskPartition->NetdiskVolume[volumeIndex].LfsDeviceExt) {

				KeReleaseSpinLock(&EnabledNetdisk->NetdiskPartitionQSpinLock, oldIrql);
				ExReleaseResourceLite( &EnabledNetdisk->Resource );
				KeLeaveCriticalRegion();

				return FALSE;
			}
		}

#endif

	}

	KeReleaseSpinLock(&EnabledNetdisk->NetdiskPartitionQSpinLock, oldIrql);

	ExReleaseResourceLite( &EnabledNetdisk->Resource );
	KeLeaveCriticalRegion();

	return TRUE;
}


BOOLEAN
EnabledNetdisk_IsStoppedPrimary (
	IN PENABLED_NETDISK	EnabledNetdisk
	)
{
	PNETDISK_PARTITION	netdiskPartition;
    PLIST_ENTRY			listEntry;
	KIRQL				oldIrql;

	KeEnterCriticalRegion();
	ExAcquireResourceSharedLite( &EnabledNetdisk->Resource, TRUE );

	KeAcquireSpinLock(&EnabledNetdisk->NetdiskPartitionQSpinLock, &oldIrql);

    for (netdiskPartition = NULL, listEntry = EnabledNetdisk->NetdiskPartitionQueue.Flink;
         listEntry != &EnabledNetdisk->NetdiskPartitionQueue;
         netdiskPartition = NULL, listEntry = listEntry->Flink) {

		netdiskPartition = CONTAINING_RECORD (listEntry, NETDISK_PARTITION, ListEntry);

		ASSERT( netdiskPartition->ReferenceCount > 0 );

		if (netdiskPartition->NetdiskVolume[NETDISK_PRIMARY].LfsDeviceExt || 
			netdiskPartition->NetdiskVolume[NETDISK_SECONDARY2PRIMARY].LfsDeviceExt) {

			KeReleaseSpinLock( &EnabledNetdisk->NetdiskPartitionQSpinLock, oldIrql );
			ExReleaseResourceLite( &EnabledNetdisk->Resource );
			KeLeaveCriticalRegion();

			return FALSE;
		}
	}

	KeReleaseSpinLock( &EnabledNetdisk->NetdiskPartitionQSpinLock, oldIrql );

	ExReleaseResourceLite( &EnabledNetdisk->Resource );
	KeLeaveCriticalRegion();

	return TRUE;
}


PNETDISK_PARTITION
EnabledNetdisk_LookupNetdiskPartition (
	IN PENABLED_NETDISK				EnabledNetdisk,
    IN PPARTITION_INFORMATION_EX	PartitionInformationEx,
	IN PLARGE_INTEGER				StartingOffset,
	IN PDEVICE_OBJECT				DiskDeviceObject
	)
{
	PPARTITION_INFORMATION	PartitionInformation = (PPARTITION_INFORMATION)PartitionInformationEx;
	PNETDISK_PARTITION		netdiskPartition;
    PLIST_ENTRY				listEntry;
	KIRQL					oldIrql;


	KeAcquireSpinLock( &EnabledNetdisk->NetdiskPartitionQSpinLock, &oldIrql );

    for (netdiskPartition = NULL, listEntry = EnabledNetdisk->NetdiskPartitionQueue.Flink;
         listEntry != &EnabledNetdisk->NetdiskPartitionQueue;
         netdiskPartition = NULL, listEntry = listEntry->Flink) {

		netdiskPartition = CONTAINING_RECORD (listEntry, NETDISK_PARTITION, ListEntry);

		ASSERT( netdiskPartition->ReferenceCount > 0 );

		if (IS_WINDOWS2K()) {

			if (PartitionInformation && 
				PartitionInformation->StartingOffset.QuadPart == 
				netdiskPartition->NetdiskPartitionInformation.PartitionInformation.StartingOffset.QuadPart) {

				ASSERT( PartitionInformation->PartitionNumber == 
					    netdiskPartition->NetdiskPartitionInformation.PartitionInformation.PartitionNumber );

				ASSERT( PartitionInformation->RewritePartition == 
					    netdiskPartition->NetdiskPartitionInformation.PartitionInformation.RewritePartition );
			
				NetdiskPartition_Reference( netdiskPartition );

				break;
			}

			if (StartingOffset && 
				StartingOffset->QuadPart == 
				netdiskPartition->NetdiskPartitionInformation.PartitionInformation.StartingOffset.QuadPart) {

				NetdiskPartition_Reference( netdiskPartition );
				break;
			}
		
		} else {

			if (PartitionInformationEx && 
				PartitionInformationEx->StartingOffset.QuadPart == 
				netdiskPartition->NetdiskPartitionInformation.PartitionInformationEx.StartingOffset.QuadPart) {

				ASSERT( PartitionInformationEx->PartitionNumber == 
					    netdiskPartition->NetdiskPartitionInformation.PartitionInformationEx.PartitionNumber );

				ASSERT( PartitionInformationEx->RewritePartition == 
					    netdiskPartition->NetdiskPartitionInformation.PartitionInformationEx.RewritePartition );
			
				NetdiskPartition_Reference( netdiskPartition );
				break;
			}

			if (StartingOffset && StartingOffset->QuadPart == 
				netdiskPartition->NetdiskPartitionInformation.PartitionInformationEx.StartingOffset.QuadPart) {

				NetdiskPartition_Reference( netdiskPartition );
				break;
			}
		}

		if (DiskDeviceObject) {

			if (netdiskPartition->NetdiskPartitionInformation.DiskDeviceObject == DiskDeviceObject) {

				NetdiskPartition_Reference( netdiskPartition );
				break;
			}
		}	
	}

	KeReleaseSpinLock( &EnabledNetdisk->NetdiskPartitionQSpinLock, oldIrql );

	return netdiskPartition;
}


PNETDISK_PARTITION
NetdiskParttion_Allocate (
	IN PENABLED_NETDISK		EnabledNetdisk
	)
{
	PNETDISK_PARTITION	netdiskPartition;


	netdiskPartition = ExAllocatePoolWithTag( NonPagedPool, 
											  sizeof(NETDISK_PARTITION),
											  NETDISK_MANAGER_PARTITION_TAG );
	
	if (netdiskPartition == NULL) {

		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
		return NULL;
	}

	EnabledNetdisk_Reference( EnabledNetdisk );	// dereferenced in NetdiskPartition_Dereference()

	RtlZeroMemory( netdiskPartition, sizeof(NETDISK_PARTITION) );
	

	ExInitializeFastMutex( &netdiskPartition->FastMutex );

	netdiskPartition->ReferenceCount = 1;
	InitializeListHead( &netdiskPartition->ListEntry );
	netdiskPartition->EnabledNetdisk = EnabledNetdisk;
	
    RtlInitEmptyUnicodeString( &netdiskPartition->NetdiskPartitionInformation.VolumeName,
							   netdiskPartition->NetdiskPartitionInformation.VolumeNameBuffer,
							   sizeof(netdiskPartition->NetdiskPartitionInformation.VolumeNameBuffer) );
	
	KeInitializeSpinLock( &netdiskPartition->PrimarySessionQSpinLock );
	InitializeListHead( &netdiskPartition->PrimarySessionQueue );

	SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_NOISE,
				   ("NetdiskParttion_Allocate netdiskPartition = %p\n", 
				    netdiskPartition) );

	ExAcquireFastMutex( &GlobalLfs.FastMutex );
	
	netdiskPartition->Tid = GlobalLfs.Tid++;

	ExReleaseFastMutex( &GlobalLfs.FastMutex );

	return netdiskPartition;
}


VOID
NetdiskPartition_Reference (
	PNETDISK_PARTITION	NetdiskPartition
	)
{
    LONG result;
	
    result = InterlockedIncrement( &NetdiskPartition->ReferenceCount );
    ASSERT( result >= 0 );
}


VOID
NetdiskPartition_Dereference (
	PNETDISK_PARTITION	NetdiskPartition
	)
{
    LONG result;


    result = InterlockedDecrement( &NetdiskPartition->ReferenceCount );
    
	if (result < 0) {

		NDAS_ASSERT( FALSE );
		return;
	}

    if (result == 0) {

		PENABLED_NETDISK	enabledNetdisk = NetdiskPartition->EnabledNetdisk;
		NETDISK_ENABLE_MODE	volumeIndex;

		ASSERT( NetdiskPartition->ListEntry.Flink == NetdiskPartition->ListEntry.Blink );
				
		for (volumeIndex = 0; volumeIndex < NETDISK_SECONDARY2PRIMARY+1; volumeIndex++) {

			ASSERT( NetdiskPartition->NetdiskVolume[volumeIndex].LfsDeviceExt == NULL );
		}

		ASSERT( NetdiskPartition->ExportCount == 0 );
		ASSERT( NetdiskPartition->LocalExportCount == 0 );
		ASSERT( NetdiskPartition->ScsiportAdapterDeviceObject == NULL );
		ASSERT( NetdiskPartition->NetdiskPartitionInformation.DiskDeviceObject == NULL );

		NetdiskPartition->EnabledNetdisk = NULL;

		ASSERT( IsListEmpty( &NetdiskPartition->PrimarySessionQueue ) );

		ExFreePoolWithTag( NetdiskPartition, NETDISK_MANAGER_PARTITION_TAG );

		EnabledNetdisk_Dereference( enabledNetdisk ); // referenced by NetdiskParttion_Allocate()

		SPY_LOG_PRINT( LFS_DEBUG_NETDISK_MANAGER_TRACE,
					   ("NetdiskPartition_Dereference: NetdiskPartition = %p\n", NetdiskPartition) );
	}
}


