#ifndef __NDASSCSI_DEVICE_REGISTRY_H__
#define __NDASSCSI_DEVICE_REGISTRY_H__

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

#endif


NTSTATUS
DrGetDeviceProperty(
	IN PDEVICE_OBJECT  DeviceObject,
    IN DEVICE_REGISTRY_PROPERTY  DeviceProperty,
    IN ULONG	BufferLength,
    OUT PVOID	PropertyBuffer,
    OUT PULONG  ResultLength,
	IN BOOLEAN	Win2K
);

#endif