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
XixFsdQueryInternalInfo (
	IN PXIFS_FCB 		pFCB, 
	IN PXIFS_CCB		pCCB, 
    IN OUT PFILE_INTERNAL_INFORMATION FileInternalInformation,
    IN OUT ULONG *Length 
);


NTSTATUS
XixFsdQueryEaInfo (
	IN PXIFS_FCB 		pFCB, 
	IN PXIFS_CCB		pCCB, 
    	IN OUT PFILE_EA_INFORMATION FileEaInformation,
    	IN OUT ULONG *Length 
    );


NTSTATUS
XixFsdQueryAccessInfo (
	IN PXIFS_FCB 		pFCB, 
	IN PXIFS_CCB		pCCB, 
    	IN OUT PFILE_ACCESS_INFORMATION FileAccessInformation,
    	IN OUT ULONG *Length 
    );


NTSTATUS
XixFsdQueryPositionInfo (
	IN PFILE_OBJECT		pFileObject, 
    	IN OUT PFILE_POSITION_INFORMATION FilePositionInformation,
    	IN OUT ULONG *Length 
    );

NTSTATUS
XixFsdQueryModeInfo (
	IN PXIFS_FCB 		pFCB, 
	IN PXIFS_CCB		pCCB, 
    	IN OUT PFILE_MODE_INFORMATION FileModeInformation,
    	IN OUT ULONG *Length 
    );

NTSTATUS
XixFsdQueryAlignInfo (
	IN PXIFS_FCB 		pFCB, 
	IN PXIFS_CCB		pCCB, 
    	IN OUT PFILE_ALIGNMENT_INFORMATION FileAlignInformation,
    	IN OUT ULONG *Length 
    );


NTSTATUS
XixFsdQueryNameInfo (
	IN PFILE_OBJECT pFileObject,
	IN OUT PFILE_NAME_INFORMATION FileNameInformation,
	IN OUT PULONG Length
    );

NTSTATUS
XixFsdQueryAlternateNameInfo (
	IN PFILE_OBJECT pFileObject,
	IN OUT PFILE_NAME_INFORMATION FileNameInformation,
	IN OUT ULONG *Length
    );

NTSTATUS
XixFsdSetBasicInformation(
	IN PXIFS_IRPCONTEXT	pIrpContext,
	IN PFILE_OBJECT		pFileObject,
	IN PXIFS_FCB		pFCB,
	IN PXIFS_CCB		pCCB,
	IN PFILE_BASIC_INFORMATION pBasicInfomation,
	IN uint32				Length
);

BOOLEAN
IsDirectoryEmpty(
	IN PXIFS_FCB pFCB
);


NTSTATUS
XixFsdSetDispositionInformation(
	IN	PXIFS_IRPCONTEXT	pIrpContext,
	IN	PFILE_OBJECT		pFileObject,
	IN	PXIFS_FCB		pFCB,
	IN	PXIFS_CCB		pCCB,
	IN	PFILE_DISPOSITION_INFORMATION pDispositionInfomation,
	IN	uint32			Length
);


NTSTATUS
XixFsdSetPositionInformation(
	IN	PXIFS_IRPCONTEXT	pIrpContext,
	IN	PFILE_OBJECT		pFileObject,
	IN	PXIFS_FCB		pFCB,
	IN	PXIFS_CCB		pCCB,
	IN	PFILE_POSITION_INFORMATION pPositionInfomation,
	IN	uint32			Length
);


VOID
XixFsdGetTargetInfo2(
	PWCHAR Name, 
	uint32	NameLength,
	PUNICODE_STRING NewLinkName
);


VOID
XixFsdGetTargetInfo(
	IN PFILE_OBJECT			TargetFileObject, 
	IN PUNICODE_STRING 		NewLinkName
);


NTSTATUS
UpdateParentChild(
	IN	PXIFS_IRPCONTEXT			pIrpContext,
	IN	PXIFS_FCB					ParentFCB,
	IN	PUNICODE_STRING				pTargetName, 
	IN	PUNICODE_STRING				pOrgFileName
);


NTSTATUS
DeleteParentChild(
	IN	PXIFS_IRPCONTEXT			pIrpContext,
	IN	PXIFS_FCB					ParentFCB, 
	IN	PUNICODE_STRING				pOrgFileName
);


NTSTATUS
AddParentChild(
	IN	PXIFS_IRPCONTEXT			pIrpContext,
	IN	PXIFS_FCB					ParentFCB,
	IN	uint64						ChildLotNumber,
	IN	uint32						ChildType,
	IN	PUNICODE_STRING				ChildName
);

NTSTATUS
UpdateFileName(
	IN	PXIFS_IRPCONTEXT			pIrpContext, 
	IN	PXIFS_FCB					pFCB,
	IN	uint64						ParentLotNumber,
	IN	uint8						*NewFileName, 
	IN	uint32						NewFileLength
);


NTSTATUS
XixFsdSetRenameInformation(
	IN	PXIFS_IRPCONTEXT			pIrpContext,
	IN	PFILE_OBJECT				pFileObject,
	IN	PXIFS_FCB					pFCB,
	IN	PXIFS_CCB					pCCB,
	IN	PFILE_RENAME_INFORMATION 	pRenameInfomation,
	IN	uint32						Length
);

NTSTATUS
XixFsdSetLinkInfo(
	IN	PXIFS_IRPCONTEXT	pIrpContext,
	IN	PFILE_OBJECT		pFileObject,
	IN	PXIFS_FCB			pFCB,
	IN	PXIFS_CCB			pCCB,
	IN	PFILE_LINK_INFORMATION pLinkInfomation,
	IN	uint32				Length
);


NTSTATUS
XixFsdSetAllocationSize(	
	IN	PXIFS_IRPCONTEXT	pIrpContext,
	IN	PFILE_OBJECT		pFileObject,
	IN	PXIFS_FCB		pFCB,
	IN	PXIFS_CCB		pCCB,
	IN 	PFILE_ALLOCATION_INFORMATION pAllocationInformation,
	IN	uint32			Length
	);

NTSTATUS
XixFsdSetEndofFileInfo(	
	IN	PXIFS_IRPCONTEXT	pIrpContext,
	IN	PFILE_OBJECT		pFileObject,
	IN	PXIFS_FCB		pFCB,
	IN	PXIFS_CCB		pCCB,
	IN	PFILE_END_OF_FILE_INFORMATION	pEndOfFileInformation,
	IN	uint32							Length
	);
















#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsdQueryBasicInfo)
#pragma alloc_text(PAGE, XixFsdQueryStandardInfo)
#pragma alloc_text(PAGE, XixFsdQueryInternalInfo)
#pragma alloc_text(PAGE, XixFsdQueryEaInfo)
#pragma alloc_text(PAGE, XixFsdQueryAccessInfo)
#pragma alloc_text(PAGE, XixFsdQueryPositionInfo)
#pragma alloc_text(PAGE, XixFsdQueryModeInfo)
#pragma alloc_text(PAGE, XixFsdQueryAlignInfo)
#pragma alloc_text(PAGE, XixFsdQueryNameInfo)
#pragma alloc_text(PAGE, XixFsdQueryAlternateNameInfo)
#pragma alloc_text(PAGE, XixFsdQueryNetworkInfo)
#pragma alloc_text(PAGE, XixFsdCommonQueryInformation)
#pragma alloc_text(PAGE, XixFsdSetBasicInformation)
#pragma alloc_text(PAGE, IsDirectoryEmpty)
#pragma alloc_text(PAGE, XixFsdSetDispositionInformation)
#pragma alloc_text(PAGE, XixFsdSetPositionInformation)
#pragma alloc_text(PAGE, XixFsdGetTargetInfo2)
#pragma alloc_text(PAGE, XixFsdGetTargetInfo)
#pragma alloc_text(PAGE, UpdateParentChild) 
#pragma alloc_text(PAGE, DeleteParentChild)
#pragma alloc_text(PAGE, AddParentChild)
#pragma alloc_text(PAGE, UpdateFileName)
#pragma alloc_text(PAGE, XixFsdSetRenameInformation)
#pragma alloc_text(PAGE, XixFsdSetLinkInfo)
#pragma alloc_text(PAGE, XixFsdSetAllocationSize)
#pragma alloc_text(PAGE, XixFsdSetEndofFileInfo)
#pragma alloc_text(PAGE, XixFsdCommonSetInformation)
#endif


NTSTATUS
XixFsdQueryBasicInfo( 
	IN PXIFS_FCB 		pFCB, 
	IN PXIFS_CCB		pCCB, 
	IN OUT PFILE_BASIC_INFORMATION BasicInformation, 
	IN OUT ULONG *Length 
)
{
   	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter XixFsdQueryBasicInfo\n"));

	if(*Length < sizeof(FILE_BASIC_INFORMATION)) return STATUS_BUFFER_OVERFLOW;


	BasicInformation->CreationTime.QuadPart = pFCB->CreationTime;
	BasicInformation->LastWriteTime.QuadPart = 
	BasicInformation->ChangeTime.QuadPart = pFCB->LastWriteTime;
	BasicInformation->LastAccessTime.QuadPart= pFCB->LastAccessTime;
	BasicInformation->FileAttributes = pFCB->FileAttribute;

	XifsdClearFlag( BasicInformation->FileAttributes,
			   (~FILE_ATTRIBUTE_VALID_FLAGS |
				FILE_ATTRIBUTE_COMPRESSED |
				FILE_ATTRIBUTE_TEMPORARY |
				FILE_ATTRIBUTE_SPARSE_FILE |
				FILE_ATTRIBUTE_ENCRYPTED) );

	if(BasicInformation->FileAttributes == 0){
		XifsdSetFlag(BasicInformation->FileAttributes, FILE_ATTRIBUTE_NORMAL);
	}


	*Length -= sizeof(FILE_BASIC_INFORMATION);
	
	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("[FILE_BASIC_INFORMATION] size(%ld) Length(%ld)"
		"XifsdQueryBasicInfo FileName(%wZ)\nCTime(%I64d)\nFileAttribute(%x)\n", 
		sizeof(FILE_BASIC_INFORMATION), *Length,
		&pFCB->FCBName, pFCB->CreationTime, pFCB->FileAttribute));	


	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit XixFsdQueryBasicInfo\n"));

	return STATUS_SUCCESS;
	
}


NTSTATUS
XixFsdQueryStandardInfo(
	IN PXIFS_FCB 		pFCB, 
	IN PXIFS_CCB		pCCB, 
	IN OUT PFILE_STANDARD_INFORMATION StandardInformation, 
	IN OUT ULONG *Length 
)
{
   	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter XixFsdQueryStandardInfo\n"));

	if(*Length < sizeof(FILE_STANDARD_INFORMATION)) return STATUS_BUFFER_OVERFLOW;


	StandardInformation->NumberOfLinks = pFCB->LinkCount;
	StandardInformation->DeletePending = XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE);
	if(XifsdCheckFlagBoolean(pFCB->FileAttribute, FILE_ATTRIBUTE_DIRECTORY)){
		StandardInformation->AllocationSize.QuadPart = StandardInformation->EndOfFile.QuadPart = 0;
		StandardInformation->Directory = TRUE;
	}else{
		StandardInformation->AllocationSize.QuadPart = pFCB->AllocationSize.QuadPart;
		StandardInformation->EndOfFile.QuadPart = pFCB->FileSize.QuadPart;
		StandardInformation->Directory = FALSE;
	}
	*Length -= sizeof(FILE_STANDARD_INFORMATION);
	
	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("FILE_STANDARD_INFORMATION size(%ld) Length(%ld)"
		"XifsdQueryStandardInfo FileName(%wZ)\nNumberOfLinks(%ld)\nEndOfFile(%I64d)\nAllocationSize(%I64d)\n", 
		sizeof(FILE_STANDARD_INFORMATION), *Length,
		&pFCB->FCBName, StandardInformation->NumberOfLinks, 
		StandardInformation->EndOfFile.QuadPart,StandardInformation->EndOfFile.QuadPart ));


	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit XixFsdQueryStandardInfo\n"));

	return STATUS_SUCCESS;   
}


NTSTATUS
XixFsdQueryInternalInfo (
	IN PXIFS_FCB 		pFCB, 
	IN PXIFS_CCB		pCCB, 
    	IN OUT PFILE_INTERNAL_INFORMATION FileInternalInformation,
    	IN OUT ULONG *Length 
    )
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter XixFsdQueryInternalInfo\n"));

	if(*Length < sizeof(FILE_INTERNAL_INFORMATION)) return STATUS_BUFFER_OVERFLOW;
	
	FileInternalInformation->IndexNumber.QuadPart = pFCB->FileId;
	
	*Length -= sizeof( FILE_INTERNAL_INFORMATION );

	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("FILE_INTERNAL_INFORMATION size(%ld) Length(%ld)" 
		"XifsdQueryInternalInfo FileName(%wZ)FileId(%I64d)\n", 
		sizeof( FILE_INTERNAL_INFORMATION ), *Length,
		&pFCB->FCBName, FileInternalInformation->IndexNumber.QuadPart ));

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit XixFsdQueryInternalInfo\n"));
	return STATUS_SUCCESS;   
}

