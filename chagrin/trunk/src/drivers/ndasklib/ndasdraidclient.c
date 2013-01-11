#include "ndasscsiproc.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__

#define __MODULE__ "ndasdraidclient"


void
DraidMonitorThreadProc (
	IN	PVOID	Context
	);

void
DraidClientThreadProc (
	IN PLURELATION_NODE		Lurn
	)
{
	NTSTATUS			status;
	ULONG				i;
	KIRQL				oldIrql;
	LARGE_INTEGER		timeOut;
	PRAID_INFO			raidInfo;
	PDRAID_CLIENT_INFO	clientInfo;

	UINT32				childCount;
	UINT32				activeDiskCount;
	UINT32				spareCount;
	UINT32				sectorsPerBit;
	PLIST_ENTRY			listEntry;
	PDRIX_MSG_CONTEXT 	msgLink;	
	PDRAID_CLIENT_LOCK	lock;
	BOOLEAN				nodeChanged;
	LARGE_INTEGER		current_time;
	LARGE_INTEGER		time_diff;
	OBJECT_ATTRIBUTES	objectAttributes;
	BOOLEAN				doMore;

	const UINT32		connectionRetryInterval = 5;
	BOOLEAN				result;
	

	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );	


	DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Starting client\n") );

	ASSERT( LURN_RAID1R == Lurn->LurnType || LURN_RAID4R == Lurn->LurnType );

	raidInfo = Lurn->LurnRAIDInfo;
	clientInfo = ( PDRAID_CLIENT_INFO)raidInfo->pDraidClient;
	
	KeSetEvent( &clientInfo->ClientThreadReadyEvent, IO_DISK_INCREMENT, FALSE );

	sectorsPerBit = raidInfo->SectorsPerBit;

	ASSERT( sectorsPerBit > 0 );

	clientInfo->TotalDiskCount = childCount = Lurn->LurnChildrenCnt;
	clientInfo->ActiveDiskCount = activeDiskCount = Lurn->LurnChildrenCnt - raidInfo->nSpareDisk;
	spareCount = raidInfo->nSpareDisk;

	NDASSCSI_ASSERT( activeDiskCount == raidInfo->nDiskCount );

	clientInfo->IsReadonly = LUR_IS_READONLY( Lurn->Lur );

	RtlCopyMemory( &clientInfo->RaidSetId, &raidInfo->RaidSetId, sizeof(GUID) );
	RtlCopyMemory( &clientInfo->ConfigSetId, &raidInfo->ConfigSetId, sizeof(GUID) );	
		
	// Read RMD and store to client local storage to use later.
		
	status = LurnRMDRead( Lurn, &clientInfo->Rmd, NULL );

	if (!NT_SUCCESS(status)) {

		DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("Client failed to read RMD. Exiting client thread\n") );

		goto terminate;
	}

	// Create client monitor thread after reading RMD
	
	KeInitializeEvent( &clientInfo->MonitorThreadEvent, NotificationEvent, FALSE );

	InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

	status = PsCreateSystemThread( &clientInfo->MonitorThreadHandle,
								   THREAD_ALL_ACCESS,
								   &objectAttributes,
								   NULL,
								   NULL,
								   DraidMonitorThreadProc,
								   Lurn );
	
	if (!NT_SUCCESS(status)) {

		DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("!!! PsCreateSystemThread FAIL\n") );
		goto terminate;
	}

	status = ObReferenceObjectByHandle( clientInfo->MonitorThreadHandle,
										GENERIC_ALL,
										NULL,
										KernelMode,
										&clientInfo->MonitorThreadObject,
										NULL );

	if (!NT_SUCCESS(status)) {

		DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("!!! ObReferenceObjectByHandle FAIL\n") );
		goto terminate;
	}

	doMore = TRUE;

	do {
		
		PKEVENT		events[3];
		LONG		eventCount;
		NTSTATUS	waitStatus;


		if (clientInfo->ClientState == DRAID_CLIENT_STATE_INITIALIZING ||
			clientInfo->ClientState == DRAID_CLIENT_STATE_NO_ARBITER) {
			
			// Try to connect to arbiter
			
			if (LUR_IS_PRIMARY(Lurn->Lur)) {
			
				// If arbiter is not running, run it.

				status = DraidArbiterStart( Lurn );

				if (!NT_SUCCESS(status)) {

					DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("Failed to start arbiter\n"));
					goto terminate;
				}

				for (i=0; i<500; i++) {				
					
					status = DraidRegisterLocalClientToArbiter( Lurn, clientInfo );
					
					if (NT_SUCCESS(status)) {

						break;

					} else if (status == STATUS_RETRY) {

						// Anyway we cannot do anything without arbiter. Try until we can.
						timeOut.QuadPart = - HZ;
						KeDelayExecutionThread( KernelMode, FALSE, &timeOut );	

					} else {

						NDASSCSI_ASSERT( FALSE );
						DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("Arbiter terminated. Terminate local client too.\n"));
						goto terminate;
					}
				}

				if (i == 500) {
				
					NDASSCSI_ASSERT( FALSE );
					DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("Failed to register client to arbiter\n"));

					goto terminate;
				}

				doMore = TRUE;

			} else if (LUR_IS_SECONDARY(Lurn->Lur)) {

				KeQueryTickCount( &current_time );
				time_diff.QuadPart = (current_time.QuadPart - clientInfo->LastTriedArbiterTime.QuadPart) * KeQueryTimeIncrement();

				if (time_diff.QuadPart >= HZ * connectionRetryInterval) {

					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Calling DraidClientEstablishArbiter in secondary mode\n") );
					
					status = LurnRMDRead( clientInfo->Lurn, &clientInfo->Rmd, NULL );

					if (!NT_SUCCESS(status)) {

						DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("Client failed to read RMD. Exiting client thread\n") );
						
						goto terminate;
					}

					status = DraidClientEstablishArbiter( clientInfo );	
					KeQueryTickCount( &clientInfo->LastTriedArbiterTime );

				} else {
				
					// Don't try again too frequently.
					status = STATUS_UNSUCCESSFUL;
				}				

				if (NT_SUCCESS(status)) {

					// Status has changed continue.
					doMore = TRUE;

				} else {

					// secondary to primary transition does not occur if no disk operation is requested and 
					// therefore no primary situation can continue for a long time.
					// So we don't terminate even if we cannot connect to primary for a long time.
					
					// Allow some Ccb operations(except writing) even when arbiter is not available.
					// Assume FS knows what it is doing.(FS direcly read only when it is safe to read regardless of primary).
					
					DraidClientRefreshRaidStatusWithoutArbiter( clientInfo );
					ACQUIRE_SPIN_LOCK( &clientInfo->SpinLock, &oldIrql );
					
					clientInfo->ClientState = DRAID_CLIENT_STATE_NO_ARBITER;
					clientInfo->InTransition = FALSE;
					
					RELEASE_SPIN_LOCK( &clientInfo->SpinLock, oldIrql );	
					
					if (!IsListEmpty(&clientInfo->CcbQueue)) {

						DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Handle some CCBs in NO ARBITER mode\n") );
						
						status = DraidClientHandleCcb(clientInfo);
						
						if (!NT_SUCCESS(status)) {	
							
							goto terminate;
						}
					}

					DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, 
								("Failed to establish connection to arbiter. Enter no arbiter mode. Waiting %d seconds\n", 
								 connectionRetryInterval) );
					
					// Delay for a while or wait event
					timeOut.QuadPart = - HZ * connectionRetryInterval;
					
					status = KeWaitForSingleObject( &clientInfo->ClientThreadEvent, Executive, KernelMode,	FALSE, &timeOut );
					KeClearEvent( &clientInfo->ClientThreadEvent );
					doMore = TRUE;
				}

			} else if (clientInfo->IsReadonly) {
			
				if (clientInfo->ClientState == DRAID_CLIENT_STATE_INITIALIZING) {

					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Calling DraidClientEstablishArbiter in read-only mode\n") );
					status = LurnRMDRead( clientInfo->Lurn, &clientInfo->Rmd, NULL );

					if (!NT_SUCCESS(status)) {

						DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("Client failed to read RMD. Exiting client thread\n"));
						goto terminate;
					}

					status = DraidClientEstablishArbiter( clientInfo );
					
					if (!NT_SUCCESS(status)) {

						DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Arbiter is not available right now. Enter read-only RAID mode\n") );

						KeQueryTickCount( &clientInfo->LastTriedArbiterTime );
						clientInfo->LastTriedArbiterUsn = clientInfo->Rmd.uiUSN;
						
						DraidClientRefreshRaidStatusWithoutArbiter( clientInfo );
						
						ACQUIRE_SPIN_LOCK( &clientInfo->SpinLock, &oldIrql );
						clientInfo->ClientState = DRAID_CLIENT_STATE_NO_ARBITER;
						clientInfo->InTransition = FALSE;
						RELEASE_SPIN_LOCK( &clientInfo->SpinLock, oldIrql );	
					}

					KeQueryTickCount( &clientInfo->LastTriedArbiterTime );
				
				} else {

					// Already working.
				}
			
			} else {
			
				NDASSCSI_ASSERT( FALSE ); // should not happen.

				goto terminate;
			}		
		}

		// Check arbiter available now.
		// To do: Notify read-only host when arbiter is available. 
		
		if (clientInfo->IsReadonly && clientInfo->ClientState == DRAID_CLIENT_STATE_NO_ARBITER) {
		
			KeQueryTickCount( &current_time );

			time_diff.QuadPart = (current_time.QuadPart - clientInfo->LastTriedArbiterTime.QuadPart) * KeQueryTimeIncrement();
			
			// Check RMD is updated at every 30 seconds
			
			if (time_diff.QuadPart >= HZ * 30) {
			
				status = LurnRMDRead( clientInfo->Lurn, &clientInfo->Rmd, NULL );

				if (NT_SUCCESS(status)) {

					if (clientInfo->Rmd.uiUSN > clientInfo->LastTriedArbiterUsn) {
				
						DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("RMD updated. Trying to connect to arbiter\n"));
						
						status = DraidClientEstablishArbiter( clientInfo );	
						
						if (NT_SUCCESS(status)) {
						
							DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Connected to arbiter in read-only mode\n"));	

						} else {

							DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Failed to connect to arbiter in read-only mode\n"));
						}
					}

					clientInfo->LastTriedArbiterUsn = clientInfo->Rmd.uiUSN;

				} else {

					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Failed to read RMD\n"));
				}

				KeQueryTickCount( &clientInfo->LastTriedArbiterTime );
			}
		}

		if (DRAID_CLIENT_STATE_ARBITER_CONNECTED == clientInfo->ClientState) {

			if (clientInfo->HasLocalArbiter) {

				KeClearEvent( &clientInfo->RequestReplyChannel.Event );

				// Handle any pending reply from local arbiter

				while (listEntry = ExInterlockedRemoveHeadList( &clientInfo->RequestReplyChannel.Queue,
					 										    &clientInfo->RequestReplyChannel.Lock)) {

					msgLink = CONTAINING_RECORD( listEntry,
												 DRIX_MSG_CONTEXT,
												 Link );

					status = DraidClientHandleReplyForRequest( clientInfo, msgLink->Message );
					
					if (!NT_SUCCESS(status)) {
						
						DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("Failed to handle repy\n"));	
						doMore = TRUE;
					}

					ExFreePoolWithTag( msgLink->Message, DRAID_CLIENT_REQUEST_REPLY_POOL_TAG );	
					ExFreePoolWithTag( msgLink, DRAID_MSG_LINK_POOL_TAG );	
				}
			
			} else {
				
				status = DraidClientRecvHeaderWithoutWait( clientInfo, &clientInfo->RequestConnection );
							
				if (status == STATUS_SUCCESS) {
			
					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Received reply to request.(%x)\n", status));
					
					status = DraidClientHandleReplyForRequest(clientInfo, (PDRIX_HEADER)clientInfo->RequestConnection.ReceiveBuf);
					
					if (NT_SUCCESS(status)) {
						
						// Wait for next message if pending request exists

						if (InterlockedCompareExchange(&clientInfo->RequestConnection.RxPendingCount, 0, 0)) {

							InterlockedDecrement( &clientInfo->RequestConnection.RxPendingCount );
							
							DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Start to wait for reply for pending request\n"));
							
							status = DraidClientRecvHeaderAsync(clientInfo, &clientInfo->RequestConnection);
						}
					}

				} else if (status == STATUS_PENDING) {
					
					// Packet is not arrived yet.
				} 

				if (!NT_SUCCESS(status)) {
				
					DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, 
								("Failed to handle reply. Reset current context and enter initialization step.\n") );
					
					DraidClientResetRemoteArbiterContext( clientInfo );
				}
			}
		}

		
		// Handle if there is any timed out requests.
		
		KeQueryTickCount( &current_time );

		ACQUIRE_SPIN_LOCK( &clientInfo->SpinLock, &oldIrql );
		
		for (listEntry = clientInfo->PendingRequestList.Flink;
			 listEntry != &clientInfo->PendingRequestList;
			 listEntry = listEntry->Flink) {

			msgLink = CONTAINING_RECORD (listEntry, DRIX_MSG_CONTEXT, Link);
			
			if (msgLink->HaveTimeout &&
				(current_time.QuadPart - msgLink->ReqTime.QuadPart > msgLink->Timeout.QuadPart)) {
				listEntry = listEntry->Blink;
				
				// Timeout. Reply timeout does not mean we don't wait anymore. 
				// If reply is arrived later, it will be handle anyway, but we will call callback handler.
				
				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Reply timed out\n") );
				
				if (msgLink->CallbackFunc) {
			
					RELEASE_SPIN_LOCK( &clientInfo->SpinLock, oldIrql );					
					msgLink->CallbackFunc( clientInfo, NULL, msgLink->CallbackParam1 );
					ACQUIRE_SPIN_LOCK( &clientInfo->SpinLock, &oldIrql );
				}
				
				RemoveEntryList(&msgLink->Link);

				if (msgLink->Message) {

					ExFreePoolWithTag( msgLink->Message, DRAID_CLIENT_REQUEST_MSG_POOL_TAG );	
				}
				
				ExFreePoolWithTag( msgLink, DRAID_MSG_LINK_POOL_TAG );
			} 
		}

		RELEASE_SPIN_LOCK( &clientInfo->SpinLock, oldIrql );
		
		// Handle any pending notifications
		
		if (DRAID_CLIENT_STATE_ARBITER_CONNECTED == clientInfo->ClientState) {

			if (clientInfo->HasLocalArbiter) {
				
				KeClearEvent( &clientInfo->NotificationChannel.Event );
				
				while (listEntry = ExInterlockedRemoveHeadList(&clientInfo->NotificationChannel.Queue,
															   &clientInfo->NotificationChannel.Lock)) {

					msgLink = CONTAINING_RECORD(listEntry,	DRIX_MSG_CONTEXT,Link);
					
					status = DraidClientHandleNotificationMsg( clientInfo, msgLink->Message );

					if (!NT_SUCCESS(status)) {
					
						DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("Failed to handle notification message\n") );

						ASSERT( FALSE );
						// This should not happen for local arbiter
					}
					
					doMore = TRUE;
					
					ExFreePoolWithTag( msgLink->Message, DRAID_ARBITER_NOTIFY_MSG_POOL_TAG );						
					ExFreePoolWithTag( msgLink, DRAID_MSG_LINK_POOL_TAG );	
				}

			} else {
				
				status = DraidClientRecvHeaderWithoutWait(clientInfo, &clientInfo->NotificationConnection);
				
				if (status == STATUS_SUCCESS) {
				
					DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("Received through notification connection.\n") );

					status = DraidClientHandleNotificationMsg( clientInfo, (PDRIX_HEADER)clientInfo->NotificationConnection.ReceiveBuf );
				
					if (status == STATUS_SUCCESS) {
						
						// Wait for more notification message
						status = DraidClientRecvHeaderAsync(clientInfo, &clientInfo->NotificationConnection);
					}
				
				} else if (status == STATUS_PENDING) {

					// Packet is not arrived yet.
				}

				if (!NT_SUCCESS(status)) {

					DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, 
								("Receive through notification connection failed. Reset current context and enter initialization step.\n"));
					
					DraidClientResetRemoteArbiterContext( clientInfo );
				}
			} 
		}
		
		nodeChanged = FALSE;
		
		// Check local node changes and send state update request.
		
		for(i=0; i<childCount; i++) {
		
			ACQUIRE_SPIN_LOCK( &clientInfo->SpinLock, &oldIrql );
			
			if (!(clientInfo->NodeChanged[i] & DRAID_NODE_CHANGE_FLAG_UPDATING) &&
				  (clientInfo->NodeChanged[i] & DRAID_NODE_CHANGE_FLAG_CHANGED)) {
				
				nodeChanged = TRUE;
				clientInfo->NodeChanged[i] &= ~DRAID_NODE_CHANGE_FLAG_CHANGED;
				clientInfo->NodeChanged[i] |= DRAID_NODE_CHANGE_FLAG_UPDATING;
				
				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Node %d has changed\n", i) );
			}
			
			RELEASE_SPIN_LOCK( &clientInfo->SpinLock, oldIrql );
		}

		if (nodeChanged) {

			if (DRAID_CLIENT_STATE_ARBITER_CONNECTED == clientInfo->ClientState) {
			
				status = DraidClientSendRequest( clientInfo, DRIX_CMD_NODE_CHANGE, 0, 0,0, NULL, NULL, NULL );

				if (!NT_SUCCESS(status)) {

					DraidClientResetRemoteArbiterContext( clientInfo );
				}

				doMore = TRUE;

			} else if (DRAID_CLIENT_STATE_NO_ARBITER == clientInfo->ClientState) {

				ACQUIRE_SPIN_LOCK( &clientInfo->SpinLock, &oldIrql );

				for (i=0; i<clientInfo->Lurn->LurnChildrenCnt; i++) {

					clientInfo->NodeChanged[i] &= ~DRAID_NODE_CHANGE_FLAG_UPDATING;
				}

				RELEASE_SPIN_LOCK( &clientInfo->SpinLock, oldIrql );
				DraidClientRefreshRaidStatusWithoutArbiter( clientInfo );

				doMore = TRUE;
			}
		}

		// Handle Ccb

		if (!IsListEmpty(&clientInfo->CcbQueue)) {
			
			status = DraidClientHandleCcb( clientInfo );
			
			if (!NT_SUCCESS(status)) {	
				
				goto terminate;
			}
			
			doMore = TRUE;
		}
	
		// Handle flush request

		ACQUIRE_SPIN_LOCK( &clientInfo->SpinLock, &oldIrql );

		if (clientInfo->RequestForFlush) {

			BOOLEAN lockInUse;
			
			// Check any lock is in-use.
			
			lockInUse = FALSE;

			for (listEntry = clientInfo->LockList.Flink;
				listEntry != &clientInfo->LockList;
				listEntry = listEntry->Flink) {

				lock = CONTAINING_RECORD( listEntry, DRAID_CLIENT_LOCK, Link );
				
				if (InterlockedCompareExchange(&lock->InUseCount, 0, 0)) {
				
					lockInUse = TRUE;
					break;
				}
			}
			
			if (lockInUse) {

				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Lock is in use. Try to flush later\n") );
				
				RELEASE_SPIN_LOCK( &clientInfo->SpinLock, oldIrql );
			
			} else {
			
				BOOLEAN flushed = TRUE;

				// Release all locks including GRANTED and PENDING

				while (!IsListEmpty(&clientInfo->LockList)) {

					listEntry = RemoveHeadList(&clientInfo->LockList);

					lock = CONTAINING_RECORD(listEntry, DRAID_CLIENT_LOCK, Link);
					
					DraidClientFreeLock( lock );
					
					flushed = FALSE;
				}

				if (flushed) {
					
					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Flushing completed\n") );

					RELEASE_SPIN_LOCK(&clientInfo->SpinLock, oldIrql);
					
					// Flushing completed
					
					if (clientInfo->FlushCompletionRoutine) {
						
						clientInfo->FlushCompletionRoutine( clientInfo->FlushCcb, NULL );
					}

					ACQUIRE_SPIN_LOCK(&clientInfo->SpinLock, &oldIrql );

					clientInfo->FlushCompletionRoutine = NULL;
					clientInfo->FlushCcb = NULL;
					clientInfo->RequestForFlush = FALSE;					
					
					RELEASE_SPIN_LOCK( &clientInfo->SpinLock, oldIrql );

				} else {
					
					LARGE_INTEGER timeout;
					
					timeout.QuadPart = HZ * 5; 

					RELEASE_SPIN_LOCK( &clientInfo->SpinLock, oldIrql );
					
					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Sending release all lock message to flush\n") );
					
					status = DraidClientSendRequest( clientInfo, DRIX_CMD_RELEASE_LOCK, 0, DRIX_LOCK_ID_ALL, 0, &timeout, NULL, NULL );
				} 
			}

		} else {
		
			RELEASE_SPIN_LOCK( &clientInfo->SpinLock, oldIrql );
		}
		
		// Check any lock is not used for a while or has been requested to yield.
		
		if (DRAID_CLIENT_STATE_ARBITER_CONNECTED == clientInfo->ClientState) {		
			
			ACQUIRE_SPIN_LOCK( &clientInfo->SpinLock, &oldIrql );
			
			KeQueryTickCount( &current_time );
			
			for (listEntry = clientInfo->LockList.Flink;
				 listEntry != &clientInfo->LockList;
				 listEntry = listEntry->Flink) {

				lock = CONTAINING_RECORD( listEntry, DRAID_CLIENT_LOCK, Link );
				
				if (InterlockedCompareExchange(&lock->InUseCount, 0, 0)) {
				
					continue;
				}

				if (lock->LockStatus != LOCK_STATUS_GRANTED) {

					continue;
				}

				time_diff.QuadPart = (current_time.QuadPart - lock->LastAccessedTime.QuadPart) * KeQueryTimeIncrement();

				if (lock->YieldRequested ||
					time_diff.QuadPart >= HZ * DRIX_IO_LOCK_CLEANUP_TIMEOUT ||
					((clientInfo->DRaidStatus == DRIX_RAID_STATUS_REBUILDING || lock->Contented) &&
					 time_diff.QuadPart >= HZ * DRIX_IO_LOCK_CLEANUP_TIMEOUT/10)) { // Release lock faster if in rebuilding mode or lock is contented
			
					LARGE_INTEGER timeout;

					timeout.QuadPart = HZ * 5; 
				
					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
								("Releasing locked region(Id=%I64x, Range=%I64x:%x)\n",
								 lock->LockId, lock->LockAddress, lock->LockLength) );
					
					// Remove from acquired lock list and send message
					
					listEntry = listEntry->Blink;
					
					RemoveEntryList( &lock->Link );
					
					RELEASE_SPIN_LOCK( &clientInfo->SpinLock, oldIrql );

					status = DraidClientSendRequest( clientInfo, DRIX_CMD_RELEASE_LOCK, 0, lock->LockId, 0, &timeout, NULL, NULL );
					
					DraidClientFreeLock( lock );
					
					if (!NT_SUCCESS(status) && !clientInfo->HasLocalArbiter) {
						
						DraidClientResetRemoteArbiterContext( clientInfo );
					} 

					ACQUIRE_SPIN_LOCK( &clientInfo->SpinLock, &oldIrql );

					doMore = TRUE;

					break; // Lock is removed and need to reiterate lock list
				}
			}

			RELEASE_SPIN_LOCK( &clientInfo->SpinLock, oldIrql );
		}		
		
		// Check termination request.
		
		result = DraidClientProcessTerminateRequest( clientInfo );
		
		if (result) {

			break;
		}
	
		if (doMore) {

			doMore = FALSE;
			continue;
		
		} 
		
		eventCount = 0;
		events[eventCount] = &clientInfo->ClientThreadEvent;
		eventCount++;
			
		// Wait for event 

		if (clientInfo->HasLocalArbiter) {
				
			// Wait for ClientThreadEvent(terminate request) and Notification message
				
			events[eventCount] = &clientInfo->NotificationChannel.Event;
			eventCount++;
			events[eventCount] = &clientInfo->RequestReplyChannel.Event;
			eventCount++;
			
		} else if (clientInfo->ClientState == DRAID_CLIENT_STATE_ARBITER_CONNECTED) {
				
			// Remote arbiter
				
			if (clientInfo->NotificationConnection.Status == DRAID_CLIENT_CON_REGISTERED) {
			
				if (clientInfo->NotificationConnection.TdiReceiveContext.Result < 0) {
					
					DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, 
								("Previous connection result is %x. Reset current context and enter initialization step.\n", 
								  clientInfo->NotificationConnection.TdiReceiveContext.Result) );

					NDASSCSI_ASSERT( FALSE );	// This case should not happen. If this happens, it mean lpx completion event is lost.
						
					DraidClientResetRemoteArbiterContext( clientInfo );
					continue;

				} else {

					events[eventCount] = &clientInfo->NotificationConnection.TdiReceiveContext.CompletionEvent;
					eventCount++;
				}
			} 

			if (clientInfo->RequestConnection.Status == DRAID_CLIENT_CON_REGISTERED) {
				
				if (clientInfo->RequestConnection.TdiReceiveContext.Result < 0) {

					DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, 
								("Previous connection result is %x. Reset current context and enter initialization step.\n", 
								  clientInfo->RequestConnection.TdiReceiveContext.Result) );
						
					NDASSCSI_ASSERT( FALSE );	// This case should not happen. If this happens, it mean lpx completion event is lost.
						
					DraidClientResetRemoteArbiterContext(clientInfo);
					continue;

				} else {
						
					events[eventCount] = &clientInfo->RequestConnection.TdiReceiveContext.CompletionEvent;
					eventCount++;
				}
			} 
		}

		if (clientInfo->DRaidStatus == DRAID_CLIENT_STATE_INITIALIZING	|| 
			clientInfo->DRaidStatus == DRIX_RAID_STATUS_REBUILDING		|| 
			clientInfo->RequestToTerminate								|| 
			clientInfo->RequestForFlush) {
				
			// Set shorter timeout in initialization or rebuilding status to wake-up write-lock clean-up more frequently
			// also when termination or flush is requested
				
			timeOut.QuadPart = - NANO100_PER_SEC * 2;
			
		} else {
			
			timeOut.QuadPart = - NANO100_PER_SEC * 20; // need to wake-up with timeout to free unused write-locks.
		}

		// To do: Iterate PendingRequestList to find out neareast timeout time
			
		waitStatus = KeWaitForMultipleObjects( eventCount, 
											   events,
											   WaitAny, 
											   Executive,
											   KernelMode,
											   TRUE,
											   &timeOut,
											   NULL );
			
		if (waitStatus == STATUS_WAIT_0) {
			
			// Got thread event. 
			DebugTrace( DBG_LURN_NOISE, ("ClientThreadEvent signaled\n") );
			KeClearEvent(events[0]);
			
		} else if (waitStatus == STATUS_WAIT_1) {
			
			// Get notification
			// DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("NotificationChannel signaled\n"));

		} else if (waitStatus == STATUS_WAIT_2) {

			//	DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("RequestReplyChannel signaled\n"));
		}
	
	} while (1);

