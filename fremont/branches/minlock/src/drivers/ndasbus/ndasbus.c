#include "ndasbusproc.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "NdasBus"

//
//	copy from W2K ntifs.h for INFORMATION_PDOEVENT
//
NTKERNELAPI
NTSTATUS
ObOpenObjectByPointer(
    IN PVOID Object,
    IN ULONG HandleAttributes,
    IN PACCESS_STATE PassedAccessState OPTIONAL,
    IN ACCESS_MASK DesiredAccess OPTIONAL,
    IN POBJECT_TYPE ObjectType OPTIONAL,
    IN KPROCESSOR_MODE AccessMode,
    OUT PHANDLE Handle
    );

NTKERNELAPI
NTSTATUS
ZwCreateEvent(
    OUT PHANDLE  EventHandle,
    IN ACCESS_MASK  DesiredAccess,
    IN POBJECT_ATTRIBUTES  ObjectAttributes OPTIONAL,
    IN EVENT_TYPE  EventType,
    IN BOOLEAN  InitialState
    );

//////////////////////////////////////////////////////////////////////////
//
//	Translate AddTargetData to LUR descriptor.
//
#include "lstransport.h"
#include "lsproto.h"

LONG
GetLurDescLenFromAddTargetData(
	IN PNDASBUS_ADD_TARGET_DATA	AddTargetData,
	IN ULONG	AddTargetDataLength
){
	LONG						lurDescLength;
	LONG						childLurnCnt;
	PNDAS_BLOCK_ACL				ndasBlockAcl;

	if(AddTargetDataLength < sizeof(NDASBUS_ADD_TARGET_DATA)) {
		return -1;
	}

	childLurnCnt	= AddTargetData->ulNumberOfUnitDiskList;

	// BACL
	if(AddTargetData->BACLOffset)
		ndasBlockAcl = (PNDAS_BLOCK_ACL)((PUCHAR)AddTargetData + AddTargetData->BACLOffset);
	else
		ndasBlockAcl = 0;

	Bus_KdPrint_Def(BUS_DBG_SS_ERROR,("SIZE_OF_LURELATION_DESC() = %d\n", SIZE_OF_LURELATION_DESC()));
	Bus_KdPrint_Def(BUS_DBG_SS_ERROR,("SIZE_OF_LURELATION_NODE_DESC(0) = %d\n", SIZE_OF_LURELATION_NODE_DESC(0)));

	switch(AddTargetData->ucTargetType)
	{
	case NDASSCSI_TYPE_DISK_MIRROR:
	case NDASSCSI_TYPE_DISK_AGGREGATION:
	case NDASSCSI_TYPE_DISK_RAID0:
		//	Associate type
		lurDescLength	=	SIZE_OF_LURELATION_DESC() + // relation descriptor
							SIZE_OF_LURELATION_NODE_DESC(childLurnCnt) + // root
							SIZE_OF_LURELATION_NODE_DESC(0) * (childLurnCnt);	// leaves
		break;
	case NDASSCSI_TYPE_DISK_RAID1R3:
		lurDescLength	=	SIZE_OF_LURELATION_DESC() + // relation descriptor
							SIZE_OF_LURELATION_NODE_DESC(childLurnCnt /2) + // root
							SIZE_OF_LURELATION_NODE_DESC(2) * (childLurnCnt /2) +// mirrored disks
							SIZE_OF_LURELATION_NODE_DESC(0) * (childLurnCnt);	// leaves
		break;
	case NDASSCSI_TYPE_DISK_RAID4R3:
	case NDASSCSI_TYPE_DISK_RAID5:        
		//	Associate type
		lurDescLength	=	SIZE_OF_LURELATION_DESC() + // relation descriptor
							SIZE_OF_LURELATION_NODE_DESC(childLurnCnt) + // root
							SIZE_OF_LURELATION_NODE_DESC(0) * (childLurnCnt);	// leaves
		break;
	case NDASSCSI_TYPE_DISK_NORMAL:
	case NDASSCSI_TYPE_VDVD:
	case NDASSCSI_TYPE_ATAPI_CDROM:
	case NDASSCSI_TYPE_ATAPI_OPTMEM:
	case NDASSCSI_TYPE_ATAPI_DIRECTACC:
	case NDASSCSI_TYPE_ATAPI_SEQUANACC:
	case NDASSCSI_TYPE_ATAPI_PRINTER:
	case NDASSCSI_TYPE_ATAPI_PROCESSOR:
	case NDASSCSI_TYPE_ATAPI_WRITEONCE:
	case NDASSCSI_TYPE_ATAPI_SCANNER:
	case NDASSCSI_TYPE_ATAPI_MEDCHANGER:
	case NDASSCSI_TYPE_ATAPI_COMM:
	case NDASSCSI_TYPE_ATAPI_ARRAYCONT:
	case NDASSCSI_TYPE_ATAPI_ENCLOSURE:
	case NDASSCSI_TYPE_ATAPI_RBC:
	case NDASSCSI_TYPE_ATAPI_OPTCARD:
		//	Single type
		lurDescLength	=	SIZE_OF_LURELATION_DESC() +
							SIZE_OF_LURELATION_NODE_DESC(0);
		break;
	default:
		lurDescLength = -1;
	}

	//
	//	If NDAS block ACL exists, add to the LUR length.
	//
	if(lurDescLength >= 0) {
		if(ndasBlockAcl) {
			lurDescLength += ndasBlockAcl->Length;
		}
	}

	return lurDescLength;
}

#define __MAX_DATA_ADJUST__	0

