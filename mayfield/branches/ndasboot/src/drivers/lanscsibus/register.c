#include <ntddk.h>
#include <scsi.h>
#include <ntddscsi.h>

#include "lanscsibus.h"
#include "lsbusioctl.h"
#include "lsminiportioctl.h"
#include "ndas/ndasdib.h"

#include "busenum.h"
#include "stdio.h"
#include "LanscsiBusProc.h"


#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "Register"


#define LSREG_POOLTAG_LURNTABLE	'trSL'

//
//	Registry key name of NDAS devices
//
#define LSBUS_NDASREG_NAME			L"Devices"

//
//	NDAS device information
//
#define LSBUS_NDASREG_DEVREGNAME	L"Dev%ld"					// Folder Name
//	Miniport
#define LSBUS_NDASREG_SLOTNO		L"SlotNo"					// 4 bytes
#define LSBUS_NDASREG_HWIDS			L"HardwareIDs"				// Variable length.
#define LSBUS_NDASREG_PLUGINTIME	L"PlugInTime"				// 4 bytes
#define LSBUS_NDASREG_FLAGS			L"Flags"					// 4 bytes
// Lur
#define LSBUS_NDASREG_LURID			L"LurId"					// 4 bytes
#define LSBUS_NDASREG_DEVTYPE		L"DevType"					// 4 bytes
#define LSBUS_NDASREG_DEVACCRIGHT	L"AccessRight"				// 4 bytes
#define LSBUS_NDASREG_DEVMAXBPR		L"MaxBlocksPerRequest"		// 4 bytes
#define LSBUS_NDASREG_DEVBYTESBLK	L"BytesInBlock"				// 4 bytes
#define LSBUS_NDASREG_LUROPTIONS	L"LurOptions"				// 4 bytes
#define LSBUS_NDASREG_LURNCNT		L"LurnCount"				// 4 bytes

//
//	Lurn information
//
#define LSBUS_NDASREG_LURNREG_NAME	L"LURN%ld"					// Folder name

#define LSBUS_NDASREG_LURNID		L"LurnId"					// 4 bytes
#define LSBUS_NDASREG_LURNTYPE		L"LurnType"					// 4 bytes
#define LSBUS_NDASREG_INITDATA		L"InitData"					// 80 bytes
#define LSBUS_NDASREG_STARTADDR		L"StartAddress"				// 8 bytes
#define LSBUS_NDASREG_ENDADDR		L"EndAddress"				// 8 bytes
#define LSBUS_NDASREG_PENDADDR		L"PhysicalEndBlockAddr"		// 8 bytes
#define LSBUS_NDASREG_UNITBLOCKS	L"UnitBlocks"				// 8 bytes
#define LSBUS_NDASREG_MAXBPR		L"MaxBlocksPerRequest"		// 4 bytes
#define LSBUS_NDASREG_BYTESBLK		L"BytesInBlock"				// 4 bytes
#define LSBUS_NDASREG_ACCRIGHT		L"AccessRight"				// 4 bytes
#define LSBUS_NDASREG_PARENTIDX		L"ParentIdx"				// 4 bytes
#define	LSBUS_NDASREG_CHILDCNT		L"ChildCount"				// 4 bytes
#define	LSBUS_NDASREG_CHILDIDX		L"ChildIndexes"				// 4 bytes * ChildCount

//
//	Max numbers
//
#define	MAX_DEVICES_IN_NDAS_REGISTRY	(MAXLONG)


//////////////////////////////////////////////////////////////////////////
//
//	Registry primitives.
//
NTSTATUS
ReadKeyValue(
		HANDLE	RegKey,
		PWCHAR	KeyName,
		ULONG	KeyType,
		PVOID	Buffer,
		ULONG	BufferLength,
		PULONG	BufferLengthNeeded
) {
	PKEY_VALUE_PARTIAL_INFORMATION	keyValue;
	NTSTATUS						status;
	ULONG							outLength;
	UNICODE_STRING					valueName;

	outLength = 0;
	keyValue = (PKEY_VALUE_PARTIAL_INFORMATION)ExAllocatePoolWithTag(PagedPool, 512, LSBUS_POOTAG_KEYINFO);
	RtlInitUnicodeString(&valueName, KeyName);
	status = ZwQueryValueKey(RegKey, &valueName, KeyValuePartialInformation, keyValue, 512, &outLength);
	if(BufferLengthNeeded) {
		*BufferLengthNeeded = keyValue->DataLength;
	}
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ZwQueryValueKey() failed. ValueKeyName:%ws\n", KeyName));
		ExFreePool(keyValue);
		return	status;
	}
	if(keyValue->Type != KeyType) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Wrong value key type. ValueKeyName:%ws\n", KeyName));
		ExFreePool(keyValue);
		return	status;
	}
	if(BufferLength < keyValue->DataLength) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Buffer too small. ValueKeyName:%ws\n", KeyName));
		ExFreePool(keyValue);
		return STATUS_BUFFER_TOO_SMALL;
	}

	RtlCopyMemory(Buffer, keyValue->Data, keyValue->DataLength);

	ExFreePool(keyValue);
	return STATUS_SUCCESS;
}

NTSTATUS
WriteKeyValue(
		HANDLE	RegKey,
		PWCHAR	KeyName,
		ULONG	KeyType,
		PVOID	Buffer,
		ULONG	BufferLength
) {
	NTSTATUS						status;
	UNICODE_STRING					valueName;

	RtlInitUnicodeString(&valueName, KeyName);
	status = ZwSetValueKey(
						RegKey,
						&valueName,
						0,
						KeyType,
						Buffer,
						BufferLength
					);

	return status;
}


//
//	Open device parameter registry.
//
NTSTATUS
Reg_OpenDeviceRegistry(
		PDEVICE_OBJECT	DeviceObject,
		HANDLE			*DeviceReg,
		ACCESS_MASK		AccessMask
	){
    HANDLE				regKey;
    NTSTATUS			status;

    //
    // Create or open the key.
    //
    status = IoOpenDeviceRegistryKey(
								DeviceObject,
								PLUGPLAY_REGKEY_DEVICE,
								AccessMask,
								&regKey);
    if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("IoOpenDeviceRegistryKey() failed. NTSTATUS:%08lx\n", status));
        regKey = NULL;
    }

	*DeviceReg = regKey;

    return status;
}


