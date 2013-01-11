#define	__NETDISK_MANAGER__
#define	__PRIMARY__
#include "LfsProc.h"


VOID
PrimaryAgentThreadProc (
	IN 	PPRIMARY	Primary
	);

NTSTATUS
BindListenSockets (
	IN 	PPRIMARY	Primary
	);

VOID
CloseListenSockets (
	IN 	PPRIMARY	Primary,
	IN  BOOLEAN		ConnectionOlny
	);

NTSTATUS
MakeConnectionObject (
    HANDLE						AddressFileHandle,
    PFILE_OBJECT				AddressFileObject,
	HANDLE						*ListenFileHandle,
	PFILE_OBJECT				*ListenFileObject,
	PLPXTDI_OVERLAPPED_CONTEXT	ListenOverlapped
	);

PPRIMARY_AGENT_REQUEST
AllocPrimaryAgentRequest (
	IN	BOOLEAN	Synchronous
	);

FORCEINLINE
NTSTATUS
QueueingPrimaryAgentRequest (
	IN 	PPRIMARY				Primary,
	IN	PPRIMARY_AGENT_REQUEST	PrimaryAgentRequest,
	IN  BOOLEAN					FastMutexAcquired
	);

VOID
DereferencePrimaryAgentRequest (
	IN	PPRIMARY_AGENT_REQUEST	PrimaryAgentRequest
	); 


NTSTATUS
LfsGeneralPassThroughCompletion (
	IN PDEVICE_OBJECT	DeviceObject,
	IN PIRP				Irp,
	IN PKEVENT			WaitEvent
	);


VOID
Primary_NetEvtCallback (
	PSOCKETLPX_ADDRESS_LIST	Original,
	PSOCKETLPX_ADDRESS_LIST	Updated,
	PSOCKETLPX_ADDRESS_LIST	Disabled,
	PSOCKETLPX_ADDRESS_LIST	Enabled,
	PVOID					Context
	) 
{
	PPRIMARY					Primary = (PPRIMARY)Context;
	PPRIMARY_AGENT_REQUEST		primaryAgentRequest;

#if DBG
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE, ("Original Address List\n"));
	DbgPrintLpxAddrList(LFS_DEBUG_PRIMARY_NOISE, Original);
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE, ("Updated Address List\n"));
	DbgPrintLpxAddrList(LFS_DEBUG_PRIMARY_NOISE, Updated);
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE, ("Disabled Address List\n"));
	DbgPrintLpxAddrList(LFS_DEBUG_PRIMARY_NOISE, Disabled);
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE, ("Enabled Address List\n"));
	DbgPrintLpxAddrList(LFS_DEBUG_PRIMARY_NOISE, Enabled);
#else
	UNREFERENCED_PARAMETER(Original);
#endif

	if (Disabled->iAddressCount) {

		primaryAgentRequest = AllocPrimaryAgentRequest(FALSE);
		
		if (primaryAgentRequest == NULL) {

			NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
			return;
		}

		primaryAgentRequest->RequestType = PRIMARY_AGENT_REQ_NIC_DISABLED;
		RtlCopyMemory(&primaryAgentRequest->AddressList, Disabled, sizeof(SOCKETLPX_ADDRESS_LIST));

		QueueingPrimaryAgentRequest( Primary, primaryAgentRequest, FALSE );
	}

	if (Enabled->iAddressCount) {

		primaryAgentRequest = AllocPrimaryAgentRequest(FALSE);

		if (primaryAgentRequest == NULL) {

			NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
			return;
		}

		primaryAgentRequest->RequestType = PRIMARY_AGENT_REQ_NIC_ENABLED;
		RtlCopyMemory(&primaryAgentRequest->AddressList, Enabled, sizeof(SOCKETLPX_ADDRESS_LIST));

		QueueingPrimaryAgentRequest( Primary, primaryAgentRequest, FALSE );

	} else {

		//
		//	Patch.
		//	Update all open addresses periodically
		//	in case of failure of address open.
		//

		if (Updated->iAddressCount) {

			primaryAgentRequest = AllocPrimaryAgentRequest(FALSE);

			if (primaryAgentRequest == NULL) {

				NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
				return;
			}

			primaryAgentRequest->RequestType = PRIMARY_AGENT_REQ_NIC_ENABLED;
			RtlCopyMemory(&primaryAgentRequest->AddressList, Updated, sizeof(SOCKETLPX_ADDRESS_LIST));

			QueueingPrimaryAgentRequest( Primary, primaryAgentRequest, FALSE );
		}
	}
}


PPRIMARY
Primary_Create (
	IN PLFS	Lfs
	)
{
	PPRIMARY			primary;
 	OBJECT_ATTRIBUTES	objectAttributes;
	LONG				i;
	NTSTATUS			ntStatus;
	LARGE_INTEGER		timeOut;


	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
				   ("Primary_Create: Entered\n"));

	LfsReference( Lfs );

	primary = ExAllocatePoolWithTag( NonPagedPool, 
									 sizeof(PRIMARY),
									 LFS_ALLOC_TAG );
	
	if (primary == NULL) {

		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
		LfsDereference( Lfs );

		return NULL;
	}

	RtlZeroMemory( primary, sizeof(PRIMARY) );

	ExInitializeFastMutex( &primary->FastMutex );
	primary->ReferenceCount	= 1;

	primary->Lfs = Lfs;

	for (i=0; i<MAX_SOCKETLPX_INTERFACE; i++) {

		InitializeListHead( &primary->PrimarySessionQueue[i] );
		KeInitializeSpinLock( &primary->PrimarySessionQSpinLock[i] );
	}

	primary->Agent.ThreadHandle = 0;
	primary->Agent.ThreadObject = NULL;

	primary->Agent.Flags = 0;

	KeInitializeEvent( &primary->Agent.ReadyEvent, NotificationEvent, FALSE );

	InitializeListHead( &primary->Agent.RequestQueue );
	KeInitializeSpinLock( &primary->Agent.RequestQSpinLock );
	KeInitializeEvent( &primary->Agent.RequestEvent, NotificationEvent, FALSE );

	
	primary->Agent.ListenPort = DEFAULT_PRIMARY_PORT;
	primary->Agent.ActiveListenSocketCount = 0;

	
	InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

	ntStatus = PsCreateSystemThread( &primary->Agent.ThreadHandle,
									 THREAD_ALL_ACCESS,
									 &objectAttributes,
									 NULL,
									 NULL,
									 PrimaryAgentThreadProc,
									 primary );
	
	if (!NT_SUCCESS(ntStatus)) {

		ASSERT( LFS_UNEXPECTED );
		Primary_Close( primary );
		
		return NULL;
	}

	ntStatus = ObReferenceObjectByHandle( primary->Agent.ThreadHandle,
										  FILE_READ_DATA,
										  NULL,
										  KernelMode,
										  &primary->Agent.ThreadObject,
										  NULL );

	if (!NT_SUCCESS(ntStatus)) {

		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
		Primary_Close( primary );
		
		return NULL;
	}

	timeOut.QuadPart = - LFS_TIME_OUT;
	ntStatus = KeWaitForSingleObject( &primary->Agent.ReadyEvent,
									  Executive,
									  KernelMode,
									  FALSE,
									  &timeOut );

	ASSERT( ntStatus == STATUS_SUCCESS );

	KeClearEvent( &primary->Agent.ReadyEvent );

	if (ntStatus != STATUS_SUCCESS) {

		ASSERT( LFS_BUG );
		Primary_Close( primary );
		
		return NULL;
	}
	
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
				   ("Primary_Create: primary = %p\n", primary) );

	return primary;
}


