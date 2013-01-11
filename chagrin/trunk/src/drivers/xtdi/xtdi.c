#if _WIN32_WINNT <= 0x0500
#define NTSTRSAFE_LIB
#endif

#include <ntddk.h>
#include <ntintsafe.h>
#include "xtdi.h"

#include "xtdidebug.h"
#ifdef RUN_WPP
#include "xtdi.tmh"
#endif

#define XTDI_POOL_TAG 'idTx'
#define XTDI_INTERNAL_MDL 0x00000001
#define XTDI_INTERNAL_IRP 0x00000002
#define XTDI_INTERNAL_THREADED_IRP 0x00000004

#if _WIN32_WINNT < 0x0501
#define ExFreePoolWithTag(POINTER, TAG) ExFreePool(POINTER)
#endif

#define FILE_FULL_EA_INFO_CEP_LENGTH  \
	FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) \
	+ TDI_CONNECTION_CONTEXT_LENGTH + 1 \
	+ sizeof(CONNECTION_CONTEXT)

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, xTdiCreateAddressObject)
#pragma alloc_text(PAGE, xTdiCloseAddressObject)
#pragma alloc_text(PAGE, xTdiCreateConnectionObject)
#pragma alloc_text(PAGE, xTdiCloseConnectionObject)
#pragma alloc_text(PAGE, xTdiAssociateAddress)
#pragma alloc_text(PAGE, xTdiDisassociateAddress)
#pragma alloc_text(PAGE, xTdiConnect)
#pragma alloc_text(PAGE, xTdiDisconnect)
#pragma alloc_text(PAGE, xTdiQueryInformation)
#pragma alloc_text(PAGE, xTdiSetEventHandler)
#pragma alloc_text(PAGE, xTdiConnect)
#endif // ALLOC_PRAGMA

//
// Actually, we cannot use Threaded IRP anymore as
// xTdi routines are called at IRQL <= DISPATCH_LEVEL.
// But TdiBuildInternalDeviceControlIrp should be called at IRQL == PASSIVE_LEVEL.
//
// This codes are retained for demonstration purpose.
//
#ifdef XTDI_USE_THREADED_IRP

static
FORCEINLINE
PIRP
xTdiBuildInternalIrp(
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PKEVENT Event,
	__in PIO_STATUS_BLOCK IoStatus)
{
	return TdiBuildInternalDeviceControlIrp(
		TDI_CONNECT, /* anything will be fine */
		ConnectionDeviceObject,
		NULL, /* ConnectionFileObject is not used */
		Event,
		IoStatus);
}

static
FORCEINLINE
VOID
xTdiFreeInternalIrp(
	__in PIRP Irp)
{
}

#else

static
FORCEINLINE
PIRP
xTdiBuildInternalIrp(
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PKEVENT Event,
	__in PIO_STATUS_BLOCK IoStatus)
{
	PIRP irp = IoAllocateIrp(ConnectionDeviceObject->StackSize + 1, FALSE);
	irp->UserEvent = Event;
	irp->UserIosb = IoStatus;
	return irp;
}

static
FORCEINLINE
VOID
xTdiFreeInternalIrp(
	__in PIRP Irp)
{
	IoFreeIrp(Irp);
}

#endif

static
FORCEINLINE
NTSTATUS
xTdiAllocateInternalMdl(
	__in PVOID Buffer,
	__in ULONG BufferLen,
	__out PMDL* Mdl)
{
	PMDL localMdl;

	ASSERT(NULL != Mdl);

	localMdl = IoAllocateMdl(
		Buffer,
		BufferLen,
		FALSE,
		FALSE,
		NULL);

	if (NULL == localMdl)
	{
		NTSTATUS status = STATUS_INSUFFICIENT_RESOURCES;
		xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_ERROR,
			"xTdiAllocateInternalMdl failed to allocate MDL, status=%x\n",
			status);
		return status;
	}

	MmBuildMdlForNonPagedPool(localMdl);

	xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_VERBOSE,
		"Allocated Buffer=%p, Length=0x%X, Mdl=%p\n",
		Buffer, BufferLen, localMdl);

	*Mdl = localMdl;

	return STATUS_SUCCESS;
}

static
FORCEINLINE
VOID
xTdiFreeInternalMdl(
	__in PMDL Mdl)
{
	ASSERT(NULL != Mdl);
	IoFreeMdl(Mdl);
}

static
NTSTATUS
xTdiOverlappedIoCompletionRoutine(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp,
	__in PVOID Context)
{
	PXTDI_OVERLAPPED overlapped;

	ASSERT(NULL != Irp);
	ASSERT(NULL != Context);

	overlapped = (PXTDI_OVERLAPPED) Context;

	//
	// Copy user status block
	//
	RtlCopyMemory(
		&overlapped->IoStatus,
		&Irp->IoStatus,
		sizeof(IO_STATUS_BLOCK));

	xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_VERBOSE,
		"xTdiOverlappedIoCompletionRoutine completed status %x: Context=%p\n",
		overlapped->IoStatus.Status, overlapped->UserContext);


	if (XTDI_INTERNAL_MDL & overlapped->Internal)
	{
		if (NULL != Irp->MdlAddress)
		{
			xTdiFreeInternalMdl(Irp->MdlAddress);
			Irp->MdlAddress = NULL;
		}
		overlapped->Internal &= ~(XTDI_INTERNAL_MDL);
	}

	if (XTDI_INTERNAL_IRP & overlapped->Internal)
	{
		xTdiFreeInternalIrp(Irp);
		overlapped->Internal &= ~(XTDI_INTERNAL_IRP);
	}

	if (NULL != overlapped->InternalBuffer)
	{
		ExFreePoolWithTag(
			overlapped->InternalBuffer,
			XTDI_POOL_TAG);
		overlapped->InternalBuffer = NULL;
	}

	if (overlapped->Event)
	{
		KeSetEvent(overlapped->Event, IO_NETWORK_INCREMENT, FALSE);
	}

	//
	// Call the Overlapped IO Completion Routine
	//
	if (overlapped->CompletionRoutine)
	{
		overlapped->CompletionRoutine(Irp, overlapped);
	}

