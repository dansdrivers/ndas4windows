#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDrv.h"
#include "SocketLpx.h"
#include "lpxtdi.h"
#include "XixFsComProto.h"
#include "XixFsGlobalData.h"
#include "XixFsProto.h"
#include "XixFsdInternalApi.h"
#include "XixFsRawDiskAccessApi.h"



NTSTATUS
XixFsdQueryFsSizeInfo (
    IN PXIFS_IRPCONTEXT 			pIrpContext,
    IN PXIFS_VCB 					pVcb,
    IN PFILE_FS_SIZE_INFORMATION 	Buffer,
    IN uint32 						Length,
    IN OUT uint32 					*ByteToReturn
    );


NTSTATUS
XixFsdQueryFsVolumeInfo(
    IN PXIFS_IRPCONTEXT					pIrpContext,
    IN PXIFS_VCB 						pVcb,
    IN PFILE_FS_VOLUME_INFORMATION 		Buffer,
    IN uint32 							Length,
    IN OUT uint32 						*ByteToReturn
    );


NTSTATUS
XixFsdQueryFsDeviceInfo(
    IN PXIFS_IRPCONTEXT					pIrpContext,
    IN PXIFS_VCB 						pVcb,
    IN PFILE_FS_DEVICE_INFORMATION 		Buffer,
    IN uint32 							Length,
    IN OUT uint32 						*ByteToReturn
    );

NTSTATUS
XixFsdQueryFsAttributeInfo(
    IN PXIFS_IRPCONTEXT	 				pIrpContext,
    IN PXIFS_VCB 						pVcb,
    IN PFILE_FS_ATTRIBUTE_INFORMATION 	Buffer,
    IN uint32 							Length,
    IN OUT uint32 						*ByteToReturn
    );




#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsdQueryFsSizeInfo)
#pragma alloc_text(PAGE, XixFsdQueryFsVolumeInfo)
#pragma alloc_text(PAGE, XixFsdQueryFsDeviceInfo)
#pragma alloc_text(PAGE, XixFsdQueryFsAttributeInfo)
#pragma alloc_text(PAGE, XixFsdCommonQueryVolumeInformation)
#pragma alloc_text(PAGE, XixFsdCommonSetVolumeInformation)
#endif


NTSTATUS
XixFsdQueryFsSizeInfo (
    IN PXIFS_IRPCONTEXT 			pIrpContext,
    IN PXIFS_VCB 					pVcb,
    IN PFILE_FS_SIZE_INFORMATION 	Buffer,
    IN uint32 						Length,
    IN OUT uint32 					*ByteToReturn
    )
{

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdQueryFsSizeInfo \n"));

	if(Length < sizeof(FILE_FS_SIZE_INFORMATION)){
		*ByteToReturn = 0;
		return STATUS_INVALID_PARAMETER;
	}

	/*
	Buffer->TotalAllocationUnits.QuadPart = pVcb->NumLots;
	Buffer->AvailableAllocationUnits.QuadPart = XixFsfindSetBitMapCount(pVcb->NumLots, pVcb->VolumeFreeMap);
	
	Buffer->SectorsPerAllocationUnit = (pVcb->LotSize / pVcb->SectorSize);
	Buffer->BytesPerSector = pVcb->SectorSize;
	*/

	Buffer->TotalAllocationUnits.QuadPart = GetLcnFromLot(pVcb->LotSize, pVcb->NumLots );
	// changed by ILGU HONG for readonly 09052006
	if(pVcb->IsVolumeWriteProctected){
		Buffer->AvailableAllocationUnits.QuadPart = ((pVcb->LotSize/CLUSTER_SIZE)* pVcb->NumLots);
	}else{
		Buffer->AvailableAllocationUnits.QuadPart = ((pVcb->LotSize/CLUSTER_SIZE)* XixFsfindSetBitMapCount(pVcb->NumLots, pVcb->VolumeFreeMap));
	}
	// changed by ILGU HONG for readonly end

	
	
	Buffer->SectorsPerAllocationUnit = (CLUSTER_SIZE / pVcb->SectorSize);
	Buffer->BytesPerSector = pVcb->SectorSize;	
	

	/*
	DbgPrint("1 NumLots(%I64d): TotalUnit(%I64d):AvailableUnit(%I64d):SecPerUnit(%ld):BytesPerSec(%ld)\n",
			pVcb->NumLots,
			Buffer->TotalAllocationUnits.QuadPart,
			Buffer->AvailableAllocationUnits.QuadPart,
			Buffer->SectorsPerAllocationUnit,
			Buffer->BytesPerSector);
	*/

	*ByteToReturn =(( sizeof(FILE_FS_SIZE_INFORMATION) + 7)/8)*8;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdQueryFsSizeInfo \n"));
	return STATUS_SUCCESS;
	
}


