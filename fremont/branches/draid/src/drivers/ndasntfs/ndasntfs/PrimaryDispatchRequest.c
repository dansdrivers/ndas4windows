#include "NtfsProc.h"

#if __NDAS_NTFS_PRIMARY__


#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('PftN')

#define Dbg                              (DEBUG_TRACE_PRIMARY)
#define Dbg2                             (DEBUG_INFO_PRIMARY)



static VOID
DispatchWinXpRequestWorker(
	IN  PPRIMARY_SESSION	PrimarySession,
	IN  _U16				Mid
    );

static _U32
CaculateReplyDataLength(
	IN 	PPRIMARY_SESSION			PrimarySession,
	IN PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader
	);



static VOID
DispatchWinXpRequestWorker0(
	IN  PPRIMARY_SESSION	PrimarySession
    )
{
	DispatchWinXpRequestWorker( PrimarySession, 0 );
}


static VOID
DispatchWinXpRequestWorker1(
	IN  PPRIMARY_SESSION	PrimarySession
    )
{
	DispatchWinXpRequestWorker( PrimarySession, 1 );
}


static VOID
DispatchWinXpRequestWorker2(
	IN  PPRIMARY_SESSION	PrimarySession
    )
{
	DispatchWinXpRequestWorker( PrimarySession, 2 );
}


static VOID
DispatchWinXpRequestWorker3(
	IN  PPRIMARY_SESSION	PrimarySession
    )
{
	DispatchWinXpRequestWorker(PrimarySession, 3);
}


NTSTATUS
DispatchRequest(
	IN PPRIMARY_SESSION	PrimarySession
	)
{
	NTSTATUS				status;
	IN PNDFS_REQUEST_HEADER	ndfsRequestHeader;


	ASSERT( PrimarySession->Thread.NdfsRequestHeader.Mid < PrimarySession->SessionContext.SessionSlotCount );
	ASSERT( PrimarySession->Thread.SessionSlot[PrimarySession->Thread.NdfsRequestHeader.Mid].State == SLOT_WAIT );
	ASSERT( PrimarySession->Thread.TdiReceiveContext.Result == sizeof(NDFS_REQUEST_HEADER) );

	RtlCopyMemory( PrimarySession->Thread.SessionSlot[PrimarySession->Thread.NdfsRequestHeader.Mid].RequestMessageBuffer,
				   &PrimarySession->Thread.NdfsRequestHeader,
				   sizeof(NDFS_REQUEST_HEADER) );

	ndfsRequestHeader = (PNDFS_REQUEST_HEADER)PrimarySession->Thread.SessionSlot[PrimarySession->Thread.NdfsRequestHeader.Mid].RequestMessageBuffer;
   
    DebugTrace( 0, Dbg,
				("DispatchRequest: ndfsRequestHeader->Command = %d\n", 
				ndfsRequestHeader->Command) );

	switch (ndfsRequestHeader->Command) {

	case NDFS_COMMAND_LOGOFF: {

		PNDFS_REQUEST_LOGOFF	ndfsRequestLogoff;
		PNDFS_REPLY_HEADER		ndfsReplyHeader;
		PNDFS_REPLY_LOGOFF		ndfsReplyLogoff;
		

		if (PrimarySession->Thread.SessionState != SESSION_TREE_CONNECT) {

			ASSERT(NDNTFS_BUG);
			status = STATUS_UNSUCCESSFUL;
			break;
		}

		if (!(ndfsRequestHeader->Uid == PrimarySession->SessionContext.Uid && ndfsRequestHeader->Tid == PrimarySession->SessionContext.Tid)) {

			ASSERT(NDNTFS_BUG);
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

			ASSERT(NDNTFS_BUG);
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

		if(ndfsRequestLogoff->SessionKey != PrimarySession->SessionContext.SessionKey) {

			ndfsReplyLogoff->Status = NDFS_LOGOFF_UNSUCCESSFUL;
		
		} else {

			ndfsReplyLogoff->Status = NDFS_LOGOFF_SUCCESS;
		}

		status = SendMessage( PrimarySession->ConnectionFileObject,
							  (_U8 *)ndfsReplyHeader,
							  ndfsReplyHeader->MessageSize,
							  NULL );

		if (status != STATUS_SUCCESS) {

			break;
		}

		PrimarySession->Thread.SessionState = SESSION_CLOSE;
		LpxTdiDisconnect( PrimarySession->ConnectionFileObject, 0 );
		SetFlag( PrimarySession->Thread.Flags, PRIMARY_SESSION_THREAD_FLAG_DISCONNECTED );

		status = STATUS_SUCCESS;

		break;
	}

	case NDFS_COMMAND_EXECUTE: {

		_U16	slotIndex;

		if(PrimarySession->Thread.SessionState != SESSION_TREE_CONNECT) {

			ASSERT(NDNTFS_BUG);
			status = STATUS_UNSUCCESSFUL;
			break;
		}

		if (!(ndfsRequestHeader->Uid == PrimarySession->SessionContext.Uid && ndfsRequestHeader->Tid == PrimarySession->SessionContext.Tid)) {

			ASSERT(NDNTFS_BUG);
			status = STATUS_UNSUCCESSFUL;
			break;
		}

		slotIndex = ndfsRequestHeader->Mid;

		PrimarySession->Thread.SessionSlot[slotIndex].RequestMessageBufferLength = sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER) + DEFAULT_MAX_DATA_SIZE;
		
		RtlZeroMemory( &PrimarySession->Thread.SessionSlot[slotIndex].RequestMessageBuffer[sizeof(NDFS_REQUEST_HEADER)], 
					   PrimarySession->Thread.SessionSlot[slotIndex].RequestMessageBufferLength - sizeof(NDFS_REQUEST_HEADER) );

		PrimarySession->Thread.SessionSlot[slotIndex].ReplyMessageBufferLength = sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER) + DEFAULT_MAX_DATA_SIZE;
		
		RtlZeroMemory(PrimarySession->Thread.SessionSlot[slotIndex].ReplyMessageBuffer, PrimarySession->Thread.SessionSlot[slotIndex].ReplyMessageBufferLength);

		ASSERT( ndfsRequestHeader->MessageSize >= sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER) );
		status = ReceiveNtfsWinxpMessage( PrimarySession, slotIndex );

		if(status != STATUS_SUCCESS)
			break;

		if(PrimarySession->Thread.SessionSlot[slotIndex].State != SLOT_WAIT) {

			break;
		}
	
		PrimarySession->Thread.SessionSlot[slotIndex].State = SLOT_EXECUTING;
		PrimarySession->Thread.IdleSlotCount --;

		if(slotIndex == 0)
			ExInitializeWorkItem( &PrimarySession->Thread.SessionSlot[slotIndex].WorkQueueItem,
								  DispatchWinXpRequestWorker0,
								  PrimarySession );
		if(slotIndex == 1)
			ExInitializeWorkItem( &PrimarySession->Thread.SessionSlot[slotIndex].WorkQueueItem,
								  DispatchWinXpRequestWorker1,
								  PrimarySession );
		if(slotIndex == 2)
			ExInitializeWorkItem( &PrimarySession->Thread.SessionSlot[slotIndex].WorkQueueItem,
								  DispatchWinXpRequestWorker2,
								  PrimarySession );
		if(slotIndex == 3)
			ExInitializeWorkItem( &PrimarySession->Thread.SessionSlot[slotIndex].WorkQueueItem,
								  DispatchWinXpRequestWorker3,
								  PrimarySession );

		ExQueueWorkItem( &PrimarySession->Thread.SessionSlot[slotIndex].WorkQueueItem, DelayedWorkQueue );	
		status = STATUS_PENDING;

		break;
	}

	default:

		ASSERT(NDNTFS_LPX_BUG);
		status = STATUS_UNSUCCESSFUL;
		
		break;
	}

	return status;
}


