#include "LfsProc.h"

#ifdef __LFSFILT_SUPPORT_NDASPORT__
#include <ndas/ndasportioctl.h>
#include <initguid.h>
#include <ndas/ndasportguid.h>
#endif

#define NTSTRSAFE_LIB
#include <ntstrsafe.h>


#define MAX_NTFS_METADATA_FILE 13

UNICODE_STRING NtfsMetadataFileNames[] = {

	{ 10, 10, L"\\$Mft" },
	{ 18, 18, L"\\$MftMirr" },
	{ 18, 18, L"\\$LogFile" },
	{ 16, 16, L"\\$Volume" },
	{ 18, 18, L"\\$AttrDef" },
	{ 12, 12, L"\\$Root" },
	{ 16, 16, L"\\$Bitmap" },
	{ 12, 12, L"\\$Boot" },
	{ 18, 18, L"\\$BadClus" },
	{ 16, 16, L"\\$Secure" },
	{ 16, 16, L"\\$UpCase" },
	{ 16, 16, L"\\$Extend" },
	{ 22, 22, L"\\$Directory" }
};


BOOLEAN	
IsMetaFile (
	IN PUNICODE_STRING	fileName
	) 
{
	LONG			idx_metafile;

	for (idx_metafile = 0; idx_metafile < MAX_NTFS_METADATA_FILE; idx_metafile ++) {
		
		if (RtlCompareUnicodeString( NtfsMetadataFileNames + idx_metafile,fileName, TRUE) == 0) {
			
			return TRUE;
		}
	}

	return FALSE;
}

NTSTATUS
RecvIt (
	IN PFILE_OBJECT					ConnectionFileObject,
	OUT PCHAR						RecvBuff, 
	IN  ULONG						RecvLen,
	OUT PULONG						TotalReceived,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedData,
	IN  ULONG						RequestIdx,
	IN  PLARGE_INTEGER				TimeOut
	)
{
	INT			len = RecvLen;
	NTSTATUS	status;
	LONG		result;
	
	if (TotalReceived) {

		*TotalReceived = 0;
	}

	if (OverlappedData) {

		//	Asynchronous

		if (TotalReceived) {

			return STATUS_INVALID_PARAMETER;
		}

		status = LpxTdiV2Recv( ConnectionFileObject,
							   RecvBuff,
							   RecvLen,
							   0,
							   NULL,
							   OverlappedData,
							   RequestIdx,
							   NULL );


		if (status != STATUS_PENDING) {

#if 0
			PLANSCSI_SESSION LSS = (PLANSCSI_SESSION)CONTAINING_RECORD(NdastConnectionFile, LANSCSI_SESSION, NdastConnectionFile);

			NDAS_ASSERT( !NT_SUCCESS(status) );

			KDPrintM( DBG_PROTO_ERROR, 
					 ("Can't recv! %02x:%02x:%02x:%02x:%02x:%02x\n",
					  LSS->NdasNodeAddress.Address[2],
					  LSS->NdasNodeAddress.Address[3],
					  LSS->NdasNodeAddress.Address[4],
					  LSS->NdasNodeAddress.Address[5],
					  LSS->NdasNodeAddress.Address[6],
					  LSS->NdasNodeAddress.Address[7]) );
#endif
		}

	} else {

		len = RecvLen;

		while (len > 0) {
		
			result = 0;

			status = LpxTdiV2Recv( ConnectionFileObject,
								   RecvBuff,
								   len,
								   0,
								   TimeOut,
								   NULL,
								   0,
								   &result );

			if (status != STATUS_SUCCESS) {

#if 0
				PLANSCSI_SESSION LSS = (PLANSCSI_SESSION)CONTAINING_RECORD(NdastConnectionFile, LANSCSI_SESSION, NdastConnectionFile);
			
				KDPrintM( DBG_PROTO_ERROR, 
						 ("Can't recv! %02x:%02x:%02x:%02x:%02x:%02x\n",
						  LSS->NdasNodeAddress.Address[2],
						  LSS->NdasNodeAddress.Address[3],
						  LSS->NdasNodeAddress.Address[4],
						  LSS->NdasNodeAddress.Address[5],
						  LSS->NdasNodeAddress.Address[6],
						  LSS->NdasNodeAddress.Address[7]) );
#endif

				break;
			}

			//KDPrintM( DBG_PROTO_NOISE, ("len %d, result %d \n", len, result) );

			//	LstransReceive() must guarantee more than one byte is received
			//	when return SUCCESS

			len -= result;
			RecvBuff += result;
		}

		if (status == STATUS_SUCCESS) {

			NDAS_ASSERT( len == 0 );
			
			if (TotalReceived) {
			
				*TotalReceived = RecvLen;
			}
		
		} else {

			if (TotalReceived) {

				*TotalReceived = RecvLen - len;
			}
		}
	}

	return status;
}

NTSTATUS
RecvMessage (
	IN PFILE_OBJECT			ConnectionFileObject,
	IN PNDAS_FC_STATISTICS	RecvNdasFcStatistics,
	IN PLARGE_INTEGER		TimeOut,
	IN UINT8				*Buffer, 
	IN UINT32				BufferLength
	)
{
	NTSTATUS		status;
	UINT32			remainDataLength;
	UINT32			chooseDataLength;
	LARGE_INTEGER	startTime;
	LARGE_INTEGER	endTime;

	return RecvIt( ConnectionFileObject,
				   Buffer,
				   BufferLength,
				   NULL,
				   NULL,
				   0,
				   TimeOut );


	chooseDataLength = NdasFcChooseTransferSize( RecvNdasFcStatistics, BufferLength );

	startTime = NdasCurrentTime();

	remainDataLength = BufferLength;

	do {

		status = RecvIt( ConnectionFileObject,
						 Buffer + BufferLength - remainDataLength,
						 (remainDataLength < chooseDataLength) ? remainDataLength : chooseDataLength,
						 NULL,
						 NULL,
						 0,
						 TimeOut );

		if (status != STATUS_SUCCESS) {

			SPY_LOG_PRINT( LFS_DEBUG_LFS_ERROR, ("Error when Send data\n") );

			return status;
		}

		if (remainDataLength > chooseDataLength) {

			remainDataLength -= chooseDataLength;
			
		} else {

			remainDataLength = 0;
		}

	} while (remainDataLength);

	endTime = NdasCurrentTime();

	NdasFcUpdateTrasnferSize( RecvNdasFcStatistics, 
							  chooseDataLength, 
							  BufferLength,
							  startTime, 
							  endTime );

	return STATUS_SUCCESS;
}

NTSTATUS
SendIt (
	IN PFILE_OBJECT					ConnectionFileObject,
	IN  PCHAR						SendBuff, 
	IN  UINT32						SendLen,
	OUT PULONG						TotalSent,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	OverlappedData,
	IN  ULONG						RequestIdx,
	IN  PLARGE_INTEGER				TimeOut
	)
{
	NTSTATUS	status;
	UINT32		result;

	if (TotalSent) {

		*TotalSent = 0;
	}

	if (OverlappedData) {

		//	Asynchronous

		if (TotalSent) {

			NDAS_ASSERT( FALSE );
			return STATUS_INVALID_PARAMETER;
		}

		status = LpxTdiV2Send( ConnectionFileObject,
							   SendBuff,
							   SendLen,
							   0,
							   NULL,
							   OverlappedData,
							   RequestIdx,
							   NULL );

#if 0
		if (status != STATUS_PENDING) {

			PLANSCSI_SESSION LSS = (PLANSCSI_SESSION)CONTAINING_RECORD(NdastConnectionFile, LANSCSI_SESSION, NdastConnectionFile);

			NDAS_ASSERT( !NT_SUCCESS(status) );
			
			KDPrintM( DBG_PROTO_ERROR, 
					  ("Can't send! %02x:%02x:%02x:%02x:%02x:%02x\n",
					   LSS->NdasNodeAddress.Address[2],
					   LSS->NdasNodeAddress.Address[3],
					   LSS->NdasNodeAddress.Address[4],
					   LSS->NdasNodeAddress.Address[5],
					   LSS->NdasNodeAddress.Address[6],
					   LSS->NdasNodeAddress.Address[7]) );
		}
#endif

	} else {

		//	Synchronous

		result = 0;

		status = LpxTdiV2Send( ConnectionFileObject,
							   SendBuff,
							   SendLen,
							   0,
							   TimeOut,
							   NULL,
							   0,
							   &result );

		if (result == SendLen) {
		
			if (TotalSent) {

				*TotalSent = SendLen;
			}

		} else {

			//PLANSCSI_SESSION LSS = (PLANSCSI_SESSION)CONTAINING_RECORD(NdastConnectionFile, LANSCSI_SESSION, NdastConnectionFile);

			NDAS_ASSERT( result == 0 );

			if (TotalSent) {

				*TotalSent = 0;
			}

			status = STATUS_PORT_DISCONNECTED;
			
#if 0
			SPY_LOG_PRINT( LFS_DEBUG_LFS_ERROR, 
						  ("Can't send! %02x:%02x:%02x:%02x:%02x:%02x\n",
							LSS->NdasNodeAddress.Address[2],
							LSS->NdasNodeAddress.Address[3],
							LSS->NdasNodeAddress.Address[4],
							LSS->NdasNodeAddress.Address[5],
							LSS->NdasNodeAddress.Address[6],
							LSS->NdasNodeAddress.Address[7]) );
#endif
		}
	}

	return status;
}

