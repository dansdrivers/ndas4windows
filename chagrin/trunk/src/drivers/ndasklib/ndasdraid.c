#include "ndasscsiproc.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__

#define __MODULE__ "nsdraid"


//
//	RAID1(mirroring V2) online-recovery interface
//

NTSTATUS
RAID1RLurnInitialize (
	IN PLURELATION_NODE			Lurn,
	IN PLURELATION_NODE_DESC	LurnDesc
	);

NTSTATUS
RAID1RLurnRequest (
	IN PLURELATION_NODE	Lurn,
	IN PCCB				Ccb
	);

NTSTATUS
RAID1RLurnDestroy (
	IN PLURELATION_NODE Lurn
	);

LURN_INTERFACE LurnRAID1RInterface = {
	
	LSSTRUC_TYPE_LURN_INTERFACE,
	sizeof(LURN_INTERFACE),
	LURN_RAID1R,
	0,

	{
		RAID1RLurnInitialize,
		RAID1RLurnDestroy,
		RAID1RLurnRequest
	}
};


//////////////////////////////////////////////////////////////////////////
//
//	RAID1 Lurn
//

NTSTATUS
RAID1RLurnInitialize (
	IN PLURELATION_NODE			Lurn,
	IN PLURELATION_NODE_DESC	LurnDesc
	) 
{
	NTSTATUS	status;
	PRAID_INFO	raidInfo = NULL;


	NDASSCSI_ASSERT( KeGetCurrentIrql() ==  PASSIVE_LEVEL );


	DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("LurnDesc->LurnType = %d\n", LurnDesc->LurnType) );
	DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("LurnDesc->LurnId = %d\n", LurnDesc->LurnId) );
	DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("LurnDesc->StartBlockAddr = 0x%I64x\n", LurnDesc->StartBlockAddr) );
	DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("LurnDesc->EndBlockAddr = 0x%I64x\n", LurnDesc->EndBlockAddr) );
	DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("LurnDesc->UnitBlocks = 0x%I64x\n", LurnDesc->UnitBlocks) );
	DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("LurnDesc->MaxDataSendLength = %d bytes\n", LurnDesc->MaxDataSendLength) );
	DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("LurnDesc->MaxDataRecvLength = %d bytes\n", LurnDesc->MaxDataRecvLength) );
	DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("LurnDesc->LurnOptions = %d\n", LurnDesc->LurnOptions) );
	DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("LurnDesc->LurnParent = %d\n", LurnDesc->LurnParent) );
	DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("LurnDesc->LurnChildrenCnt = %d\n", LurnDesc->LurnChildrenCnt) );
	DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("LurnDesc->LurnChildren = 0x%p\n", LurnDesc->LurnChildren) );

	//
	// Determine block bytes.
	//

	Lurn->ChildBlockBytes = LurnAsscGetChildBlockBytes( Lurn );

	if (Lurn->ChildBlockBytes == 0) {

		NDASSCSI_ASSERT( FALSE );
		return STATUS_DEVICE_NOT_READY;
	}

	Lurn->BlockBytes = Lurn->ChildBlockBytes;

	//	Raid Information

	Lurn->LurnRAIDInfo = ExAllocatePoolWithTag( NonPagedPool, sizeof(RAID_INFO), RAID_INFO_POOL_TAG );

	if (Lurn->LurnRAIDInfo == NULL) {

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	do {

		raidInfo = Lurn->LurnRAIDInfo;

		RtlZeroMemory( raidInfo, sizeof(RAID_INFO) );

		KeInitializeSpinLock( &raidInfo->SpinLock );

		// set spare disk count

		raidInfo->nDiskCount	= LurnDesc->LurnChildrenCnt - LurnDesc->LurnInfoRAID.nSpareDisk;
		raidInfo->nSpareDisk	= LurnDesc->LurnInfoRAID.nSpareDisk;
		raidInfo->SectorsPerBit = LurnDesc->LurnInfoRAID.SectorsPerBit;
		raidInfo->RaidSetId		= LurnDesc->LurnInfoRAID.RaidSetId;
		raidInfo->ConfigSetId	= LurnDesc->LurnInfoRAID.ConfigSetId;
	
		if (raidInfo->SectorsPerBit == 0) {

			NDASSCSI_ASSERT( FALSE );
			DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("SectorsPerBit is zero!\n") );

			status = STATUS_INVALID_PARAMETER;

			break;
		}

		raidInfo->MaxDataSendLength = LurnDesc->MaxDataSendLength;
		raidInfo->MaxDataRecvLength = LurnDesc->MaxDataRecvLength;

		// Always create draid client.
	
		status = DraidClientStart( Lurn ); 
	
		if (!NT_SUCCESS(status)) {

			break;
		}

		NDASSCSI_ASSERT( status == STATUS_SUCCESS );
	
	} while (0);

	if (!NT_SUCCESS(status)) {

		if (Lurn->LurnRAIDInfo) {

			ExFreePoolWithTag( Lurn->LurnRAIDInfo, RAID_INFO_POOL_TAG );
		}
	}
	
	return status;
}


NTSTATUS
RAID1RLurnDestroy (
	IN PLURELATION_NODE Lurn
	) 
{
	NDASSCSI_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	NDASSCSI_ASSERT( Lurn->LurnRAIDInfo );

	ExFreePoolWithTag( Lurn->LurnRAIDInfo, RAID_INFO_POOL_TAG );

	Lurn->LurnRAIDInfo = NULL;

	return STATUS_SUCCESS;
}

//
// to do: restruct this function!
//

