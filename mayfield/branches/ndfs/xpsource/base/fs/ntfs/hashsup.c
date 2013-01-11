x.ChangeCount) {

            //
            //  Use the top location in the index context to perform the
            //  read.
            //

            Sp = IndexContext.Base;

            ReadIndexBuffer( IrpContext,
                             Scb,
                             QuickIndex->IndexBlock,
                             FALSE,
                             Sp );

            //
            //  If the Lsn matches then we can use this buffer directly.
            //

            if (QuickIndex->CapturedLsn.QuadPart == Sp->CapturedLsn.QuadPart) {

                IndexBuffer = (PINDEX_ALLOCATION_BUFFER) Sp->StartOfBuffer;
                IndexEntry = (PINDEX_ENTRY) Add2Ptr( Sp->StartOfBuffer,
                                                     QuickIndex->BufferOffset );

                FileNameInIndex = (PFILE_NAME)(IndexEntry + 1);

                //
                //  Pin the index buffer
                //

                NtfsPinMappedData( IrpContext,
                                   Scb,
                                   LlBytesFromIndexBlocks( IndexBuffer->ThisBlock, Scb->ScbType.Index.IndexBlockByteShift ),
                                   Scb->ScbType.Index.BytesPerIndexBuffer,
                                   &Sp->Bcb );

                //
                //  Write a log record to change our ParentIndexEntry.
                //

                //
                //  Write the log record, but do not update the IndexBuffer Lsn,
                //  since nothing moved and we don't want to force index contexts
                //  to have to rescan.
                //
                //  Indexbuffer->Lsn =
                //

                NtfsWriteLog( IrpContext,
                              Scb,
                              Sp->Bcb,
                              UpdateFileNameAllocation,
                              Info,
                              sizeof(DUPLICATED_INFORMATION),
                              UpdateFileNameAllocation,
                              &FileNameInIndex->Info,
                              sizeof(DUPLICATED_INFORMATION),
                              LlBytesFromIndexBlocks( IndexBuffer->ThisBlock, Scb->ScbType.Index.IndexBlockByteShift ),
                              0,
                              (ULONG)((PCHAR)IndexEntry - (PCHAR)IndexBuffer),
                              Scb->ScbType.Index.BytesPerIndexBuffer );

                //
                //  Now call the Restart routine to do it.
                //

                NtfsRestartUpdateFileName( IndexEntry,
                                           Info );

                try_return( NOTHING );

            //
            //  Otherwise we need to reinitialize the index context and take
            //  the long path below.
            //

            } else {

                NtfsReinitializeIndexContext( IrpContext, &IndexContext );
            }
        }

        //
        //  Position to first possible match.
        //

        FindFirstIndexEntry( IrpContext,
                             Scb,
                             (PVOID)FileName,
                             &IndexContext );

        //
        //  See if there is an actual match.
        //

        if (FindNextIndexEntry( IrpContext,
                                Scb,
                                (PVOID)FileName,
                                FALSE,
                                FALSE,
                                &IndexContext,
                                FALSE,
                                NULL )) {

            //
            //  Point to the index entry and the file name within it.
            //

            IndexEntry = IndexContext.Current->IndexEntry;
            FileNameInIndex = (PFILE_NAME)(IndexEntry + 1);

            //
            //  Now pin the entry.
            //

            if (IndexContext.Current == IndexContext.Base) {

                PFILE_RECORD_SEGMENT_HEADER FileRecord;
                PATTRIBUTE_RECORD_HEADER Attribute;
                PATTRIBUTE_ENUMERATION_CONTEXT Context = &IndexContext.AttributeContext;

                //
                //  Pin the root
                //

                NtfsPinMappedAttribute( IrpContext,
                                        Vcb,
                                        Context );

                //
                //  Write a log record to change our ParentIndexEntry.
                //

                FileRecord = NtfsContainingFileRecord(Context);
                Attribute = NtfsFoundAttribute(Context);

                //
                //  Write the log record, but do not update the FileRecord Lsn,
                //  since nothing moved and we don't want to force index contexts
                //  to have to rescan.
                //
                //  FileRecord->Lsn =
                //

                NtfsWriteLog( IrpContext,
                              Vcb->MftScb,
                              NtfsFoundBcb(Context),
                              UpdateFileNameRoot,
                              Info,
                              sizeof(DUPLICATED_INFORMATION),
                              UpdateFileNameRoot,
                              &FileNameInIndex->Info,
                              sizeof(DUPLICATED_INFORMATION),
                              NtfsMftOffset( Context ),
                              (ULONG)((PCHAR)Attribute - (PCHAR)FileRecord),
                              (ULONG)((PCHAR)IndexEntry - (PCHAR)Attribute),
                              Vcb->BytesPerFileRecordSegment );

                if (ARGUMENT_PRESENT( QuickIndex )) {

                    QuickIndex->BufferOffset = 0;
                }

            } else {

                PINDEX_ALLOCATION_BUFFER IndexBuffer;

                Sp = IndexContext.Current;
                IndexBuffer = (PINDEX_ALLOCATION_BUFFER)Sp->StartOfBuffer;

                //
                //  Pin the index buffer
                //

                NtfsPinMappedData( IrpContext,
                                   Scb,
                                   LlBytesFromIndexBlocks( IndexBuffer->ThisBlock, Scb->ScbType.Index.IndexBlockByteShift ),
                                   Scb->ScbType.Index.BytesPerIndexBuffer,
                                   &Sp->Bcb );

                //
                //  Write a log record to change our ParentIndexEntry.
                //

                //
                //  Write the log record, but do not update the IndexBuffer Lsn,
                //  since nothing moved and we don't want to force index contexts
                //  to have to rescan.
                //
                //  Indexbuffer->Lsn =
                //

                NtfsWriteLog( IrpContext,
                              Scb,
                              Sp->Bcb,
                              UpdateFileNameAllocation,
                              Info,
                              sizeof(DUPLICATED_INFORMATION),
                              UpdateFileNameAllocation,
                              &FileNameInIndex->Info,
                              sizeof(DUPLICATED_INFORMATION),
                              LlBytesFromIndexBlocks( IndexBuffer->ThisBlock, Scb->ScbType.Index.IndexBlockByteShift ),
                              0,
                              (ULONG)((PCHAR)Sp->IndexEntry - (PCHAR)IndexBuffer),
                              Scb->ScbType.Index.BytesPerIndexBuffer );

                if (ARGUMENT_PRESENT( QuickIndex )) {

                    QuickIndex->ChangeCount = Scb->ScbType.Index.ChangeCount;
                    QuickIndex->BufferOffset = PtrOffset( Sp->StartOfBuffer, Sp->IndexEntry );
                    QuickIndex->CapturedLsn = ((PINDEX_ALLOCATION_BUFFER) Sp->StartOfBuffer)->Lsn;
                    QuickIndex->IndexBlock = ((PINDEX_ALLOCATION_BUFFER) Sp->StartOfBuffer)->ThisBlock;
                }
            }

            //
            //  Now call the Restart routine to do it.
            //

            NtfsRestartUpdateFileName( IndexEntry,
                                       Info );

        //
        //  If the file name is not in the index, this is a bad file.
        //

        } else {

            NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Scb->Fcb );
        }

    try_exit:  NOTHING;
    } finally{

        DebugUnwind( NtfsUpdateFileNameInIndex );

        NtfsCleanupIndexContext( IrpContext, &IndexContext );
    }

    DebugTrace( -1, Dbg, ("NtfsUpdateFileNameInIndex -> VOID\n") );

    return;
}


