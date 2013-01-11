#define	__SECONDARY__
#include "LfsProc.h"

//
//	FastFat Node Type Code
//
//	copied from FastFat header: NodeType.h
//
#define NTC_UNDEFINED                    ((USHORT)0x0000)

#define FAT_NTC_DATA_HEADER              ((USHORT)0x0500)
#define FAT_NTC_VCB                      ((USHORT)0x0501)
#define FAT_NTC_FCB                      ((USHORT)0x0502)
#define FAT_NTC_DCB                      ((USHORT)0x0503)
#define FAT_NTC_ROOT_DCB                 ((USHORT)0x0504)
#define FAT_NTC_CCB                      ((USHORT)0x0507)
#define FAT_NTC_IRP_CONTEXT              ((USHORT)0x0508)


NTSTATUS
RedirectIrp(
	IN  PSECONDARY	Secondary,
	IN  PIRP		Irp,
	OUT	PBOOLEAN	FastMutexSet,
	OUT	PBOOLEAN	Retry
	);


BOOLEAN
RecoverySession(
	IN  PSECONDARY	Secondary
	);


PSECONDARY
Secondary_Create(
	IN	PLFS_DEVICE_EXTENSION	LfsDeviceExt		 
	)
{
	PSECONDARY				secondary;
	NTSTATUS				tableStatus;
	NETDISK_PARTITION_INFO	netdiskPartitionInfo;

	OBJECT_ATTRIBUTES	objectAttributes;
	NTSTATUS			ntStatus ;
	LARGE_INTEGER		timeOut ;
	KIRQL				oldIrql ;
	ULONG				tryQuery;
	

	secondary = ExAllocatePoolWithTag( 
						NonPagedPool, 
                        sizeof(SECONDARY),
						LFS_ALLOC_TAG
						);
	
	if (secondary == NULL)
	{
		ASSERT(LFS_INSUFFICIENT_RESOURCES);
		return NULL;
	}

#if DBG
	InterlockedIncrement(&LfsObjectCounts.SecondaryCount);
#endif
	
	RtlZeroMemory(secondary, sizeof(SECONDARY));

	secondary->Flags = SECONDARY_INITIALIZING;

	KeInitializeSpinLock(&secondary->SpinLock);
	//ExInitializeFastMutex(&secondary->FastMutex);

	secondary->ReferenceCount = 1;

	LfsDeviceExt_Reference(LfsDeviceExt);
	secondary->LfsDeviceExt = LfsDeviceExt;

#if 0
	secondary->VolumeRootFileHandle = OpenVolumeRootFile(LfsDeviceExt);
	if(secondary->VolumeRootFileHandle == NULL)
	{
		Secondary_Dereference(secondary);		
		return NULL;
	}
	CloseVolumeRootFile(secondary->VolumeRootFileHandle);
	secondary->VolumeRootFileHandle = NULL;
#endif

	RtlCopyMemory(
		&netdiskPartitionInfo.NetDiskAddress,
		&secondary->LfsDeviceExt->NetdiskPartitionInformation.NetDiskInformation.NetDiskAddress,
		sizeof(netdiskPartitionInfo.NetDiskAddress)
		);
	netdiskPartitionInfo.UnitDiskNo = secondary->LfsDeviceExt->NetdiskPartitionInformation.NetDiskInformation.UnitDiskNo;
	netdiskPartitionInfo.StartingOffset = secondary->LfsDeviceExt->NetdiskPartitionInformation.PartitionInformation.StartingOffset;

	LfsTable_CleanCachePrimaryAddress(
					GlobalLfs.LfsTable,
					&netdiskPartitionInfo,
					&secondary->PrimaryAddress
					);

#define MAX_TRY_QUERY 2
	
	for(tryQuery = 0; tryQuery < MAX_TRY_QUERY; tryQuery++)
	{
		tableStatus = LfsTable_QueryPrimaryAddress(
						GlobalLfs.LfsTable,
						&netdiskPartitionInfo,
						&secondary->PrimaryAddress
						);

		if(tableStatus == STATUS_SUCCESS)
		{
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
				("LFS: LfsFsControlMountVolumeComplete: Found PrimaryAddress :%02x:%02x:%02x:%02x:%02x:%02x/%d\n",
					secondary->PrimaryAddress.Node[0], secondary->PrimaryAddress.Node[1],
					secondary->PrimaryAddress.Node[2], secondary->PrimaryAddress.Node[3],
					secondary->PrimaryAddress.Node[4], secondary->PrimaryAddress.Node[5],
					NTOHS(secondary->PrimaryAddress.Port)
			));
			break;
		}
	}

	if(tableStatus != STATUS_SUCCESS)
	{
		Secondary_Dereference(secondary);
		return NULL;
	}
		
	secondary->ThreadHandle = NULL;

	InitializeListHead(&secondary->FcbQueue);
	KeInitializeSpinLock(&secondary->FcbQSpinLock);
	InitializeListHead(&secondary->FileExtQueue) ;
    ExInitializeFastMutex(&secondary->FileExtQMutex);

	KeQuerySystemTime(&secondary->CleanUpTime);

	InitializeListHead(&secondary->DirNotifyList);
	FsRtlNotifyInitializeSync(&secondary->NotifySync);

	KeInitializeEvent(&secondary->Thread.ReadyEvent, NotificationEvent, FALSE);
    
	secondary->Thread.TdiReceiveContext.Irp = NULL;
	KeInitializeEvent(&secondary->Thread.TdiReceiveContext.CompletionEvent, NotificationEvent, FALSE) ;

	InitializeListHead(&secondary->Thread.RequestQueue) ;
	KeInitializeSpinLock(&secondary->Thread.RequestQSpinLock) ;
	KeInitializeEvent(&secondary->Thread.RequestEvent, NotificationEvent, FALSE) ;

	InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

	secondary->SessionId = 0;
	
	ntStatus = PsCreateSystemThread(
					&secondary->ThreadHandle,
					THREAD_ALL_ACCESS,
					&objectAttributes,
					NULL,
					NULL,
					SecondaryThreadProc,
					secondary
	);

	if(!NT_SUCCESS(ntStatus)) 
	{
		ASSERT(LFS_UNEXPECTED);
		Secondary_Close(secondary);
		
		return NULL;
	}

	ntStatus = ObReferenceObjectByHandle(
				secondary->ThreadHandle,
				FILE_READ_DATA,
				NULL,
				KernelMode,
				&secondary->ThreadObject,
				NULL
				);

	if(!NT_SUCCESS(ntStatus)) 
	{
		ASSERT(LFS_INSUFFICIENT_RESOURCES);
		Secondary_Close(secondary);
		
		return NULL;
	}

	secondary->SessionId++;

	timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
	ntStatus = KeWaitForSingleObject(
					&secondary->Thread.ReadyEvent,
					Executive,
					KernelMode,
					FALSE,
					&timeOut
					);

	if(ntStatus != STATUS_SUCCESS) 
	{
		ASSERT(LFS_BUG);
		Secondary_Close(secondary);
		
		return NULL;
	}

	KeClearEvent(&secondary->Thread.ReadyEvent);

	KeAcquireSpinLock(&secondary->SpinLock, &oldIrql);
	if(BooleanFlagOn(secondary->Thread.Flags, SECONDARY_THREAD_CONNECTED))
	{
		KeReleaseSpinLock(&secondary->SpinLock, oldIrql);
	}
	else
	{
		if(secondary->Thread.ConnectionStatus != STATUS_FILE_CORRUPT_ERROR)
		{
			KeReleaseSpinLock(&secondary->SpinLock, oldIrql);
	
			Secondary_Close(secondary);
			return NULL;
		}

		KeReleaseSpinLock(&secondary->SpinLock, oldIrql);
	} 

	ASSERT(secondary->Thread.SessionContext.RequestsPerSession);
	KeInitializeSemaphore(
		&secondary->Semaphore, 
		secondary->Thread.SessionContext.RequestsPerSession, 
		MAX_REQUEST_PER_SESSION
		);

	secondary->Flags |= SECONDARY_START;

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
				("Secondary_Create: The client thread are ready secondary = %p\n", secondary));

	return secondary;
}

VOID
Secondary_Close (
	IN  PSECONDARY	Secondary
	)
{
	NTSTATUS		ntStatus ;
	LARGE_INTEGER	timeOut ;

	PLIST_ENTRY secondaryRequestEntry;
	KIRQL		oldIrql ;


	KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql) ;

	if( Secondary->Flags & SECONDARY_THREAD_CLOSED) 
	{
		KeReleaseSpinLock(&Secondary->SpinLock, oldIrql) ;
		return ;
	}
	Secondary->Flags |= SECONDARY_THREAD_CLOSED;
	KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);

	FsRtlNotifyUninitializeSync(&Secondary->NotifySync);

	if(Secondary->ThreadHandle == NULL)
	{
		Secondary_Dereference(Secondary);
		return;
	}

	ASSERT(Secondary->ThreadObject != NULL);
	KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);

	if(!BooleanFlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_TERMINATED))
	{
		PSECONDARY_REQUEST		secondaryRequest;

		KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
		
		secondaryRequest = AllocSecondaryRequest(Secondary, 0, FALSE);
		secondaryRequest->RequestType = SECONDARY_REQ_DISCONNECT;

		QueueingSecondaryRequest(Secondary,	secondaryRequest);

		secondaryRequest = AllocSecondaryRequest(Secondary, 0, FALSE);
		secondaryRequest->RequestType = SECONDARY_REQ_DOWN;

		QueueingSecondaryRequest(Secondary,	secondaryRequest);
	}
	else 
	{
		KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
	}

	timeOut.QuadPart = - LFS_TIME_OUT;
	ntStatus = KeWaitForSingleObject(
						Secondary->ThreadObject,
						Executive,
						KernelMode,
						FALSE,
						&timeOut
						) ;

	if(ntStatus == STATUS_SUCCESS) 
	{
	    SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
						("Secondary_Close: thread stoped Secondary = %p\n", Secondary));

		ObDereferenceObject(Secondary->ThreadObject);

		Secondary->ThreadHandle = NULL;
		Secondary->ThreadObject = NULL;
	}
	else
	{
		ASSERT(LFS_BUG);
		return;
	}

	ASSERT(IsListEmpty(&Secondary->FcbQueue));
	ASSERT(IsListEmpty(&Secondary->FileExtQueue));
	ASSERT(IsListEmpty(&Secondary->Thread.RequestQueue));
	ASSERT(Secondary->FileExtCount == 0);

	while(secondaryRequestEntry = 
			ExInterlockedRemoveHeadList(
					&Secondary->Thread.RequestQueue,
					&Secondary->Thread.RequestQSpinLock
					)
		) 
	{
		PSECONDARY_REQUEST secondaryRequest;
			
		secondaryRequest = CONTAINING_RECORD(
							secondaryRequestEntry,
							SECONDARY_REQUEST,
							ListEntry
							);

		secondaryRequest->ExecuteStatus = STATUS_IO_DEVICE_ERROR ;
		if(secondaryRequest->Synchronous == TRUE)
			KeSetEvent(&secondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE) ;
		else
			DereferenceSecondaryRequest(
				secondaryRequest
				);
	}
	
	Secondary_Dereference(Secondary);

	return;
}


