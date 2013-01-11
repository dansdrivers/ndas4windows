#include "ndasscsiproc.h"

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
			LurnInfo->BlockBytes = Lurn->BlockBytes;
			LurnInfo->AccessRight = Lurn->AccessRight;

			if(IdeDev) {
				LurnInfo->UnitDiskId = IdeDev->LuHwData.LanscsiTargetID;
#if !__NDAS_SCSI_LPXTDI_V2__
				RtlCopyMemory(&LurnInfo->BindingAddress, &IdeDev->LanScsiSession->BindAddress, sizeof(TA_LSTRANS_ADDRESS));
				RtlCopyMemory(&LurnInfo->NetDiskAddress, &IdeDev->LanScsiSession->LSNodeAddress, sizeof(TA_LSTRANS_ADDRESS));
#else
				RtlCopyMemory( &LurnInfo->NdasBindingAddress, 
							   &IdeDev->LanScsiSession->NdasBindAddress, 
							   (FIELD_OFFSET(TA_ADDRESS, Address) + 
							   TDI_ADDRESS_LENGTH_LPX) );

				RtlCopyMemory( &LurnInfo->NdasNetDiskAddress, 
							   &IdeDev->LanScsiSession->NdasNodeAddress, 
							   (FIELD_OFFSET(TA_ADDRESS, Address) + 
							   TDI_ADDRESS_LENGTH_LPX) );

#endif
				RtlCopyMemory(&LurnInfo->UserID, &IdeDev->LanScsiSession->UserID, LSPROTO_USERID_LENGTH);
				RtlCopyMemory(&LurnInfo->Password, &IdeDev->LanScsiSession->Password, LSPROTO_PASSWORD_LENGTH);
			} else if (Lurn->SavedLurnDesc) {
				//
				// IdeDev can be null if in degraded mode, but RAID will send this request always to first member.
				// We should set some value for this case.
				//
				LurnInfo->UnitDiskId = Lurn->SavedLurnDesc->LurnIde.LanscsiTargetID;
#if !__NDAS_SCSI_LPXTDI_V2__
				RtlCopyMemory(&LurnInfo->BindingAddress, &Lurn->SavedLurnDesc->LurnIde.BindingAddress, sizeof(TA_LSTRANS_ADDRESS));
				RtlCopyMemory(&LurnInfo->NetDiskAddress, &Lurn->SavedLurnDesc->LurnIde.TargetAddress, sizeof(TA_LSTRANS_ADDRESS));
#else

				LurnInfo->NdasBindingAddress.AddressLength = Lurn->SavedLurnDesc->LurnIde.BindingAddress.Address[0].AddressLength;
				LurnInfo->NdasBindingAddress.AddressType   = Lurn->SavedLurnDesc->LurnIde.BindingAddress.Address[0].AddressType;
					
				RtlCopyMemory( LurnInfo->NdasBindingAddress.Address, 
							   &Lurn->SavedLurnDesc->LurnIde.BindingAddress.Address[0].Address, 
							   Lurn->SavedLurnDesc->LurnIde.BindingAddress.Address[0].AddressLength );
	
				LurnInfo->NdasNetDiskAddress.AddressLength = Lurn->SavedLurnDesc->LurnIde.TargetAddress.Address[0].AddressLength;
				LurnInfo->NdasNetDiskAddress.AddressType   = Lurn->SavedLurnDesc->LurnIde.TargetAddress.Address[0].AddressType;
					
				RtlCopyMemory( LurnInfo->NdasNetDiskAddress.Address, 
							   &Lurn->SavedLurnDesc->LurnIde.TargetAddress.Address[0].Address, 
							   Lurn->SavedLurnDesc->LurnIde.TargetAddress.Address[0].AddressLength );

#endif
				RtlCopyMemory(&LurnInfo->UserID, &Lurn->SavedLurnDesc->LurnIde.UserID, LSPROTO_USERID_LENGTH);
				RtlCopyMemory(&LurnInfo->Password, &Lurn->SavedLurnDesc->LurnIde.Password, LSPROTO_PASSWORD_LENGTH);
			} else {
				LurnInfo->UnitDiskId = 0;
#if !__NDAS_SCSI_LPXTDI_V2__
				RtlZeroMemory(&LurnInfo->BindingAddress, sizeof(TA_LSTRANS_ADDRESS));
				RtlZeroMemory(&LurnInfo->NetDiskAddress, sizeof(TA_LSTRANS_ADDRESS));
#else
				RtlZeroMemory( &LurnInfo->NdasBindingAddress, 
							   (FIELD_OFFSET(TA_ADDRESS, Address) + 
							   TDI_ADDRESS_LENGTH_LPX) );

				LurnInfo->NdasBindingAddress.AddressType = TDI_ADDRESS_TYPE_INVALID;

				RtlZeroMemory( &LurnInfo->NdasNetDiskAddress, 
							   (FIELD_OFFSET(TA_ADDRESS, Address) + 
							   TDI_ADDRESS_LENGTH_LPX) );

				LurnInfo->NdasNetDiskAddress.AddressType = TDI_ADDRESS_TYPE_INVALID;

#endif
				RtlZeroMemory(&LurnInfo->UserID, LSPROTO_USERID_LENGTH);
				RtlZeroMemory(&LurnInfo->Password, LSPROTO_PASSWORD_LENGTH);
			}

			RtlZeroMemory(LurnInfo->PrimaryId, LURN_PRIMARY_ID_LENGTH);