NTSTATUS
SendMessage (
	IN PFILE_OBJECT			ConnectionFileObject,
	IN PNDAS_FC_STATISTICS	SendNdasFcStatistics,
	IN PLARGE_INTEGER		TimeOut,
	IN UINT8				*Buffer, 
	IN UINT32				BufferLength
	)
{
	NTSTATUS		status;
	UINT32			remainDataLength;
	UINT32			chooseDataLength;
	LARGE_INTEGER	startTime;
	LARGE_INTEGER	endTime;

	chooseDataLength = NdasFcChooseTransferSize( SendNdasFcStatistics, BufferLength );

	startTime = NdasCurrentTime();

	remainDataLength = BufferLength;

	do {

		status = SendIt( ConnectionFileObject,
						 Buffer + BufferLength - remainDataLength,
						 (remainDataLength < chooseDataLength) ? remainDataLength : chooseDataLength,
						 NULL,
						 NULL,
						 0,
						 TimeOut );

		if (status != STATUS_SUCCESS) {

			SPY_LOG_PRINT( LFS_DEBUG_LFS_ERROR, ("Error when Send data\n") );

			return status;
		}

		if (remainDataLength > chooseDataLength) {

			remainDataLength -= chooseDataLength;
			
		} else {

			remainDataLength = 0;
		}

	} while (remainDataLength);

	endTime = NdasCurrentTime();

	NdasFcUpdateTrasnferSize( SendNdasFcStatistics, 
						  chooseDataLength, 
						  BufferLength,
						  startTime, 
						  endTime );

	return STATUS_SUCCESS;
}

#if 0
NTSTATUS
RecvMessage (
	IN PFILE_OBJECT		ConnectionFileObject,
	OUT UINT8				*Buf, 
	IN LONG				Size,
	IN PLARGE_INTEGER	TimeOut
	)
{
	NTSTATUS	status;
	LONG		result;
	ULONG		remaining;
	ULONG		onceReqSz;


	ASSERT( Size > 0 );

	status = STATUS_SUCCESS;

	//
	//	receive data
	//

	for (remaining = Size; remaining > 0; ) {

		onceReqSz = (remaining < LL_MAX_TRANSMIT_DATA) ? remaining:LL_MAX_TRANSMIT_DATA;

		result = 0;

		status = LpxTdiV2Recv( ConnectionFileObject,
							   Buf,
							   onceReqSz,
							   0,
							   TimeOut,
							   NULL,
							   0,
							   &result );

		if (status != STATUS_SUCCESS) {

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("RecvMessage: Receiving failed.\n") );
			break;
		}

		NDAS_ASSERT( result > 0 );

		remaining -= result;
		((PCHAR)Buf) += result;
	}

	if (remaining) {

	    SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
				("RecvMessage: unexpected data length received. remaining:%lu\n", remaining));

		NDAS_ASSERT( !NT_SUCCESS(status) );
	}

	return status;
}

#endif

NTSTATUS
LfsFilterDeviceIoControl (
    IN  PDEVICE_OBJECT	DeviceObject,
    IN  ULONG			IoCtl,
    IN  PVOID			InputBuffer OPTIONAL,
    IN  ULONG			InputBufferLength,
    IN  PVOID			OutputBuffer OPTIONAL,
    IN  ULONG			OutputBufferLength,
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
    PIRP				irp;
    KEVENT				event;
    IO_STATUS_BLOCK		iosb;
    NTSTATUS			status;

	
	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	KeInitializeEvent( &event, NotificationEvent, FALSE );

    irp = IoBuildDeviceIoControlRequest( IoCtl,
										 DeviceObject,
										 InputBuffer,
										 InputBufferLength,
										 OutputBuffer,
										 OutputBufferLength,
										 FALSE,
										 &event,
										 &iosb );

    if (irp == NULL) {

		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
		return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = IoCallDriver( DeviceObject, irp );

	if (status == STATUS_INSUFFICIENT_RESOURCES ) {

		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
	}

    if (status == STATUS_PENDING) {

        status = KeWaitForSingleObject( &event,
										Executive,
										KernelMode,
										FALSE,
										NULL );
	}

	if (status == STATUS_SUCCESS) {

		status = iosb.Status;
		NDAS_ASSERT( iosb.Status != STATUS_INSUFFICIENT_RESOURCES );

		if (ARGUMENT_PRESENT(IosbInformation)) {

			*IosbInformation = iosb.Information;
		}	
	}

    return status;
}

//////////////////////////////////////////////////////////////////////////
//
//	NDAS SCSI control
// 

NTSTATUS
UpgradeToWrite(
	IN	PDEVICE_OBJECT				TargetDeviceObject,
	OUT	PIO_STATUS_BLOCK			IoStatusBlock
){
	NTSTATUS					status;
	PSRB_IO_CONTROL				psrbioctl;
	int							outbuff_sz;
	PNDSCIOCTL_UPGRADETOWRITE	upgradeToWrite;


	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	//
	// Allocate SRB IO control structure including user buffer space.
	//

	outbuff_sz = sizeof(SRB_IO_CONTROL) + sizeof(NDSCIOCTL_UPGRADETOWRITE);
	psrbioctl = (PSRB_IO_CONTROL)ExAllocatePoolWithTag(NonPagedPool , outbuff_sz, LFS_ALLOC_TAG);
	if(psrbioctl == NULL) {
        SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
						("STATUS_INSUFFICIENT_RESOURCES\n"));
		return FALSE;
	}

	//
	// Init SRB IO control structure
	//

	memset(psrbioctl, 0, sizeof(*psrbioctl));
	psrbioctl->HeaderLength = sizeof(SRB_IO_CONTROL);
	memcpy(psrbioctl->Signature, NDASSCSI_IOCTL_SIGNATURE, 8);
	psrbioctl->Timeout = 10;
	psrbioctl->ControlCode = NDASSCSI_IOCTL_UPGRADETOWRITE;
	psrbioctl->Length = sizeof(NDSCIOCTL_UPGRADETOWRITE);
	psrbioctl->ReturnCode = SRB_STATUS_SUCCESS;

	//
	// Init user buffer
	//

	upgradeToWrite = (PNDSCIOCTL_UPGRADETOWRITE)((PUCHAR)psrbioctl + sizeof(SRB_IO_CONTROL));
	upgradeToWrite->NdasScsiAddress.NdasScsiAddress = 0;

	//
	// Send to the target device
	//

	status = LfsFilterDeviceIoControl(
					TargetDeviceObject,
					IOCTL_SCSI_MINIPORT,
					psrbioctl,
					outbuff_sz,
					psrbioctl,
					outbuff_sz,
					NULL
					);

	if(NT_SUCCESS(status)) 
	{
		// Set return IO status.
		if(IoStatusBlock) {
			IoStatusBlock->Information = 0;
			if(psrbioctl->ReturnCode != SRB_STATUS_SUCCESS) {
					IoStatusBlock->Status = STATUS_UNSUCCESSFUL;
			} else {
					IoStatusBlock->Status = STATUS_SUCCESS;
			}
		}
	} else {
	    SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
			("NdasScsiUpgradeToWrite: UPGRADETOWRITE: failed. status = %x\n", 
					status));
	}

	ExFreePool(psrbioctl);

	return status;
}

