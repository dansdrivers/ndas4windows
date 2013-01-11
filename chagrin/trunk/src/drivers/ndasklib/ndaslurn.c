#include "ndasscsiproc.h"


#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__

#define __MODULE__ "ndaslurn"


//
// Create an LUR with LUR descriptor.
//

NTSTATUS
LurCreate (
	IN PLURELATION_DESC		LurDesc,
	OUT PLURELATION			*Lur,
	IN BOOLEAN				EnableSecondary,
	IN BOOLEAN				EnableW2kReadOnlyPacth,
	IN PDEVICE_OBJECT		AdapterFunctionDeviceObject,
	IN PVOID				AdapterHardwareExtension,
	IN LURN_EVENT_CALLBACK	LurnEventCallback
	)
{
	LONG					idx_lurn;
	LONG					idx_child;
	PLURELATION_NODE_DESC	cur_lurndesc;
	PLURELATION_NODE		cur_lurn;
	LURNDESC_ENTRY			lurndesc_table[LUR_MAX_LURNS_PER_LUR];
	NTSTATUS				status;
	ULONG					child;
	PLURELATION				tmpLur;
	ULONG					tmpLurLength;
	BOOLEAN					writeCheckRequired = FALSE;
	

	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );
	
	ASSERT( LurDesc->LurnDescCount > 0 );
	ASSERT( LurDesc->LurnDescCount <= LUR_MAX_LURNS_PER_LUR );

	if (LurDesc->LurnDescCount <= 0) {
	
		DebugTrace (NDASSCSI_DEBUG_LURN_INFO, ("No child node.\n") );
		return STATUS_INVALID_PARAMETER;
	}

#if __ENABLE_CONTENTENCRYPT_AES_TEST__

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