NTSTATUS
XixFsdQueryEaInfo (
	IN PXIFS_FCB 		pFCB, 
	IN PXIFS_CCB		pCCB, 
    	IN OUT PFILE_EA_INFORMATION FileEaInformation,
    	IN OUT ULONG *Length 
    )
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter XixFsdQueryEaInfo\n"));
	
	if(*Length < sizeof(FILE_EA_INFORMATION)) return STATUS_BUFFER_OVERFLOW;
	FileEaInformation->EaSize = 0;
	*Length -= sizeof( FILE_EA_INFORMATION );

	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB), 
		("FILE_EA_INFORMATION size(%ld) Length(%ld)\n", sizeof( FILE_EA_INFORMATION ), *Length));

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit XixFsdQueryEaInfo\n"));
	return STATUS_SUCCESS;   
}


NTSTATUS
XixFsdQueryAccessInfo (
	IN PXIFS_FCB 		pFCB, 
	IN PXIFS_CCB		pCCB, 
    	IN OUT PFILE_ACCESS_INFORMATION FileAccessInformation,
    	IN OUT ULONG *Length 
    )
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter XixFsdQueryAccessInfo\n"));

	if(*Length < sizeof(FILE_ACCESS_INFORMATION)) return STATUS_BUFFER_OVERFLOW;
	FileAccessInformation->AccessFlags= pFCB->DesiredAccess;
	*Length -= sizeof( FILE_ACCESS_INFORMATION );

	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("FILE_ACCESS_INFORMATION size(%ld) Length(%ld)\n", sizeof( FILE_ACCESS_INFORMATION ), *Length));

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit XixFsdQueryAccessInfo\n"));
	return STATUS_SUCCESS;   
}


NTSTATUS
XixFsdQueryPositionInfo (
	IN PFILE_OBJECT		pFileObject, 
    	IN OUT PFILE_POSITION_INFORMATION FilePositionInformation,
    	IN OUT ULONG *Length 
    )
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter XixFsdQueryPositionInfo\n"));

	if(*Length < sizeof(FILE_POSITION_INFORMATION)) return STATUS_BUFFER_OVERFLOW;
	FilePositionInformation->CurrentByteOffset = pFileObject->CurrentByteOffset;
	*Length -= sizeof( FILE_POSITION_INFORMATION );

	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("FILE_POSITION_INFORMATION size(%ld) Length(%ld)"
		"XixFsdQueryPositionInfo  CurrentByteOffset(%I64d)\n", 
		sizeof( FILE_POSITION_INFORMATION ), *Length,
		FilePositionInformation->CurrentByteOffset.QuadPart ));


	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit XixFsdQueryPositionInfo\n"));
	return STATUS_SUCCESS;   
}

NTSTATUS
XixFsdQueryModeInfo (
	IN PXIFS_FCB 		pFCB, 
	IN PXIFS_CCB		pCCB, 
    	IN OUT PFILE_MODE_INFORMATION FileModeInformation,
    	IN OUT ULONG *Length 
    )
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter XixFsdQueryModeInfo\n"));

	if(*Length < sizeof(FILE_MODE_INFORMATION)) return STATUS_BUFFER_OVERFLOW;
	FileModeInformation->Mode= 0;
	*Length -= sizeof( FILE_MODE_INFORMATION );

	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("FILE_MODE_INFORMATION size(%ld) Length(%ld)\n", sizeof( FILE_MODE_INFORMATION ), *Length));

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit XixFsdQueryModeInfo\n"));

	return STATUS_SUCCESS;   
}


NTSTATUS
XixFsdQueryAlignInfo (
	IN PXIFS_FCB 		pFCB, 
	IN PXIFS_CCB		pCCB, 
    	IN OUT PFILE_ALIGNMENT_INFORMATION FileAlignInformation,
    	IN OUT ULONG *Length 
    )
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter XixFsdQueryAlignInfo\n"));
	
	if(*Length < sizeof(FILE_ALIGNMENT_INFORMATION)) return STATUS_BUFFER_OVERFLOW;
	FileAlignInformation->AlignmentRequirement = 0;
	*Length -= sizeof( FILE_ALIGNMENT_INFORMATION );

	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB), 
		("FILE_ALIGNMENT_INFORMATION size(%ld) Length(%ld)\n", sizeof( FILE_ALIGNMENT_INFORMATION ), *Length));

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter XixFsdQueryAlignInfo\n"));
	return STATUS_SUCCESS;   
}



NTSTATUS
XixFsdQueryNameInfo (
	IN PFILE_OBJECT pFileObject,
	IN OUT PFILE_NAME_INFORMATION FileNameInformation,
	IN OUT PULONG Length
    )
{
	ULONG LengthToCopy;
	NTSTATUS RC = STATUS_SUCCESS;


	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter XixFsdQueryNameInfo\n"));

	
	if(*Length < sizeof(FILE_NAME_INFORMATION)) return STATUS_BUFFER_OVERFLOW;


 	FileNameInformation->FileNameLength = LengthToCopy = pFileObject->FileName.Length;
	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
	   ("XixFsdQueryNameInfo Name (%wZ) Length (%ld).\n", &pFileObject->FileName, pFileObject->FileName.Length));


	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("FILE_NAME_INFORMATION size(%ld) Length(%ld)\n", sizeof( FILE_NAME_INFORMATION ), *Length));

   	*Length -= (FIELD_OFFSET( FILE_NAME_INFORMATION,FileName[0]));



	if (LengthToCopy > *Length) {
		DebugTrace(DEBUG_LEVEL_CRITICAL,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		  ("XixFsdQueryNameInfo RemainLength (%ld) RequiredLength (%ld).\n", *Length, LengthToCopy));

	    LengthToCopy = *Length;
	    RC = STATUS_BUFFER_OVERFLOW;
	}


 	RtlCopyMemory( FileNameInformation->FileName, pFileObject->FileName.Buffer, LengthToCopy );

	
	*Length -= LengthToCopy;

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit XixFsdQueryNameInfo\n"));
	return RC;
	
}

NTSTATUS
XixFsdQueryAlternateNameInfo (
	IN PFILE_OBJECT pFileObject,
	IN OUT PFILE_NAME_INFORMATION FileNameInformation,
	IN OUT ULONG *Length
    )
{
	ULONG LengthToCopy;
	NTSTATUS RC = STATUS_SUCCESS;


	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter XixFsdQueryAlternateNameInfo\n"));

	if(*Length < sizeof(FILE_NAME_INFORMATION)) return STATUS_BUFFER_OVERFLOW;




 	FileNameInformation->FileNameLength = LengthToCopy = pFileObject->FileName.Length;
	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
	   ("XixFsdQueryAlternateNameInfo Name (%wZ) Length (%ld).\n", &pFileObject->FileName, pFileObject->FileName.Length));


	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB), 
		("FILE_NAME_INFORMATION size(%ld) Length(%ld)\n", sizeof( FILE_NAME_INFORMATION ), *Length));

   	*Length -= (FIELD_OFFSET( FILE_NAME_INFORMATION,FileName[0]));



	if (LengthToCopy > *Length) {
		DebugTrace(DEBUG_LEVEL_CRITICAL,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		  ("XixFsdQueryAlternateNameInfo RemainLength (%ld) RequiredLength (%ld).\n", *Length, LengthToCopy));

	    LengthToCopy = *Length;
	    RC = STATUS_BUFFER_OVERFLOW;
	}


 	RtlCopyMemory( FileNameInformation->FileName, pFileObject->FileName.Buffer, LengthToCopy );

	
	*Length -= LengthToCopy;

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit XixFsdQueryAlternateNameInfo\n"));
	return RC;
}


NTSTATUS
XixFsdQueryNetworkInfo (
	IN PXIFS_FCB 		pFCB, 
	IN PXIFS_CCB		pCCB, 
	IN OUT PFILE_NETWORK_OPEN_INFORMATION FileNetworkInformation,
	IN OUT PULONG Length
    )
{
	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter XixFsdQueryNetworkInfo\n"));

	if(*Length < sizeof(FILE_NETWORK_OPEN_INFORMATION)) return STATUS_BUFFER_OVERFLOW;
	
	
	FileNetworkInformation->CreationTime.QuadPart =  pFCB->CreationTime;
	FileNetworkInformation->LastAccessTime.QuadPart =
		FileNetworkInformation->LastAccessTime.QuadPart = pFCB->LastWriteTime;
	FileNetworkInformation->ChangeTime.QuadPart = pFCB->LastWriteTime;
	FileNetworkInformation->LastAccessTime.QuadPart = pFCB->LastAccessTime;
	FileNetworkInformation->FileAttributes =  pFCB->FileAttribute;
		
	if(XifsdCheckFlagBoolean(pFCB->FileAttribute, FILE_ATTRIBUTE_DIRECTORY)){
		FileNetworkInformation->AllocationSize.QuadPart =FileNetworkInformation->EndOfFile.QuadPart = 0;
	}else{
		FileNetworkInformation->AllocationSize.QuadPart = pFCB->AllocationSize.QuadPart;
		FileNetworkInformation->EndOfFile.QuadPart = pFCB->FileSize.QuadPart;
	}
	
	*Length -= sizeof(FILE_NETWORK_OPEN_INFORMATION);
	
	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("XixFsdQueryNetworkInfo FileName(%wZ)\nCTime(%I64d)\nFileAttribute(%x)\n",
		&pFCB->FCBName, pFCB->CreationTime, pFCB->FileAttribute));


	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit XixFsdQueryNetworkInfoo\n"));
	return STATUS_SUCCESS;   
}




