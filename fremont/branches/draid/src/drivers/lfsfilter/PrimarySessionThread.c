#include "LfsProc.h"


VOID
PrimarySessionThreadProc(
	IN PPRIMARY_SESSION PrimarySession
	)
{
	BOOLEAN		primarySessionTerminate = FALSE;
	NTSTATUS	status;
	_U16		slotIndex;
	PLIST_ENTRY	primarySessionRequestEntry;


	ASSERT( SESSION_SLOT_COUNT == 1 );
	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO, ("PrimarySessionThreadProc: Start PrimarySession = %p\n", PrimarySession) );
	
	PrimarySession_Reference( PrimarySession );

	SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_INITIALIZING );
	
	InitializeListHead( &PrimarySession->Thread.OpenedFileQueue );
	KeInitializeSpinLock( &PrimarySession->Thread.OpenedFileQSpinLock );

	KeInitializeEvent( &PrimarySession->Thread.WorkCompletionEvent, NotificationEvent, FALSE );

	PrimarySession->Thread.SessionSlotCount = SESSION_SLOT_COUNT;

	for (slotIndex = 0; slotIndex < PrimarySession->Thread.SessionSlotCount; slotIndex ++) {

		PrimarySession->Thread.SessionSlot[slotIndex].State = SLOT_WAIT;
		PrimarySession->Thread.IdleSlotCount ++;
	}

	ClearFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_INITIALIZING );
	SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_START );
	SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_CONNECTED );

	KeSetEvent( &PrimarySession->ReadyEvent, IO_DISK_INCREMENT, FALSE );

	status = LpxTdiRecvWithCompletionEvent( PrimarySession->ConnectionFileObject,
										    &PrimarySession->Thread.TdiReceiveContext,
										    (PUCHAR)&PrimarySession->Thread.NdfsRequestHeader,
										    sizeof(NDFS_REQUEST_HEADER),
										    0,
										    NULL,
										    NULL );

	if (NT_SUCCESS(status)) {

		PrimarySession->Thread.TdiReceiving = TRUE;
	
	} else {
	
		ASSERT( LFS_BUG );
		SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_ERROR );
		primarySessionTerminate = TRUE;
	} 

	while (primarySessionTerminate == FALSE) {

		PKEVENT				events[3];
		LONG				eventCount;
		NTSTATUS			eventStatus;
		LARGE_INTEGER		timeOut;


		ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

		eventCount = 0;

		events[eventCount++] = &PrimarySession->RequestEvent;

		if (!FlagOn(PrimarySession->Flags, PRIMARY_SESSION_FLAG_STOPPING)) {

			events[eventCount++] = &PrimarySession->Thread.WorkCompletionEvent;

			if (FlagOn(PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_SHUTDOWN)) {
			
				if (PrimarySession->Thread.IdleSlotCount == PrimarySession->Thread.SessionSlotCount) {

					CloseOpenFiles( PrimarySession, TRUE );
				
					KeSetEvent( &PrimarySession->Thread.ShutdownPrimarySessionRequest->CompleteEvent, 
								IO_DISK_INCREMENT, 
								FALSE );

					SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_SHUTDOWN_WAIT );
					ClearFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_SHUTDOWN );
				}
			}	 
		
			if (!FlagOn(PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_SHUTDOWN_WAIT)) {

				if (PrimarySession->Thread.TdiReceiving == TRUE) {

					ASSERT( PrimarySession->Thread.IdleSlotCount != 0 );
					events[eventCount++] = &PrimarySession->Thread.TdiReceiveContext.CompletionEvent;
				}
			}

			ASSERT( eventCount <= THREAD_WAIT_OBJECTS );
		}

		timeOut.QuadPart = -5*HZ;
		eventStatus = KeWaitForMultipleObjects( eventCount, 
												events, 
												WaitAny, 
												Executive, 
												KernelMode,
												TRUE,
												&timeOut,
												NULL );

		if (!FlagOn(PrimarySession->Flags, PRIMARY_SESSION_FLAG_STOPPING)) {

			if (eventStatus == STATUS_TIMEOUT || eventStatus == 2) {
			
				if (PrimarySession->Thread.SessionState == SESSION_TREE_CONNECT) {
				
					ASSERT( PrimarySession->NetdiskPartition );
				
					if (!(PrimarySession->NetdiskPartition->NetdiskVolume[NETDISK_PRIMARY].VolumeState == VolumeMounted || 
						  PrimarySession->NetdiskPartition->NetdiskVolume[NETDISK_SECONDARY2PRIMARY].VolumeState == VolumeMounted)) {

						SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
									   ("Netdisk Volume is unmounted\n") );

						if (!FlagOn(PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_SHUTDOWN_WAIT))
							primarySessionTerminate = TRUE;
		
						continue;
					}
				}
			}
		}

		if (eventStatus == STATUS_TIMEOUT) {

			continue;
		}

		if (!NT_SUCCESS(eventStatus) || eventStatus >= eventCount) {

			DbgBreakPoint();
			SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_ERROR );
			primarySessionTerminate = TRUE;
			continue;
		}
		
		KeClearEvent( events[eventStatus] );

		if (eventStatus == 0) {

			while (primarySessionRequestEntry = ExInterlockedRemoveHeadList( &PrimarySession->RequestQueue,
																			 &PrimarySession->RequestQSpinLock)) {

				PPRIMARY_SESSION_REQUEST	primarySessionRequest;
			
				primarySessionRequest = CONTAINING_RECORD( primarySessionRequestEntry,
														   PRIMARY_SESSION_REQUEST,
														   ListEntry );

				if (primarySessionRequest->RequestType == PRIMARY_SESSION_REQ_DISCONNECT) {

					SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO, ("PRIMARY_SESSION_REQ_DISCONNECT: DisconnectFromSecondary\n") );

					if (!FlagOn(PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_DISCONNECTED)) {

						DisconnectFromSecondary( PrimarySession );
						ClearFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_CONNECTED );
						SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_DISCONNECTED );
					}

					if (primarySessionRequest->Synchronous == TRUE)
						KeSetEvent( &primarySessionRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
					else
						DereferencePrimarySessionRequest( primarySessionRequest );

				} else if (primarySessionRequest->RequestType == PRIMARY_SESSION_REQ_DOWN) {
					
					SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_STOPED );
					primarySessionTerminate = TRUE;

					if (primarySessionRequest->Synchronous == TRUE)
						KeSetEvent( &primarySessionRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
					else
						DereferencePrimarySessionRequest( primarySessionRequest );
				
				} else if (primarySessionRequest->RequestType == PRIMARY_SESSION_SHUTDOWN) {

					SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE, ("PrimarySessionThreadProc: PRIMARY_SESSION_THREAD_FLAG_SHUTDOWN\n") );
					SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_SHUTDOWN );

					ASSERT (primarySessionRequest->Synchronous == TRUE);

					PrimarySession->Thread.ShutdownPrimarySessionRequest = primarySessionRequest;

					if (!FlagOn(PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_DISCONNECTED)) {

						DisconnectFromSecondary( PrimarySession );
						ClearFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_CONNECTED );
						SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_DISCONNECTED );
					}

				} else if (primarySessionRequest->RequestType == PRIMARY_SESSION_STOPPING) {

					SetFlag( PrimarySession->Flags, PRIMARY_SESSION_FLAG_STOPPING );
					CloseOpenFiles( PrimarySession, FALSE );

					if (primarySessionRequest->Synchronous == TRUE)
						KeSetEvent( &primarySessionRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
					else
						DereferencePrimarySessionRequest( primarySessionRequest );

				} else if (primarySessionRequest->RequestType == PRIMARY_SESSION_CANCEL_STOPPING) {

					ClearFlag( PrimarySession->Flags, PRIMARY_SESSION_FLAG_STOPPING );

					if (primarySessionRequest->Synchronous == TRUE)
						KeSetEvent( &primarySessionRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
					else
						DereferencePrimarySessionRequest( primarySessionRequest );

				} else {

					ASSERT( LFS_BUG );
					SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_ERROR );
				}
			}

			continue;
		
		} else if (eventStatus == 1) {

			while (TRUE) {
	
				for (slotIndex = 0; slotIndex < PrimarySession->Thread.SessionSlotCount; slotIndex ++) {

					if (PrimarySession->Thread.SessionSlot[slotIndex].State == SLOT_FINISH)
						break;
				}

				if (slotIndex == PrimarySession->Thread.SessionSlotCount)
					break;
			
				PrimarySession->Thread.SessionSlot[slotIndex].State = SLOT_WAIT;
				PrimarySession->Thread.IdleSlotCount ++;

				if (PrimarySession->Thread.SessionSlot[slotIndex].Status == STATUS_SUCCESS) {

					PNDFS_REPLY_HEADER		ndfsReplyHeader;

					ndfsReplyHeader = (PNDFS_REPLY_HEADER)PrimarySession->Thread.SessionSlot[slotIndex].ReplyMessageBuffer;
										
					PrimarySession->Thread.SessionSlot[slotIndex].Status
						= SendNdfsWinxpMessage( PrimarySession,
												ndfsReplyHeader,
												PrimarySession->Thread.SessionSlot[slotIndex].NdfsWinxpReplyHeader,
												PrimarySession->Thread.SessionSlot[slotIndex].ReplyDataSize,
												slotIndex );

				}
	
				if (PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpRequestMessagePool) {

					ExFreePool(PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpRequestMessagePool);	
					PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpRequestMessagePool = NULL;
					PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpReplyMessagePoolLength = 0;
				}
		
				if (PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpReplyMessagePool) {

					ExFreePool(PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpReplyMessagePool);	
					PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpReplyMessagePool = NULL;
					PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpReplyMessagePoolLength = 0;
				}

				if (!(PrimarySession->Thread.SessionSlot[slotIndex].Status == STATUS_SUCCESS || 
					  PrimarySession->Thread.SessionSlot[slotIndex].Status == STATUS_PENDING)) {

					SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_ERROR );
					
					if (PrimarySession->NetdiskPartition) {

						NetdiskManager_ReturnPrimaryPartition( GlobalLfs.NetdiskManager, 
															   PrimarySession,
															   PrimarySession->NetdiskPartition, 
															   PrimarySession->IsLocalAddress );

						PrimarySession->NetdiskPartition = NULL;
					}
					
					primarySessionTerminate = TRUE;
					break;		
				 }
				
				if (PrimarySession->Thread.SessionState == SESSION_CLOSED) {

					if (PrimarySession->NetdiskPartition) {

						NetdiskManager_ReturnPrimaryPartition( GlobalLfs.NetdiskManager, 
															   PrimarySession,
															   PrimarySession->NetdiskPartition, 
															   PrimarySession->IsLocalAddress );
	
						PrimarySession->NetdiskPartition = NULL;
					}

					primarySessionTerminate = TRUE;
					break;		
				}

				if (PrimarySession->Thread.SessionSlot[slotIndex].Status == STATUS_SUCCESS) {

					if (PrimarySession->Thread.TdiReceiving == FALSE) {

						status = LpxTdiRecvWithCompletionEvent( PrimarySession->ConnectionFileObject,
															    &PrimarySession->Thread.TdiReceiveContext,
															    (PUCHAR)&PrimarySession->Thread.NdfsRequestHeader,
															    sizeof(NDFS_REQUEST_HEADER),
															    0,
															    NULL,
															    NULL );

						if (!NT_SUCCESS(status)) {

							ASSERT( LFS_BUG );

							SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_ERROR );
							primarySessionTerminate = TRUE;
							break;
						}
						
						PrimarySession->Thread.TdiReceiving = TRUE;
					}
				}
			}		
		
			continue;
		
		} else {

			ASSERT( eventStatus == 2 );  // Receive Event
			ASSERT( !FlagOn(PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_SHUTDOWN_WAIT) &&
				    !FlagOn(PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_SHUTDOWN) );

			if (PrimarySession->Thread.TdiReceiveContext.Result != sizeof(NDFS_REQUEST_HEADER)) {

				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
							   ("DispatchRequest: Disconnected, PrimarySession = Data received:%d\n",
							   PrimarySession->Thread.TdiReceiveContext.Result) );

				SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_DISCONNECTED );
				primarySessionTerminate = TRUE;
				
				continue;		
			}

			PrimarySession->Thread.TdiReceiving = FALSE;

