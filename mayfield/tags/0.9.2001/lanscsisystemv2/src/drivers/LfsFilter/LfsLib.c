#define __LANSCSI_BUS__
#include "LfsProc.h"

#include <ntddscsi.h>
#include <initguid.h>
#include <stdio.h>

#include <LanScsi.h>
#include "LSMPIoctl.h"

#define MAX_TRANSMIT_DATA	(64*1024)

NTSTATUS
SendMessage(
	PFILE_OBJECT		ConnectionFileObject,
	_U8					*Buf, 
	LONG				Size,
	PULONG				TotalSent
	)
{
	NTSTATUS	ntStatus;
	ULONG		result;
	ULONG		remaining;
	ULONG		onceReqSz;

	ASSERT(Size > 0);

	//
	//	send a packet
	//
	for(remaining = Size ; remaining > 0 ;  ) 
	{
		onceReqSz = (remaining < MAX_TRANSMIT_DATA)?remaining:MAX_TRANSMIT_DATA;

		ntStatus = LpxTdiSend(
				ConnectionFileObject,
				Buf,
				onceReqSz,
				0,
				&result
				);

		if(!NT_SUCCESS(ntStatus) || STATUS_PENDING == ntStatus) 
		{
		    SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
				("SendMessage: sending failed.\n"));
			break ;
		} 
		else if( 0 >= result ) 
		{
		    SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
				("SendMessage: result less than 0.\n"));
			break ;
		}

		remaining -= result;
		((PCHAR)Buf) += result;
	}

	if(remaining) {
	    SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
				("SendMessage: unexpected data length sent. remaining:%lu\n", remaining)) ;
		//ASSERT(FALSE);
			ntStatus = STATUS_UNSUCCESSFUL ;
	}

	if(TotalSent != NULL)
		*TotalSent = Size-remaining;

	return ntStatus ;
}


