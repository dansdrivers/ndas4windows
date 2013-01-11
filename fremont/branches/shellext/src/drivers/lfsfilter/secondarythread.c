#define	__SECONDARY__
#include "LfsProc.h"



NTSTATUS
ConnectToPrimary (
	IN	PSECONDARY	Secondary
	);


NTSTATUS
DisconnectFromPrimary (
	IN	PSECONDARY	Secondary
	);


NTSTATUS
DirectProcessSecondaryRequest (
	IN	PSECONDARY			Secondary,
	IN  PSECONDARY_REQUEST	SecondaryRequest
	);


NTSTATUS
SendNdfsRequestMessage (
	IN	PSECONDARY			Secondary,
	IN  PSECONDARY_REQUEST	SecondaryRequest
	);


NTSTATUS
ReceiveNdfsReplyMessage (
	IN	PSECONDARY			Secondary,
	IN  PSECONDARY_REQUEST	SecondaryRequest,
	IN  BOOLEAN				ReceiveHeader
	);

NTSTATUS
DispatchReply (
	IN	PSECONDARY					Secondary,
	IN  PSECONDARY_REQUEST			SecondaryRequest,
	IN  PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplytHeader
	);

__inline 
LARGE_INTEGER 
CurrentTime (
	VOID
	)
{
	LARGE_INTEGER	currentCount;
	ULONG			interval;
	LARGE_INTEGER	time;
	
	KeQueryTickCount( &currentCount );

	interval = KeQueryTimeIncrement ();

	time.QuadPart = currentCount.QuadPart * interval;

	return time;
}


VOID
SecondaryThreadProc (
	IN	PSECONDARY	Secondary
	)
{
	BOOLEAN		secondaryThreadTerminate = FALSE;
	PLIST_ENTRY	secondaryRequestEntry;

#if 0 //DBG
	ULONG		sendCount = 0;
#endif

#if __ND_FAT_DBG__

	ULONG			requestCount[IRP_MJ_MAXIMUM_FUNCTION] = {0};			
	LARGE_INTEGER	executionTime[IRP_MJ_MAXIMUM_FUNCTION] = {0};
	LARGE_INTEGER	startTime;
	LARGE_INTEGER	endTime;

#endif
	//
	// performance fix.
	// Increment base priority
	//

	KeSetBasePriorityThread(KeGetCurrentThread(), 2);

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("SecondaryThreadProc: Start Secondary = %p\n", Secondary) );
	
	Secondary_Reference( Secondary );
	
	Secondary->Thread.Flags = SECONDARY_THREAD_FLAG_INITIALIZING;

	Secondary->Thread.SessionContext.PrimaryMaxDataSize		= DEFAULT_MAX_DATA_SIZE;
	Secondary->Thread.SessionContext.SecondaryMaxDataSize	= DEFAULT_MAX_DATA_SIZE;
	Secondary->Thread.SessionContext.MessageSecurity		= Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.MessageSecurity;
	Secondary->Thread.SessionContext.RwDataSecurity			= Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.RwDataSecurity;

	Secondary->Thread.TdiReceiveContext.Irp = NULL;
	KeInitializeEvent( &Secondary->Thread.TdiReceiveContext.CompletionEvent, NotificationEvent, FALSE );

	Secondary->Thread.SessionStatus = ConnectToPrimary( Secondary );

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("SecondaryThreadProc: Start Secondary = %p Secondary->Thread.SessionStatus = %x\n", 
		Secondary, Secondary->Thread.SessionStatus) );

	if (Secondary->Thread.SessionStatus != STATUS_SUCCESS) {

		ExAcquireFastMutex( &Secondary->FastMutex );
		ClearFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_INITIALIZING );
		SetFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_STOPED );
		SetFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_UNCONNECTED );
		SetFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_TERMINATED );
		ExReleaseFastMutex( &Secondary->FastMutex );

		KeSetEvent( &Secondary->ReadyEvent, IO_DISK_INCREMENT, FALSE );

		Secondary_Dereference( Secondary );

		PsTerminateSystemThread( STATUS_SUCCESS );
		return;
	}

	Secondary->Thread.IdleSlotCount = Secondary->Thread.SessionContext.SessionSlotCount;

	ExAcquireFastMutex( &Secondary->FastMutex );		
	SetFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_CONNECTED );
	SetFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_START );
	ClearFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_INITIALIZING );
	ExReleaseFastMutex( &Secondary->FastMutex );
			
	KeSetEvent( &Secondary->ReadyEvent, IO_DISK_INCREMENT, FALSE );

	secondaryThreadTerminate = FALSE;
	
	while (secondaryThreadTerminate == FALSE) {

		PKEVENT			events[2];
		LONG			eventCount;
		NTSTATUS		eventStatus;
		LARGE_INTEGER	timeOut;
		

		ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

		eventCount = 0;
		events[eventCount++] = &Secondary->RequestEvent;

		if (Secondary->Thread.ReceiveWaitingCount != 0) {

			events[eventCount++] = &Secondary->Thread.TdiReceiveContext.CompletionEvent;
		}

		timeOut.QuadPart = -LFS_SECONDARY_THREAD_FLAG_TIME_OUT;

		eventStatus = KeWaitForMultipleObjects(	eventCount,
												events,
												WaitAny,
												Executive,
												KernelMode,
												TRUE,
												&timeOut,
												NULL );

#if 1
		if (eventStatus == STATUS_TIMEOUT) {

			KeQuerySystemTime( &Secondary->TryCloseTime );
			Secondary_TryCloseFilExts( Secondary );
			continue;
		}
#else
		if (eventStatus == STATUS_TIMEOUT) {

			LARGE_INTEGER	currentTime;

			KeQuerySystemTime( &currentTime );

			if (Secondary->TryCloseTime.QuadPart > currentTime.QuadPart || 
				(currentTime.QuadPart - Secondary->TryCloseTime.QuadPart) >= LFS_TRY_CLOSE_DURATION) {

				ExAcquireFastMutex( &Secondary->FastMutex );		
				
				if (!Secondary->TryCloseActive && Secondary->VolDo->Vcb.SecondaryOpenFileCount) {
				
					Secondary->TryCloseActive = TRUE;
					ExReleaseFastMutex( &Secondary->FastMutex );
					Secondary_Reference( Secondary );

					IoQueueWorkItem (Secondary->TryCloseWorkItem,
									 SecondaryTryCloseWorkItemRoutine,
									 DelayedWorkQueue,
									 Secondary );
				
				} else {
				
					ExReleaseFastMutex( &Secondary->FastMutex );
				}
			}

			KeQuerySystemTime( &Secondary->TryCloseTime );

			continue;
		}
#endif		
		ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
		ASSERT( eventCount < THREAD_WAIT_OBJECTS );
		
		if (!NT_SUCCESS( eventStatus ) || eventStatus >= eventCount) {

			ASSERT( LFS_UNEXPECTED );
			SetFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_ERROR );
			secondaryThreadTerminate = TRUE;
			continue;
		}
		
		KeClearEvent( events[eventStatus] );

		if (eventStatus == 0) {

			while (!FlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_STOPED) && 
				   (secondaryRequestEntry = ExInterlockedRemoveHeadList(&Secondary->RequestQueue,
																	    &Secondary->RequestQSpinLock))) {

				PSECONDARY_REQUEST	secondaryRequest;

				NTSTATUS			status;
				_U16				slotIndex;

							
				InitializeListHead( secondaryRequestEntry );

				secondaryRequest = CONTAINING_RECORD( secondaryRequestEntry,
													  SECONDARY_REQUEST,
													  ListEntry );

				if (!(secondaryRequest->RequestType == SECONDARY_REQ_DISCONNECT ||
					  secondaryRequest->RequestType == SECONDARY_REQ_DOWN		||
					  secondaryRequest->RequestType == SECONDARY_REQ_SEND_MESSAGE)) {

					ASSERT( FALSE );

					ExAcquireFastMutex( &Secondary->FastMutex );
					SetFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_STOPED | SECONDARY_THREAD_FLAG_ERROR );
					ExReleaseFastMutex( &Secondary->FastMutex );

					ExInterlockedInsertHeadList( &Secondary->RequestQueue,
												 &secondaryRequest->ListEntry,
												 &Secondary->RequestQSpinLock );

					secondaryThreadTerminate = TRUE;
					break;
				}

				if (secondaryRequest->RequestType == SECONDARY_REQ_DISCONNECT) {

					ExAcquireFastMutex( &Secondary->FastMutex );		
					ClearFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_CONNECTED );
					SetFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_DISCONNECTED );
					ExReleaseFastMutex( &Secondary->FastMutex );

					DisconnectFromPrimary( Secondary );
				
					if (secondaryRequest->Synchronous == TRUE)
						KeSetEvent(&secondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE);
					else
						DereferenceSecondaryRequest( secondaryRequest );

					continue;
				}

				if (secondaryRequest->RequestType == SECONDARY_REQ_DOWN) {

					SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("SecondaryThread SECONDARY_REQ_DOWN Secondary = %p\n", Secondary) );

					ExAcquireFastMutex( &Secondary->FastMutex );		
					SetFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_STOPED );
					ExReleaseFastMutex( &Secondary->FastMutex );

					ASSERT( IsListEmpty(&Secondary->RequestQueue) );

					if (secondaryRequest->Synchronous == TRUE)
						KeSetEvent( &secondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
					else
						DereferenceSecondaryRequest( secondaryRequest );

					secondaryThreadTerminate = TRUE;
					break;
				}

				ASSERT( secondaryRequest->RequestType == SECONDARY_REQ_SEND_MESSAGE );
				ASSERT( secondaryRequest->Synchronous == TRUE );

				if (Secondary->Thread.IdleSlotCount == 0) {

					ASSERT( FALSE );
				
					ExInterlockedInsertHeadList( &Secondary->RequestQueue,
												 &secondaryRequest->ListEntry,
												 &Secondary->RequestQSpinLock );
					break;
				
				} 
				
				ASSERT( secondaryRequest->SessionId == Secondary->SessionId );

					
				for (slotIndex=0; slotIndex < Secondary->Thread.SessionContext.SessionSlotCount; slotIndex++) {

					if (Secondary->Thread.SessionSlot[slotIndex] == NULL)
						break;
				}

				if (slotIndex == Secondary->Thread.SessionContext.SessionSlotCount) {
							
					ASSERT( LFS_BUG );

					ExAcquireFastMutex( &Secondary->FastMutex );
					SetFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_STOPED | SECONDARY_THREAD_FLAG_ERROR );
					ExReleaseFastMutex( &Secondary->FastMutex );

					ExInterlockedInsertHeadList( &Secondary->RequestQueue,
												 &secondaryRequest->ListEntry,
												 &Secondary->RequestQSpinLock );
					break;
				}

				do {

					Secondary->Thread.IdleSlotCount--;

					Secondary->Thread.SessionSlot[slotIndex] = secondaryRequest;
					secondaryRequest->NdfsRequestHeader.Mid = slotIndex;

#if __ND_FAT_DBG__
					requestCount[((PNDFS_WINXP_REQUEST_HEADER)(&secondaryRequest->NdfsRequestHeader+1))->IrpMajorFunction]++;
					KeQuerySystemTime(&startTime);
#endif						
				
					status = SendNdfsRequestMessage( Secondary, secondaryRequest );

					if (status != STATUS_SUCCESS) {

						ASSERT( status == STATUS_REMOTE_DISCONNECT );
						break;				
					}

					ASSERT( Secondary->Thread.SessionContext.SessionSlotCount == 1 );

					if (Secondary->Thread.SessionContext.SessionSlotCount > 1)	{
				
						if (Secondary->Thread.ReceiveWaitingCount == 0) {

							status = LpxTdiRecvWithCompletionEvent( Secondary->Thread.ConnectionFileObject,
																    &Secondary->Thread.TdiReceiveContext,
																	(PUCHAR)&Secondary->Thread.NdfsReplyHeader,
																	sizeof(NDFS_REQUEST_HEADER),
																	0,
																	NULL,
																	NULL );

							if (!NT_SUCCESS(status)) {

								ASSERT( LFS_BUG );
								break;
							}
						}

						Secondary->Thread.ReceiveWaitingCount ++;
						status = STATUS_PENDING;					
						break;
					}

					status = ReceiveNdfsReplyMessage( Secondary, secondaryRequest, TRUE );

#if 0 //DBG

					if (sendCount++ == 100 && status == STATUS_SUCCESS) {

						ULONG		result = 0;
						_U8			buffer[100];
						LARGE_INTEGER	timeOut;

						timeOut.QuadPart = 8*HZ;

						DbgPrint( "receivce time out\n" );
						status = LpxTdiRecv( Secondary->Thread.ConnectionFileObject,
											 buffer,
											 100,
											 0,
											 &timeOut,
											 NULL /* &onceTransStat */,
											 &result );

						ASSERT( status == STATUS_IO_TIMEOUT );
						status = STATUS_REMOTE_DISCONNECT;
						DbgPrint( "returned\n" );

						ExAcquireFastMutex( &Secondary->FastMutex );		
						ClearFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_CONNECTED );
						SetFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_DISCONNECTED );
						ExReleaseFastMutex( &Secondary->FastMutex );

						DisconnectFromPrimary( Secondary );
						DbgPrint( "disconnected\n" );
					}

