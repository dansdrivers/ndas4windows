#include "FatProcs.h"

#if __NDAS_FAT_SECONDARY__

#if __NDAS_FAT_DBG__

#define Dbg                              (DEBUG_TRACE_SECONDARY)
#define Dbg2                             (DEBUG_INFO_SECONDARY)

#endif

#pragma warning(error:4100)   // Unreferenced formal parameter
#pragma warning(error:4101)   // Unreferenced local variable
#pragma warning(error:4705)   // Statement has no effect
#pragma warning(disable:4116) // unnamed type definition in parentheses



NTSTATUS
NdasFatSecondaryCommonCreate (
	IN PIRP_CONTEXT IrpContext,
	IN PIRP			Irp,
	OUT PBOOLEAN	DoMore
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

	UNICODE_STRING				originalFileName;
	UNICODE_STRING				fullFileName;
	PUNICODE_STRING				fileObjectName;

	UNICODE_STRING				fullPathName;
	PWCHAR						fullPathNameBuffer = NULL; 

	UINT64 						openFileHandle = 0;

	PSECONDARY_REQUEST			secondaryRequest = NULL;

	PNDFS_REQUEST_HEADER		ndfsRequestHeader;
	PNDFS_WINXP_REQUEST_HEADER	ndfsWinxpRequestHeader;
	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeader;
	UINT8						*ndfsWinxpRequestData;

	PSECONDARY_REQUEST			secondaryRequestInfo = NULL;

	PNDFS_WINXP_REPLY_HEADER	ndfsWinxpReplytHeaderInfo = NULL;

	LARGE_INTEGER				timeOut;

	PFCB						fcb = NULL;
	BOOLEAN						acquiredFcbWithPaging = FALSE;
	BOOLEAN						vcbAcquired	= FALSE;

	WINXP_REQUEST_CREATE		createContext = {0};

	ULONG						ccbFlags;
	ULONG						ccbBufferLength;

	PCCB						ccb = NULL;

	NDAS_ASSERT( DoMore != NULL );
	NDAS_ASSERT( KeGetCurrentIrql() == PASSIVE_LEVEL );

	DebugTrace2( 0, Dbg, ("NdasFatSecondaryCommonCreate: NdNtfsSecondaryCommonCreate: FileName = %Z\n", &fileObject->FileName) );

	if (!FlagOn(((PVOLUME_DEVICE_OBJECT)irpSp->DeviceObject)->NdasFatFlags, ND_FAT_DEVICE_FLAG_MOUNTED)) {
	
		*DoMore = TRUE;
		return STATUS_SUCCESS;
	}

	if (FlagOn(((PVOLUME_DEVICE_OBJECT)irpSp->DeviceObject)->Vcb.VcbState, VCB_STATE_FLAG_LOCKED)) {

		*DoMore = TRUE;
		return STATUS_SUCCESS;
	}

	if (volDo->NetdiskEnableMode == NETDISK_SECONDARY2PRIMARY) {

		if (FlagOn(IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_OPEN_TRY_ON_SECONDARY)) {
			
			DebugTrace2( 0, Dbg, ("NdasFatSecondaryCommonCreate: NdNtfsSecondaryCommonCreate: FileName = %Z\n", &fileObject->FileName) );
			DebugTrace2( 0, Dbg, ("NdasFatSecondaryCommonCreate: NDAS_FAT_IRP_CONTEXT_FLAG_OPEN_TRY_ON_SECONDARY\n") );
			
			NDAS_ASSERT( !FlagOn(volDo->Secondary->Flags, SECONDARY_FLAG_RECONNECTING) );

			//FatDebugTraceLevel |= DEBUG_TRACE_SECONDARY;
		
		} else if (!(irpSp->FileObject->RelatedFileObject && IS_SECONDARY_FILEOBJECT(irpSp->FileObject->RelatedFileObject))) {

			*DoMore = TRUE;
			return STATUS_SUCCESS;
		}
	
	} else {
	
		NDAS_ASSERT( fileObject->FsContext == NULL );	
	}

	*DoMore = FALSE;

    if (!FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT)) {

        DebugTrace2( 0, Dbg, ("Can't wait in create\n") );

        status = FatFsdPostRequest( IrpContext, Irp );

        DebugTrace2( -1, Dbg, ("NtfsCommonCreate:  Exit -> %08lx\n", status) );
        return status;
    }

	try {

		SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_SECONDARY_CONTEXT );

		fullFileName = originalFileName = fileObject->FileName;
		fileObjectName = &fileObject->FileName;

		if ((FILE_OPEN_BY_FILE_ID & irpSp->Parameters.Create.Options) || BooleanFlagOn(irpSp->Flags, SL_OPEN_PAGING_FILE)) {

			PrintIrp( Dbg2, "NdNtfsSecondaryCommonCreate", NULL, Irp );
			NDAS_ASSERT(FALSE);

			try_return( status = STATUS_NOT_SUPPORTED );
		}

		if (RtlEqualUnicodeString(&FatData.MountMgrRemoteDatabase, &fileObject->FileName, TRUE)		|| 
			RtlEqualUnicodeString(&FatData.ExtendReparse, &fileObject->FileName, TRUE)				||
			RtlEqualUnicodeString(&FatData.MountPointManagerRemoteDatabase, &fileObject->FileName, TRUE)) {

			DebugTrace2( 0, Dbg2, ("%s: %wZ returned with STATUS_OBJECT_NAME_NOT_FOUND\n", __FUNCTION__, &fileObject->FileName) );

			try_return( status = STATUS_OBJECT_NAME_NOT_FOUND );
		}

		fullPathNameBuffer = FsRtlAllocatePoolWithTag( NonPagedPoolCacheAligned, NDFS_MAX_PATH*sizeof(WCHAR), TAG_FILENAME_BUFFER );
		RtlInitEmptyUnicodeString( &fullPathName, fullPathNameBuffer, NDFS_MAX_PATH*sizeof(WCHAR) );

		while (fileObjectName->Length > 2 && fileObjectName->Buffer[fileObjectName->Length/sizeof(WCHAR)-1] == L'\\') {

			fileObjectName->Length -= 2;
		}

		if ((status = Secondary_MakeFullPathName(fileObject, fileObjectName, &fullPathName)) != STATUS_SUCCESS) {
			
			NDAS_ASSERT(FALSE);
			try_return(status);
		}

		FatAcquireExclusiveVcb( IrpContext, IrpContext->Vcb );
		vcbAcquired = TRUE;

		secondaryCreateResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->CreateResource, 
													 BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

		if (fullPathName.Length != 0 && (irpSp->Parameters.Create.Options & 0xFF000000) != (FILE_CREATE << 24)) {

			secondarySessionResourceAcquired 
				= SecondaryAcquireResourceExclusiveLite( IrpContext, 
														 &volDo->SessionResource, 
														 BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

			if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

				PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
				NDAS_ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
				SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_DONT_POST_REQUEST );
				FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );	
			}

			secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary, 
															  IRP_MJ_CREATE, 
															  fullPathName.Length + (originalFileName.Length - fileObjectName->Length) );

			if (secondaryRequest == NULL) {

				FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
			}

			ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;

			INITIALIZE_NDFS_REQUEST_HEADER( ndfsRequestHeader,
											NDFS_COMMAND_EXECUTE,
											volDo->Secondary,
											IRP_MJ_CREATE,
											fullPathName.Length + (originalFileName.Length - fileObjectName->Length) );

			ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
			NDAS_ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );

#if defined(_WIN64)
			ndfsWinxpRequestHeader->IrpTag4   = HTONL((UINT32)((UINT64)secondaryRequest));
#else
			ndfsWinxpRequestHeader->IrpTag4   = HTONL((UINT32)secondaryRequest);
#endif
			ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_CREATE;
			ndfsWinxpRequestHeader->IrpMinorFunction = 0;

			ndfsWinxpRequestHeader->FileHandle8 = 0;

			ndfsWinxpRequestHeader->IrpFlags4   = 0;
			ndfsWinxpRequestHeader->IrpSpFlags = irpSp->Flags;

			ndfsWinxpRequestHeader->Create.AllocationSize = 0;
			ndfsWinxpRequestHeader->Create.EaLength = 0;
			ndfsWinxpRequestHeader->Create.FileAttributes = 0;

			ndfsWinxpRequestHeader->Create.Options = irpSp->Parameters.Create.Options & ~FILE_DELETE_ON_CLOSE;
			ndfsWinxpRequestHeader->Create.Options &= 0x00FFFFFF;
			ndfsWinxpRequestHeader->Create.Options |= (FILE_OPEN << 24);
		
			if (!FlagOn(irpSp->Flags, SL_CASE_SENSITIVE)) {
			
				ndfsWinxpRequestHeader->Create.FileNameLength = fullPathName.Length;
				ndfsWinxpRequestHeader->Create.FileNameLength += (originalFileName.Length - fileObjectName->Length);
				ndfsWinxpRequestHeader->Create.RelatedFileHandle = 0; 
			
			} else {

				NDAS_ASSERT(FALSE);

				if (fileObject->RelatedFileObject) {
				
					PCCB relatedCcb = fileObject->RelatedFileObject->FsContext2;


					if (FlagOn(relatedCcb->NdasFatFlags, ND_FAT_CCB_FLAG_UNOPENED)) {

						try_return( status = STATUS_OBJECT_PATH_NOT_FOUND );
					}
					ndfsWinxpRequestHeader->Create.RelatedFileHandle = relatedCcb->PrimaryFileHandle; 
				}

				ndfsWinxpRequestHeader->Create.FileNameLength = originalFileName.Length;
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
							   ((PCHAR)(originalFileName.Buffer)) + fileObjectName->Length,
							   (originalFileName.Length - fileObjectName->Length) );
			
			} else {
			
				RtlCopyMemory( ndfsWinxpRequestData,
							   originalFileName.Buffer,
							   originalFileName.Length );
			}

			secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
			QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

			timeOut.QuadPart = -NDASFAT_TIME_OUT;		
			status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
			KeClearEvent( &secondaryRequest->CompleteEvent );

			if (status != STATUS_SUCCESS) {

				NDAS_ASSERT(FALSE);

				try_return(status);
			}

			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
			secondarySessionResourceAcquired = FALSE;

			if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

				if (IrpContext->OriginatingIrp)
					PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );

				DebugTrace2( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

				NDAS_ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
				SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_DONT_POST_REQUEST );
				FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
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

				DebugTrace2( 0, Dbg2, ("NdNtfsSecondaryCommonCreate: NTOHL(ndfsWinxpReplytHeader->Status4) = %x\n", 
									   NTOHL(ndfsWinxpReplytHeader->Status4)) );
			}

			if (NTOHL(ndfsWinxpReplytHeader->Status4) == STATUS_OBJECT_NAME_INVALID) {

				DebugTrace2( 0, Dbg, ("orginalFileName = %Z\n", originalFileName) );

				if (fileObject->RelatedFileObject) {

					DebugTrace2( 0, Dbg, ("RelatedFileObjectName = %Z\n", &fileObject->RelatedFileObject->FileName) );
				}
			}

			DebugTrace2( 0, Dbg, ("NTOHL(ndfsWinxpReplytHeader->Status4) = %x, ndfsWinxpReplytHeader->Open.FileHandle = %x\n", 
									NTOHL(ndfsWinxpReplytHeader->Status4), ndfsWinxpReplytHeader->Open.FileHandle) );

			if (NTOHL(ndfsWinxpReplytHeader->Status4) == STATUS_SUCCESS) {
	
				if (ndfsWinxpReplytHeader->Open.TypeOfOpen == UserFileOpen) {

					if (Irp->Overlay.AllocationSize.QuadPart) {

						secondaryRequestInfo = AllocateWinxpSecondaryRequest( volDo->Secondary, 
																			  IRP_MJ_SET_INFORMATION,
																			  volDo->Secondary->Thread.SessionContext.SecondaryMaxDataSize );

						if (secondaryRequestInfo == NULL) {

							FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
						}

						ndfsRequestHeader = &secondaryRequestInfo->NdfsRequestHeader;
						INITIALIZE_NDFS_REQUEST_HEADER(	ndfsRequestHeader, NDFS_COMMAND_EXECUTE, volDo->Secondary, IRP_MJ_SET_INFORMATION, 0 );

						ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
						NDAS_ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequestInfo->NdfsRequestData );

						//ndfsWinxpRequestHeader->IrpTag4   = (UINT32)Irp;
						ndfsWinxpRequestHeader->IrpMajorFunction = IRP_MJ_SET_INFORMATION;
						ndfsWinxpRequestHeader->IrpMinorFunction = 0;

						ndfsWinxpRequestHeader->FileHandle8 = HTONLL(ndfsWinxpReplytHeader->Open.FileHandle);

						ndfsWinxpRequestHeader->IrpFlags4  = 0;
						ndfsWinxpRequestHeader->IrpSpFlags = 0;

						ndfsWinxpRequestHeader->SetFile.FileHandle				= 0;
						ndfsWinxpRequestHeader->SetFile.Length					= sizeof(FILE_ALLOCATION_INFORMATION);
						ndfsWinxpRequestHeader->SetFile.FileInformationClass	= FileAllocationInformation;
						ndfsWinxpRequestHeader->SetFile.AllocationInformation.AllocationSize = Irp->Overlay.AllocationSize.QuadPart;
						//ndfsWinxpRequestHeader->SetFile.Length				= sizeof(FILE_END_OF_FILE_INFORMATION);
						//ndfsWinxpRequestHeader->SetFile.FileInformationClass	= FileEndOfFileInformation;
						//ndfsWinxpRequestHeader->SetFile.EndOfFileInformation.EndOfFile = Irp->Overlay.AllocationSize.QuadPart;

						secondaryRequestInfo->RequestType = SECONDARY_REQ_SEND_MESSAGE;

						QueueingSecondaryRequest( volDo->Secondary, secondaryRequestInfo );

						timeOut.QuadPart = -NDASFAT_TIME_OUT;
						status = KeWaitForSingleObject( &secondaryRequestInfo->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
						if (status != STATUS_SUCCESS) {

							NDAS_ASSERT(FALSE);

							secondaryRequestInfo = NULL;
							status = STATUS_IO_DEVICE_ERROR;
							leave;
						}

						KeClearEvent( &secondaryRequestInfo->CompleteEvent );

						if (secondaryRequestInfo->ExecuteStatus != STATUS_SUCCESS) {

							NDAS_ASSERT(FALSE);

							DebugTrace2( 0, Dbg2, ("secondaryRequestInfo->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

							FatRaiseStatus( IrpContext, secondaryRequestInfo->ExecuteStatus );
						}

						ndfsWinxpReplytHeaderInfo = (PNDFS_WINXP_REPLY_HEADER)secondaryRequestInfo->NdfsReplyData;
					}
				}

				openFileHandle = ndfsWinxpReplytHeader->Open.FileHandle;
				DebugTrace2( 0, Dbg, ("ndfsWinxpReplytHeader->Open.FileHandle = %x\n", ndfsWinxpReplytHeader->Open.FileHandle) );
			
				fcb = Secondary_LookUpFcbByHandle( volDo->Secondary, 
												   ndfsWinxpReplytHeader->Open.FcbHandle,
												   FALSE,
												   FALSE );

				if (fcb && ndfsWinxpReplytHeader->Open.TypeOfOpen == UserFileOpen) {

					FatAcquireExclusiveFcb( IrpContext, fcb );
					ExAcquireSharedStarveExclusive( fcb->Header.PagingIoResource, TRUE );
					acquiredFcbWithPaging = TRUE;
			
					if (FlagOn(irpSp->Parameters.Create.SecurityContext->DesiredAccess, FILE_WRITE_DATA) || 
						BooleanFlagOn(irpSp->Parameters.Create.Options, FILE_DELETE_ON_CLOSE)) {

						InterlockedIncrement( &fcb->OpenCount );

						try {

							if (fcb->NonPaged->SectionObjectPointers.ImageSectionObject != NULL) {
								
								if (!MmFlushImageSection( &fcb->NonPaged->SectionObjectPointers, MmFlushForWrite )) {

									DebugTrace2( 0, Dbg, ("Couldn't flush image section\n") );

									status = BooleanFlagOn(irpSp->Parameters.Create.Options, FILE_DELETE_ON_CLOSE) ? 
														   STATUS_CANNOT_DELETE : STATUS_SHARING_VIOLATION;
								
								} else
									status = STATUS_SUCCESS;
							}

						} finally {

							InterlockedDecrement( &fcb->OpenCount );
						}

						if (status != STATUS_SUCCESS)
							try_return( status );
					}

					if (createDisposition == FILE_SUPERSEDE || createDisposition == FILE_OVERWRITE || createDisposition == FILE_OVERWRITE_IF) {

						InterlockedIncrement( &fcb->OpenCount );

						if (!MmCanFileBeTruncated(&fcb->NonPaged->SectionObjectPointers, &FatLargeZero)) {

							InterlockedDecrement( &fcb->OpenCount );
							try_return( status = STATUS_USER_MAPPED_FILE );
						} 
		
						InterlockedDecrement( &fcb->OpenCount );
					}		
				}
	
				secondarySessionResourceAcquired 
					= SecondaryAcquireResourceExclusiveLite( IrpContext, 
															 &volDo->SessionResource, 
															 BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

				if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

					PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
					NDAS_ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
					SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_DONT_POST_REQUEST );
					FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );	
				}

				ClosePrimaryFile( volDo->Secondary, openFileHandle );
				openFileHandle = 0;
	
				SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
				secondarySessionResourceAcquired = FALSE;
			}

			DereferenceSecondaryRequest( secondaryRequest );
			secondaryRequest = NULL;
		}

		DebugTrace2( 0, Dbg, ("Second Phage\n") );

		createContext.SecurityContext.DesiredAccess		= irpSp->Parameters.Create.SecurityContext->DesiredAccess;
		createContext.SecurityContext.FullCreateOptions	= irpSp->Parameters.Create.SecurityContext->FullCreateOptions;
		createContext.Options							= irpSp->Parameters.Create.Options;
		createContext.FileAttributes					= irpSp->Parameters.Create.FileAttributes;
		createContext.ShareAccess						= irpSp->Parameters.Create.ShareAccess;

		if (irpSp->Parameters.Create.EaLength == 0) {

			createContext.EaLength = 0;

		} else {

			PFILE_FULL_EA_INFORMATION	fileFullEa = (PFILE_FULL_EA_INFORMATION)Irp->AssociatedIrp.SystemBuffer;

			NDAS_ASSERT( Irp->AssociatedIrp.SystemBuffer != NULL );

			createContext.EaLength = 0;
			
			while (fileFullEa->NextEntryOffset) {

				DebugTrace2( 0, Dbg, ("create ccb->Lcb->ExactCaseLink.LinkName = %Z, fileFullea->EaName = %ws\n", 
									   &fcb->FullFileName, &fileFullEa->EaName[0]) );

				createContext.EaLength += fileFullEa->NextEntryOffset;
				fileFullEa = (PFILE_FULL_EA_INFORMATION)((UINT8 *)fileFullEa + fileFullEa->NextEntryOffset);
			}

			DebugTrace2( 0, Dbg, ("create ccb->Lcb->ExactCaseLink.LinkName = %Z, fileFullea->EaName = %ws\n", 
								   &fcb->FullFileName, &fileFullEa->EaName[0]) );
			
			createContext.EaLength += sizeof(FILE_FULL_EA_INFORMATION) - sizeof(CHAR) + fileFullEa->EaNameLength + fileFullEa->EaValueLength;

			DebugTrace2( 0, Dbg, ("RedirectIrp, IRP_MJ_CREATE: Ea is set irpSp->Parameters.Create.EaLength = %d, createContext.EaLength = %d\n", 
								 irpSp->Parameters.Create.EaLength, createContext.EaLength) );		
		} 

		NDAS_ASSERT( createContext.EaLength <= irpSp->Parameters.Create.EaLength );

		if (ndfsWinxpReplytHeaderInfo) {

			createContext.AllocationSize = 0;
		
		} else {

			createContext.AllocationSize = Irp->Overlay.AllocationSize.QuadPart;
		}

		if (!FlagOn(irpSp->Flags, SL_CASE_SENSITIVE)) {
			
			createContext.FileNameLength = fullPathName.Length;
			createContext.FileNameLength += (originalFileName.Length - fileObjectName->Length);
			createContext.RelatedFileHandle = 0; 
			
		} else {

			if (fileObject->RelatedFileObject) {
				
				PCCB	relatedCcb = fileObject->RelatedFileObject->FsContext2;


				if (FlagOn(relatedCcb->NdasFatFlags, ND_FAT_CCB_FLAG_UNOPENED)) {

					try_return( status = STATUS_OBJECT_PATH_NOT_FOUND );
				}
				createContext.RelatedFileHandle = relatedCcb->PrimaryFileHandle; 
			}

			createContext.FileNameLength = originalFileName.Length;
		}
		
		if (volDo->Secondary->Thread.SessionContext.PrimaryMaxDataSize < createContext.EaLength + fullPathName.Length + (originalFileName.Length - fileObjectName->Length)) {
			
			NDAS_ASSERT(FALSE);
			try_return( status = STATUS_INVALID_PARAMETER );
		}

		ASSERT( secondarySessionResourceAcquired == FALSE );
		
		secondarySessionResourceAcquired 
			= SecondaryAcquireResourceExclusiveLite( IrpContext, 
													 &volDo->SessionResource, 
													 BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

		if (FlagOn(volDo->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

			PrintIrp( Dbg2, "SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED", NULL, IrpContext->OriginatingIrp );
			NDAS_ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
			SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_DONT_POST_REQUEST );
			FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );	
		}

		secondaryRequest = AllocateWinxpSecondaryRequest( volDo->Secondary,
														  IRP_MJ_CREATE,
														  createContext.EaLength + fullPathName.Length + (originalFileName.Length - fileObjectName->Length) );

		if (secondaryRequest == NULL) {

			FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
		}

		ndfsRequestHeader = &secondaryRequest->NdfsRequestHeader;
		
		INITIALIZE_NDFS_REQUEST_HEADER( ndfsRequestHeader,
										NDFS_COMMAND_EXECUTE,
										volDo->Secondary,
										IRP_MJ_CREATE,
										createContext.EaLength + fullPathName.Length + (originalFileName.Length - fileObjectName->Length) );

		ndfsWinxpRequestHeader = (PNDFS_WINXP_REQUEST_HEADER)(ndfsRequestHeader+1);
		ASSERT( ndfsWinxpRequestHeader == (PNDFS_WINXP_REQUEST_HEADER)secondaryRequest->NdfsRequestData );
		INITIALIZE_NDFS_WINXP_REQUEST_HEADER( ndfsWinxpRequestHeader, Irp, irpSp, 0 );

		RtlCopyMemory( &ndfsWinxpRequestHeader->Create, &createContext, sizeof(WINXP_REQUEST_CREATE) );

		ndfsWinxpRequestData = (UINT8 *)(ndfsWinxpRequestHeader+1);

		if (createContext.EaLength) {

			// It have structure align Problem. If you wanna release, Do more
			PFILE_FULL_EA_INFORMATION eaBuffer = Irp->AssociatedIrp.SystemBuffer;
			
			RtlCopyMemory( ndfsWinxpRequestData, eaBuffer, createContext.EaLength );
		}

		if (!FlagOn(irpSp->Flags, SL_CASE_SENSITIVE)) {

			RtlCopyMemory( ndfsWinxpRequestData + createContext.EaLength, 
						   fullPathName.Buffer, 
						   fullPathName.Length );
		
			RtlCopyMemory( ndfsWinxpRequestData + fullPathName.Length, 
						   ((PCHAR)(originalFileName.Buffer)) + fileObjectName->Length,
						   (originalFileName.Length - fileObjectName->Length) );

		} else {

			NDAS_ASSERT(FALSE);

			RtlCopyMemory( ndfsWinxpRequestData + createContext.EaLength, 
						   originalFileName.Buffer, 
						   originalFileName.Length );
		}

		secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
		QueueingSecondaryRequest( volDo->Secondary, secondaryRequest );

		timeOut.QuadPart = -NDASFAT_TIME_OUT;
		status = KeWaitForSingleObject( &secondaryRequest->CompleteEvent, Executive, KernelMode, FALSE, &timeOut );
		
		KeClearEvent( &secondaryRequest->CompleteEvent );
			
		if (status != STATUS_SUCCESS) {

			NDAS_ASSERT(FALSE);
			try_return( status );
		}

		if (secondaryRequest->ExecuteStatus != STATUS_SUCCESS) {

			if (IrpContext->OriginatingIrp) {

				PrintIrp( Dbg2, "secondaryRequest->ExecuteStatus != STATUS_SUCCESS", NULL, IrpContext->OriginatingIrp );
			}

			DebugTrace2( 0, Dbg2, ("secondaryRequest->ExecuteStatus != STATUS_SUCCESS file = %s, line = %d\n", __FILE__, __LINE__) );

			NDAS_ASSERT( FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
			SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_DONT_POST_REQUEST );
			FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
		}

		ndfsWinxpReplytHeader = (PNDFS_WINXP_REPLY_HEADER)secondaryRequest->NdfsReplyData;

		status = NTOHL(ndfsWinxpReplytHeader->Status4);
		Irp->IoStatus.Status = status;
		Irp->IoStatus.Information = NTOHL(ndfsWinxpReplytHeader->Information32);

		DebugTrace2( 0, Dbg, ("Second Phase: NTOHL(ndfsWinxpReplytHeader->Status4) = %x, ndfsWinxpReplytHeader->Open.FileHandle = %x\n", 
							   NTOHL(ndfsWinxpReplytHeader->Status4), ndfsWinxpReplytHeader->Open.FileHandle) );

		if (NTOHL(ndfsWinxpReplytHeader->Status4) != STATUS_SUCCESS) {

			if (Irp->IoStatus.Status == STATUS_ACCESS_DENIED) {

				DebugTrace2( 0, Dbg2, ("Irp->IoStatus.Status == STATUS_ACCESS_DENIED DesiredAccess = %x, NTOHL(ndfsWinxpReplytHeader->Status4) = %x\n", 
									irpSp->Parameters.Create.SecurityContext->DesiredAccess, NTOHL(ndfsWinxpReplytHeader->Status4)) );
			}

			try_return(status);
		}

		if (secondarySessionResourceAcquired) {

			SecondaryReleaseResourceLite( IrpContext, &volDo->SessionResource );		
			secondarySessionResourceAcquired = FALSE;
		}

		if (fullPathName.Length > 2) {

			NDAS_ASSERT( fullPathName.Buffer[fullPathName.Length/sizeof(WCHAR)-1] != L'\\' );
		}

		if (ndfsWinxpReplytHeader->Open.TypeOfOpen == UserDirectoryOpen && fullPathName.Length != 2) {

			if ((status = RtlAppendUnicodeToString(&fullPathName, L"\\")) != STATUS_SUCCESS) {

				FatRaiseStatus( IrpContext, status );
			}
		}

		if (fcb) {

			NDAS_ASSERT( (irpSp->Parameters.Create.Options & 0xFF000000) != (FILE_CREATE << 24) );			
			NDAS_ASSERT( fcb->Handle == ndfsWinxpReplytHeader->Open.FcbHandle );
			NDAS_ASSERT( fcb->CreationTime.QuadPart == ndfsWinxpReplytHeader->Open.CreationTime );

		} else {

			SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_SECONDARY_CREATE_FILE );

			fcb = FatCreateFcb ( IrpContext,
							     &volDo->Vcb,
								 NULL,
								 0,
								 0,
								 NULL,
								 &fullPathName,
								 BooleanFlagOn(irpSp->Flags, SL_OPEN_PAGING_FILE),
								 FALSE );
		
			if (fcb == NULL) {

				FatRaiseStatus( IrpContext, STATUS_INSUFFICIENT_RESOURCES );
			}

			ClearFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_SECONDARY_CREATE_FILE );

			InitializeListHead( &fcb->ListEntry );

			RtlInitEmptyUnicodeString( &fcb->FullFileName,
									   fcb->FullFileNameBuffer,
									   sizeof(fcb->FullFileNameBuffer) );

			RtlCopyUnicodeString( &fcb->FullFileName, &fullPathName );

			RtlInitEmptyUnicodeString( &fcb->CaseInSensitiveFullFileName,
									   fcb->CaseInSensitiveFullFileNameBuffer,
									   sizeof(fcb->CaseInSensitiveFullFileNameBuffer) );

			RtlDowncaseUnicodeString( &fcb->CaseInSensitiveFullFileName, &fcb->FullFileName, FALSE );

			Secondary_Reference( volDo->Secondary );
	
			SetFlag( fcb->NdasFatFlags, ND_FAT_FCB_FLAG_SECONDARY );
		
			fcb->CreationTime.QuadPart = ndfsWinxpReplytHeader->Open.CreationTime;
			fcb->Handle = ndfsWinxpReplytHeader->Open.FcbHandle;
		
			switch (ndfsWinxpReplytHeader->Open.TypeOfOpen) {

			case UserVolumeOpen:
				
				fcb->Header.NodeTypeCode = NDFAT_NTC_VCB;
				break;

			case UserDirectoryOpen:
				
				if (RtlEqualUnicodeString(&FatData.Root, &fileObject->FileName, TRUE)) {
					
					fcb->Header.NodeTypeCode = FAT_NTC_ROOT_DCB;
				
				} else {

					fcb->Header.NodeTypeCode = FAT_NTC_DCB;
				}

				break;

			case UserFileOpen:
				
				fcb->Header.NodeTypeCode = FAT_NTC_FCB;
				break;

			case UnopenedFileObject:

				if (!FlagOn(createContext.Options, FILE_OPEN_REPARSE_POINT)) {

					NDAS_ASSERT(FALSE);
				}

				fcb->Header.NodeTypeCode = FAT_NTC_DCB;
				break;

			default:
				
				NDAS_ASSERT(FALSE);
				fcb->Header.NodeTypeCode = FAT_NTC_DCB;
			}

			ExAcquireFastMutex( &volDo->Secondary->FcbQMutex );
			InsertHeadList(	&volDo->Secondary->FcbQueue, &fcb->ListEntry );
			ExReleaseFastMutex( &volDo->Secondary->FcbQMutex );

			FatAcquireExclusiveFcb( IrpContext, fcb );
			ExAcquireSharedStarveExclusive( fcb->Header.PagingIoResource, TRUE );
			acquiredFcbWithPaging = TRUE;