VOID
LurTranslateUnitdiskToLurnNodeDesc(
	OUT PLURELATION_NODE_DESC		NodeDesc,
	IN PNDASBUS_UNITDISK			UnitDiskPointer,
	IN UINT16						NextOffset,
	IN UINT32						LurnId,
	IN LURN_TYPE					LurnType,
	IN UCHAR						LurnDeviceInterface,
	IN UINT64						StartAddr,
	IN UINT64						EndAddr,
	IN UINT64						UnitBlocks,
	IN UINT32						MaxDataSend,
	IN UINT32						MaxDataRecv,
	IN UINT32						ChildCnt,
	IN UINT32						Parent
) {

#if __MAX_DATA_ADJUST__

	if (LurnType == LURN_DIRECT_ACCESS) {
		
		if (MaxDataRecv < 64*1024) {

			MaxDataRecv = 64 * 1024;
		}

		if (MaxDataSend < 64*1024) {

			MaxDataSend = 64 * 1024;
		}
	}

#endif

	NodeDesc->NextOffset			= NextOffset;
	NodeDesc->LurnId				= LurnId;															
	NodeDesc->LurnType				= LurnType;
	NodeDesc->LurnDeviceInterface	= LurnDeviceInterface;
	NodeDesc->AccessRight			= 0;																	
	NodeDesc->StartBlockAddr		= StartAddr;
	NodeDesc->EndBlockAddr		= EndAddr;															
	NodeDesc->UnitBlocks			= UnitBlocks;															
	ASSERT(MaxDataSend && MaxDataRecv);
	NodeDesc->MaxDataSendLength	= MaxDataSend;
	NodeDesc->MaxDataRecvLength	= MaxDataRecv;
	switch(LurnType) {
	case LURN_DIRECT_ACCESS:
	case LURN_CDROM:
	case LURN_OPTICAL_MEMORY:
	case LURN_SEQUENTIAL_ACCESS:
	case LURN_PRINTER:
	case LURN_PROCCESSOR:
	case LURN_WRITE_ONCE:
	case LURN_SCANNER:
	case LURN_MEDIUM_CHANGER:
	case LURN_COMMUNICATIONS:
	case LURN_ARRAY_CONTROLLER:
	case LURN_ENCLOSURE_SERVICES:
	case LURN_REDUCED_BLOCK_COMMAND:
	case LURN_OPTIOCAL_CARD:

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


static
INLINE
VOID
ConvertNdasScsiType2LurnType(
	__in UINT32 NdasScsiType,
	__out PLURN_TYPE LurnType,
	__out PUCHAR LurnDeviceInterface
){
	LURN_TYPE	lurnType;
	UCHAR		lurnDeviceInterface;

	switch(NdasScsiType) {
		case NDASSCSI_TYPE_DISK_NORMAL:
			lurnType = LURN_DIRECT_ACCESS;
			lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATA;
		break;
		case NDASSCSI_TYPE_DISK_MIRROR:
			lurnType = LURN_MIRRORING;
			lurnDeviceInterface = LURN_DEVICE_INTERFACE_LURN;
		break;
		case NDASSCSI_TYPE_DISK_AGGREGATION:
			lurnType = LURN_NDAS_AGGREGATION;
			lurnDeviceInterface = LURN_DEVICE_INTERFACE_LURN;
		break;
		case NDASSCSI_TYPE_DISK_RAID0:
			lurnType = LURN_NDAS_RAID0;
			lurnDeviceInterface = LURN_DEVICE_INTERFACE_LURN;
		break;
		case NDASSCSI_TYPE_DISK_RAID1R3:
			lurnType = LURN_NDAS_RAID1;
			lurnDeviceInterface = LURN_DEVICE_INTERFACE_LURN;
		break;
		case NDASSCSI_TYPE_DISK_RAID4R3:
			lurnType = LURN_NDAS_RAID4;
			lurnDeviceInterface = LURN_DEVICE_INTERFACE_LURN;
		break;
		case NDASSCSI_TYPE_DISK_RAID5:
			lurnType = LURN_NDAS_RAID5;
			lurnDeviceInterface = LURN_DEVICE_INTERFACE_LURN;
		break;
		case NDASSCSI_TYPE_ATAPI_CDROM:
			lurnType = LURN_CDROM;
			lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
		case NDASSCSI_TYPE_ATAPI_OPTMEM:
			lurnType = LURN_OPTICAL_MEMORY;
			lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
		case NDASSCSI_TYPE_ATAPI_DIRECTACC:
			lurnType = LURN_DIRECT_ACCESS;
			lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
		case NDASSCSI_TYPE_ATAPI_SEQUANACC:
			lurnType = LURN_SEQUENTIAL_ACCESS;
			lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
		case NDASSCSI_TYPE_ATAPI_PRINTER:
			lurnType = LURN_PRINTER;
			lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
		case NDASSCSI_TYPE_ATAPI_PROCESSOR:
			lurnType = LURN_PROCCESSOR;
			lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
		case NDASSCSI_TYPE_ATAPI_WRITEONCE:
			lurnType = LURN_WRITE_ONCE;
			lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
		case NDASSCSI_TYPE_ATAPI_SCANNER:
			lurnType = LURN_SCANNER;
			lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
		case NDASSCSI_TYPE_ATAPI_MEDCHANGER:
			lurnType = LURN_MEDIUM_CHANGER;
			lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
		case NDASSCSI_TYPE_ATAPI_COMM:
			lurnType = LURN_COMMUNICATIONS;
			lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
		case NDASSCSI_TYPE_ATAPI_ARRAYCONT:
			lurnType = LURN_ARRAY_CONTROLLER;
			lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
		case NDASSCSI_TYPE_ATAPI_ENCLOSURE:
			lurnType = LURN_ENCLOSURE_SERVICES;
			lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
		case NDASSCSI_TYPE_ATAPI_RBC:
			lurnType = LURN_REDUCED_BLOCK_COMMAND;
			lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;
		case NDASSCSI_TYPE_ATAPI_OPTCARD:
			lurnType = LURN_OPTIOCAL_CARD;
			lurnDeviceInterface = LURN_DEVICE_INTERFACE_ATAPI;
		break;

		case NDASSCSI_TYPE_DISK_RAID1:
		case NDASSCSI_TYPE_DISK_RAID4:
		case NDASSCSI_TYPE_DISK_RAID1R2:
		case NDASSCSI_TYPE_DISK_RAID4R2:
		case NDASSCSI_TYPE_VDVD:
		case NDASSCSI_TYPE_AOD:
		default:
			ASSERT(FALSE);
			lurnType = LURN_NULL;
			lurnDeviceInterface = LURN_DEVICE_INTERFACE_LURN;
	}

	if(LurnType)
		*LurnType = lurnType;
	if(LurnDeviceInterface)
		*LurnDeviceInterface = lurnDeviceInterface;
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
	PNDASBUS_UNITDISK			cur_unitdisk;
	ULONG						maxSendRecvForRootNode;
	LURN_TYPE					lurnType;
	UCHAR						lurnDevInterface;


	UNREFERENCED_PARAMETER(LurDescLength);

	Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("In.\n"));

	ChildLurnCnt	= AddTargetData->ulNumberOfUnitDiskList;
	cur_location	= (PBYTE)LurDesc;
	if(ChildLurnCnt <= 0) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ERROR : AddTargetData->ulNumberOfUnitDiskList = %d\n", AddTargetData->ulNumberOfUnitDiskList));
		return STATUS_INVALID_PARAMETER;
	}

	//
	//	Initialize LUR descriptor
	//
	LurDesc->Type = LUR_DESC_STRUCT_TYPE;
	LurDesc->Length = SIZE_OF_LURELATION_DESC();
	LurDesc->LurOptions = AddTargetData->LurOptions;
	LurDesc->LurId[0] = 0;
	LurDesc->LurId[1] = 0;
	LurDesc->LurId[2] = 0;
	LurDesc->MaxOsRequestLength = LurMaxOsRequestLength;
	LurDesc->LurnDescCount = 0;
	LurDesc->DeviceMode = AddTargetData->DeviceMode;
	LurDesc->LurNameLength = 0; // No use in NdasBus.
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
			Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("Encryption key method:%02x, key length:%d, ",
				LurDesc->CntEcrMethod,
				(int)LurDesc->CntEcrKeyLength)
				);
			for(keyIdx = 0; keyIdx<LurDesc->CntEcrKeyLength; keyIdx++) {
				Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("%02x ", (int)LurDesc->CntEcrKey[keyIdx]));
				if((keyIdx%4) == 0)
					Bus_KdPrint_Def(BUS_DBG_SS_INFO, (" "));
			}
			Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("\n"));
		}
