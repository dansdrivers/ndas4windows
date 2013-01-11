#include <ntddk.h>
#include <stdio.h>

#include "ver.h"
#include "LSKLib.h"
#include "basetsdex.h"
#include "cipher.h"
#include "hdreg.h"
#include "binparams.h"

#include "KDebug.h"
#include "LSProto.h"
#include "LSLurn.h"
#include "LSLurnIde.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "LurnIde"


//////////////////////////////////////////////////////////////////////////
//
//	common to all IDE interface
//
void
ConvertString(
		PCHAR	result,
		PCHAR	source,
		int	size
	) {
	int	i;

	for(i = 0; i < size / 2; i++) {
		result[i * 2] = source[i * 2 + 1];
		result[i * 2 + 1] = source[i * 2];
	}
	result[size] = '\0';
	
}


BOOLEAN
Lba_capacity_is_ok(
				   struct hd_driveid *id
				   )
{
	unsigned _int32	lba_sects, chs_sects, head, tail;

	if((id->command_set_2 & 0x0400) && (id->cfs_enable_2 & 0x0400)) {
		// 48 Bit Drive.
		return TRUE;
	}

	/*
		The ATA spec tells large drivers to return
		C/H/S = 16383/16/63 independent of their size.
		Some drives can be jumpered to use 15 heads instead of 16.
		Some drives can be jumpered to use 4092 cyls instead of 16383
	*/
	if((id->cyls == 16383 || (id->cyls == 4092 && id->cur_cyls== 16383)) 
		&& id->sectors == 63 
		&& (id->heads == 15 || id->heads == 16)
		&& id->lba_capacity >= (unsigned)(16383 * 63 * id->heads))
		return TRUE;
	
	lba_sects = id->lba_capacity;
	chs_sects = id->cyls * id->heads * id->sectors;

	/* Perform a rough sanity check on lba_sects: within 10% is OK */
	if((lba_sects - chs_sects) < chs_sects / 10) {
		return TRUE;
	}

	/* Some drives have the word order reversed */
	head = ((lba_sects >> 16) & 0xffff);
	tail = (lba_sects & 0xffff);
	lba_sects = (head | (tail << 16));
	if((lba_sects - chs_sects) < chs_sects / 10) {
		id->lba_capacity = lba_sects;
		KDPrintM(DBG_LURN_ERROR, ("Lba_capacity_is_ok: Capacity reversed....\n"));
		return TRUE;
	}

	return FALSE;
}

