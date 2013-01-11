#include "LfsProc.h"


NTSTATUS
ReadonlyRedirectIrpCompletion (
	IN PDEVICE_OBJECT	DeviceObject,
	IN PIRP				Irp,
	IN PKEVENT			WaitEvent
	);

NTSTATUS
ReadonlyRedirectIrpMajorCreate (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	);

NTSTATUS
ReadonlyRedirectIrpMajorClose (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	);

NTSTATUS
ReadonlyRedirectIrpMajorRead (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	);

NTSTATUS
ReadonlyRedirectIrpMajorQueryEa (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	);

NTSTATUS
ReadonlyRedirectIrpMajorQueryInformation (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	);

NTSTATUS
ReadonlyRedirectIrpMajorDirectoryControl (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	);

NTSTATUS
ReadonlyRedirectIrpMajorQueryVolumeInformation (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	);

NTSTATUS
ReadonlyRedirectIrpMajorFileSystemControl (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	);

NTSTATUS
ReadonlyRedirectIrpMajorLockControl (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	);

NTSTATUS
ReadonlyRedirectIrpMajorCleanup (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	);

NTSTATUS
ReadonlyRedirectIrpMajorQuerySecurity (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	);

NTSTATUS
ReadonlyRedirectIrpMajorQueryQuota (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	);

NTSTATUS
ReadonlyQueryDirectoryByIndex (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
    IN  HANDLE 						FileHandle,
    IN  ULONG						FileInformationClass,
    OUT PVOID		 				FileInformation,
    IN  ULONG						Length,
    IN  ULONG						FileIndex,
    IN  PUNICODE_STRING				FileName,
    OUT PIO_STATUS_BLOCK			IoStatusBlock,
    IN  BOOLEAN						ReturnSingleEntry
    );

NTSTATUS
ReadonlyRedirectIrp (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp,
	OUT PBOOLEAN					Result
	)
{
	NTSTATUS					status = STATUS_SUCCESS;
	PIO_STACK_LOCATION			irpSp = IoGetCurrentIrpStackLocation( Irp );
	PFILE_OBJECT				fileObject = irpSp->FileObject;

	ULONG_PTR					stackBottom;
	ULONG_PTR					stackTop;
	PREADONLY_REDIRECT_REQUEST	readonlyRedirectRequest;


	if (DevExt->LfsDeviceExt.AttachedToDeviceObject == NULL) {

		NDAS_BUGON( FALSE );
	}

	if (irpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL) {
		
		ULONG	ioctlCode = irpSp->Parameters.DeviceIoControl.IoControlCode;
		ULONG	devType = DEVICE_TYPE_FROM_CTL_CODE(ioctlCode);
		UCHAR	function = (UCHAR)((irpSp->Parameters.DeviceIoControl.IoControlCode & 0x00003FFC) >> 2);

		KEVENT	waitEvent;
	

		if (IS_WINDOWS2K() && DevExt->LfsDeviceExt.DiskDeviceObject == NULL) {

			NDAS_BUGON( FALSE );

			*Result = FALSE;
			return status;
		}

		if (IS_WINDOWSXP_OR_LATER() && DevExt->LfsDeviceExt.MountVolumeDeviceObject == NULL) {

			NDAS_BUGON( FALSE );

			*Result = FALSE;
			return status;
		}

		if (ioctlCode == IOCTL_VOLUME_SET_GPT_ATTRIBUTES) {

			status = Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
			IoCompleteRequest( Irp, IO_DISK_INCREMENT );

			*Result = TRUE;
			return status;
		}

		SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE,
					   ("Open files from secondary: LfsDeviceExt = %p, irpSp->MajorFunction = %x, irpSp->MinorFunction = %x\n", 
						 &DevExt->LfsDeviceExt, irpSp->MajorFunction, irpSp->MinorFunction) );


		IoCopyCurrentIrpStackLocationToNext( Irp );
		KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

		IoSetCompletionRoutine( Irp,
								ReadonlyRedirectIrpCompletion,
								&waitEvent,
								TRUE,
								TRUE,
								TRUE );

		if (IS_WINDOWS2K()) {

			status = IoCallDriver( DevExt->LfsDeviceExt.DiskDeviceObject, Irp );
		
		} else {

			status = IoCallDriver( DevExt->LfsDeviceExt.MountVolumeDeviceObject, Irp );
		}

		if (status == STATUS_PENDING) {

			status = KeWaitForSingleObject( &waitEvent,
											Executive,
											KernelMode,
											FALSE,
											NULL );

			ASSERT( status == STATUS_SUCCESS );
		}

		ASSERT( KeReadStateEvent(&waitEvent) || !NT_SUCCESS(Irp->IoStatus.Status) );
	
		SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE, 
					   ("ReadonlyIoctl: Irp->IoStatus.Status = %x\n", Irp->IoStatus.Status) );

		status = Irp->IoStatus.Status;
		IoCompleteRequest( Irp, IO_NO_INCREMENT );

		*Result = TRUE;
		return status;
	}

	readonlyRedirectRequest = (PREADONLY_REDIRECT_REQUEST)IoGetTopLevelIrp();
    IoGetStackLimits( &stackTop, &stackBottom );

	if ((ULONG_PTR)readonlyRedirectRequest <= stackBottom - sizeof(READONLY_REDIRECT_REQUEST)	&&
		(ULONG_PTR) readonlyRedirectRequest >= stackTop											&&
		(!FlagOn( (ULONG_PTR) readonlyRedirectRequest, 0x3 ))									&&
		 readonlyRedirectRequest->Tag == READONLY_REDIRECT_REQUEST_TAG) {										

		KEVENT				waitEvent;

		ASSERT( irpSp->MajorFunction == IRP_MJ_CREATE || 
			    ReadonlyLookUpCcbByReadonlyFileObject(DevExt, irpSp->FileObject) || 
				readonlyRedirectRequest->ReadonlyDismount == 1 );

		SPY_LOG_PRINT( LFS_DEBUG_READONLY_NOISE,
					   ("ReadonlyRedirectIrp: %wZ validReadonlyRedirectRequest == TRUE\n", &irpSp->FileObject->FileName) );

#if DBG
		if (irpSp->MajorFunction == IRP_MJ_CREATE && FlagOn(irpSp->Parameters.Create.Options, FILE_OPEN_BY_FILE_ID)) {

            if (fileObject->FileName.Length == sizeof( FILE_REFERENCE ) ||
                (fileObject->FileName.Length == sizeof( FILE_REFERENCE ) + sizeof(WCHAR))) {

				FILE_REFERENCE fileReference;

               if (fileObject->FileName.Length == sizeof( FILE_REFERENCE )) {

                    RtlCopyMemory( &fileReference,
                                   fileObject->FileName.Buffer,
                                   sizeof( FILE_REFERENCE ));

                } else {

                    RtlCopyMemory( &fileReference,
                                   fileObject->FileName.Buffer + 1,
                                   sizeof( FILE_REFERENCE ));
                }

				SPY_LOG_PRINT( LFS_DEBUG_READONLY_INFO,
							   ("ReadonlyRedirectIrp: FILE_OPEN_BY_FILE_ID sizeof( FILE_REFERENCE ) = %d, fileObject->FileName.Length = %d " 
							    "SegmentNumberLowPart = %d, SegmentNumberHighPart = %d, SequenceNumber = %d\n",
								 sizeof( FILE_REFERENCE ), fileObject->FileName.Length, fileReference.SegmentNumberLowPart, fileReference.SegmentNumberHighPart, fileReference.SequenceNumber) );

				if (fileObject->RelatedFileObject) {

					NDAS_BUGON( FALSE );
				}

			} else {

				NDAS_BUGON( FALSE );
			}
		}

#endif

#if DBG
		readonlyRedirectRequest->DebugTag = READONLY_REDIRECT_REQUEST_TAG - 4;
#endif
		IoSetTopLevelIrp( readonlyRedirectRequest->OriginalTopLevelIrp );

		IoCopyCurrentIrpStackLocationToNext( Irp );
		KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

		IoSetCompletionRoutine( Irp,
								ReadonlyRedirectIrpCompletion,
								&waitEvent,
								TRUE,
								TRUE,
								TRUE );

		status = IoCallDriver( DevExt->LfsDeviceExt.AttachedToDeviceObject, Irp );

		if (status == STATUS_PENDING) {

			status = KeWaitForSingleObject( &waitEvent, Executive, KernelMode, FALSE, NULL );
			ASSERT( status == STATUS_SUCCESS );
		}

		ASSERT( KeReadStateEvent(&waitEvent) || !NT_SUCCESS(Irp->IoStatus.Status) );

		IoSetTopLevelIrp( (PIRP)readonlyRedirectRequest );

		if (NT_SUCCESS(status) && 
			irpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) {
			
			if (irpSp->MinorFunction == IRP_MN_USER_FS_REQUEST || irpSp->MinorFunction == IRP_MN_KERNEL_CALL) {

				if (irpSp->Parameters.FileSystemControl.FsControlCode == FSCTL_LOCK_VOLUME) {

					ASSERT( readonlyRedirectRequest->ReadonlyDismount == 1 ); 

					if (DevExt->LfsDeviceExt.NetdiskPartition) {

						NetdiskManager_DisMountVolume( GlobalLfs.NetdiskManager,
													   DevExt->LfsDeviceExt.NetdiskPartition,
													   DevExt->LfsDeviceExt.NetdiskEnabledMode );
		
						DevExt->LfsDeviceExt.NetdiskPartition = NULL;
					}
				}
			}
		}

		status = Irp->IoStatus.Status;
		IoCompleteRequest( Irp, IO_NO_INCREMENT );

		*Result = TRUE;
		return status;
	} 

	if (irpSp->MajorFunction == IRP_MJ_CREATE) {
		
		if (Irp->RequestorMode != UserMode) {

			PrintIrp( LFS_DEBUG_READONLY_INFO, __FUNCTION__, &DevExt->LfsDeviceExt, Irp );

			IoSkipCurrentIrpStackLocation( Irp );
			status = IoCallDriver( DevExt->LfsDeviceExt.AttachedToDeviceObject, Irp );

			*Result = TRUE;
			return status;
		}
	}

	if (irpSp->MajorFunction != IRP_MJ_CREATE) {

		if (ReadonlyLookUpCcb(DevExt->LfsDeviceExt.Readonly, irpSp->FileObject) == NULL) {

			IoSkipCurrentIrpStackLocation( Irp );
			status = IoCallDriver( DevExt->LfsDeviceExt.AttachedToDeviceObject, Irp );

			*Result = TRUE;
			return status;
		}
	} 

	switch (irpSp->MajorFunction) {

	case IRP_MJ_CREATE: {

		PrintIrp( LFS_DEBUG_READONLY_NOISE, "ReadonlyRedirectIrpMajorCreate", &DevExt->LfsDeviceExt, Irp );
		status = ReadonlyRedirectIrpMajorCreate( DevExt, Irp );
	
		break;
	}

	case IRP_MJ_CLOSE: {

		PrintIrp( LFS_DEBUG_READONLY_NOISE, "ReadonlyRedirectIrpMajorClose", &DevExt->LfsDeviceExt, Irp );
		status = ReadonlyRedirectIrpMajorClose( DevExt, Irp );
		break;
	}
 
	case IRP_MJ_READ: {

		PrintIrp( LFS_DEBUG_READONLY_NOISE, "ReadonlyRedirectIrpMajorRead", &DevExt->LfsDeviceExt, Irp );
		status = ReadonlyRedirectIrpMajorRead( DevExt, Irp );
		break;
	}

	case IRP_MJ_QUERY_INFORMATION: {

		PrintIrp( LFS_DEBUG_READONLY_NOISE, "ReadonlyRedirectIrpMajorQueryInformation", &DevExt->LfsDeviceExt, Irp );
		status = ReadonlyRedirectIrpMajorQueryInformation( DevExt, Irp );
		break;
	}

	case IRP_MJ_QUERY_EA: {

		status = Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
		IoCompleteRequest( Irp, IO_NO_INCREMENT );

		break;

		PrintIrp( LFS_DEBUG_READONLY_NOISE, "ReadonlyRedirectIrpMajorQueryEa", &DevExt->LfsDeviceExt, Irp );
		status = ReadonlyRedirectIrpMajorQueryEa( DevExt, Irp );
		break;
	}

	case IRP_MJ_QUERY_VOLUME_INFORMATION: {

		PrintIrp( LFS_DEBUG_READONLY_NOISE, "ReadonlyRedirectIrpMajorQueryVolumeInformation", &DevExt->LfsDeviceExt, Irp );
		status = ReadonlyRedirectIrpMajorQueryVolumeInformation( DevExt, Irp );
		break;
	}

	case IRP_MJ_DIRECTORY_CONTROL: {

		PrintIrp( LFS_DEBUG_READONLY_NOISE, "ReadonlyRedirectIrpMajorDirectoryControl", &DevExt->LfsDeviceExt, Irp );
		status = ReadonlyRedirectIrpMajorDirectoryControl( DevExt, Irp );
		break;
	}

	case IRP_MJ_FILE_SYSTEM_CONTROL: {

		PrintIrp( LFS_DEBUG_READONLY_NOISE, "ReadonlyRedirectIrpMajorFileSystemControl", &DevExt->LfsDeviceExt, Irp );
		status = ReadonlyRedirectIrpMajorFileSystemControl( DevExt, Irp );
		break;
	}

	case IRP_MJ_DEVICE_CONTROL: {

		goto completeIrp;
	}

	case IRP_MJ_INTERNAL_DEVICE_CONTROL: {

		goto completeIrp;
	}

	case IRP_MJ_LOCK_CONTROL: {

		//
		// @CAUTION@
		// Temporarily fix.
		// return success to enable Microsoft office to work on read-only volume.
		//
		//		status = Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;

		status = Irp->IoStatus.Status = STATUS_SUCCESS;
		IoCompleteRequest( Irp, IO_NO_INCREMENT );

		break;

		PrintIrp( LFS_DEBUG_READONLY_NOISE, "ReadonlyRedirectIrpMajorLockControl", &DevExt->LfsDeviceExt, Irp );
		status = ReadonlyRedirectIrpMajorLockControl( DevExt, Irp );
		break;
	}

	case IRP_MJ_CLEANUP: {

		PrintIrp( LFS_DEBUG_READONLY_NOISE, "ReadonlyRedirectIrpMajorCleanup", &DevExt->LfsDeviceExt, Irp );
		status = ReadonlyRedirectIrpMajorCleanup( DevExt, Irp );
		break;
	}

	case IRP_MJ_QUERY_SECURITY: {

		status = Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
		IoCompleteRequest( Irp, IO_NO_INCREMENT );

		break;

		PrintIrp( LFS_DEBUG_READONLY_NOISE, "ReadonlyRedirectIrpMajorQuerySecurity", &DevExt->LfsDeviceExt, Irp );
		status = ReadonlyRedirectIrpMajorQuerySecurity( DevExt, Irp );
		break;
	}

	case IRP_MJ_QUERY_QUOTA: {

		status = Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
		IoCompleteRequest( Irp, IO_NO_INCREMENT );

		break;

		PrintIrp( LFS_DEBUG_READONLY_NOISE, "ReadonlyRedirectIrpMajorQueryQuota", &DevExt->LfsDeviceExt, Irp );
		status = ReadonlyRedirectIrpMajorQueryQuota( DevExt, Irp );
		break;
	}

	case IRP_MJ_CREATE_NAMED_PIPE: 
	case IRP_MJ_WRITE: 
	case IRP_MJ_SET_INFORMATION: 
	case IRP_MJ_SET_EA: 
	case IRP_MJ_FLUSH_BUFFERS: 
	case IRP_MJ_SET_VOLUME_INFORMATION:
	case IRP_MJ_SHUTDOWN: 
	case IRP_MJ_CREATE_MAILSLOT: 
	case IRP_MJ_SET_SECURITY: 
	case IRP_MJ_POWER: 
	case IRP_MJ_SYSTEM_CONTROL: 
	case IRP_MJ_DEVICE_CHANGE: 
	case IRP_MJ_SET_QUOTA: 
	case IRP_MJ_PNP: 

		PrintIrp( LFS_DEBUG_READONLY_NOISE, "ReadonlyRedirectIrp STATUS_MEDIA_WRITE_PROTECTED", &DevExt->LfsDeviceExt, Irp );

		status = Irp->IoStatus.Status = STATUS_MEDIA_WRITE_PROTECTED;
		IoCompleteRequest( Irp, IO_NO_INCREMENT );

		break;

	default:

completeIrp:

		PrintIrp( LFS_DEBUG_READONLY_ERROR, "ReadonlyRedirectIrp STATUS_MEDIA_WRITE_PROTECTED", &DevExt->LfsDeviceExt, Irp );

		NDAS_BUGON( FALSE );

		status = Irp->IoStatus.Status = STATUS_MEDIA_WRITE_PROTECTED;
		IoCompleteRequest( Irp, IO_NO_INCREMENT );

		break;
	}

	*Result = TRUE;

	return status;
}


