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
XixFsdInitializeEnumeration (
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PIO_STACK_LOCATION pIrpSp,
	IN PXIFS_FCB pFCB,
	IN OUT PXIFS_CCB pCCB,
	IN OUT PXIFS_DIR_EMUL_CONTEXT DirContext,
	OUT PBOOLEAN ReturnNextEntry,
	OUT PBOOLEAN ReturnSingleEntry,
	OUT PBOOLEAN InitialQuery
);

NTSTATUS
XixFsdNameInExpression (
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PUNICODE_STRING CurrentName,
	IN PUNICODE_STRING SearchExpression,
	IN BOOLEAN Wild
);

BOOLEAN
XixFsdEnumerateIndex (
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PXIFS_CCB Ccb,
	IN OUT PXIFS_DIR_EMUL_CONTEXT DirContext,
	IN BOOLEAN ReturnNextEntry
);

NTSTATUS
XixFsdQueryDirectory(
	IN PXIFS_IRPCONTEXT		pIrpContext, 
	IN PIRP					pIrp, 
	IN PIO_STACK_LOCATION 	pIrpSp, 
	IN PFILE_OBJECT			pFileObject, 
	IN PXIFS_FCB			pFCB, 
	IN PXIFS_CCB			pCCB
);

BOOLEAN
XixFsdNotifyCheck (
    IN PXIFS_CCB pCCB,
    IN PXIFS_FCB pFCB,
    IN PSECURITY_SUBJECT_CONTEXT SubjectContext
);

NTSTATUS
XixFsdNotifyDirectory(
	IN PXIFS_IRPCONTEXT		pIrpContext, 
	IN PIRP					pIrp, 
	IN PIO_STACK_LOCATION 	pIrpSp, 
	IN PFILE_OBJECT			pFileObject, 
	IN PXIFS_FCB			pFCB, 
	IN PXIFS_CCB			pCCB
);





#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsdInitializeEnumeration)
#pragma alloc_text(PAGE, XixFsdNameInExpression) 
#pragma alloc_text(PAGE, XixFsdEnumerateIndex) 
#pragma alloc_text(PAGE, XixFsdQueryDirectory)
#pragma alloc_text(PAGE, XixFsdNotifyCheck) 
#pragma alloc_text(PAGE, XixFsdNotifyDirectory)
#pragma alloc_text(PAGE, XixFsdCommonDirectoryControl)
#endif


