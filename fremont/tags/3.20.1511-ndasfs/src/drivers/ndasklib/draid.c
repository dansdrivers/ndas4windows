#include <ntddk.h>
#include "LSKLib.h"
#include "KDebug.h"
#include "LSProto.h"
#include "LSLurn.h"
#include "draid.h"
#include "lslurnassoc.h"
#include "Scrc32.h"
#include "draidexp.h"

#include "cipher.h"
#include "lslurnide.h"
#include "draid.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "DRaid"


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
//
//	DRAID Listener. Only one instance exist for ndasscsi driver instance.
//
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

volatile PDRAID_GLOBALS g_DraidGlobals = NULL;


NTSTATUS 
DraidStartListenAddress(
	PDRAID_GLOBALS DraidGlobals,
	PDRAID_LISTEN_CONTEXT ListenContext
);

NTSTATUS 
DraidStopListenAddress(
	PDRAID_GLOBALS DraidGlobals,
	PDRAID_LISTEN_CONTEXT ListenContext
);

NTSTATUS
DraidAcceptConnection(
	PDRAID_GLOBALS DraidGlobals,
	PDRAID_LISTEN_CONTEXT ListenContext
);

PDRAID_LISTEN_CONTEXT 
DraidCreateListenContext(
	PDRAID_GLOBALS DraidGlobals,
	PLPX_ADDRESS Addr
);

NTSTATUS 
DraidListenConnection(
	PDRAID_LISTEN_CONTEXT ListenContext
);

// Returns new event count
INT32 
DraidReallocEventArray(
	PKEVENT** Events,
	PKWAIT_BLOCK* WaitBlocks,
	INT32 CurrentCount
) {
	INT32 NewCount;
	
	NewCount = CurrentCount+4;
	KDPrintM(DBG_LURN_TRACE, ("Allocating event array to count %d\n", NewCount));
	if (*Events)
		ExFreePoolWithTag(*Events, DRAID_EVENT_ARRAY_POOL_TAG);
	if (*WaitBlocks)
		ExFreePoolWithTag(*WaitBlocks, DRAID_EVENT_ARRAY_POOL_TAG);
	*Events = ExAllocatePoolWithTag(NonPagedPool, sizeof(PKEVENT) * NewCount, DRAID_EVENT_ARRAY_POOL_TAG);
	*WaitBlocks = ExAllocatePoolWithTag(NonPagedPool, sizeof(KWAIT_BLOCK) * NewCount, DRAID_EVENT_ARRAY_POOL_TAG);
	return NewCount;
}
VOID
DraidFreeEventArray(
	PKEVENT* Events,
	PKWAIT_BLOCK WaitBlocks
) {
	if (Events) {
		ExFreePoolWithTag(Events, DRAID_EVENT_ARRAY_POOL_TAG);
	}
	if (WaitBlocks) {
		ExFreePoolWithTag(WaitBlocks, DRAID_EVENT_ARRAY_POOL_TAG);
	}
}