#endif

#if __ND_FAT_DBG__
					if (status == STATUS_SUCCESS) {

						PNDFS_REPLY_HEADER			ndfsReplyHeader;
						PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplyHeader;

						KeQuerySystemTime( &endTime );
						ndfsReplyHeader = &secondaryRequest->NdfsReplyHeader;
						ndfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)(ndfsReplyHeader+1);
						executionTime[ndfsWinxpReplyHeader->IrpMajorFunction].QuadPart += (endTime.QuadPart-startTime.QuadPart);
							
						if (requestCount[ndfsWinxpReplyHeader->IrpMajorFunction] %1000 == 0) {

							CHAR	irpMajorString[OPERATION_NAME_BUFFER_SIZE];
							CHAR	irpMinorString[OPERATION_NAME_BUFFER_SIZE];

							switch(ndfsWinxpReplyHeader->IrpMajorFunction) {

							case IRP_MJ_CREATE:
							case IRP_MJ_CLOSE:
							case IRP_MJ_CLEANUP:
							case IRP_MJ_SET_INFORMATION:
							case IRP_MJ_QUERY_INFORMATION:
							case IRP_MJ_DIRECTORY_CONTROL:
			
								if (requestCount[ndfsWinxpReplyHeader->IrpMajorFunction] %10000 != 0) 
									break;
								
							case IRP_MJ_QUERY_VOLUME_INFORMATION:
									
								if (requestCount[ndfsWinxpReplyHeader->IrpMajorFunction] %1000 != 0) 
									break;

							default:
	
								GetIrpName( ndfsWinxpReplyHeader->IrpMajorFunction,
											ndfsWinxpReplyHeader->IrpMinorFunction,
											0,
											irpMajorString,
											irpMinorString );
	
								SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("%s, requestCount = %d, executionTime = %I64d\n",
													  irpMajorString, requestCount[ndfsWinxpReplyHeader->IrpMajorFunction],
													  executionTime[ndfsWinxpReplyHeader->IrpMajorFunction].QuadPart/(HZ/1000)) );
							}
						}
					}
