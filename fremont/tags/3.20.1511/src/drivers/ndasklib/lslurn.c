#include <ntddk.h>
#include "ver.h"
#include "LSKlib.h"
#include "KDebug.h"
#include "basetsdex.h"
#include "cipher.h"
#include "LSProto.h"
#include "LSTransport.h"
#include "LSCcb.h"
#include "LSLurn.h"
#include "LSLurnIDE.h"
#include "LSLurnAssoc.h"

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
	NDASFEATURE_SIMULTANEOUS_WRITE | NDASFEATURE_GP_LOCK | NDASFEATURE_DYNAMIC_REQUEST_SIZE,

	// hw version 2.0
	NDASFEATURE_SECONDARY |	NDASFEATURE_OOB_WRITE | NDASFEATURE_RO_FAKE_WRITE | 
	NDASFEATURE_SIMULTANEOUS_WRITE | NDASFEATURE_GP_LOCK|
	NDASFEATURE_DYNAMIC_REQUEST_SIZE,

	// hw version 2.5
	NDASFEATURE_SECONDARY |	NDASFEATURE_OOB_WRITE | NDASFEATURE_RO_FAKE_WRITE |
	NDASFEATURE_SIMULTANEOUS_WRITE | NDASFEATURE_ADV_GP_LOCK| NDASFEATURE_DYNAMIC_REQUEST_SIZE
};


//////////////////////////////////////////////////////////////////////////
//
//	LU Relation
//
typedef struct _LURNDESC_TABLE {
	PLURELATION_NODE_DESC	LurnDesc;
	PLURELATION_NODE		Lurn;
} LURNDESC_ENTRY, *PLURNDESC_ENTRY;


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

	ntStatus = LSCcbAllocate(&ccb);
	if(!NT_SUCCESS(ntStatus)) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	LSCCB_INITIALIZE(ccb);
	ccb->OperationCode				= CCB_OPCODE_STOP;
	ccb->HwDeviceExtension			= NULL;
	LSCcbSetFlag(ccb, CCB_FLAG_SYNCHRONOUS|CCB_FLAG_ALLOCATED|CCB_FLAG_DONOT_PASSDOWN);
	LSCcbSetCompletionRoutine(ccb, NULL, NULL);

	//
	//	Send a CCB to the LURN.
	//
	ntStatus = LurnRequest(
					Lurn,
					ccb
				);
	if(!NT_SUCCESS(ntStatus)) {
        LSCcbFree(ccb);
	}

	return ntStatus;
}


//
// Retrieve NDAS feature according to NDAS chip version.
//

static
VOID
LurDetectSupportedNdasFeatures(
	PNDAS_FEATURES	NdasFeatures,
	UINT32			LowestHwVersion
){

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

static
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

static
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
		_LUR_RESET_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_OOB_WRITE);
	} else if(LurOptions & LUROPTION_ON_OOB_WRITE) {
		_LUR_SET_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_OOB_WRITE);
	}

	//
	//	Simultaneous write
	//

	if(	(LurOptions & LUROPTION_ON_SIMULTANEOUS_WRITE) &&
		(LurOptions & LUROPTION_OFF_SIMULTANEOUS_WRITE) ) {
			return STATUS_UNSUCCESSFUL;
	}

	if(LurOptions & LUROPTION_ON_SIMULTANEOUS_WRITE) {
		_LUR_SET_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_SIMULTANEOUS_WRITE);
	} else if(LurOptions & LUROPTION_OFF_SIMULTANEOUS_WRITE) {
		_LUR_RESET_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_SIMULTANEOUS_WRITE);
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
	if( LurOptions & LUROPTION_OFF_DYNAMIC_REQUEST_SIZE) {
		_LUR_RESET_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_DYNAMIC_REQUEST_SIZE);
	} else if(LurOptions & LUROPTION_ON_DYNAMIC_REQUEST_SIZE) {
		_LUR_SET_FEATURE(SupportedFeatures, *EnabledFeatures, NDASFEATURE_DYNAMIC_REQUEST_SIZE);
	}


	return STATUS_SUCCESS;
}


//
// Create an LUR with LUR descriptor.
//

NTSTATUS
LurCreate(
	IN PLURELATION_DESC		LurDesc,
	OUT PLURELATION			*Lur,
	IN BOOLEAN				EnableSecondary,
	IN BOOLEAN				EnableW2kReadOnlyPacth,
	IN PVOID				AdapterFdo,
	IN LURN_EVENT_CALLBACK	LurnEventCallback
){
	LONG					idx_lurn;
	LONG					idx_child;
	PLURELATION_NODE_DESC	cur_lurndesc;
	PLURELATION_NODE		cur_lurn;
	LURNDESC_ENTRY			lurndesc_table[LUR_MAX_LURNS_PER_LUR];
	NTSTATUS				status;
	ULONG					child;
	PLURELATION				tmpLur;
	ULONG					tmpLurLength;
	BOOLEAN					WriteCheckRequired = FALSE;
	
	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);
	ASSERT(LurDesc->LurnDescCount > 0);
	ASSERT(LurDesc->LurnDescCount <= LUR_MAX_LURNS_PER_LUR);

	if(LurDesc->LurnDescCount <= 0) {
		KDPrintM(DBG_LURN_ERROR, ("No child node.\n"));
		return STATUS_INVALID_PARAMETER;
	}

