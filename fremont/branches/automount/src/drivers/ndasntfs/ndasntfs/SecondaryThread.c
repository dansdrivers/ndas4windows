#include "NtfsProc.h"

#if __NDAS_NTFS_SECONDARY__

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('tFtN')

#define Dbg                              (DEBUG_TRACE_SECONDARY)
#define Dbg2                             (DEBUG_INFO_SECONDARY)


NTSTATUS
ConnectToPrimary(
	IN	PSECONDARY	Secondary
	);


NTSTATUS
DisconnectFromPrimary(
	IN	PSECONDARY	Secondary
	);


NTSTATUS
DirectProcessSecondaryRequest(
	IN	PSECONDARY			Secondary,
	IN  PSECONDARY_REQUEST	SecondaryRequest
	);


NTSTATUS
SendNdfsRequestMessage(
	IN	PSECONDARY			Secondary,
	IN  PSECONDARY_REQUEST	SecondaryRequest
	);


NTSTATUS
ReceiveNdfsReplyMessage(
	IN	PSECONDARY			Secondary,
	IN  PSECONDARY_REQUEST	SecondaryRequest,
	IN  BOOLEAN				ReceiveHeader
	);


NTSTATUS
ProcessSecondaryRequest(
	IN	PSECONDARY			Secondary,
	IN  PSECONDARY_REQUEST	SecondaryRequest
	);


NTSTATUS
DispatchReply(
	IN	PSECONDARY					Secondary,
	IN  PSECONDARY_REQUEST			SecondaryRequest,
	IN  PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplytHeader
	);


VOID
SecondaryThreadProc(
	IN	PSECONDARY	Secondary
	)
{
	BOOLEAN		secondaryThreadTerminate = FALSE;
	PLIST_ENTRY	secondaryRequestEntry;

#if __NDAS_NTFS_DBG__

	ULONG			requestCount[IRP_MJ_MAXIMUM_FUNCTION] = {0};			
	LARGE_INTEGER	executionTime[IRP_MJ_MAXIMUM_FUNCTION] = {0};
	LARGE_INTEGER	startTime;
	LARGE_INTEGER	endTime;

#endif
	
	DebugTrace( 0, Dbg2, ("SecondaryThreadProc: Start Secondary = %p\n", Secondary) );
	
	Secondary_Reference( Secondary );
	
	Secondary->Thread.Flags = SECONDARY_THREAD_FLAG_INITIALIZING;

	Secondary->Thread.SessionContext.PrimaryMaxDataSize		= DEFAULT_NDAS_MAX_DATA_SIZE;
	Secondary->Thread.SessionContext.SecondaryMaxDataSize	= DEFAULT_NDAS_MAX_DATA_SIZE;
	Secondary->Thread.SessionContext.MessageSecurity		= Secondary->VolDo->NetdiskPartitionInformation.NetdiskInformation.MessageSecurity;
	Secondary->Thread.SessionContext.RwDataSecurity			= Secondary->VolDo->NetdiskPartitionInformation.NetdiskInformation.RwDataSecurity;

	Secondary->Thread.TdiReceiveContext.Irp = NULL;
	KeInitializeEvent( &Secondary->Thread.TdiReceiveContext.CompletionEvent, NotificationEvent, FALSE );

	Secondary->Thread.SessionStatus = ConnectToPrimary( Secondary );

	DebugTrace( 0, Dbg2, ("SecondaryThreadProc: Start Secondary = %p Secondary->Thread.SessionStatus = %x\n", 
		Secondary, Secondary->Thread.SessionStatus));

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
	ClearFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_INITIALIZING );
	SetFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_START );
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

		timeOut.QuadPart = -NDNTFS_SECONDARY_THREAD_FLAG_TIME_OUT;

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

			KeQuerySystemTime( &currentTime );

			if (Secondary->TryCloseTime.QuadPart > currentTime.QuadPart || 
				(currentTime.QuadPart - Secondary->TryCloseTime.QuadPart) >= NDNTFS_TRY_CLOSE_DURATION) {

				ExAcquireFastMutex( &Secondary->FastMutex );		
				
				if (!Secondary->TryCloseActive) {
				
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
		
		ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
		ASSERT( eventCount < THREAD_WAIT_OBJECTS );
		
		if (!NT_SUCCESS( eventStatus ) || eventStatus >= eventCount) {

			ASSERT( NDASNTFS_UNEXPECTED );
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

					if(Secondary->Thread.SessionSlot[slotIndex] == NULL)
						break;
				}

				if (slotIndex == Secondary->Thread.SessionContext.SessionSlotCount) {
							
					ASSERT( NDASNTFS_BUG );

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

#if __NDAS_NTFS_DBG__
					requestCount[((PNDFS_WINXP_REQUEST_HEADER)(&secondaryRequest->NdfsRequestHeader+1))->IrpMajorFunction]++;
					KeQuerySystemTime(&startTime);
#endif						
				
					status = SendNdfsRequestMessage( Secondary, secondaryRequest );

					if (status != STATUS_SUCCESS) {

						ASSERT( status == STATUS_REMOTE_DISCONNECT );
						break;				
					}

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

								ASSERT( NDASNTFS_BUG );
								break;
							}
						}

						Secondary->Thread.ReceiveWaitingCount ++;
						status = STATUS_PENDING;					
						break;
					}

					status = ReceiveNdfsReplyMessage( Secondary, secondaryRequest, TRUE );

