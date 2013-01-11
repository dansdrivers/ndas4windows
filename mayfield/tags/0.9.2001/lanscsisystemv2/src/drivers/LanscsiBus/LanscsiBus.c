#include <ntddk.h>
#include <scsi.h>
#include <ntddscsi.h>

#include "lanscsibus.h"
#include "lsbusioctl.h"
#include "LSMPIoctl.h"

#include "busenum.h"
#include "stdio.h"
#include "LanscsiBusProc.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "LanscsiBus"

//
//	I/O Control to the LanscsiMiniport.
//	Buffers must be allocated from NonPagedPool
//
//	NOTE:	Do not use  LANSCSIMINIPORT_IOCTL_QUERYINFO.
//			It uses separate input/output buffer.
//			It will be obsolete.
//
NTSTATUS
LSBus_IoctlToLSMPDevice(
		PPDO_DEVICE_DATA	PdoData,
		ULONG				IoctlCode,
		PVOID				InputBuffer,
		LONG				InputBufferLength,
		PVOID				OutputBuffer,
		LONG				OutputBufferLength
	) {

	PDEVICE_OBJECT		AttachedDevice;
    PIRP				irp;
    KEVENT				event;
	PSRB_IO_CONTROL		psrbIoctl;
	LONG				srbIoctlLength;
	PVOID				srbIoctlBuffer;
	LONG				srbIoctlBufferLength;
    NTSTATUS			status;
    PIO_STACK_LOCATION	irpStack;
    SCSI_REQUEST_BLOCK	srb;
    LARGE_INTEGER		startingOffset;
    IO_STATUS_BLOCK		ioStatusBlock;

	AttachedDevice = NULL;
	psrbIoctl	= NULL;
	irp = NULL;

	//
	//	get a ScsiPort device or attached one.
	//
	AttachedDevice = IoGetAttachedDeviceReference(PdoData->Self);

	if(AttachedDevice == NULL) {
		Bus_KdPrint_Def( BUS_DBG_SS_ERROR, ("STATUS_INVALID_DEVICE\n"));
		return STATUS_NO_SUCH_DEVICE;
	}

	//
	//	build an SRB for the miniport
	//
	srbIoctlBufferLength = (InputBufferLength>OutputBufferLength)?InputBufferLength:OutputBufferLength;
	srbIoctlLength = sizeof(SRB_IO_CONTROL) +  srbIoctlBufferLength;

	psrbIoctl = (PSRB_IO_CONTROL)ExAllocatePoolWithTag(NonPagedPool , srbIoctlLength, BUSENUM_POOL_TAG);
	if(psrbIoctl == NULL) {
		Bus_KdPrint_Def( BUS_DBG_SS_ERROR, ("STATUS_INSUFFICIENT_RESOURCES\n"));
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto cleanup;
	}

	RtlZeroMemory(psrbIoctl, srbIoctlLength);
	psrbIoctl->HeaderLength = sizeof(SRB_IO_CONTROL);
	RtlCopyMemory(psrbIoctl->Signature, LANSCSIMINIPORT_IOCTL_SIGNATURE, 8);
	psrbIoctl->Timeout = 10;
	psrbIoctl->ControlCode = IoctlCode;
	psrbIoctl->Length = srbIoctlBufferLength;

	srbIoctlBuffer = (PUCHAR)psrbIoctl + sizeof(SRB_IO_CONTROL);
	RtlCopyMemory(srbIoctlBuffer, InputBuffer, InputBufferLength);

    //
    // Initialize the notification event.
    //

    KeInitializeEvent(&event,
                        NotificationEvent,
                        FALSE);
	startingOffset.QuadPart = 1;

    //
    // Build IRP for this request.
    // Note we do this synchronously for two reasons.  If it was done
    // asynchonously then the completion code would have to make a special
    // check to deallocate the buffer.  Second if a completion routine were
    // used then an additional IRP stack location would be needed.
    //

    irp = IoBuildSynchronousFsdRequest(
                IRP_MJ_SCSI,
                AttachedDevice,
                psrbIoctl,
                srbIoctlLength,
                &startingOffset,
                &event,
                &ioStatusBlock);

    irpStack = IoGetNextIrpStackLocation(irp);

    if (irp == NULL) {
        Bus_KdPrint_Def( BUS_DBG_SS_ERROR, ("STATUS_INSUFFICIENT_RESOURCES\n"));

		status = STATUS_INSUFFICIENT_RESOURCES;
		goto cleanup;
    }

    //
    // Set major and minor codes.
    //

    irpStack->MajorFunction = IRP_MJ_SCSI;
    irpStack->MinorFunction = 1;

    //
    // Fill in SRB fields.
    //

    irpStack->Parameters.Others.Argument1 = &srb;

    //
    // Zero out the srb.
    //

    RtlZeroMemory(&srb, sizeof(SCSI_REQUEST_BLOCK));

    srb.PathId = 0;
    srb.TargetId = 0;
    srb.Lun = 0;

    srb.Function = SRB_FUNCTION_IO_CONTROL;
    srb.Length = sizeof(SCSI_REQUEST_BLOCK);

    srb.SrbFlags = /*SRB_FLAGS_DATA_IN |*/ SRB_FLAGS_NO_QUEUE_FREEZE /*| SRB_FLAGS_BYPASS_FROZEN_QUEUE */;

    srb.OriginalRequest = irp;

    //
    // Set timeout to requested value.
    //

    srb.TimeOutValue = psrbIoctl->Timeout;

    //
    // Set the data buffer.
    //

    srb.DataBuffer = psrbIoctl;
    srb.DataTransferLength = srbIoctlLength;

    //
    // Flush the data buffer for output. This will insure that the data is
    // written back to memory.  Since the data-in flag is the the port driver
    // will flush the data again for input which will ensure the data is not
    // in the cache.
    //
/*
    KeFlushIoBuffers(irp->MdlAddress, FALSE, TRUE);
*/
    status = IoCallDriver( AttachedDevice, irp );

    //
    // Wait for request to complete.
    //
    if (status == STATUS_PENDING) {

        (VOID)KeWaitForSingleObject( 
									&event,
                                     Executive,
                                     KernelMode,
                                     FALSE,
                                     (PLARGE_INTEGER)NULL 
									 );

        status = ioStatusBlock.Status;
    }

	//
	//	get the result
	//
	if(status == STATUS_SUCCESS) {
		if(OutputBuffer && OutputBufferLength)
			RtlCopyMemory(OutputBuffer, srbIoctlBuffer, OutputBufferLength);
			Bus_KdPrint_Def( BUS_DBG_SS_NOISE, ("%d succeeded!\n", IoctlCode));
	}

cleanup:
	if(psrbIoctl)
		ExFreePool(psrbIoctl);
	if(AttachedDevice)
		ObDereferenceObject(AttachedDevice);

    return status;
}


