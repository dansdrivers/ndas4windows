#include "LfsProc.h"



PSECONDARY
Secondary_Create (
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

	RtlCopyMemory( netdiskPartitionInfo.NdscId,
				   LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.NdscId,
				   NDSC_ID_LENGTH );

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

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO, ("Secondary_Create: QueryPrimaryAddress %08x\n", status) );

		if (NT_SUCCESS(status)) {

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO, ("Secondary_Create: QueryPrimaryAddress: Found PrimaryAddress :%02x:%02x:%02x:%02x:%02x:%02x/%d\n",
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
	InitializeListHead( &secondary->CcbQueue );
    ExInitializeFastMutex( &secondary->CcbQMutex );

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
	
		NDASFS_ASSERT( FALSE );
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
	ASSERT( IsListEmpty(&Secondary->CcbQueue) );
	ASSERT( IsListEmpty(&Secondary->RequestQueue) );
	ASSERT( Secondary->CcbCount == 0 );

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
AllocSecondaryRequest (
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

FORCEINLINE
PSECONDARY_REQUEST
AllocateWinxpSecondaryRequest (
	IN PSECONDARY	Secondary,
	IN UINT32		DataSize
	)
{
	return AllocSecondaryRequest( Secondary,
								  (((sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER)) > (sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER))) ?
								  (sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER)) : (sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER)))
								  + DataSize,
								  TRUE );
}


VOID
ReferenceSecondaryRequest (
	IN	PSECONDARY_REQUEST	SecondaryRequest
	) 
{
	LONG	result;

	result = InterlockedIncrement( &SecondaryRequest->ReferenceCount );

	ASSERT( result > 0 );
}