//
// Wait for incoming request to any arbiter on this host.
//
VOID DraidListenerThreadProc(
	PVOID Param
) {
	PDRAID_GLOBALS DraidGlobals = (PDRAID_GLOBALS)Param;
	NTSTATUS		status;
	PLIST_ENTRY	listEntry;
	PDRAID_LISTEN_CONTEXT ListenContext;
	KIRQL oldIrql;
	INT32 				MaxEventCount = 0;
	PKEVENT				*events = NULL;
	PKWAIT_BLOCK		WaitBlocks = NULL;
	INT32				eventCount;
	NTSTATUS			waitStatus;
	SOCKETLPX_ADDRESS_LIST	socketLpxAddressList;
	INT32 i;
	
	KDPrintM(DBG_LURN_INFO, ("Starting\n"));

	MaxEventCount = DraidReallocEventArray(&events, &WaitBlocks, MaxEventCount);
	
	status = LpxTdiGetAddressList(
		&socketLpxAddressList
        );
	if (!NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_INFO, ("Failed to get address list\n"));	
		goto out;
	}
	for(i=0;i<socketLpxAddressList.iAddressCount;i++) {
		ListenContext = DraidCreateListenContext(DraidGlobals, &socketLpxAddressList.SocketLpx[i].LpxAddress);
		if (ListenContext) {
			status = DraidStartListenAddress(DraidGlobals, ListenContext);
			if (!NT_SUCCESS(status)) {
				DraidStopListenAddress(DraidGlobals, ListenContext);
			}
		}
	}
	
	while(TRUE) {
restart:
		if (events == NULL || WaitBlocks == NULL) {
			KDPrintM(DBG_LURN_INFO, ("Insufficient memory\n"));
			break;
		}
		eventCount = 0;
		//
		//	Wait exit event, net change event, connect request.
		//
		events[0] = &DraidGlobals->DraidExitEvent;
		eventCount++;
		events[1] = &DraidGlobals->NetChangedEvent;
		eventCount++;

		//
		// Add listening event 
		//
		ACQUIRE_SPIN_LOCK(&DraidGlobals->ListenContextSpinlock, &oldIrql);
		for (listEntry = DraidGlobals->ListenContextList.Flink;
			listEntry != &DraidGlobals->ListenContextList;
			listEntry = listEntry->Flink) 
		{
			ListenContext = CONTAINING_RECORD (listEntry, DRAID_LISTEN_CONTEXT, Link);
			if (!ListenContext->Destroy) {
				if (MaxEventCount < eventCount+1) {
					RELEASE_SPIN_LOCK(&DraidGlobals->ListenContextSpinlock, oldIrql);
					MaxEventCount = DraidReallocEventArray(&events, &WaitBlocks, MaxEventCount);
					goto restart;
				}
				events[eventCount] = &ListenContext->TdiListenContext.CompletionEvent;
				eventCount++;
			}
		}
		RELEASE_SPIN_LOCK(&DraidGlobals->ListenContextSpinlock, oldIrql);
		
		waitStatus = KeWaitForMultipleObjects(
					eventCount, 
					events,
					WaitAny, 
					Executive,
					KernelMode,
					TRUE,
					NULL, //&TimeOut,
					WaitBlocks);
		if (KeReadStateEvent(&DraidGlobals->DraidExitEvent)) {
			KDPrintM(DBG_LURN_INFO, ("Exit requested\n"));
			break;
		}
		if (KeReadStateEvent(&DraidGlobals->NetChangedEvent)) {
			KDPrintM(DBG_LURN_INFO, ("NIC status has changed\n"));
			KeClearEvent(&DraidGlobals->NetChangedEvent);
			//
			// Start if not listening and stop if destorying request is on.
			//
			ACQUIRE_SPIN_LOCK(&DraidGlobals->ListenContextSpinlock, &oldIrql);
			for (listEntry = DraidGlobals->ListenContextList.Flink;
				listEntry != &DraidGlobals->ListenContextList;
				listEntry = listEntry->Flink) 
			{
				ListenContext = CONTAINING_RECORD (listEntry, DRAID_LISTEN_CONTEXT, Link);
				status = STATUS_SUCCESS;
				if (!ListenContext->Started) {
					RELEASE_SPIN_LOCK(&DraidGlobals->ListenContextSpinlock, oldIrql);	
					status = DraidStartListenAddress(DraidGlobals, ListenContext);
					ACQUIRE_SPIN_LOCK(&DraidGlobals->ListenContextSpinlock, &oldIrql);
				}
				if (!NT_SUCCESS(status) || ListenContext->Destroy) {
					listEntry = listEntry->Blink; // // Move to next entry. There may be multiple interface is down.
					RemoveEntryList(&ListenContext->Link);
					RELEASE_SPIN_LOCK(&DraidGlobals->ListenContextSpinlock, oldIrql);					
					DraidStopListenAddress(DraidGlobals, ListenContext);
					ACQUIRE_SPIN_LOCK(&DraidGlobals->ListenContextSpinlock, &oldIrql);					
				}
			}
			RELEASE_SPIN_LOCK(&DraidGlobals->ListenContextSpinlock, oldIrql);
			continue; // Reset listen event.
		}

		// Check whether listen event has been signaled.
		for(i=2;i<eventCount;i++) {
			if (KeReadStateEvent(events[i])) {
				BOOLEAN Found;
				KeClearEvent(events[i]);
				// Find listencontext related to this event.
				ACQUIRE_SPIN_LOCK(&DraidGlobals->ListenContextSpinlock, &oldIrql);
				Found = FALSE;
				for (listEntry = DraidGlobals->ListenContextList.Flink;
					listEntry != &DraidGlobals->ListenContextList;
					listEntry = listEntry->Flink) 
				{
					ListenContext = CONTAINING_RECORD (listEntry, DRAID_LISTEN_CONTEXT, Link);
					if (&ListenContext->TdiListenContext.CompletionEvent == events[i]) {
						Found = TRUE;
						break;
					}
				}
				RELEASE_SPIN_LOCK(&DraidGlobals->ListenContextSpinlock, oldIrql);	
				if (Found) {
					DraidAcceptConnection(DraidGlobals, ListenContext);
				} else {
					ASSERT(FALSE);
				}
			}
		}
	}
out:
	//
	// to do: clean-up pending client
	//

	//
	// Close and free pending listen contexts
	//
	while(listEntry = 
		ExInterlockedRemoveHeadList(&DraidGlobals->ListenContextList,  
			&DraidGlobals->ListenContextSpinlock)) 
	{
		ListenContext = CONTAINING_RECORD (listEntry, DRAID_LISTEN_CONTEXT, Link);
		DraidStopListenAddress(DraidGlobals, ListenContext);
	}

	DraidFreeEventArray(events, WaitBlocks);
	
	KDPrint(1,("Exiting.\n"));
	PsTerminateSystemThread(STATUS_SUCCESS);	
}