NTSTATUS
XixFsdCommonQueryInformation(
	IN PXIFS_IRPCONTEXT pIrpContext
	)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PIRP		pIrp = NULL;
    PIO_STACK_LOCATION pIrpSp = NULL;

  	LONG Length = 0;
    FILE_INFORMATION_CLASS FileInformationClass;
    PVOID pBuffer = NULL;
    	
   	PFILE_OBJECT pFileObject = NULL;
   	TYPE_OF_OPEN TypeOfOpen = UnopenedFileObject;

	PXIFS_FCB	pFCB = NULL;
	PXIFS_CCB	pCCB = NULL;

	BOOLEAN					MainResourceAcquired = FALSE;
	BOOLEAN					CanWait = FALSE;
	BOOLEAN					ReleaseFCB = FALSE;				

	uint32					ReturnedBytes = 0;


	ASSERT_IRPCONTEXT(pIrpContext);
	ASSERT(pIrpContext->Irp);


	PAGED_CODE();
	
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter XixFsdCommonQueryInformation\n"));

	
	pIrp = pIrpContext->Irp;
	pIrpSp = IoGetCurrentIrpStackLocation( pIrp );

	Length = pIrpSp->Parameters.QueryFile.Length;
    FileInformationClass = pIrpSp->Parameters.QueryFile.FileInformationClass;
    pBuffer = pIrp->AssociatedIrp.SystemBuffer;
	pFileObject = pIrpSp->FileObject;

	


	TypeOfOpen = XixFsdDecodeFileObject(pFileObject, &pFCB, &pCCB);


	if(TypeOfOpen <= UserVolumeOpen){
		ReturnedBytes = 0;
		RC = STATUS_INVALID_PARAMETER;
		XixFsdCompleteRequest(pIrpContext, RC, ReturnedBytes );
		return RC;

	}

	CanWait = XifsdCheckFlagBoolean(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);

	if(!XifsdAcquireFcbShared(CanWait, pFCB, FALSE)){
		DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_IRPCONTEXT), 
					("PostRequest IrpContext(%p) Irp(%p)\n",pIrpContext, pIrp));

		RC = XixFsdPostRequest(pIrpContext, pIrp);
		return RC;

	}

	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB), 
			("FileInformationClass(%ld) Length(%ld) .\n", FileInformationClass, Length));
	

	RtlZeroMemory(pBuffer, Length);

	try{


		if((TypeOfOpen == UserDirectoryOpen) || (TypeOfOpen == UserFileOpen))
		{
	
		DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!QueryInformation (%wZ) pCCB(%p)\n", &pFCB->FCBName, pCCB));

			XixFsdVerifyFcbOperation( pIrpContext, pFCB );

			switch(FileInformationClass){
				case FileAllInformation:
				{
					DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB), 
							("FileAllInformation Length(%ld) .\n", sizeof(FILE_ALL_INFORMATION)));

					if(XifsdCheckFlagBoolean(pCCB->CCBFlags, XIFSD_CCB_OPENED_BY_FILEID)){
						RC = STATUS_INVALID_PARAMETER;
						break;
					}
				
					/*
					Length -= (sizeof( FILE_ACCESS_INFORMATION ) +
					           sizeof( FILE_MODE_INFORMATION ) +
					           sizeof( FILE_ALIGNMENT_INFORMATION ));

typedef struct _FILE_ALL_INFORMATION {
    FILE_BASIC_INFORMATION BasicInformation;
    FILE_STANDARD_INFORMATION StandardInformation;
    FILE_INTERNAL_INFORMATION InternalInformation;
    FILE_EA_INFORMATION EaInformation;
    FILE_ACCESS_INFORMATION AccessInformation;
    FILE_POSITION_INFORMATION PositionInformation;
    FILE_MODE_INFORMATION ModeInformation;
    FILE_ALIGNMENT_INFORMATION AlignmentInformation;
    FILE_NAME_INFORMATION NameInformation;
} FILE_ALL_INFORMATION, *PFILE_ALL_INFORMATION;


					*/

					RC = XixFsdQueryBasicInfo( pFCB, pCCB, &((PFILE_ALL_INFORMATION)(pBuffer))->BasicInformation, &Length );
					if(!NT_SUCCESS(RC)){
						break;
					}

					RC = XixFsdQueryStandardInfo(pFCB, pCCB, &((PFILE_ALL_INFORMATION)(pBuffer))->StandardInformation, &Length);
					if(!NT_SUCCESS(RC)){
						break;
					}
					
					RC = XixFsdQueryInternalInfo( pFCB, pCCB, &((PFILE_ALL_INFORMATION)(pBuffer))->InternalInformation, &Length );
					if(!NT_SUCCESS(RC)){
						break;
					}

					RC = XixFsdQueryEaInfo(  pFCB, pCCB,  &((PFILE_ALL_INFORMATION)(pBuffer))->EaInformation, &Length );
					if(!NT_SUCCESS(RC)){
						break;
					}


					RC = XixFsdQueryAccessInfo(pFCB, pCCB,  &((PFILE_ALL_INFORMATION)(pBuffer))->AccessInformation, &Length );
					if(!NT_SUCCESS(RC)){
						break;
					}


					RC = XixFsdQueryPositionInfo(pFileObject, &((PFILE_ALL_INFORMATION)(pBuffer))->PositionInformation, &Length );
					if(!NT_SUCCESS(RC)){
						break;
					}


					RC = XixFsdQueryModeInfo(pFCB, pCCB, &((PFILE_ALL_INFORMATION)(pBuffer))->ModeInformation, &Length);
					if(!NT_SUCCESS(RC)){
						break;
					}
					

					RC = XixFsdQueryAlignInfo(pFCB, pCCB, &((PFILE_ALL_INFORMATION)(pBuffer))->AlignmentInformation, &Length);
					if(!NT_SUCCESS(RC)){
						break;
					}

					RC = XixFsdQueryNameInfo(  pFileObject, &((PFILE_ALL_INFORMATION)(pBuffer))->NameInformation, &Length );					
					if(!NT_SUCCESS(RC)){
						break;
					}
					
				}break;
				case FileBasicInformation:
				{
					RC = XixFsdQueryBasicInfo(
									pFCB, 
									pCCB, 
									(PFILE_BASIC_INFORMATION)pBuffer, 
									&Length);

					if(!NT_SUCCESS(RC)){
						break;
					}
				}break;	
				case FileStandardInformation:
				{
					RC = XixFsdQueryStandardInfo(
									pFCB, 
									pCCB, 
									(PFILE_STANDARD_INFORMATION)pBuffer, 
									&Length);
					
					if(!NT_SUCCESS(RC)){
						break;
					}
				}break;		
				case FileInternalInformation:
				{
					RC = XixFsdQueryInternalInfo(
									pFCB, 
									pCCB, 
									(PFILE_INTERNAL_INFORMATION)pBuffer, 
									&Length);
					
					if(!NT_SUCCESS(RC)){
						break;
					}
				}break;		
				case FileEaInformation:
				{
					RC = XixFsdQueryEaInfo (
									pFCB, 
									pCCB, 
									(PFILE_EA_INFORMATION)pBuffer, 
									&Length);

					if(!NT_SUCCESS(RC)){
						break;
					}
					    				
				}break;		
				case FilePositionInformation:
				{
					RC = XixFsdQueryPositionInfo(
									pFileObject,
									(PFILE_POSITION_INFORMATION )pBuffer, 
									&Length);

					if(!NT_SUCCESS(RC)){
						break;
					}
									
				}break;		
				case FileNameInformation:
				{


					if(XifsdCheckFlagBoolean(pCCB->CCBFlags, XIFSD_CCB_OPENED_BY_FILEID)){
						RC = STATUS_INVALID_PARAMETER;
						break;
					}

					RC = XixFsdQueryNameInfo(
									pFileObject,
									(PFILE_NAME_INFORMATION )pBuffer, 
									&Length);

					if(!NT_SUCCESS(RC)){
						break;
					}
				}break;		
				case FileAlternateNameInformation:
				{
					if(XifsdCheckFlagBoolean(pCCB->CCBFlags, XIFSD_CCB_OPENED_BY_FILEID)){
						RC = STATUS_INVALID_PARAMETER;
						break;
					}

					RC = XixFsdQueryAlternateNameInfo (
									pFileObject,
									(PFILE_NAME_INFORMATION)pBuffer, 
									&Length);

					if(!NT_SUCCESS(RC)){
						break;
					}
				}break;		
				case FileNetworkOpenInformation:
				{
					RC = XixFsdQueryNetworkInfo (
									pFCB, 
									pCCB, 
									(PFILE_NETWORK_OPEN_INFORMATION)pBuffer, 
									&Length);

					if(!NT_SUCCESS(RC)){
						break;
					}
				}break;		
				default:
					ReturnedBytes = 0;
					RC = STATUS_INVALID_PARAMETER;
					break;
			}



		} else {
			ReturnedBytes = 0;
			RC = STATUS_INVALID_PARAMETER;
		}

		ReturnedBytes = (uint32)pIrp->IoStatus.Information = (uint32)pIrpSp->Parameters.QueryFile.Length - Length;

	}finally{

		XifsdReleaseFcb(CanWait, pFCB);
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit (0x%x) XifsdCommonQueryInformation Information(%ld) .\n", RC, ReturnedBytes));

	XixFsdCompleteRequest(pIrpContext, RC, ReturnedBytes );
	return RC;

    	
}







NTSTATUS
XixFsdSetBasicInformation(
	IN PXIFS_IRPCONTEXT	pIrpContext,
	IN PFILE_OBJECT		pFileObject,
	IN PXIFS_FCB		pFCB,
	IN PXIFS_CCB		pCCB,
	IN PFILE_BASIC_INFORMATION pBasicInfomation,
	IN uint32				Length
)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	uint32		TempFlags = 0;
	uint32		NotifyFilter = 0;


	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter XixFsdSetBasicInformation.\n"));

	if(Length < sizeof(FILE_BASIC_INFORMATION)){
		return STATUS_INVALID_PARAMETER;
	}

	// Added by ILGU HONG for readonly 09052006
	if(pFCB->PtrVCB->IsVolumeWriteProctected){
		return STATUS_MEDIA_WRITE_PROTECTED;
	}
	// Added by ILGU HONG for readonly end


	XifsdLockFcb(pIrpContext, pFCB);

	try{
		if (pBasicInfomation->ChangeTime.QuadPart == -1) {
		
			pBasicInfomation->ChangeTime.QuadPart = 0;
		}

		if (pBasicInfomation->LastAccessTime.QuadPart == -1) {


			pBasicInfomation->LastAccessTime.QuadPart = 0;
		}

		if (pBasicInfomation->LastWriteTime.QuadPart == -1) {


			pBasicInfomation->LastWriteTime.QuadPart = 0;
		}

		if (pBasicInfomation->CreationTime.QuadPart == -1) {
	
			pBasicInfomation->CreationTime.QuadPart = 0;
		}

		if(pBasicInfomation->FileAttributes != 0){

			if(pFCB->FCBType == FCB_TYPE_FILE){
				if(XifsdCheckFlagBoolean(pBasicInfomation->FileAttributes, FILE_ATTRIBUTE_DIRECTORY)){
					RC = STATUS_INVALID_PARAMETER;
					try_return(RC);
				}
			}else{
				if(XifsdCheckFlagBoolean(pBasicInfomation->FileAttributes, FILE_ATTRIBUTE_TEMPORARY)){
					RC =STATUS_INVALID_PARAMETER;
					try_return(RC);
				}
			}

			XifsdClearFlag(pBasicInfomation->FileAttributes, ~FILE_ATTRIBUTE_VALID_SET_FLAGS);
			XifsdClearFlag(pBasicInfomation->FileAttributes, FILE_ATTRIBUTE_NORMAL);
			
			
			XifsdLockFcb(pIrpContext, pFCB);
			pFCB->FileAttribute = ((pFCB->FileAttribute & ~FILE_ATTRIBUTE_VALID_SET_FLAGS)
													| pBasicInfomation->FileAttributes);

			NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;

			if (XifsdCheckFlagBoolean(pFCB->Type ,XIFS_FD_TYPE_ROOT_DIRECTORY)) {
				XifsdSetFlag(pFCB->FileAttribute, (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN));

			}

			if( XifsdCheckFlagBoolean(pFCB->FileAttribute, FILE_ATTRIBUTE_TEMPORARY)){
				XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_OPEN_TEMPORARY);
				XifsdSetFlag(pFileObject->Flags, FO_TEMPORARY_FILE);
			}else{
				XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_OPEN_TEMPORARY);
				XifsdClearFlag(pFileObject->Flags, FO_TEMPORARY_FILE);
			}
			
			XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_ATTR);
			XifsdSetFlag(pFileObject->Flags, FO_FILE_MODIFIED);

			XifsdUnlockFcb(pIrpContext, pFCB);
			
			

		}

	
		

		if (pBasicInfomation->ChangeTime.QuadPart != 0) {
			XifsdSetFlag( TempFlags, XIFSD_CCB_MODIFY_TIME_SET );
			pFCB->LastWriteTime = pBasicInfomation->ChangeTime.QuadPart;
			XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_TIME);
			NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
			
		}

		if (pBasicInfomation->LastAccessTime.QuadPart != 0) {

			XifsdSetFlag( TempFlags, XIFSD_CCB_ACCESS_TIME_SET );
			pFCB->LastAccessTime  = pBasicInfomation->LastAccessTime.QuadPart;
			XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_TIME);
			NotifyFilter |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
		}

		if (pBasicInfomation->LastWriteTime.QuadPart != 0) {

			XifsdSetFlag( TempFlags, XIFSD_CCB_MODIFY_TIME_SET );
			pFCB->LastWriteTime =  pBasicInfomation->LastWriteTime.QuadPart;
			XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_TIME);
			NotifyFilter |= FILE_NOTIFY_CHANGE_LAST_WRITE;
		}

		if (pBasicInfomation->CreationTime.QuadPart != 0) {
			XifsdSetFlag( TempFlags, XIFSD_CCB_CREATE_TIME_SET );	
			pFCB->CreationTime = pBasicInfomation->CreationTime.QuadPart;
			XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_TIME);
			NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
		}

		if(TempFlags != 0){
			XifsdSetFlag(pCCB->CCBFlags, TempFlags);
			XifsdSetFlag(pFileObject->Flags, FO_FILE_MODIFIED);
		}
	


		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_ALL,
			("FCB flags (0x%x) CCB flags(0x%x)!!!!\n", pFCB->FCBFlags, pCCB->CCBFlags));

		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_ALL,
			("FCB Attributes (0x%x)\n", pFCB->FileAttribute));

	}finally{
		XifsdUnlockFcb(pIrpContext,pFCB);
	}


	if(NT_SUCCESS(RC)){
		if(pFCB->FCBType == FCB_TYPE_DIR){
			XixFsdUpdateFCB(pFCB);
		}
		
	}

	XixFsdNotifyReportChange(
					pFCB,
					NotifyFilter,
					FILE_ACTION_MODIFIED
					);



	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit XixFsdSetBasicInformation.\n"));

	return RC;
}

BOOLEAN
IsDirectoryEmpty(IN PXIFS_FCB pFCB)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter IsDirectoryEmpty.\n"));

	ASSERT(pFCB->FCBType == FCB_TYPE_DIR);
	
	if(pFCB->ChildCount == 0) {
		
		DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
			("Exit IsDirectoryEmpty Empty Dir.\n"));
		return TRUE;
	}
	else {
		DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
			("Exit IsDirectoryEmpty Has children.\n"));
		return FALSE;
	}
}	


