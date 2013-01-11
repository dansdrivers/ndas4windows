/*++

Copyright (c) 1996  Microsoft Corporation

Module Name:

    Quota.c

Abstract:

    This module implements the quota support routines for Ntfs

Author:

    Jeff Havens     [JHavens]        29-Feb-1996

Revision History:

--*/

#include "NtfsProc.h"

#define Dbg DEBUG_TRACE_QUOTA


//
//  Define a tag for general pool allocations from this module
//

#undef MODULE_POOL_TAG
#define MODULE_POOL_TAG                  ('QFtN')

#define MAXIMUM_SID_LENGTH \
    (FIELD_OFFSET( SID, SubAuthority ) + sizeof( ULONG ) * SID_MAX_SUB_AUTHORITIES)

#define MAXIMUM_QUOTA_ROW (SIZEOF_QUOTA_USER_DATA + MAXIMUM_SID_LENGTH + sizeof( ULONG ))

//
//  Local quota support routines.
//

VOID
NtfsClearAndVerifyQuotaIndex (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

NTSTATUS
NtfsClearPerFileQuota (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PVOID Context
    );

VOID
NtfsDeleteUnsedIds (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

VOID
NtfsMarkUserLimit (
    IN PIRP_CONTEXT IrpContext,
    IN PVOID Context
    );

NTSTATUS
NtfsPackQuotaInfo (
    IN PSID Sid,
    IN PQUOTA_USER_DATA QuotaUserData OPTIONAL,
    IN PFILE_QUOTA_INFORMATION OutBuffer,
    IN OUT PULONG OutBufferSize
    );

VOID
NtfsPostUserLimit (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PQUOTA_CONTROL_BLOCK QuotaControl
    );

NTSTATUS
NtfsPrepareForDelete (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PSID Sid
    );

VOID
NtfsRepairQuotaIndex (
    IN PIRP_CONTEXT IrpContext,
    IN PVOID Context
    );

NTSTATUS
NtfsRepairPerFileQuota (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PVOID Context
    );

VOID
NtfsSaveQuotaFlags (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

VOID
NtfsSaveQuotaFlagsSafe (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

NTSTATUS
NtfsVerifyOwnerIndex (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

RTL_GENERIC_COMPARE_RESULTS
NtfsQuotaTableCompare (
    IN PRTL_GENERIC_TABLE Table,
    PVOID FirstStruct,
    PVOID SecondStruct
    );

PVOID
NtfsQuotaTableAllocate (
    IN PRTL_GENERIC_TABLE Table,
    CLONG ByteSize
    );

VOID
NtfsQuotaTableFree (
    IN PRTL_GENERIC_TABLE Table,
    PVOID Buffer
    );

#if (DBG || defined( NTFS_FREE_ASSERTS ) || defined( NTFSDBG ))
BOOLEAN NtfsAllowFixups = 1;
BOOLEAN NtfsCheckQuota = 0;
#endif // DBG

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsAcquireQuotaControl)
#pragma alloc_text(PAGE, NtfsCalculateQuotaAdjustment)
#pragma alloc_text(PAGE, NtfsClearAndVerifyQuotaIndex)
#pragma alloc_text(PAGE, NtfsClearPerFileQuota)
#pragma alloc_text(PAGE, NtfsDeleteUnsedIds)
#pragma alloc_text(PAGE, NtfsDereferenceQuotaControlBlock)
#pragma alloc_text(PAGE, NtfsFixupQuota)
#pragma alloc_text(PAGE, NtfsFsQuotaQueryInfo)
#pragma alloc_text(PAGE, NtfsFsQuotaSetInfo)
#pragma alloc_text(PAGE, NtfsGetCallersUserId)
#pragma alloc_text(PAGE, NtfsGetOwnerId)
#pragma alloc_text(PAGE, NtfsGetRemainingQuota)
#pragma alloc_text(PAGE, NtfsInitializeQuotaControlBlock)
#pragma alloc_text(PAGE, NtfsInitializeQuotaIndex)
#pragma alloc_text(PAGE, NtfsMarkQuotaCorrupt)
#pragma alloc_text(PAGE, NtfsMarkUserLimit)
#pragma alloc_text(PAGE, NtfsMoveQuotaOwner)
#pragma alloc_text(PAGE, NtfsPackQuotaInfo)
#pragma alloc_text(PAGE, NtfsPostUserLimit)
#pragma alloc_text(PAGE, NtfsPostRepairQuotaIndex)
#pragma alloc_text(PAGE, NtfsPrepareForDelete)
#pragma alloc_text(PAGE, NtfsReleaseQuotaControl)
#pragma alloc_text(PAGE, NtfsRepairQuotaIndex)
#pragma alloc_text(PAGE, NtfsSaveQuotaFlags)
#pragma alloc_text(PAGE, NtfsSaveQuotaFlagsSafe)
#pragma alloc_text(PAGE, NtfsQueryQuotaUserSidList)
#pragma alloc_text(PAGE, NtfsQuotaTableCompare)
#pragma alloc_text(PAGE, NtfsQuotaTableAllocate)
#pragma alloc_text(PAGE, NtfsQuotaTableFree)
#pragma alloc_text(PAGE, NtfsUpdateFileQuota)
#pragma alloc_text(PAGE, NtfsUpdateQuotaDefaults)
#pragma alloc_text(PAGE, NtfsVerifyOwnerIndex)
#pragma alloc_text(PAGE, NtfsRepairPerFileQuota)
#endif


VOID
NtfsAcquireQuotaControl (
    IN PIRP_CONTEXT IrpContext,
    IN PQUOTA_CONTROL_BLOCK QuotaControl
    )

/*++

Routine Description:

    Acquire the quota control block and quota index for shared update.  Multiple
    transactions can update then index, but only one thread can update a
    particular index.

Arguments:

    QuotaControl - Quota control block to be acquired.

Return Value:

    None.

--*/

{
    PVOID *Position;
    PVOID *ScbArray;
    ULONG Count;

    PAGED_CODE();

    ASSERT( QuotaControl->ReferenceCount > 0 );

    //
    //  Make sure we have a free spot in the Scb array in the IrpContext.
    //

    if (IrpContext->SharedScb == NULL) {

        Position = &IrpContext->SharedScb;
        IrpContext->SharedScbSize = 1;

    //
    //  Too bad the first one is not available.  If the current size is one then allocate a
    //  new block and copy the existing value to it.
    //

    } else if (IrpContext->SharedScbSize == 1) {

        if (IrpContext->SharedScb == QuotaControl) {

            //
            //  The quota block has already been aquired.
            //

            return;
        }

        ScbArray = NtfsAllocatePool( PagedPool, sizeof( PVOID ) * 4 );
        RtlZeroMemory( ScbArray, sizeof( PVOID ) * 4 );
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

            if (*Position == QuotaControl) {

                //
                //  The quota block has already been aquired.
                //

                return;
            }

            Count -= 1;
            Position += 1;

        } while (Count != 0);

        //
        //  If we didn't find one then allocate a new structure.
        //

        if (Count == 0) {

            ScbArray = NtfsAllocatePool( PagedPool, sizeof( PVOID ) * IrpContext->SharedScbSize * 2 );
            RtlZeroMemory( ScbArray, sizeof( PVOID ) * IrpContext->SharedScbSize * 2 );
            RtlCopyMemory( ScbArray,
                           IrpContext->SharedScb,
                           sizeof( PVOID ) * IrpContext->SharedScbSize );

            NtfsFreePool( IrpContext->SharedScb );
            IrpContext->SharedScb = ScbArray;
            Position = ScbArray + IrpContext->SharedScbSize;
            IrpContext->SharedScbSize *= 2;
        }
    }

    //
    //  The following assert is bougus, but I want know if we hit the case
    //  where create is acquiring the scb stream shared.
    //  Then make sure that the resource is released in create.c
    //

    ASSERT( IrpContext->MajorFunction != IRP_MJ_CREATE || IrpContext->OriginatingIrp != NULL || NtfsIsExclusiveScb( IrpContext->Vcb->QuotaTableScb ));

    //
    //  Increase the reference count so the quota control block is not deleted
    //  while it is in the shared list.
    //

    ASSERT( QuotaControl->ReferenceCount > 0 );
    InterlockedIncrement( &QuotaControl->ReferenceCount );

    //
    //  The quota index must be acquired before the mft scb is acquired.
    //

    ASSERT(!NtfsIsExclusiveScb( IrpContext->Vcb->MftScb ) ||
           ExIsResourceAcquiredSharedLite( IrpContext->Vcb->QuotaTableScb->Header.Resource ));

    NtfsAcquireResourceShared( IrpContext, IrpContext->Vcb->QuotaTableScb, TRUE );
    ExAcquireFastMutexUnsafe( QuotaControl->QuotaControlLock );

    *Position = QuotaControl;

    return;
}


VOID
NtfsCalculateQuotaAdjustment (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    OUT PLONGLONG Delta
    )

/*++

Routine Description:

    This routine scans the user data streams in a file and determines
    by how much the quota needs to be adjusted.

Arguments:

    Fcb - Fcb whose quota usage is being modified.

    Delta - Returns the amount of quota adjustment required for the file.

Return Value:

    None

--*/

{
    ATTRIBUTE_ENUMERATION_CONTEXT Context;
    PATTRIBUTE_RECORD_HEADER Attribute;
    VCN StartVcn = 0;

    PAGED_CODE();

    ASSERT_EXCLUSIVE_FCB( Fcb );

    //
    //  There is nothing to do if the standard infor has not been
    //  expanded yet.
    //

    if (!FlagOn( Fcb->FcbState, FCB_STATE_LARGE_STD_INFO )) {
        *Delta = 0;
        return;
    }

    NtfsInitializeAttributeContext( &Context );

    //
    //  Use a try-finally to cleanup the enumeration structure.
    //

    try {

        //
        //  Start with the $STANDARD_INFORMATION.  This must be the first one found.
        //

        if (!NtfsLookupAttribute( IrpContext, Fcb, &Fcb->FileReference, &Context )) {

            NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Fcb );
        }

        Attribute = NtfsFoundAttribute( &Context );

        if (Attribute->TypeCode != $STANDARD_INFORMATION) {

            NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Fcb );
        }

        //
        //  Initialize quota amount to the value current in the standard information structure.
        //

        *Delta = -(LONGLONG) ((PSTANDARD_INFORMATION) NtfsAttributeValue( Attribute ))->QuotaCharged;

        //
        //  Now continue while there are more attributes to find.
        //

        while (NtfsLookupNextAttributeByVcn( IrpContext, Fcb, &StartVcn, &Context )) {

            //
            //  Point to the current attribute.
            //

            Attribute = NtfsFoundAttribute( &Context );

            //
            //  For all user data streams charge for a file record plus any non-resident allocation.
            //  For index streams charge for a file record for the INDEX_ROOT.
            //
            //  For user data look for a resident attribute or the first attribute of a non-resident stream.
            //  Otherwise look for a $I30 stream.
            //

            if (NtfsIsTypeCodeSubjectToQuota( Attribute->TypeCode ) ||
                ((Attribute->TypeCode == $INDEX_ROOT) &&
                 ((Attribute->NameLength * sizeof( WCHAR )) == NtfsFileNameIndex.Length) &&
                 RtlEqualMemory( Add2Ptr( Attribute, Attribute->NameOffset ),
                                 NtfsFileNameIndex.Buffer,
                                 NtfsFileNameIndex.Length ))) {

                //
                //  Always charge for at least one file record.
                //

                *Delta += NtfsResidentStreamQuota( Fcb->Vcb );

                //
                //  Charge for the allocated length for non-resident.
                //

                if (!NtfsIsAttributeResident( Attribute )) {

                    *Delta += Attribute->Form.Nonresident.AllocatedLength;
                }
            }
        }

    } finally {

        NtfsCleanupAttributeContext( IrpContext, &Context );
    }

    return;
}


VOID
NtfsClearAndVerifyQuotaIndex (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine iterates over the quota user data index and verifies the back
    pointer to the owner id index.  It also zeros the quota used field for
    each owner.

Arguments:

    Vcb - Pointer to the volume control block whose index is to be operated
          on.

Return Value:

    None

--*/

{
    INDEX_KEY IndexKey;
    INDEX_ROW OwnerRow;
    MAP_HANDLE MapHandle;
    PQUOTA_USER_DATA UserData;
    PINDEX_ROW QuotaRow;
    PVOID RowBuffer;
    NTSTATUS Status;
    ULONG OwnerId;
    ULONG Count;
    ULONG i;
    PSCB QuotaScb = Vcb->QuotaTableScb;
    PSCB OwnerIdScb = Vcb->OwnerIdTableScb;
    PINDEX_ROW IndexRow = NULL;
    PREAD_CONTEXT ReadContext = NULL;
    BOOLEAN IndexAcquired = FALSE;

    NtOfsInitializeMapHandle( &MapHandle );

    //
    //  Allocate a buffer lager enough for several rows.
    //

    RowBuffer = NtfsAllocatePool( PagedPool, PAGE_SIZE );

    try {

        //
        //  Allocate a bunch of index row entries.
        //

        Count = PAGE_SIZE / sizeof( QUOTA_USER_DATA );

        IndexRow = NtfsAllocatePool( PagedPool,
                                     Count * sizeof( INDEX_ROW ) );

        //
        //  Iterate through the quota entries.  Start where we left off.
        //

        OwnerId = Vcb->QuotaFileReference.SegmentNumberLowPart;
        IndexKey.KeyLength = sizeof( OwnerId );
        IndexKey.Key = &OwnerId;

        Status = NtOfsReadRecords( IrpContext,
                                   QuotaScb,
                                   &ReadContext,
                                   &IndexKey,
                                   NtOfsMatchAll,
                                   NULL,
                                   &Count,
                                   IndexRow,
                                   PAGE_SIZE,
                                   RowBuffer );


        while (NT_SUCCESS( Status )) {

            NtfsAcquireSharedVcb( IrpContext, Vcb, TRUE );

            //
            //  Acquire the VCB shared and check whether we should
            //  continue.
            //

            if (!NtfsIsVcbAvailable( Vcb )) {

                //
                //  The volume is going away, bail out.
                //

                NtfsReleaseVcb( IrpContext, Vcb );
                Status = STATUS_VOLUME_DISMOUNTED;
                leave;
            }

            NtfsAcquireExclusiveScb( IrpContext, QuotaScb );
            NtfsAcquireExclusiveScb( IrpContext, OwnerIdScb );
            IndexAcquired = TRUE;

            //
            //  The following assert must be done while the quota resource
            //  held; otherwise a lingering transaction may cause it to
            //

            ASSERT( RtlIsGenericTableEmpty( &Vcb->QuotaControlTable ));

            QuotaRow = IndexRow;

            for (i = 0; i < Count; i += 1, QuotaRow += 1) {

                UserData = QuotaRow->DataPart.Data;

                //
                //  Validate the record is long enough for the Sid.
                //

                IndexKey.KeyLength = RtlLengthSid( &UserData->QuotaSid );

                if ((IndexKey.KeyLength + SIZEOF_QUOTA_USER_DATA > QuotaRow->DataPart.DataLength) ||
                    !RtlValidSid( &UserData->QuotaSid )) {

                    ASSERT( FALSE );

                    //
                    //  The sid is bad delete the record.
                    //

                    NtOfsDeleteRecords( IrpContext,
                                        QuotaScb,
                                        1,
                                        &QuotaRow->KeyPart );

                    continue;
                }

                IndexKey.Key = &UserData->QuotaSid;

                //
                //  Look up the Sid is in the owner id index.
                //

                Status = NtOfsFindRecord( IrpContext,
                                          OwnerIdScb,
                                          &IndexKey,
                                          &OwnerRow,
                                          &MapHandle,
                                          NULL );

                ASSERT( NT_SUCCESS( Status ));

                if (!NT_SUCCESS( Status )) {

                    //
                    //  The owner id entry is missing.  Add one back in.
                    //

                    OwnerRow.KeyPart = IndexKey;
                    OwnerRow.DataPart.DataLength = QuotaRow->KeyPart.KeyLength;
                    OwnerRow.DataPart.Data = QuotaRow->KeyPart.Key;

                    NtOfsAddRecords( IrpContext,
                                     OwnerIdScb,
                                     1,
                                     &OwnerRow,
                                     FALSE );


                } else {

                    //
                    //  Verify that the owner id's match.
                    //

                    if (*((PULONG) QuotaRow->KeyPart.Key) != *((PULONG) OwnerRow.DataPart.Data)) {

                        ASSERT( FALSE );

                        //
                        //  Keep the quota record with the lower
                        //  quota id.  Delete the one with the higher
                        //  quota id.  Note this is the simple approach
                        //  and not best case of the lower id does not
                        //  exist.  In that case a user entry will be delete
                        //  and be reassigned a default quota.
                        //

                        if (*((PULONG) QuotaRow->KeyPart.Key) < *((PULONG) OwnerRow.DataPart.Data)) {

                            //
                            //  Make the ownid's match.
                            //

                            OwnerRow.KeyPart = IndexKey;
                            OwnerRow.DataPart.DataLength = QuotaRow->KeyPart.KeyLength;
                            OwnerRow.DataPart.Data = QuotaRow->KeyPart.Key;

                            NtOfsUpdateRecord( IrpContext,
                                               OwnerIdScb,
                                               1,
                                               &OwnerRow,
                                               NULL,
                                               NULL );

                        } else {

                            //
                            // Delete this record and proceed.
                            //


                            NtOfsDeleteRecords( IrpContext,
                                                QuotaScb,
                                                1,
                                                &QuotaRow->KeyPart );

                            NtOfsReleaseMap( IrpContext, &MapHandle );
                            continue;
                        }
                    }

                    NtOfsReleaseMap( IrpContext, &MapHandle );
                }

                //
                //  Set the quota used to zero.
                //

                UserData->QuotaUsed = 0;
                QuotaRow->DataPart.DataLength = SIZEOF_QUOTA_USER_DATA;

                NtOfsUpdateRecord( IrpContext,
                                   QuotaScb,
                                   1,
                                   QuotaRow,
                                   NULL,
                                   NULL );
            }

            //
            //  Release the indexes and commit what has been done so far.
            //

            NtfsReleaseScb( IrpContext, QuotaScb );
            NtfsReleaseScb( IrpContext, OwnerIdScb );
            NtfsReleaseVcb( IrpContext, Vcb );
            IndexAcquired = FALSE;

            //
            //  Complete the request which commits the pending
            //  transaction if there is one and releases of the
            //  acquired resources.  The IrpContext will not
            //  be deleted because the no delete flag is set.
            //

            SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_DONT_DELETE | IRP_CONTEXT_FLAG_RETAIN_FLAGS );
            NtfsCompleteRequest( IrpContext, NULL, STATUS_SUCCESS );

            //
            //  Remember how far we got so we can restart correctly.
            //

            Vcb->QuotaFileReference.SegmentNumberLowPart = *((PULONG) IndexRow[Count - 1].KeyPart.Key);

            //
            //  Make sure the next free id is beyond the current ids.
            //

            if (Vcb->QuotaOwnerId <= Vcb->QuotaFileReference.SegmentNumberLowPart) {

                ASSERT( Vcb->QuotaOwnerId > Vcb->QuotaFileReference.SegmentNumberLowPart );
                Vcb->QuotaOwnerId = Vcb->QuotaFileReference.SegmentNumberLowPart + 1;
            }

            //
            //  Look up the next set of entries in the quota index.
            //

            Count = PAGE_SIZE / sizeof( QUOTA_USER_DATA );
            Status = NtOfsReadRecords( IrpContext,
                                       QuotaScb,
                                       &ReadContext,
                                       NULL,
                                       NtOfsMatchAll,
                                       NULL,
                                       &Count,
                                       IndexRow,
                                       PAGE_SIZE,
                                       RowBuffer );
        }

        ASSERT( (Status == STATUS_NO_MORE_MATCHES) || (Status == STATUS_NO_MATCH) );

    } finally {

        NtfsFreePool( RowBuffer );
        NtOfsReleaseMap( IrpContext, &MapHandle );

        if (IndexAcquired) {
            NtfsReleaseScb( IrpContext, QuotaScb );
            NtfsReleaseScb( IrpContext, OwnerIdScb );
            NtfsReleaseVcb( IrpContext, Vcb );
        }

        if (IndexRow != NULL) {
            NtfsFreePool( IndexRow );
        }

        if (ReadContext != NULL) {
            NtOfsFreeReadContext( ReadContext );
        }
    }

    return;
}


NTSTATUS
NtfsClearPerFileQuota (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PVOID Context
    )

/*++

Routine Description:

    This routine clears the quota charged field in each file on the volume.  The
    Quata control block is also released in fcb.

Arguments:

    Fcb - Fcb for the file to be processed.

    Context - Unsed.

Return Value:

    STATUS_SUCCESS

--*/
{
    ULONGLONG NewQuota;
    ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;
    PSTANDARD_INFORMATION StandardInformation;
    PQUOTA_CONTROL_BLOCK QuotaControl = Fcb->QuotaControl;
    PVCB Vcb = Fcb->Vcb;

    UNREFERENCED_PARAMETER( Context);

    PAGED_CODE();

    //
    //  There is nothing to do if the standard info has not been
    //  expanded yet.
    //

    if (!FlagOn( Fcb->FcbState, FCB_STATE_LARGE_STD_INFO )) {
        return STATUS_SUCCESS;
    }

    //
    //  Use a try-finally to cleanup the attribute context.
    //

    try {

        //
        //  Initialize the context structure.
        //

        NtfsInitializeAttributeContext( &AttrContext );

        //
        //  Locate the standard information, it must be there.
        //

        if (!NtfsLookupAttributeByCode( IrpContext,
                                        Fcb,
                                        &Fcb->FileReference,
                                        $STANDARD_INFORMATION,
                                        &AttrContext )) {

            DebugTrace( 0, Dbg, ("Can't find standard information\n") );

            NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Fcb );
        }

        StandardInformation = (PSTANDARD_INFORMATION) NtfsAttributeValue( NtfsFoundAttribute( &AttrContext ));

        ASSERT( NtfsFoundAttribute( &AttrContext )->Form.Resident.ValueLength == sizeof( STANDARD_INFORMATION ));

        NewQuota = 0;

        //
        //  Call to change the attribute value.
        //

        NtfsChangeAttributeValue( IrpContext,
                                  Fcb,
                                  FIELD_OFFSET( STANDARD_INFORMATION, QuotaCharged ),
                                  &NewQuota,
                                  sizeof( StandardInformation->QuotaCharged ),
                                  FALSE,
                                  FALSE,
                                  FALSE,
                                  FALSE,
                                  &AttrContext );

        //
        //  Release the quota control block for this fcb.
        //

        if (QuotaControl != NULL) {
            NtfsDereferenceQuotaControlBlock( Vcb, &Fcb->QuotaControl );
        }

    } finally {

        NtfsCleanupAttributeContext( IrpContext, &AttrContext );
    }

    return STATUS_SUCCESS;
}


