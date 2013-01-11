#include "ndasscsiproc.h"


#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "LSLurn"

//////////////////////////////////////////////////////////////////////////
//
//	Supported NDAS features based on the hardware version
//

NDAS_FEATURES	LurNdasFeatures[] = {
	// hw version 1.0
	NDASFEATURE_SECONDARY | NDASFEATURE_RO_FAKE_WRITE | NDASFEATURE_DYNAMIC_REQUEST_SIZE,

	// hw version 1.1
	NDASFEATURE_SECONDARY |	NDASFEATURE_OOB_WRITE | NDASFEATURE_RO_FAKE_WRITE |
	NDASFEATURE_SIMULTANEOUS_WRITE | NDASFEATURE_GP_LOCK | NDASFEATURE_DYNAMIC_REQUEST_SIZE |
	NDASFEATURE_BUFFER_CONTROL,

	// hw version 2.0
	NDASFEATURE_SECONDARY |	NDASFEATURE_OOB_WRITE | NDASFEATURE_RO_FAKE_WRITE | 
	NDASFEATURE_SIMULTANEOUS_WRITE | NDASFEATURE_GP_LOCK| NDASFEATURE_DYNAMIC_REQUEST_SIZE |
	NDASFEATURE_BUFFER_CONTROL,

	// hw version 2.5
	NDASFEATURE_SECONDARY |	NDASFEATURE_OOB_WRITE | NDASFEATURE_RO_FAKE_WRITE |
	NDASFEATURE_SIMULTANEOUS_WRITE | NDASFEATURE_ADV_GP_LOCK| NDASFEATURE_DYNAMIC_REQUEST_SIZE |
	NDASFEATURE_BUFFER_CONTROL
};


VOID
LurDetectSupportedNdasFeatures (
	PNDAS_FEATURES	NdasFeatures,
	UINT32			LowestHwVersion
	);

NTSTATUS
LurEnableNdasFeauresByDeviceMode (
	OUT PNDAS_FEATURES		EnabledFeatures,
	IN NDAS_FEATURES		SupportedFeatures,
	IN BOOLEAN				W2kReadOnlyPatch,
	IN NDAS_DEV_ACCESSMODE	DevMode
	);

NTSTATUS
LurEnableNdasFeaturesByUserRequest (
	OUT PNDAS_FEATURES	EnabledFeatures,
	IN NDAS_FEATURES	SupportedFeatures,
	IN ULONG			LurOptions
	); 

NTSTATUS
LurUpdateSynchrously (
	IN  PLURELATION_NODE	Lurn,
	IN  UINT16				UpdateClass,
	OUT PUCHAR				CcbStatus
	);

NTSTATUS
LurProcessWrite (
	PLURELATION	Lur,
	PCCB		WriteCommand
	);


//////////////////////////////////////////////////////////////////////////
//
// Initialize/destroy LUR system.
//

#if 0
LURELATION_GLOBALS LurGlobals;
#endif

NTSTATUS
InitializeLurSystem (
	ULONG	Flags
	)
{
	NTSTATUS	status;

	UNREFERENCED_PARAMETER( Flags );

#if DBG

	NdasScsiDebugLevel |= NDASSCSI_DBG_LUR_ERROR;
	NdasScsiDebugLevel |= NDASSCSI_DBG_LUR_INFO;

	NdasScsiDebugLevel |= NDASSCSI_DBG_LURN_NDASR_ERROR;
	NdasScsiDebugLevel |= NDASSCSI_DBG_LURN_NDASR_INFO;

	NdasScsiDebugLevel |= NDASSCSI_DBG_LURN_IDE_ERROR;
	NdasScsiDebugLevel |= NDASSCSI_DBG_LURN_IDE_INFO;

#endif

#if 0
	KeInitializeEvent( &LurGlobals.LurSystemSynchEvent, SynchronizationEvent, TRUE );
	LurGlobals.LurCount = 0;
#endif

	status = NdasRaidStart( &NdasrGlobalData );

	return status;
}

VOID
DestroyLurSystem (VOID) {

	NdasRaidClose( &NdasrGlobalData );
}


//////////////////////////////////////////////////////////////////////////
//
//	LU Relation
//



// Create an LUR with LUR descriptor.