#if 0
			fileReference.SegmentNumberHighPart = ndfsWinxpReplytHeader->Open.FileReference.SegmentNumberHighPart;
			fileReference.SegmentNumberLowPart	= ndfsWinxpReplytHeader->Open.FileReference.SegmentNumberLowPart;
			fileReference.SequenceNumber		= ndfsWinxpReplytHeader->Open.FileReference.SequenceNumber;  

			NtfsAcquireFcbTable( IrpContext, &volDo->Vcb );
			acquiredFcbTable = TRUE;

			returnedExistingFcb = TRUE;
			SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_SECONDARY_CREATE_FILE );
			fcb = NtfsCreateFcb ( IrpContext, &volDo->Vcb, fileReference, BooleanFlagOn(irpSp->Flags, SL_OPEN_PAGING_FILE), FALSE, &returnedExistingFcb );
			ClearFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_SECONDARY_CREATE_FILE );

			fcb->ReferenceCount += 1;
			NtfsReleaseFcbTable( IrpContext, &volDo->Vcb );
			acquiredFcbTable = FALSE;

			SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_PAGING );
			NtfsAcquireFcbWithPaging( IrpContext, fcb, 0 );
			acquiredFcbWithPaging = TRUE;

			NtfsAcquireFcbTable( IrpContext, &volDo->Vcb );
			fcb->ReferenceCount -= 1;
			NtfsReleaseFcbTable( IrpContext, &volDo->Vcb );
