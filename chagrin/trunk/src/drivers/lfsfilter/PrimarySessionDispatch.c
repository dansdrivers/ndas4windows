#include "LfsProc.h"


VOID
DispatchWinXpRequestWorker (
	IN  PPRIMARY_SESSION	PrimarySession,
	IN  _U16				Mid
    );

NTSTATUS
GetVolumeInformation (
	IN PPRIMARY_SESSION	PrimarySession,
	IN PUNICODE_STRING	VolumeName
	);

_U32
CaculateReplyDataLength (
	IN 	PPRIMARY_SESSION			PrimarySession,
	IN  PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader
	);


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
   
	ASSERT (PrimarySession->Thread.ReceiveOverLapped.IoStatusBlock.Information == sizeof(NDFS_REQUEST_HEADER) );

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

				NDASFS_ASSERT( FALSE );
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
					GlobalLfs.NdasFatRwIndirect == FALSE && PrimarySession->NetdiskPartition->FileSystemType == LFS_FILE_SYSTEM_NDAS_FAT ||
					GlobalLfs.NdasNtfsRwIndirect == FALSE && PrimarySession->NetdiskPartition->FileSystemType == LFS_FILE_SYSTEM_NDAS_NTFS) {

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

				NDASFS_ASSERT( FALSE );
			}

			PrimarySession->NetdiskPartition = netdiskPartition;

			PrimarySession->SessionContext.Tid  = PrimarySession->NetdiskPartition->Tid;
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
		LpxTdiV2Disconnect( PrimarySession->ConnectionFileObject, 0 );
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
				NDASFS_ASSERT( FALSE );			
			
			if (PrimarySession->Thread.SessionSlot[mid].Status != STATUS_SUCCESS) {

				SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_ERROR );
										
				status = PrimarySession->Thread.SessionSlot[mid].Status;
				break;		
			}
				
			status = STATUS_SUCCESS;
			break;
		}

		NDASFS_ASSERT( FALSE );

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


VOID
DispatchWinXpRequestWorker (
	IN  PPRIMARY_SESSION	PrimarySession,
	IN  _U16				Mid
    )
{
	PNDFS_REQUEST_HEADER		ndfsRequestHeader = (PNDFS_REQUEST_HEADER)PrimarySession->Thread.SessionSlot[Mid].RequestMessageBuffer;
	PNDFS_REPLY_HEADER			ndfsReplyHeader = (PNDFS_REPLY_HEADER)PrimarySession->Thread.SessionSlot[Mid].ReplyMessageBuffer; 
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader = PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpRequestHeader;
	
	_U32						replyDataSize;

	
	NDASFS_ASSERT( Mid == ndfsRequestHeader->Mid );
	
    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("DispatchWinXpRequestWorker: entered PrimarySession = %p, ndfsRequestHeader->Command = %d\n", 
					PrimarySession, ndfsRequestHeader->Command) );

	NDASFS_ASSERT( PrimarySession->Thread.SessionSlot[Mid].State == SLOT_EXECUTING );

	replyDataSize = CaculateReplyDataLength(PrimarySession, ndfsWinxpRequestHeader);

	if (replyDataSize <= 
		(ULONG)(PrimarySession->SessionContext.SecondaryMaxDataSize || sizeof(PrimarySession->Thread.SessionSlot[Mid].ReplyMessageBuffer) - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER))) {

		if (ndfsRequestHeader->MessageSecurity == 1) {

			if (ndfsWinxpRequestHeader->IrpMajorFunction == IRP_MJ_READ && PrimarySession->SessionContext.RwDataSecurity == 0) {

				PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)(ndfsReplyHeader+1);
			
			} else {

				PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)PrimarySession->Thread.SessionSlot[Mid].CryptWinxpMessageBuffer;
			}
		
		} else {

			PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)(ndfsReplyHeader+1);
		}
	
	} else {

		PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePoolLength = ADD_ALIGN8(sizeof(NDFS_WINXP_REPLY_HEADER) + replyDataSize);
		PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool 
			= ExAllocatePoolWithTag( NonPagedPool,
									 PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePoolLength,
									 PRIMARY_SESSION_BUFFERE_TAG );

		NDASFS_ASSERT( PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool );
		
		if (PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool == NULL) {
		   
			PrimarySession->Thread.SessionSlot[Mid].Status = STATUS_INSUFFICIENT_RESOURCES; 
			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_ERROR, ("failed to allocate ExtendWinxpReplyMessagePool\n") );

			goto fail_replypoolalloc;
		}

		PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader 
			= (PNDFS_WINXP_REPLY_HEADER)PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool;
	}
	
    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
				   ("DispatchWinXpRequestWorker: PrimarySession = %p, ndfsRequestHeader->Command = %d\n", 
					 PrimarySession, ndfsRequestHeader->Command) );

	PrimarySession->Thread.SessionSlot[Mid].Status 
		= DispatchWinXpRequest( PrimarySession, 
								ndfsWinxpRequestHeader,
								PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader,
								ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER),
								&PrimarySession->Thread.SessionSlot[Mid].ReplyDataSize );

	ASSERT( PrimarySession->Thread.SessionSlot[Mid].Status == STATUS_SUCCESS );

    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
				   ("DispatchWinXpRequestWorker: Return PrimarySession = %p, ndfsRequestHeader->Command = %d\n", 
					 PrimarySession, ndfsRequestHeader->Command) );