VOID
NtfsAddIndexEntry (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN PVOID Value,
    IN ULONG ValueLength,
    IN PFILE_REFERENCE FileReference,
    IN PINDEX_CONTEXT IndexContext OPTIONAL,
    OUT PQUICK_INDEX QuickIndex OPTIONAL
    )

/*++

Routine Description:

    This routine may be called to add an entry to an index.  This routine
    always allows duplicates.  If duplicates are not allowed, it is the
    caller's responsibility to detect and eliminate any duplicate before
    calling this routine.

Arguments:

    Scb - Supplies the Scb for the index.

    Value - Supplies a pointer to the value to add to the index

    ValueLength - Supplies the length of the value in bytes.

    FileReference - Supplies the file reference to place with the index entry.

    QuickIndex - If specified we store the location of the index added.

    IndexContext - If specified, previous result of doing the lookup for the name in the index.

Return Value:

    None

--*/

{
    INDEX_CONTEXT IndexContextStruct;
    PINDEX_CONTEXT LocalIndexContext;
    struct {
        INDEX_ENTRY IndexEntry;
        PVOID Value;
        PVOID MustBeNull;
    } IE;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_SCB( Scb );
    ASSERT_EXCLUSIVE_SCB( Scb );
    ASSERT( (Scb->ScbType.Index.CollationRule != COLLATION_FILE_NAME) ||
            ( *(PLONGLONG)&((PFILE_NAME)Value)->ParentDirectory ==
              *(PLONGLONG)&Scb->Fcb->FileReference ) );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsAddIndexEntry\n") );
    DebugTrace( 0, Dbg, ("Scb = %08lx\n", Scb) );
    DebugTrace( 0, Dbg, ("Value = %08lx\n", Value) );
    DebugTrace( 0, Dbg, ("ValueLength = %08lx\n", ValueLength) );
    DebugTrace( 0, Dbg, ("FileReference = %08lx\n", FileReference) );

    //
    //  Remember if we are using the local or input IndexContext.
    //

    if (ARGUMENT_PRESENT( IndexContext )) {

        LocalIndexContext = IndexContext;

    } else {

        LocalIndexContext = &IndexContextStruct;
        NtfsInitializeIndexContext( LocalIndexContext );
    }

    try {

        //
        //  Do the lookup again if we don't have a context.
        //

        if (!ARGUMENT_PRESENT( IndexContext )) {

            //
            //  Position to first possible match.
            //

            FindFirstIndexEntry( IrpContext,
                                 Scb,
                                 Value,
                                 LocalIndexContext );

            //
            //  See if there is an actual match.
            //

            if (FindNextIndexEntry( IrpContext,
                                    Scb,
                                    Value,
                                    FALSE,
                                    FALSE,
                                    LocalIndexContext,
                                    FALSE,
                                    NULL )) {

                ASSERTMSG( "NtfsAddIndexEntry already exists", FALSE );

                NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Scb->Fcb );
            }
        }

        //
        //  Initialize the Index Entry in pointer form.
        //

        IE.IndexEntry.FileReference = *FileReference;
        IE.IndexEntry.Length = (USHORT)(sizeof(INDEX_ENTRY) + QuadAlign(ValueLength));
        IE.IndexEntry.AttributeLength = (USHORT)ValueLength;
        IE.IndexEntry.Flags = INDEX_ENTRY_POINTER_FORM;
        IE.IndexEntry.Reserved = 0;
        IE.Value = Value;
        IE.MustBeNull = NULL;

        //
        //  Now add it to the index.  We can only add to a leaf, so force our
        //  position back to the correct spot in a leaf first.
        //

        LocalIndexContext->Current = LocalIndexContext->Top;
        AddToIndex( IrpContext, Scb, (PINDEX_ENTRY)&IE, LocalIndexContext, QuickIndex, FALSE );

    } finally{

        DebugUnwind( NtfsAddIndexEntry );

        if (!ARGUMENT_PRESENT( IndexContext )) {

            NtfsCleanupIndexContext( IrpContext, LocalIndexContext );
        }
    }

    DebugTrace( -1, Dbg, ("NtfsAddIndexEntry -> VOID\n") );

    return;
}


