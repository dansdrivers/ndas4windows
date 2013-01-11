#include <ntddk.h>
#include <scsi.h>
#include <ntddscsi.h>
#include <ntintsafe.h>

#include "devreg.h"
#include "ndasbus.h"
#include "ndasbusioctl.h"
#include "ndasscsiioctl.h"
#include "ndas/ndasdib.h"

#include "busenum.h"
#include "stdio.h"
#include "ndasbuspriv.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "NdasBus"

//
//	copy from W2K ntifs.h for INFORMATION_PDOEVENT
//
NTKERNELAPI
NTSTATUS
ObOpenObjectByPointer(
    IN PVOID Object,
    IN ULONG HandleAttributes,
    IN PACCESS_STATE PassedAccessState OPTIONAL,
    IN ACCESS_MASK DesiredAccess OPTIONAL,
    IN POBJECT_TYPE ObjectType OPTIONAL,
    IN KPROCESSOR_MODE AccessMode,
    OUT PHANDLE Handle
    );

NTKERNELAPI
NTSTATUS
ZwCreateEvent(
    OUT PHANDLE  EventHandle,
    IN ACCESS_MASK  DesiredAccess,
    IN POBJECT_ATTRIBUTES  ObjectAttributes OPTIONAL,
    IN EVENT_TYPE  EventType,
    IN BOOLEAN  InitialState
    );




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
	psrbIoctl->Timeout = 60 * 60;
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
//	if(status == STATUS_SUCCESS) {
		if(OutputBuffer && OutputBufferLength)
			RtlCopyMemory(OutputBuffer, srbIoctlBuffer, OutputBufferLength);
			Bus_KdPrint_Def( BUS_DBG_SS_NOISE, ("%d succeeded!\n", IoctlCode));
//	}
	if(psrbIoctl->ControlCode == STATUS_BUFFER_TOO_SMALL) {
		status = STATUS_BUFFER_TOO_SMALL;
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
	if(LanscsiAdapter->AddDevInfo)
		ExFreePool(LanscsiAdapter->AddDevInfo);

	return STATUS_SUCCESS;
}


//
//	Get physical device's name by querying device properties.
//	Caller must free DeviceName.
//

NTSTATUS
GetPhyDeviceName(
	PDEVICE_OBJECT	PhysicalDeviceObject,
	PWCHAR			*DeviceName,
	PUSHORT			DeviceNameLen

){
	PWCHAR              deviceName = NULL;
	ULONG               nameLength;
	NTSTATUS			status;


	status = IoGetDeviceProperty (	PhysicalDeviceObject,
									DevicePropertyPhysicalDeviceObjectName,
									0,
									NULL,
									&nameLength);

	if (status != STATUS_BUFFER_TOO_SMALL)
	{
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR,("IoGetDeviceProperty failed (0x%x)\n", status));
		goto Error;
	}

	deviceName = ExAllocatePoolWithTag (NonPagedPool, nameLength, BUSENUM_POOL_TAG);

	if (NULL == deviceName) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR,
			("no memory to alloc for deviceName(0x%x)\n", nameLength));
		status =  STATUS_INSUFFICIENT_RESOURCES;
		goto Error;
	}

	status = IoGetDeviceProperty (	PhysicalDeviceObject,
									DevicePropertyPhysicalDeviceObjectName,
									nameLength,
									deviceName,
									&nameLength);

	if (!NT_SUCCESS (status)) {

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("IoGetDeviceProperty(2) failed (0x%x)", status));
		goto Error;
	}

	Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("%x (%ws) \n",
					PhysicalDeviceObject,
					deviceName));

	//
	//	Set return values
	//
	*DeviceName = deviceName;
	*DeviceNameLen = (USHORT)nameLength;


Error:
	if(!NT_SUCCESS(status)) {
		if(deviceName)
			ExFreePool(deviceName);
	}

	return status;
}