#endif

	}

	//
	// Determine max transfer bytes for associated disks' root.
	//

	if(LurMaxOsRequestLength) {
		maxSendRecvForRootNode = LurMaxOsRequestLength;
	} else {
		maxSendRecvForRootNode = NDAS_MAX_TRANSFER_LENGTH;
	}

	//
	//	Initialize LURN descriptors
	//
	switch(AddTargetData->ucTargetType) {

	case NDASSCSI_TYPE_DISK_NORMAL:
	case NDASSCSI_TYPE_ATAPI_DIRECTACC:
	case NDASSCSI_TYPE_ATAPI_CDROM:
	case NDASSCSI_TYPE_ATAPI_OPTMEM:
	case NDASSCSI_TYPE_ATAPI_SEQUANACC:
	case NDASSCSI_TYPE_ATAPI_PRINTER:
	case NDASSCSI_TYPE_ATAPI_PROCESSOR:
	case NDASSCSI_TYPE_ATAPI_WRITEONCE:
	case NDASSCSI_TYPE_ATAPI_SCANNER:
	case NDASSCSI_TYPE_ATAPI_MEDCHANGER:
	case NDASSCSI_TYPE_ATAPI_COMM:
	case NDASSCSI_TYPE_ATAPI_ARRAYCONT:
	case NDASSCSI_TYPE_ATAPI_ENCLOSURE:
	case NDASSCSI_TYPE_ATAPI_RBC:
	case NDASSCSI_TYPE_ATAPI_OPTCARD:

		//
		// Single type devices
		//

		ConvertNdasScsiType2LurnType(
			AddTargetData->ucTargetType,
			&lurnType,
			&lurnDevInterface);


		LurDesc->Length += SIZE_OF_LURELATION_NODE_DESC(0);
		LurDesc->EndingBlockAddr = cur_unitdisk->ulUnitBlocks-1;
		LurDesc->LurnDescCount ++;
		LurTranslateUnitdiskToLurnNodeDesc(	cur_lurn,
									cur_unitdisk,
									(UINT16)0,
                                    0,
									lurnType,
									lurnDevInterface,
									0,
									cur_unitdisk->ulUnitBlocks-1,
									cur_unitdisk->ulUnitBlocks,
									cur_unitdisk->UnitMaxDataSendLength,
									cur_unitdisk->UnitMaxDataRecvLength,
									0,
									LURN_ROOT_INDEX
									);
		Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("TYPE : DVD UserID:%08lx Password:%I64x\n",
										cur_lurn->LurnIde.UserID,
										cur_lurn->LurnIde.Password
									));

		//
		//	Set the next available memory location.
		//

		cur_location += SIZE_OF_LURELATION_NODE_DESC(0);

		break;


	case NDASSCSI_TYPE_DISK_MIRROR: {
		UINT16					length;
		PLURELATION_NODE_DESC	RootNode;


		//
		//	Build a LurnDesc for mirroring
		//
		RootNode = cur_lurn;
		length = SIZE_OF_LURELATION_NODE_DESC(2);
		LurDesc->Length += length;
		LurDesc->EndingBlockAddr = cur_unitdisk->ulUnitBlocks-1;
		LurDesc->LurnDescCount ++;

		// Root node
		LurTranslateUnitdiskToLurnNodeDesc(	cur_lurn,
									(PNDASBUS_UNITDISK)NULL, //cur_unitdisk,
									(UINT16)((cur_location - (PBYTE)LurDesc) + length),
									0,
									LURN_MIRRORING,
									LURN_DEVICE_INTERFACE_LURN,
									0,
									cur_unitdisk->ulUnitBlocks-1,
									cur_unitdisk->ulUnitBlocks,
									maxSendRecvForRootNode,
									maxSendRecvForRootNode,
									2,
									LURN_ROOT_INDEX
									);

		//
		//	Build two children
		//
		for(idx_childlurn = 0; idx_childlurn < ChildLurnCnt; idx_childlurn++ ) {

			cur_location += length;
			cur_lurn = (PLURELATION_NODE_DESC)(cur_location);
			cur_unitdisk = AddTargetData->UnitDiskList + idx_childlurn;
			RootNode->LurnChildren[idx_childlurn] = idx_childlurn + 1;
			length = SIZE_OF_LURELATION_NODE_DESC(0);
			LurDesc->Length	+= length;
			LurDesc->LurnDescCount ++;
			LurTranslateUnitdiskToLurnNodeDesc(	cur_lurn,
									cur_unitdisk,
									(UINT16)((cur_location - (PBYTE)LurDesc) + length),
									idx_childlurn + 1,
									LURN_DIRECT_ACCESS,
									LURN_DEVICE_INTERFACE_ATA,
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
		UINT64					UnitBlocks;

		//
		//	Build a LurnDesc for aggregation
		//

		RootNode = cur_lurn;
		length = (UINT16)SIZE_OF_LURELATION_NODE_DESC(ChildLurnCnt);
		LurDesc->Length += length;
		LurDesc->LurnDescCount ++;
		UnitBlocks = 0;
		for(idx_childlurn = 0; idx_childlurn < ChildLurnCnt; idx_childlurn++ ) {
				UnitBlocks += AddTargetData->UnitDiskList[idx_childlurn].ulUnitBlocks;
		}
		LurDesc->EndingBlockAddr = UnitBlocks - 1;

		// Root node
		LurTranslateUnitdiskToLurnNodeDesc(	cur_lurn,
									(PNDASBUS_UNITDISK)NULL, // cur_unitdisk,
									(UINT16)((cur_location - (PBYTE)LurDesc) + length),
									0,
									LURN_AGGREGATION,
									LURN_DEVICE_INTERFACE_LURN,
									0,
									UnitBlocks-1,
									UnitBlocks,
									maxSendRecvForRootNode,
									maxSendRecvForRootNode,
									ChildLurnCnt,
									LURN_ROOT_INDEX
									);

		//
		//	Build children
		//
		for(idx_childlurn = 0; idx_childlurn < ChildLurnCnt; idx_childlurn++ ) {

			cur_location += length;
			cur_lurn = (PLURELATION_NODE_DESC)(cur_location);
			cur_unitdisk = AddTargetData->UnitDiskList + idx_childlurn;
			LurDesc->LurnDescCount ++;

			//	set a child index to the root.
			RootNode->LurnChildren[idx_childlurn] = idx_childlurn + 1;

			// initialize the child.
			length = SIZE_OF_LURELATION_NODE_DESC(0);
			LurDesc->Length += length;
			LurTranslateUnitdiskToLurnNodeDesc(	cur_lurn,
									cur_unitdisk,
									(UINT16)((cur_location - (PBYTE)LurDesc) + length),
									idx_childlurn + 1,
									LURN_DIRECT_ACCESS,
									LURN_DEVICE_INTERFACE_ATA,
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
	case NDASSCSI_TYPE_DISK_RAID0:
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

		UnitBlocks = AddTargetData->UnitDiskList[0].ulUnitBlocks;

		for(idx_childlurn = 0; idx_childlurn < ChildLurnCnt; idx_childlurn++ ) {
			if(AddTargetData->UnitDiskList[idx_childlurn].ulUnitBlocks != AddTargetData->UnitDiskList[0].ulUnitBlocks) {
				ASSERT(FALSE);
			}
		}
		LurDesc->EndingBlockAddr = UnitBlocks - 1;

		// Root node
		LurTranslateUnitdiskToLurnNodeDesc(	cur_lurn,
									(PNDASBUS_UNITDISK)NULL, //cur_unitdisk,
									(UINT16)((cur_location - (PBYTE)LurDesc) + length),
									0,
									LURN_RAID0,
									LURN_DEVICE_INTERFACE_LURN,
									0,
									UnitBlocks-1,
									UnitBlocks,
									maxSendRecvForRootNode,
									maxSendRecvForRootNode,
									ChildLurnCnt,
									LURN_ROOT_INDEX
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
										LURN_DIRECT_ACCESS,
										LURN_DEVICE_INTERFACE_ATA,
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

			// Root node
			LurTranslateUnitdiskToLurnNodeDesc(	cur_lurn,
				(PNDASBUS_UNITDISK)NULL, //cur_unitdisk,
				(UINT16)((cur_location - (PBYTE)LurDesc) + length),
				0,
				LURN_RAID1R,
				LURN_DEVICE_INTERFACE_LURN,
				0,
				UnitBlocks-1,
				UnitBlocks,
				maxSendRecvForRootNode,
				maxSendRecvForRootNode,
				ChildLurnCnt,
				LURN_ROOT_INDEX
				);

			// copy RAID information
			RtlCopyMemory(&RootNode->LurnInfoRAID, &AddTargetData->RAID_Info, sizeof(NDAS_RAID_INFO));

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
					LURN_DIRECT_ACCESS,
					LURN_DEVICE_INTERFACE_ATA,
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

		// Root node
		LurTranslateUnitdiskToLurnNodeDesc(	cur_lurn,
									(PNDASBUS_UNITDISK)NULL, //cur_unitdisk,
									(UINT16)((cur_location - (PBYTE)LurDesc) + length),
									0,
									LURN_RAID4R,
									LURN_DEVICE_INTERFACE_LURN,
									0,
									UnitBlocks-1,
									UnitBlocks,
									maxSendRecvForRootNode,
									maxSendRecvForRootNode,
									ChildLurnCnt,
									LURN_ROOT_INDEX
									);

		// copy RAID information
		RtlCopyMemory(&RootNode->LurnInfoRAID, &AddTargetData->RAID_Info, sizeof(NDAS_RAID_INFO));

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
										LURN_DIRECT_ACCESS,
										LURN_DEVICE_INTERFACE_ATA,
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
	case NDASSCSI_TYPE_DISK_RAID5:
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

		UnitBlocks = AddTargetData->UnitDiskList[0].ulUnitBlocks; // RAID4 property

		// every unit disk should have same user space
		for(idx_childlurn = 0; idx_childlurn < ChildLurnCnt; idx_childlurn++ ) {
			if(AddTargetData->UnitDiskList[idx_childlurn].ulUnitBlocks != AddTargetData->UnitDiskList[0].ulUnitBlocks)
				ASSERT(FALSE);
		}
		LurDesc->EndingBlockAddr = UnitBlocks - 1;

		// Root node
		LurTranslateUnitdiskToLurnNodeDesc(	cur_lurn,
									(PNDASBUS_UNITDISK)NULL, //cur_unitdisk,
									(UINT16)((cur_location - (PBYTE)LurDesc) + length),
									0,
									LURN_RAID5,
									LURN_DEVICE_INTERFACE_LURN,
									0,
									UnitBlocks-1,
									UnitBlocks,
									maxSendRecvForRootNode,
									maxSendRecvForRootNode,
									ChildLurnCnt,
									LURN_ROOT_INDEX
									);

		// copy RAID information
		RtlCopyMemory(&RootNode->LurnInfoRAID, &AddTargetData->RAID_Info, sizeof(NDAS_RAID_INFO));

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
										LURN_DIRECT_ACCESS,
										LURN_DEVICE_INTERFACE_ATA,
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


//////////////////////////////////////////////////////////////////////////
//
//	I/O Control to the NDASSCSI.
//	Buffers must be allocated from NonPagedPool
//
//	NOTE:	Do not use  LANSCSIMINIPORT_IOCTL_QUERYINFO.
//			It uses separate input/output buffer.
//			It will be obsolete.
//
NTSTATUS
LSBus_IoctlToNdasScsiDevice(
		PPDO_DEVICE_DATA	PdoData,
		ULONG				IoctlCode,
		PVOID				InputBuffer,
		LONG				InputBufferLength,
		PVOID				OutputBuffer,
		LONG				OutputBufferLength
	) {

	PDEVICE_OBJECT		AttachedDevice;
    PIRP				irp;
    KEVENT				event;
	PSRB_IO_CONTROL		psrbIoctl;
	LONG				srbIoctlLength;
	PVOID				srbIoctlBuffer;
	LONG				srbIoctlBufferLength;
    NTSTATUS			status;
    PIO_STACK_LOCATION	irpStack;
    SCSI_REQUEST_BLOCK	srb;
    LARGE_INTEGER		startingOffset;
    IO_STATUS_BLOCK		ioStatusBlock;

	AttachedDevice = NULL;
	psrbIoctl	= NULL;
	irp = NULL;

	//
	//	get a ScsiPort device or attached one.
	//
	AttachedDevice = IoGetAttachedDeviceReference(PdoData->Self);

	if(AttachedDevice == NULL) {
		Bus_KdPrint_Def( BUS_DBG_SS_ERROR, ("STATUS_INVALID_DEVICE\n"));
		return STATUS_NO_SUCH_DEVICE;
	}

	//
	//	build an SRB for the miniport
	//
	srbIoctlBufferLength = (InputBufferLength>OutputBufferLength)?InputBufferLength:OutputBufferLength;
	srbIoctlLength = sizeof(SRB_IO_CONTROL) +  srbIoctlBufferLength;

	psrbIoctl = (PSRB_IO_CONTROL)ExAllocatePoolWithTag(NonPagedPool , srbIoctlLength, BUSENUM_POOL_TAG);
	if(psrbIoctl == NULL) {
		Bus_KdPrint_Def( BUS_DBG_SS_ERROR, ("STATUS_INSUFFICIENT_RESOURCES\n"));
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto cleanup;
	}

	RtlZeroMemory(psrbIoctl, srbIoctlLength);
	psrbIoctl->HeaderLength = sizeof(SRB_IO_CONTROL);
	RtlCopyMemory(psrbIoctl->Signature, NDASSCSI_IOCTL_SIGNATURE, 8);
	psrbIoctl->Timeout = 60 * 60;
	psrbIoctl->ControlCode = IoctlCode;
	psrbIoctl->Length = srbIoctlBufferLength;

	srbIoctlBuffer = (PUCHAR)psrbIoctl + sizeof(SRB_IO_CONTROL);
	RtlCopyMemory(srbIoctlBuffer, InputBuffer, InputBufferLength);

    //
    // Initialize the notification event.
    //

    KeInitializeEvent(&event,
                        NotificationEvent,
                        FALSE);
	startingOffset.QuadPart = 1;

    //
    // Build IRP for this request.
    // Note we do this synchronously for two reasons.  If it was done
    // asynchonously then the completion code would have to make a special
    // check to deallocate the buffer.  Second if a completion routine were
    // used then an additional IRP stack location would be needed.
    //

    irp = IoBuildSynchronousFsdRequest(
                IRP_MJ_SCSI,
                AttachedDevice,
                psrbIoctl,
                srbIoctlLength,
                &startingOffset,
                &event,
                &ioStatusBlock);

    irpStack = IoGetNextIrpStackLocation(irp);

    if (irp == NULL) {
        Bus_KdPrint_Def( BUS_DBG_SS_ERROR, ("STATUS_INSUFFICIENT_RESOURCES\n"));

		status = STATUS_INSUFFICIENT_RESOURCES;
		goto cleanup;
    }

    //
    // Set major and minor codes.
    //

    irpStack->MajorFunction = IRP_MJ_SCSI;
    irpStack->MinorFunction = 1;

    //
    // Fill in SRB fields.
    //

    irpStack->Parameters.Others.Argument1 = &srb;

    //
    // Zero out the srb.
    //

    RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));

    srb.PathId = 0;
    srb.TargetId = 0;
    srb.Lun = 0;

    srb.Function = SRB_FUNCTION_IO_CONTROL;
    srb.Length = sizeof(SCSI_REQUEST_BLOCK);

    srb.SrbFlags = /*SRB_FLAGS_DATA_IN |*/ SRB_FLAGS_NO_QUEUE_FREEZE /*| SRB_FLAGS_BYPASS_FROZEN_QUEUE */;

    srb.OriginalRequest = irp;

    //
    // Set timeout to requested value.
    //

    srb.TimeOutValue = psrbIoctl->Timeout;

    //
    // Set the data buffer.
    //

    srb.DataBuffer = psrbIoctl;
    srb.DataTransferLength = srbIoctlLength;

    //
    // Flush the data buffer for output. This will insure that the data is
    // written back to memory.  Since the data-in flag is the the port driver
    // will flush the data again for input which will ensure the data is not
    // in the cache.
    //
/*
    KeFlushIoBuffers(irp->MdlAddress, FALSE, TRUE);
*/
    status = IoCallDriver( AttachedDevice, irp );

    //
    // Wait for request to complete.
    //
    if (status == STATUS_PENDING) {

        (VOID)KeWaitForSingleObject( 
									&event,
                                     Executive,
                                     KernelMode,
                                     FALSE,
                                     (PLARGE_INTEGER)NULL 
									 );

        status = ioStatusBlock.Status;
    }

	//
	//	get the result
	//
//	if(status == STATUS_SUCCESS) {
		if(OutputBuffer && OutputBufferLength)
			RtlCopyMemory(OutputBuffer, srbIoctlBuffer, OutputBufferLength);
			Bus_KdPrint_Def( BUS_DBG_SS_NOISE, ("%d succeeded!\n", IoctlCode));
//	}
	if(psrbIoctl->ControlCode == STATUS_BUFFER_TOO_SMALL) {
		status = STATUS_BUFFER_TOO_SMALL;
	}

cleanup:
	if(psrbIoctl)
		ExFreePool(psrbIoctl);
	if(AttachedDevice)
		ObDereferenceObject(AttachedDevice);

    return status;
}


NTSTATUS
LSBus_OpenLanscsiAdapter(
	IN	PPDO_LANSCSI_DEVICE_DATA	LanscsiAdapter,
	IN	ULONG				MaxRequestLength,
	IN	PKEVENT				DisconEventToService,
	IN	PKEVENT				AlarmEventToService
){

	Bus_KdPrint_Def(BUS_DBG_SS_TRACE, ("Entered.\n"));

    ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL);

	RtlZeroMemory(
		LanscsiAdapter,
		sizeof(PDO_LANSCSI_DEVICE_DATA)
		);

	KeInitializeSpinLock(&LanscsiAdapter->LSDevDataSpinLock);
	LanscsiAdapter->MaxRequestLength = MaxRequestLength;
	LanscsiAdapter->DisconEventToService = DisconEventToService;
	LanscsiAdapter->AlarmEventToService = AlarmEventToService;
	InitializeListHead(&LanscsiAdapter->NdasPdoEventQueue);

	//
	//	initialize private fields.
	//
	KeInitializeEvent(
			&LanscsiAdapter->AddTargetEvent,
			NotificationEvent,
			FALSE
    );

	return STATUS_SUCCESS;
}

NTSTATUS
LSBus_WaitUntilNdasScsiStop(
		IN	PPDO_LANSCSI_DEVICE_DATA	LanscsiAdapter
	) {
	LARGE_INTEGER	Interval;
	LONG			WaitCnt;
	NTSTATUS		ntStatus;
	KIRQL			oldIrql;

    ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL);
	Bus_KdPrint_Def(BUS_DBG_SS_TRACE, ("Entered.\n"));

	//
	//	Wait for NDASSCSI stop.
	//
	ntStatus = STATUS_SUCCESS;
	Interval.QuadPart = - NANO100_PER_SEC / 2;	// 0.5 seconds.
	WaitCnt = 0;
	while(1) {

		KeAcquireSpinLock(&LanscsiAdapter->LSDevDataSpinLock, &oldIrql);
		if(ADAPTERINFO_ISSTATUS(LanscsiAdapter->LastAdapterStatus, NDASSCSI_ADAPTER_STATUS_STOPPED)) {
			KeReleaseSpinLock(&LanscsiAdapter->LSDevDataSpinLock, oldIrql);
			break;
		}
		KeReleaseSpinLock(&LanscsiAdapter->LSDevDataSpinLock, oldIrql);

		Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("Wait for NDASSCSI!!\n"));
		WaitCnt ++;
		KeDelayExecutionThread(KernelMode, TRUE,&Interval);
		if(WaitCnt >= NDASBUS_NDASSCSI_STOP_TIMEOUT) {
			Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("TimeOut!!!\n"));
			ntStatus = STATUS_TIMEOUT;
			break;
		}
	}

	return ntStatus;
}



