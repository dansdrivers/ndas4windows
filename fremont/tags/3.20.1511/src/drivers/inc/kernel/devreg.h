#ifndef __NDASSCSI_DEVICE_REGISTRY_H__
#define __NDASSCSI_DEVICE_REGISTRY_H__

#define DEVREG_MAX_REGISTRY_KEY		((ULONG)(-1))

#ifndef NTDDI_VERSION
#if WINVER < 0x0501

#define DEVICE_REGISTRY_PROPERTY	DEVICE_REGISTRY_PROPERTY_EXT2K

//
// Define PnP install-state for DrGetDeviceInstallState
//
//	Original copy: wxp\ntddk.h
//
#undef DevicePropertyDeviceDescription

typedef enum {
//	DevicePropertyDeviceDescription,			// 0
//	DevicePropertyHardwareID,
//	DevicePropertyCompatibleIDs,
//	DevicePropertyBootConfiguration,
//	DevicePropertyBootConfigurationTranslated,
//	DevicePropertyClassName,					// 5
//	DevicePropertyClassGuid,
//	DevicePropertyDriverKeyName,
//	DevicePropertyManufacturer,
//	DevicePropertyFriendlyName,
//	DevicePropertyLocationInformation,			// 10
//	DevicePropertyPhysicalDeviceObjectName,
//	DevicePropertyBusTypeGuid,
//	DevicePropertyLegacyBusType,
//	DevicePropertyBusNumber,
//	DevicePropertyEnumeratorName,				// 15
//	DevicePropertyAddress,
//	DevicePropertyUINumber,						// 17
	//
	//	Following values added since Window XP
	//
	DevicePropertyInstallState = 18,
	DevicePropertyRemovalPolicy
} DEVICE_REGISTRY_PROPERTY_EXT2K;


typedef enum _DEVICE_INSTALL_STATE {

	InstallStateInstalled,
	InstallStateNeedsReinstall,
	InstallStateFailedInstall,
	InstallStateFinishInstall

} DEVICE_INSTALL_STATE, *PDEVICE_INSTALL_STATE;

#endif // WINVER < 0x0501
#endif // IFNDEF NTDDI_VERSION

NTSTATUS
DrGetDeviceProperty(
	IN PDEVICE_OBJECT  DeviceObject,
    IN DEVICE_REGISTRY_PROPERTY  DeviceProperty,
    IN ULONG	BufferLength,
    OUT PVOID	PropertyBuffer,
    OUT PULONG  ResultLength,
	IN BOOLEAN	Win2K
);

NTSTATUS
DrReadKeyValue(
		HANDLE	RegKey,
		PWCHAR	KeyName,
		ULONG	KeyType,
		PVOID	Buffer,
		ULONG	BufferLength,
		PULONG	BufferLengthNeeded
);

NTSTATUS
DrWriteKeyValue(
		HANDLE	RegKey,
		PWCHAR	KeyName,
		ULONG	KeyType,
		PVOID	Buffer,
		ULONG	BufferLength
);

NTSTATUS
DrReadKeyValueInstantly(
		PUNICODE_STRING RegistryPath,
		PWCHAR	ValueKeyName,
		ULONG	ValueKeyType,
		PVOID	Buffer,
		ULONG	BufferLength,
		PULONG	BufferLengthNeeded
);

NTSTATUS
DrReadDevKeyValueInstantly(
		PDEVICE_OBJECT	PhyDeviceObject,
		PWCHAR	ValueKeyName,
		ULONG	ValueKeyType,
		PVOID	Buffer,
		ULONG	BufferLength,
		PULONG	BufferLengthNeeded
);

NTSTATUS
DrOpenDeviceRegistry(
		PDEVICE_OBJECT	DeviceObject,
		HANDLE			*DeviceParamReg,
		ACCESS_MASK		AccessMask
);

NTSTATUS
DrDeleteAllKeys(
		HANDLE			ParentKey
);

NTSTATUS
DrDeleteAllSubKeys(
		HANDLE				ParentKey
);

NTSTATUS
DrDriverServiceExist(
	PWSTR  DriverServiceName
);

#endif