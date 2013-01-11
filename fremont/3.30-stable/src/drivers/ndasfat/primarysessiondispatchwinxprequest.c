#include "FatProcs.h"

#if __NDAS_FAT_PRIMARY__

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('PftN')

#define Dbg                              (DEBUG_TRACE_PRIMARY)
#define Dbg2                             (DEBUG_INFO_PRIMARY)

#pragma warning(error:4100)   // Unreferenced formal parameter
#pragma warning(error:4101)   // Unreferenced local variable


//
//	ntifs.h of Windows XP does not include ZwSetVolumeInformationFile nor NtSetVolumeInformationFile.
//
NTSYSAPI
NTSTATUS
NTAPI
ZwSetVolumeInformationFile(
    IN HANDLE FileHandle,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    IN PVOID FsInformation,
    IN ULONG Length,
    IN FS_INFORMATION_CLASS FsInformationClass
    );


NTSTATUS
NtQueryEaFile(
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
NtSetEaFile(
    IN HANDLE FileHandle,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    IN PVOID Buffer,
    IN ULONG Length
    );

#ifndef NTDDI_VERSION

#if WINVER <= 0x0501

NTSTATUS
NtUnlockFile(
    IN HANDLE FileHandle,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    IN PLARGE_INTEGER ByteOffset,
    IN PLARGE_INTEGER Length,
    IN ULONG Key
    );

#endif

#endif

static NTSTATUS
PrimarySession_IrpMjCreate(
	IN  PPRIMARY_SESSION			PrimarySession,
	IN  PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader,
	IN  PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplytHeader,
	IN  UINT32						RequestDataSize,
	UINT8								*NdfsRequestData,
	OUT	UINT32						*ReplyDataSize
	);

static NTSTATUS
PrimarySession_IrpMjClose(
	IN  PPRIMARY_SESSION			PrimarySession,
	IN  PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader,
	IN  PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplytHeader,
	IN  UINT32						DataSize,
	OUT	UINT32						*ReplyDataSize,
	UINT8								*NdfsRequestData
	);

static NTSTATUS
PrimarySession_IrpMjRead(
	IN  PPRIMARY_SESSION			PrimarySession,
	IN  PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader,
	IN  PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplytHeader,
	IN  UINT32						DataSize,
	OUT	UINT32						*ReplyDataSize,
	UINT8								*NdfsRequestData
	);

static ULONG
CalculateSafeLength(
	FILE_INFORMATION_CLASS	fileInformationClass,
	ULONG					requestLength,
	ULONG					returnedLength,
	PCHAR					Buffer
	); 




NTSTATUS
DispatchWinXpRequest(
	IN  PPRIMARY_SESSION			PrimarySession,
	IN  PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader,
	IN  PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplytHeader,
	IN  UINT32						RequestDataSize,
	OUT	UINT32						*ReplyDataSize
	)
{
	NTSTATUS	status;
	UINT8			*ndfsRequestData;

#if __NDAS_FAT_RECOVERY_TEST__

	if (PrimarySession->ReceiveCount == 5000) {

		if (NdfsWinxpRequestHeader->IrpMajorFunction == IRP_MJ_CREATE) {

			status = STATUS_IO_DEVICE_ERROR;
			DebugTrace2( 0, Dbg, ("DispatchWinXpRequest: Test recovery IRP_MJ_CREATE\n") );
			//return status;
		
		} else if (NdfsWinxpRequestHeader->IrpMajorFunction == IRP_MJ_CLEANUP) {

			status = STATUS_IO_DEVICE_ERROR;
			DebugTrace2( 0, Dbg, ("DispatchWinXpRequest: Test recovery IRP_MJ_CLEANUP\n") );
			//return status;
		
		} else if (NdfsWinxpRequestHeader->IrpMajorFunction == IRP_MJ_CLOSE) {

			status = STATUS_IO_DEVICE_ERROR;
			DebugTrace2( 0, Dbg, ("DispatchWinXpRequest: Test recovery IRP_MJ_CLOSE\n") );
			//return status;
		
		} else if (NdfsWinxpRequestHeader->IrpMajorFunction == IRP_MJ_WRITE) {

			status = STATUS_IO_DEVICE_ERROR;
			DebugTrace2( 0, Dbg, ("DispatchWinXpRequest: Test recovery IRP_MJ_WRITE\n") );
			//return status;
		
		} else if (NdfsWinxpRequestHeader->IrpMajorFunction == IRP_MJ_SET_INFORMATION) {

			status = STATUS_IO_DEVICE_ERROR;
			DebugTrace2( 0, Dbg, ("DispatchWinXpRequest: Test recovery IRP_MJ_SET_INFORMATION\n") );
			//return status;
		}
	
	} else {

		PrimarySession->ReceiveCount ++;

		if (PrimarySession->ReceiveCount % 1000 == 0)
			DebugTrace2( 0, Dbg2, ("DispatchWinXpRequest: Test recovery PrimarySession->ReceiveCount = %d\n", PrimarySession->ReceiveCount) );
	}

#endif

	if (RequestDataSize)
		ndfsRequestData = (UINT8 *)(NdfsWinxpRequestHeader+1);
	else
		ndfsRequestData = NULL;


	switch (NdfsWinxpRequestHeader->IrpMajorFunction) {

	case IRP_MJ_CREATE: { // 0x00

		ndfsRequestData = (UINT8 *)(NdfsWinxpRequestHeader+1);

		status = PrimarySession_IrpMjCreate( PrimarySession,
											 NdfsWinxpRequestHeader,
											 NdfsWinxpReplytHeader,
											 RequestDataSize,
											 ndfsRequestData,
											 ReplyDataSize );

		break;
	}

	case IRP_MJ_CLOSE: { // 0x02

		status = PrimarySession_IrpMjClose( PrimarySession,
											NdfsWinxpRequestHeader,
											NdfsWinxpReplytHeader,
											RequestDataSize,
											ReplyDataSize,
											ndfsRequestData );
		break;
	}

	case IRP_MJ_READ: { // 0x03
	
		status = PrimarySession_IrpMjRead( PrimarySession,
										   NdfsWinxpRequestHeader,
										   NdfsWinxpReplytHeader,
										   RequestDataSize,
										   ReplyDataSize,
										   ndfsRequestData );

		break;
	}

	case IRP_MJ_WRITE: { // 0x04

		NTSTATUS		writeStatus;
		LONG			trial = 0;

		UINT64			openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE		openFile;

		HANDLE			fileHandle = NULL;
		PFILE_OBJECT	fileObject = NULL;

		IO_STATUS_BLOCK	ioStatusBlock;
		PVOID			buffer;
		ULONG			length;
		LARGE_INTEGER	byteOffset;
		ULONG			key;
		BOOLEAN			fakeWrite;

		PIRP					topLevelIrp;
		PRIMARY_REQUEST_INFO	primaryRequestInfo;


		openFile = PrimarySession_FindOpenFile( PrimarySession, openFileId );

		if ( openFile == NULL ) {

			ASSERT( FALSE );
			writeStatus = STATUS_UNSUCCESSFUL;

		} else {

			ASSERT(openFile && openFile->FileObject);
	
			fileHandle = openFile->FileHandle;
			fileObject = openFile->FileObject;

	
			buffer				= (UINT8 *)(NdfsWinxpRequestHeader+1);
			length				= NdfsWinxpRequestHeader->Write.Length;
			byteOffset.QuadPart = NdfsWinxpRequestHeader->Write.ByteOffset;

			key = NdfsWinxpRequestHeader->Write.Key;

		//rint( "data = %s\n", buffer );


			RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );
		
			if (NdfsWinxpRequestHeader->Write.Fake == TRUE) {
			
				fakeWrite = TRUE;
		
			} else {

				fakeWrite = FALSE;
			}

			primaryRequestInfo.PrimaryTag			  = 0xe2027482;
			primaryRequestInfo.PrimarySessionId		  = PrimarySession->PrimarySessionId;
			primaryRequestInfo.PrimarySession		  = PrimarySession;
			primaryRequestInfo.NdfsWinxpRequestHeader = NdfsWinxpRequestHeader;

			topLevelIrp = IoGetTopLevelIrp();
			ASSERT( topLevelIrp == NULL );
			IoSetTopLevelIrp( (PIRP)&primaryRequestInfo );

			writeStatus = NtWriteFile( fileHandle,
									   NULL,
									   NULL,
									   NULL,
									   &ioStatusBlock,
									   buffer,
									   length,
									   &byteOffset,
									   key ? &key : NULL );

			IoSetTopLevelIrp( topLevelIrp );

			DebugTrace2( 0, Dbg, ("writeStatus = %x\n", writeStatus) );
		}

		NdfsWinxpReplytHeader->FileInformationSet = FALSE;
		NdfsWinxpReplytHeader->NumberOfMcbEntry4 = 0;
		*ReplyDataSize = 0;

		//NdfsWinxpReplytHeader->IrpTag4			 = NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status4			 = HTONL(writeStatus);
		NdfsWinxpReplytHeader->Information32		 = HTONL((UINT32)ioStatusBlock.Information);

		NdfsWinxpReplytHeader->CurrentByteOffset8 = HTONLL(fileObject->CurrentByteOffset.QuadPart);
		
		status = STATUS_SUCCESS;

		break;
	}
	
	case IRP_MJ_QUERY_INFORMATION: { // 0x05
	
		NTSTATUS				queryInformationStatus;

		UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE				openFile;
		HANDLE					fileHandle = NULL;
		PFILE_OBJECT			fileObject = NULL;

	    PVOID					fileInformation;
		ULONG					length;
		FILE_INFORMATION_CLASS	fileInformationClass;
		ULONG					returnedLength = 0;
		

		openFile = PrimarySession_FindOpenFile( PrimarySession, openFileId );

		if (openFile == NULL) {

			NDAS_ASSERT( FALSE );
			queryInformationStatus = STATUS_UNSUCCESSFUL;
			returnedLength = 0;

		} else {
		
			ASSERT( openFile && openFile->FileObject );
			ASSERT( NdfsWinxpRequestHeader->QueryFile.Length <= PrimarySession->SessionContext.SecondaryMaxDataSize );

			fileHandle = openFile->FileHandle;
			fileObject = openFile->FileObject;

			fileInformation		 = (UINT8 *)(NdfsWinxpReplytHeader+1);
			length				 = NdfsWinxpRequestHeader->QueryFile.Length;
			fileInformationClass = NdfsWinxpRequestHeader->QueryFile.FileInformationClass;

			queryInformationStatus = IoQueryFileInformation( fileObject,
															 fileInformationClass,
															 length,
															 fileInformation,
															 &returnedLength );

			if (queryInformationStatus == STATUS_BUFFER_OVERFLOW) {
		
				ASSERT(length == returnedLength);

				DebugTrace2( 0, Dbg,
						   ("IoQueryFileInformation: openFileId = %X, length = %d, queryInformationStatus = %X, returnedLength = %d\n",
							openFileId, length, queryInformationStatus, returnedLength) );
			}

			if (NT_SUCCESS(queryInformationStatus))
				ASSERT(queryInformationStatus == STATUS_SUCCESS);

			if (!(queryInformationStatus == STATUS_SUCCESS || queryInformationStatus == STATUS_BUFFER_OVERFLOW)) {

				returnedLength = 0;
			}
		}

		//NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;
		
		NdfsWinxpReplytHeader->Status4	   = HTONL(queryInformationStatus);
		NdfsWinxpReplytHeader->Information32 = HTONL(returnedLength);

		*ReplyDataSize = returnedLength;
		
		status = STATUS_SUCCESS;

		break;
	}



	case IRP_MJ_SET_INFORMATION: { // 0x06
	
		NTSTATUS				setInformationStatus;

		UINT64					openFileId;
		POPEN_FILE				openFile;
		
	    PVOID					fileInformation;
		ULONG					length;
		FILE_INFORMATION_CLASS	fileInformationClass;

		HANDLE					fileHandle = NULL;
		PFILE_OBJECT			fileObject = NULL;

		POPEN_FILE				setFileOpenFile;
		
		PIRP					topLevelIrp;
		PRIMARY_REQUEST_INFO	primaryRequestInfo;


		openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		openFile = PrimarySession_FindOpenFile( PrimarySession, openFileId );

		do {

			if (openFile == NULL) {

				NDAS_ASSERT( FALSE );
				setInformationStatus = STATUS_UNSUCCESSFUL;
				break;
			}
		
			ASSERT( openFile->FileObject );

			fileHandle = openFile->FileHandle;
			fileObject = openFile->FileObject;

			if (NdfsWinxpRequestHeader->SetFile.FileHandle == 0) {

				setFileOpenFile = NULL;
			
			} else {
			
				setFileOpenFile = PrimarySession_FindOpenFile( PrimarySession,
															   NdfsWinxpRequestHeader->SetFile.FileHandle );

				if ( setFileOpenFile == NULL ) {

					ASSERT( FALSE );
					setInformationStatus = STATUS_UNSUCCESSFUL;
					break;
				}

			    DebugTrace2( 0, Dbg2, ("TargetFileObject = %p\n", setFileOpenFile->FileObject) );
				DebugTrace2( 0, Dbg2, ("NewName = %Z\n", &setFileOpenFile->FileObject->FileName) );
			
			} 

			fileInformation = ExAllocatePoolWithTag( NonPagedPool, 
													 NdfsWinxpRequestHeader->SetFile.Length +
													 2 + // NULL
													 8 + // 64 bit adjustment
													 8 + // dummy
													 PrimarySession->NetdiskPartitionInformation.VolumeName.Length,
													 PRIMARY_SESSION_BUFFERE_TAG );

			if (fileInformation == NULL) {

				ASSERT( NDASFAT_INSUFFICIENT_RESOURCES );
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
					//ASSERT( rootDirectoryFile->AlreadyClosed == FALSE );
			
				} else {

					rootDirectoryFile = NULL;
				}

				linkInfomation->ReplaceIfExists = NdfsWinxpRequestHeader->SetFile.LinkInformation.ReplaceIfExists;
				linkInfomation->RootDirectory = rootDirectoryFile ? rootDirectoryFile->FileHandle : NULL;
				linkInfomation->FileNameLength = NdfsWinxpRequestHeader->SetFile.LinkInformation.FileNameLength;
			
				RtlCopyMemory( linkInfomation->FileName,
							   ndfsRequestData,
							   linkInfomation->FileNameLength );
			
				DebugTrace2( 0, Dbg2,
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
					//ASSERT( rootDirectoryFile->AlreadyClosed == FALSE );
			
				} else {

					rootDirectoryFile = NULL;
				}
		
				renameInformation->ReplaceIfExists = NdfsWinxpRequestHeader->SetFile.RenameInformation.ReplaceIfExists;
				renameInformation->RootDirectory = rootDirectoryFile ? rootDirectoryFile->FileHandle : NULL;

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
				
						ASSERT( NDASFAT_BUG );
					}

					DebugTrace2( 0, Dbg2,
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

				ASSERT( NDASFAT_BUG );
			}
				
			primaryRequestInfo.PrimaryTag			  = 0xe2027482;
			primaryRequestInfo.PrimarySessionId		  = PrimarySession->PrimarySessionId;
			primaryRequestInfo.PrimarySession		  = PrimarySession;
			primaryRequestInfo.NdfsWinxpRequestHeader = NdfsWinxpRequestHeader;

			topLevelIrp = IoGetTopLevelIrp();
			ASSERT( topLevelIrp == NULL );
			IoSetTopLevelIrp( (PIRP)&primaryRequestInfo );

			setInformationStatus = IoSetInformation( fileObject,
													 fileInformationClass,
													 length,
													 fileInformation );

			IoSetTopLevelIrp( topLevelIrp );


			DebugTrace2( 0, Dbg,
						("DispatchWinXpRequest: IoSetInformation: fileHandle = %X, fileInformationClass = %X, setInformationStatus = %X\n",
						openFile->FileHandle, fileInformationClass, setInformationStatus) );

			if (NT_SUCCESS(setInformationStatus))
				ASSERT( setInformationStatus == STATUS_SUCCESS );
		
		} while (0);

		//NdfsWinxpReplytHeader->IrpTag4		 = NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status4 = HTONL(setInformationStatus);

		if (NT_SUCCESS(setInformationStatus)) {

			NdfsWinxpReplytHeader->Information32 = HTONL(length);

		} else {

			NdfsWinxpReplytHeader->Information32 = 0;
		}

		if (NT_SUCCESS(setInformationStatus)) {
		
		    TYPE_OF_OPEN	typeOfOpen;
		    PVCB			vcb;
			PFCB			fcb;
			//PSCB			scb;
			PCCB			ccb;
		
			typeOfOpen = FatDecodeFileObject( openFile->FileObject, &vcb, &fcb, &ccb );
			
			ASSERT( fcb && ccb );
#if 0
			NdfsWinxpReplytHeader->DuplicatedInformation.AllocatedLength		= fcb->Info.AllocatedLength;
			NdfsWinxpReplytHeader->DuplicatedInformation.CreationTime			= fcb->Info.CreationTime;
			NdfsWinxpReplytHeader->DuplicatedInformation.FileAttributes			= fcb->Info.FileAttributes;
			NdfsWinxpReplytHeader->DuplicatedInformation.FileSize				= fcb->Info.FileSize;
			NdfsWinxpReplytHeader->DuplicatedInformation.LastAccessTime			= fcb->Info.LastAccessTime;
			NdfsWinxpReplytHeader->DuplicatedInformation.LastChangeTime			= fcb->Info.LastChangeTime;
			NdfsWinxpReplytHeader->DuplicatedInformation.LastModificationTime	= fcb->Info.LastModificationTime;
			NdfsWinxpReplytHeader->DuplicatedInformation.ReparsePointTag		= fcb->Info.ReparsePointTag;
#endif			
			
			if (!(typeOfOpen == UserFileOpen &&
				 (fileInformationClass == FileAllocationInformation || fileInformationClass == FileEndOfFileInformation))) {

				NdfsWinxpReplytHeader->FileInformationSet = FALSE;
				*ReplyDataSize = 0;
			
			} else {
		
				ULONG			index;
				ULONG			clusterCount;
				PNDFS_FAT_MCB_ENTRY	mcbEntry;

				VBO				vcn;
				LBO				lcn;
				//LCN			startingLcn;

												
				NdfsWinxpReplytHeader->Open.TruncateOnClose = BooleanFlagOn( fcb->FcbState, FCB_STATE_TRUNCATE_ON_CLOSE );

				NDAS_ASSERT( fcb->Header.FileSize.QuadPart <= fcb->Header.AllocationSize.QuadPart );

				NdfsWinxpReplytHeader->FileSize8		= HTONLL(fcb->Header.FileSize.QuadPart);
				NdfsWinxpReplytHeader->AllocationSize8	= HTONLL(fcb->Header.AllocationSize.QuadPart);
				NdfsWinxpReplytHeader->ValidDataLength8	= HTONLL(fcb->Header.ValidDataLength.QuadPart);

				//ASSERT( !FlagOn(fcb->AttributeFlags, ATTRIBUTE_FLAG_SPARSE) );

				DebugTrace2( 0, Dbg, ("session Scb->Header.ValidDataLength.QuadPart = %I64x\n", fcb->Header.ValidDataLength.QuadPart) );
				
				mcbEntry = (PNDFS_FAT_MCB_ENTRY)(NdfsWinxpReplytHeader+1);

				vcn = 0; index = 0;

				while (FatLookupMcbEntry(vcb, &fcb->Mcb, vcn, &lcn, &clusterCount, NULL)) {

					NDAS_ASSERT( (index+1)*sizeof(NDFS_FAT_MCB_ENTRY) <= PrimarySession->SessionContext.SecondaryMaxDataSize ); 

					mcbEntry[index].Vcn = vcn; 

					ASSERT( ((UINT16)lcn & (0x1FF)) == 0 );

					mcbEntry[index].Lcn = (UINT32)(lcn >> vcb->AllocationSupport.LogOfBytesPerSector); 
					mcbEntry[index].ClusterCount = clusterCount;

					vcn += clusterCount;
					index++;

					if (vcn == 0) {

						break;
					}
				}

				NDAS_ASSERT( vcn == 0 && index != 0 || vcn == fcb->Header.AllocationSize.QuadPart );

				NdfsWinxpReplytHeader->NumberOfMcbEntry4 = HTONL(index);

				NdfsWinxpReplytHeader->FileInformationSet = TRUE;
				*ReplyDataSize = index*sizeof( NDFS_FAT_MCB_ENTRY );
			} 

		} else {
		
			NdfsWinxpReplytHeader->FileInformationSet = FALSE;
			*ReplyDataSize = 0;
		}

		if (fileInformation)		
			ExFreePoolWithTag( fileInformation, PRIMARY_SESSION_BUFFERE_TAG );

		status = STATUS_SUCCESS;
		break;
	}

	 case IRP_MJ_FLUSH_BUFFERS: { // 0x09
			
		UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE				openFile;
		

		openFile = PrimarySession_FindOpenFile( PrimarySession, openFileId );
		
		//NdfsWinxpReplytHeader->IrpTag4			 = NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status4	   = HTONL(STATUS_SUCCESS);
		NdfsWinxpReplytHeader->Information32 = 0;

		*ReplyDataSize = 0;

		
		status = STATUS_SUCCESS;

		break;
	}

	 case IRP_MJ_QUERY_VOLUME_INFORMATION: { // 0x0A

		NTSTATUS				queryVolumeInformationStatus;

		UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE				openFile;

	    PVOID					fsInformation;
		ULONG					length;
		FILE_INFORMATION_CLASS	fsInformationClass;
	    ULONG					returnedLength = 0;
		

		openFile = PrimarySession_FindOpenFile( PrimarySession, openFileId );

		if (openFile == NULL) {

			NDAS_ASSERT( FALSE );
			queryVolumeInformationStatus = STATUS_UNSUCCESSFUL;
		
		} else {

			ASSERT( openFile->FileObject );
			ASSERT( NdfsWinxpRequestHeader->QueryVolume.Length <= PrimarySession->SessionContext.SecondaryMaxDataSize );

			fsInformation		 = (UINT8 *)(NdfsWinxpReplytHeader+1);
			length				 = NdfsWinxpRequestHeader->QueryVolume.Length;
			fsInformationClass   = NdfsWinxpRequestHeader->QueryVolume.FsInformationClass;
		
			queryVolumeInformationStatus = IoQueryVolumeInformation( openFile->FileObject,
																	 fsInformationClass,
																	 length,
																	 fsInformation,
																	 &returnedLength );

			if (NT_SUCCESS(queryVolumeInformationStatus))
				ASSERT( queryVolumeInformationStatus == STATUS_SUCCESS );

			if (queryVolumeInformationStatus == STATUS_BUFFER_OVERFLOW)
				ASSERT( length == returnedLength );
			
			if (!(queryVolumeInformationStatus == STATUS_SUCCESS || queryVolumeInformationStatus == STATUS_BUFFER_OVERFLOW)) {

				returnedLength = 0;
			}

			if (queryVolumeInformationStatus == STATUS_BUFFER_OVERFLOW)
				DebugTrace2( 0, Dbg,
							("DispatchWinXpRequest: IoQueryVolumeInformation: fileHandle = %X, queryVolumeInformationStatus = %X, length = %d, returnedLength = %d\n",
							  openFile->FileHandle, queryVolumeInformationStatus, length, returnedLength));
		}

		//NdfsWinxpReplytHeader->IrpTag4			 = NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status4	     = HTONL(queryVolumeInformationStatus);
		NdfsWinxpReplytHeader->Information32   = HTONL(returnedLength);

		*ReplyDataSize = returnedLength;
		
		status = STATUS_SUCCESS;

		break;
	}

	case IRP_MJ_SET_VOLUME_INFORMATION: {
	
		NTSTATUS				setVolumeStatus;

		UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE				openFile;
		
		IO_STATUS_BLOCK			IoStatusBlock;

		PVOID					volumeInformation;
		ULONG					length;
		FS_INFORMATION_CLASS	volumeInformationClass;


		openFile = PrimarySession_FindOpenFile(	PrimarySession, openFileId );

		if (openFile == NULL) {

			NDAS_ASSERT( FALSE );
			setVolumeStatus = STATUS_UNSUCCESSFUL;

		} else {
		
			ASSERT( openFile->FileObject );

			length				   = NdfsWinxpRequestHeader->SetVolume.Length;
			volumeInformationClass = NdfsWinxpRequestHeader->SetVolume.FsInformationClass;
			volumeInformation	   = (UINT8 *)(NdfsWinxpRequestHeader+1);

			setVolumeStatus = ZwSetVolumeInformationFile( openFile->FileHandle,
														  &IoStatusBlock,
														  volumeInformation,
														  length,
														  volumeInformationClass ); 
		
			DebugTrace2( 0, Dbg,
						("DispatchWinXpRequest: ZwSetVolumeInformationFile: fileHandle = %X, volumeInformationClass = %X, setVolumeStatus = %X\n",
						openFile->FileHandle, volumeInformationClass, setVolumeStatus));

			if (NT_SUCCESS(setVolumeStatus))
				ASSERT(setVolumeStatus == STATUS_SUCCESS);
		}


		//NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status4	  = HTONL(setVolumeStatus);
		NdfsWinxpReplytHeader->Information32 = HTONL(length);

		*ReplyDataSize = 0;

		status = STATUS_SUCCESS;
		break ;
	}

	case IRP_MJ_DIRECTORY_CONTROL: { // 0x0C
	
		NTSTATUS	queryDirectoryStatus;

		DebugTrace2( 0, Dbg,
					("DispatchWinXpRequest: IRP_MJ_DIRECTORY_CONTROL: MinorFunction = %X\n",
					 NdfsWinxpRequestHeader->IrpMajorFunction) );

		if (NdfsWinxpRequestHeader->IrpMinorFunction != IRP_MN_QUERY_DIRECTORY) {

			ASSERT( NDASFAT_BUG );
			queryDirectoryStatus = STATUS_UNSUCCESSFUL;

		} else {

			UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
			POPEN_FILE				openFile;
		
			BOOLEAN					synchronousIo;

			IO_STATUS_BLOCK			ioStatusBlock;
		    PVOID					fileInformation;
			ULONG					length;
			FILE_INFORMATION_CLASS	fileInformationClass;
			BOOLEAN					returnSingleEntry;
			UNICODE_STRING			fileName;
			WCHAR					fileNameBuffer[NDFS_MAX_PATH];
			BOOLEAN					restartScan;
 		

			openFile = PrimarySession_FindOpenFile(	PrimarySession, openFileId );

			if (openFile == NULL) {

				NDAS_ASSERT( FALSE );
				queryDirectoryStatus = STATUS_UNSUCCESSFUL;
				status = STATUS_SUCCESS;

			} else {

				ASSERT( openFile->FileObject );
				ASSERT( NdfsWinxpRequestHeader->QueryDirectory.Length <= PrimarySession->SessionContext.SecondaryMaxDataSize );
	
			
				fileInformation			= (UINT8 *)(NdfsWinxpReplytHeader+1);
				length					= NdfsWinxpRequestHeader->QueryDirectory.Length;
				fileInformationClass	= NdfsWinxpRequestHeader->QueryDirectory.FileInformationClass;
				returnSingleEntry		= (NdfsWinxpRequestHeader->IrpSpFlags & SL_RETURN_SINGLE_ENTRY) ? TRUE : FALSE;
				restartScan				= (NdfsWinxpRequestHeader->IrpSpFlags & SL_RESTART_SCAN) ? TRUE : FALSE;

				RtlInitEmptyUnicodeString( &fileName, fileNameBuffer, sizeof(fileNameBuffer) );

				do {
					status = RtlAppendUnicodeToString( &fileName, (PWCHAR)ndfsRequestData );

					if (status != STATUS_SUCCESS) {

						ASSERT(NDASFAT_UNEXPECTED);
						status = STATUS_UNSUCCESSFUL;
						break;
					}

					RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

					synchronousIo = BooleanFlagOn(openFile->FileObject->Flags, FO_SYNCHRONOUS_IO);

					if (synchronousIo) {

						queryDirectoryStatus = NtQueryDirectoryFile( openFile->FileHandle,
																	 NULL,
																	 NULL,
																	 NULL,
																	 &ioStatusBlock,
																	 fileInformation,
																	 length,
																	 fileInformationClass,
																	 returnSingleEntry,
																	 &fileName,
																	 restartScan );
					} else {

						queryDirectoryStatus = NtQueryDirectoryFile( openFile->FileHandle,
																	 openFile->EventHandle,
																	 NULL,
																	 NULL,
																	 &ioStatusBlock,
																	 fileInformation,
																	 length,
																	 fileInformationClass,
																	 returnSingleEntry,
																	 &fileName,
																	 restartScan );

						if (queryDirectoryStatus == STATUS_PENDING) {

							queryDirectoryStatus = ZwWaitForSingleObject(openFile->EventHandle, TRUE, NULL);
						}
					}

					DebugTrace2( 0, Dbg, ("DispatchWinXpRequest: IRP_MJ_DIRECTORY_CONTROL queryDirectoryStatus = %X\n", queryDirectoryStatus) );
					DebugTrace2( 0, Dbg, ("DispatchWinXpRequest: IRP_MJ_DIRECTORY_CONTROL ioStatusBlock.Status = %X\n", ioStatusBlock.Status) );
	
					if (queryDirectoryStatus == STATUS_BUFFER_OVERFLOW)
						DebugTrace2( 0, Dbg,
									("DispatchWinXpRequest: NtQueryDirectoryFile: openFileId = %X, queryDirectoryStatus = %X, length = %d, ioStatusBlock.Status = %X, ioStatusBlock.Information = %d\n",
									  openFileId, queryDirectoryStatus, length, ioStatusBlock.Status, ioStatusBlock.Information));

					if (NT_SUCCESS(queryDirectoryStatus)) {
				
						ASSERT( queryDirectoryStatus == STATUS_SUCCESS );		
						ASSERT( queryDirectoryStatus == ioStatusBlock.Status );
					}
			
					if (queryDirectoryStatus == STATUS_BUFFER_OVERFLOW)
						ASSERT(length == ioStatusBlock.Information);

					if (!(queryDirectoryStatus == STATUS_SUCCESS || queryDirectoryStatus == STATUS_BUFFER_OVERFLOW)) {
					
						ASSERT(ioStatusBlock.Information == 0);
						ioStatusBlock.Information = 0;
					}
			
					//NdfsWinxpReplytHeader->IrpTag4			 = NdfsWinxpRequestHeader->IrpTag4;
					NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
					NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

					NdfsWinxpReplytHeader->Status4	  = HTONL(queryDirectoryStatus);
					NdfsWinxpReplytHeader->Information32 = HTONL((UINT32)ioStatusBlock.Information);

					if (queryDirectoryStatus == STATUS_SUCCESS || queryDirectoryStatus == STATUS_BUFFER_OVERFLOW) {

						if (ioStatusBlock.Information)
							*ReplyDataSize = CalculateSafeLength( fileInformationClass,
																  length,
																  (UINT32)ioStatusBlock.Information,
																  fileInformation );
						else
							*ReplyDataSize = 0;
				
					} else {
				
						*ReplyDataSize = 0;
					}

					status = STATUS_SUCCESS;
				
				} while(0);
			}
		} 
		break;
	}
	
	case IRP_MJ_FILE_SYSTEM_CONTROL: { // 0x0D
	
		NTSTATUS		fileSystemControlStatus;

		UINT64			openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE		openFile;

		IO_STATUS_BLOCK	ioStatusBlock;
		ULONG			fsControlCode;
		PVOID			inputBuffer;
		ULONG			inputBufferLength;
		PVOID			outputBuffer;
		ULONG			outputBufferLength;

		MOVE_FILE_DATA		moveFileData;	
		MARK_HANDLE_INFO	markHandleInfo;


		openFile = PrimarySession_FindOpenFile( PrimarySession, openFileId );
		
		if (openFile == NULL) {

			NDAS_ASSERT( FALSE );
			fileSystemControlStatus = STATUS_UNSUCCESSFUL;
			status = STATUS_SUCCESS;

		} else {

			ASSERT( openFile->FileObject );
			ASSERT( NdfsWinxpRequestHeader->FileSystemControl.OutputBufferLength <= PrimarySession->SessionContext.SecondaryMaxDataSize );


			fsControlCode		= NdfsWinxpRequestHeader->FileSystemControl.FsControlCode;
			inputBufferLength	= NdfsWinxpRequestHeader->FileSystemControl.InputBufferLength;

			do {
			
				if (fsControlCode == FSCTL_MOVE_FILE) {

					POPEN_FILE		moveFile;
			
					ASSERT(sizeof(FILE_RENAME_INFORMATION) - sizeof(WCHAR) + NdfsWinxpRequestHeader->SetFile.RenameInformation.FileNameLength <= NdfsWinxpRequestHeader->SetFile.Length);
						
					moveFile = PrimarySession_FindOpenFile( PrimarySession,
															NdfsWinxpRequestHeader->FileSystemControl.FscMoveFileData.FileHandle );

					if (moveFile == NULL) {

						ASSERT( FALSE );
						fileSystemControlStatus = STATUS_UNSUCCESSFUL;
						status = STATUS_SUCCESS;
						break;
					}

					moveFileData.FileHandle				= moveFile->FileHandle;
					moveFileData.StartingVcn.QuadPart	= NdfsWinxpRequestHeader->FileSystemControl.FscMoveFileData.StartingVcn;
					moveFileData.StartingLcn.QuadPart	= NdfsWinxpRequestHeader->FileSystemControl.FscMoveFileData.StartingLcn;
					moveFileData.ClusterCount			= NdfsWinxpRequestHeader->FileSystemControl.FscMoveFileData.ClusterCount;

					inputBuffer = &moveFileData;
		
				} else if (fsControlCode == FSCTL_MARK_HANDLE) {

					POPEN_FILE			markFile;

					markFile = PrimarySession_FindOpenFile( PrimarySession,
															NdfsWinxpRequestHeader->FileSystemControl.FscMarkHandleInfo.VolumeHandle );


					if (markFile == NULL) {

						ASSERT( FALSE );
						fileSystemControlStatus = STATUS_UNSUCCESSFUL;
						status = STATUS_SUCCESS;
						break;
					}

					markHandleInfo.UsnSourceInfo = NdfsWinxpRequestHeader->FileSystemControl.FscMarkHandleInfo.UsnSourceInfo;
					markHandleInfo.VolumeHandle	 = markFile->FileHandle;
					markHandleInfo.HandleInfo    = NdfsWinxpRequestHeader->FileSystemControl.FscMarkHandleInfo.HandleInfo;

					inputBuffer = &markHandleInfo;
		
				} else {

					inputBuffer	= ndfsRequestData;
				}

				outputBuffer		= (UINT8 *)(NdfsWinxpReplytHeader+1);
				outputBufferLength	= NdfsWinxpRequestHeader->FileSystemControl.OutputBufferLength;

				RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

				fileSystemControlStatus = ZwFsControlFile( openFile->FileHandle,
														   NULL,
														   NULL,
														   NULL,
														   &ioStatusBlock,
														   fsControlCode,
														   inputBuffer,
														   inputBufferLength,
														   outputBuffer,
														   outputBufferLength );
		
				DebugTrace2( 0, Dbg,
							("DispatchWinXpRequest: IRP_MJ_FILE_SYSTEM_CONTROL: openFileId = %x, function = %d, fileSystemControlStatus = %x, ioStatusBlock.Status = %x, ioStatusBlock.Information = %d\n",
							  openFileId, (fsControlCode & 0x00003FFC) >> 2, fileSystemControlStatus, ioStatusBlock.Status, ioStatusBlock.Information));
		
				if (NT_SUCCESS(fileSystemControlStatus)) {

					ASSERT( fileSystemControlStatus == STATUS_SUCCESS );
					ASSERT( fileSystemControlStatus == ioStatusBlock.Status );
				}

				if (fileSystemControlStatus == STATUS_BUFFER_OVERFLOW)
					ASSERT(ioStatusBlock.Information == inputBufferLength || ioStatusBlock.Information == outputBufferLength);
		
				if (!(fileSystemControlStatus == STATUS_SUCCESS || fileSystemControlStatus == STATUS_BUFFER_OVERFLOW)) {

					ioStatusBlock.Information = 0;
					ASSERT( ioStatusBlock.Information == 0 );
				}

			} while (0);

			//NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
			NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
			NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

			NdfsWinxpReplytHeader->Status4	  = HTONL(fileSystemControlStatus);
			NdfsWinxpReplytHeader->Information32 = HTONL((UINT32)ioStatusBlock.Information);

			if (outputBufferLength) {

				*ReplyDataSize = (UINT32)ioStatusBlock.Information;
			
			} else
				*ReplyDataSize = 0;


			if (NTOHL(NdfsWinxpReplytHeader->Status4) == STATUS_SUCCESS && fsControlCode == FSCTL_MOVE_FILE) {		// 29
				
				NTSTATUS		status;
				PMOVE_FILE_DATA	moveFileData = inputBuffer;	
				PFILE_OBJECT	moveFileObject;

				TYPE_OF_OPEN	typeOfOpen;
				PVCB			vcb;
				PFCB			moveFcb;
				//PSCB			moveScb;
				PCCB			moveCcb;


				status = ObReferenceObjectByHandle( moveFileData->FileHandle,
													FILE_READ_DATA,
													0,
													KernelMode,
													&moveFileObject,
													NULL );

				if (status != STATUS_SUCCESS) {

					status = STATUS_UNSUCCESSFUL;
					break;
				}
	
				ObDereferenceObject( moveFileObject );
				
				typeOfOpen = FatDecodeFileObject( moveFileObject, &vcb, &moveFcb, &moveCcb );
		
				if (typeOfOpen == UserFileOpen) {

					ULONG			index;
					ULONG			clusterCount;
					PNDFS_FAT_MCB_ENTRY	mcbEntry;

					VBO				vcn;
					LBO				lcn;
					//LCN			startingLcn;

												
					NdfsWinxpReplytHeader->Open.TruncateOnClose = BooleanFlagOn( moveFcb->FcbState, FCB_STATE_TRUNCATE_ON_CLOSE );

					NdfsWinxpReplytHeader->FileSize8			= HTONLL(moveFcb->Header.FileSize.QuadPart);
					NdfsWinxpReplytHeader->AllocationSize8	= HTONLL(moveFcb->Header.AllocationSize.QuadPart);
					NdfsWinxpReplytHeader->ValidDataLength8	= HTONLL(moveFcb->Header.ValidDataLength.QuadPart);

					//ASSERT( !FlagOn(moveScb->AttributeFlags, ATTRIBUTE_FLAG_SPARSE) );

					DebugTrace2( 0, Dbg, ("session Scb->Header.ValidDataLength.QuadPart = %I64x\n", moveFcb->Header.ValidDataLength.QuadPart) );
				
					mcbEntry = (PNDFS_FAT_MCB_ENTRY)(NdfsWinxpReplytHeader+1);

					vcn = 0; index = 0;

					while (FatLookupMcbEntry(vcb, &moveFcb->Mcb, vcn, &lcn, &clusterCount, NULL)) {

						NDAS_ASSERT( (index+1)*sizeof(NDFS_FAT_MCB_ENTRY) <= PrimarySession->SessionContext.SecondaryMaxDataSize ); 

						ASSERT( ((UINT16)lcn & (0x1FF)) == 0 );

						mcbEntry[index].Vcn = vcn; 
						mcbEntry[index].Lcn = (UINT32)(lcn >> vcb->AllocationSupport.LogOfBytesPerSector); 
						mcbEntry[index].ClusterCount = clusterCount;

						vcn += clusterCount;
						index++;

						if (vcn == 0) {

							break;
						}
					}

					NDAS_ASSERT( vcn == 0 && index != 0 || vcn == moveFcb->Header.AllocationSize.QuadPart );

					NdfsWinxpReplytHeader->NumberOfMcbEntry4 = HTONL(index);

					NdfsWinxpReplytHeader->FileInformationSet = TRUE;
					*ReplyDataSize = index*sizeof( NDFS_FAT_MCB_ENTRY );

				} else {

					NdfsWinxpReplytHeader->FileInformationSet = FALSE;
					*ReplyDataSize = 0;
				}
			}

			status = STATUS_SUCCESS;
		}

		break;
	}


	case IRP_MJ_DEVICE_CONTROL: { // 0x0E
	//	case IRP_MJ_INTERNAL_DEVICE_CONTROL:  // 0x0F 
	
		NTSTATUS		deviceControlStatus;

		UINT64			openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE		openFile;

		IO_STATUS_BLOCK	ioStatusBlock;
		ULONG			ioControlCode;
		PVOID			inputBuffer;
		ULONG			inputBufferLength;
		PVOID			outputBuffer;
		ULONG			outputBufferLength;
 		
		DbgBreakPoint();

		openFile = PrimarySession_FindOpenFile( PrimarySession,
												openFileId );

		ASSERT( openFile && openFile->FileObject );

		ioControlCode		= NdfsWinxpRequestHeader->DeviceIoControl.IoControlCode;
		inputBuffer			= ndfsRequestData;
		inputBufferLength	= NdfsWinxpRequestHeader->DeviceIoControl.InputBufferLength;
		outputBuffer		= (UINT8 *)(NdfsWinxpReplytHeader+1);
		outputBufferLength	= NdfsWinxpRequestHeader->DeviceIoControl.OutputBufferLength;


		RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

		deviceControlStatus = ZwDeviceIoControlFile( openFile->FileHandle,
													 NULL,
													 NULL,
													 NULL,
													 &ioStatusBlock,
													 ioControlCode,
													 inputBuffer,
													 inputBufferLength,
													 outputBuffer,
													 outputBufferLength );

		DebugTrace2( 0, Dbg,
						("DispatchWinXpRequest: IRP_MJ_DEVICE_CONTROL: CtrlCode:%l08x openFileId = %X, deviceControlStatus = %X, ioStatusBlock.Status = %X, ioStatusBlock.Information = %d\n",
										ioControlCode, openFileId, deviceControlStatus, ioStatusBlock.Status, ioStatusBlock.Information) );
		
		if (NT_SUCCESS(deviceControlStatus)) {

			ASSERT( deviceControlStatus == STATUS_SUCCESS );		
			ASSERT( deviceControlStatus == ioStatusBlock.Status );
		}

		if (deviceControlStatus == STATUS_BUFFER_OVERFLOW)
			ASSERT( ioStatusBlock.Information == inputBufferLength || ioStatusBlock.Information == outputBufferLength );

		if (!(deviceControlStatus == STATUS_SUCCESS || deviceControlStatus == STATUS_BUFFER_OVERFLOW)) {

			ioStatusBlock.Information = 0;
			ASSERT(ioStatusBlock.Information == 0);
		}

		//NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status4	  = HTONL(deviceControlStatus);
		NdfsWinxpReplytHeader->Information32 = HTONL((UINT32)ioStatusBlock.Information);

		if (ioStatusBlock.Information)
			ASSERT(outputBufferLength);

		if (outputBufferLength) {

			*ReplyDataSize = (UINT32)ioStatusBlock.Information;
			//*replyDataSize = NdfsWinxpRequestHeader->DeviceIoControl.OutputBufferLength <= ioStatusBlock.Information 
			//					? NdfsWinxpRequestHeader->DeviceIoControl.OutputBufferLength : ioStatusBlock.Information;
		
		} else {

			*ReplyDataSize = 0;
		}

		status = STATUS_SUCCESS;

		break;
	}

	case IRP_MJ_LOCK_CONTROL: { // 0x11
	
		NTSTATUS			lockControlStatus;

		UINT64				openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE			openFile;
		
		IO_STATUS_BLOCK		ioStatusBlock;
		LARGE_INTEGER		byteOffset;
		LARGE_INTEGER		length;
		ULONG				key;
		BOOLEAN				failImmediately;
		BOOLEAN				exclusiveLock;
		
		
		DbgBreakPoint();

		openFile = PrimarySession_FindOpenFile( PrimarySession, openFileId );
		
		if (openFileId == 0) {

			DbgBreakPoint();
			lockControlStatus = STATUS_UNSUCCESSFUL;
			status = STATUS_SUCCESS;

		} else {

			ASSERT( openFile->FileObject );

			byteOffset.QuadPart	= NdfsWinxpRequestHeader->LockControl.ByteOffset;
			length.QuadPart		= NdfsWinxpRequestHeader->LockControl.Length;
			key					= NdfsWinxpRequestHeader->LockControl.Key;

	        failImmediately		= BooleanFlagOn(NdfsWinxpRequestHeader->IrpSpFlags, SL_FAIL_IMMEDIATELY);
			exclusiveLock		= BooleanFlagOn(NdfsWinxpRequestHeader->IrpSpFlags, SL_EXCLUSIVE_LOCK);

			DebugTrace2( 0, Dbg, 
						 ("LFS: IRP_MJ_LOCK_CONTROL: Length:%I64d Key:%08lx Offset:%I64d Ime:%d Ex:%d\n",
						  length.QuadPart,
						  key,
						  byteOffset.QuadPart,
						  key,
						  failImmediately,
						  exclusiveLock) );

			RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

			if (NdfsWinxpRequestHeader->IrpMinorFunction == IRP_MN_LOCK) {

				lockControlStatus = NtLockFile( openFile->FileHandle,
												NULL,
												NULL,
												NULL,
												&ioStatusBlock,
												&byteOffset,
												&length,
												key,
												failImmediately,
												exclusiveLock );
		
			} else if (NdfsWinxpRequestHeader->IrpMinorFunction == IRP_MN_UNLOCK_SINGLE) {

				lockControlStatus = NtUnlockFile( openFile->FileHandle,
												  &ioStatusBlock,
												  &byteOffset,
												  &length,
												  key );		
		
			} else if (NdfsWinxpRequestHeader->IrpMinorFunction == IRP_MN_UNLOCK_ALL) {

				lockControlStatus = STATUS_SUCCESS;
				//lockControlStatus = STATUS_NOT_IMPLEMENTED;
			
			} else {

				ASSERT(NDASFAT_BUG);
				lockControlStatus = STATUS_NOT_IMPLEMENTED;
			}
		}

		DebugTrace2( 0, Dbg,
					("DispatchWinXpRequest: NtLockFile: openFileId = %X, LockControlStatus = %X, ioStatusBlock.Status = %X, ioStatusBlock.Information = %d\n",
					  openFileId, lockControlStatus, ioStatusBlock.Status, ioStatusBlock.Information));

		
		//NdfsWinxpReplytHeader->IrpTag4			 = NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status4	  = HTONL(lockControlStatus);

		if (NT_SUCCESS(lockControlStatus)) {

			ASSERT(lockControlStatus == ioStatusBlock.Status);
			NdfsWinxpReplytHeader->Information32 = HTONL((UINT32)ioStatusBlock.Information);
			
		} else
			NdfsWinxpReplytHeader->Information32 = 0;

		*ReplyDataSize = 0;
		
		status = STATUS_SUCCESS;

		break;
	}

	case IRP_MJ_CLEANUP: { // 0x12 
	
		NTSTATUS	cleanupStatus;
		UINT64		openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE	openFile;


		openFile = PrimarySession_FindOpenFile( PrimarySession, openFileId );
		
		if (openFile == NULL) {

			ASSERT( FALSE );
			cleanupStatus = STATUS_UNSUCCESSFUL;
			status = STATUS_SUCCESS;

		} else {

			TYPE_OF_OPEN	typeOfOpen;
			PVCB			vcb;
			PFCB			fcb;
			//PSCB			scb;
			PCCB			ccb;


			ASSERT( openFile->FileObject );

			typeOfOpen = FatDecodeFileObject( openFile->FileObject, &vcb, &fcb, &ccb );
		
#if 0
			if (typeOfOpen != UserFileOpen) {

				cleanupStatus = STATUS_SUCCESS;
				status = STATUS_SUCCESS;
			
			} else 
#endif

			{			
				PIRP				irp;
				PFILE_OBJECT		fileObject;
				PDEVICE_OBJECT		deviceObject;
				KPROCESSOR_MODE		requestorMode;
				PIO_STACK_LOCATION	irpSp;
				BOOLEAN				synchronousIo;
				PKEVENT				eventObject = (PKEVENT) NULL;
				ULONG				keyValue = 0;
				LARGE_INTEGER		fileOffset = {0,0};
				PULONG				majorFunction;
				PETHREAD			currentThread;
				KEVENT				event;

				PIRP					topLevelIrp;
				PRIMARY_REQUEST_INFO	primaryRequestInfo;

				if (NdfsWinxpRequestHeader->CleanUp.FileSize != 0) {

					ASSERT( fcb->Header.FileSize.LowPart == NdfsWinxpRequestHeader->CleanUp.FileSize );
				}

				NDAS_ASSERT( NdfsWinxpRequestHeader->CleanUp.FileSize <= NdfsWinxpRequestHeader->CleanUp.AllocationSize );

				do {

					synchronousIo = openFile->FileObject ? BooleanFlagOn(openFile->FileObject->Flags, FO_SYNCHRONOUS_IO) : TRUE;
					ASSERT( synchronousIo == TRUE );

					deviceObject = &PrimarySession->VolDo->DeviceObject;
					fileObject = openFile->FileObject;
					currentThread = PsGetCurrentThread ();
					ASSERT( deviceObject->StackSize >= 1 );
					irp = IoAllocateIrp( deviceObject->StackSize, TRUE );
					requestorMode = KernelMode;
				
					if (!irp) {

						ASSERT( NDASFAT_INSUFFICIENT_RESOURCES );
						status = STATUS_INSUFFICIENT_RESOURCES;
					}

					irp->Tail.Overlay.OriginalFileObject = fileObject;
					irp->Tail.Overlay.Thread = currentThread;
					irp->Tail.Overlay.AuxiliaryBuffer = (PVOID) NULL;
					irp->RequestorMode = requestorMode;
					irp->PendingReturned = FALSE;
					irp->Cancel = FALSE;
					irp->CancelRoutine = (PDRIVER_CANCEL) NULL;
	
					irp->UserEvent = eventObject;
					irp->UserIosb = NULL; //&ioStatusBlock;
					irp->Overlay.AsynchronousParameters.UserApcRoutine = NULL; //ApcRoutine;
					irp->Overlay.AsynchronousParameters.UserApcContext = NULL; //ApcContext;


					//RtlZeroMemory(&currentIrpSp, sizeof(currentIrpSp));
					KeInitializeEvent( &event, NotificationEvent, FALSE );
			
					IoSetCompletionRoutine( irp,
											PrimaryCompletionRoutine,
											&event,
											TRUE,
											TRUE,
											TRUE );

					IoSetNextIrpStackLocation( irp );
					irpSp = IoGetCurrentIrpStackLocation( irp ); // = &currentIrpSp; // = IoGetNextIrpStackLocation( irp );
					majorFunction = (PULONG) (&irpSp->MajorFunction);
					*majorFunction = IRP_MJ_CLEANUP;
					irpSp->Control = (SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL);
					irpSp->MinorFunction = NdfsWinxpRequestHeader->IrpMinorFunction;
					irpSp->FileObject = fileObject;
					irpSp->DeviceObject = deviceObject;
					irp->AssociatedIrp.SystemBuffer = (PVOID) NULL;
					irp->MdlAddress = (PMDL) NULL;


					primaryRequestInfo.PrimaryTag			  = 0xe2027482;
					primaryRequestInfo.PrimarySessionId		  = PrimarySession->PrimarySessionId;
					primaryRequestInfo.PrimarySession		  = PrimarySession;
					primaryRequestInfo.NdfsWinxpRequestHeader = NdfsWinxpRequestHeader;

					topLevelIrp = IoGetTopLevelIrp();
					ASSERT( topLevelIrp == NULL );
					IoSetTopLevelIrp( (PIRP)&primaryRequestInfo );

					cleanupStatus = FatFsdCleanup( PrimarySession->VolDo, irp );
				
					if (cleanupStatus == STATUS_PENDING) {

						KeWaitForSingleObject( &event,
											   Executive,
											   KernelMode,
											   FALSE,
											   NULL );

					}

					IoSetTopLevelIrp( topLevelIrp );

					cleanupStatus = irp->IoStatus.Status;
					ASSERT( cleanupStatus == STATUS_SUCCESS );

					status = STATUS_SUCCESS;

					if (irp->MdlAddress != NULL) {

						MmUnlockPages( irp->MdlAddress );
						IoFreeMdl( irp->MdlAddress );
					}

					IoFreeIrp( irp );
				
				} while (0);
			}

			openFile->CleanUp = TRUE;
		}
	
		if (status == STATUS_SUCCESS) {

			//NdfsWinxpReplytHeader->IrpTag4			 = NdfsWinxpRequestHeader->IrpTag4;
			NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
			NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

			NdfsWinxpReplytHeader->Status4	   = HTONL(cleanupStatus);
			NdfsWinxpReplytHeader->Information32 = 0;
		}

        *ReplyDataSize = 0;
		
		break;
	}

	case IRP_MJ_QUERY_SECURITY: { // 0x14

		NTSTATUS				querySecurityStatus;
		UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE				openFile;
		
		ULONG					length;
		SECURITY_INFORMATION	securityInformation;
		PSECURITY_DESCRIPTOR	securityDescriptor;
		ULONG					returnedLength = 0;


		openFile = PrimarySession_FindOpenFile( PrimarySession, openFileId );

		if (openFile == NULL) {

			ASSERT( FALSE );
			querySecurityStatus = STATUS_UNSUCCESSFUL;
			status = STATUS_SUCCESS;

		} else {

			ASSERT(openFile->FileObject );
			ASSERT( NdfsWinxpRequestHeader->QuerySecurity.Length <= PrimarySession->SessionContext.SecondaryMaxDataSize );

			length					= NdfsWinxpRequestHeader->QuerySecurity.Length;
			securityInformation		= NdfsWinxpRequestHeader->QuerySecurity.SecurityInformation;

			securityDescriptor		= (PSECURITY_DESCRIPTOR)(NdfsWinxpReplytHeader+1);

			DebugTrace2( 0, Dbg, ("IRP_MJ_QUERY_SECURITY: OutputBufferLength:%d\n", length) );

			querySecurityStatus = ZwQuerySecurityObject( openFile->FileHandle,
														 securityInformation,
														 securityDescriptor,
														 length,
														 &returnedLength );

			if (NT_SUCCESS(querySecurityStatus))
				ASSERT(querySecurityStatus == STATUS_SUCCESS);		

			if (querySecurityStatus == STATUS_SUCCESS || querySecurityStatus == STATUS_BUFFER_OVERFLOW) {

				NdfsWinxpReplytHeader->Information32 = HTONL(returnedLength);
				*ReplyDataSize = returnedLength;

			} else if (querySecurityStatus == STATUS_BUFFER_TOO_SMALL) {

				//
				//	Zw routines translate STATUS_BUFFER_OVERFLOW into STATUS_BUFFER_TOO_SMALL
				//	We have to get the status code back here.
				//
				querySecurityStatus = STATUS_BUFFER_OVERFLOW;
				NdfsWinxpReplytHeader->Information32 = HTONL(returnedLength); //NdfsWinxpRequestHeader->QuerySecurity.Length;
				*ReplyDataSize = 0; // NdfsWinxpRequestHeader->QuerySecurity.Length;
			
			} else {
		
				NdfsWinxpReplytHeader->Information32 = 0;
				*ReplyDataSize = 0;
			}
		}

		//NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction	= NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction	= NdfsWinxpRequestHeader->IrpMinorFunction;
		NdfsWinxpReplytHeader->Status4			= HTONL(querySecurityStatus);

		status = STATUS_SUCCESS;

		break;
	}

    case IRP_MJ_SET_SECURITY: { // 0x15

		NTSTATUS				setSecurityStatus;

		UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE				openFile;
		
	    PSECURITY_DESCRIPTOR	securityDescriptor;
		ULONG					length;
		SECURITY_INFORMATION	securityInformation;


		openFile = PrimarySession_FindOpenFile( PrimarySession, openFileId );

		if (openFile == NULL) {

			ASSERT( FALSE );
			setSecurityStatus = STATUS_UNSUCCESSFUL;
			status = STATUS_SUCCESS;

		} else {

			ASSERT(openFile->FileObject );

			securityInformation = NdfsWinxpRequestHeader->SetSecurity.SecurityInformation;
			length				= NdfsWinxpRequestHeader->SetSecurity.Length;
			securityDescriptor	= (PSECURITY_DESCRIPTOR)ndfsRequestData ;

			setSecurityStatus = ZwSetSecurityObject( openFile->FileHandle,
													 securityInformation,
													 securityDescriptor );

			DebugTrace2( 0, Dbg,
						("DispatchWinXpRequest: ZwSetSecurityObject: fileHandle = %X, securityInformation = %X, setSecurityStatus = %X\n",
						openFile->FileHandle, securityInformation, setSecurityStatus) );

			if (NT_SUCCESS(setSecurityStatus))
				ASSERT(setSecurityStatus == STATUS_SUCCESS);		
		}

		//NdfsWinxpReplytHeader->IrpTag4			 = NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status4	   = HTONL(setSecurityStatus);
		NdfsWinxpReplytHeader->Information32 = HTONL(length);

		*ReplyDataSize = 0;

		status = STATUS_SUCCESS;

		break;
	}
	
	case IRP_MJ_QUERY_EA: {

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


		openFile = PrimarySession_FindOpenFile( PrimarySession, openFileId );
		ASSERT( openFile && openFile->FileObject );
		//ASSERT(openFile && openFile->OpenFileId == (UINT32)openFile);

		buffer				= (UINT8 *)(NdfsWinxpReplytHeader+1);
		length				= NdfsWinxpRequestHeader->QueryEa.Length;
		returnSingleEntry	= (NdfsWinxpRequestHeader->IrpSpFlags & SL_RETURN_SINGLE_ENTRY) ? TRUE : FALSE;
		eaList				= ndfsRequestData;
		eaListLength		= NdfsWinxpRequestHeader->QueryEa.EaListLength;
		if (eaListLength == 0)
			eaList = NULL;
		eaIndex				= NdfsWinxpRequestHeader->QueryEa.EaIndex;
		restartScan			= (NdfsWinxpRequestHeader->IrpSpFlags & SL_RESTART_SCAN) ? TRUE : FALSE;

		DebugTrace2( 0, Dbg, 
					 ("LFS: DispatchWinXpRequest: IRP_MJ_QUERY_EA: length:%d\n", length) );

		indexSpecified = BooleanFlagOn(NdfsWinxpRequestHeader->IrpSpFlags, SL_INDEX_SPECIFIED);

		queryEaStatus = NtQueryEaFile( openFile->FileHandle,
									   &ioStatusBlock,
									   buffer,
									   length,
									   returnSingleEntry,
									   eaList,
									   eaListLength,
									   indexSpecified ? &eaIndex : NULL,
									   restartScan );
			
		DebugTrace2( 0, Dbg,
					("DispatchWinXpRequest: ZwQueryEaFile: fileHandle = %p, ioStatusBlock->Information = %d, queryEaStatus = %x\n",
						openFile->FileHandle, ioStatusBlock.Information, queryEaStatus) );

		//NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		if (NT_SUCCESS(queryEaStatus))
			ASSERT(queryEaStatus == STATUS_SUCCESS);		

		if (queryEaStatus == STATUS_SUCCESS ||
			queryEaStatus == STATUS_BUFFER_OVERFLOW) {

			NdfsWinxpReplytHeader->Information32 = HTONL((UINT32)ioStatusBlock.Information);
		
			if (ioStatusBlock.Information != 0) {

				PFILE_FULL_EA_INFORMATION	fileFullEa = (PFILE_FULL_EA_INFORMATION)buffer;
		
				*ReplyDataSize = 0;
			
				while (fileFullEa->NextEntryOffset) {

					*ReplyDataSize += fileFullEa->NextEntryOffset;
					fileFullEa = (PFILE_FULL_EA_INFORMATION)((UINT8 *)fileFullEa + fileFullEa->NextEntryOffset);
				}

				*ReplyDataSize += sizeof(FILE_FULL_EA_INFORMATION) - sizeof(CHAR) + fileFullEa->EaNameLength + fileFullEa->EaValueLength;

				DebugTrace2( 0, Dbg,
							 ("DispatchWinXpRequest, IRP_MJ_QUERY_EA: Ea is set QueryEa.Length = %d, inputBufferLength = %d\n",
							  NdfsWinxpRequestHeader->QueryEa.Length, *ReplyDataSize) );

				*ReplyDataSize = ((*ReplyDataSize < length) ? *ReplyDataSize : length);
			}
		
		} else if (queryEaStatus == STATUS_BUFFER_TOO_SMALL) {

			//
			//	Zw routines translate STATUS_BUFFER_OVERFLOW into STATUS_BUFFER_TOO_SMALL
			//	We have to get the status code back here.
			//
			queryEaStatus = STATUS_BUFFER_OVERFLOW ;
			NdfsWinxpReplytHeader->Information32 = HTONL((UINT32)ioStatusBlock.Information); //NdfsWinxpRequestHeader->QuerySecurity.Length;
			*ReplyDataSize = 0; // NdfsWinxpRequestHeader->QuerySecurity.Length;
	
		} else {

			NdfsWinxpReplytHeader->Information32 = 0;
			*ReplyDataSize = 0;
		}

		//ASSERT(NdfsWinxpRequestHeader->QuerySecurity.Length >= returnedLength);
		NdfsWinxpReplytHeader->Status4	  = HTONL(queryEaStatus);

		status = STATUS_SUCCESS;

		break;
	}

    case IRP_MJ_SET_EA: {

		NTSTATUS				setEaStatus;
		IO_STATUS_BLOCK			ioStatusBlock;

		UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE				openFile;
		ULONG					length;


		openFile = PrimarySession_FindOpenFile( PrimarySession, openFileId );

		ASSERT( openFile && openFile->FileObject );
		//ASSERT(openFile && openFile->OpenFileId == (UINT32)openFile);

		length = NdfsWinxpRequestHeader->SetEa.Length;

		setEaStatus = NtSetEaFile(
							openFile->FileHandle,
							&ioStatusBlock,
							ndfsRequestData,
							length
							);
		DebugTrace2( 0, Dbg,
					("DispatchWinXpRequest: ZwSetEaFile: fileHandle = %p, ioStatusBlock->Information = %d, setEaStatus = %x\n",
						openFile->FileHandle, ioStatusBlock.Information, setEaStatus));

		if (NT_SUCCESS(setEaStatus))
			ASSERT(setEaStatus == STATUS_SUCCESS);		

		//NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status4	  = HTONL(setEaStatus);
		NdfsWinxpReplytHeader->Information32 = HTONL((UINT32)ioStatusBlock.Information);

		*ReplyDataSize = 0;

		status = STATUS_SUCCESS;

		break;
	}

	case IRP_MJ_QUERY_QUOTA: {

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
		NdfsWinxpReplytHeader->Information = 0;
		*replyDataSize = 0;
		NdfsWinxpReplytHeader->Status4	  = HTONL(STATUS_NOT_SUPPORTED);
		status = STATUS_SUCCESS;

		break;
#endif

		status = STATUS_SUCCESS;
		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (UINT32)openFile);

		outputBufferLength		= NdfsWinxpRequestHeader->QueryQuota.Length;
		if (outputBufferLength)
			outputBuffer			= (UINT8 *)(NdfsWinxpReplytHeader+1);
		else
			outputBuffer			= NULL ;

		inputBufferLength		= NdfsWinxpRequestHeader->QueryQuota.InputLength ;
		if (inputBufferLength)
			inputBuffer				= ndfsRequestData;
		else
			inputBuffer				= NULL;

		if (NdfsWinxpRequestHeader->QueryQuota.StartSidOffset)
			startSid			= (PCHAR)ndfsRequestData + NdfsWinxpRequestHeader->QueryQuota.StartSidOffset;
		else
			startSid			= NULL;

		restartScan				= (NdfsWinxpRequestHeader->IrpSpFlags & SL_RESTART_SCAN) ? TRUE : FALSE;
		returnSingleEntry		= (NdfsWinxpRequestHeader->IrpSpFlags & SL_RETURN_SINGLE_ENTRY) ? TRUE : FALSE;

		DebugTrace2( 0, Dbg, 
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

		DebugTrace2( 0, Dbg,
					("DispatchWinXpRequest: NtQueryQuotaInformationFile: fileHandle = %X, ioStatusBlock->Information = %X, queryQuotaStatus = %X\n",
						openFile->FileHandle, ioStatusBlock.Information, queryQuotaStatus));

		//NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

		if (NT_SUCCESS(queryQuotaStatus))
			ASSERT(queryQuotaStatus == STATUS_SUCCESS);		

		if ( queryQuotaStatus == STATUS_SUCCESS ||
			queryQuotaStatus == STATUS_BUFFER_OVERFLOW) 
		{
			NdfsWinxpReplytHeader->Information32 = HTONL((UINT32)ioStatusBlock.Information);
			*ReplyDataSize = (UINT32)ioStatusBlock.Information;

		} else if (queryQuotaStatus == STATUS_BUFFER_TOO_SMALL) 
		{
			//
			//	Zw routines translate STATUS_BUFFER_OVERFLOW into STATUS_BUFFER_TOO_SMALL
			//	We have to get the status code back here.
			//
			queryQuotaStatus = STATUS_BUFFER_OVERFLOW ;
			NdfsWinxpReplytHeader->Information32 = HTONL((UINT32)ioStatusBlock.Information); //NdfsWinxpRequestHeader->QuerySecurity.Length;
			*ReplyDataSize = 0; // NdfsWinxpRequestHeader->QuerySecurity.Length;
		} else {
			NdfsWinxpReplytHeader->Information32 = 0;
			*ReplyDataSize = 0;
		}

		//ASSERT(NdfsWinxpRequestHeader->QuerySecurity.Length >= returnedLength);
		NdfsWinxpReplytHeader->Status4	  = HTONL(queryQuotaStatus);

		status = STATUS_SUCCESS;

		break ;
	}

	case IRP_MJ_SET_QUOTA: {

		NTSTATUS				setQuotaStatus;
		IO_STATUS_BLOCK			ioStatusBlock;

		UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
		POPEN_FILE				openFile;
		ULONG					length;

#if 0
		NdfsWinxpReplytHeader->Information = 0;
		*replyDataSize = 0;
		NdfsWinxpReplytHeader->Status	  = HTONL(STATUS_NOT_SUPPORTED);
		status = STATUS_SUCCESS;

		break;
#endif

		openFile = PrimarySession_FindOpenFile(
							PrimarySession,
							openFileId
							);
		ASSERT(openFile && openFile->FileObject);
		//ASSERT(openFile && openFile->OpenFileId == (UINT32)openFile);

		length				= NdfsWinxpRequestHeader->SetQuota.Length;

		setQuotaStatus = NtSetQuotaInformationFile(
									openFile->FileHandle,
									&ioStatusBlock,
									ndfsRequestData,
									length
									);

		DebugTrace2( 0, Dbg,
					("DispatchWinXpRequest: NtSetQuotaInformationFile: fileHandle = %X, ioStatusBlock.Information = %X, setQuotaStatus = %X\n",
						openFile->FileHandle, ioStatusBlock.Information, setQuotaStatus));

		if (NT_SUCCESS(setQuotaStatus))
			ASSERT(setQuotaStatus == STATUS_SUCCESS);		

		//NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction = NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction = NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status4	  = HTONL(setQuotaStatus);
		NdfsWinxpReplytHeader->Information32 = HTONL((UINT32)ioStatusBlock.Information);

		*ReplyDataSize = 0;

		status = STATUS_SUCCESS;

		break;
	}

	default:

		ASSERT( NDASFAT_BUG );
		status = STATUS_UNSUCCESSFUL;

		break;
	}

	if (!(NTOHL(NdfsWinxpReplytHeader->Status4) == STATUS_SUCCESS || 
		 NTOHL(NdfsWinxpReplytHeader->Status4) == STATUS_BUFFER_OVERFLOW)) {

		ASSERT( *ReplyDataSize == 0 );
	}

	ASSERT( NTOHL(NdfsWinxpReplytHeader->Status4) != STATUS_INVALID_HANDLE );
	ASSERT (NTOHL(NdfsWinxpReplytHeader->Status4) != STATUS_PENDING );

	return status;	
}


