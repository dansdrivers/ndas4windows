#include <ntddk.h>
#include <windef.h>
#include <regstr.h>
#include "KDebug.h"
#include "LSKLib.h"
#include "devreg.h"

//
//	Query the
//
NTKERNELAPI                                                     
NTSTATUS                                                        
ObQueryNameString(                                              
    IN PVOID Object,                                            
    OUT POBJECT_NAME_INFORMATION ObjectNameInfo,                
    IN ULONG Length,                                            
    OUT PULONG ReturnLength                                     
    );                                                          


//
//	Determine DevicePropertyInstallState
//		by directly reading registry key REGSTR_VAL_CONFIGFLAGS('ConfigFlags')
//	NOTE: This is not recommended by Microsoft.
//
#define KVLEN_PARTIAL	(sizeof(KEY_VALUE_PARTIAL_INFORMATION) - sizeof(UCHAR) + sizeof(ULONG))

NTSTATUS
DrGetDevicePropertyInstallState(
	IN PDEVICE_OBJECT  DeviceObject,
    IN ULONG  BufferLength,
    OUT PVOID  PropertyBuffer,
    OUT PULONG  ResultLength
) {
	NTSTATUS		status;
	HANDLE			devparamRegKey;
	HANDLE			devInstRegKey;
	UNICODE_STRING valueName;
	UCHAR			keyValueBuffer[KVLEN_PARTIAL];
	PKEY_VALUE_PARTIAL_INFORMATION  keyValue;
	ULONG			resultLength;
	PULONG			configFlags;
	DEVICE_INSTALL_STATE deviceInstallState;
	PVOID			Object;
	UCHAR					ObjectNameBuffer[512];
	POBJECT_NAME_INFORMATION ObjectNameInfo;
	OBJECT_ATTRIBUTES		objectAttributes;


	//
	//	init
	//
	devInstRegKey = NULL;
	devparamRegKey = NULL;
	keyValue = (PKEY_VALUE_PARTIAL_INFORMATION)keyValueBuffer;
	if(BufferLength < sizeof(DEVICE_INSTALL_STATE)) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	//	open the device registry's parameter key
	//
	status = IoOpenDeviceRegistryKey(
					DeviceObject,
					PLUGPLAY_REGKEY_DEVICE,
					KEY_QUERY_VALUE|KEY_ENUMERATE_SUB_KEYS,
					&devparamRegKey
				);
	if(!NT_SUCCESS(status)) {
		devInstRegKey = NULL;
		goto cleanup;
	}
	status = ObReferenceObjectByHandle(
							devparamRegKey,
							GENERIC_READ,
							NULL,
							KernelMode,
							&Object,
							NULL
					);
	if(!NT_SUCCESS(status)) {
		goto cleanup;
	}

	ObjectNameInfo = (POBJECT_NAME_INFORMATION)ObjectNameBuffer;
	status = ObQueryNameString(Object, ObjectNameInfo, sizeof(ObjectNameBuffer), &resultLength );
	ObDereferenceObject(Object);
	if(!NT_SUCCESS(status) || ObjectNameInfo->Name.Buffer == NULL) {
		goto cleanup;
	}
	KDPrint(1, ("Device parameter path: %ws. len=%d\n", ObjectNameInfo->Name.Buffer,ObjectNameInfo->Name.Length));

	ASSERT(ObjectNameInfo->Name.Length < sizeof(ObjectNameBuffer) - 20);
	// trim down to the first parent.
	for(; ObjectNameInfo->Name.Length > 1; ObjectNameInfo->Name.Length-=sizeof(WCHAR) ) {
		if( ObjectNameInfo->Name.Buffer[ObjectNameInfo->Name.Length/sizeof(WCHAR) - 1] == '\\') {
			ObjectNameInfo->Name.Length-=sizeof(WCHAR);
			KDPrint(1, ("Trimed at the index %d\n", ObjectNameInfo->Name.Length));
			break;
		}
	}

	KDPrint(1, ("Device parameter path: %ws. len=%d\n", ObjectNameInfo->Name.Buffer,ObjectNameInfo->Name.Length));
	if(ObjectNameInfo->Name.Length <= 1) {
		goto cleanup;
	}

	//
	//	Open device registry.
	//
	InitializeObjectAttributes(
					&objectAttributes,
					&ObjectNameInfo->Name,
					OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE,
					NULL,
					NULL
				);
	status = ZwOpenKey(&devInstRegKey, KEY_QUERY_VALUE|KEY_ENUMERATE_SUB_KEYS, &objectAttributes);
	if(!NT_SUCCESS(status)) {
		goto cleanup;
	}

	//
	//	Query the value of CONFIGFLAGS
	//
	RtlInitUnicodeString(&valueName, REGSTR_VAL_CONFIGFLAGS);
	status = ZwQueryValueKey(
			devInstRegKey,
			&valueName,
			KeyValuePartialInformation,
			keyValue,
			KVLEN_PARTIAL,
			&resultLength
		);
	if(!NT_SUCCESS(status)) {
		goto cleanup;
	}
	if(keyValue->Type != REG_DWORD) {
		goto cleanup;
	}
	if(keyValue->DataLength != sizeof(ULONG)) {
		goto cleanup;
	}

	//
	//	determine deviceInstallState
	//
	// TODO: If the device object is root device,
	//			always return InstallStateInstalled.
	//
	configFlags = (PULONG)keyValue->Data;
	if ((*configFlags) & CONFIGFLAG_REINSTALL) {

		deviceInstallState = InstallStateNeedsReinstall;

	} else if ((*configFlags) & CONFIGFLAG_FAILEDINSTALL) {

		deviceInstallState = InstallStateFailedInstall;

	} else if ((*configFlags) & CONFIGFLAG_FINISH_INSTALL) {

		deviceInstallState = InstallStateFinishInstall;
	} else {

		deviceInstallState = InstallStateInstalled;
	}

	//
	//	set the result
	//
	(*(PDEVICE_INSTALL_STATE)PropertyBuffer) = deviceInstallState;
	*ResultLength = sizeof(DEVICE_INSTALL_STATE);

cleanup:
	if(devInstRegKey)
		ZwClose(devInstRegKey);
	if(devparamRegKey)
		ZwClose(devparamRegKey);

	return status;
}