#ifdef XTDI_USE_THREADED_IRP
	return STATUS_SUCCESS;
#else
	return STATUS_MORE_PROCESSING_REQUIRED;
#endif
}

NTSTATUS
xTdiCreateAddressObject(
	__in PCWSTR DeviceName,
	__in PFILE_FULL_EA_INFORMATION ExtendedAttributeBuffer,
	__in ULONG ExtendedAttributeBufferLength,
	__out HANDLE *AddrHandle,
	__out PFILE_OBJECT *AddrFileObject,
	__out PDEVICE_OBJECT *AddrDeviceObject)
{
	TA_IP_ADDRESS IPAddress;
	PFILE_OBJECT FileObject;
	PDEVICE_OBJECT DeviceObject;
	PVOID Handle;
	PFILE_FULL_EA_INFORMATION eaBuffer;
	UNICODE_STRING deviceName;
	OBJECT_ATTRIBUTES objectAttributes;
	IO_STATUS_BLOCK ioStatus = {0};
	NTSTATUS status;

	PAGED_CODE();

	RtlInitUnicodeString(&deviceName, DeviceName);
	InitializeObjectAttributes(
		&objectAttributes,
		&deviceName,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	status = ZwCreateFile(
		&Handle,
		(GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE),
		&objectAttributes,
		&ioStatus,
		0,
		0,
		(FILE_SHARE_READ | FILE_SHARE_WRITE),
		FILE_CREATE,
		0,
		ExtendedAttributeBuffer,
		ExtendedAttributeBufferLength);

	if (!NT_SUCCESS(status))
	{
		xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_ERROR,
			"CreateTdiAddress failed. Status=%x\n", status);
		return status;
	}

	//
	// Get the associated file object
	//
	status = ObReferenceObjectByHandle(
		Handle,
		0,
		NULL,
		KernelMode,
		(PVOID*)(&FileObject),
		NULL);

	if (!NT_SUCCESS(status))
	{
		xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_ERROR,
			"ObReferenceObjectByHandle failed. Status=%x\n", status);
		ZwClose(Handle);
		return status;
	}

	//
	// Get the associated device object
	//
	DeviceObject = IoGetRelatedDeviceObject(FileObject);

	*AddrHandle = Handle;
	*AddrFileObject = FileObject;
	*AddrDeviceObject = DeviceObject;

	return STATUS_SUCCESS;
}

NTSTATUS
xTdiCloseAddressObject(
	__in HANDLE AddrHandle,
	__in PFILE_OBJECT AddrFileObject)
{
	PAGED_CODE();

	if (AddrFileObject != NULL)
	{
		ObDereferenceObject(AddrFileObject);
	}

	if (AddrHandle != NULL)
	{
		ZwClose(AddrHandle);
	}

	return STATUS_SUCCESS;
}

NTSTATUS
xTdiCreateConnectionObject(
	__in PCWSTR DeviceName,
	__in CONNECTION_CONTEXT ConnectionContext,
	__out HANDLE *ConnectionHandle,
	__out PFILE_OBJECT *ConnectionFileObject,
	__out PDEVICE_OBJECT *ConnectionDeviceObject)
{
	PFILE_OBJECT FileObject;
	PDEVICE_OBJECT DeviceObject;
	PVOID Handle;
	FILE_FULL_EA_INFORMATION UNALIGNED* eaBuffer;
	CONNECTION_CONTEXT UNALIGNED *eaCEPContext;
	UNICODE_STRING deviceName;
	OBJECT_ATTRIBUTES objectAttributes;
	IO_STATUS_BLOCK ioStatus = {0};
	NTSTATUS status;
	UCHAR eaBufferBuffer[FILE_FULL_EA_INFO_CEP_LENGTH];

	PAGED_CODE();

	RtlInitUnicodeString(&deviceName, DeviceName);
	InitializeObjectAttributes(
		&objectAttributes,
		&deviceName,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		NULL,
		NULL);

	eaBuffer = (FILE_FULL_EA_INFORMATION UNALIGNED *)eaBufferBuffer;

	eaBuffer->NextEntryOffset = 0;
	eaBuffer->Flags = 0;
	eaBuffer->EaNameLength = TDI_CONNECTION_CONTEXT_LENGTH;
	eaBuffer->EaValueLength = sizeof(CONNECTION_CONTEXT);

	//
	// Set EaName
	//
	RtlMoveMemory(
		eaBuffer->EaName,
		TdiConnectionContext,
		TDI_CONNECTION_CONTEXT_LENGTH + 1);

	//
	// Set EaValue
	//
	eaCEPContext = (CONNECTION_CONTEXT UNALIGNED *)
		&(eaBuffer->EaName[TDI_CONNECTION_CONTEXT_LENGTH + 1]);
	*eaCEPContext = (CONNECTION_CONTEXT)ConnectionContext;

	status = ZwCreateFile(
		&Handle,
		(GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE),
		&objectAttributes,
		&ioStatus,
		0,
		0,
		0,
		FILE_CREATE,
		0,
		eaBuffer,
		FILE_FULL_EA_INFO_CEP_LENGTH);

	if (!NT_SUCCESS(status))
	{
		xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_ERROR,
			"CreateTdiConnection failed. Status %x\n",status);
		return status;
	}

	//
	// Get the associated file object
	//
	status = ObReferenceObjectByHandle(
		Handle,
		0,
		NULL,
		KernelMode,
		(PVOID*)(&FileObject),
		NULL);

	if (!NT_SUCCESS(status))
	{
		xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_ERROR,
			"ObReferenceObjectByHandle failed. Status %x\n", status);

		ZwClose(Handle);

		return status;
	}

	//
	// Get the associated device object
	//
	DeviceObject = IoGetRelatedDeviceObject(FileObject);

	*ConnectionHandle = Handle;
	*ConnectionFileObject = FileObject;
	*ConnectionDeviceObject = DeviceObject;

	return STATUS_SUCCESS;
}

