#include "ntfsproc.h"

#undef CONSTANT_UNICODE_STRING


#include "W2KNtfsLib.h"


VOID
W2KNtfsCleanupAttributeContext (
    IN OUT PIRP_CONTEXT IrpContext,
    IN OUT PATTRIBUTE_ENUMERATION_CONTEXT AttributeContext
    )

/*++

Routine Description:

    This routine is called to free any resources claimed within an enumeration
    context and to unpin mapped or pinned data.

Arguments:

    IrpContext - context of the call

    AttributeContext - Pointer to the enumeration context to perform cleanup
                       on.

Return Value:

    None.

--*/

{

    UNREFERENCED_PARAMETER( IrpContext );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsCleanupAttributeContext\n") );

    //
    //  TEMPCODE   We need a call to cleanup any Scb's created.
    //

    //
    //  Unpin any Bcb's pinned here.
    //

    NtfsUnpinBcb( IrpContext, &AttributeContext->FoundAttribute.Bcb );
    NtfsUnpinBcb( IrpContext, &AttributeContext->AttributeList.Bcb );
    NtfsUnpinBcb( IrpContext, &AttributeContext->AttributeList.NonresidentListBcb );

    //
    //  Originally, we zeroed the entire context at this point.  This is
    //  wildly inefficient since the context is either deallocated soon thereafter
    //  or is initialized again.
    //
    //  RtlZeroMemory( AttributeContext, sizeof(ATTRIBUTE_ENUMERATION_CONTEXT) );
    //

    //  BUGBUG - set entire contents to -1 (and reset Bcb's to NULL) to verify
    //  that no one reuses this data structure

#if DBG
    RtlFillMemory( AttributeContext, sizeof( *AttributeContext ), -1 );
    AttributeContext->FoundAttribute.Bcb = NULL;
    AttributeContext->AttributeList.Bcb = NULL;
    AttributeContext->AttributeList.NonresidentListBcb = NULL;
#endif

    DebugTrace( -1, Dbg, ("NtfsCleanupAttributeContext -> VOID\n") );

    return;
}

BOOLEAN
W2KNtfsLookupAllocation (
//    IN PIRP_CONTEXT IrpContext,
    IN PATTRIBUTE_RECORD_HEADER Attribute,
    IN OUT PSCB Scb,
    IN VCN Vcn,
    OUT PLCN Lcn,
    OUT PLONGLONG ClusterCount,
    OUT PVOID *RangePtr OPTIONAL,
    OUT PULONG RunIndex OPTIONAL
    )

/*++

Routine Description:

    This routine looks up the given Vcn for an Scb, and returns whether it
    is allocated and how many contiguously allocated (or deallocated) Lcns
    exist at that point.

Arguments:

    Scb - Specifies which attribute the lookup is to occur on.

    Vcn - Specifies the Vcn to be looked up.

    Lcn - If returning TRUE, returns the Lcn that the specified Vcn is mapped
          to.  If returning FALSE, the return value is undefined.

    ClusterCount - If returning TRUE, returns the number of contiguously allocated
                   Lcns exist beginning at the Lcn returned.  If returning FALSE,
                   specifies the number of unallocated Vcns exist beginning with
                   the specified Vcn.

    RangePtr - If specified, we return the range index for the start of the mapping.

    RunIndex - If specified, we return the run index within the range for the start of the mapping.

Return Value:

    BOOLEAN - TRUE if the input Vcn has a corresponding Lcn and
        FALSE otherwise.

--*/