fail_replypoolalloc:

	PrimarySession->Thread.SessionSlot[Mid].State = SLOT_FINISH;

	KeSetEvent( &PrimarySession->Thread.WorkCompletionEvent, IO_NO_INCREMENT, FALSE );

	return;
}


NTSTATUS
GetVolumeInformation (
	IN PPRIMARY_SESSION	PrimarySession,
	IN PUNICODE_STRING	VolumeName
	)
{
	HANDLE					volumeHandle = NULL;
    ACCESS_MASK				desiredAccess;
	ULONG					attributes;
	OBJECT_ATTRIBUTES		objectAttributes;
	IO_STATUS_BLOCK			ioStatusBlock;
	LARGE_INTEGER			allocationSize;
	ULONG					fileAttributes;
    ULONG					shareAccess;
    ULONG					createDisposition;
	ULONG					createOptions;
    PVOID					eaBuffer;
	ULONG					eaLength;

	NTSTATUS				createStatus;
	NTSTATUS				fsControlStatus;

	NTFS_VOLUME_DATA_BUFFER	ntfsVolumeDataBuffer;
	
#if DBG
#else
	UNREFERENCED_PARAMETER( PrimarySession );
#endif

	desiredAccess = SYNCHRONIZE | READ_CONTROL | FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES | FILE_WRITE_EA 
					| FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_READ_EA;

	ASSERT( desiredAccess == 0x0012019F );

	attributes  = OBJ_KERNEL_HANDLE;
	attributes |= OBJ_CASE_INSENSITIVE;

	InitializeObjectAttributes( &objectAttributes,
								VolumeName,
								attributes,
								NULL,
								NULL );
		
	allocationSize.LowPart  = 0;
	allocationSize.HighPart = 0;

	fileAttributes	  = 0;		
	shareAccess		  = FILE_SHARE_READ | FILE_SHARE_WRITE;
	createDisposition = FILE_OPEN;
	createOptions     = FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE;
	eaBuffer		  = NULL;
	eaLength		  = 0;
	

	RtlZeroMemory(&ioStatusBlock, sizeof(ioStatusBlock));

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
				   ("GetVolumeInformation: PrimarySession = %p\n", PrimarySession) );

	createStatus = ZwCreateFile( &volumeHandle,
								 desiredAccess,
								 &objectAttributes,
								 &ioStatusBlock,
								 &allocationSize,
								 fileAttributes,
								 shareAccess,
								 createDisposition,
								 createOptions,
								 eaBuffer,
								 eaLength );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
				   ("GetVolumeInformation: PrimarySession = %p ZwCreateFile volumeHandle =%p, createStatus = %X, ioStatusBlock = %X\n",
					PrimarySession, volumeHandle, createStatus, ioStatusBlock.Information) );

	if (!(createStatus == STATUS_SUCCESS)) {

		return STATUS_UNSUCCESSFUL;
	
	} else {

		ASSERT( ioStatusBlock.Information == FILE_OPENED );
	}

	RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

	fsControlStatus = ZwFsControlFile( volumeHandle,
									   NULL,
									   NULL,
									   NULL,
									   &ioStatusBlock,
									   FSCTL_GET_NTFS_VOLUME_DATA,
									   NULL,
									   0,
									   &ntfsVolumeDataBuffer,
									   sizeof(ntfsVolumeDataBuffer) );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
				   ("GetFileRecordSegmentHeader: FSCTL_GET_NTFS_VOLUME_DATA: volumeHandle %p, fsControlStatus = %x, ioStatusBlock.Status = %x, ioStatusBlock.Information = %d\n",
					volumeHandle, fsControlStatus, ioStatusBlock.Status, ioStatusBlock.Information) );
		
	if (NT_SUCCESS(fsControlStatus)) {

		ASSERT( fsControlStatus == STATUS_SUCCESS );	
		ASSERT( fsControlStatus == ioStatusBlock.Status );
	}

	if (fsControlStatus == STATUS_BUFFER_OVERFLOW)
		ASSERT( ioStatusBlock.Information == sizeof(ntfsVolumeDataBuffer) );
		
	if (!(fsControlStatus == STATUS_SUCCESS || fsControlStatus == STATUS_BUFFER_OVERFLOW)) {

		ioStatusBlock.Information = 0;
		ASSERT(ioStatusBlock.Information == 0);
	}	

	if (!NT_SUCCESS(fsControlStatus)) {

		PrimarySession->Thread.BytesPerFileRecordSegment	= 0;
		PrimarySession->Thread.BytesPerSector				= 0;
		PrimarySession->Thread.BytesPerCluster				= 0;
		
		ZwClose(volumeHandle);
		return STATUS_SUCCESS;
	}
	
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
				   ("ntfsVolumeDataBuffer->VolumeSerialNumber.QuadPart = %I64u\n", 
					ntfsVolumeDataBuffer.VolumeSerialNumber.QuadPart) );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
				   ("ntfsVolumeDataBuffer->NumberSectors.QuadPart = %I64u\n", 
					ntfsVolumeDataBuffer.NumberSectors.QuadPart) );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
				   ("ntfsVolumeDataBuffer->TotalClusters.QuadPart = %I64u\n", 
					ntfsVolumeDataBuffer.TotalClusters.QuadPart) );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
				   ("ntfsVolumeDataBuffer->FreeClusters.QuadPart = %I64u\n", 
					ntfsVolumeDataBuffer.FreeClusters.QuadPart) );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
				   ("ntfsVolumeDataBuffer->TotalReserved.QuadPart = %I64u\n", 
					ntfsVolumeDataBuffer.TotalReserved.QuadPart) );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
				   ("ntfsVolumeDataBuffer->BytesPerSector = %u\n", 
					ntfsVolumeDataBuffer.BytesPerSector) );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
				   ("ntfsVolumeDataBuffer->BytesPerCluster = %u\n", 
					ntfsVolumeDataBuffer.BytesPerCluster) );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
				   ("ntfsVolumeDataBuffer->BytesPerFileRecordSegment = %u\n", 
					ntfsVolumeDataBuffer.BytesPerFileRecordSegment) );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
				   ("ntfsVolumeDataBuffer->ClustersPerFileRecordSegment = %u\n", 
					ntfsVolumeDataBuffer.ClustersPerFileRecordSegment) );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
				   ("ntfsVolumeDataBuffer->MftValidDataLength.QuadPart = %I64u\n", 
					ntfsVolumeDataBuffer.MftValidDataLength.QuadPart) );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
				   ("ntfsVolumeDataBuffer->MftStartLcn.QuadPart = %I64u\n", 
					ntfsVolumeDataBuffer.MftStartLcn.QuadPart) );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
				   ("ntfsVolumeDataBuffer->MftZoneStart.QuadPart = %I64u\n", 
					ntfsVolumeDataBuffer.MftZoneStart.QuadPart) );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
				   ("ntfsVolumeDataBuffer->MftZoneEnd.QuadPart = %I64u\n", 
					ntfsVolumeDataBuffer.MftZoneEnd.QuadPart) );

	PrimarySession->Thread.BytesPerFileRecordSegment	= ntfsVolumeDataBuffer.BytesPerFileRecordSegment;
	PrimarySession->Thread.BytesPerSector				= ntfsVolumeDataBuffer.BytesPerSector;
	PrimarySession->Thread.BytesPerCluster				= ntfsVolumeDataBuffer.BytesPerCluster;

	ZwClose( volumeHandle );
	
	return STATUS_SUCCESS;
}


