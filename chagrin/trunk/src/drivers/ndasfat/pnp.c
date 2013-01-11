/*++

Copyright (c) 1997-2000 Microsoft Corporation

Module Name:

    Pnp.c

Abstract:

    This module implements the Plug and Play routines for FAT called by
    the dispatch driver.


--*/

#include "FatProcs.h"

//
//  The Bug check file id for this module
//

#define BugCheckFileId                   (FAT_BUG_CHECK_PNP)

#define Dbg                              (DEBUG_TRACE_PNP)

NTSTATUS
FatPnpQueryRemove (
    PIRP_CONTEXT IrpContext,
    PIRP Irp,
    PVCB Vcb
    );

NTSTATUS
FatPnpRemove (
    PIRP_CONTEXT IrpContext,
    PIRP Irp,
    PVCB Vcb
    );

NTSTATUS
FatPnpSurpriseRemove (
    PIRP_CONTEXT IrpContext,
    PIRP Irp,
    PVCB Vcb
    );

NTSTATUS
FatPnpCancelRemove (
    PIRP_CONTEXT IrpContext,
    PIRP Irp,
    PVCB Vcb
    );

NTSTATUS
FatPnpCompletionRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Contxt
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FatCommonPnp)
#pragma alloc_text(PAGE, FatFsdPnp)
#pragma alloc_text(PAGE, FatPnpCancelRemove)
#pragma alloc_text(PAGE, FatPnpQueryRemove)
#pragma alloc_text(PAGE, FatPnpRemove)
#pragma alloc_text(PAGE, FatPnpSurpriseRemove)
#endif


