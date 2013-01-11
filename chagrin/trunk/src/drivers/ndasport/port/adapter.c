#include "port.h"

NTSTATUS
NdasPortAdapterDispatchDeviceControl(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp)
{
	NTSTATUS status;
	PNDASPORT_ADAPTER_EXTENSION PdoExtension;
	PNDASPORT_COMMON_EXTENSION CommonExtension;
	PIO_STACK_LOCATION irpStack;
	ULONG ioControlCode;
	ULONG isRemoved;

	CommonExtension = (PNDASPORT_COMMON_EXTENSION) Pdo->DeviceExtension;

	isRemoved = NpAcquireRemoveLock(CommonExtension, Irp);
	if (isRemoved)
	{		
		return NpReleaseRemoveLockAndCompleteIrp(
			CommonExtension, 
			Irp, 
			STATUS_DEVICE_DOES_NOT_EXIST);
	}

	PdoExtension = (PNDASPORT_PDO_EXTENSION) Pdo->DeviceExtension;
	irpStack = IoGetCurrentIrpStackLocation(Irp);
	ioControlCode = irpStack->Parameters.DeviceIoControl.IoControlCode;

	NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_INFORMATION,
		"PDO: IOCTL %s(%08x) (f%04X,a%04X,c%04X,m%04X), Pdo=%p, Irp=%p\n", 
		DbgIoControlCodeStringA(ioControlCode),
		ioControlCode, 
		ioControlCode>>16,
		(ioControlCode>>14&0x0003),
		(ioControlCode>>2&0x0FFF),
		(ioControlCode&0x0003),
		Pdo,
		Irp);

	switch (ioControlCode) 
	{
	case IOCTL_SCSI_GET_CAPABILITIES:
		{
			PIO_SCSI_CAPABILITIES scsiCapabilities;
			ULONG resultLength;

			if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
				sizeof(IO_SCSI_CAPABILITIES))
			{
				return NpReleaseRemoveLockAndCompleteIrp(
					CommonExtension,
					Irp,
					STATUS_BUFFER_TOO_SMALL);
			}

			scsiCapabilities = Irp->AssociatedIrp.SystemBuffer;

			status = PdoExtension->LogicalUnitInterface.QueryIoScsiCapabilities(
				NdasPortPdoGetLogicalUnitExtension(PdoExtension),
				scsiCapabilities,
				irpStack->Parameters.DeviceIoControl.OutputBufferLength,
				&resultLength);

			if (STATUS_NOT_SUPPORTED == status)
			{
				return NpReleaseRemoveLockAndForwardIrp(
					CommonExtension,
					Irp);
			}

			return NpReleaseRemoveLockAndCompleteIrpEx(
				CommonExtension,
				Irp,
				status,
				resultLength,
				IO_NO_INCREMENT);
		}
	case IOCTL_STORAGE_QUERY_PROPERTY: 
		{
			//
			// Validate the query
			//
			PSTORAGE_PROPERTY_QUERY query;
			query = (PSTORAGE_PROPERTY_QUERY) Irp->AssociatedIrp.SystemBuffer;

			if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
				FIELD_OFFSET(STORAGE_PROPERTY_QUERY, AdditionalParameters))
			{
				return NpReleaseRemoveLockAndCompleteIrp(
					CommonExtension,
					Irp,
					STATUS_INVALID_PARAMETER);
			}

			return NdasPortPdoQueryProperty(PdoExtension, Irp);
		}
	case IOCTL_SCSI_GET_ADDRESS: 
		{
			PSCSI_ADDRESS scsiAddress;

			scsiAddress = (PSCSI_ADDRESS) Irp->AssociatedIrp.SystemBuffer;

			if (irpStack->Parameters.DeviceIoControl.OutputBufferLength <
				sizeof(SCSI_ADDRESS)) 
			{
				status = STATUS_BUFFER_TOO_SMALL;
			}
			else
			{
				scsiAddress->Length = sizeof(SCSI_ADDRESS);
				scsiAddress->PortNumber = (UCHAR) PdoExtension->PortNumber;
				scsiAddress->PathId = PdoExtension->PathId;
				scsiAddress->TargetId = PdoExtension->TargetId;
				scsiAddress->Lun = PdoExtension->Lun;
				Irp->IoStatus.Information = sizeof(SCSI_ADDRESS);
				status = STATUS_SUCCESS;
			}
		}

		return NpReleaseRemoveLockAndCompleteIrp(
			CommonExtension,
			Irp,
			status);

	default:

		NdasPortTrace(NDASPORT_PDO_IOCTL, TRACE_LEVEL_INFORMATION,
			"PDO DeviceControl: Passing down to the device object=%p\n",
			CommonExtension->LowerDeviceObject);

		return NpReleaseRemoveLockAndForwardIrp(
			CommonExtension,
			Irp);
	}
	return STATUS_NOT_SUPPORTED;
}

NTSTATUS
NdasPortAdapterDispatchScsi(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp)
{

}
