#include "LfsProc.h"

#if __NDAS_FS_MINI__


NTSTATUS 
MiniSecondaryPassThrough (
	IN  PSECONDARY				Secondary,
	IN OUT PFLT_CALLBACK_DATA	Data
	)
{
	NTSTATUS				status = FLT_PREOP_SUCCESS_NO_CALLBACK;
	//PIO_STACK_LOCATION	iopb = IoGetCurrentIrpStackLocation( Irp );
	PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
	PFILE_OBJECT			fileObject = iopb->TargetFileObject;

	BOOLEAN				fastMutexSet = FALSE;
	BOOLEAN				retry = FALSE;

	PrintData( LFS_DEBUG_SECONDARY_INFO, "MiniSecondaryPassThrough", Secondary->LfsDeviceExt, Data );

	ASSERT( KeGetCurrentIrql() < DISPATCH_LEVEL );

	Secondary_Reference( Secondary );
 
	if (fileObject && fileObject->Flags & FO_DIRECT_DEVICE_OPEN) {

		NDASFS_ASSERT( LFS_REQUIRED );
		DbgPrint( "Direct device open\n" );
	}

	if (iopb->MajorFunction == IRP_MJ_CREATE					||	// 0x00
		iopb->MajorFunction == IRP_MJ_CLOSE					||	// 0x01
		iopb->MajorFunction == IRP_MJ_READ						||  // 0x03
		iopb->MajorFunction == IRP_MJ_WRITE					||  // 0x04
		iopb->MajorFunction == IRP_MJ_QUERY_INFORMATION		||  // 0x05
		iopb->MajorFunction == IRP_MJ_SET_INFORMATION			||	// 0x06
		iopb->MajorFunction == IRP_MJ_QUERY_EA					||	// 0x07
		iopb->MajorFunction == IRP_MJ_SET_EA					||	// 0x08
		iopb->MajorFunction == IRP_MJ_FLUSH_BUFFERS			||	// 0x09
		iopb->MajorFunction == IRP_MJ_QUERY_VOLUME_INFORMATION	||	// 0x0a
		iopb->MajorFunction == IRP_MJ_SET_VOLUME_INFORMATION	||	// 0x0b
		iopb->MajorFunction == IRP_MJ_DIRECTORY_CONTROL		||	// 0x0c
		iopb->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL		||	// 0x0c
		iopb->MajorFunction == IRP_MJ_LOCK_CONTROL				||	// 0x11
		iopb->MajorFunction == IRP_MJ_CLEANUP					||	// 0x12
		iopb->MajorFunction == IRP_MJ_QUERY_SECURITY			||	// 0x14
		iopb->MajorFunction == IRP_MJ_SET_SECURITY				||	// 0x15
		iopb->MajorFunction == IRP_MJ_QUERY_QUOTA				||	// 0x19
		iopb->MajorFunction == IRP_MJ_SET_QUOTA) {					// 0x1a
			
	} else if (iopb->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) {

		if (iopb->MinorFunction == IRP_MN_MOUNT_VOLUME		||
			iopb->MinorFunction == IRP_MN_VERIFY_VOLUME	||
			iopb->MinorFunction == IRP_MN_LOAD_FILE_SYSTEM) {
		
			NDASFS_ASSERT( LFS_UNEXPECTED );
			ASSERT( Secondary_LookUpCcb(Secondary, fileObject) == NULL );

			PrintData( LFS_DEBUG_SECONDARY_TRACE, "Secondary_PassThrough", Secondary->LfsDeviceExt, Data );

			Secondary_Dereference( Secondary );
			return FLT_PREOP_SUCCESS_NO_CALLBACK;
		}
	
	} else if (iopb->MajorFunction == IRP_MJ_INTERNAL_DEVICE_CONTROL	|| 	// 0x0f
			   iopb->MajorFunction == IRP_MJ_SHUTDOWN					||  // 0x10
			   iopb->MajorFunction == IRP_MJ_CREATE_MAILSLOT			||  // 0x13
			   iopb->MajorFunction == IRP_MJ_POWER						||  // 0x16
			   iopb->MajorFunction == IRP_MJ_SYSTEM_CONTROL			||  // 0x17
			   iopb->MajorFunction == IRP_MJ_DEVICE_CHANGE) {				// 0x18
	
		NDASFS_ASSERT( LFS_REQUIRED );
		PrintData( LFS_DEBUG_SECONDARY_TRACE, "Secondary_PassThrough", Secondary->LfsDeviceExt, Data );

		Secondary_Dereference( Secondary );

		return FLT_PREOP_SUCCESS_NO_CALLBACK;

	} else if (iopb->MajorFunction == IRP_MJ_DEVICE_CONTROL) {

		PLFS_CCB	ccb = NULL;

		ccb = Secondary_LookUpCcb(Secondary, fileObject);
		
		if (ccb == NULL || ccb->TypeOfOpen != UserVolumeOpen) {

			Secondary_Dereference( Secondary );

			Data->IoStatus.Status = STATUS_INVALID_PARAMETER;
			Data->IoStatus.Information = 0;

			return FLT_PREOP_COMPLETE;
		}

		status = MiniSecondary_Ioctl( Secondary, Data );

		Secondary_Dereference( Secondary );

		return status;	

	} else {

		NDASFS_ASSERT( LFS_UNEXPECTED );

		Secondary_Dereference( Secondary );
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}		

	PrintData( LFS_DEBUG_SECONDARY_TRACE, "Secondary_PassThrough", Secondary->LfsDeviceExt, Data );
	
	if (Secondary->Thread.SessionStatus == STATUS_DISK_CORRUPT_ERROR) {

		NDASFS_ASSERT( Secondary_LookUpCcb(Secondary, fileObject) == NULL );

		if (iopb->MajorFunction == IRP_MJ_CLOSE) {

			if (Secondary_LookUpCcb(Secondary, fileObject)) {

				SecondaryFileObjectClose( Secondary, fileObject );
			}

			Data->IoStatus.Status = STATUS_SUCCESS;
			Data->IoStatus.Information = 0;

		} else if (iopb->MajorFunction == IRP_MJ_CLEANUP) {

			InterlockedDecrement( &((PLFS_FCB)iopb->TargetFileObject->FsContext)->UncleanCount );
			SetFlag( fileObject->Flags, FO_CLEANUP_COMPLETE );

			Data->IoStatus.Status = STATUS_SUCCESS;
			Data->IoStatus.Information = 0;

		} else {

			Data->IoStatus.Status = STATUS_DISK_CORRUPT_ERROR;
			Data->IoStatus.Information = 0;
		}

		Secondary_Dereference( Secondary );
		
		return FLT_PREOP_COMPLETE;
	} 
	
	if (Secondary->Thread.SessionStatus == STATUS_UNRECOGNIZED_VOLUME) {

		NDASFS_ASSERT( Secondary_LookUpCcb(Secondary, fileObject) == NULL );

		if (iopb->MajorFunction == IRP_MJ_CLOSE) {

			if (Secondary_LookUpCcb(Secondary, fileObject)) {

				SecondaryFileObjectClose( Secondary, fileObject );
			}

			Data->IoStatus.Status = STATUS_SUCCESS;
			Data->IoStatus.Information = 0;

		} else if (iopb->MajorFunction == IRP_MJ_CLEANUP) {

			InterlockedDecrement( &((PLFS_FCB)iopb->TargetFileObject->FsContext)->UncleanCount );
			SetFlag( fileObject->Flags, FO_CLEANUP_COMPLETE );

			Data->IoStatus.Status = STATUS_SUCCESS;
			Data->IoStatus.Information = 0;

		} else {

			Data->IoStatus.Status = STATUS_DISK_CORRUPT_ERROR;
			Data->IoStatus.Information = 0;
		}

		Secondary_Dereference( Secondary );

		return FLT_PREOP_COMPLETE;
	}

	while (1) {

		NDASFS_ASSERT( fastMutexSet == FALSE );
		NDASFS_ASSERT( retry == FALSE );

		ExAcquireFastMutex( &Secondary->FastMutex );

		if (FlagOn(Secondary->Flags, SECONDARY_FLAG_CLOSED)) {

			SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR, 
						   ("Secondary is already closed Secondary = %p\n", Secondary) );

			ExReleaseFastMutex( &Secondary->FastMutex );

			NDASFS_ASSERT( iopb->MajorFunction == IRP_MJ_CREATE );

			if (iopb->MajorFunction == IRP_MJ_CLOSE) {

				if (Secondary_LookUpCcb(Secondary, fileObject)) {

					SecondaryFileObjectClose( Secondary, fileObject );
				}

				Data->IoStatus.Status = STATUS_SUCCESS;
				Data->IoStatus.Information = 0;

			} else if (iopb->MajorFunction == IRP_MJ_CLEANUP) {

				InterlockedDecrement( &((PLFS_FCB)iopb->TargetFileObject->FsContext)->UncleanCount );
				SetFlag( fileObject->Flags, FO_CLEANUP_COMPLETE );

				Data->IoStatus.Status = STATUS_SUCCESS;
				Data->IoStatus.Information = 0;

			} else {

				Data->IoStatus.Status = STATUS_TOO_LATE;
				Data->IoStatus.Information = 0;
			}

			status = FLT_PREOP_COMPLETE;
			break;
		}

		if (FlagOn(Secondary->Flags, SECONDARY_FLAG_ERROR)) {

			ExReleaseFastMutex( &Secondary->FastMutex );

			if (iopb->MajorFunction == IRP_MJ_CLOSE) {

				if (Secondary_LookUpCcb(Secondary, fileObject)) {

					SecondaryFileObjectClose( Secondary, fileObject );
				}

				Data->IoStatus.Status = STATUS_SUCCESS;
				Data->IoStatus.Information = 0;

			} else if (iopb->MajorFunction == IRP_MJ_CREATE) {

				Data->IoStatus.Status = STATUS_IO_DEVICE_ERROR;
				Data->IoStatus.Information = 0;
			
			} else if (iopb->MajorFunction == IRP_MJ_CLEANUP) {

				InterlockedDecrement( &((PLFS_FCB)iopb->TargetFileObject->FsContext)->UncleanCount );
				SetFlag( fileObject->Flags, FO_CLEANUP_COMPLETE );

				Data->IoStatus.Status = STATUS_SUCCESS;
				Data->IoStatus.Information = 0;

			} else {

				Data->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
				Data->IoStatus.Information = 0;
			} 

			status = FLT_PREOP_COMPLETE;
			break;
		}
		
		ExReleaseFastMutex( &Secondary->FastMutex );

		if (!FlagOn(Secondary->Flags, SECONDARY_FLAG_RECONNECTING)) {

			NDASFS_ASSERT( Secondary->ThreadObject );
		}

		KeEnterCriticalRegion();
		status = MiniRedirectIrp( Secondary, Data, &fastMutexSet, &retry );
		KeLeaveCriticalRegion();

		if (retry == TRUE) {

			retry = FALSE;
			continue;
		}

		if (status == STATUS_SUCCESS) {

			NDASFS_ASSERT( fastMutexSet == FALSE );

			if (Data->IoStatus.Status == STATUS_PENDING) {

				NDASFS_ASSERT( LFS_BUG );
			}

			status = FLT_PREOP_COMPLETE;
			break;	
		} 
		
		if (status == STATUS_DEVICE_REQUIRES_CLEANING) {

			PrintData( LFS_DEBUG_LFS_INFO, 
					  "STATUS_DEVICE_REQUIRES_CLEANING", 
					  CONTAINING_RECORD(Secondary, LFS_DEVICE_EXTENSION, Secondary), 
					  Data );

			if (iopb->MajorFunction == IRP_MJ_CLEANUP) {

				InterlockedDecrement( &((PLFS_FCB)iopb->TargetFileObject->FsContext)->UncleanCount );
				SetFlag( fileObject->Flags, FO_CLEANUP_COMPLETE );

				Data->IoStatus.Status = STATUS_SUCCESS;
				Data->IoStatus.Information = 0;

			} else {
			
				Data->IoStatus.Status = STATUS_ACCESS_DENIED;
				Data->IoStatus.Information = 0;
			}

			status = FLT_PREOP_COMPLETE;
			break;
		} 
		
		if (status == STATUS_ALERTED || status == STATUS_USER_APC) {

			NDASFS_ASSERT( fastMutexSet == FALSE );
			continue;		
		} 

		NDASFS_ASSERT( status == STATUS_REMOTE_DISCONNECT || status == STATUS_IO_DEVICE_ERROR );
		NDASFS_ASSERT( fastMutexSet == TRUE );

		ExAcquireFastMutex( &Secondary->FastMutex );

		ASSERT( FlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) );

		if (Secondary->LfsDeviceExt->SecondaryState == CONNECTED_TO_LOCAL_STATE		|| 
			FlagOn(Secondary->LfsDeviceExt->Flags, LFS_DEVICE_FLAG_SURPRISE_REMOVED)		||
			GlobalLfs.ShutdownOccured == TRUE) {

			SetFlag( Secondary->Flags, SECONDARY_FLAG_ERROR );
			ExReleaseFastMutex( &Secondary->FastMutex );

			KeReleaseSemaphore( &Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE );
			fastMutexSet = FALSE;

			Data->IoStatus.Status = STATUS_IO_DEVICE_ERROR;
			Data->IoStatus.Information = 0;
	
			status = FLT_PREOP_COMPLETE;
			break;
		}

		if (FlagOn(Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED)) {

			//BOOLEAN			recoveryResult;

			ASSERT( !FlagOn(Secondary->Flags, SECONDARY_FLAG_RECONNECTING) );
			SetFlag( Secondary->Flags, SECONDARY_FLAG_RECONNECTING );

			ExReleaseFastMutex( &Secondary->FastMutex );

			if (FlagOn(Secondary->Flags, SECONDARY_FLAG_CLOSED)) {

				ASSERT( iopb->MajorFunction == IRP_MJ_CREATE );
					
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR, 
							   ("Secondary is already closed Secondary = %p\n", Secondary) );

				ClearFlag( Secondary->Flags, SECONDARY_FLAG_RECONNECTING );

				KeReleaseSemaphore( &Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE );

				continue;
			}

			CallRecoverySessionAsynchronously( Secondary );
			fastMutexSet = FALSE;
				
			continue;
		}

		//
		//	Recovery failed.
		//

		SetFlag( Secondary->Flags, SECONDARY_FLAG_ERROR );
		ExReleaseFastMutex( &Secondary->FastMutex );

		KeReleaseSemaphore( &Secondary->Semaphore, IO_NO_INCREMENT, 1, FALSE );
		fastMutexSet = FALSE;

		NDASFS_ASSERT( IsListEmpty(&Secondary->RequestQueue) );

		continue;
	}

	NDASFS_ASSERT( fastMutexSet == FALSE );

	Secondary_Dereference( Secondary );

	NDASFS_ASSERT( status == FLT_PREOP_COMPLETE );

	return status;
}


