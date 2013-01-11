#include "ndasport.h"
#include "trace.h"
#include "ndasdlu.h"
#include "ndasdluioctl.h"
#include <ntdddisk.h>
#include "constants.h"

#include <initguid.h>
#include "ndasdluguid.h"
#include "lslur.h"

#ifdef RUN_WPP
#include "ndasdlu.tmh"
#endif


#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "DLUIOCTL"

//
// Query Adapter, LURs and LURNs information
//
// NOTE: This routine executes in the context of ScsiPort synchronized callback.
//

NTSTATUS
NdasDluSrbIoctlQueryInfo(
		PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension,
		PNDAS_DLU_EXTENSION			DluLuExtension,
		PSCSI_REQUEST_BLOCK			Srb,
		PNDASSCSI_QUERY_INFO_DATA	QueryInfo,
		ULONG						OutputBufferLength,
		PUCHAR						OutputBuffer,
		PULONG						SrbIoctlReturnCode,
		PUCHAR						SrbStatus
) {
	NTSTATUS	status;
	PCCB		Ccb;
//	KIRQL		oldIrql;


	UNREFERENCED_PARAMETER(LogicalUnitExtension);
	UNREFERENCED_PARAMETER(DluLuExtension);

	*SrbIoctlReturnCode = SRB_STATUS_SUCCESS;
	*SrbStatus = SRB_STATUS_SUCCESS;
	status = STATUS_SUCCESS;
	switch(QueryInfo->InfoClass) {

	case NdscPrimaryUnitDiskInformation: {
		PNDSCIOCTL_PRIMUNITDISKINFO	primUnitDisk = (PNDSCIOCTL_PRIMUNITDISKINFO)OutputBuffer;
		PLUR_QUERY					lurQuery;
		PLURN_PRIMARYINFORMATION	lurPrimaryInfo;
		PBYTE						lurBuffer;

		KDPrint(1,("NdscPrimaryUnitDiskInformation\n"));

		if(OutputBufferLength < sizeof(NDSCIOCTL_PRIMUNITDISKINFO)) {
			KDPrint(1,("Too small output buffer\n"));
			*SrbStatus = SRB_STATUS_INVALID_REQUEST;
			status = STATUS_BUFFER_TOO_SMALL;
			if (OutputBufferLength > sizeof(UINT32)) 
				primUnitDisk->Length					= sizeof(NDSCIOCTL_PRIMUNITDISKINFO);
			break;
		}

		//
		//	Query to the LUR
		//
		status = LsCcbAllocate(&Ccb);
		if(!NT_SUCCESS(status)) {
			KDPrint(1,("LsCcbAllocate() failed.\n"));
			*SrbStatus = SRB_STATUS_INVALID_REQUEST;
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		lurBuffer = ExAllocatePoolWithTag(
							NonPagedPool,
							SIZE_OF_LURQUERY(0, sizeof(LURN_PRIMARYINFORMATION)),
							LURN_IOCTL_POOL_TAG);
		if(lurBuffer == NULL) {
			KDPrint(1,("LsCcbAllocate() failed.\n"));
			*SrbStatus = SRB_STATUS_INVALID_REQUEST;
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		//
		// Set up asynchronous CCB
		//

		LSCCB_INITIALIZE(Ccb);
		Ccb->Srb = Srb;
		Ccb->OperationCode = CCB_OPCODE_QUERY;
		LsCcbSetFlag(Ccb, CCB_FLAG_ALLOCATED|CCB_FLAG_DATABUF_ALLOCATED);
		Ccb->DataBuffer = lurBuffer;
		Ccb->DataBufferLength = sizeof(LURN_PRIMARYINFORMATION);
		LsCcbSetCompletionRoutine(Ccb, NdasDluCcbCompletion, DluLuExtension);
		InterlockedIncrement(&DluLuExtension->RequestExecuting);
		LsuIncrementTdiClientInProgress();

		lurQuery = (PLUR_QUERY)lurBuffer;
		lurQuery->InfoClass			= LurPrimaryLurnInformation;
		lurQuery->Length			= SIZE_OF_LURQUERY(0, sizeof(LURN_PRIMARYINFORMATION));
		lurQuery->QueryDataLength	= 0;

		lurPrimaryInfo = (PLURN_PRIMARYINFORMATION)LUR_QUERY_INFORMATION(lurQuery);

		if(DluLuExtension->LUR == NULL) {
			LsuDecrementTdiClientInProgress();
			InterlockedDecrement(&DluLuExtension->RequestExecuting);
			ExFreePoolWithTag(lurBuffer, LURN_IOCTL_POOL_TAG);
			LsCcbFree(Ccb);

			*SrbStatus = SRB_STATUS_INVALID_REQUEST;
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		status = LurRequest(
							DluLuExtension->LUR,
							Ccb
						);
		if(!NT_SUCCESS(status)) {
			LsuDecrementTdiClientInProgress();
			InterlockedDecrement(&DluLuExtension->RequestExecuting);
			ExFreePoolWithTag(lurBuffer, LURN_IOCTL_POOL_TAG);
			LsCcbFree(Ccb);

			KDPrint(1,("LurRequest() failed.\n"));
			*SrbStatus = SRB_STATUS_INVALID_REQUEST;
			status = STATUS_INVALID_PARAMETER;
			break;
		}
		status = STATUS_PENDING;
		break;
		}

	case NdscLurInformation: {
		PNDSCIOCTL_LURINFO	info = (PNDSCIOCTL_LURINFO)OutputBuffer;
		PLUR_QUERY			lurQuery;
		UINT32				lurnEnumInfoLen;
		UINT32				nodeCount;

		KDPrint(1,("NdscLurInformation\n"));

		if(OutputBufferLength < FIELD_OFFSET(NDSCIOCTL_LURINFO, Lurns)) {
			KDPrint(2,("LurInfo: Buffer size %d is less than required %d bytes\n", OutputBufferLength, FIELD_OFFSET(NDSCIOCTL_LURINFO, Lurns)));
			*SrbStatus = SRB_STATUS_INVALID_REQUEST;
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		//
		//	allocate a CCB
		//
		status = LsCcbAllocate(&Ccb);
		if(!NT_SUCCESS(status)) {
			KDPrint(1,("LsCcbAllocate() failed.\n"));
			*SrbStatus = SRB_STATUS_INVALID_REQUEST;
			break;
		}

		//
		//	initialize query CCB
		//
		LSCCB_INITIALIZE(Ccb);
		Ccb->Srb = Srb;
		Ccb->OperationCode = CCB_OPCODE_QUERY;
		LsCcbSetFlag(Ccb, CCB_FLAG_ALLOCATED|CCB_FLAG_DATABUF_ALLOCATED);

		if (DluLuExtension->LUR) {
			nodeCount = DluLuExtension->LUR->NodeCount;
		} else {
			nodeCount = 0;
			ASSERT(FALSE);
		}

		lurnEnumInfoLen = FIELD_OFFSET(LURN_ENUM_INFORMATION, Lurns) + sizeof(LURN_INFORMATION) * nodeCount;
		// Allocate memory and initialize it.
		LUR_QUERY_INITIALIZE(lurQuery, LurEnumerateLurn, 0, lurnEnumInfoLen);
		if(!lurQuery) {
			LsCcbFree(Ccb);

			KDPrint(1,("allocating DataBuffer failed.\n"));
			*SrbStatus = SRB_STATUS_INVALID_REQUEST;
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		Ccb->DataBuffer = lurQuery;
		Ccb->DataBufferLength = lurQuery->Length;

		//
		// Set completion routine
		//

		LsCcbSetCompletionRoutine(Ccb, NdasDluCcbCompletion, DluLuExtension);
		InterlockedIncrement(&DluLuExtension->RequestExecuting);
		LsuIncrementTdiClientInProgress();

		//
		//	send the CCB down
		//
		status = LurRequest(
							DluLuExtension->LUR,
							Ccb
						);
		if(!NT_SUCCESS(status)) {
			InterlockedDecrement(&DluLuExtension->RequestExecuting);
			LsuDecrementTdiClientInProgress();
			LsCcbFree(Ccb);

			KDPrint(1,("LurRequest() failed.\n"));
			ExFreePoolWithTag(lurQuery, NDSC_PTAG_IOCTL);
			*SrbStatus = SRB_STATUS_INVALID_REQUEST;
			status = STATUS_INVALID_PARAMETER;
			break;
		}
		status = STATUS_PENDING;
		break;
		}
	case NdscSystemBacl:
	case NdscUserBacl: {
		ULONG			requiredBufLen;
		PNDAS_BLOCK_ACL	ndasBacl = (PNDAS_BLOCK_ACL)OutputBuffer;
		PLSU_BLOCK_ACL	targetBacl;


		KDPrint(1,("NdscSystemBacl/UserBacl: going to default LuExtention 0.\n"));
		if(DluLuExtension->LUR == NULL) {
			*SrbStatus = SRB_STATUS_INVALID_REQUEST;
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		if(QueryInfo->InfoClass == NdscSystemBacl)
			targetBacl = &DluLuExtension->LUR->SystemBacl;
		else
			targetBacl = &DluLuExtension->LUR->UserBacl;

		status = LsuConvertLsuBaclToNdasBacl(
								ndasBacl, 
								OutputBufferLength,
								&requiredBufLen,
								targetBacl);

		if(status == STATUS_BUFFER_TOO_SMALL) {
			//
			//	Set required field.
			//
			if(OutputBufferLength >= FIELD_OFFSET(NDAS_BLOCK_ACL, BlockACECnt)) {
				ndasBacl->Length = requiredBufLen;
			}
			//
			//	Set error code to the srb ioctl, but return success
			//
			*SrbIoctlReturnCode = SRB_STATUS_DATA_OVERRUN;

		} else if(!NT_SUCCESS(status)) {
			*SrbStatus = SRB_STATUS_ERROR;
			break;
		}

		*SrbStatus = SRB_STATUS_SUCCESS;
		status = STATUS_SUCCESS;
		break;
	}
	default:
		KDPrint(1,("Invalid Information Class!!\n"));
		*SrbStatus = SRB_STATUS_INVALID_REQUEST;
		status = STATUS_INVALID_PARAMETER;
	}

	return status;
}


//
// NOTE: This routine executes in the context of ScsiPort synchronized callback.
//

UCHAR
NdasDluSrbIoctlGetDVDSTatus(
		PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension,
		PNDAS_DLU_EXTENSION			DluLuExtension,
		ULONG						OutputBufferLength,
		PUCHAR						OutputBuffer,
		PNTSTATUS					NtStatus
		
) {
	UCHAR				status;
	PCCB				Ccb;
	NTSTATUS			ntStatus;


	PNDASBUS_DVD_STATUS DvdStatusInfo = (PNDASBUS_DVD_STATUS)OutputBuffer; 
	BYTE				LurBuffer[sizeof(LURN_DVD_STATUS)];
	PLURN_DVD_STATUS	pDvdStatusHeader;


	UNREFERENCED_PARAMETER(LogicalUnitExtension);

	KDPrint(1,("\n"));

	if(OutputBufferLength < sizeof(NDASBUS_DVD_STATUS)) {
		KDPrint(1,("Too small output buffer\n"));
		*NtStatus = STATUS_BUFFER_TOO_SMALL;
		status = SRB_STATUS_INVALID_REQUEST;
		return status;
	}

	//
	//	Query to the LUR
	//
	ntStatus = LsCcbAllocate(&Ccb);
	if(!NT_SUCCESS(ntStatus)) {
		KDPrint(1,("LsCcbAllocate() failed.\n"));
		*NtStatus = STATUS_INSUFFICIENT_RESOURCES;
		status = SRB_STATUS_INVALID_REQUEST;
		return status;
	}

	Ccb->OperationCode = CCB_OPCODE_DVD_STATUS;
	LsCcbSetFlag(Ccb, CCB_FLAG_SYNCHRONOUS|CCB_FLAG_ALLOCATED);
	Ccb->DataBuffer = LurBuffer;
	Ccb->DataBufferLength = sizeof(LURN_DVD_STATUS);
	
	pDvdStatusHeader = (PLURN_DVD_STATUS)LurBuffer;
	pDvdStatusHeader->Length = sizeof(LURN_DVD_STATUS);

	ntStatus = LurRequest(
						DluLuExtension->LUR,
						Ccb
					);

	if(!NT_SUCCESS(ntStatus)) {
		LsCcbFree(Ccb);

		KDPrint(1,("LurnRequest() failed.\n"));
		*NtStatus = STATUS_INSUFFICIENT_RESOURCES;
		status = SRB_STATUS_INVALID_REQUEST;
		return status;
	}
		
	//
	//	Set return values.
	//
	KDPrint(1,("Last Access Time :%I64d\n",pDvdStatusHeader->Last_Access_Time.QuadPart));
	KDPrint(1,("Result  %d\n",pDvdStatusHeader->Status));
	DvdStatusInfo->Status = pDvdStatusHeader->Status;
	*NtStatus = STATUS_SUCCESS;
	return SRB_STATUS_SUCCESS;
}


NTSTATUS
NdasDluSrbIoctlSendCCBWithComp(
	IN PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension,
	IN PNDAS_DLU_EXTENSION			NdasDluLuExtension,
	IN PSCSI_REQUEST_BLOCK			Srb,
	IN UINT32						CcbOpCode,
	IN PVOID						CmdBuffer,
	IN ULONG						CmdBufferLen
){
	NTSTATUS	status;
	PCCB		ccb;


	//
	//	Query to the LUR
	//

	status = LsCcbAllocate(&ccb);
	if(!NT_SUCCESS(status)) {
		Srb->SrbStatus = SRB_STATUS_ERROR;
		KDPrint(1,("LsCcbAllocate() failed.\n"));
		return STATUS_SUCCESS;
	}

	LSCCB_INITIALIZE(ccb);
	ccb->OperationCode = CcbOpCode;
	LsCcbSetFlag(ccb, CCB_FLAG_ALLOCATED);
	ccb->Srb = Srb;
	ccb->DataBufferLength = CmdBufferLen;
	ccb->DataBuffer = CmdBuffer;
	LsCcbSetCompletionRoutine(ccb, NdasDluCcbCompletion, LogicalUnitExtension);

	InterlockedIncrement(&NdasDluLuExtension->RequestExecuting);
	LsuIncrementTdiClientInProgress();
	status = LurRequest(
		NdasDluLuExtension->LUR,
		ccb
		);
	if(!NT_SUCCESS(status)) {
		LsCcbFree(ccb);
		Srb->SrbStatus = SRB_STATUS_ERROR;
		LsuDecrementTdiClientInProgress();
		KDPrint(1,("LurRequest() failed.\n"));
		return STATUS_SUCCESS;
	}


	return STATUS_PENDING;
}


//
// This is the SMART SRB_IOCONTROL handler.
//

NTSTATUS
NdasDluDiskClassControl(
	IN PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension,
	IN PNDAS_DLU_EXTENSION			NdasDluLuExtension,
	IN PSCSI_REQUEST_BLOCK			Srb
){
	//
	// The request is an I/O control request. The SRB DataBuffer points 
	// to an SRB_IO_CONTROL header followed by the data area. The value 
	// in DataBuffer can be used by the driver, regardless of the value 
	// of MapBuffers field. If the HBA miniport driver supports this 
	// request, it should execute the request and notify the OS-specific 
	// port driver when it has completed it, using the normal mechanism 
	// of NdasPortNotification with RequestComplete and NextLuRequest. 
	// Only the Function, SrbFlags, TimeOutValue, DataBuffer, 
	// DataTransferLength and SrbExtension are valid. 
	//
	NTSTATUS		status;
	PSRB_IO_CONTROL srbIoControl;
	PUCHAR			srbIoctlBuffer;
	LONG			srbIoctlBufferLength;

	status = STATUS_SUCCESS;

	KDPrint(3,("Entered.\n"));

	//
	// Get the Ioctl buffer. If this is a send message request, it gets
	// fixed up to be an I2O message later.
	//
	srbIoControl = (PSRB_IO_CONTROL) Srb->DataBuffer;
	srbIoctlBuffer = ((PUCHAR)Srb->DataBuffer) + sizeof(SRB_IO_CONTROL);
	srbIoctlBufferLength = srbIoControl->Length;

#define IOCTL_SCSI_MINIPORT_READ_SMART_LOG          ((FILE_DEVICE_SCSI << 16) + 0x050b)
#define IOCTL_SCSI_MINIPORT_WRITE_SMART_LOG         ((FILE_DEVICE_SCSI << 16) + 0x050c)

	switch (srbIoControl->ControlCode)
	{
	//
	// support for Self-Monitoring Analysis and Reporting Technology (SMART)
	//
	case IOCTL_SCSI_MINIPORT_IDENTIFY:
	case IOCTL_SCSI_MINIPORT_READ_SMART_ATTRIBS:
	case IOCTL_SCSI_MINIPORT_READ_SMART_THRESHOLDS:
	case IOCTL_SCSI_MINIPORT_ENABLE_SMART:
	case IOCTL_SCSI_MINIPORT_DISABLE_SMART:
	case IOCTL_SCSI_MINIPORT_RETURN_STATUS:
	case IOCTL_SCSI_MINIPORT_ENABLE_DISABLE_AUTOSAVE:
	case IOCTL_SCSI_MINIPORT_SAVE_ATTRIBUTE_VALUES:
	case IOCTL_SCSI_MINIPORT_EXECUTE_OFFLINE_DIAGS:
	case IOCTL_SCSI_MINIPORT_ENABLE_DISABLE_AUTO_OFFLINE:
	case IOCTL_SCSI_MINIPORT_READ_SMART_LOG:
	case IOCTL_SCSI_MINIPORT_WRITE_SMART_LOG:
		status = NdasDluSrbIoctlSendCCBWithComp(
						LogicalUnitExtension,
						NdasDluLuExtension,
						Srb,
						CCB_OPCODE_SMART,
						srbIoctlBuffer,
						srbIoctlBufferLength);
		break;
	case IOCTL_SCSI_MINIPORT_SMART_VERSION:
		{
			PGETVERSIONINPARAMS getVersion;

			getVersion = (PGETVERSIONINPARAMS)srbIoctlBuffer;

			//
			// SMART 1.03
			//
			getVersion->bVersion = 1;
			getVersion->bRevision = 1;
			getVersion->bReserved = 0;

			//
			// TODO: Add CAP_ATAPI_ID_CMD
			//
			getVersion->fCapabilities = CAP_ATA_ID_CMD | CAP_SMART_CMD;
			//
			// Regardless of unit number, we exposes the logical unit
			// as a pseudo ATA primary master
			//
			getVersion->bIDEDeviceMap = 1;

			Srb->SrbStatus = SRB_STATUS_SUCCESS;
			Srb->DataTransferLength = 
				sizeof(SRB_IO_CONTROL) +
				sizeof(GETVERSIONINPARAMS);
		}
		break;
		//
		// Cluster Support
		//
	case IOCTL_SCSI_MINIPORT_NOT_QUORUM_CAPABLE:
	case IOCTL_SCSI_MINIPORT_NOT_CLUSTER_CAPABLE:
	default:
		Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
		Srb->DataTransferLength = 0;
		break;
	}

	return status;
}

//
// Device lock control
//

NTSTATUS
NdasDluIoctlDeviceLock(
	IN PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension,
	IN PNDAS_DLU_EXTENSION			DluLuExtension,
	IN PNDSCIOCTL_DEVICELOCK		DeviceLockControl,
	IN PSCSI_REQUEST_BLOCK			Srb
){
	NTSTATUS				status;
	PLURN_DEVLOCK_CONTROL	lurnDeviceLock;
	PCCB					ccb;
	PNDAS_DLU_EXTENSION		ndasDluExtension = NdasDluGetExtension(LogicalUnitExtension);


	//
	//	Create a CCB
	//
	status = LsCcbAllocate(&ccb);
	if(!NT_SUCCESS(status)) {
		Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
		KDPrint(1,("LsCcbAllocate() failed.\n"));
		return status;
	}
	lurnDeviceLock = (PLURN_DEVLOCK_CONTROL)ExAllocatePoolWithTag(NonPagedPool, sizeof(LURN_DEVLOCK_CONTROL), NDAS_DLU_PTAG_IOCTL);
	if(!lurnDeviceLock) {
		Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
		KDPrint(1,("ExAllocatePoolWithTag() failed.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	LSCCB_INITIALIZE(ccb);
	ccb->OperationCode = CCB_OPCODE_DEVLOCK;
	ccb->DataBuffer = lurnDeviceLock;
	ccb->DataBufferLength = sizeof(LURN_DEVLOCK_CONTROL);
	LsCcbSetFlag(ccb, CCB_FLAG_ALLOCATED|CCB_FLAG_DATABUF_ALLOCATED);

	//	Ioctl Srb will complete asynchronously.
	ccb->Srb = Srb;
	InterlockedIncrement(&DluLuExtension->RequestExecuting);

	//
	// Increment in-progress count
	//

	LsuIncrementTdiClientInProgress();
	LsCcbSetCompletionRoutine(ccb, NdasDluCcbCompletion, LogicalUnitExtension);

	//
	// Set up request
	//

	lurnDeviceLock->LockId = DeviceLockControl->LockId;
	lurnDeviceLock->LockOpCode = DeviceLockControl->LockOpCode;
	lurnDeviceLock->AdvancedLock	= DeviceLockControl->AdvancedLock;
	lurnDeviceLock->AddressRangeValid	= DeviceLockControl->AddressRangeValid;
	lurnDeviceLock->RequireLockAcquisition	= DeviceLockControl->RequireLockAcquisition;
	lurnDeviceLock->StartingAddress = DeviceLockControl->StartingAddress;
	lurnDeviceLock->ContentionTimeOut = DeviceLockControl->ContentionTimeOut;
	RtlCopyMemory(lurnDeviceLock->LockData, DeviceLockControl->LockData, 
		NDSCLOCK_LOCKDATA_LENGTH);
	ASSERT(NDSCLOCK_LOCKDATA_LENGTH == LURNDEVLOCK_LOCKDATA_LENGTH);

	//
	// Send the request
	//

	status = LurRequest(
		ndasDluExtension->LUR,
		ccb
		);
	if(!NT_SUCCESS(status)) {
		KDPrint(1,("LurnRequest() failed.\n"));
		LsCcbFree(ccb);
		ExFreePoolWithTag(lurnDeviceLock, NDSC_PTAG_IOCTL);
		status = STATUS_SUCCESS;
		Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
	} else {
		status = STATUS_PENDING;
	}

	return status;
}

//
//
// This is the SRB_IO_CONTROL handler for this driver. These requests come from
// the management driver.
//
// Arguments:
//
//   DeviceExtension - Context
//   Srb - The request to process.
//
// Return Value:
//
//   Value from the helper routines, or if handled in-line, SUCCESS or INVALID_REQUEST. 
//
// NOTE: This routine executes in the context of ScsiPort synchronized callback.
//

NTSTATUS
NdasDluSrbControl(
	   IN PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension,
	   IN PSCSI_REQUEST_BLOCK			Srb
	)
{
    PSRB_IO_CONTROL	srbIoControl;
    PUCHAR			srbIoctlBuffer;
	LONG			srbIoctlBufferLength;
    ULONG			controlCode;
    NTSTATUS		status;
    UCHAR			srbStatus;
	PNDAS_DLU_EXTENSION			ndasDluExtension = NdasDluGetExtension(LogicalUnitExtension);

	//
    // Start off being paranoid.
    //
    if (Srb->DataBuffer == NULL) {
		KDPrint(1,("DataBuffer is NULL\n"));
		Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
        return STATUS_INVALID_PARAMETER;
    }
	status = STATUS_MORE_PROCESSING_REQUIRED;
	srbStatus = SRB_STATUS_SUCCESS;

	//
    // Extract the io_control
    //
    srbIoControl = (PSRB_IO_CONTROL)Srb->DataBuffer;

	//
	// Based on the signature, determine if this is Disk class ioctl or our own's those.
	//

	if (strncmp(srbIoControl->Signature, NDASSCSI_IOCTL_SIGNATURE, 8) == 0) {
		//
		// NDAS MINIPORT own's.
		// Continue to the parsing and execution in this function.
		//

	} else if(strncmp(srbIoControl->Signature, "SCSIDISK", 8) == 0) {
#if 1
		//
		// Disk class request
		// Complete the SRB in the disk class ioctl function.
		//

		return NdasDluDiskClassControl(LogicalUnitExtension, ndasDluExtension, Srb);
#else
		KDPrint(1,("Disk class control disabled.\n"));
		Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
		return STATUS_INVALID_PARAMETER;
#endif
	} else {

		KDPrint(1,("Signature mismatch %8s, %8s\n", srbIoControl->Signature, NDASSCSI_IOCTL_SIGNATURE));
		Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Get the control code.
    // 
    controlCode = srbIoControl->ControlCode;

    //
    // Get the Ioctl buffer. If this is a send message request, it gets
    // fixed up to be an I2O message later.
    //
    srbIoctlBuffer = ((PUCHAR)Srb->DataBuffer) + sizeof(SRB_IO_CONTROL);
	srbIoctlBufferLength = srbIoControl->Length;

    //
    // Based on the control code, figure out what to do.
    //
    switch (controlCode) {
		case NDASSCSI_IOCTL_GET_DVD_STATUS:
			{
				srbStatus = NdasDluSrbIoctlGetDVDSTatus(
								LogicalUnitExtension,
								ndasDluExtension,
								srbIoctlBufferLength,
								srbIoctlBuffer,
								&srbIoControl->ReturnCode
								);
				if(srbStatus == SRB_STATUS_SUCCESS) {
					KDPrint(4,("NDASSCSI_IOCTL_QUERYINFO_EX: Successful.\n"));
					status = STATUS_SUCCESS;
				} else {
					status = STATUS_UNSUCCESSFUL;
				}								
			}
			break;
		case NDASSCSI_IOCTL_GET_SLOT_NO:
            KDPrint(2,("Get Slot No. Slot number is %d\n", ndasDluExtension->LogicalUnitAddress));

			if(srbIoctlBufferLength < sizeof(ULONG)) {
				srbIoControl->ReturnCode = SRB_STATUS_DATA_OVERRUN;
				srbStatus = SRB_STATUS_DATA_OVERRUN;
				status = STATUS_UNSUCCESSFUL;
				break;
			}

			*(PULONG)srbIoctlBuffer = ndasDluExtension->LogicalUnitAddress;
			srbIoControl->ReturnCode = SRB_STATUS_SUCCESS;

			srbStatus = SRB_STATUS_SUCCESS;
			status = STATUS_SUCCESS;
            break;

		case NDASSCSI_IOCTL_QUERYINFO_EX: {
			PNDASSCSI_QUERY_INFO_DATA	QueryInfo;
			PUCHAR tmpBuffer;
            KDPrint(5, ("Query information EX.\n"));

			QueryInfo = (PNDASSCSI_QUERY_INFO_DATA)srbIoctlBuffer;

			tmpBuffer = ExAllocatePoolWithTag(NonPagedPool, srbIoctlBufferLength, NDAS_DLU_PTAG_IOCTL);
			if(tmpBuffer == NULL) {
				ASSERT(FALSE);
	            KDPrint(1,("NDASSCSI_IOCTL_QUERYINFO_EX: SRB_STATUS_DATA_OVERRUN. BufferLength:%d\n", srbIoctlBufferLength));
				srbStatus = SRB_STATUS_DATA_OVERRUN;
				break;
			}

			status = NdasDluSrbIoctlQueryInfo(
							LogicalUnitExtension,
							ndasDluExtension,
							Srb,
							QueryInfo,
							srbIoctlBufferLength,
							tmpBuffer,
							&srbIoControl->ReturnCode,
							&srbStatus
						);
//			if(NT_SUCCESS(status)) {
//	            KDPrint(4,("NDASSCSI_IOCTL_QUERYINFO_EX: Successful.\n"));
				// Need to fill structure anyway.
				RtlCopyMemory(srbIoctlBuffer, tmpBuffer, srbIoctlBufferLength);
//			}

			ExFreePoolWithTag(tmpBuffer, NDSC_PTAG_IOCTL);
			break;
		}
		case NDASSCSI_IOCTL_UPGRADETOWRITE: {
			PLURN_UPDATE		LurnUpdate;
			PCCB				Ccb;

			//
			//	Set a CCB
			//
			status = LsCcbAllocate(&Ccb);
			if(!NT_SUCCESS(status)) {
				srbStatus = SRB_STATUS_INVALID_REQUEST;
				KDPrint(1,("NDASSCSI_IOCTL_UPGRADETOWRITE: LsCcbAllocate() failed.\n"));
				break;
			}
			LurnUpdate = (PLURN_UPDATE)ExAllocatePoolWithTag(NonPagedPool, sizeof(LURN_UPDATE), NDAS_DLU_PTAG_IOCTL);
			if(!LurnUpdate) {
				srbStatus = SRB_STATUS_INVALID_REQUEST;
				KDPrint(1,("NDASSCSI_IOCTL_UPGRADETOWRITE: ExAllocatePoolWithTag() failed.\n"));
				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}

			LSCCB_INITIALIZE(Ccb);
			Ccb->OperationCode = CCB_OPCODE_UPDATE;
			Ccb->DataBuffer = LurnUpdate;
			Ccb->DataBufferLength = sizeof(LURN_UPDATE);
			LsCcbSetFlag(Ccb, CCB_FLAG_ALLOCATED|CCB_FLAG_DATABUF_ALLOCATED);

			//	Ioctl Srb will complete asynchronously.
			Ccb->Srb = Srb;
			InterlockedIncrement(&ndasDluExtension->RequestExecuting);
			LsuIncrementTdiClientInProgress();
			LsCcbSetCompletionRoutine(Ccb, NdasDluCcbCompletion, LogicalUnitExtension);

			LurnUpdate->UpdateClass = LURN_UPDATECLASS_WRITEACCESS_USERID;

			status = LurRequest(
								ndasDluExtension->LUR,
								Ccb
							);
			if(!NT_SUCCESS(status)) {
				KDPrint(1,("NDASSCSI_IOCTL_UPGRADETOWRITE:  LurnRequest() failed.\n"));

				NdasDluLogError(
						LogicalUnitExtension,
						Srb,
						Srb->PathId,
						Srb->TargetId,
						Srb->Lun,
						NDASSCSI_IO_UPGRADEIOCTL_FAIL,
						EVTLOG_UNIQUEID(EVTLOG_MODULE_IOCTL, EVTLOG_FAIL_UPGRADEIOCTL, 0)
					);
				LsuDecrementTdiClientInProgress();
				LsCcbFree(Ccb);
				ExFreePoolWithTag(LurnUpdate, NDSC_PTAG_IOCTL);
				status = STATUS_SUCCESS;
				srbStatus = SRB_STATUS_INVALID_REQUEST;
			} else {
				status = STATUS_PENDING;
			}
			break;
		}

		case NDASSCSI_IOCTL_NOOP: {
			PCCB	Ccb;

			//
			//	Query to the LUR
			//
			status = LsCcbAllocate(&Ccb);
			if(!NT_SUCCESS(status)) {
				srbStatus = SRB_STATUS_ERROR;
				status = STATUS_SUCCESS;
				KDPrint(1,("LsCcbAllocate() failed.\n"));
				break;
			}

			LSCCB_INITIALIZE(Ccb);
			Ccb->OperationCode = CCB_OPCODE_NOOP;
			LsCcbSetFlag(Ccb, CCB_FLAG_ALLOCATED);
			Ccb->Srb = Srb;
			LsCcbSetCompletionRoutine(Ccb, NdasDluCcbCompletion, LogicalUnitExtension);

			InterlockedIncrement(&ndasDluExtension->RequestExecuting);
			LsuIncrementTdiClientInProgress();
			status = LurRequest(
								ndasDluExtension->LUR,
								Ccb
							);
			if(!NT_SUCCESS(status)) {
				LsuDecrementTdiClientInProgress();
				LsCcbFree(Ccb);
				srbStatus = SRB_STATUS_ERROR;
				status = STATUS_SUCCESS;
				KDPrint(1,("LurRequest() failed.\n"));
				break;
			}

			srbStatus = SRB_STATUS_SUCCESS;
			status = STATUS_PENDING;
		break;
		}
		case NDASSCSI_IOCTL_ADD_USERBACL: {
			PNDAS_BLOCK_ACE	bace = (PNDAS_BLOCK_ACE)srbIoctlBuffer; // input
			PBLOCKACE_ID	blockAceId = (PBLOCKACE_ID)srbIoctlBuffer; // output
			UCHAR			lsuAccessMode;
			PLSU_BLOCK_ACE	lsuBace;

			UINT64	blockStartAddr;
			UINT64	blockEndAddr;

			if(srbIoctlBufferLength < sizeof(NDAS_BLOCK_ACE)) {
				srbStatus = SRB_STATUS_DATA_OVERRUN;
				status = STATUS_UNSUCCESSFUL;
				break;
			}

			if(ndasDluExtension->LUR == NULL) {
				srbStatus = SRB_STATUS_INVALID_REQUEST;
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			lsuAccessMode = 0;
			if(bace->AccessMode & NBACE_ACCESS_READ) {
				lsuAccessMode |= LSUBACE_ACCESS_READ;
			}
			if(bace->AccessMode & NBACE_ACCESS_WRITE) {
				lsuAccessMode |= LSUBACE_ACCESS_WRITE;
			}

			if (bace->IsByteAddress) {

				blockStartAddr = bace->StartingOffset / ndasDluExtension->LUR->BlockBytes;
				blockEndAddr = (bace->StartingOffset + bace->Length) / ndasDluExtension->LUR->BlockBytes - 1;

			} else {

				blockStartAddr = bace->BlockStartAddr;
				blockEndAddr = bace->BlockEndAddr;
			}

			lsuBace = LsuCreateBlockAce( lsuAccessMode, blockStartAddr, blockEndAddr );

			if(lsuBace == NULL) {
				srbStatus = SRB_STATUS_DATA_OVERRUN;
				status = STATUS_UNSUCCESSFUL;
				break;
			}

			// Set returned BACE ID.
			*blockAceId = lsuBace->BlockAceId;

			LsuInsertAce(&ndasDluExtension->LUR->UserBacl, lsuBace);

			srbStatus = SRB_STATUS_SUCCESS;
			status = STATUS_SUCCESS;
			break;
		}
		case NDASSCSI_IOCTL_REMOVE_USERBACL: {
			PNDSCIOCTL_REMOVE_USERBACL ioctlRemoveUserAcl = (PNDSCIOCTL_REMOVE_USERBACL)srbIoctlBuffer;
			BLOCKACE_ID	blockAceId;
			PLSU_BLOCK_ACE	lsuBacl;

			if(srbIoctlBufferLength < sizeof(NDSCIOCTL_REMOVE_USERBACL)) {
				srbStatus = SRB_STATUS_DATA_OVERRUN;
				status = STATUS_UNSUCCESSFUL;
				break;
			}

			blockAceId = ioctlRemoveUserAcl->NdasBlockAceId; // input

			if(ndasDluExtension->LUR == NULL) {
				srbStatus = SRB_STATUS_INVALID_REQUEST;
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			if(blockAceId == 0) {
				KDPrint(1,("REMOVE_USERBACL: Zero block ACE ID.\n"));
				srbStatus = SRB_STATUS_INVALID_REQUEST;
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			lsuBacl = LsuRemoveAceById(&ndasDluExtension->LUR->UserBacl, blockAceId);
			if(lsuBacl == NULL) {
				KDPrint(1,("REMOVE_USERBACL: Invalid block ACE ID.\n"));
				srbStatus = SRB_STATUS_INVALID_REQUEST;
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			LsuFreeBlockAce(lsuBacl);

			srbStatus = SRB_STATUS_SUCCESS;
			status = STATUS_SUCCESS;
			break;
		}
		case NDASSCSI_IOCTL_DEVICE_LOCK: {

			if(srbIoctlBufferLength < sizeof(NDSCIOCTL_DEVICELOCK)) {
				srbStatus = SRB_STATUS_DATA_OVERRUN;
				status = STATUS_UNSUCCESSFUL;
				break;
			}

			if(ndasDluExtension->LUR == NULL) {
				srbStatus = SRB_STATUS_INVALID_REQUEST;
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			status = NdasDluIoctlDeviceLock(
										LogicalUnitExtension,
										ndasDluExtension,
										(PNDSCIOCTL_DEVICELOCK)srbIoctlBuffer,
										Srb);
			srbStatus = Srb->SrbStatus;
			break;
		}
        default:

            KDPrint(2,("Control Code (%x)\n", controlCode));
            srbStatus = SRB_STATUS_INVALID_REQUEST;
			status = STATUS_MORE_PROCESSING_REQUIRED;
    }

	Srb->SrbStatus = srbStatus;

    return status;
}



NTSTATUS
NdasDluSendIoctlSrb(
		IN PDEVICE_OBJECT	DeviceObject,
		IN ULONG			IoctlCode,
		IN PVOID			InputBuffer,
		IN LONG				InputBufferLength,
		OUT PVOID			OutputBuffer,
		IN LONG				OutputBufferLength
	) {

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

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	psrbIoctl	= NULL;
	irp = NULL;

	//
	//	build an SRB for the miniport
	//
	srbIoctlBufferLength = (InputBufferLength>OutputBufferLength)?InputBufferLength:OutputBufferLength;
	srbIoctlLength = sizeof(SRB_IO_CONTROL) +  srbIoctlBufferLength;

	psrbIoctl = (PSRB_IO_CONTROL)ExAllocatePoolWithTag(NonPagedPool , srbIoctlLength, NDAS_DLU_PTAG_SRB_IOCTL);
	if(psrbIoctl == NULL) {
		KDPrint(1, ("STATUS_INSUFFICIENT_RESOURCES\n"));
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto cleanup;
	}

	RtlZeroMemory(psrbIoctl, srbIoctlLength);
	psrbIoctl->HeaderLength = sizeof(SRB_IO_CONTROL);
	RtlCopyMemory(psrbIoctl->Signature, NDASSCSI_IOCTL_SIGNATURE, 8);
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
    // asynchronously then the completion code would have to make a special
    // check to deallocate the buffer.  Second if a completion routine were
    // used then an additional IRP stack location would be needed.
    //

    irp = IoBuildSynchronousFsdRequest(
                IRP_MJ_SCSI,
                DeviceObject,
                psrbIoctl,
                srbIoctlLength,
                &startingOffset,
                &event,
                &ioStatusBlock);

    irpStack = IoGetNextIrpStackLocation(irp);

    if (irp == NULL) {
        KDPrint(1,("STATUS_INSUFFICIENT_RESOURCES\n"));

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

    srb.SrbFlags = SRB_FLAGS_DATA_IN | SRB_FLAGS_NO_QUEUE_FREEZE;

	srb.QueueAction = SRB_SIMPLE_TAG_REQUEST;
	srb.QueueTag = SP_UNTAGGED;

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
    status = IoCallDriver( DeviceObject, irp );

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
			KDPrint(1,("Ioctl(%08lx) succeeded!\n", IoctlCode));
	}

cleanup:
	if(psrbIoctl)
		ExFreePool(psrbIoctl);

    return status;
}


typedef struct _MINISENDSRB_CONTEXT {
	PIRP				Irp;
	SCSI_REQUEST_BLOCK	Srb;
	PVOID				UserBuffer;
	ULONG				UserBufferLen;
	SRB_IO_CONTROL		SrbIoctl;
} MINISENDSRB_CONTEXT, *PMINISENDSRB_CONTEXT;

NTSTATUS
NdasDluSendSrbIoCompletion(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PIRP  Irp,
    IN PVOID  Context
){
	PMINISENDSRB_CONTEXT sendSrb = (PMINISENDSRB_CONTEXT)Context;

	UNREFERENCED_PARAMETER(DeviceObject);

	KDPrint(1,("STATUS=%08lx\n", Irp->IoStatus.Status));

	//
	//	get the result
	//
	if(Irp->IoStatus.Status == STATUS_SUCCESS) {
		if(sendSrb->UserBuffer && sendSrb->UserBufferLen)
			RtlCopyMemory(
					sendSrb->UserBuffer,
					(PUCHAR)&sendSrb->SrbIoctl + sizeof(SRB_IO_CONTROL),
					sendSrb->UserBufferLen);
	}
	// Free the IRP resources
	if(Irp->AssociatedIrp.SystemBuffer)
		ExFreePool(Irp->AssociatedIrp.SystemBuffer);
	if( Irp->MdlAddress != NULL ) {
		MmUnlockPages( Irp->MdlAddress );
		IoFreeMdl( Irp->MdlAddress );
		Irp->MdlAddress = NULL;
	}
	// Free the IRP
	IoFreeIrp(Irp);
	ExFreePool(sendSrb);

	return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS
NdasDluSendIoctlSrbAsynch(
		IN PDEVICE_OBJECT	DeviceObject,
		IN ULONG			IoctlCode,
		IN PVOID			InputBuffer,
		IN LONG				InputBufferLength,
		OUT PVOID			OutputBuffer,
		IN LONG				OutputBufferLength
){

    PIRP				irp;
	PMINISENDSRB_CONTEXT	context = NULL;
 	PSRB_IO_CONTROL		psrbIoctl;
	PSCSI_REQUEST_BLOCK	srb;
	LONG				srbIoctlLength;
	PVOID				srbIoctlBuffer;
	LONG				srbIoctlBufferLength;
    NTSTATUS			status;
    PIO_STACK_LOCATION	irpStack;
    LARGE_INTEGER		startingOffset;
    IO_STATUS_BLOCK		ioStatusBlock;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	psrbIoctl	= NULL;
	irp = NULL;

	//
	//	build an SRB for the miniport
	//
	srbIoctlBufferLength = (InputBufferLength>OutputBufferLength)?InputBufferLength:OutputBufferLength;
	srbIoctlLength = sizeof(SRB_IO_CONTROL) +  srbIoctlBufferLength;

	context = (PMINISENDSRB_CONTEXT)ExAllocatePoolWithTag(NonPagedPool,
					FIELD_OFFSET(MINISENDSRB_CONTEXT, SrbIoctl) + srbIoctlLength,
					NDAS_DLU_PTAG_SRB_CMPDATA);
	if(context == NULL) {
		KDPrint(1, ("STATUS_INSUFFICIENT_RESOURCES\n"));
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto cleanup;
	}

	context->UserBuffer = OutputBuffer;
	context->UserBufferLen = OutputBufferLength;
	srb = &context->Srb;
	psrbIoctl = &context->SrbIoctl;

	RtlZeroMemory(psrbIoctl, srbIoctlLength);
	psrbIoctl->HeaderLength = sizeof(SRB_IO_CONTROL);
	RtlCopyMemory(psrbIoctl->Signature, NDASSCSI_IOCTL_SIGNATURE, 8);
	psrbIoctl->Timeout = 10;
	psrbIoctl->ControlCode = IoctlCode;
	psrbIoctl->Length = srbIoctlBufferLength;

	srbIoctlBuffer = (PUCHAR)psrbIoctl + sizeof(SRB_IO_CONTROL);
	RtlCopyMemory(srbIoctlBuffer, InputBuffer, InputBufferLength);

	startingOffset.QuadPart = 1;

    //
    // Build IRP for this request.
    // Note we do this synchronously for two reasons.  If it was done
    // asynchronously then the completion code would have to make a special
    // check to deallocate the buffer.  Second if a completion routine were
    // used then an additional IRP stack location would be needed.
    //

    irp = IoBuildAsynchronousFsdRequest(
                 IRP_MJ_SCSI,
                DeviceObject,
                psrbIoctl,
                srbIoctlLength,
                &startingOffset,
                &ioStatusBlock);

	context->Irp = irp;
	IoSetCompletionRoutine(irp, NdasDluSendSrbIoCompletion, context, TRUE, TRUE, TRUE);
    irpStack = IoGetNextIrpStackLocation(irp);

    if (irp == NULL) {
        KDPrint(1,("STATUS_INSUFFICIENT_RESOURCES\n"));

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

    irpStack->Parameters.Others.Argument1 = srb;

    //
    // Zero out the srb.
    //

    RtlZeroMemory(srb, sizeof(SCSI_REQUEST_BLOCK));

    srb->PathId = 0;
    srb->TargetId = 0;
    srb->Lun = 0;

    srb->Function = SRB_FUNCTION_IO_CONTROL;
    srb->Length = sizeof(SCSI_REQUEST_BLOCK);

    srb->SrbFlags = SRB_FLAGS_DATA_OUT | SRB_FLAGS_NO_QUEUE_FREEZE;

	srb->QueueAction = SRB_SIMPLE_TAG_REQUEST;
	srb->QueueTag = SP_UNTAGGED;

    srb->OriginalRequest = irp;

    //
    // Set timeout to requested value.
    //

    srb->TimeOutValue = psrbIoctl->Timeout;

    //
    // Set the data buffer.
    //

    srb->DataBuffer = psrbIoctl;
    srb->DataTransferLength = srbIoctlLength;

    //
    // Flush the data buffer for output. This will insure that the data is
    // written back to memory.  Since the data-in flag is the the port driver
    // will flush the data again for input which will ensure the data is not
    // in the cache.
    //
/*
    KeFlushIoBuffers(irp->MdlAddress, FALSE, TRUE);
*/
    status = IoCallDriver( DeviceObject, irp );

	return status;

cleanup:
	if(context)
		ExFreePool(context);

    return status;
}


NTSTATUS
NdasDluFireEvent(
	IN PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension,
	IN ULONG DluInternalStatus)
{
	NTSTATUS status;
	PNDAS_DLU_EXTENSION	ndasDluExtension = NdasDluGetExtension(LogicalUnitExtension);
	NDAS_DLU_EVENT dluEventData;

	dluEventData.DluInternalStatus = DluInternalStatus;
	dluEventData.LogicalUnitAddress = 
		ndasDluExtension->LogicalUnitAddress;

	status = NdasPortFirePnpEvent(
		LogicalUnitExtension,
		&NDAS_DLU_EVENT_GUID,
		NULL, // L"NdasDluEvent",
		&dluEventData,
		sizeof(NDAS_DLU_EVENT));

	return status;
}
