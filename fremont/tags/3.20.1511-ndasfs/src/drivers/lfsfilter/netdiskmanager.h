#include "LfsProc.h"


NTSTATUS
RedirectIrp(
	IN  PSECONDARY	Secondary,
	IN  PIRP		Irp,
	OUT	PBOOLEAN	FastMutexSet,
	OUT	PBOOLEAN	Retry
	);

BOOLEAN
RecoverySession(
	IN  PSECONDARY	Secondary,
	IN	BOOLEAN		FromSynchronousCall
	);

NTSTATUS
RecoverySessionAsynch(
	IN  PSECONDARY	Secondary
	);

PSECONDARY
Secondary_Create(
	IN	PLFS_DEVICE_EXTENSION	LfsDeviceExt
	)
{
	NTSTATUS				status;
	PSECONDARY				secondary;

	NETDISK_PARTITION_INFO	netdiskPartitionInfo;

	OBJECT_ATTRIBUTES		objectAttributes;
	LARGE_INTEGER			timeOut;
	ULONG					tryQuery;
	BOOLEAN					isLocalAddress;
	

	secondary = ExAllocatePoolWithTag( NonPagedPool, sizeof(SECONDARY), LFS_ALLOC_TAG );
	
	if (secondary == NULL) {

		ASSERT( LFS_INSUFFICIENT_RESOURCES );
		return NULL;
	}
	
	RtlZeroMemory( secondary, sizeof(SECONDARY) );

	RtlCopyMemory( &netdiskPartitionInfo.NetDiskAddress,
				   &LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.NetDiskAddress,
				   sizeof(netdiskPartitionInfo.NetDiskAddress) );

	netdiskPartitionInfo.UnitDiskNo = LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.UnitDiskNo;

#if __LFS_NDSC_ID__
	RtlCopyMemory( netdiskPartitionInfo.NdscId,
				   LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.NdscId,
				   NDSC_ID_LENGTH );
#endif

if (IS_WINDOWS2K())
	netdiskPartitionInfo.StartingOffset = LfsDeviceExt->NetdiskPartitionInformation.PartitionInformation.StartingOffset;
else
	netdiskPartitionInfo.StartingOffset = LfsDeviceExt->NetdiskPartitionInformation.PartitionInformationEx.StartingOffset;

	LfsTable_CleanCachePrimaryAddress( GlobalLfs.LfsTable, &netdiskPartitionInfo );

#define MAX_TRY_QUERY 2

	for (tryQuery = 0; tryQuery < MAX_TRY_QUERY; tryQuery++) {

		status = LfsTable_QueryPrimaryAddress( GlobalLfs.LfsTable,
											   &netdiskPartitionInfo,
											   &secondary->PrimaryAddress );

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("Secondary_Create: QueryPrimaryAddress %08x\n", status) );

		if (NT_SUCCESS(status)) {

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("Secondary_Create: QueryPrimaryAddress: Found PrimaryAddress :%02x:%02x:%02x:%02x:%02x:%02x/%d\n",
				secondary->PrimaryAddress.Node[0], secondary->PrimaryAddress.Node[1],
				secondary->PrimaryAddress.Node[2], secondary->PrimaryAddress.Node[3],
				secondary->PrimaryAddress.Node[4], secondary->PrimaryAddress.Node[5],
				NTOHS(secondary->PrimaryAddress.Port)) );
			break;
		}
	}

	if (status == STATUS_SUCCESS) 
		isLocalAddress = Lfs_IsLocalAddress( &secondary->PrimaryAddress );

	if (status != STATUS_SUCCESS || isLocalAddress) {

		ExFreePoolWithTag( secondary, LFS_ALLOC_TAG );
		return NULL;
	}

	secondary->Flags = SECONDARY_FLAG_INITIALIZING;

#if 0
	ExInitializeResourceLite( &secondary->RecoveryResource );
	ExInitializeResourceLite( &secondary->Resource );
	ExInitializeResourceLite( &secondary->SessionResource );
	ExInitializeResourceLite( &secondary->CreateResource );
#endif

	ExInitializeFastMutex( &secondary->FastMutex );

	secondary->ReferenceCount = 1;

	LfsDeviceExt_Reference( LfsDeviceExt );
	secondary->LfsDeviceExt = LfsDeviceExt;

	secondary->ThreadHandle = NULL;

	InitializeListHead( &secondary->FcbQueue );
	KeInitializeSpinLock( &secondary->FcbQSpinLock );
	InitializeListHead( &secondary->FileExtQueue );
    ExInitializeFastMutex( &secondary->FileExtQMutex );

#if 0
	InitializeListHead( &secondary->RecoveryCcbQueue );
    ExInitializeFastMutex( &secondary->RecoveryCcbQMutex );

	InitializeListHead( &secondary->DeletedFcbQueue );
#endif

	KeQuerySystemTime( &secondary->TryCloseTime );

#if 0
	secondary->TryCloseWorkItem = IoAllocateWorkItem( (PDEVICE_OBJECT)VolDo );
#endif

	KeInitializeEvent( &secondary->ReadyEvent, NotificationEvent, FALSE );
    
	InitializeListHead( &secondary->RequestQueue );
	KeInitializeSpinLock( &secondary->RequestQSpinLock );
	KeInitializeEvent( &secondary->RequestEvent, NotificationEvent, FALSE );

#if 0
	////////////////////////////////////////
	InitializeListHead( &secondary->FcbQueue );
	ExInitializeFastMutex( &secondary->FcbQMutex );
	/////////////////////////////////////////
#endif

	InitializeListHead( &secondary->DirNotifyList );
	FsRtlNotifyInitializeSync( &secondary->NotifySync );

	InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

	secondary->SessionId = 0;
	
	status = PsCreateSystemThread( &secondary->ThreadHandle,
								   THREAD_ALL_ACCESS,
								   &objectAttributes,
								   NULL,
								   NULL,
								   SecondaryThreadProc,
								   secondary );

	if (!NT_SUCCESS(status)) {

		ASSERT( LFS_UNEXPECTED );
		Secondary_Close( secondary );
		
		return NULL;
	}

	status = ObReferenceObjectByHandle( secondary->ThreadHandle,
										FILE_READ_DATA,
										NULL,
										KernelMode,
										&secondary->ThreadObject,
										NULL );

	if (!NT_SUCCESS(status)) {

		ASSERT( LFS_INSUFFICIENT_RESOURCES );
		Secondary_Close( secondary );
		
		return NULL;
	}

	secondary->SessionId ++;

	timeOut.QuadPart = -LFS_TIME_OUT;		
	status = KeWaitForSingleObject( &secondary->ReadyEvent,
									Executive,
									KernelMode,
									FALSE,
									&timeOut );

	if (status != STATUS_SUCCESS) {
	
		LfsDbgBreakPoint();
		Secondary_Close( secondary );
		
		return NULL;
	}

	KeClearEvent( &secondary->ReadyEvent );

	ExAcquireFastMutex( &secondary->FastMutex );

	if (!FlagOn(secondary->Thread.Flags, SECONDARY_THREAD_FLAG_START) ||
		FlagOn(secondary->Thread.Flags, SECONDARY_THREAD_FLAG_STOPED)) {

		if (secondary->Thread.SessionStatus != STATUS_DISK_CORRUPT_ERROR &&
			secondary->Thread.SessionStatus != STATUS_UNRECOGNIZED_VOLUME) {
	
			ExReleaseFastMutex( &secondary->FastMutex );

			Secondary_Close( secondary );
			return NULL;
		}
	} 

	ExReleaseFastMutex( &secondary->FastMutex );

	ASSERT( secondary->Thread.SessionContext.SessionSlotCount == 1 );
	
	KeInitializeSemaphore( &secondary->Semaphore, 
						   secondary->Thread.SessionContext.SessionSlotCount, 
						   MAX_SLOT_PER_SESSION );

	ClearFlag( secondary->Flags, SECONDARY_FLAG_INITIALIZING );
	SetFlag( secondary->Flags, SECONDARY_FLAG_START );

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
				("Secondary_Create: The client thread are ready secondary = %p\n", secondary) );

	return secondary;
}


VOID
Secondary_Close (
	IN  PSECONDARY	Secondary
	)
{
	NTSTATUS		status;
	LARGE_INTEGER	timeOut;

	PLIST_ENTRY			secondaryRequestEntry;
	PSECONDARY_REQUEST	secondaryRequest;


	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO, 
				   ("Secondary close Secondary = %p\n", Secondary) );

	ExAcquireFastMutex( &Secondary->FastMutex );

	//ASSERT( !FlagOn(Secondary->Flags, SECONDARY_FLAG_RECONNECTING) );

	if (FlagOn(Secondary->Flags, SECONDARY_FLAG_CLOSED)) {

		//ASSERT( FALSE );
		ExReleaseFastMutex( &Secondary->FastMutex );
		return;
	}

	SetFlag( Secondary->Flags, SECONDARY_FLAG_CLOSED );

	ExReleaseFastMutex( &Secondary->FastMutex );

	FsRtlNotifyUninitializeSync( &Secondary->NotifySync );

	if (Secondary->ThreadHandle == NULL) {

		//ASSERT( FALSE );
		Secondary_Dereference( Secondary );
		return;
	}

	ASSERT( Secondary->ThreadObject != NULL );

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("Secondary close SECONDARY_REQ_DISCONNECT Secondary = %p\n", Secondary) );

	secondaryRequest = AllocSecondaryRequest( Secondary, 0, FALSE );
	secondaryRequest->RequestType = SECONDARY_REQ_DISCONNECT;

	QueueingSecondaryRequest( Secondary, secondaryRequest );

	secondaryRequest = AllocSecondaryRequest( Secondary, 0, FALSE );
	secondaryRequest->RequestType = SECONDARY_REQ_DOWN;

	QueueingSecondaryRequest( Secondary, secondaryRequest );

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("Secondary close SECONDARY_REQ_DISCONNECT end Secondary = %p\n", Secondary) );

	timeOut.QuadPart = -LFS_TIME_OUT;

	status = KeWaitForSingleObject( Secondary->ThreadObject,
									Executive,
									KernelMode,
									FALSE,
									&timeOut );

	if (status == STATUS_SUCCESS) {
	   
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("Secondary_Close: thread stoped Secondary = %p\n", Secondary));

		ObDereferenceObject( Secondary->ThreadObject );

		Secondary->ThreadHandle = NULL;
		Secondary->ThreadObject = NULL;
	
	} else {

		ASSERT( LFS_BUG );
		return;
	}

	ASSERT( IsListEmpty(&Secondary->FcbQueue) );
	ASSERT( IsListEmpty(&Secondary->FileExtQueue) );
	ASSERT( IsListEmpty(&Secondary->RequestQueue) );
	ASSERT( Secondary->FileExtCount == 0 );

	while (secondaryRequestEntry = ExInterlockedRemoveHeadList(&Secondary->RequestQueue,
															   &Secondary->RequestQSpinLock)) {

		PSECONDARY_REQUEST secondaryRequest2;

		InitializeListHead( secondaryRequestEntry );
			
		secondaryRequest2 = CONTAINING_RECORD( secondaryRequestEntry,
											   SECONDARY_REQUEST,
											   ListEntry );
        
		secondaryRequest2->ExecuteStatus = STATUS_IO_DEVICE_ERROR;
		
		if (secondaryRequest2->Synchronous == TRUE)
			KeSetEvent( &secondaryRequest2->CompleteEvent, IO_DISK_INCREMENT, FALSE );
		else
			DereferenceSecondaryRequest( secondaryRequest2 );
	}
	
	Secondary_Dereference( Secondary );

	return;
}


