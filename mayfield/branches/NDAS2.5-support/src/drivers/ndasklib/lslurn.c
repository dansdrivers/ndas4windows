#include <ntddk.h>
//#include "public.h"
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
SendStopCcbToLurn(
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

	LSCCB_INITIALIZE(ccb, 0);
	ccb->OperationCode				= CCB_OPCODE_STOP;
	ccb->HwDeviceExtension			= NULL;
	LSCcbSetFlag(ccb, CCB_FLAG_SYNCHRONOUS|CCB_FLAG_ALLOCATED|CCB_FLAG_LOWER_LURN);
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


static
void
LurSetDefaultConfiguration(
	PLURELATION	Lur,
	UINT32		DefaultLurFlags
) {
	Lur->LurFlags = DefaultLurFlags;
}


static
void
LurModifyConfiguration(
	PLURELATION			Lur,
	PLURELATION_DESC	LurDesc
) {

	//
	//	Write share mode/Primary-secondary
	//
	if(	(LurDesc->LurOptions & LUROPTION_OFF_WRITESHARE_PS) &&
		(LurDesc->LurOptions & LUROPTION_ON_WRITESHARE_PS) ) {

			return;
	}

	if(LurDesc->LurOptions & LUROPTION_OFF_WRITESHARE_PS) {
		Lur->LurFlags &= ~LURFLAG_WRITESHARE_PS;
	} else if(LurDesc->LurOptions & LUROPTION_ON_WRITESHARE_PS) {
		Lur->LurFlags |= LURFLAG_WRITESHARE_PS;
	}


	//
	//	Fake write mode
	//

	if(	(LurDesc->LurOptions & LUROPTION_OFF_FAKEWRITE) &&
		(LurDesc->LurOptions & LUROPTION_ON_FAKEWRITE) ) {

			return;
		}
	if(LurDesc->LurOptions & LUROPTION_OFF_FAKEWRITE) {
		Lur->LurFlags &= ~LURFLAG_FAKEWRITE;
	} else if(LurDesc->LurOptions & LUROPTION_ON_FAKEWRITE) {
		Lur->LurFlags |= LURFLAG_FAKEWRITE;
	}


	//
	//	Locked write mode
	//

	if(	(LurDesc->LurOptions & LUROPTION_OFF_LOCKEDWRITE) &&
		(LurDesc->LurOptions & LUROPTION_ON_LOCKEDWRITE) ) {

			return;
	}

	if(LurDesc->LurOptions & LUROPTION_OFF_LOCKEDWRITE) {
		Lur->LurFlags &= ~LURFLAG_LOCKEDWRITE;
	} else if(LurDesc->LurOptions & LUROPTION_ON_LOCKEDWRITE) {
		Lur->LurFlags |= LURFLAG_LOCKEDWRITE;
	}


	//
	//	Fully shared write mode
	//

	if(	(LurDesc->LurOptions & LUROPTION_OFF_SHAREDWRITE) &&
		(LurDesc->LurOptions & LUROPTION_ON_SHAREDWRITE) ) {

			return;
	}

	if(LurDesc->LurOptions & LUROPTION_OFF_SHAREDWRITE) {
		Lur->LurFlags &= ~LURFLAG_SHAREDWRITE;
	} else if(LurDesc->LurOptions & LUROPTION_ON_SHAREDWRITE) {
		Lur->LurFlags |= LURFLAG_SHAREDWRITE;
	}

	//
	//	Shared out-of-band write mode
	//

	if(	(LurDesc->LurOptions & LUROPTION_OFF_OOB_SHAREDWRITE) &&
		(LurDesc->LurOptions & LUROPTION_ON_OOB_SHAREDWRITE) ) {

			return;
	}

	if(LurDesc->LurOptions & LUROPTION_OFF_OOB_SHAREDWRITE) {
		Lur->LurFlags &= ~LURFLAG_OOB_SHAREDWRITE;
	} else if(LurDesc->LurOptions & LUROPTION_ON_OOB_SHAREDWRITE) {
		Lur->LurFlags |= LURFLAG_OOB_SHAREDWRITE;
	}

	if(LurDesc->LurOptions & LUROPTION_OFF_NDAS_2_0_WRITE_CHECK) {
		Lur->LurFlags &= ~LURFLAG_NDAS_2_0_WRITE_CHECK;
	} else if(LurDesc->LurOptions & LUROPTION_ON_NDAS_2_0_WRITE_CHECK) {
		Lur->LurFlags |= LURFLAG_NDAS_2_0_WRITE_CHECK;
	}

	if(LurDesc->LurOptions & LUROPTION_OFF_DYNAMIC_REQUEST_SIZE) {
		Lur->LurFlags &= ~LURFLAG_DYNAMIC_REQUEST_SIZE;
	} else if(LurDesc->LurOptions & LUROPTION_ON_DYNAMIC_REQUEST_SIZE) {
		Lur->LurFlags |= LURFLAG_DYNAMIC_REQUEST_SIZE;
	}



	//
	//	Lur flags integration check
	//
	//	LURFLAG_SHAREDWRITE and LURFLAG_WRITESHARE_PS are mutual-exclusive.
	//	LURFLAG_SHAREDWRITE and LURFLAG_OOB_SHAREDWRITE are mutual-exclusive.
	//

	if(Lur->LurFlags & LURFLAG_SHAREDWRITE) {
		ASSERT(!(Lur->LurFlags & LURFLAG_FAKEWRITE));
		ASSERT(!(Lur->LurFlags & LURFLAG_OOB_SHAREDWRITE));
		ASSERT(Lur->LurFlags & LURFLAG_LOCKEDWRITE);
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
}


NTSTATUS
LurCreate(
		IN PLURELATION_DESC		LurDesc,
		IN UINT32				DefaultLurFlags,
		OUT PLURELATION			*Lur,
		IN ACCESS_MASK			InitAccessMode,
		IN PVOID				AdapterFdo,
		IN LURN_EVENT_CALLBACK	LurnEventCallback
	) {
	LONG					idx_lurn;
	LONG					idx_child;
	PLURELATION_NODE_DESC	cur_lurndesc;
	PLURELATION_NODE		cur_lurn;
	LURNDESC_ENTRY			lurndesc_table[LUR_MAX_LURNS_PER_LUR];
	NTSTATUS				status;
	ULONG					child;
	PLURELATION				tmpLur;
	ULONG					tmpLurLength;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);
	ASSERT(LurDesc->LurnDescCount > 0);
	ASSERT(LurDesc->LurnDescCount <= LUR_MAX_LURNS_PER_LUR);

	if(LurDesc->LurnDescCount <= 0) {
		KDPrintM(DBG_LURN_ERROR, ("No child node.\n"));
		return STATUS_INVALID_PARAMETER;
	}

	RtlZeroMemory(lurndesc_table, sizeof(LURNDESC_ENTRY) * LUR_MAX_LURNS_PER_LUR);
	tmpLur = NULL;

	//
	//	create LURELATION
	//
	tmpLurLength = sizeof(LURELATION) + sizeof(PLURELATION_NODE) * (LurDesc->LurnDescCount - 1);
	tmpLur = (PLURELATION)ExAllocatePoolWithTag(NonPagedPool, tmpLurLength, LUR_POOL_TAG);
	if(!tmpLur) {
		KDPrintM(DBG_LURN_ERROR, ("ExAllocatePoolWithTag() failed.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	tmpLur->Type				= LSSTRUC_TYPE_LUR;
	tmpLur->Length				= sizeof(LURELATION);
	tmpLur->MaxBlocksPerRequest = LurDesc->MaxBlocksPerRequest;
	tmpLur->DesiredAccess		= tmpLur->GrantedAccess = LurDesc->AccessRight;

	//
	//	Override init access mode
	//
	if(InitAccessMode) {
		tmpLur->GrantedAccess = InitAccessMode;
	}

	tmpLur->DevType				= LurDesc->DevType;
	tmpLur->DevSubtype			= LurDesc->DevSubtype;
	tmpLur->NodeCount			= LurDesc->LurnDescCount;
	RtlCopyMemory(tmpLur->LurId, LurDesc->LurId, LURID_LENGTH);

	tmpLur->AdapterFdo			= AdapterFdo;
	tmpLur->LurnEventCallback	= LurnEventCallback;

	//
	//	Set LurFlags to default values or values set by a user.
	//
	LurSetDefaultConfiguration(tmpLur, DefaultLurFlags);
	LurModifyConfiguration(tmpLur, LurDesc);

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
	//	Allocate LURNs to Lurndesc Table and sanity check.
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
		//	Access right
		//
		cur_lurndesc->AccessRight = tmpLur->GrantedAccess;

		//
		// Options
		//

		if(cur_lurndesc->LurnOptions & LURNOPTION_RESTRICT_UDMA) {
			cur_lurn->UDMARestrictValid = TRUE;
			cur_lurn->UDMARestrict = cur_lurndesc->UDMARestrict;
		} else {
			cur_lurn->UDMARestrictValid = FALSE;
		}

		//
		//	Initialize the LURN
		//
		ASSERT(cur_lurn->LurnDesc == NULL);

		status = LurnInitialize(cur_lurn, tmpLur, cur_lurndesc);
		if(cur_lurndesc->LurnOptions & LURNOPTION_MISSING &&
			!NT_SUCCESS(status)) {
			ULONG					lurndesc_len;
			PLURELATION_NODE_DESC	lurnDesc;
			

			//
			//	Save LURN Descriptor.
			//

			lurndesc_len = FIELD_OFFSET(LURELATION_NODE_DESC, LurnChildren) +
				cur_lurndesc->LurnChildrenCnt * sizeof(ULONG);

			lurnDesc = ExAllocatePoolWithTag(
										NonPagedPool,
										lurndesc_len,
										LURN_POOL_TAG);
			if(lurnDesc == NULL) {
				KDPrintM(DBG_LURN_ERROR, ("ExAllocatePoolWithTag() failed.\n"));
			} else {
				//
				//	Destroy the LURN to initialize later.
				//

				LurnDestroy(cur_lurn);

				RtlCopyMemory(lurnDesc, cur_lurndesc, lurndesc_len);

				KDPrintM(DBG_LURN_ERROR, ("LURELATION_NODE_DESC #%u copied.\n", cur_lurn->LurnId));

				cur_lurn->LurnDesc = lurnDesc;

				KDPrintM(DBG_LURN_ERROR, ("Saved pointer the lurn desc:%p\n", cur_lurn->LurnDesc));
				continue;
			}

		}

		if((LURN_IDE_ODD != cur_lurndesc->LurnType) 
			&& (LURN_IDE_MO != cur_lurndesc->LurnType) ){


			if(tmpLur->LurFlags & LURFLAG_WRITESHARE_PS) {

				if(status == STATUS_ACCESS_DENIED && (tmpLur->GrantedAccess & GENERIC_WRITE)) {
						LONG	idx_closinglurn;

					if(LurDesc->LurOptions & LUROPTION_OFF_FAKEWRITE) {
						KDPrintM(DBG_LURN_ERROR, ("LUROPTION_OFF_FAKEWRITE is set. Can not go to Secondary mode.\n"));
					} else if(LurDesc->LurOptions & LUROPTION_DONOT_ADJUST_ACCESSMODE) {
						KDPrintM(DBG_LURN_ERROR, ("DONOT_ADJUST_ACCESSMODE is set. Can not go to Secondary mode.\n"));
					} else {
						for(idx_closinglurn = idx_lurn; idx_closinglurn < (LONG)LurDesc->LurnDescCount; idx_closinglurn++) {
							SendStopCcbToLurn(lurndesc_table[idx_closinglurn].Lurn);
							LurnDestroy(lurndesc_table[idx_closinglurn].Lurn);
						}

						KDPrintM(DBG_LURN_ERROR, ("Degrade access right and retry.\n"));
						tmpLur->GrantedAccess &= ~GENERIC_WRITE;

						if(!(tmpLur->LurFlags & LURFLAG_SHAREDWRITE))
							tmpLur->LurFlags |= LURFLAG_FAKEWRITE;

						goto retry_init;
					}
				}
			}
		}
		if(!NT_SUCCESS(status)) {
			LONG	idx_closinglurn;

			KDPrintM(DBG_LURN_ERROR, ("LurnInitialize() failed. LURN#:%d NTSTATUS:%08lx\n", idx_lurn, status));
			for(idx_closinglurn = idx_lurn; idx_closinglurn < (LONG)LurDesc->LurnDescCount; idx_closinglurn++) {
				SendStopCcbToLurn(lurndesc_table[idx_closinglurn].Lurn);
				LurnDestroy(lurndesc_table[idx_closinglurn].Lurn);
			}
			goto error_out;
		}
	}

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
				SendStopCcbToLurn(lurn);
				LurnDestroy(lurn);
			}
			LurnFree(lurn);
			lurndesc_table[idx_lurn].Lurn = NULL;
		}
	}

	if(tmpLur) {
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

	ExFreePoolWithTag(Lur, LUR_POOL_TAG);
}

NTSTATUS
LurRequest(
		PLURELATION			Lur,
		PCCB				Ccb
	) {

	if(!Lur) {
		KDPrintM(DBG_LURN_ERROR, ("==============> LUR pointer NULL!\n"));
		return STATUS_INVALID_PARAMETER;
	}
	if(Lur->NodeCount <= 0) {
		KDPrintM(DBG_LURN_ERROR, ("==============> No LURN available.\n"));
		return STATUS_INVALID_PARAMETER;
	}
	return LurnRequest(LUR_GETROOTNODE(Lur),Ccb);
}

//////////////////////////////////////////////////////////////////////////
//
//	Translate AddTargetData to LUR descriptor.
//

#define TRANSLATE_UNITDISK_TO_LURN(LURN_POINTER, UNITDISK_POINER, NEXTOFFSET, LURNID, LURNTYPE, STARTADDR, ENDADDR, UNITBLOCKS, MAXBLOCKREQ, BYTESINBLOCK, CHILDCNT, PARENT) {	\
		(LURN_POINTER)->NextOffset			= (UINT16)(NEXTOFFSET);															\
		(LURN_POINTER)->LurnId				= (LURNID);																\
		(LURN_POINTER)->LurnType			= (LURNTYPE);															\
		(LURN_POINTER)->AccessRight			= 0;																	\
		(LURN_POINTER)->StartBlockAddr		= (STARTADDR);															\
		(LURN_POINTER)->EndBlockAddr		= (ENDADDR);															\
		(LURN_POINTER)->UnitBlocks			= (UNITBLOCKS);															\
		(LURN_POINTER)->MaxBlocksPerRequest	= (MAXBLOCKREQ);														\
		(LURN_POINTER)->BytesInBlock		= (BYTESINBLOCK);														\
		(LURN_POINTER)->LurnOptions			= (UNITDISK_POINER)->LurnOptions;										\
		(LURN_POINTER)->UDMARestrict		= (UNITDISK_POINER)->UDMARestrict;										\
		(LURN_POINTER)->ReconnTrial			= (UNITDISK_POINER)->ReconnTrial;										\
		(LURN_POINTER)->ReconnInterval		= (UNITDISK_POINER)->ReconnInterval;									\
		LSTRANS_COPY_LPXADDRESS(&(LURN_POINTER)->LurnIde.BindingAddress, &(UNITDISK_POINER)->NICAddr);				\
		LSTRANS_COPY_LPXADDRESS(&(LURN_POINTER)->LurnIde.TargetAddress, &(UNITDISK_POINER)->Address);				\
		RtlCopyMemory(&(LURN_POINTER)->LurnIde.UserID, &(UNITDISK_POINER)->iUserID, LSPROTO_USERID_LENGTH);			\
		RtlCopyMemory(&(LURN_POINTER)->LurnIde.Password, &(UNITDISK_POINER)->iPassword, LSPROTO_PASSWORD_LENGTH);	\
		RtlCopyMemory((PVOID)(LURN_POINTER)->LurnIde.Password_v2, (PVOID)(UNITDISK_POINER)->iPassword_v2, LSPROTO_PASSWORD_V2_LENGTH);	\
		(LURN_POINTER)->LurnIde.HWType		= (UNITDISK_POINER)->ucHWType;											\
		(LURN_POINTER)->LurnIde.HWVersion	= (UNITDISK_POINER)->ucHWVersion;										\
		(LURN_POINTER)->LurnIde.HWRevision	= (UNITDISK_POINER)->ucHWRevision;										\
		LSTRANS_COPY_LPXADDRESS(&(LURN_POINTER)->LurnIde.TargetAddress, &(UNITDISK_POINER)->Address);				\
		(LURN_POINTER)->LurnIde.LanscsiTargetID = (UNITDISK_POINER)->ucUnitNumber;									\
		(LURN_POINTER)->LurnIde.LanscsiLU	= 0;																	\
		(LURN_POINTER)->LurnChildrenCnt		= (CHILDCNT);															\
		(LURN_POINTER)->LurnParent			= (PARENT); }