NTSTATUS
RecvMessage(
	PFILE_OBJECT		ConnectionFileObject,
	_U8					*Buf, 
	LONG				Size,
	PULONG				Received
	)
{
	NTSTATUS	ntStatus ;
	ULONG		result ;
	ULONG		remaining ;
	ULONG		onceReqSz ;

	ASSERT(Size > 0);
	//
	//	send a packet
	//
	for(remaining = Size ; remaining > 0 ;  ) 
	{
		onceReqSz = (remaining < MAX_TRANSMIT_DATA) ? remaining:MAX_TRANSMIT_DATA ;

		ntStatus = LpxTdiRecv(
				ConnectionFileObject,
				Buf,
				onceReqSz,
				0,
				&result
			) ;

		if(!NT_SUCCESS(ntStatus) || STATUS_PENDING == ntStatus) {
		    SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
				("RecvMessage: Receiving failed.\n")) ;
			break ;
		} else if( 0 >= result ) {
		    SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
				("RecvMessage: result less than 0.\n")) ;
			break ;
		}

		remaining -= result ;
		((PCHAR)Buf) += result ;
	}

	if(remaining) {
	    SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
				("RecvMessage: unexpected data length received. remaining:%lu\n", remaining));
		//ASSERT(FALSE);
			ntStatus = STATUS_UNSUCCESSFUL ;
	}

	if(Received != NULL)
		*Received = Size-remaining;

	return ntStatus ;
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
	PLSMPIOCTL_QUERYINFO		QueryInfo ;
	PLSMPIOCTL_PRIMUNITDISKINFO	tmpPrimUnitDisk ;


	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL) ;	

	outbuff_sz = sizeof(SRB_IO_CONTROL) + sizeof(LSMPIOCTL_QUERYINFO) + sizeof(LSMPIOCTL_PRIMUNITDISKINFO) ;
	psrbioctl = (PSRB_IO_CONTROL)ExAllocatePool(NonPagedPool , outbuff_sz) ;
	if(psrbioctl == NULL) {
        SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
						("STATUS_INSUFFICIENT_RESOURCES\n"));
		return FALSE;
	}

	memset(psrbioctl, 0, sizeof(*psrbioctl)) ;
	psrbioctl->HeaderLength = sizeof(SRB_IO_CONTROL) ;
	memcpy(psrbioctl->Signature, LANSCSIMINIPORT_IOCTL_SIGNATURE, 8) ;
	psrbioctl->Timeout = 10 ;
	psrbioctl->ControlCode = LANSCSIMINIPORT_IOCTL_QUERYINFO_EX;
	psrbioctl->Length = sizeof(LSMPIOCTL_QUERYINFO) + sizeof(LSMPIOCTL_PRIMUNITDISKINFO) ;

	QueryInfo = (PLSMPIOCTL_QUERYINFO)((PUCHAR)psrbioctl + sizeof(SRB_IO_CONTROL)) ;
	tmpPrimUnitDisk = (PLSMPIOCTL_PRIMUNITDISKINFO)((PUCHAR)psrbioctl + sizeof(SRB_IO_CONTROL)) ;

	QueryInfo->Length = sizeof(LSMPIOCTL_QUERYINFO) ;
	QueryInfo->InfoClass = LsmpPrimaryUnitDiskInformation ;
	QueryInfo->QueryDataLength = 0 ;

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
		ASSERT(tmpPrimUnitDisk->Length == sizeof(LSMPIOCTL_PRIMUNITDISKINFO)) ;

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
						("desiredAccess = %x, grantedAccess = %x, GENERIC_WRITE = %x\n", 
						tmpPrimUnitDisk->UnitDisk.DesiredAccess,tmpPrimUnitDisk->UnitDisk.GrantedAccess, GENERIC_WRITE));

		RtlCopyMemory(PrimUnitDisk, tmpPrimUnitDisk, sizeof(LSMPIOCTL_PRIMUNITDISKINFO)) ;
		
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
	SCSI_ADDRESS		ScsiAddress ;
	NTSTATUS			ntStatus ;
	UNICODE_STRING		ScsiportAdapterName ;
	WCHAR				ScsiportAdapterNameBuffer[32] ;
	WCHAR				ScsiportAdapterNameTemp[32]	= L"" ;
    OBJECT_ATTRIBUTES	objectAttributes ;
    HANDLE				fileHandle					= NULL ;
	IO_STATUS_BLOCK		IoStatus ;
	PFILE_OBJECT		ScsiportDeviceFileObject	= NULL ;

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
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("GetScsiportAdapter: LfsFilterDeviceIoControl() failed.\n")) ;
		goto error_out ;

	}

    SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("GetScsiportAdapter: ScsiAddress=Len:%d PortNumber:%d PathId:%d TargetId:%d Lun:%d\n",
						(LONG)ScsiAddress.Length,
						(LONG)ScsiAddress.PortNumber,
						(LONG)ScsiAddress.PathId,
						(LONG)ScsiAddress.TargetId,
						(LONG)ScsiAddress.Lun
						)) ;


	_snwprintf(
		ScsiportAdapterNameTemp,
		32 * sizeof(WCHAR),
		L"\\Device\\ScsiPort%d",
		ScsiAddress.PortNumber
    );

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
	    SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("GetScsiportAdapter: ZwCreateFile() failed.\n")) ;
		goto error_out ;

	}

    ntStatus = ObReferenceObjectByHandle( fileHandle,
										FILE_READ_DATA,
										*IoFileObjectType,
										KernelMode,
										&ScsiportDeviceFileObject,
										NULL);
    if(!NT_SUCCESS( ntStatus )) {

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("GetScsiportAdapter: ObReferenceObjectByHandle() failed.\n")) ;
        goto error_out ;
    }

	*ScsiportAdapterDeviceObject = IoGetRelatedDeviceObject(
											ScsiportDeviceFileObject
									    );

	if(*ScsiportAdapterDeviceObject == NULL) {

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("GetScsiportAdapter: IoGetRelatedDeviceObject() failed.\n")) ;
		ObDereferenceObject(ScsiportDeviceFileObject) ;
        goto error_out ;
	}

	ObDereferenceObject(ScsiportDeviceFileObject);
	ZwClose(fileHandle);
	ObReferenceObject(*ScsiportAdapterDeviceObject);

	return TRUE ;

error_out:
	
	*ScsiportAdapterDeviceObject = NULL ;
	if(fileHandle)
		ZwClose(fileHandle) ;

	return FALSE ;
}