VOID
Secondary_Reference (
	IN  PSECONDARY	Secondary
	)
{
    LONG result;
	
    result = InterlockedIncrement ( &Secondary->ReferenceCount );

    ASSERT (result >= 0);
}


VOID
Secondary_Dereference (
	IN  PSECONDARY	Secondary
	)
{
    LONG result;
	
    result = InterlockedDecrement( &Secondary->ReferenceCount) ;
    ASSERT (result >= 0);

    if (result == 0) {

		PLFS_DEVICE_EXTENSION lfsDeviceExt = Secondary->LfsDeviceExt;

		ExFreePoolWithTag( Secondary, LFS_ALLOC_TAG );

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("Secondary_Dereference: Secondary is Freed Secondary  = %p\n", Secondary) );
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("Secondary_Dereference: LfsDeviceExt->Reference = %d\n", lfsDeviceExt->ReferenceCount) );

		LfsDeviceExt_Dereference( lfsDeviceExt );
	}
}


PSECONDARY_REQUEST
AllocSecondaryRequest(
	IN	PSECONDARY	Secondary,
	IN 	UINT32		MessageSize,
	IN	BOOLEAN		Synchronous
	) 
{
	PSECONDARY_REQUEST	secondaryRequest;
	ULONG				allocationSize;
	
	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );

	allocationSize = FIELD_OFFSET(SECONDARY_REQUEST, NdfsMessage) + MessageSize;

	secondaryRequest = ExAllocatePoolWithTag( NonPagedPool,
											  allocationSize,
											  SECONDARY_MESSAGE_TAG );

	if (secondaryRequest == NULL) {

		ASSERT( LFS_INSUFFICIENT_RESOURCES );
		return NULL;
	}

	RtlZeroMemory( secondaryRequest, allocationSize );

	secondaryRequest->ReferenceCount = 1;
	InitializeListHead( &secondaryRequest->ListEntry );

	secondaryRequest->Synchronous = Synchronous;
	KeInitializeEvent( &secondaryRequest->CompleteEvent, NotificationEvent, FALSE );

	ExAcquireFastMutex( &Secondary->FastMutex );
	secondaryRequest->SessionId = Secondary->SessionId;
	ExReleaseFastMutex( &Secondary->FastMutex );

	secondaryRequest->NdfsMessageAllocationSize = MessageSize;

	return secondaryRequest;
}


VOID
ReferenceSecondaryRequest(
	IN	PSECONDARY_REQUEST	SecondaryRequest
	) 
{
	LONG	result;

	result = InterlockedIncrement( &SecondaryRequest->ReferenceCount );

	ASSERT( result > 0 );
}


VOID
DereferenceSecondaryRequest(
	IN  PSECONDARY_REQUEST	SecondaryRequest
	)
{
	LONG	result;


	result = InterlockedDecrement(&SecondaryRequest->ReferenceCount);

	ASSERT( result >= 0 );

	if (0 == result) {

		ASSERT( SecondaryRequest->ListEntry.Flink == SecondaryRequest->ListEntry.Blink );
		ExFreePool( SecondaryRequest );
	}
}


FORCEINLINE
NTSTATUS
QueueingSecondaryRequest(
	IN	PSECONDARY			Secondary,
	IN	PSECONDARY_REQUEST	SecondaryRequest
	)
{
	NTSTATUS	status;


	ASSERT( SecondaryRequest->ListEntry.Flink == SecondaryRequest->ListEntry.Blink );

	ExAcquireFastMutex( &Secondary->FastMutex );

	if (FlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_START) &&
		!FlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_STOPED)) {

		ExInterlockedInsertTailList( &Secondary->RequestQueue,
									 &SecondaryRequest->ListEntry,
									 &Secondary->RequestQSpinLock );

		KeSetEvent( &Secondary->RequestEvent, IO_DISK_INCREMENT, FALSE );
		status = STATUS_SUCCESS;

	} else {

		status = STATUS_UNSUCCESSFUL;
	}

	ExReleaseFastMutex( &Secondary->FastMutex );

	if (status == STATUS_UNSUCCESSFUL) {
	
		SecondaryRequest->ExecuteStatus = STATUS_IO_DEVICE_ERROR;

		if (SecondaryRequest->Synchronous == TRUE)
			KeSetEvent( &SecondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
		else
			DereferenceSecondaryRequest( SecondaryRequest );
	}

	return status;
}


VOID
Secondary_TryCloseFilExts(
	PSECONDARY Secondary
	)
{
	PLIST_ENTRY		listEntry;

	Secondary_Reference( Secondary );	
	
	if (ExTryToAcquireFastMutex(&Secondary->FileExtQMutex) == FALSE) {

		Secondary_Dereference( Secondary );
		return;
	}
	
	if (FlagOn(Secondary->Flags, SECONDARY_FLAG_RECONNECTING)) {

		ExReleaseFastMutex( &Secondary->FileExtQMutex );
		Secondary_Dereference( Secondary );
		return;
	}

	try {

		listEntry = Secondary->FileExtQueue.Flink;

		while (listEntry != &Secondary->FileExtQueue) {

			PFILE_EXTENTION				fileExt = NULL;
			PLFS_FCB					fcb;
			PSECTION_OBJECT_POINTERS	section;
			BOOLEAN						dataSectionExists;
			BOOLEAN						imageSectionExists;

			fileExt = CONTAINING_RECORD( listEntry, FILE_EXTENTION, ListEntry );
			listEntry = listEntry->Flink;

			if (fileExt->TypeOfOpen != UserFileOpen)
				break;

			fcb = fileExt->Fcb;

			if (fcb == NULL) {

				ASSERT( LFS_BUG );
				break;
			}	

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("Secondary_TryCloseFilExts: fcb->FullFileName = %wZ\n", &fileExt->Fcb->FullFileName) );

			if (fcb->UncleanCount != 0) {

				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("Secondary_TryCloseFilExts: fcb->FullFileName = %wZ\n", &fileExt->Fcb->FullFileName) );
				break;
			}

		    if (fcb->Header.PagingIoResource != NULL) {

				ASSERT( LFS_REQUIRED );
				break;
			}

			section = &fcb->NonPaged->SectionObjectPointers;			

	        //CcFlushCache(section, NULL, 0, &ioStatusBlock);

			dataSectionExists = (BOOLEAN)(section->DataSectionObject != NULL);
			imageSectionExists = (BOOLEAN)(section->ImageSectionObject != NULL);

			if (imageSectionExists) {

				(VOID)MmFlushImageSection( section, MmFlushForWrite );
			}

			if (dataSectionExists) {

		        CcPurgeCacheSection( section, NULL, 0, FALSE );
		    }
		}
	
	} finally {

		ExReleaseFastMutex( &Secondary->FileExtQMutex );
		Secondary_Dereference( Secondary );
	}

	return;
}


//
//	Pass Volume and mount manager IOCTLs through.
//	Most volume and mount manager IOCTLs need secondary's volume and disk number.
//
//	The file object of IRP is opened early through primary/secondary(NDFS) protocol.
//	We can not simply pass the IRP to the next device object
//	due to the file object of IRP created by LFS filter itself.
//	So, we needs to rebuild IOCTL IRP and send it to the next device object.
//
//
//	IOCTL:
//		IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS	0
//		IOCTL_VOLUME_IS_CLUSTERED				12
//		IOCTL_VOLUME_SUPPORTS_ONLINE_OFFLINE	1
//		IOCTL_VOLUME_ONLINE						2
//		IOCTL_VOLUME_OFFLINE					3
//		IOCTL_VOLUME_IS_OFFLINE					4
//		IOCTL_VOLUME_IS_IO_CAPABLE				5
//		IOCTL_VOLUME_QUERY_FAILOVER_SET			6
//		IOCTL_VOLUME_QUERY_VOLUME_NUMBER		7
//		IOCTL_VOLUME_LOGICAL_TO_PHYSICAL		8
//		IOCTL_VOLUME_PHYSICAL_TO_LOGICAL		9
//		IOCTL_VOLUME_IS_PARTITION				10
//		IOCTL_VOLUME_READ_PLEX					11
//		IOCTL_VOLUME_SET_GPT_ATTRIBUTES			13		Not allowed on secondary
//		IOCTL_VOLUME_GET_GPT_ATTRIBUTES			14
//
//		IOCTL_MOUNTDEV_QUERY_UNIQUE_ID              0
//		IOCTL_MOUNTDEV_UNIQUE_ID_CHANGE_NOTIFY      1
//		IOCTL_MOUNTDEV_QUERY_DEVICE_NAME			2
//		IOCTL_MOUNTDEV_QUERY_SUGGESTED_LINK_NAME    3
//		IOCTL_MOUNTDEV_LINK_CREATED                 4
//		IOCTL_MOUNTDEV_LINK_DELETED                 5


#include <mountdev.h>

#ifndef IOCTL_VOLUME_SET_GPT_ATTRIBUTES
#define IOCTL_VOLUME_SET_GPT_ATTRIBUTES CTL_CODE(IOCTL_VOLUME_BASE, 13, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif

#define IOCTL_VOLSNAP_FLUSH_AND_HOLD_WRITES         CTL_CODE(VOLSNAPCONTROLTYPE, 0, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS) // ntifs
#define IOCTL_VOLSNAP_RELEASE_WRITES                CTL_CODE(VOLSNAPCONTROLTYPE, 1, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_VOLSNAP_PREPARE_FOR_SNAPSHOT          CTL_CODE(VOLSNAPCONTROLTYPE, 2, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_VOLSNAP_ABORT_PREPARED_SNAPSHOT       CTL_CODE(VOLSNAPCONTROLTYPE, 3, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_VOLSNAP_COMMIT_SNAPSHOT               CTL_CODE(VOLSNAPCONTROLTYPE, 4, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_VOLSNAP_END_COMMIT_SNAPSHOT           CTL_CODE(VOLSNAPCONTROLTYPE, 5, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_VOLSNAP_QUERY_NAMES_OF_SNAPSHOTS      CTL_CODE(VOLSNAPCONTROLTYPE, 6, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_VOLSNAP_CLEAR_DIFF_AREA               CTL_CODE(VOLSNAPCONTROLTYPE, 7, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_VOLSNAP_ADD_VOLUME_TO_DIFF_AREA       CTL_CODE(VOLSNAPCONTROLTYPE, 8, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_VOLSNAP_QUERY_DIFF_AREA               CTL_CODE(VOLSNAPCONTROLTYPE, 9, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_VOLSNAP_SET_MAX_DIFF_AREA_SIZE        CTL_CODE(VOLSNAPCONTROLTYPE, 10, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_VOLSNAP_QUERY_DIFF_AREA_SIZES         CTL_CODE(VOLSNAPCONTROLTYPE, 11, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_VOLSNAP_DELETE_OLDEST_SNAPSHOT        CTL_CODE(VOLSNAPCONTROLTYPE, 12, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_VOLSNAP_AUTO_CLEANUP                  CTL_CODE(VOLSNAPCONTROLTYPE, 13, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VOLSNAP_DELETE_SNAPSHOT               CTL_CODE(VOLSNAPCONTROLTYPE, 14, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#if 1