#if __NDAS_NTFS_DBG__
					if (status == STATUS_SUCCESS) {

						PNDFS_REPLY_HEADER			ndfsReplyHeader;
						PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplyHeader;

						KeQuerySystemTime( &endTime );
						ndfsReplyHeader = &secondaryRequest->NdfsReplyHeader;
						ndfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)(ndfsReplyHeader+1);
						executionTime[ndfsWinxpReplyHeader->IrpMajorFunction].QuadPart += (endTime.QuadPart-startTime.QuadPart);
							
						if (requestCount[ndfsWinxpReplyHeader->IrpMajorFunction] %100 == 0) {

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
	
								DebugTrace( 0, Dbg2, ("%s, requestCount = %d, executionTime = %I64d\n",
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

					if(Secondary->Thread.SessionSlot[slotIndex] != NULL) // choose arbitrary one
						break;
				}

				ASSERT (slotIndex < Secondary->Thread.SessionContext.SessionSlotCount );

				secondaryRequest = Secondary->Thread.SessionSlot[slotIndex];
				Secondary->Thread.SessionSlot[slotIndex] = NULL;
				
				secondaryRequest->ExecuteStatus = STATUS_REMOTE_DISCONNECT;
				ExAcquireFastMutex( &Secondary->FastMutex );
				SetFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ;
				ExReleaseFastMutex( &Secondary->FastMutex );
	
				secondaryThreadTerminate = TRUE;
				if(secondaryRequest->Synchronous == TRUE)
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
						
			if(status == STATUS_SUCCESS) {

				secondaryRequest->ExecuteStatus = status;
			
			} else {

				ASSERT(status == STATUS_REMOTE_DISCONNECT);
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
	
					for(slotIndex=0; slotIndex < Secondary->Thread.SessionContext.SessionSlotCount; slotIndex++) {

						if(Secondary->Thread.SessionSlot[slotIndex] != NULL)
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

					if(secondaryRequest->Synchronous == TRUE)
						KeSetEvent(&secondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE);
					else
						DereferenceSecondaryRequest(secondaryRequest);
				}
			}

			if (secondaryRequest->Synchronous == TRUE)
				KeSetEvent( &secondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
			else
				DereferenceSecondaryRequest(secondaryRequest);

			continue;
		}
		else
			ASSERT(NDASNTFS_BUG);
	}

	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	ExAcquireFastMutex( &Secondary->FastMutex );

	SetFlag( Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_STOPED );

	while (secondaryRequestEntry = ExInterlockedRemoveHeadList(&Secondary->RequestQueue,
															   &Secondary->RequestQSpinLock)) {

		PSECONDARY_REQUEST secondaryRequest;
			
		secondaryRequest = CONTAINING_RECORD( secondaryRequestEntry,
											  SECONDARY_REQUEST,
											  ListEntry );
        
		secondaryRequest->ExecuteStatus = STATUS_IO_DEVICE_ERROR;
		
		if(secondaryRequest->Synchronous == TRUE)
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

	DebugTrace( 0, Dbg2,
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
ConnectToPrimary(
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
		
	
	DebugTrace( 0, Dbg, ("ConnectToPrimary: Entered\n"));
	
	status = LpxTdiGetAddressList( &socketLpxAddressList );
	
	if (status != STATUS_SUCCESS) {

		ASSERT(NDNTFS_LPX_BUG);
		return status;
	}
	
	if (socketLpxAddressList.iAddressCount <= 0) {

		DebugTrace( 0, Dbg, ("BindListenSockets: No NICs in the host.\n") );

		return STATUS_UNEXPECTED_NETWORK_ERROR;
	}
	
	addressFileHandle = NULL; addressFileObject = NULL;
	connectionFileHandle = NULL; connectionFileObject = NULL;

	for (idx_addr=0; idx_addr<socketLpxAddressList.iAddressCount; idx_addr++) {
		
		NTSTATUS status;
	
		if ((0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[0]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[1]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[2]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[3]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[4]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[5]) ) {
			
			DebugTrace( 0, Dbg, ("BindListenSockets: We don't use SocketLpx device.\n") );
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
	
		DebugTrace( 0, Dbg, ("Connect from %02X:%02X:%02X:%02X:%02X:%02X 0x%4X\n",
							  NICAddr.Node[0], NICAddr.Node[1], NICAddr.Node[2],
							  NICAddr.Node[3], NICAddr.Node[4], NICAddr.Node[5],
							  NTOHS(NICAddr.Port)) );
		
		if (!NT_SUCCESS(status)) {

			if (status != 0xC001001FL/*NDIS_STATUS_NO_CABLE*/)
				ASSERT( NDASNTFS_UNEXPECTED );

			continue;
		}
		
		status = LpxTdiOpenConnection( &connectionFileHandle, &connectionFileObject, NULL );
	
		if (!NT_SUCCESS(status)) {

			ASSERT(NDASNTFS_UNEXPECTED);
			LpxTdiCloseAddress (addressFileHandle, addressFileObject);
			addressFileHandle = NULL; addressFileObject = NULL;
			continue;
		}

		status = LpxTdiAssociateAddress( connectionFileObject, addressFileHandle );
	

		if (!NT_SUCCESS(status)) {

			ASSERT( NDASNTFS_UNEXPECTED );
			LpxTdiCloseAddress( addressFileHandle, addressFileObject );
			addressFileHandle = NULL; addressFileObject = NULL;
			LpxTdiCloseConnection( connectionFileHandle, connectionFileObject );
			connectionFileHandle = NULL; connectionFileObject = NULL;
			continue;
		}
	
		serverAddress = &Secondary->PrimaryAddress;

		DebugTrace( 0, Dbg, ("Connect to %02X:%02X:%02X:%02X:%02X:%02X 0x%4X\n",
							  serverAddress->Node[0], serverAddress->Node[1], serverAddress->Node[2],
							  serverAddress->Node[3], serverAddress->Node[4], serverAddress->Node[5],
							  NTOHS(serverAddress->Port)) );

		status = LpxTdiConnect( connectionFileObject, serverAddress );

		if (status == STATUS_SUCCESS)
			break;

		DebugTrace( 0, Dbg, ("Connection is Failed\n") );

		LpxTdiDisassociateAddress( connectionFileObject );
		LpxTdiCloseAddress( addressFileHandle, addressFileObject );
		addressFileHandle = NULL; addressFileObject = NULL;
		LpxTdiCloseConnection( connectionFileHandle, connectionFileObject );
		connectionFileHandle = NULL; connectionFileObject = NULL;
	}

	if (connectionFileHandle == NULL) {

		return STATUS_DEVICE_NOT_CONNECTED;
	}

	DebugTrace( 0, Dbg, ("Connection is Success connectionFileObject = %p\n", connectionFileObject));

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
							  NULL);

		if (status != STATUS_SUCCESS) {

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

			ASSERT( NDASNTFS_BUG );
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

			ASSERT( FALSE );
			DereferenceSecondaryRequest( secondaryRequest );
			break;
		}

		if(ndfsReplyNegotiate->Status != NDFS_NEGOTIATE_SUCCESS)
		{
			ASSERT(NDASNTFS_BUG);
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
		
		if(ndfsReplyNegotiate->MaxBufferSize) {

			Secondary->Thread.SessionContext.PrimaryMaxDataSize
				= (ndfsReplyNegotiate->MaxBufferSize <= DEFAULT_NDAS_MAX_DATA_SIZE)
					? ndfsReplyNegotiate->MaxBufferSize : DEFAULT_NDAS_MAX_DATA_SIZE;

			Secondary->Thread.SessionContext.PrimaryMaxDataSize = DEFAULT_NDAS_MAX_DATA_SIZE;
		}
		
		DereferenceSecondaryRequest(secondaryRequest);
			
		secondaryRequest = AllocSecondaryRequest(
							Secondary,
							(sizeof(NDFS_REQUEST_HEADER)+sizeof(NDFS_REQUEST_SETUP) > sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_SETUP))
							? (sizeof(NDFS_REQUEST_HEADER)+sizeof(NDFS_REQUEST_SETUP)) : (sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_SETUP)),
							FALSE
							);

		if(secondaryRequest == NULL) {

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
					   Secondary->VolDo->NetdiskPartitionInformation.NetdiskInformation.NetDiskAddress.Node,
					   6 );

		ndfsRequestSetup->NetDiskPort = HTONS(Secondary->VolDo->NetdiskPartitionInformation.NetdiskInformation.NetDiskAddress.Port);
		ndfsRequestSetup->UnitDiskNo = Secondary->VolDo->NetdiskPartitionInformation.NetdiskInformation.UnitDiskNo;

		RtlCopyMemory( ndfsRequestSetup->NdscId, 
					   Secondary->VolDo->NetdiskPartitionInformation.NetdiskInformation.NdscId, 
					   NDSC_ID_LENGTH );


		ndfsRequestSetup->StartingOffset = 0;
		ndfsRequestSetup->ResponseLength = 0;

		{
			unsigned char		idData[1];
			MD5_CTX				context;


			MD5Init(&context);

			/* id byte */
			idData[0] = (unsigned char)Secondary->Thread.SessionContext.SessionKey;
			MD5Update(&context, idData, 1);

			MD5Update(&context, Secondary->VolDo->NetdiskPartitionInformation.NetdiskInformation.Password, 8);			
			MD5Update(&context, Secondary->Thread.SessionContext.ChallengeBuffer, Secondary->Thread.SessionContext.ChallengeLength);
			MD5Final(&context);

			ndfsRequestSetup->ResponseLength = 16;
			RtlCopyMemory( ndfsRequestSetup->ResponseBuffer, context.digest, 16 );
		}

		status = SendMessage( connectionFileObject,
							  secondaryRequest->NdfsMessage,
							  ndfsRequestHeader->MessageSize,
							  NULL );

		if (status != STATUS_SUCCESS) {

			ASSERT( FALSE );
			DereferenceSecondaryRequest( secondaryRequest );
			break;
		}

		status = RecvMessage( connectionFileObject,
							  secondaryRequest->NdfsMessage,
							  sizeof(NDFS_REPLY_HEADER),
							  NULL);
	
		if (status != STATUS_SUCCESS) {

			ASSERT( FALSE );
			DereferenceSecondaryRequest( secondaryRequest );
			break;
		}

		ndfsReplyHeader = &secondaryRequest->NdfsReplyHeader;
		
		if (!(RtlEqualMemory(ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol)) && 
			  ndfsReplyHeader->Status == NDFS_SUCCESS)) {

			ASSERT(NDASNTFS_BUG);
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
		
		if(ndfsReplySetup->Status != NDFS_SETUP_SUCCESS) {

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

		ndfsRequestTreeConnect->StartingOffset = Secondary->VolDo->NetdiskPartitionInformation.PartitionInformationEx.StartingOffset.QuadPart;

		status = SendMessage( connectionFileObject,
							  secondaryRequest->NdfsMessage,
							  ndfsRequestHeader->MessageSize,
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
				
			ASSERT(NDASNTFS_BUG);
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

		DebugTrace( 0, Dbg, ("NDFS_COMMAND_TREE_CONNECT: ndfsReplyTreeConnect->Status = %x\n", 
							  ndfsReplyTreeConnect->Status) );

		if (ndfsReplyTreeConnect->Status == NDFS_TREE_CORRUPTED || 
			ndfsReplyTreeConnect->Status == NDFS_TREE_CONNECT_NO_PARTITION) {

			if (ndfsReplyTreeConnect->Status == NDFS_TREE_CORRUPTED && sendCount <= 4 || 
				ndfsReplyTreeConnect->Status == NDFS_TREE_CONNECT_NO_PARTITION && sendCount <= 12) {

				LARGE_INTEGER	timeOut;
				KEVENT			nullEvent;
				NTSTATUS		waitStatus;
				
				KeInitializeEvent( &nullEvent, NotificationEvent, FALSE );
				timeOut.QuadPart = -10*HZ;		// 10 sec

				waitStatus = KeWaitForSingleObject( &nullEvent,
													Executive,
													KernelMode,
													FALSE,
													&timeOut );
	
				ASSERT( waitStatus == STATUS_TIMEOUT );

				DereferenceSecondaryRequest( secondaryRequest );
				goto SEND_TREE_CONNECT_COMMAND;	
			}
				
			if (ndfsReplyTreeConnect->Status == NDFS_TREE_CONNECT_NO_PARTITION)
				ASSERT(NDNTFS_REQUIRED);

			status = STATUS_DISK_CORRUPT_ERROR;
			DereferenceSecondaryRequest( secondaryRequest );
	
			break;
			
		} else if (ndfsReplyTreeConnect->Status == NDFS_TREE_CONNECT_UNSUCCESSFUL) {
				
			//ASSERT(NDASNTFS_BUG);
			DereferenceSecondaryRequest( secondaryRequest );
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		ASSERT(ndfsReplyTreeConnect->Status == NDFS_TREE_CONNECT_SUCCESS);

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

		DebugTrace( 0, Dbg, ("NDFS_COMMAND_TREE_CONNECT: ndfsReplyTreeConnect->BytesPerSector = %d, ndfsReplyTreeConnect->BytesPerCluster = %d\n", 
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
DisconnectFromPrimary(
	IN	PSECONDARY	Secondary
   )
{
	NTSTATUS			status;


	DebugTrace( 0, Dbg2, ("DisconnectFromPrimary: Entered\n"));

	ASSERT(Secondary->Thread.AddressFileHandle); 
	ASSERT(Secondary->Thread.AddressFileObject);
	ASSERT(Secondary->Thread.ConnectionFileHandle);
	ASSERT(Secondary->Thread.ConnectionFileObject); 
	
	do // just for structural programing
	{
		PSECONDARY_REQUEST		secondaryRequest;
		PNDFS_REQUEST_HEADER	ndfsRequestHeader;
		PNDFS_REQUEST_LOGOFF	ndfsRequestLogoff;
		PNDFS_REPLY_HEADER		ndfsReplyHeader;
		PNDFS_REPLY_LOGOFF		ndfsReplyLogoff;
		NTSTATUS				openStatus;


		secondaryRequest = AllocSecondaryRequest(
							Secondary,
							(sizeof(NDFS_REQUEST_HEADER)+sizeof(NDFS_REQUEST_LOGOFF) > sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_LOGOFF))
							? (sizeof(NDFS_REQUEST_HEADER)+sizeof(NDFS_REQUEST_LOGOFF)) : (sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_LOGOFF)),
							FALSE
							);

		if(secondaryRequest == NULL)
		{
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

		openStatus = SendMessage(
						Secondary->Thread.ConnectionFileObject,
						secondaryRequest->NdfsMessage,
						ndfsRequestHeader->MessageSize,
						NULL
						);

		if(openStatus != STATUS_SUCCESS)
		{
			status = openStatus;
			DereferenceSecondaryRequest(secondaryRequest);
			break;
		}

		openStatus = RecvMessage(
						Secondary->Thread.ConnectionFileObject,
						secondaryRequest->NdfsMessage,
						sizeof(NDFS_REPLY_HEADER),
						NULL
						);
	
		if(openStatus != STATUS_SUCCESS)
		{
			status = openStatus;
			DereferenceSecondaryRequest(secondaryRequest);
			break;
		}

		ndfsReplyHeader = &secondaryRequest->NdfsReplyHeader;
		
		if(!(
			RtlEqualMemory(ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol))
			&& ndfsReplyHeader->Status == NDFS_SUCCESS
			))
		{
			ASSERT(NDASNTFS_BUG);
			DereferenceSecondaryRequest(secondaryRequest);
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		ndfsReplyLogoff = (PNDFS_REPLY_LOGOFF)(ndfsReplyHeader+1);
		ASSERT(ndfsReplyLogoff == (PNDFS_REPLY_LOGOFF)secondaryRequest->NdfsReplyData);

		openStatus = RecvMessage(
						Secondary->Thread.ConnectionFileObject,
						(_U8 *)ndfsReplyLogoff,
						sizeof(NDFS_REPLY_LOGOFF),
						NULL
						);

		if(openStatus != STATUS_SUCCESS)
		{
			status = openStatus;
			DereferenceSecondaryRequest(secondaryRequest);
			break;
		}
		
		if(ndfsReplyLogoff->Status != NDFS_LOGOFF_SUCCESS)
		{
			ASSERT(NDASNTFS_BUG);
			DereferenceSecondaryRequest(
				secondaryRequest
				);
			status = STATUS_INVALID_PARAMETER;
			break;
		}
	
		DereferenceSecondaryRequest(
				secondaryRequest
			);

		status = STATUS_SUCCESS;

	} while(0);
	
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
		
	return status;
}


NTSTATUS
SendNdfsRequestMessage(
	IN	PSECONDARY			Secondary,
	IN  PSECONDARY_REQUEST	SecondaryRequest
	)
{
	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	_U8							*encryptedMessage;
	_U32						requestDataSize;

#if __NDAS_NTFS_DES__
	DES_CBC_CTX					desCbcContext;
	unsigned char				iv[8];
	int							desResult;
#endif

	NTSTATUS					tdiStatus;
	_U32						remaninigDataSize;


	ASSERT(!BooleanFlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_ERROR));

	ndfsRequestHeader = &SecondaryRequest->NdfsRequestHeader;

	ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
	requestDataSize = ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER);
	encryptedMessage = (_U8 *)ndfsRequestHeader + ndfsRequestHeader->MessageSize;

	ASSERT(ndfsRequestHeader->MessageSize >= sizeof(NDFS_REQUEST_HEADER));

	ASSERT(ndfsRequestHeader->Uid == Secondary->Thread.SessionContext.Uid);
	ASSERT(ndfsRequestHeader->Tid == Secondary->Thread.SessionContext.Tid);
	
	//ASSERT(requestDataSize <= Secondary->Thread.SessionContext.PrimaryMaxBufferSize && ndfsRequestHeader->MessageSecurity == 0);

	DebugTrace( 0, Dbg, ("ndfsWinxpRequestHeader->IrpMajorFunction = %d\n", ndfsWinxpRequestHeader->IrpMajorFunction) );

	if(requestDataSize <= Secondary->Thread.SessionContext.PrimaryMaxDataSize)
	{
		if(ndfsRequestHeader->MessageSecurity == 0)
		{
			tdiStatus = SendMessage(
							Secondary->Thread.ConnectionFileObject,
							(_U8 *)ndfsRequestHeader,
							ndfsRequestHeader->MessageSize,
							NULL
							);

			if(tdiStatus != STATUS_SUCCESS)
				return STATUS_REMOTE_DISCONNECT;
			else
				return STATUS_SUCCESS;
		}


#if __NDAS_NTFS_DES__

		ASSERT(ndfsRequestHeader->MessageSecurity == 1);
		
		RtlZeroMemory(&desCbcContext, sizeof(desCbcContext));
		RtlZeroMemory(iv, sizeof(iv));
		DES_CBCInit(&desCbcContext, Secondary->VolDo->NetdiskInformation.Password, iv, DES_ENCRYPT);
		
		if(ndfsWinxpRequestHeader->IrpMajorFunction == IRP_MJ_WRITE)
			DebugTrace( 0, Dbg,
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
							NULL
							);
		}
		else
		{
			tdiStatus = SendMessage(
							Secondary->Thread.ConnectionFileObject,
							(_U8 *)ndfsRequestHeader,
							sizeof(NDFS_REQUEST_HEADER),
							NULL
							);

			if(tdiStatus != STATUS_SUCCESS)
				return STATUS_REMOTE_DISCONNECT;
			
			desResult = DES_CBCUpdate(&desCbcContext, encryptedMessage, (_U8 *)ndfsWinxpRequestHeader, ndfsRequestHeader->MessageSize-sizeof(NDFS_REQUEST_HEADER));
			ASSERT(desResult == IDOK);
			tdiStatus = SendMessage(
							Secondary->Thread.ConnectionFileObject,
							encryptedMessage,
							SecondaryRequest->NdfsRequestHeader.MessageSize - sizeof(NDFS_REQUEST_HEADER),
							NULL
							);
		}

		if(tdiStatus != STATUS_SUCCESS)
			return STATUS_REMOTE_DISCONNECT;
		else
			return STATUS_SUCCESS;
#endif
	
	}


	ASSERT(requestDataSize > Secondary->Thread.SessionContext.PrimaryMaxDataSize);	

	ndfsRequestHeader->Splitted = 1;

#if __NDAS_NTFS_DES__

	if(ndfsRequestHeader->MessageSecurity)
	{
		RtlZeroMemory(&desCbcContext, sizeof(desCbcContext));
		RtlZeroMemory(iv, sizeof(iv));
		DES_CBCInit(&desCbcContext, Secondary->VolDo->NetdiskInformation.Password, iv, DES_ENCRYPT);
		
		desResult = DES_CBCUpdate(&desCbcContext, encryptedMessage, (_U8 *)ndfsWinxpRequestHeader, sizeof(NDFS_WINXP_REQUEST_HEADER));
		ASSERT(desResult == IDOK);

		RtlCopyMemory(
			ndfsWinxpRequestHeader,
			encryptedMessage,
			sizeof(NDFS_WINXP_REQUEST_HEADER)
			);
	}
#endif

	tdiStatus = SendMessage(
					Secondary->Thread.ConnectionFileObject,
					(_U8 *)ndfsRequestHeader,
					sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER),
					NULL
					);

	if(tdiStatus != STATUS_SUCCESS)
		return STATUS_REMOTE_DISCONNECT;
	
	remaninigDataSize = requestDataSize;

	while(1)
	{
		ndfsRequestHeader->MessageSize = sizeof(NDFS_REQUEST_HEADER) + remaninigDataSize;
		if(remaninigDataSize <= Secondary->Thread.SessionContext.PrimaryMaxDataSize)
			ndfsRequestHeader->Splitted = 0;
		else
			ndfsRequestHeader->Splitted = 1;

		tdiStatus = SendMessage(
						Secondary->Thread.ConnectionFileObject,
						(_U8 *)ndfsRequestHeader,
						sizeof(NDFS_REQUEST_HEADER),
						NULL
						);

		if(tdiStatus != STATUS_SUCCESS)
			return STATUS_REMOTE_DISCONNECT;

#if __NDAS_NTFS_DES__
		
		if(ndfsRequestHeader->MessageSecurity)
		{
			desResult = DES_CBCUpdate(
							&desCbcContext, 
							encryptedMessage, 
							(_U8 *)(ndfsWinxpRequestHeader+1) + (requestDataSize - remaninigDataSize), 
							ndfsRequestHeader->Splitted ? Secondary->Thread.SessionContext.PrimaryMaxDataSize : remaninigDataSize
							);
			ASSERT(desResult == IDOK);

			tdiStatus = SendMessage(
						Secondary->Thread.ConnectionFileObject,
						encryptedMessage,
						ndfsRequestHeader->Splitted ? Secondary->Thread.SessionContext.PrimaryMaxDataSize : remaninigDataSize,
						NULL
						);
		}
		else
#endif
		{
			tdiStatus = SendMessage(
						Secondary->Thread.ConnectionFileObject,
						(_U8 *)(ndfsWinxpRequestHeader+1) + (requestDataSize - remaninigDataSize),
						ndfsRequestHeader->Splitted ? Secondary->Thread.SessionContext.PrimaryMaxDataSize : remaninigDataSize,
						NULL
						);
		}
			
		if(tdiStatus != STATUS_SUCCESS)
			return STATUS_REMOTE_DISCONNECT;

		if(ndfsRequestHeader->Splitted)
			remaninigDataSize -= Secondary->Thread.SessionContext.PrimaryMaxDataSize;
		else
			return STATUS_SUCCESS;

		ASSERT((_S32)remaninigDataSize > 0);
	}	
}
		

NTSTATUS
ReceiveNdfsReplyMessage(
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

		ASSERT( NDASNTFS_BUG );
		return STATUS_UNSUCCESSFUL;
	}

	NDASNTFS_ASSERT( ndfsReplyHeader->MessageSize <= SecondaryRequest->NdfsMessageAllocationSize );
	
	//ASSERT(ndfsReplyHeader->Splitted == 0 && ndfsReplyHeader->MessageSecurity == 0);
	
	if (ndfsReplyHeader->Splitted == 0) {

		ndfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)(ndfsReplyHeader+1);
		ASSERT(ndfsWinxpReplyHeader == (PNDFS_WINXP_REPLY_HEADER)SecondaryRequest->NdfsReplyData);
		encryptedMessage = (_U8 *)ndfsReplyHeader + ndfsReplyHeader->MessageSize;

		if (ndfsReplyHeader->MessageSecurity == 0) {

			tdiStatus = RecvMessage( Secondary->Thread.ConnectionFileObject,
									 (_U8 *)ndfsWinxpReplyHeader,
									 ndfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER),
									 NULL );

			if (tdiStatus != STATUS_SUCCESS) {

				return STATUS_REMOTE_DISCONNECT;
			}
		}

#if __NDAS_FAT_DES__
		else
		{
			int desResult;

			tdiStatus = RecvMessage(
							Secondary->Thread.ConnectionFileObject,
							encryptedMessage,
							sizeof(NDFS_WINXP_REPLY_HEADER),
							NULL
							);
			if (tdiStatus != STATUS_SUCCESS)
			{
				return STATUS_REMOTE_DISCONNECT;
			}

			RtlZeroMemory(&SecondaryRequest->DesCbcContext, sizeof(SecondaryRequest->DesCbcContext));
			RtlZeroMemory(SecondaryRequest->Iv, sizeof(SecondaryRequest->Iv));
			DES_CBCInit(&SecondaryRequest->DesCbcContext, Secondary->VolDo->NetdiskInformation.Password, SecondaryRequest->Iv, DES_DECRYPT);
			desResult = DES_CBCUpdate(&SecondaryRequest->DesCbcContext, (_U8 *)ndfsWinxpReplyHeader, encryptedMessage, sizeof(NDFS_WINXP_REPLY_HEADER));
			ASSERT(desResult == IDOK);
			if (ndfsWinxpReplyHeader->IrpMajorFunction == IRP_MJ_READ)
				DebugTrace( 0, Dbg,
					("ProcessSecondaryRequest: ndfsReplyHeader->RwDataSecurity = %d\n", ndfsReplyHeader->RwDataSecurity));

			if (ndfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER))
			{
				if (ndfsWinxpReplyHeader->IrpMajorFunction == IRP_MJ_READ && ndfsReplyHeader->RwDataSecurity == 0)
				{
					tdiStatus = RecvMessage(
								Secondary->Thread.ConnectionFileObject,
								(_U8 *)(ndfsWinxpReplyHeader+1),
								ndfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER),
								NULL
								);
					if (tdiStatus != STATUS_SUCCESS)
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

					if (tdiStatus != STATUS_SUCCESS)
					{
						return STATUS_REMOTE_DISCONNECT;
					}
					desResult = DES_CBCUpdate(&SecondaryRequest->DesCbcContext, (_U8 *)(ndfsWinxpReplyHeader+1), encryptedMessage, ndfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER));
					ASSERT(desResult == IDOK);
				}
			}
		}