BOOLEAN	
GetHarddisk(
  	IN	PDEVICE_OBJECT	DiskDeviceObject,
  	IN	PDEVICE_OBJECT	*HarddiskDeviceObject
	) 
{
	NTSTATUS			ntStatus ;
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

	_snwprintf(
		harddiskNameTemp,
		32 * sizeof(WCHAR),
		L"\\DosDevices\\PhysicalDrive%d",
		//L"\\\\.\\PhysicalDrive%d",
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
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("GetScsiportAdapter: ObReferenceObjectByHandle() failed.\n")) ;
		ZwClose(harddiskFileHandle);
		return FALSE;
    }

	*HarddiskDeviceObject = IoGetRelatedDeviceObject(harddiskDeviceFileObject);

	if(*HarddiskDeviceObject == NULL) 
	{
		ASSERT(LFS_UNEXPECTED);
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("GetScsiportAdapter: IoGetRelatedDeviceObject() failed.\n")) ;
		ObDereferenceObject(harddiskDeviceFileObject);
		ZwClose(harddiskFileHandle);
		return FALSE;
	}

	ObReferenceObject(*HarddiskDeviceObject);
	ObDereferenceObject(harddiskDeviceFileObject);
	ZwClose(harddiskFileHandle);

	return TRUE ;
}


BOOLEAN
IsNetDiskPartition(
	IN	PDEVICE_OBJECT					DiskDeviceObject,
	OUT PLOCAL_NETDISK_PARTITION_INFO	LocalNetDiskPartitionInfo
	) 
{
	BOOLEAN						BRet ;
	LSMPIOCTL_PRIMUNITDISKINFO	PrimUnitDisk ;
	//PARTITION_INFORMATION_EX	partitionInformationEx;
	PARTITION_INFORMATION		partitionInformation;
	NTSTATUS					statusPartInfo;
	PDEVICE_OBJECT				ScsiportAdapterDeviceObject;

	//
	//	track ScsiPort Device Object
	//
	ScsiportAdapterDeviceObject = NULL;
	
	BRet = GetScsiportAdapter(DiskDeviceObject, &ScsiportAdapterDeviceObject) ;
	if(BRet != TRUE) {
	    SPY_LOG_PRINT( LFS_DEBUG_LFS_ERROR, ("IsNetDiskPartition: GetScsiportAdapter() failed.\n")) ;
		return FALSE ;
	}

	BRet = GetPrimaryUnitDisk(
			ScsiportAdapterDeviceObject,
			&PrimUnitDisk
		) ;

	if(BRet == TRUE && LocalNetDiskPartitionInfo) 
	{
		LocalNetDiskPartitionInfo->DesiredAccess = PrimUnitDisk.UnitDisk.DesiredAccess ;
		LocalNetDiskPartitionInfo->GrantedAccess = PrimUnitDisk.UnitDisk.GrantedAccess ;
		LocalNetDiskPartitionInfo->MessageSecurity = FALSE;
		LocalNetDiskPartitionInfo->RwDataSecurity = FALSE;

		RtlCopyMemory(&LocalNetDiskPartitionInfo->BindAddress, &PrimUnitDisk.UnitDisk.BindingAddress , sizeof(LPX_ADDRESS)) ;

		LocalNetDiskPartitionInfo->NetDiskPartitionInfo.EnabledTime.QuadPart = PrimUnitDisk.EnabledTime.QuadPart;

		RtlCopyMemory(&LocalNetDiskPartitionInfo->NetDiskPartitionInfo.NetDiskAddress, &PrimUnitDisk.UnitDisk.NetDiskAddress, sizeof(LPX_ADDRESS) ) ;
		LocalNetDiskPartitionInfo->NetDiskPartitionInfo.UnitDiskNo = PrimUnitDisk.UnitDisk.UnitDiskId ;
		RtlCopyMemory(
			LocalNetDiskPartitionInfo->NetDiskPartitionInfo.UserId,
			PrimUnitDisk.UnitDisk.UserID,
			sizeof(LocalNetDiskPartitionInfo->NetDiskPartitionInfo.UserId)
			);
		RtlCopyMemory(
			LocalNetDiskPartitionInfo->NetDiskPartitionInfo.Password,
			PrimUnitDisk.UnitDisk.Password,
			sizeof(LocalNetDiskPartitionInfo->NetDiskPartitionInfo.Password)
			);

	    SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("Password: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
									LocalNetDiskPartitionInfo->NetDiskPartitionInfo.Password[0], LocalNetDiskPartitionInfo->NetDiskPartitionInfo.Password[1],
									LocalNetDiskPartitionInfo->NetDiskPartitionInfo.Password[2], LocalNetDiskPartitionInfo->NetDiskPartitionInfo.Password[3],
									LocalNetDiskPartitionInfo->NetDiskPartitionInfo.Password[4], LocalNetDiskPartitionInfo->NetDiskPartitionInfo.Password[5],
									LocalNetDiskPartitionInfo->NetDiskPartitionInfo.Password[6], LocalNetDiskPartitionInfo->NetDiskPartitionInfo.Password[7]));

		LocalNetDiskPartitionInfo->SlotNo = PrimUnitDisk.UnitDisk.SlotNo;
		//
		//	get a starting offset of the partition
		//

#if 0
		statusPartInfo = LfsFilterDeviceIoControl( 
								DiskDeviceObject,
			                    IOCTL_DISK_GET_PARTITION_INFO_EX,
								NULL,
								0,
						        &partitionInformationEx,
							    sizeof(PARTITION_INFORMATION_EX),
								NULL 
								);
#endif

		statusPartInfo = LfsFilterDeviceIoControl( 
								DiskDeviceObject,
			                    IOCTL_DISK_GET_PARTITION_INFO,
								NULL,
								0,
						        &partitionInformation,
							    sizeof(PARTITION_INFORMATION),
								NULL 
								);

		if(!NT_SUCCESS(statusPartInfo)) {
		    SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("LFSS: LfsFilterDeviceIoControl() failed.\n")) ;
			BRet = FALSE ;
			goto cleanup ;
		}

