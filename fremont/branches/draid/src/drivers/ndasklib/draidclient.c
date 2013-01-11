#include <ntddk.h>
#include "LSKLib.h"
#include "KDebug.h"
#include "LSProto.h"
#include "LSLurn.h"
#include "draid.h"
#include "lslurnassoc.h"
#include "Scrc32.h"
#include "cipher.h"
#include "lslurnide.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "DRaidClient"

#define SAFE_FREE_POOL(MEM_POOL_PTR) \
	if(MEM_POOL_PTR) { \
		ExFreePool(MEM_POOL_PTR); \
		MEM_POOL_PTR = NULL; \
	}

#define SAFE_FREE_POOL_WITH_TAG(MEM_POOL_PTR, POOL_TAG) \
	if(MEM_POOL_PTR) { \
		ExFreePoolWithTag(MEM_POOL_PTR, POOL_TAG); \
		MEM_POOL_PTR = NULL; \
	}


//
// All event that client should wait
//		1. stop event - set RequestToTerminate and waken up through ClientThreadEvent, 
//		2. Reqeust from Ccbcompletion to send node status update - check NodeChanged's flag
//		3. Notification from arbiter through network or local event queue
//		4. Ccb - Until lock region implemented, Ccb will not be passed to draid client.
//		5. send completion - Currently we will not wait for simplicity. Send in synchronized mode with short timeout value.
//		6. reply packet - Currently we will not wait for simplicity. Wait for reply.


#define LOCK_STATUS_NONE 			(1<<0) // Lock is allocated. But not used yet
#define LOCK_STATUS_PENDING		(1<<3) // Sent message but lock is not available right now. waiting for grant 
#define LOCK_STATUS_GRANTED		(1<<4) // Lock is granted. 

//
// Guarded by Client's spinlock
//
typedef struct _DRAID_CLIENT_LOCK {

	// Lock info
	UCHAR 		LockType;
	UCHAR 	LockMode;

	UINT64 LockAddress;	// in sector for blo
	UINT32 LockLength;

	UINT64 LockId; // ID reccieved from arbiter.

	LIST_ENTRY	Link;			// Link to ClientInfo.LockList. List of all locks including lock to acquire
	
	UINT32	LockStatus; // LOCK_STATUS_NONE, LOCK_STATUS_*

	LARGE_INTEGER LastAccessedTime;
	LONG		InUseCount;	// Number of usage used by IO routine. Guarded by client's spinlock

	BOOLEAN		Contented; // Lock is contented by other lock. It is preferred to release this lock as fast as possible.
	BOOLEAN		YieldRequested; // Set to TRUE when arbiter sent req-to-yield message.
} DRAID_CLIENT_LOCK, *PDRAID_CLIENT_LOCK;


//
// Forward declarations.
//
VOID
DraidClientFreeLock(
	PDRAID_CLIENT_LOCK Lock
);

PDRAID_CLIENT_LOCK
DraidClientAllocLock(
	UINT64		LockId,
	UCHAR 		LockType,
	UCHAR 		LockMode,
	UINT64		Addr,
	UINT32		Length
);

void
DraidMonitorThreadProc(
	IN	PVOID	Context
);

NTSTATUS 
DraidClientResetRemoteArbiterContext(
	PDRAID_CLIENT_INFO		pClientInfo
);

NTSTATUS
DraidClientRecvHeaderAsync(
	PDRAID_CLIENT_INFO		Client,
	PDRAID_CLIENT_CONNECTION Connection
);

UCHAR DraidClientLurnStatusToNodeFlag(UINT32 LurnStatus)
{
	UCHAR Flags;
	switch(LurnStatus) {
	case LURN_STATUS_RUNNING:
	case LURN_STATUS_STALL:
		Flags = DRIX_NODE_FLAG_RUNNING;
		break;
	case LURN_STATUS_STOP_PENDING:
	case LURN_STATUS_STOP:
	case LURN_STATUS_DESTROYING:
		Flags = DRIX_NODE_FLAG_STOP;
		break;
	case LURN_STATUS_INIT:
	default:
		Flags = DRIX_NODE_FLAG_UNKNOWN;
		break;
	}
	return Flags;
}



//
// Handle reply from arbiter for request
//
NTSTATUS
DraidClientHandleReplyForRequest(
	PDRAID_CLIENT_INFO Client, 
	PDRIX_HEADER		ReplyMsg
	) 
{
	PDRIX_MSG_CONTEXT RequestMsgLink;
	PDRIX_HEADER RequestMsg;
	KIRQL	oldIrql;
	NTSTATUS status = STATUS_SUCCESS;
	PLIST_ENTRY listEntry;
	BOOLEAN 	MatchFound;
	ULONG	i;

	// Check reply message sanity
	if (!(ReplyMsg->Signature == HTONL(DRIX_SIGNATURE) &&
		ReplyMsg->ReplyFlag
	)) {
		KDPrintM(DBG_LURN_ERROR, ("Invalid reply packet\n"));
		ASSERT(FALSE);
		status = STATUS_UNSUCCESSFUL;
		goto out;
	}
	
	//
	// Find matching request from pending queue
	//
	ACQUIRE_SPIN_LOCK(&Client->SpinLock, &oldIrql);
	MatchFound = FALSE;
	for (listEntry = Client->PendingRequestList.Flink;
		listEntry != &Client->PendingRequestList;
		listEntry = listEntry->Flink) 
	{
		RequestMsgLink = CONTAINING_RECORD (listEntry, DRIX_MSG_CONTEXT, Link);
		if (RequestMsgLink->Message->Sequence == ReplyMsg->Sequence) {
			RemoveEntryList(&RequestMsgLink->Link);
			MatchFound = TRUE;
			break;
		}
	}
	RELEASE_SPIN_LOCK(&Client->SpinLock, oldIrql);				
	if (MatchFound == FALSE) {
		KDPrintM(DBG_LURN_ERROR, ("Invalid reply or timed out message. Handle any way.\n"));
		RequestMsgLink = NULL;
		RequestMsg = NULL;
	} else {
		RequestMsg = RequestMsgLink->Message;
		ASSERT(RequestMsg->Command == ReplyMsg->Command);
	}
		
	KDPrintM(DBG_LURN_INFO, ("DRAID request result=%x\n", ReplyMsg->Result));

	if (DRIX_CMD_NODE_CHANGE == ReplyMsg->Command) {
		if (ReplyMsg->Result == DRIX_RESULT_REQUIRE_SYNC) {
			ACQUIRE_SPIN_LOCK(&Client->SpinLock, &oldIrql);
			Client->InTransition = TRUE;
			RELEASE_SPIN_LOCK(&Client->SpinLock, oldIrql);				
		} else if (ReplyMsg->Result == DRIX_RESULT_NO_CHANGE) {
			// This node's change does not effect RAID status.	
		} else {
			// Any other result is bug..
			ASSERT(FALSE);
		}
		ACQUIRE_SPIN_LOCK(&Client->SpinLock, &oldIrql);
		for(i=0;i<Client->Lurn->LurnChildrenCnt;i++) {
			Client->NodeChanged[i] &= ~DRAID_NODE_CHANGE_FLAG_UPDATING;
		}
		RELEASE_SPIN_LOCK(&Client->SpinLock, oldIrql);
			
	} else if (DRIX_CMD_REGISTER == ReplyMsg->Command) {
		// to do: 
		// If RAID is in initialization state, reload RMD and retry
		// 	or unmount.
	} else if (DRIX_CMD_ACQUIRE_LOCK == ReplyMsg->Command) {
		PDRIX_ACQUIRE_LOCK_REPLY AcqReply = (PDRIX_ACQUIRE_LOCK_REPLY) ReplyMsg;
		
		if (ReplyMsg->Length == HTONS((UINT16)sizeof(DRIX_ACQUIRE_LOCK_REPLY)) &&
			(ReplyMsg->Result == DRIX_RESULT_GRANTED || ReplyMsg->Result == DRIX_RESULT_PENDING)) {
			PDRAID_CLIENT_LOCK Lock;
			BOOLEAN LockFound;
			// Find lock from lock list
			LockFound = FALSE;
			ACQUIRE_SPIN_LOCK(&Client->SpinLock, &oldIrql);
			for (listEntry = Client->LockList.Flink;
				listEntry != &Client->LockList;
				listEntry = listEntry->Flink) 
			{
				Lock = CONTAINING_RECORD (listEntry, DRAID_CLIENT_LOCK, Link);
				if (Lock->LockId == NTOHLL(AcqReply->LockId)) {
					KDPrintM(DBG_LURN_INFO, ("lock id = %I64x, range= %I64x:%x already exist\n", Lock->LockId,
						Lock->LockAddress, Lock->LockLength));
					LockFound = TRUE;
					if (Lock->LockStatus == LOCK_STATUS_PENDING && ReplyMsg->Result == DRIX_RESULT_GRANTED) {
						KDPrintM(DBG_LURN_INFO, ("Pended lock%I64x is granted\n", Lock->LockId));
						Lock->LockStatus = LOCK_STATUS_GRANTED;
					}
				}
			}
			RELEASE_SPIN_LOCK(&Client->SpinLock, oldIrql);
			if (LockFound == FALSE) {
				Lock = DraidClientAllocLock(
					NTOHLL(AcqReply->LockId), 
					AcqReply->LockType, AcqReply->LockMode, 
					NTOHLL(AcqReply->Addr),
					NTOHL(AcqReply->Length));
				if (Lock==NULL) {
					status = STATUS_INSUFFICIENT_RESOURCES;
					goto out;
				}
				KeQueryTickCount(&Lock->LastAccessedTime);	
				if (ReplyMsg->Result == DRIX_RESULT_GRANTED) {
					Lock->LockStatus = LOCK_STATUS_GRANTED;
					KDPrintM(DBG_LURN_INFO, ("Reply - lock is granted lock id = %I64x, range= %I64x:%x\n", Lock->LockId, 
						Lock->LockAddress, Lock->LockLength));
				} else {
					Lock->LockStatus = LOCK_STATUS_PENDING;
					Lock->Contented = TRUE;
					KDPrintM(DBG_LURN_INFO, ("Reply - lock is pended. lock id = %I64x, range= %I64x:%x\n", Lock->LockId, 
						Lock->LockAddress, Lock->LockLength));
				}
				ACQUIRE_SPIN_LOCK(&Client->SpinLock, &oldIrql); 
				InsertTailList(&Client->LockList, &Lock->Link);
				RELEASE_SPIN_LOCK(&Client->SpinLock, oldIrql);
			}
		} else {
			KDPrintM(DBG_LURN_INFO, ("Invalid ackquire_lock_reply\n"));
		
			ASSERT(FALSE);
			status = STATUS_UNSUCCESSFUL;
		}
	} else if (DRIX_CMD_RELEASE_LOCK == ReplyMsg->Command) {
		//
		// Nothing to do. Lock is already removed from client's list. 
		//
	}

	// Call callback
	if (RequestMsgLink && RequestMsgLink->CallbackFunc)
		RequestMsgLink->CallbackFunc(Client, ReplyMsg, RequestMsgLink->CallbackParam1);
	
out:
	// Free used resources
	if (RequestMsg)
		ExFreePoolWithTag(RequestMsg, DRAID_CLIENT_REQUEST_MSG_POOL_TAG);	
	if (RequestMsgLink)
		ExFreePoolWithTag(RequestMsgLink, DRAID_MSG_LINK_POOL_TAG);	
	
	return status;
}