//
//	Open LSBUS_NDASREG_NAME registry.
//
NTSTATUS
Reg_OpenNdasDeviceRegistry(
		HANDLE			*NdasDeviceReg,
		ACCESS_MASK		AccessMask,
		HANDLE			RootReg
	){
    HANDLE				regKey;
    NTSTATUS			status;
	OBJECT_ATTRIBUTES	objectAttributes;
	UNICODE_STRING		objectName;


	RtlInitUnicodeString(&objectName, LSBUS_NDASREG_NAME);
	InitializeObjectAttributes( &objectAttributes,
								&objectName,
								OBJ_KERNEL_HANDLE,
								RootReg,
								NULL
							);
	status = ZwCreateKey(
						&regKey,
						AccessMask,
						&objectAttributes,
						0,
						NULL,
						REG_OPTION_NON_VOLATILE,
						NULL
					);

	*NdasDeviceReg = regKey;

    return status;
}

//////////////////////////////////////////////////////////////////////////
//
//	Plug in devices by reading the registry.
//


//
//	Build LURELATION_NODE descriptor by reading the registry.
//
NTSTATUS
ReadLurnDescFromRegistry(
		HANDLE					DeviceReg,
		PLURELATION_NODE_DESC	LurnDesc
) {
	NTSTATUS	status;
	//
	//	read LUR Node information
	//

	//	LurnId
	status = ReadKeyValue(
						DeviceReg,
						LSBUS_NDASREG_LURNID,
						REG_DWORD,
						&LurnDesc->LurnId,
						sizeof(LurnDesc->LurnId),
						NULL
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_LURNID));
		return	status;
	}
	//	LurnType
	status = ReadKeyValue(
						DeviceReg,
						LSBUS_NDASREG_LURNTYPE,
						REG_BINARY,
						&LurnDesc->LurnType,
						sizeof(LurnDesc->LurnType),
						NULL
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_LURNTYPE));
		return	status;
	}
	//	LurnInit
	status = ReadKeyValue(
						DeviceReg,
						LSBUS_NDASREG_INITDATA,
						REG_BINARY,
						&LurnDesc->InitData,
						sizeof(LurnDesc->InitData),
						NULL
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_INITDATA));
		return	status;
	}
	//	StartAddress
	status = ReadKeyValue(
						DeviceReg,
						LSBUS_NDASREG_STARTADDR,
						REG_BINARY,
						&LurnDesc->StartBlockAddr,
						sizeof(LurnDesc->StartBlockAddr),
						NULL
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_STARTADDR));
		return	status;
	}
	//	EndAddress
	status = ReadKeyValue(
						DeviceReg,
						LSBUS_NDASREG_ENDADDR,
						REG_BINARY,
						&LurnDesc->EndBlockAddr,
						sizeof(LurnDesc->EndBlockAddr),
						NULL
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_ENDADDR));
		return	status;
	}
	//	UnitBlocks
	status = ReadKeyValue(
						DeviceReg,
						LSBUS_NDASREG_UNITBLOCKS,
						REG_BINARY,
						&LurnDesc->UnitBlocks,
						sizeof(LurnDesc->UnitBlocks),
						NULL
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_UNITBLOCKS));
		return	status;
	}
	//	MaxBlocksPerRequest
	status = ReadKeyValue(
						DeviceReg,
						LSBUS_NDASREG_MAXBPR,
						REG_DWORD,
						&LurnDesc->MaxBlocksPerRequest,
						sizeof(LurnDesc->MaxBlocksPerRequest),
						NULL
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_MAXBPR));
		return	status;
	}
	//	BytesInBlock
	status = ReadKeyValue(
						DeviceReg,
						LSBUS_NDASREG_BYTESBLK,
						REG_DWORD,
						&LurnDesc->BytesInBlock,
						sizeof(LurnDesc->BytesInBlock),
						NULL
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_BYTESBLK));
		return	status;
	}
	//	AccessRight
	status = ReadKeyValue(
						DeviceReg,
						LSBUS_NDASREG_ACCRIGHT,
						REG_DWORD,
						&LurnDesc->AccessRight,
						sizeof(LurnDesc->AccessRight),
						NULL
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_ACCRIGHT));
		return	status;
	}
	//	ParentIdx
	status = ReadKeyValue(
						DeviceReg,
						LSBUS_NDASREG_PARENTIDX,
						REG_DWORD,
						&LurnDesc->LurnParent,
						sizeof(LurnDesc->LurnParent),
						NULL
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_PARENTIDX));
		return	status;
	}
	//	ChildCount
	status = ReadKeyValue(
						DeviceReg,
						LSBUS_NDASREG_CHILDCNT,
						REG_DWORD,
						&LurnDesc->LurnChildrenCnt,
						sizeof(LurnDesc->LurnChildrenCnt),
						NULL
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_CHILDCNT));
		return	status;
	}
	//	ChildIndexes
	if(LurnDesc->LurnChildrenCnt) {
		status = ReadKeyValue(
						DeviceReg,
						LSBUS_NDASREG_CHILDIDX,
						REG_BINARY,
						&LurnDesc->LurnChildren,
						sizeof(ULONG) * LurnDesc->LurnChildrenCnt,
						NULL
					);
		if(!NT_SUCCESS(status)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_CHILDIDX));
			return	status;
		}
	}

	return STATUS_SUCCESS;
}

