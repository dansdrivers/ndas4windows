/*++

Copyright (C) 2003-2006 XIMETA, Inc. All rights reserved.

Module Name:

    internal.c

Abstract:

    

Author:

    Chesong Lee (cslee@ximeta.com) 2006/05/05

Environment:

    Kernel mode

--*/
#include "port.h"

#ifdef RUN_WPP
#include "internal.tmh"
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NdasPortFdoDispatchDeviceControl)
#pragma alloc_text(PAGE, NdasPortFdoDispatchCreateClose)
#pragma alloc_text(PAGE, NdasPortFdoDeviceControlPlugInLogicalUnit)
#pragma alloc_text(PAGE, NdasPortFdoDeviceControlEjectLogicalUnit)
#pragma alloc_text(PAGE, NdasPortFdoDeviceControlUnplugLogicalUnit)
#pragma alloc_text(PAGE, NdasPortFdoDeviceControlLogicalUnitAddressInUse)
#pragma alloc_text(PAGE, NdasPortFdoOpenRegistryKey)
#pragma alloc_text(PAGE, NdasPortFdoRegEnumerateLogicalUnits)
#pragma alloc_text(PAGE, NdasPortFdoRegSaveLogicalUnit)
#pragma alloc_text(PAGE, NdasPortFdoRegDeleteLogicalUnit)
#endif // ALLOC_PRAGMA

NTSYSAPI
NTSTATUS
NTAPI
ZwDeleteValueKey(
    __in HANDLE KeyHandle,
    __in PUNICODE_STRING ValueName
    );

extern
NTSTATUS
NdasAtaDeviceGetNdasLogicalUnitInterface(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__out PNDAS_LOGICALUNIT_INTERFACE LogicalUnitInterface,
	__out PULONG LogicalUnitExtensionSize,
	__out PULONG SrbExtensionSize);