//
// Send message to arbiter and wait for reply.
//
NTSTATUS
DraidClientSendRequest(
	PDRAID_CLIENT_INFO Client, 
	UCHAR					Command,
	UINT32					CmdParam1,	// Command dependent.
	UINT64					CmdParam2,
	UINT32					CmdParam3,
	PLARGE_INTEGER			Timeout,
	DRAID_MSG_CALLBACK	Callback,	// Called when replied
	PVOID					CallbackParam1
) 
{
	PDRIX_HEADER RequestMsg;
	UINT32 MsgLength;
	UINT32 i;
	NTSTATUS status = STATUS_SUCCESS;
	UINT32 TotalDiskCount;
	PLURELATION_NODE Lurn = Client->Lurn;
	PDRIX_MSG_CONTEXT MsgEntry;
	KIRQL oldIrql;
	TotalDiskCount = Client->TotalDiskCount;

	//
	// Create request message.
	//
	switch(Command) {
	case DRIX_CMD_NODE_CHANGE:
		MsgLength = SIZE_OF_DRIX_CHANGE_STATUS(TotalDiskCount);
		break;
	case DRIX_CMD_REGISTER:
		MsgLength = sizeof(DRIX_REGISTER);
		break;
	case DRIX_CMD_ACQUIRE_LOCK:
		MsgLength = sizeof(DRIX_ACQUIRE_LOCK);
		break;
	case DRIX_CMD_RELEASE_LOCK:
		MsgLength = sizeof(DRIX_RELEASE_LOCK);
		break;
//	case DRIX_CMD_UNREGISTER:
	default:
		MsgLength = sizeof(DRIX_HEADER);
		break;
	}

	RequestMsg = ExAllocatePoolWithTag(NonPagedPool, MsgLength, DRAID_CLIENT_REQUEST_MSG_POOL_TAG);
	if (RequestMsg==NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	RtlZeroMemory(RequestMsg, MsgLength);

	RequestMsg->Signature = NTOHL(DRIX_SIGNATURE);
//	RequestMsg->Version = DRIX_CURRENT_VERSION;
	RequestMsg->Command = Command;
	RequestMsg->Length = NTOHS((UINT16)MsgLength);
	RequestMsg->Sequence = NTOHL(Client->RequestSequence);
	Client->RequestSequence++;

	if (DRIX_CMD_NODE_CHANGE == Command) {
		PDRIX_NODE_CHANGE NcMsg = (PDRIX_NODE_CHANGE) RequestMsg;

		NcMsg->UpdateCount = (UCHAR)TotalDiskCount;
		for(i=0;i<NcMsg->UpdateCount;i++) {
			NcMsg->Node[i].NodeNum = (UCHAR)i;
			NcMsg->Node[i].NodeFlags = Client->NodeFlagsLocal[i];;
			NcMsg->Node[i].DefectCode = Client->NodeDefectLocal[i];
		}
		KDPrintM(DBG_LURN_INFO, ("Sending NODE_CHANGE message\n"));		
	} else if (DRIX_CMD_REGISTER == Command) {
		PDRIX_REGISTER RegMsg = (PDRIX_REGISTER) RequestMsg;
		RtlCopyMemory(&RegMsg->RaidSetId, &Client->RaidSetId, sizeof(GUID));
		RtlCopyMemory(&RegMsg->ConfigSetId, &Client->ConfigSetId, sizeof(GUID));		
		RegMsg->Usn = NTOHL(Client->Rmd.uiUSN);
		KDPrintM(DBG_LURN_INFO, ("Sending REGISTER message\n"));		
	} else if (DRIX_CMD_ACQUIRE_LOCK == Command) {
		PDRIX_ACQUIRE_LOCK AckMsg = (PDRIX_ACQUIRE_LOCK) RequestMsg;
		AckMsg->LockType = (UCHAR)(CmdParam1 >> 8);
		AckMsg->LockMode = (UCHAR)(CmdParam1 & 0x0ff);
		AckMsg->Addr = HTONLL(CmdParam2);
		AckMsg->Length = HTONL(CmdParam3);
		KDPrintM(DBG_LURN_INFO, ("Sending ACQUIRE_LOCK message %I64x:%x\n", CmdParam2, CmdParam3));
	} else if (DRIX_CMD_RELEASE_LOCK == Command) {
		PDRIX_RELEASE_LOCK RelMsg = (PDRIX_RELEASE_LOCK) RequestMsg;
		RelMsg->LockId = HTONLL(CmdParam2);
		KDPrintM(DBG_LURN_INFO, ("Sending RELEASE_LOCK message lock id = %I64x\n", CmdParam2));
	}

	//
	// Create message context and save it to pending request queue
	//

	MsgEntry = ExAllocatePoolWithTag(NonPagedPool, sizeof(DRIX_MSG_CONTEXT), DRAID_MSG_LINK_POOL_TAG);
	if (MsgEntry == NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	RtlZeroMemory(MsgEntry, sizeof(DRIX_MSG_CONTEXT));
	InitializeListHead(&MsgEntry->Link);
	MsgEntry->Message = RequestMsg;
	MsgEntry->CallbackFunc = Callback;
	MsgEntry->CallbackParam1 = CallbackParam1;
	if (Timeout) {
		MsgEntry->HaveTimeout = TRUE;
		KeQueryTickCount(&MsgEntry->ReqTime);
		MsgEntry->Timeout = *Timeout;
	} else {
		MsgEntry->HaveTimeout = FALSE;
	}

	ExInterlockedInsertTailList(&Client->PendingRequestList, &MsgEntry->Link, &Client->SpinLock);
	
	//
	// Send via request channel.
	//
	if (Client->HasLocalArbiter) {
		PDRIX_MSG_CONTEXT ArbMsgEntry;
		KDPrintM(DBG_LURN_INFO, ("Sending to local arbiter(event=%p)\n", &Client->RequestChannel->Event));
		ArbMsgEntry = ExAllocatePoolWithTag(NonPagedPool, sizeof(DRIX_MSG_CONTEXT), DRAID_MSG_LINK_POOL_TAG);
		if (!ArbMsgEntry) 
			return STATUS_INSUFFICIENT_RESOURCES;
		RtlZeroMemory(ArbMsgEntry, sizeof(DRIX_MSG_CONTEXT));
		InitializeListHead(&ArbMsgEntry->Link);
		// We need to make copy for local arbiter
		ArbMsgEntry->Message =  ExAllocatePoolWithTag(NonPagedPool, MsgLength, DRAID_MSG_LINK_POOL_TAG);
		if (!ArbMsgEntry->Message) {
			ExFreePoolWithTag(ArbMsgEntry, DRAID_MSG_LINK_POOL_TAG);
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		RtlCopyMemory(ArbMsgEntry->Message, RequestMsg, MsgLength);
		ExInterlockedInsertTailList(&Client->RequestChannel->Queue, &ArbMsgEntry->Link, &Client->RequestChannel->Lock);
		KeSetEvent(&Client->RequestChannel->Event,IO_NO_INCREMENT, FALSE);
		return STATUS_SUCCESS;		
	} else {
		LONG Result;
		LARGE_INTEGER ReqTimeout;
		ACQUIRE_SPIN_LOCK(&Client->SpinLock, &oldIrql);
		if (Client->RequestConnection.ConnectionFileObject) {
			RELEASE_SPIN_LOCK(&Client->SpinLock, oldIrql);
			KDPrintM(DBG_LURN_INFO, ("Sending request to remote arbiter\n"));
			ReqTimeout.QuadPart = 5 * HZ;
			status =	LpxTdiSend(Client->RequestConnection.ConnectionFileObject, (PUCHAR)RequestMsg, MsgLength,
				0, &ReqTimeout, NULL, &Result);
	//		ExFreePoolWithTag(RequestMsg, DRAID_CLIENT_REQUEST_MSG_POOL_TAG); // Freed when reply arrive or when terminating.
			if (!NT_SUCCESS(status)) {
				KDPrintM(DBG_LURN_INFO, ("Failed to send request\n"));
				return STATUS_UNSUCCESSFUL;
			}

			// Start asynchous receiving 
			status = DraidClientRecvHeaderAsync(Client, &Client->RequestConnection);
			if (NT_SUCCESS(status)) {
				return STATUS_SUCCESS;
			} else {
				KDPrintM(DBG_LURN_INFO, ("Failed to recv reply asynchrous\n"));
				return STATUS_UNSUCCESSFUL;
			}
		} else {
			RELEASE_SPIN_LOCK(&Client->SpinLock, oldIrql);
			KDPrintM(DBG_LURN_INFO, ("Sending request to remote arbiter failed: Connection is closed\n"));
			return STATUS_UNSUCCESSFUL;
		}
	}
}

//
// 	Handle notification message
//	Return error if fatal error occurs so that connection should be closed.
//
NTSTATUS
DraidClientHandleNotificationMsg(
	PDRAID_CLIENT_INFO pClientInfo,
	PDRIX_HEADER	Message
) {
	PRAID_INFO				pRaidInfo;
//	NTSTATUS				status;
	PDRIX_HEADER	ReplyMsg;
	KIRQL	oldIrql;
	UINT32	i;
	UINT32 ReplyLength;
	NTSTATUS		status= STATUS_SUCCESS;
	PLURELATION_NODE Lurn = pClientInfo->Lurn;
	PLIST_ENTRY listEntry;
	PDRAID_CLIENT_LOCK Lock;
	UCHAR ResultCode = DRIX_RESULT_SUCCESS;
	pRaidInfo = Lurn->LurnRAIDInfo;
	ASSERT(pRaidInfo);

	//
	// Check data validity.
	//
	if (NTOHL(Message->Signature) != DRIX_SIGNATURE) {
		KDPrintM(DBG_LURN_INFO, ("DRIX signature mismatch\n"));
		status = STATUS_UNSUCCESSFUL;
		return status;
	}

	if (Message->ReplyFlag != 0) {
		KDPrintM(DBG_LURN_INFO, ("Should not be reply packet\n"));	
		status = STATUS_UNSUCCESSFUL;
		return status;
	}

	KDPrintM(DBG_LURN_INFO, ("Client handling message %s\n", DrixGetCmdString(Message->Command)));
	
	switch(Message->Command) {
	case DRIX_CMD_RETIRE:
		{
			BOOLEAN LockInUse;
			ULONG Retry;
			LARGE_INTEGER Timeout;
			// Enter transition mode and release all locks.
			ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
			pClientInfo->InTransition = TRUE;
			RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);	

			for(Retry=0;Retry<5;Retry++) {
				LockInUse = FALSE;
				ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
				for (listEntry = pClientInfo->LockList.Flink;
					listEntry != &pClientInfo->LockList;
					listEntry = listEntry->Flink) 
				{
					Lock = CONTAINING_RECORD (listEntry, DRAID_CLIENT_LOCK, Link);
					if (InterlockedCompareExchange(&Lock->InUseCount, 0, 0)) {
						KDPrintM(DBG_LURN_INFO, ("Lock(Id=%I64x, Range=%I64x:%x) in use.Skipping\n",
							Lock->LockId, Lock->LockAddress, Lock->LockLength));
						LockInUse = TRUE;
						continue;
					}
					if (Lock->LockStatus != LOCK_STATUS_GRANTED)
						continue;
					//
					// Remove from acquired lock list
					//
					listEntry = listEntry->Blink;
					RemoveEntryList(&Lock->Link);
					DraidClientFreeLock(Lock);
				}
				RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);	
				if (LockInUse == FALSE) {
					break;
				}
				Timeout.QuadPart = - HZ /2;
				// Wait until lock is not in use.
				KeDelayExecutionThread(KernelMode, FALSE, &Timeout);
			}
			if (LockInUse == TRUE) {
				KDPrintM(DBG_LURN_INFO, ("Failed to unlock lock(Id=%I64x, Range=%I64x:%x). Returing failure to retire request\n",
					Lock->LockId, Lock->LockAddress, Lock->LockLength));
				ResultCode = DRIX_RESULT_FAIL;
			}
		}
		break;
	case DRIX_CMD_CHANGE_STATUS:
		{
			BOOLEAN		Changed = FALSE;
			PDRIX_CHANGE_STATUS CsMsg = (PDRIX_CHANGE_STATUS) Message;
			
			if (NTOHL(CsMsg->Usn) != pClientInfo->Rmd.uiUSN) {
				KDPrintM(DBG_LURN_INFO, ("USN changed. Reload RMD (not needed until online unit change is implemented)\n"));
			}

			if (CsMsg->NodeCount != pClientInfo->TotalDiskCount) {
				KDPrintM(DBG_LURN_INFO, ("Invalid node count %d!!!\n", CsMsg->NodeCount));
				status = STATUS_UNSUCCESSFUL;
				return status;
			}

			ACQUIRE_SPIN_LOCK(&pRaidInfo->SpinLock, &oldIrql);
			ACQUIRE_DPC_SPIN_LOCK(&pClientInfo->SpinLock);
			if (CsMsg->RaidStatus !=pClientInfo->DRaidStatus) {
				KDPrintM(DBG_LURN_INFO, ("Changing RAID status from %x to %x!!!\n", 
					pClientInfo->DRaidStatus ,CsMsg->RaidStatus));
			}
			if (RtlCompareMemory(&CsMsg->ConfigSetId, &pClientInfo->ConfigSetId , sizeof(GUID)) != sizeof(GUID)) {
				KDPrintM(DBG_LURN_INFO, ("Config Set Id changed!!!\n"));
				RtlCopyMemory(&pClientInfo->ConfigSetId, &CsMsg->ConfigSetId, sizeof(GUID));
			}

			pClientInfo->DRaidStatus = CsMsg->RaidStatus;
			for(i=0;i<CsMsg->NodeCount;i++) {
				if (pClientInfo->NodeFlagsRemote[i] != CsMsg->Node[i].NodeFlags) {
					KDPrintM(DBG_LURN_INFO, ("Changing node %d flag from %x to %x!!!\n", 
						i, pClientInfo->NodeFlagsRemote[i] ,CsMsg->Node[i].NodeFlags));
				}
				pClientInfo->NodeFlagsRemote[i] = CsMsg->Node[i].NodeFlags;
				if (pClientInfo->NodeToRoleMap[i] !=CsMsg->Node[i].NodeRole) {
					KDPrintM(DBG_LURN_INFO, ("Changing node %d role from %x to %x!!!\n", 
						i, pClientInfo->NodeToRoleMap[i], CsMsg->Node[i].NodeRole));
				}

				pClientInfo->NodeToRoleMap[i] =CsMsg->Node[i].NodeRole;
				pClientInfo->RoleToNodeMap[CsMsg->Node[i].NodeRole] = (UCHAR)i;
				pRaidInfo->MapLurnChildren[CsMsg->Node[i].NodeRole] = Lurn->LurnChildren[i];
			}
			if (CsMsg->WaitForSync) {
				pClientInfo->InTransition = TRUE;
			} else {
				pClientInfo->InTransition = FALSE;
			}
			RELEASE_DPC_SPIN_LOCK(&pClientInfo->SpinLock);
			RELEASE_SPIN_LOCK(&pRaidInfo->SpinLock, oldIrql);
		}
		break;
	case DRIX_CMD_STATUS_SYNCED: // RAID/Node status is synced. Continue IO
		{
			ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
			pClientInfo->InTransition = FALSE;
			RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);	
		}
		break;
	case DRIX_CMD_REQ_TO_YIELD_LOCK:
		{
			PDRIX_REQ_TO_YIELD_LOCK YieldMsg = (PDRIX_REQ_TO_YIELD_LOCK) Message;
			UINT64 LockId = NTOHLL(YieldMsg->LockId);
			BOOLEAN MatchFound = FALSE;
			// Find matching lock.
			ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
			for (listEntry = pClientInfo->LockList.Flink;
				listEntry != &pClientInfo->LockList;
				listEntry = listEntry->Flink) 
			{
				Lock = CONTAINING_RECORD (listEntry, DRAID_CLIENT_LOCK, Link);
				if (LockId == DRIX_LOCK_ID_ALL) {
					Lock->YieldRequested = TRUE; // send release message later.
					MatchFound = TRUE;
				} else if (Lock->LockId == LockId) {
					Lock->YieldRequested = TRUE; // send release message later.
					MatchFound = TRUE;
					break;
				}
			}
			RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
			KDPrintM(DBG_LURN_INFO, ("DRIX_CMD_REQ_TO_YIELD_LOCK - Lock id %I64x %s\n", LockId,
				MatchFound?"Found":"Not found"));
			if (!MatchFound) {
				// This can happen if lock is released after receiving req-to-yield msg.
				ResultCode = DRIX_RESULT_INVALID_LOCK_ID;
			}
		}
		break;
	case DRIX_CMD_GRANT_LOCK:
		{
			PDRIX_GRANT_LOCK GrantMsg = (PDRIX_GRANT_LOCK) Message;
			UINT64 LockId = NTOHLL(GrantMsg->LockId);
			UINT64 Addr = NTOHLL(GrantMsg->Addr);
			UINT32 Length = NTOHL(GrantMsg->Length);
			BOOLEAN MatchFound = FALSE;
			// Find pending lock with LockId
			ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
			for (listEntry = pClientInfo->LockList.Flink;
				listEntry != &pClientInfo->LockList;
				listEntry = listEntry->Flink) 
			{
				Lock = CONTAINING_RECORD (listEntry, DRAID_CLIENT_LOCK, Link);
				if (Lock->LockId == LockId) {
					// Update status and addr/length 
					ASSERT(Lock->LockStatus == LOCK_STATUS_PENDING);
					Lock->LockStatus = LOCK_STATUS_GRANTED;
					Lock->LockAddress = Addr;
					Lock->LockLength = Length;
					MatchFound = TRUE;
					KDPrintM(DBG_LURN_INFO, ("DRIX_CMD_GRANT_LOCK recevied: %I64x:%x\n", Addr, Length));
					break;
				}
			}
			RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
			
			if (!MatchFound) {
				//
				// This can happen if DRIX_CMD_GRANT_LOCK is processed earlier than reply to ACQUIRE_LOCK
				//	Alloc lock here.

				Lock = DraidClientAllocLock(
					LockId, 	GrantMsg->LockType, GrantMsg->LockMode, 
					Addr, Length);
				if (Lock==NULL) {
					status = STATUS_INSUFFICIENT_RESOURCES;
				} else {
					ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql); 
					KeQueryTickCount(&Lock->LastAccessedTime);	
					Lock->LockStatus = LOCK_STATUS_GRANTED;
					KDPrintM(DBG_LURN_INFO, ("Unrequested or unreplied lock granted. Accept anyway.lock id = %I64x, range= %I64x:%x\n", Lock->LockId, 
							Lock->LockAddress, Lock->LockLength));
					InsertTailList(&pClientInfo->LockList, &Lock->Link);
					RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
				}
			}
		}
		break;
	default:
		KDPrintM(DBG_LURN_INFO, ("Unsupported command\n"));	
		status = STATUS_UNSUCCESSFUL;
		return status;
	}

	//
	// Create reply
	//
	ReplyLength = sizeof(DRIX_HEADER);
 	ReplyMsg = 	ExAllocatePoolWithTag(NonPagedPool, ReplyLength, DRAID_ARBITER_NOTIFY_REPLY_POOL_TAG);
	if (ReplyMsg==NULL) {
		status = STATUS_INSUFFICIENT_RESOURCES;
		return status;
	}
	RtlZeroMemory(ReplyMsg, ReplyLength);

	ReplyMsg->Signature = NTOHL(DRIX_SIGNATURE);
//	ReplyMsg->Version = DRIX_CURRENT_VERSION;
	ReplyMsg->ReplyFlag = TRUE;
	ReplyMsg->Command = Message->Command;
	ReplyMsg->Length = NTOHS((UINT16)ReplyLength);
	ReplyMsg->Sequence = Message->Sequence;
	ReplyMsg->Result = ResultCode;

	//
	// Send reply
	//
	if (pClientInfo->HasLocalArbiter) {
		PDRIX_MSG_CONTEXT ReplyMsgEntry;
		KDPrintM(DBG_LURN_INFO, ("DRAID Sending reply %s to local arbiter, event %p\n", 
			DrixGetCmdString(Message->Command), &pClientInfo->NotificationReplyChannel->Event));
		ReplyMsgEntry = ExAllocatePoolWithTag(NonPagedPool, sizeof(DRIX_MSG_CONTEXT), DRAID_MSG_LINK_POOL_TAG);
		if (ReplyMsgEntry) {
			RtlZeroMemory(ReplyMsgEntry, sizeof(DRIX_MSG_CONTEXT));	
			InitializeListHead(&ReplyMsgEntry->Link);
			ReplyMsgEntry->Message = ReplyMsg;
			ExInterlockedInsertTailList(&pClientInfo->NotificationReplyChannel->Queue, &ReplyMsgEntry->Link, &pClientInfo->NotificationReplyChannel->Lock);
			KeSetEvent(&pClientInfo->NotificationReplyChannel->Event,IO_NO_INCREMENT, FALSE);
			return STATUS_SUCCESS;
		} else {
			return STATUS_INSUFFICIENT_RESOURCES;
		}
	} else {
		LARGE_INTEGER Timeout;
		LONG Result;
		Timeout.QuadPart = 5 * HZ;
		KDPrintM(DBG_LURN_INFO, ("DRAID Sending reply %s to remote arbiter\n", DrixGetCmdString(Message->Command)));
		if (pClientInfo->NotificationConnection.ConnectionFileObject) {
			status =	LpxTdiSend(pClientInfo->NotificationConnection.ConnectionFileObject, (PUCHAR)ReplyMsg, ReplyLength,
				0, &Timeout, NULL, &Result);
		} else {
			status = STATUS_UNSUCCESSFUL;
		}
		ExFreePoolWithTag(ReplyMsg, DRAID_ARBITER_NOTIFY_REPLY_POOL_TAG);
		if (DRIX_CMD_RETIRE == Message->Command) {
			KDPrintM(DBG_LURN_INFO, ("RETIRE message replied. Closing connect to arbiter\n"));
			DraidClientResetRemoteArbiterContext(pClientInfo);
		}
		if (NT_SUCCESS(status)) {
			return STATUS_SUCCESS;
		} else {
			KDPrintM(DBG_LURN_INFO, ("Failed to send notification reply\n"));
			return STATUS_UNSUCCESSFUL;
		}
	}
}

VOID DraidClientUpdateAllNodeFlags(
	PDRAID_CLIENT_INFO	pClientInfo
) {
	UINT32 i;
	for(i=0;i<pClientInfo->TotalDiskCount;i++) {
		DraidClientUpdateNodeFlags(pClientInfo, pClientInfo->Lurn->LurnChildren[i], 0, 0);
	}
}