#if 0
			if (PrimarySession->NetdiskPartition) {

				PENABLED_NETDISK EnabledNetdisk = PrimarySession->NetdiskPartition->EnabledNetdisk;
				
				ASSERT( EnabledNetdisk );

				if (NetdiskManager_IsStoppedNetdisk(GlobalLfs.NetdiskManager, EnabledNetdisk)) {
				    
					SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_ERROR,
								   ("PrimarySessionThread: %p Netdisk is stopped\n", PrimarySession) );

					SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_ERROR, ("DispatchWinXpRequest: Netdisk is stopped.\n") );

					if (!FlagOn(PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_DISCONNECTED)) { 
					
						// no other way to notify secondary about unmount without break backward compatability.
						
						SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO, ("IsStoppedNetdisk: DisconnectFromSecondary\n") );
						DisconnectFromSecondary( PrimarySession );
						ClearFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_CONNECTED );
						SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_DISCONNECTED );
					}

					primarySessionTerminate = TRUE;
					continue;
				}
			} 

#endif

			status = DispatchRequest( PrimarySession );

			if (!(status == STATUS_SUCCESS || status == STATUS_PENDING)) {

				primarySessionTerminate = TRUE;
				continue;		
			}

			if (PrimarySession->Thread.SessionState == SESSION_CLOSED) {

				primarySessionTerminate = TRUE;
				continue;		
			}

			if (status == STATUS_SUCCESS) {

				if (PrimarySession->Thread.TdiReceiving == FALSE) {

					status = LpxTdiRecvWithCompletionEvent( PrimarySession->ConnectionFileObject,
														    &PrimarySession->Thread.TdiReceiveContext,
														    (PUCHAR)&PrimarySession->Thread.NdfsRequestHeader,
														    sizeof(NDFS_REQUEST_HEADER),
														    0,
														    NULL,
														    NULL );

					if (!NT_SUCCESS(status)) {

						SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_ERROR );
						primarySessionTerminate = TRUE;
					}

					PrimarySession->Thread.TdiReceiving = TRUE;
				}
			}
			
			continue;
		}
	}

	ExAcquireFastMutexUnsafe( &PrimarySession->FastMutex );
	SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_STOPED );
	ExReleaseFastMutexUnsafe( &PrimarySession->FastMutex );

	while (TRUE) {

		LARGE_INTEGER	timeOut;
		NTSTATUS		eventStatus;


		if (PrimarySession->Thread.IdleSlotCount == PrimarySession->Thread.SessionSlotCount)
			break;

		timeOut.QuadPart = -10*HZ;
		eventStatus = KeWaitForSingleObject( &PrimarySession->Thread.WorkCompletionEvent,
											 Executive,
											 KernelMode,
											 FALSE,
											 &timeOut );

		KeClearEvent( &PrimarySession->Thread.WorkCompletionEvent );

		if (eventStatus == STATUS_TIMEOUT) {

			ASSERT( LFS_UNEXPECTED );
			continue;
		}

		while (TRUE) {
	
			for (slotIndex = 0; slotIndex < PrimarySession->Thread.SessionSlotCount; slotIndex++) {

				if (PrimarySession->Thread.SessionSlot[slotIndex].State == SLOT_FINISH)
					break;
			}

			if (slotIndex == PrimarySession->Thread.SessionSlotCount)
				break;

			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE, ("PrimarySessionThreadProc: eventStatus = %d\n", eventStatus) );
			
			PrimarySession->Thread.SessionSlot[slotIndex].State = SLOT_WAIT;
			PrimarySession->Thread.IdleSlotCount++;

			if (PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpRequestMessagePool) {

				ExFreePool( PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpRequestMessagePool );	
				PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpRequestMessagePool = NULL;
				PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpReplyMessagePoolLength = 0;
			}
		
			if (PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpReplyMessagePool) {

				ExFreePool( PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpReplyMessagePool );	
				PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpReplyMessagePool = NULL;
				PrimarySession->Thread.SessionSlot[slotIndex].ExtendWinxpReplyMessagePoolLength = 0;
			}
		}		
	}
	
	if (!FlagOn(PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_DISCONNECTED)) {

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO, ("PsTerminateSystemThread: DisconnectFromSecondary\n") );
		DisconnectFromSecondary( PrimarySession );
		ClearFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_CONNECTED );
		SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_DISCONNECTED );
	}

	CloseOpenFiles( PrimarySession, TRUE );

	while (primarySessionRequestEntry = ExInterlockedRemoveHeadList( &PrimarySession->RequestQueue,
																	 &PrimarySession->RequestQSpinLock)) {

		PPRIMARY_SESSION_REQUEST	primarySessionRequest;
			
		primarySessionRequest = CONTAINING_RECORD( primarySessionRequestEntry,
												   PRIMARY_SESSION_REQUEST,
												   ListEntry );

		if (primarySessionRequest->Synchronous == TRUE)
			KeSetEvent( &primarySessionRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
		else
			DereferencePrimarySessionRequest( primarySessionRequest );
	}

	if (PrimarySession->NetdiskPartition) {

		NetdiskManager_ReturnPrimaryPartition( GlobalLfs.NetdiskManager,
											   PrimarySession,
											   PrimarySession->NetdiskPartition, 
											   PrimarySession->IsLocalAddress );

		PrimarySession->NetdiskPartition = NULL;
	}

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
				   ("PrimarySessionThreadProc: PsTerminateSystemThread PrimarySession = %p\n", 
				    PrimarySession) );

	SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_TERMINATED );

	PrimarySession_Dereference ( PrimarySession );

	PsTerminateSystemThread( STATUS_SUCCESS );
}


