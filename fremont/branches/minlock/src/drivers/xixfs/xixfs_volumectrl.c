#include "xixfs_types.h"
#include "xixfs_drv.h"
#include "SocketLpx.h"
#include "lpxtdi.h"
#include "xixfs_global.h"
#include "xixfs_debug.h"
#include "xixfs_internal.h"



NTSTATUS
xixfs_QueryFsSizeInfo (
    IN PXIXFS_IRPCONTEXT 			pIrpContext,
    IN PXIXFS_VCB 					pVcb,
    IN PFILE_FS_SIZE_INFORMATION 	Buffer,
    IN uint32 						Length,
    IN OUT uint32 					*ByteToReturn
    );


NTSTATUS
xixfs_QueryFsVolumeInfo(
    IN PXIXFS_IRPCONTEXT					pIrpContext,
    IN PXIXFS_VCB 						pVcb,
    IN PFILE_FS_VOLUME_INFORMATION 		Buffer,
    IN uint32 							Length,
    IN OUT uint32 						*ByteToReturn
    );


NTSTATUS
xixfs_QueryFsDeviceInfo(
    IN PXIXFS_IRPCONTEXT					pIrpContext,
    IN PXIXFS_VCB 						pVcb,
    IN PFILE_FS_DEVICE_INFORMATION 		Buffer,
    IN uint32 							Length,
    IN OUT uint32 						*ByteToReturn
    );

NTSTATUS
xixfs_QueryFsAttributeInfo(
    IN PXIXFS_IRPCONTEXT	 				pIrpContext,
    IN PXIXFS_VCB 						pVcb,
    IN PFILE_FS_ATTRIBUTE_INFORMATION 	Buffer,
    IN uint32 							Length,
    IN OUT uint32 						*ByteToReturn
    );




#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, xixfs_QueryFsSizeInfo)
#pragma alloc_text(PAGE, xixfs_QueryFsVolumeInfo)
#pragma alloc_text(PAGE, xixfs_QueryFsDeviceInfo)
#pragma alloc_text(PAGE, xixfs_QueryFsAttributeInfo)
#pragma alloc_text(PAGE, xixfs_CommonQueryVolumeInformation)
#pragma alloc_text(PAGE, xixfs_CommonSetVolumeInformation)
#endif


NTSTATUS
xixfs_QueryFsSizeInfo (
    IN PXIXFS_IRPCONTEXT 			pIrpContext,
    IN PXIXFS_VCB 					pVcb,
    IN PFILE_FS_SIZE_INFORMATION 	Buffer,
    IN uint32 						Length,
    IN OUT uint32 					*ByteToReturn
    )
{

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_QueryFsSizeInfo \n"));

	if(Length < sizeof(FILE_FS_SIZE_INFORMATION)){
		*ByteToReturn = 0;
		return STATUS_INVALID_PARAMETER;
	}



	Buffer->TotalAllocationUnits.QuadPart = xixfs_GetLcnFromLot(pVcb->XixcoreVcb.LotSize, pVcb->XixcoreVcb.NumLots );
	// changed by ILGU HONG for readonly 09052006
	if(pVcb->XixcoreVcb.IsVolumeWriteProtected){
		Buffer->AvailableAllocationUnits.QuadPart = ((pVcb->XixcoreVcb.LotSize/CLUSTER_SIZE)* pVcb->XixcoreVcb.NumLots);
	}else{
		Buffer->AvailableAllocationUnits.QuadPart = ((pVcb->XixcoreVcb.LotSize/CLUSTER_SIZE)* xixcore_FindSetBitCount(pVcb->XixcoreVcb.NumLots, xixcore_GetDataBufferOfBitMap(pVcb->XixcoreVcb.MetaContext.VolumeFreeMap->Data)) );
	}
	// changed by ILGU HONG for readonly end

	
	
	Buffer->SectorsPerAllocationUnit = (CLUSTER_SIZE / pVcb->XixcoreVcb.SectorSize);
	Buffer->BytesPerSector = pVcb->XixcoreVcb.SectorSize;	
	

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
		("Exit xixfs_QueryFsSizeInfo \n"));
	return STATUS_SUCCESS;
	
}