VOID
Secondary_Stop (
	IN  PSECONDARY	Secondary
	)
{
	NTSTATUS		ntStatus;
	LARGE_INTEGER	timeOut;

	PLIST_ENTRY secondaryRequestEntry;
	KIRQL		oldIrql ;

	KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql) ;

	if( Secondary->Flags & SECONDARY_THREAD_CLOSED) {
		KeReleaseSpinLock(&Secondary->SpinLock, oldIrql) ;
		return ;
	}

	Secondary->Flags |= SECONDARY_THREAD_CLOSED;
	KeReleaseSpinLock(&Secondary->SpinLock, oldIrql) ;

	if(Secondary->ThreadHandle == NULL)
	{
		Secondary_Dereference(Secondary);
		return;
	}

	ASSERT(Secondary->ThreadObject != NULL);

	KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql) ;

	if(!BooleanFlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_TERMINATED))
	{
		PSECONDARY_REQUEST		secondaryRequest ;

		KeReleaseSpinLock(&Secondary->SpinLock, oldIrql) ;
		
		secondaryRequest = AllocSecondaryRequest(Secondary, 0, FALSE);
		secondaryRequest->RequestType = SECONDARY_REQ_DISCONNECT;

		QueueingSecondaryRequest(
			Secondary,
			secondaryRequest
			);

		secondaryRequest = AllocSecondaryRequest(Secondary, 0, FALSE);
		secondaryRequest->RequestType = SECONDARY_REQ_DOWN;

		QueueingSecondaryRequest(
			Secondary,
			secondaryRequest
			);
	} else
	{
		KeReleaseSpinLock(&Secondary->SpinLock, oldIrql) ;
	}

	timeOut.QuadPart = - LFS_TIME_OUT;
	ntStatus = KeWaitForSingleObject(
						Secondary->ThreadObject,
						Executive,
						KernelMode,
						FALSE,
						&timeOut
						) ;

	if(ntStatus == STATUS_SUCCESS) 
	{
	    SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
					("Secondary_Close: thread stoped Secondary   = %p\n", Secondary));

		ObDereferenceObject(Secondary->ThreadObject) ;

		Secondary->ThreadHandle = NULL;
		Secondary->ThreadObject = NULL;
		
	} else
	{
		ASSERT(LFS_BUG);
		return;
	}

	ASSERT(Secondary->Thread.RequestQueue.Flink == &Secondary->Thread.RequestQueue);
	ASSERT(Secondary->FileExtQueue.Flink == &Secondary->FileExtQueue);

	while(secondaryRequestEntry = 
			ExInterlockedRemoveHeadList(
					&Secondary->Thread.RequestQueue,
					&Secondary->Thread.RequestQSpinLock
					)
		) 
	{
		PSECONDARY_REQUEST secondaryRequest;
			
		secondaryRequest = CONTAINING_RECORD(
							secondaryRequestEntry,
							SECONDARY_REQUEST,
							ListEntry
							);

		secondaryRequest->ExecuteStatus = STATUS_IO_DEVICE_ERROR ;
		if(secondaryRequest->Synchronous == TRUE)
			KeSetEvent(&secondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE) ;
		else
			DereferenceSecondaryRequest(
				secondaryRequest
				);
	}
	
	return;
}


VOID
Secondary_Reference (
	IN  PSECONDARY	Secondary
	)
{
    LONG result;
	
    result = InterlockedIncrement (&Secondary->ReferenceCount);

    ASSERT (result >= 0);
}


VOID
Secondary_Dereference (
	IN  PSECONDARY	Secondary
	)
{
    LONG result;


    result = InterlockedDecrement (&Secondary->ReferenceCount);
    ASSERT (result >= 0);

    if (result == 0) 
	{
		PLFS_DEVICE_EXTENSION lfsDeviceExt = Secondary->LfsDeviceExt;

		ExFreePoolWithTag(
			Secondary,
			LFS_ALLOC_TAG
			);

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
				("Secondary_Dereference: Secondary is Freed Secondary  = %p\n", Secondary));

#if DBG
		InterlockedDecrement(&LfsObjectCounts.SecondaryCount);
#endif
		LfsDeviceExt_Dereference(lfsDeviceExt);
	}
}


VOID
Secondary_TryCloseFilExts(
	PSECONDARY Secondary
	)
{
	PLIST_ENTRY		listEntry;

	Secondary_Reference(Secondary);	
	if(ExTryToAcquireFastMutex(&Secondary->FileExtQMutex) == FALSE)
	{
		Secondary_Dereference(Secondary);
		return;
	}
	
	if(BooleanFlagOn(Secondary->Flags, SECONDARY_RECONNECTING))
	{
		ExReleaseFastMutex(&Secondary->FileExtQMutex);
		Secondary_Dereference(Secondary);
		return;
	}

#if 0	
	if(Secondary->LfsDeviceExt->SecondaryState == CONNECT_TO_LOCAL_STATE)
		if(IsListEmpty(&Secondary->FileExtQueue) && Secondary->LfsDeviceExt->FileSpyDeviceObject)
		{
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
						("Secondary_TryCloseFilExts: IoDeleteDevice Secondary->LfsDeviceExt = %p\n", 
							Secondary->LfsDeviceExt));
			IoDeleteDevice(Secondary->LfsDeviceExt->FileSpyDeviceObject);
//
//			Do not access KfsDeviceExt after releasing FileSpyDeviceObject.
//
//			Secondary->LfsDeviceExt->FileSpyDeviceObject = NULL;
		}
#endif
		
	listEntry = Secondary->FileExtQueue.Flink;

	while(listEntry != &Secondary->FileExtQueue)
	{
		PFILE_EXTENTION				fileExt = NULL;
		PLFS_FCB					fcb;
		PSECTION_OBJECT_POINTERS	section;
		//IO_STATUS_BLOCK				ioStatusBlock;
		BOOLEAN						dataSectionExists;
		BOOLEAN						imageSectionExists;

		fileExt = CONTAINING_RECORD (listEntry, FILE_EXTENTION, ListEntry);
		listEntry = listEntry->Flink;

		if(fileExt->TypeOfOpen != UserFileOpen)
			break;

		fcb = fileExt->Fcb;

		if(fcb == NULL)
		{
			ASSERT(LFS_BUG);
			break;
		}	

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
						("Secondary_TryCloseFilExts: fcb->FullFileName = %wZ\n", &fileExt->Fcb->FullFileName));

		if(fcb->UncleanCount != 0)
		{
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
							("Secondary_TryCloseFilExts: fcb->FullFileName = %wZ\n", &fileExt->Fcb->FullFileName));
			break;
		}

	    if (fcb->Header.PagingIoResource != NULL)
		{
			ASSERT(LFS_REQUIRED);
			break;
		}

		section = &fcb->NonPaged->SectionObjectPointers;			
		if(section == NULL)
			break;

        //CcFlushCache(section, NULL, 0, &ioStatusBlock);

		dataSectionExists = (BOOLEAN)(section->DataSectionObject != NULL);
		imageSectionExists = (BOOLEAN)(section->ImageSectionObject != NULL);

		if (imageSectionExists) 
		{
			(VOID)MmFlushImageSection( section, MmFlushForWrite );
		}

		if (dataSectionExists) 
		{
            CcPurgeCacheSection( section, NULL, 0, FALSE );
	    }
	}

	ExReleaseFastMutex(&Secondary->FileExtQMutex);
	Secondary_Dereference(Secondary);

	return;
}


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
	BOOLEAN				pendingReturned ;
	KIRQL				oldIrql ;

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
	

	RtlInitUnicodeString(&Mft, L"\\$Mft");
	RtlInitUnicodeString(&MftMirr, L"\\$MftMirr");
	RtlInitUnicodeString(&LogFile, L"\\$LogFile");
	RtlInitUnicodeString(&Directory, L"\\$Directory");
	RtlInitUnicodeString(&BitMap, L"\\$BitMap");
	RtlInitUnicodeString(&MountMgr, L"\\:$MountMgrRemoteDatabase");
	RtlInitUnicodeString(&Extend, L"\\$Extend\\$Reparse");
	RtlInitUnicodeString(&ExtendPlus, L"\\$Extend\\$Reparse:$R:$INDEX_ALLOCATION");
	RtlInitUnicodeString(&System, L"\\System Volume Information\\");
#endif

	ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL) ;

	//
	//	make sure that Secondary context will not be gone during operation.
	//
	Secondary_Reference(Secondary);
	fileObject = irpSp->FileObject;

	//
	//	Irp may come in with Irp->PendingReturned TRUE.
	//	We need to save pendingReturned to see if Irp->PendingReturned is changed during the following process.
	//
	pendingReturned = Irp->PendingReturned;
 
#if DBG
	
	if(irpSp->MajorFunction == IRP_MJ_QUERY_EA						// 0x07
		|| irpSp->MajorFunction == IRP_MJ_SET_EA					// 0x08
		|| irpSp->MajorFunction == IRP_MJ_LOCK_CONTROL				// 0x11
		|| irpSp->MajorFunction == IRP_MJ_QUERY_QUOTA				// 0x19
		|| irpSp->MajorFunction == IRP_MJ_SET_QUOTA					// 0x1a
		|| irpSp->MajorFunction == IRP_MJ_QUERY_SECURITY			// 0x14
		|| irpSp->MajorFunction == IRP_MJ_SET_SECURITY)				// 0x15
	{
		PrintIrp(LFS_DEBUG_SECONDARY_TRACE, "Secondary_PassThrough", Secondary->LfsDeviceExt, Irp);
	}
	else	
	{
		PrintIrp(LFS_DEBUG_SECONDARY_TRACE, "Secondary_PassThrough", Secondary->LfsDeviceExt, Irp);
	}

#endif

	if(fileObject && fileObject->Flags & FO_DIRECT_DEVICE_OPEN)
	{
		ASSERT(LFS_REQUIRED);
		DbgPrint("Direct device open\n");
	}

	if(irpSp->MajorFunction == IRP_MJ_CREATE						// 0x00
		|| irpSp->MajorFunction == IRP_MJ_CLOSE						// 0x01
		|| irpSp->MajorFunction == IRP_MJ_READ						// 0x03
		|| irpSp->MajorFunction == IRP_MJ_WRITE						// 0x04
		|| irpSp->MajorFunction == IRP_MJ_QUERY_INFORMATION			// 0x05
		|| irpSp->MajorFunction == IRP_MJ_SET_INFORMATION			// 0x06
		|| irpSp->MajorFunction == IRP_MJ_QUERY_EA					// 0x07
		|| irpSp->MajorFunction == IRP_MJ_SET_EA					// 0x08
		|| irpSp->MajorFunction == IRP_MJ_FLUSH_BUFFERS				// 0x09
		|| irpSp->MajorFunction == IRP_MJ_QUERY_VOLUME_INFORMATION	// 0x0a
		|| irpSp->MajorFunction == IRP_MJ_SET_VOLUME_INFORMATION	// 0x0b
		|| irpSp->MajorFunction == IRP_MJ_DIRECTORY_CONTROL			// 0x0c
		|| irpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL		// 0x0c
		|| irpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL			// 0x0e
		|| irpSp->MajorFunction == IRP_MJ_LOCK_CONTROL				// 0x11
		|| irpSp->MajorFunction == IRP_MJ_CLEANUP					// 0x12
		|| irpSp->MajorFunction == IRP_MJ_QUERY_SECURITY			// 0x14
		|| irpSp->MajorFunction == IRP_MJ_SET_SECURITY				// 0x15
		|| irpSp->MajorFunction == IRP_MJ_QUERY_QUOTA				// 0x19
		|| irpSp->MajorFunction == IRP_MJ_SET_QUOTA)				// 0x1a
	{
		ASSERT(fileObject);
		
		if(irpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL)
	{
		if(irpSp->MinorFunction == IRP_MN_MOUNT_VOLUME 
			|| irpSp->MinorFunction == IRP_MN_VERIFY_VOLUME 
			|| irpSp->MinorFunction == IRP_MN_LOAD_FILE_SYSTEM) 
		{
			ASSERT(LFS_UNEXPECTED);
			ASSERT(Secondary_LookUpFileExtension(Secondary, fileObject) == NULL);

			PrintIrp(LFS_DEBUG_SECONDARY_INFO, "Secondary_PassThrough", Secondary->LfsDeviceExt, Irp);

			Secondary_Dereference(Secondary);
			return FALSE;
			}
		}
	}
	else if(irpSp->MajorFunction == IRP_MJ_PNP)							// 0x1b
	{
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
						("Secondary_PassThrough: IRP_MJ_PNP %x\n", irpSp->MinorFunction));

		if(irpSp->MinorFunction == IRP_MN_QUERY_REMOVE_DEVICE)
		{
			if(BooleanFlagOn(Secondary->Flags, SECONDARY_RECONNECTING))  // While Disabling and disconnected
			{
				//ASSERT(!IsListEmpty(&Secondary->FcbQueue));
				*NtStatus = Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest(Irp, IO_DISK_INCREMENT) ;
				Secondary_Dereference(Secondary);
				return TRUE;
			}

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
						("Secondary_PassThrough: IRP_MN_QUERY_REMOVE_DEVICE Entered\n"));
		
			Secondary_TryCloseFilExts(Secondary);
			
			if(!IsListEmpty(&Secondary->FcbQueue))
			{
				LARGE_INTEGER interval;
				
				// Wait all files closed
				interval.QuadPart = (1 * DELAY_ONE_SECOND);      //delay 1 seconds
				KeDelayExecutionThread(KernelMode, FALSE, &interval);
			}
			
			if(!IsListEmpty(&Secondary->FcbQueue))
			{
				*NtStatus = Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest(Irp, IO_DISK_INCREMENT) ;
				Secondary_Dereference(Secondary);
				return TRUE;
			}

			Secondary_Dereference(Secondary);
			return FALSE;
		}
		else if(irpSp->MinorFunction == IRP_MN_REMOVE_DEVICE)
		{
			if(!IsListEmpty(&Secondary->FcbQueue))
			{
				ASSERT(LFS_BUG);
				*NtStatus = Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest(Irp, IO_DISK_INCREMENT) ;
				Secondary_Dereference(Secondary);
				return TRUE;
			}
			Secondary_Dereference(Secondary);
			return FALSE;			
		}
		else
		{
			if(fileObject && Secondary_LookUpFileExtension(Secondary, fileObject))
			{
				*NtStatus = Irp->IoStatus.Status = STATUS_SUCCESS;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest(Irp, IO_DISK_INCREMENT);
				Secondary_Dereference(Secondary);
				return TRUE;
			}
			Secondary_Dereference(Secondary);
			return FALSE;			
		}
	}
	else if(irpSp->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL	// 0x0f
				|| irpSp->MajorFunction == IRP_MJ_SHUTDOWN			// 0x10
				|| irpSp->MajorFunction == IRP_MJ_CREATE_MAILSLOT	// 0x13
				|| irpSp->MajorFunction == IRP_MJ_POWER				// 0x16
				|| irpSp->MajorFunction == IRP_MJ_SYSTEM_CONTROL	// 0x17
				|| irpSp->MajorFunction == IRP_MJ_DEVICE_CHANGE)	// 0x18
	{
		ASSERT(LFS_REQUIRED);
		PrintIrp(LFS_DEBUG_SECONDARY_INFO, "Secondary_PassThrough", Secondary->LfsDeviceExt, Irp);

		Secondary_Dereference(Secondary);
		return FALSE;
	}
	else
	{
		ASSERT(irpSp->MajorFunction < IRP_MJ_CREATE || irpSp->MajorFunction > IRP_MJ_MAXIMUM_FUNCTION);
		ASSERT(LFS_UNEXPECTED);

		Secondary_Dereference(Secondary);
		return FALSE;
	}		