VOID
Primary_Close (
	IN PPRIMARY	Primary
	)
{
	ULONG		listenSocketIndex;
	

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
				   ("Primary_Close: Entered primary = %p\n", Primary) );

	for (listenSocketIndex = 0; listenSocketIndex < MAX_SOCKETLPX_INTERFACE; listenSocketIndex++) {

	    PLIST_ENTRY	primarySessionListEntry;
		BOOLEAN		found   = FALSE;


		if (Primary->Agent.ListenSocket[listenSocketIndex].Active != TRUE)
			continue;
		
		while (primarySessionListEntry = ExInterlockedRemoveHeadList(&Primary->PrimarySessionQueue[listenSocketIndex],
																	 &Primary->PrimarySessionQSpinLock[listenSocketIndex])) {

			PPRIMARY_SESSION primarySession;
		 
			primarySession = CONTAINING_RECORD (primarySessionListEntry, PRIMARY_SESSION, ListEntry);
			InitializeListHead( primarySessionListEntry );
			PrimarySession_Close( primarySession );
		}
	}

	if (Primary->Agent.ThreadHandle == NULL) {

		ASSERT(LFS_BUG);
		Primary_Dereference(Primary);

		return;
	}

	ASSERT( Primary->Agent.ThreadObject != NULL );

	if (Primary->Agent.Flags & PRIMARY_AGENT_FLAG_TERMINATED) {

		ObDereferenceObject( Primary->Agent.ThreadObject );

		Primary->Agent.ThreadHandle = NULL;
		Primary->Agent.ThreadObject = NULL;

	} else {

		PPRIMARY_AGENT_REQUEST		primaryAgentRequest;
		NTSTATUS					ntStatus;
		LARGE_INTEGER				timeOut;
	
		
		primaryAgentRequest = AllocPrimaryAgentRequest(FALSE);
		primaryAgentRequest->RequestType = PRIMARY_AGENT_REQ_DISCONNECT;

		QueueingPrimaryAgentRequest( Primary, primaryAgentRequest, FALSE );

		primaryAgentRequest = AllocPrimaryAgentRequest (FALSE);
		primaryAgentRequest->RequestType = PRIMARY_AGENT_REQ_DOWN;

		QueueingPrimaryAgentRequest( Primary, primaryAgentRequest, FALSE );

		timeOut.QuadPart = - LFS_TIME_OUT;
		ntStatus = KeWaitForSingleObject( Primary->Agent.ThreadObject,
										  Executive,
										  KernelMode,
										  FALSE,
										  &timeOut );

		ASSERT( ntStatus == STATUS_SUCCESS );

		if (ntStatus == STATUS_SUCCESS) {

		    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
							("Primary_Close: thread stoped\n") );

			ObDereferenceObject( Primary->Agent.ThreadObject );

			Primary->Agent.ThreadHandle = NULL;
			Primary->Agent.ThreadObject = NULL;
		
		} else {

			ASSERT( LFS_BUG );
			return;
		}
	}

	Primary_Dereference( Primary );

	return;
}


VOID
Primary_FileSystemShutdown (
	IN PPRIMARY	Primary
	)
{
	ULONG		listenSocketIndex;


	Primary_Reference( Primary );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
				   ("Primary_FileSystemShutdown: Entered primary = %p\n", Primary) );

	do {

		if (Primary->Agent.ThreadHandle == NULL) {

			ASSERT(LFS_BUG);
			break;
		}

		ASSERT( Primary->Agent.ThreadObject != NULL );

		if (Primary->Agent.Flags & PRIMARY_AGENT_FLAG_TERMINATED) {

			break;
		
		} else {

			PPRIMARY_AGENT_REQUEST	primaryAgentRequest;
			NTSTATUS				ntStatus;
			LARGE_INTEGER			timeOut;
	
		
			primaryAgentRequest = AllocPrimaryAgentRequest(TRUE);
			primaryAgentRequest->RequestType = PRIMARY_AGENT_REQ_SHUTDOWN;

			QueueingPrimaryAgentRequest( Primary, primaryAgentRequest, FALSE );

			timeOut.QuadPart = -LFS_TIME_OUT;		// 10 sec
			ntStatus = KeWaitForSingleObject( &primaryAgentRequest->CompleteEvent,
											  Executive,
											  KernelMode,
											  FALSE,
											  &timeOut );

			ASSERT( ntStatus == STATUS_SUCCESS );

			KeClearEvent( &primaryAgentRequest->CompleteEvent );

			if (ntStatus == STATUS_SUCCESS) {

			    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
								("Primary_FileSystemShutdown : exit \n") );
			
			} else {

				ASSERT( LFS_BUG );
				break;
			}
		}

	} while(0);
	
	for (listenSocketIndex = 0; listenSocketIndex < MAX_SOCKETLPX_INTERFACE; listenSocketIndex++) {

		if (Primary->Agent.ListenSocket[listenSocketIndex].Active != TRUE)
			continue;
		
		while (1) {

			KIRQL		oldIrql;
			BOOLEAN		found;
		    PLIST_ENTRY	primarySessionListEntry;

	
			KeAcquireSpinLock( &Primary->PrimarySessionQSpinLock[listenSocketIndex], &oldIrql );
			found = FALSE;

			for (primarySessionListEntry = Primary->PrimarySessionQueue[listenSocketIndex].Flink;
				 primarySessionListEntry != &Primary->PrimarySessionQueue[listenSocketIndex];
				 primarySessionListEntry = primarySessionListEntry->Flink) {

				PPRIMARY_SESSION primarySession;
		 
				primarySession = CONTAINING_RECORD( primarySessionListEntry, PRIMARY_SESSION, ListEntry );
		 
				RemoveEntryList( primarySessionListEntry );
			    KeReleaseSpinLock( &Primary->PrimarySessionQSpinLock[listenSocketIndex], oldIrql );
					
				InitializeListHead( primarySessionListEntry );

				PrimarySession_FileSystemShutdown( primarySession );

				found = TRUE;
				break;			
			}
			
			if (found == FALSE) {

				KeReleaseSpinLock( &Primary->PrimarySessionQSpinLock[listenSocketIndex], oldIrql );
				break;
			}
		} 
	}

	Primary_Dereference( Primary );

	return;
}


VOID
Primary_Reference (
	IN PPRIMARY	Primary
	)
{
    LONG result;
	
    result = InterlockedIncrement (&Primary->ReferenceCount);

    ASSERT (result >= 0);
}


VOID
Primary_Dereference (
	IN PPRIMARY	Primary
	)
{
    LONG result;


    result = InterlockedDecrement( &Primary->ReferenceCount );
    ASSERT ( result >= 0 );

    if (result == 0) {

		int i;


		LfsDereference( Primary->Lfs );

		for(i=0; i<MAX_SOCKETLPX_INTERFACE; i++)
			ASSERT( Primary->PrimarySessionQueue[i].Flink == &Primary->PrimarySessionQueue[i] );

		ExFreePoolWithTag( Primary, LFS_ALLOC_TAG );

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					   ("Primary_Dereference: Primary is Freed Primary = %p\n", Primary) );
	}
}


//
// To clean up terminated PrimarySession is performed here
//
VOID
Primary_AcceptConnection (
	IN PPRIMARY						Primary,
	IN HANDLE						ListenFileHandle,
	IN PFILE_OBJECT					ListenFileObject,
	IN PLPXTDI_OVERLAPPED_CONTEXT	ListenOverlapped,
	IN  ULONG						ListenSocketIndex,
	IN PLPX_ADDRESS					RemoteAddress
	)
{
    PLIST_ENTRY				listEntry;
	PPRIMARY_SESSION		newPrimarySession;

	newPrimarySession = PrimarySession_Create( Primary, 
											   ListenFileHandle, 
											   ListenFileObject,
											   ListenOverlapped,
											   ListenSocketIndex,
											   RemoteAddress );

	if (newPrimarySession == NULL) {

		LpxTdiV2Disconnect( ListenFileObject, 0 );
		LpxTdiV2DisassociateAddress( ListenFileObject );
		LpxTdiV2CloseConnection( ListenFileHandle, ListenFileObject, ListenOverlapped );
	}

		
	KeEnterCriticalRegion();
	ExAcquireFastMutex( &Primary->FastMutex );

	listEntry = Primary->PrimarySessionQueue[ListenSocketIndex].Flink;
	
	while (listEntry != &Primary->PrimarySessionQueue[ListenSocketIndex]) {

		PPRIMARY_SESSION primarySession;
	

		primarySession = CONTAINING_RECORD (listEntry, PRIMARY_SESSION, ListEntry);			
		listEntry = listEntry->Flink;

		if (primarySession->Thread.Flags & PRIMARY_SESSION_THREAD_FLAG_TERMINATED) {

			ExReleaseFastMutex( &Primary->FastMutex );
			KeLeaveCriticalRegion();

			PrimarySession_Close(primarySession);

			KeEnterCriticalRegion();
			ExAcquireFastMutex( &Primary->FastMutex );
		}
	}

	ExReleaseFastMutex( &Primary->FastMutex );
	KeLeaveCriticalRegion();

	return;
}