terminate:

	ACQUIRE_SPIN_LOCK( &clientInfo->SpinLock, &oldIrql );

	clientInfo->ClientState = DRAID_CLIENT_STATE_TERMINATING;
	clientInfo->DRaidStatus = DRIX_RAID_STATUS_TERMINATED;
	
	RELEASE_SPIN_LOCK( &clientInfo->SpinLock, oldIrql );	
	
	if (clientInfo->HasLocalArbiter) {

		NDASSCSI_ASSERT( IsListEmpty(&clientInfo->LockList) );
		NDASSCSI_ASSERT( IsListEmpty(&clientInfo->PendingRequestList) );

		DraidUnregisterLocalClient( Lurn );
	}

	DraidClientDisconnectFromArbiter( clientInfo, &clientInfo->NotificationConnection );
	DraidClientDisconnectFromArbiter( clientInfo, &clientInfo->RequestConnection );
	
	// Complete pending 
	
	{
		PLIST_ENTRY ccbListEntry;
		PCCB		Ccb;
		
		while (ccbListEntry = ExInterlockedRemoveHeadList(&clientInfo->CcbQueue,
														  &clientInfo->SpinLock)) {
			
			Ccb = CONTAINING_RECORD(ccbListEntry, CCB, ListEntry);
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Client thread is terminating. Returning CCB with STOP status.\n") );
	
			LSCcbSetStatus( Ccb, CCB_STATUS_STOP );
			LSCcbCompleteCcb( Ccb );
		}
	}
	
	if (clientInfo->RequestToTerminate == FALSE) {

		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Client thread terminated by itself.\n") );
	}

	DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Exiting\n") );
	
	PsTerminateSystemThread( STATUS_SUCCESS );
	return;
}

