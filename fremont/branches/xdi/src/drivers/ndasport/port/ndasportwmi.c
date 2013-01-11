#include "port.h"
#include "ndasportwmi.h"

VOID
NdasPortWmiInitializeContext(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PNDASPORT_LOGICALUNIT_WMILIB_CONTEXT LogicalUnitWmiLibContext)
{
	PNDASPORT_PDO_EXTENSION PdoExtension;
	PdoExtension = NdasPortLogicalUnitGetPdoExtension(LogicalUnitExtension);

	PdoExtension->LogicalUnitWmiInfo = LogicalUnitWmiLibContext;
	RtlZeroMemory(&PdoExtension->WmiLibInfo, sizeof(WMILIB_CONTEXT));
	PdoExtension->WmiLibInfo.GuidCount = LogicalUnitWmiLibContext->GuidCount;
	PdoExtension->WmiLibInfo.GuidList = LogicalUnitWmiLibContext->GuidList;
	PdoExtension->WmiLibInfo.QueryWmiRegInfo = NdasPortWmipQueryRegInfo;
	PdoExtension->WmiLibInfo.QueryWmiDataBlock = NdasPortWmipQueryDataBlock;

	ASSERT(LogicalUnitWmiLibContext->QueryWmiDataBlock);
	ASSERT(LogicalUnitWmiLibContext->QueryWmiRegInfo);

	if (LogicalUnitWmiLibContext->SetWmiDataBlock)
	{
		PdoExtension->WmiLibInfo.SetWmiDataBlock = NdasPortWmipSetDataBlock;
	}
	if (LogicalUnitWmiLibContext->SetWmiDataItem)
	{
		PdoExtension->WmiLibInfo.SetWmiDataItem = NdasPortWmipSetDataItem;
	}
	if (LogicalUnitWmiLibContext->ExecuteWmiMethod)
	{
		PdoExtension->WmiLibInfo.ExecuteWmiMethod = NdasPortWmipExecuteMethod;
	}
	if (LogicalUnitWmiLibContext->WmiFunctionControl)
	{
		PdoExtension->WmiLibInfo.WmiFunctionControl = NdasPortWmipFunctionControl;
	}
}

NTSTATUS
NdasPortWmiCompleteRequest(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PIRP Irp,
	__in NTSTATUS Status,
	__in ULONG BufferUsed,
	__in CCHAR PriorityBoost)
{
	NTSTATUS status;
	PNDASPORT_PDO_EXTENSION PdoExtension;
	PDEVICE_OBJECT Pdo;
	PdoExtension = NdasPortLogicalUnitGetPdoExtension(LogicalUnitExtension);
	Pdo = PdoExtension->DeviceObject;
	NpReleaseRemoveLock(PdoExtension->CommonExtension, Irp);
	status = WmiCompleteRequest(
		Pdo,
		Irp,
		Status,
		BufferUsed,
		PriorityBoost);
	return status;
}

NTSTATUS
NdasPortWmiFireEvent(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in LPGUID Guid,
	__in ULONG InstanceIndex,
	__in ULONG EventDataSize,
	__in PVOID EventData)
{
	PNDASPORT_PDO_EXTENSION PdoExtension;
	PDEVICE_OBJECT Pdo;
	PdoExtension = NdasPortLogicalUnitGetPdoExtension(LogicalUnitExtension);
	Pdo = PdoExtension->DeviceObject;
	return WmiFireEvent(
		Pdo,
		Guid,
		InstanceIndex,
		EventDataSize,
		EventData);
}

NTSTATUS
NdasPortWmipQueryRegInfo(
	__in PDEVICE_OBJECT DeviceObject,
	__out PULONG RegFlags,
	__out PUNICODE_STRING InstanceName,
	__out PUNICODE_STRING *RegistryPath,
	__out PUNICODE_STRING MofResourceName,
	__out PDEVICE_OBJECT *Pdo)
{
	NTSTATUS status;
	PNDASPORT_PDO_EXTENSION PdoExtension;
	PNDASPORT_DRIVER_EXTENSION DriverExtension;

	PAGED_CODE();

	NdasPortTrace(NDASPORT_PDO_GENERAL, TRACE_LEVEL_WARNING, __FUNCTION__ "\n");

	PdoExtension = NdasPortPdoGetExtension(DeviceObject);
	DriverExtension = NdasPortDriverGetExtension(DeviceObject->DriverObject);

	*RegFlags = WMIREG_FLAG_INSTANCE_BASENAME;
	// -- InstanceName is ignored as WMIREG_FLAG_INSTANCE_BASENAME is not cleared

	InstanceName->Length = 0;
	InstanceName->MaximumLength = 60;
	InstanceName->Buffer = (PWCH) ExAllocatePoolWithTag(
		NonPagedPool, 
		60, 
		NDASPORT_TAG_WMI);
	if (NULL == InstanceName->Buffer)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	status = RtlUnicodeStringPrintf(
		InstanceName, 
		L"NdasLogicalUnit_%08X",
		PdoExtension->LogicalUnitAddress.Address);

	ASSERT(NT_SUCCESS(status));

	*RegistryPath = &DriverExtension->RegistryPath;
	// Pdo is ignored as WMIREG_FLAG_INSTANCE_PDO is cleared

	ASSERT(NULL != PdoExtension->LogicalUnitWmiInfo);

	status = (*PdoExtension->LogicalUnitWmiInfo->QueryWmiRegInfo)(
		NdasPortPdoGetLogicalUnitExtension(PdoExtension),
		RegistryPath,
		MofResourceName);

	ASSERT(STATUS_PENDING != status);

	if (!NT_SUCCESS(status))
	{
		ExFreePoolWithTag(InstanceName->Buffer, NDASPORT_TAG_WMI);
		InstanceName->Buffer = NULL;
		return status;
	}

	if (NULL == *RegistryPath)
	{
		*RegistryPath = &DriverExtension->RegistryPath;
	}

	return status;
}