static
NTSTATUS
PrimaryOpenOneListenSocket (
		PPRIMARY_LISTEN_SOCKET	listenSock,
		PLPX_ADDRESS			NICAddr
	) 
{
	NTSTATUS			ntStatus;
	HANDLE				addressFileHandle = NULL;
	PFILE_OBJECT		addressFileObject = NULL;
	HANDLE				listenFileHandle = NULL;
	PFILE_OBJECT		listenFileObject = NULL;
	PLPXTDI_OVERLAPPED_CONTEXT	listenOverlapped = NULL;
	

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE, ("PrimaryOpenOneSocket: Entered\n") );

	listenSock->Active = FALSE;

	RtlCopyMemory( listenSock->NICAddress.Node,
				   NICAddr->Node,
				   ETHER_ADDR_LENGTH );

	//
	//	open a address.
	//

	ntStatus = LpxTdiV2OpenAddress( NICAddr, &addressFileHandle, &addressFileObject );

	if (!NT_SUCCESS(ntStatus)) {

	  if (ntStatus != 0xC001001FL/*NDIS_STATUS_NO_CABLE*/)
		ASSERT( LFS_LPX_BUG );
		
	  return ntStatus;
	}
		
	ntStatus = MakeConnectionObject( addressFileHandle, 
									 addressFileObject, 
									 &listenFileHandle, 
									 &listenFileObject, 
									 &listenSock->ListenOverlapped );
	
	if (!NT_SUCCESS(ntStatus)) {

		ASSERT( LFS_LPX_BUG );
		LpxTdiV2CloseAddress( addressFileHandle, addressFileObject );

		listenSock->Active = FALSE;
	
		return STATUS_UNSUCCESSFUL;
	}

	listenSock->Active = TRUE;

	listenSock->AddressFileHandle = addressFileHandle;
	listenSock->AddressFileObject = addressFileObject;

	listenSock->ListenFileHandle = listenFileHandle;
	listenSock->ListenFileObject = listenFileObject;
		
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
				   ("PrimaryOpenOneListenSocket: opened a address:'%02X:%02X:%02X:%02X:%02X:%02X 0x%04X'\n",
					 NICAddr->Node[0],NICAddr->Node[1],NICAddr->Node[2],
					 NICAddr->Node[3],NICAddr->Node[4],NICAddr->Node[5],
					 NTOHS(NICAddr->Port)) );

	listenSock->Flags = TDI_QUERY_ACCEPT;

	ntStatus = LpxTdiV2Listen( listenSock->ListenFileObject,
							   &listenSock->RequestOptions,
							   &listenSock->ReturnOptions,
							   listenSock->Flags,
							   NULL,
							   &listenSock->ListenOverlapped,
							   NULL );

	if (!NT_SUCCESS(ntStatus)) {

		ASSERT( LFS_LPX_BUG );
		LpxTdiV2CompleteRequest( &listenSock->ListenOverlapped, 0 );

		LpxTdiV2DisassociateAddress( listenFileObject );
		LpxTdiV2CloseConnection( listenFileHandle, listenFileObject, &listenSock->ListenOverlapped );
		LpxTdiV2CloseAddress ( addressFileHandle, addressFileObject );

		listenSock->ListenFileHandle = NULL;
		listenSock->ListenFileObject = NULL;

		listenSock->AddressFileHandle = NULL;
		listenSock->AddressFileObject = NULL;

		listenSock->Active = FALSE;
		ntStatus = STATUS_UNSUCCESSFUL;
	}

	return ntStatus;
}


static
VOID
PrimaryAgentNICDisabled (
		PPRIMARY	Primary,
		PSOCKETLPX_ADDRESS_LIST AddressList
	) {

	LONG		idx_listen;
	LONG		idx_disabled;
	BOOLEAN		found;

	for(idx_disabled = 0; idx_disabled < AddressList->iAddressCount; idx_disabled ++ ) {

		found = FALSE;
		for(idx_listen = 0; idx_listen < MAX_SOCKETLPX_INTERFACE; idx_listen ++) {
			//
			//	find the match
			//
			if(Primary->Agent.ListenSocket[idx_listen].Active &&
				RtlCompareMemory(
					AddressList->SocketLpx[idx_disabled].LpxAddress.Node,
					Primary->Agent.ListenSocket[idx_listen].NICAddress.Node,
					ETHER_ADDR_LENGTH
					) == ETHER_ADDR_LENGTH ) {

				found = TRUE;
				break;
			}
		}

		//
		//	delete disabled one if found.
		//
		if (found) {

			PPRIMARY_LISTEN_SOCKET	listenSock = Primary->Agent.ListenSocket + idx_listen;

			listenSock->Active = FALSE;

			NDAS_ASSERT( LpxTdiV2IsRequestPending(&listenSock->ListenOverlapped, 0) );

			if (LpxTdiV2IsRequestPending(&listenSock->ListenOverlapped, 0)) {

				LpxTdiV2CancelRequest( listenSock->ListenFileObject, &listenSock->ListenOverlapped, 0, FALSE, 0 );
			}

			LpxTdiV2DisassociateAddress(listenSock->ListenFileObject);
			LpxTdiV2CloseConnection(listenSock->ListenFileHandle, listenSock->ListenFileObject , &listenSock->ListenOverlapped );
			
			LpxTdiV2CloseAddress (listenSock->AddressFileHandle, listenSock->AddressFileObject);

			listenSock->ListenFileHandle = NULL;
			listenSock->ListenFileObject = NULL;

			listenSock->AddressFileHandle = NULL;
			listenSock->AddressFileObject = NULL;

			Primary->Agent.ActiveListenSocketCount --;

			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
						   ("PrimaryAgentNICEnabled: A NIC deleted..\n") );
		}
	}
}


static
VOID
PrimaryAgentNICEnabled (
		PPRIMARY	Primary,
		PSOCKETLPX_ADDRESS_LIST AddressList) {

	LONG		idx_listen;
	LONG		idx_enabled;
	BOOLEAN		found;
	LONG		available;
	NTSTATUS	ntStatus;

	for(idx_enabled = 0; idx_enabled < AddressList->iAddressCount; idx_enabled ++ ) {

		found = FALSE;
		for(idx_listen = 0; idx_listen < MAX_SOCKETLPX_INTERFACE; idx_listen ++) {
			//
			//	find the match
			//
			if(Primary->Agent.ListenSocket[idx_listen].Active &&
				RtlCompareMemory(
					AddressList->SocketLpx[idx_enabled].LpxAddress.Node,
					Primary->Agent.ListenSocket[idx_listen].NICAddress.Node,
					ETHER_ADDR_LENGTH
					) == ETHER_ADDR_LENGTH ) {

				found = TRUE;
				break;
			}
		}

		//
		//	add enabled one if not found.
		//
		if(!found) {
			//
			//	find available slot.
			//
			available = -1;
			for(idx_listen = 0; idx_listen < MAX_SOCKETLPX_INTERFACE; idx_listen ++) {
				if(!Primary->Agent.ListenSocket[idx_listen].Active) {
					available = idx_listen;
					break;
				}
			}

			if(available >= 0) {
				PPRIMARY_LISTEN_SOCKET	listenSock = Primary->Agent.ListenSocket + available;
				LPX_ADDRESS				NICAddr;

				//
				//	open a new listen connection.
				//
				RtlCopyMemory(NICAddr.Node, AddressList->SocketLpx[idx_enabled].LpxAddress.Node, ETHER_ADDR_LENGTH);
				NICAddr.Port = HTONS(Primary->Agent.ListenPort);

				ntStatus = PrimaryOpenOneListenSocket(
						listenSock,
						&NICAddr
					);

				if(NT_SUCCESS(ntStatus)) {

					SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
						("PrimaryAgentNICEnabled: new NIC added.\n"));

					Primary->Agent.ActiveListenSocketCount ++;
				}

			} else {
				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					("PrimaryAgentNICEnabled: No available socket slot.\n"));
			}
		}

	}
}