#include <mountdev.h>

#ifndef IOCTL_VOLUME_SET_GPT_ATTRIBUTES
#define IOCTL_VOLUME_SET_GPT_ATTRIBUTES CTL_CODE(IOCTL_VOLUME_BASE, 13, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif

#define IOCTL_VOLSNAP_FLUSH_AND_HOLD_WRITES         CTL_CODE(VOLSNAPCONTROLTYPE, 0, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS) // ntifs
#define IOCTL_VOLSNAP_RELEASE_WRITES                CTL_CODE(VOLSNAPCONTROLTYPE, 1, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_VOLSNAP_PREPARE_FOR_SNAPSHOT          CTL_CODE(VOLSNAPCONTROLTYPE, 2, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_VOLSNAP_ABORT_PREPARED_SNAPSHOT       CTL_CODE(VOLSNAPCONTROLTYPE, 3, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_VOLSNAP_COMMIT_SNAPSHOT               CTL_CODE(VOLSNAPCONTROLTYPE, 4, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_VOLSNAP_END_COMMIT_SNAPSHOT           CTL_CODE(VOLSNAPCONTROLTYPE, 5, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_VOLSNAP_QUERY_NAMES_OF_SNAPSHOTS      CTL_CODE(VOLSNAPCONTROLTYPE, 6, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_VOLSNAP_CLEAR_DIFF_AREA               CTL_CODE(VOLSNAPCONTROLTYPE, 7, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_VOLSNAP_ADD_VOLUME_TO_DIFF_AREA       CTL_CODE(VOLSNAPCONTROLTYPE, 8, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_VOLSNAP_QUERY_DIFF_AREA               CTL_CODE(VOLSNAPCONTROLTYPE, 9, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_VOLSNAP_SET_MAX_DIFF_AREA_SIZE        CTL_CODE(VOLSNAPCONTROLTYPE, 10, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_VOLSNAP_QUERY_DIFF_AREA_SIZES         CTL_CODE(VOLSNAPCONTROLTYPE, 11, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_VOLSNAP_DELETE_OLDEST_SNAPSHOT        CTL_CODE(VOLSNAPCONTROLTYPE, 12, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_VOLSNAP_AUTO_CLEANUP                  CTL_CODE(VOLSNAPCONTROLTYPE, 13, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_VOLSNAP_DELETE_SNAPSHOT               CTL_CODE(VOLSNAPCONTROLTYPE, 14, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)