NTSTATUS
LSBus_CloseLanscsiAdapter(
					IN	PPDO_LANSCSI_DEVICE_DATA	LanscsiAdapter
					)
{
	
    ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL);
	Bus_KdPrint_Def(BUS_DBG_SS_TRACE, ("Entered.\n"));

	//
	// Clean up the PDO status queue
	//

	NdasBusCleanupPdoStatusQueue(LanscsiAdapter);

	//
	//	Dereference objects.
	//
	ObDereferenceObject(LanscsiAdapter->DisconEventToService);

	if(LanscsiAdapter->AlarmEventToService)
		ObDereferenceObject(LanscsiAdapter->AlarmEventToService);

	//
	//	Free allocated memory.
	//
	if(LanscsiAdapter->AddDevInfo)
		ExFreePool(LanscsiAdapter->AddDevInfo);

	return STATUS_SUCCESS;
}


//
//	Get physical device's name by querying device properties.
//	Caller must free DeviceName.
//

NTSTATUS
GetPhyDeviceName(
	PDEVICE_OBJECT	PhysicalDeviceObject,
	PWCHAR			*DeviceName,
	PUSHORT			DeviceNameLen

){
	PWCHAR              deviceName = NULL;
	ULONG               nameLength;
	NTSTATUS			status;


	status = IoGetDeviceProperty (	PhysicalDeviceObject,
									DevicePropertyPhysicalDeviceObjectName,
									0,
									NULL,
									&nameLength);

	if (status != STATUS_BUFFER_TOO_SMALL)
	{
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR,("IoGetDeviceProperty failed (0x%x)\n", status));
		goto Error;
	}

	deviceName = ExAllocatePoolWithTag (NonPagedPool, nameLength, BUSENUM_POOL_TAG);

	if (NULL == deviceName) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR,
			("no memory to alloc for deviceName(0x%x)\n", nameLength));
		status =  STATUS_INSUFFICIENT_RESOURCES;
		goto Error;
	}

	status = IoGetDeviceProperty (	PhysicalDeviceObject,
									DevicePropertyPhysicalDeviceObjectName,
									nameLength,
									deviceName,
									&nameLength);

	if (!NT_SUCCESS (status)) {

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("IoGetDeviceProperty(2) failed (0x%x)", status));
		goto Error;
	}

	Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("%x (%ws) \n",
					PhysicalDeviceObject,
					deviceName));

	//
	//	Set return values
	//
	*DeviceName = deviceName;
	*DeviceNameLen = (USHORT)nameLength;