//
//	Ide query
//
NTSTATUS
LurnIdeQuery(
		PLURELATION_NODE	Lurn,
		PLURNEXT_IDE_DEVICE	IdeDev,
		PCCB				Ccb
	) {
    PLUR_QUERY			LurQuery;
	NTSTATUS			status;


    //
    // Start off being paranoid.
    //
    if (Ccb->DataBuffer == NULL) {
		KDPrintM(DBG_LURN_ERROR,("DataBuffer is NULL\n"));
        return STATUS_UNSUCCESSFUL;
    }
	status = STATUS_SUCCESS;
	Ccb->CcbStatus = CCB_STATUS_SUCCESS;

	//
    // Extract the query
    //
    LurQuery = (PLUR_QUERY)Ccb->DataBuffer;

    switch (LurQuery->InfoClass)
	{
	case LurPrimaryLurnInformation:
		{
			PLURN_PRIMARYINFORMATION	ReturnInfo;
			PLURN_INFORMATION			LurnInfo;

			KDPrintM(DBG_LURN_ERROR,("LurPrimaryLurnInformation\n"));

			ReturnInfo = (PLURN_PRIMARYINFORMATION)LUR_QUERY_INFORMATION(LurQuery);
			ReturnInfo->Length = sizeof(LURN_PRIMARYINFORMATION);
			LurnInfo = &ReturnInfo->PrimaryLurn;

			LurnInfo->Length = sizeof(LURN_INFORMATION);
			LurnInfo->UnitBlocks = Lurn->UnitBlocks;
			LurnInfo->BlockUnit	= BLOCK_SIZE;
			LurnInfo->AccessRight = Lurn->AccessRight;

			if(IdeDev) {
				LurnInfo->UnitDiskId = IdeDev->LuData.UnitNumber;
			RtlCopyMemory(&LurnInfo->BindingAddress, &IdeDev->LanScsiSession.SourceAddress, sizeof(TA_LSTRANS_ADDRESS));
			RtlCopyMemory(&LurnInfo->NetDiskAddress, &IdeDev->LanScsiSession.LSNodeAddress, sizeof(TA_LSTRANS_ADDRESS));
			RtlCopyMemory(&LurnInfo->UserID, &IdeDev->LanScsiSession.UserID, LSPROTO_USERID_LENGTH);
			RtlCopyMemory(&LurnInfo->Password, &IdeDev->LanScsiSession.Password, LSPROTO_PASSWORD_LENGTH);
			} else {
				LurnInfo->UnitDiskId = 0;
				RtlZeroMemory(&LurnInfo->BindingAddress, sizeof(TA_LSTRANS_ADDRESS));
				RtlZeroMemory(&LurnInfo->NetDiskAddress, sizeof(TA_LSTRANS_ADDRESS));
				RtlZeroMemory(&LurnInfo->UserID, LSPROTO_USERID_LENGTH);
				RtlZeroMemory(&LurnInfo->Password, LSPROTO_PASSWORD_LENGTH);
			}
		}
		break;
	case LurEnumerateLurn:
		{
			PLURN_ENUM_INFORMATION	ReturnInfo;
			PLURN_INFORMATION		LurnInfo;

			KDPrintM(DBG_LURN_ERROR,("LurPrimaryLurnInformation\n"));

			ReturnInfo = (PLURN_ENUM_INFORMATION)LUR_QUERY_INFORMATION(LurQuery);
			LurnInfo = &ReturnInfo->Lurns[Lurn->LurnId];

			LurnInfo->Length = sizeof(LURN_INFORMATION);


			LurnInfo->UnitBlocks = Lurn->UnitBlocks;
			LurnInfo->BlockUnit	= BLOCK_SIZE;
			LurnInfo->AccessRight = Lurn->AccessRight;

			if(IdeDev) {
				LurnInfo->UnitDiskId = IdeDev->LuData.UnitNumber;
			RtlCopyMemory(&LurnInfo->BindingAddress, &IdeDev->LanScsiSession.SourceAddress, sizeof(TA_LSTRANS_ADDRESS));
			RtlCopyMemory(&LurnInfo->NetDiskAddress, &IdeDev->LanScsiSession.LSNodeAddress, sizeof(TA_LSTRANS_ADDRESS));
			RtlCopyMemory(&LurnInfo->UserID, &IdeDev->LanScsiSession.UserID, LSPROTO_USERID_LENGTH);
			RtlCopyMemory(&LurnInfo->Password, &IdeDev->LanScsiSession.Password, LSPROTO_PASSWORD_LENGTH);
			} else {
				LurnInfo->UnitDiskId = 0;
				RtlZeroMemory(&LurnInfo->BindingAddress, sizeof(TA_LSTRANS_ADDRESS));
				RtlZeroMemory(&LurnInfo->NetDiskAddress, sizeof(TA_LSTRANS_ADDRESS));
				RtlZeroMemory(&LurnInfo->UserID, LSPROTO_USERID_LENGTH);
				RtlZeroMemory(&LurnInfo->Password, LSPROTO_PASSWORD_LENGTH);
			}

			LurnInfo->LurnId = Lurn->LurnId;
			LurnInfo->LurnType = Lurn->LurnType;
			LurnInfo->StatusFlags = Lurn->LurnStatus;

		}
		break;

	case LurRefreshLurn:
		{
			PLURN_REFRESH			ReturnInfo;

			KDPrintM(DBG_LURN_TRACE,("LurRefreshLurn\n"));

			ReturnInfo = (PLURN_REFRESH)LUR_QUERY_INFORMATION(LurQuery);

			if(LURN_STATUS_STOP == Lurn->LurnStatus)
			{
				KDPrintM(DBG_LURN_ERROR,("!!!!!!!! LURN_STATUS_STOP == Lurn->LurnStatus !!!!!!!!\n"));
				ReturnInfo->CcbStatusFlags |= CCBSTATUS_FLAG_LURN_STOP;
			}
		}
		break;

	default:
		Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
		break;
	}


	return status;
}