NTSTATUS
FatFsdPnp (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    This routine implements the FSD part of PnP operations

Arguments:

    VolumeDeviceObject - Supplies the volume device object where the
        file exists

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The FSD status for the IRP

--*/

{
    NTSTATUS Status;
    PIRP_CONTEXT IrpContext = NULL;

    BOOLEAN TopLevel;
    BOOLEAN Wait;

    PAGED_CODE();

#if __NDAS_FAT__

	if ((PVOID)FatControlDeviceObject == VolumeDeviceObject) {

		Status = Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );

		return Status;
	}

#endif

    DebugTrace(+1, Dbg, "FatFsdPnp\n", 0);

    FsRtlEnterFileSystem();

    TopLevel = FatIsIrpTopLevel( Irp );

#if (__NDAS_FAT_PRIMARY__ || __NDAS_FAT_SECONDARY__)

	do {
    
		try {

			if (IrpContext == NULL) { 

				if (IoGetCurrentIrpStackLocation( Irp )->FileObject == NULL) {

					Wait = TRUE;

				} else {

					Wait = CanFsdWait( Irp );
				}

				IrpContext = FatCreateIrpContext( Irp, Wait );

				IrpContext->TopLevel = TopLevel;
			}

#if __NDAS_FAT_SECONDARY__

			if (!FlagOn(VolumeDeviceObject->NetdiskPartitionInformation.Flags, NETDISK_PARTITION_INFORMATION_FLAG_INDIRECT) &&
				VolumeDeviceObject->NetdiskEnableMode == NETDISK_SECONDARY) {

				if (IrpContext->MinorFunction == IRP_MN_QUERY_REMOVE_DEVICE	|| 
					IrpContext->MinorFunction == IRP_MN_SURPRISE_REMOVAL	||
					IrpContext->MinorFunction == IRP_MN_CANCEL_REMOVE_DEVICE) {

					BOOLEAN	volDoResourceAcquired = FALSE;
					BOOLEAN volDoRecoveryResourceAcquired = FALSE;

					ASSERT( FatIsTopLevelRequest(IrpContext) );

					Status = STATUS_SUCCESS;

					while (TRUE) {
			
						ASSERT( volDoRecoveryResourceAcquired == FALSE );
						ASSERT( volDoResourceAcquired == FALSE );

						if (FlagOn(VolumeDeviceObject->Secondary->Flags, SECONDARY_FLAG_RECONNECTING)) {
		
							if (!FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT)) {

								Status = FatFsdPostRequest( IrpContext, Irp );
								break;
							}
						}
					
						if (FlagOn(VolumeDeviceObject->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED)) {

							if (!FlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT)) {

								Status = FatFsdPostRequest( IrpContext, Irp );
								break;
							}
						
							volDoRecoveryResourceAcquired 
								= SecondaryAcquireResourceExclusiveLite( IrpContext, 
																		 &VolumeDeviceObject->RecoveryResource, 
																		 BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
								
							if (!FlagOn(VolumeDeviceObject->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

								SecondaryReleaseResourceLite( IrpContext, &VolumeDeviceObject->RecoveryResource );
								volDoRecoveryResourceAcquired = FALSE;
								continue;
							}

							volDoResourceAcquired 
								= SecondaryAcquireResourceExclusiveLite( IrpContext, 
																		 &VolumeDeviceObject->Resource, 
																		 BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );
							try {
								
								SecondaryRecoverySessionStart( VolumeDeviceObject->Secondary, IrpContext );
									
							} finally {

								SecondaryReleaseResourceLite( IrpContext, &VolumeDeviceObject->Resource );
								volDoResourceAcquired = FALSE;

								SecondaryReleaseResourceLite( IrpContext, &VolumeDeviceObject->RecoveryResource );
								volDoRecoveryResourceAcquired = FALSE;
							}

							continue;
						}

						volDoResourceAcquired 
							= SecondaryAcquireResourceSharedLite( IrpContext, 
																  &VolumeDeviceObject->Resource, 
																  BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

						if (volDoResourceAcquired == FALSE) {

							ASSERT( FlagOn(VolumeDeviceObject->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ||
									FlagOn(VolumeDeviceObject->Secondary->Flags, SECONDARY_FLAG_RECONNECTING) );

							continue;
						}

						break;
					}

					if (Status == STATUS_SUCCESS) {
					
						try {

							if (VolumeDeviceObject->NetdiskEnableMode != NETDISK_SECONDARY) {

								NDASFAT_ASSERT( VolumeDeviceObject->NetdiskEnableMode == NETDISK_SECONDARY2PRIMARY );
								SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_DONT_POST_REQUEST );
								FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
							}

							SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_SECONDARY_CONTEXT );

							Status = FatCommonPnp( IrpContext, Irp );
							
						} finally {

							ASSERT( ExIsResourceAcquiredSharedLite(&VolumeDeviceObject->Resource) );
							SecondaryReleaseResourceLite( NULL, &VolumeDeviceObject->Resource );
						}
					}

				} else {

					if (VolumeDeviceObject->NetdiskEnableMode != NETDISK_SECONDARY) {

						NDASFAT_ASSERT( VolumeDeviceObject->NetdiskEnableMode == NETDISK_SECONDARY2PRIMARY );
						SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_DONT_POST_REQUEST );
						FatRaiseStatus( IrpContext, STATUS_CANT_WAIT );
					}

					SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_SECONDARY_CONTEXT );

					Status = FatCommonPnp( IrpContext, Irp );
				}
			
			} else {

				Status = FatCommonPnp( IrpContext, Irp );
			}
#else
	        Status = FatCommonPnp( IrpContext, Irp );
#endif

		} except(FatExceptionFilter( IrpContext, GetExceptionInformation() )) {

			//
	        //  We had some trouble trying to perform the requested
		    //  operation, so we'll abort the I/O request with
			//  the error status that we get back from the
	        //  execption code
		    //

			Status = FatProcessException( IrpContext, Irp, GetExceptionCode() );
		}
	
	} while (Status == STATUS_CANT_WAIT);
	