NTSTATUS
XixFsdQueryFsVolumeInfo(
    IN PXIFS_IRPCONTEXT					pIrpContext,
    IN PXIFS_VCB 						pVcb,
    IN PFILE_FS_VOLUME_INFORMATION 		Buffer,
    IN uint32 							Length,
    IN OUT uint32 						*ByteToReturn
    )
{

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdQueryFsVolumeInfo \n"));

	if(Length < sizeof(FILE_FS_VOLUME_INFORMATION)){
		*ByteToReturn = 0;
		return STATUS_INVALID_PARAMETER;
	}
	Buffer->VolumeCreationTime.QuadPart = pVcb->VolCreationTime;
	Buffer->VolumeSerialNumber = pVcb->VolSerialNumber;
	Buffer->VolumeLabelLength = pVcb->VolLabel.Length;



	if(Length < (FIELD_OFFSET(FILE_FS_VOLUME_INFORMATION, VolumeLabel) + Buffer->VolumeLabelLength))
	{
		*ByteToReturn =(( FIELD_OFFSET(FILE_FS_VOLUME_INFORMATION, VolumeLabel) + 7 + Buffer->VolumeLabelLength)/8)*8;
		return STATUS_SUCCESS;
	}
	
	RtlCopyMemory(Buffer->VolumeLabel, pVcb->VolLabel.Buffer, Buffer->VolumeLabelLength);
	*ByteToReturn =(( sizeof(FILE_FS_VOLUME_INFORMATION) + Buffer->VolumeLabelLength + 7)/8)*8;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdQueryFsVolumeInfo \n"));

	return STATUS_SUCCESS;
}


NTSTATUS
XixFsdQueryFsDeviceInfo(
    IN PXIFS_IRPCONTEXT					pIrpContext,
    IN PXIFS_VCB 						pVcb,
    IN PFILE_FS_DEVICE_INFORMATION 		Buffer,
    IN uint32 							Length,
    IN OUT uint32 						*ByteToReturn
    )
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdQueryFsDeviceInfo \n"));
	
	if(Length < sizeof(FILE_FS_DEVICE_INFORMATION)){
		*ByteToReturn = 0;
		return STATUS_INVALID_PARAMETER;
	}

	Buffer->DeviceType = FILE_DEVICE_DISK;
	Buffer->Characteristics = pVcb->TargetDeviceObject->Characteristics;
	
	//	Added by ILGU HONG for readonly 09052006
	if(pVcb->IsVolumeWriteProctected){
		XifsdSetFlag(Buffer->Characteristics, FILE_READ_ONLY_DEVICE);
	}else{
		XifsdClearFlag(Buffer->Characteristics, FILE_READ_ONLY_DEVICE);
	}
	//	Added by ILGU HONG for readonly end
	
	*ByteToReturn = sizeof(FILE_FS_DEVICE_INFORMATION);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdQueryFsDeviceInfo \n"));
	return STATUS_SUCCESS;
	
}


NTSTATUS
XixFsdQueryFsAttributeInfo(
    IN PXIFS_IRPCONTEXT	 				pIrpContext,
    IN PXIFS_VCB 						pVcb,
    IN PFILE_FS_ATTRIBUTE_INFORMATION 	Buffer,
    IN uint32 							Length,
    IN OUT uint32 						*ByteToReturn
    )
{
	uint32	FileSystemAttribute = 0;
	uint32	ByteToCopy = 0;
	NTSTATUS		RC = STATUS_SUCCESS;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdQueryFsAttributeInfo \n"));

	if(Length < sizeof(FILE_FS_ATTRIBUTE_INFORMATION)){
		*ByteToReturn = 0;
		return STATUS_INVALID_PARAMETER;		
	}
	
	Length -= FIELD_OFFSET( FILE_FS_ATTRIBUTE_INFORMATION, FileSystemName );
	XifsdClearFlag(Length, 1);

	if(Length >= 8){
		ByteToCopy = 8;
	} else {
		ByteToCopy = Length;
		RC = STATUS_BUFFER_OVERFLOW;
	}

	FileSystemAttribute = (FILE_CASE_SENSITIVE_SEARCH | FILE_CASE_PRESERVED_NAMES | FILE_UNICODE_ON_DISK );


	// Added by ILGU HONG for readonly 09082006
	if(pVcb->IsVolumeWriteProctected){
		FileSystemAttribute |= FILE_READ_ONLY_VOLUME;
	}
	// Added by ILGU HONG for readonly end

	Buffer->FileSystemAttributes = FileSystemAttribute;
	Buffer->MaximumComponentNameLength =  XIFS_MAX_NAME_LEN;
	Buffer->FileSystemNameLength = ByteToCopy;
	
	

	RtlCopyMemory(Buffer->FileSystemName, L"XIFS", Buffer->FileSystemNameLength);
	
	*ByteToReturn = ((FIELD_OFFSET( FILE_FS_ATTRIBUTE_INFORMATION, FileSystemName ) + 3 + Buffer->FileSystemNameLength)/4)*4;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdQueryFsAttributeInfo \n"));

	return RC;
	
}