//
//	Request dispatcher for all IDE devices.
//
NTSTATUS
LurnIdeUpdate(
		PLURELATION_NODE		Lurn,
		PLURNEXT_IDE_DEVICE		IdeExt,
		PCCB					Ccb
	) {
	NTSTATUS		status;
	PLURN_UPDATE	LurnUpdate;
	KIRQL			oldIrql;
	LARGE_INTEGER	GenericTimeOut;

	LurnUpdate = (PLURN_UPDATE)Ccb->DataBuffer;

	//
	//	Update the LanscsiSession
	//
	switch(LurnUpdate->UpdateClass) {
	case LURN_UPDATECLASS_WRITEACCESS_USERID:
	case LURN_UPDATECLASS_READONLYACCESS:
		{
		LSSLOGIN_INFO		LoginInfo;
		LSPROTO_TYPE		LSProto;
		PLANSCSI_SESSION	NewLanScsiSession;
		TA_LSTRANS_ADDRESS	BindingAddress, TargetAddress;
		BYTE				pdu_response;

		//
		//	Send NOOP to make sure that the Lanscsi Node is reachable.
		//
		status = LspNoOperation(&IdeExt->LanScsiSession, IdeExt->LuData.LanscsiTargetID, &pdu_response);
		if(!NT_SUCCESS(status) || pdu_response != LANSCSI_RESPONSE_SUCCESS) {
			Ccb->CcbStatus = CCB_STATUS_COMMUNICATION_ERROR;
			break;
		}
/*
#if DBG
		{
			static LONG	debugCount = 0;
			LONG	ccbSuccess;

			ccbSuccess = InterlockedIncrement(&debugCount);
			if((ccbSuccess%2) == 1) {
				KDPrintM(DBG_LURN_ERROR, ("force Update CCB to fail.\n"));
				Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
				break;
			}
		}
#endif
*/
		//
		//	Try to make a new connection with write rights.
		//
		NewLanScsiSession = ExAllocatePool(NonPagedPool, sizeof(LANSCSI_SESSION));
		if(NewLanScsiSession == NULL) {
			KDPrintM(DBG_LURN_ERROR, ("LURN_UPDATECLASS_WRITEACCESS_USERID: ExAllocatePool() failed.\n"));
			Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
			break;
		}
		RtlZeroMemory(NewLanScsiSession, sizeof(LANSCSI_SESSION));
		LspGetAddresses(&IdeExt->LanScsiSession, &BindingAddress, &TargetAddress);
		//
		//	Set timeouts.
		//
		GenericTimeOut.QuadPart = LURNIDE_GENERIC_TIMEOUT;
		LspSetTimeOut(NewLanScsiSession, &GenericTimeOut);
		status = LspConnect(
					NewLanScsiSession,
					&BindingAddress,
					&TargetAddress
				);
		if(!NT_SUCCESS(status)) {
			KDPrintM(DBG_LURN_ERROR, ("LURN_UPDATECLASS_WRITEACCESS_USERID: LspConnect(), Can't Connect to the LS node. STATUS:0x%08x\n", status));
			Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
			ExFreePool(NewLanScsiSession);
			break;
		}

		//
		//	Upgrade the access right.
		//	Login to the Lanscsi Node.
		//
		LspBuildLoginInfo(&IdeExt->LanScsiSession, &LoginInfo);
		status = LspLookupProtocol(LoginInfo.HWType, LoginInfo.HWVersion, &LSProto);
		if(!NT_SUCCESS(status)) {
			KDPrintM(DBG_LURN_ERROR, ("Wrong hardware version.\n"));
			LspDisconnect(
					NewLanScsiSession
				);
			Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
			ExFreePool(NewLanScsiSession);
			break;
		}

		if(LurnUpdate->UpdateClass == LURN_UPDATECLASS_WRITEACCESS_USERID) {

			if(LoginInfo.UserID & 0xffff0000) {
				KDPrintM(DBG_LURN_ERROR, ("WRITE/READONLYACCESS: the write-access has been in UserID(%08lx)."
											"We don't need to do it again.\n", LoginInfo.UserID));
				LspDisconnect(
						NewLanScsiSession
					);
				Ccb->CcbStatus = CCB_STATUS_SUCCESS;
				ExFreePool(NewLanScsiSession);
				break;
			}

			//
			//	Add Write access.
			//

			LoginInfo.UserID = LoginInfo.UserID | (LoginInfo.UserID << 16);

		} else if(LurnUpdate->UpdateClass == LURN_UPDATECLASS_READONLYACCESS) {

			if(!(LoginInfo.UserID & 0xffff0000)) {
				KDPrintM(DBG_LURN_ERROR, ("WRITE/READONLYACCESS: the readonly-access has been in UserID(%08lx)."
					"We don't need to do it again.\n", LoginInfo.UserID));
				LspDisconnect(
					NewLanScsiSession
					);
				Ccb->CcbStatus = CCB_STATUS_SUCCESS;
				ExFreePool(NewLanScsiSession);
				break;
			}

			//
			//	Remove Write access.
			//

			LoginInfo.UserID = LoginInfo.UserID & ~(LoginInfo.UserID << 16);

		}

		status = LspLogin(
						NewLanScsiSession,
						&LoginInfo,
						LSProto,
						TRUE
					);
		if(!NT_SUCCESS(status)) {
			KDPrintM(DBG_LURN_ERROR, ("WRITE/READONLYACCESS: LspLogin(), Can't log into the LS node. STATUS:0x%08x\n", status));
			LspDisconnect(
					NewLanScsiSession
				);
			Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
			ExFreePool(NewLanScsiSession);
			break;
		}

		//
		//	Disconnect the original session.
		//	And copy NewLanscsiSession to LanscsiSession.
		//
		LspLogout(
				&IdeExt->LanScsiSession
			);

		LspDisconnect(
				&IdeExt->LanScsiSession
			);

		ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);

		if(LurnUpdate->UpdateClass == LURN_UPDATECLASS_WRITEACCESS_USERID) {
			Lurn->AccessRight |= GENERIC_WRITE;

			//
			//	If this is root LURN, update LUR access right.
			//
			if(Lurn->LurnParent == 0 || Lurn->LurnParent == Lurn) {
				Lurn->Lur->GrantedAccess |= GENERIC_WRITE;
			}
		} else if(LurnUpdate->UpdateClass == LURN_UPDATECLASS_READONLYACCESS) {
			Lurn->AccessRight &= ~GENERIC_WRITE;

			//
			//	If this is root LURN, update LUR access right.
			//
			if(Lurn->LurnParent == 0 || Lurn->LurnParent == Lurn) {
				Lurn->Lur->GrantedAccess &= ~GENERIC_WRITE;
			}
		}

		LspCopy(&IdeExt->LanScsiSession, NewLanScsiSession);

		RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

		ExFreePool(NewLanScsiSession);

		Ccb->CcbStatus = CCB_STATUS_SUCCESS;
		break;
	}
	default:
		Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
		break;
	}

	return STATUS_SUCCESS;
}

