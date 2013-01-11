te, IRP_CONTEXT_STATE_WAIT))) {

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

    None.

--*/

{
    ASSERT_IRP_CONTEXT(IrpContext);
    ASSERT_FCB(Fcb);

    PAGED_CODE();

    //
    //  We test that we currently hold the paging Io exclusive before releasing
    //  it. Checking the ExclusivePagingFcb in the IrpContext tells us if
    //  it is ours.
    //

    if ((IrpContext->TransactionId == 0) &&
        (IrpContext->CleanupStructure == Fcb)) {
        NtfsReleasePagingIo( IrpContext, Fcb );
    }

    NtfsReleaseFcb( IrpContext, Fcb );
}


VOID
NtfsReleaseScbWithPaging (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb
    )

/*++

Routine Description:

    This routine releases access to the Scb, including its
    paging I/O resource if it exists.

Arguments:

    Scb - Supplies the Fcb to acquire

Return Value:

    None.

--*/

{
    PFCB Fcb = Scb->Fcb;

    ASSERT_IRP_CONTEXT(IrpContext);
    ASSERT_SCB(Scb);

    PAGED_CODE();

    //
    //  Release the paging Io resource in the Scb under the following
    //  conditions.
    //
    //      - No transaction underway
    //      - This paging Io resource is in the IrpContext
    //          (This last test insures there is a paging IO resource
    //           and we own it).
    //

    if ((IrpContext->TransactionId == 0) &&
        (IrpContext->CleanupStructure == Fcb)) {
        NtfsReleasePagingIo( IrpContext, Fcb );
    }

    NtfsReleaseScb( IrpContext, Scb );
}


BOOLEAN
NtfsAcquireExclusiveFcb (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PSCB Scb OPTIONAL,
    IN ULONG AcquireFlags
    )

/*++

Routine Description:

    This routine acquires exclusive access to the Fcb.

    This routine will raise if it cannot acquire the resource and wait
    in the IrpContext is false.

Arguments:

    Fcb - Supplies the Fcb to acquire

    Scb - This is the Scb for which we are acquiring the Fcb

    AcquireFlags - Indicating whether to override the wait value in the IrpContext.  Also whether
        to noop the check for a deleted file.

Return Value:

    BOOLEAN - TRUE if acquired.  FALSE otherwise.

--*/

{
    NTSTATUS Status;
    BOOLEAN Wait;

    ASSERT_IRP_CONTEXT(IrpContext);
    ASSERT_FCB(Fcb);

    PAGED_CODE();

    Status = STATUS_CANT_WAIT;

    if (FlagOn( AcquireFlags, ACQUIRE_DONT_WAIT )) {
        Wait = FALSE;       
    } else if (FlagOn( AcquireFlags, ACQUIRE_WAIT )) {
        Wait = TRUE;
    } else {
        Wait = BooleanFlagOn( IrpContext->State, IRP_CONTEXT_STATE_WAIT );
    }

    if (NtfsAcquireResourceExclusive( IrpContext, Fcb, Wait )) {

        //
        //  The link count should be non-zero or the file has been
        //  deleted.  We allow deleted files to be acquired for close and
        //  also allow them to be acquired recursively in case we
        //  acquire them a second time after marking them deleted (i.e. rename)
        //

        if (FlagOn( AcquireFlags, ACQUIRE_NO_DELETE_CHECK )

            ||

            (!FlagOn( Fcb->FcbState, FCB_STATE_FILE_DELETED )
             && (!ARGUMENT_PRESENT( Scb )
                 || !FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED )))

            ||

            (IrpContext->MajorFunction == IRP_MJ_CLOSE)

            ||

            (IrpContext->MajorFunction == IRP_MJ_CREATE)

            ||

            (IrpContext->MajorFunction == IRP_MJ_CLEANUP)) {

            //
            //  Put Fcb in the exclusive Fcb list for this IrpContext,
            //  excluding the bitmap for the volume, since we do not need
            //  to modify its file record and do not want unnecessary
            //  serialization/deadlock problems.
            //
            //  If we are growing the volume bitmap then we do want to put
            //  it on the list and maintain the BaseExclusiveCount.  Also
            //  need to do this in the case where we see the volume bitmap
            //  during close (it can happen during restart if we have log
            //  records for the volume bitmap).
            //

            //
            //  If Fcb already acquired then bump the count.
            //

            if (Fcb->ExclusiveFcbLinks.Flink != NULL) {

                Fcb->BaseExclusiveCount += 1;

            //
            //  The fcb is not currently on an exclusive list.
            //  Put it on a list if this is not the volume
            //  bitmap or we explicitly want to put the volume
            //  bitmap on the list.
            //

            } else if (FlagOn( AcquireFlags, ACQUIRE_HOLD_BITMAP ) ||
                       (Fcb->Vcb->BitmapScb == NULL) ||
                       (Fcb->Vcb->BitmapScb->Fcb != Fcb)) {

                ASSERT( Fcb->BaseExclusiveCount == 0 );

                InsertHeadList( &IrpContext->ExclusiveFcbList,
                                &Fcb->ExclusiveFcbLinks );

                Fcb->BaseExclusiveCount += 1;
            }

            return TRUE;
        }

        //
        //  We need to release the Fcb and remember the status code.
        //

        NtfsReleaseResource( IrpContext, Fcb );
        Status = STATUS_FILE_DELETED;

    } else if (FlagOn( AcquireFlags, ACQUIRE_DONT_WAIT )) {

        return FALSE;
    }

    NtfsRaiseStatus( IrpContext, Status, NULL, NULL );
}


VOID
NtfsAcquireSharedFcb (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PSCB Scb OPTIONAL,
    IN ULONG AcquireFlags
    )

/*++

Routine Description:

    This routine acquires shared access to the Fcb.

    This routine will raise if it cannot acquire the resource and wait
    in the IrpContext is false.

Arguments:

    Fcb - Supplies the Fcb to acquire

    Scb - This is the Scb for which we are acquiring the Fcb

    AcquireFlags - Indicates if we should acquire the file even if it has been
        deleted.

Return Value:

    None.

--*/