VOID
DraidClientUpdateNodeFlags(
	PDRAID_CLIENT_INFO	pClientInfo,
	PLURELATION_NODE	ChildLurn,
	UCHAR				FlagsToAdd,	// Temp parameter.. set though lurn node info.
	UCHAR 				DefectCode  // Temp parameter.. set though lurn node info.
) {
	KIRQL oldIrql;
	UCHAR NewFlags = pClientInfo->NodeFlagsLocal[ChildLurn->LurnChildIdx];
	BOOLEAN Changed = FALSE;

	ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);

	switch(ChildLurn->LurnStatus) {
	case LURN_STATUS_RUNNING:
	case LURN_STATUS_STALL:
		// Defective flag should not be not cleared
		NewFlags &= ~(DRIX_NODE_FLAG_STOP | DRIX_NODE_FLAG_UNKNOWN);
		NewFlags |= DRIX_NODE_FLAG_RUNNING;
		break;
	case LURN_STATUS_STOP_PENDING:
	case LURN_STATUS_STOP:
	case LURN_STATUS_DESTROYING:
		// Defective flag should not be not cleared
		NewFlags &= ~(DRIX_NODE_FLAG_RUNNING | DRIX_NODE_FLAG_UNKNOWN);
		NewFlags |= DRIX_NODE_FLAG_STOP;
		break;
	case LURN_STATUS_INIT:
		// Defective flag should not be not cleared
		NewFlags  &= ~(DRIX_NODE_FLAG_RUNNING | DRIX_NODE_FLAG_STOP);
		NewFlags |= DRIX_NODE_FLAG_UNKNOWN; 
		break;
	default:
		ASSERT(FALSE);
		break;
	}

	// Check bad sector or disk is detected. - to do: get defective info in cleaner way..
	if (LurnGetCauseOfFault(ChildLurn) & (LURN_FCAUSE_BAD_SECTOR|LURN_FCAUSE_BAD_DISK)) {
		NewFlags |= DRIX_NODE_FLAG_DEFECTIVE;
		DefectCode = (LurnGetCauseOfFault(ChildLurn) & LURN_FCAUSE_BAD_SECTOR)?
			DRIX_NODE_DEFECT_BAD_BLOCK:DRIX_NODE_DEFECT_BAD_DISK;
	}

	NewFlags |= FlagsToAdd;

	//
	// To do: if newflags contains defective flag, convert fault info's defective info to drix format...
	//	
	if (pClientInfo->NodeFlagsLocal[ChildLurn->LurnChildIdx] != NewFlags ||
		((NewFlags & DRIX_NODE_FLAG_DEFECTIVE) && pClientInfo->NodeDefectLocal[ChildLurn->LurnChildIdx] != DefectCode)) 
	{
		KDPrintM(DBG_LURN_INFO, ("Changing local node %d flag from %x to %x(Defect Code=%x)\n",
			ChildLurn->LurnChildIdx, pClientInfo->NodeFlagsLocal[ChildLurn->LurnChildIdx], 	NewFlags, DefectCode)
		);
		//
		// Status changed. We need to report to arbiter.
		// Set changed flags
		//
		pClientInfo->NodeFlagsLocal[ChildLurn->LurnChildIdx] = NewFlags;
		pClientInfo->NodeDefectLocal[ChildLurn->LurnChildIdx] = DefectCode;

		if (pClientInfo->NodeChanged[ChildLurn->LurnChildIdx]  & DRAID_NODE_CHANGE_FLAG_UPDATING) {
			KDPrintM(DBG_LURN_INFO, ("Set node changed flag while node is updating\n"));
		}

		pClientInfo->NodeChanged[ChildLurn->LurnChildIdx] |=DRAID_NODE_CHANGE_FLAG_CHANGED;
		KeSetEvent(&pClientInfo->ClientThreadEvent,IO_NO_INCREMENT, FALSE);
	}
	RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
}


//
// Return role index
static ULONG DraidClientRAID1RSelectChildToRead(IN PRAID_INFO pRaidInfo, IN PCCB Ccb)
{
	ULONG raid_idx = -1;
	PDRAID_CLIENT_INFO pClient;
	UINT32 i;
	KIRQL oldIrql;
	UNREFERENCED_PARAMETER(Ccb);
	//
	// temp: no read balancing..
	//
	ACQUIRE_SPIN_LOCK(&pRaidInfo->SpinLock, &oldIrql);	
	pClient = pRaidInfo->pDraidClient;
	if (pClient == NULL) {
		RELEASE_SPIN_LOCK(&pRaidInfo->SpinLock, oldIrql);
		return -1;
	}
	for(i=0;i<pClient->ActiveDiskCount;i++) {
		if ((pClient->NodeFlagsRemote[pClient->RoleToNodeMap[i]]  & DRIX_NODE_FLAG_RUNNING) &&
			!(pClient->NodeFlagsRemote[pClient->RoleToNodeMap[i]]  & (DRIX_NODE_FLAG_OUT_OF_SYNC | DRIX_NODE_FLAG_DEFECTIVE))) {
			raid_idx = i;
		}
	}
	if (raid_idx == -1) {
		// Both node is not running. Try to read from non out-of-sync.
		for(i=0;i<pClient->ActiveDiskCount;i++) {
			if (!(pClient->NodeFlagsRemote[pClient->RoleToNodeMap[i]]  & (DRIX_NODE_FLAG_OUT_OF_SYNC | DRIX_NODE_FLAG_DEFECTIVE))) {
				raid_idx = i;
			}
		}
	}
	ASSERT(raid_idx != -1);

	
#if 0
	if (RAID_STATUS_EMERGENCY == pClient->DRaidStatus ||
		RAID_STATUS_EMERGENCY_READY == pClient->DRaidStatus ||
		RAID_STATUS_RECOVERING == pClient->DRaidStatus)
	{
		for(i=0;i<pRaidInfo->nDiskCount +  pRaidInfo->nSpareDisk;i++) {
			if ((pClient->NodeFlagsRemote[i]  & DRIX_NODE_FLAG_RUNNING) &&
				!(pClient->NodeFlagsRemote[i]  & DRIX_NODE_FLAG_OUT_OF_SYNC)) {
				raid_idx = pClient->NodeToRoleMap[i];
			}
		}
	}
	else
	{
		if (pRaidInfo->MapLurnChildren[0]->LurnStatus != LURN_STATUS_RUNNING &&
			pRaidInfo->MapLurnChildren[1]->LurnStatus != LURN_STATUS_RUNNING
		) {
			static int temp=0; // temp test...
			temp = !temp;
			// To do: if both child is in stall, select alternatively.
			raid_idx = temp;
		} else if (pRaidInfo->MapLurnChildren[0]->LurnStatus != LURN_STATUS_RUNNING) {
			raid_idx = 1;
		} else if (pRaidInfo->MapLurnChildren[1]->LurnStatus != LURN_STATUS_RUNNING) {
			raid_idx = 0;
		} else if (pRaidInfo->MapLurnChildren[0]->LurnStatus == LURN_STATUS_RUNNING &&
				pRaidInfo->MapLurnChildren[1]->LurnStatus == LURN_STATUS_RUNNING) {
			UINT64 logicalBlockAddress; 
			INT64 Diff0, Diff1;
			//
			// if both disk is active, do read-balancing.
			// Optimize for random access right now
			//    Read from target which accessed nearer position - this policy may result in reading more from one target.
			//
			LSCcbGetAddressAndLength((PCDB)Ccb->Cdb, &logicalBlockAddress, NULL);
			Diff0 = (INT64)logicalBlockAddress - (INT64)pRaidInfo->MapLurnChildren[0]->LastAccessedAddress;
			Diff1 = (INT64)logicalBlockAddress - (INT64)pRaidInfo->MapLurnChildren[1]->LastAccessedAddress;
			if (Diff0 <0 && Diff1 < 0) {
				Diff0 = -Diff0;
				Diff1 = -Diff1;
				if (Diff0 < Diff1) {
					raid_idx = 0;
				} else {
					raid_idx = 1;
				}
			} else if (Diff0 <0) {
				raid_idx = 1;
			} else if (Diff1 <0) {
				raid_idx = 0;
			} else {
				if (Diff0<=Diff1){
					// Perhaps Disk 0 is near to this request position
					raid_idx = 0;
				} else {
					raid_idx = 1;
				}
			}
		} else {
			// Should not come here.
			KDPrintM(DBG_LURN_INFO, ("Cannot find running child"));
			raid_idx = 0;
		}
	}
#endif	
	RELEASE_SPIN_LOCK(&pRaidInfo->SpinLock, oldIrql);

	return raid_idx;
}

NTSTATUS
DraidClientSendCcbToNondefectiveChildren(
	PDRAID_CLIENT_INFO pClientInfo, 
	IN PCCB						Ccb,
	IN CCB_COMPLETION_ROUTINE	CcbCompletion
) {
	//
	// Currently assume RAID1 only
	//

	BOOLEAN DefectiveExist = FALSE;
	int DiskNonDefective = -1;
	NTSTATUS status;
	PLURELATION_NODE Lurn = pClientInfo->Lurn;
	
	if (pClientInfo->DRaidStatus == DRIX_RAID_STATUS_DEGRADED) {
		if (pClientInfo->NodeFlagsRemote[Lurn->LurnRAIDInfo->MapLurnChildren[0]->LurnChildIdx] &	
			DRIX_NODE_FLAG_DEFECTIVE) 
		{
			DefectiveExist = TRUE;
			DiskNonDefective = 1;
		} else if (pClientInfo->NodeFlagsRemote[Lurn->LurnRAIDInfo->MapLurnChildren[1]->LurnChildIdx] &	
			DRIX_NODE_FLAG_DEFECTIVE) 
		{
			DefectiveExist = TRUE;
			DiskNonDefective = 0;
		}
	}
	if (DefectiveExist) {
		KDPrintM(DBG_LURN_TRACE, ("Defective disk exist. Send Ccb only to non-defective disk %d\n", DiskNonDefective));
		status = LurnAssocSendCcbToChildrenArray(
			&Lurn->LurnRAIDInfo->MapLurnChildren[DiskNonDefective],
			1,
			Ccb,
			CcbCompletion,
			NULL,
			NULL,
			FALSE
			);
	} else {
		status = LurnAssocSendCcbToChildrenArray(
			Lurn->LurnRAIDInfo->MapLurnChildren,
			Lurn->LurnRAIDInfo->nDiskCount,
			Ccb,
			CcbCompletion,
			NULL,
			NULL,
			FALSE
			);
	}
	return status;
}

//
// Currrently handles RAID1 case only...
//
NTSTATUS 
DraidClientDispatchCcb(
	PDRAID_CLIENT_INFO pClientInfo, 
	PCCB Ccb)
{
	NTSTATUS status = STATUS_SUCCESS;
	PLURELATION_NODE Lurn = pClientInfo->Lurn;
	
	// Handle 
	//   SCSIOP_WRITE, SCSIOP_WRITE16, READ_LONG, 
	//	SCSIOP_READ, SCSIOP_READ16, SCSIOP_VERIFY, SCSIOP_VERIFY16 only.
	// Other command should not be queued.

	switch(Ccb->Cdb[0]) {
	case SCSIOP_WRITE:
	case SCSIOP_WRITE16:		
		{

			KDPrintM(DBG_LURN_NOISE,("SCSIOP_WRITE\n"));

			KDPrintM(DBG_LURN_NOISE,("W Ccb->DataBufferLength %d\n", Ccb->DataBufferLength));

			if(Lurn->Lur->EnabledNdasFeatures & NDASFEATURE_SECONDARY){
				// Fake write is handled by LurProcessWrite.
				// Just check assumptions
				ASSERT(Lurn->Lur->EnabledNdasFeatures & NDASFEATURE_SIMULTANEOUS_WRITE);
			}

			status = DraidClientIoWithLock(pClientInfo, DRIX_LOCK_MODE_EX, Ccb);

			if (status == STATUS_SUCCESS) {
				// Ccb is handled or queued to pending message context.
			} else if (status == STATUS_PENDING)  {
				// Lock is still not granted
				Ccb->CcbStatus = CCB_STATUS_BUSY;
				goto complete_here;
			} else {
				KDPrintM(DBG_LURN_ERROR,("SCSIOP_WRITE error %x.\n", status));
				// Pass error. 
			}
		}
		break;
	case SCSIOP_VERIFY:
	case SCSIOP_VERIFY16:
		{
			KDPrintM(DBG_LURN_NOISE,("SCSIOP_VERIFY\n"));

			status = DraidClientSendCcbToNondefectiveChildren(pClientInfo, Ccb, LurnRAID1RCcbCompletion);
			
		}
		break;
	case 0x3E:		// READ_LONG
	case SCSIOP_READ:
	case SCSIOP_READ16:	
		{
		ULONG				role_idx;
		
		//
		//	Find a child LURN to run.
		//
		role_idx = DraidClientRAID1RSelectChildToRead(Lurn->LurnRAIDInfo, Ccb);

		KDPrintM(DBG_LURN_TRACE,("SCSIOP_READ: decided LURN#%d\n", role_idx));
		//
		//	Set completion routine
		//
		status = LurnAssocSendCcbToChildrenArray(
			&Lurn->LurnRAIDInfo->MapLurnChildren[role_idx],
			1,
			Ccb,
			LurnRAID1RCcbCompletion,
			NULL,
			NULL,
			FALSE
			);
		}

		break;
	default:
		// This should not happen.
		ASSERT(FALSE);
		break;
	}
	
	// Ccb still pending
	return status;
complete_here:
	// We can complete ccb now.
	LSAssocSetRedundentRaidStatusFlag(Lurn, Ccb);
	LSCcbCompleteCcb(Ccb);	
	return status;
}

//
// Can be called with spinlock held by SCSIPORT
//
NTSTATUS
DraidClientFlush(PLURELATION_NODE Lurn, PCCB Ccb, CCB_COMPLETION_ROUTINE CompRoutine)
{
	PRAID_INFO pRaidInfo = Lurn->LurnRAIDInfo;
	PDRAID_CLIENT_INFO pClientInfo;
	KIRQL oldIrql;
	
	KDPrintM(DBG_LURN_INFO,("Flushing RAID\n"));
	
	if (!pRaidInfo)
		return STATUS_SUCCESS;
	
	pClientInfo = pRaidInfo->pDraidClient;
	if (!pClientInfo)
		return STATUS_SUCCESS;
	
	//
	// Flush in-memory cache(currently we don't have) and release locks
	//
	ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);	
	if (pClientInfo->RequestForFlush) {
		// FLUSH is already in progress
		KDPrintM(DBG_LURN_INFO,("FLUSH is already in progress.\n"));
		RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);	
		return STATUS_SUCCESS;
	}
	
	pClientInfo->RequestForFlush = TRUE;
	pClientInfo->FlushCcb = Ccb;
	pClientInfo->FlushCompletionRoutine = CompRoutine;
	KeSetEvent(&pClientInfo->ClientThreadEvent,IO_NO_INCREMENT, FALSE);
#if 0
	LockCount = 0;
	if (DRAID_CLIENT_STATUS_ARBITER_CONNECTED == pClientInfo->ClientStatus) {		
		for (listEntry = pClientInfo->LockList.Flink;
			listEntry != &pClientInfo->LockList;
			listEntry = listEntry->Flink) 
		{
			Lock = CONTAINING_RECORD (listEntry, DRAID_CLIENT_LOCK, Link);
			Lock->YieldRequested = TRUE;
			LockCount++;
		}
	}
	if (LockCount ==0) {
		// No need to wait for flushing completion.
		pClientInfo->RequestForFlush = FALSE;
		pClientInfo->FlushCcb = NULL;
		pClientInfo->FlushCompletionRoutine = NULL;
		RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);	
		CompRoutine(Ccb, NULL);
		return STATUS_SUCCESS;		
	}
#endif	
	RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);

	return STATUS_PENDING;
}

VOID
DraidClientRefreshReadOnlyRaidStatus(
	PDRAID_CLIENT_INFO Client
) {
	KIRQL oldIrql;
	UINT32 i;
	UINT32 FaultCount;
	
	ACQUIRE_SPIN_LOCK(&Client->SpinLock, &oldIrql);

	//
	// Update NodeFlagsLocal to NodeFlagsRemote
	//
	for(i=0;i<Client->TotalDiskCount;i++) {
		Client->NodeFlagsRemote[i] =Client->NodeFlagsLocal[i];
	}

	//
	// Apply RMD information to remote flags
	//

	for(i = 0; i < Client->TotalDiskCount; i++) // i: role based.
	{
		KDPrintM(DBG_LURN_ERROR, ("MAPPING Lurn node %d to RAID role %d\n",
			Client->Rmd.UnitMetaData[i].iUnitDeviceIdx, i));
		Client->RoleToNodeMap[i] = (UCHAR)Client->Rmd.UnitMetaData[i].iUnitDeviceIdx;
		Client->NodeToRoleMap[Client->Rmd.UnitMetaData[i].iUnitDeviceIdx] = (UCHAR)i;

		Client->Lurn->LurnRAIDInfo->MapLurnChildren[i] = Client->Lurn->LurnChildren[Client->RoleToNodeMap[i]];

		if(NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED & Client->Rmd.UnitMetaData[i].UnitDeviceStatus)
		{
			if (i<Client->ActiveDiskCount) {
				Client->NodeFlagsRemote[Client->RoleToNodeMap[i]] |= DRIX_NODE_FLAG_OUT_OF_SYNC;
				KDPrintM(DBG_LURN_ERROR, ("Node %d(role %d) is out-of-sync\n",  Client->RoleToNodeMap[i], i));
			}
		}
		if(NDAS_UNIT_META_BIND_STATUS_DEFECTIVE & Client->Rmd.UnitMetaData[i].UnitDeviceStatus)
		{
			Client->NodeFlagsRemote[Client->RoleToNodeMap[i]] |= DRIX_NODE_FLAG_DEFECTIVE;
		
			// fault device found
			KDPrintM(DBG_LURN_ERROR, ("Node %d(role %d) is defective\n",  Client->RoleToNodeMap[i], i));
			Client->NodeDefectLocal[Client->RoleToNodeMap[i]] = Client->Rmd.UnitMetaData[i].DefectCode;
		}
	}

	//
	// Update raid status. In read-only raid, there is no rebuild-mode.
	//

	FaultCount = 0;
	for(i=0;i<Client->ActiveDiskCount;i++) { // i : role index
		if (!(Client->NodeFlagsRemote[Client->RoleToNodeMap[i]] & DRIX_NODE_FLAG_RUNNING)
			|| (Client->NodeFlagsRemote[Client->RoleToNodeMap[i]] & DRIX_NODE_FLAG_OUT_OF_SYNC)
			|| (Client->NodeFlagsRemote[Client->RoleToNodeMap[i]] & DRIX_NODE_FLAG_DEFECTIVE)
		) {
			// Node is not running or out-of-sync or defective.
			FaultCount++;
		}
	}

	if (FaultCount == 0) {
		if (Client->DRaidStatus !=DRIX_RAID_STATUS_NORMAL)  {
			KDPrintM(DBG_LURN_INFO, ("Entering RAID STATUS NORMAL\n"));	
		}
		Client->DRaidStatus = DRIX_RAID_STATUS_NORMAL;
	} else if (FaultCount == 1) {
		if (Client->DRaidStatus !=DRIX_RAID_STATUS_DEGRADED)  {
			KDPrintM(DBG_LURN_INFO, ("Entering DEGRADED status\n"));	
		}
		Client->DRaidStatus = DRIX_RAID_STATUS_DEGRADED;
	} else { // FaultCount > 1
		KDPrintM(DBG_LURN_INFO, ("More than 1 active node is at fault. RAID failure\n"));
		Client->DRaidStatus = DRIX_RAID_STATUS_FAILED;
	} 
	
	RELEASE_SPIN_LOCK(&Client->SpinLock, oldIrql);
}



