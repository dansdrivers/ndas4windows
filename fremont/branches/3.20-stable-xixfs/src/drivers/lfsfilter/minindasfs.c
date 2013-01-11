#include "LfsProc.h"

#if __NDAS_FS_MINI__

#define PrintData( DebugLevel, Where, LfsDeviceExt, Irp )

NTSTATUS
NdasFsGeneralPreOperationCallback (
	IN PCFLT_RELATED_OBJECTS	FltObjects,
	IN PCTX_INSTANCE_CONTEXT	InstanceContext,
	IN OUT PFLT_CALLBACK_DATA	Data
	)
{
	NTSTATUS				status = FLT_PREOP_SUCCESS_NO_CALLBACK;
	//PIO_STACK_LOCATION	iopb = IoGetCurrentIrpStackLocation( Irp );
	PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
	PFILE_OBJECT			fileObject = iopb->TargetFileObject;
	BOOLEAN					fastMutexAcquired = FALSE;


	//UNREFERENCED_PARAMETER( DeviceObject );
	UNREFERENCED_PARAMETER( FltObjects );

	ASSERT( InstanceContext->LfsDeviceExt.ReferenceCount );
	LfsDeviceExt_Reference( &InstanceContext->LfsDeviceExt );	

	PrintData( LFS_DEBUG_PRIMARY_NOISE, __FUNCTION__, &InstanceContext->LfsDeviceExt, Irp );

	ASSERT( KeGetCurrentIrql() <= APC_LEVEL );

	ExAcquireFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
	fastMutexAcquired = TRUE;

	try {

		if (!FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTED)) {

			status = FLT_PREOP_SUCCESS_NO_CALLBACK;
			leave;
		}

		ASSERT( InstanceContext->LfsDeviceExt.AttachedToDeviceObject == InstanceContext->DeviceObject );

		if (InstanceContext->LfsDeviceExt.AttachedToDeviceObject == NULL) {

			NDASFS_ASSERT( FALSE );

			status = FLT_PREOP_SUCCESS_NO_CALLBACK;
			leave;
		}

		if (!(FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTED) && !FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED))) {

			if (InstanceContext->LfsDeviceExt.Secondary != NULL && 
				Secondary_LookUpFileExtension(InstanceContext->LfsDeviceExt.Secondary, fileObject) != NULL) {
				
				NDASFS_ASSERT( FALSE );

				Data->IoStatus.Status = STATUS_FILE_CORRUPT_ERROR;
				Data->IoStatus.Information = 0;

				status = FLT_PREOP_COMPLETE;
				leave;
			}

			status = FLT_PREOP_SUCCESS_NO_CALLBACK;
			leave;
		}

#if __NDAS_FS_FLUSH_VOLUME__

		if (InstanceContext->LfsDeviceExt.FileSystemType == LFS_FILE_SYSTEM_NTFS) {

			switch (iopb->MajorFunction) {

			case IRP_MJ_CREATE: {

				ULONG	options;
				CHAR	disposition;

				options = iopb->Parameters.Create.Options & 0xFFFFFF;
				disposition = (CHAR)(((iopb->Parameters.Create.Options) >> 24) & 0xFF);

				if (options & FILE_DELETE_ON_CLOSE	||
					disposition == FILE_CREATE		||
					disposition == FILE_SUPERSEDE	||
					disposition == FILE_OVERWRITE	||
					disposition == FILE_OVERWRITE_IF) {

					InstanceContext->LfsDeviceExt.ReceiveWriteCommand = TRUE;
					KeQuerySystemTime( &lfsDeviceExt->CommandReceiveTime );

					SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, 
								   ("lfsDeviceExt->CommandReceiveTime.QuadPart = %I64d\n", InstanceContext->LfsDeviceExt.CommandReceiveTime.QuadPart) );

			
					break;
				}

				break;
			}

			case IRP_MJ_WRITE:
			case IRP_MJ_SET_INFORMATION:
			case IRP_MJ_SET_EA:
	        case IRP_MJ_SET_VOLUME_INFORMATION:
		    case IRP_MJ_SET_SECURITY:
			case IRP_MJ_SET_QUOTA:

				InstanceContext->LfsDeviceExt.ReceiveWriteCommand = TRUE;
				KeQuerySystemTime( &lfsDeviceExt->CommandReceiveTime );

				SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, 
							   ("lfsDeviceExt->CommandReceiveTime.QuadPart = %I64d\n", InstanceContext->LfsDeviceExt.CommandReceiveTime.QuadPart) );

				break;

			case IRP_MJ_FILE_SYSTEM_CONTROL:

				if (iopb->Parameters.DeviceIoControl.IoControlCode == FSCTL_MOVE_FILE) {

					InstanceContext->LfsDeviceExt.ReceiveWriteCommand = TRUE;
					KeQuerySystemTime( &lfsDeviceExt->CommandReceiveTime );

					SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, 
								   ("lfsDeviceExt->CommandReceiveTime.QuadPart = %I64d\n", InstanceContext->LfsDeviceExt.CommandReceiveTime.QuadPart) );
				}

				break;

			default:

				break;
			}
		}

#endif
		if (iopb->MajorFunction == IRP_MJ_CREATE)
			PrintData( LFS_DEBUG_PRIMARY_NOISE, __FUNCTION__, &InstanceContext->LfsDeviceExt, Irp );

		if (iopb->MajorFunction == IRP_MJ_PNP) {

			if (iopb->MinorFunction == IRP_MN_SURPRISE_REMOVAL) {

				if (InstanceContext->LfsDeviceExt.NetdiskPartition == NULL) {

					ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
					fastMutexAcquired = FALSE;
					NDASFS_ASSERT( FALSE );
				
				} else {

					PNETDISK_PARTITION	netdiskPartition2;

					netdiskPartition2 = InstanceContext->LfsDeviceExt.NetdiskPartition;
					InstanceContext->LfsDeviceExt.NetdiskPartition = NULL;

					SetFlag( InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_SURPRISE_REMOVED );

					ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
					fastMutexAcquired = FALSE;

					NetdiskManager_PrimarySessionStopping( GlobalLfs.NetdiskManager,
														   netdiskPartition2,
														   InstanceContext->LfsDeviceExt.NetdiskEnabledMode	);

					NetdiskManager_PrimarySessionDisconnect( GlobalLfs.NetdiskManager,
														     netdiskPartition2,
														     InstanceContext->LfsDeviceExt.NetdiskEnabledMode );

					NetdiskManager_SurpriseRemoval( GlobalLfs.NetdiskManager,
													netdiskPartition2,
													InstanceContext->LfsDeviceExt.NetdiskEnabledMode );
				}

				status = FLT_PREOP_SUCCESS_NO_CALLBACK;
				leave;
			}

			// Need to test much whether this is okay..
	
			if (iopb->MinorFunction == IRP_MN_CANCEL_REMOVE_DEVICE || iopb->MinorFunction == IRP_MN_REMOVE_DEVICE) {

				status = FLT_PREOP_SUCCESS_NO_CALLBACK;
				leave;
			
			} else if (iopb->MinorFunction == IRP_MN_QUERY_REMOVE_DEVICE) {

				//KEVENT				waitEvent;

				if (InstanceContext->LfsDeviceExt.AttachedToDeviceObject == NULL) {

					NDASFS_ASSERT( FALSE );

					status = FLT_PREOP_SUCCESS_NO_CALLBACK;
					leave;
				}

				SetFlag( InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );

				ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
				fastMutexAcquired = FALSE;

				NetdiskManager_PrimarySessionStopping( GlobalLfs.NetdiskManager,
													   InstanceContext->LfsDeviceExt.NetdiskPartition,
													   InstanceContext->LfsDeviceExt.NetdiskEnabledMode	);

#if 0

				IoCopyCurrentIrpStackLocationToNext( Irp );
				KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

				IoSetCompletionRoutine( Irp,
										LfsGeneralPassThroughCompletion,
										&waitEvent,
										TRUE,
										TRUE,
										TRUE );

				status = IoCallDriver( InstanceContext->LfsDeviceExt.AttachedToDeviceObject, Irp );

				if (status == STATUS_PENDING) {

					status = KeWaitForSingleObject( &waitEvent, Executive, KernelMode, FALSE, NULL );
					ASSERT( status == STATUS_SUCCESS );
				}

				ASSERT( KeReadStateEvent(&waitEvent) || !NT_SUCCESS(Irp->IoStatus.Status) );
	
				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO, 
							   ("%s: lfsDeviceExt = %p, IRP_MN_QUERY_REMOVE_DEVICE Irp->IoStatus.Status = %x\n",
								__FUNCTION__, &InstanceContext->LfsDeviceExt, Irp->IoStatus.Status) );

				if (NT_SUCCESS(Irp->IoStatus.Status)) {

					NetdiskManager_PrimarySessionDisconnect( GlobalLfs.NetdiskManager,
															 InstanceContext->LfsDeviceExt.NetdiskPartition,
															 InstanceContext->LfsDeviceExt.NetdiskEnabledMode );

				} else {
					
					NetdiskManager_PrimarySessionCancelStopping( GlobalLfs.NetdiskManager,
																 InstanceContext->LfsDeviceExt.NetdiskPartition,
																 InstanceContext->LfsDeviceExt.NetdiskEnabledMode );
				}

				ClearFlag( InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );

				if (NT_SUCCESS(Irp->IoStatus.Status)) {

					ASSERT( !FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED) );
					LfsDismountVolume( &InstanceContext->LfsDeviceExt );
				}

				status = Irp->IoStatus.Status;
				IoCompleteRequest( Irp, IO_NO_INCREMENT );

				result = TRUE;
				leave;
