#define __LANSCSI_BUS__
#include "LfsProc.h"

#define NTSTRSAFE_LIB
#include <ntstrsafe.h>

#include <ntddscsi.h>
#include <initguid.h>
#include <stdio.h>

#include <LanScsi.h>
#include "ndasscsiioctl.h"


//
//	Adjust Send MaxDataSize regarding stats from the transport protocol.
//

static
VOID
AdjustMaxDataSizeToSend(
	IN PLFS_TRANS_CTX	TransCtx
){

	if(TransCtx->AccSendTransStat.Retransmits >= 2) {


		//
		//	Decrease data transfer rate.
		//

		if(TransCtx->DynMaxSendBytes > LL_MIN_TRANSMIT_DATA) {

			TransCtx->DynMaxSendBytes /= 2;
			if(TransCtx->DynMaxSendBytes < LL_MIN_TRANSMIT_DATA) {
				TransCtx->DynMaxSendBytes = LL_MIN_TRANSMIT_DATA;
			}

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR,
				("AdjustMaxDataSizeToSend: decreased IO rate: %08ld bytes\n",
				TransCtx->DynMaxSendBytes));

		}


		//
		//	Reset retransmit counter
		//

		TransCtx->AccSendTransStat.Retransmits = 0;
		TransCtx->StableSendCnt = 0;

	} else {


		//
		//	Increase data transfer rate.
		//

		if(TransCtx->DynMaxSendBytes < TransCtx->MaxSendBytes) {

			if(TransCtx->StableSendCnt>=256) {

				TransCtx->DynMaxSendBytes += LL_TRANSMIT_UNIT;
				if(TransCtx->DynMaxSendBytes >
					TransCtx->MaxSendBytes) {

					TransCtx->DynMaxSendBytes =
						TransCtx->MaxSendBytes;
				}

				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR,
					("AdjustMaxDataSizeToSend: increased IO rate: %08ld bytes\n",
					TransCtx->DynMaxSendBytes));

				TransCtx->StableSendCnt = 0;
			}

		}

		//
		//	Increase stable IO counter
		//

		TransCtx->StableSendCnt++;


		//
		//	Reset retransmit counter
		//

		TransCtx->AccSendTransStat.Retransmits = 0;
	}
}


NTSTATUS
SendMessage(
	IN PFILE_OBJECT		ConnectionFileObject,
	IN _U8				*Buf, 
	IN LONG				Size,
	IN PLARGE_INTEGER	TimeOut,
	IN PLFS_TRANS_CTX	TransCtx
){
	NTSTATUS	ntStatus;
	ULONG		result;
	LONG		remaining;
	ULONG		onceReqSz;
	TRANS_STAT	onceTransStat;

	ASSERT(Size > 0);

	ntStatus = STATUS_SUCCESS;

	//
	//	send a packet
	//
	for(remaining = Size; remaining > 0;  ) 
	{
		//
		//	Traffic control
		//
		if(TransCtx) {

			AdjustMaxDataSizeToSend(
				TransCtx
				);

			onceReqSz = (remaining < TransCtx->DynMaxSendBytes)?
								remaining:
								TransCtx->DynMaxSendBytes;

		} else {

			onceReqSz = (remaining < LL_MAX_TRANSMIT_DATA)?remaining:LL_MAX_TRANSMIT_DATA;

		}


		result = 0;

		if(TransCtx) {
			onceTransStat.Retransmits = 0;

			ntStatus = LpxTdiSend(
					ConnectionFileObject,
					Buf,
					onceReqSz,
					0,
					TimeOut,
					&onceTransStat,
					&result
					);
		} else {
			ntStatus = LpxTdiSend(
					ConnectionFileObject,
					Buf,
					onceReqSz,
					0,
					TimeOut,
					NULL,
					&result
					);
		}

		if(!NT_SUCCESS(ntStatus) || STATUS_PENDING == ntStatus) 
		{
		    SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
				("SendMessage: sending failed.\n"));
			break;
		} 
		else if( 0 >= result ) 
		{
		    SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
				("SendMessage: result less than 0.\n"));
			break;
		}

		remaining -= result;
		((PCHAR)Buf) += result;

		//
		//	Collect stats
		//

		if(TransCtx) {
			TransCtx->AccSendTransStat.Retransmits += onceTransStat.Retransmits;
#if DBG
			if(onceTransStat.Retransmits) {
				SPY_LOG_PRINT( LFS_DEBUG_LFS_ERROR,
					("SendMessage: Ret=%08ld\n", onceTransStat.Retransmits));
			}
#endif
		}
	}

	if(remaining) {
	    SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
				("SendMessage: unexpected data length sent. remaining:%lu\n", remaining));
			ntStatus = STATUS_UNSUCCESSFUL;
	}

	return ntStatus;
}


NTSTATUS
RecvMessage(
	IN PFILE_OBJECT		ConnectionFileObject,
	OUT _U8				*Buf, 
	IN LONG				Size,
	IN PLARGE_INTEGER	TimeOut
){
	NTSTATUS	ntStatus;
	ULONG		result;
	ULONG		remaining;
	ULONG		onceReqSz;
//	TRANS_STAT	onceTransStat;

	ASSERT(Size > 0);

	ntStatus = STATUS_SUCCESS;

	//
	//	receive data
	//
	for(remaining = Size; remaining > 0;  ) 
	{
		onceReqSz = (remaining < LL_MAX_TRANSMIT_DATA) ? remaining:LL_MAX_TRANSMIT_DATA;

		result = 0;
//		onceTransStat.PacketLoss = 0;

		ntStatus = LpxTdiRecv(
						ConnectionFileObject,
						Buf,
						onceReqSz,
						0,
						TimeOut,
						NULL /* &onceTransStat */,
						&result);

		if(!NT_SUCCESS(ntStatus) || STATUS_PENDING == ntStatus) {
		    SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
				("RecvMessage: Receiving failed.\n"));
			break;
		} else if( 0 >= result ) {
		    SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
				("RecvMessage: result less than 0.\n"));
			break;
		}

		remaining -= result;
		((PCHAR)Buf) += result;

#if 0 //DBG
		if(onceTransStat.PacketLoss) {
			SPY_LOG_PRINT( LFS_DEBUG_LFS_ERROR,
				("RecvMessage: Loss=%08lu\n", onceTransStat.PacketLoss));
		}
#endif
	}

	if(remaining) {
	    SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
				("RecvMessage: unexpected data length received. remaining:%lu\n", remaining));
			ntStatus = STATUS_UNSUCCESSFUL;
	}

	return ntStatus;
}


NTSTATUS
LfsFilterDeviceIoControl (
    IN PDEVICE_OBJECT	DeviceObject,
    IN ULONG			IoCtl,
    IN PVOID			InputBuffer OPTIONAL,
    IN ULONG			InputBufferLength,
    IN PVOID			OutputBuffer OPTIONAL,
    IN ULONG			OutputBufferLength,
    OUT PULONG_PTR		IosbInformation OPTIONAL
    )

/*++

Routine Description:

    This procedure issues an Ioctl to the lower device, and waits
    for the answer.

Arguments:

    DeviceObject - Supplies the device to issue the request to

    IoCtl - Gives the IoCtl to be used

    XxBuffer - Gives the buffer pointer for the ioctl, if any

    XxBufferLength - Gives the length of the buffer, if any

Return Value:

    None.

--*/

{
    PIRP			irp;
    KEVENT			event;
    IO_STATUS_BLOCK iosb;
    NTSTATUS		status;


	KeInitializeEvent( &event, NotificationEvent, FALSE );

    irp = IoBuildDeviceIoControlRequest( 
			IoCtl,
            DeviceObject,
            InputBuffer,
            InputBufferLength,
            OutputBuffer,
            OutputBufferLength,
            FALSE,
            &event,
            &iosb 
			);

    if (irp == NULL) 
	{
		ASSERT(LFS_INSUFFICIENT_RESOURCES);

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
						("STATUS_INSUFFICIENT_RESOURCES\n"));

		return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = IoCallDriver( DeviceObject, irp );

    if (status == STATUS_PENDING) {

        (VOID)KeWaitForSingleObject( 
						&event,
						Executive,
						KernelMode,
						FALSE,
						(PLARGE_INTEGER)NULL
						);

        status = iosb.Status;
    }

    //
    //  Get the information field from the completed Irp.
    //

	if(ARGUMENT_PRESENT(IosbInformation))
	{
		*IosbInformation = iosb.Information;
	}

    return status;
}


BOOLEAN
GetPrimaryUnitDisk(
	IN	PDEVICE_OBJECT				diskDeviceObject,
	OUT	PLSMPIOCTL_PRIMUNITDISKINFO	PrimUnitDisk
	)
{
	NTSTATUS					status;
	PSRB_IO_CONTROL				psrbioctl;
	int							outbuff_sz;
	PLSMPIOCTL_QUERYINFO		QueryInfo;
	PLSMPIOCTL_PRIMUNITDISKINFO	tmpPrimUnitDisk;


	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);	

	outbuff_sz = sizeof(SRB_IO_CONTROL) + sizeof(LSMPIOCTL_QUERYINFO) + sizeof(LSMPIOCTL_PRIMUNITDISKINFO);
	psrbioctl = (PSRB_IO_CONTROL)ExAllocatePool(NonPagedPool , outbuff_sz);
	if(psrbioctl == NULL) {
        SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
						("STATUS_INSUFFICIENT_RESOURCES\n"));
		return FALSE;
	}

	memset(psrbioctl, 0, sizeof(*psrbioctl));
	psrbioctl->HeaderLength = sizeof(SRB_IO_CONTROL);
	memcpy(psrbioctl->Signature, LANSCSIMINIPORT_IOCTL_SIGNATURE, 8);
	psrbioctl->Timeout = 10;
	psrbioctl->ControlCode = LANSCSIMINIPORT_IOCTL_QUERYINFO_EX;
	psrbioctl->Length = sizeof(LSMPIOCTL_QUERYINFO) + sizeof(LSMPIOCTL_PRIMUNITDISKINFO);

	QueryInfo = (PLSMPIOCTL_QUERYINFO)((PUCHAR)psrbioctl + sizeof(SRB_IO_CONTROL));
	tmpPrimUnitDisk = (PLSMPIOCTL_PRIMUNITDISKINFO)((PUCHAR)psrbioctl + sizeof(SRB_IO_CONTROL));

	QueryInfo->Length = sizeof(LSMPIOCTL_QUERYINFO);
	QueryInfo->InfoClass = LsmpPrimaryUnitDiskInformation;
	QueryInfo->QueryDataLength = 0;

	status = LfsFilterDeviceIoControl(
					diskDeviceObject,
					IOCTL_SCSI_MINIPORT,
					psrbioctl,
					outbuff_sz,
					psrbioctl,
					outbuff_sz,
					NULL
					);

	if(NT_SUCCESS(status)) 
	{
		if(tmpPrimUnitDisk->Length == sizeof(LSMPIOCTL_PRIMUNITDISKINFO)) {
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
				("desiredAccess = %x, grantedAccess = %x, GENERIC_WRITE = %x\n", 
				tmpPrimUnitDisk->UnitDisk.DesiredAccess,tmpPrimUnitDisk->UnitDisk.GrantedAccess, GENERIC_WRITE));

			RtlCopyMemory(PrimUnitDisk, tmpPrimUnitDisk, sizeof(LSMPIOCTL_PRIMUNITDISKINFO));
		} else {
			SPY_LOG_PRINT( LFS_DEBUG_LFS_ERROR,
				("Miniport driver does not match the version.\n"));
			status = STATUS_REVISION_MISMATCH;
		}
	} else {
	    SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
					("[LFS] GetPrimaryUnitDisk: Not NetDisk. status = %x\n", 
					status));
	}

	ExFreePool(psrbioctl);

	if(NT_SUCCESS(status)) 
		return TRUE;
	else
		return FALSE;
}