NTSTATUS 
DraidStart(
	PDRAID_GLOBALS DraidGlobals
) {
	NTSTATUS		status;
	OBJECT_ATTRIBUTES  objectAttributes;
#if 0
	TDI_CLIENT_INTERFACE_INFO   info;
	UNICODE_STRING              clientName;
#endif

	KDPrintM(DBG_LURN_INFO, ("Starting\n"));
	
	InitializeListHead(&DraidGlobals->ArbiterList);
	KeInitializeSpinLock(&DraidGlobals->ArbiterListSpinlock);
	InitializeListHead(&DraidGlobals->ClientList);
	KeInitializeSpinLock(&DraidGlobals->ClientListSpinlock);
	KeInitializeEvent(&DraidGlobals->DraidExitEvent, NotificationEvent, FALSE);	
	KeInitializeEvent(&DraidGlobals->NetChangedEvent, NotificationEvent, FALSE);		

	InitializeListHead(&DraidGlobals->ListenContextList);
	KeInitializeSpinLock(&DraidGlobals->ListenContextSpinlock);

	InitializeObjectAttributes(
		&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

	g_DraidGlobals = DraidGlobals;

	status = PsCreateSystemThread(
		&DraidGlobals->DraidThreadHandle,
		THREAD_ALL_ACCESS,
		&objectAttributes,
		NULL,
		NULL,
		DraidListenerThreadProc,
		DraidGlobals
		);
	if(!NT_SUCCESS(status)) {
		ASSERT(FALSE);
	
		DraidGlobals->DraidThreadHandle = NULL;
		DraidGlobals->DraidThreadObject = NULL;
		return STATUS_UNSUCCESSFUL;
	}

	status = ObReferenceObjectByHandle(
			DraidGlobals->DraidThreadHandle,
			FILE_READ_DATA,
			NULL,
			KernelMode,
			&DraidGlobals->DraidThreadObject,
			NULL
		);
	if(!NT_SUCCESS(status)) {
		ASSERT(FALSE);
		DraidGlobals->DraidThreadObject = NULL;
		DraidGlobals->DraidThreadHandle = NULL;
		return STATUS_UNSUCCESSFUL;
	}
	
	return STATUS_SUCCESS;
}

NTSTATUS 
DraidStop(
	PDRAID_GLOBALS DraidGlobals
) {
	NTSTATUS status;
	LARGE_INTEGER Timeout;
	
	KDPrintM(DBG_LURN_INFO, ("Stopping\n"));
	
	if(DraidGlobals->DraidThreadHandle && DraidGlobals->DraidThreadObject) {
		KeSetEvent(&DraidGlobals->DraidExitEvent, IO_NO_INCREMENT, FALSE);
		status = KeWaitForSingleObject(
					DraidGlobals->DraidThreadObject,
					Executive,
					KernelMode,
					FALSE,
					NULL);
		ObDereferenceObject(DraidGlobals->DraidThreadObject);
		ZwClose(DraidGlobals->DraidThreadHandle);
		DraidGlobals->DraidThreadHandle = NULL;
		DraidGlobals->DraidThreadObject = NULL;
	}

	while(TRUE) {
		if (InterlockedCompareExchange(&DraidGlobals->ReceptionThreadCount, 0, 0)==0) 
			break;
		KDPrintM(DBG_LURN_INFO, ("Reception thread is still running.. Waiting..\n"));
		Timeout.QuadPart = - HZ/2; // 500ms
		KeDelayExecutionThread(KernelMode, FALSE, &Timeout);		
	}

	g_DraidGlobals = NULL;

	KDPrintM(DBG_LURN_INFO, ("Stopped\n"));

	return STATUS_SUCCESS;
}

//
// Wait for registration message and forward proper arbiter.
//
VOID
DraidReceptionThreadProc(
	IN PVOID Param
) {
	PDRAID_REMOTE_CLIENT_CONNECTION Connection = Param;
	LARGE_INTEGER Timeout;
	NTSTATUS status;
	DRIX_REGISTER RegMsg;
	DRIX_HEADER Reply = {0};
	PDRAID_ARBITER_INFO Arbiter;
	KIRQL	oldIrql;
	PLIST_ENTRY listEntry;
	BOOLEAN Disconnect = TRUE;
	BOOLEAN MatchFound;
	ULONG		result;
	
	Connection->TdiReceiveContext.Irp = NULL;
	KeInitializeEvent(&Connection->TdiReceiveContext.CompletionEvent, NotificationEvent, FALSE) ;
	
	// Wait for network event or short timeout
	status = LpxTdiRecvWithCompletionEvent(
					Connection->ConnectionFileObject,
					&Connection->TdiReceiveContext,
					(PUCHAR)&RegMsg,
					sizeof(DRIX_REGISTER),
					0,
					NULL,
					NULL
					);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_INFO, ("LpxTdiRecvWithCompletionEvent returned %d.\n", status));
		goto out;
	}

	Timeout.QuadPart =  - HZ * 5;
	
	status = KeWaitForSingleObject(
		&Connection->TdiReceiveContext.CompletionEvent,
		Executive, KernelMode, 	FALSE, &Timeout);
	
	if (status == STATUS_SUCCESS) {
		UCHAR ResultCode;
		
		//
		// Data received. Check validity and forward channel to arbiter.
		//
		if (Connection->TdiReceiveContext.Result != sizeof(DRIX_REGISTER)) {
			KDPrintM(DBG_LURN_INFO, ("Registration packet size is not %d.\n", sizeof(DRIX_REGISTER)));
			status = STATUS_UNSUCCESSFUL;
			goto out;
		}

		if (NTOHL(RegMsg.Header.Signature) != DRIX_SIGNATURE) {
			KDPrintM(DBG_LURN_INFO, ("DRIX signature mismatch\n"));
			status = STATUS_UNSUCCESSFUL;
			goto out;
		}
		
		if (RegMsg.Header.Command != DRIX_CMD_REGISTER) {
			KDPrintM(DBG_LURN_INFO, ("Inappropriate command %x sent.\n", RegMsg.Header.Command));
			status = STATUS_UNSUCCESSFUL;
			goto out;
		}

		if (RegMsg.Header.ReplyFlag) {
			KDPrintM(DBG_LURN_INFO, ("Reply flag should be cleared\n"));
			status = STATUS_UNSUCCESSFUL;
			goto out;
		}
		if (NTOHS(RegMsg.Header.Length) !=  sizeof(DRIX_REGISTER)) {
			KDPrintM(DBG_LURN_INFO, ("Invalid packet length %d\n", NTOHS(RegMsg.Header.Length)));
			status = STATUS_UNSUCCESSFUL;
			goto out;
		}

		ACQUIRE_SPIN_LOCK(&g_DraidGlobals->ArbiterListSpinlock, &oldIrql);
		MatchFound = FALSE;
		for (listEntry = g_DraidGlobals->ArbiterList.Flink;
			listEntry != &g_DraidGlobals->ArbiterList;
			listEntry = listEntry->Flink) 
		{
			Arbiter = CONTAINING_RECORD (listEntry, DRAID_ARBITER_INFO, AllArbiterList);
			if (RtlCompareMemory(&Arbiter->Rmd.RaidSetId, &RegMsg.RaidSetId, sizeof(GUID)) == sizeof(GUID) &&
				RtlCompareMemory(&Arbiter->Rmd.ConfigSetId, &RegMsg.ConfigSetId, sizeof(GUID)) == sizeof(GUID)
			) {
				MatchFound = TRUE;
				break;
			}
		}
		RELEASE_SPIN_LOCK(&g_DraidGlobals->ArbiterListSpinlock, oldIrql);

		if (MatchFound) {
			ResultCode = DRIX_RESULT_SUCCESS;
		} else {
			ResultCode = DRIX_RESULT_RAID_SET_NOT_FOUND;
		}
//reply:
		//
		// Send reply
		//
		Reply.Signature = 	NTOHL(DRIX_SIGNATURE);
		Reply.Command = DRIX_CMD_REGISTER;
		Reply.Length = NTOHS((UINT16)sizeof(DRIX_HEADER));
		Reply.ReplyFlag = 1;
		Reply.Sequence = RegMsg.Header.Sequence;
		Reply.Result = ResultCode;

		Timeout.QuadPart =  HZ * 5;

		KDPrintM(DBG_LURN_INFO, ("DRAID Sending registration reply(result=%x) to remote client\n", ResultCode));
		status = LpxTdiSend(
					Connection->ConnectionFileObject, (PUCHAR)&Reply, sizeof(DRIX_HEADER), 
					0, &Timeout,	NULL, &result	);
		KDPrintM(DBG_LURN_INFO, ("LpxTdiSend status=%x, result=%x.\n", status, result));
		if (status !=STATUS_SUCCESS) {
			Disconnect = TRUE;
			goto out;
		}
		if (MatchFound) {
			status = DraidArbiterAcceptClient(Arbiter, RegMsg.ConnType, Connection);
			if (status == STATUS_SUCCESS) {
				Disconnect = FALSE;
			} else {
				KDPrintM(DBG_LURN_INFO, ("Failed to accept client %x.\n", status));
			}
		}
	} else if (status == STATUS_TIMEOUT) {
		KDPrintM(DBG_LURN_INFO, ("Timeout before registration.\n"));
	}
	