VOID
PrimaryAgentThreadProc (
	IN 	PPRIMARY	Primary
	)
{
	BOOLEAN			primaryAgentThreadExit = FALSE;
	PKWAIT_BLOCK	waitBlocks;
	ULONG			i;


	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
				   ("PrimaryAgentThreadProc: Start\n") );

	Primary->Agent.Flags = PRIMARY_AGENT_FLAG_INITIALIZING;
	
	//
	// Allocate wait block
	//

	waitBlocks = ExAllocatePoolWithTag( NonPagedPool, sizeof(KWAIT_BLOCK) * MAXIMUM_WAIT_OBJECTS, LFS_ALLOC_TAG );
	
	if (waitBlocks == NULL) {

		ASSERT(LFS_REQUIRED);
		PsTerminateSystemThread( STATUS_INSUFFICIENT_RESOURCES );
	}

	for (i=0; i<MAX_SOCKETLPX_INTERFACE; i++) {

		Primary->Agent.ListenSocket[i].Active = FALSE;
		KeInitializeEvent( &Primary->Agent.ListenSocket[i].CompletionEvent, NotificationEvent, FALSE );
	}

	BindListenSockets( Primary );

	Primary->Agent.Flags |= PRIMARY_AGENT_FLAG_START;
				
	KeSetEvent( &Primary->Agent.ReadyEvent, IO_DISK_INCREMENT, FALSE );

	while (primaryAgentThreadExit == FALSE) {

		PKEVENT				events[MAXIMUM_WAIT_OBJECTS];
		LONG				eventCnt;

		LARGE_INTEGER		timeOut;
		NTSTATUS			ntStatus;
		PLIST_ENTRY			primaryAgentRequestEntry;


		ASSERT(MAX_SOCKETLPX_INTERFACE + 1 <= MAXIMUM_WAIT_OBJECTS);

		eventCnt = 0;
		events[eventCnt++] = &Primary->Agent.RequestEvent;

		if (!FlagOn(Primary->Agent.Flags, PRIMARY_AGENT_FLAG_SHUTDOWN)) {

			for (i=0; i<MAX_SOCKETLPX_INTERFACE; i++) {

				if (Primary->Agent.ListenSocket[i].Active) {

					events[eventCnt++] = &Primary->Agent.ListenSocket[i].ListenOverlapped.Request[0].CompletionEvent;
				
				} else {

					// events[eventCnt++] = NULL; // I wanna set NULL, But It's not Work
					events[eventCnt++] = &Primary->Agent.ListenSocket[i].CompletionEvent;
				}
			}

			ASSERT( eventCnt == MAX_SOCKETLPX_INTERFACE + 1 );
		}

		timeOut.QuadPart = - 5 * NANO100_PER_SEC;
		ntStatus = KeWaitForMultipleObjects( eventCnt,
											 events,
											 WaitAny,
											 Executive,
											 KernelMode,
											 TRUE,
											 &timeOut,
											 waitBlocks );

		if (ntStatus == STATUS_TIMEOUT) {

			continue;
		}

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
					   ("PrimaryAgentThreadProc: NTSTATUS:%lu\n", ntStatus) );
		

		if (!NT_SUCCESS(ntStatus) || ntStatus >= eventCnt) {

			ASSERT( LFS_UNEXPECTED );
			SetFlag( Primary->Agent.Flags, PRIMARY_AGENT_FLAG_ERROR );

			primaryAgentThreadExit = TRUE;

			continue;
		}

		if (0 == ntStatus) {

			KeClearEvent(events[ntStatus]);

			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
						   ("PrimaryAgentThreadProc: RequestEvent received\n") );

			while (primaryAgentRequestEntry = ExInterlockedRemoveHeadList(&Primary->Agent.RequestQueue,
																		  &Primary->Agent.RequestQSpinLock)) {

				PPRIMARY_AGENT_REQUEST		primaryAgentRequest;

				primaryAgentRequest = CONTAINING_RECORD( primaryAgentRequestEntry,
														 PRIMARY_AGENT_REQUEST,
														 ListEntry );
	
				switch (primaryAgentRequest->RequestType) {

				case PRIMARY_AGENT_REQ_DISCONNECT: {

					CloseListenSockets( Primary, FALSE );
				
					break;
				}

				case PRIMARY_AGENT_REQ_SHUTDOWN: {

					CloseListenSockets( Primary, TRUE );

					SetFlag( Primary->Agent.Flags, PRIMARY_AGENT_FLAG_SHUTDOWN );
					break;
				}

				case PRIMARY_AGENT_REQ_DOWN: 

					ExAcquireFastMutex( &Primary->FastMutex );		
					SetFlag( Primary->Agent.Flags, PRIMARY_AGENT_FLAG_STOPED );
					ExReleaseFastMutex( &Primary->FastMutex );

					primaryAgentThreadExit = TRUE;
					break;

				//
				//	added to adapt network card changes.
				//

				case PRIMARY_AGENT_REQ_NIC_DISABLED:
				
					SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
								   ("PrimaryAgentThreadProc: PRIMARY_AGENT_REQ_NIC_DISABLED\n") );

					PrimaryAgentNICDisabled( Primary, &primaryAgentRequest->AddressList );
				
					break;

				case PRIMARY_AGENT_REQ_NIC_ENABLED:
				
					SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
								   ("PrimaryAgentThreadProc: PRIMARY_AGENT_REQ_NIC_ENABLED\n") );

					PrimaryAgentNICEnabled( Primary, &primaryAgentRequest->AddressList );
				
					break;

				default:
		
					ASSERT( LFS_BUG );
					
					SetFlag( Primary->Agent.Flags, PRIMARY_AGENT_FLAG_ERROR );

					break;
				}

				if (primaryAgentRequest->Synchronous == TRUE) {

					KeSetEvent( &primaryAgentRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
				
				} else {

					DereferencePrimaryAgentRequest( primaryAgentRequest);
				}
			}

			continue;	
		}

		ASSERT(1 <= ntStatus && ntStatus < eventCnt); // LpxEvent 
		
		if (1 <= ntStatus && ntStatus <= MAX_SOCKETLPX_INTERFACE) {

			NTSTATUS				tdiStatus;

			HANDLE					listenFileHandle;
			PFILE_OBJECT			listenFileObject;

			HANDLE						connFileHandle;
			PFILE_OBJECT				connFileObject;
			LPXTDI_OVERLAPPED_CONTEXT	connOverlapped;

			PPRIMARY_LISTEN_SOCKET	listenSocket;
			LPX_ADDRESS				remoteAddress;

			PTRANSPORT_ADDRESS	tpAddr;
			PTA_ADDRESS			taAddr;
			PLPX_ADDRESS		lpxAddr;

			listenSocket = &Primary->Agent.ListenSocket[ntStatus-1];

			if (listenSocket->Active == FALSE) {

				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
							   ("ListenSocket is not active. Maybe a NIC disabled.\n") );
				
				NDAS_ASSERT( !LpxTdiV2IsRequestPending(&listenSocket->ListenOverlapped, 0) );
				continue;
			}

			LpxTdiV2CompleteRequest( &listenSocket->ListenOverlapped, 0 );
			
			if (listenSocket->ListenOverlapped.Request[0].IoStatusBlock.Status != STATUS_SUCCESS) {

				LpxTdiV2DisassociateAddress( listenSocket->ListenFileObject );
				LpxTdiV2CloseConnection( listenSocket->ListenFileHandle, listenSocket->ListenFileObject, 
										 &listenSocket->ListenOverlapped );

				listenSocket->ListenFileHandle = NULL;
				listenSocket->ListenFileObject = NULL;

				LpxTdiV2CloseAddress( listenSocket->AddressFileHandle, 
									  listenSocket->AddressFileObject );

				listenSocket->AddressFileHandle = NULL;
				listenSocket->AddressFileObject = NULL;

				listenSocket->Active = FALSE;
				Primary->Agent.ActiveListenSocketCount --;

				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
							   ("Listen IRP #%d failed.\n", ntStatus) );

				continue;
			}

			connFileHandle	= listenSocket->ListenFileHandle;
			connFileObject	= listenSocket->ListenFileObject;

			LpxTdiV2MoveOverlappedContext( &connOverlapped, &listenSocket->ListenOverlapped );

			listenSocket->ListenFileHandle = NULL;
			listenSocket->ListenFileObject = NULL;

			tpAddr = (PTRANSPORT_ADDRESS)connOverlapped.Request[0].AddressBuffer;
			taAddr = tpAddr->Address;
			lpxAddr = (PLPX_ADDRESS)taAddr->Address;

			RtlCopyMemory( &remoteAddress, lpxAddr, sizeof(LPX_ADDRESS) );

			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
						   ("PrimaryAgentThreadProc: Connect from %02X:%02X:%02X:%02X:%02X:%02X 0x%4X\n",
							remoteAddress.Node[0], remoteAddress.Node[1],
							remoteAddress.Node[2], remoteAddress.Node[3],
							remoteAddress.Node[4], remoteAddress.Node[5],
							NTOHS(remoteAddress.Port)) );

			//
			//	Make a new listen connection first of all to get another connection.
			//	It must be earlier than start a session that takes long time.
			//	Primary cannot accept a connection before it creates a new listen object.
			//

			tdiStatus = MakeConnectionObject( listenSocket->AddressFileHandle,
											  listenSocket->AddressFileObject,
											  &listenFileHandle,
											  &listenFileObject,
											  &listenSocket->ListenOverlapped );

			if (!NT_SUCCESS(tdiStatus)) {

				ASSERT(LFS_LPX_BUG);
				
				LpxTdiV2CloseAddress( listenSocket->AddressFileHandle, 
									listenSocket->AddressFileObject );

				listenSocket->AddressFileHandle = NULL;
				listenSocket->AddressFileObject = NULL;

				listenSocket->Active = FALSE;

				Primary->Agent.ActiveListenSocketCount --;
	
				goto start_session;
			}

			listenSocket->ListenFileHandle = listenFileHandle;
			listenSocket->ListenFileObject = listenFileObject;

			listenSocket->Flags	= TDI_QUERY_ACCEPT;
			tdiStatus = LpxTdiV2Listen( listenSocket->ListenFileObject,
									    &listenSocket->RequestOptions,
										&listenSocket->ReturnOptions,
										listenSocket->Flags,
										NULL,
										&listenSocket->ListenOverlapped,
										NULL );

			if (!NT_SUCCESS(tdiStatus)) {

				ASSERT( LFS_LPX_BUG );

				LpxTdiV2CompleteRequest( &listenSocket->ListenOverlapped, 0 );
			
				LpxTdiV2DisassociateAddress( listenFileObject );
				LpxTdiV2CloseConnection( listenFileHandle, listenFileObject, &listenSocket->ListenOverlapped );
				LpxTdiV2CloseAddress ( listenSocket->AddressFileHandle, listenSocket->AddressFileObject );
				listenSocket->Active = FALSE;
				Primary->Agent.ActiveListenSocketCount --;
			}

