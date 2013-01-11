#pragma once
#include <windows.h>
#include <ndas/ndasportioctl.h>
#include <ndas/ndasdiskioctl.h>
#include <ndas/ndasdluioctl.h>
#include <ndas/filediskioctl.h>
#include <ndas/ramdiskioctl.h>

#ifdef __cplusplus
extern "C" {
#endif

HANDLE
WINAPI
NdasPortCtlCreateControlDevice(
	DWORD DesiredAccess);

BOOL
WINAPI
NdasPortCtlGetPortNumber(
	__in HANDLE NdasPortHandle,
	__in PUCHAR PortNumber);

BOOL
WINAPI
NdasPortCtlPlugInLogicalUnit(
	__in HANDLE NdasPortHandle,
	__in PNDAS_LOGICALUNIT_DESCRIPTOR Descriptor);

BOOL
WINAPI
NdasPortCtlEjectLogicalUnit(
	__in HANDLE NdasPortHandle,
	__in NDAS_LOGICALUNIT_ADDRESS Address,
	__in ULONG Flags);

BOOL
WINAPI
NdasPortCtlUnplugLogicalUnit(
	__in HANDLE NdasPortHandle,
	__in NDAS_LOGICALUNIT_ADDRESS Address,
	__in ULONG Flags);

BOOL
WINAPI
NdasPortCtlIsLogicalUnitAddressInUse(
	__in HANDLE NdasPortHandle,
	__in NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress);

/*++

	Build the Logical Unit Descriptor and returned the pointer.
	Returns NULL if failed.
	Caller is responsible for freeing the buffer after use
	with HeapFree(GetProcessHeap(), ...)

--*/

PNDAS_LOGICALUNIT_DESCRIPTOR
WINAPI
NdasPortCtlBuildNdasAtaDeviceDescriptor(
	__in NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress,
	__in ULONG LogicalUnitFlags,
	__in CONST NDAS_DEVICE_IDENTIFIER* DeviceIdentifier,
	__in ULONG NdasDeviceFlagMask,
	__in ULONG NdasDeviceFlags,
	__in ACCESS_MASK AccessMode,
	__in ULONG VirtualBytesPerBlock,
	__in PLARGE_INTEGER VirtualLogicalBlockAddress);

/*++

	Set the NDAS disk encryption information to the existing descriptor.
	New descriptor may be reallocated with HeapReAlloc(GetProcessHeap(),...)
	Returns NULL if failed.
	Caller is responsible for freeing the buffer after use
	with HeapFree(GetProcessHeap(), ...)

--*/

PNDAS_LOGICALUNIT_DESCRIPTOR
WINAPI
NdasPortCtlSetNdasDiskEncryption(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in CONST NDAS_DISK_ENCRYPTION_DESCRIPTOR* EncryptionDescriptor);

/*++

Set the device user ID / password other than the default.

--*/

BOOL
WINAPI
NdasPortCtlSetNdasDeviceUserIdPassword(
	__inout PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in ULONG TargetNodeIndex,
	__in ULONG UserId,
	__in_bcount(UserPasswordLength) CONST BYTE* UserPassword,
	__in ULONG UserPasswordLength);

/*++

	Set the device OEM code other than the default.
	DeviceOemCodeLength must be 8

--*/

BOOL
WINAPI
NdasPortCtlSetNdasDeviceOemCode(
	__inout PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in ULONG TargetNodeIndex,
	__in_bcount(DeviceOemCodeLength) CONST BYTE* DeviceOemCode,
	__in ULONG DeviceOemCodeLength);

/*++

Get NDAS block access control list from the LUR.

++*/

PNDAS_BLOCK_ACL
WINAPI
NdasPortCtlGetNdasBacl(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR	LogicalUnitDescriptor
);

/*++
	NDAS DLU initialization data structures
++*/

#define NDASPORTCTL_USERPASSWORD_LENGTH	8
#define NDASPORTCTL_OEMCODE_LENGTH	8

typedef struct _NDASPORTCTL_INIT_RAID {
	UINT32 	SectorsPerBit;
	UINT32 	SpareDisks;
	GUID	RaidSetId;
	GUID	ConfigSetId;
} NDASPORTCTL_INIT_RAID, *PNDASPORTCTL_INIT_RAID;

#define NDASPORTCTL_ATAINIT_VALID_TRANSPORT_PORTNO	0x00000001
#define NDASPORTCTL_ATAINIT_VALID_BINDING_ADDRESS	0x00000002
#define NDASPORTCTL_ATAINIT_VALID_USERID			0x00000004
#define NDASPORTCTL_ATAINIT_VALID_USERPASSWORD		0x00000008
#define NDASPORTCTL_ATAINIT_VALID_OEMCODE			0x00000010

typedef struct _NDASPORTCTL_INIT_ATADEV {
	NDAS_DEVICE_IDENTIFIER DeviceIdentifier;
	UCHAR				HardwareType;
	UCHAR				HardwareVersion;
	UCHAR				HardwareRevision;
	UCHAR				Reserved;

	// Indicate validation of the following field.
	UINT32				ValidFieldMask;
	ULONG				TransportPortNumber;
	TA_LSTRANS_ADDRESS	BindingAddress;
	UINT32				UserId;
	BYTE				UserPassword[NDASPORTCTL_USERPASSWORD_LENGTH];
	BYTE				DeviceOemCode[NDASPORTCTL_OEMCODE_LENGTH];
} NDASPORTCTL_INIT_ATADEV, *PNDASPORTCTL_INIT_ATADEV;

typedef	struct _NDASPORTCTL_NODE_INITDATA {
	LURN_TYPE		NodeType;
	LARGE_INTEGER	StartLogicalBlockAddress;
	LARGE_INTEGER	EndLogicalBlockAddress;
	union {
		NDASPORTCTL_INIT_RAID Raid;
		NDASPORTCTL_INIT_ATADEV Ata;
	} NodeSpecificData;
} NDASPORTCTL_NODE_INITDATA, *PNDASPORTCTL_NODE_INITDATA;


/*++

Build the Down level Logical Unit Descriptor and returned the pointer.
Returns NULL if failed.
Caller is responsible for freeing the buffer after use
with HeapFree(GetProcessHeap(), ...)

--*/

PNDAS_LOGICALUNIT_DESCRIPTOR
WINAPI
NdasPortCtlBuildNdasDluDeviceDescriptor(
	__in NDAS_LOGICALUNIT_ADDRESS	LogicalUnitAddress,
	__in ULONG						LogicalUnitFlags,
	__in NDAS_DEV_ACCESSMODE		NdasDevAccessMode,
	__in USHORT						LurDeviceType,
	__in PLARGE_INTEGER				LurVirtualLogicalBlockAddress,
	__in ULONG						LeafCount,
	__in ULONG						NdasBaclLength,
	__in PNDASPORTCTL_NODE_INITDATA	RootNodeInitData
);

/*++

Find a node from the LUR relation with an node index.

++*/
PLURELATION_NODE_DESC
WINAPI
NdasPortCtlFindNodeDesc(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in ULONG TargetNodeIndex
);

/*++

Set up a LUR relation node.

++*/
BOOL
WINAPI
NdasPortCtlSetupLurNode(
	__inout PLURELATION_NODE_DESC			LurNodeDesc,
	__in NDAS_DEV_ACCESSMODE				NdasDevAccessMode,
	__in PNDASPORTCTL_NODE_INITDATA			NodeInitData
);

/*++

Calculate RAID ending address according the RAID type.

++*/
BOOL
WINAPI
NdasPortCtlGetRaidEndAddress(
	__in LURN_TYPE	LurnType,
	__in  LONGLONG	InputEndAddress,
	__in ULONG		MemberCount,
	__out PLONGLONG	RaidEndAddress
);

/*++

Convert an NDAS device identifier to an LPX address in form of TA LS transport address

++*/

VOID
WINAPI
NdasPortCtlConvertDeviceIndentiferToLpxTaLsTransAddress(
	__in PNDAS_DEVICE_IDENTIFIER	NdasDeviceIndentifier,
	__in USHORT						LpxPortNumber,
	__out PTA_LSTRANS_ADDRESS		TaLsTransAddress
);

//////////////////////////////////////////////////////////////////////////
//
// Legacy NDASBus interface support.
//
//////////////////////////////////////////////////////////////////////////

BOOL 
WINAPI
NdasPortCtlQueryNodeAlive(
	__in NDAS_LOGICALUNIT_ADDRESS NdasLogicalUnitAddress,
	__out LPBOOL pbAlive,
	__out LPBOOL pbAdapterHasError
);

BOOL 
WINAPI
NdasPortCtlStartStopRegistrarEnum(
	__in BOOL		bOnOff,
	__out LPBOOL	pbPrevState
);

BOOL
WINAPI
NdasPortCtlQueryEvent(
	__in HANDLE						DiskHandle,
	__in NDAS_LOGICALUNIT_ADDRESS	NdasLogicalUnitAddress,
	__out PULONG					pStatus
);

BOOL
WINAPI
NdasPortCtlQueryInformation(
	__in HANDLE		DiskHandle,
	__in PNDASSCSI_QUERY_INFO_DATA Query,
	__in DWORD QueryLength,
	__out PVOID Information,
	__in DWORD InformationLength
);

BOOL 
WINAPI
NdasPortCtlQueryPdoEventPointers(
	__in HANDLE						DiskHandle,
	__in NDAS_LOGICALUNIT_ADDRESS	NdasLogicalUnitAddress,
	__out PHANDLE					AlarmEvent,
	__out PHANDLE					DisconnectEvent
);

BOOL
WINAPI
NdasPortCtlQueryDeviceMode(
	__in HANDLE						DiskHandle,
	__in NDAS_LOGICALUNIT_ADDRESS	NdasLogicalUnitAddress,
	__out PULONG					DeviceMode,
	__out PULONG					SupportedFeatures,
	__out PULONG					EnabledFeatures
);

inline
VOID
NdasPortCtlConvertNdasLocationToNdasLUAddr(
	ULONG NdasLocation, PNDAS_LOGICALUNIT_ADDRESS NdasLogicalUnitAddress
){
	NdasLogicalUnitAddress->Address = 0;
	NdasLogicalUnitAddress->TargetId = (UCHAR)((NdasLocation >> 8) & 0xff);
	NdasLogicalUnitAddress->Lun = (UCHAR)(NdasLocation & 0xff);
}

inline
VOID
NdasPortCtlConvertNdasLUAddrToNdasLocation(
	NDAS_LOGICALUNIT_ADDRESS NdasLogicalUnitAddress, PULONG NdasLocation
){
	*NdasLocation = ((ULONG)NdasLogicalUnitAddress.TargetId << 8) |
					NdasLogicalUnitAddress.Lun;
}


#ifdef __cplusplus
}
#endif