#else
				status = FLT_PREOP_SUCCESS_WITH_CALLBACK;
				leave;
#endif
			}

			status = FLT_PREOP_SUCCESS_NO_CALLBACK;
			leave;
		
		} else if (iopb->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) {

			if (!FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_INDIRECT) &&
				(InstanceContext->LfsDeviceExt.FileSystemType == LFS_FILE_SYSTEM_NDAS_FAT ||
				 InstanceContext->LfsDeviceExt.FileSystemType == LFS_FILE_SYSTEM_NDAS_NTFS)) {

				if (!(iopb->MinorFunction == IRP_MN_USER_FS_REQUEST || iopb->MinorFunction == IRP_MN_KERNEL_CALL)) {

					status = FLT_PREOP_SUCCESS_NO_CALLBACK;
					leave;
				}
										
				//if (iopb->Parameters.FileSystemControl.FsControlCode != FSCTL_DISMOUNT_VOLUME) {

				//	result = FALSE;
				//	break;
				//}
			}
			
			PrintData( LFS_DEBUG_PRIMARY_NOISE, __FUNCTION__, &InstanceContext->LfsDeviceExt, Irp );

			if (iopb->MinorFunction == IRP_MN_USER_FS_REQUEST || iopb->MinorFunction == IRP_MN_KERNEL_CALL) {

				//	Do not allow exclusive access to the volume and dismount volume to protect format
				//	We allow exclusive access if secondaries are connected locally.
				//

				if (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_LOCK_VOLUME) {
						
					ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
					fastMutexAcquired = FALSE;

					if (NetdiskManager_ThisVolumeHasSecondary(GlobalLfs.NetdiskManager,
															  InstanceContext->LfsDeviceExt.NetdiskPartition,
															  InstanceContext->LfsDeviceExt.NetdiskEnabledMode,
															  FALSE) ) {

						SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
									   ("Primary_PassThrough, IRP_MJ_FILE_SYSTEM_CONTROL: Trying to acquire the volume exclusively. \n") );

						ASSERT( fileObject && fileObject->FileName.Length == 0 && fileObject->RelatedFileObject == NULL ); 

						Data->IoStatus.Status = STATUS_ACCESS_DENIED;
						Data->IoStatus.Information = 0;

						status = FLT_PREOP_COMPLETE;
						leave;
					}
					
				} else if (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_UNLOCK_VOLUME) {

					status = FLT_PREOP_SUCCESS_NO_CALLBACK;
					leave;
	
				} else if (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_DISMOUNT_VOLUME) {
						
					//KEVENT				waitEvent;

					ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
					fastMutexAcquired = FALSE;

					if (NetdiskManager_ThisVolumeHasSecondary( GlobalLfs.NetdiskManager,
															   InstanceContext->LfsDeviceExt.NetdiskPartition,
															   InstanceContext->LfsDeviceExt.NetdiskEnabledMode,
															   FALSE) ) {

						ASSERT ( FALSE );

						SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
									   ("Primary_PassThrough, IRP_MJ_FILE_SYSTEM_CONTROL: Trying to acquire the volume exclusively. \n") );

						ASSERT( fileObject && fileObject->FileName.Length == 0 && fileObject->RelatedFileObject == NULL ); 

						Data->IoStatus.Status = STATUS_ACCESS_DENIED;
						Data->IoStatus.Information = 0;

						status = FLT_PREOP_COMPLETE;
						leave;
					}
	
					ExAcquireFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
					fastMutexAcquired = TRUE;

					SetFlag( InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );

					ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
					fastMutexAcquired = FALSE;

					NetdiskManager_PrimarySessionStopping( GlobalLfs.NetdiskManager,
														   InstanceContext->LfsDeviceExt.NetdiskPartition,
														   InstanceContext->LfsDeviceExt.NetdiskEnabledMode	);

#if 0

					IoCopyCurrentIrpStackLocationToNext( Irp );
					KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

					IoSetCompletionRoutine( Irp,
											LfsGeneralPassThroughCompletion,
											&waitEvent,
											TRUE,
											TRUE,
											TRUE );

					status = IoCallDriver( InstanceContext->LfsDeviceExt.AttachedToDeviceObject, Irp );

					if (status == STATUS_PENDING) {

						status = KeWaitForSingleObject( &waitEvent, Executive, KernelMode, FALSE, NULL );
						ASSERT( status == STATUS_SUCCESS );
					}

					ASSERT( KeReadStateEvent(&waitEvent) || !NT_SUCCESS(Irp->IoStatus.Status) );

					if (NT_SUCCESS(Irp->IoStatus.Status)) {

						NetdiskManager_PrimarySessionDisconnect( GlobalLfs.NetdiskManager,
																 InstanceContext->LfsDeviceExt.NetdiskPartition,
																 InstanceContext->LfsDeviceExt.NetdiskEnabledMode );

					} else {
					
						NetdiskManager_PrimarySessionCancelStopping( GlobalLfs.NetdiskManager,
																	 InstanceContext->LfsDeviceExt.NetdiskPartition,
																	 InstanceContext->LfsDeviceExt.NetdiskEnabledMode );
					}

					ClearFlag( InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );
	
					if (NT_SUCCESS(Irp->IoStatus.Status)) {

						ASSERT( !FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED) );
						LfsDismountVolume( &InstanceContext->LfsDeviceExt );
					}

					status = Irp->IoStatus.Status;
					IoCompleteRequest( Irp, IO_NO_INCREMENT );

					SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, 
								   ("%s: FSCTL_DISMOUNT_VOLUME Irp->IoStatus.Status = %x\n",
									__FUNCTION__, Irp->IoStatus.Status) );

					result = TRUE;
					leave;

#else

					status = FLT_PREOP_SUCCESS_WITH_CALLBACK;
					leave;