NTSTATUS
LurCreate (
	IN  PLURELATION_DESC	LurDesc2,
	IN  BOOLEAN				EnableSecondaryOnly,
	IN  BOOLEAN				EnableW2kReadOnlyPacth,
	IN  PDEVICE_OBJECT		AdapterFunctionDeviceObject,
	IN  PVOID				AdapterHardwareExtension,
	IN  LURN_EVENT_CALLBACK	LurnEventCallback,
	OUT PLURELATION			*Lur
	)
{
	NTSTATUS				status;

	PLURELATION				tmpLur;
	ULONG					tmpLurLength;

	PLURELATION_NODE		tmpLurn[LUR_MAX_LURNS_PER_LUR];

	PLURELATION_NODE_DESC	currentLurnDesc;

	LONG					idx_lurn;
	LONG					idx_child;

	ULONG					child;
	BOOLEAN					writeCheckRequired = FALSE;
	ULONG					lowestHwVersion;
	

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	//return STATUS_INSUFFICIENT_RESOURCES;

	if (LurDesc2->LurnDescCount <= 0 || LurDesc2->LurnDescCount > LUR_MAX_LURNS_PER_LUR) {
	
		NDAS_ASSERT( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

#if 0
	// Synchronize the initialization process.
	// Acquire the initialization lock only if the first initialization does not occur.

	KeWaitForSingleObject( &LurGlobals.LurSystemSynchEvent, Executive, KernelMode, FALSE, NULL );
#endif

#if __ENABLE_CONTENTENCRYPT_AES_TEST__
	InitialEcrKey( LurDesc );
#endif

	//	create LURELATION
	
	tmpLurLength = sizeof(LURELATION) + 
				   sizeof(PLURELATION_NODE) * (LurDesc2->LurnDescCount - 1) + 
				   LurDesc2->Length;
	
	tmpLur = (PLURELATION)ExAllocatePoolWithTag( NonPagedPool, tmpLurLength, LUR_POOL_TAG );
	
	if (!tmpLur) {

		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
#if 0
		KeSetEvent( &LurGlobals.LurSystemSynchEvent, IO_NO_INCREMENT, FALSE );		
#endif
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory( tmpLur, tmpLurLength );

	tmpLur->LurDesc = (PLURELATION_DESC) ((PUCHAR)tmpLur + 
										  (sizeof(LURELATION) + 
										   sizeof(PLURELATION_NODE) * (LurDesc2->LurnDescCount - 1)));

	RtlCopyMemory( tmpLur->LurDesc, LurDesc2, LurDesc2->Length );

	tmpLur->Type					= LSSTRUC_TYPE_LUR;
	tmpLur->Length					= sizeof(LURELATION);
	tmpLur->DeviceMode				= tmpLur->LurDesc->DeviceMode;
	tmpLur->EndingBlockAddr			= tmpLur->LurDesc->EndingBlockAddr;
	tmpLur->LowestHwVer				= -1;
	tmpLur->LurFlags				= 0;
	tmpLur->SupportedNdasFeatures	= 0;
	tmpLur->EnabledNdasFeatures		= 0;

	tmpLur->UnitBlocks				= tmpLur->LurDesc->EndingBlockAddr + 1;
	tmpLur->MaxChildrenSectorCount	= tmpLur->UnitBlocks;

	tmpLur->DevType				= tmpLur->LurDesc->DevType;
	tmpLur->DevSubtype			= tmpLur->LurDesc->DevSubtype;
	tmpLur->NodeCount			= tmpLur->LurDesc->LurnDescCount;

	RtlCopyMemory( tmpLur->LurId, tmpLur->LurDesc->LurId, LURID_LENGTH );

	tmpLur->AdapterFunctionDeviceObject	= AdapterFunctionDeviceObject;
	tmpLur->AdapterHardwareExtension	= AdapterHardwareExtension;
	tmpLur->LurnEventCallback			= LurnEventCallback;

	LsuInitializeBlockAcl( &tmpLur->SystemBacl );
	LsuInitializeBlockAcl( &tmpLur->UserBacl );

	//	Block access control list
	
	if (tmpLur->LurDesc->BACLOffset) {
	
		PNDAS_BLOCK_ACL	srcBACL;

		srcBACL = (PNDAS_BLOCK_ACL)((PUCHAR)tmpLur->LurDesc + tmpLur->LurDesc->BACLOffset);
	
		ASSERT(srcBACL->Length == tmpLur->LurDesc->BACLLength);

		status = LsuConvertNdasBaclToLsuBacl( &tmpLur->SystemBacl, srcBACL );

		if (status != STATUS_SUCCESS) {

			NDAS_ASSERT( FALSE );

			ExFreePoolWithTag( tmpLur, LUR_POOL_TAG );
#if 0
			KeSetEvent(&LurGlobals.LurSystemSynchEvent, IO_NO_INCREMENT, FALSE);
#endif
			return status;
		}
	}

#if __ENABLE_BACL_TEST__
	{
		PLSU_BLOCK_ACE	blockAce;

		blockAce = LsuCreateBlockAce(LSUBACE_ACCESS_READ, 0x3f, 0x3f);
		if(blockAce) {
			LsuInsertAce(&tmpLur->SystemBacl, blockAce);
		}
	}
#endif

	//	Content encryption
	
	tmpLur->CntEcrMethod		= tmpLur->LurDesc->CntEcrMethod;
	tmpLur->CntEcrKeyLength		= tmpLur->LurDesc->CntEcrKeyLength;
	
	if (tmpLur->LurDesc->CntEcrKeyLength) {
	
		RtlCopyMemory(tmpLur->CntEcrKey, tmpLur->LurDesc->CntEcrKey, tmpLur->CntEcrKeyLength);

#if DBG
		{
			ULONG	keyIdx;
			DebugTrace(DBG_LURN_INFO, ("Encryption key method:%02x, key length:%d * 4bytes, ",
												tmpLur->LurDesc->CntEcrMethod,
												(int)tmpLur->LurDesc->CntEcrKeyLength)
												);
			for(keyIdx = 0; keyIdx<tmpLur->CntEcrKeyLength; keyIdx++) {
				DebugTrace(DBG_LURN_INFO, ("%02x ", (int)tmpLur->CntEcrKey[keyIdx]));
				if((keyIdx%4) == 0)
					DebugTrace(DBG_LURN_INFO, (" "));
			}
			DebugTrace(DBG_LURN_INFO, ("\n"));
		}
#endif

	}

	status = NdasRaidStart( &NdasrGlobalData );

	if (status != STATUS_SUCCESS) {

		goto error_out;
	}

	//	Allocate LURNs to LURN descriptor Table and sanity check.

	RtlZeroMemory( tmpLurn, sizeof(PLURELATION_NODE) * LUR_MAX_LURNS_PER_LUR );

	lowestHwVersion = -1;
	currentLurnDesc = tmpLur->LurDesc->LurnDesc;

	for (idx_lurn = 0; idx_lurn < (LONG)tmpLur->LurDesc->LurnDescCount; idx_lurn++) {

		DebugTrace( NDASSCSI_DBG_LUR_INFO, 
					("Idx:%d LurnDesc:%p NextOffset:%d\n", 
					idx_lurn, currentLurnDesc, currentLurnDesc->NextOffset) );

		// Save the lowest hardware version from leaf nodes.
		
		if (currentLurnDesc->LurnChildrenCnt == 0) {

			if (lowestHwVersion > currentLurnDesc->LurnIde.HWVersion) {
			
				lowestHwVersion = currentLurnDesc->LurnIde.HWVersion;
			}
		}
		
		if (currentLurnDesc->NextOffset == 0 && (LONG)idx_lurn + 1 < (LONG)tmpLur->LurDesc->LurnDescCount) {
	
			NDAS_ASSERT( FALSE );

			status = STATUS_INVALID_PARAMETER;
		}

		if (currentLurnDesc->LurnType < 0 || currentLurnDesc->LurnType >= LurnInterfaceCnt) {

			NDAS_ASSERT( FALSE );

			status = STATUS_INVALID_PARAMETER;
			break;
		}

		status = LurnAllocate( &tmpLurn[idx_lurn], currentLurnDesc->LurnChildrenCnt );
		
		if (status != STATUS_SUCCESS) {
		
			NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
			break;
		}

		//	Also initialize LURN index in the LUR.
		
		tmpLurn[idx_lurn]->LurnStatus = LURN_STATUS_INIT;
		tmpLur->Nodes[idx_lurn] = tmpLurn[idx_lurn];

		tmpLurn[idx_lurn]->LurnDesc = currentLurnDesc;

		currentLurnDesc = (PLURELATION_NODE_DESC)((PBYTE)tmpLur->LurDesc + currentLurnDesc->NextOffset);
	}

	if (status != STATUS_SUCCESS) {

		goto error_out;
	}

	if (lowestHwVersion > LANSCSIIDE_CURRENT_VERSION) {

		NDAS_ASSERT( FALSE );

		status = STATUS_INVALID_PARAMETER;
		goto error_out;
	}

	//	Detect supported NDAS features based on the lowest hardware version
	//	We can only support minimum of all child hardware for RAID.

	LurDetectSupportedNdasFeatures( &tmpLur->SupportedNdasFeatures, lowestHwVersion );

	//	Enable features to default values or values set by a user.
	//	User requests overrides the default features.

	status = LurEnableNdasFeauresByDeviceMode( &tmpLur->EnabledNdasFeatures,
											   tmpLur->SupportedNdasFeatures,
											   EnableW2kReadOnlyPacth,
											   tmpLur->DeviceMode );

	if (status != STATUS_SUCCESS) {

		NDAS_ASSERT( FALSE );
		goto error_out;
	}

	status = LurEnableNdasFeaturesByUserRequest( &tmpLur->EnabledNdasFeatures,
												 tmpLur->SupportedNdasFeatures,
												 tmpLur->LurDesc->LurOptions );

	if (status != STATUS_SUCCESS) {

		NDAS_ASSERT( FALSE );
		goto error_out;
	}

	DebugTrace( NDASSCSI_DBG_LUR_INFO, 
				("LurOptions=%08lx Supported features=%08lx Enabled features=%08lx tmpLur->DeviceMode = %x\n",
				tmpLur->LurDesc->LurOptions, tmpLur->SupportedNdasFeatures, tmpLur->EnabledNdasFeatures, tmpLur->DeviceMode) );

	//	Build tree. Set up LURN.
	//	Initialize nodes from leaves to a root.
	//	Node 0 is root.

	status = STATUS_SUCCESS;
	
	for (idx_lurn = tmpLur->LurDesc->LurnDescCount - 1; idx_lurn >= 0; idx_lurn--) {

		//	Set LURN IDs of itself, parent,and  children.
		//	LURN ID must be the index number in LUR->Nodes[] array.
		
		tmpLurn[idx_lurn]->LurnId			= tmpLurn[idx_lurn]->LurnDesc->LurnId;

		if (tmpLurn[idx_lurn]->LurnDesc->LurnParent == LURN_ROOT_INDEX) {

			tmpLurn[idx_lurn]->LurnParent = tmpLurn[tmpLurn[idx_lurn]->LurnDesc->LurnParent];
		
		} else {

			tmpLurn[idx_lurn]->LurnParent = tmpLurn[tmpLurn[idx_lurn]->LurnDesc->LurnParent];
		}

		// Children
	
		tmpLurn[idx_lurn]->LurnChildrenCnt	= tmpLurn[idx_lurn]->LurnDesc->LurnChildrenCnt;
		
		status = STATUS_SUCCESS;

		for (idx_child = 0; idx_child < (LONG)tmpLurn[idx_lurn]->LurnDesc->LurnChildrenCnt; idx_child++) {
		
			child = tmpLurn[idx_lurn]->LurnDesc->LurnChildren[idx_child];
			
			if (child > tmpLur->LurDesc->LurnDescCount) {
				
				NDAS_ASSERT( FALSE );

				DebugTrace( NDASSCSI_DBG_LUR_INFO, ("invalid child number.\n") );

				status = STATUS_INVALID_PARAMETER;
				break;
			}

			tmpLurn[idx_lurn]->LurnChildren[idx_child] = tmpLurn[child];
			tmpLurn[child]->LurnChildIdx = idx_child;
		}

		if (status != STATUS_SUCCESS) {

			break;
		}

		//	LUR Node's access right
		
		tmpLurn[idx_lurn]->LurnDesc->AccessRight = 0;

		if (tmpLur->DeviceMode == DEVMODE_SHARED_READONLY) {
		
			tmpLurn[idx_lurn]->LurnDesc->AccessRight = GENERIC_READ;

		} else if (tmpLur->DeviceMode == DEVMODE_SHARED_READWRITE) {

			if (tmpLur->LurDesc->LurOptions & LUROPTION_DONOT_ADJUST_PRIMSEC_ROLE) {

				if (EnableSecondaryOnly) {

					tmpLurn[idx_lurn]->LurnDesc->AccessRight = GENERIC_READ;
					tmpLur->EnabledNdasFeatures |= NDASFEATURE_SECONDARY;
				
				} else {

					tmpLurn[idx_lurn]->LurnDesc->AccessRight = GENERIC_WRITE | GENERIC_READ;
				}
			
			} else {

				tmpLur->EnabledNdasFeatures |= NDASFEATURE_SECONDARY;
				tmpLurn[idx_lurn]->LurnDesc->AccessRight = GENERIC_READ;
			}

		} else {

			tmpLurn[idx_lurn]->LurnDesc->AccessRight = GENERIC_WRITE | GENERIC_READ;
		}

		// LUR node options
		
		if ((tmpLur->EnabledNdasFeatures & NDASFEATURE_BUFFER_CONTROL) == 0) {

			tmpLurn[idx_lurn]->LurnDesc->LurnOptions |= LURNOPTION_NO_BUFF_CONTROL;
		}

		if (tmpLurn[idx_lurn]->LurnDesc->LurnOptions & LURNOPTION_RESTRICT_UDMA) {
		
			tmpLurn[idx_lurn]->UDMARestrictValid = TRUE;
			tmpLurn[idx_lurn]->UDMARestrict = tmpLurn[idx_lurn]->LurnDesc->UDMARestrict;

		} else {

			tmpLurn[idx_lurn]->UDMARestrictValid = FALSE;
		}

		if (LURN_IDE_DISK == tmpLurn[idx_lurn]->LurnDesc->LurnType && 
			LANSCSIIDE_VERSION_2_0 == tmpLurn[idx_lurn]->LurnDesc->LurnIde.HWVersion) {
		
			writeCheckRequired = TRUE;
		}

		//	Initialize the LURN

		status = LurnCreate( tmpLur, tmpLurn[idx_lurn] );

		if (status != STATUS_SUCCESS) {

			if (tmpLurn[idx_lurn]->LurnDesc->LurnChildrenCnt != 0) {

				NDAS_ASSERT( NDAS_ASSERT_NODE_UNRECHABLE );

				DebugTrace( NDASSCSI_DBG_LUR_INFO, 
							("LurnCreate() failed. LURN#:%d NTSTATUS:%08lx\n", idx_lurn, status) );
		
				break;
			}
		}
	}

error_out:

	if (status != STATUS_SUCCESS) {

		for (idx_lurn = 0; (ULONG)idx_lurn < tmpLur->LurDesc->LurnDescCount; idx_lurn++) {
	
			if (tmpLurn[idx_lurn]) {

				DebugTrace( NDASSCSI_DBG_LUR_INFO, 
							("Freeing LURN:%p(%d)\n", tmpLurn[idx_lurn], idx_lurn) );

				if (LURN_IS_RUNNING(tmpLurn[idx_lurn]->LurnStatus)) {

					LurnSendStopCcb( tmpLurn[idx_lurn] );
					LurnClose( tmpLurn[idx_lurn] );
				}

				LurnFree( tmpLurn[idx_lurn] );

				tmpLurn[idx_lurn] = NULL;
			}
		}

		if (tmpLur) {

			LsuFreeAllBlockAce( &tmpLur->UserBacl.BlockAclHead );
			LsuFreeAllBlockAce( &tmpLur->SystemBacl.BlockAclHead );
			ExFreePoolWithTag( tmpLur, LUR_POOL_TAG );
		}

		NdasRaidClose( &NdasrGlobalData );

#if 0
		// Release synch lock.	
		KeSetEvent(&LurGlobals.LurSystemSynchEvent, IO_NO_INCREMENT, FALSE);
#endif

		NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

		return status;
	}

	if (tmpLur->DeviceMode == DEVMODE_SHARED_READWRITE) {

		if (EnableSecondaryOnly) {
		
		} else if (tmpLur->LurDesc->LurOptions & LUROPTION_DONOT_ADJUST_PRIMSEC_ROLE) {
			
			DebugTrace( NDASSCSI_DBG_LUR_INFO, 
						("DONOT_ADJUST_ACCESSMODE is set. Can not go to Secondary mode.\n") );

		} else {

			UCHAR	ccbStatus;

			status = LurUpdateSynchrously( tmpLurn[0], 
										   LURN_UPDATECLASS_WRITEACCESS_USERID,
										   &ccbStatus );

			DebugTrace( NDASSCSI_DBG_LUR_INFO,
						("LurUpdateSynchrously status = %x, ccbStatus = %x", status, ccbStatus) );
		}
	}

	// Indicate Win2K read-only patch
	
	if (EnableW2kReadOnlyPacth) {

		tmpLur->LurFlags |= LURFLAG_W2K_READONLY_PATCH;
	}

	if (writeCheckRequired) {

		if (tmpLur->LurDesc->LurOptions & LUROPTION_ON_NDAS_2_0_WRITE_CHECK) {
		
			tmpLur->LurFlags |= LURFLAG_WRITE_CHECK_REQUIRED;

		} else if (tmpLur->LurDesc->LurOptions & LUROPTION_OFF_NDAS_2_0_WRITE_CHECK) {

			tmpLur->LurFlags &= ~LURFLAG_WRITE_CHECK_REQUIRED;

		} else {

			// Default on.

			tmpLur->LurFlags |= LURFLAG_WRITE_CHECK_REQUIRED;
		}
	}

#if 0
	// Increase LUR count and release synchronization lock.	
	globalLurCount = InterlockedIncrement( &LurGlobals.LurCount );

	KeSetEvent( &LurGlobals.LurSystemSynchEvent, IO_NO_INCREMENT, FALSE );
#endif

	// set Lur

	RtlCopyMemory( LurDesc2, tmpLur->LurDesc, LurDesc2->Length );
	
	*Lur = tmpLur;

	return STATUS_SUCCESS;	
}

static
VOID
LurCloseRecur (
	PLURELATION_NODE	Lurn
	) 
{
	ULONG	idx_lurn;

	LurnClose( Lurn );

	for( idx_lurn = 0; idx_lurn < Lurn->LurnChildrenCnt; idx_lurn++ ) {

		LurCloseRecur( Lurn->LurnChildren[idx_lurn] );
	}

	LurnFree( Lurn );
}

VOID
LurClose (
	PLURELATION	Lur
	) 
{
	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

#if 0
	KeWaitForSingleObject(&LurGlobals.LurSystemSynchEvent, Executive, KernelMode, FALSE, NULL);
#endif

	if (LUR_GETROOTNODE(Lur) == NULL) {

		DebugTrace( DBG_LURN_INFO, ("No LURN.\n") );
	
	} else {
	
		LurCloseRecur( LUR_GETROOTNODE(Lur) );
	}

	LsuFreeAllBlockAce( &Lur->UserBacl.BlockAclHead );
	LsuFreeAllBlockAce( &Lur->SystemBacl.BlockAclHead );

	NdasRaidClose( &NdasrGlobalData );

	ExFreePoolWithTag( Lur, LUR_POOL_TAG );

#if 0
	globalLurCount = InterlockedDecrement(&LurGlobals.LurCount);
	ASSERT( globalLurCount >= 0 );
	KeSetEvent(&LurGlobals.LurSystemSynchEvent, IO_NO_INCREMENT, FALSE);
#endif
}

VOID
LurStop (
	PLURELATION	Lur
	) 
{
	if (!Lur || Lur->NodeCount <= 0) {

		NDAS_ASSERT( FALSE );

		return;
	}

	LurnStop( LUR_GETROOTNODE(Lur) );	

	return;
}

NTSTATUS
LurRequest (
	PLURELATION	Lur,
	PCCB		Ccb
	) 
{
	NTSTATUS	status;

	if (!Lur || Lur->NodeCount <= 0) {

		NDAS_ASSERT( FALSE );

		return STATUS_INVALID_PARAMETER;
	}

	//
	//	Increase stack location
	//

	NDAS_ASSERT( Ccb->CcbCurrentStackLocation == &Ccb->CcbStackLocation[Ccb->CcbCurrentStackLocationIndex] );

	Ccb->CcbCurrentStackLocation--;
	Ccb->CcbCurrentStackLocationIndex--;
	Ccb->CcbCurrentStackLocation->Lurn = Lur;
	
	//	LUR's stack location should be on the second place.
	
	NDAS_ASSERT( Ccb->CcbCurrentStackLocationIndex == NR_MAX_CCB_STACKLOCATION - 2 );

	if (Ccb->CcbCurrentStackLocationIndex < 0) {

		NDAS_ASSERT( FALSE );

		return STATUS_DATA_OVERRUN;
	}

	if (Ccb->OperationCode == CCB_OPCODE_UPDATE) {

		PLURN_UPDATE	LurnUpdate;

		if (!LUR_IS_PRIMARY(Lur) && !LUR_IS_SECONDARY(Lur)) {
		
			Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
			LsCcbCompleteCcb( Ccb );
			return STATUS_SUCCESS;
		};

		LurnUpdate = (PLURN_UPDATE)Ccb->DataBuffer;

		switch(LurnUpdate->UpdateClass) {

		case LURN_UPDATECLASS_WRITEACCESS_USERID: {

			if (LUR_IS_PRIMARY(Lur)) {

				Ccb->CcbStatus = CCB_STATUS_SUCCESS;
				LsCcbCompleteCcb( Ccb );

				return STATUS_SUCCESS;
			}

			break;
		}

		case LURN_UPDATECLASS_READONLYACCESS: {

			if (LUR_IS_SECONDARY(Lur)) {

				Ccb->CcbStatus = CCB_STATUS_SUCCESS;
				LsCcbCompleteCcb( Ccb );

				return STATUS_SUCCESS;
			}

			break;
		}

		default:

			Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
			LsCcbCompleteCcb( Ccb );
			return STATUS_SUCCESS;
		}
	}

	//	Access check

	if (Ccb->OperationCode == CCB_OPCODE_EXECUTE) {

		switch(Ccb->Cdb[0]) {
	
		case SCSIOP_WRITE:
		case SCSIOP_WRITE16:

			if (LUR_GETROOTNODE(Lur)->LurnType == LURN_IDE_ODD) {

				break;
			}

			status = LurProcessWrite( Lur, Ccb );

			return status;

		case 0x3E:		// READ_LONG
		case SCSIOP_READ:
		case SCSIOP_READ16:

			//	Check to see if the buffer lock required.
			
#if __ENABLE_LOCKED_READ__

			if (FlagOn(Lur->EnabledNdasFeatures, NDASFEATURE_SIMULTANEOUS_WRITE)) {
				
				LsCcbSetFlag( Ccb, CCB_FLAG_ACQUIRE_BUFLOCK );

			} else {

				LsCcbResetFlag( Ccb, CCB_FLAG_ACQUIRE_BUFLOCK );
			}
#else
			LsCcbResetFlag( Ccb, CCB_FLAG_ACQUIRE_BUFLOCK );
#endif
			break;

			case SCSIOP_MODE_SENSE:
			
				//	Check to see if the mode sense,
				//	LUR node should perform win2k read-only patch
			
				if (Lur->LurFlags & LURFLAG_W2K_READONLY_PATCH) {
			
					LsCcbSetFlag( Ccb, CCB_FLAG_W2K_READONLY_PATCH );
				}

				//	Check to see if the mode sense go through the secondary(read-only) LUR node
				//	If it does, set ALLOW_WRITE_FLAG
				
				if (Lur->EnabledNdasFeatures & NDASFEATURE_SECONDARY) {
				
					LsCcbSetFlag( Ccb, CCB_FLAG_ALLOW_WRITE_IN_RO_ACCESS );
				}

			break;

			default: 

				break;
		}
	}

	return LurnRequest( LUR_GETROOTNODE(Lur), Ccb );
}


NTSTATUS
LurnUpdateSynchrouslyCompletionRoutine (
	IN PCCB		Ccb,
	IN PKEVENT	Event
	)
{
	UNREFERENCED_PARAMETER( Ccb );

	KeSetEvent( Event, 0, FALSE );

	return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
LurUpdateSynchrously (
	IN  PLURELATION_NODE	Lurn,
	IN  UINT16				UpdateClass,
	OUT PUCHAR				CcbStatus
	)
{
	NTSTATUS			status;

	CCB					ccb;
	LURN_UPDATE			lurnUpdate;
	KEVENT				completionEvent;

	LARGE_INTEGER		interval;

retry:

	LSCCB_INITIALIZE( &ccb );

	LsCcbSetFlag( &ccb, CCB_FLAG_CALLED_INTERNEL );

	ccb.OperationCode = CCB_OPCODE_UPDATE;
	ccb.DataBuffer = &lurnUpdate;
	ccb.DataBufferLength = sizeof(LURN_UPDATE);		

	RtlZeroMemory( &lurnUpdate, sizeof(LURN_UPDATE) );

	lurnUpdate.UpdateClass = UpdateClass;

	KeInitializeEvent( &completionEvent, SynchronizationEvent, FALSE );

	LsCcbSetCompletionRoutine( &ccb, 
							   LurnUpdateSynchrouslyCompletionRoutine, 
							   &completionEvent );

	status = LurnRequest( Lurn, &ccb );
		
	DebugTrace( DBG_LURN_NOISE, ("LurnRequest status : %08x\n", status) );

	if (!NT_SUCCESS(status)) {

		NDAS_ASSERT( FALSE );
	}

	if (status == STATUS_PENDING) {

		status = KeWaitForSingleObject( &completionEvent,
										Executive,
										KernelMode,
										FALSE,
										NULL );

		NDAS_ASSERT( status == STATUS_SUCCESS );
	}

	if (status != STATUS_SUCCESS) {

		NDAS_ASSERT( FALSE );

		DebugTrace( NDASSCSI_DBG_LUR_INFO, ("Failed : Status %08lx\n", status) );
		status = STATUS_CLUSTER_NODE_UNREACHABLE;

		return status;
	
	} 
	
	if (ccb.CcbStatus == CCB_STATUS_SUCCESS ||
		ccb.OperationCode == CCB_OPCODE_UPDATE && ccb.CcbStatus == CCB_STATUS_NO_ACCESS) {

		if (CcbStatus) {

			*CcbStatus = ccb.CcbStatus;
		}

		return status;
	}

	if (ccb.CcbStatus == CCB_STATUS_BUSY) {
		
		interval.QuadPart = (-5*NANO100_PER_SEC);      //delay 5 seconds
		KeDelayExecutionThread( KernelMode, FALSE, &interval );

		goto retry;
	}

	if (ccb.CcbStatus == CCB_STATUS_RESET) {

		NDAS_ASSERT( ccb.OperationCode != CCB_OPCODE_RESETBUS );

		interval.QuadPart = (-5*NANO100_PER_SEC);      //delay 5 seconds
		KeDelayExecutionThread( KernelMode, FALSE, &interval );

		goto retry;		
	}

	if (ccb.OperationCode == CCB_OPCODE_DEVLOCK && ccb.CcbStatus == CCB_STATUS_LOST_LOCK) {
		
		DebugTrace( NDASSCSI_DBG_LUR_INFO, ("ccb.CcbStatus = %x\n", ccb.CcbStatus) );
		
		status = STATUS_LOCK_NOT_GRANTED;
	
	} else {

		DebugTrace( NDASSCSI_DBG_LUR_INFO, ("ccb.CcbStatus = %x\n", ccb.CcbStatus) );

		if (ccb.CcbStatus != CCB_STATUS_STOP															&&
			!(ccb.OperationCode == CCB_OPCODE_DEVLOCK && ccb.CcbStatus == CCB_STATUS_COMMAND_FAILED)	&&
			ccb.CcbStatus != CCB_STATUS_NOT_EXIST) {
		
			DebugTrace( NDASSCSI_DBG_LUR_INFO, ("ccb.CcbStatus = %x\n", ccb.CcbStatus) );
			NDAS_ASSERT( FALSE );
		}

		status = STATUS_CLUSTER_NODE_UNREACHABLE;
	}

	if (CcbStatus) {

		*CcbStatus = ccb.CcbStatus;
	}

	return status;
}

// internal functions

//
//	Send Stop Ccb to the LURN.
//
NTSTATUS
LurnSendStopCcb(
		PLURELATION_NODE	Lurn
	) {
	PCCB		ccb;
	NTSTATUS	ntStatus;

	KDPrintM(DBG_LURN_TRACE,("entered.\n"));
	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	ntStatus = LsCcbAllocate(&ccb);
	if(!NT_SUCCESS(ntStatus)) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	LSCCB_INITIALIZE(ccb);
	ccb->OperationCode				= CCB_OPCODE_STOP;
	ccb->HwDeviceExtension			= NULL;
	LsCcbSetFlag(ccb, CCB_FLAG_SYNCHRONOUS|CCB_FLAG_ALLOCATED|CCB_FLAG_DONOT_PASSDOWN);
	LsCcbSetCompletionRoutine(ccb, NULL, NULL);

	//
	//	Send a CCB to the LURN.
	//
	ntStatus = LurnRequest(
					Lurn,
					ccb
				);
	if(!NT_SUCCESS(ntStatus)) {
        LsCcbFree(ccb);
	}

	return ntStatus;
}


//
// Retrieve NDAS feature according to NDAS chip version.
//


VOID
LurDetectSupportedNdasFeatures(
	PNDAS_FEATURES	NdasFeatures,
	UINT32			LowestHwVersion
){
	ASSERT(LowestHwVersion <= LANSCSIIDE_CURRENT_VERSION);

	*NdasFeatures |= LurNdasFeatures[LowestHwVersion];

	//
	//	NDAS device must support secondary feature.
	//

	ASSERT(*NdasFeatures & NDASFEATURE_SECONDARY);
}

//
// set/reset NDAS feature macros
//

#define _LUR_SET_FEATURE(SUPPORTED_FEAT, ENABLED_FEAT, NDAS_FEATURE) { \
	if((SUPPORTED_FEAT)&(NDAS_FEATURE)) {	\
		(ENABLED_FEAT) |= (NDAS_FEATURE);	\
	} else { \
		return STATUS_UNSUCCESSFUL;	\
	} \
}

#define _LUR_SET_OPTIONAL_FEATURE(SUPPORTED_FEAT, ENABLED_FEAT, NDAS_FEATURE) { \
	if((SUPPORTED_FEAT)&(NDAS_FEATURE)) {	\
		(ENABLED_FEAT) |= (NDAS_FEATURE);	\
	} else { \
		KDPrintM(DBG_LURN_ERROR, ((#NDAS_FEATURE " not supported.\n")));	\
	} \
}

#define _LUR_RESET_FEATURE(SUPPORTED_FEAT, ENABLED_FEAT, NDAS_FEATURE) { \
	if((SUPPORTED_FEAT)&(NDAS_FEATURE)) {	\
		(ENABLED_FEAT) &= ~(NDAS_FEATURE);	\
	} else { \
		return STATUS_UNSUCCESSFUL;	\
	} \
}

#define _LUR_RESET_OPTIONAL_FEATURE(SUPPORTED_FEAT, ENABLED_FEAT, NDAS_FEATURE) { \
	if((SUPPORTED_FEAT)&(NDAS_FEATURE)) {	\
		(ENABLED_FEAT) &= ~(NDAS_FEATURE);	\
	} else { \
		KDPrintM(DBG_LURN_ERROR, ((#NDAS_FEATURE " not supported.\n")));	\
	} \
}

//
// Enable NDAS features according to device access mode.
//


NTSTATUS
LurEnableNdasFeauresByDeviceMode(
	OUT PNDAS_FEATURES		EnabledFeatures,
	IN NDAS_FEATURES		SupportedFeatures,
	IN BOOLEAN				W2kReadOnlyPatch,
	IN NDAS_DEV_ACCESSMODE	DevMode
){
	//
	//	Default features
	//
	//	NDAS chip 1.0 does not support SIMULTANEOUS_WRITE, ADV_GP_LOCK, and GP_LOCK.
	//	NDAS chip 1.1 and 2.0 does not support ADV_GP_LOCK.
	//

	_LUR_SET_OPTIONAL_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_GP_LOCK);
	_LUR_SET_OPTIONAL_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_ADV_GP_LOCK);

	// User request can override features following.
//	_LUR_SET_OPTIONAL_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_WRITE_CHECK);
	_LUR_SET_OPTIONAL_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_DYNAMIC_REQUEST_SIZE);
	_LUR_SET_OPTIONAL_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_BUFFER_CONTROL);

	switch(DevMode) {
		case DEVMODE_SHARED_READONLY:
			if(W2kReadOnlyPatch) {
				_LUR_SET_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_RO_FAKE_WRITE);
			}
			break;

		case DEVMODE_SHARED_READWRITE:
			_LUR_SET_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_RO_FAKE_WRITE);
	        // User request can override features following.
	        _LUR_SET_OPTIONAL_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_SIMULTANEOUS_WRITE);
			break;

		case DEVMODE_EXCLUSIVE_READWRITE:
			// Nothing to do
			break;

		default: ;
			ASSERT(FALSE);
			return STATUS_UNSUCCESSFUL;
	}

	return STATUS_SUCCESS;
}

