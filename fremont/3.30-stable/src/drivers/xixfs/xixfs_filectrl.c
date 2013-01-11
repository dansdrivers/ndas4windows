#include "xixfs_types.h"
#include "xixfs_drv.h"
#include "SocketLpx.h"
#include "lpxtdi.h"
#include "xixfs_global.h"
#include "xixfs_debug.h"
#include "xixfs_internal.h"
#include "xixcore/lotaddr.h"
#include "xixcore/fileaddr.h"

NTSTATUS
xixfs_QueryInternalInfo (
	IN PXIXFS_FCB 		pFCB, 
	IN PXIXFS_CCB		pCCB, 
    IN OUT PFILE_INTERNAL_INFORMATION FileInternalInformation,
    IN OUT ULONG *Length 
);


NTSTATUS
xixfs_QueryEaInfo (
	IN PXIXFS_FCB 		pFCB, 
	IN PXIXFS_CCB		pCCB, 
    	IN OUT PFILE_EA_INFORMATION FileEaInformation,
    	IN OUT ULONG *Length 
    );


NTSTATUS
xixfs_QueryAccessInfo (
	IN PXIXFS_FCB 		pFCB, 
	IN PXIXFS_CCB		pCCB, 
    	IN OUT PFILE_ACCESS_INFORMATION FileAccessInformation,
    	IN OUT ULONG *Length 
    );


NTSTATUS
xixfs_QueryPositionInfo (
	IN PFILE_OBJECT		pFileObject, 
    	IN OUT PFILE_POSITION_INFORMATION FilePositionInformation,
    	IN OUT ULONG *Length 
    );

NTSTATUS
xixfs_QueryModeInfo (
	IN PXIXFS_FCB 		pFCB, 
	IN PXIXFS_CCB		pCCB, 
    	IN OUT PFILE_MODE_INFORMATION FileModeInformation,
    	IN OUT ULONG *Length 
    );

NTSTATUS
xixfs_QueryAlignInfo (
	IN PXIXFS_FCB 		pFCB, 
	IN PXIXFS_CCB		pCCB, 
    	IN OUT PFILE_ALIGNMENT_INFORMATION FileAlignInformation,
    	IN OUT ULONG *Length 
    );


NTSTATUS
xixfs_QueryNameInfo (
	IN PFILE_OBJECT pFileObject,
	IN OUT PFILE_NAME_INFORMATION FileNameInformation,
	IN OUT PULONG Length
    );

NTSTATUS
xixfs_QueryAlternateNameInfo (
	IN PFILE_OBJECT pFileObject,
	IN OUT PFILE_NAME_INFORMATION FileNameInformation,
	IN OUT ULONG *Length
    );

NTSTATUS
xixfs_SetBasicInformation(
	IN PXIXFS_IRPCONTEXT	pIrpContext,
	IN PFILE_OBJECT		pFileObject,
	IN PXIXFS_FCB		pFCB,
	IN PXIXFS_CCB		pCCB,
	IN PFILE_BASIC_INFORMATION pBasicInfomation,
	IN uint32				Length
);

BOOLEAN
xixfs_FileIsDirectoryEmpty(
	IN PXIXFS_FCB pFCB
);


NTSTATUS
xixfs_SetDispositionInformation(
	IN	PXIXFS_IRPCONTEXT	pIrpContext,
	IN	PFILE_OBJECT		pFileObject,
	IN	PXIXFS_FCB		pFCB,
	IN	PXIXFS_CCB		pCCB,
	IN	PFILE_DISPOSITION_INFORMATION pDispositionInfomation,
	IN	uint32			Length
);


NTSTATUS
xixfs_SetPositionInformation(
	IN	PXIXFS_IRPCONTEXT	pIrpContext,
	IN	PFILE_OBJECT		pFileObject,
	IN	PXIXFS_FCB		pFCB,
	IN	PXIXFS_CCB		pCCB,
	IN	PFILE_POSITION_INFORMATION pPositionInfomation,
	IN	uint32			Length
);


VOID
xixfs_GetTargetInfo2(
	PWCHAR Name, 
	uint32	NameLength,
	PUNICODE_STRING NewLinkName
);


VOID
xixfs_GetTargetInfo(
	IN PFILE_OBJECT			TargetFileObject, 
	IN PUNICODE_STRING 		NewLinkName
);


NTSTATUS
xixfs_FileUpdateParentChild(
	IN	PXIXFS_IRPCONTEXT			pIrpContext,
	IN	PXIXFS_FCB					ParentFCB,
	IN	PUNICODE_STRING				pTargetName, 
	IN	PUNICODE_STRING				pOrgFileName
);


NTSTATUS
xixfs_FileDeleteParentChild(
	IN	PXIXFS_IRPCONTEXT			pIrpContext,
	IN	PXIXFS_FCB					ParentFCB, 
	IN	PUNICODE_STRING				pOrgFileName
);


NTSTATUS
xixfs_FileAddParentChild(
	IN	PXIXFS_IRPCONTEXT			pIrpContext,
	IN	PXIXFS_FCB					ParentFCB,
	IN	uint64						ChildLotNumber,
	IN	uint32						ChildType,
	IN	PUNICODE_STRING				ChildName
);

NTSTATUS
xixfs_FileUpdateFileName(
	IN	PXIXFS_IRPCONTEXT			pIrpContext, 
	IN	PXIXFS_FCB					pFCB,
	IN	uint64						ParentLotNumber,
	IN	uint8						*NewFileName, 
	IN	uint32						NewFileLength
);


NTSTATUS
xixfs_SetRenameInformation(
	IN	PXIXFS_IRPCONTEXT			pIrpContext,
	IN	PFILE_OBJECT				pFileObject,
	IN	PXIXFS_FCB					pFCB,
	IN	PXIXFS_CCB					pCCB,
	IN	PFILE_RENAME_INFORMATION 	pRenameInfomation,
	IN	uint32						Length
);

NTSTATUS
xixfs_SetLinkInfo(
	IN	PXIXFS_IRPCONTEXT	pIrpContext,
	IN	PFILE_OBJECT		pFileObject,
	IN	PXIXFS_FCB			pFCB,
	IN	PXIXFS_CCB			pCCB,
	IN	PFILE_LINK_INFORMATION pLinkInfomation,
	IN	uint32				Length
);


NTSTATUS
xixfs_SetAllocationSize(	
	IN	PXIXFS_IRPCONTEXT	pIrpContext,
	IN	PFILE_OBJECT		pFileObject,
	IN	PXIXFS_FCB		pFCB,
	IN	PXIXFS_CCB		pCCB,
	IN 	PFILE_ALLOCATION_INFORMATION pAllocationInformation,
	IN	uint32			Length
	);

NTSTATUS
xixfs_SetEndofFileInfo(	
	IN	PXIXFS_IRPCONTEXT	pIrpContext,
	IN	PFILE_OBJECT		pFileObject,
	IN	PXIXFS_FCB		pFCB,
	IN	PXIXFS_CCB		pCCB,
	IN	PFILE_END_OF_FILE_INFORMATION	pEndOfFileInformation,
	IN	uint32							Length
	);





WCHAR XifsdUnicodeSelfArray[] = { L'.' };
WCHAR XifsdUnicodeParentArray[] = { L'.', L'.' };

UNICODE_STRING XifsdUnicodeDirectoryNames[] = {
    { sizeof(XifsdUnicodeSelfArray), sizeof(XifsdUnicodeSelfArray), XifsdUnicodeSelfArray},
    { sizeof(XifsdUnicodeParentArray), sizeof(XifsdUnicodeParentArray), XifsdUnicodeParentArray}
};









#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, xixfs_QueryBasicInfo)
#pragma alloc_text(PAGE, xixfs_QueryStandardInfo)
#pragma alloc_text(PAGE, xixfs_QueryInternalInfo)
#pragma alloc_text(PAGE, xixfs_QueryEaInfo)
#pragma alloc_text(PAGE, xixfs_QueryAccessInfo)
#pragma alloc_text(PAGE, xixfs_QueryPositionInfo)
#pragma alloc_text(PAGE, xixfs_QueryModeInfo)
#pragma alloc_text(PAGE, xixfs_QueryAlignInfo)
#pragma alloc_text(PAGE, xixfs_QueryNameInfo)
#pragma alloc_text(PAGE, xixfs_QueryAlternateNameInfo)
#pragma alloc_text(PAGE, xixfs_QueryNetworkInfo)
#pragma alloc_text(PAGE, xixfs_CommonQueryInformation)
#pragma alloc_text(PAGE, xixfs_SetBasicInformation)
#pragma alloc_text(PAGE, xixfs_FileIsDirectoryEmpty)
#pragma alloc_text(PAGE, xixfs_SetDispositionInformation)
#pragma alloc_text(PAGE, xixfs_SetPositionInformation)
#pragma alloc_text(PAGE, xixfs_GetTargetInfo2)
#pragma alloc_text(PAGE, xixfs_GetTargetInfo)
#pragma alloc_text(PAGE, xixfs_FileUpdateParentChild) 
#pragma alloc_text(PAGE, xixfs_FileDeleteParentChild)
#pragma alloc_text(PAGE, xixfs_FileAddParentChild)
#pragma alloc_text(PAGE, xixfs_FileUpdateFileName)
#pragma alloc_text(PAGE, xixfs_SetRenameInformation)
#pragma alloc_text(PAGE, xixfs_SetLinkInfo)
#pragma alloc_text(PAGE, xixfs_SetAllocationSize)
#pragma alloc_text(PAGE, xixfs_SetEndofFileInfo)
#pragma alloc_text(PAGE, xixfs_CommonSetInformation)
#endif


NTSTATUS
xixfs_QueryBasicInfo( 
	IN PXIXFS_FCB 		pFCB, 
	IN PXIXFS_CCB		pCCB, 
	IN OUT PFILE_BASIC_INFORMATION BasicInformation, 
	IN OUT ULONG *Length 
)
{
   	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter xixfs_QueryBasicInfo\n"));

	if(*Length < sizeof(FILE_BASIC_INFORMATION)) return STATUS_BUFFER_OVERFLOW;


	BasicInformation->CreationTime.QuadPart = pFCB->XixcoreFcb.Create_time;
	BasicInformation->LastWriteTime.QuadPart = 
	BasicInformation->ChangeTime.QuadPart = pFCB->XixcoreFcb.Modified_time;
	BasicInformation->LastAccessTime.QuadPart= pFCB->XixcoreFcb.Access_time;
	BasicInformation->FileAttributes = pFCB->XixcoreFcb.FileAttribute;

	XIXCORE_CLEAR_FLAGS( BasicInformation->FileAttributes,
			   (~FILE_ATTRIBUTE_VALID_FLAGS |
				FILE_ATTRIBUTE_COMPRESSED |
				FILE_ATTRIBUTE_TEMPORARY |
				FILE_ATTRIBUTE_SPARSE_FILE |
				FILE_ATTRIBUTE_ENCRYPTED) );

	if(BasicInformation->FileAttributes == 0){
		XIXCORE_SET_FLAGS(BasicInformation->FileAttributes, FILE_ATTRIBUTE_NORMAL);
	}


	*Length -= sizeof(FILE_BASIC_INFORMATION);
	
	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("[FILE_BASIC_INFORMATION] size(%ld) Length(%ld)"
		"XifsdQueryBasicInfo \nCTime(%I64d)\nFileAttribute(%x)\n", 
		sizeof(FILE_BASIC_INFORMATION), *Length,
		pFCB->XixcoreFcb.Create_time, pFCB->XixcoreFcb.FileAttribute));	


	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit xixfs_QueryBasicInfo\n"));

	return STATUS_SUCCESS;
	
}