NTSTATUS
xixfs_QueryFsVolumeInfo(
    IN PXIXFS_IRPCONTEXT					pIrpContext,
    IN PXIXFS_VCB 						pVcb,
    IN PFILE_FS_VOLUME_INFORMATION 		Buffer,
    IN uint32 							Length,
    IN OUT uint32 						*ByteToReturn
    )
{

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_QueryFsVolumeInfo \n"));

	if(Length < sizeof(FILE_FS_VOLUME_INFORMATION)){
		*ByteToReturn = 0;
		return STATUS_INVALID_PARAMETER;
	}
	Buffer->VolumeCreationTime.QuadPart = pVcb->XixcoreVcb.VolCreateTime;
	Buffer->VolumeSerialNumber = pVcb->XixcoreVcb.VolSerialNumber;
	Buffer->VolumeLabelLength = pVcb->XixcoreVcb.VolumeNameLength;



	if(Length < (FIELD_OFFSET(FILE_FS_VOLUME_INFORMATION, VolumeLabel) + Buffer->VolumeLabelLength))
	{
		*ByteToReturn =(( FIELD_OFFSET(FILE_FS_VOLUME_INFORMATION, VolumeLabel) + 7 + Buffer->VolumeLabelLength)/8)*8;
		return STATUS_SUCCESS;
	}
	
	if(Buffer->VolumeLabelLength) {
		RtlCopyMemory(Buffer->VolumeLabel, pVcb->XixcoreVcb.VolumeName, Buffer->VolumeLabelLength);
	}
	*ByteToReturn =(( sizeof(FILE_FS_VOLUME_INFORMATION) + Buffer->VolumeLabelLength + 7)/8)*8;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Exit xixfs_QueryFsVolumeInfo \n"));

	return STATUS_SUCCESS;
}


NTSTATUS
xixfs_QueryFsDeviceInfo(
    IN PXIXFS_IRPCONTEXT					pIrpContext,
    IN PXIXFS_VCB 						pVcb,
    IN PFILE_FS_DEVICE_INFORMATION 		Buffer,
    IN uint32 							Length,
    IN OUT uint32 						*ByteToReturn
    )
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_QueryFsDeviceInfo \n"));
	
	if(Length < sizeof(FILE_FS_DEVICE_INFORMATION)){
		*ByteToReturn = 0;
		return STATUS_INVALID_PARAMETER;
	}

	Buffer->DeviceType = FILE_DEVICE_DISK;
	Buffer->Characteristics = pVcb->TargetDeviceObject->Characteristics;
	
	//	Added by ILGU HONG for readonly 09052006
	if(pVcb->XixcoreVcb.IsVolumeWriteProtected){
		XIXCORE_SET_FLAGS(Buffer->Characteristics, FILE_READ_ONLY_DEVICE);
	}else{
		XIXCORE_CLEAR_FLAGS(Buffer->Characteristics, FILE_READ_ONLY_DEVICE);
	}
	//	Added by ILGU HONG for readonly end
	
	*ByteToReturn = sizeof(FILE_FS_DEVICE_INFORMATION);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Exit xixfs_QueryFsDeviceInfo \n"));
	return STATUS_SUCCESS;
	
}