Error:
	if(!NT_SUCCESS(status)) {
		if(deviceName)
			ExFreePool(deviceName);
	}

	return status;
}

//
//	Query information on LanscsiBus
//
NTSTATUS
LSBus_QueryInformation(
		PFDO_DEVICE_DATA				FdoData,
		BOOLEAN							Request32,
		PNDASBUS_QUERY_INFORMATION		Query,
		PNDASBUS_INFORMATION			Information,
		LONG							OutBufferLength,
		PLONG							OutBufferLenNeeded
	) {

	NTSTATUS			status;
	PLIST_ENTRY         entry;
	PPDO_DEVICE_DATA	PdoData;
	

	status = STATUS_SUCCESS;
	*OutBufferLenNeeded = OutBufferLength;
	PdoData = NULL;

	//
	//	Acquire the mutex to prevent PdoData ( Device Extension ) to disappear.
	//
    KeEnterCriticalRegion();
	ExAcquireFastMutex (&FdoData->Mutex);

	switch(Query->InfoClass) {
	case INFORMATION_NUMOFPDOS: {
		ULONG				NumOfPDOs;

		NumOfPDOs = 0;

		for (entry = FdoData->ListOfPDOs.Flink;
			entry != &FdoData->ListOfPDOs;
			entry = entry->Flink) {
			NumOfPDOs ++;
		}

		Information->NumberOfPDOs = NumOfPDOs;
		break;
	}
	case INFORMATION_PDO: {
		KIRQL	oldIrql;

		for (entry = FdoData->ListOfPDOs.Flink;
			entry != &FdoData->ListOfPDOs;
			entry = entry->Flink) {

				PdoData = CONTAINING_RECORD(entry, PDO_DEVICE_DATA, Link);
				if(Query->SlotNo == PdoData->SerialNo) {
					ObReferenceObject(PdoData->Self);
					break;
				}
				PdoData = NULL;

		}

		if(PdoData) {
			KeAcquireSpinLock(&PdoData->LanscsiAdapterPDO.LSDevDataSpinLock, &oldIrql);

			if((Query->Flags & NDASBUS_QUERYFLAG_EVENTQUEUE) == 0)  {
				//
				// Return legacy status value.
				//
				Bus_KdPrint_Def(BUS_DBG_SS_TRACE, ("LastAdapterStatus:PDO:%p Status:%08lx DevMod:%08lx SupFeat:%08lx EnFeat:%08lx\n",
												PdoData->Self,
												PdoData->LanscsiAdapterPDO.LastAdapterStatus,
												PdoData->LanscsiAdapterPDO.DeviceMode,
												PdoData->LanscsiAdapterPDO.SupportedFeatures,
												PdoData->LanscsiAdapterPDO.EnabledFeatures
										));
				Information->PdoInfo.AdapterStatus = PdoData->LanscsiAdapterPDO.LastAdapterStatus;
				Information->PdoInfo.DeviceMode = PdoData->LanscsiAdapterPDO.DeviceMode;
				Information->PdoInfo.SupportedFeatures = PdoData->LanscsiAdapterPDO.SupportedFeatures;
				Information->PdoInfo.EnabledFeatures = PdoData->LanscsiAdapterPDO.EnabledFeatures;
			} else {

				//
				// Return status from the event queue.
				//

				ULONG					adapterStatus;
				PNDASBUS_PDOEVENT_ENTRY ndasPdoStatus;

				ndasPdoStatus = NdasBusDequeuePdoStatusItem(&PdoData->LanscsiAdapterPDO);
				if(ndasPdoStatus) {
					BOOLEAN	empty;

					adapterStatus = ndasPdoStatus->AdapterStatus;
					empty = NdasBusIsEmpty(&PdoData->LanscsiAdapterPDO);
					if(!empty) {
						adapterStatus |= NDASSCSI_ADAPTER_STATUSFLAG_NEXT_EVENT_EXIST;
					}

					//
					// Free the entry
					//

					NdasBusDestroyPdoStatusItem(ndasPdoStatus);
				} else {
					KeReleaseSpinLock(&PdoData->LanscsiAdapterPDO.LSDevDataSpinLock, oldIrql);

					//
					// No more entry
					//

					Bus_KdPrint_Def(BUS_DBG_SS_NOISE, ("No more status item for SlotNo %d!\n", Query->SlotNo));
					status = STATUS_NO_MORE_ENTRIES;
					break;
				}

				Bus_KdPrint_Def(BUS_DBG_SS_TRACE, ("QueuedAdapterStatus:PDO:%p Status:%08lx DevMod:%08lx SupFeat:%08lx EnFeat:%08lx\n",
												PdoData->Self,
												adapterStatus,
												PdoData->LanscsiAdapterPDO.DeviceMode,
												PdoData->LanscsiAdapterPDO.SupportedFeatures,
												PdoData->LanscsiAdapterPDO.EnabledFeatures
										));
				Information->PdoInfo.AdapterStatus = adapterStatus;
				Information->PdoInfo.DeviceMode = PdoData->LanscsiAdapterPDO.DeviceMode;
				Information->PdoInfo.SupportedFeatures = PdoData->LanscsiAdapterPDO.SupportedFeatures;
				Information->PdoInfo.EnabledFeatures = PdoData->LanscsiAdapterPDO.EnabledFeatures;
			}

			KeReleaseSpinLock(&PdoData->LanscsiAdapterPDO.LSDevDataSpinLock, oldIrql);

			ObDereferenceObject(PdoData->Self);
		} else {
			Bus_KdPrint_Def(BUS_DBG_SS_NOISE, ("No PDO for SlotNo %d!\n", Query->SlotNo));
			status = STATUS_NO_SUCH_DEVICE;
		}
		break;
	}
	case INFORMATION_PDOENUM: {
		LARGE_INTEGER			TimeOut;
		ULONG					resultLength;
		DEVICE_INSTALL_STATE	deviceInstallState;
		LONG					lurDescLength;
		OUT PLURELATION_DESC	lurDesc;

		for (entry = FdoData->ListOfPDOs.Flink;
			entry != &FdoData->ListOfPDOs;
			entry = entry->Flink) {

				PdoData = CONTAINING_RECORD(entry, PDO_DEVICE_DATA, Link);
				if(Query->SlotNo == PdoData->SerialNo) {
					ObReferenceObject(PdoData->Self);
					break;
				}
				PdoData = NULL;

		}

		ExReleaseFastMutex (&FdoData->Mutex);
	    KeLeaveCriticalRegion();

		if(!PdoData) {
		    KeEnterCriticalRegion();
			ExAcquireFastMutex (&FdoData->Mutex);
			status = STATUS_NO_SUCH_DEVICE;
			break;
		}
		//
		//	Wait until NDAS service sends AddTargetData.
		//
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("waiting for AddTargetEvent.\n"));
		TimeOut.QuadPart = -10 * 1000 * 1000 * 120;			// 120 seconds
		status = KeWaitForSingleObject(
						&PdoData->LanscsiAdapterPDO.AddTargetEvent,
						Executive,
						KernelMode,
						FALSE,
						&TimeOut
					);
		if(status != STATUS_SUCCESS) {
		    KeEnterCriticalRegion();
			ExAcquireFastMutex (&FdoData->Mutex);

			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("failed to wait for AddTargetEvent.\n"));
			status = STATUS_NO_SUCH_DEVICE;
			ObDereferenceObject(PdoData->Self);
			break;
		}
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Completed to wait for AddTargetEvent.\n"));

		if(PdoData->LanscsiAdapterPDO.AddDevInfo == NULL) {
			KeEnterCriticalRegion();
			ExAcquireFastMutex (&FdoData->Mutex);

			status = STATUS_NO_SUCH_DEVICE;
			ObDereferenceObject(PdoData->Self);
			break;
		}

		//
		// Translate add target data to LUR descriptor 
		// if we have add target data format in hand.
		//
		if(!(PdoData->LanscsiAdapterPDO.Flags & LSDEVDATA_FLAG_LURDESC)) {
			lurDescLength = GetLurDescLenFromAddTargetData(
				(PNDASBUS_ADD_TARGET_DATA)PdoData->LanscsiAdapterPDO.AddDevInfo,
				PdoData->LanscsiAdapterPDO.AddDevInfoLength
				);
			if(lurDescLength < 0) {
				status = STATUS_BAD_DEVICE_TYPE;
			}
			// Translate later if the user buffer is big enough.
			lurDesc = NULL;
		} else {
			lurDesc = (PLURELATION_DESC)PdoData->LanscsiAdapterPDO.AddDevInfo;
			lurDescLength = PdoData->LanscsiAdapterPDO.AddDevInfoLength;
		}

		*OutBufferLenNeeded =	FIELD_OFFSET(NDASBUS_INFORMATION, PdoEnumInfo) +
			FIELD_OFFSET(NDASBUS_INFORMATION_PDOENUM, AddDevInfo) +
			lurDescLength;
		if(OutBufferLength < *OutBufferLenNeeded) {
			status = STATUS_BUFFER_TOO_SMALL;
		} else {
			if(!(PdoData->LanscsiAdapterPDO.Flags & LSDEVDATA_FLAG_LURDESC)) {
				lurDesc = ExAllocatePoolWithTag(NonPagedPool, lurDescLength, BUSENUM_LURDESC_TAG);
				if(lurDesc == NULL) {
					status = STATUS_INSUFFICIENT_RESOURCES;
					ObDereferenceObject(PdoData->Self);
					break;
				}

				status = LurTranslateAddTargetDataToLURDesc(
					(PNDASBUS_ADD_TARGET_DATA)PdoData->LanscsiAdapterPDO.AddDevInfo,
					PdoData->LanscsiAdapterPDO.MaxRequestLength,
					lurDescLength,
					lurDesc
					);
				if(!NT_SUCCESS(status)) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("LurTranslateAddTargetDataToLURDesc() failed. STATUS=%08lx\n", status));
					ExFreePoolWithTag(lurDesc, BUSENUM_LURDESC_TAG);
					ObDereferenceObject(PdoData->Self);
					break;
				}
			}

			RtlCopyMemory(
				Information->PdoEnumInfo.AddDevInfo,
				lurDesc,
				lurDescLength
				);
			Information->PdoEnumInfo.Flags = PDOENUM_FLAG_LURDESC;
			Information->PdoEnumInfo.MaxRequestLength = PdoData->LanscsiAdapterPDO.MaxRequestLength;


			//
			//	Check to see if this is the first enumeration.
			//

			status = DrGetDeviceProperty(
				PdoData->Self,
				DevicePropertyInstallState,
				sizeof(deviceInstallState),
				&deviceInstallState,
				&resultLength,
				(Globals.MajorVersion == 5) && (Globals.MinorVersion == 0)
				);
			if(NT_SUCCESS(status)) {
				if(deviceInstallState != InstallStateInstalled) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("First time installation. Do not enumerate a device.\n"));
					Information->PdoEnumInfo.Flags |= PDOENUM_FLAG_DRV_NOT_INSTALLED;
				}
			} else {

				//
				//	Even if DrGetDeviceProperty() fails,
				//	we can mount the device successfully.
				//

				status = STATUS_SUCCESS;
			}

			if(!(PdoData->LanscsiAdapterPDO.Flags & LSDEVDATA_FLAG_LURDESC)) {
				ExFreePoolWithTag(lurDesc, BUSENUM_LURDESC_TAG);
			}
		}

		KeEnterCriticalRegion();
		ExAcquireFastMutex (&FdoData->Mutex);

		ObDereferenceObject(PdoData->Self);
		break;
	}
	case INFORMATION_PDOEVENT: {

		for (entry = FdoData->ListOfPDOs.Flink;
			entry != &FdoData->ListOfPDOs;
			entry = entry->Flink) {

				PdoData = CONTAINING_RECORD(entry, PDO_DEVICE_DATA, Link);
				if(Query->SlotNo == PdoData->SerialNo) {
					ObReferenceObject(PdoData->Self);
					break;
				}
				PdoData = NULL;

		}

		if(PdoData == NULL) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Could not find the PDO:%u.\n", Query->SlotNo));
			status = STATUS_NO_SUCH_DEVICE;
			break;
		}

		if( Query->Flags & NDASBUS_QUERYFLAG_USERHANDLE) {

			if(Request32 == FALSE) {

				Information->PdoEvents.SlotNo = PdoData->SerialNo;
				Information->PdoEvents.Flags = NDASBUS_QUERYFLAG_USERHANDLE;

				//
				//	Get user-mode event handles.
				//
				status = ObOpenObjectByPointer(
						PdoData->LanscsiAdapterPDO.DisconEventToService,
						0,
						NULL,
						EVENT_ALL_ACCESS,
						*ExEventObjectType,
						UserMode,
						&Information->PdoEvents.DisconEvent
					);
				if(!NT_SUCCESS(status)) {
					ObDereferenceObject(PdoData->Self);
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Could not open Disconnect event object!!\n"));
					break;
				}
				status = ObOpenObjectByPointer(
						PdoData->LanscsiAdapterPDO.AlarmEventToService,
						0,
						NULL,
						EVENT_ALL_ACCESS,
						*ExEventObjectType,
						UserMode,
						&Information->PdoEvents.AlarmEvent
					);
				if(!NT_SUCCESS(status)) {
					ObDereferenceObject(PdoData->Self);
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Could not open Disconnect event object!!\n"));
					break;
				}
			} else {
				HANDLE	disconEvent, alarmEvent;

				//
				//	Need 32-bit thunk
				//


				Information->PdoEvents32.SlotNo = PdoData->SerialNo;
				Information->PdoEvents32.Flags = NDASBUS_QUERYFLAG_USERHANDLE;

				//
				//	Get user-mode event handles.
				//
				status = ObOpenObjectByPointer(
						PdoData->LanscsiAdapterPDO.DisconEventToService,
						0,
						NULL,
						EVENT_ALL_ACCESS,
						*ExEventObjectType,
						UserMode,
						&disconEvent
					);
				if(!NT_SUCCESS(status)) {
					ObDereferenceObject(PdoData->Self);
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Could not open Disconnect event object!!\n"));
					break;
				}
				status = ObOpenObjectByPointer(
						PdoData->LanscsiAdapterPDO.AlarmEventToService,
						0,
						NULL,
						EVENT_ALL_ACCESS,
						*ExEventObjectType,
						UserMode,
						&alarmEvent
					);
				if(!NT_SUCCESS(status)) {
					ObDereferenceObject(PdoData->Self);
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Could not open Disconnect event object!!\n"));
					break;
				}


				//
				//	Thunk event handles to 32 bit
				//

				status = RtlULongPtrToULong((ULONG_PTR)disconEvent,
								(PULONG)&Information->PdoEvents32.DisconEvent);
				if(!NT_SUCCESS(status)) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PDOEVENTS: error conversion. Handle=%p\n", disconEvent));
					ObDereferenceObject(PdoData->Self);
					break;
				}

				status = RtlULongPtrToULong((ULONG_PTR)alarmEvent,
								(PULONG)&Information->PdoEvents32.AlarmEvent);
				if(!NT_SUCCESS(status)) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PDOEVENTS: error conversion. Handle=%p\n", alarmEvent));
					ObDereferenceObject(PdoData->Self);
					break;
				}
			}

		} else {
			if(Request32) {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PDOEVENTS: 32-bit thunking not supported.\n"));

				ObDereferenceObject(PdoData->Self);
				status = STATUS_INVALID_DEVICE_REQUEST;
				break;
			}

			Information->PdoEvents.SlotNo = PdoData->SerialNo;
			Information->PdoEvents.Flags = 0;
			Information->PdoEvents.DisconEvent = PdoData->LanscsiAdapterPDO.DisconEventToService;
			Information->PdoEvents.AlarmEvent = PdoData->LanscsiAdapterPDO.AlarmEventToService;
		}

		ObDereferenceObject(PdoData->Self);
		break;
   }
	case INFORMATION_ISREGISTERED: {

		ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

		status = LSBus_IsRegistered(FdoData, Query->SlotNo);
		break;
	}

	case INFORMATION_PDOSLOTLIST: {
		LONG	outputLength;
		LONG	entryCnt;

		if(OutBufferLength < FIELD_OFFSET(NDASBUS_INFORMATION, PdoSlotList)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PDOSLOTLIST: Buffer size is less than required\n"));
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		Information->Size = 0;
		Information->InfoClass = INFORMATION_PDOSLOTLIST;

		//
		//	Add the size of information header.
		//
		outputLength = FIELD_OFFSET(NDASBUS_INFORMATION, PdoSlotList);
		outputLength += sizeof(UINT32);					// SlotNoCnt
		entryCnt = 0;

		for (entry = FdoData->ListOfPDOs.Flink;
			entry != &FdoData->ListOfPDOs;
			entry = entry->Flink) {

			PdoData = CONTAINING_RECORD(entry, PDO_DEVICE_DATA, Link);

			//
			//	Add the size of each slot entry.
			//
			outputLength += sizeof(UINT32);
			if(outputLength > OutBufferLength) {
				continue;
			}

			Information->PdoSlotList.SlotNo[entryCnt] = PdoData->SerialNo;
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PDOSLOTLIST: Entry #%u: %u\n", entryCnt, PdoData->SerialNo));

			entryCnt ++;
		}

		if(outputLength > OutBufferLength) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PDOSLOTLIST: Could not find a Device(%d).\n", Query->SlotNo));
			*OutBufferLenNeeded = outputLength;
			status = STATUS_BUFFER_TOO_SMALL;
		} else {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PDOSLOTLIST: Entry count:%d.\n", entryCnt));
			Information->Size = outputLength;
			Information->PdoSlotList.SlotNoCnt = entryCnt;
			*OutBufferLenNeeded = outputLength;
		}

		break;
	}
	case INFORMATION_PDOFILEHANDLE: {
		PWCHAR	devName;
		USHORT	devNameLen;
		OBJECT_ATTRIBUTES  objectAttr;
		UNICODE_STRING		objName;
		HANDLE				devFileHandle;
		IO_STATUS_BLOCK		ioStatus;

		devName = NULL;


		for (entry = FdoData->ListOfPDOs.Flink;
			entry != &FdoData->ListOfPDOs;
			entry = entry->Flink) {

				PdoData = CONTAINING_RECORD(entry, PDO_DEVICE_DATA, Link);
				if(Query->SlotNo == PdoData->SerialNo) {
					ObReferenceObject(PdoData->Self);
					break;
				}
				PdoData = NULL;

		}

		if(PdoData == NULL) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Could not find the PDO:%u.\n", Query->SlotNo));
			status = STATUS_NO_SUCH_DEVICE;
			break;
		}


		//
		//	Get the name of the physical device object.
		//

		status = GetPhyDeviceName(PdoData->Self, &devName, &devNameLen);
		if(!NT_SUCCESS(status)) {
			ObDereferenceObject(PdoData->Self);
			break;
		}


		//
		//	Build unicode name of the device.
		//	Get rid of NULL termination from the length
		//

		objName.Buffer = devName;
		if(devNameLen > 0 && devName[devNameLen/sizeof(WCHAR) - 1] == L'\0') {
			objName.MaximumLength = devNameLen;
			objName.Length = devNameLen - sizeof(WCHAR);
		} else {
			objName.MaximumLength = objName.Length = devNameLen;
		}

		//
		//	Open a device file.
		//

		if(Query->Flags & NDASBUS_QUERYFLAG_USERHANDLE) {
			if(Request32 == FALSE)
				Information->PdoFile.Flags = NDASBUS_QUERYFLAG_USERHANDLE;
			else
				Information->PdoFile32.Flags = NDASBUS_QUERYFLAG_USERHANDLE;

			InitializeObjectAttributes(&objectAttr, &objName, OBJ_CASE_INSENSITIVE , NULL, NULL);
		} else {
			if(Request32 == FALSE)
				Information->PdoFile.Flags = 0;
			else
				Information->PdoFile32.Flags = 0;

			InitializeObjectAttributes(&objectAttr, &objName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
		}


		status = ZwCreateFile(
						&devFileHandle,
						SYNCHRONIZE|FILE_READ_DATA,
						&objectAttr,
						&ioStatus,
						NULL,
						0,
						FILE_SHARE_READ|FILE_SHARE_WRITE,
						FILE_OPEN,
						FILE_SYNCHRONOUS_IO_NONALERT,
						NULL,
						0);
		if(!NT_SUCCESS(status)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Could not open the PDO:%u. STATUS=%08lx\n", Query->SlotNo, status));
			ExFreePool(devName);
			ObDereferenceObject(PdoData->Self);
			break;
		}


		//
		//	Set the return values
		//
		if(Request32 == FALSE) {

			Information->PdoFile.SlotNo = PdoData->SerialNo;
			Information->PdoFile.PdoFileHandle = devFileHandle;
		} else {
			
			//
			//	Need 32 bit thunking
			//

			status = RtlULongPtrToULong((ULONG_PTR)devFileHandle,
								(PULONG)&Information->PdoFile32.PdoFileHandle);
			if(!NT_SUCCESS(status)) {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PDOEVENTS: error conversion. Handle=%p\n", devFileHandle));
				ObDereferenceObject(PdoData->Self);
				break;
			}

			Information->PdoFile32.SlotNo = PdoData->SerialNo;
		}

		if(devName)
			ExFreePool(devName);

		ObDereferenceObject(PdoData->Self);
		break;
   }

	default:
		status = STATUS_INVALID_PARAMETER;
	}

	ExReleaseFastMutex (&FdoData->Mutex);
    KeLeaveCriticalRegion();

	return status;
}