#endif
				
					if (status != STATUS_SUCCESS) {

						ASSERT( status == STATUS_REMOTE_DISCONNECT );
						break;
					}

				} while (0);
				
                if (status != STATUS_PENDING) {

					if (status != STATUS_SUCCESS) {

						ExAcquireFastMutex( &Secondary->FastMutex );		


						if (status == STATUS_REMOTE_DISCONNECT) {

							SetFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED );
						
						} else {

							SetFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_ERROR );
						}

						ClearFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_CONNECTED );
						SetFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_STOPED );

						ExReleaseFastMutex( &Secondary->FastMutex );
					}


					Secondary->Thread.SessionSlot[slotIndex] = NULL;
					Secondary->Thread.IdleSlotCount++;

					secondaryRequest->ExecuteStatus = status;

					if (secondaryRequest->Synchronous == TRUE)
						KeSetEvent( &secondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
					else
						DereferenceSecondaryRequest( secondaryRequest );

					if (status != STATUS_SUCCESS && status != STATUS_PENDING) {

						secondaryThreadTerminate = TRUE;
						break;
					}	
				}

			} // while 
		
		} else if (eventStatus == 1) {

			NTSTATUS			status;
			PSECONDARY_REQUEST	secondaryRequest;


			if (Secondary->Thread.TdiReceiveContext.Result != sizeof(NDFS_REQUEST_HEADER)) {
			
				_U16		slotIndex;

				for (slotIndex=0; slotIndex < Secondary->Thread.SessionContext.SessionSlotCount; slotIndex++) {

					if (Secondary->Thread.SessionSlot[slotIndex] != NULL) // choose arbitrary one
						break;
				}

				ASSERT (slotIndex < Secondary->Thread.SessionContext.SessionSlotCount );

				secondaryRequest = Secondary->Thread.SessionSlot[slotIndex];
				Secondary->Thread.SessionSlot[slotIndex] = NULL;
				
				secondaryRequest->ExecuteStatus = STATUS_REMOTE_DISCONNECT;
				ExAcquireFastMutex( &Secondary->FastMutex );
				SetFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED);
				ExReleaseFastMutex( &Secondary->FastMutex );
	
				secondaryThreadTerminate = TRUE;
				if (secondaryRequest->Synchronous == TRUE)
					KeSetEvent(&secondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE);
				else
					DereferenceSecondaryRequest(secondaryRequest);

				continue;
			}

			secondaryRequest = Secondary->Thread.SessionSlot[Secondary->Thread.NdfsReplyHeader.Mid];
			Secondary->Thread.SessionSlot[Secondary->Thread.NdfsReplyHeader.Mid] = NULL;

			RtlCopyMemory( secondaryRequest->NdfsMessage,
						   &Secondary->Thread.NdfsReplyHeader,
						   sizeof(NDFS_REPLY_HEADER) );
			
			status = ReceiveNdfsReplyMessage(Secondary, secondaryRequest, FALSE);

			if (status == STATUS_SUCCESS) {

				status = DispatchReply( Secondary,
										secondaryRequest,
										(PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData );
			}
						
			if (status == STATUS_SUCCESS) {

				secondaryRequest->ExecuteStatus = status;
			
			} else {

				ASSERT( status == STATUS_REMOTE_DISCONNECT );
				if (status == STATUS_REMOTE_DISCONNECT) {

					secondaryRequest->ExecuteStatus = status;

					ExAcquireFastMutex( &Secondary->FastMutex );
					SetFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED );
					ExReleaseFastMutex( &Secondary->FastMutex );
				}
				
				secondaryThreadTerminate = TRUE;
			}
			
			if (status == STATUS_SUCCESS && InterlockedDecrement(&Secondary->Thread.ReceiveWaitingCount) != 0) {
			
				NTSTATUS	tdiStatus;

				tdiStatus = LpxTdiRecvWithCompletionEvent( Secondary->Thread.ConnectionFileObject,
														   &Secondary->Thread.TdiReceiveContext,
														   (PUCHAR)&Secondary->Thread.NdfsReplyHeader,
														   sizeof(NDFS_REQUEST_HEADER),
														   0,
														   NULL,
														   NULL );

				if (!NT_SUCCESS(tdiStatus)) {

					_U16		slotIndex;
	
					for (slotIndex=0; slotIndex < Secondary->Thread.SessionContext.SessionSlotCount; slotIndex++) {

						if (Secondary->Thread.SessionSlot[slotIndex] != NULL)
							break;
					}
				
					ASSERT (slotIndex < Secondary->Thread.SessionContext.SessionSlotCount );

					secondaryRequest = Secondary->Thread.SessionSlot[slotIndex];
					Secondary->Thread.SessionSlot[slotIndex] = NULL;
					
					secondaryRequest->ExecuteStatus = STATUS_REMOTE_DISCONNECT;

					ExAcquireFastMutex( &Secondary->FastMutex );
					SetFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED );
					ExReleaseFastMutex( &Secondary->FastMutex );
					secondaryThreadTerminate = TRUE;

					if (secondaryRequest->Synchronous == TRUE)
						KeSetEvent(&secondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE);
					else
						DereferenceSecondaryRequest(secondaryRequest);
				}
			}

			if (secondaryRequest->Synchronous == TRUE)
				KeSetEvent( &secondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
			else
				DereferenceSecondaryRequest( secondaryRequest );

			continue;
		}
		else
			ASSERT(LFS_BUG);
	}

	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	ExAcquireFastMutex( &Secondary->FastMutex );

	SetFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_STOPED );

	while (secondaryRequestEntry = ExInterlockedRemoveHeadList(&Secondary->RequestQueue,
															   &Secondary->RequestQSpinLock)) {

		PSECONDARY_REQUEST secondaryRequest;

		InitializeListHead( secondaryRequestEntry );
			
		secondaryRequest = CONTAINING_RECORD( secondaryRequestEntry,
											  SECONDARY_REQUEST,
											  ListEntry );
        
		secondaryRequest->ExecuteStatus = STATUS_IO_DEVICE_ERROR;
		
		if (secondaryRequest->Synchronous == TRUE)
			KeSetEvent( &secondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
		else
			DereferenceSecondaryRequest( secondaryRequest );
	}

	ExReleaseFastMutex( &Secondary->FastMutex );

	if (FlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_CONNECTED)) {

		ExAcquireFastMutex( &Secondary->FastMutex );
		ClearFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_CONNECTED );
		SetFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_DISCONNECTED );
		ExReleaseFastMutex( &Secondary->FastMutex );

		DisconnectFromPrimary(Secondary);
	
	} else if (Secondary->Thread.ConnectionFileObject) {

		LpxTdiDisconnect( Secondary->Thread.ConnectionFileObject, 0 );
		LpxTdiDisassociateAddress( Secondary->Thread.ConnectionFileObject );
		LpxTdiCloseConnection( Secondary->Thread.ConnectionFileHandle, 
							   Secondary->Thread.ConnectionFileObject );

		Secondary->Thread.ConnectionFileHandle = NULL;
		Secondary->Thread.ConnectionFileObject = NULL;

		LpxTdiCloseAddress( Secondary->Thread.AddressFileHandle,
			Secondary->Thread.AddressFileObject );

		Secondary->Thread.AddressFileObject = NULL;
		Secondary->Thread.AddressFileHandle = NULL;
	}

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
				("SecondaryThreadProc: PsTerminateSystemThread Secondary = %p, IsListEmpty(&Secondary->RequestQueue) = %d\n", 
				  Secondary, IsListEmpty(&Secondary->RequestQueue)));
	
	ExAcquireFastMutex( &Secondary->FastMutex );
	SetFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_TERMINATED );
	ExReleaseFastMutex( &Secondary->FastMutex );
	
	if (BooleanFlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_UNCONNECTED)) 
		KeSetEvent( &Secondary->ReadyEvent, IO_DISK_INCREMENT, FALSE );

	Secondary_Dereference( Secondary );

	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	
	PsTerminateSystemThread( STATUS_SUCCESS );
}