#endif
		
		return STATUS_SUCCESS;
	}

	ASSERT( ndfsReplyHeader->Splitted == 1 );
		
	ndfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)(ndfsReplyHeader+1);
	ASSERT( ndfsWinxpReplyHeader == (PNDFS_WINXP_REPLY_HEADER)SecondaryRequest->NdfsReplyData );

#if __NDAS_FAT_DES__
	if (ndfsReplyHeader->MessageSecurity)
	{
		int desResult;

		encryptedMessage = (_U8 *)ndfsReplyHeader + ndfsReplyHeader->MessageSize;
	
		tdiStatus = RecvMessage(
						Secondary->Thread.ConnectionFileObject,
						encryptedMessage,
						sizeof(NDFS_WINXP_REPLY_HEADER),
						NULL
						);
		if (tdiStatus != STATUS_SUCCESS)
		{
			return STATUS_REMOTE_DISCONNECT;
		}

		RtlZeroMemory(&SecondaryRequest->DesCbcContext, sizeof(SecondaryRequest->DesCbcContext));
		RtlZeroMemory(SecondaryRequest->Iv, sizeof(SecondaryRequest->Iv));
		DES_CBCInit(&SecondaryRequest->DesCbcContext, Secondary->VolDo->NetdiskInformation.Password, SecondaryRequest->Iv, DES_DECRYPT);
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

	while(1) {

		PNDFS_REPLY_HEADER	splitNdfsReplyHeader = &Secondary->Thread.SplitNdfsReplyHeader;
		
		tdiStatus = RecvMessage( Secondary->Thread.ConnectionFileObject,
								 (_U8 *)splitNdfsReplyHeader,
								 sizeof(NDFS_REPLY_HEADER),
								 NULL );

		if (tdiStatus != STATUS_SUCCESS) {

			return STATUS_REMOTE_DISCONNECT;
		}
			
		DebugTrace( 0, Dbg,
					("ndfsReplyHeader->MessageSize = %d splitNdfsReplyHeader.MessageSize = %d\n", 
					ndfsReplyHeader->MessageSize, splitNdfsReplyHeader->MessageSize) );

		if (!(RtlEqualMemory(ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol)) && 
			ndfsReplyHeader->Status == NDFS_SUCCESS)) {

			ASSERT( NDASNTFS_BUG );
			return STATUS_UNSUCCESSFUL;
		}

#if __NDAS_NTFS_DES__
		if (splitNdfsReplyHeader->MessageSecurity) {

			int desResult;

			tdiStatus = RecvMessage(
							Secondary->Thread.ConnectionFileObject,
							encryptedMessage,
							splitNdfsReplyHeader->Splitted ? Secondary->Thread.SessionContext.SecondaryMaxDataSize : (splitNdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER)),
							NULL
							);

			if (tdiStatus != STATUS_SUCCESS)
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
									 splitNdfsReplyHeader->Splitted ? Secondary->Thread.SessionContext.SecondaryMaxDataSize : (splitNdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER)),
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