NTSTATUS
Secondary_IoctlCompletion (
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
Secondary_Ioctl(
	IN  PSECONDARY			Secondary,
	IN  PIRP				Irp,
	IN PIO_STACK_LOCATION	IrpSp,
	OUT PNTSTATUS			NtStatus
	)
{
    PIO_STACK_LOCATION	irpSp = IoGetCurrentIrpStackLocation(Irp);
    KEVENT				waitEvent;
	NTSTATUS			status;
	ULONG				ioctlCode = IrpSp->Parameters.DeviceIoControl.IoControlCode;
	ULONG				devType = DEVICE_TYPE_FROM_CTL_CODE(ioctlCode);
	UCHAR				function = (UCHAR)((IrpSp->Parameters.DeviceIoControl.IoControlCode & 0x00003FFC) >> 2);

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, 
				   ("Secondary_Ioctl: IRP_MJ_DEVICE_CONTROL Entered, deviceType = %d, function = %d IOCTL_VOLUME_BASE = %d, MOUNTDEVCONTROLTYPE = %d\n",
					devType, function, IOCTL_VOLUME_BASE, MOUNTDEVCONTROLTYPE) );

	if (IS_WINDOWS2K() && Secondary->LfsDeviceExt->DiskDeviceObject == NULL) {

		ASSERT( LFS_BUG );
		return FALSE;
	}

	if (IS_WINDOWSXP_OR_LATER() && Secondary->LfsDeviceExt->MountVolumeDeviceObject == NULL) {

		ASSERT( LFS_BUG );
		return FALSE;
	}

	if (ioctlCode == IOCTL_VOLUME_SET_GPT_ATTRIBUTES) {

		Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;

		if (NtStatus)
			*NtStatus = STATUS_NOT_SUPPORTED;

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );

		return TRUE;
	}

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
				   ("Open files from secondary: LfsDeviceExt = %p, irpSp->MajorFunction = %x, irpSp->MinorFunction = %x\n", 
					 Secondary->LfsDeviceExt, irpSp->MajorFunction, irpSp->MinorFunction) );


	IoCopyCurrentIrpStackLocationToNext( Irp );
	KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

	IoSetCompletionRoutine( Irp,
							Secondary_IoctlCompletion,
							&waitEvent,
							TRUE,
							TRUE,
							TRUE );

	if (IS_WINDOWS2K())
		status = IoCallDriver( Secondary->LfsDeviceExt->DiskDeviceObject, Irp );
	else
		status = IoCallDriver( Secondary->LfsDeviceExt->MountVolumeDeviceObject, Irp );

	if (status == STATUS_PENDING) {

		status = KeWaitForSingleObject( &waitEvent,
										Executive,
										KernelMode,
										FALSE,
										NULL );

		ASSERT( status == STATUS_SUCCESS );
	}

	ASSERT( KeReadStateEvent(&waitEvent) || !NT_SUCCESS(Irp->IoStatus.Status) );
	
	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR, ("Secondary_Ioctl: Irp->IoStatus.Status = %x\n", Irp->IoStatus.Status) );

	//if (IS_WINDOWSNET() && devType == VOLSNAPCONTROLTYPE) {

	//	Irp->IoStatus.Status = STATUS_SUCCESS;
	//	Irp->IoStatus.Information = 0;
	//}


	*NtStatus = Irp->IoStatus.Status;
	IoCompleteRequest( Irp, IO_NO_INCREMENT );

	return TRUE;
}

#else

BOOLEAN
Secondary_Ioctl(
	IN  PSECONDARY			Secondary,
	IN  PIRP				Irp,
	IN PIO_STACK_LOCATION	IrpSp,
	OUT PNTSTATUS			NtStatus
	)
{
	NTSTATUS	status;
	ULONG		ioctlCode = IrpSp->Parameters.DeviceIoControl.IoControlCode;
	ULONG		devType = DEVICE_TYPE_FROM_CTL_CODE(ioctlCode);
	PVOID		InputBuffer, OutputBuffer;
	UCHAR		function = (UCHAR)((IrpSp->Parameters.FileSystemControl.FsControlCode & 0x00003FFC) >> 2);

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR, 
				   ("Secondary_Ioctl: IRP_MJ_DEVICE_CONTROL Entered, function = %d, deviType = %d\n", function, devType) );
	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR, 
				   ("ioctlCode = %x, IOCTL_VOLSNAP_CLEAR_DIFF_AREA = %x\n", ioctlCode, IOCTL_VOLSNAP_CLEAR_DIFF_AREA) );

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR, 
				   ("ioctlCode = %x, IOCTL_VOLSNAP_ADD_VOLUME_TO_DIFF_AREA = %x\n", ioctlCode, IOCTL_VOLSNAP_ADD_VOLUME_TO_DIFF_AREA) );

#if 0


	if (devType == VOLSNAPCONTROLTYPE						||
		ioctlCode == IOCTL_VOLSNAP_CLEAR_DIFF_AREA			|| 
		ioctlCode == IOCTL_VOLSNAP_ADD_VOLUME_TO_DIFF_AREA	||
		ioctlCode == IOCTL_VOLSNAP_SET_MAX_DIFF_AREA_SIZE	||
		ioctlCode == IOCTL_VOLSNAP_PREPARE_FOR_SNAPSHOT		||
		ioctlCode == IOCTL_VOLSNAP_FLUSH_AND_HOLD_WRITES	||
		ioctlCode == IOCTL_VOLSNAP_COMMIT_SNAPSHOT			||
		ioctlCode == IOCTL_VOLSNAP_RELEASE_WRITES			||
		ioctlCode == IOCTL_VOLSNAP_END_COMMIT_SNAPSHOT) {

		Irp->IoStatus.Status = STATUS_SUCCESS;
		
		if (NtStatus)
			*NtStatus = STATUS_SUCCESS;

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
		
		return TRUE;
	}

#endif

	//
	//	Only volume ioctl will be redirected here.
	//

	if (devType != IOCTL_VOLUME_BASE && devType != MOUNTDEVCONTROLTYPE) {
		
		return FALSE;
	}

	//
	// LfsFilt does not allow device name query to a directory.
	// Windows explorer seems to detect mount point by querying device name.
	// This patch also prevents disk management to set mount point on an NDAS device.
	//
	// NOTE: This patch will prevents Windows explorer to take it wrong,
	//      but this might have side effects.
	//

	if(ioctlCode == IOCTL_MOUNTDEV_QUERY_DEVICE_NAME) {

		if(	IrpSp->FileObject &&
			IrpSp->FileObject->FileName.Length){

			//
			//	Complete the IRP
			//
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
				("Secondary_Ioctl: Mount point Ioctl dismissed %x.\n", ioctlCode));

			Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
			if(NtStatus)
				*NtStatus = STATUS_INVALID_DEVICE_REQUEST;
			IoCompleteRequest(Irp, IO_DISK_INCREMENT);
			return TRUE;
		}
	}


	do {

		//
		//	We do not allow GPT change on secondary.
		//

		if (ioctlCode == IOCTL_VOLUME_SET_GPT_ATTRIBUTES) {
		
			Irp->IoStatus.Information = 0;
			status = STATUS_NOT_SUPPORTED;
			break;;
		}

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("Secondary_Ioctl: Volume IOCTL\n") );

		InputBuffer = MapInputBuffer(Irp);
		OutputBuffer = MapOutputBuffer(Irp);

		//
		//	Redirect the request to the disk device object.
		//	NTFS on XP seems to require a file object.
		//	It causes memory reference error if the new created IOCTL IRP
		//		is sent to the file system device object.
		//

		status = LfsFilterDeviceIoControl( Secondary->LfsDeviceExt->DiskDeviceObject,
										   ioctlCode,
										   InputBuffer,
										   IrpSp->Parameters.DeviceIoControl.InputBufferLength,
										   OutputBuffer,
										   IrpSp->Parameters.DeviceIoControl.OutputBufferLength,
										   NULL,
										   &Irp->IoStatus.Information );

#if 0
		if (devType == VOLSNAPCONTROLTYPE) {

			status = STATUS_SUCCESS;
			Irp->IoStatus.Information = 0;
		}
#endif

	} while(0);

	//
	//	Set return value
	//

	Irp->IoStatus.Status = status;
	if (NtStatus)
		*NtStatus = status;


	//
	//	Complete the IRP
	//

	IoCompleteRequest( Irp, IO_DISK_INCREMENT );

	return TRUE;
}


#endif

BOOLEAN 
Secondary_PassThrough(
	IN  PSECONDARY	Secondary,
	IN  PIRP		Irp,
	OUT PNTSTATUS	NtStatus
	)
{
	PIO_STACK_LOCATION  irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT		fileObject;

	NTSTATUS			redirectStatus;
	BOOLEAN				pendingReturned;

	BOOLEAN				fastMutexSet = FALSE;
	BOOLEAN				retry = FALSE;


#if DBG
	UNICODE_STRING		Mft;
	UNICODE_STRING		MftMirr;
	UNICODE_STRING		LogFile;
	UNICODE_STRING		Directory;
	UNICODE_STRING		BitMap;
	UNICODE_STRING		MountMgr;
	UNICODE_STRING		Extend;
	UNICODE_STRING		ExtendPlus;
	UNICODE_STRING		System;
	

	RtlInitUnicodeString( &Mft, L"\\$Mft" );
	RtlInitUnicodeString( &MftMirr, L"\\$MftMirr" );
	RtlInitUnicodeString( &LogFile, L"\\$LogFile" );
	RtlInitUnicodeString( &Directory, L"\\$Directory" );
	RtlInitUnicodeString( &BitMap, L"\\$BitMap" );
	RtlInitUnicodeString( &MountMgr, L"\\:$MountMgrRemoteDatabase" );
	RtlInitUnicodeString( &Extend, L"\\$Extend\\$Reparse" );
	RtlInitUnicodeString( &ExtendPlus, L"\\$Extend\\$Reparse:$R:$INDEX_ALLOCATION" );
	RtlInitUnicodeString( &System, L"\\System Volume Information\\" );
#endif

	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );

	//
	//	make sure that Secondary context will not be gone during operation.
	//
	Secondary_Reference( Secondary );
	fileObject = irpSp->FileObject;

	//
	//	Irp may come in with Irp->PendingReturned TRUE.
	//	We need to save pendingReturned to see if Irp->PendingReturned is changed during the following process.
	//
	pendingReturned = Irp->PendingReturned;
 
#if DBG
	
	if (irpSp->MajorFunction == IRP_MJ_QUERY_EA			||			// 0x07
		irpSp->MajorFunction == IRP_MJ_SET_EA			||			// 0x08
		irpSp->MajorFunction == IRP_MJ_LOCK_CONTROL		||			// 0x11
		irpSp->MajorFunction == IRP_MJ_QUERY_QUOTA		||			// 0x19
		irpSp->MajorFunction == IRP_MJ_SET_QUOTA		||			// 0x1a
		irpSp->MajorFunction == IRP_MJ_QUERY_SECURITY	||			// 0x14
		irpSp->MajorFunction == IRP_MJ_SET_SECURITY) {	 			// 0x15
	
		PrintIrp( LFS_DEBUG_SECONDARY_TRACE, "Secondary_PassThrough", Secondary->LfsDeviceExt, Irp );
	
	} else {

		PrintIrp( LFS_DEBUG_SECONDARY_TRACE, "Secondary_PassThrough", Secondary->LfsDeviceExt, Irp );
	}