NTSTATUS
ConnectToPrimary (
	IN	PSECONDARY	Secondary
   )
{
	NTSTATUS				status;

	SOCKETLPX_ADDRESS_LIST	socketLpxAddressList;
	LONG					idx_addr;

    HANDLE					addressFileHandle;
	HANDLE					connectionFileHandle;

    PFILE_OBJECT			addressFileObject;
	PFILE_OBJECT			connectionFileObject;
	
	LPX_ADDRESS				NICAddr;
	PLPX_ADDRESS			serverAddress;
		
	
	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("ConnectToPrimary: Entered\n") );
	
	status = LpxTdiGetAddressList( &socketLpxAddressList );
	
	if (status != STATUS_SUCCESS) {

		ASSERT( LFS_LPX_BUG );
		return status;
	}
	
	if (socketLpxAddressList.iAddressCount <= 0) {

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("BindListenSockets: No NICs in the host.\n") );

		return STATUS_UNEXPECTED_NETWORK_ERROR;
	}
	
	addressFileHandle = NULL; addressFileObject = NULL;
	connectionFileHandle = NULL; connectionFileObject = NULL;

	for (idx_addr=0; idx_addr<socketLpxAddressList.iAddressCount; idx_addr++) {
			
		if ((0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[0]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[1]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[2]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[3]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[4]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[5]) ) {
			
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("BindListenSockets: We don't use SocketLpx device.\n") );
			continue;
		}

		RtlZeroMemory( &NICAddr, sizeof(LPX_ADDRESS) );

		RtlCopyMemory( &NICAddr, 
					   &socketLpxAddressList.SocketLpx[idx_addr].LpxAddress, 
					   sizeof(LPX_ADDRESS) );

		NICAddr.Port = 0;

		status = LpxTdiOpenAddress( &addressFileHandle,
									&addressFileObject,
									&NICAddr );
	
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("Connect from %02X:%02X:%02X:%02X:%02X:%02X 0x%4X\n",
							  NICAddr.Node[0], NICAddr.Node[1], NICAddr.Node[2],
							  NICAddr.Node[3], NICAddr.Node[4], NICAddr.Node[5],
							  NTOHS(NICAddr.Port)) );
		
		if (!NT_SUCCESS(status)) {

			if (status != 0xC001001FL/*NDIS_STATUS_NO_CABLE*/)
				ASSERT(LFS_LPX_BUG);

			continue;
		}
		
		status = LpxTdiOpenConnection( &connectionFileHandle, &connectionFileObject, NULL );
	
		if (!NT_SUCCESS(status)) {

			ASSERT(LFS_UNEXPECTED);
			LpxTdiCloseAddress (addressFileHandle, addressFileObject);
			addressFileHandle = NULL; addressFileObject = NULL;
			continue;
		}

		status = LpxTdiAssociateAddress( connectionFileObject, addressFileHandle );

		if (!NT_SUCCESS(status)) {

			ASSERT( LFS_UNEXPECTED );
			LpxTdiCloseAddress( addressFileHandle, addressFileObject );
			addressFileHandle = NULL; addressFileObject = NULL;
			LpxTdiCloseConnection( connectionFileHandle, connectionFileObject );
			connectionFileHandle = NULL; connectionFileObject = NULL;
			continue;
		}
	
		serverAddress = &Secondary->PrimaryAddress;

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("Connect to %02X:%02X:%02X:%02X:%02X:%02X 0x%4X\n",
							  serverAddress->Node[0], serverAddress->Node[1], serverAddress->Node[2],
							  serverAddress->Node[3], serverAddress->Node[4], serverAddress->Node[5],
							  NTOHS(serverAddress->Port)) );

		status = LpxTdiConnect( connectionFileObject, serverAddress );

		if (status == STATUS_SUCCESS)
			break;

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("Connection is Failed\n") );

		LpxTdiDisassociateAddress( connectionFileObject );
		LpxTdiCloseAddress( addressFileHandle, addressFileObject );
		addressFileHandle = NULL; addressFileObject = NULL;
		LpxTdiCloseConnection( connectionFileHandle, connectionFileObject );
		connectionFileHandle = NULL; connectionFileObject = NULL;
	}

	if (connectionFileHandle == NULL) {

		return STATUS_DEVICE_NOT_CONNECTED;
	}

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("Connection is Success connectionFileObject = %p\n", connectionFileObject));

	do { // just for structural programing

		PSECONDARY_REQUEST			secondaryRequest;
		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_REQUEST_NEGOTIATE		ndfsRequestNegotiate;
		PNDFS_REQUEST_SETUP			ndfsRequestSetup;
		PNDFS_REPLY_HEADER			ndfsReplyHeader;
		PNDFS_REPLY_NEGOTIATE		ndfsReplyNegotiate;
		PNDFS_REPLY_SETUP			ndfsReplySetup;

		ULONG						sendCount = 0;
		PNDFS_REQUEST_TREE_CONNECT	ndfsRequestTreeConnect;
		PNDFS_REPLY_TREE_CONNECT	ndfsReplyTreeConnect;
		ULONG						i;

		secondaryRequest 
			= AllocSecondaryRequest( Secondary,
									 (sizeof(NDFS_REQUEST_HEADER)+sizeof(NDFS_REQUEST_NEGOTIATE) > sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_NEGOTIATE))
									 ? (sizeof(NDFS_REQUEST_HEADER)+sizeof(NDFS_REQUEST_NEGOTIATE)) : (sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_NEGOTIATE)),
									 FALSE );

		if (secondaryRequest == NULL) {

			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		RtlCopyMemory(ndfsRequestHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsRequestHeader->Protocol));
		ndfsRequestHeader->Command	= NDFS_COMMAND_NEGOTIATE;
		ndfsRequestHeader->Flags	= 0;
		ndfsRequestHeader->Uid		= 0;
		ndfsRequestHeader->Tid		= 0;
		ndfsRequestHeader->Mid		= 0;
		ndfsRequestHeader->MessageSize = sizeof(NDFS_REQUEST_HEADER)+sizeof(NDFS_REQUEST_NEGOTIATE);

		ndfsRequestNegotiate = (PNDFS_REQUEST_NEGOTIATE)(ndfsRequestHeader+1);
		ASSERT(ndfsRequestNegotiate == (PNDFS_REQUEST_NEGOTIATE)secondaryRequest->NdfsRequestData);
	
		ndfsRequestNegotiate->NdfsMajorVersion = NDFS_PROTOCOL_MAJOR_2;
		ndfsRequestNegotiate->NdfsMinorVersion = NDFS_PROTOCOL_MINOR_0;
		ndfsRequestNegotiate->OsMajorType	   = OS_TYPE_WINDOWS;
		ndfsRequestNegotiate->OsMinorType	   = OS_TYPE_WINXP;

		status = SendMessage( connectionFileObject,
							  secondaryRequest->NdfsMessage,
							  ndfsRequestHeader->MessageSize,
							  NULL,
							  NULL );

		if (status != STATUS_SUCCESS) {

			DereferenceSecondaryRequest( secondaryRequest );
			break;
		}

		status = RecvMessage( connectionFileObject,
							  secondaryRequest->NdfsMessage,
							  sizeof(NDFS_REPLY_HEADER),
							  NULL );
	
		if (status != STATUS_SUCCESS) {

			//ASSERT( FALSE );
			DereferenceSecondaryRequest( secondaryRequest );
			break;
		}

		ndfsReplyHeader = &secondaryRequest->NdfsReplyHeader;
		
		if (!(RtlEqualMemory(ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol)) && 
			  ndfsReplyHeader->Status == NDFS_SUCCESS)) {

			ASSERT( LFS_BUG );
			DereferenceSecondaryRequest( secondaryRequest );
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		ndfsReplyNegotiate = (PNDFS_REPLY_NEGOTIATE)(ndfsReplyHeader+1);
		ASSERT(ndfsReplyNegotiate == (PNDFS_REPLY_NEGOTIATE)secondaryRequest->NdfsReplyData);

		status = RecvMessage( connectionFileObject,
							  (_U8 *)ndfsReplyNegotiate,
							  sizeof(NDFS_REPLY_NEGOTIATE),
							  NULL );
	
		if (status != STATUS_SUCCESS) {

			//ASSERT( FALSE );
			DereferenceSecondaryRequest( secondaryRequest );
			break;
		}

		if (ndfsReplyNegotiate->Status != NDFS_NEGOTIATE_SUCCESS) {

			ASSERT(LFS_BUG);
			DereferenceSecondaryRequest(secondaryRequest);
			status = STATUS_INVALID_PARAMETER;
			break;
		}
		
		Secondary->Thread.SessionContext.NdfsMajorVersion = ndfsReplyNegotiate->NdfsMajorVersion;
		Secondary->Thread.SessionContext.NdfsMinorVersion = ndfsReplyNegotiate->NdfsMinorVersion;

		Secondary->Thread.SessionContext.SessionKey = ndfsReplyNegotiate->SessionKey;
		
		RtlCopyMemory( Secondary->Thread.SessionContext.ChallengeBuffer,
					   ndfsReplyNegotiate->ChallengeBuffer,
					   ndfsReplyNegotiate->ChallengeLength );

		Secondary->Thread.SessionContext.ChallengeLength = ndfsReplyNegotiate->ChallengeLength;
		
		if (ndfsReplyNegotiate->MaxBufferSize) {

			Secondary->Thread.SessionContext.PrimaryMaxDataSize
				= (ndfsReplyNegotiate->MaxBufferSize <= DEFAULT_MAX_DATA_SIZE)
					? ndfsReplyNegotiate->MaxBufferSize : DEFAULT_MAX_DATA_SIZE;

			//
			//	Initialize transport context for traffic control
			//

			InitTransCtx(&Secondary->Thread.TransportCtx,
				Secondary->Thread.SessionContext.PrimaryMaxDataSize);
		}

		DereferenceSecondaryRequest( secondaryRequest );
			
		secondaryRequest = AllocSecondaryRequest(
							Secondary,
							(sizeof(NDFS_REQUEST_HEADER)+sizeof(NDFS_REQUEST_SETUP) > sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_SETUP))
							? (sizeof(NDFS_REQUEST_HEADER)+sizeof(NDFS_REQUEST_SETUP)) : (sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_SETUP)),
							FALSE
							);

		if (secondaryRequest == NULL) {

			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		RtlCopyMemory(ndfsRequestHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsRequestHeader->Protocol));
		ndfsRequestHeader->Command	= NDFS_COMMAND_SETUP;
		ndfsRequestHeader->Flags	= 0;
		ndfsRequestHeader->Uid		= 0;
		ndfsRequestHeader->Tid		= 0;
		ndfsRequestHeader->Mid		= 0;
		ndfsRequestHeader->MessageSize = sizeof(NDFS_REQUEST_HEADER)+sizeof(NDFS_REQUEST_SETUP);

		ndfsRequestSetup = (PNDFS_REQUEST_SETUP)(ndfsRequestHeader+1);
		ASSERT(ndfsRequestSetup == (PNDFS_REQUEST_SETUP)secondaryRequest->NdfsRequestData);

		ndfsRequestSetup->SessionKey = Secondary->Thread.SessionContext.SessionKey;
		ndfsRequestSetup->MaxBufferSize = Secondary->Thread.SessionContext.SecondaryMaxDataSize;

		RtlCopyMemory( ndfsRequestSetup->NetDiskNode,
					   Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.NetDiskAddress.Node,
					   6 );

		ndfsRequestSetup->NetDiskPort = HTONS(Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.NetDiskAddress.Port);
		ndfsRequestSetup->UnitDiskNo = Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.UnitDiskNo;
		RtlCopyMemory(ndfsRequestSetup->NdscId, Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.NdscId, NDSC_ID_LENGTH);

		ndfsRequestSetup->StartingOffset = 0;
		ndfsRequestSetup->ResponseLength = 0;

		{
			unsigned char		idData[1];
			MD5_CTX	context;

			MD5Init(&context);

			/* id byte */
			idData[0] = (unsigned char)Secondary->Thread.SessionContext.SessionKey;
			MD5Update( &context, idData, 1 );

			MD5Update( &context, Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.Password, 8 );			
			MD5Update( &context, Secondary->Thread.SessionContext.ChallengeBuffer, Secondary->Thread.SessionContext.ChallengeLength );

#if 1
			MD5Final(ndfsRequestSetup->ResponseBuffer, &context);
			ndfsRequestSetup->ResponseLength = 16;
#else
			MD5Final( context.digest, &context );
			RtlCopyMemory( ndfsRequestSetup->ResponseBuffer, context.digest, 16 );
			ndfsRequestSetup->ResponseLength = 16;
#endif
		}

		status = SendMessage( connectionFileObject,
							  secondaryRequest->NdfsMessage,
							  ndfsRequestHeader->MessageSize,
							  NULL,
							  NULL );

		if (status != STATUS_SUCCESS) {

			ASSERT( FALSE );
			DereferenceSecondaryRequest( secondaryRequest );
			break;
		}

		status = RecvMessage( connectionFileObject,
							  secondaryRequest->NdfsMessage,
							  sizeof(NDFS_REPLY_HEADER),
							  NULL );
	
		if (status != STATUS_SUCCESS) {

			ASSERT( FALSE );
			DereferenceSecondaryRequest( secondaryRequest );
			break;
		}

		ndfsReplyHeader = &secondaryRequest->NdfsReplyHeader;
		
		if (!(RtlEqualMemory(ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol)) && 
			  ndfsReplyHeader->Status == NDFS_SUCCESS)) {

			ASSERT(LFS_BUG);
			DereferenceSecondaryRequest( secondaryRequest );
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		ndfsReplySetup = (PNDFS_REPLY_SETUP)(ndfsReplyHeader+1);
		ASSERT( ndfsReplySetup == (PNDFS_REPLY_SETUP)secondaryRequest->NdfsReplyData );

		status = RecvMessage( connectionFileObject,
							  (_U8 *)ndfsReplySetup,
							  sizeof(NDFS_REPLY_SETUP),
							  NULL );

		if (status != STATUS_SUCCESS) {

			ASSERT( FALSE );
			DereferenceSecondaryRequest( secondaryRequest );
			break;
		}
		
		if (ndfsReplySetup->Status != NDFS_SETUP_SUCCESS) {

			DereferenceSecondaryRequest(secondaryRequest);
			status = STATUS_INVALID_PARAMETER;
			break;
		}
		
		Secondary->Thread.SessionContext.Uid = ndfsReplyHeader->Uid;
		
		ASSERT(Secondary->Thread.SessionContext.BytesPerFileRecordSegment == 0);
		Secondary->Thread.SessionContext.SessionSlotCount = 1;
		Secondary->Thread.SessionContext.BytesPerFileRecordSegment = 0;
		DereferenceSecondaryRequest( secondaryRequest );


SEND_TREE_CONNECT_COMMAND:

		secondaryRequest 
			= AllocSecondaryRequest( Secondary,
								     (sizeof(NDFS_REQUEST_HEADER)+sizeof(NDFS_REQUEST_TREE_CONNECT) > sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_TREE_CONNECT))
									 ? (sizeof(NDFS_REQUEST_HEADER)+sizeof(NDFS_REQUEST_TREE_CONNECT)) : (sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_TREE_CONNECT)),
									 FALSE );

		if (secondaryRequest == NULL) {

			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		sendCount++;

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		RtlCopyMemory(ndfsRequestHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsRequestHeader->Protocol));
		ndfsRequestHeader->Command	= NDFS_COMMAND_TREE_CONNECT;
		ndfsRequestHeader->Flags	= 0;
		ndfsRequestHeader->Uid		= Secondary->Thread.SessionContext.Uid;
		ndfsRequestHeader->Tid		= 0;
		ndfsRequestHeader->Mid		= 0;
		ndfsRequestHeader->MessageSize = sizeof(NDFS_REQUEST_HEADER)+sizeof(NDFS_REQUEST_TREE_CONNECT);

		ndfsRequestTreeConnect = (PNDFS_REQUEST_TREE_CONNECT)(ndfsRequestHeader+1);
		ASSERT(ndfsRequestTreeConnect == (PNDFS_REQUEST_TREE_CONNECT)secondaryRequest->NdfsRequestData);