#if DBG
	
	if(fileObject->FileName.Length < 2
		|| RtlEqualUnicodeString(&Mft, &fileObject->FileName, TRUE) 
		|| RtlEqualUnicodeString(&MftMirr, &fileObject->FileName, TRUE) 
		|| RtlEqualUnicodeString(&LogFile, &fileObject->FileName, TRUE) 
		|| RtlEqualUnicodeString(&Directory, &fileObject->FileName, TRUE)
		|| RtlEqualUnicodeString(&MountMgr, &fileObject->FileName, TRUE)
		|| RtlEqualUnicodeString(&Extend, &fileObject->FileName, TRUE)
		|| RtlEqualUnicodeString(&ExtendPlus, &fileObject->FileName, TRUE)
		|| RtlEqualUnicodeString(&System, &fileObject->FileName, TRUE)
		|| RtlEqualUnicodeString(&BitMap, &fileObject->FileName, TRUE)
		|| fileObject->FileName.Length >=2 && fileObject->FileName.Buffer[1] == L'$')
	{
		PrintIrp(LFS_DEBUG_SECONDARY_TRACE, "Secondary_PassThrough", Secondary->LfsDeviceExt, Irp);

		if(
			//fileObject->FileName.Length < 2 
			RtlEqualUnicodeString(&Mft, &fileObject->FileName, TRUE) 
				|| RtlEqualUnicodeString(&MftMirr, &fileObject->FileName, TRUE) 
				|| RtlEqualUnicodeString(&LogFile, &fileObject->FileName, TRUE) 
				|| RtlEqualUnicodeString(&BitMap, &fileObject->FileName, TRUE)
				|| RtlEqualUnicodeString(&Directory, &fileObject->FileName, TRUE)
			//|| RtlEqualUnicodeString(&MountMgr, &fileObject->FileName, TRUE)
			//|| RtlEqualUnicodeString(&Extend, &fileObject->FileName, TRUE)
			//|| RtlEqualUnicodeString(&ExtendPlus, &fileObject->FileName, TRUE)
			//|| RtlEqualUnicodeString(&System, &fileObject->FileName, TRUE)
			//|| fileObject->FileName.Length >=2 && fileObject->FileName.Buffer[1] == L'$'
			)
		{
			//Secondary_Dereference(Secondary);
			//return FALSE;
		}
	}		

#endif

    ASSERT(fileObject/*&& fileObject->FileName.Length*/);
	
	if(Secondary->Thread.ConnectionStatus == STATUS_FILE_CORRUPT_ERROR)
	{
		*NtStatus = Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_DISK_INCREMENT);

		Secondary_Dereference(Secondary);
		return TRUE;
	}

	while(1)
	{	
		BOOLEAN	fastMutexSet;
		BOOLEAN	retry;


		KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql) ;

		if(BooleanFlagOn(Secondary->Flags, SECONDARY_ERROR))
		{
			KeReleaseSpinLock(&Secondary->SpinLock, oldIrql) ;
			*NtStatus = Irp->IoStatus.Status = STATUS_IO_DEVICE_ERROR;
			Irp->IoStatus.Information = 0;
			break;
		}
		
		KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);

		if(!BooleanFlagOn(Secondary->Flags, SECONDARY_RECONNECTING))
		{
			ASSERT(Secondary->ThreadObject);
			ASSERT(BooleanFlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_CONNECTED));
		}

		ASSERT(fileObject->DeviceObject);

		
#if DBG
		InterlockedIncrement(&LfsObjectCounts.RedirectIrpCount);
#endif

		//
		//	redirect the IRP to the primary host.
		//
		redirectStatus = RedirectIrp(Secondary, Irp, &fastMutexSet, &retry);

#if DBG

		if(LfsObjectCounts.RedirectIrpCount > 3)
			SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_INFO, 
				("LfsObjectCounts.RedirectIrpCount = %d\n", LfsObjectCounts.RedirectIrpCount));

		InterlockedDecrement(&LfsObjectCounts.RedirectIrpCount);

		if(redirectStatus != STATUS_SUCCESS)
		{
			PrintIrp(LFS_DEBUG_SECONDARY_ERROR, "RedirectIrp Error", Secondary->LfsDeviceExt, Irp);
			SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_INFO, 
				("PurgeVolumeSafe = %d, retry = %d\n", Secondary->LfsDeviceExt->PurgeVolumeSafe, retry));
		}

#endif

		if(retry == TRUE)
			continue;

		if(redirectStatus == STATUS_ABANDONED)
		{
			KIRQL	oldIrql;

			SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_INFO, 
				("redirectStatus == STATUS_ABANDONED\n"));

			ASSERT(fastMutexSet == TRUE);

			KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
			
			if(Secondary->SemaphoreConsumeRequiredCount == 0)
				KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);
			else
				Secondary->SemaphoreConsumeRequiredCount --;
			
			KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);

			fastMutexSet = FALSE;

			continue;
		}

		if(redirectStatus == STATUS_WORKING_SET_LIMIT_RANGE)
		{
			SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_INFO, 
				("redirectStatus == STATUS_WORKING_SET_LIMIT_RANGE\n"));

			ASSERT(fastMutexSet == TRUE);
			//KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);
			fastMutexSet = FALSE;

			continue;
		}

		if(redirectStatus == STATUS_SUCCESS)
		{
			ASSERT(fastMutexSet == FALSE);

			*NtStatus = Irp->IoStatus.Status;
			if(*NtStatus == STATUS_PENDING)
			{
				ASSERT(LFS_BUG);
				IoMarkIrpPending(Irp);
			}
			else 
			{
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
						( "completed. Stat=%08lx Info=%d\n", 
							Irp->IoStatus.Status, Irp->IoStatus.Information));
				PrintIrp(LFS_DEBUG_SECONDARY_TRACE, "RedirectIrp", Secondary->LfsDeviceExt, Irp);
				if(!(
					*NtStatus == STATUS_SUCCESS
					|| irpSp->MajorFunction == IRP_MJ_CREATE && *NtStatus == STATUS_NO_SUCH_FILE && Irp->IoStatus.Information == 0
					|| irpSp->MajorFunction == IRP_MJ_CREATE && *NtStatus == STATUS_OBJECT_NAME_NOT_FOUND && Irp->IoStatus.Information == 0
					|| irpSp->MajorFunction == IRP_MJ_CREATE && *NtStatus == STATUS_OBJECT_PATH_NOT_FOUND && Irp->IoStatus.Information == 0
					|| irpSp->MajorFunction == IRP_MJ_DIRECTORY_CONTROL && *NtStatus == STATUS_NO_MORE_FILES && Irp->IoStatus.Information == 0
					|| irpSp->MajorFunction == IRP_MJ_DIRECTORY_CONTROL && *NtStatus == STATUS_NO_SUCH_FILE && Irp->IoStatus.Information == 0
					|| irpSp->MajorFunction == IRP_MJ_READ && *NtStatus == STATUS_END_OF_FILE && Irp->IoStatus.Information == 0
					))
				{
					SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
							( "completed. Stat=%08lx Info=%d\t", 
								Irp->IoStatus.Status, Irp->IoStatus.Information));
					PrintIrp(LFS_DEBUG_SECONDARY_INFO, NULL, Secondary->LfsDeviceExt, Irp);
				}
			}
			break;
		}
		else if(redirectStatus == STATUS_TIMEOUT)
		{
			*NtStatus = Irp->IoStatus.Status = STATUS_PENDING;
			IoMarkIrpPending(Irp);

			break;
		}
		else
		{
			PLIST_ENTRY		secondaryRequestEntry;
	
			ASSERT(fastMutexSet == TRUE);
			KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql) ;

			ASSERT(BooleanFlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_REMOTE_DISCONNECTED));

			if(Secondary->LfsDeviceExt->SecondaryState == CONNECT_TO_LOCAL_STATE
				|| BooleanFlagOn(Secondary->LfsDeviceExt->Flags, LFS_DEVICE_SURPRISE_REMOVAL)
				|| GlobalLfs.ShutdownOccured == TRUE)
			{
				SetFlag(Secondary->Flags, SECONDARY_ERROR);
				KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
				
				*NtStatus = Irp->IoStatus.Status = STATUS_IO_DEVICE_ERROR;
				Irp->IoStatus.Information = 0;
	
				//ExReleaseFastMutex(&Secondary->FastMutex);
				KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

				fastMutexSet = FALSE;
				break;
			}

			if(BooleanFlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_REMOTE_DISCONNECTED))
			{
				BOOLEAN			recoveryResult;


				ASSERT(Secondary->SemaphoreReturnCount == 0);

				while(1)
				{
					NTSTATUS		waitStatus;
					LARGE_INTEGER	timeOut;
								
					timeOut.QuadPart = HZ >> 2; 

					KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
					waitStatus = KeWaitForSingleObject( 
									&Secondary->Semaphore,
									Executive,
									KernelMode,
									FALSE,
									&timeOut
									);
					KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);

					if(waitStatus == STATUS_TIMEOUT)
						break;

					if(waitStatus == STATUS_SUCCESS)
					{
						Secondary->SemaphoreReturnCount++;
					}
					else
					{
						ASSERT(LFS_UNEXPECTED);
						break;
					}	
				}

				ASSERT(!BooleanFlagOn(Secondary->Flags, SECONDARY_RECONNECTING));
				SetFlag(Secondary->Flags, SECONDARY_RECONNECTING);

				KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
				recoveryResult = RecoverySession(Secondary);
				KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);


				while(Secondary->SemaphoreReturnCount)
				{
					KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);
					Secondary->SemaphoreReturnCount--;
				}

				if(recoveryResult == TRUE)
				{						
					ClearFlag(Secondary->Flags, SECONDARY_RECONNECTING);

					if(Secondary->SemaphoreConsumeRequiredCount == 0)
						KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);
					else
						Secondary->SemaphoreConsumeRequiredCount--;
					
					KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
	
					fastMutexSet = FALSE;
					continue;
				}
			}

			SetFlag(Secondary->Flags, SECONDARY_ERROR);
			KeReleaseSpinLock(&Secondary->SpinLock, oldIrql) ;

			//ExReleaseFastMutex(&Secondary->FastMutex);
			KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

			fastMutexSet = FALSE;

			*NtStatus = Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
			Irp->IoStatus.Information = 0;

			while(secondaryRequestEntry = 
							ExInterlockedRemoveHeadList(
										&Secondary->Thread.RequestQueue,
										&Secondary->Thread.RequestQSpinLock
										))
			{
				PSECONDARY_REQUEST	secondaryRequest;

				InitializeListHead(secondaryRequestEntry);

				secondaryRequest = CONTAINING_RECORD(
										secondaryRequestEntry,
										SECONDARY_REQUEST,
										ListEntry
										) ;

				ASSERT(secondaryRequest->RequestType == SECONDARY_REQ_DISCONNECT);
				secondaryRequest->ExecuteStatus = STATUS_ABANDONED;

				ASSERT(secondaryRequest->Synchronous == TRUE);
				KeSetEvent(&secondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE);				
			}

			break;
		}

		break;
	}

	if(*NtStatus != STATUS_PENDING)
	{
		if (!pendingReturned && Irp->PendingReturned) 
		{
			//
			//	It must not happen.
			//
			ASSERT(FALSE) ;
/*
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
				( "Secondary_PassThrough: PendingReturned = TRUE. Mark pending.\n")) ;

			IoMarkIrpPending( Irp );
			*NtStatus = STATUS_PENDING ;
*/
		}
		IoCompleteRequest(Irp, IO_DISK_INCREMENT) ;
	}

	Secondary_Dereference(Secondary);
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
	if(DataSize > Secondary->Thread.SessionContext.SecondaryMaxDataSize)
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
			("ALLOC_WINXP_SECONDARY_REQUEST %s(%d) MdataSize = %X\n", 
				IrpMajors[IrpMajorFunction], IrpMajorFunction, DataSize));

		
	return AllocSecondaryRequest(
				(Secondary),
				sizeof(NDFS_REQUEST_HEADER)
				+ (
					(Secondary->Thread.SessionContext.MessageSecurity == 0)
						? (sizeof(NDFS_WINXP_REQUEST_HEADER) + (DataSize))
							: (
								 ((IrpMajorFunction == IRP_MJ_WRITE
									&& Secondary->Thread.SessionContext.RwDataSecurity == 0
									&& DataSize <= Secondary->Thread.SessionContext.PrimaryMaxDataSize)
								 ||
								 (IrpMajorFunction == IRP_MJ_READ
									&& Secondary->Thread.SessionContext.RwDataSecurity == 0	
									&& DataSize <= Secondary->Thread.SessionContext.SecondaryMaxDataSize))		
								   ? (ADD_ALIGN8(sizeof(NDFS_WINXP_REQUEST_HEADER))*2 + DataSize)		
								   : (ADD_ALIGN8(sizeof(NDFS_WINXP_REQUEST_HEADER) + DataSize)*2)
					)
				  ),
				TRUE
				);
}


