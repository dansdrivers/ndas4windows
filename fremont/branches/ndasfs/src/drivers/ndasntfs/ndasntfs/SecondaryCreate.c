#include "NtfsProc.h"


#if __NDAS_NTFS_SECONDARY__

#define BugCheckFileId                   (NTFS_BUG_CHECK_CREATE)

#define Dbg                              (DEBUG_TRACE_CREATE)
#define Dbg2                             (DEBUG_INFO_CREATE)

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG ('cfdN')



#define MOUNT_MGR_REMOTE_DATABASE	(L"\\:$MountMgrRemoteDatabase")
#define EXTEND_REPARSE				(L"\\$Extend\\$Reparse:$R:$INDEX_ALLOCATION")



#define PRECREATE_USER_FILE_ONLY	0

NTSTATUS
NdasNtfsSecondaryCommonCreate (     
    IN  PIRP_CONTEXT					IrpContext,
    IN  PIRP							Irp,
	IN  POPLOCK_CLEANUP					OplockCleanup,
	OUT PFILE_NETWORK_OPEN_INFORMATION	NetworkInfo OPTIONAL,
	OUT PBOOLEAN						DoMore
	)
{
	NTSTATUS					status;

	PVOLUME_DEVICE_OBJECT		volDo = CONTAINING_RECORD( IrpContext->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	BOOLEAN						pendingReturned = Irp->PendingReturned;

	BOOLEAN						secondarySessionResourceAcquired = FALSE;
	BOOLEAN						secondaryCreateResourceAcquired = FALSE;

	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT				fileObject = irpSp->FileObject;
	ULONG						createDisposition = (irpSp->Parameters.Create.Options >> 24) & 0x000000ff;
	
	UNICODE_STRING				fullPathName;
	PWCHAR						fullPathNameBuffer = NULL; 

	UNICODE_STRING				attrName;
	UNICODE_STRING				attrCodeName;
	ATTRIBUTE_TYPE_CODE			attrTypeCode;
	PUNICODE_STRING				originalFileName = &OplockCleanup->OriginalFileName;
	PUNICODE_STRING				fullFileName = &OplockCleanup->FullFileName;
	PUNICODE_STRING				fileObjectName;

	BOOLEAN						nameParseResult;

	WINXP_REQUEST_CREATE		createContext;
	UINT64						openFileHandle = 0;

	PSECONDARY_REQUEST			secondaryRequest = NULL;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
	UINT8							*ndfsWinxpRequestData;

	LARGE_INTEGER				timeOut;

	PFCB						fcb = NULL;
	BOOLEAN						returnedExistingFcb;
	PSCB						scb = NULL;
	BOOLEAN						returnedExistingScb;
	PLCB						lcb = NULL;
	BOOLEAN						returnedExistingLcb;
	PCCB						ccb = NULL;

	BOOLEAN						vcbAcquired	= FALSE;
	BOOLEAN						acquiredFcbTable = FALSE;
	BOOLEAN						acquiredFcbWithPaging = FALSE;
	BOOLEAN						acquiredMcb = FALSE;
	
	FILE_REFERENCE				fileReference;

	ULONG						ccbFlags;
	ULONG						ccbBufferLength;



	ASSERT( DoMore != NULL );
	ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	if (FlagOn(volDo->NetdiskPartitionInformation.Flags, NETDISK_PARTITION_INFORMATION_FLAG_INDIRECT)) {

		DebugTrace( 0, Dbg, ("NdasNtfsSecondaryCommonCreate: NETDISK_PARTITION_INFORMATION_FLAG_INDIRECT FileName = %wZ\n", 
							  &fileObject->FileName) );

		*DoMore = TRUE;
		return STATUS_SUCCESS;
	}

	DebugTrace( 0, Dbg, ("NdasNtfsSecondaryCommonCreate: FileName = %wZ\n", &fileObject->FileName) );

	if (volDo->NetdiskEnableMode == NETDISK_SECONDARY2PRIMARY) {

		if (FlagOn(IrpContext->NdasNtfsFlags, NDAS_NTFS_IRP_CONTEXT_FLAG_OPEN_TRY_ON_SECONDARY)) {
			
			DebugTrace( 0, Dbg, ("NDAS_NTFS_IRP_CONTEXT_FLAG_OPEN_TRY_ON_SECONDARY\n") );
			ASSERT( !FlagOn(volDo->Secondary->Flags, SECONDARY_FLAG_RECONNECTING) );
		
		} else if (!(irpSp->FileObject->RelatedFileObject && IS_SECONDARY_FILEOBJECT(irpSp->FileObject->RelatedFileObject))) {

			*DoMore = TRUE;
			return STATUS_SUCCESS;
		}
	
	} else {
	
		ASSERT( fileObject->FsContext == NULL );	
	}

	if (FlagOn(volDo->Secondary->Flags, SECONDARY_FLAG_DISMOUNTING)) {

		DebugTrace( 0, Dbg2, ("NdasNtfsSecondaryCommonCreate: SECONDARY_FLAG_DISMOUNTING FileName = %wZ\n", &fileObject->FileName) );

		if (!FlagOn(volDo->NetdiskPartitionInformation.Flags, NETDISK_PARTITION_INFORMATION_FLAG_INDIRECT) &&
			volDo->NetdiskEnableMode == NETDISK_SECONDARY) {

			SetFlag( IrpContext->NdasNtfsFlags, NDAS_NTFS_IRP_CONTEXT_FLAG_SECONDARY_CONTEXT );
		}

		//NDAS_ASSERT( fileObject->FileName.Length == 2 );

		*DoMore = TRUE;
		return STATUS_SUCCESS;
	}

	if (FlagOn(IrpContext->Vcb->VcbState, VCB_STATE_TARGET_DEVICE_STOPPED)) {

		DebugTrace( 0, Dbg2, ("NdasNtfsSecondaryCommonCreate: VCB_STATE_TARGET_DEVICE_STOPPED FileName = %wZ\n", &fileObject->FileName) );

		if (!FlagOn(volDo->NetdiskPartitionInformation.Flags, NETDISK_PARTITION_INFORMATION_FLAG_INDIRECT) &&
			volDo->NetdiskEnableMode == NETDISK_SECONDARY) {

			SetFlag( IrpContext->NdasNtfsFlags, NDAS_NTFS_IRP_CONTEXT_FLAG_SECONDARY_CONTEXT );
		}

		*DoMore = TRUE;
		return STATUS_SUCCESS;
	}

	if (!FlagOn(((PVOLUME_DEVICE_OBJECT)irpSp->DeviceObject)->NdasNtfsFlags, NDAS_NTFS_DEVICE_FLAG_MOUNTED)) {

		NDAS_ASSERT( FALSE );

		*DoMore = TRUE;
		return STATUS_SUCCESS;
	}

	*DoMore = FALSE;

	IrpContext->Union.OplockCleanup = OplockCleanup;

	try {

		ULONG			ntfsSystemFilesSize = 12; //sizeof(NtfsSystemFiles)/sizeof(UNICODE_STRING);
		ULONG			index;


		SetFlag( IrpContext->NdasNtfsFlags, NDAS_NTFS_IRP_CONTEXT_FLAG_SECONDARY_CONTEXT );
		SetFlag( IrpContext->NdasNtfsFlags, NDAS_NTFS_IRP_CONTEXT_FLAG_SECONDARY_FILE );

#if __NDAS_NTFS_DBG__
		IrpContext->RequestIrp = Irp;
		IrpContext->RequestIrpSp = IoGetCurrentIrpStackLocation( Irp );
		IrpContext->CurrentIrql = KeGetCurrentIrql();
		IrpContext->VcbSharedCount = ExIsResourceAcquiredSharedLite(&IrpContext->Vcb->Resource);
#endif

		if ((NtfsData.AsyncCloseCount + NtfsData.DelayedCloseCount) > NtfsThrottleCreates) {

			NtfsFspClose( (PVCB) 1 );
		}

		NtfsAcquireSharedVcb( IrpContext, IrpContext->Vcb, TRUE );
		vcbAcquired = TRUE;

		if ((FILE_OPEN_BY_FILE_ID & irpSp->Parameters.Create.Options) || BooleanFlagOn(irpSp->Flags, SL_OPEN_PAGING_FILE)) {

			PrintIrp( Dbg2, "NdasNtfsSecondaryCommonCreate", NULL, Irp );
			ASSERT( NDNTFS_REQUIRED );
			try_return( status = STATUS_NOT_SUPPORTED );
		}

#if 0
		if (irpSp->FileObject->RelatedFileObject) {

			if (!IS_SECONDARY_FILEOBJECT(irpSp->FileObject->RelatedFileObject)) {

				ASSERT( FALSE );
				try_return( status = STATUS_OBJECT_PATH_NOT_FOUND );
			}
		}
#endif

		for (index=0; index<ntfsSystemFilesSize; index++) {

			if (NtfsSystemFiles[index].Length != 2 && RtlEqualUnicodeString(&fileObject->FileName, &NtfsSystemFiles[index], TRUE)) {

				DebugTrace(0, Dbg2, ("NdasNtfsSecondaryCommonCreate: FileName = %Z\n", &fileObject->FileName) );
				//ASSERT( FALSE );
				try_return( status = STATUS_ACCESS_DENIED );
			}
		}

		if (RtlEqualUnicodeString(&NtfsData.MountMgrRemoteDatabase, &fileObject->FileName, TRUE)	|| 
			RtlEqualUnicodeString(&NtfsData.ExtendReparse, &fileObject->FileName, TRUE)				||
			RtlEqualUnicodeString(&NtfsData.MountPointManagerRemoteDatabase, &fileObject->FileName, TRUE)) {

			DebugTrace( 0, Dbg2, ("%s: %wZ returned with STATUS_OBJECT_NAME_NOT_FOUND\n", __FUNCTION__, &fileObject->FileName) );

			try_return( status = STATUS_OBJECT_NAME_NOT_FOUND );
		}

		*fullFileName = *originalFileName = OplockCleanup->FileObject->FileName;
		fileObjectName = &OplockCleanup->FileObject->FileName;

		attrName.Length = 0;
		attrCodeName.Length = 0;

		if (fileObjectName->Length == 0) {

			if (fileObject->RelatedFileObject == 0) {

				attrName.Length = 0;
				attrTypeCode	= $DATA;
			
			} else {
						
				attrName		= ((PSCB)(fileObject->RelatedFileObject->FsContext))->AttributeName;
				attrTypeCode	= ((PSCB)(fileObject->RelatedFileObject->FsContext))->AttributeTypeCode;
			}
		
		} else { 

			nameParseResult = NtfsParseNameForCreate( IrpContext,
													  OplockCleanup->FileObject->FileName,
					                                  fileObjectName,
						                              originalFileName,
							                          fullFileName,
								                      &attrName,
									                  &attrCodeName );

			if(nameParseResult == FALSE) {

				try_return( status = STATUS_INVALID_PARAMETER );
			}

			if(attrCodeName.Length) {

				attrTypeCode = NtfsGetAttributeTypeCode( IrpContext->Vcb, &attrCodeName );

				if (attrTypeCode == $UNUSED) {

					ASSERT( FALSE );
					DebugTrace( -1, Dbg, ("NtfsCheckValidAttributeAccess:  Bad attribute name for index\n") );
					try_return( status = STATUS_INVALID_PARAMETER );
				}
			
			} else
				attrTypeCode = $END;

			
			if(attrName.Length != 0 || attrCodeName.Length != 0) {
				
				DebugTrace( 0, Dbg, ("nameParseResult = %d\n", nameParseResult) );
				DebugTrace( 0, Dbg, ("FileObjectName = %Z\n", fileObjectName) );
				DebugTrace( 0, Dbg, ("OriginalFileName = %Z\n", originalFileName) );
				DebugTrace( 0, Dbg, ("AttrName = %Z\n", &attrName) );
				DebugTrace( 0, Dbg, ("AttrCodeName = %Z\n", &attrCodeName) );
			}
		}

		fullPathNameBuffer = NtfsAllocatePool( NonPagedPoolCacheAligned, NDFS_MAX_PATH );
		RtlInitEmptyUnicodeString( &fullPathName, fullPathNameBuffer, NDFS_MAX_PATH );

		if ((status = Secondary_MakeFullPathName(fileObject, fileObjectName, &fullPathName)) != STATUS_SUCCESS) {
			
			ASSERT( FALSE );
			try_return( status );
		}

		if(attrName.Length != 0 || attrCodeName.Length != 0) {

			DebugTrace( 0, Dbg, ("FileObjectName = %Z\n", fileObjectName) );
			DebugTrace( 0, Dbg, ("OriginalFileName = %Z\n", originalFileName) );
			DebugTrace( 0, Dbg, ("AttrName = %Z\n", &attrName) );
			DebugTrace( 0, Dbg, ("AttrCodeName = %Z\n", &attrCodeName) );
			DebugTrace( 0, Dbg, ("fullPathName = %Z\n", &fullPathName) );
		}

		secondaryCreateResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->CreateResource, 
													 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

		if (FlagOn(volDo->Vcb.VcbState, VCB_STATE_TARGET_DEVICE_STOPPED) ) {

			PrintIrp( Dbg2, "VCB_STATE_TARGET_DEVICE_STOPPED", NULL, IrpContext->OriginatingIrp );
			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );	
		}

#if PRECREATE_USER_FILE_ONLY

		if (!(fullPathName.Length == 0															||
			  originalFileName->Buffer[fileObject->FileName.Length/sizeof(WCHAR)-1] == L'\\'	||
			  BooleanFlagOn(irpSp->Parameters.Create.Options, FILE_DIRECTORY_FILE)				|| 		
			  BooleanFlagOn(irpSp->Flags, SL_OPEN_TARGET_DIRECTORY))) {

#else
		if (fullPathName.Length != 0 && (irpSp->Parameters.Create.Options & 0xFF000000) != (FILE_CREATE << 24)) {
#endif

			secondarySessionResourceAcquired 
				= SecondaryAcquireResourceExclusiveLite( IrpContext, 
														 &volDo->SessionResource, 
														 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

			if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

				PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
				NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );	
			}

			secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
															  IRP_MJ_CREATE, 
															  fullPathName.Length + (originalFileName->Length - fileObjectName->Length) );

			if(secondaryRequest == NULL) {

				NtfsRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES, NULL, NULL );
			}

			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;

			INITIALIZE_NDFS_REQUEST_HEADER( ndfsRequestHeader,
											NDFS_COMMAND_EXECUTE,
											volDo->Secondary,
											IRP_MJ_CREATE,
											fullPathName.Length + (originalFileName->Length - fileObjectName->Length) );

			ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

			//ndfsWinxpRequestHeader->IrpTag4   = (UINT32)secondaryRequest;
			ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_CREATE;
			ndfsWinxpRequestHeader->IrpMinorFunction = 0;

			ndfsWinxpRequestHeader->FileHandle8 = 0;

			ndfsWinxpRequestHeader->IrpFlags4   = 0;
			ndfsWinxpRequestHeader->IrpSpFlags = irpSp->Flags;

			ndfsWinxpRequestHeader->Create.AllocationSize = 0;
			ndfsWinxpRequestHeader->Create.EaLength = 0;
			ndfsWinxpRequestHeader->Create.FileAttributes = 0;

			ndfsWinxpRequestHeader->Create.Options = irpSp->Parameters.Create.Options & ~FILE_DELETE_ON_CLOSE;
#if PRECREATE_USER_FILE_ONLY
			ndfsWinxpRequestHeader->Create.Options |= FILE_NON_DIRECTORY_FILE;
#endif
			ndfsWinxpRequestHeader->Create.Options &= 0x00FFFFFF;
			ndfsWinxpRequestHeader->Create.Options |= (FILE_OPEN << 24);
			
			
			if (!FlagOn(irpSp->Flags, SL_CASE_SENSITIVE)) {
			
				ndfsWinxpRequestHeader->Create.FileNameLength = fullPathName.Length;
				ndfsWinxpRequestHeader->Create.FileNameLength += (originalFileName->Length - fileObjectName->Length);
				ndfsWinxpRequestHeader->Create.RelatedFileHandle = 0; 
			
			} else {

				ASSERT( FALSE );

				if (fileObject->RelatedFileObject) {
				
					PCCB	relatedCcb = fileObject->RelatedFileObject->FsContext2;


					if (FlagOn(relatedCcb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED)) {

						try_return( status = STATUS_OBJECT_PATH_NOT_FOUND );
					}
					ndfsWinxpRequestHeader->Create.RelatedFileHandle = relatedCcb->PrimaryFileHandle; 
				}

				ndfsWinxpRequestHeader->Create.FileNameLength = originalFileName->Length;
			}

			ndfsWinxpRequestHeader->Create.SecurityContext.DesiredAccess = 0; 
			ndfsWinxpRequestHeader->Create.SecurityContext.FullCreateOptions = 0;
			ndfsWinxpRequestHeader->Create.ShareAccess = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
			
			ndfsWinxpRequestData = (UINT8 *)(ndfsWinxpRequestHeader+1);

			if (!FlagOn(irpSp->Flags, SL_CASE_SENSITIVE)) {
	
				RtlCopyMemory( ndfsWinxpRequestData,
							   fullPathName.Buffer,
							   fullPathName.Length );

				RtlCopyMemory( ndfsWinxpRequestData + fullPathName.Length, 
							   ((PCHAR)(originalFileName->Buffer)) + fileObjectName->Length,
							   (originalFileName->Length - fileObjectName->Length) );
			
			} else {
			
				RtlCopyMemory( ndfsWinxpRequestData,
							   originalFileName->Buffer,
							   originalFileName->Length );
			}

			secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
			QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

			timeOut.QuadPart = -NDASNTFS_TIME_OUT;
			status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
			KeClearEvent( &secondaryRequest->CompleteEvent );

			if(status != STATUS_SUCCESS) {

				ASSERT( NDASNTFS_BUG );
				try_return( status );
			}

			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
			secondarySessionResourceAcquired = FALSE;

			if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

				if (IrpContext->OriginatingIrp)
					PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
				DebugTrace( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

				NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
			}
			
			ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;
			status = NTOHL(ndfsWinxpReplytHeader->Status4);

			if (NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_SUCCESS && 
			    NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_OBJECT_NAME_NOT_FOUND &&
				NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_OBJECT_PATH_NOT_FOUND && 
				NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_NOT_A_DIRECTORY		  && 
				NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_FILE_IS_A_DIRECTORY   &&
				NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_ACCESS_DENIED		  &&
				NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_OBJECT_NAME_INVALID ) {

				DebugTrace( 0, Dbg2, ("NdasNtfsSecondaryCommonCreate: NTOHL(ndfsWinxpReplytHeader->Status4) = %x\n", 
									   NTOHL(ndfsWinxpReplytHeader->Status4)) );
			}

			if (NTOHL(ndfsWinxpReplytHeader->Status4) == STATUS_OBJECT_NAME_INVALID) {

				DebugTrace( 0, Dbg, ("orginalFileName = %Z\n", originalFileName) );
				if(fileObject->RelatedFileObject)
					DebugTrace( 0, Dbg, ("RelatedFileObjectName = %Z\n", &fileObject->RelatedFileObject->FileName) );
			}

#if PRECREATE_USER_FILE_ONLY
			if (NTOHL(ndfsWinxpReplytHeader->Status4) == STATUS_SUCCESS)
				ASSERT( ndfsWinxpReplytHeader->Open.TypeOfOpen == UserFileOpen );

			if (NTOHL(ndfsWinxpReplytHeader->Status4) == STATUS_SUCCESS && ndfsWinxpReplytHeader->Open.TypeOfOpen == UserFileOpen) {
				ASSERT( ndfsWinxpReplytHeader->Open.FileReference.SegmentNumberLowPart >= FIRST_USER_FILE_NUMBER );
#else
			if (NTOHL(ndfsWinxpReplytHeader->Status4) == STATUS_SUCCESS) {
#endif							
				openFileHandle = ndfsWinxpReplytHeader->Open.FileHandle;

				fileReference.SegmentNumberHighPart = ndfsWinxpReplytHeader->Open.FileReference.SegmentNumberHighPart;
				fileReference.SegmentNumberLowPart	= ndfsWinxpReplytHeader->Open.FileReference.SegmentNumberLowPart;
				fileReference.SequenceNumber		= ndfsWinxpReplytHeader->Open.FileReference.SequenceNumber;  

				NtfsAcquireFcbTable( IrpContext, &volDo->Vcb );
				acquiredFcbTable = TRUE;

				returnedExistingFcb = FALSE;

				SetFlag( IrpContext->NdasNtfsFlags, NDAS_NTFS_IRP_CONTEXT_FLAG_SECONDARY_CREATE_FILE );
				fcb = NtfsCreateFcb ( IrpContext, &volDo->Vcb, fileReference, BooleanFlagOn(irpSp->Flags, SL_OPEN_PAGING_FILE), FALSE, &returnedExistingFcb );
				ClearFlag( IrpContext->NdasNtfsFlags, NDAS_NTFS_IRP_CONTEXT_FLAG_SECONDARY_CREATE_FILE );
					
				if (fcb == NULL) {
					
					DebugTrace( 0, Dbg, ("fcb == NULL\n") );
					NtfsReleaseFcbTable( IrpContext, &volDo->Vcb );						
					acquiredFcbTable = FALSE;
					scb = NULL;
				
				} else {
				
					ASSERT( returnedExistingFcb == TRUE );
						
					fcb->ReferenceCount += 1;
					NtfsReleaseFcbTable( IrpContext, &volDo->Vcb );
					acquiredFcbTable = FALSE;

					SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_PAGING );
					NtfsAcquireFcbWithPaging( IrpContext, fcb, 0 );
					acquiredFcbWithPaging = TRUE;

					NtfsAcquireFcbTable( IrpContext, &volDo->Vcb );
					fcb->ReferenceCount -= 1;
					NtfsReleaseFcbTable( IrpContext, &volDo->Vcb );

					SetFlag( IrpContext->NdasNtfsFlags, NDAS_NTFS_IRP_CONTEXT_FLAG_SECONDARY_CREATE_FILE );
					scb = NtfsCreateScb ( IrpContext, fcb, ndfsWinxpReplytHeader->Open.AttributeTypeCode, &attrName, TRUE, NULL );
					ClearFlag( IrpContext->NdasNtfsFlags, NDAS_NTFS_IRP_CONTEXT_FLAG_SECONDARY_CREATE_FILE );
				} 

#if PRECREATE_USER_FILE_ONLY
				if (scb) {
#else
				if (scb && ndfsWinxpReplytHeader->Open.TypeOfOpen == UserFileOpen) {
#endif
					ASSERT( scb->Handle == ndfsWinxpReplytHeader->Open.ScbHandle || scb->CleanupCount == 0 );

					if (FlagOn(irpSp->Parameters.Create.SecurityContext->DesiredAccess, FILE_WRITE_DATA) || 
						BooleanFlagOn(irpSp->Parameters.Create.Options, FILE_DELETE_ON_CLOSE)) {

						InterlockedIncrement( &scb->CloseCount );

						try {

							if (scb->NonpagedScb->SegmentObject.ImageSectionObject != NULL) {
								
								if (!MmFlushImageSection( &scb->NonpagedScb->SegmentObject, MmFlushForWrite )) {

									DebugTrace( 0, Dbg, ("Couldn't flush image section\n") );

									status = BooleanFlagOn(irpSp->Parameters.Create.Options, FILE_DELETE_ON_CLOSE) ? 
														   STATUS_CANNOT_DELETE : STATUS_SHARING_VIOLATION;
								
								} else
									status = STATUS_SUCCESS;
							}

						} finally {

							InterlockedDecrement( &scb->CloseCount );
						}

						if (status != STATUS_SUCCESS)
							try_return( status );
					}
		
					if (createDisposition == FILE_SUPERSEDE ||
					    createDisposition == FILE_OVERWRITE ||
					    createDisposition == FILE_OVERWRITE_IF) {

						InterlockedIncrement( &scb->CloseCount );

						if (!MmCanFileBeTruncated(&scb->NonpagedScb->SegmentObject, &Li0)) {

							InterlockedDecrement( &scb->CloseCount );
							try_return( status = STATUS_USER_MAPPED_FILE );
						} 
		
						InterlockedDecrement( &scb->CloseCount );
					}		
				}
	
				if (acquiredFcbWithPaging == TRUE) {

					if (scb == NULL) {
						
						NtfsReleaseFcbWithPaging( IrpContext, fcb );
						acquiredFcbWithPaging = FALSE;
						fcb = NULL;
					}
				}


				secondarySessionResourceAcquired 
					= SecondaryAcquireResourceExclusiveLite( IrpContext, 
															 &volDo->SessionResource, 
															 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

				if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

					PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
					NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );	
				}

				ClosePrimaryFile( volDo->Secondary, openFileHandle );
				openFileHandle = 0;

				SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
				secondarySessionResourceAcquired = FALSE;
			}

			DereferenceSecondaryRequest( secondaryRequest );
			secondaryRequest = NULL;
		}

		createContext.SecurityContext.DesiredAccess		= irpSp->Parameters.Create.SecurityContext->DesiredAccess;
		createContext.SecurityContext.FullCreateOptions	= irpSp->Parameters.Create.SecurityContext->FullCreateOptions;
		createContext.Options							= irpSp->Parameters.Create.Options;
		createContext.FileAttributes					= irpSp->Parameters.Create.FileAttributes;
		createContext.ShareAccess						= irpSp->Parameters.Create.ShareAccess;

		if(irpSp->Parameters.Create.EaLength == 0) {

			createContext.EaLength = 0;

		} else {

			PFILE_FULL_EA_INFORMATION	fileFullEa = (PFILE_FULL_EA_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

			ASSERT( Irp->AssociatedIrp.SystemBuffer != NULL );

			createContext.EaLength = 0;
			
			while (fileFullEa->NextEntryOffset) {

				DebugTrace( 0, Dbg, ("create ccb->Lcb->ExactCaseLink.LinkName = %Z, fileFullea->EaName = %ws\n", 
					&ccb->Lcb->ExactCaseLink.LinkName, &fileFullEa->EaName[0]) );

				createContext.EaLength += fileFullEa->NextEntryOffset;
				fileFullEa = (PFILE_FULL_EA_INFORMATION)((UINT8 *)fileFullEa + fileFullEa->NextEntryOffset);
			}

			DebugTrace( 0, Dbg, ("create ccb->Lcb->ExactCaseLink.LinkName = %Z, fileFullea->EaName = %ws\n", 
				&ccb->Lcb->ExactCaseLink.LinkName, &fileFullEa->EaName[0]) );
			
			createContext.EaLength += sizeof(FILE_FULL_EA_INFORMATION) - sizeof(CHAR) + fileFullEa->EaNameLength + fileFullEa->EaValueLength;

			DebugTrace( 0, Dbg, ("RedirectIrp, IRP_MJ_CREATE: Ea is set irpSp->Parameters.Create.EaLength = %d, createContext.EaLength = %d\n", 
								 irpSp->Parameters.Create.EaLength, createContext.EaLength) );		
		} 

		ASSERT( createContext.EaLength <= irpSp->Parameters.Create.EaLength );

		createContext.AllocationSize	= Irp->Overlay.AllocationSize.QuadPart;

		if (!FlagOn(irpSp->Flags, SL_CASE_SENSITIVE)) {
			
			createContext.FileNameLength = fullPathName.Length;
			createContext.FileNameLength += (originalFileName->Length - fileObjectName->Length);
			createContext.RelatedFileHandle = 0; 
			
		} else {

			if (fileObject->RelatedFileObject) {
				
				PCCB	relatedCcb = fileObject->RelatedFileObject->FsContext2;


				if (FlagOn(relatedCcb->NdasNtfsFlags, ND_NTFS_CCB_FLAG_UNOPENED)) {

					try_return( status = STATUS_OBJECT_PATH_NOT_FOUND );
				}
				createContext.RelatedFileHandle = relatedCcb->PrimaryFileHandle; 
			}

			createContext.FileNameLength = originalFileName->Length;
		}
		
		if (volDo->Secondary->Thread.SessionContext.PrimaryMaxDataSize < createContext.EaLength + fullPathName.Length + (originalFileName->Length - fileObjectName->Length)) {
			
			ASSERT( FALSE );
			try_return( status = STATUS_INVALID_PARAMETER );
		}

		ASSERT( secondarySessionResourceAcquired == FALSE );
		
		secondarySessionResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->SessionResource, 
													 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );	
		}

		secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary,
														  IRP_MJ_CREATE,
														  createContext.EaLength + fullPathName.Length + (originalFileName->Length - fileObjectName->Length) );

		if (secondaryRequest == NULL) {

			NtfsRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES, NULL, NULL );
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		
		INITIALIZE_NDFS_REQUEST_HEADER( ndfsRequestHeader,
										NDFS_COMMAND_EXECUTE,
										volDo->Secondary,
										IRP_MJ_CREATE,
										createContext.EaLength + fullPathName.Length + (originalFileName->Length - fileObjectName->Length) );

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER( ndfsWinxpRequestHeader, Irp, irpSp, 0 );

		RtlCopyMemory( &ndfsWinxpRequestHeader->Create, &createContext, sizeof(WINXP_REQUEST_CREATE) );

		ndfsWinxpRequestData = (UINT8 *)(ndfsWinxpRequestHeader+1);

		if(createContext.EaLength) {

			// It have structure align Problem. If you wanna release, Do more
			PFILE_FULL_EA_INFORMATION eaBuffer = Irp->AssociatedIrp.SystemBuffer;
			
			RtlCopyMemory( ndfsWinxpRequestData, eaBuffer, createContext.EaLength );
		}

		if (!FlagOn(irpSp->Flags, SL_CASE_SENSITIVE)) {

			RtlCopyMemory( ndfsWinxpRequestData + createContext.EaLength, 
						   fullPathName.Buffer, 
						   fullPathName.Length );
		
			RtlCopyMemory( ndfsWinxpRequestData + fullPathName.Length, 
						   ((PCHAR)(originalFileName->Buffer)) + fileObjectName->Length,
						   (originalFileName->Length - fileObjectName->Length) );

		} else {

			ASSERT( FALSE );

			RtlCopyMemory( ndfsWinxpRequestData + createContext.EaLength, 
						   originalFileName->Buffer, 
						   originalFileName->Length );
		}

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

		timeOut.QuadPart = -NDASNTFS_TIME_OUT;
		status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
		KeClearEvent( &secondaryRequest->CompleteEvent );
			
		if (status != STATUS_SUCCESS) {

			ASSERT( NDASNTFS_BUG );
			try_return( status );
		}

		if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

			if (IrpContext->OriginatingIrp)
				PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
			DebugTrace( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

			NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
		}

		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

		status = NTOHL(ndfsWinxpReplytHeader->Status4);
		Irp->IoStatus.Information = NTOHL(ndfsWinxpReplytHeader->Information32);

		if(NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_SUCCESS) {

			if(Irp->IoStatus.Status == STATUS_ACCESS_DENIED)
				DebugTrace( 0, Dbg2, ("Irp->IoStatus.Status == STATUS_ACCESS_DENIED DesiredAccess = %x, NTOHL(ndfsWinxpReplytHeader->Status4) = %x\n", 
									irpSp->Parameters.Create.SecurityContext->DesiredAccess, NTOHL(ndfsWinxpReplytHeader->Status4)) );
			
			try_return( status );
		}

		if (secondarySessionResourceAcquired) {

			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
			secondarySessionResourceAcquired = FALSE;
		}

		ASSERT( ndfsWinxpReplytHeader->Open.AttributeTypeCode == $DATA ||
				ndfsWinxpReplytHeader->Open.AttributeTypeCode == $INDEX_ALLOCATION );
		
		if (attrTypeCode != $END)
			ASSERT( attrTypeCode == ndfsWinxpReplytHeader->Open.AttributeTypeCode );

		if (fcb) {
			
			ASSERT( fcb->FileReference.SegmentNumberHighPart == ndfsWinxpReplytHeader->Open.FileReference.SegmentNumberHighPart );
			ASSERT( fcb->FileReference.SegmentNumberLowPart	 == ndfsWinxpReplytHeader->Open.FileReference.SegmentNumberLowPart );
			ASSERT( fcb->FileReference.SequenceNumber		 == ndfsWinxpReplytHeader->Open.FileReference.SequenceNumber );  

			ASSERT( !FlagOn( fcb->FcbState, FCB_STATE_FILE_DELETED) );
			ASSERT( FlagOn( fcb->FcbState, FCB_STATE_IN_FCB_TABLE) );
			ASSERT( !FlagOn( scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED) );

		} else {
		
			fileReference.SegmentNumberHighPart = ndfsWinxpReplytHeader->Open.FileReference.SegmentNumberHighPart;
			fileReference.SegmentNumberLowPart	= ndfsWinxpReplytHeader->Open.FileReference.SegmentNumberLowPart;
			fileReference.SequenceNumber		= ndfsWinxpReplytHeader->Open.FileReference.SequenceNumber;  

			NtfsAcquireFcbTable( IrpContext, &volDo->Vcb );
			acquiredFcbTable = TRUE;

			returnedExistingFcb = TRUE;
			SetFlag( IrpContext->NdasNtfsFlags, NDAS_NTFS_IRP_CONTEXT_FLAG_SECONDARY_CREATE_FILE );
			fcb = NtfsCreateFcb ( IrpContext, &volDo->Vcb, fileReference, BooleanFlagOn(irpSp->Flags, SL_OPEN_PAGING_FILE), FALSE, &returnedExistingFcb );
			ClearFlag( IrpContext->NdasNtfsFlags, NDAS_NTFS_IRP_CONTEXT_FLAG_SECONDARY_CREATE_FILE );

			fcb->ReferenceCount += 1;
			NtfsReleaseFcbTable( IrpContext, &volDo->Vcb );
			acquiredFcbTable = FALSE;

			SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_PAGING );
			NtfsAcquireFcbWithPaging( IrpContext, fcb, 0 );
			acquiredFcbWithPaging = TRUE;

			NtfsAcquireFcbTable( IrpContext, &volDo->Vcb );
			fcb->ReferenceCount -= 1;
			NtfsReleaseFcbTable( IrpContext, &volDo->Vcb );
		}

		fcb->Info.AllocatedLength		= ndfsWinxpReplytHeader->DuplicatedInformation.AllocatedLength;
		fcb->Info.CreationTime			= ndfsWinxpReplytHeader->DuplicatedInformation.CreationTime;
		fcb->Info.FileAttributes		= ndfsWinxpReplytHeader->DuplicatedInformation.FileAttributes;
		fcb->Info.FileSize				= ndfsWinxpReplytHeader->DuplicatedInformation.FileSize;
		fcb->Info.LastAccessTime		= ndfsWinxpReplytHeader->DuplicatedInformation.LastAccessTime;
		fcb->Info.LastChangeTime		= ndfsWinxpReplytHeader->DuplicatedInformation.LastChangeTime;
		fcb->Info.LastModificationTime	= ndfsWinxpReplytHeader->DuplicatedInformation.LastModificationTime;
		fcb->Info. ReparsePointTag		= ndfsWinxpReplytHeader->DuplicatedInformation.ReparsePointTag;

		if (!IsDirectory(&fcb->Info)) 
			ASSERT( ndfsWinxpReplytHeader->Open.TypeOfOpen == UserFileOpen || 
					ndfsWinxpReplytHeader->Open.TypeOfOpen == UserVolumeOpen ||
					ndfsWinxpReplytHeader->Open.TypeOfOpen == UserViewIndexOpen );

		if (fullPathName.Length > 2)
			ASSERT( fullPathName.Buffer[fullPathName.Length/sizeof(WCHAR)-1] != L'\\' );

		if (IsDirectory(&fcb->Info) && fullPathName.Length != 2) {

			if ((status = RtlAppendUnicodeToString(&fullPathName, L"\\")) != STATUS_SUCCESS) {

				NtfsRaiseStatus( IrpContext, status, NULL, NULL );
			}
		}

		NtfsUpcaseName( IrpContext->Vcb->UpcaseTable,
                        IrpContext->Vcb->UpcaseTableSize,
	                    &fullPathName );

		returnedExistingLcb = TRUE;

        lcb = NtfsCreateLcb( IrpContext,
                             (PSCB)ndfsWinxpReplytHeader->Open.Lcb.Scb,
                             fcb,
                             fullPathName,
                             FILE_NAME_NTFS,
                             &returnedExistingLcb );

		ASSERT( lcb );

		if (!returnedExistingLcb) {
		
			fcb->LinkCount++;
			fcb->TotalLinks++;
			//ASSERT( fcb->LinkCount == 1 );
			//ASSERT( fcb->TotalLinks == 1 );
		}

		SetFlag( IrpContext->NdasNtfsFlags, NDAS_NTFS_IRP_CONTEXT_FLAG_SECONDARY_CREATE_FILE );
		scb = NtfsCreateScb ( IrpContext, fcb, ndfsWinxpReplytHeader->Open.AttributeTypeCode, &attrName, FALSE, &returnedExistingScb );
		ClearFlag( IrpContext->NdasNtfsFlags, NDAS_NTFS_IRP_CONTEXT_FLAG_SECONDARY_CREATE_FILE );

		ASSERT( !FlagOn(scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT) );

		if (returnedExistingScb) {

			ASSERT( scb->AttributeTypeCode == ndfsWinxpReplytHeader->Open.AttributeTypeCode );

			if(ndfsWinxpReplytHeader->Open.TypeOfOpen == UserFileOpen) {
			
				ASSERT( scb->Handle == ndfsWinxpReplytHeader->Open.ScbHandle || scb->CleanupCount == 0 );
			}

			scb->Handle = ndfsWinxpReplytHeader->Open.ScbHandle;
		
		} else {
			
			scb->Secondary = volDo->Secondary;
			scb->Handle	= ndfsWinxpReplytHeader->Open.ScbHandle;
		}

		if (ndfsWinxpReplytHeader->Open.TypeOfOpen == UserFileOpen) {

			if (FlagOn(fileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING)) {

			} else {

				fileObject->Flags |= FO_CACHE_SUPPORTED;
			}
		}

		if (createContext.RelatedFileHandle)
			ccbBufferLength = originalFileName->Length;
		else
			ccbBufferLength = (originalFileName->Length - fileObjectName->Length);

		ccbFlags = 0;
		
		if (FlagOn(irpSp->Parameters.Create.Options, FILE_DELETE_ON_CLOSE))

			SetFlag( ccbFlags, CCB_FLAG_DELETE_ON_CLOSE );
		
		if (attrName.Length == 0) {

			SetFlag( ccbFlags, CCB_FLAG_OPEN_AS_FILE );
		}

		if (ndfsWinxpReplytHeader->Open.TypeOfOpen == UserFileOpen) {

			ASSERT( FlagOn((ndfsWinxpReplytHeader->Open.CcbFlags & ~CCB_FLAG_IGNORE_CASE), CCB_FLAG_DELETE_ON_CLOSE | CCB_FLAG_OPEN_AS_FILE) 
					== FlagOn(ccbFlags, CCB_FLAG_DELETE_ON_CLOSE | CCB_FLAG_OPEN_AS_FILE) );
		}

		ccb = NtfsCreateCcb ( IrpContext,
							  fcb,
							  scb,
							  (scb->AttributeTypeCode == $INDEX_ALLOCATION),
							  0,	//  EaModificationCount,
							  ccbFlags,
							  fileObject,
							  0	/*	LastFileNameOffset */ );

		if(ccbBufferLength)
			ccb->Buffer = NtfsAllocatePoolWithTag( PagedPool, ccbBufferLength, 'CftN' );
		else
			ccb->Buffer = NULL;

		ccb->FileObject	= fileObject;

		InitializeListHead( &ccb->ListEntry );

		ccb->BufferLength = ccbBufferLength;

		ExAcquireFastMutex( &volDo->Secondary->FastMutex );
		ccb->SessionId = volDo->Secondary->SessionId;
		ExReleaseFastMutex( &volDo->Secondary->FastMutex );

		ccb->TypeOfOpen = (UCHAR)ndfsWinxpReplytHeader->Open.TypeOfOpen;

		ASSERT( ndfsWinxpReplytHeader->Open.FileHandle != 0) ;
		ccb->PrimaryFileHandle = ndfsWinxpReplytHeader->Open.FileHandle;

		ccb->IrpFlags = Irp->Flags;
		ccb->IrpSpFlags = irpSp->Flags;

		RtlCopyMemory( &ccb->CreateContext, &createContext, sizeof(WINXP_REQUEST_CREATE) );
		ccb->CreateContext.EaLength = 0;
		ccb->CreateContext.AllocationSize = 0;

		if(createContext.EaLength) {

			PFILE_FULL_EA_INFORMATION eaBuffer = Irp->AssociatedIrp.SystemBuffer;

			RtlCopyMemory( ccb->Buffer, eaBuffer, createContext.EaLength );
		}

		if (ccb->CreateContext.RelatedFileHandle) {

			PCCB	relatedCcb2 = fileObject->RelatedFileObject->FsContext2;

			ASSERT( ccb->CreateContext.RelatedFileHandle == relatedCcb2->PrimaryFileHandle );

			RtlCopyMemory( ccb->Buffer + createContext.EaLength, 
						   originalFileName->Buffer, 
						   originalFileName->Length );
		
		} else {

			RtlCopyMemory( ccb->Buffer + createContext.EaLength, 
						   ((PCHAR)(originalFileName->Buffer)) + fileObjectName->Length,
						   (originalFileName->Length - fileObjectName->Length) );
		}

		ExAcquireFastMutex( &volDo->Secondary->RecoveryCcbQMutex );
		InsertHeadList( &volDo->Secondary->RecoveryCcbQueue, &ccb->ListEntry );
		ExReleaseFastMutex( &volDo->Secondary->RecoveryCcbQMutex );

		NtfsLinkCcbToLcb( IrpContext, ccb, lcb );


        NtfsIncrementCleanupCounts( scb,
                                    lcb,
                                    BooleanFlagOn( irpSp->FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING ));

        NtfsIncrementCloseCounts( scb,
                                  BooleanFlagOn( scb->Fcb->FcbState, FCB_STATE_PAGING_FILE ),
                                  (BOOLEAN) IsFileObjectReadOnly( irpSp->FileObject ) );


		if (FlagOn(irpSp->FileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING)  &&
			scb->AttributeTypeCode == $DATA &&
			scb->CleanupCount == scb->NonCachedCleanupCount &&
			scb->NonpagedScb->SegmentObject.ImageSectionObject == NULL &&
			scb->CompressionUnit == 0 &&
			MmCanFileBeTruncated(&scb->NonpagedScb->SegmentObject, NULL) &&
			FlagOn(scb->ScbState, SCB_STATE_HEADER_INITIALIZED) &&
			!FlagOn(scb->Fcb->FcbState, FCB_STATE_SYSTEM_FILE)) {

			NtfsFlushAndPurgeScb( IrpContext, scb, NULL );
		}


		if (ndfsWinxpReplytHeader->Open.TypeOfOpen == UserFileOpen) {

			PNDFS_NTFS_MCB_ENTRY	mcbEntry;
			ULONG			index;

			BOOLEAN			lookupResut;
			VCN				vcn;
			LCN				lcn;
			LCN				startingLcn;
			LONGLONG		clusterCount;

			if (Irp->Overlay.AllocationSize.QuadPart) {

				ASSERT( Irp->Overlay.AllocationSize.QuadPart == NTOHLL(ndfsWinxpReplytHeader->AllocationSize8) );
			}

			if (NTOHLL(ndfsWinxpReplytHeader->AllocationSize8)) {

				ASSERT( ndfsWinxpReplytHeader->FileInformationSet );
				ASSERT( ndfsWinxpReplytHeader->FileSize8 <= ndfsWinxpReplytHeader->AllocationSize8 );
			}

			if (scb->CloseCount != 1) {

				if (NTOHLL(ndfsWinxpReplytHeader->AllocationSize8) != scb->Header.AllocationSize.QuadPart) {
				
					ASSERT( createDisposition == FILE_SUPERSEDE ||
							createDisposition == FILE_OVERWRITE || 
							createDisposition == FILE_OVERWRITE_IF );
					
					if (NTOHLL(ndfsWinxpReplytHeader->AllocationSize8))
						ASSERT( Irp->Overlay.AllocationSize.QuadPart );
				}
			}


			fileObject->SectionObjectPointer = &scb->NonpagedScb->SegmentObject;

			if (createDisposition == FILE_SUPERSEDE ||
				createDisposition == FILE_OVERWRITE ||
				createDisposition == FILE_OVERWRITE_IF) {

				if (scb->Header.ValidDataLength.QuadPart != 0) {

					//BOOLEAN	purgeResult = FALSE;

					//purgeResult = CcPurgeCacheSection( &scb->NonpagedScb->SegmentObject, NULL, 0, FALSE );
					//ASSERT( purgeResult == TRUE );

					NtfsFlushAndPurgeScb( IrpContext, scb, NULL );
				}

				scb->Header.FileSize.QuadPart = 0;
				scb->Header.ValidDataLength.QuadPart = 0;
				scb->ValidDataToDisk = 0;
#if __NDAS_NTFS_DBG__
				strncpy(scb->LastUpdateFile, strrchr(__FILE__,'\\')+1, 15); 
				scb->LastUpdateLine = __LINE__; 
#endif				

				if (NTOHLL(ndfsWinxpReplytHeader->AllocationSize8)) {

					mcbEntry = (PNDFS_NTFS_MCB_ENTRY)( ndfsWinxpReplytHeader+1 );

					for (index=0, vcn=0; index < NTOHL(ndfsWinxpReplytHeader->NumberOfMcbEntry4); index++, mcbEntry++) {

						ASSERT( mcbEntry->Vcn == vcn );

						lookupResut = NtfsLookupNtfsMcbEntry( &scb->Mcb, vcn, &lcn, &clusterCount, &startingLcn, NULL, NULL, NULL );
					
						if (vcn < LlClustersFromBytes(&volDo->Secondary->VolDo->Vcb, scb->Header.AllocationSize.QuadPart)) {

							ASSERT( lookupResut == TRUE );

							if (vcn < LlClustersFromBytes(&volDo->Secondary->VolDo->Vcb, NTOHLL(ndfsWinxpReplytHeader->AllocationSize8))) {
							
								ASSERT( startingLcn == lcn );
								ASSERT( vcn == mcbEntry->Vcn );
								ASSERT( lcn == mcbEntry->Lcn );
								ASSERT( clusterCount <= mcbEntry->ClusterCount );
							
								if (clusterCount < mcbEntry->ClusterCount) {

									NtfsAddNtfsMcbEntry( &scb->Mcb, 
														 mcbEntry->Vcn, 
														 mcbEntry->Lcn, 
														 (LONGLONG)mcbEntry->ClusterCount, 
														 FALSE );
	
									lookupResut = NtfsLookupNtfsMcbEntry( &scb->Mcb, vcn, &lcn, &clusterCount, &startingLcn, NULL, NULL, NULL );

									ASSERT( lookupResut == TRUE );
									ASSERT( startingLcn == lcn );
									ASSERT( vcn == mcbEntry->Vcn );
									ASSERT( lcn == mcbEntry->Lcn );
									ASSERT( clusterCount == mcbEntry->ClusterCount );
								}
							}
					
						} else { 

							ASSERT( lookupResut == FALSE || lcn == UNUSED_LCN );

							NtfsAddNtfsMcbEntry( &scb->Mcb, 
												 mcbEntry->Vcn, 
												 mcbEntry->Lcn, 
												 (LONGLONG)mcbEntry->ClusterCount, 
												 FALSE );
						}

						vcn += mcbEntry->ClusterCount;
					}
			
					ASSERT( LlBytesFromClusters(&volDo->Secondary->VolDo->Vcb, vcn) == NTOHLL(ndfsWinxpReplytHeader->AllocationSize8) );
				}

				if (scb->Header.AllocationSize.QuadPart < (INT64)NTOHLL(ndfsWinxpReplytHeader->AllocationSize8)) {

					ASSERT( ndfsWinxpReplytHeader->Open.TruncateOnClose );

					scb->Header.AllocationSize.QuadPart = NTOHLL(ndfsWinxpReplytHeader->AllocationSize8);

					if (fileObject->SectionObjectPointer->DataSectionObject != NULL && fileObject->PrivateCacheMap == NULL) {

						CcInitializeCacheMap( fileObject,
											  (PCC_FILE_SIZES)&scb->Header.AllocationSize,
											  FALSE,
											  &NtfsData.CacheManagerCallbacks,
											  scb );
					}

					NtfsSetBothCacheSizes( fileObject,
										   (PCC_FILE_SIZES)&scb->Header.AllocationSize,
										   scb );

					SetFlag( scb->ScbState, SCB_STATE_TRUNCATE_ON_CLOSE );
				}

                if (scb->Header.AllocationSize.QuadPart > (INT64)NTOHLL(ndfsWinxpReplytHeader->AllocationSize8)) {

					scb->Header.AllocationSize.QuadPart = NTOHLL(ndfsWinxpReplytHeader->AllocationSize8);

					NtfsRemoveNtfsMcbEntry( &scb->Mcb, 
											LlClustersFromBytes(&volDo->Secondary->VolDo->Vcb, scb->Header.AllocationSize.QuadPart), 
											0xFFFFFFFF );

					NtfsSetBothCacheSizes( fileObject,
										   (PCC_FILE_SIZES)&scb->Header.AllocationSize,
										   scb );
				}
			
			} else {
			
				if (scb->Header.AllocationSize.QuadPart != 0) {

					if (FlagOn(scb->ScbState, SCB_STATE_TRUNCATE_ON_CLOSE))
						ASSERT( FlagOn(scb->ScbState, SCB_STATE_TRUNCATE_ON_CLOSE) == ndfsWinxpReplytHeader->Open.TruncateOnClose );
					ASSERT( scb->Header.FileSize.QuadPart == NTOHLL(ndfsWinxpReplytHeader->FileSize8) );
					ASSERT( scb->Header.AllocationSize.QuadPart == NTOHLL(ndfsWinxpReplytHeader->AllocationSize8) );
			
				} else {

					if (NTOHLL(ndfsWinxpReplytHeader->AllocationSize8)) {

						ASSERT( scb->CloseCount == 1 );

						mcbEntry = (PNDFS_NTFS_MCB_ENTRY)( ndfsWinxpReplytHeader+1 );

						for (index=0, vcn=0; index < NTOHL(ndfsWinxpReplytHeader->NumberOfMcbEntry4); index++, mcbEntry++) {

							ASSERT( mcbEntry->Vcn == vcn );

							NtfsAddNtfsMcbEntry( &scb->Mcb, 
												 mcbEntry->Vcn, 
												 mcbEntry->Lcn, 
												 (LONGLONG)mcbEntry->ClusterCount, 
												 FALSE );

							vcn += mcbEntry->ClusterCount;
						}
			
						ASSERT( LlBytesFromClusters(&volDo->Secondary->VolDo->Vcb, vcn) == NTOHLL(ndfsWinxpReplytHeader->AllocationSize8) );

						scb->Header.FileSize.QuadPart = NTOHLL(ndfsWinxpReplytHeader->FileSize8);
						scb->Header.AllocationSize.QuadPart = NTOHLL(ndfsWinxpReplytHeader->AllocationSize8);
						scb->Header.ValidDataLength.QuadPart = NTOHLL(ndfsWinxpReplytHeader->ValidDataLength8);
#if __NDAS_NTFS_DBG__
						strncpy(scb->LastUpdateFile, strrchr(__FILE__,'\\')+1, 15); 
						scb->LastUpdateLine = __LINE__; 
#endif				
						if (scb->Header.FileSize.QuadPart == 0) {
							
							ASSERT( ndfsWinxpReplytHeader->Open.TruncateOnClose );
							SetFlag( scb->ScbState, SCB_STATE_TRUNCATE_ON_CLOSE );
						
						} else
							ASSERT( ndfsWinxpReplytHeader->Open.TruncateOnClose == FALSE );

						if (fileObject->SectionObjectPointer->DataSectionObject != NULL && fileObject->PrivateCacheMap == NULL) {

							CcInitializeCacheMap( fileObject,
												  (PCC_FILE_SIZES)&scb->Header.AllocationSize,
												  FALSE,
												  &NtfsData.CacheManagerCallbacks,
												  scb );
						}

						NtfsSetBothCacheSizes( fileObject,
											   (PCC_FILE_SIZES)&scb->Header.AllocationSize,
											   scb );
					}
				}
			}

#if DBG
			{
				BOOLEAN			lookupResut;
				VCN				vcn;
				LCN				lcn;
				LCN				startingLcn;
				LONGLONG		clusterCount;

				vcn = 0;
				while (1) {

					lookupResut = NtfsLookupNtfsMcbEntry( &scb->Mcb, vcn, &lcn, &clusterCount, &startingLcn, NULL, NULL, NULL );
					if (lookupResut == FALSE || lcn == UNUSED_LCN)
						break;

					vcn += clusterCount;
				}

				ASSERT( LlBytesFromClusters(&volDo->Secondary->VolDo->Vcb, vcn) == scb->Header.AllocationSize.QuadPart );
			}
#endif


			SetFlag( scb->ScbState, SCB_STATE_HEADER_INITIALIZED );
		}

		fileObject->FsContext = scb;
		fileObject->FsContext2 = ccb;

		fileObject->Vpb = volDo->Secondary->VolDo->Vcb.Vpb;
	