//	LocalNetDiskPartitionInfo->NetDiskPartitionInfo.StartingOffset.QuadPart = partitionInformationEx.StartingOffset.QuadPart ;
		LocalNetDiskPartitionInfo->NetDiskPartitionInfo.StartingOffset.QuadPart = partitionInformation.StartingOffset.QuadPart ;

	    SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("LFS: IsNetDiskPartition: DesiredAccess=%08x, GrantedAccess=%08x\n",
							PrimUnitDisk.UnitDisk.DesiredAccess,
							PrimUnitDisk.UnitDisk.GrantedAccess
			));
	}

cleanup:
	if(ScsiportAdapterDeviceObject)
		ObDereferenceObject(ScsiportAdapterDeviceObject);
	return BRet ;
}


BOOLEAN
IsNetDisk(
	IN	PDEVICE_OBJECT			DiskDeviceObject,
	OUT PNETDISK_INFORMATION	NetdiskInformation
	) 
{
	BOOLEAN						returnResult;
	PDEVICE_OBJECT				scsiportAdapterDeviceObject;
	LSMPIOCTL_PRIMUNITDISKINFO	primUnitDisk;


	returnResult = GetScsiportAdapter(DiskDeviceObject, &scsiportAdapterDeviceObject) ;
	if(returnResult != TRUE) 
	{
	    SPY_LOG_PRINT( LFS_DEBUG_LFS_ERROR, ("IsNetDisk: GetScsiportAdapter() failed.\n")) ;
		return FALSE;
	}

	returnResult = GetPrimaryUnitDisk(
					scsiportAdapterDeviceObject,
					&primUnitDisk
					);

	if(returnResult == TRUE && NetdiskInformation)
	{
		RtlCopyMemory(&NetdiskInformation->NetDiskAddress,primUnitDisk.UnitDisk.NetDiskAddress.Address[0].Address, sizeof(LPX_ADDRESS)); 
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
		RtlCopyMemory(&NetdiskInformation->BindAddress, primUnitDisk.UnitDisk.BindingAddress.Address[0].Address, sizeof(LPX_ADDRESS));
	}

	if(scsiportAdapterDeviceObject)
		ObDereferenceObject(scsiportAdapterDeviceObject);

	return returnResult;
}


//////////////////////////////////////////////////////////////////////////
//
//
// added by hootch 01172004
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

	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlOpenLanScsiBus: entered.\n"));

	ASSERT(LanScsiDev) ;
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

	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlOpenLanScsiBus: symbolicLinkList = %ws\n", symbolicLinkList));

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
		return ntStatus ;
	}

	ExFreePool(symbolicLinkList);

	*LanScsiDev = deviceObject ;
	*FileObject = fileObject ;

	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "NDCtrlOpenLanScsiBus: Done.\n"));

	return ntStatus ;
}