NTSTATUS
XixFsdInitializeEnumeration (
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PIO_STACK_LOCATION pIrpSp,
	IN PXIFS_FCB pFCB,
	IN OUT PXIFS_CCB pCCB,
	IN OUT PXIFS_DIR_EMUL_CONTEXT DirContext,
	OUT PBOOLEAN ReturnNextEntry,
	OUT PBOOLEAN ReturnSingleEntry,
	OUT PBOOLEAN InitialQuery
)
{
	NTSTATUS Status;

	PUNICODE_STRING		FileName;
	UNICODE_STRING		SearchExpression;
	


	PUNICODE_STRING		RestartName = NULL;

	ULONG CcbFlags;
	BOOLEAN		LockFCB = FALSE;
	uint64		FileIndex;
	uint64		HighFileIndex;
	BOOLEAN		KnownIndex;

	NTSTATUS Found;

	//PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_DIRINFO, ("Enter XixFsdInitializeEnumeration .\n"));

	//
	//  Check inputs.
	//

	ASSERT_IRPCONTEXT( pIrpContext );
	ASSERT_FCB( pFCB );
	ASSERT_CCB( pCCB );




	// CCB is not set initial search
	if(!XifsdCheckFlagBoolean(pCCB->CCBFlags, XIFSD_CCB_FLAG_ENUM_INITIALIZED))
	{
		FileName = (PUNICODE_STRING) pIrpSp->Parameters.QueryDirectory.FileName;
		CcbFlags = 0;

		//
		//  If the filename is not specified or is a single '*' then we will
		//  match all names.
		//

		if ((FileName == NULL) ||
			(FileName->Buffer == NULL) ||
			(FileName->Length == 0) ||
			((FileName->Length == sizeof( WCHAR )) &&
			 (FileName->Buffer[0] == L'*'))) 
		{

			XifsdSetFlag( CcbFlags, XIFSD_CCB_FLAG_ENUM_MATCH_ALL);
			SearchExpression.Length =
			SearchExpression.MaximumLength = 0;
			SearchExpression.Buffer = NULL;



		} else {

			if (FileName->Length == 0) {

				XifsdRaiseStatus( pIrpContext, STATUS_INVALID_PARAMETER );
			}

			if (FsRtlDoesNameContainWildCards( FileName)) {

				XifsdSetFlag( CcbFlags, XIFSD_CCB_FLAG_ENUM_NAME_EXP_HAS_WILD );
			}

			SearchExpression.Buffer = FsRtlAllocatePoolWithTag( PagedPool,
																FileName->Length,
																TAG_EXP	 );

			SearchExpression.MaximumLength = FileName->Length;

	
			if(XifsdCheckFlagBoolean(pCCB->CCBFlags, XIFSD_CCB_FLAGS_IGNORE_CASE)){
				
				RtlDowncaseUnicodeString(&SearchExpression, FileName, FALSE);

			}else{
				RtlCopyMemory( SearchExpression.Buffer,
							   FileName->Buffer,
							   FileName->Length );
			}

			

			SearchExpression.Length = FileName->Length;
		}

		//
		//  But we do not want to return the constant "." and ".." entries for
		//  the root directory, for consistency with the rest of Microsoft's
		//  filesystems.
		//

		
		if (pFCB == pFCB->PtrVCB->RootDirFCB) 
		{
			XifsdSetFlag( CcbFlags, XIFSD_CCB_FLAG_ENUM_NOMATCH_CONSTANT_ENTRY );
		}
		

		XifsdLockFcb( pIrpContext, pFCB );
		LockFCB = TRUE;

		//
		//  Check again that this is the initial search.
		//

		if (!XifsdCheckFlagBoolean( pCCB->CCBFlags, XIFSD_CCB_FLAG_ENUM_INITIALIZED )) {

			//
			//  Update the values in the Ccb.
			//

			pCCB->currentFileIndex = 0;
			pCCB->SearchExpression = SearchExpression;

			XifsdSetFlag( pCCB->CCBFlags, (CcbFlags | XIFSD_CCB_FLAG_ENUM_INITIALIZED) );

		//
		//  Otherwise cleanup any buffer allocated here.
		//
		}else {

			if (!XifsdCheckFlagBoolean( CcbFlags, XIFSD_CCB_FLAG_ENUM_MATCH_ALL )) {

				ExFreePool( &SearchExpression.Buffer );
			}
		}

	} 
	else 
	{

		XifsdLockFcb( pIrpContext, pFCB );
		LockFCB = TRUE;
	}


	if ( XifsdCheckFlagBoolean( pIrpSp->Flags, SL_INDEX_SPECIFIED ) 
		&& (pIrpSp->Parameters.QueryDirectory.FileName != NULL)) 
	{

		KnownIndex = FALSE;
		FileIndex = pIrpSp->Parameters.QueryDirectory.FileIndex;

		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DIRINFO, ("SL_INDEX_SPECIFIED FileIndex(%I64d)\n", FileIndex));
		
		RestartName = (PUNICODE_STRING) pIrpSp->Parameters.QueryDirectory.FileName;
		*ReturnNextEntry = TRUE; 
		HighFileIndex = pCCB->highestFileIndex;

	//
	//  If we are restarting the scan then go from the self entry.
	//

	} else if (XifsdCheckFlagBoolean( pIrpSp->Flags, SL_RESTART_SCAN )) {

		KnownIndex = TRUE;
		FileIndex = 0;
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DIRINFO, ("SL_RESTART_SCAN FileIndex(%I64d)\n", FileIndex));
		*ReturnNextEntry = FALSE;

	} else { //SL_RETURN_SINGLE_ENTRY

		KnownIndex = TRUE;
		FileIndex = pCCB->currentFileIndex;
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DIRINFO,("pCCB->currentFileIndex FileIndex(%I64d)\n", FileIndex));
		*ReturnNextEntry = XifsdCheckFlagBoolean( pCCB->CCBFlags, XIFSD_CCB_FLAG_ENUM_RETURN_NEXT );
	}

	//
	//  Unlock the Fcb.
	//
	if(LockFCB){
		XifsdUnlockFcb( pIrpContext, pFCB );
		LockFCB = FALSE;
	}

	*InitialQuery = FALSE;

	if ((FileIndex == 0) 
		&& !(*ReturnNextEntry)) 
	{
		*InitialQuery = TRUE;
	}

	if (KnownIndex) 
	{

		Found = XixFsLookupInitialFileIndex( pIrpContext, pFCB, DirContext, FileIndex);

		if(!NT_SUCCESS( Found )){
			ASSERT(TRUE);
			return STATUS_NO_SUCH_FILE;
		}

	} 
	else 
	{
    
		if (XixFsdFullCompareNames( pIrpContext,
								 RestartName,
								 &XifsdUnicodeDirectoryNames[SELF_ENTRY] ) == EqualTo) 
		{

			FileIndex = 0;

			Found = XixFsLookupInitialFileIndex( pIrpContext, pFCB, DirContext, FileIndex);

			if(!NT_SUCCESS( Found )){
				ASSERT(TRUE);
				return Found;
			}
    
		} else {

			//
			//  See if we need the high water mark.
			//
        
			if (FileIndex == 0)
			{

				//
				//  We know that this is good.
				//
            
				FileIndex = Max( pCCB->highestFileIndex, 1 );
				KnownIndex = TRUE;
        
			}

			Found = XixFsLookupInitialFileIndex( pIrpContext, pFCB, DirContext, FileIndex );
        


			if (KnownIndex) {
            
				//
				//  Walk forward to discover an entry named per the caller's expectation.
				//
            
				while(1){

					Found = XixFsUpdateDirNames( pIrpContext, DirContext);
					if(!NT_SUCCESS( Found )){
						break;
					}

					if(XifsdCheckFlagBoolean(pCCB->CCBFlags, XIFSD_CCB_FLAGS_IGNORE_CASE)){
						

						if (RtlCompareUnicodeString( &(DirContext->ChildName),RestartName, TRUE ) == 0) 
						{
							break;
						}


					}else{

						if (RtlCompareUnicodeString(&(DirContext->ChildName),RestartName, FALSE ) == 0) 
						{

							break;
						}
					}



				}
        
			} else if (!NT_SUCCESS(Found)) {

				uint64	LastFileIndex;
				//
				//  Perform the search for the entry by index from the beginning of the physical directory.
				//

				LastFileIndex = 1;

				Found = XixFsLookupInitialFileIndex( pIrpContext, pFCB, DirContext, LastFileIndex);

				if(!NT_SUCCESS(Found)){
					ASSERT(TRUE);
					return Found;
				}


				while(1)
				{


					Found = XixFsUpdateDirNames( pIrpContext, DirContext);
					if(!NT_SUCCESS(Found)) {
						break;
					}

					if(DirContext->SearchedVirtualDirIndex > FileIndex){
						LastFileIndex = DirContext->SearchedVirtualDirIndex;
						break;
					}

				} 

				//
				//  If we didn't find the entry then go back to the last known entry.
				//

				if (!NT_SUCCESS(Found)) 
				{
					Found = XixFsLookupInitialFileIndex( pIrpContext, pFCB, DirContext, LastFileIndex );

					if(!NT_SUCCESS(Found)) {
						ASSERT(TRUE);
						return Found;
					}
				}
			}
		}
	}

	//
	//  Only update the dirent name if we will need it for some reason.
	//  Don't update this name if we are returning the next entry, and
	//  don't update it if it was already done.
	//

	if (!(*ReturnNextEntry) &&
		DirContext->ChildName.Buffer == NULL) {

		//
		//  If the caller specified an index that corresponds to a
		//  deleted file, they are trying to be tricky. Don't let them.
		//

		return STATUS_INVALID_PARAMETER;
    
	}

	//
	//  Look at the flag in the IrpSp indicating whether to return just
	//  one entry.
	//

	*ReturnSingleEntry = FALSE;

	if (XifsdCheckFlagBoolean( pIrpSp->Flags, SL_RETURN_SINGLE_ENTRY )) {

		*ReturnSingleEntry = TRUE;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_DIRINFO, ("Exit XixFsdInitializeEnumeration .\n"));

	return STATUS_SUCCESS;
}