//
//	Build LURELATION descriptor by reading the registry.
//
NTSTATUS
BuildLurDescByRegistry(
		HANDLE				DeviceReg,
		PLURELATION_DESC	*LurDesc
) {
	PLURELATION_DESC		lurDesc;
	LONG					lurDescLength;
	ULONG					lurnCnt;
	NTSTATUS				status;
	HANDLE					lurnReg;
	PKEY_BASIC_INFORMATION	keyInfo;
	ULONG					outLength;
	ULONG					idxKey;
	OBJECT_ATTRIBUTES		objectAttributes;
	UNICODE_STRING			objectName;
	ULONG					lurnOffset;
	PLURELATION_NODE_DESC	lurnDesc;

	//
	//	LurnCnt
	//
	status = ReadKeyValue(
						DeviceReg,
						LSBUS_NDASREG_LURNCNT,
						REG_DWORD,
						&lurnCnt,
						sizeof(lurnCnt),
						NULL
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_LURNCNT));
		return	status;
	}

	//
	//	One child can have two parent.
	//
	lurDescLength = SIZE_OF_LURELATION_DESC() + lurnCnt * (sizeof(LURELATION_NODE_DESC) + sizeof(LONG));
	if(lurDescLength >= 0x10000) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Too large descriptor. Exceeds 64KB.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	lurDesc = ExAllocatePoolWithTag(PagedPool, lurDescLength, LSBUS_POOTAG_LURDESC);
	if(lurDesc == NULL) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ExAllocatePoolWithTag() failed. Low resource.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(lurDesc, lurDescLength);
	lurDesc->LurnDescCount = lurnCnt;

	//
	//	read LUR information
	//

	//	DevType
	status = ReadKeyValue(
						DeviceReg,
						LSBUS_NDASREG_DEVTYPE,
						REG_DWORD,
						&lurDesc->DevType,
						sizeof(lurDesc->DevType),
						NULL
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_DEVTYPE));
		ExFreePool(lurDesc);
		return	status;
	}
	// AccessRight
	status = ReadKeyValue(
						DeviceReg,
						LSBUS_NDASREG_DEVACCRIGHT,
						REG_DWORD,
						&lurDesc->AccessRight,
						sizeof(lurDesc->AccessRight),
						NULL
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_DEVACCRIGHT));
		ExFreePool(lurDesc);
		return	status;
	}
	// MaxBlocksPerRequest
	status = ReadKeyValue(
						DeviceReg,
						LSBUS_NDASREG_DEVMAXBPR,
						REG_DWORD,
						&lurDesc->MaxBlocksPerRequest,
						sizeof(lurDesc->MaxBlocksPerRequest),
						NULL
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_DEVMAXBPR));
		ExFreePool(lurDesc);
		return	status;
	}
	// BytesInBlock
	status = ReadKeyValue(
						DeviceReg,
						LSBUS_NDASREG_DEVBYTESBLK,
						REG_DWORD,
						&lurDesc->BytesInBlock,
						sizeof(lurDesc->BytesInBlock),
						NULL
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_DEVBYTESBLK));
		ExFreePool(lurDesc);
		return	status;
	}
	// LurId
	status = ReadKeyValue(
						DeviceReg,
						LSBUS_NDASREG_LURID,
						REG_DWORD,
						&lurDesc->LurId,
						sizeof(lurDesc->LurId),
						NULL
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_LURID));
		ExFreePool(lurDesc);
		return	status;
	}
/*
	// SlotNo
	status = ReadKeyValue(
						DeviceReg,
						LSBUS_NDASREG_SLOTNO,
						REG_DWORD,
						&lurDesc->SlotNo,
						sizeof(lurDesc->SlotNo),
						NULL
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_SLOTNO));
		ExFreePool(lurDesc);
		return	status;
	}
*/
	// LurFlags
	status = ReadKeyValue(
						DeviceReg,
						LSBUS_NDASREG_LUROPTIONS,
						REG_DWORD,
						&lurDesc->LurOptions,
						sizeof(lurDesc->LurOptions),
						NULL
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_LUROPTIONS));
		ExFreePool(lurDesc);
		return	status;
	}

	//
	//	read LURN information.
	//
	keyInfo = (PKEY_BASIC_INFORMATION)ExAllocatePoolWithTag(PagedPool, 512, LSBUS_POOTAG_KEYINFO);
	if(!keyInfo) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ExAllocatePoolWithTag(KEY_BASIC_INFORMATION) failed.\n"));
		ExFreePool(lurDesc);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	lurnOffset = 0;
	for(idxKey = 0 ; idxKey < lurnCnt; idxKey ++) {
		lurnDesc = (PLURELATION_NODE_DESC)( (PBYTE)lurDesc->LurnDesc + lurnOffset);

		status = ZwEnumerateKey(
						DeviceReg,
						idxKey,
						KeyBasicInformation,
						keyInfo,
						128,
						&outLength
						);
		if(status == STATUS_NO_MORE_ENTRIES) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("No more entries.\n"));
			break;
		}
		if(status != STATUS_SUCCESS) {
			ASSERT(status != STATUS_BUFFER_OVERFLOW && status != STATUS_BUFFER_TOO_SMALL);
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ZwEnumerateKey() failed. NTSTATUS:%08lx\n", status));
			ExFreePool(keyInfo);
			ExFreePool(lurDesc);
			return status;
		}

		//
		//	Name verification
		//
		//	TODO
		//

		//
		//	Open a sub key (NdasDevices\Devxx) and plug in device with the registry key.
		//
		objectName.Length = objectName.MaximumLength = (USHORT)keyInfo->NameLength;
		objectName.Buffer = keyInfo->Name;
		InitializeObjectAttributes(		&objectAttributes,
										&objectName,
										OBJ_KERNEL_HANDLE,
										DeviceReg,
										NULL
								);
		status = ZwOpenKey(&lurnReg, KEY_READ, &objectAttributes);
		if(!NT_SUCCESS(status)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ZwOpenKey() failed. NTSTATUS:%08lx\n", status));
			continue;
		}

		status = ReadLurnDescFromRegistry(lurnReg, lurnDesc);

		ZwClose(lurnReg);

		if(!NT_SUCCESS(status)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadLurnDescFromRegistry() failed. NTSTATUS:%08lx\n", status));
			ExFreePool(keyInfo);
			ExFreePool(lurDesc);
			return status;
		}

		lurnOffset += sizeof(LURELATION_NODE_DESC) + sizeof(ULONG) * ( lurnDesc->LurnChildrenCnt - 1);
	}

	ExFreePool(keyInfo);

	//
	//	Set LUR length.
	//
	lurDesc->Length = (USHORT)(SIZE_OF_LURELATION_DESC() + lurnOffset);

	//
	//	set the return value.
	//
	*LurDesc = lurDesc;
	return STATUS_SUCCESS;
}