#endif

	if (fileObject && fileObject->Flags & FO_DIRECT_DEVICE_OPEN) {

		ASSERT( LFS_REQUIRED );
		DbgPrint( "Direct device open\n" );
	}

	if (irpSp->MajorFunction == IRP_MJ_CREATE					||	// 0x00
		irpSp->MajorFunction == IRP_MJ_CLOSE					||	// 0x01
		irpSp->MajorFunction == IRP_MJ_READ						||  // 0x03
		irpSp->MajorFunction == IRP_MJ_WRITE					||  // 0x04
		irpSp->MajorFunction == IRP_MJ_QUERY_INFORMATION		||  // 0x05
		irpSp->MajorFunction == IRP_MJ_SET_INFORMATION			||	// 0x06
		irpSp->MajorFunction == IRP_MJ_QUERY_EA					||	// 0x07
		irpSp->MajorFunction == IRP_MJ_SET_EA					||	// 0x08
		irpSp->MajorFunction == IRP_MJ_FLUSH_BUFFERS			||	// 0x09
		irpSp->MajorFunction == IRP_MJ_QUERY_VOLUME_INFORMATION	||	// 0x0a
		irpSp->MajorFunction == IRP_MJ_SET_VOLUME_INFORMATION	||	// 0x0b
		irpSp->MajorFunction == IRP_MJ_DIRECTORY_CONTROL		||	// 0x0c
		irpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL		||	// 0x0c
		irpSp->MajorFunction == IRP_MJ_LOCK_CONTROL				||	// 0x11
		irpSp->MajorFunction == IRP_MJ_CLEANUP					||	// 0x12
		irpSp->MajorFunction == IRP_MJ_QUERY_SECURITY			||	// 0x14
		irpSp->MajorFunction == IRP_MJ_SET_SECURITY				||	// 0x15
		irpSp->MajorFunction == IRP_MJ_QUERY_QUOTA				||	// 0x19
		irpSp->MajorFunction == IRP_MJ_SET_QUOTA) {					// 0x1a
	
		ASSERT( fileObject );
		
		if (irpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) {

			if (irpSp->MinorFunction == IRP_MN_MOUNT_VOLUME		||
				irpSp->MinorFunction == IRP_MN_VERIFY_VOLUME	||
				irpSp->MinorFunction == IRP_MN_LOAD_FILE_SYSTEM) {
		
				ASSERT( LFS_UNEXPECTED );
				ASSERT( Secondary_LookUpFileExtension(Secondary, fileObject) == NULL );

				PrintIrp( LFS_DEBUG_SECONDARY_TRACE, "Secondary_PassThrough", Secondary->LfsDeviceExt, Irp );

				Secondary_Dereference( Secondary );
				return FALSE;
			}
		}
		
	} else if(irpSp->MajorFunction == IRP_MJ_PNP) {							// 0x1b
	
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("Secondary_PassThrough: IRP_MJ_PNP %x\n", irpSp->MinorFunction) );

		if (irpSp->MinorFunction == IRP_MN_QUERY_REMOVE_DEVICE) {

			if (FlagOn(Secondary->Flags, SECONDARY_FLAG_RECONNECTING)) {  // While Disabling and disconnected
	
				*NtStatus = Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest( Irp, IO_DISK_INCREMENT );
				Secondary_Dereference( Secondary );

				return TRUE;
			}

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("Secondary_PassThrough: IRP_MN_QUERY_REMOVE_DEVICE Entered\n") );
		
			Secondary_TryCloseFilExts( Secondary );
			
			if (!IsListEmpty(&Secondary->FcbQueue)) {

				LARGE_INTEGER interval;
				
				// Wait all files closed
				interval.QuadPart = (1 * DELAY_ONE_SECOND);      //delay 1 seconds
				KeDelayExecutionThread( KernelMode, FALSE, &interval );
			}
			
			if (!IsListEmpty(&Secondary->FcbQueue)) {

				*NtStatus = Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest( Irp, IO_DISK_INCREMENT );
				Secondary_Dereference( Secondary );

				return TRUE;
			}

			Secondary_Dereference( Secondary );
			return FALSE;
		
		} else if (irpSp->MinorFunction == IRP_MN_REMOVE_DEVICE) {

			if (!IsListEmpty(&Secondary->FcbQueue)) {

				ASSERT( LFS_BUG );
				*NtStatus = Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest( Irp, IO_DISK_INCREMENT );
				Secondary_Dereference( Secondary );
				return TRUE;
			}

			Secondary_Dereference( Secondary );
			return FALSE;			
		
		} else {

			if (fileObject && Secondary_LookUpFileExtension(Secondary, fileObject)) {

				*NtStatus = Irp->IoStatus.Status = STATUS_SUCCESS;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest( Irp, IO_DISK_INCREMENT );
				Secondary_Dereference( Secondary );
				
				return TRUE;
			}

			Secondary_Dereference( Secondary );
			return FALSE;			
		}
	
	} else if (irpSp->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL	|| 	// 0x0f
			   irpSp->MajorFunction == IRP_MJ_SHUTDOWN					||  // 0x10
			   irpSp->MajorFunction == IRP_MJ_CREATE_MAILSLOT			||  // 0x13
			   irpSp->MajorFunction == IRP_MJ_POWER						||  // 0x16
			   irpSp->MajorFunction == IRP_MJ_SYSTEM_CONTROL			||  // 0x17
			   irpSp->MajorFunction == IRP_MJ_DEVICE_CHANGE) {				// 0x18
	
		ASSERT( LFS_REQUIRED );
		PrintIrp( LFS_DEBUG_SECONDARY_TRACE, "Secondary_PassThrough", Secondary->LfsDeviceExt, Irp );

		Secondary_Dereference( Secondary );

		return FALSE;

	} else if (irpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL) {

		BOOLEAN			bret;
		PFILE_EXTENTION	fileExt = NULL;

		fileExt = Secondary_LookUpFileExtension(Secondary, fileObject);
		
		if (fileExt == NULL || fileExt->TypeOfOpen != UserVolumeOpen) {

			Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;

			if (NtStatus)
				*NtStatus = STATUS_INVALID_PARAMETER;

			IoCompleteRequest( Irp, IO_DISK_INCREMENT );

			return TRUE;
		}

		bret = Secondary_Ioctl( Secondary, Irp, irpSp, NtStatus );

		if (bret == TRUE) {
		
			Secondary_Dereference( Secondary );
			return TRUE;
		}

	} else {

		ASSERT( irpSp->MajorFunction < IRP_MJ_CREATE || irpSp->MajorFunction > IRP_MJ_MAXIMUM_FUNCTION );
		ASSERT( LFS_UNEXPECTED );

		Secondary_Dereference( Secondary );
		return FALSE;
	}		

	PrintIrp( LFS_DEBUG_SECONDARY_TRACE, "Secondary_PassThrough", Secondary->LfsDeviceExt, Irp );

#if DBG
	
	if (fileObject->FileName.Length < 2										||
		RtlEqualUnicodeString(&Mft, &fileObject->FileName, TRUE)			||
		RtlEqualUnicodeString(&MftMirr, &fileObject->FileName, TRUE)		||
		RtlEqualUnicodeString(&LogFile, &fileObject->FileName, TRUE)		||
		RtlEqualUnicodeString(&Directory, &fileObject->FileName, TRUE)		||
		RtlEqualUnicodeString(&MountMgr, &fileObject->FileName, TRUE)		||
		RtlEqualUnicodeString(&Extend, &fileObject->FileName, TRUE)			||
		RtlEqualUnicodeString(&ExtendPlus, &fileObject->FileName, TRUE)		||
		RtlEqualUnicodeString(&System, &fileObject->FileName, TRUE)			||
		RtlEqualUnicodeString(&BitMap, &fileObject->FileName, TRUE)			||
		fileObject->FileName.Length >=2 && fileObject->FileName.Buffer[1] == L'$') {

		PrintIrp( LFS_DEBUG_SECONDARY_TRACE, "Secondary_PassThrough", Secondary->LfsDeviceExt, Irp );

		if (RtlEqualUnicodeString(&Mft, &fileObject->FileName, TRUE)		|| 
			RtlEqualUnicodeString(&MftMirr, &fileObject->FileName, TRUE)	||
			RtlEqualUnicodeString(&LogFile, &fileObject->FileName, TRUE)	||
			RtlEqualUnicodeString(&BitMap, &fileObject->FileName, TRUE)		||
			RtlEqualUnicodeString(&Directory, &fileObject->FileName, TRUE)) {
		}
	}		

#endif

    ASSERT( fileObject/*&& fileObject->FileName.Length*/ );
	
	if (Secondary->Thread.SessionStatus == STATUS_DISK_CORRUPT_ERROR) {

		*NtStatus = Irp->IoStatus.Status = STATUS_DISK_CORRUPT_ERROR;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest( Irp, IO_DISK_INCREMENT );

		Secondary_Dereference( Secondary );
		
		return TRUE;
	
	} else if (Secondary->Thread.SessionStatus == STATUS_UNRECOGNIZED_VOLUME) {

		*NtStatus = Irp->IoStatus.Status = STATUS_DISK_CORRUPT_ERROR;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest( Irp, IO_DISK_INCREMENT );

		Secondary_Dereference( Secondary );
		
		return TRUE;
	}

	while (1) {

		if (fastMutexSet == TRUE)
			LfsDbgBreakPoint();

		ExAcquireFastMutex( &Secondary->FastMutex );

		if (FlagOn(Secondary->Flags, SECONDARY_FLAG_CLOSED)) {

			ExReleaseFastMutex( &Secondary->FastMutex );

			ASSERT( irpSp->MajorFunction == IRP_MJ_CREATE );
					
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR, 
						   ("Secondary is already closed Secondary = %p\n", Secondary) );

			*NtStatus = Irp->IoStatus.Status = STATUS_TOO_LATE;
			Irp->IoStatus.Information = 0;
			break;
		}

		if (FlagOn(Secondary->Flags, SECONDARY_FLAG_ERROR)) {

			if (irpSp->MajorFunction != IRP_MJ_CLOSE) {

				ExReleaseFastMutex( &Secondary->FastMutex );

				if (irpSp->MajorFunction == IRP_MJ_CREATE)
					*NtStatus = Irp->IoStatus.Status = STATUS_IO_DEVICE_ERROR;
				else
					*NtStatus = Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;

				Irp->IoStatus.Information = 0;
			
				break;
			}
		}
		
		ExReleaseFastMutex( &Secondary->FastMutex );

		if (!FlagOn(Secondary->Flags, SECONDARY_FLAG_RECONNECTING)) {

			ASSERT( Secondary->ThreadObject );
			//ASSERT( FlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_CONNECTED) );
		}

		ASSERT( fileObject->DeviceObject );
		
#if DBG
		InterlockedIncrement( &LfsObjectCounts.RedirectIrpCount );
#endif

		//
		// Prevent secondary operation from being suspended.
		//
		KeEnterCriticalRegion();
		//
		//	redirect the IRP to the primary host.
		//
		redirectStatus = RedirectIrp( Secondary, Irp, &fastMutexSet, &retry );
		KeLeaveCriticalRegion();

#if DBG

		if (LfsObjectCounts.RedirectIrpCount > 3)
			SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE, ("LfsObjectCounts.RedirectIrpCount = %d\n", LfsObjectCounts.RedirectIrpCount) );

		InterlockedDecrement( &LfsObjectCounts.RedirectIrpCount );

		if (redirectStatus != STATUS_SUCCESS) {

			PrintIrp( LFS_DEBUG_SECONDARY_ERROR, "RedirectIrp Error", Secondary->LfsDeviceExt, Irp );

			if (irpSp->MajorFunction == IRP_MJ_WRITE) {
			
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR, ("Redirected-write failed at offset %lx, key=%x, length=%x\n",
															irpSp->Parameters.Write.ByteOffset.QuadPart, 
															irpSp->Parameters.Write.Key, 
															irpSp->Parameters.Write.Length) );
			}
		}

