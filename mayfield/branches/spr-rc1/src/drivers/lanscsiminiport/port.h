 IrpContext,
    IN PVCB Vcb,
    IN BOOLEAN RaiseOnCantWait
    )

/*++

Routine Description:

    This routine acquires exclusive access to the Vcb.

    This routine will raise if it cannot acquire the resource and wait
    in the IrpContext is false.

Arguments:

    Vcb - Supplies the Vcb to acquire

    RaiseOnCantWait - Indicates if we should raise on an acquisition error
        or simply return a BOOLEAN indicating that we couldn't get the
        resource.

Return Value:

    BOOLEAN - Indicates if we were able to acquire the resource.  This is really
        only meaningful if the RaiseOnCantWait value is FALSE.

--*/

{
    ASSERT_IRP_CONTEXT(IrpContext);
    ASSERT_VCB(Vcb);

    PAGED_CODE();

    if (ExAcquireResourceExclusiveLite( &Vcb->SecondaryResource, (BOOLEAN) FlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT))) {

#ifdef NTFSDBG
        if (FlagOn( IrpContext->Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {
            if (1 == ExIsResourceAcquiredSharedLite( &Vcb->SecondaryResource )) {
                NtfsChangeResourceOrderState( IrpContext, NtfsResourceExVcb, FALSE, (BOOLEAN) !FlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );
            }
        }
#endif

        return TRUE;
    }

    if (RaiseOnCantWait) {

        NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
    }

    return FALSE;
}


BOOLEAN
NtfsAcquireSharedSecondaryVcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN BOOLEAN RaiseOnCantWait
    )

/*++

Routine Description:

    This routine acquires shared access to the Vcb.

    This routine will raise if it cannot acquire the resource and wait
    in the IrpContext is false.

Arguments:

    Vcb - Supplies the Vcb to acquire

    RaiseOnCantWait - Indicates if we should raise on an acquisition error
        or simply return a BOOLEAN indicating that we couldn't get the
        resource.

        N.B. -- If you pass FALSE for this parameter you ABSOLUTELY MUST
                test the return value.  Otherwise you aren't certain that
                you hold the Vcb, and you don't know if it's safe to free it.

Return Value:

    None.

--*/

{
    ASSERT_IRP_CONTEXT(IrpContext);
    ASSERT_VCB(Vcb);

    PAGED_CODE();

    if (ExAcquireResourceSharedLite( &Vcb->Resource, (BOOLEAN) FlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT))) {

#ifdef NTFSDBG
        if (FlagOn( IrpContext->Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {
            if (1 == ExIsResourceAcquiredSharedLite( &Vcb->Resource )) {
                NtfsChangeResourceOrderState( IrpContext, NtfsResourceSharedVcb, FALSE, (BOOLEAN) !FlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );
            }
        }
#endif

        return TRUE;
    }

    if (RaiseOnCantWait) {

        NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );

    } else {

        return FALSE;
    }
}


#endif


BOOLEAN
NtfsAcquireExclusiveVcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN BOOLEAN RaiseOnCantWait
    )
/*++

Routine Description:

    This routine acquires exclusive access to the Vcb.

    This routine will raise if it cannot acquire the resource and wait
    in the IrpContext is false.

Arguments:

    Vcb - Supplies the Vcb to acquire

    RaiseOnCantWait - Indicates if we should raise on an acquisition error
        or simply return a BOOLEAN indicating that we couldn't get the
        resource.

Return Value:

    BOOLEAN - Indicates if we were able to acquire the resource.  This is really
        only meaningful if the RaiseOnCantWait value is FALSE.

--*/

{
#ifdef __ND_NTFS_SECONDARY__

	PERESOURCE	resource;

    ASSERT_IRP_CONTEXT(IrpContext);
    ASSERT_VCB(Vcb);

    PAGED_CODE();

	if (FlagOn(IrpContext->NdNtfsFlags, ND_NTFS_IRP_CONTEXT_FLAG_SECONDARY_FILE))
		resource = &Vcb->SecondaryResource;
	else
		resource = &Vcb->Resource;

    if (ExAcquireResourceExclusiveLite( resource, (BOOLEAN) FlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT))) {

#ifdef NTFSDBG
        if (FlagOn( IrpContext->Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {
            if (1 == ExIsResourceAcquiredSharedLite( resource )) {
                NtfsChangeResourceOrderState( IrpContext, NtfsResourceExVcb, FALSE, (BOOLEAN) !FlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );
            }
        }
#endif

        return TRUE;
    }

    if (RaiseOnCantWait) {

        NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
    }

    return FALSE;

#else

    ASSERT_IRP_CONTEXT(IrpContext);
    ASSERT_VCB(Vcb);

    PAGED_CODE();

    if (ExAcquireResourceExclusiveLite( &Vcb->Resource, (BOOLEAN) FlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT))) {

#ifdef NTFSDBG
        if (FlagOn( IrpContext->Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {
            if (1 == ExIsResourceAcquiredSharedLite( &Vcb->Resource )) {
                NtfsChangeResourceOrderState( IrpContext, NtfsResourceExVcb, FALSE, (BOOLEAN) !FlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );
            }
        }
#endif

        return TRUE;
    }

    if (RaiseOnCantWait) {

        NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
    }

    return FALSE;

#endif
}


BOOLEAN
NtfsAcquireSharedVcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN BOOLEAN RaiseOnCantWait
    )
/*++

Routine Description:

    This routine acquires shared access to the Vcb.

    This routine will raise if it cannot acquire the resource and wait
    in the IrpContext is false.

Arguments:

    Vcb - Supplies the Vcb to acquire

    RaiseOnCantWait - Indicates if we should raise on an acquisition error
        or simply return a BOOLEAN indicating that we couldn't get the
        resource.

        N.B. -- If you pass FALSE for this parameter you ABSOLUTELY MUST
                test the return value.  Otherwise you aren't certain that
                you hold the Vcb, and you don't know if it's safe to free it.

Return Value:

    None.

--*/

{

#ifdef __ND_NTFS_SECONDARY__

	PERESOURCE	resource;

    ASSERT_IRP_CONTEXT(IrpContext);
    ASSERT_VCB(Vcb);

    PAGED_CODE();

	if (FlagOn(IrpContext->NdNtfsFlags, ND_NTFS_IRP_CONTEXT_FLAG_SECONDARY_FILE))
		resource = &Vcb->SecondaryResource;
	else
		resource = &Vcb->Resource;

    if (ExAcquireResourceSharedLite( resource, (BOOLEAN) FlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT))) {

#ifdef NTFSDBG
        if (FlagOn( IrpContext->Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {
            if (1 == ExIsResourceAcquiredSharedLite( resource )) {
                NtfsChangeResourceOrderState( IrpContext, NtfsResourceSharedVcb, FALSE, (BOOLEAN) !FlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );
            }
        }
#endif

        return TRUE;
    }

    if (RaiseOnCantWait) {

        NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );

    } else {

        return FALSE;
    }


#else
		
	ASSERT_IRP_CONTEXT(IrpContext);
    ASSERT_VCB(Vcb);

    PAGED_CODE();

    if (ExAcquireResourceSharedLite( &Vcb->Resource, (BOOLEAN) FlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT))) {

#ifdef NTFSDBG
        if (FlagOn( IrpContext->Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {
            if (1 == ExIsResourceAcquiredSharedLite( &Vcb->Resource )) {
                NtfsChangeResourceOrderState( IrpContext, NtfsResourceSharedVcb, FALSE, (BOOLEAN) !FlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );
            }
        }
#endif

        return TRUE;
    }

    if (RaiseOnCantWait) {

        NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );

    } else {

        return FALSE;
    }

#endif
}


#ifdef NTFSDBG

#ifdef __ND_NTFS_SECONDARY__

VOID
NtfsReleaseVcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )
/*++

Routine Description:

    This routine will release the Vcb. Normally its a define for lock_order testing
    we use a function so we can easily change the owernship state

Arguments:

    Vcb - Supplies the Vcb to release


Return Value:

    None.
--*/