NTSTATUS
LurnRAID1RCcbCompletion (
	IN PCCB	Ccb,
	IN PCCB	OriginalCcb
	)
{
	KIRQL				oldIrql;
	LONG				assocCount;
	NTSTATUS			status;
	PRAID_INFO			raidInfo;
	PLURELATION_NODE	lurnOriginal;
	PLURELATION_NODE	lurnCurrent;
	PDRAID_CLIENT_INFO	client;
	UINT32				draidStatus;


	status = STATUS_SUCCESS;

	ACQUIRE_SPIN_LOCK( &OriginalCcb->CcbSpinLock, &oldIrql );

	lurnOriginal = OriginalCcb->CcbCurrentStackLocation->Lurn;
	lurnCurrent = Ccb->CcbCurrentStackLocation->Lurn;
	
	raidInfo = lurnOriginal->LurnRAIDInfo;
	
	ACQUIRE_DPC_SPIN_LOCK( &raidInfo->SpinLock );
	
	client = raidInfo->pDraidClient;
	
	if (client == NULL) {
	
		// Client is alreay terminated.
		draidStatus = DRIX_RAID_STATUS_TERMINATED;

	} else {
	
		draidStatus = client->DRaidStatus;
		DraidClientUpdateNodeFlags( client, lurnCurrent, 0, 0 );
	}

	RELEASE_DPC_SPIN_LOCK( &raidInfo->SpinLock );

	// 
	// Find proper Ccbstatus based on OriginalCcb->CcbStatus(Empty or may contain first completed child Ccb's staus) and this Ccb.
	//

	if (Ccb->CcbStatus != CCB_STATUS_SUCCESS) {

		DebugTrace( NDASSCSI_DEBUG_LURN_NOISE, ("LurnRAID1RCcbCompletion: CcbStatus = %x\n", Ccb->CcbStatus) );
	}

	if (Ccb->OperationCode == CCB_OPCODE_EXECUTE) {

		switch (Ccb->Cdb[0]) {
		
		case 0x3E:			// READ_LONG
		case SCSIOP_READ:
		case SCSIOP_READ16:	

			//
			// Read request is sent to only one target.
			// If success, pass through to original ccb
			// If error and 
			// 	another target is running state, return busy to make upper layer to retry to another host.
			// 	another target is down, pass this Ccb status to original ccb
			//
			
			if (Ccb->CcbStatus == CCB_STATUS_SUCCESS) {
			
				OriginalCcb->CcbStatus = Ccb->CcbStatus;
				break;
			
			} else {
			
				if (client && client->DRaidStatus == DRIX_RAID_STATUS_NORMAL) {

					// Maybe another node is in running state. Hope that node can handle request and return busy
					
					NDASSCSI_ASSERT( OriginalCcb->Srb || OriginalCcb->OperationCode == CCB_OPCODE_RESETBUS );
					DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("Read on RAID1 failed. Enter emergency and return busy for retrial on redundent target\n") );

					OriginalCcb->CcbStatus = CCB_STATUS_BUSY;

				} else {

					// Error in degraded mode. Pass error 
					DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("Read on RAID1 failed and redundent target is in defective. Returning error %x\n", Ccb->CcbStatus) );
	 
					
					// No other target is alive. Pass error including sense data
					OriginalCcb->CcbStatus = Ccb->CcbStatus;

					if (OriginalCcb->SenseBuffer != NULL) {
					
						RtlCopyMemory(OriginalCcb->SenseBuffer, Ccb->SenseBuffer, Ccb->SenseDataLength);
					}
				}
			}

			break;

		case SCSIOP_WRITE:
		case SCSIOP_WRITE16:
		case SCSIOP_VERIFY:
		case SCSIOP_VERIFY16:		 // fall through
		default:

			//
			// CCB Status priority(Higher level of error override another device's error) : In high to low order.
			//	Etc error(Invalid command, out of bound, unknown error)
			//	CCB_STATUS_COMMAND_FAILED(Errors that upper layer need to know about error - bad sector, etc)
			//	CCB_STATUS_BUSY: (lower this status to below success priority to improve responsiveness??)
			//	CCB_STATUS_SUCCES: 
			//	CCB_STATUS_STOP/CCB_STATUS_NOT_EXIST/CCB_STATUS_COMMUNICATION_ERROR: Enter emergency if in normal status.
			//														Return as success if another target is success, or 
			//

			switch (Ccb->CcbStatus) {
			
			case CCB_STATUS_SUCCESS:
			
				if (OriginalCcb->CcbStatus == CCB_STATUS_STOP ||
					OriginalCcb->CcbStatus == CCB_STATUS_NOT_EXIST ||
					OriginalCcb->CcbStatus == CCB_STATUS_COMMUNICATION_ERROR) {

					// Make upper layer retry.
					OriginalCcb->CcbStatus = CCB_STATUS_BUSY;

					ACQUIRE_DPC_SPIN_LOCK( &raidInfo->SpinLock );
					
					if (raidInfo->pDraidClient) {

						client = raidInfo->pDraidClient;
						
						ACQUIRE_DPC_SPIN_LOCK( &client->SpinLock );
						
						if (client->DRaidStatus == DRIX_RAID_STATUS_DEGRADED) {
		
							if (!(client->NodeFlagsRemote[lurnCurrent->LurnChildIdx] & 
								(DRIX_NODE_FLAG_STOP|DRIX_NODE_FLAG_DEFECTIVE | DRIX_NODE_FLAG_OUT_OF_SYNC))) {

								// Succeeded node is none-fault node. It is okay.
								OriginalCcb->CcbStatus = CCB_STATUS_SUCCESS;

							} else {

								// None-fault node has failed, this cause RAID fail. Need to stop
								DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("Non-defective target %d failed. RAID failure\n", 
																		 lurnCurrent->LurnChildIdx) );
								OriginalCcb->CcbStatus = CCB_STATUS_STOP;
							}

						} else {

							// No target should fail in other cases. If so, RAID status will be changed. Wait for it returning busy.
							DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, 
										("Ccb for target %d failed not in degraded mode. Returning busy.\n", lurnCurrent->LurnChildIdx));
							OriginalCcb->CcbStatus = CCB_STATUS_BUSY;						
						}

						RELEASE_DPC_SPIN_LOCK( &client->SpinLock );

					} else {

						DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, 
									("RAID client already terminated OriginalCcb->CcbStatus = %x, Ccb = %p, OriginalCcb = %p, OriginalCcb->Cdb[0] = %s\n", 
									  OriginalCcb->CcbStatus, Ccb, OriginalCcb, CdbOperationString(OriginalCcb->Cdb[0])) );

						// RAID has terminated. Pass status
						OriginalCcb->CcbStatus = CCB_STATUS_STOP;
					}

					RELEASE_DPC_SPIN_LOCK( &raidInfo->SpinLock );

				} else if (OriginalCcb->CcbStatus == CCB_STATUS_SUCCESS) {

					// Both target returned success or another target has not completed yet
					// Set success
					OriginalCcb->CcbStatus = CCB_STATUS_SUCCESS;

				} else {

					// All other error including CCB_STATUS_BUSY & CCB_STATUS_COMMAND_FAILED
					// OriginalCcb is already filled with error info, so just keep them.
				}

				break;

			case CCB_STATUS_STOP:
			case CCB_STATUS_NOT_EXIST:
			case CCB_STATUS_COMMUNICATION_ERROR: 

				if (OriginalCcb->ChildReqCount == 1) {

					// Request sent to only one host. Pass error to upper layer.
					OriginalCcb->CcbStatus = Ccb->CcbStatus;

					if (OriginalCcb->SenseBuffer != NULL) {

						RtlCopyMemory( OriginalCcb->SenseBuffer, Ccb->SenseBuffer, Ccb->SenseDataLength );
					}

				} else if (OriginalCcb->AssociateCount == 2) {
					
					// Another target host didn't return yet. Fill Ccb with current status
					 
					OriginalCcb->CcbStatus = Ccb->CcbStatus;
					
					if (OriginalCcb->SenseBuffer != NULL) {
					
						RtlCopyMemory( OriginalCcb->SenseBuffer, Ccb->SenseBuffer, Ccb->SenseDataLength );
					}

					ACQUIRE_DPC_SPIN_LOCK( &raidInfo->SpinLock );

					if (raidInfo->pDraidClient) {

						client = raidInfo->pDraidClient;
						
						ACQUIRE_DPC_SPIN_LOCK( &client->SpinLock );

						if (client->DRaidStatus == DRIX_RAID_STATUS_DEGRADED) {
			
							if (client->NodeFlagsRemote[lurnCurrent->LurnChildIdx] & 		
								(DRIX_NODE_FLAG_STOP|DRIX_NODE_FLAG_DEFECTIVE | DRIX_NODE_FLAG_OUT_OF_SYNC)) {

								// Fault target has failed. It is okay. Pass code.

							} else {

								// Normal target has failed, this cause RAID fail. Need to stop
								DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, 
											("Non-defective target %d failed. RAID failure\n", lurnCurrent->LurnChildIdx));
								OriginalCcb->CcbStatus = CCB_STATUS_STOP;
							}
		
						} else {

							// One of target is stopped not in degraded mode.
							DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, 
										("Ccb for target %d failed not in degraded mode.\n", lurnCurrent->LurnChildIdx));
							OriginalCcb->CcbStatus = CCB_STATUS_STOP;						
						}

						RELEASE_DPC_SPIN_LOCK( &client->SpinLock );						

					} else {

						// RAID has terminated. Pass status
					}

					RELEASE_DPC_SPIN_LOCK( &raidInfo->SpinLock );

				} else if (OriginalCcb->AssociateCount ==1) {
					
					// Another target has completed already.  Check its status
					
					switch (OriginalCcb->CcbStatus) {
					
					case CCB_STATUS_SUCCESS:
					
						ACQUIRE_DPC_SPIN_LOCK( &raidInfo->SpinLock );

						if (raidInfo->pDraidClient) {

							client = raidInfo->pDraidClient;
							ACQUIRE_DPC_SPIN_LOCK( &client->SpinLock );
							
							// One target has succeeded, another target has failed. 
							// If RAID is degraded status and failed target is already recognized as fault unit, it is okay
							//	But if not return busy.
							
							if (client->DRaidStatus == DRIX_RAID_STATUS_DEGRADED) {
							
								if (client->NodeFlagsRemote[lurnCurrent->LurnChildIdx] & 
									(DRIX_NODE_FLAG_STOP|DRIX_NODE_FLAG_DEFECTIVE | DRIX_NODE_FLAG_OUT_OF_SYNC)) {

									// Failed target is already marked as failure. This is expected situation.
									OriginalCcb->CcbStatus = CCB_STATUS_SUCCESS;

								} else {

									// Failed target is not marked as failed. 
									DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, 
												("Non-defective target %d failed. Target flag = %x, RAID failure\n", 
												 lurnCurrent->LurnChildIdx) );

									OriginalCcb->CcbStatus = CCB_STATUS_BUSY;
								}

							} else {
								
								// No target should fail in other cases. If so, RAID status will be changed. Wait for it returning busy.
								DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, 
											("One of the target ccb failed not in degraded mode. Returning busy.\n") );
								OriginalCcb->CcbStatus = CCB_STATUS_BUSY;
							}

							RELEASE_DPC_SPIN_LOCK( &client->SpinLock );

						} else {
							
							// client stopped
							OriginalCcb->CcbStatus = CCB_STATUS_STOP;
						}

						RELEASE_DPC_SPIN_LOCK( &raidInfo->SpinLock );
						break;

					case CCB_STATUS_STOP:
					case CCB_STATUS_NOT_EXIST:
					case CCB_STATUS_COMMUNICATION_ERROR: 
						
						DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("Both target returned error\n") );
						// both target completed with error
						// pass error.
						break;

					case CCB_STATUS_BUSY:
					case CCB_STATUS_COMMAND_FAILED:					
					default:
						
						// CCB_STATUS_BUSY,CCB_STATUS_COMMAND_FAILED has higher priority.
						// Go with OriginalCcb's status.
						break;
					}
				}

				break;

			case CCB_STATUS_BUSY:

				if (OriginalCcb->CcbStatus != CCB_STATUS_SUCCESS &&
					OriginalCcb->CcbStatus != CCB_STATUS_STOP &&
					OriginalCcb->CcbStatus != CCB_STATUS_NOT_EXIST &&
					OriginalCcb->CcbStatus != CCB_STATUS_COMMUNICATION_ERROR) {

					// FAIL and unknown error. Preserve previous error

				} else {

					// Overwrite low priority errors
					OriginalCcb->CcbStatus = Ccb->CcbStatus;
					
					if(OriginalCcb->SenseBuffer != NULL) {
						RtlCopyMemory(OriginalCcb->SenseBuffer, Ccb->SenseBuffer, Ccb->SenseDataLength);
					}
				}

				break;	

			case CCB_STATUS_COMMAND_FAILED:
		
				if (OriginalCcb->CcbStatus != CCB_STATUS_BUSY		&&
					OriginalCcb->CcbStatus != CCB_STATUS_SUCCESS	&&
					OriginalCcb->CcbStatus != CCB_STATUS_STOP		&&
					OriginalCcb->CcbStatus != CCB_STATUS_NOT_EXIST	&&
					OriginalCcb->CcbStatus != CCB_STATUS_COMMUNICATION_ERROR) {

						// Unknown error. Preserve previous error
		
				} else {

					// Overwrite low priority errors
					OriginalCcb->CcbStatus = Ccb->CcbStatus;

					if(OriginalCcb->SenseBuffer != NULL) {
					
						RtlCopyMemory(OriginalCcb->SenseBuffer, Ccb->SenseBuffer, Ccb->SenseDataLength);
					}
				}

			break;

			default:

				//
				// Pass error to upper level regardless result of another child.
				//
				// To do: STATUS and error code from the target completed late overwrite previous status. Need to combine two error.
				//

				if (OriginalCcb->CcbStatus == CCB_STATUS_BUSY) {

					OriginalCcb->CcbStatus = Ccb->CcbStatus;

					if (OriginalCcb->SenseBuffer != NULL) {

						RtlCopyMemory( OriginalCcb->SenseBuffer, Ccb->SenseBuffer, Ccb->SenseDataLength );
					}
				}
			
				break;
			}

			break;
		}

	}else {

		NDASSCSI_ASSERT( FALSE );
		OriginalCcb->CcbStatus = CCB_STATUS_SUCCESS;			
	}

	if (Ccb->OperationCode != CCB_OPCODE_STOP) {

		if (DRIX_RAID_STATUS_FAILED ==  draidStatus || DRIX_RAID_STATUS_TERMINATED == draidStatus) {

			OriginalCcb->CcbStatus = CCB_STATUS_STOP;
			DebugTrace( NDASSCSI_DEBUG_LURN_TRACE,("RAID status is %d. Ccb status stop\n", draidStatus) );
		}
	}

	LSCcbSetStatusFlag(	OriginalCcb, Ccb->CcbStatusFlags & CCBSTATUS_FLAG_ASSIGNMASK );

	RELEASE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, oldIrql);

	//	Complete the original CCB

	assocCount = InterlockedDecrement( &OriginalCcb->AssociateCount );

	NDASSCSI_ASSERT( assocCount >= 0 );

	if (assocCount != 0) {

		return status;
	}

	if (OriginalCcb->CcbStatus != CCB_STATUS_SUCCESS) {

		DebugTrace( NDASSCSI_DEBUG_LURN_NOISE, ("Completing Ccb with status %x\n", OriginalCcb->CcbStatus) );
	}

	if (client && 
		(Ccb->OperationCode == CCB_OPCODE_EXECUTE) &&
		(OriginalCcb->Cdb[0] == SCSIOP_WRITE ||OriginalCcb->Cdb[0] == SCSIOP_WRITE16)) {
		DraidReleaseBlockIoPermissionToClient( client, OriginalCcb );
	}

	LSAssocSetRedundentRaidStatusFlag( lurnOriginal, OriginalCcb );
	LSCcbCompleteCcb( OriginalCcb );

	return STATUS_SUCCESS;
}


