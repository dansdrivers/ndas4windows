#include "LfsProc.h"


VOID
PrimarySession_FreeOpenFile (
	IN	PPRIMARY_SESSION PrimarySession,
	IN  POPEN_FILE		 OpenedFile
	);

HANDLE
PrimarySessionOpenFile (
	IN  PPRIMARY_SESSION	PrimarySession,
	IN	POPEN_FILE			OpenFile
	);

POPEN_FILE
PrimarySession_FindOpenFile (
	IN  PPRIMARY_SESSION PrimarySession,
	IN	UINT64			 OpenFileId
	);

POPEN_FILE
PrimarySession_FindOpenFileCleanUpedAndNotClosed (
	IN  PPRIMARY_SESSION	PrimarySession,
	IN	PUNICODE_STRING		FullFileName,
	IN  BOOLEAN				DeleteOnClose
	);

POPEN_FILE
PrimarySession_FindOrReopenOpenFile (
	IN  PPRIMARY_SESSION PrimarySession,
	IN	UINT64			 OpenFileId
	);

ULONG
CalculateSafeLength (
	FILE_INFORMATION_CLASS	fileInformationClass,
	ULONG					requestLength,
	ULONG					returnedLength,
	PCHAR					Buffer
	);

//
//	ntifs.h of Windows XP does not include ZwSetVolumeInformationFile nor NtSetVolumeInformationFile.
//

NTSYSAPI
NTSTATUS
NTAPI
ZwSetVolumeInformationFile (
    IN HANDLE FileHandle,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    IN PVOID FsInformation,
    IN ULONG Length,
    IN FS_INFORMATION_CLASS FsInformationClass
    );


NTSTATUS
NtQueryEaFile (
    IN HANDLE FileHandle,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    OUT PVOID Buffer,
    IN ULONG Length,
    IN BOOLEAN ReturnSingleEntry,
    IN PVOID EaList OPTIONAL,
    IN ULONG EaListLength,
    IN PULONG EaIndex OPTIONAL,
    IN BOOLEAN RestartScan
    );


NTSTATUS
NtSetEaFile (
    IN HANDLE FileHandle,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    IN PVOID Buffer,
    IN ULONG Length
    );


NTSTATUS
DispatchWinXpRequest (
	IN  PPRIMARY_SESSION			PrimarySession,
	IN  PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader,
	IN  PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplytHeader,
	IN  UINT32						DataSize,
	OUT	UINT32						*replyDataSize
	)
{
	NTSTATUS	status;
	UINT8			*ndfsRequestData;
	
#if DBG

	CHAR	irpMajorString[OPERATION_NAME_BUFFER_SIZE];
	CHAR	irpMinorString[OPERATION_NAME_BUFFER_SIZE];

	GetIrpName ( NdfsWinxpRequestHeader->IrpMajorFunction,
				 NdfsWinxpRequestHeader->IrpMinorFunction,
				 NdfsWinxpRequestHeader->FileSystemControl.FsControlCode,
				 irpMajorString,
				 OPERATION_NAME_BUFFER_SIZE,
				 irpMinorString,
				 OPERATION_NAME_BUFFER_SIZE );

    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
				   ("DispatchWinXpRequest: PrimarySession = %p, NdfsWinxpRequestHeader->IrpTag4 = %x, %s %s, DataSize = %d\n", 
					 PrimarySession, NTOHL(NdfsWinxpRequestHeader->IrpTag4), irpMajorString, irpMinorString, DataSize) );

#endif

	if (DataSize) {

		ndfsRequestData = (UINT8 *)(NdfsWinxpRequestHeader+1);

	} else {

		ndfsRequestData = NULL;
	}

#if DBG

	if (NdfsWinxpRequestHeader->IrpMajorFunction != IRP_MJ_CREATE) {

		UINT64				openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE			openFile;

		UNICODE_STRING		RECYCLER;
//		ULONG				originalFileSpyDebugLevel; 
		
		openFile = PrimarySession_FindOpenFile( PrimarySession, openFileId );
		
		if (openFile != NULL) {

			RtlInitUnicodeString(&RECYCLER, L"\\RECYCLER");
		
			if (openFile->FileObject && openFile->FileObject->FileName.Buffer && 
				openFile->FileObject->FileName.Length >= RECYCLER.Length      && 
				wcsncmp(RECYCLER.Buffer, openFile->FileObject->FileName.Buffer, RECYCLER.Length) == 0) {

				//originalFileSpyDebugLevel = gFileSpyDebugLevel;
				//gFileSpyDebugLevel = originalFileSpyDebugLevel;
			}
		}
	}

#endif

	switch (NdfsWinxpRequestHeader->IrpMajorFunction) {

	case IRP_MJ_CREATE: { //0x00
	
		UNICODE_STRING		fileName;

		HANDLE				fileHandle = NULL;
	    ACCESS_MASK			desiredAccess;
		ULONG				attributes;
		OBJECT_ATTRIBUTES	objectAttributes;
		IO_STATUS_BLOCK		ioStatusBlock;
		LARGE_INTEGER		allocationSize;
		ULONG				fileAttributes;
	    ULONG				shareAccess;
	    ULONG				disposition;
		ULONG				createOptions;
	    PVOID				eaBuffer;
		ULONG				eaLength;
	    CREATE_FILE_TYPE	createFileType;
		ULONG				options;

		POPEN_FILE			openFile = NULL;
		UINT64				relatedOpenFileId = NdfsWinxpRequestHeader->Create.RelatedFileHandle;
		POPEN_FILE			relatedOpenFile = NULL;
		
		try {

			RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

			RtlInitEmptyUnicodeString( &fileName, 
									   PrimarySession->FileNameBuffer, 
									   sizeof(PrimarySession->FileNameBuffer) );

			if (relatedOpenFileId == 0) {

			    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
							   ("DispatchWinXpRequest: LFS_WINXP_REQUEST_CREATE: No RelatedFileHandle\n") );

				status = RtlAppendUnicodeStringToString( &fileName,
													     &PrimarySession->NetdiskPartitionInformation.VolumeName );

				if (status != STATUS_SUCCESS) {

					NDAS_ASSERT( LFS_UNEXPECTED );
					leave;
				}
		
			} else {

				relatedOpenFile = PrimarySession_FindOpenFile( PrimarySession, relatedOpenFileId );
			
				if (relatedOpenFile == NULL) {

					NDAS_ASSERT( FALSE );
					status = STATUS_FILE_CLOSED;
					goto OUT2;
				}

				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
							   ("DispatchWinXpRequest: LFS_WINXP_REQUEST_CREATE: relatedOpenFile->OpenFileId = %X\n",
								 relatedOpenFile->OpenFileId) );
			}

			if (NdfsWinxpRequestHeader->Create.FileNameLength) {

				status = RtlAppendUnicodeToString( &fileName,
												   (PWCHAR)&ndfsRequestData[NdfsWinxpRequestHeader->Create.EaLength] );

				if (status != STATUS_SUCCESS) {

					NDAS_ASSERT( LFS_UNEXPECTED );
					status = STATUS_NAME_TOO_LONG;
					goto OUT2;
				}
			}

		    SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
							("DispatchWinXpRequest: LFS_WINXP_REQUEST_CREATE: fileName = %wZ\n", &fileName) );
		
			desiredAccess = NdfsWinxpRequestHeader->Create.SecurityContext.DesiredAccess;

			attributes = OBJ_KERNEL_HANDLE;
			if (!FlagOn(NdfsWinxpRequestHeader->IrpSpFlags, SL_CASE_SENSITIVE))
				attributes |= OBJ_CASE_INSENSITIVE;

			InitializeObjectAttributes( &objectAttributes,
										&fileName,
										attributes,
										relatedOpenFileId ? relatedOpenFile->FileHandle : NULL,
										NULL );

			allocationSize.QuadPart = NdfsWinxpRequestHeader->Create.AllocationSize;

			fileAttributes			= NdfsWinxpRequestHeader->Create.FileAttributes;		
			shareAccess				= NdfsWinxpRequestHeader->Create.ShareAccess;
			disposition				= (NdfsWinxpRequestHeader->Create.Options & 0xFF000000) >> 24;
			createOptions			= NdfsWinxpRequestHeader->Create.Options & 0x00FFFFFF;
			createOptions			|= (NdfsWinxpRequestHeader->Create.SecurityContext.FullCreateOptions & FILE_DELETE_ON_CLOSE);
			eaBuffer				= &ndfsRequestData[0];
			eaLength				= NdfsWinxpRequestHeader->Create.EaLength;
			createFileType			= CreateFileTypeNone;
			options					= NdfsWinxpRequestHeader->IrpSpFlags & 0x000000FF;
		
			SPY_LOG_PRINT( (NdfsWinxpRequestHeader->Create.FileNameLength == 0) ? LFS_DEBUG_PRIMARY_TRACE : LFS_DEBUG_PRIMARY_TRACE,
							("DispatchWinXpRequest: LFS_WINXP_REQUEST_CREATE: DesiredAccess = %X. Synchronize:%d "
							 "Dispo:%02lX CreateOptions = %X. Synchronize: Alert=%d Non-Alert=%d EaLen:%d\n",
							  desiredAccess,
							  (desiredAccess & SYNCHRONIZE) != 0,
							  disposition,
							  createOptions,
							  (createOptions & FILE_SYNCHRONOUS_IO_ALERT) != 0,
							  (createOptions & FILE_SYNCHRONOUS_IO_NONALERT) != 0,
							  eaLength) );

			//
			//	force the file to be synchronized.
			//

			desiredAccess |= SYNCHRONIZE;
		
			if (!(createOptions & FILE_SYNCHRONOUS_IO_NONALERT))
				createOptions |= FILE_SYNCHRONOUS_IO_ALERT;
			//createOptions |=  FILE_WRITE_THROUGH;
		
			do {
			
				POPEN_FILE	siblingOpenFile;

				status = IoCreateFile( &fileHandle,
									   desiredAccess,
									   &objectAttributes,
									   &ioStatusBlock,
									   &allocationSize,
									   fileAttributes,
									   shareAccess,
									   disposition,
									   createOptions,
									   eaBuffer,
									   eaLength,
									   createFileType,
									   NULL,
									   options );

				if (status != STATUS_SHARING_VIOLATION) {
			
					break;
				} 

				if (relatedOpenFileId) {

					break;
				}

				//
				// Close open handle that I may already opened.
				//

				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
							   ("DispatchWinXpRequest: IoCreateFile failed %x\n", status) );

				siblingOpenFile = PrimarySession_FindOpenFileCleanUpedAndNotClosed( PrimarySession,
																					&fileName,
																					FALSE );

				if (siblingOpenFile == NULL) {

					SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
								   ("DispatchWinXpRequest: Failed to open sibling open file. createStatus=%x\n", status) );
					break;
				} 

				NDAS_ASSERT( siblingOpenFile->FileObject );
				ObDereferenceObject( siblingOpenFile->FileObject );
				siblingOpenFile->FileObject = NULL;

				status = ZwClose( siblingOpenFile->FileHandle );
				NDAS_ASSERT( status == STATUS_SUCCESS );
				siblingOpenFile->FileHandle = NULL;

				status = ZwClose( siblingOpenFile->EventHandle );
				NDAS_ASSERT( status == STATUS_SUCCESS );
				siblingOpenFile->EventHandle = NULL;

				siblingOpenFile->AlreadyClosed = TRUE;

			} while (1);

			if (status == STATUS_SUCCESS) {

				PFILE_OBJECT	fileObject;
				HANDLE			eventHandle;

				NDAS_ASSERT( fileHandle != 0 );
				NDAS_ASSERT( status == ioStatusBlock.Status );

				openFile = PrimarySession_AllocateOpenFile( PrimarySession,
															fileHandle,
															NULL,
															&fileName );

				if (openFile == NULL) {
			
					NDAS_ASSERT( LFS_UNEXPECTED );

					SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_ERROR, ("Failed to allocate OpenFile structure.\n") );
				
					status = STATUS_INSUFFICIENT_RESOURCES;
					break;
				}

				status = ObReferenceObjectByHandle( fileHandle,
													FILE_READ_DATA,
													NULL,
													KernelMode,
													&fileObject,
													NULL );
	
				if (status != STATUS_SUCCESS) {

					NDAS_ASSERT( LFS_UNEXPECTED );
					ZwClose( fileHandle );
					leave;
				}

				openFile->FileObject = fileObject;

				status = ZwCreateEvent( &eventHandle,
										GENERIC_READ,
										NULL,
										SynchronizationEvent,
										FALSE );

				if (status != STATUS_SUCCESS) {

					NDAS_ASSERT( LFS_UNEXPECTED );	
					ObDereferenceObject( fileObject );
					ZwClose( fileHandle );
					leave;
				}

				openFile->EventHandle = eventHandle;
				openFile->AlreadyClosed = FALSE;
				openFile->CleanUp = FALSE;
				openFile->DesiredAccess = desiredAccess;
				openFile->CreateOptions = createOptions;
		
			} else {
			
				NDAS_ASSERT( !NT_SUCCESS(status) );
			}