NTSTATUS
xixfs_QueryStandardInfo(
	IN PXIXFS_FCB 		pFCB, 
	IN PXIXFS_CCB		pCCB, 
	IN OUT PFILE_STANDARD_INFORMATION StandardInformation, 
	IN OUT ULONG *Length 
)
{
   	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter xixfs_QueryStandardInfo\n"));

	if(*Length < sizeof(FILE_STANDARD_INFORMATION)) return STATUS_BUFFER_OVERFLOW;


	StandardInformation->NumberOfLinks = pFCB->XixcoreFcb.LinkCount;
	StandardInformation->DeletePending = XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE);
	if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FileAttribute, FILE_ATTRIBUTE_DIRECTORY)){
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
		"XifsdQueryStandardInfo \nNumberOfLinks(%ld)\nEndOfFile(%I64d)\nAllocationSize(%I64d)\n", 
		sizeof(FILE_STANDARD_INFORMATION), *Length,
		StandardInformation->NumberOfLinks, 
		StandardInformation->EndOfFile.QuadPart,StandardInformation->EndOfFile.QuadPart ));


	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit xixfs_QueryStandardInfo\n"));

	return STATUS_SUCCESS;   
}


NTSTATUS
xixfs_QueryInternalInfo (
	IN PXIXFS_FCB 		pFCB, 
	IN PXIXFS_CCB		pCCB, 
    	IN OUT PFILE_INTERNAL_INFORMATION FileInternalInformation,
    	IN OUT ULONG *Length 
    )
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter xixfs_QueryInternalInfo\n"));

	if(*Length < sizeof(FILE_INTERNAL_INFORMATION)) return STATUS_BUFFER_OVERFLOW;
	
	FileInternalInformation->IndexNumber.QuadPart = pFCB->FileId;
	
	*Length -= sizeof( FILE_INTERNAL_INFORMATION );

	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("FILE_INTERNAL_INFORMATION size(%ld) Length(%ld)" 
		"XifsdQueryInternalInfo FileId(%I64d)\n", 
		sizeof( FILE_INTERNAL_INFORMATION ), *Length,
		FileInternalInformation->IndexNumber.QuadPart ));

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit xixfs_QueryInternalInfo\n"));
	return STATUS_SUCCESS;   
}

NTSTATUS
xixfs_QueryEaInfo (
	IN PXIXFS_FCB 		pFCB, 
	IN PXIXFS_CCB		pCCB, 
    	IN OUT PFILE_EA_INFORMATION FileEaInformation,
    	IN OUT ULONG *Length 
    )
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter xixfs_QueryEaInfo\n"));
	
	if(*Length < sizeof(FILE_EA_INFORMATION)) return STATUS_BUFFER_OVERFLOW;
	FileEaInformation->EaSize = 0;
	*Length -= sizeof( FILE_EA_INFORMATION );

	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB), 
		("FILE_EA_INFORMATION size(%ld) Length(%ld)\n", sizeof( FILE_EA_INFORMATION ), *Length));

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit xixfs_QueryEaInfo\n"));
	return STATUS_SUCCESS;   
}


NTSTATUS
xixfs_QueryAccessInfo (
	IN PXIXFS_FCB 		pFCB, 
	IN PXIXFS_CCB		pCCB, 
    	IN OUT PFILE_ACCESS_INFORMATION FileAccessInformation,
    	IN OUT ULONG *Length 
    )
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter xixfs_QueryAccessInfo\n"));

	if(*Length < sizeof(FILE_ACCESS_INFORMATION)) return STATUS_BUFFER_OVERFLOW;
	FileAccessInformation->AccessFlags= pFCB->XixcoreFcb.DesiredAccess;
	*Length -= sizeof( FILE_ACCESS_INFORMATION );

	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("FILE_ACCESS_INFORMATION size(%ld) Length(%ld)\n", sizeof( FILE_ACCESS_INFORMATION ), *Length));

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit xixfs_QueryAccessInfo\n"));
	return STATUS_SUCCESS;   
}


NTSTATUS
xixfs_QueryPositionInfo (
	IN PFILE_OBJECT		pFileObject, 
    	IN OUT PFILE_POSITION_INFORMATION FilePositionInformation,
    	IN OUT ULONG *Length 
    )
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter xixfs_QueryPositionInfo\n"));

	if(*Length < sizeof(FILE_POSITION_INFORMATION)) return STATUS_BUFFER_OVERFLOW;
	FilePositionInformation->CurrentByteOffset = pFileObject->CurrentByteOffset;
	*Length -= sizeof( FILE_POSITION_INFORMATION );

	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("FILE_POSITION_INFORMATION size(%ld) Length(%ld)"
		"xixfs_QueryPositionInfo  CurrentByteOffset(%I64d)\n", 
		sizeof( FILE_POSITION_INFORMATION ), *Length,
		FilePositionInformation->CurrentByteOffset.QuadPart ));


	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit xixfs_QueryPositionInfo\n"));
	return STATUS_SUCCESS;   
}

NTSTATUS
xixfs_QueryModeInfo (
	IN PXIXFS_FCB 		pFCB, 
	IN PXIXFS_CCB		pCCB, 
    	IN OUT PFILE_MODE_INFORMATION FileModeInformation,
    	IN OUT ULONG *Length 
    )
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter xixfs_QueryModeInfo\n"));

	if(*Length < sizeof(FILE_MODE_INFORMATION)) return STATUS_BUFFER_OVERFLOW;
	FileModeInformation->Mode= 0;
	*Length -= sizeof( FILE_MODE_INFORMATION );

	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("FILE_MODE_INFORMATION size(%ld) Length(%ld)\n", sizeof( FILE_MODE_INFORMATION ), *Length));

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit xixfs_QueryModeInfo\n"));

	return STATUS_SUCCESS;   
}


NTSTATUS
xixfs_QueryAlignInfo (
	IN PXIXFS_FCB 		pFCB, 
	IN PXIXFS_CCB		pCCB, 
    	IN OUT PFILE_ALIGNMENT_INFORMATION FileAlignInformation,
    	IN OUT ULONG *Length 
    )
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter xixfs_QueryAlignInfo\n"));
	
	if(*Length < sizeof(FILE_ALIGNMENT_INFORMATION)) return STATUS_BUFFER_OVERFLOW;
	FileAlignInformation->AlignmentRequirement = 0;
	*Length -= sizeof( FILE_ALIGNMENT_INFORMATION );

	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB), 
		("FILE_ALIGNMENT_INFORMATION size(%ld) Length(%ld)\n", sizeof( FILE_ALIGNMENT_INFORMATION ), *Length));

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter xixfs_QueryAlignInfo\n"));
	return STATUS_SUCCESS;   
}



NTSTATUS
xixfs_QueryNameInfo (
	IN PFILE_OBJECT pFileObject,
	IN OUT PFILE_NAME_INFORMATION FileNameInformation,
	IN OUT PULONG Length
    )
{
	ULONG LengthToCopy;
	NTSTATUS RC = STATUS_SUCCESS;


	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter xixfs_QueryNameInfo\n"));

	
	if(*Length < sizeof(FILE_NAME_INFORMATION)) return STATUS_BUFFER_OVERFLOW;


 	FileNameInformation->FileNameLength = LengthToCopy = pFileObject->FileName.Length;
	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
	   ("xixfs_QueryNameInfo Name (%wZ) Length (%ld).\n", &pFileObject->FileName, pFileObject->FileName.Length));


	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("FILE_NAME_INFORMATION size(%ld) Length(%ld)\n", sizeof( FILE_NAME_INFORMATION ), *Length));

   	*Length -= (FIELD_OFFSET( FILE_NAME_INFORMATION,FileName[0]));



	if (LengthToCopy > *Length) {
		DebugTrace(DEBUG_LEVEL_CRITICAL,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		  ("xixfs_QueryNameInfo RemainLength (%ld) RequiredLength (%ld).\n", *Length, LengthToCopy));

	    LengthToCopy = *Length;
	    RC = STATUS_BUFFER_OVERFLOW;
	}


 	RtlCopyMemory( FileNameInformation->FileName, pFileObject->FileName.Buffer, LengthToCopy );

	
	*Length -= LengthToCopy;

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit xixfs_QueryNameInfo\n"));
	return RC;
	
}

NTSTATUS
xixfs_QueryAlternateNameInfo (
	IN PFILE_OBJECT pFileObject,
	IN OUT PFILE_NAME_INFORMATION FileNameInformation,
	IN OUT ULONG *Length
    )
{
	ULONG LengthToCopy;
	NTSTATUS RC = STATUS_SUCCESS;


	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter xixfs_QueryAlternateNameInfo\n"));

	if(*Length < sizeof(FILE_NAME_INFORMATION)) return STATUS_BUFFER_OVERFLOW;




 	FileNameInformation->FileNameLength = LengthToCopy = pFileObject->FileName.Length;
	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
	   ("xixfs_QueryAlternateNameInfo Name (%wZ) Length (%ld).\n", &pFileObject->FileName, pFileObject->FileName.Length));


	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB), 
		("FILE_NAME_INFORMATION size(%ld) Length(%ld)\n", sizeof( FILE_NAME_INFORMATION ), *Length));

   	*Length -= (FIELD_OFFSET( FILE_NAME_INFORMATION,FileName[0]));



	if (LengthToCopy > *Length) {
		DebugTrace(DEBUG_LEVEL_CRITICAL,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		  ("xixfs_QueryAlternateNameInfo RemainLength (%ld) RequiredLength (%ld).\n", *Length, LengthToCopy));

	    LengthToCopy = *Length;
	    RC = STATUS_BUFFER_OVERFLOW;
	}


 	RtlCopyMemory( FileNameInformation->FileName, pFileObject->FileName.Buffer, LengthToCopy );

	
	*Length -= LengthToCopy;

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit xixfs_QueryAlternateNameInfo\n"));
	return RC;
}


NTSTATUS
xixfs_QueryNetworkInfo (
	IN PXIXFS_FCB 		pFCB, 
	IN PXIXFS_CCB		pCCB, 
	IN OUT PFILE_NETWORK_OPEN_INFORMATION FileNetworkInformation,
	IN OUT PULONG Length
    )
{
	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter xixfs_QueryNetworkInfo\n"));

	if(*Length < sizeof(FILE_NETWORK_OPEN_INFORMATION)) return STATUS_BUFFER_OVERFLOW;
	
	
	FileNetworkInformation->CreationTime.QuadPart =  pFCB->XixcoreFcb.Create_time;
	FileNetworkInformation->LastAccessTime.QuadPart =
		FileNetworkInformation->LastAccessTime.QuadPart = pFCB->XixcoreFcb.Modified_time;
	FileNetworkInformation->ChangeTime.QuadPart = pFCB->XixcoreFcb.Modified_time;
	FileNetworkInformation->LastAccessTime.QuadPart = pFCB->XixcoreFcb.Access_time;
	FileNetworkInformation->FileAttributes =  pFCB->XixcoreFcb.FileAttribute;
		
	if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FileAttribute, FILE_ATTRIBUTE_DIRECTORY)){
		FileNetworkInformation->AllocationSize.QuadPart =FileNetworkInformation->EndOfFile.QuadPart = 0;
	}else{
		FileNetworkInformation->AllocationSize.QuadPart = pFCB->AllocationSize.QuadPart;
		FileNetworkInformation->EndOfFile.QuadPart = pFCB->FileSize.QuadPart;
	}
	
	*Length -= sizeof(FILE_NETWORK_OPEN_INFORMATION);
	
	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("xixfs_QueryNetworkInfo \nCTime(%I64d)\nFileAttribute(%x)\n",
		pFCB->XixcoreFcb.Create_time, pFCB->XixcoreFcb.FileAttribute));


	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Exit XixFsdQueryNetworkInfoo\n"));
	return STATUS_SUCCESS;   
}