//
// Initialize local RAID information
//

NTSTATUS 
DraidClientStart (
	PLURELATION_NODE	Lurn
	)
{
	NTSTATUS			status;
	PRAID_INFO			raidInfo = Lurn->LurnRAIDInfo;
	PDRAID_CLIENT_INFO	clientInfo;
	
	UINT32				i;
	OBJECT_ATTRIBUTES	objectAttributes;

	NDASSCSI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	NDASSCSI_ASSERT( raidInfo->pDraidClient == NULL ); // Multiple arbiter is not possible.
	 
	// Allocate client and set initial value.
	
	raidInfo->pDraidClient = ExAllocatePoolWithTag( NonPagedPool, sizeof(DRAID_CLIENT_INFO), DRAID_CLIENT_INFO_POOL_TAG );
	
	if (raidInfo->pDraidClient == NULL) {
	
		return STATUS_INSUFFICIENT_RESOURCES;
	}
		
	clientInfo = raidInfo->pDraidClient;

	RtlZeroMemory( clientInfo, sizeof(DRAID_CLIENT_INFO) );

	KeInitializeSpinLock( &clientInfo->SpinLock );

	clientInfo->ClientState = DRAID_CLIENT_STATE_INITIALIZING;
	clientInfo->InTransition = TRUE; // Hold IO until we get information from arbiter
	clientInfo->DRaidStatus = DRIX_RAID_STATUS_INITIALIZING;
	
	for (i=0;i<Lurn->LurnChildrenCnt;i++) {

		clientInfo->NodeFlagsLocal[i]	= DraidClientLurnStatusToNodeFlag(Lurn->LurnChildren[i]->LurnStatus);
		clientInfo->NodeFlagsRemote[i]	= DRIX_NODE_FLAG_UNKNOWN;
		clientInfo->RoleToNodeMap[i]	= (UCHAR) -1;	// not valid until LurnFlag is not unknown.
	}

	clientInfo->Lurn = Lurn;

	InitializeListHead( &clientInfo->LockList );
	InitializeListHead( &clientInfo->CcbQueue );
	InitializeListHead( &clientInfo->PendingRequestList );

	KeInitializeSpinLock( &clientInfo->RequestConnection.SpinLock );
	
	clientInfo->RequestConnection.TdiReceiveContext.Irp = 0;
	clientInfo->RequestConnection.TdiReceiveContext.Result = 0;
	
	KeInitializeEvent( &clientInfo->RequestConnection.TdiReceiveContext.CompletionEvent, NotificationEvent, FALSE );
	clientInfo->RequestConnection.Timeout.QuadPart = 5*HZ;

	KeInitializeSpinLock( &clientInfo->NotificationConnection.SpinLock );

	clientInfo->NotificationConnection.TdiReceiveContext.Irp = 0;
	clientInfo->NotificationConnection.TdiReceiveContext.Result= 0;
	
	KeInitializeEvent( &clientInfo->NotificationConnection.TdiReceiveContext.CompletionEvent, NotificationEvent, FALSE );
	
	clientInfo->NotificationConnection.Timeout.QuadPart = 0;

	DRIX_INITIALIZE_LOCAL_CHANNEL( &clientInfo->NotificationChannel );
	DRIX_INITIALIZE_LOCAL_CHANNEL( &clientInfo->RequestReplyChannel );

	// Create client thread
	
	KeInitializeEvent( &clientInfo->ClientThreadEvent, NotificationEvent, FALSE );
	KeInitializeEvent( &clientInfo->ClientThreadReadyEvent, NotificationEvent, FALSE );

	InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );

	status = PsCreateSystemThread( &clientInfo->ClientThreadHandle,
								   THREAD_ALL_ACCESS,
								   &objectAttributes,
								   NULL,
								   NULL,
								   DraidClientThreadProc,
								   Lurn );
	
	if (!NT_SUCCESS(status)) {

		DebugTrace(  NDASSCSI_DEBUG_LURN_ERROR, ("!!! PsCreateSystemThread FAIL\n") );
		
		return STATUS_THREAD_NOT_IN_PROCESS;
	}

	status = ObReferenceObjectByHandle( clientInfo->ClientThreadHandle,
										GENERIC_ALL,
										NULL,
										KernelMode,
										&clientInfo->ClientThreadObject,
										NULL );

	if (!NT_SUCCESS(status)) {

		DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("!!! ObReferenceObjectByHandle FAIL\n") );
		return STATUS_THREAD_NOT_IN_PROCESS;
	}

	status = KeWaitForSingleObject( &clientInfo->ClientThreadReadyEvent,
									Executive,
									KernelMode,
									FALSE,
									NULL );

	if (status != STATUS_SUCCESS) {
	
		NDASSCSI_ASSERT( FALSE );
		return STATUS_THREAD_NOT_IN_PROCESS;
	}

	DraidRegisterClient( clientInfo );

	return STATUS_SUCCESS;
}