#if 0

BOOLEAN
NtfsLookupInFileRecord(
	IN  PSECONDARY					Secondary,
	IN  PFILE_RECORD_SEGMENT_HEADER	FileRecordSegmentHeader,
    IN  ATTRIBUTE_TYPE_CODE			QueriedTypeCode,
	IN	PATTRIBUTE_RECORD_HEADER	CurrentAttributeRecordHeader,
	OUT PATTRIBUTE_RECORD_HEADER	*AttributeRecordHeader
	)
{
	ULONG	attributeOffset;

	if(CurrentAttributeRecordHeader)
	{
		attributeOffset = (ULONG)CurrentAttributeRecordHeader - (ULONG)FileRecordSegmentHeader;
		attributeOffset += CurrentAttributeRecordHeader->RecordLength;
	}
	else
		attributeOffset = FileRecordSegmentHeader->FirstAttributeOffset;

	do
	{
		PATTRIBUTE_RECORD_HEADER	attributeRecordHeader;

		if(attributeOffset > Secondary->Thread.SessionContext.BytesPerFileRecordSegment)
		{
			ASSERT(NDASNTFS_UNEXPECTED);
			return FALSE;
		}
		attributeRecordHeader = (PATTRIBUTE_RECORD_HEADER)((PCHAR)FileRecordSegmentHeader + attributeOffset);
				
		DebugTrace( 0,Dbg,
			("\nattributeRecordHeader->TypeCode %8x %s\n", 
				attributeRecordHeader->TypeCode, 
				(attributeRecordHeader->TypeCode == $END) ? "$END                   " : AttributeTypeCode[attributeRecordHeader->TypeCode>>4]));

		if(attributeRecordHeader->TypeCode == $END)
			break;

		if(attributeRecordHeader->TypeCode == QueriedTypeCode)
		{
			if(AttributeRecordHeader)
				*AttributeRecordHeader = attributeRecordHeader;
			
			return TRUE;
		}
	
		if(attributeRecordHeader->RecordLength == 0)
		{
			ASSERT(NDASNTFS_UNEXPECTED);
			return FALSE;
		}
		attributeOffset += attributeRecordHeader->RecordLength;

	} while(1);

	return FALSE;
}