VOID
NDCtrlCloseLanScsiBus(
		PFILE_OBJECT FileObject
	) {
	ASSERT(FileObject) ;

	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "NDCtrlCloseLanScsiBus: Closed LanScsiBus interface.\n"));

	ObDereferenceObject(FileObject) ;

}


NTSTATUS
NDCtrlUpgradeToWrite(
	ULONG				SlotNo,
	USHORT				UnitDiskNo,
	PIO_STATUS_BLOCK	IoStatus
) 
{
	NTSTATUS			ntStatus;
	PFILE_OBJECT		fileObject;
	PDEVICE_OBJECT		deviceObject;
	PIRP				irp;
    KEVENT				event;
	BUSENUM_UPGRADE_TO_WRITE	Param ;

	ASSERT(KeGetCurrentIrql() <= PASSIVE_LEVEL ) ;
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlUpradeToWrite: SlotNo:%ld UnitDiskNo:%d\n", SlotNo, UnitDiskNo));

	ntStatus = NDCtrlOpenLanScsiBus(&deviceObject, &fileObject) ;
	if(!NT_SUCCESS(ntStatus)) {
		return ntStatus ;
	}

	Param.Size = sizeof(BUSENUM_UPGRADE_TO_WRITE) ;
	Param.SlotNo = SlotNo * 10 + UnitDiskNo;
	
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

		ntStatus = STATUS_INSUFFICIENT_RESOURCES ;

		goto cleanup ;
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
//		ASSERT(FALSE) ;
	}

cleanup:

	NDCtrlCloseLanScsiBus(fileObject) ;
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlUpgradeToWrite: Done...\n"));

	
	return ntStatus ;
}


NTSTATUS
NDCtrlUnplug(
	ULONG				SlotNo,
	USHORT				UnitDiskNo,
	PIO_STATUS_BLOCK	IoStatus
) 
{
	NTSTATUS				ntStatus;
	PFILE_OBJECT			fileObject;
	PDEVICE_OBJECT			deviceObject;
	PIRP					irp;
    KEVENT					event;

    BUSENUM_UNPLUG_HARDWARE unplug;

	UNREFERENCED_PARAMETER(UnitDiskNo);

	ASSERT(KeGetCurrentIrql() <= PASSIVE_LEVEL ) ;
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlUnplug: SlotNo:%ld UnitDiskNo:%d\n", SlotNo, UnitDiskNo));

	ntStatus = NDCtrlOpenLanScsiBus(&deviceObject, &fileObject) ;
	if(!NT_SUCCESS(ntStatus)) {
		return ntStatus ;
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
			IoStatus
		);

    if (irp == NULL) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlUnplug: irp NULL\n"));

		ntStatus = STATUS_INSUFFICIENT_RESOURCES ;

		goto cleanup ;
	}

	ntStatus = IoCallDriver(deviceObject, irp);
	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlUnplug: NTSTATUS:%lx IOSTATUS:%lx\n", ntStatus, IoStatus->Status));
    if (ntStatus == STATUS_PENDING)
    {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlUnplug: IoCallDriver STATUS_PENDING\n"));
		KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
    }
	if (!NT_SUCCESS (ntStatus)) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlUnplug: IoCallDriver() failed.\n"));
//		ASSERT(FALSE) ;
	}

cleanup:

	NDCtrlCloseLanScsiBus(fileObject) ;
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] NDCtrlUnplug: Done...\n"));

	
	return ntStatus ;
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
} ;


#define DEBUG_BUFFER_LENGTH 512

PWCHAR
NullTerminateUnicode(
	IN ULONG		UnicodeStringLength,
	IN PWCHAR		UnicodeString,
	IN ULONG		BufferLength,
	IN PWCHAR		Buffer
) {

	ULONG	printLength ;

	BufferLength -= sizeof(WCHAR) ;		// occupation of NULL termination.
	if(BufferLength <= 0) return L"Not enough buffer" ;

	printLength = BufferLength < UnicodeStringLength ? BufferLength : UnicodeStringLength ;
	RtlCopyMemory(Buffer, UnicodeString, printLength) ;
	Buffer[printLength / 2] = L'\0' ;

	return Buffer ;

}