BOOLEAN	
GetScsiportAdapter(
  	IN	PDEVICE_OBJECT				DiskDeviceObject,
  	IN	PDEVICE_OBJECT				*ScsiportAdapterDeviceObject
	) 
{
	SCSI_ADDRESS		ScsiAddress;
	NTSTATUS			ntStatus;
	UNICODE_STRING		ScsiportAdapterName;
	WCHAR				ScsiportAdapterNameBuffer[32];
	WCHAR				ScsiportAdapterNameTemp[32]	= L"";
    OBJECT_ATTRIBUTES	objectAttributes;
    HANDLE				fileHandle					= NULL;
	IO_STATUS_BLOCK		IoStatus;
	PFILE_OBJECT		ScsiportDeviceFileObject	= NULL;

	ntStatus = LfsFilterDeviceIoControl( 
								DiskDeviceObject,
			                    IOCTL_SCSI_GET_ADDRESS,
								NULL,
								0,
						        &ScsiAddress,
							    sizeof(SCSI_ADDRESS),
								NULL 
								);
	if(!NT_SUCCESS(ntStatus)) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("GetScsiportAdapter: LfsFilterDeviceIoControl() failed.\n"));
		goto error_out;

	}

    SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("GetScsiportAdapter: ScsiAddress=Len:%d PortNumber:%d PathId:%d TargetId:%d Lun:%d\n",
						(LONG)ScsiAddress.Length,
						(LONG)ScsiAddress.PortNumber,
						(LONG)ScsiAddress.PathId,
						(LONG)ScsiAddress.TargetId,
						(LONG)ScsiAddress.Lun
						));

	RtlStringCchPrintfW(ScsiportAdapterNameTemp,
						sizeof(ScsiportAdapterNameTemp) / sizeof(ScsiportAdapterNameTemp[0]),
						L"\\Device\\ScsiPort%d",
						ScsiAddress.PortNumber);


	RtlInitEmptyUnicodeString( &ScsiportAdapterName, ScsiportAdapterNameBuffer, sizeof( ScsiportAdapterNameBuffer ) );
    RtlAppendUnicodeToString( &ScsiportAdapterName, ScsiportAdapterNameTemp );

	InitializeObjectAttributes( &objectAttributes,
							&ScsiportAdapterName,
							OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
							NULL,
							NULL);

    //
	// open the file object for the given device
	//
    ntStatus = ZwCreateFile( &fileHandle,
						   SYNCHRONIZE|FILE_READ_DATA,
						   &objectAttributes,
						   &IoStatus,
						   NULL,
						   0,
						   FILE_SHARE_READ | FILE_SHARE_WRITE,
						   FILE_OPEN,
						   FILE_SYNCHRONOUS_IO_NONALERT,
						   NULL,
						   0);
    if (!NT_SUCCESS( ntStatus )) {
	    SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("GetScsiportAdapter: ZwCreateFile() failed.\n"));
		goto error_out;

	}

    ntStatus = ObReferenceObjectByHandle( fileHandle,
										FILE_READ_DATA,
										*IoFileObjectType,
										KernelMode,
										&ScsiportDeviceFileObject,
										NULL);
    if(!NT_SUCCESS( ntStatus )) {

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("GetScsiportAdapter: ObReferenceObjectByHandle() failed.\n"));
        goto error_out;
    }

	*ScsiportAdapterDeviceObject = IoGetRelatedDeviceObject(
											ScsiportDeviceFileObject
									    );

	if(*ScsiportAdapterDeviceObject == NULL) {

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("GetScsiportAdapter: IoGetRelatedDeviceObject() failed.\n"));
		ObDereferenceObject(ScsiportDeviceFileObject);
        goto error_out;
	}

	ObDereferenceObject(ScsiportDeviceFileObject);
	ZwClose(fileHandle);
	ObReferenceObject(*ScsiportAdapterDeviceObject);

	return TRUE;

error_out:
	
	*ScsiportAdapterDeviceObject = NULL;
	if(fileHandle)
		ZwClose(fileHandle);

	return FALSE;
}


BOOLEAN	
GetHarddisk(
  	IN	PDEVICE_OBJECT	DiskDeviceObject,
  	IN	PDEVICE_OBJECT	*HarddiskDeviceObject
	) 
{
	NTSTATUS			ntStatus;
	VOLUME_DISK_EXTENTS	volumeDiskExtents;

	UNICODE_STRING		harddiskName;
	WCHAR				harddiskNameBuffer[32];
	WCHAR				harddiskNameTemp[32];

	ACCESS_MASK			desiredAccess;
	ULONG				attributes;
	OBJECT_ATTRIBUTES	objectAttributes;
	ULONG				fileAttributes;
	ULONG				shareAccess;
	ULONG				createDisposition;
	ULONG				createOptions;
	IO_STATUS_BLOCK		ioStatusBlock;

	HANDLE				harddiskFileHandle;
	PFILE_OBJECT		harddiskDeviceFileObject;


	ntStatus = LfsFilterDeviceIoControl( 
								DiskDeviceObject,
			                    IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
								NULL,
								0,
						        &volumeDiskExtents,
							    sizeof(VOLUME_DISK_EXTENTS),
								NULL 
								);
	if(!NT_SUCCESS(ntStatus)) 
	{
		//ASSERT(LFS_UNEXPECTED);
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("GetScsiportAdapter: LfsFilterDeviceIoControl() failed.\n"));
		return FALSE;
	}

    SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
					("ntStatus = %x, volumeDiskExtents.Extents[0].DiskNumber = %d\n", 
						ntStatus, volumeDiskExtents.Extents[0].DiskNumber));

	RtlStringCchPrintfW(
		harddiskNameTemp,
		sizeof(harddiskNameTemp) / sizeof(harddiskNameTemp[0]),
		L"\\DosDevices\\PhysicalDrive%d",
		volumeDiskExtents.Extents[0].DiskNumber
		);

	RtlInitEmptyUnicodeString(&harddiskName, harddiskNameBuffer, sizeof(harddiskNameBuffer));
    RtlAppendUnicodeToString(&harddiskName, harddiskNameTemp);

	desiredAccess = SYNCHRONIZE|FILE_READ_DATA;

	attributes  = OBJ_KERNEL_HANDLE;
	attributes |= OBJ_CASE_INSENSITIVE;

	InitializeObjectAttributes(
			&objectAttributes,
			&harddiskName,
			attributes,
			NULL,
			NULL
			);
		
	fileAttributes	  = 0;		
	shareAccess		  = FILE_SHARE_READ | FILE_SHARE_WRITE;
	createDisposition = FILE_OPEN;
	createOptions     = FILE_SYNCHRONOUS_IO_NONALERT;
		
	ntStatus = ZwCreateFile(
					&harddiskFileHandle,
					desiredAccess,
					&objectAttributes,
					&ioStatusBlock,
					NULL,
					fileAttributes,
					shareAccess,
					createDisposition,
					createOptions,
					NULL,
					0
					);	
  
	if (!NT_SUCCESS( ntStatus )) 
	{
		//ASSERT(LFS_UNEXPECTED);
	    SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("GetScsiportAdapter: ZwCreateFile() failed.\n"));
		return FALSE;
	}

	if(ntStatus == STATUS_SUCCESS)
	{
		ASSERT(ioStatusBlock.Information == FILE_OPENED);
	}

    ntStatus = ObReferenceObjectByHandle(
					harddiskFileHandle,
					FILE_READ_DATA,
					*IoFileObjectType,
					KernelMode,
					&harddiskDeviceFileObject,
					NULL
					);

    if(!NT_SUCCESS( ntStatus ))
	{
		ASSERT(LFS_UNEXPECTED);
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("GetScsiportAdapter: ObReferenceObjectByHandle() failed.\n"));
		ZwClose(harddiskFileHandle);
		return FALSE;
    }

	*HarddiskDeviceObject = IoGetRelatedDeviceObject(harddiskDeviceFileObject);

	if(*HarddiskDeviceObject == NULL) 
	{
		ASSERT(LFS_UNEXPECTED);
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("GetScsiportAdapter: IoGetRelatedDeviceObject() failed.\n"));
		ObDereferenceObject(harddiskDeviceFileObject);
		ZwClose(harddiskFileHandle);
		return FALSE;
	}

	ObReferenceObject(*HarddiskDeviceObject);
	ObDereferenceObject(harddiskDeviceFileObject);
	ZwClose(harddiskFileHandle);

	return TRUE;
}