NTSTATUS
xixfs_CommonQueryInformation(
	IN PXIXFS_IRPCONTEXT pIrpContext
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

	PXIXFS_FCB	pFCB = NULL;
	PXIXFS_CCB	pCCB = NULL;

	BOOLEAN					MainResourceAcquired = FALSE;
	BOOLEAN					CanWait = FALSE;
	BOOLEAN					ReleaseFCB = FALSE;				

	uint32					ReturnedBytes = 0;


	ASSERT_IRPCONTEXT(pIrpContext);
	ASSERT(pIrpContext->Irp);


	PAGED_CODE();
	
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
		("Enter xixfs_CommonQueryInformation\n"));

	
	pIrp = pIrpContext->Irp;
	pIrpSp = IoGetCurrentIrpStackLocation( pIrp );

	Length = pIrpSp->Parameters.QueryFile.Length;
    FileInformationClass = pIrpSp->Parameters.QueryFile.FileInformationClass;
    pBuffer = pIrp->AssociatedIrp.SystemBuffer;
	pFileObject = pIrpSp->FileObject;

	


	TypeOfOpen = xixfs_DecodeFileObject(pFileObject, &pFCB, &pCCB);


	if(TypeOfOpen <= UserVolumeOpen){
		ReturnedBytes = 0;
		RC = STATUS_INVALID_PARAMETER;
		xixfs_CompleteRequest(pIrpContext, RC, ReturnedBytes );
		return RC;

	}

	CanWait = XIXCORE_TEST_FLAGS(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);

	if(!XifsdAcquireFcbShared(CanWait, pFCB, FALSE)){
		DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_IRPCONTEXT), 
					("PostRequest IrpContext(%p) Irp(%p)\n",pIrpContext, pIrp));

		RC = xixfs_PostRequest(pIrpContext, pIrp);
		return RC;

	}

	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB), 
			("FileInformationClass(%ld) Length(%ld) .\n", FileInformationClass, Length));
	

	RtlZeroMemory(pBuffer, Length);

	try{


		if((TypeOfOpen == UserDirectoryOpen) || (TypeOfOpen == UserFileOpen))
		{
	
		DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!QueryInformation  pCCB(%p)\n",  pCCB));

			xixfs_VerifyFCBOperation( pIrpContext, pFCB );

			switch(FileInformationClass){
				case FileAllInformation:
				{
					DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB), 
							("FileAllInformation Length(%ld) .\n", sizeof(FILE_ALL_INFORMATION)));

					if(XIXCORE_TEST_FLAGS(pCCB->CCBFlags, XIXFSD_CCB_OPENED_BY_FILEID)){
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

					RC = xixfs_QueryBasicInfo( pFCB, pCCB, &((PFILE_ALL_INFORMATION)(pBuffer))->BasicInformation, &Length );
					if(!NT_SUCCESS(RC)){
						break;
					}

					RC = xixfs_QueryStandardInfo(pFCB, pCCB, &((PFILE_ALL_INFORMATION)(pBuffer))->StandardInformation, &Length);
					if(!NT_SUCCESS(RC)){
						break;
					}
					
					RC = xixfs_QueryInternalInfo( pFCB, pCCB, &((PFILE_ALL_INFORMATION)(pBuffer))->InternalInformation, &Length );
					if(!NT_SUCCESS(RC)){
						break;
					}

					RC = xixfs_QueryEaInfo(  pFCB, pCCB,  &((PFILE_ALL_INFORMATION)(pBuffer))->EaInformation, &Length );
					if(!NT_SUCCESS(RC)){
						break;
					}


					RC = xixfs_QueryAccessInfo(pFCB, pCCB,  &((PFILE_ALL_INFORMATION)(pBuffer))->AccessInformation, &Length );
					if(!NT_SUCCESS(RC)){
						break;
					}


					RC = xixfs_QueryPositionInfo(pFileObject, &((PFILE_ALL_INFORMATION)(pBuffer))->PositionInformation, &Length );
					if(!NT_SUCCESS(RC)){
						break;
					}


					RC = xixfs_QueryModeInfo(pFCB, pCCB, &((PFILE_ALL_INFORMATION)(pBuffer))->ModeInformation, &Length);
					if(!NT_SUCCESS(RC)){
						break;
					}
					

					RC = xixfs_QueryAlignInfo(pFCB, pCCB, &((PFILE_ALL_INFORMATION)(pBuffer))->AlignmentInformation, &Length);
					if(!NT_SUCCESS(RC)){
						break;
					}

					RC = xixfs_QueryNameInfo(  pFileObject, &((PFILE_ALL_INFORMATION)(pBuffer))->NameInformation, &Length );					
					if(!NT_SUCCESS(RC)){
						break;
					}
					
				}break;
				case FileBasicInformation:
				{
					RC = xixfs_QueryBasicInfo(
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
					RC = xixfs_QueryStandardInfo(
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
					RC = xixfs_QueryInternalInfo(
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
					RC = xixfs_QueryEaInfo (
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
					RC = xixfs_QueryPositionInfo(
									pFileObject,
									(PFILE_POSITION_INFORMATION )pBuffer, 
									&Length);

					if(!NT_SUCCESS(RC)){
						break;
					}
									
				}break;		
				case FileNameInformation:
				{


					if(XIXCORE_TEST_FLAGS(pCCB->CCBFlags, XIXFSD_CCB_OPENED_BY_FILEID)){
						RC = STATUS_INVALID_PARAMETER;
						break;
					}

					RC = xixfs_QueryNameInfo(
									pFileObject,
									(PFILE_NAME_INFORMATION )pBuffer, 
									&Length);

					if(!NT_SUCCESS(RC)){
						break;
					}
				}break;		
				case FileAlternateNameInformation:
				{
					if(XIXCORE_TEST_FLAGS(pCCB->CCBFlags, XIXFSD_CCB_OPENED_BY_FILEID)){
						RC = STATUS_INVALID_PARAMETER;
						break;
					}

					RC = xixfs_QueryAlternateNameInfo (
									pFileObject,
									(PFILE_NAME_INFORMATION)pBuffer, 
									&Length);

					if(!NT_SUCCESS(RC)){
						break;
					}
				}break;		
				case FileNetworkOpenInformation:
				{
					RC = xixfs_QueryNetworkInfo (
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

	xixfs_CompleteRequest(pIrpContext, RC, ReturnedBytes );
	return RC;

    	
}







NTSTATUS
xixfs_SetBasicInformation(
	IN PXIXFS_IRPCONTEXT	pIrpContext,
	IN PFILE_OBJECT		pFileObject,
	IN PXIXFS_FCB		pFCB,
	IN PXIXFS_CCB		pCCB,
	IN PFILE_BASIC_INFORMATION pBasicInfomation,
	IN uint32				Length
)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	uint32		TempFlags = 0;
	uint32		NotifyFilter = 0;


	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter xixfs_SetBasicInformation.\n"));

	if(Length < sizeof(FILE_BASIC_INFORMATION)){
		return STATUS_INVALID_PARAMETER;
	}

	// Added by ILGU HONG for readonly 09052006
	if(pFCB->PtrVCB->XixcoreVcb.IsVolumeWriteProtected){
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

			if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE){
				if(XIXCORE_TEST_FLAGS(pBasicInfomation->FileAttributes, FILE_ATTRIBUTE_DIRECTORY)){
					RC = STATUS_INVALID_PARAMETER;
					try_return(RC);
				}
			}else{
				if(XIXCORE_TEST_FLAGS(pBasicInfomation->FileAttributes, FILE_ATTRIBUTE_TEMPORARY)){
					RC =STATUS_INVALID_PARAMETER;
					try_return(RC);
				}
			}

			XIXCORE_CLEAR_FLAGS(pBasicInfomation->FileAttributes, ~FILE_ATTRIBUTE_VALID_SET_FLAGS);
			XIXCORE_CLEAR_FLAGS(pBasicInfomation->FileAttributes, FILE_ATTRIBUTE_NORMAL);
			
			
			XifsdLockFcb(pIrpContext, pFCB);
			pFCB->XixcoreFcb.FileAttribute = ((pFCB->XixcoreFcb.FileAttribute & ~FILE_ATTRIBUTE_VALID_SET_FLAGS)
													| pBasicInfomation->FileAttributes);

			NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;

			if (XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.Type ,XIFS_FD_TYPE_ROOT_DIRECTORY)) {
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FileAttribute, (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN));

			}

			if( XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FileAttribute, FILE_ATTRIBUTE_TEMPORARY)){
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_OPEN_TEMPORARY);
				XIXCORE_SET_FLAGS(pFileObject->Flags, FO_TEMPORARY_FILE);
			}else{
				XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_OPEN_TEMPORARY);
				XIXCORE_CLEAR_FLAGS(pFileObject->Flags, FO_TEMPORARY_FILE);
			}
			
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_ATTR);
			XIXCORE_SET_FLAGS(pFileObject->Flags, FO_FILE_MODIFIED);

			XifsdUnlockFcb(pIrpContext, pFCB);
			
			

		}

	
		

		if (pBasicInfomation->ChangeTime.QuadPart != 0) {
			XIXCORE_SET_FLAGS( TempFlags, XIXFSD_CCB_MODIFY_TIME_SET );
			pFCB->XixcoreFcb.Modified_time = pBasicInfomation->ChangeTime.QuadPart;
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_TIME);
			NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
			
		}

		if (pBasicInfomation->LastAccessTime.QuadPart != 0) {

			XIXCORE_SET_FLAGS( TempFlags, XIXFSD_CCB_ACCESS_TIME_SET );
			pFCB->XixcoreFcb.Access_time  = pBasicInfomation->LastAccessTime.QuadPart;
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_TIME);
			NotifyFilter |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
		}

		if (pBasicInfomation->LastWriteTime.QuadPart != 0) {

			XIXCORE_SET_FLAGS( TempFlags, XIXFSD_CCB_MODIFY_TIME_SET );
			pFCB->XixcoreFcb.Modified_time =  pBasicInfomation->LastWriteTime.QuadPart;
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_TIME);
			NotifyFilter |= FILE_NOTIFY_CHANGE_LAST_WRITE;
		}

		if (pBasicInfomation->CreationTime.QuadPart != 0) {
			XIXCORE_SET_FLAGS( TempFlags, XIXFSD_CCB_CREATE_TIME_SET );	
			pFCB->XixcoreFcb.Create_time = pBasicInfomation->CreationTime.QuadPart;
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_TIME);
			NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
		}

		if(TempFlags != 0){
			XIXCORE_SET_FLAGS(pCCB->CCBFlags, TempFlags);
			XIXCORE_SET_FLAGS(pFileObject->Flags, FO_FILE_MODIFIED);
		}
	


		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_ALL,
			("FCB flags (0x%x) CCB flags(0x%x)!!!!\n", pFCB->XixcoreFcb.FCBFlags, pCCB->CCBFlags));

		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_ALL,
			("FCB Attributes (0x%x)\n", pFCB->XixcoreFcb.FileAttribute));

		;
	}finally{
		XifsdUnlockFcb(pIrpContext,pFCB);
	}


	if(NT_SUCCESS(RC)){
		if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR){
			xixfs_UpdateFCB(pFCB);
		}
		
	}

	xixfs_NotifyReportChangeToXixfs(
					pFCB,
					NotifyFilter,
					FILE_ACTION_MODIFIED
					);



	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit xixfs_SetBasicInformation.\n"));

	return RC;
}