NTSTATUS
DraidClientConnectToArbiter(
	PDRAID_CLIENT_INFO		Client,
	PDRAID_CLIENT_CONNECTION Connection
) {
	KIRQL oldIrql;
	PLURNEXT_IDE_DEVICE		IdeDisk;
	LPX_ADDRESS LocalAddr = {0};
	LPX_ADDRESS RemoteAddr = {0};
	BOOLEAN LocalAddrFound;
	UINT32 i;
	NTSTATUS status;	
	HANDLE					addressFileHandle = NULL;
	PFILE_OBJECT				addressFileObject = NULL;
	HANDLE					connectionFileHandle = NULL;
	PFILE_OBJECT				connectionFileObject = NULL;
	BOOLEAN Connected;

	KDPrintM(DBG_LURN_INFO, ("Connecting to arbiter\n"));
	ACQUIRE_SPIN_LOCK(&Connection->SpinLock, &oldIrql);
	if (Connection->Status != DRAID_CLIENT_CON_INIT) {
		KDPrintM(DBG_LURN_INFO, ("Already in connected or connecting status\n"));
		RELEASE_SPIN_LOCK(&Connection->SpinLock, oldIrql);
		return STATUS_CONNECTION_ACTIVE;
	}
	Connection->Status = DRAID_CLIENT_CON_CONNECTING;
	RELEASE_SPIN_LOCK(&Connection->SpinLock, oldIrql);

	// Find local address to use.

	LocalAddrFound = FALSE;
	for (i=0;i<Client->TotalDiskCount;i++) {
		//
		// To do: get bind address without breaking LURNEXT_IDE_DEVICE abstraction 
		//
		if (!Client->Lurn->LurnChildren[i])
			continue;
		if(LURN_STATUS_RUNNING != Client->Lurn->LurnChildren[i]->LurnStatus)
			continue;
		IdeDisk = (PLURNEXT_IDE_DEVICE)Client->Lurn->LurnChildren[i]->LurnExtension;
		if (!IdeDisk)
			continue;
		RtlCopyMemory(&LocalAddr, &IdeDisk->LanScsiSession.BindAddress.Address[0].Address, sizeof(LPX_ADDRESS));
		LocalAddrFound = TRUE;
		break;
	}
	if (!LocalAddrFound) {
		KDPrintM(DBG_LURN_INFO, ("No local address to use\n"));
		status = STATUS_UNSUCCESSFUL;
		goto errout;
	}

	Connected = FALSE;
	// Try to connect for each arbiter address
	for(i=0;i<NDAS_DRAID_ARBITER_ADDR_COUNT;i++) {
		if (Client->Rmd.ArbiterInfo[i].Type != NDAS_DRAID_ARBITER_TYPE_LPX)
			continue;
		RtlCopyMemory(RemoteAddr.Node, Client->Rmd.ArbiterInfo[i].Addr, 6);
		RemoteAddr.Port = HTONS(DRIX_ARBITER_PORT_NUM_BASE);

		KDPrintM(DBG_LURN_INFO, ("Connecting from %02X:%02X:%02X:%02X:%02X:%02X 0x%4X to %02X:%02X:%02X:%02X:%02X:%02X 0x%4X\n",
					LocalAddr.Node[0], LocalAddr.Node[1], LocalAddr.Node[2],
					LocalAddr.Node[3], LocalAddr.Node[4], LocalAddr.Node[5],
					NTOHS(LocalAddr.Port),
					RemoteAddr.Node[0], RemoteAddr.Node[1], RemoteAddr.Node[2],
					RemoteAddr.Node[3], RemoteAddr.Node[4], RemoteAddr.Node[5],
					NTOHS(RemoteAddr.Port)));

		status = LpxTdiOpenAddress(
						&addressFileHandle,
						&addressFileObject,
						&LocalAddr
						);
		if (!NT_SUCCESS(status)) {
			ASSERT(FALSE);
			addressFileHandle = NULL;
			addressFileObject = NULL;
			continue;
		}
		status = LpxTdiOpenConnection(
						&connectionFileHandle,
						&connectionFileObject,
						NULL
						);
		if(!NT_SUCCESS(status)) 
		{
			ASSERT(FALSE);
			LpxTdiCloseAddress (addressFileHandle, addressFileObject);
			addressFileHandle = NULL; 
			addressFileObject = NULL;
			continue;
		}
		status = 	LpxTdiAssociateAddress(
						connectionFileObject,
						addressFileHandle
						);
		if(!NT_SUCCESS(status)) 
		{
			ASSERT(FALSE);
			LpxTdiCloseAddress (addressFileHandle, addressFileObject);
			addressFileHandle = NULL; 
			addressFileObject = NULL;
			LpxTdiCloseConnection(connectionFileHandle, connectionFileObject);
			connectionFileHandle = NULL; 
			connectionFileObject = NULL;
			continue;
		}
		status = 	LpxTdiConnect(
						connectionFileObject,
						&RemoteAddr
						);
		if (!NT_SUCCESS(status)) {
			KDPrintM(DBG_LURN_INFO, ("Connection failed\n"));
			LpxTdiDisassociateAddress(connectionFileObject);
			LpxTdiCloseConnection(connectionFileHandle, connectionFileObject);
			connectionFileHandle = NULL; connectionFileObject = NULL;
			LpxTdiCloseAddress (addressFileHandle, addressFileObject);
			addressFileHandle = NULL; addressFileObject = NULL;
			continue;
		}
		KDPrintM(DBG_LURN_INFO, ("Connected\n"));
		Connected = TRUE;
		break;
	}
	if (!Connected) {
		status = STATUS_UNSUCCESSFUL;
		goto errout;
	}
	//
	// Store connection info.
	//
	Connection->AddressFileHandle = addressFileHandle;
	Connection->AddressFileObject = addressFileObject;
	Connection->ConnectionFileHandle = connectionFileHandle;
	Connection->ConnectionFileObject = connectionFileObject;
	Connection->RemoteAddr = RemoteAddr;
	Connection->LocalAddr = LocalAddr;
	
	ACQUIRE_SPIN_LOCK(&Connection->SpinLock, &oldIrql);
	Connection->Status = DRAID_CLIENT_CON_CONNECTED;
	RELEASE_SPIN_LOCK(&Connection->SpinLock, oldIrql);
	return STATUS_SUCCESS;

errout:
	ACQUIRE_SPIN_LOCK(&Connection->SpinLock, &oldIrql);
	Connection->Status = DRAID_CLIENT_CON_INIT;
	RELEASE_SPIN_LOCK(&Connection->SpinLock, oldIrql);
	return status;
}

NTSTATUS
DraidClientDisconnectFromArbiter(
	PDRAID_CLIENT_INFO		Client,
	PDRAID_CLIENT_CONNECTION Connection
) {
	KIRQL oldIrql;
	UNREFERENCED_PARAMETER(Client);

	KDPrintM(DBG_LURN_INFO, ("Disconnecting from arbiter\n"));
	ACQUIRE_SPIN_LOCK(&Connection->SpinLock, &oldIrql);
	if (Connection->Status == DRAID_CLIENT_CON_INIT) {
		RELEASE_SPIN_LOCK(&Connection->SpinLock, oldIrql);
		KDPrintM(DBG_LURN_INFO, ("Already disconnected\n"));
		return STATUS_SUCCESS;
	}
	RELEASE_SPIN_LOCK(&Connection->SpinLock, oldIrql);

	if (Connection->ConnectionFileObject) {
		LpxTdiDisconnect(Connection->ConnectionFileObject, 0);
		LpxTdiDisassociateAddress(Connection->ConnectionFileObject);
		LpxTdiCloseConnection(Connection->ConnectionFileHandle, Connection->ConnectionFileObject);
		Connection->ConnectionFileHandle = NULL;
		Connection->ConnectionFileObject = NULL;
	}
	if (Connection->AddressFileHandle) {
		LpxTdiCloseAddress(Connection->AddressFileHandle, Connection->AddressFileObject);
		Connection->AddressFileHandle = NULL; 
		Connection->AddressFileObject = NULL;
	}
	KDPrintM(DBG_LURN_INFO, ("Disconnected from arbiter\n"));

	ACQUIRE_SPIN_LOCK(&Connection->SpinLock, &oldIrql);
	Connection->Status = DRAID_CLIENT_CON_INIT;
	RELEASE_SPIN_LOCK(&Connection->SpinLock, oldIrql);
	return STATUS_SUCCESS;
}

NTSTATUS
DraidClientRegisterToRemoteArbiter(
	PDRAID_CLIENT_INFO		pClientInfo,
	PDRAID_CLIENT_CONNECTION Connection,
	UCHAR					ConType	// DRIX_CONN_TYPE_*
) {
	DRIX_REGISTER 	RegMsg = {0};
	DRIX_HEADER	ReplyMsg = {0};
	LARGE_INTEGER TimeOut;
	NTSTATUS status;
	LONG result;
	KIRQL oldIrql;
	
	ASSERT(Connection->Status == DRAID_CLIENT_CON_CONNECTED);

	Connection->Sequence++;
	RegMsg.Header.Signature = HTONL(DRIX_SIGNATURE);
	RegMsg.Header.Command = DRIX_CMD_REGISTER;
	RegMsg.Header.Length = HTONS((UINT16)sizeof(DRIX_REGISTER));
	RegMsg.Header.ReplyFlag = 0;
	RegMsg.Header.Sequence =  HTONS((UINT16)Connection->Sequence);
	RegMsg.Header.Result = 0;

	RegMsg.ConnType = ConType;
	RtlCopyMemory(&RegMsg.RaidSetId, &pClientInfo->RaidSetId, sizeof(GUID));
	RtlCopyMemory(&RegMsg.ConfigSetId, &pClientInfo->ConfigSetId, sizeof(GUID));	
	RegMsg.Usn = HTONL(pClientInfo->Rmd.uiUSN);

	KDPrintM(DBG_LURN_INFO, ("Sending register message to remote arbiter\n"));
	TimeOut.QuadPart = 5 * HZ;
	// Send register message
	status =	LpxTdiSend(Connection->ConnectionFileObject, (PUCHAR)&RegMsg, sizeof(DRIX_REGISTER), 
					0, &TimeOut,	NULL, &result);
	KDPrintM(DBG_LURN_INFO, ("LpxTdiSend status=%x, result=%x.\n", status, result));

	TimeOut.QuadPart = 5 * HZ;
	status = LpxTdiRecv(Connection->ConnectionFileObject, (PUCHAR)&ReplyMsg, sizeof(DRIX_HEADER),
		0, &TimeOut, NULL, &result);
	if (status != STATUS_SUCCESS) {
		KDPrintM(DBG_LURN_INFO, ("Failed to recv %x\n", status));
		return STATUS_UNSUCCESSFUL;
	}
	// Check received message
	if (result != sizeof(DRIX_HEADER) || NTOHL(ReplyMsg.Signature) != DRIX_SIGNATURE ||
		ReplyMsg.Command != DRIX_CMD_REGISTER || ReplyMsg.ReplyFlag !=1 ||
		NTOHS(ReplyMsg.Length) !=  sizeof(DRIX_HEADER)) {
		ASSERT(FALSE);
		KDPrintM(DBG_LURN_INFO, ("Invalid replay packet to register msg\n"));
		return STATUS_UNSUCCESSFUL;
	}
	switch(ReplyMsg.Result) {
	case DRIX_RESULT_SUCCESS:
		KDPrintM(DBG_LURN_INFO, ("Registration succeeded.\n"));
		ACQUIRE_SPIN_LOCK(&Connection->SpinLock, &oldIrql);
		Connection->Status = DRAID_CLIENT_CON_REGISTERED;
		RELEASE_SPIN_LOCK(&Connection->SpinLock, oldIrql);	
		return STATUS_SUCCESS;
	case DRIX_RESULT_FAIL:
	case DRIX_RESULT_RAID_SET_NOT_FOUND:
	default:
		KDPrintM(DBG_LURN_INFO, ("Registration failed with result %s\n", DrixGetResultString(ReplyMsg.Result)));
		return STATUS_UNSUCCESSFUL;
	}
}

//
// Establish both notification and request connection.
//
NTSTATUS
DraidClientEstablishArbiter(
	PDRAID_CLIENT_INFO		pClientInfo
) {
	NTSTATUS status;
	KIRQL	oldIrql;

	status = LurnRMDRead(pClientInfo->Lurn, &pClientInfo->Rmd, NULL);

	if(!NT_SUCCESS(status))
	{
		KDPrintM(DBG_LURN_ERROR, ("Client failed to read RMD. Exiting client thread\n"));
		return STATUS_UNSUCCESSFUL;
	}
	
	status = DraidClientConnectToArbiter(pClientInfo, &pClientInfo->NotificationConnection);
	if (!NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_ERROR, ("Failed to connect notification\n"));
		return STATUS_UNSUCCESSFUL;
	}

	// Register as notification type
	status = DraidClientRegisterToRemoteArbiter(pClientInfo, &pClientInfo->NotificationConnection, DRIX_CONN_TYPE_NOTIFICATION);
	if (!NT_SUCCESS(status)) {
		DraidClientDisconnectFromArbiter(pClientInfo, &pClientInfo->NotificationConnection);
		
		KDPrintM(DBG_LURN_ERROR, ("Failed to register connect notification\n"));
		return STATUS_UNSUCCESSFUL;
	}
	
	status = DraidClientConnectToArbiter(pClientInfo, &pClientInfo->RequestConnection);
	if (!NT_SUCCESS(status)) {
		DraidClientDisconnectFromArbiter(pClientInfo, &pClientInfo->NotificationConnection);
		KDPrintM(DBG_LURN_ERROR, ("Failed to connect request\n"));
		return STATUS_UNSUCCESSFUL;
	}

	// Register as request type
	status = DraidClientRegisterToRemoteArbiter(pClientInfo, &pClientInfo->RequestConnection, DRIX_CONN_TYPE_REQUEST);
	if (!NT_SUCCESS(status)) {
		DraidClientDisconnectFromArbiter(pClientInfo, &pClientInfo->NotificationConnection);
		DraidClientDisconnectFromArbiter(pClientInfo, &pClientInfo->RequestConnection);		
		KDPrintM(DBG_LURN_ERROR, ("Failed to register connect request\n"));
		return STATUS_UNSUCCESSFUL;
	}

	// Initialize request connection.	
	pClientInfo->RequestConnection.TdiReceiveContext.Irp = NULL;
	KeInitializeEvent(&pClientInfo->RequestConnection.TdiReceiveContext.CompletionEvent, NotificationEvent, FALSE) ;

	// Wait for network event of notification connection 
	status = DraidClientRecvHeaderAsync(pClientInfo, &pClientInfo->NotificationConnection);
	if(!NT_SUCCESS(status)) {
		DraidClientDisconnectFromArbiter(pClientInfo, &pClientInfo->NotificationConnection);
		DraidClientDisconnectFromArbiter(pClientInfo, &pClientInfo->RequestConnection);		
		KDPrintM(DBG_LURN_ERROR, ("Failed to recv notification message\n"));
		return STATUS_UNSUCCESSFUL;
	}

	ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
	KDPrintM(DBG_LURN_INFO, ("Connected to arbiter\n"));	
	pClientInfo->ClientStatus = DRAID_CLIENT_STATUS_ARBITER_CONNECTED;
	RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
	return STATUS_SUCCESS;
}

NTSTATUS
DraidClientRecvHeaderAsync(
	PDRAID_CLIENT_INFO		Client,
	PDRAID_CLIENT_CONNECTION Connection
) {
	NTSTATUS status;
	PLARGE_INTEGER Timeout;
	KIRQL oldIrql;
	
	UNREFERENCED_PARAMETER(Client);
	KDPrintM(DBG_LURN_INFO, ("RxPendingCount = %x\n",Connection->RxPendingCount));	
	ACQUIRE_SPIN_LOCK(&Client->SpinLock, &oldIrql);
	if (!Connection->ConnectionFileObject) {
		KDPrintM(DBG_LURN_INFO, ("Connection is closed\n"));
		RELEASE_SPIN_LOCK(&Client->SpinLock, oldIrql);	
		return STATUS_UNSUCCESSFUL;
	}
	RELEASE_SPIN_LOCK(&Client->SpinLock, oldIrql);
	
	if (InterlockedCompareExchange(&Connection->Waiting, 1, 0) == 0) {
		Connection->TdiReceiveContext.Irp = NULL;
		KeInitializeEvent(&Connection->TdiReceiveContext.CompletionEvent, NotificationEvent, FALSE) ;
		if (Connection->Timeout.QuadPart) {
			Timeout = &Connection->Timeout;
		} else {
			Timeout = NULL;
		}
#if DBG
		RtlZeroMemory(Connection->ReceiveBuf, sizeof(Connection->ReceiveBuf));
#endif
		status = LpxTdiRecvWithCompletionEvent(
						Connection->ConnectionFileObject,
						&Connection->TdiReceiveContext,
						Connection->ReceiveBuf,
						sizeof(DRIX_HEADER),	0, NULL, Timeout
		);
		if (!NT_SUCCESS(status)) {
			KDPrintM(DBG_LURN_ERROR, ("Failed to recv async\n"));
		}
		
	}else {
		KDPrintM(DBG_LURN_ERROR, ("Previous recv is pending\n"));
		InterlockedIncrement(&Connection->RxPendingCount);
		// Already waiting 
		status = STATUS_PENDING;
	}
	return status;
}