//
//	Plug in a device on NDAS bus in KernelMode.
//
NTSTATUS
LSBus_PlugInLSBUSDevice(
			PFDO_DEVICE_DATA				FdoData,
			PNDASBUS_PLUGIN_HARDWARE_EX2	PlugIn
	){
	HANDLE						DisconEvent;
	HANDLE						AlarmEvent;
    NTSTATUS					status;

    //
	//	Create events
    //
	status = ZwCreateEvent(
							&DisconEvent,
							GENERIC_READ,
							NULL,
							NotificationEvent,
							FALSE
						);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PlugInLSBUSDevice: ZwCreateEvent() failed. Disconnection event.\n"));
		return status;
	}

	status = ZwCreateEvent(
							&AlarmEvent,
							GENERIC_READ,
							NULL,
							NotificationEvent,
							FALSE
						);
    if(!NT_SUCCESS(status)) {
		ZwClose(DisconEvent);
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PlugInLSBUSDevice: ExAllocatePoolWithTag() failed. AlarmEvent\n"));
		return status;
    }

	//
	//	Set up NDASBUS_PLUGIN_HARDWARE_EX2 structure.
	//
	PlugIn->DisconEvent = DisconEvent;
	PlugIn->AlarmEvent = AlarmEvent;

	status = Bus_PlugInDevice(PlugIn, PlugIn->Size, FdoData, FALSE, KernelMode, TRUE);

	//
	//	Close handle to decrease one reference from events.
	//
	ZwClose(AlarmEvent);
	ZwClose(DisconEvent);

	return status;
}