#define IsCharZero(C)    (((C) & 0x000000ff) == 0x00000000)
#define IsCharMinus1(C)  (((C) & 0x000000ff) == 0x000000ff)
#define IsCharLtrZero(C) (((C) & 0x00000080) == 0x00000080)
#define IsCharGtrZero(C) (!IsCharLtrZero(C) && !IsCharZero(C))

#define ClustersFromBytes(ClusterShift, ClusterMask, Bytes) (	\
    (((ULONG)(Bytes)) + ClusterMask) >> ClusterShift			\
)

#define ClustersFromBytesTruncate(ClusterShift, Bytes) (    \
    ((ULONG)(Bytes)) >> ClusterShift						\
)

#define LlClustersFromBytes(ClusterShift, ClusterMask, Bytes) (		\
    Int64ShllMod32(Bytes + (LONGLONG)ClusterMask, ClusterShift)		\
)

#define LlClustersFromBytesTruncate(ClusterShift, Bytes) (  \
    Int64ShraMod32(Bytes, ClusterShift)						\
)

#define Add2Ptr(P,I) ((PVOID)((PUCHAR)(P) + (I)))

#define PtrOffset(B,O) ((ULONG)((ULONG_PTR)(O) - (ULONG_PTR)(B)))

BOOLEAN
LookupAllocation(
	IN	PSECONDARY					Secondary,
	IN  PATTRIBUTE_RECORD_HEADER	AttributeRecordHeader,	
    IN  VCN							Vcn,
	OUT	PVCN						StartVcn,
    OUT PLCN						Lcn,
    OUT PLONGLONG					ClusterCount
	)
{
	BOOLEAN		returnResult;
    LONGLONG	allocationClusters;
    VCN			highestCandidate;

	VCN			currentVcn;
	LCN			currentLcn;
    PCHAR		ch;


	allocationClusters = LlClustersFromBytesTruncate(
							Secondary->Thread.SessionContext.ClusterShift,
							AttributeRecordHeader->Form.Nonresident.AllocatedLength 
							);
	
	if ((AttributeRecordHeader->Form.Nonresident.LowestVcn < 0) 
			|| (AttributeRecordHeader->Form.Nonresident.LowestVcn - 1 > AttributeRecordHeader->Form.Nonresident.HighestVcn) 
			|| (Vcn < AttributeRecordHeader->Form.Nonresident.LowestVcn) 
			|| (AttributeRecordHeader->Form.Nonresident.HighestVcn >= allocationClusters)) 
	{
		// NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Scb->Fcb );
		return FALSE;
    }


	returnResult = FALSE;
	highestCandidate = AttributeRecordHeader->Form.Nonresident.LowestVcn;
	currentLcn = 0;
    ch = (PCHAR)AttributeRecordHeader + AttributeRecordHeader->Form.Nonresident.MappingPairsOffset;

	while (!IsCharZero(*ch)) 
	{
	    ULONG		vcnBytes;
		ULONG		lcnBytes;
		LONGLONG	change;
		
		
		currentVcn = highestCandidate;

		if (currentVcn < 0) 
		{
			ASSERT(NDASNTFS_UNEXPECTED);
			//NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Scb->Fcb );
			return FALSE;
		}

		vcnBytes = *ch & 0xF;
        lcnBytes = *ch++ >> 4;

		change = 0;

		if (((ULONG)(vcnBytes - 1) > 7) || (lcnBytes > 8) 
			|| ((ch + vcnBytes + lcnBytes + 1) > (PCHAR)Add2Ptr(AttributeRecordHeader, AttributeRecordHeader->RecordLength)) 
			|| IsCharLtrZero(*(ch + vcnBytes - 1))) 
		{
			ASSERT(NDASNTFS_UNEXPECTED);
			//NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Scb->Fcb );
			return FALSE;
		}
            
		RtlCopyMemory(&change, ch, vcnBytes);
		ch += vcnBytes;
		highestCandidate = highestCandidate + change;

		if (lcnBytes != 0) 
		{
			change = 0;
            if (IsCharLtrZero(*(ch+lcnBytes-1))) 
			{
				change = change - 1; //negative
			}
            RtlCopyMemory(&change, ch, lcnBytes);
            ch += lcnBytes;
            currentLcn = currentLcn + change;
		}

		if (Vcn < highestCandidate) 
		{
			if(lcnBytes == 0 || currentLcn == UNUSED_LCN)
			{
				returnResult = FALSE;
			}
			else
			{
				*Lcn = currentLcn;
				*StartVcn = currentVcn;
				*ClusterCount = highestCandidate - currentVcn;
				returnResult = TRUE;
			}
    
			break;
        }
	}

    return returnResult;
}