NTSTATUS
DraidClientRecvAdditionalDataFromRemote(
	PDRAID_CLIENT_INFO		pClientInfo,
	PDRAID_CLIENT_CONNECTION Connection
) {
	PDRIX_HEADER	Message;
	ULONG MsgLength;
	BOOLEAN InvalidPacket = FALSE;
	LARGE_INTEGER TimeOut;
	NTSTATUS status;
	UNREFERENCED_PARAMETER(pClientInfo);
	
	//
	// Read remaining data if needed.
	//
	Message = (PDRIX_HEADER) Connection->ReceiveBuf;
	MsgLength = NTOHS(Message->Length);
	if (MsgLength > DRIX_MAX_REQUEST_SIZE) {
		KDPrintM(DBG_LURN_INFO, ("DRIX message is too long %d\n", MsgLength));
		InvalidPacket = TRUE;
	} else if (MsgLength > sizeof(DRIX_HEADER)) {
		LONG result;
		ULONG AddtionalLength;
		AddtionalLength = MsgLength - sizeof(DRIX_HEADER);
		
		TimeOut.QuadPart = 5 * HZ;
		KDPrintM(DBG_LURN_TRACE, ("Reading additional message data %d bytes\n", AddtionalLength));
		if (Connection->ConnectionFileObject) {
			status = LpxTdiRecv(Connection->ConnectionFileObject, 
				(PUCHAR)(Connection->ReceiveBuf + sizeof(DRIX_HEADER)),
				AddtionalLength,
				0, &TimeOut, NULL, &result);
		} else {
			status = STATUS_UNSUCCESSFUL;
		}
		if (!NT_SUCCESS(status) || result != AddtionalLength) {
			KDPrintM(DBG_LURN_INFO, ("Failed to get remaining reply message: status=%x, result=%x\n",
				status, result
			));
			ASSERT(FALSE);
			InvalidPacket = TRUE;
		}
	} else if (MsgLength < sizeof(DRIX_HEADER)) {
		KDPrintM(DBG_LURN_INFO, ("Too small message length=%d\n",MsgLength));
		InvalidPacket = TRUE;
		ASSERT(FALSE);
	}
	if (InvalidPacket)
		return STATUS_UNSUCCESSFUL;
	else 
		return STATUS_SUCCESS;
}
	

//
// Return STATUS_SUCCESS if packet is received
// Return STATUS_UNSUCCESSFUL if connection is closed or in error or failed to read additional data.
// Return STATUS_PENDING if packet is not yet received.
//
NTSTATUS
DraidClientRecvNoWait(
	PDRAID_CLIENT_INFO		Client,
	PDRAID_CLIENT_CONNECTION Connection
) {
	NTSTATUS status = STATUS_SUCCESS;
	LONG Waiting;
	if (KeReadStateEvent(&Connection->TdiReceiveContext.CompletionEvent)) {
		KeClearEvent(&Connection->TdiReceiveContext.CompletionEvent);
		if (Connection->TdiReceiveContext.Result <0) {
			KDPrintM(DBG_LURN_INFO, ("Receive result=%x\n",Connection->TdiReceiveContext.Result));
			status = STATUS_UNSUCCESSFUL;
		} else {
			status = DraidClientRecvAdditionalDataFromRemote(Client, Connection);
		}
		if (status == STATUS_SUCCESS) {
			Waiting = InterlockedDecrement(&Connection->Waiting);
			ASSERT(Waiting == 0);
		} else {
			status = STATUS_UNSUCCESSFUL;
		}
	} else {
		status = STATUS_PENDING;
	}
	return status;
}



NTSTATUS 
DraidClientResetRemoteArbiterContext(
	PDRAID_CLIENT_INFO		pClientInfo
) {
	KIRQL oldIrql;
	PLIST_ENTRY listEntry;
	PDRAID_CLIENT_LOCK Lock;
	LARGE_INTEGER Timeout;
	PDRIX_MSG_CONTEXT MsgContext;
	BOOLEAN		LockInUse;
	KDPrintM(DBG_LURN_INFO, ("Resetting remote arbiter context\n"));
	
	DraidClientDisconnectFromArbiter(pClientInfo, &pClientInfo->NotificationConnection);
	DraidClientDisconnectFromArbiter(pClientInfo, &pClientInfo->RequestConnection);	

	ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
	pClientInfo->ClientStatus = DRAID_CLIENT_STATUS_INITIALIZING;
	pClientInfo->DRaidStatus = DRIX_RAID_STATUS_INITIALIZING;
	pClientInfo->InTransition = TRUE; // Hold IO until we get information from arbiter


	// Remove pending requests
	while(TRUE) 
	{
		listEntry = RemoveHeadList(&pClientInfo->PendingRequestList);
		if (listEntry == &pClientInfo->PendingRequestList) {
			break;
		}
		MsgContext = CONTAINING_RECORD(
				listEntry,
				DRIX_MSG_CONTEXT,
				Link
		);
		KDPrintM(DBG_LURN_INFO, ("Freeing pending request messages\n"));		
		ExFreePoolWithTag(MsgContext->Message, DRAID_CLIENT_REQUEST_MSG_POOL_TAG);
		ExFreePoolWithTag(MsgContext, DRAID_MSG_LINK_POOL_TAG);
	}

	// Remove lock contexts

	while(TRUE) {
		LockInUse = FALSE;
		if (IsListEmpty(&pClientInfo->LockList)) 
			break;
		for (listEntry = pClientInfo->LockList.Flink;
			listEntry != &pClientInfo->LockList;
			listEntry = listEntry->Flink) 
		{
			Lock = CONTAINING_RECORD (listEntry, DRAID_CLIENT_LOCK, Link);
			if (InterlockedCompareExchange(&Lock->InUseCount, 0, 0)) {
				LockInUse = TRUE;
				break;
			} else {
				// No one is using the lock. Free it and restart outer loop
				RemoveEntryList(&Lock->Link);
				DraidClientFreeLock(Lock);
				break;
			}
		}
		if (LockInUse) {
			KDPrintM(DBG_LURN_INFO, ("Lock %I64x:%x is in use. Waiting for freed\n", Lock->LockAddress, Lock->LockLength));
			Timeout.QuadPart = - HZ /2;
			// Wait until lock is not in use.
			RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
			KeDelayExecutionThread(KernelMode, FALSE, &Timeout);
			ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
		}
	}
	RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
	
	return STATUS_SUCCESS;
}

// Return TRUE if it is OK to terminate
BOOLEAN
DraidClientProcessTerminateRequest(
	PDRAID_CLIENT_INFO		pClientInfo
) {
	KIRQL oldIrql;
	PLIST_ENTRY listEntry;
	BOOLEAN LockInUse;
	PDRAID_CLIENT_LOCK Lock;
	NTSTATUS status;
	BOOLEAN LockAcquired;
	PDRIX_MSG_CONTEXT RequestMsgLink;
	BOOLEAN MsgPending;
	LARGE_INTEGER current_time;
	
	ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
	if (!pClientInfo->RequestToTerminate) {
		RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
		return FALSE;
	}
	KDPrintM(DBG_LURN_INFO, ("Client termination requested\n"));

	// Delay termination if any lock is acquired.
	LockInUse = FALSE;
	for (listEntry = pClientInfo->LockList.Flink;
		listEntry != &pClientInfo->LockList;
		listEntry = listEntry->Flink) 
	{
		Lock = CONTAINING_RECORD (listEntry, DRAID_CLIENT_LOCK, Link);
		if (InterlockedCompareExchange(&Lock->InUseCount, 0, 0)) {
			LockInUse = TRUE;
			break;
		}
	}
	if (LockInUse) {
		KDPrintM(DBG_LURN_INFO, ("Lock is in use. Delaying termination\n"));
		RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
		return FALSE;
	} 
	// Releasing all locks
	LockAcquired = FALSE;
	while(TRUE) {
		listEntry = RemoveHeadList(&pClientInfo->LockList);
		if (listEntry == &pClientInfo->LockList) {
			break;
		} 
		Lock = CONTAINING_RECORD (listEntry, DRAID_CLIENT_LOCK, Link);
		if (Lock->LockStatus == LOCK_STATUS_GRANTED) {
			LockAcquired = TRUE;
		}
		KDPrintM(DBG_LURN_INFO, ("Freeing lock before terminating\n"));
		DraidClientFreeLock(Lock);
	}
	if (LockAcquired) {
		RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
		KDPrintM(DBG_LURN_INFO, ("Sending release all lock message\n"));
		status = DraidClientSendRequest(pClientInfo, DRIX_CMD_RELEASE_LOCK, 0, DRIX_LOCK_ID_ALL, 0, NULL, NULL, NULL);
		return FALSE;
	} 

	// Check any pending request exists.
	MsgPending = FALSE;
	KeQueryTickCount(&current_time);
	for (listEntry = pClientInfo->PendingRequestList.Flink;
		listEntry != &pClientInfo->PendingRequestList;
		listEntry = listEntry->Flink) 
	{
		RequestMsgLink = CONTAINING_RECORD (listEntry, DRIX_MSG_CONTEXT, Link);
		if ((current_time.QuadPart - RequestMsgLink->ReqTime.QuadPart) * KeQueryTimeIncrement() < HZ * 5) {
			MsgPending = TRUE;
			KDPrintM(DBG_LURN_INFO, ("Pending request exist. Delaying termination\n"));
		} else {
			KDPrintM(DBG_LURN_INFO, ("Pending request is timed out. Ignoring.\n"));
		}
	}
	if (MsgPending) {
		RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
		return FALSE;
	}
	RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
	return TRUE;
}