{
	PERESOURCE	resource;

	ASSERT_IRP_CONTEXT(IrpContext);
	ASSERT_VCB(Vcb);

	PAGED_CODE();

	if (FlagOn(IrpContext->NdNtfsFlags, ND_NTFS_IRP_CONTEXT_FLAG_SECONDARY_FILE))
		resource = &Vcb->SecondaryResource;
	else
		resource = &Vcb->Resource;

	if (FlagOn( IrpContext->Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {
        if ((ExIsResourceAcquiredExclusiveLite( resource)) &&
            (1 == ExIsResourceAcquiredSharedLite( resource ))) {
            NtfsChangeResourceOrderState( IrpContext, NtfsResourceExVcb, TRUE, FALSE );
        } else if (1 == ExIsResourceAcquiredSharedLite( resource )) {
            NtfsChangeResourceOrderState( IrpContext, NtfsResourceSharedVcb, TRUE, FALSE );
        }
    } else {
        IrpContext->OwnershipState = None;
    }

    ExReleaseResourceLite( resource );                    
}

#else


VOID
NtfsReleaseVcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )
/*++

Routine Description:

    This routine will release the Vcb. Normally its a define for lock_order testing
    we use a function so we can easily change the owernship state

Arguments:

    Vcb - Supplies the Vcb to release


Return Value:

    None.
--*/

{
    if (FlagOn( IrpContext->Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {
        if ((ExIsResourceAcquiredExclusiveLite( &Vcb->Resource)) &&
            (1 == ExIsResourceAcquiredSharedLite( &Vcb->Resource ))) {
            NtfsChangeResourceOrderState( IrpContext, NtfsResourceExVcb, TRUE, FALSE );
        } else if (1 == ExIsResourceAcquiredSharedLite( &Vcb->Resource )) {
            NtfsChangeResourceOrderState( IrpContext, NtfsResourceSharedVcb, TRUE, FALSE );
        }
    } else {
        IrpContext->OwnershipState = None;
    }
    ExReleaseResourceLite( &Vcb->Resource );                    
}

#endif

#endif


VOID
NtfsReleaseVcbCheckDelete (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN UCHAR MajorCode,
    IN PFILE_OBJECT FileObject OPTIONAL
    )

/*++

Routine Description:

    This routine will release the Vcb.  We will also test here whether we should
    teardown the Vcb at this point.  If this is the last open queued to a dismounted
    volume or the last close from a failed mount or the failed mount then we will
    want to test the Vcb for a teardown.

Arguments:

    Vcb - Supplies the Vcb to release

    MajorCode - Indicates what type of operation we were called from.

    FileObject - Optionally supplies the file object whose VPB pointer we need to
        zero out

Return Value:

    None.

--*/

{
    BOOLEAN ReleaseVcb = TRUE;

    ASSERT_IRP_CONTEXT(IrpContext);
    ASSERT_VCB(Vcb);

#ifdef __ND_NTFS_SECONDARY__

	if (FlagOn(IrpContext->NdNtfsFlags, ND_NTFS_IRP_CONTEXT_FLAG_SECONDARY_FILE)) {
	
#ifdef NTFSDBG

		if (FlagOn( IrpContext->Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {
		    if ((ExIsResourceAcquiredExclusiveLite( &Vcb->SecondaryResource )) &&
			    (1 == ExIsResourceAcquiredSharedLite( &Vcb->SecondaryResource ))) {
				NtfsChangeResourceOrderState( IrpContext, NtfsResourceExVcb, TRUE, FALSE );
	        } else if (1 == ExIsResourceAcquiredSharedLite( resource )) {
		        NtfsChangeResourceOrderState( IrpContext, NtfsResourceSharedVcb, TRUE, FALSE );
			}
	    } else {
		    IrpContext->OwnershipState = None;
		}
#endif

		ExReleaseResourceLite( &Vcb->SecondaryResource );
		return;
	}

#endif

    if (FlagOn( Vcb->VcbState, VCB_STATE_PERFORMED_DISMOUNT ) &&
        (Vcb->CloseCount == 0)) {

        ULONG ReferenceCount;
        ULONG ResidualCount;

        KIRQL SavedIrql;
        BOOLEAN DeleteVcb = FALSE;

        ASSERT_EXCLUSIVE_RESOURCE( &Vcb->Resource );

        //
        //  The volume has gone through dismount.  Now we need to decide if this
        //  release of the Vcb is the last reference for this volume.  If so we
        //  can tear the volume down.
        //
        //  We compare the reference count in the Vpb with the state of the volume
        //  and the type of operation.  We also need to check if there is a
        //  referenced log file object.
        //
        //  If the temp vpb flag isn't set then we already let the iosubsys
        //  delete it during dismount
        //

        if (FlagOn( Vcb->VcbState, VCB_STATE_TEMP_VPB )) {

            IoAcquireVpbSpinLock( &SavedIrql );
            ReferenceCount = Vcb->Vpb->ReferenceCount;
            IoReleaseVpbSpinLock( SavedIrql );

        } else {

            ReferenceCount = 0;
        }


        ResidualCount = 0;

        if ((Vcb->LogFileObject != NULL) &&
            !FlagOn( Vcb->CheckpointFlags, VCB_DEREFERENCED_LOG_FILE )) {

            ResidualCount = 1;
        }

        if (MajorCode == IRP_MJ_CREATE) {

            ResidualCount += 1;
        }

        //
        //  If the residual count is the same as the count in the Vpb then we
        //  can delete the Vpb.
        //

        if ((ResidualCount == ReferenceCount) &&
            !FlagOn( Vcb->VcbState, VCB_STATE_DELETE_UNDERWAY )) {

            SetFlag( Vcb->VcbState, VCB_STATE_DELETE_UNDERWAY );

            //
            //  Release the vcb before we grab the global
            //  

#ifdef NTFSDBG
            if (FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {
                if (1 == ExIsResourceAcquiredExclusiveLite( &Vcb->Resource) ) {
                    NtfsChangeResourceOrderState( IrpContext, NtfsResourceExVcb, TRUE, FALSE );
                }
            }
#endif
            ExReleaseResourceLite( &Vcb->Resource );
            ReleaseVcb = FALSE;

            //
            //  Never delete the Vcb unless this is the last release of
            //  this Vcb.
            //

            if (ExIsResourceAcquiredSharedLite( &Vcb->Resource ) ==  0) {

                if (ARGUMENT_PRESENT(FileObject)) { FileObject->Vpb = NULL; }

                //
                //  If this is a create then the IO system will handle the
                //  Vpb.
                //

                if (MajorCode == IRP_MJ_CREATE) {

                    ClearFlag( Vcb->VcbState, VCB_STATE_TEMP_VPB );
                }

                //
                //  Use the global resource to synchronize the DeleteVcb process.
                //

                NtfsAcquireExclusiveGlobal( IrpContext, TRUE );
                RemoveEntryList( &Vcb->VcbLinks );
                NtfsReleaseGlobal( IrpContext );
                
                //
                //  Try to delete the Vcb, reinsert into the queue if
                //  the delete is blocked.
                //

                if (!NtfsDeleteVcb( IrpContext, &Vcb )) {

                    ClearFlag( Vcb->VcbState, VCB_STATE_DELETE_UNDERWAY );

                    NtfsAcquireExclusiveGlobal( IrpContext, TRUE );
                    InsertHeadList( &NtfsData.VcbQueue, &Vcb->VcbLinks );
                    NtfsReleaseGlobal( IrpContext );
                } 
            } else {

                //
                //  From test above we must still own the vcb so its safe to change the flag
                //  
                
                ClearFlag( Vcb->VcbState, VCB_STATE_DELETE_UNDERWAY );   
            }
        } 
    } 

    if (ReleaseVcb) {

#ifdef NTFSDBG
        if (FlagOn( Vcb->VcbState, VCB_STATE_VOLUME_MOUNTED )) {
            if (1 == ExIsResourceAcquiredExclusiveLite( &Vcb->Resource) ) {
                NtfsChangeResourceOrderState( IrpContext, NtfsResourceExVcb, TRUE, FALSE );
            }
        }
#endif
        ExReleaseResourceLite( &Vcb->Resource );
    }
}


BOOLEAN
NtfsAcquireFcbWithPaging (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN ULONG AcquireFlags
    )

/*++

Routine Description:

    This routine is used in the create path, fsctl path and close path .  It acquires the Fcb
    and also the paging IO resource if it exists but only if the irpcontext flag is set.
    i.e during a create  supersede/overwrite operation.

    This routine will raise if it cannot acquire the resource and wait
    in the IrpContext is false.

Arguments:

    Fcb - Supplies the Fcb to acquire

    AcquireFlags - ACQUIRE_DONT_WAIT overrides the wait value in the IrpContext.
        We won't wait for the resource and return whether the resource
        was acquired.

Return Value:

    BOOLEAN - TRUE if acquired.  FALSE otherwise.

--*/

{
    BOOLEAN Status = FALSE;
    BOOLEAN Wait = FALSE;
    BOOLEAN PagingIoAcquired = FALSE;

    ASSERT_IRP_CONTEXT(IrpContext);
    ASSERT_FCB(Fcb);

    PAGED_CODE();

    //
    //  Sanity check that this is create.  The supersede flag is only
    //  set in the create path and only tested here.
    //

    ASSERT( IrpContext->MajorFunction == IRP_MJ_CREATE ||
            IrpContext->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL ||
            IrpContext->MajorFunction == IRP_MJ_CLOSE ||
            IrpContext->MajorFunction == IRP_MJ_SET_INFORMATION ||
            IrpContext->MajorFunction == IRP_MJ_SET_VOLUME_INFORMATION ||
            IrpContext->MajorFunction == IRP_MJ_SET_EA );

    if (!FlagOn( AcquireFlags, ACQUIRE_DONT_WAIT ) && FlagOn( IrpContext->State, IRP_CONTEXT_STATE_WAIT )) {

        Wait = TRUE;
    }

    //
    //  Free any exclusive paging I/O resource, we currently have, which
    //  must presumably be from a directory with a paging I/O resource.
    //
    //  We defer releasing the paging io resource when we have logged
    //  changes against a stream.  The only transaction that should be
    //  underway at this point is the create file case where we allocated
    //  a file record.  In this case it is OK to release the paging io
    //  resource for the parent.
    //

    if (IrpContext->CleanupStructure != NULL) {

        ASSERT( IrpContext->CleanupStructure != Fcb );

        //  ASSERT(IrpContext->TransactionId == 0);
        NtfsReleasePagingIo( IrpContext, IrpContext->CleanupStructure );
    }

    //
    //  Loop until we get it right - worst case is twice through loop.
    //

    while (TRUE) {

        //
        //  Acquire Paging I/O first.  Testing for the PagingIoResource
        //  is not really safe without holding the main resource, so we
        //  correct for that below.
        //

        if (FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_PAGING ) &&
            (Fcb->PagingIoResource != NULL)) {
            if (!ExAcquireResourceExclusiveLite( Fcb->PagingIoResource, Wait )) {
                break;
            }
            IrpContext->CleanupStructure = Fcb;
            PagingIoAcquired = TRUE;
        }

        //
        //  Let's acquire this Fcb exclusively.
        //

        if (!NtfsAcquireExclusiveFcb( IrpContext, Fcb, NULL, ACQUIRE_NO_DELETE_CHECK | AcquireFlags )) {

            if (PagingIoAcquired) {
                ASSERT(IrpContext->TransactionId == 0);
                NtfsReleasePagingIo( IrpContext, Fcb );
            }
            break;
        }

        //
        //  If we now do not see a paging I/O resource we are golden,
        //  othewise we can absolutely release and acquire the resources
        //  safely in the right order, since a resource in the Fcb is
        //  not going to go away.
        //

        if (!FlagOn( IrpContext->Flags, IRP_CONTEXT_FLAG_ACQUIRE_PAGING ) ||
            PagingIoAcquired ||
            (Fcb->PagingIoResource == NULL)) {

            Status = TRUE;
            break;
        }

        NtfsReleaseFcb( IrpContext, Fcb );
    }

    return Status;
}


VOID
NtfsReleaseFcbWithPaging (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb
    )

/*++

Routine Description:

    This routine releases access to the Fcb, including its
    paging I/O resource if it exists.

Arguments:

    Fcb - Supplies the Fcb to acquire

Return Value:

    None.;

    //
    // Error log information.
    //

    ERROR_LOG_ENTRY  LogEntry;

    //
    // Logical unit to start next.
    //

    PLOGICAL_UNIT_EXTENSION ReadyLogicalUnit;

    //
    // List of completed abort reqeusts.
    //

    PLOGICAL_UNIT_EXTENSION CompletedAbort;

    //
    // Miniport timer request routine.
    //

    PHW_INTERRUPT HwTimerRequest;

    //
    // Mini port timer request time in micro seconds.
    //

    ULONG MiniportTimerValue;

    //
    // Queued WMI request items.
    //

    PWMI_MINIPORT_REQUEST_ITEM WmiMiniPortRequests;

} INTERRUPT_DATA, *PINTERRUPT_DATA;

typedef struct {
    ULONG SparseLun;
    ULONG OneLun;
    ULONG LargeLuns;
} SP_SPECIAL_CONTROLLER_FLAGS, *PSP_SPECIAL_CONTROLLER_FLAGS;

typedef struct _CONFIGURATION_CONTEXT {
    BOOLEAN DisableTaggedQueueing;
    BOOLEAN DisableMultipleLu;
    ULONG AdapterNumber;
    ULONG BusNumber;
    PVOID Parameter;
    PACCESS_RANGE AccessRanges;
    UNICODE_STRING RegistryPath;
    PORT_CONFIGURATION_INFORMATION PortConfig;
}CONFIGURATION_CONTEXT, *PCONFIGURATION_CONTEXT;

typedef struct _DEVICE_MAP_HANDLES {
    HANDLE BusKey;
    HANDLE InitiatorKey;
} DEVICE_MAP_HANDLES, *PDEVICE_MAP_HANDLES;

typedef struct _COMMON_EXTENSION {

    //
    // Back pointer to the device object
    //

    PDEVICE_OBJECT DeviceObject;

    struct {

        //
        // True if this device object is a physical device object
        //

        BOOLEAN IsPdo : 1;

        //
        // True if this device object has processed it's first start and
        // has been initialized.
        //

        BOOLEAN IsInitialized : 1;

        //
        // Has WMI been initialized for this device object?
        //

        BOOLEAN WmiInitialized : 1;

        //
        // Has the miniport associated with this FDO or PDO indicated WMI 
        // support?
        //

        BOOLEAN WmiMiniPortSupport : 1;

    };

    //
    // Current plug and play state or 0xff if no state operations have been 
    // sent yet.
    //

    UCHAR CurrentPnpState;

    //
    // Previous plug and play state or 0xff if there is no requirement that we
    // be able to roll back in the current state (current state is not a query)
    //

    UCHAR PreviousPnpState;

    //
    // Interlocked counter indicating that the device has been removed.
    //

    ULONG IsRemoved;


    //
    // Pointer to the device object this is on top of
    //

    PDEVICE_OBJECT LowerDeviceObject;

    //
    // Srb flags to OR into all SRBs coming through this device object.
    //

    ULONG SrbFlags;

    //
    // Pointer to the dispatch table for this object
    //

    PDRIVER_DISPATCH *MajorFunction;


    //
    // Current and desired power state for this device and the system.
    //

    SYSTEM_POWER_STATE CurrentSystemState;

    DEVICE_POWER_STATE CurrentDeviceState;
    DEVICE_POWER_STATE DesiredDeviceState;

    //
    // Idle timer for this device
    //

    PULONG IdleTimer;

    //
    // Pointer to the SCSIPORT-provided WMIREGINFO structures registered on
    // behalf of the miniport for this device object.  Size is the size of the
    // entire WMIREGINFO buffer in bytes.
    //

    PWMIREGINFO WmiScsiPortRegInfoBuf;
    ULONG       WmiScsiPortRegInfoBufSize;

    //
    // INTERLOCKED counter of the number of consumers of this device object.
    // When this count goes to zero the RemoveEvent will be set.
    //

    //
    // This variable is only manipulated by SpAcquireRemoveLock and
    // SpReleaseRemoveLock.
    //

    LONG RemoveLock;

    //
    // This event will be signalled when it is safe to remove the device object
    //

    KEVENT RemoveEvent;

    //
    // The spinlock and the list are only used in checked builds to track who
    // has acquired the remove lock.  Free systems will leave these initialized
    // to 0xff (they are still in the structure to make debugging easier)
    //

    KSPIN_LOCK RemoveTrackingSpinlock;

    PVOID RemoveTrackingList;

    LONG RemoveTrackingUntrackedCount;

    NPAGED_LOOKASIDE_LIST RemoveTrackingLookasideList;

    BOOLEAN RemoveTrackingLookasideListInitialized;

    //
    // Count of different services this device is being used for (ala
    // IRP_MN_DEVICE_USAGE_NOTIFICATION)
    //

    ULONG PagingPathCount;
    ULONG HibernatePathCount;
    ULONG DumpPathCount;

} COMMON_EXTENSION, *PCOMMON_EXTENSION;

struct _ADAPTER_EXTENSION {

    union {
        PDEVICE_OBJECT DeviceObject;
        COMMON_EXTENSION CommonExtension;
    };

    //
    // Pointer to the PDO we attached to - necessary for PnP routines
    //

    PDEVICE_OBJECT LowerPdo;

#if TEST_LISTS

    //
    // Some simple performance counters to determine how often we use the
    // small vs. medium vs. large scatter gather lists.
    //

    ULONGLONG ScatterGatherAllocationCount;

    //
    // Counters used to calculate the average size of a small medium and
    // large allocation.  There are two values for each counter - a total
    // count and an overflow count.  The total count will be right-shifted one
    // bit if it overflows on an increment.  When this happens the overflow
    // count will also be incremented.  This count is used to adjust the
    // allocation count when determining averages.
    //

    ULONGLONG SmallAllocationSize;
    ULONGLONG MediumAllocationSize;
    ULONGLONG LargeAllocationSize;

    ULONG SmallAllocationCount;
    ULONG LargeAllocationCount;

    //
    // Counters to determine how often we can service a request off the
    // srb data list, how often we need to queue a request and how often
    // we can resurrect a free'd srb data to service something off the queue.
    //

    INTERLOCKED ULONGLONG SrbDataAllocationCount;
    INTERLOCKED ULONGLONG SrbDataQueueInsertionCount;
    INTERLOCKED ULONGLONG SrbDataEmergencyFreeCount;
    INTERLOCKED ULONGLONG SrbDataServicedFromTickHandlerCount;
    INTERLOCKED ULONGLONG SrbDataResurrectionCount;

#endif

    //
    // Device extension for miniport routines.
    //

    PVOID HwDeviceExtension;

    //
    // Miniport noncached device extension
    //

    PVOID NonCachedExtension;
    ULONG PortNumber;

    ULONG AdapterNumber;

    //
    // Active requests count.  This count is biased by -1 so a value of -1
    // indicates there are no requests out standing.
    //

    LONG ActiveRequestCount;

    //
    // Binary Flags
    //

    typedef struct {

        //
        // Did pnp or the port driver detect this device and provide resources
        // to the miniport, or did the miniport detect the device for us.  This
        // flag also indicates whether the AllocatedResources list is non-null
        // going into the find adapter routine.
        //

        BOOLEAN IsMiniportDetected : 1;

        //
        // Do we need to virtualize this adapter and make it look like the only
        // adapter on it's own bus?
        //

        BOOLEAN IsInVirtualSlot : 1;

        //
        // Is this a pnp adapter?
        //

        BOOLEAN IsPnp : 1;

        //
        // Does this device have an interrupt connected?
        //

        BOOLEAN HasInterrupt : 1;

        //
        // Can this device be powered off?
        //

        BOOLEAN DisablePower : 1;

        //
        // Can this device be stopped?
        //

        BOOLEAN DisableStop : 1;
    };

    //
    // For most virtual slot devices this will be zero.  However for some 
    // the real slot/function number is needed by the miniport to access 
    // hardware shared by multiple slots/functions.
    //

    PCI_SLOT_NUMBER VirtualSlotNumber;

    //
    // The bus and slot number of this device as returned by the PCI driver.
    // This is used when building the ConfigInfo block for crashdump so that
    // the dump drivers can talk directly with the hal.  These are only
    // valid if IsInVirtualSlot is TRUE above.
    //

    ULONG RealBusNumber;

    ULONG RealSlotNumber;

    //
    // Number of SCSI buses
    //

    UCHAR NumberOfBuses;
    UCHAR MaximumTargetIds;
    UCHAR MaxLuCount;

    //
    // SCSI port driver flags
    //

    ULONG Flags;

    INTERLOCKED ULONG DpcFlags;

    //
    // The number of times this adapter has been disabled.
    //

    ULONG DisableCount;

    LONG PortTimeoutCounter;

    //
    // A pointer to the interrupt object to be used with
    // the SynchronizeExecution routine.  If the miniport is
    // using SpSynchronizeExecution then this will actually point
    // back to the adapter extension.
    //

    PKINTERRUPT InterruptObject;

    //
    // Second Interrupt object (PCI IDE work-around)
    //

    PKINTERRUPT InterruptObject2;

    //
    // Routine to call to synchronize execution for the miniport.
    //

    PSYNCHRONIZE_ROUTINE  SynchronizeExecution;

    //
    // Global device sequence number.
    //

    ULONG SequenceNumber;
    KSPIN_LOCK SpinLock;

    //
    // Second spin lock (PCI IDE work-around).  This is only initalized
    // if the miniport has requested multiple interrupts.
    //

    KSPIN_LOCK MultipleIrqSpinLock;

    //
    // Dummy interrupt spin lock.
    //

    KSPIN_LOCK InterruptSpinLock;

    //
    // Dma Adapter information.
    //

    PVOID MapRegisterBase;
    PADAPTER_OBJECT DmaAdapterObject;
    ADAPTER_TRANSFER FlushAdapterParameters;

    //
    // miniport's copy of the configuraiton informaiton.
    // Used only during initialization.
    //

    PPORT_CONFIGURATION_INFORMATION PortConfig;

    //
    // Resources allocated and translated for this particular adapter.
    //

    PCM_RESOURCE_LIST AllocatedResources;

    PCM_RESOURCE_LIST TranslatedResources;

    //
    // Common buffer size.  Used for HalFreeCommonBuffer.
    //

    ULONG CommonBufferSize;
    ULONG SrbExtensionSize;

    //
    // Indicates whether the common buffer was allocated using
    // ALLOCATE_COMMON_BUFFER or MmAllocateContiguousMemorySpecifyCache.
    //

    BOOLEAN UncachedExtensionIsCommonBuffer;

    //
    // The number of srb extensions which were allocated.
    //

    ULONG SrbExtensionCount;

    //
    // Placeholder for the minimum number of requests to allocate for.
    // This can be a registry parameter.
    //

    ULONG NumberOfRequests;

    //
    // SrbExtension and non-cached common buffer
    //

    PVOID SrbExtensionBuffer;

    //
    // List head of free SRB extentions.
    //

    PVOID SrbExtensionListHeader;

    //
    // A bitmap for keeping track of which queue tags are in use.
    //

    KSPIN_LOCK QueueTagSpinLock;
    PRTL_BITMAP QueueTagBitMap;

    UCHAR MaxQueueTag;

    //
    // Hint for allocating queue tags.  Value will be the last queue
    // tag allocated + 1.
    //

    ULONG QueueTagHint;

    //
    // Logical Unit Extensions
    //

    ULONG HwLogicalUnitExtensionSize;

    //
    // Array of mapped address entries for use when powering up the adapter
    // or cleaning up its mappings.
    //

    ULONG MappedAddressCount;
    PMAPPED_ADDRESS MappedAddressList;

    //
    // Miniport service routine pointers.
    //

    PHW_FIND_ADAPTER HwFindAdapter;
    PHW_INITIALIZE HwInitialize;
    PHW_STARTIO HwStartIo;
    PHW_INTERRUPT HwInterrupt;
    PHW_RESET_BUS HwResetBus;
    PHW_DMA_STARTED HwDmaStarted;
    PHW_INTERRUPT HwRequestInterrupt;
    PHW_INTERRUPT HwTimerRequest;
    PHW_ADAPTER_CONTROL HwAdapterControl;

    ULONG InterruptLevel;
    ULONG IoAddress;

    //
    // BitMap containing the list of supported adapter control types for this
    // adapter/miniport.
    //

    RTL_BITMAP SupportedControlBitMap;
    ULONG SupportedControlBits[ARRAY_ELEMENTS_FOR_BITMAP(
                                    (ScsiAdapterControlMax),
                                    ULONG)];

    //
    // Array of logical unit extensions.
    //

    LOGICAL_UNIT_BIN LogicalUnitList[NUMBER_LOGICAL_UNIT_BINS];

    //
    // The last logical unit for which the miniport completed a request.  This
    // will give us a chance to stay out of the LogicalUnitList for the common
    // completion type.
    // 
    // This value is set by ScsiPortNotification and will be cleared by 
    // SpRemoveLogicalUnitFromBin.
    //

    PLOGICAL_UNIT_EXTENSION CachedLogicalUnit;

    //
    // Interrupt level data storage.
    //

    INTERRUPT_DATA InterruptData;

    //
    // Whether or not an interrupt has occured since the last timeout.
    // Used to determine if interrupts may not be getting delivered.
    // This value must be set within KeSynchronizeExecution
    //

    ULONG WatchdogInterruptCount;

    //
    // SCSI Capabilities structure
    //

    IO_SCSI_CAPABILITIES Capabilities;

    //
    // Miniport timer object.
    //

    KTIMER MiniPortTimer;

    //
    // Miniport DPC for timer object.
    //

    KDPC MiniPortTimerDpc;

    //
    // Physical address of common buffer
    //

    PHYSICAL_ADDRESS PhysicalCommonBuffer;

    //
    // Buffers must be mapped into system space.
    //

    BOOLEAN MapBuffers;

    //
    // Buffers must be remapped into system space after IoMapTransfer has been 
    // called.
    //

    BOOLEAN RemapBuffers;

    //
    // Is this device a bus master and does it require map registers.
    //

    BOOLEAN MasterWithAdapter;

    //
    // Supports tagged queuing
    //

    BOOLEAN TaggedQueuing;

    //
    // Supports auto request sense.
    //

    BOOLEAN AutoRequestSense;

    //
    // Supports multiple requests per logical unit.
    //

    BOOLEAN MultipleRequestPerLu;

    //
    // Support receive event function.
    //

    BOOLEAN ReceiveEvent;

    //
    // Indicates an srb extension needs to be allocated.
    //

    BOOLEAN AllocateSrbExtension;

    //
    // Indicates the contorller caches data.
    //

    BOOLEAN CachesData;

    //
    // Indicates that the adapter can handle 64-bit DMA.
    //

    BOOLEAN Dma64BitAddresses;

    //
    // Indicates that the adapter can handle 32-bit DMA.
    //

    BOOLEAN Dma32BitAddresses;

    //
    // Queued WMI request items that are not in use.
    //
    INTERLOCKED SLIST_HEADER    WmiFreeMiniPortRequestList;
    KSPIN_LOCK                  WmiFreeMiniPortRequestLock;
    INTERLOCKED ULONG           WmiFreeMiniPortRequestWatermark;
    INTERLOCKED ULONG           WmiFreeMiniPortRequestCount;
    BOOLEAN                     WmiFreeMiniPortRequestInitialized;

    //
    // Free WMI request items were exhausted at least once in the lifetime
    // of this adapter (used to log error only once).
    //

    BOOLEAN                    WmiFreeMiniPortRequestsExhausted;


    //
    // Event used to synchronize enumeration requests from non-pnp and pnp
    // callers.
    //

    KEVENT EnumerationSynchronization;

    //
    // System time of the last bus scan.
    //

    LARGE_INTEGER LastBusScanTime;

    //
    // Indicates that the next rescan which comes in should be "forced", ie.
    // it should rescan no matter how recent the last one was.
    //

    INTERLOCKED LONG ForceNextBusScan;

    //
    // A lookaside list to pull SRB_DATA blocks off of.
    //

    NPAGED_LOOKASIDE_LIST SrbDataLookasideList;

    //
    // The following members are used to keep an SRB_DATA structure allocated
    // for emergency use and to queue requests which need to use it.  The
    // structures are synchronized with the EmergencySrbDataSpinLock.
    // The routines Sp[Allocate|Free]SrbData & ScsiPortTickHandler will
    // handle queueing and eventual restarting of these requests.
    //

    //
    // This spinlock protects the blocked request list.
    //

    KSPIN_LOCK EmergencySrbDataSpinLock;

    //
    // Contains a queue of irps which could not be dispatched because of
    // low memory conditions and because the EmergencySrbData block is already
    // allocated.
    //

    LIST_ENTRY SrbDataBlockedRequests;

    //
    // The SRB_DATA reserved for "emergency" use.  This pointer should be set
    // to NULL if the SRB_DATA is in use.  Any SRB_DATA block may be used
    // for the emergency request.
    //

    INTERLOCKED PSRB_DATA EmergencySrbData;

    //
    // Flags to indicate whether the srbdata and scatter gather lookaside
    // lists have been allocated already.
    //

    BOOLEAN SrbDataListInitialized;

    BOOLEAN MediumScatterGatherListInitialized;

    //
    // Sizes for small, medium and large scatter gather lists.  This is the
    // number of entries in the list, not the number of bytes.
    //

    UCHAR LargeScatterGatherListSize;

    //
    // Lookaside list for medium scatter gather lists.  Medium lists are used
    // to service anything between a small and large number of physical
    // breaks.
    //

    NPAGED_LOOKASIDE_LIST MediumScatterGatherLookasideList;

    //
    // Bus standard interface.  Retrieved from the lower driver immediately
    // after it completes the start irp.
    //

    BOOLEAN LowerBusInterfaceStandardRetrieved;
    BUS_INTERFACE_STANDARD LowerBusInterfaceStandard;

    //
    // Handles into the device map for the various entries this adapter will 
    // have created.
    //

    //
    // An array of handles for each

    HANDLE PortDeviceMapKey;

    PDEVICE_MAP_HANDLES BusDeviceMapKeys;

    //
    // Unicode string containing the device name of this object
    //

    PWSTR DeviceName;

    //
    // The guid for the underlying bus.  Saved here so we don't have to 
    // retrieve it so often.
    //

    GUID BusTypeGuid;

    //
    // The pnp interface name for this device.
    //

    UNICODE_STRING InterfaceName;

    //
    // The device state for this adapter.
    //

    PNP_DEVICE_STATE DeviceState;

    //
    // The number of calls to ScsiPortTickHandler for this adapter since 
    // the machine was booted.
    //

    INTERLOCKED ULONG TickCount;

    //
    // Preallocated memory to use for an inquiry command.
    //

    PINQUIRYDATA InquiryBuffer;
    PSENSE_DATA InquirySenseBuffer;
    PIRP InquiryIrp;
    PMDL InquiryMdl;

    //
    // Mutex used to synchronize multiple threads all synchronously waiting for
    // a power up to occur.
    //

    FAST_MUTEX PowerMutex;

};

struct _LOGICAL_UNIT_EXTENSION {

    union {
        PDEVICE_OBJECT DeviceObject;
        COMMON_EXTENSION CommonExtension;
    };

    //
    // Logical Unit flags
    //

    ULONG LuFlags;

    //
    // The adapter number this device is attached to
    //

    ULONG PortNumber;

    //
    // Has this device been claimed by a driver (legacy or pnp)
    //

    BOOLEAN IsClaimed;

    BOOLEAN IsLegacyClaim;

    //
    // Has this device been enumerated yet?  If so then we cannot actually 
    // delete it until we've explicitly told the PNP system that it's gone
    // (by not enumerating it)
    //

    BOOLEAN IsEnumerated;

    //
    // Has this device gone missing?
    //

    BOOLEAN IsMissing;

    //
    // Is this device visible - should it be exposed to PNP?
    //

    BOOLEAN IsVisible;

    //
    // Was this device marked missing because we found something different at 
    // it's bus location?  If so then the removal of this device from the 
    // logical unit bins will trigger a new bus scan.
    //

    BOOLEAN IsMismatched;

    //
    // The bus address of this device.
    //

    UCHAR PathId;
    UCHAR TargetId;
    UCHAR Lun;

    //
    // The number of times the current busy request has been retried
    //

    UCHAR RetryCount;

    //
    // The current queue sort key
    //

    ULONG CurrentKey;

    //
    // A pointer to the miniport's logical unit extension.
    //

    PVOID HwLogicalUnitExtension;

    //
    // A pointer to the device extension for the adapter.
    //

    PADAPTER_EXTENSION AdapterExtension;

    //
    // The number of unreleased queue locks on this device
    //

    ULONG QueueLockCount;

    //
    // Reference counts for pausing & unpausing the queue (see LU_QUEUE_PAUSED)
    //

    ULONG QueuePauseCount;

    //
    // The current lock request for this LUN.  Only one lock request can be 
    // outstanding at any given time.
    //

    PSCSI_REQUEST_BLOCK LockRequest;

    //
    // A pointer to the next logical unit extension in the logical unit bin.
    //

    PLOGICAL_UNIT_EXTENSION NextLogicalUnit;

    //
    // Used to chain logical units in the interrupt data block.
    //

    PLOGICAL_UNIT_EXTENSION ReadyLogicalUnit;

    //
    // Used to chain completed abort requests in the interrupt data block.
    //

    PLOGICAL_UNIT_EXTENSION CompletedAbort;

    //
    // The current abort request for this logical unit
    //

    PSCSI_REQUEST_BLOCK AbortSrb;

    //
    // Timeout counter for this logical unit
    //

    LONG RequestTimeoutCounter;

    //
    // The list of requests for this logical unit.
    //

    LIST_ENTRY RequestList;

    //
    // The next request to be executed.
    //

    PSRB_DATA PendingRequest;

    //
    // This irp could not be executed before because the
    // device returned BUSY.
    //

    PSRB_DATA BusyRequest;

    //
    // The current untagged request for this logical unit.
    //

    PSRB_DATA CurrentUntaggedRequest;

    //
    // The maximum number of request which we will issue to the device
    //

    UCHAR MaxQueueDepth;

    //
    // The current number of outstanding requests.
    //

    UCHAR QueueCount;

    //
    // The inquiry data for this request.
    //

    INQUIRYDATA InquiryData;

    //
    // The special controller flags for this device (if any)
    //

    SP_SPECIAL_CONTROLLER_FLAGS SpecialFlags;

    //
    // The handles for the target & logical unit keys in the device map.
    //

    HANDLE TargetDeviceMapKey;
    HANDLE LunDeviceMapKey;

    //
    // Our fixed set of SRB_DATA blocks for use when processing bypass requests.
    // If this set is exhausted then scsiport will bugcheck - this should be 
    // okay since bypass requests are only sent in certain extreme conditions
    // and should never be overlapped (we should only see one bypass request 
    // at a time).
    //

    SRB_DATA BypassSrbDataBlocks[NUMBER_BYPASS_SRB_DATA_BLOCKS];

    //
    // A list of the free bypass SRB_DATA blocks.
    //

    KSPIN_LOCK BypassSrbDataSpinLock;
    SLIST_HEADER BypassSrbDataList;

    //
    // A spinlock to protect the following two fields.
    //

    KSPIN_LOCK RequestSenseLock;

    //
    // A pointer to the request for which we have issued a request-sense irp
    // (if any).
    //

    PSRB_DATA ActiveFailedRequest;

    //
    // A pointer to the request for which we need to issue a request-sense irp 
    // (if any).  RequestSenseCompletion will promote this to the active 
    // failed request and issue a new RS operation when it runs.
    //

    PSRB_DATA BlockedFailedRequest;

    //
    // Resources for issuing request-sense commands.
    //

    PIRP RequestSenseIrp;
    SCSI_REQUEST_BLOCK RequestSenseSrb;

    struct {
        MDL RequestSenseMdl;
        PFN_NUMBER RequestSenseMdlPfn1;
        PFN_NUMBER RequestSenseMdlPfn2;
    };

};

//
// Miniport specific device extension wrapper
//

struct _HW_DEVICE_EXTENSION {
    PADAPTER_EXTENSION FdoExtension;
    UCHAR HwDeviceExtension[0];
};

typedef struct _INTERRUPT_CONTEXT {
    PADAPTER_EXTENSION DeviceExtension;
    PINTERRUPT_DATA SavedInterruptData;
}INTERRUPT_CONTEXT, *PINTERRUPT_CONTEXT;

typedef struct _RESET_CONTEXT {
    PADAPTER_EXTENSION DeviceExtension;
    UCHAR PathId;
}RESET_CONTEXT, *PRESET_CONTEXT;

//
// Used in LUN rescan determination.
//

typedef struct _UNICODE_LUN_LIST {
    UCHAR TargetId;
    struct _UNICODE_LUN_LIST *Next;
    UNICODE_STRING UnicodeInquiryData;
} UNICODE_LUN_LIST, *PUNICODE_LUN_LIST;

typedef struct _POWER_CHANGE_CONTEXT {
    PDEVICE_OBJECT DeviceObject;
    POWER_STATE_TYPE Type;
    POWER_STATE State;
    PIRP OriginalIrp;
    PSCSI_REQUEST_BLOCK Srb;
} POWER_CHANGE_CONTEXT, *PPOWER_CHANGE_CONTEXT;

//
// Driver extension
//

struct _SP_INIT_CHAIN_ENTRY {
    HW_INITIALIZATION_DATA InitData;
    PSP_INIT_CHAIN_ENTRY NextEntry;
};

typedef struct _SCSIPORT_INTERFACE_TYPE_DATA {
    INTERFACE_TYPE InterfaceType;
    ULONG Flags;
} SCSIPORT_INTERFACE_TYPE_DATA, *PSCSIPORT_INTERFACE_TYPE_DATA;

typedef struct _SCSIPORT_DRIVER_EXTENSION {

    //
    // Pointer back to the driver object
    //

    PDRIVER_OBJECT DriverObject;

    //
    // Unicode string containing the registry path information
    // for this driver
    //

    UNICODE_STRING RegistryPath;

    //
    // the chain of HwInitializationData structures that were passed in during
    // the miniport's initialization
    //

    PSP_INIT_CHAIN_ENTRY InitChain;

    //
    // A count of the number of adapter which are using scsiport.  This is
    // used for generating unique Id's
    //

    ULONG AdapterCount;

    //
    // The bus type for this driver.
    //

    STORAGE_BUS_TYPE BusType;

    //
    // Flag indicating whether this miniport is set to do device detection.
    // This flag will be initialized out of the registry when the driver
    // extension is setup.
    //

    BOOLEAN LegacyAdapterDetection;

    //
    // The list of pnp interface values we read out of the registry for this
    // device.  The number of entries here can vary.
    //

    ULONG PnpInterfaceCount;

    //
    // The number of interfaces which are safe for pnp.
    //

    ULONG SafeInterfaceCount;

    SCSIPORT_INTERFACE_TYPE_DATA PnpInterface[0];

    //
    // The remaining pnp interface flags trail the defined structure
    //

} SCSIPORT_DRIVER_EXTENSION, *PSCSIPORT_DRIVER_EXTENSION;


//
// Port driver extension flags.
// These flags are protected by the adapter spinlock.
//

//
// This flag indicates that a request has been passed to the miniport and the
// miniport has not indicated it is ready for another request.  It is set by
// SpStartIoSynchronized. It is cleared by ScsiPortCompletionDpc when the
// miniport asks for another request.  Note the port driver will defer giving
// the miniport driver a new request if the current request disabled disconnects.
//

#define PD_DEVICE_IS_BUSY            0X00001

//
// Indicates there is a pending request for which resources
// could not be allocated.  This flag is set by SpAllocateRequestStructures
// which is called from ScsiPortStartIo.  It is cleared by
// SpProcessCompletedRequest when a request completes which then calls
// ScsiPortStartIo to try the request again.
//

#define PD_PENDING_DEVICE_REQUEST    0X00800

//
// This flag indicates that there are currently no requests executing with
// disconnects disabled.  This flag is normally on.  It is cleared by
// SpStartIoSynchronized when a request with disconnect disabled is started
// and is set when that request completes.  SpProcessCompletedRequest will
// start the next request for the miniport if PD_DEVICE_IS_BUSY is clear.
//

#define PD_DISCONNECT_RUNNING        0X01000

//
// Indicates the miniport wants the system interrupts disabled.  Set by
// ScsiPortNofitication and cleared by ScsiPortCompletionDpc.  This flag is
// NOT stored in the interrupt data structure.  The parameters are stored in
// the device extension.
//

#define PD_DISABLE_CALL_REQUEST      0X02000

//
// Indicates that the miniport is being reinitialized.  This is set and
// cleared by SpReinitializeAdapter is is tested by some of the ScsiPort APIs.
//

#define PD_MINIPORT_REINITIALIZING          0x40000
#define PD_UNCACHED_EXTENSION_RETURNED      0x80000

//
// Interrupt Data Flags
// These flags are protected by the interrupt spinlock.
//

//
// Indicates that ScsiPortCompletionDpc needs to be run.  This is set when
// A miniport makes a request which must be done at DPC and is cleared when
// when the request information is gotten by SpGetInterruptState.
//

#define PD_NOTIFICATION_REQUIRED     0X00004

//
// Indicates the miniport is ready for another request.  Set by
// ScsiPortNotification and cleared by SpGetInterruptState.  This flag is
// stored in the interrupt data structure.
//

#define PD_READY_FOR_NEXT_REQUEST    0X00008

//
// Indicates the miniport wants the adapter channel flushed.  Set by
// ScsiPortFlushDma and cleared by SpGetInterruptState.  This flag is
// stored in the data interrupt structure.  The flush adapter parameters
// are saved in the device object.
//

#define PD_FLUSH_ADAPTER_BUFFERS     0X00010

//
// Indicates the miniport wants the adapter channel programmed.  Set by
// ScsiPortIoMapTransfer and cleared by SpGetInterruptState or
// ScsiPortFlushDma.  This flag is stored in the interrupt data structure.
// The I/O map transfer parameters are saved in the interrupt data structure.
//

#define PD_MAP_TRANSFER              0X00020

//
// Indicates the miniport wants to log an error.  Set by
// ScsiPortLogError and cleared by SpGetInterruptState.  This flag is
// stored in the interrupt data structure.  The error log parameters
// are saved in the interrupt data structure.  Note at most one error per DPC
// can be logged.
//

#define PD_LOG_ERROR                 0X00040

//
// Indicates that no request should be sent to the miniport after
// a bus reset. Set when the miniport reports a reset or the port driver
// resets the bus. It is cleared by SpTimeoutSynchronized.  The
// PortTimeoutCounter is used to time the length of the reset hold.  This flag
// is stored in the interrupt data structure.
//

#define PD_RESET_HOLD                0X00080

//
// Indicates a request was stopped due to a reset hold.  The held request is
// stored in the current request of the device object.  This flag is set by
// SpStartIoSynchronized and cleared by SpTimeoutSynchronized which also
// starts the held request when the reset hold has ended.  This flag is stored
// in the interrupt data structure.
//

#define PD_HELD_REQUEST              0X00100

//
// Indicates the miniport has reported a bus reset.  Set by
// ScsiPortNotification and cleared by SpGetInterruptState.  This flag is
// stored in the interrupt data structure.
//

#define PD_RESET_REPORTED            0X00200

//
// Indicates that system interrupts have been enabled and that the miniport
// has disabled its adapter from interruptint.  The miniport's interrupt
// routine is not called while this flag is set.  This flag is set by
// ScsiPortNotification when a CallEnableInterrupts request is made and
// cleared by SpEnableInterruptSynchronized when the miniport requests that
// system interrupts be disabled.  This flag is stored in the interrupt data
// structure.
//

#define PD_DISABLE_INTERRUPTS        0X04000

//
// Indicates the miniport wants the system interrupt enabled.  Set by
// ScsiPortNotification and cleared by SpGetInterruptState.  This flag is
// stored in the interrupt data structure.  The call enable interrupts
// parameters are saved in the device extension.
//

#define PD_ENABLE_CALL_REQUEST       0X08000

//
// Indicates the miniport is wants a timer request.  Set by
// ScsiPortNotification and cleared by SpGetInterruptState.  This flag is
// stored in the interrupt data structure. The timer request parameters are
// stored in the interrupt data structure.
//

#define PD_TIMER_CALL_REQUEST        0X10000

//
// Indicates the miniport has a WMI request.  Set by ScsiPortNotification
// and cleared by SpGetInterruptState.  This flag is stored in the interrupt
// data structure.    The WMI request parameters are stored in the interrupt
// data structure.
//

#define PD_WMI_REQUEST               0X20000

//
// Indicates that the miniport has detected some sort of change on the bus - 
// usually device arrival or removal - and wishes the port driver to rescan 
// the bus.
//

#define PD_BUS_CHANGE_DETECTED       0x40000

//
// Indicates that the adapter has disappeared.  If this flag is set then no 
// calls should be made into the miniport.
//

#define PD_ADAPTER_REMOVED           0x80000

//
// Indicates that interrupts from the miniport do not appear to be getting 
// delivered to scsiport.  This flag is set by SpTimeoutSynchronized and 
// will cause the DPC routine to log an error to this effect.
//

#define PD_INTERRUPT_FAILURE         0x100000


//
// The following flags should not be cleared from the interrupt data structure
// by SpGetInterruptState.
//

#define PD_INTERRUPT_FLAG_MASK (PD_RESET_HOLD | PD_HELD_REQUEST | PD_DISABLE_INTERRUPTS | PD_ADAPTER_REMOVED)

//
// Adapter extension flags for DPC routine.
//

//
// Indicates that the completion DPC is either already running or has been
// queued to service completed requests.  This flag is checked when the
// completion DPC needs to be run - the DPC should only be started if this
// flag is already clear.  It will be cleared when the DPC has completed
// processing any work items.
//

#define PD_DPC_RUNNING              0x20000

//
// Logical unit extension flags.
//

//
// Indicates the logical unit queue is frozen.  Set by
// SpProcessCompletedRequest when an error occurs and is cleared by the class
// driver.
//

#define LU_QUEUE_FROZEN              0X0001

//
// Indicates that the miniport has an active request for this logical unit.
// Set by SpStartIoSynchronized when the request is started and cleared by
// GetNextLuRequest.  This flag is used to track when it is ok to start another
// request from the logical unit queue for this device.
//

#define LU_LOGICAL_UNIT_IS_ACTIVE    0X0002

//
// Indicates that a request for this logical unit has failed and a REQUEST
// SENSE command needs to be done. This flag prevents other requests from
// being started until an untagged, by-pass queue command is started.  This
// flag is cleared in SpStartIoSynchronized.  It is set by
// SpGetInterruptState.
//

#define LU_NEED_REQUEST_SENSE  0X0004

//
// Indicates that a request for this logical unit has completed with a status
// of BUSY or QUEUE FULL.  This flag is set by SpProcessCompletedRequest and
// the busy request is saved in the logical unit structure.  This flag is
// cleared by ScsiPortTickHandler which also restarts the request.  Busy
// request may also be requeued to the logical unit queue if an error occurs
// on the device (This will only occur with command queueing.).  Not busy
// requests are nasty because they are restarted asynchronously by
// ScsiPortTickHandler rather than GetNextLuRequest. This makes error recovery
// more complex.
//

#define LU_LOGICAL_UNIT_IS_BUSY      0X0008

//
// This flag indicates a queue full has been returned by the device.  It is
// similar to PD_LOGICAL_UNIT_IS_BUSY but is set in SpGetInterruptState when
// a QUEUE FULL status is returned.  This flag is used to prevent other
// requests from being started for the logical unit before
// SpProcessCompletedRequest has a chance to set the busy flag.
//

#define LU_QUEUE_IS_FULL             0X0010

//
// Indicates that there is a request for this logical unit which cannot be
// executed for now.  This flag is set by SpAllocateRequestStructures.  It is
// cleared by GetNextLuRequest when it detects that the pending request
// can now be executed. The pending request is stored in the logical unit
// structure.  A new single non-queued reqeust cannot be executed on a logical
// that is currently executing queued requests.  Non-queued requests must wait
// unit for all queued requests to complete.  A non-queued requests is one
// which is not tagged and does not have SRB_FLAGS_NO_QUEUE_FREEZE set.
// Normally only read and write commands can be queued.
//

#define LU_PENDING_LU_REQUEST        0x0020

//
// Indicates that the logical unit queue has been paused due to an error.  Set
// by SpProcessCompletedRequest when an error occurs and is cleared by the
// class driver either by unfreezing or flushing the queue.  This flag is used
// with the following one to determine why the logical unit queue is paused.
//

#define LU_QUEUE_LOCKED             0x0040

//
// Indicates that this LUN has been "paused".  This flag is set and cleared by
// the power management code while changing the power state.  It causes 
// GetNextLuRequest to return without starting another request and is used 
// by SpSrbIsBypassRequest to determine that a bypass request should get 
// shoved to the front of the line.
//

#define LU_QUEUE_PAUSED             0x0080

//
// Indicates that the LogicalUnit has been allocated for a rescan request.
// This flag prevents IOCTL_SCSI_MINIPORT requests from attaching to this
// logical unit, since the possibility exists that it could be freed before
// the IOCTL request is complete.
//

#define LU_RESCAN_ACTIVE             0x8000

//
// SRB_DATA flags.
//

//
// These three flags indicate the size of scatter gather list necessary to
// service the request and are used to determine how the scatter gather list
// should be freed.  Small requests require <= SP_SMALL_PHYSICAL_BREAK_VALUE
// breaks and the scatter gather list is preallocated in the SRB_DATA structure.
// Large requests are >= SP_LARGE_PHYSICAL_BREAK_VALUE and have scatter gather
// lists allocated from non-paged pool.  Medium requests are between small
// and large - they use scatter gather lists from a lookaside list which contain
// one less entry than a large list would.
//

#define SRB_DATA_SMALL_SG_LIST      0x00000001
#define SRB_DATA_MEDIUM_SG_LIST     0x00000002
#define SRB_DATA_LARGE_SG_LIST      0x00000004

//
// Indicates that the srb_data block was for a bypass request 
//

#define SRB_DATA_BYPASS_REQUEST     0x10000000

//
// Port Timeout Counter values.
//

#define PD_TIMER_STOPPED             -1
#define PD_TIMER_RESET_HOLD_TIME     4

//
// Possible registry flags for pnp interface key
//

//
// The absence of any information about a particular interface in the
// PnpInterface key in the registry indicates that pnp is not safe for this
// particular card.
//

#define SP_PNP_NOT_SAFE             0x00000000

//
// Indicates that pnp is a safe operation for this device.  If this flag is
// set then the miniport will not be allowed to do detection and will always
// be handed resources provided by the pnp system.  This flag may or may not
// be set in the registry - the fact that a value for a particular interface
// exists is enough to indicate that pnp is safe and this flag will always
// be set.
//

#define SP_PNP_IS_SAFE              0x00000001

//
// Indicates that we should take advantage of a chance to enumerate a particular
// bus type using the miniport.  This flag is set for all non-enumerable legacy
// buses (ISA, EISA, etc...) and is cleared for the non-legacy ones and for the
// PnpBus type.
//

#define SP_PNP_NON_ENUMERABLE       0x00000002

//
// Indicates that we need to include some sort of location information in the 
// config data to discern this adapter from any others.
//

#define SP_PNP_NEEDS_LOCATION       0x00000004

//
// Indicates that this type of adapter must have an interrupt for us to try 
// and start it.  If PNP doesn't provide an interrupt then scsiport will 
// log an error and fail the start operation.  If this flag is set then 
// SP_PNP_IS_SAFE must also be set.
//

#define SP_PNP_INTERRUPT_REQUIRED   0x00000008

//
// Indicates that legacy detection should not be done.
//

#define SP_PNP_NO_LEGACY_DETECTION  0x00000010

//
// Internal scsiport srb status codes.
// these must be between 0x38 and 0x3f (inclusive) and should never get 
// returned to a class driver.
//
// These values are used after the srb has been put on the adapter's 
// startio queue and thus cannot be completed without running it through the 
// completion DPC.
//

#ifndef KDBG_EXT
//
// Function declarations
//

NTSTATUS
ScsiPortGlobalDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ScsiPortFdoCreateClose (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ScsiPortFdoDispatch(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ScsiPortPdoScsi(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
ScsiPortStartIo(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

BOOLEAN
ScsiPortInterrupt(
    IN PKINTERRUPT InterruptObject,
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
ScsiPortFdoDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ScsiPortPdoDeviceControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ScsiPortPdoCreateClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ScsiPortPdoPnp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
ScsiPortTickHandler(
    IN PDEVICE_OBJECT DeviceObject,
    IN PVOID Context
    );

VOID
IssueRequestSense(
    IN PADAPTER_EXTENSION deviceExtension,
    IN PSCSI_REQUEST_BLOCK FailingSrb
    );

BOOLEAN
SpStartIoSynchronized (
    PVOID ServiceContext
    );

BOOLEAN
SpResetBusSynchronized (
    PVOID ServiceContext
    );

BOOLEAN
SpTimeoutSynchronized (
    PVOID ServiceContext
    );

BOOLEAN
SpEnableInterruptSynchronized (
    PVOID ServiceContext
    );

VOID
IssueAbortRequest(
    IN PADAPTER_EXTENSION DeviceExtension,
    IN PLOGICAL_UNIT_EXTENSION LogicalUnit
    );

IO_ALLOCATION_ACTION
SpBuildScatterGather(
    IN struct _DEVICE_OBJECT *DeviceObject,
    IN struct _IRP *Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context
    );

BOOLEAN
SpGetInterruptState(
    IN PVOID ServiceContext
    );

#if DBG

#define GetLogicalUnitExtension(fdo, path, target, lun, lock, getlock) \
    GetLogicalUnitExtensionEx(fdo, path, target, lun, lock, getlock, __file__, __LINE__)

PLOGICAL_UNIT_EXTENSION
GetLogicalUnitExtensionEx(
    PADAPTER_EXTENSION DeviceExtension,
    UCHAR PathId,
    UCHAR TargetId,
    UCHAR Lun,
    PVOID LockTag,
    BOOLEAN AcquireBinLock,
    PCSTR File,
    ULONG Line
    );

#else

PLOGICAL_UNIT_EXTENSION
GetLogicalUnitExtension(
    PADAPTER_EXTENSION DeviceExtension,
    UCHAR PathId,
    UCHAR TargetId,
    UCHAR Lun,
    PVOID LockTag,
    BOOLEAN AcquireBinLock
    );

#endif

IO_ALLOCATION_ACTION
ScsiPortAllocationRoutine (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID MapRegisterBase,
    IN PVOID Context
    );

VOID
LogErrorEntry(
    IN PADAPTER_EXTENSION DeviceExtension,
    IN PERROR_LOG_ENTRY LogEntry
    );

VOID
FASTCALL
GetNextLuRequest(
    IN PLOGICAL_UNIT_EXTENSION LogicalUnit
    );

VOID
GetNextLuRequestWithoutLock(
    IN PLOGICAL_UNIT_EXTENSION LogicalUnit
    );

VOID
SpLogTimeoutError(
    IN PADAPTER_EXTENSION DeviceExtension,
    IN PIRP Irp,
    IN ULONG UniqueId
    );

VOID
SpProcessCompletedRequest(
    IN PADAPTER_EXTENSION DeviceExtension,
    IN PSRB_DATA SrbData,
    OUT PBOOLEAN CallStartIo
    );

PSRB_DATA
SpGetSrbData(
    IN PADAPTER_EXTENSION DeviceExtension,
    UCHAR PathId,
    UCHAR TargetId,
    UCHAR Lun,
    UCHAR QueueTag,
    BOOLEAN AcquireBinLock
    );

VOID
SpCompleteSrb(
    IN PADAPTER_EXTENSION DeviceExtension,
    IN PSRB_DATA SrbData,
    IN UCHAR SrbStatus
    );

BOOLEAN
SpAllocateSrbExtension(
    IN PADAPTER_EXTENSION DeviceExtension,
    IN PLOGICAL_UNIT_EXTENSION LogicalUnit,
    IN PSCSI_REQUEST_BLOCK Srb
    );

NTSTATUS
SpSendMiniPortIoctl(
    IN PADAPTER_EXTENSION DeviceExtension,
    IN PIRP RequestIrp
    );

NTSTATUS
SpGetInquiryData(
    IN PADAPTER_EXTENSION DeviceExtension,
    IN PIRP Irp
    );

NTSTATUS
SpSendPassThrough(
    IN PADAPTER_EXTENSION DeviceExtension,
    IN PIRP Irp
    );

NTSTATUS
SpClaimLogicalUnit(
    IN PADAPTER_EXTENSION AdapterExtension,
    IN PLOGICAL_UNIT_EXTENSION LogicalUnitExtension,
    IN PIRP Irp,
    IN BOOLEAN StartDevice
    );

VOID
SpMiniPortTimerDpc(
    IN struct _KDPC *Dpc,
    IN PVOID DeviceObject,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

BOOLEAN
SpSynchronizeExecution (
    IN PKINTERRUPT Interrupt,
    IN PKSYNCHRONIZE_ROUTINE SynchronizeRoutine,
    IN PVOID SynchronizeContext
    );

NTSTATUS
IssueInquiry(
    IN PDEVICE_OBJECT LogicalUnit,
    OUT PINQUIRYDATA InquiryData,
    OUT PULONG BytesReturned
    );

NTSTATUS
SpGetCommonBuffer(
    IN PADAPTER_EXTENSION DeviceExtension,
    IN ULONG NonCachedExtensionSize
    );

VOID
SpDestroyAdapter(
    IN PADAPTER_EXTENSION Adapter,
    IN BOOLEAN Surprise
    );

VOID
SpReleaseAdapterResources(
    IN PADAPTER_EXTENSION Adapter,
    IN BOOLEAN Surprise
    );

NTSTATUS
SpInitializeConfiguration(
    IN PADAPTER_EXTENSION DeviceExtension,
    IN PUNICODE_STRING RegistryPath,
    IN PHW_INITIALIZATION_DATA HwInitData,
    IN PCONFIGURATION_CONTEXT Context
    );

VOID
SpParseDevice(
    IN PADAPTER_EXTENSION DeviceExtension,
    IN HANDLE Key,
    IN PCONFIGURATION_CONTEXT Context,
    IN PUCHAR Buffer
    );

NTSTATUS
SpConfigurationCallout(
    IN PVOID Context,
    IN PUNICODE_STRING PathName,
    IN INTERFACE_TYPE BusType,
    IN ULONG BusNumber,
    IN PKEY_VALUE_FULL_INFORMATION *BusInformation,
    IN CONFIGURATION_TYPE ControllerType,
    IN ULONG ControllerNumber,
    IN PKEY_VALUE_FULL_INFORMATION *ControllerInformation,
    IN CONFIGURATION_TYPE PeripheralType,
    IN ULONG PeripheralNumber,
    IN PKEY_VALUE_FULL_INFORMATION *PeripheralInformation
    );

PCM_RESOURCE_LIST
SpBuildResourceList(
    PADAPTER_EXTENSION DeviceExtension,
    PPORT_CONFIGURATION_INFORMATION MiniportConfigInfo
    );

BOOLEAN
GetPciConfiguration(
    IN PDRIVER_OBJECT          DriverObject,
    IN OUT PDEVICE_OBJECT      DeviceObject,
    IN PHW_INITIALIZATION_DATA HwInitializationData,
    IN PVOID                   RegistryPath,
    IN ULONG                   BusNumber,
    IN OUT PPCI_SLOT_NUMBER    SlotNumber
    );

NTSTATUS
ScsiPortAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject
    );

VOID
ScsiPortUnload(
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
ScsiPortFdoPnp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
ScsiPortStartAdapter(
    IN PDEVICE_OBJECT Fdo
    );

NTSTATUS
ScsiPortStopAdapter(
    IN PDEVICE_OBJECT Adapter,
    IN PIRP StopRequest
    );

NTSTATUS
ScsiPortStartDevice(
    IN PDEVICE_OBJECT Pdo
    );

NTSTATUS
ScsiPortInitDevice(
    IN PDEVICE_OBJECT Pdo
    );

NTSTATUS
ScsiPortStopDevice(
    IN PDEVICE_OBJECT LogicalUnit
    );

NTSTATUS
SpEnumerateAdapter(
    IN PDEVICE_OBJECT Fdo,
    IN BOOLEAN Force
    );

VOID
SpUnenumerateDevices(
    IN PDEVICE_OBJECT Fdo
    );

VOID
SpUnlinkUnenumeratedDevices(
    IN PDEVICE_OBJECT Fdo
    );

PDEVICE_OBJECT
SpFindDevice(
    IN PDEVICE_OBJECT Fdo,
    UCHAR PathId,
    UCHAR TargetId,
    UCHAR Lun
    );

VOID
SpScanBusInfo(
    IN PDEVICE_OBJECT Fdo,
    IN PSCSI_CONFIGURATION_INFO ScsiInfo
    );

NTSTATUS
SpFindNextDevice(
    IN PDEVICE_OBJECT Fdo,
    IN PDEVICE_OBJECT PreviousPdo,
    OUT PDEVICE_OBJECT *NextPdo
    );

NTSTATUS
SpExtractDeviceRelations(
    IN PDEVICE_OBJECT Fdo,
    IN DEVICE_RELATION_TYPE RelationType,
    OUT PDEVICE_RELATIONS *DeviceRelations
    );

NTSTATUS
SpCreateLogicalUnit(
    IN PDEVICE_OBJECT AdapterFdo,
    UCHAR PathId,
    UCHAR TargetId,
    UCHAR Lun,
    OUT PDEVICE_OBJECT *NewPdo
    );

VOID
ScsiPortInitializeDispatchTables(
    VOID
    );

NTSTATUS
ScsiPortStringArrayToMultiString(
    PUNICODE_STRING MultiString,
    PCSTR StringArray[]
    );

NTSTATUS
ScsiPortGetDeviceId(
    IN PDEVICE_OBJECT Pdo,
    OUT PUNICODE_STRING UnicodeString
    );

NTSTATUS
ScsiPortGetInstanceId(
    IN PDEVICE_OBJECT Pdo,
    OUT PUNICODE_STRING UnicodeString
    );

NTSTATUS
ScsiPortGetCompatibleIds(
    IN PINQUIRYDATA InquiryData,
    OUT PUNICODE_STRING UnicodeString
    );

NTSTATUS
ScsiPortGetHardwareIds(
    IN PINQUIRYDATA InquiryData,
    OUT PUNICODE_STRING UnicodeString
    );

NTSTATUS
ScsiPortStartAdapterCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
SpReportNewAdapter(
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
ScsiPortQueryProperty(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP QueryIrp
    );

NTSTATUS
ScsiPortInitLegacyAdapter(
    IN PSCSIPORT_DRIVER_EXTENSION DriverExtension,
    IN PHW_INITIALIZATION_DATA HwInitializationData,
    IN PVOID HwContext
    );

NTSTATUS
SpCreateAdapter(
    IN PDRIVER_OBJECT DriverObject,
    OUT PDEVICE_OBJECT *Fdo
    );

VOID
SpInitializeAdapterExtension(
    IN PADAPTER_EXTENSION FdoExtension,
    IN PHW_INITIALIZATION_DATA HwInitializationData,
    IN OUT PHW_DEVICE_EXTENSION HwDeviceExtension OPTIONAL
    );

PHW_INITIALIZATION_DATA
SpFindInitData(
    IN PSCSIPORT_DRIVER_EXTENSION DriverExtension,
    IN INTERFACE_TYPE InterfaceType
    );

VOID
SpBuildConfiguration(
    IN PADAPTER_EXTENSION    AdapterExtension,
    IN PHW_INITIALIZATION_DATA         HwInitializationData,
    IN PPORT_CONFIGURATION_INFORMATION ConfigInformation
    );

NTSTATUS
SpCallHwFindAdapter(
    IN PDEVICE_OBJECT Fdo,
    IN PHW_INITIALIZATION_DATA HwInitData,
    IN PVOID HwContext OPTIONAL,
    IN OUT PCONFIGURATION_CONTEXT ConfigurationContext,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN CallAgain
    );

NTSTATUS
SpCallHwInitialize(
    IN PDEVICE_OBJECT Fdo
    );

HANDLE
SpOpenParametersKey(
    IN PUNICODE_STRING RegistryPath
    );

HANDLE
SpOpenDeviceKey(
    IN PUNICODE_STRING RegistryPath,
    IN ULONG DeviceNumber
    );

ULONG
SpQueryPnpInterfaceFlags(
    IN PSCSIPORT_DRIVER_EXTENSION DriverExtension,
    IN INTERFACE_TYPE InterfaceType
    );

NTSTATUS
SpGetRegistryValue(
    IN HANDLE Handle,
    IN PWSTR KeyString,
    OUT PKEY_VALUE_FULL_INFORMATION *KeyInformation
    );

NTSTATUS
SpInitDeviceMap(
    VOID
    );

NTSTATUS
SpBuildDeviceMapEntry(
    IN PDEVICE_OBJECT DeviceObject
    );

VOID
SpDeleteDeviceMapEntry(
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
SpUpdateLogicalUnitDeviceMapEntry(
    IN PLOGICAL_UNIT_EXTENSION LogicalUnit
    );

VOID
SpLogResetError(
    IN PADAPTER_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK  Srb,
    IN ULONG UniqueId
    );

VOID
SpRemoveLogicalUnitFromBin (
    IN PADAPTER_EXTENSION AdapterExtension,
    IN PLOGICAL_UNIT_EXTENSION LogicalUnitExtension
    );

VOID
SpAddLogicalUnitToBin (
    IN PADAPTER_EXTENSION AdapterExtension,
    IN PLOGICAL_UNIT_EXTENSION LogicalUnitExtension
    );

PSCSIPORT_DEVICE_TYPE
SpGetDeviceTypeInfo(
    IN UCHAR DeviceType
    );

BOOLEAN
SpRemoveLogicalUnit(
    IN PDEVICE_OBJECT LogicalUnit,
    IN UCHAR RemoveType
    );

PLOGICAL_UNIT_EXTENSION
SpFindSafeLogicalUnit(
    IN PDEVICE_OBJECT DeviceObject,
    IN UCHAR PathId,
    IN PVOID LockTag
    );

NTSTATUS
ScsiPortSystemControlIrp(
    IN     PDEVICE_OBJECT DeviceObject,
    IN OUT PIRP           Irp);

NTSTATUS
SpWmiIrpNormalRequest(
    IN     PDEVICE_OBJECT  DeviceObject,
    IN     UCHAR           WmiMinorCode,
    IN OUT PWMI_PARAMETERS WmiParameters);

NTSTATUS
SpWmiIrpRegisterRequest(
    IN     PDEVICE_OBJECT  DeviceObject,
    IN OUT PWMI_PARAMETERS WmiParameters);

NTSTATUS
SpWmiHandleOnMiniPortBehalf(
    IN     PDEVICE_OBJECT  DeviceObject,
    IN     UCHAR           WmiMinorCode,
    IN OUT PWMI_PARAMETERS WmiParameters);

NTSTATUS
SpWmiPassToMiniPort(
    IN     PDEVICE_OBJECT  DeviceObject,
    IN     UCHAR           WmiMinorCode,
    IN OUT PWMI_PARAMETERS WmiParameters);

VOID
SpWmiInitializeSpRegInfo(
    IN  PDEVICE_OBJECT  DeviceObject);

VOID
SpWmiGetSpRegInfo(
    IN  PDEVICE_OBJECT DeviceObject,
    OUT PWMIREGINFO  * SpRegInfoBuf,
    OUT ULONG        * SpRegInfoBufSize);

VOID
SpWmiDestroySpRegInfo(
    IN  PDEVICE_OBJECT DeviceObject);

NTSTATUS
SpWmiInitializeFreeRequestList(
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG          NumberOfItems
    );

NTSTATUS
SpWmiPushFreeRequestItem(
    IN PADAPTER_EXTENSION           Adapter
    );


PWMI_MINIPORT_REQUEST_ITEM
SpWmiPopFreeRequestItem(
    IN PADAPTER_EXTENSION           Adapter
    );

BOOLEAN
SpWmiRemoveFreeMiniPortRequestItems(
    IN PADAPTER_EXTENSION   fdoExtension
    );

#if DBG
ULONG
FASTCALL
FASTCALL
SpAcquireRemoveLockEx(
    IN PDEVICE_OBJECT DeviceObject,
    IN OPTIONAL PVOID Tag,
    IN PCSTR File,
    IN ULONG Line
    );
#else 
ULONG
INLINE
SpAcquireRemoveLock(
    IN PDEVICE_OBJECT DeviceObject,
    IN OPTIONAL PVOID Tag
    )   
{
    PCOMMON_EXTENSION commonExtension = DeviceObject->DeviceExtension;
    InterlockedIncrement(&commonExtension->RemoveLock);
    return (commonExtension->IsRemoved);
}
#endif

VOID
FASTCALL
SpReleaseRemoveLock(
    IN PDEVICE_OBJECT DeviceObject,
    IN OPTIONAL PVOID Tag
    );

VOID
FASTCALL
FASTCALL
SpCompleteRequest(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN OPTIONAL PSRB_DATA SrbData,
    IN CCHAR PriorityBoost
    );

NTSTATUS
ScsiPortDispatchPower(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
SpDefaultPowerCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN UCHAR MinorFunction,
    IN POWER_STATE PowerState,
    IN PIRP OriginalIrp,
    IN PIO_STATUS_BLOCK IoStatus
    );

PCM_RESOURCE_LIST
RtlDuplicateCmResourceList(
    POOL_TYPE PoolType,
    PCM_RESOURCE_LIST ResourceList,
    ULONG Tag
    );

ULONG
RtlSizeOfCmResourceList(
    IN PCM_RESOURCE_LIST ResourceList
    );

BOOLEAN
SpTranslateResources(
    IN PCM_RESOURCE_LIST AllocatedResources,
    OUT PCM_RESOURCE_LIST *TranslatedResources
    );

BOOLEAN
SpFindAddressTranslation(
    IN PADAPTER_EXTENSION AdapterExtension,
    IN INTERFACE_TYPE BusType,
    IN ULONG BusNumber,
    IN PHYSICAL_ADDRESS RangeStart,
    IN ULONG RangeLength,
    IN BOOLEAN InIoSpace,
    IN OUT PCM_PARTIAL_RESOURCE_DESCRIPTOR Translation
    );

NTSTATUS
SpAllocateAdapterResources(
    IN PDEVICE_OBJECT Fdo
    );

NTSTATUS
SpLockUnlockQueue(
    IN PDEVICE_OBJECT LogicalUnit,
    IN BOOLEAN LockQueue,
    IN BOOLEAN BypassLockedQueue
    );

VOID
ScsiPortRemoveAdapter(
    IN PDEVICE_OBJECT Adapter,
    IN BOOLEAN Surprise
    );

VOID
SpTerminateAdapter(
    IN PADAPTER_EXTENSION Adapter
    );

NTSTATUS
SpQueryDeviceText(
    IN PDEVICE_OBJECT LogicalUnit,
    IN DEVICE_TEXT_TYPE TextType,
    IN LCID LocaleId,
    IN OUT PWSTR *DeviceText
    );

NTSTATUS
SpCheckSpecialDeviceFlags(
    IN PLOGICAL_UNIT_EXTENSION LogicalUnit,
    IN PINQUIRYDATA InquiryData
    );

PSRB_DATA
FASTCALL
SpAllocateSrbData(
    IN PADAPTER_EXTENSION Adapter,
    IN OPTIONAL PIRP Request
    );

PSRB_DATA
FASTCALL
SpAllocateBypassSrbData(
    IN PLOGICAL_UNIT_EXTENSION LogicalUnit
    );

VOID
SpCheckSrbLists(
    IN PADAPTER_EXTENSION Adapter,
    IN PUCHAR FailureString
    );

VOID
ScsiPortCompletionDpc(
    IN PKDPC Dpc,
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

NTSTATUS
SpAllocateTagBitMap(
    IN PADAPTER_EXTENSION Adapter
    );

NTSTATUS
SpRequestValidPowerState(
    IN PADAPTER_EXTENSION Adapter,
    IN PLOGICAL_UNIT_EXTENSION LogicalUnit,
    IN PSCSI_REQUEST_BLOCK Srb
    );

NTSTATUS
SpRequestValidAdapterPowerStateSynchronous(
    IN PADAPTER_EXTENSION Adapter
    );

NTSTATUS
SpEnableDisableAdapter(
    IN PADAPTER_EXTENSION Adapter,
    IN BOOLEAN Enable
    );

NTSTATUS
SpEnableDisableLogicalUnit(
    IN PLOGICAL_UNIT_EXTENSION LogicalUnit,
    IN BOOLEAN Enable,
    IN PSP_ENABLE_DISABLE_COMPLETION_ROUTINE CompletionRoutine,
    IN PVOID Context
    );

VOID
ScsiPortProcessAdapterPower(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

INTERFACE_TYPE
SpGetPdoInterfaceType(
    IN PDEVICE_OBJECT Pdo
    );

NTSTATUS
SpReadNumericInstanceValue(
    IN PDEVICE_OBJECT Pdo,
    IN PWSTR ValueName,
    OUT PULONG Value
    );

NTSTATUS
SpWriteNumericInstanceValue(
    IN PDEVICE_OBJECT Pdo,
    IN PWSTR ValueName,
    IN ULONG Value
    );

VOID
SpGetSupportedAdapterControlFunctions(
    IN PADAPTER_EXTENSION Adapter
    );

NTSTATUS
SpShrinkMappedAddressList(
    IN PADAPTER_EXTENSION Adapter
   );

VOID
SpReleaseMappedAddresses(
    IN PADAPTER_EXTENSION Adapter
    );

VOID
SpGetSupportedAdapterControlFunctions(
    PADAPTER_EXTENSION Adapter
    );

BOOLEAN
SpIsAdapterControlTypeSupported(
    IN PADAPTER_EXTENSION AdapterExtension,
    IN SCSI_ADAPTER_CONTROL_TYPE ControlType
    );

SCSI_ADAPTER_CONTROL_STATUS
SpCallAdapterControl(
    IN PADAPTER_EXTENSION AdapterExtension,
    IN SCSI_ADAPTER_CONTROL_TYPE ControlType,
    IN PVOID Parameters
    );

PVOID
SpAllocateSrbDataBackend(
    IN POOL_TYPE PoolType,
    IN ULONG NumberOfBytes,
    IN ULONG AdapterIndex
    );

VOID
SpFreeSrbDataBackend(
    IN PSRB_DATA SrbData
    );

ULONG
SpAllocateQueueTag(
    IN PADAPTER_EXTENSION Adapter
    );

VOID
SpReleaseQueueTag(
    IN PADAPTER_EXTENSION Adapter,
    IN ULONG QueueTag
    );

NTSTATUS
SpInitializeGuidInterfaceMapping(
    VOID
    );

NTSTATUS
SpSignalCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PKEVENT Event
    );

NTSTATUS
SpSendIrpSynchronous(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
SpDeleteLogicalUnit(
    IN PDEVICE_OBJECT LogicalUnit
    );

NTSTATUS
SpGetBusTypeGuid(
    IN PADAPTER_EXTENSION Adapter
    );

BOOLEAN
SpDetermine64BitSupport(
    VOID
    );

VOID
SpAdjustDisabledBit(
    IN PLOGICAL_UNIT_EXTENSION LogicalUnit, 
    IN BOOLEAN Enable
    );

NTSTATUS
SpReadNumericValue(
    IN OPTIONAL HANDLE Root,
    IN OPTIONAL PUNICODE_STRING KeyName,
    IN PUNICODE_STRING ValueName,
    OUT PULONG Value
    );


NTSTATUS
INLINE
SpDispatchRequest(
    IN PLOGICAL_UNIT_EXTENSION LogicalUnit,
    IN PIRP Irp
    )
{
    PCOMMON_EXTENSION commonExtension = &(LogicalUnit->CommonExtension);
    PCOMMON_EXTENSION lowerCommonExtension = 
        commonExtension->LowerDeviceObject->DeviceExtension;

    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);
    PSCSI_REQUEST_BLOCK srb = irpStack->Parameters.Scsi.Srb;

    ASSERT_PDO(LogicalUnit->CommonExtension.DeviceObject);
    ASSERT_SRB_DATA(srb->OriginalRequest);
    
    if((LogicalUnit->CommonExtension.IdleTimer != NULL) &&
       (SpSrbRequiresPower(srb)) &&
       !(srb->SrbFlags & SRB_FLAGS_BYPASS_LOCKED_QUEUE) &&
       !(srb->SrbFlags & SRB_FLAGS_NO_KEEP_AWAKE)) {
       PoSetDeviceBusy(LogicalUnit->CommonExtension.IdleTimer);
    }

    ASSERT(irpStack->MajorFunction == IRP_MJ_SCSI);
    return (lowerCommonExtension->MajorFunction[IRP_MJ_SCSI])(
                commonExtension->LowerDeviceObject, 
                Irp);
}


BOOLEAN
INLINE
SpSrbIsBypassRequest(
    PSCSI_REQUEST_BLOCK Srb,
    ULONG LuFlags
    )
/*++

Routine Description:

    This routine determines whether a request is a "bypass" request - one which 
    should skip the lun queueing and be injected straight into the startio 
    queue.
    
    Bypass requests do not start the next LU request when they complete.  This
    ensures that no new i/o is run until the condition being bypassed is 
    cleared.

    Note: LOCK & UNLOCK requests are not bypass requests unless the queue 
          is already locked.  This ensures that the first LOCK request will 
          get run after previously queued requests, but that additional LOCK 
          requests will not get stuck in the lun queue.
          
          Likewise any UNLOCK request sent when the queue is locked will be 
          run immediately.  However since SpStartIoSynchronized checks to 
          see if the request is a bypass request AFTER ScsiPortStartIo has 
          cleared the QUEUE_LOCKED flag this will force the completion dpc 
          to call GetNextLuRequest which will suck the next operation out of
          the lun queue.  This is how i/o is restarted after a lock sequence
          has been completed.

Arguments:

    Srb - the srb in question
    
    LuFlags - the flags for the lun.
    
Return Value:

    TRUE if the request should bypass the lun queue, be injected into the 
         StartIo queue and if GetNextLuRequest should not be called after this
         request has completed.
         
    FALSE otherwise
    
--*/                          
    
{
    ULONG flags = Srb->SrbFlags & (SRB_FLAGS_BYPASS_FROZEN_QUEUE | 
                                   SRB_FLAGS_BYPASS_LOCKED_QUEUE);

    ASSERT(TEST_FLAG(LuFlags, LU_QUEUE_FROZEN | LU_QUEUE_LOCKED) !=
           (LU_QUEUE_FROZEN | LU_QUEUE_LOCKED));

    if(flags == 0) {
        return FALSE;
    }

    if(flags & SRB_FLAGS_BYPASS_LOCKED_QUEUE) {

        DebugPrint((2, "SpSrbIsBypassRequest: Srb %#08lx is marked to bypass "
                       "locked queue\n", Srb));

        if(TEST_FLAG(LuFlags, LU_QUEUE_LOCKED | LU_QUEUE_PAUSED)) {

            DebugPrint((1, "SpSrbIsBypassRequest: Queue is locked - %#08lx is "
                           "a bypass srb\n", Srb));
            return TRUE;
        } else {
            DebugPrint((3, "SpSrbIsBypassRequest: Queue is not locked - not a "
                           "bypass request\n"));
            return FALSE;
        }
    }

    return TRUE;
}

VOID
INLINE
SpRequestCompletionDpc(
    IN PDEVICE_OBJECT Adapter
    )

/*++

Routine Description:

    This routine will request that the Completion DPC be queued if there isn't
    already one queued or in progress.  It will set the DpcFlags
    PD_DPC_NOTIFICATION_REQUIRED and PD_DPC_RUNNING.  If the DPC_RUNNING flag
    was not already set then it will request a DPC from the system as well.

Arguments:

    Adapter - the Adapter to request the DPC for

Return Value:

    none

--*/

{
    PADAPTER_EXTENSION adapterExtension = Adapter->DeviceExtension;
    ULONG oldDpcFlags;

    //
    // Set the DPC flags to indicate that there is work to be processed
    // (otherwise we wouldn't queue the DPC) and that the DPC is queued.
    //

    oldDpcFlags = InterlockedExchange(
                    &(adapterExtension->DpcFlags),
                    (PD_NOTIFICATION_REQUIRED | PD_DPC_RUNNING));

    //
    // If the DPC was already queued or running then don't bother requesting
    // a new one - the current one will pickup the work itself.
    //

    if(TEST_FLAG(oldDpcFlags, PD_DPC_RUNNING) == FALSE) {
        IoRequestDpc(Adapter, NULL, NULL);
    }

    return;
}


NTSTATUS
INLINE
SpTranslateScsiStatus(
    IN PSCSI_REQUEST_BLOCK Srb
    )
/*++

Routine Description:

    This routine translates an srb status into an ntstatus.

Arguments:

    Srb - Supplies a pointer to the failing Srb.

Return Value:

    An nt status approprate for the error.

--*/

{
    switch (SRB_STATUS(Srb->SrbStatus)) {
    case SRB_STATUS_INVALID_LUN:
    case SRB_STATUS_INVALID_TARGET_ID:
    case SRB_STATUS_NO_DEVICE:
    case SRB_STATUS_NO_HBA:
        return(STATUS_DEVICE_DOES_NOT_EXIST);
    case SRB_STATUS_COMMAND_TIMEOUT:
    case SRB_STATUS_TIMEOUT:
        return(STATUS_IO_TIMEOUT);
    case SRB_STATUS_SELECTION_TIMEOUT:
        return(STATUS_DEVICE_NOT_CONNECTED);
    case SRB_STATUS_BAD_FUNCTION:
    case SRB_STATUS_BAD_SRB_BLOCK_LENGTH:
        return(STATUS_INVALID_DEVICE_REQUEST);
    case SRB_STATUS_DATA_OVERRUN:
        return(STATUS_BUFFER_OVERFLOW);
    default:
        return(STATUS_IO_DEVICE_ERROR);
    }

    return(STATUS_IO_DEVICE_ERROR);
}

#if defined(_X86_)
PMDL
SpBuildMdlForMappedTransfer(
    IN PDEVICE_OBJECT DeviceObject,
    IN PADAPTER_OBJECT AdapterObject,
    IN PMDL OriginalMdl,
    IN PVOID StartVa,
    IN ULONG ByteCount,
    IN PSRB_SCATTER_GATHER ScatterGatherList,
    IN ULONG ScatterGatherEntries
    );
#endif
#endif

#endif // __INTERRUPT__