NTSTATUS
LurnIdeResetBus(
		PLURELATION_NODE Lurn,
		PCCB Ccb
	) {
	PLURNEXT_IDE_DEVICE		IdeDisk;
	KIRQL					oldIrql;

	ASSERT(Lurn->LurnExtension);
	UNREFERENCED_PARAMETER(Ccb);

	KDPrintM(DBG_LURN_INFO, ("entered with LURN%d.\n", Lurn->LurnId));

	IdeDisk = (PLURNEXT_IDE_DEVICE)Lurn->LurnExtension;

	ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);
	CcbCompleteList(&IdeDisk->ThreadCcbQueue, CCB_STATUS_RESET, 0);
	RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

	return STATUS_SUCCESS;
}

VOID
LurnIdeStop(
		PLURELATION_NODE Lurn,
		PCCB Ccb
	) {
	PLURNEXT_IDE_DEVICE		IdeDisk;
	KIRQL					oldIrql;

	ASSERT(Lurn->LurnExtension);
	UNREFERENCED_PARAMETER(Ccb);

	KDPrintM(DBG_LURN_INFO, ("entered.\n"));

	IdeDisk = (PLURNEXT_IDE_DEVICE)Lurn->LurnExtension;

	ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);
	CcbCompleteList(&IdeDisk->ThreadCcbQueue, CCB_STATUS_RESET, CCBSTATUS_FLAG_LURN_STOP);
	RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);
}