NTSTATUS
XixFsdSetDispositionInformation(
	IN	PXIFS_IRPCONTEXT	pIrpContext,
	IN	PFILE_OBJECT		pFileObject,
	IN	PXIFS_FCB		pFCB,
	IN	PXIFS_CCB		pCCB,
	IN	PFILE_DISPOSITION_INFORMATION pDispositionInfomation,
	IN	uint32			Length
)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	PXIFS_LCB	pLCB = NULL;
	PXIFS_VCB	pVCB = NULL;


	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter XixFsdSetDispositionInformation pFCB(%I64d).\n", pFCB->LotNumber));

	
	if(Length < sizeof(FILE_DISPOSITION_INFORMATION)) {
		return STATUS_INVALID_PARAMETER;
	}
	
	ASSERT_FCB(pFCB);
	ASSERT_CCB(pCCB);

	pLCB = pCCB->PtrLCB;

	if(pLCB == NULL)
	{
		return STATUS_INVALID_PARAMETER;
	}

	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	// Added by ILGU HONG for readonly 09052006
	if(pVCB->IsVolumeWriteProctected){
		return STATUS_MEDIA_WRITE_PROTECTED;
	}
	// Added by ILGU HONG for readonly end


	XifsdAcquireFcbExclusive(TRUE,pLCB->ParentFcb, FALSE);
	XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);

	try{
		if(pDispositionInfomation->DeleteFile){

			if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("XixFsdSetDispositionInformation pFCB(%I64d) is alread set CLOSE.\n", pFCB->LotNumber));
				try_return(RC);
			}

			if(pFCB->FCBType == FCB_TYPE_FILE ){
				if(pFCB->HasLock != FCB_FILE_LOCK_HAS){

					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("XixFsdSetDispositionInformation pFCB(%I64d) is Has not Write Permission.\n", pFCB->LotNumber));

					RC = XixFsLotLock(TRUE, pVCB, pVCB->TargetDeviceObject, pFCB->LotNumber, &pFCB->HasLock, TRUE, FALSE);
						
					if(!NT_SUCCESS(RC)){
					
						try_return(RC = STATUS_CANNOT_DELETE);
					}

					//DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					//	("XifsdSetDispositionInformation pFCB(%I64d) is Has not Write Permission.\n", pFCB->LotNumber));
					//try_return(RC = STATUS_CANNOT_DELETE);
				}
			}
			
			/*
			if(!XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_OPEN_WRITE)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL
					("XifsdSetDispositionInformation pFCB(%I64d) is Has is not Open for wirte.\n", pFCB->LotNumber));
				try_return(RC = STATUS_CANNOT_DELETE);
			}
			*/

			/*
			if(pFCB->FCBCleanup > 1){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("XifsdSetDispositionInformation pFCB(%I64d) is Has Opened by Other(%ld).\n", pFCB->LotNumber, pFCB->FCBCleanup));
					try_return(RC = STATUS_CANNOT_DELETE);			
			}
			*/

			if(pFCB->FCBType == FCB_TYPE_FILE ){
				if (!MmFlushImageSection(&(pFCB->SectionObject), MmFlushForDelete)) {
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("XifsdSetDispositionInformation pFCB(%I64d) fail MmFlushImageSecton.\n", pFCB->LotNumber));
	
					try_return(RC = STATUS_CANNOT_DELETE);
				}
			}

			if (XifsdCheckFlagBoolean(pFCB->Type,XIFS_FD_TYPE_ROOT_DIRECTORY)) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("XifsdSetDispositionInformation pFCB(%I64d) is failed because it is ROOT DIRECTORY.\n", pFCB->LotNumber));
				try_return(RC = STATUS_CANNOT_DELETE);
			}


/*
			if(XifsdCheckFlagBoolean(pFCB->FileAttribute, FILE_ATTRIBUTE_SYSTEM)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("XifsdSetDispositionInformation pFCB(%I64d) is failed because it  is System file attribute.\n", pFCB->LotNumber));
				try_return(RC = STATUS_CANNOT_DELETE);
			}

			if(XifsdCheckFlagBoolean(pFCB->Type, XIFS_FD_TYPE_SYSTEM_FILE)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("XifsdSetDispositionInformation pFCB(%I64d) is failed because it  is System file type.\n", pFCB->LotNumber));
				try_return(RC = STATUS_CANNOT_DELETE);
			}
*/


			if(pFCB->FCBType == FCB_TYPE_VOLUME){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("XifsdSetDispositionInformation pFCB(%I64d) is failed because it  is for Volume file.\n", pFCB->LotNumber));
				try_return(RC = STATUS_CANNOT_DELETE);
			}

			if(!XifsdCheckFlagBoolean(pLCB->LCBFlags, XIFSD_LCB_STATE_DELETE_ON_CLOSE)){

				if(pFCB->FCBType == FCB_TYPE_DIR){
					if(!IsDirectoryEmpty(pFCB)){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
							("XifsdSetDispositionInformation pFCB(%I64d) Directory is not empty.\n", pFCB->LotNumber));
						try_return(RC = STATUS_CANNOT_DELETE);
					}

					XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE);
					XifsdSetFlag(pFileObject->Flags, FO_FILE_MODIFIED);
					/*
					if(!XifsdCheckFlagBoolean(pCCB->CCBFlags, XIFSD_CCB_OPENED_BY_FILEID)
						&& (pCCB->FullPath.Buffer != NULL))
					{
						FsRtlNotifyFullChangeDirectory(
							pFCB->PtrVCB->NotifyIRPSync,
							&pFCB->PtrVCB->NextNotifyIRP,
							(void *)pCCB,
							NULL,
							FALSE,
							FALSE,
							0,
							NULL,
							NULL,
							NULL
							);
					}
					*/	

					if(!XifsdCheckFlagBoolean(pCCB->CCBFlags, XIFSD_CCB_OPENED_BY_FILEID)
						&& (pFCB->FCBFullPath.Buffer != NULL))
					{
						FsRtlNotifyFullChangeDirectory(
							pFCB->PtrVCB->NotifyIRPSync,
							&pFCB->PtrVCB->NextNotifyIRP,
							(void *)pCCB,
							NULL,
							FALSE,
							FALSE,
							0,
							NULL,
							NULL,
							NULL
							);
					}

				
				}else{
					if(pFCB->LinkCount  > 0){
						XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE);
						XifsdSetFlag(pFileObject->Flags, FO_FILE_MODIFIED);
					}	
				}
				
				XifsdSetFlag(pLCB->LCBFlags, XIFSD_LCB_STATE_DELETE_ON_CLOSE);
				// Delete From Parent Directory Entry //
				//RC = XifsdDeleteEntryFromDir(pIrpContext, pLCB->ParentFcb, pFCB->LotNumber);	
			}else{
				XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE);
				XifsdSetFlag(pLCB->LCBFlags, XIFSD_LCB_STATE_DELETE_ON_CLOSE);
			}
			pFileObject->DeletePending = TRUE;
			//DbgPrint(" !!!Delete Pending Set TRUE (%wZ)  .\n", &pFCB->FCBFullPath);
		}else{
	
			
			if(XifsdCheckFlagBoolean(pLCB->LCBFlags, XIFSD_LCB_STATE_DELETE_ON_CLOSE)){
				XifsdClearFlag(pLCB->LCBFlags, XIFSD_LCB_STATE_DELETE_ON_CLOSE);
			}


			if(XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE)){
				XifsdClearFlag(pFCB->FCBFlags, XIFSD_FCB_DELETE_ON_CLOSE);
			}
			

			pFileObject->DeletePending = FALSE;
			//DbgPrint(" !!!Delete Pending Set FALSE (%wZ)  .\n", &pFCB->FCBFullPath);
		}

		
		
		

	}finally{
		XifsdReleaseFcb(TRUE, pFCB);
		XifsdReleaseFcb(TRUE, pLCB->ParentFcb);
	}

	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit XixFsdSetDispositionInformation.\n"));
	return RC;
}



NTSTATUS
XixFsdSetPositionInformation(
	IN	PXIFS_IRPCONTEXT	pIrpContext,
	IN	PFILE_OBJECT		pFileObject,
	IN	PXIFS_FCB		pFCB,
	IN	PXIFS_CCB		pCCB,
	IN	PFILE_POSITION_INFORMATION pPositionInfomation,
	IN	uint32			Length
)
{

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter XixFsdSetPositionInformation.\n"));

	if(Length < sizeof(FILE_POSITION_INFORMATION)){
		return STATUS_INVALID_PARAMETER;
	}
	


	try{
		XifsdLockFcb(pIrpContext, pFCB);
		pFileObject->CurrentByteOffset = pPositionInfomation->CurrentByteOffset;
		DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("XixFsdSetPositionInformation CurrentByteOffset(%I64ld).\n", pFileObject->CurrentByteOffset.QuadPart));
		XifsdUnlockFcb(pIrpContext, pFCB);
	}finally{
		;
	}

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit XixFsdSetPositionInformation.\n"));
	return STATUS_SUCCESS;
}


VOID
XixFsdGetTargetInfo2(
	PWCHAR Name, 
	uint32	NameLength,
	PUNICODE_STRING NewLinkName
)
{
	/*
	uint32	Index = (NameLength /sizeof(WCHAR));
	uint32	offset = 0;
	uint16	wcharLen = 0;
	uint8	*StartPosition =NULL;


	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter XixFsdGetTargetInfo2.\n"));

	// Back up until we come to the last '\'
	// But first, skip any trailing '\' characters

	while (Name[Index] == L'\\') {
		ASSERT(Index > 0);
		Name[Index] =L'';
		Index --;
	}
	wcharLen = sizeof(WCHAR);

	while (Name[Index] != L'\\') {
		// Keep backing up until we hit one
		ASSERT(Index > 0);
		Index --;
		wcharLen += sizeof(WCHAR);
	}

	// We must be at a '\' character
	ASSERT(Name[Index] == L'\\');

	Index++;
	

	NewLinkName->MaximumLength = 
	NewLinkName->Length = wcharLen;
	offset = Index*sizeof(WCHAR);
	NewLinkName->Buffer = (PWSTR) Add2Ptr(Name, offset);   
	*/
	
	NewLinkName->MaximumLength = 
	NewLinkName->Length = (uint16)NameLength;
	NewLinkName->Buffer = (PWSTR) Name;   
	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB), 
		("NewLinkName from renamebuffer (%wZ)\n",NewLinkName));


	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit XixFsdGetTargetInfo2.\n"));
}



VOID
XixFsdGetTargetInfo(
	IN PFILE_OBJECT			TargetFileObject, 
	IN PUNICODE_STRING 		NewLinkName
)
{
	USHORT LastFileNameOffset = 0;
	
	/*
	uint32 i = 0;
	LastFileNameOffset = 0;
	//
	//  If the first character at the final component is a backslash, move the
	//  offset ahead by 2.
	//
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter XixFsdGetTargetInfo.\n"));
	

	while (TargetFileObject->FileName.Buffer[i] == L'\\') {
		LastFileNameOffset += sizeof( WCHAR );
		i++;
	}	
	*/

	LastFileNameOffset =	(uint16) XixFsdSearchLastComponetOffset(&(TargetFileObject->FileName));

	NewLinkName->MaximumLength = 
	NewLinkName->Length = TargetFileObject->FileName.Length  - LastFileNameOffset;
	NewLinkName->Buffer = (PWSTR) Add2Ptr( TargetFileObject->FileName.Buffer, LastFileNameOffset);   
	

	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("NewLinkName from TagetFileObject (%wZ)\n",NewLinkName));

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit XixFsdGetTargetInfo.\n"));
}



NTSTATUS
IsExistChecChild(
	IN	PXIFS_IRPCONTEXT			pIrpContext,
	IN	PXIFS_FCB					ParentFCB,
	IN	PUNICODE_STRING				pTargetName
)
{
	NTSTATUS 					RC = STATUS_SUCCESS;
	XIFS_DIR_EMUL_CONTEXT		DirContext;
	PXIDISK_CHILD_INFORMATION	pChild = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter CheckParentChild.\n"));	

	XixFsInitializeDirContext(ParentFCB->PtrVCB, pIrpContext, &DirContext);
	

	RC = XixFsFindDirEntry ( pIrpContext, ParentFCB, pTargetName, &DirContext, FALSE);
	
	XixFsCleanupDirContext(pIrpContext, &DirContext);

	return RC;

}




NTSTATUS
UpdateParentChild(
	IN	PXIFS_IRPCONTEXT			pIrpContext,
	IN	PXIFS_FCB					ParentFCB,
	IN	PUNICODE_STRING				pTargetName, 
	IN	PUNICODE_STRING				pOrgFileName)
{
	NTSTATUS 					RC = STATUS_SUCCESS;
	XIFS_DIR_EMUL_CONTEXT		DirContext;
	UNICODE_STRING				NewName;
	PXIDISK_CHILD_INFORMATION	pChild = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter UpdateParentChild.\n"));	

	XixFsInitializeDirContext(ParentFCB->PtrVCB, pIrpContext, &DirContext);
	

	RC = XixFsFindDirEntry ( pIrpContext, ParentFCB, pOrgFileName, &DirContext, FALSE);
	if(!NT_SUCCESS(RC))
	{
		XixFsCleanupDirContext(pIrpContext, &DirContext);
		return STATUS_ACCESS_DENIED;
	}


	pChild = (PXIDISK_CHILD_INFORMATION)DirContext.ChildEntry;
	RC = XixFsUpdateChildFromDir(
							TRUE, 
							ParentFCB->PtrVCB, 
							ParentFCB,  
							pChild->StartLotIndex,
							pChild->Type,
							pChild->State,
							pTargetName,
							pChild->ChildIndex,
							&DirContext);

	XixFsCleanupDirContext(pIrpContext, &DirContext);

	if(!NT_SUCCESS(RC)){
		return RC;		
	}
	

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit UpdateParentChild.\n"));	
	return STATUS_SUCCESS;
}
	