FORCEINLINE
VOID
INITIALIZE_NDFS_REQUEST_HEADER(
	PNDFS_REQUEST_HEADER	NdfsRequestHeader,
	_U8						Command,
	PSECONDARY				Secondary,
	_U8						IrpMajorFunction,
	_U32					DataSize
	)
{
	KIRQL	oldIrql;

	KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);

	RtlCopyMemory(NdfsRequestHeader->Protocol, NDFS_PROTOCOL, sizeof(NdfsRequestHeader->Protocol));
	NdfsRequestHeader->Command	= Command;				
	NdfsRequestHeader->Flags	= Secondary->Thread.SessionContext.Flags;							    
	NdfsRequestHeader->Uid		= Secondary->Thread.SessionContext.Uid;									
	NdfsRequestHeader->Tid		= Secondary->Thread.SessionContext.Tid;									
	NdfsRequestHeader->Mid		= 0;																	    
	NdfsRequestHeader->MessageSize																			
		= sizeof(NDFS_REQUEST_HEADER)																			
			+ (																									
				(Secondary->Thread.SessionContext.MessageSecurity == 0)										
	 			 ? sizeof(NDFS_WINXP_REQUEST_HEADER) + DataSize											
				 : (																							
					 ((IrpMajorFunction == IRP_MJ_WRITE														
						&& Secondary->Thread.SessionContext.RwDataSecurity == 0								
						&& DataSize <= Secondary->Thread.SessionContext.PrimaryMaxDataSize)				
					 ||																							
					 IrpMajorFunction == IRP_MJ_READ																
						&& Secondary->Thread.SessionContext.RwDataSecurity == 0								
						&& DataSize <= Secondary->Thread.SessionContext.SecondaryMaxDataSize)

					? ADD_ALIGN8(sizeof(NDFS_WINXP_REQUEST_HEADER) + DataSize)							
					: ADD_ALIGN8(sizeof(NDFS_WINXP_REQUEST_HEADER) + DataSize)				
				   )																							
				);																								
	
	KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);

	return;
}																										


// FALSE : Irp IO is occurred
// TRUE	 : Fast IO is executed

BOOLEAN
Secondary_RedirectFileIo(
	IN  PSECONDARY		Secondary,
	IN  PLFS_FILE_IO	LfsFileIo
	)
{
	BOOLEAN			returnResult;
	PFILE_OBJECT	fileObject;

	PLFS_FCB			fcb;
	PFILE_EXTENTION		fileExt;
	PSECONDARY_REQUEST	secondaryRequest;


	return FALSE;

	
	fileObject = LfsFileIo->FileObject;
 
	if(LfsFileIo->FileIoType != LFS_FILE_IO_CREATE)
	{
		fcb = fileObject->FsContext;
		fileExt = fileObject->FsContext2;
	
		ASSERT(Secondary_LookUpFileExtension(Secondary, fileObject) == fileExt);
		ASSERT(fileExt->Fcb == fcb);

		if(fileExt->Corrupted == TRUE)
			return FALSE;
 	}


	switch(LfsFileIo->FileIoType)
	{
    case LFS_FILE_IO_FAST_IO_CHECK_IF_POSSIBLE: // 0x1c
	{
		returnResult = TRUE;
		break;
	}
    case LFS_FILE_IO_READ: // 0x03
	{
		PREAD_FILE_IO				readFileIo = &LfsFileIo->Read;

		PVOID						inputBuffer = NULL;
		ULONG						inputBufferLength = 0;
		PVOID						outputBuffer;
		
		LARGE_INTEGER				currentByteOffset;

		ULONG						totalReadRequestLength;
		NTSTATUS					lastStatus;

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;


		if(readFileIo->Wait == FALSE)
			return FALSE;

		outputBuffer	= readFileIo->Buffer;
		

		ASSERT(!(readFileIo->FileOffset->LowPart == FILE_USE_FILE_POINTER_POSITION 
						&& readFileIo->FileOffset->HighPart == -1));

		currentByteOffset.QuadPart = readFileIo->FileOffset->QuadPart;
		totalReadRequestLength = 0;
		
		do
		{
			ULONG						outputBufferLength;
			LARGE_INTEGER				timeOut;
			NTSTATUS					waitStatus;
			PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
			

//			outputBufferLength = read->Length;
			outputBufferLength = ((readFileIo->Length-totalReadRequestLength) <= Secondary->Thread.SessionContext.SecondaryMaxDataSize) 
									? (readFileIo->Length-totalReadRequestLength) : Secondary->Thread.SessionContext.SecondaryMaxDataSize;


			secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
								Secondary,
								IRP_MJ_READ,
								outputBufferLength
								);

			if(secondaryRequest == NULL)
			{
				ASSERT(LFS_REQUIRED);

				returnResult = FALSE;	
				break;
			}
			
			secondaryRequest->OutputBuffer = (PUCHAR)outputBuffer+totalReadRequestLength;
			secondaryRequest->OutputBufferLength = outputBufferLength;
	
			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
			INITIALIZE_NDFS_REQUEST_HEADER(
				ndfsRequestHeader,
				NDFS_COMMAND_EXECUTE,
				Secondary,
				IRP_MJ_READ,
				0
				);

			ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);

			ndfsWinxpRequestHeader->IrpTag				= (_U32)LfsFileIo;
			ndfsWinxpRequestHeader->IrpMajorFunction	= IRP_MJ_READ;
			ndfsWinxpRequestHeader->IrpMinorFunction	= 0;
			ndfsWinxpRequestHeader->FileHandle			= fileExt->PrimaryFileHandle;
			ndfsWinxpRequestHeader->IrpFlags			= 0;
			ndfsWinxpRequestHeader->IrpSpFlags			= 0;
	
			ndfsWinxpRequestHeader->Read.Length		= outputBufferLength;
			ndfsWinxpRequestHeader->Read.Key		= readFileIo->LockKey;
			ndfsWinxpRequestHeader->Read.ByteOffset = currentByteOffset.QuadPart+totalReadRequestLength;

			{														
				KIRQL		oldIrql;
				NTSTATUS	waitStatus;
															
				//ExAcquireFastMutex(&Secondary->FastMutex);
				waitStatus = KeWaitForSingleObject( 
								&Secondary->Semaphore,
								Executive,
								KernelMode,
								FALSE,
								NULL
								);
				ASSERT(waitStatus == STATUS_SUCCESS);
																											
				KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
																											
				if(Secondary->Flags & SECONDARY_ERROR
					|| Secondary->SessionId != secondaryRequest->SessionId
					|| fileExt->Corrupted == TRUE)
				{						
					DereferenceSecondaryRequest(			
						secondaryRequest	
						);				
					secondaryRequest = NULL;									
																				
					KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);		

					//ExReleaseFastMutex(&Secondary->FastMutex);				
					KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);
		
					return FALSE;
				}											
				KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
			}
			
			secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
			QueueingSecondaryRequest(
				Secondary,
				secondaryRequest
				);
				
			timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec

#if DBG
			if(KeGetCurrentIrql() == APC_LEVEL) {
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
					("PrimaryAgentThreadProc: READ: IRLQ is APC! going to sleep.\n"));
			}
#endif
			waitStatus = KeWaitForSingleObject(
								&secondaryRequest->CompleteEvent,
								Executive,
								KernelMode,
								FALSE,
								&timeOut
								);

			KeClearEvent(&secondaryRequest->CompleteEvent);

			ASSERT(waitStatus == STATUS_SUCCESS);

#if DBG
			if(KeGetCurrentIrql() == APC_LEVEL) {
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
					("PrimaryAgentThreadProc: READ: IRLQ is APC! woke up.\n"));
			}
#endif

			if(waitStatus != STATUS_SUCCESS) 
			{
				ASSERT(LFS_BUG);
				secondaryRequest = NULL;
				returnResult = FALSE;	
				
				//ExReleaseFastMutex(&Secondary->FastMutex);
				KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

				break;
			}

			if(secondaryRequest->ExecuteStatus != STATUS_SUCCESS)
			{
				returnResult = FALSE;
				DereferenceSecondaryRequest(
					secondaryRequest
					);
				secondaryRequest = NULL;
				
				//ExReleaseFastMutex(&Secondary->FastMutex);
				KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);
				
				break;
			}
				
			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

			totalReadRequestLength += ndfsWinxpReplytHeader->Information;
			lastStatus = ndfsWinxpReplytHeader->Status;
			returnResult = TRUE;
			
			if(lastStatus != STATUS_SUCCESS)
			{	
				returnResult = TRUE;
				DereferenceSecondaryRequest(
					secondaryRequest
					);
				secondaryRequest = NULL;				
				
				//ExReleaseFastMutex(&Secondary->FastMutex);
				KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

				break;
			}

			ASSERT(ADD_ALIGN8(ndfsWinxpReplytHeader->Information) == ADD_ALIGN8(secondaryRequest->NdfsReplyHeader.MessageSize - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER)));
			ASSERT(ndfsWinxpReplytHeader->Information <= secondaryRequest->OutputBufferLength);
			ASSERT(secondaryRequest->OutputBufferLength == 0 || secondaryRequest->OutputBuffer);

			if(ndfsWinxpReplytHeader->Information)
			{
				RtlCopyMemory(
					secondaryRequest->OutputBuffer,
					(_U8 *)(ndfsWinxpReplytHeader+1),
					ndfsWinxpReplytHeader->Information
					);
			}