#endif

				} else if (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_SET_ENCRYPTION) {

					//
					//	Do not support encryption.
					//

					SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
								   ("Primary_PassThrough, IRP_MJ_FILE_SYSTEM_CONTROL: Setting encryption denied.\n") );

					Data->IoStatus.Status = STATUS_NOT_SUPPORTED;
					Data->IoStatus.Information = 0;

					status = FLT_PREOP_COMPLETE;
					leave;
				}
			}
		}

	} finally {

		if (fastMutexAcquired)
			ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );

		LfsDeviceExt_Dereference( &InstanceContext->LfsDeviceExt );
	}

	return status;
}

NTSTATUS
NdasFsGeneralPostOperationCallback (
	IN PCFLT_RELATED_OBJECTS	FltObjects,
	IN PCTX_INSTANCE_CONTEXT	InstanceContext,
	IN OUT PFLT_CALLBACK_DATA	Data
	)
{
	NTSTATUS				status = FLT_POSTOP_FINISHED_PROCESSING;
	//PIO_STACK_LOCATION	iopb = IoGetCurrentIrpStackLocation( Irp );
	PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
	PFILE_OBJECT			fileObject = iopb->TargetFileObject;
	BOOLEAN					fastMutexAcquired = FALSE;


	//UNREFERENCED_PARAMETER( DeviceObject );
	UNREFERENCED_PARAMETER( FltObjects );

	ASSERT( InstanceContext->LfsDeviceExt.ReferenceCount );
	LfsDeviceExt_Reference( &InstanceContext->LfsDeviceExt );	

	PrintData( LFS_DEBUG_PRIMARY_NOISE, __FUNCTION__, &InstanceContext->LfsDeviceExt, Irp );

	ASSERT( KeGetCurrentIrql() <= APC_LEVEL );

	ExAcquireFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
	fastMutexAcquired = TRUE;

	try {

		if (!FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTED)) {

			NDASFS_ASSERT( FALSE );

			status = FLT_POSTOP_FINISHED_PROCESSING;
			leave;
		}

		if (FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED)) {

			NDASFS_ASSERT( FALSE );

			status = FLT_POSTOP_FINISHED_PROCESSING;
			leave;
		}

		ASSERT( InstanceContext->LfsDeviceExt.AttachedToDeviceObject == InstanceContext->DeviceObject );

		if (InstanceContext->LfsDeviceExt.AttachedToDeviceObject == NULL) {

			NDASFS_ASSERT( FALSE );

			status = FLT_POSTOP_FINISHED_PROCESSING;
			leave;
		}

		if (!(FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTED) && !FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED))) {

			NDASFS_ASSERT( FALSE );

			if (InstanceContext->LfsDeviceExt.Secondary != NULL && 
				Secondary_LookUpFileExtension(InstanceContext->LfsDeviceExt.Secondary, fileObject) != NULL) {
				
				status = FLT_POSTOP_FINISHED_PROCESSING;
				leave;
			}

			status = FLT_POSTOP_FINISHED_PROCESSING;
			leave;
		}

		if (iopb->MajorFunction == IRP_MJ_CREATE)
			PrintData( LFS_DEBUG_PRIMARY_NOISE, __FUNCTION__, &InstanceContext->LfsDeviceExt, Irp );

		if (iopb->MajorFunction == IRP_MJ_PNP) {

			if (iopb->MinorFunction == IRP_MN_SURPRISE_REMOVAL) {

				NDASFS_ASSERT( FALSE );

				status = FLT_POSTOP_FINISHED_PROCESSING;
				leave;
			}

			// Need to test much whether this is okay..
	
			if (iopb->MinorFunction == IRP_MN_CANCEL_REMOVE_DEVICE || iopb->MinorFunction == IRP_MN_REMOVE_DEVICE) {

				NDASFS_ASSERT( FALSE );

				status = FLT_POSTOP_FINISHED_PROCESSING;
				leave;
			
			} else if (iopb->MinorFunction == IRP_MN_QUERY_REMOVE_DEVICE) {

				if (InstanceContext->LfsDeviceExt.AttachedToDeviceObject == NULL) {

					NDASFS_ASSERT( FALSE );

					status = FLT_POSTOP_FINISHED_PROCESSING;
					leave;
				}

				NDASFS_ASSERT( FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING) );

				ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
				fastMutexAcquired = FALSE;
	
				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO, 
							   ("%s: lfsDeviceExt = %p, IRP_MN_QUERY_REMOVE_DEVICE Data->IoStatus.Status = %x\n",
								__FUNCTION__, &InstanceContext->LfsDeviceExt, Data->IoStatus.Status) );

				if (NT_SUCCESS(Data->IoStatus.Status)) {

					NetdiskManager_PrimarySessionDisconnect( GlobalLfs.NetdiskManager,
															 InstanceContext->LfsDeviceExt.NetdiskPartition,
															 InstanceContext->LfsDeviceExt.NetdiskEnabledMode );

				} else {
					
					NetdiskManager_PrimarySessionCancelStopping( GlobalLfs.NetdiskManager,
																 InstanceContext->LfsDeviceExt.NetdiskPartition,
																 InstanceContext->LfsDeviceExt.NetdiskEnabledMode );
				}

				ClearFlag( InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );

				if (NT_SUCCESS(Data->IoStatus.Status)) {

					NDASFS_ASSERT( !FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED) );
					LfsDismountVolume( &InstanceContext->LfsDeviceExt );
				}

				status = FLT_POSTOP_FINISHED_PROCESSING;
				leave;
			}

			NDASFS_ASSERT( FALSE );

			status = FLT_POSTOP_FINISHED_PROCESSING;
			leave;
		
		} else if (iopb->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) {

			if (!FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_INDIRECT) &&
				(InstanceContext->LfsDeviceExt.FileSystemType == LFS_FILE_SYSTEM_NDAS_FAT ||
				 InstanceContext->LfsDeviceExt.FileSystemType == LFS_FILE_SYSTEM_NDAS_NTFS)) {

				if (!(iopb->MinorFunction == IRP_MN_USER_FS_REQUEST || iopb->MinorFunction == IRP_MN_KERNEL_CALL)) {

					NDASFS_ASSERT( FALSE );

					status = FLT_POSTOP_FINISHED_PROCESSING;
					leave;
				}
			}
			
			PrintData( LFS_DEBUG_PRIMARY_NOISE, __FUNCTION__, &InstanceContext->LfsDeviceExt, Irp );

			if (iopb->MinorFunction == IRP_MN_USER_FS_REQUEST || iopb->MinorFunction == IRP_MN_KERNEL_CALL) {

				//	Do not allow exclusive access to the volume and dismount volume to protect format
				//	We allow exclusive access if secondaries are connected locally.
				//

				if (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_LOCK_VOLUME) {
						
					NDASFS_ASSERT( FALSE );

					status = FLT_POSTOP_FINISHED_PROCESSING;
					leave;
					
				} else if (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_UNLOCK_VOLUME) {

					NDASFS_ASSERT( FALSE );

					status = FLT_POSTOP_FINISHED_PROCESSING;
					leave;
	
				} else if (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_DISMOUNT_VOLUME) {
						
					ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
					fastMutexAcquired = FALSE;

					if (NetdiskManager_ThisVolumeHasSecondary( GlobalLfs.NetdiskManager,
															   InstanceContext->LfsDeviceExt.NetdiskPartition,
															   InstanceContext->LfsDeviceExt.NetdiskEnabledMode,
															   FALSE) ) {

						SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
									   ("Primary_PassThrough, IRP_MJ_FILE_SYSTEM_CONTROL: Trying to acquire the volume exclusively. \n") );

						ASSERT( fileObject && fileObject->FileName.Length == 0 && fileObject->RelatedFileObject == NULL ); 

						NDASFS_ASSERT( FALSE );

						status = FLT_POSTOP_FINISHED_PROCESSING;
						leave;
					}
	
					NDASFS_ASSERT( FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING) );

					if (NT_SUCCESS(Data->IoStatus.Status)) {

						NetdiskManager_PrimarySessionDisconnect( GlobalLfs.NetdiskManager,
																 InstanceContext->LfsDeviceExt.NetdiskPartition,
																 InstanceContext->LfsDeviceExt.NetdiskEnabledMode );

					} else {
					
						NetdiskManager_PrimarySessionCancelStopping( GlobalLfs.NetdiskManager,
																	 InstanceContext->LfsDeviceExt.NetdiskPartition,
																	 InstanceContext->LfsDeviceExt.NetdiskEnabledMode );
					}

					ClearFlag( InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );
	
					if (NT_SUCCESS(Data->IoStatus.Status)) {

						ASSERT( !FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED) );
						LfsDismountVolume( &InstanceContext->LfsDeviceExt );
					}

					SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, 
								   ("%s: FSCTL_DISMOUNT_VOLUME Data->IoStatus.Status = %x\n",
									__FUNCTION__, Data->IoStatus.Status) );

					status = FLT_POSTOP_FINISHED_PROCESSING;
					leave;

				} else if (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_SET_ENCRYPTION) {

					NDASFS_ASSERT( FALSE );

					status = FLT_POSTOP_FINISHED_PROCESSING;
					leave;
				}
			}
		}

	} finally {

		if (fastMutexAcquired)
			ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );

		LfsDeviceExt_Dereference( &InstanceContext->LfsDeviceExt );
	}

	return status;
}