NTSTATUS
LSBus_OpenLanscsiAdapter(
				   IN	PPDO_LANSCSI_DEVICE_DATA	LanscsiAdapter,
				   IN	ULONG				MaxBlocks,
				   IN	PKEVENT				DisconEventToService,
				   IN	PKEVENT				AlarmEventToService
				   )
{

	Bus_KdPrint_Def(BUS_DBG_SS_TRACE, ("Entered.\n"));

    ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL);

	RtlZeroMemory(
		LanscsiAdapter,
		sizeof(PDO_LANSCSI_DEVICE_DATA)
		);

	KeInitializeSpinLock(&LanscsiAdapter->LSDevDataSpinLock);
	LanscsiAdapter->MaxBlocksPerRequest = MaxBlocks;
	LanscsiAdapter->DisconEventToService = DisconEventToService;
	LanscsiAdapter->AlarmEventToService = AlarmEventToService;

	//
	//	initialize private fields.
	//
	KeInitializeEvent(
			&LanscsiAdapter->AddTargetEvent,
			NotificationEvent,
			FALSE
    );

	return STATUS_SUCCESS;
}

NTSTATUS
LSBus_WaitUntilLanscsiMiniportStop(
		IN	PPDO_LANSCSI_DEVICE_DATA	LanscsiAdapter
	) {
	LARGE_INTEGER	Interval;
	LONG			WaitCnt;
	NTSTATUS		ntStatus;
	KIRQL			oldIrql;

    ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL);
	Bus_KdPrint_Def(BUS_DBG_SS_TRACE, ("Entered.\n"));

	//
	//	Wait for Lanscsiminiport.
	//
	ntStatus = STATUS_SUCCESS;
	Interval.QuadPart = - NANO100_PER_SEC / 2;	// 0.5 seconds.
	WaitCnt = 0;
	while(1) {

		KeAcquireSpinLock(&LanscsiAdapter->LSDevDataSpinLock, &oldIrql);
		if(ADAPTERINFO_ISSTATUS(LanscsiAdapter->AdapterStatus, ADAPTERINFO_STATUS_STOPPED)) {
			KeReleaseSpinLock(&LanscsiAdapter->LSDevDataSpinLock, oldIrql);
			break;
		}
		KeReleaseSpinLock(&LanscsiAdapter->LSDevDataSpinLock, oldIrql);

		Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("Wait for Lanscsiminiport!!\n"));
		WaitCnt ++;
		KeDelayExecutionThread(KernelMode, TRUE,&Interval);
		if(WaitCnt >= LSBUS_LANSCSIMINIPORT_STOP_TIMEOUT) {
			Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("TimeOut!!!\n"));
			ntStatus = STATUS_TIMEOUT;
			break;
		}
	}

	return ntStatus;
}