//			ASSERT(Irp->PendingReturned == FALSE) ;

			if(ndfsWinxpReplytHeader->Information != outputBufferLength) 
			{
				DereferenceSecondaryRequest(
					secondaryRequest
				);

				secondaryRequest = NULL;
				
				//ExReleaseFastMutex(&Secondary->FastMutex);
				KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

				returnResult = TRUE;
				break; // Maybe End of File
			}

			DereferenceSecondaryRequest(
				secondaryRequest
				);
			secondaryRequest = NULL;

			//ExReleaseFastMutex(&Secondary->FastMutex);
			KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

			returnResult = TRUE;

		} while(totalReadRequestLength < readFileIo->Length);

		secondaryRequest = NULL;
		if(returnResult != TRUE)
			break;

		fileObject->Flags |= FO_FILE_FAST_IO_READ;
		fileObject->CurrentByteOffset.QuadPart += totalReadRequestLength;

		if(totalReadRequestLength)
		{
			readFileIo->IoStatus->Information = totalReadRequestLength;
			readFileIo->IoStatus->Status = STATUS_SUCCESS;
		}
		else
		{
			readFileIo->IoStatus->Information = 0;
			readFileIo->IoStatus->Status = lastStatus;
		}

		if(fileObject->SectionObjectPointer == NULL)
			fileObject->SectionObjectPointer = &fcb->NonPaged->SectionObjectPointers;

		returnResult = TRUE;

		break;
	}
    case LFS_FILE_IO_WRITE: // 0x04
	{
		PWRITE_FILE_IO		writeFileIo = &LfsFileIo->Write;

		PVOID				inputBuffer;
		PVOID				outputBuffer = NULL;
		ULONG				outputBufferLength = 0;

		BOOLEAN				writeToEof;

		LARGE_INTEGER		currentByteOffset;

		ULONG				totalWriteRequestLength;
		IO_STATUS_BLOCK		ioStatus;
	

		if(writeFileIo->Wait == FALSE)
			return FALSE;
	
		inputBuffer			= writeFileIo->Buffer;

		ASSERT(!(writeFileIo->FileOffset->LowPart == FILE_USE_FILE_POINTER_POSITION 
						&& writeFileIo->FileOffset->HighPart == -1));

		writeToEof = ((writeFileIo->FileOffset->LowPart == FILE_WRITE_TO_END_OF_FILE) 
						&& (writeFileIo->FileOffset->HighPart == -1) );

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE, ("WRITE: Offset:%I64d Length:%d\n", 
			writeFileIo->FileOffset->QuadPart, writeFileIo->Length)) ;

		if(writeToEof)
		{
			currentByteOffset.LowPart = FILE_USE_FILE_POINTER_POSITION;
			currentByteOffset.HighPart = -1;
		}
		else
			currentByteOffset.QuadPart = writeFileIo->FileOffset->QuadPart;

		ioStatus.Information = 0;
		totalWriteRequestLength = 0;
		
		do
		{
			ULONG						inputBufferLength;
			_U8							*ndfsWinxpRequestData;
	
			PNDFS_REQUEST_HEADER		ndfsRequestHeader;
			PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	
			LARGE_INTEGER				timeOut;
			NTSTATUS					waitStatus;
			PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
			

//			inputBufferLength = write->Length;
			inputBufferLength = (writeFileIo->Length-totalWriteRequestLength <= Secondary->Thread.SessionContext.PrimaryMaxDataSize) 
								? (writeFileIo->Length-totalWriteRequestLength) : Secondary->Thread.SessionContext.PrimaryMaxDataSize;

			secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
								Secondary,
								IRP_MJ_WRITE,
								inputBufferLength
								);

			if(secondaryRequest == NULL)
			{
				ASSERT(LFS_REQUIRED);
				
				returnResult = FALSE;
				break;
			}
				
			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
			INITIALIZE_NDFS_REQUEST_HEADER(
				ndfsRequestHeader,
				NDFS_COMMAND_EXECUTE,
				Secondary,
				IRP_MJ_WRITE,
				inputBufferLength
				);

			ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);

			ndfsWinxpRequestHeader->IrpTag				= (_U32)LfsFileIo;
			ndfsWinxpRequestHeader->IrpMajorFunction	= IRP_MJ_WRITE;
			ndfsWinxpRequestHeader->IrpMinorFunction	= 0;
			ndfsWinxpRequestHeader->FileHandle			= fileExt->PrimaryFileHandle;
			ndfsWinxpRequestHeader->IrpFlags			= 0;
			ndfsWinxpRequestHeader->IrpSpFlags			= 0;

			ndfsWinxpRequestHeader->Write.Length	= inputBufferLength;
			ndfsWinxpRequestHeader->Write.Key		= writeFileIo->LockKey;
			
			if(writeToEof)
				ndfsWinxpRequestHeader->Write.ByteOffset = currentByteOffset.QuadPart;
			else
				ndfsWinxpRequestHeader->Write.ByteOffset = currentByteOffset.QuadPart+totalWriteRequestLength;

			ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);
			if(inputBufferLength)
				RtlCopyMemory(
					ndfsWinxpRequestData,
					(PUCHAR)inputBuffer + totalWriteRequestLength,
					inputBufferLength
					);

			{														
				KIRQL		oldIrql;																									
				NTSTATUS	waitStatus;
															
				//ExAcquireFastMutex(&Secondary->FastMutex);
				waitStatus = KeWaitForSingleObject( 
								&Secondary->Semaphore,
								Executive,
								KernelMode,
								FALSE,
								NULL
								);
				ASSERT(waitStatus == STATUS_SUCCESS);

				//ExAcquireFastMutex(&Secondary->FastMutex);																											
				//KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

				KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
																											
				if(Secondary->Flags & SECONDARY_ERROR
					|| Secondary->SessionId != secondaryRequest->SessionId
					|| fileExt->Corrupted == TRUE)
				{						
					DereferenceSecondaryRequest(			
						secondaryRequest	
						);				
					secondaryRequest = NULL;									
																				
					KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);		

					//ExReleaseFastMutex(&Secondary->FastMutex);				
					KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);
		
					return FALSE;
				}											
				KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
			}

			secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
			QueueingSecondaryRequest(
				Secondary,
				secondaryRequest
				);
				
			timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
#if DBG
			if(KeGetCurrentIrql() == APC_LEVEL) 
			{
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
					("PrimaryAgentThreadProc: WRITE: IRLQ is APC! going to sleep.\n"));
			}
#endif
			waitStatus = KeWaitForSingleObject(
								&secondaryRequest->CompleteEvent,
								Executive,
								KernelMode,
								FALSE,
								&timeOut
								);

			KeClearEvent(&secondaryRequest->CompleteEvent);

#if DBG
			if(KeGetCurrentIrql() == APC_LEVEL) 
			{
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
					("PrimaryAgentThreadProc: WRITE:  IRLQ is APC! going to sleep.\n"));
			}
#endif

			if(waitStatus != STATUS_SUCCESS) 
			{
				ASSERT(LFS_BUG);
				secondaryRequest = NULL;
				returnResult = FALSE;	
				
				//ExReleaseFastMutex(&Secondary->FastMutex);	
				KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

				break;
			}

			if(secondaryRequest->ExecuteStatus != STATUS_SUCCESS)
			{
				DereferenceSecondaryRequest(
					secondaryRequest
					);
				secondaryRequest = NULL;
				returnResult = FALSE;
				
				//ExReleaseFastMutex(&Secondary->FastMutex);	
				KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

				break;
			}
				
			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
			
			if(ndfsWinxpReplytHeader->Status != STATUS_SUCCESS)
			{
				if(ioStatus.Information) // already read
				{
					ioStatus.Status = STATUS_SUCCESS;
				} 
				else
				{
					ioStatus.Status = ndfsWinxpReplytHeader->Status;
					ASSERT(ndfsWinxpReplytHeader->Information == 0);
					ioStatus.Information = 0;
				}
				
				DereferenceSecondaryRequest(
					secondaryRequest
					);
				
				secondaryRequest = NULL;
				returnResult = TRUE;
				
				//ExReleaseFastMutex(&Secondary->FastMutex) ;
				KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

				break;
			}


			ASSERT(ndfsWinxpReplytHeader->Information == inputBufferLength);
			
			ioStatus.Information += ndfsWinxpReplytHeader->Information;
			ioStatus.Status	= STATUS_SUCCESS;
			returnResult = TRUE;

			DereferenceSecondaryRequest(
				secondaryRequest
				);
			secondaryRequest = NULL;

			totalWriteRequestLength += inputBufferLength;

			if(totalWriteRequestLength != ioStatus.Information)
			{
				ASSERT(LFS_UNEXPECTED);
				
				//ExReleaseFastMutex(&Secondary->FastMutex);
				KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

				returnResult = TRUE;
				break; // Write Failed
			}

			//ExReleaseFastMutex(&Secondary->FastMutex);
			KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

		} while(totalWriteRequestLength < writeFileIo->Length);

		secondaryRequest = NULL;
		if(returnResult != TRUE)
			break;


		if(ioStatus.Status == STATUS_SUCCESS)
		{    
			fileObject->CurrentByteOffset.QuadPart += ioStatus.Information;
		}

		writeFileIo->IoStatus->Status		= ioStatus.Status;
		writeFileIo->IoStatus->Information	= ioStatus.Information;
			
		if(fileObject->SectionObjectPointer == NULL)
			fileObject->SectionObjectPointer = &fcb->NonPaged->SectionObjectPointers;

		returnResult = TRUE;
		break;
	}
	case LFS_FILE_IO_QUERY_BASIC_INFO:
	{
		PQUERY_BASIC_INFO_FILE_IO	queryBasicInfo = &LfsFileIo->QueryBasicInfo;
		struct QueryFile			queryFile;

		PVOID						inputBuffer = NULL;
		ULONG						inputBufferLength = 0;
		PVOID						outputBuffer;
		ULONG						outputBufferLength;

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;

		LARGE_INTEGER				timeOut;
		NTSTATUS					waitStatus;
		PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
		_U32						returnedDataSize;
		

		if(queryBasicInfo->Wait == FALSE)
			return FALSE;

		queryFile.FileInformationClass	= FileBasicInformation;
		queryFile.Length				= sizeof(FILE_BASIC_INFORMATION);

		outputBuffer					= queryBasicInfo->Buffer;
		outputBufferLength				= sizeof(FILE_BASIC_INFORMATION);


		ASSERT(outputBufferLength <= Secondary->Thread.SessionContext.SecondaryMaxDataSize);
		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
								Secondary,
								IRP_MJ_QUERY_INFORMATION,
								outputBufferLength
								);

		if(secondaryRequest == NULL)
		{
			returnResult = FALSE;
			break;
		}

		secondaryRequest->OutputBuffer = outputBuffer;
		secondaryRequest->OutputBufferLength = outputBufferLength;
	
		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(
				ndfsRequestHeader,
				NDFS_COMMAND_EXECUTE,
				Secondary,
				IRP_MJ_QUERY_INFORMATION,
				0
				);

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);

		ndfsWinxpRequestHeader->IrpTag				= (_U32)LfsFileIo;
		ndfsWinxpRequestHeader->IrpMajorFunction	= IRP_MJ_QUERY_INFORMATION;
		ndfsWinxpRequestHeader->IrpMinorFunction	= 0;
		ndfsWinxpRequestHeader->FileHandle			= fileExt->PrimaryFileHandle;
		ndfsWinxpRequestHeader->IrpFlags			= 0;
		ndfsWinxpRequestHeader->IrpSpFlags			= 0;

		ndfsWinxpRequestHeader->QueryFile.Length				= outputBufferLength;
		ndfsWinxpRequestHeader->QueryFile.FileInformationClass	= queryFile.FileInformationClass;

		{														
			KIRQL		oldIrql;										
			NTSTATUS	waitStatus;
															
				//ExAcquireFastMutex(&Secondary->FastMutex);
			waitStatus = KeWaitForSingleObject( 
								&Secondary->Semaphore,
								Executive,
								KernelMode,
								FALSE,
								NULL
								);
			ASSERT(waitStatus == STATUS_SUCCESS);
			//ExAcquireFastMutex(&Secondary->FastMutex);																											
			//KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

			KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
																											
			if(Secondary->Flags & SECONDARY_ERROR
				|| Secondary->SessionId != secondaryRequest->SessionId
				|| fileExt->Corrupted == TRUE)
			{						
				DereferenceSecondaryRequest(			
					secondaryRequest	
					);				
				secondaryRequest = NULL;									
																			
				KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);		
				
				//ExReleaseFastMutex(&Secondary->FastMutex);				
				KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);
		
				return FALSE;
			}											
			KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
		}
	
		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest(
			Secondary,
			secondaryRequest
			);
			
		timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
		waitStatus = KeWaitForSingleObject(
							&secondaryRequest->CompleteEvent,
							Executive,
							KernelMode,
							FALSE,
							&timeOut
							);

		KeClearEvent(&secondaryRequest->CompleteEvent);

		if(waitStatus != STATUS_SUCCESS) 
		{
			ASSERT(LFS_BUG);

			secondaryRequest = NULL;
			returnResult = FALSE;	
			
			//ExReleaseFastMutex(&Secondary->FastMutex) ;
			KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

			break;
		}

		if(secondaryRequest->ExecuteStatus != STATUS_SUCCESS)
		{
			DereferenceSecondaryRequest(
				secondaryRequest
				);
			secondaryRequest = NULL;		
			
			//ExReleaseFastMutex(&Secondary->FastMutex) ;
			KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

			returnResult = FALSE;	
			break;
		}

		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

		queryBasicInfo->IoStatus->Status		= ndfsWinxpReplytHeader->Status;
		queryBasicInfo->IoStatus->Information	= ndfsWinxpReplytHeader->Information; 

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
				("RedirectIrp, IRP_MJ_QUERY_EA: Irp->IoStatus.Status = %d, Irp->IoStatus.Information = %d\n",
					queryBasicInfo->IoStatus->Status, queryBasicInfo->IoStatus->Information));

		returnedDataSize = secondaryRequest->NdfsReplyHeader.MessageSize - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER);

		if(returnedDataSize)
		{
			ASSERT(queryBasicInfo->IoStatus->Status == STATUS_SUCCESS 
				|| queryBasicInfo->IoStatus->Status == STATUS_BUFFER_OVERFLOW);
		
			if(queryBasicInfo->IoStatus->Status == STATUS_SUCCESS)
				ASSERT(ADD_ALIGN8(returnedDataSize) == ADD_ALIGN8(ndfsWinxpReplytHeader->Information));
				
			ASSERT(queryBasicInfo->IoStatus->Information <= outputBufferLength);
			ASSERT(outputBuffer);
		
			RtlCopyMemory(
				outputBuffer,
				(_U8 *)(ndfsWinxpReplytHeader+1),
				queryBasicInfo->IoStatus->Information
				);
		}
		
		DereferenceSecondaryRequest(
			secondaryRequest
			);
		
		secondaryRequest = NULL;
		returnResult = TRUE;
		
		//ExReleaseFastMutex(&Secondary->FastMutex);
		KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

		break;
	}
	case LFS_FILE_IO_QUERY_STANDARD_INFO:
	{
		PQUERY_STANDARD_INFO_FILE_IO	queryStandardInfo = &LfsFileIo->QueryStandardInfo;
		struct QueryFile				queryFile;

		PVOID						inputBuffer = NULL;
		ULONG						inputBufferLength = 0;
		PVOID						outputBuffer;
		ULONG						outputBufferLength;

		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;

		LARGE_INTEGER				timeOut;
		NTSTATUS					waitStatus;
		PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
		_U32						returnedDataSize;
		

		if(queryStandardInfo->Wait == FALSE)
			return FALSE;

		queryFile.FileInformationClass	= FileStandardInformation;
		queryFile.Length				= sizeof(FILE_STANDARD_INFORMATION);

		outputBuffer					= queryStandardInfo->Buffer;
		outputBufferLength				= sizeof(FILE_STANDARD_INFORMATION);


		ASSERT(outputBufferLength <= Secondary->Thread.SessionContext.SecondaryMaxDataSize);
		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
								Secondary,
								IRP_MJ_QUERY_INFORMATION,
								outputBufferLength
								);

		if(secondaryRequest == NULL)
		{
			returnResult = FALSE;
			break;
		}

		secondaryRequest->OutputBuffer = outputBuffer;
		secondaryRequest->OutputBufferLength = outputBufferLength;
	
		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		INITIALIZE_NDFS_REQUEST_HEADER(
				ndfsRequestHeader,
				NDFS_COMMAND_EXECUTE,
				Secondary,
				IRP_MJ_QUERY_INFORMATION,
				0
				);

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);

		ndfsWinxpRequestHeader->IrpTag				= (_U32)LfsFileIo;
		ndfsWinxpRequestHeader->IrpMajorFunction	= IRP_MJ_QUERY_INFORMATION;
		ndfsWinxpRequestHeader->IrpMinorFunction	= 0;
		ndfsWinxpRequestHeader->FileHandle			= fileExt->PrimaryFileHandle;
		ndfsWinxpRequestHeader->IrpFlags			= 0;
		ndfsWinxpRequestHeader->IrpSpFlags			= 0;

		ndfsWinxpRequestHeader->QueryFile.Length				= outputBufferLength;
		ndfsWinxpRequestHeader->QueryFile.FileInformationClass	= queryFile.FileInformationClass;

		{														
			KIRQL oldIrql;										
			NTSTATUS	waitStatus;
															
			//ExAcquireFastMutex(&Secondary->FastMutex);
			waitStatus = KeWaitForSingleObject( 
								&Secondary->Semaphore,
								Executive,
								KernelMode,
								FALSE,
								NULL
								);
			ASSERT(waitStatus == STATUS_SUCCESS);
															
			//ExAcquireFastMutex(&Secondary->FastMutex);																											
			//KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

			KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
																											
			if(Secondary->Flags & SECONDARY_ERROR
				|| Secondary->SessionId != secondaryRequest->SessionId
				|| fileExt->Corrupted == TRUE)
			{						
				DereferenceSecondaryRequest(			
					secondaryRequest	
					);				
				secondaryRequest = NULL;									
																			
				KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);		

				//ExReleaseFastMutex(&Secondary->FastMutex);				
				KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);
		
				return FALSE;
			}											
			KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
		}
	
		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest(
			Secondary,
			secondaryRequest
			);
			
		timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
		waitStatus = KeWaitForSingleObject(
							&secondaryRequest->CompleteEvent,
							Executive,
							KernelMode,
							FALSE,
							&timeOut
							);

		KeClearEvent(&secondaryRequest->CompleteEvent);

		if(waitStatus != STATUS_SUCCESS) 
		{
			ASSERT(LFS_BUG);

			secondaryRequest = NULL;
			returnResult = FALSE;	
			
			//ExReleaseFastMutex(&Secondary->FastMutex);
			KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);
	
			break;
		}

		if(secondaryRequest->ExecuteStatus != STATUS_SUCCESS)
		{
			DereferenceSecondaryRequest(
				secondaryRequest
				);
			secondaryRequest = NULL;		
			
			//ExReleaseFastMutex(&Secondary->FastMutex) ;
			KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

			returnResult = FALSE;	
			break;
		}

		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

		queryStandardInfo->IoStatus->Status		= ndfsWinxpReplytHeader->Status;
		queryStandardInfo->IoStatus->Information = ndfsWinxpReplytHeader->Information; 

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
				("RedirectIrp, IRP_MJ_QUERY_EA: Irp->IoStatus.Status = %d, Irp->IoStatus.Information = %d\n",
					queryStandardInfo->IoStatus->Status, queryStandardInfo->IoStatus->Information));

		returnedDataSize = secondaryRequest->NdfsReplyHeader.MessageSize - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER);

		if(returnedDataSize)
		{
			ASSERT(queryStandardInfo->IoStatus->Status == STATUS_SUCCESS 
				|| queryStandardInfo->IoStatus->Status == STATUS_BUFFER_OVERFLOW);
		
			if(queryStandardInfo->IoStatus->Status == STATUS_SUCCESS)
				ASSERT(ADD_ALIGN8(returnedDataSize) == ADD_ALIGN8(ndfsWinxpReplytHeader->Information));
				
			ASSERT(queryStandardInfo->IoStatus->Information <= outputBufferLength);
			ASSERT(outputBuffer);
		
			RtlCopyMemory(
				outputBuffer,
				(_U8 *)(ndfsWinxpReplytHeader+1),
				queryStandardInfo->IoStatus->Information
				);
		}
		
		DereferenceSecondaryRequest(
			secondaryRequest
			);
		
		secondaryRequest = NULL;
		returnResult = TRUE;
		
		//ExReleaseFastMutex(&Secondary->FastMutex);
		KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);

		break;
	}
	default:

		returnResult = FALSE;
		break;
	}

	return returnResult;
}