start_session:

			//
			//	start a session.
			//
			Primary_AcceptConnection( Primary, 
									  connFileHandle, 
									  connFileObject, 
									  &connOverlapped, 
									  ntStatus-1, 
									  &remoteAddress );

			continue;
		}
	}

	//
	// Free wait blocks
	//

	ExFreePool( waitBlocks );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
				   ("PrimaryAgentThreadProc: PsTerminateSystemThread\n") );
	
	Primary->Agent.Flags |= PRIMARY_AGENT_FLAG_TERMINATED;

	PsTerminateSystemThread( STATUS_SUCCESS );
}


NTSTATUS
BindListenSockets (
	IN 	PPRIMARY	Primary
	)
{
	NTSTATUS				ntStatus;
	PSOCKETLPX_ADDRESS_LIST	socketLpxAddressList;
	LONG					idx_addr;


	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
				   ("BindListenSockets: Entered\n") );

	Primary->Agent.ActiveListenSocketCount = 0;
	socketLpxAddressList = &Primary->Agent.SocketLpxAddressList;
	ntStatus = LpxTdiV2GetAddressList( socketLpxAddressList );

	if (!NT_SUCCESS(ntStatus)) {

		//ASSERT(LFS_LPX_BUG);
		return	ntStatus;
	}
	
	if (socketLpxAddressList->iAddressCount <= 0) {

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					   ( "BindListenSockets: No NICs in the host.\n") );

		return STATUS_UNSUCCESSFUL;
	}
	
	if (socketLpxAddressList->iAddressCount > MAX_SOCKETLPX_INTERFACE)
		socketLpxAddressList->iAddressCount = MAX_SOCKETLPX_INTERFACE;

	for (idx_addr = 0; idx_addr < socketLpxAddressList->iAddressCount; idx_addr ++) {

		LPX_ADDRESS		NICAddr;
	
		HANDLE			addressFileHandle = NULL;
		PFILE_OBJECT	addressFileObject = NULL;

		HANDLE				listenFileHandle = NULL;
		PFILE_OBJECT		listenFileObject = NULL;
		PLPXTDI_OVERLAPPED_CONTEXT	listenOverlapped = NULL;
		
		
		Primary->Agent.ListenSocket[idx_addr].Active = FALSE;

		if( (0 == socketLpxAddressList->SocketLpx[idx_addr].LpxAddress.Node[0]) &&
			(0 == socketLpxAddressList->SocketLpx[idx_addr].LpxAddress.Node[1]) &&
			(0 == socketLpxAddressList->SocketLpx[idx_addr].LpxAddress.Node[2]) &&
			(0 == socketLpxAddressList->SocketLpx[idx_addr].LpxAddress.Node[3]) &&
			(0 == socketLpxAddressList->SocketLpx[idx_addr].LpxAddress.Node[4]) &&
			(0 == socketLpxAddressList->SocketLpx[idx_addr].LpxAddress.Node[5]) ) {

			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
						   ( "BindListenSockets: We don't use SocketLpx device.\n")) ;
			continue;
		}

		RtlCopyMemory( &Primary->Agent.ListenSocket[idx_addr].NICAddress,
					   &socketLpxAddressList->SocketLpx[idx_addr].LpxAddress, 
					   sizeof(LPX_ADDRESS) );

		RtlCopyMemory( &NICAddr, 
					   &socketLpxAddressList->SocketLpx[idx_addr].LpxAddress, 
					   sizeof(LPX_ADDRESS) );

		NICAddr.Port = HTONS(Primary->Agent.ListenPort);
	
		//
		//	open a address.
		//

		ntStatus = LpxTdiV2OpenAddress( &NICAddr,
										&addressFileHandle,
									    &addressFileObject );

		if (!NT_SUCCESS(ntStatus)) {

			if (ntStatus != 0xC001001FL/*NDIS_STATUS_NO_CABLE*/)
				ASSERT( LFS_LPX_BUG );
	
			continue;
		}
		
		ntStatus = MakeConnectionObject( addressFileHandle, 
										 addressFileObject, 
										 &listenFileHandle, 
										 &listenFileObject, 
										 &Primary->Agent.ListenSocket[idx_addr].ListenOverlapped );
	
		if (!NT_SUCCESS(ntStatus)) {

			ASSERT( LFS_LPX_BUG );
			LpxTdiV2CloseAddress( addressFileHandle, addressFileObject );

			Primary->Agent.ListenSocket[idx_addr].Active = FALSE;	
			continue;
		}

		Primary->Agent.ActiveListenSocketCount ++;

		Primary->Agent.ListenSocket[idx_addr].Active = TRUE;

		Primary->Agent.ListenSocket[idx_addr].AddressFileHandle = addressFileHandle;
		Primary->Agent.ListenSocket[idx_addr].AddressFileObject = addressFileObject;

		Primary->Agent.ListenSocket[idx_addr].ListenFileHandle = listenFileHandle;
		Primary->Agent.ListenSocket[idx_addr].ListenFileObject = listenFileObject;
		
		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					   ("BindListenSockets: opened a address:'%02X:%02X:%02X:%02X:%02X:%02X 0x%04X'\n",
						NICAddr.Node[0],NICAddr.Node[1],NICAddr.Node[2],
						NICAddr.Node[3],NICAddr.Node[4],NICAddr.Node[5],
						NTOHS(NICAddr.Port)) );

		Primary->Agent.ListenSocket[idx_addr].Flags	= TDI_QUERY_ACCEPT;

		ntStatus = LpxTdiV2Listen( Primary->Agent.ListenSocket[idx_addr].ListenFileObject,
								   &Primary->Agent.ListenSocket[idx_addr].RequestOptions,
								   &Primary->Agent.ListenSocket[idx_addr].ReturnOptions,
								   Primary->Agent.ListenSocket[idx_addr].Flags,
								   NULL,
								   &Primary->Agent.ListenSocket[idx_addr].ListenOverlapped,
								   NULL );

		if (!NT_SUCCESS(ntStatus)) {

			ASSERT( LFS_LPX_BUG );

			LpxTdiV2CompleteRequest( &Primary->Agent.ListenSocket[idx_addr].ListenOverlapped, 0 );

			LpxTdiV2DisassociateAddress( listenFileObject );
			LpxTdiV2CloseConnection( listenFileHandle, listenFileObject, &Primary->Agent.ListenSocket[idx_addr].ListenOverlapped );
			LpxTdiV2CloseAddress( addressFileHandle, addressFileObject );

			Primary->Agent.ListenSocket[idx_addr].ListenFileHandle = NULL;
			Primary->Agent.ListenSocket[idx_addr].ListenFileObject = NULL;

			Primary->Agent.ListenSocket[idx_addr].AddressFileHandle = NULL;
			Primary->Agent.ListenSocket[idx_addr].AddressFileObject = NULL;

			Primary->Agent.ListenSocket[idx_addr].Active = FALSE;
			Primary->Agent.ActiveListenSocketCount --;
	
			continue;
		}
	}

	return STATUS_SUCCESS;
}


