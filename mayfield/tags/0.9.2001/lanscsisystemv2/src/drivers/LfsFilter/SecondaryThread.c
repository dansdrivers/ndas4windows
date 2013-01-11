#define	__SECONDARY__
#include "LfsProc.h"

#define SECTHREAD
#include "md5.h"


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
	BOOLEAN	secondaryThreadExit = FALSE;
	KIRQL	oldIrql;


#if DBG
	LfsObjectCounts.SecondaryThreadCount ++;
#endif
	
	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
				("SecondaryThreadProc: Start Secondary = %p\n", Secondary));
	
	Secondary_Reference(Secondary);
	
	Secondary->Thread.Flags = SECONDARY_THREAD_INITIALIZING;
	Secondary->Thread.SessionContext.PrimaryMaxDataSize = DEFAULT_MAX_DATA_SIZE;
	Secondary->Thread.SessionContext.SecondaryMaxDataSize = DEFAULT_MAX_DATA_SIZE;
	Secondary->Thread.SessionContext.MessageSecurity = Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetDiskInformation.MessageSecurity;
	Secondary->Thread.SessionContext.RwDataSecurity = Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetDiskInformation.RwDataSecurity;

	Secondary->Thread.ConnectionStatus = ConnectToPrimary(Secondary);

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
				("SecondaryThreadProc: Start Secondary = %p Secondary->Thread.ConnectionStatus = %x\n", 
				Secondary, Secondary->Thread.ConnectionStatus));
 
	if(Secondary->Thread.ConnectionStatus == STATUS_SUCCESS)
	{
		KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql) ;
		ClearFlag(Secondary->Thread.Flags, SECONDARY_THREAD_INITIALIZING);
		SetFlag(Secondary->Thread.Flags, SECONDARY_THREAD_CONNECTED);
		KeReleaseSpinLock(&Secondary->SpinLock, oldIrql) ;
		KeSetEvent(&Secondary->Thread.ReadyEvent, IO_DISK_INCREMENT, FALSE);
	}
	else 
	{
		KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql) ;
		ClearFlag(Secondary->Thread.Flags, SECONDARY_THREAD_INITIALIZING);
		SetFlag(Secondary->Thread.Flags, SECONDARY_THREAD_UNCONNECTED);
		KeReleaseSpinLock(&Secondary->SpinLock, oldIrql) ;
		secondaryThreadExit = TRUE;
	}

	while(secondaryThreadExit == FALSE)
	{
		PKEVENT			events[2];
		LONG			eventCount;
		NTSTATUS		eventStatus;
		LARGE_INTEGER	timeOut;
		
		PLIST_ENTRY		secondaryRequestEntry;


		eventCount = 0;
		events[eventCount++] = &Secondary->Thread.RequestEvent;

		if(Secondary->Thread.WaitReceive != 0)
		{
			events[eventCount++] = &Secondary->Thread.TdiReceiveContext.CompletionEvent;
		}

		timeOut.QuadPart = -LFS_SECONDARY_THREAD_TIME_OUT;

		eventStatus = KeWaitForMultipleObjects(
						eventCount,
						events,
						WaitAny,
						Executive,
						KernelMode,
						TRUE,
						&timeOut,
						NULL
						);

		if(eventStatus == STATUS_TIMEOUT) 
		{
			KeQuerySystemTime(&Secondary->CleanUpTime);
			Secondary_TryCloseFilExts(Secondary);
			continue;
		}
		
		ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL);
		ASSERT(eventCount < THREAD_WAIT_OBJECTS);
		
		if(!NT_SUCCESS(eventStatus) || eventStatus >= eventCount)
		{
			ASSERT(LFS_UNEXPECTED);
			SetFlag(Secondary->Thread.Flags, SECONDARY_THREAD_ERROR);
			secondaryThreadExit = TRUE;
			continue;
		}
		
		KeClearEvent(events[eventStatus]);

		if(eventStatus == 0)
		{
#if 0
			LARGE_INTEGER	currentTime;

#define CLEANUP_TIMEOUT			((LONGLONG)(10*HZ))
#define CLEANUP_FILEEXT_COUNT	10

			KeQuerySystemTime(&currentTime);
			if(Secondary->CleanUpTime.QuadPart > currentTime.QuadPart
					|| currentTime.QuadPart - Secondary->CleanUpTime.QuadPart >= CLEANUP_TIMEOUT)
			{
				if(Secondary->FileExtCount > 100)
					SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
						("SecondaryThreadProc: Secondary = %p Secondary->FileExtCount = %d\n", 
							Secondary, Secondary->FileExtCount));
				
				if(Secondary->FileExtCount >= CLEANUP_FILEEXT_COUNT)
				{
					KeQuerySystemTime(&Secondary->CleanUpTime);
					Secondary_TryCloseFilExts(Secondary);
				}
			}