NTSTATUS
NdasPortWmipQueryDataBlock(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp,
	__in ULONG GuidIndex,
	__in ULONG InstanceIndex,
	__in ULONG InstanceCount,
	__inout PULONG InstanceLengthArray,
	__in ULONG BufferAvail,
	__out PUCHAR Buffer)
{	
	PNDASPORT_PDO_EXTENSION PdoExtension = NdasPortPdoGetExtension(DeviceObject);
	ULONG isRemoved;
	isRemoved = NpAcquireRemoveLock(PdoExtension->CommonExtension, Irp);
	ASSERT(!isRemoved);
	ASSERT(NULL != PdoExtension->LogicalUnitWmiInfo->QueryWmiDataBlock);
	return (*PdoExtension->LogicalUnitWmiInfo->QueryWmiDataBlock)(
		NdasPortPdoGetLogicalUnitExtension(PdoExtension),
		Irp,
		GuidIndex,
		InstanceIndex,
		InstanceCount,
		InstanceLengthArray,
		BufferAvail,
		Buffer);
}

NTSTATUS
NdasPortWmipSetDataBlock(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp,
	__in ULONG GuidIndex,
	__in ULONG InstanceIndex,
	__in ULONG BufferSize,
	__in PUCHAR Buffer)
{
	PNDASPORT_PDO_EXTENSION PdoExtension = NdasPortPdoGetExtension(DeviceObject);
	ULONG isRemoved;
	isRemoved = NpAcquireRemoveLock(PdoExtension->CommonExtension, Irp);
	ASSERT(!isRemoved);
	ASSERT(NULL != PdoExtension->LogicalUnitWmiInfo->SetWmiDataBlock);
	return (*PdoExtension->LogicalUnitWmiInfo->SetWmiDataBlock)(
		NdasPortPdoGetLogicalUnitExtension(PdoExtension),
		Irp,
		GuidIndex,
		InstanceIndex,
		BufferSize,
		Buffer);
}

NTSTATUS
NdasPortWmipSetDataItem(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp,
	__in ULONG GuidIndex,
	__in ULONG InstanceIndex,
	__in ULONG DataItemId,
	__in ULONG BufferSize,
	__in PUCHAR Buffer)
{
	PNDASPORT_PDO_EXTENSION PdoExtension = NdasPortPdoGetExtension(DeviceObject);
	ULONG isRemoved;
	isRemoved = NpAcquireRemoveLock(PdoExtension->CommonExtension, Irp);
	ASSERT(!isRemoved);
	ASSERT(NULL != PdoExtension->LogicalUnitWmiInfo->SetWmiDataItem);
	return (*PdoExtension->LogicalUnitWmiInfo->SetWmiDataItem)(
		NdasPortPdoGetLogicalUnitExtension(PdoExtension),
		Irp,
		GuidIndex,
		InstanceIndex,
		DataItemId,
		BufferSize,
		Buffer);
}

NTSTATUS
NdasPortWmipExecuteMethod(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp,
	__in ULONG GuidIndex,
	__in ULONG InstanceIndex,
	__in ULONG MethodId,
	__in ULONG InBufferSize,
	__in ULONG OutBufferSize,
	__inout PUCHAR Buffer)
{
	PNDASPORT_PDO_EXTENSION PdoExtension = NdasPortPdoGetExtension(DeviceObject);
	ULONG isRemoved;
	isRemoved = NpAcquireRemoveLock(PdoExtension->CommonExtension, Irp);
	ASSERT(!isRemoved);
	ASSERT(NULL != PdoExtension->LogicalUnitWmiInfo->ExecuteWmiMethod);
	return (*PdoExtension->LogicalUnitWmiInfo->ExecuteWmiMethod)(
		NdasPortPdoGetLogicalUnitExtension(PdoExtension),
		Irp,
		GuidIndex,
		InstanceIndex,
		MethodId,
		InBufferSize,
		OutBufferSize,
		Buffer);
}