NTSTATUS
ReadonlyRedirectIrpCompletion (
	IN PDEVICE_OBJECT	DeviceObject,
	IN PIRP				Irp,
	IN PKEVENT			WaitEvent
	)
{
	SPY_LOG_PRINT( LFS_DEBUG_READONLY_NOISE, ("%s: called\n", __FUNCTION__) );

	UNREFERENCED_PARAMETER( DeviceObject );
	UNREFERENCED_PARAMETER( Irp );

	KeSetEvent( WaitEvent, IO_NO_INCREMENT, FALSE );

	return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS
ReadonlyRedirectIrpMajorCreate (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	)
{
	NTSTATUS				status = STATUS_UNSUCCESSFUL;
	IO_STATUS_BLOCK			ioStatusBlock = {0, 0};

	PIO_STACK_LOCATION		irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT			fileObject = irpSp->FileObject;

	PNDAS_FCB				fcb;
	PNDAS_CCB				ccb;
	ULONG					ccbSize;

	struct Create			create;
    ULONG					eaLength;			
		
	CREATE_CONTEXT			createContext;

	UNICODE_STRING			fullFileName;
	PWCHAR					fullFileNameBuffer = NULL;

	HANDLE					readonlyFileHandle = NULL;
	PFILE_OBJECT			readonlyFileObject = NULL;

	TYPE_OF_OPEN			typeOfOpen;

	//PFILE_RECORD_SEGMENT_HEADER	fileRecordSegmentHeader = NULL;

	//
	//	Build parameters for the request
	//

	create.EaLength			= irpSp->Parameters.Create.EaLength;
	create.FileAttributes	= irpSp->Parameters.Create.FileAttributes;
	create.Options			= irpSp->Parameters.Create.Options;
	create.SecurityContext	= irpSp->Parameters.Create.SecurityContext;
	create.ShareAccess		= irpSp->Parameters.Create.ShareAccess;

	if (IS_WINDOWS2K()) {

		ULONG	disposition;
		ULONG	createOptions;

		disposition		= (create.Options & 0xFF000000) >> 24;
		createOptions	= create.Options & 0x00FFFFFF;
		createOptions  |= (create.SecurityContext->FullCreateOptions & FILE_DELETE_ON_CLOSE);
	
		if (FlagOn(createOptions, FILE_DELETE_ON_CLOSE) ||
			disposition == FILE_CREATE					||
			disposition == FILE_SUPERSEDE				||
			disposition == FILE_OVERWRITE				||
			disposition == FILE_OVERWRITE_IF) {

			status = Irp->IoStatus.Status = STATUS_MEDIA_WRITE_PROTECTED;
			Irp->IoStatus.Information = 0;
		
			IoCompleteRequest( Irp, IO_DISK_INCREMENT );
			
			return status;
		}
	}

	ASSERT( KeGetCurrentIrql() <= APC_LEVEL );

	try {

		if (FlagOn(irpSp->Flags, SL_OPEN_PAGING_FILE)) {

			PrintIrp( LFS_DEBUG_READONLY_ERROR, "SL_OPEN_PAGING_FILE", &DevExt->LfsDeviceExt, Irp );
			ASSERT( LFS_REQUIRED );
		}

		if (FlagOn(create.Options, FILE_OPEN_BY_FILE_ID)) {

			PrintIrp(LFS_DEBUG_READONLY_TRACE, "FILE_OPEN_BY_FILE_ID2", &DevExt->LfsDeviceExt, Irp);

            if (fileObject->FileName.Length == sizeof( FILE_REFERENCE ) ||
                (fileObject->FileName.Length == sizeof( FILE_REFERENCE ) + sizeof(WCHAR))) {

#if DBG

				FILE_REFERENCE fileReference;

               if (fileObject->FileName.Length == sizeof( FILE_REFERENCE )) {

                    RtlCopyMemory( &fileReference,
                                   fileObject->FileName.Buffer,
                                   sizeof( FILE_REFERENCE ));

                } else {

                    RtlCopyMemory( &fileReference,
                                   fileObject->FileName.Buffer + 1,
                                   sizeof( FILE_REFERENCE ));
                }

				SPY_LOG_PRINT( LFS_DEBUG_READONLY_INFO,
							   ("ReadonlyRedirectIrpMajorCreate: FILE_OPEN_BY_FILE_ID sizeof( FILE_REFERENCE ) = %d, fileObject->FileName.Length = %d " 
							    "SegmentNumberLowPart = %d, SegmentNumberHighPart = %d, SequenceNumber = %d\n",
								 sizeof( FILE_REFERENCE ), fileObject->FileName.Length, fileReference.SegmentNumberLowPart, fileReference.SegmentNumberHighPart, fileReference.SequenceNumber) );
#endif

				//NDAS_BUGON( FALSE );

				if (fileObject->RelatedFileObject) {

					//NDAS_BUGON( FALSE );
					//status = STATUS_NOT_IMPLEMENTED;
					//leave;
				}

			} else {

				ASSERT( LFS_REQUIRED );

				status = STATUS_NOT_IMPLEMENTED;
				leave;
			}
		}

		if (Irp->AssociatedIrp.SystemBuffer != NULL) {

			PFILE_FULL_EA_INFORMATION	fileFullEa = (PFILE_FULL_EA_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
		
			eaLength = 0;

			while (fileFullEa->NextEntryOffset) {

				eaLength += fileFullEa->NextEntryOffset;
				fileFullEa = (PFILE_FULL_EA_INFORMATION)((UINT8 *)fileFullEa + fileFullEa->NextEntryOffset);
			}

			eaLength += sizeof(FILE_FULL_EA_INFORMATION) - sizeof(CHAR) + fileFullEa->EaNameLength + fileFullEa->EaValueLength;

			SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE,
						   ("ReadonlyRedirectIrpMajorCreate: Ea is set create->EaLength = %d, eaLength = %d\n",
							 create.EaLength, eaLength) );
	
			ASSERT( create.EaLength == eaLength );
	
		} else {

			eaLength = 0;
		}

		ASSERT( fileObject->FsContext == NULL );
		ASSERT( ReadonlyLookUpCcb(DevExt->LfsDeviceExt.Readonly, fileObject) == NULL );

		createContext.IrpFlags = Irp->Flags;
		createContext.IrpSpFlags = irpSp->Flags;

		if (fileObject->RelatedFileObject) {

			if (ReadonlyLookUpCcb(DevExt->LfsDeviceExt.Readonly, fileObject->RelatedFileObject)) {

				PNDAS_CCB relatedCcb = fileObject->RelatedFileObject->FsContext2;


				SPY_LOG_PRINT( LFS_DEBUG_READONLY_NOISE,
							   ("ReadonlyRedirectIrpMajorCreate: RelatedFileObject is binded\n") );

				if (relatedCcb->Mark != CCB_MARK) {

					NDAS_BUGON( FALSE );
					status = STATUS_UNSUCCESSFUL;
					leave;
				}
	
				if (relatedCcb->Corrupted == TRUE) {

					status = STATUS_OBJECT_PATH_NOT_FOUND;
	
					leave;
				}

				createContext.RelatedFileHandle = relatedCcb->ReadonlyFileHandle; 

			} else {

				createContext.RelatedFileHandle = 0;
			}

		} else {

			createContext.RelatedFileHandle = 0;
		}		

		createContext.SecurityContext.DesiredAccess	= create.SecurityContext->DesiredAccess;
		createContext.Options						= create.Options;
		createContext.FileAttributes				= create.FileAttributes;
		createContext.ShareAccess					= create.ShareAccess;
	
		createContext.EaLength						= eaLength; /* ? create->EaLength : 0; */

		createContext.AllocationSize				= Irp->Overlay.AllocationSize.QuadPart;

		createContext.SecurityContext.FullCreateOptions	= create.SecurityContext->FullCreateOptions;
		createContext.FileNameLength				= fileObject->FileName.Length;

		fullFileNameBuffer = ExAllocatePoolWithTag( NonPagedPool, 
											 NDFS_MAX_PATH + DevExt->LfsDeviceExt.NetdiskPartitionInformation.VolumeName.Length, LFS_ALLOC_TAG );
	
		if (fullFileNameBuffer == NULL) {
		
			NDAS_BUGON( FALSE );
	
			status = STATUS_INSUFFICIENT_RESOURCES;
		}

		RtlInitEmptyUnicodeString( &fullFileName,
								   fullFileNameBuffer,
								   NDFS_MAX_PATH + DevExt->LfsDeviceExt.NetdiskPartitionInformation.VolumeName.Length );

		if (FlagOn(createContext.Options, FILE_OPEN_BY_FILE_ID)) {

			status = RtlAppendUnicodeStringToString( &fullFileName,
													 &DevExt->LfsDeviceExt.NetdiskPartitionInformation.VolumeName );

			if (status != STATUS_SUCCESS) {

				NDAS_BUGON( FALSE );
				leave;
			}
#if 1
			status = RtlAppendUnicodeToString( &fullFileName, L"\\" );

			if (status != STATUS_SUCCESS) {
				
				NDAS_BUGON( FALSE );
				leave;
			}
#endif
			SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE,
						   ("fullFileName.Length = %d, %wZ\n",
						    fullFileName.Length, &fullFileName) );
			
			RtlCopyMemory( &fullFileName.Buffer[fullFileName.Length/sizeof(WCHAR)],
						   fileObject->FileName.Buffer,
						   fileObject->FileName.Length );

			fullFileName.Length += fileObject->FileName.Length;

			SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE,
						   ("fullFileName.Length = %d, %wZ\n",
						    fullFileName.Length, &fullFileName) );

		} else {

			status = ReadonlyMakeFullFileName( DevExt,
											   fileObject, 
											   &fullFileName,
											   FALSE );
		}

		if (status != STATUS_SUCCESS) {

			SPY_LOG_PRINT( LFS_DEBUG_READONLY_ERROR, ("ReadonlyMakeFullFileName: status = %x\n", status) );

			if (status == STATUS_BUFFER_TOO_SMALL) {

				status = STATUS_OBJECT_NAME_INVALID;
			
			} else if (status == STATUS_OBJECT_NAME_INVALID) {

			} else {

				NDAS_BUGON( FALSE );
			}

			leave;
		}

		SPY_LOG_PRINT( LFS_DEBUG_READONLY_NOISE,
						("ReadonlyRedirectIrpMajorCreate: fullFileName = %wZ length = %d FlagOn(create.Options, FILE_OPEN_BY_FILE_ID) = %d\n", 
						 &fullFileName, fullFileName.Length, BooleanFlagOn(createContext.Options, FILE_OPEN_BY_FILE_ID)) );
		

		do {

			ACCESS_MASK			desiredAccess;
			ULONG				attributes;
			OBJECT_ATTRIBUTES	objectAttributes;
			LARGE_INTEGER		allocationSize;
			ULONG				fileAttributes;
			ULONG				shareAccess;
		    ULONG				disposition;
			ULONG				createOptions;
			PVOID				eaBuffer;
			ULONG				eaLength;
		    CREATE_FILE_TYPE	createFileType;
			ULONG				options;

			PIRP						topLevelIrp;
			READONLY_REDIRECT_REQUEST	readonlyRedirectRequest;


			desiredAccess = createContext.SecurityContext.DesiredAccess;

			attributes =  OBJ_KERNEL_HANDLE;
			if (!FlagOn(createContext.IrpSpFlags, SL_CASE_SENSITIVE))
				attributes |= OBJ_CASE_INSENSITIVE;

			InitializeObjectAttributes( &objectAttributes,
										&fullFileName,
										attributes,
										NULL,
										NULL );

			allocationSize.QuadPart = createContext.AllocationSize;

			fileAttributes			= createContext.FileAttributes;

			shareAccess				= createContext.ShareAccess;
		
			disposition				= (createContext.Options & 0xFF000000) >> 24;
		
			createOptions			= createContext.Options & 0x00FFFFFF;
			createOptions			|= (createContext.SecurityContext.FullCreateOptions & FILE_DELETE_ON_CLOSE);

			eaBuffer				= Irp->AssociatedIrp.SystemBuffer;
			eaLength				= createContext.EaLength;

			createFileType			= CreateFileTypeNone;
		
			options					= createContext.IrpSpFlags & 0x000000FF;
		
			RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

			SPY_LOG_PRINT( LFS_DEBUG_READONLY_NOISE,
							("ReadonlyRedirectIrpMajorCreate: LFS_WINXP_REQUEST_CREATE: DesiredAccess = %X. Synchronize:%d. "
							 "Dispo:%02lX CreateOptions = %X. Synchronize: Alert=%d Non-Alert=%d EaLen:%d.\n",
							  desiredAccess,
							  (desiredAccess & SYNCHRONIZE) != 0,
							  disposition,
							  createOptions,
							  (createOptions & FILE_SYNCHRONOUS_IO_ALERT) != 0,
							  (createOptions & FILE_SYNCHRONOUS_IO_NONALERT) != 0,
							  eaLength) );

			if (createContext.FileNameLength == 0) {
		
				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_TRACE,
							   ("DispatchWinXpRequest: LFS_WINXP_REQUEST_CREATE: desiredAccess = %x, fileAtributes %x, "
							    "shareAccess %x, disposition %x, createOptions %x, createFileType %x\n",
								 desiredAccess,
								 fileAttributes,
								 shareAccess,
								 disposition,
								 createOptions,
								 createFileType) );
			}

			//
			//	force the file to be synchronized.
			//

			desiredAccess |= SYNCHRONIZE;

			if (!FlagOn(createOptions, FILE_SYNCHRONOUS_IO_NONALERT))
				createOptions |= FILE_SYNCHRONOUS_IO_ALERT;
		
			topLevelIrp = IoGetTopLevelIrp();
			ASSERT( topLevelIrp == NULL );

			readonlyRedirectRequest.Tag					= READONLY_REDIRECT_REQUEST_TAG;
#if DBG
			readonlyRedirectRequest.DebugTag			= READONLY_REDIRECT_REQUEST_TAG;
#endif
			readonlyRedirectRequest.DevExt				= DevExt;
			readonlyRedirectRequest.OriginalTopLevelIrp = topLevelIrp;
			readonlyRedirectRequest.ReadonlyDismount	= 0;

			IoSetTopLevelIrp( (PIRP)&readonlyRedirectRequest );

		    status = ZwCreateFile( &readonlyFileHandle,
								   desiredAccess,
								   &objectAttributes,
								   &ioStatusBlock,
								   &allocationSize,
								   fileAttributes,
								   shareAccess,
								   disposition,
								   createOptions,
								   eaBuffer,
								   eaLength );


#if 0
			status = IoCreateFile( &readonlyFileHandle,
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
#endif
			ASSERT( readonlyRedirectRequest.DebugTag == (READONLY_REDIRECT_REQUEST_TAG - 4) ||
				    fullFileName.Buffer[DevExt->LfsDeviceExt.NetdiskPartitionInformation.VolumeName.Length/sizeof(WCHAR)] != L'\\');

			IoSetTopLevelIrp( topLevelIrp );

			SPY_LOG_PRINT( NT_SUCCESS(status) ? LFS_DEBUG_READONLY_NOISE : LFS_DEBUG_READONLY_TRACE,
						   ("ReadonlyRedirectIrpMajorCreate: %wZ, status = %x, desiredAccess = %x, shareAccess = %x, disposition = %x, createOption = %x, eaLength = %d, eaBuffer = %p\n", 
						     &fileObject->FileName, status, desiredAccess, shareAccess, disposition, createOptions, eaLength, eaBuffer) ); 

			if (status == STATUS_ACCESS_VIOLATION) {

				KEVENT				waitEvent;

				IoCopyCurrentIrpStackLocationToNext( Irp );
				KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

				IoSetCompletionRoutine( Irp,
										ReadonlyRedirectIrpCompletion,
										&waitEvent,
										TRUE,
										TRUE,
										TRUE );

				status = IoCallDriver( DevExt->LfsDeviceExt.AttachedToDeviceObject, Irp );

				if (status == STATUS_PENDING) {

					status = KeWaitForSingleObject( &waitEvent, Executive, KernelMode, FALSE, NULL );
					ASSERT( status == STATUS_SUCCESS );
				}

				ASSERT( KeReadStateEvent(&waitEvent) || !NT_SUCCESS(Irp->IoStatus.Status) );

				SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE, ("status = %x\n", status) );

				leave;
			}

			if (NT_SUCCESS(status)) {

				Irp->IoStatus.Status = ioStatusBlock.Status;
				Irp->IoStatus.Information = ioStatusBlock.Information;

				status = ObReferenceObjectByHandle( readonlyFileHandle,
													FILE_READ_DATA,
													NULL,
													KernelMode,
													&readonlyFileObject,
													NULL );


				if (status != STATUS_SUCCESS) {
				
					NDAS_BUGON( FALSE );
					leave;		
				}
			}


#if 0
			if (NT_SUCCESS(status)) {

				PFILE_OBJECT	fileObject;
				HANDLE			eventHandle;


				ASSERT( fileHandle != 0 );
				ASSERT( status == STATUS_SUCCESS );
				ASSERT( status == ioStatusBlock.Status );

				status = ObReferenceObjectByHandle( fileHandle,
													FILE_READ_DATA,
													NULL,
													KernelMode,
													&fileObject,
													NULL );
	
				if (status != STATUS_SUCCESS) {

					ASSERT( LFS_UNEXPECTED );		
				}
			
				if (createContext.FileNameLength &&
					DevExt->LfsDeviceExt.FileSystemType == LFS_FILE_SYSTEM_NTFS) {
			
					status = GetFileRecordSegmentHeader( PrimarySession,
														 fileHandle,
														 fileRecordSegmentHeader );

					if (status != STATUS_SUCCESS) {

						//ASSERT(LFS_UNEXPECTED);
						fileRecordSegmentHeader = NULL;
				
					} else {

						if(BooleanFlagOn(fileRecordSegmentHeader->Flags, FILE_FILE_NAME_INDEX_PRESENT))
							fileRecordSegmentHeader = NULL;
					}
			
				} else {

					fileRecordSegmentHeader = NULL;
				}
			}
#endif

		} while (0);

		if (!NT_SUCCESS(status)) {

			leave;
		}

		ccb = NULL;