void
DraidClientThreadProc(
	IN	PVOID	Context
	)
{
	NTSTATUS				status;
	ULONG					i;
	PLURELATION_NODE		Lurn;
	KIRQL					oldIrql;
	LARGE_INTEGER			TimeOut;
	PRAID_INFO				pRaidInfo;
	PDRAID_CLIENT_INFO		pClientInfo;

	UINT32					nChildCount;
	UINT32					nActiveDiskCount;
	UINT32					nSpareCount;
	UINT32					SectorsPerBit;
	PLIST_ENTRY				listEntry;
	PDRIX_MSG_CONTEXT 			MsgLink;	
	PDRAID_CLIENT_LOCK		Lock;
	BOOLEAN					NodeChanged;
	LARGE_INTEGER current_time;
	LARGE_INTEGER time_diff;
	PCCB	Ccb;
	OBJECT_ATTRIBUTES objectAttributes;
	BOOLEAN DoMore;
	UINT32 ArbiterConnectionTrialCount;
	static const UINT32 MaxArbiterConnectionTrialCount = 30; // This should be larger than LFS's secondary to primary transition time.
	static const UINT32 ConnectionRetryInterval = 5;
	BOOLEAN bRet;
	
	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);	
	KDPrintM(DBG_LURN_INFO, ("Starting client\n"));
	Lurn = (PLURELATION_NODE)Context;
	ASSERT(LURN_RAID1R == Lurn->LurnType || LURN_RAID4R == Lurn->LurnType);

	pRaidInfo = Lurn->LurnRAIDInfo;
	ASSERT(pRaidInfo);

	pClientInfo = ( PDRAID_CLIENT_INFO)pRaidInfo->pDraidClient;
	ASSERT(pClientInfo);

	SectorsPerBit = pRaidInfo->SectorsPerBit;
	ASSERT(SectorsPerBit > 0);

	pClientInfo->TotalDiskCount = nChildCount = Lurn->LurnChildrenCnt;
	pClientInfo->ActiveDiskCount = nActiveDiskCount = Lurn->LurnChildrenCnt - pRaidInfo->nSpareDisk;
	nSpareCount = pRaidInfo->nSpareDisk;
	ASSERT(nActiveDiskCount == pRaidInfo->nDiskCount);

	pClientInfo->IsReadonly = LUR_IS_READONLY(Lurn->Lur);

	RtlCopyMemory(&pClientInfo->RaidSetId, &pRaidInfo->RaidSetId, sizeof(GUID));
	RtlCopyMemory(&pClientInfo->ConfigSetId, &pRaidInfo->ConfigSetId, sizeof(GUID));	
		
	//
	// Read RMD and store to client local storage to use later.
	//
	
	status = LurnRMDRead(Lurn, &pClientInfo->Rmd, NULL);

	if(!NT_SUCCESS(status))
	{
		KDPrintM(DBG_LURN_ERROR, ("Client failed to read RMD. Exiting client thread\n"));
		ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
		pClientInfo->ClientStatus = DRAID_CLIENT_STATUS_TERMINATING;
		RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
		goto terminate;
	}

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	//
	// Create client monitor thread after reading RMD
	//
	KeInitializeEvent(&pClientInfo->MonitorThreadEvent, NotificationEvent, FALSE);

	InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

	status = PsCreateSystemThread(
		&pClientInfo->MonitorThreadHandle,
		THREAD_ALL_ACCESS,
		&objectAttributes,
		NULL,
		NULL,
		DraidMonitorThreadProc,
		Lurn
		);
	
	if(!NT_SUCCESS(status))
	{
		KDPrintM(DBG_LURN_ERROR, ("!!! PsCreateSystemThread FAIL\n"));
		ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
		pClientInfo->ClientStatus = DRAID_CLIENT_STATUS_TERMINATING;
		RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
		goto terminate;
	}

	status = ObReferenceObjectByHandle(
		pClientInfo->MonitorThreadHandle,
		GENERIC_ALL,
		NULL,
		KernelMode,
		&pClientInfo->MonitorThreadObject,
		NULL
		);

	if(!NT_SUCCESS(status))
	{
		KDPrintM(DBG_LURN_ERROR, ("!!! ObReferenceObjectByHandle FAIL\n"));
		ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
		pClientInfo->ClientStatus = DRAID_CLIENT_STATUS_TERMINATING;
		RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
		goto terminate;
	}

	ArbiterConnectionTrialCount = 0;
	DoMore = TRUE;
	while(TRUE) {
		PKEVENT				events[3];
		LONG				eventCount;
		NTSTATUS			waitStatus;

		if (DoMore) {
			DoMore = FALSE;
		} else {
			ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

			eventCount = 0;
			events[eventCount] = &pClientInfo->ClientThreadEvent;
			eventCount++;
			
			// Wait for event 
			if (pClientInfo->HasLocalArbiter) {
				//
				// Wait for ClientThreadEvent(terminate request) and Notification message
				//
				events[eventCount] = &pClientInfo->NotificationChannel.Event;
				eventCount++;
				events[eventCount] = &pClientInfo->RequestReplyChannel.Event;
				eventCount++;
			} else if (pClientInfo->ClientStatus == DRAID_CLIENT_STATUS_ARBITER_CONNECTED) {
				// Remote arbiter
				if (pClientInfo->NotificationConnection.Status == DRAID_CLIENT_CON_REGISTERED) {
					events[eventCount] = &pClientInfo->NotificationConnection.TdiReceiveContext.CompletionEvent;
					eventCount++;
				} 
				if (pClientInfo->RequestConnection.Status == DRAID_CLIENT_CON_REGISTERED) {
					events[eventCount] = &pClientInfo->RequestConnection.TdiReceiveContext.CompletionEvent;
					eventCount++;
				} 
			}

			if (pClientInfo->DRaidStatus == DRAID_CLIENT_STATUS_INITIALIZING 
				|| pClientInfo->DRaidStatus == DRIX_RAID_STATUS_REBUILDING 
				|| pClientInfo->RequestToTerminate
				|| pClientInfo->RequestForFlush) {
				// Set shorter timeout in initialization or rebuilding status to wake-up write-lock clean-up more frequently
				// 	also when termination or flush is requested
				TimeOut.QuadPart = - NANO100_PER_SEC * 2;
			} else {
				TimeOut.QuadPart = - NANO100_PER_SEC * 20; // need to wake-up with timeout to free unused write-locks.
			}

			//
			// To do: Iterate PendingRequestList to find out neareast timeout time
			//
			waitStatus = KeWaitForMultipleObjects(
					eventCount, 
					events,
					WaitAny, 
					Executive,
					KernelMode,
					TRUE,
					&TimeOut,
					NULL);
			if (waitStatus == STATUS_WAIT_0) {
				// Got thread event. 
				KDPrintM(DBG_LURN_NOISE, ("ClientThreadEvent signaled\n"));
				KeClearEvent(events[0]);
			} else if (waitStatus == STATUS_WAIT_1) {
				// Get notification
//				KDPrintM(DBG_LURN_INFO, ("NotificationChannel signaled\n"));
			} else if (waitStatus == STATUS_WAIT_2) {
//				KDPrintM(DBG_LURN_INFO, ("RequestReplyChannel signaled\n"));
			}
		}
		if (pClientInfo->ClientStatus == DRAID_CLIENT_STATUS_INITIALIZING) {
			//
			// Try to connect to arbiter
			//
			if (LUR_IS_PRIMARY(Lurn->Lur)) {
				// If arbiter is not running, run it.

				status = DraidArbiterStart(Lurn);
				if (!NT_SUCCESS(status)) {
					KDPrintM(DBG_LURN_NOISE, ("Failed to start arbiter\n"));
					goto terminate;
				}
				for(i=0;i<500;i++) {
					status = 	DraidRegisterLocalClientToArbiter(
						Lurn,
						pClientInfo
					);
					if (NT_SUCCESS(status)) {
						ArbiterConnectionTrialCount = 0;
						break;
					} else {
						// Anyway we cannot do anything without arbiter. Try until we can.
						TimeOut.QuadPart = - HZ;
						KeDelayExecutionThread(KernelMode, FALSE, &TimeOut);	
					}
				}
				if (i==500) {
					KDPrintM(DBG_LURN_NOISE, ("Failed to register client to arbiter\n"));
					goto terminate;
				}
				DoMore = TRUE;
			} else if (LUR_IS_SECONDARY(Lurn->Lur)) {
				KDPrintM(DBG_LURN_INFO, ("Calling DraidClientEstablishArbiter in secondary mode\n"));
				status = DraidClientEstablishArbiter(pClientInfo);
				if (NT_SUCCESS(status)) {
					ArbiterConnectionTrialCount = 0;
					// Status has changed continue.
					DoMore = TRUE;
				} else {
					ArbiterConnectionTrialCount++;		
					if (ArbiterConnectionTrialCount>MaxArbiterConnectionTrialCount) {
						KDPrintM(DBG_LURN_INFO, ("Failed to establish connection to arbiter after %d retrial. Failing\n", 
							ArbiterConnectionTrialCount));
						goto terminate;
					}
					KDPrintM(DBG_LURN_INFO, ("Failed to establish connection to arbiter. Waiting %d seconds, retrial count %d.\n", 
						ConnectionRetryInterval, ArbiterConnectionTrialCount));
					// Delay for a while.
					TimeOut.QuadPart = - HZ * ConnectionRetryInterval;
					KeDelayExecutionThread(KernelMode, FALSE, &TimeOut);	
					DoMore = TRUE;
				}
			}else if (pClientInfo->IsReadonly) {
				KDPrintM(DBG_LURN_INFO, ("Calling DraidClientEstablishArbiter in read-only mode\n"));
				status = DraidClientEstablishArbiter(pClientInfo);
				if (!NT_SUCCESS(status)) {
					KDPrintM(DBG_LURN_INFO, ("Arbiter is not available right now. Enter read-only RAID mode\n"));
					KeQueryTickCount(&pClientInfo->LastTriedArbiterTime);
					pClientInfo->LastTriedArbiterUsn = pClientInfo->Rmd.uiUSN;
					DraidClientRefreshReadOnlyRaidStatus(pClientInfo);
					ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
					pClientInfo->ClientStatus = DRAID_CLIENT_STATUS_NO_ARBITER;
					pClientInfo->InTransition = FALSE;
					RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);	
				}
			} else {
				ASSERT(FALSE); // should not happen.
				goto terminate;
			}		
		}

		// Check arbiter available now.
		// To do: Notify read-only host when arbiter is available. 
		if (pClientInfo->IsReadonly && pClientInfo->ClientStatus == DRAID_CLIENT_STATUS_NO_ARBITER) {
			KeQueryTickCount(&current_time);
			time_diff.QuadPart = (current_time.QuadPart - pClientInfo->LastTriedArbiterTime.QuadPart) * KeQueryTimeIncrement();
			// Check RMD is updated at every 30 seconds
			if (time_diff.QuadPart >= HZ * 30) {
				status = LurnRMDRead(pClientInfo->Lurn, &pClientInfo->Rmd, NULL);
				if(NT_SUCCESS(status))
				{
					if (pClientInfo->Rmd.uiUSN > pClientInfo->LastTriedArbiterUsn) {
						KDPrintM(DBG_LURN_INFO, ("RMD updated. Trying to connect to arbiter\n"));
						status = DraidClientEstablishArbiter(pClientInfo);	
						if (NT_SUCCESS(status)) {
							KDPrintM(DBG_LURN_INFO, ("Connected to arbiter in read-only mode\n"));	
						} else {
							KDPrintM(DBG_LURN_INFO, ("Failed to connect to arbiter in read-only mode\n"));
						}
					}
				} else {
					KDPrintM(DBG_LURN_INFO, ("Failed to read RMD\n"));
				}
				pClientInfo->LastTriedArbiterUsn = pClientInfo->Rmd.uiUSN;
				KeQueryTickCount(&pClientInfo->LastTriedArbiterTime);
			}
		}

		if (DRAID_CLIENT_STATUS_ARBITER_CONNECTED == pClientInfo->ClientStatus) {
			if (pClientInfo->HasLocalArbiter) {
				KeClearEvent(&pClientInfo->RequestReplyChannel.Event);
				// Handle any pending reply from local arbiter
				while(listEntry = ExInterlockedRemoveHeadList(
									&pClientInfo->RequestReplyChannel.Queue,
									&pClientInfo->RequestReplyChannel.Lock
									)) 
				{
					MsgLink = CONTAINING_RECORD(
							listEntry,
							DRIX_MSG_CONTEXT,
							Link
					);
					status = DraidClientHandleReplyForRequest(pClientInfo, MsgLink->Message);
					if (!NT_SUCCESS(status)) {
						KDPrintM(DBG_LURN_ERROR, ("Failed to handle repy\n"));	
						DoMore = TRUE;
					}
					ExFreePoolWithTag(MsgLink->Message, DRAID_CLIENT_REQUEST_REPLY_POOL_TAG);	
					ExFreePoolWithTag(MsgLink, DRAID_MSG_LINK_POOL_TAG);	
				}
			} else {
				status = DraidClientRecvNoWait(pClientInfo, &pClientInfo->RequestConnection);
				if (status == STATUS_SUCCESS) {
					KDPrintM(DBG_LURN_INFO, ("Received reply to request.(%x)\n", status));
					status = DraidClientHandleReplyForRequest(pClientInfo, (PDRIX_HEADER)pClientInfo->RequestConnection.ReceiveBuf);
					if (NT_SUCCESS(status)) {
						// Wait for next message if pending request exists
						if (InterlockedCompareExchange(
							&pClientInfo->RequestConnection.RxPendingCount,0,0)) {
							InterlockedDecrement(&pClientInfo->RequestConnection.RxPendingCount);
							KDPrintM(DBG_LURN_INFO, ("Start to wait for reply for pending request\n"));
							status = DraidClientRecvHeaderAsync(pClientInfo, &pClientInfo->RequestConnection);
						}
					}
				} else if (status == STATUS_PENDING) {
					// Packet is not arrived yet.
				} 
				if (!NT_SUCCESS(status)) {
					KDPrintM(DBG_LURN_ERROR, ("Failed to handle reply. Reset current context and enter initialization step.\n"));
					DraidClientResetRemoteArbiterContext(pClientInfo);
				}
			}
		}
		//
		// Handle if there is any timed out requests.
		//
		KeQueryTickCount(&current_time);
		ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
		for (listEntry = pClientInfo->PendingRequestList.Flink;
			listEntry != &pClientInfo->PendingRequestList;
			listEntry = listEntry->Flink) 
		{
			MsgLink = CONTAINING_RECORD (listEntry, DRIX_MSG_CONTEXT, Link);
			if (MsgLink->HaveTimeout &&
				(current_time.QuadPart - MsgLink->ReqTime.QuadPart > MsgLink->Timeout.QuadPart)) {
				listEntry = listEntry->Blink;
				// Timeout. Reply timeout does not mean we don't wait anymore. 
				// If reply is arrived later, it will be handle anyway, but we will call callback handler.
				KDPrintM(DBG_LURN_INFO, ("Reply timed out\n"));
				if (MsgLink->CallbackFunc) {
					RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);					
					MsgLink->CallbackFunc(pClientInfo, NULL, MsgLink->CallbackParam1);
					ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
				}
				RemoveEntryList(&MsgLink->Link);
				if (MsgLink->Message)
					ExFreePoolWithTag(MsgLink->Message, DRAID_CLIENT_REQUEST_MSG_POOL_TAG);	
				if (MsgLink)
					ExFreePoolWithTag(MsgLink, DRAID_MSG_LINK_POOL_TAG);
			} 
		}
		RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);

		//
		// Handle any pending notifications
		//
		if (DRAID_CLIENT_STATUS_ARBITER_CONNECTED == pClientInfo->ClientStatus) {
			if (pClientInfo->HasLocalArbiter) {
				KeClearEvent(&pClientInfo->NotificationChannel.Event);
				while(listEntry = ExInterlockedRemoveHeadList(
									&pClientInfo->NotificationChannel.Queue,
									&pClientInfo->NotificationChannel.Lock
									)) 
				{
					MsgLink = CONTAINING_RECORD(listEntry,	DRIX_MSG_CONTEXT,Link);
					status = DraidClientHandleNotificationMsg(pClientInfo, MsgLink->Message);
					if (!NT_SUCCESS(status)) {
						KDPrintM(DBG_LURN_ERROR, ("Failed to handle notification message\n"));
						ASSERT(FALSE);
						// This should not happen for local arbiter
					}
					DoMore = TRUE;
					ExFreePoolWithTag(MsgLink->Message, DRAID_ARBITER_NOTIFY_MSG_POOL_TAG);						
					ExFreePoolWithTag(MsgLink, DRAID_MSG_LINK_POOL_TAG);	
				}
			} else {
				status = DraidClientRecvNoWait(pClientInfo, &pClientInfo->NotificationConnection);
				if (status == STATUS_SUCCESS || status == STATUS_CONNECTION_DISCONNECTED) {
					KDPrintM(DBG_LURN_ERROR, ("Received through notification connection.\n"));
					status = DraidClientHandleNotificationMsg(pClientInfo, (PDRIX_HEADER)pClientInfo->NotificationConnection.ReceiveBuf);
				} else if (status == STATUS_PENDING) {
					// Packet is not arrived yet.
				} 
				if (!NT_SUCCESS(status)) {
					KDPrintM(DBG_LURN_ERROR, ("Receive through notification connection failed. Reset current context and enter initialization step.\n"));
					DraidClientResetRemoteArbiterContext(pClientInfo);
				}
				if (status == STATUS_SUCCESS) {
					// Wait for more notification message
					status = DraidClientRecvHeaderAsync(pClientInfo, &pClientInfo->NotificationConnection);
					if (!NT_SUCCESS(status)) {
						KDPrintM(DBG_LURN_ERROR, ("Failed to wait for more notification message.\n"));
						DraidClientResetRemoteArbiterContext(pClientInfo);						
					}
				}
			} 
		}
		
		NodeChanged = FALSE;
		//
		// Check local node changes and send state update request.
		//
		for(i=0;i<nChildCount;i++) {
			ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
			if (!(pClientInfo->NodeChanged[i] & DRAID_NODE_CHANGE_FLAG_UPDATING) &&
				(pClientInfo->NodeChanged[i] & DRAID_NODE_CHANGE_FLAG_CHANGED)) {
				NodeChanged = TRUE;
				pClientInfo->NodeChanged[i] &= ~DRAID_NODE_CHANGE_FLAG_CHANGED;
				pClientInfo->NodeChanged[i] |= DRAID_NODE_CHANGE_FLAG_UPDATING;
				KDPrintM(DBG_LURN_INFO, ("Node %d has changed\n", i));
			}
			RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
		}

		if (NodeChanged) {
			if (DRAID_CLIENT_STATUS_ARBITER_CONNECTED == pClientInfo->ClientStatus) {
				status = DraidClientSendRequest(pClientInfo, DRIX_CMD_NODE_CHANGE, 0, 0,0, NULL, NULL, NULL);
				if (!NT_SUCCESS(status)) {
					DraidClientResetRemoteArbiterContext(pClientInfo);
				}
				DoMore = TRUE;
			} else if (DRAID_CLIENT_STATUS_NO_ARBITER == pClientInfo->ClientStatus) {
				ASSERT(pClientInfo->IsReadonly == TRUE);
				ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
				for(i=0;i<pClientInfo->Lurn->LurnChildrenCnt;i++) {
					pClientInfo->NodeChanged[i] &= ~DRAID_NODE_CHANGE_FLAG_UPDATING;
				}
				RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
				DraidClientRefreshReadOnlyRaidStatus(pClientInfo);
				DoMore = TRUE;
			}
		}

		//
		// Handle IO request
		//

		while(listEntry = ExInterlockedRemoveHeadList(
							&pClientInfo->CcbQueue,
							&pClientInfo->SpinLock
							)) 
		{
			Ccb = CONTAINING_RECORD(listEntry, CCB, ListEntry);
			status = DraidClientDispatchCcb(pClientInfo, Ccb);
			if(!NT_SUCCESS(status)) {
				KDPrintM(DBG_LURN_ERROR, ("DraidClientDispatchCcb failed. NTSTATUS:%08lx\n", status));
				goto terminate;
			}
			DoMore = TRUE;
		}

		// Handle flush request
		ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
		if (pClientInfo->RequestForFlush) {
			BOOLEAN LockInUse;
			//
			// Check any lock is in-use.
			//
			LockInUse = FALSE;
			for (listEntry = pClientInfo->LockList.Flink;
				listEntry != &pClientInfo->LockList;
				listEntry = listEntry->Flink) 
			{
				Lock = CONTAINING_RECORD (listEntry, DRAID_CLIENT_LOCK, Link);
				if (InterlockedCompareExchange(&Lock->InUseCount, 0, 0)) {
					LockInUse = TRUE;
					break;
				}
			}
			if (LockInUse) {
				KDPrintM(DBG_LURN_INFO, ("Lock is in use. Try to flush later\n"));
				RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
			} else {
				BOOLEAN Flushed = TRUE;
				// Release all locks including GRANTED and PENDING
				while(TRUE) {
					listEntry = RemoveHeadList(&pClientInfo->LockList);
					if (listEntry == &pClientInfo->LockList) {
						break;
					} 
					Lock = CONTAINING_RECORD (listEntry, DRAID_CLIENT_LOCK, Link);
					DraidClientFreeLock(Lock);
					Flushed = FALSE;
				}
				if (Flushed) {
					KDPrintM(DBG_LURN_INFO, ("Flushing completed\n"));
					RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
					// Flushing completed
					pClientInfo->FlushCompletionRoutine(pClientInfo->FlushCcb);
					ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);

					pClientInfo->FlushCompletionRoutine = NULL;
					pClientInfo->FlushCcb = NULL;
					pClientInfo->RequestForFlush = FALSE;					
					RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
				} else {
					RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
					KDPrintM(DBG_LURN_INFO, ("Sending release all lock message to flush\n"));
					status = DraidClientSendRequest(pClientInfo, DRIX_CMD_RELEASE_LOCK, 0, DRIX_LOCK_ID_ALL, 0, NULL, NULL, NULL);
				} 
			}
		}else {
			RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
		}
		//
		// Check any lock is not used for a while or has been requested to yield.
		//
		if (DRAID_CLIENT_STATUS_ARBITER_CONNECTED == pClientInfo->ClientStatus) {		
			ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
			KeQueryTickCount(&current_time);
			for (listEntry = pClientInfo->LockList.Flink;
				listEntry != &pClientInfo->LockList;
				listEntry = listEntry->Flink) 
			{
				Lock = CONTAINING_RECORD (listEntry, DRAID_CLIENT_LOCK, Link);
				if (InterlockedCompareExchange(&Lock->InUseCount, 0, 0))
					continue;
				if (Lock->LockStatus != LOCK_STATUS_GRANTED) {
					continue;
				}
				time_diff.QuadPart = (current_time.QuadPart - Lock->LastAccessedTime.QuadPart) * KeQueryTimeIncrement();

				if (Lock->YieldRequested ||
					time_diff.QuadPart >= HZ * DRIX_IO_LOCK_CLEANUP_TIMEOUT ||
					((pClientInfo->DRaidStatus == DRIX_RAID_STATUS_REBUILDING || Lock->Contented)&&
					time_diff.QuadPart >= HZ * DRIX_IO_LOCK_CLEANUP_TIMEOUT/10)) // Release lock faster if in rebuilding mode or lock is contented
				{
					KDPrintM(DBG_LURN_INFO, ("Releasing locked region(Id=%I64x, Range=%I64x:%x)\n",
						Lock->LockId, Lock->LockAddress, Lock->LockLength));
					//
					// Remove from acquired lock list and send message
					//
					listEntry = listEntry->Blink;
					RemoveEntryList(&Lock->Link);
					RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
					status = DraidClientSendRequest(pClientInfo, DRIX_CMD_RELEASE_LOCK, 0, Lock->LockId, 0, NULL, NULL, NULL);
					DraidClientFreeLock(Lock);
					if (!NT_SUCCESS(status) && !pClientInfo->HasLocalArbiter) {
						DraidClientResetRemoteArbiterContext(pClientInfo);
					} 
					ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
					DoMore = TRUE;
					break; // Lock is removed and need to reiterate locklist
				}
			}
			RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
		}		
		//
		// Check termination request.
		//
		bRet = DraidClientProcessTerminateRequest(pClientInfo);
		if (bRet) {
			break;
		}
	}
terminate:
	ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
	pClientInfo->ClientStatus = DRAID_CLIENT_STATUS_TERMINATING;
	pClientInfo->DRaidStatus = DRIX_RAID_STATUS_TERMINATED;
	RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);	
	
	if (pClientInfo->HasLocalArbiter)
		DraidUnregisterLocalClient(Lurn);

	DraidClientDisconnectFromArbiter(pClientInfo, &pClientInfo->NotificationConnection);
	DraidClientDisconnectFromArbiter(pClientInfo, &pClientInfo->RequestConnection);
	 
	ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
	while(pClientInfo->RequestToTerminate == FALSE) {
		RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);	
		// ClientThread terminating by itself. Wait for stop event.
		KDPrintM(DBG_LURN_INFO, ("Client thread terminated by itself. Wait for terminate request before exiting\n"));		
		status = KeWaitForSingleObject(
			&pClientInfo->ClientThreadEvent, Executive, KernelMode,	FALSE, NULL);
		ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
	}
	RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
	
	KDPrintM(DBG_LURN_INFO, ("Exiting\n"));
	PsTerminateSystemThread(STATUS_SUCCESS);
	return;
}

//
// Initialize local RAID information
//
NTSTATUS 
DraidClientStart(
	PLURELATION_NODE	Lurn
	)
{
	PRAID_INFO pRaidInfo = Lurn->LurnRAIDInfo;
	NTSTATUS ntStatus;
	PDRAID_CLIENT_INFO pClientInfo;
	UINT32 i;
	
	OBJECT_ATTRIBUTES objectAttributes;

	ASSERT(pRaidInfo->pDraidClient == NULL); // Multiple arbiter is not possible.

	//
	// Allocate client and set initial value.
	//
	pRaidInfo->pDraidClient = ExAllocatePoolWithTag(NonPagedPool, sizeof(DRAID_CLIENT_INFO),
		DRAID_CLIENT_INFO_POOL_TAG);
	if (NULL ==  pRaidInfo->pDraidClient) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}
		
	pClientInfo = pRaidInfo->pDraidClient;

	RtlZeroMemory(pClientInfo, sizeof(DRAID_CLIENT_INFO));

	KeInitializeSpinLock(&pClientInfo->SpinLock);
	pClientInfo->ClientStatus = DRAID_CLIENT_STATUS_INITIALIZING;
	pClientInfo->InTransition = TRUE; // Hold IO until we get information from arbiter
	pClientInfo->DRaidStatus = DRIX_RAID_STATUS_INITIALIZING;
	
	for(i=0;i<Lurn->LurnChildrenCnt;i++) {
		pClientInfo->NodeFlagsLocal[i] = DraidClientLurnStatusToNodeFlag(Lurn->LurnChildren[i]->LurnStatus);
		pClientInfo->NodeFlagsRemote[i] = DRIX_NODE_FLAG_UNKNOWN;
		pClientInfo->RoleToNodeMap[i] = (UCHAR) -1;	// not valid until LurnFlag is not unknown.
	}

	pClientInfo->Lurn = Lurn;
	InitializeListHead(&pClientInfo->LockList);
	InitializeListHead(&pClientInfo->CcbQueue);
	InitializeListHead(&pClientInfo->PendingRequestList);
	