{
    ATTRIBUTE_ENUMERATION_CONTEXT Context;
//    PATTRIBUTE_RECORD_HEADER Attribute;

    VCN HighestCandidate;

    BOOLEAN Found;
    BOOLEAN EntryAdded;

    VCN CapturedLowestVcn;
    VCN CapturedHighestVcn;

    PVCB Vcb = Scb->Vcb;
    BOOLEAN McbMutexAcquired = FALSE;
    LONGLONG AllocationClusters;


//    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_SCB( Scb );

    DebugTrace( +1, Dbg, ("NtfsLookupAllocation\n") );
    DebugTrace( 0, Dbg, ("Scb = %08lx\n", Scb) );
    DebugTrace( 0, Dbg, ("Vcn = %I64x\n", Vcn) );

    //
    //  First try to look up the allocation in the mcb, and return the run
    //  from there if we can.  Also, if we are doing restart, just return
    //  the answer straight from the Mcb, because we cannot read the disk.
    //  We also do this for the Mft if the volume has been mounted as the
    //  Mcb for the Mft should always represent the entire file.
    //

    HighestCandidate = MAXLONGLONG;
    if ((Found = NtfsLookupNtfsMcbEntry( &Scb->Mcb, Vcn, Lcn, ClusterCount, NULL, NULL, RangePtr, RunIndex ))

#if 0
          ||

        (Scb == Scb->Vcb->MftScb

            &&

         FlagOn( Scb->Vcb->Vpb->Flags, VPB_MOUNTED ))

          ||

        FlagOn( Scb->Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS )
#endif
		) {
#if 0
        //
        //  If not found (beyond the end of the Mcb), we will return the
        //  count to the largest representable Lcn.
        //

        if ( !Found ) {
            *ClusterCount = MAXLONGLONG - Vcn;

        //
        //  Test if we found a hole in the allocation.  In this case
        //  Found will be TRUE and the Lcn will be the UNUSED_LCN.
        //  We only expect this case at restart.
        //

        } else if (*Lcn == UNUSED_LCN) {

            //
            //  If the Mcb package returned UNUSED_LCN, because of a hole, then
            //  we turn this into FALSE.
            //

            Found = FALSE;
        }

        ASSERT( !Found ||
                (*Lcn != 0) ||
                (NtfsEqualMftRef( &Scb->Fcb->FileReference, &BootFileReference )) ||
                (NtfsEqualMftRef( &Scb->Fcb->FileReference, &VolumeFileReference )));

        DebugTrace( -1, Dbg, ("NtfsLookupAllocation -> %02lx\n", Found) );

#endif

        return Found;
    }

    PAGED_CODE();

    //
    //  Prepare for looking up attribute records to get the retrieval
    //  information.
    //

    CapturedLowestVcn = MAXLONGLONG;
    NtfsInitializeAttributeContext( &Context );

    //
    //  Make sure we have the main resource acquired shared so that the
    //  attributes in the file record are not moving around.  We blindly
    //  use Wait = TRUE.  Most of the time when we go to the disk for I/O
    //  (and thus need mapping) we are synchronous, and otherwise, the Mcb
    //  is virtually always loaded anyway and we do not get here.
    //

    ExAcquireResourceShared( Scb->Header.Resource, TRUE );

    try {

        //
        //  Lookup the attribute record for this Scb.
        //

//        NtfsLookupAttributeForScb( IrpContext, Scb, &Vcn, &Context );
//        Attribute = NtfsFoundAttribute( &Context );
        
        ASSERT( !NtfsIsAttributeResident(Attribute) );

        if (FlagOn( Scb->ScbState, SCB_STATE_FILE_SIZE_LOADED )) {
            AllocationClusters = LlClustersFromBytesTruncate( Vcb, Scb->Header.AllocationSize.QuadPart );
        } else {
            ASSERT( NtfsUnsafeSegmentNumber( &Context.FoundAttribute.FileRecord->BaseFileRecordSegment ) == 0 );
            AllocationClusters = LlClustersFromBytesTruncate( Vcb, Attribute->Form.Nonresident.AllocatedLength );
        }
        
        
        //
        //  The desired Vcn is not currently in the Mcb.  We will loop to lookup all
        //  the allocation, and we need to make sure we cleanup on the way out.
        //
        //  It is important to note that if we ever optimize this lookup to do random
        //  access to the mapping pairs, rather than sequentially loading up the Mcb
        //  until we get the Vcn he asked for, then NtfsDeleteAllocation will have to
        //  be changed.
        //

        //
        //  Acquire exclusive access to the mcb to keep others from looking at
        //  it while it is not fully loaded.  Otherwise they might see a hole
        //  while we're still filling up the mcb
        //

        if (!FlagOn(Scb->Fcb->FcbState, FCB_STATE_NONPAGED)) {
            NtfsAcquireNtfsMcbMutex( &Scb->Mcb );
            McbMutexAcquired = TRUE;
        }

        //
        //  Store run information in the Mcb until we hit the last Vcn we are
        //  interested in, or until we cannot find any more attribute records.
        //

        while(TRUE) {

            VCN CurrentVcn;
            LCN CurrentLcn;
            LONGLONG Change;
            PCHAR ch;
            ULONG VcnBytes;
            ULONG LcnBytes;

            //
            //  Verify the highest and lowest Vcn values.
            //  Also verify that our starting Vcn sits within this range.
            //

            if ((Attribute->Form.Nonresident.LowestVcn < 0) ||
                (Attribute->Form.Nonresident.LowestVcn - 1 > Attribute->Form.Nonresident.HighestVcn) ||
                (Vcn < Attribute->Form.Nonresident.LowestVcn) ||
                (Attribute->Form.Nonresident.HighestVcn >= AllocationClusters)) {

//                NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Scb->Fcb );
				ASSERT(FALSE);
				return FALSE; // before return, check more
            }

            //
            //  Define the new range.
            //

            NtfsDefineNtfsMcbRange( &Scb->Mcb,
                                    CapturedLowestVcn = Attribute->Form.Nonresident.LowestVcn,
                                    CapturedHighestVcn = Attribute->Form.Nonresident.HighestVcn,
                                    McbMutexAcquired );

            //
            //  Implement the decompression algorithm, as defined in ntfs.h.
            //

            HighestCandidate = Attribute->Form.Nonresident.LowestVcn;
            CurrentLcn = 0;
            ch = (PCHAR)Attribute + Attribute->Form.Nonresident.MappingPairsOffset;

            //
            //  Loop to process mapping pairs.
            //

            EntryAdded = FALSE;
            while (!IsCharZero(*ch)) {

                //
                //  Set Current Vcn from initial value or last pass through loop.
                //

                CurrentVcn = HighestCandidate;

                //
                //  VCNs should never be negative.
                //

                if (CurrentVcn < 0) {
                
                    ASSERT( FALSE );
                    //NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Scb->Fcb );
					return FALSE; // before return, check more
                }
                
                //
                //  Extract the counts from the two nibbles of this byte.
                //

                VcnBytes = *ch & 0xF;
                LcnBytes = *ch++ >> 4;

                //
                //  Extract the Vcn change (use of RtlCopyMemory works for little-Endian)
                //  and update HighestCandidate.
                //

                Change = 0;

                //
                //  The file is corrupt if there are 0 or more than 8 Vcn change bytes,
                //  more than 8 Lcn change bytes, or if we would walk off the end of
                //  the record, or a Vcn change is negative.
                //

                if (((ULONG)(VcnBytes - 1) > 7) || (LcnBytes > 8) ||
                    ((ch + VcnBytes + LcnBytes + 1) > (PCHAR)Add2Ptr(Attribute, Attribute->RecordLength)) ||
                    IsCharLtrZero(*(ch + VcnBytes - 1))) {

                    ASSERT( FALSE );
                    //NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Scb->Fcb );
					return FALSE;
                }
                RtlCopyMemory( &Change, ch, VcnBytes );
                ch += VcnBytes;
                HighestCandidate = HighestCandidate + Change;

                //
                //  Extract the Lcn change and update CurrentLcn.
                //

                if (LcnBytes != 0) {

                    Change = 0;
                    if (IsCharLtrZero(*(ch + LcnBytes - 1))) {
                        Change = Change - 1;
                    }
                    RtlCopyMemory( &Change, ch, LcnBytes );
                    ch += LcnBytes;
                    CurrentLcn = CurrentLcn + Change;

                    //
                    // Now add it in to the mcb.
                    //

                    if ((CurrentLcn >= 0) && (LcnBytes != 0)) {

                        LONGLONG ClustersToAdd;
                        ClustersToAdd = HighestCandidate - CurrentVcn;

                        //
                        //  Now try to add the current run.  We never expect this
                        //  call to return false.
                        //

                        ASSERT( ((ULONG)CurrentLcn) != 0xffffffff );

#ifdef NTFS_CHECK_BITMAP
                        //
                        //  Make sure these bits are allocated in our copy of the bitmap.
                        //

                        if ((Vcb->BitmapCopy != NULL) &&
                            !NtfsCheckBitmap( Vcb,
                                              (ULONG) CurrentLcn,
                                              (ULONG) ClustersToAdd,
                                              TRUE )) {

                            NtfsBadBitmapCopy( IrpContext, (ULONG) CurrentLcn, (ULONG) ClustersToAdd );
                        }
#endif
                        if (!NtfsAddNtfsMcbEntry( &Scb->Mcb,
                                                  CurrentVcn,
                                                  CurrentLcn,
                                                  ClustersToAdd,
                                                  McbMutexAcquired )) {

                            ASSERTMSG( "Unable to add entry to Mcb\n", FALSE );

                            //NtfsRaiseStatus( IrpContext,
                            //                 STATUS_FILE_CORRUPT_ERROR,
                            //                 NULL,
                            //                 Scb->Fcb );
 							return FALSE; // before return, check more
                       }

                        EntryAdded = TRUE;
                    }
                }
            }

            //
            //  Make sure that at least the Mcb gets loaded.
            //

            if (!EntryAdded) {
                NtfsAddNtfsMcbEntry( &Scb->Mcb,
                                     CapturedLowestVcn,
                                     UNUSED_LCN,
                                     1,
                                     McbMutexAcquired );
            }

            if ((Vcn < HighestCandidate) /*|| 
                (!NtfsLookupNextAttributeForScb( IrpContext, Scb, &Context ))*/) {
                break;                                                 
            } else {
                Attribute = NtfsFoundAttribute( &Context );
                ASSERT( !NtfsIsAttributeResident(Attribute) );
            }
        }

        //
        //  Now free the mutex and lookup in the Mcb while we still own
        //  the resource.
        //

        if (McbMutexAcquired) {
            NtfsReleaseNtfsMcbMutex( &Scb->Mcb );
            McbMutexAcquired = FALSE;
        }

        if (NtfsLookupNtfsMcbEntry( &Scb->Mcb, Vcn, Lcn, ClusterCount, NULL, NULL, RangePtr, RunIndex )) {

            Found = (BOOLEAN)(*Lcn != UNUSED_LCN);

            if (Found) { ASSERT_LCN_RANGE_CHECKING( Scb->Vcb, (*Lcn + *ClusterCount) ); }

        } else {

            Found = FALSE;

            //
            //  At the end of file, we pretend there is one large hole!
            //

            if (HighestCandidate >=
                LlClustersFromBytes(Vcb, Scb->Header.AllocationSize.QuadPart)) {
                HighestCandidate = MAXLONGLONG;
            }

            *ClusterCount = HighestCandidate - Vcn;
        }

    } finally {

        DebugUnwind( NtfsLookupAllocation );

        //
        //  If this is an error case then we better unload what we've just
        //  loaded
        //

        if (AbnormalTermination() &&
            (CapturedLowestVcn != MAXLONGLONG) ) {

            NtfsUnloadNtfsMcbRange( &Scb->Mcb,
                                    CapturedLowestVcn,
                                    CapturedHighestVcn,
                                    FALSE,
                                    McbMutexAcquired );
        }

        //
        //  In all cases we free up the mcb that we locked before entering
        //  the try statement
        //

        if (McbMutexAcquired) {
            NtfsReleaseNtfsMcbMutex( &Scb->Mcb );
        }

        ExReleaseResource( Scb->Header.Resource );

        //
        // Cleanup the attribute context on the way out.
        //

        //NtfsCleanupAttributeContext( IrpContext, &Context );
        W2KNtfsCleanupAttributeContext( NULL, &Context );
    }

#if 0
    ASSERT( !Found ||
            (*Lcn != 0) ||
            (NtfsEqualMftRef( &Scb->Fcb->FileReference, &BootFileReference )) ||
            (NtfsEqualMftRef( &Scb->Fcb->FileReference, &VolumeFileReference )));
#endif
	
    DebugTrace( 0, Dbg, ("Lcn < %0I64x\n", *Lcn) );
    DebugTrace( 0, Dbg, ("ClusterCount < %0I64x\n", *ClusterCount) );
    DebugTrace( -1, Dbg, ("NtfsLookupAllocation -> %02lx\n", Found) );

    return Found;
}