//
//	Process DevicePropertyInstallState and DevicePropertyRemovalPolicy on Windows 2000
//
NTSTATUS
DrGetDeviceProperty_ExtW2K(
	IN PDEVICE_OBJECT  DeviceObject,
    IN DEVICE_REGISTRY_PROPERTY  DeviceProperty,
    IN ULONG  BufferLength,
    OUT PVOID  PropertyBuffer,
    OUT PULONG  ResultLength
) {
	NTSTATUS status;

	switch(DevicePropertyInstallState) {
		case DevicePropertyInstallState:
			status = DrGetDevicePropertyInstallState(
							DeviceObject,
							BufferLength,
							PropertyBuffer,
							ResultLength
						);
			break;
		default:
			status = IoGetDeviceProperty(
							DeviceObject,
							DeviceProperty,
							BufferLength,
							PropertyBuffer,
							ResultLength
						);
	}

	return status;
}

//
//	Complement Windows 2000's IoGetDeviceProperty()
//
NTSTATUS
DrGetDeviceProperty(
	IN PDEVICE_OBJECT  DeviceObject,
    IN DEVICE_REGISTRY_PROPERTY  DeviceProperty,
    IN ULONG	BufferLength,
    OUT PVOID	PropertyBuffer,
    OUT PULONG  ResultLength,
	IN BOOLEAN	Win2K
) {
	NTSTATUS	status;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	if(!DeviceObject)
		return STATUS_INVALID_PARAMETER;
	if(Win2K) {
		status = DrGetDeviceProperty_ExtW2K(
							DeviceObject,
							DeviceProperty,
							BufferLength,
							PropertyBuffer,
							ResultLength
			);
	} else {
		status = IoGetDeviceProperty(
							DeviceObject,
							DeviceProperty,
							BufferLength,
							PropertyBuffer,
							ResultLength
						);
	}


	return status;
}