out:
	if (Disconnect) {
		KDPrintM(DBG_LURN_INFO, ("Closing connection to client.\n"));			
		// Close connection.
		LpxTdiDisassociateAddress(Connection->ConnectionFileObject);
		LpxTdiCloseConnection(
					Connection->ConnectionFileHandle, 
					Connection->ConnectionFileObject
					);
		Connection->ConnectionFileHandle = NULL;
		Connection->ConnectionFileObject = NULL;

		ExFreePoolWithTag(Connection, DRAID_REMOTE_CLIENT_CHANNEL_POOL_TAG);
	} else {
		// Arbiter thread will close connection and free channel
	}
	
	KDPrintM(DBG_LURN_INFO, ("Exiting reception thread.\n"));
	// Decrease counter
	InterlockedDecrement(&g_DraidGlobals->ReceptionThreadCount);
}

//
// Start reception thread to handle registration request from client.
//
NTSTATUS
DraidAcceptConnection(
	PDRAID_GLOBALS DraidGlobals,
	PDRAID_LISTEN_CONTEXT ListenContext
) {
	PDRAID_REMOTE_CLIENT_CONNECTION Connection;
	HANDLE			ConFileHandle;
	PFILE_OBJECT		ConFileObject;
	LPX_ADDRESS	RemoteAddr;
	NTSTATUS	status;
	OBJECT_ATTRIBUTES objectAttributes;
	HANDLE		ThreadHandle;
	
	if (ListenContext->TdiListenContext.Status != STATUS_SUCCESS) {
		// Failed to accept connection. Maybe NIC is disabled. This context will be handled by DraidStopListenAddr
		KDPrintM(DBG_LURN_INFO, ("Failed to listen. Maybe network is down.\n"));	
		return STATUS_UNSUCCESSFUL;
	}

	//
	// Get information about new connection and prepare for new listening.
	//
	ConFileHandle = ListenContext->ListenFileHandle;
	ConFileObject = ListenContext->ListenFileObject;
	RtlCopyMemory(&RemoteAddr, &ListenContext->TdiListenContext.RemoteAddress, sizeof(LPX_ADDRESS));
	
	ListenContext->TdiListenContext.Irp = NULL;
	ListenContext->ListenFileHandle = NULL;
	ListenContext->ListenFileObject = NULL;

	KDPrintM(DBG_LURN_INFO, 
		("Connected from %02X:%02X:%02X:%02X:%02X:%02X 0x%4X\n",
						RemoteAddr.Node[0], RemoteAddr.Node[1],
						RemoteAddr.Node[2], RemoteAddr.Node[3],
						RemoteAddr.Node[4], RemoteAddr.Node[5],
						NTOHS(RemoteAddr.Port)
	));

	status = DraidListenConnection(ListenContext);
	if (!NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_INFO, ("Failed to listen again\n"));		
		ASSERT(FALSE);
		// Continue anyway even if anothre listening is failure
	}
	
	//
	// Now we can Handle accpeted connection.
	//
	Connection = ExAllocatePoolWithTag(NonPagedPool, 
		sizeof(DRAID_REMOTE_CLIENT_CONNECTION), DRAID_REMOTE_CLIENT_CHANNEL_POOL_TAG);

	if (!Connection) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	RtlZeroMemory(Connection, sizeof(DRAID_REMOTE_CLIENT_CONNECTION));
	Connection->ConnectionFileHandle = ConFileHandle;
	Connection->ConnectionFileObject = ConFileObject;
	RtlCopyMemory(&Connection->RemoteAddr, &RemoteAddr, sizeof(LPX_ADDRESS));

	// Start reception thread.
	InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

	ASSERT(KeGetCurrentIrql() ==  PASSIVE_LEVEL);
	
	status = PsCreateSystemThread(
		&ThreadHandle,
		THREAD_ALL_ACCESS,
		&objectAttributes,
		NULL,
		NULL,
		DraidReceptionThreadProc,
		(PVOID) Connection
		);

	if(!NT_SUCCESS(status))
	{
		KDPrintM(DBG_LURN_ERROR, ("Reception thread creation failedPsCreateSystemThread FAIL %x\n", status));
		ExFreePoolWithTag(Connection, DRAID_REMOTE_CLIENT_CHANNEL_POOL_TAG);
		return status;
	}

	ZwClose(ThreadHandle);  // Reception thread will be exited by itself. Close now.

	InterlockedIncrement(&DraidGlobals->ReceptionThreadCount);

	return STATUS_SUCCESS;
}