NTSTATUS
XixFsdQueryFsFullSizeInfo (
    IN PXIFS_IRPCONTEXT 				pIrpContext,
    IN PXIFS_VCB 						pVcb,
    IN PFILE_FS_FULL_SIZE_INFORMATION 	Buffer,
    IN uint32 							Length,
    IN OUT uint32 						*ByteToReturn
    )
{

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdQueryFsFullSizeInfo \n"));

	if(Length < sizeof(FILE_FS_FULL_SIZE_INFORMATION)){
		*ByteToReturn = 0;
		return STATUS_INVALID_PARAMETER;
	}
	/*
	Buffer->TotalAllocationUnits.QuadPart = pVcb->NumLots;
	Buffer->ActualAvailableAllocationUnits.QuadPart = XixFsfindSetBitMapCount(pVcb->NumLots, pVcb->VolumeFreeMap);
	Buffer->CallerAvailableAllocationUnits.QuadPart = Buffer->ActualAvailableAllocationUnits.QuadPart;
	
	
	Buffer->SectorsPerAllocationUnit = (pVcb->LotSize / pVcb->SectorSize);
	Buffer->BytesPerSector = pVcb->SectorSize;
	*/
	Buffer->TotalAllocationUnits.QuadPart = GetLcnFromLot(pVcb->LotSize, pVcb->NumLots);
	// changed by ILGU HONG for readonly 09052006
	if(pVcb->IsVolumeWriteProctected){
		Buffer->ActualAvailableAllocationUnits.QuadPart = ((pVcb->LotSize/CLUSTER_SIZE)* pVcb->NumLots);
	}else{
		Buffer->ActualAvailableAllocationUnits.QuadPart = ((pVcb->LotSize/CLUSTER_SIZE)* XixFsfindSetBitMapCount(pVcb->NumLots, pVcb->VolumeFreeMap));
	}
	// changed by ILGU HONG for readonly end

	Buffer->CallerAvailableAllocationUnits.QuadPart = Buffer->ActualAvailableAllocationUnits.QuadPart;
	
	
	Buffer->SectorsPerAllocationUnit = (CLUSTER_SIZE / pVcb->SectorSize);
	Buffer->BytesPerSector = pVcb->SectorSize;
	
	
	/*
	DbgPrint("NumLots(%I64d) TotalUnit(%I64d):AvailableUnit(%I64d):SecPerUnit(%ld):BytesPerSec(%ld)\n",
			pVcb->NumLots,
			Buffer->TotalAllocationUnits.QuadPart,
			Buffer->ActualAvailableAllocationUnits.QuadPart,
			Buffer->SectorsPerAllocationUnit,
			Buffer->BytesPerSector);
	*/
	
	
	*ByteToReturn =(( sizeof(FILE_FS_FULL_SIZE_INFORMATION) + 7)/8)*8;
	


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdQueryFsFullSizeInfo \n"));
	return STATUS_SUCCESS;
	
}