NTSTATUS
LurnRAID1RFlushCompletionForStopUnit (
	IN PCCB	Ccb,	
	IN PVOID Param // Not used.
	) 
{
	NTSTATUS status;

	UNREFERENCED_PARAMETER( Param );

	// Ccb is not for this completion. Send to all children.
	
	status = LurnAssocSendCcbToAllChildren( Ccb->CcbCurrentStackLocation->Lurn,
											Ccb,
											LurnRAID1RCcbCompletion,
											NULL,
											NULL,
											LURN_CASCADE_FORWARD );

	return STATUS_SUCCESS;
}


NTSTATUS
LurnRAID1RExecute (
	IN	PLURELATION_NODE	Lurn,
	IN	PCCB				Ccb
	) 
{
	NTSTATUS			status;
	PRAID_INFO			raidInfo;
	KIRQL				oldIrql;
	PDRAID_CLIENT_INFO	client;

	raidInfo = Lurn->LurnRAIDInfo;

	// Forward disk IO related request to Client thread
	
	ACQUIRE_SPIN_LOCK( &raidInfo->SpinLock, &oldIrql );

	client = raidInfo->pDraidClient;
	
	if (client == NULL) {

		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
					("client == NULL Returning stop status Ccb->Cdb[0] =%s\n", CdbOperationString(Ccb->Cdb[0])) );

		RELEASE_SPIN_LOCK( &raidInfo->SpinLock, oldIrql );

		goto nextStep;

		Ccb->CcbStatus = CCB_STATUS_STOP;

		goto complete_here;
	}

	ACQUIRE_DPC_SPIN_LOCK( &client->SpinLock );

	if (client->DRaidStatus == DRIX_RAID_STATUS_TERMINATED) {

		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, 
					("client->DRaidStatus == DRIX_RAID_STATUS_TERMINATED Returning stop status Ccb->Cdb[0] =%x\n", Ccb->Cdb[0]) );

		RELEASE_DPC_SPIN_LOCK( &client->SpinLock );
		RELEASE_SPIN_LOCK( &raidInfo->SpinLock, oldIrql );
		Ccb->CcbStatus = CCB_STATUS_STOP;

		goto complete_here;
	}

	if (client->ClientState == DRAID_CLIENT_STATE_NO_ARBITER) {

		if (Ccb->Cdb[0] == SCSIOP_WRITE || Ccb->Cdb[0] == SCSIOP_WRITE16) {

			ASSERT( !FlagOn(Lurn->Lur->EnabledNdasFeatures, NDASFEATURE_SECONDARY) );

#if __ALLOW_WRITE_WHEN_NO_ARBITER_STATUS__
			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("Allowing writing in no arbiter mode\n") );
#else

			// We cannot handle write in no arbiter mode.

			RELEASE_DPC_SPIN_LOCK( &client->SpinLock );
			RELEASE_SPIN_LOCK( &raidInfo->SpinLock, oldIrql );

			DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("Cannot handle write in no arbiter mode. Returning busy\n") );
			Ccb->CcbStatus = CCB_STATUS_BUSY;
			goto complete_here;