#endif
			
			while(secondaryThreadExit == FALSE 
					&& (secondaryRequestEntry = 
							ExInterlockedRemoveHeadList(
										&Secondary->Thread.RequestQueue,
										&Secondary->Thread.RequestQSpinLock
										))
				) 
			{
				PSECONDARY_REQUEST	secondaryRequest;
				NTSTATUS			processStatus;

			
				InitializeListHead(secondaryRequestEntry);

				secondaryRequest = CONTAINING_RECORD(
										secondaryRequestEntry,
										SECONDARY_REQUEST,
										ListEntry
										);

				if(secondaryRequest->RequestType == SECONDARY_REQ_DISCONNECT)
				{
					DisconnectFromPrimary(Secondary);
				
					KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
					SetFlag(Secondary->Thread.Flags, SECONDARY_THREAD_DISCONNECTED);
					KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
				}
				else if(secondaryRequest->RequestType == SECONDARY_REQ_DOWN)
				{				
					secondaryThreadExit = TRUE;
					ASSERT(IsListEmpty(&Secondary->Thread.RequestQueue));
				}
				else if(secondaryRequest->RequestType == SECONDARY_REQ_SEND_MESSAGE)
				{
					ASSERT(secondaryRequest->Synchronous == TRUE);

					do 
					{
						_U16		mid;
						

						if(secondaryRequest->SessionId != Secondary->SessionId)
						{
							ASSERT(secondaryRequest->SessionId < Secondary->SessionId);
							secondaryRequest->ExecuteStatus = STATUS_ABANDONED;

							processStatus = STATUS_SUCCESS;
							break;		
						}

						if(secondaryRequest->FileExt)
						{
							processStatus = DirectProcessSecondaryRequest(Secondary, secondaryRequest);
							if(processStatus == STATUS_SUCCESS)
								break;
						}
					
						for(mid=0; mid < Secondary->Thread.SessionContext.RequestsPerSession; mid++)
						{
							if(Secondary->Thread.ProcessingSecondaryRequest[mid] == NULL)
								break;
						}

						if(mid == Secondary->Thread.SessionContext.RequestsPerSession)
						{
							ASSERT(LFS_BUG);
							KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
							SetFlag(Secondary->Thread.Flags, SECONDARY_THREAD_ERROR);
							KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
						}

						Secondary->Thread.ProcessingSecondaryRequest[mid] = secondaryRequest;
						secondaryRequest->NdfsRequestHeader.Mid = mid;

						processStatus = SendNdfsRequestMessage(Secondary, secondaryRequest);

						if(processStatus != STATUS_SUCCESS)
						{
							ASSERT(processStatus == STATUS_REMOTE_DISCONNECT);
							if(processStatus == STATUS_REMOTE_DISCONNECT) 
							{
								Secondary->Thread.ProcessingSecondaryRequest[mid] = NULL;
								secondaryRequest->ExecuteStatus = processStatus;

								KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
								SetFlag(Secondary->Thread.Flags, SECONDARY_THREAD_REMOTE_DISCONNECTED);
								KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
							}
							secondaryThreadExit = TRUE;

							if(secondaryRequest->Synchronous == TRUE)
								KeSetEvent(&secondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE);
							else
								DereferenceSecondaryRequest(secondaryRequest);
							
							break;
						}

						if(Secondary->Thread.SessionContext.RequestsPerSession > 1)
						{
							NTSTATUS	tdiStatus;
	
							if(InterlockedIncrement(&Secondary->Thread.WaitReceive) == 1)
							{
								tdiStatus = LpxTdiRecvWithCompletionEvent(
												Secondary->Thread.ConnectionFileObject,
												&Secondary->Thread.TdiReceiveContext,
												(PUCHAR)&Secondary->Thread.NdfsReplyHeader,
												sizeof(NDFS_REQUEST_HEADER),
												0
												);

								if(!NT_SUCCESS(tdiStatus))
								{
									ASSERT(LFS_BUG);
									secondaryRequest->ExecuteStatus = processStatus;

									KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql) ;
									SetFlag(Secondary->Thread.Flags, SECONDARY_THREAD_REMOTE_DISCONNECTED);
									KeReleaseSpinLock(&Secondary->SpinLock, oldIrql) ;
									secondaryThreadExit = TRUE;

									break;
								}
							}

							processStatus = STATUS_PENDING;					
							break;
						}
						
						Secondary->Thread.ProcessingSecondaryRequest[0] = NULL;
	
						processStatus = ReceiveNdfsReplyMessage(Secondary, secondaryRequest, TRUE);

						if(processStatus == STATUS_SUCCESS)
						{
							processStatus = DispatchReply(
												Secondary,
												secondaryRequest,
												(PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData
												);
						}
						
						if(processStatus != STATUS_SUCCESS)
						{
							ASSERT(processStatus == STATUS_REMOTE_DISCONNECT);
							if(processStatus == STATUS_REMOTE_DISCONNECT) 
							{
								secondaryRequest->ExecuteStatus = processStatus;

								KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
								SetFlag(Secondary->Thread.Flags, SECONDARY_THREAD_REMOTE_DISCONNECTED);
								KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
							}
							secondaryThreadExit = TRUE;

							break;
						}
	
						ASSERT(processStatus == STATUS_SUCCESS);
						secondaryRequest->ExecuteStatus = processStatus;
						
						break;

					} while(0);

					if(secondaryThreadExit == TRUE)
						continue;
				}
				else
				{
					ASSERT(LFS_BUG);
					KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
					SetFlag(Secondary->Thread.Flags, SECONDARY_THREAD_ERROR);
					KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
					secondaryThreadExit = TRUE;
				}

				if(secondaryRequest->RequestType == SECONDARY_REQ_DISCONNECT
					|| secondaryRequest->RequestType == SECONDARY_REQ_DOWN
					|| secondaryRequest->RequestType == SECONDARY_REQ_SEND_MESSAGE && processStatus != STATUS_PENDING)
				{
					if(secondaryRequest->Synchronous == TRUE)
						KeSetEvent(&secondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE);
					else
						DereferenceSecondaryRequest(secondaryRequest);
				}
			}
		}
		else if(eventStatus == 1)
		{
			NTSTATUS			processStatus;
			PSECONDARY_REQUEST	secondaryRequest;


			if(Secondary->Thread.TdiReceiveContext.Result != sizeof(NDFS_REQUEST_HEADER))
			{
				_U16		mid;

				for(mid=0; mid < Secondary->Thread.SessionContext.RequestsPerSession; mid++)
				{
					if(Secondary->Thread.ProcessingSecondaryRequest[mid] != NULL) // choose arbitrary one
						break;
				}

				ASSERT(mid < Secondary->Thread.SessionContext.RequestsPerSession);
				
				secondaryRequest = Secondary->Thread.ProcessingSecondaryRequest[mid];
				Secondary->Thread.ProcessingSecondaryRequest[mid] = NULL;
				
				secondaryRequest->ExecuteStatus = STATUS_REMOTE_DISCONNECT;
				KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
				SetFlag(Secondary->Thread.Flags, SECONDARY_THREAD_REMOTE_DISCONNECTED);
				KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
	
				secondaryThreadExit = TRUE;
				if(secondaryRequest->Synchronous == TRUE)
					KeSetEvent(&secondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE);
				else
					DereferenceSecondaryRequest(secondaryRequest);

				continue;
			}

			secondaryRequest = Secondary->Thread.ProcessingSecondaryRequest[Secondary->Thread.NdfsReplyHeader.Mid];
			Secondary->Thread.ProcessingSecondaryRequest[Secondary->Thread.NdfsReplyHeader.Mid] = NULL;

			RtlCopyMemory(
				secondaryRequest->NdfsMessage,
				&Secondary->Thread.NdfsReplyHeader,
				sizeof(NDFS_REPLY_HEADER)
				);
			
			processStatus = ReceiveNdfsReplyMessage(Secondary, secondaryRequest, FALSE);

			if(processStatus == STATUS_SUCCESS)
			{
				processStatus = DispatchReply(
									Secondary,
									secondaryRequest,
									(PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData
									);
			}
						
			if(processStatus == STATUS_SUCCESS)
			{
				secondaryRequest->ExecuteStatus = processStatus;
			}
			else
			{
				ASSERT(processStatus == STATUS_REMOTE_DISCONNECT);
				if(processStatus == STATUS_REMOTE_DISCONNECT) 
				{
					secondaryRequest->ExecuteStatus = processStatus;

					KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
					SetFlag(Secondary->Thread.Flags, SECONDARY_THREAD_REMOTE_DISCONNECTED);
					KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
				}
				
				secondaryThreadExit = TRUE;
			}
			
			if(processStatus == STATUS_SUCCESS && InterlockedDecrement(&Secondary->Thread.WaitReceive) != 0)
			{
				NTSTATUS	tdiStatus;

				tdiStatus = LpxTdiRecvWithCompletionEvent(
								Secondary->Thread.ConnectionFileObject,
								&Secondary->Thread.TdiReceiveContext,
								(PUCHAR)&Secondary->Thread.NdfsReplyHeader,
								sizeof(NDFS_REQUEST_HEADER),
								0
								);

				if(!NT_SUCCESS(tdiStatus))
				{
					_U16		mid;
	
					for(mid=0; mid < Secondary->Thread.SessionContext.RequestsPerSession; mid++)
					{
						if(Secondary->Thread.ProcessingSecondaryRequest[mid] != NULL)
							break;
					}
				
					ASSERT(mid < Secondary->Thread.SessionContext.RequestsPerSession);

					secondaryRequest = Secondary->Thread.ProcessingSecondaryRequest[mid];
					Secondary->Thread.ProcessingSecondaryRequest[mid] = NULL;
					
					secondaryRequest->ExecuteStatus = STATUS_REMOTE_DISCONNECT;

					KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
					SetFlag(Secondary->Thread.Flags, SECONDARY_THREAD_REMOTE_DISCONNECTED);
					KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
					secondaryThreadExit = TRUE;

					if(secondaryRequest->Synchronous == TRUE)
						KeSetEvent(&secondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE);
					else
						DereferenceSecondaryRequest(secondaryRequest);
				}
			}

			if(secondaryRequest->Synchronous == TRUE)
				KeSetEvent(&secondaryRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE);
			else
				DereferenceSecondaryRequest(secondaryRequest);

			continue;
		}
		else
			ASSERT(LFS_BUG);
	}


	KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql) ;
	if(BooleanFlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_CONNECTED)
		&& !BooleanFlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_DISCONNECTED)
		)
	{
		SetFlag(Secondary->Thread.Flags, SECONDARY_THREAD_DISCONNECTED);
		KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);

		DisconnectFromPrimary(Secondary);

		KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
	}

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
				("SecondaryThreadProc: PsTerminateSystemThread Secondary = %p, IsListEmpty(&Secondary->Thread.RequestQueue) = %d\n", 
					Secondary, IsListEmpty(&Secondary->Thread.RequestQueue)));
	
	SetFlag(Secondary->Thread.Flags, SECONDARY_THREAD_TERMINATED);
	KeReleaseSpinLock(&Secondary->SpinLock, oldIrql) ;
	