NTSTATUS
LurnIdeRestart(
		PLURELATION_NODE	Lurn,
		PCCB				Ccb
	) {
	NTSTATUS	status;

	UNREFERENCED_PARAMETER(Lurn);
	UNREFERENCED_PARAMETER(Ccb);

	KDPrintM(DBG_LURN_INFO, ("entered.\n"));
	status = STATUS_NOT_IMPLEMENTED;


	return status;
}

NTSTATUS
LurnIdeAbortCommand(
		PLURELATION_NODE Lurn,
		PCCB Ccb
	) {
	NTSTATUS	status;

	UNREFERENCED_PARAMETER(Lurn);
	UNREFERENCED_PARAMETER(Ccb);

	KDPrintM(DBG_LURN_INFO, ("entered.\n"));
	status = STATUS_NOT_IMPLEMENTED;


	return status;
}


#define RECONNECTION_MAX_TRY_PRIMARY(RECONNECTION_MAX_TRY)		((RECONNECTION_MAX_TRY + 1) / 2)
#define RECONNECTION_INTERVAL_SECONDARY(PRIMARY_RECONNECT_INTERVAL)		(PRIMARY_RECONNECT_INTERVAL + NANO100_PER_SEC*4)

//
//	Reconnect to NetDisk.
//
NTSTATUS
LSLurnIdeReconnect(
		PLURELATION_NODE		Lurn,
		PLURNEXT_IDE_DEVICE		IdeExt
	) {
	LARGE_INTEGER	TimeInterval;
	LONG			StallCount;
	LONG			MaxStall;
	NTSTATUS		status;
	KIRQL			oldIrql;
	BOOLEAN			LssEncBuff;
	LARGE_INTEGER	shortTimeout;
	LARGE_INTEGER	genericTimeout;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);
	KDPrintM(DBG_LURN_ERROR,("entered.\n"));

	status = STATUS_SUCCESS;

	//
	//	Adjust number of trials and interval by determining NDAS share mode.
	//
	if(Lurn->Lur->LurFlags & LURFLAG_WRITESHARE_PS) {

		if(Lurn->Lur->DesiredAccess & GENERIC_WRITE) {
			if(Lurn->Lur->GrantedAccess & GENERIC_WRITE) {
				//
				//	NDFS Primary mode.
				//
				TimeInterval.QuadPart = - Lurn->ReconnInterval;
				MaxStall = RECONNECTION_MAX_TRY_PRIMARY(Lurn->ReconnTrial);
			} else {
				//
				//	NDFS Secondary mode.
				//	Maximum interval should be less than BUS RESET timeout ( 10 seconds )
				//
				TimeInterval.QuadPart = - RECONNECTION_INTERVAL_SECONDARY(Lurn->ReconnInterval);
				ASSERT(RECONNECTION_INTERVAL_SECONDARY(Lurn->ReconnInterval) <= NANO100_PER_SEC*9);
				MaxStall = Lurn->ReconnTrial;
			}
		} else {
			//
			//	Read-only mode
			//
			TimeInterval.QuadPart = - Lurn->ReconnInterval;
			MaxStall = Lurn->ReconnTrial;
		}
	} else {
		TimeInterval.QuadPart = - Lurn->ReconnInterval;
		MaxStall = Lurn->ReconnTrial;

	}

	StallCount = InterlockedIncrement(&Lurn->NrOfStall);
	if(StallCount == 1) {
		//
		//	Reconnect first time and set LurnStatus to STALL.
		//
		ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);
		Lurn->LurnStatus = LURN_STATUS_STALL;
		RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);
	}
	if(StallCount>MaxStall) {
		KDPrintM(DBG_LURN_ERROR,("reconnection trial reach the maximum %d! Connection will be lost.\n", MaxStall));
//		ASSERT(FALSE);

		return STATUS_CONNECTION_COUNT_LIMIT;
	}


	//
	//	Set the short timeout not to block too long here.
	//
	shortTimeout.QuadPart = 1 * NANO100_PER_SEC;
	LspSetTimeOut(&IdeExt->LanScsiSession, &shortTimeout);

	LspLogout(
			&IdeExt->LanScsiSession
		);
	LspDisconnect(
			&IdeExt->LanScsiSession
		);

	genericTimeout.QuadPart = LURNIDE_GENERIC_TIMEOUT;
	LspSetTimeOut(&IdeExt->LanScsiSession, &shortTimeout);

	KeDelayExecutionThread(
			KernelMode,
			FALSE,
			&(TimeInterval)
		);

	//
	//	If the Ide extension has a encryption buffer, LSS does not need a encryption buffer.
	//
	if(IdeExt->CntEcrBufferLength &&IdeExt->CntEcrBuffer) {
		LssEncBuff = FALSE;
	} else {
		LssEncBuff = TRUE;
	}
	status = LspReconnectAndLogin(&IdeExt->LanScsiSession, IdeExt->LstransType, LssEncBuff);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_ERROR,("failed.\n"));
		return status;
	}
	if(NT_SUCCESS(status)) {
		//
		//	Reconnecting succeeded.
		//	Reset Stall counter.
		//
		ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);
		ASSERT(Lurn->LurnStatus == LURN_STATUS_STALL);
		Lurn->LurnStatus = LURN_STATUS_RUNNING;
		RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);
		InterlockedExchange(&Lurn->NrOfStall, 0);
	} else {
		status = STATUS_UNSUCCESSFUL;
	}

	return status;
}