//
// Return false if NDAS device is CD/DVD.
//
BOOLEAN
IsNetDisk(
	IN	PDEVICE_OBJECT			DiskDeviceObject,
	OUT PNETDISK_INFORMATION	NetdiskInformation
	) 
{
	BOOLEAN						returnResult;
	PDEVICE_OBJECT				scsiportAdapterDeviceObject;
	LSMPIOCTL_PRIMUNITDISKINFO	primUnitDisk;

	if(DiskDeviceObject == NULL) {
		return FALSE;
	}

	returnResult = GetScsiportAdapter(DiskDeviceObject, &scsiportAdapterDeviceObject);
	if(returnResult != TRUE) 
	{
	    SPY_LOG_PRINT( LFS_DEBUG_LFS_ERROR, ("IsNetDisk: GetScsiportAdapter() failed.\n"));
		return FALSE;
	}

	returnResult = GetPrimaryUnitDisk(
					scsiportAdapterDeviceObject,
					&primUnitDisk
					);

	if(returnResult == TRUE && NetdiskInformation)
	{
		RtlCopyMemory(&NetdiskInformation->NetDiskAddress, &primUnitDisk.UnitDisk.NetDiskAddress.Address[0].Address, sizeof(LPX_ADDRESS)); 
		NetdiskInformation->UnitDiskNo = primUnitDisk.UnitDisk.UnitDiskId;

		NetdiskInformation->DesiredAccess = primUnitDisk.UnitDisk.DesiredAccess;
		NetdiskInformation->GrantedAccess = primUnitDisk.UnitDisk.GrantedAccess;

		RtlCopyMemory(
			NetdiskInformation->UserId,
			primUnitDisk.UnitDisk.UserID,
			sizeof(NetdiskInformation->UserId)
			);
		RtlCopyMemory(
			NetdiskInformation->Password,
			primUnitDisk.UnitDisk.Password,
			sizeof(NetdiskInformation->Password)
			);

		NetdiskInformation->MessageSecurity = FALSE;
		NetdiskInformation->RwDataSecurity = FALSE;

		NetdiskInformation->SlotNo = primUnitDisk.UnitDisk.SlotNo;
		NetdiskInformation->EnabledTime = primUnitDisk.EnabledTime;
		RtlCopyMemory(&NetdiskInformation->BindAddress, &primUnitDisk.UnitDisk.BindingAddress.Address[0].Address, sizeof(LPX_ADDRESS));


		//
		//	Determine if this LUR is a disk group
		//

		if(primUnitDisk.Lur.LurnCnt >= 2) {
			NetdiskInformation->DiskGroup = TRUE;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("IsNetDisk: LUR is a disk group\n"));
		} else {
			NetdiskInformation->DiskGroup = FALSE;
		}

	}

	if(scsiportAdapterDeviceObject)
		ObDereferenceObject(scsiportAdapterDeviceObject);

	return returnResult;
}


//////////////////////////////////////////////////////////////////////////
//
//	NDAS bus control
// 

NTSTATUS
NDCtrlOpenLanScsiBus(
	OUT	PDEVICE_OBJECT		*LanScsiDev,
	OUT PFILE_OBJECT		*FileObject
)
{
	NTSTATUS			ntStatus;
	PWSTR				symbolicLinkList;
    UNICODE_STRING		objectName;
	PFILE_OBJECT		fileObject;
	PDEVICE_OBJECT		deviceObject;

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NDCtrlOpenLanScsiBus: entered.\n"));

	ASSERT(LanScsiDev);
	ASSERT(KeGetCurrentIrql() <= PASSIVE_LEVEL);

	ntStatus = IoGetDeviceInterfaces(
			&GUID_LANSCSI_BUS_ENUMERATOR_INTERFACE_CLASS,
			NULL,
			0,
			&symbolicLinkList
		);
	
	if(!NT_SUCCESS(ntStatus)) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlOpenLanScsiBus: IoGetDeviceInterfaces ntStatus = 0x%x\n", ntStatus));
		return ntStatus;
	}

	ASSERT(symbolicLinkList != NULL);

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NDCtrlOpenLanScsiBus: symbolicLinkList = %ws\n", symbolicLinkList));

    RtlInitUnicodeString(&objectName, symbolicLinkList);
	ntStatus = IoGetDeviceObjectPointer(
						&objectName,
						FILE_ALL_ACCESS,
						&fileObject,
						&deviceObject
					);

	if(!NT_SUCCESS(ntStatus)) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlOpenLanScsiBus: ntStatus = 0x%x\n", ntStatus));
		ExFreePool(symbolicLinkList);
		return ntStatus;
	}

	ExFreePool(symbolicLinkList);

	*LanScsiDev = deviceObject;
	*FileObject = fileObject;

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "NDCtrlOpenLanScsiBus: Done.\n"));

	return ntStatus;
}


VOID
NDCtrlCloseLanScsiBus(
		PFILE_OBJECT FileObject
	) {
	ASSERT(FileObject);

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "NDCtrlCloseLanScsiBus: Closed LanScsiBus interface.\n"));

	ObDereferenceObject(FileObject);

}


NTSTATUS
NDCtrlUpgradeToWrite(
	ULONG				SlotNo,
	PIO_STATUS_BLOCK	IoStatus
) 
{
	NTSTATUS			ntStatus;
	PFILE_OBJECT		fileObject;
	PDEVICE_OBJECT		deviceObject;
	PIRP				irp;
    KEVENT				event;
	BUSENUM_UPGRADE_TO_WRITE	Param;

	ASSERT(KeGetCurrentIrql() <= PASSIVE_LEVEL );
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlUpradeToWrite: SlotNo:%ld\n", SlotNo));

	ntStatus = NDCtrlOpenLanScsiBus(&deviceObject, &fileObject);
	if(!NT_SUCCESS(ntStatus)) {
		return ntStatus;
	}

	Param.Size = sizeof(BUSENUM_UPGRADE_TO_WRITE);
	Param.SlotNo = SlotNo;

    KeInitializeEvent(&event, NotificationEvent, FALSE);


	irp = IoBuildDeviceIoControlRequest(
			IOCTL_LANSCSI_UPGRADETOWRITE,
			deviceObject,
			&Param,
			sizeof(BUSENUM_UPGRADE_TO_WRITE),
			NULL,
			0,
			FALSE,
			&event,
			IoStatus
		);

    if (irp == NULL) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlUpgradeToWrite: irp NULL\n"));

		ntStatus = STATUS_INSUFFICIENT_RESOURCES;

		goto cleanup;
	}

	ntStatus = IoCallDriver(deviceObject, irp);
	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlUpradeToWrite: NTSTATUS:%lx IOSTATUS:%lx\n", ntStatus, IoStatus->Status));
    if (ntStatus == STATUS_PENDING)
    {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlUpradeToWrite: IoCallDriver STATUS_PENDING\n"));
		KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
    }
	if (!NT_SUCCESS (ntStatus)) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlUpradeToWrite: IoCallDriver() failed.\n"));
//		ASSERT(FALSE);
	}

cleanup:

	NDCtrlCloseLanScsiBus(fileObject);
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NDCtrlUpgradeToWrite: Done...\n"));

	
	return ntStatus;
}

static
NTSTATUS
NDCtrlRemoveTarget(
	ULONG				SlotNo
) 
{
	NTSTATUS				ntStatus;
	PFILE_OBJECT			fileObject;
	PDEVICE_OBJECT			deviceObject;
	PIRP					irp;
    KEVENT					event;
	IO_STATUS_BLOCK			ioStatusBlock;

    LANSCSI_REMOVE_TARGET_DATA removeTarget;

	ASSERT(KeGetCurrentIrql() <= PASSIVE_LEVEL);
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlRemoveTarget: SlotNo:%ld\n", SlotNo));

	ntStatus = NDCtrlOpenLanScsiBus(&deviceObject, &fileObject);
	if(!NT_SUCCESS(ntStatus)) {
		return ntStatus;
	}

	RtlZeroMemory(&removeTarget, sizeof(removeTarget));
	removeTarget.ulSlotNo = SlotNo;

    KeInitializeEvent(&event, NotificationEvent, FALSE);


	irp = IoBuildDeviceIoControlRequest(
			IOCTL_LANSCSI_REMOVE_TARGET,
			deviceObject,
			&removeTarget,
			sizeof(removeTarget),
			&removeTarget,
			sizeof(removeTarget),
			FALSE,
			&event,
			&ioStatusBlock
		);

    if (irp == NULL) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlRemoveTarget: irp NULL\n"));

		ntStatus = STATUS_INSUFFICIENT_RESOURCES;

		goto cleanup;
	}

	ntStatus = IoCallDriver(deviceObject, irp);
	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlRemoveTarget: NTSTATUS:%lx IOSTATUS:%lx\n", ntStatus, ioStatusBlock.Status));
    if (ntStatus == STATUS_PENDING)
    {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlRemoveTarget: IoCallDriver STATUS_PENDING\n"));
		KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
    }
	if (!NT_SUCCESS (ntStatus) || !NT_SUCCESS(ioStatusBlock.Status)) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, 
			( "[LFS] NDCtrlRemoveTarget: IoCallDriver() failed. STATUS=%08lx\n", ntStatus));
//		ASSERT(FALSE);
	}

cleanup:

	NDCtrlCloseLanScsiBus(fileObject);
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlRemoveTarget: Done...\n"));

	
	return ntStatus;
}

NTSTATUS
NDCtrlUnplug(
	ULONG				SlotNo
) 
{
	NTSTATUS				ntStatus;
	PFILE_OBJECT			fileObject;
	PDEVICE_OBJECT			deviceObject;
	PIRP					irp;
    KEVENT					event;
	IO_STATUS_BLOCK			ioStatusBlock;

    BUSENUM_UNPLUG_HARDWARE unplug;

	ASSERT(KeGetCurrentIrql() <= PASSIVE_LEVEL );
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlUnplug: SlotNo:%ld\n", SlotNo));

	//
	//	Before unplugging, make sure to remove target devices in the NDAS adapter
	//
	NDCtrlRemoveTarget(SlotNo);

	//
	//	Continue to unplug
	//
	ntStatus = NDCtrlOpenLanScsiBus(&deviceObject, &fileObject);
	if(!NT_SUCCESS(ntStatus)) {
		return ntStatus;
	}

	unplug.SlotNo = SlotNo;
    unplug.Size = sizeof (unplug);
	
    KeInitializeEvent(&event, NotificationEvent, FALSE);


	irp = IoBuildDeviceIoControlRequest(
			IOCTL_BUSENUM_UNPLUG_HARDWARE,
			deviceObject,
			&unplug,
			sizeof(unplug),
			&unplug,
			sizeof(unplug),
			FALSE,
			&event,
			&ioStatusBlock
		);

    if (irp == NULL) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlUnplug: irp NULL\n"));

		ntStatus = STATUS_INSUFFICIENT_RESOURCES;

		goto cleanup;
	}

	ntStatus = IoCallDriver(deviceObject, irp);
	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlUnplug: NTSTATUS:%lx IOSTATUS:%lx\n", ntStatus, ioStatusBlock.Status));
    if (ntStatus == STATUS_PENDING)
    {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlUnplug: IoCallDriver STATUS_PENDING\n"));
		KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
    }
	if (!NT_SUCCESS (ntStatus) || !NT_SUCCESS(ioStatusBlock.Status)) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, 
			( "[LFS] NDCtrlUnplug: IoCallDriver() failed. STATUS=%08lx\n", ntStatus));
