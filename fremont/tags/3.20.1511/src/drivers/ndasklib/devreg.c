#include <ntddk.h>
#include <windef.h>
#include <regstr.h>
#include "KDebug.h"
#include "LSKLib.h"
#include "devreg.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "DevReg"


#define	DEVREG_POOTAG_KEYINFO		'ikRD'


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
	UCHAR					ObjectNameBuffer[512] = {0};
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

	switch(DeviceProperty) {
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

		try {

			status = IoGetDeviceProperty(
							DeviceObject,
							DeviceProperty,
							BufferLength,
							PropertyBuffer,
							ResultLength
						);

		}  except (EXCEPTION_EXECUTE_HANDLER) {

			status = GetExceptionCode();
			KDPrint(1, ("IoGetDeviceProperty() Exception: %08lx\n", status));

		}

	}


	return status;
}


//////////////////////////////////////////////////////////////////////////
//
//	Registry primitives.
//
NTSTATUS
DrReadKeyValue(
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
	keyValue = (PKEY_VALUE_PARTIAL_INFORMATION)ExAllocatePoolWithTag(PagedPool, 512, DEVREG_POOTAG_KEYINFO);
	if(keyValue == NULL)
		return STATUS_INSUFFICIENT_RESOURCES;
	RtlInitUnicodeString(&valueName, KeyName);
	status = ZwQueryValueKey(RegKey, &valueName, KeyValuePartialInformation, keyValue, 512, &outLength);
	if(BufferLengthNeeded) {
		*BufferLengthNeeded = keyValue->DataLength;
	}
	if(!NT_SUCCESS(status)) {
		KDPrint(1, ("ZwQueryValueKey() failed. ValueKeyName:%ws\n", KeyName));
		ExFreePool(keyValue);
		return	status;
	}
	if(keyValue->Type != KeyType) {
		KDPrint(1, ("Wrong value key type. ValueKeyName:%ws\n", KeyName));
		ExFreePool(keyValue);
		return	status;
	}
	if(BufferLength < keyValue->DataLength) {
		KDPrint(1, ("Buffer too small. ValueKeyName:%ws\n", KeyName));
		ExFreePool(keyValue);
		return STATUS_BUFFER_TOO_SMALL;
	}

	RtlCopyMemory(Buffer, keyValue->Data, keyValue->DataLength);

	ExFreePool(keyValue);
	return STATUS_SUCCESS;
}

NTSTATUS
DrWriteKeyValue(
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


//////////////////////////////////////////////////////////////////////////
//
//	Instantly read/write a key
//

NTSTATUS
DrReadKeyValueInstantly(
		PUNICODE_STRING RegistryPath,
		PWCHAR	ValueKeyName,
		ULONG	ValueKeyType,
		PVOID	Buffer,
		ULONG	BufferLength,
		PULONG	BufferLengthNeeded
){
	NTSTATUS	status;
	HANDLE		registryKey;
	OBJECT_ATTRIBUTES	objectAttributes;

	InitializeObjectAttributes( &objectAttributes,
								RegistryPath,
								OBJ_KERNEL_HANDLE,
								NULL,
								NULL
							);
	status = ZwCreateKey(
						&registryKey,
						KEY_READ,
						&objectAttributes,
						0,
						NULL,
						REG_OPTION_NON_VOLATILE,
						NULL
					);
	if(!NT_SUCCESS(status)) {
		return status;
	}

	status = DrReadKeyValue(registryKey, ValueKeyName, ValueKeyType, Buffer, BufferLength, BufferLengthNeeded);
	ZwClose(registryKey);

	return status;

}

NTSTATUS
DrReadDevKeyValueInstantly(
		PDEVICE_OBJECT	PhyDeviceObject,
		PWCHAR	ValueKeyName,
		ULONG	ValueKeyType,
		PVOID	Buffer,
		ULONG	BufferLength,
		PULONG	BufferLengthNeeded
){
	NTSTATUS	status;
	HANDLE		registryKey;

	status = DrOpenDeviceRegistry(PhyDeviceObject, &registryKey, KEY_READ);
	if(!NT_SUCCESS(status)) {
		return status;
	}

	status = DrReadKeyValue(registryKey, ValueKeyName, ValueKeyType, Buffer, BufferLength, BufferLengthNeeded);
	ZwClose(registryKey);

	return status;

}
		
//
//	Open a device parameter registry.
//
NTSTATUS
DrOpenDeviceRegistry(
		PDEVICE_OBJECT	DeviceObject,
		HANDLE			*DeviceParamReg,
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
		KDPrint(1, ("IoOpenDeviceRegistryKey() failed. NTSTATUS:%08lx\n", status));
        regKey = NULL;
    }

	*DeviceParamReg = regKey;

    return status;
}


