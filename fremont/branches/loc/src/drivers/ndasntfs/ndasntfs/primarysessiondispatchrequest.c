#include "NtfsProc.h"

#if __NDAS_NTFS_PRIMARY__


#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('PftN')

#define Dbg                              (DEBUG_TRACE_PRIMARY)
#define Dbg2                             (DEBUG_INFO_PRIMARY)


static VOID
DispatchWinXpRequestWorker (
	IN  PPRIMARY_SESSION	PrimarySession,
	IN  UINT16				Mid
    );

static UINT32
CaculateReplyDataLength (
	IN 	PPRIMARY_SESSION			PrimarySession,
	IN PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader
	);



static VOID
DispatchWinXpRequestWorker0 (
	IN  PPRIMARY_SESSION	PrimarySession
    )
{
	DispatchWinXpRequestWorker( PrimarySession, 0 );
}


static VOID
DispatchWinXpRequestWorker1 (
	IN  PPRIMARY_SESSION	PrimarySession
    )
{
	DispatchWinXpRequestWorker( PrimarySession, 1 );
}


static VOID
DispatchWinXpRequestWorker2 (
	IN  PPRIMARY_SESSION	PrimarySession
    )
{
	DispatchWinXpRequestWorker( PrimarySession, 2 );
}


static VOID
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


	ASSERT( NTOHS(PrimarySession->Thread.NdfsRequestHeader.Mid2) < PrimarySession->SessionContext.SessionSlotCount );
	ASSERT( PrimarySession->Thread.SessionSlot[NTOHS(PrimarySession->Thread.NdfsRequestHeader.Mid2)].State == SLOT_WAIT );
	ASSERT( PrimarySession->Thread.ReceiveOverlapped.Request[0].IoStatusBlock.Information == sizeof(NDFS_REQUEST_HEADER) );

	RtlCopyMemory( PrimarySession->Thread.SessionSlot[NTOHS(PrimarySession->Thread.NdfsRequestHeader.Mid2)].RequestMessageBuffer,
				   &PrimarySession->Thread.NdfsRequestHeader,
				   sizeof(NDFS_REQUEST_HEADER) );

	ndfsRequestHeader = (PNDFS_REQUEST_HEADER)PrimarySession->Thread.SessionSlot[NTOHS(PrimarySession->Thread.NdfsRequestHeader.Mid2)].RequestMessageBuffer;
   
    DebugTrace( 0, Dbg,
				("DispatchRequest: ndfsRequestHeader->Command = %d\n", 
				ndfsRequestHeader->Command) );

	switch (ndfsRequestHeader->Command) {

	case NDFS_COMMAND_LOGOFF: {

		PNDFS_REQUEST_LOGOFF	ndfsRequestLogoff;
		PNDFS_REPLY_HEADER		ndfsReplyHeader;
		PNDFS_REPLY_LOGOFF		ndfsReplyLogoff;
		

		if (PrimarySession->Thread.SessionState != SESSION_TREE_CONNECT) {

			ASSERT(NDASNTFS_BUG);
			status = STATUS_UNSUCCESSFUL;
			break;
		}

		if (!(NTOHS(ndfsRequestHeader->Uid2) == PrimarySession->SessionContext.Uid && NTOHS(ndfsRequestHeader->Tid2) == PrimarySession->SessionContext.Tid)) {

			ASSERT(NDASNTFS_BUG);
			status = STATUS_UNSUCCESSFUL;
			break;
		}
		
		ASSERT( NTOHL(ndfsRequestHeader->MessageSize4) == sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_REQUEST_LOGOFF) );

		ndfsRequestLogoff = (PNDFS_REQUEST_LOGOFF)(ndfsRequestHeader+1);
		
		status = RecvMessage( PrimarySession->ConnectionFileObject,
							 (UINT8 *)ndfsRequestLogoff,
							 sizeof(NDFS_REQUEST_LOGOFF),
							 NULL );
	
		if (status != STATUS_SUCCESS) {

			ASSERT(NDASNTFS_BUG);
			break;
		}

		ndfsReplyHeader = (PNDFS_REPLY_HEADER)(ndfsRequestLogoff+1);

		RtlCopyMemory( ndfsReplyHeader->Protocol, NDFS_PROTOCOL, sizeof(ndfsReplyHeader->Protocol) );
		ndfsReplyHeader->Status		= NDFS_SUCCESS;
		ndfsReplyHeader->Flags	    = 0;
		ndfsReplyHeader->Uid2		= HTONS(PrimarySession->SessionContext.Uid);
		ndfsReplyHeader->Tid2		= 0;
		ndfsReplyHeader->Mid2		= 0;
		ndfsReplyHeader->MessageSize4 = HTONL((UINT32)(sizeof(NDFS_REPLY_HEADER)+sizeof(NDFS_REPLY_LOGOFF)));

		ndfsReplyLogoff = (PNDFS_REPLY_LOGOFF)(ndfsReplyHeader+1);

		if (NTOHL(ndfsRequestLogoff->SessionKey4) != PrimarySession->SessionContext.SessionKey) {

			ndfsReplyLogoff->Status = NDFS_LOGOFF_UNSUCCESSFUL;
		
		} else {

			ndfsReplyLogoff->Status = NDFS_LOGOFF_SUCCESS;
		}

		status = SendMessage( PrimarySession->ConnectionFileObject,
							  (UINT8 *)ndfsReplyHeader,
							  NTOHL(ndfsReplyHeader->MessageSize4),
							  NULL );

		if (status != STATUS_SUCCESS) {

			break;
		}

		PrimarySession->Thread.SessionState = SESSION_CLOSED;

		status = STATUS_SUCCESS;

		break;
	}

	case NDFS_COMMAND_EXECUTE: {

		UINT16	mid;

		if(PrimarySession->SessionContext.NdfsMinorVersion == NDFS_PROTOCOL_MINOR_0) {

			if (PrimarySession->Thread.SessionState != SESSION_TREE_CONNECT) {

				ASSERT( NDASNTFS_BUG );
				status = STATUS_UNSUCCESSFUL;
				break;
			}
		}

		if (!(NTOHS(ndfsRequestHeader->Uid2) == PrimarySession->SessionContext.Uid && 
			NTOHS(ndfsRequestHeader->Tid2) == PrimarySession->SessionContext.Tid)) {

			ASSERT( NDASNTFS_BUG );
			status = STATUS_UNSUCCESSFUL;

			break;
		}

		mid = NTOHS(ndfsRequestHeader->Mid2);

		PrimarySession->Thread.SessionSlot[mid].RequestMessageBufferLength = sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER) + DEFAULT_NDAS_MAX_DATA_SIZE;
		RtlZeroMemory( &PrimarySession->Thread.SessionSlot[mid].RequestMessageBuffer[sizeof(NDFS_REQUEST_HEADER)], 
					   PrimarySession->Thread.SessionSlot[mid].RequestMessageBufferLength - sizeof(NDFS_REQUEST_HEADER) );

		PrimarySession->Thread.SessionSlot[mid].ReplyMessageBufferLength = sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER) + DEFAULT_NDAS_MAX_DATA_SIZE;
		RtlZeroMemory( PrimarySession->Thread.SessionSlot[mid].ReplyMessageBuffer, 
					   PrimarySession->Thread.SessionSlot[mid].ReplyMessageBufferLength );

		ASSERT( NTOHL(ndfsRequestHeader->MessageSize4) >= sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER) );
		status = ReceiveNdfsWinxpMessage( PrimarySession, mid );

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

			if (PrimarySession->Thread.SessionSlot[mid].Status == STATUS_PENDING) {

				NDAS_BUGON( FALSE );
			}
			
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

		NDAS_BUGON( FALSE );
		status = STATUS_UNSUCCESSFUL;
		
		break;
	}

	return status;
}