VOID
CloseListenSockets (
	IN 	PPRIMARY	Primary,
	IN  BOOLEAN		ConnectionOlny
	)
{
	ULONG	i;

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
				   ("CloseListenSockets: Entered\n") );

	for (i = 0; i < MAX_SOCKETLPX_INTERFACE; i++) {

		if (Primary->Agent.ListenSocket[i].Active != TRUE)
			continue;

		NDAS_ASSERT( Primary->Agent.ListenSocket[i].AddressFileObject != NULL );
		NDAS_ASSERT( Primary->Agent.ListenSocket[i].ListenFileObject != NULL );

		LpxTdiV2CancelRequest( Primary->Agent.ListenSocket[i].ListenFileObject, 
							   &Primary->Agent.ListenSocket[i].ListenOverlapped, 
							   0,
							   FALSE, 
							   0 );

		LpxTdiV2Disconnect( Primary->Agent.ListenSocket[i].ListenFileObject, 0 );
		LpxTdiV2DisassociateAddress( Primary->Agent.ListenSocket[i].ListenFileObject );
		LpxTdiV2CloseConnection( Primary->Agent.ListenSocket[i].ListenFileHandle, 
							     Primary->Agent.ListenSocket[i].ListenFileObject,
							     &Primary->Agent.ListenSocket[i].ListenOverlapped );

		Primary->Agent.ListenSocket[i].ListenFileHandle = NULL;
		Primary->Agent.ListenSocket[i].ListenFileObject = NULL;

		if (ConnectionOlny == FALSE) {

			LpxTdiV2CloseAddress( Primary->Agent.ListenSocket[i].AddressFileHandle,
								Primary->Agent.ListenSocket[i].AddressFileObject );

			Primary->Agent.ListenSocket[i].AddressFileObject = NULL;
			Primary->Agent.ListenSocket[i].AddressFileHandle = NULL;

			Primary->Agent.ListenSocket[i].Active = FALSE;		
			Primary->Agent.ActiveListenSocketCount --;
		}

	}

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
				   ("CloseListenSockets: Returned\n") );

	return;
}

				
NTSTATUS
MakeConnectionObject (
    HANDLE						AddressFileHandle,
    PFILE_OBJECT				AddressFileObject,
	HANDLE						*ListenFileHandle,
	PFILE_OBJECT				*ListenFileObject,
	PLPXTDI_OVERLAPPED_CONTEXT	ListenOverlapped
	)
{
	NTSTATUS		ntStatus;

	HANDLE			listenFileHandle;
	PFILE_OBJECT	listenFileObject;


	UNREFERENCED_PARAMETER(AddressFileObject);

	ntStatus = LpxTdiV2OpenConnection( NULL,
									   &listenFileHandle,
									   &listenFileObject,
									   ListenOverlapped );

	if (!NT_SUCCESS(ntStatus)) {

		ASSERT(LFS_LPX_BUG);

		*ListenFileHandle = NULL;
		*ListenFileObject = NULL;
		
		return ntStatus;
	}

	ntStatus = LpxTdiV2AssociateAddress( listenFileObject,
									     AddressFileHandle );

	if (!NT_SUCCESS(ntStatus)) {

		ASSERT( LFS_LPX_BUG );

		LpxTdiV2CloseConnection( listenFileHandle, listenFileObject, ListenOverlapped );

		*ListenFileHandle = NULL;
		*ListenFileObject = NULL;
		
		return ntStatus;
	}

	*ListenFileHandle  = listenFileHandle;
	*ListenFileObject  = listenFileObject;

	return ntStatus;
}


PPRIMARY_AGENT_REQUEST
AllocPrimaryAgentRequest (
	IN	BOOLEAN	Synchronous
	) 
{
	PPRIMARY_AGENT_REQUEST	primaryAgentRequest;


	primaryAgentRequest = ExAllocatePoolWithTag( NonPagedPool,
												 sizeof(PRIMARY_AGENT_REQUEST),
												 PRIMARY_AGENT_MESSAGE_TAG );

	if (primaryAgentRequest == NULL) {

		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
		return NULL;
	}

	RtlZeroMemory( primaryAgentRequest, sizeof(PRIMARY_AGENT_REQUEST) );

	primaryAgentRequest->ReferenceCount = 1;
	InitializeListHead( &primaryAgentRequest->ListEntry );
	
	primaryAgentRequest->Synchronous = Synchronous;
	KeInitializeEvent( &primaryAgentRequest->CompleteEvent, NotificationEvent, FALSE );


	return primaryAgentRequest;
}


FORCEINLINE
NTSTATUS
QueueingPrimaryAgentRequest (
	IN 	PPRIMARY				Primary,
	IN	PPRIMARY_AGENT_REQUEST	PrimaryAgentRequest,
	IN  BOOLEAN					FastMutexAcquired
	)
{
	NTSTATUS	status;


	ASSERT( PrimaryAgentRequest->ListEntry.Flink == PrimaryAgentRequest->ListEntry.Blink );

	if (FastMutexAcquired == FALSE)
		ExAcquireFastMutex( &Primary->FastMutex );

	if (FlagOn(Primary->Agent.Flags, PRIMARY_AGENT_FLAG_START) &&
		!FlagOn(Primary->Agent.Flags, PRIMARY_AGENT_FLAG_STOPED)) {

		ExInterlockedInsertTailList( &Primary->Agent.RequestQueue,
									 &PrimaryAgentRequest->ListEntry,
									 &Primary->Agent.RequestQSpinLock );

		KeSetEvent( &Primary->Agent.RequestEvent, IO_DISK_INCREMENT, FALSE );
		status = STATUS_SUCCESS;

	} else {

		status = STATUS_UNSUCCESSFUL;
	}

	if (FastMutexAcquired == FALSE)
		ExReleaseFastMutex( &Primary->FastMutex );

	if (status == STATUS_UNSUCCESSFUL) {
	
		if (PrimaryAgentRequest->Synchronous == TRUE)
			KeSetEvent( &PrimaryAgentRequest->CompleteEvent, IO_DISK_INCREMENT, FALSE );
		else
			DereferencePrimaryAgentRequest( PrimaryAgentRequest );
	}

	return status;
}


VOID
DereferencePrimaryAgentRequest (
	IN	PPRIMARY_AGENT_REQUEST	PrimaryAgentRequest
	) 
{
	LONG	result;

	result = InterlockedDecrement( &PrimaryAgentRequest->ReferenceCount );

	ASSERT( result >= 0 );

	if (0 == result) {

		ExFreePoolWithTag( PrimaryAgentRequest, PRIMARY_AGENT_MESSAGE_TAG );
		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,	("DereferencePrimarySessionRequest: PrimaryAgentRequest freed\n") );
	}

	return;
}

BOOLEAN
LfsGeneralPassThrough (
    IN  PDEVICE_OBJECT				DeviceObject,
    IN  PIRP						Irp,
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	OUT PNTSTATUS					NtStatus
	)
{
	NTSTATUS			status = STATUS_SUCCESS;
	BOOLEAN				result = FALSE;
    PIO_STACK_LOCATION	irpSp = IoGetCurrentIrpStackLocation( Irp );
	PFILE_OBJECT		fileObject = irpSp->FileObject;
	BOOLEAN				fastMutexAcquired = FALSE;


	UNREFERENCED_PARAMETER( DeviceObject );

	ASSERT( DevExt->LfsDeviceExt.ReferenceCount );
	LfsDeviceExt_Reference( &DevExt->LfsDeviceExt );	

	PrintIrp( LFS_DEBUG_PRIMARY_NOISE, __FUNCTION__, &DevExt->LfsDeviceExt, Irp );

	ASSERT( KeGetCurrentIrql() <= APC_LEVEL );

	ExAcquireFastMutex( &DevExt->LfsDeviceExt.FastMutex );
	fastMutexAcquired = TRUE;

	try {

		if (!FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTED)) {

			result = FALSE;
			leave;
		}

		ASSERT( DevExt->LfsDeviceExt.AttachedToDeviceObject == DevExt->NLExtHeader.AttachedToDeviceObject );

		if (DevExt->LfsDeviceExt.AttachedToDeviceObject == NULL) {

			NDAS_ASSERT( FALSE );

			result = FALSE;
			leave;
		}

		if (!(FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTED) && !FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED))) {

			if (DevExt->LfsDeviceExt.Secondary != NULL && 
				Secondary_LookUpCcb(DevExt->LfsDeviceExt.Secondary, fileObject) != NULL) {
				
				NDAS_ASSERT( FALSE );

				status = Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest( Irp, IO_DISK_INCREMENT );

				result = TRUE;
				leave;
			}

			result = FALSE;
			leave;
		}