//
// Enable NDAS feature according to user request (LUR options)
//


NTSTATUS
LurEnableNdasFeaturesByUserRequest(
	OUT PNDAS_FEATURES		EnabledFeatures,
	IN NDAS_FEATURES		SupportedFeatures,
	IN ULONG				LurOptions
) {

	//
	//	Shared out-of-band write mode
	//

	if(	(LurOptions & LUROPTION_OFF_OOB_WRITE) &&
		(LurOptions & LUROPTION_ON_OOB_WRITE) ) {
			return STATUS_UNSUCCESSFUL;
	}

	if(LurOptions & LUROPTION_OFF_OOB_WRITE) {
		_LUR_RESET_OPTIONAL_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_OOB_WRITE);
	} else if(LurOptions & LUROPTION_ON_OOB_WRITE) {
		_LUR_SET_OPTIONAL_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_OOB_WRITE);
	}

	//
	//	Simultaneous write
	//

	if(	(LurOptions & LUROPTION_ON_SIMULTANEOUS_WRITE) &&
		(LurOptions & LUROPTION_OFF_SIMULTANEOUS_WRITE) ) {
			return STATUS_UNSUCCESSFUL;
	}

	if(LurOptions & LUROPTION_OFF_SIMULTANEOUS_WRITE) {
		_LUR_RESET_OPTIONAL_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_SIMULTANEOUS_WRITE);
	} else  if(LurOptions & LUROPTION_ON_SIMULTANEOUS_WRITE) {
		_LUR_SET_OPTIONAL_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_SIMULTANEOUS_WRITE);
	}

