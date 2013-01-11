#define	__READONLY__
#include "LfsProc.h"

#define MAX_NTFS_METADATA_FILE 11
UNICODE_STRING NtfsMetadataFileNames[] = {
		{ 10, 10, L"\\$Mft" },
		{ 18, 18, L"\\$MftMirr" },
		{ 18, 18, L"\\$LogFile" },
		{ 16, 16, L"\\$Volume" },
		{ 18, 18, L"\\$AttrDef" },
		{ 12, 12, L"\\$Root" },
		{ 16, 16, L"\\$Bitmap" },
		{ 12, 12, L"\\$Boot" },
		{ 18, 18, L"\\$BadClus" },
		{ 16, 16, L"\\$Secure" },
		{ 16, 16, L"\\$UpCase" },
		{ 16, 16, L"\\$Extend" }
	};

BOOLEAN	IsMetaFile(PUNICODE_STRING	fileName) {

	LONG	idx_metafile;

	for (idx_metafile = 0; idx_metafile < MAX_NTFS_METADATA_FILE ; idx_metafile ++) {
		
		if (RtlCompareUnicodeString( NtfsMetadataFileNames + idx_metafile,fileName, TRUE) == 0) {
			
			return TRUE;
		}
	}

	return FALSE;
}

BOOLEAN
ReadOnlyPassThrough(
    IN PDEVICE_OBJECT				DeviceObject,
    IN PIRP							Irp,
    IN PIO_STACK_LOCATION			IrpSp,
	IN PLFS_DEVICE_EXTENSION		LfsDevExt,
	OUT	PNTSTATUS					NtStatus
   )
{
	BOOLEAN				ProcessDone;
	ULONG				options;
	CHAR				disposition;
	PFILE_OBJECT		fileObject;

	SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, ("ReadOnlyPassThrough FileSystemType = %x\n", 
										  LfsDevExt->FileSystemType) );

	SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, ("Is windows2k = %x\n", IS_WINDOWS2K()) );

	SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, ("Major = %d, Minor = %d\n", gSpyOsMajorVersion, gSpyOsMinorVersion) );

	if (((PFILESPY_DEVICE_EXTENSION)(DeviceObject->DeviceExtension))->DiskDeviceObject == NULL) // It's a Control Device 
		return FALSE;

	//
	//	skip Meta Files
	//

	fileObject = IrpSp->FileObject;

#if DBG
	if (fileObject) {
		SPY_LOG_PRINT( LFS_DEBUG_LIB_INFO,
					   ("ReadOnlyPassThrough: IrpFlag:%08lx fileObject->FileName = %wZ\n", Irp->Flags, &fileObject->FileName) );
	}
#endif

    if (IrpSp->MajorFunction != IRP_MJ_CREATE) {

		if ((fileObject && IsMetaFile(&fileObject->FileName)) ||
			(Irp->Flags & IRP_SYNCHRONOUS_PAGING_IO)) {		// file system meta file through CCM

			SPY_LOG_PRINT( LFS_DEBUG_LIB_INFO,
						   ("ReadOnlyPassThrough: system volume file fileObject->FileName = %wZ\n", &fileObject->FileName) );
			return FALSE;
		}
	}

	ProcessDone = FALSE;

	switch (IrpSp->MajorFunction) {

		case IRP_MJ_CREATE:
			//
			//	if it contains creation, stop it
			//	(Options >> 24) & 0xFF
			options = IrpSp->Parameters.Create.Options & 0xFFFFFF;
			disposition = (CHAR)(((IrpSp->Parameters.Create.Options) >> 24) & 0xFF);
			
			if ((options & FILE_DELETE_ON_CLOSE) || 
				 disposition == FILE_CREATE ||
				 disposition == FILE_SUPERSEDE ||
				 disposition == FILE_OVERWRITE ||
				 disposition == FILE_OVERWRITE_IF) {

				*NtStatus = STATUS_MEDIA_WRITE_PROTECTED;
				Irp->IoStatus.Status = STATUS_MEDIA_WRITE_PROTECTED;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest( Irp, IO_DISK_INCREMENT );

				ProcessDone = TRUE;

				break;
			}

			//
			//	check the purge flag.
			//
			if (!LfsDevExt->Purge)
				break ;

			//
			//	purge cache
			//
			switch (LfsDevExt->FileSystemType) {

			case LFS_FILE_SYSTEM_NTFS:

#ifndef _WIN64
				if(IS_WINDOWS2K()) {

					BOOLEAN flushResult;
		
	
					flushResult = W2kNtfsFlushMetaFile( DeviceObject,
														LfsDevExt->BaseVolumeDeviceObject,
														LfsDevExt->BufferLength,
														LfsDevExt->Buffer );

					SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, ("ReadOnlyPassThrough flushResult = %d\n", flushResult) );

				} else if(IS_WINDOWSXP()) {

					BOOLEAN flushResult;

					flushResult = WxpNtfsFlushMetaFile( DeviceObject,
														LfsDevExt->BaseVolumeDeviceObject,
														LfsDevExt->BufferLength,
														LfsDevExt->Buffer );

					SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, ("ReadOnlyPassThrough flushResult = %d\n", flushResult) );
				}
#endif
			break;

		default:

			break;
		}

		case IRP_MJ_WRITE:
		case IRP_MJ_SET_INFORMATION:
        case IRP_MJ_SET_EA:
        case IRP_MJ_SET_VOLUME_INFORMATION:
        case IRP_MJ_SET_SECURITY:
		case IRP_MJ_SET_QUOTA:

			*NtStatus = STATUS_MEDIA_WRITE_PROTECTED;
			Irp->IoStatus.Status = STATUS_MEDIA_WRITE_PROTECTED;
			Irp->IoStatus.Information = 0;
			IoCompleteRequest(Irp, IO_DISK_INCREMENT);

			ProcessDone = TRUE ;

			break ;

        case IRP_MJ_FILE_SYSTEM_CONTROL:

			if (IrpSp->Parameters.DeviceIoControl.IoControlCode == FSCTL_MOVE_FILE) {

				*NtStatus = STATUS_MEDIA_WRITE_PROTECTED;
				Irp->IoStatus.Status = STATUS_MEDIA_WRITE_PROTECTED;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest(Irp, IO_DISK_INCREMENT);

				ProcessDone = TRUE;
			}

			break ;

		case IRP_MJ_DIRECTORY_CONTROL:
			//
			//	purge only on Query Directory 
			//
			if(IrpSp->MinorFunction != IRP_MN_QUERY_DIRECTORY)
				break ;

			//
			//	check the purge flag.
			//
			if(!LfsDevExt->Purge)
				break ;

			switch(LfsDevExt->FileSystemType)
			{
			case LFS_FILE_SYSTEM_NTFS:
#ifndef _WIN64
				if(IS_WINDOWS2K())
				{
					W2kNtfsFlushOnDirectoryControl(
								DeviceObject,
								LfsDevExt->BaseVolumeDeviceObject,
								IrpSp,
								LfsDevExt->BufferLength,
								LfsDevExt->Buffer
							);

				} else if(IS_WINDOWSXP())
				{
					WxpNtfsFlushOnDirectoryControl(
								DeviceObject,
								LfsDevExt->BaseVolumeDeviceObject,
								IrpSp,
								LfsDevExt->BufferLength,
								LfsDevExt->Buffer
							);
					SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
								("ReadOnlyPassThrough Flush triggered by DirectoryControl\n"));
				}
#endif

				break;

			default:
				break ;
			}
		break ;

	default:
		;
	}

	return ProcessDone;
}