static VOID
DispatchWinXpRequestWorker (
	IN  PPRIMARY_SESSION	PrimarySession,
	IN  UINT16				Mid
    )
{
	PNDFS_REQUEST_HEADER		ndfsRequestHeader = (PNDFS_REQUEST_HEADER)PrimarySession->Thread.SessionSlot[Mid].RequestMessageBuffer;
	PNDFS_REPLY_HEADER			ndfsReplyHeader = (PNDFS_REPLY_HEADER)PrimarySession->Thread.SessionSlot[Mid].ReplyMessageBuffer; 
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader = PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpRequestHeader;
	
	UINT32						replyDataSize;

	
	ASSERT(Mid == NTOHS(ndfsRequestHeader->Mid2));
	
    DebugTrace( 0, Dbg, ("DispatchWinXpRequestWorker: entered PrimarySession = %p, ndfsRequestHeader->Command = %d\n", 
						  PrimarySession, ndfsRequestHeader->Command));

	ASSERT( PrimarySession->Thread.SessionSlot[Mid].State == SLOT_EXECUTING );


	replyDataSize = CaculateReplyDataLength(PrimarySession, ndfsWinxpRequestHeader);

	if (replyDataSize <= (ULONG)(PrimarySession->SessionContext.SecondaryMaxDataSize || 
	    sizeof(PrimarySession->Thread.SessionSlot[Mid].ReplyMessageBuffer) - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER))) {

		if (ndfsRequestHeader->MessageSecurity == 1) {

			if (ndfsWinxpRequestHeader->IrpMajorFunction == IRP_MJ_READ && PrimarySession->SessionContext.RwDataSecurity == 0)
				PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)(ndfsReplyHeader+1);
			else
				PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)PrimarySession->Thread.SessionSlot[Mid].CryptWinxpMessageBuffer;
		}
		else
			PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)(ndfsReplyHeader+1);
	
	} else {

		PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePoolLength = 
			ADD_ALIGN8(sizeof(NDFS_WINXP_REPLY_HEADER) + replyDataSize);
		PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool = 
			ExAllocatePoolWithTag( NonPagedPool,
								   PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePoolLength,
								   PRIMARY_SESSION_BUFFERE_TAG );

		ASSERT( PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool );

		if (PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool == NULL) {
	
			DebugTrace( 0, Dbg, ("failed to allocate ExtendWinxpReplyMessagePool\n"));
			goto fail_replypoolalloc;
		}

		PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader 
			= (PNDFS_WINXP_REPLY_HEADER)PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool;
	}
	
    DebugTrace( 0, Dbg,
				 ("DispatchWinXpRequestWorker: PrimarySession = %p, ndfsRequestHeader->Command = %d\n", 
				  PrimarySession, ndfsRequestHeader->Command) );

	PrimarySession->Thread.SessionSlot[Mid].Status = 
		DispatchWinXpRequest( PrimarySession, 
							  ndfsWinxpRequestHeader,
							  PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader,
							  NTOHL(ndfsRequestHeader->MessageSize4) - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER),
							  &PrimarySession->Thread.SessionSlot[Mid].ReplyDataSize );

    DebugTrace( 0, Dbg, ("DispatchWinXpRequestWorker: Return PrimarySession = %p, ndfsRequestHeader->Command = %d\n", 
						  PrimarySession, ndfsRequestHeader->Command) );