NTSTATUS
XixFsdNameInExpression (
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PUNICODE_STRING CurrentName,
	IN PUNICODE_STRING SearchExpression,
	IN BOOLEAN Wild
)
{
	BOOLEAN Match = TRUE;

	//PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_DIRINFO, ("Enter XixFsdNameInExpression .\n"));
	//
	//  Check inputs.
	//

	ASSERT_IRPCONTEXT( IrpContext );

	//
	//  If there are wildcards in the expression then we call the
	//  appropriate FsRtlRoutine.
	//

	if (Wild) {

		Match = FsRtlIsNameInExpression( SearchExpression,
										 CurrentName,
										 FALSE,
										 NULL );

	//
	//  Otherwise do a direct memory comparison for the name string.
	//

	} else {

		if ((CurrentName->Length != SearchExpression->Length) ||
			(!RtlEqualMemory( CurrentName->Buffer,
							  SearchExpression->Buffer,
							  CurrentName->Length ))) {

			Match = FALSE;
		}
	}

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_DIRINFO, 
		("Exit XixFsdNameInExpression (%s) .\n", ((Match )?"MATCH":"NonMatch")));

	if(Match){
		return STATUS_SUCCESS;
	}else{
		return STATUS_UNSUCCESSFUL;
	}
}





BOOLEAN
XixFsdEnumerateIndex (
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PXIFS_CCB Ccb,
	IN OUT PXIFS_DIR_EMUL_CONTEXT DirContext,
	IN BOOLEAN ReturnNextEntry
)
{
	BOOLEAN Found = FALSE;
	NTSTATUS RC= STATUS_SUCCESS;
	UNICODE_STRING IgnoreTempName;
	uint8			*Buffer = NULL;
	//PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_DIRINFO, ("Enter XixFsdEnumerateIndex .\n"));

	//
	//  Check inputs.
	//

	ASSERT_IRPCONTEXT( IrpContext );
	ASSERT_CCB( Ccb );


	if(XifsdCheckFlagBoolean(Ccb->CCBFlags, XIFSD_CCB_FLAGS_IGNORE_CASE)){
		Buffer = ExAllocatePoolWithTag(NonPagedPool, 2048, TAG_BUFFER);
		if(!Buffer){
			return FALSE;
		}
		IgnoreTempName.Buffer = (PWSTR)Buffer;
		IgnoreTempName.MaximumLength = 2048;
	}

	//
	//  Loop until we find a match or exaust the directory.
	//

	while (TRUE) {

		RC= STATUS_SUCCESS;
		//
		//  Move to the next entry unless we want to consider the current
		//  entry.
		//

		if (ReturnNextEntry) {

			RC = XixFsUpdateDirNames( IrpContext,DirContext);

			if(!NT_SUCCESS(RC))
			{
				Found = FALSE;
				break;
			}
		} else {

			ReturnNextEntry = TRUE;
		}
        
		//
		//  Don't bother if we have a constant entry and are ignoring them.
		//
    
		
		
		if ((DirContext->SearchedVirtualDirIndex < 2 ) 
			&& XifsdCheckFlagBoolean( Ccb->CCBFlags, XIFSD_CCB_FLAG_ENUM_NOMATCH_CONSTANT_ENTRY )) 
		{
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DIRINFO, 
					("Root Dir Not Support parent and current dir entry \n"));
			continue;
		}

		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DIRINFO,
				("[find result] VIndex(%I64d): CCBFlags(0x%x): Ccb Exp(%wZ) : ChildName(%wZ)\n", 
				DirContext->SearchedRealDirIndex,Ccb->CCBFlags, &Ccb->SearchExpression,&DirContext->ChildName ));

		//
		//  If we match all names then return to our caller.
		//


		if (XifsdCheckFlagBoolean( Ccb->CCBFlags, XIFSD_CCB_FLAG_ENUM_MATCH_ALL )) {
			Found = TRUE;

			break;
		}

		//
		//  Check if the long name matches the search expression.
		//
		if(XifsdCheckFlagBoolean(Ccb->CCBFlags, XIFSD_CCB_FLAGS_IGNORE_CASE)){
			RtlZeroMemory(IgnoreTempName.Buffer, 2048);
			IgnoreTempName.Length = DirContext->ChildName.Length;
			RtlDowncaseUnicodeString(&IgnoreTempName, &(DirContext->ChildName), FALSE);

			RC = XixFsdNameInExpression( IrpContext,
									   &IgnoreTempName,
									   &Ccb->SearchExpression,
									   XifsdCheckFlagBoolean( Ccb->CCBFlags, XIFSD_CCB_FLAG_ENUM_NAME_EXP_HAS_WILD ));
		}else{
			RC = XixFsdNameInExpression( IrpContext,
									   &DirContext->ChildName,
									   &Ccb->SearchExpression,
									   XifsdCheckFlagBoolean( Ccb->CCBFlags, XIFSD_CCB_FLAG_ENUM_NAME_EXP_HAS_WILD ));
		}


		if (NT_SUCCESS(RC)) 
		{
			Found = TRUE;
			break;
		}
	}

	if(XifsdCheckFlagBoolean(Ccb->CCBFlags, XIFSD_CCB_FLAGS_IGNORE_CASE)){
		ExFreePool(IgnoreTempName.Buffer);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_DIRINFO, ("Exit XixFsdEnumerateIndex .\n"));

	return Found ;
}