#if DBG
	LfsObjectCounts.SecondaryThreadCount --;
#endif

	if(BooleanFlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_UNCONNECTED)) 
		KeSetEvent(&Secondary->Thread.ReadyEvent, IO_DISK_INCREMENT, FALSE);

	Secondary_Dereference(Secondary);
	
	PsTerminateSystemThread(STATUS_SUCCESS);
}


NTSTATUS
ConnectToPrimary(
	IN	PSECONDARY	Secondary
   )
{
	NTSTATUS				returnStatus;

	SOCKETLPX_ADDRESS_LIST	socketLpxAddressList;
	LONG					idx_addr;

    HANDLE					addressFileHandle;
	HANDLE					connectionFileHandle;

    PFILE_OBJECT			addressFileObject;
	PFILE_OBJECT			connectionFileObject;
	
	LPX_ADDRESS				NICAddr;
	PLPX_ADDRESS			serverAddress;
		
	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
				("ConnectToPrimary: Entered\n"));
	
	returnStatus = LpxTdiGetAddressList(&socketLpxAddressList);
	if(returnStatus != STATUS_SUCCESS)
	{
		ASSERT(LPX_BUG);
		return	returnStatus;
	}
	
	if(socketLpxAddressList.iAddressCount <= 0) 
	{
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
					( "BindListenSockets: No NICs in the host.\n") );

		return STATUS_UNEXPECTED_NETWORK_ERROR;
	}
	

	addressFileHandle = NULL; addressFileObject = NULL;
	connectionFileHandle = NULL; connectionFileObject = NULL;

	for(idx_addr=0; idx_addr<socketLpxAddressList.iAddressCount; idx_addr++)
	{
		NTSTATUS lpxStatus;
	
		if( (0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[0]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[1]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[2]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[3]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[4]) &&
			(0 == socketLpxAddressList.SocketLpx[idx_addr].LpxAddress.Node[5]) ) 
		{
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
						( "BindListenSockets: We don't use SocketLpx device.\n")) ;
			continue ;
		}

		RtlZeroMemory(&NICAddr, sizeof(LPX_ADDRESS));

		if(Lfs_IsLocalAddress(&Secondary->PrimaryAddress))
		{
			RtlCopyMemory(
				&NICAddr, 
				&Secondary->PrimaryAddress, 
				sizeof(LPX_ADDRESS)
				);
		}
		else
		{
			RtlCopyMemory(
				&NICAddr, 
				&socketLpxAddressList.SocketLpx[idx_addr].LpxAddress, 
				sizeof(LPX_ADDRESS)
				);
		}

		NICAddr.Port = 0;

		lpxStatus = LpxTdiOpenAddress(
						&addressFileHandle,
						&addressFileObject,
						&NICAddr
						);
	
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
						("Connect from %02X:%02X:%02X:%02X:%02X:%02X 0x%4X\n",
							NICAddr.Node[0], NICAddr.Node[1], NICAddr.Node[2],
							NICAddr.Node[3], NICAddr.Node[4], NICAddr.Node[5],
							NTOHS(NICAddr.Port)
							));
		if(!NT_SUCCESS(lpxStatus)) 
		{
			ASSERT(LFS_UNEXPECTED);
			continue;
		}
		
		lpxStatus = LpxTdiOpenConnection(
						&connectionFileHandle,
						&connectionFileObject,
						NULL
						);
	
		if(!NT_SUCCESS(lpxStatus)) 
		{
			ASSERT(LFS_UNEXPECTED);
			LpxTdiCloseAddress (addressFileHandle, addressFileObject);
			addressFileHandle = NULL; addressFileObject = NULL;
			continue;
		}

		lpxStatus = LpxTdiAssociateAddress(
						connectionFileObject,
						addressFileHandle
						);
	

		if(!NT_SUCCESS(lpxStatus)) 
		{
			ASSERT(LFS_UNEXPECTED);
			LpxTdiCloseAddress (addressFileHandle, addressFileObject);
			addressFileHandle = NULL; addressFileObject = NULL;
			LpxTdiCloseConnection(connectionFileHandle, connectionFileObject);
			connectionFileHandle = NULL; connectionFileObject = NULL;
			continue;
		}
	
		serverAddress = &Secondary->PrimaryAddress;

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
						("Connect to %02X:%02X:%02X:%02X:%02X:%02X 0x%4X\n",
							serverAddress->Node[0], serverAddress->Node[1], serverAddress->Node[2],
							serverAddress->Node[3], serverAddress->Node[4], serverAddress->Node[5],
							NTOHS(serverAddress->Port)
							));

		lpxStatus = LpxTdiConnect(
						connectionFileObject,
						serverAddress
						);

		if(lpxStatus == STATUS_SUCCESS)
			break;

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
			("Connection is Failed\n"));

		LpxTdiDisassociateAddress(connectionFileObject);
		LpxTdiCloseAddress (addressFileHandle, addressFileObject);
		addressFileHandle = NULL; addressFileObject = NULL;
		LpxTdiCloseConnection(connectionFileHandle, connectionFileObject);
		connectionFileHandle = NULL; connectionFileObject = NULL;
	}

	if(connectionFileHandle == NULL)
	{
		//if(!BooleanFlagOn(Secondary->Flags,  SECONDARY_RECONNECTING))
			//ASSERT(LFS_BUG);
		return STATUS_DEVICE_NOT_CONNECTED;
	}

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
					("Connection is Success connectionFileObject = %p\n", connectionFileObject));

	do // just for structural programing
	{
		PSECONDARY_REQUEST			secondaryRequest;
		PNDFS_REQUEST_HEADER		ndfsRequestHeader;
		PNDFS_REQUEST_NEGOTIATE		ndfsRequestNegotiate;
		PNDFS_REQUEST_SETUP			ndfsRequestSetup;
		PNDFS_REPLY_HEADER			ndfsReplyHeader;
		PNDFS_REPLY_NEGOTIATE		ndfsReplyNegotiate;
		PNDFS_REPLY_SETUP			ndfsReplySetup;
		NTSTATUS					openStatus;


		secondaryRequest = AllocSecondaryRequest(
							Secondary,
							(sizeof(NDFS_REQUEST_HEADER)+sizeof(NDFS_REQUEST_NEGOTIATE) > sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_NEGOTIATE))
							? (sizeof(NDFS_REQUEST_HEADER)+sizeof(NDFS_REQUEST_NEGOTIATE)) : (sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_NEGOTIATE)),
							FALSE
							);

		if(secondaryRequest == NULL)
		{
			returnStatus = STATUS_INSUFFICIENT_RESOURCES;
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
	
		ndfsRequestNegotiate->NdfsMajorVersion = NDFS_PROTOCOL_MAJOR_1;
		ndfsRequestNegotiate->NdfsMinorVersion = NDFS_PROTOCOL_MINOR_0;
		ndfsRequestNegotiate->OsMajorType	   = OS_TYPE_WINDOWS;
		ndfsRequestNegotiate->OsMinorType	   = OS_TYPE_WINXP;
		ndfsRequestNegotiate->MinorVersionPlusOne = 1;

		openStatus = SendMessage(
						connectionFileObject,
						secondaryRequest->NdfsMessage,
						ndfsRequestHeader->MessageSize,
						NULL
						);

		if(openStatus != STATUS_SUCCESS)
		{
			DereferenceSecondaryRequest(secondaryRequest);
			returnStatus = openStatus;
			break;
		}

		openStatus = RecvMessage(
						connectionFileObject,
						secondaryRequest->NdfsMessage,
						sizeof(NDFS_REPLY_HEADER),
						NULL
						);
	
		if(openStatus != STATUS_SUCCESS)
		{
			DereferenceSecondaryRequest(secondaryRequest);
			returnStatus = openStatus;
			break;
		}

		ndfsReplyHeader = &secondaryRequest->NdfsReplyHeader;
		
		if(!(
			RtlEqualMemory(ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol))
			&& ndfsReplyHeader->Status == NDFS_SUCCESS
			))
		{
			ASSERT(LFS_BUG);
			DereferenceSecondaryRequest(secondaryRequest);
			returnStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		ndfsReplyNegotiate = (PNDFS_REPLY_NEGOTIATE)(ndfsReplyHeader+1);
		ASSERT(ndfsReplyNegotiate == (PNDFS_REPLY_NEGOTIATE)secondaryRequest->NdfsReplyData);

		openStatus = RecvMessage(
						connectionFileObject,
						(_U8 *)ndfsReplyNegotiate,
						sizeof(NDFS_REPLY_NEGOTIATE),
						NULL
						);
	
		if(openStatus != STATUS_SUCCESS)
		{
			returnStatus = openStatus;
			DereferenceSecondaryRequest(secondaryRequest);
			break;
		}

		if(ndfsReplyNegotiate->Status != NDFS_NEGOTIATE_SUCCESS)
		{
			ASSERT(LFS_BUG);
			DereferenceSecondaryRequest(secondaryRequest);
			returnStatus = STATUS_INVALID_PARAMETER;
			break;
		}
		
		Secondary->Thread.SessionContext.NdfsMajorVersion = ndfsReplyNegotiate->NdfsMajorVersion;
		Secondary->Thread.SessionContext.NdfsMinorVersion = ndfsReplyNegotiate->NdfsMinorVersion;

		Secondary->Thread.SessionContext.SessionKey = ndfsReplyNegotiate->SessionKey;
		RtlCopyMemory(
			Secondary->Thread.SessionContext.ChallengeBuffer,
			ndfsReplyNegotiate->ChallengeBuffer,
			ndfsReplyNegotiate->ChallengeLength
			);
		Secondary->Thread.SessionContext.ChallengeLength = ndfsReplyNegotiate->ChallengeLength;
		
		if(ndfsReplyNegotiate->MaxBufferSize)
		{
			Secondary->Thread.SessionContext.PrimaryMaxDataSize
				= (ndfsReplyNegotiate->MaxBufferSize <= DEFAULT_MAX_DATA_SIZE)
					? ndfsReplyNegotiate->MaxBufferSize : DEFAULT_MAX_DATA_SIZE;
		}
		
		DereferenceSecondaryRequest(secondaryRequest);
			
		secondaryRequest = AllocSecondaryRequest(
							Secondary,
							(sizeof(NDFS_REQUEST_HEADER)+sizeof(NDFS_REQUEST_SETUP) > sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_SETUP))
							? (sizeof(NDFS_REQUEST_HEADER)+sizeof(NDFS_REQUEST_SETUP)) : (sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_SETUP)),
							FALSE
							);

		if(secondaryRequest == NULL)
		{
			returnStatus = STATUS_INSUFFICIENT_RESOURCES;
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

		RtlCopyMemory(
			ndfsRequestSetup->NetDiskNode,
			Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetDiskInformation.NetDiskAddress.Node,
			6
			);
		ndfsRequestSetup->NetDiskPort	 
			= HTONS(Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetDiskInformation.NetDiskAddress.Port);
		ndfsRequestSetup->UnitDiskNo	 
			= Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetDiskInformation.UnitDiskNo;
		if(Secondary->Thread.SessionContext.NdfsMinorVersion == NDFS_PROTOCOL_MINOR_0)
			ndfsRequestSetup->StartingOffset 
				= Secondary->LfsDeviceExt->NetdiskPartitionInformation.PartitionInformation.StartingOffset.QuadPart;
		else
			ndfsRequestSetup->StartingOffset = 0;

		ndfsRequestSetup->ResponseLength = 0;

		{
			unsigned char		idData[1];
			struct MD5Context	context;


			MD5Init(&context);

			/* id byte */
			idData[0] = (unsigned char)Secondary->Thread.SessionContext.SessionKey;
			MD5Update(&context, idData, 1);

			MD5Update(&context, Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetDiskInformation.Password, 8);			
			MD5Update(&context, Secondary->Thread.SessionContext.ChallengeBuffer, Secondary->Thread.SessionContext.ChallengeLength);

			ndfsRequestSetup->ResponseLength = 16;
			MD5Final(ndfsRequestSetup->ResponseBuffer, &context);
		}

		openStatus = SendMessage(
						connectionFileObject,
						secondaryRequest->NdfsMessage,
						ndfsRequestHeader->MessageSize,
						NULL
						);

		if(openStatus != STATUS_SUCCESS)
		{
			returnStatus = openStatus;
			DereferenceSecondaryRequest(secondaryRequest);
			break;
		}

		openStatus = RecvMessage(
						connectionFileObject,
						secondaryRequest->NdfsMessage,
						sizeof(NDFS_REPLY_HEADER),
						NULL
						);
	
		if(openStatus != STATUS_SUCCESS)
		{
			returnStatus = openStatus;
			DereferenceSecondaryRequest(secondaryRequest);
			break;
		}

		ndfsReplyHeader = &secondaryRequest->NdfsReplyHeader;
		
		if(!(
			RtlEqualMemory(ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol))
			&& ndfsReplyHeader->Status == NDFS_SUCCESS
			))
		{
			ASSERT(LFS_BUG);
			DereferenceSecondaryRequest(secondaryRequest);
			returnStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		ndfsReplySetup = (PNDFS_REPLY_SETUP)(ndfsReplyHeader+1);
		ASSERT(ndfsReplySetup == (PNDFS_REPLY_SETUP)secondaryRequest->NdfsReplyData);

		openStatus = RecvMessage(
						connectionFileObject,
						(_U8 *)ndfsReplySetup,
						sizeof(NDFS_REPLY_SETUP),
						NULL
						);

		if(openStatus != STATUS_SUCCESS)
		{
			returnStatus = openStatus;
			DereferenceSecondaryRequest(secondaryRequest);
			break;
		}
		
		if(ndfsReplySetup->Status != NDFS_SETUP_SUCCESS)
		{
			DereferenceSecondaryRequest(secondaryRequest);
			returnStatus = STATUS_INVALID_PARAMETER;
			break;
		}
		
		Secondary->Thread.SessionContext.Uid = ndfsReplyHeader->Uid;
		
		if(Secondary->Thread.SessionContext.NdfsMinorVersion == NDFS_PROTOCOL_MINOR_0)
			Secondary->Thread.SessionContext.Tid = ndfsReplyHeader->Tid;

		ASSERT(Secondary->Thread.SessionContext.BytesPerFileRecordSegment == 0);
		Secondary->Thread.SessionContext.RequestsPerSession = 1;
		Secondary->Thread.SessionContext.BytesPerFileRecordSegment = 0;
		DereferenceSecondaryRequest(secondaryRequest);

		if(Secondary->Thread.SessionContext.NdfsMinorVersion == NDFS_PROTOCOL_MINOR_0)
		{
			returnStatus = STATUS_SUCCESS;
			break;
		}

		if(Secondary->Thread.SessionContext.NdfsMinorVersion == NDFS_PROTOCOL_MINOR_1)
		{
			ULONG						sendCount = 0;
			PNDFS_REQUEST_TREE_CONNECT	ndfsRequestTreeConnect;
			PNDFS_REPLY_TREE_CONNECT	ndfsReplyTreeConnect;
			ULONG						i;

SEND_TREE_CONNECT_COMMAND:

			secondaryRequest = AllocSecondaryRequest(
								Secondary,
								(sizeof(NDFS_REQUEST_HEADER)+sizeof(NDFS_REQUEST_TREE_CONNECT) > sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_TREE_CONNECT))
								? (sizeof(NDFS_REQUEST_HEADER)+sizeof(NDFS_REQUEST_TREE_CONNECT)) : (sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_TREE_CONNECT)),
								FALSE
								);

			if(secondaryRequest == NULL)
			{
				returnStatus = STATUS_INSUFFICIENT_RESOURCES;
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

			ndfsRequestTreeConnect->StartingOffset
				= Secondary->LfsDeviceExt->NetdiskPartitionInformation.PartitionInformation.StartingOffset.QuadPart;

			openStatus = SendMessage(
							connectionFileObject,
							secondaryRequest->NdfsMessage,
							ndfsRequestHeader->MessageSize,
							NULL
							);

			if(openStatus != STATUS_SUCCESS)
			{
				returnStatus = openStatus;
				DereferenceSecondaryRequest(secondaryRequest);
				break;
			}

			openStatus = RecvMessage(
							connectionFileObject,
							secondaryRequest->NdfsMessage,
							sizeof(NDFS_REPLY_HEADER),
							NULL
							);
	
			if(openStatus != STATUS_SUCCESS)
			{
				returnStatus = openStatus;
				DereferenceSecondaryRequest(secondaryRequest);
				break;
			}

			ndfsReplyHeader = &secondaryRequest->NdfsReplyHeader;
		
			if(!(
				RtlEqualMemory(ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol))
				&& ndfsReplyHeader->Status == NDFS_SUCCESS
				))
			{
				ASSERT(LFS_BUG);
				DereferenceSecondaryRequest(secondaryRequest);
				returnStatus = STATUS_INVALID_PARAMETER;
				break;
			}

			ndfsReplyTreeConnect = (PNDFS_REPLY_TREE_CONNECT)(ndfsReplyHeader+1);
			ASSERT(ndfsReplyTreeConnect == (PNDFS_REPLY_TREE_CONNECT)secondaryRequest->NdfsReplyData);

			openStatus = RecvMessage(
							connectionFileObject,
							(_U8 *)ndfsReplyTreeConnect,
							sizeof(NDFS_REPLY_TREE_CONNECT),
							NULL
							);
	
			if(openStatus != STATUS_SUCCESS)
			{
				returnStatus = openStatus;
				DereferenceSecondaryRequest(secondaryRequest);
				break;
			}

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
				("NDFS_COMMAND_TREE_CONNECT: ndfsReplyTreeConnect->Status = %x\n", 
					ndfsReplyTreeConnect->Status));

			if(ndfsReplyTreeConnect->Status == NDFS_TREE_CORRUPTED
				|| ndfsReplyTreeConnect->Status == NDFS_TREE_CONNECT_NO_PARTITION)
			{
				if(ndfsReplyTreeConnect->Status == NDFS_TREE_CORRUPTED && sendCount <= 4
					|| ndfsReplyTreeConnect->Status == NDFS_TREE_CONNECT_NO_PARTITION && sendCount <= 12)
				{
					LARGE_INTEGER	timeOut;
					KEVENT			nullEvent;
					NTSTATUS		waitStatus;
				
					KeInitializeEvent(&nullEvent, NotificationEvent, FALSE);
					timeOut.QuadPart = -10*HZ;		// 10 sec

					waitStatus = KeWaitForSingleObject(
									&nullEvent,
									Executive,
									KernelMode,
									FALSE,
									&timeOut
									);
	
					ASSERT(waitStatus == STATUS_TIMEOUT);

					DereferenceSecondaryRequest(secondaryRequest);
					goto SEND_TREE_CONNECT_COMMAND;	
				}
				
				if(ndfsReplyTreeConnect->Status == NDFS_TREE_CONNECT_NO_PARTITION)
					ASSERT(LFS_REQUIRED);

				returnStatus = STATUS_FILE_CORRUPT_ERROR;
				DereferenceSecondaryRequest(secondaryRequest);
	
				break;
			}
			else if(ndfsReplyTreeConnect->Status == NDFS_TREE_CONNECT_UNSUCCESSFUL)
			{
				//ASSERT(LFS_BUG);
				DereferenceSecondaryRequest(secondaryRequest);
				returnStatus = STATUS_INVALID_PARAMETER;
				break;
			}

			ASSERT(ndfsReplyTreeConnect->Status == NDFS_TREE_CONNECT_SUCCESS);

			Secondary->Thread.SessionContext.Tid = ndfsReplyHeader->Tid;

			Secondary->Thread.SessionContext.RequestsPerSession = ndfsReplyTreeConnect->RequestsPerSession;
	
			Secondary->Thread.SessionContext.BytesPerFileRecordSegment	
				= ndfsReplyTreeConnect->BytesPerFileRecordSegment;

			Secondary->Thread.SessionContext.BytesPerSector
				= ndfsReplyTreeConnect->BytesPerSector;
			for (Secondary->Thread.SessionContext.SectorShift = 0, i = Secondary->Thread.SessionContext.BytesPerSector; 
					i > 1; 
					i = i / 2) 
			{
				Secondary->Thread.SessionContext.SectorShift += 1;
			}
			Secondary->Thread.SessionContext.SectorMask = Secondary->Thread.SessionContext.BytesPerSector - 1;

			Secondary->Thread.SessionContext.BytesPerCluster = ndfsReplyTreeConnect->BytesPerCluster;
			for (Secondary->Thread.SessionContext.ClusterShift = 0, i = Secondary->Thread.SessionContext.BytesPerCluster; 
					i > 1; 
					i = i / 2) 
			{
				Secondary->Thread.SessionContext.ClusterShift += 1;
			}
			Secondary->Thread.SessionContext.ClusterMask = Secondary->Thread.SessionContext.BytesPerCluster - 1;

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
				("NDFS_COMMAND_TREE_CONNECT: ndfsReplyTreeConnect->BytesPerSector = %d, ndfsReplyTreeConnect->BytesPerCluster = %d\n", 
					ndfsReplyTreeConnect->BytesPerSector, ndfsReplyTreeConnect->BytesPerCluster));
	
			DereferenceSecondaryRequest(secondaryRequest);
			returnStatus = STATUS_SUCCESS;
		}
		
	} while(0);

	if(returnStatus != STATUS_SUCCESS) 
	{
		LpxTdiDisconnect(connectionFileObject, 0);
		LpxTdiDisassociateAddress(connectionFileObject);
		LpxTdiCloseAddress (addressFileHandle, addressFileObject);
		LpxTdiCloseConnection(connectionFileHandle, connectionFileObject);

		return returnStatus;
	}

	Secondary->Thread.AddressFileHandle = addressFileHandle;
	Secondary->Thread.AddressFileObject = addressFileObject;

	Secondary->Thread.ConnectionFileHandle = connectionFileHandle;
	Secondary->Thread.ConnectionFileObject = connectionFileObject;

	
	return returnStatus;
}