VOID
FreeLurDesc(
	PLURELATION_DESC	LurDesc
) {
	ExFreePool(LurDesc);
}

NTSTATUS
FindParentLurnDescAndAssocID(
		PLURELATION_DESC		LurDesc,
		PLURELATION_NODE_DESC	LurnDescTable[],
		PLURELATION_NODE_DESC	Lurn,
		PLURELATION_NODE_DESC	*Parent,
		PULONG					AssocId
) {
	PLURELATION_NODE_DESC	parent;
	ULONG	assocId;

	if(Lurn->LurnParent == Lurn->LurnId) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Root lurn!\n"));
		*Parent = NULL;
		*AssocId = 0;
		return STATUS_SUCCESS;
	}
	if(Lurn->LurnParent >= LurDesc->LurnDescCount || !LurnDescTable[Lurn->LurnParent]) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Lurn->LurnParent has a invalid value.\n"));
		return STATUS_INVALID_PARAMETER;
	}

	parent = LurnDescTable[Lurn->LurnParent];
	for(assocId = 0; assocId < parent->LurnChildrenCnt; assocId ++) {
		if( parent->LurnChildren[assocId] == Lurn->LurnId ) {
			break;
		}
	}
	if(assocId >= parent->LurnChildrenCnt) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Lurn does not belong to the parent.\n"));
		return STATUS_INVALID_PARAMETER;
	}

	*Parent = parent;
	*AssocId = assocId;

	return STATUS_SUCCESS;
}

//
//	Verify LUR descriptor with DIB in a NDAS device.
//
NTSTATUS
LSBus_VerifyLurDescWithDIB(
		PLURELATION_DESC	LurDesc
) {
	PLURELATION_NODE_DESC	lurnDesc;
	PLURELATION_NODE_DESC	parent;
	ULONG					assocId;
	NTSTATUS				status;
	ULONG					lurnOffset;
	ULONG					idxLurn;
	PLURELATION_NODE_DESC	*lurnDescTable;

	lurnDescTable = (PLURELATION_NODE_DESC *)ExAllocatePoolWithTag(PagedPool,
									sizeof(PLURELATION_NODE_DESC *)*LurDesc->LurnDescCount,
									LSREG_POOLTAG_LURNTABLE
								);
	if(!lurnDescTable) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ExAllocatePoolWithTag(LURELATION_NODE_DESC table) failed.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	RtlZeroMemory(lurnDescTable, sizeof(PLURELATION_NODE_DESC *)*LurDesc->LurnDescCount);

	lurnOffset = 0;
	status = STATUS_SUCCESS;
	for(idxLurn = 0 ; idxLurn < LurDesc->LurnDescCount; idxLurn ++) {
		lurnDesc = (PLURELATION_NODE_DESC)( (PBYTE)LurDesc->LurnDesc + lurnOffset);
		lurnDescTable[idxLurn] = lurnDesc;

		status = FindParentLurnDescAndAssocID(LurDesc, lurnDescTable, lurnDesc, &parent, &assocId);
		if(!NT_SUCCESS(status)) {
			break;
		}

		if(parent) {
			status = NCommVerifyLurnWithDIB(lurnDesc, parent->LurnType, assocId);
		} else {
			status = NCommVerifyLurnWithDIB(lurnDesc, LURN_NULL, 0);
		}
		if(!NT_SUCCESS(status)) {
			break;
		}
		Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("  Lurn #%d is verified.\n", lurnDesc->LurnId));

		lurnOffset = lurnDesc->NextOffset;
	}

	ExFreePool(lurnDescTable);

	return status;
}