NTSTATUS
XixFsdQueryDirectory(
	IN PXIFS_IRPCONTEXT		pIrpContext, 
	IN PIRP					pIrp, 
	IN PIO_STACK_LOCATION 	pIrpSp, 
	IN PFILE_OBJECT			pFileObject, 
	IN PXIFS_FCB			pFCB, 
	IN PXIFS_CCB			pCCB
	)
{
	NTSTATUS RC = STATUS_SUCCESS;
	ULONG Information = 0;

	ULONG LastEntry = 0;
	ULONG NextEntry = 0;

	ULONG FileNameBytes;
	ULONG BytesConverted;

	PXIFS_VCB	pVCB = NULL;

	XIFS_DIR_EMUL_CONTEXT DirContext;
	XIFS_FILE_EMUL_CONTEXT FileContext;
	PXIDISK_FILE_HEADER_LOT pFileHeader = NULL;

	BOOLEAN	CanWait = FALSE;

	uint64	PreviousFileIndex;
	uint64	ThisFid;

	BOOLEAN InitialQuery;
	BOOLEAN ReturnNextEntry;
	BOOLEAN ReturnSingleEntry;
	BOOLEAN Found = FALSE;
	BOOLEAN	SetInit = FALSE;
	BOOLEAN	AcquireVCB = FALSE;
	BOOLEAN	AcquireFCB = FALSE;
	

	BOOLEAN DirContextClean = FALSE;
	BOOLEAN FileContextClean = FALSE;

	PCHAR UserBuffer;
	ULONG BytesRemainingInBuffer;
	ULONG BaseLength;

	PFILE_BOTH_DIR_INFORMATION		DirInfo;
	PFILE_NAMES_INFORMATION			NamesInfo;
	PFILE_ID_FULL_DIR_INFORMATION	IdFullDirInfo;
	PFILE_ID_BOTH_DIR_INFORMATION	IdBothDirInfo;

	//PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_DIRINFO, ("Enter XixFsdQueryDirectory .\n"));

	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);


	CanWait = XifsdCheckFlagBoolean(pIrpContext->IrpContextFlags, XIFSD_IRP_CONTEXT_WAIT);

	
	if(CanWait != TRUE){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("XixFsdQueryDirectory Post request IrpContext %p .\n", pIrpContext));
		RC = XixFsdPostRequest(pIrpContext, pIrp);
		return RC;		
	}


	SetInit = (BOOLEAN)( (pCCB->SearchExpression.Buffer == NULL) &&
							(!XifsdCheckFlagBoolean( pCCB->CCBFlags, XIFSD_CCB_FLAG_ENUM_RETURN_NEXT )) );

	if(SetInit){
		XifsdAcquireVcbExclusive(CanWait, pVCB, FALSE);
		AcquireVCB = TRUE;
	}else{
		XifsdAcquireFcbShared(CanWait, pFCB, FALSE);
		AcquireFCB = TRUE;
	}

	
	
	DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
				 ("call XixFsdQueryDirectory 0x%x\n", pIrpSp->Parameters.QueryDirectory.FileInformationClass));


	switch (pIrpSp->Parameters.QueryDirectory.FileInformationClass) {

	case FileDirectoryInformation:

		BaseLength = FIELD_OFFSET( FILE_DIRECTORY_INFORMATION,
								   FileName[0] );
		break;

	case FileFullDirectoryInformation:

		BaseLength = FIELD_OFFSET( FILE_FULL_DIR_INFORMATION,
								   FileName[0] );
		break;

	case FileIdFullDirectoryInformation:

		BaseLength = FIELD_OFFSET( FILE_ID_FULL_DIR_INFORMATION,
								   FileName[0] );
		break;

	case FileNamesInformation:

		BaseLength = FIELD_OFFSET( FILE_NAMES_INFORMATION,
								   FileName[0] );
		break;

	case FileBothDirectoryInformation:

		BaseLength = FIELD_OFFSET( FILE_BOTH_DIR_INFORMATION,
								   FileName[0] );
		break;

	case FileIdBothDirectoryInformation:

		BaseLength = FIELD_OFFSET( FILE_ID_BOTH_DIR_INFORMATION,
								   FileName[0] );
		break;

	default:

		DebugTrace(DEBUG_LEVEL_ALL,DEBUG_TARGET_ALL , 
			("XixFsdQueryDirectory Name(%wZ) CCB(%p) Complete1 RC(%x).\n", &pFCB->FCBName, pCCB, RC));
	
		if(AcquireFCB){
			XifsdReleaseFcb(CanWait, pFCB);
		}
		
		if(AcquireVCB){
			XifsdReleaseVcb(CanWait, pVCB);
		}
			
		XixFsdCompleteRequest( pIrpContext, STATUS_INVALID_INFO_CLASS, 0 );

		return STATUS_INVALID_INFO_CLASS;
	}

	
	RC = XixFsInitializeDirContext(pVCB, pIrpContext, &DirContext);
	
	if(!NT_SUCCESS(RC)){
		DebugTrace(DEBUG_LEVEL_ALL,DEBUG_TARGET_ALL , 
			("XixFsdQueryDirectory Name(%wZ) CCB(%p) Complete2 RC(%x).\n", &pFCB->FCBName, pCCB, RC));
		
		if(AcquireFCB){
			XifsdReleaseFcb(CanWait, pFCB);
		}
		
		if(AcquireVCB){
			XifsdReleaseVcb(CanWait, pVCB);
		}
		
		XixFsdCompleteRequest( pIrpContext, RC, 0 );
		
		return RC;
	}

	DirContextClean = TRUE;


	


	try
	{


		RC = XixFsInitializeFileContext(&FileContext);
		if(!NT_SUCCESS(RC)){
			try_return(RC);
		}

		FileContextClean = TRUE;
		

		RC = XixFsLookupInitialDirEntry(pIrpContext, pFCB, &DirContext, 0);
		
		if(!NT_SUCCESS(RC)){
			try_return(RC);
		}
			
		UserBuffer = XixFsdGetCallersBuffer(pIrp);




		if(!XixFsdVerifyFcbOperation( pIrpContext, pFCB )){
			RC = STATUS_INVALID_PARAMETER;
			try_return(RC);
		}

		RC = XixFsdInitializeEnumeration (pIrpContext,
									pIrpSp,
									pFCB,
									pCCB,
									&DirContext,
									&ReturnNextEntry,
									&ReturnSingleEntry,
									&InitialQuery
									);
		if(!NT_SUCCESS(RC)){
			if (NextEntry == 0) {

				RC = STATUS_NO_MORE_FILES;

				if (InitialQuery) {

					RC = STATUS_NO_SUCH_FILE;
				}
			}
			try_return(RC);
		}

		while(1)
		{

			if ((NextEntry != 0) && ReturnSingleEntry) 
			{

				try_return( RC );
			}

			PreviousFileIndex = DirContext.SearchedVirtualDirIndex;

			try {
				
				Found = XixFsdEnumerateIndex( pIrpContext, pCCB, &DirContext, ReturnNextEntry );
			} except (((0 != NextEntry) && 
					 ((GetExceptionCode() == STATUS_FILE_CORRUPT_ERROR) || 
					  (GetExceptionCode() == STATUS_CRC_ERROR)))
					 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)  
			{

				ReturnNextEntry = TRUE;

				DirContext.VirtualDirIndex = PreviousFileIndex;

				try_return( RC = STATUS_SUCCESS);
			}



			ReturnNextEntry = TRUE;

			if (!Found) {

				if (NextEntry == 0) {

					RC = STATUS_NO_MORE_FILES;

					if (InitialQuery) {

						RC = STATUS_NO_SUCH_FILE;
					}
				}

				
				try_return( RC );
			}

			ThisFid = DirContext.LotNumber;
			FileNameBytes = DirContext.ChildName.Length;



			if (NextEntry > pIrpSp->Parameters.QueryDirectory.Length) {
    
				ReturnNextEntry = FALSE;
				try_return( RC = STATUS_SUCCESS );
			}


			BytesRemainingInBuffer = pIrpSp->Parameters.QueryDirectory.Length - NextEntry;
			XifsdClearFlag( BytesRemainingInBuffer, 1 );

			if ((BaseLength + FileNameBytes) > BytesRemainingInBuffer) 
			{

				//
				//  If we already found an entry then just exit.
				//

				if (NextEntry != 0) {

					ReturnNextEntry = FALSE;
					try_return( RC = STATUS_SUCCESS );
				}

				//
				//  Reduce the FileNameBytes to just fit in the buffer.
				//

				FileNameBytes = BytesRemainingInBuffer - BaseLength;

				//
				//  Use a status code of STATUS_BUFFER_OVERFLOW.  Also set
				//  ReturnSingleEntry so that we will exit the loop at the top.
				//

				RC = STATUS_BUFFER_OVERFLOW;
				ReturnSingleEntry = TRUE;
			}


			DebugTrace( DEBUG_LEVEL_INFO, DEBUG_TARGET_DIRINFO,
					 ("Final Searched LotNumber(%I64d) FileType(%ld)\n", 
					 DirContext.LotNumber,DirContext.FileType));
			
			RC = XixFsSetFileContext(pVCB, DirContext.LotNumber,DirContext.FileType, &FileContext);
			
			if(!NT_SUCCESS(RC)){
				try_return(RC);
			}

			RC = XixFsReadFileInfoFromContext(CanWait, &FileContext);

			if(!NT_SUCCESS(RC)){
				try_return(RC);
			}

			pFileHeader = (PXIDISK_FILE_HEADER_LOT)FileContext.Buffer;


			try
			{
				RtlZeroMemory( Add2Ptr( UserBuffer, NextEntry),
							   BaseLength );

				//
				//  Now we have an entry to return to our caller.
				//  We'll case on the type of information requested and fill up
				//  the user buffer if everything fits.
				//

				switch (pIrpSp->Parameters.QueryDirectory.FileInformationClass) {

				case FileBothDirectoryInformation:
				case FileFullDirectoryInformation:
				case FileIdBothDirectoryInformation:
				case FileIdFullDirectoryInformation:
				case FileDirectoryInformation:

					DirInfo = Add2Ptr( UserBuffer, NextEntry);
					if(FileContext.pSearchedFCB){
						PXIFS_FCB pCurrentFCB = FileContext.pSearchedFCB;

						DirInfo->LastWriteTime.QuadPart = 
						DirInfo->ChangeTime.QuadPart = pCurrentFCB->LastWriteTime;
						DirInfo->LastAccessTime.QuadPart = pCurrentFCB->LastAccessTime;
						DirInfo->CreationTime.QuadPart = pCurrentFCB->CreationTime;
						DirInfo->FileAttributes = pCurrentFCB->FileAttribute;
						if(DirContext.FileType == FCB_TYPE_DIR){
							DirInfo->EndOfFile.QuadPart = DirInfo->AllocationSize.QuadPart = 0;
							XifsdSetFlag(DirInfo->FileAttributes, FILE_ATTRIBUTE_DIRECTORY );
						}else{
							DirInfo->EndOfFile.QuadPart = pCurrentFCB->FileSize.QuadPart;
							DirInfo->AllocationSize.QuadPart = pCurrentFCB->AllocationSize.QuadPart;
						}
						
						DirInfo->FileIndex = (uint32)DirContext.SearchedVirtualDirIndex;
						DirInfo->FileNameLength = FileNameBytes;

					}else{
						

						DirInfo->LastWriteTime.QuadPart =
						DirInfo->ChangeTime.QuadPart = pFileHeader->FileInfo.Change_time;
						DirInfo->LastAccessTime.QuadPart = pFileHeader->FileInfo.Access_time;
						DirInfo->CreationTime.QuadPart = pFileHeader->FileInfo.Create_time;
						DirInfo->FileAttributes = pFileHeader->FileInfo.FileAttribute;
						if(DirContext.FileType == FCB_TYPE_DIR){
							DirInfo->EndOfFile.QuadPart = DirInfo->AllocationSize.QuadPart = 0;
							XifsdSetFlag(DirInfo->FileAttributes, FILE_ATTRIBUTE_DIRECTORY );
						}else{
							DirInfo->EndOfFile.QuadPart = pFileHeader->FileInfo.FileSize;
							DirInfo->AllocationSize.QuadPart = pFileHeader->FileInfo.AllocationSize;
						}
    
						DirInfo->FileIndex = (uint32)DirContext.SearchedVirtualDirIndex;
						DirInfo->FileNameLength = FileNameBytes;
					}




					DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB),
						("[DIR ENTRY INFO]FileIndex(%ld):EndofFile(%I64d):AllocationSize(%I64d):AtrriBute(0x%x):"
							"ChangeT(%I64d):LastAccessT(%I64d):CreationT(%I64d)\n",
							DirInfo->FileIndex,
							DirInfo->EndOfFile.QuadPart,
							DirInfo->AllocationSize.QuadPart,
							DirInfo->FileAttributes,
							DirInfo->ChangeTime.QuadPart,
							DirInfo->LastAccessTime.QuadPart,
							DirInfo->CreationTime.QuadPart));




					break;

				case FileNamesInformation:

					NamesInfo = Add2Ptr( UserBuffer, NextEntry);
					NamesInfo->FileIndex = (uint32)DirContext.SearchedVirtualDirIndex;
					NamesInfo->FileNameLength = FileNameBytes;

					break;
				}

				//
				//  Fill in the FileId
				//

				switch (pIrpSp->Parameters.QueryDirectory.FileInformationClass) {

				case FileIdBothDirectoryInformation:

					IdBothDirInfo = Add2Ptr( UserBuffer, NextEntry);
					if(DirContext.FileType == FCB_TYPE_DIR){
						IdBothDirInfo->FileId.QuadPart 
							= (FCB_TYPE_DIR_INDICATOR|(FCB_ADDRESS_MASK & DirContext.LotNumber));
					}else{
						IdBothDirInfo->FileId.QuadPart  
							= (FCB_TYPE_FILE_INDICATOR|(FCB_ADDRESS_MASK & DirContext.LotNumber));
					}

					DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB),
						("File LotNumber(%I64d)\nFileId(0x%08x)\n",
							DirContext.LotNumber,
							IdBothDirInfo->FileId.QuadPart));


					break;

				case FileIdFullDirectoryInformation:

					IdFullDirInfo = Add2Ptr( UserBuffer, NextEntry);
					if(DirContext.FileType == FCB_TYPE_DIR){
						IdFullDirInfo->FileId.QuadPart  
							= (FCB_TYPE_DIR_INDICATOR|(FCB_ADDRESS_MASK & DirContext.LotNumber));
					}else{
						IdFullDirInfo->FileId.QuadPart  
							= (FCB_TYPE_FILE_INDICATOR|(FCB_ADDRESS_MASK & DirContext.LotNumber));
					}

					DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB),
						("File LotNumber(%I64d)\nFileId(0x%08x)\n",
							DirContext.LotNumber,
							IdBothDirInfo->FileId.QuadPart));
					break;

				default:
					break;
				}

				//
				//  Now copy as much of the name as possible.
				//

				if (FileNameBytes != 0) {

					//
					//  This is a Unicode name, we can copy the bytes directly.
					//

					RtlCopyMemory( Add2Ptr( UserBuffer, NextEntry + BaseLength),
								   DirContext.ChildName.Buffer,
								   FileNameBytes );

					DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB),
						("Org FileName(%wZ) NameSize(%ld) FileNameBytes(%ld)\n", 
						&DirContext.ChildName, &DirContext.ChildName.Length, FileNameBytes));


						
						//DbgPrint("Org FileName(%wZ) NameSize(%ld) FileNameBytes(%ld)\n", 
						//&DirContext.ChildName, DirContext.ChildName.Length, FileNameBytes);

				}

				Information = NextEntry + BaseLength + FileNameBytes;

				*((uint32 *)(Add2Ptr( UserBuffer, LastEntry))) = NextEntry - LastEntry;

				//
				//  Set up our variables for the next dirent.
				//

				InitialQuery = FALSE;

				LastEntry = NextEntry;
				NextEntry = XifsdQuadAlign( Information );

			} 
			except (!FsRtlIsNtstatusExpected(GetExceptionCode()) ?
					  EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {

				  //
				  //  We must have had a problem filling in the user's buffer, so stop
				  //  and fail this request.
				  //
      
				  Information = 0;
				  try_return( RC = GetExceptionCode());
			}
		
		}

        XixFsCleanupDirContext( pIrpContext, &DirContext );
		DirContextClean = FALSE;
		XixFsClearFileContext(&FileContext);
		FileContextClean = FALSE;
	}finally{
		if (!AbnormalTermination() && !NT_ERROR( RC)) {

			//
			//  Update the Ccb to show the current state of the enumeration.
			//

			XifsdLockFcb( pIrpContext, pFCB );

			pCCB->currentFileIndex = DirContext.SearchedVirtualDirIndex;

			if ((DirContext.SearchedVirtualDirIndex > 1)
				&& (DirContext.SearchedVirtualDirIndex > pCCB->highestFileIndex)) {

					pCCB->highestFileIndex = DirContext.SearchedVirtualDirIndex;
			}

			//
			//  Mark in the CCB whether or not to skip the current entry on next call
			//  (if we  returned it in the current buffer).
			//
    
			XifsdClearFlag( pCCB->CCBFlags, XIFSD_CCB_FLAG_ENUM_RETURN_NEXT );

			if (ReturnNextEntry) {

				XifsdSetFlag( pCCB->CCBFlags, XIFSD_CCB_FLAG_ENUM_RETURN_NEXT );
			}
			
			XifsdUnlockFcb( pIrpContext, pFCB );
		}

		if(DirContextClean){
			XixFsCleanupDirContext( pIrpContext, &DirContext );
		}

		if(FileContextClean){
			XixFsClearFileContext(&FileContext);
		}

		if(AcquireFCB){
			XifsdReleaseFcb(CanWait, pFCB);
		}
		
		if(AcquireVCB){
			XifsdReleaseVcb(CanWait, pVCB);
		}
	}
	DebugTrace(DEBUG_LEVEL_ALL,DEBUG_TARGET_ALL , 
			("XixFsdQueryDirectory Name(%wZ) CCB(%p) Complete3 RC(%x).\n", &pFCB->FCBName, pCCB, RC));

	XixFsdCompleteRequest( pIrpContext, RC, Information );
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_DIRINFO, ("Exit XixFsdQueryDirectory .\n"));
	return RC;
}