NTSTATUS
FORCEINLINE
NdasPortFdoQueryProperty(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PSTORAGE_PROPERTY_QUERY Query,
	__in ULONG InputBufferLength,
	__in ULONG OutputBufferLength,
	__out PULONG ResultLength,
	__out PSTORAGE_DESCRIPTOR_HEADER DescriptorHeader)
{
	ASSERT(InputBufferLength >= 
		FIELD_OFFSET(STORAGE_PROPERTY_QUERY, AdditionalParameters));

	//
	// FDO only supports a query on the adapter property.
	//

	if (StorageAdapterProperty != Query->PropertyId)
	{
		*ResultLength = 0;
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	*ResultLength = min(OutputBufferLength, sizeof(STORAGE_ADAPTER_DESCRIPTOR));

	RtlCopyMemory(
		DescriptorHeader,
		&FdoExtension->StorageAdapterDescriptor,
		*ResultLength);

	return STATUS_SUCCESS;	
}

NTSTATUS
NdasPortFdoDispatchDeviceControl(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp)
{
	NTSTATUS status;
	
	PNDASPORT_FDO_EXTENSION FdoExtension = 
		(PNDASPORT_FDO_EXTENSION) DeviceObject->DeviceExtension;
	PNDASPORT_COMMON_EXTENSION CommonExtension = 
		FdoExtension->CommonExtension;

    PIO_STACK_LOCATION irpStack;
	ULONG ioControlCode;
	ULONG isRemoved;

	PAGED_CODE();

	irpStack = IoGetCurrentIrpStackLocation(Irp);
	ioControlCode = irpStack->Parameters.DeviceIoControl.IoControlCode;

	NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_INFORMATION,
		"FDO: IOCTL %s(%08X) (Type=%04X,Function=%03X,Method=%01X,Access=%01X), Fdo=%p, Irp=%p\n", 
		DbgIoControlCodeStringA(ioControlCode),
		ioControlCode, 
		(ioControlCode & 0xFFFF0000) >> 16,
		(ioControlCode & 0x00001FFC) >> 2 ,
		(ioControlCode & 0x00000003) >> 0,
		(ioControlCode & 0x0000C000) >> 13,
		DeviceObject,
		Irp);

	isRemoved = NpAcquireRemoveLock(CommonExtension, Irp);
    if (isRemoved) 
    {
		return NpReleaseRemoveLockAndCompleteIrp(
			CommonExtension, 
			Irp, 
			STATUS_DEVICE_DOES_NOT_EXIST);
    }

	switch (ioControlCode) 
    {
	//
	// Adapter-specific IOCTL
	//
    case IOCTL_STORAGE_QUERY_PROPERTY:
		{
			PSTORAGE_PROPERTY_QUERY query;
			ULONG inputBufferLength;
			ULONG resultLength;

			query = (PSTORAGE_PROPERTY_QUERY) Irp->AssociatedIrp.SystemBuffer;
			inputBufferLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;

			if (inputBufferLength < sizeof(STORAGE_PROPERTY_QUERY))
			{
				return NpReleaseRemoveLockAndCompleteIrp(
					CommonExtension,
					Irp,
					STATUS_INVALID_PARAMETER);
			}

			status = NdasPortFdoQueryProperty(
				FdoExtension,
				query,
				inputBufferLength,
				irpStack->Parameters.DeviceIoControl.OutputBufferLength,
				&resultLength,
				(PSTORAGE_DESCRIPTOR_HEADER) query);

			return NpReleaseRemoveLockAndCompleteIrpEx(
				CommonExtension,
				Irp,
				status,
				resultLength,
				IO_NO_INCREMENT);
		}
    case IOCTL_SCSI_GET_CAPABILITIES: 
		//
		// If the output buffer is equal to the size of the a PVOID then just
		// return a pointer to the buffer.
		//
		if (irpStack->Parameters.DeviceIoControl.OutputBufferLength == 
			sizeof(PVOID)) 
		{

			*((PVOID *)Irp->AssociatedIrp.SystemBuffer)
				= &FdoExtension->IoScsiCapabilities;

			return NpReleaseRemoveLockAndCompleteIrpEx(
				CommonExtension,
				Irp,
				STATUS_SUCCESS,
				sizeof(PVOID),
				IO_NO_INCREMENT);
		}

		if (irpStack->Parameters.DeviceIoControl.OutputBufferLength < 
			sizeof(IO_SCSI_CAPABILITIES)) 
		{
			return NpReleaseRemoveLockAndCompleteIrpEx(
				CommonExtension,
				Irp,
				STATUS_BUFFER_TOO_SMALL,
				Irp->IoStatus.Information,
				IO_NO_INCREMENT);
		}

		RtlCopyMemory(
			Irp->AssociatedIrp.SystemBuffer,
			&FdoExtension->IoScsiCapabilities,
			sizeof(IO_SCSI_CAPABILITIES));

		return NpReleaseRemoveLockAndCompleteIrpEx(
			CommonExtension,
			Irp,
			STATUS_SUCCESS,
			sizeof(IO_SCSI_CAPABILITIES),
			IO_NO_INCREMENT);

    case IOCTL_SCSI_PASS_THROUGH:
    case IOCTL_SCSI_PASS_THROUGH_DIRECT: 

    case IOCTL_SCSI_GET_DUMP_POINTERS: 

	case IOCTL_SCSI_RESCAN_BUS:
	case IOCTL_SCSI_GET_INQUIRY_DATA: 

		return NpReleaseRemoveLockAndCompleteIrp(
			CommonExtension,
			Irp,
			STATUS_NOT_SUPPORTED);

	case IOCTL_SCSI_MINIPORT: 

		//
		// TODO: Redirect IOCTL to the logical unit implementation
		// for some IOCTLs, of which structure contains an identifier
		// for the target logical unit.
		//

		return NpReleaseRemoveLockAndCompleteIrp(
			CommonExtension,
			Irp,
			STATUS_NOT_SUPPORTED);

		//
		// Proprietary IOCTLs of NDASPORT
		//
	case IOCTL_NDASPORT_PLUGIN_LOGICALUNIT:
		{
			PNDAS_LOGICALUNIT_DESCRIPTOR descriptor;
			ULONG bufferLength;

			descriptor = (PNDAS_LOGICALUNIT_DESCRIPTOR) Irp->AssociatedIrp.SystemBuffer;
			bufferLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;

			if (bufferLength < sizeof(NDAS_LOGICALUNIT_DESCRIPTOR) ||
				descriptor->Version != sizeof(NDAS_LOGICALUNIT_DESCRIPTOR) ||
				descriptor->Size < bufferLength)
			{
				status = STATUS_INVALID_PARAMETER;
			}
			else
			{
				status = NdasPortFdoDeviceControlPlugInLogicalUnit(
					FdoExtension, 
					descriptor,
					NDASPORT_FDO_PLUGIN_FLAG_NONE);
			}

			return NpReleaseRemoveLockAndCompleteIrp(
				CommonExtension,
				Irp,
				status);
		}
	case IOCTL_NDASPORT_EJECT_LOGICALUNIT:
		{
			PNDASPORT_LOGICALUNIT_EJECT param;
			ULONG bufferLength;
			param = (PNDASPORT_LOGICALUNIT_EJECT) Irp->AssociatedIrp.SystemBuffer;
			bufferLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;

			if (bufferLength < sizeof(NDASPORT_LOGICALUNIT_EJECT) ||
				param->Size != sizeof(NDASPORT_LOGICALUNIT_EJECT))
			{
				status = STATUS_INVALID_PARAMETER;
			}
			else
			{
				status = NdasPortFdoDeviceControlEjectLogicalUnit(FdoExtension, param);
			}

			return NpReleaseRemoveLockAndCompleteIrp(
				CommonExtension,
				Irp,
				status);
		}
	case IOCTL_NDASPORT_UNPLUG_LOGICALUNIT:
		{
			PNDASPORT_LOGICALUNIT_UNPLUG param;
			ULONG bufferLength;
			param = (PNDASPORT_LOGICALUNIT_UNPLUG) Irp->AssociatedIrp.SystemBuffer;
			bufferLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;

			Irp->IoStatus.Information = 0;

			if (bufferLength < sizeof(NDASPORT_LOGICALUNIT_UNPLUG) ||
				param->Size != sizeof(NDASPORT_LOGICALUNIT_UNPLUG))
			{
				status = STATUS_INVALID_PARAMETER;
			}
			else
			{
				status = NdasPortFdoDeviceControlUnplugLogicalUnit(FdoExtension, param);
			}

			return NpReleaseRemoveLockAndCompleteIrp(
				CommonExtension,
				Irp,
				status);
		}
	case IOCTL_NDASPORT_IS_LOGICALUNIT_ADDRESS_IN_USE:
		{
			PNDAS_LOGICALUNIT_ADDRESS logicalUnitAddress;
			ULONG bufferLength;

			logicalUnitAddress = (PNDAS_LOGICALUNIT_ADDRESS) 
				Irp->AssociatedIrp.SystemBuffer;

			bufferLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;

			Irp->IoStatus.Information = 0;

			if (bufferLength < sizeof(NDAS_LOGICALUNIT_ADDRESS))
			{
				status = STATUS_INVALID_PARAMETER;
			}
			else
			{
				status = NdasPortFdoDeviceControlLogicalUnitAddressInUse(
					FdoExtension,
					logicalUnitAddress);
				Irp->IoStatus.Information = NT_SUCCESS(status) ?
					sizeof(NDAS_LOGICALUNIT_ADDRESS) : 0;				
			}

			return NpReleaseRemoveLockAndCompleteIrp(
				CommonExtension,
				Irp,
				status);
		}
	case IOCTL_NDASPORT_GET_PORT_NUMBER:
		{
			PULONG portNumber;
			ULONG bufferLength;
			bufferLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
			if (bufferLength < sizeof(ULONG))
			{
				status = STATUS_INVALID_PARAMETER;
				Irp->IoStatus.Information = 0;
			}
			else
			{
				portNumber = (PULONG) Irp->AssociatedIrp.SystemBuffer;
				*portNumber = (ULONG) FdoExtension->PortNumber;
				status = STATUS_SUCCESS;
				Irp->IoStatus.Information = sizeof(ULONG);
			}
			return NpReleaseRemoveLockAndCompleteIrp(
				CommonExtension,
				Irp,
				status);
		}
		break;
	case IOCTL_NDASPORT_GET_LOGICALUNIT_DESCRIPTOR:
		{
			PNDAS_LOGICALUNIT_ADDRESS logicalUnitAddress;
			ULONG bufferLength;

			logicalUnitAddress = (PNDAS_LOGICALUNIT_ADDRESS) 
				Irp->AssociatedIrp.SystemBuffer;

			bufferLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;

			Irp->IoStatus.Information = 0;

			if (bufferLength < sizeof(NDAS_LOGICALUNIT_ADDRESS))
			{
				status = STATUS_INVALID_PARAMETER;
			}
			else
			{
				ULONG outputBufferLength = 
					irpStack->Parameters.DeviceIoControl.OutputBufferLength;

				status = NdasPortFdoDeviceControlGetLogicalUnitDescriptor(
					FdoExtension,
					logicalUnitAddress,
					Irp->AssociatedIrp.SystemBuffer,
					&outputBufferLength);

				Irp->IoStatus.Information = NT_SUCCESS(status) ? outputBufferLength : 0;				
			}

			return NpReleaseRemoveLockAndCompleteIrp(
				CommonExtension,
				Irp,
				status);
		}
    default: 
		{
			NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_WARNING,
				"NdasPortFdoDispatchDeviceControl: Unsupported IOCTL (%x)\n",
				ioControlCode);

			return NpReleaseRemoveLockAndCompleteIrp(
				CommonExtension,
				Irp,
				STATUS_NOT_SUPPORTED);
		}
    }
}