#if 0
	//
	//	Write check
	//

	if(	(LurOptions & LUROPTION_OFF_NDAS_2_0_WRITE_CHECK) &&
		(LurOptions & LUROPTION_ON_NDAS_2_0_WRITE_CHECK) ) {
			return STATUS_UNSUCCESSFUL;
	}
	if(LurOptions & LUROPTION_OFF_NDAS_2_0_WRITE_CHECK) {
		_LUR_RESET_OPTIONAL_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_WRITE_CHECK);
	} else if(LurOptions & LUROPTION_ON_NDAS_2_0_WRITE_CHECK) {
		_LUR_SET_OPTIONAL_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_WRITE_CHECK);
	}
#endif
	if(	(LurOptions & LUROPTION_ON_DYNAMIC_REQUEST_SIZE) &&
		(LurOptions & LUROPTION_OFF_DYNAMIC_REQUEST_SIZE) ) {
			return STATUS_UNSUCCESSFUL;
	}
	if( LurOptions & LUROPTION_OFF_DYNAMIC_REQUEST_SIZE) {
		_LUR_RESET_OPTIONAL_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_DYNAMIC_REQUEST_SIZE);
	} else if(LurOptions & LUROPTION_ON_DYNAMIC_REQUEST_SIZE) {
		_LUR_SET_OPTIONAL_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_DYNAMIC_REQUEST_SIZE);
	}

	if(	(LurOptions & LUROPTION_ON_BUFFER_CONTROL) &&
		(LurOptions & LUROPTION_OFF_BUFFER_CONTROL) ) {
			return STATUS_UNSUCCESSFUL;
	}
	if( LurOptions & LUROPTION_OFF_BUFFER_CONTROL) {
		_LUR_RESET_OPTIONAL_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_BUFFER_CONTROL);
	} else if(LurOptions & LUROPTION_ON_BUFFER_CONTROL) {
		_LUR_SET_OPTIONAL_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_BUFFER_CONTROL);
	}

	return STATUS_SUCCESS;
}