//
//	Enumerate NDAS devices by reading registry.
//
NTSTATUS
EnumerateByRegistry(
		PFDO_DEVICE_DATA	FdoData,
		HANDLE				NdasDeviceReg,
		ULONG				PlugInTimeMask
	) {

	NTSTATUS					status;
	PKEY_BASIC_INFORMATION		keyInfo;
	ULONG						outLength;
	LONG						idxKey;
	OBJECT_ATTRIBUTES			objectAttributes;
	UNICODE_STRING				objectName;
	HANDLE						SubKey;
	PLURELATION_DESC			lurDesc;
	ULONG						SlotNo;
	ULONG						MaxBlocksPerRequest;
	ULONG						PlugInTime;


	keyInfo = (PKEY_BASIC_INFORMATION)ExAllocatePoolWithTag(PagedPool, 512, LSBUS_POOTAG_KEYINFO);
	if(!keyInfo) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ExAllocatePoolWithTag(KEY_BASIC_INFORMATION) failed.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	for(idxKey = 0 ; idxKey < MAX_DEVICES_IN_NDAS_REGISTRY; idxKey ++) {
		status = ZwEnumerateKey(
						NdasDeviceReg,
						idxKey,
						KeyBasicInformation,
						keyInfo,
						512,
						&outLength
						);

		if(status == STATUS_NO_MORE_ENTRIES) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("No more entries.\n"));
			break;
		}
		if(status != STATUS_SUCCESS) {
			ASSERT(status != STATUS_BUFFER_OVERFLOW && status != STATUS_BUFFER_TOO_SMALL);
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ZwEnumerateKey() failed. NTSTATUS:%08lx\n", status));
			ExFreePool(keyInfo);
			return STATUS_SUCCESS;
		}

		//
		//	Name verification
		//
		//	TODO
		//

		//
		//	Open a sub key (NdasDevices\Devxx) and plug in device with the registry key.
		//
		objectName.Length = objectName.MaximumLength = (USHORT)keyInfo->NameLength;
		objectName.Buffer = keyInfo->Name;
		InitializeObjectAttributes(		&objectAttributes,
										&objectName,
										OBJ_KERNEL_HANDLE,
										NdasDeviceReg,
										NULL
								);
		status = ZwOpenKey(&SubKey, KEY_READ, &objectAttributes);
		if(!NT_SUCCESS(status)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ZwOpenKey() failed. NTSTATUS:%08lx\n", status));
			continue;
		}

		//	Slot number
		status = ReadKeyValue(
						SubKey,
						LSBUS_NDASREG_SLOTNO,
						REG_DWORD,
						&SlotNo,
						sizeof(SlotNo),
						NULL
					);
		if(!NT_SUCCESS(status)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_SLOTNO));
			return	status;
		}
		//	MaxBlocksPerRequest
		status = ReadKeyValue(
						SubKey,
						LSBUS_NDASREG_DEVMAXBPR,
						REG_DWORD,
						&MaxBlocksPerRequest,
						sizeof(MaxBlocksPerRequest),
						NULL
					);
		if(!NT_SUCCESS(status)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_DEVMAXBPR));
			return	status;
		}

		//	PlugInTime
		status = ReadKeyValue(
						SubKey,
						LSBUS_NDASREG_PLUGINTIME,
						REG_DWORD,
						&PlugInTime,
						sizeof(PlugInTime),
						NULL
					);
		if(!NT_SUCCESS(status)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_PLUGINTIME));
			return	status;
		}

		//	HardwareIDs
		//	Utilize keyInfo to avoid memory allocation. 
		status = ReadKeyValue(
						SubKey,
						LSBUS_NDASREG_HWIDS,
						REG_MULTI_SZ,
						keyInfo->Name,
						512 - sizeof(keyInfo) + sizeof(WCHAR),
						&keyInfo->NameLength
					);
		if(!NT_SUCCESS(status)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_HWIDS));
			return	status;
		}

		if(PlugInTime & PlugInTimeMask) {

			status = BuildLurDescByRegistry(SubKey, &lurDesc);
			if(!NT_SUCCESS(status)) {
				ZwClose(SubKey);
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("BuildLurDescByRegistry() failed. NTSTATUS:%08lx\n", status));
				continue;
			}

			//
			//	Verify LUR descriptor with DIB in a NDAS device.
			//
			status = LSBus_VerifyLurDescWithDIB(lurDesc);
			if(!NT_SUCCESS(status)) {
				ZwClose(SubKey);
				FreeLurDesc(lurDesc);
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("LSBus_VerifyLurDescWithDIB() failed. NTSTATUS:%08lx\n", status));
				continue;
			}

			//
			//	Plug in a LanscsiBus device.
			//
			status = LSBus_PlugInLSBUSDevice(FdoData, SlotNo, keyInfo->Name, keyInfo->NameLength, MaxBlocksPerRequest);
			if(!NT_SUCCESS(status)) {
				ZwClose(SubKey);
				FreeLurDesc(lurDesc);
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("LSBus_PlugInLSBUSDevice() failed. NTSTATUS:%08lx\n", status));
				continue;
			}

			//
			//	Add the LUR to the BUS device which was just plugged in.
			//
			status = LSBus_AddNdasDeviceWithLurDesc(FdoData, lurDesc, SlotNo);
			if(!NT_SUCCESS(status)) {
				ZwClose(SubKey);
				FreeLurDesc(lurDesc);
				LSBus_PlugOutLSBUSDevice(FdoData, SlotNo);
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("LSBus_AddNdasDeviceWithLurDesc() failed. NTSTATUS:%08lx\n", status));
				continue;
			}
		}

		ZwClose(SubKey);
		FreeLurDesc(lurDesc);
	
	}

	ExFreePool(keyInfo);
	return STATUS_SUCCESS;
}


//
//	Plug in NDAS devices by reading registry.
//
NTSTATUS
LSBus_PlugInDeviceFromRegistry(
		PFDO_DEVICE_DATA	FdoData,
		ULONG				PlugInTimeMask
) {
	NTSTATUS			status;
	HANDLE				DeviceReg;
	HANDLE				NdasDeviceReg;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	DeviceReg = NULL;
	NdasDeviceReg = NULL;
	status = Reg_OpenDeviceRegistry(FdoData->UnderlyingPDO, &DeviceReg, KEY_READ|KEY_WRITE);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("OpenServiceRegistry() failed.\n"));
		return status;
	}
	status = Reg_OpenNdasDeviceRegistry(&NdasDeviceReg, KEY_READ, DeviceReg);
	if(!NT_SUCCESS(status)) {
		ZwClose(DeviceReg);
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("OpenNdasDeviceRegistry() failed.\n"));
		return status;
	}

	status = EnumerateByRegistry(FdoData, NdasDeviceReg, PlugInTimeMask);

	if(NdasDeviceReg)
		ZwClose(NdasDeviceReg);
	if(DeviceReg)
		ZwClose(DeviceReg);

	return status;
}


//
//	Queue a workitem to plug in NDAS device by reading registry.
//
NTSTATUS
LSBUS_QueueWorker_PlugInByRegistry(
		PFDO_DEVICE_DATA	FdoData,
		ULONG				PlugInTimeMask
	) {
	Bus_KdPrint_Def(BUS_DBG_SS_TRACE, ("entered.\n"));

	//
	//	TODO: queue an actual woker thread.
	//
	return LSBus_PlugInDeviceFromRegistry(FdoData, PlugInTimeMask);
}



//////////////////////////////////////////////////////////////////////////
//
//	Write a device information to the registry.
//