#if DBG
			if (NdfsWinxpRequestHeader->Create.FileNameLength) {

				UNICODE_STRING	RECYCLER;
				PWCHAR			createFileName = (PWCHAR)&ndfsRequestData[NdfsWinxpRequestHeader->Create.EaLength];	
			

				RtlInitUnicodeString( &RECYCLER, L"\\RECYCLER" );
	
				if (wcslen(createFileName) >= RECYCLER.Length && wcsncmp(RECYCLER.Buffer, createFileName, RECYCLER.Length) == 0) {

					SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
								   ("DispatchWinXpRequest: LFS_WINXP_REQUEST_CREATE: openFile->OpenFileId = %X, createStatus = %X, ioStatusBlock = %X\n",
									 openFile ? openFile->OpenFileId : 0, status, ioStatusBlock.Information) );
				}
			}

			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
						   ("DispatchWinXpRequest: LFS_WINXP_REQUEST_CREATE: openFile->OpenFileId = %X, createStatus = %X, ioStatusBlock = %X\n",
							 openFile ? openFile->OpenFileId : 0, status, ioStatusBlock.Information) );
#endif
		
OUT2:
			NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
			NdfsWinxpReplytHeader->IrpMajorFunction	= NdfsWinxpRequestHeader->IrpMajorFunction;
			NdfsWinxpReplytHeader->IrpMinorFunction	= NdfsWinxpRequestHeader->IrpMinorFunction;

			NdfsWinxpReplytHeader->Status4	  = HTONL(status);

			NDAS_ASSERT( ioStatusBlock.Information <= 0xffffffff );

			NdfsWinxpReplytHeader->Information32 = HTONL((UINT32)ioStatusBlock.Information);

			if (status == STATUS_SUCCESS) {

				NdfsWinxpReplytHeader->Open.FileHandle = openFile->OpenFileId;
				NdfsWinxpReplytHeader->Open.SetSectionObjectPointer = openFile->FileObject->SectionObjectPointer ? TRUE : FALSE;
			}
	
			status = STATUS_SUCCESS;

		} finally {

			*replyDataSize = 0;
		}

		break;
	}

	case IRP_MJ_CLOSE: { // 0x02

	   UINT64			openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE		openFile;
		KIRQL			oldIrql;

		
		openFile = PrimarySession_FindOpenFile( PrimarySession, openFileId );
		NDAS_ASSERT( openFile );

		if (openFile->AlreadyClosed == FALSE) {

			NDAS_ASSERT( openFile->FileObject );
			ObDereferenceObject( openFile->FileObject );
			openFile->FileObject = NULL;

			status = ZwClose( openFile->FileHandle );
			NDAS_ASSERT( status == STATUS_SUCCESS );
			openFile->FileHandle = NULL;

			status = ZwClose( openFile->EventHandle );
			NDAS_ASSERT( status == STATUS_SUCCESS );
			openFile->EventHandle = NULL;

			openFile->AlreadyClosed = TRUE;
		}
		
		KeAcquireSpinLock( &PrimarySession->Thread.OpenedFileQSpinLock, &oldIrql );
		RemoveEntryList( &openFile->ListEntry );
		KeReleaseSpinLock( &PrimarySession->Thread.OpenedFileQSpinLock, oldIrql );
		
		InitializeListHead( &openFile->ListEntry );
		PrimarySession_FreeOpenFile( PrimarySession, openFile );
		
		NdfsWinxpReplytHeader->IrpTag4			 = NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status4		= HTONL(STATUS_SUCCESS);
		NdfsWinxpReplytHeader->Information32	= 0;

		*replyDataSize = 0;
		status = STATUS_SUCCESS;

		break;
	}

	case IRP_MJ_READ: { // 0x03
	
		UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE				openFile;

		HANDLE					fileHandle = NULL;	
		BOOLEAN					synchronousIo;
		
		IO_STATUS_BLOCK			ioStatusBlock;
		PVOID					buffer;
		ULONG					length;
		LARGE_INTEGER			byteOffset;
		ULONG					key;


		openFile = PrimarySession_FindOrReopenOpenFile( PrimarySession, openFileId );

		if (openFile == NULL) {

			status = STATUS_UNSUCCESSFUL;
			break;
		}

		fileHandle = openFile->FileHandle;

		RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

		buffer = (UINT8 *)(NdfsWinxpReplytHeader+1);

		length				= NdfsWinxpRequestHeader->Read.Length;
		byteOffset.QuadPart = NdfsWinxpRequestHeader->Read.ByteOffset;
		
		key = (openFile->AlreadyClosed == FALSE) ? NdfsWinxpRequestHeader->Read.Key : 0;
		synchronousIo = openFile->FileObject ? BooleanFlagOn(openFile->FileObject->Flags, FO_SYNCHRONOUS_IO) : TRUE;

		if (synchronousIo) {

			status = NtReadFile( fileHandle,
								 NULL,
								 NULL,
								 NULL,
								 &ioStatusBlock,
								 buffer,
								 length,
								 &byteOffset,
								 key ? &key : NULL );
		
		} else {

			status = NtReadFile( fileHandle,
								 openFile->EventHandle,
								 NULL,
								 NULL,
								 &ioStatusBlock,
								 buffer,
								 length,
								 &byteOffset,
								 key ? &key : NULL );
		
			if (status == STATUS_PENDING) {

				status = ZwWaitForSingleObject(openFile->EventHandle, TRUE, NULL);
				
				if (status == STATUS_SUCCESS) {

					status = ioStatusBlock.Status;
				}
			}
		}

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					   ("DispatchWinXpRequest: ZwReadFile: openFileId = %X, synchronousIo = %d, length = %d, readStatus = %X, byteOffset = %I64d, ioStatusBlock.Status = %X, ioStatusBlock.Information = %d\n",
						 openFileId, synchronousIo, length, status, byteOffset.QuadPart, ioStatusBlock.Status, ioStatusBlock.Information));

		if (status != STATUS_SUCCESS) {

			NDAS_ASSERT( !NT_SUCCESS(status) );
			NDAS_ASSERT( ioStatusBlock.Information == 0 );
			ioStatusBlock.Information = 0;
		}

		NdfsWinxpReplytHeader->IrpTag4			 = NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		ASSERT(ioStatusBlock.Information <= 0xffffffff);

		NdfsWinxpReplytHeader->Status4		= HTONL(status);
		NdfsWinxpReplytHeader->Information32	= HTONL((UINT32)ioStatusBlock.Information);

		*replyDataSize = (UINT32)ioStatusBlock.Information;
		ASSERT(*replyDataSize <= NdfsWinxpRequestHeader->Read.Length);

		status = STATUS_SUCCESS;

		break;
	}
	
	case IRP_MJ_WRITE: { // 0x04
	
		LONG					trial = 0;

		UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE				openFile;

		HANDLE					fileHandle = NULL;
		PFILE_OBJECT			fileObject = NULL;

		IO_STATUS_BLOCK			ioStatusBlock;
		PVOID					buffer;
		ULONG					length;
		LARGE_INTEGER			byteOffset;
		ULONG					key;

		
RETRY_WRITE:

		openFile = PrimarySession_FindOrReopenOpenFile( PrimarySession, openFileId );

		if (openFile == NULL) {

			status = STATUS_UNSUCCESSFUL;
			break;
		}
		
		fileHandle = openFile->FileHandle;
		fileObject = openFile->FileObject;

		buffer				= (UINT8 *)(NdfsWinxpRequestHeader+1);
		length				= NdfsWinxpRequestHeader->Write.Length;
		byteOffset.QuadPart = NdfsWinxpRequestHeader->Write.ByteOffset;

		key = (openFile->AlreadyClosed == FALSE) ? NdfsWinxpRequestHeader->Write.Key : 0;

		RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );
		
		status = NtWriteFile( fileHandle,
							  NULL,
							  NULL,
							  NULL,
							  &ioStatusBlock,
							  buffer,
							  length,
							  &byteOffset,
							  key ? &key : NULL );

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					   ("DispatchWinXpRequest: NtWriteFile: openFileId = %wZ, Offset=%I64x, Length=%x, Result=%08x\n",
						 &openFile->FullFileName, byteOffset.QuadPart, length, status) );
		
		if (status != STATUS_SUCCESS) {

			NDAS_ASSERT( !NT_SUCCESS(status) );
			NDAS_ASSERT( ioStatusBlock.Information == 0 );
			ioStatusBlock.Information = 0;
		}

		if (status == STATUS_ACCESS_DENIED) {

			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					       ("DispatchWinXpRequest: ZwWriteFile: Access denied\n") );
			
			if(openFile->AlreadyClosed == TRUE) {

				NDAS_ASSERT( LFS_UNEXPECTED );
				
				NdfsWinxpReplytHeader->IrpTag4			 = NdfsWinxpRequestHeader->IrpTag4;
				NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
				NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

				NdfsWinxpReplytHeader->Status4	   = HTONL(STATUS_SUCCESS);
				NdfsWinxpReplytHeader->Information32 = HTONL(NdfsWinxpRequestHeader->Write.Length);

				status = STATUS_SUCCESS;
			
			} else if (openFile->CleanUp == TRUE) {

				NDAS_ASSERT( openFile->FileObject );
				ObDereferenceObject( openFile->FileObject );
				openFile->FileObject = NULL;

				status = ZwClose( openFile->FileHandle );
				NDAS_ASSERT( status == STATUS_SUCCESS );
				openFile->FileHandle = NULL;

				status = ZwClose( openFile->EventHandle );
				NDAS_ASSERT( status == STATUS_SUCCESS );
				openFile->EventHandle = NULL;

				openFile->AlreadyClosed = TRUE;
				
				goto RETRY_WRITE;
			
			} else {

				goto RETRY_WRITE;
			}

		} else if (!NT_SUCCESS(status)) {
			
			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					       ("DispatchWinXpRequest: ZwWriteFile: failed %x\n", status) );
		}

		NdfsWinxpReplytHeader->IrpTag4			 = NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NDAS_ASSERT( ioStatusBlock.Information <= 0xffffffff );

		NdfsWinxpReplytHeader->Status4			  = HTONL(status);
		NdfsWinxpReplytHeader->Information32	  = HTONL((UINT32)ioStatusBlock.Information);
		NdfsWinxpReplytHeader->CurrentByteOffset8 = HTONLL(fileObject->CurrentByteOffset.QuadPart);

		*replyDataSize = 0;
		status = STATUS_SUCCESS;

		break;
	}
	
    case IRP_MJ_QUERY_INFORMATION: // 0x05
	{
		NTSTATUS				queryInformationStatus;

		UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE				openFile;
		HANDLE					fileHandle = NULL;
		PFILE_OBJECT			fileObject = NULL;
		BOOLEAN					closeOnExit = FALSE;

	    PVOID					fileInformation;
		ULONG					length;
		FILE_INFORMATION_CLASS	fileInformationClass;
		ULONG					returnedLength = 0;


		openFile = PrimarySession_FindOrReopenOpenFile(
			PrimarySession,
			openFileId);

		if (openFile == NULL) {

			status = STATUS_UNSUCCESSFUL;
			break;
		}

		fileHandle = openFile->FileHandle;
		fileObject = openFile->FileObject;


		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
				("DispatchWinXpRequest: IoQueryFileInformation: openFileId = %X, Infoclass = %X\n",
					openFileId, NdfsWinxpRequestHeader->QueryFile.FileInformationClass));

		ASSERT(NdfsWinxpRequestHeader->QueryFile.Length <= PrimarySession->SessionContext.SecondaryMaxDataSize);

		fileInformation		 = (UINT8 *)(NdfsWinxpReplytHeader+1);
		length				 = NdfsWinxpRequestHeader->QueryFile.Length;
		fileInformationClass = NdfsWinxpRequestHeader->QueryFile.FileInformationClass;


		queryInformationStatus = IoQueryFileInformation(
										fileObject,
										fileInformationClass,
										length,
										fileInformation,
										&returnedLength
										);

		if(queryInformationStatus == STATUS_BUFFER_OVERFLOW)
			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
					("DispatchWinXpRequest: IoQueryFileInformation: openFileId = %X, length = %d, queryInformationStatus = %X, returnedLength = %d\n",
						openFileId, length, queryInformationStatus, returnedLength));

		if(NT_SUCCESS(queryInformationStatus))
			ASSERT(queryInformationStatus == STATUS_SUCCESS);

		if(queryInformationStatus == STATUS_BUFFER_OVERFLOW)
			ASSERT(length == returnedLength);

		if(!(queryInformationStatus == STATUS_SUCCESS || queryInformationStatus == STATUS_BUFFER_OVERFLOW))
		{
			returnedLength = 0;
			//ASSERT(returnedLength == 0);
		}
		
		NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;
		
		NdfsWinxpReplytHeader->Status4	     = HTONL(queryInformationStatus);
		NdfsWinxpReplytHeader->Information32 = HTONL(returnedLength);

		//*replyDataSize = NdfsWinxpRequestHeader->QueryFile.Length <= returnedLength 
		//					? NdfsWinxpRequestHeader->QueryFile.Length : returnedLength;
		*replyDataSize = returnedLength;
		
		status = STATUS_SUCCESS;

		if(closeOnExit == TRUE)
		{
			ObDereferenceObject(fileObject);
			ZwClose(fileHandle);
		}

		break;
	}
	
#if 0

    case IRP_MJ_SET_INFORMATION:  // 0x06
	{
		NTSTATUS				setInformationStatus;

		UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE				openFile;
		
	    PVOID					fileInformation;
		ULONG					length;
		FILE_INFORMATION_CLASS	fileInformationClass;


#if 0
		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (UINT32)openFile);
#else
		openFile = PrimarySession_FindOrReopenOpenFile(
			PrimarySession,
			openFileId);