#if __ENABLE_CONTENTENCRYPT_AES_TEST__

VOID
InitialEcrKey (
	IN PLURELATION_DESC		LurDesc
	)
{
	LurDesc->CntEcrMethod = NDAS_CONTENTENCRYPT_METHOD_AES;
	LurDesc->CntEcrKeyLength = 32;
	LurDesc->CntEcrKey[0] = 0x1a;
	LurDesc->CntEcrKey[1] = 0xc3;
	LurDesc->CntEcrKey[2] = 0x4d;
	LurDesc->CntEcrKey[3] = 0x59;
	LurDesc->CntEcrKey[4] = 0x1a;
	LurDesc->CntEcrKey[5] = 0xc1;
	LurDesc->CntEcrKey[6] = 0xad;
	LurDesc->CntEcrKey[7] = 0x5d;
	LurDesc->CntEcrKey[8] = 0xca;
	LurDesc->CntEcrKey[9] = 0xcd;
	LurDesc->CntEcrKey[10] = 0x8d;
	LurDesc->CntEcrKey[11] = 0x50;
	LurDesc->CntEcrKey[12] = 0x12;
	LurDesc->CntEcrKey[13] = 0xc1;
	LurDesc->CntEcrKey[14] = 0xfd;
	LurDesc->CntEcrKey[15] = 0x5e;
	LurDesc->CntEcrKey[16] = 0x1a;
	LurDesc->CntEcrKey[17] = 0x33;
	LurDesc->CntEcrKey[18] = 0x43;
	LurDesc->CntEcrKey[19] = 0x59;
	LurDesc->CntEcrKey[20] = 0x3a;
	LurDesc->CntEcrKey[21] = 0xc3;
	LurDesc->CntEcrKey[22] = 0x43;
	LurDesc->CntEcrKey[23] = 0x59;
	LurDesc->CntEcrKey[24] = 0x2a;
	LurDesc->CntEcrKey[25] = 0xc2;
	LurDesc->CntEcrKey[26] = 0x2d;
	LurDesc->CntEcrKey[27] = 0x5c;
	LurDesc->CntEcrKey[28] = 0x9a;
	LurDesc->CntEcrKey[29] = 0xc0;
	LurDesc->CntEcrKey[30] = 0xdd;
	LurDesc->CntEcrKey[31] = 0x5a;

	return;
}