#if 0
		if (fileRecordSegmentHeader) {

			if (FlagOn(fileRecordSegmentHeader->Flags, FILE_FILE_NAME_INDEX_PRESENT)) {

				typeOfOpen = UserDirectoryOpen;
		
			} else {

				if (fileObject->FileName.Buffer[fileObject->FileName.Length/sizeof(WCHAR)-1] == L'\\' || 
					FlagOn( irpSp->Parameters.Create.Options, FILE_DIRECTORY_FILE)) {
				
					typeOfOpen = UserDirectoryOpen;
			
				} else {

					typeOfOpen = UserFileOpen;
				}
			}
	
		} else 
#endif		
		{

			if (fileObject->FileName.Length == 0) {

				typeOfOpen = UserVolumeOpen;
		
			} else if (fileObject->FileName.Buffer[fileObject->FileName.Length/sizeof(WCHAR)-1] == L'\\' || 
					   FlagOn( irpSp->Parameters.Create.Options, FILE_DIRECTORY_FILE)) {

				SPY_LOG_PRINT( LFS_DEBUG_READONLY_NOISE,
							   ("ReadonlyRedirectIrpMajorCreate: Directory fileObject->FileName = %wZ " 
							    "BooleanFlagOn( irpSp->Parameters.Create.Options, FILE_DIRECTORY_FILE) = %d\n",
							    &fileObject->FileName, BooleanFlagOn( irpSp->Parameters.Create.Options, FILE_DIRECTORY_FILE)) );
		
				typeOfOpen = UserDirectoryOpen;
		
			} else {

				typeOfOpen = UserFileOpen;	
			}
		}
	
		if (typeOfOpen == UserFileOpen && FlagOn(irpSp->Parameters.Create.Options, FILE_NO_INTERMEDIATE_BUFFERING)) {

			SetFlag( fileObject->Flags, FO_CACHE_SUPPORTED );
		}

#if 0
		fcb = Secondary_LookUpFcb( 	Secondary,
									&fullFileName,
									!BooleanFlagOn(irpSp->Flags, SL_CASE_SENSITIVE) );
#else
		fcb = NULL;
#endif

		if (fcb == NULL) {

			KIRQL	oldIrql;
			BOOLEAN fcbQueueEmpty;
			
			KeAcquireSpinLock( &DevExt->LfsDeviceExt.Readonly->FcbQSpinLock, &oldIrql );

			fcbQueueEmpty = IsListEmpty( &DevExt->LfsDeviceExt.Readonly->FcbQueue );
			KeReleaseSpinLock( &DevExt->LfsDeviceExt.Readonly->FcbQSpinLock, oldIrql );
			
			fcb = ReadonlyAllocateFcb( DevExt,
									   &fullFileName,
									   BooleanFlagOn(irpSp->Flags, SL_OPEN_PAGING_FILE) );

			if (fcb == NULL) {

				NDAS_BUGON( NDAS_BUGON_INSUFFICIENT_RESOURCES );

				status = STATUS_INSUFFICIENT_RESOURCES;
				leave;
			}

#if 0
			if (fileRecordSegmentHeader) {
			
				RtlCopyMemory( &fcb->FileRecordSegmentHeader,
							   fileRecordSegmentHeader,
							   sizeof(*fileRecordSegmentHeader) );

				fcb->FileRecordSegmentHeaderAvail = TRUE;
		
			} else {

				fcb->FileRecordSegmentHeaderAvail = FALSE;
			}
#endif

			ExInterlockedInsertHeadList( &DevExt->LfsDeviceExt.Readonly->FcbQueue,
										 &fcb->ListEntry,
										 &DevExt->LfsDeviceExt.Readonly->FcbQSpinLock );
	
		} 

		if (create.SecurityContext->FullCreateOptions & FILE_DELETE_ON_CLOSE)
			fcb->DeletePending = TRUE;
	
		InterlockedIncrement( &fcb->OpenCount );
		InterlockedIncrement( &fcb->UncleanCount );

		ccbSize = eaLength + NDFS_MAX_PATH*sizeof(WCHAR);

#if 0
		if (fcb->FileRecordSegmentHeaderAvail == TRUE)
			ccbSize += DevExt->LfsDeviceExt.Readonly->Thread.SessionContext.BytesPerSector;
#endif

		ccb = ReadonlyAllocateCcb( DevExt,
								   fileObject,
								   ccbSize );

		if (ccb == NULL) {

			KIRQL oldIrql;

			NDAS_BUGON( NDAS_BUGON_INSUFFICIENT_RESOURCES );
	
			KeAcquireSpinLock( &DevExt->LfsDeviceExt.Readonly->FcbQSpinLock, &oldIrql );
			InterlockedDecrement( &fcb->UncleanCount );
		
			if (InterlockedDecrement(&fcb->OpenCount) == 0) {

				RemoveEntryList( &fcb->ListEntry );
				InitializeListHead( &fcb->ListEntry );
			}

			KeReleaseSpinLock( &DevExt->LfsDeviceExt.Readonly->FcbQSpinLock, oldIrql );
			ReadonlyDereferenceFcb( fcb );

			fileObject->FsContext = NULL;

			status = STATUS_INSUFFICIENT_RESOURCES;
			leave;
		}

		ccb->ReadonlyFileHandle = readonlyFileHandle;
		ccb->ReadonlyFileObject = readonlyFileObject;

		ccb->Fcb = fcb;
		ccb->TypeOfOpen = typeOfOpen;

		RtlCopyMemory( &ccb->CreateContext,
					   &createContext,
					   sizeof(CREATE_CONTEXT) );

		if (eaLength) {

			// It have structure align Problem. If you wanna release, Do more
			PFILE_FULL_EA_INFORMATION eaBuffer = Irp->AssociatedIrp.SystemBuffer;
		
			RtlCopyMemory( ccb->Buffer,
						   eaBuffer,
						   eaLength );
		}

		if (fileObject->FileName.Length) {
	
			RtlCopyMemory( ccb->Buffer + eaLength,
						   fileObject->FileName.Buffer,
						   fileObject->FileName.Length );
		}

		ExAcquireFastMutex( &DevExt->LfsDeviceExt.Readonly->CcbQMutex );
	
		InsertHeadList( &DevExt->LfsDeviceExt.Readonly->CcbQueue,
						&ccb->ListEntry );

		ExReleaseFastMutex( &DevExt->LfsDeviceExt.Readonly->CcbQMutex );

		fileObject->FsContext = fcb;
		fileObject->FsContext2 = ccb;
		fileObject->Vpb = DevExt->LfsDeviceExt.Vpb;

		fileObject->SectionObjectPointer = &fcb->NonPaged->SectionObjectPointers;

	} finally {

		if (AbnormalTermination()) {
			
			//status = GetExceptionCode();

			//if (NT_SUCCESS(status)) {

			//	NDAS_BUGON( FALSE );
				status = STATUS_UNSUCCESSFUL;
			//}
		}

		if (fullFileNameBuffer)
			ExFreePool( fullFileNameBuffer );

		if (!NT_SUCCESS(status)) {

			if (readonlyFileObject)
				ObDereferenceObject( readonlyFileObject );

			if (readonlyFileHandle)
				ZwClose( readonlyFileHandle );

			Irp->IoStatus.Status = status;
			Irp->IoStatus.Information = ioStatusBlock.Information;
		}

		if (NT_SUCCESS(status)) {

			if (fileObject->FsContext == NULL || fileObject->FsContext2 == NULL) {

				NDAS_BUGON( FALSE );
			}
		}
		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
	}
	
	return status;
}


NTSTATUS
ReadonlyRedirectIrpMajorClose (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	)
{
	NTSTATUS			status;
	
	PIO_STACK_LOCATION  irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT		fileObject = irpSp->FileObject;

	PNDAS_FCB			fcb = fileObject->FsContext;
	PNDAS_CCB			ccb = fileObject->FsContext2;

	BOOLEAN				fcbQueueEmpty;

	KIRQL				oldIrql;	
	

	if (ReadonlyLookUpCcb(DevExt->LfsDeviceExt.Readonly, fileObject) != ccb) {
		
		NDAS_BUGON( FALSE );
	}

	if (ccb->Fcb != fcb) {
		
		NDAS_BUGON( FALSE );
	}
 
	if (FlagOn(DevExt->LfsDeviceExt.Flags, READONLY_FLAG_ERROR) || 
		ccb->Corrupted == TRUE) {

		fileObject->SectionObjectPointer = NULL;

		KeAcquireSpinLock( &DevExt->LfsDeviceExt.Readonly->FcbQSpinLock, &oldIrql );
		
		if (InterlockedDecrement(&fcb->OpenCount) == 0) {

			RemoveEntryList(&fcb->ListEntry);
			InitializeListHead(&fcb->ListEntry);
		}

		fcbQueueEmpty = IsListEmpty( &DevExt->LfsDeviceExt.Readonly->FcbQueue );
		
		KeReleaseSpinLock( &DevExt->LfsDeviceExt.Readonly->FcbQSpinLock, oldIrql );

		ReadonlyDereferenceFcb( fcb );
	
		ExAcquireFastMutex( &DevExt->LfsDeviceExt.Readonly->CcbQMutex );
		RemoveEntryList( &ccb->ListEntry );
		InitializeListHead(&ccb->ListEntry);
	    ExReleaseFastMutex( &DevExt->LfsDeviceExt.Readonly->CcbQMutex );

		ReadonlyFreeCcb( DevExt, ccb );

		fileObject->FsContext = NULL;
		fileObject->FsContext2 = NULL;
		
		SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE,
					   ("ReadonlyRedirectIrpMajorClose: free a corrupted file extension:%p\n", fileObject) ); 		

		status = Irp->IoStatus.Status = STATUS_SUCCESS;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );

		return status;
	}		

	ObDereferenceObject( ccb->ReadonlyFileObject );
	ccb->ReadonlyFileObject = NULL;

	ZwClose( ccb->ReadonlyFileHandle );
	ccb->ReadonlyFileHandle = NULL;

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	fileObject->SectionObjectPointer = NULL;

	KeAcquireSpinLock( &DevExt->LfsDeviceExt.Readonly->FcbQSpinLock, &oldIrql );
		
	if (InterlockedDecrement(&fcb->OpenCount) == 0) {

		RemoveEntryList(&fcb->ListEntry);
		InitializeListHead(&fcb->ListEntry);
	}

	fcbQueueEmpty = IsListEmpty( &DevExt->LfsDeviceExt.Readonly->FcbQueue );
		
	KeReleaseSpinLock( &DevExt->LfsDeviceExt.Readonly->FcbQSpinLock, oldIrql );

	ReadonlyDereferenceFcb( fcb );
	
	ExAcquireFastMutex( &DevExt->LfsDeviceExt.Readonly->CcbQMutex );
	RemoveEntryList( &ccb->ListEntry );
	InitializeListHead(&ccb->ListEntry);
	ExReleaseFastMutex( &DevExt->LfsDeviceExt.Readonly->CcbQMutex );

	ReadonlyFreeCcb( DevExt, ccb );

	fileObject->FsContext = NULL;
	fileObject->FsContext2 = NULL;
		
	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_NOISE,
				   ("ReadonlyRedirectIrpMajorClose: %p\n", fileObject) ); 		

	status = Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest( Irp, IO_DISK_INCREMENT );

	return status;
}