#endif /* __ALLOW_WRITE_WHEN_NO_ARBITER_STATUS__ */

		}

		if (client->InTransition) {

			DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("Allowing operation in transition status.\n") );
		}

		// Allow read when no arbiter exist.
			
	} else if (client->InTransition) {

		//
		// RAID can handle some operations even in transition
		//

		switch (Ccb->Cdb[0]) {

			case SCSIOP_WRITE:
			case SCSIOP_WRITE16:		
			case 0x3E:		// READ_LONG
			case SCSIOP_READ:
			case SCSIOP_READ16:		
			case SCSIOP_VERIFY:
			case SCSIOP_VERIFY16:	

				RELEASE_DPC_SPIN_LOCK( &client->SpinLock );
				RELEASE_SPIN_LOCK( &raidInfo->SpinLock, oldIrql );

				DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, 
							("IO(opcode=%x) cannot be process in transition. Returning busy\n", Ccb->Cdb[0]) );

				Ccb->CcbStatus = CCB_STATUS_BUSY;
				goto complete_here;
					
			default:
					
				DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, 
							("Allowing operation in transition status: %x.\n", Ccb->Cdb[0]) );

				break;
		}
	}

	switch (Ccb->Cdb[0]) {
			
		case SCSIOP_WRITE:
		case SCSIOP_WRITE16:		
		case 0x3E:		// READ_LONG
		case SCSIOP_READ:
		case SCSIOP_READ16:		
		case SCSIOP_VERIFY:
		case SCSIOP_VERIFY16:		
		
			// These command will be handled by client thread

			InsertTailList( &client->CcbQueue, &Ccb->ListEntry );
			KeSetEvent( &client->ClientThreadEvent, IO_NO_INCREMENT, FALSE );
			LSCcbMarkCcbAsPending( Ccb );
					
			RELEASE_DPC_SPIN_LOCK( &client->SpinLock );
			RELEASE_SPIN_LOCK( &raidInfo->SpinLock, oldIrql );
					
			return STATUS_PENDING; // no meaning..

		default:

			break;
	}

	RELEASE_DPC_SPIN_LOCK( &client->SpinLock );

	RELEASE_SPIN_LOCK( &raidInfo->SpinLock, oldIrql );