NTSTATUS
xTdiCloseConnectionObject(
	__in HANDLE ConnectionHandle,
	__in PFILE_OBJECT ConnectionFileObject)
{
	PAGED_CODE();

	if (ConnectionFileObject != NULL)
	{
		ObDereferenceObject(ConnectionFileObject);
	}

	if (ConnectionHandle != NULL)
	{
		ZwClose(ConnectionHandle);
	}

	return STATUS_SUCCESS;
}

NTSTATUS
xTdiAssociateAddressEx(
	__in PIRP Irp,
	__in HANDLE AddressHandle,
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PXTDI_OVERLAPPED Overlapped)
{
	NTSTATUS status;

	ASSERT(NULL != Irp);
	ASSERT(NULL != AddressHandle);
	ASSERT(NULL != ConnectionDeviceObject);
	ASSERT(NULL != ConnectionFileObject);
	ASSERT(NULL != Overlapped);

	TdiBuildAssociateAddress(
		Irp,
		ConnectionDeviceObject,
		ConnectionFileObject,
		xTdiOverlappedIoCompletionRoutine,
		Overlapped,
		AddressHandle);

	status = IoCallDriver(ConnectionDeviceObject, Irp);

	return status;
}

NTSTATUS
xTdiDisassociateAddressEx(
	__in PIRP Irp,
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PXTDI_OVERLAPPED Overlapped)
{
	NTSTATUS status;

	ASSERT(NULL != Irp);
	ASSERT(NULL != ConnectionDeviceObject);
	ASSERT(NULL != ConnectionFileObject);
	ASSERT(NULL != Overlapped);

	TdiBuildDisassociateAddress(
		Irp,
		ConnectionDeviceObject,
		ConnectionFileObject,
		xTdiOverlappedIoCompletionRoutine,
		Overlapped);

	status = IoCallDriver(ConnectionDeviceObject, Irp);

	return status;
}

NTSTATUS
xTdiConnectEx(
	__in PIRP Irp,
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in_bcount(RemoteAddressLength) PTRANSPORT_ADDRESS RemoteAddress,
	__in LONG RemoteAddressLength,
	__in_opt PLARGE_INTEGER ConnectionTimeout,
	__in PXTDI_OVERLAPPED Overlapped)
{
	NTSTATUS status;
	PTDI_CONNECTION_INFORMATION  requestConnectionInformation;

	ASSERT(NULL != Irp);
	ASSERT(NULL != ConnectionDeviceObject);
	ASSERT(NULL != ConnectionFileObject);
	ASSERT(NULL != Overlapped);
	ASSERT(NULL == Overlapped->InternalBuffer);

	Overlapped->InternalBuffer = 
		ExAllocatePoolWithTag(
			NonPagedPool,
			sizeof(TDI_CONNECTION_INFORMATION),
			XTDI_POOL_TAG);

	if (NULL == Overlapped->InternalBuffer)
	{
		Overlapped->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		Overlapped->IoStatus.Information = 0;
		if (Overlapped->CompletionRoutine)
		{
			(*Overlapped->CompletionRoutine)(Irp, Overlapped);
		}
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	requestConnectionInformation = (PTDI_CONNECTION_INFORMATION) 
		Overlapped->InternalBuffer;

	RtlZeroMemory(
		requestConnectionInformation,
		sizeof(TDI_CONNECTION_INFORMATION));

	requestConnectionInformation->RemoteAddressLength = RemoteAddressLength;
	requestConnectionInformation->RemoteAddress = RemoteAddress;

	TdiBuildConnect(
		Irp,
		ConnectionDeviceObject,
		ConnectionFileObject,
		xTdiOverlappedIoCompletionRoutine,
		Overlapped,
		ConnectionTimeout,
		requestConnectionInformation,
		NULL);

	status = IoCallDriver(
		ConnectionDeviceObject, 
		Irp);

	xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_VERBOSE,
		"nspTdiConnectEx returned: status %x\n", status);

	return status;
}

NTSTATUS
xTdiDisconnectEx(
	__in PIRP Irp,
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in ULONG DisconnectFlags,
	__in_opt PLARGE_INTEGER  DisconnectTimeout,
	__in_opt PTDI_CONNECTION_INFORMATION RequestConnectionInfo,
	__out_opt PTDI_CONNECTION_INFORMATION ReturnConnectionInfo,
	__in PXTDI_OVERLAPPED Overlapped)
{
	NTSTATUS status;

	ASSERT(NULL != Irp);
	ASSERT(NULL != ConnectionDeviceObject);
	ASSERT(NULL != ConnectionFileObject);
	ASSERT(NULL != Overlapped);

	xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_VERBOSE,
		"xTdiDisconnectEx Irp %p\n", Irp);

	//
	// Build an IRP
	//
	TdiBuildDisconnect(
		Irp,
		ConnectionDeviceObject,
		ConnectionFileObject,
		xTdiOverlappedIoCompletionRoutine,
		Overlapped,
		DisconnectTimeout,
		DisconnectFlags,
		RequestConnectionInfo,
		ReturnConnectionInfo);

	//
	// Send an IRP
	//
	status = IoCallDriver(ConnectionDeviceObject, Irp);

	xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_VERBOSE,
		"xTdiDisconnectEx returns status %x\n", status);

	return status;
}