PDRAID_LISTEN_CONTEXT 
DraidCreateListenContext(
	PDRAID_GLOBALS DraidGlobals,
	PLPX_ADDRESS Addr
) {
	KIRQL	oldIrql;
	BOOLEAN AlreadyExist;
	PLIST_ENTRY listEntry;
	PDRAID_LISTEN_CONTEXT ListenContext;
		
	//
	// Check address is already in the listen context list
	//
	ACQUIRE_SPIN_LOCK(&DraidGlobals->ListenContextSpinlock, &oldIrql);
	AlreadyExist = FALSE;
	for (listEntry = DraidGlobals->ListenContextList.Flink;
		listEntry != &DraidGlobals->ListenContextList;
		listEntry = listEntry->Flink) 
	{
		ListenContext = CONTAINING_RECORD (listEntry, DRAID_LISTEN_CONTEXT, Link);
		if (!ListenContext->Destroy && RtlCompareMemory(ListenContext->Addr.Node, 
			Addr->Node, 6) == 6) {
			KDPrintM(DBG_LURN_INFO, ("New LPX address already exist.Ignoring.\n"));
			AlreadyExist = TRUE;
			break;
		}
	}
	RELEASE_SPIN_LOCK(&DraidGlobals->ListenContextSpinlock, oldIrql);
	if (AlreadyExist) {
		return NULL;
	}

	//
	// Alloc listen context
	//
	ListenContext = ExAllocatePoolWithTag(NonPagedPool, sizeof(DRAID_LISTEN_CONTEXT), 
		DRAID_LISTEN_CONTEXT_POOL_TAG);
	if (!ListenContext) {
		KDPrintM(DBG_LURN_INFO, ("Failed to alloc listen context\n"));
		return NULL;
	}
	RtlZeroMemory(ListenContext, sizeof(DRAID_LISTEN_CONTEXT));
	
	KeInitializeEvent(
			&ListenContext->TdiListenContext.CompletionEvent, 
			NotificationEvent, 
			FALSE
			);
	InitializeListHead(&ListenContext->Link);

	RtlCopyMemory(ListenContext->Addr.Node, Addr->Node, 6);
	ListenContext->Addr.Port = HTONS(DRIX_ARBITER_PORT_NUM_BASE);

	ExInterlockedInsertTailList(&DraidGlobals->ListenContextList, 
		&ListenContext->Link, 
		&DraidGlobals->ListenContextSpinlock
	);
	return ListenContext;
}

NTSTATUS 
DraidListenConnection(
	PDRAID_LISTEN_CONTEXT ListenContext
) {
	NTSTATUS status;
//	KIRQL	oldIrql;
	
	KeClearEvent(&ListenContext->TdiListenContext.CompletionEvent);
	
	status = LpxTdiOpenConnection(
			&ListenContext->ListenFileHandle,
			&ListenContext->ListenFileObject,
			NULL);

	if(!NT_SUCCESS(status)) 
	{
		ASSERT(FALSE);// This should not happen
		return status;
	}

	// 3. Associate address
	status = LpxTdiAssociateAddress(
			ListenContext->ListenFileObject,
			ListenContext->AddressFileHandle
			);

	if(!NT_SUCCESS(status)) 
	{
		ASSERT(FALSE);
		LpxTdiCloseConnection(ListenContext->ListenFileHandle, ListenContext->ListenFileObject);
		return status;
	}

	// 4. Start listening

	ListenContext->Flags = TDI_QUERY_ACCEPT; //???

	status = LpxTdiListenWithCompletionEvent(
					ListenContext->ListenFileObject,
					&ListenContext->TdiListenContext,
					&ListenContext->Flags
				);
	if(!NT_SUCCESS(status)) {
		ASSERT(FALSE); // May be this can be happen if multiple instances of this function is called with same address
		LpxTdiDisassociateAddress(ListenContext->ListenFileObject);
		LpxTdiCloseConnection(ListenContext->ListenFileHandle, ListenContext->ListenFileObject);
		return status;
	}
	return status;
}

NTSTATUS 
DraidStartListenAddress(
	PDRAID_GLOBALS DraidGlobals,
	PDRAID_LISTEN_CONTEXT ListenContext
) {
	NTSTATUS status;
	KIRQL	oldIrql;

	KDPrintM(DBG_LURN_INFO, ("Starting to listen address %02x:%02x:%02x:%02x:%02x:%02x\n",
		ListenContext->Addr.Node[0], ListenContext->Addr.Node[1], ListenContext->Addr.Node[2],
		ListenContext->Addr.Node[3], ListenContext->Addr.Node[4], ListenContext->Addr.Node[5]
	));

	ACQUIRE_SPIN_LOCK(&DraidGlobals->ListenContextSpinlock, &oldIrql);
	ListenContext->Started = TRUE;
	RELEASE_SPIN_LOCK(&DraidGlobals->ListenContextSpinlock, oldIrql);

	//
	// Start listen.
	//

	// 1. Open address
	status = LpxTdiOpenAddress(
				&ListenContext->AddressFileHandle,
				&ListenContext->AddressFileObject,
				&ListenContext->Addr
			);

	if(!NT_SUCCESS(status)) 
	{
		goto errout;
	}

	
	// 2. Open connection
	status = DraidListenConnection(ListenContext);

	if(!NT_SUCCESS(status)) 
	{
		goto errout;
	}

	return status;
	
errout:

	if (ListenContext->AddressFileHandle && ListenContext->AddressFileObject) {
		LpxTdiCloseAddress(ListenContext->AddressFileHandle, ListenContext->AddressFileObject);
		ListenContext->AddressFileHandle = NULL;
		ListenContext->AddressFileObject = NULL;
	}
	ACQUIRE_SPIN_LOCK(&DraidGlobals->ListenContextSpinlock, &oldIrql);
	ListenContext->Started = FALSE;
	RELEASE_SPIN_LOCK(&DraidGlobals->ListenContextSpinlock, oldIrql);
	return status;
}