NTSTATUS
LSBus_PlugOutLSBUSDevice(
			PFDO_DEVICE_DATA	FdoData,
			ULONG				SlotNo,
			BOOLEAN				Persistency
	) {
	NDASBUS_UNPLUG_HARDWARE		UnPlug;
	NTSTATUS	status;
	PPDO_DEVICE_DATA			pdoData;

	//
	//	Set/Reset persistency.
	//

	pdoData = LookupPdoData(FdoData, SlotNo);
	if(pdoData) {
		pdoData->Persistent = Persistency;
		ObDereferenceObject(pdoData->Self);
	} else {
		return STATUS_NOT_FOUND;
	}

	//
	//	Unplug
	//

	UnPlug.Size = sizeof(NDASBUS_UNPLUG_HARDWARE);
	UnPlug.SerialNo = SlotNo;

	status = Bus_UnPlugDevice (
							&UnPlug,
							FdoData
					);

		return status;
	}

//
//	Plug in a NDAS device with a LUR descriptor.
//
NTSTATUS
LSBus_AddTargetWithLurDesc(
		PFDO_DEVICE_DATA	FdoData,
		PLURELATION_DESC	LurDesc,
		ULONG				SlotNo
) {
	PPDO_DEVICE_DATA	pdoData;
	NTSTATUS			status;

	status = STATUS_SUCCESS;
	//
	//	Verify LurDesc
	//	TODO:
	//
	
	// Find Pdo Data...
	pdoData = LookupPdoData(FdoData, SlotNo);
	if(pdoData == NULL) {
		Bus_KdPrint_Cont (FdoData, BUS_DBG_IOCTL_ERROR, ("no pdo\n"));
		return STATUS_NOT_FOUND;
}

	//
	//	Copy AddDevInfo into the PDO.
	//
	if(pdoData->LanscsiAdapterPDO.AddDevInfo) {
		Bus_KdPrint_Cont (FdoData, BUS_DBG_IOCTL_ERROR, ("AddDevInfo already set.\n"));

		status = STATUS_DEVICE_ALREADY_ATTACHED;
		goto cleanup;
	}
	pdoData->LanscsiAdapterPDO.AddDevInfo = ExAllocatePoolWithTag(NonPagedPool, LurDesc->Length, BUSENUM_POOL_TAG);
	if(pdoData->LanscsiAdapterPDO.AddDevInfo == NULL) {
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto cleanup;
	} else {
		RtlCopyMemory(pdoData->LanscsiAdapterPDO.AddDevInfo, LurDesc, LurDesc->Length);
		status = STATUS_SUCCESS;
	}
	pdoData->LanscsiAdapterPDO.AddDevInfoLength = LurDesc->Length;
	pdoData->LanscsiAdapterPDO.Flags |= LSDEVDATA_FLAG_LURDESC;

	//
	//	Notify to NDASSCSI
	//
	Bus_KdPrint(FdoData, BUS_DBG_IOCTL_ERROR, ("IOCTL_NDASBUS_ADD_TARGET SetEvent AddTargetEvent!\n"));
	KeSetEvent(&pdoData->LanscsiAdapterPDO.AddTargetEvent, IO_NO_INCREMENT, FALSE);

cleanup:
	ObDereferenceObject(pdoData->Self);

	return status;
}