#endif

	RtlZeroMemory( lurndesc_table, sizeof(LURNDESC_ENTRY) * LUR_MAX_LURNS_PER_LUR );

	//	create LURELATION
	
	tmpLurLength = sizeof(LURELATION) + sizeof(PLURELATION_NODE) * (LurDesc->LurnDescCount - 1);
	
	tmpLur = (PLURELATION)ExAllocatePoolWithTag(NonPagedPool, tmpLurLength, LUR_POOL_TAG);

	if (!tmpLur) {

		DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("ExAllocatePoolWithTag() failed.\n") );
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	tmpLur->Type					= LSSTRUC_TYPE_LUR;
	tmpLur->Length					= sizeof(LURELATION);
	tmpLur->DeviceMode				= LurDesc->DeviceMode;
	tmpLur->EndingBlockAddr			= LurDesc->EndingBlockAddr;
	tmpLur->LowestHwVer				= -1;
	tmpLur->LurFlags				= 0;
	tmpLur->SupportedNdasFeatures	= 0;
	tmpLur->EnabledNdasFeatures		= 0;
	
	if (EnableSecondary) {
	
		tmpLur->EnabledNdasFeatures |= NDASFEATURE_SECONDARY;
	}

	tmpLur->DevType				= LurDesc->DevType;
	tmpLur->DevSubtype			= LurDesc->DevSubtype;
	tmpLur->NodeCount			= LurDesc->LurnDescCount;

	RtlCopyMemory( tmpLur->LurId, LurDesc->LurId, LURID_LENGTH );

	tmpLur->AdapterFunctionDeviceObject	= AdapterFunctionDeviceObject;
	tmpLur->AdapterHardwareExtension	= AdapterHardwareExtension;
	tmpLur->LurnEventCallback			= LurnEventCallback;

	LsuInitializeBlockAcl( &tmpLur->SystemBacl );
	LsuInitializeBlockAcl( &tmpLur->UserBacl );

	//	Block access control list
	
	if (LurDesc->BACLOffset) {
	
		PNDAS_BLOCK_ACL	srcBACL;

		srcBACL = (PNDAS_BLOCK_ACL)((PUCHAR)LurDesc + LurDesc->BACLOffset);
		ASSERT(srcBACL->Length == LurDesc->BACLLength);

		status = LsuConvertNdasBaclToLsuBacl(&tmpLur->SystemBacl, srcBACL);

		if (!NT_SUCCESS(status)) {

			ExFreePoolWithTag(tmpLur, LUR_POOL_TAG);
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
	
	tmpLur->CntEcrMethod		= LurDesc->CntEcrMethod;
	tmpLur->CntEcrKeyLength		= LurDesc->CntEcrKeyLength;
	
	if(LurDesc->CntEcrKeyLength) {
	
		RtlCopyMemory(tmpLur->CntEcrKey, LurDesc->CntEcrKey, tmpLur->CntEcrKeyLength);

#if DBG
		{
			ULONG	keyIdx;
			DebugTrace(DBG_LURN_INFO, ("Encryption key method:%02x, key length:%d * 4bytes, ",
												LurDesc->CntEcrMethod,
												(int)LurDesc->CntEcrKeyLength)
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

	//	Allocate LURNs to LURN descriptor Table and sanity check.
	
	RtlZeroMemory( lurndesc_table, sizeof(LURNDESC_ENTRY) * LUR_MAX_LURNS_PER_LUR );
	
	cur_lurndesc = LurDesc->LurnDesc;

	for (idx_lurn = 0; idx_lurn < (LONG)LurDesc->LurnDescCount; idx_lurn++) {

		DebugTrace( DBG_LURN_INFO, ("Idx:%d LurnDesc:%p NextOffset:%d\n", idx_lurn, cur_lurndesc, cur_lurndesc->NextOffset)) ;
		
		lurndesc_table[idx_lurn].LurnDesc = cur_lurndesc;

		if (cur_lurndesc->NextOffset == 0 && (LONG)idx_lurn + 1 < (LONG)LurDesc->LurnDescCount) {
	
			DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("Invaild NextOffset.\n"));
			status = STATUS_INVALID_PARAMETER;
			goto error_out;
		}

		if (cur_lurndesc->LurnType < 0 || cur_lurndesc->LurnType >= LurnInterfaceCnt) {

			DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("Invaild LurnType:%x.\n", cur_lurndesc->LurnType));
			status = STATUS_INVALID_PARAMETER;
			goto error_out;
		}

		status = LurnAllocate(&lurndesc_table[idx_lurn].Lurn, lurndesc_table[idx_lurn].LurnDesc->LurnChildrenCnt);
		
		if (!NT_SUCCESS(status)) {
		
			DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("LurnAllocate() failed.\n"));
			goto error_out;
		}

		//	Also initialize LURN index in the LUR.
		
		lurndesc_table[idx_lurn].Lurn->LurnStatus = LURN_STATUS_INIT;
		tmpLur->Nodes[idx_lurn] = lurndesc_table[idx_lurn].Lurn;

		cur_lurndesc = (PLURELATION_NODE_DESC)((PBYTE)LurDesc + cur_lurndesc->NextOffset);
	}

	//	Build tree. Set up LURN.
	//	Initialize nodes from leaves to a root.
	//	Node 0 is root.
	
retry_init:

	for (idx_lurn = LurDesc->LurnDescCount - 1; idx_lurn >= 0; idx_lurn--) {

		cur_lurndesc= lurndesc_table[idx_lurn].LurnDesc;
		cur_lurn	= lurndesc_table[idx_lurn].Lurn;

		//	Set LURN IDs of itself, parent,and  children.
		//	LURN ID must be the index number in LUR->Nodes[] array.
		
		cur_lurn->LurnId			= cur_lurndesc->LurnId;
		cur_lurn->LurnParent		= lurndesc_table[cur_lurndesc->LurnParent].Lurn;

		// Children
		
		cur_lurn->LurnChildrenCnt	= cur_lurndesc->LurnChildrenCnt;
		
		for (idx_child = 0; idx_child < (LONG)cur_lurndesc->LurnChildrenCnt; idx_child++) {
		
			child = cur_lurndesc->LurnChildren[idx_child];
			
			if (child > LurDesc->LurnDescCount) {

				DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("invalid child number.\n"));
				goto error_out;
			}

			cur_lurn->LurnChildren[idx_child] = lurndesc_table[child].Lurn;
			lurndesc_table[child].Lurn->LurnChildIdx = idx_child;
		}

		//	LUR Node's access right
		
		cur_lurndesc->AccessRight = 0;

		if (tmpLur->DeviceMode == DEVMODE_SHARED_READONLY) {
		
			cur_lurndesc->AccessRight = GENERIC_READ;

		} else if (tmpLur->DeviceMode == DEVMODE_SHARED_READWRITE) {

			if (tmpLur->EnabledNdasFeatures & NDASFEATURE_SECONDARY) {
			
				cur_lurndesc->AccessRight = GENERIC_READ;

			} else {

				cur_lurndesc->AccessRight = GENERIC_WRITE | GENERIC_READ;
			}

		} else {

			cur_lurndesc->AccessRight = GENERIC_WRITE | GENERIC_READ;
		}

		// LUR node options

		if (cur_lurndesc->LurnOptions & LURNOPTION_RESTRICT_UDMA) {
		
			cur_lurn->UDMARestrictValid = TRUE;
			cur_lurn->UDMARestrict = cur_lurndesc->UDMARestrict;

		} else {

			cur_lurn->UDMARestrictValid = FALSE;
		}

		if (LURN_IDE_DISK == cur_lurndesc->LurnType && LANSCSIIDE_VERSION_2_0 == cur_lurndesc->LurnIde.HWVersion) {
		
			writeCheckRequired = TRUE;
		}

		//	Initialize the LURN

		ASSERT( cur_lurn->SavedLurnDesc == NULL );

		status = LurnInitialize( cur_lurn, tmpLur, cur_lurndesc );

		if (tmpLur->DeviceMode == DEVMODE_SHARED_READWRITE) {

			if (status == STATUS_ACCESS_DENIED && !(tmpLur->EnabledNdasFeatures & NDASFEATURE_SECONDARY)) {
			
				LONG	idx_closinglurn;

				NDASSCSI_ASSERT( idx_lurn == LurDesc->LurnDescCount - 1 );

				if (LurDesc->LurOptions & LUROPTION_DONOT_ADJUST_PRIMSEC_ROLE) {
			
					DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("DONOT_ADJUST_ACCESSMODE is set. Can not go to Secondary mode.\n"));

				} else {

					for (idx_closinglurn = idx_lurn; idx_closinglurn < (LONG)LurDesc->LurnDescCount; idx_closinglurn++) {

						LurnSendStopCcb(lurndesc_table[idx_closinglurn].Lurn);
						LurnDestroy(lurndesc_table[idx_closinglurn].Lurn);
					}

					DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("Degrade access right and retry.\n"));
					tmpLur->EnabledNdasFeatures |= NDASFEATURE_SECONDARY;

					goto retry_init;
				}
			}
		}

		if (!NT_SUCCESS(status)) {

			LONG	idx_closinglurn;

			NDASSCSI_ASSERT( FALSE );

			DebugTrace( NDASSCSI_DEBUG_LURN_INFO, ("LurnInitialize() failed. LURN#:%d NTSTATUS:%08lx\n", idx_lurn, status) );
		
			for (idx_closinglurn = idx_lurn; idx_closinglurn < (LONG)LurDesc->LurnDescCount; idx_closinglurn++) {
				
				LurnSendStopCcb(lurndesc_table[idx_closinglurn].Lurn);
				LurnDestroy(lurndesc_table[idx_closinglurn].Lurn);
			}
			goto error_out;
		}
	}

	// Indicate Win2K read-only patch
	
	if (EnableW2kReadOnlyPacth) {

		tmpLur->LurFlags |= LURFLAG_W2K_READONLY_PATCH;
	}

	if (writeCheckRequired) {

		if (LurDesc->LurOptions & LUROPTION_ON_NDAS_2_0_WRITE_CHECK) {
		
			tmpLur->LurFlags |= LURFLAG_WRITE_CHECK_REQUIRED;

		} else if (LurDesc->LurOptions & LUROPTION_OFF_NDAS_2_0_WRITE_CHECK) {

			tmpLur->LurFlags &= ~LURFLAG_WRITE_CHECK_REQUIRED;

		} else {

			// Default on.
			tmpLur->LurFlags |= LURFLAG_WRITE_CHECK_REQUIRED;
		}
	}

	//	Detect supported NDAS features based on the lowest hardware version
	//	We can only support minimum of all child hardware for RAID.
	
	
	LurDetectSupportedNdasFeatures(&tmpLur->SupportedNdasFeatures, tmpLur->LowestHwVer);

	//	Enable features to default values or values set by a user.
	//	User requests overrides the default features.
	
	status = LurEnableNdasFeauresByDeviceMode( &tmpLur->EnabledNdasFeatures,
											   tmpLur->SupportedNdasFeatures,
											   EnableW2kReadOnlyPacth,
											   tmpLur->DeviceMode );
	
	if (!NT_SUCCESS(status)) {

		goto error_out;
	}

	status = LurEnableNdasFeaturesByUserRequest( &tmpLur->EnabledNdasFeatures,
												 tmpLur->SupportedNdasFeatures,
												 LurDesc->LurOptions );
	
	if (!NT_SUCCESS(status)) {

		goto error_out;
	}

	// set Lur
	
	*Lur = tmpLur;

	return STATUS_SUCCESS;

error_out:
	
	for(idx_lurn = 0; (ULONG)idx_lurn < LurDesc->LurnDescCount; idx_lurn++) {
	
		PLURELATION_NODE	lurn;

		lurn = lurndesc_table[idx_lurn].Lurn;

		if(lurn) {

			DebugTrace(NDASSCSI_DEBUG_LURN_INFO, ("Freeing LURN:%p(%d)\n", lurn, idx_lurn));

			if (lurn->LurnStatus == LURN_STATUS_RUNNING) {

				LurnSendStopCcb(lurn);
				LurnDestroy(lurn);
			}

			LurnFree(lurn);
			lurndesc_table[idx_lurn].Lurn = NULL;
		}
	}

	if (tmpLur) {

		LsuFreeAllBlockAce(&tmpLur->UserBacl.BlockAclHead);
		LsuFreeAllBlockAce(&tmpLur->SystemBacl.BlockAclHead);
		ExFreePoolWithTag(tmpLur, LUR_POOL_TAG);
	}

	return status;
}