#ifdef __ENABLE_CONTENTENCRYPT_AES_TEST__

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

	RtlZeroMemory(lurndesc_table, sizeof(LURNDESC_ENTRY) * LUR_MAX_LURNS_PER_LUR);
	tmpLur = NULL;

	//
	//	create LURELATION
	//
	tmpLurLength =	sizeof(LURELATION) +
					sizeof(PLURELATION_NODE) * (LurDesc->LurnDescCount - 1);
	tmpLur = (PLURELATION)ExAllocatePoolWithTag(NonPagedPool, tmpLurLength, LUR_POOL_TAG);
	if(!tmpLur) {
		KDPrintM(DBG_LURN_ERROR, ("ExAllocatePoolWithTag() failed.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	tmpLur->Type				= LSSTRUC_TYPE_LUR;
	tmpLur->Length				= sizeof(LURELATION);
	tmpLur->DeviceMode			= LurDesc->DeviceMode;
	tmpLur->EndingBlockAddr		= LurDesc->EndingBlockAddr;
	tmpLur->LowestHwVer			= -1;
	tmpLur->LurFlags			= 0;
	tmpLur->SupportedNdasFeatures = 0;
	tmpLur->EnabledNdasFeatures = 0;
	if(EnableSecondary) {
		tmpLur->EnabledNdasFeatures |= NDASFEATURE_SECONDARY;
	}

	tmpLur->DevType				= LurDesc->DevType;
	tmpLur->DevSubtype			= LurDesc->DevSubtype;
	tmpLur->NodeCount			= LurDesc->LurnDescCount;
	RtlCopyMemory(tmpLur->LurId, LurDesc->LurId, LURID_LENGTH);

	tmpLur->AdapterFdo			= AdapterFdo;
	tmpLur->LurnEventCallback	= LurnEventCallback;

	LsuInitializeBlockAcl(&tmpLur->SystemBacl);
	LsuInitializeBlockAcl(&tmpLur->UserBacl);

	//
	//	Block access control list
	//

	if(LurDesc->BACLOffset) {
		PNDAS_BLOCK_ACL	srcBACL;

		srcBACL = (PNDAS_BLOCK_ACL)((PUCHAR)LurDesc + LurDesc->BACLOffset);
		ASSERT(srcBACL->Length == LurDesc->BACLLength);

		status = LsuConvertNdasBaclToLsuBacl(&tmpLur->SystemBacl, srcBACL);
		if(!NT_SUCCESS(status)) {
			ExFreePoolWithTag(tmpLur, LUR_POOL_TAG);
			return status;
		}
	}

#ifdef __ENABLE_BACL_TEST__
	{
		PLSU_BLOCK_ACE	blockAce;

		blockAce = LsuCreateBlockAce(LSUBACE_ACCESS_READ, 0x3f, 0x3f);
		if(blockAce) {
			LsuInsertAce(&tmpLur->SystemBacl, blockAce);
		}
	}
#endif

	//
	//	Content encryption
	//
	tmpLur->CntEcrMethod		= LurDesc->CntEcrMethod;
	tmpLur->CntEcrKeyLength		= LurDesc->CntEcrKeyLength;
	if(LurDesc->CntEcrKeyLength) {
		RtlCopyMemory(tmpLur->CntEcrKey, LurDesc->CntEcrKey, tmpLur->CntEcrKeyLength);
#if DBG
		{
			ULONG	keyIdx;
			KDPrintM(DBG_LURN_INFO, ("Encryption key method:%02x, key length:%d * 4bytes, ",
												LurDesc->CntEcrMethod,
												(int)LurDesc->CntEcrKeyLength)
												);
			for(keyIdx = 0; keyIdx<tmpLur->CntEcrKeyLength; keyIdx++) {
				KDPrintM(DBG_LURN_INFO, ("%02x ", (int)tmpLur->CntEcrKey[keyIdx]));
				if((keyIdx%4) == 0)
					KDPrintM(DBG_LURN_INFO, (" "));
			}
			KDPrintM(DBG_LURN_INFO, ("\n"));
		}
#endif

	}

	//
	//	Allocate LURNs to LURN descriptor Table and sanity check.
	//
	RtlZeroMemory(lurndesc_table, sizeof(LURNDESC_ENTRY)*LUR_MAX_LURNS_PER_LUR);
	cur_lurndesc = LurDesc->LurnDesc;
	for(idx_lurn = 0; idx_lurn < (LONG)LurDesc->LurnDescCount; idx_lurn++) {

		KDPrintM(DBG_LURN_INFO, ("Idx:%d LurnDesc:%p NextOffset:%d\n", idx_lurn, cur_lurndesc, cur_lurndesc->NextOffset)) ;
		lurndesc_table[idx_lurn].LurnDesc = cur_lurndesc;

		if(cur_lurndesc->NextOffset == 0 && (LONG)idx_lurn + 1 < (LONG)LurDesc->LurnDescCount) {
			KDPrintM(DBG_LURN_ERROR, ("Invaild NextOffset.\n"));
			status = STATUS_INVALID_PARAMETER;
			goto error_out;
		}
		if(cur_lurndesc->LurnType < 0 || cur_lurndesc->LurnType >= LurnInterfaceCnt) {
			KDPrintM(DBG_LURN_ERROR, ("Invaild LurnType:%x.\n", cur_lurndesc->LurnType));
			status = STATUS_INVALID_PARAMETER;
			goto error_out;
		}

		status = LurnAllocate(&lurndesc_table[idx_lurn].Lurn, lurndesc_table[idx_lurn].LurnDesc->LurnChildrenCnt);
		if(!NT_SUCCESS(status)) {
			KDPrintM(DBG_LURN_ERROR, ("LurnAllocate() failed.\n"));
			goto error_out;
		}

		//
		//	Also initialize LURN index in the LUR.
		//
		lurndesc_table[idx_lurn].Lurn->LurnStatus = LURN_STATUS_INIT;
		tmpLur->Nodes[idx_lurn] = lurndesc_table[idx_lurn].Lurn;

		cur_lurndesc = (PLURELATION_NODE_DESC)((PBYTE)LurDesc + cur_lurndesc->NextOffset);
	}

	//
	//	Build tree. Set up LURN.
	//	Initialize nodes from leaves to a root.
	//	Node 0 is root.
	//
retry_init:

	for(idx_lurn = LurDesc->LurnDescCount - 1; idx_lurn >= 0; idx_lurn--) {
		cur_lurndesc= lurndesc_table[idx_lurn].LurnDesc;
		cur_lurn	= lurndesc_table[idx_lurn].Lurn;

		//
		//	Set LURN IDs of itself, parent,and  children.
		//	LURN ID must be the index number in LUR->Nodes[] array.
		//

		cur_lurn->LurnId			= cur_lurndesc->LurnId;
		cur_lurn->LurnParent		= lurndesc_table[cur_lurndesc->LurnParent].Lurn;

		// Children
		cur_lurn->LurnChildrenCnt	= cur_lurndesc->LurnChildrenCnt;
		for(idx_child = 0; idx_child < (LONG)cur_lurndesc->LurnChildrenCnt; idx_child++) {
			child = cur_lurndesc->LurnChildren[idx_child];
			if(/* child < 0 || */ child > LurDesc->LurnDescCount)
			{
				if(!NT_SUCCESS(status)) {
					KDPrintM(DBG_LURN_ERROR, ("invalid child number.\n"));
					goto error_out;
				}
			}
			cur_lurn->LurnChildren[idx_child] = lurndesc_table[child].Lurn;
			lurndesc_table[child].Lurn->LurnChildIdx = idx_child;
		}

		//
		//	LUR Node's access right
		//

		cur_lurndesc->AccessRight = 0;

		if(tmpLur->DeviceMode == DEVMODE_SHARED_READONLY) {
			cur_lurndesc->AccessRight = GENERIC_READ;
		} else if(tmpLur->DeviceMode == DEVMODE_SHARED_READWRITE) {

			if(tmpLur->EnabledNdasFeatures & NDASFEATURE_SECONDARY) {
				cur_lurndesc->AccessRight = GENERIC_READ;
			} else {
				cur_lurndesc->AccessRight = GENERIC_WRITE | GENERIC_READ;
			}

		} else {
			cur_lurndesc->AccessRight = GENERIC_WRITE | GENERIC_READ;
		}

		//
		// LUR node options
		//

		if(cur_lurndesc->LurnOptions & LURNOPTION_RESTRICT_UDMA) {
			cur_lurn->UDMARestrictValid = TRUE;
			cur_lurn->UDMARestrict = cur_lurndesc->UDMARestrict;
		} else {
			cur_lurn->UDMARestrictValid = FALSE;
		}

		if (LURN_IDE_DISK == cur_lurndesc->LurnType && 
			 LANSCSIIDE_VERSION_2_0 == cur_lurndesc->LurnIde.HWVersion) {
			WriteCheckRequired = TRUE;
		}

		//
		//	Initialize the LURN
		//
		ASSERT(cur_lurn->SavedLurnDesc == NULL);

		status = LurnInitialize(cur_lurn, tmpLur, cur_lurndesc);

//		if((LURN_IDE_ODD != cur_lurndesc->LurnType) 
//			&& (LURN_IDE_MO != cur_lurndesc->LurnType) ){

			if(tmpLur->DeviceMode == DEVMODE_SHARED_READWRITE) {

				if(status == STATUS_ACCESS_DENIED && !(tmpLur->EnabledNdasFeatures & NDASFEATURE_SECONDARY)) {
						LONG	idx_closinglurn;

					if(LurDesc->LurOptions & LUROPTION_DONOT_ADJUST_PRIMSEC_ROLE) {
						KDPrintM(DBG_LURN_ERROR, ("DONOT_ADJUST_ACCESSMODE is set. Can not go to Secondary mode.\n"));
					} else {
						for(idx_closinglurn = idx_lurn; idx_closinglurn < (LONG)LurDesc->LurnDescCount; idx_closinglurn++) {
							LurnSendStopCcb(lurndesc_table[idx_closinglurn].Lurn);
							LurnDestroy(lurndesc_table[idx_closinglurn].Lurn);
						}

						KDPrintM(DBG_LURN_ERROR, ("Degrade access right and retry.\n"));
						tmpLur->EnabledNdasFeatures |= NDASFEATURE_SECONDARY;

						goto retry_init;
					}
				}
			}
//		}
		if(!NT_SUCCESS(status)) {
			LONG	idx_closinglurn;

			KDPrintM(DBG_LURN_ERROR, ("LurnInitialize() failed. LURN#:%d NTSTATUS:%08lx\n", idx_lurn, status));
			for(idx_closinglurn = idx_lurn; idx_closinglurn < (LONG)LurDesc->LurnDescCount; idx_closinglurn++) {
				LurnSendStopCcb(lurndesc_table[idx_closinglurn].Lurn);
				LurnDestroy(lurndesc_table[idx_closinglurn].Lurn);
			}
			goto error_out;
		}
	}

	//
	// Indicate Win2K read-only patch
	//

	if(EnableW2kReadOnlyPacth)
		tmpLur->LurFlags |= LURFLAG_W2K_READONLY_PATCH;

	if (WriteCheckRequired) {
		if (LurDesc->LurOptions & LUROPTION_ON_NDAS_2_0_WRITE_CHECK) {
			tmpLur->LurFlags |= LURFLAG_WRITE_CHECK_REQUIRED;
		} else if (LurDesc->LurOptions & LUROPTION_OFF_NDAS_2_0_WRITE_CHECK) {
			tmpLur->LurFlags &= ~LURFLAG_WRITE_CHECK_REQUIRED;
		} else {
			// Default on.
			tmpLur->LurFlags |= LURFLAG_WRITE_CHECK_REQUIRED;
		}
	}

	//
	//	Detect supported NDAS features based on the lowest hardware version
	//	We can only support minimum of all child hardware for RAID.
	//
	
	LurDetectSupportedNdasFeatures(&tmpLur->SupportedNdasFeatures, tmpLur->LowestHwVer);

	//
	//	Enable features to default values or values set by a user.
	//	User requests overrides the default features.
	//
	status = LurEnableNdasFeauresByDeviceMode(
					&tmpLur->EnabledNdasFeatures,
					tmpLur->SupportedNdasFeatures,
					EnableW2kReadOnlyPacth,
					tmpLur->DeviceMode);
	if(!NT_SUCCESS(status))
		goto error_out;

	status = LurEnableNdasFeaturesByUserRequest(
					&tmpLur->EnabledNdasFeatures,
					tmpLur->SupportedNdasFeatures,
					LurDesc->LurOptions);
	if(!NT_SUCCESS(status))
		goto error_out;

	//
	// set Lur
	//
	*Lur = tmpLur;

	return STATUS_SUCCESS;

error_out:
	for(idx_lurn = 0; (ULONG)idx_lurn < LurDesc->LurnDescCount; idx_lurn++) {
		PLURELATION_NODE	lurn;

		lurn = lurndesc_table[idx_lurn].Lurn;
		if(lurn) {
			KDPrintM(DBG_LURN_ERROR, ("Freeing LURN:%p(%d)\n", lurn, idx_lurn));

			if(lurn->LurnStatus == LURN_STATUS_RUNNING) {
				LurnSendStopCcb(lurn);
				LurnDestroy(lurn);
			}
			LurnFree(lurn);
			lurndesc_table[idx_lurn].Lurn = NULL;
		}
	}

	if(tmpLur) {
		LsuFreeAllBlockAce(&tmpLur->UserBacl.BlockAclHead);
		LsuFreeAllBlockAce(&tmpLur->SystemBacl.BlockAclHead);
		ExFreePoolWithTag(tmpLur, LUR_POOL_TAG);

	}

	return status;
}

static
VOID
LurCloseRecur(
	  PLURELATION_NODE			Lurn
	) {
	ULONG					idx_lurn;

	LurnDestroy(Lurn);

	for(idx_lurn = 0; idx_lurn < Lurn->LurnChildrenCnt; idx_lurn++ ) {

		LurCloseRecur(Lurn->LurnChildren[idx_lurn]);
	}

	LurnFree(Lurn);
}


VOID
LurClose(
		PLURELATION			Lur
	) {

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	if(LUR_GETROOTNODE(Lur) == NULL) {
		KDPrintM(DBG_LURN_INFO, ("No LURN.\n"));
	} else {
		LurCloseRecur(LUR_GETROOTNODE(Lur));
	}

	LsuFreeAllBlockAce(&Lur->UserBacl.BlockAclHead);
	LsuFreeAllBlockAce(&Lur->SystemBacl.BlockAclHead);
	ExFreePoolWithTag(Lur, LUR_POOL_TAG);
}

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
				LSCcbCompleteCcb(WriteCommand);
				status = STATUS_SUCCESS;
			} else {
				WriteCommand->CcbStatus = CCB_STATUS_INVALID_COMMAND;
				LSCcbCompleteCcb(WriteCommand);
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
				LSCcbSetFlag(WriteCommand, CCB_FLAG_ALLOW_WRITE_IN_RO_ACCESS);
			}

			status = LurnRequest(LUR_GETROOTNODE(Lur), WriteCommand);
		}
	} else {
		status = STATUS_NO_MORE_ENTRIES;
	}

	return status;
}