NTSTATUS
DraidClientStop (
	PLURELATION_NODE	Lurn
	)
{
	NTSTATUS			status = STATUS_SUCCESS;
	KIRQL				oldIrql;
	
	PRAID_INFO			raidInfo = Lurn->LurnRAIDInfo;
	PLIST_ENTRY			listEntry;
	PDRAID_CLIENT_LOCK	Lock;
	PDRIX_MSG_CONTEXT	msgEntry;
	PDRAID_CLIENT_INFO	clientInfo = (PDRAID_CLIENT_INFO)raidInfo->pDraidClient;
	PDRIX_MSG_CONTEXT	msgContext;

	NDASSCSI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	
	ACQUIRE_SPIN_LOCK( &raidInfo->SpinLock, &oldIrql );

	if (!raidInfo->pDraidClient) {
		
		// Already stopped.
		RELEASE_SPIN_LOCK( &raidInfo->SpinLock, oldIrql );	
		return STATUS_SUCCESS;
	}

	raidInfo->pDraidClient  = NULL;
	RELEASE_SPIN_LOCK( &raidInfo->SpinLock, oldIrql );	

	DraidUnregisterClient( clientInfo );

	ACQUIRE_SPIN_LOCK( &clientInfo->SpinLock, &oldIrql );

	clientInfo->RequestToTerminate = TRUE;
	
	if (clientInfo->ClientThreadHandle) {

		DebugTrace(  NDASSCSI_DEBUG_LURN_INFO, ("KeSetEvent\n") );
		KeSetEvent( &clientInfo->ClientThreadEvent,IO_NO_INCREMENT, FALSE );
	}

	if (clientInfo->MonitorThreadHandle) {
	
		KeSetEvent( &clientInfo->MonitorThreadEvent,IO_NO_INCREMENT, FALSE );
	}
	
	RELEASE_SPIN_LOCK( &clientInfo->SpinLock, oldIrql );

	if (clientInfo->MonitorThreadHandle) {

		status = KeWaitForSingleObject( clientInfo->MonitorThreadObject,
										Executive,
										KernelMode,
										FALSE,
										NULL );

		ASSERT( status == STATUS_SUCCESS );

		ObDereferenceObject( clientInfo->MonitorThreadObject );
		ZwClose( clientInfo->MonitorThreadHandle );		
	}

	if (clientInfo->ClientThreadHandle) {

		status = KeWaitForSingleObject( clientInfo->ClientThreadObject,
										Executive,
										KernelMode,
										FALSE,
										NULL );

		ASSERT( status == STATUS_SUCCESS );

		ObDereferenceObject( clientInfo->ClientThreadObject );
		ZwClose( clientInfo->ClientThreadHandle );
	}

	ACQUIRE_SPIN_LOCK( &clientInfo->SpinLock, &oldIrql );

	clientInfo->ClientThreadObject = NULL;
	clientInfo->ClientThreadHandle = NULL;
	clientInfo->MonitorThreadObject = NULL;
	clientInfo->MonitorThreadHandle = NULL;

	RELEASE_SPIN_LOCK( &clientInfo->SpinLock, oldIrql );

	ACQUIRE_SPIN_LOCK( &clientInfo->SpinLock, &oldIrql );
	
	while (!IsListEmpty(&clientInfo->LockList)) {

		listEntry = RemoveHeadList( &clientInfo->LockList );
		
		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Freeing lock\n") );

		Lock = CONTAINING_RECORD( listEntry, DRAID_CLIENT_LOCK, Link );

		NDASSCSI_ASSERT( InterlockedCompareExchange(&Lock->InUseCount, 0, 0) == 0 );

		ExFreePoolWithTag( Lock, DRAID_CLIENT_LOCK_POOL_TAG );
	}

	while (!IsListEmpty(&clientInfo->PendingRequestList)) {

		listEntry = RemoveHeadList(&clientInfo->PendingRequestList);
		
		msgContext = CONTAINING_RECORD( listEntry, DRIX_MSG_CONTEXT, Link );

		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Freeing pending request messages\n") );

		ExFreePoolWithTag( msgContext->Message, DRAID_CLIENT_REQUEST_MSG_POOL_TAG );
		ExFreePoolWithTag( msgContext, DRAID_MSG_LINK_POOL_TAG );
	}

	while (!IsListEmpty(&clientInfo->NotificationChannel.Queue)) {

		listEntry = RemoveHeadList( &clientInfo->NotificationChannel.Queue );

		msgEntry = CONTAINING_RECORD( listEntry, DRIX_MSG_CONTEXT, Link );
		
		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Freeing unhandled notification message\n"));

		ExFreePoolWithTag( msgEntry->Message, DRAID_ARBITER_NOTIFY_MSG_POOL_TAG );
		ExFreePoolWithTag( msgEntry, DRAID_MSG_LINK_POOL_TAG );
	}

	while (!IsListEmpty(&clientInfo->RequestReplyChannel.Queue)) {
	
		listEntry = RemoveHeadList( &clientInfo->RequestReplyChannel.Queue );

		msgEntry = CONTAINING_RECORD( listEntry, DRIX_MSG_CONTEXT, Link );
		
		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Freeing unhandled request-reply message\n") );
		
		ExFreePoolWithTag( msgEntry->Message, DRAID_CLIENT_REQUEST_REPLY_POOL_TAG );
		ExFreePoolWithTag( msgEntry, DRAID_MSG_LINK_POOL_TAG );
	}
	
	RELEASE_SPIN_LOCK( &clientInfo->SpinLock, oldIrql );
	
	ExFreePoolWithTag( clientInfo, DRAID_CLIENT_INFO_POOL_TAG );
	DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Client stopped completely\n") );

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
DraidMonitorThreadProc (
	IN PLURELATION_NODE	Lurn
	)
{
	NTSTATUS				status;
	ULONG					i;
	PLURELATION_NODE		childLurn;
	KIRQL					oldIrql;
	LARGE_INTEGER			timeOut;
	PRAID_INFO				raidInfo;
	PDRAID_CLIENT_INFO		clientInfo;

	UINT32					childCount;
	NDAS_RAID_META_DATA		rmd;


	DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Starting client monitor\n") );

	ASSERT( LURN_RAID1R == Lurn->LurnType || LURN_RAID4R == Lurn->LurnType );

	raidInfo = Lurn->LurnRAIDInfo;
	
	clientInfo = ( PDRAID_CLIENT_INFO)raidInfo->pDraidClient;
	
	childCount = Lurn->LurnChildrenCnt;


	do {
		
		PKEVENT		events[1];
		LONG		eventCount = 1;
		NTSTATUS	waitStatus;


		events[0] = &clientInfo->MonitorThreadEvent; // wait for quit message
		
		timeOut.QuadPart = -NANO100_PER_SEC * 30;
			
		waitStatus = KeWaitForMultipleObjects( eventCount, events, WaitAny, Executive, KernelMode, TRUE, &timeOut, NULL );
		
		// Check termination request.
		
		ACQUIRE_SPIN_LOCK( &clientInfo->SpinLock, &oldIrql );
		
		if (clientInfo->RequestToTerminate) {

			RELEASE_SPIN_LOCK( &clientInfo->SpinLock, oldIrql );
			
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Client termination requested\n") );	
			break;
		}

		if (!(clientInfo->ClientState == DRAID_CLIENT_STATE_NO_ARBITER || 
			  clientInfo->ClientState == DRAID_CLIENT_STATE_ARBITER_CONNECTED)) {
			
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Client is not initialized or terminating\n") );	
			
			RELEASE_SPIN_LOCK( &clientInfo->SpinLock, oldIrql );

			continue;
		}

		if (clientInfo->InTransition) {
		
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Client is in transition.\n") );
			
			// If it's in transition, we are better not to access RAID map or change another status.
			
			RELEASE_SPIN_LOCK( &clientInfo->SpinLock, oldIrql );
			
			continue;
		}

		RELEASE_SPIN_LOCK( &clientInfo->SpinLock, oldIrql );

		// Revive stopped node.
		
		for (i = 0; i < childCount; i++) {

			childLurn = raidInfo->MapLurnChildren[i];

			if (LURN_STATUS_STALL == childLurn->LurnStatus) {
				
				PCCB ccb;
				
				// If no more request came after returning busy because of stalling, lurn may have no chance to reconnect.
				// Send NOOP to reinitiate reconnect.
				
				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Send NOOP message to stalling node %d\n", clientInfo->RoleToNodeMap[i]) );

				status = LSCcbAllocate( &ccb );
				
				if (!NT_SUCCESS(status)) {
				
					DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("Failed allocate Ccb") );

					break;
				}

				LSCCB_INITIALIZE( ccb );
			
				ccb->OperationCode				= CCB_OPCODE_NOOP;
				ccb->HwDeviceExtension			= NULL;

				LSCcbSetFlag(ccb, CCB_FLAG_ALLOCATED);
				LSCcbSetCompletionRoutine(ccb, NULL, NULL);

				//	Send a CCB to the LURN.
				
				status = LurnRequest( childLurn, ccb );
				
				if (!NT_SUCCESS(status)) {
				
					DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("Failed to send NOOP request\n"));
					LSCcbFree(ccb);
				}

				continue;
			}

			if (clientInfo->NodeFlagsRemote[clientInfo->RoleToNodeMap[i]] & DRIX_NODE_FLAG_DEFECTIVE) {
				
				// Node is defect. No need to revive
				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Don't revive defective node %d\n", clientInfo->RoleToNodeMap[i]) );
				continue;
			}
		
			if (LURN_STATUS_RUNNING == childLurn->LurnStatus) {

				if (clientInfo->NodeFlagsLocal[clientInfo->RoleToNodeMap[i]] & DRIX_NODE_FLAG_RUNNING) {
					
					// Node is running and marked as running. Nothing to do.
					
					continue;
				}
				
				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
							("Reviving: Node %d is already in running state.\n", clientInfo->RoleToNodeMap[i]) );
				
				status = STATUS_SUCCESS;
				
				// LurnIdeDisk may have succeeded in reconnecting.
				
			} else {
				
				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Trying to revive node %d\n", clientInfo->RoleToNodeMap[i]) );
				
				// We may need to change LurnDesc if access right is updated.
				
				status = LurnInitialize( childLurn, childLurn->Lur, childLurn->SavedLurnDesc );
			}
			
			if (NT_SUCCESS(status) && LURN_IS_RUNNING(childLurn->LurnStatus)) {

				BOOLEAN	 Defected = FALSE;
				
				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Revived node %d\n", clientInfo->RoleToNodeMap[i]) );
	
				// Read rmd
				
				status = LurnExecuteSyncRead( childLurn, (PUCHAR)&rmd, NDAS_BLOCK_LOCATION_RMD, 1 );

				if (!NT_SUCCESS(status)) {

					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Failed to read RMD from node %d. Marking disk as defective\n", i) );
					
					DraidClientUpdateNodeFlags( clientInfo, 
												raidInfo->MapLurnChildren[i], 
												DRIX_NODE_FLAG_DEFECTIVE, 
												DRIX_NODE_DEFECT_BAD_DISK );
					Defected = TRUE;
				
				} else if (NDAS_RAID_META_DATA_SIGNATURE != rmd.Signature  || !IS_RMD_CRC_VALID(crc32_calc, rmd)) {

					// Check RMD signature. Currently hot-swapping is not supported. Mark new disk as defective disk.
					// To do for hot-swapping:
					// Check disk is large enough
					// Clear defective flag and mark all bit dirty
					
					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("RMD signature or CRC of node %d is not correct. May be disk is replaced with new one.\n", i) );
					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("   Currently hot-swapping is not supported\n", i) );

					DraidClientUpdateNodeFlags( clientInfo, 
												raidInfo->MapLurnChildren[i], 
												DRIX_NODE_FLAG_DEFECTIVE, 
												DRIX_NODE_DEFECT_ETC );
					Defected = TRUE;
				
				} else if (RtlCompareMemory(&rmd.RaidSetId, &clientInfo->RaidSetId, sizeof(GUID)) != sizeof(GUID)) {	// Check RMD's RAID set is same		
					
					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("RAID Set ID for node %d is not matched. Consider defective.\n", i) );

					DraidClientUpdateNodeFlags(	clientInfo, 
												raidInfo->MapLurnChildren[i], 
												DRIX_NODE_FLAG_DEFECTIVE, 
												DRIX_NODE_DEFECT_ETC );
					Defected = TRUE;
				
				} else if (RtlCompareMemory(&rmd.ConfigSetId, &clientInfo->ConfigSetId, sizeof(GUID)) != sizeof(GUID)) {

					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Configuration Set ID for node %d is not matched. Consider defective.\n", i));
					
					DraidClientUpdateNodeFlags( clientInfo, 
												raidInfo->MapLurnChildren[i], 
												DRIX_NODE_FLAG_DEFECTIVE, 
												DRIX_NODE_DEFECT_ETC );
					Defected = TRUE;
				
				} else if ((rmd.state & NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED) && i < raidInfo->nDiskCount) {

					// NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED is invalid for spare disk.
					
					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("This member is used without this disk in degraded mode. We cannot accept this disk as member\n"));
					
					DraidClientUpdateNodeFlags( clientInfo, 
												raidInfo->MapLurnChildren[i], 
												DRIX_NODE_FLAG_DEFECTIVE, 
												DRIX_NODE_DEFECT_ETC );
					Defected = TRUE;
				}
				
				if (Defected) {

					NDASSCSI_ASSERT( FALSE );

					LurnSendStopCcb( childLurn );
//					LurnDestroy(ChildLurn);

					continue;
				}
			}

			// Update status.
			
			DraidClientUpdateNodeFlags( clientInfo, raidInfo->MapLurnChildren[i], 0, 0 );
		}		
	
	} while (TRUE);
	
	DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Exiting\n") );
	
	PsTerminateSystemThread( STATUS_SUCCESS );

	return;
}


