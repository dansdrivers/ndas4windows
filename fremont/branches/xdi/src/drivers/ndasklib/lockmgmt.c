/********************************************************************
	created:	2007/05/14
	created:	14:5:2007   16:58
	filename: 	bufflock.c
	file path:	src\drivers\ndasklib
	file base:	bufflock
	file ext:	c
	author:		XIMETA, Inc.
	
	purpose:	Buffer lock management
*********************************************************************/

#include "ndasscsiproc.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "DevLock"


//
// Lock ID map
// Maps LUR device lock ID to ndas device lock index depending on the target ID.
//

// General purpose lock supported by NDAS chip 1.1, 2.0
UINT32	DevLockIdMap[MAX_TARGET_ID][NDAS_NR_GPLOCK] = {
	{-1, 2, 3, 0}, // Target 0 uses lock 0 for buffer lock
	{-1, 2, 3, 1}  // Target 0 uses lock 1 for buffer lock
};
// Advanced general purpose lock supported by NDAS chip 2.5
UINT32	DevLockIdMap_Adv[MAX_TARGET_ID][NDAS_NR_ADV_GPLOCK] = {
	{-1, 0, 1, 2, 3, 4, 5, 6, 7},
	{-1, 0, 1, 2, 3, 4, 5, 6, 7}
};


ULONG
LockIdToTargetLockIdx(ULONG HwVesion, ULONG TargetId, ULONG LockId) {
	ULONG	lockIdx;
	//
	// Map the lock ID.
	//

	NDASSCSI_ASSERT(TargetId < MAX_TARGET_ID);
	if(HwVesion == LANSCSIIDE_VERSION_1_1 ||
		HwVesion == LANSCSIIDE_VERSION_2_0
		) {
		lockIdx = DevLockIdMap[TargetId][LockId];
	} else {
		lockIdx = DevLockIdMap_Adv[TargetId][LockId];
	}

	return lockIdx;
}

//////////////////////////////////////////////////////////////////////////
//
// Time management
//
//////////////////////////////////////////////////////////////////////////

//
//	get the current system clock
//	100 Nano-second unit
//

static
__inline 
LARGE_INTEGER 
LMCurrentTime (VOID)
{
	LARGE_INTEGER	currentCount;
	ULONG			interval;
	LARGE_INTEGER	time;

	KeQueryTickCount(&currentCount);

	interval = KeQueryTimeIncrement();

	time.QuadPart = currentCount.QuadPart * interval;

	return time;
}


//////////////////////////////////////////////////////////////////////////
//
// Lock operations
//

//
// Acquire a general purpose lock.
// NOTE: With RetryWhenFailure is FALSE, be aware that the lock-cleanup
//		workaround will not be performed in this function.
//
// return values
//	STATUS_LOCK_NOT_GRANTED : Lock is owned by another session.
//							This is not communication error.
//

static
NTSTATUS
OpAcquireDevLock(
	IN PLANSCSI_SESSION	LSS,
	IN ULONG			LockIndex,
	OUT PBYTE			LockData,
	IN PLARGE_INTEGER	TimeOut,
	IN BOOLEAN			RetryWhenFailure
){
	NTSTATUS		status;
	ULONG			lockContention;
	LARGE_INTEGER	startTime;
	LARGE_INTEGER	maximumWaitTime;

	NDASSCSI_ASSERT( TimeOut == NULL || TimeOut->QuadPart <= 0 );

	lockContention = 0;
	startTime = LMCurrentTime();
	if(TimeOut && TimeOut->QuadPart) {
		maximumWaitTime.QuadPart = -TimeOut->QuadPart;
	} else {
		maximumWaitTime.QuadPart = 2 * NANO100_PER_SEC;
	}

	while(LMCurrentTime().QuadPart <= startTime.QuadPart + maximumWaitTime.QuadPart) {

		status = LspAcquireLock(
			LSS,
			LockIndex,
			LockData,
			TimeOut);

		if(status == STATUS_LOCK_NOT_GRANTED) {
			KDPrintM(DBG_OTHER_INFO, ("LockIndex%u: Lock contention #%u!!!\n", LockIndex, lockContention));
		} else if(NT_SUCCESS(status)) {
			break;
		} else {
			break;
		}
		lockContention ++;


		//
		//	Clean up the lock on the NDAS device.
		//

		if(lockContention != 0 && (lockContention % 100 == 0)) {

			LspWorkaroundCleanupLock(LSS,
				LockIndex, NULL);
		}

		if(RetryWhenFailure == FALSE) {
			break;
		}
	}
	if(status == STATUS_LOCK_NOT_GRANTED) {
		KDPrintM(DBG_OTHER_INFO, ("Lock denied. idx=%u lock contention=%u\n", LockIndex, lockContention));
	} else if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_OTHER_INFO, ("Failed to acquire lock idx=%u lock contention=%u\n", LockIndex, lockContention));
	}

	return status;
}

static
NTSTATUS
OpReleaseDevLock(
	IN PLANSCSI_SESSION	LSS,
	IN ULONG			LockIndex,
	IN PBYTE			LockData,
	IN PLARGE_INTEGER	TimeOut
){
	NTSTATUS	status;

	status = LspReleaseLock(
				LSS,
				LockIndex,
				LockData,
				TimeOut);

	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_OTHER_ERROR, ("Failed to release lock idx=%d\n", LockIndex));
		return status;
	}

	return STATUS_SUCCESS;
}

static
NTSTATUS
OpQueryDevLockOwner(
	IN PLANSCSI_SESSION	LSS,
	IN ULONG			LockIndex,
	IN PBYTE			LockOwner,
	IN PLARGE_INTEGER	TimeOut
){
	NTSTATUS	status;

	status = LspGetLockOwner(
				LSS,
				LockIndex,
				LockOwner,
				TimeOut);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_OTHER_ERROR, ("Failed to query lock owner idx=%d\n", LockIndex));
		return status;
	}

	return STATUS_SUCCESS;
}

static
NTSTATUS
OpGetDevLockData(
	IN PLANSCSI_SESSION	LSS,
	IN ULONG			LockIndex,
	IN PBYTE			LockData,
	IN PLARGE_INTEGER	TimeOut
){
	NTSTATUS	status;

	status = LspGetLockData(
				LSS,
				LockIndex,
				LockData,
				TimeOut);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_OTHER_ERROR, ("Failed to get lock data idx=%d\n", LockIndex));
		return status;
	}

	return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////
//
// Lock cache
//

static
INLINE
VOID
LockCacheSetDevLockAcquisition(
	IN PLU_HWDATA	LuHwData,
	IN ULONG		LockId,
	IN BOOLEAN		AddressRangeValid,
	IN UINT64		StartingAddress,
	IN UINT64		EndingAddress
){
	PNDAS_DEVLOCK_STATUS	lockStatus = &LuHwData->DevLockStatus[LockId];

	ASSERT(lockStatus->Acquired == FALSE);
	ASSERT(LuHwData->AcquiredLockCount < NDAS_NR_MAX_GPLOCK);
	ASSERT(StartingAddress <= EndingAddress);

	LuHwData->AcquiredLockCount ++;
	lockStatus->Acquired = TRUE;
	if(lockStatus->Lost) {
		KDPrintM(DBG_OTHER_ERROR, ("%u: Cleared lost state.\n", LockId));
		LuHwData->LostLockCount --;
		lockStatus->Lost = FALSE;
	}
	if(AddressRangeValid) {
		lockStatus->AddressRangeValid = 1;
		lockStatus->StartingAddress = StartingAddress;
		lockStatus->EndingAddress = EndingAddress;
	} else {
		lockStatus->AddressRangeValid = 0;
	}
}