BOOLEAN
XixFsdNotifyCheck (
    IN PXIFS_CCB pCCB,
    IN PXIFS_FCB pFCB,
    IN PSECURITY_SUBJECT_CONTEXT SubjectContext
    )
{

	//PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL, DEBUG_TARGET_DIRINFO|DEBUG_TARGET_ALL, ("Enter XixFsdNotifyCheck .\n"));

	if(pCCB != NULL){
		DebugTrace(DEBUG_LEVEL_INFO|DEBUG_LEVEL_ALL, DEBUG_TARGET_DIRINFO|DEBUG_TARGET_ALL,
				 ("XixFsdNotifyCheck Check pCCB (%p)\n", pCCB));	
	}

	if(pFCB != NULL){
		DebugTrace(DEBUG_LEVEL_INFO|DEBUG_LEVEL_ALL, DEBUG_TARGET_DIRINFO|DEBUG_TARGET_ALL,
				 ("XixFsdNotifyCheck Check pFCB (%p) LotNumber(%I64d)\n", pFCB, pFCB->LotNumber));
	}

	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL, DEBUG_TARGET_DIRINFO|DEBUG_TARGET_ALL, ("Exit XixFsdNotifyCheck .\n"));
	return TRUE;
}




NTSTATUS
XixFsdNotifyDirectory(
	IN PXIFS_IRPCONTEXT		pIrpContext, 
	IN PIRP					pIrp, 
	IN PIO_STACK_LOCATION 	pIrpSp, 
	IN PFILE_OBJECT			pFileObject, 
	IN PXIFS_FCB			pFCB, 
	IN PXIFS_CCB			pCCB
	)
{


	NTSTATUS				RC = STATUS_SUCCESS;
	BOOLEAN					CompleteRequest = FALSE;
	BOOLEAN					PostRequest = FALSE;
	BOOLEAN					CanWait = FALSE;
	BOOLEAN					WatchTree = FALSE;
	PXIFS_VCB				pVCB = NULL;
	BOOLEAN					AcquiredFCB = FALSE;
	PCHECK_FOR_TRAVERSE_ACCESS CallBack = NULL;

	//PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_ALL),
		("Enter XixFsdNotifyDirectory .\n"));

	ASSERT_FCB(pFCB);
	pVCB = pFCB->PtrVCB;
	ASSERT_VCB(pVCB);


	CallBack = XixFsdNotifyCheck;


	CanWait = XifsdCheckFlagBoolean(pIrpContext->IrpContextFlags , XIFSD_IRP_CONTEXT_WAIT);
	if(!CanWait){
		DebugTrace( DEBUG_LEVEL_INFO|DEBUG_LEVEL_ALL, ( DEBUG_TARGET_DIRINFO| DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_ALL),
				 ("post request pIrpContext(%p) pIrp(%p)\n", pIrpContext, pIrp));
		RC = XixFsdPostRequest(pIrpContext, pIrp);
		return RC; 
	}



	if(!XifsdAcquireVcbExclusive(TRUE, pVCB, FALSE)){
		DebugTrace( DEBUG_LEVEL_INFO|DEBUG_LEVEL_ALL, ( DEBUG_TARGET_DIRINFO| DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_ALL),
				 ("post request pIrpContext(%p) pIrp(%p)\n", pIrpContext, pIrp));
		RC = XixFsdPostRequest(pIrpContext, pIrp);
		return RC; 
	}
	
	

	WatchTree = XifsdCheckFlagBoolean( pIrpSp->Flags, SL_WATCH_TREE );

	//XifsdAcquireVcbShared(TRUE, pVCB, FALSE);
	




	try {

		/*	
		FsRtlNotifyFilterChangeDirectory (
			pVCB->NotifyIRPSync,
			&(pVCB->NextNotifyIRP),
			(void *)pCCB,
			(PSTRING) &pIrpSp->FileObject->FileName, 
			WatchTree, 
			FALSE, 
			pIrpSp->Parameters.NotifyDirectory.CompletionFilter, 
			pIrp,
			CallBack,
			NULL,
			NULL
			);
		*/


		DebugTrace( DEBUG_LEVEL_INFO|DEBUG_LEVEL_ALL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_ALL),
				 ("!!CompletionFilter Name(%wZ) Filter(%x) !!\n",&pFCB->FCBName, pIrpSp->Parameters.NotifyDirectory.CompletionFilter ));
		
		DebugTrace( DEBUG_LEVEL_INFO|DEBUG_LEVEL_ALL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_IRPCONTEXT|DEBUG_TARGET_ALL),
				("!! Set notifier pCCB %p pFCB %p LotNumber(%I64d)\n", pCCB, pFCB, pFCB->LotNumber ));
		
		if(!XifsdCheckFlagBoolean(pCCB->CCBFlags, XIFSD_CCB_FLAG_NOFITY_SET)){
			XifsdSetFlag(pCCB->CCBFlags, XIFSD_CCB_FLAG_NOFITY_SET);
		}

		FsRtlNotifyFullChangeDirectory(
						pVCB->NotifyIRPSync, 
						&(pVCB->NextNotifyIRP), 
						(void *)pCCB,
						(PSTRING) &pFCB->FCBFullPath,
						WatchTree, 
						FALSE, 
						pIrpSp->Parameters.NotifyDirectory.CompletionFilter, 
						pIrp,
						NULL,//CallBack,
						NULL);	


		/*
		DbgPrint("!!DirNotification Name(%wZ) pCCB(%p) Filter(%x) WatchTree(%s) !!\n",
				&pFCB->FCBName,
				pCCB,
				pIrpSp->Parameters.NotifyDirectory.CompletionFilter,
				(WatchTree?"TRUE":"FALSE"));
		*/



		RC = STATUS_PENDING;


	} finally {
		XifsdReleaseVcb(TRUE, pVCB);
	}

	XixFsdReleaseIrpContext(pIrpContext);

	DebugTrace(DEBUG_LEVEL_TRACE|DEBUG_LEVEL_ALL, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_ALL),
		("Exit XixFsdNotifyDirectory .\n"));
	
	return(RC);
}