BOOLEAN
GetPrimaryUnitDisk(
	IN	PDEVICE_OBJECT				TargetDeviceObject,
	OUT	PNDSCIOCTL_PRIMUNITDISKINFO	PrimUnitDisk
	)
{
	NTSTATUS					status;
	PSRB_IO_CONTROL				psrbioctl;
	int							outbuff_sz;
	PNDASSCSI_QUERY_INFO_DATA		QueryInfo;
	PNDSCIOCTL_PRIMUNITDISKINFO	tmpPrimUnitDisk;


	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);	

	outbuff_sz = sizeof(SRB_IO_CONTROL) + sizeof(NDASSCSI_QUERY_INFO_DATA) + sizeof(NDSCIOCTL_PRIMUNITDISKINFO);
	psrbioctl = (PSRB_IO_CONTROL)ExAllocatePoolWithTag(NonPagedPool , outbuff_sz, LFS_ALLOC_TAG);
	if(psrbioctl == NULL) {
        SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
						("STATUS_INSUFFICIENT_RESOURCES\n"));
		return FALSE;
	}

	memset(psrbioctl, 0, sizeof(*psrbioctl));
	psrbioctl->HeaderLength = sizeof(SRB_IO_CONTROL);
	memcpy(psrbioctl->Signature, NDASSCSI_IOCTL_SIGNATURE, 8);
	psrbioctl->Timeout = 10;
	psrbioctl->ControlCode = NDASSCSI_IOCTL_QUERYINFO_EX;
	psrbioctl->Length = sizeof(NDASSCSI_QUERY_INFO_DATA) + sizeof(NDSCIOCTL_PRIMUNITDISKINFO);

	QueryInfo = (PNDASSCSI_QUERY_INFO_DATA)((PUCHAR)psrbioctl + sizeof(SRB_IO_CONTROL));
	tmpPrimUnitDisk = (PNDSCIOCTL_PRIMUNITDISKINFO)((PUCHAR)psrbioctl + sizeof(SRB_IO_CONTROL));

	QueryInfo->Length = sizeof(NDASSCSI_QUERY_INFO_DATA);
	QueryInfo->InfoClass = NdscPrimaryUnitDiskInformation;
	QueryInfo->QueryDataLength = 0;

	status = LfsFilterDeviceIoControl(
					TargetDeviceObject,
					IOCTL_SCSI_MINIPORT,
					psrbioctl,
					outbuff_sz,
					psrbioctl,
					outbuff_sz,
					NULL
					);

	if(NT_SUCCESS(status)) 
	{
		if(tmpPrimUnitDisk->Length == sizeof(NDSCIOCTL_PRIMUNITDISKINFO)) {
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
				("DevMode = %x, Enabled features = %x, NDASACCRIGHT_WRITE = %x\n", 
				tmpPrimUnitDisk->Lur.DeviceMode, tmpPrimUnitDisk->Lur.EnabledFeatures, NDASFEATURE_SECONDARY));

			RtlCopyMemory(PrimUnitDisk, tmpPrimUnitDisk, sizeof(NDSCIOCTL_PRIMUNITDISKINFO));
		} else {
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
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

NTSTATUS
NdasScsiQuery(
	IN	PDEVICE_OBJECT				TargetDeviceObject,
	IN NDASSCSI_INFORMATION_CLASS	InfoClass,
	OUT PUCHAR						OutBuffer,
	IN ULONG						OutBufferLen,
	OUT PULONG						SrbReturnCode
){
	NTSTATUS					status;
	PSRB_IO_CONTROL				psrbioctl;
	ULONG						srbioctlbuff_sz;
	PNDASSCSI_QUERY_INFO_DATA	QueryInfo;
	PUCHAR						outBuff;
	ULONG						outBuffLen;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	outBuffLen = sizeof(NDASSCSI_QUERY_INFO_DATA) > OutBufferLen?
		sizeof(NDASSCSI_QUERY_INFO_DATA):OutBufferLen;
	srbioctlbuff_sz = sizeof(SRB_IO_CONTROL) + outBuffLen;

	psrbioctl = (PSRB_IO_CONTROL)ExAllocatePoolWithTag(NonPagedPool , srbioctlbuff_sz, LFS_ALLOC_TAG);
	if(psrbioctl == NULL) {
        SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
						("STATUS_INSUFFICIENT_RESOURCES\n"));
		return FALSE;
	}

	memset(psrbioctl, 0, sizeof(*psrbioctl));
	psrbioctl->HeaderLength = sizeof(SRB_IO_CONTROL);
	memcpy(psrbioctl->Signature, NDASSCSI_IOCTL_SIGNATURE, 8);
	psrbioctl->Timeout = 10;
	psrbioctl->ControlCode = NDASSCSI_IOCTL_QUERYINFO_EX;
	psrbioctl->Length = outBuffLen;
	psrbioctl->ReturnCode = SRB_STATUS_SUCCESS;

	QueryInfo = (PNDASSCSI_QUERY_INFO_DATA)((PUCHAR)psrbioctl + sizeof(SRB_IO_CONTROL));
	outBuff = (PUCHAR)QueryInfo;

	QueryInfo->Length = sizeof(NDASSCSI_QUERY_INFO_DATA);
	QueryInfo->InfoClass = InfoClass;
	QueryInfo->QueryDataLength = 0;

	status = LfsFilterDeviceIoControl(
					TargetDeviceObject,
					IOCTL_SCSI_MINIPORT,
					psrbioctl,
					srbioctlbuff_sz,
					psrbioctl,
					srbioctlbuff_sz,
					NULL
					);

	//
	//	Copy the output buffer no matter what status returns
	//

	if(NT_SUCCESS(status))
		RtlCopyMemory(OutBuffer, outBuff, OutBufferLen);

	if(SrbReturnCode)
		*SrbReturnCode = psrbioctl->ReturnCode;

	ExFreePool(psrbioctl);

	return status;
}