//
//	Plug in a NDAS device with a LUR descriptor.
//
NTSTATUS
LSBus_AddTarget(
		PFDO_DEVICE_DATA			FdoData,
		PNDASBUS_ADD_TARGET_DATA	AddTargetData
) {
	PPDO_DEVICE_DATA	pdoData;
	NTSTATUS			status;

	status = STATUS_SUCCESS;
	//
	//	Verify LurDesc
	//	TODO:
	//
	
	// Find Pdo Data...
	pdoData = LookupPdoData(FdoData, AddTargetData->ulSlotNo);
	if(pdoData == NULL) {
		Bus_KdPrint_Cont (FdoData, BUS_DBG_IOCTL_ERROR, ("no pdo for Slot %u\n", AddTargetData->ulSlotNo));
		return STATUS_NOT_FOUND;
	}

	//
	//	Copy AddDevInfo into the PDO.
	//
	if(pdoData->LanscsiAdapterPDO.AddDevInfo) {
		Bus_KdPrint_Cont (FdoData, BUS_DBG_IOCTL_ERROR, ("AddDevInfo already set.\n"));

		status = STATUS_DEVICE_ALREADY_ATTACHED;
		goto cleanup;
	}
	pdoData->LanscsiAdapterPDO.AddDevInfo = ExAllocatePoolWithTag(NonPagedPool, AddTargetData->ulSize, BUSENUM_POOL_TAG);
	if(pdoData->LanscsiAdapterPDO.AddDevInfo == NULL) {
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto cleanup;
	}

	RtlCopyMemory(pdoData->LanscsiAdapterPDO.AddDevInfo, AddTargetData, AddTargetData->ulSize);
	pdoData->LanscsiAdapterPDO.AddDevInfoLength = AddTargetData->ulSize;

	//
	// Init PDO status
	//

	pdoData->LanscsiAdapterPDO.LastAdapterStatus = NDASSCSI_ADAPTER_STATUS_INIT;
	pdoData->LanscsiAdapterPDO.DeviceMode = AddTargetData->DeviceMode;
	InitializeListHead(&pdoData->LanscsiAdapterPDO.NdasPdoEventQueue);

	//
	//	Notify to NDASCSI
	//
	Bus_KdPrint(FdoData, BUS_DBG_IOCTL_ERROR, ("IOCTL_NDASBUS_ADD_TARGET SetEvent AddTargetEvent!\n"));
	KeSetEvent(&pdoData->LanscsiAdapterPDO.AddTargetEvent, IO_NO_INCREMENT, FALSE);

cleanup:
	ObDereferenceObject(pdoData->Self);

	return status;
}


//////////////////////////////////////////////////////////////////////////
//
// PDO event queue
//
//////////////////////////////////////////////////////////////////////////

PNDASBUS_PDOEVENT_ENTRY
NdasBusCreatePdoStatusItem(
	 ULONG	AdapterStatus
){
	PNDASBUS_PDOEVENT_ENTRY pdoEventEntry;

	pdoEventEntry = ExAllocatePoolWithTag(
							NonPagedPool,
							sizeof(NDASBUS_PDOEVENT_ENTRY),
							NDBUS_POOLTAG_PDOSTATUS);
	if(pdoEventEntry == NULL)
		return NULL;

	pdoEventEntry->AdapterStatus = AdapterStatus;
	InitializeListHead(&pdoEventEntry->AdapterStatusQueueEntry);

	return pdoEventEntry;
}

VOID
NdasBusDestroyPdoStatusItem(
	 PNDASBUS_PDOEVENT_ENTRY PdoEventEntry
){
	if(PdoEventEntry == NULL) {
		ASSERT(FALSE);
		return;
	}

	ExFreePoolWithTag(PdoEventEntry, NDBUS_POOLTAG_PDOSTATUS);
}

VOID
NdasBusQueuePdoStatusItem(
	PPDO_LANSCSI_DEVICE_DATA NdasPdoData,
	PNDASBUS_PDOEVENT_ENTRY PdoEventEntry
){
	InsertTailList(&NdasPdoData->NdasPdoEventQueue,
		&PdoEventEntry->AdapterStatusQueueEntry);

	ASSERT(NdasPdoData->NdasPdoEventEntryCount >= 0);
	NdasPdoData->NdasPdoEventEntryCount ++;

	//
	// Check to see if the number of entries reaches the threshold.
	// Free the oldest entry if so.
	//
	if(NdasPdoData->NdasPdoEventEntryCount > NDASPDO_EVENTQUEUE_THRESHOLD) {
		PLIST_ENTRY entry;
		PNDASBUS_PDOEVENT_ENTRY pdoStatusEntry;

		while(NdasPdoData->NdasPdoEventEntryCount > NDASPDO_EVENTQUEUE_THRESHOLD) {
			entry = RemoveHeadList(&NdasPdoData->NdasPdoEventQueue);
			if(entry == &NdasPdoData->NdasPdoEventQueue) {
				ASSERT(NdasPdoData->NdasPdoEventEntryCount == 0);
				break;
			}
			// Decrement the count.
			NdasPdoData->NdasPdoEventEntryCount --;
			// Free the entry
			pdoStatusEntry = CONTAINING_RECORD(entry, NDASBUS_PDOEVENT_ENTRY, AdapterStatusQueueEntry);
			NdasBusDestroyPdoStatusItem(pdoStatusEntry);
		}
	}

}

PNDASBUS_PDOEVENT_ENTRY
NdasBusDequeuePdoStatusItem(
	PPDO_LANSCSI_DEVICE_DATA NdasPdoData
){
	PLIST_ENTRY entry;
	PNDASBUS_PDOEVENT_ENTRY pdoEventEntry;

	entry = RemoveHeadList(&NdasPdoData->NdasPdoEventQueue);
	if(entry == &NdasPdoData->NdasPdoEventQueue)
		return NULL;

	ASSERT(NdasPdoData->NdasPdoEventEntryCount >= 1);
	NdasPdoData->NdasPdoEventEntryCount --;

	pdoEventEntry = CONTAINING_RECORD(entry, NDASBUS_PDOEVENT_ENTRY, AdapterStatusQueueEntry);

	return pdoEventEntry;
}

VOID
NdasBusCleanupPdoStatusQueue(
	PPDO_LANSCSI_DEVICE_DATA NdasPdoData
){
	PLIST_ENTRY entry;
	PNDASBUS_PDOEVENT_ENTRY pdoEventEntry;

	do {
		entry = RemoveHeadList(&NdasPdoData->NdasPdoEventQueue);
		if(entry == &NdasPdoData->NdasPdoEventQueue)
			break;

		pdoEventEntry = CONTAINING_RECORD(entry, NDASBUS_PDOEVENT_ENTRY, AdapterStatusQueueEntry);
		NdasBusDestroyPdoStatusItem(pdoEventEntry);
	} while(TRUE);

}