NTSTATUS
DisconnectFromPrimary(
	IN	PSECONDARY	Secondary
   )
{
	NTSTATUS			returnStatus;


	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO,
				("DisconnectFromPrimary: Entered\n"));

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
			returnStatus = STATUS_INSUFFICIENT_RESOURCES;
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
			returnStatus = openStatus;
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
			returnStatus = openStatus;
			DereferenceSecondaryRequest(secondaryRequest);
			break;
		}

		ndfsReplyHeader = &secondaryRequest->NdfsReplyHeader;
		
		if(!(
			RtlEqualMemory(ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol))
			&& ndfsReplyHeader->Status == NDFS_SUCCESS
			))
		{
			ASSERT(LFS_BUG);
			DereferenceSecondaryRequest(secondaryRequest);
			returnStatus = STATUS_INVALID_PARAMETER;
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
			returnStatus = openStatus;
			DereferenceSecondaryRequest(secondaryRequest);
			break;
		}
		
		if(ndfsReplyLogoff->Status != NDFS_LOGOFF_SUCCESS)
		{
			ASSERT(LFS_BUG);
			DereferenceSecondaryRequest(
				secondaryRequest
				);
			returnStatus = STATUS_INVALID_PARAMETER;
			break;
		}
	
		DereferenceSecondaryRequest(
				secondaryRequest
			);

		returnStatus = STATUS_SUCCESS;

	} while(0);
	
		LpxTdiDisconnect(Secondary->Thread.ConnectionFileObject, 0);
		LpxTdiDisassociateAddress(Secondary->Thread.ConnectionFileObject);
		LpxTdiCloseConnection(
			Secondary->Thread.ConnectionFileHandle, 
			Secondary->Thread.ConnectionFileObject
			);
		Secondary->Thread.ConnectionFileHandle = NULL;
		Secondary->Thread.ConnectionFileObject = NULL;

	LpxTdiCloseAddress (
		Secondary->Thread.AddressFileHandle,
		Secondary->Thread.AddressFileObject
		);
	Secondary->Thread.AddressFileObject = NULL;
	Secondary->Thread.AddressFileHandle = NULL;
		
	return returnStatus;
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

	DES_CBC_CTX					desCbcContext;
	unsigned char				iv[8];
	int							desResult;

	NTSTATUS					tdiStatus;
	_U32						remaninigDataSize;


	ASSERT(!BooleanFlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_ERROR));

	ndfsRequestHeader = &SecondaryRequest->NdfsRequestHeader;

	ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
	requestDataSize = ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER);
	encryptedMessage = (_U8 *)ndfsRequestHeader + ndfsRequestHeader->MessageSize;

	ASSERT(ndfsRequestHeader->MessageSize >= sizeof(NDFS_REQUEST_HEADER));

	ASSERT(ndfsRequestHeader->Uid == Secondary->Thread.SessionContext.Uid);
	ASSERT(ndfsRequestHeader->Tid == Secondary->Thread.SessionContext.Tid);
	
	//ASSERT(requestDataSize <= Secondary->Thread.SessionContext.PrimaryMaxBufferSize && ndfsRequestHeader->MessageSecurity == 0);

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

		ASSERT(ndfsRequestHeader->MessageSecurity == 1);
		
		RtlZeroMemory(&desCbcContext, sizeof(desCbcContext));
		RtlZeroMemory(iv, sizeof(iv));
		DES_CBCInit(&desCbcContext, Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetDiskInformation.Password, iv, DES_ENCRYPT);
		
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
	}

	ASSERT(requestDataSize > Secondary->Thread.SessionContext.PrimaryMaxDataSize);	

	ndfsRequestHeader->Splitted = 1;

	if(ndfsRequestHeader->MessageSecurity)
	{
		RtlZeroMemory(&desCbcContext, sizeof(desCbcContext));
		RtlZeroMemory(iv, sizeof(iv));
		DES_CBCInit(&desCbcContext, Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetDiskInformation.Password, iv, DES_ENCRYPT);
		
		desResult = DES_CBCUpdate(&desCbcContext, encryptedMessage, (_U8 *)ndfsWinxpRequestHeader, sizeof(NDFS_WINXP_REQUEST_HEADER));
		ASSERT(desResult == IDOK);

		RtlCopyMemory(
			ndfsWinxpRequestHeader,
			encryptedMessage,
			sizeof(NDFS_WINXP_REQUEST_HEADER)
			);
	}

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


	if(ReceiveHeader == TRUE)
	{
		tdiStatus = RecvMessage(
						Secondary->Thread.ConnectionFileObject,
						SecondaryRequest->NdfsMessage,
						sizeof(NDFS_REPLY_HEADER),
						NULL
						);

		if(tdiStatus != STATUS_SUCCESS)
		{
			return STATUS_REMOTE_DISCONNECT;
		}
	}

	ndfsReplyHeader = &SecondaryRequest->NdfsReplyHeader;
		
	if(!(
		RtlEqualMemory(ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol))
		&& ndfsReplyHeader->Status == NDFS_SUCCESS
		))
	{
		ASSERT(LFS_BUG);
		return STATUS_UNSUCCESSFUL;
	}

	//ASSERT(ndfsReplyHeader->Splitted == 0 && ndfsReplyHeader->MessageSecurity == 0);
	
	if(ndfsReplyHeader->Splitted == 0)
	{
		ndfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)(ndfsReplyHeader+1);
		ASSERT(ndfsWinxpReplyHeader == (PNDFS_WINXP_REPLY_HEADER)SecondaryRequest->NdfsReplyData);
		encryptedMessage = (_U8 *)ndfsReplyHeader + ndfsReplyHeader->MessageSize;

		if(ndfsReplyHeader->MessageSecurity == 0)
		{
			tdiStatus = RecvMessage(
							Secondary->Thread.ConnectionFileObject,
							(_U8 *)ndfsWinxpReplyHeader,
							ndfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER),
							NULL
							);
			if(tdiStatus != STATUS_SUCCESS)
			{
				return STATUS_REMOTE_DISCONNECT;
			}
		}
		else
		{
			int desResult;

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
			DES_CBCInit(&SecondaryRequest->DesCbcContext, Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetDiskInformation.Password, SecondaryRequest->Iv, DES_DECRYPT);
			desResult = DES_CBCUpdate(&SecondaryRequest->DesCbcContext, (_U8 *)ndfsWinxpReplyHeader, encryptedMessage, sizeof(NDFS_WINXP_REPLY_HEADER));
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
					desResult = DES_CBCUpdate(&SecondaryRequest->DesCbcContext, (_U8 *)(ndfsWinxpReplyHeader+1), encryptedMessage, ndfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REPLY_HEADER));
					ASSERT(desResult == IDOK);
				}
			}
		}
		
		return STATUS_SUCCESS;
	}

	ASSERT(ndfsReplyHeader->Splitted == 1);
		
	ndfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)(ndfsReplyHeader+1);
	ASSERT(ndfsWinxpReplyHeader == (PNDFS_WINXP_REPLY_HEADER)SecondaryRequest->NdfsReplyData);

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
		DES_CBCInit(&SecondaryRequest->DesCbcContext, Secondary->LfsDeviceExt->NetdiskPartitionInformation.NetDiskInformation.Password, SecondaryRequest->Iv, DES_DECRYPT);
		desResult = DES_CBCUpdate(&SecondaryRequest->DesCbcContext, (_U8 *)ndfsWinxpReplyHeader, encryptedMessage, sizeof(NDFS_WINXP_REPLY_HEADER));
		ASSERT(desResult == IDOK);
	}
	else
	{
		tdiStatus = RecvMessage(
						Secondary->Thread.ConnectionFileObject,
						(_U8 *)ndfsWinxpReplyHeader,
						sizeof(NDFS_WINXP_REPLY_HEADER),
						NULL
						);

		if(tdiStatus != STATUS_SUCCESS)
		{
			return STATUS_REMOTE_DISCONNECT;
		}
	}

	while(1)
	{
		PNDFS_REPLY_HEADER	splitNdfsReplyHeader = &Secondary->Thread.SplitNdfsReplyHeader;
		
		tdiStatus = RecvMessage(
						Secondary->Thread.ConnectionFileObject,
						(_U8 *)splitNdfsReplyHeader,
						sizeof(NDFS_REPLY_HEADER),
						NULL
						);

		if(tdiStatus != STATUS_SUCCESS)
		{
			return STATUS_REMOTE_DISCONNECT;
		}
			
		SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_NOISE,
					("ndfsReplyHeader->MessageSize = %d splitNdfsReplyHeader.MessageSize = %d\n", ndfsReplyHeader->MessageSize, splitNdfsReplyHeader->MessageSize));

		if(!(
			RtlEqualMemory(ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol))
			&& ndfsReplyHeader->Status == NDFS_SUCCESS
			))
		{
			ASSERT(LFS_BUG);
			return STATUS_UNSUCCESSFUL;
		}

		if(splitNdfsReplyHeader->MessageSecurity)
		{
			int desResult;

			tdiStatus = RecvMessage(
							Secondary->Thread.ConnectionFileObject,
							encryptedMessage,
							splitNdfsReplyHeader->Splitted ? Secondary->Thread.SessionContext.SecondaryMaxDataSize : (splitNdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER)),
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
		{
			tdiStatus = RecvMessage(
							Secondary->Thread.ConnectionFileObject,
							(_U8 *)ndfsWinxpReplyHeader + ndfsReplyHeader->MessageSize - splitNdfsReplyHeader->MessageSize, 
							splitNdfsReplyHeader->Splitted ? Secondary->Thread.SessionContext.SecondaryMaxDataSize : (splitNdfsReplyHeader->MessageSize - sizeof(NDFS_REPLY_HEADER)),
							NULL
							);

			if(tdiStatus != STATUS_SUCCESS)
			{
				return STATUS_REMOTE_DISCONNECT;
			}
		}
	
		if(splitNdfsReplyHeader->Splitted)
			continue;

		return STATUS_SUCCESS;
	}
}
		

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
			ASSERT(LFS_UNEXPECTED);
			return FALSE;
		}
		attributeRecordHeader = (PATTRIBUTE_RECORD_HEADER)((PCHAR)FileRecordSegmentHeader + attributeOffset);
				
		SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE,
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
			ASSERT(LFS_UNEXPECTED);
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
			ASSERT(LFS_UNEXPECTED);
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
			ASSERT(LFS_UNEXPECTED);
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
			Secondary->LfsDeviceExt->DiskDeviceObject,
			OutputBuffer,
			Length,
			StartingOffset,
			&event,
			&ioStatusBlock
		    );

    if (irp == NULL) 
	{
		ASSERT(LFS_INSUFFICIENT_RESOURCES);
		return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = IoCallDriver(Secondary->LfsDeviceExt->DiskDeviceObject, irp);

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
	NTSTATUS					returnStatus = DBG_CONTINUE; // for debbuging
	IO_STATUS_BLOCK				ioStatusBlock;

	// ndfsWinxpRequestHeader can be updated by outputBuffer Must be careful
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)SecondaryRequest->NdfsRequestData;
	_U8							*outputBuffer = (_U8*)SecondaryRequest->NdfsMessage + sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER); 		

	_U32						irpTag			 = ndfsWinxpRequestHeader->IrpTag;
	_U8							irpMajorFunction = ndfsWinxpRequestHeader->IrpMajorFunction;
	_U8							irpMinorFunction = ndfsWinxpRequestHeader->IrpMinorFunction;
	_U32						irpFlags		 = ndfsWinxpRequestHeader->IrpFlags;
	
	PFILE_EXTENTION				fileExt = SecondaryRequest->FileExt;
	PLFS_FCB					fcb = fileExt->Fcb;

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

		if(fileExt->TypeOfOpen != UserFileOpen)
		{
			returnStatus = STATUS_UNSUCCESSFUL;
			break;
		}
		if(read.Length == 0)
		{
			returnStatus = STATUS_UNSUCCESSFUL;
			break;
		}
		if(read.Key != 0)
		{
			returnStatus = STATUS_UNSUCCESSFUL;
			break;
		}

		lookupResult = 	NtfsLookupInFileRecord(
							Secondary,
							(PFILE_RECORD_SEGMENT_HEADER)fcb->Buffer,
							$STANDARD_INFORMATION,
							NULL,
							&attributeRecordHeader
							);

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
					("DirectProcessSecondaryRequest: IRP_MJ_READ LookupAllocation lookupResult = %d\n", lookupResult));
		
		if(lookupResult == FALSE || attributeRecordHeader->FormCode != RESIDENT_FORM)
		{
			ASSERT(LFS_UNEXPECTED);
			returnStatus = STATUS_UNSUCCESSFUL;
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
			returnStatus = STATUS_UNSUCCESSFUL;
			break;
		}

		lookupResult = 	NtfsLookupInFileRecord(
							Secondary,
							(PFILE_RECORD_SEGMENT_HEADER)fcb->Buffer,
							$DATA,
							NULL,
							&attributeRecordHeader
							);

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
					("DirectProcessSecondaryRequest: IRP_MJ_READ LookupAllocation lookupResult = %d\n", lookupResult)); 		

		if(lookupResult == FALSE)
		{
			//ASSERT(LFS_UNEXPECTED);
			returnStatus = STATUS_UNSUCCESSFUL;
			break;
		}
		
		if(NtfsLookupInFileRecord(
						Secondary, 
						(PFILE_RECORD_SEGMENT_HEADER)fcb->Buffer,
						$DATA, attributeRecordHeader, NULL) == TRUE
						)
		{
			returnStatus = STATUS_UNSUCCESSFUL;
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
				returnStatus = STATUS_UNSUCCESSFUL;
				break;
			}

			if(read.ByteOffset.HighPart != 0)
			{
				ASSERT(LFS_UNEXPECTED);
				returnStatus = STATUS_UNSUCCESSFUL;
			}

			if(attributeRecordHeader->Form.Resident.ValueLength == 0)
			{
				returnStatus = STATUS_UNSUCCESSFUL;
				break;
			}
			
			if(attributeRecordHeader->Form.Resident.ValueLength <= read.ByteOffset.LowPart)
			{	
				ioStatusBlock.Information = 0;
				ioStatusBlock.Status = STATUS_END_OF_FILE;
				returnStatus = STATUS_SUCCESS;
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
						
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
					("DirectProcessSecondaryRequest: IRP_MJ_READ RESIDENT_FORM readLength = %d\n", readLength));

			currentByteOffset.QuadPart = readLength;
			
			ioStatusBlock.Information  = readLength;
			ioStatusBlock.Status = STATUS_SUCCESS;
							
			returnStatus = STATUS_SUCCESS;
			break;
		}

		ASSERT(attributeRecordHeader->Form.Nonresident.LowestVcn == 0);
		ASSERT(attributeRecordHeader->FormCode == NONRESIDENT_FORM);

		if(attributeRecordHeader->Form.Nonresident.CompressionUnit)
		{
			returnStatus = STATUS_UNSUCCESSFUL;
			break;
		}
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
					("DirectProcessSecondaryRequest: IRP_MJ_READ NONRESIDENT_FORM read.Length = %d, read.Key = %d, %d\n", 
					read.Length, read.Key, read.ByteOffset.QuadPart % Secondary->Thread.SessionContext.BytesPerSector != 0));

		if(attributeRecordHeader->Form.Nonresident.ValidDataLength < attributeRecordHeader->Form.Nonresident.FileSize)
		{
			returnStatus = STATUS_UNSUCCESSFUL;
			break;
		}