#endif
		}


#if 0
		fcb->Info.AllocatedLength		= ndfsWinxpReplytHeader->DuplicatedInformation.AllocatedLength;
		fcb->Info.CreationTime			= ndfsWinxpReplytHeader->DuplicatedInformation.CreationTime;
		fcb->Info.FileAttributes		= ndfsWinxpReplytHeader->DuplicatedInformation.FileAttributes;
		fcb->Info.FileSize				= ndfsWinxpReplytHeader->DuplicatedInformation.FileSize;
		fcb->Info.LastAccessTime		= ndfsWinxpReplytHeader->DuplicatedInformation.LastAccessTime;
		fcb->Info.LastChangeTime		= ndfsWinxpReplytHeader->DuplicatedInformation.LastChangeTime;
		fcb->Info.LastModificationTime	= ndfsWinxpReplytHeader->DuplicatedInformation.LastModificationTime;
		fcb->Info. ReparsePointTag		= ndfsWinxpReplytHeader->DuplicatedInformation.ReparsePointTag;

		if (!IsDirectory(&fcb->Info)) 
			ASSERT( ndfsWinxpReplytHeader->Open.TypeOfOpen == UserFileOpen		|| 
					ndfsWinxpReplytHeader->Open.TypeOfOpen == UserVolumeOpen	||
					ndfsWinxpReplytHeader->Open.TypeOfOpen == UserViewIndexOpen );