NTSTATUS
ReadonlyRedirectIrpMajorDirectoryControl (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	)
{
	NTSTATUS			status = STATUS_UNSUCCESSFUL;
	IO_STATUS_BLOCK		ioStatusBlock = {0, 0};

	PIO_STACK_LOCATION  irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT		fileObject = irpSp->FileObject;

	PNDAS_FCB			fcb = fileObject->FsContext;
	PNDAS_CCB			ccb = fileObject->FsContext2;


	if (ReadonlyLookUpCcb(DevExt->LfsDeviceExt.Readonly, fileObject) != ccb) {

		NDAS_BUGON( FALSE );
	}

	if (ccb->Fcb != fcb) {

		NDAS_BUGON( FALSE );
	}

	if (ccb->Corrupted == TRUE) {

		status = Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
		return status;
	}

	try {

		SPY_LOG_PRINT( LFS_DEBUG_READONLY_NOISE,
					   ("ReadonlyRedirectIrpMajorDirectoryControl: MinorFunction = %X\n", irpSp->MinorFunction) );

        if (irpSp->MinorFunction == IRP_MN_NOTIFY_CHANGE_DIRECTORY) {

			status = Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
			Irp->IoStatus.Information = 0;

			leave;

		} else if (irpSp->MinorFunction == IRP_MN_QUERY_DIRECTORY) {

			struct QueryDirectory		queryDirectory;
			PVOID						inputBuffer;
			ULONG						inputBufferLength;
			PVOID						outputBuffer = NULL;
			BOOLEAN						userModeAddressOutputBuffer = FALSE;
			PVOID						originalOutputBuffer = NULL;
			ULONG						outputBufferLength;
			PMDL						mdl = NULL;

			BOOLEAN						indexSpecified = FALSE;

			queryDirectory.FileIndex			= irpSp->Parameters.QueryDirectory.FileIndex;
			queryDirectory.FileInformationClass = irpSp->Parameters.QueryDirectory.FileInformationClass;
			queryDirectory.FileName				= (PSTRING)irpSp->Parameters.QueryDirectory.FileName;
			queryDirectory.Length				= irpSp->Parameters.QueryDirectory.Length;

#if 0
			if (ccb->LastQueryFileIndex != (ULONG)-1				&&
				ccb->LastDirectoryQuerySessionId != ccb->SessionId	&&
				!FlagOn(irpSp->Flags, SL_RESTART_SCAN)) {

				SPY_LOG_PRINT( LFS_DEBUG_READONLY_ERROR,
							   ("ReadonlyRedirectIrpMajorDirectoryControl: SL_INDEX_SPECIFIED set\n") );

				queryDirectory.FileIndex = ccb->LastQueryFileIndex;
				indexSpecified = TRUE;
			}
#endif

			inputBuffer			= (queryDirectory.FileName) ? (queryDirectory.FileName->Buffer) : NULL;
			inputBufferLength	= (queryDirectory.FileName) ? (queryDirectory.FileName->Length) : 0;
			outputBufferLength	= queryDirectory.Length;

			if (outputBufferLength) {
			
				do {

					if (FlagOn(Irp->Flags, IRP_BUFFERED_IO) && Irp->AssociatedIrp.SystemBuffer) {

						ASSERT( !(Irp->Flags & IRP_ASSOCIATED_IRP) );
						ASSERT( Irp->MdlAddress == NULL );

						outputBuffer = Irp->AssociatedIrp.SystemBuffer;
						userModeAddressOutputBuffer = FALSE;

						break;
					}

					if (Irp->MdlAddress) {

						outputBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
						userModeAddressOutputBuffer = FALSE;

						ASSERT( outputBuffer );
			
						break;
					}

					if (Irp->UserBuffer) {
					
						ASSERT( Irp->RequestorMode == UserMode );

						try {

							if (Irp->UserBuffer && Irp->RequestorMode != KernelMode) {

								ProbeForWrite( Irp->UserBuffer, 
											   outputBufferLength, 
											   sizeof(UCHAR) );
							}

						} except (Irp->RequestorMode != KernelMode ? EXCEPTION_EXECUTE_HANDLER: EXCEPTION_CONTINUE_SEARCH) {

							status = GetExceptionCode();
							NDAS_BUGON( FALSE );
						}

						outputBuffer = Irp->UserBuffer;
						userModeAddressOutputBuffer = TRUE;
					
						break;
					}

					if (Irp->AssociatedIrp.SystemBuffer) {
					
						outputBuffer = Irp->AssociatedIrp.SystemBuffer;
						userModeAddressOutputBuffer = FALSE;
					
						break;
					}

					NDAS_BUGON( FALSE );
			
				} while (0);

				if (userModeAddressOutputBuffer) {

					originalOutputBuffer = outputBuffer;

#if 1
					outputBuffer = ExAllocatePoolWithTag( NonPagedPool, 
														  outputBufferLength, 
														  LFS_ALLOC_TAG );
#else
					mdl = IoAllocateMdl( outputBuffer, outputBufferLength, FALSE, FALSE, NULL );
					outputBuffer = MmGetSystemAddressForMdlSafe(mdl, NormalPagePriority);
#endif
				}
			}

			if (queryDirectory.FileName && queryDirectory.FileName->Length) {

				SPY_LOG_PRINT( LFS_DEBUG_READONLY_NOISE,
							   ("ReadonlyRedirectIrpMajorDirectoryControl: queryFileName = %wZ\n",
								queryDirectory.FileName) );
			}

			{
				BOOLEAN					synchronousIo;

				ULONG					length;
				FILE_INFORMATION_CLASS	fileInformationClass;
				BOOLEAN					returnSingleEntry;
				BOOLEAN					restartScan;

	
				length					= queryDirectory.Length;
				fileInformationClass	= queryDirectory.FileInformationClass;
				
				returnSingleEntry		= BooleanFlagOn(irpSp->Flags, SL_RETURN_SINGLE_ENTRY);
				restartScan				= BooleanFlagOn(irpSp->Flags, SL_RESTART_SCAN);
				indexSpecified 			= BooleanFlagOn(irpSp->Flags, SL_INDEX_SPECIFIED);	
				synchronousIo			= BooleanFlagOn(fileObject->Flags, FO_SYNCHRONOUS_IO);

	
				RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

				if (indexSpecified) {
				
					status = ReadonlyQueryDirectoryByIndex( DevExt,
															ccb->ReadonlyFileHandle,
															fileInformationClass,
															outputBuffer,
															length,
															(ULONG)queryDirectory.FileIndex,				
															(PUNICODE_STRING)queryDirectory.FileName,
															&ioStatusBlock,											
															returnSingleEntry );
				} else {

					READONLY_REDIRECT_REQUEST	readonlyRedirectRequest;
					PIRP						topLevelIrp;


					topLevelIrp = IoGetTopLevelIrp();
					ASSERT( topLevelIrp == NULL );

					readonlyRedirectRequest.Tag					= READONLY_REDIRECT_REQUEST_TAG;
#if DBG
					readonlyRedirectRequest.DebugTag			= READONLY_REDIRECT_REQUEST_TAG;
#endif
					readonlyRedirectRequest.DevExt				= DevExt;
					readonlyRedirectRequest.OriginalTopLevelIrp = topLevelIrp;
					readonlyRedirectRequest.ReadonlyDismount	= 0;

					IoSetTopLevelIrp( (PIRP)&readonlyRedirectRequest );

					if (synchronousIo) {

						status = ZwQueryDirectoryFile( ccb->ReadonlyFileHandle,
													   NULL,
													   NULL,
													   NULL,
													   &ioStatusBlock,
													   outputBuffer,
													   length,
													   fileInformationClass,
													   returnSingleEntry,
													   (PUNICODE_STRING)queryDirectory.FileName,
													   restartScan );

#if 0

						status = NtQueryDirectoryFile( ccb->ReadonlyFileHandle,
													   NULL,
													   NULL,
													   NULL,
													   &ioStatusBlock,
													   outputBuffer,
													   length,
													   fileInformationClass,
													   returnSingleEntry,
													   (PUNICODE_STRING)queryDirectory.FileName,
													   restartScan );
#endif			  
					
					} else {

						HANDLE	eventHandle;
						
						status = ZwCreateEvent( &eventHandle,
												GENERIC_READ,
												NULL,
												SynchronizationEvent,
												FALSE );

						if (status != STATUS_SUCCESS) {

							NDAS_BUGON( FALSE );
							leave;
						}

						status = ZwQueryDirectoryFile( ccb->ReadonlyFileHandle,
													   eventHandle,
													   NULL,
													   NULL,
													   &ioStatusBlock,
													   outputBuffer,
													   length,
													   fileInformationClass,
													   returnSingleEntry,
													   (PUNICODE_STRING)queryDirectory.FileName,
													   restartScan );
  
						if (status == STATUS_PENDING) {

							status = ZwWaitForSingleObject(eventHandle, TRUE, NULL);

							if (status != STATUS_SUCCESS) {

								NDAS_BUGON( FALSE );
							
							} else {

								status = ioStatusBlock.Status;
							}
						}


						ZwClose( eventHandle );
					}

					ASSERT( readonlyRedirectRequest.DebugTag == (READONLY_REDIRECT_REQUEST_TAG -  4) );
					IoSetTopLevelIrp( topLevelIrp );
				}

				SPY_LOG_PRINT( NT_SUCCESS(status) ? LFS_DEBUG_READONLY_NOISE : LFS_DEBUG_READONLY_TRACE,
							   ("ReadonlyRedirectIrpMajorDirectoryControl: %wZ, %wZ, fileHandle = %p, status = %x, " 
							    "length = %d, ioStatusBlock.Status = %x, ioStatusBlock.Information = %d, queryDirectory.FileName = %wZ\n",
								 &fileObject->FileName, &ccb->ReadonlyFileObject->FileName, ccb->ReadonlyFileHandle, status, 
								 length, ioStatusBlock.Status, ioStatusBlock.Information, queryDirectory.FileName) );

				if (userModeAddressOutputBuffer == TRUE) {
#if 1
					RtlCopyMemory( originalOutputBuffer,
								   outputBuffer,
								   outputBufferLength );

					ExFreePoolWithTag( outputBuffer, LFS_ALLOC_TAG );
#else
					IoFreeMdl( mdl );
#endif
					outputBuffer = originalOutputBuffer;
				}

				if (status == STATUS_BUFFER_OVERFLOW) {

					SPY_LOG_PRINT( LFS_DEBUG_READONLY_NOISE,
								   ("ReadonlyRedirectIrpMajorDirectoryControl: fileHandle = %p, status = %x, " 
								    "length = %d, ioStatusBlock.Status = %x, ioStatusBlock.Information = %d\n",
									 ccb->ReadonlyFileHandle, status, length, ioStatusBlock.Status, ioStatusBlock.Information) );
				}

				if (NT_SUCCESS(status)) {

					ASSERT(status == STATUS_SUCCESS);		
					ASSERT(status == ioStatusBlock.Status);
				}
			
				if (status == STATUS_BUFFER_OVERFLOW)
					ASSERT(length == ioStatusBlock.Information);

				if (!(status == STATUS_SUCCESS || status == STATUS_BUFFER_OVERFLOW)) {

					ASSERT( ioStatusBlock.Information == 0 );
					ioStatusBlock.Information = 0;
				}

			}

			Irp->IoStatus.Status	  = status;
			Irp->IoStatus.Information = ioStatusBlock.Information; 

			if (ioStatusBlock.Information) {

				//	
				// Save last query index for the case that primary changes and lose its query context.
				//
	
				ccb->LastQueryFileIndex = LfsGetLastFileIndexFromQuery( queryDirectory.FileInformationClass,
																		outputBuffer,
																		(ULONG)Irp->IoStatus.Information );
				ccb->LastDirectoryQuerySessionId = ccb->SessionId;
			
			} else {

				//	
				// Save last query index for the case that primary changes and lose its query context.
				//
				
				ccb->LastQueryFileIndex = (ULONG)-1;
				ccb->LastDirectoryQuerySessionId = ccb->SessionId;
			}
		
		} else {

			ASSERT(LFS_UNEXPECTED);

			status = Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
			Irp->IoStatus.Information = 0;
			
			leave;
		}

	} finally {

		if (AbnormalTermination()) {

			//status = GetExceptionCode();

			//if (NT_SUCCESS(status)) {

			//	NDAS_BUGON( FALSE );
			status = STATUS_UNSUCCESSFUL;
			//}
		}

		if (!NT_SUCCESS(status)) {

			Irp->IoStatus.Status = status;
			Irp->IoStatus.Information = ioStatusBlock.Information;
		}

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
	}

	return status;
}


NTSTATUS
ReadonlyQueryDirectoryByIndexCompletion (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PKEVENT SynchronizingEvent
    )
{

    UNREFERENCED_PARAMETER( DeviceObject );
    
    ASSERT( NULL != Irp->UserIosb );
    *Irp->UserIosb = Irp->IoStatus;

    KeSetEvent( SynchronizingEvent, IO_NO_INCREMENT, FALSE );

    IoFreeIrp( Irp );

    return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS
ReadonlyQueryDirectoryByIndex (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
    IN  HANDLE 						FileHandle,
    IN  ULONG						FileInformationClass,
    OUT PVOID		 				FileInformation,
    IN  ULONG						Length,
    IN  ULONG						FileIndex,
    IN  PUNICODE_STRING				FileName,
    OUT PIO_STATUS_BLOCK			IoStatusBlock,
    IN  BOOLEAN						ReturnSingleEntry
	)
{
    NTSTATUS			status;
    KEVENT				event;
    PFILE_OBJECT		fileObject;
    PDEVICE_OBJECT		deviceObject;
    PMDL				mdl;
    PIRP				irp;
    PIO_STACK_LOCATION	irpSp;
    PCHAR				systemBuffer;

	READONLY_REDIRECT_REQUEST	readonlyRedirectRequest;
	PIRP						topLevelIrp;


    SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
				   ("[LFS] LfsQueryDirectoryByIndex: FileIndex=%x\n", FileIndex) );
	
    status = ObReferenceObjectByHandle( FileHandle,
                                        FILE_LIST_DIRECTORY,
                                        NULL,
                                        KernelMode,
                                        (PVOID *) &fileObject,
                                        (POBJECT_HANDLE_INFORMATION) NULL );

    if (!NT_SUCCESS( status )) {

		return status;
    }

    KeInitializeEvent( &event, NotificationEvent, FALSE );

    KeClearEvent( &fileObject->Event );

    deviceObject = IoGetRelatedDeviceObject( fileObject );

    irp = IoAllocateIrp( deviceObject->StackSize, TRUE );

    if (!irp) {

        ObDereferenceObject( fileObject );
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    irp->Flags = (ULONG)IRP_SYNCHRONOUS_API;
    irp->RequestorMode = KernelMode;

    irp->UserIosb = IoStatusBlock;
    irp->UserEvent = NULL;

    irp->Overlay.AsynchronousParameters.UserApcRoutine = NULL;
    irp->AssociatedIrp.SystemBuffer = (PVOID) NULL;
    systemBuffer = NULL;

    irp->Tail.Overlay.Thread = PsGetCurrentThread();
    irp->Tail.Overlay.OriginalFileObject = fileObject;
    irp->Tail.Overlay.AuxiliaryBuffer = NULL;
    irp->MdlAddress = NULL;

    irpSp = IoGetNextIrpStackLocation( irp );
    irpSp->MajorFunction = IRP_MJ_DIRECTORY_CONTROL;
    irpSp->MinorFunction = IRP_MN_QUERY_DIRECTORY;
    irpSp->FileObject = fileObject;

    irpSp->Parameters.QueryDirectory.Length = Length;
    irpSp->Parameters.QueryDirectory.FileInformationClass = FileInformationClass; //FileBothDirectoryInformation;
    irpSp->Parameters.QueryDirectory.FileIndex = FileIndex;

    irpSp->Flags = (ReturnSingleEntry?SL_RETURN_SINGLE_ENTRY:0) | SL_INDEX_SPECIFIED;

	if (FileName && FileName->Length) {
	    
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
					   ("[LFS] LfsQueryDirectoryByIndex: FileName=%wZ\n", FileName) );

#ifndef NTDDI_VERSION

#if WINVER >= 0x0502 // Windows 2003 or later
		irpSp->Parameters.QueryDirectory.FileName = FileName;
#else
		irpSp->Parameters.QueryDirectory.FileName = (PSTRING)FileName;
#endif

#else

		irpSp->Parameters.QueryDirectory.FileName = FileName;

#endif

	} else {

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
					   ("[LFS] LfsQueryDirectoryByIndex: No file name\n") );

		irpSp->Parameters.QueryDirectory.FileName = NULL;
	}

    if (deviceObject->Flags & DO_BUFFERED_IO) {

        try {
        
			systemBuffer = ExAllocatePoolWithQuotaTag( NonPagedPool, Length, 'QSFL' );
            irp->AssociatedIrp.SystemBuffer = systemBuffer;

        } except(EXCEPTION_EXECUTE_HANDLER) {
            
			IoFreeIrp( irp );
            ObDereferenceObject( fileObject );
            return GetExceptionCode();
        }

    } else if (deviceObject->Flags & DO_DIRECT_IO) {
        
		mdl = (PMDL) NULL;
        
		try {

            mdl = IoAllocateMdl( FileInformation, Length, FALSE, TRUE, irp );
            if (mdl == NULL) {
                ExRaiseStatus( STATUS_INSUFFICIENT_RESOURCES );
            }
            MmProbeAndLockPages( mdl, UserMode, IoWriteAccess );

        } except(EXCEPTION_EXECUTE_HANDLER) {
            
			if (irp->MdlAddress != NULL) {
            
				IoFreeMdl( irp->MdlAddress );
            }
            IoFreeIrp( irp );
            ObDereferenceObject( fileObject );
            return GetExceptionCode();
        }

    } else {
        
		irp->UserBuffer = FileInformation;
    }