NTSTATUS	
GetScsiportAdapter (
  	IN	PDEVICE_OBJECT	DiskDeviceObject,
  	OUT	PDEVICE_OBJECT	*ScsiportAdapterDeviceObject
	) 
{
	NTSTATUS			status;
	SCSI_ADDRESS		ScsiAddress;
	UNICODE_STRING		ScsiportAdapterName;
	WCHAR				ScsiportAdapterNameBuffer[32];
	WCHAR				ScsiportAdapterNameTemp[32]	= L"";
    OBJECT_ATTRIBUTES	objectAttributes;
    HANDLE				fileHandle					= NULL;
	IO_STATUS_BLOCK		IoStatus;
	PFILE_OBJECT		ScsiportDeviceFileObject	= NULL;


	if (DiskDeviceObject == NULL) {

		NDAS_ASSERT( FALSE );
		return STATUS_INVALID_PARAMETER;
	}

	status = LfsFilterDeviceIoControl( DiskDeviceObject,
									   IOCTL_SCSI_GET_ADDRESS,
									   NULL,
									   0,
									   &ScsiAddress,
									   sizeof(SCSI_ADDRESS),
									   NULL );

	if (status != STATUS_SUCCESS) {
	
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("GetScsiportAdapter: LfsFilterDeviceIoControl() failed.\n") );
		goto error_out;
	}

    SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, 
				   ("GetScsiportAdapter: ScsiAddress=Len:%d PortNumber:%d PathId:%d TargetId:%d Lun:%d\n",
					(LONG)ScsiAddress.Length,
					(LONG)ScsiAddress.PortNumber,
					(LONG)ScsiAddress.PathId,
					(LONG)ScsiAddress.TargetId,
					(LONG)ScsiAddress.Lun) );

	RtlStringCchPrintfW( ScsiportAdapterNameTemp,
						 sizeof(ScsiportAdapterNameTemp) / sizeof(ScsiportAdapterNameTemp[0]),
						 L"\\Device\\ScsiPort%d",
						 ScsiAddress.PortNumber );

	RtlInitEmptyUnicodeString( &ScsiportAdapterName, ScsiportAdapterNameBuffer, sizeof( ScsiportAdapterNameBuffer ) );
    RtlAppendUnicodeToString( &ScsiportAdapterName, ScsiportAdapterNameTemp );

	InitializeObjectAttributes( &objectAttributes,
								&ScsiportAdapterName,
								OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
								NULL,
								NULL );

    //
	// open the file object for the given device
	//

    status = ZwCreateFile( &fileHandle,
						   SYNCHRONIZE|FILE_READ_DATA,
						   &objectAttributes,
						   &IoStatus,
						   NULL,
						   0,
						   FILE_SHARE_READ | FILE_SHARE_WRITE,
						   FILE_OPEN,
						   FILE_SYNCHRONOUS_IO_NONALERT,
						   NULL,
						   0 );

    if (!NT_SUCCESS(status)) {
	
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("GetScsiportAdapter: ZwCreateFile() failed.\n") );
		goto error_out;
	}

    status = ObReferenceObjectByHandle( fileHandle,
										FILE_READ_DATA,
										*IoFileObjectType,
										KernelMode,
										&ScsiportDeviceFileObject,
										NULL );
    
	if (!NT_SUCCESS(status)) {

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("GetScsiportAdapter: ObReferenceObjectByHandle() failed.\n") );
        goto error_out;
    }

	*ScsiportAdapterDeviceObject = IoGetRelatedDeviceObject( ScsiportDeviceFileObject );

	if (*ScsiportAdapterDeviceObject == NULL) {

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("GetScsiportAdapter: IoGetRelatedDeviceObject() failed.\n") );
		ObDereferenceObject(ScsiportDeviceFileObject);

		status = STATUS_UNSUCCESSFUL;
		goto error_out;
	}

	ObDereferenceObject( ScsiportDeviceFileObject );
	ZwClose( fileHandle );
	ObReferenceObject( *ScsiportAdapterDeviceObject );

	ASSERT( status == STATUS_SUCCESS );

	return status;

error_out:
	
	*ScsiportAdapterDeviceObject = NULL;
	
	if (fileHandle)
		ZwClose( fileHandle );

	return status;
}



//
// Return false if NDAS device is CD/DVD.
//
BOOLEAN
IsNetdiskPartition (
	IN	PDEVICE_OBJECT			DiskDeviceObject,
	OUT PNETDISK_INFORMATION	NetdiskInformation,
	OUT	PDEVICE_OBJECT			*ScsiportDeviceObject
	) 
{
	NTSTATUS					status;
	BOOLEAN						returnResult;
	NDSCIOCTL_PRIMUNITDISKINFO	primUnitDisk;
	BOOLEAN						decreaseReference = FALSE;

	if (DiskDeviceObject == NULL) {

		return FALSE;
	}

	if (*ScsiportDeviceObject == NULL) { 

		status = GetScsiportAdapter( DiskDeviceObject, ScsiportDeviceObject );

		if (status != STATUS_SUCCESS) {

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("IsNetdiskPartition: GetScsiportAdapter() failed.\n") );
			return FALSE;
		}

		decreaseReference = TRUE;
	} 

	// Try with the disk device

	returnResult = GetPrimaryUnitDisk( DiskDeviceObject, &primUnitDisk );

	if (returnResult == FALSE) {

		// Try with the scsi adapter

		returnResult = GetPrimaryUnitDisk( *ScsiportDeviceObject, &primUnitDisk );
	}

	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("IsNetDisk: GetPrimaryUnitDisk returnResult = %d\n", returnResult) );

	if (returnResult == TRUE && NetdiskInformation) {

		NetdiskInformation->NetdiskAddress = primUnitDisk.UnitDisk.NetDiskAddress.Address[0].NdasAddress.Lpx, 
		NetdiskInformation->UnitDiskNo = primUnitDisk.UnitDisk.UnitDiskId;

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, 
					   ("NdscId: %x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x:%x\n", 
						primUnitDisk.NDSC_ID[0], primUnitDisk.NDSC_ID[1], primUnitDisk.NDSC_ID[2], primUnitDisk.NDSC_ID[3],
						primUnitDisk.NDSC_ID[4], primUnitDisk.NDSC_ID[5], primUnitDisk.NDSC_ID[6], primUnitDisk.NDSC_ID[7],
						primUnitDisk.NDSC_ID[8], primUnitDisk.NDSC_ID[9], primUnitDisk.NDSC_ID[10], primUnitDisk.NDSC_ID[11],
						primUnitDisk.NDSC_ID[12], primUnitDisk.NDSC_ID[13], primUnitDisk.NDSC_ID[14], primUnitDisk.NDSC_ID[15]) );

		RtlCopyMemory( NetdiskInformation->NdscId, primUnitDisk.NDSC_ID, NDSC_ID_LENGTH );

		NetdiskInformation->DeviceMode = primUnitDisk.Lur.DeviceMode;
		NetdiskInformation->SupportedFeatures = primUnitDisk.Lur.SupportedFeatures;
		NetdiskInformation->EnabledFeatures = primUnitDisk.Lur.EnabledFeatures;

		//NDAS_ASSERT( FlagOn(primUnitDisk.Lur.EnabledFeatures, NDASFEATURE_SIMULTANEOUS_WRITE) );

		RtlCopyMemory( NetdiskInformation->UserId, primUnitDisk.UnitDisk.UserID, sizeof(NetdiskInformation->UserId) );
		
		RtlCopyMemory( NetdiskInformation->Password, primUnitDisk.UnitDisk.Password, sizeof(NetdiskInformation->Password) );

		NetdiskInformation->MessageSecurity = FALSE;
		NetdiskInformation->RwDataSecurity = FALSE;

		NetdiskInformation->SlotNo = primUnitDisk.UnitDisk.SlotNo;
		NetdiskInformation->EnabledTime = primUnitDisk.EnabledTime;
		
		NetdiskInformation->BindAddress = primUnitDisk.UnitDisk.BindingAddress.Address[0].NdasAddress.Lpx;

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, 
					    ("NetdiskInformation->BindAddress %02x:%02x/%02x:%02x:%02x:%02x\n",
						 NetdiskInformation->BindAddress.Node[0], NetdiskInformation->BindAddress.Node[1],
						 NetdiskInformation->BindAddress.Node[2], NetdiskInformation->BindAddress.Node[3],
						 NetdiskInformation->BindAddress.Node[4], NetdiskInformation->BindAddress.Node[5]) );

		//
		//	Determine if this LUR is a disk group
		//

		if (primUnitDisk.Lur.LurnCnt >= 2) {

			NetdiskInformation->DiskGroup = TRUE;
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("IsNetDisk: LUR is a disk group\n"));
		
		} else {
		
			NetdiskInformation->DiskGroup = FALSE;
		}

	}

	if (returnResult == FALSE && decreaseReference == TRUE) {

		ObDereferenceObject( *ScsiportDeviceObject );
		*ScsiportDeviceObject = NULL;
	} 

	return returnResult;
}


//
// Return false if NDAS device is CD/DVD.
//