//		ASSERT(FALSE);
	}

cleanup:

	NDCtrlCloseLanScsiBus(fileObject);
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlUnplug: Done...\n"));

	
	return ntStatus;
}

NTSTATUS
NDCtrlEject(
	ULONG				SlotNo
){
	NTSTATUS				ntStatus;
	PFILE_OBJECT			fileObject;
	PDEVICE_OBJECT			deviceObject;
	PIRP					irp;
    KEVENT					event;
	IO_STATUS_BLOCK			ioStatusBlock;

    BUSENUM_EJECT_HARDWARE eject;

	ASSERT(KeGetCurrentIrql() <= PASSIVE_LEVEL );
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlEject: SlotNo:%ld\n", SlotNo));

	ntStatus = NDCtrlOpenLanScsiBus(&deviceObject, &fileObject);
	if(!NT_SUCCESS(ntStatus)) {
		return ntStatus;
	}

	eject.SlotNo = SlotNo;
    eject.Size = sizeof (eject);
	
    KeInitializeEvent(&event, NotificationEvent, FALSE);


	irp = IoBuildDeviceIoControlRequest(
			IOCTL_BUSENUM_EJECT_HARDWARE,
			deviceObject,
			&eject,
			sizeof(eject),
			&eject,
			sizeof(eject),
			FALSE,
			&event,
			&ioStatusBlock
		);

    if (irp == NULL) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlEject: irp NULL\n"));

		ntStatus = STATUS_INSUFFICIENT_RESOURCES;

		goto cleanup;
	}

	ntStatus = IoCallDriver(deviceObject, irp);
	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlEject: NTSTATUS:%lx IOSTATUS:%lx\n", ntStatus, ioStatusBlock.Status));
    if (ntStatus == STATUS_PENDING)
    {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlEject: IoCallDriver STATUS_PENDING\n"));
		KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
    }
	if (!NT_SUCCESS (ntStatus) || !NT_SUCCESS(ioStatusBlock.Status)) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlEject: IoCallDriver() failed. STATUS=%08lx\n", ntStatus));
	}

cleanup:

	NDCtrlCloseLanScsiBus(fileObject);
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlEject: Done...\n"));

	
	return ntStatus;
}

//////////////////////////////////////////////////////////////////////////
//
//
//
#if DBG

PUCHAR	FileInformationClassString[FileMaximumInformation] = {
	"FileInvalidClass",
    "FileDirectoryInformation",		  // 1
    "FileFullDirectoryInformation",   // 2
    "FileBothDirectoryInformation",   // 3
    "FileBasicInformation",           // 4  wdm
    "FileStandardInformation",        // 5  wdm
    "FileInternalInformation",        // 6
    "FileEaInformation",              // 7
    "FileAccessInformation",          // 8
    "FileNameInformation",            // 9
    "FileRenameInformation",          // 10
    "FileLinkInformation",            // 11
    "FileNamesInformation",           // 12
    "FileDispositionInformation",     // 13
    "FilePositionInformation",        // 14 wdm
    "FileFullEaInformation",          // 15
    "FileModeInformation",            // 16
    "FileAlignmentInformation",       // 17
    "FileAllInformation",             // 18
    "FileAllocationInformation",      // 19
    "FileEndOfFileInformation",       // 20 wdm
    "FileAlternateNameInformation",   // 21
    "FileStreamInformation",          // 22
    "FilePipeInformation",            // 23
    "FilePipeLocalInformation",       // 24
    "FilePipeRemoteInformation",      // 25
    "FileMailslotQueryInformation",   // 26
    "FileMailslotSetInformation",     // 27
    "FileCompressionInformation",     // 28
    "FileObjectIdInformation",        // 29
    "FileCompletionInformation",      // 30
    "FileMoveClusterInformation",     // 31
    "FileQuotaInformation",           // 32
    "FileReparsePointInformation",    // 33
    "FileNetworkOpenInformation",     // 34
    "FileAttributeTagInformation",    // 35
    "FileTrackingInformation",        // 36
    "FileIdBothDirectoryInformation", // 37
    "FileIdFullDirectoryInformation", // 38
};


#define DEBUG_BUFFER_LENGTH 512

PWCHAR
NullTerminateUnicode(
	IN ULONG		UnicodeStringLength,
	IN PWCHAR		UnicodeString,
	IN ULONG		BufferLength,
	IN PWCHAR		Buffer
) {

	ULONG	printLength;

	BufferLength -= sizeof(WCHAR);		// occupation of NULL termination.
	if(BufferLength <= 0) return L"Not enough buffer";

	printLength = BufferLength < UnicodeStringLength ? BufferLength : UnicodeStringLength;
	RtlCopyMemory(Buffer, UnicodeString, printLength);
	Buffer[printLength / 2] = L'\0';

	return Buffer;

}