NTSTATUS
LurTranslateAddTargetDataToLURDesc(
	   PLANSCSI_ADD_TARGET_DATA	AddTargetData,
	   ULONG					LurMaxRequestBlocks,
	   LONG						LurDescLengh,
	   PLURELATION_DESC			LurDesc
	) {
	LONG						ChildLurnCnt;
	LONG						idx_childlurn;
	PBYTE						cur_location;
	PLURELATION_NODE_DESC		cur_lurn;
	PLSBUS_UNITDISK				cur_unitdisk;


	UNREFERENCED_PARAMETER(LurDescLengh);

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
	LurDesc->Length = sizeof(LURELATION_DESC);
	LurDesc->LurOptions = AddTargetData->LurOptions;
	LurDesc->LurId[0] = 0;
	LurDesc->LurId[1] = 0;
	LurDesc->LurId[2] = 0;
	LurDesc->MaxBlocksPerRequest = LurMaxRequestBlocks;
	LurDesc->LurnDescCount = 0;
	LurDesc->AccessRight = AddTargetData->DesiredAccess;
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
		LurDesc->LurnDescCount ++;
		TRANSLATE_UNITDISK_TO_LURN(	cur_lurn,
									cur_unitdisk,
									0,
                                    0,
									LURN_IDE_ODD,
									0,
									cur_unitdisk->ulUnitBlocks-1,
									cur_unitdisk->ulUnitBlocks,
									cur_unitdisk->UnitMaxRequestBlocks,
									BLOCK_SIZE,
									0,
									0
									);
		KDPrintM(DBG_LURN_INFO, ("TYPE : DVD UserID:%08lx Password:%I64x\n",
										cur_lurn->LurnIde.UserID,
										cur_lurn->LurnIde.Password
									));

		break;
	case NDASSCSI_TYPE_MO:

		LurDesc->DevType = LUR_DEVTYPE_MOD;

		LurDesc->Length += sizeof(LURELATION_NODE_DESC) - sizeof(LONG);
		LurDesc->LurnDescCount ++;
		TRANSLATE_UNITDISK_TO_LURN(	cur_lurn,
									cur_unitdisk,
									0,
                                    0,
									LURN_IDE_MO,
									0,
									cur_unitdisk->ulUnitBlocks-1,
									cur_unitdisk->ulUnitBlocks,
									cur_unitdisk->UnitMaxRequestBlocks,
									BLOCK_SIZE,
									0,
									0
									);
		KDPrintM(DBG_LURN_INFO, ("TYPE : MO UserID:%08lx Password:%I64x\n",
										cur_lurn->LurnIde.UserID,
										cur_lurn->LurnIde.Password
									));
		break;

	case NDASSCSI_TYPE_DISK_NORMAL:

		LurDesc->DevType = LUR_DEVTYPE_HDD;

		LurDesc->Length += sizeof(LURELATION_NODE_DESC) - sizeof(LONG);
		LurDesc->LurnDescCount ++;
		TRANSLATE_UNITDISK_TO_LURN(	cur_lurn,
									cur_unitdisk,
									0,
									0,
									LURN_IDE_DISK,
									0,
									cur_unitdisk->ulUnitBlocks-1,
									cur_unitdisk->ulUnitBlocks,
									cur_unitdisk->UnitMaxRequestBlocks,
									BLOCK_SIZE,
									0,
									0
									);
		KDPrintM(DBG_LURN_INFO, ("UserID:%08lx Password:%I64x\n",
										cur_lurn->LurnIde.UserID,
										cur_lurn->LurnIde.Password
									));
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
		LurDesc->LurnDescCount ++;
		TRANSLATE_UNITDISK_TO_LURN(	cur_lurn,
									cur_unitdisk,
									(cur_location - (PBYTE)LurDesc) + length,
									0,
									LURN_MIRRORING,
									0,
									cur_unitdisk->ulUnitBlocks-1,
									cur_unitdisk->ulUnitBlocks,
									LurMaxRequestBlocks,
									BLOCK_SIZE,
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
			TRANSLATE_UNITDISK_TO_LURN(	cur_lurn,
									cur_unitdisk,
									(cur_location - (PBYTE)LurDesc) + length,
									idx_childlurn + 1,
									LURN_IDE_DISK,
									0,
									cur_unitdisk->ulUnitBlocks-1,
									cur_unitdisk->ulUnitBlocks,
									cur_unitdisk->UnitMaxRequestBlocks,
									BLOCK_SIZE,
									0,
									0
									);
		}
		//
		//	set 0 offset to the last one.
		//
		cur_lurn->NextOffset = 0;
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
		TRANSLATE_UNITDISK_TO_LURN(	cur_lurn,
									cur_unitdisk,
									(cur_location - (PBYTE)LurDesc) + length,
									0,
									LURN_AGGREGATION,
									0,
									UnitBlocks-1,
									UnitBlocks,
									LurMaxRequestBlocks,
									BLOCK_SIZE,
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
			TRANSLATE_UNITDISK_TO_LURN(	cur_lurn,
									cur_unitdisk,
									(cur_location - (PBYTE)LurDesc) + length,
									idx_childlurn + 1,
									LURN_IDE_DISK,
									StartBlockAddress,
									StartBlockAddress + cur_unitdisk->ulUnitBlocks-1,
									cur_unitdisk->ulUnitBlocks,
									cur_unitdisk->UnitMaxRequestBlocks,
									BLOCK_SIZE,
									0,
									0
									);
			StartBlockAddress += cur_unitdisk->ulUnitBlocks;
		}
		//
		//	set 0 offset to the last one.
		//
		cur_lurn->NextOffset = 0;
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
			if(AddTargetData->UnitDiskList[idx_childlurn].ulUnitBlocks != AddTargetData->UnitDiskList[0].ulUnitBlocks)
				ASSERT(FALSE);
		}

		TRANSLATE_UNITDISK_TO_LURN(	cur_lurn,
									cur_unitdisk,
									(cur_location - (PBYTE)LurDesc) + length,
									0,
									LURN_RAID0,
									0,
									UnitBlocks-1,
									UnitBlocks,
									LurMaxRequestBlocks,
									BLOCK_SIZE,
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

			TRANSLATE_UNITDISK_TO_LURN(	cur_lurn,
										cur_unitdisk, // use leaf 0
										(cur_location - (PBYTE)LurDesc) + length,
										RootNode->LurnChildren[idx_childlurn],
										LURN_IDE_DISK,
										0,
										UnitBlocks - 1,
										UnitBlocks,
										cur_unitdisk->UnitMaxRequestBlocks,
										BLOCK_SIZE,
										0,
										0
										);

		}

		//
		//	set 0 offset to the last one.
		//
		cur_lurn->NextOffset = 0;
	}
	break;
	case NDASSCSI_TYPE_DISK_RAID1R:
		{
			UINT16					length;
			PLURELATION_NODE_DESC	RootNode;
			UINT64					UnitBlocks;

			// root
			RootNode = cur_lurn;
			length = (UINT16)SIZE_OF_LURELATION_NODE_DESC(ChildLurnCnt);
			LurDesc->Length += length;
			LurDesc->LurnDescCount ++;
			UnitBlocks = 0;

			UnitBlocks = AddTargetData->UnitDiskList[0].ulUnitBlocks; // RAID1 property

			// every unit disk should have same user space
			for(idx_childlurn = 0; idx_childlurn < ChildLurnCnt; idx_childlurn++ ) {
				if(AddTargetData->UnitDiskList[idx_childlurn].ulUnitBlocks != AddTargetData->UnitDiskList[0].ulUnitBlocks)
					ASSERT(FALSE);
			}

			// root
			TRANSLATE_UNITDISK_TO_LURN(	cur_lurn,
				cur_unitdisk,
				(cur_location - (PBYTE)LurDesc) + length,
				0,
				LURN_RAID1R,
				0,
				UnitBlocks-1,
				UnitBlocks,
				LurMaxRequestBlocks,
				BLOCK_SIZE,
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

				TRANSLATE_UNITDISK_TO_LURN(	cur_lurn,
					cur_unitdisk, // use leaf 0
					(cur_location - (PBYTE)LurDesc) + length,
					RootNode->LurnChildren[idx_childlurn],
					LURN_IDE_DISK,
					0,
					UnitBlocks - 1,
					UnitBlocks,
					cur_unitdisk->UnitMaxRequestBlocks,
					BLOCK_SIZE,
					0,
					0
					);
			}

			//
			//	set 0 offset to the last one.
			//
			cur_lurn->NextOffset = 0;
		}
		break;

	case NDASSCSI_TYPE_DISK_RAID4R:
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

		// root
		TRANSLATE_UNITDISK_TO_LURN(	cur_lurn,
									cur_unitdisk,
									(cur_location - (PBYTE)LurDesc) + length,
									0,
									LURN_RAID4R,
									0,
									UnitBlocks-1,
									UnitBlocks,
									LurMaxRequestBlocks,
									BLOCK_SIZE,
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

			TRANSLATE_UNITDISK_TO_LURN(	cur_lurn,
										cur_unitdisk, // use leaf 0
										(cur_location - (PBYTE)LurDesc) + length,
										RootNode->LurnChildren[idx_childlurn],
										LURN_IDE_DISK,
										0,
										UnitBlocks - 1,
										UnitBlocks,
										cur_unitdisk->UnitMaxRequestBlocks,
										BLOCK_SIZE,
										0,
										0
										);
		}

		//
		//	set 0 offset to the last one.
		//
		cur_lurn->NextOffset = 0;
	}
	break;
	default:
		return STATUS_INVALID_PARAMETER;
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

		if(LurnDesc->LurnType < 0 || LurnDesc->LurnType >= LurnInterfaceCnt ) {
			KDPrintM(DBG_LURN_ERROR, ("Error in Lurn Type %x\n", LurnDesc->LurnType));
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
		KDPrintM(DBG_LURN_ERROR, ("lurntype %x interface not implements LurnInitialize furnction\n", LurnDesc->LurnType ));
		return STATUS_NOT_IMPLEMENTED;
	}
	status = Lurn->LurnInterface->LurnFunc.LurnInitialize(
					Lurn,
					LurnDesc
				);
	if(NT_SUCCESS(status)) {
		Lurn->LurnStatus = LURN_STATUS_RUNNING;
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

//	ASSERT(status == STATUS_PENDING && !LSCcbIsStatusFlagOn(CCBSTATUS_FLAG_COMPLETED) && LSCcbIsStatusFlagOn(CCBSTATUS_FLAG_PENDING));
//	ASSERT(status == STATUS_SUCCESS && LSCcbIsStatusFlagOn(CCBSTATUS_FLAG_COMPLETED));
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

UINT32
LurnGetMaxRequestLength(
	 PLURELATION_NODE Lurn
)
{
	return Lurn->Lur->MaxBlocksPerRequest;
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
	if(Lurn->LurnDesc) {
		ExFreePoolWithTag(Lurn->LurnDesc, LURNEXT_POOL_TAG);
		Lurn->LurnDesc = NULL;
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
	// Recognize some indirect error patterns.
	//


	//
	// Check accesing same range failed.
	//
	if ((Type == LURN_ERR_READ || Type == LURN_ERR_WRITE) && pLfi &&
		(ErrorCode == LURN_FAULT_COMMUNICATION))	{
		if (fInfo->LastIoOperation) {
			//
			// Check accesing same range failed.
			//
			if (fInfo->LastIoAddr + fInfo->LastIoLength > pLfi->Address && pLfi->Address+pLfi->Length > fInfo->LastIoAddr) {
				KDPrintM(DBG_LURN_ERROR, ("IO failed on same location. Marking error\n"));
				ErrorCode = LURN_FAULT_BAD_SECTOR;
			}
		} else {
			// no previous IO error recorded.
		}
	}

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
			KDPrintM(DBG_LURN_INFO, ("Resetting error count\n"));

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

	if ((Type == LURN_ERR_READ || Type == LURN_ERR_WRITE) &&
		ErrorCode == LURN_FAULT_BAD_SECTOR) {
		fInfo->FaultCause |=  LURN_FCAUSE_BAD_SECTOR;
	}

	if (Type == LURN_ERR_DISK_OP) {
		if (ErrorCode == LURN_FAULT_IDENTIFY) {
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