#endif


//////////////////////////////////////////////////////////////////////////
//
//	Process a CCB to an LUR
//

NTSTATUS
FilterWriteByBACL(
	PLURELATION		Lur,
	PLSU_BLOCK_ACL	Bacl,
	BOOLEAN			RoFakeWrite,
	UINT64			WriteAddr,
	UINT32			WriteLen,
	PCCB			WriteCommand
){
	NTSTATUS	status;
	PLSU_BLOCK_ACE bace;

	bace = LsuGetAce(Bacl, WriteAddr, WriteAddr + WriteLen - 1);
	if(bace) {

		//
		//	Read-only block area
		//

		if(!(bace->AccessMode & LSUBACE_ACCESS_WRITE)) {

			if(RoFakeWrite) {
				//
				//	Complete the write command
				//
				WriteCommand->CcbStatus = CCB_STATUS_SUCCESS;
				LsCcbCompleteCcb(WriteCommand);
				status = STATUS_SUCCESS;
			} else {
				WriteCommand->CcbStatus = CCB_STATUS_INVALID_COMMAND;
				LsCcbCompleteCcb(WriteCommand);
				status = STATUS_SUCCESS;
			}

		//
		//	Read-write block area
		//

		} else {

			//
			//	Check to see if the write might go through read-only LUR node.
			//	If it does, set ALLOW_WRITE to the CCB
			//

			if(Lur->EnabledNdasFeatures & NDASFEATURE_SECONDARY) {
				LsCcbSetFlag(WriteCommand, CCB_FLAG_ALLOW_WRITE_IN_RO_ACCESS);
			}

			status = LurnRequest(LUR_GETROOTNODE(Lur), WriteCommand);
		}
	} else {
		status = STATUS_NO_MORE_ENTRIES;
	}

	return status;
}


NTSTATUS
LurProcessWrite (
	PLURELATION	Lur,
	PCCB		WriteCommand
	)
{
	NTSTATUS	status;
	UINT64		writeAddr;
	UINT32		writeLen;

	//
	//	Check to see if the buffer lock is required.
	//

	if(Lur->EnabledNdasFeatures & NDASFEATURE_SIMULTANEOUS_WRITE){
		LsCcbSetFlag(WriteCommand, CCB_FLAG_ACQUIRE_BUFLOCK);
	} else {
		LsCcbResetFlag(WriteCommand, CCB_FLAG_ACQUIRE_BUFLOCK);
	}

	//
	//	Check to see if the write check (NDAS chip 2.0 bug patch) is required.
	//	Anyway this option is activated only when HW is 2.0 rev.0.
	//

	if (Lur->LurFlags & LURFLAG_WRITE_CHECK_REQUIRED) {
		LsCcbSetFlag(WriteCommand, CCB_FLAG_WRITE_CHECK);
	} else {
		LsCcbResetFlag(WriteCommand, CCB_FLAG_WRITE_CHECK);
	}

	//
	//	Get address and length
	//

	if(WriteCommand->CdbLength == 10) {
		writeAddr = CDB10_LOGICAL_BLOCK_BYTE((PCDB)WriteCommand->Cdb);
		writeLen = CDB10_TRANSFER_BLOCKS((PCDB)WriteCommand->Cdb);
	} else if(WriteCommand->CdbLength == 16) {
		writeAddr = CDB16_LOGICAL_BLOCK_BYTE((PCDB)WriteCommand->Cdb);
		writeLen = CDB16_TRANSFER_BLOCKS((PCDB)WriteCommand->Cdb);
	} else {

		NDAS_ASSERT( FALSE );
		return STATUS_NOT_SUPPORTED;
	}

	if (writeAddr < Lur->MaxChildrenSectorCount && (writeAddr + writeLen) > Lur->UnitBlocks ) {

		NDAS_ASSERT( FALSE );

		WriteCommand->CcbStatus = CCB_STATUS_INVALID_COMMAND;
		LsCcbCompleteCcb( WriteCommand );
		status = STATUS_SUCCESS;
	}

	do {
		//
		//	Layer 0
		//	User BACL
		//

		status = FilterWriteByBACL(
			Lur,
			&Lur->UserBacl,
			(Lur->EnabledNdasFeatures & NDASFEATURE_RO_FAKE_WRITE) != 0,
			writeAddr,
			writeLen,
			WriteCommand);
		if(status != STATUS_NO_MORE_ENTRIES) {

			//
			// If an BACE found, break here no matter what status
			// it returns. If not, go to the next layer
			//

			break;
		}

		//
		//	Layer 1
		//	out-of-bound access
		//

		if( writeAddr > Lur->EndingBlockAddr) {

			//
			//	If OOB write is neither enabled nor writable device access mode,
			//	deny it.
			//

			if(	!(Lur->EnabledNdasFeatures & NDASFEATURE_OOB_WRITE) &&
				!(Lur->DeviceMode & NDASACCRIGHT_WRITE)) {
				//
				//	Complete the write command with an error.
				//
				WriteCommand->CcbStatus = CCB_STATUS_INVALID_COMMAND;
				LsCcbCompleteCcb(WriteCommand);
				status = STATUS_SUCCESS;
				break;
			}

			//
			//	If the write command is in safety area,
			//	deny it.
			//
			if(writeAddr <= Lur->EndingBlockAddr + 256) {
				//
				//	Complete the write command with an error.
				//
				WriteCommand->CcbStatus = CCB_STATUS_INVALID_COMMAND;
				LsCcbCompleteCcb(WriteCommand);
				status = STATUS_SUCCESS;
				break;
			}
			//
			// Turn off write-check for OOB access
			//
			LsCcbResetFlag(WriteCommand, CCB_FLAG_WRITE_CHECK);			
			status = LurnRequest(LUR_GETROOTNODE(Lur), WriteCommand);
			break;
		}

		//
		//	Layer 2
		//	Device mode
		//

		if(Lur->DeviceMode == DEVMODE_SHARED_READONLY) {
			if(Lur->EnabledNdasFeatures & NDASFEATURE_RO_FAKE_WRITE) {
				//
				//	Complete the write command
				//
				KDPrint(1, ("Readonly fake write: command %p\n", WriteCommand));
				WriteCommand->CcbStatus = CCB_STATUS_SUCCESS;
				LsCcbCompleteCcb(WriteCommand);
				status = STATUS_SUCCESS;
				break;
			}
			WriteCommand->CcbStatus = CCB_STATUS_INVALID_COMMAND;
			LsCcbCompleteCcb(WriteCommand);
			status = STATUS_SUCCESS;
			break;
		} else if(Lur->DeviceMode ==  DEVMODE_SHARED_READWRITE) {

			//
			//	If secondary, return write operation without actual write.
			//	If secondary wants to write, insert writable BACE.
			//

			if(Lur->EnabledNdasFeatures & NDASFEATURE_SECONDARY) {

				//
				//	Complete the write command
				//
				KDPrint(1, ("Secondary fake write: command %p\n", WriteCommand));
				WriteCommand->CcbStatus = CCB_STATUS_SUCCESS;
				LsCcbCompleteCcb(WriteCommand);
				status = STATUS_SUCCESS;
				break;
			}
		} else if(Lur->DeviceMode == DEVMODE_EXCLUSIVE_READWRITE) {
			//
			//	Nothing to do
			//
		} else if(Lur->DeviceMode == DEVMODE_SUPER_READWRITE) {
			//
			//	Nothing to do
			//
		} else {
			//
			//	Complete the write command
			//
			WriteCommand->CcbStatus = CCB_STATUS_INVALID_COMMAND;
			LsCcbCompleteCcb(WriteCommand);
			status = STATUS_SUCCESS;
			break;
		}

		//
		//	Layer 3
		//	Disk DIB BACL
		//

		status = FilterWriteByBACL(
			Lur,
			&Lur->SystemBacl,
			(Lur->EnabledNdasFeatures & NDASFEATURE_RO_FAKE_WRITE) != 0,
			writeAddr,
			writeLen,
			WriteCommand);
		if(status == STATUS_NO_MORE_ENTRIES) {

			//
			//	No BACL filtered. Just write it.
			//

			status = LurnRequest(LUR_GETROOTNODE(Lur), WriteCommand);
		}

	} while(0);

	return status;
}