#if __NDAS_FS_FLUSH_VOLUME__

		if (DevExt->LfsDeviceExt.FileSystemType == LFS_FILE_SYSTEM_NTFS) {

			switch (irpSp->MajorFunction) {

			case IRP_MJ_CREATE: {

				ULONG	options;
				CHAR	disposition;

				options = irpSp->Parameters.Create.Options & 0xFFFFFF;
				disposition = (CHAR)(((irpSp->Parameters.Create.Options) >> 24) & 0xFF);

				if (options & FILE_DELETE_ON_CLOSE	||
					disposition == FILE_CREATE		||
					disposition == FILE_SUPERSEDE	||
					disposition == FILE_OVERWRITE	||
					disposition == FILE_OVERWRITE_IF) {

					DevExt->LfsDeviceExt.ReceiveWriteCommand = TRUE;
					KeQuerySystemTime( &lfsDeviceExt->CommandReceiveTime );

					SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, 
								   ("lfsDeviceExt->CommandReceiveTime.QuadPart = %I64d\n", DevExt->LfsDeviceExt.CommandReceiveTime.QuadPart) );

			
					break;
				}

				break;
			}

			case IRP_MJ_WRITE:
			case IRP_MJ_SET_INFORMATION:
			case IRP_MJ_SET_EA:
	        case IRP_MJ_SET_VOLUME_INFORMATION:
		    case IRP_MJ_SET_SECURITY:
			case IRP_MJ_SET_QUOTA:

				DevExt->LfsDeviceExt.ReceiveWriteCommand = TRUE;
				KeQuerySystemTime( &lfsDeviceExt->CommandReceiveTime );

				SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, 
							   ("lfsDeviceExt->CommandReceiveTime.QuadPart = %I64d\n", DevExt->LfsDeviceExt.CommandReceiveTime.QuadPart) );

				break;

			case IRP_MJ_FILE_SYSTEM_CONTROL:

				if (irpSp->Parameters.DeviceIoControl.IoControlCode == FSCTL_MOVE_FILE) {

					DevExt->LfsDeviceExt.ReceiveWriteCommand = TRUE;
					KeQuerySystemTime( &lfsDeviceExt->CommandReceiveTime );

					SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, 
								   ("lfsDeviceExt->CommandReceiveTime.QuadPart = %I64d\n", DevExt->LfsDeviceExt.CommandReceiveTime.QuadPart) );
				}

				break;

			default:

				break;
			}
		}