NTSTATUS
xTdiQueryInformationEx(
	__in PIRP Irp,
	__in PDEVICE_OBJECT TdiDeviceObject,
	__in PFILE_OBJECT TdiFileObject,
	__in ULONG QueryType,
	__out_bcount(BufferLength) PVOID Buffer,
	__in ULONG BufferLength,
	__in PXTDI_OVERLAPPED Overlapped)
{
	NTSTATUS status;
	PMDL localMdl;

	ASSERT(NULL != Irp);
	ASSERT(NULL != TdiDeviceObject);
	ASSERT(NULL != TdiFileObject);
	ASSERT(NULL != Overlapped);

	status = xTdiAllocateInternalMdl(
		Buffer,
		BufferLength,
		&localMdl);

	if (!NT_SUCCESS(status))
	{
		xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_ERROR, 
			"xTdiAllocateInternalMdl failed, status=%x\n", status);
		Overlapped->IoStatus.Status = status;
		Overlapped->IoStatus.Information = 0;
		if (Overlapped->CompletionRoutine)
		{
			(*Overlapped->CompletionRoutine)(Irp, Overlapped);
		}
		return status;
	}

	ASSERT(0 == (Overlapped->Internal & XTDI_INTERNAL_MDL));
	Overlapped->Internal |= XTDI_INTERNAL_MDL;

	TdiBuildQueryInformation(
		Irp,
		TdiDeviceObject,
		TdiFileObject,
		xTdiOverlappedIoCompletionRoutine,
		Overlapped,
		QueryType,
		localMdl);

	status = IoCallDriver(TdiDeviceObject, Irp);

	xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_VERBOSE,
		"xTdiQueryInformationEx returned status=%x\n", status);

	return status;

}

NTSTATUS
xTdiAssociateAddress(
	__in HANDLE AddressHandle,
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject)
{
	NTSTATUS status;
	PIRP localIrp;
	KEVENT event;
	XTDI_OVERLAPPED overlapped = {0};

	PAGED_CODE();

	KeInitializeEvent(&event, NotificationEvent, FALSE);
	overlapped.Event = &event;

	localIrp = xTdiBuildInternalIrp(
		ConnectionDeviceObject,
		ConnectionFileObject,
		&event,
		&overlapped.IoStatus);

	if (NULL == localIrp)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	status = xTdiAssociateAddressEx(
		localIrp,
		AddressHandle,
		ConnectionDeviceObject,
		ConnectionFileObject,
		&overlapped);

	if (STATUS_PENDING == status)
	{
		status = KeWaitForSingleObject(
			&event,
			Executive,
			KernelMode,
			FALSE,
			NULL);

		ASSERT(STATUS_SUCCESS == status);

		status = overlapped.IoStatus.Status;
	}

	xTdiFreeInternalIrp(localIrp);

	return status;
}

NTSTATUS
xTdiDisassociateAddress(
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject)
{
	NTSTATUS status;
	PIRP localIrp;
	KEVENT event;
	XTDI_OVERLAPPED overlapped = {0};
	
	KeInitializeEvent(&event, NotificationEvent, FALSE);
	overlapped.Event = &event;

	localIrp = xTdiBuildInternalIrp(
		ConnectionDeviceObject,
		ConnectionFileObject,
		&event,
		&overlapped.IoStatus);

	if (NULL == localIrp)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	status = xTdiDisassociateAddressEx(
		localIrp,
		ConnectionDeviceObject,
		ConnectionFileObject,
		&overlapped);

	if (STATUS_PENDING == status)
	{
		status = KeWaitForSingleObject(
			&event,
			Executive,
			KernelMode,
			FALSE,
			NULL);

		ASSERT(STATUS_SUCCESS == status);

		status = overlapped.IoStatus.Status;
	}

	xTdiFreeInternalIrp(localIrp);

	return status;
}

NTSTATUS
xTdiConnect(
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PTRANSPORT_ADDRESS RemoteAddress,
	__in LONG RemoteAddressLength,
	__in_opt PLARGE_INTEGER ConnectionTimeout)
{
	NTSTATUS status;
	PIRP localIrp;
	KEVENT event;
	XTDI_OVERLAPPED overlapped = {0};

	PAGED_CODE();

	KeInitializeEvent(&event, NotificationEvent, FALSE);
	overlapped.Event = &event;

	localIrp = xTdiBuildInternalIrp(
		ConnectionDeviceObject,
		ConnectionFileObject,
		&event,
		&overlapped.IoStatus);

	if (NULL == localIrp)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_INFORMATION,
		"xTdiConnect: ConnectionDeviceObject=%p, localIrp=%p\n",
		ConnectionDeviceObject, localIrp);

	status = xTdiConnectEx(
		localIrp,
		ConnectionDeviceObject,
		ConnectionFileObject,
		RemoteAddress,
		RemoteAddressLength,
		ConnectionTimeout,
		&overlapped);

	if (STATUS_PENDING == status)
	{
		status = KeWaitForSingleObject(
			&event,
			Executive,
			KernelMode,
			FALSE,
			NULL);

		ASSERT(STATUS_SUCCESS == status);
		status = overlapped.IoStatus.Status;
	}

	xTdiFreeInternalIrp(localIrp);

	return status;
}

