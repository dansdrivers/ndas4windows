#include "port.h"
#include <ntddstor.h>
#include <ntddscsi.h>
/* WDK does not have SCSI_REQUEST_BLOCK definition in ntddscsi.h */
#ifndef SCSI_REQUEST_BLOCK_SIZE
#include <storport.h>
#endif
#include "utils.h"

#define NdasPortTrace _NdasPortTrace

typedef enum _NDASPORT_TRACE_FLAGS {
	NDASPORT_PDO_INIT  = 0x00000001,
	NDASPORT_PDO_IOCTL = 0x00000002,
	NDASPORT_PDO_SCSI  = 0x00000004
} NDASPORT_TRACE_FLAGS;

#ifndef TRACE_LEVEL_NONE
#define TRACE_LEVEL_NONE        0   // Tracing is not on
#define TRACE_LEVEL_FATAL       1   // Abnormal exit or termination
#define TRACE_LEVEL_ERROR       2   // Severe errors that need logging
#define TRACE_LEVEL_WARNING     3   // Warnings such as allocation failure
#define TRACE_LEVEL_INFORMATION 4   // Includes non-error cases(e.g.,Entry-Exit)
#define TRACE_LEVEL_VERBOSE     5   // Detailed traces from intermediate steps
#define TRACE_LEVEL_RESERVED6   6
#define TRACE_LEVEL_RESERVED7   7
#define TRACE_LEVEL_RESERVED8   8
#define TRACE_LEVEL_RESERVED9   9
#endif

void _NdasPortTrace(ULONG Flag, ULONG Level, const CHAR* Format, ...)
{
	CHAR buffer[256];
	va_list ap;
	va_start(ap, Format);
	RtlStringCchVPrintfA(buffer, RTL_NUMBER_OF(buffer), Format, ap);
	DbgPrint("ndasport: %s", buffer);
	va_end(ap);
}

typedef enum _NDASPORT_PDO_LU_FLAGS {
	NDASPORT_LU_FLAGS_QUEUE_FROZEN = 0x00000001,
	NDASPORT_LU_FLAGS_IS_ACTIVE    = 0x00000002,
	NDASPORT_LU_FLAGS_QUEUE_LOCKED = 0x00000004,
	NDASPORT_LU_FLAGS_QUEUE_PAUSED = 0x00000008
} NDASPORT_PDO_LU_FLAGS;

BOOLEAN
FORCEINLINE
NdasPortPdoSrbIsBypassRequest(
	PSCSI_REQUEST_BLOCK Srb,
	ULONG LogicalUnitFlags)
{
	ULONG flags = Srb->SrbFlags & (SRB_FLAGS_BYPASS_FROZEN_QUEUE |
		SRB_FLAGS_BYPASS_LOCKED_QUEUE);

	ASSERT(TestFlag(LogicalUnitFlags, 
		NDASPORT_LU_FLAGS_QUEUE_FROZEN | NDASPORT_LU_FLAGS_QUEUE_LOCKED) !=
		(NDASPORT_LU_FLAGS_QUEUE_FROZEN | NDASPORT_LU_FLAGS_QUEUE_LOCKED));

	if (flags == 0) 
	{
		return FALSE;
	}

	if (flags & SRB_FLAGS_BYPASS_LOCKED_QUEUE) 
	{
		DebugPrint((2, "SpSrbIsBypassRequest: Srb %#08lx is marked to bypass locked queue\n", Srb));

		if (TestFlag(LogicalUnitFlags, NDASPORT_LU_FLAGS_QUEUE_LOCKED | NDASPORT_LU_FLAGS_QUEUE_PAUSED)) 
		{
			DebugPrint((1, "SpSrbIsBypassRequest: Queue is locked - %#08lx is a bypass srb\n", Srb));
			return TRUE;
		} 
		else
		{
			DebugPrint((3, "SpSrbIsBypassRequest: Queue is not locked - not a bypass request\n"));
			return FALSE;
		}
	}

	return TRUE;
}

