#include "LfsProc.h"


INLINE
PNON_PAGED_FCB
LfsAllocateNonPagedFcb (
    )
{
    return (PNON_PAGED_FCB) ExAllocateFromNPagedLookasideList( &GlobalLfs.NonPagedFcbLookasideList );
}

INLINE
VOID
LfsFreeNonPagedFcb (
    PNON_PAGED_FCB NonPagedFcb
    )
{
#if FAT_FILL_FREE
    RtlFillMemoryUlong(NonPagedFcb, sizeof(NON_PAGED_FCB), FAT_FILL_FREE);
#endif

    ExFreeToNPagedLookasideList( &GlobalLfs.NonPagedFcbLookasideList, (PVOID) NonPagedFcb );
}

INLINE
PERESOURCE
LfsAllocateResource (
    )
{
    PERESOURCE Resource;

    Resource = (PERESOURCE) ExAllocateFromNPagedLookasideList( &GlobalLfs.EResourceLookasideList );

    ExInitializeResourceLite( Resource );

    return Resource;
}

INLINE
VOID
LfsFreeResource (
    IN PERESOURCE Resource
    )
{
    ExDeleteResourceLite( Resource );

#if FAT_FILL_FREE
    RtlFillMemoryUlong(Resource, sizeof(ERESOURCE), FAT_FILL_FREE);
#endif

    ExFreeToNPagedLookasideList( &GlobalLfs.EResourceLookasideList, (PVOID) Resource );
}


PLFS_FCB
AllocateFcb (
	IN	PSECONDARY		Secondary,
	IN PUNICODE_STRING	FullFileName,
	IN BOOLEAN			IsPagingFile
    )
{
    PLFS_FCB fcb;


	UNREFERENCED_PARAMETER( Secondary );
	
    fcb = FsRtlAllocatePoolWithTag( NonPagedPool,
									sizeof(LFS_FCB),
									LFS_FCB_TAG );
	
	if (fcb == NULL) {
	
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR, ("AllocateFcb: failed to allocate fcb\n") );
		return NULL;
	}
	
	RtlZeroMemory( fcb, sizeof(LFS_FCB) - sizeof(CHAR) );

    fcb->NonPaged = LfsAllocateNonPagedFcb();
	
	if (fcb->NonPaged == NULL) {
	
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR, ("AllocateFcb: failed to allocate fcb->NonPaged\n") );
		
		ExFreePool( fcb );
		
		return NULL;
	}

    RtlZeroMemory( fcb->NonPaged, sizeof(NON_PAGED_FCB) );

#define FAT_NTC_FCB                      (0x0502)
	
	fcb->Header.NodeTypeCode = FAT_NTC_FCB;
	fcb->Header.IsFastIoPossible = FastIoIsPossible;
    fcb->Header.Resource = LfsAllocateResource();
	fcb->Header.PagingIoResource = NULL; //fcb->Header.Resource;
	
	if (fcb->Header.Resource == NULL) {
	
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR, ("AllocateFcb: failed to allocate fcb->Header.Resource\n") );
		ExFreePool( fcb->NonPaged );
		ExFreePool( fcb );
		return NULL;
	}

	if (LfsFsRtlTeardownPerStreamContexts) {

		ExInitializeFastMutex( &fcb->NonPaged->AdvancedFcbHeaderMutex );
		FsRtlSetupAdvancedHeader( &fcb->Header, &fcb->NonPaged->AdvancedFcbHeaderMutex );

		if (IsPagingFile) {

			ClearFlag( fcb->Header.Flags2, FSRTL_FLAG2_SUPPORTS_FILTER_CONTEXTS );
		}
	}

	FsRtlInitializeFileLock( &fcb->FileLock, NULL, NULL );

	fcb->ReferenceCount = 1;
	InitializeListHead( &fcb->ListEntry );

    RtlInitEmptyUnicodeString( &fcb->FullFileName,
							   fcb->FullFileNameBuffer,
							   sizeof(fcb->FullFileNameBuffer) );

	RtlCopyUnicodeString( &fcb->FullFileName, FullFileName );

    RtlInitEmptyUnicodeString( &fcb->CaseInSensitiveFullFileName,
							   fcb->CaseInSensitiveFullFileNameBuffer,
							   sizeof(fcb->CaseInSensitiveFullFileNameBuffer) );

	RtlDowncaseUnicodeString( &fcb->CaseInSensitiveFullFileName,
							  &fcb->FullFileName,
							  FALSE );

	if (FullFileName->Length) {

		if (FullFileName->Buffer[0] != L'\\') {

			ASSERT( LFS_BUG );
		}
	}

	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE, ("AllocateFcb: Fcb = %p\n", fcb) );

	return fcb;
}