NTSTATUS
DeleteParentChild(
	IN	PXIFS_IRPCONTEXT			pIrpContext,
	IN	PXIFS_FCB					ParentFCB, 
	IN	PUNICODE_STRING				pOrgFileName)
{
	NTSTATUS 					RC = STATUS_SUCCESS;
	XIFS_DIR_EMUL_CONTEXT		DirContext;
	UNICODE_STRING				NewName;
	PXIDISK_CHILD_INFORMATION	pChild = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter DeleteParentChild.\n"));	

	XixFsInitializeDirContext(ParentFCB->PtrVCB, pIrpContext, &DirContext);
	
	RC = XixFsFindDirEntry ( pIrpContext, ParentFCB, pOrgFileName, &DirContext, FALSE);

	if(!NT_SUCCESS(RC))
	{
		XixFsCleanupDirContext(pIrpContext, &DirContext);
		return STATUS_ACCESS_DENIED;
	}


	pChild = (PXIDISK_CHILD_INFORMATION)DirContext.ChildEntry;

	RC = XixFsDeleteChildFromDir(TRUE,
								ParentFCB->PtrVCB,
								ParentFCB, 
								pChild->ChildIndex,
								&DirContext
								);



	XixFsCleanupDirContext(pIrpContext, &DirContext);

	if(!NT_SUCCESS(RC)){
		return RC;		
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit DeleteParentChild.\n"));		
	return STATUS_SUCCESS;
}





	
NTSTATUS
AddParentChild(
	IN	PXIFS_IRPCONTEXT			pIrpContext,
	IN	PXIFS_FCB					ParentFCB,
	IN	uint64						ChildLotNumber,
	IN	uint32						ChildType,
	IN	PUNICODE_STRING				ChildName)
{
	NTSTATUS 					RC = STATUS_SUCCESS;
	XIFS_DIR_EMUL_CONTEXT		DirContext;
	UNICODE_STRING				NewName;
	PXIDISK_CHILD_INFORMATION	pChild = NULL;
	PXIFS_VCB					pVCB = NULL;
	uint64						ChildIndex;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter AddParentChild.\n"));		

	ASSERT_FCB(ParentFCB);
	pVCB = ParentFCB->PtrVCB;
	ASSERT_VCB(pVCB);




	XixFsInitializeDirContext(ParentFCB->PtrVCB, pIrpContext, &DirContext);
	
	XixFsLookupInitialDirEntry( pIrpContext, ParentFCB, &DirContext, 2 );


	RC = XixFsAddChildToDir(
					TRUE,
					pVCB,
					ParentFCB,
					ChildLotNumber,
					ChildType,
					ChildName,
					&DirContext,
					&ChildIndex
					);

	XixFsCleanupDirContext(pIrpContext, &DirContext);

	if(!NT_SUCCESS(RC)){
		return RC;		
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit AddParentChild.\n"));	

	return STATUS_SUCCESS;
}





NTSTATUS
UpdateFileName(
	IN	PXIFS_IRPCONTEXT			pIrpContext, 
	IN	PXIFS_FCB					pFCB,
	IN	uint64						ParentLotNumber,
	IN	uint8						*NewFileName, 
	IN	uint32						NewFileLength)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter UpdateFileName.\n"));	


	ASSERT_FCB(pFCB);


	if(pFCB->FCBName.MaximumLength < NewFileLength){
		ExFreePool(pFCB->FCBName.Buffer);
		pFCB->FCBName.Buffer = ExAllocatePoolWithTag(NonPagedPool, NewFileLength, TAG_FILE_NAME);
		if(pFCB->FCBName.Buffer == NULL) {
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		pFCB->FCBName.MaximumLength = (uint16)NewFileLength;
	}
	RtlZeroMemory(pFCB->FCBName.Buffer, pFCB->FCBName.MaximumLength);
	pFCB->FCBName.Length = (uint16)NewFileLength;
	RtlCopyMemory(pFCB->FCBName.Buffer, NewFileName, pFCB->FCBName.Length);
	
	pFCB->ParentLotNumber = ParentLotNumber;

	XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_NAME);

	DebugTrace(DEBUG_LEVEL_ALL,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_ALL),  
					("New File Name (%wZ) Id(%I64d)\n", &pFCB->FCBName, pFCB->LotNumber));	

	try{
		RC = XixFsdUpdateFCB(pFCB);
	}finally{
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR,DEBUG_TARGET_ALL,  
					("FAIL SET New File Name (%wZ) Id(%I64d)\n", &pFCB->FCBName, pFCB->LotNumber));	
		}
	}


	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit UpdateFileName. Status(0x%x)\n", RC));	
	return RC;

}







