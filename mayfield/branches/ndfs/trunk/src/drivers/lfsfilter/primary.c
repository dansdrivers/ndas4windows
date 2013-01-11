#define	__NETDISK_MANAGER__
#define	__PRIMARY__
#include "LfsProc.h"
#include "netdiskmanager.h"

VOID
Primary_AcceptConnection(
	IN PPRIMARY			Primary,
	IN HANDLE			ListenFileHandle,
	IN PFILE_OBJECT		ListenFileObject,
	IN  ULONG			ListenSocketIndex,
	IN PLPX_ADDRESS		RemoteAddress
	);


PPRIMARY_AGENT_REQUEST
AllocPrimaryAgentRequest(
	IN	BOOLEAN	Synchronous
);


VOID
DereferencePrimaryAgentRequest(
	IN	PPRIMARY_AGENT_REQUEST	PrimaryAgentRequest
	);


FORCEINLINE
VOID
QueueingPrimaryAgentRequest(
	IN 	PPRIMARY	Primary,
	IN	PPRIMARY_AGENT_REQUEST	PrimaryAgentRequest
	);


VOID
PrimaryAgentThreadProc(
	IN 	PPRIMARY	Primary
	);


NTSTATUS
BindListenSockets(
	IN 	PPRIMARY	Primary
	);


VOID
CloseListenSockets(
	IN 	PPRIMARY	Primary,
	IN  BOOLEAN		OnlyConnection
	);


NTSTATUS
MakeConnectionObject(
    HANDLE			AddressFileHandle,
    PFILE_OBJECT	AddressFileObject,
	HANDLE			*ListenFileHandle,
	PFILE_OBJECT	*ListenFileObject
);


VOID
Primary_NetEvtCallback(
		PSOCKETLPX_ADDRESS_LIST	Original,
		PSOCKETLPX_ADDRESS_LIST	Updated,
		PSOCKETLPX_ADDRESS_LIST	Disabled,
		PSOCKETLPX_ADDRESS_LIST	Enabled,
		PVOID					Context
	) 
{
	PPRIMARY					Primary = (PPRIMARY)Context;
	PPRIMARY_AGENT_REQUEST		primaryAgentRequest;

#if DBG
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE, ("Original Address List\n"));
	DbgPrintLpxAddrList(LFS_DEBUG_PRIMARY_NOISE, Original);
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE, ("Updated Address List\n"));
	DbgPrintLpxAddrList(LFS_DEBUG_PRIMARY_NOISE, Updated);
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE, ("Disabled Address List\n"));
	DbgPrintLpxAddrList(LFS_DEBUG_PRIMARY_NOISE, Disabled);
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE, ("Enabled Address List\n"));
	DbgPrintLpxAddrList(LFS_DEBUG_PRIMARY_NOISE, Enabled);
#else
	UNREFERENCED_PARAMETER(Original);
#endif

	if(Disabled->iAddressCount) {
		primaryAgentRequest = AllocPrimaryAgentRequest(FALSE);
		primaryAgentRequest->RequestType = PRIMARY_AGENT_REQ_NIC_DISABLED;
		RtlCopyMemory(&primaryAgentRequest->AddressList, Disabled, sizeof(SOCKETLPX_ADDRESS_LIST));

		QueueingPrimaryAgentRequest(
			Primary,
			primaryAgentRequest
		);
	}

	if(Enabled->iAddressCount) 
	{
		primaryAgentRequest = AllocPrimaryAgentRequest(FALSE);
		primaryAgentRequest->RequestType = PRIMARY_AGENT_REQ_NIC_ENABLED;
		RtlCopyMemory(&primaryAgentRequest->AddressList, Enabled, sizeof(SOCKETLPX_ADDRESS_LIST));

		QueueingPrimaryAgentRequest(
			Primary,
			primaryAgentRequest
		);
	} else {

		//
		//	Patch.
		//	Update all open addresses periodically
		//	in case of failure of address open.
		//

		if(Updated->iAddressCount) {
			primaryAgentRequest = AllocPrimaryAgentRequest(FALSE);
			primaryAgentRequest->RequestType = PRIMARY_AGENT_REQ_NIC_ENABLED;
			RtlCopyMemory(&primaryAgentRequest->AddressList, Updated, sizeof(SOCKETLPX_ADDRESS_LIST));

			QueueingPrimaryAgentRequest(
				Primary,
				primaryAgentRequest
				);
		}
	}
}


PPRIMARY
Primary_Create (
	IN PLFS	Lfs
	)
{
	PPRIMARY			primary;
 	OBJECT_ATTRIBUTES	objectAttributes;
	LONG				i;
	NTSTATUS			ntStatus;
	LARGE_INTEGER		timeOut;


	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
		("Primary_Create: Entered\n"));

	LfsReference (Lfs);

	primary = ExAllocatePoolWithTag( 
						NonPagedPool, 
                        sizeof(PRIMARY),
						LFS_ALLOC_TAG
						);
	
	if (primary == NULL)
	{
		ASSERT(LFS_INSUFFICIENT_RESOURCES);
		LfsDereference (Lfs);

		return NULL;
	}

	RtlZeroMemory(
		primary,
		sizeof(PRIMARY)
		);

	KeInitializeSpinLock(&primary->SpinLock);	
	primary->ReferenceCount	= 1;

	primary->Lfs = Lfs;

//	InitializeListHead(&primary->LfsDeviceExtQueue);
//	KeInitializeSpinLock(&primary->LfsDeviceExtQSpinLock);

	for(i=0; i<MAX_SOCKETLPX_INTERFACE; i++)
	{
		InitializeListHead(&primary->PrimarySessionQueue[i]);
		KeInitializeSpinLock(&primary->PrimarySessionQSpinLock[i]);
	}

	primary->Agent.ThreadHandle = 0;
	primary->Agent.ThreadObject = NULL;

	primary->Agent.Flags = 0;

	KeInitializeEvent(&primary->Agent.ReadyEvent, NotificationEvent, FALSE);

	InitializeListHead(&primary->Agent.RequestQueue);
	KeInitializeSpinLock(&primary->Agent.RequestQSpinLock);
	KeInitializeEvent(&primary->Agent.RequestEvent, NotificationEvent, FALSE);

	
	primary->Agent.ListenPort = DEFAULT_PRIMARY_PORT;
	primary->Agent.ActiveListenSocketCount = 0;

	
	InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

	ntStatus = PsCreateSystemThread(
					&primary->Agent.ThreadHandle,
					THREAD_ALL_ACCESS,
					&objectAttributes,
					NULL,
					NULL,
					PrimaryAgentThreadProc,
					primary
					);
	
	if(!NT_SUCCESS(ntStatus)) 
	{
		ASSERT(LFS_UNEXPECTED);
		Primary_Close(primary);
		
		return NULL;
	}

	ntStatus = ObReferenceObjectByHandle(
					primary->Agent.ThreadHandle,
					FILE_READ_DATA,
					NULL,
					KernelMode,
					&primary->Agent.ThreadObject,
					NULL
					);

	if(!NT_SUCCESS(ntStatus)) 
	{
		ASSERT(LFS_INSUFFICIENT_RESOURCES);
		Primary_Close(primary);
		
		return NULL;
	}

	timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
	ntStatus = KeWaitForSingleObject(
					&primary->Agent.ReadyEvent,
					Executive,
					KernelMode,
					FALSE,
					&timeOut
					);

	ASSERT(ntStatus == STATUS_SUCCESS);

	KeClearEvent(&primary->Agent.ReadyEvent);

	if(ntStatus != STATUS_SUCCESS) 
	{
		ASSERT(LFS_BUG);
		Primary_Close(primary);
		
		return NULL;
	}
	
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
		("Primary_Create: primary = %p\n", primary));

	return primary;
}