VOID
DereferenceSecondaryRequest (
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
QueueingSecondaryRequest (
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

		ExReleaseFastMutex( &Secondary->FastMutex );

		KeSetEvent( &Secondary->RequestEvent, IO_DISK_INCREMENT, FALSE );
		status = STATUS_SUCCESS;

	} else {

		ExReleaseFastMutex( &Secondary->FastMutex );

		status = STATUS_UNSUCCESSFUL;
	}

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
Secondary_TryCloseCcbs (
	PSECONDARY Secondary
	)
{
	PLIST_ENTRY		listEntry;

	Secondary_Reference( Secondary );	
	
	if (ExTryToAcquireFastMutex(&Secondary->CcbQMutex) == FALSE) {

		Secondary_Dereference( Secondary );
		return;
	}
	
	if (FlagOn(Secondary->Flags, SECONDARY_FLAG_RECONNECTING)) {

		ExReleaseFastMutex( &Secondary->CcbQMutex );
		Secondary_Dereference( Secondary );
		return;
	}

	try {

		listEntry = Secondary->CcbQueue.Flink;

		while (listEntry != &Secondary->CcbQueue) {

			PLFS_CCB				ccb = NULL;
			PLFS_FCB					fcb;
			PSECTION_OBJECT_POINTERS	section;
			BOOLEAN						dataSectionExists;
			BOOLEAN						imageSectionExists;

			ccb = CONTAINING_RECORD( listEntry, LFS_CCB, ListEntry );
			listEntry = listEntry->Flink;

			if (ccb->TypeOfOpen != UserFileOpen)
				break;

			fcb = ccb->Fcb;

			if (fcb == NULL) {

				ASSERT( LFS_BUG );
				break;
			}	

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("Secondary_TryCloseCcbs: fcb->FullFileName = %wZ\n", &ccb->Fcb->FullFileName) );

			if (fcb->UncleanCount != 0) {

				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("Secondary_TryCloseCcbs: fcb->FullFileName = %wZ\n", &ccb->Fcb->FullFileName) );
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

		ExReleaseFastMutex( &Secondary->CcbQMutex );
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
Secondary_Ioctl (
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

BOOLEAN 
SecondaryPassThrough (
	IN  PSECONDARY	Secondary,
	IN  PIRP		Irp,
	OUT PNTSTATUS	Status
	)
{
	BOOLEAN				result;
	PIO_STACK_LOCATION  irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT		fileObject = irpSp->FileObject;

	BOOLEAN				pendingReturned;

	BOOLEAN				fastMutexSet = FALSE;
	BOOLEAN				retry = FALSE;

	PrintIrp( LFS_DEBUG_SECONDARY_TRACE, "Secondary_PassThrough", Secondary->LfsDeviceExt, Irp );

	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );

	Secondary_Reference( Secondary );

	pendingReturned = Irp->PendingReturned;
 
	if (fileObject && fileObject->Flags & FO_DIRECT_DEVICE_OPEN) {

		NDASFS_ASSERT( LFS_REQUIRED );
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
			
	} else if (irpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) {

		if (irpSp->MinorFunction == IRP_MN_MOUNT_VOLUME		||
			irpSp->MinorFunction == IRP_MN_VERIFY_VOLUME	||
			irpSp->MinorFunction == IRP_MN_LOAD_FILE_SYSTEM) {
		
			NDASFS_ASSERT( LFS_UNEXPECTED );
			ASSERT( Secondary_LookUpCcb(Secondary, fileObject) == NULL );

			PrintIrp( LFS_DEBUG_SECONDARY_TRACE, "Secondary_PassThrough", Secondary->LfsDeviceExt, Irp );

			Secondary_Dereference( Secondary );
			return FALSE;
		}
	
	} else if (irpSp->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL	|| 	// 0x0f
			   irpSp->MajorFunction == IRP_MJ_SHUTDOWN					||  // 0x10
			   irpSp->MajorFunction == IRP_MJ_CREATE_MAILSLOT			||  // 0x13
			   irpSp->MajorFunction == IRP_MJ_POWER						||  // 0x16
			   irpSp->MajorFunction == IRP_MJ_SYSTEM_CONTROL			||  // 0x17
			   irpSp->MajorFunction == IRP_MJ_DEVICE_CHANGE) {				// 0x18
	
		NDASFS_ASSERT( LFS_REQUIRED );
		PrintIrp( LFS_DEBUG_SECONDARY_TRACE, "Secondary_PassThrough", Secondary->LfsDeviceExt, Irp );

		Secondary_Dereference( Secondary );

		return FALSE;

	} else if (irpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL) {

		PLFS_CCB	ccb = NULL;

		ccb = Secondary_LookUpCcb(Secondary, fileObject);
		
		if (ccb == NULL || ccb->TypeOfOpen != UserVolumeOpen) {

			Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;

			if (Status)
				*Status = STATUS_INVALID_PARAMETER;

			IoCompleteRequest( Irp, IO_DISK_INCREMENT );

			Secondary_Dereference( Secondary );

			return TRUE;
		}

		result = Secondary_Ioctl( Secondary, Irp, irpSp, Status );

		Secondary_Dereference( Secondary );

		return result;	

	} else {

		NDASFS_ASSERT( LFS_UNEXPECTED );

		Secondary_Dereference( Secondary );
		return FALSE;
	}		

	PrintIrp( LFS_DEBUG_SECONDARY_TRACE, "Secondary_PassThrough", Secondary->LfsDeviceExt, Irp );
	
	if (Secondary->Thread.SessionStatus == STATUS_DISK_CORRUPT_ERROR) {

		NDASFS_ASSERT( Secondary_LookUpCcb(Secondary, fileObject) == NULL );

		if (irpSp->MajorFunction == IRP_MJ_CLOSE) {

			if (Secondary_LookUpCcb(Secondary, fileObject)) {

				SecondaryFileObjectClose( Secondary, fileObject );
			}

			*Status = Irp->IoStatus.Status = STATUS_SUCCESS;

		} else if (irpSp->MajorFunction == IRP_MJ_CLEANUP) {

			InterlockedDecrement( &((PLFS_FCB)irpSp->FileObject->FsContext)->UncleanCount );
			SetFlag( fileObject->Flags, FO_CLEANUP_COMPLETE );

			*Status = Irp->IoStatus.Status = STATUS_SUCCESS;

		} else {

			*Status = Irp->IoStatus.Status = STATUS_DISK_CORRUPT_ERROR;
		}

		Irp->IoStatus.Information = 0;
		IoCompleteRequest( Irp, IO_DISK_INCREMENT );

		Secondary_Dereference( Secondary );
		
		return TRUE;
	
	} 
	
	if (Secondary->Thread.SessionStatus == STATUS_UNRECOGNIZED_VOLUME) {

		NDASFS_ASSERT( Secondary_LookUpCcb(Secondary, fileObject) == NULL );

		if (irpSp->MajorFunction == IRP_MJ_CLOSE) {

			if (Secondary_LookUpCcb(Secondary, fileObject)) {

				SecondaryFileObjectClose( Secondary, fileObject );
			}

			*Status = Irp->IoStatus.Status = STATUS_SUCCESS;

		} else if (irpSp->MajorFunction == IRP_MJ_CLEANUP) {

			InterlockedDecrement( &((PLFS_FCB)irpSp->FileObject->FsContext)->UncleanCount );
			SetFlag( fileObject->Flags, FO_CLEANUP_COMPLETE );

			*Status = Irp->IoStatus.Status = STATUS_SUCCESS;

		} else {

			*Status = Irp->IoStatus.Status = STATUS_DISK_CORRUPT_ERROR;
		}

		Irp->IoStatus.Information = 0;
		IoCompleteRequest( Irp, IO_DISK_INCREMENT );

		Secondary_Dereference( Secondary );
		
		return TRUE;
	}

	while (1) {

		NTSTATUS status;

		NDASFS_ASSERT( fastMutexSet == FALSE );
		NDASFS_ASSERT( retry == FALSE );

		ExAcquireFastMutex( &Secondary->FastMutex );

		if (FlagOn(Secondary->Flags, SECONDARY_FLAG_CLOSED)) {

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR, 
						   ("Secondary is already closed Secondary = %p\n", Secondary) );

			ExReleaseFastMutex( &Secondary->FastMutex );

			NDASFS_ASSERT( irpSp->MajorFunction == IRP_MJ_CREATE );

			if (irpSp->MajorFunction == IRP_MJ_CLOSE) {

				if (Secondary_LookUpCcb(Secondary, fileObject)) {

					SecondaryFileObjectClose( Secondary, fileObject );
				}

				*Status = Irp->IoStatus.Status = STATUS_SUCCESS;

			} else if (irpSp->MajorFunction == IRP_MJ_CLEANUP) {

				InterlockedDecrement( &((PLFS_FCB)irpSp->FileObject->FsContext)->UncleanCount );
				SetFlag( fileObject->Flags, FO_CLEANUP_COMPLETE );

				*Status = Irp->IoStatus.Status = STATUS_SUCCESS;

			} else {

				*Status = Irp->IoStatus.Status = STATUS_TOO_LATE;
			}

			Irp->IoStatus.Information = 0;
			break;
		}

		if (FlagOn(Secondary->Flags, SECONDARY_FLAG_ERROR)) {

			ExReleaseFastMutex( &Secondary->FastMutex );

			if (irpSp->MajorFunction == IRP_MJ_CLOSE) {

				if (Secondary_LookUpCcb(Secondary, fileObject)) {

					SecondaryFileObjectClose( Secondary, fileObject );
				}

				*Status = Irp->IoStatus.Status = STATUS_SUCCESS;

			} else if (irpSp->MajorFunction == IRP_MJ_CREATE) {

				*Status = Irp->IoStatus.Status = STATUS_IO_DEVICE_ERROR;
			
			} else if (irpSp->MajorFunction == IRP_MJ_CLEANUP) {

				InterlockedDecrement( &((PLFS_FCB)irpSp->FileObject->FsContext)->UncleanCount );
				SetFlag( fileObject->Flags, FO_CLEANUP_COMPLETE );

				*Status = Irp->IoStatus.Status = STATUS_SUCCESS;

			} else {

				*Status = Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
			} 

			Irp->IoStatus.Information = 0;
			break;
		}
		
		ExReleaseFastMutex( &Secondary->FastMutex );

		if (!FlagOn(Secondary->Flags, SECONDARY_FLAG_RECONNECTING)) {

			NDASFS_ASSERT( Secondary->ThreadObject );
		}

		KeEnterCriticalRegion();
		status = RedirectIrp( Secondary, Irp, &fastMutexSet, &retry );
		KeLeaveCriticalRegion();

		if (retry == TRUE) {

			retry = FALSE;
			continue;
		}

		if (status == STATUS_SUCCESS) {

			NDASFS_ASSERT( fastMutexSet == FALSE );

			*Status = Irp->IoStatus.Status;

			if (*Status == STATUS_PENDING) {

				NDASFS_ASSERT( LFS_BUG );
				IoMarkIrpPending(Irp);
			}

			break;	
		} 
		
		if (status == STATUS_DEVICE_REQUIRES_CLEANING) {

			PrintIrp( LFS_DEBUG_LFS_INFO, 
					  "STATUS_DEVICE_REQUIRES_CLEANING", 
					  CONTAINING_RECORD(Secondary, LFS_DEVICE_EXTENSION, Secondary), 
					  Irp );

			if (irpSp->MajorFunction == IRP_MJ_CLEANUP) {

				InterlockedDecrement( &((PLFS_FCB)irpSp->FileObject->FsContext)->UncleanCount );
				SetFlag( fileObject->Flags, FO_CLEANUP_COMPLETE );

				*Status = Irp->IoStatus.Status = STATUS_SUCCESS;

			} else {
			
				*Status = Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
			}

			break;
		} 
		
		if (status == STATUS_ALERTED || status == STATUS_USER_APC) {

			NDASFS_ASSERT( fastMutexSet == FALSE );
			continue;		
		} 

		NDASFS_ASSERT( status == STATUS_REMOTE_DISCONNECT || status == STATUS_IO_DEVICE_ERROR );
		NDASFS_ASSERT( fastMutexSet == TRUE );

		ExAcquireFastMutex( &Secondary->FastMutex );

		ASSERT( FlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) );

		if (Secondary->LfsDeviceExt->SecondaryState == CONNECTED_TO_LOCAL_STATE		|| 
			FlagOn(Secondary->LfsDeviceExt->Flags, LFS_DEVICE_FLAG_SURPRISE_REMOVED)		||
			GlobalLfs.ShutdownOccured == TRUE) {

			SetFlag( Secondary->Flags, SECONDARY_FLAG_ERROR );
			ExReleaseFastMutex( &Secondary->FastMutex );

			KeReleaseSemaphore( &Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE );
			fastMutexSet = FALSE;

			*Status = Irp->IoStatus.Status = STATUS_IO_DEVICE_ERROR;
			Irp->IoStatus.Information = 0;
	
			break;
		}

		if (FlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED)) {

			//BOOLEAN			recoveryResult;

			ASSERT( !FlagOn(Secondary->Flags, SECONDARY_FLAG_RECONNECTING) );
			SetFlag( Secondary->Flags, SECONDARY_FLAG_RECONNECTING );

			ExReleaseFastMutex( &Secondary->FastMutex );

			if (FlagOn(Secondary->Flags, SECONDARY_FLAG_CLOSED)) {

				ASSERT( irpSp->MajorFunction == IRP_MJ_CREATE );
					
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR, 
							   ("Secondary is already closed Secondary = %p\n", Secondary) );

				ClearFlag( Secondary->Flags, SECONDARY_FLAG_RECONNECTING );

				KeReleaseSemaphore( &Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE );

				continue;
			}

			CallRecoverySessionAsynchronously( Secondary );
			fastMutexSet = FALSE;
				
			continue;
		}

		//
		//	Recovery failed.
		//

		SetFlag( Secondary->Flags, SECONDARY_FLAG_ERROR );
		ExReleaseFastMutex( &Secondary->FastMutex );

		KeReleaseSemaphore( &Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE );
		fastMutexSet = FALSE;

		NDASFS_ASSERT( IsListEmpty(&Secondary->RequestQueue) );

		continue;
	}

	NDASFS_ASSERT( fastMutexSet == FALSE );

	if (*Status != STATUS_PENDING) {

		if (!pendingReturned && Irp->PendingReturned) {

			NDASFS_ASSERT( FALSE );
		}

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
	}

	Secondary_Dereference( Secondary );

	return TRUE;
}