NTSTATUS
XixFsdSetRenameInformation(
	IN	PXIFS_IRPCONTEXT			pIrpContext,
	IN	PFILE_OBJECT				pFileObject,
	IN	PXIFS_FCB					pFCB,
	IN	PXIFS_CCB					pCCB,
	IN	PFILE_RENAME_INFORMATION 	pRenameInfomation,
	IN	uint32						Length
)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	PIRP				pIrp = NULL;
	PIO_STACK_LOCATION	pIrpSp = NULL;
	PFILE_OBJECT		TargetFileObject = NULL;
	UNICODE_STRING		TargetFileName;
	PXIFS_LCB			pLCB = NULL;
	PXIFS_VCB			pVCB = NULL;
	PXIFS_FCB			pParentFCB = NULL;
	
	PXIFS_FCB			pTargetParentFCB = NULL;
	PXIFS_CCB			pTargetParentCCB = NULL;
	
	PXIFS_LCB			pTargetLCB = NULL;
	PXIFS_LCB			FountLCB = NULL;

	
	uint64				OldParentLotNumber = 0;
	uint64				NewParentLotNumber = 0;

	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter XixFsdSetRenameInformation.\n"));

	if(Length < sizeof(FILE_RENAME_INFORMATION)){
		return STATUS_INVALID_PARAMETER;
	}

	if(Length < ( sizeof(FILE_RENAME_INFORMATION) + pRenameInfomation->FileNameLength -sizeof(WCHAR))){
		return STATUS_INVALID_PARAMETER;
	}

	
	ASSERT_IRPCONTEXT(pIrpContext);

	ASSERT_FCB(pFCB);

	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	//	added by ILGU HONG for readonly 09052006
	if(pVCB->IsVolumeWriteProctected){
		return STATUS_MEDIA_WRITE_PROTECTED;
	}
	//	added by ILGU HONG for readonly end
	

	ASSERT_CCB(pCCB);
	pLCB = pCCB->PtrLCB;

	if(pLCB == NULL){
		return STATUS_INVALID_PARAMETER;
	}
		
	ASSERT_LCB(pLCB);

	pParentFCB = pLCB->ParentFcb;
	ASSERT_FCB(pParentFCB);


	OldParentLotNumber = pParentFCB->LotNumber;


	if(XifsdCheckFlagBoolean(pLCB->LCBFlags, (XIFSD_LCB_STATE_DELETE_ON_CLOSE|XIFSD_LCB_STATE_LINK_IS_GONE)))
	{
		return STATUS_INVALID_PARAMETER;
	}

	


	pIrp = pIrpContext->Irp;
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	
	



	ASSERT(XifsdCheckFlagBoolean(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT));


	TargetFileObject = pIrpSp->Parameters.SetFile.FileObject;
	if(TargetFileObject == NULL){
		pTargetParentFCB = pParentFCB;
		XixFsdGetTargetInfo2(pRenameInfomation->FileName,pRenameInfomation->FileNameLength, &TargetFileName);
	}else{

		
		XixFsdDecodeFileObject(TargetFileObject, &pTargetParentFCB, &pTargetParentCCB);
		ASSERT_FCB(pTargetParentFCB);
		XixFsdGetTargetInfo(TargetFileObject, &TargetFileName);
	}


	NewParentLotNumber = pTargetParentFCB->LotNumber;
	

	try{

		if(XifsdCheckFlagBoolean(pFCB->PtrVCB->VCBFlags , XIFSD_VCB_FLAGS_VOLUME_READ_ONLY)){
			try_return(RC = STATUS_INVALID_PARAMETER);
		}

		if(XifsdCheckFlagBoolean(pFCB->Type, XIFS_FD_TYPE_ROOT_DIRECTORY)){
			try_return(RC = STATUS_INVALID_PARAMETER);
		}

/*
		if(XifsdCheckFlagBoolean(pFCB->FileAttribute, FILE_ATTRIBUTE_SYSTEM)){
			try_return(RC = STATUS_INVALID_PARAMETER);
		}
*/

		if(XifsdCheckFlagBoolean(pFCB->FCBFlags,XIFSD_FCB_CHANGE_DELETED)){
			try_return(RC = STATUS_INVALID_DEVICE_REQUEST);
		}

		if(pFCB->FCBType == FCB_TYPE_FILE){
//			if(!XifsdCheckFlagBoolean(pFCB->FCBFlags, XIFSD_FCB_OPEN_WRITE)){
//				try_return(RC = STATUS_INVALID_PARAMETER);
//			}

			if(pFCB->HasLock != FCB_FILE_LOCK_HAS){
				
				
				RC = XixFsLotLock(TRUE, pVCB, pVCB->TargetDeviceObject, pFCB->LotNumber, &pFCB->HasLock, TRUE, FALSE);
					
				if(!NT_SUCCESS(RC)){
					
					try_return(RC = STATUS_INVALID_PARAMETER);
				}
				

				//try_return(RC = STATUS_ACCESS_DENIED);
				
			}
		}

		

		
		if(TargetFileName.Length > XIFS_MAX_FILE_NAME_LENGTH){
			try_return(RC = STATUS_OBJECT_NAME_INVALID);
		}



		if(TargetFileObject == NULL){
				
up_same:
			
			if(!RtlCompareUnicodeString(&pFCB->FCBName, &TargetFileName, TRUE)){
				// Same String do nothing
				try_return(RC = STATUS_SUCCESS);
			}


			if(pFCB->FCBType  == FCB_TYPE_FILE)
			{
				if(pFCB->HasLock != FCB_FILE_LOCK_HAS){
					
					
					RC = XixFsLotLock(TRUE, pVCB, pVCB->TargetDeviceObject, pFCB->LotNumber, &pFCB->HasLock, TRUE, FALSE);
						
					if(!NT_SUCCESS(RC)){
						try_return(RC = STATUS_ACCESS_DENIED);
					}
					

					//try_return(RC = STATUS_ACCESS_DENIED);
					
				}
			}

	
			XifsdAcquireFcbExclusive(TRUE, pTargetParentFCB, FALSE);
			FountLCB = XixFsdFindPrefix(pIrpContext,
								&pTargetParentFCB,
								&TargetFileName,
								FALSE);

			
			XifsdReleaseFcb(TRUE,pTargetParentFCB);

			if(FountLCB){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Alread Exist File Name (%wZ)\n", &TargetFileName));
				try_return(RC = STATUS_OBJECT_NAME_INVALID);
			}
			

			RC = IsExistChecChild(
							pIrpContext,
							pTargetParentFCB,
							&TargetFileName
							);

			
			if(NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Alread Exist File Name (%wZ)\n", &TargetFileName));
				try_return(RC = STATUS_OBJECT_NAME_INVALID);
			}


			XifsdAcquireFcbExclusive(TRUE, pParentFCB, FALSE);
			RC = UpdateParentChild(pIrpContext, 
					pParentFCB,
					&TargetFileName, 
					&pLCB->FileName);

			if(!NT_SUCCESS(RC)){
				XifsdReleaseFcb(TRUE,pParentFCB);
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("FAIL UpdateParentChild (%wZ)\n", &TargetFileName));

				try_return(RC = STATUS_INVALID_PARAMETER);
			}

			XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);
			XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_NAME);
			XifsdSetFlag(pFileObject->Flags, FO_FILE_MODIFIED);
			RC = UpdateFileName(pIrpContext, 
								pFCB,
								pFCB->ParentLotNumber,
								(uint8 *)TargetFileName.Buffer, 
								TargetFileName.Length);
			
			if(!NT_SUCCESS(RC)){
				XifsdReleaseFcb(TRUE,pFCB);
				XifsdReleaseFcb(TRUE,pParentFCB);
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("FAIL UpdateFileName (%wZ)\n", &TargetFileName));
				try_return(RC = STATUS_INVALID_PARAMETER);
			}

			XixFsdRemovePrefix(TRUE, pLCB);

			pTargetLCB = XixFsdInsertPrefix(pIrpContext, pFCB, &TargetFileName, pParentFCB);
			XifsdReleaseFcb(TRUE,pFCB);
			XifsdReleaseFcb(TRUE,pParentFCB);

			pCCB->PtrLCB = pTargetLCB;

	

			if(pFCB->FCBFullPath.Buffer != NULL){
				uint16	ReCalcLen = 0;

				uint8	* tmpBuffer = NULL;
				ReCalcLen = (uint16)(pFCB->FCBFullPath.Length - pFCB->FCBTargetOffset + TargetFileName.Length);

				if(pFCB->FCBFullPath.MaximumLength < ReCalcLen){
					uint16		FullPathLen = 0;
					FullPathLen = SECTORALIGNSIZE_512(ReCalcLen);
					tmpBuffer = ExAllocatePoolWithTag(NonPagedPool, FullPathLen, TAG_FILE_NAME);
					ASSERT(tmpBuffer);
					
					RtlZeroMemory(tmpBuffer, FullPathLen);
					RtlCopyMemory(tmpBuffer, pFCB->FCBFullPath.Buffer, (pFCB->FCBFullPath.Length - pFCB->FCBTargetOffset));
					ExFreePool(pFCB->FCBFullPath.Buffer);
					pFCB->FCBFullPath.Buffer = (PWSTR)tmpBuffer;
					pFCB->FCBFullPath.MaximumLength = FullPathLen;
				}
				
				pFCB->FCBFullPath.Length = ReCalcLen;
				RtlCopyMemory(Add2Ptr(pFCB->FCBFullPath.Buffer, pFCB->FCBTargetOffset), TargetFileName.Buffer, TargetFileName.Length);

			}
			



			XixFsdNotifyReportChange(
						pFCB,
						((pFCB->FCBType == FCB_TYPE_FILE)?FILE_NOTIFY_CHANGE_FILE_NAME: FILE_NOTIFY_CHANGE_DIR_NAME ),
						FILE_ACTION_RENAMED_NEW_NAME
						);
				

					
		}else{
			
			if(pTargetParentFCB->FCBType != FCB_TYPE_DIR) 
			{
				try_return(RC = STATUS_INVALID_PARAMETER);
			}

			if(pTargetParentFCB == pParentFCB){
				if(!RtlCompareUnicodeString(&pFCB->FCBName, &TargetFileName, TRUE)){
					// Same String do nothing
					try_return(RC = STATUS_SUCCESS);
				}

				goto up_same;	
				
			}

			XifsdAcquireFcbExclusive(TRUE, pTargetParentFCB, FALSE);
			FountLCB = XixFsdFindPrefix(pIrpContext,
								&pTargetParentFCB,
								&TargetFileName,
								FALSE);
			
			XifsdReleaseFcb(TRUE,pTargetParentFCB);
			if(FountLCB){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Alread Exist File Name (%wZ)\n", &TargetFileName));
				try_return(RC = STATUS_OBJECT_NAME_COLLISION);
			}


			RC = IsExistChecChild(
							pIrpContext,
							pTargetParentFCB,
							&TargetFileName
							);

			
			if(NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Alread Exist File Name (%wZ)\n", &TargetFileName));
				try_return(RC = STATUS_OBJECT_NAME_INVALID);
			}



			//
			//	Replacement is not allowed
			//

			XifsdAcquireFcbExclusive(TRUE, pParentFCB, FALSE);
			RC = DeleteParentChild(pIrpContext, pParentFCB, &pLCB->FileName);

			XifsdReleaseFcb(TRUE,pParentFCB);
			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Fail DeleteParentChild (%wZ)\n", &TargetFileName));
				try_return(RC = STATUS_INVALID_PARAMETER);
			}

	


			XixFsdNotifyReportChange(
							pFCB,
							FILE_NOTIFY_CHANGE_ATTRIBUTES
								   | FILE_NOTIFY_CHANGE_SIZE
								   | FILE_NOTIFY_CHANGE_LAST_WRITE
								   | FILE_NOTIFY_CHANGE_LAST_ACCESS
								   | FILE_NOTIFY_CHANGE_CREATION,
							FILE_ACTION_MODIFIED
							);

	


			XifsdAcquireFcbExclusive(TRUE, pTargetParentFCB, FALSE);
			RC = AddParentChild(pIrpContext, 
									pTargetParentFCB, 
									pFCB->LotNumber, 
									pFCB->Type, 
									&TargetFileName); 

			XifsdReleaseFcb(TRUE,pTargetParentFCB);
			
			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Fail AddParentChild  (%wZ)\n", &TargetFileName));
				try_return(RC = STATUS_INVALID_PARAMETER);
			}

			XifsdAcquireFcbExclusive(TRUE, pParentFCB, FALSE);
			XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);
			XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_NAME);
			XifsdSetFlag(pFileObject->Flags, FO_FILE_MODIFIED);
			RC = UpdateFileName(pIrpContext, 
								pFCB,
								pTargetParentFCB->LotNumber,
								(uint8 *)TargetFileName.Buffer, 
								TargetFileName.Length);
			
			if(!NT_SUCCESS(RC)){
				XifsdReleaseFcb(TRUE,pFCB);
				XifsdReleaseFcb(TRUE,pParentFCB);
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("FAIL UpdateFileName (%wZ)\n", &TargetFileName));
				try_return(RC = STATUS_INVALID_PARAMETER);
			}

		
			XixFsdRemovePrefix(TRUE, pLCB);

			//
			//  Now Decrement the reference counts for the parent and drop the Vcb.
			//
			XifsdLockVcb(pIrpContext,pVCB);
			DebugTrace( DEBUG_LEVEL_INFO, (DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
						("XifsdSetRenameInformation, PFcb (%I64d) Vcb %d/%d Fcb %d/%d\n", 
						pParentFCB->LotNumber,
						 pVCB->VCBReference,
						 pVCB->VCBUserReference,
						 pParentFCB->FCBReference,
						 pParentFCB->FCBUserReference ));

			XifsdDecRefCount( pParentFCB, 1, 1 );

			DebugTrace( DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
						("XifsdSetRenameInformation, PFcb (%I64d) Vcb %d/%d Fcb %d/%d\n", 
						pParentFCB->LotNumber,
						 pVCB->VCBReference,
						 pVCB->VCBUserReference,
						 pParentFCB->FCBReference,
						 pParentFCB->FCBUserReference ));

			XifsdUnlockVcb( pIrpContext, pVCB );

			XifsdReleaseFcb(TRUE,pFCB);
			XifsdReleaseFcb(TRUE,pParentFCB);

			XifsdAcquireFcbExclusive(TRUE, pTargetParentFCB, FALSE);
			XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);	
			pTargetLCB = XixFsdInsertPrefix(pIrpContext, pFCB, &TargetFileName, pTargetParentFCB);

			XifsdLockVcb(pIrpContext,pVCB);
			DebugTrace( DEBUG_LEVEL_INFO, (DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
						("XifsdSetRenameInformation, PFcb LotNumber(%I64d) Vcb %d/%d Fcb %d/%d\n", 
							pTargetParentFCB->LotNumber,
							pVCB->VCBReference,
							pVCB->VCBUserReference,
							pTargetParentFCB->FCBReference,
							pTargetParentFCB->FCBUserReference ));

			XifsdIncRefCount( pTargetParentFCB, 1, 1 );

			DebugTrace( DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
						 ("XifsdSetRenameInformation, PFcb LotNumber(%I64d) Vcb %d/%d Fcb %d/%d\n",
						 pTargetParentFCB->LotNumber,
						 pVCB->VCBReference,
						 pVCB->VCBUserReference,
						 pTargetParentFCB->FCBReference,
						 pTargetParentFCB->FCBUserReference ));

			XifsdUnlockVcb( pIrpContext, pVCB );

			XifsdReleaseFcb(TRUE,pFCB);
			XifsdReleaseFcb(TRUE,pTargetParentFCB);
			pCCB->PtrLCB = pTargetLCB;




			

			if(pFCB->FCBFullPath.Buffer != NULL){
				
				if(pFCB->FCBFullPath.MaximumLength < TargetFileObject->FileName.Length){
					uint16 FullPathSize = 0;
					ExFreePool(pFCB->FCBFullPath.Buffer);
					pFCB->FCBFullPath.Buffer = NULL;

					FullPathSize = SECTORALIGNSIZE_512(TargetFileObject->FileName.Length);
					pFCB->FCBFullPath.Buffer = ExAllocatePoolWithTag(NonPagedPool, FullPathSize, TAG_FILE_NAME);
					ASSERT(pFCB->FCBFullPath.Buffer);
					pFCB->FCBFullPath.MaximumLength =FullPathSize;

				}

				RtlZeroMemory(pFCB->FCBFullPath.Buffer, pFCB->FCBFullPath.MaximumLength);

				pFCB->FCBFullPath.Length = TargetFileObject->FileName.Length;
				RtlCopyMemory(pFCB->FCBFullPath.Buffer, TargetFileObject->FileName.Buffer, TargetFileObject->FileName.Length);
									
	

			}else{
				uint16 FullPathSize = 0;
				
				FullPathSize = SECTORALIGNSIZE_512(TargetFileObject->FileName.Length);
				pFCB->FCBFullPath.Buffer = ExAllocatePoolWithTag(NonPagedPool, FullPathSize, TAG_FILE_NAME);
				ASSERT(pFCB->FCBFullPath.Buffer);
				pFCB->FCBFullPath.MaximumLength =FullPathSize;	
				
								RtlZeroMemory(pFCB->FCBFullPath.Buffer, pFCB->FCBFullPath.MaximumLength);

				pFCB->FCBFullPath.Length = TargetFileObject->FileName.Length;
				RtlCopyMemory(pFCB->FCBFullPath.Buffer, TargetFileObject->FileName.Buffer, TargetFileObject->FileName.Length);
			}


			if(pFCB->FCBFullPath.Length == 2){
				pFCB->FCBTargetOffset = 0;
			}else{
				pFCB->FCBTargetOffset = XixFsdSearchLastComponetOffset(&pFCB->FCBFullPath);
			}

			XixFsdNotifyReportChange(
								pFCB,
								((pFCB->FCBType == FCB_TYPE_FILE)
									?FILE_NOTIFY_CHANGE_FILE_NAME
									: FILE_NOTIFY_CHANGE_DIR_NAME ),
									FILE_ACTION_ADDED
								);
		
		}
	}finally{

		if(NT_SUCCESS(RC)){
			XixFsdSendRenameLinkBC(
				TRUE,
				XIFS_SUBTYPE_FILE_RENAME,
				pVCB->HostMac,
				pFCB->LotNumber,
				pVCB->DiskId,
				pVCB->PartitionId,
				OldParentLotNumber,
				NewParentLotNumber
			);
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit XixFsdSetRenameInformation.\n"));

	return RC;
}



NTSTATUS
XixFsdSetLinkInfo(
	IN	PXIFS_IRPCONTEXT	pIrpContext,
	IN	PFILE_OBJECT		pFileObject,
	IN	PXIFS_FCB			pFCB,
	IN	PXIFS_CCB			pCCB,
	IN	PFILE_LINK_INFORMATION pLinkInfomation,
	IN	uint32				Length
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PIRP		pIrp = NULL;
	PIO_STACK_LOCATION pIrpSp = NULL;
	PFILE_OBJECT			TargetFileObject = NULL;
	PXIFS_VCB				pVCB = NULL;
	PXIFS_FCB 				pTargetParentFCB;
	PXIFS_CCB				pTargetParentCCB;
	PXIFS_LCB				pTargetLCB;


	USHORT 					PreviousLength;
	USHORT 					LastFileNameOffset;

	UNICODE_STRING			TargetFileName;
	PXIFS_FCB				pParentFCB = NULL;
	PXIFS_LCB				pLCB = NULL;
	PXIFS_LCB				FoundLCB = NULL;	
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter XixFsdSetLinkInfo.\n"));

	ASSERT(pIrpContext);
	ASSERT(pIrpContext->Irp);
	ASSERT_FCB(pFCB);
	ASSERT_CCB(pCCB);
	pLCB = pCCB->PtrLCB;

	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	// Added by ILGU HONG for readonly 09052006
	if(pVCB->IsVolumeWriteProctected){
		return STATUS_MEDIA_WRITE_PROTECTED;
	}
	// Added by ILGU HONG for readonly end

	if(pLCB == NULL){
		return STATUS_INVALID_PARAMETER;
	}
	ASSERT_LCB(pLCB);

	pIrp = pIrpContext->Irp;
	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	
	if(Length < sizeof(PFILE_LINK_INFORMATION)){
		return STATUS_INVALID_PARAMETER;
	}

	if(Length < ( sizeof(FILE_RENAME_INFORMATION) + pLinkInfomation->FileNameLength -sizeof(WCHAR))){
		return STATUS_INVALID_PARAMETER;
	}


	//if(!XifsdCheckFlagBoolean(pCCB->CCBFlags, XIFSD_CCB_OPENED_AS_FILE)){
	//	return STATUS_INVALID_PARAMETER;
	//}

	// Not allowed to create link to Dir
	if(pFCB->FCBType == FCB_TYPE_DIR){
		return STATUS_INVALID_PARAMETER;
	}
	
	if(XifsdCheckFlagBoolean(pLCB->LCBFlags, (XIFSD_LCB_STATE_DELETE_ON_CLOSE|XIFSD_LCB_STATE_LINK_IS_GONE)))
	{
		return STATUS_ACCESS_DENIED;
	}

	if(pFCB->FCBType == FCB_TYPE_VOLUME){
		return STATUS_ACCESS_DENIED;
	}

	if(XifsdCheckFlagBoolean(pFCB->FCBFlags,XIFSD_FCB_CHANGE_DELETED)){
			return STATUS_INVALID_DEVICE_REQUEST;
	}

	if(pFCB->HasLock != FCB_FILE_LOCK_HAS){
		return STATUS_ACCESS_DENIED;
	}


	TargetFileObject = pIrpSp->Parameters.SetFile.FileObject;
	if(TargetFileObject == NULL){
		pTargetParentFCB = pParentFCB;
		XixFsdGetTargetInfo2(pLinkInfomation->FileName,pLinkInfomation->FileNameLength, &TargetFileName);
	}else{
		XixFsdDecodeFileObject(TargetFileObject, &pTargetParentFCB, &pTargetParentCCB);
		ASSERT_FCB(pTargetParentFCB);
		XixFsdGetTargetInfo(TargetFileObject, &TargetFileName);
	}
	
	
	

	if(TargetFileName.Length > XIFS_MAX_FILE_NAME_LENGTH){
		return STATUS_OBJECT_NAME_INVALID;
	}



	try{

		if(TargetFileObject == NULL){

up_same:					

			if(!RtlCompareUnicodeString(&pLCB->FileName, &TargetFileName, TRUE)){
				// Same String do nothing
				try_return(RC = STATUS_SUCCESS);
			}
	

			FoundLCB = XixFsdFindPrefix(pIrpContext,
								&pTargetParentFCB,
								&TargetFileName,
								FALSE);


			if(FoundLCB){
				try_return(RC = STATUS_OBJECT_NAME_INVALID);
			}


			RC = IsExistChecChild(
							pIrpContext,
							pTargetParentFCB,
							&TargetFileName
							);

			
			if(NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Alread Exist File Name (%wZ)\n", &TargetFileName));
				try_return(RC = STATUS_OBJECT_NAME_INVALID);
			}



			// Up Link Count for File
			XifsdAcquireFcbExclusive(TRUE, pTargetParentFCB, FALSE);
			XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);	
			pFCB->LinkCount ++;
			XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_LINKCOUNT);
			XifsdSetFlag(pFileObject->Flags, FO_FILE_MODIFIED);
			XixFsdUpdateFCB(pFCB);

			pTargetLCB = XixFsdInsertPrefix(pIrpContext, pFCB, &TargetFileName, pTargetParentFCB);
			XifsdReleaseFcb(TRUE,pFCB);
			XifsdReleaseFcb(TRUE,pTargetParentFCB);
			try_return(RC = STATUS_SUCCESS);
		}
		
		


		if(pTargetParentFCB == pParentFCB){
			if(!RtlCompareUnicodeString(&pLCB->FileName, &TargetFileName, TRUE)){
				// Same String do nothing
				try_return(RC = STATUS_SUCCESS);
			}
			goto up_same;			
		}


		FoundLCB = XixFsdFindPrefix(pIrpContext,
							&pTargetParentFCB,
							&TargetFileName,
							FALSE);


		if(FoundLCB){
			try_return(RC = STATUS_OBJECT_NAME_INVALID);
		}


		RC = IsExistChecChild(
						pIrpContext,
						pTargetParentFCB,
						&TargetFileName
						);

		
		if(NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Alread Exist File Name (%wZ)\n", &TargetFileName));
			try_return(RC = STATUS_OBJECT_NAME_INVALID);
		}



		// Up Link Count for File
		XifsdAcquireFcbExclusive(TRUE, pTargetParentFCB, FALSE);
		XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);	
		pFCB->LinkCount ++;
		XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_LINKCOUNT);
		XifsdSetFlag(pFileObject->Flags, FO_FILE_MODIFIED);
		XixFsdUpdateFCB(pFCB);

		pTargetLCB = XixFsdInsertPrefix(pIrpContext, pFCB, &TargetFileName, pTargetParentFCB);


		XifsdLockVcb(pIrpContext,pVCB);
		DebugTrace( DEBUG_LEVEL_INFO, (DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
					("XifsdSetLinkInformation, PFcb  LotNumber(%I64d) Vcb %d/%d Fcb %d/%d\n", 
					pTargetParentFCB->LotNumber,
					 pVCB->VCBReference,
					 pVCB->VCBUserReference,
					 pTargetParentFCB->FCBReference,
					 pTargetParentFCB->FCBUserReference ));

		XifsdIncRefCount( pTargetParentFCB, 1, 1 );

		DebugTrace( DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
					 ("XifsdSetLinkInformation, PFcb  LotNumber(%I64d) Vcb %d/%d Fcb %d/%d\n",
					 pTargetParentFCB->LotNumber,
					 pVCB->VCBReference,
					 pVCB->VCBUserReference,
					 pTargetParentFCB->FCBReference,
					 pTargetParentFCB->FCBUserReference ));

		XifsdUnlockVcb( pIrpContext, pVCB );

		XifsdReleaseFcb(TRUE,pFCB);
		XifsdReleaseFcb(TRUE,pTargetParentFCB);
		
		try_return(RC = STATUS_SUCCESS);
		

	}finally{
		if(NT_SUCCESS(RC)){
			XixFsdSendRenameLinkBC(
				TRUE,
				XIFS_SUBTYPE_FILE_LINK,
				pVCB->HostMac,
				pFCB->LotNumber,
				pVCB->DiskId,
				pVCB->PartitionId,
				0,
				0
			);
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit XixFsdSetLinkInfo.\n"));

	return RC;
}


NTSTATUS
XixFsdSetAllocationSize(	
	IN	PXIFS_IRPCONTEXT	pIrpContext,
	IN	PFILE_OBJECT		pFileObject,
	IN	PXIFS_FCB		pFCB,
	IN	PXIFS_CCB		pCCB,
	IN 	PFILE_ALLOCATION_INFORMATION pAllocationInformation,
	IN	uint32			Length
	)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	PXIFS_VCB		pVCB = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter XixFsdSetAllocationSize.\n"));

	if(Length < sizeof(FILE_ALLOCATION_INFORMATION)){
		return STATUS_INVALID_PARAMETER;
	}
	

	

	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	// Added by ILGU HONG for readonly 09052006
	if(pVCB->IsVolumeWriteProctected){
		return STATUS_MEDIA_WRITE_PROTECTED;
	}
	// Added by ILGU HONG for readonly end


	if(pFCB->FCBType != FCB_TYPE_FILE){
		return STATUS_INVALID_PARAMETER;
	}


	XifsdLockFcb(pIrpContext, pFCB);


	try{

		


		if(IoGetCurrentIrpStackLocation(pIrpContext->Irp)->Parameters.SetFile.AdvanceOnly){
				DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
				(" File.AdvanceOnly .\n"));	
				

			if((uint64)(pAllocationInformation->AllocationSize.QuadPart) > pFCB->RealAllocationSize)
			{		
				uint64 Offset = 0;
				uint64 RequestStartOffset = 0;
				uint32 EndLotIndex = 0;
				uint32 CurentEndIndex = 0;
				uint32 RequestStatIndex = 0;
				uint32 LotCount = 0;
				uint32 AllocatedLotCount = 0;

				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
				("  FileSize(%x) FileSize(%I64d)\n", pFCB, pFCB->FileSize));

				
				Offset = pAllocationInformation->AllocationSize.QuadPart;
				

				CurentEndIndex = GetIndexOfLogicalAddress(pVCB->LotSize, (pFCB->RealAllocationSize-1));
				EndLotIndex = GetIndexOfLogicalAddress(pVCB->LotSize, Offset);
				
				if(EndLotIndex > CurentEndIndex){
					
					LotCount = EndLotIndex - CurentEndIndex;
					
					XifsdUnlockFcb(NULL, pFCB);
					RC = XixFsAddNewLot(
								pVCB, 
								pFCB, 
								CurentEndIndex, 
								LotCount, 
								&AllocatedLotCount,
								(uint8 *)pFCB->AddrLot,
								pFCB->AddrLotSize,
								&(pFCB->AddrStartSecIndex)
								);
					
					XifsdLockFcb(NULL, pFCB);
					if(!NT_SUCCESS(RC)){
						try_return(RC);
					}
					
				}

				if(CcIsFileCached(pFileObject)){
					CcSetFileSizes(pFileObject, (PCC_FILE_SIZES)&pFCB->AllocationSize);
					DbgPrint(" 6 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);
					
					XifsdSetFlag(pFileObject->Flags, FO_FILE_MODIFIED);
				}

			}


			/*
			if((uint64)pAllocationInformation->AllocationSize.QuadPart>= (uint64)pFCB->FileSize.QuadPart){ 
					pFCB->AllocationSize.QuadPart = pAllocationInformation->AllocationSize.QuadPart;
				

				CcSetFileSizes(pFileObject, (PCC_FILE_SIZES)&pFCB->AllocationSize);
				DbgPrint(" 6 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBName, pFCB->FileSize.QuadPart);
				
				XifsdSetFlag(pFileObject->Flags, FO_FILE_MODIFIED);
			}
			*/

		}else{


			if((uint64)(pAllocationInformation->AllocationSize.QuadPart) > pFCB->RealAllocationSize)
			{		
				uint64 Offset = 0;
				uint64 RequestStartOffset = 0;
				uint32 EndLotIndex = 0;
				uint32 CurentEndIndex = 0;
				uint32 RequestStatIndex = 0;
				uint32 LotCount = 0;
				uint32 AllocatedLotCount = 0;

				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
				("  FileSize(%x) FileSize(%I64d)\n", pFCB, pFCB->FileSize));

				
				Offset = pAllocationInformation->AllocationSize.QuadPart;
				

				CurentEndIndex = GetIndexOfLogicalAddress(pVCB->LotSize, (pFCB->RealAllocationSize-1));
				EndLotIndex = GetIndexOfLogicalAddress(pVCB->LotSize, Offset);
				
				if(EndLotIndex > CurentEndIndex){
					
					LotCount = EndLotIndex - CurentEndIndex;
					
					XifsdUnlockFcb(NULL, pFCB);
					RC = XixFsAddNewLot(
								pVCB, 
								pFCB, 
								CurentEndIndex, 
								LotCount, 
								&AllocatedLotCount,
								(uint8 *)pFCB->AddrLot,
								pFCB->AddrLotSize,
								&(pFCB->AddrStartSecIndex)
								);
					
					XifsdLockFcb(NULL, pFCB);
					if(!NT_SUCCESS(RC)){
						try_return(RC);
					}
					
				}

				if(CcIsFileCached(pFileObject)){
					CcSetFileSizes(pFileObject, (PCC_FILE_SIZES)&pFCB->AllocationSize);
					DbgPrint(" 6-1 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);
					
					XifsdSetFlag(pFileObject->Flags, FO_FILE_MODIFIED);
				}

			}else{
				if(CcIsFileCached(pFileObject)){
					if(!MmCanFileBeTruncated(&(pFCB->SectionObject), 
								&(pAllocationInformation->AllocationSize)))
					{
					
						try_return(RC = STATUS_USER_MAPPED_FILE);
					}

					if(CcIsFileCached(pFileObject)){
						CcSetFileSizes(pFileObject, (PCC_FILE_SIZES)&pFCB->AllocationSize);
						DbgPrint(" 7 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);
						XifsdSetFlag(pFileObject->Flags, FO_FILE_MODIFIED);
					}
		

				}



		
				/*
				pFCB->AllocationSize.QuadPart =pAllocationInformation->AllocationSize.QuadPart;
				CcSetFileSizes(pFileObject, (PCC_FILE_SIZES)&pFCB->AllocationSize);
				DbgPrint(" 7 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBName, pFCB->FileSize.QuadPart);
				XifsdSetFlag(pFileObject->Flags, FO_FILE_MODIFIED);
				*/
			}
	

		}


		

		DebugTrace(DEBUG_LEVEL_CRITICAL,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB), 
						("New XifsdSetAllocationSize = (%I64d).\n", pFCB->FileSize.QuadPart));


		



		XixFsdNotifyReportChange(
						pFCB,
						FILE_NOTIFY_CHANGE_SIZE,
						FILE_ACTION_MODIFIED
						);


	}finally{
		XifsdUnlockFcb(pIrpContext, pFCB);
	}

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit XixFsdSetAllocationSize.\n"));
	return RC;
}