NTSTATUS
ReadBuffer(
	IN  PSECONDARY		Secondary,
	IN  PLARGE_INTEGER	StartingOffset,
	IN  ULONG			Length,
	OUT PCHAR			OutputBuffer
	)
{
	PIRP			irp;
	KEVENT			event;
	IO_STATUS_BLOCK	ioStatusBlock;
	NTSTATUS		status;


	KeInitializeEvent(&event, NotificationEvent, FALSE);
	RtlZeroMemory(&ioStatusBlock, sizeof(IO_STATUS_BLOCK));

	irp = IoBuildSynchronousFsdRequest(
			IRP_MJ_READ,
			Secondary->VolDo->DiskDeviceObject,
			OutputBuffer,
			Length,
			StartingOffset,
			&event,
			&ioStatusBlock
		    );

    if (irp == NULL) 
	{
		ASSERT(NDASNTFS_INSUFFICIENT_RESOURCES);
		return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = IoCallDriver(Secondary->VolDo->DiskDeviceObject, irp);

    if (status == STATUS_PENDING) 
	{
		KeWaitForSingleObject( 
			&event,
            Executive,
            KernelMode,
            FALSE,
            (PLARGE_INTEGER)NULL 
			);

        status = ioStatusBlock.Status;
    }

    return status;
}


NTSTATUS
DirectProcessSecondaryRequest(
	IN	PSECONDARY			Secondary,
	IN  PSECONDARY_REQUEST	SecondaryRequest
	)
{
	NTSTATUS					status = DBG_CONTINUE; // for debbuging
	IO_STATUS_BLOCK				ioStatusBlock;

	// ndfsWinxpRequestHeader can be updated by outputBuffer Must be careful
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)SecondaryRequest->NdfsRequestData;
	_U8							*outputBuffer = (_U8*)SecondaryRequest->NdfsMessage + sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER); 		

	_U32						irpTag			 = ndfsWinxpRequestHeader->IrpTag;
	_U8							irpMajorFunction = ndfsWinxpRequestHeader->IrpMajorFunction;
	_U8							irpMinorFunction = ndfsWinxpRequestHeader->IrpMinorFunction;
	_U32						irpFlags		 = ndfsWinxpRequestHeader->IrpFlags;
	
	PCCB				fileExt = SecondaryRequest->FileExt;
	PNDNTFS_FCB					fcb = fileExt->Fcb;

	UNREFERENCED_PARAMETER(Secondary);

	if(fcb->FileRecordSegmentHeaderAvail == FALSE)
		return STATUS_UNSUCCESSFUL;

	switch(irpMajorFunction)
	{
	case IRP_MJ_READ:
	{
		struct Read					read;
		BOOLEAN						lookupResult;
		PATTRIBUTE_RECORD_HEADER	attributeRecordHeader;	

		PSTANDARD_INFORMATION		standardInformation;
		LARGE_INTEGER				lastUpdateTime;

		LARGE_INTEGER				currentByteOffset;
	
		BOOLEAN						pagingIo = BooleanFlagOn(irpFlags, IRP_PAGING_IO);
		BOOLEAN						nonCachedIo = BooleanFlagOn(irpFlags,IRP_NOCACHE);


		read.Length = ndfsWinxpRequestHeader->Read.Length;
		read.Key = ndfsWinxpRequestHeader->Read.Key;
		read.ByteOffset.QuadPart = ndfsWinxpRequestHeader->Read.ByteOffset;

		ioStatusBlock.Information = 0;
		ioStatusBlock.Status = STATUS_SUCCESS;

		if(fileExt->TypeOfOpen != SecondaryUserFileOpen)
		{
			status = STATUS_UNSUCCESSFUL;
			break;
		}
		if(read.Length == 0)
		{
			status = STATUS_UNSUCCESSFUL;
			break;
		}
		if(read.Key != 0)
		{
			status = STATUS_UNSUCCESSFUL;
			break;
		}

		lookupResult = 	NtfsLookupInFileRecord(
							Secondary,
							(PFILE_RECORD_SEGMENT_HEADER)fcb->Buffer,
							$STANDARD_INFORMATION,
							NULL,
							&attributeRecordHeader
							);

		DebugTrace( 0, Dbg,
					("DirectProcessSecondaryRequest: IRP_MJ_READ LookupAllocation lookupResult = %d\n", lookupResult));
		
		if(lookupResult == FALSE || attributeRecordHeader->FormCode != RESIDENT_FORM)
		{
			ASSERT(NDASNTFS_UNEXPECTED);
			status = STATUS_UNSUCCESSFUL;
			break;
		}

		standardInformation = (PSTANDARD_INFORMATION)((PCHAR)attributeRecordHeader + attributeRecordHeader->Form.Resident.ValueOffset);
		
		//lastUpdateTime.QuadPart 
		//	= (standardInformation->LastModificationTime > standardInformation->LastChangeTime)
		//	? standardInformation->LastModificationTime : standardInformation->LastChangeTime;

		lastUpdateTime.QuadPart = standardInformation->LastModificationTime;

		//ASSERT(fcb->OpenTime.QuadPart >= ((lastUpdateTime.QuadPart >> 24) << 24)); // It can be occured
			
		if(fcb->OpenTime.QuadPart < lastUpdateTime.QuadPart
			|| (fcb->OpenTime.QuadPart - lastUpdateTime.QuadPart) < 600*HZ)
		{
			status = STATUS_UNSUCCESSFUL;
			break;
		}

		lookupResult = 	NtfsLookupInFileRecord(
							Secondary,
							(PFILE_RECORD_SEGMENT_HEADER)fcb->Buffer,
							$DATA,
							NULL,
							&attributeRecordHeader
							);

		DebugTrace( 0, Dbg,
					("DirectProcessSecondaryRequest: IRP_MJ_READ LookupAllocation lookupResult = %d\n", lookupResult)); 		

		if(lookupResult == FALSE)
		{
			//ASSERT(NDASNTFS_UNEXPECTED);
			status = STATUS_UNSUCCESSFUL;
			break;
		}
		
		if(NtfsLookupInFileRecord(
						Secondary, 
						(PFILE_RECORD_SEGMENT_HEADER)fcb->Buffer,
						$DATA, attributeRecordHeader, NULL) == TRUE
						)
		{
			status = STATUS_UNSUCCESSFUL;
			break;
		}

		currentByteOffset.QuadPart = read.ByteOffset.QuadPart;

		if(attributeRecordHeader->FormCode == RESIDENT_FORM)
		{
			PCHAR	fileData;
			ULONG	remainFileLength;
			ULONG	readLength;

			
			if(BooleanFlagOn(attributeRecordHeader->Form.Resident.ResidentFlags, RESIDENT_FORM_INDEXED))
			{
				status = STATUS_UNSUCCESSFUL;
				break;
			}

			if(read.ByteOffset.HighPart != 0)
			{
				ASSERT(NDASNTFS_UNEXPECTED);
				status = STATUS_UNSUCCESSFUL;
			}

			if(attributeRecordHeader->Form.Resident.ValueLength == 0)
			{
				status = STATUS_UNSUCCESSFUL;
				break;
			}
			
			if(attributeRecordHeader->Form.Resident.ValueLength <= read.ByteOffset.LowPart)
			{	
				ioStatusBlock.Information = 0;
				ioStatusBlock.Status = STATUS_END_OF_FILE;
				status = STATUS_SUCCESS;
				break;
			}
			
			fileData = (PCHAR)attributeRecordHeader + attributeRecordHeader->Form.Resident.ValueOffset;
			fileData += read.ByteOffset.LowPart;

			remainFileLength = attributeRecordHeader->Form.Resident.ValueLength - read.ByteOffset.LowPart;

			readLength = (read.Length <= remainFileLength)
							? read.Length : remainFileLength;

			ASSERT(readLength);
									
			RtlCopyMemory(
				(PCHAR)outputBuffer,
				fileData,
				readLength
				);			
						
			DebugTrace( 0, Dbg,
					("DirectProcessSecondaryRequest: IRP_MJ_READ RESIDENT_FORM readLength = %d\n", readLength));

			currentByteOffset.QuadPart = readLength;
			
			ioStatusBlock.Information  = readLength;
			ioStatusBlock.Status = STATUS_SUCCESS;
							
			status = STATUS_SUCCESS;
			break;
		}

		ASSERT(attributeRecordHeader->Form.Nonresident.LowestVcn == 0);
		ASSERT(attributeRecordHeader->FormCode == NONRESIDENT_FORM);

		if(attributeRecordHeader->Form.Nonresident.CompressionUnit)
		{
			status = STATUS_UNSUCCESSFUL;
			break;
		}
		DebugTrace( 0, Dbg,
					("DirectProcessSecondaryRequest: IRP_MJ_READ NONRESIDENT_FORM read.Length = %d, read.Key = %d, %d\n", 
					read.Length, read.Key, read.ByteOffset.QuadPart % Secondary->Thread.SessionContext.BytesPerSector != 0));

		if(attributeRecordHeader->Form.Nonresident.ValidDataLength < attributeRecordHeader->Form.Nonresident.FileSize)
		{
			status = STATUS_UNSUCCESSFUL;
			break;
		}

#if 0
		if(attributeRecordHeader->Form.Nonresident.FileSize <= 10*1024*1024)
		{
			status = STATUS_UNSUCCESSFUL;
			break;
		}
#endif

		if(attributeRecordHeader->Form.Nonresident.FileSize <= read.ByteOffset.QuadPart)
		{
			ASSERT(ioStatusBlock.Information == 0);

			ioStatusBlock.Information = 0;
			ioStatusBlock.Status = STATUS_END_OF_FILE;
			status = STATUS_SUCCESS;
			break;
		}

		if(pagingIo || nonCachedIo)
		{
			ASSERT(read.ByteOffset.QuadPart % Secondary->Thread.SessionContext.BytesPerSector == 0);
			ASSERT(read.Length % Secondary->Thread.SessionContext.BytesPerSector == 0);
		}

		do 
		{
			VCN				vcn;
			VCN				startVcn;
			LCN				lcn;
			LONGLONG		clusterCount;
			LONGLONG		remainDataSizeInClulsters;
			ULONG			readLength;
			LONGLONG		remainFileLength;
			LARGE_INTEGER	startingOffset;
			LONGLONG		currentSector;
			NTSTATUS		readStatus;
	
			vcn = LlClustersFromBytesTruncate(
					Secondary->Thread.SessionContext.ClusterShift,
					currentByteOffset.QuadPart
					);

			lookupResult = LookupAllocation(
								Secondary,
								attributeRecordHeader,
								vcn,
								&startVcn,
								&lcn,
								&clusterCount
								);
			
			if(lookupResult == FALSE)
			{
				ASSERT(NDASNTFS_UNEXPECTED);
				ioStatusBlock.Information = 0;
				break;
			}

			remainDataSizeInClulsters
				= (clusterCount << Secondary->Thread.SessionContext.ClusterShift) 
					- (currentByteOffset.QuadPart - (startVcn << Secondary->Thread.SessionContext.ClusterShift));

			DebugTrace( 0, Dbg,
					("DirectProcessSecondaryRequest: IRP_MJ_READ NONRESIDENT_FORM %I64d, %I64d, %I64d\n",
					(clusterCount << Secondary->Thread.SessionContext.ClusterShift),
					(startVcn << Secondary->Thread.SessionContext.ClusterShift),
					(currentByteOffset.QuadPart - (startVcn << Secondary->Thread.SessionContext.ClusterShift))));

			if((read.Length - ioStatusBlock.Information) <= remainDataSizeInClulsters)
			{
				readLength = read.Length - ioStatusBlock.Information;
			}
			else
			{
				readLength = (ULONG) remainDataSizeInClulsters;
			}

			remainFileLength = attributeRecordHeader->Form.Nonresident.FileSize - currentByteOffset.QuadPart;		
			readLength = (readLength <= remainFileLength) ? readLength : (ULONG)remainFileLength;

			ASSERT(readLength);
					
			DebugTrace( 0, Dbg,
					("DirectProcessSecondaryRequest: IRP_MJ_READ NONRESIDENT_FORM %ws LookupAllocation lookupResult = %d, vcn = %I64x, lcn = %I64x, ClusterCount = %I64x\n",
					&fcb->FullFileName, lookupResult, vcn, lcn, clusterCount));
				
			DebugTrace( 0,Dbg, 
					("read->Length = %d %s\n",
					read.Length, ((clusterCount << Secondary->Thread.SessionContext.ClusterShift) >= read.Length) 
					? "included" : "not included"));
	
			currentSector = LlClustersFromBytesTruncate(
								Secondary->Thread.SessionContext.SectorShift,
								currentByteOffset.QuadPart
								);

			startingOffset.QuadPart = lcn << Secondary->Thread.SessionContext.ClusterShift;
			startingOffset.QuadPart 
				+= ((currentSector << Secondary->Thread.SessionContext.SectorShift) 
					- (startVcn << Secondary->Thread.SessionContext.ClusterShift));

			ASSERT(ioStatusBlock.Information + readLength <= read.Length);
			
			if(currentByteOffset.QuadPart != (currentSector << Secondary->Thread.SessionContext.SectorShift))
			{
				ULONG	sectorOffset;

				ASSERT(currentByteOffset.QuadPart % Secondary->Thread.SessionContext.BytesPerSector);

				sectorOffset = (ULONG)(currentByteOffset.QuadPart % Secondary->Thread.SessionContext.BytesPerSector);

				ASSERT(sectorOffset);

				readLength = (readLength <= Secondary->Thread.SessionContext.BytesPerSector - sectorOffset)
						? readLength : (Secondary->Thread.SessionContext.BytesPerSector - sectorOffset);
				
				ASSERT(ioStatusBlock.Information == 0);
				
				readStatus = ReadBuffer(
								Secondary,
								&startingOffset,
								Secondary->Thread.SessionContext.BytesPerSector,
								&fileExt->Cache[0]
								);
			
				DebugTrace( 0,Dbg, ("readStatus = %x\n", readStatus));

				if(readStatus != STATUS_SUCCESS)
				{
					ASSERT(NDASNTFS_UNEXPECTED);
					ioStatusBlock.Information = 0;
					break;
				}
				
				RtlCopyMemory(
					(PCHAR)outputBuffer + ioStatusBlock.Information,
					&fileExt->Cache[sectorOffset],
					readLength
					);
			}
			else if(readLength >= Secondary->Thread.SessionContext.BytesPerSector)
			{
				readLength >>= Secondary->Thread.SessionContext.SectorShift;
				readLength <<= Secondary->Thread.SessionContext.SectorShift;
				
				readStatus = ReadBuffer(
								Secondary,
								&startingOffset,
								readLength,
								(PCHAR)outputBuffer + ioStatusBlock.Information
								);

				DebugTrace( 0,Dbg, ("readStatus = %x\n", readStatus));

				if(readStatus != STATUS_SUCCESS)
				{
					ASSERT(NDASNTFS_UNEXPECTED);
					ioStatusBlock.Information = 0;
					break;
				}
			}
			else
			{
				readStatus = ReadBuffer(
								Secondary,
								&startingOffset,
								Secondary->Thread.SessionContext.BytesPerSector,
								&fileExt->Cache[0]
								);
			
				DebugTrace( 0,Dbg, ("readStatus = %x\n", readStatus));

				if(readStatus != STATUS_SUCCESS)
				{
					ASSERT(NDASNTFS_UNEXPECTED);
					ioStatusBlock.Information = 0;
					break;
				}
				
				RtlCopyMemory(
					(PCHAR)outputBuffer + ioStatusBlock.Information,
					&fileExt->Cache[0],
					readLength
					);
			}

			ioStatusBlock.Information += readLength;
			currentByteOffset.QuadPart += readLength;

			ASSERT(ioStatusBlock.Information <= read.Length);
			ASSERT(currentByteOffset.QuadPart <= attributeRecordHeader->Form.Nonresident.FileSize);
		
		} while(ioStatusBlock.Information < read.Length 
				&& currentByteOffset.QuadPart < attributeRecordHeader->Form.Nonresident.FileSize);

		if(ioStatusBlock.Information)
		{
			ASSERT(ioStatusBlock.Information == read.Length
					|| currentByteOffset.QuadPart == attributeRecordHeader->Form.Nonresident.FileSize);

			ioStatusBlock.Status = STATUS_SUCCESS;
			status = STATUS_SUCCESS;
			break;
		}
		
		status = STATUS_UNSUCCESSFUL;
		break;
	}
	default:

		ASSERT(NDASNTFS_BUG);
		status = STATUS_UNSUCCESSFUL;

		break;
	}

	if(status == STATUS_SUCCESS)
	{
		switch(irpMajorFunction)
		{
		case IRP_MJ_READ:
		{
			PNDFS_REPLY_HEADER			ndfsReplyHeader;
			PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplyHeader;


			ndfsReplyHeader = &SecondaryRequest->NdfsReplyHeader;	
			ndfsReplyHeader->MessageSize = sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER) + ioStatusBlock.Information;
			
			ndfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)(ndfsReplyHeader + 1);
			ndfsWinxpReplyHeader->IrpTag = irpTag;
			ndfsWinxpReplyHeader->IrpMajorFunction = irpMajorFunction;
			ndfsWinxpReplyHeader->IrpMinorFunction = irpMinorFunction;
			ndfsWinxpReplyHeader->Status = ioStatusBlock.Status;
			ndfsWinxpReplyHeader->Information = (_U32)ioStatusBlock.Information;

			break;
		}
		default:

			ASSERT(NDASNTFS_BUG);
			status = STATUS_UNSUCCESSFUL;

			break;
		}
	}

	ASSERT(status != DBG_CONTINUE);

	return status;
}