VOID
Secondary_DereferenceFcb (
	IN	PLFS_FCB	Fcb
	)
{
	LONG		result;


	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
				   ("Secondary_DereferenceFcb: Fcb->OpenCount = %d, Fcb->UncleanCount = %d\n", Fcb->OpenCount, Fcb->UncleanCount) );

	ASSERT( Fcb->OpenCount >= Fcb->UncleanCount );
	result = InterlockedDecrement( &Fcb->ReferenceCount );

	ASSERT( result >= 0 );

	if (0 == result) {

		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
					   ("Secondary_DereferenceFcb: Fcb = %p\n", Fcb) );
		
		ASSERT( Fcb->ListEntry.Flink == Fcb->ListEntry.Blink );
		ASSERT( Fcb->OpenCount == 0 );

		if (LfsFsRtlTeardownPerStreamContexts) {

			if (FlagOn( Fcb->Header.Flags2, FSRTL_FLAG2_SUPPORTS_FILTER_CONTEXTS )) {

				(*LfsFsRtlTeardownPerStreamContexts)( &Fcb->Header );
			}
		}

		LfsFreeResource( Fcb->Header.Resource );
		LfsFreeNonPagedFcb( Fcb->NonPaged );
	    ExFreePool( Fcb );	
	}
}


PLFS_FCB
Secondary_LookUpFcb (
	IN PSECONDARY		Secondary,
	IN PUNICODE_STRING	FullFileName,
    IN BOOLEAN			CaseInSensitive
	)
{
	PLFS_FCB		fcb = NULL;
    PLIST_ENTRY		listEntry;
	KIRQL			oldIrql;
	UNICODE_STRING	caseInSensitiveFullFileName;
	PWCHAR			caseInSensitiveFullFileNameBuffer;
	NTSTATUS		downcaseStatus;

	//
	//	Allocate a name buffer
	//

	caseInSensitiveFullFileNameBuffer = ExAllocatePoolWithTag(NonPagedPool, NDFS_MAX_PATH*sizeof(WCHAR), LFS_ALLOC_TAG);
	
	if (caseInSensitiveFullFileNameBuffer == NULL) {
	
		ASSERT( LFS_REQUIRED );
		return NULL;
	}

	ASSERT( FullFileName->Length <= NDFS_MAX_PATH*sizeof(WCHAR) );

	if (CaseInSensitive == TRUE) {

		RtlInitEmptyUnicodeString( &caseInSensitiveFullFileName,
								   caseInSensitiveFullFileNameBuffer,
								   NDFS_MAX_PATH*sizeof(WCHAR) );

		downcaseStatus = RtlDowncaseUnicodeString( &caseInSensitiveFullFileName,
												   FullFileName,
												   FALSE );
	
		if (downcaseStatus != STATUS_SUCCESS) {

			ExFreePool( caseInSensitiveFullFileNameBuffer );
			ASSERT( LFS_UNEXPECTED );
			return NULL;
		}
	}

	KeAcquireSpinLock( &Secondary->FcbQSpinLock, &oldIrql );

    for (listEntry = Secondary->FcbQueue.Flink;
         listEntry != &Secondary->FcbQueue;
         listEntry = listEntry->Flink) {

		fcb = CONTAINING_RECORD( listEntry, LFS_FCB, ListEntry );
		
		if (fcb->FullFileName.Length != FullFileName->Length) {

			fcb = NULL;
			continue;
		}

		if (CaseInSensitive == TRUE) {

			if (RtlEqualMemory(fcb->CaseInSensitiveFullFileName.Buffer, 
							   caseInSensitiveFullFileName.Buffer, 
							   fcb->CaseInSensitiveFullFileName.Length)) {

				InterlockedIncrement( &fcb->ReferenceCount );
				break;
			}
		
		} else {

			if (RtlEqualMemory(fcb->FullFileName.Buffer,
							   FullFileName->Buffer,
							   fcb->FullFileName.Length)) {

				InterlockedIncrement( &fcb->ReferenceCount );
				break;
			}
		}

		fcb = NULL;
	}

	KeReleaseSpinLock( &Secondary->FcbQSpinLock, oldIrql );

	ExFreePool( caseInSensitiveFullFileNameBuffer );

	return fcb;
}