//
//	Query information on LanscsiBus
//
NTSTATUS
LSBus_QueryInformation(
		PFDO_DEVICE_DATA				FdoData,
		BOOLEAN							Request32,
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
			Bus_KdPrint_Def(BUS_DBG_SS_TRACE, ("PDO:%p Status:%08lx DevMod:%08lx SupFeat:%08lx EnFeat:%08lx\n",
											PdoData->Self,
											PdoData->LanscsiAdapterPDO.AdapterStatus,
											PdoData->LanscsiAdapterPDO.DeviceMode,
											PdoData->LanscsiAdapterPDO.SupportedFeatures,
											PdoData->LanscsiAdapterPDO.EnabledFeatures
									));
			Information->PdoInfo.AdapterStatus = PdoData->LanscsiAdapterPDO.AdapterStatus;
			Information->PdoInfo.DeviceMode = PdoData->LanscsiAdapterPDO.DeviceMode;
			Information->PdoInfo.SupportedFeatures = PdoData->LanscsiAdapterPDO.SupportedFeatures;
			Information->PdoInfo.EnabledFeatures = PdoData->LanscsiAdapterPDO.EnabledFeatures;
			KeReleaseSpinLock(&PdoData->LanscsiAdapterPDO.LSDevDataSpinLock, oldIrql);

			ObDereferenceObject(PdoData->Self);
		} else {
			Bus_KdPrint_Def(BUS_DBG_SS_NOISE, ("No PDO for SlotNo %d!\n", Query->SlotNo));
			ntStatus = STATUS_NO_SUCH_DEVICE;
		}
		break;
	}
	case INFORMATION_PDOENUM: {
		LARGE_INTEGER			TimeOut;
		ULONG					resultLength;
		DEVICE_INSTALL_STATE	deviceInstallState;

		if(Request32) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PDOENUM: 32-bit thunking not supported.\n"));
			ntStatus = STATUS_INVALID_DEVICE_REQUEST;
			break;
		}

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
		//	Wait until NDAS service sends AddTargetData.
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


		if(PdoData) {

			if(PdoData->LanscsiAdapterPDO.Flags & LSDEVDATA_FLAG_LURDESC) {
				//
				//	A LUR descriptor is set.
				//

				if(PdoData->LanscsiAdapterPDO.AddDevInfo == NULL) {
					KeEnterCriticalRegion();
					ExAcquireFastMutex (&FdoData->Mutex);

					ntStatus = STATUS_NO_SUCH_DEVICE;
					ObDereferenceObject(PdoData->Self);
					break;
				}

				*OutBufferLenNeeded =	FIELD_OFFSET(BUSENUM_INFORMATION, PdoEnumInfo) +
										FIELD_OFFSET(BUSENUM_INFORMATION_PDOENUM, AddDevInfo) +
										PdoData->LanscsiAdapterPDO.AddDevInfoLength;
				if(OutBufferLength < *OutBufferLenNeeded) {
					ntStatus = STATUS_BUFFER_TOO_SMALL;
				} else {
					RtlCopyMemory(
								Information->PdoEnumInfo.AddDevInfo,
								PdoData->LanscsiAdapterPDO.AddDevInfo,
								PdoData->LanscsiAdapterPDO.AddDevInfoLength
							);
					Information->PdoEnumInfo.Flags = PDOENUM_FLAG_LURDESC;
					Information->PdoEnumInfo.DisconEventToService = PdoData->LanscsiAdapterPDO.DisconEventToService;
					Information->PdoEnumInfo.AlarmEventToService = PdoData->LanscsiAdapterPDO.AlarmEventToService;
					Information->PdoEnumInfo.MaxBlocksPerRequest = PdoData->LanscsiAdapterPDO.MaxBlocksPerRequest;


					//
					//	Check to see if this is the first enumeration.
					//

					ntStatus = DrGetDeviceProperty(
						PdoData->Self,
						DevicePropertyInstallState,
						sizeof(deviceInstallState),
						&deviceInstallState,
						&resultLength,
						(Globals.MajorVersion == 5) && (Globals.MinorVersion == 0)
						);
					if(NT_SUCCESS(ntStatus)) {
						if(deviceInstallState != InstallStateInstalled) {
							Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("First time installation. Do not enumerate a device.\n"));
							Information->PdoEnumInfo.Flags |= PDOENUM_FLAG_DRV_NOT_INSTALLED;
						}
					} else {

						//
						//	Even if DrGetDeviceProperty() fails,
						//	we can mount the device successfully.
						//

						ntStatus = STATUS_SUCCESS;
					}
					
				}
			} else {
				//
				//	ADD_TARGET_DATA is set.
				//
				PLANSCSI_ADD_TARGET_DATA	AddTargetData;
				LONG						InfoBuffLenNeeded;

				AddTargetData = PdoData->LanscsiAdapterPDO.AddDevInfo;
				if(AddTargetData == NULL) {
					KeEnterCriticalRegion();
					ExAcquireFastMutex (&FdoData->Mutex);

					ntStatus = STATUS_NO_SUCH_DEVICE;
					ObDereferenceObject(PdoData->Self);
					break;
				}

				//
				//	calculate the length needed.
				//
				InfoBuffLenNeeded = FIELD_OFFSET(BUSENUM_INFORMATION, PdoEnumInfo) +
									FIELD_OFFSET(BUSENUM_INFORMATION_PDOENUM, AddDevInfo) +
									PdoData->LanscsiAdapterPDO.AddDevInfoLength;
				*OutBufferLenNeeded = InfoBuffLenNeeded;
				if(OutBufferLength < InfoBuffLenNeeded) {
					ntStatus = STATUS_BUFFER_TOO_SMALL;
				} else {
					RtlCopyMemory(	Information->PdoEnumInfo.AddDevInfo,
									AddTargetData,
									PdoData->LanscsiAdapterPDO.AddDevInfoLength);
					Information->PdoEnumInfo.Flags = 0;
					Information->PdoEnumInfo.DisconEventToService = PdoData->LanscsiAdapterPDO.DisconEventToService;
					Information->PdoEnumInfo.AlarmEventToService = PdoData->LanscsiAdapterPDO.AlarmEventToService;
					Information->PdoEnumInfo.MaxBlocksPerRequest = PdoData->LanscsiAdapterPDO.MaxBlocksPerRequest;

					//
					//	Check to see if this is the first enumeration.
					//

					ntStatus = DrGetDeviceProperty(
						PdoData->Self,
						DevicePropertyInstallState,
						sizeof(deviceInstallState),
						&deviceInstallState,
						&resultLength,
						(Globals.MajorVersion == 5) && (Globals.MinorVersion == 0)
						);
					if(NT_SUCCESS(ntStatus)) {
						if(deviceInstallState != InstallStateInstalled) {
							Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("First time installation. Do not enumerate a device.\n"));
							Information->PdoEnumInfo.Flags |= PDOENUM_FLAG_DRV_NOT_INSTALLED;
						}
					} else {

						//
						//	Even if DrGetDeviceProperty() fails,
						//	we can mount the device successfully.
						//

						ntStatus = STATUS_SUCCESS;
						Information->PdoEnumInfo.Flags &= ~PDOENUM_FLAG_DRV_NOT_INSTALLED;
					}
				}
			}
		} else {

			ntStatus = STATUS_NO_SUCH_DEVICE;
		}

		KeEnterCriticalRegion();
		ExAcquireFastMutex (&FdoData->Mutex);

		ObDereferenceObject(PdoData->Self);
		break;
	}
	case INFORMATION_PDOEVENT: {

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

		if(PdoData == NULL) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Could not find the PDO:%u.\n", Query->SlotNo));
			ntStatus = STATUS_NO_SUCH_DEVICE;
			break;
		}

		if( Query->Flags & LSBUS_QUERYFLAG_USERHANDLE) {

			if(Request32 == FALSE) {

				Information->PdoEvents.SlotNo = PdoData->SlotNo;
				Information->PdoEvents.Flags = LSBUS_QUERYFLAG_USERHANDLE;

				//
				//	Get user-mode event handles.
				//
				ntStatus = ObOpenObjectByPointer(
						PdoData->LanscsiAdapterPDO.DisconEventToService,
						0,
						NULL,
						EVENT_ALL_ACCESS,
						*ExEventObjectType,
						UserMode,
						&Information->PdoEvents.DisconEvent
					);
				if(!NT_SUCCESS(ntStatus)) {
					ObDereferenceObject(PdoData->Self);
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Could not open Disconnect event object!!\n"));
					break;
				}
				ntStatus = ObOpenObjectByPointer(
						PdoData->LanscsiAdapterPDO.AlarmEventToService,
						0,
						NULL,
						EVENT_ALL_ACCESS,
						*ExEventObjectType,
						UserMode,
						&Information->PdoEvents.AlarmEvent
					);
				if(!NT_SUCCESS(ntStatus)) {
					ObDereferenceObject(PdoData->Self);
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Could not open Disconnect event object!!\n"));
					break;
				}
			} else {
				HANDLE	disconEvent, alarmEvent;

				//
				//	Need 32-bit thunk
				//


				Information->PdoEvents32.SlotNo = PdoData->SlotNo;
				Information->PdoEvents32.Flags = LSBUS_QUERYFLAG_USERHANDLE;

				//
				//	Get user-mode event handles.
				//
				ntStatus = ObOpenObjectByPointer(
						PdoData->LanscsiAdapterPDO.DisconEventToService,
						0,
						NULL,
						EVENT_ALL_ACCESS,
						*ExEventObjectType,
						UserMode,
						&disconEvent
					);
				if(!NT_SUCCESS(ntStatus)) {
					ObDereferenceObject(PdoData->Self);
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Could not open Disconnect event object!!\n"));
					break;
				}
				ntStatus = ObOpenObjectByPointer(
						PdoData->LanscsiAdapterPDO.AlarmEventToService,
						0,
						NULL,
						EVENT_ALL_ACCESS,
						*ExEventObjectType,
						UserMode,
						&alarmEvent
					);
				if(!NT_SUCCESS(ntStatus)) {
					ObDereferenceObject(PdoData->Self);
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Could not open Disconnect event object!!\n"));
					break;
				}


				//
				//	Thunk event handles to 32 bit
				//

				ntStatus = RtlULongPtrToULong((ULONG_PTR)disconEvent,
								(PULONG)&Information->PdoEvents32.DisconEvent);
				if(!NT_SUCCESS(ntStatus)) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PDOEVENTS: error conversion. Handle=%p\n", disconEvent));
					ObDereferenceObject(PdoData->Self);
					break;
				}

				ntStatus = RtlULongPtrToULong((ULONG_PTR)alarmEvent,
								(PULONG)&Information->PdoEvents32.AlarmEvent);
				if(!NT_SUCCESS(ntStatus)) {
					Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PDOEVENTS: error conversion. Handle=%p\n", alarmEvent));
					ObDereferenceObject(PdoData->Self);
					break;
				}
			}

		} else {
			if(Request32) {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PDOEVENTS: 32-bit thunking not supported.\n"));

				ObDereferenceObject(PdoData->Self);
				ntStatus = STATUS_INVALID_DEVICE_REQUEST;
				break;
			}

			Information->PdoEvents.SlotNo = PdoData->SlotNo;
			Information->PdoEvents.Flags = 0;
			Information->PdoEvents.DisconEvent = PdoData->LanscsiAdapterPDO.DisconEventToService;
			Information->PdoEvents.AlarmEvent = PdoData->LanscsiAdapterPDO.AlarmEventToService;
		}

		ObDereferenceObject(PdoData->Self);
		break;
   }
	case INFORMATION_ISREGISTERED: {

		ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

		ntStatus = LSBus_IsRegistered(FdoData, Query->SlotNo);
		break;
	}

	case INFORMATION_PDOSLOTLIST: {
		LONG	outputLength;
		LONG	entryCnt;

		if(OutBufferLength < FIELD_OFFSET(BUSENUM_INFORMATION, PdoSlotList)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PDOSLOTLIST: Buffer size is less than required\n"));
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		Information->Size = 0;
		Information->InfoClass = INFORMATION_PDOSLOTLIST;

		//
		//	Add the size of information header.
		//
		outputLength = FIELD_OFFSET(BUSENUM_INFORMATION, PdoSlotList);
		outputLength += sizeof(UINT32);					// SlotNoCnt
		entryCnt = 0;

		for (entry = FdoData->ListOfPDOs.Flink;
			entry != &FdoData->ListOfPDOs;
			entry = entry->Flink) {

			PdoData = CONTAINING_RECORD(entry, PDO_DEVICE_DATA, Link);

			//
			//	Add the size of each slot entry.
			//
			outputLength += sizeof(UINT32);
			if(outputLength > OutBufferLength) {
				continue;
			}

			Information->PdoSlotList.SlotNo[entryCnt] = PdoData->SlotNo;
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PDOSLOTLIST: Entry #%u: %u\n", entryCnt, PdoData->SlotNo));

			entryCnt ++;
		}

		if(outputLength > OutBufferLength) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PDOSLOTLIST: Could not find a Device(%d).\n", Query->SlotNo));
			*OutBufferLenNeeded = outputLength;
			ntStatus = STATUS_BUFFER_TOO_SMALL;
		} else {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PDOSLOTLIST: Entry count:%d.\n", entryCnt));
			Information->Size = outputLength;
			Information->PdoSlotList.SlotNoCnt = entryCnt;
			*OutBufferLenNeeded = outputLength;
		}

		break;
	}
	case INFORMATION_PDOFILEHANDLE: {
		PWCHAR	devName;
		USHORT	devNameLen;
		OBJECT_ATTRIBUTES  objectAttr;
		UNICODE_STRING		objName;
		HANDLE				devFileHandle;
		IO_STATUS_BLOCK		ioStatus;

		devName = NULL;


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

		if(PdoData == NULL) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Could not find the PDO:%u.\n", Query->SlotNo));
			ntStatus = STATUS_NO_SUCH_DEVICE;
			break;
		}


		//
		//	Get the name of the physical device object.
		//

		ntStatus = GetPhyDeviceName(PdoData->Self, &devName, &devNameLen);
		if(!NT_SUCCESS(ntStatus)) {
			ObDereferenceObject(PdoData->Self);
			break;
		}


		//
		//	Build unicode name of the device.
		//	Get rid of NULL termination from the length
		//

		objName.Buffer = devName;
		if(devNameLen > 0 && devName[devNameLen/sizeof(WCHAR) - 1] == L'\0') {
			objName.MaximumLength = devNameLen;
			objName.Length = devNameLen - sizeof(WCHAR);
		} else {
			objName.MaximumLength = objName.Length = devNameLen;
		}

		//
		//	Open a device file.
		//

		if(Query->Flags & LSBUS_QUERYFLAG_USERHANDLE) {
			if(Request32 == FALSE)
				Information->PdoFile.Flags = LSBUS_QUERYFLAG_USERHANDLE;
			else
				Information->PdoFile32.Flags = LSBUS_QUERYFLAG_USERHANDLE;

			InitializeObjectAttributes(&objectAttr, &objName, OBJ_CASE_INSENSITIVE , NULL, NULL);
		} else {
			if(Request32 == FALSE)
				Information->PdoFile.Flags = 0;
			else
				Information->PdoFile32.Flags = 0;

			InitializeObjectAttributes(&objectAttr, &objName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
		}


		ntStatus = ZwCreateFile(
						&devFileHandle,
						SYNCHRONIZE|FILE_READ_DATA,
						&objectAttr,
						&ioStatus,
						NULL,
						0,
						FILE_SHARE_READ|FILE_SHARE_WRITE,
						FILE_OPEN,
						FILE_SYNCHRONOUS_IO_NONALERT,
						NULL,
						0);
		if(!NT_SUCCESS(ntStatus)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Could not open the PDO:%u. STATUS=%08lx\n", Query->SlotNo, ntStatus));
			ExFreePool(devName);
			ObDereferenceObject(PdoData->Self);
			break;
		}


		//
		//	Set the return values
		//
		if(Request32 == FALSE) {

			Information->PdoFile.SlotNo = PdoData->SlotNo;
			Information->PdoFile.PdoFileHandle = devFileHandle;
		} else {
			
			//
			//	Need 32 bit thunking
			//

			ntStatus = RtlULongPtrToULong((ULONG_PTR)devFileHandle,
								(PULONG)&Information->PdoFile32.PdoFileHandle);
			if(!NT_SUCCESS(ntStatus)) {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PDOEVENTS: error conversion. Handle=%p\n", devFileHandle));
				ObDereferenceObject(PdoData->Self);
				break;
			}

			Information->PdoFile32.SlotNo = PdoData->SlotNo;
		}

		if(devName)
			ExFreePool(devName);

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


//
//	Plug in a device on NDAS bus in KernelMode.
//
NTSTATUS
LSBus_PlugInLSBUSDevice(
			PFDO_DEVICE_DATA				FdoData,
			PBUSENUM_PLUGIN_HARDWARE_EX2	PlugIn
	){
	HANDLE						DisconEvent;
	HANDLE						AlarmEvent;
    NTSTATUS					status;

    //
	//	Create events
    //
	status = ZwCreateEvent(
							&DisconEvent,
							GENERIC_READ,
							NULL,
							NotificationEvent,
							FALSE
						);
	if(!NT_SUCCESS(status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PlugInLSBUSDevice: ZwCreateEvent() failed. Disconnection event.\n"));
		return status;
	}

	status = ZwCreateEvent(
							&AlarmEvent,
							GENERIC_READ,
							NULL,
							NotificationEvent,
							FALSE
						);
    if(!NT_SUCCESS(status)) {
		ZwClose(DisconEvent);
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PlugInLSBUSDevice: ExAllocatePoolWithTag() failed. AlarmEvent\n"));
		return status;
    }

	//
	//	Set up BUSENUM_PLUGIN_HARDWARE_EX2 structure.
	//
	PlugIn->DisconEvent = DisconEvent;
	PlugIn->AlarmEvent = AlarmEvent;

	status = Bus_PlugInDeviceEx2(PlugIn, PlugIn->Size, FdoData, FALSE, KernelMode, TRUE);

	//
	//	Close handle to decrease one reference from events.
	//
	ZwClose(AlarmEvent);
	ZwClose(DisconEvent);

	return status;
}

NTSTATUS
LSBus_PlugOutLSBUSDevice(
			PFDO_DEVICE_DATA	FdoData,
			ULONG				SlotNo,
			BOOLEAN				Persistency
	) {
	BUSENUM_UNPLUG_HARDWARE		UnPlug;
	NTSTATUS	status;
	PPDO_DEVICE_DATA			pdoData;

	//
	//	Set/Reset persistency.
	//

	pdoData = LookupPdoData(FdoData, SlotNo);
	if(pdoData) {
		pdoData->Persistent = Persistency;
		ObDereferenceObject(pdoData->Self);
	} else {
		return STATUS_NOT_FOUND;
	}

	//
	//	Unplug
	//

	UnPlug.Size = sizeof(BUSENUM_UNPLUG_HARDWARE);
	UnPlug.SlotNo = SlotNo;

	status = Bus_UnPlugDevice (
							&UnPlug,
							FdoData
					);

		return status;
	}

//
//	Plug in a NDAS device with a LUR descriptor.
//
NTSTATUS
LSBus_AddTargetWithLurDesc(
		PFDO_DEVICE_DATA	FdoData,
		PLURELATION_DESC	LurDesc,
		ULONG				SlotNo
) {
	PPDO_DEVICE_DATA	pdoData;
	NTSTATUS			status;

	status = STATUS_SUCCESS;
	//
	//	Verify LurDesc
	//	TODO:
	//
	
	// Find Pdo Data...
	pdoData = LookupPdoData(FdoData, SlotNo);
	if(pdoData == NULL) {
		Bus_KdPrint_Cont (FdoData, BUS_DBG_IOCTL_ERROR, ("no pdo\n"));
		return STATUS_NOT_FOUND;
}

	//
	//	Copy AddDevInfo into the PDO.
	//
	if(pdoData->LanscsiAdapterPDO.AddDevInfo) {
		Bus_KdPrint_Cont (FdoData, BUS_DBG_IOCTL_ERROR, ("AddDevInfo already set.\n"));

		status = STATUS_DEVICE_ALREADY_ATTACHED;
		goto cleanup;
	}
	pdoData->LanscsiAdapterPDO.AddDevInfo = ExAllocatePool(NonPagedPool, LurDesc->Length);
	if(pdoData->LanscsiAdapterPDO.AddDevInfo == NULL) {
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto cleanup;
	} else {
		RtlCopyMemory(pdoData->LanscsiAdapterPDO.AddDevInfo, LurDesc, LurDesc->Length);
		status = STATUS_SUCCESS;
	}
	pdoData->LanscsiAdapterPDO.AddDevInfoLength = LurDesc->Length;
	pdoData->LanscsiAdapterPDO.Flags |= LSDEVDATA_FLAG_LURDESC;

	//
	//	Notify to LanscsiMiniport
	//
	Bus_KdPrint(FdoData, BUS_DBG_IOCTL_ERROR, ("IOCTL_LANSCSI_ADD_TARGET SetEvent AddTargetEvent!\n"));
	KeSetEvent(&pdoData->LanscsiAdapterPDO.AddTargetEvent, IO_NO_INCREMENT, FALSE);

cleanup:
	ObDereferenceObject(pdoData->Self);

	return status;
}

//
//	Plug in a NDAS device with a LUR descriptor.
//
NTSTATUS
LSBus_AddTarget(
		PFDO_DEVICE_DATA			FdoData,
		PLANSCSI_ADD_TARGET_DATA	AddTargetData
) {
	PPDO_DEVICE_DATA	pdoData;
	NTSTATUS			status;

	status = STATUS_SUCCESS;
	//
	//	Verify LurDesc
	//	TODO:
	//
	
	// Find Pdo Data...
	pdoData = LookupPdoData(FdoData, AddTargetData->ulSlotNo);
	if(pdoData == NULL) {
		Bus_KdPrint_Cont (FdoData, BUS_DBG_IOCTL_ERROR, ("no pdo for Slot %u\n", AddTargetData->ulSlotNo));
		return STATUS_NOT_FOUND;
	}

	//
	//	Copy AddDevInfo into the PDO.
	//
	if(pdoData->LanscsiAdapterPDO.AddDevInfo) {
		Bus_KdPrint_Cont (FdoData, BUS_DBG_IOCTL_ERROR, ("AddDevInfo already set.\n"));

		status = STATUS_DEVICE_ALREADY_ATTACHED;
		goto cleanup;
	}
	pdoData->LanscsiAdapterPDO.AddDevInfo = ExAllocatePool(NonPagedPool, AddTargetData->ulSize);
	if(pdoData->LanscsiAdapterPDO.AddDevInfo == NULL) {
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto cleanup;
	}

	RtlCopyMemory(pdoData->LanscsiAdapterPDO.AddDevInfo, AddTargetData, AddTargetData->ulSize);
	pdoData->LanscsiAdapterPDO.AddDevInfoLength = AddTargetData->ulSize;

	//
	//	Notify to LanscsiMiniport
	//
	Bus_KdPrint(FdoData, BUS_DBG_IOCTL_ERROR, ("IOCTL_LANSCSI_ADD_TARGET SetEvent AddTargetEvent!\n"));
	KeSetEvent(&pdoData->LanscsiAdapterPDO.AddTargetEvent, IO_NO_INCREMENT, FALSE);

cleanup:
	ObDereferenceObject(pdoData->Self);

	return status;
}