#endif

		if (createContext.RelatedFileHandle) {

			ccbBufferLength = originalFileName.Length;
		
		} else {

			ccbBufferLength = (originalFileName.Length - fileObjectName->Length);
		}

		ccbFlags = 0;
		
		if (FlagOn(irpSp->Parameters.Create.Options, FILE_DELETE_ON_CLOSE)) {

			SetFlag( ccbFlags, CCB_FLAG_DELETE_ON_CLOSE );
		}

		if (ndfsWinxpReplytHeader->Open.TypeOfOpen == UserFileOpen) {

		//	ASSERT( FlagOn((ndfsWinxpReplytHeader->Open.CcbFlags & ~CCB_FLAG_IGNORE_CASE), CCB_FLAG_DELETE_ON_CLOSE | CCB_FLAG_OPEN_AS_FILE) 
		//			== FlagOn(ccbFlags, CCB_FLAG_DELETE_ON_CLOSE | CCB_FLAG_OPEN_AS_FILE) );
		}

		//ccb = NtfsCreateCcb ( IrpContext,
		//					  fcb,
		//					  scb,
		//					  (scb->AttributeTypeCode == $INDEX_ALLOCATION),
		//					  0,	//  EaModificationCount,
		//					  ccbFlags,
		//					  fileObject,
		//					  0	/*	LastFileNameOffset */ );

		ccb = FatCreateCcb( IrpContext );

		if (ndfsWinxpReplytHeader->Open.TypeOfOpen == UserFileOpen) {

			ExAcquireFastMutex( &fcb->NonPaged->CcbQMutex );
		
			ccb->FileObject = fileObject;
			ccb->Fcb = fcb;
			InsertTailList( &fcb->NonPaged->CcbQueue, &ccb->FcbListEntry );
		
			ExReleaseFastMutex( &fcb->NonPaged->CcbQMutex );
		}

		if (ccbBufferLength) {

			ccb->Buffer = FsRtlAllocatePoolWithTag( PagedPool, ccbBufferLength, 'CftN' );
		
		} else {

			ccb->Buffer = NULL;
		}

		ccb->BufferLength = ccbBufferLength;

		///////////////////////////
		//ccb->LfsMark    = NDFAT_MARK;
		InitializeListHead( &ccb->ListEntry );
		ccb->Fcb = fcb;
		///////////////////////////


		ccb->FileObject	= fileObject;

		InitializeListHead( &ccb->ListEntry );

		ExAcquireFastMutex( &volDo->Secondary->FastMutex );
		ccb->SessionId = volDo->Secondary->SessionId;
		ExReleaseFastMutex( &volDo->Secondary->FastMutex );

		ASSERT( ndfsWinxpReplytHeader->Open.FileHandle != 0) ;
		ccb->PrimaryFileHandle = ndfsWinxpReplytHeader->Open.FileHandle;

		ccb->IrpFlags = Irp->Flags;
		ccb->IrpSpFlags = irpSp->Flags;

		RtlCopyMemory( &ccb->CreateContext, &createContext, sizeof(WINXP_REQUEST_CREATE) );

		ccb->CreateContext.EaLength = 0;
		ccb->CreateContext.AllocationSize = 0;

		if (createContext.EaLength) {

			PFILE_FULL_EA_INFORMATION eaBuffer = Irp->AssociatedIrp.SystemBuffer;

			RtlCopyMemory( ccb->Buffer, eaBuffer, createContext.EaLength );
		}

		if (ccb->CreateContext.RelatedFileHandle) {

			PCCB relatedCcb2 = fileObject->RelatedFileObject->FsContext2;

			NDAS_ASSERT( ccb->CreateContext.RelatedFileHandle == relatedCcb2->PrimaryFileHandle );

			RtlCopyMemory( ccb->Buffer + createContext.EaLength, originalFileName.Buffer, originalFileName.Length );
		
		} else {

			RtlCopyMemory( ccb->Buffer + createContext.EaLength, 
						   ((PCHAR)(originalFileName.Buffer)) + fileObjectName->Length,
						   (originalFileName.Length - fileObjectName->Length) );
		}

		ExAcquireFastMutex( &volDo->Secondary->RecoveryCcbQMutex );
		InsertHeadList( &volDo->Secondary->RecoveryCcbQueue, &ccb->ListEntry );
		ExReleaseFastMutex( &volDo->Secondary->RecoveryCcbQMutex );

		if (ndfsWinxpReplytHeader->Open.TypeOfOpen == UserFileOpen) {

			if (FlagOn(fileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING)) {

				fcb->NonCachedUncleanCount += 1;
		
			} else {

				fileObject->Flags |= FO_CACHE_SUPPORTED;
			}
		}

		if (FlagOn(fileObject->Flags, FO_NO_INTERMEDIATE_BUFFERING)) {

			if ((fcb->UncleanCount == fcb->NonCachedUncleanCount) &&
			    (fcb->NonPaged->SectionObjectPointers.DataSectionObject != NULL) &&
				 FlagOn( fcb->FcbState, FCB_STATE_PAGING_FILE )) {

				SetFlag( fcb->Vcb->VcbState, VCB_STATE_FLAG_CREATE_IN_PROGRESS );
				CcFlushCache( &fcb->NonPaged->SectionObjectPointers, NULL, 0, NULL );
				ExAcquireResourceExclusiveLite( fcb->Header.PagingIoResource, TRUE );
				ExReleaseResourceLite( fcb->Header.PagingIoResource );
				CcPurgeCacheSection( &fcb->NonPaged->SectionObjectPointers,	NULL, 0, FALSE );
				ClearFlag( fcb->Vcb->VcbState, VCB_STATE_FLAG_CREATE_IN_PROGRESS );
			}
		}

		InterlockedIncrement( &volDo->Vcb.SecondaryOpenFileCount );

		InterlockedIncrement( &fcb->OpenCount );
		InterlockedIncrement( &fcb->UncleanCount );

		//if (createContext.SecurityContext.FullCreateOptions & FILE_DELETE_ON_CLOSE)
		//	fcb->DeletePending = TRUE;

		fileObject->FsContext = fcb;
		fileObject->FsContext2 = ccb;

		fileObject->Vpb = volDo->Secondary->VolDo->Vcb.Vpb;

		NDAS_ASSERT( NTOHLL(ndfsWinxpReplytHeader->FileSize8) <= NTOHLL(ndfsWinxpReplytHeader->AllocationSize8) );

		if (ndfsWinxpReplytHeader->Open.TypeOfOpen == UserFileOpen) {

			PNDFS_FAT_MCB_ENTRY	mcbEntry;
			ULONG				index;

			BOOLEAN			lookupResut;
			VBO				vcn;
			LBO				lcn;
			//LCN			startingLcn;
			ULONG			clusterCount;

			if (!FlagOn(volDo->NdasFatFlags, ND_FAT_DEVICE_FLAG_DIRECT_RW)) {

				fileObject->SectionObjectPointer = &fcb->NonPaged->SectionObjectPointers;
				
				NDAS_ASSERT( fcb->Header.FileSize.LowPart == 0 );
				goto next_step;
			}			

			if (Irp->Overlay.AllocationSize.QuadPart) {

				if (ndfsWinxpReplytHeaderInfo) {

					ndfsWinxpReplytHeader = ndfsWinxpReplytHeaderInfo;
				}

				NDAS_ASSERT( Irp->Overlay.AllocationSize.QuadPart <= (INT64)NTOHLL(ndfsWinxpReplytHeader->AllocationSize8) );
			}

			if (NTOHLL(ndfsWinxpReplytHeader->AllocationSize8)) {

				NDAS_ASSERT( ndfsWinxpReplytHeader->FileInformationSet );
			}

			if (fcb->OpenCount != 1 + ((ndfsWinxpReplytHeaderInfo) ? 1 : 0)) {

				if (NTOHLL(ndfsWinxpReplytHeader->AllocationSize8) != fcb->Header.AllocationSize.QuadPart) {
				
					NDAS_ASSERT( createDisposition == FILE_SUPERSEDE ||
								 createDisposition == FILE_OVERWRITE || 
								 createDisposition == FILE_OVERWRITE_IF );
					
					if (NTOHLL(ndfsWinxpReplytHeader->AllocationSize8)) {

						NDAS_ASSERT( Irp->Overlay.AllocationSize.QuadPart );
					}
				}
			}

			fileObject->SectionObjectPointer = &fcb->NonPaged->SectionObjectPointers;

			if (createDisposition == FILE_SUPERSEDE || createDisposition == FILE_OVERWRITE || createDisposition == FILE_OVERWRITE_IF) {

				if (fcb->Header.ValidDataLength.QuadPart != 0) {

					CcFlushCache( &fcb->NonPaged->SectionObjectPointers, NULL, 0, NULL );
					CcPurgeCacheSection( &fcb->NonPaged->SectionObjectPointers, NULL, 0, FALSE );
				}

				fcb->Header.FileSize.LowPart = 0;
				fcb->Header.ValidDataLength.QuadPart = 0;
				fcb->ValidDataToDisk = 0;

#if __ND_NTFS_DBG__
				strncpy(scb->LastUpdateFile, strrchr(__FILE__,'\\')+1, 15); 
				scb->LastUpdateLine = __LINE__; 
#endif				
				
				if (NTOHLL(ndfsWinxpReplytHeader->AllocationSize8)) {

					mcbEntry = (PNDFS_FAT_MCB_ENTRY)( ndfsWinxpReplytHeader+1 );

					for (index=0, vcn=0; index < NTOHL(ndfsWinxpReplytHeader->NumberOfMcbEntry4); index++, mcbEntry++) {

						NDAS_ASSERT( mcbEntry->Vcn == vcn );

						lookupResut = FatLookupMcbEntry( &volDo->Vcb, &fcb->Mcb, vcn, &lcn, &clusterCount, NULL );
					
						if (lookupResut == TRUE && vcn < fcb->Header.AllocationSize.QuadPart) {

							ASSERT( lookupResut == TRUE );

							if (vcn < NTOHLL(ndfsWinxpReplytHeader->AllocationSize8)) {
							
								//ASSERT( startingLcn == lcn );
								ASSERT( vcn == mcbEntry->Vcn );
								ASSERT( lcn == (((LBO)mcbEntry->Lcn) << volDo->Vcb.AllocationSupport.LogOfBytesPerSector) );
								ASSERT( clusterCount <= mcbEntry->ClusterCount || 
										vcn+mcbEntry->ClusterCount == NTOHLL(ndfsWinxpReplytHeader->AllocationSize8) );
							
								if (clusterCount < mcbEntry->ClusterCount) {

									FatAddMcbEntry ( &volDo->Vcb, 
													 &fcb->Mcb, 
													 (VBO)mcbEntry->Vcn, 
													 ((LBO)mcbEntry->Lcn) << volDo->Vcb.AllocationSupport.LogOfBytesPerSector, 
													 (ULONG)mcbEntry->ClusterCount );
	
									lookupResut = FatLookupMcbEntry( &volDo->Vcb, &fcb->Mcb, vcn, &lcn, &clusterCount, NULL );

									ASSERT( lookupResut == TRUE );
									//ASSERT( startingLcn == lcn );
									ASSERT( vcn == mcbEntry->Vcn );
									ASSERT( lcn == (((LBO)mcbEntry->Lcn) << volDo->Vcb.AllocationSupport.LogOfBytesPerSector) );
									ASSERT( clusterCount == mcbEntry->ClusterCount );
								}
							}
					
						} else { 

							ASSERT( lookupResut == FALSE || lcn == 0 );
							
							FatAddMcbEntry ( &volDo->Vcb, 
											 &fcb->Mcb, 
											 (VBO)mcbEntry->Vcn, 
											 ((LBO)mcbEntry->Lcn) << volDo->Vcb.AllocationSupport.LogOfBytesPerSector, 
											 (ULONG)mcbEntry->ClusterCount );
						}

						vcn += (ULONG)mcbEntry->ClusterCount;
					}
			
					NDAS_ASSERT( vcn == 0 && index != 0 || vcn == NTOHLL(ndfsWinxpReplytHeader->AllocationSize8) );
				}

				if (fcb->Header.AllocationSize.QuadPart < (INT64)NTOHLL(ndfsWinxpReplytHeader->AllocationSize8)) {

					ASSERT( ndfsWinxpReplytHeader->Open.TruncateOnClose );

					fcb->Header.AllocationSize.QuadPart = NTOHLL(ndfsWinxpReplytHeader->AllocationSize8);

					if (fileObject->SectionObjectPointer->DataSectionObject != NULL && fileObject->PrivateCacheMap == NULL) {

						CcInitializeCacheMap( fileObject,
											  (PCC_FILE_SIZES)&fcb->Header.AllocationSize,
											  TRUE,
											  &FatData.CacheManagerCallbacks,
											  fcb );

						//CcSetAdditionalCacheAttributes( fileObject, TRUE, TRUE );
					}

					CcSetFileSizes( fileObject, (PCC_FILE_SIZES)&fcb->Header.AllocationSize );

					SetFlag( fcb->FcbState, FCB_STATE_TRUNCATE_ON_CLOSE );
				
				} else if (fcb->Header.AllocationSize.QuadPart > (INT64)NTOHLL(ndfsWinxpReplytHeader->AllocationSize8)) {

					VBO				vbo;
					LBO				lbo;
					ULONG			byteCount;

					fcb->Header.AllocationSize.QuadPart = NTOHLL(ndfsWinxpReplytHeader->AllocationSize8);
					
					vbo = fcb->Header.AllocationSize.LowPart;

					while (FatLookupMcbEntry(&volDo->Vcb, &fcb->Mcb, vbo, &lbo, &byteCount, NULL)) {

						FatRemoveMcbEntry( &volDo->Vcb, &fcb->Mcb, vbo, byteCount );

						vbo += byteCount;

						if (vbo == 0) {

							break;
						}
					}

					//FatRemoveMcbEntry( &volDo->Vcb, &fcb->Mcb, fcb->Header.AllocationSize.LowPart, 0xFFFFFFFF );

					CcSetFileSizes( fileObject, (PCC_FILE_SIZES)&fcb->Header.AllocationSize );
				}

			} else {
			
				if (fcb->Header.AllocationSize.QuadPart != 0) {

					//if (FlagOn(fcb->FcbState, FCB_STATE_TRUNCATE_ON_CLOSE))
					//	ASSERT( FlagOn(fcb->FcbState, FCB_STATE_TRUNCATE_ON_CLOSE) == ndfsWinxpReplytHeader->Open.TruncateOnClse );

					ASSERT( fcb->Header.FileSize.LowPart == NTOHLL(ndfsWinxpReplytHeader->FileSize8) );
					ASSERT( fcb->Header.AllocationSize.QuadPart == NTOHLL(ndfsWinxpReplytHeader->AllocationSize8) );
			
				} else {

					if (NTOHLL(ndfsWinxpReplytHeader->AllocationSize8)) {

						ASSERT( fcb->OpenCount == 1 + ((ndfsWinxpReplytHeaderInfo) ? 1 : 0) );

						mcbEntry = (PNDFS_FAT_MCB_ENTRY)( ndfsWinxpReplytHeader+1 );

						for (index=0, vcn=0; index < NTOHL(ndfsWinxpReplytHeader->NumberOfMcbEntry4); index++, mcbEntry++) {

							ASSERT( mcbEntry->Vcn == vcn );

							FatAddMcbEntry ( &volDo->Vcb, 
											 &fcb->Mcb, 
											 (VBO)mcbEntry->Vcn, 
											 ((LBO)mcbEntry->Lcn) << volDo->Vcb.AllocationSupport.LogOfBytesPerSector, 
											 (ULONG)mcbEntry->ClusterCount );

							vcn += (ULONG)mcbEntry->ClusterCount;
						}
			
						ASSERT( vcn == NTOHLL(ndfsWinxpReplytHeader->AllocationSize8) );

						ASSERT( ndfsWinxpReplytHeader->FileSize8 == ndfsWinxpReplytHeader->ValidDataLength8 );

						fcb->Header.FileSize.QuadPart = NTOHLL(ndfsWinxpReplytHeader->FileSize8);
						fcb->Header.AllocationSize.QuadPart = NTOHLL(ndfsWinxpReplytHeader->AllocationSize8);
						fcb->Header.ValidDataLength.QuadPart = NTOHLL(ndfsWinxpReplytHeader->ValidDataLength8);
						fcb->ValidDataToDisk = (UINT32)NTOHLL(ndfsWinxpReplytHeader->ValidDataLength8);

						//DbgPrint( "create return fcb->Header.FileSize.LowPart = %x\n", fcb->Header.FileSize.LowPart );

#if __ND_NTFS_DBG__
						strncpy(scb->LastUpdateFile, strrchr(__FILE__,'\\')+1, 15); 
						scb->LastUpdateLine = __LINE__; 
#endif

						if (fcb->Header.FileSize.LowPart == 0) {
							
							NDAS_ASSERT( ndfsWinxpReplytHeader->Open.TruncateOnClose );
							SetFlag( fcb->FcbState, FCB_STATE_TRUNCATE_ON_CLOSE );
						
						} else {

							NDAS_ASSERT( ndfsWinxpReplytHeader->Open.TruncateOnClose == FALSE );
						}

						if (fileObject->SectionObjectPointer->DataSectionObject != NULL && fileObject->PrivateCacheMap == NULL) {

							CcInitializeCacheMap( fileObject,
												  (PCC_FILE_SIZES)&fcb->Header.AllocationSize,
												  TRUE,
												  &FatData.CacheManagerCallbacks,
												  fcb );

							//CcSetAdditionalCacheAttributes( fileObject, TRUE, TRUE );
						}

						CcSetFileSizes( fileObject, (PCC_FILE_SIZES)&fcb->Header.AllocationSize );
					}
				}
			}

#if DBG
			{
				BOOLEAN	lookupResut;
				VBO		vcn;
				LBO		lcn;
				//LCN	startingLcn;
				ULONG	clusterCount;

				vcn = 0;

				while (1) {

					lookupResut = FatLookupMcbEntry( &volDo->Vcb, &fcb->Mcb, vcn, &lcn, &clusterCount, NULL );

					if (lookupResut == FALSE || lcn == 0) {

						break;
					}

					vcn += clusterCount;

					if (vcn == 0) {

						break;
					}
				}

				ASSERT( vcn == fcb->Header.AllocationSize.QuadPart );
			}

#endif
			NDAS_ASSERT( fcb->Header.FileSize.LowPart <= fcb->Header.AllocationSize.QuadPart );

next_step: NOTHING;

		}