VOID
Primary_Close (
	IN PPRIMARY	Primary
	)
{
	ULONG		listenSocketIndex;
	

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
		("Primary_Close: Entered primary = %p\n", Primary));

	for(listenSocketIndex=0; listenSocketIndex<MAX_SOCKETLPX_INTERFACE; listenSocketIndex++)
	{
	    PLIST_ENTRY	primarySessionListEntry;
		BOOLEAN		found   = FALSE;


		if(Primary->Agent.ListenSocket[listenSocketIndex].Active != TRUE)
			continue;
		
		while(primarySessionListEntry = 
				ExInterlockedRemoveHeadList(
						&Primary->PrimarySessionQueue[listenSocketIndex],
						&Primary->PrimarySessionQSpinLock[listenSocketIndex]
						)
			) 
		{
			PPRIMARY_SESSION primarySession;
		 
			primarySession = CONTAINING_RECORD (primarySessionListEntry, PRIMARY_SESSION, ListEntry);
			InitializeListHead(primarySessionListEntry);
			PrimarySession_Close(primarySession);
		}
	}

	if(Primary->Agent.ThreadHandle == NULL)
	{
		ASSERT(LFS_BUG);
		Primary_Dereference(Primary);

		return;
	}

	ASSERT(Primary->Agent.ThreadObject != NULL);

	if(Primary->Agent.Flags & PRIMARY_AGENT_TERMINATED)
	{
		ObDereferenceObject(Primary->Agent.ThreadObject);

		Primary->Agent.ThreadHandle = NULL;
		Primary->Agent.ThreadObject = NULL;

	} else
	{
		PPRIMARY_AGENT_REQUEST		primaryAgentRequest;
		NTSTATUS					ntStatus;
		LARGE_INTEGER				timeOut;
	
		
		primaryAgentRequest = AllocPrimaryAgentRequest(FALSE);
		primaryAgentRequest->RequestType = PRIMARY_AGENT_REQ_DISCONNECT;

		QueueingPrimaryAgentRequest(
			Primary,
			primaryAgentRequest
			);

		primaryAgentRequest = AllocPrimaryAgentRequest (FALSE);
		primaryAgentRequest->RequestType = PRIMARY_AGENT_REQ_DOWN;

		QueueingPrimaryAgentRequest(
			Primary,
			primaryAgentRequest
			);

		timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
		ntStatus = KeWaitForSingleObject(
						Primary->Agent.ThreadObject,
						Executive,
						KernelMode,
						FALSE,
						&timeOut
						);

		ASSERT(ntStatus == STATUS_SUCCESS);

		if(ntStatus == STATUS_SUCCESS) 
		{
		    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
							("Primary_Close: thread stoped\n"));

			ObDereferenceObject(Primary->Agent.ThreadObject);

			Primary->Agent.ThreadHandle = NULL;
			Primary->Agent.ThreadObject = NULL;
		
		} else
		{
			ASSERT(LFS_BUG);
			return;
		}
	}

	Primary_Dereference(Primary);

	return;
}


VOID
Primary_FileSystemShutdown (
	IN PPRIMARY	Primary
	)
{
	ULONG		listenSocketIndex;


	Primary_Reference(Primary);

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_ERROR,
		("Primary_FileSystemShutdown: Entered primary = %p\n", Primary));

	do 
	{
		if(Primary->Agent.ThreadHandle == NULL)
		{
			ASSERT(LFS_BUG);
			break;
		}

		ASSERT(Primary->Agent.ThreadObject != NULL);

		if(Primary->Agent.Flags & PRIMARY_AGENT_TERMINATED)
		{
			break;
		} 
		else
		{
			PPRIMARY_AGENT_REQUEST	primaryAgentRequest;
			NTSTATUS				ntStatus;
			LARGE_INTEGER			timeOut;
	
		
			primaryAgentRequest = AllocPrimaryAgentRequest(TRUE);
			primaryAgentRequest->RequestType = PRIMARY_AGENT_REQ_SHUTDOWN;

			QueueingPrimaryAgentRequest(
				Primary,
				primaryAgentRequest
				);

			timeOut.QuadPart = - LFS_TIME_OUT;		// 10 sec
			ntStatus = KeWaitForSingleObject(
							&primaryAgentRequest->CompleteEvent,
							Executive,
							KernelMode,
							FALSE,
							&timeOut
							);

			ASSERT(ntStatus == STATUS_SUCCESS);

			KeClearEvent(&primaryAgentRequest->CompleteEvent);

			if(ntStatus == STATUS_SUCCESS) 
			{
			    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
								("Primary_FileSystemShutdown: thread shutdown\n"));
			} 
			else
			{
				ASSERT(LFS_BUG);
				break;
			}
		}
	}while(0);
	
	for(listenSocketIndex=0; listenSocketIndex<MAX_SOCKETLPX_INTERFACE; listenSocketIndex++)
	{
		if(Primary->Agent.ListenSocket[listenSocketIndex].Active != TRUE)
			continue;
		

		while(1) 
		{	
			KIRQL		oldIrql;
			BOOLEAN		found;
		    PLIST_ENTRY	primarySessionListEntry;

	
			KeAcquireSpinLock(&Primary->PrimarySessionQSpinLock[listenSocketIndex], &oldIrql);
			found = FALSE;

			for (primarySessionListEntry = Primary->PrimarySessionQueue[listenSocketIndex].Flink;
				 primarySessionListEntry != &Primary->PrimarySessionQueue[listenSocketIndex];
				 primarySessionListEntry = primarySessionListEntry->Flink)
			{		
				PPRIMARY_SESSION primarySession;
		 
				primarySession = CONTAINING_RECORD (primarySessionListEntry, PRIMARY_SESSION, ListEntry);
		 
				//if(primarySession->LfsDeviceExt && primarySession->LfsDeviceExt->FileSystemType == FileSystemType)
				//{
					RemoveEntryList(primarySessionListEntry);
				    KeReleaseSpinLock(&Primary->PrimarySessionQSpinLock[listenSocketIndex], oldIrql);
					
					InitializeListHead(primarySessionListEntry);
					PrimarySession_FileSystemShutdown(primarySession);

					found = TRUE;
					break;			
				//}
			}
			
			if(found == FALSE)
			{
				KeReleaseSpinLock(&Primary->PrimarySessionQSpinLock[listenSocketIndex], oldIrql);
				break;
			}
		} 
	}

	Primary_Dereference(Primary);

	return;
}