#endif
		fileInformation = ExAllocatePoolWithTag( 
								NonPagedPool, 
								NdfsWinxpRequestHeader->SetFile.Length 
								+ PrimarySession->NetdiskPartition->NetdiskPartitionInformation.VolumeName.Length,
								PRIMARY_SESSION_BUFFERE_TAG
								);

		if (fileInformation == NULL) {

			NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		RtlZeroMemory(
			fileInformation,
			NdfsWinxpRequestHeader->SetFile.Length 
				+ PrimarySession->NetdiskPartition->NetdiskPartitionInformation.VolumeName.Length
			);

		if(NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileBasicInformation)
		{
			PFILE_BASIC_INFORMATION		basicInformation = fileInformation;
				
			basicInformation->CreationTime.QuadPart   = NdfsWinxpRequestHeader->SetFile.BasicInformation.CreationTime;
			basicInformation->LastAccessTime.QuadPart = NdfsWinxpRequestHeader->SetFile.BasicInformation.LastAccessTime;
			basicInformation->LastWriteTime.QuadPart  = NdfsWinxpRequestHeader->SetFile.BasicInformation.LastWriteTime;
			basicInformation->ChangeTime.QuadPart     = NdfsWinxpRequestHeader->SetFile.BasicInformation.ChangeTime;
			basicInformation->FileAttributes          = NdfsWinxpRequestHeader->SetFile.BasicInformation.FileAttributes;

		}
		else if(NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileLinkInformation) 
		{
			PFILE_LINK_INFORMATION		linkInfomation = fileInformation;
			POPEN_FILE					rootDirectoryFile;

			ASSERT(sizeof(FILE_LINK_INFORMATION) - sizeof(WCHAR) + NdfsWinxpRequestHeader->SetFile.LinkInformation.FileNameLength <= NdfsWinxpRequestHeader->SetFile.Length);
				
			if(NdfsWinxpRequestHeader->SetFile.RenameInformation.RootDirectoryHandle != 0)
			{
				rootDirectoryFile = PrimarySession_FindOpenFile(
										PrimarySession,
										NdfsWinxpRequestHeader->SetFile.LinkInformation.RootDirectoryHandle
										);
				ASSERT(rootDirectoryFile);
				ASSERT(rootDirectoryFile->AlreadyClosed == FALSE);
			}
			else
				rootDirectoryFile = NULL;

			linkInfomation->ReplaceIfExists = NdfsWinxpRequestHeader->SetFile.LinkInformation.ReplaceIfExists;
			linkInfomation->RootDirectory = rootDirectoryFile ? rootDirectoryFile->FileHandle : NULL;
			linkInfomation->FileNameLength = NdfsWinxpRequestHeader->SetFile.LinkInformation.FileNameLength;
			
			RtlCopyMemory(
				linkInfomation->FileName,
				ndfsRequestData,
				linkInfomation->FileNameLength
				);
			
			SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_INFO,
						("DispatchWinXpRequest: linkInfomation->FileNameLength = %u\n", linkInfomation->FileNameLength));
		}
		else if(NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileRenameInformation) 
		{
			PFILE_RENAME_INFORMATION	renameInformation = fileInformation;
			POPEN_FILE					rootDirectoryFile;
			
			ASSERT(sizeof(FILE_RENAME_INFORMATION) - sizeof(WCHAR) + NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength <= NdfsWinxpRequestHeader->SetFile.Length);
						
			if(NdfsWinxpRequestHeader->SetFile.RenameInformation.RootDirectoryHandle != 0)
			{
				rootDirectoryFile = PrimarySession_FindOpenFile(
										PrimarySession,
										NdfsWinxpRequestHeader->SetFile.RenameInformation.RootDirectoryHandle
										);
				ASSERT(rootDirectoryFile);
				ASSERT(rootDirectoryFile->AlreadyClosed == FALSE);
			}
			else
				rootDirectoryFile = NULL;
		
			renameInformation->ReplaceIfExists = NdfsWinxpRequestHeader->SetFile.RenameInformation.ReplaceIfExists;
			renameInformation->RootDirectory = rootDirectoryFile ? rootDirectoryFile->FileHandle : NULL;

#if 0
			renameInformation->FileNameLength = NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength;
			RtlCopyMemory(
				renameInformation->FileName,
				ndfsRequestData,
				renameInformation->FileNameLength
				);

			SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_INFO,
					("DispatchWinXpRequest: renameInformation->FileName = %ws\n", renameInformation->FileName));
#endif
			
			//
			// Check to see whether or not a fully qualified pathname was supplied.
			// If so, then more processing is required.
			//
			if(ndfsRequestData[0] == (UCHAR) OBJ_NAME_PATH_SEPARATOR ||
				renameInformation->RootDirectory != NULL
				) 
			{
				ULONG		byteoffset ;
				ULONG		idx_unicodestr ;
				PWCHAR		unicodestr ;
				BOOLEAN		found ;

				found = FALSE ;
				unicodestr = (PWCHAR)ndfsRequestData;
				for(	idx_unicodestr = 0 ;
						idx_unicodestr < ( NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength / sizeof(WCHAR) - 1 );
						idx_unicodestr ++
					) {
					if( L':'  == unicodestr[idx_unicodestr] &&
						L'\\' == unicodestr[idx_unicodestr + 1] ) {

						idx_unicodestr ++;
						found = TRUE;
						break ;
					}
				}

				if(found) {
					RtlCopyMemory(
						renameInformation->FileName,
						PrimarySession->NetdiskPartition->NetdiskPartitionInformation.VolumeName.Buffer,
						PrimarySession->NetdiskPartition->NetdiskPartitionInformation.VolumeName.Length
					);
					renameInformation->FileNameLength 
						= PrimarySession->NetdiskPartition->NetdiskPartitionInformation.VolumeName.Length;

					byteoffset = idx_unicodestr*sizeof(WCHAR);
					RtlCopyMemory(
							(PUCHAR)renameInformation->FileName + renameInformation->FileNameLength,
							(PUCHAR)ndfsRequestData + byteoffset,
							NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength - byteoffset
						) ;
					renameInformation->FileNameLength += NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength - byteoffset ;
				} else {
					ASSERT(LFS_BUG);
				}

				SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_INFO,
						("DispatchWinXpRequest: renameInformation->FileName = %ws\n", renameInformation->FileName));
			} 
			else 
			{
				renameInformation->FileNameLength = NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength;
				RtlCopyMemory(
					renameInformation->FileName,
					ndfsRequestData,
					renameInformation->FileNameLength
				);
			}
		}
		else if(NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileDispositionInformation) 
		{
			PFILE_DISPOSITION_INFORMATION	dispositionInformation = fileInformation;
			
			dispositionInformation->DeleteFile = NdfsWinxpRequestHeader->SetFile.DispositionInformation.DeleteFile;
		}
		else if(NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileEndOfFileInformation) 
		{
			PFILE_END_OF_FILE_INFORMATION fileEndOfFileInformation = fileInformation;
			
			fileEndOfFileInformation->EndOfFile.QuadPart = NdfsWinxpRequestHeader->SetFile.EndOfFileInformation.EndOfFile;
		}
		else if(NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileAllocationInformation) 
		{
			PFILE_ALLOCATION_INFORMATION fileAllocationInformation = fileInformation;

			fileAllocationInformation->AllocationSize.QuadPart = NdfsWinxpRequestHeader->SetFile.AllocationInformation.AllocationSize;
		}
		else if( FilePositionInformation == NdfsWinxpRequestHeader->SetFile.FileInformationClass) 
		{
			PFILE_POSITION_INFORMATION filePositionInformation = fileInformation;

			filePositionInformation->CurrentByteOffset.QuadPart = NdfsWinxpRequestHeader->SetFile.PositionInformation.CurrentByteOffset;
		}
		else
			ASSERT(LFS_BUG);


		length = NdfsWinxpRequestHeader->SetFile.Length;
		fileInformationClass = NdfsWinxpRequestHeader->SetFile.FileInformationClass;
		
		setInformationStatus = IoSetInformation(
										openFile->FileObject,
										fileInformationClass,
										length,
										fileInformation
										);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("DispatchWinXpRequest: IoSetInformation: fileHandle = %p, fileInformationClass = %X, setInformationStatus = %X\n",
						openFile->FileHandle, fileInformationClass, setInformationStatus));

		if(NT_SUCCESS(setInformationStatus))
			ASSERT(setInformationStatus == STATUS_SUCCESS);
		
		NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status4	   = HTONL(setInformationStatus);

		if(NT_SUCCESS(setInformationStatus))
			NdfsWinxpReplytHeader->Information32 = HTONL(length);
		else
			NdfsWinxpReplytHeader->Information32 = 0;

		*replyDataSize = 0;
		
		
		status = STATUS_SUCCESS;

		ASSERT(fileInformation);
		
		ExFreePoolWithTag(
			fileInformation,
			PRIMARY_SESSION_BUFFERE_TAG
			);

		break;
	}