VOID
DraidClientUpdateNodeFlags (
	PDRAID_CLIENT_INFO	ClientInfo,
	PLURELATION_NODE	ChildLurn,
	UCHAR				FlagsToAdd,	// Temp parameter.. set though lurn node info.
	UCHAR 				DefectCode  // Temp parameter.. set though lurn node info.
	) 
{
	KIRQL oldIrql;
	UCHAR newFlags = ClientInfo->NodeFlagsLocal[ChildLurn->LurnChildIdx];


	ACQUIRE_SPIN_LOCK( &ClientInfo->SpinLock, &oldIrql );

	switch (ChildLurn->LurnStatus) {

	case LURN_STATUS_RUNNING:
	case LURN_STATUS_STALL:
		
		// Defective flag should not be not cleared

		newFlags &= ~(DRIX_NODE_FLAG_STOP | DRIX_NODE_FLAG_UNKNOWN);
		newFlags |= DRIX_NODE_FLAG_RUNNING;
		
		break;
	
	case LURN_STATUS_STOP_PENDING:
	case LURN_STATUS_STOP:
	case LURN_STATUS_DESTROYING:
		
		// Defective flag should not be not cleared
		newFlags &= ~(DRIX_NODE_FLAG_RUNNING | DRIX_NODE_FLAG_UNKNOWN);
		newFlags |= DRIX_NODE_FLAG_STOP;
		break;
	
	case LURN_STATUS_INIT:
	
		// Defective flag should not be not cleared
		newFlags  &= ~(DRIX_NODE_FLAG_RUNNING | DRIX_NODE_FLAG_STOP);
		newFlags |= DRIX_NODE_FLAG_UNKNOWN; 

		break;

	default:

		ASSERT(FALSE);
		break;
	}

	// Check bad sector or disk is detected. - to do: get defective info in cleaner way..
	
	if (LurnGetCauseOfFault(ChildLurn) & (LURN_FCAUSE_BAD_SECTOR|LURN_FCAUSE_BAD_DISK)) {
		
		newFlags |= DRIX_NODE_FLAG_DEFECTIVE;
		
		DefectCode = (LurnGetCauseOfFault(ChildLurn) & LURN_FCAUSE_BAD_SECTOR) ? DRIX_NODE_DEFECT_BAD_SECTOR : DRIX_NODE_DEFECT_BAD_DISK;
	}

	newFlags |= FlagsToAdd;

	// To do: if newflags contains defective flag, convert fault info's defective info to drix format...
		
	if (ClientInfo->NodeFlagsLocal[ChildLurn->LurnChildIdx] != newFlags ||
		(newFlags & DRIX_NODE_FLAG_DEFECTIVE) && ClientInfo->NodeDefectLocal[ChildLurn->LurnChildIdx] != DefectCode) {

		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
					("Changing local node %d flag from %x to %x(Defect Code=%x)\n",
					 ChildLurn->LurnChildIdx, ClientInfo->NodeFlagsLocal[ChildLurn->LurnChildIdx], newFlags, DefectCode) );
		
		// Status changed. We need to report to arbiter.
		// Set changed flags
		
		ClientInfo->NodeFlagsLocal[ChildLurn->LurnChildIdx] = newFlags;
		ClientInfo->NodeDefectLocal[ChildLurn->LurnChildIdx] = DefectCode;

		if (ClientInfo->NodeChanged[ChildLurn->LurnChildIdx]  & DRAID_NODE_CHANGE_FLAG_UPDATING) {
			
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Set node changed flag while node is updating\n") );
		}

		ClientInfo->NodeChanged[ChildLurn->LurnChildIdx] |= DRAID_NODE_CHANGE_FLAG_CHANGED;

		KeSetEvent( &ClientInfo->ClientThreadEvent,IO_NO_INCREMENT, FALSE );
	}

	RELEASE_SPIN_LOCK( &ClientInfo->SpinLock, oldIrql );
}