BOOLEAN
xixfs_FileIsDirectoryEmpty(IN PXIXFS_FCB pFCB)
{
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter xixfs_FileIsDirectoryEmpty.\n"));

	ASSERT(pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR);
	
	if(pFCB->XixcoreFcb.ChildCount == 0) {
		
		DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
			("Exit xixfs_FileIsDirectoryEmpty Empty Dir.\n"));
		return TRUE;
	}
	else {
		DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
			("Exit xixfs_FileIsDirectoryEmpty Has children.\n"));
		return FALSE;
	}
}	


NTSTATUS
xixfs_SetDispositionInformation(
	IN	PXIXFS_IRPCONTEXT	pIrpContext,
	IN	PFILE_OBJECT		pFileObject,
	IN	PXIXFS_FCB		pFCB,
	IN	PXIXFS_CCB		pCCB,
	IN	PFILE_DISPOSITION_INFORMATION pDispositionInfomation,
	IN	uint32			Length
)
{
	NTSTATUS	RC = STATUS_SUCCESS;
	PXIXFS_LCB	pLCB = NULL;
	PXIXFS_VCB	pVCB = NULL;


	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter xixfs_SetDispositionInformation pFCB(%I64d).\n", pFCB->XixcoreFcb.LotNumber));

	
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
	if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
		return STATUS_MEDIA_WRITE_PROTECTED;
	}
	// Added by ILGU HONG for readonly end


	XifsdAcquireFcbExclusive(TRUE,pLCB->ParentFcb, FALSE);
	XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);

	try{
		if(pDispositionInfomation->DeleteFile){

			if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("xixfs_SetDispositionInformation pFCB(%I64d) is alread set CLOSE.\n", pFCB->XixcoreFcb.LotNumber));
				try_return(RC);
			}

			if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE ){
				if(pFCB->XixcoreFcb.HasLock != FCB_FILE_LOCK_HAS){

					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("xixfs_SetDispositionInformation pFCB(%I64d) is Has not Write Permission.\n", pFCB->XixcoreFcb.LotNumber));
					//DbgPrint("<%s:%d>:Get xixcore_LotLock\n", __FILE__,__LINE__);
					RC = xixcore_LotLock(&(pVCB->XixcoreVcb), pFCB->XixcoreFcb.LotNumber, &pFCB->XixcoreFcb.HasLock, 1, 0);
						
					if(!NT_SUCCESS(RC)){
					
						RC = STATUS_CANNOT_DELETE;
						try_return(RC);
					}

					//DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					//	("XifsdSetDispositionInformation pFCB(%I64d) is Has not Write Permission.\n", pFCB->LotNumber));
					//RC = STATUS_CANNOT_DELETE;
					//try_return(RC);
				}
			}
			
			/*
			if(!XIXCORE_TEST_FLAGS(pFCB->FCBFlags, XIXCORE_FCB_OPEN_WRITE)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL
					("XifsdSetDispositionInformation pFCB(%I64d) is Has is not Open for wirte.\n", pFCB->LotNumber));
				RC = STATUS_CANNOT_DELETE;
				try_return(RC);
			}
			*/

			/*
			if(pFCB->FCBCleanup > 1){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("XifsdSetDispositionInformation pFCB(%I64d) is Has Opened by Other(%ld).\n", pFCB->LotNumber, pFCB->FCBCleanup));
					RC = STATUS_CANNOT_DELETE;
					try_return(RC);
			}
			*/

			if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE ){
				if (!MmFlushImageSection(&(pFCB->SectionObject), MmFlushForDelete)) {
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("XifsdSetDispositionInformation pFCB(%I64d) fail MmFlushImageSecton.\n", pFCB->XixcoreFcb.LotNumber));
	
					RC = STATUS_CANNOT_DELETE;
					try_return(RC);
				}
			}

			if (XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.Type,XIFS_FD_TYPE_ROOT_DIRECTORY)) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("XifsdSetDispositionInformation pFCB(%I64d) is failed because it is ROOT DIRECTORY.\n", pFCB->XixcoreFcb.LotNumber));
				RC = STATUS_CANNOT_DELETE;
				try_return(RC);
			}


/*
			if(XIXCORE_TEST_FLAGS(pFCB->FileAttribute, FILE_ATTRIBUTE_SYSTEM)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("XifsdSetDispositionInformation pFCB(%I64d) is failed because it  is System file attribute.\n", pFCB->LotNumber));
				RC = STATUS_CANNOT_DELETE;
				try_return(RC);
			}

			if(XIXCORE_TEST_FLAGS(pFCB->Type, XIFS_FD_TYPE_SYSTEM_FILE)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("XifsdSetDispositionInformation pFCB(%I64d) is failed because it  is System file type.\n", pFCB->LotNumber));
				RC = STATUS_CANNOT_DELETE;
				try_return(RC);
			}
*/


			if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_VOLUME){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("XifsdSetDispositionInformation pFCB(%I64d) is failed because it  is for Volume file.\n", pFCB->XixcoreFcb.LotNumber));
				RC = STATUS_CANNOT_DELETE;
				try_return(RC);
			}

			if(!XIXCORE_TEST_FLAGS(pLCB->LCBFlags, XIFSD_LCB_STATE_DELETE_ON_CLOSE)){

				if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR){
					if(!xixfs_FileIsDirectoryEmpty(pFCB)){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
							("XifsdSetDispositionInformation pFCB(%I64d) Directory is not empty.\n", pFCB->XixcoreFcb.LotNumber));
						RC = STATUS_CANNOT_DELETE;
						try_return(RC);
					}

					XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE);
					XIXCORE_SET_FLAGS(pFileObject->Flags, FO_FILE_MODIFIED);
					/*
					if(!XIXCORE_TEST_FLAGS(pCCB->CCBFlags, XIXFSD_CCB_OPENED_BY_FILEID)
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

					if(!XIXCORE_TEST_FLAGS(pCCB->CCBFlags, XIXFSD_CCB_OPENED_BY_FILEID)
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
					if(pFCB->XixcoreFcb.LinkCount  > 0){
						XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE);
						XIXCORE_SET_FLAGS(pFileObject->Flags, FO_FILE_MODIFIED);
					}	
				}
				
				XIXCORE_SET_FLAGS(pLCB->LCBFlags, XIFSD_LCB_STATE_DELETE_ON_CLOSE);
				// Delete From Parent Directory Entry //
				//RC = XifsdDeleteEntryFromDir(pIrpContext, pLCB->ParentFcb, pFCB->LotNumber);	
			}else{
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE);
				XIXCORE_SET_FLAGS(pLCB->LCBFlags, XIFSD_LCB_STATE_DELETE_ON_CLOSE);
			}
			pFileObject->DeletePending = TRUE;
			//DbgPrint(" !!!Delete Pending Set TRUE (%wZ)  .\n", &pFCB->FCBFullPath);
		}else{
	
			
			if(XIXCORE_TEST_FLAGS(pLCB->LCBFlags, XIFSD_LCB_STATE_DELETE_ON_CLOSE)){
				XIXCORE_CLEAR_FLAGS(pLCB->LCBFlags, XIFSD_LCB_STATE_DELETE_ON_CLOSE);
			}


			if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE)){
				XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE);
			}
			

			pFileObject->DeletePending = FALSE;
			//DbgPrint(" !!!Delete Pending Set FALSE (%wZ)  .\n", &pFCB->FCBFullPath);
		}

		
		
		

		;
	}finally{
		XifsdReleaseFcb(TRUE, pFCB);
		XifsdReleaseFcb(TRUE, pLCB->ParentFcb);
	}

	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit xixfs_SetDispositionInformation.\n"));
	return RC;
}



NTSTATUS
xixfs_SetPositionInformation(
	IN	PXIXFS_IRPCONTEXT	pIrpContext,
	IN	PFILE_OBJECT		pFileObject,
	IN	PXIXFS_FCB		pFCB,
	IN	PXIXFS_CCB		pCCB,
	IN	PFILE_POSITION_INFORMATION pPositionInfomation,
	IN	uint32			Length
)
{

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter xixfs_SetPositionInformation.\n"));

	if(Length < sizeof(FILE_POSITION_INFORMATION)){
		return STATUS_INVALID_PARAMETER;
	}
	


	try{
		XifsdLockFcb(pIrpContext, pFCB);
		pFileObject->CurrentByteOffset = pPositionInfomation->CurrentByteOffset;
		DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("xixfs_SetPositionInformation CurrentByteOffset(%I64ld).\n", pFileObject->CurrentByteOffset.QuadPart));
		XifsdUnlockFcb(pIrpContext, pFCB);
	}finally{
		;
	}

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit xixfs_SetPositionInformation.\n"));
	return STATUS_SUCCESS;
}


VOID
xixfs_GetTargetInfo2(
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
		("Enter xixfs_GetTargetInfo2.\n"));

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
		("Exit xixfs_GetTargetInfo2.\n"));
}



VOID
xixfs_GetTargetInfo(
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
		("Enter xixfs_GetTargetInfo.\n"));
	

	while (TargetFileObject->FileName.Buffer[i] == L'\\') {
		LastFileNameOffset += sizeof( WCHAR );
		i++;
	}	
	*/

	LastFileNameOffset =	(uint16) xixfs_SearchLastComponetOffsetOfName(&(TargetFileObject->FileName));

	NewLinkName->MaximumLength = 
	NewLinkName->Length = TargetFileObject->FileName.Length  - LastFileNameOffset;
	NewLinkName->Buffer = (PWSTR) Add2Ptr( TargetFileObject->FileName.Buffer, LastFileNameOffset);   
	

	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("NewLinkName from TagetFileObject (%wZ)\n",NewLinkName));

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit xixfs_GetTargetInfo.\n"));
}



NTSTATUS
xixfs_FileIsExistChecChild(
	IN	PXIXFS_IRPCONTEXT			pIrpContext,
	IN	PXIXFS_FCB					ParentFCB,
	IN	PUNICODE_STRING				pTargetName
)
{
	NTSTATUS 					RC = STATUS_SUCCESS;
	XIXCORE_DIR_EMUL_CONTEXT		DirContext;
	uint64						EntryIndex;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter CheckParentChild.\n"));	
	
	RtlZeroMemory(&DirContext, sizeof(XIXCORE_DIR_EMUL_CONTEXT));

	xixcore_InitializeDirContext(&(ParentFCB->PtrVCB->XixcoreVcb), &DirContext);
	

	RC = xixcore_FindDirEntry ( 
						&(ParentFCB->PtrVCB->XixcoreVcb),
						&(ParentFCB->XixcoreFcb),
						(xc_uint8*)pTargetName->Buffer,
						pTargetName->Length,
						&DirContext,
						&EntryIndex,
						0
						);
	
	xixcore_CleanupDirContext(&DirContext);

	return RC;

}