nextStep:

	switch (Ccb->Cdb[0]) {

	case SCSIOP_INQUIRY: {
		
		INQUIRYDATA	inquiryData;
		UCHAR		model[16] = RAID1R_MODEL_NAME;

		DebugTrace( NDASSCSI_DEBUG_LURN_TRACE,("SCSIOP_INQUIRY Ccb->Lun = 0x%x\n", Ccb->LurId[2]) );
		
		//	We don't support EVPD(enable vital product data).
		
		if (Ccb->Cdb[1]  & CDB_INQUIRY_EVPD) {

			DebugTrace( NDASSCSI_DEBUG_LURN_ERROR, ("SCSIOP_INQUIRY: got EVPD. Not supported.\n") );

			LSCCB_SETSENSE( Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0 );
			LSCcbSetStatus( Ccb, CCB_STATUS_COMMAND_FAILED );
			goto complete_here;
		}

		RtlZeroMemory( Ccb->DataBuffer, Ccb->DataBufferLength );
		RtlZeroMemory( &inquiryData, sizeof(INQUIRYDATA) );

		inquiryData.DeviceType = DIRECT_ACCESS_DEVICE;
		inquiryData.DeviceTypeQualifier = DEVICE_QUALIFIER_ACTIVE;

		inquiryData.DeviceTypeModifier; // : 7;
		inquiryData.RemovableMedia = FALSE;

		inquiryData.Versions = 2;
#if 0	// These fields are not available on W2K headers
        inquiryData.ANSIVersion; // : 3;
        inquiryData.ECMAVersion; // : 3;
        inquiryData.ISOVersion;  // : 2;
#endif
		inquiryData.ResponseDataFormat = 2;
		inquiryData.HiSupport;		// : 1;
		inquiryData.NormACA;		// : 1;

#if 0	// These fields are not available on W2K headers
		inquiryData.TerminateTask;	// : 1;
#endif
		inquiryData.AERC;			// : 1;

		inquiryData.AdditionalLength = 31;
		inquiryData.Reserved;

#if 0	// These fields are not available on W2K headers		
		inquiryData.Addr16;				// : 1;               // defined only for SIP devices.
		inquiryData.Addr32;				// : 1;               // defined only for SIP devices.
		inquiryData.AckReqQ;			// : 1;               // defined only for SIP devices.
		inquiryData.MediumChanger;		// : 1;
		inquiryData.MultiPort;			// : 1;
		inquiryData.ReservedBit2;		// : 1;
		inquiryData.EnclosureServices;	// : 1;
		inquiryData.ReservedBit3;		// : 1;
#endif

		inquiryData.SoftReset;			// : 1;
		inquiryData.CommandQueue;		// : 1;
#if 0	// These fields are not available on W2K headers				
		inquiryData.TransferDisable;	// : 1;      // defined only for SIP devices.
#endif		
		inquiryData.LinkedCommands;		// : 1;
		inquiryData.Synchronous;		// : 1;          // defined only for SIP devices.
		inquiryData.Wide16Bit;			// : 1;            // defined only for SIP devices.
		inquiryData.Wide32Bit;			// : 1;            // defined only for SIP devices.
		inquiryData.RelativeAddressing;	// : 1;
		
		RtlCopyMemory( inquiryData.VendorId,
					   NDAS_DISK_VENDOR_ID,
					   (strlen(NDAS_DISK_VENDOR_ID)+1) < 8 ? (strlen(NDAS_DISK_VENDOR_ID)+1) : 8 );

		RtlCopyMemory( inquiryData.ProductId,
					   model,
					   16 );

		RtlCopyMemory( inquiryData.ProductRevisionLevel,
					   PRODUCT_REVISION_LEVEL,
					   (strlen(PRODUCT_REVISION_LEVEL)+1) < 4 ?  
						(strlen(PRODUCT_REVISION_LEVEL)+1) : 4 );

		inquiryData.VendorSpecific;
		inquiryData.Reserved3;

		RtlMoveMemory( Ccb->DataBuffer,
					   &inquiryData,
					   Ccb->DataBufferLength > sizeof (INQUIRYDATA) ? sizeof (INQUIRYDATA) : Ccb->DataBufferLength );

		LurnAssocRefreshCcbStatusFlag( Lurn, &Ccb->CcbStatusFlags );
		LSCcbSetStatusFlag( Ccb, CCBSTATUS_FLAG_TIMER_COMPLETE );
		Ccb->CcbStatus = CCB_STATUS_SUCCESS;	//SRB_STATUS_SUCCESS;

		status = STATUS_SUCCESS;
		goto complete_here;
	}

	case SCSIOP_READ_CAPACITY: {

		PREAD_CAPACITY_DATA	readCapacityData;
		ULONG				blockSize;
		UINT64				sectorCount;
		UINT64				logicalBlockAddress;

		sectorCount = Lurn->UnitBlocks;

		readCapacityData = (PREAD_CAPACITY_DATA)Ccb->DataBuffer;

		logicalBlockAddress = sectorCount - 1;
		
		if (logicalBlockAddress < 0xffffffff) {
		
			REVERSE_BYTES(&readCapacityData->LogicalBlockAddress, &logicalBlockAddress);

		} else {

			readCapacityData->LogicalBlockAddress = 0xffffffff;
		}

		blockSize = Lurn->BlockBytes;
		REVERSE_BYTES(&readCapacityData->BytesPerBlock, &blockSize);

		LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
		LSAssocSetRedundentRaidStatusFlag(Lurn, Ccb);
		LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
		LSCcbCompleteCcb(Ccb);
		break;
	}

	case SCSIOP_START_STOP_UNIT: {

		PCDB		cdb = (PCDB)(Ccb->Cdb);

		if (cdb->START_STOP.Start == START_UNIT_CODE) {
		
			// Start. Nothing to do. Pass down Ccb.

			status = LurnAssocSendCcbToAllChildren( Lurn,
													Ccb,
													LurnRAID1RCcbCompletion,
													NULL,
													NULL,
													LURN_CASCADE_FORWARD );
			break;

		} else if (cdb->START_STOP.Start == STOP_UNIT_CODE) {
	
			// In rebuilding state, don't send stop to child.
			// To do: check another host is accessing the disk.
			// STOP is sent to spin-down HDD. It may be safe to ignore.
		
			if (client->DRaidStatus == DRIX_RAID_STATUS_REBUILDING) {
		
				DebugTrace( NDASSCSI_DEBUG_LURN_ERROR,
							("RAID is in rebuilding status: Don't stop unit\n") );

				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				DraidClientFlush(client, NULL, NULL);
				goto complete_here;
			}

			
			// Flush to reset dirty bitmaps.
			// LurnRAID1RFlushCompletionForStopUnit will send stop to child.
			
			status = DraidClientFlush( client, Ccb, LurnRAID1RFlushCompletionForStopUnit );

			if (status == STATUS_PENDING) {
		
				LSCcbMarkCcbAsPending(Ccb);

			} else {

				// Assume success
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				goto complete_here;
			}
				
		} else {

			DebugTrace( NDASSCSI_DEBUG_LURN_ERROR,
						("SCSIOP_START_STOP_UNIT: Invaild operation!!! %d %d.\n", Ccb->LurId[1], cdb->START_STOP.LogicalUnitNumber) );

			Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;	//SRB_STATUS_INVALID_REQUEST;
			goto complete_here;
		}

		break;
	}

	case SCSIOP_READ_CAPACITY16: {

		PREAD_CAPACITY_DATA_EX	readCapacityDataEx;
		ULONG					blockSize;
		UINT64					sectorCount;
		UINT64					logicalBlockAddress;

		sectorCount = Lurn->UnitBlocks;

		readCapacityDataEx = (PREAD_CAPACITY_DATA_EX)Ccb->DataBuffer;

		logicalBlockAddress = sectorCount - 1;
		REVERSE_BYTES_QUAD(&readCapacityDataEx->LogicalBlockAddress.QuadPart, &logicalBlockAddress);

		blockSize = Lurn->BlockBytes;
		REVERSE_BYTES(&readCapacityDataEx->BytesPerBlock, &blockSize);

		LurnAssocRefreshCcbStatusFlag(Lurn, &Ccb->CcbStatusFlags);
		LSAssocSetRedundentRaidStatusFlag(Lurn, Ccb);
		LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
		LSCcbCompleteCcb(Ccb);
		break;
	}
	
	case SCSIOP_MODE_SENSE:
	
		LurnAssocModeSense( Lurn, Ccb );
		break;

	case SCSIOP_MODE_SELECT:

		LurnAssocModeSelect( Lurn, Ccb );
		break;

	default: {

		//	send to all child LURNs.
		//	Set WriteVerify completion routine to CCB
		//		it guarantees CCB_STATUS_SUCCESS when least one LURN is working fine.
		//

		status = LurnAssocSendCcbToAllChildren( Lurn,
												Ccb,
												LurnRAID1RCcbCompletion,
												NULL,
												NULL,
												LURN_CASCADE_FORWARD );
		break;
	}
	}

	return STATUS_SUCCESS;