NTSTATUS
LurProcessWrite(
	PLURELATION			Lur,
	PCCB				WriteCommand
){
	NTSTATUS	status;
	UINT64		writeAddr;
	UINT32		writeLen;

	//
	//	Check to see if the buffer lock is required.
	//

	if(Lur->EnabledNdasFeatures & NDASFEATURE_SIMULTANEOUS_WRITE){
		LSCcbSetFlag(WriteCommand, CCB_FLAG_ACQUIRE_BUFLOCK);
	} else {
		LSCcbResetFlag(WriteCommand, CCB_FLAG_ACQUIRE_BUFLOCK);
	}

	//
	//	Check to see if the write check (NDAS chip 2.0 bug patch) is required.
	//	Anyway this option is activated only when HW is 2.0 rev.0.
	//

	if (Lur->LurFlags & LURFLAG_WRITE_CHECK_REQUIRED) {
		LSCcbSetFlag(WriteCommand, CCB_FLAG_WRITE_CHECK);
	} else {
		LSCcbResetFlag(WriteCommand, CCB_FLAG_WRITE_CHECK);
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
		return STATUS_NOT_SUPPORTED;
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
				LSCcbCompleteCcb(WriteCommand);
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
				LSCcbCompleteCcb(WriteCommand);
				status = STATUS_SUCCESS;
				break;
			}
			//
			// Turn off write-check for OOB access
			//
			LSCcbResetFlag(WriteCommand, CCB_FLAG_WRITE_CHECK);			
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
				WriteCommand->CcbStatus = CCB_STATUS_SUCCESS;
				LSCcbCompleteCcb(WriteCommand);
				status = STATUS_SUCCESS;
				break;
			}
			WriteCommand->CcbStatus = CCB_STATUS_INVALID_COMMAND;
			LSCcbCompleteCcb(WriteCommand);
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
				WriteCommand->CcbStatus = CCB_STATUS_SUCCESS;
				LSCcbCompleteCcb(WriteCommand);
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
			LSCcbCompleteCcb(WriteCommand);
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