#else

	case IRP_MJ_SET_INFORMATION:  { // 0x06

		NTSTATUS				setInformationStatus;

		UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE				openFile;
		
	    PVOID					fileInformation;
		ULONG					length;
		FILE_INFORMATION_CLASS	fileInformationClass;


		openFile = PrimarySession_FindOrReopenOpenFile( PrimarySession, openFileId );
		if (openFile == NULL) {

			status = STATUS_UNSUCCESSFUL;
			break;
		}

		//ASSERT( openFile && openFile->OpenFileId == openFile );

		fileInformation = ExAllocatePoolWithTag( NonPagedPool, 
												 NdfsWinxpRequestHeader->SetFile.Length +
												 2 + // NULL
												 8 + // 64 bit adjustment
												 8 + // dummy
												 PrimarySession->NetdiskPartitionInformation.VolumeName.Length,
												 PRIMARY_SESSION_BUFFERE_TAG );

		if (fileInformation == NULL) {

			NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		RtlZeroMemory( fileInformation, 
					   NdfsWinxpRequestHeader->SetFile.Length + 
					   2 +
					   8 +
					   8 +
					   PrimarySession->NetdiskPartitionInformation.VolumeName.Length );

		if (NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileBasicInformation) {

			PFILE_BASIC_INFORMATION		basicInformation = fileInformation;
				
			basicInformation->CreationTime.QuadPart   = NdfsWinxpRequestHeader->SetFile.BasicInformation.CreationTime;
			basicInformation->LastAccessTime.QuadPart = NdfsWinxpRequestHeader->SetFile.BasicInformation.LastAccessTime;
			basicInformation->LastWriteTime.QuadPart  = NdfsWinxpRequestHeader->SetFile.BasicInformation.LastWriteTime;
			basicInformation->ChangeTime.QuadPart     = NdfsWinxpRequestHeader->SetFile.BasicInformation.ChangeTime;
			basicInformation->FileAttributes          = NdfsWinxpRequestHeader->SetFile.BasicInformation.FileAttributes;

			length = sizeof( FILE_BASIC_INFORMATION );
			fileInformationClass = FileBasicInformation;

		} else if (NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileLinkInformation) {

			PFILE_LINK_INFORMATION		linkInfomation = fileInformation;
			POPEN_FILE					rootDirectoryFile;

			ASSERT(sizeof(FILE_LINK_INFORMATION) - sizeof(WCHAR) - 8 + NdfsWinxpRequestHeader->SetFile.LinkInformation.FileNameLength <= NdfsWinxpRequestHeader->SetFile.Length);

			if (NdfsWinxpRequestHeader->SetFile.RenameInformation.RootDirectoryHandle != 0) {

				rootDirectoryFile = PrimarySession_FindOpenFile( PrimarySession,
																 NdfsWinxpRequestHeader->SetFile.LinkInformation.RootDirectoryHandle );

				//ASSERT( rootDirectoryFile && rootDirectoryFile->OpenFileId == rootDirectoryFile );
				ASSERT( rootDirectoryFile->AlreadyClosed == FALSE );
			
			} else {

				rootDirectoryFile = NULL;
			}

			linkInfomation->ReplaceIfExists = NdfsWinxpRequestHeader->SetFile.LinkInformation.ReplaceIfExists;
			linkInfomation->RootDirectory = rootDirectoryFile ? rootDirectoryFile->FileHandle : NULL;
			linkInfomation->FileNameLength = NdfsWinxpRequestHeader->SetFile.LinkInformation.FileNameLength;
			
			RtlCopyMemory( linkInfomation->FileName,
						   ndfsRequestData,
						   linkInfomation->FileNameLength );
			
			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
						   ("DispatchWinXpRequest: linkInfomation->FileNameLength = %u\n", linkInfomation->FileNameLength) );

			length = sizeof(FILE_LINK_INFORMATION) - sizeof(WCHAR) + linkInfomation->FileNameLength;
			fileInformationClass = FileLinkInformation;

		} else if (NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileRenameInformation) {

			PFILE_RENAME_INFORMATION	renameInformation = fileInformation;
			POPEN_FILE					rootDirectoryFile;
			
			ASSERT( sizeof(FILE_RENAME_INFORMATION) - sizeof(WCHAR) - 8 + NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength <= NdfsWinxpRequestHeader->SetFile.Length );
						
			if (NdfsWinxpRequestHeader->SetFile.RenameInformation.RootDirectoryHandle != 0) {

				rootDirectoryFile = PrimarySession_FindOpenFile( PrimarySession,
																 NdfsWinxpRequestHeader->SetFile.RenameInformation.RootDirectoryHandle );

				//ASSERT( rootDirectoryFile && rootDirectoryFile->OpenFileId == rootDirectoryFile );
				ASSERT( rootDirectoryFile->AlreadyClosed == FALSE );
			
			} else {

				rootDirectoryFile = NULL;
			}
		
			renameInformation->ReplaceIfExists = NdfsWinxpRequestHeader->SetFile.RenameInformation.ReplaceIfExists;
			renameInformation->RootDirectory = rootDirectoryFile ? rootDirectoryFile->FileHandle : NULL;

#if 0
			renameInformation->FileNameLength = NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength;
			RtlCopyMemory( renameInformation->FileName,
						   ndfsRequestData,
						   renameInformation->FileNameLength );

			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO, 
						   ("DispatchWinXpRequest: renameInformation->FileName = %ws\n", renameInformation->FileName) );
#endif
			
			//
			// Check to see whether or not a fully qualified pathname was supplied.
			// If so, then more processing is required.
			//

			if (ndfsRequestData[0] == (UCHAR) OBJ_NAME_PATH_SEPARATOR ||
				renameInformation->RootDirectory != NULL ) {

				ULONG		byteoffset;
				ULONG		idx_unicodestr;
				PWCHAR		unicodestr;
				BOOLEAN		found;

				found = FALSE;

				unicodestr = (PWCHAR)ndfsRequestData;
				
				for (idx_unicodestr = 0;
					 idx_unicodestr < ( NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength / sizeof(WCHAR) - 1 );
					 idx_unicodestr ++) {

					if (L':'  == unicodestr[idx_unicodestr] && L'\\' == unicodestr[idx_unicodestr + 1]) {

						idx_unicodestr ++;
						found = TRUE;
						break;
					}
				}

				if (found) {

					RtlCopyMemory( renameInformation->FileName,
								   PrimarySession->NetdiskPartitionInformation.VolumeName.Buffer,
								   PrimarySession->NetdiskPartitionInformation.VolumeName.Length );

					renameInformation->FileNameLength = 
						PrimarySession->NetdiskPartitionInformation.VolumeName.Length;

					byteoffset = idx_unicodestr*sizeof(WCHAR);
					
					RtlCopyMemory( (PUCHAR)renameInformation->FileName + renameInformation->FileNameLength,
								   (PUCHAR)ndfsRequestData + byteoffset,
								   NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength - byteoffset );

					renameInformation->FileNameLength += 
						NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength - byteoffset;
				
				} else {
				
					ASSERT( LFS_BUG );
				}

				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_ERROR,
							   ("DispatchWinXpRequest: renameInformation->FileName = %ws\n", renameInformation->FileName) );
			
			} else {

				renameInformation->FileNameLength = NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength;
				
				RtlCopyMemory( renameInformation->FileName,
							   ndfsRequestData,
							   renameInformation->FileNameLength );
			}

			length = sizeof(FILE_RENAME_INFORMATION) - sizeof(WCHAR) + renameInformation->FileNameLength;
			fileInformationClass = FileRenameInformation;

		} else if (NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileDispositionInformation) {

			PFILE_DISPOSITION_INFORMATION	dispositionInformation = fileInformation;
			
			dispositionInformation->DeleteFile = NdfsWinxpRequestHeader->SetFile.DispositionInformation.DeleteFile;
			
			length = sizeof( FILE_DISPOSITION_INFORMATION );
			fileInformationClass = FileDispositionInformation;

		} else if (NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileEndOfFileInformation) {
			
			PFILE_END_OF_FILE_INFORMATION fileEndOfFileInformation = fileInformation;
			
			fileEndOfFileInformation->EndOfFile.QuadPart = NdfsWinxpRequestHeader->SetFile.EndOfFileInformation.EndOfFile;

			length = sizeof( FILE_END_OF_FILE_INFORMATION );
			fileInformationClass = FileEndOfFileInformation;

		} else if (NdfsWinxpRequestHeader->SetFile.FileInformationClass == FileAllocationInformation) {

			PFILE_ALLOCATION_INFORMATION fileAllocationInformation = fileInformation;

			fileAllocationInformation->AllocationSize.QuadPart = NdfsWinxpRequestHeader->SetFile.AllocationInformation.AllocationSize;
			
			length = sizeof( FILE_ALLOCATION_INFORMATION );
			fileInformationClass = FileAllocationInformation;

		} else if (FilePositionInformation == NdfsWinxpRequestHeader->SetFile.FileInformationClass) {

			PFILE_POSITION_INFORMATION filePositionInformation = fileInformation;

			filePositionInformation->CurrentByteOffset.QuadPart = NdfsWinxpRequestHeader->SetFile.PositionInformation.CurrentByteOffset;
			
			length = sizeof( FILE_POSITION_INFORMATION );
			fileInformationClass = FilePositionInformation;

		} else {

			ASSERT( LFS_BUG );
			length = NdfsWinxpRequestHeader->SetFile.Length - FIELD_OFFSET(WINXP_REQUEST_SET_FILE, LinkInformation);
			fileInformationClass = NdfsWinxpRequestHeader->SetFile.FileInformationClass;
		}

		setInformationStatus = IoSetInformation( openFile->FileObject,
												 fileInformationClass,
												 length,
												 fileInformation );

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					   ("DispatchWinXpRequest: IoSetInformation: fileHandle = %p, fileInformationClass = %X, setInformationStatus = %X\n",
						openFile->FileHandle, fileInformationClass, setInformationStatus) );

		if (NT_SUCCESS(setInformationStatus))
			ASSERT( setInformationStatus == STATUS_SUCCESS );
		
		NdfsWinxpReplytHeader->IrpTag4			 = NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status4	   = HTONL(setInformationStatus);

		if (NT_SUCCESS(setInformationStatus))
			NdfsWinxpReplytHeader->Information32 = HTONL(length);
		else
			NdfsWinxpReplytHeader->Information32 = 0;

		*replyDataSize = 0;
		
		status = STATUS_SUCCESS;

		ASSERT( fileInformation );
		
		ExFreePoolWithTag( fileInformation, PRIMARY_SESSION_BUFFERE_TAG );

		break;
	}

#endif

     case IRP_MJ_FLUSH_BUFFERS: // 0x09
	{		
#if 0
		NTSTATUS			flushBufferStatus;

		
		closeStatus = ZwClose((HANDLE)NTOHLL(NdfsWinxpRequestHeader->FileHandle8));

		ASSERT(closeStatus == STATUS_SUCCESS);
		
		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
						("DispatchWinXpRequest: IRP_MJ_CLOSE: openFileId = %x, closeStatus = %x\n",
										openFileId, closeStatus));
#endif

		NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status4	  = HTONL(STATUS_SUCCESS);
		NdfsWinxpReplytHeader->Information32 = 0;

		*replyDataSize = 0;

		
		status = STATUS_SUCCESS;

		break;
	}

	case IRP_MJ_QUERY_VOLUME_INFORMATION: // 0x0A
	{
		NTSTATUS				queryVolumeInformationStatus;

		UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE				openFile;

	    PVOID					fsInformation;
		ULONG					length;
		FILE_INFORMATION_CLASS	fsInformationClass;
	    ULONG					returnedLength = 0;
		

		openFile = PrimarySession_FindOrReopenOpenFile( PrimarySession, openFileId );

		if (openFile == NULL) {

			status = STATUS_UNSUCCESSFUL;
			break;
		}

		ASSERT(NdfsWinxpRequestHeader->QueryVolume.Length <= PrimarySession->SessionContext.SecondaryMaxDataSize);

		fsInformation		 = (UINT8 *)(NdfsWinxpReplytHeader+1);
		length				 = NdfsWinxpRequestHeader->QueryVolume.Length;
		fsInformationClass   = NdfsWinxpRequestHeader->QueryVolume.FsInformationClass;
		
		queryVolumeInformationStatus = IoQueryVolumeInformation(
										openFile->FileObject,
										fsInformationClass,
										length,
										fsInformation,
										&returnedLength
										);

		if(NT_SUCCESS(queryVolumeInformationStatus))
			ASSERT(queryVolumeInformationStatus == STATUS_SUCCESS);

		if(queryVolumeInformationStatus == STATUS_BUFFER_OVERFLOW)
			ASSERT(length == returnedLength);

		if(!(queryVolumeInformationStatus == STATUS_SUCCESS || queryVolumeInformationStatus == STATUS_BUFFER_OVERFLOW))
		{
			returnedLength = 0;
			//ASSERT(returnedLength == 0);
		} 

		if(queryVolumeInformationStatus == STATUS_BUFFER_OVERFLOW)
			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
					("DispatchWinXpRequest: IoQueryVolumeInformation: fileHandle = %p, queryVolumeInformationStatus = %X, length = %d, returnedLength = %d\n",
						openFile->FileHandle, queryVolumeInformationStatus, length, returnedLength));


		NdfsWinxpReplytHeader->IrpTag4			 = NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status4	     = HTONL(queryVolumeInformationStatus);
		NdfsWinxpReplytHeader->Information32   = HTONL(returnedLength);

		//*replyDataSize = NdfsWinxpRequestHeader->QueryVolume.Length <= returnedLength 
		//					? NdfsWinxpRequestHeader->QueryVolume.Length : returnedLength;
		*replyDataSize = returnedLength;
		
		status = STATUS_SUCCESS;

		break;
	}
	case IRP_MJ_SET_VOLUME_INFORMATION:
	{
		NTSTATUS				setVolumeStatus;

		UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE				openFile;
		
		IO_STATUS_BLOCK			IoStatusBlock;
	    PVOID					volumeInformation;
		ULONG					length;
		FS_INFORMATION_CLASS	volumeInformationClass;

		openFile = PrimarySession_FindOrReopenOpenFile( PrimarySession, openFileId );

		if (openFile == NULL) {

			status = STATUS_UNSUCCESSFUL;
			break;
		}

		length				   = NdfsWinxpRequestHeader->SetVolume.Length;
		volumeInformationClass = NdfsWinxpRequestHeader->SetVolume.FsInformationClass ;
		volumeInformation	   = (UINT8 *)(NdfsWinxpRequestHeader+1);

		setVolumeStatus = ZwSetVolumeInformationFile(
									openFile->FileHandle,
									&IoStatusBlock,
									volumeInformation,
									length,
									volumeInformationClass
								); 

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("DispatchWinXpRequest: ZwSetVolumeInformationFile: fileHandle = %p, volumeInformationClass = %X, setVolumeStatus = %X\n",
						openFile->FileHandle, volumeInformationClass, setVolumeStatus));

		if(NT_SUCCESS(setVolumeStatus))
			ASSERT(setVolumeStatus == STATUS_SUCCESS);

		NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status4	  = HTONL(setVolumeStatus);
		NdfsWinxpReplytHeader->Information32 = HTONL(length);

		*replyDataSize = 0;

		status = STATUS_SUCCESS;
		break ;
	}

	case IRP_MJ_DIRECTORY_CONTROL: // 0x0C
	{
		SPY_LOG_PRINT(LFS_DEBUG_PRIMARY_NOISE,
				("DispatchWinXpRequest: IRP_MJ_DIRECTORY_CONTROL: MinorFunction = %X\n",
					NdfsWinxpRequestHeader->IrpMajorFunction));

        if(NdfsWinxpRequestHeader->IrpMinorFunction == IRP_MN_QUERY_DIRECTORY) 
		{
			NTSTATUS				queryDirectoryStatus;

			UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
			POPEN_FILE				openFile;
		
			BOOLEAN					synchronousIo;

			IO_STATUS_BLOCK			ioStatusBlock;
		    PVOID					fileInformation;
			ULONG					length;
			FILE_INFORMATION_CLASS	fileInformationClass;
			BOOLEAN					returnSingleEntry;
			UNICODE_STRING			fileName;
			PWCHAR					fileNameBuffer;
			BOOLEAN					restartScan;
			BOOLEAN					indexSpecified;

			//
			//	Allocate a name buffer
			//
			fileNameBuffer = ExAllocatePoolWithTag(NonPagedPool, NDFS_MAX_PATH*sizeof(WCHAR), LFS_ALLOC_TAG);
			if(fileNameBuffer == NULL) {
				ASSERT(LFS_UNEXPECTED);
				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}

#if 0
			openFile = PrimarySession_FindOpenFile(
								PrimarySession,
								openFileId
								);
#else
			openFile = PrimarySession_FindOrReopenOpenFile(
				PrimarySession,
				openFileId);

			if (openFile == NULL) {

				status = STATUS_UNSUCCESSFUL;
				break;
			}
#endif
			ASSERT(openFile && openFile->FileObject);
			//ASSERT(openFile && openFile->OpenFileId == (UINT32)openFile);

			ASSERT(NdfsWinxpRequestHeader->QueryDirectory.Length <= PrimarySession->SessionContext.SecondaryMaxDataSize);
	
			fileInformation			= (UINT8 *)(NdfsWinxpReplytHeader+1);
			length					= NdfsWinxpRequestHeader->QueryDirectory.Length;
			fileInformationClass	= NdfsWinxpRequestHeader->QueryDirectory.FileInformationClass;
			returnSingleEntry		= (NdfsWinxpRequestHeader->IrpSpFlags & SL_RETURN_SINGLE_ENTRY) ? TRUE : FALSE;
			restartScan				= (NdfsWinxpRequestHeader->IrpSpFlags & SL_RESTART_SCAN) ? TRUE : FALSE;
			indexSpecified 			= (NdfsWinxpRequestHeader->IrpSpFlags & SL_INDEX_SPECIFIED) ? TRUE : FALSE;	

			if (indexSpecified) {

				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO, ("DispatchWinXpRequest: inedexspecified\n") );
			}

			RtlInitEmptyUnicodeString( 
					&fileName,
					fileNameBuffer,
					NDFS_MAX_PATH*sizeof(WCHAR)
					);

			queryDirectoryStatus = RtlAppendUnicodeToString(
										&fileName,
										(PWCHAR)ndfsRequestData
										);

			if(queryDirectoryStatus != STATUS_SUCCESS)
			{
				ExFreePool(fileNameBuffer);
				ASSERT(LFS_UNEXPECTED);
				status = STATUS_UNSUCCESSFUL;
				break;
			}

			RtlZeroMemory(
				&ioStatusBlock,
				sizeof(ioStatusBlock)
				);

			if (indexSpecified) {
				queryDirectoryStatus = LfsQueryDirectoryByIndex(
											openFile->FileHandle,
											fileInformationClass,
											fileInformation,
											length,
											(ULONG)NdfsWinxpRequestHeader->QueryDirectory.FileIndex,				
											&fileName,
											&ioStatusBlock,											
											returnSingleEntry
										);
			} else {
				synchronousIo = BooleanFlagOn(openFile->FileObject->Flags, FO_SYNCHRONOUS_IO);

			  if(synchronousIo)
			  {
				  queryDirectoryStatus = NtQueryDirectoryFile(
										  openFile->FileHandle,
										  NULL,
										  NULL,
										  NULL,
										  &ioStatusBlock,
										  fileInformation,
										  length,
										  fileInformationClass,
										  returnSingleEntry,
										  &fileName,
										  restartScan
										  );
			  }
			  else
			  {
					  ASSERT(openFile->EventHandle != NULL);
			  queryDirectoryStatus = NtQueryDirectoryFile(
										  openFile->FileHandle,
										  openFile->EventHandle,
										  NULL,
										  NULL,
										  &ioStatusBlock,
										  fileInformation,
										  length,
										  fileInformationClass,
										  returnSingleEntry,
										  &fileName,
										  restartScan
										  );
  
				  if (queryDirectoryStatus == STATUS_PENDING) 
				  {
					  queryDirectoryStatus = ZwWaitForSingleObject(openFile->EventHandle, TRUE, NULL);
				  }
			  }
			}

			if(queryDirectoryStatus == STATUS_BUFFER_OVERFLOW)
				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
						("DispatchWinXpRequest: NtQueryDirectoryFile: openFileId = %X, queryDirectoryStatus = %X, length = %d, ioStatusBlock.Status = %X, ioStatusBlock.Information = %d\n",
										openFileId, queryDirectoryStatus, length, ioStatusBlock.Status, ioStatusBlock.Information));