complete_here:

	LSAssocSetRedundentRaidStatusFlag( Lurn, Ccb );

	LSCcbCompleteCcb( Ccb );

	return STATUS_SUCCESS;
}

NTSTATUS
LurnRAID1RStopCcbCompletion (
	IN PCCB Ccb,
	IN PCCB OriginalCcb
	) 
{
	KIRQL				oldIrql;
	LONG				ass;
	PLURELATION_NODE	Lurn;

	
	Lurn = OriginalCcb->CcbCurrentStackLocation->Lurn;
	ASSERT(Lurn);

	ACQUIRE_SPIN_LOCK( &OriginalCcb->CcbSpinLock, &oldIrql );

	DebugTrace( DBG_LURN_INFO, ("Stop Ccb status %x\n", Ccb->CcbStatus) );

	// Assume success if any of the child succeed.
	
	switch(Ccb->CcbStatus) {
	
	case CCB_STATUS_SUCCESS:	// prority 0
	
		OriginalCcb->CcbStatus = CCB_STATUS_SUCCESS;
		break;

	default:

		if (OriginalCcb->CcbStatus != CCB_STATUS_SUCCESS) {

			OriginalCcb->CcbStatus = Ccb->CcbStatus;
		}

		break;
	}

	if (Ccb->CascadePrevCcb) {
		
		DebugTrace(DBG_LURN_INFO, ("Handling out-of-sync completed ccb(prev=%p)\n", Ccb->CascadePrevCcb));
		
		// If CascadePrevCcb is not NULL, this ccb is completed in out-of-order. Don't call next ccb
		// Rearrange ccb cascade without this link.
		
		Ccb->CascadePrevCcb->CascadeNextCcb = Ccb->CascadeNextCcb;
		
		if (Ccb->CascadeNextCcb) {
		
			Ccb->CascadeNextCcb->CascadePrevCcb = Ccb->CascadePrevCcb;
		}

	} else if (Ccb->CascadeNextCcb) {

		KDPrintM(DBG_LURN_INFO, ("Set event for next ccb(%p)\n", Ccb->CascadeNextCcb));
		Ccb->CascadeNextCcb->CascadePrevCcb = NULL;
		KeSetEvent(Ccb->CascadeNextCcb->CascadeEvent, IO_NO_INCREMENT, FALSE);
	}

	if (Ccb->CascadeEvent) {

		// set own cascade event (even if already set) as a completion mark

		KeSetEvent( Ccb->CascadeEvent, IO_NO_INCREMENT, FALSE );
	}
	
	LSCcbSetStatusFlag(	OriginalCcb, Ccb->CcbStatusFlags & CCBSTATUS_FLAG_ASSIGNMASK );
	
	RELEASE_SPIN_LOCK( &OriginalCcb->CcbSpinLock, oldIrql );

	//	Complete the original CCB
	
	DebugTrace( DBG_LURN_INFO,
				("OriginalCcb:%p. OrignalCcb->StatusFlags:%08lx\n", OriginalCcb, OriginalCcb->CcbStatusFlags) );

	ass = InterlockedDecrement( &OriginalCcb->AssociateCount );

	if (ass != 0) {
	
		DebugTrace( DBG_LURN_INFO,
					("Ass:%d Ccb:%p Ccb->CcbStatus:%x Ccb->StatusFlags:%08lx Ccb->AssociateID#%d\n",
					 ass, Ccb, Ccb->CcbStatus, Ccb->CcbStatusFlags, Ccb->AssociateID) );

		return STATUS_SUCCESS;
	}

	DebugTrace( DBG_LURN_INFO,
				("OriginalCcb:%p Completed. Ccb->AssociateID#%d\n",
				 OriginalCcb, Ccb->AssociateID) );

	if (OriginalCcb->CascadeEventArray) {

		ExFreePoolWithTag(OriginalCcb->CascadeEventArray, EVENT_ARRAY_TAG);
		OriginalCcb->CascadeEventArray = NULL;
	}
	
	LSAssocSetRedundentRaidStatusFlag( Lurn, OriginalCcb );
	LSCcbCompleteCcb( OriginalCcb );

	return STATUS_SUCCESS;
}

//
// Called for Misc Ccb opcode: CCB_OPCODE_RESETBUS, CCB_OPCODE_NOOP
//
NTSTATUS
LurnRAID1RMiscCcbCompletion (
	IN PCCB Ccb,
	IN PCCB OriginalCcb
	) 
{
	KIRQL	oldIrql;
	LONG	ass;
	PLURELATION_NODE	Lurn;
	Lurn = OriginalCcb->CcbCurrentStackLocation->Lurn;
	ASSERT(Lurn);
	ASSERT(Ccb->CascadeEvent == NULL);
	
	ACQUIRE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, &oldIrql);

	KDPrintM(DBG_LURN_INFO, ("Misc Ccb status %x\n", Ccb->CcbStatus));

	//
	// Assume success if any of the child succeed.
	//
	switch(Ccb->CcbStatus) {
	case CCB_STATUS_SUCCESS:	// prority 0
		OriginalCcb->CcbStatus = CCB_STATUS_SUCCESS;
		break;
	default:
		if(OriginalCcb->CcbStatus != CCB_STATUS_SUCCESS) {
			OriginalCcb->CcbStatus = Ccb->CcbStatus;
		}
		break;
	}

	LSCcbSetStatusFlag(	OriginalCcb,
		Ccb->CcbStatusFlags & CCBSTATUS_FLAG_ASSIGNMASK
		);
	RELEASE_SPIN_LOCK(&OriginalCcb->CcbSpinLock, oldIrql);

	//
	//	Complete the original CCB
	//
	KDPrintM(DBG_LURN_INFO,("OriginalCcb:%p. OrignalCcb->StatusFlags:%08lx\n",
		OriginalCcb, OriginalCcb->CcbStatusFlags));
	ass = InterlockedDecrement(&OriginalCcb->AssociateCount);
	ASSERT(ass >= 0);
	if(ass != 0) {
		KDPrintM(DBG_LURN_INFO,("Ass:%d Ccb:%p Ccb->CcbStatus:%x Ccb->StatusFlags:%08lx Ccb->AssociateID#%d\n",
			ass, Ccb, Ccb->CcbStatus, Ccb->CcbStatusFlags, Ccb->AssociateID));
		return STATUS_SUCCESS;
	}

	LSAssocSetRedundentRaidStatusFlag(Lurn, OriginalCcb);
	LSCcbCompleteCcb(OriginalCcb);

	return STATUS_SUCCESS;
}

NTSTATUS
LurnRAID1RFlushCompletion (
	IN PCCB		Ccb,	
	IN PVOID	Param
	) 
{
	UNREFERENCED_PARAMETER( Param );

	LSCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );

	LSAssocSetRedundentRaidStatusFlag( Ccb->CcbCurrentStackLocation->Lurn, Ccb );
	LSCcbCompleteCcb( Ccb );
	
	return STATUS_SUCCESS;
}