VOID
NtfsDeleteUnsedIds (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine iterates over the quota user data index and removes any
    entries still marked as deleted.

Arguments:

    Vcb - Pointer to the volume control block whoes index is to be operated
          on.

Return Value:

    None

--*/

{
    INDEX_KEY IndexKey;
    PINDEX_KEY KeyPtr;
    PQUOTA_USER_DATA UserData;
    PINDEX_ROW QuotaRow;
    PVOID RowBuffer;
    NTSTATUS Status = STATUS_SUCCESS;
    ULONG OwnerId;
    ULONG Count;
    ULONG i;
    PSCB QuotaScb = Vcb->QuotaTableScb;
    PSCB OwnerIdScb = Vcb->OwnerIdTableScb;
    PINDEX_ROW IndexRow = NULL;
    PREAD_CONTEXT ReadContext = NULL;
    BOOLEAN IndexAcquired = FALSE;

    //
    //  Allocate a buffer large enough for several rows.
    //

    RowBuffer = NtfsAllocatePool( PagedPool, PAGE_SIZE );

    try {

        //
        //  Allocate a bunch of index row entries.
        //

        Count = PAGE_SIZE / sizeof( QUOTA_USER_DATA );

        IndexRow = NtfsAllocatePool( PagedPool,
                                     Count * sizeof( INDEX_ROW ) );

        //
        //  Iterate through the quota entries.  Start where we left off.
        //

        OwnerId = Vcb->QuotaFileReference.SegmentNumberLowPart;
        IndexKey.KeyLength = sizeof( OwnerId );
        IndexKey.Key = &OwnerId;
        KeyPtr = &IndexKey;

        while (NT_SUCCESS( Status )) {

            NtfsAcquireSharedVcb( IrpContext, Vcb, TRUE );

            //
            //  Acquire the VCB shared and check whether we should
            //  continue.
            //

            if (!NtfsIsVcbAvailable( Vcb )) {

                //
                //  The volume is going away, bail out.
                //

                NtfsReleaseVcb( IrpContext, Vcb );
                Status = STATUS_VOLUME_DISMOUNTED;
                leave;
            }

            NtfsAcquireExclusiveScb( IrpContext, QuotaScb );
            NtfsAcquireExclusiveScb( IrpContext, OwnerIdScb );
            ExAcquireFastMutexUnsafe( &Vcb->QuotaControlLock );
            IndexAcquired = TRUE;

            //
            //  Make sure the delete secquence number has not changed since
            //  the scan was delete.
            //

            if (ULongToPtr( Vcb->QuotaDeleteSecquence ) != IrpContext->Union.NtfsIoContext) {

                //
                //  The scan needs to be restarted. Set the state to posted
                //  and raise status can not wait which will cause us to retry.
                //

                ClearFlag( Vcb->QuotaState, VCB_QUOTA_REPAIR_RUNNING );
                SetFlag( Vcb->QuotaState, VCB_QUOTA_REPAIR_POSTED );
                NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
            }

            Status = NtOfsReadRecords( IrpContext,
                                       QuotaScb,
                                       &ReadContext,
                                       KeyPtr,
                                       NtOfsMatchAll,
                                       NULL,
                                       &Count,
                                       IndexRow,
                                       PAGE_SIZE,
                                       RowBuffer );

            if (!NT_SUCCESS( Status )) {
                break;
            }

            QuotaRow = IndexRow;

            for (i = 0; i < Count; i += 1, QuotaRow += 1) {

                PQUOTA_CONTROL_BLOCK QuotaControl;

                UserData = QuotaRow->DataPart.Data;

                if (!FlagOn( UserData->QuotaFlags, QUOTA_FLAG_ID_DELETED )) {
                    continue;
                }

                //
                //  Check to see if there is a quota control entry
                //  for this id.
                //

                ASSERT( FIELD_OFFSET( QUOTA_CONTROL_BLOCK, OwnerId ) <= FIELD_OFFSET( INDEX_ROW, KeyPart.Key ));

                QuotaControl = RtlLookupElementGenericTable( &Vcb->QuotaControlTable,
                                                             CONTAINING_RECORD( &QuotaRow->KeyPart.Key,
                                                                                QUOTA_CONTROL_BLOCK,
                                                                                OwnerId ));

                //
                //  If there is a quota control entry or there is now
                //  some quota charged, then clear the deleted flag
                //  and update the entry.
                //

                if ((QuotaControl != NULL) || (UserData->QuotaUsed != 0)) {

                    ASSERT( (QuotaControl == NULL) && (UserData->QuotaUsed == 0) );

                    ClearFlag( UserData->QuotaFlags, QUOTA_FLAG_ID_DELETED );

                    QuotaRow->DataPart.DataLength = SIZEOF_QUOTA_USER_DATA;

                    IndexKey.KeyLength = sizeof( OwnerId );
                    IndexKey.Key = &OwnerId;
                    NtOfsUpdateRecord( IrpContext,
                                       QuotaScb,
                                       1,
                                       QuotaRow,
                                       NULL,
                                       NULL );

                    continue;
                }

                //
                //  Delete the user quota data record.
                //

                IndexKey.KeyLength = sizeof( OwnerId );
                IndexKey.Key = &OwnerId;
                NtOfsDeleteRecords( IrpContext,
                                    QuotaScb,
                                    1,
                                    &QuotaRow->KeyPart );

                //
                // Delete the owner id record.
                //

                IndexKey.Key = &UserData->QuotaSid;
                IndexKey.KeyLength = RtlLengthSid( &UserData->QuotaSid );
                NtOfsDeleteRecords( IrpContext,
                                    OwnerIdScb,
                                    1,
                                    &IndexKey );
            }

            //
            //  Release the indexes and commit what has been done so far.
            //

            ExReleaseFastMutexUnsafe( &Vcb->QuotaControlLock );
            NtfsReleaseScb( IrpContext, QuotaScb );
            NtfsReleaseScb( IrpContext, OwnerIdScb );
            NtfsReleaseVcb( IrpContext, Vcb );
            IndexAcquired = FALSE;

            //
            //  Complete the request which commits the pending
            //  transaction if there is one and releases of the
            //  acquired resources.  The IrpContext will not
            //  be deleted because the no delete flag is set.
            //

            SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_DONT_DELETE | IRP_CONTEXT_FLAG_RETAIN_FLAGS );
            NtfsCompleteRequest( IrpContext, NULL, STATUS_SUCCESS );

            //
            //  Remember how far we got so we can restart correctly.
            //

            Vcb->QuotaFileReference.SegmentNumberLowPart = *((PULONG) IndexRow[Count - 1].KeyPart.Key);

            KeyPtr = NULL;
        }

        ASSERT( (Status == STATUS_NO_MORE_MATCHES) || (Status == STATUS_NO_MATCH) );

    } finally {

        NtfsFreePool( RowBuffer );

        if (IndexAcquired) {
            ExReleaseFastMutexUnsafe( &Vcb->QuotaControlLock );
            NtfsReleaseScb( IrpContext, QuotaScb );
            NtfsReleaseScb( IrpContext, OwnerIdScb );
            NtfsReleaseVcb( IrpContext, Vcb );
        }

        if (IndexRow != NULL) {
            NtfsFreePool( IndexRow );
        }

        if (ReadContext != NULL) {
            NtOfsFreeReadContext( ReadContext );
        }
    }

    return;
}


VOID
NtfsDereferenceQuotaControlBlock (
    IN PVCB Vcb,
    IN PQUOTA_CONTROL_BLOCK *QuotaControl
    )

/*++

Routine Description:

    This routine dereferences the quota control block.
    If reference count is now zero the block will be deallocated.

Arguments:

    Vcb - Vcb for the volume that own the quota contorl block.

    QuotaControl - Quota control block to be derefernece.

Return Value:

    None.

--*/

{
    PQUOTA_CONTROL_BLOCK TempQuotaControl;
    LONG ReferenceCount;
    ULONG OwnerId;
    ULONG QuotaControlDeleteCount;

    PAGED_CODE();

    //
    //  Capture the owner id and delete count;
    //

    OwnerId = (*QuotaControl)->OwnerId;
    QuotaControlDeleteCount = Vcb->QuotaControlDeleteCount;

    //
    //  Update the reference count.
    //

    ReferenceCount = InterlockedDecrement( &(*QuotaControl)->ReferenceCount );

    ASSERT( ReferenceCount >= 0 );

    //
    // If the reference count is not zero we are done.
    //

    if (ReferenceCount != 0) {

        //
        //  Clear the pointer from the FCB and return.
        //

        *QuotaControl = NULL;
        return;
    }

    //
    //  Lock the quota table.
    //

    ExAcquireFastMutexUnsafe( &Vcb->QuotaControlLock );

    try {

        //
        //  Now things get messy.  Check the delete count.
        //

        if (QuotaControlDeleteCount != Vcb->QuotaControlDeleteCount) {

            //
            //  This is a bogus assert, but I want to see if this ever occurs.
            //

            ASSERT( QuotaControlDeleteCount != Vcb->QuotaControlDeleteCount );

            //
            //  Something has already been deleted, the old quota control
            //  block may have been deleted already.  Look it up again.
            //

            TempQuotaControl = RtlLookupElementGenericTable( &Vcb->QuotaControlTable,
                                                             CONTAINING_RECORD( &OwnerId,
                                                                                QUOTA_CONTROL_BLOCK,
                                                                                OwnerId ));

            //
            //  The block was already deleted we are done.
            //

            if (TempQuotaControl == NULL) {
                leave;
            }

        } else {

            TempQuotaControl = *QuotaControl;
            ASSERT( TempQuotaControl == RtlLookupElementGenericTable( &Vcb->QuotaControlTable,
                                                                      CONTAINING_RECORD( &OwnerId,
                                                                                         QUOTA_CONTROL_BLOCK,
                                                                                         OwnerId )));
        }

        //
        //  Verify the reference count is still zero.  The reference count
        //  cannot transision from zero to one while the quota table lock is
        //  held.
        //

        if (TempQuotaControl->ReferenceCount != 0) {
            leave;
        }

        //
        //  Increment the delete count.
        //

        InterlockedIncrement( &Vcb->QuotaControlDeleteCount );

        NtfsFreePool( TempQuotaControl->QuotaControlLock );
        RtlDeleteElementGenericTable( &Vcb->QuotaControlTable,
                                      TempQuotaControl );

    } finally {

        ExReleaseFastMutexUnsafe( &Vcb->QuotaControlLock );
        *QuotaControl = NULL;
    }

    return;
}


VOID
NtfsFixupQuota (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb
    )

/*++

Routine Description:

    This routine ensures that the charged field is correct in the
    standard information attribute of a file.  If there is a problem
    the it is fixed.

Arguments:

    Fcb - Pointer to the FCB of the file being opened.

Return Value:

    NONE

--*/

{
    LONGLONG Delta = 0;

    PAGED_CODE();

    ASSERT( FlagOn( Fcb->Vcb->QuotaFlags, QUOTA_FLAG_TRACKING_ENABLED ));
    ASSERT( NtfsIsExclusiveFcb( Fcb ));

    if (Fcb->OwnerId != QUOTA_INVALID_ID) {

        ASSERT( Fcb->QuotaControl == NULL );

        Fcb->QuotaControl = NtfsInitializeQuotaControlBlock( Fcb->Vcb, Fcb->OwnerId );
    }

    if ((NtfsPerformQuotaOperation( Fcb )) && (!NtfsIsVolumeReadOnly( Fcb->Vcb ))) {

        NtfsCalculateQuotaAdjustment( IrpContext, Fcb, &Delta );

        ASSERT( NtfsAllowFixups || FlagOn( Fcb->Vcb->QuotaState, VCB_QUOTA_REPAIR_RUNNING ) || (Delta == 0) );

        if (Delta != 0) {
#if DBG

            if (IrpContext->OriginatingIrp != NULL ) {
                PFILE_OBJECT FileObject;

                FileObject = IoGetCurrentIrpStackLocation(
                                IrpContext->OriginatingIrp )->FileObject;

                if (FileObject != NULL && FileObject->FileName.Buffer != NULL) {
                    DebugTrace( 0, Dbg, ( "NtfsFixupQuota: Quota fix up required on %Z of %I64x bytes\n",
                              &FileObject->FileName,
                              Delta ));
                }
            }
#endif

            NtfsUpdateFileQuota( IrpContext, Fcb, &Delta, TRUE, FALSE );
        }
    }

    return;
}


NTSTATUS
NtfsFsQuotaQueryInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN ULONG StartingId,
    IN BOOLEAN ReturnSingleEntry,
    IN OUT PFILE_QUOTA_INFORMATION *QuotaInfoOutBuffer,
    IN OUT PULONG Length,
    IN OUT PCCB Ccb OPTIONAL
    )