VOID
Primary_Reference (
	IN PPRIMARY	Primary
	)
{
    LONG result;
	
    result = InterlockedIncrement (&Primary->ReferenceCount);

    ASSERT (result >= 0);
}


VOID
Primary_Dereference (
	IN PPRIMARY	Primary
	)
{
    LONG result;


    result = InterlockedDecrement (&Primary->ReferenceCount);
    ASSERT (result >= 0);

    if (result == 0) 
	{
		int i;


		LfsDereference (Primary->Lfs);

		//ASSERT(Primary->LfsDeviceExtQueue.Flink == &Primary->LfsDeviceExtQueue);
		for(i=0; i<MAX_SOCKETLPX_INTERFACE; i++)
			ASSERT(Primary->PrimarySessionQueue[i].Flink == &Primary->PrimarySessionQueue[i]);

		ExFreePoolWithTag(
			Primary,
			LFS_ALLOC_TAG
			);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
				("Primary_Dereference: Primary is Freed Primary = %p\n", Primary));
	}
}

#if 0

NTSTATUS
Primary_FsControlMountVolumeComplete
(
	IN PPRIMARY					Primary,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	)
{
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
			("Primary_FsControlMountVolumeComplete: LfsDeviceExt = %p, Primary = %p, LfsDeviceExt->LocalNetDiskPartitionInfo.NetDiskPartitionInfo.StartingOffset = %I64x\n", 
			LfsDeviceExt, Primary, LfsDeviceExt->NetdiskPartitionInformation.PartitionInformation.StartingOffset));
	
	LfsDeviceExt_Reference (
		LfsDeviceExt
		);

	ExInterlockedInsertHeadList(				// must be attached to head
		&Primary->LfsDeviceExtQueue,
		&LfsDeviceExt->PrimaryQListEntry,
		&Primary->LfsDeviceExtQSpinLock
		);	

	return STATUS_SUCCESS;
}


VOID
Primary_CleanupMountedDevice(
	IN PPRIMARY					Primary,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	)
{
	KIRQL			oldIrql;
	
		
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
				("Primary_CleanupMountedDevice: LfsDeviceExt = %p\n", 
					LfsDeviceExt));

	KeAcquireSpinLock(&Primary->LfsDeviceExtQSpinLock, &oldIrql);
	RemoveEntryList(&LfsDeviceExt->PrimaryQListEntry);
	KeReleaseSpinLock(&Primary->LfsDeviceExtQSpinLock, oldIrql);

	InitializeListHead(&LfsDeviceExt->PrimaryQListEntry);

	LfsDeviceExt_Dereference (
		LfsDeviceExt
		);

	return;
}

#endif
#if 0

PLFS_DEVICE_EXTENSION
Primary_LookUpVolume(
	PPRIMARY				Primary, 
	PPRIMARY_SESSION		PrimarySession, 
	PNETDISK_PARTITION_INFO	NetDiskPartitionInfo
	)
{
	PLFS_DEVICE_EXTENSION			lfsDeviceExt = NULL;


	UNREFERENCED_PARAMETER(Primary);
	UNREFERENCED_PARAMETER(PrimarySession);
	
	lfsDeviceExt = MountManager_LookupPrimaryVolume(
								GlobalLfs.NetdiskManager,
								&NetDiskPartitionInfo->NetDiskAddress,
								NetDiskPartitionInfo->UnitDiskNo,
								&NetDiskPartitionInfo->StartingOffset
								);

	return lfsDeviceExt;
}

#endif