NTSTATUS
XixFsdSetEndofFileInfo(	
	IN	PXIFS_IRPCONTEXT	pIrpContext,
	IN	PFILE_OBJECT		pFileObject,
	IN	PXIFS_FCB		pFCB,
	IN	PXIFS_CCB		pCCB,
	IN	PFILE_END_OF_FILE_INFORMATION	pEndOfFileInformation,
	IN	uint32							Length
	)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	PXIFS_VCB		pVCB = NULL;
	BOOLEAN			IsEOFProcessing = FALSE;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter XixFsdSetEndofFileInfo.\n"));

	if(Length < sizeof(FILE_END_OF_FILE_INFORMATION)){
		return STATUS_INVALID_PARAMETER;
	}

	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	//	Added by ILGU HONG for readonly 09052006
	if(pVCB->IsVolumeWriteProctected){
		return STATUS_MEDIA_WRITE_PROTECTED;
	}
	//	Added by ILGU HONG for readonly end


	if(pFCB->FCBType != FCB_TYPE_FILE){
		return STATUS_INVALID_PARAMETER;
	}

	XifsdLockFcb(pIrpContext, pFCB);


	


	try{

		//DbgPrint(" EndOfFile (%wZ) Size Request EOF(0x%I64X) ADVANCE ONFY(%s).\n", 
		//	&pFCB->FCBFullPath, 
		//	pEndOfFileInformation->EndOfFile.QuadPart,
		//	((IoGetCurrentIrpStackLocation(pIrpContext->Irp)->Parameters.SetFile.AdvanceOnly == TRUE)?"TRUE":"FALSE"));
		
		if(IoGetCurrentIrpStackLocation(pIrpContext->Irp)->Parameters.SetFile.AdvanceOnly){
			LARGE_INTEGER	NewFileSize ;
			LARGE_INTEGER	NewValidDataLength ;	

				DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
				(" File.AdvanceOnly .\n"));		
			


				
			if((uint64)(pEndOfFileInformation->EndOfFile.QuadPart) > pFCB->RealAllocationSize)
			{		
				uint64 Offset = 0;
				uint64 RequestStartOffset = 0;
				uint32 EndLotIndex = 0;
				uint32 CurentEndIndex = 0;
				uint32 RequestStatIndex = 0;
				uint32 LotCount = 0;
				uint32 AllocatedLotCount = 0;

				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
				("  FileSize(%x) FileSize(%I64d)\n", pFCB, pFCB->FileSize));

				
				Offset = pEndOfFileInformation->EndOfFile.QuadPart;
				

				CurentEndIndex = GetIndexOfLogicalAddress(pVCB->LotSize, (pFCB->RealAllocationSize-1));
				EndLotIndex = GetIndexOfLogicalAddress(pVCB->LotSize, Offset);
				
				if(EndLotIndex > CurentEndIndex){
					
					LotCount = EndLotIndex - CurentEndIndex;
					
					XifsdUnlockFcb(NULL, pFCB);
					RC = XixFsAddNewLot(
								pVCB, 
								pFCB, 
								CurentEndIndex, 
								LotCount, 
								&AllocatedLotCount,
								(uint8 *)pFCB->AddrLot,
								pFCB->AddrLotSize,
								&(pFCB->AddrStartSecIndex)
								);
					
					XifsdLockFcb(NULL, pFCB);
					if(!NT_SUCCESS(RC)){
						try_return(RC);
					}
					
				}

			}

			/*
			NewValidDataLength.QuadPart = pEndOfFileInformation->EndOfFile.QuadPart;
			NewFileSize.QuadPart = pFCB->FileSize.QuadPart;


			if((pFCB->ValidDataLength.QuadPart < NewFileSize.QuadPart) && 
				(NewValidDataLength.QuadPart > pFCB->ValidDataLength.QuadPart))
			{
				NewValidDataLength.QuadPart = pFCB->ValidDataLength.QuadPart;
			}

			if(NewValidDataLength.QuadPart > NewFileSize.QuadPart){
				NewFileSize.QuadPart = NewValidDataLength.QuadPart;
			}


			if((NewFileSize.QuadPart > pFCB->FileSize.QuadPart) || (NewValidDataLength.QuadPart > pFCB->ValidDataLength.QuadPart)){
				pFCB->FileSize.QuadPart = NewFileSize.QuadPart ;
				pFCB->ValidDataLength.QuadPart = NewValidDataLength.QuadPart;
			*/
			if((uint64)pEndOfFileInformation->EndOfFile.QuadPart >= (uint64)pFCB->FileSize.QuadPart)
			{
				XifsdSetFlag(pFileObject->Flags, FO_FILE_SIZE_CHANGED);
				if(CcIsFileCached(pFileObject)){
					CcSetFileSizes(pFileObject, (PCC_FILE_SIZES)&pFCB->AllocationSize);
					DbgPrint(" 8 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);
				}

				

				DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
				(" 8 Changed File Size (%I64d) .\n", pFCB->FileSize.QuadPart));
			}


					
		}else{
			//DbgPrint("Set File Size !!! FileSize(%I64d) Request Size(%I64d)\n", pFCB->FileSize.QuadPart, pEndOfFileInformation->EndOfFile.QuadPart);


			if((uint64)(pEndOfFileInformation->EndOfFile.QuadPart) > pFCB->RealAllocationSize)
			{		
				uint64 Offset = 0;
				uint64 RequestStartOffset = 0;
				uint32 EndLotIndex = 0;
				uint32 CurentEndIndex = 0;
				uint32 RequestStatIndex = 0;
				uint32 LotCount = 0;
				uint32 AllocatedLotCount = 0;

				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_WRITE|DEBUG_TARGET_FCB| DEBUG_TARGET_IRPCONTEXT),
				("  FileSize(%x) FileSize(%I64d)\n", pFCB, pFCB->FileSize));

				
				Offset = pEndOfFileInformation->EndOfFile.QuadPart;
				

				CurentEndIndex = GetIndexOfLogicalAddress(pVCB->LotSize, (pFCB->RealAllocationSize-1));
				EndLotIndex = GetIndexOfLogicalAddress(pVCB->LotSize, Offset);
				
				if(EndLotIndex > CurentEndIndex){
					
					LotCount = EndLotIndex - CurentEndIndex;
					
					XifsdUnlockFcb(NULL, pFCB);
					RC = XixFsAddNewLot(
								pVCB, 
								pFCB, 
								CurentEndIndex, 
								LotCount, 
								&AllocatedLotCount,
								(uint8 *)pFCB->AddrLot,
								pFCB->AddrLotSize,
								&(pFCB->AddrStartSecIndex)
								);
					
					XifsdLockFcb(NULL, pFCB);
					if(!NT_SUCCESS(RC)){
						try_return(RC);
					}
					
				}

			}

			if((uint64)pEndOfFileInformation->EndOfFile.QuadPart >= (uint64)pFCB->FileSize.QuadPart){
					pFCB->FileSize.QuadPart = pEndOfFileInformation->EndOfFile.QuadPart ;
			
	
			}else {

				if(CcIsFileCached(pFileObject)){
					if(!MmCanFileBeTruncated(&(pFCB->SectionObject), 
								&(pEndOfFileInformation->EndOfFile)))
					{
						DbgPrint(" ilgu Error!!");
						try_return(RC = STATUS_USER_MAPPED_FILE);
					}
				}

				
				pFCB->FileSize.QuadPart = pEndOfFileInformation->EndOfFile.QuadPart ;
				if(pFCB->ValidDataLength.QuadPart > pFCB->FileSize.QuadPart){
					DbgPrint("Reduce Valid Data Length");
					pFCB->ValidDataLength.QuadPart = pFCB->FileSize.QuadPart;
				}
			}

			if(CcIsFileCached(pFileObject)){
				CcSetFileSizes(pFileObject, (PCC_FILE_SIZES)&pFCB->AllocationSize);
				DbgPrint(" 9 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);
			}

			XifsdSetFlag(pFileObject->Flags, FO_FILE_SIZE_CHANGED);
			


				
				DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FASTIO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
				(" 8-2 Changed File Size (%I64d) .\n", pFCB->FileSize.QuadPart));

		}
		
		DebugTrace(DEBUG_LEVEL_CRITICAL,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
						("New XifsdSetEndofFileInfo = (%I64d).\n", pFCB->FileSize.QuadPart));


		
		XifsdSetFlag(pFCB->FCBFlags, XIFSD_FCB_MODIFIED_FILE_SIZE);




		XixFsdNotifyReportChange(
							pFCB,
							FILE_NOTIFY_CHANGE_SIZE,
							FILE_ACTION_MODIFIED
							);




	}finally{
		XifsdUnlockFcb(pIrpContext, pFCB);
	}

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit XixFsdSetEndofFileInfo.\n"));
	return RC;
}