NTSTATUS
NdasPortFdoGetExtendedInterface(
	CONST GUID* ExternalTypeGuid,
	PFILE_OBJECT* ExternalFileObject,
	PDEVICE_OBJECT* ExternalDeviceObject,
	PNDAS_LU_QUERY_NDAS_LOGICALUNIT_INTERFACE* LogicalUnitInterface)
{
	NTSTATUS status;
	UNICODE_STRING externalDeviceTypeGuidString;
	WCHAR externalDeviceNameBuffer[64];
	UNICODE_STRING externalDeviceName;
	PIRP irp;
	KEVENT event;
	IO_STATUS_BLOCK ioStatus;
	NDASPORT_EXTERNAL_TYPE_GET_INTERFACE externalTypeGetInterface;

	PAGED_CODE();

	*ExternalFileObject = NULL;
	*ExternalDeviceObject = NULL;

	RtlInitEmptyUnicodeString(
		&externalDeviceName,
		externalDeviceNameBuffer,
		sizeof(externalDeviceNameBuffer));

	status = RtlStringFromGUID(
		ExternalTypeGuid,
		&externalDeviceTypeGuidString);

	if (!NT_SUCCESS(status))
	{
		return status;
	}

	status = RtlUnicodeStringCopyString(
		&externalDeviceName,
		L"\\Device\\NdasPort_");

	ASSERT(NT_SUCCESS(status));
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	status = RtlUnicodeStringCatUnicodeString(
		&externalDeviceName,
		&externalDeviceTypeGuidString);

	RtlFreeUnicodeString(&externalDeviceTypeGuidString);

	ASSERT(NT_SUCCESS(status));
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	status = IoGetDeviceObjectPointer(
		&externalDeviceName,
		FILE_ALL_ACCESS,
		ExternalFileObject,
		ExternalDeviceObject);

	if (!NT_SUCCESS(status))
	{
		return status;
	}

	KeInitializeEvent(&event, NotificationEvent, FALSE);

	externalTypeGetInterface.Size = sizeof(NDASPORT_EXTERNAL_TYPE_GET_INTERFACE);
	externalTypeGetInterface.Version = NDASPORT_EXTERNAL_TYPE_GET_INTERFACE_VERSION;
	externalTypeGetInterface.ExternalTypeGuid = *ExternalTypeGuid;
	externalTypeGetInterface.GetInterfaceFunction = NULL;
	
	irp = IoBuildDeviceIoControlRequest(
		NDASPORTEXT_IOCTL_GET_LOGICALUNIT_INTERFACE,
		*ExternalDeviceObject,
		&externalTypeGetInterface, sizeof(NDASPORT_EXTERNAL_TYPE_GET_INTERFACE),
		&externalTypeGetInterface, sizeof(NDASPORT_EXTERNAL_TYPE_GET_INTERFACE),
		TRUE,
		&event,
		&ioStatus);

	if (NULL == irp)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto error1;
	}

	status = IoCallDriver(*ExternalDeviceObject, irp);

	if (!NT_SUCCESS(status))
	{
		goto error2;
	}

	KeWaitForSingleObject(
		&event, 
		Executive, 
		KernelMode, 
		FALSE, 
		NULL);

	status = ioStatus.Status;

	if (!NT_SUCCESS(status))
	{
		goto error3;
	}
	
	*LogicalUnitInterface = externalTypeGetInterface.GetInterfaceFunction;

	return status;

error3:
error2:
error1:

	ObDereferenceObject(*ExternalFileObject);
	*ExternalDeviceObject = NULL;
	*ExternalFileObject = NULL;

	return status;
}