#endif
		if (retry == TRUE)
			continue;

		//
		//	IRP waiting for the semaphore is completed due to potential dead volume-lock
		//

		if (redirectStatus == STATUS_DEVICE_REQUIRES_CLEANING) {

			PrintIrp( LFS_DEBUG_SECONDARY_INFO, 
					  "redirectStatus == STATUS_DEVICE_REQUIRES_CLEANING Error", 
					  Secondary->LfsDeviceExt, 
					  Irp );

			*NtStatus = Irp->IoStatus.Status = STATUS_ACCESS_DENIED;

			break;
		}
#if 0
		if (redirectStatus == STATUS_ABANDONED) {

			SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE, ("redirectStatus == STATUS_ABANDONED\n") );

			ASSERT( fastMutexSet == TRUE );

			ExAcquireFastMutex( &Secondary->FastMutex );

			if (Secondary->SemaphoreConsumeRequiredCount == 0)
				KeReleaseSemaphore( &Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE );
			else
				Secondary->SemaphoreConsumeRequiredCount --;
			
			ExReleaseFastMutex( &Secondary->FastMutex );

			fastMutexSet = FALSE;

			continue;
		}
#endif
		if (redirectStatus == STATUS_WORKING_SET_LIMIT_RANGE) {

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("redirectStatus == STATUS_WORKING_SET_LIMIT_RANGE\n") );

			ASSERT( fastMutexSet == TRUE );
			fastMutexSet = FALSE;

			continue;
		}

		if (redirectStatus == STATUS_SUCCESS) {

			ASSERT( fastMutexSet == FALSE );

			*NtStatus = Irp->IoStatus.Status;

			if (*NtStatus == STATUS_PENDING) {

				ASSERT( LFS_BUG );
				IoMarkIrpPending(Irp);
			
			} else {

				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("completed. Stat=%08lx Info=%d\n", 
															Irp->IoStatus.Status, Irp->IoStatus.Information) );
				PrintIrp( LFS_DEBUG_SECONDARY_TRACE, "RedirectIrp", Secondary->LfsDeviceExt, Irp );
				
				if (!(*NtStatus == STATUS_SUCCESS || 
					  irpSp->MajorFunction == IRP_MJ_CREATE && *NtStatus == STATUS_NO_SUCH_FILE && Irp->IoStatus.Information == 0				|| 
					  irpSp->MajorFunction == IRP_MJ_CREATE && *NtStatus == STATUS_OBJECT_NAME_NOT_FOUND && Irp->IoStatus.Information == 0		|| 
					  irpSp->MajorFunction == IRP_MJ_CREATE && *NtStatus == STATUS_OBJECT_PATH_NOT_FOUND && Irp->IoStatus.Information == 0		|| 
					  irpSp->MajorFunction == IRP_MJ_DIRECTORY_CONTROL && *NtStatus == STATUS_NO_MORE_FILES && Irp->IoStatus.Information == 0	|| 
					  irpSp->MajorFunction == IRP_MJ_DIRECTORY_CONTROL && *NtStatus == STATUS_NO_SUCH_FILE && Irp->IoStatus.Information == 0	|| 
					  irpSp->MajorFunction == IRP_MJ_READ && *NtStatus == STATUS_END_OF_FILE && Irp->IoStatus.Information == 0)) {

					SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("completed. Stat=%08lx Info=%d\t", 
																Irp->IoStatus.Status, Irp->IoStatus.Information) );
					PrintIrp( LFS_DEBUG_SECONDARY_TRACE, NULL, Secondary->LfsDeviceExt, Irp );
				}
			}

			break;
		
		} else if (redirectStatus == STATUS_TIMEOUT) {

			*NtStatus = Irp->IoStatus.Status = STATUS_PENDING;
			IoMarkIrpPending(Irp);

			break;
		
		} else {

			PLIST_ENTRY		secondaryRequestEntry;
	
			ExAcquireFastMutex( &Secondary->FastMutex );

			ASSERT( FlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) );

			if (Secondary->LfsDeviceExt->SecondaryState == CONNECT_TO_LOCAL_STATE		|| 
				FlagOn(Secondary->LfsDeviceExt->Flags, LFS_DEVICE_SURPRISE_REMOVAL)		||
				GlobalLfs.ShutdownOccured == TRUE) {

				SetFlag( Secondary->Flags, SECONDARY_FLAG_ERROR );
				ExReleaseFastMutex( &Secondary->FastMutex );
				
				*NtStatus = Irp->IoStatus.Status = STATUS_IO_DEVICE_ERROR;
				Irp->IoStatus.Information = 0;
	
				break;
			}

			if (redirectStatus == STATUS_ALERTED || redirectStatus == STATUS_USER_APC) {

				ASSERT( fastMutexSet == FALSE );

				ExReleaseFastMutex( &Secondary->FastMutex );
				continue;
			}

			if (fastMutexSet == FALSE)
				LfsDbgBreakPoint();

			if (FlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED)) {

				//BOOLEAN			recoveryResult;

				ASSERT( !FlagOn(Secondary->Flags, SECONDARY_FLAG_RECONNECTING) );
				SetFlag( Secondary->Flags, SECONDARY_FLAG_RECONNECTING );

#if 0
				ASSERT( Secondary->SemaphoreReturnCount == 0 );
#endif

				while(1) {

					NTSTATUS		waitStatus;
					LARGE_INTEGER	timeOut;
								
					timeOut.QuadPart = HZ >> 2; 

					ExReleaseFastMutex( &Secondary->FastMutex );
					
					waitStatus = KeWaitForSingleObject( &Secondary->Semaphore,
														Executive,
														KernelMode,
														FALSE,
														&timeOut );

					if (waitStatus != STATUS_TIMEOUT)
						LfsDbgBreakPoint();

					ExAcquireFastMutex( &Secondary->FastMutex );

					if (waitStatus == STATUS_TIMEOUT)
						break;
			
					if (waitStatus == STATUS_SUCCESS) {

						//Secondary->SemaphoreReturnCount++;
						SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,("Semaphore to block other IRPs\n") );
					
					} else {

						ASSERT( LFS_UNEXPECTED );
						break;
					}	
				}

				//Secondary->SemaphoreReturnCount++;

				ExReleaseFastMutex( &Secondary->FastMutex );

				if (FlagOn(Secondary->Flags, SECONDARY_FLAG_CLOSED)) {

					ASSERT( irpSp->MajorFunction == IRP_MJ_CREATE );
					
					SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR, 
								   ("Secondary is already closed Secondary = %p\n", Secondary) );

					ClearFlag( Secondary->Flags, SECONDARY_FLAG_RECONNECTING );

					*NtStatus = Irp->IoStatus.Status = STATUS_TOO_LATE;
					Irp->IoStatus.Information = 0;
					break;
				}

				//
				//	Try to recover lost session asynchronously
				//

				if (fastMutexSet == FALSE)
					LfsDbgBreakPoint();

				RecoverySessionAsynch( Secondary );
				fastMutexSet = FALSE;
				continue;

#if 0

#if __ENABLE_DEAD_LOCKVOLUME_PREVENTION__

				//
				//	Complete this IRP with an error
				//	if this IRP comes from potential dead-lock processes.
				//	This thread will perform volume lock or wait volume lock.
				//	Volume lock notify those processes and this will cause
				//	dead lock.
				//
				//	Currently identified process:
				//			VMwareService.exe
				//			svchost.exe
				//			explorer.exe
				//

				if (Irp->RequestorMode == UserMode) {
				
					if (irpSp->MajorFunction == IRP_MJ_CREATE && irpSp->MinorFunction == 0) {

						if (fileObject) {

							UNICODE_STRING		rootDir;
							
							RtlInitUnicodeString( &rootDir, L"\\" );

							if (1) { //RtlEqualUnicodeString(&rootDir, &fileObject->FileName, TRUE) || fileObject->FileName.Length == 0) {

								SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,("++++++++++++++ Potential PnP dead-lock detected. +++++++++++++++\n") );

								//
								//	Turn this IRP's semaphore to the counter
								//	that will be released after recovery
								//

								ExAcquireFastMutex( &Secondary->FastMutex );
								Secondary->SemaphoreReturnCount++;
								ExReleaseFastMutex( &Secondary->FastMutex );

								//
								//	Try to recover lost session asynchronously
								//

								RecoverySessionAsynch( Secondary );

								//
								//	Complete the IRP with an error.
								//

								*NtStatus = Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
								Irp->IoStatus.Information = 0;
								break;
							}
						}
					}
				}
#endif
				//
				//	Try to recover lost session
				//

				recoveryResult = RecoverySession( Secondary, TRUE );

				ExAcquireFastMutex( &Secondary->FastMutex );

				//
				//	Release semaphore acquired to block other IRPs
				//

				while (Secondary->SemaphoreReturnCount) {

					KeReleaseSemaphore( &Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE );
					Secondary->SemaphoreReturnCount--;
				}

				if (recoveryResult == TRUE) {

					ClearFlag( Secondary->Flags, SECONDARY_FLAG_RECONNECTING );

					if(Secondary->SemaphoreConsumeRequiredCount == 0)
						KeReleaseSemaphore( &Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE );
					else
						Secondary->SemaphoreConsumeRequiredCount--;

					ExReleaseFastMutex( &Secondary->FastMutex );

					fastMutexSet = FALSE;

					continue;
				}
#endif

			}

			if (fastMutexSet != TRUE && redirectStatus != STATUS_ALERTED && redirectStatus != STATUS_USER_APC)
				LfsDbgBreakPoint();

			//
			//	Recovery failed.
			//	Complete all pending IRPs with error.
			//

			SetFlag( Secondary->Flags, SECONDARY_FLAG_ERROR );
			ExReleaseFastMutex( &Secondary->FastMutex );

			if (fastMutexSet == TRUE)
				KeReleaseSemaphore( &Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE );

			fastMutexSet = FALSE;

			*NtStatus = Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
			Irp->IoStatus.Information = 0;

			while (secondaryRequestEntry = ExInterlockedRemoveHeadList( &Secondary->RequestQueue,
																		&Secondary->RequestQSpinLock)) {

				PSECONDARY_REQUEST	secondaryRequest;

				InitializeListHead( secondaryRequestEntry );

				secondaryRequest = CONTAINING_RECORD( secondaryRequestEntry, SECONDARY_REQUEST, ListEntry );

				ASSERT( secondaryRequest->RequestType == SECONDARY_REQ_DISCONNECT );
				secondaryRequest->ExecuteStatus = STATUS_ABANDONED;

				ASSERT( secondaryRequest->Synchronous == TRUE );
				KeSetEvent( &secondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );				
			}

			break;
		}

		break;
	}

	if (*NtStatus != STATUS_PENDING) {

		if (!pendingReturned && Irp->PendingReturned) {

			ASSERT( FALSE );
		}

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
	}

	Secondary_Dereference( Secondary );

	return TRUE;
}


FORCEINLINE
PSECONDARY_REQUEST
ALLOC_WINXP_SECONDARY_REQUEST(
	IN PSECONDARY	Secondary,
	IN _U8			IrpMajorFunction,
	IN UINT32		DataSize
	)
{
	UNREFERENCED_PARAMETER( IrpMajorFunction );

	return AllocSecondaryRequest( Secondary,
								  (((sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER)) > (sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER))) ?
								  (sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER)) : (sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER)))
								  + DataSize,
								  TRUE );
}