BOOLEAN
Primary_PassThrough(
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt,
	IN  PIRP					Irp,
	OUT PNTSTATUS				NtStatus
	)
{
	PIO_STACK_LOCATION  irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT		fileObject;
	KIRQL				oldIrql;


	//	Do not allow exclusive access to the volume when connected to secondaries.

	if (irpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL && 
		(irpSp->MinorFunction == IRP_MN_USER_FS_REQUEST || irpSp->MinorFunction == IRP_MN_KERNEL_CALL)) {

		struct FileSystemControl	*fileSystemControl =(struct FileSystemControl *)&(irpSp->Parameters.FileSystemControl);

		fileObject = irpSp->FileObject;

		//
		//	Do not allow exclusive access to the volume and dismount volume to protect format
		//	We allow exclusive access if secondaries are connected locally.
		//

		if ((fileSystemControl->FsControlCode == FSCTL_LOCK_VOLUME	||			// 6
			 fileSystemControl->FsControlCode == FSCTL_DISMOUNT_VOLUME)	// 8
			 && 
			 NetdiskManager_ThisVolumeHasSecondary(GlobalLfs.NetdiskManager, LfsDeviceExt->EnabledNetdisk, LfsDeviceExt, FALSE)) {

			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
						   ("Primary_PassThrough, IRP_MJ_FILE_SYSTEM_CONTROL: Trying to acquire the volume exclusively. \n") );


			ASSERT( fileObject && fileObject->FileName.Length == 0 && fileObject->RelatedFileObject == NULL ); 

			*NtStatus = Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
			Irp->IoStatus.Information = 0;
			IoCompleteRequest( Irp, IO_DISK_INCREMENT );

			return TRUE;

		//
		//	Do not support encryption.
		//

		} else if (fileSystemControl->FsControlCode == FSCTL_SET_ENCRYPTION) {

			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
						   ("Primary_PassThrough, IRP_MJ_FILE_SYSTEM_CONTROL: Setting encryption denied.\n") );

			*NtStatus = Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
			Irp->IoStatus.Information = 0;
			IoCompleteRequest( Irp, IO_DISK_INCREMENT );

			return TRUE;
		}
	}

    // Need to test much whether this is okay..
	if (irpSp->MajorFunction == IRP_MJ_PNP && 
		(irpSp->MinorFunction == IRP_MN_CANCEL_REMOVE_DEVICE || irpSp->MinorFunction == IRP_MN_REMOVE_DEVICE)) {
		
		PNETDISK_PARTITION Partition;
		
		Partition = LookUpNetdiskPartition( LfsDeviceExt->EnabledNetdisk, NULL, 0, LfsDeviceExt, NULL );
		
		if (Partition) {

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,	
						   ("%s: Releasing Unplugging lock\n", 
							(irpSp->MinorFunction == IRP_MN_CANCEL_REMOVE_DEVICE) ? "CANCEL_REMOVE_DEVICE" : "REMOVE_DEVICE") );
			
			InterlockedDecrement( &LfsDeviceExt->EnabledNetdisk->UnplugInProgressCount );

			NetdiskPartition_Dereference( Partition );		// by LookUpNetdiskPartition

		} else {

			ASSERT(0);
			
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,	
				           ("%s: Failed to find matching parition. Release unplugging lock anyway\n", 
						   (irpSp->MinorFunction == IRP_MN_CANCEL_REMOVE_DEVICE) ? "CANCEL_REMOVE_DEVICE":"REMOVE_DEVICE") );

			InterlockedDecrement( &LfsDeviceExt->EnabledNetdisk->UnplugInProgressCount );
		}
	}

	//
	// Close files opened by secondary if unmount of primary host is requested .
	// This helps primary can unmount while secondary has mounted.
	//
	if (irpSp->MajorFunction == IRP_MJ_PNP && irpSp->MinorFunction == IRP_MN_QUERY_REMOVE_DEVICE) {

		int i;
		PLIST_ENTRY	primarySessionListEntry;
		PLIST_ENTRY PrimarySessionList;
		POPEN_FILE	openFile;
	    PLIST_ENTRY	OpenFileListEntry;
	    NTSTATUS   closeStatus;

#if DBG
		int j;
		CHAR NameBuf[512] = {0};
#endif		
		PPRIMARY_SESSION primarySession;

		PNETDISK_PARTITION Partition;

		Partition = LookUpNetdiskPartition(LfsDeviceExt->EnabledNetdisk, NULL, 0, LfsDeviceExt, NULL);

		if (Partition) {

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,	("QUERY_REMOVE: Locking disks for removal\n") );
			InterlockedIncrement( &LfsDeviceExt->EnabledNetdisk->UnplugInProgressCount );
			
			// wait until DispatchInProgressCount becomes zero
			KeAcquireSpinLock( &LfsDeviceExt->EnabledNetdisk->SpinLock, &oldIrql );
			
			while (LfsDeviceExt->EnabledNetdisk->DispatchInProgressCount > 0) {

				LARGE_INTEGER Timeout;

				SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,	("QUERY_REMOVE: %d IO is in progress by secondary\n", LfsDeviceExt->EnabledNetdisk->DispatchInProgressCount));
				Timeout.QuadPart = - HZ/10; // 100ms
				KeReleaseSpinLock(&LfsDeviceExt->EnabledNetdisk->SpinLock, oldIrql);
				KeDelayExecutionThread(KernelMode, FALSE, &Timeout);
				KeAcquireSpinLock(&LfsDeviceExt->EnabledNetdisk->SpinLock, &oldIrql);
			}

			KeReleaseSpinLock(&LfsDeviceExt->EnabledNetdisk->SpinLock, oldIrql);

			NetdiskPartition_Dereference( Partition );		// by LookUpNetdiskPartition

		} else {

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,	("QUERY_REMOVE: Failed to find matching parition\n") );
		}

		for (i=0;i<MAX_SOCKETLPX_INTERFACE;i++) {
			
			PrimarySessionList = &GlobalLfs.Primary->PrimarySessionQueue[i];

			for (primarySessionListEntry = PrimarySessionList->Flink;
				 primarySessionListEntry != PrimarySessionList;
				 primarySessionListEntry = primarySessionListEntry->Flink) {

				primarySession = CONTAINING_RECORD( primarySessionListEntry, PRIMARY_SESSION, ListEntry );
				
				if (primarySession->NetdiskPartition == NULL || primarySession->NetdiskPartition->EnabledNetdisk == NULL) {
				
					SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("Primary is closed: Session=%p\n", primarySession) );
					continue;
				}
				
				//
				// Is there any way to check to get proper netdisk??
				//
				if (primarySession->NetdiskPartition->EnabledNetdisk == LfsDeviceExt->EnabledNetdisk) {

				    for (OpenFileListEntry = primarySession->OpenedFileQueue.Flink;
				         OpenFileListEntry != &primarySession->OpenedFileQueue;
				         OpenFileListEntry = OpenFileListEntry->Flink) {

						openFile = CONTAINING_RECORD( OpenFileListEntry, OPEN_FILE, ListEntry );

						// Close open files opened by secondary.
						if (openFile->AlreadyClosed == FALSE) {
							
							// Store file offset before close.
							
							if (openFile->CreateOptions & (FILE_SYNCHRONOUS_IO_ALERT | FILE_SYNCHRONOUS_IO_NONALERT)) {
							
								NTSTATUS					QueryStatus;
								IO_STATUS_BLOCK				IoStatus;
								FILE_POSITION_INFORMATION	FileInfo;
								
								// File position is meaningful only in this open mode.
								QueryStatus = ZwQueryInformationFile( openFile->FileHandle,
																	  &IoStatus, 
																	  (PVOID)&FileInfo,
																	  sizeof(FileInfo),
																	  FilePositionInformation );

								if (NT_SUCCESS(QueryStatus)) {
								
									SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
												   ("Closing secondary files: Stroring offset %x\n", FileInfo.CurrentByteOffset) );
									openFile->CurrentByteOffset = FileInfo.CurrentByteOffset;
								
								} else {

									SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
												   ("Closing secondary files: Failed to get current file offset\n") );

									openFile->CurrentByteOffset.HighPart = 0;
									openFile->CurrentByteOffset.LowPart = 0;									
								}
							}
							
							closeStatus = ZwClose( openFile->FileHandle );
							ObDereferenceObject( openFile->FileObject );
							ASSERT( openFile->EventHandle !=NULL );
							ZwClose( openFile->EventHandle );
							openFile->EventHandle = NULL;
							openFile->AlreadyClosed = TRUE;
						}
#if DBG						
						// Print of unicode is available only PASSIVE_LEVEL. 
						// So print in ASCII assuming file name is always ascii range.
						for (j=0;openFile->FullFileNameBuffer[j]!=0 && j<sizeof(NameBuf)-1;j++) {
							
							NameBuf[j]= (CHAR)openFile->FullFileNameBuffer[j];
						}

						NameBuf[j] = 0;

						SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
									   ("Closing secondary files: Session=%p, FileId=%x, FileName=%s\n",
										 primarySession, openFile->OpenFileId, NameBuf) );
#endif
					}
				}
			}
		}
	}

	 return FALSE;
}



//
// To clean up terminated PrimarySession is performed here
//
VOID
Primary_AcceptConnection(
	IN PPRIMARY			Primary,
	IN HANDLE			ListenFileHandle,
	IN PFILE_OBJECT		ListenFileObject,
	IN  ULONG			ListenSocketIndex,
	IN PLPX_ADDRESS		RemoteAddress
	)
{
    PLIST_ENTRY				listEntry;
	PPRIMARY_SESSION		newPrimarySession;
	KIRQL					oldIrql;



	newPrimarySession = PrimarySession_Create(
							Primary, 
							ListenFileHandle, 
							ListenFileObject,
							ListenSocketIndex,
							RemoteAddress
					);

	if(newPrimarySession == NULL)
	{	
		LpxTdiDisconnect(ListenFileObject, 0);
		LpxTdiDisassociateAddress(ListenFileObject);
		LpxTdiCloseConnection(
					ListenFileHandle, 
					ListenFileObject
					);
	}

		
	KeAcquireSpinLock(&Primary->SpinLock, &oldIrql);

	listEntry = Primary->PrimarySessionQueue[ListenSocketIndex].Flink;
	while(listEntry != &Primary->PrimarySessionQueue[ListenSocketIndex])
	{
		PPRIMARY_SESSION primarySession;
	

		primarySession = CONTAINING_RECORD (listEntry, PRIMARY_SESSION, ListEntry);			
		listEntry = listEntry->Flink;

		if(primarySession->ThreadFlags & PRIMARY_SESSION_THREAD_TERMINATED
			|| primarySession->State == SESSION_CLOSED)
	{
			KeReleaseSpinLock(&Primary->SpinLock, oldIrql);
			PrimarySession_Close(primarySession);
			KeAcquireSpinLock(&Primary->SpinLock, &oldIrql);
		}
	}

	KeReleaseSpinLock(&Primary->SpinLock, oldIrql);

	return;
}