NTSTATUS
LSLurnIdeNoop(
		PLURELATION_NODE		Lurn,
		PLURNEXT_IDE_DEVICE		IdeExt,
		PCCB					Ccb
	) {
	BYTE		PduResponse;
	NTSTATUS	status;

	KDPrintM(DBG_LURN_TRACE, ("Send NOOP to Remote.\n"));

	UNREFERENCED_PARAMETER(Lurn);

	status = LspNoOperation(&IdeExt->LanScsiSession, IdeExt->LuData.LanscsiTargetID, &PduResponse);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_ERROR, ("LspNoOperation() failed. NTSTATUS:%08lx\n", status));
		if(Ccb) Ccb->CcbStatus = CCB_STATUS_COMMUNICATION_ERROR;
		return STATUS_PORT_DISCONNECTED;

	} else if(PduResponse != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_LURN_ERROR, ("Failure reply. PDUSTATUS:%08lx\n",(int)PduResponse));

		if(Ccb) Ccb->CcbStatus = CCB_STATUS_COMMUNICATION_ERROR;
		return STATUS_PORT_DISCONNECTED;
	}

	if(Ccb) Ccb->CcbStatus = CCB_STATUS_SUCCESS;

	return STATUS_SUCCESS;
}



NTSTATUS
LurnIdeDestroy(
		PLURELATION_NODE Lurn
	) {
	PLURNEXT_IDE_DEVICE	IdeDev = (PLURNEXT_IDE_DEVICE)Lurn->LurnExtension;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	if(IdeDev) {
		if(IdeDev->CntEcrBuffer && IdeDev->CntEcrBufferLength)
			ExFreePoolWithTag(IdeDev->CntEcrBuffer, LURNEXT_ENCBUFF_TAG);
		if(IdeDev->CntEcrKey)
			CloseCipherKey(IdeDev->CntEcrKey);
		if(IdeDev->CntEcrCipher)
			CloseCipher(IdeDev->CntEcrCipher);
	}

	if(Lurn->LurnExtension) {
		ExFreePoolWithTag(Lurn->LurnExtension, LURNEXT_POOL_TAG);
		Lurn->LurnExtension =NULL;
	}
	if(Lurn->LurnDesc) {
		ExFreePoolWithTag(Lurn->LurnDesc, LURNEXT_POOL_TAG);
		Lurn->LurnDesc = NULL;
	}

	return STATUS_SUCCESS;
}