NTSTATUS
LSBus_CloseLanscsiAdapter(
					IN	PPDO_LANSCSI_DEVICE_DATA	LanscsiAdapter
					)
{
	
    ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL);
	Bus_KdPrint_Def(BUS_DBG_SS_TRACE, ("Entered.\n"));

	//
	//	Dereference objects.
	//
	ObDereferenceObject(LanscsiAdapter->DisconEventToService);

	if(LanscsiAdapter->AlarmEventToService)
		ObDereferenceObject(LanscsiAdapter->AlarmEventToService);

	//
	//	Free allocated memory.
	//
	if(LanscsiAdapter->AddTargetData)
		ExFreePool(LanscsiAdapter->AddTargetData);

	return STATUS_SUCCESS;
}


//
//	Query information on LanscsiBus
//
NTSTATUS
LSBus_QueryInformation(
		PFDO_DEVICE_DATA				FdoData,
		PBUSENUM_QUERY_INFORMATION		Query,
		PBUSENUM_INFORMATION			Information,
		LONG							OutBufferLength,
		PLONG							OutBufferLenNeeded
	) {

	NTSTATUS			ntStatus;
	PLIST_ENTRY         entry;
	PPDO_DEVICE_DATA	PdoData;
	

	ntStatus = STATUS_SUCCESS;
	*OutBufferLenNeeded = OutBufferLength;
	PdoData = NULL;

	//
	//	Acquire the mutex to prevent PdoData ( Device Extension ) to disappear.
	//
    KeEnterCriticalRegion();
	ExAcquireFastMutex (&FdoData->Mutex);

	switch(Query->InfoClass) {
	case INFORMATION_NUMOFPDOS: {
		ULONG				NumOfPDOs;

		NumOfPDOs = 0;

		for (entry = FdoData->ListOfPDOs.Flink;
			entry != &FdoData->ListOfPDOs;
			entry = entry->Flink) {
			NumOfPDOs ++;
		}

		Information->NumberOfPDOs = NumOfPDOs;
		break;
	}
	case INFORMATION_PDO: {
		KIRQL	oldIrql;

		for (entry = FdoData->ListOfPDOs.Flink;
			entry != &FdoData->ListOfPDOs;
			entry = entry->Flink) {

				PdoData = CONTAINING_RECORD(entry, PDO_DEVICE_DATA, Link);
				if(Query->SlotNo == PdoData->SlotNo) {
					ObReferenceObject(PdoData->Self);
					break;
				}
				PdoData = NULL;

		}

		if(PdoData) {
			KeAcquireSpinLock(&PdoData->LanscsiAdapterPDO.LSDevDataSpinLock, &oldIrql);
			Bus_KdPrint_Def(BUS_DBG_SS_TRACE, ("Status:%08lx DAcc:%08lx GAcc:%08lx\n",
											PdoData->LanscsiAdapterPDO.AdapterStatus,
											PdoData->LanscsiAdapterPDO.DesiredAccess,
											PdoData->LanscsiAdapterPDO.GrantedAccess
									));
			Information->PdoInfo.AdapterStatus = PdoData->LanscsiAdapterPDO.AdapterStatus;
			Information->PdoInfo.DesiredAccess = PdoData->LanscsiAdapterPDO.DesiredAccess;
			Information->PdoInfo.GrantedAccess = PdoData->LanscsiAdapterPDO.GrantedAccess;
			KeReleaseSpinLock(&PdoData->LanscsiAdapterPDO.LSDevDataSpinLock, oldIrql);

			ObDereferenceObject(PdoData->Self);
		} else {
			Bus_KdPrint_Def(BUS_DBG_SS_NOISE, ("No PDO for SlotNo %d!\n", Query->SlotNo));
			ntStatus = STATUS_NO_SUCH_DEVICE;
		}
		break;
	}
	case INFORMATION_PDOENUM: {
		LARGE_INTEGER		TimeOut;

		for (entry = FdoData->ListOfPDOs.Flink;
			entry != &FdoData->ListOfPDOs;
			entry = entry->Flink) {

				PdoData = CONTAINING_RECORD(entry, PDO_DEVICE_DATA, Link);
				if(Query->SlotNo == PdoData->SlotNo) {
					ObReferenceObject(PdoData->Self);
					break;
				}
				PdoData = NULL;

		}

		ExReleaseFastMutex (&FdoData->Mutex);
	    KeLeaveCriticalRegion();

		if(!PdoData) {
		    KeEnterCriticalRegion();
			ExAcquireFastMutex (&FdoData->Mutex);
			ntStatus = STATUS_NO_SUCH_DEVICE;
			break;
		}
		//
		//	Wait until LDServ sends AddTargetData.
		//
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("waiting for AddTargetEvent.\n"));
		TimeOut.QuadPart = -10 * 1000 * 1000 * 120;			// 120 seconds
		ntStatus = KeWaitForSingleObject(
						&PdoData->LanscsiAdapterPDO.AddTargetEvent,
						Executive,
						KernelMode,
						FALSE,
						&TimeOut
					);
		if(ntStatus != STATUS_SUCCESS) {
		    KeEnterCriticalRegion();
			ExAcquireFastMutex (&FdoData->Mutex);

			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("failed to wait for AddTargetEvent.\n"));
			ntStatus = STATUS_NO_SUCH_DEVICE;
			ObDereferenceObject(PdoData->Self);
			break;
		}
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Completed to wait for AddTargetEvent.\n"));

	    KeEnterCriticalRegion();
		ExAcquireFastMutex (&FdoData->Mutex);


		if(PdoData) {
			PLANSCSI_ADD_TARGET_DATA	AddTarget;
			LONG						AddTargetLenNeeded;
			LONG						InfoBuffLenNeeded;

			AddTarget = PdoData->LanscsiAdapterPDO.AddTargetData;
			if(AddTarget == NULL) {
				ntStatus = STATUS_NO_SUCH_DEVICE;
				ObDereferenceObject(PdoData->Self);
				break;
			}
			AddTargetLenNeeded = sizeof(LANSCSI_ADD_TARGET_DATA) + (AddTarget->ulNumberOfUnitDiskList-1)*sizeof(LSBUS_UNITDISK);
			InfoBuffLenNeeded = sizeof(BUSENUM_INFORMATION) - sizeof(LANSCSI_ADD_TARGET_DATA) + (AddTargetLenNeeded);
//			DbgBreakPoint();
			*OutBufferLenNeeded = InfoBuffLenNeeded;
			if(OutBufferLength < InfoBuffLenNeeded) {
				ntStatus = STATUS_BUFFER_TOO_SMALL;
			} else {
				RtlCopyMemory(&Information->PdoEnumInfo.AddTargetData, AddTarget, AddTargetLenNeeded);
				Information->PdoEnumInfo.DisconEventToService = PdoData->LanscsiAdapterPDO.DisconEventToService;
				Information->PdoEnumInfo.AlarmEventToService = PdoData->LanscsiAdapterPDO.AlarmEventToService;
				Information->PdoEnumInfo.MaxBlocksPerRequest = PdoData->LanscsiAdapterPDO.MaxBlocksPerRequest;
			}
		} else {

			ntStatus = STATUS_NO_SUCH_DEVICE;
		}

		ObDereferenceObject(PdoData->Self);
		break;
	}

	default:
		ntStatus = STATUS_INVALID_PARAMETER;
	}

	ExReleaseFastMutex (&FdoData->Mutex);
    KeLeaveCriticalRegion();

	return ntStatus;
}