#if 0
			//
			//	sometimes it returns STATUS_PENDING even though NtQueryDirectoryFile() is a synchronous operation.
			//	We assume STATUS_PENDING as STATUS_SUCCESS here.
			//
			if(STATUS_PENDING == queryDirectoryStatus) {
				ASSERT(LFS_REQUIRED);
				SPY_LOG_PRINT( SPYDEBUG_TRACE_LEVEL1, ("LFS: DispatchWinXpRequest: translate STATUS_PENDING to %08lx\n", ioStatusBlock.Status)) ;	
				queryDirectoryStatus = ioStatusBlock.Status ;
			}
#endif

			if(NT_SUCCESS(queryDirectoryStatus))
			{
				ASSERT(queryDirectoryStatus == STATUS_SUCCESS);		
				ASSERT(queryDirectoryStatus == ioStatusBlock.Status);
			}
			
			if(queryDirectoryStatus == STATUS_BUFFER_OVERFLOW)
				ASSERT(length == ioStatusBlock.Information);

			if(!(queryDirectoryStatus == STATUS_SUCCESS || queryDirectoryStatus == STATUS_BUFFER_OVERFLOW))
			{
				ASSERT(ioStatusBlock.Information == 0);
				ioStatusBlock.Information = 0;
			}

			NdfsWinxpReplytHeader->IrpTag4			 = NdfsWinxpRequestHeader->IrpTag4;
			NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
			NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

			//
			//	[64bit issue]
			//	We assume Information value of DIRECTORY_CONTROL operation will be
			//	less than 32bit.
			//

			ASSERT(ioStatusBlock.Information <= 0xffffffff);

			NdfsWinxpReplytHeader->Status4	  = HTONL(queryDirectoryStatus);
			NdfsWinxpReplytHeader->Information32 = HTONL((UINT32)ioStatusBlock.Information);

			if(queryDirectoryStatus == STATUS_SUCCESS || queryDirectoryStatus == STATUS_BUFFER_OVERFLOW)
			{
				if(ioStatusBlock.Information)
					*replyDataSize = CalculateSafeLength(
										fileInformationClass,
										length,
										(UINT32)ioStatusBlock.Information,
										fileInformation
										);
			}else
			{
				*replyDataSize = 0;
			}

			//*replyDataSize = NdfsWinxpRequestHeader->QueryDirectory.Length <= ioStatusBlock.Information 
			//					? NdfsWinxpRequestHeader->QueryDirectory.Length : ioStatusBlock.Information;
			//*replyDataSize = ioStatusBlock.Information;
		
			status = STATUS_SUCCESS;

			//
			//	Free the name buffer
			//

			ExFreePool(fileNameBuffer);
		} else
		{
			ASSERT(LFS_BUG);
			status = STATUS_UNSUCCESSFUL;
		}

		break;
	}
	
	case IRP_MJ_FILE_SYSTEM_CONTROL: // 0x0D
	{
		NTSTATUS		fileSystemControlStatus;

		UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE		openFile;

		IO_STATUS_BLOCK	ioStatusBlock;
		ULONG			fsControlCode;
		PVOID			inputBuffer;
		ULONG			inputBufferLength;
		PVOID			outputBuffer;
		ULONG			outputBufferLength;
 #if 0
		openFile = PrimarySession_FindOpenFile(
								PrimarySession,
								openFileId
								);
#else
		openFile = PrimarySession_FindOrReopenOpenFile(
			PrimarySession,
			openFileId);

		if (openFile == NULL) {

			status = STATUS_UNSUCCESSFUL;
			break;
		}

#endif
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(NdfsWinxpRequestHeader->FileSystemControl.OutputBufferLength <= MAX_PRIMARY_SESSION_SEND_BUFFER);


		fsControlCode		= NdfsWinxpRequestHeader->FileSystemControl.FsControlCode;
		inputBufferLength	= NdfsWinxpRequestHeader->FileSystemControl.InputBufferLength;

		if(fsControlCode == FSCTL_MOVE_FILE)		// 29
		{
			MOVE_FILE_DATA	moveFileData;	
			POPEN_FILE		moveFile;
			
			ASSERT(sizeof(FILE_RENAME_INFORMATION) - sizeof(WCHAR) + NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength <= NdfsWinxpRequestHeader->SetFile.Length);
						
			moveFile = PrimarySession_FindOpenFile(
									PrimarySession,
									NdfsWinxpRequestHeader->FileSystemControl.FscMoveFileData.FileHandle
									);

			ASSERT(moveFile);
			ASSERT(moveFile->AlreadyClosed == FALSE);

			moveFileData.FileHandle				= moveFile->FileHandle;
			moveFileData.StartingVcn.QuadPart	= NdfsWinxpRequestHeader->FileSystemControl.FscMoveFileData.StartingVcn;
			moveFileData.StartingLcn.QuadPart	= NdfsWinxpRequestHeader->FileSystemControl.FscMoveFileData.StartingLcn;
			moveFileData.ClusterCount			= NdfsWinxpRequestHeader->FileSystemControl.FscMoveFileData.ClusterCount;

			inputBuffer = &moveFileData;
		} 
		else if(fsControlCode == FSCTL_MARK_HANDLE)	// 63
		{
			MARK_HANDLE_INFO	markHandleInfo;
			POPEN_FILE			markFile;

			markFile = PrimarySession_FindOpenFile(
									PrimarySession,
									NdfsWinxpRequestHeader->FileSystemControl.FscMarkHandleInfo.VolumeHandle
									);
			ASSERT(markFile);
			ASSERT(markFile->AlreadyClosed == FALSE);

			markHandleInfo.UsnSourceInfo = NdfsWinxpRequestHeader->FileSystemControl.FscMarkHandleInfo.UsnSourceInfo;
			markHandleInfo.VolumeHandle	 = markFile->FileHandle;
			markHandleInfo.HandleInfo    = NdfsWinxpRequestHeader->FileSystemControl.FscMarkHandleInfo.HandleInfo;

			inputBuffer = &markHandleInfo;
		}
		else
		{
			inputBuffer	= ndfsRequestData;
		}

		outputBuffer		= (UINT8 *)(NdfsWinxpReplytHeader+1);
		outputBufferLength	= NdfsWinxpRequestHeader->FileSystemControl.OutputBufferLength;

		RtlZeroMemory(
			&ioStatusBlock,
			sizeof(ioStatusBlock)
			);

		fileSystemControlStatus = ZwFsControlFile(
									openFile->FileHandle,
									NULL,
									NULL,
									NULL,
									&ioStatusBlock,
									fsControlCode,
									inputBuffer,
									inputBufferLength,
									outputBuffer,
									outputBufferLength
									);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
						("DispatchWinXpRequest: IRP_MJ_FILE_SYSTEM_CONTROL: openFileId = %x, function = %d, fileSystemControlStatus = %x, ioStatusBlock.Status = %x, ioStatusBlock.Information = %d\n",
										openFileId, (fsControlCode & 0x00003FFC) >> 2, fileSystemControlStatus, ioStatusBlock.Status, ioStatusBlock.Information));
		
		if(NT_SUCCESS(fileSystemControlStatus))
		{
			ASSERT(fileSystemControlStatus == STATUS_SUCCESS);	
			ASSERT(fileSystemControlStatus == ioStatusBlock.Status);
		}

		if(fileSystemControlStatus == STATUS_BUFFER_OVERFLOW)
			ASSERT(ioStatusBlock.Information == inputBufferLength || ioStatusBlock.Information == outputBufferLength);
		
		if(!(fileSystemControlStatus == STATUS_SUCCESS || fileSystemControlStatus == STATUS_BUFFER_OVERFLOW))
		{
			ioStatusBlock.Information = 0;
			ASSERT(ioStatusBlock.Information == 0);
		}

		NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		//
		//	[64bit issue]
		//	We assume Information value of FILESYSTEM_CONTROL operation will be
		//	less than 32bit.
		//

		ASSERT(ioStatusBlock.Information <= 0xffffffff);

		NdfsWinxpReplytHeader->Status4	  = HTONL(fileSystemControlStatus);
		NdfsWinxpReplytHeader->Information32 = HTONL((UINT32)ioStatusBlock.Information);

		if(outputBufferLength && NTOHL(NdfsWinxpReplytHeader->Information32))
		{
			*replyDataSize = (UINT32)ioStatusBlock.Information;
			//*replyDataSize = NdfsWinxpRequestHeader->FileSystemControl.OutputBufferLength <= (UINT32)ioStatusBlock.Information 
			//					? NdfsWinxpRequestHeader->FileSystemControl.OutputBufferLength : (UINT32)ioStatusBlock.Information;
		}
		else
			*replyDataSize = 0;

		if (outputBufferLength < *replyDataSize)
			NDAS_ASSERT( FALSE );
		
		status = STATUS_SUCCESS;

		break;
	}

    case IRP_MJ_DEVICE_CONTROL: // 0x0E
	//	case IRP_MJ_INTERNAL_DEVICE_CONTROL:  // 0x0F 
	{
		NTSTATUS		deviceControlStatus;

		UINT64			openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE		openFile;

		IO_STATUS_BLOCK	ioStatusBlock;
		ULONG			ioControlCode;
		PVOID			inputBuffer;
		ULONG			inputBufferLength;
		PVOID			outputBuffer;
		ULONG			outputBufferLength;
 		
#if 0
		openFile = PrimarySession_FindOpenFile(
								PrimarySession,
								openFileId
								);
#else
		openFile = PrimarySession_FindOrReopenOpenFile(
			PrimarySession,
			openFileId);

		if (openFile == NULL) {

			status = STATUS_UNSUCCESSFUL;
			break;
		}

#endif
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (UINT32)openFile);

		//ASSERT(NdfsWinxpRequestHeader->DeviceIoControl.OutputBufferLength <= PrimarySession->PrimaryMaxBufferSize);

		ioControlCode		= NdfsWinxpRequestHeader->DeviceIoControl.IoControlCode;
		inputBuffer			= ndfsRequestData;
		inputBufferLength	= NdfsWinxpRequestHeader->DeviceIoControl.InputBufferLength;
		outputBuffer		= (UINT8 *)(NdfsWinxpReplytHeader+1);
		outputBufferLength	= NdfsWinxpRequestHeader->DeviceIoControl.OutputBufferLength;


		RtlZeroMemory(
			&ioStatusBlock,
			sizeof(ioStatusBlock)
			);

		deviceControlStatus = ZwDeviceIoControlFile(
								openFile->FileHandle,
								NULL,
								NULL,
								NULL,
								&ioStatusBlock,
								ioControlCode,
								inputBuffer,
								inputBufferLength,
								outputBuffer,
								outputBufferLength
								);
		
		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
						("DispatchWinXpRequest: IRP_MJ_DEVICE_CONTROL: "
						"CtrlCode:%x openFileId = %X, deviceControlStatus = %X, "
						"ioStatusBlock.Status = %X, ioStatusBlock.Information = %d\n",
						ioControlCode, openFileId, deviceControlStatus, ioStatusBlock.Status, ioStatusBlock.Information));

		if(NT_SUCCESS(deviceControlStatus))
		{
			ASSERT(deviceControlStatus == STATUS_SUCCESS);		
			ASSERT(deviceControlStatus == ioStatusBlock.Status);
		}

		if(deviceControlStatus == STATUS_BUFFER_OVERFLOW)
			ASSERT(ioStatusBlock.Information == inputBufferLength || ioStatusBlock.Information == outputBufferLength);

		if(!(deviceControlStatus == STATUS_SUCCESS || deviceControlStatus == STATUS_BUFFER_OVERFLOW))
		{
			ioStatusBlock.Information = 0;
			ASSERT(ioStatusBlock.Information == 0);
		}

		NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		//
		//	[64bit issue]
		//	We assume Information value of DEVICE_CONTROL operation will be
		//	less than 32bit.
		//

		ASSERT(ioStatusBlock.Information <= 0xffffffff);

		NdfsWinxpReplytHeader->Status4	  = HTONL(deviceControlStatus);
		NdfsWinxpReplytHeader->Information32 = HTONL((UINT32)ioStatusBlock.Information);

		if(ioStatusBlock.Information)
			ASSERT(outputBufferLength);

		if(outputBufferLength && HTONL(NdfsWinxpReplytHeader->Information32))
		{
			*replyDataSize = (UINT32)ioStatusBlock.Information;
			//*replyDataSize = NdfsWinxpRequestHeader->DeviceIoControl.OutputBufferLength <= ioStatusBlock.Information 
			//					? NdfsWinxpRequestHeader->DeviceIoControl.OutputBufferLength : ioStatusBlock.Information;
		} 
		else
			*replyDataSize = 0;
		
		status = STATUS_SUCCESS;

		break;
	}
	
	case IRP_MJ_LOCK_CONTROL: // 0x11
	{
		NTSTATUS			lockControlStatus;

		UINT64				openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE			openFile;
		
		IO_STATUS_BLOCK		ioStatusBlock;
		LARGE_INTEGER		byteOffset;
		LARGE_INTEGER		length;
		ULONG				key;
		BOOLEAN				failImmediately;
		BOOLEAN				exclusiveLock;
		
#if 0
		openFile = PrimarySession_FindOpenFile(
								PrimarySession,
								openFileId
								);
#else
		openFile = PrimarySession_FindOrReopenOpenFile(
			PrimarySession,
			openFileId);

		if (openFile == NULL) {

			status = STATUS_UNSUCCESSFUL;
			break;
		}

#endif
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (UINT32)openFile);
		//ASSERT(NdfsWinxpRequestHeader->LockControl.Length <= MAX_PRIMARY_SESSION_SEND_BUFFER);

		byteOffset.QuadPart	= NdfsWinxpRequestHeader->LockControl.ByteOffset;
		length.QuadPart		= NdfsWinxpRequestHeader->LockControl.Length;
		key					= NdfsWinxpRequestHeader->LockControl.Key;

        failImmediately		= BooleanFlagOn(NdfsWinxpRequestHeader->IrpSpFlags, SL_FAIL_IMMEDIATELY);
		exclusiveLock		= BooleanFlagOn(NdfsWinxpRequestHeader->IrpSpFlags, SL_EXCLUSIVE_LOCK);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE, ( "LFS: IRP_MJ_LOCK_CONTROL: Length:%I64d Key:%08lx Offset:%I64d key:%u Ime:%d Ex:%d\n",
						length.QuadPart,
						key,
						byteOffset.QuadPart,
						key,
						failImmediately,
						exclusiveLock
					)) ;

		RtlZeroMemory(
			&ioStatusBlock,
			sizeof(ioStatusBlock)
			);
		
		if(NdfsWinxpRequestHeader->IrpMinorFunction == IRP_MN_LOCK)
		{
#if 1
			lockControlStatus = NtLockFile(
										openFile->FileHandle,
										NULL,
										NULL,
										NULL,
										&ioStatusBlock,
										&byteOffset,
										&length,
										key,
										failImmediately,
										exclusiveLock
										);
#else
			ioStatusBlock.Information = (ULONG_PTR)length.QuadPart;
			lockControlStatus = STATUS_SUCCESS ;
#endif
		}
		else if(NdfsWinxpRequestHeader->IrpMinorFunction == IRP_MN_UNLOCK_SINGLE)
		{
#if 1
			lockControlStatus = NtUnlockFile(
										openFile->FileHandle,
										&ioStatusBlock,
										&byteOffset,
										&length,
										key
										);		
#else
			ioStatusBlock.Information = (ULONG_PTR)length.QuadPart;
			lockControlStatus = STATUS_SUCCESS ;
#endif
		}
		else if(NdfsWinxpRequestHeader->IrpMinorFunction == IRP_MN_UNLOCK_ALL)
		{
#if	__NDAS_FS_HCT_TEST_MODE__
			ASSERT(LFS_REQUIRED);
#endif
			lockControlStatus = STATUS_SUCCESS;
			//lockControlStatus = STATUS_NOT_IMPLEMENTED;
		}
		else
		{
			ASSERT(LFS_BUG);
			lockControlStatus = STATUS_NOT_IMPLEMENTED;
		}

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("DispatchWinXpRequest: NtLockFile: openFileId = %X, LockControlStatus = %X, ioStatusBlock.Status = %X, ioStatusBlock.Information = %d\n",
						openFileId, lockControlStatus, ioStatusBlock.Status, ioStatusBlock.Information));

		
		NdfsWinxpReplytHeader->IrpTag4			 = NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status4	  = HTONL(lockControlStatus);

		if(NT_SUCCESS(lockControlStatus))
		{
			ASSERT(lockControlStatus == ioStatusBlock.Status);

			//
			//	[64bit issue]
			//	We assume Information value of LOCK_CONTROL operation will be
			//	less than 32bit.
			//

			//ASSERT(ioStatusBlock.Information <= 0xffffffff);

			NdfsWinxpReplytHeader->Information32 = HTONL((UINT32)ioStatusBlock.Information);
		}
		else
			NdfsWinxpReplytHeader->Information32 = 0;

		*replyDataSize = 0;
		
		status = STATUS_SUCCESS;

		break;
	}
	case IRP_MJ_CLEANUP: // 0x12
	{
		UINT64				openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE			openFile;

		//PIRP				irp;
		//PIO_STACK_LOCATION	irpSp;
		//PDEVICE_OBJECT	deviceObject;
		//PFAST_IO_DISPATCH	fastIoDispatch;
		//NTSTATUS			status;
		//KEVENT			event;
		//KIRQL				irql;

		
		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);

		if (openFile &&
			PrimarySession->SessionContext.NdfsMinorVersion == NDFS_PROTOCOL_MINOR_0 && 
			BooleanFlagOn(NdfsWinxpRequestHeader->IrpFlags4, HTONL(IRP_CLOSE_OPERATION))) {

			openFile->CleanUp = TRUE;
		}
		
		NdfsWinxpReplytHeader->IrpTag4			 = NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status4	  = HTONL(STATUS_SUCCESS);
		NdfsWinxpReplytHeader->Information32 = 0;

		*replyDataSize = 0;
		
		status = STATUS_SUCCESS;

		break;
	}
    case IRP_MJ_QUERY_SECURITY:
	{
		NTSTATUS				querySecurityStatus;
		UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE				openFile;
		
		ULONG					length;
		SECURITY_INFORMATION	securityInformation;
		PSECURITY_DESCRIPTOR	securityDescriptor;
		ULONG					returnedLength = 0;

#if 0
		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
#else
		openFile = PrimarySession_FindOrReopenOpenFile(
			PrimarySession,
			openFileId);

		if (openFile == NULL) {

			status = STATUS_UNSUCCESSFUL;
			break;
		}

#endif
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (UINT32)openFile);
		ASSERT(NdfsWinxpRequestHeader->QuerySecurity.Length <= PrimarySession->SessionContext.SecondaryMaxDataSize);

		length					= NdfsWinxpRequestHeader->QuerySecurity.Length;
		securityInformation		= NdfsWinxpRequestHeader->QuerySecurity.SecurityInformation;

		securityDescriptor		= (PSECURITY_DESCRIPTOR)(NdfsWinxpReplytHeader+1);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE, 
			("LFS: DispatchWinXpRequest: IRP_MJ_QUERY_SECURITY: OutputBufferLength:%d\n", length));

		querySecurityStatus = ZwQuerySecurityObject(
										openFile->FileHandle,
										securityInformation,
										securityDescriptor,
										length,
										&returnedLength
									);

		NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction	= NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction	= NdfsWinxpRequestHeader->IrpMinorFunction;

		if(NT_SUCCESS(querySecurityStatus))
			ASSERT(querySecurityStatus == STATUS_SUCCESS);		

		if( querySecurityStatus == STATUS_SUCCESS ||
			querySecurityStatus == STATUS_BUFFER_OVERFLOW) 
		{
			NdfsWinxpReplytHeader->Information32 = HTONL(returnedLength);
			*replyDataSize = returnedLength;

		} else if(querySecurityStatus == STATUS_BUFFER_TOO_SMALL) 
		{
			//
			//	Zw routines translate STATUS_BUFFER_OVERFLOW into STATUS_BUFFER_TOO_SMALL
			//	We have to get the status code back here.
			//
			querySecurityStatus = STATUS_BUFFER_OVERFLOW ;
			NdfsWinxpReplytHeader->Information32 = HTONL(returnedLength); //NdfsWinxpRequestHeader->QuerySecurity.Length;
			*replyDataSize = 0; // NdfsWinxpRequestHeader->QuerySecurity.Length;
		} else {
			NdfsWinxpReplytHeader->Information32 = 0;
			*replyDataSize = 0;
		}

		//ASSERT(NdfsWinxpRequestHeader->QuerySecurity.Length >= returnedLength);
		NdfsWinxpReplytHeader->Status4	  = HTONL(querySecurityStatus);

		status = STATUS_SUCCESS;

		break;
	}

    case IRP_MJ_SET_SECURITY:
	{
		NTSTATUS				setSecurityStatus;

		UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE				openFile;
		
	    PSECURITY_DESCRIPTOR	securityDescriptor;
		ULONG					length;
		SECURITY_INFORMATION	securityInformation;

#if 0
		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
#else
		openFile = PrimarySession_FindOrReopenOpenFile(
			PrimarySession,
			openFileId);

		if (openFile == NULL) {

			status = STATUS_UNSUCCESSFUL;
			break;
		}

#endif
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (UINT32)openFile);

		securityInformation = NdfsWinxpRequestHeader->SetSecurity.SecurityInformation;
		length				= NdfsWinxpRequestHeader->SetSecurity.Length;
		securityDescriptor	= (PSECURITY_DESCRIPTOR)ndfsRequestData ;

		setSecurityStatus = ZwSetSecurityObject(
										openFile->FileHandle,
										securityInformation,
										securityDescriptor
									);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("DispatchWinXpRequest: ZwSetSecurityObject: fileHandle = %p, securityInformation = %X, setSecurityStatus = %X\n",
						openFile->FileHandle, securityInformation, setSecurityStatus));

		if(NT_SUCCESS(setSecurityStatus))
			ASSERT(setSecurityStatus == STATUS_SUCCESS);		

		NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status4	  = HTONL(setSecurityStatus);
		NdfsWinxpReplytHeader->Information32 = HTONL(length);

		*replyDataSize = 0;

		status = STATUS_SUCCESS;

		break;
	}

	case IRP_MJ_QUERY_EA:
	{
		UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE				openFile;
		
		NTSTATUS				queryEaStatus;
		IO_STATUS_BLOCK			ioStatusBlock ;
		PVOID					buffer;
		ULONG					length;
		BOOLEAN					returnSingleEntry;
		PVOID					eaList;
		ULONG					eaListLength;
		ULONG					eaIndex ;
		BOOLEAN					restartScan ;

		BOOLEAN					indexSpecified;

#if 0
		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
#else
		openFile = PrimarySession_FindOrReopenOpenFile(
			PrimarySession,
			openFileId);

		if (openFile == NULL) {

			status = STATUS_UNSUCCESSFUL;
			break;
		}

#endif
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (UINT32)openFile);

		buffer				= (UINT8 *)(NdfsWinxpReplytHeader+1);
		length				= NdfsWinxpRequestHeader->QueryEa.Length;
		returnSingleEntry	= (NdfsWinxpRequestHeader->IrpSpFlags & SL_RETURN_SINGLE_ENTRY) ? TRUE : FALSE;
		eaList				= ndfsRequestData;
		eaListLength		= NdfsWinxpRequestHeader->QueryEa.EaListLength;
		if(eaListLength == 0)
			eaList = NULL;
		eaIndex				= NdfsWinxpRequestHeader->QueryEa.EaIndex;
		restartScan			= (NdfsWinxpRequestHeader->IrpSpFlags & SL_RESTART_SCAN) ? TRUE : FALSE;

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE, 
			("LFS: DispatchWinXpRequest: IRP_MJ_QUERY_EA: length:%d\n", length));

		indexSpecified = BooleanFlagOn(NdfsWinxpRequestHeader->IrpSpFlags, SL_INDEX_SPECIFIED);

		queryEaStatus = NtQueryEaFile(
								openFile->FileHandle,
								&ioStatusBlock,
								buffer,
								length,
								returnSingleEntry,
								eaList,
								eaListLength,
								indexSpecified ? &eaIndex : NULL,
								restartScan
								);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
					("DispatchWinXpRequest: ZwQueryEaFile: fileHandle = %p, ioStatusBlock->Information = %d, queryEaStatus = %x\n",
						openFile->FileHandle, ioStatusBlock.Information, queryEaStatus));

		NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		if(NT_SUCCESS(queryEaStatus))
			ASSERT(queryEaStatus == STATUS_SUCCESS);		

		//
		//	[64bit issue]
		//	We assume Information value of QUERY_EA operation will be
		//	less than 32bit.
		//

		ASSERT(ioStatusBlock.Information <= 0xffffffff);

		if( queryEaStatus == STATUS_SUCCESS ||
			queryEaStatus == STATUS_BUFFER_OVERFLOW) 
		{
			NdfsWinxpReplytHeader->Information32 = HTONL((UINT32)ioStatusBlock.Information);
		
			if(ioStatusBlock.Information != 0)
			{
				PFILE_FULL_EA_INFORMATION	fileFullEa = (PFILE_FULL_EA_INFORMATION)buffer;
		
				*replyDataSize = 0;
			
				while(fileFullEa->NextEntryOffset)
				{
					*replyDataSize += fileFullEa->NextEntryOffset;
					fileFullEa = (PFILE_FULL_EA_INFORMATION)((UINT8 *)fileFullEa + fileFullEa->NextEntryOffset);
				}

				*replyDataSize += sizeof(FILE_FULL_EA_INFORMATION) - sizeof(CHAR) + fileFullEa->EaNameLength + fileFullEa->EaValueLength;

				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
								("DispatchWinXpRequest, IRP_MJ_QUERY_EA: Ea is set QueryEa.Length = %d, inputBufferLength = %d\n",
										NdfsWinxpRequestHeader->QueryEa.Length, *replyDataSize));
				*replyDataSize = ((*replyDataSize < length) ? *replyDataSize : length);
			}
		} 
		else if(queryEaStatus == STATUS_BUFFER_TOO_SMALL) 
		{
			//
			//	Zw routines translate STATUS_BUFFER_OVERFLOW into STATUS_BUFFER_TOO_SMALL
			//	We have to get the status code back here.
			//
			queryEaStatus = STATUS_BUFFER_OVERFLOW ;
			NdfsWinxpReplytHeader->Information32 = HTONL((UINT32)ioStatusBlock.Information); //NdfsWinxpRequestHeader->QuerySecurity.Length;
			*replyDataSize = 0; // NdfsWinxpRequestHeader->QuerySecurity.Length;
		} else {
			NdfsWinxpReplytHeader->Information32 = 0;
			*replyDataSize = 0;
		}

		//ASSERT(NdfsWinxpRequestHeader->QuerySecurity.Length >= returnedLength);
		NdfsWinxpReplytHeader->Status4	  = HTONL(queryEaStatus);

		status = STATUS_SUCCESS;

		break;
	}

    case IRP_MJ_SET_EA:
	{
		NTSTATUS				setEaStatus;
		IO_STATUS_BLOCK			ioStatusBlock;

		UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE				openFile;
		ULONG					length;

#if 0
		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
#else
		openFile = PrimarySession_FindOrReopenOpenFile(
			PrimarySession,
			openFileId);

		if (openFile == NULL) {

			status = STATUS_UNSUCCESSFUL;
			break;
		}

#endif
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (UINT32)openFile);

		length = NdfsWinxpRequestHeader->SetEa.Length;

		setEaStatus = NtSetEaFile(
							openFile->FileHandle,
							&ioStatusBlock,
							ndfsRequestData,
							length
							);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
					("DispatchWinXpRequest: ZwSetEaFile: fileHandle = %p, ioStatusBlock->Information = %d, setEaStatus = %x\n",
						openFile->FileHandle, ioStatusBlock.Information, setEaStatus));

		if(NT_SUCCESS(setEaStatus))
			ASSERT(setEaStatus == STATUS_SUCCESS);		

		NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		//
		//	[64bit issue]
		//	We assume Information value of SET_EA operation will be
		//	less than 32bit.
		//

		ASSERT(ioStatusBlock.Information <= 0xffffffff);

		NdfsWinxpReplytHeader->Status4	  = HTONL(setEaStatus);
		NdfsWinxpReplytHeader->Information32 = HTONL((UINT32)ioStatusBlock.Information);

		*replyDataSize = 0;

		status = STATUS_SUCCESS;

		break;
	}

	case IRP_MJ_QUERY_QUOTA:
	{
		NTSTATUS				queryQuotaStatus;
		IO_STATUS_BLOCK			ioStatusBlock ;
		UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE				openFile;
		
		PVOID					outputBuffer ;
		ULONG					outputBufferLength;
		PVOID					inputBuffer ;
		ULONG					inputBufferLength ;
		PVOID					startSid ;
		BOOLEAN					restartScan ;
		BOOLEAN					returnSingleEntry ;
#if 0
		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
#else
		openFile = PrimarySession_FindOrReopenOpenFile(
			PrimarySession,
			openFileId);

		if (openFile == NULL) {

			status = STATUS_UNSUCCESSFUL;
			break;
		}

#endif
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (UINT32)openFile);

		outputBufferLength		= NdfsWinxpRequestHeader->QueryQuota.Length;
		if(outputBufferLength)
			outputBuffer			= (UINT8 *)(NdfsWinxpReplytHeader+1);
		else
			outputBuffer			= NULL ;

		inputBufferLength		= NdfsWinxpRequestHeader->QueryQuota.InputLength ;
		if(inputBufferLength)
			inputBuffer				= ndfsRequestData;
		else
			inputBuffer				= NULL;

		if(NdfsWinxpRequestHeader->QueryQuota.StartSidOffset)
			startSid			= (PCHAR)ndfsRequestData + NdfsWinxpRequestHeader->QueryQuota.StartSidOffset;
		else
			startSid			= NULL;

		restartScan				= (NdfsWinxpRequestHeader->IrpSpFlags & SL_RESTART_SCAN) ? TRUE : FALSE;
		returnSingleEntry		= (NdfsWinxpRequestHeader->IrpSpFlags & SL_RETURN_SINGLE_ENTRY) ? TRUE : FALSE;

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE, 
			("LFS: DispatchWinXpRequest: IRP_MJ_QUERY_QUOTA: OutputBufferLength:%d\n", outputBufferLength));

		queryQuotaStatus = NtQueryQuotaInformationFile(
										openFile->FileHandle,
										&ioStatusBlock,
										outputBuffer,
										outputBufferLength,
										returnSingleEntry,
										inputBuffer,
										inputBufferLength,
										startSid,
										restartScan
									);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					("DispatchWinXpRequest: NtQueryQuotaInformationFile: fileHandle = %p, ioStatusBlock->Information = %X, queryQuotaStatus = %X\n",
						openFile->FileHandle, ioStatusBlock.Information, queryQuotaStatus));

		NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		if(NT_SUCCESS(queryQuotaStatus))
			ASSERT(queryQuotaStatus == STATUS_SUCCESS);		

		//
		//	[64bit issue]
		//	We assume Information value of QUERY_QUOTA operation will be
		//	less than 32bit.
		//

		ASSERT(ioStatusBlock.Information <= 0xffffffff);


		if( queryQuotaStatus == STATUS_SUCCESS ||
			queryQuotaStatus == STATUS_BUFFER_OVERFLOW) 
		{
			NdfsWinxpReplytHeader->Information32 = HTONL((UINT32)ioStatusBlock.Information);
			*replyDataSize = (UINT32)ioStatusBlock.Information;

		} else if(queryQuotaStatus == STATUS_BUFFER_TOO_SMALL) 
		{
			//
			//	Zw routines translate STATUS_BUFFER_OVERFLOW into STATUS_BUFFER_TOO_SMALL
			//	We have to get the status code back here.
			//
			queryQuotaStatus = STATUS_BUFFER_OVERFLOW ;
			NdfsWinxpReplytHeader->Information32 = HTONL((UINT32)ioStatusBlock.Information); //NdfsWinxpRequestHeader->QuerySecurity.Length;
			*replyDataSize = 0; // NdfsWinxpRequestHeader->QuerySecurity.Length;
		} else {
			NdfsWinxpReplytHeader->Information32 = 0;
			*replyDataSize = 0;
		}

		//ASSERT(NdfsWinxpRequestHeader->QuerySecurity.Length >= returnedLength);
		NdfsWinxpReplytHeader->Status4	  = HTONL(queryQuotaStatus);

		status = STATUS_SUCCESS;

		break ;
	}
	case IRP_MJ_SET_QUOTA:
	{
		NTSTATUS				setQuotaStatus;
		IO_STATUS_BLOCK			ioStatusBlock;

		UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE				openFile;
		ULONG					length;

#if 0
		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
#else
		openFile = PrimarySession_FindOrReopenOpenFile(
			PrimarySession,
			openFileId);

		if (openFile == NULL) {

			status = STATUS_UNSUCCESSFUL;
			break;
		}

#endif
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (UINT32)openFile);

		length				= NdfsWinxpRequestHeader->SetQuota.Length;

		setQuotaStatus = NtSetQuotaInformationFile(
									openFile->FileHandle,
									&ioStatusBlock,
									ndfsRequestData,
									length
									);

		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("DispatchWinXpRequest: NtSetQuotaInformationFile: fileHandle = %p, ioStatusBlock.Information = %X, setQuotaStatus = %X\n",
						openFile->FileHandle, ioStatusBlock.Information, setQuotaStatus));

		if(NT_SUCCESS(setQuotaStatus))
			ASSERT(setQuotaStatus == STATUS_SUCCESS);		

		NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction = NdfsWinxpRequestHeader->IrpMinorFunction;

		//
		//	[64bit issue]
		//	We assume Information value of SET_QUOTA operation will be
		//	less than 32bit.
		//

		ASSERT(ioStatusBlock.Information <= 0xffffffff);


		NdfsWinxpReplytHeader->Status4	  = HTONL(setQuotaStatus);
		NdfsWinxpReplytHeader->Information32 = HTONL((UINT32)ioStatusBlock.Information);

		*replyDataSize = 0;

		status = STATUS_SUCCESS;

		break;
	}
	default:

		ASSERT(LFS_BUG);
		status = STATUS_UNSUCCESSFUL;

		break;
	}

	if(!(
			NTOHL(NdfsWinxpReplytHeader->Status4) == STATUS_SUCCESS
		||  NTOHL(NdfsWinxpReplytHeader->Status4) == STATUS_BUFFER_OVERFLOW
//		||  NTOHL(NdfsWinxpReplytHeader->Status4) == STATUS_END_OF_FILE
		))
		ASSERT(*replyDataSize == 0);