NTSTATUS
xixfs_QueryFsAttributeInfo(
    IN PXIXFS_IRPCONTEXT	 				pIrpContext,
    IN PXIXFS_VCB 						pVcb,
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
		("Enter xixfs_QueryFsAttributeInfo \n"));

	if(Length < sizeof(FILE_FS_ATTRIBUTE_INFORMATION)){
		*ByteToReturn = 0;
		return STATUS_INVALID_PARAMETER;		
	}
	
	Length -= FIELD_OFFSET( FILE_FS_ATTRIBUTE_INFORMATION, FileSystemName );
	XIXCORE_CLEAR_FLAGS(Length, 1);

	if(Length >= 10){
		ByteToCopy = 10;
	} else {
		ByteToCopy = Length;
		RC = STATUS_BUFFER_OVERFLOW;
	}

	FileSystemAttribute = (FILE_CASE_SENSITIVE_SEARCH | FILE_CASE_PRESERVED_NAMES | FILE_UNICODE_ON_DISK );


	// Added by ILGU HONG for readonly 09082006
	if(pVcb->XixcoreVcb.IsVolumeWriteProtected){
		FileSystemAttribute |= FILE_READ_ONLY_VOLUME;
	}
	// Added by ILGU HONG for readonly end

	Buffer->FileSystemAttributes = FileSystemAttribute;
	Buffer->MaximumComponentNameLength =  XIFS_MAX_NAME_LEN;
	Buffer->FileSystemNameLength = ByteToCopy;
	
	

	RtlCopyMemory(Buffer->FileSystemName, L"XIXFS", Buffer->FileSystemNameLength);
	
	*ByteToReturn = ((FIELD_OFFSET( FILE_FS_ATTRIBUTE_INFORMATION, FileSystemName ) + 3 + Buffer->FileSystemNameLength)/4)*4;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Exit xixfs_QueryFsAttributeInfo \n"));

	return RC;
	
}


NTSTATUS
xixfs_QueryFsFullSizeInfo (
    IN PXIXFS_IRPCONTEXT 				pIrpContext,
    IN PXIXFS_VCB 						pVcb,
    IN PFILE_FS_FULL_SIZE_INFORMATION 	Buffer,
    IN uint32 							Length,
    IN OUT uint32 						*ByteToReturn
    )
{

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_QueryFsFullSizeInfo \n"));

	if(Length < sizeof(FILE_FS_FULL_SIZE_INFORMATION)){
		*ByteToReturn = 0;
		return STATUS_INVALID_PARAMETER;
	}
	
	Buffer->TotalAllocationUnits.QuadPart = xixfs_GetLcnFromLot(pVcb->XixcoreVcb.LotSize, pVcb->XixcoreVcb.NumLots);
	// changed by ILGU HONG for readonly 09052006
	if(pVcb->XixcoreVcb.IsVolumeWriteProtected){
		Buffer->ActualAvailableAllocationUnits.QuadPart = ((pVcb->XixcoreVcb.LotSize/CLUSTER_SIZE)* pVcb->XixcoreVcb.NumLots);
	}else{
		Buffer->ActualAvailableAllocationUnits.QuadPart = ((pVcb->XixcoreVcb.LotSize/CLUSTER_SIZE)* xixcore_FindSetBitCount(pVcb->XixcoreVcb.NumLots, xixcore_GetDataBufferOfBitMap(pVcb->XixcoreVcb.MetaContext.VolumeFreeMap->Data)) );
	}
	// changed by ILGU HONG for readonly end

	Buffer->CallerAvailableAllocationUnits.QuadPart = Buffer->ActualAvailableAllocationUnits.QuadPart;
	
	
	Buffer->SectorsPerAllocationUnit = (CLUSTER_SIZE / pVcb->XixcoreVcb.SectorSize);
	Buffer->BytesPerSector = pVcb->XixcoreVcb.SectorSize;
	
	
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
		("Exit xixfs_QueryFsFullSizeInfo \n"));
	return STATUS_SUCCESS;
	
}