fail_replypoolalloc:
	PrimarySession->Thread.SessionSlot[Mid].State = SLOT_FINISH;

	KeSetEvent( &PrimarySession->Thread.WorkCompletionEvent, IO_NO_INCREMENT, FALSE );

	return;
}


static UINT32
CaculateReplyDataLength (
	IN 	PPRIMARY_SESSION			PrimarySession,
	IN PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader
	)
{
	UINT32 returnSize;

	UNREFERENCED_PARAMETER( PrimarySession );

	switch (NdfsWinxpRequestHeader->IrpMajorFunction) {

    case IRP_MJ_CREATE: {

		returnSize = 0;
		break;
	}

	case IRP_MJ_CLOSE: {

		returnSize = 0;
		break;
	}

    case IRP_MJ_READ: {

		returnSize = NdfsWinxpRequestHeader->Read.Length;
		break;
	}
		
    case IRP_MJ_WRITE: {

		returnSize = 0;
		break;
	}
	
    case IRP_MJ_QUERY_INFORMATION: {

		returnSize = NdfsWinxpRequestHeader->QueryFile.Length;
		break;
	}
	
    case IRP_MJ_SET_INFORMATION: {

		returnSize = 0;
		break;
	}
	
	case IRP_MJ_FLUSH_BUFFERS: {

		returnSize = 0;
		break;
	}

	case IRP_MJ_QUERY_VOLUME_INFORMATION: {

		returnSize = NdfsWinxpRequestHeader->QueryVolume.Length;
		break;
	}

	case IRP_MJ_SET_VOLUME_INFORMATION:	{

		returnSize = 0;
		break;
	}

	case IRP_MJ_DIRECTORY_CONTROL: {

        if (NdfsWinxpRequestHeader->IrpMinorFunction == IRP_MN_QUERY_DIRECTORY) {

			returnSize = NdfsWinxpRequestHeader->QueryDirectory.Length;
		
		} else {

			returnSize = 0;
		}

		break;
	}
	
	case IRP_MJ_FILE_SYSTEM_CONTROL: {

		returnSize = NdfsWinxpRequestHeader->FileSystemControl.OutputBufferLength;
		break;
	}

    case IRP_MJ_DEVICE_CONTROL: {

		returnSize = NdfsWinxpRequestHeader->DeviceIoControl.OutputBufferLength;
		break;
	}
	
	case IRP_MJ_LOCK_CONTROL: {

		returnSize = 0;
		break;
	}

	case IRP_MJ_CLEANUP: {

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

#endif