NTSTATUS 
DraidStopListenAddress(
	PDRAID_GLOBALS DraidGlobals,
	PDRAID_LISTEN_CONTEXT ListenContext
) {
	UNREFERENCED_PARAMETER(DraidGlobals);
	
	KDPrintM(DBG_LURN_INFO, ("Stop listening to address %02x:%02x:%02x:%02x:%02x:%02x\n",
		ListenContext->Addr.Node[0], ListenContext->Addr.Node[1], ListenContext->Addr.Node[2],
		ListenContext->Addr.Node[3], ListenContext->Addr.Node[4], ListenContext->Addr.Node[5]
	));
	if (ListenContext->ListenFileObject)
		LpxTdiDisconnect(ListenContext->ListenFileObject, 0);
	if (ListenContext->ListenFileObject)
		LpxTdiDisassociateAddress(ListenContext->ListenFileObject);
	if (ListenContext->ListenFileHandle && ListenContext->ListenFileObject) {
		LpxTdiCloseConnection(
			ListenContext->ListenFileHandle, 
			ListenContext->ListenFileObject
		);
	}
	if (ListenContext->AddressFileHandle && ListenContext->AddressFileObject) {
		LpxTdiCloseAddress (
			ListenContext->AddressFileHandle,
			ListenContext->AddressFileObject
		);
	}
	ExFreePoolWithTag(ListenContext, DRAID_LISTEN_CONTEXT_POOL_TAG);
	
	return STATUS_SUCCESS;
}

VOID
DraidListnerAddAddress(
	PTDI_ADDRESS_LPX Addr
) {
	PDRAID_GLOBALS DraidGlobals;
	if (!g_DraidGlobals) {
		KDPrintM(DBG_LURN_INFO, ("DRAID is not running\n"));
		return;
	}
	
	DraidGlobals = g_DraidGlobals;

	DraidCreateListenContext(DraidGlobals, Addr);

	KeSetEvent(&DraidGlobals->NetChangedEvent, IO_NO_INCREMENT, FALSE);	
}

VOID
DraidListnerDelAddress(
	PTDI_ADDRESS_LPX Addr
) {
	PDRAID_GLOBALS DraidGlobals;
	PLIST_ENTRY listEntry;
	KIRQL oldIrql;
	PDRAID_LISTEN_CONTEXT ListenContext;
	
	if (!g_DraidGlobals) {
		KDPrintM(DBG_LURN_INFO, ("DRAID is not running\n"));
		return;
	}

	DraidGlobals = g_DraidGlobals;
	
	// Find matching address and just mark active flag false because Wait event may be in use.	
	ACQUIRE_SPIN_LOCK(&DraidGlobals->ListenContextSpinlock, &oldIrql);
	for (listEntry = DraidGlobals->ListenContextList.Flink;
		listEntry != &DraidGlobals->ListenContextList;
		listEntry = listEntry->Flink) 
	{
		ListenContext = CONTAINING_RECORD (listEntry, DRAID_LISTEN_CONTEXT, Link);
		if (RtlCompareMemory(ListenContext->Addr.Node, 
			Addr->Node, 6) == 6) {
			KDPrintM(DBG_LURN_INFO, ("Found matching address\n"));
			ListenContext->Destroy = TRUE;
			KeSetEvent(&DraidGlobals->NetChangedEvent, IO_NO_INCREMENT, FALSE);
			break;
		}
	}
	RELEASE_SPIN_LOCK(&DraidGlobals->ListenContextSpinlock, oldIrql);
}


VOID
DraidPnpAddAddressHandler ( 
	IN PTA_ADDRESS NetworkAddress,
	IN PUNICODE_STRING  DeviceName,
	IN PTDI_PNP_CONTEXT Context
    )