NTSTATUS
LurRequest(
		PLURELATION			Lur,
		PCCB				Ccb
	) {
	NTSTATUS	status;

	if(!Lur) {
		KDPrintM(DBG_LURN_ERROR, ("==============> LUR pointer NULL!\n"));
		return STATUS_INVALID_PARAMETER;
	}
	if(Lur->NodeCount <= 0) {
		KDPrintM(DBG_LURN_ERROR, ("==============> No LURN available.\n"));
		return STATUS_INVALID_PARAMETER;
	}

	//
	//	Increase stack location
	//

	ASSERT(Ccb->CcbCurrentStackLocation == &Ccb->CcbStackLocation[Ccb->CcbCurrentStackLocationIndex]);
	Ccb->CcbCurrentStackLocation--;
	Ccb->CcbCurrentStackLocationIndex--;
	Ccb->CcbCurrentStackLocation->Lurn = Lur;
	//	LUR's stack location should be on the second place.
	ASSERT(Ccb->CcbCurrentStackLocationIndex == NR_MAX_CCB_STACKLOCATION - 2);


	if(Ccb->CcbCurrentStackLocationIndex < 0) {
		ASSERT(FALSE);
		return STATUS_DATA_OVERRUN;
	}

	//
	//	Access check
	//

	if(Ccb->OperationCode == CCB_OPCODE_EXECUTE) {

		switch(Ccb->Cdb[0]) {
			case SCSIOP_WRITE:
			case SCSIOP_WRITE16:

			status = LurProcessWrite(Lur, Ccb);
			return status;

			case 0x3E:		// READ_LONG
			case SCSIOP_READ:
			case SCSIOP_READ16:
				//
				//	Check to see if the buffer lock required.
				//
#ifdef __ENABLE_LOCKED_READ__

				if(Lur->EnabledNdasFeatures & NDASFEATURE_SIMULTANEOUS_WRITE){
					LSCcbsetFlag(Ccb, CCB_FLAG_ACQUIRE_BUFLOCK);
				} else {
					LSCcbResetFlag(Ccb, CCB_FLAG_ACQUIRE_BUFLOCK);
				}
#else
				LSCcbResetFlag(Ccb, CCB_FLAG_ACQUIRE_BUFLOCK);
#endif
			break;
			case SCSIOP_MODE_SENSE:
				//
				//	Check to see if the mode sense,
				//	LUR node should perform win2k read-only patch
				//
				if(Lur->LurFlags & LURFLAG_W2K_READONLY_PATCH) {
					LSCcbSetFlag(Ccb, CCB_FLAG_W2K_READONLY_PATCH);
				}
				//
				//	Check to see if the mode sense go through the secondary(read-only) LUR node
				//	If it does, set ALLOW_WRITE_FLAG
				//
				if(Lur->EnabledNdasFeatures & NDASFEATURE_SECONDARY) {
					LSCcbSetFlag(Ccb, CCB_FLAG_ALLOW_WRITE_IN_RO_ACCESS);
				}
			break;
			default: ;
		}
	}

	return LurnRequest(LUR_GETROOTNODE(Lur),Ccb);
}

//////////////////////////////////////////////////////////////////////////
//
//	Translate AddTargetData to LUR descriptor.
//
VOID
LurTranslateUnitdiskToLurnNodeDesc(
	OUT PLURELATION_NODE_DESC		NodeDesc,
	IN PNDASBUS_UNITDISK				UnitDiskPointer,
	IN UINT16						NextOffset,
	IN UINT32						LurnId,
	IN LURN_TYPE						LurnType,
	IN UINT64						StartAddr,
	IN UINT64						EndAddr,
	IN UINT64						UnitBlocks,
	IN UINT32						MaxDataSend,
	IN UINT32						MaxDataRecv,
	IN UINT32						ChildCnt,
	IN UINT32						Parent
) {
	NodeDesc->NextOffset			= NextOffset;
	NodeDesc->LurnId				= LurnId;															
	NodeDesc->LurnType			= LurnType;															
	NodeDesc->AccessRight			= 0;																	
	NodeDesc->StartBlockAddr		= StartAddr;
	NodeDesc->EndBlockAddr		= EndAddr;															
	NodeDesc->UnitBlocks			= UnitBlocks;															
	NodeDesc->MaxDataSendLength	= MaxDataSend;
	NodeDesc->MaxDataRecvLength	= MaxDataRecv;
	switch(LurnType) {
	case LURN_IDE_DISK:
	case LURN_IDE_ODD:
	case LURN_IDE_MO:	
		ASSERT(UnitDiskPointer);
		NodeDesc->LurnOptions			= UnitDiskPointer->LurnOptions;
		NodeDesc->UDMARestrict		= UnitDiskPointer->UDMARestrict;
		NodeDesc->ReconnTrial			= UnitDiskPointer->ReconnTrial;
		NodeDesc->ReconnInterval		= UnitDiskPointer->ReconnInterval;
		LSTRANS_COPY_LPXADDRESS(&NodeDesc->LurnIde.BindingAddress, &UnitDiskPointer->NICAddr);
		LSTRANS_COPY_LPXADDRESS(&NodeDesc->LurnIde.TargetAddress, &UnitDiskPointer->Address);
		RtlCopyMemory(&NodeDesc->LurnIde.UserID, &UnitDiskPointer->iUserID, LSPROTO_USERID_LENGTH);
		RtlCopyMemory(&NodeDesc->LurnIde.Password, &UnitDiskPointer->iPassword, LSPROTO_PASSWORD_LENGTH);
		NodeDesc->LurnIde.HWType		= UnitDiskPointer->ucHWType;
		NodeDesc->LurnIde.HWVersion	= UnitDiskPointer->ucHWVersion;
		NodeDesc->LurnIde.HWRevision	= UnitDiskPointer->ucHWRevision;
		LSTRANS_COPY_LPXADDRESS(&NodeDesc->LurnIde.TargetAddress, &UnitDiskPointer->Address);
		NodeDesc->LurnIde.LanscsiTargetID = UnitDiskPointer->ucUnitNumber;
		break;
	default:
		NodeDesc->LurnOptions			= 0;
		NodeDesc->UDMARestrict		= 0;
		NodeDesc->ReconnTrial			= 0;
		NodeDesc->ReconnInterval		= 0;
		NodeDesc->LurnIde.LanscsiLU	= 0;
		break;
	}
	NodeDesc->LurnChildrenCnt		= ChildCnt;
	NodeDesc->LurnParent			= Parent; 
}