try_exit:  NOTHING;

	} finally {

		if(AbnormalTermination()) {

			status = IrpContext->ExceptionStatus;
		}

		if (acquiredMcb == TRUE) 
			NtfsReleaseNtfsMcbMutex( &scb->Mcb );

		if(acquiredFcbWithPaging == TRUE)
			NtfsReleaseFcbWithPaging( IrpContext, fcb );

		if (acquiredFcbTable == TRUE)
			NtfsReleaseFcbTable( IrpContext, &volDo->Vcb );

		if(fullPathNameBuffer)
			NtfsFreePool( fullPathNameBuffer );
		
		if(secondaryRequest)
			DereferenceSecondaryRequest( secondaryRequest );

		if (openFileHandle) {

			if (secondarySessionResourceAcquired == FALSE) {

				secondarySessionResourceAcquired 
					= SecondaryAcquireResourceExclusiveLite( IrpContext, 
															 &volDo->SessionResource, 
															 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );
			}

            if (!FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

				ClosePrimaryFile( volDo->Secondary, openFileHandle );
				openFileHandle = 0;
			}
		}

		if (secondarySessionResourceAcquired) {

			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
		}

		if (secondaryCreateResourceAcquired) {

			SecondaryReleaseResourceLite( IrpContext, &volDo->CreateResource );		
		}
		
		if (!NT_SUCCESS(status)) {

			OplockCleanup->FileObject->FileName = *originalFileName;
		}

		if (vcbAcquired)
			NtfsReleaseVcb( IrpContext, IrpContext->Vcb );

		if (!NT_SUCCESS(status)) {

			ClearFlag( IrpContext->NdasNtfsFlags, NDAS_NTFS_IRP_CONTEXT_FLAG_SECONDARY_CONTEXT );
			ClearFlag( IrpContext->NdasNtfsFlags, NDAS_NTFS_IRP_CONTEXT_FLAG_SECONDARY_FILE );
		}
	}

	if(irpSp->Parameters.Create.EaLength) 
		DebugTrace( 0, Dbg2, ("create originalFileName = %Z status = %x\n", originalFileName, status) );
		

	if (FlagOn( IrpContext->State, IRP_CONTEXT_STATE_EFS_CREATE ) || (ARGUMENT_PRESENT( NetworkInfo ))) {

		NtfsCompleteRequest( IrpContext, NULL, status );
#ifdef NTFSDBG
		ASSERT( None == IrpContext->OwnershipState );
#endif

	} else {

		NtfsCompleteRequest( IrpContext, Irp, status );
	}
			
	return status;
}