VOID
CloseOpenFiles(
	IN PPRIMARY_SESSION	PrimarySession,
	IN BOOLEAN			Remove
	)
{
	PLIST_ENTRY	openFileEntry;
	//KIRQL		oldIrql;


	//KeAcquireSpinLock( &PrimarySession->Thread.OpenedFileQSpinLock, &oldIrql );

	for (openFileEntry = PrimarySession->Thread.OpenedFileQueue.Flink; 
		 openFileEntry != &PrimarySession->Thread.OpenedFileQueue; ) {

		POPEN_FILE openFile;
		NTSTATUS   closeStatus;

		openFile = CONTAINING_RECORD( openFileEntry, OPEN_FILE, ListEntry );
		openFileEntry = openFileEntry->Flink;
		
		if (openFile->AlreadyClosed == FALSE) {

			closeStatus = ZwClose( openFile->FileHandle );
			ObDereferenceObject( openFile->FileObject );
			ASSERT( openFile->EventHandle !=NULL );
			ZwClose( openFile->EventHandle );
			openFile->EventHandle = NULL;
			openFile->AlreadyClosed = TRUE;
		
		} else {
		
			closeStatus = STATUS_SUCCESS;
		}

		ASSERT( closeStatus == STATUS_SUCCESS );

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
					   ("CloseOpenFiles: ZwClose: flieHandle = %p, closeStatus = %x\n", 
					    openFile->FileHandle, closeStatus) );

		if (Remove) {

			RemoveEntryList( &openFile->ListEntry );
			InitializeListHead( &openFile->ListEntry );
			PrimarySession_FreeOpenFile( PrimarySession, openFile );
		}
	}

	//KeReleaseSpinLock( &PrimarySession->Thread.OpenedFileQSpinLock, oldIrql );	
	
	return;
}