if (IS_WINDOWS2K())
		ndfsRequestTreeConnect->StartingOffset = Secondary->LfsDeviceExt->NetdiskPartitionInformation.PartitionInformation.StartingOffset.QuadPart;
else
		ndfsRequestTreeConnect->StartingOffset = Secondary->LfsDeviceExt->NetdiskPartitionInformation.PartitionInformationEx.StartingOffset.QuadPart;

		status = SendMessage( connectionFileObject,
							  secondaryRequest->NdfsMessage,
							  ndfsRequestHeader->MessageSize,
							  NULL,
							  NULL );

		if (status != STATUS_SUCCESS) {

			DereferenceSecondaryRequest( secondaryRequest );
			break;
		}

		status = RecvMessage( connectionFileObject,
							  secondaryRequest->NdfsMessage,
							  sizeof(NDFS_REPLY_HEADER),
							  NULL );
	
		if (status != STATUS_SUCCESS) {

			DereferenceSecondaryRequest( secondaryRequest );
			break;
		}

		ndfsReplyHeader = &secondaryRequest->NdfsReplyHeader;
		
		if (!(RtlEqualMemory(ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol)) && 
			  ndfsReplyHeader->Status == NDFS_SUCCESS)) {
				
			ASSERT(LFS_BUG);
			DereferenceSecondaryRequest( secondaryRequest );
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		ndfsReplyTreeConnect = (PNDFS_REPLY_TREE_CONNECT)(ndfsReplyHeader+1);
		ASSERT( ndfsReplyTreeConnect == (PNDFS_REPLY_TREE_CONNECT)secondaryRequest->NdfsReplyData );

		status = RecvMessage( connectionFileObject,
							  (_U8 *)ndfsReplyTreeConnect,
							  sizeof(NDFS_REPLY_TREE_CONNECT),
							  NULL );
	
		if (status != STATUS_SUCCESS) {
				
			DereferenceSecondaryRequest(secondaryRequest);
			break;
		}

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, 
					   ("NDFS_COMMAND_TREE_CONNECT: ndfsReplyTreeConnect->Status = %x\n", 
						ndfsReplyTreeConnect->Status) );

		if (ndfsReplyTreeConnect->Status == NDFS_TREE_CORRUPTED || 
			ndfsReplyTreeConnect->Status == NDFS_TREE_CONNECT_NO_PARTITION) {

			if (ndfsReplyTreeConnect->Status == NDFS_TREE_CORRUPTED && sendCount <= 4 || 
				ndfsReplyTreeConnect->Status == NDFS_TREE_CONNECT_NO_PARTITION && sendCount <= 100) {

				LARGE_INTEGER	interval;

				interval.QuadPart = (10 * DELAY_ONE_SECOND);      //delay 10 seconds
				KeDelayExecutionThread( KernelMode, FALSE, &interval );
	
				DereferenceSecondaryRequest( secondaryRequest );
				goto SEND_TREE_CONNECT_COMMAND;	
			}

#if 0

			ASSERT( LFS_REQUIRED ); // sometimes occur. need to handle.

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR,
						   ("Unplugging the NDAS device in slot %u. TreeStatus=%u\n",
							Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.SlotNo,
							ndfsReplyTreeConnect->Status) );

			//
			//	Queue an event to notify user applications
			//
			
			XevtQueueVolumeInvalidOrLocked( -1,
											Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.SlotNo,
											Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.UnitDiskNo );

			//
			//	Unplug whole NDAS device immediately.
			//
			
			unplugStatus = NDCtrlUnplug( Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.SlotNo );
	
			ASSERT( NT_SUCCESS(unplugStatus) );
#endif

			if (ndfsReplyTreeConnect->Status == NDFS_TREE_CORRUPTED) {

				status = STATUS_DISK_CORRUPT_ERROR;
			
			} else if (ndfsReplyTreeConnect->Status == NDFS_TREE_CONNECT_NO_PARTITION ) {

				status = STATUS_UNRECOGNIZED_VOLUME;
			
			} else {

				NDASFS_ASSERT( FALSE );
			}

			DereferenceSecondaryRequest( secondaryRequest );
	
			break;
			
		} else if (ndfsReplyTreeConnect->Status == NDFS_TREE_CONNECT_UNSUCCESSFUL) {
				
			//ASSERT(LFS_BUG);
			DereferenceSecondaryRequest( secondaryRequest );
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		ASSERT( ndfsReplyTreeConnect->Status == NDFS_TREE_CONNECT_SUCCESS );

		Secondary->Thread.SessionContext.Tid = ndfsReplyHeader->Tid;
		Secondary->Thread.SessionContext.SessionSlotCount = ndfsReplyTreeConnect->SessionSlotCount;
		Secondary->Thread.SessionContext.BytesPerFileRecordSegment = ndfsReplyTreeConnect->BytesPerFileRecordSegment;
		Secondary->Thread.SessionContext.BytesPerSector = ndfsReplyTreeConnect->BytesPerSector;
	
		for (Secondary->Thread.SessionContext.SectorShift = 0, i = Secondary->Thread.SessionContext.BytesPerSector; 
			i > 1; 
			i = i / 2) {
		
			Secondary->Thread.SessionContext.SectorShift += 1;
		}
		Secondary->Thread.SessionContext.SectorMask = Secondary->Thread.SessionContext.BytesPerSector - 1;
		Secondary->Thread.SessionContext.BytesPerCluster = ndfsReplyTreeConnect->BytesPerCluster;

		for (Secondary->Thread.SessionContext.ClusterShift = 0, i = Secondary->Thread.SessionContext.BytesPerCluster; 
			i > 1; 
			i = i / 2) {

			Secondary->Thread.SessionContext.ClusterShift += 1;
		}

		Secondary->Thread.SessionContext.ClusterMask = Secondary->Thread.SessionContext.BytesPerCluster - 1;

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("NDFS_COMMAND_TREE_CONNECT: ndfsReplyTreeConnect->BytesPerSector = %d, ndfsReplyTreeConnect->BytesPerCluster = %d\n", 
							  ndfsReplyTreeConnect->BytesPerSector, ndfsReplyTreeConnect->BytesPerCluster));
	
		DereferenceSecondaryRequest( secondaryRequest );
		status = STATUS_SUCCESS;
		
	} while(0);

	if (status != STATUS_SUCCESS) {

		LpxTdiDisconnect( connectionFileObject, 0 );
		LpxTdiDisassociateAddress( connectionFileObject );
		LpxTdiCloseAddress ( addressFileHandle, addressFileObject );
		LpxTdiCloseConnection( connectionFileHandle, connectionFileObject );

		return status;
	}

	Secondary->Thread.AddressFileHandle = addressFileHandle;
	Secondary->Thread.AddressFileObject = addressFileObject;

	Secondary->Thread.ConnectionFileHandle = connectionFileHandle;
	Secondary->Thread.ConnectionFileObject = connectionFileObject;

	return status;
}