NTSTATUS
NdasPortFdoDeviceControlGetLogicalUnitDescriptor(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PNDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress,
	__in PVOID OutputBuffer,
	__inout PULONG OutputBufferLength)
{
	NTSTATUS status;
	PNDASPORT_PDO_EXTENSION targetPdo;
	PLIST_ENTRY entry;
	BOOLEAN found;

	PAGED_CODE();

	NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_INFORMATION,
		"NdasPortFdoDeviceControlGetLogicalUnitDescriptor: Fdo=%p, (%d,%d,%d,%d)\n", 
		FdoExtension->DeviceObject, 
		LogicalUnitAddress->PortNumber,
		LogicalUnitAddress->PathId,
		LogicalUnitAddress->TargetId,
		LogicalUnitAddress->Lun);

	if (LogicalUnitAddress->PortNumber != FdoExtension->PortNumber)
	{
		return STATUS_INVALID_PARAMETER_1;
	}

	found = FALSE;

	status = STATUS_ADDRESS_NOT_ASSOCIATED;

	ExAcquireFastMutex (&FdoExtension->Mutex);

	for (entry = FdoExtension->ListOfPDOs.Flink;
		entry != &FdoExtension->ListOfPDOs;
		entry = entry->Flink) 
	{
		targetPdo = CONTAINING_RECORD(entry, NDASPORT_PDO_EXTENSION, Link);

		if (LogicalUnitAddress->Address == targetPdo->LogicalUnitAddress.Address)
		{
			//
			// Copy as much as given only
			//

			if (*OutputBufferLength > targetPdo->LogicalUnitDescriptor->Size)
			{
				*OutputBufferLength = targetPdo->LogicalUnitDescriptor->Size;
			}
			
			RtlCopyMemory(
				OutputBuffer, 
				targetPdo->LogicalUnitDescriptor,
				*OutputBufferLength);

			status = STATUS_SUCCESS;

			break;
		}
	}

	ExReleaseFastMutex (&FdoExtension->Mutex); 

	return status;
}

NTSTATUS
NdasPortFdoOpenRegistryKey(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in NDASPORT_FDO_REGKEY_TYPE RegKeyType,
	__in ACCESS_MASK DesiredAccess,
	__out PHANDLE KeyHandle)
{
	NTSTATUS status, status2;
	HANDLE rootKeyHandle, targetKeyHandle;

	PAGED_CODE();

	*KeyHandle = NULL;

	switch (RegKeyType)
	{
	case NDASPORT_FDO_REGKEY_ROOT:
	case NDASPORT_FDO_REGKEY_LOGICALUNIT_LIST:
		break;
	default:
		ASSERTMSG("Invalid parameter specified", FALSE);
		return STATUS_INVALID_PARAMETER_2;
	}

	//
	// IoOpenDeviceRegistryKey requires PDO (not FDO)
	//

	rootKeyHandle = NULL;
	status = IoOpenDeviceRegistryKey(
		FdoExtension->LowerPdo,
		PLUGPLAY_REGKEY_DEVICE, 
		DesiredAccess | KEY_CREATE_SUB_KEY, // required for subkey creation
		&rootKeyHandle);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_ERROR,
			"IoOpenDeviceRegistryKey failed, status=%X, fdo=%p\n", 
			status, FdoExtension->DeviceObject);
		return status;
	}

	switch (RegKeyType)
	{
	case NDASPORT_FDO_REGKEY_ROOT:
		targetKeyHandle = rootKeyHandle;
		break;
	case NDASPORT_FDO_REGKEY_LOGICALUNIT_LIST:
		{
			OBJECT_ATTRIBUTES keyAttributes;
			WCHAR keyNameBuffer[16] = L"LogicalUnits";
			UNICODE_STRING keyName;

			RtlInitUnicodeString(&keyName, keyNameBuffer);
			
			InitializeObjectAttributes(
				&keyAttributes,
				&keyName,
				OBJ_CASE_INSENSITIVE | OBJ_OPENIF | OBJ_KERNEL_HANDLE,
				rootKeyHandle,
				NULL);

			targetKeyHandle = NULL;

			status = ZwCreateKey(
				&targetKeyHandle,
				DesiredAccess,
				&keyAttributes,
				0,
				NULL,
				REG_OPTION_NON_VOLATILE,
				NULL);

			if (!NT_SUCCESS(status))
			{
				status2 = ZwClose(rootKeyHandle);
				ASSERT(NT_SUCCESS(status2));
				return status;
			}

			status2 = ZwClose(rootKeyHandle);
			ASSERT(NT_SUCCESS(status2));
		}
		break;
	default:
		;
	}

	*KeyHandle = targetKeyHandle;

	return status;
}