NTSTATUS
XixFsdCommonDirectoryControl(
	IN PXIFS_IRPCONTEXT pIrpContext
	)
{
	NTSTATUS				RC = STATUS_SUCCESS;
	PIRP						pIrp = NULL;
	PIO_STACK_LOCATION		pIrpSp = NULL;
	PFILE_OBJECT				pFileObject = NULL;
	PXIFS_FCB				pFCB = NULL;
	PXIFS_CCB				pCCB = NULL;

	//PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_DIRINFO, 
		("Enter XixFsdCommonDirectoryControl .\n"));

	ASSERT_IRPCONTEXT(pIrpContext);
	ASSERT(pIrpContext->Irp);

	pIrp = pIrpContext->Irp;
	pIrpSp = IoGetCurrentIrpStackLocation(pIrp);
	ASSERT(pIrpSp);

	pFileObject = pIrpSp->FileObject;
	ASSERT(pFileObject);

    if (XixFsdDecodeFileObject( pFileObject,
                             &pFCB,
                             &pCCB ) != UserDirectoryOpen) {


		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				 ("Is Not Dir Object !!!!\n"));	

        XixFsdCompleteRequest( pIrpContext,STATUS_INVALID_PARAMETER, 0 );
        return STATUS_INVALID_PARAMETER;
    }

	//DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
	//				("!!!!DirectoryControl (%wZ) pCCB(%p)\n", &pFCB->FCBName, pCCB));


	try{
		switch(pIrpSp->MinorFunction){
		case IRP_MN_QUERY_DIRECTORY:
		{
			DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!DirectoryControl (%wZ) pCCB(%p) : XixFsdQueryDirectory\n", &pFCB->FCBName, pCCB));
			
			RC = XixFsdQueryDirectory(pIrpContext, pIrp, pIrpSp, pFileObject, pFCB, pCCB);
			
			//DbgPrint("Query Dir %wZ  pIrpCtonext %p  Status %x\n", &pFCB->FCBName, pIrpContext, RC);

			if(!NT_SUCCESS(RC)){
				try_return(RC);
			}
		}break;
		case IRP_MN_NOTIFY_CHANGE_DIRECTORY:			
		{
			DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!DirectoryControl (%wZ) pCCB(%p) : XixFsdNotifyDirectory\n", &pFCB->FCBName, pCCB));

			RC = XixFsdNotifyDirectory(pIrpContext, pIrp, pIrpSp, pFileObject, pFCB, pCCB);

			if(!NT_SUCCESS(RC)){
				try_return(RC);
			}			
		}break;
		default:
			DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!DirectoryControl (%wZ) pCCB(%p) : INVALID REQUEST\n", &pFCB->FCBName, pCCB));

			RC = STATUS_INVALID_DEVICE_REQUEST;
			XixFsdCompleteRequest(pIrpContext, RC, 0);
			break;
		}

	}finally{
		;
	}
	
	DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
					("!!!!DirectoryControl (%wZ) pCCB(%p) End !!!!\n", &pFCB->FCBName, pCCB));

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_DIRINFO, 
		("Exit XixFsdCommonDirectoryControl .\n"));
	return RC;
	
}