#if 0 /* what's this?? */
    KeRaiseIrql( APC_LEVEL, &irql );
    InsertHeadList( &((PETHREAD)irp->Tail.Overlay.Thread)->IrpList,
                    &irp->ThreadListEntry );
    KeLowerIrql( irql );
#endif

    IoSetCompletionRoutine( irp, 
                            ReadonlyQueryDirectoryByIndexCompletion, 
                            &event, 
                            TRUE, 
                            TRUE, 
                            TRUE );

	topLevelIrp = IoGetTopLevelIrp();
	ASSERT( topLevelIrp == NULL );

	readonlyRedirectRequest.Tag					= READONLY_REDIRECT_REQUEST_TAG;
#if DBG
	readonlyRedirectRequest.DebugTag			= READONLY_REDIRECT_REQUEST_TAG;
#endif
	readonlyRedirectRequest.DevExt				= DevExt;
	readonlyRedirectRequest.OriginalTopLevelIrp = topLevelIrp;
	readonlyRedirectRequest.ReadonlyDismount	= 0;

	IoSetTopLevelIrp( (PIRP)&readonlyRedirectRequest );

    status = IoCallDriver( deviceObject, irp );

    if (status == STATUS_PENDING) {

        status = KeWaitForSingleObject( &event,
										Executive,
										KernelMode,
										FALSE,
										NULL );
    }

    if (NT_SUCCESS(status)) {

        status = IoStatusBlock->Status;
        
		if (NT_SUCCESS(status) || status == STATUS_BUFFER_OVERFLOW) {
        
			if (systemBuffer) {

				try {

					RtlCopyMemory( FileInformation,
                                   systemBuffer,
                                   Length );

                } except(EXCEPTION_EXECUTE_HANDLER) {

                    status = GetExceptionCode();
                }
            }
        }
    }

	ASSERT( readonlyRedirectRequest.DebugTag == (READONLY_REDIRECT_REQUEST_TAG -  4) );
	IoSetTopLevelIrp( topLevelIrp );

    if (systemBuffer) {

        ExFreePool( systemBuffer );
    }

    ObDereferenceObject( fileObject );
    
    return status;
}


NTSTATUS
ReadonlyRedirectIrpMajorRead (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	)
{
	NTSTATUS			status = STATUS_UNSUCCESSFUL;
	IO_STATUS_BLOCK		ioStatusBlock = {0, 0};


	PIO_STACK_LOCATION  irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT		fileObject = irpSp->FileObject;

	PNDAS_FCB			fcb = fileObject->FsContext;
	PNDAS_CCB			ccb = fileObject->FsContext2;


	if (ReadonlyLookUpCcb(DevExt->LfsDeviceExt.Readonly, fileObject) != ccb) {

		NDAS_BUGON( FALSE );
	}

	if (ccb->Fcb != fcb) {

		NDAS_BUGON( FALSE );
	}

	if (ccb->Corrupted == TRUE) {

		status = Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
		return status;
	}

	try {

		struct Read					read;

		PVOID						inputBuffer = NULL;
		ULONG						inputBufferLength = 0;
		PVOID						outputBuffer = NULL;
		BOOLEAN						userModeAddressOutputBuffer = FALSE;
		PVOID						originalOutputBuffer = NULL;
		PMDL						mdl = NULL;

		BOOLEAN						synchronousIo = BooleanFlagOn(fileObject->Flags, FO_SYNCHRONOUS_IO);
		BOOLEAN						pagingIo      = BooleanFlagOn(Irp->Flags, IRP_PAGING_IO);
		BOOLEAN						nonCachedIo   = BooleanFlagOn(Irp->Flags,IRP_NOCACHE);

		READONLY_REDIRECT_REQUEST	readonlyRedirectRequest;
		PIRP						topLevelIrp;


		read.ByteOffset	= irpSp->Parameters.Read.ByteOffset;
		read.Key		= irpSp->Parameters.Read.Key;
		read.Length		= irpSp->Parameters.Read.Length;

		ASSERT( !(read.ByteOffset.LowPart == FILE_USE_FILE_POINTER_POSITION && read.ByteOffset.HighPart == -1) );

		if (!pagingIo && nonCachedIo && fileObject->SectionObjectPointer->DataSectionObject != NULL) {

			CcFlushCache( fileObject->SectionObjectPointer,
						  &read.ByteOffset,
						  read.Length,
						  &ioStatusBlock );

			ASSERT( ioStatusBlock.Status == STATUS_SUCCESS );
		}

		if (read.Length) {
			
			do {

				if (FlagOn(Irp->Flags, IRP_BUFFERED_IO) && Irp->AssociatedIrp.SystemBuffer) {

					ASSERT( !(Irp->Flags & IRP_ASSOCIATED_IRP) );
					ASSERT( Irp->MdlAddress == NULL );

					outputBuffer = Irp->AssociatedIrp.SystemBuffer;
					userModeAddressOutputBuffer = FALSE;

					break;
				}

				if (Irp->MdlAddress) {

					outputBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
					userModeAddressOutputBuffer = FALSE;

					ASSERT( outputBuffer );
			
					break;
				}

				if (Irp->UserBuffer) {
					
					ASSERT( Irp->RequestorMode == UserMode );

					try {

						if (Irp->UserBuffer && Irp->RequestorMode != KernelMode) {

							ProbeForWrite( Irp->UserBuffer, 
										   read.Length, 
										   sizeof(UCHAR) );
						}

					} except (Irp->RequestorMode != KernelMode ? EXCEPTION_EXECUTE_HANDLER: EXCEPTION_CONTINUE_SEARCH) {

						status = GetExceptionCode();
						NDAS_BUGON( FALSE );
					}

					outputBuffer = Irp->UserBuffer;
					userModeAddressOutputBuffer = TRUE;
					
					break;
				}

				if (Irp->AssociatedIrp.SystemBuffer) {
				
					outputBuffer = Irp->AssociatedIrp.SystemBuffer;
					userModeAddressOutputBuffer = FALSE;
				
					break;
				}

				NDAS_BUGON( FALSE );
			
			} while (0);

			if (userModeAddressOutputBuffer) {

				originalOutputBuffer = outputBuffer;

#if 1
				outputBuffer = ExAllocatePoolWithTag( NonPagedPool, 
													  read.Length, 
													  LFS_ALLOC_TAG );
#else
				mdl = IoAllocateMdl( outputBuffer, read.Length, FALSE, FALSE, NULL );
				outputBuffer = MmGetSystemAddressForMdlSafe(mdl, NormalPagePriority);
#endif
			}
		}

		RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );
		
		topLevelIrp = IoGetTopLevelIrp();
		if (!pagingIo)
			ASSERT( topLevelIrp == NULL );

		readonlyRedirectRequest.Tag					= READONLY_REDIRECT_REQUEST_TAG;
#if DBG
		readonlyRedirectRequest.DebugTag			= READONLY_REDIRECT_REQUEST_TAG;
#endif
		readonlyRedirectRequest.DevExt				= DevExt;
		readonlyRedirectRequest.OriginalTopLevelIrp = topLevelIrp;
		readonlyRedirectRequest.ReadonlyDismount	= 0;
		
		IoSetTopLevelIrp( (PIRP)&readonlyRedirectRequest );

		if (synchronousIo) {

			status = ZwReadFile( ccb->ReadonlyFileHandle,
								 NULL,
								 NULL,
								 NULL,
								 &ioStatusBlock,
								 outputBuffer,
								 read.Length,
								 &read.ByteOffset,
								 &read.Key );

#if 0
			status = NtReadFile( ccb->ReadonlyFileHandle,
								 NULL,
								 NULL,
								 NULL,
								 &ioStatusBlock,
								 outputBuffer,
								 read.Length,
								 &read.ByteOffset,
								 &read.Key );
#endif

		} else {

			HANDLE eventHandle;

			status = ZwCreateEvent( &eventHandle,
									GENERIC_READ,
									NULL,
									SynchronizationEvent,
									FALSE );

			if (status != STATUS_SUCCESS) {

				NDAS_BUGON( FALSE );
				leave;
			}

			status = ZwReadFile( ccb->ReadonlyFileHandle,
								 eventHandle,
								 NULL,
								 NULL,
								 &ioStatusBlock,
								 outputBuffer,
								 read.Length,
								 &read.ByteOffset,
								 &read.Key );
		
			if (status == STATUS_PENDING) {

				status = ZwWaitForSingleObject(eventHandle, TRUE, NULL);

				if (status != STATUS_SUCCESS) {

					NDAS_BUGON( FALSE );

				} else {

					status = ioStatusBlock.Status;
				}
			}

			ZwClose( eventHandle );
		}

		ASSERT( readonlyRedirectRequest.DebugTag == (READONLY_REDIRECT_REQUEST_TAG - 4) );
		IoSetTopLevelIrp( topLevelIrp );

		if (userModeAddressOutputBuffer == TRUE) {
#if 1
			RtlCopyMemory( originalOutputBuffer,
						   outputBuffer,
						   read.Length );

			ExFreePoolWithTag( outputBuffer, LFS_ALLOC_TAG );
#else
			IoFreeMdl( mdl );
#endif
			outputBuffer = originalOutputBuffer;
		}

		if (!NT_SUCCESS(status)) {

			ASSERT( ioStatusBlock.Information == 0 );
			ioStatusBlock.Information = 0;
		}

		Irp->IoStatus.Status	  = status;
		Irp->IoStatus.Information = ioStatusBlock.Information; 

		if (Irp->IoStatus.Status == STATUS_SUCCESS && synchronousIo && !pagingIo) {

			fileObject->CurrentByteOffset.QuadPart = read.ByteOffset.QuadPart + Irp->IoStatus.Information;
		}

		if (fileObject->SectionObjectPointer == NULL)
			fileObject->SectionObjectPointer = &fcb->NonPaged->SectionObjectPointers;

		if (Irp->IoStatus.Status != STATUS_SUCCESS) {

			SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE,
						   ("%d %d read.ByteOffset.QuadPart = %I64x, read.Length = %x, totalReadRequestLength = %x lastStatus = %x\n", 
							nonCachedIo, pagingIo, read.ByteOffset.QuadPart, read.Length, Irp->IoStatus.Information, status) );

			PrintIrp( LFS_DEBUG_READONLY_TRACE, "RedirectIrpMajorRead", &DevExt->LfsDeviceExt, Irp );
		}

		SPY_LOG_PRINT( NT_SUCCESS(status) ? LFS_DEBUG_READONLY_NOISE : LFS_DEBUG_READONLY_TRACE,
					   ("ReadonlyRedirectIrpMajorRead: %wZ, synchronousIo = %d, length = %d, status = %x, "
					    "byteOffset = %I64d, ioStatusBlock.Status = %X, ioStatusBlock.Information = %d\n",
						 &ccb->ReadonlyFileObject->FileName, synchronousIo, read.Length, status, read.ByteOffset.QuadPart, ioStatusBlock.Status, ioStatusBlock.Information) );


	} finally {

		if (AbnormalTermination()) {

			//status = GetExceptionCode();

			//if (NT_SUCCESS(status)) {

			//	NDAS_BUGON( FALSE );
			status = STATUS_UNSUCCESSFUL;
			//}
		}

		if (!NT_SUCCESS(status)) {

			Irp->IoStatus.Status = status;
			Irp->IoStatus.Information = ioStatusBlock.Information;
		}

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
	}

	return status;
}