NTSTATUS
GetNdasScsiBacl(
	IN	PDEVICE_OBJECT	DiskDeviceObject,
	OUT PNDAS_BLOCK_ACL	NdasBacl,
	IN BOOLEAN			SystemOrUser
	) 
{
	NTSTATUS		status;
	PDEVICE_OBJECT	scsiportDeviceObject;
	ULONG			srbIoctlReturnCode;

	if (DiskDeviceObject == NULL) {

		return STATUS_INVALID_PARAMETER;
	}

	NDAS_ASSERT( NdasBacl->Length );

	//
	// Try with the disk
	//

	status = NdasScsiQuery( DiskDeviceObject,
							SystemOrUser?NdscSystemBacl:NdscUserBacl,
							(PUCHAR)NdasBacl,
							NdasBacl->Length,
							&srbIoctlReturnCode );


	if (NT_SUCCESS(status)) {
		if (srbIoctlReturnCode == SRB_STATUS_DATA_OVERRUN) {
			status = STATUS_BUFFER_OVERFLOW;
		}
		return status;
	}

	//
	// Try with the scsi port
	//

	status = GetScsiportAdapter( DiskDeviceObject, &scsiportDeviceObject );

	if (status != STATUS_SUCCESS) {

		NDAS_ASSERT( FALSE );
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("GetNdasScsiBacl: GetScsiportAdapter() failed.\n") );
		return status;
	}

	status = NdasScsiQuery( scsiportDeviceObject,
							SystemOrUser?NdscSystemBacl:NdscUserBacl,
							(PUCHAR)NdasBacl,
							NdasBacl->Length,
							&srbIoctlReturnCode );

	ObDereferenceObject( scsiportDeviceObject );

	if (NT_SUCCESS(status)) {
		if (srbIoctlReturnCode == SRB_STATUS_DATA_OVERRUN) {
			status = STATUS_BUFFER_OVERFLOW;
		}
	} else {

		NDAS_ASSERT( FALSE );
	}

	return status;
}

NTSTATUS
NdasScsiCtrlUpgradeToWrite(
	IN PDEVICE_OBJECT	DiskDeviceObject,
	OUT PIO_STATUS_BLOCK	IoStatus
){
	NTSTATUS		status;
	PDEVICE_OBJECT	scsiportDeviceObject;

	if (DiskDeviceObject == NULL) {

		return STATUS_INVALID_PARAMETER;
	}

	//
	// Try with the disk
	//

	status = UpgradeToWrite( DiskDeviceObject,
							IoStatus );
	if(NT_SUCCESS(status))
		return status;

	//
	// Try with the scsi port
	//

	status = GetScsiportAdapter( DiskDeviceObject, &scsiportDeviceObject );

	if (status != STATUS_SUCCESS) {

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("NdasScsiCtrlUpgradeToWrite: GetScsiportAdapter() failed.\n") );
		return status;
	}

	status = UpgradeToWrite( scsiportDeviceObject,
							IoStatus );

	ObDereferenceObject( scsiportDeviceObject );

	return status;
}


//////////////////////////////////////////////////////////////////////////
//
//	NDAS bus control
// 

NTSTATUS
NdasBusCtrlOpen(
	OUT	PDEVICE_OBJECT		*NdasBusDeviceObject,
	OUT PFILE_OBJECT		*NdasBusDeviceFileObject
)
{
	NTSTATUS			ntStatus;
	PWSTR				symbolicLinkList;
    UNICODE_STRING		objectName;
	PFILE_OBJECT		fileObject;
	PDEVICE_OBJECT		deviceObject;

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NdasBusCtrlOpen: entered.\n"));

	ASSERT(NdasBusDeviceObject);
	if (KeGetCurrentIrql() > PASSIVE_LEVEL)
		NDAS_ASSERT( FALSE );

	ntStatus = IoGetDeviceInterfaces(
			&GUID_NDAS_BUS_ENUMERATOR_INTERFACE_CLASS,
			NULL,
			0,
			&symbolicLinkList
		);
	
	if(!NT_SUCCESS(ntStatus)) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NdasBusCtrlOpen: IoGetDeviceInterfaces ntStatus = 0x%x\n", ntStatus));
		return ntStatus;
	}

	ASSERT(symbolicLinkList != NULL);

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NdasBusCtrlOpen: symbolicLinkList = %ws\n", symbolicLinkList));

    RtlInitUnicodeString(&objectName, symbolicLinkList);
	ntStatus = IoGetDeviceObjectPointer(
						&objectName,
						FILE_ALL_ACCESS,
						&fileObject,
						&deviceObject
					);

	if(!NT_SUCCESS(ntStatus)) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NdasBusCtrlOpen: ntStatus = 0x%x\n", ntStatus));
		ExFreePool(symbolicLinkList);
		return ntStatus;
	}

	ExFreePool(symbolicLinkList);

	*NdasBusDeviceObject = deviceObject;
	*NdasBusDeviceFileObject = fileObject;

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "NdasBusCtrlOpen: Done.\n"));

	return ntStatus;
}


VOID
NdasBusCtrlClose(
		PFILE_OBJECT FileObject
	) {
	ASSERT(FileObject);

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "NdasBusCtrlOpen: Closed LanScsiBus interface.\n"));

	ObDereferenceObject(FileObject);

}

#if 0

static
NTSTATUS
NdasBusCtrlRemoveTarget(
	ULONG				SlotNo
) 
{
	NTSTATUS				ntStatus;
	PFILE_OBJECT			fileObject;
	PDEVICE_OBJECT			deviceObject;
	PIRP					irp;
    KEVENT					event;
	IO_STATUS_BLOCK			ioStatusBlock;

    NDASBUS_REMOVE_TARGET_DATA removeTarget;

	ASSERT(KeGetCurrentIrql() <= PASSIVE_LEVEL);
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NdasBusCtrlRemoveTarget: SlotNo:%ld\n", SlotNo));

	ntStatus = NdasBusCtrlOpen(&deviceObject, &fileObject);
	if(!NT_SUCCESS(ntStatus)) {
		return ntStatus;
	}

	RtlZeroMemory(&removeTarget, sizeof(removeTarget));
	removeTarget.ulSlotNo = SlotNo;

    KeInitializeEvent(&event, NotificationEvent, FALSE);


	irp = IoBuildDeviceIoControlRequest(
			IOCTL_NDASBUS_REMOVE_TARGET,
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
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NdasBusCtrlRemoveTarget: irp NULL\n"));

		ntStatus = STATUS_INSUFFICIENT_RESOURCES;

		goto cleanup;
	}

	ntStatus = IoCallDriver(deviceObject, irp);
	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NdasBusCtrlRemoveTarget: NTSTATUS:%lx IOSTATUS:%lx\n", ntStatus, ioStatusBlock.Status));
    if (ntStatus == STATUS_PENDING)
    {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NdasBusCtrlRemoveTarget: IoCallDriver STATUS_PENDING\n"));
		KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
    }
	if (!NT_SUCCESS (ntStatus) || !NT_SUCCESS(ioStatusBlock.Status)) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, 
			( "[LFS] NdasBusCtrlRemoveTarget: IoCallDriver() failed. STATUS=%08lx\n", ntStatus));
//		ASSERT(FALSE);
	}

cleanup:

	NdasBusCtrlClose(fileObject);
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NdasBusCtrlRemoveTarget: Done...\n"));

	
	return ntStatus;
}

#endif

NTSTATUS
NdasBusCtrlUnplug(
	ULONG				SlotNo
){
	NTSTATUS				ntStatus;
	PFILE_OBJECT			fileObject;
	PDEVICE_OBJECT			deviceObject;
	PIRP					irp;
    KEVENT					event;
	IO_STATUS_BLOCK			ioStatusBlock;

    NDASBUS_UNPLUG_HARDWARE unplug;

	ASSERT(KeGetCurrentIrql() <= PASSIVE_LEVEL );
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NdasBusCtrlUnplug: SlotNo:%ld\n", SlotNo));

#if 0
	//
	//	Before unplugging, make sure to remove target devices in the NDAS adapter
	//
	NdasBusCtrlRemoveTarget(SlotNo);
#endif

	//
	//	Continue to unplug
	//
	ntStatus = NdasBusCtrlOpen(&deviceObject, &fileObject);
	if(!NT_SUCCESS(ntStatus)) {
		return ntStatus;
	}

	unplug.SerialNo = SlotNo;
    unplug.Size = sizeof (unplug);
	
    KeInitializeEvent(&event, NotificationEvent, FALSE);


	irp = IoBuildDeviceIoControlRequest(
			IOCTL_NDASBUS_UNPLUG_HARDWARE,
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
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NdasBusCtrlUnplug: irp NULL\n"));

		ntStatus = STATUS_INSUFFICIENT_RESOURCES;

		goto cleanup;
	}

	ntStatus = IoCallDriver(deviceObject, irp);
	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NdasBusCtrlUnplug: NTSTATUS:%lx IOSTATUS:%lx\n", ntStatus, ioStatusBlock.Status));
    if (ntStatus == STATUS_PENDING)
    {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NdasBusCtrlUnplug: IoCallDriver STATUS_PENDING\n"));
		KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
    }
	if (!NT_SUCCESS (ntStatus) || !NT_SUCCESS(ioStatusBlock.Status)) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, 
			( "[LFS] NdasBusCtrlUnplug: IoCallDriver() failed. STATUS=%08lx\n", ntStatus));