PLFS_CCB
AllocateCcb (
	IN	PSECONDARY		Secondary,
	IN	PFILE_OBJECT	FileObject,
	IN  ULONG			BufferLength
	) 
{
	PLFS_CCB	ccb;


	ccb = ExAllocatePoolWithTag( NonPagedPool, 
								 sizeof(LFS_CCB) - sizeof(UINT8) + BufferLength,
								 LFS_CCB_TAG );
	
	if (ccb == NULL) {

		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
		return NULL;
	}

	RtlZeroMemory( ccb, sizeof(LFS_CCB) - sizeof(UINT8) + BufferLength );
	

	ccb->LfsMark    = LFS_MARK;
	ccb->Secondary	= Secondary;
	ccb->FileObject	= FileObject;

	ccb->LastQueryFileIndex = (ULONG)-1;

	InitializeListHead( &ccb->ListEntry );

	ccb->BufferLength = BufferLength;

	ExAcquireFastMutex( &Secondary->FastMutex );
	ccb->SessionId = Secondary->SessionId;
	ccb->LastDirectoryQuerySessionId = Secondary->SessionId;
	ExReleaseFastMutex( &Secondary->FastMutex );

	InterlockedIncrement( &Secondary->CcbCount );

	return ccb;
}


VOID
FreeCcb (
	IN  PSECONDARY		Secondary,
	IN  PLFS_CCB	Ccb
	)
{
	PLIST_ENTRY		listEntry;


	ASSERT( Ccb->ListEntry.Flink == Ccb->ListEntry.Blink );

	ExAcquireFastMutex( &Secondary->CcbQMutex );

    for (listEntry = Secondary->CcbQueue.Flink;
         listEntry != &Secondary->CcbQueue;
         listEntry = listEntry->Flink) {

		PLFS_CCB	childCcb;
		
		childCcb = CONTAINING_RECORD( listEntry, LFS_CCB, ListEntry );
        
		if (childCcb->CreateContext.RelatedFileHandle == Ccb->PrimaryFileHandle)
			childCcb->RelatedFileObjectClosed = TRUE;
	}

    ExReleaseFastMutex( &Secondary->CcbQMutex );

	InterlockedDecrement( &Secondary->CcbCount );

	ExFreePoolWithTag( Ccb, LFS_CCB_TAG );
}


PLFS_CCB
Secondary_LookUpCcbByHandle (
	IN PSECONDARY	Secondary,
	IN HANDLE		FileHandle
	)
{
	NTSTATUS		referenceStatus;
	PFILE_OBJECT	fileObject = NULL;


	referenceStatus = ObReferenceObjectByHandle( FileHandle,
												 FILE_READ_DATA,
												 NULL,
												 KernelMode,
												 &fileObject,
												 NULL );

    if (referenceStatus != STATUS_SUCCESS) {

		return NULL;
	}
	
	ObDereferenceObject( fileObject );

	return Secondary_LookUpCcb( Secondary, fileObject );
}
	
PLFS_CCB
Secondary_LookUpCcb (
	IN PSECONDARY	Secondary,
	IN PFILE_OBJECT	FileObject
	)
{
	PLFS_CCB	ccb = NULL;
    PLIST_ENTRY		listEntry;

	
    ExAcquireFastMutex( &Secondary->CcbQMutex );

    for (listEntry = Secondary->CcbQueue.Flink;
         listEntry != &Secondary->CcbQueue;
         listEntry = listEntry->Flink) {

		 ccb = CONTAINING_RECORD( listEntry, LFS_CCB, ListEntry );
         
		 if (ccb->FileObject == FileObject)
			break;

		ccb = NULL;
	}

    ExReleaseFastMutex( &Secondary->CcbQMutex );

	return ccb;
}