/*++

Routine Description:

    TDI add address handler

Arguments:
    
    NetworkAddress  - new network address available on the system

    DeviceName      - name of the device to which address belongs

    Context         - PDO to which address belongs


Return Value:

    None

--*/
{
	UNICODE_STRING	lpxPrefix;

	PAGED_CODE ();

	UNREFERENCED_PARAMETER(Context);

	if (DeviceName==NULL) {
		KDPrintM(DBG_OTHER_ERROR, ("NO DEVICE NAME SUPPLIED when deleting address of type %d.\n",
			NetworkAddress->AddressType));
		return;
	}
	if (g_DraidGlobals == NULL) {
		ASSERT(FALSE);
		KDPrintM(DBG_OTHER_ERROR, ("DRAID is not active.\n",
			NetworkAddress->AddressType));
		return; 
	}
	KDPrintM(DBG_OTHER_TRACE, ("DeviceName=%ws AddrType=%u AddrLen=%u\n",
										DeviceName->Buffer,
										(ULONG)NetworkAddress->AddressType,
										(ULONG)NetworkAddress->AddressLength));

	//
	//	LPX
	//
	RtlInitUnicodeString(&lpxPrefix, LPX_BOUND_DEVICE_NAME_PREFIX);

	if(	RtlPrefixUnicodeString(&lpxPrefix, DeviceName, TRUE) &&
		NetworkAddress->AddressType == TDI_ADDRESS_TYPE_LPX
		){
		PTDI_ADDRESS_LPX	lpxAddr;

		lpxAddr = (PTDI_ADDRESS_LPX)NetworkAddress->Address;
		KDPrintM(DBG_OTHER_ERROR, ("LPX address added: %02x:%02x:%02x:%02x:%02x:%02x\n",
								lpxAddr->Node[0],
								lpxAddr->Node[1],
								lpxAddr->Node[2],
								lpxAddr->Node[3],
								lpxAddr->Node[4],
								lpxAddr->Node[5]));
		//
		//	LPX may leave dummy values.
		//
		RtlZeroMemory(lpxAddr->Reserved, sizeof(lpxAddr->Reserved));

		DraidListnerAddAddress(lpxAddr);

	} else if(NetworkAddress->AddressType == TDI_ADDRESS_TYPE_IP) {
		//
		//	IP	address
		//

		PTDI_ADDRESS_IP	ipAddr;
		PUCHAR			digit;

		ipAddr = (PTDI_ADDRESS_IP)NetworkAddress->Address;
		digit = (PUCHAR)&ipAddr->in_addr;
		KDPrintM(DBG_OTHER_TRACE, ("IP: %u.%u.%u.%u\n",digit[0],digit[1],digit[2],digit[3]));
	} else {
		KDPrintM(DBG_OTHER_TRACE, ("AddressType %u discarded.\n", (ULONG)NetworkAddress->AddressType));
	}
}




VOID
DraidPnpDelAddressHandler( 
	IN PTA_ADDRESS NetworkAddress,
	IN PUNICODE_STRING DeviceName,
	IN PTDI_PNP_CONTEXT Context
    )
/*++

Routine Description:

    TDI delete address handler

Arguments:
    
    NetworkAddress  - network address that is no longer available on the system

    Context1        - name of the device to which address belongs

    Context2        - PDO to which address belongs


Return Value:

    None

--*/
{
	UNICODE_STRING	lpxPrefix;

	PAGED_CODE ();

	UNREFERENCED_PARAMETER(Context);

	if (DeviceName==NULL) {
		KDPrintM(DBG_OTHER_ERROR, (
			"AfdDelAddressHandler: "
			"NO DEVICE NAME SUPPLIED when deleting address of type %d.\n",
			NetworkAddress->AddressType));
		return;
	}

	if (g_DraidGlobals == NULL) {
		ASSERT(FALSE);
		KDPrintM(DBG_OTHER_ERROR, ("DRAID is not active.\n",
			NetworkAddress->AddressType));
		return; 
	}
	
	KDPrintM(DBG_OTHER_TRACE, ("DeviceName=%ws AddrType=%u AddrLen=%u\n",
		DeviceName->Buffer,
		(ULONG)NetworkAddress->AddressType,
		(ULONG)NetworkAddress->AddressLength));

	//
	//	LPX
	//
	RtlInitUnicodeString(&lpxPrefix, LPX_BOUND_DEVICE_NAME_PREFIX);

	if(	RtlPrefixUnicodeString(&lpxPrefix, DeviceName, TRUE)){
		PTDI_ADDRESS_LPX	lpxAddr;

		lpxAddr = (PTDI_ADDRESS_LPX)NetworkAddress->Address;
		KDPrintM(DBG_OTHER_ERROR, ("LPX address deleted: %02x:%02x:%02x:%02x:%02x:%02x\n",
			lpxAddr->Node[0],
			lpxAddr->Node[1],
			lpxAddr->Node[2],
			lpxAddr->Node[3],
			lpxAddr->Node[4],
			lpxAddr->Node[5]));
		RtlZeroMemory(lpxAddr->Reserved, sizeof(lpxAddr->Reserved));		

		DraidListnerDelAddress(lpxAddr);
	} else if(NetworkAddress->AddressType == TDI_ADDRESS_TYPE_IP) {
		//
		//	IP	address
		//

		PTDI_ADDRESS_IP	ipAddr;
		PUCHAR			digit;

		ipAddr = (PTDI_ADDRESS_IP)NetworkAddress->Address;
		digit = (PUCHAR)&ipAddr->in_addr;
		KDPrintM(DBG_OTHER_TRACE, ("IP: %u.%u.%u.%u\n",digit[0],digit[1],digit[2],digit[3]));
	} else {
		KDPrintM(DBG_OTHER_TRACE, ("AddressType %u discarded.\n", (ULONG)NetworkAddress->AddressType));
	}
}


//
// Used when entering power down mode.
//
VOID
DraidFlushAll(
	VOID
) {
	KIRQL oldIrql;
	PLIST_ENTRY listEntry;
	PDRAID_CLIENT_INFO Client;
	PDRAID_GLOBALS DraidGlobals = g_DraidGlobals;
	if (!g_DraidGlobals)
		return;

	KDPrintM(DBG_LURN_INFO, ("DRAID flush all\n"));
	//
	// Flush request to all client 
	//
	ACQUIRE_SPIN_LOCK(&DraidGlobals->ClientListSpinlock, &oldIrql);
	for (listEntry = DraidGlobals->ClientList.Flink;
		listEntry != &DraidGlobals->ClientList;
		listEntry = listEntry->Flink) 
	{
		Client = CONTAINING_RECORD (listEntry, DRAID_CLIENT_INFO, AllClientList);
		DraidClientFlush(Client, NULL, NULL);
	}
	RELEASE_SPIN_LOCK(&DraidGlobals->ClientListSpinlock, oldIrql);

	// Send flush request to arbiter. (Not needed if multi-write is not used.)

}