//
//	Write a LURN descriptor to the registry.
//
NTSTATUS
WriteLurnDescToRegistry(
		HANDLE					DeviceReg,
		PLURELATION_NODE_DESC	LurnDesc
) {
	NTSTATUS	status;
	//
	//	read LUR Node information
	//

	//	LurnId
	status = WriteKeyValue(
						DeviceReg,
						LSBUS_NDASREG_LURNID,
						REG_DWORD,
						&LurnDesc->LurnId,
						sizeof(LurnDesc->LurnId)
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_LURNID));
		return	status;
	}
	//	LurnType
	status = WriteKeyValue(
						DeviceReg,
						LSBUS_NDASREG_LURNTYPE,
						REG_BINARY,
						&LurnDesc->LurnType,
						sizeof(LurnDesc->LurnType)
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_LURNTYPE));
		return	status;
	}
	//	InitData
	status = WriteKeyValue(
						DeviceReg,
						LSBUS_NDASREG_INITDATA,
						REG_BINARY,
						&LurnDesc->InitData,
						sizeof(LurnDesc->InitData)
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_INITDATA));
		return	status;
	}
	//	StartAddress
	status = WriteKeyValue(
						DeviceReg,
						LSBUS_NDASREG_STARTADDR,
						REG_BINARY,
						&LurnDesc->StartBlockAddr,
						sizeof(LurnDesc->StartBlockAddr)
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_STARTADDR));
		return	status;
	}
	//	EndAddress
	status = WriteKeyValue(
						DeviceReg,
						LSBUS_NDASREG_ENDADDR,
						REG_BINARY,
						&LurnDesc->EndBlockAddr,
						sizeof(LurnDesc->EndBlockAddr)
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_ENDADDR));
		return	status;
	}
	//	UnitBlocks
	status = WriteKeyValue(
						DeviceReg,
						LSBUS_NDASREG_UNITBLOCKS,
						REG_BINARY,
						&LurnDesc->UnitBlocks,
						sizeof(LurnDesc->UnitBlocks)
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_UNITBLOCKS));
		return	status;
	}
	//	MaxBlocksPerRequest
	status = WriteKeyValue(
						DeviceReg,
						LSBUS_NDASREG_MAXBPR,
						REG_DWORD,
						&LurnDesc->MaxBlocksPerRequest,
						sizeof(LurnDesc->MaxBlocksPerRequest)
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_MAXBPR));
		return	status;
	}
	//	BytesInBlock
	status = WriteKeyValue(
						DeviceReg,
						LSBUS_NDASREG_BYTESBLK,
						REG_DWORD,
						&LurnDesc->BytesInBlock,
						sizeof(LurnDesc->BytesInBlock)
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_BYTESBLK));
		return	status;
	}
	//	AccessRight
	status = WriteKeyValue(
						DeviceReg,
						LSBUS_NDASREG_ACCRIGHT,
						REG_DWORD,
						&LurnDesc->AccessRight,
						sizeof(LurnDesc->AccessRight)
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_ACCRIGHT));
		return	status;
	}
	//	ParentIdx
	status = WriteKeyValue(
						DeviceReg,
						LSBUS_NDASREG_PARENTIDX,
						REG_DWORD,
						&LurnDesc->LurnParent,
						sizeof(LurnDesc->LurnParent)
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_PARENTIDX));
		return	status;
	}
	//	ChildCount
	status = WriteKeyValue(
						DeviceReg,
						LSBUS_NDASREG_CHILDCNT,
						REG_DWORD,
						&LurnDesc->LurnChildrenCnt,
						sizeof(LurnDesc->LurnChildrenCnt)
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_CHILDCNT));
		return	status;
	}
	//	ChildIndexes
	if(LurnDesc->LurnChildrenCnt) {
		status = WriteKeyValue(
						DeviceReg,
						LSBUS_NDASREG_CHILDIDX,
						REG_BINARY,
						&LurnDesc->LurnChildren,
						sizeof(ULONG) * LurnDesc->LurnChildrenCnt
					);
		if(!NT_SUCCESS(status)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_CHILDIDX));
			return	status;
		}
	}

	return STATUS_SUCCESS;
}


//
//	Write  LURELATION descriptor to the registry.
//
NTSTATUS
WriteLurDescToRegistry(
		HANDLE				DeviceReg,
		PLURELATION_DESC	LurDesc
) {
	NTSTATUS				status;
	HANDLE					lurnReg;
	PWSTR					nameBuffer;
	ULONG					idxKey;
	OBJECT_ATTRIBUTES		objectAttributes;
	UNICODE_STRING			objectName;
	ULONG					lurnOffset;
	PLURELATION_NODE_DESC	lurnDesc;


	//
	//	Write LUR information.
	//

	// LurId
	status = WriteKeyValue(
						DeviceReg,
						LSBUS_NDASREG_LURID,
						REG_DWORD,
						&LurDesc->LurId,
						sizeof(LurDesc->LurId)
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_LURID));
		return	status;
	}
	//	DevType
	status = WriteKeyValue(
						DeviceReg,
						LSBUS_NDASREG_DEVTYPE,
						REG_DWORD,
						&LurDesc->DevType,
						sizeof(LurDesc->DevType)
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_DEVTYPE));
		return	status;
	}
	// AccessRight
	status = WriteKeyValue(
						DeviceReg,
						LSBUS_NDASREG_DEVACCRIGHT,
						REG_DWORD,
						&LurDesc->AccessRight,
						sizeof(LurDesc->AccessRight)
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_DEVACCRIGHT));
		return	status;
	}
	// MaxBlocksPerRequest
	status = WriteKeyValue(
						DeviceReg,
						LSBUS_NDASREG_DEVMAXBPR,
						REG_DWORD,
						&LurDesc->MaxBlocksPerRequest,
						sizeof(LurDesc->MaxBlocksPerRequest)
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_DEVMAXBPR));
		return	status;
	}
	// BytesInBlock
	status = WriteKeyValue(
						DeviceReg,
						LSBUS_NDASREG_DEVBYTESBLK,
						REG_DWORD,
						&LurDesc->BytesInBlock,
						sizeof(LurDesc->BytesInBlock)
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_DEVBYTESBLK));
		return	status;
	}
	// LurFlags
	status = WriteKeyValue(
						DeviceReg,
						LSBUS_NDASREG_LUROPTIONS,
						REG_DWORD,
						&LurDesc->LurOptions,
						sizeof(LurDesc->LurOptions)
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_LUROPTIONS));
		return	status;
	}
	//	LurnCnt
	status = WriteKeyValue(
						DeviceReg,
						LSBUS_NDASREG_LURNCNT,
						REG_DWORD,
						&LurDesc->LurnDescCount,
						sizeof(LurDesc->LurnDescCount)
					);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_LURNCNT));
		return	status;
	}

	//
	//	Write LURN information.
	//
	nameBuffer = (PWSTR)ExAllocatePoolWithTag(PagedPool, 512, LSBUS_POOTAG_WSTRBUFFER);
	if(!nameBuffer) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ExAllocatePoolWithTag(KEY_BASIC_INFORMATION) failed.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	lurnOffset = 0;
	objectName.Length = 0;
	objectName.MaximumLength = 512;
	objectName.Buffer = nameBuffer;

	for(idxKey = 0 ; idxKey < LurDesc->LurnDescCount; idxKey ++) {
		lurnDesc = (PLURELATION_NODE_DESC)( (PBYTE)LurDesc->LurnDesc + lurnOffset);

		status = swprintf(
							objectName.Buffer,
							LSBUS_NDASREG_LURNREG_NAME,
							idxKey
						);
		if(!NT_SUCCESS(status)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("RtlStringCbPrintfW() failed. NTSTATUS:%08lx\n", status));
			break;
		}
		//
		//	Open a sub key (NdasDevices\Devxx) and plug in device with the registry key.
		//
		InitializeObjectAttributes(
							&objectAttributes,
							&objectName,
							OBJ_KERNEL_HANDLE,
							DeviceReg,
							NULL
						);
		status = ZwOpenKey(&lurnReg, KEY_READ|KEY_WRITE, &objectAttributes);
		if(!NT_SUCCESS(status)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ZwOpenKey() failed. NTSTATUS:%08lx\n", status));
			continue;
		}

		status = WriteLurnDescToRegistry(lurnReg, lurnDesc);

		ZwClose(lurnReg);

		if(!NT_SUCCESS(status)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadLurnDescFromRegistry() failed. NTSTATUS:%08lx\n", status));
			break;
		}

		lurnOffset = lurnDesc->NextOffset;
	}

	ExFreePool(nameBuffer);

	return status;
}