NTSTATUS
xixfs_FileUpdateParentChild(
	IN	PXIXFS_IRPCONTEXT			pIrpContext,
	IN	PXIXFS_FCB					ParentFCB,
	IN	PUNICODE_STRING				pTargetName, 
	IN	PUNICODE_STRING				pOrgFileName)
{
	NTSTATUS 					RC = STATUS_SUCCESS;
	XIXCORE_DIR_EMUL_CONTEXT		DirContext;
	UNICODE_STRING				NewName;
	uint64						EntryIndex = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter xixfs_FileUpdateParentChild.\n"));	

	RtlZeroMemory(&DirContext, sizeof(XIXCORE_DIR_EMUL_CONTEXT));	

	xixcore_InitializeDirContext(&(ParentFCB->PtrVCB->XixcoreVcb),&DirContext);
	

	RC = xixcore_FindDirEntry (
			&(ParentFCB->PtrVCB->XixcoreVcb),
			&(ParentFCB->XixcoreFcb), 
			(xc_uint8 *)pOrgFileName->Buffer,
			pOrgFileName->Length,
			&DirContext,
			&EntryIndex,
			0
			);

	if(!NT_SUCCESS(RC))
	{
		xixcore_CleanupDirContext(&DirContext);
		return STATUS_ACCESS_DENIED;
	}


	RC = xixcore_UpdateChildFromDir(
							&(ParentFCB->PtrVCB->XixcoreVcb), 
							&(ParentFCB->XixcoreFcb),  
							DirContext.StartLotIndex,
							DirContext.Type,
							DirContext.State,
							(xc_uint8 *)pTargetName->Buffer,
							pTargetName->Length,
							DirContext.ChildIndex,
							&DirContext
							);

	xixcore_CleanupDirContext(&DirContext);

	if(!NT_SUCCESS(RC)){
		return RC;		
	}
	

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit xixfs_FileUpdateParentChild.\n"));	
	return STATUS_SUCCESS;
}
	

NTSTATUS
xixfs_FileDeleteParentChild(
	IN	PXIXFS_IRPCONTEXT			pIrpContext,
	IN	PXIXFS_FCB					ParentFCB, 
	IN	PUNICODE_STRING				pOrgFileName)
{
	NTSTATUS 					RC = STATUS_SUCCESS;
	XIXCORE_DIR_EMUL_CONTEXT		DirContext;
	UNICODE_STRING				NewName;
	uint64						EntryIndex = 0;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter xixfs_FileDeleteParentChild.\n"));	
	
	RtlZeroMemory(&DirContext, sizeof(XIXCORE_DIR_EMUL_CONTEXT));

	xixcore_InitializeDirContext(&(ParentFCB->PtrVCB->XixcoreVcb),&DirContext);
	
	RC = xixcore_FindDirEntry (
			&(ParentFCB->PtrVCB->XixcoreVcb),
			&(ParentFCB->XixcoreFcb), 
			(xc_uint8 *)pOrgFileName->Buffer,
			pOrgFileName->Length,
			&DirContext,
			&EntryIndex,
			0
			);

	if(!NT_SUCCESS(RC))
	{
		xixcore_CleanupDirContext(&DirContext);
		return STATUS_ACCESS_DENIED;
	}


	

	RC = xixcore_DeleteChildFromDir(
								&(ParentFCB->PtrVCB->XixcoreVcb),
								&(ParentFCB->XixcoreFcb), 
								DirContext.ChildIndex,
								&DirContext
								);



	xixcore_CleanupDirContext(&DirContext);

	if(!NT_SUCCESS(RC)){
		return RC;		
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit xixfs_FileDeleteParentChild.\n"));		
	return STATUS_SUCCESS;
}





	
NTSTATUS
xixfs_FileAddParentChild(
	IN	PXIXFS_IRPCONTEXT			pIrpContext,
	IN	PXIXFS_FCB					ParentFCB,
	IN	uint64						ChildLotNumber,
	IN	uint32						ChildType,
	IN	PUNICODE_STRING				ChildName)
{
	NTSTATUS 					RC = STATUS_SUCCESS;
	XIXCORE_DIR_EMUL_CONTEXT		DirContext;
	UNICODE_STRING				NewName;
	PXIXFS_VCB					pVCB = NULL;
	uint64						ChildIndex;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter xixfs_FileAddParentChild.\n"));		

	ASSERT_FCB(ParentFCB);
	pVCB = ParentFCB->PtrVCB;
	ASSERT_VCB(pVCB);


	RtlZeroMemory(&DirContext, sizeof(XIXCORE_DIR_EMUL_CONTEXT));

	RC = xixcore_InitializeDirContext(&(pVCB->XixcoreVcb),&DirContext);
	if(!NT_SUCCESS(RC)){
		return RC;		
	}

	RC = xixcore_LookupInitialDirEntry(
					&(pVCB->XixcoreVcb),
					&(ParentFCB->XixcoreFcb), 
					&DirContext, 
					2 
					);

	if(!NT_SUCCESS(RC)){
		goto error_out;		
	}

	RC = xixcore_AddChildToDir(
					&(pVCB->XixcoreVcb),
					&(ParentFCB->XixcoreFcb),
					ChildLotNumber,
					ChildType,
					(xc_uint8 *)ChildName->Buffer,
					ChildName->Length,
					&DirContext,
					&ChildIndex
					);

error_out:
	xixcore_CleanupDirContext(&DirContext);

	if(!NT_SUCCESS(RC)){
		return RC;		
	}

	XifsdLockFcb(NULL, ParentFCB);
	ParentFCB->AllocationSize.QuadPart = ParentFCB->XixcoreFcb.RealAllocationSize;
	XifsdUnlockFcb(NULL, ParentFCB);
	
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit xixfs_FileAddParentChild.\n"));	

	return STATUS_SUCCESS;
}





NTSTATUS
xixfs_FileUpdateFileName(
	IN	PXIXFS_IRPCONTEXT			pIrpContext, 
	IN	PXIXFS_FCB					pFCB,
	IN	uint64						ParentLotNumber,
	IN	uint8						*NewFileName, 
	IN	uint32						NewFileLength)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter xixfs_FileUpdateFileName.\n"));	


	ASSERT_FCB(pFCB);


	if(pFCB->XixcoreFcb.FCBNameLength < NewFileLength){
		ExFreePoolWithTag(pFCB->XixcoreFcb.FCBName, XCTAG_FCBNAME);
		pFCB->XixcoreFcb.FCBName= ExAllocatePoolWithTag(NonPagedPool, SECTORALIGNSIZE_512(NewFileLength), XCTAG_FCBNAME);
		if(pFCB->XixcoreFcb.FCBName == NULL) {
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		pFCB->XixcoreFcb.FCBNameLength = (uint16)NewFileLength;
	}
	RtlZeroMemory(pFCB->XixcoreFcb.FCBName, pFCB->XixcoreFcb.FCBNameLength);
	RtlCopyMemory(pFCB->XixcoreFcb.FCBName, NewFileName, pFCB->XixcoreFcb.FCBNameLength);
	
	pFCB->XixcoreFcb.ParentLotNumber = ParentLotNumber;

	XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_NAME);

	DebugTrace(DEBUG_LEVEL_ALL,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_ALL),  
					("New File Name  Id(%I64d)\n",  pFCB->XixcoreFcb.LotNumber));	

	try{
		RC = xixfs_UpdateFCB(pFCB);
	}finally{
		if(!NT_SUCCESS(RC)){
			DebugTrace(DEBUG_LEVEL_ERROR,DEBUG_TARGET_ALL,  
					("FAIL SET New File Name  Id(%I64d)\n", pFCB->XixcoreFcb.LotNumber));	
		}
	}


	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit xixfs_FileUpdateFileName. Status(0x%x)\n", RC));	
	return RC;

}







NTSTATUS
xixfs_SetRenameInformation(
	IN	PXIXFS_IRPCONTEXT			pIrpContext,
	IN	PFILE_OBJECT				pFileObject,
	IN	PXIXFS_FCB					pFCB,
	IN	PXIXFS_CCB					pCCB,
	IN	PFILE_RENAME_INFORMATION 	pRenameInfomation,
	IN	uint32						Length
)
{
	NTSTATUS			RC = STATUS_SUCCESS;
	PIRP				pIrp = NULL;
	PIO_STACK_LOCATION	pIrpSp = NULL;
	PFILE_OBJECT		TargetFileObject = NULL;
	UNICODE_STRING		TargetFileName;
	UNICODE_STRING		FileName;
	PXIXFS_LCB			pLCB = NULL;
	PXIXFS_VCB			pVCB = NULL;
	PXIXFS_FCB			pParentFCB = NULL;
	
	PXIXFS_FCB			pTargetParentFCB = NULL;
	PXIXFS_CCB			pTargetParentCCB = NULL;
	
	PXIXFS_LCB			pTargetLCB = NULL;
	PXIXFS_LCB			FountLCB = NULL;

	
	uint64				OldParentLotNumber = 0;
	uint64				NewParentLotNumber = 0;

	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter xixfs_SetRenameInformation.\n"));

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
	if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
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


	OldParentLotNumber = pParentFCB->XixcoreFcb.LotNumber;


	if(XIXCORE_TEST_FLAGS(pLCB->LCBFlags, (XIFSD_LCB_STATE_DELETE_ON_CLOSE|XIFSD_LCB_STATE_LINK_IS_GONE)))
	{
		return STATUS_INVALID_PARAMETER;
	}

	


	pIrp = pIrpContext->Irp;
	ASSERT(pIrp);

	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	
	



	ASSERT(XIXCORE_TEST_FLAGS(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT));

	FileName.Buffer = (PWSTR)pFCB->XixcoreFcb.FCBName;
	FileName.Length=FileName.MaximumLength= (uint16)pFCB->XixcoreFcb.FCBNameLength;

	TargetFileObject = pIrpSp->Parameters.SetFile.FileObject;
	if(TargetFileObject == NULL){
		pTargetParentFCB = pParentFCB;
		xixfs_GetTargetInfo2(pRenameInfomation->FileName,pRenameInfomation->FileNameLength, &TargetFileName);
	}else{

		
		xixfs_DecodeFileObject(TargetFileObject, &pTargetParentFCB, &pTargetParentCCB);
		ASSERT_FCB(pTargetParentFCB);
		xixfs_GetTargetInfo(TargetFileObject, &TargetFileName);
	}


	NewParentLotNumber = pTargetParentFCB->XixcoreFcb.LotNumber;
	

	try{

		if(XIXCORE_TEST_FLAGS(pFCB->PtrVCB->VCBFlags , XIFSD_VCB_FLAGS_VOLUME_READ_ONLY)){
			RC = STATUS_INVALID_PARAMETER;
			try_return(RC);
		}

		if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.Type, XIFS_FD_TYPE_ROOT_DIRECTORY)){
			RC = STATUS_INVALID_PARAMETER;
			try_return(RC);
		}