NTSTATUS
NdasPortFdoRegEnumerateLogicalUnits(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PNDASPORT_ENUMERATE_LOGICALUNIT_CALLBACK Callback,
	__in PVOID CallbackContext)
{
	NTSTATUS status, status2;
	HANDLE keyHandle;
	PKEY_VALUE_FULL_INFORMATION keyValueInformation;
	ULONG keyValueInformationLength;
	ULONG resultLength;
	ULONG index;

	PNDAS_LOGICALUNIT_DESCRIPTOR descriptor;

	PAGED_CODE();

	keyHandle = NULL;
	status = NdasPortFdoOpenRegistryKey(
		FdoExtension,
		NDASPORT_FDO_REGKEY_LOGICALUNIT_LIST,
		KEY_READ,
		&keyHandle);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_ERROR,
			"IoOpenDeviceRegistryKey failed, status=%X, fdo=%p\n", 
			status,
			FdoExtension->DeviceObject);
		return status;
	}

	keyValueInformation = NULL;
	keyValueInformationLength = 0;

	for (index = 0; ;)
	{
		status = ZwEnumerateValueKey(
			keyHandle,
			index,
			KeyValueFullInformation,
			keyValueInformation,
			keyValueInformationLength,
			&resultLength);

		if (NT_SUCCESS(status))
		{
			//
			// value prefix must be LU
			//
			if (keyValueInformation->NameLength < 4 ||
				!RtlEqualMemory(keyValueInformation->Name, L"LU", 4))
			{
				NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_WARNING,
					"Value is not prefixed with LU, value=%ws\n", 
					keyValueInformation->NameLength ? 
					keyValueInformation->Name : L"(default)");
				// ignore the invalid key values
				++index;
				continue;
			}
			//
			// It should contain valid logical unit descriptor
			//
			if (keyValueInformation->DataLength < sizeof(NDAS_LOGICALUNIT_DESCRIPTOR))
			{
				NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_WARNING,
					"Data is not a valid descriptor, DataLength=0x%X\n", 
					keyValueInformation->DataLength);
				// ignore the invalid key values
				++index;
				continue;
			}

			descriptor = (PNDAS_LOGICALUNIT_DESCRIPTOR) NdasPortOffsetOf(
				keyValueInformation, keyValueInformation->DataOffset);
			
			//
			// Descriptor version should be the size of the structure
			//
			if (sizeof(NDAS_LOGICALUNIT_DESCRIPTOR) != descriptor->Version)
			{	
				NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_WARNING,
					"Data is not a valid descriptor, descriptor->Version=0x%X\n", 
					descriptor->Version);
				// ignore the invalid key values
				++index;
				continue;
			}

			//
			// DataLength should be larger than the size specified in
			// the descriptor
			//
			if (keyValueInformation->DataLength < descriptor->Size)
			{
				NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_WARNING,
					"Data is not a valid descriptor, "
					"DataLength is less than the size in the descriptor "
					"descriptor->Size=0x%X, dataLength=0x%X\n", 
					descriptor->Size,
					keyValueInformation->DataLength);
				// ignore the invalid key values
				++index;
				continue;
			}

			//
			// Now we get the valid logical unit descriptor
			// Invoke the callback function
			//
			(*Callback)(FdoExtension, descriptor, CallbackContext);

			//
			// Next entry
			//
			++index;
			continue;
		}
		else if (STATUS_NO_MORE_ENTRIES == status)
		{
			//
			// STATUS_NO_MORE_ENTRIES means success for this function
			//
			status = STATUS_SUCCESS;
			break;
		}
		else if (STATUS_BUFFER_OVERFLOW == status ||
			STATUS_BUFFER_TOO_SMALL == status)
		{
			if (NULL != keyValueInformation)
			{
				ExFreePoolWithTag(
					keyValueInformation, 
					NDASPORT_TAG_REGISTRY_ENUM);
			}
			
			keyValueInformation = ExAllocatePoolWithTag(
				PagedPool, resultLength, NDASPORT_TAG_REGISTRY_ENUM);

			if (NULL == keyValueInformation)
			{
				status2 = ZwClose(keyHandle);
				ASSERT(NT_SUCCESS(status2));
				status = STATUS_NO_MEMORY;
				break;
			}
			
			keyValueInformationLength = resultLength;

			//
			// try again with the same index
			//
			continue;
		}
		else
		{
			//
			// retain the status value and return the status code
			//
			break;
		}
	}

	if (NULL != keyValueInformation)
	{
		ExFreePoolWithTag(
			keyValueInformation, 
			NDASPORT_TAG_REGISTRY_ENUM);
	}

	status2 = ZwClose(keyHandle);

	ASSERT(NT_SUCCESS(status2));
	if (!NT_SUCCESS(status2))
	{
		NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_WARNING,
			"ZwClose failed, keyHandle=%p, status=%X\n", 
			keyHandle, status2);
	}

	return status;
}

NTSTATUS
NdasPortFdoRegSaveLogicalUnit(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor)
{
	NTSTATUS status, status2;
	HANDLE keyHandle;
	WCHAR valueNameBuffer[32];
	UNICODE_STRING valueName;

	PAGED_CODE();

	keyHandle = NULL;
	status = NdasPortFdoOpenRegistryKey(
		FdoExtension,
		NDASPORT_FDO_REGKEY_LOGICALUNIT_LIST,
		KEY_WRITE,
		&keyHandle);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_ERROR,
			"IoOpenDeviceRegistryKey failed, status=%X, fdo=%p\n", 
			status, FdoExtension->DeviceObject);
		return status;
	}

	RtlInitEmptyUnicodeString(
		&valueName, 
		valueNameBuffer, 
		sizeof(valueNameBuffer));

	status2 = RtlUnicodeStringPrintf(
		&valueName, 
		L"LU%08X",
		LogicalUnitDescriptor->Address.Address);

	ASSERT(NT_SUCCESS(status2));

	status = ZwSetValueKey(
		keyHandle, 
		&valueName,
		0, 
		REG_BINARY,
		LogicalUnitDescriptor,
		LogicalUnitDescriptor->Size);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_ERROR,
			"ZwSetValueKey failed, keyHandle=%p, value=%wZ, status=%X\n", 
			keyHandle, &valueName, status);
		goto ret;
	}

	status = STATUS_SUCCESS;

	goto ret;

ret:
	status2 = ZwClose(keyHandle);

	ASSERT(NT_SUCCESS(status2));
	if (!NT_SUCCESS(status2))
	{
		NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_WARNING,
			"ZwClose failed, keyHandle=%p, status=%X\n", 
			keyHandle, status2);
	}

	return status;
}

NTSTATUS
NdasPortFdoRegDeleteLogicalUnit(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress)
{
	NTSTATUS status, status2;
	HANDLE keyHandle;
	WCHAR valueNameBuffer[32];
	UNICODE_STRING valueName;

	PAGED_CODE();

	keyHandle = NULL;
	status = NdasPortFdoOpenRegistryKey(
		FdoExtension,
		NDASPORT_FDO_REGKEY_LOGICALUNIT_LIST,
		KEY_WRITE,
		&keyHandle);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_ERROR,
			"IoOpenDeviceRegistryKey failed, status=%X, fdo=%p\n", 
			status,
			FdoExtension->DeviceObject);
		return status;
	}

	RtlInitEmptyUnicodeString(
		&valueName, 
		valueNameBuffer, 
		sizeof(valueNameBuffer));

	status2 = RtlUnicodeStringPrintf(
		&valueName, 
		L"LU%08X",
		LogicalUnitAddress);

	ASSERT(NT_SUCCESS(status2));

	status = ZwDeleteValueKey(
		keyHandle,
		&valueName);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_ERROR,
			"ZwDeleteValueKey failed, keyHandle=%p, value=%wZ, status=%X\n", 
			keyHandle, &valueName, status);
		goto ret;
	}

	status = STATUS_SUCCESS;

	goto ret;