#if !__NDAS_SCSI_LPXTDI_V2__
			RtlCopyMemory(&LurnInfo->PrimaryId[0], &(LurnInfo->NetDiskAddress.Address[0].Address), 8);
#else
			RtlCopyMemory( &LurnInfo->PrimaryId[0], LurnInfo->NdasNetDiskAddress.Address, 8 );
#endif
			LurnInfo->PrimaryId[8] = LurnInfo->UnitDiskId;
		}
		break;
	case LurEnumerateLurn:
		{
			PLURN_ENUM_INFORMATION	ReturnInfo;
			PLURN_INFORMATION		LurnInfo;

			KDPrintM(DBG_LURN_ERROR,("LurEnumerateLurn\n"));

			ReturnInfo = (PLURN_ENUM_INFORMATION)LUR_QUERY_INFORMATION(LurQuery);
			LurnInfo = &ReturnInfo->Lurns[Lurn->LurnId];

			LurnInfo->Length = sizeof(LURN_INFORMATION);


			LurnInfo->UnitBlocks  = Lurn->UnitBlocks;
			LurnInfo->BlockBytes  = Lurn->BlockBytes;
			LurnInfo->AccessRight = Lurn->AccessRight;

			if(IdeDev) {
				LurnInfo->UnitDiskId = IdeDev->LuHwData.LanscsiTargetID;
#if !__NDAS_SCSI_LPXTDI_V2__
				RtlCopyMemory(&LurnInfo->BindingAddress, &IdeDev->LanScsiSession->BindAddress, sizeof(TA_LSTRANS_ADDRESS));
				RtlCopyMemory(&LurnInfo->NetDiskAddress, &IdeDev->LanScsiSession->LSNodeAddress, sizeof(TA_LSTRANS_ADDRESS));
#else
				RtlCopyMemory( &LurnInfo->NdasBindingAddress, 
							   &IdeDev->LanScsiSession->NdasBindAddress, 
							   (FIELD_OFFSET(TA_ADDRESS, Address) + 
							   TDI_ADDRESS_LENGTH_LPX) );

				RtlCopyMemory( &LurnInfo->NdasNetDiskAddress, 
							   &IdeDev->LanScsiSession->NdasNodeAddress, 
							   (FIELD_OFFSET(TA_ADDRESS, Address) + 
							   TDI_ADDRESS_LENGTH_LPX) );

#endif
				RtlCopyMemory(&LurnInfo->UserID, &IdeDev->LanScsiSession->UserID, LSPROTO_USERID_LENGTH);
				RtlCopyMemory(&LurnInfo->Password, &IdeDev->LanScsiSession->Password, LSPROTO_PASSWORD_LENGTH);
			} else {
				LurnInfo->UnitDiskId = 0;
#if !__NDAS_SCSI_LPXTDI_V2__
				RtlZeroMemory(&LurnInfo->BindingAddress, sizeof(TA_LSTRANS_ADDRESS));
				RtlZeroMemory(&LurnInfo->NetDiskAddress, sizeof(TA_LSTRANS_ADDRESS));
#else
				RtlZeroMemory( &LurnInfo->NdasBindingAddress, 
							   (FIELD_OFFSET(TA_ADDRESS, Address) + 
							   TDI_ADDRESS_LENGTH_LPX) );

				LurnInfo->NdasBindingAddress.AddressType = TDI_ADDRESS_TYPE_INVALID;

				RtlZeroMemory( &LurnInfo->NdasNetDiskAddress, 
							   (FIELD_OFFSET(TA_ADDRESS, Address) + 
							   TDI_ADDRESS_LENGTH_LPX) );

				LurnInfo->NdasNetDiskAddress.AddressType = TDI_ADDRESS_TYPE_INVALID;

#endif
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

#if !__NDAS_SCSI_LPXTDI_V2__
		TA_LSTRANS_ADDRESS	BindingAddress;
		TA_LSTRANS_ADDRESS  TargetAddress;
#else
		UCHAR			bindingBuffer[(FIELD_OFFSET(TA_ADDRESS, Address) + TDI_ADDRESS_LENGTH_LPX)];
		UCHAR			targetBuffer[(FIELD_OFFSET(TA_ADDRESS, Address) + TDI_ADDRESS_LENGTH_LPX)];
		PTA_ADDRESS		BindingAddress = (PTA_ADDRESS)bindingBuffer;
		PTA_ADDRESS  	TargetAddress = (PTA_ADDRESS)targetBuffer;
#endif

		BYTE				pdu_response;
		LARGE_INTEGER		genericTimeOut;

		//
		//	Send NOOP to make sure that the Lanscsi Node is reachable.
		//
		status = LspNoOperation(IdeExt->LanScsiSession, IdeExt->LuHwData.LanscsiTargetID, &pdu_response, NULL);
		if(!NT_SUCCESS(status) || pdu_response != LANSCSI_RESPONSE_SUCCESS) {
			KDPrintM(DBG_LURN_ERROR, ("NOOP failed during update\n"));
			Ccb->CcbStatus = CCB_STATUS_COMMUNICATION_ERROR;
			break;
		}

#if 0
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

		//
		//	Try to make a new connection with write rights.
		//
		NewLanScsiSession = ExAllocatePoolWithTag(NonPagedPool, sizeof(LANSCSI_SESSION), LSS_POOLTAG);
		if(NewLanScsiSession == NULL) {
			KDPrintM(DBG_LURN_ERROR, ("LURN_UPDATECLASS_WRITEACCESS_USERID: ExAllocatePool() failed.\n"));
			Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
			break;
		}
		RtlZeroMemory(NewLanScsiSession, sizeof(LANSCSI_SESSION));
#if !__NDAS_SCSI_LPXTDI_V2__
		LspGetAddresses(IdeExt->LanScsiSession, &BindingAddress, &TargetAddress);
#else
		LspGetAddresses(IdeExt->LanScsiSession, BindingAddress, TargetAddress);
#endif
		//
		//	Set timeouts.
		//

		genericTimeOut.QuadPart = -LURNIDE_GENERIC_TIMEOUT;
		LspSetDefaultTimeOut(NewLanScsiSession, &genericTimeOut);
#if !__NDAS_SCSI_LPXTDI_V2__
		status = LspConnect(
					NewLanScsiSession,
					&BindingAddress,
					&TargetAddress,
					NULL,
					&genericTimeOut
				);
#else
		status = LspConnectMultiBindTaAddr( NewLanScsiSession,
										    BindingAddress,
										    NULL,
										    NULL,
										    TargetAddress,
										    TRUE,
											NULL,
											NULL,
										    &genericTimeOut );
#endif
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
		LspBuildLoginInfo(IdeExt->LanScsiSession, &LoginInfo);
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
						NULL,
						TRUE
					);
		if(!NT_SUCCESS(status)) {
			KDPrintM(DBG_LURN_ERROR, ("WRITE/READONLYACCESS: LspLogin(), Can't log into the LS node. STATUS:0x%08x\n", status));
			LspDisconnect(
					NewLanScsiSession
				);
			// Assume we can connect and negotiate but we can't get RW right due to another connection.
			Ccb->CcbStatus = CCB_STATUS_NO_ACCESS;
			ExFreePool(NewLanScsiSession);
			break;
		}

		//
		//	Disconnect the original session.
		//	And copy NewLanscsiSession to LanscsiSession.
		//
		LspLogout(
				IdeExt->LanScsiSession,
				NULL
			);

		LspDisconnect(
				IdeExt->LanScsiSession
			);

		ACQUIRE_SPIN_LOCK(&Lurn->SpinLock, &oldIrql);

		if(LurnUpdate->UpdateClass == LURN_UPDATECLASS_WRITEACCESS_USERID) {
			Lurn->AccessRight |= GENERIC_WRITE;

			//
			//	If this is root LURN, update LUR access right.
			//
			if (LURN_IS_ROOT_NODE(Lurn)) {
				Lurn->Lur->EnabledNdasFeatures &= ~NDASFEATURE_SECONDARY;
			}
		} else if(LurnUpdate->UpdateClass == LURN_UPDATECLASS_READONLYACCESS) {
			Lurn->AccessRight &= ~GENERIC_WRITE;

			//
			//	If this is root LURN, update LUR access right.
			//
			if (LURN_IS_ROOT_NODE(Lurn)) {
				Lurn->Lur->EnabledNdasFeatures |= NDASFEATURE_SECONDARY;
			}
		}

		LspCopy(IdeExt->LanScsiSession, NewLanScsiSession, TRUE);

		RELEASE_SPIN_LOCK(&Lurn->SpinLock, oldIrql);

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

	ACQUIRE_SPIN_LOCK(&Lurn->SpinLock, &oldIrql);
	CcbCompleteList(&IdeDisk->CcbQueue, CCB_STATUS_RESET, CCBSTATUS_FLAG_LURN_STOP |
		Lurn->LurnStopReasonCcbStatusFlags);
	RELEASE_SPIN_LOCK(&Lurn->SpinLock, oldIrql);

	LsCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
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
#define RECONNECTION_INTERVAL_SECONDARY(PRIMARY_RECONNECT_INTERVAL)		((PRIMARY_RECONNECT_INTERVAL) + NANO100_PER_SEC*4)

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
#if !__NDAS_SCSI_LPXTDI_V2__

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);
	KDPrintM(DBG_LURN_ERROR,("entered.(%02x:%02x:%02x)\n", 
		IdeExt->LanScsiSession->LSNodeAddress.Address[0].Address.Address[5], // Low three byte of target MAC
		IdeExt->LanScsiSession->LSNodeAddress.Address[0].Address.Address[6],
		IdeExt->LanScsiSession->LSNodeAddress.Address[0].Address.Address[7]
		));