//////////////////////////////////////////////////////////////////////////
//
//	Enable devices by reading registry.
//
/*
NTSTATUS
OpenServiceRegistry(
		HANDLE	*SvcReg
	){
    UNICODE_STRING		name;
    OBJECT_ATTRIBUTES	objectAttributes;
    HANDLE				regKey;
    ULONG				disposition;
    NTSTATUS			status;

    //
    // Open the SCSI key in the device map.
    //

    RtlInitUnicodeString(&name,
                         L"\\Registry\\Machine\\SOFTWARE\\XiMeta\\NetDisks\0");

    InitializeObjectAttributes(&objectAttributes,
                               &name,
                               OBJ_CASE_INSENSITIVE|OBJ_KERNEL_HANDLE,
                               NULL,
                               (PSECURITY_DESCRIPTOR) NULL);
	disposition = REG_OPENED_EXISTING_KEY;
    //
    // Create or open the key.
    //
    status = ZwCreateKey(&regKey,
                         KEY_READ,
                         &objectAttributes,
                         0,
                         (PUNICODE_STRING) NULL,
                         REG_OPTION_NON_VOLATILE,
                         &disposition);

    if(!NT_SUCCESS(status)) {
        regKey = NULL;
    }

	*SvcReg = regKey;

    return status;
}

VOID
LSBus_PlugInDeviceFromRegistry() {
	NTSTATUS	status;
	HANDLE		SvcReg;

	SvcReg = NULL;
	status = OpenServiceRegistry(&SvcReg);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("[LSBus] OpenServiceRegistry() failed.\n"));
		return status;
	}

	

cleanup:
	if(SvcReg)
		ZwCloseHandle(SvcReg);
}
*/