{
    NTSTATUS Status;
    ASSERT_IRP_CONTEXT(IrpContext);
    ASSERT_FCB(Fcb);

    Status = STATUS_CANT_WAIT;

    if (NtfsAcquireResourceShared( IrpContext, Fcb, (BOOLEAN) FlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT))) {

        //
        //  The link count should be non-zero or the file has been
        //  deleted.
        //

        if (FlagOn( AcquireFlags, ACQUIRE_NO_DELETE_CHECK ) ||
            (!FlagOn( Fcb->FcbState, FCB_STATE_FILE_DELETED ) &&
             (!ARGUMENT_PRESENT( Scb ) ||
              !FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED )))) {

            //
            //  It's possible that this is a recursive shared aquisition of an
            //  Fcb we own exclusively at the top level.  In that case we
            //  need to bump the acquisition count.
            //

            if (Fcb->ExclusiveFcbLinks.Flink != NULL) {

                Fcb->BaseExclusiveCount += 1;
            }

            return;
        }

        //
        //  We need to release the Fcb and remember the status code.
        //

        NtfsReleaseResource( IrpContext, Fcb );
        Status = STATUS_FILE_DELETED;
    }

    NtfsRaiseStatus( IrpContext, Status, NULL, NULL );
}


BOOLEAN
NtfsAcquireSharedFcbCheckWait (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN ULONG AcquireFlags
    )

/*++

Routine Description:

    This routine acquires shared access to the Fcb but checks whether to wait.

Arguments:

    Fcb - Supplies the Fcb to acquire

    AcquireFlags - Indicates if we should override the wait value in the IrpContext.
        We won't wait for the resource and return whether the resource
        was acquired.

Return Value:

    BOOLEAN - TRUE if acquired.  FALSE otherwise.

--*/

{
    BOOLEAN Wait;
    PAGED_CODE();

    if (FlagOn( AcquireFlags, ACQUIRE_DONT_WAIT )) {
        Wait = FALSE;       
    } else if (FlagOn( AcquireFlags, ACQUIRE_WAIT )) {
        Wait = TRUE;
    } else {
        Wait = BooleanFlagOn( IrpContext->State, IRP_CONTEXT_STATE_WAIT );
    }

    if (NtfsAcquireResourceShared( IrpContext, Fcb, Wait )) {

        //
        //  It's possible that this is a recursive shared aquisition of an
        //  Fcb we own exclusively at the top level.  In that case we
        //  need to bump the acquisition count.
        //

        if (Fcb->ExclusiveFcbLinks.Flink != NULL) {

            Fcb->BaseExclusiveCount += 1;
        }

        return TRUE;

    } else {

        return FALSE;
    }
}


VOID
NtfsReleaseFcb (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb
    )

/*++

Routine Description:

    This routine releases the specified Fcb resource.  If the Fcb is acquired
    exclusive, and a transaction is still active, then the release is nooped
    in order to preserve two-phase locking.  If there is no longer an active
    transaction, then we remove the Fcb from the Exclusive Fcb List off the
    IrpContext, and clear the Flink as a sign.  Fcbs are released when the
    transaction is commited.

Arguments:

    Fcb - Fcb to release

Return Value:

    None.

--*/

{
    //
    //  Check if this resource is owned exclusively and we are at the last
    //  release for this transaction.
    //

    if (Fcb->ExclusiveFcbLinks.Flink != NULL) {

        if (Fcb->BaseExclusiveCount == 1) {

            //
            //  If there is a transaction then noop this request.
            //

            if (IrpContext->TransactionId != 0) {

                return;
            }

            RemoveEntryList( &Fcb->ExclusiveFcbLinks );
            Fcb->ExclusiveFcbLinks.Flink = NULL;

            //
            //  This is a good time to free any Scb snapshots for this Fcb.
            //

            NtfsFreeSnapshotsForFcb( IrpContext, Fcb );
        }

        Fcb->BaseExclusiveCount -= 1;
    }

    ASSERT((Fcb->ExclusiveFcbLinks.Flink == NULL && Fcb->BaseExclusiveCount == 0) ||
           (Fcb->ExclusiveFcbLinks.Flink != NULL && Fcb->BaseExclusiveCount != 0));

    NtfsReleaseResource( IrpContext, Fcb );
}


VOID
NtfsAcquireExclusiveScb (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb
    )

/*++

Routine Description:

    This routine acquires exclusive access to the Scb.

    This routine will raise if it cannot acquire the resource and wait
    in the IrpContext is false.

Arguments:

    Scb - Scb to acquire

Return Value:

    None.

--*/

{
    PAGED_CODE();

    NtfsAcquireExclusiveFcb( IrpContext, Scb->Fcb, Scb, 0 );

    ASSERT( Scb->Fcb->ExclusiveFcbLinks.Flink != NULL
            || (Scb->Vcb->BitmapScb != NULL
                && Scb->Vcb->BitmapScb == Scb) );

#ifdef __ND_NTFS_SECONDARY__
	if (!FlagOn(Scb->Fcb->NdNtfsFlags, ND_NTFS_FCB_FLAG_SECONDARY))
#endif
    if (FlagOn(Scb->ScbState, SCB_STATE_FILE_SIZE_LOADED)) {

        NtfsSnapshotScb( IrpContext, Scb );
    }
}


VOID
NtfsAcquireSharedScbForTransaction (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb
    )

/*++

Routine Description:

    This routine is called to acquire an Scb shared in order to perform updates to
    the an Scb stream.  This is used if the transaction writes to a range of the
    stream without changing the size or position of the data.  The caller must
    already provide synchronization to the data itself.

    There is no corresponding Scb release.  It will be released when the transaction commits.
    We will acquire the Scb exclusive if it is not yet in the open attribute table.

Arguments:

    Scb - Scb to acquire

Return Value:

    None.

--*/