//
//	Call back to the miniport device.
//
VOID
LurCallBack(
	PLURELATION	Lur,
	PLURN_EVENT	LurnEvent
) {
	ASSERT(Lur);

	if(Lur->LurnEventCallback) {
		Lur->LurnEventCallback(Lur, LurnEvent);
	}
}


//////////////////////////////////////////////////////////////////////////
//
//	LU Relation node
//

//
//	Arrange interfaces in order matching LURN_XXXX type code.
//

UINT32			LurnInterfaceCnt	= NR_LURN_LAST_INTERFACE;


PLURN_INTERFACE	LurnInterfaceList[NR_LURN_MAX_INTERFACE] = {
					
					&LurnNdasAggregationInterface,
					NULL,					// Legacy Mirror
					&LurnIdeDiskInterface,
					&LurnIdeOddInterface,
					&LurnIdeMoInterface,
					NULL,					// Legacy RAID1
					NULL,
					&LurnNdasRaid0Interface,
					NULL,
					&LurnNdasRaid1Interface,
					&LurnNdasRaid4Interface,	// &LurnRAID4RInterface, RAID4 is temporarily disabled.
					&LurnNdasRaid5Interface,
				};

VOID
LurnSetDefaultConfiguration (
	PLURELATION_NODE	Lurn
	) 
{
	Lurn->ReconnTrial		= RECONNECTION_MAX_TRY;
	Lurn->ReconnInterval	= MAX_RECONNECTION_INTERVAL;
}


VOID
LurnModifyConfiguration (
	PLURELATION_NODE		Lurn,
	PLURELATION_NODE_DESC	LurDesc
	) 
{

	if (LurDesc->LurnOptions & LURNOPTION_SET_RECONNECTION) {

		Lurn->ReconnTrial		= LurDesc->ReconnTrial;
		Lurn->ReconnInterval	= (UINT64)LurDesc->ReconnInterval * 10000;
	}
}


NTSTATUS
LurnCreate (
	PLURELATION				Lur,
	PLURELATION_NODE		Lurn
	) 
{
	NTSTATUS	status;

	DebugTrace( NDASSCSI_DBG_LUR_INFO, ("In. Lurn : %p, Lur : %p, LurnDesc : %p\n", Lurn, Lur, Lurn->LurnDesc) );

	NDAS_ASSERT( Lurn );
	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	NDAS_ASSERT( Lurn->LurnStatus == LURN_STATUS_INIT || Lurn->LurnStatus == LURN_STATUS_STOP || Lurn->LurnStatus == LURN_STATUS_DESTROYING );

	if (Lurn->LurnDesc->LurnType >= LurnInterfaceCnt ) {

		NDAS_ASSERT( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	//	set default values.
	//	Do not zero pointers to children.

	KeInitializeSpinLock( &Lurn->SpinLock );

	Lurn->LurnType		  = Lurn->LurnDesc->LurnType;
	Lurn->Lur			  = Lur;
	Lurn->LurnChildrenCnt = Lurn->LurnDesc->LurnChildrenCnt;
	Lurn->LurnInterface   = LurnInterfaceList[Lurn->LurnDesc->LurnType];

	Lurn->StartBlockAddr  = Lurn->LurnDesc->StartBlockAddr;
	Lurn->EndBlockAddr    = Lurn->LurnDesc->EndBlockAddr;
	Lurn->UnitBlocks	  = Lurn->LurnDesc->UnitBlocks;
	Lurn->AccessRight	  = Lurn->LurnDesc->AccessRight;
		
	LurnResetFaultInfo( Lurn );
	LurnSetDefaultConfiguration( Lurn );
	LurnModifyConfiguration( Lurn, Lurn->LurnDesc );

	if (Lurn->LurnInterface == NULL) {

		NDAS_ASSERT( FALSE );
		return STATUS_NOT_IMPLEMENTED;
	}

	if (Lurn->LurnInterface->LurnFunc.LurnCreate == NULL) {

		NDAS_ASSERT( FALSE );
		return STATUS_NOT_IMPLEMENTED;
	}

#if 0
	if (Lurn->LurnDesc->LurnOptions & LURNOPTION_MISSING) {

		Lurn->LurnDesc->LurnOptions &= ~LURNOPTION_MISSING;

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, 
					("Missing LURN. Saved pointer the lurn desc:%p\n", Lurn->LurnDesc) );

		return STATUS_SUCCESS;
	}
#endif

	status = Lurn->LurnInterface->LurnFunc.LurnCreate( Lurn, Lurn->LurnDesc );

	if (NT_SUCCESS(status)) {
	
		Lurn->LurnStatus = LURN_STATUS_RUNNING;
	}

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, ("return 0x%08lx : Idx:%d\n", status, Lurn->LurnId) );

	return status;
}

VOID
LurnClose (
	PLURELATION_NODE Lurn
	) 
{
	DebugTrace( NDASSCSI_DBG_LURN_NDASR_ERROR, ("In. Lurn %p\n", Lurn) );

	NDAS_ASSERT( Lurn );

	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	if (!Lurn->LurnInterface) {
	
		NDAS_ASSERT( FALSE );
		return;
	}

	if (!Lurn->LurnInterface->LurnFunc.LurnClose) {
	
		return;
	}

	Lurn->LurnInterface->LurnFunc.LurnClose( Lurn );

	Lurn->LurnStatus = LURN_STATUS_DESTROYING;

	return;
}

