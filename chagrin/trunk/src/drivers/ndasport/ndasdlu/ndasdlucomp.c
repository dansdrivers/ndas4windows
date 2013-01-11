#include "ndasport.h"
#include "trace.h"
#include "ndasdlu.h"
#include <ntdddisk.h>
#include "constants.h"

#include <initguid.h>
#include "ndasdluguid.h"
#include "lslur.h"

#ifdef RUN_WPP
#include "ndasdlu.tmh"
#endif

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "DLUCOMP"




//
// Translate CCB status to SRB status
//

VOID
CcbStatusToSrbStatus(PCCB Ccb, PSCSI_REQUEST_BLOCK Srb) {

	Srb->DataTransferLength -= Ccb->ResidualDataLength;

	switch(Ccb->CcbStatus) {
		case CCB_STATUS_SUCCESS:

			Srb->SrbStatus = SRB_STATUS_SUCCESS;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			break;

		case CCB_STATUS_NOT_EXIST:

			Srb->SrbStatus = SRB_STATUS_NO_DEVICE;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			break;

		case CCB_STATUS_INVALID_COMMAND:

			Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			break;

		case CCB_STATUS_LOST_LOCK:
		case CCB_STATUS_COMMAND_FAILED:

			Srb->SrbStatus = SRB_STATUS_ERROR | SRB_STATUS_AUTOSENSE_VALID;
			Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
			break;

		case CCB_STATUS_COMMMAND_DONE_SENSE2:
			Srb->SrbStatus =  SRB_STATUS_BUSY  | SRB_STATUS_AUTOSENSE_VALID ;
			Srb->DataTransferLength = 0;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			break;

		case CCB_STATUS_COMMMAND_DONE_SENSE:
			Srb->SrbStatus =  SRB_STATUS_ERROR | SRB_STATUS_AUTOSENSE_VALID ;
			Srb->DataTransferLength = 0;
			Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
			break;

		case CCB_STATUS_RESET:

			Srb->SrbStatus = SRB_STATUS_BUS_RESET;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			break;

		case CCB_STATUS_DATA_OVERRUN:
			Srb->SrbStatus = SRB_STATUS_DATA_OVERRUN;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			break;

		case CCB_STATUS_COMMUNICATION_ERROR:
			{
				PSENSE_DATA	senseData;

				Srb->SrbStatus = SRB_STATUS_ERROR | SRB_STATUS_AUTOSENSE_VALID;
				Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
				Srb->DataTransferLength = 0;

				senseData = Srb->SenseInfoBuffer;

				senseData->ErrorCode = 0x70;
				senseData->Valid = 1;
				//senseData->SegmentNumber = 0;
				senseData->SenseKey = SCSI_SENSE_HARDWARE_ERROR;	//SCSI_SENSE_MISCOMPARE;
				//senseData->IncorrectLength = 0;
				//senseData->EndOfMedia = 0;
				//senseData->FileMark = 0;

				senseData->AdditionalSenseLength = 0xb;
				senseData->AdditionalSenseCode = SCSI_ADSENSE_NO_SENSE;
				senseData->AdditionalSenseCodeQualifier = 0;
			}
			break;

		case CCB_STATUS_BUSY:
			Srb->SrbStatus = SRB_STATUS_BUSY;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			KDPrint(1,("CCB_STATUS_BUSY\n"));
			break;

			//
			//	Stop one LUR
			//
		case CCB_STATUS_STOP: {
			Srb->SrbStatus = SRB_STATUS_BUS_RESET;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			Srb->DataTransferLength = 0;
			KDPrint(1,("CCB_STATUS_STOP. Stopping!\n"));
			break;
		}
		default:
			// Error in Connection...
			// CCB_STATUS_UNKNOWN_STATUS, CCB_STATUS_RESET, and so on.
			Srb->SrbStatus = SRB_STATUS_ERROR;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			Srb->DataTransferLength = 0;
	}
}


//
//	MACRO for the convenience
//

#define NDAS_DLU_LOG_ERROR_MODULE_COMPLETION(ERRORCODE, EVT_ID)	\
	NdasDluLogError(												\
							LogicalUnitExtension,					\
							srb,									\
							srb->PathId,							\
							srb->TargetId,							\
							srb->Lun,								\
				(ERRORCODE),										\
				EVTLOG_UNIQUEID(EVTLOG_MODULE_COMPLETION,			\
								EVT_ID,								\
								0))

//
//	NDASSCSI adapter device object's completion routine
//