NTSTATUS
xixfs_CommonQueryVolumeInformation(
	IN PXIXFS_IRPCONTEXT pIrpContext
	)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	PIRP					pIrp = NULL;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	PXIXFS_VCB				pVCB = NULL;
	PXIXFS_FCB				pFCB = NULL;
	PXIXFS_CCB				pCCB = NULL;
	PFILE_OBJECT				pFileObject = NULL;
	BOOLEAN					Wait = FALSE;
	uint32					BytesToReturn = 0;
	uint32					Length = 0;
	TYPE_OF_OPEN			TypeOfOpen = UnopenedFileObject;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_CommonQueryVolumeInformation \n"));

	ASSERT(pIrpContext);

	pIrp = pIrpContext->Irp;
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	TypeOfOpen = xixfs_DecodeFileObject( pFileObject, &pFCB, &pCCB );

    if (TypeOfOpen == UnopenedFileObject) {
		RC = STATUS_INVALID_PARAMETER;
        xixfs_CompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER, 0 );
        return RC;
    }


	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!VolumeInformation  pCCB(%p)\n", pCCB));

	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);


	Wait = XIXCORE_TEST_FLAGS(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);
	




	if(!ExAcquireResourceSharedLite(&(pVCB->VCBResource), Wait)){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
					("PostRequest IrpContext(%p) Irp(%p)\n", pIrpContext, pIrp));
		RC = xixfs_PostRequest(pIrpContext, pIrp);
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
	

			RC = xixfs_QueryFsSizeInfo( pIrpContext, pVCB, pIrp->AssociatedIrp.SystemBuffer, Length, &BytesToReturn );
			xixfs_CompleteRequest(pIrpContext, RC, BytesToReturn);
			break;
		}
		case FileFsVolumeInformation:
		{

			RC = xixfs_QueryFsVolumeInfo( pIrpContext, pVCB, pIrp->AssociatedIrp.SystemBuffer, Length, &BytesToReturn );
			xixfs_CompleteRequest(pIrpContext, RC, BytesToReturn);
			break;
		}
		case FileFsDeviceInformation:
		{

			RC = xixfs_QueryFsDeviceInfo( pIrpContext, pVCB, pIrp->AssociatedIrp.SystemBuffer, Length, &BytesToReturn );
			xixfs_CompleteRequest(pIrpContext, RC, BytesToReturn);
			break;
		}
		case FileFsAttributeInformation:
		{

			RC = xixfs_QueryFsAttributeInfo( pIrpContext, pVCB, pIrp->AssociatedIrp.SystemBuffer, Length, &BytesToReturn );
			xixfs_CompleteRequest(pIrpContext, RC, BytesToReturn);
			break;
		}		
		case FileFsFullSizeInformation:
		{
			RC = xixfs_QueryFsFullSizeInfo(pIrpContext, pVCB, pIrp->AssociatedIrp.SystemBuffer, Length, &BytesToReturn);
			xixfs_CompleteRequest(pIrpContext, RC, BytesToReturn);
			break;
		}
		default:
			DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("default Not supported Volume Info %ld\n",pIrpSp->Parameters.QueryVolume.FsInformationClass));
			RC = STATUS_INVALID_PARAMETER;
			xixfs_CompleteRequest(pIrpContext, RC, 0);
		break;
		 }

		

	}finally{
		ExReleaseResourceLite(&(pVCB->VCBResource));
	}
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Exit xixfs_CommonQueryVolumeInformation \n"));
	return RC;	
}

NTSTATUS
xixfs_CommonSetVolumeInformation(
	IN PXIXFS_IRPCONTEXT pIrpContext
	)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	PXIXFS_FCB				pFCB = NULL;
	PXIXFS_CCB				pCCB = NULL;
	PIRP					pIrp = NULL;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	PFILE_OBJECT			pFileObject = NULL;
	TYPE_OF_OPEN			TypeOfOpen = UnopenedFileObject;


	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Enter xixfs_CommonSetVolumeInformation \n"));


	ASSERT(pIrpContext);

	pIrp = pIrpContext->Irp;
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

	TypeOfOpen = xixfs_DecodeFileObject( pFileObject, &pFCB, &pCCB );

    if (TypeOfOpen == UnopenedFileObject) {
		RC = STATUS_INVALID_PARAMETER;
        xixfs_CompleteRequest( pIrpContext, STATUS_INVALID_PARAMETER, 0 );
        return RC;
    }

	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!etVolumeInformation pCCB(%p)\n", pCCB));

	RC = STATUS_INVALID_PARAMETER;
	xixfs_CompleteRequest(pIrpContext,RC , 0);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VOLINFO|DEBUG_TARGET_IRPCONTEXT),
		("Exit xixfs_CommonSetVolumeInformation \n"));
	return RC;
}