VOID
ClosePrimaryFile(
	IN PSECONDARY	Secondary,
	IN UINT64			FileHandle
	)
{
	NTSTATUS					status;
	PSECONDARY_REQUEST			secondaryRequest = NULL;
	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;

	LARGE_INTEGER				timeOut;


	secondaryRequest = AllocateWinxpSecondaryRequest( Secondary, IRP_MJ_CLOSE, 0 );

	if (secondaryRequest == NULL) {

		ASSERT( FALSE );
		status = STATUS_INSUFFICIENT_RESOURCES;	
		return;
	}

	ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;

	INITIALIZE_NDFS_REQUEST_HEADER( ndfsRequestHeader, NDFS_COMMAND_EXECUTE, Secondary, IRP_MJ_CLOSE, 0	);

	ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
	ASSERT(ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData);
	
	//ndfsWinxpRequestHeader->IrpTag4				= (UINT32)secondaryRequest;
	ndfsWinxpRequestHeader->IrpMajorFunction	= IRP_MJ_CLOSE;
	ndfsWinxpRequestHeader->IrpMinorFunction	= 0;
	ndfsWinxpRequestHeader->FileHandle8			= HTONLL(FileHandle);
	ndfsWinxpRequestHeader->IrpFlags4			= 0;
	ndfsWinxpRequestHeader->IrpSpFlags			= 0;
	

	secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
	
	QueueingSecondaryRequest( Secondary, secondaryRequest );
				
	timeOut.QuadPart = -NDASNTFS_TIME_OUT;
	
	status = KeWaitForSingleObject(	&secondaryRequest->CompleteEvent,
										Executive,
										KernelMode,
										FALSE,
										&timeOut );

	KeClearEvent( &secondaryRequest->CompleteEvent );

	if (status != STATUS_SUCCESS) {
	
		ASSERT( NDASNTFS_BUG );

		secondaryRequest = NULL;
		status = STATUS_TIMEOUT;	

		return;
	}

	status = secondaryRequest->ExecuteStatus;	
	DereferenceSecondaryRequest( secondaryRequest );
	secondaryRequest = NULL;

	return;
}