static VOID
DispatchWinXpRequestWorker(
	IN  PPRIMARY_SESSION	PrimarySession,
	IN  _U16				Mid
    )
{
	PNDFS_REQUEST_HEADER		ndfsRequestHeader = (PNDFS_REQUEST_HEADER)PrimarySession->Thread.SessionSlot[Mid].RequestMessageBuffer;
	PNDFS_REPLY_HEADER			ndfsReplyHeader = (PNDFS_REPLY_HEADER)PrimarySession->Thread.SessionSlot[Mid].ReplyMessageBuffer; 
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader = PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpRequestHeader;
	
	_U32						replyDataSize;

	
	ASSERT(Mid == ndfsRequestHeader->Mid);
	
    DebugTrace( 0, Dbg, ("DispatchWinXpRequestWorker: entered PrimarySession = %p, ndfsRequestHeader->Command = %d\n", 
						  PrimarySession, ndfsRequestHeader->Command));

	ASSERT( PrimarySession->Thread.SessionSlot[Mid].State == SLOT_EXECUTING );


	replyDataSize = CaculateReplyDataLength(PrimarySession, ndfsWinxpRequestHeader);

	if (replyDataSize <= (ULONG)(PrimarySession->SessionContext.SecondaryMaxDataSize || 
	    sizeof(PrimarySession->Thread.SessionSlot[Mid].ReplyMessageBuffer) - sizeof(NDFS_REPLY_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER))) {

		if(ndfsRequestHeader->MessageSecurity == 1) {

			if(ndfsWinxpRequestHeader->IrpMajorFunction == IRP_MJ_READ && PrimarySession->SessionContext.RwDataSecurity == 0)
				PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)(ndfsReplyHeader+1);
			else
				PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)PrimarySession->Thread.SessionSlot[Mid].CryptWinxpMessageBuffer;
		}
		else
			PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader = (PNDFS_WINXP_REPLY_HEADER)(ndfsReplyHeader+1);
	
	} else {

		PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePoolLength = ADD_ALIGN8(sizeof(NDFS_WINXP_REPLY_HEADER) + replyDataSize);
		PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool = ExAllocatePoolWithTag(
																	NonPagedPool,
																	PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePoolLength,
																	PRIMARY_SESSION_BUFFERE_TAG
																	);		
		ASSERT(PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool);
		if(PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool == NULL) {
		    DebugTrace( 0, Dbg, ("failed to allocate ExtendWinxpReplyMessagePool\n"));
			goto fail_replypoolalloc;
		}
		PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader 
			= (PNDFS_WINXP_REPLY_HEADER)PrimarySession->Thread.SessionSlot[Mid].ExtendWinxpReplyMessagePool;
	}
	
    DebugTrace( 0, Dbg,
					("DispatchWinXpRequestWorker: PrimarySession = %p, ndfsRequestHeader->Command = %d\n", 
					PrimarySession, ndfsRequestHeader->Command));

	PrimarySession->Thread.SessionSlot[Mid].status = DispatchWinXpRequest( PrimarySession, 
																   ndfsWinxpRequestHeader,
																   PrimarySession->Thread.SessionSlot[Mid].NdfsWinxpReplyHeader,
																   ndfsRequestHeader->MessageSize - sizeof(NDFS_REQUEST_HEADER) - sizeof(NDFS_WINXP_REQUEST_HEADER),
																   &PrimarySession->Thread.SessionSlot[Mid].ReplyDataSize );

    DebugTrace( 0, Dbg,	("DispatchWinXpRequestWorker: Return PrimarySession = %p, ndfsRequestHeader->Command = %d\n", 
						  PrimarySession, ndfsRequestHeader->Command));