{
    PSCB *Position;
    PSCB *ScbArray;
    ULONG Count;

    PAGED_CODE();

    //
    //  Make sure we have a free spot in the Scb array in the IrpContext.
    //

    if (IrpContext->SharedScb == NULL) {

        Position = (PSCB *) &IrpContext->SharedScb;
        IrpContext->SharedScbSize = 1;

    //
    //  Too bad the first one is not available.  If the current size is one then allocate a
    //  new block and copy the existing value to it.
    //

    } else if (IrpContext->SharedScbSize == 1) {

        ScbArray = NtfsAllocatePool( PagedPool, sizeof( PSCB ) * 4 );
        RtlZeroMemory( ScbArray, sizeof( PSCB ) * 4 );
        *ScbArray = IrpContext->SharedScb;
        IrpContext->SharedScb = ScbArray;
        IrpContext->SharedScbSize = 4;
        Position = ScbArray + 1;

    //
    //  Otherwise look through the existing array and look for a free spot.  Allocate a larger
    //  array if we need to grow it.
    //

    } else {

        Position = IrpContext->SharedScb;
        Count = IrpContext->SharedScbSize;

        do {

            if (*Position == NULL) {

                break;
            }

            Count -= 1;
            Position += 1;

        } while (Count != 0);

        //
        //  If we didn't find one then allocate a new structure.
        //

        if (Count == 0) {

            ScbArray = NtfsAllocatePool( PagedPool, sizeof( PSCB ) * IrpContext->SharedScbSize * 2 );
            RtlZeroMemory( ScbArray, sizeof( PSCB ) * IrpContext->SharedScbSize * 2 );
            RtlCopyMemory( ScbArray,
                           IrpContext->SharedScb,
                           sizeof( PSCB ) * IrpContext->SharedScbSize );

            NtfsFreePool( IrpContext->SharedScb );
            IrpContext->SharedScb = ScbArray;
            Position = ScbArray + IrpContext->SharedScbSize;
            IrpContext->SharedScbSize *= 2;
        }
    }

    NtfsAcquireResourceShared( IrpContext, Scb, TRUE );

    if (Scb->NonpagedScb->OpenAttributeTableIndex == 0) {

        NtfsReleaseResource( IrpContext, Scb );
        NtfsAcquireResourceExclusive( IrpContext, Scb, TRUE );
    }

    *Position = Scb;

    return;
}


VOID
NtfsReleaseSharedResources (
    IN PIRP_CONTEXT IrpContext
    )

/*++

Routine Description:

    The routine releases all of the resources acquired shared for
    transaction.  The SharedScb structure is freed if necessary and
    the Irp Context field is cleared.

Arguments:


Return Value:

    None.

--*/
{

    PAGED_CODE();

    //
    //  If only one then free the Scb main resource.
    //

    if (IrpContext->SharedScbSize == 1) {

        if (SafeNodeType(IrpContext->SharedScb) == NTFS_NTC_QUOTA_CONTROL) {
            NtfsReleaseQuotaControl( IrpContext,
                              (PQUOTA_CONTROL_BLOCK) IrpContext->SharedScb );
        } else {
    
            NtfsReleaseResource( IrpContext, ((PSCB) IrpContext->SharedScb) );
        }

    //
    //  Otherwise traverse the array and look for Scb's to release.
    //

    } else {

        PSCB *NextScb;
        ULONG Count;

        NextScb = IrpContext->SharedScb;
        Count = IrpContext->SharedScbSize;

        do {

            if (*NextScb != NULL) {

                if (SafeNodeType(*NextScb) == NTFS_NTC_QUOTA_CONTROL) {

                    NtfsReleaseQuotaControl( IrpContext,
                                      (PQUOTA_CONTROL_BLOCK) *NextScb );
                } else {

                    NtfsReleaseResource( IrpContext, (*NextScb) );
                }
                *NextScb = NULL;
            }

            Count -= 1;
            NextScb += 1;

        } while (Count != 0);

        NtfsFreePool( IrpContext->SharedScb );
    }

    IrpContext->SharedScb = NULL;
    IrpContext->SharedScbSize = 0;

}


VOID
NtfsReleaseAllResources (
    IN PIRP_CONTEXT IrpContext
    )
/*++

Routine Description:

    This routine release all resources tracked in the irpcontext including
    exclusive fcb, paging / locked headers in the cleanup structure / cached file records
    shared resources / quota blocks acquired for transactions
    
    Does not release the vcb since this is hand-tracked.
    Not paged since called by NtfsCleanupIrpContext which is not paged
    

Arguments:


Return Value:

    None

--*/

{
    PFCB Fcb;

    //
    //  Release the cached file record map
    //

    NtfsPurgeFileRecordCache( IrpContext );


#ifdef MAPCOUNT_DBG
    //
    //  Check all mapping are gone now that we cleaned out cache
    //

    ASSERT( IrpContext->MapCount == 0 );

#endif

    //
    //  Go through and free any Scb's in the queue of shared Scb's for transactions.
    //

    if (IrpContext->SharedScb != NULL) {

        NtfsReleaseSharedResources( IrpContext );
    }

    //
    //  Free any exclusive paging I/O resource, or IoAtEof condition,
    //  this field is overlayed, minimally in write.c.
    //

    Fcb = IrpContext->CleanupStructure;
    if (Fcb != NULL) {

        if (Fcb->NodeTypeCode == NTFS_NTC_FCB) {

            NtfsReleasePagingIo( IrpContext, Fcb );

        } else {

            FsRtlUnlockFsRtlHeader( (PNTFS_ADVANCED_FCB_HEADER) Fcb );
            IrpContext->CleanupStructure = NULL;
        }
    }

    //
    //  Finally, now that we have written the forget record, we can free
    //  any exclusive Scbs that we have been holding.
    //

    ASSERT( IrpContext->TransactionId == 0 );

    while (!IsListEmpty( &IrpContext->ExclusiveFcbList )) {

        Fcb = (PFCB)CONTAINING_RECORD( IrpContext->ExclusiveFcbList.Flink,
                                       FCB,
                                       ExclusiveFcbLinks );

        NtfsReleaseFcb( IrpContext, Fcb );
    }

    ClearFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_RELEASE_USN_JRNL |
                                  IRP_CONTEXT_FLAG_RELEASE_MFT );
}