//
//	Enumerate NDAS devices by reading registry.
//
NTSTATUS
WriteDevInfoToRegistry(
		HANDLE							NdasDeviceReg,
		PLANSCSI_REGISTER_NDASDEV		NdasInfo
	) {

	NTSTATUS					status;
	PWSTR						nameBuffer;
	LONG						idxKey;
	OBJECT_ATTRIBUTES			objectAttributes;
	UNICODE_STRING				objectName;
	HANDLE						SubKey;

	nameBuffer = (PWSTR)ExAllocatePoolWithTag(PagedPool, 512, LSBUS_POOTAG_KEYINFO);
	if(!nameBuffer) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ExAllocatePoolWithTag(KEY_BASIC_INFORMATION) failed.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	//	Find a empty key.
	//
	objectName.Buffer = nameBuffer;
	objectName.MaximumLength = 512;
	for(idxKey = 0 ; idxKey < MAX_DEVICES_IN_NDAS_REGISTRY; idxKey ++) {

		status = swprintf(objectName.Buffer, LSBUS_NDASREG_DEVREGNAME, idxKey);
		if(!NT_SUCCESS(status)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("RtlStringCbPrintfW() failed. NTSTATUS:%08lx\n", status));
			goto cleanup;
		}

		InitializeObjectAttributes(&objectAttributes, NdasDeviceReg, OBJ_KERNEL_HANDLE, &objectName, NULL);

		status = ZwOpenKey(
					&SubKey,
					GENERIC_READ,
					&objectAttributes
			);
		if(!NT_SUCCESS(status)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ZwOpenKey() failed. found an empty key name. idxKey:%ld NTSTATUS:%08lx\n", idxKey, status));
			break;
		}

	}

	if(idxKey >= MAX_DEVICES_IN_NDAS_REGISTRY) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Could not find an empty key name. idxKey:%ld NTSTATUS:%08lx\n", idxKey, status));
		goto cleanup;
	}


	//	Slot number
	status = WriteKeyValue(
					SubKey,
					LSBUS_NDASREG_SLOTNO,
					REG_DWORD,
					&NdasInfo->SlotNo,
					sizeof(NdasInfo->SlotNo)
				);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_SLOTNO));
		return	status;
	}
	//	MaxBlocksPerRequest
	status = WriteKeyValue(
					SubKey,
					LSBUS_NDASREG_DEVMAXBPR,
					REG_DWORD,
					&NdasInfo->MaxRequestBlocks,
					sizeof(NdasInfo->MaxRequestBlocks)
				);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_DEVMAXBPR));
		return	status;
	}

	//	PlugInTime
	status = WriteKeyValue(
					SubKey,
					LSBUS_NDASREG_PLUGINTIME,
					REG_DWORD,
					&NdasInfo->PluginTime,
					sizeof(NdasInfo->PluginTime)
				);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_PLUGINTIME));
		return	status;
	}

	//	Flags
	status = WriteKeyValue(
					SubKey,
					LSBUS_NDASREG_FLAGS,
					REG_DWORD,
					&NdasInfo->Flags,
					sizeof(NdasInfo->Flags)
				);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_FLAGS));
		return	status;
	}

	//	HardwareIDs
	status = WriteKeyValue(
					SubKey,
					LSBUS_NDASREG_HWIDS,
					REG_DWORD,
					&NdasInfo->HardwareIDs,
					sizeof(NdasInfo->HardwareIDs)
			);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("WriteKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_HWIDS));
		return	status;
	}

	status = WriteLurDescToRegistry(SubKey, &NdasInfo->LurDesc);
	if(!NT_SUCCESS(status)) {
		ZwClose(SubKey);
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("BuildLurDescByRegistry() failed. NTSTATUS:%08lx\n", status));
	}

cleanup:
	if(nameBuffer)
		ExFreePool(nameBuffer);
	return status;
}