static
NTSTATUS
PrimaryOpenOneListenSocket(
		PPRIMARY_LISTEN_SOCKET	listenSock,
		PLPX_ADDRESS			NICAddr
	) {
	NTSTATUS	ntStatus;
	HANDLE					addressFileHandle = NULL;
	PFILE_OBJECT			addressFileObject = NULL;
	HANDLE					listenFileHandle = NULL;
	PFILE_OBJECT			listenFileObject = NULL;
	

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
				("PrimaryOpenOneSocket: Entered\n"));

	listenSock->Active = FALSE;
	RtlCopyMemory(
			listenSock->NICAddress.Node,
			NICAddr->Node,
			ETHER_ADDR_LENGTH
		);

	//
	//	open a address.
	//
	ntStatus = LpxTdiOpenAddress(
						&addressFileHandle,
						&addressFileObject,
						NICAddr
					);

	if(!NT_SUCCESS(ntStatus)) 
	{
		ASSERT(LPX_BUG);
		return ntStatus;
	}

	listenSock->Active = TRUE;

	KeClearEvent(
			&listenSock->TdiListenContext.CompletionEvent
		);

	listenSock->AddressFileHandle = addressFileHandle;
	listenSock->AddressFileObject = addressFileObject;
		
	ntStatus = MakeConnectionObject(addressFileHandle, addressFileObject, &listenFileHandle, &listenFileObject);
	
	if(!NT_SUCCESS(ntStatus)) 
		{
			ASSERT(LPX_BUG);
			LpxTdiCloseAddress (addressFileHandle, addressFileObject);

			listenSock->Active = FALSE;
	
			return STATUS_UNSUCCESSFUL;
		}

	listenSock->ListenFileHandle = listenFileHandle;
	listenSock->ListenFileObject = listenFileObject;
		
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
				( "PrimaryOpenOneListenSocket: opened a address:'%02X:%02X:%02X:%02X:%02X:%02X 0x%04X'\n",
					NICAddr->Node[0],NICAddr->Node[1],NICAddr->Node[2],
					NICAddr->Node[3],NICAddr->Node[4],NICAddr->Node[5],
					NTOHS(NICAddr->Port)
			) );

	listenSock->Flags	= TDI_QUERY_ACCEPT;

	ntStatus = LpxTdiListenWithCompletionEvent(
					listenSock->ListenFileObject,
					&listenSock->TdiListenContext,
					&listenSock->Flags
				);

	if(!NT_SUCCESS(ntStatus)) 
	{
		ASSERT(LPX_BUG);
		LpxTdiDisassociateAddress(listenFileObject);
		LpxTdiCloseConnection(listenFileHandle, listenFileObject);
		LpxTdiCloseAddress (addressFileHandle, addressFileObject);

		listenSock->Active = FALSE;
		ntStatus = STATUS_UNSUCCESSFUL;
	}

	return ntStatus;
}


static
VOID
PrimaryAgentNICDisabled(
		PPRIMARY	Primary,
		PSOCKETLPX_ADDRESS_LIST AddressList
	) {

	LONG		idx_listen;
	LONG		idx_disabled;
	BOOLEAN		found;

	for(idx_disabled = 0; idx_disabled < AddressList->iAddressCount; idx_disabled ++ ) {

		found = FALSE;
		for(idx_listen = 0; idx_listen < MAX_SOCKETLPX_INTERFACE; idx_listen ++) {
			//
			//	find the match
			//
			if(Primary->Agent.ListenSocket[idx_listen].Active &&
				RtlCompareMemory(
					AddressList->SocketLpx[idx_disabled].LpxAddress.Node,
					Primary->Agent.ListenSocket[idx_listen].NICAddress.Node,
					ETHER_ADDR_LENGTH
					) == ETHER_ADDR_LENGTH ) {

				found = TRUE;
				break;
			}
		}

		//
		//	delete disabled one if found.
		//
		if(found) {
			PPRIMARY_LISTEN_SOCKET	listenSock = Primary->Agent.ListenSocket + idx_listen;

			listenSock->Active = FALSE;

			LpxTdiDisassociateAddress(listenSock->ListenFileObject);
			LpxTdiCloseConnection(listenSock->ListenFileHandle, listenSock->ListenFileObject);
			LpxTdiCloseAddress (listenSock->AddressFileHandle, listenSock->AddressFileObject);
			
			Primary->Agent.ActiveListenSocketCount --;

			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					("PrimaryAgentNICEnabled: A NIC deleted..\n"));
		}
	}
}


static
VOID
PrimaryAgentNICEnabled(
		PPRIMARY	Primary,
		PSOCKETLPX_ADDRESS_LIST AddressList) {

	LONG		idx_listen;
	LONG		idx_enabled;
	BOOLEAN		found;
	LONG		available;
	NTSTATUS	ntStatus;

	for(idx_enabled = 0; idx_enabled < AddressList->iAddressCount; idx_enabled ++ ) {

		found = FALSE;
		for(idx_listen = 0; idx_listen < MAX_SOCKETLPX_INTERFACE; idx_listen ++) {
			//
			//	find the match
			//
			if(Primary->Agent.ListenSocket[idx_listen].Active &&
				RtlCompareMemory(
					AddressList->SocketLpx[idx_enabled].LpxAddress.Node,
					Primary->Agent.ListenSocket[idx_listen].NICAddress.Node,
					ETHER_ADDR_LENGTH
					) == ETHER_ADDR_LENGTH ) {

				found = TRUE;
				break;
			}
		}

		//
		//	add enabled one if not found.
		//
		if(!found) {
			//
			//	find available slot.
			//
			available = -1;
			for(idx_listen = 0; idx_listen < MAX_SOCKETLPX_INTERFACE; idx_listen ++) {
				if(!Primary->Agent.ListenSocket[idx_listen].Active) {
					available = idx_listen;
					break;
				}
			}

			if(available >= 0) {
				PPRIMARY_LISTEN_SOCKET	listenSock = Primary->Agent.ListenSocket + available;
				LPX_ADDRESS				NICAddr;

				//
				//	open a new listen connection.
				//
				RtlCopyMemory(NICAddr.Node, AddressList->SocketLpx[idx_enabled].LpxAddress.Node, ETHER_ADDR_LENGTH);
				NICAddr.Port = HTONS(Primary->Agent.ListenPort);

				ntStatus = PrimaryOpenOneListenSocket(
						listenSock,
						&NICAddr
					);

				if(NT_SUCCESS(ntStatus)) {

					SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
						("PrimaryAgentNICEnabled: new NIC added.\n"));

					Primary->Agent.ActiveListenSocketCount ++;
				}

			} else {
				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					("PrimaryAgentNICEnabled: No available socket slot.\n"));
			}
		}

	}
}