NTSTATUS
LurTranslateAddTargetDataToLURDesc(
	   IN PNDASBUS_ADD_TARGET_DATA	AddTargetData,
	   IN ULONG						LurMaxOsRequestLength, // in bytes
	   IN LONG						LurDescLength,
	   OUT PLURELATION_DESC			LurDesc
	) {
	LONG						ChildLurnCnt;
	LONG						idx_childlurn;
	PBYTE						cur_location;
	PLURELATION_NODE_DESC		cur_lurn;
	PNDASBUS_UNITDISK				cur_unitdisk;


	UNREFERENCED_PARAMETER(LurDescLength);

	KDPrintM(DBG_LURN_ERROR, ("In.\n"));

	ChildLurnCnt	= AddTargetData->ulNumberOfUnitDiskList;
	cur_location	= (PBYTE)LurDesc;
	if(ChildLurnCnt <= 0) {
		KDPrintM(DBG_LURN_ERROR, ("ERROR : AddTargetData->ulNumberOfUnitDiskList = %d\n", AddTargetData->ulNumberOfUnitDiskList));
		return STATUS_INVALID_PARAMETER;
	}

	//
	//	Initialize LUR descriptor
	//
	LurDesc->Type = LSSTRUC_TYPE_LUR;
	LurDesc->Length = SIZE_OF_LURELATION_DESC();
	LurDesc->LurOptions = AddTargetData->LurOptions;
	LurDesc->LurId[0] = 0;
	LurDesc->LurId[1] = 0;
	LurDesc->LurId[2] = 0;
	LurDesc->MaxOsRequestLength = LurMaxOsRequestLength;
	LurDesc->LurnDescCount = 0;
	LurDesc->DeviceMode = AddTargetData->DeviceMode;
	LurDesc->DevSubtype = AddTargetData->ucTargetType;
	cur_location += SIZE_OF_LURELATION_DESC();
	cur_lurn = (PLURELATION_NODE_DESC)cur_location;
	cur_unitdisk = AddTargetData->UnitDiskList;

	//
	//	Encryption key
	//
	LurDesc->CntEcrMethod		= AddTargetData->CntEcrMethod;
	LurDesc->CntEcrKeyLength	= AddTargetData->CntEcrKeyLength;
	if(LurDesc->CntEcrMethod && LurDesc->CntEcrKeyLength) {
		RtlCopyMemory(LurDesc->CntEcrKey, AddTargetData->CntEcrKey, AddTargetData->CntEcrKeyLength);
#if DBG
		{
			ULONG	keyIdx;
			KDPrintM(DBG_LURN_INFO, ("Encryption key method:%02x, key length:%d, ",
				LurDesc->CntEcrMethod,
				(int)LurDesc->CntEcrKeyLength)
				);
			for(keyIdx = 0; keyIdx<LurDesc->CntEcrKeyLength; keyIdx++) {
				KDPrintM(DBG_LURN_INFO, ("%02x ", (int)LurDesc->CntEcrKey[keyIdx]));
				if((keyIdx%4) == 0)
					KDPrintM(DBG_LURN_INFO, (" "));
			}
			KDPrintM(DBG_LURN_INFO, ("\n"));
		}
#endif

	}

	//
	//	Initialize LURNs
	//
	switch(AddTargetData->ucTargetType) {

	case NDASSCSI_TYPE_DVD:

		LurDesc->DevType = LUR_DEVTYPE_ODD;

		LurDesc->Length += sizeof(LURELATION_NODE_DESC) - sizeof(LONG);
		LurDesc->EndingBlockAddr = cur_unitdisk->ulUnitBlocks-1;
		LurDesc->LurnDescCount ++;
		LurTranslateUnitdiskToLurnNodeDesc(	cur_lurn,
									cur_unitdisk,
									(UINT16)0,
                                    0,
									LURN_IDE_ODD,
									0,
									cur_unitdisk->ulUnitBlocks-1,
									cur_unitdisk->ulUnitBlocks,
									cur_unitdisk->UnitMaxDataSendLength,
									cur_unitdisk->UnitMaxDataRecvLength,
									0,
									0
									);
		KDPrintM(DBG_LURN_INFO, ("TYPE : DVD UserID:%08lx Password:%I64x\n",
										cur_lurn->LurnIde.UserID,
										cur_lurn->LurnIde.Password
									));

		//
		//	Set the next available memory location.
		//

		cur_location += sizeof(LURELATION_NODE_DESC) - sizeof(LONG);

		break;
	case NDASSCSI_TYPE_MO:

		LurDesc->DevType = LUR_DEVTYPE_MOD;

		LurDesc->Length += sizeof(LURELATION_NODE_DESC) - sizeof(LONG);
		LurDesc->EndingBlockAddr = cur_unitdisk->ulUnitBlocks-1;
		LurDesc->LurnDescCount ++;
		LurTranslateUnitdiskToLurnNodeDesc(	cur_lurn,
									cur_unitdisk,
									(UINT16)0,
                                    0,
									LURN_IDE_MO,
									0,
									cur_unitdisk->ulUnitBlocks-1,
									cur_unitdisk->ulUnitBlocks,
									cur_unitdisk->UnitMaxDataSendLength,
									cur_unitdisk->UnitMaxDataRecvLength,
									0,
									0
									);
		KDPrintM(DBG_LURN_INFO, ("TYPE : MO UserID:%08lx Password:%I64x\n",
										cur_lurn->LurnIde.UserID,
										cur_lurn->LurnIde.Password
									));
		//
		//	Set the next available memory location.
		//

		cur_location += sizeof(LURELATION_NODE_DESC) - sizeof(LONG);
		break;

	case NDASSCSI_TYPE_DISK_NORMAL:

		LurDesc->DevType = LUR_DEVTYPE_HDD;

		LurDesc->Length += sizeof(LURELATION_NODE_DESC) - sizeof(LONG);
		LurDesc->EndingBlockAddr = cur_unitdisk->ulUnitBlocks-1;
		LurDesc->LurnDescCount ++;
		LurTranslateUnitdiskToLurnNodeDesc(	cur_lurn,
									cur_unitdisk,
									(UINT16)0,
									0,
									LURN_IDE_DISK,
									0,
									cur_unitdisk->ulUnitBlocks-1,
									cur_unitdisk->ulUnitBlocks,
									cur_unitdisk->UnitMaxDataSendLength,
									cur_unitdisk->UnitMaxDataRecvLength,
									0,
									0
									);
		KDPrintM(DBG_LURN_INFO, ("UserID:%08lx Password:%I64x\n",
										cur_lurn->LurnIde.UserID,
										cur_lurn->LurnIde.Password
									));
		//
		//	Set the next available memory location.
		//

		cur_location += sizeof(LURELATION_NODE_DESC) - sizeof(LONG);
		break;
	case NDASSCSI_TYPE_DISK_MIRROR: {
		UINT16					length;
		PLURELATION_NODE_DESC	RootNode;

		LurDesc->DevType = LUR_DEVTYPE_HDD;

		//
		//	Build a LurnDesc for mirroring
		//
		RootNode = cur_lurn;
		length = sizeof(LURELATION_NODE_DESC) + sizeof(LONG);
		LurDesc->Length += length;
		LurDesc->EndingBlockAddr = cur_unitdisk->ulUnitBlocks-1;
		LurDesc->LurnDescCount ++;
		LurTranslateUnitdiskToLurnNodeDesc(	cur_lurn,
									(PNDASBUS_UNITDISK)NULL, //cur_unitdisk,
									(UINT16)((cur_location - (PBYTE)LurDesc) + length),
									0,
									LURN_MIRRORING,
									0,
									cur_unitdisk->ulUnitBlocks-1,
									cur_unitdisk->ulUnitBlocks,
									LurMaxOsRequestLength,
									LurMaxOsRequestLength,
									2,
									0
									);

		//
		//	Build two children
		//
		for(idx_childlurn = 0; idx_childlurn < ChildLurnCnt; idx_childlurn++ ) {

			cur_location += length;
			cur_lurn = (PLURELATION_NODE_DESC)(cur_location);
			cur_unitdisk = AddTargetData->UnitDiskList + idx_childlurn;
			RootNode->LurnChildren[idx_childlurn] = idx_childlurn + 1;
			length = sizeof(LURELATION_NODE_DESC) - sizeof(LONG);
			LurDesc->Length	+= length;
			LurDesc->LurnDescCount ++;
			LurTranslateUnitdiskToLurnNodeDesc(	cur_lurn,
									cur_unitdisk,
									(UINT16)((cur_location - (PBYTE)LurDesc) + length),
									idx_childlurn + 1,
									LURN_IDE_DISK,
									0,
									cur_unitdisk->ulUnitBlocks-1,
									cur_unitdisk->ulUnitBlocks,
									cur_unitdisk->UnitMaxDataSendLength,
									cur_unitdisk->UnitMaxDataRecvLength,
									0,
									0
									);
		}
		//
		//	set 0 offset to the last one.
		//
		cur_lurn->NextOffset = 0;

		//
		//	Set the next available memory location.
		//

		cur_location += length;

		break;
	}
	case NDASSCSI_TYPE_DISK_AGGREGATION:
		{
		UINT16					length;
		PLURELATION_NODE_DESC	RootNode;
		UINT64					StartBlockAddress;
		UINT64					UnitBlocks;

		LurDesc->DevType = LUR_DEVTYPE_HDD;
		//
		//	Build a LurnDesc for aggregation
		//

		RootNode = cur_lurn;
		length = sizeof(LURELATION_NODE_DESC) + sizeof(LONG) * ((UINT16)ChildLurnCnt-1);
		LurDesc->Length += length;
		LurDesc->LurnDescCount ++;
		UnitBlocks = 0;
		for(idx_childlurn = 0; idx_childlurn < ChildLurnCnt; idx_childlurn++ ) {
				UnitBlocks += AddTargetData->UnitDiskList[idx_childlurn].ulUnitBlocks;
		}
		LurDesc->EndingBlockAddr = UnitBlocks - 1;

		LurTranslateUnitdiskToLurnNodeDesc(	cur_lurn,
									(PNDASBUS_UNITDISK)NULL, // cur_unitdisk,
									(UINT16)((cur_location - (PBYTE)LurDesc) + length),
									0,
									LURN_AGGREGATION,
									0,
									UnitBlocks-1,
									UnitBlocks,
									LurMaxOsRequestLength,
									LurMaxOsRequestLength,
									ChildLurnCnt,
									0
									);

		//
		//	Build children
		//
		StartBlockAddress = 0;
		for(idx_childlurn = 0; idx_childlurn < ChildLurnCnt; idx_childlurn++ ) {

			cur_location += length;
			cur_lurn = (PLURELATION_NODE_DESC)(cur_location);
			cur_unitdisk = AddTargetData->UnitDiskList + idx_childlurn;
			LurDesc->LurnDescCount ++;

			//	set a child index to the root.
			RootNode->LurnChildren[idx_childlurn] = idx_childlurn + 1;

			// initialize the child.
			length = sizeof(LURELATION_NODE_DESC) - sizeof(LONG);
			LurDesc->Length += length;
			LurTranslateUnitdiskToLurnNodeDesc(	cur_lurn,
									cur_unitdisk,
									(UINT16)((cur_location - (PBYTE)LurDesc) + length),
									idx_childlurn + 1,
									LURN_IDE_DISK,
									StartBlockAddress,
									StartBlockAddress + cur_unitdisk->ulUnitBlocks-1,
									cur_unitdisk->ulUnitBlocks,
									cur_unitdisk->UnitMaxDataSendLength,
									cur_unitdisk->UnitMaxDataRecvLength,
									0,
									0
									);
			StartBlockAddress += cur_unitdisk->ulUnitBlocks;
		}
		//
		//	set 0 offset to the last one.
		//
		cur_lurn->NextOffset = 0;

		//
		//	Set the next available memory location.
		//

		cur_location += length;
		break;
	}
	case NDASSCSI_TYPE_DISK_RAID0:
	{
		UINT16					length;
		PLURELATION_NODE_DESC	RootNode;
		UINT64					UnitBlocks;

		LurDesc->DevType = LUR_DEVTYPE_HDD;
		// root
		RootNode = cur_lurn;
		length = (UINT16)SIZE_OF_LURELATION_NODE_DESC(ChildLurnCnt);
		LurDesc->Length += length;
		LurDesc->LurnDescCount ++;
		UnitBlocks = 0;

		UnitBlocks = AddTargetData->UnitDiskList[0].ulUnitBlocks;

		for(idx_childlurn = 0; idx_childlurn < ChildLurnCnt; idx_childlurn++ ) {
			if(AddTargetData->UnitDiskList[idx_childlurn].ulUnitBlocks != AddTargetData->UnitDiskList[0].ulUnitBlocks) {
				ASSERT(FALSE);
			}
		}
		LurDesc->EndingBlockAddr = UnitBlocks - 1;

		LurTranslateUnitdiskToLurnNodeDesc(	cur_lurn,
									(PNDASBUS_UNITDISK)NULL, //cur_unitdisk,
									(UINT16)((cur_location - (PBYTE)LurDesc) + length),
									0,
									LURN_RAID0,
									0,
									UnitBlocks-1,
									UnitBlocks,
									LurMaxOsRequestLength,
									LurMaxOsRequestLength,
									ChildLurnCnt,
									0
									);

		for(idx_childlurn = 0; idx_childlurn < ChildLurnCnt; idx_childlurn++)
		{
			cur_location += length;
			cur_lurn = (PLURELATION_NODE_DESC)(cur_location);
			length = (UINT16)SIZE_OF_LURELATION_NODE_DESC(0);
			LurDesc->Length += length;
			LurDesc->LurnDescCount++;
			UnitBlocks = AddTargetData->UnitDiskList[idx_childlurn].ulUnitBlocks;
			cur_unitdisk = &AddTargetData->UnitDiskList[idx_childlurn];

			// set a child index to the root.
			RootNode->LurnChildren[idx_childlurn] = 1 + idx_childlurn;

			LurTranslateUnitdiskToLurnNodeDesc(	cur_lurn,
										cur_unitdisk, // use leaf 0
										(UINT16)((cur_location - (PBYTE)LurDesc) + length),
										RootNode->LurnChildren[idx_childlurn],
										LURN_IDE_DISK,
										0,
										UnitBlocks - 1,
										UnitBlocks,
										cur_unitdisk->UnitMaxDataSendLength,
										cur_unitdisk->UnitMaxDataRecvLength,
										0,
										0
										);

		}

		//
		//	set 0 offset to the last one.
		//
		cur_lurn->NextOffset = 0;
		//
		//	Set the next available memory location.
		//

		cur_location += length;
	}
	break;
	case NDASSCSI_TYPE_DISK_RAID1R3:
		{
			UINT16					length;
			PLURELATION_NODE_DESC	RootNode;
			UINT64					UnitBlocks;

			LurDesc->DevType = LUR_DEVTYPE_HDD;
			// root
			RootNode = cur_lurn;
			length = (UINT16)SIZE_OF_LURELATION_NODE_DESC(ChildLurnCnt);
			LurDesc->Length += length;
			LurDesc->LurnDescCount ++;
			UnitBlocks = 0;
			
			//
			//	The number of blocks remains same as one member disk,
			//	RAID1 R2 increases block size instead.
			//
			UnitBlocks = AddTargetData->UnitDiskList[0].ulUnitBlocks; // RAID1 property

			// every unit disk should have same user space
			for(idx_childlurn = 0; idx_childlurn < ChildLurnCnt; idx_childlurn++ ) {
				if(AddTargetData->UnitDiskList[idx_childlurn].ulUnitBlocks != AddTargetData->UnitDiskList[0].ulUnitBlocks) {
					ASSERT(FALSE);
					return STATUS_INVALID_PARAMETER;
				}
			}
			LurDesc->EndingBlockAddr = UnitBlocks - 1;

			// root
			LurTranslateUnitdiskToLurnNodeDesc(	cur_lurn,
				(PNDASBUS_UNITDISK)NULL, //cur_unitdisk,
				(UINT16)((cur_location - (PBYTE)LurDesc) + length),
				0,
				LURN_RAID1R,
				0,
				UnitBlocks-1,
				UnitBlocks,
				LurMaxOsRequestLength,
				LurMaxOsRequestLength,
				ChildLurnCnt,
				0
				);

			// copy RAID information
			RtlCopyMemory(&RootNode->LurnInfoRAID, &AddTargetData->RAID_Info, sizeof(INFO_RAID));

			// children
			for(idx_childlurn = 0; idx_childlurn < ChildLurnCnt; idx_childlurn++)
			{
				cur_location += length;
				cur_lurn = (PLURELATION_NODE_DESC)(cur_location);
				length = (UINT16)SIZE_OF_LURELATION_NODE_DESC(0);
				LurDesc->Length += length;
				LurDesc->LurnDescCount++;
				UnitBlocks = AddTargetData->UnitDiskList[idx_childlurn].ulUnitBlocks;
				cur_unitdisk = &AddTargetData->UnitDiskList[idx_childlurn];

				// set a child index to the root.
				RootNode->LurnChildren[idx_childlurn] = 1 + idx_childlurn;

				LurTranslateUnitdiskToLurnNodeDesc(	cur_lurn,
					cur_unitdisk, // use leaf 0
					(UINT16)((cur_location - (PBYTE)LurDesc) + length),
					RootNode->LurnChildren[idx_childlurn],
					LURN_IDE_DISK,
					0,
					UnitBlocks - 1,
					UnitBlocks,
					cur_unitdisk->UnitMaxDataSendLength,
					cur_unitdisk->UnitMaxDataRecvLength,
					0,
					0
					);
			}

			//
			//	set 0 offset to the last one.
			//
			cur_lurn->NextOffset = 0;

			//
			//	Set the next available memory location.
			//

			cur_location += length;
		}
		break;

	case NDASSCSI_TYPE_DISK_RAID4R3:
	{
		UINT16					length;
		PLURELATION_NODE_DESC	RootNode;
		UINT64					UnitBlocks;

		LurDesc->DevType = LUR_DEVTYPE_HDD;

		// root
		RootNode = cur_lurn;
		length = (UINT16)SIZE_OF_LURELATION_NODE_DESC(ChildLurnCnt);
		LurDesc->Length += length;
		LurDesc->LurnDescCount ++;
		UnitBlocks = 0;

		UnitBlocks = AddTargetData->UnitDiskList[0].ulUnitBlocks; // RAID4 property

		// every unit disk should have same user space
		for(idx_childlurn = 0; idx_childlurn < ChildLurnCnt; idx_childlurn++ ) {
			if(AddTargetData->UnitDiskList[idx_childlurn].ulUnitBlocks != AddTargetData->UnitDiskList[0].ulUnitBlocks)
				ASSERT(FALSE);
		}
		LurDesc->EndingBlockAddr = UnitBlocks - 1;

		// root
		LurTranslateUnitdiskToLurnNodeDesc(	cur_lurn,
									(PNDASBUS_UNITDISK)NULL, //cur_unitdisk,
									(UINT16)((cur_location - (PBYTE)LurDesc) + length),
									0,
									LURN_RAID4R,
									0,
									UnitBlocks-1,
									UnitBlocks,
									LurMaxOsRequestLength,
									LurMaxOsRequestLength,
									ChildLurnCnt,
									0
									);

		// copy RAID information
		RtlCopyMemory(&RootNode->LurnInfoRAID, &AddTargetData->RAID_Info, sizeof(INFO_RAID));

		// children
		for(idx_childlurn = 0; idx_childlurn < ChildLurnCnt; idx_childlurn++)
		{
			cur_location += length;
			cur_lurn = (PLURELATION_NODE_DESC)(cur_location);
			length = (UINT16)SIZE_OF_LURELATION_NODE_DESC(0);
			LurDesc->Length += length;
			LurDesc->LurnDescCount++;
			UnitBlocks = AddTargetData->UnitDiskList[idx_childlurn].ulUnitBlocks;
			cur_unitdisk = &AddTargetData->UnitDiskList[idx_childlurn];

			// set a child index to the root.
			RootNode->LurnChildren[idx_childlurn] = 1 + idx_childlurn;

			LurTranslateUnitdiskToLurnNodeDesc(	cur_lurn,
										cur_unitdisk, // use leaf 0
										(UINT16)((cur_location - (PBYTE)LurDesc) + length),
										RootNode->LurnChildren[idx_childlurn],
										LURN_IDE_DISK,
										0,
										UnitBlocks - 1,
										UnitBlocks,
										cur_unitdisk->UnitMaxDataSendLength,
										cur_unitdisk->UnitMaxDataRecvLength,
										0,
										0
										);
		}

		//
		//	set 0 offset to the last one.
		//
		cur_lurn->NextOffset = 0;
		//
		//	Set the next available memory location.
		//

		cur_location += length;
	}
	break;
	default:
		return STATUS_INVALID_PARAMETER;
	}

	//
	//	Copy NDAS block ACL
	//

	if(AddTargetData->BACLOffset) {
		PNDAS_BLOCK_ACL	ndasBlockAcl;

		ndasBlockAcl = (PNDAS_BLOCK_ACL)((PUCHAR)AddTargetData + AddTargetData->BACLOffset);
		RtlCopyMemory(cur_location, ndasBlockAcl, ndasBlockAcl->Length);
		LurDesc->BACLOffset = (ULONG)(ULONG_PTR)((PUCHAR)cur_location - (PUCHAR)LurDesc);
		LurDesc->BACLLength = ndasBlockAcl->Length;
	} else {
		LurDesc->BACLOffset = 0;
		LurDesc->BACLLength = 0;
	}

	return STATUS_SUCCESS;
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
					&LurnAggrInterface,
					NULL,					// Legacy Mirror
					&LurnIdeDiskInterface,
					&LurnIdeODDInterface,
					&LurnIdeMOInterface,
					NULL,					// Legacy RAID1
					NULL,					// Legacy RAID4
					&LurnRAID0Interface,
					NULL,
					&LurnRAID1RInterface,
					NULL, 					// &LurnRAID4RInterface, RAID4 is temporarily disabled.
			};