//		ASSERT(FALSE);
	}

cleanup:

	NdasBusCtrlClose(fileObject);
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NdasBusCtrlUnplug: Done...\n"));

	
	return ntStatus;
}

#ifdef __LFSFILT_SUPPORT_NDASPORT__

//////////////////////////////////////////////////////////////////////////
//
//	NDAS port control
//
NTSTATUS
NdasPortCtrlOpen(
	OUT	PDEVICE_OBJECT		*NdasPortDeviceObject,
	OUT PFILE_OBJECT		*NdasPortDeviceFileObject
)
{
	NTSTATUS			ntStatus;
	PWSTR				symbolicLinkList;
    UNICODE_STRING		objectName;
	PFILE_OBJECT		fileObject;
	PDEVICE_OBJECT		deviceObject;

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NdasPortCtrlOpen: entered.\n"));

	ASSERT(NdasPortDeviceObject);
	if (KeGetCurrentIrql() > PASSIVE_LEVEL)
		NDAS_ASSERT( FALSE );

	ntStatus = IoGetDeviceInterfaces(
			&GUID_DEVINTERFACE_NDASPORT,
			NULL,
			0,
			&symbolicLinkList
		);

	if(!NT_SUCCESS(ntStatus)) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NdasPortCtrlOpen: IoGetDeviceInterfaces ntStatus = 0x%x\n", ntStatus));
		return ntStatus;
	}

	ASSERT(symbolicLinkList != NULL);

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NdasPortCtrlOpen: symbolicLinkList = %ws\n", symbolicLinkList));

    RtlInitUnicodeString(&objectName, symbolicLinkList);
	ntStatus = IoGetDeviceObjectPointer(
						&objectName,
						FILE_ALL_ACCESS,
						&fileObject,
						&deviceObject
					);

	if(!NT_SUCCESS(ntStatus)) {
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] NdasPortCtrlOpen: ntStatus = 0x%x\n", ntStatus));
		ExFreePool(symbolicLinkList);
		return ntStatus;
	}

	ExFreePool(symbolicLinkList);

	*NdasPortDeviceObject = deviceObject;
	*NdasPortDeviceFileObject = fileObject;

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "NdasPortCtrlOpen: Done.\n"));

	return ntStatus;
}


VOID
NdasPortCtrlClose(
		PFILE_OBJECT FileObject
	) {
	ASSERT(FileObject);

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "NdasPortCtrlOpen: Closed LanScsiBus interface.\n"));

	ObDereferenceObject(FileObject);

}

NTSTATUS
NdasPortCtrlUnplug(
	ULONG		NdasLogicalUnitAddress,
	ULONG		UnplugFlags
){
	NTSTATUS	status;
	PDEVICE_OBJECT	ndasPortDeviceObject;
	PFILE_OBJECT	ndasPortDeviceFileObject;
	NDASPORT_LOGICALUNIT_UNPLUG unplugParam = {0};

	status = NdasPortCtrlOpen(&ndasPortDeviceObject, &ndasPortDeviceFileObject);
	if(!NT_SUCCESS(status))
		return status;

	unplugParam.Size = sizeof(NDASPORT_LOGICALUNIT_EJECT);
	unplugParam.LogicalUnitAddress.Address = NdasLogicalUnitAddress;
	unplugParam.Flags = UnplugFlags;

	status = LfsFilterDeviceIoControl(
		ndasPortDeviceObject,
		IOCTL_NDASPORT_UNPLUG_LOGICALUNIT,
		&unplugParam,
		sizeof(NDASPORT_LOGICALUNIT_EJECT),
		NULL,
		0,
		NULL
		);

	(VOID)NdasPortCtrlClose(ndasPortDeviceFileObject);

	return status;
}

#endif

//////////////////////////////////////////////////////////////////////////
//
// NDAS control
//