NTSTATUS
XixFsdCommonSetInformation(
	IN PXIFS_IRPCONTEXT pIrpContext
	)
{
	NTSTATUS			RC = STATUS_UNSUCCESSFUL;
	PIRP				pIrp = NULL;
    PIO_STACK_LOCATION	pIrpSp = NULL;

  	ULONG Length = 0;
    FILE_INFORMATION_CLASS FileInformationClass;
    PVOID pBuffer = NULL;
    	
   	PFILE_OBJECT pFileObject = NULL;
   	TYPE_OF_OPEN TypeOfOpen = UnopenedFileObject;

	PXIFS_FCB	pFCB = NULL;
	PXIFS_CCB	pCCB = NULL;
	PXIFS_VCB	pVCB = NULL;
	
	BOOLEAN					VCBResourceAcquired = FALSE;	
	BOOLEAN					MainResourceAcquired = FALSE;
	BOOLEAN					CanWait = FALSE;
	BOOLEAN					PostRequest = FALSE;
	BOOLEAN					AcquireFCB = FALSE;
	BOOLEAN					AcquireVCB = FALSE;
	uint32					ReturnedBytes = 0;
	

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter XixFsdCommonSetInformation.\n"));
	
	ASSERT_IRPCONTEXT(pIrpContext);
	ASSERT(pIrpContext->Irp);
	

	
	
	pIrp = pIrpContext->Irp;
	pIrpSp = IoGetCurrentIrpStackLocation( pIrp );

	Length = pIrpSp->Parameters.QueryFile.Length;
    FileInformationClass = pIrpSp->Parameters.QueryFile.FileInformationClass;
    pBuffer = pIrp->AssociatedIrp.SystemBuffer;
	pFileObject = pIrpSp->FileObject;

/*
		if((FileInformationClass == FileDispositionInformation) 
				|| (FileInformationClass == FileRenameInformation)
				|| (FileInformationClass == FileLinkInformation))
			{
				if (!ExAcquireResourceExclusiveLite(&(pFCB->PtrVCB->VCBResource), CanWait)) {
					PostRequest = TRUE;
					try_return(RC = STATUS_PENDING);
				}
				// We have the VCB acquired exclusively.
				VCBResourceAcquired = TRUE;				
			}
 */


	TypeOfOpen = XixFsdDecodeFileObject(pFileObject, &pFCB, &pCCB);
	CanWait = XifsdCheckFlagBoolean(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);





	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB), 
					("FileInformationClass = (0x%x).\n", FileInformationClass));
	if(!CanWait){
			DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_IRPCONTEXT), 
					("Post Request IrpContext(%p) Irp(%p).\n", pIrpContext, pIrp));
		RC = XixFsdPostRequest(pIrpContext, pIrp);
		return RC;
	}





	if( (TypeOfOpen <=UserVolumeOpen)
		|| (TypeOfOpen == UnopenedFileObject) 
		)
	{
		RC = STATUS_INVALID_PARAMETER;
		XixFsdCompleteRequest(pIrpContext, RC , 0);
		return RC;
	}


	ASSERT_FCB(pFCB);
	
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	ASSERT_CCB(pCCB);

	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!SetInformation (%wZ) pCCB(%p)\n", &pFCB->FCBName, pCCB));


	switch(FileInformationClass){
		case FileBasicInformation:
		case FilePositionInformation:
		case FileDispositionInformation:
		case FileAllocationInformation:
		case FileEndOfFileInformation:
			XifsdAcquireFcbExclusive(CanWait, pFCB, FALSE);
			AcquireFCB = TRUE;
			break;
		case FileRenameInformation:
		case FileLinkInformation:
			XifsdAcquireVcbExclusive(CanWait, pVCB, FALSE);
			AcquireVCB = TRUE;
			break;
	}
	
	
	try{	

		XixFsdVerifyFcbOperation( pIrpContext, pFCB );

		if((TypeOfOpen == UserFileOpen) || (TypeOfOpen == UserDirectoryOpen ) )
		{
			
			switch(FileInformationClass){
				case FileBasicInformation:
				{
					

					RC = XixFsdSetBasicInformation(pIrpContext,
							pFileObject,
							pFCB,
							pCCB,
							(PFILE_BASIC_INFORMATION) pBuffer,
							Length
							);

					if(!NT_SUCCESS(RC)){
						try_return(RC);
					}
					
				}break;
				case FilePositionInformation:
				{
				

					RC = XixFsdSetPositionInformation(pIrpContext,
							pFileObject,
							pFCB,
							pCCB,
							(PFILE_POSITION_INFORMATION)pBuffer,
							Length
							);

					if(!NT_SUCCESS(RC)){
						try_return(RC);
					}
					
				}break;					
				case FileDispositionInformation:
				{
					

					RC = XixFsdSetDispositionInformation(pIrpContext,
							pFileObject,
							pFCB,
							pCCB,
							(PFILE_DISPOSITION_INFORMATION) pBuffer,
							Length
							);
					
					if(!NT_SUCCESS(RC)){
						try_return(RC);
					}
					
				}break;					
				case FileRenameInformation:
				{
					

					RC = XixFsdSetRenameInformation(pIrpContext,
							pFileObject,
							pFCB,
							pCCB,
							(PFILE_RENAME_INFORMATION) pBuffer,
							Length
							);
					
					if(!NT_SUCCESS(RC)){
						try_return(RC);
					}
				}break;					
				case FileLinkInformation:
				{
					

					RC = XixFsdSetLinkInfo(pIrpContext,
							pFileObject,
							pFCB,
							pCCB,
							(PFILE_LINK_INFORMATION)pBuffer,
							Length
							);
					
					if(!NT_SUCCESS(RC)){
						try_return(RC);
					}
					
				}break;					
				case FileAllocationInformation:
				{
					

					RC = XixFsdSetAllocationSize(pIrpContext,
							pFileObject,
							pFCB,
							pCCB,
							(PFILE_ALLOCATION_INFORMATION)pBuffer,
							Length
							);

					if(!NT_SUCCESS(RC)){
						try_return(RC);
					}
				}break;					
				case FileEndOfFileInformation:
				{
					

					RC = XixFsdSetEndofFileInfo(pIrpContext,
							pFileObject,
							pFCB,
							pCCB,
							(PFILE_END_OF_FILE_INFORMATION)pBuffer,
							Length
							);

					if(!NT_SUCCESS(RC)){
						try_return(RC);
					}
				}break;					
				default:
					RC = STATUS_INVALID_PARAMETER;
					try_return(RC);
					break;
			}		
		
		}else{
			RC = STATUS_INVALID_PARAMETER;
		}
		
		ReturnedBytes = (uint32)pIrp->IoStatus.Information = (uint32)pIrpSp->Parameters.QueryFile.Length - Length;

	}finally{
		if(AcquireFCB == TRUE){
			XifsdReleaseFcb(CanWait, pFCB);
		}

		if(AcquireVCB == TRUE){
			XifsdReleaseVcb(CanWait, pVCB);
		}
	}

	XixFsdCompleteRequest(pIrpContext, RC, ReturnedBytes);
	
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit XXixFsdCommonSetInformation.\n"));
	return RC;
}