static 
NTSTATUS
PrimarySession_IrpMjCreate (
	IN  PPRIMARY_SESSION			PrimarySession,
	IN  PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader,
	IN  PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplytHeader,
	IN  UINT32						RequestDataSize,
	IN  UINT8						*NdfsRequestData,
	OUT	UINT32						*ReplyDataSize
	)
{
	NTSTATUS			status;

	UNICODE_STRING		fileName;
	PWCHAR				fileNameBuffer = NULL;

	HANDLE				fileHandle = NULL;
    PFILE_OBJECT		fileObject = NULL;
	HANDLE				eventHandle = NULL;

	NTSTATUS			createStatus;

	ACCESS_MASK			desiredAccess;
	ULONG				attributes;
	OBJECT_ATTRIBUTES	objectAttributes;
	IO_STATUS_BLOCK		ioStatusBlock;
	LARGE_INTEGER		allocationSize;
	ULONG				fileAttributes;
    ULONG				shareAccess;
    ULONG				disposition;
	ULONG				createOptions;
    PCHAR				eaBuffer;
	ULONG				eaLength;
    CREATE_FILE_TYPE	createFileType;
	ULONG				options;

	POPEN_FILE			openFile = NULL;
	UINT64				relatedOpenFileId = NdfsWinxpRequestHeader->Create.RelatedFileHandle;
	POPEN_FILE			relatedOpenFile = NULL;

	PIRP					topLevelIrp;
	PRIMARY_REQUEST_INFO	primaryRequestInfo;


	UNREFERENCED_PARAMETER( RequestDataSize );

	do {

		fileNameBuffer = FsRtlAllocatePoolWithTag( PagedPool, NDFS_MAX_PATH*sizeof(WCHAR), TAG_FILENAME_BUFFER );
		RtlInitEmptyUnicodeString( &fileName, fileNameBuffer, NDFS_MAX_PATH*sizeof(WCHAR) );

		if (relatedOpenFileId) {

			relatedOpenFile = PrimarySession_FindOpenFile( PrimarySession, relatedOpenFileId );
#if defined(_WIN64)
			ASSERT( relatedOpenFile && relatedOpenFile->OpenFileId == (UINT64)relatedOpenFile );
#else
			ASSERT( relatedOpenFile && relatedOpenFile->OpenFileId == (UINT32)relatedOpenFile );
#endif
		    
			DebugTrace2( 0, Dbg, ("PrimarySession_IrpMjCreate: relatedOpenFile->OpenFileId = %X\n", relatedOpenFile->OpenFileId) );
		
		} else {

			status = RtlAppendUnicodeStringToString( &fileName,
												     &PrimarySession->NetdiskPartitionInformation.VolumeName );

			if (status != STATUS_SUCCESS) {

				ASSERT(NDASFAT_UNEXPECTED);
				break;
			}
		}

		if (NdfsWinxpRequestHeader->Create.FileNameLength) {

			status = RtlAppendUnicodeToString( &fileName,
											   (PWCHAR)&NdfsRequestData[NdfsWinxpRequestHeader->Create.EaLength] );

			if (status != STATUS_SUCCESS) {

				ASSERT( NDASFAT_UNEXPECTED );
				break;
			}
		}

	    DebugTrace2( 0, Dbg, ("PrimarySession_IrpMjCreate: fileName = %wZ\n", &fileName) );
		
		desiredAccess = NdfsWinxpRequestHeader->Create.SecurityContext.DesiredAccess;

		attributes =  OBJ_KERNEL_HANDLE;
		
		if (!(NdfsWinxpRequestHeader->IrpSpFlags & SL_CASE_SENSITIVE))
			attributes |= OBJ_CASE_INSENSITIVE;

		InitializeObjectAttributes(	&objectAttributes,
									&fileName,
									attributes,
									relatedOpenFileId ? relatedOpenFile->FileHandle : NULL,
									NULL );
		
		allocationSize.QuadPart  = NdfsWinxpRequestHeader->Create.AllocationSize;

		fileAttributes			= NdfsWinxpRequestHeader->Create.FileAttributes;		
		shareAccess				= NdfsWinxpRequestHeader->Create.ShareAccess;
		disposition				= (NdfsWinxpRequestHeader->Create.Options & 0xFF000000) >> 24;
		createOptions			= NdfsWinxpRequestHeader->Create.Options & 0x00FFFFFF;
		createOptions			|= (NdfsWinxpRequestHeader->Create.SecurityContext.FullCreateOptions & FILE_DELETE_ON_CLOSE);
		eaBuffer				= &NdfsRequestData[0];
		eaLength				= NdfsWinxpRequestHeader->Create.EaLength;
		createFileType			= CreateFileTypeNone;
		options					= NdfsWinxpRequestHeader->IrpSpFlags & 0x000000FF;
		
		RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

		DebugTrace2( 0, Dbg, ("PrimarySession_IrpMjCreate: DesiredAccess = %X. Synchronize:%d "
							  "Dispo:%02lX CreateOptions = %X. Synchronize: Alert=%d Non-Alert=%d EaLen:%d\n",
							  desiredAccess, (desiredAccess & SYNCHRONIZE) != 0, disposition,
							  createOptions,
							  (createOptions & FILE_SYNCHRONOUS_IO_ALERT) != 0,
							  (createOptions & FILE_SYNCHRONOUS_IO_NONALERT) != 0,
							  eaLength) );

		if (NdfsWinxpRequestHeader->Create.FileNameLength == 0) {

			DebugTrace2( 0, Dbg, ("PrimarySession_IrpMjCreate: desiredAccess = %x, fileAtributes %x, "
								  "shareAccess %x, disposition %x, createOptions %x, createFileType %x\n",
								   desiredAccess, fileAttributes, shareAccess, disposition, createOptions, createFileType) );
		}

		desiredAccess |= SYNCHRONIZE;

		if (!(createOptions & FILE_SYNCHRONOUS_IO_NONALERT))
			createOptions |= FILE_SYNCHRONOUS_IO_ALERT;

		primaryRequestInfo.PrimaryTag			  = 0xe2027482;
		primaryRequestInfo.PrimarySessionId		  = PrimarySession->PrimarySessionId;
		primaryRequestInfo.PrimarySession		  = PrimarySession;
		primaryRequestInfo.NdfsWinxpRequestHeader = NdfsWinxpRequestHeader;

		topLevelIrp = IoGetTopLevelIrp();
		ASSERT( topLevelIrp == NULL );
		IoSetTopLevelIrp( (PIRP)&primaryRequestInfo );

		createStatus = IoCreateFile( &fileHandle,
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

		IoSetTopLevelIrp( topLevelIrp );

		DebugTrace2( 0, Dbg, ("createStatus = %x\n", createStatus) );

		if (NT_SUCCESS(createStatus)) {

			if (createStatus != STATUS_SUCCESS || createStatus != ioStatusBlock.Status) {

				DbgBreakPoint();
			}

			createStatus = ioStatusBlock.Status;
		}

		if (NT_SUCCESS(createStatus)) {

			ASSERT( fileHandle != 0 );
			ASSERT( createStatus == ioStatusBlock.Status );

			openFile = PrimarySession_AllocateOpenFile( PrimarySession, fileHandle, NULL );

			if (openFile == NULL) {
			
				NDAS_ASSERT( NDAS_ASSERT_INSUFFICIENT_RESOURCES );
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

				createStatus = status;
				ASSERT( NDASFAT_UNEXPECTED );
				PrimarySession_CloseFile( PrimarySession, fileHandle );
				break;
			}

			openFile->FileObject = fileObject;

			status = ZwCreateEvent( &eventHandle, GENERIC_READ, NULL, SynchronizationEvent, FALSE );

			if (status != STATUS_SUCCESS) {

				ASSERT( NDASFAT_UNEXPECTED );
				break;
			}

			openFile->EventHandle = eventHandle;
			openFile->CleanUp = FALSE;
		}

		NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
		NdfsWinxpReplytHeader->IrpMajorFunction	= NdfsWinxpRequestHeader->IrpMajorFunction;
		NdfsWinxpReplytHeader->IrpMinorFunction	= NdfsWinxpRequestHeader->IrpMinorFunction;

		NdfsWinxpReplytHeader->Status4			= HTONL(createStatus);
		NdfsWinxpReplytHeader->Information32		= HTONL((UINT32)ioStatusBlock.Information);

		DebugTrace2( 0, Dbg, ("PrimarySession_IrpMjCreate: fileName = %wZ\n", &fileName) );

		if (NT_SUCCESS(createStatus)) {

		    TYPE_OF_OPEN	typeOfOpen;
		    PVCB			vcb;
			PFCB			fcb;
			//PSCB			scb;
			PCCB			ccb;
		
			typeOfOpen = FatDecodeFileObject( openFile->FileObject, &vcb, &fcb, &ccb );
			
			//ASSERT( fcb->LinkCount == 1 );

			//DebugTrace2( 0, Dbg, ("System_File = %d, Scb->Header.PagingIoResource = %p\n", 
			//					  BooleanFlagOn( fcb->FcbState, FCB_STATE_SYSTEM_FILE), fcb->Header.PagingIoResource) );
			
			ASSERT( typeOfOpen );

			NdfsWinxpReplytHeader->Open.FileHandle			= openFile->OpenFileId;
			//NdfsWinxpReplytHeader->Open.ScbHandle			= (UINT32)scb;
			NdfsWinxpReplytHeader->Open.TypeOfOpen			= typeOfOpen;
			//NdfsWinxpReplytHeader->Open.AttributeTypeCode	= scb->AttributeTypeCode;

			if (ccb) {

				NdfsWinxpReplytHeader->Open.CcbFlags			= ccb->Flags;
			}

			if (fcb) {

#if defined(_WIN64)
				NdfsWinxpReplytHeader->Open.FcbHandle			= (UINT64)fcb;
#else
				NdfsWinxpReplytHeader->Open.FcbHandle			= (UINT32)fcb;
#endif

				NdfsWinxpReplytHeader->Open.CreationTime		= fcb->CreationTime.QuadPart;


			} else if (vcb) {

				ASSERT( typeOfOpen == UserVolumeOpen );
#if defined(_WIN64)
				NdfsWinxpReplytHeader->Open.FcbHandle			= (UINT64)vcb;
#else
				NdfsWinxpReplytHeader->Open.FcbHandle			= (UINT32)vcb;
#endif
			} else {
				
				ASSERT( typeOfOpen == UnopenedFileObject );
				if (!FlagOn(createOptions, FILE_OPEN_REPARSE_POINT))
					DbgBreakPoint();
			}
#if 0
			NdfsWinxpReplytHeader->Open.FileReference.SegmentNumberLowPart	= fcb->FileReference.SegmentNumberLowPart;
			NdfsWinxpReplytHeader->Open.FileReference.SegmentNumberHighPart = fcb->FileReference.SegmentNumberHighPart;
			NdfsWinxpReplytHeader->Open.FileReference.SequenceNumber		= fcb->FileReference.SequenceNumber;

			if (ccb->Lcb) {
			
				NdfsWinxpReplytHeader->Open.Lcb.LcbState	= ccb->Lcb->LcbState;
				NdfsWinxpReplytHeader->Open.Lcb.Scb			= (ULONG)ccb->Lcb->Scb;
				NdfsWinxpReplytHeader->Open.Lcb.Fcb			= (ULONG)ccb->Lcb->Fcb;
			
			} else {

				ASSERT( typeOfOpen == UserVolumeOpen );
			}

			NdfsWinxpReplytHeader->DuplicatedInformation.AllocatedLength		= fcb->Info.AllocatedLength;
			NdfsWinxpReplytHeader->DuplicatedInformation.CreationTime			= fcb->Info.CreationTime;
			NdfsWinxpReplytHeader->DuplicatedInformation.FileAttributes			= fcb->Info.FileAttributes;
			NdfsWinxpReplytHeader->DuplicatedInformation.FileSize				= fcb->Info.FileSize;
			NdfsWinxpReplytHeader->DuplicatedInformation.LastAccessTime			= fcb->Info.LastAccessTime;
			NdfsWinxpReplytHeader->DuplicatedInformation.LastChangeTime			= fcb->Info.LastChangeTime;
			NdfsWinxpReplytHeader->DuplicatedInformation.LastModificationTime	= fcb->Info.LastModificationTime;
			NdfsWinxpReplytHeader->DuplicatedInformation.ReparsePointTag		= fcb->Info.ReparsePointTag;
#endif

			switch (typeOfOpen) {

			case UserFileOpen: {
			
				ULONG			index;
				ULONG			clusterCount;
				PNDFS_FAT_MCB_ENTRY	mcbEntry;

				VBO				vcn;
				LBO				lcn;
				//LCN			startingLcn;

												
				NdfsWinxpReplytHeader->Open.TruncateOnClose = BooleanFlagOn( fcb->FcbState, FCB_STATE_TRUNCATE_ON_CLOSE );

				NDAS_ASSERT( fcb->Header.FileSize.QuadPart <= fcb->Header.AllocationSize.QuadPart );

				NdfsWinxpReplytHeader->FileSize8		= HTONLL(fcb->Header.FileSize.QuadPart);
				NdfsWinxpReplytHeader->AllocationSize8	= HTONLL(fcb->Header.AllocationSize.QuadPart);
				NdfsWinxpReplytHeader->ValidDataLength8	= HTONLL(fcb->Header.ValidDataLength.QuadPart);

				//ASSERT( !FlagOn(scb->AttributeFlags, ATTRIBUTE_FLAG_SPARSE) );

				DebugTrace2( 0, Dbg, ("session Scb->Header.ValidDataLength.QuadPart = %I64x\n", fcb->Header.ValidDataLength.QuadPart) );
				
				mcbEntry = (PNDFS_FAT_MCB_ENTRY)(NdfsWinxpReplytHeader+1);

				vcn = 0; index = 0;

				while (FatLookupMcbEntry(vcb, &fcb->Mcb, vcn, &lcn, &clusterCount, NULL)) {

					NDAS_ASSERT( (index+1)*sizeof(NDFS_FAT_MCB_ENTRY) <= PrimarySession->SessionContext.SecondaryMaxDataSize );

					ASSERT( ((UINT16)lcn & (0x1FF)) == 0 );

					mcbEntry[index].Vcn = vcn; 
					mcbEntry[index].Lcn = (UINT32)(lcn >> vcb->AllocationSupport.LogOfBytesPerSector); 
					mcbEntry[index].ClusterCount = clusterCount;

					vcn += clusterCount;
					index++;

					if (vcn == 0) {

						break;
					}
				}

				NDAS_ASSERT( vcn == 0 && index != 0 || vcn == fcb->Header.AllocationSize.QuadPart );

				NdfsWinxpReplytHeader->NumberOfMcbEntry4 = HTONL(index);

				NdfsWinxpReplytHeader->FileInformationSet = TRUE;
				*ReplyDataSize = index*sizeof( NDFS_FAT_MCB_ENTRY );

				break;
			}

			case UserDirectoryOpen:

				NdfsWinxpReplytHeader->FileInformationSet = FALSE;
				*ReplyDataSize = 0;
				break;

			case UserVolumeOpen:

				NdfsWinxpReplytHeader->FileInformationSet = FALSE;
				*ReplyDataSize = 0;
				break;
#if 0
			case UserViewIndexOpen:

				NdfsWinxpReplytHeader->FileInformationSet = FALSE;
				*ReplyDataSize = 0;
				break;
#endif

			case UnopenedFileObject:

				NdfsWinxpReplytHeader->FileInformationSet = FALSE;
				*ReplyDataSize = 0;
				break;

			//case StreamFileOpen:
			default:

				ASSERT( NDASFAT_BUG );
				NdfsWinxpReplytHeader->FileInformationSet = FALSE;
				*ReplyDataSize = 0;
				break;
			}		
		
		} else {

			NdfsWinxpReplytHeader->FileInformationSet = FALSE;
			*ReplyDataSize = 0;
		}

		status = STATUS_SUCCESS;
	
	} while (0);

	if (status != STATUS_SUCCESS) {

		if (fileObject) {

			ObDereferenceObject( fileObject );
		}

		if (fileHandle) {

			PrimarySession_CloseFile( PrimarySession, fileHandle );
		}

		if (eventHandle) {

			ZwClose( eventHandle );
		}

	} else {

		if (NTOHL(NdfsWinxpReplytHeader->Status4) == STATUS_SUCCESS)
			ASSERT( NdfsWinxpReplytHeader->Open.TypeOfOpen );
	}

	if (fileNameBuffer)
		ExFreePool( fileNameBuffer );

	return status;
}


static NTSTATUS
PrimarySession_IrpMjClose (
	IN  PPRIMARY_SESSION			PrimarySession,
	IN  PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader,
	IN  PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplytHeader,
	IN  UINT32						DataSize,
	OUT	UINT32						*ReplyDataSize,
	UINT8								*NdfsRequestData
	)
{
	NTSTATUS	status;
	UINT64		openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
	POPEN_FILE	openFile;
	KIRQL		oldIrql;

	NTSTATUS	closeStatus;


	UNREFERENCED_PARAMETER( NdfsRequestData );
	UNREFERENCED_PARAMETER( DataSize );
		
	openFile = PrimarySession_FindOpenFile( PrimarySession, openFileId );
	
	if (openFile == NULL) {
		
		ASSERT( FALSE );
		closeStatus = STATUS_UNSUCCESSFUL;

	} else {

		DebugTrace2( 0, Dbg, 
					("openFile->FileObject->FileName = %Z openFile->FileObject = %p\n", 
					  &openFile->FileObject->FileName, openFile->FileObject) );

		ObDereferenceObject( openFile->FileObject );		
		openFile->FileObject = NULL;

		closeStatus = PrimarySession_CloseFile( PrimarySession, openFile->FileHandle );

		openFile->FileHandle = NULL;

		ASSERT( closeStatus == STATUS_SUCCESS );

		if (openFile->EventHandle) {

			closeStatus = ZwClose(openFile->EventHandle);
			openFile->EventHandle = NULL;
			ASSERT(closeStatus == STATUS_SUCCESS);
		}
	}

	KeAcquireSpinLock( &PrimarySession->Thread.OpenedFileQSpinLock, &oldIrql );
	RemoveEntryList( &openFile->ListEntry );
	KeReleaseSpinLock( &PrimarySession->Thread.OpenedFileQSpinLock, oldIrql );
		
	InitializeListHead( &openFile->ListEntry );
	PrimarySession_FreeOpenFile( PrimarySession, openFile );
		
	//NdfsWinxpReplytHeader->IrpTag4			 = NdfsWinxpRequestHeader->IrpTag4;
	NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
	NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

	NdfsWinxpReplytHeader->Status4		= NTOHL(closeStatus);
	NdfsWinxpReplytHeader->Information32	= 0;

	*ReplyDataSize = 0;
	status = STATUS_SUCCESS;

	DebugTrace2( 0, Dbg, ("Vcb->Vpb->ReferenceCount CLOSE = %d\n", PrimarySession->VolDo->Vcb.Vpb->ReferenceCount) );

	return status;
}


static NTSTATUS
PrimarySession_IrpMjRead(
	IN  PPRIMARY_SESSION			PrimarySession,
	IN  PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader,
	IN  PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplytHeader,
	IN  UINT32						DataSize,
	OUT	UINT32						*ReplyDataSize,
	UINT8								*NdfsRequestData
	)
{
	NTSTATUS				status;
	NTSTATUS				readStatus;

	UINT64					openFileId = NTOHLL(NdfsWinxpRequestHeader->FileHandle8);
	POPEN_FILE				openFile;

	HANDLE					fileHandle = NULL;	

	BOOLEAN					synchronousIo;
		
	IO_STATUS_BLOCK			ioStatusBlock;
	PVOID					buffer;
	ULONG					length;
	LARGE_INTEGER			byteOffset;
	ULONG					key;

	PIRP					topLevelIrp;
	PRIMARY_REQUEST_INFO	primaryRequestInfo;


	UNREFERENCED_PARAMETER( NdfsRequestData );
	UNREFERENCED_PARAMETER( DataSize );

	
	DebugTrace2( 0, Dbg, ("DispatchWinXpRequest: IRP_MJ_READ\n") );

	openFile = PrimarySession_FindOpenFile( PrimarySession, openFileId );
	
	if (openFile == NULL) {

		ASSERT( FALSE );
		readStatus = STATUS_UNSUCCESSFUL;
		status = STATUS_SUCCESS;

	} else {

		//ASSERT( !openFile->AlreadyClosed );
		ASSERT(openFile && openFile->FileObject);
		fileHandle = openFile->FileHandle;

		RtlZeroMemory(&ioStatusBlock, sizeof(ioStatusBlock));

		buffer				= (UINT8 *)(NdfsWinxpReplytHeader+1);

		length				= NdfsWinxpRequestHeader->Read.Length;
		byteOffset.QuadPart = NdfsWinxpRequestHeader->Read.ByteOffset;
		
		key = NdfsWinxpRequestHeader->Read.Key;
		synchronousIo = openFile->FileObject ? BooleanFlagOn(openFile->FileObject->Flags, FO_SYNCHRONOUS_IO) : TRUE;
	
		ASSERT(synchronousIo == TRUE);

		primaryRequestInfo.PrimaryTag			  = 0xe2027482;
		primaryRequestInfo.PrimarySessionId		  = PrimarySession->PrimarySessionId;
		primaryRequestInfo.PrimarySession		  = PrimarySession;
		primaryRequestInfo.NdfsWinxpRequestHeader = NdfsWinxpRequestHeader;

		topLevelIrp = IoGetTopLevelIrp();
		ASSERT( topLevelIrp == NULL );
		IoSetTopLevelIrp( (PIRP)&primaryRequestInfo );

		if (synchronousIo) {

			readStatus = NtReadFile( fileHandle,
									 NULL,
									 NULL,
									 NULL,
									 &ioStatusBlock,
									 buffer,
									 length,
									 &byteOffset,
									 key ? &key : NULL );
	
		} else {

			readStatus = NtReadFile( fileHandle,
									 openFile->EventHandle,
									 NULL,
									 NULL,
									 &ioStatusBlock,
									 buffer,
									 length,
									 &byteOffset,
									 key ? &key : NULL );
		
			if (readStatus == STATUS_PENDING) {

				readStatus = ZwWaitForSingleObject( openFile->EventHandle, TRUE, NULL );
			}
		}

		IoSetTopLevelIrp( topLevelIrp );

		DebugTrace2( 0, Dbg,
					("PrimarySession_IrpMjRead:openFileId = %X, synchronousIo = %d, length = %d, readStatus = %X, byteOffset = %I64d, ioStatusBlock.Status = %X, ioStatusBlock.Information = %d\n",
					openFileId, synchronousIo, length, readStatus, byteOffset.QuadPart, ioStatusBlock.Status, ioStatusBlock.Information) );

	}
	
	if (NT_SUCCESS(readStatus)) {

	} else {

		ASSERT( ioStatusBlock.Information == 0 );
		ioStatusBlock.Information = 0;
	}

	//NdfsWinxpReplytHeader->IrpTag4			= NdfsWinxpRequestHeader->IrpTag4;
	NdfsWinxpReplytHeader->IrpMajorFunction  = NdfsWinxpRequestHeader->IrpMajorFunction;
	NdfsWinxpReplytHeader->IrpMinorFunction  = NdfsWinxpRequestHeader->IrpMinorFunction;

	NdfsWinxpReplytHeader->Status4		= HTONL(readStatus);
	NdfsWinxpReplytHeader->Information32	= HTONL((UINT32)ioStatusBlock.Information);
	*ReplyDataSize = (UINT32)ioStatusBlock.Information;
	ASSERT(*ReplyDataSize <= NdfsWinxpRequestHeader->Read.Length);
		
	status = STATUS_SUCCESS;
	return status;
}


#define SAFELEN_ALIGNMENT_ADD		7 // for 4 bytes(long) alignment.

static ULONG
CalculateSafeLength (
	FILE_INFORMATION_CLASS	fileInformationClass,
	ULONG					requestLength,
	ULONG					returnedLength,
	PCHAR					Buffer
	) 
{
	ULONG	offset = 0;
	ULONG	safeAddSize = 0;


	switch (fileInformationClass) {

	case FileDirectoryInformation: {

		PFILE_DIRECTORY_INFORMATION	fileDirectoryInformation;

		while (offset < returnedLength) {

			fileDirectoryInformation = (PFILE_DIRECTORY_INFORMATION)(Buffer + offset);
			safeAddSize += SAFELEN_ALIGNMENT_ADD;

			if (fileDirectoryInformation->NextEntryOffset == 0)
				break ;

			offset += fileDirectoryInformation->NextEntryOffset;
		}

		break;
	}

	case FileFullDirectoryInformation: {

		PFILE_FULL_DIR_INFORMATION	fileFullDirInformation;

		while (offset < returnedLength) {

			fileFullDirInformation = (PFILE_FULL_DIR_INFORMATION)(Buffer + offset);
			safeAddSize += SAFELEN_ALIGNMENT_ADD;

			if (fileFullDirInformation->NextEntryOffset == 0)
				break ;

			offset += fileFullDirInformation->NextEntryOffset;
		}

		break;
	}

	case FileBothDirectoryInformation: {

		PFILE_BOTH_DIR_INFORMATION	fileBothDirInformation;

		while (offset < returnedLength) {

			fileBothDirInformation = (PFILE_BOTH_DIR_INFORMATION)(Buffer + offset);

			safeAddSize += SAFELEN_ALIGNMENT_ADD;

			if (!fileBothDirInformation->NextEntryOffset)
				break;

			offset += fileBothDirInformation->NextEntryOffset;
		}

		break;
	}

	case FileIdBothDirectoryInformation: {

		PFILE_ID_BOTH_DIR_INFORMATION	fileIdBothDirInformation;

		while (offset < returnedLength) {

			fileIdBothDirInformation = (PFILE_ID_BOTH_DIR_INFORMATION)(Buffer + offset) ;

			safeAddSize += SAFELEN_ALIGNMENT_ADD ;

			if (!fileIdBothDirInformation->NextEntryOffset)
				break ;

			offset += fileIdBothDirInformation->NextEntryOffset;
		}

		break;
	}

	case FileIdFullDirectoryInformation: {

		PFILE_ID_FULL_DIR_INFORMATION	fileIdFullDirInformation;

		while (offset < returnedLength) {

			fileIdFullDirInformation = (PFILE_ID_FULL_DIR_INFORMATION)(Buffer + offset) ;

			safeAddSize += SAFELEN_ALIGNMENT_ADD ;

			if (!fileIdFullDirInformation->NextEntryOffset)
				break ;

			offset += fileIdFullDirInformation->NextEntryOffset;
		}

		break;
	}

	case FileNamesInformation: {

		PFILE_NAMES_INFORMATION	fileNamesInformation;

		while(offset < returnedLength) 
		{
			fileNamesInformation = (PFILE_NAMES_INFORMATION)(Buffer + offset);

			safeAddSize += SAFELEN_ALIGNMENT_ADD ;

			if (!fileNamesInformation->NextEntryOffset)
				break ;

			offset += fileNamesInformation->NextEntryOffset;
		}
		break;
	}

	case FileStreamInformation: {

		PFILE_STREAM_INFORMATION	fileStreamInformation;

		while (offset < returnedLength) {

			fileStreamInformation = (PFILE_STREAM_INFORMATION)(Buffer + offset);

			safeAddSize += SAFELEN_ALIGNMENT_ADD ;

			if (!fileStreamInformation->NextEntryOffset)
				break;

			offset += fileStreamInformation->NextEntryOffset;
		}

		break;
	}

	case FileFullEaInformation: {

		PFILE_FULL_EA_INFORMATION	fileFullEaInformation;

		while (offset < returnedLength) {

			fileFullEaInformation = (PFILE_FULL_EA_INFORMATION)(Buffer + offset);

			safeAddSize += SAFELEN_ALIGNMENT_ADD ;

			if (!fileFullEaInformation->NextEntryOffset)
				break ;

			offset += fileFullEaInformation->NextEntryOffset;
		}

		break;
	}

	case FileQuotaInformation: {

		PFILE_QUOTA_INFORMATION	fileQuotaInformation;

		while (offset < returnedLength) {

			fileQuotaInformation = (PFILE_QUOTA_INFORMATION)(Buffer + offset);

			safeAddSize += SAFELEN_ALIGNMENT_ADD ;

			if (!fileQuotaInformation->NextEntryOffset)
				break;

			offset += fileQuotaInformation->NextEntryOffset;
		}

		break ;
	}

	default:
		safeAddSize = 0 ;
	}

	return ((returnedLength + safeAddSize) < requestLength) ? (returnedLength+safeAddSize) : requestLength;
}


NTSTATUS
PrimaryCompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    PKEVENT Event = (PKEVENT) Context;

	//ASSERT(FALSE);
	//DbgPrint("PrimaryCompletionRoutine\n");
 	//DebugTrace2(0, Dbg2, "PrimaryCompletionRoutine %X", Irp);

	KeSetEvent( Event, 0, FALSE );


    return STATUS_MORE_PROCESSING_REQUIRED;

    UNREFERENCED_PARAMETER( DeviceObject );
    UNREFERENCED_PARAMETER( Irp );
}


POPEN_FILE
PrimarySession_AllocateOpenFile (
	IN	PPRIMARY_SESSION	PrimarySession,
	IN  HANDLE				FileHandle,
	IN  PFILE_OBJECT		FileObject
	) 
{
	POPEN_FILE	openFile;

	openFile = ExAllocatePoolWithTag( NonPagedPool, sizeof(OPEN_FILE), OPEN_FILE_TAG );
	
	if (openFile == NULL) {

		ASSERT( NDASFAT_INSUFFICIENT_RESOURCES );
		return NULL;
	}

	RtlZeroMemory( openFile, sizeof(OPEN_FILE) );

	openFile->FileHandle = FileHandle;
	openFile->FileObject = FileObject;

	openFile->PrimarySession = PrimarySession;

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


	DebugTrace2( 0, Dbg,
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

	DebugTrace2( 0, Dbg,
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


#endif