VOID
NtfsAcquireIndexCcb (
    IN PSCB Scb,
    IN PCCB Ccb,
    IN PEOF_WAIT_BLOCK EofWaitBlock
    )

/*++

Routine Description:

    This routine is called to serialize access to a Ccb for a directory.
    We must serialize access to the index context or we will corrupt
    the data structure.

Arguments:

    Scb - Scb for the directory to enumerate.

    Ccb - Pointer to the Ccb for the handle.

    EofWaitBlock - Uninitialized structure used only to serialize Eof updates.  Our
        caller will put this on the stack.

Return Value:

    None

--*/

{
    PAGED_CODE();

    //
    //  Acquire the mutex for serialization.
    //

    NtfsAcquireFsrtlHeader( Scb );
    
    //
    //  Typical case is that we are the only active handle.
    //

    if (Ccb->EnumQueue.Flink == NULL) {

        InitializeListHead( &Ccb->EnumQueue );
        NtfsReleaseFsrtlHeader( Scb );

    } else {

        //
        //  Initialize our event an put ourselves on the stack.
        //

        KeInitializeEvent( &EofWaitBlock->Event, NotificationEvent, FALSE );
        InsertTailList( &Ccb->EnumQueue, &EofWaitBlock->EofWaitLinks );

        //
        //  Free the mutex and wait.  When the wait is satisfied then we are
        //  the active handle.
        //

        NtfsReleaseFsrtlHeader( Scb );
        
        KeWaitForSingleObject( &EofWaitBlock->Event,
                               Executive,
                               KernelMode,
                               FALSE,
                               (PLARGE_INTEGER)NULL);
    }

    return;
}


VOID
NtfsReleaseIndexCcb (
    IN PSCB Scb,
    IN PCCB Ccb
    )

/*++

Routine Description:

    This routine is called to release a Ccb for other people to access.

Arguments:

    Scb - Scb for the directory to enumerate.
    Ccb - Pointer to the Ccb for the handle.

Return Value:

    None

--*/

{
    PEOF_WAIT_BLOCK EofWaitBlock;
    PAGED_CODE();

    //
    //  Acquire the header and wake the next waiter or clear the list if it
    //  is now empty.
    //

    NtfsAcquireFsrtlHeader( Scb );
    
    ASSERT( Ccb->EnumQueue.Flink != NULL );
    if (IsListEmpty( &Ccb->EnumQueue )) {

        Ccb->EnumQueue.Flink = NULL;

    } else {

        EofWaitBlock = (PEOF_WAIT_BLOCK) RemoveHeadList( &Ccb->EnumQueue );
        KeSetEvent( &EofWaitBlock->Event, 0, FALSE );
    }

    NtfsReleaseFsrtlHeader( Scb );
    return;
}


BOOLEAN
NtfsAcquireScbForLazyWrite (
    IN PVOID OpaqueScb,
    IN BOOLEAN Wait
    )

/*++

Routine Description:

    The address of this routine is specified when creating a CacheMap for
    a file.  It is subsequently called by the Lazy Writer prior to its
    performing lazy writes to the file.  This callback is necessary to
    avoid deadlocks with the Lazy Writer.  (Note that normal writes
    acquire the Fcb, and then call the Cache Manager, who must acquire
    some of his internal structures.  If the Lazy Writer could not call
    this routine first, and were to issue a write after locking Caching
    data structures, then a deadlock could occur.)

Arguments:

    OpaqueScb - The Scb which was specified as a context parameter for this
                routine.

    Wait - TRUE if the caller is willing to block.

Return Value:

    FALSE - if Wait was specified as FALSE and blocking would have
            been required.  The Fcb is not acquired.

    TRUE - if the Scb has been acquired

--*/

