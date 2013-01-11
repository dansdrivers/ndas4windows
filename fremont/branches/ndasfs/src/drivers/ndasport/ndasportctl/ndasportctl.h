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

HRESULT
WINAPI
NdasPortCtlCreateControlDevice(
	__in DWORD DesiredAccess,
	__deref_out HANDLE* DeviceHandle);

HRESULT
WINAPI
NdasPortCtlGetPortNumber(
	__in HANDLE NdasPortHandle,
	__in PUCHAR PortNumber);

HRESULT
WINAPI
NdasPortCtlPlugInLogicalUnit(
	__in HANDLE NdasPortHandle,
	__in PNDAS_LOGICALUNIT_DESCRIPTOR Descriptor);

HRESULT
WINAPI
NdasPortCtlEjectLogicalUnit(
	__in HANDLE NdasPortHandle,
	__in NDAS_LOGICALUNIT_ADDRESS Address,
	__in ULONG Flags);

HRESULT
WINAPI
NdasPortCtlUnplugLogicalUnit(
	__in HANDLE NdasPortHandle,
	__in NDAS_LOGICALUNIT_ADDRESS Address,
	__in ULONG Flags);

HRESULT
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
HRESULT
WINAPI
NdasPortCtlBuildNdasAtaDeviceDescriptor(
	__in NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress,
	__in ULONG LogicalUnitFlags,
	__in CONST NDAS_DEVICE_IDENTIFIER* DeviceIdentifier,
	__in ULONG NdasDeviceFlagMask,
	__in ULONG NdasDeviceFlags,
	__in ACCESS_MASK AccessMode,
	__in ULONG VirtualBytesPerBlock,
	__in PLARGE_INTEGER VirtualLogicalBlockAddress,
	__deref_out PNDAS_LOGICALUNIT_DESCRIPTOR* LogicalUnitDescriptor);

/*++

	Set the NDAS disk encryption information to the existing descriptor.
	New descriptor may be reallocated with HeapReAlloc(GetProcessHeap(),...)
	If the function succeeded, LogicalUnitDescriptor can be reallocated.
	Otherwise, LogicalUnitDescriptor remains in tact.
	Either case, it is the caller's responsibility to free the buffer with
	HeapFree(GetProcessHeap(), ...).

--*/

HRESULT
WINAPI
NdasPortCtlSetNdasDiskEncryption(
	__deref_inout PNDAS_LOGICALUNIT_DESCRIPTOR* LogicalUnitDescriptor,
	__in CONST NDAS_DISK_ENCRYPTION_DESCRIPTOR* EncryptionDescriptor);

/*++

Set the device user ID / password other than the default.

--*/

HRESULT
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

HRESULT
WINAPI
NdasPortCtlSetNdasDeviceOemCode(
	__inout PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in ULONG TargetNodeIndex,
	__in_bcount(DeviceOemCodeLength) CONST BYTE* DeviceOemCode,
	__in ULONG DeviceOemCodeLength);

/*++

Get NDAS block access control list from the LUR.

++*/

HRESULT
WINAPI
NdasPortCtlGetNdasBacl(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__out PNDAS_BLOCK_ACL* Bacl);

/*++
	NDAS DLU initialization data structures
++*/

#define NDASPORTCTL_USERPASSWORD_LENGTH	8
#define NDASPORTCTL_OEMCODE_LENGTH	8

#if 0
typedef struct _NDASPORTCTL_INIT_RAID {
	UINT32 	SectorsPerBit;
	UINT32 	SpareDisks;
	GUID	RaidSetId;
	GUID	ConfigSetId;
} NDASPORTCTL_INIT_RAID, *PNDASPORTCTL_INIT_RAID;
#else
typedef NDAS_RAID_INFO	NDASPORTCTL_INIT_RAID;
typedef PNDAS_RAID_INFO PNDASPORTCTL_INIT_RAID;
#endif

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
	__in DWORD						LogicalUnitFlags,
	__in NDAS_DEV_ACCESSMODE		NdasDevAccessMode,
	__in WORD						LurDeviceType,
	__in PLARGE_INTEGER				LurVirtualLogicalBlockAddress,
	__in DWORD						LeafCount,
	__in WORD						LurNameLength,
	__in DWORD						NdasBaclLength,
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

HRESULT 
WINAPI
NdasPortCtlQueryNodeAlive(
	__in NDAS_LOGICALUNIT_ADDRESS NdasLogicalUnitAddress);

HRESULT 
WINAPI
NdasPortCtlStartStopRegistrarEnum(
	__in BOOL bOnOff,
	__out LPBOOL pbPrevState);

HRESULT
WINAPI
NdasPortCtlQueryInformation(
	__in HANDLE DiskHandle,
	__in PNDASSCSI_QUERY_INFO_DATA Query,
	__in DWORD QueryLength,
	__out PVOID Information,
	__in DWORD InformationLength);

HRESULT
WINAPI
NdasPortCtlQueryDeviceMode(
	__in HANDLE DiskHandle,
	__in NDAS_LOGICALUNIT_ADDRESS NdasLogicalUnitAddress,
	__out PULONG DeviceMode,
	__out PULONG SupportedFeatures,
	__out PULONG EnabledFeatures,
	__out PULONG ConnectionCount);

HRESULT
WINAPI
NdasPortCtlQueryLurFullInformation(
	__in HANDLE DiskHandle,
	__in NDAS_LOGICALUNIT_ADDRESS NdasLogicalUnitAddress,
	__deref_out PNDSCIOCTL_LURINFO * LurFullInfo);

typedef DWORD NDAS_LOCATION;

FORCEINLINE
NDAS_LOGICALUNIT_ADDRESS
NdasLocationToLogicalUnitAddress(
	__in NDAS_LOCATION NdasLocation)
{
	NDAS_LOGICALUNIT_ADDRESS logicalUnitAddress;
	logicalUnitAddress.Address = NdasLocation;
	logicalUnitAddress.PortNumber = 0;
	return logicalUnitAddress;
}

FORCEINLINE
NDAS_LOCATION
NdasLogicalUnitAddressToLocation(
	__in NDAS_LOGICALUNIT_ADDRESS NdasLogicalUnitAddress)
{
	NdasLogicalUnitAddress.PortNumber = 0;
	NDAS_LOCATION ndasLocation = NdasLogicalUnitAddress.Address;
	return ndasLocation;
}


#ifdef __cplusplus
}
#endif