try_exit:  NOTHING;

	} finally {

		if (AbnormalTermination()) {

			status = IrpContext->ExceptionStatus;
		}

		if (acquiredFcbWithPaging == TRUE) {

			ExReleaseResourceLite( fcb->Header.PagingIoResource );
			FatReleaseFcb( IrpContext, fcb );
		}

		if (fullPathNameBuffer) {

			ExFreePool( fullPathNameBuffer );
		}

		if (secondaryRequest) {

			DereferenceSecondaryRequest( secondaryRequest );
		}

		if (secondaryRequestInfo) {

			DereferenceSecondaryRequest( secondaryRequestInfo );
		}


		if (openFileHandle) {

			if (secondarySessionResourceAcquired == FALSE) {

				secondarySessionResourceAcquired 
					= SecondaryAcquireResourceExclusiveLite( IrpContext, 
															 &volDo->SessionResource, 
															 BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
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

			fileObject->FileName = originalFileName;
		}

		if (vcbAcquired) {

			FatReleaseVcb( IrpContext, IrpContext->Vcb );
		}

		if (!NT_SUCCESS(status)) {

			ClearFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_SECONDARY_CONTEXT );
		}
	}

	FatCompleteRequest( IrpContext, Irp, status );

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

		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
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
	
	//status = KeWaitForSingleObject( &Secondary->Semaphore, Executive, KernelMode, FALSE, NULL );
	//ASSERT( status == STATUS_SUCCESS );
	//ASSERT( Secondary->Semaphore.Header.SignalState <= Secondary->Thread.SessionContext.RequestsPerSession );

	secondaryRequest->RequestType = SECONDARY_REQ_SEND_MESSAGE;
	
	QueueingSecondaryRequest( Secondary, secondaryRequest );
				
	timeOut.QuadPart = -NDASFAT_TIME_OUT;
	
	status = KeWaitForSingleObject(	&secondaryRequest->CompleteEvent,
									Executive,
									KernelMode,
									FALSE,
									&timeOut );

	KeClearEvent( &secondaryRequest->CompleteEvent );

	if (status != STATUS_SUCCESS) {
	
		ASSERT(NDASFAT_BUG);

		secondaryRequest = NULL;
		status = STATUS_TIMEOUT;	

		//KeReleaseSemaphore(&Secondary->Semaphore, 0, 1, FALSE);
			
		return;
	}

	status = secondaryRequest->ExecuteStatus;	
	DereferenceSecondaryRequest( secondaryRequest );
	secondaryRequest = NULL;

	//KeReleaseSemaphore( &Secondary->Semaphore, 0, 1, FALSE );

	return;
}