/*++

Routine Description:

    This routine returns the quota information for the volume.

Arguments:

    Vcb - Volume control block for the volume to be quered.

    StartingId - Owner Id after which to start the listing.

    ReturnSingleEntry - Indicates only one entry should be returned.

    QuotaInfoOutBuffer - Buffer to return the data. On return, points at the
    last good entry copied.

    Length - In the size of the buffer. Out the amount of space remaining.

    Ccb - Optional Ccb which is updated with the last returned owner id.

Return Value:

    Returns the status of the operation.

--*/

{
    INDEX_ROW IndexRow;
    INDEX_KEY IndexKey;
    PINDEX_KEY KeyPtr;
    PQUOTA_USER_DATA UserData;
    PVOID RowBuffer;
    NTSTATUS Status;
    ULONG OwnerId;
    ULONG Count = 1;
    PREAD_CONTEXT ReadContext = NULL;
    ULONG UserBufferLength = *Length;
    PFILE_QUOTA_INFORMATION OutBuffer = *QuotaInfoOutBuffer;

    PAGED_CODE();

    if (UserBufferLength < sizeof(FILE_QUOTA_INFORMATION)) {

        //
        //  The user buffer is way too small.
        //

        return STATUS_BUFFER_TOO_SMALL;
    }

    //
    //  Return nothing if quotas are not enabled.
    //

    if (Vcb->QuotaTableScb == NULL) {

        return STATUS_SUCCESS;
    }

    //
    //  Allocate a buffer large enough for the largest quota entry and key.
    //

    RowBuffer = NtfsAllocatePool( PagedPool, MAXIMUM_QUOTA_ROW );

    //
    //  Look up each entry in the quota index start with the next
    //  requested owner id.
    //

    OwnerId = StartingId + 1;

    if (OwnerId < QUOTA_FISRT_USER_ID) {
        OwnerId = QUOTA_FISRT_USER_ID;
    }

    IndexKey.KeyLength = sizeof( OwnerId );
    IndexKey.Key = &OwnerId;
    KeyPtr = &IndexKey;

    try {

        while (NT_SUCCESS( Status = NtOfsReadRecords( IrpContext,
                                                      Vcb->QuotaTableScb,
                                                      &ReadContext,
                                                      KeyPtr,
                                                      NtOfsMatchAll,
                                                      NULL,
                                                      &Count,
                                                      &IndexRow,
                                                      MAXIMUM_QUOTA_ROW,
                                                      RowBuffer ))) {

            ASSERT( Count == 1 );

            KeyPtr = NULL;
            UserData = IndexRow.DataPart.Data;

            //
            //  Skip this entry if it has been deleted.
            //

            if (FlagOn( UserData->QuotaFlags, QUOTA_FLAG_ID_DELETED )) {
                continue;
            }

            if (!NT_SUCCESS( Status = NtfsPackQuotaInfo(&UserData->QuotaSid,
                                                        UserData,
                                                        OutBuffer,
                                                        &UserBufferLength ))) {
                break;
            }

            //
            //  Remember the owner id of the last entry returned.
            //

            OwnerId = *((PULONG) IndexRow.KeyPart.Key);

            if (ReturnSingleEntry) {
                break;
            }

            *QuotaInfoOutBuffer = OutBuffer;
            OutBuffer = Add2Ptr( OutBuffer, OutBuffer->NextEntryOffset );
        }

        //
        //  If we're returning at least one entry, it's a SUCCESS.
        //

        if (UserBufferLength != *Length) {

            Status =  STATUS_SUCCESS;

            //
            //  Set the next entry offset to zero to
            //  indicate list termination. If we are only returning a
            //  single entry, it makes more sense to let the caller
            //  take care of it.
            //

            if (!ReturnSingleEntry) {

                (*QuotaInfoOutBuffer)->NextEntryOffset = 0;
            }

            if (Ccb != NULL) {
                Ccb->LastOwnerId = OwnerId;
            }

            //
            //  Return how much of the buffer was used up.
            //  QuotaInfoOutBuffer already points at the last good entry.
            //

            *Length = UserBufferLength;

        } else if (Status != STATUS_BUFFER_OVERFLOW) {

            //
            //  We return NO_MORE_ENTRIES if we aren't returning any
            //  entries (even when the buffer was large enough).
            //

            Status = STATUS_NO_MORE_ENTRIES;
        }

    } finally {

        NtfsFreePool( RowBuffer );

        if (ReadContext != NULL) {
            NtOfsFreeReadContext( ReadContext );
        }
    }

    return Status;
}


NTSTATUS
NtfsFsQuotaSetInfo (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFILE_QUOTA_INFORMATION FileQuotaInfo,
    IN ULONG Length
    )

/*++

Routine Description:

    This routine sets the quota information on the volume for the
    owner pasted in from the user buffer.

Arguments:

    Vcb - Volume control block for the volume to be changed.

    FileQuotaInfo - Buffer to return the data.

    Length - The size of the buffer in bytes.

Return Value:

    Returns the status of the operation.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    ULONG LengthUsed = 0;

    PAGED_CODE();

    //
    //  Return nothing if quotas are not enabled.
    //

    if (Vcb->QuotaTableScb == NULL) {

        return STATUS_INVALID_DEVICE_REQUEST;

    }

    //
    //  Validate the entire buffer before doing any work.
    //

    Status = IoCheckQuotaBufferValidity( FileQuotaInfo,
                                         Length,
                                         &LengthUsed );

    IrpContext->OriginatingIrp->IoStatus.Information = LengthUsed;

    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    LengthUsed = 0;

    //
    //  Perform the requested updates.
    //

    while (TRUE) {

        //
        //  Make sure that the administrator limit is not being changed.
        //

        if (RtlEqualSid( SeExports->SeAliasAdminsSid, &FileQuotaInfo->Sid ) &&
            (FileQuotaInfo->QuotaLimit.QuadPart != -1)) {

            //
            //  Reject the request with access denied.
            //

            NtfsRaiseStatus( IrpContext, STATUS_ACCESS_DENIED, NULL, NULL );

        }

        if (FileQuotaInfo->QuotaLimit.QuadPart == -2) {

            Status = NtfsPrepareForDelete( IrpContext,
                                           Vcb,
                                           &FileQuotaInfo->Sid );

            if (!NT_SUCCESS( Status )) {
                break;
            }

        } else {

            NtfsGetOwnerId( IrpContext,
                            &FileQuotaInfo->Sid,
                            TRUE,
                            FileQuotaInfo );
        }

        if (FileQuotaInfo->NextEntryOffset == 0) {
            break;
        }

        //
        //  Advance to the next entry.
        //

        FileQuotaInfo = Add2Ptr( FileQuotaInfo, FileQuotaInfo->NextEntryOffset);
    }

    //
    //  If the quota tracking has been requested and the quotas need to be
    //  repaired then try to repair them now.
    //

    if (FlagOn( Vcb->QuotaFlags, QUOTA_FLAG_TRACKING_REQUESTED ) &&
        FlagOn( Vcb->QuotaFlags,
                (QUOTA_FLAG_OUT_OF_DATE |
                 QUOTA_FLAG_CORRUPT |
                 QUOTA_FLAG_PENDING_DELETES) )) {

        NtfsPostRepairQuotaIndex( IrpContext, Vcb );
    }

    return Status;
}


NTSTATUS
NtfsQueryQuotaUserSidList (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFILE_GET_QUOTA_INFORMATION SidList,
    IN OUT PFILE_QUOTA_INFORMATION QuotaInfoOutBuffer,
    IN OUT PULONG BufferLength,
    IN BOOLEAN ReturnSingleEntry
    )

/*++

Routine Description:

    This routine query for the quota data for each user specified in the
    user provided sid list.

Arguments:

    Vcb - Supplies a pointer to the volume control block.

    SidList - Supplies a pointer to the Sid list.  The list has already
              been validated.

    QuotaInfoOutBuffer - Indicates where the retrived query data should be placed.

    BufferLength - Indicates that size of the buffer, and is updated with the
                  amount of data actually placed in the buffer.

    ReturnSingleEntry - Indicates if just one entry should be returned.

Return Value:

    Returns the status of the operation.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    ULONG BytesRemaining = *BufferLength;
    PFILE_QUOTA_INFORMATION LastEntry = QuotaInfoOutBuffer;
    ULONG OwnerId;

    PAGED_CODE( );

    //
    //  Loop through each of the entries.
    //

    while (TRUE) {

        //
        //  Get the owner id.
        //

        OwnerId = NtfsGetOwnerId( IrpContext,
                                  &SidList->Sid,
                                  FALSE,
                                  NULL );

       if (OwnerId != QUOTA_INVALID_ID) {

            //
            //  Send ownerid and ask for a single entry.
            //

            Status = NtfsFsQuotaQueryInfo( IrpContext,
                                           Vcb,
                                           OwnerId - 1,
                                           TRUE,
                                           &QuotaInfoOutBuffer,
                                           &BytesRemaining,
                                           NULL );

        } else {

            //
            //  Send back zeroed data alongwith the Sid.
            //

            Status = NtfsPackQuotaInfo( &SidList->Sid,
                                        NULL,
                                        QuotaInfoOutBuffer,
                                        &BytesRemaining );
        }

        //
        //  Bail out if we got a real error.
        //

        if (!NT_SUCCESS( Status ) && (Status != STATUS_NO_MORE_ENTRIES)) {

            break;
        }

        if (ReturnSingleEntry) {

            break;
        }

        //
        //  Make a note of the last entry filled in.
        //

        LastEntry = QuotaInfoOutBuffer;

        //
        //  If we've exhausted the SidList, we're done
        //

        if (SidList->NextEntryOffset == 0) {
            break;
        }

        SidList =  Add2Ptr( SidList, SidList->NextEntryOffset );

        ASSERT(QuotaInfoOutBuffer->NextEntryOffset > 0);
        QuotaInfoOutBuffer = Add2Ptr( QuotaInfoOutBuffer,
                                      QuotaInfoOutBuffer->NextEntryOffset );
    }

    //
    //  Set the next entry offset to zero to
    //  indicate list termination.
    //

    if (BytesRemaining != *BufferLength) {

        LastEntry->NextEntryOffset = 0;
        Status =  STATUS_SUCCESS;
    }

    //
    //  Update the buffer length to reflect what's left.
    //  If we've copied anything at all, we must return SUCCESS.
    //

    ASSERT( (BytesRemaining == *BufferLength) || (Status == STATUS_SUCCESS ) );
    *BufferLength = BytesRemaining;

    return Status;
}


NTSTATUS
NtfsPackQuotaInfo (
    IN PSID Sid,
    IN PQUOTA_USER_DATA QuotaUserData OPTIONAL,
    IN PFILE_QUOTA_INFORMATION OutBuffer,
    IN OUT PULONG OutBufferSize
    )

/*++
Routine Description:

    This is an internal routine that fills a given FILE_QUOTA_INFORMATION
    structure with information from a given QUOTA_USER_DATA structure.

Arguments:

    Sid - SID to be copied. Same as the one embedded inside the USER_DATA struct.
    This routine doesn't care if it's a valid sid.

    QuotaUserData - Source of data

    QuotaInfoBufferPtr - Buffer to have user data copied in to.

    OutBufferSize - IN size of the buffer, OUT size of the remaining buffer.
--*/

{
    ULONG SidLength;
    ULONG NextOffset;
    ULONG EntrySize;

    SidLength = RtlLengthSid( Sid );
    EntrySize = SidLength +  FIELD_OFFSET( FILE_QUOTA_INFORMATION, Sid );

    //
    //  Abort if this entry won't fit in the buffer.
    //

    if (*OutBufferSize < EntrySize) {

        return STATUS_BUFFER_OVERFLOW;
    }

    if (ARGUMENT_PRESENT(QuotaUserData)) {

        //
        //  Fill in the user buffer for this entry.
        //

        OutBuffer->ChangeTime.QuadPart = QuotaUserData->QuotaChangeTime;
        OutBuffer->QuotaUsed.QuadPart = QuotaUserData->QuotaUsed;
        OutBuffer->QuotaThreshold.QuadPart = QuotaUserData->QuotaThreshold;
        OutBuffer->QuotaLimit.QuadPart = QuotaUserData->QuotaLimit;

    } else {

        //
        //  Return all zeros for the data, up until the Sid.
        //

        RtlZeroMemory( OutBuffer, FIELD_OFFSET(FILE_QUOTA_INFORMATION, Sid) );
    }

    OutBuffer->SidLength = SidLength;
    RtlCopyMemory( &OutBuffer->Sid,
                   Sid,
                   SidLength );

    //
    //  Calculate the next offset.
    //

    NextOffset = QuadAlign( EntrySize );

    //
    //  Add the offset to the amount used.
    //  NextEntryOffset may be sligthly larger than Length due to
    //  rounding of the previous entry size to longlong.
    //

    if (*OutBufferSize > NextOffset) {

        *OutBufferSize -= NextOffset;
        OutBuffer->NextEntryOffset = NextOffset;

    } else {

        //
        //  We did have enough room for this entry, but quad-alignment made
        //  it look like we didn't. Return the last few bytes left
        //  (what we lost in rounding up) just for correctness, although
        //  those really won't be of much use. The NextEntryOffset will be
        //  zeroed subsequently by the caller.
        //  Note that the OutBuffer is pointing at the _beginning_ of the
        //  last entry returned in this case.
        //

        ASSERT( *OutBufferSize >= EntrySize );
        *OutBufferSize -= EntrySize;
        OutBuffer->NextEntryOffset = EntrySize;
    }

    return STATUS_SUCCESS;
}


ULONG
NtfsGetOwnerId (
    IN PIRP_CONTEXT IrpContext,
    IN PSID Sid,
    IN BOOLEAN CreateNew,
    IN PFILE_QUOTA_INFORMATION FileQuotaInfo OPTIONAL
    )

/*++

Routine Description:

    This routine determines the owner id for the requested SID.  First the
    Sid is looked up in the Owner Id index.  If the entry exists, then that
    owner id is returned.  If the sid does not exist then  new entry is
    created in the owner id index.

Arguments:

    Sid - Security id to determine the owner id.

    CreateNew - Create a new id if necessary.

    FileQuotaInfo - Optional quota data to update quota index with.

Return Value:

    ULONG - Owner Id for the security id. QUOTA_INVALID_ID is returned if id
        did not exist and CreateNew was FALSE.

--*/