NTSTATUS
ReadonlyRedirectIrpMajorQueryInformation (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	)
{
	NTSTATUS			status = STATUS_UNSUCCESSFUL;
	IO_STATUS_BLOCK		ioStatusBlock = {0, 0};

	PIO_STACK_LOCATION  irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT		fileObject = irpSp->FileObject;

	PNDAS_FCB			fcb = fileObject->FsContext;
	PNDAS_CCB			ccb = fileObject->FsContext2;


	if (ReadonlyLookUpCcb(DevExt->LfsDeviceExt.Readonly, fileObject) != ccb) {

		NDAS_BUGON( FALSE );
	}

	if (ccb->Fcb != fcb) {

		NDAS_BUGON( FALSE );
	}

	if (ccb->Corrupted == TRUE) {

		status = Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
		return status;
	}

	try {

		struct QueryFile			queryFile;

		PVOID						inputBuffer = NULL;
		ULONG						inputBufferLength = 0;
		PVOID						outputBuffer = NULL;
		BOOLEAN						userModeAddressOutputBuffer = FALSE;
		PVOID						originalOutputBuffer = NULL;
		ULONG						outputBufferLength;
		PMDL						mdl = NULL;

		//ULONG						returnLength;

		READONLY_REDIRECT_REQUEST	readonlyRedirectRequest;
		PIRP						topLevelIrp;

		queryFile.FileInformationClass	= irpSp->Parameters.QueryFile.FileInformationClass;
		queryFile.Length				= irpSp->Parameters.QueryFile.Length;

		outputBufferLength				= queryFile.Length;

		if (outputBufferLength) {
			
			do {

				if (FlagOn(Irp->Flags, IRP_BUFFERED_IO) && Irp->AssociatedIrp.SystemBuffer) {

					ASSERT( !(Irp->Flags & IRP_ASSOCIATED_IRP) );
					ASSERT( Irp->MdlAddress == NULL );

					outputBuffer = Irp->AssociatedIrp.SystemBuffer;
					userModeAddressOutputBuffer = FALSE;

					break;
				}

				if (Irp->MdlAddress) {

					outputBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
					userModeAddressOutputBuffer = FALSE;
	
					ASSERT( outputBuffer );
			
					break;
				}

				if (Irp->UserBuffer) {
					
					ASSERT( Irp->RequestorMode == UserMode );

					try {

						if (Irp->UserBuffer && Irp->RequestorMode != KernelMode) {

							ProbeForWrite( Irp->UserBuffer, 
										   outputBufferLength, 
										   sizeof(UCHAR) );
						}

					} except (Irp->RequestorMode != KernelMode ? EXCEPTION_EXECUTE_HANDLER: EXCEPTION_CONTINUE_SEARCH) {

						status = GetExceptionCode();
						NDAS_BUGON( FALSE );
					}

					outputBuffer = Irp->UserBuffer;
					userModeAddressOutputBuffer = TRUE;
					
					break;
				}

				if (Irp->AssociatedIrp.SystemBuffer) {
					
					outputBuffer = Irp->AssociatedIrp.SystemBuffer;
					userModeAddressOutputBuffer = FALSE;
					
					break;
				}

				NDAS_BUGON( FALSE );
			
			} while (0);

			if (userModeAddressOutputBuffer) {

				originalOutputBuffer = outputBuffer;

#if 1
				outputBuffer = ExAllocatePoolWithTag( NonPagedPool, 
													  outputBufferLength, 
													  LFS_ALLOC_TAG );
#else
				mdl = IoAllocateMdl( outputBuffer, outputBufferLength, FALSE, FALSE, NULL );
				outputBuffer = MmGetSystemAddressForMdlSafe(mdl, NormalPagePriority);
#endif
			}
		}

		RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

		topLevelIrp = IoGetTopLevelIrp();
		ASSERT( topLevelIrp == NULL || topLevelIrp == (PIRP)FSRTL_FSP_TOP_LEVEL_IRP );

		readonlyRedirectRequest.Tag					= READONLY_REDIRECT_REQUEST_TAG;
#if DBG
		readonlyRedirectRequest.DebugTag			= READONLY_REDIRECT_REQUEST_TAG;
#endif
		readonlyRedirectRequest.DevExt				= DevExt;
		readonlyRedirectRequest.OriginalTopLevelIrp = topLevelIrp;
		readonlyRedirectRequest.ReadonlyDismount	= 0;

		IoSetTopLevelIrp( (PIRP)&readonlyRedirectRequest );

		status = ZwQueryInformationFile( ccb->ReadonlyFileHandle,
										 &ioStatusBlock,
										 outputBuffer,
										 queryFile.Length,
										 queryFile.FileInformationClass );

#if 0
		returnLength = 0;

		status = IoQueryFileInformation( ccb->ReadonlyFileObject,
										 queryFile.FileInformationClass,
										 queryFile.Length,
										 outputBuffer,
										 &returnLength );
#endif

		ASSERT( readonlyRedirectRequest.DebugTag == (READONLY_REDIRECT_REQUEST_TAG - 4) ||
			    (status == STATUS_INVALID_DEVICE_REQUEST && fcb->FullFileName.Length == DevExt->LfsDeviceExt.NetdiskPartitionInformation.VolumeName.Length) );
			    
		IoSetTopLevelIrp( topLevelIrp );

		if (userModeAddressOutputBuffer == TRUE) {
#if 1
			RtlCopyMemory( originalOutputBuffer,
						   outputBuffer,
						   outputBufferLength );

			ExFreePoolWithTag( outputBuffer, LFS_ALLOC_TAG );
#else
			IoFreeMdl( mdl );
#endif
			outputBuffer = originalOutputBuffer;
		}

		if (NT_SUCCESS(status)) {

			ASSERT( status == STATUS_SUCCESS );
		}

		if (status == STATUS_BUFFER_OVERFLOW) {

			ASSERT( queryFile.Length == ioStatusBlock.Information );
			//ASSERT( queryFile.Length == returnLength );
		}

		SPY_LOG_PRINT( (NT_SUCCESS(status) || status == STATUS_INVALID_PARAMETER) ? LFS_DEBUG_READONLY_NOISE : LFS_DEBUG_READONLY_TRACE,
				       ("ReadonlyRedirectIrpMajorQueryInformation: %wZ, fileHandle = %p queryFile.FileInformationClass = %d"
					    "status = %x, length = %d, returnLength = %d\n",
						 &fileObject->FileName, ccb->ReadonlyFileHandle, queryFile.FileInformationClass, status, queryFile.Length, ioStatusBlock.Information) );

		if (!(status == STATUS_SUCCESS || status == STATUS_BUFFER_OVERFLOW)) {

			ASSERT( ioStatusBlock.Information == 0 );
			ioStatusBlock.Information = 0;
			//ASSERT( returnLength == 0 );
			//returnLength = 0;
		} 

		if (status == STATUS_BUFFER_OVERFLOW) {

			SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE,
					       ("ReadonlyRedirectIrpMajorQueryInformation: fileHandle = %p "
						    "status = %x, length = %d, returnLength = %d\n",
							 ccb->ReadonlyFileHandle, status, queryFile.Length, ioStatusBlock.Information) );
		}

		Irp->IoStatus.Status	  = status;
		Irp->IoStatus.Information = ioStatusBlock.Information;

	} finally {

		if (AbnormalTermination()) {

			//status = GetExceptionCode();

			//if (NT_SUCCESS(status)) {

			//	NDAS_BUGON( FALSE );
			status = STATUS_UNSUCCESSFUL;
			//}
		}

		if (!(status == STATUS_SUCCESS || status == STATUS_BUFFER_OVERFLOW)) {

			Irp->IoStatus.Status = status;
			Irp->IoStatus.Information = ioStatusBlock.Information;
		}

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
	}

	return status;
}

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
ReadonlyRedirectIrpMajorQueryEa (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	)
{
	NTSTATUS			status = STATUS_UNSUCCESSFUL;
	IO_STATUS_BLOCK		ioStatusBlock = {0, 0};

	PIO_STACK_LOCATION  irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT		fileObject = irpSp->FileObject;

	PNDAS_FCB			fcb = fileObject->FsContext;
	PNDAS_CCB			ccb = fileObject->FsContext2;


	if (ReadonlyLookUpCcb(DevExt->LfsDeviceExt.Readonly, fileObject) != ccb) {

		NDAS_BUGON( FALSE );
	}

	if (ccb->Fcb != fcb) {

		NDAS_BUGON( FALSE );
	}

	if (ccb->Corrupted == TRUE) {

		status = Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
		return status;
	}

	try {

		struct QueryEa				queryEa;

		PVOID						inputBuffer;
		ULONG						inputBufferLength; /* = queryEa->EaListLength;*/
		PVOID						outputBuffer = MapOutputBuffer(Irp);
		ULONG						outputBufferLength;

		READONLY_REDIRECT_REQUEST	readonlyRedirectRequest;
		PIRP						topLevelIrp;

		BOOLEAN						returnSingleEntry;
		BOOLEAN						restartScan;
		BOOLEAN						indexSpecified;


		queryEa.EaIndex			= irpSp->Parameters.QueryEa.EaIndex;
		queryEa.EaList			= irpSp->Parameters.QueryEa.EaList;
		queryEa.EaListLength	= irpSp->Parameters.QueryEa.EaListLength;
		queryEa.Length			= irpSp->Parameters.QueryEa.Length;
		
		inputBuffer				= queryEa.EaList;
		outputBufferLength		= queryEa.Length;

		if (inputBuffer != NULL) {

			PFILE_GET_EA_INFORMATION	fileGetEa = (PFILE_GET_EA_INFORMATION)inputBuffer;
		
			inputBufferLength = 0;
			
			while (fileGetEa->NextEntryOffset) {

				inputBufferLength += fileGetEa->NextEntryOffset;
				fileGetEa = (PFILE_GET_EA_INFORMATION)((UINT8 *)fileGetEa + fileGetEa->NextEntryOffset);
			}

			inputBufferLength += (sizeof(FILE_GET_EA_INFORMATION) - sizeof(CHAR) + fileGetEa->EaNameLength);
		
		} else {
		
			inputBufferLength = 0;
		}

		returnSingleEntry	= BooleanFlagOn(irpSp->Flags, SL_RETURN_SINGLE_ENTRY);
		restartScan			= BooleanFlagOn(irpSp->Flags, SL_RESTART_SCAN);
		indexSpecified		= BooleanFlagOn(irpSp->Flags, SL_INDEX_SPECIFIED);


		topLevelIrp = IoGetTopLevelIrp();
		ASSERT( topLevelIrp == NULL );

		readonlyRedirectRequest.Tag					= READONLY_REDIRECT_REQUEST_TAG;
#if DBG
		readonlyRedirectRequest.DebugTag			= READONLY_REDIRECT_REQUEST_TAG;
#endif
		readonlyRedirectRequest.DevExt				= DevExt;
		readonlyRedirectRequest.OriginalTopLevelIrp = topLevelIrp;
		readonlyRedirectRequest.ReadonlyDismount	= 0;

		IoSetTopLevelIrp( (PIRP)&readonlyRedirectRequest );

		status = NtQueryEaFile( ccb->ReadonlyFileHandle,
								&ioStatusBlock,
								outputBuffer,
								queryEa.Length,
								returnSingleEntry,
								queryEa.EaList,
								inputBufferLength,
								indexSpecified ? &queryEa.EaIndex : NULL,
								restartScan );

		ASSERT( readonlyRedirectRequest.DebugTag == (READONLY_REDIRECT_REQUEST_TAG - 4) );
		IoSetTopLevelIrp( topLevelIrp );

		SPY_LOG_PRINT( NT_SUCCESS(status) ? LFS_DEBUG_READONLY_INFO : LFS_DEBUG_READONLY_INFO,
					   ("ReadonlyRedirectIrpMajorFileSystemControl: %wZ, openFileId = %p, function = %d, "
					    "status=%x ioStatusBlock.Status = %x, ioStatusBlock.Information = %d\n",
						 &fileObject->FileName, ccb->ReadonlyFileHandle, status, ioStatusBlock.Status, ioStatusBlock.Information) );

		if (NT_SUCCESS(status)) {

			ASSERT( status == STATUS_SUCCESS );
		}

		if (status == STATUS_BUFFER_OVERFLOW) {

			ASSERT( outputBufferLength == ioStatusBlock.Information );
		}

		if (!(status == STATUS_SUCCESS || status == STATUS_BUFFER_OVERFLOW)) {

			ASSERT( ioStatusBlock.Information == 0 );
			ioStatusBlock.Information = 0;
		} 

		if (status == STATUS_BUFFER_OVERFLOW) {

			SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE,
					       ("ReadonlyRedirectIrpMajorQueryEa: fileHandle = %p "
						    "status = %x, length = %d, returnLength = %d\n",
							 ccb->ReadonlyFileHandle, status, inputBufferLength, ioStatusBlock.Information) );
		}

		Irp->IoStatus.Status	  = status;
		Irp->IoStatus.Information = ioStatusBlock.Information;

	} finally {

		if (AbnormalTermination()) {

			//status = GetExceptionCode();

			//if (NT_SUCCESS(status)) {

			//	NDAS_BUGON( FALSE );
			status = STATUS_UNSUCCESSFUL;
			//}
		}

		if (!(status == STATUS_SUCCESS || status == STATUS_BUFFER_OVERFLOW)) {

			Irp->IoStatus.Status = status;
			Irp->IoStatus.Information = ioStatusBlock.Information;
		}

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
	}

	return status;
}

NTSTATUS
ReadonlyRedirectIrpMajorQueryVolumeInformation (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	)
{
	NTSTATUS			status = STATUS_UNSUCCESSFUL;
	//ULONG				returnLength = 0;
	IO_STATUS_BLOCK		ioStatusBlock = {0, 0};

	PIO_STACK_LOCATION  irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT		fileObject = irpSp->FileObject;

	PNDAS_FCB			fcb = fileObject->FsContext;
	PNDAS_CCB			ccb = fileObject->FsContext2;


	if (ReadonlyLookUpCcb(DevExt->LfsDeviceExt.Readonly, fileObject) != ccb) {

		NDAS_BUGON( FALSE );
	}

	if (ccb->Fcb != fcb) {

		NDAS_BUGON( FALSE );
	}

	if (ccb->Corrupted == TRUE) {

		status = Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
		return status;
	}

	try {

		struct QueryVolume			queryVolume;

		PVOID						inputBuffer = NULL;
		ULONG						inputBufferLength = 0;
		PVOID						outputBuffer = NULL;
		BOOLEAN						userModeAddressOutputBuffer = FALSE;
		PVOID						originalOutputBuffer = NULL;
		ULONG						outputBufferLength;
		PMDL						mdl = NULL;

		READONLY_REDIRECT_REQUEST	readonlyRedirectRequest;
		PIRP						topLevelIrp;

		queryVolume.FsInformationClass	= irpSp->Parameters.QueryVolume.FsInformationClass;
		queryVolume.Length				= irpSp->Parameters.QueryVolume.Length;
		
		outputBufferLength				= queryVolume.Length;

		if (outputBufferLength) {
			
			do {

				if (FlagOn(Irp->Flags, IRP_BUFFERED_IO) && Irp->AssociatedIrp.SystemBuffer) {

					ASSERT( !(Irp->Flags & IRP_ASSOCIATED_IRP) );
					ASSERT( Irp->MdlAddress == NULL );

					outputBuffer = Irp->AssociatedIrp.SystemBuffer;
					userModeAddressOutputBuffer = FALSE;

					break;
				}

				if (Irp->MdlAddress) {

					outputBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
					userModeAddressOutputBuffer = FALSE;
	
					ASSERT( outputBuffer );
			
					break;
				}

				if (Irp->UserBuffer) {
					
					ASSERT( Irp->RequestorMode == UserMode );

					try {

						if (Irp->UserBuffer && Irp->RequestorMode != KernelMode) {

							ProbeForWrite( Irp->UserBuffer, 
										   outputBufferLength, 
										   sizeof(UCHAR) );
						}

					} except (Irp->RequestorMode != KernelMode ? EXCEPTION_EXECUTE_HANDLER: EXCEPTION_CONTINUE_SEARCH) {

						status = GetExceptionCode();
						NDAS_BUGON( FALSE );
					}

					outputBuffer = Irp->UserBuffer;
					userModeAddressOutputBuffer = TRUE;
					
					break;
				}

				if (Irp->AssociatedIrp.SystemBuffer) {
					
					outputBuffer = Irp->AssociatedIrp.SystemBuffer;
					userModeAddressOutputBuffer = FALSE;
					
					break;
				}

				NDAS_BUGON( FALSE );
			
			} while (0);

			if (userModeAddressOutputBuffer) {

				originalOutputBuffer = outputBuffer;

#if 1
				outputBuffer = ExAllocatePoolWithTag( NonPagedPool, 
													  outputBufferLength, 
													  LFS_ALLOC_TAG );
#else
				mdl = IoAllocateMdl( outputBuffer, outputBufferLength, FALSE, FALSE, NULL );
				outputBuffer = MmGetSystemAddressForMdlSafe(mdl, NormalPagePriority);
#endif
			}
		}

		topLevelIrp = IoGetTopLevelIrp();
		ASSERT( topLevelIrp == NULL );

		readonlyRedirectRequest.Tag					= READONLY_REDIRECT_REQUEST_TAG;
#if DBG
		readonlyRedirectRequest.DebugTag			= READONLY_REDIRECT_REQUEST_TAG;
#endif
		readonlyRedirectRequest.DevExt				= DevExt;
		readonlyRedirectRequest.OriginalTopLevelIrp = topLevelIrp;
		readonlyRedirectRequest.ReadonlyDismount	= 0;

		IoSetTopLevelIrp( (PIRP)&readonlyRedirectRequest );

#if 0
		status = IoQueryVolumeInformation( ccb->ReadonlyFileObject,
										   queryVolume.FsInformationClass,
										   queryVolume.Length,
										   outputBuffer,
										   &returnLength );
#else
		status = ZwQueryVolumeInformationFile( ccb->ReadonlyFileHandle,
											   &ioStatusBlock,
											   outputBuffer,
											   queryVolume.Length,
											   queryVolume.FsInformationClass );
#endif
		ASSERT( readonlyRedirectRequest.DebugTag == (READONLY_REDIRECT_REQUEST_TAG - 4) );
		IoSetTopLevelIrp( topLevelIrp );

		if (userModeAddressOutputBuffer == TRUE) {
#if 1
			RtlCopyMemory( originalOutputBuffer,
						   outputBuffer,
						   outputBufferLength );

			ExFreePoolWithTag( outputBuffer, LFS_ALLOC_TAG );
#else
			IoFreeMdl( mdl );
#endif
			outputBuffer = originalOutputBuffer;
		}

		if (NT_SUCCESS(status)) {

			ASSERT( status == STATUS_SUCCESS );
		}

		if (status == STATUS_BUFFER_OVERFLOW) {

			//ASSERT( queryVolume.Length == returnLength );
			ASSERT( queryVolume.Length == ioStatusBlock.Information );
		}

		SPY_LOG_PRINT( NT_SUCCESS(status) ? LFS_DEBUG_READONLY_NOISE : LFS_DEBUG_READONLY_TRACE,
				       ("ReadonlyRedirectIrpMajorQueryVolumeInformation: fileHandle = %p "
					    "status = %x, length = %d, returnLength = %d\n",
						 ccb->ReadonlyFileHandle, status, queryVolume.Length, ioStatusBlock.Information) );

		if (!(status == STATUS_SUCCESS || status == STATUS_BUFFER_OVERFLOW)) {

			//ASSERT( returnLength == 0 );
			//returnLength = 0;
			ASSERT( ioStatusBlock.Information == 0 );
			ioStatusBlock.Information = 0;
		} 

		if (status == STATUS_BUFFER_OVERFLOW) {

			SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE,
					       ("ReadonlyRedirectIrpMajorQueryVolumeInformation: fileHandle = %p "
						    "status = %x, length = %d, returnLength = %d\n",
							 ccb->ReadonlyFileHandle, status, queryVolume.Length, ioStatusBlock.Information) );
		}

		Irp->IoStatus.Status	  = status;
		//Irp->IoStatus.Information = returnLength;
		Irp->IoStatus.Information = ioStatusBlock.Information;

	} finally {

		if (AbnormalTermination()) {

			//status = GetExceptionCode();

			//if (NT_SUCCESS(status)) {

			//	NDAS_BUGON( FALSE );
			status = STATUS_UNSUCCESSFUL;
			//}
		}

		if (!(status == STATUS_SUCCESS || status == STATUS_BUFFER_OVERFLOW)) {

			Irp->IoStatus.Status = status;
			//Irp->IoStatus.Information = returnLength;
			Irp->IoStatus.Information = ioStatusBlock.Information;
		}

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
	}

	return status;
}