BOOLEAN
RecoverySession(
	IN  PSECONDARY	Secondary,
	IN	BOOLEAN		FromSynchronousCall
	)
{
	BOOLEAN					result;
	NETDISK_PARTITION_INFO	netdiskPartitionInfo;

	LIST_ENTRY				tempRequestQueue;
	PLIST_ENTRY				secondaryRequestEntry;
	_U16					mid;
	_U16					previousSessionSlotCount = Secondary->Thread.SessionContext.SessionSlotCount;
	_U16					queuedRequestCount = 0;

	ULONG					reconnectionTry;
    PLIST_ENTRY				fileExtlistEntry;


#if DBG
#else
	UNREFERENCED_PARAMETER( FromSynchronousCall );
#endif

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("RecoverySession Called Secondary = %p\n", Secondary) );

	ASSERT( Secondary->ThreadHandle );

	RtlCopyMemory( &netdiskPartitionInfo.NetDiskAddress,
				   &Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.NetDiskAddress,
				   sizeof(netdiskPartitionInfo.NetDiskAddress) );

	netdiskPartitionInfo.UnitDiskNo = Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.UnitDiskNo;

#if __LFS_NDSC_ID__
	RtlCopyMemory( netdiskPartitionInfo.NdscId,
				   Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.NdscId,
				   NDSC_ID_LENGTH );
#endif

if (IS_WINDOWS2K())
	netdiskPartitionInfo.StartingOffset = Secondary->LfsDeviceExt->NetdiskPartitionInformation.PartitionInformation.StartingOffset;
else
	netdiskPartitionInfo.StartingOffset = Secondary->LfsDeviceExt->NetdiskPartitionInformation.PartitionInformationEx.StartingOffset;

	InitializeListHead( &tempRequestQueue );

	//
	//	Collect secondary's request into a temporary queue.
	//

	for (mid=0; mid < Secondary->Thread.SessionContext.SessionSlotCount; mid++) {

		if (Secondary->Thread.SessionSlot[mid] != NULL) {

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("RecoverySession: insert to tempRequestQueue\n") );
		
			InsertTailList( &tempRequestQueue, &Secondary->Thread.SessionSlot[mid]->ListEntry );	
			Secondary->Thread.SessionSlot[mid] = NULL;
			queuedRequestCount++;
		}
	}

	while (secondaryRequestEntry = ExInterlockedRemoveHeadList(&Secondary->RequestQueue,
															   &Secondary->RequestQSpinLock)) {

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("RecoverySession: insert to tempRequestQueue\n") );
		InsertTailList( &tempRequestQueue, secondaryRequestEntry );
		queuedRequestCount++;
	}

	result = FALSE;

#define MAX_RECONNECTION_TRY	300

	for (reconnectionTry=0; reconnectionTry<MAX_RECONNECTION_TRY; reconnectionTry++) {

		LARGE_INTEGER		timeOut;
		NTSTATUS			waitStatus;
		NTSTATUS			tableStatus;
		OBJECT_ATTRIBUTES	objectAttributes;
		LARGE_INTEGER		interval;

		if (reconnectionTry) {
		
			interval.QuadPart = (5 * DELAY_ONE_SECOND);      //delay 5 seconds
			KeDelayExecutionThread( KernelMode, FALSE, &interval );
		}

		if (GlobalLfs.ShutdownOccured == TRUE) {

			return FALSE;
		}

		if (FlagOn(Secondary->LfsDeviceExt->Flags, LFS_DEVICE_STOP) ||
			FlagOn(Secondary->LfsDeviceExt->Flags, LFS_DEVICE_SURPRISE_REMOVAL)) {

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("RecoverySession: device:%p flags:%x stopped.\n",
													   Secondary->LfsDeviceExt, Secondary->LfsDeviceExt->Flags) );

			return FALSE;
		}

		if (Secondary->ThreadHandle) {

			ASSERT( Secondary->ThreadObject );
		
			timeOut.QuadPart = -LFS_TIME_OUT;		// 10 sec

			waitStatus = KeWaitForSingleObject( Secondary->ThreadObject,
												Executive,
												KernelMode,
												FALSE,
												&timeOut );

			if (waitStatus == STATUS_SUCCESS) {

				_U16		midTemp;

				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("RecoverySession: thread stoped\n") );
				ObDereferenceObject( Secondary->ThreadObject );
		
				Secondary->ThreadHandle = NULL;
				Secondary->ThreadObject = NULL;

				Secondary->Thread.Flags = 0;
				Secondary->Thread.SessionStatus = 0;

				KeInitializeEvent( &Secondary->ReadyEvent, NotificationEvent, FALSE );
				KeInitializeEvent( &Secondary->RequestEvent, NotificationEvent, FALSE );
				
				Secondary->Thread.ReceiveWaitingCount = 0;
				for (midTemp=0; midTemp < MAX_SLOT_PER_SESSION; midTemp++) {

					if (Secondary->Thread.SessionSlot[midTemp] == NULL)
						break;
				}
				
				RtlZeroMemory( &Secondary->Thread.SessionContext,
							   sizeof(Secondary->Thread.SessionContext) );

				Secondary->Thread.TdiReceiveContext.Irp = NULL;
				KeInitializeEvent( &Secondary->Thread.TdiReceiveContext.CompletionEvent, NotificationEvent, FALSE );

				Secondary->Thread.SessionContext.PrimaryMaxDataSize = 
					LfsRegistry.MaxDataTransferPri < DEFAULT_MAX_DATA_SIZE ? LfsRegistry.MaxDataTransferPri : DEFAULT_MAX_DATA_SIZE;
				
				Secondary->Thread.SessionContext.SecondaryMaxDataSize = 
					LfsRegistry.MaxDataTransferSec<DEFAULT_MAX_DATA_SIZE ? LfsRegistry.MaxDataTransferSec : DEFAULT_MAX_DATA_SIZE;

				//
				//	Initialize transport context for traffic control
				//

				InitTransCtx( &Secondary->Thread.TransportCtx, Secondary->Thread.SessionContext.PrimaryMaxDataSize );

				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE, ("RecoverySession: PriMaxData:%08u SecMaxData:%08u\n",
														  Secondary->Thread.SessionContext.PrimaryMaxDataSize,
														  Secondary->Thread.SessionContext.SecondaryMaxDataSize) );
			
			} else {

				ASSERT( LFS_BUG );
				return FALSE;
			}
		}