//
//	Delete all subkey under a parent key.
//

NTSTATUS
DrDeleteAllSubKeys(
		HANDLE				ParentKey
	) {

	NTSTATUS					status;
	PKEY_BASIC_INFORMATION		keyInfo;
	ULONG						outLength;
	ULONG						idxKey;
	OBJECT_ATTRIBUTES			objectAttributes;
	UNICODE_STRING				objectName;
	HANDLE						childKey;

	keyInfo = (PKEY_BASIC_INFORMATION)ExAllocatePoolWithTag(PagedPool, 512, DEVREG_POOTAG_KEYINFO);
	if(!keyInfo) {
		KDPrint(1, ("ExAllocatePoolWithTag(KEY_BASIC_INFORMATION) failed.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	status = STATUS_SUCCESS;
	for(idxKey = 0 ; idxKey < DEVREG_MAX_REGISTRY_KEY; idxKey ++) {
		status = ZwEnumerateKey(
						ParentKey,
						idxKey,
						KeyBasicInformation,
						keyInfo,
						512,
						&outLength
						);

		if(status == STATUS_NO_MORE_ENTRIES) {
			KDPrint(1, ("No more entries.\n"));
			status = STATUS_SUCCESS;
			break;
		}
		if(status != STATUS_SUCCESS) {
			ASSERT(status != STATUS_BUFFER_OVERFLOW && status != STATUS_BUFFER_TOO_SMALL);
			KDPrint(1, ("ZwEnumerateKey() failed. NTSTATUS:%08lx\n", status));
			ExFreePool(keyInfo);
			return STATUS_SUCCESS;
		}


		//
		//	Open a sub key
		//
		objectName.Length = objectName.MaximumLength = (USHORT)keyInfo->NameLength;
		objectName.Buffer = keyInfo->Name;
		InitializeObjectAttributes(		&objectAttributes,
										&objectName,
										OBJ_KERNEL_HANDLE,
										ParentKey,
										NULL
								);
		status = ZwOpenKey(&childKey, KEY_ALL_ACCESS, &objectAttributes);
		if(!NT_SUCCESS(status)) {
			KDPrint(1, ("ZwOpenKey() failed. NTSTATUS:%08lx\n", status));
			continue;
		}

		//
		//	Delete all subkeys
		//
		status = DrDeleteAllSubKeys(childKey);
		if(!NT_SUCCESS(status)) {
			KDPrint(1, ("Recursive DrDeleteAllSubKeys() failed. NTSTATUS:%08lx\n", status));
			ZwClose(childKey);
			continue;
		}

		//
		//	Delete NDAS device instance.
		//
		status = ZwDeleteKey(childKey);
#if DBG
		if(!NT_SUCCESS(status)) {
			KDPrint(1, ("ZwDeleteKey() failed. NTSTATUS:%08lx\n", status));
		}
#endif
		ZwClose(childKey);

		//
		//	One key was deleted, decrement key index.
		//

		idxKey--;

	}

	ExFreePool(keyInfo);
	return STATUS_SUCCESS;
}


//
//	Delete a key including a parent key.
//

NTSTATUS
DrDeleteAllKeys(
		HANDLE			ParentKey
){
	NTSTATUS	status;

	status = DrDeleteAllSubKeys(ParentKey);
	if(!NT_SUCCESS(status)) {
		return status;
	}
	status = ZwDeleteKey(ParentKey);

	return status;
}



//////////////////////////////////////////////////////////////////////////
//
//	Driver installation check.
//

NTSTATUS
DrDriverServiceExist(
	PWSTR  DriverServiceName
){
	NTSTATUS	status;

	status = RtlCheckRegistryKey(RTL_REGISTRY_SERVICES,
									DriverServiceName);

	return status;
}