#else

    try {

        //
        //  We expect there to never be a fileobject, in which case we will always
        //  wait.  Since at the moment we don't have any concept of pending Pnp
        //  operations, this is a bit nitpicky.
        //
        
        if (IoGetCurrentIrpStackLocation( Irp )->FileObject == NULL) {

            Wait = TRUE;

        } else {

            Wait = CanFsdWait( Irp );
        }

        IrpContext = FatCreateIrpContext( Irp, Wait );

        Status = FatCommonPnp( IrpContext, Irp );

    } except(FatExceptionFilter( IrpContext, GetExceptionInformation() )) {

        //
        //  We had some trouble trying to perform the requested
        //  operation, so we'll abort the I/O request with
        //  the error status that we get back from the
        //  execption code
        //

        Status = FatProcessException( IrpContext, Irp, GetExceptionCode() );
    }

#endif

    if (TopLevel) { IoSetTopLevelIrp( NULL ); }

    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    DebugTrace(-1, Dbg, "FatFsdPnp -> %08lx\n", Status);

    UNREFERENCED_PARAMETER( VolumeDeviceObject );

    return Status;
}


NTSTATUS
FatCommonPnp (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for doing PnP operations called
    by both the fsd and fsp threads

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    
    PIO_STACK_LOCATION IrpSp;

    PVOLUME_DEVICE_OBJECT OurDeviceObject;
    PVCB Vcb;

    PAGED_CODE();

    //
    //  Force everything to wait.
    //
    
    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT);
    
    //
    //  Get the current Irp stack location.
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    //
    //  Find our Vcb.  This is tricky since we have no file object in the Irp.
    //

    OurDeviceObject = (PVOLUME_DEVICE_OBJECT) IrpSp->DeviceObject;

    //
    //  Take the global lock to synchronise against volume teardown.
    //

#if __NDAS_FAT_PRIMARY__

	if (IrpSp->MinorFunction == IRP_MN_QUERY_REMOVE_DEVICE) {

		NdasNtfsPrimarySessionStopping( OurDeviceObject );
		SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_PRIMARY_STOPPING );
	
	} else if (IrpSp->MinorFunction == IRP_MN_SURPRISE_REMOVAL) {

		NdasNtfsPrimarySessionSurpriseRemoval( OurDeviceObject );
		SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_PRIMARY_SURPRISE_REMOVAL );
	}

#endif

    FatAcquireExclusiveGlobal( IrpContext );    
    
    //
    //  Make sure this device object really is big enough to be a volume device
    //  object.  If it isn't, we need to get out before we try to reference some
    //  field that takes us past the end of an ordinary device object.
    //
    
    if (OurDeviceObject->DeviceObject.Size != sizeof(VOLUME_DEVICE_OBJECT) ||
        NodeType( &OurDeviceObject->Vcb ) != FAT_NTC_VCB) {
        
        //
        //  We were called with something we don't understand.
        //

        FatReleaseGlobal( IrpContext );
        
        Status = STATUS_INVALID_PARAMETER;
        FatCompleteRequest( IrpContext, Irp, Status );

#if __NDAS_FAT_PRIMARY__
		if (FlagOn(IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_PRIMARY_STOPPING)) {

			NdasNtfsPrimarySessionCancelStopping( OurDeviceObject );
			ClearFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_PRIMARY_STOPPING );
		}
#endif
        return Status;
    }

    Vcb = &OurDeviceObject->Vcb;

    //
    //  Case on the minor code.
    //
    
    switch ( IrpSp->MinorFunction ) {

        case IRP_MN_QUERY_REMOVE_DEVICE:
            
            Status = FatPnpQueryRemove( IrpContext, Irp, Vcb );
            break;
        
        case IRP_MN_SURPRISE_REMOVAL:
        
            Status = FatPnpSurpriseRemove( IrpContext, Irp, Vcb );
            break;

        case IRP_MN_REMOVE_DEVICE:

            Status = FatPnpRemove( IrpContext, Irp, Vcb );
            break;

        case IRP_MN_CANCEL_REMOVE_DEVICE:
    
            Status = FatPnpCancelRemove( IrpContext, Irp, Vcb );
            break;

        default:

            FatReleaseGlobal( IrpContext );
            
            //
            //  Just pass the IRP on.  As we do not need to be in the
            //  way on return, ellide ourselves out of the stack.
            //
            
            IoSkipCurrentIrpStackLocation( Irp );
    
            Status = IoCallDriver(Vcb->TargetDeviceObject, Irp);
            
            //
            //  Cleanup our Irp Context.  The driver has completed the Irp.
            //
        
            FatCompleteRequest( IrpContext, NULL, STATUS_SUCCESS );
            
            break;
    }
        
    return Status;
}