#if 0
		if(attributeRecordHeader->Form.Nonresident.FileSize <= 10*1024*1024)
		{
			returnStatus = STATUS_UNSUCCESSFUL;
			break;
		}
#endif

		if(attributeRecordHeader->Form.Nonresident.FileSize <= read.ByteOffset.QuadPart)
		{
			ASSERT(ioStatusBlock.Information == 0);

			ioStatusBlock.Information = 0;
			ioStatusBlock.Status = STATUS_END_OF_FILE;
			returnStatus = STATUS_SUCCESS;
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
				ASSERT(LFS_UNEXPECTED);
				ioStatusBlock.Information = 0;
				break;
			}

			remainDataSizeInClulsters
				= (clusterCount << Secondary->Thread.SessionContext.ClusterShift) 
					- (currentByteOffset.QuadPart - (startVcn << Secondary->Thread.SessionContext.ClusterShift));

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
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
					
			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
					("DirectProcessSecondaryRequest: IRP_MJ_READ NONRESIDENT_FORM %ws LookupAllocation lookupResult = %d, vcn = %I64x, lcn = %I64x, ClusterCount = %I64x\n",
					&fcb->FullFileName, lookupResult, vcn, lcn, clusterCount));
				
			SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE, 
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
			
				SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE, ("readStatus = %x\n", readStatus));

				if(readStatus != STATUS_SUCCESS)
				{
					ASSERT(LFS_UNEXPECTED);
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

				SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE, ("readStatus = %x\n", readStatus));

				if(readStatus != STATUS_SUCCESS)
				{
					ASSERT(LFS_UNEXPECTED);
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
			
				SPY_LOG_PRINT(LFS_DEBUG_SECONDARY_TRACE, ("readStatus = %x\n", readStatus));

				if(readStatus != STATUS_SUCCESS)
				{
					ASSERT(LFS_UNEXPECTED);
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
			returnStatus = STATUS_SUCCESS;
			break;
		}
		
		returnStatus = STATUS_UNSUCCESSFUL;
		break;
	}
	default:

		ASSERT(LFS_BUG);
		returnStatus = STATUS_UNSUCCESSFUL;

		break;
	}

	if(returnStatus == STATUS_SUCCESS)
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
			ndfsWinxpReplyHeader->Information = ioStatusBlock.Information;

			break;
		}
		default:

			ASSERT(LFS_BUG);
			returnStatus = STATUS_UNSUCCESSFUL;

			break;
		}
	}

	ASSERT(returnStatus != DBG_CONTINUE);

	return returnStatus;
}