static
void
LurnSetDefaultConfiguration(
	PLURELATION_NODE	Lurn
) {
	Lurn->ReconnTrial		= RECONNECTION_MAX_TRY;
	Lurn->ReconnInterval	= MAX_RECONNECTION_INTERVAL;
}


static
void
LurnModifyConfiguration(
	PLURELATION_NODE		Lurn,
	PLURELATION_NODE_DESC	LurDesc
) {

	if(LurDesc->LurnOptions & LURNOPTION_SET_RECONNECTION) {
		Lurn->ReconnTrial		= LurDesc->ReconnTrial;
		Lurn->ReconnInterval	= (UINT64)LurDesc->ReconnInterval * 10000;
	}

}


NTSTATUS
LurnInitialize(
		PLURELATION_NODE		Lurn,
		PLURELATION				Lur,
		PLURELATION_NODE_DESC	LurnDesc
	) {
	NTSTATUS			status;

	KDPrintM(DBG_LURN_ERROR, ("In. Lurn : %p, Lur : %p, LurnDesc : %p\n", Lurn, Lur, LurnDesc));

	ASSERT(Lurn);
	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	if(Lur && LurnDesc)
	{
		// normal initialize

		if(LurnDesc->LurnType >= LurnInterfaceCnt ) {
			KDPrintM(DBG_LURN_ERROR, ("Error in Lurn Type %x\n", LurnDesc->LurnType));
			ASSERT(FALSE);
			return STATUS_INVALID_PARAMETER;
		}

		//
		//	set default values.
		//	Do not zero pointers to children.
		//
		KeInitializeSpinLock(&Lurn->LurnSpinLock);
		Lurn->LurnType = LurnDesc->LurnType;
		Lurn->Lur = Lur;
		Lurn->LurnChildrenCnt = LurnDesc->LurnChildrenCnt;
		Lurn->LurnInterface = LurnInterfaceList[LurnDesc->LurnType];

		Lurn->StartBlockAddr = LurnDesc->StartBlockAddr;
		Lurn->EndBlockAddr = LurnDesc->EndBlockAddr;
		Lurn->UnitBlocks = LurnDesc->UnitBlocks;
		Lurn->AccessRight = LurnDesc->AccessRight;

		LurnResetFaultInfo(Lurn);
		LurnSetDefaultConfiguration(Lurn);
		LurnModifyConfiguration(Lurn, LurnDesc);
	} else {
		// revive mode
		KDPrintM(DBG_LURN_ERROR, ("Revive Lurn : %08x\n", Lurn));
	}

	if(!Lurn->LurnInterface->LurnFunc.LurnInitialize) {
		if(LurnDesc) {
			KDPrintM(DBG_LURN_ERROR, ("Lurntype %x interface not implements LurnInitialize furnction\n", LurnDesc->LurnType ));
		} else {
			KDPrintM(DBG_LURN_ERROR, ("Unknown lurntype interface not implements LurnInitialize furnction\n"));
		}
		return STATUS_NOT_IMPLEMENTED;
	}

	if (LurnDesc && (LurnDesc->LurnOptions & LURNOPTION_MISSING)) {
		//
		// If this is missing LURN, don't initialize right now. 
		// It may be online, but it is not correctly configured as RAID member.
		// Let RAID handle it later.
		//
		ULONG					lurndesc_len;
		PLURELATION_NODE_DESC	lurnDesc;

		ASSERT(Lurn->LurnType == LURN_IDE_DISK);
		//
		//	Save LURN Descriptor.
		//

		lurndesc_len = FIELD_OFFSET(LURELATION_NODE_DESC, LurnChildren) +
			LurnDesc->LurnChildrenCnt * sizeof(ULONG);

		lurnDesc = ExAllocatePoolWithTag(
									NonPagedPool,
									lurndesc_len,
									LURN_DESC_POOL_TAG);
		if(lurnDesc == NULL) {
			KDPrintM(DBG_LURN_ERROR, ("ExAllocatePoolWithTag() failed.\n"));
			return STATUS_INSUFFICIENT_RESOURCES;
		} else {
			RtlCopyMemory(lurnDesc, LurnDesc, lurndesc_len);
			ASSERT(Lurn->SavedLurnDesc == NULL);
			Lurn->SavedLurnDesc = lurnDesc;
			Lurn->SavedLurnDesc->LurnOptions &= ~LURNOPTION_MISSING;
			KDPrintM(DBG_LURN_ERROR, ("Missing LURN. Saved pointer the lurn desc:%p\n", Lurn->SavedLurnDesc));
			return STATUS_SUCCESS;
		}
	} 
	
	status = Lurn->LurnInterface->LurnFunc.LurnInitialize(
					Lurn,
					LurnDesc
				);
	if(NT_SUCCESS(status)) {
		Lurn->LurnStatus = LURN_STATUS_RUNNING;
		if (Lurn->SavedLurnDesc) {
			// LurnDesc is not required once it is initialized
			ExFreePoolWithTag(Lurn->SavedLurnDesc, LURN_DESC_POOL_TAG);
			Lurn->SavedLurnDesc = NULL;
		}
	}

	KDPrintM(DBG_LURN_ERROR, ("return 0x%08lx : Idx:%d\n", status, Lurn->LurnId));

	return status;
}