static
INLINE
VOID
LockCacheSetDevLockRelease(
	IN PLU_HWDATA	LuHwData,
	IN ULONG		LockId
){
	PNDAS_DEVLOCK_STATUS	lockStatus = &LuHwData->DevLockStatus[LockId];

	ASSERT(lockStatus->Acquired == TRUE);
	ASSERT(LuHwData->AcquiredLockCount > 0);

	LuHwData->AcquiredLockCount --;
	lockStatus->Acquired = FALSE;
	lockStatus->AddressRangeValid = 0;
}

static
INLINE
VOID
LockCacheInvalidateAddressRange(
	IN PLU_HWDATA	LuHwData,
	IN ULONG		LockId
){
	PNDAS_DEVLOCK_STATUS	lockStatus = &LuHwData->DevLockStatus[LockId];

	lockStatus->AddressRangeValid = 0;
}

static
INLINE
BOOLEAN
LockCacheIsLost(
	IN PLU_HWDATA	LuHwData,
	IN ULONG		LockId
){
	PNDAS_DEVLOCK_STATUS	lockStatus = &LuHwData->DevLockStatus[LockId];

	return lockStatus->Lost;
}

static
INLINE
VOID
LockCacheSetDevLockLoss(
	IN PLU_HWDATA	LuHwData,
	IN ULONG		LockId
){
	PNDAS_DEVLOCK_STATUS	lockStatus = &LuHwData->DevLockStatus[LockId];

	ASSERT(lockStatus->Acquired == TRUE);
	ASSERT(lockStatus->Lost == FALSE);
	ASSERT(LuHwData->LostLockCount < NDAS_NR_MAX_GPLOCK);

	LockCacheSetDevLockRelease(LuHwData, LockId);
	KDPrintM(DBG_OTHER_ERROR, ("Lost lock #%d\n", LockId));

	LuHwData->LostLockCount ++;
	lockStatus->Lost = TRUE;
}

static
INLINE
VOID
LockCacheClearDevLockLoss(
	IN PLU_HWDATA	LuHwData,
	IN ULONG		LockId
){
	PNDAS_DEVLOCK_STATUS	lockStatus = &LuHwData->DevLockStatus[LockId];

	ASSERT(lockStatus->Lost == TRUE);
	ASSERT(LuHwData->LostLockCount > 0);

	KDPrintM(DBG_OTHER_ERROR, ("Confirmed lost lock #%d\n", LockId));

	LuHwData->LostLockCount --;
	lockStatus->Lost = FALSE;
}

//
// Mark all lock lost during reconnection.
//

VOID
LockCacheAllLocksLost(
	IN PLU_HWDATA	LuHwData
){
	ULONG		lockId;

	for(lockId = 0; lockId < NDAS_NR_MAX_GPLOCK; lockId ++) {
		if(LuHwData->DevLockStatus[lockId].Acquired) {

			LockCacheSetDevLockLoss(LuHwData, lockId);
#if DBG
			if(lockId == LURNDEVLOCK_ID_BUFFLOCK) {
				KDPrintM( DBG_OTHER_ERROR, ("Lost Buf lock #%d\n", lockId) );
			} else {
				KDPrintM(DBG_OTHER_ERROR, ("Lost lock #%d\n", lockId));
			}
#endif
		}
	}


#if DBG
	if(LuHwData->LostLockCount == 0) {
		KDPrintM(DBG_OTHER_ERROR, ("No Lost lock\n"));
	}
#endif
}

//
// Check to see if there are lock acquired except for the buffer lock.
//

BOOLEAN
LockCacheAcquiredLocksExistsExceptForBufferLock(
	IN PLU_HWDATA	LuHwData
){
	ULONG		lockId;

	for(lockId = 0; lockId < NDAS_NR_MAX_GPLOCK; lockId ++) {
		if(LuHwData->DevLockStatus[lockId].Acquired) {

			if(lockId != LURNDEVLOCK_ID_BUFFLOCK) {
				KDPrintM(DBG_OTHER_ERROR, ("Lost lock #%d\n", lockId));
				return TRUE;
			}
		}
	}

	return FALSE;
}

//
// Check if the IO range requires a lock that is lost.
//