#endif

NTSTATUS
ProcessSecondaryRequest(
	IN	PSECONDARY			Secondary,
	IN  PSECONDARY_REQUEST	SecondaryRequest
	)
{
	NTSTATUS					status;
	NTSTATUS					sendStatus;
	


	DebugTrace( 0, Dbg,
				("ProcessSecondaryRequest: Start SecondaryRequest = %p\n", SecondaryRequest));

	sendStatus = SendNdfsRequestMessage(Secondary, SecondaryRequest);

	if(sendStatus != STATUS_SUCCESS)
	{
		ASSERT(sendStatus == STATUS_REMOTE_DISCONNECT);
		SecondaryRequest->ExecuteStatus = sendStatus;
		return sendStatus;
	}

	status = ReceiveNdfsReplyMessage(Secondary, SecondaryRequest, TRUE);

	if(status == STATUS_SUCCESS)
	{
		status = DispatchReply(
						Secondary,
						SecondaryRequest,
						(PNDFS_WINXP_REPLY_HEADER)SecondaryRequest->NdfsReplyData
						);
	}else
		ASSERT(status == STATUS_REMOTE_DISCONNECT);		
	
	ASSERT(SecondaryRequest->Synchronous == TRUE);
	SecondaryRequest->ExecuteStatus = status;
	
	return status;
}