VOID
PrintFileInfoClass(
		ULONG			infoType,
		ULONG			systemBuffLength,
		PCHAR			systemBuff
) {
	ULONG	offset;
	ULONG	count;
#if DBG
	PWCHAR	UnicodeBuffer;


	UnicodeBuffer = ExAllocatePool(NonPagedPool, DEBUG_BUFFER_LENGTH);
	if(UnicodeBuffer == NULL)
		return;
#endif

	SPY_LOG_PRINT( LFS_DEBUG_LFS_NOISE,( "[LFS] PrintFileInfoClass:\n"));
	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileInformationClass: %s(%d)\n",
						FileInformationClassString[infoType],
						infoType
					));
	switch(infoType) {
	case FileDirectoryInformation: {
		PFILE_DIRECTORY_INFORMATION	info;

		offset = 0;
		count = 0;
		while(offset < systemBuffLength) {
			info = (PFILE_DIRECTORY_INFORMATION)(systemBuff + offset);

			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "  * entry #%lu *\n", count));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "NextEntryOffset	: %lu\n",info->NextEntryOffset ));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileIndex		: %lu\n", info->FileIndex));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "CreationTime	: %I64u\n", info->CreationTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastAccessTime	: %I64u\n", info->LastAccessTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastWriteTime	: %I64u\n", info->LastWriteTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ChangeTime		: %I64u\n", info->ChangeTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "EndOfFile		: %I64u\n", info->EndOfFile.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "AllocationSize	: %I64u\n", info->AllocationSize.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileAttributes	: %lx\n", info->FileAttributes));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileNameLength	: %lu\n", info->FileNameLength));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileName			: '%ws'\n", 
								NullTerminateUnicode(	info->FileNameLength,
									(PWCHAR)info->FileName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				));
	
			count ++;
			offset += info->NextEntryOffset;

			if(!info->NextEntryOffset) break;
		}
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "%lu chunks printed\n", count));

		break;
	}
	case FileFullDirectoryInformation: {
		PFILE_FULL_DIR_INFORMATION	info;

		offset = 0;
		count = 0;
		while(offset < systemBuffLength) {
			info = (PFILE_FULL_DIR_INFORMATION)(systemBuff + offset);

			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "  * entry #%lu *\n", count));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "NextEntryOffset	: %lu\n",info->NextEntryOffset ) );
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileIndex		: %lu\n", info->FileIndex));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "CreationTime	: %I64u\n", info->CreationTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastAccessTime	: %I64u\n", info->LastAccessTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastWriteTime	: %I64u\n", info->LastWriteTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ChangeTime		: %I64u\n", info->ChangeTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "EndOfFile		: %I64u\n", info->EndOfFile.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "AllocationSize	: %I64u\n", info->AllocationSize.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileAttributes	: %lx\n", info->FileAttributes));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileNameLength	: %lu\n", info->FileNameLength));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "EaSize			: %lu\n", info->EaSize));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileName			: '%ws'\n", 
								NullTerminateUnicode(	info->FileNameLength,
									(PWCHAR)info->FileName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				));
	
			count ++;
			offset += info->NextEntryOffset;

			if(!info->NextEntryOffset) break;
		}
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "%lu chunks printed\n", count));

		break;
	}
	case FileBothDirectoryInformation: {
		PFILE_BOTH_DIR_INFORMATION	info;

		offset = 0;
		count = 0;
		while(offset < systemBuffLength) {
			info = (PFILE_BOTH_DIR_INFORMATION)(systemBuff + offset);

			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "  * entry #%lu *\n", count));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "NextEntryOffset	: %lu\n",info->NextEntryOffset ));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileIndex		: %lu\n", info->FileIndex));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "CreationTime	: %I64u\n", info->CreationTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastAccessTime	: %I64u\n", info->LastAccessTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastWriteTime	: %I64u\n", info->LastWriteTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ChangeTime		: %I64u\n", info->ChangeTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "EndOfFile		: %I64u\n", info->EndOfFile.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "AllocationSize	: %I64u\n", info->AllocationSize.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileAttributes	: %lx\n", info->FileAttributes));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileNameLength	: %lu\n", info->FileNameLength));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "EaSize			: %lu\n", info->EaSize));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ShortNameLength	: %u\n", (int)info->ShortNameLength));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ShortName		: '%ws'\n", 
								NullTerminateUnicode(	info->ShortNameLength,
									(PWCHAR)info->ShortName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileName			: '%ws'\n", 
								NullTerminateUnicode(	info->FileNameLength,
									(PWCHAR)info->FileName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				));

			count ++;
			offset += info->NextEntryOffset;

			if(!info->NextEntryOffset) break;
		}
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "%lu chunks printed\n", count));

		break;
	}
	case FileIdBothDirectoryInformation: {
		PFILE_ID_BOTH_DIR_INFORMATION	info;

		offset = 0;
		count = 0;
		while(offset < systemBuffLength) {
			info = (PFILE_ID_BOTH_DIR_INFORMATION)(systemBuff + offset);

			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "  * entry #%lu *\n", count));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "NextEntryOffset	: %lu\n",info->NextEntryOffset ));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileIndex		: %lu\n", info->FileIndex));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "CreationTime	: %I64u\n", info->CreationTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastAccessTime	: %I64u\n", info->LastAccessTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastWriteTime	: %I64u\n", info->LastWriteTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ChangeTime		: %I64u\n", info->ChangeTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "EndOfFile		: %I64u\n", info->EndOfFile.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "AllocationSize	: %I64u\n", info->AllocationSize.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileAttributes	: %lx\n", info->FileAttributes));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileNameLength	: %lu\n", info->FileNameLength));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "EaSize			: %lu\n", info->EaSize));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ShortNameLength	: %u\n", (int)info->ShortNameLength));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ShortName		: '%ws'\n", 
								NullTerminateUnicode(	info->ShortNameLength,
									(PWCHAR)info->ShortName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileId			: %I64x\n", info->FileId.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileName			: '%ws'\n", 
								NullTerminateUnicode(	info->FileNameLength,
									(PWCHAR)info->FileName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				));

			count ++;
			offset += info->NextEntryOffset;

			if(!info->NextEntryOffset) break;
		}
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "%lu chunks printed\n", count));

		break;
	}
	case FileIdFullDirectoryInformation: {
		PFILE_ID_FULL_DIR_INFORMATION	info;

		offset = 0;
		count = 0;
		while(offset < systemBuffLength) {
			info = (PFILE_ID_FULL_DIR_INFORMATION)(systemBuff + offset);

			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "  * entry #%lu *\n", count));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "NextEntryOffset	: %lu\n",info->NextEntryOffset ));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileIndex		: %lu\n", info->FileIndex));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "CreationTime	: %I64u\n", info->CreationTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastAccessTime	: %I64u\n", info->LastAccessTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastWriteTime	: %I64u\n", info->LastWriteTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ChangeTime		: %I64u\n", info->ChangeTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "EndOfFile		: %I64u\n", info->EndOfFile.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "AllocationSize	: %I64u\n", info->AllocationSize.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileAttributes	: %lx\n", info->FileAttributes));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileNameLength	: %lu\n", info->FileNameLength));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "EaSize			: %lu\n", info->EaSize));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileId			: %I64x\n", info->FileId.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileName			: '%ws'\n", 
								NullTerminateUnicode(	info->FileNameLength,
									(PWCHAR)info->FileName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				));

			count ++;
			offset += info->NextEntryOffset;

			if(!info->NextEntryOffset) break;
		}
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "%lu chunks printed\n", count));

		break;
	}
	case FileNamesInformation: {
		PFILE_NAMES_INFORMATION	info;

		offset = 0;
		count = 0;
		while(offset < systemBuffLength) {
			info = (PFILE_NAMES_INFORMATION)(systemBuff + offset);

			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "  * entry #%lu *\n", count));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "NextEntryOffset	: %lu\n",info->NextEntryOffset ));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileIndex		: %lu\n", info->FileIndex));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileNameLength	: %lu\n", info->FileNameLength));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileName			: '%ws'\n", 
								NullTerminateUnicode(	info->FileNameLength,
									(PWCHAR)info->FileName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				));

			count ++;
			offset += info->NextEntryOffset;

			if(!info->NextEntryOffset) break;
		}
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "%lu chunks printed\n", count));

		break;
	}
	case FileObjectIdInformation: {
		PFILE_OBJECTID_INFORMATION	info = (PFILE_OBJECTID_INFORMATION)systemBuff;

			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileReference	: %I64u\n", info->FileReference));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ObjectId			: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
									info->ObjectId[0], info->ObjectId[1], info->ObjectId[2], info->ObjectId[3],
									info->ObjectId[4], info->ObjectId[5], info->ObjectId[6], info->ObjectId[7],
									info->ObjectId[8], info->ObjectId[9], info->ObjectId[10], info->ObjectId[11],
									info->ObjectId[12], info->ObjectId[13], info->ObjectId[14], info->ObjectId[!5]
							));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "BirthVolumeId	: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
									info->BirthVolumeId[0], info->BirthVolumeId[1], info->BirthVolumeId[2], info->BirthVolumeId[3],
									info->BirthVolumeId[4], info->BirthVolumeId[5], info->BirthVolumeId[6], info->BirthVolumeId[7],
									info->BirthVolumeId[8], info->BirthVolumeId[9], info->BirthVolumeId[10], info->BirthVolumeId[11],
									info->BirthVolumeId[12], info->BirthVolumeId[13], info->BirthVolumeId[14], info->BirthVolumeId[!5]
							) );
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "BirthObjectId	: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
									info->BirthObjectId[0], info->BirthObjectId[1], info->BirthObjectId[2], info->BirthObjectId[3],
									info->BirthObjectId[4], info->BirthObjectId[5], info->BirthObjectId[6], info->BirthObjectId[7],
									info->BirthObjectId[8], info->BirthObjectId[9], info->BirthObjectId[10], info->BirthObjectId[11],
									info->BirthObjectId[12], info->BirthObjectId[13], info->BirthObjectId[14], info->BirthObjectId[!5]
							) );
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "DomainId			: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
									info->DomainId[0], info->DomainId[1], info->DomainId[2], info->DomainId[3],
									info->DomainId[4], info->DomainId[5], info->DomainId[6], info->DomainId[7],
									info->DomainId[8], info->DomainId[9], info->DomainId[10], info->DomainId[11],
									info->DomainId[12], info->DomainId[13], info->DomainId[14], info->DomainId[!5]
							) );

		break;
	}
	case FileAllocationInformation: {
		PFILE_ALLOCATION_INFORMATION	info = (PFILE_ALLOCATION_INFORMATION)systemBuff;

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "AllocationSize	: %I64u\n", info->AllocationSize.QuadPart));
		break;
	}

	case FileAttributeTagInformation: {
		PFILE_ATTRIBUTE_TAG_INFORMATION	info = (PFILE_ATTRIBUTE_TAG_INFORMATION)systemBuff;

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileAttributes	: %lx\n", info->FileAttributes ));
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ReparseTag		: %lx\n", info->ReparseTag ));
		break;
	}
	case FileBasicInformation: {
		PFILE_BASIC_INFORMATION	info = (PFILE_BASIC_INFORMATION)systemBuff;

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "CreationTime	: %I64u\n", info->CreationTime.QuadPart));
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastAccessTime	: %I64u\n", info->LastAccessTime.QuadPart));
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastWriteTime	: %I64u\n", info->LastWriteTime.QuadPart));
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ChangeTime		: %I64u\n", info->ChangeTime.QuadPart));
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileAttributes	: %lx\n", info->FileAttributes));
		break;
	}
	case FileStandardInformation: {
		PFILE_STANDARD_INFORMATION	info = (PFILE_STANDARD_INFORMATION)systemBuff;

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "AllocationSize	: %I64u\n", info->AllocationSize.QuadPart));
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "EndOfFile		: %I64u\n", info->EndOfFile.QuadPart));
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "NumberOfLinks	: %lu\n", info->NumberOfLinks));
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "DeletePending	: %u\n", info->DeletePending));
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "Directory		: %u\n", info->Directory));
		break;
	 }
	case FileStreamInformation: {
		PFILE_STREAM_INFORMATION	info ;

		offset = 0;
		count = 0;
		while(offset < systemBuffLength) {
			info = (PFILE_STREAM_INFORMATION)(systemBuff + offset);

			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "  * entry #%lu *\n", count));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "NextEntryOffset	: %lu\n",info->NextEntryOffset ));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "CreationTime	: %I64u\n", info->StreamSize.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastAccessTime	: %I64u\n", info->StreamAllocationSize.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "StreamNameLength	: %lu\n", info->StreamNameLength));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "StreamName		: '%ws'\n", 
								NullTerminateUnicode(	info->StreamNameLength,
									(PWCHAR)info->StreamName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				));
	
			count ++;
			offset += info->NextEntryOffset;

			if(!info->NextEntryOffset) break;
		}
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "%lu chunks printed\n", count));
		
		break;
	}
	case FileEaInformation: {
		PFILE_EA_INFORMATION	info = (PFILE_EA_INFORMATION)systemBuff;
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "EaSize	: %lu\n",info->EaSize ));
		break;
	}

	case FileNameInformation: {
		PFILE_NAME_INFORMATION	info = (PFILE_NAME_INFORMATION)systemBuff;
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileNameLength	: %lu\n", info->FileNameLength ));
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileName       : %ws\n", 
								NullTerminateUnicode(info->FileNameLength,
									(PWCHAR)info->FileName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				));
		break;
	}

	case FileEndOfFileInformation: {
		PFILE_END_OF_FILE_INFORMATION	info = (PFILE_END_OF_FILE_INFORMATION)systemBuff;

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("EndOfFile : %I64d", info->EndOfFile.QuadPart));
		break;
	}

	case FileAllInformation:
	case FileReparsePointInformation:
	case FileQuotaInformation:
	default:
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] PrintFileInfoClass:Not supported query type.\n"));
	}

#if DBG
	ExFreePool(UnicodeBuffer);
#endif
}


#endif



VOID
PrintTime (
	IN ULONG	DebugLevel,
    IN PTIME	Time
    )
{
    TIME_FIELDS TimeFields;

	UNREFERENCED_PARAMETER(DebugLevel);
    RtlTimeToTimeFields( Time, &TimeFields );

	SPY_LOG_PRINT( DebugLevel,
			( "%02u-%02u-%02u  %02u:%02u:%02u \n",
            TimeFields.Month,
            TimeFields.Day,
            TimeFields.Year % 100,
            TimeFields.Hour,
            TimeFields.Minute,
            TimeFields.Second ));

    return;
}


//////////////////////////////////////////////////////////////////////////
//
//	Event log
//