ret:
	status2 = ZwClose(keyHandle);

	ASSERT(NT_SUCCESS(status2));
	if (!NT_SUCCESS(status2))
	{
		NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_WARNING,
			"ZwClose failed, keyHandle=%p, status=%X\n", 
			keyHandle, status2);
	}

	return status;
}


NTSTATUS
NdasPortFdoDeviceControlPlugInLogicalUnit(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in ULONG InternalFlags)
{
	NTSTATUS status;
	PDEVICE_OBJECT physicalDeviceObject;

	PNDASPORT_PDO_EXTENSION pdoExtension;
	NDAS_LOGICALUNIT_INTERFACE logicalUnitInterface;
	ULONG logicalUnitExtensionSize;
	ULONG logicalUnitSrbExtensionSize;
	NDASPORT_PDO_EXTENSION tmp = {0};
	PNDAS_LU_QUERY_NDAS_LOGICALUNIT_INTERFACE logicalUnitInterfaceQuery;

	PFILE_OBJECT externalFileObject;
	PDEVICE_OBJECT externalDeviceObject;

	PLIST_ENTRY entry;

	PAGED_CODE();

	NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_INFORMATION,
		"PlugInLogicalUnit: Fdo=%p, Descriptor=%p (%d,%d,%d,%d)\n", 
		FdoExtension->DeviceObject, 
		LogicalUnitDescriptor,
		LogicalUnitDescriptor->Address.PortNumber,
		LogicalUnitDescriptor->Address.PathId,
		LogicalUnitDescriptor->Address.TargetId,
		LogicalUnitDescriptor->Address.Lun);

	if (LogicalUnitDescriptor->Address.PortNumber != FdoExtension->PortNumber)
	{
		NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_ERROR,
			"Port Number is not invalid. UserPortNumber=%d, PortNumber=%d\n",
			LogicalUnitDescriptor->Address.PortNumber,
			FdoExtension->PortNumber);

		return STATUS_INVALID_PARAMETER_1;
	}

	//
	// Following code walks the PDO list to check whether
	// the input serial number is unique.
	// Let's first assume it's unique
	//

	tmp.IsTemporary = TRUE;
	tmp.LogicalUnitAddress = LogicalUnitDescriptor->Address;

	//
	// Just be aware: If a user unplugs and plugs in the device 
	// immediately, in a split second, even before we had a chance to 
	// clean up the previous PDO completely, we might fail to 
	// accept the device.
	//

	ExAcquireFastMutex (&FdoExtension->Mutex);

	for (entry = FdoExtension->ListOfPDOs.Flink;
		entry != &FdoExtension->ListOfPDOs;
		entry = entry->Flink) 
	{
		PNDASPORT_PDO_EXTENSION pdoEntry;
		pdoEntry = CONTAINING_RECORD(entry, NDASPORT_PDO_EXTENSION, Link);

		if (tmp.LogicalUnitAddress.Address == pdoEntry->LogicalUnitAddress.Address)
		{
			ExReleaseFastMutex (&FdoExtension->Mutex); 
			return STATUS_OBJECT_NAME_COLLISION;
		}
	}

	//
	// We adds a temporary LU for the place holder
	//

	InsertTailList(&FdoExtension->ListOfPDOs, &tmp.Link);
	++(FdoExtension->NumberOfPDOs);

	ExReleaseFastMutex (&FdoExtension->Mutex); 

	//
	// Save the information to the registry
	//

	if (!TestFlag(InternalFlags, NDASPORT_FDO_PLUGIN_FLAG_NO_REGISTRY))
	{
		status = NdasPortFdoRegSaveLogicalUnit(
			FdoExtension, LogicalUnitDescriptor);

		if (!NT_SUCCESS(status))
		{
			NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_ERROR,
				"NdasPortFdoGetExtendedInterface failed, Status=%x\n", 
				status);

			//
			// Delete the temporary LU
			//
			ExAcquireFastMutex (&FdoExtension->Mutex);
			RemoveEntryList(&tmp.Link);
			--(FdoExtension->NumberOfPDOs);
			ExReleaseFastMutex (&FdoExtension->Mutex);

			return status;
		}
	}

	//
	// Create the PDO
	//

	status = STATUS_INVALID_PARAMETER;

	logicalUnitInterface.Version = NDAS_LOGICALUNIT_INTERFACE_VERSION;
	logicalUnitInterface.Size = sizeof(NDAS_LOGICALUNIT_INTERFACE);

	externalFileObject = NULL;
	externalDeviceObject = NULL;

	if (LogicalUnitDescriptor->Type & NdasExternalType)
	{
		status = NdasPortFdoGetExtendedInterface(
			&LogicalUnitDescriptor->ExternalTypeGuid,
			&externalFileObject,
			&externalDeviceObject,
			&logicalUnitInterfaceQuery);

		if (!NT_SUCCESS(status))
		{
			NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_ERROR,
				"NdasPortFdoGetExtendedInterface failed, Status=%x\n", 
				status);
		}
		else
		{
			status = (*logicalUnitInterfaceQuery)(
				LogicalUnitDescriptor,
				&logicalUnitInterface,
				&logicalUnitExtensionSize,
				&logicalUnitSrbExtensionSize);
		}

		//
		// Be sure to call ObDereferenceObject(externalFileObject)
		// on failure in this function or later when the PDO is destroyed
		// 
	}
	else
	{
		switch (LogicalUnitDescriptor->Type)
		{
		case NdasDiskDevice:
		case NdasAtaDevice:
			logicalUnitInterfaceQuery = NdasAtaDeviceGetNdasLogicalUnitInterface;
			break;
		default:
			logicalUnitInterfaceQuery = NULL;
			break;
		}

		if (NULL == logicalUnitInterfaceQuery)
		{
			status = STATUS_NOT_SUPPORTED;
		}
		else
		{
			status = (*logicalUnitInterfaceQuery)(
				LogicalUnitDescriptor,
				&logicalUnitInterface,
				&logicalUnitExtensionSize,
				&logicalUnitSrbExtensionSize);
		}
	}

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_ERROR,
			"GetNdasLogicalUnitInterface failed for %d, Status=%x\n", 
			LogicalUnitDescriptor->Type,
			status);

		//
		// Delete the temporary LU
		//
		ExAcquireFastMutex (&FdoExtension->Mutex);
		RemoveEntryList(&tmp.Link);
		--(FdoExtension->NumberOfPDOs);
		ExReleaseFastMutex (&FdoExtension->Mutex);

		//
		// Dereference External File Object if it is external
		//
		if (NULL != externalFileObject)
		{
			ObDereferenceObject(externalFileObject);
			externalFileObject = NULL;
			externalDeviceObject = NULL;
		}

		return status;
	}

	status = NdasPortPdoCreatePhysicalDeviceObject(
		FdoExtension,
		&logicalUnitInterface,
		LogicalUnitDescriptor,
		logicalUnitExtensionSize,
		logicalUnitSrbExtensionSize,
		externalDeviceObject,
		externalFileObject,
		&physicalDeviceObject);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_ERROR,
			"NdasPortPdoCreatePhysicalDeviceObject failed, Status=%08X\n", status);

		//
		// Delete the temporary LU
		//
		ExAcquireFastMutex (&FdoExtension->Mutex);
		RemoveEntryList(&tmp.Link);
		--(FdoExtension->NumberOfPDOs);
		ExReleaseFastMutex (&FdoExtension->Mutex);

		//
		// Dereference External File Object if it is external
		//
		if (NULL != externalFileObject)
		{
			ObDereferenceObject(externalFileObject);
			externalFileObject = NULL;
			externalDeviceObject = NULL;
		}

		return status;
	}

	//
	// Remove the temporary entry and add the actual entry
	// We don't need to change the counter (-1 + +1 = 0 !)
	//

	pdoExtension = NdasPortPdoGetExtension(physicalDeviceObject);

	ExAcquireFastMutex(&FdoExtension->Mutex);

	RemoveEntryList(&tmp.Link);
	InsertTailList(&FdoExtension->ListOfPDOs, &pdoExtension->Link);

	ExReleaseFastMutex(&FdoExtension->Mutex);

	//
	// This should be the last step in initialization.
	//

	physicalDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_INFORMATION,
		"Child physical device object created, Pdo=%p\n", physicalDeviceObject);

	//
	// Device Relation changes if a new pdo is created. So let
	// the PNP system now about that. This forces it to send bunch of PNP
	// queries and cause the function driver to be loaded.
	//

	NdasPortPdoNotifyPnpEvent(
		FdoExtension,
		NdasPortLogicalUnitIsReady,
		LogicalUnitDescriptor->Address);

	IoInvalidateDeviceRelations(FdoExtension->LowerPdo, BusRelations);

	return STATUS_SUCCESS;
}