BOOLEAN
LockCacheCheckLostLockIORange(
	IN PLU_HWDATA	LuHwData,
	IN UINT64		StartingAddress,
	IN UINT64		EndingAddress
){
	LONG	lostLockCnt;
	ULONG	lockId;
	PNDAS_DEVLOCK_STATUS	devLockStatus;

	ASSERT(StartingAddress <= EndingAddress);

	lostLockCnt = LuHwData->LostLockCount;
	lockId = 0;
	while(lostLockCnt ||
		lockId < NDAS_NR_MAX_GPLOCK) {

		devLockStatus = &LuHwData->DevLockStatus[lockId];

		if(devLockStatus->Lost) {

			//
			// Check the intersection.
			//
			if(devLockStatus->AddressRangeValid) {

				if(StartingAddress < devLockStatus->StartingAddress) {
					if(EndingAddress >= devLockStatus->StartingAddress) {
						KDPrintM(DBG_OTHER_ERROR, ("The address range is in lost lock's address range(1).\n"));
						return TRUE;
					}
				} else if(StartingAddress <= devLockStatus->EndingAddress) {
					KDPrintM(DBG_OTHER_ERROR, ("The address range is in lost lock's address range(2).\n"));
					return TRUE;
				}
			}

			lostLockCnt --;

		}
		lockId++;
	}

	ASSERT(lostLockCnt == 0);

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
//
// Device lock control request dispatcher
//

NTSTATUS
LurnIdeDiskDeviceLockControl(
	IN PLURELATION_NODE		Lurn,
	IN PLURNEXT_IDE_DEVICE	IdeDisk,
	IN PCCB					Ccb
){
	NTSTATUS				status;
	PLURN_DEVLOCK_CONTROL	devLockControl;
	ULONG					lockIdx;
	ULONG					lockDataLength;

	UNREFERENCED_PARAMETER(Lurn);

	if(Ccb->DataBufferLength < sizeof(LURN_DEVLOCK_CONTROL)) {
		Ccb->CcbStatus = CCB_STATUS_DATA_OVERRUN;
		return STATUS_SUCCESS;
	}
	status = STATUS_SUCCESS;

	//
	// Verify the lock operation
	//

	devLockControl = (PLURN_DEVLOCK_CONTROL)Ccb->DataBuffer;

	if(devLockControl->LockId == LURNDEVLOCK_ID_NONE) {

		NDASSCSI_ASSERT( FALSE );

		// Nothing to do.
		Ccb->CcbStatus = CCB_STATUS_SUCCESS; 
		return STATUS_SUCCESS;
	}

	switch(IdeDisk->LuHwData.HwVersion) {
	case LANSCSIIDE_VERSION_1_1:
	case LANSCSIIDE_VERSION_2_0:
#if DBG
	if (devLockControl->LockId == LURNDEVLOCK_ID_BUFFLOCK) {
	
		NDASSCSI_ASSERT( LsCcbIsFlagOn(Ccb, CCB_FLAG_CALLED_INTERNEL) );

	}
#endif

	if(devLockControl->LockId >= NDAS_NR_GPLOCK) {
		Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED; 
		return STATUS_SUCCESS;
	}
	if(devLockControl->AdvancedLock) {
		Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED; 
		return STATUS_SUCCESS;
	}
	lockDataLength = 4;

	break;


#if 0
	case LANSCSIIDE_VERSION_2_5:
		if(devLockControl->LockId >= NDAS_NR_ADV_GPLOCK) {
			Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED; 
			return STATUS_SUCCESS;
		}
		if(!devLockControl->AdvancedLock) {
			Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED; 
			return STATUS_SUCCESS;
		}
		lockDataLength = 64;
		break;
#endif

	case LANSCSIIDE_VERSION_1_0:
	default:

		NDASSCSI_ASSERT( FALSE );

		Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND; 
		return STATUS_SUCCESS;
	}

	// Check to see if the lock acquisition is required.
	if(	devLockControl->RequireLockAcquisition
		) {
		if(IdeDisk->LuHwData.DevLockStatus[devLockControl->LockId].Acquired == FALSE) {
			Ccb->CcbStatus = CCB_STATUS_LOST_LOCK;
			return STATUS_SUCCESS;
		}
	}

	//
	// Map the lock ID.
	//

	NDASSCSI_ASSERT(IdeDisk->LuHwData.LanscsiTargetID < MAX_TARGET_ID);
	if(IdeDisk->LuHwData.HwVersion == LANSCSIIDE_VERSION_1_1 ||
		IdeDisk->LuHwData.HwVersion == LANSCSIIDE_VERSION_2_0
		) {
		lockIdx = DevLockIdMap[IdeDisk->LuHwData.LanscsiTargetID][devLockControl->LockId];
	} else {
		lockIdx = DevLockIdMap_Adv[IdeDisk->LuHwData.LanscsiTargetID][devLockControl->LockId];
	}

	//
	// Execute the lock operation
	//

	switch(devLockControl->LockOpCode) {
	case LURNLOCK_OPCODE_ACQUIRE: {
		LARGE_INTEGER			timeOut;

		if(devLockControl->AddressRangeValid) {
			if(devLockControl->StartingAddress > devLockControl->EndingAddress) {
				Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
				return STATUS_SUCCESS;
			}
		}
		if(LockCacheIsLost(&IdeDisk->LuHwData, devLockControl->LockId)) {
			//
			// The lock lost. Clear lost status by releasing it.
			//
//			Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
			Ccb->CcbStatus = CCB_STATUS_LOST_LOCK;

			return STATUS_SUCCESS;
		}

		if (devLockControl->LockId == LURNDEVLOCK_ID_BUFFLOCK) {

			NDASSCSI_ASSERT( LockCacheIsAcquired(&IdeDisk->LuHwData,devLockControl->LockId) == FALSE ); 
		}

		//
		// Release the buffer lock to prevent deadlock or race condition
		// between the buffer and the others.
		//

		if(IdeDisk->BuffLockCtl.BufferLockConrol == TRUE) {
			if (devLockControl->LockId != LURNDEVLOCK_ID_BUFFLOCK) {
				status = NdasReleaseBufferLock(
					&IdeDisk->BuffLockCtl,
					IdeDisk->LanScsiSession,
					&IdeDisk->LuHwData,
					NULL,
					NULL,
					TRUE,
					0);
				if(!NT_SUCCESS(status)) {
					Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
					return status;
				}
			}
		}

		timeOut.QuadPart = devLockControl->ContentionTimeOut;
		status = OpAcquireDevLock(
						IdeDisk->LanScsiSession,
						lockIdx,
						devLockControl->LockData, 
						&timeOut,
						TRUE);
		if(NT_SUCCESS(status)) {
			Ccb->CcbStatus = CCB_STATUS_SUCCESS;

				ASSERT( LockCacheIsAcquired(&IdeDisk->LuHwData,devLockControl->LockId) == FALSE ); 
			//
			// The buffer lock acquired by outside of the IDE LURN.
			// Hand in the buffer lock control to the outside of the IDE LURN.
			// Turn off the collision control
			//
			if(devLockControl->LockId == LURNDEVLOCK_ID_BUFFLOCK && IdeDisk->BuffLockCtl.BufferLockConrol) {
				KDPrintM(DBG_OTHER_ERROR, ("Bufflock control off.\n"));
				IdeDisk->BuffLockCtl.BufferLockConrol = FALSE;
			}

			//
			// Acquire success
			// Update the lock status
			// Update the lock status only if previously we released the lock.
			//
			if(LockCacheIsAcquired(&IdeDisk->LuHwData, devLockControl->LockId) == FALSE) {

				LockCacheSetDevLockAcquisition(
					&IdeDisk->LuHwData,
					devLockControl->LockId,
					devLockControl->AddressRangeValid,
					devLockControl->StartingAddress,
					devLockControl->EndingAddress
					);
			}
		} else if(status == STATUS_LOCK_NOT_GRANTED) {
			Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
			status = STATUS_SUCCESS;
		} else {
			Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
		}
		return status;
	}
	case LURNLOCK_OPCODE_RELEASE: {

		if(LockCacheIsAcquired(&IdeDisk->LuHwData, devLockControl->LockId) == FALSE) {
			if(LockCacheIsLost(&IdeDisk->LuHwData, devLockControl->LockId)) {
				//
				// Clear lost status
				//
				LockCacheClearDevLockLoss(&IdeDisk->LuHwData, devLockControl->LockId);
				Ccb->CcbStatus = CCB_STATUS_SUCCESS;
				return STATUS_SUCCESS;
			}

			//
			// Already released.
			//

			if (devLockControl->LockId == LURNDEVLOCK_ID_BUFFLOCK) {

				NDASSCSI_ASSERT( FALSE ); 

			}

			Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
			return STATUS_SUCCESS;
		}

		ASSERT( LockCacheIsAcquired(&IdeDisk->LuHwData,devLockControl->LockId) == TRUE ); 

		//
		// Release success
		// Update the lock status only if previously we acquired the lock
		// whether the release request succeeds or not.
		//

		if(LockCacheIsAcquired(&IdeDisk->LuHwData, devLockControl->LockId)) {
			LockCacheSetDevLockRelease(&IdeDisk->LuHwData, devLockControl->LockId);
		}

		status = OpReleaseDevLock(
						IdeDisk->LanScsiSession,
						lockIdx,
						devLockControl->LockData,
						NULL);
		if(NT_SUCCESS(status)) {
			Ccb->CcbStatus = CCB_STATUS_SUCCESS;
		} else {
			Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
		}
		return status;
	}
	case LURNLOCK_OPCODE_QUERY_OWNER:
		status = OpQueryDevLockOwner(
						IdeDisk->LanScsiSession,
						lockIdx,
						devLockControl->LockData,
						NULL);
		if(NT_SUCCESS(status)) {
			Ccb->CcbStatus = CCB_STATUS_SUCCESS;
		} else {
			Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
		}
		return status;
	case LURNLOCK_OPCODE_GETDATA:		// Not yet implemented
		status = OpGetDevLockData(
						IdeDisk->LanScsiSession,
						lockIdx,
						devLockControl->LockData,
						NULL);

		if(NT_SUCCESS(status)) {
			Ccb->CcbStatus = CCB_STATUS_SUCCESS;
		} else {
			Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
		}
		return status;
	case LURNLOCK_OPCODE_SETDATA:		// Not yet implemented
	case LURNLOCK_OPCODE_BREAK:			// Not yet implemented
	default: ;
		Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
		return STATUS_SUCCESS;
	}

	return status;
}


//////////////////////////////////////////////////////////////////////////
//
// Buffer lock intention
//

//
// Increase the buffer lock acquisition request count.
// This is expensive operation due to four NDAS requests of lock and target data.
// Avoid to call this function during the buffer lock acquisition.
//

static
NTSTATUS
QueueBufferLockRequest(
	IN PBUFFLOCK_CONTROL	BuffLockCtl,
	IN PLANSCSI_SESSION		LSS,
	IN PLU_HWDATA			LuHwData,
	IN PLARGE_INTEGER		TimeOut
){
	NTSTATUS	status, release_status;
	TARGET_DATA	targetData;
	ULONG		lockIdx;
	ULONG		devRequestCount;

	//
	// Map the lock ID.
	//

	lockIdx = LockIdToTargetLockIdx(
		LuHwData->HwVersion,
		LuHwData->LanscsiTargetID,
		LURNDEVLOCK_ID_EXTLOCK);

	//
	// Acquire the extension lock.
	//
	if(LockCacheIsAcquired(LuHwData, LURNDEVLOCK_ID_EXTLOCK) == FALSE) {
		status = OpAcquireDevLock(
			LSS,
			lockIdx,
			NULL, 
			TimeOut,
			TRUE);
		if(!NT_SUCCESS(status)) {
			KDPrintM(DBG_OTHER_ERROR, ("OpAcquireDevLock() failed. STATUS=%08lx\n", status));
			return status;
		}

		LockCacheSetDevLockAcquisition(LuHwData, LURNDEVLOCK_ID_EXTLOCK, FALSE, 0, 0);
	}

	//
	// Read the target data
	//

	status = LspTextTartgetData(
		LSS,
		FALSE,
		LuHwData->LanscsiTargetID,
		&targetData,
		TimeOut);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_OTHER_ERROR, ("LspTextTartgetData() failed. STATUS=%08lx\n", status));
		goto exit;
	}

	//
	// Set intention bit.
	// Do not touch other bit fields.
	//

	devRequestCount = (ULONG)(targetData & TARGETDATA_REQUEST_COUNT_MASK);
	// Update the intention count before increase.

	BuffLockCtl->MyRequestCount ++;
	devRequestCount ++;
	devRequestCount &= TARGETDATA_REQUEST_COUNT_MASK;
	targetData = (targetData & (~TARGETDATA_REQUEST_COUNT_MASK)) | devRequestCount;

	//
	// Write the target data
	//

	status = LspTextTartgetData(
		LSS,
		TRUE,
		LuHwData->LanscsiTargetID,
		&targetData,
		TimeOut);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_OTHER_ERROR, ("LspTextTartgetData() failed. STATUS=%08lx\n", status));
		goto exit;
	}

	KDPrintM(DBG_OTHER_INFO, ("Request count=%u\n", devRequestCount));