VOID
PrimaryAgentThreadProc(
	IN 	PPRIMARY	Primary
	)
{
	BOOLEAN			primaryAgentThreadExit = FALSE;
	ULONG			listenSocketIndex;
	PKWAIT_BLOCK	waitBlocks;

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
				("PrimaryAgentThreadProc: Start\n"));

	Primary->Agent.Flags |= PRIMARY_AGENT_INITIALIZING;
	
	//
	// Allocate wait block
	//
	 waitBlocks = ExAllocatePool(NonPagedPool, sizeof(KWAIT_BLOCK) * MAXIMUM_WAIT_OBJECTS);
	 if(waitBlocks == NULL) {
		 ASSERT(LFS_REQUIRED);
		 PsTerminateSystemThread(STATUS_INSUFFICIENT_RESOURCES);
	 }

	for(listenSocketIndex=0; listenSocketIndex<MAX_SOCKETLPX_INTERFACE; listenSocketIndex++)
		KeInitializeEvent(
					&Primary->Agent.ListenSocket[listenSocketIndex].TdiListenContext.CompletionEvent, 
					NotificationEvent, 
					FALSE
					);

	BindListenSockets(
		Primary
		);

	Primary->Agent.Flags |= PRIMARY_AGENT_START;
				
	KeSetEvent(&Primary->Agent.ReadyEvent, IO_DISK_INCREMENT, FALSE);


	while(primaryAgentThreadExit == FALSE)
	{
		PKEVENT				events[MAXIMUM_WAIT_OBJECTS];
		LONG				eventCnt;

		ULONG				i;

		LARGE_INTEGER		timeOut;
		NTSTATUS			ntStatus;
		PLIST_ENTRY			primaryAgentRequestEntry;


		ASSERT(MAX_SOCKETLPX_INTERFACE + 1 <= MAXIMUM_WAIT_OBJECTS);

		eventCnt = 0;
		events[eventCnt++] = &Primary->Agent.RequestEvent;

		if(!BooleanFlagOn(Primary->Agent.Flags, PRIMARY_AGENT_SHUTDOWN))
		{
			for(i=0; i<MAX_SOCKETLPX_INTERFACE; i++)
			{
				if(Primary->Agent.ListenSocket[i].TdiListenContext.Irp &&
					Primary->Agent.ListenSocket[i].Active)
				{
					events[eventCnt++] = &Primary->Agent.ListenSocket[i].TdiListenContext.CompletionEvent;
				}
				else
				{
					// events[eventCnt++] = NULL; // I wanna set NULL, But It's not Work
					events[eventCnt++] = &Primary->Agent.ListenSocket[i].TdiListenContext.CompletionEvent;
				}
			}

			ASSERT(eventCnt == MAX_SOCKETLPX_INTERFACE + 1);
		}

		timeOut.QuadPart = - 5 * HZ;
		ntStatus = KeWaitForMultipleObjects(
					eventCnt,
					events,
					WaitAny,
					Executive,
					KernelMode,
					TRUE,
					&timeOut,
					waitBlocks
				);

		if(ntStatus == STATUS_TIMEOUT) 
		{
			continue;
		}

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
			("PrimaryAgentThreadProc: NTSTATUS:%lu\n", ntStatus));
		

		if(!NT_SUCCESS(ntStatus) || ntStatus >= eventCnt)
		{
			ASSERT(LFS_UNEXPECTED);
			SetFlag(Primary->Agent.Flags, PRIMARY_AGENT_ERROR);

			primaryAgentThreadExit = TRUE;

			continue;
		}

		KeClearEvent(events[ntStatus]);

		if(0 == ntStatus)
		{
			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
					("PrimaryAgentThreadProc: RequestEvent received\n"));

			while(primaryAgentRequestEntry = 
					ExInterlockedRemoveHeadList(
							&Primary->Agent.RequestQueue,
							&Primary->Agent.RequestQSpinLock
							)
				) 
			{
				PPRIMARY_AGENT_REQUEST		primaryAgentRequest;

				primaryAgentRequest = CONTAINING_RECORD(
										primaryAgentRequestEntry,
										PRIMARY_AGENT_REQUEST,
										ListEntry
										);
	
				switch(primaryAgentRequest->RequestType) 
				{

				case PRIMARY_AGENT_REQ_DISCONNECT:
				{
					CloseListenSockets(
						Primary,
						FALSE
						);
				
					break;
				}

				case PRIMARY_AGENT_REQ_SHUTDOWN:
				{
					CloseListenSockets(
						Primary,
						TRUE
						);				
					SetFlag(Primary->Agent.Flags, PRIMARY_AGENT_SHUTDOWN);
					break;
				}

				case PRIMARY_AGENT_REQ_DOWN:

					primaryAgentThreadExit = TRUE;
					break;

				//
				//	added to adapt network card changes.
				//
				case PRIMARY_AGENT_REQ_NIC_DISABLED:
				
					SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
						("PrimaryAgentThreadProc: PRIMARY_AGENT_REQ_NIC_DISABLED\n"));
					PrimaryAgentNICDisabled(Primary, &primaryAgentRequest->AddressList);
				
					break;

				case PRIMARY_AGENT_REQ_NIC_ENABLED:
				
					SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
						("PrimaryAgentThreadProc: PRIMARY_AGENT_REQ_NIC_ENABLED\n"));
					PrimaryAgentNICEnabled(Primary, &primaryAgentRequest->AddressList);
				
					break;

				default:
		
					ASSERT(LFS_BUG);
					SetFlag(Primary->Agent.Flags, PRIMARY_AGENT_ERROR);

					break;
				}

				if(primaryAgentRequest->Synchronous == TRUE)
						KeSetEvent(&primaryAgentRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE);
				else
					DereferencePrimaryAgentRequest(
						primaryAgentRequest
						);
			}

			continue;	
		}

		ASSERT(1 <= ntStatus && ntStatus < eventCnt); // LpxEvent 
		
		if(	1 <= ntStatus &&
			ntStatus <= MAX_SOCKETLPX_INTERFACE
			) // Connected 
		{
			NTSTATUS		tdiStatus;
			HANDLE			listenFileHandle;
			PFILE_OBJECT	listenFileObject;
			HANDLE			connFileHandle;
			PFILE_OBJECT	connFileObject;
			PPRIMARY_LISTEN_SOCKET	listenSocket;
			LPX_ADDRESS		remoteAddress;

			listenSocket = &Primary->Agent.ListenSocket[ntStatus-1];

			//
			//	retreive a connection file and remote address.
			//
			connFileHandle = listenSocket->ListenFileHandle;
			connFileObject = listenSocket->ListenFileObject;
			RtlCopyMemory(&remoteAddress, &listenSocket->TdiListenContext.RemoteAddress, sizeof(LPX_ADDRESS));

			listenSocket->TdiListenContext.Irp = NULL;
			listenSocket->ListenFileHandle = NULL;
			listenSocket->ListenFileObject = NULL;
			KeClearEvent(&listenSocket->TdiListenContext.CompletionEvent);

			if(!listenSocket->Active) 
			{
				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
						("ListenSocket is not active. Maybe a NIC disabled.\n"));
				continue;

			} 
			else if(listenSocket->TdiListenContext.Status != STATUS_SUCCESS) 
			{
				LpxTdiCloseAddress (
					listenSocket->AddressFileHandle, 
					listenSocket->AddressFileObject
					);

				listenSocket->Active = FALSE;
				Primary->Agent.ActiveListenSocketCount --;

				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
						("Listen IRP #%d failed.\n", ntStatus));
				continue;
			}

			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					("PrimaryAgentThreadProc: Connect from %02X:%02X:%02X:%02X:%02X:%02X 0x%4X\n",
						listenSocket->TdiListenContext.RemoteAddress.Node[0], listenSocket->TdiListenContext.RemoteAddress.Node[1],
						listenSocket->TdiListenContext.RemoteAddress.Node[2], listenSocket->TdiListenContext.RemoteAddress.Node[3],
						listenSocket->TdiListenContext.RemoteAddress.Node[4], listenSocket->TdiListenContext.RemoteAddress.Node[5],
						NTOHS(listenSocket->TdiListenContext.RemoteAddress.Port)
						));

			//
			//	Make a new listen connection first of all to get another connection.
			//	It must be earlier than start a session that takes long time.
			//	Primary cannot accept a connection before it creates a new listen object.
			//
			tdiStatus = MakeConnectionObject(
							listenSocket->AddressFileHandle,
							listenSocket->AddressFileObject,
							&listenFileHandle,
							&listenFileObject
							);

			if(!NT_SUCCESS(tdiStatus)) 
			{
				ASSERT(LPX_BUG);
				LpxTdiCloseAddress (
					listenSocket->AddressFileHandle, 
					listenSocket->AddressFileObject
					);

				listenSocket->Active = FALSE;
				Primary->Agent.ActiveListenSocketCount --;
	
				goto start_session;
			}

			listenSocket->ListenFileHandle = listenFileHandle;
			listenSocket->ListenFileObject = listenFileObject;

			listenSocket->Flags	= TDI_QUERY_ACCEPT;
			tdiStatus = LpxTdiListenWithCompletionEvent(
							listenSocket->ListenFileObject,
							&listenSocket->TdiListenContext,
							&listenSocket->Flags
							);

			if(!NT_SUCCESS(tdiStatus)) 
			{
				ASSERT(LPX_BUG);
				LpxTdiDisassociateAddress(listenFileObject);
				LpxTdiCloseConnection(listenFileHandle, listenFileObject);
				LpxTdiCloseAddress (
					listenSocket->AddressFileHandle, 
					listenSocket->AddressFileObject
					);
				listenSocket->Active = FALSE;
				Primary->Agent.ActiveListenSocketCount --;
	
			}