NTSTATUS
xTdiDisconnect(
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in ULONG DisconnectFlags,
	__in_opt PLARGE_INTEGER DisconnectTimeout,
	__in_opt PTDI_CONNECTION_INFORMATION RequestConnectionInfo,
	__out_opt PTDI_CONNECTION_INFORMATION ReturnConnectionInfo)
{
	NTSTATUS status;
	PIRP localIrp;
	KEVENT event;
	XTDI_OVERLAPPED overlapped = {0};

	PAGED_CODE();

	KeInitializeEvent(&event, NotificationEvent, FALSE);
	overlapped.Event = &event;

	localIrp = xTdiBuildInternalIrp(
		ConnectionDeviceObject,
		ConnectionFileObject,
		&event,
		&overlapped.IoStatus);

	if (NULL == localIrp)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	status = xTdiDisconnectEx(
		localIrp,
		ConnectionDeviceObject,
		ConnectionFileObject,
		DisconnectFlags,
		DisconnectTimeout,
		RequestConnectionInfo,
		ReturnConnectionInfo,
		&overlapped);

	if (STATUS_PENDING == status)
	{
		status = KeWaitForSingleObject(
			&event,
			Executive,
			KernelMode,
			FALSE,
			NULL);

		ASSERT(STATUS_SUCCESS == status);
		status = overlapped.IoStatus.Status;
	}

	xTdiFreeInternalIrp(localIrp);

	return status;
}

NTSTATUS
xTdiQueryInformation(
	__in PDEVICE_OBJECT TdiDeviceObject,
	__in PFILE_OBJECT TdiFileObject,
	__in ULONG QueryType,
	__out_bcount(BufferLength) PVOID Buffer,
	__in ULONG BufferLength)
{
	NTSTATUS status;
	PIRP localIrp;
	KEVENT event;
	XTDI_OVERLAPPED overlapped = {0};

	PAGED_CODE();

	KeInitializeEvent(&event, NotificationEvent, FALSE);
	overlapped.Event = &event;

	localIrp = xTdiBuildInternalIrp(
		TdiDeviceObject,
		TdiFileObject,
		&event,
		&overlapped.IoStatus);

	if (NULL == localIrp)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	status = xTdiQueryInformationEx(
		localIrp,
		TdiDeviceObject,
		TdiFileObject,
		QueryType,
		Buffer,
		BufferLength,
		&overlapped);

	if (STATUS_PENDING == status)
	{
		status = KeWaitForSingleObject(
			&event,
			Executive,
			KernelMode,
			FALSE,
			NULL);

		ASSERT(STATUS_SUCCESS == status);
		status = overlapped.IoStatus.Status;
	}

	xTdiFreeInternalIrp(localIrp);

	return status;
}


NTSTATUS
xTdiSetEventHandler(
	__in PDEVICE_OBJECT DeviceObject,
	__in PFILE_OBJECT FileObject,
	__in PXTDI_EVENT_HANDLER EventsToSet,
	__in ULONG CountOfEvents,
	__in PVOID EventContext)
{
	NTSTATUS status;
	ULONG i;
	PIRP localIrp;
	KEVENT event;
	XTDI_OVERLAPPED overlapped = {0};

	PAGED_CODE();

	KeInitializeEvent(&event, NotificationEvent, FALSE);
	overlapped.Event = &event;

	localIrp = xTdiBuildInternalIrp(
		DeviceObject,
		FileObject,
		&event,
		&overlapped.IoStatus);

	status = STATUS_SUCCESS;

	for (i = 0; i < CountOfEvents; ++i)
	{
		TdiBuildSetEventHandler(
			localIrp,
			DeviceObject,
			FileObject,
			xTdiOverlappedIoCompletionRoutine,
			&overlapped,
			EventsToSet[i].EventId,
			EventsToSet[i].EventHandler,
			EventContext);

		status = IoCallDriver(DeviceObject, localIrp);

		if (STATUS_PENDING == status)
		{
			status = KeWaitForSingleObject(
				&event,
				Executive,
				KernelMode,
				FALSE,
				NULL);

			ASSERT(STATUS_SUCCESS == status);
			status = overlapped.IoStatus.Status;
		}

		KeClearEvent(&event);

		if (!NT_SUCCESS(status))
		{
			xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_ERROR, 
				"TdiSetEventHandler failed, status=%x\n", status);
			break;
		}
	}

	if (!NT_SUCCESS(status))
	{
		NTSTATUS cleanupStatus;
		ULONG tmp;
		for (tmp = 0; tmp < i; ++tmp)
		{
			TdiBuildSetEventHandler(
				localIrp,
				DeviceObject,
				FileObject,
				xTdiOverlappedIoCompletionRoutine,
				&overlapped,
				EventsToSet[tmp].EventId,
				NULL,
				NULL);

			cleanupStatus = IoCallDriver(DeviceObject, localIrp);

			if (STATUS_PENDING == cleanupStatus)
			{
				status = KeWaitForSingleObject(
					&event,
					Executive,
					KernelMode,
					FALSE,
					NULL);

				ASSERT(STATUS_SUCCESS == cleanupStatus);
				cleanupStatus = overlapped.IoStatus.Status;
			}

			KeClearEvent(&event);

			if (!NT_SUCCESS(cleanupStatus))
			{
				xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_ERROR, 
					"TdiSetEventHandler cleanup failed, status=%x\n", cleanupStatus);
			}
		}
	}

	xTdiFreeInternalIrp(localIrp);

	xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_VERBOSE,
		"xTdiSetEventHandler returned status=%x\n", status);

	return status;
}