{
    ULONG OwnerId;
    ULONG DefaultId;
    ULONG SidLength;
    NTSTATUS Status;
    INDEX_ROW IndexRow;
    INDEX_KEY IndexKey;
    MAP_HANDLE MapHandle;
    PQUOTA_USER_DATA NewQuotaData = NULL;
    QUICK_INDEX_HINT QuickIndexHint;
    PSCB QuotaScb;
    PVCB Vcb = IrpContext->Vcb;
    PSCB OwnerIdScb = Vcb->OwnerIdTableScb;

    BOOLEAN ExistingRecord;

    PAGED_CODE();

    //
    //  Determine the Sid length.
    //

    SidLength = RtlLengthSid( Sid );

    IndexKey.KeyLength = SidLength;
    IndexKey.Key = Sid;

    //
    //  If there is quota information to update or there are pending deletes
    //  then long path must be taken where the user quota entry is found.
    //

    if (FileQuotaInfo == NULL) {

        //
        //  Acquire the owner id index shared.
        //

        NtfsAcquireSharedScb( IrpContext, OwnerIdScb );

        try {

            //
            //  Assume the Sid is in the index.
            //

            Status = NtOfsFindRecord( IrpContext,
                                      OwnerIdScb,
                                      &IndexKey,
                                      &IndexRow,
                                      &MapHandle,
                                      NULL );

            //
            //  If the sid was found then capture is value.
            //

            if (NT_SUCCESS( Status )) {

                ASSERT( IndexRow.DataPart.DataLength == sizeof( ULONG ));
                OwnerId = *((PULONG) IndexRow.DataPart.Data);

                //
                //  Release the index map handle.
                //

                NtOfsReleaseMap( IrpContext, &MapHandle );
            }

        } finally {
            NtfsReleaseScb( IrpContext, OwnerIdScb );
        }

        //
        //  If the sid was found and there are no pending deletes, we are done.
        //

        if (NT_SUCCESS(Status)) {

            if (!FlagOn( Vcb->QuotaFlags, QUOTA_FLAG_PENDING_DELETES )) {
                return OwnerId;
            }

            //
            //  Look up the actual record to see if it is deleted.
            //

            QuotaScb = Vcb->QuotaTableScb;
            NtfsAcquireSharedScb( IrpContext, QuotaScb );

            try {

                IndexKey.KeyLength = sizeof(ULONG);
                IndexKey.Key = &OwnerId;

                Status = NtOfsFindRecord( IrpContext,
                                          QuotaScb,
                                          &IndexKey,
                                          &IndexRow,
                                          &MapHandle,
                                          NULL );

                if (!NT_SUCCESS( Status )) {

                    ASSERT( NT_SUCCESS( Status ));
                    NtfsMarkQuotaCorrupt( IrpContext, Vcb );
                    OwnerId = QUOTA_INVALID_ID;
                    leave;
                }

                if (FlagOn( ((PQUOTA_USER_DATA) IndexRow.DataPart.Data)->QuotaFlags,
                            QUOTA_FLAG_ID_DELETED )) {

                    //
                    //  Return invalid user.
                    //

                    OwnerId = QUOTA_INVALID_ID;
                }

                //
                //  Release the index map handle.
                //

                NtOfsReleaseMap( IrpContext, &MapHandle );

            } finally {

                NtfsReleaseScb( IrpContext, QuotaScb );
            }

      

    This routine updates the quota amount for a file and owner by the
    requested amount. If quota is being increated and the CheckQuota is true
    than the new quota amount will be tested for quota violations. If the
    hard limit is exceeded an error is raised.  If the LogIt flags is not set
    then changes to the standard information structure are not logged.
    Changes to the user quota data are always logged.

Arguments:

    Fcb - Fcb whose quota usage is being modified.

    Delta - Supplies the signed amount to change the quota for the file.

    LogIt - Indicates whether we should log this change.

    CheckQuota - Indicates whether we should check for quota violations.

Return Value:

    None.

--*/

{

    ULONGLONG NewQuota;
    LARGE_INTEGER CurrentTime;
    ATTRIBUTE_ENUMERATION_CONTEXT AttrContext;
    PSTANDARD_INFORMATION StandardInformation;
    PQUOTA_USER_DATA UserData;
    INDEX_ROW IndexRow;
    INDEX_KEY IndexKey;
    MAP_HANDLE MapHandle;
    NTSTATUS Status;
    PQUOTA_CONTROL_BLOCK QuotaControl = Fcb->QuotaControl;
    PVCB Vcb = Fcb->Vcb;
    ULONG Length;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsUpdateFileQuota:  Entered\n") );

    ASSERT( FlagOn( Fcb->FcbState, FCB_STATE_LARGE_STD_INFO ));


    //
    //  Readonly volumes shouldn't proceed.
    //

    if (NtfsIsVolumeReadOnly( Vcb )) {

        ASSERT( FALSE );
        NtfsRaiseStatus( IrpContext, STATUS_MEDIA_WRITE_PROTECTED, NULL, NULL );
    }

    //
    //  Use a try-finally to cleanup the attribute context.
    //

    try {

        //
        //  Initialize the context structure and map handle.
        //

        NtfsInitializeAttributeContext( &AttrContext );
        NtOfsInitializeMapHandle( &MapHandle );

        //
        //  Locate the standard information, it must be there.
        //

        if (!NtfsLookupAttributeByCode( IrpContext,
                                        Fcb,
                                        &Fcb->FileReference,
                                        $STANDARD_INFORMATION,
                                        &AttrContext )) {

            DebugTrace( 0, Dbg, ("Can't find standard information\n") );

            NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Fcb );
        }

        StandardInformation = (PSTANDARD_INFORMATION) NtfsAttributeValue( NtfsFoundAttribute( &AttrContext ));

        ASSERT( NtfsFoundAttribute( &AttrContext )->Form.Resident.ValueLength == sizeof( STANDARD_INFORMATION ));

        NewQuota = StandardInformation->QuotaCharged + *Delta;

        SetFlag( Fcb->FcbState, FCB_STATE_UPDATE_STD_INFO );

        if ((LONGLONG) NewQuota < 0) {

            //
            //  Do not let the quota data go negitive.
            //

            NewQuota = 0;
        }

        if (LogIt) {

            //
            //  Call to change the attribute value.
            //

            NtfsChangeAttributeValue( IrpContext,
                                      Fcb,
                                      FIELD_OFFSET(STANDARD_INFORMATION, QuotaCharged),
                                      &NewQuota,
                                      sizeof( StandardInformation->QuotaCharged),
                                      FALSE,
                                      FALSE,
                                      FALSE,
                                      FALSE,
                                      &AttrContext );
        } else {

            //
            //  Just update the value in the standard information
            //  it will be logged later.
            //

            StandardInformation->QuotaCharged = NewQuota;
        }

        //
        //  Update the quota information block.
        //

        NtfsAcquireQuotaControl( IrpContext, QuotaControl );

        IndexKey.KeyLength = sizeof(ULONG);
        IndexKey.Key = &QuotaControl->OwnerId;

        Status = NtOfsFindRecord( IrpContext,
                                  Vcb->QuotaTableScb,
                                  &IndexKey,
                                  &IndexRow,
                                  &MapHandle,
                                  &QuotaControl->QuickIndexHint );

        if (!(NT_SUCCESS( Status )) ||
            (IndexRow.DataPart.DataLength < SIZEOF_QUOTA_USER_DATA + FIELD_OFFSET( SID, SubAuthority )) ||
             ((ULONG)SeLengthSid( &(((PQUOTA_USER_DATA)(IndexRow.DataPart.Data))->QuotaSid)) + SIZEOF_QUOTA_USER_DATA !=
                IndexRow.DataPart.DataLength)) {

            //
            //  This look up should not fail.
            //

            ASSERT( NT_SUCCESS( Status ));
            ASSERTMSG(( "NTFS: corrupt quotasid\n" ), FALSE);

            NtfsMarkQuotaCorrupt( IrpContext, IrpContext->Vcb );
            leave;
        }

        //
        //  Space is allocated for the new record after the quota control
        //  block.
        //

        UserData = (PQUOTA_USER_DATA) (QuotaControl + 1);
        ASSERT( IndexRow.DataPart.DataLength >= SIZEOF_QUOTA_USER_DATA );

        RtlCopyMemory( UserData,
                       IndexRow.DataPart.Data,
                       SIZEOF_QUOTA_USER_DATA );

        ASSERT( (LONGLONG) UserData->QuotaUsed >= -*Delta );

        UserData->QuotaUsed += *Delta;

        if ((LONGLONG) UserData->QuotaUsed < 0) {

            //
            //  Do not let the quota data go negative.
            //

            UserData->QuotaUsed = 0;
        }

        //
        //  Indicate only the quota used field has been set so far.
        //

        Length = FIELD_OFFSET( QUOTA_USER_DATA, QuotaChangeTime );

        //
        //  Only update the quota modified time if this is the last cleanup
        //  for the owner.
        //

        if (IrpContext->MajorFunction == IRP_MJ_CLEANUP) {

            KeQuerySystemTime( &CurrentTime );
            UserData->QuotaChangeTime = CurrentTime.QuadPart;

            ASSERT( Length <= FIELD_OFFSET( QUOTA_USER_DATA, QuotaThreshold ));
            Length = FIELD_OFFSET( QUOTA_USER_DATA, QuotaThreshold );
        }

        if (CheckQuota && (*Delta > 0)) {

            if ((UserData->QuotaUsed > UserData->QuotaLimit) &&
                (UserData->QuotaUsed >= (UserData->QuotaLimit + Vcb->BytesPerCluster))) {

                KeQuerySystemTime( &CurrentTime );
                UserData->QuotaChangeTime = CurrentTime.QuadPart;

                if (FlagOn( Vcb->QuotaFlags, QUOTA_FLAG_LOG_LIMIT ) &&
                    (!FlagOn( UserData->QuotaFlags, QUOTA_FLAG_LIMIT_REACHED ) ||
                     ((ULONGLONG) CurrentTime.QuadPart > UserData->QuotaExceededTime + NtfsMaxQuotaNotifyRate))) {

                    if (FlagOn( Vcb->QuotaFlags, QUOTA_FLAG_ENFORCEMENT_ENABLED) &&
                        (Vcb->AdministratorId != QuotaControl->OwnerId)) {

                        //
                        //  The operation to mark the user's quota data entry
                        //  must be posted since any changes to the entry
                        //  will be undone by the following raise.
                        //

                        NtfsPostUserLimit( IrpContext, Vcb, QuotaControl );
                        NtfsRaiseStatus( IrpContext, STATUS_DISK_FULL, NULL, Fcb );

                    } else {

                        //
                        //  Log the fact that quota was exceeded.
                        //

                        if (NtfsLogEvent( IrpContext,
                                          IndexRow.DataPart.Data,
                                          IO_FILE_QUOTA_LIMIT,
                                          STATUS_SUCCESS )) {

                            //
                            //  The event was successfuly logged.  Do not log
                            //  another for a while.
                            //

                            DebugTrace( 0, Dbg, ("NtfsUpdateFileQuota: Quota Limit exceeded. OwnerId = %lx\n", QuotaControl->OwnerId));

                            UserData->QuotaExceededTime = CurrentTime.QuadPart;
                            SetFlag( UserData->QuotaFlags, QUOTA_FLAG_LIMIT_REACHED );

                            //
                            //  Log all of the changed data.
                            //

                            Length = SIZEOF_QUOTA_USER_DATA;
                        }
                    }

                } else if (FlagOn( Vcb->QuotaFlags, QUOTA_FLAG_ENFORCEMENT_ENABLED) &&
                           (Vcb->AdministratorId != QuotaControl->OwnerId)) {

                    NtfsRaiseStatus( IrpContext, STATUS_DISK_FULL, NULL, Fcb );
                }

            }

            if (UserData->QuotaUsed > UserData->QuotaThreshold) {

                KeQuerySystemTime( &CurrentTime );
                UserData->QuotaChangeTime = CurrentTime.QuadPart;

                if ((ULONGLONG) CurrentTime.QuadPart >
                    (UserData->QuotaExceededTime + NtfsMaxQuotaNotifyRate)) {

                    if (FlagOn( Vcb->QuotaFlags, QUOTA_FLAG_LOG_THRESHOLD)) {

                        if (NtfsLogEvent( IrpContext,
                                          IndexRow.DataPart.Data,
                                          IO_FILE_QUOTA_THRESHOLD,
                                          STATUS_SUCCESS )) {

                            //
                            //  The event was successfuly logged.  Do not log
                            //  another for a while.
                            //

                            DebugTrace( 0, Dbg, ("NtfsUpdateFileQuota: Quota threshold exceeded. OwnerId = %lx\n", QuotaControl->OwnerId));

                            UserData->QuotaExceededTime = CurrentTime.QuadPart;

                            //
                            //  Log all of the changed data.
                            //

                            Length = SIZEOF_QUOTA_USER_DATA;
                        }
                    }

                    //
                    //  Now is a good time to clear the limit reached flag.
                    //

                    ClearFlag( UserData->QuotaFlags, QUOTA_FLAG_LIMIT_REACHED );
                }
            }
        }

        //
        //  Always clear the deleted flag.
        //

        ClearFlag( UserData->QuotaFlags, QUOTA_FLAG_ID_DELETED );

        //
        // Only log the part that changed.
        //

        IndexRow.KeyPart.Key = &QuotaControl->OwnerId;
        ASSERT( IndexRow.KeyPart.KeyLength == sizeof(ULONG) );
        IndexRow.DataPart.Data = UserData;
        IndexRow.DataPart.DataLength = Length;

        NtOfsUpdateRecord( IrpContext,
                         Vcb->QuotaTableScb,
                         1,
                         &IndexRow,
                         &QuotaControl->QuickIndexHint,
                         &MapHandle );

    } finally {

        DebugUnwind( NtfsUpdateFileQuota );

        NtfsCleanupAttributeContext( IrpContext, &AttrContext );
        NtOfsReleaseMap( IrpContext, &MapHandle );

        DebugTrace( -1, Dbg, ("NtfsUpdateFileQuota:  Exit\n") );
    }

    return;
}


VOID
NtfsSaveQuotaFlags (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine saves the quota flags in the defaults quota entry.

Arguments:

    Vcb - Volume control block for volume be query.

Return Value:

    None.

--*/

{
    ULONG OwnerId;
    NTSTATUS Status;
    INDEX_ROW IndexRow;
    INDEX_KEY IndexKey;
    MAP_HANDLE MapHandle;
    QUICK_INDEX_HINT QuickIndexHint;
    QUOTA_USER_DATA NewQuotaData;
    PSCB QuotaScb;

    PAGED_CODE();

    //
    //  Acquire quota index exclusive.
    //

    QuotaScb = Vcb->QuotaTableScb;
    NtfsAcquireExclusiveScb( IrpContext, QuotaScb );
    NtOfsInitializeMapHandle( &MapHandle );
    ExAcquireFastMutexUnsafe( &Vcb->QuotaControlLock );

    try {

        //
        //  Find the default quota record and update it.
        //

        OwnerId = QUOTA_DEFAULTS_ID;
        IndexKey.KeyLength = sizeof(ULONG);
        IndexKey.Key = &OwnerId;

        RtlZeroMemory( &QuickIndexHint, sizeof( QuickIndexHint ));

        Status = NtOfsFindRecord( IrpContext,
                                  QuotaScb,
                                  &IndexKey,
                                  &IndexRow,
                                  &MapHandle,
                                  &QuickIndexHint );

        if (!NT_SUCCESS( Status )) {
            NtfsRaiseStatus( IrpContext, STATUS_QUOTA_LIST_INCONSISTENT, NULL, QuotaScb->Fcb );
        }

        ASSERT( IndexRow.DataPart.DataLength <= sizeof( QUOTA_USER_DATA ));

        RtlCopyMemory( &NewQuotaData,
                       IndexRow.DataPart.Data,
                       IndexRow.DataPart.DataLength );

        //
        //  Update the changed fields in the record.
        //

        NewQuotaData.QuotaFlags = Vcb->QuotaFlags;

        //
        //  Note the sizes in the IndexRow stay the same.
        //

        IndexRow.KeyPart.Key = &OwnerId;
        ASSERT( IndexRow.KeyPart.KeyLength == sizeof(ULONG) );
        IndexRow.DataPart.Data = &NewQuotaData;

        NtOfsUpdateRecord( IrpContext,
                           QuotaScb,
                           1,
                           &IndexRow,
                           &QuickIndexHint,
                           &MapHandle );

        ClearFlag( Vcb->QuotaState, VCB_QUOTA_SAVE_QUOTA_FLAGS);

    } finally {

        //
        //  Release the index map handle and scb.
        //

        ExReleaseFastMutexUnsafe( &Vcb->QuotaControlLock );
        NtOfsReleaseMap( IrpContext, &MapHandle );
        NtfsReleaseScb( IrpContext, QuotaScb );
    }

    return;
}


VOID
NtfsSaveQuotaFlagsSafe (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine safely saves the quota flags in the defaults quota entry.
    It acquires the volume shared, checks to see if it is ok to write,
    updates the flags and finally commits the transaction.

Arguments:

    Vcb - Volume control block for volume be query.

Return Value:

    None.

--*/

{
    PAGED_CODE();

    ASSERT( IrpContext->TransactionId == 0);

    NtfsAcquireSharedVcb( IrpContext, Vcb, TRUE );

    try {

        //
        //  Acquire the VCB shared and check whether we should
        //  continue.
        //

        if (!NtfsIsVcbAvailable( Vcb )) {

            //
            //  The volume is going away, bail out.
            //

            NtfsRaiseStatus( IrpContext, STATUS_VOLUME_DISMOUNTED, NULL, NULL );
        }

        //
        //  Do the work.
        //

        NtfsSaveQuotaFlags( IrpContext, Vcb );

        //
        //  Set the irp context flags to indicate that we are in the
        //  fsp and that the irp context should not be deleted when
        //  complete request or process exception are called. The in
        //  fsp flag keeps us from raising in a few places.  These
        //  flags must be set inside the loop since they are cleared
        //  under certain conditions.
        //

        SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_DONT_DELETE | IRP_CONTEXT_FLAG_RETAIN_FLAGS );
        SetFlag( IrpContext->State, IRP_CONTEXT_STATE_IN_FSP);

        NtfsCompleteRequest( IrpContext, NULL, STATUS_SUCCESS );

    } finally {

        NtfsReleaseVcb( IrpContext, Vcb );
    }

    return;
}


VOID
NtfsUpdateQuotaDefaults (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PFILE_FS_CONTROL_INFORMATION FileControlInfo
    )

/*++

Routine Description:

    This function updates the default settings index entry for quotas.

Arguments:

    Vcb - Volume control block for volume be query.

    FileQuotaInfo - Optional quota data to update quota index with.

Return Value:

    None.

--*/

{
    ULONG OwnerId;
    NTSTATUS Status;
    INDEX_ROW IndexRow;
    INDEX_KEY IndexKey;
    MAP_HANDLE MapHandle;
    QUOTA_USER_DATA NewQuotaData;
    QUICK_INDEX_HINT QuickIndexHint;
    ULONG Flags;
    PSCB QuotaScb;

    PAGED_CODE();

    //
    //  Acquire quota index exclusive.
    //

    QuotaScb = Vcb->QuotaTableScb;
    NtfsAcquireExclusiveScb( IrpContext, QuotaScb );
    NtOfsInitializeMapHandle( &MapHandle );
    ExAcquireFastMutexUnsafe( &Vcb->QuotaControlLock );

    try {

        //
        //  Find the default quota record and update it.
        //

        OwnerId = QUOTA_DEFAULTS_ID;
        IndexKey.KeyLength = sizeof( ULONG );
        IndexKey.Key = &OwnerId;

        RtlZeroMemory( &QuickIndexHint, sizeof( QuickIndexHint ));

        Status = NtOfsFindRecord( IrpContext,
                                  QuotaScb,
                                  &IndexKey,
                                  &IndexRow,
                                  &MapHandle,
                                  &QuickIndexHint );

        if (!NT_SUCCESS( Status )) {
            NtfsRaiseStatus( IrpContext, STATUS_QUOTA_LIST_INCONSISTENT, NULL, QuotaScb->Fcb );
        }

        ASSERT( IndexRow.DataPart.DataLength == SIZEOF_QUOTA_USER_DATA );

        RtlCopyMemory( &NewQuotaData,
                       IndexRow.DataPart.Data,
                       IndexRow.DataPart.DataLength );

        //
        //  Update the changed fields in the record.
        //

        NewQuotaData.QuotaThreshold = FileControlInfo->DefaultQuotaThreshold.QuadPart;
        NewQuotaData.QuotaLimit = FileControlInfo->DefaultQuotaLimit.QuadPart;
        KeQuerySystemTime( (PLARGE_INTEGER) &NewQuotaData.QuotaChangeTime );

        //
        //  Update the quota flags.
        //

        Flags = FlagOn( FileControlInfo->FileSystemControlFlags,
                        FILE_VC_QUOTA_MASK );

        switch (Flags) {

        case FILE_VC_QUOTA_NONE:

            //
            //  Disable quotas
            //

            ClearFlag( Vcb->QuotaFlags,
                       (QUOTA_FLAG_TRACKING_ENABLED |
                        QUOTA_FLAG_ENFORCEMENT_ENABLED |
                        QUOTA_FLAG_TRACKING_REQUESTED) );

            break;

        case FILE_VC_QUOTA_TRACK:

            //
            //  Clear the enforment flags.
            //

            ClearFlag( Vcb->QuotaFlags, QUOTA_FLAG_ENFORCEMENT_ENABLED );

            //
            //  Request tracking be enabled.
            //

            SetFlag( Vcb->QuotaFlags, QUOTA_FLAG_TRACKING_REQUESTED );
            break;

        case FILE_VC_QUOTA_ENFORCE:

            //
            //  Set the enforcement and tracking enabled flags.
            //

            SetFlag( Vcb->QuotaFlags,
                     QUOTA_FLAG_ENFORCEMENT_ENABLED | QUOTA_FLAG_TRACKING_REQUESTED);

            break;
        }

        //
        //  If quota tracking is not now
        //  enabled then the quota data will need
        //  to be rebuild so indicate quotas are out of date.
        //  Note the out of date flags always set of quotas
        //  are disabled.
        //

        if (!FlagOn( Vcb->QuotaFlags, QUOTA_FLAG_TRACKING_ENABLED )) {
            SetFlag( Vcb->QuotaFlags, QUOTA_FLAG_OUT_OF_DATE );
        }

        //
        //  Track the logging flags.
        //

        ClearFlag( Vcb->QuotaFlags,
                   QUOTA_FLAG_LOG_THRESHOLD | QUOTA_FLAG_LOG_LIMIT );

        if (FlagOn( FileControlInfo->FileSystemControlFlags, FILE_VC_LOG_QUOTA_THRESHOLD )) {

            SetFlag( Vcb->QuotaFlags, QUOTA_FLAG_LOG_THRESHOLD );
        }

        if (FlagOn( FileControlInfo->FileSystemControlFlags, FILE_VC_LOG_QUOTA_LIMIT )) {

            SetFlag( Vcb->QuotaFlags, QUOTA_FLAG_LOG_LIMIT );
        }

        SetFlag( Vcb->QuotaState, VCB_QUOTA_SAVE_QUOTA_FLAGS );

        //
        //  Save the new flags in the new index entry.
        //

        NewQuotaData.QuotaFlags = Vcb->QuotaFlags;

        //
        //  Note the sizes in the IndexRow stays the same.
        //

        IndexRow.KeyPart.Key = &OwnerId;
        ASSERT( IndexRow.KeyPart.KeyLength == sizeof( ULONG ));
        IndexRow.DataPart.Data = &NewQuotaData;

        NtOfsUpdateRecord( IrpContext,
                           QuotaScb,
                           1,
                           &IndexRow,
                           &QuickIndexHint,
                           &MapHandle );

        ClearFlag( Vcb->QuotaState, VCB_QUOTA_SAVE_QUOTA_FLAGS );

    } finally {

        //
        //  Release the index map handle and scb.
        //

        ExReleaseFastMutexUnsafe( &Vcb->QuotaControlLock );
        NtOfsReleaseMap( IrpContext, &MapHandle );
        NtfsReleaseScb( IrpContext, QuotaScb );
    }

    //
    //  If the quota tracking has been requested and the quotas need to be
    //  repaired then try to repair them now.
    //

    if (FlagOn( Vcb->QuotaFlags, QUOTA_FLAG_TRACKING_REQUESTED ) &&
        FlagOn( Vcb->QuotaFlags,
                QUOTA_FLAG_OUT_OF_DATE | QUOTA_FLAG_CORRUPT | QUOTA_FLAG_PENDING_DELETES )) {

        NtfsPostRepairQuotaIndex( IrpContext, Vcb );
    }

    return;
}


NTSTATUS
NtfsVerifyOwnerIndex (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine iterates over the owner id index and verifies the pointer
    to the quota user data index.

Arguments:

    Vcb - Pointer to the volume control block whoes index is to be operated
          on.

Return Value:

    Returns a status indicating if the owner index was ok.

--*/

{
    INDEX_KEY IndexKey;
    INDEX_ROW QuotaRow;
    MAP_HANDLE MapHandle;
    PQUOTA_USER_DATA UserData;
    PINDEX_ROW OwnerRow;
    PVOID RowBuffer;
    NTSTATUS Status;
    NTSTATUS ReturnStatus = STATUS_SUCCESS;
    ULONG Count;
    ULONG i;
    PSCB QuotaScb = Vcb->QuotaTableScb;
    PSCB OwnerIdScb = Vcb->OwnerIdTableScb;
    PINDEX_ROW IndexRow = NULL;
    PREAD_CONTEXT ReadContext = NULL;
    BOOLEAN IndexAcquired = FALSE;

    NtOfsInitializeMapHandle( &MapHandle );

    //
    //  Allocate a buffer lager enough for several rows.
    //

    RowBuffer = NtfsAllocatePool( PagedPool, PAGE_SIZE );

    try {

        //
        //  Allocate a bunch of index row entries.
        //

        Count = PAGE_SIZE / sizeof( SID );

        IndexRow = NtfsAllocatePool( PagedPool,
                                     Count * sizeof( INDEX_ROW ));

        //
        //  Iterate through the owner id entries.  Start with a zero sid.
        //

        RtlZeroMemory( IndexRow, sizeof( SID ));
        IndexKey.KeyLength = sizeof( SID );
        IndexKey.Key = IndexRow;

        Status = NtOfsReadRecords( IrpContext,
                                   OwnerIdScb,
                                   &ReadContext,
                                   &IndexKey,
                                   NtOfsMatchAll,
                                   NULL,
                                   &Count,
                                   IndexRow,
                                   PAGE_SIZE,
                                   RowBuffer );

        while (NT_SUCCESS( Status )) {

            //
            //  Acquire the VCB shared and check whether we should
            //  continue.
            //

            NtfsAcquireSharedVcb( IrpContext, Vcb, TRUE );

            if (!NtfsIsVcbAvailable( Vcb )) {

                //
                //  The volume is going away, bail out.
                //

                NtfsReleaseVcb( IrpContext, Vcb );
                Status = STATUS_VOLUME_DISMOUNTED;
                leave;
            }

            NtfsAcquireExclusiveScb( IrpContext, QuotaScb );
            NtfsAcquireExclusiveScb( IrpContext, OwnerIdScb );
            IndexAcquired = TRUE;

            OwnerRow = IndexRow;

            for (i = 0; i < Count; i += 1, OwnerRow += 1) {

                IndexKey.KeyLength = OwnerRow->DataPart.DataLength;
                IndexKey.Key = OwnerRow->DataPart.Data;

                //
                //  Look up the Owner id in the quota index.
                //

                Status = NtOfsFindRecord( IrpContext,
                                          QuotaScb,
                                          &IndexKey,
                                          &QuotaRow,
                                          &MapHandle,
                                          NULL );


                ASSERT( NT_SUCCESS( Status ));

                if (!NT_SUCCESS( Status )) {

                    //
                    //  The quota entry is missing just delete this row;
                    //

                    NtOfsDeleteRecords( IrpContext,
                                        OwnerIdScb,
                                        1,
                                        &OwnerRow->KeyPart );

                    continue;
                }

                UserData = QuotaRow.DataPart.Data;

                ASSERT( (OwnerRow->KeyPart.KeyLength == QuotaRow.DataPart.DataLength - SIZEOF_QUOTA_USER_DATA) &&
                        RtlEqualMemory( OwnerRow->KeyPart.Key, &UserData->QuotaSid, OwnerRow->KeyPart.KeyLength ));

                if ((OwnerRow->KeyPart.KeyLength != QuotaRow.DataPart.DataLength - SIZEOF_QUOTA_USER_DATA) ||
                    !RtlEqualMemory( OwnerRow->KeyPart.Key,
                                     &UserData->QuotaSid,
                                     OwnerRow->KeyPart.KeyLength )) {

                    NtOfsReleaseMap( IrpContext, &MapHandle );

                    //
                    //  The Sids do not match delete both of these records.
                    //  This causes the user whatever their Sid is to get
                    //  the defaults.
                    //


                    NtOfsDeleteRecords( IrpContext,
                                        OwnerIdScb,
                                        1,
                                        &OwnerRow->KeyPart );

                    NtOfsDeleteRecords( IrpContext,
                                        QuotaScb,
                                        1,
                                        &IndexKey );

                    ReturnStatus = STATUS_QUOTA_LIST_INCONSISTENT;
                }

                NtOfsReleaseMap( IrpContext, &MapHandle );
            }

            //
            //  Release the indexes and commit what has been done so far.
            //

            NtfsReleaseScb( IrpContext, QuotaScb );
            NtfsReleaseScb( IrpContext, OwnerIdScb );
            NtfsReleaseVcb( IrpContext, Vcb );
            IndexAcquired = FALSE;

            //
            //  Complete the request which commits the pending
            //  transaction if there is one and releases of the
            //  acquired resources.  The IrpContext will not
            //  be deleted because the no delete flag is set.
            //

            SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_DONT_DELETE | IRP_CONTEXT_FLAG_RETAIN_FLAGS );
            NtfsCompleteRequest( IrpContext, NULL, STATUS_SUCCESS );

            //
            //  Look up the next set of entries in the quota index.
            //

            Count = PAGE_SIZE / sizeof( SID );
            Status = NtOfsReadRecords( IrpContext,
                                       OwnerIdScb,
                                       &ReadContext,
                                       NULL,
                                       NtOfsMatchAll,
                                       NULL,
                                       &Count,
                                       IndexRow,
                                       PAGE_SIZE,
                                       RowBuffer );
        }

        ASSERT( (Status == STATUS_NO_MORE_MATCHES) || (Status == STATUS_NO_MATCH) );

    } finally {

        NtfsFreePool( RowBuffer );
        NtOfsReleaseMap( IrpContext, &MapHandle );

        if (IndexAcquired) {
            NtfsReleaseScb( IrpContext, QuotaScb );
            NtfsReleaseScb( IrpContext, OwnerIdScb );
            NtfsReleaseVcb( IrpContext, Vcb );
        }

        if (IndexRow != NULL) {
            NtfsFreePool( IndexRow );
        }

        if (ReadContext != NULL) {
            NtOfsFreeReadContext( ReadContext );
        }
    }

    return ReturnStatus;
}


RTL_GENERIC_COMPARE_RESULTS
NtfsQuotaTableCompare (
    IN PRTL_GENERIC_TABLE Table,
    PVOID FirstStruct,
    PVOID SecondStruct
    )

/*++

Routine Description:

    This is a generic table support routine to compare two quota table elements

Arguments:

    Table - Supplies the generic table being queried.  Not used.

    FirstStruct - Supplies the first quota table element to compare

    SecondStruct - Supplies the second quota table element to compare

Return Value:

    RTL_GENERIC_COMPARE_RESULTS - The results of comparing the two
        input structures

--*/
{
    ULONG Key1 = ((PQUOTA_CONTROL_BLOCK) FirstStruct)->OwnerId;
    ULONG Key2 = ((PQUOTA_CONTROL_BLOCK) SecondStruct)->OwnerId;

    PAGED_CODE();

    if (Key1 < Key2) {

        return GenericLessThan;
    }

    if (Key1 > Key2) {

        return GenericGreaterThan;
    }

    return GenericEqual;

    UNREFERENCED_PARAMETER( Table );
}

PVOID
NtfsQuotaTableAllocate (
    IN PRTL_GENERIC_TABLE Table,
    CLONG ByteSize
    )

/*++

Routine Description:

    This is a generic table support routine to allocate memory

Arguments:

    Table - Supplies the generic table being used

    ByteSize - Supplies the number of bytes to allocate

Return Value:

    PVOID - Returns a pointer to the allocated data

--*/

{
    PAGED_CODE();

    return NtfsAllocatePoolWithTag( PagedPool, ByteSize, 'QftN' );

    UNREFERENCED_PARAMETER( Table );
}

VOID
NtfsQuotaTableFree (
    IN PRTL_GENERIC_TABLE Table,
    IN PVOID Buffer
    )

/*++

Routine Description:

    This is a generic table support routine to free memory

Arguments:

    Table - Supplies the generic table being used

    Buffer - Supplies pointer to the buffer to be freed

Return Value:

    None

--*/

{
    PAGED_CODE();

    NtfsFreePool( Buffer );

    UNREFERENCED_PARAMETER( Table );
}


ULONG
NtfsGetCallersUserId (
    IN PIRP_CONTEXT IrpContext
    )

/*++

Routine Description:

    This routine finds the calling thread's SID and translates it to an
    owner id.

Arguments:

Return Value:

    Returns the owner id.

--*/

{
    SECURITY_SUBJECT_CONTEXT SubjectContext;
    PACCESS_TOKEN Token;
    PTOKEN_USER UserToken = NULL;
    NTSTATUS Status;
    ULONG OwnerId;

    PAGED_CODE();

    SeCaptureSubjectContext( &SubjectContext );

    try {

        Token = SeQuerySubjectContextToken( &SubjectContext );

        Status = SeQueryInformationToken( Token, TokenOwner, &UserToken );


        if (!NT_SUCCESS( Status )) {
            NtfsRaiseStatus( IrpContext, Status, NULL, NULL );
        }


        OwnerId = NtfsGetOwnerId( IrpContext, UserToken->User.Sid, FALSE, NULL );

        if (OwnerId == QUOTA_INVALID_ID) {

            //
            //  If the user does not currently have an id on this
            //  system just use the current defaults.
            //

            OwnerId = QUOTA_DEFAULTS_ID;
        }

    } finally {

        if (UserToken != NULL) {
            NtfsFreePool( UserToken);
        }

        SeReleaseSubjectContext( &SubjectContext );
    }

    return OwnerId;
}

                                                                                                                                                                                                                                                                                                                                                                 /*++


Copyright (c) 1991  Microsoft Corporation

Module Name:

    Read.c

Abstract:

    This module implements the File Read routine for Ntfs called by the
    dispatch driver.

Author:

    Brian Andrew    BrianAn         15-Aug-1991

Revision History:

--*/

#include "NtfsProc.h"
#include "lockorder.h"

//
//  The local debug trace level
//

#define Dbg                              (DEBUG_TRACE_READ)
#if __NDAS_NTFS_DBG__
#define Dbg2                             (DEBUG_INFO_READ)
#endif

#ifdef NTFS_RWC_DEBUG
PRWC_HISTORY_ENTRY
NtfsGetHistoryEntry (
    IN PSCB Scb
    );
BOOLEAN NtfsBreakOnConflict = TRUE;
#endif

//
//  Define stack overflow read threshhold.
//

#ifdef _X86_
#if DBG
#define OVERFLOW_READ_THRESHHOLD         (0xD00)
#else
#define OVERFLOW_READ_THRESHHOLD         (0xA00)
#endif
#else
#define OVERFLOW_READ_THRESHHOLD         (0x1000)
#endif // _X86_

//
//  Local procedure prototypes
//

//
//  The following procedure is used to handling read stack overflow operations.
//

VOID
NtfsStackOverflowRead (
    IN PVOID Context,
    IN PKEVENT Event
    );

VOID 
NtfsNonCachedResidentRead (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PSCB Scb,
    IN ULONG StartingVbo,
    IN ULONG ByteCount
    );

//
//  VOID
//  SafeZeroMemory (
//      IN PUCHAR At,
//      IN ULONG ByteCount
//      );
//

//
//  This macro just puts a nice little try-except around RtlZeroMemory
//

#define SafeZeroMemory(AT,BYTE_COUNT) {                            \
    try {                                                          \
        RtlZeroMemory((AT), (BYTE_COUNT));                         \
    } except(EXCEPTION_EXECUTE_HANDLER) {                          \
         NtfsRaiseStatus( IrpContext, STATUS_INVALID_USER_BUFFER, NULL, NULL );\
    }                                                              \
}

#define CollectReadStats(VCB,OPEN_TYPE,SCB,FCB,BYTE_COUNT) {                             \
    PFILE_SYSTEM_STATISTICS FsStats = &(VCB)->Statistics[KeGetCurrentProcessorNumber()]; \
    if (!FlagOn( (FCB)->FcbState, FCB_STATE_SYSTEM_FILE)) {                              \
        if (NtfsIsTypeCodeUserData( (SCB)->AttributeTypeCode )) {                        \
            FsStats->Common.UserFileReads += 1;                                          \
            FsStats->Common.UserFileReadBytes += (ULONG)(BYTE_COUNT);                    \
        } else {                                                                         \
            FsStats->Ntfs.UserIndexReads += 1;                                           \
            FsStats->Ntfs.UserIndexReadBytes += (ULONG)(BYTE_COUNT);                     \
        }                                                                                \
    } else {                                                                             \
        if ((SCB) != (VCB)->LogFileScb) {                                                \
            FsStats->Common.MetaDataReads += 1;                                          \
            FsStats->Common.MetaDataReadBytes += (ULONG)(BYTE_COUNT);                    \
        } else {                                                                         \
            FsStats->Ntfs.LogFileReads += 1;                                             \
            FsStats->Ntfs.LogFileReadBytes += (ULONG)(BYTE_COUNT);                       \
        }                                                                                \
                                                                                         \
        if ((SCB) == (VCB)->MftScb) {                                                    \
            FsStats->Ntfs.MftReads += 1;                                                 \
            FsStats->Ntfs.MftReadBytes += (ULONG)(BYTE_COUNT);                           \
        } else if ((SCB) == (VCB)->RootIndexScb) {                                       \
            FsStats->Ntfs.RootIndexReads += 1;                                           \
            FsStats->Ntfs.RootIndexReadBytes += (ULONG)(BYTE_COUNT);                     \
        } else if ((SCB) == (VCB)->BitmapScb) {                                          \
            FsStats->Ntfs.BitmapReads += 1;                                              \
            FsStats->Ntfs.BitmapReadBytes += (ULONG)(BYTE_COUNT);                        \
        } else if ((SCB) == (VCB)->MftBitmapScb) {                                       \
            FsStats->Ntfs.MftBitmapReads += 1;                                           \
            FsStats->Ntfs.MftBitmapReadBytes += (ULONG)(BYTE_COUNT);                     \
        }                                                                                \
    }                                                                                    \
}


NTSTATUS
NtfsFsdRead (
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    )

/*++

Routine Description:

    For synchronous requests, the CommonRead is called with Wait == TRUE,
    which means the request will always be completed in the current thread,
    and never passed to the Fsp.  If it is not a synchronous request,
    CommonRead is called with Wait == FALSE, which means the request
    will be passed to the Fsp only if there is a need to block.

Arguments:

    IrpContext - If present this an IrpContext put on the caller's stack
        to avoid having to allocate it from pool.

    Irp - Supplies the Irp being processed

Return Value:

    NTSTATUS - The FSD status for the IRP

--*/

{
    TOP_LEVEL_CONTEXT TopLevelContext;
    PTOP_LEVEL_CONTEXT ThreadTopLevelContext;

    NTSTATUS Status = STATUS_SUCCESS;
    PIRP_CONTEXT IrpContext = NULL;
    ULONG RetryCount = 0;

#if __NDAS_NTFS__

	if ((PVOID)NdasNtfsControlDeviceObject == VolumeDeviceObject) {

		Status = Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest( Irp, IO_DISK_INCREMENT );

		return Status;
	}

#endif

    ASSERT_IRP( Irp );

    DebugTrace( +1, Dbg, ("NtfsFsdRead\n") );

    //
    //  Call the common Read routine
    //

    FsRtlEnterFileSystem();

    //
    //  Always make the reads appear to be top level.  As long as we don't have
    //  log file full we won't post these requests.  This will prevent paging
    //  reads from trying to attach to uninitialized top level requests.
    //    

    ThreadTopLevelContext = NtfsInitializeTopLevelIrp( &TopLevelContext, TRUE, TRUE );

    ASSERT( ThreadTopLevelContext == &TopLevelContext );

    do {

        try {

            //
            //  We are either initiating this request or retrying it.
            //

            if (IrpContext == NULL) {
                                                                             
                //
                //  Allocate the Irp and update the top level storage. For synchronous
                //  paging io allocate the irp on the stack
                //

                if (CanFsdWait( Irp ) && FlagOn( Irp->Flags, IRP_PAGING_IO )) {

                    IrpContext = (PIRP_CONTEXT) NtfsAllocateFromStack( sizeof( IRP_CONTEXT ));
                }

                NtfsInitializeIrpContext( Irp, CanFsdWait( Irp ), &IrpContext );

                if (ThreadTopLevelContext->ScbBeingHotFixed != NULL) {

                    SetFlag( IrpContext->Flags, IRP_CONTEXT_FLAG_HOTFIX_UNDERWAY );
                }

                //
                //  Initialize the thread top level structure, if needed.
                //

                NtfsUpdateIrpContextWithTopLevel( IrpContext, ThreadTopLevelContext );

            } else if (Status == STATUS_LOG_FILE_FULL) {

                NtfsCheckpointForLogFileFull( IrpContext );
            }

            //
            //  If this is an Mdl complete request, don't go through
            //  common read.
            //

            ASSERT(!FlagOn( IrpContext->MinorFunction, IRP_MN_DPC ));

            if (FlagOn( IrpContext->MinorFunction, IRP_MN_COMPLETE )) {

                DebugTrace( 0, Dbg, ("Calling NtfsCompleteMdl\n") );

                Status = NtfsCompleteMdl( IrpContext, Irp );

            //
            //  Check if we have enough stack space to process this request.  If there
            //  isn't enough then we will create a new thread to process this single
            //  request
            //

            } else if (IoGetRemainingStackSize() < OVERFLOW_READ_THRESHHOLD) {

                KEVENT Event;
                PFILE_OBJECT FileObject;
                TYPE_OF_OPEN TypeOfOpen;
                PVCB Vcb;
                PFCB Fcb;
                PSCB Scb;
                PCCB Ccb;
                PERESOURCE Resource;

                DebugTrace( 0, Dbg, ("Getting too close to stack limit pass request to Fsp\n") );

                //
                //  Decode the file object to get the Scb
                //

                FileObject = IoGetCurrentIrpStackLocation(Irp)->FileObject;

                TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );
                if ((TypeOfOpen != UserFileOpen) &&
                    (TypeOfOpen != StreamFileOpen) &&
                    (TypeOfOpen != UserVolumeOpen)) {

                    NtfsCompleteRequest( IrpContext, Irp, STATUS_INVALID_DEVICE_REQUEST );
                    Status = STATUS_INVALID_DEVICE_REQUEST;
                    break;
                }

                //
                //  We cannot post any compressed reads, because that would interfere
                //  with our reserved buffer strategy.  We may currently own
                //  NtfsReservedBufferResource, and it is important for our read to
                //  be able to get a buffer.
                //

                ASSERT( (Scb->CompressionUnit == 0) ||
                        !ExIsResourceAcquiredExclusiveLite(&NtfsReservedBufferResource) );

                //
                //  To avoid deadlocks we only should post recursive paging file and mft requests
                //  o.w we might need to do lockups for example and reacquire the main in a diff. thread
                //  from where it was preacquired
                //

//                ASSERT( (Scb == Vcb->MftScb) || (FlagOn( Scb->Fcb->FcbState, FCB_STATE_PAGING_FILE )) );

                //
                //  Allocate an event and get shared on the scb.  We won't grab the
                //  Scb for the paging file path or for non-cached io for our
                //  system files.
                //

                KeInitializeEvent( &Event, NotificationEvent, FALSE );

                if ((FlagOn( Fcb->FcbState, FCB_STATE_PAGING_FILE )
                     && FlagOn( Scb->ScbState, SCB_STATE_UNNAMED_DATA )) ||
                    (NtfsLeqMftRef( &Fcb->FileReference, &VolumeFileReference ))) {

                    //
                    //  There is nothing to release in this case.
                    //

                    Resource = NULL;

                } else {

                    NtfsAcquireResourceShared( IrpContext, Scb, TRUE );
                    Resource = Scb->Header.Resource;

                }

                try {

                    //
                    //  Make the Irp just like a regular post request and
                    //  then send the Irp to the special overflow thread.
                    //  After the post we will wait for the stack overflow
                    //  read routine to set the event that indicates we can
                    //  now release the scb resource and return.
                    //

                    NtfsPrePostIrp( IrpContext, Irp );

                    if (FlagOn( Fcb->FcbState, FCB_STATE_PAGING_FILE ) &&
                        FlagOn( Scb->ScbState, SCB_STATE_UNNAMED_DATA )) {

                        FsRtlPostPagingFileStackOverflow( IrpContext, &Event, NtfsStackOverflowRead );

                    } else {

                        FsRtlPostStackOverflow( IrpContext, &Event, NtfsStackOverflowRead );
                    }

                    //
                    //  And wait for the worker thread to complete the item
                    //

                    (VOID) KeWaitForSingleObject( &Event, Executive, KernelMode, FALSE, NULL );

                    Status = STATUS_PENDING;

                } finally {

                    if (Resource != NULL) {

                        NtfsReleaseResource( IrpContext, Scb );
                    }
                }

            //
            //  Identify read requests which can't wait and post them to the
            //  Fsp.
            //

            } else {

                //
                //  Capture the auxiliary buffer and clear its address if it
                //  is not supposed to be deleted by the I/O system on I/O completion.
                //

                if (Irp->Tail.Overlay.AuxiliaryBuffer != NULL) {

                    IrpContext->Union.AuxiliaryBuffer =
                      (PFSRTL_AUXILIARY_BUFFER)Irp->Tail.Overlay.AuxiliaryBuffer;

                    if (!FlagOn(IrpContext->Union.AuxiliaryBuffer->Flags,
                                FSRTL_AUXILIARY_FLAG_DEALLOCATE)) {

                        Irp->Tail.Overlay.AuxiliaryBuffer = NULL;
                    }
                }

#if __NDAS_NTFS_SECONDARY__

				if (FlagOn(Irp->Flags, IRP_PAGING_IO)) {
						
					ASSERT( FlagOn(Irp->Flags, IRP_NOCACHE) );

					if( !NtfsIsTopLevelRequest(IrpContext) )
						DebugTrace( 0,  Dbg2, ("NtfsFsdRead: SavedTopLevelIrp = %p, NtfsIsTopLevelRequest() = %d, NtfsIsTopLevelNtfs() = %d, CanWait = %d\n", 
							NtfsGetTopLevelContext()->SavedTopLevelIrp, NtfsIsTopLevelRequest(IrpContext), NtfsIsTopLevelNtfs(IrpContext), FlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT)) );

					ASSERT( NtfsIsTopLevelRequest(IrpContext) );
					
					//if( !FlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) && VolumeDeviceObject->Secondary )
					//	ASSERT( NtfsGetTopLevelContext()->SavedTopLevelIrp == 0 );

					if (NtfsIsTopLevelRequest(IrpContext)) {
							
						//ASSERT( FlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );
						
					} else {
							
						ASSERT( FALSE );

						if (NtfsIsTopLevelNtfs(IrpContext))
						
							if (FlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT))

								ASSERT( NtfsGetTopLevelContext()->SavedTopLevelIrp == (PIRP)FSRTL_CACHE_TOP_LEVEL_IRP );

							else

								ASSERT( NtfsGetTopLevelContext()->SavedTopLevelIrp == (PIRP)FSRTL_MOD_WRITE_TOP_LEVEL_IRP );
						
						else {

							DebugTrace( 0, Dbg2, ("IrpContext->TopLevelIrpContext->MajorFunction = %d\n", 
												 IrpContext->TopLevelIrpContext->MajorFunction) );

							ASSERT( FlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );
							ASSERT( IrpContext->TopLevelIrpContext->MajorFunction == IRP_MJ_WRITE );
						}
					}
				}

				if (IS_SECONDARY_FILEOBJECT(IoGetCurrentIrpStackLocation(Irp)->FileObject))  {

					BOOLEAN secondaryResourceAcquired = FALSE;
					BOOLEAN secondaryRecoveryResourceAcquired = FALSE;
					
					SetFlag( IrpContext->NdasNtfsFlags, NDAS_NTFS_IRP_CONTEXT_FLAG_SECONDARY_CONTEXT );
					SetFlag( IrpContext->NdasNtfsFlags, NDAS_NTFS_IRP_CONTEXT_FLAG_SECONDARY_FILE );

#if 0
					if (FlagOn(Irp->Flags, IRP_PAGING_IO)) {
					
						secondaryResourceAcquired = 
							SecondaryAcquireResourceSharedStarveExclusiveLite( IrpContext, 
																			   &VolumeDeviceObject->Resource, 
																			   BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

						if (secondaryResourceAcquired == FALSE) {

							ASSERT( FlagOn(VolumeDeviceObject->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ||
									FlagOn(VolumeDeviceObject->Secondary->Flags, SECONDARY_FLAG_RECONNECTING) );
							NtfsCompleteRequest( IrpContext, Irp, Status = STATUS_FILE_LOCK_CONFLICT );							
							break;
						}

					} else {

						ASSERT( NtfsIsTopLevelRequest(IrpContext) );
						
						secondaryResourceAcquired = 
							SecondaryAcquireResourceSharedLite( IrpContext, 
															    &VolumeDeviceObject->Resource, 
															    BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

						if (secondaryResourceAcquired == FALSE) {

							ASSERT( FlagOn(VolumeDeviceObject->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ||
									FlagOn(VolumeDeviceObject->Secondary->Flags, SECONDARY_FLAG_RECONNECTING) );
							Status = NtfsPostRequest( IrpContext, Irp );
							break;
						}
					}

					try {

		                Status = NtfsCommonRead( IrpContext, Irp, TRUE );
							
					} finally {

						ASSERT( ExIsResourceAcquiredSharedLite(&VolumeDeviceObject->Resource) );
						SecondaryReleaseResourceLite( NULL, &VolumeDeviceObject->Resource );
					}
					
				} else
	                Status = NtfsCommonRead( IrpContext, Irp, TRUE );
#endif

					if (FlagOn(Irp->Flags, IRP_PAGING_IO)) {
					
						secondaryResourceAcquired = 
							SecondaryAcquireResourceSharedStarveExclusiveLite( IrpContext, 
																			   &VolumeDeviceObject->Resource, 
																			   BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

						if (secondaryResourceAcquired == FALSE) {

							ASSERT( FlagOn(VolumeDeviceObject->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ||
									FlagOn(VolumeDeviceObject->Secondary->Flags, SECONDARY_FLAG_RECONNECTING) );
							NtfsCompleteRequest( IrpContext, Irp, Status = STATUS_FILE_LOCK_CONFLICT );							
							break;
						}

						try {

							Status = NtfsCommonRead( IrpContext, Irp, TRUE );
							
						} finally {

							ASSERT( ExIsResourceAcquiredSharedLite(&VolumeDeviceObject->Resource) );
							SecondaryReleaseResourceLite( NULL, &VolumeDeviceObject->Resource );
						}

						break;
					}

					Status = STATUS_SUCCESS;

					while (TRUE) {
			
						ASSERT( secondaryRecoveryResourceAcquired == FALSE );
						ASSERT( secondaryResourceAcquired == FALSE );

						if (FlagOn(VolumeDeviceObject->Secondary->Flags, SECONDARY_FLAG_RECONNECTING)) {
		
							if (!FlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT)) {

								Status = NtfsPostRequest( IrpContext, Irp );
								break;
							}
						}
					
						if (FlagOn(VolumeDeviceObject->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED)) {
						
							if (!FlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT)) {

								Status = NtfsPostRequest( IrpContext, Irp );
								break;
							}
						
							secondaryRecoveryResourceAcquired 
								= SecondaryAcquireResourceExclusiveLite( IrpContext, 
																		 &VolumeDeviceObject->RecoveryResource, 
																		 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );
								
							if (!FlagOn(VolumeDeviceObject->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ) {

								SecondaryReleaseResourceLite( IrpContext, &VolumeDeviceObject->RecoveryResource );
								secondaryRecoveryResourceAcquired = FALSE;
								continue;
							}

							secondaryResourceAcquired 
								= SecondaryAcquireResourceExclusiveLite( IrpContext, 
																		 &VolumeDeviceObject->Resource, 
																		 BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );
							try {
								
								SecondaryRecoverySessionStart( VolumeDeviceObject->Secondary, IrpContext );
								
							} finally {

								SecondaryReleaseResourceLite( IrpContext, &VolumeDeviceObject->Resource );
								secondaryResourceAcquired = FALSE;

								SecondaryReleaseResourceLite( IrpContext, &VolumeDeviceObject->RecoveryResource );
								secondaryRecoveryResourceAcquired = FALSE;
							}

							continue;
						}

						secondaryResourceAcquired 
							= SecondaryAcquireResourceSharedLite( IrpContext, 
																  &VolumeDeviceObject->Resource, 
																  BooleanFlagOn(IrpContext->State, IRP_CONTEXT_STATE_WAIT) );

						if (secondaryResourceAcquired == FALSE) {

							ASSERT( FlagOn(VolumeDeviceObject->Secondary->Thread.Flags, SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED) ||
									FlagOn(VolumeDeviceObject->Secondary->Flags, SECONDARY_FLAG_RECONNECTING) );

							continue;
						}

						break;
					}

					if (Status == STATUS_SUCCESS) {
					
						try {

							Status = NtfsCommonRead( IrpContext, Irp, TRUE );
							
						} finally {

							ASSERT( ExIsResourceAcquiredSharedLite(&VolumeDeviceObject->Resource) );
							SecondaryReleaseResourceLite( NULL, &VolumeDeviceObject->Resource );
						}
					}

				} else
					Status = NtfsCommonRead( IrpContext, Irp, TRUE );

#else
                Status = NtfsCommonRead( IrpContext, Irp, TRUE );
#endif
            }

            break;

        } except(NtfsExceptionFilter( IrpContext, GetExceptionInformation() )) {

            NTSTATUS ExceptionCode;

            //
            //  We had some trouble trying to perform the requested
            //  operation, so we'll abort the I/O request with
            //  the error status that we get back from the
            //  execption code
            //

            ExceptionCode = GetExceptionCode();

            if (ExceptionCode == STATUS_FILE_DELETED) {
                IrpContext->ExceptionStatus = ExceptionCode = STATUS_END_OF_FILE;

                Irp->IoStatus.Information = 0;
            }

            Status = NtfsProcessException( IrpContext,
                                           Irp,
                                           ExceptionCode );
        }

    //
    //  Retry if this is a top level request, and the Irp was not completed due
    //  to a retryable error.
    //

    RetryCount += 1;

    } while ((Status == STATUS_CANT_WAIT || Status == STATUS_LOG_FILE_FULL) &&
             TopLevelContext.TopLevelRequest);

    ASSERT( IoGetTopLevelIrp() != (PIRP) &TopLevelContext );
    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsFsdRead -> %08lx\n", Status) );

    return Status;

    UNREFERENCED_PARAMETER( VolumeDeviceObject );
}


//
//  Internal support routine
//

VOID
NtfsStackOverflowRead (
    IN PVOID Context,
    IN PKEVENT Event
    )

/*++

Routine Description:

    This routine processes a read request that could not be processed by
    the fsp thread because of stack overflow potential.

Arguments:

    Context - Supplies the IrpContext being processed

    Event - Supplies the event to be signaled when we are done processing this
        request.

Return Value:

    None.

--*/

{
    TOP_LEVEL_CONTEXT TopLevelContext;
    PTOP_LEVEL_CONTEXT ThreadTopLevelContext;
    PIRP_CONTEXT IrpContext = Context;

    //
    //  Make it now look like we can wait for I/O to complete
    //

    SetFlag( IrpContext->State, IRP_CONTEXT_STATE_WAIT );
    ThreadTopLevelContext = NtfsInitializeTopLevelIrp( &TopLevelContext, TRUE, FALSE );

    //
    //  Do the read operation protected by a try-except clause
    //

    try {

        NtfsUpdateIrpContextWithTopLevel( IrpContext, ThreadTopLevelContext );

        //
        //  Set the flag to indicate that we are in the overflow thread.
        //

        ThreadTopLevelContext->OverflowReadThread = TRUE;

        (VOID) NtfsCommonRead( IrpContext, IrpContext->OriginatingIrp, FALSE );

    } except(NtfsExceptionFilter( IrpContext, GetExceptionInformation() )) {

        NTSTATUS ExceptionCode;

        //
        //  We had some trouble trying to perform the requested
        //  operation, so we'll abort the I/O request with
        //  the error status that we get back from the
        //  execption code
        //

        ExceptionCode = GetExceptionCode();

        if (ExceptionCode == STATUS_FILE_DELETED) {

            IrpContext->ExceptionStatus = ExceptionCode = STATUS_END_OF_FILE;
            IrpContext->OriginatingIrp->IoStatus.Information = 0;
        }

        (VOID) NtfsProcessException( IrpContext, IrpContext->OriginatingIrp, ExceptionCode );
    }

    ASSERT( IoGetTopLevelIrp() != (PIRP) &TopLevelContext );

    //
    //  Set the stack overflow item's event to tell the original
    //  thread that we're done and then go get another work item.
    //

    KeSetEvent( Event, 0, FALSE );
}


NTSTATUS
NtfsCommonRead (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN BOOLEAN AcquireScb
    )

/*++

Routine Description:

    This is the common routine for Read called by both the fsd and fsp
    threads.

Arguments:

    Irp - Supplies the Irp to process

    AcquireScb - Indicates if this routine should acquire the scb

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    PFILE_OBJECT FileObject;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PFCB Fcb;
    PSCB Scb;
    PCCB Ccb;

    PNTFS_ADVANCED_FCB_HEADER Header;

    PTOP_LEVEL_CONTEXT TopLevelContext;

    VBO StartingVbo;
    LONGLONG ByteCount;
    LONGLONG ByteRange;
    ULONG RequestedByteCount;

    BOOLEAN PostIrp = FALSE;
    BOOLEAN OplockPostIrp = FALSE;

    BOOLEAN ScbAcquired = FALSE;
    BOOLEAN ReleaseScb;
    BOOLEAN PagingIoAcquired = FALSE;
    BOOLEAN DoingIoAtEof = FALSE;

    BOOLEAN Wait;
    BOOLEAN PagingIo;
    BOOLEAN NonCachedIo;
    BOOLEAN SynchronousIo;
#ifdef  COMPRESS_ON_WIRE
    PCOMPRESSION_SYNC CompressionSync = NULL;
    BOOLEAN CompressedIo = FALSE;
#endif

    NTFS_IO_CONTEXT LocalContext;

    //
    // A system buffer is only used if we have to access the
    // buffer directly from the Fsp to clear a portion or to
    // do a synchronous I/O, or a cached transfer.  It is
    // possible that our caller may have already mapped a
    // system buffer, in which case we must remember this so
    // we do not unmap it on the way out.
    //

    PVOID SystemBuffer = NULL;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_IRP( Irp );

    //
    //  Get the current Irp stack location
    //

    IrpSp = IoGetCurrentIrpStackLocation( Irp );

    DebugTrace( +1, Dbg, ("NtfsCommonRead\n") );
    DebugTrace( 0, Dbg, ("IrpContext = %08lx\n", IrpContext) );
    DebugTrace( 0, Dbg, ("Irp        = %08lx\n", Irp) );
    DebugTrace( 0, Dbg, ("ByteCount  = %08lx\n", IrpSp->Parameters.Read.Length) );
    DebugTrace( 0, Dbg, ("ByteOffset = %016I64x\n", IrpSp->Parameters.Read.ByteOffset) );
    //
    //  Extract and decode the file object
    //

    FileObject = IrpSp->FileObject;
    TypeOfOpen = NtfsDecodeFileObject( IrpContext, FileObject, &Vcb, &Fcb, &Scb, &Ccb, TRUE );

    //
    //  Let's kill invalid read requests.
    //

    if ((TypeOfOpen != UserFileOpen) &&
        (TypeOfOpen != StreamFileOpen) &&
        (TypeOfOpen != UserVolumeOpen)) {

        DebugTrace( 0, Dbg, ("Invalid file object for read\n") );
        DebugTrace( -1, Dbg, ("NtfsCommonRead:  Exit -> %08lx\n", STATUS_INVALID_DEVICE_REQUEST) );

        NtfsCompleteRequest( IrpContext, Irp, STATUS_INVALID_DEVICE_REQUEST );
        return STATUS_INVALID_DEVICE_REQUEST;
    }

#if __NDAS_NTFS_DIRECT_READWRITE__

		if(IoGetCurrentIrpStackLocation(Irp)->FileObject == NULL) {

			DebugTrace( 0, DEBUG_TRACE_ALL, ("IrpSp->FileObject is NULL, IrpSp->MajorFunction = %x, IrpSp->MinorFunction = %x\n", IrpSp->MajorFunction, IrpSp->MinorFunction) );
		}

		if (IS_SECONDARY_FILEOBJECT(IoGetCurrentIrpStackLocation(Irp)->FileObject)) {

			if(TypeOfOpen == UserFileOpen) {

			} else {

				ASSERT( FALSE );

		        NtfsCompleteRequest( IrpContext, Irp, STATUS_INVALID_DEVICE_REQUEST );
				return STATUS_INVALID_DEVICE_REQUEST;
			}
		}

#endif

    //
    // Initialize the appropriate local variables.
    //

    Wait = (BOOLEAN) FlagOn( IrpContext->State, IRP_CONTEXT_STATE_WAIT );
    PagingIo = BooleanFlagOn( Irp->Flags, IRP_PAGING_IO );
    NonCachedIo = BooleanFlagOn( Irp->Flags,IRP_NOCACHE );
    SynchronousIo = BooleanFlagOn( FileObject->Flags, FO_SYNCHRONOUS_IO );

#ifdef COMPRESS_ON_WIRE
    if (FileObject->SectionObjectPointer == &Scb->NonpagedScb->SegmentObjectC) {

        CompressedIo = TRUE;
    }
#endif

    //
    //  Extract starting Vbo and offset.
    //

    StartingVbo = IrpSp->Parameters.Read.ByteOffset.QuadPart;
    ByteCount = (LONGLONG)IrpSp->Parameters.Read.Length;

    //
    // Check for overflow and underflow.
    //

    if (MAXLONGLONG - StartingVbo < ByteCount) {

        ASSERT( !PagingIo );

        NtfsCompleteRequest( IrpContext, Irp, STATUS_INVALID_PARAMETER );
        return STATUS_INVALID_PARAMETER;
    }

    ByteRange = StartingVbo + ByteCount;
    RequestedByteCount = (ULONG)ByteCount;

    //
    //  Check for a null request, and return immediately
    //

    if ((ULONG)ByteCount == 0) {

        DebugTrace( 0, Dbg, ("No bytes to read\n") );
        DebugTrace( -1, Dbg, ("NtfsCommonRead:  Exit -> %08lx\n", STATUS_SUCCESS) );

        NtfsCompleteRequest( IrpContext, Irp, STATUS_SUCCESS );
        return STATUS_SUCCESS;
    }

    //
    //  Convert all paging i/o against a usa_protected_sequence or compressed file
    //  to synchrnonous because we must do the transform after finishing the i/o
    //  If the header isn't initialized just do the op synchronous rather than
    //  trying to figure out if its compressed by resyncing with disk
    //

    if (!Wait &&
        PagingIo &&
        (FlagOn( Scb->ScbState, SCB_STATE_USA_PRESENT ) ||
         Scb->CompressionUnit != 0 ||
         Scb->EncryptionContext) ||
         !FlagOn( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED )) {

        Wait = TRUE;
        SetFlag( IrpContext->State, IRP_CONTEXT_STATE_WAIT );
    }


    //
    //  Make sure there is an initialized NtfsIoContext block.
    //

    if (TypeOfOpen == UserVolumeOpen
        || NonCachedIo) {

        //
        //  If there is a context pointer, we need to make sure it was
        //  allocated and not a stale stack pointer.
        //

        if (IrpContext->Union.NtfsIoContext == NULL
            || !FlagOn( IrpContext->State, IRP_CONTEXT_STATE_ALLOC_IO_CONTEXT )) {

            //
            //  If we can wait, use the context on the stack.  Otherwise
            //  we need to allocate one.
            //

            if (Wait) {

                IrpContext->Union.NtfsIoContext = &LocalContext;
                ClearFlag( IrpContext->State, IRP_CONTEXT_STATE_ALLOC_IO_CONTEXT );

            } else {

                IrpContext->Union.NtfsIoContext = (PNTFS_IO_CONTEXT)ExAllocateFromNPagedLookasideList( &NtfsIoContextLookasideList );
                SetFlag( IrpContext->State, IRP_CONTEXT_STATE_ALLOC_IO_CONTEXT );
            }
        }

        RtlZeroMemory( IrpContext->Union.NtfsIoContext, sizeof( NTFS_IO_CONTEXT ));

        //
        //  Store whether we allocated this context structure in the structure
        //  itself.
        //

        IrpContext->Union.NtfsIoContext->AllocatedContext =
            BooleanFlagOn( IrpContext->State, IRP_CONTEXT_STATE_ALLOC_IO_CONTEXT );

        if (Wait) {

            KeInitializeEvent( &IrpContext->Union.NtfsIoContext->Wait.SyncEvent,
                               NotificationEvent,
                               FALSE );

        } else {

            IrpContext->Union.NtfsIoContext->PagingIo = PagingIo;
            IrpContext->Union.NtfsIoContext->Wait.Async.ResourceThreadId =
                ExGetCurrentResourceThread();

            IrpContext->Union.NtfsIoContext->Wait.Async.RequestedByteCount =
                (ULONG)ByteCount;
        }
    }

    //
    //  Handle volume Dasd here.
    //

    if (TypeOfOpen == UserVolumeOpen) {

        NTSTATUS Status;

        //
        //  If the caller has not asked for extended DASD IO access then
        //  limit with the volume siz€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€```ЯЯЯ€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€   яяя€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€pppППП€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€ €€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€   €€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€њњњ@@@€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€```ЯЯЯ€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€   яяя€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€   яяя€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€ €€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€€ззз€иии€иии€иии€ззз€иии€иии€ййй€иии€иии€иии€иии€иии€ззз€ззз€ззз€ззз€жжж€ззз€ззз€ззз€иии€иии€ззз€ззз€иии€ззз€иии€ззз€ззз€ззз€ззз€ззз€ззз€ззз€ззз€жжж€ззз€жжж€жжж€жжж€жжж€жжж€жжж€жжж€жжж€жжж€жжж€жжж€жжж€жжж€еее€жжж€жжж€жжж€жжж€ззз€ззз€ззз€иии€иии€иии€иии€иии€иии€иии€иии€ййй€ккк€ййй€ккк€иии€ййй€иии€ййй€иии€ййй€иии€ййй€ййй€иии€иии€иии€ййй€иии€иии€иии€иии€ййй€иии€иии€ййй€ййй€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€ййй€иии€иии€ййй€иии€иии€иии€иии€иии€иии€иии€иии€иии€ййй€ййй€иии€ййй€иии€ййй€ййй€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ййй€ккк€ккк€ллл€ллл€ккк€ллл€ллл€ллл€ккк€ллл€ккк€ккк€ккк€ккк€ллл€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ййй€ййй€ккк€ййй€ккк€ййй€ййй€ййй€ййй€иии€ййй€ккк€ккк€ккк€ллл€ккк€ккк€ккк€ккк€ккк€ккк€ййй€ккк€ййй€ййй€ййй€ййй€ййй€ййй€ккк€ккк€ллл€ллл€ллл€ккк€ллл€ккк€ллл€ммм€ммм€ммм€ммм€ооо€ннн€ннн€ммм€ммм€ммм€ллл€ллл€ллл€ллл€ллл€ллл€ккк€ллл€ммм€ммм€ллл€ммм€ллл€ллл€ллл€ллл€ллл€ккк€ккк€ллл€ккк€ккк€ккк€ййй€ййй€ййй€иии€иии€иии€иии€иии€иии€ззз€ззз€жжж€жжж€жжж€жжж€жжж€жжж€еее€еее€еее€еее€еее€еее€иии€ккк€иии€иии€ккк€иии€иии€иии€иии€ййй€ллл€ккк€ййй€ййй€иии€иии€иии€иии€иии€иии€ййй€ййй€ййй€ййй€иии€ййй€ййй€иии€иии€иии€иии€иии€иии€ззз€иии€ззз€жжж€жжж€жжж€еее€еее€еее€еее€жжж€жжж€жжж€жжж€жжж€еее€жжж€жжж€жжж€жжж€жжж€еее€жжж€еее€еее€еее€еее€ддд€еее€еее€еее€еее€еее€еее€еее€жжж€жжж€жжж€ззз€ззз€жжж€ззз€ззз€ззз€иии€иии€ззз€ззз€ззз€ззз€ззз€ззз€ззз€ззз€ззз€иии€ззз€иии€ззз€ззз€ззз€ззз€ззз€ззз€ззз€жжж€ззз€жжж€жжж€ззз€жжж€жжж€жжж€жжж€жжж€жжж€жжж€жжж€жжж€жжж€ззз€жжж€ззз€ззз€иии€ззз€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€ййй€ккк€ккк€ккк€иии€ккк€ййй€иии€ккк€ккк€ккк€ккк€ййй€ккк€ллл€ййй€иии€ййй€ййй€иии€ййй€ккк€ккк€иии€ййй€ййй€ййй€иии€иии€иии€иии€иии€ййй€ййй€ййй€иии€иии€иии€иии€ййй€иии€иии€иии€иии€иии€ккк€ккк€ййй€ййй€ййй€ййй€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ккк€иии€ййй€иии€иии€ззз€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€ззз€иии€ззз€ззз€иии€иии€иии€иии€ззз€ззз€жжж€ззз€ззз€ззз€ззз€ззз€ззз€ййй€ййй€ййй€ккк€ккк€ййй€ккк€ккк€ййй€ккк€ллл€ккк€ллл€ккк€ллл€ллл€ллл€ммм€ллл€ккк€ллл€ллл€ллл€ллл€ллл€ккк€ккк€ккк€ллл€ккк€ллл€ккк€ккк€ййй€ййй€ккк€ккк€ййй€иии€иии€иии€иии€ййй€ййй€ллл€ммм€ммм€ллл€ллл€ллл€ммм€ммм€ллл€ллл€ллл€ммм€ллл€ммм€ннн€ммм€ммм€ммм€ммм€ллл€ммм€ооо€ммм€ммм€ммм€ммм€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ккк€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ккк€ккк€ккк€ккк€ккк€ккк€ллл€ллл€ййй€ккк€ккк€ккк€ййй€иии€ккк€ййй€ккк€ййй€ййй€иии€иии€иии€иии€иии€иии€иии€ззз€ззз€ззз€жжж€жжж€жжж€жжж€жжж€жжж€ззз€жжж€жжж€жжж€жжж€ззз€ззз€жжж€ззз€жжж€ззз€ззз€жжж€жжж€жжж€жжж€иии€ззз€иии€иии€ззз€ззз€иии€иии€иии€иии€иии€иии€иии€ззз€иии€иии€ззз€ззз€ззз€ззз€жжж€жжж€ззз€ззз€ззз€ззз€иии€жжж€ззз€ззз€ззз€ззз€ззз€ззз€ззз€жжж€ззз€ззз€ззз€ззз€ззз€иии€ззз€иии€иии€иии€иии€иии€иии€ззз€иии€иии€иии€иии€иии€иии€ййй€ййй€иии€иии€иии€иии€иии€иии€ййй€ййй€ййй€ййй€ккк€иии€ййй€иии€ййй€иии€ййй€иии€иии€ййй€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ййй€ййй€ййй€иии€иии€иии€иии€ййй€иии€ййй€ййй€иии€ййй€ккк€ййй€ййй€ййй€ккк€ккк€ккк€ккк€ййй€ккк€ккк€ккк€ллл€ккк€ллл€ккк€ккк€ккк€ллл€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ллл€ккк€ллл€ккк€ммм€ллл€ллл€ккк€ллл€ллл€ммм€ммм€ккк€ккк€ллл€ллл€ййй€ккк€ййй€ййй€иии€иии€иии€иии€иии€ййй€ккк€иии€иии€иии€ззз€иии€ййй€иии€иии€иии€ккк€ййй€ккк€ккк€ккк€ллл€ллл€ллл€ллл€ллл€ллл€ккк€ллл€ккк€ккк€ккк€ккк€ккк€ккк€ллл€ллл€ллл€ллл€ллл€ммм€ммм€ннн€ннн€ннн€ннн€ннн€ммм€ннн€ммм€ллл€ллл€ллл€ллл€ллл€ккк€ккк€ллл€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ййй€иии€иии€иии€иии€ззз€ззз€ззз€иии€иии€ззз€ззз€ззз€ззз€ззз€иии€иии€ззз€ззз€иии€иии€иии€иии€ййй€иии€ййй€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ллл€ллл€ккк€ккк€ккк€ккк€ккк€ккк€иии€ккк€ккк€ккк€ййй€ййй€ййй€ййй€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€ззз€ззз€ззз€иии€ззз€иии€иии€иии€иии€ззз€ззз€ззз€иии€ззз€иии€иии€иии€иии€иии€иии€иии€иии€иии€ззз€иии€иии€ззз€иии€иии€иии€ззз€ззз€ззз€ззз€жжж€жжж€жжж€жжж€жжж€ззз€жжж€жжж€ззз€жжж€ззз€ззз€ззз€иии€ййй€иии€иии€иии€иии€иии€иии€иии€иии€иии€ззз€ззз€ззз€ззз€иии€ззз€ззз€ззз€ззз€ззз€ззз€ззз€ззз€ззз€иии€иии€ззз€ззз€иии€иии€иии€иии€иии€ззз€ззз€ззз€ззз€ззз€иии€ззз€ззз€жжж€жжж€ззз€иии€иии€иии€иии€иии€иии€ззз€иии€ззз€иии€иии€ззз€иии€иии€иии€ййй€ййй€ййй€иии€иии€ззз€ззз€иии€ййй€ккк€ккк€ккк€ккк€ккк€ллл€ллл€ллл€ллл€ллл€ммм€ммм€еее€еее€жжж€жжж€ддд€ддд€еее€еее€жжж€еее€еее€жжж€еее€еее€еее€еее€жжж€жжж€жжж€жжж€ззз€ззз€ззз€иии€ззз€ззз€ззз€иии€иии€иии€иии€иии€ззз€иии€ззз€иии€иии€иии€иии€иии€иии€ззз€иии€иии€иии€иии€иии€иии€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ййй€иии€иии€иии€ййй€ййй€иии€иии€иии€иии€ззз€ззз€ззз€ззз€ззз€жжж€жжж€жжж€еее€еее€еее€еее€еее€еее€еее€ддд€еее€еее€еее€еее€еее€жжж€жжж€жжж€жжж€жжж€жжж€ззз€жжж€жжж€жжж€ззз€ззз€ззз€жжж€ззз€жжж€жжж€жжж€жжж€жжж€жжж€жжж€жжж€жжж€жжж€жжж€ззз€жжж€жжж€жжж€жжж€жжж€жжж€жжж€ззз€ззз€ззз€жжж€жжж€жжж€жжж€ззз€ззз€ззз€ззз€ззз€ззз€жжж€жжж€ззз€иии€ззз€иии€ззз€иии€иии€иии€иии€ззз€иии€ззз€ззз€ззз€ззз€ззз€ззз€ззз€ззз€ззз€жжж€жжж€жжж€ззз€ззз€жжж€ззз€ззз€иии€ззз€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€ззз€ззз€ззз€ззз€ззз€ззз€ззз€иии€иии€ййй€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€ййй€ййй€иии€иии€иии€иии€ззз€иии€ззз€ззз€ззз€ззз€иии€иии€ззз€ззз€иии€иии€иии€ййй€ййй€ййй€ккк€ййй€иии€ййй€ййй€ййй€иии€иии€иии€иии€иии€иии€иии€ййй€ккк€ййй€иии€иии€ййй€ййй€ййй€иии€иии€иии€иии€иии€иии€иии€иии€иии€ззз€ззз€ззз€ззз€жжж€ззз€ззз€ззз€ззз€ззз€ззз€жжж€жжж€жжж€ззз€ззз€ззз€жжж€жжж€жжж€ззз€иии€иии€ззз€иии€иии€иии€иии€иии€иии€ззз€ззз€ззз€жжж€ззз€ззз€ззз€ззз€ззз€иии€ззз€иии€иии€иии€иии€ййй€иии€иии€ййй€ккк€ккк€ккк€ккк€ллл€ккк€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ммм€ллл€ллл€ллл€ккк€ллл€ллл€ллл€ккк€ллл€ккк€ллл€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ййй€ккк€ййй€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€ззз€ззз€ззз€ззз€ззз€ззз€ззз€ззз€иии€ззз€ззз€ззз€ззз€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€ййй€ййй€ккк€ййй€ййй€ккк€ййй€иии€ккк€ккк€ккк€ккк€ллл€ккк€ккк€ййй€ййй€ййй€иии€иии€иии€иии€ззз€жжж€жжж€еее€еее€еее€жжж€жжж€жжж€жжж€жжж€еее€жжж€жжж€ззз€жжж€жжж€ззз€ззз€ззз€жжж€ззз€ззз€ззз€ззз€ззз€ззз€иии€иии€иии€иии€иии€иии€иии€иии€ййй€иии€ййй€ййй€ййй€ййй€иии€иии€иии€иии€иии€иии€ййй€ййй€ккк€ййй€ййй€ййй€ййй€ййй€ййй€иии€иии€иии€ййй€ййй€ййй€ккк€ййй€ййй€ккк€ккк€ккк€ййй€ккк€ккк€ллл€ккк€ккк€ккк€ййй€ййй€иии€иии€иии€иии€ййй€ййй€ййй€ккк€ккк€ккк€ккк€ллл€ккк€ллл€ккк€ллл€ллл€ллл€ллл€ккк€ккк€ййй€иии€иии€иии€ййй€иии€жжж€ззз€ззз€жжж€жжж€еее€жжж€иии€иии€ййй€ййй€иии€ййй€ккк€ййй€ккк€ккк€ллл€ллл€ллл€ккк€ммм€ммм€ннн€ннн€ллл€ллл€ммм€ллл€ллл€ккк€ккк€ллл€ккк€ккк€ккк€ллл€ллл€ллл€ккк€ккк€ллл€ллл€ллл€ккк€ккк€ккк€ккк€ккк€ллл€ккк€ййй€ккк€ккк€ккк€иии€ййй€ййй€ййй€иии€иии€ййй€иии€иии€иии€ййй€иии€ззз€ззз€ззз€ззз€ззз€иии€ззз€ззз€ззз€ззз€ззз€ззз€ззз€ззз€ззз€ззз€иии€ззз€ззз€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€ззз€иии€иии€ззз€ззз€ззз€ззз€жжж€ззз€жжж€жжж€ззз€ззз€ззз€жжж€ззз€ззз€ззз€ззз€ззз€ззз€ззз€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€ззз€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€ккк€ййй€ккк€ккк€ккк€ккк€ккк€ккк€ккк€ййй€иии€иии€иии€иии€иии€ййй€ййй€ййй€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€ззз€ззз€ззз€ззз€ззз€ззз€ззз€жжж€ззз€ззз€ззз€жжж€жжж€жжж€ззз€жжж€ззз€ззз€иии€ззз€ззз€ззз€ззз€иии€ззз€ззз€ззз€ззз€ззз€жжж€жжж€жжж€жжж€жжж€жжж€жжж€жжж€жжж€ззз€ззз€иии€иии€иии€иии€ййй€ййй€ййй€ккк€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ллл€ммм€ммм€ммм€ммм€ммм€ллл€ллл€ллл€ллл€ммм€ммм€ммм€ммм€ммм€ллл€ммм€ллл€ллл€ллл€ммм€ллл€ллл€ммм€ллл€ммм€ллл€ллл€ккк€ллл€ккк€ййй€иии€иии€иии€ййй€иии€иии€иии€иии€ййй€ййй€ййй€иии€ййй€ккк€ккк€ййй€ййй€ййй€иии€иии€ййй€ййй€иии€ййй€иии€иии€иии€иии€иии€иии€иии€иии€ййй€иии€иии€ззз€иии€иии€иии€иии€иии€иии€иии€иии€ййй€иии€ййй€ййй€ззз€иии€иии€ййй€ййй€иии€иии€иии€иии€иии€иии€иии€иии€иии€ййй€ккк€ккк€ккк€ккк€иии€ккк€ййй€ййй€иии€ккк€ккк€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€ззз€ззз€ззз€ззз€ззз€ззз€ззз€ззз€ззз€ззз€жжж€жжж€жжж€ззз€ззз€жжж€жжж€ззз€ззз€ззз€иии€ззз€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€иии€ззз€ззз€иии€иии€иии€иии€ййй€ййй€ййй€иии€ййй€иии€иии€ййй€ййй€ккк€ййй€иии€ккк€ййй€ккк€ккк€ккк€ккк€ккк€ккк€ййй€ййй€иии€иии€иии€иии€иии€ззз€ззз€ззз€ззз€ззз€ззз€ззз€ззз€ззз€ззз€ззз€ззз€иии€иии€иии€иии€иии€иии€иии€иии€иии€ззз€иии€иии€иии€иии€иии€ййй€иии€иии€иии€иии€ззз€ззз€ззз€ззз€ззз€ззз€ззз€ззз€иии€ззз€ззз€иии€иии€иии€иии€иии€        A            рO                     electrophorus          !       D G S                   A            P O    АWS             electroplate           !       M S                     A            ∞ O                     electroscope           A                     аp             electroscopic          !       D G M S                 A            P!O    АBQ             electroshock           !       S                       A            ∞!O                     electrostatic          !       M                       A            "O    Рmy             electrostatics         !       M                       A            p"O     ?`             electrotherapist       !       D G M S Z               A            –"O                     electrotype            A                                      electroweak            A                                      eleemosynary           !       I M S                   A            ∞#O                     elegance               !       I Y                     A            $O    ∞ \             elegant                !       S                       A            p$O     .X             elegiac                A       		              `'\             elegiacal              !       M S                     A            %O                     elegy                  A                                      elem                   !       M S                     A            ∞%O                     element                !       S Y                     A       		     &O                     elemental              A                      4p             elementarily           !       M                       A            ∞&O                     elementariness         !       P                       A       

     'O                     elementary             !       M S                     A            p'O    0e             elephant               A                     †АX             elephantiases          !       M                       A            (O    pаm             elephantiasis          A                     Pоm             elephantine            !       N X                     A            ∞(O                     elev                   !       D G N S X               A            )O                     elevate                !       S                       A            p)O    А•\             elevated               !       M                       A       		     –)O                     elevation              !       M S                     A            0*O    Р_             elevator               !       H M                     A            Р*O                     eleven                 !       S                       A            р*O    Рa             elevens                A       		              0ўp             elevenths              !       M                       A            Р+O                     elf                    !       S                       A            р+O                     elfin                  A                     а
T             elfish                 !       D G S                   A            Р,O                     elicit                 !       M S                     A            р,O    јЋZ             elicitation            !       D G S                   A            P-O                     elide                  !       I M S                   A            ∞-O    0W             eligibility            !       I S                     A            .O    ∞Іy             eligible               !       D G N S V X Y           A       		     p.O    ∞\             eliminate              !       M                       A            –.O    р
Y             elimination            !       M S                     A       

     0/O    @c             eliminator             !       M S                     A            Р/O    –5W             elision                !       M P S           нБи}%  wЕпPwЕпPwЕпP    и          
у               @                                     яі@                           Аpe|4b|4bwЕпP|4b                                                                                                        нБиь   wЕпPwЕпPwЕпP    и          
у                                                    аі@                           АpeАpeАpewЕпPАpe                                                                                                        нБиH  wЕпPwЕпPwЕпP    и          
у                                                    бі@                           АpeАpeАpewЕпPАpe                                                                                                        нБи6  wЕпPwЕпPwЕпP    и          
у               C                                     ві@                           АpeАpeАpewЕпPАpe                                                                                                        нБи\  wЕпPwЕпPwЕпP    и 0         
у               D                                     гі@                           АpeАpeАpewЕпPАpe                                                                                                        нБи÷W  wЕпPwЕпPwЕпP    и 0         
у               J                                     ді@                           А$’iА$’iАpewЕпPАpe                                                                                                        нБи÷W  wЕпPwЕпPwЕпP    и 0         
у               P                                     еі@                           АH…jАH…jА$’iwЕпPА$’i                                                                                                        нБиђ  wЕпPwЕпPwЕпP    и          
у               6                                     жі@                           ДlљkДlљkДlљkwЕпPДlљk                                                                                                        нБиЈ  wЕпPwЕпPwЕпP    и          
у               7                                     зі@                           ДlљkДlљkДlљkwЕпPДlљk                                                                                                        нБиь   wЕпPwЕпPwЕпP    и          
у               V                                     иі@                           ДlљkДlљkДlљkwЕпPДlљk                                                                                                        нБи±  wЕпPwЕпPwЕпP    и          
у               W                                     йі@                           ДlљkДlљkДlљkwЕпPДlљk                                                                                                        нБиБ  wЕпPwЕпPwЕпP    и          
у               X                                     кі@                           ДlљkДlљkДlљkwЕпPДlљk                                                                                                        нБиc  wЕпPwЕпPwЕпP    и          
у               Z                                     лі@                           ДlљkДlљkДlљkwЕпPДlљk                                                                                                        нБи   wЕпPwЕпPwЕпP    и          
у               [                                     мі@                           ДlљkДlљkДlљkwЕпPДlљk                                                                                                        нБиЅ  wЕпPwЕпPwЕпP    и          
у               \                                     ні@                           ДlљkДlљkДlљkwЕпPДlљk                                                                                                        нБиЕ   wЕпPwЕпPwЕпP    и          
у               ]                                     оі@                           ДЎЩnДlљkДlљkwЕпPДlљk                                                                                                            @   < d       "     " " е rЃ   	 
                   	 
                   	 
                           	 
   	 
 	 	 	 
 
 
 	 
   	 
   	 
 
 
         	 
   	 
   	 
                                                         
               	 
         
     
                     
 
   	 	   
 
           	 	       	 	   
 
   	 	                     	 
                         	       	 
             
           	 
   	 
                                           	 
   	 
                   	 
                   	 
                   	 
                     
                   	 
     	 
                                             	 	   
 
                                                                                             	 	   
 
                                                                         п   7   Ї	   Sheet2
   	   ёЌ…ј    4           g !' 2 ќ; ∆F @Q X[ иd 3o ÷s      d          ь©с“MbP?_   *    +    В   А          %   Б  Ѕ      Г    Д    M ≤  H P   L a s e r J e t   4 2 0 0 L   P C L   6                    № ‘C€А  Ъ4d   X  X  A 4                                                                                          €€€€                DINU"   4†)~ёе                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              †  IUPH                                       d             A 4                                                                                                 [ n o n e ]                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     [ n o n e ]                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   A r i a l                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       4   P              јјј     јјј                      d         А?                                                              S J C H O                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         < A u t o m a t i c >                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  @                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              €€€€° "  d     XX      а?      а? U   }       y   }    8l   }     l   }    qЖ   }    гR                       *    @      ,           w    @      	           з	           ≥           W           ,           Е       	    W       
    ≥           )           Е           ≥                                 ,           ,           Х    @      Х    @      v    @      ю    @      ,           ≥           W           Е           Е           ,           ≥           ≥           ≥           ≥      э 
     ƒ Ц  Њ     ƒ ƒ ≈  э 
    « Ч  Њ 
   « «      t э 
   T R  э 
   U    э 
   T ъ  ~
    t   р?э 
   V     э 
   W 9  э 
   З        t        @    €	 D  ј э 
   V T  э 
   W :     k      t       @(   €    Љ      		 L€€ ј э 
   V 
   э 
   W {     З      t       @(   €    э 
   V |  э 
   W А     k      t       @(   €    э 
   X }  э 
   Y Б     k      t       @( 	  €    э 
   Y В  э 
   Z Н  э 
   k x    	   t       @( 
  €    э 
 	  Y Г  э 
 	  Z     	  k   
   t        @(   €    э 
 
  Y Д  э 
 
  Z {   
  k      t       "@(   €    э 
   Y    э 
   Z       k      t       $@(   €    э 
   Y И  э 
   Z К     k      t       &@(   €    э 
   Y Й  э 
   Z    э 
   k <  Њ     » » »  э 
    … Ђ  Њ 
   … …  Њ     » » »      t э 
   T R  э Ряб    OYкf™  г       8       ∞]    OYкf™  °?йf™                      А?   ?`BҐ        Ряб    iYкf™  г       8       ∞]    iYкf™  ≠?йf™                      А?   ?`BҐ        pЏ    љ?йf™  г              ∞]    љ?йf™  є?йf™                    €€€€                pЏ    ∆?йf™  г              ∞]    ∆?йf™  ¬?йf™                    €€€€                РЏ    ’?йf™  г              ∞]    ’?йf™  Ћ?йf™                €€€€€€€€€€€            РЏ    е?йf™  г              ∞]    е?йf™  џ?йf™             	   €€€€€€€€€€€            pЏ    ЫLйf™  г              ∞^    @лиf™  HMйf™                    €€€€                Џ    z5фf™  г        Oш    ]     Uйf™   Uйf™                –Oш                     Џ    0Uйf™  г       аИ    ]    +Uйf™  +Uйf™                АЛ                    РЏ    =Uйf™  г              ]    8Uйf™  8Uйf™                    €€€Р                                      
       –П    –ы            Q       јH£e™  8      –B£e™                 АОѕf™  –Ъ                    с       ∞]            †∞£e™  c≈f™  аe≈f™  †в—f™  в—f™  ©£e™          `Ю£e™                                                                          в—f™                                                                          !              ј            !                      Р$ю    q         ™  ∞]    √:йf™                        `             z£e™  ∞z£e™           её    аеё    q                                    А    n      @    8Ъ    –    р                           !        Ч∆f™                  Q       р÷    (                       РK≈f™                  А#            1       P       0б    –5б    08б    xЗ©d™  Q       јH£e™  »       –B£e™                 PПѕf™  ∞]                    !              p            !                              !       р    @               q         ™  ∞]    e?йf™        Р                        PПѕf™                  –Ав    0Св    !                           q                           
         0\    q      P                    @    0                       q                           
         0Л    П       	                    ∞    0                       !              @	            !          ™          0,в    1       ∞           	       †јлe™          q                           
         ∞Л    Х       
                    Р	    0                       !               
            !          ™                        ∞^            †∞£e™  АЫ≈f™  †Я≈f™  †в—f™  `°≈f™  ©£e™          `Ю£e™                                                                          в—f™  –Т≈f™   Ч≈f™                                                                                          !          ™          pЮ    a       `-÷                         ај≈f™                  PMш    x `     pт≈f™          q                                    @    Й      А\                    –    H                       Б       X    †d»e™  @d»e™  а.£e™   1£e™   1£e™  р.£e™  Ш    P1£e™  Ъ    †1£e™  p p                             A       R€      pж                     °    АЕ            A       R€     pж            ј            јЕ            A       R€     pж                          Ж            A       R€     pж            @    –§ы    @Ж            A       Ч€      pж            А    †°    АЖ            A       Ч€     pж            ј            јЖ            A       Ч€     pж                          З            A       Ч€     pж            @   