start_session:
			//
			//	start a session.
			//
			Primary_AcceptConnection(Primary, connFileHandle, connFileObject, ntStatus-1, &remoteAddress);
			
			continue;
		}
	}

	for(listenSocketIndex=0; listenSocketIndex<MAX_SOCKETLPX_INTERFACE; listenSocketIndex++)
		KeClearEvent(&Primary->Agent.ListenSocket[listenSocketIndex].TdiListenContext.CompletionEvent);

	//
	// Free wait blocks
	//

	ExFreePool(waitBlocks);

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
				("PrimaryAgentThreadProc: PsTerminateSystemThread\n"));
	
	Primary->Agent.Flags |= PRIMARY_AGENT_TERMINATED;

	PsTerminateSystemThread(STATUS_SUCCESS);
}


NTSTATUS
BindListenSockets(
	IN 	PPRIMARY	Primary
	)
{
	NTSTATUS				ntStatus;
	PSOCKETLPX_ADDRESS_LIST	socketLpxAddressList;
	LONG					idx_addr;


	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
				("BindListenSockets: Entered\n"));

	Primary->Agent.ActiveListenSocketCount = 0;
	socketLpxAddressList = &Primary->Agent.SocketLpxAddressList;
	ntStatus = LpxTdiGetAddressList(
		socketLpxAddressList
        );

	if(!NT_SUCCESS(ntStatus)) 
	{
		//ASSERT(LPX_BUG);
		return	ntStatus;
	}
	
	if(socketLpxAddressList->iAddressCount <= 0) 
	{
		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					( "BindListenSockets: No NICs in the host.\n") );

		return STATUS_UNSUCCESSFUL;
	}
	
	if(socketLpxAddressList->iAddressCount > MAX_SOCKETLPX_INTERFACE)
		socketLpxAddressList->iAddressCount = MAX_SOCKETLPX_INTERFACE;

	for(idx_addr = 0; idx_addr < socketLpxAddressList->iAddressCount; idx_addr ++) 
	{
		LPX_ADDRESS		NICAddr;
	
		HANDLE			addressFileHandle = NULL;
		PFILE_OBJECT	addressFileObject = NULL;

		HANDLE			listenFileHandle = NULL;
		PFILE_OBJECT	listenFileObject = NULL;
		
		
		Primary->Agent.ListenSocket[idx_addr].Active = FALSE;

		if( (0 == socketLpxAddressList->SocketLpx[idx_addr].LpxAddress.Node[0]) &&
			(0 == socketLpxAddressList->SocketLpx[idx_addr].LpxAddress.Node[1]) &&
			(0 == socketLpxAddressList->SocketLpx[idx_addr].LpxAddress.Node[2]) &&
			(0 == socketLpxAddressList->SocketLpx[idx_addr].LpxAddress.Node[3]) &&
			(0 == socketLpxAddressList->SocketLpx[idx_addr].LpxAddress.Node[4]) &&
			(0 == socketLpxAddressList->SocketLpx[idx_addr].LpxAddress.Node[5]) ) 
		{
			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
						( "BindListenSockets: We don't use SocketLpx device.\n"));
			continue;
		}

		RtlCopyMemory(
				&Primary->Agent.ListenSocket[idx_addr].NICAddress,
				&socketLpxAddressList->SocketLpx[idx_addr].LpxAddress, 
				sizeof(LPX_ADDRESS)
			);

		RtlCopyMemory(
				&NICAddr, 
				&socketLpxAddressList->SocketLpx[idx_addr].LpxAddress, 
				sizeof(LPX_ADDRESS)
			);

		NICAddr.Port = HTONS(Primary->Agent.ListenPort);
	
		//
		//	open a address.
		//
		ntStatus = LpxTdiOpenAddress(
						&addressFileHandle,
						&addressFileObject,
						&NICAddr
						);

		if(!NT_SUCCESS(ntStatus)) 
		{
			ASSERT(LPX_BUG);
			continue;
		}

		Primary->Agent.ListenSocket[idx_addr].Active = TRUE;

		KeInitializeEvent(&Primary->Agent.ListenSocket[idx_addr].TdiListenContext.CompletionEvent, NotificationEvent, FALSE);

		Primary->Agent.ListenSocket[idx_addr].AddressFileHandle = addressFileHandle;
		Primary->Agent.ListenSocket[idx_addr].AddressFileObject = addressFileObject;
		
		Primary->Agent.ActiveListenSocketCount ++;

		ntStatus = MakeConnectionObject(addressFileHandle, addressFileObject, &listenFileHandle, &listenFileObject);
	
		if(!NT_SUCCESS(ntStatus)) 
		{
			ASSERT(LPX_BUG);
			LpxTdiCloseAddress (addressFileHandle, addressFileObject);

			Primary->Agent.ListenSocket[idx_addr].Active = FALSE;
			Primary->Agent.ActiveListenSocketCount --;
	
			continue;
		}

		Primary->Agent.ListenSocket[idx_addr].ListenFileHandle = listenFileHandle;
		Primary->Agent.ListenSocket[idx_addr].ListenFileObject = listenFileObject;

		
		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					( "BindListenSockets: opened a address:'%02X:%02X:%02X:%02X:%02X:%02X 0x%04X'\n",
						NICAddr.Node[0],NICAddr.Node[1],NICAddr.Node[2],
						NICAddr.Node[3],NICAddr.Node[4],NICAddr.Node[5],
						NTOHS(NICAddr.Port)
						) );

		Primary->Agent.ListenSocket[idx_addr].Flags	= TDI_QUERY_ACCEPT;

		ntStatus = LpxTdiListenWithCompletionEvent(
						Primary->Agent.ListenSocket[idx_addr].ListenFileObject,
						&Primary->Agent.ListenSocket[idx_addr].TdiListenContext,
						&Primary->Agent.ListenSocket[idx_addr].Flags
						);

		if(!NT_SUCCESS(ntStatus)) 
		{
			ASSERT(LPX_BUG);
			LpxTdiDisassociateAddress(listenFileObject);
			LpxTdiCloseConnection(listenFileHandle, listenFileObject);
			LpxTdiCloseAddress (addressFileHandle, addressFileObject);

			Primary->Agent.ListenSocket[idx_addr].Active = FALSE;
			Primary->Agent.ActiveListenSocketCount --;
	
			continue;
		}
	}

	return STATUS_SUCCESS;
}