NTSTATUS
LurnDestroy(
		PLURELATION_NODE Lurn
	) {
	NTSTATUS ntStatus;

	KDPrintM(DBG_LURN_ERROR, ("In. Lurn %p\n", Lurn));

	ASSERT(Lurn);
	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	if(!Lurn->LurnInterface) {
		KDPrintM(DBG_LURN_ERROR, ("LURN->LurnInterface NULL!\n"));
		return STATUS_INVALID_PARAMETER;
	}
	if(!Lurn->LurnInterface->LurnFunc.LurnDestroy) {
		return STATUS_NOT_IMPLEMENTED;
	}

	ntStatus = Lurn->LurnInterface->LurnFunc.LurnDestroy(
					Lurn
				);
	ASSERT(NT_SUCCESS(ntStatus));
	Lurn->LurnStatus =  LURN_STATUS_DESTROYING;

	if (Lurn->SavedLurnDesc) {
		ExFreePoolWithTag(Lurn->SavedLurnDesc, LURN_DESC_POOL_TAG);
		Lurn->SavedLurnDesc = NULL;
	}

	return ntStatus;
}


NTSTATUS
LurnRequest(
		PLURELATION_NODE Lurn,
		PCCB Ccb
	) {
	NTSTATUS	status;

	ASSERT(Lurn);
	ASSERT(Ccb->CcbCurrentStackLocation == &Ccb->CcbStackLocation[Ccb->CcbCurrentStackLocationIndex]);

	if(!Lurn->LurnInterface) {
		KDPrintM(DBG_LURN_ERROR, ("LURN->LurnInterface NULL!\n"));
		return STATUS_INVALID_PARAMETER;
	}
	if(!Lurn->LurnInterface->LurnFunc.LurnRequest) {
		return STATUS_NOT_IMPLEMENTED;
	}

	Ccb->CcbCurrentStackLocation--;
	Ccb->CcbCurrentStackLocationIndex--;
	Ccb->CcbCurrentStackLocation->Lurn = Lurn;

	if(Ccb->CcbCurrentStackLocationIndex < 0) {
		return STATUS_DATA_OVERRUN;
	}

	status = Lurn->LurnInterface->LurnFunc.LurnRequest(
					Lurn,
					Ccb
				);

	return status;
}