VOID
NdasPortPdoEvtIoScsi(
	__in WDFQUEUE Queue,
	__in WDFREQUEST Request,
	__in size_t OutputBufferLength,
	__in size_t InputBufferLength,
	__in ULONG IoControlCode)
{
	NTSTATUS status;
	PSCSI_REQUEST_BLOCK srb;
	WDF_REQUEST_PARAMETERS params;

	UNREFERENCED_PARAMETER(IoControlCode);

	WDF_REQUEST_PARAMETERS_INIT(&params);
	srb = (PSCSI_REQUEST_BLOCK) params.Parameters.Others.Arg1;
	
	NdasPortTrace(NDASPORT_PDO_SCSI, TRACE_LEVEL_INFORMATION,
		"EvtIoScsi: Queue=%p, Request=%p, IoControlCode=%x, Srb.Function=%x\n",
		Queue, Request, IoControlCode, srb->Function);

	switch (srb->Function)
	{
		//
		// Functions to be forwarded to the StartIo routine of the LU
		//
	case SRB_FUNCTION_EXECUTE_SCSI:
	case SRB_FUNCTION_IO_CONTROL:
	case SRB_FUNCTION_SHUTDOWN:
	case SRB_FUNCTION_FLUSH:
		//
		// Functions related to the claim/release 
		//
	case SRB_FUNCTION_CLAIM_DEVICE:
	case SRB_FUNCTION_ATTACH_DEVICE:
	case SRB_FUNCTION_RELEASE_DEVICE:
	case SRB_FUNCTION_REMOVE_DEVICE:
		//
		// Queue management functions
		//
	case SRB_FUNCTION_RELEASE_QUEUE:
	case SRB_FUNCTION_FLUSH_QUEUE:
	case SRB_FUNCTION_LOCK_QUEUE:
	case SRB_FUNCTION_UNLOCK_QUEUE:

	case SRB_FUNCTION_RECEIVE_EVENT:
	case SRB_FUNCTION_ABORT_COMMAND:
	case SRB_FUNCTION_RELEASE_RECOVERY:
	case SRB_FUNCTION_RESET_BUS:
	case SRB_FUNCTION_RESET_DEVICE:
	case SRB_FUNCTION_TERMINATE_IO:
	case SRB_FUNCTION_WMI:
	case SRB_FUNCTION_RESET_LOGICAL_UNIT:
		//
		// Unsupported functions
		//
	case SRB_FUNCTION_SET_LINK_TIMEOUT:
	case SRB_FUNCTION_LINK_TIMEOUT_OCCURRED:
	case SRB_FUNCTION_LINK_TIMEOUT_COMPLETE:
	case SRB_FUNCTION_POWER:
	case SRB_FUNCTION_PNP:
	default:
		srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
		srb->DataTransferLength = 0;
		WdfRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
		return;
	}
}

NTSTATUS
NdasPortPdoProbeScsiMiniportIoctl(
	__in WDFQUEUE Queue,
	__in WDFREQUEST Request,
	__in size_t OutputBufferLength,
	__in size_t InputBufferLength,
	__out PSRB_IO_CONTROL* SrbIoControl)
{
	NTSTATUS status;
	ULONG length;
	PSRB_IO_CONTROL buffer;

	*SrbIoControl = NULL;

	if (InputBufferLength < sizeof(SRB_IO_CONTROL))
	{
		NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_WARNING,
			"IOCTL_SCSIMINIPORT DeviceIoControl input buffer too small "
			"(InputBufferLength=0x%X < 0x%X in bytes)\n",
			InputBufferLength,
			sizeof(SRB_IO_CONTROL));
		return STATUS_INVALID_PARAMETER;
	}

	status = WdfRequestRetrieveInputBuffer(
		Request,
		sizeof(SRB_IO_CONTROL),
		&buffer,
		NULL);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_WARNING,
			"IOCTL_SCSIMINIPORT DeviceIoControl, RetrieveInputBuffer failed, status=%x\n",
			status);
		return status;
	}

	if (sizeof(SRB_IO_CONTROL) != buffer->HeaderLength) 
	{
		NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_WARNING,
			"IOCTL_SCSIMINIPORT SrbControl->HeaderLength is invalid, "
			"(HeaderLength=0x%X != 0x%X in bytes)\n",
			buffer->HeaderLength,
			sizeof(SRB_IO_CONTROL));
		return STATUS_REVISION_MISMATCH;
	}

	//
	// Buffer overflow check
	//

	length = buffer->HeaderLength + buffer->Length;

	if ((length < buffer->HeaderLength) || (length < buffer->Length))
	{
		//
		// total length overflows a ULONG
		//
		NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_WARNING,
			"IOCTL_SCSIMINIPORT SrbControl->(HeaderLength + Length) overflows ULONG "
			"(HeaderLength=0x%X, Length=0x%X\n",
			buffer->HeaderLength,
			buffer->Length);
		return STATUS_INVALID_PARAMETER;
	}

	if (OutputBufferLength < length && InputBufferLength < length) 
	{
		NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_WARNING,
			"IOCTL_SCSIMINIPORT BufferLengths are less than HeaderLength + Length "
			"(OutputBufferLength 0x%X or InputBufferLength 0x%X < "
			"HeaderLength=0x%X + Length=0x%X = 0x%X\n",
			OutputBufferLength,
			InputBufferLength,
			buffer->HeaderLength,
			buffer->Length,
			length);

		return STATUS_BUFFER_TOO_SMALL;
	}

	*SrbIoControl = buffer;

	return STATUS_SUCCESS;
}