// Return TRUE if it is OK to terminate

BOOLEAN
DraidClientProcessTerminateRequest (
	IN PDRAID_CLIENT_INFO	ClientInfo
	) 
{
	KIRQL				oldIrql;
	PLIST_ENTRY			listEntry;
	BOOLEAN				lockInUse;
	PDRAID_CLIENT_LOCK	lock;
	NTSTATUS			status;
	BOOLEAN				lockAcquired;
	PDRIX_MSG_CONTEXT	requestMsgLink;
	BOOLEAN				msgPending;
	LARGE_INTEGER		current_time;
	

	DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("DraidClientProcessTerminateRequest entered\n") );

	ACQUIRE_SPIN_LOCK( &ClientInfo->SpinLock, &oldIrql );
	
	if (!ClientInfo->RequestToTerminate) {
		
		RELEASE_SPIN_LOCK( &ClientInfo->SpinLock, oldIrql );
		return FALSE;
	}

	DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Client termination requested\n") );

	// Delay termination if any lock is acquired.
	
	lockInUse = FALSE;
	
	for (listEntry = ClientInfo->LockList.Flink;
		 listEntry != &ClientInfo->LockList;
		 listEntry = listEntry->Flink) {

		lock = CONTAINING_RECORD (listEntry, DRAID_CLIENT_LOCK, Link);
	
		if (InterlockedCompareExchange(&lock->InUseCount, 0, 0)) {
			
			lockInUse = TRUE;
			break;
		}
	}

	if (lockInUse) {
	
		NDASSCSI_ASSERT( FALSE );

		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Lock is in use. Delaying termination\n") );
		
		RELEASE_SPIN_LOCK( &ClientInfo->SpinLock, oldIrql );
		
		return FALSE;
	}

	// Releasing all locks
	
	lockAcquired = FALSE;
	
	while (TRUE) {
	
		listEntry = RemoveHeadList( &ClientInfo->LockList );

		if (listEntry == &ClientInfo->LockList) {
			
			break;
		} 
		
		lock = CONTAINING_RECORD (listEntry, DRAID_CLIENT_LOCK, Link);
		
		if (lock->LockStatus == LOCK_STATUS_GRANTED) {
		
			lockAcquired = TRUE;
		}

		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Freeing lock before terminating\n") );
		
		DraidClientFreeLock( lock );
	}

	if (lockAcquired) {

		LARGE_INTEGER Timeout;

		Timeout.QuadPart = HZ * 5; 
		
		RELEASE_SPIN_LOCK( &ClientInfo->SpinLock, oldIrql );
		
		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Sending release all lock message\n") );
		
		status = DraidClientSendRequest( ClientInfo, DRIX_CMD_RELEASE_LOCK, 0, DRIX_LOCK_ID_ALL, 0, &Timeout, NULL, NULL );
		
		return FALSE;
	} 

	// Check any pending request exists.
	
	msgPending = FALSE;
	
	KeQueryTickCount( &current_time );
	
	for (listEntry = ClientInfo->PendingRequestList.Flink;
		 listEntry != &ClientInfo->PendingRequestList;
		 listEntry = listEntry->Flink) {

		requestMsgLink = CONTAINING_RECORD (listEntry, DRIX_MSG_CONTEXT, Link);
		
		if (requestMsgLink->HaveTimeout == FALSE ||
			(current_time.QuadPart - requestMsgLink->ReqTime.QuadPart) * KeQueryTimeIncrement() < HZ * 300) {
			
			msgPending = TRUE;
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Pending request exist. Delaying termination\n") );
		
		} else {
		
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Pending request is timed out. Ignoring.\n") );
		}
	}

	if (msgPending) {
		
		RELEASE_SPIN_LOCK( &ClientInfo->SpinLock, oldIrql );
		return FALSE;
	}

	RELEASE_SPIN_LOCK( &ClientInfo->SpinLock, oldIrql );
	
	return TRUE;
}


//
// 	Handle notification message
//	Return error if fatal error occurs so that connection should be closed.
//