NTSTATUS
NdasFsReadonlyPreOperationCallback (
	IN PCFLT_RELATED_OBJECTS	FltObjects,
	IN PCTX_INSTANCE_CONTEXT	InstanceContext,
	IN OUT PFLT_CALLBACK_DATA	Data
	)
{
	NTSTATUS				status = FLT_PREOP_SUCCESS_NO_CALLBACK;
	//PIO_STACK_LOCATION	iopb = IoGetCurrentIrpStackLocation( Irp );
	PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
	PFILE_OBJECT			fileObject = iopb->TargetFileObject;
	BOOLEAN					fastMutexAcquired = FALSE;


	//UNREFERENCED_PARAMETER( DeviceObject );
	UNREFERENCED_PARAMETER( FltObjects );

	ASSERT( InstanceContext->LfsDeviceExt.ReferenceCount );
	LfsDeviceExt_Reference( &InstanceContext->LfsDeviceExt );	

	PrintData( LFS_DEBUG_READONLY_NOISE, __FUNCTION__, &InstanceContext->LfsDeviceExt, Irp );

	ASSERT( KeGetCurrentIrql() <= APC_LEVEL );

	ExAcquireFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
	fastMutexAcquired = TRUE;

	try {

		if (!FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTED)) {

			status = FLT_PREOP_SUCCESS_NO_CALLBACK;
			leave;
		}

		ASSERT( InstanceContext->LfsDeviceExt.AttachedToDeviceObject == InstanceContext->DeviceObject );

		if (InstanceContext->LfsDeviceExt.AttachedToDeviceObject == NULL) {

			NDASFS_ASSERT( FALSE );

			status = FLT_PREOP_SUCCESS_NO_CALLBACK;
			leave;
		}

		if (!(FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTED) && !FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED))) {

			ASSERT( InstanceContext->LfsDeviceExt.Readonly == NULL || ReadonlyLookUpCcb(InstanceContext->LfsDeviceExt.Readonly, fileObject) == NULL );
			
			status = FLT_PREOP_SUCCESS_NO_CALLBACK;
			leave;
		}

		switch (iopb->MajorFunction) {

		case IRP_MJ_PNP:

			break;

		default: 

			ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
			fastMutexAcquired = FALSE;

			if (InstanceContext->LfsDeviceExt.Readonly) {

				//status = ReadonlyRedirectIrp( InstanceContext, Irp, &result );
			
			} else {

				status = FLT_PREOP_SUCCESS_NO_CALLBACK;
			}

			leave;
		}

		if (iopb->MajorFunction == IRP_MJ_PNP) {

			PrintData( LFS_DEBUG_READONLY_NOISE, __FUNCTION__, &InstanceContext->LfsDeviceExt, Irp );

			if (iopb->MinorFunction == IRP_MN_SURPRISE_REMOVAL) {

				if (InstanceContext->LfsDeviceExt.NetdiskPartition == NULL) {

					NDASFS_ASSERT( FALSE );
				
				} else {

					SetFlag( InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_SURPRISE_REMOVED );

					ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
					fastMutexAcquired = FALSE;

					NetdiskManager_SurpriseRemoval( GlobalLfs.NetdiskManager,
													InstanceContext->LfsDeviceExt.NetdiskPartition,
													InstanceContext->LfsDeviceExt.NetdiskEnabledMode );
				}

				InstanceContext->LfsDeviceExt.NetdiskPartition = NULL;			

				status = FLT_PREOP_SUCCESS_NO_CALLBACK;
				leave;
			}

			// Need to test much whether this is okay..
	
			if (iopb->MinorFunction == IRP_MN_CANCEL_REMOVE_DEVICE || iopb->MinorFunction == IRP_MN_REMOVE_DEVICE) {

				status = FLT_PREOP_SUCCESS_NO_CALLBACK;
				leave;
			
			} else if (iopb->MinorFunction == IRP_MN_QUERY_REMOVE_DEVICE) {

				ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
				fastMutexAcquired = FALSE;

				if (InstanceContext->LfsDeviceExt.Readonly) {

					ReadonlyTryCloseCcb( InstanceContext->LfsDeviceExt.Readonly );

					if (!IsListEmpty(&InstanceContext->LfsDeviceExt.Readonly->FcbQueue)) {

						LARGE_INTEGER interval;
				
						// Wait all files closed
						interval.QuadPart = (1 * DELAY_ONE_SECOND);      //delay 1 seconds
						KeDelayExecutionThread( KernelMode, FALSE, &interval );
					}
			
					if (!IsListEmpty(&InstanceContext->LfsDeviceExt.Readonly->FcbQueue)) {

						Data->IoStatus.Status = STATUS_UNSUCCESSFUL;
						Data->IoStatus.Information = 0;

						status = FLT_PREOP_COMPLETE;
						leave;
					} 
				}

				SetFlag( InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );

#if 0

				IoCopyCurrentIrpStackLocationToNext( Irp );
				KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

				IoSetCompletionRoutine( Irp,
										ReadonlyPassThroughCompletion,
										&waitEvent,
										TRUE,
										TRUE,
										TRUE );

				status = IoCallDriver( InstanceContext->LfsDeviceExt.AttachedToDeviceObject, Irp );

				if (status == STATUS_PENDING) {

					status = KeWaitForSingleObject( &waitEvent, Executive, KernelMode, FALSE, NULL );
					ASSERT( status == STATUS_SUCCESS );
				}

				ASSERT( KeReadStateEvent(&waitEvent) || !NT_SUCCESS(Irp->IoStatus.Status) );
	
				SPY_LOG_PRINT( LFS_DEBUG_READONLY_INFO, 
							   ("%s: lfsDeviceExt = %p, IRP_MN_QUERY_REMOVE_DEVICE Irp->IoStatus.Status = %x\n",
								__FUNCTION__, &InstanceContext->LfsDeviceExt, Irp->IoStatus.Status) );

				ClearFlag( InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );

				if (NT_SUCCESS(Irp->IoStatus.Status)) {

					ASSERT( !FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED) );
					LfsDismountVolume( &InstanceContext->LfsDeviceExt );
				}

				status = Irp->IoStatus.Status;
				IoCompleteRequest( Irp, IO_NO_INCREMENT );

				result = TRUE;
				leave;

#else

				status = FLT_PREOP_SUCCESS_WITH_CALLBACK;
				leave;

#endif
			}

			status = FLT_PREOP_SUCCESS_NO_CALLBACK;
			leave;
		}

	} finally {

		if (fastMutexAcquired)
			ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );

		LfsDeviceExt_Dereference( &InstanceContext->LfsDeviceExt );
	}

	return status;
}