typedef struct _LURN_RAID_SHUT_DOWN_PARAM {

	PIO_WORKITEM 	IoWorkItem;
	PLURELATION_NODE	Lurn;
	PCCB				Ccb;

} LURN_RAID_SHUT_DOWN_PARAM, *PLURN_RAID_SHUT_DOWN_PARAM;

VOID
LurnRAID1ShutDownWorker (
	IN PDEVICE_OBJECT	DeviceObject,
	IN PVOID			Parameter
	) 
{
	PLURN_RAID_SHUT_DOWN_PARAM	params = (PLURN_RAID_SHUT_DOWN_PARAM) Parameter;
	PRAID_INFO					raidInfo;

	UNREFERENCED_PARAMETER( DeviceObject );

	
	// Is it possible that LURN is destroyed already?
	
	DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("Shutdowning DRAID\n") );
	
	raidInfo = params->Lurn->LurnRAIDInfo;

	DraidClientStop( params->Lurn );

	if (raidInfo->pDraidArbiter)
		DraidArbiterStop( params->Lurn );

	// Don't need to pass to child. lslurnide does nothing about CCB_OPCODE_SHUTDOWN 
	// Or we can set synchronous flag to ccb
	// status = LurnAssocSendCcbToAllChildren(Params->Lurn, Params->Ccb, LurnRAID1RCcbCompletion, NULL, NULL, LURN_CASCADE_FORWARD);

	LSCcbSetStatus( params->Ccb, CCB_STATUS_SUCCESS );
	LSCcbCompleteCcb( params->Ccb );
	
	IoFreeWorkItem( params->IoWorkItem );
	ExFreePoolWithTag( params, DRAID_SHUTDOWN_POOL_TAG );
}


NTSTATUS
RAID1RLurnRequest (
	IN	PLURELATION_NODE	Lurn,
	IN	PCCB				Ccb
	)
{
	NTSTATUS	status;
	PRAID_INFO	raidInfo;
	KIRQL		oldIrql;
	
	
	raidInfo = Lurn->LurnRAIDInfo;

	//
	//	dispatch a request
	//
	
	//if (raidInfo->pDraidClient == NULL || raidInfo->pDraidClient->ClientThreadHandle == NULL) {
	//if (Ccb->Cdb[0] == SCSIOP_TEST_UNIT_READY)

		DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, 
					("Ccb = %p, Ccb->OperationCode = %x, Ccb->Cdb[0] =%x\n", Ccb, Ccb->OperationCode, Ccb->Cdb[0]) );
	//}

	switch (Ccb->OperationCode) {

	case CCB_OPCODE_EXECUTE:

		DebugTrace( NDASSCSI_DEBUG_LURN_NOISE, ("CCB_OPCODE_EXECUTE!\n") );
		LurnRAID1RExecute( Lurn, Ccb );
		break;

	//	Send to all LURNs
		
	case CCB_OPCODE_ABORT_COMMAND:
		LSCcbSetStatus( Ccb, CCB_STATUS_COMMAND_FAILED );
		goto complete_here;
		break;

	case CCB_OPCODE_RESTART:
		LSCcbSetStatus( Ccb, CCB_STATUS_COMMAND_FAILED );
		goto complete_here;
		break;

	case CCB_OPCODE_RESETBUS:
	case CCB_OPCODE_NOOP: {

		//	Check to see if the CCB is coming for only this LURN.
	
		if (LSCcbIsFlagOn(Ccb, CCB_FLAG_DONOT_PASSDOWN)) {

			LSCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );
			goto complete_here;
		}
		
		status = LurnAssocSendCcbToAllChildren(Lurn, Ccb, LurnRAID1RMiscCcbCompletion, NULL, NULL, LURN_CASCADE_FORWARD);
		break;
	}
	
	case CCB_OPCODE_FLUSH:
	
		DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("CCB_OPCODE_FLUSH\n") );
		
		// This code may be running at DPC level.
		// Flush operation should not block
		
		status = DraidClientFlush( raidInfo->pDraidClient, Ccb, LurnRAID1RFlushCompletion );
		
		if (status == STATUS_PENDING) {

			LSCcbMarkCcbAsPending( Ccb );
		
		} else {
		
			// Assume success
			LSCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );
			goto complete_here;
		}

		break;

	case CCB_OPCODE_SHUTDOWN: {

		PLURN_RAID_SHUT_DOWN_PARAM Param;

		DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("CCB_OPCODE_SHUTDOWN\n") );

		// This code may be running at DPC level.
		// Run stop operation asynchrously.
		// Alloc work item and call LurnRAID1ShutDownWorker


		ACQUIRE_SPIN_LOCK( &Lurn->LurnSpinLock, &oldIrql );

		if (Lurn->LurnStatus == LURN_STATUS_STOP_PENDING || Lurn->LurnStatus == LURN_STATUS_STOP) {

			RELEASE_SPIN_LOCK( &Lurn->LurnSpinLock, oldIrql );		
		
			// Already stopping
			LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			goto complete_here;				

		} else {

			Lurn->LurnStatus = LURN_STATUS_STOP_PENDING;
		}

		RELEASE_SPIN_LOCK( &Lurn->LurnSpinLock, oldIrql );

		Param = ExAllocatePoolWithTag( NonPagedPool, sizeof(LURN_RAID_SHUT_DOWN_PARAM), DRAID_SHUTDOWN_POOL_TAG );
		
		if (Param == NULL) {

			DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("Failed to alloc shutdown worker\n") );
			LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			goto complete_here;				
		}

		if (Lurn && Lurn->Lur && Lurn->Lur->AdapterFunctionDeviceObject) {
		
			Param->IoWorkItem = IoAllocateWorkItem(Lurn->Lur->AdapterFunctionDeviceObject);

			if (!Param->IoWorkItem) {

				DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("Failed to alloc shutdown worker\n") );
				LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				ExFreePoolWithTag(Param, DRAID_SHUTDOWN_POOL_TAG);
				goto complete_here;					
			}

			Param->Lurn = Lurn;
			Param->Ccb = Ccb;

			DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("Queuing shutdown work to IoWorkItem\n") );
			IoQueueWorkItem( Param->IoWorkItem, LurnRAID1ShutDownWorker, DelayedWorkQueue, Param );
	
		} else {

			ASSERT( FALSE );
			LSCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );
			ExFreePoolWithTag( Param, DRAID_SHUTDOWN_POOL_TAG );
			goto complete_here;				
		}

		break;
	}

	case CCB_OPCODE_STOP: {

		DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("CCB_OPCODE_STOP\n") );

		ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);
		
		if (Lurn->LurnStatus == LURN_STATUS_STOP_PENDING || Lurn->LurnStatus == LURN_STATUS_STOP) {

			RELEASE_SPIN_LOCK( &Lurn->LurnSpinLock, oldIrql );		
			// Already stopping
			LSCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );
			goto complete_here;				
		
		} else {
		
			Lurn->LurnStatus = LURN_STATUS_STOP_PENDING;
		}

		RELEASE_SPIN_LOCK( &Lurn->LurnSpinLock, oldIrql );

		DraidClientStop( Lurn );

		if (raidInfo->pDraidArbiter) {

			DraidArbiterStop( Lurn );
		}

		//	Check to see if the CCB is coming for only this LURN.
		
		if (LSCcbIsFlagOn(Ccb, CCB_FLAG_DONOT_PASSDOWN)) {
		
			LSCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );

			goto complete_here;
		}

		status = LurnAssocSendCcbToAllChildren( Lurn, Ccb, LurnRAID1RStopCcbCompletion, NULL, NULL, LURN_CASCADE_BACKWARD);

		break;
	}

	case CCB_OPCODE_UPDATE: {

		DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("CCB_OPCODE_UPDATE requested to RAID1\n") );	
	
		//	Check to see if the CCB is coming for only this LURN.
		
		if (LSCcbIsFlagOn(Ccb, CCB_FLAG_DONOT_PASSDOWN)) {
		
			LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			goto complete_here;
		}

		status = LurnAssocSendCcbToAllChildren( Lurn, Ccb, LurnRAIDUpdateCcbCompletion, NULL, NULL, LURN_CASCADE_FORWARD_CHAINING);
		break;
	}

	case CCB_OPCODE_QUERY:

		status = LurnAssocQuery( Lurn, LurnAggrCcbCompletion, Ccb );
		break;

	default:

		DebugTrace( NDASSCSI_DEBUG_LURN_NOISE, ("INVALID COMMAND\n") );
		LSCcbSetStatus( Ccb, CCB_STATUS_INVALID_COMMAND );
		//		LSCcbCompleteCcb(Ccb);
		goto complete_here;
	}

	return STATUS_SUCCESS;