/*
		if(XIXCORE_TEST_FLAGS(pFCB->FileAttribute, FILE_ATTRIBUTE_SYSTEM)){
			RC = STATUS_INVALID_PARAMETER;
			try_return(RC);
		}
*/

		if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_CHANGE_DELETED)){
			RC = STATUS_INVALID_DEVICE_REQUEST;
			try_return(RC);
		}

		if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE){
//			if(!XIXCORE_TEST_FLAGS(pFCB->FCBFlags, XIXCORE_FCB_OPEN_WRITE)){
//				RC = STATUS_INVALID_PARAMETER;
//				try_return(RC);
//			}

			if(pFCB->XixcoreFcb.HasLock != FCB_FILE_LOCK_HAS){
				
				//DbgPrint("<%s:%d>:Get xixcore_LotLock\n", __FILE__,__LINE__);
				RC = xixcore_LotLock(&(pVCB->XixcoreVcb), pFCB->XixcoreFcb.LotNumber, &pFCB->XixcoreFcb.HasLock, 1, 0);
					
				if(!NT_SUCCESS(RC)){
					RC = STATUS_ACCESS_DENIED;
					try_return(RC);
				}
				

				
				
			}
		}

		

		
		if(TargetFileName.Length > XIFS_MAX_FILE_NAME_LENGTH){
			RC = STATUS_OBJECT_NAME_INVALID;
			try_return(RC);
		}



		if(TargetFileObject == NULL){
				
up_same:
			
			if(!RtlCompareUnicodeString(&FileName, &TargetFileName, TRUE)){
				// Same String do nothing
				RC = STATUS_SUCCESS;
				try_return(RC);
			}


			if(pFCB->XixcoreFcb.FCBType  == FCB_TYPE_FILE)
			{
				if(pFCB->XixcoreFcb.HasLock != FCB_FILE_LOCK_HAS){
					
					//DbgPrint("<%s:%d>:Get xixcore_LotLock\n", __FILE__,__LINE__);
					RC = xixcore_LotLock(
							&(pVCB->XixcoreVcb),
							pFCB->XixcoreFcb.LotNumber, 
							&pFCB->XixcoreFcb.HasLock, 
							1, 
							0
							);
						
					if(!NT_SUCCESS(RC)){
						RC = STATUS_ACCESS_DENIED;
						try_return(RC);
					}
					
					
				}
			}

	
			XifsdAcquireFcbExclusive(TRUE, pTargetParentFCB, FALSE);
			FountLCB = xixfs_FCBTLBFindPrefix(pIrpContext,
								&pTargetParentFCB,
								&TargetFileName,
								FALSE);

			
			XifsdReleaseFcb(TRUE,pTargetParentFCB);

			if(FountLCB){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Alread Exist File Name (%wZ)\n", &TargetFileName));
				RC = STATUS_OBJECT_NAME_INVALID;
				try_return(RC);
			}
			

			RC = xixfs_FileIsExistChecChild(
							pIrpContext,
							pTargetParentFCB,
							&TargetFileName
							);

			
			if(NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Alread Exist File Name (%wZ)\n", &TargetFileName));
				RC = STATUS_OBJECT_NAME_INVALID;
				try_return(RC);
			}


			XifsdAcquireFcbExclusive(TRUE, pParentFCB, FALSE);
			RC = xixfs_FileUpdateParentChild(pIrpContext, 
					pParentFCB,
					&TargetFileName, 
					&pLCB->FileName);

			if(!NT_SUCCESS(RC)){
				XifsdReleaseFcb(TRUE,pParentFCB);
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("FAIL xixfs_FileUpdateParentChild (%wZ)\n", &TargetFileName));

				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}

			XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_NAME);
			XIXCORE_SET_FLAGS(pFileObject->Flags, FO_FILE_MODIFIED);
			RC = xixfs_FileUpdateFileName(pIrpContext, 
								pFCB,
								pFCB->XixcoreFcb.ParentLotNumber,
								(uint8 *)TargetFileName.Buffer, 
								(uint32)TargetFileName.Length
								);
			
			if(!NT_SUCCESS(RC)){
				XifsdReleaseFcb(TRUE,pFCB);
				XifsdReleaseFcb(TRUE,pParentFCB);
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("FAIL xixfs_FileUpdateFileName (%wZ)\n", &TargetFileName));
				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}

			xixfs_FCBTLBRemovePrefix(TRUE, pLCB);

			pTargetLCB = xixfs_FCBTLBInsertPrefix(pIrpContext, pFCB, &TargetFileName, pParentFCB);
			XifsdReleaseFcb(TRUE,pFCB);
			XifsdReleaseFcb(TRUE,pParentFCB);

			pCCB->PtrLCB = pTargetLCB;

	

			if(pFCB->FCBFullPath.Buffer != NULL){
				uint16	ReCalcLen = 0;

				uint8	* tmpBuffer = NULL;
				//ReCalcLen = (uint16)(pFCB->FCBFullPath.Length - pFCB->FCBTargetOffset + TargetFileName.Length);
				ReCalcLen = (uint16)(pFCB->FCBTargetOffset + TargetFileName.Length);

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
			



			xixfs_NotifyReportChangeToXixfs(
						pFCB,
						((pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE)?FILE_NOTIFY_CHANGE_FILE_NAME: FILE_NOTIFY_CHANGE_DIR_NAME ),
						FILE_ACTION_RENAMED_NEW_NAME
						);
				

					
		}else{
			
			FileName.Buffer = (PWSTR)pFCB->XixcoreFcb.FCBName;
			FileName.Length=FileName.MaximumLength= (uint16)pFCB->XixcoreFcb.FCBNameLength;

			if(pTargetParentFCB->XixcoreFcb.FCBType != FCB_TYPE_DIR) 
			{
				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}

			if(pTargetParentFCB == pParentFCB){
				if(!RtlCompareUnicodeString(&FileName, &TargetFileName, TRUE)){
					// Same String do nothing
					RC = STATUS_SUCCESS;
					try_return(RC);
				}

				goto up_same;	
				
			}

			XifsdAcquireFcbExclusive(TRUE, pTargetParentFCB, FALSE);
			FountLCB = xixfs_FCBTLBFindPrefix(pIrpContext,
								&pTargetParentFCB,
								&TargetFileName,
								FALSE);
			
			XifsdReleaseFcb(TRUE,pTargetParentFCB);
			if(FountLCB){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Alread Exist File Name (%wZ)\n", &TargetFileName));
				RC = STATUS_OBJECT_NAME_COLLISION;
				try_return(RC);
			}


			RC = xixfs_FileIsExistChecChild(
							pIrpContext,
							pTargetParentFCB,
							&TargetFileName
							);

			
			if(NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Alread Exist File Name (%wZ)\n", &TargetFileName));
				RC = STATUS_OBJECT_NAME_INVALID;
				try_return(RC);
			}



			//
			//	Replacement is not allowed
			//

			XifsdAcquireFcbExclusive(TRUE, pParentFCB, FALSE);
			RC = xixfs_FileDeleteParentChild(pIrpContext, pParentFCB, &pLCB->FileName);

			XifsdReleaseFcb(TRUE,pParentFCB);
			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Fail xixfs_FileDeleteParentChild (%wZ)\n", &TargetFileName));
				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}

	


			xixfs_NotifyReportChangeToXixfs(
							pFCB,
							FILE_NOTIFY_CHANGE_ATTRIBUTES
								   | FILE_NOTIFY_CHANGE_SIZE
								   | FILE_NOTIFY_CHANGE_LAST_WRITE
								   | FILE_NOTIFY_CHANGE_LAST_ACCESS
								   | FILE_NOTIFY_CHANGE_CREATION,
							FILE_ACTION_MODIFIED
							);

	


			XifsdAcquireFcbExclusive(TRUE, pTargetParentFCB, FALSE);
			RC = xixfs_FileAddParentChild(pIrpContext, 
									pTargetParentFCB, 
									pFCB->XixcoreFcb.LotNumber, 
									pFCB->XixcoreFcb.Type, 
									&TargetFileName); 

			XifsdReleaseFcb(TRUE,pTargetParentFCB);
			
			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Fail xixfs_FileAddParentChild  (%wZ)\n", &TargetFileName));
				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}

			XifsdAcquireFcbExclusive(TRUE, pParentFCB, FALSE);
			XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_NAME);
			XIXCORE_SET_FLAGS(pFileObject->Flags, FO_FILE_MODIFIED);
			RC = xixfs_FileUpdateFileName(pIrpContext, 
								pFCB,
								pTargetParentFCB->XixcoreFcb.LotNumber,
								(uint8 *)TargetFileName.Buffer, 
								TargetFileName.Length);
			
			if(!NT_SUCCESS(RC)){
				XifsdReleaseFcb(TRUE,pFCB);
				XifsdReleaseFcb(TRUE,pParentFCB);
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("FAIL xixfs_FileUpdateFileName (%wZ)\n", &TargetFileName));
				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}

		
			xixfs_FCBTLBRemovePrefix(TRUE, pLCB);

			//
			//  Now Decrement the reference counts for the parent and drop the Vcb.
			//
			XifsdLockVcb(pIrpContext,pVCB);
			DebugTrace( DEBUG_LEVEL_INFO, (DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
						("XifsdSetRenameInformation, PFcb (%I64d) Vcb %d/%d Fcb %d/%d\n", 
						pParentFCB->XixcoreFcb.LotNumber,
						 pVCB->VCBReference,
						 pVCB->VCBUserReference,
						 pParentFCB->FCBReference,
						 pParentFCB->FCBUserReference ));

			XifsdDecRefCount( pParentFCB, 1, 1 );

			DebugTrace( DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
						("XifsdSetRenameInformation, PFcb (%I64d) Vcb %d/%d Fcb %d/%d\n", 
						pParentFCB->XixcoreFcb.LotNumber,
						 pVCB->VCBReference,
						 pVCB->VCBUserReference,
						 pParentFCB->FCBReference,
						 pParentFCB->FCBUserReference ));

			XifsdUnlockVcb( pIrpContext, pVCB );

			XifsdReleaseFcb(TRUE,pFCB);
			XifsdReleaseFcb(TRUE,pParentFCB);

			XifsdAcquireFcbExclusive(TRUE, pTargetParentFCB, FALSE);
			XifsdAcquireFcbExclusive(TRUE, pFCB, FALSE);	
			pTargetLCB = xixfs_FCBTLBInsertPrefix(pIrpContext, pFCB, &TargetFileName, pTargetParentFCB);

			XifsdLockVcb(pIrpContext,pVCB);
			DebugTrace( DEBUG_LEVEL_INFO, (DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
						("XifsdSetRenameInformation, PFcb LotNumber(%I64d) Vcb %d/%d Fcb %d/%d\n", 
							pTargetParentFCB->XixcoreFcb.LotNumber,
							pVCB->VCBReference,
							pVCB->VCBUserReference,
							pTargetParentFCB->FCBReference,
							pTargetParentFCB->FCBUserReference ));

			XifsdIncRefCount( pTargetParentFCB, 1, 1 );

			DebugTrace( DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
						 ("XifsdSetRenameInformation, PFcb LotNumber(%I64d) Vcb %d/%d Fcb %d/%d\n",
						 pTargetParentFCB->XixcoreFcb.LotNumber,
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
				pFCB->FCBTargetOffset = xixfs_SearchLastComponetOffsetOfName(&pFCB->FCBFullPath);
			}

			xixfs_NotifyReportChangeToXixfs(
								pFCB,
								((pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE)
									?FILE_NOTIFY_CHANGE_FILE_NAME
									: FILE_NOTIFY_CHANGE_DIR_NAME ),
									FILE_ACTION_ADDED
								);
		
		}

		;
	}finally{

		if(NT_SUCCESS(RC)){
			xixfs_SendRenameLinkBC(
				TRUE,
				XIXFS_SUBTYPE_FILE_RENAME,
				pVCB->XixcoreVcb.HostMac,
				pFCB->XixcoreFcb.LotNumber,
				pVCB->XixcoreVcb.VolumeId,
				OldParentLotNumber,
				NewParentLotNumber
			);
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit xixfs_SetRenameInformation.\n"));

	return RC;
}



NTSTATUS
xixfs_SetLinkInfo(
	IN	PXIXFS_IRPCONTEXT	pIrpContext,
	IN	PFILE_OBJECT		pFileObject,
	IN	PXIXFS_FCB			pFCB,
	IN	PXIXFS_CCB			pCCB,
	IN	PFILE_LINK_INFORMATION pLinkInfomation,
	IN	uint32				Length
)
{
	NTSTATUS RC = STATUS_SUCCESS;
	PIRP		pIrp = NULL;
	PIO_STACK_LOCATION pIrpSp = NULL;
	PFILE_OBJECT			TargetFileObject = NULL;
	PXIXFS_VCB				pVCB = NULL;
	PXIXFS_FCB 				pTargetParentFCB;
	PXIXFS_CCB				pTargetParentCCB;
	PXIXFS_LCB				pTargetLCB;


	USHORT 					PreviousLength;
	USHORT 					LastFileNameOffset;

	UNICODE_STRING			TargetFileName;
	PXIXFS_FCB				pParentFCB = NULL;
	PXIXFS_LCB				pLCB = NULL;
	PXIXFS_LCB				FoundLCB = NULL;	
	
	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter xixfs_SetLinkInfo.\n"));

	ASSERT(pIrpContext);
	ASSERT(pIrpContext->Irp);
	ASSERT_FCB(pFCB);
	ASSERT_CCB(pCCB);
	pLCB = pCCB->PtrLCB;

	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	// Added by ILGU HONG for readonly 09052006
	if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
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


	//if(!XIXCORE_TEST_FLAGS(pCCB->CCBFlags, XIXFSD_CCB_OPENED_AS_FILE)){
	//	return STATUS_INVALID_PARAMETER;
	//}

	// Not allowed to create link to Dir
	if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR){
		return STATUS_INVALID_PARAMETER;
	}
	
	if(XIXCORE_TEST_FLAGS(pLCB->LCBFlags, (XIFSD_LCB_STATE_DELETE_ON_CLOSE|XIFSD_LCB_STATE_LINK_IS_GONE)))
	{
		return STATUS_ACCESS_DENIED;
	}

	if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_VOLUME){
		return STATUS_ACCESS_DENIED;
	}

	if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_CHANGE_DELETED)){
			return STATUS_INVALID_DEVICE_REQUEST;
	}

	if(pFCB->XixcoreFcb.HasLock != FCB_FILE_LOCK_HAS){
		return STATUS_ACCESS_DENIED;
	}


	TargetFileObject = pIrpSp->Parameters.SetFile.FileObject;
	if(TargetFileObject == NULL){
		pTargetParentFCB = pParentFCB;
		xixfs_GetTargetInfo2(pLinkInfomation->FileName,pLinkInfomation->FileNameLength, &TargetFileName);
	}else{
		xixfs_DecodeFileObject(TargetFileObject, &pTargetParentFCB, &pTargetParentCCB);
		ASSERT_FCB(pTargetParentFCB);
		xixfs_GetTargetInfo(TargetFileObject, &TargetFileName);
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
	

			FoundLCB = xixfs_FCBTLBFindPrefix(pIrpContext,
								&pTargetParentFCB,
								&TargetFileName,
								FALSE);


			if(FoundLCB){
				try_return(RC = STATUS_OBJECT_NAME_INVALID);
			}


			RC = xixfs_FileIsExistChecChild(
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
			pFCB->XixcoreFcb.LinkCount ++;
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_LINKCOUNT);
			XIXCORE_SET_FLAGS(pFileObject->Flags, FO_FILE_MODIFIED);
			xixfs_UpdateFCB(pFCB);

			pTargetLCB = xixfs_FCBTLBInsertPrefix(pIrpContext, pFCB, &TargetFileName, pTargetParentFCB);
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


		FoundLCB = xixfs_FCBTLBFindPrefix(pIrpContext,
							&pTargetParentFCB,
							&TargetFileName,
							FALSE);


		if(FoundLCB){
			try_return(RC = STATUS_OBJECT_NAME_INVALID);
		}


		RC = xixfs_FileIsExistChecChild(
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
		pFCB->XixcoreFcb.LinkCount ++;
		XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_LINKCOUNT);
		XIXCORE_SET_FLAGS(pFileObject->Flags, FO_FILE_MODIFIED);
		xixfs_UpdateFCB(pFCB);

		pTargetLCB = xixfs_FCBTLBInsertPrefix(pIrpContext, pFCB, &TargetFileName, pTargetParentFCB);


		XifsdLockVcb(pIrpContext,pVCB);
		DebugTrace( DEBUG_LEVEL_INFO, (DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
					("XifsdSetLinkInformation, PFcb  LotNumber(%I64d) Vcb %d/%d Fcb %d/%d\n", 
					pTargetParentFCB->XixcoreFcb.LotNumber,
					 pVCB->VCBReference,
					 pVCB->VCBUserReference,
					 pTargetParentFCB->FCBReference,
					 pTargetParentFCB->FCBUserReference ));

		XifsdIncRefCount( pTargetParentFCB, 1, 1 );

		DebugTrace( DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_REFCOUNT),
					 ("XifsdSetLinkInformation, PFcb  LotNumber(%I64d) Vcb %d/%d Fcb %d/%d\n",
					 pTargetParentFCB->XixcoreFcb.LotNumber,
					 pVCB->VCBReference,
					 pVCB->VCBUserReference,
					 pTargetParentFCB->FCBReference,
					 pTargetParentFCB->FCBUserReference ));

		XifsdUnlockVcb( pIrpContext, pVCB );

		XifsdReleaseFcb(TRUE,pFCB);
		XifsdReleaseFcb(TRUE,pTargetParentFCB);
		
		
	}finally{
		;
	}

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit xixfs_SetLinkInfo.\n"));

	return RC;
}