NTSTATUS
DraidClientHandleNotificationMsg (
	PDRAID_CLIENT_INFO	ClientInfo,
	PDRIX_HEADER		Message
	) 
{
	PRAID_INFO			raidInfo;
	PDRIX_HEADER		replyMsg;
	KIRQL				oldIrql;
	UINT32				i;
	UINT32				replyLength;
	NTSTATUS			status= STATUS_SUCCESS;
	PLURELATION_NODE	lurn = ClientInfo->Lurn;
	PLIST_ENTRY			listEntry;
	PDRAID_CLIENT_LOCK	lock;
	UCHAR				resultCode = DRIX_RESULT_SUCCESS;
	
	
	raidInfo = lurn->LurnRAIDInfo;

	// Check data validity.

	if (NTOHL(Message->Signature) != DRIX_SIGNATURE) {

		NDASSCSI_ASSERT( FALSE );
		status = STATUS_UNSUCCESSFUL;

		return status;
	}

	if (Message->ReplyFlag != 0) {

		NDASSCSI_ASSERT( FALSE );
		status = STATUS_UNSUCCESSFUL;
		
		return status;
	}

	DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Client handling message %s\n", DrixGetCmdString(Message->Command)) );
	
	switch (Message->Command) {

	case DRIX_CMD_RETIRE: {

		BOOLEAN			lockInUse;
		ULONG			retry;
		LARGE_INTEGER	timeout;
	

		// Enter transition mode and release all locks.
		
		ACQUIRE_SPIN_LOCK( &ClientInfo->SpinLock, &oldIrql );

		ClientInfo->InTransition = TRUE;
		
		RELEASE_SPIN_LOCK( &ClientInfo->SpinLock, oldIrql );	

		for (retry=0; retry<5; retry++) {
		
			lockInUse = FALSE;
			
			ACQUIRE_SPIN_LOCK( &ClientInfo->SpinLock, &oldIrql );

			for (listEntry = ClientInfo->LockList.Flink;
				 listEntry != &ClientInfo->LockList;
				 listEntry = listEntry->Flink) {

				lock = CONTAINING_RECORD (listEntry, DRAID_CLIENT_LOCK, Link);
				
				if (InterlockedCompareExchange(&lock->InUseCount, 0, 0)) {

					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
								("Lock(Id=%I64x, Range=%I64x:%x) in use.Skipping\n", lock->LockId, lock->LockAddress, lock->LockLength) );

					lockInUse = TRUE;
					
					continue;
				}

				if (lock->LockStatus != LOCK_STATUS_GRANTED) {
					
					continue;
				}
				
				// Remove from acquired lock list
				
				listEntry = listEntry->Blink;
				
				RemoveEntryList( &lock->Link );

				DraidClientFreeLock( lock );
			}
			
			RELEASE_SPIN_LOCK( &ClientInfo->SpinLock, oldIrql );	

			if (lockInUse == FALSE) {
			
				break;
			}

			timeout.QuadPart = - HZ /2;
			
			// Wait until lock is not in use.
			KeDelayExecutionThread(KernelMode, FALSE, &timeout);
		}

		if (lockInUse == TRUE) {
			
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
						("Failed to unlock lock(Id=%I64x, Range=%I64x:%x). Returing failure to retire request\n",
						 lock->LockId, lock->LockAddress, lock->LockLength) );
				
			resultCode = DRIX_RESULT_FAIL;
		}

		break;
	}

	case DRIX_CMD_CHANGE_STATUS: {

		PDRIX_CHANGE_STATUS csMsg = (PDRIX_CHANGE_STATUS) Message;
			
		if (NTOHL(csMsg->Usn) != ClientInfo->Rmd.uiUSN) {
	
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
						("USN changed. Reload RMD (not needed until online unit change is implemented)\n") );
		}

		if (csMsg->NodeCount != ClientInfo->TotalDiskCount) {
		
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Invalid node count %d!!!\n", csMsg->NodeCount) );
			status = STATUS_UNSUCCESSFUL;
		
			return status;
		}

		ACQUIRE_SPIN_LOCK( &raidInfo->SpinLock, &oldIrql );
		ACQUIRE_DPC_SPIN_LOCK( &ClientInfo->SpinLock );

		if (csMsg->RaidStatus !=ClientInfo->DRaidStatus) {

			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
						("Changing RAID status from %x to %x!!!\n", ClientInfo->DRaidStatus ,csMsg->RaidStatus) );
		}

		if (RtlCompareMemory(&csMsg->ConfigSetId, &ClientInfo->ConfigSetId , sizeof(GUID)) != sizeof(GUID)) {
		
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Config Set Id changed!!!\n") );

			RtlCopyMemory( &ClientInfo->ConfigSetId, &csMsg->ConfigSetId, sizeof(GUID) );
			RtlCopyMemory( &raidInfo->ConfigSetId, &csMsg->ConfigSetId, sizeof(GUID) );
		}

		ClientInfo->DRaidStatus = csMsg->RaidStatus;

		for (i=0; i<csMsg->NodeCount; i++) {
			
			if (ClientInfo->NodeFlagsRemote[i] != csMsg->Node[i].NodeFlags) {
					
				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
							("Changing node %d flag from %x to %x!!!\n", i, ClientInfo->NodeFlagsRemote[i] ,csMsg->Node[i].NodeFlags) );
			}

			ClientInfo->NodeFlagsRemote[i] = csMsg->Node[i].NodeFlags;
				
			if (ClientInfo->NodeFlagsRemote[i] & DRIX_NODE_FLAG_DEFECTIVE) {
			
				// Once defective, defective until user clears it.
				
				if (!(ClientInfo->NodeFlagsLocal[i] &DRIX_NODE_FLAG_DEFECTIVE)) {

					DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
								("Setting defective flag to node %d!!!\n", i, ClientInfo->NodeFlagsRemote[i] ,csMsg->Node[i].NodeFlags));
						
					ClientInfo->NodeFlagsLocal[i] |= DRIX_NODE_FLAG_DEFECTIVE;
				}
			}

			if (ClientInfo->NodeToRoleMap[i] !=csMsg->Node[i].NodeRole) {
					
				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
							("Changing node %d role from %x to %x!!!\n", i, ClientInfo->NodeToRoleMap[i], csMsg->Node[i].NodeRole));
			}

			ClientInfo->NodeToRoleMap[i] =csMsg->Node[i].NodeRole;
			ClientInfo->RoleToNodeMap[csMsg->Node[i].NodeRole] = (UCHAR)i;
			raidInfo->MapLurnChildren[csMsg->Node[i].NodeRole] = lurn->LurnChildren[i];
		}

		if (csMsg->WaitForSync) {
				
			ClientInfo->InTransition = TRUE;

		} else {
				
			ClientInfo->InTransition = FALSE;
		}

		RELEASE_DPC_SPIN_LOCK( &ClientInfo->SpinLock );
		RELEASE_SPIN_LOCK( &raidInfo->SpinLock, oldIrql );
	
		break;
	}
	
	case DRIX_CMD_STATUS_SYNCED: { // RAID/Node status is synced. Continue IO

		LURN_EVENT lurnEvent;

		ACQUIRE_SPIN_LOCK( &ClientInfo->SpinLock, &oldIrql );
		
		ClientInfo->InTransition = FALSE;

		RELEASE_SPIN_LOCK( &ClientInfo->SpinLock, oldIrql );

		//	Set a event to make NDASSCSI fire NoOperation to trigger ndasscsi status change alarm.
	
		lurnEvent.LurnId = lurn->LurnId;
		lurnEvent.LurnEventClass = LURN_REQUEST_NOOP_EVENT;
		
		LurCallBack( lurn->Lur, &lurnEvent );	
	
		break;
	}

	case DRIX_CMD_REQ_TO_YIELD_LOCK: {

		PDRIX_REQ_TO_YIELD_LOCK yieldMsg = (PDRIX_REQ_TO_YIELD_LOCK) Message;
		UINT64					lockId = NTOHLL(yieldMsg->LockId);
		BOOLEAN					matchFound = FALSE;
	
		// Find matching lock.

		ACQUIRE_SPIN_LOCK( &ClientInfo->SpinLock, &oldIrql );

		for (listEntry = ClientInfo->LockList.Flink;
			 listEntry != &ClientInfo->LockList;
			 listEntry = listEntry->Flink) {

			lock = CONTAINING_RECORD (listEntry, DRAID_CLIENT_LOCK, Link);
			
			if (lockId == DRIX_LOCK_ID_ALL) {
				
				lock->YieldRequested = TRUE; // send release message later.
				matchFound = TRUE;
			
			} else if (lock->LockId == lockId) {
			
				lock->YieldRequested = TRUE; // send release message later.
				matchFound = TRUE;
				break;
			}
		}

		RELEASE_SPIN_LOCK( &ClientInfo->SpinLock, oldIrql );
		
		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
					("DRIX_CMD_REQ_TO_YIELD_LOCK - Lock id %I64x %s\n", lockId, matchFound ? "Found" : "Not found") );

		if (!matchFound) {
		
			// This can happen if lock is released after receiving req-to-yield msg.

			resultCode = DRIX_RESULT_INVALID_LOCK_ID;
		}
	
		break;
	}

	case DRIX_CMD_GRANT_LOCK: {

		PDRIX_GRANT_LOCK	grantMsg = (PDRIX_GRANT_LOCK) Message;
		UINT64				lockId = NTOHLL(grantMsg->LockId);
		UINT64				addr = NTOHLL(grantMsg->Addr);
		UINT32				length = NTOHL(grantMsg->Length);
		BOOLEAN				matchFound = FALSE;
		

		// Find pending lock with LockId

		ACQUIRE_SPIN_LOCK( &ClientInfo->SpinLock, &oldIrql );
		
		for (listEntry = ClientInfo->LockList.Flink;
			 listEntry != &ClientInfo->LockList;
			 listEntry = listEntry->Flink) {

			lock = CONTAINING_RECORD (listEntry, DRAID_CLIENT_LOCK, Link);
			
			if (lock->LockId == lockId) {
					
				// Update status and addr/length 
				ASSERT(lock->LockStatus == LOCK_STATUS_PENDING);
				
				lock->LockStatus = LOCK_STATUS_GRANTED;
					
				lock->LockAddress = addr;
				lock->LockLength = length;
				matchFound = TRUE;
				
				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("DRIX_CMD_GRANT_LOCK recevied: %I64x:%x\n", addr, length) );
				
				break;
			}
		}

		RELEASE_SPIN_LOCK( &ClientInfo->SpinLock, oldIrql );
			
		if (!matchFound) {

			// This can happen if DRIX_CMD_GRANT_LOCK is processed earlier than reply to ACQUIRE_LOCK
			//	Alloc lock here.

			lock = DraidClientAllocLock( lockId, grantMsg->LockType, grantMsg->LockMode, addr, length );

			if (lock == NULL) {
			
				status = STATUS_INSUFFICIENT_RESOURCES;
				
			} else {
			
				ACQUIRE_SPIN_LOCK( &ClientInfo->SpinLock, &oldIrql );

				KeQueryTickCount( &lock->LastAccessedTime);	
				lock->LockStatus = LOCK_STATUS_GRANTED;
	
				DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
							("Unrequested or unreplied lock granted. Accept anyway.lock id = %I64x, range= %I64x:%x\n", 
							 lock->LockId,  lock->LockAddress, lock->LockLength) );
				
				InsertTailList( &ClientInfo->LockList, &lock->Link );
				RELEASE_SPIN_LOCK( &ClientInfo->SpinLock, oldIrql );
			}
		}
		
		break;
	}

	default:

		NDASSCSI_ASSERT( FALSE );
		status = STATUS_UNSUCCESSFUL;

		return status;
	}

	// Create reply
	
	replyLength = sizeof(DRIX_HEADER);

	replyMsg = 	ExAllocatePoolWithTag(NonPagedPool, replyLength, DRAID_ARBITER_NOTIFY_REPLY_POOL_TAG);
	
	if (replyMsg == NULL) {
		
		status = STATUS_INSUFFICIENT_RESOURCES;
		return status;
	}

	RtlZeroMemory( replyMsg, replyLength );

	replyMsg->Signature = NTOHL(DRIX_SIGNATURE);
	replyMsg->ReplyFlag = TRUE;
	replyMsg->Command = Message->Command;
	replyMsg->Length = NTOHS((UINT16)replyLength);
	replyMsg->Sequence = Message->Sequence;
	replyMsg->Result = resultCode;

	// Send reply

	if (ClientInfo->HasLocalArbiter) {

		if (ClientInfo->NotificationReplyChannel)  {

			PDRIX_MSG_CONTEXT replyMsgEntry;

			DebugTrace( DBG_LURN_TRACE, ("DRAID Sending reply %s to local arbiter, event %p\n", 
										 DrixGetCmdString(Message->Command), &ClientInfo->NotificationReplyChannel->Event) );

			replyMsgEntry = ExAllocatePoolWithTag(NonPagedPool, sizeof(DRIX_MSG_CONTEXT), DRAID_MSG_LINK_POOL_TAG);
			
			if (replyMsgEntry) {

				RtlZeroMemory( replyMsgEntry, sizeof(DRIX_MSG_CONTEXT) );	
				InitializeListHead( &replyMsgEntry->Link );
				
				replyMsgEntry->Message = replyMsg;
				ExInterlockedInsertTailList(&ClientInfo->NotificationReplyChannel->Queue, &replyMsgEntry->Link, &ClientInfo->NotificationReplyChannel->Lock);
				KeSetEvent(&ClientInfo->NotificationReplyChannel->Event,IO_NO_INCREMENT, FALSE);
				
				return STATUS_SUCCESS;
			
			} else {
			
				ExFreePoolWithTag( replyMsg, DRAID_ARBITER_NOTIFY_REPLY_POOL_TAG );
				return STATUS_INSUFFICIENT_RESOURCES;
			}

		} else {
		
			NDASSCSI_ASSERT( FALSE );
			ExFreePoolWithTag( replyMsg, DRAID_ARBITER_NOTIFY_REPLY_POOL_TAG );

			return STATUS_UNSUCCESSFUL;
		}

	} else {

		LARGE_INTEGER	timeout;
		LONG			result;

		timeout.QuadPart = 5 * HZ;
		
		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
					("DRAID Sending reply %s to remote arbiter\n", DrixGetCmdString(Message->Command)) );
		
		if (ClientInfo->NotificationConnection.ConnectionFileObject) {

			status = LpxTdiSend( ClientInfo->NotificationConnection.ConnectionFileObject, 
								 (PUCHAR)replyMsg, 
								 replyLength,
								 0, 
								 &timeout, 
								 NULL, 
								 &result );
		} else {

			status = STATUS_UNSUCCESSFUL;
		}

		ExFreePoolWithTag( replyMsg, DRAID_ARBITER_NOTIFY_REPLY_POOL_TAG );
		
		if (DRIX_CMD_RETIRE == Message->Command) {
		
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("RETIRE message replied. Closing connect to arbiter\n") );
			DraidClientResetRemoteArbiterContext(ClientInfo);
		}

		if (NT_SUCCESS(status)) {

			return STATUS_SUCCESS;

		} else {

			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Failed to send notification reply\n") );

			return STATUS_UNSUCCESSFUL;
		}
	}
}