_U32
CaculateReplyDataLength (
	IN 	PPRIMARY_SESSION			PrimarySession,
	IN PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader
	)
{
	_U32 returnSize;

	UNREFERENCED_PARAMETER( PrimarySession );

	switch(NdfsWinxpRequestHeader->IrpMajorFunction) {

	case IRP_MJ_CREATE: { //0x00
	
#if 0
		if (NdfsWinxpRequestHeader->IrpMajorFunction == IRP_MJ_CREATE && 
		    PrimarySession->NetdiskPartition->FileSystemType == LFS_FILE_SYSTEM_NTFS) {

			returnSize = PrimarySession->Thread.BytesPerFileRecordSegment;
		
		} else
#endif
			returnSize = 0;
	
		break;
	}

	case IRP_MJ_CLOSE: { // 0x02

		returnSize = 0;
		break;
	}

	case IRP_MJ_READ: { // 0x03
	
		returnSize = NdfsWinxpRequestHeader->Read.Length;
		break;
	}
		
	case IRP_MJ_WRITE: { // 0x04
	
		returnSize = 0;
		break;
	}
	
	case IRP_MJ_QUERY_INFORMATION: { // 0x05
	
		returnSize = NdfsWinxpRequestHeader->QueryFile.Length;
		break;
	}
	
	case IRP_MJ_SET_INFORMATION: { // 0x06
	
		returnSize = 0;
		break;
	}
	
	case IRP_MJ_FLUSH_BUFFERS: { // 0x09
	
		returnSize = 0;
		break;
	}

	case IRP_MJ_QUERY_VOLUME_INFORMATION: { // 0x0A
	
		returnSize = NdfsWinxpRequestHeader->QueryVolume.Length;
		break;
	}

	case IRP_MJ_SET_VOLUME_INFORMATION:	{ // 0x0B
	
		returnSize = 0;
		break;
	}

	case IRP_MJ_DIRECTORY_CONTROL: { // 0x0C
	
        if (NdfsWinxpRequestHeader->IrpMinorFunction == IRP_MN_QUERY_DIRECTORY) {

			returnSize = NdfsWinxpRequestHeader->QueryDirectory.Length;
		
		} else {
		
			returnSize = 0;
		}

		break;
	}
	
	case IRP_MJ_FILE_SYSTEM_CONTROL: { // 0x0D
	
		returnSize = NdfsWinxpRequestHeader->FileSystemControl.OutputBufferLength;
		break;
	}

	case IRP_MJ_DEVICE_CONTROL: { // 0x0E
	//	case IRP_MJ_INTERNAL_DEVICE_CONTROL:  // 0x0F 
	
		returnSize = NdfsWinxpRequestHeader->DeviceIoControl.OutputBufferLength;
		break;
	}
	
	case IRP_MJ_LOCK_CONTROL: { // 0x11
	
		returnSize = 0;
		break;
	}

	case IRP_MJ_CLEANUP: { // 0x12
	
		returnSize = 0;
		break;
	}

	case IRP_MJ_QUERY_SECURITY: {
	
		returnSize = NdfsWinxpRequestHeader->QuerySecurity.Length;
		break;
	}

	case IRP_MJ_SET_SECURITY: {
	
		returnSize = 0;
		break;
	}

	default:

		returnSize = 0;
		break;
	}

	return returnSize;
}