VOID
CloseListenSockets(
	IN 	PPRIMARY	Primary,
	IN  BOOLEAN		OnlyConnection
	)
{
	ULONG	i;

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
				("CloseListenSockets: Entered\n"));

	for(i=0; i < MAX_SOCKETLPX_INTERFACE; i++)
	{
		if(Primary->Agent.ListenSocket[i].Active != TRUE)
			continue;

		ASSERT(Primary->Agent.ListenSocket[i].AddressFileObject != NULL);
		ASSERT(Primary->Agent.ListenSocket[i].ListenFileObject != NULL);

		LpxTdiDisconnect(Primary->Agent.ListenSocket[i].ListenFileObject, 0);
		LpxTdiDisassociateAddress(Primary->Agent.ListenSocket[i].ListenFileObject);
		LpxTdiCloseConnection(
					Primary->Agent.ListenSocket[i].ListenFileHandle, 
					Primary->Agent.ListenSocket[i].ListenFileObject
					);
		
		if(OnlyConnection == FALSE)
		{
			LpxTdiCloseAddress (
				Primary->Agent.ListenSocket[i].AddressFileHandle,
				Primary->Agent.ListenSocket[i].AddressFileObject
				);

			Primary->Agent.ListenSocket[i].AddressFileObject = NULL;
			Primary->Agent.ListenSocket[i].AddressFileHandle = NULL;

			Primary->Agent.ListenSocket[i].Active = FALSE;		
			Primary->Agent.ActiveListenSocketCount --;
		}

	}

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
				("CloseListenSockets: Returned\n"));

	return;
}

				
NTSTATUS
MakeConnectionObject(
    HANDLE			AddressFileHandle,
    PFILE_OBJECT	AddressFileObject,
	HANDLE			*ListenFileHandle,
	PFILE_OBJECT	*ListenFileObject
)
{
	HANDLE			listenFileHandle;
	PFILE_OBJECT	listenFileObject;
	NTSTATUS		ntStatus;


	UNREFERENCED_PARAMETER(AddressFileObject);

	ntStatus = LpxTdiOpenConnection(
					&listenFileHandle,
					&listenFileObject,
					NULL
					);

	if(!NT_SUCCESS(ntStatus)) 
	{
		ASSERT(LPX_BUG);

		*ListenFileHandle = NULL;
		*ListenFileObject = NULL;
		
		return ntStatus;
	}

	ntStatus = LpxTdiAssociateAddress(
					listenFileObject,
					AddressFileHandle
					);

	if(!NT_SUCCESS(ntStatus)) 
	{
		ASSERT(LPX_BUG);

		LpxTdiCloseConnection(listenFileHandle, listenFileObject);

		*ListenFileHandle = NULL;
		*ListenFileObject = NULL;
		
		return ntStatus;
	}

	*ListenFileHandle = listenFileHandle;
	*ListenFileObject = listenFileObject;

	return ntStatus;
}


PPRIMARY_AGENT_REQUEST
AllocPrimaryAgentRequest(
	IN	BOOLEAN	Synchronous
) 
{
	PPRIMARY_AGENT_REQUEST	primaryAgentRequest;


	primaryAgentRequest = ExAllocatePoolWithTag(
							NonPagedPool,
							sizeof(PRIMARY_AGENT_REQUEST),
							PRIMARY_AGENT_MESSAGE_TAG
							);

	if(primaryAgentRequest == NULL) 
	{
		ASSERT(LFS_INSUFFICIENT_RESOURCES);
		return NULL;
	}

	RtlZeroMemory(primaryAgentRequest, sizeof(PRIMARY_AGENT_REQUEST));

	(primaryAgentRequest)->ReferenceCount = 1;
	InitializeListHead(&primaryAgentRequest->ListEntry);
	
	primaryAgentRequest->Synchronous = Synchronous;
	KeInitializeEvent(&primaryAgentRequest->CompleteEvent, NotificationEvent, FALSE);

	return primaryAgentRequest;
}


VOID
DereferencePrimaryAgentRequest(
	IN	PPRIMARY_AGENT_REQUEST	PrimaryAgentRequest
	) 
{
	LONG	result;

	result = InterlockedDecrement(&PrimaryAgentRequest->ReferenceCount);

	ASSERT(result >= 0);

	if(0 == result)
	{
		ExFreePoolWithTag(
			PrimaryAgentRequest,
			PRIMARY_AGENT_MESSAGE_TAG
			);

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
				("DereferencePrimaryAgentRequest: PrimaryAgentRequest freed\n"));
	}	
}

FORCEINLINE
VOID
QueueingPrimaryAgentRequest(
	IN 	PPRIMARY				Primary,
	IN	PPRIMARY_AGENT_REQUEST	PrimaryAgentRequest
	)
{
	ExInterlockedInsertTailList(
		&Primary->Agent.RequestQueue,
		&PrimaryAgentRequest->ListEntry,
		&Primary->Agent.RequestQSpinLock
		);

	KeSetEvent(&Primary->Agent.RequestEvent, IO_DISK_INCREMENT, FALSE);
	
	return;
}