NTSTATUS
DisconnectFromPrimary (
	IN	PSECONDARY	Secondary
   )
{
	NTSTATUS			status;

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("DisconnectFromPrimary: Entered\n"));

	ASSERT( Secondary->Thread.AddressFileHandle ); 
	ASSERT( Secondary->Thread.AddressFileObject );
	ASSERT( Secondary->Thread.ConnectionFileHandle );
	ASSERT( Secondary->Thread.ConnectionFileObject ); 

	status = LpxTdiQueryInformation( Secondary->Thread.ConnectionFileObject,
									 TDI_QUERY_CONNECTION_INFO,
									 &Secondary->ConnectionInfo,
									 sizeof(TDI_CONNECTION_INFO) );

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("TransmittedTsdus = %u\n", Secondary->ConnectionInfo.TransmittedTsdus) );
	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("ReceivedTsdus = %u\n", Secondary->ConnectionInfo.ReceivedTsdus) );
	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("TransmissionErrors = %u\n", Secondary->ConnectionInfo.TransmissionErrors) );
	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("ReceiveErrors = %u\n", Secondary->ConnectionInfo.ReceiveErrors) );
	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("Throughput = %I64d Bytes/Sec\n", Secondary->ConnectionInfo.Throughput.QuadPart));
	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, ("Delay = %I64d nanoSecond\n", Secondary->ConnectionInfo.Delay.QuadPart * 100) );

	ASSERT( status == STATUS_SUCCESS );
	

	do { // just for structural programing
	
		PSECONDARY_REQUEST		secondaryRequest;
		PNDFS_REQUEST_HEADER	ndfsRequestHeader;
		PNDFS_REQUEST_LOGOFF	ndfsRequestLogoff;
		PNDFS_REPLY_HEADER		ndfsReplyHeader;
		PNDFS_REPLY_LOGOFF		ndfsReplyLogoff;
		NTSTATUS				openStatus;


		secondaryRequest = AllocSecondaryRequest( Secondary,
												  (sizeof(NDFS_REQUEST_HEADER)+sizeof(NDFS_REQUEST_LOGOFF) > sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_LOGOFF))
												   ? (sizeof(NDFS_REQUEST_HEADER)+sizeof(NDFS_REQUEST_LOGOFF)) : (sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_LOGOFF)),
												  FALSE );

		if (secondaryRequest == NULL) {

			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		RtlCopyMemory(ndfsRequestHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsRequestHeader->Protocol));
		ndfsRequestHeader->Command	= NDFS_COMMAND_LOGOFF;
		ndfsRequestHeader->Flags	= 0;
		ndfsRequestHeader->Uid		= Secondary->Thread.SessionContext.Uid;
		ndfsRequestHeader->Tid		= Secondary->Thread.SessionContext.Tid;
		ndfsRequestHeader->Mid		= 0;
		ndfsRequestHeader->MessageSize = sizeof(NDFS_REQUEST_HEADER)+sizeof(NDFS_REQUEST_LOGOFF);

		ndfsRequestLogoff = (PNDFS_REQUEST_LOGOFF)(ndfsRequestHeader+1);
		ASSERT(ndfsRequestLogoff == (PNDFS_REQUEST_LOGOFF)secondaryRequest->NdfsRequestData);
	
		ndfsRequestLogoff->SessionKey = Secondary->Thread.SessionContext.SessionKey;

		openStatus = SendMessage( Secondary->Thread.ConnectionFileObject,
								  secondaryRequest->NdfsMessage,
								  ndfsRequestHeader->MessageSize,
								  NULL,
								  NULL );

		if (openStatus != STATUS_SUCCESS) {

			status = openStatus;
			DereferenceSecondaryRequest( secondaryRequest );
			break;
		}

		openStatus = RecvMessage( Secondary->Thread.ConnectionFileObject,
								  secondaryRequest->NdfsMessage,
								  sizeof(NDFS_REPLY_HEADER),
								  NULL );
	
		if (openStatus != STATUS_SUCCESS) {

			status = openStatus;
			DereferenceSecondaryRequest( secondaryRequest );
			break;
		}

		ndfsReplyHeader = &secondaryRequest->NdfsReplyHeader;
		
		if (!(RtlEqualMemory(ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol)) && 
			ndfsReplyHeader->Status == NDFS_SUCCESS)) {

			ASSERT( LFS_BUG );
			DereferenceSecondaryRequest( secondaryRequest );
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		ndfsReplyLogoff = (PNDFS_REPLY_LOGOFF)(ndfsReplyHeader+1);
		ASSERT(ndfsReplyLogoff == (PNDFS_REPLY_LOGOFF)secondaryRequest->NdfsReplyData);

		openStatus = RecvMessage( Secondary->Thread.ConnectionFileObject,
								  (_U8 *)ndfsReplyLogoff,
								  sizeof(NDFS_REPLY_LOGOFF),
								  NULL );

		if (openStatus != STATUS_SUCCESS) {

			ASSERT( FALSE );

			status = openStatus;
			DereferenceSecondaryRequest( secondaryRequest );
			break;
		}
		
		if (ndfsReplyLogoff->Status != NDFS_LOGOFF_SUCCESS) {

			ASSERT( LFS_BUG );
			
			DereferenceSecondaryRequest( secondaryRequest );
			status = STATUS_INVALID_PARAMETER;
			break;
		}
	
		DereferenceSecondaryRequest( secondaryRequest );

		status = STATUS_SUCCESS;

	} while(0);
	
	LpxTdiDisconnect( Secondary->Thread.ConnectionFileObject, 0 );
	LpxTdiDisassociateAddress( Secondary->Thread.ConnectionFileObject );
	LpxTdiCloseConnection( Secondary->Thread.ConnectionFileHandle, 
						   Secondary->Thread.ConnectionFileObject );

	Secondary->Thread.ConnectionFileHandle = NULL;
	Secondary->Thread.ConnectionFileObject = NULL;

	LpxTdiCloseAddress( Secondary->Thread.AddressFileHandle, Secondary->Thread.AddressFileObject );

	Secondary->Thread.AddressFileObject = NULL;
	Secondary->Thread.AddressFileHandle = NULL;
		
	return status;
}


//
//	Send NDFS request message
//

NTSTATUS
SendNdfsRequestMessage (
	IN	PSECONDARY			Secondary,
	IN  PSECONDARY_REQUEST	SecondaryRequest
	)
{
	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	_U8							*encryptedMessage;
	_U32						requestDataSize;

	//DES_CBC_CTX				desCbcContext;
	//unsigned char				iv[8];
	//int						desResult;

	NTSTATUS					tdiStatus;
	_U32						remaninigDataSize;

	PSESSION_CONTEXT			sessionCtx = &Secondary->Thread.SessionContext;


	ASSERT( !FlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_ERROR) );

	//
	//	Init variables
	//

	ndfsRequestHeader = &SecondaryRequest->NdfsRequestHeader;

	ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
	requestDataSize = ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER);
	encryptedMessage = (_U8 *)ndfsRequestHeader + ndfsRequestHeader->MessageSize;

	ASSERT( ndfsRequestHeader->MessageSize >= sizeof(NDFS_REQUEST_HEADER) );

	ASSERT( ndfsRequestHeader->Uid == sessionCtx->Uid );
	ASSERT( ndfsRequestHeader->Tid == sessionCtx->Tid );


	//
	//	If data size is less than primary's maximum data size,
	//	Send data at a time and return to the caller.
	//

	if (requestDataSize <= sessionCtx->PrimaryMaxDataSize) {

		//
		//	If no security setting, send message in raw and return.
		//

		if (ndfsRequestHeader->MessageSecurity == 0) {

			tdiStatus = SendMessage( Secondary->Thread.ConnectionFileObject,
									 (_U8 *)ndfsRequestHeader,
									 ndfsRequestHeader->MessageSize,
									 NULL,
									 &Secondary->Thread.TransportCtx );

			if (tdiStatus != STATUS_SUCCESS)
				return STATUS_REMOTE_DISCONNECT;
			else
				return STATUS_SUCCESS;
		}

#if 0

		ASSERT(ndfsRequestHeader->MessageSecurity == 1);

		RtlZeroMemory(&desCbcContext, sizeof(desCbcContext));
		RtlZeroMemory(iv, sizeof(iv));
		DES_CBCInit(&desCbcContext, Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.Password, iv, DES_ENCRYPT);

		if(ndfsWinxpRequestHeader->IrpMajorFunction == IRP_MJ_WRITE)
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
				("ProcessSecondaryRequest: ndfsRequestHeader->RwDataSecurity = %d\n", ndfsRequestHeader->RwDataSecurity));

		if(ndfsWinxpRequestHeader->IrpMajorFunction == IRP_MJ_WRITE && ndfsRequestHeader->RwDataSecurity == 0)
		{
			desResult = DES_CBCUpdate(&desCbcContext, encryptedMessage, (_U8 *)ndfsWinxpRequestHeader, sizeof(NDFS_WINXP_REQUEST_HEADER));
			ASSERT(desResult == IDOK);
			RtlCopyMemory(
				ndfsWinxpRequestHeader,
				encryptedMessage,
				sizeof(NDFS_WINXP_REQUEST_HEADER)
				);

			tdiStatus = SendMessage(
							Secondary->Thread.ConnectionFileObject,
							(_U8 *)ndfsRequestHeader,
							SecondaryRequest->NdfsRequestHeader.MessageSize,
							NULL,
							&sessionCtx->TransportCtx
							);
		}
		else
		{
			tdiStatus = SendMessage(
							Secondary->Thread.ConnectionFileObject,
							(_U8 *)ndfsRequestHeader,
							sizeof(NDFS_REQUEST_HEADER),
							NULL,
							&sessionCtx->TransportCtx
							);

			if(tdiStatus != STATUS_SUCCESS)
				return STATUS_REMOTE_DISCONNECT;

			desResult = DES_CBCUpdate(&desCbcContext,
									encryptedMessage,
									(_U8 *)ndfsWinxpRequestHeader,
									ndfsRequestHeader->MessageSize-sizeof(NDFS_REQUEST_HEADER));
			ASSERT(desResult == IDOK);
			tdiStatus = SendMessage(
							Secondary->Thread.ConnectionFileObject,
							encryptedMessage,
							SecondaryRequest->NdfsRequestHeader.MessageSize - sizeof(NDFS_REQUEST_HEADER),
							NULL,
							&sessionCtx->TransportCtx
							);
		}

		//
		//	Return to the caller
		//

		if(tdiStatus != STATUS_SUCCESS)
			return STATUS_REMOTE_DISCONNECT;
		else
			return STATUS_SUCCESS;
