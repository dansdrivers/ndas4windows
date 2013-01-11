#include "LfsProc.h"


VOID
DispatchWinXpRequestWorker0 (
	IN  PPRIMARY_SESSION	PrimarySession
    )
{
	DispatchWinXpRequestWorker( PrimarySession, 0 );
}


VOID
DispatchWinXpRequestWorker1 (
	IN  PPRIMARY_SESSION	PrimarySession
    )
{
	DispatchWinXpRequestWorker( PrimarySession, 1 );
}


VOID
DispatchWinXpRequestWorker2 (
	IN  PPRIMARY_SESSION	PrimarySession
    )
{
	DispatchWinXpRequestWorker( PrimarySession, 2 );
}


VOID
DispatchWinXpRequestWorker3 (
	IN  PPRIMARY_SESSION	PrimarySession
    )
{
	DispatchWinXpRequestWorker( PrimarySession, 3 );
}

NTSTATUS
DispatchRequest (
	IN PPRIMARY_SESSION	PrimarySession
	)
{
	NTSTATUS				status;
	IN PNDFS_REQUEST_HEADER	ndfsRequestHeader;


	ASSERT( PrimarySession->Thread.NdfsRequestHeader.Mid < PrimarySession->SessionContext.SessionSlotCount );

	RtlCopyMemory( PrimarySession->Thread.SessionSlot[PrimarySession->Thread.NdfsRequestHeader.Mid].RequestMessageBuffer,
				   &PrimarySession->Thread.NdfsRequestHeader,
				   sizeof(NDFS_REQUEST_HEADER) );

	ndfsRequestHeader 
		= (PNDFS_REQUEST_HEADER)PrimarySession->Thread.SessionSlot[PrimarySession->Thread.NdfsRequestHeader.Mid].RequestMessageBuffer;
   
	ASSERT (PrimarySession->Thread.TdiReceiveContext.Result == sizeof(NDFS_REQUEST_HEADER) );

    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
				   ("DispatchRequest: PrimarySession = %p, ndfsRequestHeader->Command = %d\n", 
				   PrimarySession, ndfsRequestHeader->Command) );

	
	switch (ndfsRequestHeader->Command) {

	case NDFS_COMMAND_NEGOTIATE: {

		PNDFS_REQUEST_NEGOTIATE	ndfsRequestNegotiate;
		PNDFS_REPLY_HEADER		ndfsReplyHeader;
		PNDFS_REPLY_NEGOTIATE	ndfsReplyNegotiate;
		
		
		if (PrimarySession->Thread.SessionState != SESSION_CLOSE) {

			ASSERT( LFS_BUG );
			status = STATUS_UNSUCCESSFUL;

			break;
		}
		
		ASSERT( ndfsRequestHeader->MessageSize == sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_REQUEST_NEGOTIATE) );
		ndfsRequestNegotiate = (PNDFS_REQUEST_NEGOTIATE)(ndfsRequestHeader+1);
		
		status = RecvMessage( PrimarySession->ConnectionFileObject,
							 (_U8 *)ndfsRequestNegotiate,
							 sizeof(NDFS_REQUEST_NEGOTIATE),
							 NULL );
	
		if (status != STATUS_SUCCESS) {

			ASSERT( LFS_BUG );

			break;
		}

		PrimarySession->SessionContext.Flags = ndfsRequestNegotiate->Flags;
		ndfsReplyHeader = (PNDFS_REPLY_HEADER)(ndfsRequestNegotiate+1);

		RtlCopyMemory( ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol) );
		ndfsReplyHeader->Status		= NDFS_SUCCESS;
		ndfsReplyHeader->Flags	    = PrimarySession->SessionContext.Flags;
		ndfsReplyHeader->Uid		= 0;
		ndfsReplyHeader->Tid		= 0;
		ndfsReplyHeader->Mid		= 0;
		ndfsReplyHeader->MessageSize = sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_NEGOTIATE);

		ndfsReplyNegotiate = (PNDFS_REPLY_NEGOTIATE)(ndfsReplyHeader+1);

		if (ndfsRequestNegotiate->NdfsMajorVersion == NDFS_PROTOCOL_MAJOR_2 && 
			ndfsRequestNegotiate->NdfsMinorVersion == NDFS_PROTOCOL_MINOR_0 && 
			ndfsRequestNegotiate->OsMajorType == OS_TYPE_WINDOWS			&& 
			ndfsRequestNegotiate->OsMinorType == OS_TYPE_WINXP) {

			PrimarySession->SessionContext.NdfsMajorVersion = ndfsRequestNegotiate->NdfsMajorVersion;
			PrimarySession->SessionContext.NdfsMinorVersion = ndfsRequestNegotiate->NdfsMinorVersion;

			ndfsReplyNegotiate->Status = NDFS_NEGOTIATE_SUCCESS;
			ndfsReplyNegotiate->NdfsMajorVersion = PrimarySession->SessionContext.NdfsMajorVersion;
			ndfsReplyNegotiate->NdfsMinorVersion = PrimarySession->SessionContext.NdfsMinorVersion;
			ndfsReplyNegotiate->OsMajorType = OS_TYPE_WINDOWS;	
			ndfsReplyNegotiate->OsMinorType = OS_TYPE_WINXP;
			ndfsReplyNegotiate->SessionKey = PrimarySession->SessionContext.SessionKey;
			ndfsReplyNegotiate->MaxBufferSize = PrimarySession->SessionContext.PrimaryMaxDataSize;

			RtlCopyMemory( ndfsReplyNegotiate->ChallengeBuffer,
						   &PrimarySession,
						   sizeof(PPRIMARY_SESSION) );

			ndfsReplyNegotiate->ChallengeLength = sizeof(PPRIMARY_SESSION);

			PrimarySession->Thread.SessionState = SESSION_NEGOTIATE;
		
		} else {

			ndfsReplyNegotiate->Status = NDFS_NEGOTIATE_UNSUCCESSFUL;
		}

		status = SendMessage( PrimarySession->ConnectionFileObject,
							 (_U8 *)ndfsReplyHeader,
							 ndfsReplyHeader->MessageSize,
							 NULL,
							 &PrimarySession->Thread.TransportCtx );

		if (status != STATUS_SUCCESS) {

			break;
		}

		break;
	
	}

	case NDFS_COMMAND_SETUP: {

		PNDFS_REQUEST_SETUP	ndfsRequestSetup;
		PNDFS_REPLY_HEADER	ndfsReplyHeader;
		PNDFS_REPLY_SETUP	ndfsReplySetup;

		_U8					ndfsReplySetupStatus;

		unsigned char		idData[1];
		MD5_CTX				context;
		_U8					responseBuffer[16]; 
		
		
		if (PrimarySession->Thread.SessionState != SESSION_NEGOTIATE) {

			ASSERT( LFS_BUG );
			status = STATUS_UNSUCCESSFUL;

			break;
		}
		
		ASSERT(ndfsRequestHeader->MessageSize == sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_REQUEST_SETUP));
		ndfsRequestSetup = (PNDFS_REQUEST_SETUP)(ndfsRequestHeader+1);
		
		status = RecvMessage( PrimarySession->ConnectionFileObject,
							 (_U8 *)ndfsRequestSetup,
							 sizeof(NDFS_REQUEST_SETUP),
							 NULL );
	
		if (status != STATUS_SUCCESS) {

			ASSERT( LFS_BUG );

			break;
		}
	
		do {

			ASSERT( PrimarySession->NetdiskPartition == NULL );

			if (ndfsRequestSetup->SessionKey != PrimarySession->SessionContext.SessionKey) {

				ndfsReplySetupStatus = NDFS_SETUP_UNSUCCESSFUL;				
				break;
			}

			RtlCopyMemory( PrimarySession->NetDiskAddress.Node,
						   ndfsRequestSetup->NetDiskNode,
						   6 );
			
			PrimarySession->NetDiskAddress.Port = HTONS(ndfsRequestSetup->NetDiskPort);//HTONS(ndfsRequestSetup->NetDiskPort);
			PrimarySession->UnitDiskNo = ndfsRequestSetup->UnitDiskNo;
			RtlCopyMemory( PrimarySession->NdscId, ndfsRequestSetup->NdscId, NDSC_ID_LENGTH);
				
			if (PrimarySession->SessionContext.NdfsMinorVersion == NDFS_PROTOCOL_MINOR_0) {

				status = NetdiskManager_GetPrimaryPartition( GlobalLfs.NetdiskManager,
															 PrimarySession,
															 &PrimarySession->NetDiskAddress,
															 PrimarySession->UnitDiskNo,
															 PrimarySession->NdscId,
															 NULL,
															 PrimarySession->IsLocalAddress,
															 &PrimarySession->NetdiskPartition,
															 &PrimarySession->NetdiskPartitionInformation,
															 &PrimarySession->FileSystemType );

				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
							   ("PRIM:SETUP:MIN1 PrimarySession->NetdiskPartition = %p netDiskPartitionInfo.StartingOffset = %I64x\n",
								 PrimarySession->NetdiskPartition, PrimarySession->StartingOffset.QuadPart) );

				if (status != STATUS_SUCCESS) {

					ndfsReplySetupStatus = NDFS_SETUP_UNSUCCESSFUL;
					break;
				}
			
			} else {

				LfsDbgBreakPoint();
			}

			
			MD5Init( &context );

			/* id byte */
			idData[0] = (unsigned char)PrimarySession->SessionContext.SessionKey;
			
			MD5Update( &context, idData, 1 );

			MD5Update( &context, 
					   PrimarySession->NetdiskPartitionInformation.NetdiskInformation.Password, 
					   8 );

			MD5Update( &context, &(UCHAR)PrimarySession, sizeof(PPRIMARY_SESSION) );
			MD5Final( responseBuffer, &context );

			if (!RtlEqualMemory(ndfsRequestSetup->ResponseBuffer,
								responseBuffer,
								16)) {

				ASSERT( LFS_BUG );
				ndfsReplySetupStatus = NDFS_SETUP_UNSUCCESSFUL;				
				break;
			}

			ndfsReplySetupStatus = NDFS_SETUP_SUCCESS;
		
		} while(0);

		ndfsReplyHeader = (PNDFS_REPLY_HEADER)(ndfsRequestSetup+1);
			
		RtlCopyMemory( ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol) );
		
		ndfsReplyHeader->Status		= NDFS_SUCCESS;
		ndfsReplyHeader->Flags	    = 0;
		ndfsReplyHeader->Uid		= 0;
		ndfsReplyHeader->Tid		= 0;
		ndfsReplyHeader->Mid		= 0;
		ndfsReplyHeader->MessageSize = sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_SETUP);

		if (ndfsReplySetupStatus == NDFS_SETUP_SUCCESS) {

			if (ndfsRequestSetup->MaxBufferSize) {

				if (PrimarySession->NetdiskPartition->FileSystemType == LFS_FILE_SYSTEM_NDAS_NTFS ||
					PrimarySession->NetdiskPartition->FileSystemType == LFS_FILE_SYSTEM_NDAS_FAT) {

					PrimarySession->SessionContext.SecondaryMaxDataSize = ndfsRequestSetup->MaxBufferSize;
				
				} else {

					PrimarySession->SessionContext.SecondaryMaxDataSize = 
						(ndfsRequestSetup->MaxBufferSize <= PrimarySession->SessionContext.SecondaryMaxDataSize) ? 
							   ndfsRequestSetup->MaxBufferSize : PrimarySession->SessionContext.SecondaryMaxDataSize;
				}

				//
				//	Initialize transport context for traffic control
				//

				InitTransCtx(&PrimarySession->Thread.TransportCtx, PrimarySession->SessionContext.SecondaryMaxDataSize);
			}

			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
						   ("NDFS_COMMAND_SETUP: PrimarySession->NetdiskPartition->FileSystemType = %d "  
						    "ndfsRequestSetup->MaxBufferSize = %x PrimaryMaxDataSize:%x SecondaryMaxDataSize:%x \n",
							PrimarySession->NetdiskPartition->FileSystemType, ndfsRequestSetup->MaxBufferSize, 
							PrimarySession->SessionContext.PrimaryMaxDataSize,
							PrimarySession->SessionContext.SecondaryMaxDataSize) );

			ndfsReplyHeader->Uid = PrimarySession->SessionContext.Uid;
			ndfsReplyHeader->Tid = PrimarySession->SessionContext.Tid;
		
		} else {

			if (PrimarySession->NetdiskPartition) {

				NetdiskManager_ReturnPrimaryPartition( GlobalLfs.NetdiskManager, 
													   PrimarySession,
													   PrimarySession->NetdiskPartition, 
													   PrimarySession->IsLocalAddress );

				PrimarySession->NetdiskPartition = NULL;
			}
		}

		ndfsReplySetup = (PNDFS_REPLY_SETUP)(ndfsReplyHeader+1);
		ndfsReplySetup->Status = ndfsReplySetupStatus;


		status = SendMessage( PrimarySession->ConnectionFileObject,
							 (_U8 *)ndfsReplyHeader,
							 ndfsReplyHeader->MessageSize,
							 NULL,
							 &PrimarySession->Thread.TransportCtx );

		if (status != STATUS_SUCCESS) {

			break;
		}

		if (ndfsReplySetupStatus == NDFS_SETUP_SUCCESS)
			PrimarySession->Thread.SessionState = SESSION_SETUP;
		
		break;
	}

	case NDFS_COMMAND_TREE_CONNECT:{

		PNDFS_REQUEST_TREE_CONNECT	ndfsRequestTreeConnect;
		PNDFS_REPLY_HEADER			ndfsReplyHeader;
		PNDFS_REPLY_TREE_CONNECT	ndfsReplyTreeConnect;
	
		_U8							ndfsReplyTreeConnectStatus;
		
		
		if (!(PrimarySession->Thread.SessionState == SESSION_SETUP && \
			  ndfsRequestHeader->Uid == PrimarySession->SessionContext.Uid)) {

			ASSERT( LFS_BUG );
			status = STATUS_UNSUCCESSFUL;

			break;
		}
		
		ASSERT( ndfsRequestHeader->MessageSize == sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_REQUEST_TREE_CONNECT) );
		ndfsRequestTreeConnect = (PNDFS_REQUEST_TREE_CONNECT)(ndfsRequestHeader+1);

		status = RecvMessage( PrimarySession->ConnectionFileObject,
							 (_U8 *)ndfsRequestTreeConnect,
							 sizeof(NDFS_REQUEST_TREE_CONNECT),
							 NULL );
	
		if (status != STATUS_SUCCESS) {

			ASSERT( LFS_BUG );

			break;
		}
		
		do {

			NTSTATUS			getVolumeInformationStatus;
			PNETDISK_PARTITION	netdiskPartition;


			ndfsReplyHeader = (PNDFS_REPLY_HEADER)(ndfsRequestTreeConnect+1);

			RtlCopyMemory( ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol) );

			ndfsReplyHeader->Status		= NDFS_SUCCESS;
			ndfsReplyHeader->Flags	    = 0;
			ndfsReplyHeader->Uid		= PrimarySession->SessionContext.Uid;
			ndfsReplyHeader->Tid		= 0;
			ndfsReplyHeader->Mid		= 0;
			ndfsReplyHeader->MessageSize = sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_TREE_CONNECT);

			PrimarySession->StartingOffset.QuadPart = ndfsRequestTreeConnect->StartingOffset;

			status = NetdiskManager_GetPrimaryPartition( GlobalLfs.NetdiskManager,
														 PrimarySession,
														 &PrimarySession->NetDiskAddress,
														 PrimarySession->UnitDiskNo,
														 PrimarySession->NdscId,
														 &PrimarySession->StartingOffset,
														 PrimarySession->IsLocalAddress,
														 &netdiskPartition,
														 &PrimarySession->NetdiskPartitionInformation,
														 &PrimarySession->FileSystemType );

			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
						   ("PRIM:TREE_CONNECT: netdiskPartition = %p netDiskPartitionInfo.StartingOffset = %I64x\n", 
						    netdiskPartition, PrimarySession->StartingOffset.QuadPart) );

			if (status != STATUS_SUCCESS) {
				
				if (status == STATUS_UNRECOGNIZED_VOLUME) {

					SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
								   ("PRIM:TREE_CONNECT: Partition is not available\n") );

					ndfsReplyTreeConnectStatus = NDFS_TREE_CONNECT_NO_PARTITION;				
				
				} else {

					ndfsReplyTreeConnectStatus = NDFS_TREE_CONNECT_UNSUCCESSFUL;
				}
			
				break;
			}

			if (FlagOn(netdiskPartition->Flags, NETDISK_PARTITION_FLAG_MOUNT_CORRUPTED)) {

				ndfsReplyTreeConnectStatus = NDFS_TREE_CORRUPTED;

				NetdiskManager_ReturnPrimaryPartition( GlobalLfs.NetdiskManager, 
													   PrimarySession,
													   netdiskPartition, 
													   PrimarySession->IsLocalAddress );
				break;
			}

			if (netdiskPartition->FileSystemType == LFS_FILE_SYSTEM_NTFS && IS_WINDOWSXP_OR_LATER()) {

				getVolumeInformationStatus = 
					GetVolumeInformation( PrimarySession, 
										  &netdiskPartition->NetdiskPartitionInformation.VolumeName );
		
				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
							   ("PRIM:TREE_CONNECT: getVolumeInformationStatus = %x\n", 
								getVolumeInformationStatus) );

				if (getVolumeInformationStatus != STATUS_SUCCESS) {

					NetdiskManager_ReturnPrimaryPartition( GlobalLfs.NetdiskManager, 
														   PrimarySession,
														   netdiskPartition, 
														   PrimarySession->IsLocalAddress );

					ndfsReplyTreeConnectStatus = NDFS_TREE_CONNECT_UNSUCCESSFUL;				
					break;
				}
			}

			if (PrimarySession->NetdiskPartition) { 

				NetdiskManager_ReturnPrimaryPartition( GlobalLfs.NetdiskManager, 
													   PrimarySession,
													   PrimarySession->NetdiskPartition, 
													   PrimarySession->IsLocalAddress );
			
				PrimarySession->NetdiskPartition = NULL;
			
			} else {

				LfsDbgBreakPoint();
			}

			PrimarySession->NetdiskPartition = netdiskPartition;

			PrimarySession->SessionContext.Tid  = (_U16)PrimarySession->NetdiskPartition;
			ndfsReplyHeader->Tid = PrimarySession->SessionContext.Tid;

			ndfsReplyTreeConnectStatus = NDFS_TREE_CONNECT_SUCCESS;				
		
		} while(0);
		
		ndfsReplyTreeConnect = (PNDFS_REPLY_TREE_CONNECT)(ndfsReplyHeader+1);
		ndfsReplyTreeConnect->Status = ndfsReplyTreeConnectStatus;
		
		ndfsReplyTreeConnect->SessionSlotCount = SESSION_SLOT_COUNT;
		
		ndfsReplyTreeConnect->BytesPerFileRecordSegment	= PrimarySession->Thread.BytesPerFileRecordSegment;
		ndfsReplyTreeConnect->BytesPerSector			= PrimarySession->Thread.BytesPerSector;
		ndfsReplyTreeConnect->BytesPerCluster			= PrimarySession->Thread.BytesPerCluster;

		status = SendMessage( PrimarySession->ConnectionFileObject,
							 (_U8 *)ndfsReplyHeader,
							 ndfsReplyHeader->MessageSize,
							 NULL,
							 &PrimarySession->Thread.TransportCtx );

		if (status != STATUS_SUCCESS) {
		
			break;
		}

		if (ndfsReplyTreeConnectStatus == NDFS_TREE_CONNECT_SUCCESS) {

			status = PrimarySessionTakeOver( PrimarySession );

			if (status == STATUS_INVALID_DEVICE_REQUEST) {

				PrimarySession->Thread.SessionState = SESSION_TREE_CONNECT;
			
			} else {
				
				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
						   ("PrimarySessionTakeOver: Success PrimarySession = %p status = %x\n", 
						    PrimarySession, status) );

				if (PrimarySession->NetdiskPartition) {

					NetdiskManager_ReturnPrimaryPartition( GlobalLfs.NetdiskManager,
														   PrimarySession,
														   PrimarySession->NetdiskPartition, 
														   PrimarySession->IsLocalAddress );	

					PrimarySession->NetdiskPartition = NULL;
				}
						
				if (status == STATUS_SUCCESS) {

					PrimarySession->ConnectionFileHandle = NULL;
					PrimarySession->ConnectionFileObject = NULL;
					
				} else {

					DisconnectFromSecondary( PrimarySession );
				} 

				SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_DISCONNECTED );

				PrimarySession->Thread.SessionState = SESSION_CLOSED;
			}
		}

		status = STATUS_SUCCESS;

		break;
	}

	case NDFS_COMMAND_LOGOFF: {

		PNDFS_REQUEST_LOGOFF	ndfsRequestLogoff;
		PNDFS_REPLY_HEADER		ndfsReplyHeader;
		PNDFS_REPLY_LOGOFF		ndfsReplyLogoff;
		

		if(PrimarySession->SessionContext.NdfsMinorVersion == NDFS_PROTOCOL_MINOR_0) {

			if(PrimarySession->Thread.SessionState != SESSION_TREE_CONNECT) {

				ASSERT( LFS_BUG );
				status = STATUS_UNSUCCESSFUL;
				break;
			}
		}

		if (!(ndfsRequestHeader->Uid == PrimarySession->SessionContext.Uid && 
			  ndfsRequestHeader->Tid == PrimarySession->SessionContext.Tid)) {

			ASSERT( LFS_BUG );
			status = STATUS_UNSUCCESSFUL;

			break;
		}
		
		ASSERT( ndfsRequestHeader->MessageSize == sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_REQUEST_LOGOFF) );

		ndfsRequestLogoff = (PNDFS_REQUEST_LOGOFF)(ndfsRequestHeader+1);
		
		status = RecvMessage( PrimarySession->ConnectionFileObject,
							 (_U8 *)ndfsRequestLogoff,
							 sizeof(NDFS_REQUEST_LOGOFF),
							 NULL );
	
		if (status != STATUS_SUCCESS) {

			//ASSERT( LFS_BUG );

			break;
		}

		ndfsReplyHeader = (PNDFS_REPLY_HEADER)(ndfsRequestLogoff+1);

		RtlCopyMemory( ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol) );

		ndfsReplyHeader->Status		= NDFS_SUCCESS;
		ndfsReplyHeader->Flags	    = 0;
		ndfsReplyHeader->Uid		= PrimarySession->SessionContext.Uid;
		ndfsReplyHeader->Tid		= 0;
		ndfsReplyHeader->Mid		= 0;
		ndfsReplyHeader->MessageSize = sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_LOGOFF);

		ndfsReplyLogoff = (PNDFS_REPLY_LOGOFF)(ndfsReplyHeader+1);

		if (ndfsRequestLogoff->SessionKey != PrimarySession->SessionContext.SessionKey) {

			ndfsReplyLogoff->Status = NDFS_LOGOFF_UNSUCCESSFUL;
		
		} else {

			ndfsReplyLogoff->Status = NDFS_LOGOFF_SUCCESS;
		}

		status = SendMessage( PrimarySession->ConnectionFileObject,
							 (_U8 *)ndfsReplyHeader,
							 ndfsReplyHeader->MessageSize,
							 NULL,
							 &PrimarySession->Thread.TransportCtx );

		if (status != STATUS_SUCCESS) {

			break;
		}

		PrimarySession->Thread.SessionState = SESSION_CLOSED;
		LpxTdiDisconnect( PrimarySession->ConnectionFileObject, 0 );
		PrimarySession->Thread.Flags |= PRIMARY_SESSION_THREAD_FLAG_DISCONNECTED;

		break;
	}

	case NDFS_COMMAND_EXECUTE: {

		_U16	mid;

		if(PrimarySession->SessionContext.NdfsMinorVersion == NDFS_PROTOCOL_MINOR_0) {

			if (PrimarySession->Thread.SessionState != SESSION_TREE_CONNECT) {

				ASSERT( LFS_BUG );
				status = STATUS_UNSUCCESSFUL;
				break;
			}
		}

		if (!(ndfsRequestHeader->Uid == PrimarySession->SessionContext.Uid && 
			ndfsRequestHeader->Tid == PrimarySession->SessionContext.Tid)) {

			ASSERT( LFS_BUG );
			status = STATUS_UNSUCCESSFUL;

			break;
		}

		mid = ndfsRequestHeader->Mid;

		PrimarySession->Thread.SessionSlot[mid].RequestMessageBufferLength = sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER) + DEFAULT_MAX_DATA_SIZE;
		RtlZeroMemory( &PrimarySession->Thread.SessionSlot[mid].RequestMessageBuffer[sizeof(NDFS_REQUEST_HEADER)], 
					   PrimarySession->Thread.SessionSlot[mid].RequestMessageBufferLength - sizeof(NDFS_REQUEST_HEADER) );

		PrimarySession->Thread.SessionSlot[mid].ReplyMessageBufferLength = sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER) + DEFAULT_MAX_DATA_SIZE;
		RtlZeroMemory( PrimarySession->Thread.SessionSlot[mid].ReplyMessageBuffer, 
					   PrimarySession->Thread.SessionSlot[mid].ReplyMessageBufferLength );

		ASSERT( ndfsRequestHeader->MessageSize >= sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER) );
		status = ReceiveNtfsWinxpMessage(PrimarySession, mid );

		if (status != STATUS_SUCCESS)
			break;

		if (PrimarySession->Thread.SessionSlot[mid].State != SLOT_WAIT) {

			break;
		}
	
		PrimarySession->Thread.SessionSlot[mid].State = SLOT_EXECUTING;
		PrimarySession->Thread.IdleSlotCount --;

		if (PrimarySession->SessionContext.SessionSlotCount == 1) {

			ASSERT( mid == 0 );

			DispatchWinXpRequestWorker( PrimarySession, mid );

			PrimarySession->Thread.SessionSlot[mid].State = SLOT_WAIT;
			PrimarySession->Thread.IdleSlotCount ++;

			if (PrimarySession->Thread.SessionSlot[mid].Status == STATUS_SUCCESS) {

				PNDFS_REPLY_HEADER		ndfsReplyHeader;

				ndfsReplyHeader = (PNDFS_REPLY_HEADER)PrimarySession->Thread.SessionSlot[mid].ReplyMessageBuffer;
										
				PrimarySession->Thread.SessionSlot[mid].Status = 
					SendNdfsWinxpMessage( PrimarySession,
										  ndfsReplyHeader,
										  PrimarySession->Thread.SessionSlot[mid].NdfsWinxpReplyHeader,
										  PrimarySession->Thread.SessionSlot[mid].ReplyDataSize,
										  mid );

			}
	
			if (PrimarySession->Thread.SessionSlot[mid].ExtendWinxpRequestMessagePool) {

				ExFreePool( PrimarySession->Thread.SessionSlot[mid].ExtendWinxpRequestMessagePool );	
				PrimarySession->Thread.SessionSlot[mid].ExtendWinxpRequestMessagePool = NULL;
				PrimarySession->Thread.SessionSlot[mid].ExtendWinxpReplyMessagePoolLength = 0;
			}
		
			if (PrimarySession->Thread.SessionSlot[mid].ExtendWinxpReplyMessagePool) {

				ExFreePool( PrimarySession->Thread.SessionSlot[mid].ExtendWinxpReplyMessagePool );	
				PrimarySession->Thread.SessionSlot[mid].ExtendWinxpReplyMessagePool = NULL;
				PrimarySession->Thread.SessionSlot[mid].ExtendWinxpReplyMessagePoolLength = 0;
			}

			if (PrimarySession->Thread.SessionSlot[mid].Status == STATUS_PENDING)
				LfsDbgBreakPoint();			
			
			if (PrimarySession->Thread.SessionSlot[mid].Status != STATUS_SUCCESS) {

				SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_ERROR );
										
				status = PrimarySession->Thread.SessionSlot[mid].Status;
				break;		
			}
				
			status = STATUS_SUCCESS;
			break;
		}

		if (mid == 0)
			ExInitializeWorkItem( &PrimarySession->Thread.SessionSlot[mid].WorkQueueItem,
								  DispatchWinXpRequestWorker0,
								  PrimarySession );
		
		if (mid == 1)
			ExInitializeWorkItem( &PrimarySession->Thread.SessionSlot[mid].WorkQueueItem,
								  DispatchWinXpRequestWorker1,
								  PrimarySession );
		
		if (mid == 2)
			ExInitializeWorkItem( &PrimarySession->Thread.SessionSlot[mid].WorkQueueItem,
								  DispatchWinXpRequestWorker2,
								  PrimarySession );

		
		if (mid == 3)
			ExInitializeWorkItem( &PrimarySession->Thread.SessionSlot[mid].WorkQueueItem,
								  DispatchWinXpRequestWorker3,
								  PrimarySession );

		ExQueueWorkItem( &PrimarySession->Thread.SessionSlot[mid].WorkQueueItem, DelayedWorkQueue );	

		status = STATUS_PENDING;
		break;
	}

	default:

		ASSERT( LFS_LPX_BUG );
		status = STATUS_UNSUCCESSFUL;
		
		break;
	}

	return status;
}