//	ASSERT(NTOHL(NdfsWinxpReplytHeader->Status4) != STATUS_INVALID_HANDLE); // this can be happen...
	if (NTOHL(NdfsWinxpReplytHeader->Status4) == STATUS_INVALID_HANDLE) {
		SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
					("DispatchWinXpRequest: ReplyHeader->Statu == STATUS_INVALID_HANDLE\n"));
	}
	ASSERT(NTOHL(NdfsWinxpReplytHeader->Status4) != STATUS_PENDING);

	return status;	
}


#define SAFELEN_ALIGNMENT_ADD		7 // for 4 bytes(long) alignment.


ULONG
CalculateSafeLength (
	FILE_INFORMATION_CLASS	fileInformationClass,
	ULONG					requestLength,
	ULONG					returnedLength,
	PCHAR					Buffer
	) 
{
	ULONG	offset = 0;
	ULONG	safeAddSize = 0;


	switch(fileInformationClass) 
	{
	case FileDirectoryInformation: 
	{
		PFILE_DIRECTORY_INFORMATION	fileDirectoryInformation;

		while(offset < returnedLength) 
		{
			fileDirectoryInformation = (PFILE_DIRECTORY_INFORMATION)(Buffer + offset);
			safeAddSize += SAFELEN_ALIGNMENT_ADD;

			if(fileDirectoryInformation->NextEntryOffset == 0)
				break ;

			offset += fileDirectoryInformation->NextEntryOffset;
		}
		break ;
	}
	case FileFullDirectoryInformation: 
	{
		PFILE_FULL_DIR_INFORMATION	fileFullDirInformation;

		while(offset < returnedLength) 
		{
			fileFullDirInformation = (PFILE_FULL_DIR_INFORMATION)(Buffer + offset);
			safeAddSize += SAFELEN_ALIGNMENT_ADD;

			if(fileFullDirInformation->NextEntryOffset == 0)
				break ;

			offset += fileFullDirInformation->NextEntryOffset;
		}
		break ;
	}
	case FileBothDirectoryInformation: 
	{
		PFILE_BOTH_DIR_INFORMATION	fileBothDirInformation;

		while(offset < returnedLength) 
		{
			fileBothDirInformation = (PFILE_BOTH_DIR_INFORMATION)(Buffer + offset) ;

			safeAddSize += SAFELEN_ALIGNMENT_ADD ;

			if(!fileBothDirInformation->NextEntryOffset)
				break ;

			offset += fileBothDirInformation->NextEntryOffset;
		}
		break ;
	}
	case FileIdBothDirectoryInformation: 
	{
		PFILE_ID_BOTH_DIR_INFORMATION	fileIdBothDirInformation;

		while(offset < returnedLength) 
		{
			fileIdBothDirInformation = (PFILE_ID_BOTH_DIR_INFORMATION)(Buffer + offset) ;

			safeAddSize += SAFELEN_ALIGNMENT_ADD ;

			if(!fileIdBothDirInformation->NextEntryOffset)
				break ;

			offset += fileIdBothDirInformation->NextEntryOffset;
		}
		break ;
	}
	case FileIdFullDirectoryInformation: 
	{
		PFILE_ID_FULL_DIR_INFORMATION	fileIdFullDirInformation;

		while(offset < returnedLength) 
		{
			fileIdFullDirInformation = (PFILE_ID_FULL_DIR_INFORMATION)(Buffer + offset) ;

			safeAddSize += SAFELEN_ALIGNMENT_ADD ;

			if(!fileIdFullDirInformation->NextEntryOffset)
				break ;

			offset += fileIdFullDirInformation->NextEntryOffset;
		}
		break;
	}
	case FileNamesInformation: 
	{
		PFILE_NAMES_INFORMATION	fileNamesInformation;

		while(offset < returnedLength) 
		{
			fileNamesInformation = (PFILE_NAMES_INFORMATION)(Buffer + offset);

			safeAddSize += SAFELEN_ALIGNMENT_ADD ;

			if(!fileNamesInformation->NextEntryOffset)
				break ;

			offset += fileNamesInformation->NextEntryOffset;
		}
		break;
	}
	case FileStreamInformation: 
	{
		PFILE_STREAM_INFORMATION	fileStreamInformation;

		while(offset < returnedLength) 
		{
			fileStreamInformation = (PFILE_STREAM_INFORMATION)(Buffer + offset);

			safeAddSize += SAFELEN_ALIGNMENT_ADD ;

			if(!fileStreamInformation->NextEntryOffset)
				break ;

			offset += fileStreamInformation->NextEntryOffset;
		}
		break;
	}
	case FileFullEaInformation: 
	{
		PFILE_FULL_EA_INFORMATION	fileFullEaInformation;

		while(offset < returnedLength) 
		{
			fileFullEaInformation = (PFILE_FULL_EA_INFORMATION)(Buffer + offset);

			safeAddSize += SAFELEN_ALIGNMENT_ADD ;

			if(!fileFullEaInformation->NextEntryOffset)
				break ;

			offset += fileFullEaInformation->NextEntryOffset;
		}
		break;
	}
	case FileQuotaInformation: 
	{
		PFILE_QUOTA_INFORMATION	fileQuotaInformation;

		while(offset < returnedLength) 
		{
			fileQuotaInformation = (PFILE_QUOTA_INFORMATION)(Buffer + offset);

			safeAddSize += SAFELEN_ALIGNMENT_ADD ;

			if(!fileQuotaInformation->NextEntryOffset)
				break ;

			offset += fileQuotaInformation->NextEntryOffset;
		}
		break ;
	}

	default:
		safeAddSize = 0 ;
	}

	return ((returnedLength + safeAddSize) < requestLength) ? (returnedLength+safeAddSize) : requestLength;
}