NTSTATUS
XixFsdCommonQueryVolumeInformation(
	IN PXIFS_IRPCONTEXT pIrpContext
	)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	PIRP					pIrp = NULL;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	PXIFS_VCB				pVCB = NULL;
	PXIFS_FCB				pFCB = NULL;
	PXIFS_CCB				pCCB = NULL;
	PFILE_OBJECT				pFileObject = NULL;
	BOOLEAN					Wait = FALSE;
	uint32					BytesToReturn = 0;
	uint32					Length = 0;
	TYPE_OF_OPEN			TypeOfOpen = UnopenedFileObject;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdCommonQueryVolumeInformation \n"));

	ASSERT(pIrpContext);

	pIrp = pIrpContext->Irp;
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	TypeOfOpen = XixFsdDecodeFileObject( pFileObject, &pFCB, &pCCB );

    if (TypeOfOpen == UnopenedFileObject) {
		RC = STATUS_INVALID_PARAMETER;
        XixFsdCompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER, 0 );
        return RC;
    }


	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!VolumeInformation (%wZ) pCCB(%p)\n", &pFCB->FCBName, pCCB));

	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);


	Wait = XifsdCheckFlagBoolean(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);
	




	if(!ExAcquireResourceSharedLite(&(pVCB->VCBResource), Wait)){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
					("PostRequest IrpContext(%p) Irp(%p)\n", pIrpContext, pIrp));
		RC = XixFsdPostRequest(pIrpContext, pIrp);
		return RC;
	}

	try{


		Length = pIrpSp->Parameters.QueryVolume.Length ;


		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
					 ("pIrpSp->Parameters.QueryVolume.FsInformationClass (0x%x)\n", 
					 pIrpSp->Parameters.QueryVolume.FsInformationClass));
	

		switch (pIrpSp->Parameters.QueryVolume.FsInformationClass) {

		case FileFsSizeInformation:
		{
	

			RC = XixFsdQueryFsSizeInfo( pIrpContext, pVCB, pIrp->AssociatedIrp.SystemBuffer, Length, &BytesToReturn );
			XixFsdCompleteRequest(pIrpContext, RC, BytesToReturn);
			break;
		}
		case FileFsVolumeInformation:
		{

			RC = XixFsdQueryFsVolumeInfo( pIrpContext, pVCB, pIrp->AssociatedIrp.SystemBuffer, Length, &BytesToReturn );
			XixFsdCompleteRequest(pIrpContext, RC, BytesToReturn);
			break;
		}
		case FileFsDeviceInformation:
		{

			RC = XixFsdQueryFsDeviceInfo( pIrpContext, pVCB, pIrp->AssociatedIrp.SystemBuffer, Length, &BytesToReturn );
			XixFsdCompleteRequest(pIrpContext, RC, BytesToReturn);
			break;
		}
		case FileFsAttributeInformation:
		{

			RC = XixFsdQueryFsAttributeInfo( pIrpContext, pVCB, pIrp->AssociatedIrp.SystemBuffer, Length, &BytesToReturn );
			XixFsdCompleteRequest(pIrpContext, RC, BytesToReturn);
			break;
		}		
		case FileFsFullSizeInformation:
		{
			RC = XixFsdQueryFsFullSizeInfo(pIrpContext, pVCB, pIrp->AssociatedIrp.SystemBuffer, Length, &BytesToReturn);
			XixFsdCompleteRequest(pIrpContext, RC, BytesToReturn);
			break;
		}
		default:
			DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("default Not supported Volume Info %ld\n",pIrpSp->Parameters.QueryVolume.FsInformationClass));
			RC = STATUS_INVALID_PARAMETER;
			XixFsdCompleteRequest(pIrpContext, RC, 0);
		break;
		 }

		

	}finally{
		ExReleaseResourceLite(&(pVCB->VCBResource));
	}
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdCommonQueryVolumeInformation \n"));
	return RC;	
}

NTSTATUS
XixFsdCommonSetVolumeInformation(
	IN PXIFS_IRPCONTEXT pIrpContext
	)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	PXIFS_FCB				pFCB = NULL;
	PXIFS_CCB				pCCB = NULL;
	PIRP					pIrp = NULL;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	PFILE_OBJECT			pFileObject = NULL;
	TYPE_OF_OPEN			TypeOfOpen = UnopenedFileObject;


	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Enter XixFsdCommonSetVolumeInformation \n"));


	ASSERT(pIrpContext);

	pIrp = pIrpContext->Irp;
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	TypeOfOpen = XixFsdDecodeFileObject( pFileObject, &pFCB, &pCCB );

    if (TypeOfOpen == UnopenedFileObject) {
		RC = STATUS_INVALID_PARAMETER;
        XixFsdCompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER, 0 );
        return RC;
    }

	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!etVolumeInformation (%wZ) pCCB(%p)\n", &pFCB->FCBName, pCCB));

	RC = STATUS_INVALID_PARAMETER;
	XixFsdCompleteRequest(pIrpContext,RC , 0);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Exit XixFsdCommonSetVolumeInformation \n"));
	return RC;
}