NTSTATUS
NdasFsReadonlyPostOperationCallback (
	IN PCFLT_RELATED_OBJECTS	FltObjects,
	IN PCTX_INSTANCE_CONTEXT	InstanceContext,
	IN OUT PFLT_CALLBACK_DATA	Data
	)
{
	NTSTATUS				status = FLT_PREOP_SUCCESS_NO_CALLBACK;
	//PIO_STACK_LOCATION	iopb = IoGetCurrentIrpStackLocation( Irp );
	PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
	PFILE_OBJECT			fileObject = iopb->TargetFileObject;
	BOOLEAN					fastMutexAcquired = FALSE;


	//UNREFERENCED_PARAMETER( DeviceObject );
	UNREFERENCED_PARAMETER( FltObjects );

	ASSERT( InstanceContext->LfsDeviceExt.ReferenceCount );
	LfsDeviceExt_Reference( &InstanceContext->LfsDeviceExt );	

	PrintData( LFS_DEBUG_READONLY_NOISE, __FUNCTION__, &InstanceContext->LfsDeviceExt, Irp );

	ASSERT( KeGetCurrentIrql() <= APC_LEVEL );

	ExAcquireFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
	fastMutexAcquired = TRUE;

	try {

		if (!FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTED)) {

			NDASFS_ASSERT( FALSE );

			status = FLT_POSTOP_FINISHED_PROCESSING;
			leave;
		}

		ASSERT( InstanceContext->LfsDeviceExt.AttachedToDeviceObject == InstanceContext->DeviceObject );

		if (InstanceContext->LfsDeviceExt.AttachedToDeviceObject == NULL) {

			NDASFS_ASSERT( FALSE );

			status = FLT_POSTOP_FINISHED_PROCESSING;
			leave;
		}

		if (!(FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTED) && !FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED))) {

			NDASFS_ASSERT( FALSE );

			status = FLT_POSTOP_FINISHED_PROCESSING;
			leave;
		}

		switch (iopb->MajorFunction) {

		case IRP_MJ_PNP:

			break;

		default: 

			ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
			fastMutexAcquired = FALSE;

			if (InstanceContext->LfsDeviceExt.Readonly) {

				//status = ReadonlyRedirectIrp( InstanceContext, Irp, &result );
			
			} else {

				NDASFS_ASSERT( FALSE );
				status = FLT_POSTOP_FINISHED_PROCESSING;
			}

			leave;
		}

		if (iopb->MajorFunction == IRP_MJ_PNP) {

			PrintData( LFS_DEBUG_READONLY_NOISE, __FUNCTION__, &InstanceContext->LfsDeviceExt, Irp );

			if (iopb->MinorFunction == IRP_MN_SURPRISE_REMOVAL) {

				NDASFS_ASSERT( FALSE );

				status = FLT_POSTOP_FINISHED_PROCESSING;
				leave;
			}

			// Need to test much whether this is okay..
	
			if (iopb->MinorFunction == IRP_MN_CANCEL_REMOVE_DEVICE || iopb->MinorFunction == IRP_MN_REMOVE_DEVICE) {

				NDASFS_ASSERT( FALSE );

				status = FLT_POSTOP_FINISHED_PROCESSING;
				leave;
			
			} else if (iopb->MinorFunction == IRP_MN_QUERY_REMOVE_DEVICE) {

				SPY_LOG_PRINT( LFS_DEBUG_READONLY_INFO, 
							   ("%s: lfsDeviceExt = %p, IRP_MN_QUERY_REMOVE_DEVICE Irp->IoStatus.Status = %x\n",
								__FUNCTION__, &InstanceContext->LfsDeviceExt, Data->IoStatus.Status) );

				ClearFlag( InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );

				if (NT_SUCCESS(Data->IoStatus.Status)) {

					ASSERT( !FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED) );
					LfsDismountVolume( &InstanceContext->LfsDeviceExt );
				}

				status = FLT_POSTOP_FINISHED_PROCESSING;
				leave;

			}

			NDASFS_ASSERT( FALSE );

			status = FLT_POSTOP_FINISHED_PROCESSING;
			leave;
		}

	} finally {

		if (fastMutexAcquired)
			ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );

		LfsDeviceExt_Dereference( &InstanceContext->LfsDeviceExt );
	}

	return status;
}