BOOLEAN
RecoverySession(
	IN  PSECONDARY	Secondary
	)
{
	BOOLEAN					result;
	NETDISK_PARTITION_INFO	netdiskPartitionInfo;

	LIST_ENTRY				tempRequestQueue;
	PLIST_ENTRY				secondaryRequestEntry;
	_U16					mid;
	_U16					previousRequestPerSession = Secondary->Thread.SessionContext.RequestsPerSession;
	_U16					queuedRequestCount = 0;

	ULONG					reconnectionTry;
    PLIST_ENTRY				fileExtlistEntry;

	KIRQL					oldIrql;


	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
		("RecoverSession Called Secondary = %p\n", Secondary));

	ASSERT(Secondary->ThreadHandle);

	RtlCopyMemory(
		&netdiskPartitionInfo.NetDiskAddress,
		&Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetDiskInformation.NetDiskAddress,
		sizeof(netdiskPartitionInfo.NetDiskAddress)
		);
	netdiskPartitionInfo.UnitDiskNo = Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetDiskInformation.UnitDiskNo;
	netdiskPartitionInfo.StartingOffset = Secondary->LfsDeviceExt->NetdiskPartitionInformation.PartitionInformation.StartingOffset;

	InitializeListHead(&tempRequestQueue);

	for(mid=0; mid < Secondary->Thread.SessionContext.RequestsPerSession; mid++)
	{
		if(Secondary->Thread.ProcessingSecondaryRequest[mid] != NULL)
		{
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,			
				("RecoverSession: insert to tempRequestQueue\n"));
		
			InsertHeadList(&tempRequestQueue, &Secondary->Thread.ProcessingSecondaryRequest[mid]->ListEntry);	
			Secondary->Thread.ProcessingSecondaryRequest[mid] = NULL;
			queuedRequestCount++;
		}
	}

	while(secondaryRequestEntry = 
					ExInterlockedRemoveHeadList(
								&Secondary->Thread.RequestQueue,
								&Secondary->Thread.RequestQSpinLock
								))
	{
		//ASSERT(LFS_BUG);
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,			
			("RecoverSession: insert to tempRequestQueue\n"));
		InsertHeadList(&tempRequestQueue, secondaryRequestEntry);
		queuedRequestCount++;
	}

	result = FALSE;