//
// DRAID need flush before power down. 
// Copied from LsuClientPnPPowerChange
//
NTSTATUS
DraidTdiClientPnPPowerChange(
	IN PUNICODE_STRING	DeviceName,
	IN PNET_PNP_EVENT	PowerEvent,
	IN PTDI_PNP_CONTEXT	Context1,
	IN PTDI_PNP_CONTEXT	Context2
){
	NTSTATUS				status;
	UNICODE_STRING			lpxPrefix;
	NET_DEVICE_POWER_STATE	powerState;

	UNREFERENCED_PARAMETER(Context1);
	UNREFERENCED_PARAMETER(Context2);

	if (DeviceName==NULL) {
		KDPrintM(DBG_OTHER_ERROR, (
			"NO DEVICE NAME SUPPLIED when power event of type %x.\n",
			PowerEvent->NetEvent));
		return STATUS_SUCCESS;
	}

	if(PowerEvent == NULL) {
		return STATUS_SUCCESS;
	}

	if(PowerEvent->Buffer == NULL ||
		PowerEvent->BufferLength == 0) {
		powerState = NetDeviceStateUnspecified;
	} else {
		powerState = *((PNET_DEVICE_POWER_STATE) PowerEvent->Buffer);
	}

	RtlInitUnicodeString(&lpxPrefix, LPX_BOUND_DEVICE_NAME_PREFIX);
	if(	DeviceName == NULL || RtlPrefixUnicodeString(&lpxPrefix, DeviceName, TRUE) == FALSE) {

		KDPrintM(DBG_OTHER_ERROR, (
			"Not LPX binding device.\n"));

		return STATUS_SUCCESS;
	}

	status = STATUS_SUCCESS;
	switch(PowerEvent->NetEvent) {
		case NetEventSetPower:
			KDPrintM(DBG_OTHER_INFO, ("SetPower\n"));
			if(powerState != NetDeviceStateD0) {
				// Flush all RAID instances if exist.
				DraidFlushAll();				
			}
			break;
		default:
			break;
	}

	// Call default power change handler
	return LsuClientPnPPowerChange(DeviceName, PowerEvent, Context1, Context2);
}

NTSTATUS 
DraidRegisterArbiter(
	PDRAID_ARBITER_INFO Arbiter
) {
	ASSERT(g_DraidGlobals);
	ExInterlockedInsertTailList(&g_DraidGlobals->ArbiterList, &Arbiter->AllArbiterList, &g_DraidGlobals->ArbiterListSpinlock);
	return STATUS_SUCCESS;
}

NTSTATUS
DraidUnregisterArbiter(
	PDRAID_ARBITER_INFO Arbiter
) {
	KIRQL oldIrql;
	ASSERT(g_DraidGlobals);
	ACQUIRE_SPIN_LOCK(&g_DraidGlobals->ArbiterListSpinlock, &oldIrql);
	RemoveEntryList(&Arbiter->AllArbiterList);
	RELEASE_SPIN_LOCK(&g_DraidGlobals->ArbiterListSpinlock, oldIrql);
	return STATUS_SUCCESS;	
}

NTSTATUS
DraidRegisterClient(
	PDRAID_CLIENT_INFO Client
) {
	ASSERT(g_DraidGlobals);
	ExInterlockedInsertTailList(&g_DraidGlobals->ClientList, &Client->AllClientList, &g_DraidGlobals->ClientListSpinlock);
	return STATUS_SUCCESS;
}

NTSTATUS
DraidUnregisterClient(
	PDRAID_CLIENT_INFO Client
) {
	KIRQL oldIrql;
	ASSERT(g_DraidGlobals);
	ACQUIRE_SPIN_LOCK(&g_DraidGlobals->ClientListSpinlock, &oldIrql);
	RemoveEntryList(&Client->AllClientList);
	RELEASE_SPIN_LOCK(&g_DraidGlobals->ClientListSpinlock, oldIrql);
	return STATUS_SUCCESS;	
}

//
// Utility function
//

//
//
// Return TRUE if overlapped range exist
// All parameter is inclusive to range.
//
UINT32 DraidGetOverlappedRange(
	UINT64 Start1, UINT64 Length1,
	UINT64 Start2, UINT64 Length2,
	UINT64* OverlapStart, UINT64* OverlapEnd
) {
	UINT64 End1 = Start1 + Length1 -1;
	UINT64 End2 = Start2 + Length2 -1;
	if (End1 < Start2 || End2 < Start1) {
		*OverlapStart = 0;
		*OverlapEnd = 0;
		// No match
		return DRAID_RANGE_NO_OVERLAP;
	}
	// Full match
	if (Start2<=Start1 && End1<=End2) {
		*OverlapStart = Start1;
		*OverlapEnd = End1;
		return DRAID_RANGE_SRC2_CONTAINS_SRC1;
	} else if (Start1<=Start2 && End2<=End1) {
		*OverlapStart = Start2;
		*OverlapEnd = End2;
		return DRAID_RANGE_SRC1_CONTAINS_SRC2;
	}

	if (Start1 <= Start2 && Start2<=End1) {
		// Partial match at tail part of range 1
		*OverlapStart = Start2;
		*OverlapEnd = End1;
		return DRAID_RANGE_SRC1_TAIL_OVERLAP;
	}
	if (Start2 <= Start1 && Start1 <=End2) {
		// Partial match at head part of range 1
		*OverlapStart = Start1;
		*OverlapEnd  = End2;
		return DRAID_RANGE_SRC1_HEAD_OVERLAP;
	}
	ASSERT(FALSE); // this should not happen
	return DRAID_RANGE_NO_OVERLAP;
}