NTSTATUS
Secondary_MakeFullPathName (
	IN  PFILE_OBJECT	FileObject,
	IN  PUNICODE_STRING	FileObjectName,
	OUT PUNICODE_STRING	FullPathName
	)
{
	NTSTATUS	status;

	if (FileObjectName->Length > FullPathName->MaximumLength) {

		NDAS_ASSERT(FALSE);
		return STATUS_NAME_TOO_LONG;
	}

	if (FileObject->RelatedFileObject) {
	
		if (IS_SECONDARY_FILEOBJECT(FileObject->RelatedFileObject)) {

			PFCB relatedFcb = FileObject->RelatedFileObject->FsContext;

			if ((relatedFcb->FullFileName.Length + sizeof(WCHAR) + FileObjectName->Length) > FullPathName->MaximumLength) {

				NDAS_ASSERT(FALSE);
				return STATUS_NAME_TOO_LONG;
			}

			if ((status = RtlAppendUnicodeStringToString(FullPathName, &relatedFcb->FullFileName)) != STATUS_SUCCESS) {
		
				NDAS_ASSERT(FALSE);
				return status;
			}


#if 0
			PCCB relatedCcb = FileObject->RelatedFileObject->FsContext2;
		
			if ((relatedCcb->Lcb->ExactCaseLink.LinkName.Length + sizeof(WCHAR) + FileObjectName->Length) > NDFS_MAX_PATH*sizeof(WCHAR)) {

				ASSERT( FALSE );
				return STATUS_NAME_TOO_LONG;
			}

			if ((status = RtlAppendUnicodeStringToString(FullPathName, &relatedCcb->Lcb->ExactCaseLink.LinkName)) != STATUS_SUCCESS) {
		
				ASSERT( FALSE );
				return status;
			}
#endif

		} else {

			NDAS_ASSERT(FALSE);

#if 0

			PUNICODE_STRING	relatedFileObjectName;
			PSCB			relatedFileObjectScb;

			relatedFileObjectName = &FileObject->RelatedFileObject->FileName;
            relatedFileObjectScb = FileObject->RelatedFileObject->FsContext;

			if ((relatedFileObjectName->Length + sizeof(WCHAR) + FileObjectName->Length) > NDFS_MAX_PATH*sizeof(WCHAR)) {

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
#endif
		}
	}

	if (FileObjectName->Length == 0) {

		return STATUS_SUCCESS;
	}

	if ((status = RtlAppendUnicodeStringToString(FullPathName, FileObjectName)) != STATUS_SUCCESS) {

		NDAS_ASSERT(FALSE);
		return status;
	}

	if (FullPathName->Length != 2)
		ASSERT( FullPathName->Buffer[FullPathName->Length/sizeof(WCHAR)-1] != L'\\' );

	return STATUS_SUCCESS;
}

#endif