//		if(reconnectionTry >= 2) // first and second lookup primary address
		{
			LfsDeviceExt_SecondaryToPrimary( Secondary->LfsDeviceExt );
		}

		if (FlagOn(Secondary->Flags, SECONDARY_FLAG_CLOSED)) {

			ASSERT( FromSynchronousCall == FALSE );
			return TRUE;
		}

		LfsTable_CleanCachePrimaryAddress( GlobalLfs.LfsTable, &netdiskPartitionInfo );

		tableStatus = LfsTable_QueryPrimaryAddress( GlobalLfs.LfsTable,
													&netdiskPartitionInfo,
													&Secondary->PrimaryAddress );

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("RecoverySession: LfsTable_QueryPrimaryAddress tableStatus = %X\n", tableStatus) );

		if (tableStatus == STATUS_SUCCESS) {

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("RecoverySession: Found PrimaryAddress :%02x:%02x:%02x:%02x:%02x:%02x/%d\n",
													   Secondary->PrimaryAddress.Node[0],
													   Secondary->PrimaryAddress.Node[1],
													   Secondary->PrimaryAddress.Node[2],
													   Secondary->PrimaryAddress.Node[3],
													   Secondary->PrimaryAddress.Node[4],
													   Secondary->PrimaryAddress.Node[5],
													   NTOHS(Secondary->PrimaryAddress.Port)) );

			if (Lfs_IsLocalAddress(&Secondary->PrimaryAddress) && Secondary->LfsDeviceExt->SecondaryState == SECONDARY_STATE) { // not yet purged
	
				// another volume changed to primary and this volume didn't purge yet 
				result = FALSE;

				continue;
			}
		
		} else {

			result = FALSE;
			continue;
		}	
		
		KeInitializeEvent( &Secondary->ReadyEvent, NotificationEvent, FALSE );
		KeInitializeEvent( &Secondary->RequestEvent, NotificationEvent, FALSE );

		InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

		waitStatus = PsCreateSystemThread( &Secondary->ThreadHandle,
										   THREAD_ALL_ACCESS,
										   &objectAttributes,
										   NULL,
										   NULL,
										   SecondaryThreadProc,
										   Secondary );

		if (!NT_SUCCESS(waitStatus)) {

			ASSERT( LFS_UNEXPECTED );
			result = FALSE;
			break;
		}

		waitStatus = ObReferenceObjectByHandle( Secondary->ThreadHandle,
												FILE_READ_DATA,
												NULL,
												KernelMode,
												&Secondary->ThreadObject,
												NULL );

		if (!NT_SUCCESS(waitStatus)) {

			ASSERT( LFS_INSUFFICIENT_RESOURCES );
			result = FALSE;
			break;
		}

		timeOut.QuadPart = - LFS_TIME_OUT;	

		waitStatus = KeWaitForSingleObject( &Secondary->ReadyEvent,
											Executive,
											KernelMode,
											FALSE,
											&timeOut );

		if (waitStatus != STATUS_SUCCESS) {

			ASSERT( LFS_BUG );
			result = FALSE;
			break;
		}

		KeClearEvent( &Secondary->ReadyEvent );

		InterlockedIncrement( &Secondary->SessionId );	

		ExAcquireFastMutex( &Secondary->FastMutex );
		
		if (BooleanFlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_UNCONNECTED)) {

			ExReleaseFastMutex( &Secondary->FastMutex );

			if (Secondary->Thread.SessionStatus == STATUS_DISK_CORRUPT_ERROR ||
				Secondary->Thread.SessionStatus == STATUS_UNRECOGNIZED_VOLUME) {

				result = FALSE;
				break;
			}

			continue;
		
		} else {

			ExReleaseFastMutex( &Secondary->FastMutex );
		}

		ASSERT( FlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_CONNECTED) && !FlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_ERROR) );

		result = TRUE;
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("RecoverySession Success Secondary = %p\n", Secondary) );

		break;
	}

	if (result == FALSE) {

		//
		//	if failed to recover the connection to a primary.
		//
		
		LfsDbgBreakPoint();

		while (!IsListEmpty(&tempRequestQueue)) {

			secondaryRequestEntry = RemoveHeadList( &tempRequestQueue );
		
			ASSERT( LFS_BUG );
			ExInterlockedInsertTailList( &Secondary->RequestQueue, 
										 secondaryRequestEntry,
										 &Secondary->RequestQSpinLock );
		}

		return result;
	}
	
	//
	//	Recovering the connection to a primary is successful,
	//	try to reopen files to continue I/O.
	//

    for (fileExtlistEntry = Secondary->FileExtQueue.Blink;
         fileExtlistEntry != &Secondary->FileExtQueue;
         fileExtlistEntry = fileExtlistEntry->Blink) {

		PFILE_EXTENTION				fileExt;
		ULONG						disposition;
		
		ULONG						dataSize;
		PSECONDARY_REQUEST			secondaryRequest;
		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		//KIRQL						oldIrqlTemp;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
		_U8							*ndfsWinxpRequestData;

		LARGE_INTEGER				timeOut;
		NTSTATUS					waitStatus;
		PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;


		fileExt = CONTAINING_RECORD (fileExtlistEntry, FILE_EXTENTION, ListEntry);
		fileExt->Fcb->FileRecordSegmentHeaderAvail = FALSE;

		if (fileExt->CreateContext.RelatedFileHandle != 0) {

			PFILE_EXTENTION	relatedFileExt;

			if (fileExt->FileObject->RelatedFileObject == NULL) {

				ASSERT( LFS_UNEXPECTED );
				result = FALSE;	
				break;
			}

			if (fileExt->RelatedFileObjectClosed == FALSE) {

				relatedFileExt = Secondary_LookUpFileExtension(Secondary, fileExt->FileObject->RelatedFileObject);
				ASSERT( relatedFileExt != NULL && relatedFileExt->LfsMark == LFS_MARK );				
				ASSERT( relatedFileExt->SessionId == Secondary->SessionId ); // must be already Updated

				if (relatedFileExt->Corrupted == TRUE) {

					fileExt->SessionId = Secondary->SessionId;
					fileExt->Corrupted = TRUE;
				
					continue;
				}

				fileExt->CreateContext.RelatedFileHandle = relatedFileExt->PrimaryFileHandle;
			
			} else { // relateFileObject is already closed;
			
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("RecoverySession: relateFileObject is already closed\n") );
				
				fileExt->CreateContext.RelatedFileHandle = 0;
				fileExt->CreateContext.FileNameLength = fileExt->Fcb->FullFileName.Length;

				if (fileExt->Fcb->FullFileName.Length) {

					RtlCopyMemory( fileExt->Buffer + fileExt->CreateContext.EaLength,
								   fileExt->Fcb->FullFileName.Buffer,
								   fileExt->Fcb->FullFileName.Length );
				}
			}	
		}
				
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, 
					   ("RecoverySession: fileExt->Fcb->FullFileName = %wZ fileExt->FileObject->CurrentByteOffset.QuadPart = %I64d\n", 
						 &fileExt->Fcb->FullFileName, fileExt->FileObject->CurrentByteOffset.QuadPart) );

		if (Secondary->LfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NTFS) {

			dataSize = ((fileExt->CreateContext.EaLength + fileExt->CreateContext.FileNameLength) > Secondary->Thread.SessionContext.BytesPerFileRecordSegment)
						? (fileExt->CreateContext.EaLength + fileExt->CreateContext.FileNameLength) 
						  : Secondary->Thread.SessionContext.BytesPerFileRecordSegment;
		
		} else
			dataSize = fileExt->CreateContext.EaLength + fileExt->CreateContext.FileNameLength;

		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST( Secondary, IRP_MJ_CREATE, dataSize );

		if (secondaryRequest == NULL) {

			result = FALSE;	
			break;
		}

		secondaryRequest->OutputBuffer = NULL;
		secondaryRequest->OutputBufferLength = 0;

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;

		ExAcquireFastMutex( &Secondary->FastMutex );
	
		RtlCopyMemory( ndfsRequestHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsRequestHeader->Protocol) );

		ndfsRequestHeader->Command	= NDFS_COMMAND_EXECUTE;
		ndfsRequestHeader->Flags	= Secondary->Thread.SessionContext.Flags;
		ndfsRequestHeader->Uid		= Secondary->Thread.SessionContext.Uid;
		ndfsRequestHeader->Tid		= Secondary->Thread.SessionContext.Tid;
		ndfsRequestHeader->Mid		= 0;
		ndfsRequestHeader->MessageSize = 
			(Secondary->Thread.SessionContext.MessageSecurity == 1) 
					? sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER) + ADD_ALIGN8(fileExt->CreateContext.EaLength + fileExt->CreateContext.FileNameLength)
					: sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER) + fileExt->CreateContext.EaLength + fileExt->CreateContext.FileNameLength;

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

		ExReleaseFastMutex( &Secondary->FastMutex );

		//
		//	[64bit issue]
		//	I assume that IrpTag may have any dummy value.
		//

		ndfsWinxpRequestHeader->IrpTag   = (_U32)PtrToUlong(fileExt);
		ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_CREATE;
		ndfsWinxpRequestHeader->IrpMinorFunction = 0;

		ndfsWinxpRequestHeader->FileHandle = 0;

		ndfsWinxpRequestHeader->IrpFlags   = fileExt->IrpFlags;
		ndfsWinxpRequestHeader->IrpSpFlags = fileExt->IrpSpFlags;

		RtlCopyMemory( &ndfsWinxpRequestHeader->Create,
					   &fileExt->CreateContext,
					   sizeof(WINXP_REQUEST_CREATE) );
		
		disposition = FILE_OPEN_IF;
		ndfsWinxpRequestHeader->Create.Options &= 0x00FFFFFF;
		ndfsWinxpRequestHeader->Create.Options |= (disposition << 24);

		ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);

		RtlCopyMemory( ndfsWinxpRequestData,
					   fileExt->Buffer,
					   fileExt->CreateContext.EaLength + fileExt->CreateContext.FileNameLength );

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest(Secondary, secondaryRequest);

		timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
		waitStatus = KeWaitForSingleObject( &secondaryRequest->CompleteEvent,
											Executive,
											KernelMode,
											FALSE,
											&timeOut );

		KeClearEvent( &secondaryRequest->CompleteEvent );

		if (waitStatus != STATUS_SUCCESS) {

			ASSERT( LFS_BUG );

			secondaryRequest = NULL;
			result = FALSE;
			break;
		}

		if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

			DereferenceSecondaryRequest( secondaryRequest );
			
			result = TRUE;		
			break;
		}
				
		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
		
		if (ndfsWinxpReplytHeader->Status == STATUS_SUCCESS) {

	 		fileExt->PrimaryFileHandle = ndfsWinxpReplytHeader->Open.FileHandle;
		
		} else {

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR,
						   ("RecoverySession: fileExt->Fcb->FullFileName = %wZ Corrupted\n", &fileExt->Fcb->FullFileName) );

			fileExt->Corrupted = TRUE;
		}

		fileExt->SessionId = Secondary->SessionId;

		DereferenceSecondaryRequest( secondaryRequest );
	}

	//
	//	Recovering the connection to a primary is successful,
	//	Re-queue request to the thread list
	//

	while (!IsListEmpty(&tempRequestQueue)) {

		PSECONDARY_REQUEST			secondaryRequest;

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("RecoverySession: extract from tempRequestQueue\n") );

		secondaryRequestEntry = RemoveHeadList(&tempRequestQueue);
		secondaryRequest = CONTAINING_RECORD(secondaryRequestEntry, SECONDARY_REQUEST, ListEntry);

		//
		//	Update TID
		//

		secondaryRequest->NdfsRequestHeader.Tid	= Secondary->Thread.SessionContext.Tid;

		//
		//	Queue the request
		//

		ExInterlockedInsertTailList( &Secondary->RequestQueue, 
									 secondaryRequestEntry,
									 &Secondary->RequestQSpinLock );
	}

	//ASSERT( Secondary->SemaphoreConsumeRequiredCount == 0 );

	//
	//	Recovering the connection to a primary is successful,
	//	Adjust request semaphore
	//

	if (result == TRUE) {

#if 0
		if (previousSessionSlotCount < Secondary->Thread.SessionContext.SessionSlotCount) {

			while (previousSessionSlotCount < Secondary->Thread.SessionContext.SessionSlotCount) {

				Secondary->SemaphoreReturnCount++;
				previousSessionSlotCount++;
			}
		
		} else if (previousSessionSlotCount > Secondary->Thread.SessionContext.SessionSlotCount) {

			_U16	consumeRequireCount = previousSessionSlotCount - Secondary->Thread.SessionContext.SessionSlotCount;

			if (consumeRequireCount <= Secondary->SemaphoreReturnCount) {

				Secondary->SemaphoreReturnCount -= consumeRequireCount;
			
			} else {

				Secondary->SemaphoreConsumeRequiredCount = consumeRequireCount - Secondary->SemaphoreReturnCount;
				Secondary->SemaphoreReturnCount = 0;
			}
		}
#if DBG
		if (FromSynchronousCall)
			ASSERT( queuedRequestCount + 1 + Secondary->SemaphoreReturnCount - Secondary->SemaphoreConsumeRequiredCount 
					== Secondary->Thread.SessionContext.SessionSlotCount );
		else
			ASSERT( queuedRequestCount + /* 1 + */ Secondary->SemaphoreReturnCount - Secondary->SemaphoreConsumeRequiredCount 
					== Secondary->Thread.SessionContext.SessionSlotCount );
#endif
#endif

		KeSetEvent( &Secondary->RequestEvent, IO_DISK_INCREMENT, FALSE );
	}

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO, ("RecoverySession Completed. Secondary = %p, result = %d\n", Secondary, result) );

	return result;
}


static
VOID
CallRecoverySession(
	PSECONDARY	Secondary
	)
{
	BOOLEAN				recoveryResult;
	PLIST_ENTRY			secondaryRequestEntry;
	PSECONDARY_REQUEST	secondaryRequest;

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("CallRecoverySession entered. Secondary=%p\n", Secondary) );

	do {

		if (!FlagOn(Secondary->Flags, SECONDARY_FLAG_CLOSED)) {

			recoveryResult = RecoverySession(Secondary, FALSE);

			ExAcquireFastMutex( &Secondary->FastMutex );

			if (recoveryResult == TRUE) {

				ClearFlag( Secondary->Flags, SECONDARY_FLAG_RECONNECTING );

				ExReleaseFastMutex( &Secondary->FastMutex );
				break;
			}

			//
			//	Recovery failed.
			//	Complete all other pending IRPs with error.
			//

			SetFlag( Secondary->Flags, SECONDARY_FLAG_ERROR );

			ExReleaseFastMutex( &Secondary->FastMutex );
		}

		while (secondaryRequestEntry = ExInterlockedRemoveHeadList( &Secondary->RequestQueue,
																	&Secondary->RequestQSpinLock)) {

			InitializeListHead( secondaryRequestEntry );

			secondaryRequest = CONTAINING_RECORD( secondaryRequestEntry, SECONDARY_REQUEST, ListEntry );

			ASSERT( secondaryRequest->RequestType == SECONDARY_REQ_DISCONNECT );
			secondaryRequest->ExecuteStatus = STATUS_ABANDONED;

			ASSERT( secondaryRequest->Synchronous == TRUE );
			KeSetEvent( &secondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );				
		}


	} while (0);

	//
	//	Release semaphore acquired in Secondary_PassThrough()
	//			to block other IRPs
	//

//	while (Secondary->SemaphoreReturnCount) {

		KeReleaseSemaphore( &Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE );
//		Secondary->SemaphoreReturnCount--;
//	}

	//
	//	Dereference the secondary which was referenced in RecoverySessionAsynch()
	//

	Secondary_Dereference( Secondary );

	PsTerminateSystemThread( STATUS_SUCCESS );
}


NTSTATUS
RecoverySessionAsynch(
	IN  PSECONDARY	Secondary
	) 
{
	OBJECT_ATTRIBUTES	objectAttributes;
	NTSTATUS			status;
	HANDLE				threadHandle;

	Secondary_Reference( Secondary );

	ASSERT( FlagOn(Secondary->Flags, SECONDARY_FLAG_RECONNECTING) );
	InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

	status = PsCreateSystemThread( &threadHandle,
								   THREAD_ALL_ACCESS,
								   &objectAttributes,
								   NULL,
								   NULL,
								   CallRecoverySession,
								   Secondary );

	if (!NT_SUCCESS(status)) {

		ASSERT(LFS_UNEXPECTED);
	}

	return status;
}