#endif
		if (irpSp->MajorFunction == IRP_MJ_CREATE)
			PrintIrp( LFS_DEBUG_PRIMARY_NOISE, __FUNCTION__, &DevExt->LfsDeviceExt, Irp );

		if (irpSp->MajorFunction == IRP_MJ_PNP) {

			if (irpSp->MinorFunction == IRP_MN_SURPRISE_REMOVAL) {

				if (DevExt->LfsDeviceExt.NetdiskPartition == NULL) {

					ExReleaseFastMutex( &DevExt->LfsDeviceExt.FastMutex );
					fastMutexAcquired = FALSE;
					NDAS_ASSERT( FALSE );
				
				} else {

					PNETDISK_PARTITION	netdiskPartition2;

					netdiskPartition2 = DevExt->LfsDeviceExt.NetdiskPartition;
					DevExt->LfsDeviceExt.NetdiskPartition = NULL;

					SetFlag( DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_SURPRISE_REMOVED );

					ExReleaseFastMutex( &DevExt->LfsDeviceExt.FastMutex );
					fastMutexAcquired = FALSE;

					NetdiskManager_PrimarySessionStopping( GlobalLfs.NetdiskManager,
														   netdiskPartition2,
														   DevExt->LfsDeviceExt.NetdiskEnabledMode	);

					NetdiskManager_PrimarySessionDisconnect( GlobalLfs.NetdiskManager,
														     netdiskPartition2,
														     DevExt->LfsDeviceExt.NetdiskEnabledMode );

					NetdiskManager_SurpriseRemoval( GlobalLfs.NetdiskManager,
													netdiskPartition2,
													DevExt->LfsDeviceExt.NetdiskEnabledMode );
				}

				result = FALSE;
				leave;
			}

			// Need to test much whether this is okay..
	
			if (irpSp->MinorFunction == IRP_MN_CANCEL_REMOVE_DEVICE || irpSp->MinorFunction == IRP_MN_REMOVE_DEVICE) {

				KEVENT				waitEvent;

				if (!FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_QUERY_REMOVE)) {

					NDAS_ASSERT( irpSp->MinorFunction == IRP_MN_CANCEL_REMOVE_DEVICE );

					result = FALSE;
					leave;
				}
				
				ClearFlag( DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_QUERY_REMOVE );

				if (irpSp->MinorFunction == IRP_MN_CANCEL_REMOVE_DEVICE) {

					NDAS_ASSERT( FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING) );
				}

				ExReleaseFastMutex( &DevExt->LfsDeviceExt.FastMutex );
				fastMutexAcquired = FALSE;

				if (irpSp->MinorFunction == IRP_MN_CANCEL_REMOVE_DEVICE) {

					SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO, 
								   ("%s: lfsDeviceExt = %p, IRP_MN_CANCEL_REMOVE_DEVICE Irp->IoStatus.Status = %x\n",
									__FUNCTION__, &DevExt->LfsDeviceExt, Irp->IoStatus.Status) );

					NetdiskManager_CancelRemoveMountVolume( GlobalLfs.NetdiskManager,
														   DevExt->LfsDeviceExt.NetdiskPartition,
														   DevExt->LfsDeviceExt.NetdiskEnabledMode );

					ClearFlag( DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );
					NetdiskManager_PrimarySessionCancelStopping( GlobalLfs.NetdiskManager,
																 DevExt->LfsDeviceExt.NetdiskPartition,
																 DevExt->LfsDeviceExt.NetdiskEnabledMode );

				} else {

					SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO, 
								   ("%s: lfsDeviceExt = %p, IRP_MN_REMOVE_DEVICE Irp->IoStatus.Status = %x\n",
									__FUNCTION__, &DevExt->LfsDeviceExt, Irp->IoStatus.Status) );

					ASSERT( !FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED) );

					NetdiskManager_PrimarySessionDisconnect( GlobalLfs.NetdiskManager,
															 DevExt->LfsDeviceExt.NetdiskPartition,
															 DevExt->LfsDeviceExt.NetdiskEnabledMode );

					ClearFlag( DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );
					LfsDismountVolume( &DevExt->LfsDeviceExt );
				}

				IoCopyCurrentIrpStackLocationToNext( Irp );
				KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

				IoSetCompletionRoutine( Irp,
										LfsGeneralPassThroughCompletion,
										&waitEvent,
										TRUE,
										TRUE,
										TRUE );

				status = IoCallDriver( DevExt->LfsDeviceExt.AttachedToDeviceObject, Irp );

				if (status == STATUS_PENDING) {

					status = KeWaitForSingleObject( &waitEvent, Executive, KernelMode, FALSE, NULL );
					ASSERT( status == STATUS_SUCCESS );
				}

				ASSERT( KeReadStateEvent(&waitEvent) || !NT_SUCCESS(Irp->IoStatus.Status) );
		
				NDAS_ASSERT( NT_SUCCESS(Irp->IoStatus.Status) );

				status = Irp->IoStatus.Status;
				IoCompleteRequest( Irp, IO_NO_INCREMENT );

				result = TRUE;
				leave;
			
			} else if (irpSp->MinorFunction == IRP_MN_QUERY_REMOVE_DEVICE) {

				KEVENT				waitEvent;


				if (DevExt->LfsDeviceExt.AttachedToDeviceObject == NULL) {

					NDAS_ASSERT( FALSE );

					result = FALSE;
					leave;
				}

				SetFlag( DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );

				ExReleaseFastMutex( &DevExt->LfsDeviceExt.FastMutex );
				fastMutexAcquired = FALSE;

				NetdiskManager_PrimarySessionStopping( GlobalLfs.NetdiskManager,
													   DevExt->LfsDeviceExt.NetdiskPartition,
													   DevExt->LfsDeviceExt.NetdiskEnabledMode	);

				IoCopyCurrentIrpStackLocationToNext( Irp );
				KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

				IoSetCompletionRoutine( Irp,
										LfsGeneralPassThroughCompletion,
										&waitEvent,
										TRUE,
										TRUE,
										TRUE );

				status = IoCallDriver( DevExt->LfsDeviceExt.AttachedToDeviceObject, Irp );

				if (status == STATUS_PENDING) {

					status = KeWaitForSingleObject( &waitEvent, Executive, KernelMode, FALSE, NULL );
					ASSERT( status == STATUS_SUCCESS );
				}

				ASSERT( KeReadStateEvent(&waitEvent) || !NT_SUCCESS(Irp->IoStatus.Status) );
	
				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO, 
							   ("%s: lfsDeviceExt = %p, IRP_MN_QUERY_REMOVE_DEVICE Irp->IoStatus.Status = %x\n",
								__FUNCTION__, &DevExt->LfsDeviceExt, Irp->IoStatus.Status) );

				if (NT_SUCCESS(Irp->IoStatus.Status)) {

#if 1
					if (IS_VISTA_OR_LATER() && DevExt->LfsDeviceExt.FileSystemType == LFS_FILE_SYSTEM_NTFS) {

						NetdiskManager_QueryRemoveMountVolume( GlobalLfs.NetdiskManager,
															   DevExt->LfsDeviceExt.NetdiskPartition,
															   DevExt->LfsDeviceExt.NetdiskEnabledMode );

						SetFlag( DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_QUERY_REMOVE );

					} else {
#else
					{
#endif
						NetdiskManager_PrimarySessionDisconnect( GlobalLfs.NetdiskManager,
																 DevExt->LfsDeviceExt.NetdiskPartition,
																 DevExt->LfsDeviceExt.NetdiskEnabledMode );

						ASSERT( !FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED) );

						ClearFlag( DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );
						LfsDismountVolume( &DevExt->LfsDeviceExt );
					}			
				
				} else { 

					NetdiskManager_PrimarySessionCancelStopping( GlobalLfs.NetdiskManager,
																 DevExt->LfsDeviceExt.NetdiskPartition,
																 DevExt->LfsDeviceExt.NetdiskEnabledMode );

					ExAcquireFastMutex( &DevExt->LfsDeviceExt.FastMutex );
					fastMutexAcquired = TRUE;

					ClearFlag( DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );

					ExReleaseFastMutex( &DevExt->LfsDeviceExt.FastMutex );
					fastMutexAcquired = FALSE;
				}

				status = Irp->IoStatus.Status;
				IoCompleteRequest( Irp, IO_NO_INCREMENT );

				result = TRUE;
				leave;
			}

			result = FALSE;
			leave;
		
		} else if (irpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) {

			if (!FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_FILTERING)) {
				
				NDAS_ASSERT( DevExt->LfsDeviceExt.FileSystemType == LFS_FILE_SYSTEM_NDAS_FAT														||
							 DevExt->LfsDeviceExt.FileSystemType == LFS_FILE_SYSTEM_NDAS_ROFS && DevExt->LfsDeviceExt.FilteringMode == LFS_READONLY	||
							 DevExt->LfsDeviceExt.FileSystemType == LFS_FILE_SYSTEM_NDAS_NTFS );

				if (!(irpSp->MinorFunction == IRP_MN_USER_FS_REQUEST || irpSp->MinorFunction == IRP_MN_KERNEL_CALL)) {

					result = FALSE;
					leave;
				}
			}
			
			PrintIrp( LFS_DEBUG_PRIMARY_NOISE, __FUNCTION__, &DevExt->LfsDeviceExt, Irp );

			if (irpSp->MinorFunction == IRP_MN_USER_FS_REQUEST || irpSp->MinorFunction == IRP_MN_KERNEL_CALL) {

				//	Do not allow exclusive access to the volume and dismount volume to protect format
				//	We allow exclusive access if secondaries are connected locally.
				//

				if (irpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_LOCK_VOLUME) {
						
					ExReleaseFastMutex( &DevExt->LfsDeviceExt.FastMutex );
					fastMutexAcquired = FALSE;

					if (NetdiskManager_ThisVolumeHasSecondary(GlobalLfs.NetdiskManager,
															  DevExt->LfsDeviceExt.NetdiskPartition,
															  DevExt->LfsDeviceExt.NetdiskEnabledMode,
															  FALSE) ) {

						SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
									   ("Primary_PassThrough, IRP_MJ_FILE_SYSTEM_CONTROL: Trying to acquire the volume exclusively. \n") );

						ASSERT( fileObject && fileObject->FileName.Length == 0 && fileObject->RelatedFileObject == NULL ); 

						status = Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
						Irp->IoStatus.Information = 0;
						IoCompleteRequest( Irp, IO_DISK_INCREMENT );

						result = TRUE;
						leave;
					}
					
				} else if (irpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_UNLOCK_VOLUME) {

					result = FALSE;
					leave;
	
				} else if (irpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_DISMOUNT_VOLUME) {
						
					KEVENT				waitEvent;

					ExReleaseFastMutex( &DevExt->LfsDeviceExt.FastMutex );
					fastMutexAcquired = FALSE;

					if (NetdiskManager_ThisVolumeHasSecondary( GlobalLfs.NetdiskManager,
															   DevExt->LfsDeviceExt.NetdiskPartition,
															   DevExt->LfsDeviceExt.NetdiskEnabledMode,
															   FALSE) ) {

						ASSERT ( FALSE );

						SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
									   ("Primary_PassThrough, IRP_MJ_FILE_SYSTEM_CONTROL: Trying to acquire the volume exclusively. \n") );

						ASSERT( fileObject && fileObject->FileName.Length == 0 && fileObject->RelatedFileObject == NULL ); 

						status = Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
						Irp->IoStatus.Information = 0;
						IoCompleteRequest( Irp, IO_DISK_INCREMENT );

						result = TRUE;
						leave;
					}
	
					ExAcquireFastMutex( &DevExt->LfsDeviceExt.FastMutex );
					fastMutexAcquired = TRUE;

					SetFlag( DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );

					ExReleaseFastMutex( &DevExt->LfsDeviceExt.FastMutex );
					fastMutexAcquired = FALSE;

					NetdiskManager_PrimarySessionStopping( GlobalLfs.NetdiskManager,
														   DevExt->LfsDeviceExt.NetdiskPartition,
														   DevExt->LfsDeviceExt.NetdiskEnabledMode	);

					IoCopyCurrentIrpStackLocationToNext( Irp );
					KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

					IoSetCompletionRoutine( Irp,
											LfsGeneralPassThroughCompletion,
											&waitEvent,
											TRUE,
											TRUE,
											TRUE );

					status = IoCallDriver( DevExt->LfsDeviceExt.AttachedToDeviceObject, Irp );

					if (status == STATUS_PENDING) {

						status = KeWaitForSingleObject( &waitEvent, Executive, KernelMode, FALSE, NULL );
						ASSERT( status == STATUS_SUCCESS );
					}

					ASSERT( KeReadStateEvent(&waitEvent) || !NT_SUCCESS(Irp->IoStatus.Status) );

					if (NT_SUCCESS(Irp->IoStatus.Status)) {

						NetdiskManager_PrimarySessionDisconnect( GlobalLfs.NetdiskManager,
																 DevExt->LfsDeviceExt.NetdiskPartition,
																 DevExt->LfsDeviceExt.NetdiskEnabledMode );

					} else {
					
						NetdiskManager_PrimarySessionCancelStopping( GlobalLfs.NetdiskManager,
																	 DevExt->LfsDeviceExt.NetdiskPartition,
																	 DevExt->LfsDeviceExt.NetdiskEnabledMode );
					}

					ClearFlag( DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );
	
					if (NT_SUCCESS(Irp->IoStatus.Status)) {

						ASSERT( !FlagOn(DevExt->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED) );
						LfsDismountVolume( &DevExt->LfsDeviceExt );
					}

					status = Irp->IoStatus.Status;
					IoCompleteRequest( Irp, IO_NO_INCREMENT );

					SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, 
								   ("%s: FSCTL_DISMOUNT_VOLUME Irp->IoStatus.Status = %x\n",
									__FUNCTION__, Irp->IoStatus.Status) );

					result = TRUE;
					leave;

				} else if (irpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_SET_ENCRYPTION) {

					//
					//	Do not support encryption.
					//

					SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
								   ("Primary_PassThrough, IRP_MJ_FILE_SYSTEM_CONTROL: Setting encryption denied.\n") );

					status = Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
					Irp->IoStatus.Information = 0;
					IoCompleteRequest( Irp, IO_DISK_INCREMENT );

					result = TRUE;
					leave;
				}
			}
		}

	} finally {

		if (fastMutexAcquired)
			ExReleaseFastMutex( &DevExt->LfsDeviceExt.FastMutex );

		LfsDeviceExt_Dereference( &DevExt->LfsDeviceExt );
		*NtStatus = status;
	}

	return result;
}


NTSTATUS
LfsGeneralPassThroughCompletion (
	IN PDEVICE_OBJECT	DeviceObject,
	IN PIRP				Irp,
	IN PKEVENT			WaitEvent
	)
{
	SPY_LOG_PRINT( LFS_DEBUG_READONLY_NOISE, ("%s: called\n", __FUNCTION__) );

	UNREFERENCED_PARAMETER( DeviceObject );
	UNREFERENCED_PARAMETER( Irp );

	KeSetEvent( WaitEvent, IO_NO_INCREMENT, FALSE );

	return STATUS_MORE_PROCESSING_REQUIRED;
}