exit:
	//
	// Release the extension lock.
	// Do not override the status value.
	//

	release_status = OpReleaseDevLock(
		LSS,
		lockIdx,
		NULL,
		TimeOut);

	LockCacheSetDevLockRelease(LuHwData, LURNDEVLOCK_ID_EXTLOCK);

	if(!NT_SUCCESS(release_status)) {
		KDPrintM(DBG_OTHER_ERROR, ("OpReleaseDevLock() failed. STATUS=%08lx\n", release_status));
	}


	return status;
}

//
// Retrieve the number of pending requests.
//
static
INLINE
VOID
UpdateRequestCount(
	IN PBUFFLOCK_CONTROL	BuffLockCtl,
	IN ULONG				RequestCount,
	OUT PULONG				PendingRequests
){
	ULONG pendingRequests;


#if DBG
	if(BuffLockCtl->RequestCountWhenReleased != RequestCount) {
		KDPrintM(DBG_OTHER_TRACE, ("Another host wants the buffer lock\n"));
	}
#endif
	if(RequestCount >= BuffLockCtl->RequestCountWhenReleased)
		pendingRequests = RequestCount - BuffLockCtl->RequestCountWhenReleased;
	else
		pendingRequests = RequestCount + 
		((TARGETDATA_REQUEST_COUNT_MASK+1) - BuffLockCtl->RequestCountWhenReleased);

	//
	// Set return value of the buffer lock request pending count.
	//
	if(PendingRequests)
		*PendingRequests = pendingRequests;
}

static
NTSTATUS
GetBufferLockPendingRequests(
	IN PBUFFLOCK_CONTROL	BuffLockCtl,
	IN PLANSCSI_SESSION		LSS,
	IN PLU_HWDATA			LuHwData,
	OUT PULONG				PendingRequests,
	IN PLARGE_INTEGER		TimeOut
){
	NTSTATUS	status;
	TARGET_DATA	targetData;
	ULONG		requestCount;

	status = LspTextTartgetData(
		LSS,
		FALSE,
		LuHwData->LanscsiTargetID,
		&targetData,
		TimeOut);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_OTHER_ERROR, ("LspTextTartgetData() failed.\n", status));
		return status;
	}

	KDPrintM(DBG_OTHER_TRACE, ("TargetData:%u\n", targetData));
	//
	// Match the signature.
	// If not match, it might be interference by anonymous application.
	//
	requestCount = (ULONG)(targetData & TARGETDATA_REQUEST_COUNT_MASK);
	UpdateRequestCount(BuffLockCtl, requestCount, PendingRequests);

	return status;
}

//
// Retrieve the numbers of pending requests, RW hosts, and RO hosts.
//