NTSTATUS
NdasFsSecondaryPreOperationCallback (
	IN PCFLT_RELATED_OBJECTS	FltObjects,
	IN PCTX_INSTANCE_CONTEXT	InstanceContext,
	IN OUT PFLT_CALLBACK_DATA	Data
	)
{
	NTSTATUS				status = FLT_PREOP_SUCCESS_NO_CALLBACK;
	//PIO_STACK_LOCATION	iopb = IoGetCurrentIrpStackLocation( Irp );
	PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
	PFILE_OBJECT			fileObject = iopb->TargetFileObject;
	BOOLEAN					fastMutexAcquired = FALSE;


	//UNREFERENCED_PARAMETER( DeviceObject );
	UNREFERENCED_PARAMETER( FltObjects );

	ASSERT( KeGetCurrentIrql() <= APC_LEVEL );

	LfsDeviceExt_Reference( &InstanceContext->LfsDeviceExt );	

	InterlockedIncrement( &InstanceContext->LfsDeviceExt.ProcessingIrpCount );

	ExAcquireFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
	fastMutexAcquired = TRUE;
	
	try {

		if (iopb->MajorFunction == IRP_MJ_WRITE) {

			if (InstanceContext->LfsDeviceExt.Secondary == NULL ||
				Secondary_LookUpFileExtension(InstanceContext->LfsDeviceExt.Secondary, fileObject) == NULL) {

				SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsPassThrough Fake Write %wZ\n", &fileObject->FileName) );

				Data->IoStatus.Status = STATUS_SUCCESS;
				Data->IoStatus.Information = 0;

				status = FLT_PREOP_COMPLETE;
				leave;
			}
		}

		if (iopb->MajorFunction == IRP_MJ_CREATE) {

			//UNICODE_STRING	Extend = { 16, 16, L"\\$Extend" };
			//UNICODE_STRING	tempUnicode;

			if (IsMetaFile(&fileObject->FileName)) {

				status = FLT_PREOP_SUCCESS_NO_CALLBACK;
				leave;
			}

			if (RtlEqualUnicodeString(&GlobalLfs.MountMgrRemoteDatabase, &fileObject->FileName, TRUE)	|| 
				RtlEqualUnicodeString(&GlobalLfs.ExtendReparse, &fileObject->FileName, TRUE)			||
				RtlEqualUnicodeString(&GlobalLfs.MountPointManagerRemoteDatabase, &fileObject->FileName, TRUE)) {

				SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, 
							  ("LfsPassThrough: %wZ returned with STATUS_OBJECT_NAME_NOT_FOUND\n", &fileObject->FileName) );

				NDASFS_ASSERT( FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTED) );

				if (FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTED)) {

					Data->IoStatus.Status = STATUS_OBJECT_NAME_NOT_FOUND;
					Data->IoStatus.Information = 0;

					status = FLT_PREOP_COMPLETE;
					leave;
				
				} else {

					status = FLT_PREOP_SUCCESS_NO_CALLBACK;
					leave;
				}
			}

			if ((fileObject->FileName.Length == (sizeof(REMOUNT_VOLUME_FILE_NAME)-sizeof(WCHAR)) && 
				 RtlEqualMemory(fileObject->FileName.Buffer, REMOUNT_VOLUME_FILE_NAME, fileObject->FileName.Length)) ||
				(fileObject->FileName.Length == (sizeof(TOUCH_VOLUME_FILE_NAME)-sizeof(WCHAR)) && 
				 RtlEqualMemory(fileObject->FileName.Buffer, TOUCH_VOLUME_FILE_NAME, fileObject->FileName.Length))) {

				PrintData( LFS_DEBUG_LFS_TRACE, "TOUCH/REMOUNT_VOLUME_FILE_NAME", &InstanceContext->LfsDeviceExt, Irp );

				Data->IoStatus.Status = STATUS_OBJECT_NAME_NOT_FOUND;
				Data->IoStatus.Information = 0;

				status = FLT_PREOP_COMPLETE;
				leave;
			}

			PrintData( LFS_DEBUG_LFS_NOISE, __FUNCTION__, &InstanceContext->LfsDeviceExt, Irp );
		}

		if (!FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTED)) {

			status = FLT_PREOP_SUCCESS_NO_CALLBACK;
			leave;
		}

		if (FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED)) {

			status = FLT_PREOP_SUCCESS_NO_CALLBACK;
			leave;
		}

		if (FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING)) {

			NDASFS_ASSERT( InstanceContext->LfsDeviceExt.Secondary == NULL || 
						   Secondary_LookUpFileExtension(InstanceContext->LfsDeviceExt.Secondary, fileObject) == NULL );

			status = FLT_PREOP_SUCCESS_NO_CALLBACK;
			leave;
		}

		NDASFS_ASSERT( FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTED) && !FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED) );

		if (FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_SURPRISE_REMOVED)) {

			if (Secondary_LookUpFileExtension(InstanceContext->LfsDeviceExt.Secondary, fileObject)) {

				if (iopb->MajorFunction == IRP_MJ_CLOSE) {

					SecondaryFileObjectClose( InstanceContext->LfsDeviceExt.Secondary, fileObject );
		
					Data->IoStatus.Status = STATUS_SUCCESS;
					Data->IoStatus.Information = 0;

					status = FLT_PREOP_COMPLETE;
					leave;
				} 
				
				if (iopb->MajorFunction == IRP_MJ_PNP) {

					status = FLT_PREOP_SUCCESS_NO_CALLBACK;
					leave;
				} 

				if (iopb->MajorFunction == IRP_MJ_CLEANUP) {

					InterlockedDecrement( &((PLFS_FCB)iopb->TargetFileObject->FsContext)->UncleanCount );
					SetFlag( fileObject->Flags, FO_CLEANUP_COMPLETE );

					Data->IoStatus.Status = STATUS_SUCCESS;
					Data->IoStatus.Information = 0;
				
				} else {
				
					Data->IoStatus.Status = STATUS_DEVICE_REMOVED;
					Data->IoStatus.Information = 0;
				}

				status = FLT_PREOP_COMPLETE;
				leave;
			}

			status = FLT_PREOP_SUCCESS_NO_CALLBACK;
			leave;
		}

		if (iopb->MajorFunction == IRP_MJ_PNP) {

			switch (iopb->MinorFunction) {

			case IRP_MN_SURPRISE_REMOVAL: {
				
				PNETDISK_PARTITION	netdiskPartition2;

				NDASFS_ASSERT( InstanceContext->LfsDeviceExt.NetdiskPartition );
				
				netdiskPartition2 = InstanceContext->LfsDeviceExt.NetdiskPartition;
				InstanceContext->LfsDeviceExt.NetdiskPartition = NULL;

				SetFlag( InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_SURPRISE_REMOVED );

				ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
				fastMutexAcquired = FALSE;

				NetdiskManager_SurpriseRemoval( GlobalLfs.NetdiskManager,
												netdiskPartition2,
												InstanceContext->LfsDeviceExt.NetdiskEnabledMode );

				status = FLT_PREOP_SUCCESS_NO_CALLBACK;
				leave;
			} 
		
			case IRP_MN_QUERY_REMOVE_DEVICE: {
							
				//KEVENT				waitEvent;

				if (InstanceContext->LfsDeviceExt.ProcessingIrpCount != 1) {

					Data->IoStatus.Status = STATUS_UNSUCCESSFUL;
					Data->IoStatus.Information = 0;

					status = FLT_PREOP_COMPLETE;
					leave;
				}

				if (InstanceContext->LfsDeviceExt.SecondaryState == VOLUME_PURGE_STATE) {
					
					Data->IoStatus.Status = STATUS_UNSUCCESSFUL;
					Data->IoStatus.Information = 0;

					status = FLT_PREOP_COMPLETE;
					leave;
				}
			
				if (FlagOn(InstanceContext->LfsDeviceExt.Secondary->Flags, SECONDARY_FLAG_RECONNECTING)) {  // While Disabling and disconnected
	
					Data->IoStatus.Status = STATUS_UNSUCCESSFUL;
					Data->IoStatus.Information = 0;

					status = FLT_PREOP_COMPLETE;
					leave;
				}

				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO, ("Secondary_PassThrough: IRP_MN_QUERY_REMOVE_DEVICE Entered\n") );

				ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
				fastMutexAcquired = FALSE;

				Secondary_TryCloseFilExts( InstanceContext->LfsDeviceExt.Secondary );
			
				if (!IsListEmpty(&InstanceContext->LfsDeviceExt.Secondary->FcbQueue)) {

					LARGE_INTEGER interval;
				
					// Wait all files closed
					interval.QuadPart = (1 * DELAY_ONE_SECOND);      //delay 1 seconds
					KeDelayExecutionThread( KernelMode, FALSE, &interval );
				}
								
				ExAcquireFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
				fastMutexAcquired = TRUE;

				NDASFS_ASSERT( InstanceContext->LfsDeviceExt.AttachedToDeviceObject );

				if (!IsListEmpty(&InstanceContext->LfsDeviceExt.Secondary->FcbQueue)) {

					Data->IoStatus.Status = STATUS_UNSUCCESSFUL;
					Data->IoStatus.Information = 0;

					status = FLT_PREOP_COMPLETE;
					leave;
				}

				if (InstanceContext->LfsDeviceExt.ProcessingIrpCount != 1) {

					Data->IoStatus.Status = STATUS_UNSUCCESSFUL;
					Data->IoStatus.Information = 0;

					status = FLT_PREOP_COMPLETE;
					leave;
				}

				SetFlag( InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );

#if 0

				IoCopyCurrentIrpStackLocationToNext( Irp );
				KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

				IoSetCompletionRoutine( Irp,
										LfsPassThroughCompletion,
										&waitEvent,
										TRUE,
										TRUE,
										TRUE );

				status = IoCallDriver( InstanceContext->LfsDeviceExt.AttachedToDeviceObject, Irp );

				if (status == STATUS_PENDING) {

					status = KeWaitForSingleObject( &waitEvent, Executive, KernelMode, FALSE, NULL );
					ASSERT( status == STATUS_SUCCESS );
				}

				ASSERT( KeReadStateEvent(&waitEvent) || !NT_SUCCESS(Irp->IoStatus.Status) );
	
				SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO, 
							   ("%s: lfsDeviceExt = %p, IRP_MN_QUERY_REMOVE_DEVICE Irp->IoStatus.Status = %x\n",
								__FUNCTION__, &InstanceContext->LfsDeviceExt, Irp->IoStatus.Status) );

				ClearFlag( InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );

				if (NT_SUCCESS(Irp->IoStatus.Status)) {

					ASSERT( !FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED) );
					LfsDismountVolume( &InstanceContext->LfsDeviceExt );
				}

				status = Irp->IoStatus.Status;
				IoCompleteRequest( Irp, IO_NO_INCREMENT );

				result = TRUE;
				leave;

#else

				status = FLT_PREOP_SUCCESS_WITH_CALLBACK;
				leave;