{
    BOOLEAN AcquiredFile = FALSE;

    ULONG CompressedStream = (ULONG)((ULONG_PTR)OpaqueScb & 1);
    PSCB Scb = (PSCB)((ULONG_PTR)OpaqueScb & ~1);
    PFCB Fcb = Scb->Fcb;

#ifdef __ND_NTFS_SECONDARY__

	BOOLEAN secondaryResourceAcquired = FALSE;
	PVOLUME_DEVICE_OBJECT	volDo = CONTAINING_RECORD( Scb->Vcb, VOLUME_DEVICE_OBJECT, Vcb );

#endif

    ASSERT_SCB(Scb);

    PAGED_CODE();

#ifdef __ND_NTFS_SECONDARY__

	if (FlagOn(Fcb->NdNtfsFlags, ND_NTFS_FCB_FLAG_SECONDARY)) {

		secondaryResourceAcquired 
			= SecondaryAcquireResourceSharedLite( NULL, 
												  &volDo->Secondary->Resource, 
												  (IoGetTopLevelIrp() == NULL) ? Wait : 0 );

		if (secondaryResourceAcquired == FALSE) {

				return FALSE;
		}
	}

#endif

    //
    //  Acquire the Scb only for those files that the write will
    //  acquire it for, i.e., not the first set of system files.
    //  Otherwise we can deadlock, for example with someone needing
    //  a new Mft record.
    //

    if (NtfsSegmentNumber( &Fcb->FileReference ) <= MASTER_FILE_TABLE2_NUMBER) {

        //
        //  We need to synchronize the lazy writer with the clean volume
        //  checkpoint.  We do this by acquiring and immediately releasing this
        //  Scb.  This is to prevent the lazy writer from flushing the log file
        //  when the space may be at a premium.
        //

        if (NtfsAcquireResourceShared( NULL, Scb, Wait )) {

            if (ExAcquireResourceSharedLite( &Scb->Vcb->MftFlushResource, Wait )) {
                //
                //  The mft bitmap will reacquire the mft resource in LookupAllocation
                //  if its not loaded during a write - this would deadlock with allocating
                //  a mft record.  bcb exclusive - mft main vs mft main - bcb shared
                // 

                ASSERT( (Scb != Scb->Vcb->MftBitmapScb) ||

                        ((Scb->Mcb.NtfsMcbArraySizeInUse > 0) &&
                         ((Scb->Mcb.NtfsMcbArray[ Scb->Mcb.NtfsMcbArraySizeInUse - 1].EndingVcn + 1) == 
                          LlClustersFromBytes( Scb->Vcb, Scb->Header.AllocationSize.QuadPart ))) );
                
                AcquiredFile = TRUE;
            } 
            NtfsReleaseResource( NULL, Scb );
        } 
    //
    //  Now acquire either the main or paging io resource depending on the
    //  state of the file.
    //

    } else if (Scb->Header.PagingIoResource != NULL) {
        AcquiredFile = ExAcquireResourceSharedLite( Scb->Header.PagingIoResource, Wait );
    } else {

        if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_RESIDENT | SCB_STATE_CONVERT_UNDERWAY )) {

            AcquiredFile = NtfsAcquireResourceExclusive( NULL, Scb, Wait );

        } else {

            AcquiredFile = NtfsAcquireResourceShared( NULL, Scb, Wait );
        }
    }

    if (AcquiredFile) {

        //
        // We assume the Lazy Writer only acquires this Scb once.  When he
        // has acquired it, then he has eliminated anyone who would extend
        // valid data, since they must take out the resource exclusive.
        // Therefore, it should be guaranteed that this flag is currently
        // clear (the ASSERT), and then we will set this flag, to insure
        // that the Lazy Writer will never try to advance Valid Data, and
        // also not deadlock by trying to get the Fcb exclusive.
        //

        ASSERT( Scb->LazyWriteThread[CompressedStream] == NULL );

        Scb->LazyWriteThread[CompressedStream] = PsGetCurrentThread();

        //
        //  Make Cc top level, so that we will not post or retry on errors.
        //  (If it is not NULL, it must be one of our internal calls to this
        //  routine, such as from Restart or Hot Fix.)
        //

        if (IoGetTopLevelIrp() == NULL) {
            IoSetTopLevelIrp( (PIRP)FSRTL_CACHE_TOP_LEVEL_IRP );
        }
    }

#ifdef __ND_NTFS_SECONDARY__

	if (AcquiredFile == FALSE && FlagOn(Fcb->NdNtfsFlags, ND_NTFS_FCB_FLAG_SECONDARY)) {

		SecondaryReleaseResourceLite( NULL, &volDo->Secondary->Resource );
	}

#endif

    return AcquiredFile;
}


VOID
NtfsReleaseScbFromLazyWrite (
    IN PVOID OpaqueScb
    )

/*++

Routine Description:

    The address of this routine is specified when creating a CacheMap for
    a file.  It is subsequently called by the Lazy Writer after its
    performing lazy writes to the file.

Arguments:

    Scb - The Scb which was specified as a context parameter for this
          routine.

Return Value:

    None

--*/