static
NTSTATUS
GetBufferLockPendingRequestsWithHostInfo(
	IN PBUFFLOCK_CONTROL	BuffLockCtl,
	IN PLANSCSI_SESSION		LSS,
	IN PLU_HWDATA			LuHwData,
	OUT PULONG				RequestCounts,
	OUT PULONG				PendingRequests,
	OUT PULONG				RWHostCount,
	OUT PULONG				ROHostCount,
	IN PLARGE_INTEGER		TimeOut
){
	NTSTATUS		status;
	TARGETINFO_LIST	targetList;
	ULONG			idx_targetentry;
	PTARGETINFO_ENTRY	targetEntry;
	BOOLEAN			found;
	ULONG		requestCount;

	status = LspTextTargetList(
		LSS,
		&targetList,
		sizeof(TARGETINFO_LIST),
		TimeOut);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_OTHER_ERROR, ("LspTextTartgetData() failed.\n", status));
		return status;
	}

	found = FALSE;
	for(idx_targetentry = 0;
		idx_targetentry < MAX_TARGET_ID && idx_targetentry < targetList.TargetInfoEntryCnt;
		idx_targetentry ++) {

		targetEntry = &targetList.TargetInfoEntry[idx_targetentry];

		if(targetEntry->TargetID == LuHwData->LanscsiTargetID) {

			requestCount = (ULONG)(targetEntry->TargetData & TARGETDATA_REQUEST_COUNT_MASK);
			if(RequestCounts)
				*RequestCounts = requestCount;
			KDPrintM(DBG_OTHER_TRACE, ("TargetData:%llu intentionCount:%u In-mem:%u\n",
				targetEntry->TargetData,
				requestCount,
				BuffLockCtl->RequestCountWhenReleased));
			UpdateRequestCount(BuffLockCtl, requestCount, PendingRequests);


			if(RWHostCount)
				*RWHostCount = targetEntry->NRRWHost;
			if(ROHostCount)
				*ROHostCount = targetEntry->NRROHost;

			//
			// Workaround for NDAS chip 2.0 original
			// It does not return correct ReadOnly host count
			//
			if(	LuHwData->HwVersion == LANSCSIIDE_VERSION_2_0 &&
				LuHwData->HwRevision == LANSCSIIDE_VER20_REV_1G_ORIGINAL) {

				if(RWHostCount)
					*RWHostCount = 1;
				if(ROHostCount)
					*ROHostCount = LANSCSIIDE_MAX_CONNECTION_VER11 - 1;
			}

			found = TRUE;
		}
	}

	if(found == FALSE) {
		return STATUS_NO_MORE_ENTRIES;
	}

	return status;
}

//////////////////////////////////////////////////////////////////////////
//
// Quantum management
//

VOID
DelayForQuantum(PBUFFLOCK_CONTROL BuffLockCtl, ULONG QuantumCount) {
	LARGE_INTEGER	interval;

	if(QuantumCount == 0)
		return;
	interval.QuadPart = - BuffLockCtl->Quantum.QuadPart * QuantumCount;
	KeDelayExecutionThread(KernelMode, FALSE, &interval);
}

#define LMPRIORITY_NODE_IN_A_LOT			8
#define LMPRIORITY_SQUEEZE(PRIORITY)		(((PRIORITY) + \
	LMPRIORITY_NODE_IN_A_LOT - 1)/LMPRIORITY_NODE_IN_A_LOT)

VOID
WaitForPriority(PBUFFLOCK_CONTROL BuffLockCtl, ULONG Priority) {
	LARGE_INTEGER	interval;

	if(Priority == 0)
		return;
	interval.QuadPart = - BuffLockCtl->PriorityWaitTime.QuadPart * 
		LMPRIORITY_SQUEEZE(Priority);
	KeDelayExecutionThread(KernelMode, FALSE, &interval);
}

//////////////////////////////////////////////////////////////////////////
//
// Buffer lock
//


//
// Wait for a host to release the buffer lock.
// If the priority is the highest, will use try to acquire the buffer lock, and
// return the lock data.
//

NTSTATUS
WaitForBufferLockRelease(
	IN PBUFFLOCK_CONTROL BuffLockCtl,
	IN PLANSCSI_SESSION	LSS,
	IN PLU_HWDATA		LuHwData,
	OUT PBYTE			LockData,
	OUT PBOOLEAN		LockAcquired
){
	NTSTATUS		status;
	LARGE_INTEGER	startTime;
	ULONG			idx_trial;
	ULONG			lockCount;
	LARGE_INTEGER	maximumWaitTime;
	ULONG			bufferLockIdx;


	if(LockAcquired == NULL)
		return STATUS_INVALID_PARAMETER;
	*LockAcquired = FALSE;

	bufferLockIdx = LockIdToTargetLockIdx(
		LuHwData->HwVersion,
		LuHwData->LanscsiTargetID,
		LURNDEVLOCK_ID_BUFFLOCK);

	maximumWaitTime.QuadPart = BuffLockCtl->AcquisitionMaxTime.QuadPart *3 / 2;
	ASSERT(LURNIDE_GENERIC_TIMEOUT >= maximumWaitTime.QuadPart);

	startTime = LMCurrentTime();

	for(idx_trial = 0;
		LMCurrentTime().QuadPart < startTime.QuadPart + maximumWaitTime.QuadPart;
		idx_trial++) {
		KDPrintM(DBG_OTHER_INFO, ("Trial #%u\n", idx_trial));


		if(BuffLockCtl->Priority != 0 && BuffLockCtl->IoIdle == FALSE) {

			LARGE_INTEGER timeOut;

			timeOut.QuadPart = -1 * BuffLockCtl->AcquisitionMaxTime.QuadPart;

			status = OpGetDevLockData(
				LSS,
				bufferLockIdx,
				(PBYTE)&lockCount,
				NULL);
			if(!NT_SUCCESS(status)) {
				KDPrintM(DBG_OTHER_ERROR, ("LurnIdeDiskGetDevLockData() failed. STATUS=%08lx\n", status));
				DelayForQuantum(BuffLockCtl, 1);
				continue;
			}

			if(lockCount != BuffLockCtl->CurrentLockCount) {
				// Set return value.
				if(LockData)
					*(PULONG)LockData = lockCount;
				BuffLockCtl->CurrentLockCount = lockCount;
				return STATUS_SUCCESS;
			} else {
				KDPrintM(DBG_OTHER_INFO, ("Lock count did not change. %u\n", lockCount));
				DelayForQuantum(BuffLockCtl, 1);
				continue;
			}
		} else {

			//
			// If priority is the highest, try to acquire the lock right away.
			//

			status = OpAcquireDevLock(
				LSS,
				bufferLockIdx,
				(PBYTE)&lockCount,
				NULL,
				FALSE);
			if(status == STATUS_LOCK_NOT_GRANTED) {
				//
				// Lock count is valid.
				//
				if(LockData)
					*(PULONG)LockData = lockCount;
				BuffLockCtl->CurrentLockCount = lockCount;
			}
			if(!NT_SUCCESS(status)) {
				KDPrintM(DBG_OTHER_INFO, ("OpAcquireDevLock() failed. STATUS=%08lx\n", status));
				DelayForQuantum(BuffLockCtl, 1);
				continue;
			}

			//
			// Lock acquired
			//

			*LockAcquired = TRUE;
			// Set return value.
			if(LockData)
				*(PULONG)LockData = lockCount;
			BuffLockCtl->CurrentLockCount = lockCount;

			return STATUS_SUCCESS;
		}
	}

	// Lock clean up for NDAS chip 2.0 original.
	LspWorkaroundCleanupLock(LSS,
		bufferLockIdx, NULL);

	KDPrintM(DBG_OTHER_ERROR, ("Timeout. Trial = %u LockCount=%u\n", idx_trial, lockCount));
	return STATUS_IO_TIMEOUT;
}