POPEN_FILE
PrimarySession_AllocateOpenFile (
	IN	PPRIMARY_SESSION	PrimarySession,
	IN  HANDLE				FileHandle,
	IN  PFILE_OBJECT		FileObject,
	IN	PUNICODE_STRING		FullFileName
	) 
{
	POPEN_FILE	openFile;

	openFile = ExAllocatePoolWithTag( NonPagedPool, sizeof(OPEN_FILE), OPEN_FILE_TAG );
	
	if (openFile == NULL) {

		NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
		return NULL;
	}

	RtlZeroMemory( openFile, sizeof(OPEN_FILE) );

	openFile->FileHandle = FileHandle;
	openFile->FileObject = FileObject;

	openFile->PrimarySession = PrimarySession;

	openFile->CurrentByteOffset.HighPart = 0;
	openFile->CurrentByteOffset.LowPart = 0;
	
    RtlInitEmptyUnicodeString( &openFile->FullFileName,
							   openFile->FullFileNameBuffer,
							   sizeof(openFile->FullFileNameBuffer) );

	RtlCopyUnicodeString( &openFile->FullFileName,
						  FullFileName );

#if defined(_WIN64)
			openFile->OpenFileId = (UINT64)openFile;
			ASSERT( openFile->OpenFileId == (UINT64)openFile );
#else
			openFile->OpenFileId = (UINT32)openFile;
			ASSERT( openFile->OpenFileId == (UINT32)openFile );
#endif
	
	InitializeListHead( &openFile->ListEntry );
	
	ExInterlockedInsertHeadList( &PrimarySession->Thread.OpenedFileQueue,
								 &openFile->ListEntry,
								 &PrimarySession->Thread.OpenedFileQSpinLock );


	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
				   ("PrimarySession_AllocateOpenFile OpenFile = %p\n", openFile) );

	return openFile;
}