NTSTATUS
ReadonlyRedirectIrpMajorFileSystemControl (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	)
{
	NTSTATUS			status = STATUS_UNSUCCESSFUL;
	IO_STATUS_BLOCK		ioStatusBlock = {0, 0};

	PIO_STACK_LOCATION  irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT		fileObject = irpSp->FileObject;

	PNDAS_FCB			fcb = fileObject->FsContext;
	PNDAS_CCB			ccb = fileObject->FsContext2;

	READONLY_REDIRECT_REQUEST	readonlyRedirectRequest;
	PIRP						topLevelIrp;

	if (ReadonlyLookUpCcb(DevExt->LfsDeviceExt.Readonly, fileObject) != ccb) {

		NDAS_BUGON( FALSE );
	}

	if (ccb->Fcb != fcb) {

		NDAS_BUGON( FALSE );
	}

	if (ccb->Corrupted == TRUE) {

		status = Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
		return status;
	}

	if (IS_WINDOWS2K()) {

		if (irpSp->Parameters.DeviceIoControl.IoControlCode == FSCTL_MOVE_FILE) {

			status = Irp->IoStatus.Status = STATUS_MEDIA_WRITE_PROTECTED;
			Irp->IoStatus.Information = 0 ;
		
			IoCompleteRequest( Irp, IO_DISK_INCREMENT );
			
			return status;
		}
	}

	try {

		struct FileSystemControl	fileSystemControl;

		PVOID						inputBuffer = NULL;
		BOOLEAN						userModeAddressInputBuffer = FALSE;
		PVOID						originalInputBuffer = NULL;
		ULONG						inputBufferLength;

		PVOID						outputBuffer = NULL;
		BOOLEAN						userModeAddressOutputBuffer = FALSE;
		PVOID						originalOutputBuffer = NULL;
		ULONG						outputBufferLength;

		MOVE_FILE_DATA				moveFileDataInput;	
		MARK_HANDLE_INFO			markHandleInfoInput;	
	

		fileSystemControl.FsControlCode			= irpSp->Parameters.FileSystemControl.FsControlCode;
		fileSystemControl.InputBufferLength		= irpSp->Parameters.FileSystemControl.InputBufferLength;
		fileSystemControl.OutputBufferLength	= irpSp->Parameters.FileSystemControl.OutputBufferLength;
		fileSystemControl.Type3InputBuffer		= irpSp->Parameters.FileSystemControl.Type3InputBuffer;

		outputBufferLength = fileSystemControl.OutputBufferLength;
		inputBufferLength  = fileSystemControl.InputBufferLength;

		if (inputBufferLength) {

			if ((fileSystemControl.FsControlCode & 0x3) == METHOD_BUFFERED) {

				inputBuffer = Irp->AssociatedIrp.SystemBuffer;
				userModeAddressInputBuffer = FALSE;

			} else if ((fileSystemControl.FsControlCode & 0x3) == METHOD_OUT_DIRECT || (fileSystemControl.FsControlCode & 0x3) == METHOD_IN_DIRECT) {

				inputBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
				userModeAddressInputBuffer = FALSE;
			
			} else if ((fileSystemControl.FsControlCode & 0x3) == METHOD_NEITHER) {

				try {

					if (Irp->UserBuffer && Irp->RequestorMode != KernelMode) {

						ProbeForRead( Irp->UserBuffer, 
									  inputBufferLength, 
									  sizeof(UCHAR) );
					}

				} except (Irp->RequestorMode != KernelMode ? EXCEPTION_EXECUTE_HANDLER: EXCEPTION_CONTINUE_SEARCH) {

					status = GetExceptionCode();
					NDAS_BUGON( FALSE );
				}

				inputBuffer = Irp->UserBuffer;
				userModeAddressInputBuffer = TRUE;
			}

			if (userModeAddressInputBuffer) {

				originalInputBuffer = inputBuffer;
				inputBuffer = ExAllocatePoolWithTag( NonPagedPool, 
													 inputBufferLength, 
													 LFS_ALLOC_TAG );

				RtlCopyMemory( inputBuffer,
							   originalInputBuffer,
							   inputBufferLength );
			}
		}

		if (outputBufferLength) {

			if ((fileSystemControl.FsControlCode & 0x3) == METHOD_BUFFERED) {

				outputBuffer = Irp->AssociatedIrp.SystemBuffer;
				userModeAddressOutputBuffer = FALSE;

			} else if ((fileSystemControl.FsControlCode & 0x3) == METHOD_OUT_DIRECT || (fileSystemControl.FsControlCode & 0x3) == METHOD_IN_DIRECT) {

				outputBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
				userModeAddressOutputBuffer = FALSE;
			
			} else if ((fileSystemControl.FsControlCode & 0x3) == METHOD_NEITHER) {

				try {

					if (Irp->UserBuffer && Irp->RequestorMode != KernelMode) {

						ProbeForWrite( Irp->UserBuffer, 
									   outputBufferLength, 
									   sizeof(UCHAR) );
					}

				} except (Irp->RequestorMode != KernelMode ? EXCEPTION_EXECUTE_HANDLER: EXCEPTION_CONTINUE_SEARCH) {

					status = GetExceptionCode();
					NDAS_BUGON( FALSE );
				}

				outputBuffer = Irp->UserBuffer;
				userModeAddressOutputBuffer = TRUE;
			}

			if (userModeAddressOutputBuffer) {

				originalOutputBuffer = outputBuffer;
				outputBuffer = ExAllocatePoolWithTag( NonPagedPool, 
													  outputBufferLength, 
													  LFS_ALLOC_TAG );
			}
		}

		if (fileSystemControl.FsControlCode == FSCTL_MOVE_FILE) {

			PMOVE_FILE_DATA	moveFileData = inputBuffer;	
			PNDAS_CCB		moveFileCcb;


			moveFileCcb = ReadonlyLookUpCcbByHandle( DevExt, moveFileData->FileHandle );	

			if (!moveFileCcb) {

				NDAS_BUGON( FALSE );

				status = Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
				Irp->IoStatus.Information = 0;
			
				leave;
			}

			moveFileDataInput = *moveFileData;
			moveFileDataInput.FileHandle	= moveFileCcb->ReadonlyFileHandle;

			inputBuffer = &moveFileDataInput;
			ASSERT( inputBufferLength == sizeof(moveFileDataInput) );
			inputBufferLength = sizeof(moveFileDataInput);

		} else if (fileSystemControl.FsControlCode == FSCTL_MARK_HANDLE) {

			PMARK_HANDLE_INFO	markHandleInfo = inputBuffer;	
			PNDAS_CCB			markHandleCcb;


			markHandleCcb = ReadonlyLookUpCcbByHandle( DevExt, markHandleInfo->VolumeHandle );	

			if (!markHandleCcb) {

				NDAS_BUGON( FALSE );

				status = Irp->IoStatus.Status = STATUS_NOT_IMPLEMENTED;
				Irp->IoStatus.Information = 0;
			
				leave;
			}

			markHandleInfoInput = *markHandleInfo;
			markHandleInfoInput.VolumeHandle = markHandleCcb->ReadonlyFileHandle;

			inputBuffer = &markHandleInfoInput;
			ASSERT( inputBufferLength == sizeof(markHandleInfoInput) );
			inputBufferLength = sizeof(markHandleInfoInput);
		}

		RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

		topLevelIrp = IoGetTopLevelIrp();
		ASSERT( topLevelIrp == NULL );

		readonlyRedirectRequest.Tag					= READONLY_REDIRECT_REQUEST_TAG;
#if DBG
		readonlyRedirectRequest.DebugTag			= READONLY_REDIRECT_REQUEST_TAG;
#endif
		readonlyRedirectRequest.DevExt				= DevExt;
		readonlyRedirectRequest.OriginalTopLevelIrp = topLevelIrp;
		readonlyRedirectRequest.ReadonlyDismount	= 0;

		IoSetTopLevelIrp( (PIRP)&readonlyRedirectRequest );

		status = ZwFsControlFile( ccb->ReadonlyFileHandle,
							      NULL,
								  NULL,
								  NULL,
								  &ioStatusBlock,
								  fileSystemControl.FsControlCode,
								  inputBuffer,
								  inputBufferLength,
								  outputBuffer,
								  outputBufferLength );

		ASSERT( readonlyRedirectRequest.DebugTag == (READONLY_REDIRECT_REQUEST_TAG - 4) );
		IoSetTopLevelIrp( topLevelIrp );

		SPY_LOG_PRINT( NT_SUCCESS(status) ? LFS_DEBUG_READONLY_NOISE : LFS_DEBUG_READONLY_TRACE,
					   ("ReadonlyRedirectIrpMajorFileSystemControl: openFileId = %p, function = %d, "
					    "fileSystemControlStatus = %x, status=%x ioStatusBlock.Status = %x, ioStatusBlock.Information = %d\n",
						 ccb->ReadonlyFileHandle, (fileSystemControl.FsControlCode & 0x00003FFC) >> 2, status, ioStatusBlock.Status, ioStatusBlock.Information) );

		if (userModeAddressOutputBuffer == TRUE) {

			RtlCopyMemory( originalOutputBuffer,
						   outputBuffer,
						   outputBufferLength );

			ExFreePoolWithTag( outputBuffer, LFS_ALLOC_TAG );

			outputBuffer = originalOutputBuffer;
		}

		if (userModeAddressInputBuffer == TRUE) {

			ExFreePoolWithTag( inputBuffer, LFS_ALLOC_TAG );

			inputBuffer = originalInputBuffer;
		}

		if (NT_SUCCESS(status)) {

			ASSERT( status == STATUS_SUCCESS );
		}

		if (status == STATUS_BUFFER_OVERFLOW) {

			ASSERT( outputBufferLength == ioStatusBlock.Information );
		}

		if (!(status == STATUS_SUCCESS || status == STATUS_BUFFER_OVERFLOW)) {

			ASSERT( ioStatusBlock.Information == 0 );
			ioStatusBlock.Information = 0;
		} 

		if (status == STATUS_BUFFER_OVERFLOW) {

			SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE,
					       ("ReadonlyRedirectIrpMajorFileSystemControl: fileHandle = %p "
						    "status = %x, length = %d, returnLength = %d\n",
							 ccb->ReadonlyFileHandle, status, inputBufferLength, ioStatusBlock.Information) );
		}

		Irp->IoStatus.Status	  = status;
		Irp->IoStatus.Information = ioStatusBlock.Information;

	} finally {

		if (AbnormalTermination()) {

			//status = GetExceptionCode();

			//if (NT_SUCCESS(status)) {

			//	NDAS_BUGON( FALSE );
			status = STATUS_UNSUCCESSFUL;
			//}
		}

		if (!(status == STATUS_SUCCESS || status == STATUS_BUFFER_OVERFLOW)) {

			Irp->IoStatus.Status = status;
			Irp->IoStatus.Information = ioStatusBlock.Information;
		}

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
	}

	return status;
}


