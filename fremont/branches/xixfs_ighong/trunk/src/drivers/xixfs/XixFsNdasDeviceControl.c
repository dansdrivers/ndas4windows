#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDiskForm.h"
#include "XixFsDrv.h"
#include "XixFsRawDiskAccessApi.h"
#include "XixFsProto.h"



#include "ndasscsiioctl.h"
#include "lurdesc.h"

#define NTSTRSAFE_LIB
#include <ntddscsi.h>

#include <Ntstrsafe.h>





#ifndef RtlInitEmptyUnicodeString

#define RtlInitEmptyUnicodeString(_ucStr,_buf,_bufSize) \
		((_ucStr)->Buffer = (_buf), \
		(_ucStr)->Length = 0, \
		(_ucStr)->MaximumLength = (USHORT)(_bufSize))

#endif


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsGetScsiportAdapter)
#pragma alloc_text(PAGE, XixFsCheckXifsd)
#pragma alloc_text(PAGE, XixFsNdasLock)
#pragma alloc_text(PAGE, XixFsNdasUnLock)
#pragma alloc_text(PAGE, XixFsNdasQueryLock)
#endif



/*
 *	Function must be done within waitable thread context
 */


BOOLEAN	
XixFsGetScsiportAdapter(
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



	ntStatus = XixFsRawDevIoCtrl ( 
					DiskDeviceObject,
					IOCTL_SCSI_GET_ADDRESS,
					NULL,
					0,
					(uint8 *)&ScsiAddress,
					sizeof(SCSI_ADDRESS),
					FALSE,
					NULL
					);

	if(!NT_SUCCESS(ntStatus)) {
		DebugTrace( DEBUG_LEVEL_ERROR,  (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("XixFsGetScsiportAdapter: LfsFilterDeviceIoControl() failed.\n"));
		goto error_out;

	}

    DebugTrace( DEBUG_LEVEL_ALL,  (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("XixFsGetScsiportAdapter: ScsiAddress=Len:%d PortNumber:%d PathId:%d TargetId:%d Lun:%d\n",
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
	    DebugTrace( DEBUG_LEVEL_ERROR,  (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB),
			("XixFsGetScsiportAdapter: ZwCreateFile() failed.\n"));
		goto error_out;

	}

    ntStatus = ObReferenceObjectByHandle( fileHandle,
										FILE_READ_DATA,
										*IoFileObjectType,
										KernelMode,
										&ScsiportDeviceFileObject,
										NULL);
    if(!NT_SUCCESS( ntStatus )) {

		DebugTrace( DEBUG_LEVEL_ERROR,  (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("XixFsGetScsiportAdapter: ObReferenceObjectByHandle() failed.\n"));
        goto error_out;
    }

	*ScsiportAdapterDeviceObject = IoGetRelatedDeviceObject(
											ScsiportDeviceFileObject
									    );

	if(*ScsiportAdapterDeviceObject == NULL) {

		DebugTrace( DEBUG_LEVEL_ERROR,  (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("XixFsGetScsiportAdapter: IoGetRelatedDeviceObject() failed.\n"));
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

	//	Added by ILGU HONG for 08312006
NTSTATUS
XixFsGetNdadDiskInfo(
	IN PDEVICE_OBJECT	DeviceObject,
	OUT	PLSMPIOCTL_PRIMUNITDISKINFO	PrimUnitDisk
	)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	PDEVICE_OBJECT		scsiAdpaterDeviceObject = NULL;
	BOOLEAN				result = FALSE;
	PSRB_IO_CONTROL		pSrbIoCtl = NULL;
	uint32				OutbuffSize = 0;
	PLSMPIOCTL_QUERYINFO		QueryInfo;
	PLSMPIOCTL_PRIMUNITDISKINFO	pPrimUnitDisk;

	PAGED_CODE();

	ASSERT(DeviceObject);

	
	DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XixFsGetNdadDeviceInfo \n"));

	result = XixFsGetScsiportAdapter(DeviceObject, &scsiAdpaterDeviceObject);
	if(result != TRUE){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail GetScsiportAdpater!!! \n"));
		return STATUS_UNSUCCESSFUL;
	}

	OutbuffSize = sizeof(SRB_IO_CONTROL) + sizeof(LSMPIOCTL_QUERYINFO) + sizeof(LSMPIOCTL_PRIMUNITDISKINFO);
	OutbuffSize = SECTORALIGNSIZE_512(OutbuffSize);
	pSrbIoCtl = (PSRB_IO_CONTROL)ExAllocatePoolWithTag(NonPagedPool, OutbuffSize, TAG_BUFFER);
	
	if(!pSrbIoCtl){
		ObDereferenceObject(scsiAdpaterDeviceObject);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	try{
		RtlZeroMemory(pSrbIoCtl, OutbuffSize);
		
		pSrbIoCtl->HeaderLength = sizeof(SRB_IO_CONTROL);
		RtlCopyMemory(pSrbIoCtl->Signature, LANSCSIMINIPORT_IOCTL_SIGNATURE, 8);
		pSrbIoCtl->Timeout = 10;
		pSrbIoCtl->ControlCode = LANSCSIMINIPORT_IOCTL_QUERYINFO_EX;
		pSrbIoCtl->Length =  sizeof(LSMPIOCTL_QUERYINFO) + sizeof(LSMPIOCTL_PRIMUNITDISKINFO);
		
		QueryInfo = (PLSMPIOCTL_QUERYINFO)((PUCHAR)pSrbIoCtl + sizeof(SRB_IO_CONTROL));
		pPrimUnitDisk = (PLSMPIOCTL_PRIMUNITDISKINFO)((PUCHAR)pSrbIoCtl + sizeof(SRB_IO_CONTROL));

		QueryInfo->Length = sizeof(LSMPIOCTL_QUERYINFO);
		QueryInfo->InfoClass = LsmpPrimaryUnitDiskInformation;
		QueryInfo->QueryDataLength = 0;


		RC = XixFsRawDevIoCtrl ( 
						scsiAdpaterDeviceObject,
						IOCTL_SCSI_MINIPORT,
						(uint8 *)pSrbIoCtl,
						OutbuffSize,
						(uint8 *)pSrbIoCtl,
						OutbuffSize,
						FALSE,
						NULL
						);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Fail XixFsGetNdadDeviceInfo Status (0x%x)\n", RC));
		}else{

			if(pPrimUnitDisk->Length == sizeof(LSMPIOCTL_PRIMUNITDISKINFO)) {
				RtlCopyMemory(PrimUnitDisk, pPrimUnitDisk, sizeof(LSMPIOCTL_PRIMUNITDISKINFO));
			}

			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Success XixFsGetNdadDeviceInfo Status (0x%x)\n", RC));
		}

	}finally{
		ObDereferenceObject(scsiAdpaterDeviceObject);
		ExFreePool(pSrbIoCtl);

	}

	DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit XixFsGetNdadDeviceInfo Status (0x%x)\n", RC));

	return RC;

}




NTSTATUS
XixFsAddUserBacl(
	IN	PDEVICE_OBJECT	DeviceObject,
	IN	PNDAS_BLOCK_ACE	NdasBace,
	OUT PBLOCKACE_ID	BlockAceId
){
	NTSTATUS		RC = STATUS_SUCCESS;
	BOOLEAN			result;
	PSRB_IO_CONTROL	pSrbIoCtl;
	int				OutbuffSize;
	PNDAS_BLOCK_ACE	ndasBace;
	PBLOCKACE_ID	blockAceId;
	PDEVICE_OBJECT	scsiAdpaterDeviceObject;

	PAGED_CODE();

	ASSERT(DeviceObject);	


	
	result = XixFsGetScsiportAdapter(DeviceObject, &scsiAdpaterDeviceObject);
	
	if(result != TRUE){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail XixFsAddUserBacl!!! \n"));
		return STATUS_UNSUCCESSFUL;
	}	
	

	OutbuffSize = sizeof(SRB_IO_CONTROL) + sizeof(LSMPIOCTL_QUERYINFO) + sizeof(NDAS_BLOCK_ACE);
	OutbuffSize = SECTORALIGNSIZE_512(OutbuffSize);
	pSrbIoCtl = (PSRB_IO_CONTROL)ExAllocatePool(NonPagedPool , OutbuffSize);

	if(!pSrbIoCtl){
		ObDereferenceObject(scsiAdpaterDeviceObject);
		return STATUS_INSUFFICIENT_RESOURCES;
	}	


	try{
		RtlZeroMemory(pSrbIoCtl, OutbuffSize);
		pSrbIoCtl->HeaderLength = sizeof(SRB_IO_CONTROL);
		RtlCopyMemory(pSrbIoCtl->Signature, LANSCSIMINIPORT_IOCTL_SIGNATURE, 8);
		pSrbIoCtl->Timeout = 10;
		pSrbIoCtl->ControlCode = LANSCSIMINIPORT_IOCTL_ADD_USERBACL;
		pSrbIoCtl->Length = sizeof(NDAS_BLOCK_ACE);

		ndasBace = (PNDAS_BLOCK_ACE)((PUCHAR)pSrbIoCtl + sizeof(SRB_IO_CONTROL));
		blockAceId = (PBLOCKACE_ID)ndasBace;
		RtlCopyMemory(ndasBace, NdasBace, sizeof(NDAS_BLOCK_ACE));



		RC = XixFsRawDevIoCtrl ( 
						scsiAdpaterDeviceObject,
						IOCTL_SCSI_MINIPORT,
						(uint8 *)pSrbIoCtl,
						OutbuffSize,
						(uint8 *)pSrbIoCtl,
						OutbuffSize,
						FALSE,
						NULL
						);

		if(NT_SUCCESS(RC)) {
			*BlockAceId = *blockAceId;
		} else {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail XixFsAddUserBacl Not NetDisk. status = %x\n", RC));
		}


	}finally{
		ObDereferenceObject(scsiAdpaterDeviceObject);
		ExFreePool(pSrbIoCtl);
	}

	return RC;
}




NTSTATUS
XixFsRemoveUserBacl(
	IN	PDEVICE_OBJECT	DeviceObject,
	IN	BLOCKACE_ID		BlockAceId
){
	NTSTATUS		RC = STATUS_SUCCESS;
	BOOLEAN			result;
	PSRB_IO_CONTROL	pSrbIoCtl;
	int				OutbuffSize;
	PBLOCKACE_ID	blockAceId;
	PDEVICE_OBJECT	scsiAdpaterDeviceObject;

	PAGED_CODE();

	result = XixFsGetScsiportAdapter(DeviceObject, &scsiAdpaterDeviceObject);
	
	if(result != TRUE){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail XixFsRemoveUserBacl!!! \n"));
		return STATUS_UNSUCCESSFUL;
	}	

	OutbuffSize = sizeof(SRB_IO_CONTROL) + sizeof(LSMPIOCTL_QUERYINFO) + sizeof(BLOCKACE_ID);
	OutbuffSize = SECTORALIGNSIZE_512(OutbuffSize);
	pSrbIoCtl = (PSRB_IO_CONTROL)ExAllocatePool(NonPagedPool , OutbuffSize);

	if(!pSrbIoCtl){
		ObDereferenceObject(scsiAdpaterDeviceObject);
		return STATUS_INSUFFICIENT_RESOURCES;
	}	

	
	try{
		RtlZeroMemory(pSrbIoCtl, OutbuffSize);
		pSrbIoCtl->HeaderLength = sizeof(SRB_IO_CONTROL);
		RtlCopyMemory(pSrbIoCtl->Signature, LANSCSIMINIPORT_IOCTL_SIGNATURE, 8);
		pSrbIoCtl->Timeout = 10;
		pSrbIoCtl->ControlCode = LANSCSIMINIPORT_IOCTL_REMOVE_USERBACL;
		pSrbIoCtl->Length = sizeof(BLOCKACE_ID);

		
		blockAceId = (PBLOCKACE_ID)((PUCHAR)pSrbIoCtl + sizeof(SRB_IO_CONTROL));
		*blockAceId = BlockAceId;

		RC = XixFsRawDevIoCtrl ( 
						scsiAdpaterDeviceObject,
						IOCTL_SCSI_MINIPORT,
						(uint8 *)pSrbIoCtl,
						OutbuffSize,
						(uint8 *)pSrbIoCtl,
						OutbuffSize,
						FALSE,
						NULL
						);

		if(!NT_SUCCESS(RC)) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail XixFsRemoveUserBacl Not NetDisk. status = %x\n", RC));
		}


	}finally{
		ObDereferenceObject(scsiAdpaterDeviceObject);
		ExFreePool(pSrbIoCtl);
	}

	return RC;
}



NTSTATUS
XixfsGetNdasBacl(
	IN	PDEVICE_OBJECT	DeviceObject,
	OUT PNDAS_BLOCK_ACL	NdasBacl,
	IN BOOLEAN			SystemOrUser
)
{

	NTSTATUS		RC = STATUS_SUCCESS;
	BOOLEAN			result;
	PSRB_IO_CONTROL	pSrbIoCtl;
	PLSMPIOCTL_QUERYINFO	QueryInfo;
	int				OutbuffSize = 0;
	PUCHAR			outBuff = NULL;
	PDEVICE_OBJECT	scsiAdpaterDeviceObject;


	PAGED_CODE();

	ASSERT(DeviceObject);	


	// need somemore
	ASSERT( NdasBacl->Length >= sizeof(LSMPIOCTL_QUERYINFO) );
	
	result = XixFsGetScsiportAdapter(DeviceObject, &scsiAdpaterDeviceObject);
	
	if(result != TRUE){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail XixfsGetNdasBacl!!! \n"));
		return STATUS_UNSUCCESSFUL;
	}	


	

	if(NdasBacl->Length < sizeof(LSMPIOCTL_QUERYINFO)){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail XixfsGetNdasBacl : TOO SAMLL BUFFER !!! \n"));
		return STATUS_UNSUCCESSFUL;
	}

	OutbuffSize = sizeof(SRB_IO_CONTROL) + NdasBacl->Length;
	OutbuffSize = SECTORALIGNSIZE_512(OutbuffSize);
	pSrbIoCtl = (PSRB_IO_CONTROL)ExAllocatePool(NonPagedPool , OutbuffSize);

	if(!pSrbIoCtl){
		ObDereferenceObject(scsiAdpaterDeviceObject);
		return STATUS_INSUFFICIENT_RESOURCES;
	}	

	try{
		RtlZeroMemory(pSrbIoCtl, OutbuffSize);
		pSrbIoCtl->HeaderLength = sizeof(SRB_IO_CONTROL);
		RtlCopyMemory(pSrbIoCtl->Signature, LANSCSIMINIPORT_IOCTL_SIGNATURE, 8);
		pSrbIoCtl->Timeout = 10;
		pSrbIoCtl->ControlCode = LANSCSIMINIPORT_IOCTL_QUERYINFO_EX;
		pSrbIoCtl->Length = OutbuffSize;


		
		QueryInfo = (PLSMPIOCTL_QUERYINFO)((PUCHAR)pSrbIoCtl + sizeof(SRB_IO_CONTROL));
		outBuff = (PUCHAR)QueryInfo;

		QueryInfo->Length = sizeof(LSMPIOCTL_QUERYINFO);
		QueryInfo->InfoClass = SystemOrUser?LsmpSystemBacl:LsmpUserBacl;
		QueryInfo->QueryDataLength = 0;

		RC = XixFsRawDevIoCtrl ( 
						scsiAdpaterDeviceObject,
						IOCTL_SCSI_MINIPORT,
						(uint8 *)pSrbIoCtl,
						OutbuffSize,
						(uint8 *)pSrbIoCtl,
						OutbuffSize,
						FALSE,
						NULL
						);
		
		if(NT_SUCCESS(RC)){
			RtlCopyMemory(NdasBacl, outBuff, NdasBacl->Length);
		}
		
	}finally{
		ObDereferenceObject(scsiAdpaterDeviceObject);
		ExFreePool(pSrbIoCtl);
	}

	return RC;
}





NTSTATUS
XixFsCheckXifsd(
	PDEVICE_OBJECT			DeviceObject,
	PPARTITION_INFORMATION	partitionInfo,
	PBLOCKACE_ID			BlockAceId,
	PBOOLEAN				IsWriteProtected
)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	LSMPIOCTL_PRIMUNITDISKINFO DiskInfo;
	NDAS_BLOCK_ACE	Partition_BACL;
	ASSERT(DeviceObject);
	
	*IsWriteProtected = FALSE;
	
	DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XixFsSetXifsd \n"));

	try{
		RC = XixFsGetNdadDiskInfo(DeviceObject, &DiskInfo);

		if(NT_SUCCESS(RC)) {
				
			// is device support ndas lock
			if((DiskInfo.Lur.SupportedFeatures & (NDASFEATURE_GP_LOCK|NDASFEATURE_ADV_GP_LOCK)) == 0 ){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Enter XixFsSetXifsd : don't support lock \n"));
				RC = STATUS_UNSUCCESSFUL;
				try_return(RC);
			}

		
		
			if(DiskInfo.Lur.DeviceMode != DEVMODE_SHARED_READWRITE){

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Failed XixFsSetXifsd : don't support read write \n"));
// Changed by ILGU HONG for readonly 09052006
				*IsWriteProtected = TRUE;
				RC = STATUS_SUCCESS;
// Changed by ILGU HONG for readonly end
				try_return(RC);
			}

				
			if(DiskInfo.Lur.EnabledFeatures  & NDASFEATURE_RO_FAKE_WRITE){
				
				RtlZeroMemory(&Partition_BACL, sizeof(PNDAS_BLOCK_ACE));
				Partition_BACL.AccessMode = NBACE_ACCESS_WRITE;
				Partition_BACL.BlockEndAddr = (partitionInfo->StartingOffset.QuadPart + partitionInfo->PartitionLength.QuadPart)/512 -1;
				Partition_BACL.BlockStartAddr = partitionInfo->StartingOffset.QuadPart/512;

				RC = XixFsAddUserBacl(DeviceObject, &Partition_BACL,BlockAceId);
				
				if(!NT_SUCCESS(RC)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Faile XixFsSetXifsd : XixFsAddUserBacl \n"));					
				}

			}


		}
	}finally{

	}


	return RC;	
}




/*
NTSTATUS
XixFsSetXifsd(
	PXIFS_IRPCONTEXT IrpContext, 
	PXIFS_VCB VCB)
{

	PDEVICE_OBJECT		DeviceObject = NULL;
	PDEVICE_OBJECT		scsiAdpaterDeviceObject = NULL;
	BOOLEAN				result = FALSE;
	NTSTATUS			RC;
	PSRB_IO_CONTROL		pSrbIoCtl = NULL;
	uint32				OutbuffSize = 0;


	PAGED_CODE();

	ASSERT_VCB(VCB);

	DeviceObject = VCB->TargetDeviceObject;
	ASSERT(DeviceObject);

	
	DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XixFsSetXifsd \n"));

	result = XixFsGetScsiportAdapter(DeviceObject, &scsiAdpaterDeviceObject);
	if(result != TRUE){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail GetScsiportAdpater!!! \n"));
		return STATUS_UNSUCCESSFUL;
	}

	OutbuffSize = sizeof(SRB_IO_CONTROL);
	OutbuffSize = SECTORALIGNSIZE_512(OutbuffSize);
	pSrbIoCtl = (PSRB_IO_CONTROL)ExAllocatePoolWithTag(NonPagedPool, OutbuffSize, TAG_BUFFER);
	
	if(!pSrbIoCtl){
		ObDereferenceObject(scsiAdpaterDeviceObject);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	try{
		RtlZeroMemory(pSrbIoCtl, OutbuffSize);
		
		pSrbIoCtl->HeaderLength = sizeof(SRB_IO_CONTROL);
		RtlCopyMemory(pSrbIoCtl->Signature, LANSCSIMINIPORT_IOCTL_SIGNATURE, 8);
		pSrbIoCtl->Timeout = 10;
		pSrbIoCtl->ControlCode = LANSCSIMINIPORT_IOCTL_UPDATE_XIFS;
		pSrbIoCtl->Length =  0;
		
		RC = XixFsRawDevIoCtrl ( 
						scsiAdpaterDeviceObject,
						IOCTL_SCSI_MINIPORT,
						(uint8 *)pSrbIoCtl,
						OutbuffSize,
						(uint8 *)pSrbIoCtl,
						OutbuffSize,
						FALSE,
						NULL
						);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Fail XixFsRawDevIoCtrl Status (0x%x)\n", RC));
		}else{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Success XixFsRawDevIoCtrl Status (0x%x)\n", RC));
		}

	}finally{
		ObDereferenceObject(scsiAdpaterDeviceObject);
		ExFreePool(pSrbIoCtl);

	}

	DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit XixFsSetXifsd Status (0x%x)\n", RC));

	return STATUS_SUCCESS;
	
}
*/

	//	Added by ILGU HONG end

NTSTATUS
XixFsNdasLock(
	PDEVICE_OBJECT	DeviceObject
)
{
	NTSTATUS		RC;
	PSRB_IO_CONTROL	pSrbIoCtl = NULL;
	uint32			OutbuffSize = 0;
	PLSMPIOCTL_GB_LOCK_QUERY	QueryLock = NULL;
	PDEVICE_OBJECT		scsiAdpaterDeviceObject = NULL;
	BOOLEAN				result = FALSE;
	
	PAGED_CODE();

	

	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XixFsNdasLock \n"));

	result = XixFsGetScsiportAdapter(DeviceObject, &scsiAdpaterDeviceObject);
	if(result != TRUE){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail XixFsGetScsiportAdpater!!! \n"));
		return STATUS_UNSUCCESSFUL;
	}

	OutbuffSize = sizeof(SRB_IO_CONTROL) +  sizeof(LSMPIOCTL_GB_LOCK_QUERY);
	OutbuffSize = SECTORALIGNSIZE_512(OutbuffSize);
	pSrbIoCtl = (PSRB_IO_CONTROL)ExAllocatePoolWithTag(NonPagedPool, OutbuffSize, TAG_BUFFER);
	if(!pSrbIoCtl){
		ObDereferenceObject(scsiAdpaterDeviceObject);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	try{
		RtlZeroMemory(pSrbIoCtl, OutbuffSize);
		
		pSrbIoCtl->HeaderLength = sizeof(SRB_IO_CONTROL);
		RtlCopyMemory(pSrbIoCtl->Signature, LANSCSIMINIPORT_IOCTL_SIGNATURE, 8);
		pSrbIoCtl->Timeout = 10;
		pSrbIoCtl->ControlCode = LANSCSIMINIPORT_IOCTL_QUERY_GB_LOCK;
		pSrbIoCtl->Length = sizeof(LSMPIOCTL_GB_LOCK_QUERY);

		QueryLock = (PLSMPIOCTL_GB_LOCK_QUERY)((PUCHAR)pSrbIoCtl + sizeof(SRB_IO_CONTROL));
		QueryLock->Operation = NdasXifsdLockGB;
		
		RC = XixFsRawDevIoCtrl ( 
						scsiAdpaterDeviceObject,
						IOCTL_SCSI_MINIPORT,
						(uint8 *)pSrbIoCtl,
						OutbuffSize,
						(uint8 *)pSrbIoCtl,
						OutbuffSize,
						FALSE,
						NULL
						);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Fail XixFsRawDevIoCtrl Status (0x%x)\n", RC));
		}else{
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
				("Status of Lock (0x%x) Info(0x%02x:%02x:%02x:%02x-%02x:%02x:%02x:%02x)\n",
					QueryLock->Status,
					QueryLock->Information[0],	QueryLock->Information[1],
					QueryLock->Information[2],	QueryLock->Information[3],
					QueryLock->Information[4],	QueryLock->Information[5],
					QueryLock->Information[6],	QueryLock->Information[7] ));
		}

	}finally{
		ObDereferenceObject(scsiAdpaterDeviceObject);
		ExFreePool(pSrbIoCtl);

	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit XixFsNdasLock Status (0x%x)\n", RC));

	return RC;
}


NTSTATUS
XixFsNdasUnLock(
	PDEVICE_OBJECT	DeviceObject
)
{

	NTSTATUS		RC;
	PSRB_IO_CONTROL	pSrbIoCtl = NULL;
	uint32			OutbuffSize = 0;
	PLSMPIOCTL_GB_LOCK_QUERY	QueryLock = NULL;
	PDEVICE_OBJECT		scsiAdpaterDeviceObject = NULL;
	BOOLEAN				result = FALSE;
	

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XixFsNdasUnLock \n"));

	result = XixFsGetScsiportAdapter(DeviceObject, &scsiAdpaterDeviceObject);
	if(result != TRUE){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail GetScsiportAdpater!!! \n"));
		return STATUS_UNSUCCESSFUL;
	}

	OutbuffSize = sizeof(SRB_IO_CONTROL) +  sizeof(LSMPIOCTL_GB_LOCK_QUERY);
	OutbuffSize = SECTORALIGNSIZE_512(OutbuffSize);
	pSrbIoCtl = (PSRB_IO_CONTROL)ExAllocatePoolWithTag(NonPagedPool, OutbuffSize, TAG_BUFFER);
	if(!pSrbIoCtl){
		ObDereferenceObject(scsiAdpaterDeviceObject);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	try{
		RtlZeroMemory(pSrbIoCtl, OutbuffSize);
		
		pSrbIoCtl->HeaderLength = sizeof(SRB_IO_CONTROL);
		RtlCopyMemory(pSrbIoCtl->Signature, LANSCSIMINIPORT_IOCTL_SIGNATURE, 8);
		pSrbIoCtl->Timeout = 10;
		pSrbIoCtl->ControlCode = LANSCSIMINIPORT_IOCTL_QUERY_GB_LOCK;
		pSrbIoCtl->Length = sizeof(LSMPIOCTL_GB_LOCK_QUERY);

		QueryLock = (PLSMPIOCTL_GB_LOCK_QUERY)((PUCHAR)pSrbIoCtl + sizeof(SRB_IO_CONTROL));
		QueryLock->Operation = NdasXifsdUnlockGB;
		
		RC = XixFsRawDevIoCtrl ( 
						scsiAdpaterDeviceObject,
						IOCTL_SCSI_MINIPORT,
						(uint8 *)pSrbIoCtl,
						OutbuffSize,
						(uint8 *)pSrbIoCtl,
						OutbuffSize,
						FALSE,
						NULL
						);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Fail XixFsRawDevIoCtrl Status (0x%x)\n", RC));
		}else{
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
				("Status of Lock (0x%x) Info(0x%02x:%02x:%02x:%02x-%02x:%02x:%02x:%02x)\n",
					QueryLock->Status,
					QueryLock->Information[0],	QueryLock->Information[1],
					QueryLock->Information[2],	QueryLock->Information[3],
					QueryLock->Information[4],	QueryLock->Information[5],
					QueryLock->Information[6],	QueryLock->Information[7] ));
		}

	}finally{
		ObDereferenceObject(scsiAdpaterDeviceObject);
		ExFreePool(pSrbIoCtl);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit XixFsNdasUnLock Status (0x%x)\n", RC));

	return RC;

}


NTSTATUS
XixFsNdasQueryLock(
	PDEVICE_OBJECT	DeviceObject
)
{



	NTSTATUS		RC;
	PSRB_IO_CONTROL	pSrbIoCtl = NULL;
	uint32			OutbuffSize = 0;
	PLSMPIOCTL_GB_LOCK_QUERY	QueryLock = NULL;
	PDEVICE_OBJECT		scsiAdpaterDeviceObject = NULL;
	BOOLEAN				result = FALSE;


	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XixFsNdasQueryLock \n"));

	result = XixFsGetScsiportAdapter(DeviceObject, &scsiAdpaterDeviceObject);
	if(result != TRUE){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail XixFsGetScsiportAdpater!!! \n"));
		return STATUS_UNSUCCESSFUL;
	}


	OutbuffSize = sizeof(SRB_IO_CONTROL) +  sizeof(LSMPIOCTL_GB_LOCK_QUERY);
	OutbuffSize = SECTORALIGNSIZE_512(OutbuffSize);
	pSrbIoCtl = (PSRB_IO_CONTROL)ExAllocatePoolWithTag(NonPagedPool, OutbuffSize, TAG_BUFFER);
	if(!pSrbIoCtl){
		ObDereferenceObject(scsiAdpaterDeviceObject);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	try{
		RtlZeroMemory(pSrbIoCtl, OutbuffSize);
		
		pSrbIoCtl->HeaderLength = sizeof(SRB_IO_CONTROL);
		RtlCopyMemory(pSrbIoCtl->Signature, LANSCSIMINIPORT_IOCTL_SIGNATURE, 8);
		pSrbIoCtl->Timeout = 10;
		pSrbIoCtl->ControlCode = LANSCSIMINIPORT_IOCTL_QUERY_GB_LOCK;
		pSrbIoCtl->Length =  sizeof(LSMPIOCTL_GB_LOCK_QUERY);

		QueryLock = (PLSMPIOCTL_GB_LOCK_QUERY)((PUCHAR)pSrbIoCtl + sizeof(SRB_IO_CONTROL));
		QueryLock->Operation = NdasXifsdQueryLockGB;
		
		RC = XixFsRawDevIoCtrl ( 
						scsiAdpaterDeviceObject,
						IOCTL_SCSI_MINIPORT,
						(uint8 *)pSrbIoCtl,
						OutbuffSize,
						(uint8 *)pSrbIoCtl,
						OutbuffSize,
						FALSE,
						NULL
						);

		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Fail XixFsRawDevIoCtrl Status (0x%x)\n", RC));
		}else{
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
				("Status of Lock (0x%x) Info(0x%02x:%02x:%02x:%02x-%02x:%02x:%02x:%02x)\n",
					QueryLock->Status,
					QueryLock->Information[0],	QueryLock->Information[1],
					QueryLock->Information[2],	QueryLock->Information[3],
					QueryLock->Information[4],	QueryLock->Information[5],
					QueryLock->Information[6],	QueryLock->Information[7] ));
		}

	}finally{
		ObDereferenceObject(scsiAdpaterDeviceObject);
		ExFreePool(pSrbIoCtl);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit XixFsNdasUnLock Status (0x%x)\n", RC));

	return RC;

}