NTSTATUS
NdasPortFdoDeviceControlEjectLogicalUnit(
	PNDASPORT_FDO_EXTENSION FdoExtension,
	PNDASPORT_LOGICALUNIT_EJECT Parameter)
{
	PNDASPORT_PDO_EXTENSION targetPdo;
	PLIST_ENTRY entry;
	BOOLEAN found;

	PAGED_CODE();

	NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_INFORMATION,
		"EjectLogicalUnit: Fdo=%p, Param=%p (%d,%d,%d,%d)\n", 
		FdoExtension->DeviceObject, 
		Parameter,
		Parameter->LogicalUnitAddress.PortNumber,
		Parameter->LogicalUnitAddress.PathId,
		Parameter->LogicalUnitAddress.TargetId,
		Parameter->LogicalUnitAddress.Lun);

	if (Parameter->LogicalUnitAddress.PortNumber != FdoExtension->PortNumber)
	{
		NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_ERROR,
			"Port Number is not invalid. UserPortNumber=%d, PortNumber=%d\n",
			Parameter->LogicalUnitAddress.PortNumber,
			FdoExtension->PortNumber);

		return STATUS_INVALID_PARAMETER_1;
	}

	found = FALSE;

	ExAcquireFastMutex (&FdoExtension->Mutex);

	for (entry = FdoExtension->ListOfPDOs.Flink;
		entry != &FdoExtension->ListOfPDOs;
		entry = entry->Flink) 
	{
		targetPdo = CONTAINING_RECORD(entry, NDASPORT_PDO_EXTENSION, Link);

		if (Parameter->LogicalUnitAddress.Address == targetPdo->LogicalUnitAddress.Address)
		{
			//
			// Temporary pdo is regarded as none
			//
			if (!targetPdo->IsTemporary)
			{
				found = TRUE;
				IoRequestDeviceEject(targetPdo->DeviceObject);
			}
			break;
		}
	}

	ExReleaseFastMutex (&FdoExtension->Mutex); 

	if (!found)
	{
		return STATUS_DEVICE_DOES_NOT_EXIST;
	}

	return STATUS_SUCCESS;
}