VOID
DisconnectFromSecondary(
	IN	PPRIMARY_SESSION PrimarySession
	)
{
	ASSERT(PrimarySession->ConnectionFileHandle);
	ASSERT(PrimarySession->ConnectionFileObject);

	LpxTdiDisconnect(PrimarySession->ConnectionFileObject, 0);

	return;
}	


NTSTATUS
PrimarySessionTakeOver( 
	IN  PPRIMARY_SESSION	PrimarySession
    )
{
	NTSTATUS				status = STATUS_UNSUCCESSFUL;
	IO_STATUS_BLOCK			ioStatusBlock;
	ULONG					ioControlCode;
	PSESSION_INFORMATION	sessionInformation = &PrimarySession->SessionInformation;	
	ULONG					inputBufferLength;
	PVOID					handle = NULL;
	ULONG					outputBufferLength;
 		

	sessionInformation->NetdiskPartitionInformation = PrimarySession->NetdiskPartitionInformation;
	sessionInformation->ConnectionFileHandle = PrimarySession->ConnectionFileHandle;
	sessionInformation->ConnectionFileObject = PrimarySession->ConnectionFileObject;
	sessionInformation->SessionKey = PrimarySession->Thread.SessionKey;
	sessionInformation->SessionFlags = PrimarySession->Thread.SessionFlags;
	sessionInformation->PrimaryMaxDataSize = PrimarySession->Thread.PrimaryMaxDataSize;
	sessionInformation->SecondaryMaxDataSize = PrimarySession->Thread.SecondaryMaxDataSize;
	sessionInformation->Uid = PrimarySession->Thread.Uid;
	sessionInformation->Tid = PrimarySession->Thread.Tid;
	sessionInformation->NdfsMajorVersion = PrimarySession->Thread.NdfsMajorVersion;
	sessionInformation->NdfsMinorVersion = PrimarySession->Thread.NdfsMinorVersion;
	sessionInformation->SessionSlotCount = PrimarySession->Thread.SessionSlotCount;
		
	KeInitializeEvent( &sessionInformation->CompletionEvent, SynchronizationEvent , FALSE );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO, ("PrimarySessionTakeOver: set event, sessionInformation->CompletionEvent %p\n", 
											 &sessionInformation->CompletionEvent) );

	ioControlCode		= IOCTL_INSERT_PRIMARY_SESSION; 
	inputBufferLength	= sizeof( SESSION_INFORMATION );
	outputBufferLength	= sizeof( handle );

	RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

	status = NetdiskManager_TakeOver( GlobalLfs.NetdiskManager, 
									  PrimarySession->NetdiskPartition,
									  &PrimarySession->SessionInformation );

	return status;
}	