fail_replypoolalloc:
	PrimarySession->Thread.SessionSlot[Mid].State = SLOT_FINISH;

	KeSetEvent( &PrimarySession->Thread.WorkCompletionEvent, IO_NO_INCREMENT, FALSE );

	return;
}


static _U32
CaculateReplyDataLength(
	IN 	PPRIMARY_SESSION			PrimarySession,
	IN PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader
	)
{
	_U32 returnSize;

	UNREFERENCED_PARAMETER( PrimarySession );

	switch(NdfsWinxpRequestHeader->IrpMajorFunction)
	{
    case IRP_MJ_CREATE: //0x00
	{
//		if(NdfsWinxpRequestHeader->IrpMajorFunction == IRP_MJ_CREATE
//			&& PrimarySession->NetdiskPartition->FileSystemType == NDNTFS_FILE_SYSTEM_NTFS)
//		{
//			returnSize = PrimarySession->BytesPerFileRecordSegment;
//		}
//		else
			returnSize = 0;
	
		break;
	}

	case IRP_MJ_CLOSE: // 0x02
	{
		returnSize = 0;
		break;
	}

    case IRP_MJ_READ: // 0x03
	{
		returnSize = NdfsWinxpRequestHeader->Read.Length;
		break;
	}
		
    case IRP_MJ_WRITE: // 0x04
	{
		returnSize = 0;
		break;
	}
	
    case IRP_MJ_QUERY_INFORMATION: // 0x05
	{
		returnSize = NdfsWinxpRequestHeader->QueryFile.Length;
		break;
	}
	
    case IRP_MJ_SET_INFORMATION:  // 0x06
	{
		returnSize = 0;
		break;
	}
	
     case IRP_MJ_FLUSH_BUFFERS: // 0x09
	{
		returnSize = 0;
		break;
	}

	case IRP_MJ_QUERY_VOLUME_INFORMATION: // 0x0A
	{
		returnSize = NdfsWinxpRequestHeader->QueryVolume.Length;
		break;
	}

	case IRP_MJ_SET_VOLUME_INFORMATION:	// 0x0B
	{
		returnSize = 0;
		break;
	}

	case IRP_MJ_DIRECTORY_CONTROL: // 0x0C
	{
        if(NdfsWinxpRequestHeader->IrpMinorFunction == IRP_MN_QUERY_DIRECTORY) 
		{
			returnSize = NdfsWinxpRequestHeader->QueryDirectory.Length;
		} else
		{
			returnSize = 0;
		}

		break;
	}
	
	case IRP_MJ_FILE_SYSTEM_CONTROL: // 0x0D
	{
		returnSize = NdfsWinxpRequestHeader->FileSystemControl.OutputBufferLength;
		break;
	}

    case IRP_MJ_DEVICE_CONTROL: // 0x0E
	//	case IRP_MJ_INTERNAL_DEVICE_CONTROL:  // 0x0F 
	{
		returnSize = NdfsWinxpRequestHeader->DeviceIoControl.OutputBufferLength;
		break;
	}
	
	case IRP_MJ_LOCK_CONTROL: // 0x11
	{
		returnSize = 0;
		break;
	}

	case IRP_MJ_CLEANUP: // 0x12
	{
		returnSize = 0;
		break;
	}

    case IRP_MJ_QUERY_SECURITY:
	{
		returnSize = NdfsWinxpRequestHeader->QuerySecurity.Length;
		break;
	}

    case IRP_MJ_SET_SECURITY:
	{
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