NTSTATUS
ReadonlyRedirectIrpMajorLockControl (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	)
{
	NTSTATUS			status = STATUS_UNSUCCESSFUL;
	IO_STATUS_BLOCK		ioStatusBlock = {0, 0};

	PIO_STACK_LOCATION  irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT		fileObject = irpSp->FileObject;

	PNDAS_FCB			fcb = fileObject->FsContext;
	PNDAS_CCB			ccb = fileObject->FsContext2;


	if (ReadonlyLookUpCcb(DevExt->LfsDeviceExt.Readonly, fileObject) != ccb) {

		NDAS_BUGON( FALSE );
	}

	if (ccb->Fcb != fcb) {

		NDAS_BUGON( FALSE );
	}

	if (ccb->Corrupted == TRUE) {

		status = Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
		return status;
	}

	try {

		struct LockControl			lockControl;

		BOOLEAN						failImmediately;
		BOOLEAN						exclusiveLock;

		READONLY_REDIRECT_REQUEST	readonlyRedirectRequest;
		PIRP						topLevelIrp;


		lockControl.ByteOffset	= irpSp->Parameters.LockControl.ByteOffset;
		lockControl.Key			= irpSp->Parameters.LockControl.Key;
		lockControl.Length		= irpSp->Parameters.LockControl.Length;


		failImmediately		= BooleanFlagOn(irpSp->Flags, SL_FAIL_IMMEDIATELY);
		exclusiveLock		= BooleanFlagOn(irpSp->Flags, SL_EXCLUSIVE_LOCK);

		topLevelIrp = IoGetTopLevelIrp();
		ASSERT( topLevelIrp == NULL );

		readonlyRedirectRequest.Tag					= READONLY_REDIRECT_REQUEST_TAG;
#if DBG
		readonlyRedirectRequest.DebugTag			= READONLY_REDIRECT_REQUEST_TAG;
#endif
		readonlyRedirectRequest.DevExt				= DevExt;
		readonlyRedirectRequest.OriginalTopLevelIrp = topLevelIrp;
		readonlyRedirectRequest.ReadonlyDismount	= 0;

		IoSetTopLevelIrp( (PIRP)&readonlyRedirectRequest );

		if (irpSp->MinorFunction == IRP_MN_LOCK) {

			status = NtLockFile( ccb->ReadonlyFileHandle,
								 NULL,
								 NULL,
								 NULL,
								 &ioStatusBlock,
								 &lockControl.ByteOffset,
								 lockControl.Length,
								 lockControl.Key,
								 failImmediately,
								 exclusiveLock );

		} else if (irpSp->MinorFunction == IRP_MN_UNLOCK_SINGLE) {

			status = NtUnlockFile( ccb->ReadonlyFileHandle,
								   &ioStatusBlock,
								   &lockControl.ByteOffset,
								   lockControl.Length,
								   lockControl.Key );
		
		} else if (irpSp->MinorFunction == IRP_MN_UNLOCK_ALL) {

            status = FsRtlFastUnlockAll( &fcb->FileLock,
										 fileObject,
										 IoGetRequestorProcess(Irp),
										 NULL );
		
		} else {

			NDAS_BUGON( FALSE );
			status = STATUS_NOT_IMPLEMENTED;
		}

		ASSERT( readonlyRedirectRequest.DebugTag == (READONLY_REDIRECT_REQUEST_TAG - 4) );
		IoSetTopLevelIrp( topLevelIrp );

		SPY_LOG_PRINT( NT_SUCCESS(status) ? LFS_DEBUG_READONLY_INFO : LFS_DEBUG_READONLY_INFO,
					   ("ReadonlyRedirectIrpMajorLockControl: %wZ, openFileId = %p, function = %d, "
					    "status=%x ioStatusBlock.Status = %x, ioStatusBlock.Information = %d\n",
						 &fileObject->FileName, ccb->ReadonlyFileHandle, status, ioStatusBlock.Status, ioStatusBlock.Information) );

		if (NT_SUCCESS(status)) {

			ASSERT( status == STATUS_SUCCESS );
		}

		if (!(status == STATUS_SUCCESS)) {

			ASSERT( ioStatusBlock.Information == 0 );
			ioStatusBlock.Information = 0;
		} 

		Irp->IoStatus.Status	  = status;
		Irp->IoStatus.Information = ioStatusBlock.Information;

	} finally {

		if (AbnormalTermination()) {

			//status = GetExceptionCode();

			//if (NT_SUCCESS(status)) {

			//	NDAS_BUGON( FALSE );
			status = STATUS_UNSUCCESSFUL;
			//}
		}

		if (!(status == STATUS_SUCCESS || status == STATUS_BUFFER_OVERFLOW)) {

			Irp->IoStatus.Status = status;
			Irp->IoStatus.Information = ioStatusBlock.Information;
		}

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
	}

	return status;
}


NTSTATUS
ReadonlyRedirectIrpMajorCleanup (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	)
{
	NTSTATUS			status = STATUS_UNSUCCESSFUL;
	IO_STATUS_BLOCK		ioStatusBlock = {0, 0};

	PIO_STACK_LOCATION  irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT		fileObject = irpSp->FileObject;

	PNDAS_FCB			fcb = fileObject->FsContext;
	PNDAS_CCB			ccb = fileObject->FsContext2;


	if (ReadonlyLookUpCcb(DevExt->LfsDeviceExt.Readonly, fileObject) != ccb) {

		NDAS_BUGON( FALSE );
	}

	if (ccb->Fcb != fcb) {

		NDAS_BUGON( FALSE );
	}

	try {

		if (ccb->TypeOfOpen == UserFileOpen && fileObject->SectionObjectPointer) {

			LARGE_INTEGER		largeZero = {0,0};
		
			if (fcb->Header.PagingIoResource) {

				NDAS_BUGON( FALSE );

				status = Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
				Irp->IoStatus.Information = 0;

				leave;
			}				
			
            FsRtlFastUnlockAll( &fcb->FileLock,
								fileObject,
							    IoGetRequestorProcess(Irp),
								NULL );

            CcFlushCache( &fcb->NonPaged->SectionObjectPointers, NULL, 0, &ioStatusBlock );

			CcPurgeCacheSection( &fcb->NonPaged->SectionObjectPointers,
								 NULL,
								 0,
								 TRUE );
		}

		status = Irp->IoStatus.Status = STATUS_SUCCESS;
		Irp->IoStatus.Information = 0;
	
	} finally {

		if (AbnormalTermination()) {

			//status = GetExceptionCode();

			//if (NT_SUCCESS(status)) {

			//	NDAS_BUGON( FALSE );
			status = STATUS_UNSUCCESSFUL;
			//}
		}

		InterlockedDecrement( &fcb->UncleanCount );
		SetFlag( fileObject->Flags, FO_CLEANUP_COMPLETE );

		if (!NT_SUCCESS(status)) {

			Irp->IoStatus.Status = status;
			Irp->IoStatus.Information = ioStatusBlock.Information;
		}

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
	}

	return status;
}


NTSTATUS
ReadonlyRedirectIrpMajorQuerySecurity (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	)
{
	NTSTATUS			status = STATUS_UNSUCCESSFUL;
	ULONG				returnLength = 0;


	PIO_STACK_LOCATION  irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT		fileObject = irpSp->FileObject;

	PNDAS_FCB			fcb = fileObject->FsContext;
	PNDAS_CCB			ccb = fileObject->FsContext2;


	if (ReadonlyLookUpCcb(DevExt->LfsDeviceExt.Readonly, fileObject) != ccb) {

		NDAS_BUGON( FALSE );
	}

	if (ccb->Fcb != fcb) {

		NDAS_BUGON( FALSE );
	}

	if (ccb->Corrupted == TRUE) {

		status = Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
		return status;
	}

	try {

		struct QuerySecurity		querySecurity;

		PVOID						inputBuffer = NULL;
		ULONG						inputBufferLength = 0;
		PVOID						outputBuffer = MapOutputBuffer(Irp);

		READONLY_REDIRECT_REQUEST	readonlyRedirectRequest;
		PIRP						topLevelIrp;


		querySecurity.Length				= irpSp->Parameters.QuerySecurity.Length;
		querySecurity.SecurityInformation	= irpSp->Parameters.QuerySecurity.SecurityInformation;

		returnLength = 0;

		topLevelIrp = IoGetTopLevelIrp();
		ASSERT( topLevelIrp == NULL );

		readonlyRedirectRequest.Tag					= READONLY_REDIRECT_REQUEST_TAG;
#if DBG
		readonlyRedirectRequest.DebugTag			= READONLY_REDIRECT_REQUEST_TAG;
#endif
		readonlyRedirectRequest.DevExt				= DevExt;
		readonlyRedirectRequest.OriginalTopLevelIrp = topLevelIrp;
		readonlyRedirectRequest.ReadonlyDismount	= 0;

		IoSetTopLevelIrp( (PIRP)&readonlyRedirectRequest );

		status = ZwQuerySecurityObject( ccb->ReadonlyFileHandle,
										querySecurity.SecurityInformation,
										outputBuffer,
										querySecurity.Length,
										&returnLength );

		ASSERT( readonlyRedirectRequest.DebugTag == (READONLY_REDIRECT_REQUEST_TAG - 4) );
		IoSetTopLevelIrp( topLevelIrp );

		if (NT_SUCCESS(status)) {

			ASSERT( status == STATUS_SUCCESS );
		}

		if (status == STATUS_BUFFER_OVERFLOW) {

			ASSERT( querySecurity.Length == returnLength );
		}

		SPY_LOG_PRINT( NT_SUCCESS(status) ? LFS_DEBUG_READONLY_NOISE : LFS_DEBUG_READONLY_TRACE,
				       ("ReadonlyRedirectIrpMajorQuerySecurity: fileHandle = %p "
					    "status = %x, length = %d, returnLength = %d\n",
						 ccb->ReadonlyFileHandle, status, querySecurity.Length, returnLength) );

		if (!(status == STATUS_SUCCESS || status == STATUS_BUFFER_OVERFLOW || status == STATUS_BUFFER_TOO_SMALL)) {

			ASSERT( returnLength == 0 );
			returnLength = 0;
		} 

		if (status == STATUS_BUFFER_OVERFLOW || status == STATUS_BUFFER_TOO_SMALL) {

			SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE,
					       ("ReadonlyRedirectIrpMajorQuerySecurity: fileHandle = %p "
						    "status = %x, length = %d, returnLength = %d\n",
							 ccb->ReadonlyFileHandle, status, querySecurity.Length, returnLength) );
		}

		Irp->IoStatus.Status	  = status;
		Irp->IoStatus.Information = returnLength;

	} finally {

		if (AbnormalTermination()) {

			//status = GetExceptionCode();

			//if (NT_SUCCESS(status)) {

			//	NDAS_BUGON( FALSE );
			status = STATUS_UNSUCCESSFUL;
			//}
		}

		if (!(status == STATUS_SUCCESS || status == STATUS_BUFFER_OVERFLOW || status == STATUS_BUFFER_TOO_SMALL)) {

			Irp->IoStatus.Status = status;
			Irp->IoStatus.Information = returnLength;
		}

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
	}

	return status;
}


NTSTATUS
ReadonlyRedirectIrpMajorQueryQuota (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp
	)
{
	NTSTATUS			status = STATUS_UNSUCCESSFUL;
	IO_STATUS_BLOCK		ioStatusBlock = {0, 0};


	PIO_STACK_LOCATION  irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT		fileObject = irpSp->FileObject;

	PNDAS_FCB			fcb = fileObject->FsContext;
	PNDAS_CCB			ccb = fileObject->FsContext2;


	if (ReadonlyLookUpCcb(DevExt->LfsDeviceExt.Readonly, fileObject) != ccb) {

		NDAS_BUGON( FALSE );
	}

	if (ccb->Fcb != fcb) {

		NDAS_BUGON( FALSE );
	}

	if (ccb->Corrupted == TRUE) {

		status = Irp->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
		return status;
	}

	try {

		struct QueryQuota			queryQuota;

		PVOID						inputBuffer;
		ULONG						inputBufferLength;
		PVOID						outputBuffer;
		ULONG						outputBufferLength;

		BOOLEAN						restartScan;
		BOOLEAN						returnSingleEntry;

		READONLY_REDIRECT_REQUEST	readonlyRedirectRequest;
		PIRP						topLevelIrp;


		queryQuota.Length			= irpSp->Parameters.QueryQuota.Length;
		queryQuota.SidList			= irpSp->Parameters.QueryQuota.SidList;
		queryQuota.SidListLength	= irpSp->Parameters.QueryQuota.SidListLength;
		queryQuota.StartSid			= irpSp->Parameters.QueryQuota.StartSid;

		inputBuffer			= queryQuota.SidList;
		inputBufferLength	= queryQuota.SidListLength;
		outputBuffer		= MapOutputBuffer(Irp);
		outputBufferLength	= queryQuota.Length;

		restartScan				= BooleanFlagOn(irpSp->Flags, SL_RESTART_SCAN);
		returnSingleEntry		= BooleanFlagOn(irpSp->Flags, SL_RETURN_SINGLE_ENTRY);

		RtlZeroMemory( &ioStatusBlock, sizeof(ioStatusBlock) );

		topLevelIrp = IoGetTopLevelIrp();
		ASSERT( topLevelIrp == NULL || topLevelIrp == (PIRP)FSRTL_FSP_TOP_LEVEL_IRP );

		readonlyRedirectRequest.Tag					= READONLY_REDIRECT_REQUEST_TAG;
#if DBG
		readonlyRedirectRequest.DebugTag			= READONLY_REDIRECT_REQUEST_TAG;
#endif
		readonlyRedirectRequest.DevExt				= DevExt;
		readonlyRedirectRequest.OriginalTopLevelIrp = topLevelIrp;
		readonlyRedirectRequest.ReadonlyDismount	= 0;

		IoSetTopLevelIrp( (PIRP)&readonlyRedirectRequest );

		status = NtQueryQuotaInformationFile( ccb->ReadonlyFileHandle,
											  &ioStatusBlock,
											  outputBuffer,
											  outputBufferLength,
											  returnSingleEntry,
											  inputBuffer,
											  inputBufferLength,
											  ((ULONG)((PCHAR)queryQuota.StartSid - (PCHAR)queryQuota.SidList)) ? queryQuota.StartSid : NULL,
										      restartScan );


		ASSERT( readonlyRedirectRequest.DebugTag == (READONLY_REDIRECT_REQUEST_TAG - 4) );
		IoSetTopLevelIrp( topLevelIrp );

		if (NT_SUCCESS(status)) {

			ASSERT( status == STATUS_SUCCESS );
		}

		if (status == STATUS_BUFFER_OVERFLOW) {

			ASSERT( queryQuota.Length == ioStatusBlock.Information );
		}

		SPY_LOG_PRINT( (NT_SUCCESS(status) || status == STATUS_INVALID_PARAMETER) ? LFS_DEBUG_READONLY_NOISE : LFS_DEBUG_READONLY_TRACE,
				       ("ReadonlyRedirectIrpMajorQueryQuota: %wZ, fileHandle = %p "
					    "status = %x, length = %d, returnLength = %d\n",
						 &fileObject->FileName, ccb->ReadonlyFileHandle, status, queryQuota.Length, ioStatusBlock.Information) );

		if (!(status == STATUS_SUCCESS || status == STATUS_BUFFER_OVERFLOW)) {

			ASSERT( ioStatusBlock.Information == 0 );
			ioStatusBlock.Information = 0;
		} 

		if (status == STATUS_BUFFER_OVERFLOW) {

			SPY_LOG_PRINT( LFS_DEBUG_READONLY_TRACE,
					       ("ReadonlyRedirectIrpMajorQueryQuota: fileHandle = %p "
						    "status = %x, length = %d, returnLength = %d\n",
							 ccb->ReadonlyFileHandle, status, queryQuota.Length, ioStatusBlock.Information) );
		}

		Irp->IoStatus.Status	  = status;
		Irp->IoStatus.Information = ioStatusBlock.Information;

	} finally {

		if (AbnormalTermination()) {

			//status = GetExceptionCode();

			//if (NT_SUCCESS(status)) {

			//	NDAS_BUGON( FALSE );
			status = STATUS_UNSUCCESSFUL;
			//}
		}

		if (!(status == STATUS_SUCCESS || status == STATUS_BUFFER_OVERFLOW)) {

			Irp->IoStatus.Status = status;
			Irp->IoStatus.Information = ioStatusBlock.Information;
		}

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );
	}

	return status;
}