//
// The xTdiSendMdlEx function sends Mdl asynchronously.
// It is designed solely for asynchronous operation.
//
NTSTATUS
xTdiSendMdlEx(
	__in PIRP Irp,
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PMDL DataMdl,
	__in ULONG BytesToSend,
	__out PULONG BytesSent,
	__in ULONG InFlags,
	__in PXTDI_OVERLAPPED Overlapped)
{
	NTSTATUS status;

	ASSERT(NULL != Irp);
	ASSERT(NULL != BytesSent);

	*BytesSent = 0;

	xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_VERBOSE,
		"xTdiSendMdlEx: Irp=%p, Mdl=%p\n", Irp, DataMdl);

	TdiBuildSend(
		Irp,
		ConnectionDeviceObject,
		ConnectionFileObject,
		xTdiOverlappedIoCompletionRoutine,
		Overlapped,
		DataMdl,
		InFlags,
		BytesToSend);

	status = IoCallDriver(ConnectionDeviceObject, Irp);

	xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_VERBOSE,
		"xTdiSendMdlEx returned status=%x\n", status);

	if (STATUS_PENDING != status)
	{
		NTSTATUS miscStatus;
		miscStatus = RtlULongPtrToULong(Irp->IoStatus.Information, BytesSent);
		ASSERT(NT_SUCCESS(miscStatus));
	}

	return status;
}

//
// The xTdiSendMdlEx function sends Mdl asynchronously.
// It is designed solely for asynchronous operation.
//
NTSTATUS
xTdiReceiveMdlEx(
	__in PIRP Irp,
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PMDL DataMdl,
	__in ULONG BytesToReceive,
	__out PULONG BytesReceived,
	__in ULONG InFlags,
	__in PXTDI_OVERLAPPED Overlapped)
{
	NTSTATUS status;

	ASSERT(NULL != Irp);
	ASSERT(NULL != BytesReceived);

	*BytesReceived = 0;

	xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_VERBOSE,
		"xTdiReceiveMdlEx: Irp=%p, Mdl=%p\n", Irp, DataMdl);

	TdiBuildReceive(
		Irp,
		ConnectionDeviceObject,
		ConnectionFileObject,
		xTdiOverlappedIoCompletionRoutine,
		Overlapped,
		DataMdl,
		InFlags,
		BytesToReceive);

	status = IoCallDriver(ConnectionDeviceObject, Irp);

	xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_VERBOSE,
		"xTdiReceiveMdlEx returned status=%x\n", status);

	if (STATUS_PENDING != status)
	{
		NTSTATUS miscStatus;
		miscStatus = RtlULongPtrToULong(Irp->IoStatus.Information, BytesReceived);
		ASSERT(NT_SUCCESS(miscStatus));
	}

	return status;
}


NTSTATUS
xTdiSendEx(
	__in PIRP Irp,
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PVOID DataBuffer,
	__in ULONG DataBufferLen,
	__out PULONG BytesSent,
	__in ULONG InFlags,
	__in PXTDI_OVERLAPPED Overlapped)
{
	NTSTATUS status;
	PMDL localMdl;

	ASSERT(NULL != Overlapped && 0 == Overlapped->Internal);
	if (NULL == Overlapped || 0 != Overlapped->Internal)
	{
		return STATUS_INVALID_PARAMETER;
	}

	//
	// Allocate an MDL
	//
	// Note: this locally allocated MDL will be freed at xTdiMdlIoCompletionRoutine
	//
	status = xTdiAllocateInternalMdl(
		DataBuffer,
		DataBufferLen,
		&localMdl);

	if (!NT_SUCCESS(status))
	{
		xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_ERROR,
			"xTdiSendEx failed status=%x\n", status);
		Overlapped->IoStatus.Status = status;
		Overlapped->IoStatus.Information = 0;
		if (Overlapped->CompletionRoutine)
		{
			(*Overlapped->CompletionRoutine)(Irp, Overlapped);
		}
		return status;
	}

	ASSERT(0 == (Overlapped->Internal & XTDI_INTERNAL_MDL));
	Overlapped->Internal |= XTDI_INTERNAL_MDL;

	status = xTdiSendMdlEx(
		Irp,
		ConnectionDeviceObject,
		ConnectionFileObject,
		localMdl,
		DataBufferLen,
		BytesSent,
		InFlags,
		Overlapped);

	return status;
}

NTSTATUS
xTdiReceiveEx(
	__in PIRP Irp,
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PVOID DataBuffer,
	__in ULONG DataBufferLen,
	__out PULONG BytesReceived,
	__in ULONG InFlags,
	__in PXTDI_OVERLAPPED Overlapped)
{
	NTSTATUS status;
	PMDL localMdl;

	ASSERT(NULL != Overlapped && 0 == Overlapped->Internal);
	if (NULL == Overlapped || 0 != Overlapped->Internal)
	{
		return STATUS_INVALID_PARAMETER;
	}
		
	//
	// Allocate an MDL
	//
	// Note: this locally allocated MDL will be freed at xTdiMdlIoCompletionRoutine
	//
	status = xTdiAllocateInternalMdl(
		DataBuffer,
		DataBufferLen,
		&localMdl);

	if (!NT_SUCCESS(status))
	{
		xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_ERROR,
			"xTdiReceiveEx failed status=%x\n", status);
		Overlapped->IoStatus.Status = status;
		Overlapped->IoStatus.Information = 0;
		if (Overlapped->CompletionRoutine)
		{
			(*Overlapped->CompletionRoutine)(Irp, Overlapped);
		}
		return status;
	}

	//
	// We use local internal MDL. Set INTERNAL_MDL to free the Mdl
	// on completion
	//
	ASSERT(0 == (Overlapped->Internal & XTDI_INTERNAL_MDL));
	Overlapped->Internal |= XTDI_INTERNAL_MDL;

	status = xTdiReceiveMdlEx(
		Irp,
		ConnectionDeviceObject,
		ConnectionFileObject,
		localMdl,
		DataBufferLen,
		BytesReceived,
		InFlags,
		Overlapped);

	return status;
}