#if 0
	InitializeListHead(&pClientInfo->NotificationQueue);
	InitializeListHead(&pClientInfo->SentRequestQueue);
#endif
	KeInitializeSpinLock(&pClientInfo->RequestConnection.SpinLock);
	KeInitializeEvent(&pClientInfo->RequestConnection.TdiReceiveContext.CompletionEvent, NotificationEvent, FALSE) ;
	pClientInfo->RequestConnection.Timeout.QuadPart = 5*HZ;

	KeInitializeSpinLock(&pClientInfo->NotificationConnection.SpinLock);
	KeInitializeEvent(&pClientInfo->NotificationConnection.TdiReceiveContext.CompletionEvent, NotificationEvent, FALSE) ;
	pClientInfo->NotificationConnection.Timeout.QuadPart = 0;

	DRIX_INITIALIZE_LOCAL_CHANNEL(&pClientInfo->NotificationChannel);
	DRIX_INITIALIZE_LOCAL_CHANNEL(&pClientInfo->RequestReplyChannel);

	//
	// Create client thread
	//
	KeInitializeEvent(&pClientInfo->ClientThreadEvent, NotificationEvent, FALSE);

	InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

	ntStatus = PsCreateSystemThread(
		&pClientInfo->ClientThreadHandle,
		THREAD_ALL_ACCESS,
		&objectAttributes,
		NULL,
		NULL,
		DraidClientThreadProc,
		Lurn
		);
	
	if(!NT_SUCCESS(ntStatus))
	{
		KDPrintM(DBG_LURN_ERROR, ("!!! PsCreateSystemThread FAIL\n"));
		return STATUS_THREAD_NOT_IN_PROCESS;
	}

	ntStatus = ObReferenceObjectByHandle(
		pClientInfo->ClientThreadHandle,
		GENERIC_ALL,
		NULL,
		KernelMode,
		&pClientInfo->ClientThreadObject,
		NULL
		);

	if(!NT_SUCCESS(ntStatus))
	{
		KDPrintM(DBG_LURN_ERROR, ("!!! ObReferenceObjectByHandle FAIL\n"));
		return STATUS_THREAD_NOT_IN_PROCESS;
	}

	return STATUS_SUCCESS;
}

NTSTATUS
DraidClientStop(
	PLURELATION_NODE	Lurn
	)
{
	LARGE_INTEGER TimeOut;
	KIRQL oldIrql;
	
	PRAID_INFO pRaidInfo = Lurn->LurnRAIDInfo;
	NTSTATUS status;
	PLIST_ENTRY				listEntry;
	PDRAID_CLIENT_LOCK		Lock;
	PDRIX_MSG_CONTEXT	MsgEntry;
	PDRAID_CLIENT_INFO pClientInfo = (PDRAID_CLIENT_INFO)pRaidInfo->pDraidClient;
	PDRIX_MSG_CONTEXT MsgContext;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);
	
	ACQUIRE_SPIN_LOCK(&pRaidInfo->SpinLock, &oldIrql);
	if (!pRaidInfo->pDraidClient) {
		// Already stopped.
		RELEASE_SPIN_LOCK(&pRaidInfo->SpinLock, oldIrql);	
		return STATUS_SUCCESS;
	}
	pRaidInfo->pDraidClient  = NULL;
	RELEASE_SPIN_LOCK(&pRaidInfo->SpinLock, oldIrql);	

	ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);

	pClientInfo->RequestToTerminate = TRUE;
	
	if(pClientInfo->ClientThreadHandle)
	{
		KDPrintM(DBG_LURN_INFO, ("KeSetEvent\n"));
		KeSetEvent(&pClientInfo->ClientThreadEvent,IO_NO_INCREMENT, FALSE);
	}
	if (pClientInfo->MonitorThreadHandle) {
		KeSetEvent(&pClientInfo->MonitorThreadEvent,IO_NO_INCREMENT, FALSE);
	}
	
	RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);
	if (pClientInfo->MonitorThreadHandle) {	
		TimeOut.QuadPart = - NANO100_PER_SEC * 120;
		status = KeWaitForSingleObject(
			pClientInfo->MonitorThreadObject,
			Executive,
			KernelMode,
			FALSE,
			&TimeOut
			);

		ASSERT(status == STATUS_SUCCESS);
		ObDereferenceObject(pClientInfo->MonitorThreadObject);

		ZwClose(pClientInfo->MonitorThreadHandle);		
	}

	if(pClientInfo->ClientThreadHandle)
	{

		TimeOut.QuadPart = - NANO100_PER_SEC * 120;
		status = KeWaitForSingleObject(
			pClientInfo->ClientThreadObject,
			Executive,
			KernelMode,
			FALSE,
			&TimeOut
			);

		ASSERT(status == STATUS_SUCCESS); // should not timeout 

		ObDereferenceObject(pClientInfo->ClientThreadObject);

		ZwClose(pClientInfo->ClientThreadHandle);

	}

	ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
	pClientInfo->ClientThreadObject = NULL;
	pClientInfo->ClientThreadHandle = NULL;
	pClientInfo->MonitorThreadObject = NULL;
	pClientInfo->MonitorThreadHandle = NULL;
	RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);

	ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
	while(TRUE)
	{
		listEntry = RemoveHeadList(&pClientInfo->LockList);
		if (listEntry == &pClientInfo->LockList) {
			break;
		}
		KDPrintM(DBG_LURN_INFO, ("Freeing lock\n"));
		Lock = CONTAINING_RECORD(
				listEntry,
				DRAID_CLIENT_LOCK,
				Link
		);
		ASSERT(InterlockedCompareExchange(&Lock->InUseCount, 0, 0) == 0);
		ExFreePoolWithTag(Lock, DRAID_CLIENT_LOCK_POOL_TAG);
	}
	while(TRUE) 
	{
		listEntry = RemoveHeadList(&pClientInfo->PendingRequestList);
		if (listEntry == &pClientInfo->PendingRequestList) {
			break;
		}
		MsgContext = CONTAINING_RECORD(
				listEntry,
				DRIX_MSG_CONTEXT,
				Link
		);
		KDPrintM(DBG_LURN_INFO, ("Freeing pending request messages\n"));		
		ExFreePoolWithTag(MsgContext->Message, DRAID_CLIENT_REQUEST_MSG_POOL_TAG);
		ExFreePoolWithTag(MsgContext, DRAID_MSG_LINK_POOL_TAG);
	}

	while(TRUE)
	{
		listEntry = RemoveHeadList(&pClientInfo->NotificationChannel.Queue);
		if (listEntry == &pClientInfo->NotificationChannel.Queue) {
			break;
		}
		MsgEntry = CONTAINING_RECORD(
				listEntry,
				DRIX_MSG_CONTEXT,
				Link
		);
		
		KDPrintM(DBG_LURN_INFO, ("Freeing unhandled notification message\n"));
		ExFreePoolWithTag(MsgEntry->Message, DRAID_ARBITER_NOTIFY_MSG_POOL_TAG);
		ExFreePoolWithTag(MsgEntry, DRAID_MSG_LINK_POOL_TAG);
	}

	while(TRUE)
	{
		listEntry = RemoveHeadList(&pClientInfo->RequestReplyChannel.Queue);
		if (listEntry == &pClientInfo->RequestReplyChannel.Queue) {
			break;
		}
		MsgEntry = CONTAINING_RECORD(
				listEntry,
				DRIX_MSG_CONTEXT,
				Link
		);
		
		KDPrintM(DBG_LURN_INFO, ("Freeing unhandled request-reply message\n"));
		ExFreePoolWithTag(MsgEntry->Message, DRAID_CLIENT_REQUEST_REPLY_POOL_TAG);
		ExFreePoolWithTag(MsgEntry, DRAID_MSG_LINK_POOL_TAG);
	}
	
	RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
	
	ExFreePoolWithTag(pClientInfo, DRAID_CLIENT_INFO_POOL_TAG);
	KDPrintM(DBG_LURN_INFO, ("Client stopped completely\n"));

	return status;
}


PDRAID_CLIENT_LOCK
DraidClientAllocLock(
	UINT64		LockId,
	UCHAR 		LockType,
	UCHAR 		LockMode,
	UINT64		Addr,
	UINT32		Length
) {
	PDRAID_CLIENT_LOCK Lock;

	Lock = ExAllocatePoolWithTag(NonPagedPool, sizeof(DRAID_CLIENT_LOCK), DRAID_CLIENT_LOCK_POOL_TAG);
	if (Lock == NULL)
		return NULL;
	RtlZeroMemory(Lock, sizeof(DRAID_CLIENT_LOCK));
	Lock->LockType = LockType;
	Lock->LockMode= LockMode;
	Lock->LockAddress = Addr;
	Lock->LockLength = Length;
	Lock->LockId = LockId;

	InitializeListHead(&Lock->Link);

	Lock->LockStatus = LOCK_STATUS_NONE;
	
	return Lock;
}


VOID
DraidClientFreeLock(
	PDRAID_CLIENT_LOCK Lock
){
	ExFreePoolWithTag(Lock, DRAID_CLIENT_LOCK_POOL_TAG);
}


//
// Get check IO permission for given address range.
// If we don't have permission, return required addr
// Return if we have all permission.
//
//
// Return SUCCESS if given range is covered.
// 		UNSUCCESSFUL if not covered. UnauthAddr and UnauthLength will be filled.
//		PENDING if range is covered, but it is pending status.
//
NTSTATUS
DraidClientCheckIoPermission(
	IN PDRAID_CLIENT_INFO pClientInfo,
	IN UINT64 Addr,
	IN UINT32 Length,
	OUT UINT64* UnauthAddr,
	OUT UINT32* UnauthLength
) {
	PLIST_ENTRY listEntry;
	PDRAID_CLIENT_LOCK Lock;
	UINT32 OverlapStatus;
	UINT64 OverlapStartAddr, OverlapEndAddr;
	PDRAID_CLIENT_LOCK FullOverlap;
	NTSTATUS status;
	ULONG BitmapBuf[64]; // Cover up to length 1Mbytes.
	PULONG BitmapBufPtr = BitmapBuf;
	ULONG ByteCount;
	RTL_BITMAP Bitmap;
	KIRQL oldIrql;
	BOOLEAN LockIsPended;
	
	ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
	// Pass1. Brief search: Check most common case - full match only.
	FullOverlap = NULL;
	for (listEntry = pClientInfo->LockList.Flink;
		listEntry != &pClientInfo->LockList;
		listEntry = listEntry->Flink) 
	{
		Lock = CONTAINING_RECORD (listEntry, DRAID_CLIENT_LOCK, Link);
		OverlapStatus = DraidGetOverlappedRange(
			Addr, Length,
			Lock->LockAddress, Lock->LockLength,
			&OverlapStartAddr, &OverlapEndAddr
		);
		if (OverlapStatus == DRAID_RANGE_SRC2_CONTAINS_SRC1) {
			FullOverlap = Lock;
			break;
		}
	}
	if (FullOverlap) {
		KDPrintM(DBG_LURN_NOISE, ("Lock full matched to LockId %I64x\n", FullOverlap->LockId));
		// Full matched lock entry found.
		if (FullOverlap->LockStatus == LOCK_STATUS_GRANTED) {
			InterlockedIncrement(&FullOverlap->InUseCount);
			KeQueryTickCount(&FullOverlap->LastAccessedTime);
			status = STATUS_SUCCESS;
		} else {
			// Lock is not yet granted. But in progress
			status = STATUS_PENDING;
		}
		RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
		return status;
	}
	RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
	
	// No full match. Need to add up partial matched.
	// Pass2. Bitmap search.
	ByteCount = ((Length+31)/32)*8;
	if (Length > sizeof(BitmapBuf) * 8) {
		BitmapBufPtr = ExAllocatePoolWithTag(NonPagedPool, ByteCount , DRAID_LOCKED_RANGE_BITMAP_POOL_TAG);
	} else {
		BitmapBufPtr = BitmapBuf;
	}

	RtlInitializeBitMap(&Bitmap, BitmapBufPtr,ByteCount * 32);
	RtlClearAllBits(&Bitmap);
	ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
	LockIsPended = FALSE;

	// To do: iterate locks and set matching bits
	// 	if matching lock is pended, set LockIsPended TRUE
	for (listEntry = pClientInfo->LockList.Flink;
		listEntry != &pClientInfo->LockList;
		listEntry = listEntry->Flink) 
	{
		Lock = CONTAINING_RECORD (listEntry, DRAID_CLIENT_LOCK, Link);
		OverlapStatus = DraidGetOverlappedRange(
			Addr, Length,
			Lock->LockAddress, Lock->LockLength,
			&OverlapStartAddr, &OverlapEndAddr
		);
		if (OverlapStatus != DRAID_RANGE_NO_OVERLAP) {
			// Convert overlapping address to bit.
			ULONG StartIndex;
			ULONG NumberToSet;
			StartIndex = (ULONG)(OverlapStartAddr-Addr);
			NumberToSet =(ULONG)(OverlapEndAddr-OverlapStartAddr+1);
			RtlSetBits(&Bitmap, StartIndex, NumberToSet);
			if (Lock->LockStatus == LOCK_STATUS_PENDING) {
				LockIsPended = TRUE;	
			}
		}
	}	
	if (RtlAreBitsSet(&Bitmap, 0, Length)) {
		//
		// All bit is set. Lock is covered
		// 
		if (LockIsPended == TRUE) {
			// But one of the lock is in pending.
			KDPrintM(DBG_LURN_INFO, ("Range %I64x:%x is covered. Partial pending\n", Addr, Length));
			status = STATUS_PENDING;
		} else {
			KDPrintM(DBG_LURN_INFO, ("Range %I64x:%x is covered and all range is granted\n", 
				Addr, Length
			));
			status = STATUS_SUCCESS;
			// Increase usage count.
			for (listEntry = pClientInfo->LockList.Flink;
				listEntry != &pClientInfo->LockList;
				listEntry = listEntry->Flink) 
			{
				Lock = CONTAINING_RECORD (listEntry, DRAID_CLIENT_LOCK, Link);
				OverlapStatus = DraidGetOverlappedRange(
					Addr, Length,
					Lock->LockAddress, Lock->LockLength,
					&OverlapStartAddr, &OverlapEndAddr
				);
				if (OverlapStatus != DRAID_RANGE_NO_OVERLAP) {
					InterlockedIncrement(&Lock->InUseCount);
					KeQueryTickCount(&Lock->LastAccessedTime);
					KDPrintM(DBG_LURN_TRACE, ("  Partial match with lock %I64x:%x\n",
						Lock->LockAddress, Lock->LockLength
					));
				}
			}
		}
	} else {
		ULONG StartIndex = 0;
		ULONG NumberOfClear;
		// Find clear bits
		NumberOfClear = RtlFindFirstRunClear(&Bitmap, &StartIndex);
		ASSERT(NumberOfClear);
		// Currently return only one range. If multiple range is missing, multiple lock_acquire need to be sent.
		// 	to do: return all unlocked range.
		*UnauthAddr = StartIndex + Addr;
		*UnauthLength = NumberOfClear;
		KDPrintM(DBG_LURN_INFO, ("Lock range %I64x:%x is not covered.\n", *UnauthAddr, *UnauthLength));
		status = STATUS_UNSUCCESSFUL;
	}
	RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
	if (BitmapBufPtr != BitmapBuf) {
		ExFreePoolWithTag(BitmapBufPtr, DRAID_LOCKED_RANGE_BITMAP_POOL_TAG);
	}
	return status;
#if 0
	for (listEntry = pClientInfo->LockList.Flink;
		listEntry != &pClientInfo->LockList;
		listEntry = listEntry->Flink) 
	{
		Lock = CONTAINING_RECORD (listEntry, DRAID_CLIENT_LOCK, Link);
		OverlapStatus = DraidGetOverlappedRange(
			Addr, Length,
			Lock->LockAddress, Lock->LockLength,
			&OverlapStartAddr, &OverlapEndAddr
		);
		if (OverlapStatus == DRAID_RANGE_NO_OVERLAP) {
			// 
		} else if (OverlapStatus == DRAID_RANGE_SRC1_HEAD_OVERLAP) {
			ASSERT(HeadOverlap==NULL);
			HeadOverlap = Lock;
		} else if (OverlapStatus == DRAID_RANGE_SRC1_TAIL_OVERLAP) {
			ASSERT(TailOverlap==NULL);
			TailOverlap = Lock;
		} else if (OverlapStatus == DRAID_RANGE_SRC1_CONTAINS_SRC2) {
		
			// This should not happen --
			ASSERT(FALSE);
		} else if (OverlapStatus == DRAID_RANGE_SRC2_CONTAINS_SRC1) {
			ASSERT(FullOverlap == NULL);
			FullOverlap = Lock;
		}
	}
	if (FullOverlap) {
		KDPrintM(DBG_LURN_NOISE, ("Lock full matched to LockId %I64x\n", FullOverlap->LockId));

		ASSERT(HeadOverlap == NULL);
		ASSERT(TailOverlap == NULL);
		// Full matched lock entry found.
		if (FullOverlap->LockStatus == LOCK_STATUS_GRANTED) {
			if (UpdateUseCount) {
				InterlockedIncrement(&FullOverlap->InUseCount);
				KeQueryTickCount(&FullOverlap->LastAccessedTime);
			}
			status = STATUS_SUCCESS;
			LockCovered = TRUE;
		} else {
			// Lock is not yet granted. But in progress
			status = STATUS_PENDING;
			LockCovered = TRUE;
		}
	} else if (HeadOverlap && TailOverlap == NULL) {
		KDPrintM(DBG_LURN_INFO, ("Lock head matched to LockId %I64x\n", HeadOverlap->LockId));
		// Partial match. Request lock for tail part
		DraidGetOverlappedRange(
			Addr, Length,
			HeadOverlap->LockAddress, HeadOverlap->LockLength,
			&OverlapStartAddr, &OverlapEndAddr
		);
		LockCovered = FALSE;
		*UnauthAddr = OverlapEndAddr+1;
		*UnauthLength = (UINT32)(Addr + Length - OverlapEndAddr);
	} else if (TailOverlap && HeadOverlap == NULL) {
		KDPrintM(DBG_LURN_INFO, ("Lock tail matched to LockId %I64x\n", TailOverlap->LockId));	
		// Partial match. Request lock for head part
		DraidGetOverlappedRange(
			Addr, Length,
			TailOverlap->LockAddress, TailOverlap->LockLength,
			&OverlapStartAddr, &OverlapEndAddr
		);
		LockCovered = FALSE;
		*UnauthAddr = Addr;
		*UnauthLength = (UINT32)(OverlapStartAddr - Addr);
	} else if (HeadOverlap && TailOverlap) {
		KDPrintM(DBG_LURN_NOISE, ("Lock partial matched to LockId %I64x and %I64x\n", 
			HeadOverlap->LockId, TailOverlap->LockId));	
		// Partial match. Assume all region is covered. -- this assumption is not right, now...
		if (HeadOverlap->LockStatus == LOCK_STATUS_GRANTED && TailOverlap->LockStatus == LOCK_STATUS_GRANTED) {
			if (UpdateUseCount) {			
				InterlockedIncrement(&HeadOverlap->InUseCount);
				InterlockedIncrement(&TailOverlap->InUseCount);
				KeQueryTickCount(&HeadOverlap->LastAccessedTime);	
				KeQueryTickCount(&TailOverlap->LastAccessedTime);
			}
			status = STATUS_SUCCESS;
			LockCovered = TRUE;
		} else {
			LockCovered = TRUE;
			status = STATUS_PENDING;
		}
	} else {
		KDPrintM(DBG_LURN_INFO, ("Lock no match.\n"));
		LockCovered = FALSE;
		*UnauthAddr = Addr;
		*UnauthLength = Length;
	}

	if (LockCovered)  {
		return status; // PENDING or SUCCESS
	} else {
		return STATUS_UNSUCCESSFUL;
	}
#endif	
}