NTSTATUS
ProcessSecondaryRequest(
	IN	PSECONDARY			Secondary,
	IN  PSECONDARY_REQUEST	SecondaryRequest
	)
{
	NTSTATUS					returnStatus;
	NTSTATUS					sendStatus;
	


	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
				("ProcessSecondaryRequest: Start SecondaryRequest = %p\n", SecondaryRequest));

	sendStatus = SendNdfsRequestMessage(Secondary, SecondaryRequest);

	if(sendStatus != STATUS_SUCCESS)
	{
		ASSERT(sendStatus == STATUS_REMOTE_DISCONNECT);
		SecondaryRequest->ExecuteStatus = sendStatus;
		return sendStatus;
	}

	returnStatus = ReceiveNdfsReplyMessage(Secondary, SecondaryRequest, TRUE);

	if(returnStatus == STATUS_SUCCESS)
	{
		returnStatus = DispatchReply(
						Secondary,
						SecondaryRequest,
						(PNDFS_WINXP_REPLY_HEADER)SecondaryRequest->NdfsReplyData
						);
	}else
		ASSERT(returnStatus == STATUS_REMOTE_DISCONNECT);		
	
	ASSERT(SecondaryRequest->Synchronous == TRUE);
	SecondaryRequest->ExecuteStatus = returnStatus;
	
	return returnStatus;
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
		ASSERT(SecondaryRequest->NdfsReplyHeader.MessageSize == sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_WINXP_REPLY_HEADER));
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
#if DBG

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

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
					("DispatchReply: %s %s (%d %d): is not successful\n", irpMinorString, irpMinorString, 
					NdfsWinxpReplytHeader->IrpMajorFunction, NdfsWinxpReplytHeader->IrpMinorFunction));

#endif
					
			ntStatus = STATUS_SUCCESS;
			break;
		}

		ntStatus = STATUS_SUCCESS;
		break;
	}
	
	case IRP_MJ_INTERNAL_DEVICE_CONTROL:	// 0x0fb
	case IRP_MJ_SHUTDOWN:					// 0x10
	case IRP_MJ_CREATE_MAILSLOT:			// 0x13
	case IRP_MJ_POWER:						// 0x16
	case IRP_MJ_SYSTEM_CONTROL:				// 0x17
	case IRP_MJ_DEVICE_CHANGE:				// 0x18
	case IRP_MJ_PNP:						// 0x1b
	default:

		ASSERT(LFS_BUG);
		return STATUS_UNSUCCESSFUL;
	}

	return ntStatus;
}