NTSTATUS 
DraidClientHandleCcb (
	PDRAID_CLIENT_INFO	ClientInfo
	) 
{
	PLIST_ENTRY listEntry;
	PCCB		ccb;
	NTSTATUS	status;
	
	while (listEntry = ExInterlockedRemoveHeadList(&ClientInfo->CcbQueue, &ClientInfo->SpinLock)) {

		ccb = CONTAINING_RECORD( listEntry, CCB, ListEntry );

		status = DraidClientDispatchCcb( ClientInfo, ccb );
		
		if (!NT_SUCCESS(status)) {
		
			DebugTrace( DBG_LURN_ERROR, ("DraidClientDispatchCcb failed. NTSTATUS:%08lx\n", status) );

			return STATUS_UNSUCCESSFUL;
		}
	}

	return STATUS_SUCCESS;
}


NTSTATUS 
DraidClientDispatchCcb (
	PDRAID_CLIENT_INFO	ClientInfo, 
	PCCB				Ccb
	)
{
	NTSTATUS			status = STATUS_SUCCESS;
	PLURELATION_NODE	lurn = ClientInfo->Lurn;


	if (Ccb->OperationCode != CCB_OPCODE_EXECUTE) {

		NDASSCSI_ASSERT( FALSE );
		Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
		goto complete_here;
	}
		
	// Handle 
	// CCB_OPCODE_EXECUTE with
	// SCSIOP_WRITE, SCSIOP_WRITE16, READ_LONG, 
	// SCSIOP_READ, SCSIOP_READ16, SCSIOP_VERIFY, SCSIOP_VERIFY16 only.
	// Other command should not be queued.

	switch(Ccb->Cdb[0]) {

	case SCSIOP_WRITE:
	case SCSIOP_WRITE16: {

		DebugTrace( DBG_LURN_NOISE,("SCSIOP_WRITE\n") );

		DebugTrace( DBG_LURN_NOISE,("W Ccb->DataBufferLength %d\n", Ccb->DataBufferLength) );

		if (lurn->Lur->EnabledNdasFeatures & NDASFEATURE_SECONDARY) {

			// Fake write is handled by LurProcessWrite.
			// Just check assumptions
		
			NDASSCSI_ASSERT( lurn->Lur->EnabledNdasFeatures & NDASFEATURE_SIMULTANEOUS_WRITE );
		}

			
#if __ALLOW_WRITE_WHEN_NO_ARBITER_STATUS__

		if (ClientInfo->ClientState == DRAID_CLIENT_STATE_NO_ARBITER) {
		
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Handling write in no arbiter mode. Attempt to write without range locking\n") );
				
			// Set force unit access flag.

			if (Ccb->Cdb[0] == SCSIOP_WRITE) {
				
				PCDB	cdb;
				
				cdb = (PCDB)Ccb->Cdb;
				cdb->CDB10.ForceUnitAccess = TRUE;

			} else {

				PCDBEXT cdb16;

				cdb16 = (PCDBEXT)Ccb->Cdb;
				cdb16->CDB16.ForceUnitAccess = TRUE;
			}

			DraidClientSendCcbToNondefectiveChildren( ClientInfo, Ccb, LurnRAID1RCcbCompletion, TRUE );

			break; // break from case SCSIOP_WRITE:
		}
#endif

		status = DraidClientIoWithLock( ClientInfo, DRIX_LOCK_MODE_EX, Ccb );

		if (status == STATUS_SUCCESS) {

			// Ccb is handled or queued to pending message context.
			
		} else if (status == STATUS_PENDING)  {
			
			// Lock is still not granted
 
			Ccb->CcbStatus = CCB_STATUS_BUSY;
			goto complete_here;

		} else {

			DebugTrace( DBG_LURN_ERROR,("SCSIOP_WRITE error %x.\n", status) );
			// Pass error. 
		}

		break;
	}

	case SCSIOP_VERIFY:
	case SCSIOP_VERIFY16: {


		DebugTrace( DBG_LURN_NOISE, ("SCSIOP_VERIFY\n") );

		status = DraidClientSendCcbToNondefectiveChildren( ClientInfo, Ccb, LurnRAID1RCcbCompletion, FALSE );

		break;
	}

	case 0x3E:		// READ_LONG
	case SCSIOP_READ:
	case SCSIOP_READ16: {

		ULONG role_idx;
		
		//	Find a child LURN to run.
		
		role_idx = DraidClientRAID1RSelectChildToRead( lurn->LurnRAIDInfo, Ccb );

		if (role_idx == (ULONG)-1) {
		
			// Error status.
			Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
			goto complete_here;
		}

		DebugTrace(DBG_LURN_TRACE,("SCSIOP_READ: decided LURN#%d\n", role_idx));

		
		//	Set completion routine
		
		status = LurnAssocSendCcbToChildrenArray( &lurn->LurnRAIDInfo->MapLurnChildren[role_idx],
												  1,
												  Ccb,
												  LurnRAID1RCcbCompletion,
												  NULL,
												  NULL,
												  LURN_CASCADE_FORWARD );
		}

		break;

	default:
		
		// This should not happen.
		NDASSCSI_ASSERT( FALSE );
		break;
	}
	
	// Ccb still pending

	return status;

complete_here:

	// We can complete ccb now.
	LSAssocSetRedundentRaidStatusFlag( lurn, Ccb );
	LSCcbCompleteCcb( Ccb );	

	return status;
}


//
// Can be called with spinlock held by SCSIPORT
//

NTSTATUS
DraidClientFlush (
	PDRAID_CLIENT_INFO		ClientInfo, 
	PCCB					Ccb, 
	CCB_COMPLETION_ROUTINE	CompRoutine
	)
{
	KIRQL oldIrql;
	
	
	DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("%s Called\n", __FUNCTION__) );
	
	NDASSCSI_ASSERT( ClientInfo );

	// Flush in-memory cache(currently we don't have) and release locks

	ACQUIRE_SPIN_LOCK( &ClientInfo->SpinLock, &oldIrql );	

	if (ClientInfo->RequestForFlush) {

		// FLUSH is already in progress
		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("FLUSH is already in progress.\n") );
		
		RELEASE_SPIN_LOCK(&ClientInfo->SpinLock, oldIrql);	
		
		return STATUS_SUCCESS;
	}
	
	ClientInfo->RequestForFlush = TRUE;
	ClientInfo->FlushCcb = Ccb;
	ClientInfo->FlushCompletionRoutine = CompRoutine;
	
	KeSetEvent( &ClientInfo->ClientThreadEvent,IO_NO_INCREMENT, FALSE );

	RELEASE_SPIN_LOCK( &ClientInfo->SpinLock, oldIrql );

	return STATUS_PENDING;
}


VOID
DraidClientFreeLock (
	PDRAID_CLIENT_LOCK Lock
	)
{
	ExFreePoolWithTag( Lock, DRAID_CLIENT_LOCK_POOL_TAG );
}


NTSTATUS
DraidClientRecvHeaderAsync (
	PDRAID_CLIENT_INFO			Client,
	PDRAID_CLIENT_CONNECTION	Connection
	) 
{
	NTSTATUS		status;
	PLARGE_INTEGER	timeout;
	KIRQL			oldIrql;

	
	UNREFERENCED_PARAMETER( Client );
	
	DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("RxPendingCount = %x\n",Connection->RxPendingCount) );

	ACQUIRE_SPIN_LOCK( &Client->SpinLock, &oldIrql );
	
	if (!Connection->ConnectionFileObject) {

		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Connection is closed\n") );
		RELEASE_SPIN_LOCK( &Client->SpinLock, oldIrql );	
	
		return STATUS_UNSUCCESSFUL;

	} else if (Connection->TdiReceiveContext.Result < 0) {
	
		NDASSCSI_ASSERT( FALSE );
		RELEASE_SPIN_LOCK(&Client->SpinLock, oldIrql);	

		return STATUS_UNSUCCESSFUL;
	}
	
	RELEASE_SPIN_LOCK(&Client->SpinLock, oldIrql);
	
	if (InterlockedCompareExchange(&Connection->Waiting, 1, 0) == 0) {

		Connection->TdiReceiveContext.Irp = NULL;
		Connection->TdiReceiveContext.Result = 0;
		KeInitializeEvent(&Connection->TdiReceiveContext.CompletionEvent, NotificationEvent, FALSE) ;
	
		if (Connection->Timeout.QuadPart) {

			timeout = &Connection->Timeout;

		} else {

			timeout = NULL;
		}

#if DBG
		RtlZeroMemory(Connection->ReceiveBuf, sizeof(Connection->ReceiveBuf));
#endif

		status = LpxTdiRecvWithCompletionEvent( Connection->ConnectionFileObject,
												&Connection->TdiReceiveContext,
												Connection->ReceiveBuf,
												sizeof(DRIX_HEADER),
												0, 
												NULL, 
												timeout );

		if (!NT_SUCCESS(status)) {
		
			DebugTrace(DBG_LURN_ERROR, ("Failed to recv async\n"));
		}
		
	}else {

		DebugTrace(DBG_LURN_ERROR, ("Previous recv is pending\n"));
		InterlockedIncrement(&Connection->RxPendingCount);

		// Already waiting 
		status = STATUS_PENDING;
	}

	return status;
}