NTSTATUS
xixfs_SetAllocationSize(	
	IN	PXIXFS_IRPCONTEXT	pIrpContext,
	IN	PFILE_OBJECT		pFileObject,
	IN	PXIXFS_FCB		pFCB,
	IN	PXIXFS_CCB		pCCB,
	IN 	PFILE_ALLOCATION_INFORMATION pAllocationInformation,
	IN	uint32			Length
	)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	PXIXFS_VCB		pVCB = NULL;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter xixfs_SetAllocationSize.\n"));

	if(Length < sizeof(FILE_ALLOCATION_INFORMATION)){
		return STATUS_INVALID_PARAMETER;
	}
	

	

	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	// Added by ILGU HONG for readonly 09052006
	if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
		return STATUS_MEDIA_WRITE_PROTECTED;
	}
	// Added by ILGU HONG for readonly end


	if(pFCB->XixcoreFcb.FCBType != FCB_TYPE_FILE){
		return STATUS_INVALID_PARAMETER;
	}


	XifsdLockFcb(pIrpContext, pFCB);


	try{

		


		if(IoGetCurrentIrpStackLocation(pIrpContext->Irp)->Parameters.SetFile.AdvanceOnly){
				DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
				(" File.AdvanceOnly .\n"));	
				

			if((uint64)(pAllocationInformation->AllocationSize.QuadPart) > pFCB->XixcoreFcb.RealAllocationSize)
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
				

				CurentEndIndex = xixcore_GetIndexOfLogicalAddress(pVCB->XixcoreVcb.LotSize, (pFCB->XixcoreFcb.RealAllocationSize-1));
				EndLotIndex = xixcore_GetIndexOfLogicalAddress(pVCB->XixcoreVcb.LotSize, Offset);
				
				if(EndLotIndex > CurentEndIndex){
					
					LotCount = EndLotIndex - CurentEndIndex;
					
					XifsdUnlockFcb(NULL, pFCB);
					RC = xixcore_AddNewLot(
								&(pVCB->XixcoreVcb), 
								&(pFCB->XixcoreFcb), 
								CurentEndIndex, 
								LotCount, 
								&AllocatedLotCount,
								pFCB->XixcoreFcb.AddrLot,
								pFCB->XixcoreFcb.AddrLotSize,
								&(pFCB->XixcoreFcb.AddrStartSecIndex)
								);
					
					XifsdLockFcb(NULL, pFCB);
					if(!NT_SUCCESS(RC)){
						try_return(RC);
					}
					pFCB->AllocationSize.QuadPart = pFCB->XixcoreFcb.RealAllocationSize;
					
				}

				if(CcIsFileCached(pFileObject)){
					CcSetFileSizes(pFileObject, (PCC_FILE_SIZES)&pFCB->AllocationSize);
					//DbgPrint(" 6 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);
					
					XIXCORE_SET_FLAGS(pFileObject->Flags, FO_FILE_MODIFIED);
				}

			}


			/*
			if((uint64)pAllocationInformation->AllocationSize.QuadPart>= (uint64)pFCB->FileSize.QuadPart){ 
					pFCB->AllocationSize.QuadPart = pAllocationInformation->AllocationSize.QuadPart;
				

				CcSetFileSizes(pFileObject, (PCC_FILE_SIZES)&pFCB->AllocationSize);
				DbgPrint(" 6 Changed File Size (%I64d) .\n", pFCB->FileSize.QuadPart);
				
				XIXCORE_SET_FLAGS(pFileObject->Flags, FO_FILE_MODIFIED);
			}
			*/

		}else{


			if((uint64)(pAllocationInformation->AllocationSize.QuadPart) > pFCB->XixcoreFcb.RealAllocationSize)
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
				

				CurentEndIndex = xixcore_GetIndexOfLogicalAddress(pVCB->XixcoreVcb.LotSize, (pFCB->XixcoreFcb.RealAllocationSize-1));
				EndLotIndex = xixcore_GetIndexOfLogicalAddress(pVCB->XixcoreVcb.LotSize, Offset);
				
				if(EndLotIndex > CurentEndIndex){
					
					LotCount = EndLotIndex - CurentEndIndex;
					
					XifsdUnlockFcb(NULL, pFCB);
					RC = xixcore_AddNewLot(
								&(pVCB->XixcoreVcb), 
								&(pFCB->XixcoreFcb), 
								CurentEndIndex, 
								LotCount, 
								&AllocatedLotCount,
								pFCB->XixcoreFcb.AddrLot,
								pFCB->XixcoreFcb.AddrLotSize,
								&(pFCB->XixcoreFcb.AddrStartSecIndex)
								);
					
					XifsdLockFcb(NULL, pFCB);
					if(!NT_SUCCESS(RC)){
						try_return(RC);
					}
					pFCB->AllocationSize.QuadPart = pFCB->XixcoreFcb.RealAllocationSize;
				}

				if(CcIsFileCached(pFileObject)){
					CcSetFileSizes(pFileObject, (PCC_FILE_SIZES)&pFCB->AllocationSize);
					//DbgPrint(" 6-1 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);
					
					XIXCORE_SET_FLAGS(pFileObject->Flags, FO_FILE_MODIFIED);
				}

			}else{
				if(CcIsFileCached(pFileObject)){
					if(!MmCanFileBeTruncated(&(pFCB->SectionObject), 
								&(pAllocationInformation->AllocationSize)))
					{
					
						RC = STATUS_USER_MAPPED_FILE;
						try_return(RC);
					}

					if(CcIsFileCached(pFileObject)){
						CcSetFileSizes(pFileObject, (PCC_FILE_SIZES)&pFCB->AllocationSize);
						//DbgPrint(" 7 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);
						XIXCORE_SET_FLAGS(pFileObject->Flags, FO_FILE_MODIFIED);
					}
		

				}



		
				/*
				pFCB->AllocationSize.QuadPart =pAllocationInformation->AllocationSize.QuadPart;
				CcSetFileSizes(pFileObject, (PCC_FILE_SIZES)&pFCB->AllocationSize);
				DbgPrint(" 7 Changed File Size (%I64d) .\n",pFCB->FileSize.QuadPart);
				XIXCORE_SET_FLAGS(pFileObject->Flags, FO_FILE_MODIFIED);
				*/
			}
	

		}


		

		DebugTrace(DEBUG_LEVEL_CRITICAL,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB), 
						("New XifsdSetAllocationSize = (%I64d).\n", pFCB->FileSize.QuadPart));


		



		xixfs_NotifyReportChangeToXixfs(
						pFCB,
						FILE_NOTIFY_CHANGE_SIZE,
						FILE_ACTION_MODIFIED
						);


		;
	}finally{
		XifsdUnlockFcb(pIrpContext, pFCB);
	}

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit xixfs_SetAllocationSize.\n"));
	return RC;
}