static
VOID
_WriteLogErrorEntry(
	IN PDEVICE_OBJECT			DeviceObject,
	IN PLFS_ERROR_LOG_ENTRY		LogEntry
){
	PIO_ERROR_LOG_PACKET errorLogEntry;
	WCHAR					strBuff[16];
	NTSTATUS				status;
	ULONG					stringOffset;
	ULONG_PTR				stringLen;
	ULONG					idx_dump;

	//
	//	Parameter to unicode string
	//
	ASSERT(LogEntry->DumpDataEntry <= LFS_MAX_ERRLOG_DATA_ENTRIES);
	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	status = RtlStringCchPrintfW(strBuff, 16, L"%u", LogEntry->Parameter2);
	if(!NT_SUCCESS(status)) {
		SPY_LOG_PRINT(LFS_DEBUG_LFS_ERROR, ("RtlStringCchVPrintfW() failed.\n"));
		return;
	}

	status = RtlStringCchLengthW(strBuff, 16, &stringLen);
	if(!NT_SUCCESS(status)) {
		SPY_LOG_PRINT(LFS_DEBUG_LFS_ERROR, ("RtlStringCchLengthW() failed.\n"));
		return;
	}

	//
	//	Translate unicode length into byte length including NULL termination.
	//

	stringLen = ( stringLen + 1 ) * sizeof(WCHAR);
	stringOffset = FIELD_OFFSET(IO_ERROR_LOG_PACKET, DumpData) + LogEntry->DumpDataEntry * sizeof(ULONG);


	errorLogEntry = (PIO_ERROR_LOG_PACKET)
					IoAllocateErrorLogEntry(
									DeviceObject,
									(sizeof(IO_ERROR_LOG_PACKET) +
									(LogEntry->DumpDataEntry * sizeof(ULONG)) +
									(UCHAR)stringLen));
	if(errorLogEntry == NULL) {
		SPY_LOG_PRINT(LFS_DEBUG_LFS_ERROR, ("Could not allocate error log entry.\n"));
		ASSERT(FALSE);
		return ;
	}

	errorLogEntry->ErrorCode = LogEntry->ErrorCode;
	errorLogEntry->MajorFunctionCode = LogEntry->MajorFunctionCode;
	errorLogEntry->IoControlCode = LogEntry->IoctlCode;
	errorLogEntry->EventCategory;
	errorLogEntry->SequenceNumber = LogEntry->SequenceNumber;
	errorLogEntry->RetryCount = (UCHAR) LogEntry->ErrorLogRetryCount;
	errorLogEntry->UniqueErrorValue = LogEntry->UniqueId;
	errorLogEntry->FinalStatus = LogEntry->FinalStatus;
	errorLogEntry->DumpDataSize = LogEntry->DumpDataEntry * sizeof(ULONG);
	for(idx_dump=0; idx_dump < LogEntry->DumpDataEntry; idx_dump++) {
		errorLogEntry->DumpData[idx_dump] = LogEntry->DumpData[idx_dump];
	}

	errorLogEntry->NumberOfStrings = 1;
	errorLogEntry->StringOffset = (USHORT)stringOffset;
	RtlCopyMemory((PUCHAR)errorLogEntry + stringOffset, strBuff, stringLen);

	IoWriteErrorLogEntry(errorLogEntry);

	return;
}


typedef struct _LFS_ERRORLOGCTX {

	PIO_WORKITEM		IoWorkItem;
	LFS_ERROR_LOG_ENTRY	ErrorLogEntry;

} LFS_ERRORLOGCTX, *PLFS_ERRORLOGCTX;


static
VOID
WriteErrorLogWorker(
 IN PDEVICE_OBJECT	DeviceObject,
 IN PVOID			Context
){
	PLFS_ERRORLOGCTX	errorLogCtx = (PLFS_ERRORLOGCTX)Context;

	UNREFERENCED_PARAMETER(DeviceObject);

	_WriteLogErrorEntry(DeviceObject, &errorLogCtx->ErrorLogEntry);

	IoFreeWorkItem(errorLogCtx->IoWorkItem);
	ExFreePoolWithTag(errorLogCtx, LFS_ERRORLOGWORKER_TAG);

}

VOID
LfsWriteLogErrorEntry(
	IN PDEVICE_OBJECT		DeviceObject,
    IN PLFS_ERROR_LOG_ENTRY ErrorLogEntry
){


	if(KeGetCurrentIrql() > PASSIVE_LEVEL) {
		PIO_WORKITEM	workitem;
		PLFS_ERRORLOGCTX	context;

		context = ExAllocatePoolWithTag(NonPagedPool, sizeof(LFS_ERRORLOGCTX), LFS_ERRORLOGWORKER_TAG);
		if(context == NULL) {
			SPY_LOG_PRINT(LFS_DEBUG_LFS_ERROR, ("Allocating context failed.\n"));
			return;
		}

		RtlCopyMemory(&context->ErrorLogEntry, ErrorLogEntry, sizeof(LFS_ERROR_LOG_ENTRY));


		workitem = IoAllocateWorkItem(DeviceObject);
		if(workitem == NULL) {
			SPY_LOG_PRINT(LFS_DEBUG_LFS_ERROR, ("IoAllocateWorkItem() failed.\n"));
			return;
		}

		context->IoWorkItem = workitem;

		IoQueueWorkItem(workitem, WriteErrorLogWorker, DelayedWorkQueue, context);

	} else {
		_WriteLogErrorEntry(DeviceObject, ErrorLogEntry);
	}
}

//////////////////////////////////////////////////////////////////////////
//
//	Event queue for user mode applications
//


//
//	Initialize event queue manager
//

NTSTATUS
XEvtqInitializeEventQueueManager(
	PXEVENT_QUEUE_MANAGER EventQueueManager
){

	RtlZeroMemory(EventQueueManager, sizeof(XEVENT_QUEUE_MANAGER));
	InitializeListHead(&EventQueueManager->EventQueueList);
	KeInitializeSpinLock(&EventQueueManager->EventQueueListSpinLock);

	return STATUS_SUCCESS;
}


//
//	Destroy even queue manager
//

NTSTATUS
XEvtqDestroyEventQueueManager(
	PXEVENT_QUEUE_MANAGER EventQueueManager
){
	KIRQL			oldIrql;
	PLIST_ENTRY		listEntry, nextListEntry;
	PXEVENT_QUEUE	evtQ;
	LONG			cnt;

	KeAcquireSpinLock(&EventQueueManager->EventQueueListSpinLock, &oldIrql);

	for(listEntry = EventQueueManager->EventQueueList.Flink;
		listEntry != &EventQueueManager->EventQueueList;
		listEntry = nextListEntry) {

		//
		//	Save the next entry before freeing the entry
		//

		nextListEntry = listEntry->Flink;


		//
		//	Disconnect the event queue from the manager
		//

		RemoveEntryList(listEntry);
		InitializeListHead(listEntry);
		cnt = InterlockedDecrement(&EventQueueManager->EventQueueCnt);
		ASSERT(EventQueueManager->EventQueueCnt >= 0);


		//
		//	Close the event queue
		//

		evtQ = CONTAINING_RECORD(listEntry, XEVENT_QUEUE, EventQueueEntry);
		XEvtqCloseEventQueue((LFS_EVTQUEUE_HANDLE)evtQ);
	}

	KeReleaseSpinLock(&EventQueueManager->EventQueueListSpinLock, oldIrql);

	return STATUS_SUCCESS;
}


//
//	Insert events into event queues registered.
//

NTSTATUS
XevtqInsertEvent(
	PXEVENT_QUEUE_MANAGER	EventQueueManager,
	PXEVENT_ITEM			EventItem
){
	KIRQL			oldIrql;
	PLIST_ENTRY		listEntry, nextEntry;
	PXEVENT_QUEUE	evtQ;
	BOOLEAN			queueFull;

	KeAcquireSpinLock(&EventQueueManager->EventQueueListSpinLock, &oldIrql);

	for(listEntry = EventQueueManager->EventQueueList.Flink;
		listEntry != &EventQueueManager->EventQueueList;
		listEntry = nextEntry) {


		//
		//	Save the next entry before freeing the entry
		//

		nextEntry = listEntry->Flink;

		//
		//	Queue the event
		//

		evtQ = CONTAINING_RECORD(listEntry, XEVENT_QUEUE, EventQueueEntry);
		XEvtqQueueEvent((LFS_EVTQUEUE_HANDLE)evtQ, EventItem, &queueFull);

		if(queueFull) {

			//
			//	If the queue is full, remove the queue.
			//

			XevtqUnregisterEventQueue(
				EventQueueManager,
				(LFS_EVTQUEUE_HANDLE)evtQ,
				FALSE
				);

			XEvtqCloseEventQueue((LFS_EVTQUEUE_HANDLE)evtQ);
		}
	}

	KeReleaseSpinLock(&EventQueueManager->EventQueueListSpinLock, oldIrql);

	return STATUS_SUCCESS;
}

//
//	Register event queue
//

NTSTATUS
XevtqRegisterEventQueue(
	PXEVENT_QUEUE_MANAGER	EventQueueManager,
	LFS_EVTQUEUE_HANDLE		EventQueueHandle
) {
	NTSTATUS		status;
	PXEVENT_QUEUE	xeventQ;
	KIRQL			oldIrql;
	LONG			cnt;

	status = XEvtqVerifyEventQueueHandle(EventQueueHandle);
	if(!NT_SUCCESS(status)) {
		return status;
	}

	xeventQ = (PXEVENT_QUEUE)EventQueueHandle;

	KeAcquireSpinLock(&EventQueueManager->EventQueueListSpinLock, &oldIrql);
	InsertTailList(&EventQueueManager->EventQueueList, &xeventQ->EventQueueEntry);
	KeReleaseSpinLock(&EventQueueManager->EventQueueListSpinLock, oldIrql);

	cnt = InterlockedIncrement(&EventQueueManager->EventQueueCnt);
	ASSERT(cnt >= 1);


	return STATUS_SUCCESS;
}


//
//	Unregister event queue
//

NTSTATUS
XevtqUnregisterEventQueue(
	PXEVENT_QUEUE_MANAGER	EventQueueManager,
	LFS_EVTQUEUE_HANDLE		EventQueueHandle,
	BOOLEAN					SafeLocking
){
	NTSTATUS		status;
	PXEVENT_QUEUE	xeventQ;
	KIRQL			oldIrql;
	LONG			cnt;

	UNREFERENCED_PARAMETER(EventQueueManager);

	status = XEvtqVerifyEventQueueHandle(EventQueueHandle);
	if(!NT_SUCCESS(status)) {
		return status;
	}

	xeventQ = (PXEVENT_QUEUE)EventQueueHandle;

	if(SafeLocking)
		KeAcquireSpinLock(&EventQueueManager->EventQueueListSpinLock, &oldIrql);

	RemoveEntryList(&xeventQ->EventQueueEntry);
	InitializeListHead(&xeventQ->EventQueueEntry);

	if(SafeLocking)
		KeReleaseSpinLock(&EventQueueManager->EventQueueListSpinLock, oldIrql);

	cnt = InterlockedDecrement(&EventQueueManager->EventQueueCnt);
	ASSERT(cnt >= 0);

	return STATUS_SUCCESS;
}


//
//	Create event queue
//