VOID
PrimarySession_FreeOpenFile (
	IN	PPRIMARY_SESSION PrimarySession,
	IN  POPEN_FILE		 OpenedFile
	)
{
	UNREFERENCED_PARAMETER( PrimarySession );

	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_NOISE,
					("PrimarySession_FreeOpenFile OpenFile = %p\n", OpenedFile) );
	
	ASSERT( OpenedFile->ListEntry.Flink == OpenedFile->ListEntry.Blink );
	ASSERT( OpenedFile );

	OpenedFile->FileHandle = 0;
	OpenedFile->PrimarySession = NULL;

	ExFreePoolWithTag( OpenedFile, OPEN_FILE_TAG );
}


POPEN_FILE
PrimarySession_FindOpenFile (
	IN  PPRIMARY_SESSION PrimarySession,
	IN	UINT64			 OpenFileId
	)
{	
	POPEN_FILE	openFile;
    PLIST_ENTRY	listEntry;
	KIRQL		oldIrql;

	openFile = NULL;

	KeAcquireSpinLock( &PrimarySession->Thread.OpenedFileQSpinLock, &oldIrql );
    
	for (listEntry = PrimarySession->Thread.OpenedFileQueue.Flink;
         listEntry != &PrimarySession->Thread.OpenedFileQueue;
         listEntry = listEntry->Flink) {

		openFile = CONTAINING_RECORD(listEntry, OPEN_FILE, ListEntry);

		if (openFile->OpenFileId == OpenFileId)
			break;

		openFile = NULL;
	}

	KeReleaseSpinLock( &PrimarySession->Thread.OpenedFileQSpinLock, oldIrql );

	return openFile;
}


POPEN_FILE
PrimarySession_FindOpenFileCleanUpedAndNotClosed (
	IN  PPRIMARY_SESSION	PrimarySession,
	IN	PUNICODE_STRING		FullFileName,
	IN  BOOLEAN				DeleteOnClose
	)
{	
	POPEN_FILE	openFile;
    PLIST_ENTRY	listEntry;
	KIRQL		oldIrql;


	KeAcquireSpinLock( &PrimarySession->Thread.OpenedFileQSpinLock, &oldIrql );

    for (openFile = NULL, listEntry = PrimarySession->Thread.OpenedFileQueue.Flink;
         listEntry != &PrimarySession->Thread.OpenedFileQueue;
         openFile = NULL, listEntry = listEntry->Flink) 
	{

		openFile = CONTAINING_RECORD (listEntry, OPEN_FILE, ListEntry);

		if(openFile->CleanUp != TRUE)
			continue;

		if(openFile->AlreadyClosed == TRUE)
			continue;

		if(openFile->FullFileName.Length != FullFileName->Length)
			continue;

		if(RtlEqualMemory(openFile->FullFileName.Buffer, FullFileName->Buffer, FullFileName->Length))
		{
			if(DeleteOnClose == FALSE && openFile->CreateOptions & FILE_DELETE_ON_CLOSE)
				continue;
			else
				break;
		}
	}

	KeReleaseSpinLock( &PrimarySession->Thread.OpenedFileQSpinLock, oldIrql );

	return openFile;
}


//
// Find open file including AlreadyClosed file.
// If file is already closed, reopen it.
//

POPEN_FILE
PrimarySession_FindOrReopenOpenFile (
	IN  PPRIMARY_SESSION PrimarySession,
	IN	UINT64			 OpenFileId
	)
{	
	POPEN_FILE		openFile;

	HANDLE			fileHandle = NULL;	
	NTSTATUS		status;
	

	NDAS_ASSERT( OpenFileId );

	openFile = PrimarySession_FindOpenFile( PrimarySession, OpenFileId );
	NDAS_ASSERT( openFile );

	if (openFile->AlreadyClosed == TRUE) {

		while(1) {
		
			POPEN_FILE	siblingOpenFile;

			fileHandle = PrimarySessionOpenFile( PrimarySession, openFile );
			
			if (fileHandle) {

				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
							   ("PrimarySession_FindOrReopenOpenFile Reopened file = %p\n", openFile) );	
				break;
			}
			
			SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
						   ("PrimarySession_FindOrReopenOpenFile: Reopening failed\n") );	

			siblingOpenFile = PrimarySession_FindOpenFileCleanUpedAndNotClosed( PrimarySession,
																				&openFile->FullFileName,
																				FALSE );
					
			if (siblingOpenFile == NULL)
				break;

			while (1) {

				siblingOpenFile = PrimarySession_FindOpenFileCleanUpedAndNotClosed( PrimarySession,
																					&openFile->FullFileName,
																					FALSE );
					
				if (siblingOpenFile == NULL)
					break;
				
				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO,
								("PrimarySession_FindOrReopenOpenFile: Uncleaned file found. Trying to close it and reopen\n") );	

				NDAS_ASSERT( siblingOpenFile->FileObject );
				ObDereferenceObject( siblingOpenFile->FileObject );
				siblingOpenFile->FileObject = NULL;

				status = ZwClose( siblingOpenFile->FileHandle );
				NDAS_ASSERT( status == STATUS_SUCCESS );
				siblingOpenFile->FileHandle = NULL;

				status = ZwClose( siblingOpenFile->EventHandle );
				NDAS_ASSERT( status == STATUS_SUCCESS );
				siblingOpenFile->EventHandle = NULL;

				siblingOpenFile->AlreadyClosed = TRUE;
			}
		}
	}

	return (openFile->AlreadyClosed == FALSE) ? openFile : NULL;
}


HANDLE
PrimarySessionOpenFile (
	IN  PPRIMARY_SESSION	PrimarySession,
	IN	POPEN_FILE			OpenFile
	)
{
	HANDLE						fileHandle = NULL;
	PFILE_OBJECT				fileObject;	
	HANDLE						eventHandle;

	NTSTATUS					status;

	ACCESS_MASK					desiredAccess;
	ULONG						attributes;
	OBJECT_ATTRIBUTES			objectAttributes;
	IO_STATUS_BLOCK				ioStatusBlock;
	LARGE_INTEGER				allocationSize;
	ULONG						fileAttributes;
	ULONG						shareAccess;
	ULONG						createDisposition;
	ULONG						createOptions;
	PFILE_FULL_EA_INFORMATION	eaBuffer;
	ULONG						eaLength;			

	UNREFERENCED_PARAMETER( PrimarySession );

	NDAS_ASSERT( OpenFile->AlreadyClosed == TRUE );

	if (OpenFile->EventHandle == NULL) {

		status = ZwCreateEvent( &eventHandle,
							    GENERIC_READ,
								NULL,
								SynchronizationEvent,
								FALSE );

		if (status != STATUS_SUCCESS) {

			ASSERT( LFS_UNEXPECTED );
			return NULL;

		} 
	}
	
	ioStatusBlock.Information = 0;

	desiredAccess = OpenFile->DesiredAccess;

	attributes  = OBJ_KERNEL_HANDLE;
	attributes |= OBJ_CASE_INSENSITIVE;

	InitializeObjectAttributes( &objectAttributes,
								&OpenFile->FullFileName,
								attributes,
								NULL,
								NULL );
		
	allocationSize.LowPart  = 0;
	allocationSize.HighPart = 0;

	fileAttributes	  = 0;		
	shareAccess		  = FILE_SHARE_READ | FILE_SHARE_WRITE;
	createDisposition = FILE_OPEN;

	createOptions     = OpenFile->CreateOptions;

	eaBuffer		  = NULL;
	eaLength		  = 0;
				
	status = ZwCreateFile( &fileHandle,
						   desiredAccess,
						   &objectAttributes,
						   &ioStatusBlock,
						   &allocationSize,
						   fileAttributes,
						   shareAccess,
						   createDisposition,
						   createOptions,
						   eaBuffer,
						   0 );
		
	SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
				   ("PrimarySessionOpenFile: PrimarySession %p, %wZ, createStatus = %x, ioStatusBlock.Information = %d\n",
					 PrimarySession, &OpenFile->FullFileName, status, ioStatusBlock.Information) );

	if (status != STATUS_SUCCESS) {

		ZwClose( eventHandle );
		return NULL;
	}

	status = ObReferenceObjectByHandle( fileHandle,
									    FILE_READ_DATA,
										NULL,
										KernelMode,
										&fileObject,
										NULL );
			
	if (status != STATUS_SUCCESS) {

		NDAS_ASSERT( LFS_UNEXPECTED );		

		ZwClose( eventHandle );
		ZwClose( fileHandle );

		return NULL;
	}
	
	// If previous file position is set, set it again.

	if ((createOptions & (FILE_SYNCHRONOUS_IO_ALERT | FILE_SYNCHRONOUS_IO_NONALERT))  && 
		(OpenFile->CurrentByteOffset.LowPart !=0 || OpenFile->CurrentByteOffset.HighPart !=0)) {
		
		IO_STATUS_BLOCK IoStatus;
		FILE_POSITION_INFORMATION FileInfo;

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
					   ("Setting offset of reopened file to %I64x\n", OpenFile->CurrentByteOffset.QuadPart) );

		FileInfo.CurrentByteOffset = OpenFile->CurrentByteOffset;
		
		//
		// File position is meaningful only in this open mode.
		//
		
		status = ZwSetInformationFile( fileHandle,
									   &IoStatus, 
									   (PVOID)&FileInfo,
									   sizeof(FileInfo),
									   FilePositionInformation );

		if (!NT_SUCCESS(status)) {

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
						   ("Failed to set offset of reopened file to %I64d\n", OpenFile->CurrentByteOffset.QuadPart) );
		}

		OpenFile->CurrentByteOffset.LowPart = 0;
		OpenFile->CurrentByteOffset.HighPart = 0;
	}

	OpenFile->EventHandle = eventHandle;
	OpenFile->FileHandle = fileHandle;
	OpenFile->FileObject = fileObject;

	OpenFile->AlreadyClosed = FALSE;

	return fileHandle;
}