//
// Reset acquisition expire time and bytes.
//

static
INLINE
NdasResetAcqTimeAndAccIOBytes(
	IN PBUFFLOCK_CONTROL BuffLockCtl
){
	BuffLockCtl->AcquisitionExpireTime.QuadPart =
		LMCurrentTime().QuadPart + BuffLockCtl->AcquisitionMaxTime.QuadPart;
	BuffLockCtl->AccumulatedIOBytes = 0;
}

//
// Acquire the NDAS buffer lock.
//

NTSTATUS
NdasAcquireBufferLock(
	IN PBUFFLOCK_CONTROL BuffLockCtl,
	IN PLANSCSI_SESSION	LSS,
	IN PLU_HWDATA		LuHwData,
	OUT PBYTE			LockData,
	IN PLARGE_INTEGER	TimeOut
){
	NTSTATUS			status;
	LARGE_INTEGER		startTime;
	PLARGE_INTEGER		timeOut;
	ULONG				idx_trial;
	ULONG				lockCount;
	BOOLEAN				lockAcquired;

	if(LuHwData->LanscsiTargetID >= MAX_TARGET_ID)
		return STATUS_INVALID_PARAMETER;

	if(BuffLockCtl->BufferLockParent) {
		return STATUS_SUCCESS;
	}
	if(LockCacheIsAcquired(LuHwData, LURNDEVLOCK_ID_BUFFLOCK)) {
		KDPrintM(DBG_OTHER_INFO, ("The buffer lock already acquired.\n"));
		return STATUS_SUCCESS;
	}

	NDASSCSI_ASSERT( TimeOut == NULL || TimeOut->QuadPart < 0 );

	//
	// Buffer lock control is disabled.
	// Perform the lock request right away.
	//

	if(BuffLockCtl->BufferLockConrol == FALSE) {
		status = OpAcquireDevLock(
			LSS,
			DevLockIdMap[LuHwData->LanscsiTargetID][LURNDEVLOCK_ID_BUFFLOCK],
			(PBYTE)&lockCount,
			TimeOut,
			TRUE);
		if(NT_SUCCESS(status)) {
			LockCacheSetDevLockAcquisition(LuHwData, LURNDEVLOCK_ID_BUFFLOCK, FALSE, 0, 0);
		}

		return status;
	}

	startTime = LMCurrentTime();
	if(TimeOut == NULL) {
		timeOut = &LSS->DefaultTimeOut;
	} else {
		timeOut = TimeOut;
	}


	for(idx_trial = 0;
		LMCurrentTime().QuadPart <= startTime.QuadPart - timeOut->QuadPart;
		idx_trial++) {

		KDPrintM(DBG_OTHER_INFO, ("Trial #%u\n", idx_trial));

		//
		// Increase the buffer lock request count to promote the lock holder
		// to release it.
		//

		status = QueueBufferLockRequest(BuffLockCtl, LSS, LuHwData, NULL);
		if(status == STATUS_LOCK_NOT_GRANTED) {
			KDPrintM(DBG_OTHER_ERROR, ("Extension lock denied. retry. STATUS=%08lx\n", status));
			continue;
		}

		if(!NT_SUCCESS(status)) {

			NDASSCSI_ASSERT( status == STATUS_PORT_DISCONNECTED );
			KDPrintM(DBG_OTHER_ERROR, ("SetBufferLockIntention() failed. STATUS=%08lx\n", status));

			break;
		}

		//
		// Wait for the buffer lock to be released.
		//

		lockAcquired = FALSE;
		status = WaitForBufferLockRelease(BuffLockCtl, LSS, LuHwData, (PBYTE)&lockCount, &lockAcquired);
		if(!NT_SUCCESS(status)) {
			KDPrintM(DBG_OTHER_INFO, ("WaitForBufferLockRelease() failed. STATUS=%08lx\n", status));
			//
			// Somebody hold the lock too long or hang.
			// Go to the higher priority and retry to queue buffer lock request.
			// No need to delay. Delay has already occurred in WaitForBufferLockRelease().
			//
			if(status == STATUS_IO_TIMEOUT) {
				if(BuffLockCtl->Priority > 0) {
					BuffLockCtl->Priority --;
					KDPrintM(DBG_OTHER_ERROR, ("Somebody hold the lock too long or hang."
						" Go to the higher priority=%u. STATUS=%08lx\n", BuffLockCtl->Priority, status));
				} else {
					KDPrintM(DBG_OTHER_ERROR, ("Could not acquire the buffer lock at the highest priority."
						"priority=%u. STATUS=%08lx\n", BuffLockCtl->Priority, status));
				}
			}
			continue;
		}
#if DBG
		KDPrintM(DBG_OTHER_INFO, ("Priority=%u\n", BuffLockCtl->Priority));
#endif

		if(lockAcquired == FALSE) {
			//
			// Lock release detected.
			// Wait for the priority and send the lock request.
			//
			WaitForPriority(BuffLockCtl, BuffLockCtl->Priority);
			status = OpAcquireDevLock(
				LSS,
				DevLockIdMap[LuHwData->LanscsiTargetID][LURNDEVLOCK_ID_BUFFLOCK],
				(PBYTE)&lockCount,
				TimeOut,
				FALSE);
		}
		if(NT_SUCCESS(status)) {

			KDPrintM(DBG_OTHER_ERROR, ("Trial #%u: Acquired. LockCnt=%u Priority=%u\n", idx_trial, lockCount, BuffLockCtl->Priority));

			LockCacheSetDevLockAcquisition(LuHwData, LURNDEVLOCK_ID_BUFFLOCK, FALSE, 0, 0);
			// Set return lock data.
			if(LockData)
				*(PULONG)LockData = lockCount;

			//
			// Reset acquisition expire time and bytes.
			//

			NdasResetAcqTimeAndAccIOBytes(BuffLockCtl);
			
			//
			// Exit loop with success status
			//
			break;
		} else {
			KDPrintM(DBG_OTHER_INFO, ("LurnIdeDiskAcquireDevLock() failed. STATUS=%08lx\n", status));
			//
			// Assume that collision occurs.
			// Go to the higher priority
			//
			if(BuffLockCtl->Priority > 0) {
				BuffLockCtl->Priority --;
			}
		}

		status = STATUS_IO_TIMEOUT;
	}

	return status;
}