NTSTATUS
XEvtqCreateEventQueue(
	LFS_EVTQUEUE_HANDLE		*EventQueueHandle,
	HANDLE					*EventWaitHandle
){
	NTSTATUS			status;
	PXEVENT_QUEUE		xeventQ;
	OBJECT_ATTRIBUTES	objAttr;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	xeventQ = (PXEVENT_QUEUE)ExAllocatePoolWithTag(
									NonPagedPool,
									sizeof(XEVENT_QUEUE),
									POOLTAG_XEVENT_QUEUE);
	if(xeventQ == NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(xeventQ, sizeof(XEVENT_QUEUE));
	xeventQ->Signature = XEVENT_QUEUE_SIG;
	InitializeListHead(&xeventQ->EventQueueEntry);
	InitializeListHead(&xeventQ->EventList);
	KeInitializeSpinLock(&xeventQ->EventListSpinLock);
	xeventQ->NotifiedProcessId = PsGetCurrentProcess();


	//
	//	Create notification event
	//
	
	InitializeObjectAttributes(
			&objAttr,
			NULL,
			OBJ_FORCE_ACCESS_CHECK,
			NULL,
			NULL);
	status = ZwCreateEvent(	&xeventQ->EventOccur,
							SYNCHRONIZE | EVENT_QUERY_STATE | EVENT_MODIFY_STATE,
							&objAttr,
							NotificationEvent,
							FALSE);
	if(!NT_SUCCESS(status)) {
		ExFreePoolWithTag(xeventQ, POOLTAG_XEVENT_QUEUE);
		return status;
	}


	//
	//	Reference the event object
	//

	status = ObReferenceObjectByHandle(
			xeventQ->EventOccur,
			EVENT_ALL_ACCESS | EVENT_MODIFY_STATE,
			NULL,
			KernelMode,
			&xeventQ->EventOccurObject,
			NULL);
	if(!NT_SUCCESS(status)) {
		ZwClose(xeventQ->EventOccur);
		ExFreePoolWithTag(xeventQ, POOLTAG_XEVENT_QUEUE);
		return status;
	}


	//
	//	set return values
	//

	*EventQueueHandle = (LFS_EVTQUEUE_HANDLE)xeventQ;
	*EventWaitHandle = xeventQ->EventOccur;

	return status;
}


//
//	Close event queue
//

NTSTATUS
XEvtqCloseEventQueue(
	LFS_EVTQUEUE_HANDLE		EventQueueHandle
){
	NTSTATUS		status;
	PXEVENT_QUEUE	xeventQ;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	status = XEvtqVerifyEventQueueHandle(EventQueueHandle);
	if(!NT_SUCCESS(status)) {
		return status;
	}

	xeventQ = (PXEVENT_QUEUE)EventQueueHandle;

	XEvtqEmptyEventQueue(EventQueueHandle);
	ObDereferenceObject(xeventQ->EventOccurObject);
	ZwClose(xeventQ->EventOccur);
	ExFreePoolWithTag(xeventQ, POOLTAG_XEVENT_QUEUE);

	return STATUS_SUCCESS;
}


//
//	Queue an event
//
#define EVENTQUEUE_MAX_ITEM	50

NTSTATUS
XEvtqQueueEvent(
	LFS_EVTQUEUE_HANDLE	EventQueueHandle,
	PXEVENT_ITEM		EventItem,
	PBOOLEAN			QueueFull
){
	NTSTATUS			status;
	LONG				cnt;
	PXEVENT_QUEUE		xeventQ;
	PXEVENT_ITEM_ENTRY	xevent;

	status = XEvtqVerifyEventQueueHandle(EventQueueHandle);
	if(!NT_SUCCESS(status)) {
		return status;
	}

	if(QueueFull)
		*QueueFull = FALSE;

	xeventQ = (PXEVENT_QUEUE)EventQueueHandle;
	if(xeventQ->NotifiedProcessId != PsGetCurrentProcess()) {
		return STATUS_ACCESS_DENIED;
	}

	xevent = ExAllocatePoolWithTag(NonPagedPool, sizeof(XEVENT_ITEM_ENTRY), POOLTAG_XEVENT);
	if(xevent == NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	//
	//	Copy the event item to the event item entry and
	//	insert it into the event queue.
	//

	RtlCopyMemory(&xevent->EventItem, EventItem, sizeof(XEVENT_ITEM));

	ExInterlockedInsertTailList(	&xeventQ->EventList,
									&xevent->EventListEntry,
									&xeventQ->EventListSpinLock);

	KeSetEvent(xeventQ->EventOccurObject, IO_DISK_INCREMENT, FALSE);

	cnt = InterlockedIncrement(&xeventQ->EventCnt);
	ASSERT(cnt >= 1);

	if(cnt >= EVENTQUEUE_MAX_ITEM) {
		if(QueueFull)
			*QueueFull = TRUE;
	}

	return STATUS_SUCCESS;
}

NTSTATUS
XEvtqGetEventHeader(
	LFS_EVTQUEUE_HANDLE	EventQueueHandle,
	PUINT32				EventLength,
	PUINT32				EventClass
){
	NTSTATUS		status;
	PXEVENT_QUEUE	xeventQ;
	KIRQL			oldIrql;
	PLIST_ENTRY		listEntry;
	PXEVENT_ITEM_ENTRY	eventEntry;

	//
	//	Verify event queue handle and process.
	//

	status = XEvtqVerifyEventQueueHandle(EventQueueHandle);
	if(!NT_SUCCESS(status)) {
		return status;
	}
	xeventQ = (PXEVENT_QUEUE)EventQueueHandle;
	if(xeventQ->NotifiedProcessId != PsGetCurrentProcess()) {
		return STATUS_ACCESS_DENIED;
	}

	//
	//	Get the header information of the first entry.
	//

	KeAcquireSpinLock(&xeventQ->EventListSpinLock, &oldIrql);

	listEntry = xeventQ->EventList.Flink;
	if(listEntry != &xeventQ->EventList) {
		eventEntry = CONTAINING_RECORD(listEntry, XEVENT_ITEM_ENTRY, EventListEntry);

		if(EventLength) {
			*EventLength = eventEntry->EventItem.EventLength;
		}
		if(EventClass) {
			*EventClass = eventEntry->EventItem.EventClass;
		}

	} else {
		status = STATUS_NO_MORE_ENTRIES;
	}

	KeReleaseSpinLock(&xeventQ->EventListSpinLock, oldIrql);

	return status;
}

//
//	Dequeue an event
//

NTSTATUS
XEvtqDequeueEvent(
	LFS_EVTQUEUE_HANDLE		EventQueueHandle,
	PXEVENT_ITEM			EventItem
){
	NTSTATUS			status;
	LONG				cnt;
	PXEVENT_QUEUE		xeventQ;
	PLIST_ENTRY			listEntry;
	PXEVENT_ITEM_ENTRY	xevent;

	status = XEvtqVerifyEventQueueHandle(EventQueueHandle);
	if(!NT_SUCCESS(status)) {
		return status;
	}

	xeventQ = (PXEVENT_QUEUE)EventQueueHandle;
	if(xeventQ->NotifiedProcessId != PsGetCurrentProcess()) {
		return STATUS_ACCESS_DENIED;
	}

	listEntry = ExInterlockedRemoveHeadList(&xeventQ->EventList, &xeventQ->EventListSpinLock);
	if(listEntry != NULL) {

		//
		//	Copy an event item to the buffer that the caller supplies.
		//

		xevent = CONTAINING_RECORD(listEntry, XEVENT_ITEM_ENTRY, EventListEntry);

		if(EventItem)
			RtlCopyMemory(EventItem, &xevent->EventItem, xevent->EventItem.EventLength);

		ExFreePoolWithTag(xevent, POOLTAG_XEVENT);

		cnt = InterlockedDecrement(&xeventQ->EventCnt);
		ASSERT(cnt >= 0);

		status = STATUS_SUCCESS;
	} else {
		status = STATUS_NO_MORE_ENTRIES;
	}

	return status;
}


//
//	Empty event queue
//

NTSTATUS
XEvtqEmptyEventQueue(
	LFS_EVTQUEUE_HANDLE		EventQueueHandle
){
	NTSTATUS	status;

	status = XEvtqVerifyEventQueueHandle(EventQueueHandle);
	if(!NT_SUCCESS(status)) {
		return status;
	}

	while(TRUE) {

		status = XEvtqDequeueEvent(
						EventQueueHandle,
						NULL);
		if(status == STATUS_NO_MORE_ENTRIES) {
			status = STATUS_SUCCESS;
			break;
		} if(!NT_SUCCESS(status)) {
			break;
		}
	}

	return status;
}


//
//	Verify event queue handle
//

NTSTATUS
XEvtqVerifyEventQueueHandle(
	LFS_EVTQUEUE_HANDLE	EventQueueHandle
){
	NTSTATUS	status;
	PXEVENT_QUEUE	evtQueue = (PXEVENT_QUEUE)EventQueueHandle;

	status = STATUS_SUCCESS;
	_try {

		if(evtQueue->Signature != XEVENT_QUEUE_SIG) {
			status = STATUS_OBJECT_TYPE_MISMATCH;
		}

	} except(EXCEPTION_EXECUTE_HANDLER) {
		status = GetExceptionCode();
	}

	return status;
}

//////////////////////////////////////////////////////////////////////////
//
//	Queue an event
//


// XEVTCLS_PRIMARY_VOLUME_INVALID_OR_LOCKED

NTSTATUS
XevtQueueVolumeInvalidOrLocked(
	UINT32	PhysicalDriveNumber,
	UINT32	SlotNumber,
	UINT32	UnitNumber
){
	NTSTATUS	status;
	XEVENT_ITEM	eventItem;

	RtlZeroMemory(&eventItem, sizeof(XEVENT_ITEM));
	eventItem.EventLength = FIELD_OFFSET(XEVENT_ITEM, VolumeLocked) +
							sizeof(XEVENT_VPARAM_VOLUMELOCKED);
	eventItem.EventClass = XEVTCLS_PRIMARY_VOLUME_INVALID_OR_LOCKED;
	eventItem.DiskVolumeNumber = PhysicalDriveNumber;
	eventItem.VParamLength = sizeof(XEVENT_VPARAM_VOLUMELOCKED);
	eventItem.VolumeLocked.SlotNumber = SlotNumber;
	eventItem.VolumeLocked.UnitNumber = UnitNumber;

	status = XevtqInsertEvent(
					&GlobalLfs.EvtQueueMgr,
					&eventItem
		);

	return status;
}

// XEVTCLS_PRIMARY_SHUTDOWN

NTSTATUS
XevtQueueShutdown(){
	NTSTATUS	status;
	XEVENT_ITEM	eventItem;

	RtlZeroMemory(&eventItem, sizeof(XEVENT_ITEM));
	eventItem.EventLength = FIELD_OFFSET(XEVENT_ITEM, VolumeLocked);
	eventItem.EventClass = XEVTCLS_LFSFILT_SHUTDOWN;
	eventItem.DiskVolumeNumber = 0;
	eventItem.VParamLength = 0;

	status = XevtqInsertEvent(
					&GlobalLfs.EvtQueueMgr,
					&eventItem
		);

	return status;
}


//////////////////////////////////////////////////////////////////////////
//
//
//

NTSTATUS
LfsStopSecVolume(
	UINT32	PhysicalDriveNumber,
	UINT32	Hint
){

	NTSTATUS	status;

	UNREFERENCED_PARAMETER(PhysicalDriveNumber);
	UNREFERENCED_PARAMETER(Hint);

	status = STATUS_NOT_SUPPORTED;

	return status;
}



NTSTATUS
LfsQueryDirectoryByIndexCompletion (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PKEVENT SynchronizingEvent
    )
{

    UNREFERENCED_PARAMETER( DeviceObject );
    
    ASSERT( NULL != Irp->UserIosb );
    *Irp->UserIosb = Irp->IoStatus;

    KeSetEvent( SynchronizingEvent, IO_NO_INCREMENT, FALSE );

    IoFreeIrp( Irp );

    return STATUS_MORE_PROCESSING_REQUIRED;
}

//
// Used to continue query from the last queried position 
//		in case that primary host has changed since last directory query. 
//
NTSTATUS
LfsQueryDirectoryByIndex(
    HANDLE 	FileHandle,
    ULONG	FileInformationClass,
    PVOID  	FileInformation,
    ULONG	Length,
    ULONG	FileIndex,
    PUNICODE_STRING FileName,
    PIO_STATUS_BLOCK IoStatusBlock,
    BOOLEAN ReturnSingleEntry
    )
{
//    KIRQL    irql;
    NTSTATUS status;
    KEVENT  Event;
    PFILE_OBJECT fileObject;
    PDEVICE_OBJECT DeviceObject;
    PMDL mdl;
    PIRP irp;
    PIO_STACK_LOCATION irpSp;
    PCHAR SystemBuffer;

    SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
				("[LFS] LfsQueryDirectoryByIndex: FileIndex=%x\n", FileIndex));
	
    status = ObReferenceObjectByHandle( FileHandle,
                                        FILE_LIST_DIRECTORY,
                                        NULL,
                                        KernelMode,
                                        (PVOID *) &fileObject,
                                        (POBJECT_HANDLE_INFORMATION) NULL );

    if (!NT_SUCCESS( status )) {
		return status;
    }

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    KeClearEvent( &fileObject->Event );

    DeviceObject = IoGetRelatedDeviceObject( fileObject );

    irp = IoAllocateIrp( DeviceObject->StackSize, TRUE );

    if (!irp) {
        ObDereferenceObject( fileObject );
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    irp->Flags = (ULONG)IRP_SYNCHRONOUS_API;
    irp->RequestorMode = KernelMode;

    irp->UserIosb = IoStatusBlock;
    irp->UserEvent = NULL;

    irp->Overlay.AsynchronousParameters.UserApcRoutine = NULL;
    irp->AssociatedIrp.SystemBuffer = (PVOID) NULL;
    SystemBuffer = NULL;

    irp->Tail.Overlay.Thread = PsGetCurrentThread();
    irp->Tail.Overlay.OriginalFileObject = fileObject;
    irp->Tail.Overlay.AuxiliaryBuffer = NULL;
    irp->MdlAddress = NULL;

    irpSp = IoGetNextIrpStackLocation( irp );
    irpSp->MajorFunction = IRP_MJ_DIRECTORY_CONTROL;
    irpSp->MinorFunction = IRP_MN_QUERY_DIRECTORY;
    irpSp->FileObject = fileObject;

    irpSp->Parameters.QueryDirectory.Length = Length;
    irpSp->Parameters.QueryDirectory.FileInformationClass = FileInformationClass; //FileBothDirectoryInformation;
    irpSp->Parameters.QueryDirectory.FileIndex = FileIndex;

    irpSp->Flags = (ReturnSingleEntry?SL_RETURN_SINGLE_ENTRY:0) | SL_INDEX_SPECIFIED;

	if (FileName && FileName->Length) {
	    SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
					("[LFS] LfsQueryDirectoryByIndex: FileName=%wZ\n", FileName));
#if WINVER >= 0x0502 // Windows 2003 or later
		irpSp->Parameters.QueryDirectory.FileName = FileName;
#else
		irpSp->Parameters.QueryDirectory.FileName = (PSTRING)FileName;
#endif

	} else {
	    SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
					("[LFS] LfsQueryDirectoryByIndex: No file name\n"));
		irpSp->Parameters.QueryDirectory.FileName = NULL;
	}

    if (DeviceObject->Flags & DO_BUFFERED_IO) {
        try {
            SystemBuffer = ExAllocatePoolWithQuotaTag( NonPagedPool,
                                                       Length,
                                                       'QSFL' );
            irp->AssociatedIrp.SystemBuffer = SystemBuffer;
        } except(EXCEPTION_EXECUTE_HANDLER) {
            IoFreeIrp(irp);
            ObDereferenceObject( fileObject );
            return GetExceptionCode();
        }
    } else if (DeviceObject->Flags & DO_DIRECT_IO) {
        mdl = (PMDL) NULL;
        try {
            mdl = IoAllocateMdl( FileInformation, Length, FALSE, TRUE, irp );
            if (mdl == NULL) {
                ExRaiseStatus( STATUS_INSUFFICIENT_RESOURCES );
            }
            MmProbeAndLockPages( mdl, UserMode, IoWriteAccess );
        } except(EXCEPTION_EXECUTE_HANDLER) {
            if (irp->MdlAddress != NULL) {
                 IoFreeMdl( irp->MdlAddress );
            }
            IoFreeIrp(irp);
            ObDereferenceObject( fileObject );
            return GetExceptionCode();
        }
    } else {
        irp->UserBuffer = FileInformation;
    }

#if 0 /* what's this?? */
    KeRaiseIrql( APC_LEVEL, &irql );
    InsertHeadList( &((PETHREAD)irp->Tail.Overlay.Thread)->IrpList,
                    &irp->ThreadListEntry );
    KeLowerIrql( irql );
#endif

    IoSetCompletionRoutine( irp, 
                            LfsQueryDirectoryByIndexCompletion, 
                            &Event, 
                            TRUE, 
                            TRUE, 
                            TRUE );

    status = IoCallDriver(DeviceObject, irp);

    if (status == STATUS_PENDING) {
        status = KeWaitForSingleObject(
                     &Event,
                     Executive,
                     KernelMode,
                     FALSE,
                     NULL );
    }

    if (NT_SUCCESS(status)) {
        status = IoStatusBlock->Status;
        if (NT_SUCCESS(status) || status == STATUS_BUFFER_OVERFLOW) {
            if (SystemBuffer) {
                try {
                    RtlCopyMemory( FileInformation,
                                   SystemBuffer,
                                   IoStatusBlock->Information
                                   );

                } except(EXCEPTION_EXECUTE_HANDLER) {
                    status = GetExceptionCode();
                }
            }
        }
    }

    if (SystemBuffer) {
        ExFreePool(SystemBuffer);
    }
    ObDereferenceObject( fileObject );
    
    return status;
}