VOID
FatPnpAdjustVpbRefCount( 
    IN PVCB Vcb,
    IN ULONG Delta
    )
{
    KIRQL OldIrql;
    
    IoAcquireVpbSpinLock( &OldIrql);
    Vcb->Vpb->ReferenceCount += Delta;
    IoReleaseVpbSpinLock( OldIrql);
}


NTSTATUS
FatPnpQueryRemove (
    PIRP_CONTEXT IrpContext,
    PIRP Irp,
    PVCB Vcb
    )

/*++

Routine Description:

    This routine handles the PnP query remove operation.  The filesystem
    is responsible for answering whether there are any reasons it sees
    that the volume can not go away (and the device removed).  Initiation
    of the dismount begins when we answer yes to this question.
    
    Query will be followed by a Cancel or Remove.

Arguments:

    Irp - Supplies the Irp to process
    
    Vcb - Supplies the volume being queried.

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    KEVENT Event;
    BOOLEAN VcbDeleted = FALSE;
    BOOLEAN GlobalHeld = TRUE;

#if __NDAS_FAT_SECONDARY__
	PVOLUME_DEVICE_OBJECT	volDo = CONTAINING_RECORD(Vcb, VOLUME_DEVICE_OBJECT, Vcb );
	BOOLEAN					secondaryCreateResourceAcquired = FALSE;
	BOOLEAN					secondaryVcbAcquired = FALSE;
#endif

    PAGED_CODE();

    //
    //  Having said yes to a QUERY, any communication with the
    //  underlying storage stack is undefined (and may block)
    //  until the bounding CANCEL or REMOVE is sent.
    //

    FatAcquireExclusiveVcb( IrpContext, Vcb );

    FatReleaseGlobal( IrpContext);
    GlobalHeld = FALSE;

    try {
        
#if __NDAS_FAT_SECONDARY__

		if (volDo->Secondary) {
		
			BOOLEAN	secondaryRecoveryResourceAcquired;

			ASSERT( volDo->NetdiskEnableMode == NETDISK_SECONDARY || 
					volDo->NetdiskEnableMode == NETDISK_SECONDARY2PRIMARY );

			Secondary_Reference( volDo->Secondary );

			DebugTrace2( 0, Dbg, ("FatPnpQueryRemove: IRP_MN_QUERY_REMOVE_DEVICE volDo = %p, NetdiskEnableMode = %d\n", 
								   volDo, volDo->NetdiskEnableMode) );

			secondaryRecoveryResourceAcquired 
				= SecondaryAcquireResourceExclusiveLite( IrpContext, 
														 &volDo->RecoveryResource, 
														 FALSE );
			
			if (secondaryRecoveryResourceAcquired == FALSE) {

				Status = STATUS_ACCESS_DENIED;
				Secondary_Dereference( volDo->Secondary );	
				leave;
			}

			SecondaryReleaseResourceLite( IrpContext, &volDo->RecoveryResource );

			ExAcquireFastMutex( &volDo->Secondary->FastMutex );	

			if (!volDo->Secondary->TryCloseActive) {
				
				volDo->Secondary->TryCloseActive = TRUE;
		
				ExReleaseFastMutex( &volDo->Secondary->FastMutex );

				Secondary_Reference( volDo->Secondary );
				SecondaryTryClose( volDo->Secondary );
			
			} else {
				
				ExReleaseFastMutex( &volDo->Secondary->FastMutex );
			}

			if (Vcb->SecondaryOpenFileCount) {

				LARGE_INTEGER interval;

				// Wait all files closed
				interval.QuadPart = (1 * HZ);      //delay 1 seconds
				KeDelayExecutionThread(KernelMode, FALSE, &interval);
			}

			if (!FlagOn(IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_SECONDARY_CONTEXT)) {
				
				SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_SECONDARY_CONTEXT );
	
				FatAcquireExclusiveVcb( IrpContext, Vcb );
				secondaryVcbAcquired = TRUE;

				ClearFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_SECONDARY_CONTEXT );
			
			} else {

				FatAcquireExclusiveVcb( IrpContext, Vcb );
				secondaryVcbAcquired = TRUE;
			}

			secondaryCreateResourceAcquired 
				= SecondaryAcquireResourceExclusiveLite( IrpContext, 
														 &volDo->CreateResource, 
														 BooleanFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) );

			if (secondaryCreateResourceAcquired == FALSE) {

				Status = STATUS_ACCESS_DENIED;
				Secondary_Dereference( volDo->Secondary );	
				leave;
			}
		
			if (Vcb->SecondaryOpenFileCount) {

				LONG		ccbCount;
				PLIST_ENTRY	ccbListEntry;

				ExAcquireFastMutex( &volDo->Secondary->RecoveryCcbQMutex );

			    for (ccbCount = 0, ccbListEntry = volDo->Secondary->RecoveryCcbQueue.Flink; 
					 ccbListEntry != &volDo->Secondary->RecoveryCcbQueue; 
					 ccbListEntry = ccbListEntry->Flink, ccbCount++);

				ExReleaseFastMutex( &volDo->Secondary->RecoveryCcbQMutex );

				ASSERT( !IsListEmpty(&volDo->Secondary->RecoveryCcbQueue) );
				ASSERT( ccbCount == Vcb->SecondaryOpenFileCount );

				DebugTrace2( 0, Dbg, ("IRP_MN_QUERY_REMOVE_DEVICE: Vcb->SecondaryCloseCount = %d, Vcb->OpenFileCount = %d, ccbCount = %d\n",
				                       Vcb->SecondaryOpenFileCount, Vcb->OpenFileCount, ccbCount) );

#if 0
				//PVOID		restartKey;
				//PFCB		fcb;

				restartKey = NULL;
				fcb = NdasFatGetNextFcbTableEntry( &OurDeviceObject->Secondary->VolDo->Vcb, &restartKey );
				ASSERT( fcb != NULL || !IsListEmpty(&OurDeviceObject->Secondary->DeletedFcbQueue) );
#endif
				Status = STATUS_ACCESS_DENIED;		
				Secondary_Dereference( volDo->Secondary );	
				leave;
			}

			Secondary_Dereference( volDo->Secondary );	
		}

#endif

        Status = FatLockVolumeInternal( IrpContext, Vcb, NULL );

        //
        //  Drop an additional reference on the Vpb so that the volume cannot be
        //  torn down when we drop all the locks below.
        //
        
        FatPnpAdjustVpbRefCount( Vcb, 1);
        
        //
        //  Drop and reacquire the resources in the right order.
        //

        FatReleaseVcb( IrpContext, Vcb );
        FatAcquireExclusiveGlobal( IrpContext );
        GlobalHeld = TRUE;
        FatAcquireExclusiveVcb( IrpContext, Vcb );

        //
        //  Drop the reference we added above.
        //
        
        FatPnpAdjustVpbRefCount( Vcb, -1);

        if (NT_SUCCESS( Status )) {

            //
            //  With the volume held locked, note that we must finalize as much
            //  as possible right now.
            //

            FatFlushAndCleanVolume( IrpContext, Irp, Vcb, Flush );

            //
            //  We need to pass this down before starting the dismount, which
            //  could disconnect us immediately from the stack.
            //

            //
            //  Get the next stack location, and copy over the stack location
            //

            IoCopyCurrentIrpStackLocationToNext( Irp );

            //
            //  Set up the completion routine
            //

            KeInitializeEvent( &Event, NotificationEvent, FALSE );
            IoSetCompletionRoutine( Irp,
                                    FatPnpCompletionRoutine,
                                    &Event,
                                    TRUE,
                                    TRUE,
                                    TRUE );

            //
            //  Send the request and wait.
            //

            Status = IoCallDriver(Vcb->TargetDeviceObject, Irp);

            if (Status == STATUS_PENDING) {

                KeWaitForSingleObject( &Event,
                                       Executive,
                                       KernelMode,
                                       FALSE,
                                       NULL );

                Status = Irp->IoStatus.Status;
            }

            //
            //  Now if no one below us failed already, initiate the dismount
            //  on this volume, make it go away.  PnP needs to see our internal
            //  streams close and drop their references to the target device.
            //
            //  Since we were able to lock the volume, we are guaranteed to
            //  move this volume into dismount state and disconnect it from
            //  the underlying storage stack.  The force on our part is actually
            //  unnecesary, though complete.
            //
            //  What is not strictly guaranteed, though, is that the closes
            //  for the metadata streams take effect synchronously underneath
            //  of this call.  This would leave references on the target device
            //  even though we are disconnected!
            //

            if (NT_SUCCESS( Status )) {

                VcbDeleted = FatCheckForDismount( IrpContext, Vcb, TRUE );

                ASSERT( VcbDeleted || Vcb->VcbCondition == VcbBad );

            }
        }

    } finally {
        
        //
        //  Release the Vcb if it could still remain.
        //

        if (!VcbDeleted) {

            FatReleaseVcb( IrpContext, Vcb );

#if __NDAS_FAT_SECONDARY__

			if (secondaryCreateResourceAcquired) {

				SecondaryReleaseResourceLite( IrpContext, &volDo->CreateResource );
			}

			if (secondaryVcbAcquired == TRUE) {

				if (!FlagOn(IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_SECONDARY_CONTEXT)) {

					SetFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_SECONDARY_CONTEXT );
					FatReleaseVcb( IrpContext, Vcb );
					ClearFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_SECONDARY_CONTEXT );

				} else {

					FatReleaseVcb( IrpContext, Vcb );
				}
			}
#endif
		}

#if __NDAS_FAT_SECONDARY__
	
		if (NT_SUCCESS(Status)) {

			NDASFAT_ASSERT( Vcb->SecondaryOpenFileCount == 0 );

			if (CONTAINING_RECORD(Vcb, VOLUME_DEVICE_OBJECT, Vcb)->BlockAceId) {

				((PVOLUME_DEVICE_OBJECT)FatData.DiskFileSystemDeviceObject)->NdfsCallback.RemoveWriteRange( Vcb->Vpb->RealDevice, 
																										    CONTAINING_RECORD(Vcb, VOLUME_DEVICE_OBJECT, Vcb)->BlockAceId );

				CONTAINING_RECORD(Vcb, VOLUME_DEVICE_OBJECT, Vcb)->BlockAceId = 0;
			}
		}

#endif

#if __NDAS_FAT_PRIMARY__

		if (FlagOn(IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_PRIMARY_STOPPING)) {

			if (NT_SUCCESS(Status)) {

				NdasNtfsPrimarySessionDisconnect( CONTAINING_RECORD(Vcb, VOLUME_DEVICE_OBJECT, Vcb) );
				
			} else {

				NdasNtfsPrimarySessionCancelStopping( CONTAINING_RECORD(Vcb, VOLUME_DEVICE_OBJECT, Vcb) );
			}

			ClearFlag( IrpContext->NdasFatFlags, NDAS_FAT_IRP_CONTEXT_FLAG_PRIMARY_STOPPING );
		}

#endif

        if (GlobalHeld) {
            
            FatReleaseGlobal( IrpContext );
        }
    }
    
    //
    //  Cleanup our IrpContext and complete the IRP if neccesary.
    //

    FatCompleteRequest( IrpContext, Irp, Status );

    return Status;
}


NTSTATUS
FatPnpRemove (
    PIRP_CONTEXT IrpContext,
    PIRP Irp,
    PVCB Vcb
    )

/*++

Routine Description:

    This routine handles the PnP remove operation.  This is our notification
    that the underlying storage device for the volume we have is gone, and
    an excellent indication that the volume will never reappear. The filesystem
    is responsible for initiation or completion of the dismount.
    
Arguments:

    Irp - Supplies the Irp to process
    
    Vcb - Supplies the volume being removed.

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    KEVENT Event;
    BOOLEAN VcbDeleted;

    PAGED_CODE();

    //
    //  REMOVE - a storage device is now gone.  We either got
    //  QUERY'd and said yes OR got a SURPRISE OR a storage
    //  stack failed to spin back up from a sleep/stop state
    //  (the only case in which this will be the first warning).
    //
    //  Note that it is entirely unlikely that we will be around
    //  for a REMOVE in the first two cases, as we try to intiate
    //  dismount.
    //
    
    //
    //  Acquire the global resource so that we can try to vaporize
    //  the volume, and the vcb resource itself.
    //

    FatAcquireExclusiveVcb( IrpContext, Vcb );

    //
    //  The device will be going away.  Remove our lock (benign
    //  if we never had it).
    //

    (VOID) FatUnlockVolumeInternal( IrpContext, Vcb, NULL );
    
    //
    //  We need to pass this down before starting the dismount, which
    //  could disconnect us immediately from the stack.
    //
    
    //
    //  Get the next stack location, and copy over the stack location
    //

    IoCopyCurrentIrpStackLocationToNext( Irp );

    //
    //  Set up the completion routine
    //

    KeInitializeEvent( &Event, NotificationEvent, FALSE );
    IoSetCompletionRoutine( Irp,
                            FatPnpCompletionRoutine,
                            &Event,
                            TRUE,
                            TRUE,
                            TRUE );

    //
    //  Send the request and wait.
    //

    Status = IoCallDriver(Vcb->TargetDeviceObject, Irp);

    if (Status == STATUS_PENDING) {

        KeWaitForSingleObject( &Event,
                               Executive,
                               KernelMode,
                               FALSE,
                               NULL );

        Status = Irp->IoStatus.Status;
    }

    try {
        
        //
        //  Knock as many files down for this volume as we can.
        //

        FatFlushAndCleanVolume( IrpContext, Irp, Vcb, NoFlush );

        //
        //  Now make our dismount happen.  This may not vaporize the
        //  Vcb, of course, since there could be any number of handles
        //  outstanding if we were not preceeded by a QUERY.
        //
        //  PnP will take care of disconnecting this stack if we
        //  couldn't get off of it immediately.
        //

        VcbDeleted = FatCheckForDismount( IrpContext, Vcb, TRUE );

    } finally {
        
        //
        //  Release the Vcb if it could still remain.
        //

        if (!VcbDeleted) {

            FatReleaseVcb( IrpContext, Vcb );
        }

        FatReleaseGlobal( IrpContext );
    }

    //
    //  Cleanup our IrpContext and complete the IRP.
    //

    FatCompleteRequest( IrpContext, Irp, Status );

    return Status;
}


NTSTATUS
FatPnpSurpriseRemove (
    PIRP_CONTEXT IrpContext,
    PIRP Irp,
    PVCB Vcb
    )

/*++

Routine Description:

    This routine handles the PnP surprise remove operation.  This is another
    type of notification that the underlying storage device for the volume we
    have is gone, and is excellent indication that the volume will never reappear.
    The filesystem is responsible for initiation or completion the dismount.
    
    For the most part, only "real" drivers care about the distinction of a
    surprise remove, which is a result of our noticing that a user (usually)
    physically reached into the machine and pulled something out.
    
    Surprise will be followed by a Remove when all references have been shut down.

Arguments:

    Irp - Supplies the Irp to process
    
    Vcb - Supplies the volume being removed.

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    KEVENT Event;
    BOOLEAN VcbDeleted;

    PAGED_CODE();

    //
    //  SURPRISE - a device was physically yanked away without
    //  any warning.  This means external forces.
    //
    
    FatAcquireExclusiveVcb( IrpContext, Vcb );
        
    //
    //  We need to pass this down before starting the dismount, which
    //  could disconnect us immediately from the stack.
    //
    
    //
    //  Get the next stack location, and copy over the stack location
    //

    IoCopyCurrentIrpStackLocationToNext( Irp );

    //
    //  Set up the completion routine
    //

    KeInitializeEvent( &Event, NotificationEvent, FALSE );
    IoSetCompletionRoutine( Irp,
                            FatPnpCompletionRoutine,
                            &Event,
                            TRUE,
                            TRUE,
                            TRUE );

    //
    //  Send the request and wait.
    //

    Status = IoCallDriver(Vcb->TargetDeviceObject, Irp);

    if (Status == STATUS_PENDING) {

        KeWaitForSingleObject( &Event,
                               Executive,
                               KernelMode,
                               FALSE,
                               NULL );

        Status = Irp->IoStatus.Status;
    }
    
    try {
        
        //
        //  Knock as many files down for this volume as we can.
        //

        FatFlushAndCleanVolume( IrpContext, Irp, Vcb, NoFlush );

        //
        //  Now make our dismount happen.  This may not vaporize the
        //  Vcb, of course, since there could be any number of handles
        //  outstanding since this is an out of band notification.
        //

        VcbDeleted = FatCheckForDismount( IrpContext, Vcb, TRUE );

    } finally {
        
        //
        //  Release the Vcb if it could still remain.
        //

        if (!VcbDeleted) {

            FatReleaseVcb( IrpContext, Vcb );
        }

        FatReleaseGlobal( IrpContext );
    }
    
    //
    //  Cleanup our IrpContext and complete the IRP.
    //

    FatCompleteRequest( IrpContext, Irp, Status );

    return Status;
}


NTSTATUS
FatPnpCancelRemove (
    PIRP_CONTEXT IrpContext,
    PIRP Irp,
    PVCB Vcb
    )

/*++

Routine Description:

    This routine handles the PnP cancel remove operation.  This is our
    notification that a previously proposed remove (query) was eventually
    vetoed by a component.  The filesystem is responsible for cleaning up
    and getting ready for more IO.
    
Arguments:

    Irp - Supplies the Irp to process
    
    Vcb - Supplies the volume being removed.

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;

    PAGED_CODE();

    //
    //  CANCEL - a previous QUERY has been rescinded as a result
    //  of someone vetoing.  Since PnP cannot figure out who may
    //  have gotten the QUERY (think about it: stacked drivers),
    //  we must expect to deal with getting a CANCEL without having
    //  seen the QUERY.
    //
    //  For FAT, this is quite easy.  In fact, we can't get a
    //  CANCEL if the underlying drivers succeeded the QUERY since
    //  we disconnect the Vpb on our dismount initiation.  This is
    //  actually pretty important because if PnP could get to us
    //  after the disconnect we'd be thoroughly unsynchronized
    //  with respect to the Vcb getting torn apart - merely referencing
    //  the volume device object is insufficient to keep us intact.
    //
    
    FatAcquireExclusiveVcb( IrpContext, Vcb );
    FatReleaseGlobal( IrpContext);
    
    //
    //  Unlock the volume.  This is benign if we never had seen
    //  a QUERY.
    //

    Status = FatUnlockVolumeInternal( IrpContext, Vcb, NULL );

    try {
        
        //
        //  Send the request.  The underlying driver will complete the
        //  IRP.  Since we don't need to be in the way, simply ellide
        //  ourselves out of the IRP stack.
        //

        IoSkipCurrentIrpStackLocation( Irp );

        Status = IoCallDriver(Vcb->TargetDeviceObject, Irp);
    } 
    finally {
        
        FatReleaseVcb( IrpContext, Vcb );
    }

    FatCompleteRequest( IrpContext, NULL, STATUS_SUCCESS );

    return Status;
}


//
//  Local support routine
//

NTSTATUS
FatPnpCompletionRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Contxt
    )
{
    PKEVENT Event = (PKEVENT) Contxt;

    KeSetEvent( Event, 0, FALSE );

    return STATUS_MORE_PROCESSING_REQUIRED;

    UNREFERENCED_PARAMETER( DeviceObject );
    UNREFERENCED_PARAMETER( Contxt );
}