VOID
PrintFileInfoClass(
		ULONG			infoType,
		ULONG			systemBuffLength,
		PCHAR			systemBuff
) {
	ULONG	offset ;
	ULONG	count ;
#if DBG
	WCHAR	UnicodeBuffer[DEBUG_BUFFER_LENGTH] ;
#endif

	SPY_LOG_PRINT( LFS_DEBUG_LFS_NOISE,( "[LFS] PrintFileInfoClass:\n"));
	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileInformationClass: %s(%d)\n",
						FileInformationClassString[infoType],
						infoType
					));
	switch(infoType) {
	case FileDirectoryInformation: {
		PFILE_DIRECTORY_INFORMATION	info ;

		offset = 0 ;
		count = 0 ;
		while(offset < systemBuffLength) {
			info = (PFILE_DIRECTORY_INFORMATION)(systemBuff + offset) ;

			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "  * entry #%lu *\n", count)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "NextEntryOffset	: %lu\n",info->NextEntryOffset )) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileIndex		: %lu\n", info->FileIndex)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "CreationTime	: %I64u\n", info->CreationTime.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastAccessTime	: %I64u\n", info->LastAccessTime.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastWriteTime	: %I64u\n", info->LastWriteTime.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ChangeTime		: %I64u\n", info->ChangeTime.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "EndOfFile		: %I64u\n", info->EndOfFile.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "AllocationSize	: %I64u\n", info->AllocationSize.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileAttributes	: %lx\n", info->FileAttributes)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileNameLength	: %lu\n", info->FileNameLength)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileName			: '%ws'\n", 
								NullTerminateUnicode(	info->FileNameLength,
									(PWCHAR)info->FileName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				)) ;
	
			count ++ ;
			offset += info->NextEntryOffset ;

			if(!info->NextEntryOffset) break ;
		}
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "%lu chunks printed\n", count)) ;

		break ;
	}
	case FileFullDirectoryInformation: {
		PFILE_FULL_DIR_INFORMATION	info ;

		offset = 0 ;
		count = 0 ;
		while(offset < systemBuffLength) {
			info = (PFILE_FULL_DIR_INFORMATION)(systemBuff + offset) ;

			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "  * entry #%lu *\n", count)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "NextEntryOffset	: %lu\n",info->NextEntryOffset ) );
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileIndex		: %lu\n", info->FileIndex)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "CreationTime	: %I64u\n", info->CreationTime.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastAccessTime	: %I64u\n", info->LastAccessTime.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastWriteTime	: %I64u\n", info->LastWriteTime.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ChangeTime		: %I64u\n", info->ChangeTime.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "EndOfFile		: %I64u\n", info->EndOfFile.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "AllocationSize	: %I64u\n", info->AllocationSize.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileAttributes	: %lx\n", info->FileAttributes)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileNameLength	: %lu\n", info->FileNameLength)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "EaSize			: %lu\n", info->EaSize)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileName			: '%ws'\n", 
								NullTerminateUnicode(	info->FileNameLength,
									(PWCHAR)info->FileName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				)) ;
	
			count ++ ;
			offset += info->NextEntryOffset ;

			if(!info->NextEntryOffset) break ;
		}
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "%lu chunks printed\n", count)) ;

		break ;
	}
	case FileBothDirectoryInformation: {
		PFILE_BOTH_DIR_INFORMATION	info ;

		offset = 0 ;
		count = 0 ;
		while(offset < systemBuffLength) {
			info = (PFILE_BOTH_DIR_INFORMATION)(systemBuff + offset) ;

			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "  * entry #%lu *\n", count)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "NextEntryOffset	: %lu\n",info->NextEntryOffset )) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileIndex		: %lu\n", info->FileIndex)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "CreationTime	: %I64u\n", info->CreationTime.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastAccessTime	: %I64u\n", info->LastAccessTime.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastWriteTime	: %I64u\n", info->LastWriteTime.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ChangeTime		: %I64u\n", info->ChangeTime.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "EndOfFile		: %I64u\n", info->EndOfFile.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "AllocationSize	: %I64u\n", info->AllocationSize.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileAttributes	: %lx\n", info->FileAttributes)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileNameLength	: %lu\n", info->FileNameLength)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "EaSize			: %lu\n", info->EaSize)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ShortNameLength	: %u\n", (int)info->ShortNameLength)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ShortName		: '%ws'\n", 
								NullTerminateUnicode(	info->ShortNameLength,
									(PWCHAR)info->ShortName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileName			: '%ws'\n", 
								NullTerminateUnicode(	info->FileNameLength,
									(PWCHAR)info->FileName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				)) ;

			count ++ ;
			offset += info->NextEntryOffset ;

			if(!info->NextEntryOffset) break ;
		}
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "%lu chunks printed\n", count)) ;

		break ;
	}
	case FileIdBothDirectoryInformation: {
		PFILE_ID_BOTH_DIR_INFORMATION	info ;

		offset = 0 ;
		count = 0 ;
		while(offset < systemBuffLength) {
			info = (PFILE_ID_BOTH_DIR_INFORMATION)(systemBuff + offset) ;

			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "  * entry #%lu *\n", count)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "NextEntryOffset	: %lu\n",info->NextEntryOffset )) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileIndex		: %lu\n", info->FileIndex)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "CreationTime	: %I64u\n", info->CreationTime.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastAccessTime	: %I64u\n", info->LastAccessTime.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastWriteTime	: %I64u\n", info->LastWriteTime.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ChangeTime		: %I64u\n", info->ChangeTime.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "EndOfFile		: %I64u\n", info->EndOfFile.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "AllocationSize	: %I64u\n", info->AllocationSize.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileAttributes	: %lx\n", info->FileAttributes)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileNameLength	: %lu\n", info->FileNameLength)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "EaSize			: %lu\n", info->EaSize)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ShortNameLength	: %u\n", (int)info->ShortNameLength)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ShortName		: '%ws'\n", 
								NullTerminateUnicode(	info->ShortNameLength,
									(PWCHAR)info->ShortName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileId			: %I64x\n", info->FileId.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileName			: '%ws'\n", 
								NullTerminateUnicode(	info->FileNameLength,
									(PWCHAR)info->FileName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				)) ;

			count ++ ;
			offset += info->NextEntryOffset ;

			if(!info->NextEntryOffset) break ;
		}
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "%lu chunks printed\n", count)) ;

		break ;
	}
	case FileIdFullDirectoryInformation: {
		PFILE_ID_FULL_DIR_INFORMATION	info ;

		offset = 0 ;
		count = 0 ;
		while(offset < systemBuffLength) {
			info = (PFILE_ID_FULL_DIR_INFORMATION)(systemBuff + offset) ;

			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "  * entry #%lu *\n", count)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "NextEntryOffset	: %lu\n",info->NextEntryOffset )) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileIndex		: %lu\n", info->FileIndex)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "CreationTime	: %I64u\n", info->CreationTime.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastAccessTime	: %I64u\n", info->LastAccessTime.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastWriteTime	: %I64u\n", info->LastWriteTime.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ChangeTime		: %I64u\n", info->ChangeTime.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "EndOfFile		: %I64u\n", info->EndOfFile.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "AllocationSize	: %I64u\n", info->AllocationSize.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileAttributes	: %lx\n", info->FileAttributes)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileNameLength	: %lu\n", info->FileNameLength)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "EaSize			: %lu\n", info->EaSize)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileId			: %I64x\n", info->FileId.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileName			: '%ws'\n", 
								NullTerminateUnicode(	info->FileNameLength,
									(PWCHAR)info->FileName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				)) ;

			count ++ ;
			offset += info->NextEntryOffset ;

			if(!info->NextEntryOffset) break ;
		}
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "%lu chunks printed\n", count)) ;

		break ;
	}
	case FileNamesInformation: {
		PFILE_NAMES_INFORMATION	info ;

		offset = 0 ;
		count = 0 ;
		while(offset < systemBuffLength) {
			info = (PFILE_NAMES_INFORMATION)(systemBuff + offset) ;

			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "  * entry #%lu *\n", count)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "NextEntryOffset	: %lu\n",info->NextEntryOffset )) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileIndex		: %lu\n", info->FileIndex)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileNameLength	: %lu\n", info->FileNameLength)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileName			: '%ws'\n", 
								NullTerminateUnicode(	info->FileNameLength,
									(PWCHAR)info->FileName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				)) ;

			count ++ ;
			offset += info->NextEntryOffset ;

			if(!info->NextEntryOffset) break ;
		}
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "%lu chunks printed\n", count)) ;

		break ;
	}
	case FileObjectIdInformation: {
		PFILE_OBJECTID_INFORMATION	info = (PFILE_OBJECTID_INFORMATION)systemBuff ;

			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileReference	: %I64u\n", info->FileReference)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ObjectId			: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
									info->ObjectId[0], info->ObjectId[1], info->ObjectId[2], info->ObjectId[3],
									info->ObjectId[4], info->ObjectId[5], info->ObjectId[6], info->ObjectId[7],
									info->ObjectId[8], info->ObjectId[9], info->ObjectId[10], info->ObjectId[11],
									info->ObjectId[12], info->ObjectId[13], info->ObjectId[14], info->ObjectId[!5]
							)) ;
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

		break ;
	}
	case FileAllocationInformation: {
		PFILE_ALLOCATION_INFORMATION	info = (PFILE_ALLOCATION_INFORMATION)systemBuff ;

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "AllocationSize	: %I64u\n", info->AllocationSize.QuadPart)) ;
		break ;
	}

	case FileAttributeTagInformation: {
		PFILE_ATTRIBUTE_TAG_INFORMATION	info = (PFILE_ATTRIBUTE_TAG_INFORMATION)systemBuff ;

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileAttributes	: %lx\n", info->FileAttributes )) ;
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ReparseTag		: %lx\n", info->ReparseTag )) ;
		break ;
	}
	case FileBasicInformation: {
		PFILE_BASIC_INFORMATION	info = (PFILE_BASIC_INFORMATION)systemBuff ;

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "CreationTime	: %I64u\n", info->CreationTime.QuadPart)) ;
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastAccessTime	: %I64u\n", info->LastAccessTime.QuadPart)) ;
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastWriteTime	: %I64u\n", info->LastWriteTime.QuadPart)) ;
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "ChangeTime		: %I64u\n", info->ChangeTime.QuadPart)) ;
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileAttributes	: %lx\n", info->FileAttributes)) ;
		break ;
	}
	case FileStandardInformation: {
		PFILE_STANDARD_INFORMATION	info = (PFILE_STANDARD_INFORMATION)systemBuff ;

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "AllocationSize	: %I64u\n", info->AllocationSize.QuadPart)) ;
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "EndOfFile		: %I64u\n", info->EndOfFile.QuadPart)) ;
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "NumberOfLinks	: %lu\n", info->NumberOfLinks)) ;
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "DeletePending	: %u\n", info->DeletePending)) ;
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "Directory		: %u\n", info->Directory)) ;
		break ;
	 }
	case FileStreamInformation: {
		PFILE_STREAM_INFORMATION	info  ;

		offset = 0 ;
		count = 0 ;
		while(offset < systemBuffLength) {
			info = (PFILE_STREAM_INFORMATION)(systemBuff + offset) ;

			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "  * entry #%lu *\n", count)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "NextEntryOffset	: %lu\n",info->NextEntryOffset )) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "CreationTime	: %I64u\n", info->StreamSize.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "LastAccessTime	: %I64u\n", info->StreamAllocationSize.QuadPart)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "StreamNameLength	: %lu\n", info->StreamNameLength)) ;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "StreamName		: '%ws'\n", 
								NullTerminateUnicode(	info->StreamNameLength,
									(PWCHAR)info->StreamName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				)) ;
	
			count ++ ;
			offset += info->NextEntryOffset ;

			if(!info->NextEntryOffset) break ;
		}
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "%lu chunks printed\n", count)) ;
		
		break ;
	}
	case FileEaInformation: {
		PFILE_EA_INFORMATION	info = (PFILE_EA_INFORMATION)systemBuff ;
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "EaSize	: %lu\n",info->EaSize )) ;
		break ;
	}

	case FileNameInformation: {
		PFILE_NAME_INFORMATION	info = (PFILE_NAME_INFORMATION)systemBuff ;
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileNameLength	: %lu\n", info->FileNameLength )) ;
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "FileName       : %ws\n", 
								NullTerminateUnicode(info->FileNameLength,
									(PWCHAR)info->FileName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				)) ;
		break ;
	}

	case FileEndOfFileInformation: {
		PFILE_END_OF_FILE_INFORMATION	info = (PFILE_END_OF_FILE_INFORMATION)systemBuff ;

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("EndOfFile : %I64d", info->EndOfFile)) ;
		break ;
	}

	case FileAllInformation:
	case FileReparsePointInformation:
	case FileQuotaInformation:
	default:
		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ( "[LFS] PrintFileInfoClass:Not supported query type.\n")) ;
	}
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