#define MAX_RECONNECTION_TRY	60

	for(reconnectionTry=0; reconnectionTry<MAX_RECONNECTION_TRY; reconnectionTry++) 
	{	
		LARGE_INTEGER		timeOut;
		NTSTATUS			waitStatus;
		NTSTATUS			tableStatus;
		OBJECT_ATTRIBUTES	objectAttributes;


		if(GlobalLfs.ShutdownOccured == TRUE)
		{
			return FALSE;
		}

		if(BooleanFlagOn(Secondary->LfsDeviceExt->Flags, LFS_DEVICE_STOP))
		{
			return FALSE;
		}

		if(Secondary->ThreadHandle)
		{
			ASSERT(Secondary->ThreadObject);
		
			timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
			waitStatus = KeWaitForSingleObject(
							Secondary->ThreadObject,
							Executive,
							KernelMode,
							FALSE,
							&timeOut
							) ;

			if(waitStatus == STATUS_SUCCESS)
			{
				_U16		mid;

				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
							("Secondary_Stop: thread stoped\n"));
				ObDereferenceObject(Secondary->ThreadObject) ;
		
				Secondary->ThreadHandle = NULL;
				Secondary->ThreadObject = NULL;

				Secondary->Thread.Flags = 0;
				Secondary->Thread.ConnectionStatus = 0;

				KeInitializeEvent(&Secondary->Thread.ReadyEvent, NotificationEvent, FALSE);
				KeInitializeEvent(&Secondary->Thread.RequestEvent, NotificationEvent, FALSE);
				
				Secondary->Thread.WaitReceive = 0;
				for(mid=0; mid < MAX_REQUEST_PER_SESSION; mid++)
				{
					if(Secondary->Thread.ProcessingSecondaryRequest[mid] == NULL)
						break;
				}
				
				RtlZeroMemory(
					&Secondary->Thread.SessionContext,
					sizeof(Secondary->Thread.SessionContext)
					);

				Secondary->Thread.TdiReceiveContext.Irp = NULL;
				KeInitializeEvent(&Secondary->Thread.TdiReceiveContext.CompletionEvent, NotificationEvent, FALSE);

				Secondary->Thread.SessionContext.PrimaryMaxDataSize = DEFAULT_MAX_DATA_SIZE;
				Secondary->Thread.SessionContext.SecondaryMaxDataSize = DEFAULT_MAX_DATA_SIZE;
			}
			else
			{
				ASSERT(LFS_BUG);
				return FALSE;
			}
		}

		if(Secondary->LfsDeviceExt->PurgeVolumeSafe == TRUE)
		{
//			if(reconnectionTry >= 2) // first and second lookup primary address
			{
				LfsDeviceExt_SecondaryToPrimary(Secondary->LfsDeviceExt);
			}
		}

		LfsTable_CleanCachePrimaryAddress(
						GlobalLfs.LfsTable,
						&netdiskPartitionInfo,
						&Secondary->PrimaryAddress
						);

		tableStatus = LfsTable_QueryPrimaryAddress(
							GlobalLfs.LfsTable,
							&netdiskPartitionInfo,
							&Secondary->PrimaryAddress
							);

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
				("RecoverSession: LfsTable_QueryPrimaryAddress tableStatus = %X\n", tableStatus));

		if(tableStatus == STATUS_SUCCESS)
		{
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
				("LFS: RecoverySession: Found PrimaryAddress :%02x:%02x:%02x:%02x:%02x:%02x/%d\n",
					Secondary->PrimaryAddress.Node[0],
					Secondary->PrimaryAddress.Node[1],
					Secondary->PrimaryAddress.Node[2],
					Secondary->PrimaryAddress.Node[3],
					Secondary->PrimaryAddress.Node[4],
					Secondary->PrimaryAddress.Node[5],
					NTOHS(Secondary->PrimaryAddress.Port)
					));
			if(Lfs_IsLocalAddress(&Secondary->PrimaryAddress)
				&& Secondary->LfsDeviceExt->SecondaryState == SECONDARY_STATE) // not yet purged
			{
				// another volume changed to primary and this volume didn't purge yet 
				result = FALSE;

				if(Secondary->LfsDeviceExt->PurgeVolumeSafe != TRUE)
				{
					ASSERT(LFS_REQUIRED);
					break;
				}
			
				continue;
			}
		} 
		else
		{
			result = FALSE;
			continue;
		}	
		
		KeInitializeEvent(&Secondary->Thread.ReadyEvent, NotificationEvent, FALSE) ;
		KeInitializeEvent(&Secondary->Thread.RequestEvent, NotificationEvent, FALSE) ;

		InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

		waitStatus = PsCreateSystemThread(
						&Secondary->ThreadHandle,
						THREAD_ALL_ACCESS,
						&objectAttributes,
						NULL,
						NULL,
						SecondaryThreadProc,
						Secondary
		);

		if(!NT_SUCCESS(waitStatus)) 
		{
			ASSERT(LFS_UNEXPECTED);
			result = FALSE;
			break;
		}

		waitStatus = ObReferenceObjectByHandle(
							Secondary->ThreadHandle,
							FILE_READ_DATA,
							NULL,
							KernelMode,
							&Secondary->ThreadObject,
							NULL
							);

		if(!NT_SUCCESS(waitStatus)) 
		{
			ASSERT(LFS_INSUFFICIENT_RESOURCES);
			result = FALSE;
			break;
		}

		timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
		waitStatus = KeWaitForSingleObject(
						&Secondary->Thread.ReadyEvent,
						Executive,
						KernelMode,
						FALSE,
						&timeOut
						);

		if(waitStatus != STATUS_SUCCESS) 
		{
			ASSERT(LFS_BUG);
			result = FALSE;
			break;
		}

		KeClearEvent(&Secondary->Thread.ReadyEvent);

		InterlockedIncrement(&Secondary->SessionId);	

		KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
		if(BooleanFlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_UNCONNECTED)) 
		{
			KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);

			if(Secondary->Thread.ConnectionStatus == STATUS_FILE_CORRUPT_ERROR)
			{
				result = FALSE;
				break;
			}

			continue;
		} 
		else 
		{
			KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
		}

		ASSERT(BooleanFlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_CONNECTED) && !BooleanFlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_ERROR));

		result = TRUE;
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
						("RecoverySession Success Secondary = %p\n", Secondary));

		break;
	}

	if(result == FALSE)
	{
		//
		//	if failed to recover the connection to a primary.
		//
		while(!IsListEmpty(&tempRequestQueue))
		{	
			secondaryRequestEntry = 
					RemoveHeadList(
								&tempRequestQueue
								);
		
			ASSERT(LFS_BUG);
			ExInterlockedInsertTailList(
				&Secondary->Thread.RequestQueue, 
				secondaryRequestEntry,
				&Secondary->Thread.RequestQSpinLock
				);
		}
		return result;
	}
	
	//
	//	if recovering the connection to a primary is successful,
	//	try to reopen files to continue I/O.
	//

    for (fileExtlistEntry = Secondary->FileExtQueue.Blink;
         fileExtlistEntry != &Secondary->FileExtQueue;
         fileExtlistEntry = fileExtlistEntry->Blink) 
	{
		PFILE_EXTENTION				fileExt;
		ULONG						disposition;
		
		ULONG						dataSize;
		PSECONDARY_REQUEST			secondaryRequest;
		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		KIRQL						oldIrql;
		PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
		_U8							*ndfsWinxpRequestData;

		LARGE_INTEGER				timeOut;
		NTSTATUS					waitStatus;
		PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;


		fileExt = CONTAINING_RECORD (fileExtlistEntry, FILE_EXTENTION, ListEntry);
		fileExt->Fcb->FileRecordSegmentHeaderAvail = FALSE;

		if(fileExt->CreateContext.RelatedFileHandle != 0)
		{
			PFILE_EXTENTION	relatedFileExt;

			if(fileExt->FileObject->RelatedFileObject == NULL)
			{
				ASSERT(LFS_UNEXPECTED);
				result = FALSE;	
				break;
			}

			if(fileExt->RelatedFileObjectClosed == FALSE)
			{
				relatedFileExt = Secondary_LookUpFileExtension(Secondary, fileExt->FileObject->RelatedFileObject);
				ASSERT(relatedFileExt != NULL && relatedFileExt->LfsMark == LFS_MARK);				
				ASSERT(relatedFileExt->SessionId == Secondary->SessionId); // must be already Updated

				if(relatedFileExt->Corrupted == TRUE)
				{
					fileExt->SessionId = Secondary->SessionId;
					fileExt->Corrupted = TRUE;
					//InterlockedIncrement(&fileExt->Fcb->CleanCountByCorruption);
				
					continue;
				}

				fileExt->CreateContext.RelatedFileHandle = relatedFileExt->PrimaryFileHandle;
			}
			else // relateFileObject is already closed;
			{
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
					("RecoverSession: relateFileObject is already closed\n"));
				
				fileExt->CreateContext.RelatedFileHandle = 0;
				fileExt->CreateContext.FileNameLength = fileExt->Fcb->FullFileName.Length;

				if(fileExt->Fcb->FullFileName.Length)
				{
					RtlCopyMemory(
						fileExt->Buffer + fileExt->CreateContext.EaLength,
						fileExt->Fcb->FullFileName.Buffer,
						fileExt->Fcb->FullFileName.Length
						);
				}
			}	
		}
				
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
				("RecoverSession: fileExt->Fcb->FullFileName = %wZ fileExt->FileObject->CurrentByteOffset.QuadPart = %I64d\n", 
					&fileExt->Fcb->FullFileName, fileExt->FileObject->CurrentByteOffset.QuadPart));

		if(Secondary->LfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NTFS)
		{
			dataSize = ((fileExt->CreateContext.EaLength + fileExt->CreateContext.FileNameLength) > Secondary->Thread.SessionContext.BytesPerFileRecordSegment)
						? (fileExt->CreateContext.EaLength + fileExt->CreateContext.FileNameLength) 
						  : Secondary->Thread.SessionContext.BytesPerFileRecordSegment;
		}
		else
			dataSize = fileExt->CreateContext.EaLength + fileExt->CreateContext.FileNameLength;

		secondaryRequest = ALLOC_WINXP_SECONDARY_REQUEST(
								Secondary,
								IRP_MJ_CREATE,
								dataSize
							);

		if(secondaryRequest == NULL)
		{
			result = FALSE;	
			break;
		}

		secondaryRequest->OutputBuffer = NULL;
		secondaryRequest->OutputBufferLength = 0;

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;

		KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
	
		RtlCopyMemory(ndfsRequestHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsRequestHeader->Protocol));
		ndfsRequestHeader->Command	= NDFS_COMMAND_EXECUTE;
		ndfsRequestHeader->Flags	= Secondary->Thread.SessionContext.Flags;
		ndfsRequestHeader->Uid		= Secondary->Thread.SessionContext.Uid;
		ndfsRequestHeader->Tid		= Secondary->Thread.SessionContext.Tid;
		ndfsRequestHeader->Mid		= 0;
		ndfsRequestHeader->MessageSize
			= (Secondary->Thread.SessionContext.MessageSecurity == 1)
				? sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER) + ADD_ALIGN8(fileExt->CreateContext.EaLength + fileExt->CreateContext.FileNameLength)
					: sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER) + fileExt->CreateContext.EaLength + fileExt->CreateContext.FileNameLength;

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);

		KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);

		ndfsWinxpRequestHeader->IrpTag   = (_U32)fileExt;
		ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_CREATE;
		ndfsWinxpRequestHeader->IrpMinorFunction = 0;

		ndfsWinxpRequestHeader->FileHandle = 0;

		ndfsWinxpRequestHeader->IrpFlags   = fileExt->IrpFlags;
		ndfsWinxpRequestHeader->IrpSpFlags = fileExt->IrpSpFlags;

		RtlCopyMemory(
			&ndfsWinxpRequestHeader->Create,
			&fileExt->CreateContext,
			sizeof(WINXP_REQUEST_CREATE)
			);
		
		disposition = FILE_OPEN_IF;
		ndfsWinxpRequestHeader->Create.Options &= 0x00FFFFFF;
		ndfsWinxpRequestHeader->Create.Options |= (disposition << 24);

		ndfsWinxpRequestData = (_U8 *)(ndfsWinxpRequestHeader+1);

		RtlCopyMemory(
			ndfsWinxpRequestData,
			fileExt->Buffer,
			fileExt->CreateContext.EaLength + fileExt->CreateContext.FileNameLength
			);

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest(Secondary, secondaryRequest);
				
		timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
		waitStatus = KeWaitForSingleObject(
							&secondaryRequest->CompleteEvent,
							Executive,
							KernelMode,
							FALSE,
							&timeOut
							);

		KeClearEvent(&secondaryRequest->CompleteEvent);

		if(waitStatus != STATUS_SUCCESS) 
		{
			ASSERT(LFS_BUG);

			secondaryRequest = NULL;
			result = FALSE;
			break;
		}

		if(secondaryRequest->ExecuteStatus != STATUS_SUCCESS)
		{
			DereferenceSecondaryRequest(secondaryRequest);
			
			result = FALSE;		
			break;
		}
				
		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
		
		if(ndfsWinxpReplytHeader->Status == STATUS_SUCCESS)
		{
	 		fileExt->PrimaryFileHandle = ndfsWinxpReplytHeader->Open.FileHandle;
		}
		else
		{
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,			
				("RecoverSession: fileExt->Fcb->FullFileName = %wZ Corrupted\n", &fileExt->Fcb->FullFileName));

			fileExt->Corrupted = TRUE;
		}

		fileExt->SessionId = Secondary->SessionId;

		DereferenceSecondaryRequest(secondaryRequest);
	}

	while(!IsListEmpty(&tempRequestQueue))
	{	
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
			("RecoverSession: extract from tempRequestQueue\n"));

		secondaryRequestEntry = RemoveHeadList(&tempRequestQueue);
		
		ExInterlockedInsertTailList(
			&Secondary->Thread.RequestQueue, 
			secondaryRequestEntry,
			&Secondary->Thread.RequestQSpinLock
			);
	}

	ASSERT(Secondary->SemaphoreConsumeRequiredCount == 0);

	if(result == TRUE)
	{
		if(previousRequestPerSession < Secondary->Thread.SessionContext.RequestsPerSession)
		{
			while(previousRequestPerSession < Secondary->Thread.SessionContext.RequestsPerSession)
			{
				Secondary->SemaphoreReturnCount++;
				previousRequestPerSession++;
			}
		}
		else if(previousRequestPerSession > Secondary->Thread.SessionContext.RequestsPerSession)
		{
			_U16	consumeRequireCount = previousRequestPerSession - Secondary->Thread.SessionContext.RequestsPerSession;
			
			if(consumeRequireCount <= Secondary->SemaphoreReturnCount)
			{
				Secondary->SemaphoreReturnCount -= consumeRequireCount;
			}
			else
			{
				Secondary->SemaphoreConsumeRequiredCount = consumeRequireCount - Secondary->SemaphoreReturnCount;
				Secondary->SemaphoreReturnCount = 0;
			}
		}
			
		ASSERT(queuedRequestCount + 1 + Secondary->SemaphoreReturnCount - Secondary->SemaphoreConsumeRequiredCount 
				== Secondary->Thread.SessionContext.RequestsPerSession);

		KeSetEvent(&Secondary->Thread.RequestEvent, IO_DISK_INCREMENT, FALSE);
	}

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
		("RecoverySession Completed. Secondary = %p, result = %d\n", Secondary, result));

	return result;
}