NTSTATUS
xixfs_SetEndofFileInfo(	
	IN	PXIXFS_IRPCONTEXT	pIrpContext,
	IN	PFILE_OBJECT		pFileObject,
	IN	PXIXFS_FCB		pFCB,
	IN	PXIXFS_CCB		pCCB,
	IN	PFILE_END_OF_FILE_INFORMATION	pEndOfFileInformation,
	IN	uint32							Length
	)
{
	NTSTATUS		RC = STATUS_SUCCESS;
	PXIXFS_VCB		pVCB = NULL;
	BOOLEAN			IsChangeLenProcessing = FALSE;

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter xixfs_SetEndofFileInfo.\n"));

	if(Length < sizeof(FILE_END_OF_FILE_INFORMATION)){
		return STATUS_INVALID_PARAMETER;
	}

	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	//	Added by ILGU HONG for readonly 09052006
	if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
		return STATUS_MEDIA_WRITE_PROTECTED;
	}
	//	Added by ILGU HONG for readonly end


	if(pFCB->XixcoreFcb.FCBType != FCB_TYPE_FILE){
		return STATUS_INVALID_PARAMETER;
	}

	XifsdLockFcb(pIrpContext, pFCB);


	


	try{

		//DbgPrint(" EndOfFile (%wZ) Size Request EOF(0x%I64X) ADVANCE ONFY(%s).\n", 
		//	&pFCB->FCBFullPath, 
		//	pEndOfFileInformation->EndOfFile.QuadPart,
		//	((IoGetCurrentIrpStackLocation(pIrpContext->Irp)->Parameters.SetFile.AdvanceOnly)?"TRUE":"FALSE"));
		
		if(IoGetCurrentIrpStackLocation(pIrpContext->Irp)->Parameters.SetFile.AdvanceOnly){
			LARGE_INTEGER	NewFileSize ;
			LARGE_INTEGER	NewValidDataLength ;	

				DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
				(" File.AdvanceOnly .\n"));		
			


				
			if((uint64)(pEndOfFileInformation->EndOfFile.QuadPart) > pFCB->XixcoreFcb.RealAllocationSize)
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
				

				CurentEndIndex = xixcore_GetIndexOfLogicalAddress(pVCB->XixcoreVcb.LotSize, (pFCB->XixcoreFcb.RealAllocationSize-1));
				EndLotIndex = xixcore_GetIndexOfLogicalAddress(pVCB->XixcoreVcb.LotSize, Offset);
				
				if(EndLotIndex > CurentEndIndex){
					
					LotCount = EndLotIndex - CurentEndIndex;
					
					XifsdUnlockFcb(NULL, pFCB);
					RC = xixcore_AddNewLot(
								&(pVCB->XixcoreVcb), 
								&(pFCB->XixcoreFcb), 
								CurentEndIndex, 
								LotCount, 
								&AllocatedLotCount,
								pFCB->XixcoreFcb.AddrLot,
								pFCB->XixcoreFcb.AddrLotSize,
								&(pFCB->XixcoreFcb.AddrStartSecIndex)
								);
					
					XifsdLockFcb(NULL, pFCB);
					if(!NT_SUCCESS(RC)){
						try_return(RC);
					}
					pFCB->AllocationSize.QuadPart = pFCB->XixcoreFcb.RealAllocationSize;
					
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
				pFCB->FileSize.QuadPart = pEndOfFileInformation->EndOfFile.QuadPart;
				XIXCORE_SET_FLAGS(pFileObject->Flags, FO_FILE_SIZE_CHANGED);
				if(CcIsFileCached(pFileObject)){
					CcSetFileSizes(pFileObject, (PCC_FILE_SIZES)&pFCB->AllocationSize);
					//DbgPrint(" 8 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);
				}

				IsChangeLenProcessing = TRUE;

				DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
				(" 8 Changed File Size (%I64d) .\n", pFCB->FileSize.QuadPart));
			}


					
		}else{
			//DbgPrint("Set File Size !!! FileSize(%I64d) Request Size(%I64d)\n", pFCB->FileSize.QuadPart, pEndOfFileInformation->EndOfFile.QuadPart);


			if((uint64)(pEndOfFileInformation->EndOfFile.QuadPart) > pFCB->XixcoreFcb.RealAllocationSize)
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
				

				CurentEndIndex = xixcore_GetIndexOfLogicalAddress(pVCB->XixcoreVcb.LotSize, (pFCB->XixcoreFcb.RealAllocationSize-1));
				EndLotIndex = xixcore_GetIndexOfLogicalAddress(pVCB->XixcoreVcb.LotSize, Offset);
				
				if(EndLotIndex > CurentEndIndex){
					
					LotCount = EndLotIndex - CurentEndIndex;
					
					XifsdUnlockFcb(NULL, pFCB);
					RC = xixcore_AddNewLot(
								&(pVCB->XixcoreVcb), 
								&(pFCB->XixcoreFcb), 
								CurentEndIndex, 
								LotCount, 
								&AllocatedLotCount,
								pFCB->XixcoreFcb.AddrLot,
								pFCB->XixcoreFcb.AddrLotSize,
								&(pFCB->XixcoreFcb.AddrStartSecIndex)
								);
					
					XifsdLockFcb(NULL, pFCB);
					if(!NT_SUCCESS(RC)){
						try_return(RC);
					}

					pFCB->AllocationSize.QuadPart = pFCB->XixcoreFcb.RealAllocationSize;
					
				}

			}

			if((uint64)pEndOfFileInformation->EndOfFile.QuadPart >= (uint64)pFCB->FileSize.QuadPart){
					pFCB->FileSize.QuadPart = pEndOfFileInformation->EndOfFile.QuadPart ;
					IsChangeLenProcessing = TRUE;
	
			}else {

				if(CcIsFileCached(pFileObject)){
					if(!MmCanFileBeTruncated(&(pFCB->SectionObject), 
								&(pEndOfFileInformation->EndOfFile)))
					{
						//DbgPrint(" ilgu Error!!");
						RC = STATUS_USER_MAPPED_FILE;
						try_return(RC);
					}
				}

				
				pFCB->FileSize.QuadPart = pEndOfFileInformation->EndOfFile.QuadPart ;
				if(pFCB->ValidDataLength.QuadPart > pFCB->FileSize.QuadPart){
					//DbgPrint("Reduce Valid Data Length");
					pFCB->ValidDataLength.QuadPart = pFCB->FileSize.QuadPart;
				}
				IsChangeLenProcessing = TRUE;
			}

			if(CcIsFileCached(pFileObject)){
				CcSetFileSizes(pFileObject, (PCC_FILE_SIZES)&pFCB->AllocationSize);
				//DbgPrint(" 9 Changed File (%wZ) Size (%I64d) .\n", &pFCB->FCBFullPath, pFCB->FileSize.QuadPart);
			}

			XIXCORE_SET_FLAGS(pFileObject->Flags, FO_FILE_SIZE_CHANGED);
			


				
				DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
				(" 8-2 Changed File Size (%I64d) .\n", pFCB->FileSize.QuadPart));

		}
		
		DebugTrace(DEBUG_LEVEL_CRITICAL,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),
						("New XifsdSetEndofFileInfo = (%I64d).\n", pFCB->FileSize.QuadPart));


		if(IsChangeLenProcessing == TRUE){
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);




			xixfs_NotifyReportChangeToXixfs(
								pFCB,
								FILE_NOTIFY_CHANGE_SIZE,
								FILE_ACTION_MODIFIED
								);

		}


		;
	}finally{
		XifsdUnlockFcb(pIrpContext, pFCB);
	}

	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit xixfs_SetEndofFileInfo.\n"));
	return RC;
}


NTSTATUS
xixfs_CommonSetInformation(
	IN PXIXFS_IRPCONTEXT pIrpContext
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

	PXIXFS_FCB	pFCB = NULL;
	PXIXFS_CCB	pCCB = NULL;
	PXIXFS_VCB	pVCB = NULL;
	
	BOOLEAN					VCBResourceAcquired = FALSE;	
	BOOLEAN					MainResourceAcquired = FALSE;
	BOOLEAN					CanWait = FALSE;
	BOOLEAN					PostRequest = FALSE;
	BOOLEAN					AcquireFCB = FALSE;
	BOOLEAN					AcquireVCB = FALSE;
	uint32					ReturnedBytes = 0;
	

	PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Enter xixfs_CommonSetInformation.\n"));
	
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


	TypeOfOpen = xixfs_DecodeFileObject(pFileObject, &pFCB, &pCCB);
	CanWait = XIXCORE_TEST_FLAGS(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);





	DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB), 
					("FileInformationClass = (0x%x).\n", FileInformationClass));
	if(!CanWait){
			DebugTrace(DEBUG_LEVEL_INFO,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_IRPCONTEXT), 
					("Post Request IrpContext(%p) Irp(%p).\n", pIrpContext, pIrp));
		RC = xixfs_PostRequest(pIrpContext, pIrp);
		return RC;
	}





	if( (TypeOfOpen <=UserVolumeOpen)
		|| (TypeOfOpen == UnopenedFileObject) 
		)
	{
		RC = STATUS_INVALID_PARAMETER;
		xixfs_CompleteRequest(pIrpContext, RC , 0);
		return RC;
	}


	ASSERT_FCB(pFCB);
	
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);

	ASSERT_CCB(pCCB);

	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!SetInformation pCCB(%p)\n", pCCB));


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

		xixfs_VerifyFCBOperation( pIrpContext, pFCB );

		if((TypeOfOpen == UserFileOpen) || (TypeOfOpen == UserDirectoryOpen ) )
		{
			
			switch(FileInformationClass){
				case FileBasicInformation:
				{
					

					RC = xixfs_SetBasicInformation(pIrpContext,
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
				

					RC = xixfs_SetPositionInformation(pIrpContext,
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
					

					RC = xixfs_SetDispositionInformation(pIrpContext,
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
					

					RC = xixfs_SetRenameInformation(pIrpContext,
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
					

					RC = xixfs_SetLinkInfo(pIrpContext,
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
					

					RC = xixfs_SetAllocationSize(pIrpContext,
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
					

					RC = xixfs_SetEndofFileInfo(pIrpContext,
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


		;
	}finally{
		if(AcquireFCB){
			XifsdReleaseFcb(CanWait, pFCB);
		}

		if(AcquireVCB){
			XifsdReleaseVcb(CanWait, pVCB);
		}
	}

	xixfs_CompleteRequest(pIrpContext, RC, ReturnedBytes);
	
	DebugTrace(DEBUG_LEVEL_TRACE,(DEBUG_TARGET_FILEINFO|DEBUG_TARGET_FCB),  
		("Exit XXixFsdCommonSetInformation.\n"));
	return RC;
}