VOID
LurnStop (
	PLURELATION_NODE Lurn
	)
{
	Lurn->LurnInterface->LurnFunc.LurnStop( Lurn );
}

NTSTATUS
LurnRequest (
	PLURELATION_NODE Lurn,
	PCCB Ccb
	) 
{
	NTSTATUS	status;

	NDAS_ASSERT( Ccb->CcbCurrentStackLocation == &Ccb->CcbStackLocation[Ccb->CcbCurrentStackLocationIndex] );

	if (!Lurn->LurnInterface || !Lurn->LurnInterface->LurnFunc.LurnRequest) {

		NDAS_ASSERT( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	Ccb->CcbCurrentStackLocation--;
	Ccb->CcbCurrentStackLocationIndex--;
	Ccb->CcbCurrentStackLocation->Lurn = Lurn;

	if (Ccb->CcbCurrentStackLocationIndex < 0) {

		NDAS_ASSERT( FALSE );
		return STATUS_DATA_OVERRUN;
	}

	status = Lurn->LurnInterface->LurnFunc.LurnRequest( Lurn, Ccb );

	return status;
}

NTSTATUS
LurnAllocate (
	PLURELATION_NODE	*Lurn,
	LONG				ChildrenCnt
	) 
{
	ULONG				length;
	PLURELATION_NODE	lurn;

	length = ALIGN_UP( FIELD_OFFSET(LURELATION_NODE, LurnChildren) + ChildrenCnt * sizeof(PLURELATION_NODE), 
					   LONGLONG );

	lurn = ExAllocatePoolWithTag( NonPagedPool, length, LURN_POOL_TAG );

	if (!lurn) {
	
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory( lurn, length );
	
	lurn->Length = (UINT16)length;
	lurn->Type = LSSTRUC_TYPE_LURN;

	*Lurn = lurn;

	return STATUS_SUCCESS;
}


VOID
LurnFree (
	PLURELATION_NODE Lurn
	) 
{
	ExFreePoolWithTag( Lurn, LURN_POOL_TAG );
}

//////////////////////////////////////////////////////////////////////////
//
//	LURN interface default functions
//
NTSTATUS
LurnCreateDefault(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
	) {

	UNREFERENCED_PARAMETER(Lurn);
	UNREFERENCED_PARAMETER(LurnDesc);

	return STATUS_SUCCESS;
}

VOID
LurnCloseDefault (
	PLURELATION_NODE Lurn
	) 
{

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	if (Lurn->IdeDisk) {

		ExFreePoolWithTag( Lurn->IdeDisk, LURNEXT_POOL_TAG );
		Lurn->IdeDisk = NULL;
	}

	return;
}


VOID
LurnResetFaultInfo(PLURELATION_NODE Lurn)
{
	RtlZeroMemory(&Lurn->FaultInfo, sizeof(LURN_FAULT_INFO));
}


// 
// Add fault to Lurn fault list for future analysys
// 
// ErrorCode is dependent on fault type. It can be custom error code.
//
// Param is Type dependent parameter.
// Param is CCB if / Ccb is valid only for fault type LURN_ERR_IO, LURN_ERR_LOCK
//
NTSTATUS
LurnRecordFault(PLURELATION_NODE Lurn, LURN_FAULT_TYPE Type, UINT32 ErrorCode, PVOID Param)
{
	PLURN_FAULT_INFO fInfo = &Lurn->FaultInfo;
	LONG count;
	LURN_FAULT_TYPE	LastFaultType;
	LONG			LastErrorCode;
	PLURN_FAULT_IO pLfi= (PLURN_FAULT_IO) Param;

	LastFaultType = fInfo->LastFaultType;
	LastErrorCode = fInfo->LastErrorCode[LastFaultType];

	//
	// To do: Recognize some indirect error patterns.
	//

#if 0
	//
	// Check accesing same range failed.
	//
	if ((Type == LURN_ERR_READ || Type == LURN_ERR_WRITE || Type == LURN_ERR_VERIFY) && pLfi &&
		(ErrorCode == LURN_FAULT_COMMUNICATION))	{
		if (fInfo->LastIoOperation) {
			//
			// Check accesing same range failed and error occured multiple times.
			//
			if (fInfo->LastIoAddr + fInfo->LastIoLength > pLfi->Address 
				&& pLfi->Address+pLfi->Length > fInfo->LastIoAddr &&
						fInfo->ErrorCount[Type] > 5) {
				KDPrintM(DBG_LURN_ERROR, ("IO failed on same location. Marking error\n"));
				ErrorCode = LURN_FAULT_BAD_SECTOR;
			}
		} else {
			// no previous IO error recorded.
		}
	}
#endif
	//
	// to do: handle disk hang after bad sector..
	//
	
	// 
	// Update internal fault record.
	//		
	switch(Type) {
	case LURN_ERR_SUCCESS:
		count = InterlockedIncrement(&fInfo->ErrorCount[Type]);
		if (count == 128) {
			KDPrintM(DBG_LURN_TRACE, ("Resetting error count\n"));

			//
			// If some IO operation completed without any error, clear all other errors.
			//
			LurnResetFaultInfo(Lurn);
			InterlockedExchange(&fInfo->ErrorCount[Type], count);
		}
		break;
	case LURN_ERR_CONNECT:
	case LURN_ERR_LOGIN:
	case LURN_ERR_NDAS_OP:
	case LURN_ERR_DISK_OP:
	case LURN_ERR_READ:
	case LURN_ERR_WRITE: 
	case LURN_ERR_VERIFY: 		
	case LURN_ERR_UNKNOWN:
	case LURN_ERR_FLUSH:
	default:		
		// Error occured. Reset success count
		InterlockedExchange(&fInfo->ErrorCount[LURN_ERR_SUCCESS], 0);
		InterlockedIncrement(&fInfo->ErrorCount[Type]);
		InterlockedExchange(&fInfo->LastErrorCode[Type], ErrorCode);
		fInfo->LastFaultType = Type;
		KeQuerySystemTime(&fInfo->LastFaultTime);
		break;
	}

	if (Type == LURN_ERR_READ || Type == LURN_ERR_WRITE) {
		if (pLfi) {
			fInfo->LastIoOperation = Type;
			fInfo->LastIoAddr = pLfi->Address;
			fInfo->LastIoLength = pLfi->Length;
		}
	}

	//
	// Update error causes.
	//

	//
	// Guess error cause.  Be cautious when setting error because this can mark a disk as defective permanently.
	//
	if (Type == LURN_ERR_CONNECT) {
		fInfo->FaultCause |=  LURN_FCAUSE_TARGET_DOWN;
	} else {
		// At least target is not down
		fInfo->FaultCause &= ~LURN_FCAUSE_TARGET_DOWN;
	}

	if ((Type == LURN_ERR_READ || Type == LURN_ERR_WRITE || Type == LURN_ERR_VERIFY) &&
		ErrorCode == LURN_FAULT_BAD_SECTOR) {
		fInfo->FaultCause |=  LURN_FCAUSE_BAD_SECTOR;
	}

	if (Type == LURN_ERR_DISK_OP) {
		if (fInfo->ErrorCount[LURN_ERR_DISK_OP] > 5 && ErrorCode == LURN_FAULT_IDENTIFY) {
			//
			// If HDD's board is broken, identify may not work. 
			//
			// To do: mark LURN_FAULT_IDENTIFY fault only when this is just communication problem. (Need support from lpx)
			//
			fInfo->FaultCause |=  LURN_FCAUSE_BAD_DISK; 
		} else if (ErrorCode == LURN_FAULT_NOT_EXIST){
			//
			// NDAS chip cannot detect HDD. HDD may be removed or dead
			//
			fInfo->FaultCause |=  LURN_FAULT_NO_TARGET;
		}
	}

	//
	// to do: handle more error cases: LURN_ERR_FLUSH, LURN_ERR_LOGIN, LURN_ERR_NDAS_OP
	//
	return STATUS_SUCCESS;
}

ULONG
LurnGetCauseOfFault(PLURELATION_NODE Lurn)
{
	return Lurn->FaultInfo.FaultCause;
}