#endif
			}

			case IRP_MN_REMOVE_DEVICE:

				NDASFS_ASSERT( FALSE );
	
			default:

				status = FLT_PREOP_SUCCESS_NO_CALLBACK;
				leave;
			}
		}
		
		if (iopb->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) {

			if (iopb->MinorFunction == IRP_MN_USER_FS_REQUEST || iopb->MinorFunction == IRP_MN_KERNEL_CALL) {

				switch (iopb->Parameters.FileSystemControl.Common.FsControlCode) {

				case FSCTL_LOCK_VOLUME: 
					
					if (InstanceContext->LfsDeviceExt.SecondaryState == VOLUME_PURGE_STATE) {

						status = FLT_PREOP_SUCCESS_NO_CALLBACK;
						leave;
					}

					SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO,
								   ("LfsPassThrough, IRP_MJ_FILE_SYSTEM_CONTROL: Return failure of acquiring the volume exclusively.\n") );

					NDASFS_ASSERT( fileObject && fileObject->FileName.Length == 0 && fileObject->RelatedFileObject == NULL );

					Data->IoStatus.Status = STATUS_ACCESS_DENIED;
					Data->IoStatus.Information = 0;

					status = FLT_PREOP_COMPLETE;
					leave;
									
				case FSCTL_DISMOUNT_VOLUME: {

					ASSERT( IS_WINDOWSVISTA_OR_LATER() );

					ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
					fastMutexAcquired = FALSE;

					SPY_LOG_PRINT( LFS_DEBUG_SECONDARY_INFO, ("Secondary_PassThrough: FSCTL_DISMOUNT_VOLUME Entered\n") );

					Secondary_TryCloseFilExts( InstanceContext->LfsDeviceExt.Secondary );
		
					if (!IsListEmpty(&InstanceContext->LfsDeviceExt.Secondary->FcbQueue)) {

						LARGE_INTEGER interval;
				
						// Wait all files closed
						interval.QuadPart = (2 * DELAY_ONE_SECOND);      //delay 1 seconds
						KeDelayExecutionThread( KernelMode, FALSE, &interval );
					}
			
					if (!IsListEmpty(&InstanceContext->LfsDeviceExt.Secondary->FcbQueue)) {

						ASSERT( FALSE );

						Data->IoStatus.Status = STATUS_UNSUCCESSFUL;
						Data->IoStatus.Information = 0;

						status = FLT_PREOP_COMPLETE;
						leave;
					} 

					ExAcquireFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
					fastMutexAcquired = TRUE;

					SetFlag( InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );

#if 0

					IoCopyCurrentIrpStackLocationToNext( Irp );
					KeInitializeEvent( &waitEvent, NotificationEvent, FALSE );

					IoSetCompletionRoutine( Irp,
											LfsPassThroughCompletion,
											&waitEvent,
											TRUE,
											TRUE,
											TRUE );

					status = IoCallDriver( InstanceContext->LfsDeviceExt.AttachedToDeviceObject, Irp );

					if (status == STATUS_PENDING) {

						status = KeWaitForSingleObject( &waitEvent, Executive, KernelMode, FALSE, NULL );
						ASSERT( status == STATUS_SUCCESS );
					}

					ASSERT( KeReadStateEvent(&waitEvent) || !NT_SUCCESS(Irp->IoStatus.Status) );

					ClearFlag( InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );
	
					if (NT_SUCCESS(Irp->IoStatus.Status)) {

						ASSERT( !FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED) );
						LfsDismountVolume( &InstanceContext->LfsDeviceExt );
					}

					status = Irp->IoStatus.Status;
					IoCompleteRequest( Irp, IO_NO_INCREMENT );

					SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, 
								   ("%s: FSCTL_DISMOUNT_VOLUME Irp->IoStatus.Status = %x\n",
									__FUNCTION__, Irp->IoStatus.Status) );

					result = TRUE;
					leave;	

#else

					status = FLT_PREOP_SUCCESS_WITH_CALLBACK;
					leave;

#endif
				}

				default:

					break;
				}
			}
		}

		if (iopb->MajorFunction != IRP_MJ_CREATE && 
			Secondary_LookUpFileExtension(InstanceContext->LfsDeviceExt.Secondary, fileObject) == NULL) {

			status = FLT_PREOP_SUCCESS_NO_CALLBACK;
			leave;
		} 

		if (InstanceContext->LfsDeviceExt.SecondaryState == WAIT_PURGE_SAFE_STATE) {

			NDASFS_ASSERT( iopb->MajorFunction == IRP_MJ_CREATE );

			if ((fileObject->FileName.Length == (sizeof(WAKEUP_VOLUME_FILE_NAME)-sizeof(WCHAR)) && 
				 RtlEqualMemory(fileObject->FileName.Buffer, WAKEUP_VOLUME_FILE_NAME, fileObject->FileName.Length)) || 
				(fileObject->FileName.Length == (sizeof(TOUCH_VOLUME_FILE_NAME)-sizeof(WCHAR)) && 
				 RtlEqualMemory(fileObject->FileName.Buffer, TOUCH_VOLUME_FILE_NAME, fileObject->FileName.Length))) {

				PrintData( LFS_DEBUG_LFS_INFO, "LfsPassThrough WAIT_PURGE_SAFE_STATE", &InstanceContext->LfsDeviceExt, Irp );

				Data->IoStatus.Status = STATUS_OBJECT_NAME_NOT_FOUND;
				Data->IoStatus.Information = 0;

				status = FLT_PREOP_COMPLETE;
				leave;
			}		

			PrintData( LFS_DEBUG_LFS_INFO, "LfsPassThrough WAIT_PURGE_SAFE_STATE", &InstanceContext->LfsDeviceExt, Irp );

			if (Data->RequestorMode == UserMode && fileObject && fileObject->FileName.Length) {

				InstanceContext->LfsDeviceExt.SecondaryState = SECONDARY_STATE;
			
			} else {

				status = FLT_PREOP_SUCCESS_NO_CALLBACK;
				leave;
			}
		}

		if (InstanceContext->LfsDeviceExt.SecondaryState == VOLUME_PURGE_STATE) {

			if (iopb->MajorFunction == IRP_MJ_CREATE) {

				if (fileObject->FileName.Length == (sizeof(WAKEUP_VOLUME_FILE_NAME)-sizeof(WCHAR)) && 
					RtlEqualMemory(fileObject->FileName.Buffer, WAKEUP_VOLUME_FILE_NAME, fileObject->FileName.Length)) {

					PrintData( LFS_DEBUG_LFS_TRACE, "WAKEUP_VOLUME_FILE_NAME", &InstanceContext->LfsDeviceExt, Irp );

					Data->IoStatus.Status = STATUS_OBJECT_NAME_NOT_FOUND;
					Data->IoStatus.Information = 0;

					status = FLT_PREOP_COMPLETE;
					leave;
				}

				if (Data->RequestorMode == KernelMode &&
					(fileObject->RelatedFileObject == NULL ||
					 Secondary_LookUpFileExtension(InstanceContext->LfsDeviceExt.Secondary, fileObject->RelatedFileObject) == NULL)) {

					PrintData( LFS_DEBUG_LFS_INFO, "LfsPassThrough VOLUME_PURGE_STATE", &InstanceContext->LfsDeviceExt, Irp );
					//ASSERT( fileObject->FileName.Length == 0 );

					status = FLT_PREOP_SUCCESS_NO_CALLBACK;
					leave;
				}
			}
		}

		ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
		fastMutexAcquired = FALSE;

		NDASFS_ASSERT( fileObject );

		if ((FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_INDIRECT) && InstanceContext->LfsDeviceExt.FileSystemType == LFS_FILE_SYSTEM_NDAS_FAT) ||
			InstanceContext->LfsDeviceExt.FileSystemType == LFS_FILE_SYSTEM_FAT) {

			if (InstanceContext->LfsDeviceExt.SecondaryState == CONNECTED_TO_LOCAL_STATE) {

				if (fileObject != NULL &&
					Secondary_LookUpFileExtension(InstanceContext->LfsDeviceExt.Secondary, fileObject) == NULL && 
					(fileObject->RelatedFileObject == NULL ||
					Secondary_LookUpFileExtension(InstanceContext->LfsDeviceExt.Secondary, fileObject->RelatedFileObject) == NULL)) {

					status = FLT_PREOP_SUCCESS_NO_CALLBACK;
					leave;
				} 
			}
		}

		status = MiniSecondaryNtfsPassThrough( InstanceContext->LfsDeviceExt.Secondary, Data );

	} finally {

		InterlockedDecrement( &InstanceContext->LfsDeviceExt.ProcessingIrpCount );

		if (fastMutexAcquired)
			ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );

		LfsDeviceExt_Dereference( &InstanceContext->LfsDeviceExt );
	}

	return status;
}