VOID
NdasPortPdoRedirectScsiMiniportIoctl(
	__in WDFQUEUE Queue,
	__in WDFREQUEST Request,
	__in size_t OutputBufferLength,
	__in size_t InputBufferLength)
{
	NTSTATUS status;
	PNDASPORT_PDO_EXTENSION pdoExtension;
	PSRB_IO_CONTROL srbIoControl;
	WDFIOTARGET ioTarget;
	WDF_MEMORY_DESCRIPTOR srbMemoryDescriptor;
	SCSI_REQUEST_BLOCK srb;
	ULONG dataLength;
	size_t srbLength;

	status = NdasPortPdoProbeScsiMiniportIoctl(
		Queue,
		Request,
		OutputBufferLength,
		InputBufferLength,
		&srbIoControl);

	if (!NT_SUCCESS(status))
	{
		WdfRequestComplete(Request, status);
		return;
	}

	dataLength = srbIoControl->HeaderLength + srbIoControl->Length;

	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(
		&srbMemoryDescriptor, 
		&srb, 
		sizeof(SCSI_REQUEST_BLOCK));

	pdoExtension = NdasPortPdoGetExtention(WdfIoQueueGetDevice(Queue));

	RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));
	srb.Length = sizeof(SCSI_REQUEST_BLOCK);
	srb.Function = SRB_FUNCTION_IO_CONTROL;
	srb.PathId = pdoExtension->LogicalUnitAddress.PathId;
	srb.TargetId = pdoExtension->LogicalUnitAddress.TargetId;
	srb.Lun = pdoExtension->LogicalUnitAddress.Lun;
	srb.SrbFlags = SRB_FLAGS_DATA_IN | SRB_FLAGS_NO_QUEUE_FREEZE;
	srb.OriginalRequest = WdfRequestWdmGetIrp(Request);
	srb.TimeOutValue = srbIoControl->Timeout;
	srb.DataBuffer = srbIoControl;
	srb.DataTransferLength = dataLength;

	ioTarget = WdfDeviceGetIoTarget(WdfIoQueueGetDevice(Queue));

	status = WdfIoTargetSendInternalIoctlOthersSynchronously(
		ioTarget,
		NULL,
		0, /* not used */
		&srbMemoryDescriptor,
		NULL,
		NULL,
		NULL,
		NULL);

	//
	// Set the information length to the smaller of the output buffer length
	// and the length returned in the srb.
	//

	dataLength = min(srb.DataTransferLength, OutputBufferLength);
	WdfRequestCompleteWithInformation(Request, status, dataLength);

	return;
}

VOID
NdasPortPdoQueryStorageProperty(
	__in WDFQUEUE Queue,
	__in WDFREQUEST Request,
	__in size_t OutputBufferLength,
	__in size_t InputBufferLength,
	__in PSTORAGE_PROPERTY_QUERY Query)
{
	NTSTATUS status;
	PVOID outputBuffer;

	ASSERT(StorageDeviceProperty == Query->PropertyId ||
		StorageDeviceIdProperty == Query->PropertyId ||
		StorageDeviceUniqueIdProperty == Query->PropertyId ||
		StorageDeviceWriteCacheProperty == Query->PropertyId);

	if (Query->QueryType >= PropertyMaskQuery)
	{
		//
		// We do not support masked query (or other not-known queries)
		//
		WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
		return;
	}

	status = WdfRequestRetrieveOutputBuffer(
		Request,
		OutputBufferLength,
		&outputBuffer,
		NULL);

	if (!NT_SUCCESS(status))
	{
		WdfRequestComplete(Request, status);
		return;
	}

	switch (Query->PropertyId)
	{
	case StorageDeviceProperty:
		{
			PSTORAGE_DEVICE_DESCRIPTOR deviceDescriptor = 
				(PSTORAGE_DEVICE_DESCRIPTOR) outputBuffer;

		}
	}
}