NTSTATUS
NdasPortWmipFunctionControl(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp,
	__in ULONG GuidIndex,
	__in WMIENABLEDISABLECONTROL Function,
	__in BOOLEAN Enable)
{
	PNDASPORT_PDO_EXTENSION PdoExtension = NdasPortPdoGetExtension(DeviceObject);
	ULONG isRemoved;
	isRemoved = NpAcquireRemoveLock(PdoExtension->CommonExtension, Irp);
	ASSERT(!isRemoved);
	ASSERT(NULL != PdoExtension->LogicalUnitWmiInfo->WmiFunctionControl);
	return (*PdoExtension->LogicalUnitWmiInfo->WmiFunctionControl)(
		NdasPortPdoGetLogicalUnitExtension(PdoExtension),
		Irp,
		GuidIndex,
		Function,
		Enable);
}

#ifdef NDASPORT_IMP_SCSI_WMI

BOOLEAN
NdasPortWmiDispatchFunction(
    __in PNDASPORT_WMILIB_CONTEXT WmiLibInfo,
    __in UCHAR  MinorFunction,
    __in PVOID  DeviceContext,
    __in PNDASPORT_WMI_REQUEST_CONTEXT  RequestContext,
    __in PVOID  DataPath,
    __in ULONG  BufferSize,
    __in PVOID  Buffer)
{
	switch (MinorFunction)
	{
	case IRP_MN_REGINFO:
		{
			WmiLibInfo->GuidList;
			WmiLibInfo->GuidCount;
		}
	// case IRP_MN_REGINFO_EX:
	case IRP_MN_QUERY_ALL_DATA:
		{
			
		}
	case IRP_MN_QUERY_SINGLE_INSTANCE:
	case IRP_MN_CHANGE_SINGLE_INSTANCE:
	case IRP_MN_CHANGE_SINGLE_ITEM:
	case IRP_MN_ENABLE_EVENTS:
	case IRP_MN_DISABLE_EVENTS:
	case IRP_MN_ENABLE_COLLECTION:
	case IRP_MN_DISABLE_COLLECTION:
	case IRP_MN_EXECUTE_METHOD:
	default:
		;
	}
	WmiLibInfo->QueryWmiRegInfo;
	return TRUE;
}

VOID
NdasPortWmiFireAdapterEvent(
  __in PVOID  HwDeviceExtension,
  __in LPGUID  Guid,
  __in ULONG  InstanceIndex,
  __in ULONG  EventDataSize,
  __in PVOID  EventData);

VOID
NdasPortWmiFireLogicalUnitEvent(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
    __in UCHAR  PathId,
    __in UCHAR  TargetId,
    __in UCHAR  Lun,
    __in LPGUID  Guid,
    __in ULONG  InstanceIndex,
    __in ULONG  EventDataSize,
    __in PVOID  EventData)
{
	NTSTATUS status;
	PNDASPORT_PDO_EXTENSION PdoExtension;

	UNREFERENCED_PARAMETER(PathId);
	UNREFERENCED_PARAMETER(TargetId);
	UNREFERENCED_PARAMETER(Lun);

	PdoExtension = NdasPortLogicalUnitGetPdoExtension(LogicalUnitExtension);

	status = WmiFireEvent(
		PdoExtension->DeviceObject,
		Guid,
		InstanceIndex,
		EventDataSize,
		EventData);
}

PWCHAR 
NdasPortWmiGetInstanceName (
  __in PNDASPORT_WMI_REQUEST_CONTEXT  RequestContext)
{
	return NULL;
}

ULONG 
NdasPortWmiGetReturnSize(
	__in PNDASPORT_WMI_REQUEST_CONTEXT  RequestContext)
{
	return RequestContext->ReturnSize;
}

UCHAR 
NdasPortWmiGetReturnStatus(
    __in PNDASPORT_WMI_REQUEST_CONTEXT  RequestContext)
{
	return RequestContext->ReturnStatus;
}

VOID
NdasPortWmiPostProcess(
    __in PNDASPORT_WMI_REQUEST_CONTEXT  RequestContext,
    __in UCHAR  SrbStatus,
    __in ULONG  BufferUsed)
{
	return;
}

PVOID 
NdasPortWmiSetData(
  __in PNDASPORT_WMI_REQUEST_CONTEXT  RequestContext,
  __in ULONG  InstanceIndex,
  __in ULONG  DataLength,
  __out PULONG  BufferAvail,
  __inout PULONG  SizeNeeded)
{

}

BOOLEAN 
NdasPortWmiSetInstanceCount(
  __in PNDASPORT_WMI_REQUEST_CONTEXT  RequestContext,
  __in ULONG  InstanceCount,
  __out PULONG  BufferAvail,
  __out PULONG  SizeNeeded)
{

}

PWCHAR 
NdasPortWmiSetInstanceName (
  __in PNDASPORT_WMI_REQUEST_CONTEXT  RequestContext,
  __in ULONG  InstanceIndex,
  __in ULONG  InstanceNameLength,
  __out PULONG  BufferAvail,
  __inout PULONG  SizeNeeded)
{

}

#endif /* NDASPORT_IMP_SCSI_WMI */