NTSTATUS
NDCtrlUnplug(
	ULONG	SlotNo
){
	NTSTATUS	status;

#ifdef __LFSFILT_SUPPORT_NDASPORT__

	//
	// Try with NDAS port
	//

	status = NdasPortCtrlUnplug(SlotNo, 0);
	if(!NT_SUCCESS(status)) {
#endif
		//
		// Try with NDAS bus
		//
		status = NdasBusCtrlUnplug(SlotNo);
#ifdef __LFSFILT_SUPPORT_NDASPORT__
	}
#endif

	return status;
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


	UnicodeBuffer = ExAllocatePoolWithTag(NonPagedPool, DEBUG_BUFFER_LENGTH, LFS_ALLOC_TAG);
	if(UnicodeBuffer == NULL)
		return;
#endif

	SPY_LOG_PRINT( LFS_DEBUG_LFS_NOISE,( "[LFS] PrintFileInfoClass:\n"));
	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileInformationClass: %s(%d)\n",
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

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "  * entry #%lu *\n", count));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "NextEntryOffset	: %lu\n",info->NextEntryOffset ));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileIndex		: %lu\n", info->FileIndex));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "CreationTime	: %I64u\n", info->CreationTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "LastAccessTime	: %I64u\n", info->LastAccessTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "LastWriteTime	: %I64u\n", info->LastWriteTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "ChangeTime		: %I64u\n", info->ChangeTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "EndOfFile		: %I64u\n", info->EndOfFile.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "AllocationSize	: %I64u\n", info->AllocationSize.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileAttributes	: %lx\n", info->FileAttributes));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileNameLength	: %lu\n", info->FileNameLength));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileName			: '%ws'\n", 
								NullTerminateUnicode(	info->FileNameLength,
									(PWCHAR)info->FileName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				));
	
			count ++;
			offset += info->NextEntryOffset;

			if(!info->NextEntryOffset) break;
		}
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "%lu chunks printed\n", count));

		break;
	}
	case FileFullDirectoryInformation: {
		PFILE_FULL_DIR_INFORMATION	info;

		offset = 0;
		count = 0;
		while(offset < systemBuffLength) {
			info = (PFILE_FULL_DIR_INFORMATION)(systemBuff + offset);

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "  * entry #%lu *\n", count));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "NextEntryOffset	: %lu\n",info->NextEntryOffset ) );
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileIndex		: %lu\n", info->FileIndex));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "CreationTime	: %I64u\n", info->CreationTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "LastAccessTime	: %I64u\n", info->LastAccessTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "LastWriteTime	: %I64u\n", info->LastWriteTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "ChangeTime		: %I64u\n", info->ChangeTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "EndOfFile		: %I64u\n", info->EndOfFile.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "AllocationSize	: %I64u\n", info->AllocationSize.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileAttributes	: %lx\n", info->FileAttributes));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileNameLength	: %lu\n", info->FileNameLength));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "EaSize			: %lu\n", info->EaSize));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileName			: '%ws'\n", 
								NullTerminateUnicode(	info->FileNameLength,
									(PWCHAR)info->FileName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				));
	
			count ++;
			offset += info->NextEntryOffset;

			if(!info->NextEntryOffset) break;
		}
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "%lu chunks printed\n", count));

		break;
	}
	case FileBothDirectoryInformation: {
		PFILE_BOTH_DIR_INFORMATION	info;

		offset = 0;
		count = 0;
		while(offset < systemBuffLength) {
			info = (PFILE_BOTH_DIR_INFORMATION)(systemBuff + offset);

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "  * entry #%lu *\n", count));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "NextEntryOffset	: %lu\n",info->NextEntryOffset ));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileIndex		: %lu\n", info->FileIndex));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "CreationTime	: %I64u\n", info->CreationTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "LastAccessTime	: %I64u\n", info->LastAccessTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "LastWriteTime	: %I64u\n", info->LastWriteTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "ChangeTime		: %I64u\n", info->ChangeTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "EndOfFile		: %I64u\n", info->EndOfFile.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "AllocationSize	: %I64u\n", info->AllocationSize.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileAttributes	: %lx\n", info->FileAttributes));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileNameLength	: %lu\n", info->FileNameLength));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "EaSize			: %lu\n", info->EaSize));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "ShortNameLength	: %u\n", (int)info->ShortNameLength));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "ShortName		: '%ws'\n", 
								NullTerminateUnicode(	info->ShortNameLength,
									(PWCHAR)info->ShortName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileName			: '%ws'\n", 
								NullTerminateUnicode(	info->FileNameLength,
									(PWCHAR)info->FileName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				));

			count ++;
			offset += info->NextEntryOffset;

			if(!info->NextEntryOffset) break;
		}
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "%lu chunks printed\n", count));

		break;
	}
	case FileIdBothDirectoryInformation: {
		PFILE_ID_BOTH_DIR_INFORMATION	info;

		offset = 0;
		count = 0;
		while(offset < systemBuffLength) {
			info = (PFILE_ID_BOTH_DIR_INFORMATION)(systemBuff + offset);

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "  * entry #%lu *\n", count));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "NextEntryOffset	: %lu\n",info->NextEntryOffset ));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileIndex		: %lu\n", info->FileIndex));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "CreationTime	: %I64u\n", info->CreationTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "LastAccessTime	: %I64u\n", info->LastAccessTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "LastWriteTime	: %I64u\n", info->LastWriteTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "ChangeTime		: %I64u\n", info->ChangeTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "EndOfFile		: %I64u\n", info->EndOfFile.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "AllocationSize	: %I64u\n", info->AllocationSize.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileAttributes	: %lx\n", info->FileAttributes));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileNameLength	: %lu\n", info->FileNameLength));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "EaSize			: %lu\n", info->EaSize));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "ShortNameLength	: %u\n", (int)info->ShortNameLength));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "ShortName		: '%ws'\n", 
								NullTerminateUnicode(	info->ShortNameLength,
									(PWCHAR)info->ShortName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileId			: %I64x\n", info->FileId.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileName			: '%ws'\n", 
								NullTerminateUnicode(	info->FileNameLength,
									(PWCHAR)info->FileName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				));

			count ++;
			offset += info->NextEntryOffset;

			if(!info->NextEntryOffset) break;
		}
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "%lu chunks printed\n", count));

		break;
	}
	case FileIdFullDirectoryInformation: {
		PFILE_ID_FULL_DIR_INFORMATION	info;

		offset = 0;
		count = 0;
		while(offset < systemBuffLength) {
			info = (PFILE_ID_FULL_DIR_INFORMATION)(systemBuff + offset);

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "  * entry #%lu *\n", count));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "NextEntryOffset	: %lu\n",info->NextEntryOffset ));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileIndex		: %lu\n", info->FileIndex));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "CreationTime	: %I64u\n", info->CreationTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "LastAccessTime	: %I64u\n", info->LastAccessTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "LastWriteTime	: %I64u\n", info->LastWriteTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "ChangeTime		: %I64u\n", info->ChangeTime.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "EndOfFile		: %I64u\n", info->EndOfFile.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "AllocationSize	: %I64u\n", info->AllocationSize.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileAttributes	: %lx\n", info->FileAttributes));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileNameLength	: %lu\n", info->FileNameLength));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "EaSize			: %lu\n", info->EaSize));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileId			: %I64x\n", info->FileId.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileName			: '%ws'\n", 
								NullTerminateUnicode(	info->FileNameLength,
									(PWCHAR)info->FileName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				));

			count ++;
			offset += info->NextEntryOffset;

			if(!info->NextEntryOffset) break;
		}
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "%lu chunks printed\n", count));

		break;
	}
	case FileNamesInformation: {
		PFILE_NAMES_INFORMATION	info;

		offset = 0;
		count = 0;
		while(offset < systemBuffLength) {
			info = (PFILE_NAMES_INFORMATION)(systemBuff + offset);

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "  * entry #%lu *\n", count));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "NextEntryOffset	: %lu\n",info->NextEntryOffset ));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileIndex		: %lu\n", info->FileIndex));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileNameLength	: %lu\n", info->FileNameLength));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileName			: '%ws'\n", 
								NullTerminateUnicode(	info->FileNameLength,
									(PWCHAR)info->FileName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				));

			count ++;
			offset += info->NextEntryOffset;

			if(!info->NextEntryOffset) break;
		}
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "%lu chunks printed\n", count));

		break;
	}
	case FileObjectIdInformation: {
		PFILE_OBJECTID_INFORMATION	info = (PFILE_OBJECTID_INFORMATION)systemBuff;

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileReference	: %I64u\n", info->FileReference));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "ObjectId			: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
									info->ObjectId[0], info->ObjectId[1], info->ObjectId[2], info->ObjectId[3],
									info->ObjectId[4], info->ObjectId[5], info->ObjectId[6], info->ObjectId[7],
									info->ObjectId[8], info->ObjectId[9], info->ObjectId[10], info->ObjectId[11],
									info->ObjectId[12], info->ObjectId[13], info->ObjectId[14], info->ObjectId[!5]
							));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "BirthVolumeId	: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
									info->BirthVolumeId[0], info->BirthVolumeId[1], info->BirthVolumeId[2], info->BirthVolumeId[3],
									info->BirthVolumeId[4], info->BirthVolumeId[5], info->BirthVolumeId[6], info->BirthVolumeId[7],
									info->BirthVolumeId[8], info->BirthVolumeId[9], info->BirthVolumeId[10], info->BirthVolumeId[11],
									info->BirthVolumeId[12], info->BirthVolumeId[13], info->BirthVolumeId[14], info->BirthVolumeId[!5]
							) );
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "BirthObjectId	: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
									info->BirthObjectId[0], info->BirthObjectId[1], info->BirthObjectId[2], info->BirthObjectId[3],
									info->BirthObjectId[4], info->BirthObjectId[5], info->BirthObjectId[6], info->BirthObjectId[7],
									info->BirthObjectId[8], info->BirthObjectId[9], info->BirthObjectId[10], info->BirthObjectId[11],
									info->BirthObjectId[12], info->BirthObjectId[13], info->BirthObjectId[14], info->BirthObjectId[!5]
							) );
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "DomainId			: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
									info->DomainId[0], info->DomainId[1], info->DomainId[2], info->DomainId[3],
									info->DomainId[4], info->DomainId[5], info->DomainId[6], info->DomainId[7],
									info->DomainId[8], info->DomainId[9], info->DomainId[10], info->DomainId[11],
									info->DomainId[12], info->DomainId[13], info->DomainId[14], info->DomainId[!5]
							) );

		break;
	}
	case FileAllocationInformation: {
		PFILE_ALLOCATION_INFORMATION	info = (PFILE_ALLOCATION_INFORMATION)systemBuff;

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "AllocationSize	: %I64u\n", info->AllocationSize.QuadPart));
		break;
	}

	case FileAttributeTagInformation: {
		PFILE_ATTRIBUTE_TAG_INFORMATION	info = (PFILE_ATTRIBUTE_TAG_INFORMATION)systemBuff;

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileAttributes	: %lx\n", info->FileAttributes ));
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "ReparseTag		: %lx\n", info->ReparseTag ));
		break;
	}
	case FileBasicInformation: {
		PFILE_BASIC_INFORMATION	info = (PFILE_BASIC_INFORMATION)systemBuff;

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "CreationTime	: %I64u\n", info->CreationTime.QuadPart));
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "LastAccessTime	: %I64u\n", info->LastAccessTime.QuadPart));
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "LastWriteTime	: %I64u\n", info->LastWriteTime.QuadPart));
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "ChangeTime		: %I64u\n", info->ChangeTime.QuadPart));
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileAttributes	: %lx\n", info->FileAttributes));
		break;
	}
	case FileStandardInformation: {
		PFILE_STANDARD_INFORMATION	info = (PFILE_STANDARD_INFORMATION)systemBuff;

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "AllocationSize	: %I64u\n", info->AllocationSize.QuadPart));
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "EndOfFile		: %I64u\n", info->EndOfFile.QuadPart));
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "NumberOfLinks	: %lu\n", info->NumberOfLinks));
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "DeletePending	: %u\n", info->DeletePending));
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "Directory		: %u\n", info->Directory));
		break;
	 }
	case FileStreamInformation: {
		PFILE_STREAM_INFORMATION	info ;

		offset = 0;
		count = 0;
		while(offset < systemBuffLength) {
			info = (PFILE_STREAM_INFORMATION)(systemBuff + offset);

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "  * entry #%lu *\n", count));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "NextEntryOffset	: %lu\n",info->NextEntryOffset ));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "CreationTime	: %I64u\n", info->StreamSize.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "LastAccessTime	: %I64u\n", info->StreamAllocationSize.QuadPart));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "StreamNameLength	: %lu\n", info->StreamNameLength));
			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "StreamName		: '%ws'\n", 
								NullTerminateUnicode(	info->StreamNameLength,
									(PWCHAR)info->StreamName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				));
	
			count ++;
			offset += info->NextEntryOffset;

			if(!info->NextEntryOffset) break;
		}
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "%lu chunks printed\n", count));
		
		break;
	}
	case FileEaInformation: {
		PFILE_EA_INFORMATION	info = (PFILE_EA_INFORMATION)systemBuff;
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "EaSize	: %lu\n",info->EaSize ));
		break;
	}

	case FileNameInformation: {
		PFILE_NAME_INFORMATION	info = (PFILE_NAME_INFORMATION)systemBuff;
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileNameLength	: %lu\n", info->FileNameLength ));
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "FileName       : %ws\n", 
								NullTerminateUnicode(info->FileNameLength,
									(PWCHAR)info->FileName,
									DEBUG_BUFFER_LENGTH, UnicodeBuffer )
				));
		break;
	}

	case FileEndOfFileInformation: {
		PFILE_END_OF_FILE_INFORMATION	info = (PFILE_END_OF_FILE_INFORMATION)systemBuff;

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("EndOfFile : %I64d", info->EndOfFile.QuadPart));
		break;
	}

	case FileAllInformation:
	case FileReparsePointInformation:
	case FileQuotaInformation:
	default:
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ( "[LFS] PrintFileInfoClass:Not supported query type.\n"));
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
		SPY_LOG_PRINT(LFS_DEBUG_LFS_TRACE, ("RtlStringCchVPrintfW() failed.\n"));
		return;
	}

	status = RtlStringCchLengthW(strBuff, 16, &stringLen);
	if(!NT_SUCCESS(status)) {
		SPY_LOG_PRINT(LFS_DEBUG_LFS_TRACE, ("RtlStringCchLengthW() failed.\n"));
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
		SPY_LOG_PRINT(LFS_DEBUG_LFS_TRACE, ("Could not allocate error log entry.\n"));
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
			SPY_LOG_PRINT(LFS_DEBUG_LFS_TRACE, ("Allocating context failed.\n"));
			return;
		}

		RtlCopyMemory(&context->ErrorLogEntry, ErrorLogEntry, sizeof(LFS_ERROR_LOG_ENTRY));


		workitem = IoAllocateWorkItem(DeviceObject);
		if(workitem == NULL) {
			SPY_LOG_PRINT(LFS_DEBUG_LFS_TRACE, ("IoAllocateWorkItem() failed.\n"));
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
    IN PDEVICE_OBJECT	DeviceObject,
    IN PIRP				Irp,
    IN PKEVENT			SynchronizingEvent
    )
{

    UNREFERENCED_PARAMETER( DeviceObject );
    
    NDAS_ASSERT( NULL != Irp->UserIosb );

    *Irp->UserIosb = Irp->IoStatus;

    KeSetEvent( SynchronizingEvent, IO_NO_INCREMENT, FALSE );

    return STATUS_MORE_PROCESSING_REQUIRED;
}