static
INLINE
ULONG
LMGetPriority(
	IN ULONG	ConnectedHosts,
	IN UINT32	LockCountWhenReleased,
	IN UINT32	CurrentLockCount,
	IN ULONG	LastOthersRequests
){
	UINT32 diff;
	UINT32 pendingRequestPerLockAcq;
	ULONG	priority;

	diff = CurrentLockCount - LockCountWhenReleased;
	if(diff < 1) {
		// Minimum diff is 1 to prevent divide-by-zero fault.
		diff = 1;
	}

	// Calculate the pending request per lock acquisition with round-up.
	pendingRequestPerLockAcq = (LastOthersRequests + diff/2) /  diff;

	// Translate the pending request per lock acquisition to the priority.
	priority = pendingRequestPerLockAcq;
#if 0
	if(pendingRequestPerLockAcq > 1) {
		priority = pendingRequestPerLockAcq - 1;
	} else {
		priority = 0;
	}
#endif

	// Check the priority is in valid range.
	if(priority >= ConnectedHosts) {
		KDPrintM(DBG_OTHER_ERROR, ("Too big priority %u %u %u\n",
			priority, LastOthersRequests, diff));
		priority = ConnectedHosts - 1;
	}
#if DBG
	KDPrintM(DBG_OTHER_INFO, ("Priority=%u. pending req=%u diff=%u\n",
		priority,
		LastOthersRequests,
		diff
		));
#endif

	return priority;
}

//
// Release the NDAS buffer lock
//

NTSTATUS
NdasReleaseBufferLock(
	IN PBUFFLOCK_CONTROL BuffLockCtl,
	IN PLANSCSI_SESSION	LSS,
	IN PLU_HWDATA		LuHwData,
	OUT PBYTE			LockData,
	IN PLARGE_INTEGER	TimeOut,
	IN BOOLEAN			Force,
	IN ULONG			TransferredIoLength
){
	NTSTATUS	status;
	BOOLEAN		release;
	LARGE_INTEGER	currentTime;

	if(LuHwData->LanscsiTargetID >= MAX_TARGET_ID)
		return STATUS_INVALID_PARAMETER;
	if(BuffLockCtl->BufferLockParent) {
		return STATUS_SUCCESS;
	}

	// Accumulate the IO length
	//

	BuffLockCtl->AccumulatedIOBytes += TransferredIoLength;

	//
	// Clear lost status
	//
	if(LockCacheIsLost(LuHwData, LURNDEVLOCK_ID_BUFFLOCK)) {
		LockCacheClearDevLockLoss(LuHwData, LURNDEVLOCK_ID_BUFFLOCK);
	}

	if(LockCacheIsAcquired(LuHwData, LURNDEVLOCK_ID_BUFFLOCK) == FALSE) {
		KDPrintM(DBG_OTHER_INFO, ("The buffer lock already released.\n"));
		return STATUS_SUCCESS;
	}

	//
	// Check release lock request condition.
	//
	currentTime = LMCurrentTime();
	if(BuffLockCtl->BufferLockConrol == FALSE) {
		// Release the buffer lock right away when BufferLock Control is off.
		release = TRUE;
	} else if(Force) {
		release = TRUE;
		KDPrintM(DBG_OTHER_INFO, ("Force option on.  Acc=%llu Elap=%lld Diff=%lld\n",
			BuffLockCtl->AccumulatedIOBytes,
			currentTime.QuadPart - (BuffLockCtl->AcquisitionExpireTime.QuadPart - BuffLockCtl->AcquisitionMaxTime.QuadPart),
			currentTime.QuadPart - BuffLockCtl->AcquisitionExpireTime.QuadPart
			));
	} else if(BuffLockCtl->AcquisitionExpireTime.QuadPart <= currentTime.QuadPart) {
		release = TRUE;
		KDPrintM(DBG_OTHER_INFO, ("Time over. Acc=%llu Elap=%lld Diff=%lld\n", 
			BuffLockCtl->AccumulatedIOBytes,
			currentTime.QuadPart - (BuffLockCtl->AcquisitionExpireTime.QuadPart - BuffLockCtl->AcquisitionMaxTime.QuadPart),
			currentTime.QuadPart - BuffLockCtl->AcquisitionExpireTime.QuadPart
			));
	} else if(BuffLockCtl->AccumulatedIOBytes >= BuffLockCtl->MaxIOBytes) {
		release = TRUE;
		KDPrintM(DBG_OTHER_INFO, ("Bytes over. Acc=%llu Elap=%lld Diff=%lld\n", 
			BuffLockCtl->AccumulatedIOBytes,
			currentTime.QuadPart - (BuffLockCtl->AcquisitionExpireTime.QuadPart - BuffLockCtl->AcquisitionMaxTime.QuadPart),
			currentTime.QuadPart - BuffLockCtl->AcquisitionExpireTime.QuadPart
			));
	} else {
		release = FALSE;
	}

	if(release) {
		ULONG	lockCount;

		if(BuffLockCtl->BufferLockConrol) {
			ULONG	pendingRequests, othersPendingRequest;
			ULONG	requestCount;
			ULONG	rwHosts, roHosts;

			status = GetBufferLockPendingRequestsWithHostInfo(
				BuffLockCtl,
				LSS,
				LuHwData,
				&requestCount,
				&pendingRequests,
				&rwHosts,
				&roHosts,
				TimeOut);
			if(!NT_SUCCESS(status)) {
				KDPrintM(DBG_OTHER_ERROR, ("GetBufferLockPendingRequestsWithHostInfo() failed."
					" STATUS=%08lx\n", status));
				return status;
			}
			KDPrintM(DBG_OTHER_INFO, ("pending lock request = %u\n", pendingRequests));

			if(pendingRequests > BuffLockCtl->MyRequestCount)
				othersPendingRequest = pendingRequests - BuffLockCtl->MyRequestCount;
			else
				othersPendingRequest = 0;

			// Update the host counters

			BuffLockCtl->ConnectedHosts = rwHosts + roHosts; // including myself

			//
			// Estimate the priority using the pending request count.
			// Reset the priority to the end of the request queue.
			//
			BuffLockCtl->Priority = LMGetPriority(
				BuffLockCtl->ConnectedHosts,
				BuffLockCtl->LockCountWhenReleased,
				BuffLockCtl->CurrentLockCount,
				othersPendingRequest);

			// Reset acquisition expire time and accumulated IO length
			// to start another acquisition period.
			// Reset request counts
			// Reset the LockCount
			BuffLockCtl->AcquisitionExpireTime.QuadPart = currentTime.QuadPart + BuffLockCtl->AcquisitionMaxTime.QuadPart;
			BuffLockCtl->AccumulatedIOBytes = 0;
			BuffLockCtl->RequestCountWhenReleased = requestCount;
			BuffLockCtl->MyRequestCount = 0;
			BuffLockCtl->LockCountWhenReleased = BuffLockCtl->CurrentLockCount;


			// If nobody wants the buffer lock, release the buffer lock later.
			if(othersPendingRequest == 0 && Force == FALSE) {

				KDPrintM(DBG_OTHER_INFO, ("Nobody wants the buffer lock. Release the buffer lock later.\n"));
				return STATUS_SUCCESS;
			}

		}

		// Send buffer lock release request.
		status = OpReleaseDevLock(
			LSS,
			DevLockIdMap[LuHwData->LanscsiTargetID][LURNDEVLOCK_ID_BUFFLOCK],
			(PBYTE)&lockCount,
			TimeOut);
		if(NT_SUCCESS(status)) {
			if(LockData)
				*(PULONG)LockData = lockCount;

			KDPrintM(DBG_OTHER_INFO, ("Buffer lock released. %u\n", lockCount));

			//
			// Release success
			// Update the lock status only if previously we acquired the lock.
			//

			LockCacheSetDevLockRelease(LuHwData, LURNDEVLOCK_ID_BUFFLOCK);

		}
	}

	return status;
}