complete_here:

	LSAssocSetRedundentRaidStatusFlag( Lurn, Ccb );
	LSCcbCompleteCcb( Ccb );

	return STATUS_SUCCESS;
}

//
// Currently we do not support hot-swap and RAID with conflict configuration.
// Return error if any of the member does not have expected value.
// Make user to resolve the problem using bindtool.
//

NTSTATUS
LurnRMDRead (
	IN  PLURELATION_NODE		Lurn, 
	OUT PNDAS_RAID_META_DATA	Rmd,
	OUT PUINT32					UpTodateNode
	)
{
	NTSTATUS			status;
	ULONG				i, j;
	NDAS_RAID_META_DATA rmd_tmp;
	UINT32				usnMax;
	UINT32				freshestNode = 0;
	BOOLEAN				usedInDegraded[MAX_DRAID_MEMBER_DISK] = {0};
	
	// Update NodeFlags if it's RMD is missing or invalid.
	
	DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("IN\n") );

	usnMax = 0;

	for (i= 0; i < Lurn->LurnChildrenCnt; i++) { // i is node flag
	
		if (!LURN_IS_RUNNING(Lurn->LurnChildren[i]->LurnStatus)) {
	
			DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("Lurn is not running. Skip reading node %d.\n", i) );
			continue;
		}

		status = LurnExecuteSyncRead( Lurn->LurnChildren[i], (PUCHAR)&rmd_tmp, NDAS_BLOCK_LOCATION_RMD, 1 );

		if (!NT_SUCCESS(status)) {
	
			DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("Failed to read from node %d\n", i) );
			usnMax = 0;
			break;
		}

		if (NDAS_RAID_META_DATA_SIGNATURE != rmd_tmp.Signature || !IS_RMD_CRC_VALID(crc32_calc, rmd_tmp)) {

			DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("Node %d has invalid RMD. All disk must have RMD\n", i));
			usnMax = 0;
			break;
		
		} else if (RtlCompareMemory(&Lurn->LurnRAIDInfo->RaidSetId, &rmd_tmp.RaidSetId, sizeof(GUID)) != sizeof(GUID)) {
		
			DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("Node %d is not member of this RAID set\n", i) );
			usnMax = 0;
			break;

		} else if (RtlCompareMemory(&Lurn->LurnRAIDInfo->ConfigSetId, &rmd_tmp.ConfigSetId, sizeof(GUID)) != sizeof(GUID)) {
	
			DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("Node %d has different configuration set.\n", i) );

			// To do: mark this node as defective and continue.

			usnMax = 0;
			break;	

		} else {
			
			if (usnMax < rmd_tmp.uiUSN) {

				BOOLEAN SpareDisk = FALSE;
				BOOLEAN OosDisk = FALSE;
				
				for (j=0;j<Lurn->LurnChildrenCnt;j++) { // Role index
					
					if (rmd_tmp.UnitMetaData[j].iUnitDeviceIdx == i && 
						(rmd_tmp.UnitMetaData[j].UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_SPARE)) {
					
						SpareDisk = TRUE;
					}

					if (rmd_tmp.UnitMetaData[j].iUnitDeviceIdx == i && 
						(rmd_tmp.UnitMetaData[j].UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED)) {
						OosDisk = TRUE;
					}
				}

				if (SpareDisk) {

					// Ignore spare disk's RMD. Because this disk's OOS bitmap information may not be up-to-date

					DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("Spare disk has newer RMD USN %x but ignore it\n", usnMax) );
				
				} else if (OosDisk) {
					//
					// Ignore OOS disk's RMD. Because this disk's OOS bitmap information may not be up-to-date
					//
					DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("Disk has newer RMD USN %x but ignore it\n", usnMax) );
				
				} else {
				
					usnMax = rmd_tmp.uiUSN;
					DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("Found newer RMD USN %x from node %d\n", usnMax, i) );

					// newer one
					RtlCopyMemory( Rmd, &rmd_tmp, sizeof(NDAS_RAID_META_DATA) );
					freshestNode = i;
				}
			}

			if (rmd_tmp.state & NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED) {
			
				usedInDegraded[i] = TRUE;
			}
		}
	}

	if (0 == usnMax) {

		// This can happen if information that svc given is different from actual RMD.
		DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("Cannot find valid RMD or some LURN does not have valid RMD.\n") );
		RtlZeroMemory(Rmd, sizeof(NDAS_RAID_META_DATA));
		status = STATUS_UNSUCCESSFUL;
		ASSERT(FALSE); // You can ignore this. Simply RAID will be unmounted.
	
	} else {

		status = STATUS_SUCCESS;
	}

	if (Lurn->LurnType == LURN_RAID1R) {
	
		
		// Check UsedInDegraded flag is conflicted
		//	(We can assume RAID map is same if ConfigurationSetId matches)
		//		Check active member is all marked as used in degraded mode.
		
		if (usedInDegraded[Rmd->UnitMetaData[0].iUnitDeviceIdx] == TRUE &&
			usedInDegraded[Rmd->UnitMetaData[1].iUnitDeviceIdx] == TRUE) {
			
			// Both disk is used in degraded mode. User need to solve this problem.
			// fail ReadRmd
			
			DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, 
						("All active members had been independently mounted in degraded mode. Conflict RAID situation. Cannot continue\n") );
			
			RtlZeroMemory( Rmd, sizeof(NDAS_RAID_META_DATA) );
			status = STATUS_UNSUCCESSFUL;
			usnMax = 0;
		}
	}

	if (UpTodateNode) {

		*UpTodateNode = freshestNode;
	}

	DebugTrace( NDASSCSI_DEBUG_LURN_TRACE, ("OUT\n") );

	return status;
}