NTSTATUS
NdasPortFdoDeviceControlUnplugLogicalUnit(
	PNDASPORT_FDO_EXTENSION FdoExtension,
	PNDASPORT_LOGICALUNIT_UNPLUG Parameter)
{
	PNDASPORT_PDO_EXTENSION targetPdo;
	PLIST_ENTRY entry;
	BOOLEAN found;

	PAGED_CODE();

	NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_INFORMATION,
		"UnplugLogicalUnit: Fdo=%p, Param=%p (%d,%d,%d,%d)\n", 
		FdoExtension->DeviceObject, 
		Parameter,
		Parameter->LogicalUnitAddress.PortNumber,
		Parameter->LogicalUnitAddress.PathId,
		Parameter->LogicalUnitAddress.TargetId,
		Parameter->LogicalUnitAddress.Lun);

	if (Parameter->LogicalUnitAddress.PortNumber != FdoExtension->PortNumber)
	{
		NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_ERROR,
			"Port Number is not invalid. UserPortNumber=%d, PortNumber=%d\n",
			Parameter->LogicalUnitAddress.PortNumber,
			FdoExtension->PortNumber);

		return STATUS_INVALID_PARAMETER_1;
	}

	found = FALSE;

	ExAcquireFastMutex (&FdoExtension->Mutex);

	for (entry = FdoExtension->ListOfPDOs.Flink;
		entry != &FdoExtension->ListOfPDOs;
		entry = entry->Flink) 
	{
		targetPdo = CONTAINING_RECORD(entry, NDASPORT_PDO_EXTENSION, Link);

		if (Parameter->LogicalUnitAddress.Address == targetPdo->LogicalUnitAddress.Address)
		{
			//
			// Temporary pdo is regarded as none
			//
			if (!targetPdo->IsTemporary)
			{
				found = TRUE;
				targetPdo->Present = FALSE;
			}
			break;
		}
	}

	ExReleaseFastMutex (&FdoExtension->Mutex); 

	if (!found)
	{
		return STATUS_DEVICE_DOES_NOT_EXIST;
	}

	IoInvalidateDeviceRelations(
		FdoExtension->LowerPdo, 
		BusRelations);

	return STATUS_SUCCESS;
}

NTSTATUS
NdasPortFdoDeviceControlLogicalUnitAddressInUse(
	PNDASPORT_FDO_EXTENSION FdoExtension,
	PNDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress)
{
	PNDASPORT_PDO_EXTENSION targetPdo;
	PLIST_ENTRY entry;
	BOOLEAN found;

	PAGED_CODE();

	NdasPortTrace(NDASPORT_FDO_IOCTL, TRACE_LEVEL_INFORMATION,
		"NdasPortFdoDeviceControlLogicalUnitAddressInUse: Fdo=%p, (%d,%d,%d,%d)\n", 
		FdoExtension->DeviceObject, 
		LogicalUnitAddress->PortNumber,
		LogicalUnitAddress->PathId,
		LogicalUnitAddress->TargetId,
		LogicalUnitAddress->Lun);

	if (LogicalUnitAddress->PortNumber != FdoExtension->PortNumber)
	{
		return STATUS_INVALID_PARAMETER_1;
	}

	found = FALSE;

	ExAcquireFastMutex (&FdoExtension->Mutex);

	for (entry = FdoExtension->ListOfPDOs.Flink;
		entry != &FdoExtension->ListOfPDOs;
		entry = entry->Flink) 
	{
		targetPdo = CONTAINING_RECORD(entry, NDASPORT_PDO_EXTENSION, Link);

		if (LogicalUnitAddress->Address == targetPdo->LogicalUnitAddress.Address)
		{
			found = TRUE;
			break;
		}
	}

	ExReleaseFastMutex (&FdoExtension->Mutex); 

	return found ? STATUS_SUCCESS : STATUS_ADDRESS_NOT_ASSOCIATED;
}

NTSTATUS
NdasPortFdoDispatchScsi(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp)
{
    PNDASPORT_FDO_EXTENSION FdoExtension = 
		(PNDASPORT_FDO_EXTENSION) DeviceObject->DeviceExtension;

    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PSCSI_REQUEST_BLOCK srb = irpStack->Parameters.Scsi.Srb;

	//
	// Should never get here. All SCSI requests are handled
	// by the PDO Dispatch routine
	//
	NdasPortTrace(NDASPORT_FDO_SCSI, TRACE_LEVEL_ERROR,
		"SRB function sent to FDO Dispatch: Fdo=%p, Irp=%p\n",
		DeviceObject, Irp);

    Irp->IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_INVALID_DEVICE_REQUEST;
}


NTSTATUS
NdasPortFdoDispatchCreateClose(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp)
{
	NTSTATUS status;
    PNDASPORT_COMMON_EXTENSION CommonExtension = 
		(PNDASPORT_COMMON_EXTENSION) DeviceObject->DeviceExtension;

	PIO_STACK_LOCATION irpStack;
	ULONG isRemoved;

	NdasPortTrace(NDASPORT_FDO_GENERAL, TRACE_LEVEL_INFORMATION,
		"NdasPortFdoDispatchCreateClose: Fdo=%p, Irp=%p\n",
		DeviceObject, Irp);

	status = STATUS_SUCCESS;
	irpStack = IoGetCurrentIrpStackLocation(Irp);

	isRemoved = NpAcquireRemoveLock(CommonExtension, Irp);

	if (IRP_MJ_CREATE == irpStack->MajorFunction)
	{
		if (isRemoved)
		{
			status = STATUS_DEVICE_DOES_NOT_EXIST;
		}
		else if (Started != CommonExtension->DevicePnPState)
		{
			status = STATUS_DEVICE_NOT_READY;
		}
	}

	return NpReleaseRemoveLockAndCompleteIrp(
		CommonExtension, 
		Irp, 
		status);
}


NTSTATUS
NpSetEvent(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp,
    __in PVOID Context)
{
    KeSetEvent((PKEVENT)Context, IO_NO_INCREMENT, FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}