//////////////////////////////////////////////////////////////////////////
//
// Idle timer management
//

NTSTATUS
StartIoIdleTimer(
	PBUFFLOCK_CONTROL BuffLockCtl
){
	LARGE_INTEGER	dueTime;
	BOOLEAN			alreadySet;

	if(BuffLockCtl->BufferLockConrol == FALSE) {
		return STATUS_SUCCESS;
	}

	dueTime.QuadPart = - BuffLockCtl->IoIdleTimeOut.QuadPart;
	alreadySet = KeSetTimer(&BuffLockCtl->IoIdleTimer, dueTime, NULL);
	ASSERT(alreadySet == FALSE);

	return STATUS_SUCCESS;
}

NTSTATUS
StopIoIdleTimer(
	PBUFFLOCK_CONTROL BuffLockCtl
){
	BOOLEAN	cancelledInSystemTimerQueue;

	if(BuffLockCtl->BufferLockConrol == FALSE) {
		return STATUS_SUCCESS;
	}

	// Clear IO idle state.
	BuffLockCtl->IoIdle = FALSE;

	cancelledInSystemTimerQueue = KeCancelTimer(&BuffLockCtl->IoIdleTimer);
	if(cancelledInSystemTimerQueue == FALSE) {
		KDPrintM(DBG_OTHER_INFO, ("Buffer lock IO idle timer is not canceled in the system timer queue.\n"));
	}

	return STATUS_SUCCESS;
}

NTSTATUS
EnterBufferLockIoIdle(
	IN PBUFFLOCK_CONTROL BuffLockCtl,
	IN PLANSCSI_SESSION	LSS,
	IN PLU_HWDATA		LuHwData
){
	NTSTATUS	status;

	KDPrintM(DBG_OTHER_INFO, (
		"Enter buffer lock IO idle.\n"));

	if(BuffLockCtl->BufferLockConrol == FALSE) {
		return STATUS_SUCCESS;
	}

	// Release the buffer lock
	status = NdasReleaseBufferLock(BuffLockCtl, LSS, LuHwData
		, NULL, NULL, TRUE, 0);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_OTHER_ERROR, (
			" ReleaseNdasBufferLock() failed. STATUS=%08lx.\n",
			status));
	}

	// Indicate IO idle state.
	BuffLockCtl->IoIdle = TRUE;

	return status;
}

//////////////////////////////////////////////////////////////////////////
//
// Initialization
//

#define LOCKMGMT_QUANTUM					((UINT64)156250)		// 15.625 ms ( 1/64 second )
#define LOCKMGMT_MAX_ACQUISITION_TIME		(8 * LOCKMGMT_QUANTUM)	// 125 ms ( 1/8 second )
#define LOCKMGMT_MAX_ACQUISITION_BYTES		(8 * 1024 * 1024)		// 8 MBytes
#define LOCKMGMT_IOIDLE_TIMEOUT				(LOCKMGMT_MAX_ACQUISITION_TIME / 4)	// 31.25 ms
#define LOCKMGMT_PRIORITYWAITTIME			(LOCKMGMT_QUANTUM * 2)

NTSTATUS
LMInitialize(
	IN PLURELATION_NODE		Lurn,
	IN PBUFFLOCK_CONTROL	BuffLockCtl,
	IN BOOLEAN				InitialState
){

	RtlZeroMemory(BuffLockCtl, sizeof(PBUFFLOCK_CONTROL));

	//
	// Timer resolution data:
	//
	// Windows Vista Business x86 with Xeon SMP: 1/64 second per tick. ( 15.6250 msec )
	// Windows Vista Enterprise x86_64 with AMD Sempron 2600+: 1/64
	// Windows XP Professional x86 with VMWare 5.5: 1/64
	// Windows XP Professional x86 with Xeon SMP: 1/64
	// Windows XP Professional x86 with Dell Latitude D520 note book: 15.6001 msec per tick.
	//
	BuffLockCtl->TimerResolution = KeQueryTimeIncrement();
	KDPrintM(DBG_OTHER_ERROR, ("KeQueryTickCount()'s 100nano per tick = %u, LOCKMGMT_QUANTUM = %x\n", 
								BuffLockCtl->TimerResolution, LOCKMGMT_QUANTUM));

	BuffLockCtl->Quantum.QuadPart = LOCKMGMT_QUANTUM;
	BuffLockCtl->PriorityWaitTime.QuadPart = LOCKMGMT_PRIORITYWAITTIME;
	BuffLockCtl->AcquisitionMaxTime.QuadPart = LOCKMGMT_MAX_ACQUISITION_TIME;
	BuffLockCtl->AcquisitionExpireTime.QuadPart = 0;
	BuffLockCtl->MaxIOBytes = LOCKMGMT_MAX_ACQUISITION_BYTES;
	BuffLockCtl->IoIdleTimeOut.QuadPart = LOCKMGMT_IOIDLE_TIMEOUT;
	BuffLockCtl->IoIdle = TRUE;
	BuffLockCtl->ConnectedHosts = 1; // add myself.
	BuffLockCtl->CurrentLockCount = 0;
	BuffLockCtl->RequestCountWhenReleased = 0;
	BuffLockCtl->Priority = 0;
	KeInitializeTimer(&BuffLockCtl->IoIdleTimer);
	BuffLockCtl->AccumulatedIOBytes = 0;
	BuffLockCtl->BufferLockConrol = InitialState;

#if __NDAS_SCSI_DISABLE_LOCK_RELEASE_DELAY__
	if (LURN_IS_ROOT_NODE(Lurn)) {

		BuffLockCtl->BufferLockParent = FALSE;
	
	} else {

		BuffLockCtl->BufferLockParent = TRUE;
		// Must be off when the parent controls the buffer lock.
		BuffLockCtl->BufferLockConrol = FALSE;
	}
#else
	BuffLockCtl->BufferLockParent = FALSE;
#endif

#if DBG
	if(BuffLockCtl->BufferLockParent)
		ASSERT(BuffLockCtl->BufferLockConrol == FALSE);
	KDPrintM(DBG_LURN_ERROR, ("Buffer lock control:%x Controlled by parent:%x\n",
		BuffLockCtl->BufferLockConrol,
		BuffLockCtl->BufferLockParent));
#endif

	return STATUS_SUCCESS;
}


NTSTATUS
LMDestroy(
	IN PBUFFLOCK_CONTROL BuffLockCtl
){
	BOOLEAN	cancelledInSystemTimerQueue;

	cancelledInSystemTimerQueue = KeCancelTimer(&BuffLockCtl->IoIdleTimer);
	if(cancelledInSystemTimerQueue == FALSE) {
		KDPrintM(DBG_OTHER_ERROR, ("Buffer lock IO idle timer is not cancelled in the system timer queue.\n"));
	}
	return STATUS_SUCCESS;
}