PSECONDARY_REQUEST
AllocSecondaryRequest(
	IN	PSECONDARY	Secondary,
	IN 	UINT32	MessageSize,
	IN	BOOLEAN	Synchronous
) 
{
	PSECONDARY_REQUEST	secondaryRequest;
	ULONG				requestSize ;
	KIRQL				oldIrql;
	

	requestSize = FIELD_OFFSET(SECONDARY_REQUEST, NdfsMessage) + MessageSize + MEMORY_CHECK_SIZE;

	secondaryRequest = ExAllocatePoolWithTag(
							NonPagedPool,
							requestSize,
							SECONDARY_MESSAGE_TAG
							);

	if(secondaryRequest == NULL) 
	{
		ASSERT(LFS_INSUFFICIENT_RESOURCES);
		return NULL ;
	}

	RtlZeroMemory(secondaryRequest, requestSize);

#if DBG
	{	
		UCHAR	i;
		ULONG	memorySize = FIELD_OFFSET(SECONDARY_REQUEST, NdfsMessage) + MessageSize;
		
		for(i=0; i<MEMORY_CHECK_SIZE; i++)
			*((_U8*)secondaryRequest + memorySize + i) = i;
	}
#endif

	secondaryRequest->ReferenceCount = 1;
	InitializeListHead(&secondaryRequest->ListEntry);

	secondaryRequest->Synchronous = Synchronous;
	KeInitializeEvent(&secondaryRequest->CompleteEvent, NotificationEvent, FALSE) ;

	KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
	secondaryRequest->SessionId = Secondary->SessionId;
	KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);

	secondaryRequest->NdfsMessageLength = MessageSize;
	
#if DBG
	InterlockedIncrement(&LfsObjectCounts.SecondaryRequestCount);
#endif

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
			("AllocSecondaryRequest: secondaryRequest = %p, SeconaryRequestFreed SecondaryRequestCount = %d\n", secondaryRequest, LfsObjectCounts.SecondaryRequestCount)) ;

	return secondaryRequest;
}


VOID
ReferenceSencondaryRequest(
	IN	PSECONDARY_REQUEST	SecondaryRequest
	) 
{
	LONG	result ;

	result = InterlockedIncrement(&SecondaryRequest->ReferenceCount) ;

	ASSERT( result > 0) ;
}


VOID
DereferenceSecondaryRequest(
	IN  PSECONDARY_REQUEST	SecondaryRequest
	)
{
	LONG	result ;


#if DBG
	{	
		UCHAR	i;
		ULONG	memorySize = FIELD_OFFSET(SECONDARY_REQUEST, NdfsMessage) + SecondaryRequest->NdfsMessageLength;
		
		for(i=0; i<MEMORY_CHECK_SIZE; i++)
			if(*((_U8*)SecondaryRequest + memorySize + i) != i)
			{
				ASSERT(LFS_BUG);
				break;
			}
	}
#endif

	result = InterlockedDecrement(&SecondaryRequest->ReferenceCount) ;

	ASSERT( result >= 0) ;

	if(0 == result)
	{
		ASSERT(SecondaryRequest->ListEntry.Flink == SecondaryRequest->ListEntry.Blink);
		ExFreePool(SecondaryRequest) ;

#if DBG
		InterlockedDecrement(&LfsObjectCounts.SecondaryRequestCount);
#endif

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
				("FreeSencondaryRequest: SecondaryRequest = %p, SeconaryRequestFreed SecondaryRequestCount = %d\n", SecondaryRequest, LfsObjectCounts.SecondaryRequestCount)) ;
	}
}


//
//	make sure to enter secondary critical section before calling.
//
FORCEINLINE
VOID
QueueingSecondaryRequest(
	IN	PSECONDARY			Secondary,
	IN	PSECONDARY_REQUEST	SecondaryRequest
	)
{


	ASSERT(SecondaryRequest->ListEntry.Flink == SecondaryRequest->ListEntry.Blink);

	ExInterlockedInsertTailList(
		&Secondary->Thread.RequestQueue,
		&SecondaryRequest->ListEntry,
		&Secondary->Thread.RequestQSpinLock
		);

	KeSetEvent(&Secondary->Thread.RequestEvent, IO_DISK_INCREMENT, FALSE) ;
}


PLFS_FCB
AllocateFcb (
	IN	PSECONDARY		Secondary,
	IN PUNICODE_STRING	FullFileName,
	IN  ULONG			BufferLength
    )
{
    PLFS_FCB fcb;


	UNREFERENCED_PARAMETER(Secondary);
	
 
    fcb = FsRtlAllocatePoolWithTag( 
				NonPagedPool,
                sizeof(LFS_FCB) - sizeof(CHAR) + BufferLength,
                LFS_FCB_TAG 
				);
	
	RtlZeroMemory(fcb, sizeof(LFS_FCB) - sizeof(CHAR) + BufferLength);

	//
	//	set NodeTypeCode to avoid BSOD when calling XxxxAcquireFileForCcFlush()
	//
	if(Secondary->LfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_FAT) 
	{
			fcb->Header.NodeTypeCode = FAT_NTC_FCB ;
	}

    fcb->NonPaged = LfsAllocateNonPagedFcb();

    RtlZeroMemory(fcb->NonPaged, sizeof(NON_PAGED_FCB));

	fcb->Header.IsFastIoPossible = FastIoIsPossible ;
    fcb->Header.Resource = LfsAllocateResource();
	fcb->Header.PagingIoResource = NULL; //fcb->Header.Resource;

	ExInitializeFastMutex(&fcb->NonPaged->AdvancedFcbHeaderMutex);
#if WINVER >= 0x0501
	if(IS_WINDOWSXP_OR_LATER())
	{
    FsRtlSetupAdvancedHeader( 
					&fcb->Header, 
                    &fcb->NonPaged->AdvancedFcbHeaderMutex);

	}
#endif
    FsRtlInitializeFileLock(&fcb->FileLock, NULL, NULL);

	fcb->ReferenceCount = 1;
	InitializeListHead(&fcb->ListEntry);

    RtlInitEmptyUnicodeString( 
				&fcb->FullFileName,
                fcb->FullFileNameBuffer,
                sizeof(fcb->FullFileNameBuffer) 
				);

	RtlCopyUnicodeString(
		&fcb->FullFileName,
		FullFileName
		);

    RtlInitEmptyUnicodeString( 
				&fcb->CaseInSensitiveFullFileName,
                fcb->CaseInSensitiveFullFileNameBuffer,
                sizeof(fcb->CaseInSensitiveFullFileNameBuffer) 
				);

	RtlDowncaseUnicodeString(
		&fcb->CaseInSensitiveFullFileName,
		&fcb->FullFileName,
		FALSE);

	if(FullFileName->Length)
	if(FullFileName->Buffer[0] != L'\\')
		ASSERT(LFS_BUG);
	
#if DBG
	InterlockedIncrement(&LfsObjectCounts.FcbCount);
#endif

	return fcb;
}


VOID
Secondary_DereferenceFcb (
	IN	PLFS_FCB	Fcb
   )
{
	LONG		result;


	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
			("Secondary_DereferenceFcb: Fcb->OpenCount = %d, Fcb->UncleanCount = %d\n", Fcb->OpenCount, Fcb->UncleanCount));

	ASSERT(Fcb->OpenCount >= Fcb->UncleanCount);
	result = InterlockedDecrement(&Fcb->ReferenceCount);

	ASSERT( result >= 0);

	if(0 == result)
	{
		ASSERT(Fcb->ListEntry.Flink == Fcb->ListEntry.Blink);
		ASSERT(Fcb->OpenCount == 0);
		
	    LfsFreeResource(Fcb->Header.Resource);
		LfsFreeNonPagedFcb(Fcb->NonPaged);
	    ExFreePool(Fcb);	
#if DBG
		InterlockedDecrement(&LfsObjectCounts.FcbCount);
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
	WCHAR			caseInSensitiveFullFileNameBuffer[NDFS_MAX_PATH];
	NTSTATUS		downcaseStatus;


	ASSERT(FullFileName->Length <= NDFS_MAX_PATH*sizeof(WCHAR));

	if(CaseInSensitive == TRUE)
	{
		//ASSERT(Secondary->LfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NTFS);

		RtlInitEmptyUnicodeString( 
				&caseInSensitiveFullFileName,
                caseInSensitiveFullFileNameBuffer,
                sizeof(caseInSensitiveFullFileNameBuffer) 
				);
		downcaseStatus = RtlDowncaseUnicodeString(
							&caseInSensitiveFullFileName,
							FullFileName,
							FALSE
							);
	
		if(downcaseStatus != STATUS_SUCCESS)
		{
			ASSERT(LFS_UNEXPECTED);
			return NULL;
		}
	}

	KeAcquireSpinLock(&Secondary->FcbQSpinLock, &oldIrql);

    for (listEntry = Secondary->FcbQueue.Flink;
         listEntry != &Secondary->FcbQueue;
         listEntry = listEntry->Flink) 
	{
		fcb = CONTAINING_RECORD (listEntry, LFS_FCB, ListEntry);
		if(fcb->FullFileName.Length != FullFileName->Length)
		{
			fcb = NULL;
			continue;
		}
		if(CaseInSensitive == TRUE)
		{
			if(RtlEqualMemory(
				fcb->CaseInSensitiveFullFileName.Buffer, 
				caseInSensitiveFullFileName.Buffer, 
				fcb->CaseInSensitiveFullFileName.Length))
		{
			InterlockedIncrement(&fcb->ReferenceCount);
			break;
		}
		}
		else
		{
			if(RtlEqualMemory(
				fcb->FullFileName.Buffer, 
				FullFileName->Buffer,
				fcb->FullFileName.Length))
			{
				InterlockedIncrement(&fcb->ReferenceCount);
				break;
			}
		}

		fcb = NULL;
	}

	KeReleaseSpinLock(&Secondary->FcbQSpinLock, oldIrql);

	return fcb;
}


PFILE_EXTENTION
AllocateFileExt(
	IN	PSECONDARY		Secondary,
	IN	PFILE_OBJECT	FileObject,
	IN  ULONG			BufferLength
	) 
{
	PFILE_EXTENTION	fileExt ;
	KIRQL			oldIrql;


	fileExt = ExAllocatePoolWithTag( 
						NonPagedPool, 
                        sizeof(FILE_EXTENTION) - sizeof(_U8) + BufferLength + MEMORY_CHECK_SIZE,
						FILE_EXT_TAG
						);
	
	if (fileExt == NULL)
	{
		ASSERT(LFS_INSUFFICIENT_RESOURCES);
		return NULL;
	}

	RtlZeroMemory(
		fileExt,
        sizeof(FILE_EXTENTION) - sizeof(_U8) + BufferLength
		);
	
#if DBG
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

	InitializeListHead(&fileExt->ListEntry) ;

	fileExt->BufferLength = BufferLength;

	KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
	fileExt->SessionId = Secondary->SessionId;
	KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);

#if DBG
	InterlockedIncrement(&LfsObjectCounts.FileExtCount);
#endif

	InterlockedIncrement(&Secondary->FileExtCount);

	return fileExt;
}


VOID
FreeFileExt(
	IN  PSECONDARY		Secondary,
	IN  PFILE_EXTENTION	FileExt
	)
{
	PLIST_ENTRY		listEntry;


	ASSERT(FileExt->ListEntry.Flink == FileExt->ListEntry.Blink);

#if DBG
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

	ExAcquireFastMutex(&Secondary->FileExtQMutex);

    for (listEntry = Secondary->FileExtQueue.Flink;
         listEntry != &Secondary->FileExtQueue;
         listEntry = listEntry->Flink) 
	{
		PFILE_EXTENTION	childFileExt;
		
		childFileExt = CONTAINING_RECORD (listEntry, FILE_EXTENTION, ListEntry);
        
		if(childFileExt->CreateContext.RelatedFileHandle == FileExt->PrimaryFileHandle)
			childFileExt->RelatedFileObjectClosed = TRUE;
	}

    ExReleaseFastMutex(&Secondary->FileExtQMutex);

	InterlockedDecrement(&Secondary->FileExtCount);

	ExFreePoolWithTag(
		FileExt,
		FILE_EXT_TAG
		);
#if DBG
	InterlockedDecrement(&LfsObjectCounts.FileExtCount);
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


	referenceStatus = ObReferenceObjectByHandle(
									FileHandle,
									FILE_READ_DATA,
									0,
									KernelMode,
									&fileObject,
									NULL
									);

    if(referenceStatus != STATUS_SUCCESS)
	{
		return NULL;
	}
	
	ObDereferenceObject(fileObject);

	return Secondary_LookUpFileExtension(Secondary, fileObject);
}

	
PFILE_EXTENTION
Secondary_LookUpFileExtension(
	IN PSECONDARY	Secondary,
	IN PFILE_OBJECT	FileObject
	)
{
	PFILE_EXTENTION	fileExt = NULL;
    PLIST_ENTRY		listEntry;

	
    ExAcquireFastMutex(&Secondary->FileExtQMutex);

    for (listEntry = Secondary->FileExtQueue.Flink;
         listEntry != &Secondary->FileExtQueue;
         listEntry = listEntry->Flink) 
	{
		 fileExt = CONTAINING_RECORD (listEntry, FILE_EXTENTION, ListEntry);
         if(fileExt->FileObject == FileObject)
			break;

		fileExt = NULL;
	}

    ExReleaseFastMutex(&Secondary->FileExtQMutex);

	return fileExt;
}