#endif
	}

	ASSERT( requestDataSize > sessionCtx->PrimaryMaxDataSize );


	//
	//	Indicate the message is split
	//

	ndfsRequestHeader->Splitted = 1;

#if 0

	if(ndfsRequestHeader->MessageSecurity)
	{
		RtlZeroMemory(&desCbcContext, sizeof(desCbcContext));
		RtlZeroMemory(iv, sizeof(iv));
		DES_CBCInit(&desCbcContext, Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.Password, iv, DES_ENCRYPT);
		
		desResult = DES_CBCUpdate(&desCbcContext, encryptedMessage, (_U8 *)ndfsWinxpRequestHeader, sizeof(NDFS_WINXP_REQUEST_HEADER));
		ASSERT(desResult == IDOK);

		RtlCopyMemory(
			ndfsWinxpRequestHeader,
			encryptedMessage,
			sizeof(NDFS_WINXP_REQUEST_HEADER)
			);
	}
#endif

	//
	//	Send the message NDFS and WinXP header
	//

	tdiStatus = SendMessage( Secondary->Thread.ConnectionFileObject,
							 (_U8 *)ndfsRequestHeader,
							 sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER),
							 NULL,
							 &Secondary->Thread.TransportCtx );

	if (tdiStatus != STATUS_SUCCESS)
		return STATUS_REMOTE_DISCONNECT;
	
	remaninigDataSize = requestDataSize;


	//
	//	Send a pair of a NDFS header and data.
	//

	while (1) {

		//
		//	Set remaining data size to message size.
		//

		ndfsRequestHeader->MessageSize = sizeof(NDFS_REQUEST_HEADER) + remaninigDataSize;
		if (remaninigDataSize <= sessionCtx->PrimaryMaxDataSize)
			ndfsRequestHeader->Splitted = 0;
		else
			ndfsRequestHeader->Splitted = 1;


		//
		//	 Send a header
		//

		tdiStatus = SendMessage( Secondary->Thread.ConnectionFileObject,
								 (_U8 *)ndfsRequestHeader,
								 sizeof(NDFS_REQUEST_HEADER),
								 NULL,
								 &Secondary->Thread.TransportCtx );

		if(tdiStatus != STATUS_SUCCESS)
			return STATUS_REMOTE_DISCONNECT;

		//
		//	Send a part of data
		//
#if 0
		if (ndfsRequestHeader->MessageSecurity) {

			desResult = DES_CBCUpdate( &desCbcContext,
									   encryptedMessage,
									   (_U8 *)(ndfsWinxpRequestHeader+1) + (requestDataSize - remaninigDataSize),
									   ndfsRequestHeader->Splitted ? sessionCtx->PrimaryMaxDataSize : remaninigDataSize );
			
			ASSERT(desResult == IDOK);

			tdiStatus = SendMessage(
						Secondary->Thread.ConnectionFileObject,
						encryptedMessage,
						ndfsRequestHeader->Splitted ? sessionCtx->PrimaryMaxDataSize : remaninigDataSize,
						NULL,
						&sessionCtx->TransportCtx
						);
		}
		else
#endif
		{
			tdiStatus = SendMessage( Secondary->Thread.ConnectionFileObject,
									 (_U8 *)(ndfsWinxpRequestHeader+1) + (requestDataSize - remaninigDataSize),
									 ndfsRequestHeader->Splitted ? sessionCtx->PrimaryMaxDataSize : remaninigDataSize,
									 NULL,
									 &Secondary->Thread.TransportCtx );
		}

		if (tdiStatus != STATUS_SUCCESS)
			return STATUS_REMOTE_DISCONNECT;


		//
		//	Decrease remaining bytes
		//

		if (ndfsRequestHeader->Splitted)
			remaninigDataSize -= sessionCtx->PrimaryMaxDataSize;
		else
			return STATUS_SUCCESS;


		ASSERT( (_S32)remaninigDataSize > 0 );
	}
}


NTSTATUS
ReceiveNdfsReplyMessage (
	IN	PSECONDARY			Secondary,
	IN  PSECONDARY_REQUEST	SecondaryRequest,
	IN  BOOLEAN				ReceiveHeader
	)
{
	NTSTATUS					tdiStatus;
	PNDFS_REPLY_HEADER			ndfsReplyHeader;
	_U8							*encryptedMessage;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplyHeader;


	if (ReceiveHeader == TRUE) {

		tdiStatus = RecvMessage( Secondary->Thread.ConnectionFileObject,
								 SecondaryRequest->NdfsMessage,
								 sizeof(NDFS_REPLY_HEADER),
								 NULL );

		if (tdiStatus != STATUS_SUCCESS) {

			return STATUS_REMOTE_DISCONNECT;
		}
	}

	ndfsReplyHeader = &SecondaryRequest->NdfsReplyHeader;
		
	if (!(RtlEqualMemory(ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol)) && 
		ndfsReplyHeader->Status == NDFS_SUCCESS)) {

		ASSERT( LFS_BUG );
		return STATUS_UNSUCCESSFUL;
	}

	//ASSERT(ndfsReplyHeader->Splitted == 0 && ndfsReplyHeader->MessageSecurity == 0);

	//
	// If the request is not split, receive the request at a time
	//	and return to the caller.
	//

	if (ndfsReplyHeader->MessageSize > SecondaryRequest->NdfsMessageAllocationSize)
		NDASFS_ASSERT( FALSE );

	if (ndfsReplyHeader->Splitted == 0) {

		ndfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)(ndfsReplyHeader+1);
		ASSERT(ndfsWinxpReplyHeader == (PNDFS_WINXP_REPLY_HEADER)SecondaryRequest->NdfsReplyData);
		encryptedMessage = (_U8 *)ndfsReplyHeader + ndfsReplyHeader->MessageSize;

		//
		// Receive non-encrypted request at a time and return to the caller.
		//

		if (ndfsReplyHeader->MessageSecurity == 0) {

			tdiStatus = RecvMessage( Secondary->Thread.ConnectionFileObject,
									 (_U8 *)ndfsWinxpReplyHeader,
									 ndfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER),
									 NULL );

			if (tdiStatus != STATUS_SUCCESS) {

				return STATUS_REMOTE_DISCONNECT;
			}
		}
#if 0
		else
		{
			int desResult;

			//
			//  Receive encrypted WinXP request header
			//	and return to the caller
			//

			tdiStatus = RecvMessage(
							Secondary->Thread.ConnectionFileObject,
							encryptedMessage,
							sizeof(NDFS_WINXP_REPLY_HEADER),
							NULL
							);
			if(tdiStatus != STATUS_SUCCESS)
			{
				return STATUS_REMOTE_DISCONNECT;
			}

			RtlZeroMemory(&SecondaryRequest->DesCbcContext, sizeof(SecondaryRequest->DesCbcContext));
			RtlZeroMemory(SecondaryRequest->Iv, sizeof(SecondaryRequest->Iv));
			DES_CBCInit(&SecondaryRequest->DesCbcContext,
						Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.Password,
						SecondaryRequest->Iv,
						DES_DECRYPT);
			desResult = DES_CBCUpdate(&SecondaryRequest->DesCbcContext,
								(_U8 *)ndfsWinxpReplyHeader,
								encryptedMessage,
								sizeof(NDFS_WINXP_REPLY_HEADER));
			ASSERT(desResult == IDOK);

			if(ndfsWinxpReplyHeader->IrpMajorFunction == IRP_MJ_READ)
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
					("ProcessSecondaryRequest: ndfsReplyHeader->RwDataSecurity = %d\n", ndfsReplyHeader->RwDataSecurity));

			if(ndfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER))
			{
				if(ndfsWinxpReplyHeader->IrpMajorFunction == IRP_MJ_READ && ndfsReplyHeader->RwDataSecurity == 0)
				{
					tdiStatus = RecvMessage(
								Secondary->Thread.ConnectionFileObject,
								(_U8 *)(ndfsWinxpReplyHeader+1),
								ndfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER),
								NULL
								);
					if(tdiStatus != STATUS_SUCCESS)
					{
						return STATUS_REMOTE_DISCONNECT;
					}
				}
				else
				{
					tdiStatus = RecvMessage(
								Secondary->Thread.ConnectionFileObject,
								encryptedMessage,
								ndfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER),
								NULL
								);

					if(tdiStatus != STATUS_SUCCESS)
					{
						return STATUS_REMOTE_DISCONNECT;
					}
					desResult = DES_CBCUpdate(&SecondaryRequest->DesCbcContext,
									(_U8 *)(ndfsWinxpReplyHeader+1),
									encryptedMessage,
									ndfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER));
					ASSERT(desResult == IDOK);
				}
			}
		}