NTSTATUS
NdasFsSecondaryPostOperationCallback (
	IN PCFLT_RELATED_OBJECTS	FltObjects,
	IN PCTX_INSTANCE_CONTEXT	InstanceContext,
	IN OUT PFLT_CALLBACK_DATA	Data
	)
{
	NTSTATUS				status = FLT_POSTOP_FINISHED_PROCESSING;
	//PIO_STACK_LOCATION	iopb = IoGetCurrentIrpStackLocation( Irp );
	PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
	PFILE_OBJECT			fileObject = iopb->TargetFileObject;
	BOOLEAN					fastMutexAcquired = FALSE;


	//UNREFERENCED_PARAMETER( DeviceObject );
	UNREFERENCED_PARAMETER( FltObjects );

	ASSERT( InstanceContext->LfsDeviceExt.ReferenceCount );
	LfsDeviceExt_Reference( &InstanceContext->LfsDeviceExt );	

	PrintData( LFS_DEBUG_PRIMARY_NOISE, __FUNCTION__, &InstanceContext->LfsDeviceExt, Irp );

	ASSERT( KeGetCurrentIrql() <= APC_LEVEL );

	ExAcquireFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
	fastMutexAcquired = TRUE;

	try {

		if (!FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTED)) {

			NDASFS_ASSERT( FALSE );

			status = FLT_POSTOP_FINISHED_PROCESSING;
			leave;
		}

		if (FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED)) {

			NDASFS_ASSERT( FALSE );

			status = FLT_POSTOP_FINISHED_PROCESSING;
			leave;
		}

		ASSERT( InstanceContext->LfsDeviceExt.AttachedToDeviceObject == InstanceContext->DeviceObject );

		if (InstanceContext->LfsDeviceExt.AttachedToDeviceObject == NULL) {

			NDASFS_ASSERT( FALSE );

			status = FLT_POSTOP_FINISHED_PROCESSING;
			leave;
		}

		if (!(FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_MOUNTED) && !FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED))) {

			NDASFS_ASSERT( FALSE );

			if (InstanceContext->LfsDeviceExt.Secondary != NULL && 
				Secondary_LookUpFileExtension(InstanceContext->LfsDeviceExt.Secondary, fileObject) != NULL) {
				
				status = FLT_POSTOP_FINISHED_PROCESSING;
				leave;
			}

			status = FLT_POSTOP_FINISHED_PROCESSING;
			leave;
		}

		if (iopb->MajorFunction == IRP_MJ_CREATE)
			PrintData( LFS_DEBUG_PRIMARY_NOISE, __FUNCTION__, &InstanceContext->LfsDeviceExt, Irp );

		if (iopb->MajorFunction == IRP_MJ_PNP) {

			if (iopb->MinorFunction == IRP_MN_SURPRISE_REMOVAL) {

				NDASFS_ASSERT( FALSE );

				status = FLT_POSTOP_FINISHED_PROCESSING;
				leave;
			}

			// Need to test much whether this is okay..
	
			if (iopb->MinorFunction == IRP_MN_CANCEL_REMOVE_DEVICE || iopb->MinorFunction == IRP_MN_REMOVE_DEVICE) {

				NDASFS_ASSERT( FALSE );

				status = FLT_POSTOP_FINISHED_PROCESSING;
				leave;
			
			} else if (iopb->MinorFunction == IRP_MN_QUERY_REMOVE_DEVICE) {

				if (InstanceContext->LfsDeviceExt.AttachedToDeviceObject == NULL) {

					NDASFS_ASSERT( FALSE );

					status = FLT_POSTOP_FINISHED_PROCESSING;
					leave;
				}

				NDASFS_ASSERT( FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING) );

				ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
				fastMutexAcquired = FALSE;
	
				SPY_LOG_PRINT( LFS_DEBUG_PRIMARY_INFO, 
							   ("%s: lfsDeviceExt = %p, IRP_MN_QUERY_REMOVE_DEVICE Data->IoStatus.Status = %x\n",
								__FUNCTION__, &InstanceContext->LfsDeviceExt, Data->IoStatus.Status) );

				ClearFlag( InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );

				if (NT_SUCCESS(Data->IoStatus.Status)) {

					NDASFS_ASSERT( !FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED) );
					LfsDismountVolume( &InstanceContext->LfsDeviceExt );
				}

				status = FLT_POSTOP_FINISHED_PROCESSING;
				leave;
			}

			NDASFS_ASSERT( FALSE );

			status = FLT_POSTOP_FINISHED_PROCESSING;
			leave;
		
		} else if (iopb->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) {

			if (!FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_INDIRECT) &&
				(InstanceContext->LfsDeviceExt.FileSystemType == LFS_FILE_SYSTEM_NDAS_FAT ||
				 InstanceContext->LfsDeviceExt.FileSystemType == LFS_FILE_SYSTEM_NDAS_NTFS)) {

				if (!(iopb->MinorFunction == IRP_MN_USER_FS_REQUEST || iopb->MinorFunction == IRP_MN_KERNEL_CALL)) {

					NDASFS_ASSERT( FALSE );

					status = FLT_POSTOP_FINISHED_PROCESSING;
					leave;
				}
			}
			
			PrintData( LFS_DEBUG_PRIMARY_NOISE, __FUNCTION__, &InstanceContext->LfsDeviceExt, Irp );

			if (iopb->MinorFunction == IRP_MN_USER_FS_REQUEST || iopb->MinorFunction == IRP_MN_KERNEL_CALL) {

				//	Do not allow exclusive access to the volume and dismount volume to protect format
				//	We allow exclusive access if secondaries are connected locally.
				//

				if (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_LOCK_VOLUME) {
						
					NDASFS_ASSERT( FALSE );

					status = FLT_POSTOP_FINISHED_PROCESSING;
					leave;
					
				} else if (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_UNLOCK_VOLUME) {

					NDASFS_ASSERT( FALSE );

					status = FLT_POSTOP_FINISHED_PROCESSING;
					leave;
	
				} else if (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_DISMOUNT_VOLUME) {
						
					ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );
					fastMutexAcquired = FALSE;
	
					NDASFS_ASSERT( FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING) );

					ClearFlag( InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTING );
	
					if (NT_SUCCESS(Data->IoStatus.Status)) {

						ASSERT( !FlagOn(InstanceContext->LfsDeviceExt.Flags, LFS_DEVICE_FLAG_DISMOUNTED) );
						LfsDismountVolume( &InstanceContext->LfsDeviceExt );
					}

					SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, 
								   ("%s: FSCTL_DISMOUNT_VOLUME Data->IoStatus.Status = %x\n",
									__FUNCTION__, Data->IoStatus.Status) );

					status = FLT_POSTOP_FINISHED_PROCESSING;
					leave;

				} else if (iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_SET_ENCRYPTION) {

					NDASFS_ASSERT( FALSE );

					status = FLT_POSTOP_FINISHED_PROCESSING;
					leave;
				}
			}
		}

	} finally {

		if (fastMutexAcquired)
			ExReleaseFastMutex( &InstanceContext->LfsDeviceExt.FastMutex );

		LfsDeviceExt_Dereference( &InstanceContext->LfsDeviceExt );
	}

	return status;
}


#endif