NTSTATUS
Secondary_MakeFullPathName(
	IN  PFILE_OBJECT	FileObject,
	IN  PUNICODE_STRING	FileObjectName,
	OUT PUNICODE_STRING	FullPathName
	)
{
	NTSTATUS	status;

	if (FileObjectName->Length > NDFS_MAX_PATH) {

		ASSERT( FALSE );
		return STATUS_NAME_TOO_LONG;
	}

	if (FileObject->RelatedFileObject) {
	
		if (IS_SECONDARY_FILEOBJECT(FileObject->RelatedFileObject)) {

			PCCB relatedCcb = FileObject->RelatedFileObject->FsContext2;
		
			if ((relatedCcb->Lcb->ExactCaseLink.LinkName.Length + sizeof(WCHAR) + FileObjectName->Length) > NDFS_MAX_PATH) {

				ASSERT( FALSE );
				return STATUS_NAME_TOO_LONG;
			}

			if ((status = RtlAppendUnicodeStringToString(FullPathName, &relatedCcb->Lcb->ExactCaseLink.LinkName)) != STATUS_SUCCESS) {
		
				ASSERT( FALSE );
				return status;
			}
		
		} else {

			PUNICODE_STRING	relatedFileObjectName;
			PSCB			relatedFileObjectScb;

			relatedFileObjectName = &FileObject->RelatedFileObject->FileName;
            relatedFileObjectScb = FileObject->RelatedFileObject->FsContext;

			if ((relatedFileObjectName->Length + sizeof(WCHAR) + FileObjectName->Length) > NDFS_MAX_PATH) {

				ASSERT( FALSE );
				return STATUS_NAME_TOO_LONG;
			}

			if (relatedFileObjectName->Buffer[0] != L'\\') {

				ASSERT( FALSE );
				return STATUS_OBJECT_PATH_NOT_FOUND;
			}

			if ((status = RtlAppendUnicodeStringToString(FullPathName, relatedFileObjectName)) != STATUS_SUCCESS) {
		
				ASSERT( FALSE );
				return status;
			}

			if (IsDirectory(&relatedFileObjectScb->Fcb->Info) && FullPathName->Length != 2) {

				if ((status = RtlAppendUnicodeToString(FullPathName, L"\\")) != STATUS_SUCCESS) {

					ASSERT( FALSE );
					return status;
				}
			}

			DebugTrace( 0, Dbg, ("FullPathName = %Z\n", FullPathName) );
		}
	}

	if (FileObjectName->Length == 0) 
		return STATUS_SUCCESS;
	
	if ((status = RtlAppendUnicodeStringToString(FullPathName, FileObjectName)) != STATUS_SUCCESS) {

		ASSERT( FALSE );
		return status;
	}

	if (FullPathName->Length != 2)
		ASSERT( FullPathName->Buffer[FullPathName->Length/sizeof(WCHAR)-1] != L'\\' );

	return STATUS_SUCCESS;
}


#endif