#endif

		//
		//	return to the caller
		//
		
		return STATUS_SUCCESS;
	}

	ASSERT( ndfsReplyHeader->Splitted == 1 );

	//
	//  Receive WinXP request header
	//

	ndfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)(ndfsReplyHeader+1);
	ASSERT(ndfsWinxpReplyHeader == (PNDFS_WINXP_REPLY_HEADER)SecondaryRequest->NdfsReplyData);

#if 0

	if(ndfsReplyHeader->MessageSecurity)
	{
		int desResult;

		encryptedMessage = (_U8 *)ndfsReplyHeader + ndfsReplyHeader->MessageSize;
	
		tdiStatus = RecvMessage(
						Secondary->Thread.ConnectionFileObject,
						encryptedMessage,
						sizeof(NDFS_WINXP_REPLY_HEADER),
						NULL
						);
		if(tdiStatus != STATUS_SUCCESS)
		{
			return STATUS_REMOTE_DISCONNECT;
		}

		RtlZeroMemory(&SecondaryRequest->DesCbcContext, sizeof(SecondaryRequest->DesCbcContext));
		RtlZeroMemory(SecondaryRequest->Iv, sizeof(SecondaryRequest->Iv));
		DES_CBCInit(&SecondaryRequest->DesCbcContext, Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetdiskInformation.Password, SecondaryRequest->Iv, DES_DECRYPT);
		desResult = DES_CBCUpdate(&SecondaryRequest->DesCbcContext, (_U8 *)ndfsWinxpReplyHeader, encryptedMessage, sizeof(NDFS_WINXP_REPLY_HEADER));
		ASSERT(desResult == IDOK);
	}
	else
#endif
	{
		tdiStatus = RecvMessage( Secondary->Thread.ConnectionFileObject,
								 (_U8 *)ndfsWinxpReplyHeader,
								 sizeof(NDFS_WINXP_REPLY_HEADER),
								 NULL );

		if (tdiStatus != STATUS_SUCCESS) {

			return STATUS_REMOTE_DISCONNECT;
		}
	}

	//
	//	Receive a pair of NDFS reply header and data
	//

	while (1) {

		PNDFS_REPLY_HEADER	splitNdfsReplyHeader = &Secondary->Thread.SplitNdfsReplyHeader;
		
		//
		//	header part
		//

		tdiStatus = RecvMessage( Secondary->Thread.ConnectionFileObject,
								 (_U8 *)splitNdfsReplyHeader,
								 sizeof(NDFS_REPLY_HEADER),
								 NULL );

		if (tdiStatus != STATUS_SUCCESS) {

			return STATUS_REMOTE_DISCONNECT;
		}
			
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
					   ("ndfsReplyHeader->MessageSize = %d splitNdfsReplyHeader.MessageSize = %d\n", 
					    ndfsReplyHeader->MessageSize, splitNdfsReplyHeader->MessageSize) );

		if (!(RtlEqualMemory(ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol)) && 
			  ndfsReplyHeader->Status == NDFS_SUCCESS)) {

			ASSERT( LFS_BUG );
			return STATUS_UNSUCCESSFUL;
		}

		//
		//	data part
		//
#if 0
		if(splitNdfsReplyHeader->MessageSecurity)
		{
			int desResult;

			//
			//	Don't know if this encryptedMessage is correctly assigned.
			//

			ASSERT(LFS_UNEXPECTED);
			encryptedMessage = (_U8 *)ndfsReplyHeader + ndfsReplyHeader->MessageSize;

			tdiStatus = RecvMessage(
							Secondary->Thread.ConnectionFileObject,
							encryptedMessage,
							splitNdfsReplyHeader->Splitted ?
								Secondary->Thread.SessionContext.SecondaryMaxDataSize :
								(splitNdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER)),
							NULL
							);

			if(tdiStatus != STATUS_SUCCESS)
			{
				return STATUS_REMOTE_DISCONNECT;
			}
				
			desResult = DES_CBCUpdate(
					&SecondaryRequest->DesCbcContext, 
					(_U8 *)ndfsWinxpReplyHeader + ndfsReplyHeader->MessageSize - splitNdfsReplyHeader->MessageSize, 
					encryptedMessage,
					splitNdfsReplyHeader->Splitted ? Secondary->Thread.SessionContext.SecondaryMaxDataSize : (splitNdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER))
					);
			ASSERT(desResult == IDOK);
		}
		else
#endif
		{
			tdiStatus = RecvMessage( Secondary->Thread.ConnectionFileObject,
									 (_U8 *)ndfsWinxpReplyHeader + ndfsReplyHeader->MessageSize - splitNdfsReplyHeader->MessageSize, 
									 splitNdfsReplyHeader->Splitted ?
										Secondary->Thread.SessionContext.SecondaryMaxDataSize :
										(splitNdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER)),
									 NULL );

			if (tdiStatus != STATUS_SUCCESS) {

				return STATUS_REMOTE_DISCONNECT;
			}
		}
	
		if (splitNdfsReplyHeader->Splitted)
			continue;

		return STATUS_SUCCESS;
	}
}
		

NTSTATUS
DispatchReply (
	IN	PSECONDARY					Secondary,
	IN  PSECONDARY_REQUEST			SecondaryRequest,
	IN  PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplytHeader
	)
{
	NTSTATUS			ntStatus;

	UNREFERENCED_PARAMETER( SecondaryRequest );
	UNREFERENCED_PARAMETER( Secondary );


	switch (NdfsWinxpReplytHeader->IrpMajorFunction) {

	case IRP_MJ_CREATE:	{					// 0x00
	
		//if(NdfsWinxpReplytHeader->Status == STATUS_SUCCESS)
		//	ASSERT(SecondaryRequest->NdfsReplyHeader.MessageSize 
		//			== sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER) + Secondary->Thread.SessionContext.BytesPerFileRecordSegment);
		//else
		//	ASSERT(SecondaryRequest->NdfsReplyHeader.MessageSize 
		//			== sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER));

		ntStatus = STATUS_SUCCESS;
		break;
	}

	case IRP_MJ_CLOSE:						// 0x02
	case IRP_MJ_WRITE:						// 0x04
	case IRP_MJ_SET_INFORMATION:			// 0x06
	case IRP_MJ_SET_EA:						// 0x08
	case IRP_MJ_FLUSH_BUFFERS:				// 0x09
	case IRP_MJ_SET_VOLUME_INFORMATION:		// 0x0b
	case IRP_MJ_LOCK_CONTROL:				// 0x11
	case IRP_MJ_CLEANUP:					// 0x12
	case IRP_MJ_SET_SECURITY:				// 0x15
	case IRP_MJ_SET_QUOTA: {				// 0x1a
		
		ASSERT( SecondaryRequest->NdfsReplyHeader.MessageSize == sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_WINXP_REPLY_HEADER) );
		ntStatus = STATUS_SUCCESS;
		break;
	}

	case IRP_MJ_READ:						// 0x03
	case IRP_MJ_QUERY_INFORMATION:			// 0x05
	case IRP_MJ_QUERY_EA:					// 0x07
	case IRP_MJ_QUERY_VOLUME_INFORMATION:	// 0x0a
	case IRP_MJ_DIRECTORY_CONTROL:			// 0x0c
	case IRP_MJ_FILE_SYSTEM_CONTROL:		// 0x0d
	case IRP_MJ_DEVICE_CONTROL:				// 0x0e
	case IRP_MJ_QUERY_SECURITY:				// 0x14
	case IRP_MJ_QUERY_QUOTA: {				// 0x19
	
		if (!(NdfsWinxpReplytHeader->Status == STATUS_SUCCESS ||
			  NdfsWinxpReplytHeader->Status == STATUS_BUFFER_OVERFLOW)) {
#if DBG

			CHAR		irpMajorString[OPERATION_NAME_BUFFER_SIZE];
			CHAR		irpMinorString[OPERATION_NAME_BUFFER_SIZE];

			ASSERT( SecondaryRequest->NdfsReplyHeader.MessageSize == sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_WINXP_REPLY_HEADER) );
		
			GetIrpName( NdfsWinxpReplytHeader->IrpMajorFunction,
						NdfsWinxpReplytHeader->IrpMinorFunction,
						0,
						irpMajorString,
						OPERATION_NAME_BUFFER_SIZE,
						irpMinorString,
						OPERATION_NAME_BUFFER_SIZE );

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
						   ("DispatchReply: %s %s (%d %d): is not successful\n", irpMinorString, irpMinorString, 
							NdfsWinxpReplytHeader->IrpMajorFunction, NdfsWinxpReplytHeader->IrpMinorFunction) );

#endif
					
			ntStatus = STATUS_SUCCESS;
			break;
		}

		ntStatus = STATUS_SUCCESS;
		break;
	}
	
	case IRP_MJ_INTERNAL_DEVICE_CONTROL:	// 0x0f
	case IRP_MJ_SHUTDOWN:					// 0x10
	case IRP_MJ_CREATE_MAILSLOT:			// 0x13
	case IRP_MJ_POWER:						// 0x16
	case IRP_MJ_SYSTEM_CONTROL:				// 0x17
	case IRP_MJ_DEVICE_CHANGE:				// 0x18
	case IRP_MJ_PNP:						// 0x1b
	default:

		ASSERT( LFS_BUG );
		return STATUS_UNSUCCESSFUL;
	}

	return ntStatus;
}