VOID
NdasPortPdoEvtIoDeviceControl(
	__in WDFQUEUE Queue,
	__in WDFREQUEST Request,
	__in size_t OutputBufferLength,
	__in size_t InputBufferLength,
	__in ULONG IoControlCode)
{
	NTSTATUS status;

	switch (IoControlCode)
	{
	case IOCTL_SCSI_GET_ADDRESS:
		{
			ULONG byteReturned = 0;

			if (OutputBufferLength < sizeof(SCSI_ADDRESS))
			{
				status = STATUS_BUFFER_TOO_SMALL;
			}
			else
			{
				PSCSI_ADDRESS scsiAddress = NULL;
				status = WdfRequestRetrieveOutputBuffer(
					Request,
					sizeof(SCSI_ADDRESS),
					&scsiAddress,
					NULL);
				if (NT_SUCCESS(status))
				{
					PNDASPORT_PDO_EXTENSION pdoExtension;

					pdoExtension = NdasPortPdoGetExtention(WdfIoQueueGetDevice(Queue));

					scsiAddress->Length = sizeof(SCSI_ADDRESS);
					scsiAddress->PortNumber = pdoExtension->LogicalUnitAddress.PortNumber;
					scsiAddress->PathId = pdoExtension->LogicalUnitAddress.PathId;
					scsiAddress->TargetId = pdoExtension->LogicalUnitAddress.TargetId;
					scsiAddress->Lun = pdoExtension->LogicalUnitAddress.Lun;
					byteReturned = sizeof(SCSI_ADDRESS);
					status = STATUS_SUCCESS;
				}
			}
			WdfRequestCompleteWithInformation(Request, status, byteReturned);
		}
		return;
	case IOCTL_SCSI_MINIPORT:
		NdasPortPdoRedirectScsiMiniportIoctl(
			Queue, 
			Request, 
			OutputBufferLength, 
			InputBufferLength);
		return;
	case IOCTL_STORAGE_QUERY_PROPERTY:
		{
			PSTORAGE_PROPERTY_QUERY query;
			size_t requiredLength;
			
			requiredLength = FIELD_OFFSET(STORAGE_PROPERTY_QUERY, AdditionalParameters);
			
			if (InputBufferLength < requiredLength)
			{
				WdfRequestComplete(Request, STATUS_INVALID_PARAMETER);
				return;
			}
			
			status = WdfRequestRetrieveInputBuffer(
				Request,
				requiredLength,
				&query,
				NULL);

			if (!NT_SUCCESS(status))
			{
				WdfRequestComplete(Request, status);
				return;
			}

			//
			// PDOs are interested in device-related properties only
			// Otherwise, we should send it down to the adapter FDO.
			//

			switch (query->PropertyId)
			{
			case StorageDeviceProperty:
			case StorageDeviceIdProperty:
			case StorageDeviceUniqueIdProperty:
			case StorageDeviceWriteCacheProperty:
				NdasPortPdoQueryStorageProperty(
					Queue,
					Request,
					OutputBufferLength,
					InputBufferLength,
					query);
				return;
			//
			// Send all other queries down to the adapter FDO
			//
			}
		}
		// fall through
	default:
		{
			WDF_REQUEST_SEND_OPTIONS requestSendOptions;
			BOOLEAN sent;

			WdfRequestFormatRequestUsingCurrentType(Request);

			WDF_REQUEST_SEND_OPTIONS_INIT(
				&requestSendOptions, 
				WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

			sent = WdfRequestSend(
				Request, 
				WdfDeviceGetIoTarget(WdfIoQueueGetDevice(Queue)),
				&requestSendOptions);

			if (!sent)
			{
				status = WdfRequestGetStatus(Request);
				WdfRequestComplete(Request, status);
			}
		}
		return;
	}
}