NTSTATUS
NdasDluCcbCompletion(
		  IN PCCB							Ccb,
		  IN PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension
	  )
{
	KIRQL						oldIrql;
	static	LONG				SrbSeq;
	LONG						srbSeqIncremented;
	PSCSI_REQUEST_BLOCK			srb;
	PCCB						abortCcb;
	NTSTATUS					return_status;
	UINT32						AdapterStatus, AdapterStatusBefore;
	UINT32						NeedToUpdatePdoInfoInLSBus;
	BOOLEAN						busResetOccured;
	PNDAS_DLU_EXTENSION			ndasDluExtension = NdasDluGetExtension(LogicalUnitExtension);

	KDPrint(3,("RequestExecuting = %d\n", ndasDluExtension->RequestExecuting));

	srb = Ccb->Srb;
	if(!srb) {
		KDPrint(1,("Ccb:%p CcbStatus %d. No srb assigned.\n", Ccb, Ccb->CcbStatus));
		ASSERT(srb);
		return STATUS_SUCCESS;
	}
 
	//
	//	NDASSCSI completion routine will do post operation to complete CCBs.
	//
	return_status = STATUS_SUCCESS;

	//
	// Set SRB completion sequence for debugging
	//

	srbSeqIncremented = InterlockedIncrement(&SrbSeq);

#if 0
	if(KeGetCurrentIrql() == PASSIVE_LEVEL) {
		if((srbSeqIncremented%100) == 0) {
			LARGE_INTEGER	interval;

			KDPrint(1,("Interval for debugging.\n"));

			interval.QuadPart = - 11 * 10000000; // 10 seconds
			KeDelayExecutionThread(KernelMode, FALSE, &interval);
		}
	}
#endif

	//
	// Update Adapter status flag
	//

	NeedToUpdatePdoInfoInLSBus = FALSE;

	ACQUIRE_SPIN_LOCK(&ndasDluExtension->DluSpinLock, &oldIrql);

	// Save the bus-reset flag
	if(DLUINTERNAL_ISSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_BUSRESET_PENDING)) {
		busResetOccured = TRUE;
	} else {
		busResetOccured = FALSE;
	}

	// Save the current flag
	AdapterStatusBefore = ndasDluExtension->DluInternalStatus;


	//	Check reconnecting process.
	if(LSCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_RECONNECTING))
	{
		DLUINTERNAL_SETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_RECONNECT_PENDING);
	} else {
		DLUINTERNAL_RESETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_RECONNECT_PENDING);
	}

	//
	// Update adapter status only when CCBSTATUS_FLAG_RAID_FLAG_VALID is on.
	// In other case, Ccb has no chance to get flag information from RAID.
	//
	if (LSCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_RAID_FLAG_VALID)) {
		//	Check to see if the associate member is in error.
		if(LSCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_RAID_DEGRADED))
		{
			DLUINTERNAL_SETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_MEMBER_FAULT);
		} else {
			DLUINTERNAL_RESETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_MEMBER_FAULT);
		}

		//	Check recovering process.
		if(LSCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_RAID_RECOVERING))	{
			DLUINTERNAL_SETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_RECOVERING);
		} else {
			DLUINTERNAL_RESETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_RECOVERING);
		}

		// Check RAID failure
		if(LSCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_RAID_FAILURE))	{
			DLUINTERNAL_SETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_RAID_FAILURE);
		} else {
			DLUINTERNAL_RESETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_RAID_FAILURE);
		}

		// Set RAID normal status
		if(LSCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_RAID_NORMAL))	{
			DLUINTERNAL_SETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_RAID_NORMAL);
		} else {
			DLUINTERNAL_RESETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_RAID_NORMAL);
		}
	}	
	// power-recycle occurred.
	if(LSCcbIsStatusFlagOn(Ccb, CCBSTATUS_FLAG_POWERRECYLE_OCCUR)) {
		DLUINTERNAL_SETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_POWERRECYCLED);
	} else {
		DLUINTERNAL_RESETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_POWERRECYCLED);
	}


	AdapterStatus = ndasDluExtension->DluInternalStatus;
	RELEASE_SPIN_LOCK(&ndasDluExtension->DluSpinLock, oldIrql);

	if(AdapterStatus != AdapterStatusBefore)
	{
		if(
			!(AdapterStatusBefore & DLUINTERNAL_STATUSFLAG_MEMBER_FAULT) &&
			(AdapterStatus & DLUINTERNAL_STATUSFLAG_MEMBER_FAULT)
			)
		{
			// DLUINTERNAL_STATUSFLAG_MEMBER_FAULT on
			NDAS_DLU_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_MEMBER_FAULT, EVTLOG_MEMBER_IN_ERROR);
			KDPrint(1,("Ccb:%p CcbStatus %d. Set member fault.\n", Ccb, Ccb->CcbStatus));
		}
		if(
			(AdapterStatusBefore & DLUINTERNAL_STATUSFLAG_MEMBER_FAULT) &&
			!(AdapterStatus & DLUINTERNAL_STATUSFLAG_MEMBER_FAULT)
			)
		{
			// DLUINTERNAL_STATUSFLAG_MEMBER_FAULT off
			NDAS_DLU_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_MEMBER_FAULT_RECOVERED, EVTLOG_MEMBER_RECOVERED);
			KDPrint(1,("Ccb:%p CcbStatus %d. Reset member fault.\n", Ccb, Ccb->CcbStatus));
		}
		if(
			!(AdapterStatusBefore & DLUINTERNAL_STATUSFLAG_RECONNECT_PENDING) &&
			(AdapterStatus & DLUINTERNAL_STATUSFLAG_RECONNECT_PENDING)
			)
		{
			// DLUINTERNAL_STATUSFLAG_RECONNECT_PENDING on
			NDAS_DLU_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_RECONNECT_START, EVTLOG_START_RECONNECTION);
			KDPrint(1,("Ccb:%p CcbStatus %d. Start reconnecting\n", Ccb, Ccb->CcbStatus));
		}
		if(
			(AdapterStatusBefore & DLUINTERNAL_STATUSFLAG_RECONNECT_PENDING) &&
			!(AdapterStatus & DLUINTERNAL_STATUSFLAG_RECONNECT_PENDING)
			)
		{
			// DLUINTERNAL_STATUSFLAG_RECONNECT_PENDING off
			NDAS_DLU_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_RECONNECTED, EVTLOG_END_RECONNECTION);
			KDPrint(1,("Ccb:%p CcbStatus %d. Finish reconnecting\n", Ccb, Ccb->CcbStatus));
		}
		if(
			!(AdapterStatusBefore & DLUINTERNAL_STATUSFLAG_RECOVERING) &&
			(AdapterStatus & DLUINTERNAL_STATUSFLAG_RECOVERING)
			)
		{
			// DLUINTERNAL_STATUSFLAG_RECOVERING on
			NDAS_DLU_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_RECOVERY_START, EVTLOG_START_RECOVERING);
			KDPrint(1,("Ccb:%p CcbStatus %d. Started recovering\n", Ccb, Ccb->CcbStatus));
		}
		if(
			(AdapterStatusBefore & DLUINTERNAL_STATUSFLAG_RECOVERING) &&
			!(AdapterStatus & DLUINTERNAL_STATUSFLAG_RECOVERING) &&
			!(AdapterStatus & (DLUINTERNAL_STATUSFLAG_RAID_FAILURE|DLUINTERNAL_STATUSFLAG_MEMBER_FAULT)) &&
			(AdapterStatus & DLUINTERNAL_STATUSFLAG_RAID_NORMAL))
		{
			// DLUINTERNAL_STATUSFLAG_RECOVERING off
			NDAS_DLU_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_RECOVERED, EVTLOG_END_RECOVERING);
			KDPrint(1,("Ccb:%p CcbStatus %d. Ended recovering\n", Ccb, Ccb->CcbStatus));
		}
		if (
			(AdapterStatus & DLUINTERNAL_STATUSFLAG_RAID_FAILURE)	 &&
			!(AdapterStatusBefore & DLUINTERNAL_STATUSFLAG_RAID_FAILURE))
		{
			NDAS_DLU_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_RAID_FAILURE, EVTLOG_RAID_FAILURE);
			KDPrint(1,("Ccb:%p CcbStatus %d. RAID failure\n", Ccb, Ccb->CcbStatus));
		}
		
		if(
			!(AdapterStatusBefore & DLUINTERNAL_STATUSFLAG_POWERRECYCLED) &&
			(AdapterStatus & DLUINTERNAL_STATUSFLAG_POWERRECYCLED)
			)
		{
			// DLUINTERNAL_STATUSFLAG_POWERRECYCLED on
			NDAS_DLU_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_DISK_POWERRECYCLE, EVTLOG_DISK_POWERRECYCLED);
			KDPrint(1,("Ccb:%p CcbStatus %d. Started recovering\n", Ccb, Ccb->CcbStatus));
		}


		NeedToUpdatePdoInfoInLSBus = TRUE;
	}

	//
	//	If CCB_OPCODE_UPDATE is successful, update adapter status in LanscsiBus
	//
	if(Ccb->OperationCode == CCB_OPCODE_UPDATE) {
		if(Ccb->CcbStatus == CCB_STATUS_SUCCESS) {
			NDAS_DLU_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_UPGRADE_SUCC, EVTLOG_SUCCEED_UPGRADE);
		} else {
			NDAS_DLU_LOG_ERROR_MODULE_COMPLETION(NDASSCSI_IO_UPGRADE_FAIL, EVTLOG_FAIL_UPGRADE);
		}
		NeedToUpdatePdoInfoInLSBus = TRUE;
	}

	//
	// If device lock CCB is successful, copy the result to the SRB.
	//

	if(Ccb->OperationCode == CCB_OPCODE_DEVLOCK) {
		if(Ccb->CcbStatus == CCB_STATUS_SUCCESS) {
			PSRB_IO_CONTROL			srbIoctlBuffer;
			PUCHAR					lockIoctlBuffer;
			PNDSCIOCTL_DEVICELOCK	ioCtlAcReDeviceLock;
			PLURN_DEVLOCK_CONTROL	lurnAcReDeviceLock;

			//
			// Get the Ioctl buffer.
			//
			srbIoctlBuffer = (PSRB_IO_CONTROL)srb->DataBuffer;
			srbIoctlBuffer->ReturnCode = SRB_STATUS_SUCCESS;
			lockIoctlBuffer = (PUCHAR)(srbIoctlBuffer + 1);
			ioCtlAcReDeviceLock = (PNDSCIOCTL_DEVICELOCK)lockIoctlBuffer;
			lurnAcReDeviceLock = (PLURN_DEVLOCK_CONTROL)Ccb->DataBuffer;

			// Copy the result
			RtlCopyMemory(	ioCtlAcReDeviceLock->LockData,
							lurnAcReDeviceLock->LockData,
							NDSCLOCK_LOCKDATA_LENGTH);
		}
	}

	KDPrint(3,("CcbStatus %d\n", Ccb->CcbStatus));

	//
	//	Translate CcbStatus to SrbStatus
	//

	CcbStatusToSrbStatus(Ccb, srb);

	//
	// Perform stop process when we get stop status.
	//

	if(Ccb->CcbStatus == CCB_STATUS_STOP) {
		//
		// Stop in the timer routine.
		//
		KDPrint(1, ("LUR initiate stop procedure.\n"));

		InterlockedDecrement(&ndasDluExtension->RequestExecuting);
		LsuDecrementTdiClientInProgress();

		NdasPortNotification(
			BusChangeDetected,
			LogicalUnitExtension, 
			ndasDluExtension->PathId);

		NdasPortCompleteRequest(
			LogicalUnitExtension, 
			ndasDluExtension->PathId,
			ndasDluExtension->TargetId,
			ndasDluExtension->Lun,
			SRB_STATUS_UNEXPECTED_BUS_FREE);

		return return_status;
	} if(Ccb->CcbStatus == CCB_STATUS_BUSY) {
		NDASDLU_WORKITEM_INIT			workitemCtx;

		KDPrint(1, ("LUR initiate reconnect. Retry...\n"));

		// Clear the request count.
		InterlockedDecrement(&ndasDluExtension->RequestExecuting);
		LsuDecrementTdiClientInProgress();

		// Queue the retry worker.
		NDASDLU_INIT_WORKITEM(
			&workitemCtx,
			NdasDluRetryWorker,
			NULL,
			(PVOID)srb,
			(PVOID)NULL,
			NULL
		);
		NdasDluQueueWorkItem(LogicalUnitExtension, &workitemCtx);

		return return_status;
	} else {
		//
		// Update PDO information on the NDAS bus.
		//
#if 0
		if(NeedToUpdatePdoInfoInLSBus)
		{
			KDPrint(1, ("<<<<<<<<<<<<<<<< %08lx -> %08lx ADAPTER STATUS CHANGED"
				" >>>>>>>>>>>>>>>>\n", AdapterStatusBefore, AdapterStatus));
			UpdatePdoInfoInLSBus(LogicalUnitExtension, ndasDluExtension->DluInternalStatus);
		}
#endif
	}

	//
	// Process Abort CCB.
	//
	abortCcb = Ccb->AbortCcb;

	if(abortCcb != NULL) {

		KDPrint(1,("abortSrb\n"));
		ASSERT(FALSE);

		((PSCSI_REQUEST_BLOCK)abortCcb->Srb)->SrbStatus = SRB_STATUS_ABORTED;

		InterlockedDecrement(&ndasDluExtension->RequestExecuting);
		LsuDecrementTdiClientInProgress();

		NdasPortNotification(
			RequestComplete,
			LogicalUnitExtension,
			(PSCSI_REQUEST_BLOCK)abortCcb->Srb);

	}

	//
	// Complete the current SRB
	//
	InterlockedDecrement(&ndasDluExtension->RequestExecuting);
	LsuDecrementTdiClientInProgress();

	NdasPortNotification(
		RequestComplete,
		LogicalUnitExtension,
		srb);
	NdasPortNotification(
		NextLuRequest,
		LogicalUnitExtension,
		srb);


	return return_status;
}