//
// Used to continue query from the last queried position 
//		in case that primary host has changed since last directory query. 
//
NTSTATUS
LfsQueryDirectoryByIndex (
    HANDLE 				FileHandle,
    ULONG				FileInformationClass,
    PVOID  				FileInformation,
    ULONG				Length,
    ULONG				FileIndex,
    PUNICODE_STRING		FileName,
    PIO_STATUS_BLOCK	IoStatusBlock,
    BOOLEAN				ReturnSingleEntry
    )
{
    NTSTATUS			status;
    KEVENT				event;
    PFILE_OBJECT		fileObject;
    PDEVICE_OBJECT		deviceObject;
    PMDL				mdl = NULL;
    PIRP				irp;
    PIO_STACK_LOCATION	irpSp;
    PCHAR				systemBuffer = NULL;

    SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("[LFS] LfsQueryDirectoryByIndex: FileIndex=%x\n", FileIndex) );
	
    status = ObReferenceObjectByHandle( FileHandle,
                                        FILE_LIST_DIRECTORY,
                                        NULL,
                                        KernelMode,
                                        (PVOID *) &fileObject,
                                        (POBJECT_HANDLE_INFORMATION) NULL );

    if (!NT_SUCCESS( status )) {

		return status;
    }

    KeInitializeEvent( &event, NotificationEvent, FALSE );

    KeClearEvent( &fileObject->Event );

    deviceObject = IoGetRelatedDeviceObject( fileObject );

    irp = IoAllocateIrp( deviceObject->StackSize, TRUE );

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

	    SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("[LFS] LfsQueryDirectoryByIndex: FileName=%wZ\n", FileName) );

#ifndef NTDDI_VERSION

#if WINVER >= 0x0502 // Windows 2003 or later
		irpSp->Parameters.QueryDirectory.FileName = FileName;
#else
		irpSp->Parameters.QueryDirectory.FileName = (PSTRING)FileName;
#endif

#else

		irpSp->Parameters.QueryDirectory.FileName = FileName;

#endif

	} else {

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("[LFS] LfsQueryDirectoryByIndex: No file name\n") );
		irpSp->Parameters.QueryDirectory.FileName = NULL;
	}

    if (deviceObject->Flags & DO_BUFFERED_IO) {

        try {
        
			systemBuffer = ExAllocatePoolWithQuotaTag( NonPagedPool, Length, 'QSFL' );
            irp->AssociatedIrp.SystemBuffer = systemBuffer;

		} except(EXCEPTION_EXECUTE_HANDLER) {

			IoFreeIrp(irp);
            ObDereferenceObject(fileObject);
            return GetExceptionCode();
        }

	} else if (deviceObject->Flags & DO_DIRECT_IO) {

		try {

			mdl = IoAllocateMdl( FileInformation, Length, FALSE, TRUE, irp );

			if (mdl == NULL) {

				ExRaiseStatus( STATUS_INSUFFICIENT_RESOURCES );
            }

            MmProbeAndLockPages( mdl, UserMode, IoWriteAccess );

		} except(EXCEPTION_EXECUTE_HANDLER) {

			if (mdl != NULL) {

				IoFreeMdl(mdl);
            }

			IoFreeIrp(irp);
            ObDereferenceObject( fileObject );

			return GetExceptionCode();
        }

		irp->MdlAddress = mdl;

	} else {

		irp->UserBuffer = FileInformation;
    }

    IoSetCompletionRoutine( irp, 
                            LfsQueryDirectoryByIndexCompletion, 
                            &event, 
                            TRUE, 
                            TRUE, 
                            TRUE );

    status = IoCallDriver( deviceObject, irp );

    if (status == STATUS_PENDING) {

        status = KeWaitForSingleObject( &event, Executive, KernelMode, FALSE, NULL );
    }

    if (NT_SUCCESS(status)) {

        status = IoStatusBlock->Status;
        
		if (NT_SUCCESS(status) || status == STATUS_BUFFER_OVERFLOW) {
        
			if (systemBuffer) {
                
				try {
                
					RtlCopyMemory( FileInformation, systemBuffer, Length );

                } except (EXCEPTION_EXECUTE_HANDLER) {

                    status = GetExceptionCode();
                }
            }
        }
    }

	if (mdl) {

		MmUnlockPages(mdl);
		IoFreeMdl(mdl);
	}

    if (systemBuffer) {

        ExFreePool(systemBuffer);
    }

	irp->AssociatedIrp.SystemBuffer = NULL;
	irp->MdlAddress = NULL;
	IoFreeIrp(irp);

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