NTSTATUS
DispatchReply(
	IN	PSECONDARY					Secondary,
	IN  PSECONDARY_REQUEST			SecondaryRequest,
	IN  PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplytHeader
	)
{
	NTSTATUS			ntStatus;

	UNREFERENCED_PARAMETER(SecondaryRequest);
	UNREFERENCED_PARAMETER(Secondary);


	switch(NdfsWinxpReplytHeader->IrpMajorFunction) 
	{
	case IRP_MJ_CREATE:						// 0x00
	{
		//if(NdfsWinxpReplytHeader->Status == STATUS_SUCCESS)
		//	ASSERT(SecondaryRequest->NdfsReplyHeader.MessageSize 
		//			== sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER) + Secondary->Thread.SessionContext.BytesPerFileRecordSegment);
		//else
		//	ASSERT(SecondaryRequest->NdfsReplyHeader.MessageSize 
		//			== sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER));

		ntStatus = STATUS_SUCCESS;
		break ;
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
	case IRP_MJ_SET_QUOTA:					// 0x1a
	{	
		//ASSERT(SecondaryRequest->NdfsReplyHeader.MessageSize == sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_WINXP_REPLY_HEADER));
		ntStatus = STATUS_SUCCESS;
		break ;
	}

	case IRP_MJ_READ:						// 0x03
	case IRP_MJ_QUERY_INFORMATION:			// 0x05
	case IRP_MJ_QUERY_EA:					// 0x07
	case IRP_MJ_QUERY_VOLUME_INFORMATION:	// 0x0a
	case IRP_MJ_DIRECTORY_CONTROL:			// 0x0c
	case IRP_MJ_FILE_SYSTEM_CONTROL:		// 0x0d
	case IRP_MJ_DEVICE_CONTROL:				// 0x0e
	case IRP_MJ_QUERY_SECURITY:				// 0x14
	case IRP_MJ_QUERY_QUOTA:				// 0x19
	{
		if(!(
				NdfsWinxpReplytHeader->Status == STATUS_SUCCESS 
			||	NdfsWinxpReplytHeader->Status == STATUS_BUFFER_OVERFLOW
			))
		{
#if 0 //	DBG

			CHAR		irpMajorString[OPERATION_NAME_BUFFER_SIZE];
			CHAR		irpMinorString[OPERATION_NAME_BUFFER_SIZE];

			ASSERT(SecondaryRequest->NdfsReplyHeader.MessageSize == sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_WINXP_REPLY_HEADER));
		
			GetIrpName (
				NdfsWinxpReplytHeader->IrpMajorFunction,
				NdfsWinxpReplytHeader->IrpMinorFunction,
				0,
				irpMajorString,
				irpMinorString
				);

			DebugTrace( 0, Dbg,
					("DispatchReply: %s %s (%d %d): is not successful\n", irpMinorString, irpMinorString, 
					NdfsWinxpReplytHeader->IrpMajorFunction, NdfsWinxpReplytHeader->IrpMinorFunction));

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

		ASSERT(NDASNTFS_BUG);
		return STATUS_UNSUCCESSFUL;
	}

	return ntStatus;
}


#endif