//
//	Look up the registry to find a specific NDAS device registry.
//
NTSTATUS
Reg_LookupRegDeviceWithSlotNo(
		HANDLE	NdasDeviceReg,
		ULONG	SlotNo,
		HANDLE	*OneDevice
		) {
	NTSTATUS					status;
	PKEY_BASIC_INFORMATION		keyInfo;
	ULONG						outLength;
	LONG						idxKey;
	OBJECT_ATTRIBUTES			objectAttributes;
	UNICODE_STRING				objectName;
	HANDLE						SubKey;
	ULONG						curSlotNo;

	status = STATUS_SUCCESS;

	keyInfo = (PKEY_BASIC_INFORMATION)ExAllocatePoolWithTag(PagedPool, 512, LSBUS_POOTAG_KEYINFO);
	if(!keyInfo) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ExAllocatePoolWithTag(KEY_BASIC_INFORMATION) failed.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	for(idxKey = 0 ; idxKey < MAX_DEVICES_IN_NDAS_REGISTRY; idxKey ++) {
		status = ZwEnumerateKey(
						NdasDeviceReg,
						idxKey,
						KeyBasicInformation,
						keyInfo,
						512,
						&outLength
						);

		if(status == STATUS_NO_MORE_ENTRIES) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("No more entries.\n"));
			break;
		}
		if(!NT_SUCCESS(status)) {
			ASSERT(status != STATUS_BUFFER_OVERFLOW && status != STATUS_BUFFER_TOO_SMALL);
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ZwEnumerateKey() failed. NTSTATUS:%08lx\n", status));
			break;
		}

		//
		//	Name verification
		//
		//	TODO
		//

		//
		//	Open a sub key (NdasDevices\Devxx) and plug in device with the registry key.
		//
		objectName.Length = objectName.MaximumLength = (USHORT)keyInfo->NameLength;
		objectName.Buffer = keyInfo->Name;
		InitializeObjectAttributes(		&objectAttributes,
										&objectName,
										OBJ_KERNEL_HANDLE,
										NdasDeviceReg,
										NULL
								);
		status = ZwOpenKey(&SubKey, KEY_ALL_ACCESS, &objectAttributes);
		if(!NT_SUCCESS(status)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ZwOpenKey() failed. NTSTATUS:%08lx\n", status));
			continue;
		}

		//	Slot number
		status = ReadKeyValue(
						SubKey,
						LSBUS_NDASREG_SLOTNO,
						REG_DWORD,
						&curSlotNo,
						sizeof(curSlotNo),
						NULL
					);
		if(!NT_SUCCESS(status)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadKeyValue() failed. ValueKeyName:%ws\n", LSBUS_NDASREG_SLOTNO));
			goto cleanup;
		}
		if(SlotNo == curSlotNo) {
			Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("Found a device:%d\n", SlotNo));
			status = STATUS_SUCCESS;
			*OneDevice = SubKey;
			break;
		}

		ZwClose(SubKey);

	}
	if(idxKey >= MAX_DEVICES_IN_NDAS_REGISTRY) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Could not found a device:%d\n", SlotNo));
			status = STATUS_OBJECT_PATH_NOT_FOUND;
	}

cleanup:
	if(keyInfo)
		ExFreePool(keyInfo);
	return status;
}


//////////////////////////////////////////////////////////////////////////
//
//	Exported functions to IOCTL.
//

//
//	Register a device by writing registry.
//
NTSTATUS
LSBus_RegisterDevice(
		PFDO_DEVICE_DATA				FdoData,
		PLANSCSI_REGISTER_NDASDEV		NdasInfo
) {
	NTSTATUS			status;
	HANDLE				DeviceReg;
	HANDLE				NdasDeviceReg;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	DeviceReg = NULL;
	NdasDeviceReg = NULL;
	status = Reg_OpenDeviceRegistry(FdoData->UnderlyingPDO, &DeviceReg, KEY_READ|KEY_WRITE);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("OpenServiceRegistry() failed.\n"));
		return status;
	}
	status = Reg_OpenNdasDeviceRegistry(&NdasDeviceReg, KEY_READ|KEY_WRITE, DeviceReg);
	if(!NT_SUCCESS(status)) {
		ZwClose(DeviceReg);
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("OpenNdasDeviceRegistry() failed.\n"));
		return status;
	}

	status = WriteDevInfoToRegistry(NdasDeviceReg, NdasInfo);

	if(NdasDeviceReg)
		ZwClose(NdasDeviceReg);
	if(DeviceReg)
		ZwClose(DeviceReg);

	return status;
}


NTSTATUS
LSBus_UnregisterDevice(
		PFDO_DEVICE_DATA				FdoData,
		PLANSCSI_UNREGISTER_NDASDEV		NdasInfo
) {
	NTSTATUS			status;
	HANDLE				DeviceReg;
	HANDLE				NdasDeviceReg;
	HANDLE				DeviceTobeDeleted;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	status = STATUS_SUCCESS;
	DeviceReg = NULL;
	NdasDeviceReg = NULL;
	status = Reg_OpenDeviceRegistry(FdoData->UnderlyingPDO, &DeviceReg, KEY_READ|KEY_WRITE);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("OpenServiceRegistry() failed.\n"));
		return status;
	}
	status = Reg_OpenNdasDeviceRegistry(&NdasDeviceReg, KEY_READ|KEY_WRITE, DeviceReg);
	if(!NT_SUCCESS(status)) {
		ZwClose(DeviceReg);
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("OpenNdasDeviceRegistry() failed.\n"));
		return status;
	}

	status = Reg_LookupRegDeviceWithSlotNo(NdasDeviceReg, NdasInfo->SlotNo, &DeviceTobeDeleted);
	if(NT_SUCCESS(status)) {
		status = ZwDeleteKey(DeviceTobeDeleted);
		ZwClose(DeviceTobeDeleted);
#if DBG
		if(NT_SUCCESS(status)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("A device(Slot %d) is deleted.\n", NdasInfo->SlotNo));
		} else {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ZwDeleteKey() failed. NTSTATUS:%08lx\n", status));
		}
#endif
	}

	if(NdasDeviceReg)
		ZwClose(NdasDeviceReg);
	if(DeviceReg)
		ZwClose(DeviceReg);

	return status;
}