PLFS_FCB
AllocateFcb(
	IN	PSECONDARY		Secondary,
	IN PUNICODE_STRING	FullFileName,
	IN  ULONG			BufferLength
    )
{
    PLFS_FCB fcb;


	UNREFERENCED_PARAMETER( Secondary );
	
    fcb = FsRtlAllocatePoolWithTag( NonPagedPool,
									sizeof(LFS_FCB) - sizeof(CHAR) + BufferLength,
									LFS_FCB_TAG );
	
	if (fcb == NULL) {
	
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR, ("AllocateFcb: failed to allocate fcb\n") );
		return NULL;
	}
	
	RtlZeroMemory( fcb, sizeof(LFS_FCB) - sizeof(CHAR) + BufferLength );

    fcb->NonPaged = LfsAllocateNonPagedFcb();
	
	if (fcb->NonPaged == NULL) {
	
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR, ("AllocateFcb: failed to allocate fcb->NonPaged\n") );
		ExFreePool( fcb );
		
		return NULL;
	}

    RtlZeroMemory( fcb->NonPaged, sizeof(NON_PAGED_FCB) );

#define FAT_NTC_FCB                      (0x0502)
	
	fcb->Header.NodeTypeCode = FAT_NTC_FCB;
	fcb->Header.IsFastIoPossible = FastIoIsPossible;
    fcb->Header.Resource = LfsAllocateResource();
	fcb->Header.PagingIoResource = NULL; //fcb->Header.Resource;
	
	if (fcb->Header.Resource == NULL) {
	
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR, ("AllocateFcb: failed to allocate fcb->Header.Resource\n") );
		ExFreePool( fcb->NonPaged );
		ExFreePool( fcb );
		return NULL;
	}

	ExInitializeFastMutex( &fcb->NonPaged->AdvancedFcbHeaderMutex );

#if WINVER >= 0x0501

	if (IS_WINDOWSXP_OR_LATER()) {

		FsRtlSetupAdvancedHeader( &fcb->Header, &fcb->NonPaged->AdvancedFcbHeaderMutex );
	}

#endif

	FsRtlInitializeFileLock( &fcb->FileLock, NULL, NULL );

	fcb->ReferenceCount = 1;
	InitializeListHead( &fcb->ListEntry );

    RtlInitEmptyUnicodeString( &fcb->FullFileName,
							   fcb->FullFileNameBuffer,
							   sizeof(fcb->FullFileNameBuffer) );

	RtlCopyUnicodeString( &fcb->FullFileName, FullFileName );

    RtlInitEmptyUnicodeString( &fcb->CaseInSensitiveFullFileName,
							   fcb->CaseInSensitiveFullFileNameBuffer,
							   sizeof(fcb->CaseInSensitiveFullFileNameBuffer) );

	RtlDowncaseUnicodeString( &fcb->CaseInSensitiveFullFileName,
							  &fcb->FullFileName,
							  FALSE );

	if(FullFileName->Length)
		if(FullFileName->Buffer[0] != L'\\')
			ASSERT( LFS_BUG );
	
#if DBG
	InterlockedIncrement( &LfsObjectCounts.FcbCount );
#endif

	return fcb;
}


VOID
Secondary_DereferenceFcb(
	IN	PLFS_FCB	Fcb
	)
{
	LONG		result;


	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
				   ("Secondary_DereferenceFcb: Fcb->OpenCount = %d, Fcb->UncleanCount = %d\n", Fcb->OpenCount, Fcb->UncleanCount) );

	ASSERT( Fcb->OpenCount >= Fcb->UncleanCount );
	result = InterlockedDecrement( &Fcb->ReferenceCount );

	ASSERT( result >= 0 );

	if (0 == result) {

		ASSERT( Fcb->ListEntry.Flink == Fcb->ListEntry.Blink );
		ASSERT( Fcb->OpenCount == 0 );
		
	    LfsFreeResource( Fcb->Header.Resource );
		LfsFreeNonPagedFcb( Fcb->NonPaged );
	    ExFreePool( Fcb );	

#if DBG
		InterlockedDecrement( &LfsObjectCounts.FcbCount );
#endif
	}
}


PLFS_FCB
Secondary_LookUpFcb(
	IN PSECONDARY		Secondary,
	IN PUNICODE_STRING	FullFileName,
    IN BOOLEAN			CaseInSensitive
	)
{
	PLFS_FCB		fcb = NULL;
    PLIST_ENTRY		listEntry;
	KIRQL			oldIrql;
	UNICODE_STRING	caseInSensitiveFullFileName;
	PWCHAR			caseInSensitiveFullFileNameBuffer;
	NTSTATUS		downcaseStatus;

	//
	//	Allocate a name buffer
	//

	caseInSensitiveFullFileNameBuffer = ExAllocatePool(NonPagedPool, NDFS_MAX_PATH);
	
	if (caseInSensitiveFullFileNameBuffer == NULL) {
	
		ASSERT( LFS_REQUIRED );
		return NULL;
	}

	ASSERT( FullFileName->Length <= NDFS_MAX_PATH*sizeof(WCHAR) );

	if (CaseInSensitive == TRUE) {

		RtlInitEmptyUnicodeString( &caseInSensitiveFullFileName,
								   caseInSensitiveFullFileNameBuffer,
								   NDFS_MAX_PATH );

		downcaseStatus = RtlDowncaseUnicodeString( &caseInSensitiveFullFileName,
												   FullFileName,
												   FALSE );
	
		if (downcaseStatus != STATUS_SUCCESS) {

			ExFreePool( caseInSensitiveFullFileNameBuffer );
			ASSERT( LFS_UNEXPECTED );
			return NULL;
		}
	}

	KeAcquireSpinLock( &Secondary->FcbQSpinLock, &oldIrql );

    for (listEntry = Secondary->FcbQueue.Flink;
         listEntry != &Secondary->FcbQueue;
         listEntry = listEntry->Flink) {

		fcb = CONTAINING_RECORD( listEntry, LFS_FCB, ListEntry );
		
		if (fcb->FullFileName.Length != FullFileName->Length) {

			fcb = NULL;
			continue;
		}

		if (CaseInSensitive == TRUE) {

			if (RtlEqualMemory(fcb->CaseInSensitiveFullFileName.Buffer, 
							   caseInSensitiveFullFileName.Buffer, 
							   fcb->CaseInSensitiveFullFileName.Length)) {

				InterlockedIncrement( &fcb->ReferenceCount );
				break;
			}
		
		} else {

			if (RtlEqualMemory(fcb->FullFileName.Buffer,
							   FullFileName->Buffer,
							   fcb->FullFileName.Length)) {

				InterlockedIncrement( &fcb->ReferenceCount );
				break;
			}
		}

		fcb = NULL;
	}

	KeReleaseSpinLock( &Secondary->FcbQSpinLock, oldIrql );

	ExFreePool( caseInSensitiveFullFileNameBuffer );

	return fcb;
}


PFILE_EXTENTION
AllocateFileExt(
	IN	PSECONDARY		Secondary,
	IN	PFILE_OBJECT	FileObject,
	IN  ULONG			BufferLength
	) 
{
	PFILE_EXTENTION	fileExt;


	fileExt = ExAllocatePoolWithTag( NonPagedPool, 
									 sizeof(FILE_EXTENTION) - sizeof(_U8) + BufferLength + MEMORY_CHECK_SIZE,
									 FILE_EXT_TAG );
	
	if (fileExt == NULL) {

		ASSERT( LFS_INSUFFICIENT_RESOURCES );
		return NULL;
	}

	RtlZeroMemory( fileExt, sizeof(FILE_EXTENTION) - sizeof(_U8) + BufferLength );
	
#if DBG && MEMORY_CHECK_SIZE
	{
		UCHAR	i;
		ULONG	memorySize = sizeof(FILE_EXTENTION) - sizeof(_U8) + BufferLength;

		for(i=0; i<MEMORY_CHECK_SIZE; i++)
			*((_U8*)fileExt + memorySize + i) = i;
	}
#endif

	fileExt->LfsMark    = LFS_MARK;
	fileExt->Secondary	= Secondary;
	fileExt->FileObject	= FileObject;

	fileExt->LastQueryFileIndex = (ULONG)-1;

	InitializeListHead( &fileExt->ListEntry );

	fileExt->BufferLength = BufferLength;

	ExAcquireFastMutex( &Secondary->FastMutex );
	fileExt->SessionId = Secondary->SessionId;
	fileExt->LastDirectoryQuerySessionId = Secondary->SessionId;
	ExReleaseFastMutex( &Secondary->FastMutex );

#if DBG
	InterlockedIncrement( &LfsObjectCounts.FileExtCount );
#endif

	InterlockedIncrement( &Secondary->FileExtCount );

	return fileExt;
}


VOID
FreeFileExt(
	IN  PSECONDARY		Secondary,
	IN  PFILE_EXTENTION	FileExt
	)
{
	PLIST_ENTRY		listEntry;


	ASSERT( FileExt->ListEntry.Flink == FileExt->ListEntry.Blink );

#if DBG && MEMORY_CHECK_SIZE

	{
		UCHAR	i;
		ULONG	memorySize = sizeof(FILE_EXTENTION) - sizeof(_U8) + FileExt->BufferLength;

		
		for(i=0; i<MEMORY_CHECK_SIZE; i++)
			if(*((_U8*)FileExt + memorySize + i) != i)
			{
				ASSERT(LFS_BUG);
				break;
			}
	}
#endif

	ExAcquireFastMutex( &Secondary->FileExtQMutex );

    for (listEntry = Secondary->FileExtQueue.Flink;
         listEntry != &Secondary->FileExtQueue;
         listEntry = listEntry->Flink) {

		PFILE_EXTENTION	childFileExt;
		
		childFileExt = CONTAINING_RECORD( listEntry, FILE_EXTENTION, ListEntry );
        
		if (childFileExt->CreateContext.RelatedFileHandle == FileExt->PrimaryFileHandle)
			childFileExt->RelatedFileObjectClosed = TRUE;
	}

    ExReleaseFastMutex( &Secondary->FileExtQMutex );

	InterlockedDecrement( &Secondary->FileExtCount );

	ExFreePoolWithTag( FileExt, FILE_EXT_TAG );

#if DBG
	InterlockedDecrement( &LfsObjectCounts.FileExtCount );
#endif
}


PFILE_EXTENTION
Secondary_LookUpFileExtensionByHandle(
	IN PSECONDARY	Secondary,
	IN HANDLE		FileHandle
	)
{
	NTSTATUS		referenceStatus;
	PFILE_OBJECT	fileObject = NULL;


	referenceStatus = ObReferenceObjectByHandle( FileHandle,
												 FILE_READ_DATA,
												 NULL,
												 KernelMode,
												 &fileObject,
												 NULL );

    if (referenceStatus != STATUS_SUCCESS) {

		return NULL;
	}
	
	ObDereferenceObject( fileObject );

	return Secondary_LookUpFileExtension( Secondary, fileObject );
}


	
PFILE_EXTENTION
Secondary_LookUpFileExtension(
	IN PSECONDARY	Secondary,
	IN PFILE_OBJECT	FileObject
	)
{
	PFILE_EXTENTION	fileExt = NULL;
    PLIST_ENTRY		listEntry;

	
    ExAcquireFastMutex( &Secondary->FileExtQMutex );

    for (listEntry = Secondary->FileExtQueue.Flink;
         listEntry != &Secondary->FileExtQueue;
         listEntry = listEntry->Flink) {

		 fileExt = CONTAINING_RECORD( listEntry, FILE_EXTENTION, ListEntry );
         
		 if (fileExt->FileObject == FileObject)
			break;

		fileExt = NULL;
	}

    ExReleaseFastMutex( &Secondary->FileExtQMutex );

	return fileExt;
}