#else

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);
	KDPrintM(DBG_LURN_ERROR,("entered.(%02x:%02x:%02x)\n", 
		IdeExt->LanScsiSession->NdasNodeAddress.Address[5], // Low three byte of target MAC
		IdeExt->LanScsiSession->NdasNodeAddress.Address[6],
		IdeExt->LanScsiSession->NdasNodeAddress.Address[7]
		));

#endif


	ACQUIRE_SPIN_LOCK(&Lurn->SpinLock, &oldIrql);
	if(!LURN_IS_RUNNING(Lurn->LurnStatus)) {
		RELEASE_SPIN_LOCK(&Lurn->SpinLock, oldIrql);
		return STATUS_UNSUCCESSFUL;
	}
	RELEASE_SPIN_LOCK(&Lurn->SpinLock, oldIrql);


	status = STATUS_SUCCESS;
#if !__NDAS_SCSI_LPXTDI_V2__
	shortTimeout.QuadPart = -1 * NANO100_PER_SEC;
#else
	shortTimeout.QuadPart = -3.5 * NANO100_PER_SEC;
#endif

	//
	//	Adjust number of trials and interval by determining NDAS share mode.
	//
	if(Lurn->Lur->DeviceMode == DEVMODE_SHARED_READWRITE) {

		if(!(Lurn->Lur->EnabledNdasFeatures & NDASFEATURE_SECONDARY)) {
			//
			//	Secondary feature disabled
			//
			TimeInterval.QuadPart = - Lurn->ReconnInterval;
			MaxStall = RECONNECTION_MAX_TRY_PRIMARY(Lurn->ReconnTrial);
		} else {
			//
			//	Secondary feature enabled
			//	Maximum interval should be less than BUS RESET timeout ( 10 seconds )
			//
			TimeInterval.QuadPart = - RECONNECTION_INTERVAL_SECONDARY(Lurn->ReconnInterval);
			ASSERT(RECONNECTION_INTERVAL_SECONDARY(Lurn->ReconnInterval) <= NANO100_PER_SEC*9);
			MaxStall = Lurn->ReconnTrial;
		}
		if (Lurn->LurnParent && LURN_REDUNDENT_TYPE(Lurn->LurnParent->LurnType)) {
			//
			// If this LURN is part of RAID with redundency, reconnecting without RAID system's ignorance is not safe 
			// 	because disconnection may be caused by power-down.
			// But we cannot differentiate whether disconnection is caused by spin-up/network instability/power-down
			//	and spin-up/network instability case is more frequent.
			//
			MaxStall = (MaxStall+3)/4;
			KDPrintM(DBG_LURN_ERROR,("Max reconnection time for RAID member = %d\n", MaxStall));
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
		ACQUIRE_SPIN_LOCK(&Lurn->SpinLock, &oldIrql);
		Lurn->LurnStatus = LURN_STATUS_STALL;
		RELEASE_SPIN_LOCK(&Lurn->SpinLock, oldIrql);
	}
	if(StallCount>MaxStall) {
		KDPrintM(DBG_LURN_ERROR,("reconnection trial reach the maximum %d! Connection will be lost.\n", MaxStall));
//		ASSERT(FALSE);

		return STATUS_CONNECTION_COUNT_LIMIT;
	}
	KDPrintM(DBG_LURN_INFO,("reconnection trial count %d\n", StallCount));


	//
	//	Set the short timeout not to block too long here.
	//

	LspLogout(
			IdeExt->LanScsiSession,
			&shortTimeout
		);

	LspDisconnect(
			IdeExt->LanScsiSession
		);


	KeDelayExecutionThread(
			KernelMode,
			FALSE,
			&(TimeInterval)
		);

	//
	// If the IDE extension has a content encryption buffer,
	// LSS does not need a encryption buffer.
	//

	if(IdeExt->CntEcrBufferLength &&IdeExt->CntEcrBuffer) {
		LssEncBuff = FALSE;
	} else {
		LssEncBuff = TRUE;
	}

	status = LspReconnect(IdeExt->LanScsiSession, NULL, NULL, &shortTimeout);

	if(!NT_SUCCESS(status)) {
		// Connect failed.
		LurnRecordFault(Lurn, LURN_ERR_CONNECT, status, NULL);
		KDPrintM(DBG_LURN_ERROR,("failed.\n"));
		return status;
	}

	status = LspRelogin(IdeExt->LanScsiSession, LssEncBuff, &shortTimeout);

	if(!NT_SUCCESS(status)) {
		LurnRecordFault(Lurn, LURN_ERR_LOGIN, IdeExt->LanScsiSession->LastLoginError, NULL);
		//
		// Login failed.
		// 
		
		KDPrintM(DBG_LURN_ERROR,("Login failed.\n"));
	} else {
		//
		//	Reconnecting succeeded.
		//	Reset Stall counter.
		//
		ACQUIRE_SPIN_LOCK(&Lurn->SpinLock, &oldIrql);
		ASSERT(Lurn->LurnStatus == LURN_STATUS_STALL);
		Lurn->LurnStatus = LURN_STATUS_RUNNING;
		RELEASE_SPIN_LOCK(&Lurn->SpinLock, oldIrql);
		InterlockedExchange(&Lurn->NrOfStall, 0);
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

	status = LspNoOperation(IdeExt->LanScsiSession, IdeExt->LuHwData.LanscsiTargetID, &PduResponse, NULL);
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



VOID
IdeLurnClose (
	PLURELATION_NODE Lurn
	) 
{
	PLURNEXT_IDE_DEVICE	IdeDev = (PLURNEXT_IDE_DEVICE)Lurn->LurnExtension;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	if(IdeDev) {
		LMDestroy(&IdeDev->BuffLockCtl);

		if(IdeDev->CntEcrBuffer && IdeDev->CntEcrBufferLength) {
			ExFreePoolWithTag(IdeDev->CntEcrBuffer, LURNEXT_ENCBUFF_TAG);
			IdeDev->CntEcrBuffer = NULL;
		}
		if(IdeDev->WriteCheckBuffer) {
			ExFreePoolWithTag(IdeDev->WriteCheckBuffer, LURNEXT_WRITECHECK_BUFFER_POOLTAG);
			IdeDev->WriteCheckBuffer = NULL;
		}
		if(IdeDev->CntEcrKey)
			CloseCipherKey(IdeDev->CntEcrKey);
		if(IdeDev->CntEcrCipher)
			CloseCipher(IdeDev->CntEcrCipher);
	}

	if(Lurn->LurnExtension) {
		ExFreePoolWithTag(Lurn->LurnExtension, LURNEXT_POOL_TAG);
		Lurn->LurnExtension =NULL;
	}

	return;
}