VOID
NtfsDeleteIndexEntry (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb,
    IN PVOID Value,
    IN PFILE_REFERENCE FileReference
    )

/*++

Routine Description:

    This routine may be called to delete a specified index entry.  The
    first entry is removed which matches the value exactly (including in Case,
    if relevant) and the file reference.

Arguments:

    Scb - Supplies the Scb for the index.

    Value - Supplies a pointer to the value to delete from the index.

    FileReference - Supplies the file reference of the index entry.

Return Value:

    None

--*/

{
    INDEX_CONTEXT IndexContext;
    PINDEX_ENTRY IndexEntry;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_SCB( Scb );
    ASSERT_EXCLUSIVE_SCB( Scb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsDeleteIndexEntry\n") );
    DebugTrace( 0, Dbg, ("Scb = %08lx\n", Scb) );
    DebugTrace( 0, Dbg, ("Value = %08lx\n", Value) );
    DebugTrace( 0, Dbg, ("FileReference = %08lx\n", FileReference) );

    NtfsInitializeIndexContext( &IndexContext );

    try {

        //
        //  Position to first possible match.
        //

        FindFirstIndexEntry( IrpContext,
                             Scb,
                             Value,
                             &IndexContext );

        //
        //  See if there is an actual match.
        //

        if (!FindNextIndexEntry( IrpContext,
                                 Scb,
                                 Value,
                                 FALSE,
                                 FALSE,
                                 &IndexContext,
                                 FALSE,
                                 NULL )) {

            ASSERTMSG( "NtfsDeleteIndexEntry does not exist", FALSE );

            NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Scb->Fcb );
        }

        //
        //  Extract the found index entry pointer.
        //

        IndexEntry = IndexContext.Current->IndexEntry;

        //
        //  If the file reference also matches, then this is the one we
        //  are supposed to delete.
        //

        if (!NtfsEqualMftRef(&IndexEntry->FileReference, FileReference)) {

            ASSERTMSG( "NtfsDeleteIndexEntry unexpected file reference", FALSE );

            NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Scb->Fcb );
        }

        DeleteFromIndex( IrpContext, Scb, &IndexContext );

    } finally{

        DebugUnwind( NtfsDeleteIndexEntry );

        NtfsCleanupIndexContext( IrpContext, &IndexContext );
    }

    DebugTrace( -1, Dbg, ("NtfsDeleteIndexEntry -> VOID\n") );

    return;
}