//
// Return (ULONG)-1 for no or last query.
//
ULONG
LfsGetLastFileIndexFromQuery(
		ULONG			infoType,
		PCHAR			QueryBuff,
		ULONG			BuffLength
) {
	ULONG	offset;
	ULONG	count;
	ULONG	FileIndex = (ULONG)-1;
	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "LfsGetLastFileIndexFromQuery\n"));
	switch(infoType) {
		case FileDirectoryInformation: {
			PFILE_DIRECTORY_INFORMATION	info;

			offset = 0;
			count = 0;
			while(offset < BuffLength) {
				info = (PFILE_DIRECTORY_INFORMATION)(QueryBuff + offset);
				count ++;
				offset += info->NextEntryOffset;
				FileIndex = info->FileIndex;
				if(!info->NextEntryOffset) break;
			}
			break;
		}
		case FileFullDirectoryInformation: {
			PFILE_FULL_DIR_INFORMATION	info;
			offset = 0;
			count = 0;
			while(offset < BuffLength) {
				info = (PFILE_FULL_DIR_INFORMATION)(QueryBuff + offset);
				count ++;
				offset += info->NextEntryOffset;
				FileIndex = info->FileIndex;
				if(!info->NextEntryOffset) break;
			}
			break;
		}
		case FileBothDirectoryInformation: {
			PFILE_BOTH_DIR_INFORMATION	info;

			offset = 0;
			count = 0;
			while(offset < BuffLength) {
				info = (PFILE_BOTH_DIR_INFORMATION)(QueryBuff + offset);

				count ++;
				offset += info->NextEntryOffset;
				FileIndex = info->FileIndex;
				if(!info->NextEntryOffset) break;
			}
			break;
		}
		case FileIdBothDirectoryInformation: {
			PFILE_ID_BOTH_DIR_INFORMATION	info;

			offset = 0;
			count = 0;
			while(offset < BuffLength) {
				info = (PFILE_ID_BOTH_DIR_INFORMATION)(QueryBuff + offset);
				count ++;
				offset += info->NextEntryOffset;
				FileIndex = info->FileIndex;

				if(!info->NextEntryOffset) break;
			}

			break;
		}
		case FileIdFullDirectoryInformation: {
			PFILE_ID_FULL_DIR_INFORMATION	info;

			offset = 0;
			count = 0;
			while(offset < BuffLength) {
				info = (PFILE_ID_FULL_DIR_INFORMATION)(QueryBuff + offset);


				count ++;
				offset += info->NextEntryOffset;
				FileIndex = info->FileIndex;

				if(!info->NextEntryOffset) break;
			}

			break;
		}
		case FileNamesInformation: {
			PFILE_NAMES_INFORMATION	info;

			offset = 0;
			count = 0;
			while(offset < BuffLength) {
				info = (PFILE_NAMES_INFORMATION)(QueryBuff + offset);

				count ++;
				offset += info->NextEntryOffset;
				FileIndex = info->FileIndex;

				if(!info->NextEntryOffset) break;
			}
			break;
		}
	}
	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "LfsGetLastFileIndexFromQuery returning %x\n", FileIndex));
	return FileIndex;
}