{
    ULONG CompressedStream = (ULONG)((ULONG_PTR)OpaqueScb & 1);
    PSCB Scb = (PSCB)((ULONG_PTR)OpaqueScb & ~1);
    PFCB Fcb = Scb->Fcb;
    ULONG CleanCheckpoint = 0;

#ifdef __ND_NTFS_SECONDARY__
	PVOLUME_DEVICE_OBJECT	volDo = CONTAINING_RECORD( Scb->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
#endif

    ASSERT_SCB(Scb);

    PAGED_CODE();

    //
    //  Clear the toplevel at this point, if we set it above.
    //

    if ((((ULONG_PTR) IoGetTopLevelIrp()) & ~0x80000000) == FSRTL_CACHE_TOP_LEVEL_IRP) {

        //
        //  We use the upper bit of this field to indicate that we need to
        //  do a clean checkpoint.
        //

        CleanCheckpoint = (ULONG)FlagOn( (ULONG_PTR) IoGetTopLevelIrp(), 0x80000000 );
        IoSetTopLevelIrp( NULL );
    }

    Scb->LazyWriteThread[CompressedStream] = NULL;

    if (NtfsSegmentNumber( &Fcb->FileReference ) <= MASTER_FILE_TABLE2_NUMBER) {

        ExReleaseResourceLite( &Scb->Vcb->MftFlushResource );

    } else if (Scb->Header.PagingIoResource != NULL) {
        ExReleaseResourceLite( Scb->Header.PagingIoResource );
    } else {
        NtfsReleaseResource( NULL, Scb );
    }

    //
    //  Do a clean checkpoint if necessary.
    //

    if (CleanCheckpoint) {

        NtfsCleanCheckpoint( Scb->Vcb );
    }

#ifdef __ND_NTFS_SECONDARY__
	if (FlagOn(Fcb->NdNtfsFlags, ND_NTFS_FCB_FLAG_SECONDARY)) {

		SecondaryReleaseResourceLite( NULL, &volDo->Secondary->Resource );
	}
#endif

    return;
}


NTSTATUS
NtfsAcquireFileForModWrite (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER EndingOffset,
    OUT PERESOURCE *ResourceToRelease,
    IN PDEVICE_OBJECT DeviceObject
    )

{
    BOOLEAN AcquiredFile;

    PSCB Scb = (PSCB) (FileObject->FsContext);
    PFCB Fcb = Scb->Fcb;

    ASSERT_SCB(Scb);

    UNREFERENCED_PARAMETER( DeviceObject );

    PAGED_CODE();

    //
    //  Acquire the Scb only for those files that the write will
    //  acquire it for, i.e., not the first set of system files.
    //  Otherwise we can deadlock, for example with someone needing
    //  a new Mft record.
    //

    if (NtfsSegmentNumber( &Fcb->FileReference ) <= MASTER_FILE_TABLE2_NUMBER) {

        //
        //  We need to synchronize the lazy writer with the clean volume
        //  checkpoint.  We do this by acquiring and immediately releasing this
        //  Scb.  This is to prevent the lazy writer from flushing the log file
        //  when the space may be at a premium.
        //

        if (AcquiredFile = ExAcquireResourceSharedLite( Scb->Header.Resource, FALSE )) {
            ExReleaseResourceLite( Scb->Header.Resource );
        }
        *ResourceToRelease = NULL;

    //
    //  Now acquire either the main or paging io resource depending on the
    //  state of the file.
    //

    } else {

        //
        //  Figure out which resource to acquire.
        //

        if (Scb->Header.PagingIoResource != NULL) {
            *ResourceToRelease = Scb->Header.PagingIoResource;
        } else {
            *ResourceToRelease = Scb->Header.Resource;
        }

        if (Scb->AttributeTypeCode == $EA) {

            AcquiredFile = ExAcquireResourceExclusiveLite( *ResourceToRelease, FALSE );

        } else {

            //
            //  Try to acquire the resource with Wait FALSE
            //

            AcquiredFile = ExAcquireResourceSharedLite( *ResourceToRelease, FALSE );
        }

        //
        //  If we got the resource, check if he is possibly trying to extend
        //  ValidDataLength.  If so that will cause us to go into useless mode
        //  possibly doing actual I/O writing zeros out to the file past actual
        //  valid data in the cache.  This is so inefficient that it is better
        //  to tell MM not to do this write.
        //

        if (AcquiredFile) {
            if (FlagOn( Scb->AttributeFlags, ATTRIBUTE_FLAG_COMPRESSION_MASK )) {
                NtfsAcquireFsrtlHeader( Scb );
                if ((EndingOffset->QuadPart > Scb->ValidDataToDisk) &&
                    (EndingOffset->QuadPart < Scb->Header.FileSize.QuadPart) &&
                    !FlagOn(Scb->Header.Flags, FSRTL_FLAG_USER_MAPPED_FILE)) {

                    ExReleaseResourceLite(*ResourceToRelease);
                    AcquiredFile = FALSE;
                    *ResourceToRelease = NULL;
                }
                NtfsReleaseFsrtlHeader( Scb );
            }
        } else {
            *ResourceToRelease = NULL;
        }
    }

    return (AcquiredFile ? STATUS_SUCCESS : STATUS_CANT_WAIT);
}

NTSTATUS
NtfsAcquireFileForCcFlush (
    IN PFILE_OBJECT FileObject,
    IN PDEVICE_OBJECT DeviceObject
    )
{
    PFSRTL_COMMON_FCB_HEADER Header = FileObject->FsContext;

    PAGED_CODE();

    if (Header->PagingIoResource != NULL) {
        ExAcquireResourceSharedLite( Header->PagingIoResource, TRUE );
    }

    return STATUS_SUCCESS;

    UNREFERENCED_PARAMETER( DeviceObject );
}

NTSTATUS
NtfsReleaseFileForCcFlush (
    IN PFILE_OBJECT FileObject,
    IN PDEVICE_OBJECT DeviceObject
    )
{
    PSCB Scb = (PSCB) FileObject->FsContext;
    BOOLEAN CleanCheckpoint = FALSE;

    PAGED_CODE();

    if (Scb->Header.PagingIoResource != NULL) {

        //
        //  If we are getting repeated log file fulls then we want to process that before retrying
        //  this request.  This will prevent a section flush from failing and returning
        //  STATUS_FILE_LOCK_CONFLICT to the user.
        //

        if (Scb->Vcb->CleanCheckpointMark + 3 < Scb->Vcb->LogFileFullCount) {

            CleanCheckpoint = TRUE;
        }

        ExReleaseResourceLite( Scb->Header.PagingIoResource );

        //
        //  We may be be in a recursive acquisition callback in that case even
        //  after releasing the resource we may still own it and be unable to
        //  checkpoint
        //

        if (CleanCheckpoint &&
            (IoGetTopLevelIrp() == NULL) &&
            !NtfsIsExclusiveScbPagingIo( Scb )) {

            NtfsCleanCheckpoint( Scb->Vcb );
        }
    }

    return STATUS_SUCCESS;

    UNREFERENCED_PARAMETER( DeviceObject );
}

VOID
NtfsAcquireForCreateSection (
    IN PFILE_OBJECT FileObject
    )

{
    PSCB Scb = (PSCB)FileObject->FsContext;

    PAGED_CODE();

    if (Scb->Header.PagingIoResource != NULL) {

        //
        //  Use an unsafe test to see if a dummy checkpoint has been posted.
        //  We can use an un pP
 ˆÿÿ       Ê€ë3À;Æ£DŒtÿ@jZ‰}ü_S‰=HŒÇLŒ(ä€fÇPŒ è,¢ùÿ;ÆYt‰pÇ ÔÊ€ë3À;Æ£TŒtÿ@j[‰}ü_S‰=XŒÇ\Œä€fÇ`Œ èé¡ùÿ;ÆYt‰pÇ üÊ€ë3À;Æ£dŒtÿ@j\‰}ü_S‰=hŒÇlŒôã€fÇpŒ è¦¡ùÿ;ÆYt‰pÇ $Ë€ë3À;Æ£tŒtÿ@j]‰}ü_S‰=xŒÇ|Œàã€fÇ€Œ èc¡ùÿ;ÆYt‰pÇ LË€ë3À;Æ£„Œtÿ@j^‰}ü_S‰=ˆŒÇŒŒ ã€fÇŒ è ¡ùÿ;ÆYt‰pÇ tË€ë3À;Æ£”Œtÿ@j_‰}ü_S‰=˜ŒÇœŒŒã€fÇ Œ èİ ùÿ;ÆYt‰pÇ œË€ë3À;Æ£¤Œtÿ@j`‰}ü_S‰=¨ŒÇ¬ŒPã€fÇ°Œ èš ùÿ;ÆYt‰pÇ ÄË€ë3À;Æ£´Œtÿ@ja‰}ü_S‰=¸ŒÇ¼Œ8ã€fÇÀŒ èW ùÿ;ÆYt‰pÇ ìË€ë3À;Æ£ÄŒtÿ@jb‰}ü_S‰=ÈŒÇÌŒã€fÇĞŒ è ùÿ;ÆYt‰pÇ Ì€ë3À;Æ£ÔŒtÿ@jc‰}ü_S‰=ØŒÇÜŒôâ€fÇàŒ èÑŸùÿ;ÆYt‰pÇ <Ì€ë3À;Æ£äŒtÿ@jd‰}ü_S‰=èŒÇìŒÔâ€fÇğŒ èŸùÿ;ÆYt‰pÇ dÌ€ë3À;Æ£ôŒtÿ@je‰}ü_S‰=øŒÇüŒ¸â€fÇ  èKŸùÿ;ÆYt‰pÇ ŒÌ€ë3À;Æ£tÿ@jf‰}ü_S‰=Çâ€fÇ èŸùÿ;ÆYt‰pÇ ´Ì€ë3À;Æ£tÿ@jg‰}ü_S‰=ÇTâ€fÇ  èÅùÿ;ÆYt‰pÇ ÜÌ€ë3À;Æ£$tÿ@jh‰}ü_S‰=(Ç,0â€fÇ0 è‚ùÿ;ÆYt‰pÇ Í€ë3À;Æ£4tÿ@ji‰}ü_S‰=8Ç<â€f‰@èAùÿ;ÆYt‰pÇ ,Í€ë3À;Æ£Dtÿ@jj‰}ü_S‰=HÇL â€fÇP èşùÿ;ÆYt‰pÇ TÍ€ë3À;Æ£Ttÿ@jk‰}ü_S‰=XÇ\èá€fÇ` è»ùÿ;ÆYt‰pÇ |Í€ë3À;Æ£dtÿ@jl‰}ü_S‰=hÇlĞá€fÇp èxùÿ;ÆYt‰pÇ ¤Í€ë3À;Æ£ttÿ@jm‰}ü_S‰=xÇ|¬á€fÇ€ è5ùÿ;ÆYt‰pÇ ÌÍ€ë3À;Æ£„tÿ@jn‰}ü_S‰=ˆÇŒ˜á€fÇ èòœùÿ;ÆYt‰pÇ ôÍ€ë3À;Æ£”tÿ@jo‰}ü_S‰=˜Çœxá€fÇ  è¯œùÿ;ÆYt‰pÇ Î€ë3À;Æ£¤tÿ@jp‰}ü_S‰=¨Ç¬Tá€fÇ° èlœùÿ;ÆYt‰pÇ DÎ€ë3À;Æ£´tÿ@jq‰}ü_S‰=¸Ç¼Dá€fÇÀ è)œùÿ;ÆYt‰pÇ lÎ€ë3À;Æ£Ätÿ@jr‰}ü_S‰=ÈÇÌ(á€fÇĞ èæ›ùÿ;ÆYt‰pÇ ”Î€ë3À;Æ£Ôtÿ@js‰}ü_S‰=ØÇÜá€fÇà è£›ùÿ;ÆYt‰pÇ ¼Î€ë3À;Æ£ätÿ@S‰}üÇèt   Çìüà€fÇğ è_›ùÿ;ÆYt‰pÇ äÎ€ë3À_;Æ^£ô[tÿ@hbôŒè œùÿY‹Môd‰    ÉÃh   €¹ìèãıÿhvôŒèÚ›ùÿYÃh   €¹$èÿâıÿh‚ôŒè¿›ùÿYÃh   €¹<èäâıÿhôŒè¤›ùÿYÃhšôŒè˜›ùÿYÃh§ôŒèŒ›ùÿYÃh´ôŒè€›ùÿYÃhÒ<‡jjhPè›OÿÿÃhÒ<‡j
jhÀ‚è‡OÿÿÃhÒ<‡jjh`ƒèsOÿÿÃhÒ<‡jjhĞƒè_OÿÿÃhÒ<‡jjh …èKOÿÿÃhÒ<‡jjhøè7OÿÿÃhÒ<‡jjh¨è#OÿÿÃhÒ<‡jjhèOÿÿÃhÒ<‡jujh¨†èûNÿÿÃhìÿ„€Ãh$ÿ„€Ãh<ÿ„€Ãÿ5‘èõÿYÃÿ5‘èşŒõÿYÃ¹TéßœşÿÌÌÿÿÿÿ    ÿ    ÿÿÿÿ    ±7‚    ÿÿÿÿ    ¾H‚    ÿÿÿÿf‚f‚    ÿÿÿÿlg‚rg‚    ÿÿÿÿj‚j‚    ÿÿÿÿc‚s‚    ÿÿÿÿî‚ô‚    ÿÿÿÿ2É‚EÉ‚    ÿÿÿÿ+Ë‚;Ë‚    ÿÿÿÿ    ÖË‚    ÿÿÿÿÄÕ‚ÔÕ‚    ÿÿÿÿì×‚ü×‚    ÿÿÿÿiÚ‚|Ú‚    ÿÿÿÿ&İ‚6İ‚    ÿÿÿÿ£à‚¶à‚    ÿÿÿÿ_æ‚oæ‚    ÿÿÿÿpë‚€ë‚    ÿÿÿÿ´î‚Çî‚    ÿÿÿÿÜõ‚ïõ‚    ÿÿÿÿdù‚tù‚    ÿÿÿÿ”ş‚¤ş‚    ÿÿÿÿ†ÿ‚–ÿ‚    ÿÿÿÿªƒºƒ    ÿÿÿÿƒƒ    ÿÿÿÿ2ƒBƒ    ÿÿÿÿî	ƒş	ƒ    ÿÿÿÿİ
ƒí
ƒ    ÿÿÿÿ´ƒÄƒ    ÿÿÿÿƒ.ƒ    ÿÿÿÿƒ.ƒ    ÿÿÿÿMƒ]ƒ    ÿÿÿÿGƒZƒ    ÿÿÿÿƒ%ƒ    ÿÿÿÿÕƒåƒ    ÿÿÿÿŞƒîƒ    ÿÿÿÿàƒğƒ    ÿÿÿÿíƒıƒ    ÿÿÿÿÛƒëƒ    ÿÿÿÿcƒsƒ    ÿÿÿÿ."ƒ>"ƒ    ÿÿÿÿğ%ƒ&ƒ    ÿÿÿÿË(ƒÛ(ƒ    ÿÿÿÿv*ƒ†*ƒ    ÿÿÿÿæ,ƒö,ƒ    ÿÿÿÿV.ƒf.ƒ    ÿÿÿÿ 3ƒ³3ƒ    ÿÿÿÿâ9ƒè9ƒ    ÿÿÿÿ¨Pƒ®Pƒ    Pƒ…Pƒÿÿÿÿ2Rƒ8Rƒ    ÿÿÿÿ}aƒƒaƒ    ÿÿÿÿ´mƒºmƒ    ÿÿÿÿonƒnƒ    ÿÿÿÿoƒoƒ    ÿÿÿÿªqƒ°qƒ    ÿÿÿÿ    Ávƒ    ÿÿÿÿ[xƒaxƒ    ÿÿÿÿUˆƒ[ˆƒ    ÿÿÿÿVŒƒ\Œƒ    ÿÿÿÿ–£ƒœ£ƒ    ÿÿÿÿ§¹ƒ­¹ƒ    ÿÿÿÿ+¼ƒ;¼ƒ    ÿÿÿÿ´ÊƒºÊƒÿÿÿÿ‚ËƒˆËƒÿÿÿÿ¹Ìƒ¿Ìƒ    ÿÿÿÿOÏƒUÏƒ    ÿÿÿÿ³Ğƒ¹Ğƒ    ÿÿÿÿ„ „    ÿÿÿÿi	„o	„    ÿÿÿÿL„R„    ÿÿÿÿš„ª„    ÿÿÿÿ¿„Ï„    ÿÿÿÿP„`„    ÿÿÿÿR5„X5„ÿÿÿÿÑ5„×5„ÿÿÿÿR6„X6„ÿÿÿÿÈ6„Î6„ÿÿÿÿO7„U7„    ÿÿÿÿ¥N…«N…    ÿÿÿÿ³ô…Æô…    ÿÿÿÿä†÷†    ÿÿÿÿö0†ü0†    ÿÿÿÿÍS†ÓS†    ÿÿÿÿ5[†;[†    ÿÿÿÿË\†Ñ\†    ÿÿÿÿê ‡!‡ÿÿÿÿOŒ    WŒ    aŒ “   úŒ                 “   \úŒ                ÿÿÿÿuŒ    ‰Œ   —Œ   ¥Œ   ³Œ   ÁŒ “   ¨úŒ                ÿÿÿÿÙŒ    íŒ   øŒ   Œ   Œ   Œ   $Œÿÿÿÿ9Œ “   àúŒ                ÿÿÿÿNŒ “   ûŒ                ÿÿÿÿ`Œ “   (ûŒ                ÿÿÿÿuŒ “   LûŒ                ÿÿÿÿ‡Œ    Œ “   pûŒ                 “   ¸ûŒ                ÿÿÿÿ¤Œ    ğŒ    ıŒ    ãŒ    ÉŒ    ÖŒ    ¯Œ    ¼ŒÿÿÿÿŸŒ  P
 ˆÿÿ                   ÿÿÿÿ&ŸŒ    .ŸŒ   9ŸŒ “   üŒ                 “   lüŒ                ÿÿÿÿNŸŒ    VŸŒ   aŸŒ   lŸŒÿÿÿÿŸŒ “   ŒüŒ                ÿÿÿÿ“ŸŒ “   °üŒ                ÿÿÿÿ§ŸŒ    »ŸŒ “   ÔüŒ                ÿÿÿÿĞŸŒ    äŸŒ “    ıŒ                ÿÿÿÿùŸŒ     Œ “   ,ıŒ                ÿÿÿÿ" Œ “   XıŒ                ÿÿÿÿ7 Œ “   |ıŒ                ÿÿÿÿL Œ “    ıŒ                ÿÿÿÿj Œ “   ÄıŒ                ÿÿÿÿ| Œ “   èıŒ                ÿÿÿÿ Œÿÿÿÿ˜ Œ “   şŒ                 “   TşŒ                ÿÿÿÿª Œ    ´ Œ   ¾ Œ   È Œ   Ò Œ   Ü Œ   æ Œ   ğ Œ   ú Œ   ¡Œ	   ¡Œ
   ¡Œ   "¡Œ   ,¡Œ   6¡Œ   @¡Œ   J¡Œ   T¡Œ   ^¡Œ   h¡Œ   r¡Œ   |¡Œ “	    ÿŒ                ÿÿÿÿ¡Œ    š¡Œ   ¤¡Œ   ®¡Œ   ¸¡Œ   Â¡Œ   Ì¡Œ   Ö¡Œ   à¡Œ “   „ÿŒ                ÿÿÿÿô¡Œ    ş¡Œ   ¢Œ   ¢Œ   ¢Œ   &¢Œ “   ĞÿŒ                ÿÿÿÿ:¢Œ    D¢Œ   N¢Œ   X¢Œ   b¢Œ   l¢Œ   v¢Œ   €¢Œ   Š¢Œ   ”¢Œ	   ¢Œ
   ¨¢Œ   ²¢Œ   ¼¢Œ   Æ¢Œ   Ğ¢Œ   Ú¢Œ   ä¢Œ “   |                 ÿÿÿÿø¢Œ    £Œ   £Œ   £Œ    £Œ   *£Œ   4£Œ   >£Œ   H£Œ   R£Œ	   \£Œ
   f£Œ   p£Œ   z£Œÿÿ