//
// The xTdiSendMdlEx function sends Mdl.
// It is designed for both synchronous and asynchronous operation.
//
NTSTATUS
xTdiSendMdl(
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PMDL DataMdl,
	__in ULONG BytesToSend,
	__out PULONG BytesSent,
	__in ULONG InFlags,
	__in_opt PXTDI_OVERLAPPED Overlapped)
{
	NTSTATUS status;
	PIRP localIrp;

	BOOLEAN synchronous = FALSE;
	KEVENT event;
	XTDI_OVERLAPPED localOverlapped = {0};

	if (NULL == Overlapped)
	{
		KeInitializeEvent(&event, NotificationEvent, FALSE);
		localOverlapped.Event = &event;
		Overlapped = &localOverlapped;
		synchronous = TRUE;
	}

	localIrp = xTdiBuildInternalIrp(
		ConnectionDeviceObject,
		ConnectionFileObject,
		Overlapped->Event,
		&Overlapped->IoStatus);

	if (NULL == localIrp)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_ERROR,
			"xTdiSendMdl failed, status=%x\n", status);
		if (!synchronous)
		{
			Overlapped->IoStatus.Status = status;
			Overlapped->IoStatus.Information = 0;
			if (Overlapped->CompletionRoutine)
			{
				(*Overlapped->CompletionRoutine)(NULL, Overlapped);
			}
		}
		return status;
	}

	ASSERT(0 == (Overlapped->Internal & XTDI_INTERNAL_IRP));
	Overlapped->Internal |= XTDI_INTERNAL_IRP;

	status = xTdiSendMdlEx(
		localIrp,
		ConnectionDeviceObject,
		ConnectionFileObject,
		DataMdl,
		BytesToSend,
		BytesSent,
		InFlags,
		Overlapped);

	if (synchronous && STATUS_PENDING == status)
	{
		NTSTATUS miscStatus;

		status = KeWaitForSingleObject(
			&event,
			Executive,
			KernelMode,
			FALSE,
			NULL);

		ASSERT(STATUS_SUCCESS == status);

		status = Overlapped->IoStatus.Status;
		miscStatus = RtlULongPtrToULong(Overlapped->IoStatus.Information, BytesSent);
		ASSERT(NT_SUCCESS(miscStatus));
	}

	return status;
}

//
// The xTdiSendMdlEx function sends Mdl.
// It is designed for both synchronous and asynchronous operation.
//
NTSTATUS
xTdiReceiveMdl(
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PMDL DataMdl,
	__in ULONG BytesToReceive,
	__out PULONG BytesReceived,
	__in ULONG InFlags,
	__in_opt PXTDI_OVERLAPPED Overlapped)
{
	NTSTATUS status;
	PIRP localIrp;

	BOOLEAN synchronous = FALSE;
	KEVENT event;
	XTDI_OVERLAPPED localOverlapped = {0};

	if (NULL == Overlapped)
	{
		KeInitializeEvent(&event, NotificationEvent, FALSE);
		localOverlapped.Event = &event;
		Overlapped = &localOverlapped;
		synchronous = TRUE;
	}

	localIrp = xTdiBuildInternalIrp(
		ConnectionDeviceObject,
		ConnectionFileObject,
		Overlapped->Event,
		&Overlapped->IoStatus);

	if (NULL == localIrp)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_ERROR,
			"xTdiReceiveMdl failed, status=%x\n", status);
		if (!synchronous)
		{
			Overlapped->IoStatus.Status = status;
			Overlapped->IoStatus.Information = 0;
			if (Overlapped->CompletionRoutine)
			{
				(*Overlapped->CompletionRoutine)(NULL, Overlapped);
			}
		}
		return status;
	}

	ASSERT(0 == (Overlapped->Internal & XTDI_INTERNAL_IRP));
	Overlapped->Internal |= XTDI_INTERNAL_IRP;

	status = xTdiReceiveMdlEx(
		localIrp,
		ConnectionDeviceObject,
		ConnectionFileObject,
		DataMdl,
		BytesToReceive,
		BytesReceived,
		InFlags,
		Overlapped);

	if (synchronous && STATUS_PENDING == status)
	{
		NTSTATUS miscStatus;

		status = KeWaitForSingleObject(
			&event,
			Executive,
			KernelMode,
			FALSE,
			NULL);

		ASSERT(STATUS_SUCCESS == status);
		status = Overlapped->IoStatus.Status;
		miscStatus = RtlULongPtrToULong(Overlapped->IoStatus.Information, BytesReceived);
		ASSERT(NT_SUCCESS(miscStatus));
	}

	return status;
}