VOID
NtfsPushIndexRoot (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb
    )

/*++

Routine Description:

    This routine may be called to "push" the index root, i.e., add another
    level to the BTree, to make more room in the file record.

Arguments:

    Scb - Supplies the Scb for the index.

Return Value:

    None

--*/

{
    INDEX_CONTEXT IndexContext;
    PINDEX_LOOKUP_STACK Sp;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_SCB( Scb );
    ASSERT_EXCLUSIVE_SCB( Scb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsPushIndexRoot\n") );
    DebugTrace( 0, Dbg, ("Scb = %08lx\n", Scb) );

    NtfsInitializeIndexContext( &IndexContext );

    try {

        //
        //  Position to first possible match.
        //

        FindFirstIndexEntry( IrpContext,
                             Scb,
                             NULL,
                             &IndexContext );

        //
        //  See if the stack will have to be grown to do the push
        //

        Sp = IndexContext.Top + 1;

        if (Sp >= IndexContext.Base + (ULONG)IndexContext.NumberEntries) {
            NtfsGrowLookupStack( Scb,
                                 &IndexContext,
                                 &Sp );
        }

        PushIndexRoot( IrpContext, Scb, &IndexContext );

    } finally{

        DebugUnwind( NtfsPushIndexRoot );

        NtfsCleanupIndexContext( IrpContext, &IndexContext );
    }

    DebugTrace( -1, Dbg, ("NtfsPushIndexRoot -> VOID\n") );

    return;
}


BOOLEAN
NtfsRestartIndexEnumeration (
    IN PIRP_CONTEXT IrpContext,
    IN PCCB Ccb,
    IN PSCB Scb,
    IN PVOID Value,
    IN BOOLEAN IgnoreCase,
    IN BOOLEAN NextFlag,
    OUT PINDEX_ENTRY *IndexEntry,
    IN PFCB AcquiredFcb
    )

/*++

Routine Description:

    This routine may be called to start or restart an index enumeration,
    according to the parameters as described below.  The first matching
    entry, if any, is returned by this call.  Subsequent entries, if any,
    may be returned by subsequent calls to NtfsContinueIndexEnumeration.

    For each entry found, a pointer is returned to a copy of the entry, in
    dynamically allocated pool pointed to by the Ccb.  Therefore, there is
    nothing for the caller to unpin.

    Note that the Value, ValueLength, and IgnoreCase parameters on the first
    call for a given Ccb fix what will be returned for this Ccb forever.  A
    subsequent call to this routine may also specify these parameters, but
    in this case these parameters will be used for positioning only; all
    matches returned will continue to match the value and IgnoreCase flag
    specified on the first call for the Ccb.

    Note that all calls to this routine must be from within a try-finally,
    and the finally clause must include a call to NtfsCleanupAfterEnumeration.

Arguments:

    Ccb - Pointer to the Ccb for this enumeration.

    Scb - Supplies the Scb for the index.

    Value - Pointer to the value containing the pattern which is to match
            all returns for enumerations on this Ccb.

    IgnoreCase - If FALSE, all returns will match the pattern value with
                 exact case (if relevant).  If TRUE, all returns will match
                 the pattern value ignoring case.  On a second or subsequent
                 call for a Ccb, this flag may be specified differently just
                 for positioning.  For example, an IgnoreCase TRUE enumeration
                 may be restarted at a previously returned value found by exact
                 case match.

    NextFlag - FALSE if the first match of the enumeration is to be returned.
               TRUE if the next match after the first one is to be returned.

    IndexEntry - Returns a pointer to a copy of the index entry.

    AcquiredFcb - Supplies a pointer to an Fcb which has been preacquired to
                  potentially aide NtfsRetrieveOtherFileName

Return Value:

    FALSE - If no match is being returned, and the output pointer is undefined.
    TRUE - If a match is being returned.

--*/

{
    PINDEX_ENTRY FoundIndexEntry;
    INDEX_CONTEXT OtherContext;
    BOOLEAN WildCardsInExpression;
    BOOLEAN SynchronizationError;
    PWCH UpcaseTable = IrpContext->Vcb->UpcaseTable;
    PINDEX_CONTEXT IndexContext = NULL;
    BOOLEAN CleanupOtherContext = FALSE;
    BOOLEAN Result = FALSE;
    BOOLEAN ContextJustCreated = FALSE;

    PAGED_CODE();

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_CCB( Ccb );
    ASSERT_SCB( Scb );
    ASSERT_SHARED_SCB( Scb );
    ASSERT( ARGUMENT_PRESENT(Value) || (Ccb->IndexContext != NULL) );

    DebugTrace( +1, Dbg, ("NtfsRestartIndexEnumeration\n") );
    DebugTrace( 0, Dbg, ("Ccb = %08lx\n", Ccb) );
    DebugTrace( 0, Dbg, ("Scb = %08lx\n", Scb) );
    DebugTrace( 0, Dbg, ("Value = %08lx\n", Value) );
    DebugTrace( 0, Dbg, ("IgnoreCase = %02lx\n", IgnoreCase) );
    DebugTrace( 0, Dbg, ("NextFlag = %02lx\n", NextFlag) );

    try {

        //
        //  If the Ccb does not yet have an index context, then we must
        //  allocate one and initialize this Context and the Ccb as well.
        //

        if (Ccb->IndexContext == NULL) {

            //
            //  Allocate and initialize the index context.
            //

            Ccb->IndexContext = (PINDEX_CONTEXT)ExAllocateFromPagedLookasideList( &NtfsIndexContextLookasideList );

            NtfsInitializeIndexContext( Ccb->IndexContext );
            ContextJustCreated = TRUE;

            //
            //  Capture the caller's IgnoreCase flag.
            //

            if (IgnoreCase) {
                SetFlag( Ccb->Flags, CCB_FLAG_IGNORE_CASE );
            }
        }

        //
        //  Pick up the pointer to the index context, and save the current
        //  change count from the Scb.
        //

        IndexContext = Ccb->IndexContext;

        //
        //  The first step of enumeration is to position our IndexContext.
        //

        FindFirstIndexEntry( IrpContext,
                             Scb,
                             Value,
                             IndexContext );

        //
        //  The following code only applies to file name indices.
        //

        if (!FlagOn( Scb->ScbState, SCB_STATE_VIEW_INDEX )) {

            //
            //  Remember if there are wild cards.
            //

            if ((*NtfsContainsWildcards[Scb->ScbType.Index.CollationRule])
                                        ( Value )) {

                WildCardsInExpression = TRUE;

            } else {

                WildCardsInExpression = FALSE;
            }

            //
            //  If the operation is caseinsensitive, upcase the string.
            //

            if (IgnoreCase) {

                (*NtfsUpcaseValue[Scb->ScbType.Index.CollationRule])
                                  ( UpcaseTable,
                                    IrpContext->Vcb->UpcaseTableSize,
                                    Value );
            }
        } else {

            //
            //  For view indices, it is implied that all searches
            //  are wildcard searches.
            //

            WildCardsInExpression = TRUE;
        }

        //
        //  If this is not actually the first call, then we have to
        //  position exactly to the Value specified, and set NextFlag
        //  correctly.  The first call can either the initial call
        //  to query or the first call after a restart.
        //

        if (!ContextJustCreated && NextFlag) {

            PIS_IN_EXPRESSION MatchRoutine;
            PFILE_NAME NameInIndex;
            BOOLEAN ItsThere;

            //
            //  See if the specified value is actually there, because
            //  we are not allowed to resume from a Dos-only name.
            //

            ItsThere = FindNextIndexEntry( IrpContext,
                                           Scb,
                                           Value,
                                           WildCardsInExpression,
                                           IgnoreCase,
                                           IndexContext,
                                           FALSE,
                                           NULL );

            //
            //  We will set up pointers from our returns, but we must
            //  be careful only to use them if we found something.
            //

            FoundIndexEntry = IndexContext->Current->IndexEntry;
            NameInIndex = (PFILE_NAME)(FoundIndexEntry + 1);

            //
            //  Figure out which match routine to use.
            //

            if (FlagOn(Ccb->Flags, CCB_FLAG_WILDCARD_IN_EXPRESSION)) {
                MatchRoutine = NtfsIsInExpression[COLLATION_FILE_NAME];
            } else {
                MatchRoutine = (PIS_IN_EXPRESSION)NtfsIsEqual[COLLATION_FILE_NAME];
            }

            //
            //  If we are trying to resume from a Ntfs-only or Dos-Only name, then
            //  we take action here.  Do not do this on the internal
            //  call from NtfsContinueIndexEnumeration, which is the
            //  only one who would point at the index entry in the Ccb.
            //
            //  We can think of this code this way.  No matter what our search
            //  expression is, we traverse the index only one way.  For each
            //  name we find, we will only return the file name once - either
            //  from an Ntfs-only match or from a Dos-only match if the Ntfs-only
            //  name does not match.  Regardless of whether resuming from the
            //  Ntfs-Only or Dos-only name, we still can determine a unique
            //  position in the directory.  That unique position is the Ntfs-only
            //  name if it matches the expression, or else the Dos-only name if
            //  it only matches.  In the illegal case that neither matches, we
            //  arbitrarily resume from the Ntfs-only name.
            //
            //      This code may be read aloud to the tune
            //          "While My Heart Gently Weeps"
            //

            if (ItsThere &&
                (Value != (PVOID)(Ccb->IndexEntry + 1)) &&
                (Scb->ScbType.Index.CollationRule == COLLATION_FILE_NAME) &&

                //
                //  Is it a Dos-only or Ntfs-only name?
                //

                (BooleanFlagOn( NameInIndex->Flags, FILE_NAME_DOS ) !=
                  BooleanFlagOn( NameInIndex->Flags, FILE_NAME_NTFS )) &&

                //
                //  Try to resume from the other name if he either gave
                //  us a Dos-only name, or he gave us an Ntfs-only name
                //  that does not fit in the search expression.
                //

                (FlagOn( NameInIndex->Flags, FILE_NAME_DOS ) ||
                 !(*MatchRoutine)( UpcaseTable,
                                   Ccb->QueryBuffer,
                                   FoundIndexEntry,
                                   IgnoreCase ))) {

                PFILE_NAME FileNameBuffer;
                ULONG FileNameLength;

                NtfsInitializeIndexContext( &OtherContext );
                CleanupOtherContext = TRUE;

                FileNameBuffer = NtfsRetrieveOtherFileName( IrpContext,
                                                            Ccb,
                                                            Scb,
                                                            FoundIndexEntry,
                                                            &OtherContext,
                                                            AcquiredFcb,
                                                            &SynchronizationError );

                //
                //  We have to position to the long name and actually
                //  resume from there.  To do this we have to cleanup and initialize
                //  the IndexContext in the Ccb, and lookup the long name we just
                //  found.
                //
                //  If the other index entry is not there, there is some minor
                //  corruption going on, but we will just charge on in that event.
                //  Also, if the other index entry is there, but it does not match
                //  our expression, then we are supposed to resume from the short
                //  name, so we carry on.
                //

                ItsThere = (FileNameBuffer != NULL);

                if (!ItsThere && SynchronizationError) {
                    NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
                }

                if (ItsThere &&

                    (FlagOn(Ccb->Flags, CCB_FLAG_WILDCARD_IN_EXPRESSION)  ?

                     NtfsFileNameIsInExpression(UpcaseTable,
                                                (PFILE_NAME)Ccb->QueryBuffer,
                                                FileNameBuffer,
                                                IgnoreCase) :



                     NtfsFileNameIsEqual(UpcaseTable,
                                         (PFILE_NAME)Ccb->QueryBuffer,
                                         FileNameBuffer,
                                         IgnoreCase))) {

                    ULONG SizeOfFileName = FIELD_OFFSET( FILE_NAME, FileName );

                    NtfsReinitializeIndexContext( IrpContext, IndexContext );

                    //
                    //  Extract a description of the file name from the found index
                    //  entry.
                    //

                    FileNameLength = FileNameBuffer->FileNameLength * sizeof( WCHAR );

                    //
                    //  Call FindFirst/FindNext to position our context to the corresponding
                    //  long name.
                    //

                    FindFirstIndexEntry( IrpContext,
                                         Scb,
                                         (PVOID)FileNameBuffer,
                                         IndexContext );

                    ItsThere = FindNextIndexEntry( IrpContext,
                                                   Scb,
                                                   (PVOID)FileNameBuffer,
                                                   FALSE,
                                                   FALSE,
                                                   IndexContext,
                                                   FALSE,
                                                   NULL );

                    if (!ItsThere) {

                        NtfsRaiseStatus( IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, Scb->Fcb );
                    }
                }
            }

            //
            //  NextFlag should only remain TRUE, if the specified value
            //  is actually there, and NextFlag was specified as TRUE
            //  on input.  In particular, it is important to make
            //  NextFlag FALSE if the specified value is not actually
            //  there.  (Experience shows this behavior is important to
            //  implement "delnode" correctly for the Lan Manager Server!)
            //

            NextFlag = (BOOLEAN)(NextFlag && ItsThere);

        //
        //  If we created the context then we need to remember if the
        //  expression has wildcards.
        //

        } else {

            //
            //  We may not handle correctly an initial enumeration with
            //  NextFlag TRUE, because the context is initially sitting
            //  in the root.  Dirctrl must always pass NextFlag FALSE
            //  on the initial enumeration.
            //

            ASSERT(!NextFlag);

            //
            //  Remember if the value has wild cards.
            //

            if (WildCardsInExpression) {

                SetFlag( Ccb->Flags, CCB_FLAG_WILDCARD_IN_EXPRESSION );
            }
        }

        //
        //  Now we are correctly positioned.  See if there is an actual
        //  match at our current position.  If not, return FALSE.
        //
        //  (Note, FindFirstIndexEntry always leaves us positioned in
        //  some leaf of the index, and it is the first FindNext that
        //  actually positions us to the first match.)
        //

        if (!FindNextIndexEntry( IrpContext,
                                 Scb,
                                 Ccb->QueryBuffer,
                                 BooleanFlagOn( Ccb->Flags, CCB_FLAG_WILDCARD_IN_EXPRESSION ),
                                 BooleanFlagOn( Ccb->Flags, CCB_FLAG_IGNORE_CASE ),
                                 IndexContext,
                                 NextFlag,
                                 NULL )) {

            try_return( Result = FALSE );
        }

        //
        //  If we get here, then we have a match that we want to return.
        //  We always copy the complete IndexEntry away and pass a pointer
        //  back to the copy.  See if our current buffer for the index entry
        //  is large enough.
        //

        FoundIndexEntry = IndexContext->Current->IndexEntry;

        if (Ccb->IndexEntryLength < (ULONG)FoundIndexEntry->Length) {

            //
            //  If there is a buffer currently allocated, deallocate it before
            //  allocating a larger one.  (Clear Ccb fields in case we get an
            //  allocation error.)
            //

            if (Ccb->IndexEntry != NULL) {

                NtfsFreePool( Ccb->IndexEntry );
                Ccb->IndexEntry = NULL;
                Ccb->IndexEntryLength = 0;
            }

            //
            //  Allocate a new buffer for the index entry we just found, with
            //  some "padding" in case the next match is larger.
            //

            Ccb->IndexEntry = (PINDEX_ENTRY)NtfsAllocatePool(PagedPool, (ULONG)FoundIndexEntry->Length + 16 );

            Ccb->IndexEntryLength = (ULONG)FoundIndexEntry->Length + 16;
        }

        //
        //  Now, save away our copy of the IndexEntry, and return a pointer
        //  to it.
        //

        RtlMoveMemory( Ccb->IndexEntry,
                       FoundIndexEntry,
                       (ULONG)FoundIndexEntry->Length );

        *IndexEntry = Ccb->IndexEntry;

        try_return( Result = TRUE );

    try_exit: NOTHING;

    } finally {

        DebugUnwind( NtfsRestartIndexEnumeration );

        if (CleanupOtherContext) {
            NtfsCleanupIndexContext( IrpContext, &OtherContext );
        }
        //
        //  If we died during the first call, then deallocate everything
        //  that we might have allocated.
        //

        if (AbnormalTermination() && ContextJustCreated) {

            if (Ccb->IndexEntry != NULL) {
                NtfsFreePool( Ccb->IndexEntry );
                Ccb->IndexEntry = NULL;
            }

            if (Ccb->IndexContext != NULL) {
                NtfsCleanupIndexContext( IrpContext, Ccb->IndexContext );
                ExFreeToPagedLookasideList( &NtfsIndexContextLookasideList, Ccb->IndexContext );
                Ccb->IndexContext = NULL;
            }
        }

        //
        //  Remember if we are not returning anything, to save work later.
        //

        if (!Result && (Ccb->IndexEntry != NULL)) {
            Ccb->IndexEntry->Length = 0;
        }
    }

    DebugTrace( 0, Dbg, ("*IndexEntry < %08lx\n", *IndexEntry) );
    DebugTrace( -1, Dbg, ("NtfsRestartIndexEnumeration -> %08lx\n", Result) );

    return Result;
}


BOOLEAN
NtfsContinueIndexEnumeration (
    IN PIRP_CONTEXT IrpContext,
    IN PCCB Ccb,
    IN PSCB Scb,
    IN BOOLEAN NextFlag,
    OUT PINDEX_ENTRY *IndexEntry
    )

/*++

Routine Description:

    This routine may be called to return again the last match on an active
    enumeration, or to return the next match.  Enumerations must always be
    started or restarted via a call to NtfsRestartIndexEnumeration.

    Note that all calls to this routine must be from within a try-finally,
    and the finally clause must include a call to NtfsCleanupAfterEnumeration.

Arguments:

    Ccb - Pointer to the Ccb for this enumeration.

    Scb - Supplies the Scb for the index.

    NextFlag - FALSE if the last returned match is to be returned again.
               TRUE if the next match is to be returned.

    IndexEntry - Returns a pointer to a copy of the index entry.

Return Value:

    FALSE - If no match is being returned, and the output pointer is undefined.
    TRUE - If a match is being returned.

--*/

{
    PINDEX_ENTRY FoundIndexEntry;
    PINDEX_CONTEXT IndexContext;
    BOOLEAN MustRestart;
    BOOLEAN Result = FALSE;

    ASSERT_IRP_CONTEXT( IrpContext );
    ASSERT_CCB( Ccb );
    ASSERT_SCB( Scb );
    ASSERT_SHARED_SCB( Scb );

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsContinueIndexEnumeration\n") );
    DebugTrace( 0, Dbg, ("Ccb = %08lx\n", Ccb) );
    DebugTrace( 0, Dbg, ("Scb = %08lx\n", Scb) );
    DebugTrace( 0, Dbg, ("NextFlag = %02lx\n", NextFlag) );

    //
    //  It seems many apps like to come back one more time and really get
    //  an error status, so if we did not return anything last time, we can
    //  get out now too.
    //
    //  There also may be no index entry, in the case of an empty directory
    //  and dirctrl is cycling through with "." and "..".
   