//
// Called when acquire lock has been replied or timeout.
//
VOID DraidClientAcquireLockCallback(PDRAID_CLIENT_INFO Client, PDRIX_HEADER Reply, PVOID Param1)
{
	NTSTATUS status;
	PCCB Ccb;
	UINT64 Addr;
	UINT32 Length;
	UINT64 AddrToLock;		//	Address to lock 
	UINT32 LengthToLock;	//

	Ccb = (PCCB) Param1;
	ASSERT(Ccb);
	
	LSCcbGetAddressAndLength((PCDB)&Ccb->Cdb[0], &Addr, &Length);
	
	if (Reply) {
		// Check again whether lock is granted.
		status = DraidClientCheckIoPermission(Client, Addr, Length, &AddrToLock, &LengthToLock);
		if (status == STATUS_SUCCESS) {
			status = DraidClientSendCcbToNondefectiveChildren(Client, Ccb, LurnRAID1RCcbCompletion);
		} else {
			// Lock is not granted
			KDPrintM(DBG_LURN_INFO, ("Lock is not granted. Returning ccb busy\n"));
			Ccb->CcbStatus = CCB_STATUS_BUSY;
			LSAssocSetRedundentRaidStatusFlag(Client->Lurn, Ccb);
			LSCcbCompleteCcb(Ccb);
		}
	} else {
		// Timeout
		KDPrintM(DBG_LURN_INFO, ("Lock is not granted. Returning ccb busy\n"));
		Ccb->CcbStatus = CCB_STATUS_BUSY;
		LSAssocSetRedundentRaidStatusFlag(Client->Lurn, Ccb);
		LSCcbCompleteCcb(Ccb);		
	}
}


//
// IO system want to do some IO. DRAID client checks it has lock for the region
//
NTSTATUS
DraidClientIoWithLock(
	PDRAID_CLIENT_INFO pClientInfo,
	UCHAR LockMode, // DRIX_LOCK_MODE_
	PCCB	Ccb
) 
{
	PLURELATION_NODE		Lurn = pClientInfo->Lurn;
	PRAID_INFO pRaidInfo = Lurn->LurnRAIDInfo;
	NTSTATUS status;
	PDRAID_CLIENT_LOCK NewLock = NULL;
	UINT64 Addr;
	UINT32 Length;
	UINT64 AddrToLock;		//	Address to lock 
	UINT32 LengthToLock;	//
	
	LSCcbGetAddressAndLength((PCDB)&Ccb->Cdb[0], &Addr, &Length);
	
	if (LUR_IS_SECONDARY(Lurn->Lur)) {
		KDPrintM(DBG_LURN_TRACE,  ("Secondary writes directly: %I64x:%x\n", Addr, Length));
	}

	status = DraidClientCheckIoPermission(pClientInfo, Addr, Length, &AddrToLock, &LengthToLock);
	
	if (status == STATUS_PENDING) {
		// Lock request has sent but not granted yet. 
		// Just return pending status.
	} else if (status == STATUS_SUCCESS) {
		// Lock is covered. Handle Ccb here.
		status = DraidClientSendCcbToNondefectiveChildren(pClientInfo, Ccb, LurnRAID1RCcbCompletion);
	} else {
		LARGE_INTEGER Timeout;
		Timeout.QuadPart = HZ / 2; 
		KDPrintM(DBG_LURN_INFO, ("Send lock request for range %I64x:%x\n", AddrToLock, LengthToLock));
		// Lock is not covered. Request lock with callback. 
		// Return busy if lock is acquired in 500ms.
		status = DraidClientSendRequest(pClientInfo, DRIX_CMD_ACQUIRE_LOCK, 
			(DRIX_LOCK_TYPE_BLOCK_IO<<8) | LockMode, AddrToLock, LengthToLock, &Timeout, 
			DraidClientAcquireLockCallback, (PVOID)Ccb);
		if (!NT_SUCCESS(status)) {
			DraidClientResetRemoteArbiterContext(pClientInfo);
		}
	}
	return status;
}

NTSTATUS
DraidReleaseBlockIoPermissionToClient(
	PDRAID_CLIENT_INFO pClientInfo,
	PCCB				Ccb
)
{
	PLURELATION_NODE		Lurn = pClientInfo->Lurn;
	PRAID_INFO pRaidInfo = Lurn->LurnRAIDInfo;
	NTSTATUS status = STATUS_SUCCESS;
	KIRQL oldIrql;
	PLIST_ENTRY listEntry;
	PDRAID_CLIENT_LOCK Lock;
	UINT64 Addr;
	UINT32 Length;
	UINT32 OverlapStatus;
	UINT64 OverlapStartAddr, OverlapEndAddr;

	LSCcbGetAddressAndLength((PCDB)&Ccb->Cdb[0], &Addr, &Length);
		
	ACQUIRE_SPIN_LOCK(&pRaidInfo->SpinLock, &oldIrql);
	pClientInfo = (PDRAID_CLIENT_INFO)pRaidInfo->pDraidClient;
	if (pClientInfo == NULL) { // Client is already stopped or ready to be stopped.
		RELEASE_SPIN_LOCK(&pRaidInfo->SpinLock, oldIrql);
		return STATUS_UNSUCCESSFUL;
	}

	ACQUIRE_DPC_SPIN_LOCK(&pClientInfo->SpinLock);

	//
	// to do: save used lock info to Ccb instead of searching again.
	//
	for (listEntry = pClientInfo->LockList.Flink;
		listEntry != &pClientInfo->LockList;
		listEntry = listEntry->Flink) 
	{
		Lock = CONTAINING_RECORD (listEntry, DRAID_CLIENT_LOCK, Link);
		OverlapStatus = DraidGetOverlappedRange(
			Addr, Length,
			Lock->LockAddress, Lock->LockLength,
			&OverlapStartAddr, &OverlapEndAddr
		);
		if (OverlapStatus == DRAID_RANGE_NO_OVERLAP) {
			// 
		} else {
			InterlockedDecrement(&Lock->InUseCount);
			ASSERT(Lock->InUseCount>=0);
			KeQueryTickCount(&Lock->LastAccessedTime);
			if (Lock->YieldRequested) {
				// Wakeup client thread to make it to send release message.
				KeSetEvent(&pClientInfo->ClientThreadEvent,IO_NO_INCREMENT, FALSE);
			}
		}
	}

	RELEASE_DPC_SPIN_LOCK(&pClientInfo->SpinLock);
	RELEASE_SPIN_LOCK(&pRaidInfo->SpinLock, oldIrql);
	return status;
}

////////////////////////////////////////////////
//
// Draid Client Monitor thread
//
// Job of monitor thread
//   1. Revive stopped node. Test revived disk has not changed by checking RMD
//   2. Run smart to check disk status every hour - not yet implemented.
//
// Monitor thread have same life span of draid client thread
//
// to do: run smart every hour...
void
DraidMonitorThreadProc(
	IN	PVOID	Context
	)
{
	NTSTATUS				status;
	ULONG					i;
	PLURELATION_NODE		Lurn;
	PLURELATION_NODE		ChildLurn;
	KIRQL					oldIrql;
	LARGE_INTEGER			TimeOut;
	PRAID_INFO				pRaidInfo;
	PDRAID_CLIENT_INFO		pClientInfo;

	UINT32					nChildCount;
	NDAS_RAID_META_DATA	Rmd;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);	
	KDPrintM(DBG_LURN_INFO, ("Starting client monitor\n"));
	Lurn = (PLURELATION_NODE)Context;
	ASSERT(LURN_RAID1R == Lurn->LurnType || LURN_RAID4R == Lurn->LurnType);

	pRaidInfo = Lurn->LurnRAIDInfo;
	ASSERT(pRaidInfo);

	pClientInfo = ( PDRAID_CLIENT_INFO)pRaidInfo->pDraidClient;
	ASSERT(pClientInfo);

	nChildCount = Lurn->LurnChildrenCnt;


	while(TRUE) {
		PKEVENT				events[1];
		LONG				eventCount = 1;
		NTSTATUS			waitStatus;

		ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

		events[0] = &pClientInfo->MonitorThreadEvent; // wait for quit message
		
		TimeOut.QuadPart = - NANO100_PER_SEC * 30;
			
		waitStatus = KeWaitForMultipleObjects(
				eventCount, 
				events,
				WaitAny, 
				Executive,
				KernelMode,
				TRUE,
				&TimeOut,
				NULL);
		//
		// Check termination request.
		//
		ACQUIRE_SPIN_LOCK(&pClientInfo->SpinLock, &oldIrql);
		if (pClientInfo->RequestToTerminate) {
			RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
			KDPrintM(DBG_LURN_INFO, ("Client termination requested\n"));	
			break;
		}
		if (pClientInfo->ClientStatus !=	DRAID_CLIENT_STATUS_NO_ARBITER 
			&& pClientInfo->ClientStatus != DRAID_CLIENT_STATUS_ARBITER_CONNECTED) {
			KDPrintM(DBG_LURN_INFO, ("Client is initialized yet.\n"));	
			RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
			continue;
		}
		if (pClientInfo->InTransition) {
			KDPrintM(DBG_LURN_INFO, ("Client is in transition.\n"));
			// If it's in transition, we are better not to access RAID map or change another status.
			RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);
			continue;
		}
		RELEASE_SPIN_LOCK(&pClientInfo->SpinLock, oldIrql);

		//
		// Revive stopped node.
		//
		for(i = 0; i < nChildCount; i++)
		{
			ChildLurn = pRaidInfo->MapLurnChildren[i];

			if (LURN_STATUS_STALL == ChildLurn->LurnStatus) {
				PCCB ccb;
				//
				// If no more request came after returning busy because of stalling, lurn may have no chance to reconnect.
				// Send NOOP to reinitiate reconnect.
				//

				KDPrintM(DBG_LURN_INFO, ("Send NOOP message to stalling node %d\n", pClientInfo->RoleToNodeMap[i]));				

				status = LSCcbAllocate(&ccb);
				if(!NT_SUCCESS(status)) {
					KDPrintM(DBG_LURN_ERROR, ("Failed allocate Ccb"));
					break;

				}

				LSCCB_INITIALIZE(ccb);
				ccb->OperationCode				= CCB_OPCODE_NOOP;
				ccb->HwDeviceExtension			= NULL;
				LSCcbSetFlag(ccb, CCB_FLAG_ALLOCATED);
				LSCcbSetCompletionRoutine(ccb, NULL, NULL);

				//
				//	Send a CCB to the LURN.
				//
				status = LurnRequest(
								ChildLurn,
								ccb
							);
				if(!NT_SUCCESS(status)) {
					KDPrintM(DBG_LURN_ERROR, ("Failed to send NOOP request\n"));
					LSCcbFree(ccb);
				}
				continue;
			}

			if (pClientInfo->NodeFlagsRemote[pClientInfo->RoleToNodeMap[i]] & DRIX_NODE_FLAG_DEFECTIVE) {
				// Node is defect. No need to revive
				KDPrintM(DBG_LURN_INFO, ("Dont try to revive. Node %d is marked as defective\n", pClientInfo->RoleToNodeMap[i]));
				continue;
			}
		
			if (LURN_STATUS_RUNNING == ChildLurn->LurnStatus)
			{
				if (pClientInfo->NodeFlagsLocal[pClientInfo->RoleToNodeMap[i]] & DRIX_NODE_FLAG_RUNNING) {
					// Node is running and marked as running. Nothing to do.
					continue;
				}
				KDPrintM(DBG_LURN_INFO, ("Reviving: Node %d is already in running state.\n", pClientInfo->RoleToNodeMap[i]));
				status = STATUS_SUCCESS;
				//
				// LurnIdeDisk may have succeeded in reconnecting.
				//
			} else {
				KDPrintM(DBG_LURN_INFO, ("Trying to revive node %d\n", pClientInfo->RoleToNodeMap[i]));
				//
				// We may need to change LurnDesc if access right is updated.
				//
				status = LurnInitialize(ChildLurn, ChildLurn->Lur, ChildLurn->SavedLurnDesc);
			}
			
			if(NT_SUCCESS(status) && LURN_IS_RUNNING(ChildLurn->LurnStatus))
			{
				BOOLEAN	 Defected = FALSE;
				UCHAR DefectCode = 0;
				KDPrintM(DBG_LURN_INFO, ("Revived node %d\n", pClientInfo->RoleToNodeMap[i]));
	
				//
				// Read rmd
				//
				status = LurnExecuteSyncRead(ChildLurn, (PUCHAR)&Rmd,
					NDAS_BLOCK_LOCATION_RMD, 1);

				if(!NT_SUCCESS(status)) 
				{
					KDPrintM(DBG_LURN_INFO, ("Failed to read RMD from node %d. Marking disk as defective\n", i));
					DraidClientUpdateNodeFlags(pClientInfo, pRaidInfo->MapLurnChildren[i], 
						DRIX_NODE_FLAG_DEFECTIVE, DRIX_NODE_DEFECT_BAD_DISK);
					Defected = TRUE;
				} 
				else if (NDAS_RAID_META_DATA_SIGNATURE != Rmd.Signature  ||
					!IS_RMD_CRC_VALID(crc32_calc, Rmd)) 
				{
					// Check RMD signature. Currently hot-swapping is not supported. Mark new disk as defective disk.
					// To do for hot-swapping:
					//		Check disk is large enough
					//		Clear defective flag and mark all bit dirty
					//

					KDPrintM(DBG_LURN_INFO, ("RMD signature or CRC of node %d is not correct. May be disk is replaced with new one.\n", i));
					KDPrintM(DBG_LURN_INFO, ("   Currently hot-swapping is not supported\n", i));
					DraidClientUpdateNodeFlags(pClientInfo, pRaidInfo->MapLurnChildren[i], 
						DRIX_NODE_FLAG_DEFECTIVE, DRIX_NODE_DEFECT_NOT_A_MEMBER);
					Defected = TRUE;
				} 
				else if (RtlCompareMemory(&Rmd.RaidSetId, &pClientInfo->RaidSetId, sizeof(GUID)) != sizeof(GUID)) 
				{	// Check RMD's RAID set is same		
					KDPrintM(DBG_LURN_INFO, ("RAID Set ID for node %d is not matched. Consider defective.\n", i));
					DraidClientUpdateNodeFlags(pClientInfo, pRaidInfo->MapLurnChildren[i], 
						DRIX_NODE_FLAG_DEFECTIVE, DRIX_NODE_DEFECT_NOT_A_MEMBER);
					Defected = TRUE;
				} 
				else if (RtlCompareMemory(&Rmd.ConfigSetId, &pClientInfo->ConfigSetId, sizeof(GUID)) != sizeof(GUID)) 
				{
					KDPrintM(DBG_LURN_INFO, ("Configuration Set ID for node %d is not matched. Consider defective.\n", i));
					DraidClientUpdateNodeFlags(pClientInfo, pRaidInfo->MapLurnChildren[i], 
						DRIX_NODE_FLAG_DEFECTIVE, DRIX_NODE_DEFECT_PRIOR_MEMBER);
					Defected = TRUE;
				}
				
				if (Defected) {
					LurnSendStopCcb(ChildLurn);
					LurnDestroy(ChildLurn);
					continue;
				}
			}
			// Update status.
			DraidClientUpdateNodeFlags(pClientInfo, pRaidInfo->MapLurnChildren[i], 
					0, 0);
		}		
	}
	
	KDPrintM(DBG_LURN_INFO, ("Exiting\n"));
	PsTerminateSystemThread(STATUS_SUCCESS);
	return;
}