BOOLEAN
MiniSecondary_Ioctl (
	IN  PSECONDARY				Secondary,
	IN OUT PFLT_CALLBACK_DATA	Data
	)
{
	NTSTATUS			status;
	//PIO_STACK_LOCATION	iopb = IoGetCurrentIrpStackLocation( Irp );
	PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
    //KEVENT				waitEvent;
	ULONG				ioctlCode = iopb->Parameters.DeviceIoControl.Common.IoControlCode;
	ULONG				devType = DEVICE_TYPE_FROM_CTL_CODE(ioctlCode);
	UCHAR				function = (UCHAR)((ioctlCode & 0x00003FFC) >> 2);
	PVOID				inputBuffer, outputBuffer;
	IO_STATUS_BLOCK		ioStatusBlock;


	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_TRACE, 
				   ("Secondary_Ioctl: IRP_MJ_DEVICE_CONTROL Entered, deviceType = %d, function = %d IOCTL_VOLUME_BASE = %d, MOUNTDEVCONTROLTYPE = %d\n",
					devType, function, IOCTL_VOLUME_BASE, MOUNTDEVCONTROLTYPE) );

	if (IS_WINDOWS2K() && Secondary->LfsDeviceExt->DiskDeviceObject == NULL) {

		NDASFS_ASSERT( LFS_BUG );
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	if (IS_WINDOWSXP_OR_LATER() && Secondary->LfsDeviceExt->MountVolumeDeviceObject == NULL) {

		NDASFS_ASSERT( LFS_BUG );
		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	if (ioctlCode == IOCTL_VOLUME_SET_GPT_ATTRIBUTES) {

		Data->IoStatus.Status = STATUS_NOT_SUPPORTED;
		Data->IoStatus.Information = 0;

		return FLT_PREOP_COMPLETE;
	}

	SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE,
				   ("Open files from secondary: LfsDeviceExt = %p, iopb->MajorFunction = %x, iopb->MinorFunction = %x\n", 
					 Secondary->LfsDeviceExt, iopb->MajorFunction, iopb->MinorFunction) );

	inputBuffer = MinispyMapInputBuffer( iopb, Data->RequestorMode );
	outputBuffer = MinispyMapOutputBuffer( iopb, Data->RequestorMode );

	status = LfsFilterDeviceIoControl( IS_WINDOWS2K() ? Secondary->LfsDeviceExt->DiskDeviceObject : Secondary->LfsDeviceExt->MountVolumeDeviceObject,
									   ioctlCode,
									   inputBuffer,
									   iopb->Parameters.DeviceIoControl.Common.InputBufferLength,
									   outputBuffer,
									   iopb->Parameters.DeviceIoControl.Common.OutputBufferLength,
									   &ioStatusBlock.Information );


	SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_ERROR, ("%s: status = %x\n", __FUNCTION__, status) );

	Data->IoStatus.Status = status;
	Data->IoStatus.Information = ioStatusBlock.Information;

	return FLT_PREOP_COMPLETE;
}


#endif