VOID
SecondaryFileObjectClose (
	IN PSECONDARY		Secondary,
	IN OUT PFILE_OBJECT	FileObject
	)
{
	PLFS_FCB				fcb = FileObject->FsContext;
	PLFS_CCB			ccb = FileObject->FsContext2;
	KIRQL					oldIrql;

	FileObject->SectionObjectPointer = NULL;
			
	KeAcquireSpinLock( &Secondary->FcbQSpinLock, &oldIrql );
			
	if (InterlockedDecrement(&fcb->OpenCount) == 0) {

		RemoveEntryList( &fcb->ListEntry );
		InitializeListHead( &fcb->ListEntry );
	}

	KeReleaseSpinLock( &Secondary->FcbQSpinLock, oldIrql );
				
	Secondary_DereferenceFcb( fcb );
	
	ExAcquireFastMutex( &Secondary->CcbQMutex );
	RemoveEntryList( &ccb->ListEntry );
	ExReleaseFastMutex( &Secondary->CcbQMutex );

	InitializeListHead( &ccb->ListEntry );
	FreeCcb( Secondary, ccb );

	FileObject->FsContext = NULL;
	FileObject->FsContext2 = NULL;

	return;
}


BOOLEAN
RecoverySession (
	IN  PSECONDARY	Secondary
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
    PLIST_ENTRY				ccblistEntry;


	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("RecoverySession Called Secondary = %p\n", Secondary) );

	ASSERT( Secondary->ThreadHandle );

	RtlCopyMemory( &netdiskPartitionInfo.NetDiskAddress,
				   &Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.NetDiskAddress,
				   sizeof(netdiskPartitionInfo.NetDiskAddress) );

	netdiskPartitionInfo.UnitDiskNo = Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.UnitDiskNo;

	RtlCopyMemory( netdiskPartitionInfo.NdscId,
				   Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.NdscId,
				   NDSC_ID_LENGTH );

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

		if (FlagOn(Secondary->LfsDeviceExt->Flags, LFS_DEVICE_FLAG_DISMOUNTED) ||
			FlagOn(Secondary->LfsDeviceExt->Flags, LFS_DEVICE_FLAG_SURPRISE_REMOVED)) {

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

				//Secondary->Thread.TdiReceiveContext.Irp = NULL;
				KeInitializeEvent( &Secondary->Thread.ReceiveOverLapped.CompletionEvent, NotificationEvent, FALSE );

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


		LfsSecondaryToPrimary( Secondary->LfsDeviceExt );

		if (FlagOn(Secondary->Flags, SECONDARY_FLAG_CLOSED)) {

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
		
		NDASFS_ASSERT( FALSE );

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

    for (ccblistEntry = Secondary->CcbQueue.Blink;
         ccblistEntry != &Secondary->CcbQueue;
         ccblistEntry = ccblistEntry->Blink) {

		PLFS_CCB				ccb;
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


		ccb = CONTAINING_RECORD (ccblistEntry, LFS_CCB, ListEntry);
		ccb->Fcb->FileRecordSegmentHeaderAvail = FALSE;

		if (ccb->CreateContext.RelatedFileHandle != 0) {

			PLFS_CCB	relatedCcb;

			if (ccb->FileObject->RelatedFileObject == NULL) {

				ASSERT( LFS_UNEXPECTED );
				result = FALSE;	
				break;
			}

			if (ccb->RelatedFileObjectClosed == FALSE) {

				relatedCcb = Secondary_LookUpCcb(Secondary, ccb->FileObject->RelatedFileObject);
				ASSERT( relatedCcb != NULL && relatedCcb->LfsMark == LFS_MARK );				
				ASSERT( relatedCcb->SessionId == Secondary->SessionId ); // must be already Updated

				if (relatedCcb->Corrupted == TRUE) {

					ccb->SessionId = Secondary->SessionId;
					ccb->Corrupted = TRUE;
				
					continue;
				}

				ccb->CreateContext.RelatedFileHandle = relatedCcb->PrimaryFileHandle;
			
			} else { // relateFileObject is already closed;
			
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("RecoverySession: relateFileObject is already closed\n") );
				
				ccb->CreateContext.RelatedFileHandle = 0;
				ccb->CreateContext.FileNameLength = ccb->Fcb->FullFileName.Length;

				if (ccb->Fcb->FullFileName.Length) {

					RtlCopyMemory( ccb->Buffer + ccb->CreateContext.EaLength,
								   ccb->Fcb->FullFileName.Buffer,
								   ccb->Fcb->FullFileName.Length );
				}
			}	
		}
				
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, 
					   ("RecoverySession: ccb->Fcb->FullFileName = %wZ ccb->FileObject->CurrentByteOffset.QuadPart = %I64d\n", 
						 &ccb->Fcb->FullFileName, ccb->FileObject->CurrentByteOffset.QuadPart) );

		if (Secondary->LfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NTFS) {

			dataSize = ((ccb->CreateContext.EaLength + ccb->CreateContext.FileNameLength) > Secondary->Thread.SessionContext.BytesPerFileRecordSegment)
						? (ccb->CreateContext.EaLength + ccb->CreateContext.FileNameLength) 
						  : Secondary->Thread.SessionContext.BytesPerFileRecordSegment;
		
		} else
			dataSize = ccb->CreateContext.EaLength + ccb->CreateContext.FileNameLength;

		secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, dataSize );

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
					? sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER) + ADD_ALIGN8(ccb->CreateContext.EaLength + ccb->CreateContext.FileNameLength)
					: sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER) + ccb->CreateContext.EaLength + ccb->CreateContext.FileNameLength;

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

		ExReleaseFastMutex( &Secondary->FastMutex );

		//
		//	[64bit issue]
		//	I assume that IrpTag may have any dummy value.
		//

		ndfsWinxpRequestHeader->IrpTag   = (_U32)PtrToUlong(ccb);
		ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_CREATE;
		ndfsWinxpRequestHeader->IrpMinorFunction = 0;

		ndfsWinxpRequestHeader->FileHandle = 0;

		ndfsWinxpRequestHeader->IrpFlags   = ccb->IrpFlags;
		ndfsWinxpRequestHeader->IrpSpFlags = ccb->IrpSpFlags;

		RtlCopyMemory( &ndfsWinxpRequestHeader->Create,
					   &ccb->CreateContext,
					   sizeof(WINXP_REQUEST_CREATE) );
		
		disposition = FILE_OPEN_IF;
		ndfsWinxpRequestHeader->Create.Options &= 0x00FFFFFF;
		ndfsWinxpRequestHeader->Create.Options |= (disposition << 24);

		ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);

		RtlCopyMemory( ndfsWinxpRequestData,
					   ccb->Buffer,
					   ccb->CreateContext.EaLength + ccb->CreateContext.FileNameLength );

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

	 		ccb->PrimaryFileHandle = ndfsWinxpReplytHeader->Open.FileHandle;
		
		} else {

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR,
						   ("RecoverySession: ccb->Fcb->FullFileName = %wZ Corrupted\n", &ccb->Fcb->FullFileName) );

			ccb->Corrupted = TRUE;
		}

		ccb->SessionId = Secondary->SessionId;

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

VOID
RecoverySessionThreadProc (
	PSECONDARY	Secondary
	)
{
	BOOLEAN	result;

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("RecoverySessionThreadProc entered. Secondary=%p\n", Secondary) );

	if (!FlagOn(Secondary->Flags, SECONDARY_FLAG_CLOSED)) {

		result = RecoverySession( Secondary );

		ExAcquireFastMutex( &Secondary->FastMutex );

		if (result == TRUE) {

			ClearFlag( Secondary->Flags, SECONDARY_FLAG_RECONNECTING );

		} else {
			
			SetFlag( Secondary->Flags, SECONDARY_FLAG_ERROR );

			NDASFS_ASSERT( IsListEmpty(&Secondary->RequestQueue) );
		}

		ExReleaseFastMutex( &Secondary->FastMutex );
	}

	KeReleaseSemaphore( &Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE );
	Secondary_Dereference( Secondary );

	PsTerminateSystemThread( STATUS_SUCCESS );
}


NTSTATUS
CallRecoverySessionAsynchronously (
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
								   RecoverySessionThreadProc,
								   Secondary );

	NDASFS_ASSERT( NT_SUCCESS(status) );

	return status;
}