NTSTATUS
LurnAllocate(
		PLURELATION_NODE	*Lurn,
		LONG				ChildrenCnt
	) {
	ULONG				length;
	PLURELATION_NODE	lurn;

	length = sizeof(LURELATION_NODE) + (ChildrenCnt-1) * sizeof(PLURELATION_NODE);
	lurn = ExAllocatePoolWithTag(
					NonPagedPool,
					length,
					LURN_POOL_TAG
				);
	if(!lurn) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	RtlZeroMemory(lurn, length);
	lurn->Length = (UINT16)length;
	lurn->Type = LSSTRUC_TYPE_LURN;

	*Lurn = lurn;

	return STATUS_SUCCESS;

}


VOID
LurnFree(
	PLURELATION_NODE Lurn
) {
	ExFreePoolWithTag(
		Lurn,
		LURN_POOL_TAG
    );

}

//////////////////////////////////////////////////////////////////////////
//
//	LURN interface default functions
//
NTSTATUS
LurnInitializeDefault(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
	) {

	UNREFERENCED_PARAMETER(Lurn);
	UNREFERENCED_PARAMETER(LurnDesc);

	return STATUS_SUCCESS;
}

NTSTATUS
LurnDestroyDefault(
		PLURELATION_NODE Lurn
	) {

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	if(Lurn->LurnExtension) {
		ExFreePoolWithTag(Lurn->LurnExtension, LURNEXT_POOL_TAG);
		Lurn->LurnExtension = NULL;
	}
	if(Lurn->SavedLurnDesc) {
		ExFreePoolWithTag(Lurn->SavedLurnDesc, LURN_DESC_POOL_TAG);
		Lurn->SavedLurnDesc = NULL;
	}

	return STATUS_SUCCESS;
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