NTSTATUS
xTdiSend(
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PVOID DataBuffer,
	__in ULONG DataBufferLen,
	__out PULONG BytesSent,
	__in ULONG InFlags,
	__in_opt PXTDI_OVERLAPPED Overlapped)
{
	NTSTATUS status;
	PIRP localIrp;
	PMDL localMdl;
	BOOLEAN synchronous = FALSE;
	KEVENT event;
	XTDI_OVERLAPPED localOverlapped = {0};

	if (NULL == Overlapped)
	{
		KeInitializeEvent(&event, NotificationEvent, FALSE);
		localOverlapped.Event = &event;
		Overlapped = &localOverlapped;
		synchronous = TRUE;
	}

	//
	// Clear Internal Flags
	//
	Overlapped->Internal = 0;

	//
	// Allocate an MDL
	//
	// Note: this locally allocated MDL will be freed at xTdiMdlIoCompletionRoutine
	//
	status = xTdiAllocateInternalMdl(
		DataBuffer,
		DataBufferLen,
		&localMdl);

	if (!NT_SUCCESS(status))
	{
		xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_ERROR,
			"xTdiSend(xTdiAllocateInternalMdl) failed, Buffer=%p, Length=0x%x, status=%x\n", 
			DataBuffer, DataBufferLen, status);
		if (!synchronous)
		{
			Overlapped->IoStatus.Status = status;
			Overlapped->IoStatus.Information = 0;
			if (Overlapped->CompletionRoutine)
			{
				(*Overlapped->CompletionRoutine)(NULL, Overlapped);
			}
		}
		return status;
	}

	ASSERT(0 == (Overlapped->Internal & XTDI_INTERNAL_MDL));
	Overlapped->Internal |= XTDI_INTERNAL_MDL;

	localIrp = xTdiBuildInternalIrp(
		ConnectionDeviceObject,
		ConnectionFileObject,
		Overlapped->Event,
		&Overlapped->IoStatus);

	if (NULL == localIrp)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_ERROR,
			"xTdiSendMdl(xTdiBuildInternalIrp) failed, status=%x\n", status);
		xTdiFreeInternalMdl(localMdl);
		Overlapped->Internal &= ~XTDI_INTERNAL_MDL;
		if (!synchronous)
		{
			Overlapped->IoStatus.Status = status;
			Overlapped->IoStatus.Information = 0;
			if (Overlapped->CompletionRoutine)
			{
				(*Overlapped->CompletionRoutine)(NULL, Overlapped);
			}
		}
		return status;
	}

	ASSERT(0 == (Overlapped->Internal & XTDI_INTERNAL_IRP));
	Overlapped->Internal |= XTDI_INTERNAL_IRP;

	status = xTdiSendMdlEx(
		localIrp,
		ConnectionDeviceObject,
		ConnectionFileObject,
		localMdl,
		DataBufferLen,
		BytesSent,
		InFlags,
		Overlapped);

	if (synchronous && STATUS_PENDING == status)
	{
		NTSTATUS miscStatus;
		status = KeWaitForSingleObject(
			&event,
			Executive,
			KernelMode,
			FALSE,
			NULL);

		ASSERT(STATUS_SUCCESS == status);
		status = Overlapped->IoStatus.Status;
		miscStatus = RtlULongPtrToULong(Overlapped->IoStatus.Information, BytesSent);
		ASSERT(NT_SUCCESS(miscStatus));
	}

	return status;
}

NTSTATUS
xTdiReceive(
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PVOID DataBuffer,
	__in ULONG DataBufferLen,
	__out PULONG BytesReceived,
	__in ULONG InFlags,
	__in PXTDI_OVERLAPPED Overlapped)
{
	NTSTATUS status;
	PIRP localIrp;
	PMDL localMdl;
	BOOLEAN synchronous = FALSE;
	KEVENT event;
	XTDI_OVERLAPPED localOverlapped = {0};

	if (NULL == Overlapped)
	{
		KeInitializeEvent(&event, NotificationEvent, FALSE);
		localOverlapped.Event = &event;
		Overlapped = &localOverlapped;
		synchronous = TRUE;
	}

	//
	// Clear Internal Flags
	//
	Overlapped->Internal = 0;

	//
	// Allocate an MDL
	//
	// Note: this locally allocated MDL will be freed at xTdiMdlIoCompletionRoutine
	//
	status = xTdiAllocateInternalMdl(
		DataBuffer,
		DataBufferLen,
		&localMdl);

	if (!NT_SUCCESS(status))
	{
		xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_ERROR,
			"xTdiReceive(xTdiAllocateInternalMdl) failed, Buffer=%p, Length=0x%x, status=%x\n", 
			DataBuffer, DataBufferLen, status);
		if (!synchronous)
		{
			Overlapped->IoStatus.Status = status;
			Overlapped->IoStatus.Information = 0;
			if (Overlapped->CompletionRoutine)
			{
				(*Overlapped->CompletionRoutine)(NULL, Overlapped);
			}
		}
		return status;
	}

	ASSERT(0 == (Overlapped->Internal & XTDI_INTERNAL_MDL));
	Overlapped->Internal |= XTDI_INTERNAL_MDL;

	localIrp = xTdiBuildInternalIrp(
		ConnectionDeviceObject,
		ConnectionFileObject,
		Overlapped->Event,
		&Overlapped->IoStatus);

	if (NULL == localIrp)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		xTdiTrace(XTDI_GENERAL, TRACE_LEVEL_ERROR,
			"xTdiReceive(xTdiBuildInternalIrp) failed, status=%x\n", status);
		xTdiFreeInternalMdl(localMdl);
		Overlapped->Internal &= ~XTDI_INTERNAL_MDL;
		if (!synchronous)
		{
			Overlapped->IoStatus.Status = status;
			Overlapped->IoStatus.Information = 0;
			if (Overlapped->CompletionRoutine)
			{
				(*Overlapped->CompletionRoutine)(NULL, Overlapped);
			}
		}
		return status;
	}

	ASSERT(0 == (Overlapped->Internal & XTDI_INTERNAL_IRP));
	Overlapped->Internal |= XTDI_INTERNAL_IRP;

	status = xTdiReceiveMdlEx(
		localIrp,
		ConnectionDeviceObject,
		ConnectionFileObject,
		localMdl,
		DataBufferLen,
		BytesReceived,
		InFlags,
		Overlapped);

	if (synchronous && STATUS_PENDING == status)
	{
		NTSTATUS miscStatus;

		status = KeWaitForSingleObject(
			&event,
			Executive,
			KernelMode,
			FALSE,
			NULL);

		ASSERT(STATUS_SUCCESS == status);
		status = Overlapped->IoStatus.Status;
		miscStatus = RtlULongPtrToULong(Overlapped->IoStatus.Information, BytesReceived);
		ASSERT(NT_SUCCESS(miscStatus));
	}

	return status;
}
