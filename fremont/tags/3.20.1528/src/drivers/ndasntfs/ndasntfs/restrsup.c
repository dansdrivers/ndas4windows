we must always skip the first record.
        //
        //  Loop to read all subsequent records to the end of the log file.
        //

        while ( LfsReadNextLogRecord( LogHandle,
                                      LogContext,
                                      &RecordType,
                                      &TransactionId,
                                      &UndoNextLsn,
                                      &PreviousLsn,
                                      &LogRecordLsn,
                                      &LogRecordLength,
                                      (PVOID *)&LogRecord )) {

            //
            //  Check that the log record is valid.
            //

            if (!NtfsCheckLogRecord( LogRecord,
                                     LogRecordLength,
                                     TransactionId,
                                     Vcb->OatEntrySize )) {

                NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
            }

            //
            //  The first Lsn after the previous Lsn remembered in the checkpoint is
            //  the first candidate for the RedoLsn.
            //

            if (RedoLsn->QuadPart == 0) {
                *RedoLsn = LogRecordLsn;
            }

            if (RecordType != LfsClientRecord) {
                continue;
            }

            DebugTrace( 0, Dbg, ("Analysis of LogRecord at: %08lx\n", LogRecord) );
            DebugTrace( 0, Dbg, ("Log Record Lsn = %016I64x\n", LogRecordLsn) );
            DebugTrace( 0, Dbg, ("LogRecord->RedoOperation = %08lx\n", LogRecord->RedoOperation) );
            DebugTrace( 0, Dbg, ("TransactionId = %08lx\n", TransactionId) );

            //
            //  Now update the Transaction Table for this transaction.  If there is no
            //  entry present or it is unallocated we allocate the entry.
            //

            Transaction = (PTRANSACTION_ENTRY)GetRestartEntryFromIndex( &Vcb->TransactionTable,
                                                                        TransactionId );

            if (!IsRestartIndexWithinTable( &Vcb->TransactionTable, TransactionId ) ||
                !IsRestartTableEntryAllocated( Transaction )) {

                Transaction = (PTRANSACTION_ENTRY) NtfsAllocateRestartTableFromIndex( &Vcb->TransactionTable,
                                                                                      TransactionId );

                Transaction->TransactionState = TransactionActive;
                Transaction->FirstLsn = LogRecordLsn;
            }

            Transaction->PreviousLsn =
            Transaction->UndoNextLsn = LogRecordLsn;

            //
            //  If this is a compensation log record (CLR), then change the UndoNextLsn to
            //  be the UndoNextLsn of this record.
            //

            if (LogRecord->UndoOperation == CompensationLogRecord) {

                Transaction->UndoNextLsn = UndoNextLsn;
            }

            //
            //  Dispatch to handle log record depending on type.
            //

            switch (LogRecord->RedoOperation) {

            //
            //  The following cases are performing various types of updates
            //  and need to make the appropriate updates to the Transaction
            //  and Dirty Page Tables.
            //

            case InitializeFileRecordSegment:
            case DeallocateFileRecordSegment:
            case WriteEndOfFileRecordSegment:
            case CreateAttribute:
            case DeleteAttribute:
            case UpdateResidentValue:
            case UpdateNonresidentValue:
            case UpdateMappingPairs:
            case SetNewAttributeSizes:
            case AddIndexEntryRoot:
            case DeleteIndexEntryRoot:
            case AddIndexEntryAllocation:
            case DeleteIndexEntryAllocation:
            case WriteEndOfIndexBuffer:
            case SetIndexEntryVcnRoot:
            case SetIndexEntryVcnAllocation:
            case UpdateFileNameRoot:
            case UpdateFileNameAllocation:
            case SetBitsInNonresidentBitMap:
            case ClearBitsInNonresidentBitMap:
            case UpdateRecordDataRoot:
            case UpdateRecordDataAllocation:

                PageUpdateAnalysis( Vcb,
                                    LogRecordLsn,
                                    DirtyPageTable,
                                    LogRecord );

                break;

            //
            //  This case is deleting clusters from a nonresident attribute,
            //  thus it deletes a range of pages from the Dirty Page Table.
            //  This log record is written each time a nonresident attribute
            //  is truncated, whether explicitly or as part of deletion.
            //
            //  Processing one of these records is pretty compute-intensive
            //  (three nested loops, where a couple of them can be large),
            //  but this is the code that prevents us from dropping, for example,
            //  index updates into the middle of user files, if the index stream
            //  is truncated and the sectors are reallocated to a user file
            //  and we crash after the user data has been written.
            //
            //  I.e., note the following sequence:
            //
            //      <checkpoint>
            //      <Index update>
            //      <Index page deleted>
            //      <Same cluster(s) reallocated to user file>
            //      <User data written>
            //
            //      CRASH!
            //
            //  Since the user data was not logged (else there would be no problem),
            //  It could get overwritten while applying the index update after a
            //  crash - Pisses off the user as well as the security dudes!
            //

            case DeleteDirtyClusters:

                {
                    PDIRTY_PAGE_ENTRY DirtyPage;
                    PLCN_RANGE LcnRange;
                    ULONG i, j;
                    LCN FirstLcn, LastLcn;
                    ULONG RangeCount = LogRecord->RedoLength / sizeof(LCN_RANGE);

                    //
                    //  Point to the Lcn range array.
                    //

                    LcnRange = Add2Ptr(LogRecord, LogRecord->RedoOffset);

                    //
                    //  Loop through all of the Lcn ranges in this log record.
                    //

                    for (i = 0; i < RangeCount; i++) {

                        FirstLcn = LcnRange[i].StartLcn;
                        LastLcn = FirstLcn + (LcnRange[i].Count - 1);

                        DebugTrace( 0, Dbg, ("Deleting from FirstLcn = %016I64x\n", FirstLcn));
                        DebugTrace( 0, Dbg, ("Deleting to LastLcn =  %016I64x\n", LastLcn ));

                        //
                        //  Point to first Dirty Page Entry.
                        //

                        DirtyPage = NtfsGetFirstRestartTable( DirtyPageTable );

                        //
                        //  Loop to end of table.
                        //

                        while (DirtyPage != NULL) {

                            //
                            //  Loop through all of the Lcns for this dirty page.
                            //

                            for (j = 0; j < (ULONG)DirtyPage->LcnsToFollow; j++) {

                                if ((DirtyPage->LcnsForPage[j] >= FirstLcn) &&
                                    (DirtyPage->LcnsForPage[j] <= LastLcn)) {

                                    DirtyPage->LcnsForPage[j] = 0;
                                }
                            }

                            //
                            //  Point to next entry in table, or NULL.
                            //

                            DirtyPage = NtfsGetNextRestartTable( DirtyPageTable,
                                                                 DirtyPage );
                        }
                    }
                }

                break;

            //
            //  When a record is encountered for a nonresident attribute that
            //  was opened, we have to add an entry to the Open Attribute Table.
            //

            case OpenNonresidentAttribute:

                {
                    POPEN_ATTRIBUTE_ENTRY AttributeEntry;
                    ULONG NameSize;

                    //
                    //  If the table is not currently big enough, then we must
                    //  expand it.
                    //

                    if (!IsRestartIndexWithinTable( Vcb->OnDiskOat,
                                                    (ULONG)LogRecord->TargetAttribute )) {

                        ULONG NeededEntries;

                        //
                        //  Compute how big the table needs to be.  Add 10 extra entries
                        //  for some cushion.
                        //

                        NeededEntries = (LogRecord->TargetAttribute / Vcb->OnDiskOat->Table->EntrySize);
                        NeededEntries = (NeededEntries + 10 - Vcb->OnDiskOat->Table->NumberEntries);

                        NtfsExtendRestartTable( Vcb->OnDiskOat,
                                                NeededEntries,
                                                MAXULONG );
                    }

                    ASSERT( IsRestartIndexWithinTable( Vcb->OnDiskOat,
                                                       (ULONG)LogRecord->TargetAttribute ));

                    //
                    //  Calculate size of Attribute Name Entry, if there is one.
                    //

                    NameSize = LogRecord->UndoLength;

                    //
                    //  Point to the entry being opened.
                    //

                    OatData = NtfsAllocatePool( PagedPool, sizeof( OPEN_ATTRIBUTE_DATA ) );
                    RtlZeroMemory( OatData, sizeof( OPEN_ATTRIBUTE_DATA ));

                    OatData->OnDiskAttributeIndex = LogRecord->TargetAttribute;

                    AttributeEntry = NtfsAllocateRestartTableFromIndex( Vcb->OnDiskOat,
                                                                        LogRecord->TargetAttribute );

                    //
                    //  The attribute entry better either not be allocated or it must
                    //  be for the same file.
                    //

                    //  **** May eliminate this test.
                    //
                    //  ASSERT( !IsRestartTableEntryAllocated(AttributeEntry) ||
                    //          xxEql(AttributeEntry->FileReference,
                    //                ((POPEN_ATTRIBUTE_ENTRY)Add2Ptr(LogRecord,
                    //                                                LogRecord->RedoOffset))->FileReference));

                    //
                    //  Initialize this entry from the log record.
                    //

                    ASSERT( LogRecord->RedoLength == Vcb->OnDiskOat->Table->EntrySize );

                    RtlCopyMemory( AttributeEntry,
                                   (PCHAR)LogRecord + LogRecord->RedoOffset,
                                   LogRecord->RedoLength );

                    ASSERT( IsRestartTableEntryAllocated(AttributeEntry) );

                    //
                    //  Get a new entry for the in-memory copy if needed.
                    //

                    if (Vcb->RestartVersion == 0) {

                        POPEN_ATTRIBUTE_ENTRY_V0 OldEntry = (POPEN_ATTRIBUTE_ENTRY_V0) AttributeEntry;
                        ULONG NewIndex;

                        NewIndex = NtfsAllocateRestartTableIndex( &Vcb->OpenAttributeTable, TRUE );
                        AttributeEntry = GetRestartEntryFromIndex( &Vcb->OpenAttributeTable, NewIndex );

                        AttributeEntry->BytesPerIndexBuffer = OldEntry->BytesPerIndexBuffer;

                        AttributeEntry->AttributeTypeCode = OldEntry->AttributeTypeCode;
                        AttributeEntry->FileReference = OldEntry->FileReference;
                        AttributeEntry->LsnOfOpenRecord.QuadPart = OldEntry->LsnOfOpenRecord.QuadPart;

                        OldEntry->OatIndex = NewIndex;

                    }

                    //
                    //  Finish initializing the AttributeData.
                    //

                    AttributeEntry->OatData = OatData;
                    InsertTailList( &Vcb->OpenAttributeData, &OatData->Links );
                    OatData = NULL;

                    //
                    //  If there is a name at the end, then allocate space to
                    //  copy it into, and do the copy.  We also set the buffer
                    //  pointer in the string descriptor, although note that the
                    //  lengths must be correct.
                    //

                    if (NameSize != 0) {

                        AttributeEntry->OatData->Overlay.AttributeName =
                          NtfsAllocatePool( NonPagedPool, NameSize );
                        RtlCopyMemory( AttributeEntry->OatData->Overlay.AttributeName,
                                       Add2Ptr(LogRecord, LogRecord->UndoOffset),
                                       NameSize );

                        AttributeEntry->OatData->AttributeName.Buffer = AttributeEntry->OatData->Overlay.AttributeName;

                        AttributeEntry->OatData->AttributeNamePresent = TRUE;

                    //
                    //  Otherwise, show there is no name.
                    //

                    } else {
                        AttributeEntry->OatData->Overlay.AttributeName = NULL;
                        AttributeEntry->OatData->AttributeName.Buffer = NULL;
                        AttributeEntry->OatData->AttributeNamePresent = FALSE;
                    }

                    AttributeEntry->OatData->AttributeName.MaximumLength =
                    AttributeEntry->OatData->AttributeName.Length = (USHORT) NameSize;
                }

                break;

            //
            //  For HotFix records, we need to update the Lcn in the Dirty Page
            //  Table.
            //

            case HotFix:

                {
                    PDIRTY_PAGE_ENTRY DirtyPage;

                    //
                    //  First see if the Vcn is currently in the Dirty Page
                    //  Table.  If not, there is nothing to do.
                    //

                    if (FindDirtyPage( DirtyPageTable,
                                       LogRecord->TargetAttribute,
                                       LogRecord->TargetVcn,
                                       &DirtyPage )) {

                        //
                        //  Index to the Lcn in question in the Dirty Page Entry
                        //  and rewrite it with the Hot Fixed Lcn from the log
                        //  record.  Note that it is ok to just use the LowPart
                        //  of the Vcns to calculate the array offset, because
                        //  any multiple of 2**32 is guaranteed to be on a page
                        //  boundary!
                        //

                        if (DirtyPage->LcnsForPage[((ULONG)LogRecord->TargetVcn) - ((ULONG)DirtyPage->Vcn)] != 0) {

                            DirtyPage->LcnsForPage[((ULONG)LogRecord->TargetVcn) - ((ULONG)DirtyPage->Vcn)] = LogRecord->LcnsForPage[0];
                        }
                    }
                }

                break;

            //
            //  For end top level action, we will just update the transaction
            //  table to skip the top level action on undo.
            //

            case EndTopLevelAction:

                {
                    PTRANSACTION_ENTRY Transaction;

                    //
                    //  Now update the Transaction Table for this transaction.
                    //

                    Transaction = (PTRANSACTION_ENTRY)GetRestartEntryFromIndex( &Vcb->TransactionTable,
                                                                                TransactionId );

                    Transaction->PreviousLsn = LogRecordLsn;
                    Transaction->UndoNextLsn = UndoNextLsn;

                }

                break;

            //
            //  For Prepare Transaction, we just change the state of our entry.
            //

            case PrepareTransaction:

                {
                    PTRANSACTION_ENTRY CurrentEntry;

                    CurrentEntry = GetRestartEntryFromIndex( &Vcb->TransactionTable,
                                                             TransactionId );

                    ASSERT( !IsRestartTableEntryAllocated( CurrentEntry ));

                    CurrentEntry->TransactionState = TransactionPrepared;
                }

                break;

            //
            //  For Commit Transaction, we just change the state of our entry.
            //

            case CommitTransaction:

                {
                    PTRANSACTION_ENTRY CurrentEntry;

                    CurrentEntry = GetRestartEntryFromIndex( &Vcb->TransactionTable,
                                                             TransactionId );

                    ASSERT( !IsRestartTableEntryAllocated( CurrentEntry ));

                    CurrentEntry->TransactionState = TransactionCommitted;
                }

                break;

            //
            //  For forget, we can delete our transaction entry, since the transaction
            //  will not have to be aborted.
            //

            case ForgetTransaction:

                {
                    NtfsFreeRestartTableIndex( &Vcb->TransactionTable,
                                               TransactionId );
                }

                break;

            //
            //  The following cases require no action in the Analysis Pass.
            //

            case Noop:
            case OpenAttributeTableDump:
            case AttributeNamesDump:
            case DirtyPageTableDump:
            case TransactionTableDump:

                break;

            //
            //  All codes will be explicitly handled.  If we see a code we
            //  do not expect, then we are in trouble.
            //

            default:

                DebugTrace( 0, Dbg, ("Unexpected Log Record Type: %04lx\n", LogRecord->RedoOperation) );
                DebugTrace( 0, Dbg, ("Record address: %08lx\n", LogRecord) );
                DebugTrace( 0, Dbg, ("Record length: %08lx\n", LogRecordLength) );

                ASSERTMSG( "Unknown Action!\n", FALSE );

                break;
            }
        }

    } finally {

        //
        //  Finally we can kill the log handle.
        //

        LfsTerminateLogQuery( LogHandle, LogContext );

        if (OatData != NULL) { NtfsFreePool( OatData ); }
    }

    //
    //  Now we just have to scan the Dirty Page Table and Transaction Table
    //  for the lowest Lsn, and return it as the Redo Lsn.
    //

    {
        PDIRTY_PAGE_ENTRY DirtyPage;

        //
        //  Point to first Dirty Page Entry.
        //

        DirtyPage = NtfsGetFirstRestartTable( DirtyPageTable );

        //
        //  Loop to end of table.
        //

        while (DirtyPage != NULL) {

            //
            //  Update the Redo Lsn if this page has an older one.
            //

            if ((DirtyPage->OldestLsn.QuadPart != 0) &&
                (DirtyPage->OldestLsn.QuadPart < RedoLsn->QuadPart)) {

                *RedoLsn = DirtyPage->OldestLsn;
            }

            //
            //  Point to next entry in table, or NULL.
            //

            DirtyPage = NtfsGetNextRestartTable( DirtyPageTable,
                                                 DirtyPage );
        }
    }

    {
        PTRANSACTION_ENTRY Transaction;

        //
        //  Point to first Transaction Entry.
        //

        Transaction = NtfsGetFirstRestartTable( &Vcb->TransactionTable );

        //
        //  Loop to end of table.
        //

        while (Transaction != NULL) {

            //
            //  Update the Redo Lsn if this transaction has an older one.
            //

            if ((Transaction->FirstLsn.QuadPart != 0) &&
                (Transaction->FirstLsn.QuadPart < RedoLsn->QuadPart)) {

                *RedoLsn = Transaction->FirstLsn;
            }

            //
            //  Point to next entry in table, or NULL.
            //

            Transaction = NtfsGetNextRestartTable( &Vcb->TransactionTable,
                                                   Transaction );
        }
    }

    DebugTrace( 0, Dbg, ("RedoLsn > %016I64x\n", *RedoLsn) );
    DebugTrace( 0, Dbg, ("AnalysisPass -> VOID\n") );
}


//
//  Internal support routine
//

VOID
RedoPass (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LSN RedoLsn,
    IN OUT PRESTART_POINTERS DirtyPageTable
    )

/*++

Routine Description:

    This routine performs the Redo Pass of Restart.  Beginning at the
    Redo Lsn established during the Analysis Pass, the redo operations
    of all log records are applied, until the end of file is encountered.

    Updates are only applied to clusters in the dirty page table.  If a
    cluster was deleted, then its entry will have been deleted during the
    Analysis Pass.

    The Redo actions are all performed in the common routine DoAction,
    which is also used by the Undo Pass.

Arguments:

    Vcb - Volume which is being restarted.

    RedoLsn - Lsn at which the Redo Pass is to begin.

    DirtyPageTable - Pointer to the Dirty Page Table, as reconstructed
                     from the Analysis Pass.

Return Value:

    None.

--*/

{
    LFS_LOG_CONTEXT LogContext;
    PNTFS_LOG_RECORD_HEADER LogRecord;
    ULONG LogRecordLength;
    PVOID Data;
    ULONG Length;
    LFS_RECORD_TYPE RecordType;
    TRANSACTION_ID TransactionId;
    LSN UndoNextLsn;
    LSN PreviousLsn;
    ULONG i, SavedLength;

    LSN LogRecordLsn = RedoLsn;
    LFS_LOG_HANDLE LogHandle = Vcb->LogHandle;
    PBCB PageBcb = NULL;
    BOOLEAN GeneratedUsnBias = FALSE;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("RedoPass:\n") );
    DebugTrace( 0, Dbg, ("RedoLsn = %016I64x\n", RedoLsn) );
    DebugTrace( 0, Dbg, ("DirtyPageTable = %08lx\n", DirtyPageTable) );

    //
    //  If the dirty page table is empty, then we can skip the entire Redo Pass.
    //

    if (IsRestartTableEmpty( DirtyPageTable )) {
        return;
    }

    //
    //  Read the record at the Redo Lsn, before falling into common code
    //  to handle each record.
    //

    LfsReadLogRecord( LogHandle,
                      RedoLsn,
                      LfsContextForward,
                      &LogContext,
                      &RecordType,
                      &TransactionId,
                      &UndoNextLsn,
                      &PreviousLsn,
                      &LogRecordLength,
                      (PVOID *)&LogRecord );

    //
    //  Now loop to read all of our log records forwards, until we hit
    //  the end of the file, cleaning up at the end.
    //

    try {

        do {

            PDIRTY_PAGE_ENTRY DirtyPage;
            PLSN PageLsn;
            BOOLEAN FoundPage;

            if (RecordType != LfsClientRecord) {
                continue;
            }

            //
            //  Check that the log record is valid.
            //

            if (!NtfsCheckLogRecord( LogRecord,
                                     LogRecordLength,
                                     TransactionId,
                                     Vcb->OatEntrySize )) {

                NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
            }

            DebugTrace( 0, Dbg, ("Redo of LogRecord at: %08lx\n", LogRecord) );
            DebugTrace( 0, Dbg, ("Log Record Lsn = %016I64x\n", LogRecordLsn) );

            //
            //  Ignore log records that do not update pages.
            //

            if (LogRecord->LcnsToFollow == 0) {

                DebugTrace( 0, Dbg, ("Skipping log record (no update)\n") );

                continue;
            }

            //
            //  Consult Dirty Page Table to see if we have to apply this update.
            //  If the page is not there, or if the Lsn of this Log Record is
            //  older than the Lsn in the Dirty Page Table, then we do not have
            //  to apply the update.
            //

            FoundPage = FindDirtyPage( DirtyPageTable,
                                       LogRecord->TargetAttribute,
                                       LogRecord->TargetVcn,
                                       &DirtyPage );

            if (!FoundPage

                    ||

                (LogRecordLsn.QuadPart < DirtyPage->OldestLsn.QuadPart)) {

                DebugDoit(

                    DebugTrace( 0, Dbg, ("Skipping log record operation %08lx\n",
                                         LogRecord->RedoOperation ));

                    if (!FoundPage) {
                        DebugTrace( 0, Dbg, ("Page not in dirty page table\n") );
                    } else {
                        DebugTrace( 0, Dbg, ("Page Lsn more current: %016I64x\n",
                                              DirtyPage->OldestLsn) );
                    }
                );

                continue;

            //
            //  We also skip the update if the entry was never put in the Mcb for
            //  the file.

            } else {

                POPEN_ATTRIBUTE_ENTRY ThisEntry;
                PSCB TargetScb;
                LCN TargetLcn;

                //
                //  Check that the entry is within the table and is allocated.
                //

                if (!IsRestartIndexWithinTable( Vcb->OnDiskOat,
                                                LogRecord->TargetAttribute )) {

                    NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
                }

                ThisEntry = GetRestartEntryFromIndex( Vcb->OnDiskOat, LogRecord->TargetAttribute );

                if (!IsRestartTableEntryAllocated( ThisEntry )) {

                    NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
                }

                //
                //  Check if we need to go to a different restart table.
                //

                if (Vcb->RestartVersion == 0) {

                    ThisEntry = GetRestartEntryFromIndex( &Vcb->OpenAttributeTable,
                                                          ((POPEN_ATTRIBUTE_ENTRY_V0) ThisEntry)->OatIndex );
                }

                TargetScb = ThisEntry->OatData->Overlay.Scb;

                //
                //  If there is no Scb it means that we don't have an entry in Open
                //  Attribute Table for this attribute.
                //

                if (TargetScb == NULL) {

                    NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
                }

                if (!NtfsLookupNtfsMcbEntry( &TargetScb->Mcb,
                                             LogRecord->TargetVcn,
                                             &TargetLcn,
                                             NULL,
                                             NULL,
                                             NULL,
                                             NULL,
                                             NULL ) ||

                    (TargetLcn == UNUSED_LCN)) {

                    DebugTrace( 0, Dbg, ("Clusters removed from page entry\n") );
                    continue;
                }

                //
                //  Check if we need to generate the usncachebias.
                //  Since we read log records fwd the usn offsets are also going to be
                //  monotonic - the 1st one we see will be the farthest back
                //

                if (FlagOn( TargetScb->ScbPersist, SCB_PERSIST_USN_JOURNAL ) &&
                    !GeneratedUsnBias) {

                    LONGLONG ClusterOffset;
                    LONGLONG FileOffset;

                    if (LogRecord->RedoLength > 0) {

                        ClusterOffset = BytesFromLogBlocks( LogRecord->ClusterBlockOffset );
                        FileOffset = LlBytesFromClusters( Vcb, LogRecord->TargetVcn ) + ClusterOffset;

                        ASSERT( FileOffset >= Vcb->UsnCacheBias );

                        Vcb->UsnCacheBias = FileOffset & ~(USN_JOURNAL_CACHE_BIAS - 1);
                        if (Vcb->UsnCacheBias != 0) {
                            Vcb->UsnCacheBias -= USN_JOURNAL_CACHE_BIAS;
                        }

#ifdef BENL_DBG
                        if (Vcb->UsnCacheBias != 0) {
                            KdPrint(( "Ntfs: vcb:0x%x restart cache bias: 0x%x\n", Vcb, Vcb->UsnCacheBias ));
                        }
#endif
                    }
                    GeneratedUsnBias = TRUE;
                }
            }

            //
            //  Point to the Redo Data and get its length.
            //

            Data = (PVOID)((PCHAR)LogRecord + LogRecord->RedoOffset);
            Length = LogRecord->RedoLength;

            //
            //  Shorten length by any Lcns which were deleted.
            //

            SavedLength = Length;

            for (i = (ULONG)LogRecord->LcnsToFollow; i != 0; i--) {

                ULONG AllocatedLength;
                ULONG VcnOffset;

                VcnOffset = BytesFromLogBlocks( LogRecord->ClusterBlockOffset ) + LogRecord->RecordOffset + LogRecord->AttributeOffset;

                //
                //  If the Vcn in question is allocated, we can just get out.
                //

                if (DirtyPage->LcnsForPage[((ULONG)LogRecord->TargetVcn) - ((ULONG)DirtyPage->Vcn) + i - 1] != 0) {
                    break;
                }

                //
                //  The only log records that update pages but have a length of zero
                //  are deleting things from Usa-protected structures.  If we hit such
                //  a log record and any Vcn has been deleted within the Usa structure,
                //  let us assume that the entire Usa structure has been deleted.  Change
                //  the SavedLength to be nonzero to cause us to skip this log record
                //  at the end of this for loop!
                //

                if (SavedLength == 0) {
                    SavedLength = 1;
                }

                //
                //  Calculate the allocated space left relative to the log record Vcn,
                //  after removing this unallocated Vcn.
                //

                AllocatedLength = BytesFromClusters( Vcb, i - 1 );

                //
                //  If the update described in this log record goes beyond the allocated
                //  space, then we will have to reduce the length.
                //

                if ((VcnOffset + Length) > AllocatedLength) {

                    //
                    //  If the specified update starts at or beyond the allocated length, then
                    //  we must set length to zero.
                    //

                    if (VcnOffset >= AllocatedLength) {

                        Length = 0;

                    //
                    //  Otherwise set the length to end exactly at the end of the previous
                    //  cluster.
                    //

                    } else {

                        Length = AllocatedLength - VcnOffset;
                    }
                }
            }

            //
            //  If the resulting Length from above is now zero, we can skip this log record.
            //

            if ((Length == 0) && (SavedLength != 0)) {
                continue;
            }

#ifdef BENL_DBG

            {
                PRESTART_LOG RedoLog;

                RedoLog = (PRESTART_LOG) NtfsAllocatePoolNoRaise( NonPagedPool, sizeof( RESTART_LOG ) );
                if (RedoLog) {
                    RedoLog->Lsn = LogRecordLsn;
                    InsertTailList( &(Vcb->RestartRedoHead), &(RedoLog->Links) );
                } else {
                    KdPrint(( "NTFS: out of memory during restart redo\n" ));
                }
            }
#endif

            //
            //  Apply the Redo operation in a common routine.
            //

            DoAction( IrpContext,
                      Vcb,
                      LogRecord,
                      LogRecord->RedoOperation,
                      Data,
                      Length,
                      LogRecordLength,
                      &LogRecordLsn,
                      NULL,
                      &PageBcb,
                      &PageLsn );


            if (PageLsn != NULL) {
                *PageLsn = LogRecordLsn;
            }

            if (PageBcb != NULL) {

                CcSetDirtyPinnedData( PageBcb, &LogRecordLsn );

                NtfsUnpinBcb( IrpContext, &PageBcb );
            }

        //
        //  Keep reading and looping back until end of file.
        //

        } while (LfsReadNextLogRecord( LogHandle,
                                       LogContext,
                                       &RecordType,
                                       &TransactionId,
                                       &UndoNextLsn,
                                       &PreviousLsn,
                                       &LogRecordLsn,
                                       &LogRecordLength,
                                       (PVOID *)&LogRecord ));

    } finally {

        NtfsUnpinBcb( IrpContext, &PageBcb );

        //
        //  Finally we can kill the log handle.
        //

        LfsTerminateLogQuery( LogHandle, LogContext );
    }

    DebugTrace( -1, Dbg, ("RedoPass -> VOID\n") );
}


//
//  Internal support routine
//

VOID
UndoPass (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine performs the Undo Pass of Restart.  It does this by scanning
    the Transaction Table produced by the Analysis Pass.  For every transaction
    in this table which is in the active state, all of its Undo log records, as
    linked together by the UndoNextLsn, are applied to undo the logged operation.
    Note that all pages at this point should be uptodate with the contents they
    had at about the time of the crash.  The dirty page table is not consulted
    during the Undo Pass, all relevant Undo operations are unconditionally
    performed.

    The Undo actions are all performed in the common routine DoAction,
    which is also used by the Redo Pass.

Arguments:

    Vcb - Volume which is being restarted.

Return Value:

    None.

--*/

{
    PTRANSACTION_ENTRY Transaction;
    POPEN_ATTRIBUTE_ENTRY OpenEntry;
    PRESTART_POINTERS TransactionTable = &Vcb->TransactionTable;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("UndoPass:\n") );

    //
    //  Point to first Transaction Entry.
    //

    Transaction = NtfsGetFirstRestartTable( TransactionTable );

    //
    //  Loop to end of table.
    //

    while (Transaction != NULL) {

        if ((Transaction->TransactionState == TransactionActive)

                &&

            (Transaction->UndoNextLsn.QuadPart != 0)) {

                //
                //  Abort transaction if it is active and has undo work to do.
                //

                NtfsAbortTransaction( IrpContext, Vcb, Transaction );

#ifdef ber='PropertiesChanged',path='/org/mate/panel/applet/ClockApplet/0',arg0='org.mate.panel.applet.Applet'                 a              `±~y/        ˜ˇˇ                                  êuÇy/                 !        áy/  @0uy/  X0uy/  !       1000   WÜy/          !               YÜy/   ‚áy/  !       :1.72   pÅy/          1       e9075976f7bf4e270359419c0000001a        @      òŒ(w/  Ä Üy/  ‡'ây/  Ä Üy/                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          !H      Äáy/  x«(w/                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  ÈS  .   ÁC  ..  ÃY  ndasdevid.h17E7kÀY  makefile…Y   _vs71_nhixnotify.vcproj  Y   _vs80_nhixnotify.vcproj ŒY  ndashixnotify.cppgW ÕY  	ndashix.h   “Y  
stdafx.cpp2ZX6Vt—Y  sources œY   ndashixnotifyutil.cpp   –Y 4 ndashixnotifyutil.h ”Y  .stdafx.h.gJpNv8”Y ‘stdafx.h                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    ËS  .   ÁC  ..  ?X  ndsccmd √Y  	ndscctl.c.u1UaWW¬Y 0 makefile≈Y   .sources.e9SsiK.Mp6ha9  ƒY  ndscctl.vcproj  ≈Y xsources                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          Ä      L D              H  
   ,   Ä    $ L D        P      H  
   ,   Ä    ( L D        à      H  
   ,   Ä    , L D        ¿      H  
   ,   Ä    0 L D        ¯      H  
   ,   Ä    4 L D        0      H  
   ,   Ä    8 L D        d      H  
   ,   Ä    < L D        ò      H  
   , 	  Ä    @ L D	        »      H  
   , 
  Ä    D L D
        ¸      H  
                           	   
      ,I  DI  \I  ÑI  ®I  ÃI  ÙI  J  8J  TJ  xJ      ,   X   Ñ   ∞   ‹     4  `  å  ∏  Ñ   ,    Ä     L D         h      H  
   ,   Ä      L D        ú      H  
   ,   Ä    $ T D        ÿ  X   H  
            ,I  DI  êH      ,   X   ‹   ,    Ä     L D         @      H  
   ,   Ä      L D        Ñ      H  
   ,   Ä    $ L D        –      H  
   ,   Ä    ( L D              H  
   ,   Ä    , L D        L  8   H  
                  ,I  DI  K  8K  `K      ,   X   Ñ   ∞   ,   ,    Ä    D T D         (  ÿ   H  
      LM        ,    Ä    , L D        h
  à   H  
   ,   Ä    0 D !         å
   Äˇˇˇˇ   ,   Ä    4 L D        ∞
  8   H  
   ,   Ä    8 D !        
   Äˇˇˇˇ   ,   Ä    < T 	D        î  ¿   H  
   ,   Ä    @ T 	D        –  ¿   H  
                     ,L  ,L  HL  HL  L  M      ,   X   Ñ   ∞   ‹   ∞   ,    Ä     L D         ¸      H  
   ,   Ä      L D        8	  h   H  
   ,   Ä    $ D !        `	  `   ˇˇˇˇ   ,   Ä    ( T 	D        
  Ä   H  
                 L  L  L  PH      ,   X   Ñ     8    Ä     \ D         p   ÄƒH     0   H  
   ,   Ä      L D        Ä  8   H  
   ,   Ä   $ L D        î  @   H  
   ,   Ä    ( L 	        d  ®   H     ,   Ä    , D 	        t   ÄƒH         Ä    0 4 	         Ñ         ¸ˇˇˇ         ¥H  ÿH  ÏH  ∏L  »L  ‹L      8   d   ê   º   Ë     ,    Ä     L D        T  ò   H  
   ,   Ä      D !         t  ê   ˇˇˇˇ   ,   Ä    $ L D        î  (   H  
   ,   Ä    ( D !        º   Äˇˇˇˇ   ,   Ä    , L D        ‰  (   H  
   ,   Ä    0 D !        $   Äˇˇˇˇ                       pH  pH  åL  åL  §L  §L      ,   X   Ñ   ∞   ‹   ê   8    Ä     \ D         p   ÄƒH     0   H  
   ,   Ä      L D        Ä  8   H  
   ,   Ä   $ L D        î  @   H  
          ¸ˇˇˇ¥H  ÿH  ÏH      8   d      ,    Ä    \ L D        p      H  
   ,   Ä    ` D !         î   Äˇˇˇˇ   ,   Ä    d T 	D           h  H  
   ,   Ä    h T 	D        Ï  †  H  
   ,   Ä    l T D        ê  ‡  H  
   ,   Ä    p L !        ∏  ÿ  ˇˇˇˇ   ,   Ä    t T 	D          ¯  H  
   ,   Ä    x T 	D        $  0  H  
   `   Ä    | º 	        X  ˇˇˇˇ\	     å   å Ä‰R      Ä¯R  1   8  S  1   @  (S  1   ` 	  Ä    Ä º 		        Ñ  ˇˇˇˇ\	    å  å Ä‰R      Ä¯R  1   H  PS  1   P  dS  1                                 0O  0O  åO  tP  R  R  HR  ¥R  ‘R  @S      ,   X   Ñ   ∞   ‹     4  `  ¿  ‘  ,    Ä     L D         §  8   H  
   ,   Ä      L D        Ã  (   H  
   ,   Ä    $ L D          (   H  
   ,   Ä    ( L D          (   H  
   ,   Ä    , L D        <  à   H  
   ,   Ä    0 L D        h  à   H  
   ,   Ä    4 L D        î  (   H  
   ,   Ä    8 L 	D        ¥      H  
   8   Ä    < \ 	D        ‰  ‡   îN     (   H  
   , 	  Ä    @ T 	D	           ¯   H  
   , 
  Ä    D T 	D
        |    H  
   ,   Ä    H T 	D        §  (  H  
   ,   Ä    L T 	D        ‘  @  H  
   @   Ä    P t 	T             åˇˇˇˇH  –N  1   (   H  
   ,   Ä    T D 	        (   Ä¸N         Ä    X 4 	         L                       
                           ºM  –M  ËM   N  N  4N  LN  dN  ÄN  ®N   I  úJ  ÿJ  ºN  ËN  O      ,   X   Ñ   ∞   ‹     4  `  ò  ƒ      H  ackPacket = PrepareSmpNonDataPacket( Connection, SMP_TYPE_ACK );

		if (ackPacket)
			LpxSendPacket( Connection, ackPacket, SMP_TYPE_ACK );

	} else if (NTOHS(lpxHeader->Lsctl) & LSCTL_DATA) {

		if (NTOHS(lpxHeader->Sequence) == SHORT_SEQNUM(Connection->LpxSmp.RemoteSequence)) {

			Connection->LpxSmp.RemoteSequence ++;

			ExInterlockedInsertTailList( &Connection->LpxSmp.RecvDataQueue,
										 &RESERVED(Packet)->ListEntry,
										 &Connection->LpxSmp.RecvDataQSpinLock );
		}

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

		LpxSmpProcessReceivePacket( Connection, NULL );

		ackPacket = PrepareSmpNonDataPacket( Connection, SMP_TYPE_ACK );

		if (ackPacket)
			LpxSendPacket( Connection, ackPacket, SMP_TYPE_ACK );

		return TRUE;

	} else {

		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
	}

	PacketFree( Connection->AddressFile->Address->Provider, Packet );
	return TRUE;
}


BOOLEAN 
LpxStateDoReceiveWhenClosing (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet,
	IN PLIST_ENTRY		FreePacketList
	) 
{
	PLPX_HEADER		lpxHeader;
	PNDIS_PACKET	ackPacket;


	lpxHeader	 = &RESERVED(Packet)->LpxHeader;
	
	ASSERT( Connection->LpxSmp.SmpState == SMP_STATE_CLOSING );
	
	DebugPrint( 2, ("SmpDoReceive SMP_STATE_CLOSING lpxHeader->Lsctl = %d\n", NTOHS(lpxHeader->Lsctl)) );

	if (NTOHS(lpxHeader->Lsctl) != (LSCTL_DISCONNREQ | LSCTL_ACK) &&
		NTOHS(lpxHeader->Lsctl) != (LSCTL_ACKREQ | LSCTL_ACK) &&
		NTOHS(lpxHeader->Lsctl) != LSCTL_ACK) {
		
		DebugPrint( 0, ("[LPX] SmpDoReceive/SMP_STATE_CLOSING:  Unexpected packet NTOHS(lpxHeader->Lsctl) = %x\n", NTOHS(lpxHeader->Lsctl)) );
	} 
	
	if (NTOHS(lpxHeader->Lsctl) & LSCTL_ACK) {
	
		if (((SHORT)(NTOHS(lpxHeader->AckSequence) - SHORT_SEQNUM(Connection->LpxSmp.RemoteAckSequence))) >= 0) {

			Connection->LpxSmp.RemoteAckSequence = NTOHS(lpxHeader->AckSequence);
			SmpRetransmitCheck( Connection, Connection->LpxSmp.RemoteAckSequence, FreePacketList );
		}

		if (SHORT_SEQNUM(Connection->LpxSmp.RemoteAckSequence) == SHORT_SEQNUM(Connection->LpxSmp.FinSequence)) {
		
			DebugPrint( 2, ("[LPX] SmpDoReceive/SMP_STATE_FIN_WAIT1: SMP_STATE_CLOSING to SMP_STATE_TIME_WAIT\n") );
			LpxChangeState( Connection, SMP_STATE_TIME_WAIT, TRUE );

			if (Connection->LpxSmp.DisconnectIrp == NULL) {

				LPX_ASSERT( FALSE );			
			
			} else {
			    
				PIRP	irp;
				KIRQL	cancelIrql;

				irp = Connection->LpxSmp.DisconnectIrp;
			    Connection->LpxSmp.DisconnectIrp = NULL;
			    
				IoAcquireCancelSpinLock( &cancelIrql );
			    IoSetCancelRoutine( irp, NULL );
				IoReleaseCancelSpinLock( cancelIrql );
			    
			    irp->IoStatus.Status = STATUS_SUCCESS;
				DebugPrint( 0, ("[LPX]LpxStateDoReceiveWhenClosing: Disconnect IRP %p completed.\n ", irp) );

				LpxIoCompleteRequest( irp, IO_NETWORK_INCREMENT );
			} 
		}
	} 

	if (NTOHS(lpxHeader->Lsctl) & LSCTL_ACKREQ) {
	
		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );

		ackPacket = PrepareSmpNonDataPacket( Connection, SMP_TYPE_ACK );

		if (ackPacket)
			LpxSendPacket( Connection, ackPacket, SMP_TYPE_ACK );
	
	} else {
	
		RELEASE_DPC_C_SPIN_LOCK( &Connection->SpinLock );
	}

	PacketFree( Connection->AddressFile->Address->Provider, Packet );		
	return TRUE;
}


BOOLEAN 
LpxStateDoReceiveWhenCloseWait (
	IN PTP_CONNECTION	Connection,
	IN PNDIS_PACKET		Packet,
	IN PLIST_ENTRY		FreePacketList
	) 
{
	PLPX_HEADER	lpxHeader;


	lpxHeader = &RESERVED(Packet)->LpxHeader;

	ASSERT( Connection->LpxSmp.SmpState == SMP_STATE_CLOSE_WAIT );

	if (NTOHS(lpxHeader->Lsctl) != (LSCTL_DISCONNREQ | LSCTL_ACK) &&
		NTOHS(lpxHeader->Lsctl) != (LSCTL_ACKREQ | LSCTL_ACK) &&
		NTOHS(lpxHeader->Lsctl) != LSCTL_ACK) {
		
		DebugPrint( 0, ("[LPX] SmpDoReceive/SMP_STATE_CLOSE_WAIT:  Unexpected packet NTOHS(lpxHeader->Lsctl) = %x\n", NTOHS(lpxHeader->Lsctl)) );
	} 

	if (NTOHS(lpxHeader->Lsctl) == LSCTL_ACK) {

		if (((SHORT)(NTOHS(lpxHeader->AckSequence) - SHORT_SEQNUM(Connection->LpxSmp.RemoteAckSequence))) >= 0) {
olumeDeviceObject->Secondary->Flags, SECONDARY_FLAG_RECONNECTING)) {
		
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

						Status = NtfsCommonDirectoryControl( IrpContext, Irp );
							
					} finally {

						ASSERT( ExIsResourceAcquiredSharedLite(&VolumeDeviceObject->Resource) );
						SecondaryReleaseResourceLite( NULL, &VolumeDeviceObject->Resource );
					}
				}

			} else
				Status = NtfsCommonDirectoryControl( IrpContext, Irp );
#else
            Status = NtfsCommonDirectoryControl( IrpContext, Irp );
#endif
            break;

        } except(NtfsExceptionFilter( IrpContext, GetExceptionInformation() )) {

            //
            //  We had some trouble trying to perform the requested
            //  operation, so we'll abort the I/O request with
            //  the error status that we get back from the
            //  execption code
            //

            Status = NtfsProcessException( IrpContext, Irp, GetExceptionCode() );
        }

    } while (Status == STATUS_CANT_WAIT ||
             Status == STATUS_LOG_FILE_FULL);

    ASSERT( IoGetTopLevelIrp() != (PIRP) &TopLevelContext );
    FsRtlExitFileSystem();

    //
    //  And return to our caller
    //

    DebugTrace( -1, Dbg, ("NtfsFsdDirectoryControl -> %08lx\n", Status) );

    return Status;
}


NTSTATUS
NtfsCommonDirectoryControl (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    )

/*++

Routine Description:

    This is the common routine for Directory Control called by both the fsd
    and fsp threads.

Arguments:

    Irp - Supplies the Irp to process

Return Value:

    NTSTATUS - The return status for the operation

--*/

{
    NTSTATUS Status;
    PIO_STACK_LOCATION IrpSp;
    PFILE_OBJECT FileObject;

    TYPE_OF_OPEN TypeOfOpen;
    PVCB Vcb;
    PSCB Scb;
    PCCB Ccb;
    PFCB Fcb;

    ASSERT_IRP_CONTEXT( IrpContext );=R  .   	Q  ..  ~Y  ndnetcomp.libfgO7qmK{Y  nddevice.lib|Y  ndfilter.lib}Y §	ndlog.lib   ~Y ê.ndnetcomp.lib.BlUsgv                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   we find anything.
            //
            //      - Current point to Zone start
            //      - Zone end to end of volume
            //      - Start of volume to current
            //

            if (NtfsScanBitmapRange( IrpContext,
                                     Vcb,
                                     Lcn,
                                     Vcb->MftZoneStart,
                                     NumberToFind,
                                     ReturnedLcn,
                                     ClusterCountFound )) {
                leave;
            }

            if (NtfsScanBitmapRange( IrpContext,
                                     Vcb,
                                     Vcb->MftZoneEnd,
                                     Vcb->TotalClusters,
                                     NumberToFind,
                                     ReturnedLcn,
                                     ClusterCountFound )) {

                leave;
            }

            if (NtfsScanBitmapRange( IrpContext,
                                     Vcb,
                                     0,
                                     Lcn,
                                     NumberToFind,
                                     ReturnedLcn,
                                     ClusterCountFound )) {

                leave;
            }

        //
        //  Check if we are beyond the Mft zone.
        //

        } else if (Lcn > Vcb->MftZoneEnd) {

            //
            //  Look in the following ranges.  Break out if we find anything.
            //
            //      - Current point to end of volume
            //      - Start of volume to Zone start
            //      - Zone end to current point.
            //

            if (NtfsScanBitmapRange( IrpContext,
                                     Vcb,
                                     Lcn,
                                     Vcb->TotalClusters,
                                     NumberToFind,
                                     ReturnedLcn,
                                     ClusterCountFound )) {
                leave;
            }

            if (NtfsScanBitmapRange( IrpContext,
                                     Vcb,
                                     0,
                                     Vcb->MftZoneStart,
                                     NumberToFind,
                                     ReturnedLcn,
                                     ClusterCountFound )) {
                leave;
            }

            if (NtfsScanBitmapRange( IrpContext,
                                     Vcb,
                                     Vcb->MftZoneEnd,
                                     Lcn,
                                     NumberToFind,
                                     ReturnedLcn,
                                     ClusterCountFound )) {
                leave;
            }

        //
        //  We are starting within the zone.  Skip over the zone to check it last.
        //

        } else {

            //
            //  Look in the following ranges.  Break out if we find anything.
            //
            //      - End of zone to end of volume
            //      - Start of volume to start of zone
            //

            if (NtfsScanBitmapRange( IrpContext,
                                     Vcb,
                                     Vcb->MftZoneEnd,
                                     Vcb->TotalClusters,
                                     NumberToFind,
                                     ReturnedLcn,
                                     ClusterCountFound )) {

                leave;
            }

            if (NtfsScanBitmapRange( IrpContext,
                                     Vcb,
                                     0,
                                     Vcb->MftZoneStart,
                                     NumberToFind,
                                     ReturnedLcn,
                                     ClusterCountFound )) {

                leave;
            }
        }

        //
        //  We didn't find anything.  Let's examine the zone explicitly.
        //

        if (NtfsScanBitmapRange( IrpContext,
                                 Vcb,
                                 Vcb->MftZoneStart,
                                 Vcb->MftZoneEnd,
                                 NumberToFind,
                                 ReturnedLcn,
                                 ClusterCountFound )) {

            AllocatedFromZone = TRUE;
            leave;
        }

        //
        //  No luck.
        //

        *ClusterCountFound = 0;

    } finally {

        DebugUnwind( NtfsFindFreeBitmapRun );

        if (StuffAdded) { NtfsFreePool( BitmapBuffer ); }

        NtfsUnpinBcb( IrpContext, &BitmapBcb );
    }

    DebugTrace( 0, Dbg, ("ReturnedLcn <- %016I64x\n", *ReturnedLcn) );
    DebugTrace( 0, Dbg, ("ClusterCountFound <- %016I64x\n", *ClusterCountFound) );
    DebugTrace( -1, Dbg, ("NtfsFindFreeBitmapRun -> VOID\n") );

    return AllocatedFromZone;
}


//
//  Local support routine
//

BOOLEAN
NtfsScanBitmapRange (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN LCN StartLcn,
    IN LCN BeyondLcn,
    IN LONGLONG NumberToFind,
    OUT PLCN ReturnedLcn,
    OUT PLONGLONG ClusterCountFound
    )

/*++

Routine Description:

    This routine will scan a range of the bitmap looking for a free run.
    It is called when we need to limit the bits we are willing to consider
    at a time, typically to skip over the Mft zone.

Arguments:

    Vcb - Volume being scanned.

    StartLcn - First Lcn in the bitmap to consider.

    BeyondLcn - First Lcn in the bitmap past the range we want to consider.

    NumberToFind - Supplies the number of clusters that we would
        really like to find

    ReturnedLcn - Start of free range if found.

    ClusterCountFound - Length of free range if found.

Return Value:

    BOOLEAN - TRUE if a bitmap range was found.  FALSE otherwise.

--*/

{
    BOOLEAN FreeRangeFound = FALSE;
    RTL_BITMAP Bitmap;
    PVOID BitmapBuffer;
    ULONG BitOffset;

    PBCB BitmapBcb = NULL;

    BOOLEAN StuffAdded = FALSE;
    LCN BaseLcn;

    RTL_BITMAP_RUN RunArray[16];
    ULONG RunArrayIndex;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("NtfsScanBitmapRange...\n") );

    //
    //  The end Lcn might be beyond the end of the bitmap.
    //

    if (BeyondLcn > Vcb->TotalClusters) {

        BeyondLcn = Vcb->TotalClusters;
    }

    //
    //  Use a try-finally to facilitate cleanup.
    //

    try {

        //
        //  Now search the rest of the bitmap starting with right after the mft zone
        //  followed by the mft zone (or the beginning of the disk).  Again take whatever
        //  we can get and not bother with the longest runs.
        //

        while (StartLcn < BeyondLcn) {

            NtfsUnpinBcb( IrpContext, &BitmapBcb );
            NtfsMapPageInBitmap( IrpContext, Vcb, StartLcn, &BaseLcn, &Bitmap, &BitmapBcb );

            StuffAdded = NtfsAddRecentlyDeallocated( Vcb, BaseLcn, &Bitmap );
            BitmapBuffer = Bitmap.Buffer;

            //
            //  Check if we don't want to use the entire page.
            //

            if ((BaseLcn + Bitmap.SizeOfBitMap) > BeyondLcn) {

                Bitmap.SizeOfBitMap = (ULONG) (BeyondLcn - BaseLcn);
            }

            //
            //  Now adjust the starting Lcn if not at the beginning
            //  of the bitmap page.  We know this will be a multiple
            //  of bytes since the MftZoneEnd is always on a ulong
            //  boundary in the bitmap.
            //

            if (BaseLcn != StartLcn) {

                BitOffset = (ULONG) (StartLcn - BaseLcn);

                Bitmap.SizeOfBitMap -= BitOffset;
                Bitmap.Buffer = Add2Ptr( Bitmap.Buffer, BitOffset / 8 );

                BaseLcn = StartLc.Oplock,
                                       Irp,
                                       IrpContext,
                                       NtfsOplockComplete,
                                       NtfsPrePostIrp );

            if (Status != STATUS_SUCCESS) {

                OplockPostIrp = TRUE;
                PostIrp = TRUE;
                try_return( NOTHING );
            }

            //
            //  This oplock call can affect whether fast IO is possible.
            //  We may have broken an oplock to no oplock held.  If the
            //  current state of the file is FastIoIsNotPossible then
            //  recheck the fast IO state.
            //

            if (Scb->Header.IsFastIoPossible == FastIoIsNotPossible) {

                NtfsAcquireFsrtlHeader( Scb );
                Scb->Header.IsFastIoPossible = NtfsIsFastIoPossible( Scb );
                NtfsReleaseFsrtlHeader( Scb );
            }

            //
            // We have to check for read access according to the current
            // state of the file locks.
            //

            if (!PagingIo
                && Scb->ScbType.Data.FileLock != NULL
                && !FsRtlCheckLockForReadAccess( Scb->ScbType.Data.FileLock,
                                                 Irp )) {

                try_return( Status = STATUS_FILE_LOCK_CONFLICT );
            }
        }

        //
        //  Now synchronize with the FsRtl Header
        //

        NtfsAcquireFsrtlHeader( (PSCB) Header );
        
        //
        //  Now see if we are reading beyond ValidDataLength.  We have to
        //  do it now so that our reads are not nooped.  We only need to block
        //  on nonrecursive I/O (cached or page fault to user section, because
        //  if it is paging I/O, we must be part of a reader or writer who is
        //  synchronized.
        //

        if ((ByteRange > Header->ValidDataLength.QuadPart) && !PagingIo) {

            //
            //  We must serialize with anyone else doing I/O at beyond
            //  ValidDataLength, and then remember if we need to declare
            //  when we are done.  If our caller has already serialized
            //  with EOF then there is nothing for us to do here.
            //

            if ((IrpContext->TopLevelIrpContext->CleanupStructure == Fcb) ||
                (IrpContext->TopLevelIrpContext->CleanupStructure == Scb)) {

                DoingIoAtEof = TRUE;

            } else {

                DoingIoAtEof = !FlagOn( Header->Flags, FSRTL_FLAG_EOF_ADVANCE_ACTIVE ) ||
                               NtfsWaitForIoAtEof( Header,
                                                   (PLARGE_INTEGER)&StartingVbo,
                                                   (ULONG)ByteCount );

                //
                //  Set the Flag if we are in fact beyond ValidDataLength.
                //

                if (DoingIoAtEof) {
                    SetFlag( Header->Flags, FSRTL_FLAG_EOF_ADVANCE_ACTIVE );
                    IrpContext->CleanupStructure = Scb;

#if (DBG || defined( NTFS_FREE_ASSERTS ))
                    ((PSCB) Header)->IoAtEofThread = (PERESOURCE_THREAD) ExGetCurrentResourceThread();

                } else {

                    ASSERT( ((PSCB) Header)->IoAtEofThread != (PERESOURCE_THREAD) ExGetCurrentResourceThread() );
#endif
                }
            }
        }

        //
        //  Get file sizes from the Scb.
        //
        //  We must get ValidDataLength first since it is always
        //  increased second (the case we are unprotected) and
        //  we don't want to capture ValidDataLength > FileSize.
        //

        ValidDataLength = Header->ValidDataLength.QuadPart;
        FileSize = Header->FileSize.QuadPart;

        NtfsReleaseFsrtlHeader( (PSCB) Header );
        
        //
        //  Optimize for the case where we are trying to fault in an entire
        //  compression unit, even if past the end of the file.//  Make sure we are serialized with the FileSizes, and
                        //  will remove this condition if we abort.
                        //

                        FsRtlLockFsRtlHeader( Header );
                        IrpContext->CleanupStructure = Scb;

                        NtfsCreateInternalAttributeStream( IrpContext, Scb, FALSE, NULL );

                        FsRtlUnlockFsRtlHeader( Header );
                        IrpContext->CleanupStructure = NULL;
                    }

                    FileObject = Scb->FileObject;
                }

                //
                //  Now check if the attribute has been deleted or if the
                //  volume has been dismounted.
                //

                if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED | SCB_STATE_VOLUME_DISMOUNTED)) {

                    if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED )) {
                        NtfsRaiseStatus( IrpContext, STATUS_FILE_DELETED, NULL, NULL );
                    } else {
                        NtfsRaiseStatus( IrpContext, STATUS_VOLUME_DISMOUNTED, NULL, NULL );
                    }
                }

            //
            //  If this is a paging I/O, and there is a paging I/O resource, then
            //  we acquire the main resource here.  Note that for most paging I/Os
            //  (like faulting for cached I/O), we already own the paging I/O resource,
            //  so we acquire nothing here!  But, for other cases like user-mapped files,
            //  we do check if paging I/O is acquired, and acquire the main resource if
            //  not.  The point is, we need some guarantee still that the file will not
            //  be truncated.
            //

            } else if ((Scb->Header.PagingIoResource != NULL) &&
                        !NtfsIsSharedScbPagingIo( Scb )) {

                //
                //  If we cannot acquire the resource, then raise.
                //

                if (!NtfsAcquireResourceShared( IrpContext, Scb, Wait )) {
                    NtfsRaiseStatus( IrpContext, STATUS_CANT_WAIT, NULL, NULL );
                }

                ScbAcquired = TRUE;

                //
                //  If this is async I/O save away the async resource.
                //

                if (!Wait && NonCachedIo) {
                    IrpContext->Union.NtfsIoContext->Wait.Async.Resource = Scb->Header.Resource;
                }


                //
                //  Now check if the attribute has been deleted or if the
                //  volume has been dismounted.
                //

                if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED | SCB_STATE_VOLUME_DISMOUNTED )) {
                    if (FlagOn( Scb->ScbState, SCB_STATE_ATTRIBUTE_DELETED )) {
                        NtfsRaiseStatus( IrpContext, STATUS_FILE_DELETED, NULL, NULL );
                    } else {
                        NtfsRaiseStatus( IrpContext, STATUS_VOLUME_DISMOUNTED, NULL, NULL );
                    }
                }
            }
        }

        //
        //  If the Scb is uninitialized, we initialize it now.
        //

        if (!FlagOn( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED )) {

            DebugTrace( 0, Dbg, ("Initializing Scb  ->  %08lx\n", Scb) );

            ReleaseScb = FALSE;

            if (AcquireScb && !ScbAcquired) {

                NtfsAcquireResourceShared( IrpContext, Scb, TRUE );
                ScbAcquired = TRUE;
                ReleaseScb = TRUE;
            }

            NtfsUpdateScbFromAttribute( IrpContext, Scb, NULL );

            if (ReleaseScb) {

                NtfsReleaseResource( IrpContext, Scb );
                ScbAcquired = FALSE;
            }
        }

        //
        //  We check whether we can proceed
        //  based on the state of the file oplocks.
        //

        if (TypeOfOpen == UserFileOpen) {

            Status = FsRtlCheckOplock( &Scb->ScbType.Data—gÏåÜ≥3∆w—0vG√ÿ£"E˝â˙b=ñXè#÷ôƒz<â:ãzWË	¸er9«•hó¢±\ä∆p)ÕÂhˇD£®àFQç¢°Œ§Öû@K=ë=ëùMäûLä˛íñz
-ÙÓ”Si†'S_gì§'ë§'í§'ê§≥®Ø«”@g“Pg“Pgí™'Û™û kz*i˙+“Ù◊ºÆßì¶göû…´z&ØËôt‘”Ë†ß—^M{˝ÌıT⁄Î)t–_ÚåûL™ûL™ûÃ =ùL=ãÒzYz6Ù&Ë≤t.Y:èÒz.„Ù\Í“uÙ˙ÎŸÙ◊≥ËØg0@O']OgêûŒ =ù|ùÀ=è=èùO©ûO©.†T/†D/dç^»jΩày∫Äπz>y:ü\=è\=ó<ù«\ùÀ<ùKæŒ%_Á≤OpR/‚î˛Ü”˙NÎ%ú÷K9≠9•óqR/„Ñ^∆^Ωî=z	ªıbvÎoÿ≠±[/dè^¿^]¿>]¿>]@%≥î+ÕrÆ2À©iVP”¨‰jSDMSÃUf5Wö’ƒf5ïÃ*îYEÖ^IÖ^AÖ^NÖ.DôBÃR*ô•T2Kibä∏ﬂ¨·≥ÜV¶ÑM)öµ¥2Îx¿îqøYOäYOc≥ñFf-çL)M	Õjöbô"n5E41E41Et6Îx›¨ß∑Ÿ@≥ë7Ã&ﬁ0Âºaæ•èŸLo≥ô4≥ÖgM9ùÃ&RÕF:öt4ÎI5e§öu<k÷—Ÿ¨£≥Y«SŒD≥ÖIfŸf+ŸfŸfŸf'ìÃˇ	Ç ≠Ê6å√˜y;Áø<ø!î$%)Àç"J—j˙RmîJcãêI£E©H(°§E••BEe-bàRdIëHÊªÆèmrˆ±M >∂≥ç6<€h•Ÿ+Õ>¥“Ï+Õ÷€lù=ò≠µŸZë≠µ˘Ÿ&{'˚ƒVeüÿÍÏS[ù}fk≤Õ∂:€b´≤-ˆNˆÖ≠Ãæ∞yŸÁ67˚‹ÊdüŸúÏSõì}bs≤èlnˆëÕÀ6Ÿ¸lìÕœ6Ÿ÷l≥˝í}iøf_⁄oŸV€ì}e{≤ØÌ∑¨¬~Õæ±_≤oÌÁÏ[˚2˚⁄∂d_ŸñÏ+€úmµÕŸ∂9€b[≤ÕˆE∂Ÿ∂fõmk∂ŸÚ\Ö’pﬂ⁄±nõ’tﬂŸqÓ{;Œm∑öná’t?ÿ±Ó´ÓvZU∑›R˜ΩUqﬂYŒm≥ú˚÷™∏o¨ä´∞™Æ¬Ú\ÖÂπ
k‰∂[∑”⁄∫ù÷ŒÌ≤vÓGkÁv[;˜≥µuøX˜ãµvøÿôÓ';√˝dÖÓG+pª¨¿Ì¥B˜Éù·vÿônª5r€≠ë€n›‹n‰~µ[›Ø6ÿ˝fÉ›Ï~∑¡nØ›ÍˆŸ ˜átXW˜ª]Î~∑.nèuqøY˜´uq?€µn∑uuª≠õ€m›‹nÂˆ⁄˜ß=Ì˛¥©nøMuŸ3ÓÄMu€”Óõ‚⁄SÓ†=ÏÿCÓ/·˛≤nøçpÿ∑œr{m§€k£‹^Âˆ⁄"w¿÷∏ÉˆÆ˚◊ﬁsˇ⁄˚Ó?{ﬂU⁄{NºÁﬁu	´]éÖÆ“∏ˇÏUw»Êªmæ;hÛ›?ˆ™˚€∏∂»∞EÓÄU∏J˚›ÂÿÎrÏuUÿÁRˆπ<ˆπ√ÿÎÁww8{‹·|Ì™Úï´ VóÚ•´¬ó.«Vóï_ªJ´pïV·*-Ûy‘Ú«˚å⁄ﬁQ€{j˚@m9ﬁµ<Á·p8ÃÚº'œ;Ú|Fû?å√|á˚<2üGÊÛhÏ#Ì}>˘|ä˝˚#πÿW£ÿ≈E˛h⁄˚chÁè·,_ç"$ç¸4Ú˘4Ú–»E>rñè4ˆë∆>“√W£ƒ√mæ:∑˚Í‹·èÂ_ì€˝q‹ÊkQ‚k1ÿ◊‚z_ìÓ˛X∫˚tÛ’ÈÊè°õ?öÓ˛(ÆÛ’Ë·´—√W£Ã◊‰<œ˙„yŒ◊fö?Åiæ.”¸â<ÁÎÒ¨Ø«T_èG}ÒuÂO‡a_õá˝ÒåÚµÂè„Q_ì2_ì2_ì%æ.k˝I¨Û'±ﬁ◊gΩ?ôıæÎ}C÷˘SXÎOÂ}*ã}^ÛX‰OfëØœ"_èE˛D^ÛuYÏÎ≤ƒ◊eâØÀ6ﬂê?¸i¸ÈOcø?ù˝æÄ˝æê˝˛˛ÙgÚá?ì}æﬂ˙BæÒT¯”©ßQ·O•¬ü¬7æ!ﬂ˙ÜlÛŸÊC!'ÑF‘	E‘E‘ç914°n8õ:·NÁP;úCMp°1Y8ã,ëÖFd·L\8
â°ê
iöpqh %°)¬π\Œ„“–ú·|:ÑÛπ$¥†8¥‡ú–å≥C3öÑÛhŒ•qhJ„p6M¬Ÿúö–44°ihBœ–ú;CKÓ
-.`H∏ê!°CBkÓ
m∏3¥Âé–ñC+n“#\»ı·Æ-ËŒÁÜ–úCszÜÊÙÕZ3=¥Â˘–é°=3¬EÃ≈ÃÛ|∏ÑÈ·¶Öå≈<.¢,¥ß,¥£,¥•,¥·±–ö—°5cBk∆Ñ÷,≈|:aË¿Üp)¬elŸÆ‡√p%Ñ+YÆdY∏ú◊√Â,	ó±$\ í–Å%·^≥4≥,≥,≥#t‰Ø–â°á´¯'\Õ?·˛ù9˛«_°˚C∂ák¯>\√w·j∂Ö´ÿ:±-\¡w·
æŸ:≤#t$?vÊƒx-ı‚µúªR?v£~ÏN˝x'≈Î©{pbÏ±;ªc7BÏJà]àÒƒÿbgÚcgÚcgö≈Ó\{pyºÅéÒ:∆ûtåΩË{syÏ√e±ó∆æú{qnÏI”x#M„4ç=hØÁ‹xÁ≈Ó4ã›iª”'ˆ‚Óÿó{b_Ü∆õoÊﬁÿü°ÒÓâ∏;`H@Ôÿè^±=„ÕÙå7—3ˆ•gÏCØÿõﬁ±}b/˙ƒ^åè˝y!‰≈8êó‚ f∆[ôÛR,·≈x/ƒ€ôoÁÒ8ò±q0c„≠åâÉ2&ﬁ¬ÿÿüq±?„c∆«˛,è%låw∞)ﬁ¡GÒN>éwÒq¬GÒn>ä˜∞)eCJy¬õÒ.ﬁàw±,ﬁ…≤x;oƒ€x#ñPKXKXKÿáp0Âﬂx/á‚Ωä˜s(„P|Äc)c)ˇƒRv∆a¸ÔgGºèÒ^vƒ°Ïà˜Cºõùqª‚v≈!T≥aúl√i`√ih“–F––F“–¶Åç‚dE}≈ëˆGÿC‰€ÚÌAÚm8˘V ˆ G⁄0™Ÿ0™Ÿ0Z⁄HÆ∞G∏“°ì=J'+„*{åN6ö+mWÿX:⁄XZÿc4∑«hfe4≥Gifè–Ã¶πç‰|IKIKI?ÕΩ6é˚l˜€„≥Ò≥	‹oOpøM‰>õ»PõƒÕ6Åõl<}Ìq˙ÿ8˙ÿX˙ÿ˙⁄hn≤—Ù≥—Ù≥—L¥	Ã≤IºlìyŸ&3€ûb∂Ma∂=ÕÀ6ïY6ïô6ï'l
Ï)∆€ì<nìy‹&1ﬁ&2¡û‡	õ¿Dõ¿Dõ¿
õ¬'ˆü⁄3|fœÚô=«g6ùœÏy>µ|b3¯ÿf∂M„-õ∆r{éÂˆ,ÀÌñ€”ºeOÛ∂MaÖMaÖMa∑MÁ?{ÅJ{Ò"‚%f"fQi/ÛüÕÊêÕÊ'õ…è6ì]ˆªÏEvŸvŸÛ¸h”˘…¶≥€¶≥€¶SùYú¬lNÂNcß3ó”ô«iÃÁT^Â^•!8Üy≈\éb’xÖjÃ¶/s≥8öYTg’ôE+Êq∏ö\√B:≥àŒ,¶3K∏Ü◊πö◊ÈƒÎ\»k\¿k¥d-XH–íWi…|.d≠òG+Ê1Ä≈<¿RJY pñ1ú7N9√YN)ÀyÄ∑∆[‹¬õÙÁM˙Ò˝XF?ñ“è%Ùg	∑∞ò,f ãyír^·mÊ6sY¡\V2èwòÀ*Ê∞öWX√l÷0ôwòƒJ&≤íâ¨`"o1ëÂL¢ú…îÛ$Â<I9´Y≈V≥ë’ldõxóMºœ&÷≤ëul`≤éU¨e%Ô≥í˜X¡ª¨`+XÕJVÒ´XÕ*V≥äµÃe?X…7¨e.©§º$Q*© ©¢TRAíS*iO"•í$ïïµ$ÃïUJí4.ëT*©hA˘“Öí$Ií$Ií$IíîJ*H§TRaRC©§\í®™§,')ó/©»˝ÈªYû§‚æïÙÆ£ˆj£vj†VJ%&R*© IïJ⁄ìHµ$ÃïUJí4.ëT*©hA˘“Öí$Ií$Ií$IíîJ*H§TRaRC©§\í®™§,')ó/©»Â≈Æñ'©∏Ô¿A%ΩÎ®Ω⁄®ù®ïRIÖâîJ*HR•íˆ$R-Iseïí$çK$ïJ*ZPæt°$Ií$Ií$Ií$•í
)ïTò‘P*)ó$™*)ÀI ÂK*rábWÀìT‹w‡†íﬁu‘^m‘N‘J©§¬DJ%$©RI{©ñ§Éπ≤JIí∆%íJ%-(_∫Pí$Ií$Ií$IíRIâîJ*Lj(ïîKUïîÂ$ÂÚ%πo¨´ÂI*Ó;pPIÔ:jØ6jßj•TRa"•í
íT©§=âTK“¡\Y•$I„I•íäî/](Ií$Ií$Ií$I©§ÇDJ%&5îJ %â™J rír˘íä‹Át≥<I≈}*È]GÌ’FÌ‘@≠îJ*L§TRAí*ï¥'ëjI:ò+´î$©<'©TR—ÇÚ•%Ií$Ií$Ií$)ïTêH©§¬§ÜRIπ$QUIYNRIEÆW˛I™*©uáÀ;µÌ}ï.–µ∫X)ïTòH©§Ç$U*iO"Uë$Ií$Ií$Ií$Ií$Ií$Ií$Ií$Ií$Ií$Ií$Ií$Ií$Ií$Ií$Ií$Ií$Ií$Ií$Ií˛_     Ò_                                  @,Jˆ                                                                                                                                                                                                                                                                                                                                                                                                       †Øk    `Øk    `∞k    Øk    ÄØk     Æk    @±k             ùk                            @∞k     ∞k    ¿∞k              k                           ¥k                            †∞k    `∞k     ±k            `¡k                            ∞¥k                             ±k    ¿∞k     Æk            ‡∞k     ∞k    †±k            Ä∞k                            √k                            Ä±k    @±k    @ k                                           –Àk    –Øk            a       /usr/share/pixmaps/mono-runtime.png ^  xøû^                                  `       Q       /home/auser/.icons/ubuntu-mono-light                                    1       /home/auser/.icons/hicolor  ns/         !       xawtv32x32             Q       /home/auser/.local/share/icons/ubuntu-mono-light                        Q       /usr/share/icons/cab_extract.png                                P       Q       /home/auser/.icons/gnome-colors-wise                                    !       thunderbird             Q       /usr/share/mdm/pixmaps/ubuntu-mono-light                                Q       /usr/share/pixmaps/mdm-foot-logo.png                            P       Q       /usr/share/icons/ubuntu-mono-light                                      Q       `yP    pQ    @1Q    @yP    `1Q    ∞1Q    ∞¡k    –¡k    ¡k    !       standard::edit-name     !       access::can-read ˚T    1                                             Q       /home/auser/.local/share/icons/hicolor  .theme theme e e eme            Q       /usr/share/icons/gnome-colors-wise                                      Q       /usr/share/default/icons/hicolor                                        Q       /home/auser/.local/share/icons/gnome-colors-wise                        Q       /usr/share/pixmaps/gksuexec-debian.xpm  xøû^  @                       !       pstree32 øû^          Q       /usr/share/default/pixmaps/hicolor                                      !       0•0†^                  !       `•0†^                                                 …v     Íu     …v    ,A     ÄA             †-A     Ä)x^           πk                    ∞Bp    Ä⁄u    @v    ,A     ÄA            †-A     Ä)x^           ∏k                    ‡v    0v    †v    ,A     ÄA            †-A     Ä)x^          Äπk                    @Bp    n    @Bp    ,A     ÄA             †-A     Ä)x^          @∫k                    0v    @v    0v    ,A     ÄA             †-A     Ä)x^          ¿∏k                     v    –v     v    ,A     ÄA             †-A     Ä)x^          ‡πk                    v    @v    v    ,A     ÄA             †-A     Ä)x^          ∞t;    ZŒF                 ÄDp    Ä†m    êáH                      ˇˇˇˇ                `hn                    ˇ     PiÔû^                  ˇ      PiÔû^                 ˇ                           `Çn    –}H    `Çn    ,A     ÄA            †-A     †-A                             `∏k           –˚m    –ãV            Q       /usr/share/icons/mate/16x16/emblems     ns,16x16/apps,16x16/categories,1Q           Ö               ºk    †ºk    4   pes,@ﬁl    aces,16x        !       16x16/emblems   imations!       16x16/emotes   ries,22x!       16x16/emotes   ,22x22/eQ       /usr/share/icons/gnome/16x16/emotes     24x24/actions,24x24/apps,24x24/cQ           á               Ωk    ¿ºk       4x24 m    es,24x24        Q       /usr/share/icons/gnome/16x16/emotes     2/apps,32x32/categories,32x32/deQ           á              †Ωk    ‡ºk       32/p¿m    x32/stat        Q       /usr/share/icons/hicolor/16x16/emotes   a      xøû^  xøû^  8/emblemQ           á              @æk    ‡æk    ˇˇˇˇatusPTq    /actions        !       16x16/emotes   ,256x256Q       /usr/share/icons/hicolor/16x16/emotes   °       xøû^  xøû^  ,256x256!       pøk    @ÚU    ê¿k    1       preview::* û^  ¿k    ¿ûk    0       !       standard::is-hidden scal!                             !       id û^  xøû^          Q          //

    if (ARGUMENT_PRESENT( FileReference )) {

        if ((FileReference->SequenceNumber != FileRecord->SequenceNumber) ||
            ((FileRecord->FirstAttributeOffset > BytesInOldHeader) &&
             ((FileRecord->SegmentNumberHighPart != FileReference->SegmentNumberHighPart) ||
              (FileRecord->SegmentNumberLowPart != FileReference->SegmentNumberLowPart)))) {

            *CorruptionHint = 3;
            ASSERTMSG( "Filerecord fileref doesn't match expected value\n", FALSE );
            return FALSE;
        }
    }

    //
    //  Loop to check all of the attributes.
    //

    for (Attribute = NtfsFirstAttribute(FileRecord);
         Attribute->TypeCode != $END;
         Attribute = NtfsGetNextRecord(Attribute)) {

//      if (!StandardInformationSeen &&
//          (Attribute->TypeCode != $STANDARD_INFORMATION) &&
//          XxEqlZero(FileRecord->BaseFileRecordSegment)) {
//
//          DebugTrace( 0, 0, ("Standard Information missing: %08lx\n", Attribute) );
//
//          ASSERTMSG( "Standard Information missing\n", FALSE );
//          return FALSE;
//      }

        StandardInformationSeen = TRUE;

        if (!NtfsCheckAttributeRecord( Vcb,
                                       FileRecord,
                                       Attribute,
                                       FALSE,
                                       CorruptionHint )) {

            return FALSE;
        }
    }
    return TRUE;
}


BOOLEAN
NtfsCheckAttributeRecord (
    IN PVCB Vcb,
    IN PFILE_RECORD_SEGMENT_HEADER FileRecord,
    IN PATTRIBUTE_RECORD_HEADER Attribute,
    IN ULONG CheckHeaderOnly,
    IN PULONG CorruptionHint
    )

{
    PVOID NextAttribute;
    PVOID EndOfFileRecord;
    PVOID FirstFreeByte;
    PVOID Data;
    ULONG Length;
    ULONG BytesPerFileRecordSegment = Vcb->BytesPerFileRecordSegment;

    PAGED_CODE();

    EndOfFileRecord = Add2Ptr( FileRecord, BytesPerFileRecordSegment );
    FirstFreeByte = Add2Ptr( FileRecord, FileRecord->FirstFreeByte );

    //
    //  Do an alignment check before creating a ptr based on this value
    //

    if (!IsQuadAligned( Attribute->RecordLength )) {

        *CorruptionHint = Attribute->TypeCode + 0xc;
        ASSERTMSG( "Misaligned attribute length\n", FALSE );
        return FALSE;
    }

    NextAttribute = NtfsGetNextRecord(Attribute);

    //
    //  Check the fixed part of the attribute record header.
    //

    if ((Attribute->RecordLength >= BytesPerFileRecordSegment)

            ||

        (NextAttribute >= EndOfFileRecord)

            ||

        (FlagOn(Attribute->NameOffset, 1) != 0)

            ||

        ((Attribute->NameLength != 0) &&
         (((ULONG)Attribute->NameOffset + (ULONG)Attribute->NameLength) >
           Attribute->RecordLength))) {

        DebugTrace( 0, 0, ("Invalid attribute record header: %08lx\n", Attribute) );

        *CorruptionHint = Attribute->TypeCode + 1;
        ASSERTMSG( "Invalid attribute record header\n", FALSE );
        return FALSE;
    }

    if (NextAttribute > FirstFreeByte) {
        *CorruptionHint = Attribute->TypeCode + 2;
        ASSERTMSG( "Attributes beyond first free byte\n", FALSE );
        return FALSE;
    }

    //
    //  Check the resident attribute fields.
    //

    if (Attribute->FormCode == RESIDENT_FORM) {

        if ((Attribute->Form.Resident.ValueLength >= Attribute->RecordLength) ||

            (((ULONG)Attribute->Form.Resident.ValueOffset +
              Attribute->Form.Resident.ValueLength) > Attribute->RecordLength) ||

            (!IsQuadAligned( Attribute->Form.Resident.ValueOffset ))) {

            DebugTrace( 0, 0, ("Invalid resident attribute record header: %08lx\n", Attribute) );

            *CorruptionHint = Attribute->TypeCode + 3;
            ASSERTMSG( "Invalid resident attribute record header\n", FALSE );
            return FALSE;
        }

    //
    //  Check the nonresident attribute fields
    //

    } e/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    CheckSup.c

Abstract:

    This module implements check routines for Ntfs structures.

Author:

    Tom Miller      [TomM]          14-4-92

Revision History:

--*/

#include "NtfsProc.h"

//
//  Array for log records which require a target attribute.
//  A TRUE indicates that the corresponding restart operation
//  requires a target attribute.
//

BOOLEAN TargetAttributeRequired[] = {FALSE, FALSE, TRUE, TRUE,
                                     TRUE, TRUE, TRUE, TRUE,
                                     TRUE, TRUE, FALSE, TRUE,
                                     TRUE, TRUE, TRUE, TRUE,
                                     TRUE, TRUE, TRUE, TRUE,
                                     TRUE, TRUE, TRUE, TRUE,
                                     FALSE, FALSE, FALSE, FALSE,
                                     TRUE, FALSE, FALSE, FALSE,
                                     FALSE, TRUE, TRUE };

//
//  Local procedure prototypes
//

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtfsCheckAttributeRecord)
#pragma alloc_text(PAGE, NtfsCheckFileRecord)
#pragma alloc_text(PAGE, NtfsCheckIndexBuffer)
#pragma alloc_text(PAGE, NtfsCheckIndexHeader)
#pragma alloc_text(PAGE, NtfsCheckIndexRoot)
#pragma alloc_text(PAGE, NtfsCheckLogRecord)
#pragma alloc_text(PAGE, NtfsCheckRestartTable)
#endif


BOOLEAN
NtfsCheckFileRecord (
    IN PVCB Vcb,
    IN PFILE_RECORD_SEGMENT_HEADER FileRecord,
    IN PFILE_REFERENCE FileReference OPTIONAL,
    OUT PULONG CorruptionHint
    )

/*++

Routine Description:

    Consistency check for file records.

Arguments:

    Vcb - the vcb it belongs to

    FileRecord - the filerecord to check

    FileReference - if specified double check the sequence number and self ref.
        fileref against it

    CorruptionHint - hint for debugging on where corruption occured;

Return Value:

    FALSE - if the file record is not valid
    TRUE - if it is

--*/
{
    PATTRIBUTE_RECORD_HEADER Attribute;
    PFILE_RECORD_SEGMENT_HEADER EndOfFileRecord;
    ULONG BytesPerFileRecordSegment = Vcb->BytesPerFileRecordSegment;
    BOOLEAN StandardInformationSeen = FALSE;
    ULONG BytesInOldHeader;

    PAGED_CODE();

    *CorruptionHint = 0;

    EndOfFileRecord = Add2Ptr( FileRecord, BytesPerFileRecordSegment );

    //
    //  Check the file record header for consistency.
    //

    if ((*(PULONG)FileRecord->MultiSectorHeader.Signature != *(PULONG)FileSignature)

            ||

        ((ULONG)FileRecord->MultiSectorHeader.UpdateSequenceArrayOffset >
         (SEQUENCE_NUMBER_STRIDE -
          (PAGE_SIZE / SEQUENCE_NUMBER_STRIDE + 1) * sizeof(USHORT)))

            ||

        ((ULONG)((FileRecord->MultiSectorHeader.UpdateSequenceArraySize - 1) * SEQUENCE_NUMBER_STRIDE) !=
         BytesPerFileRecordSegment)

            ||

        !FlagOn(FileRecord->Flags, FILE_RECORD_SEGMENT_IN_USE)) {

        DebugTrace( 0, 0, ("Invalid file record: %08lx\n", FileRecord) );

        *CorruptionHint = 1;
        ASSERTMSG( "Invalid resident file record\n", FALSE );
        return FALSE;
    }

    BytesInOldHeader = QuadAlign( sizeof( FILE_RECORD_SEGMENT_HEADER_V0 ) + (UpdateSequenceArraySize( BytesPerFileRecordSegment ) - 1) * sizeof( USHORT ));

    //
    //  Offset bounds checks
    //

    if ((FileRecord->FirstFreeByte > BytesPerFileRecordSegment) ||
        (FileRecord->FirstFreeByte < BytesInOldHeader) ||

        (FileRecord->BytesAvailable != BytesPerFileRecordSegment) ||

        (((ULONG)FileRecord->FirstAttributeOffset < BytesInOldHeader)   ||
         ((ULONG)FileRecord->FirstAttributeOffset >
                 BytesPerFileRecordSegment - SIZEOF_RESIDENT_ATTRIBUTE_HEADER)) ||

        (!IsQuadAligned( FileRecord->FirstAttributeOffset ))) {

        *CorruptionHint = 2;
        ASSERTMSG( "Out of bound offset in frs\n", FALSE );
        return FALSE;
    }

    //
    //  Optional fileref number check
 
            Vcb->VcbState |= VCB_STATE_FLAG_LOCKED;
            Vcb->FileObjectWithVcbLocked = FileObject;
            UnwindVolumeLock = TRUE;

            //
            //  Clean the volume
            //

            CleanedVolume = TRUE;

        }  else if (FlagOn( *DesiredAccess, FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA )) {

            //
            //  Flush the volume and let ourselves push the clean bit out if everything
            //  worked.
            //

            if (NT_SUCCESS( FatFlushVolume( IrpContext, Vcb, Flush ))) {

                CleanedVolume = TRUE;
            }
        }

        //
        //  Clean the volume if we believe it safe and reasonable.
        //

        if (CleanedVolume &&
            FlagOn( Vcb->VcbState, VCB_STATE_FLAG_VOLUME_DIRTY ) &&
            !FlagOn( Vcb->VcbState, VCB_STATE_FLAG_MOUNTED_DIRTY ) &&
            !CcIsThereDirtyData(Vcb->Vpb)) {

            //
            //  Cancel any pending clean volumes.
            //

            (VOID)KeCancelTimer( &Vcb->CleanVolumeTimer );
            (VOID)KeRemoveQueueDpc( &Vcb->CleanVolumeDpc );

            FatMarkVolume( IrpContext, Vcb, VolumeClean );
            ClearFlag( Vcb->VcbState, VCB_STATE_FLAG_VOLUME_DIRTY );

            //
            //  Unlock the volume if it is removable.
            //

            if (FlagOn(Vcb->VcbState, VCB_STATE_FLAG_REMOVABLE_MEDIA) &&
                !FlagOn(Vcb->VcbState, VCB_STATE_FLAG_BOOT_OR_PAGING_FILE)) {

                FatToggleMediaEjectDisable( IrpContext, Vcb, FALSE );
            }
        }

        //
        //  If the volume is already opened by someone then we need to check
        //  the share access
        //

        if (Vcb->DirectAccessOpenCount > 0) {

            if (!NT_SUCCESS(Iosb.Status = IoCheckShareAccess( *DesiredAccess,
                                                              ShareAccess,
                                                              FileObject,
                                                              &Vcb->ShareAccess,
                                                              TRUE ))) {

                try_return( Iosb.Status );
            }

        } else {

            IoSetShareAccess( *DesiredAccess,
                              ShareAccess,
                              FileObject,
                              &Vcb->ShareAccess );
        }

        UnwindShareAccess = TRUE;

        //
        //  Set up the context and section object pointers, and update
        //  our reference counts
        //

        FatSetFileObject( FileObject,
                          UserVolumeOpen,
                          Vcb,
                          UnwindCcb = FatCreateCcb( IrpContext ));

        FileObject->SectionObjectPointer = &Vcb->SectionObjectPointers;

        Vcb->DirectAccessOpenCount += 1;
        Vcb->OpenFileCount += 1;
        if (IsFileObjectReadOnly(FileObject)) { Vcb->ReadOnlyCount += 1; }
        UnwindCounts = TRUE;
        FileObject->Flags |= FO_NO_INTERMEDIATE_BUFFERING;

        //
        //  At this point the open will succeed, so check if the user is getting explicit access
        //  to the device.  If not, we will note this so we can deny modifying FSCTL to it.
        //

        IrpSp = IoGetCurrentIrpStackLocation( IrpContext->OriginatingIrp );
        Status = FatExplicitDeviceAccessGranted( IrpContext,
                                                 Vcb->Vpb->RealDevice,
                                                 IrpSp->Parameters.Create.SecurityContext->AccessState,
                                                 (KPROCESSOR_MODE)( FlagOn( IrpSp->Flags, SL_FORCE_ACCESS_CHECK ) ?
                                                                    UserMode :
                                                                    IrpContext->OriginatingIrp->RequestorMode ));

        if (NT_SUCCESS( Status )) {

            SetFlag( UnwindCcb->Flags, CCBBENL_DBG
                {
                    PRESTART_LOG UndoLog;

                    UndoLog = (PRESTART_LOG) NtfsAllocatePoolNoRaise( NonPagedPool, sizeof( RESTART_LOG ) );
                    if (UndoLog) {
                        UndoLog->Lsn = Transaction->FirstLsn;
                        InsertTailList( &(Vcb->RestartUndoHead), &(UndoLog->Links) );
                    } else {
                        KdPrint(( "NTFS: out of memory during restart undo\n" ));
                    }
                }
#endif


        //
        //  Remove this entry from the transaction table.
        //

        } else {

            TRANSACTION_ID TransactionId = GetIndexFromRestartEntry( &Vcb->TransactionTable,
                                                                     Transaction );

            NtfsAcquireExclusiveRestartTable( &Vcb->TransactionTable,
                                              TRUE );

            NtfsFreeRestartTableIndex( &Vcb->TransactionTable,
                                       TransactionId );

            NtfsReleaseRestartTable( &Vcb->TransactionTable );
        }

        //
        //  Point to next entry in table, or NULL.
        //

        Transaction = NtfsGetNextRestartTable( TransactionTable, Transaction );
    }

    //
    //  Now we will flush and purge all the streams to verify that the purges
    //  will work.
    //

    OpenEntry = NtfsGetFirstRestartTable( &Vcb->OpenAttributeTable );

    //
    //  Loop to end of table.
    //

    while (OpenEntry != NULL) {

        IO_STATUS_BLOCK IoStatus;
        PSCB Scb;

        Scb = OpenEntry->OatData->Overlay.Scb;

        //
        //  We clean up the Scb only if it exists and this is index in the
        //  OpenAttributeTable that this Scb actually refers to.
        //  If this Scb has several entries in the table, this check will insure
        //  that it only gets cleaned up once.
        //

        if ((Scb != NULL) &&
            (Scb->NonpagedScb->OpenAttributeTableIndex == GetIndexFromRestartEntry( &Vcb->OpenAttributeTable, OpenEntry))) {

            //
            //  Now flush the file.  It is important to call the
            //  same routine the Lazy Writer calls, so that write.c
            //  will not decide to update file size for the attribute,
            //  since we really are working here with the wrong size.
            //
            //  We also now purge all pages, in case we go to update
            //  half of a page that was clean and read in as zeros in
            //  the Redo Pass.
            //

            NtfsPurgeFileRecordCache( IrpContext );

            NtfsAcquireScbForLazyWrite( (PVOID)Scb, TRUE );
            CcFlushCache( &Scb->NonpagedScb->SegmentObject, NULL, 0, &IoStatus );
            NtfsReleaseScbFromLazyWrite( (PVOID)Scb );

            NtfsNormalizeAndCleanupTransaction( IrpContext,
                                                &IoStatus.Status,
                                                TRUE,
                                                STATUS_UNEXPECTED_IO_ERROR );

            if (!CcPurgeCacheSection( &Scb->NonpagedScb->SegmentObject, NULL, 0, FALSE )) {

                KdPrint(("NtfsUndoPass:  Unable to purge volume\n"));

                NtfsRaiseStatus( IrpContext, STATUS_INTERNAL_ERROR, NULL, NULL );
            }
        }

        //
        //  Point to next entry in table, or NULL.
        //

        OpenEntry = NtfsGetNextRestartTable( &Vcb->OpenAttributeTable,
                                             OpenEntry );
    }

    DebugTrace( -1, Dbg, ("UndoPass -> VOID\n") );
}


//
//  Internal support routine
//

//
//  First define some "local" macros for Lsn in page manipulation.
//

//
//  Macro to check the Lsn and break (out of the switch statement in DoAction)
//  if the respective redo record need not be applied.  Note that if the structure's
//  clusters were deleted, then it will read as all zero's so we also check a field
//  which must be nonzero.
//

#define CheckLsn(PAGE) {                                                            \
    if (*(PULONG)((PMULTI_SECTOR_HEADER)(PAGE))->Signature ==                       \
        *(PULONG)BaadSignature) {                                                   \
        NtfsMarkVolumeDirty( IrpContext, Vcb, TRUE );                               \
        NtfsUnpinBcb( IrpContext, Bcb );                                            \
        break;                                                                      \
    }                                                                               \
                                                                                    \
    if (ARGUMENT_PRESENT(RedoLsn) &&                                                \
        ((*(PULONG)((PMULTI_SECTOR_HEADER)(PAGE))->Signature ==                     \
        *(PULONG)HoleSignature) ||                                                  \
        (RedoLsn->QuadPart <= ((PFILE_RECORD_SEGMENT_HEADER)(PAGE))->Lsn.QuadPart))) {  \
                 /**** xxLeq(*RedoLsn,((PFILE_RECORD_SEGMENT_HEADER)(PAGE))->Lsn) ****/ \
        DebugTrace( 0, Dbg, ("Skipping Page with Lsn: %016I64x\n",                    \
                             ((PFILE_RECORD_SEGMENT_HEADER)(PAGE))->Lsn) );         \
                                                                                    \
        NtfsUnpinBcb( IrpContext, Bcb );                                            \
        break;                                                                      \
    }                                                                               \
}

//
//  Macros for checking File Records and Index Buffers before and after the action
//  routines.  The after checks are only for debug.  The before check is not
//  always possible.
//

#define CheckFileRecordBefore {                                        \
    if (!NtfsCheckFileRecord( Vcb, FileRecord, NULL, &CorruptHint )) { \
        NtfsMarkVolumeDirty( IrpContext, Vcb, TRUE );                  \
        NtfsUnpinBcb( IrpContext, Bcb );                               \
        break;                                                         \
    }                                                                  \
}

#define CheckFileRecordAfter {                                            \
    DbgDoit(NtfsCheckFileRecord( Vcb, FileRecord, NULL, &CorruptHint ));  \
}

#define CheckIndexBufferBefore {                                    \
    if (!NtfsCheckIndexBuffer( Scb, IndexBuffer )) {                \
        NtfsMarkVolumeDirty( IrpContext, Vcb, TRUE );               \
        NtfsUnpinBcb( IrpContext, Bcb );                            \
        break;                                                      \
    }                                                               \
}

#define CheckIndexBufferAfter {                                     \
    DbgDoit(NtfsCheckIndexBuffer( Scb, IndexBuffer ));              \
}

//
//  Checks if the record offset + length will fit into a file record.
//

#define CheckWriteFileRecord {                                                  \
    if (LogRecord->RecordOffset + Length > Vcb->BytesPerFileRecordSegment) {    \
        NtfsMarkVolumeDirty( IrpContext, Vcb, TRUE ) ;                          \
        NtfsUnpinBcb( IrpContext, Bcb );                                        \
        break;                                                                  \
    }                                                                           \
}

//
//  Checks if the record offset in the log record points to an attribute.
//

#define CheckIfAttribute( ENDOK ) {                                             \
    _Length = FileRecord->FirstAttributeOffset;                                 \
    _AttrHeader = Add2Ptr( FileRecord, _Length );                               \
    while (_Length < LogRecord->RecordOffset) {                                 \
        if ((_AttrHeader->TypeCode == $END) ||                                  \
            (_AttrHeader->RecordLength == 0)) {                                 \
            break;                                                              \
        }                                                                       \
        _Length += _AttrHeader->RecordLength;                                   \
        _AttrHeader = NtfsGetNextRecord( _AttrHeader );                         \
    }                                                                           \
    if ((_Length != LogRecord->RecordOffset) ||                                 \
        (!(ENDOK) && (_AttrHeader->TypeCode == $END))) {                        \
        NtfsMarkVolumeDirty( IrpContext, Vcb, TRUE );                           \
        NtfsUnpinBcb( IrpContext, Bcb );                                        \
        break;                                                                  \
    }                                                                           \
}

//
//  Checks if the attribute described by 'Data' fits within the log record
//  and will fit in the file record.
//

#define CheckInsertAttribute {                                                  \
    _AttrHeader = (PATTRIBUTE_RECORD_HEADER) Data;                              \
    if ((Length < (ULONG) SIZEOF_RESIDENT_ATTRIBUTE_HEADER) ||                  \
        (_AttrHeader->RecordLength & 7) ||                                      \
        ((ULONG_PTR) Add2Ptr( Data, _AttrHeader->RecordLength )                 \
           > (ULONG_PTR) Add2Ptr( LogRecord, LogRecordLength )) ||              \
        (Length > FileRecord->BytesAvailable - FileRecord->FirstFreeByte)) {    \
        NtfsMarkVolumeDirty( IrpContext, Vcb, TRUE );                           \
        NtfsUnpinBcb( IrpContext, Bcb );                                        \
        break;                                                                  \
    }                                                                           \
}

//
//  This checks
//      - the attribute fits if we are growing the attribute
//

#define CheckResidentFits {                                                         \
    _AttrHeader = (PATTRIBUTE_RECORD_HEADER) Add2Ptr( FileRecord, LogRecord->RecordOffset ); \
    _Length = LogRecord->AttributeOffset + Length;                                  \
    if ((LogRecord->RedoLength == LogRecord->UndoLength) ?                          \
        (LogRecord->AttributeOffset + Length > _AttrHeader->RecordLength) :         \
        ((_Length > _AttrHeader->RecordLength) &&                                   \
         ((_Length - _AttrHeader->RecordLength) >                                   \
          (FileRecord->BytesAvailable - FileRecord->FirstFreeByte)))) {             \
        NtfsMarkVolumeDirty( IrpContext, Vcb, TRUE );                               \
        NtfsUnpinBcb( IrpContext, Bcb );                                            \
        break;                                                                      \
    }                                                                               \
}

//
//  This routine checks that the data in this log record will fit into the
//  allocation described in the log record.
//

#define CheckNonResidentFits {                                                  \
    if (BytesFromClusters( Vcb, LogRecord->LcnsToFollow )                       \
        < (BytesFromLogBlocks( LogRecord->ClusterBlockOffset ) + LogRecord->RecordOffset + Length)) { \
        NtfsMarkVolumeDirty( IrpContext, Vcb, TRUE );                           \
        NtfsUnpinBcb( IrpContext, Bcb );                                        \
        break;                                                                  \
    }                                                                           \
}

//
//  This routine checks
//      - the attribute is non-resident.
//      - the data is beyond the mapping pairs offset.
//      - the new data begins within the current size of the attribute.
//      - the new data will fit in the file record.
//

#define CheckMappingFits {                                                      \
    _AttrHeader = (PATTRIBUTE_RECORD_HEADER) Add2Ptr( FileRecord, LogRecord->RecordOffset );\
    _Length = LogRecord->AttributeOffset + Length;                              \
    if ((_AttrHeader->TypeCode == $END) ||                                      \
        NtfsIsAttributeResident( _AttrHeader ) ||                               \
        (LogRecord->AttributeOffset < _AttrHeader->Form.Nonresident.MappingPairsOffset) ||  \
        (LogRecord->AttributeOffset > _AttrHeader->RecordLength) ||             \
        ((_Length > _AttrHeader->RecordLength) &&                               \
         ((_Length - _AttrHeader->RecordLength) >                               \
          (FileRecord->BytesAvailable - FileRecord->FirstFreeByte)))) {         \
        NtfsMarkVolumeDirty( IrpContext, Vcb, TRUE );                           \
        NtfsUnpinBcb( IrpContext, Bcb );                                        \
        break;                                                                  \
    }                                                                           \
}

//
//  This routine simply checks that the attribute is non-resident.
//

#define CheckIfNonResident {                                                        \
    if (NtfsIsAttributeResident( (PATTRIBUTE_RECORD_HEADER) Add2Ptr( FileRecord,    \
                                                                     LogRecord->RecordOffset ))) { \
        NtfsMarkVolumeDirty( IrpContext, Vcb, TRUE );                               \
        NtfsUnpinBcb( IrpContext, Bcb );                                            \
        break;                                                                      \
    }                                                                               \
}

//
//  This routine checks if the record offset points to an index_root attribute.
//

#define CheckIfIndexRoot {                                                          \
    _Length = FileRecord->FirstAttributeOffset;                                     \
    _AttrHeader = Add2Ptr( FileRecord, FileRecord->FirstAttributeOffset );          \
    while (_Length < LogRecord->RecordOffset) {                                     \
        if ((_AttrHeader->TypeCode == $END) ||                                      \
            (_AttrHeader->RecordLength == 0)) {                                     \
            break;                                                                  \
        }                                                                           \
        _Length += _AttrHeader->RecordLength;                                       \
        _AttrHeader = NtfsGetNextRecord( _AttrHeader );                             \
    }                                                                               \
    if ((_Length != LogRecord->RecordOffset) ||                                     \
        (_AttrHeader->TypeCode != $INDEX_ROOT)) {                                   \
        NtfsMarkVolumeDirty( IrpContext, Vcb, TRUE );                               \
        NtfsUnpinBcb( IrpContext, Bcb );                                            \
        break;                                                                      \
    }                                                                               \
}

//
//  This routine checks if the attribute offset points to a valid index entry.
//

#define CheckIfRootIndexEntry {                                                     \
    _Length = PtrOffset( Attribute, IndexHeader ) +                                 \
                     IndexHeader->FirstIndexEntry;                                  \
    _CurrentEntry = Add2Ptr( IndexHeader, IndexHeader->FirstIndexEntry );           \
    while (_Length < LogRecord->AttributeOffset) {                                  \
        if ((_Length >= Attribute->RecordLength) ||                                 \
            (_CurrentEntry->Length == 0)) {                                         \
            break;                                                                  \
        }                                                                           \
        _Length += _CurrentEntry->Length;                                           \
        _CurrentEntry = Add2Ptr( _CurrentEntry, _CurrentEntry->Length );            \
    }                                                                               \
    if (_Length != LogRecord->AttributeOffset) {                                    \
        NtfsMarkVolumeDirty( IrpContext, Vcb, TRUE );                               \
        NtfsUnpinBcb( IrpContext, Bcb );                                            \
        break;                                                                      \
    }                                                                               \
}

//
//  This routine checks if the attribute offset points to a valid index entry.
//

#define CheckIfAllocationIndexEntry {                                               \
    ULONG _AdjustedOffset;                                                          \
    _Length = IndexHeader->FirstIndexEntry;                                         \
    _AdjustedOffset = FIELD_OFFSET( INDEX_ALLOCATION_BUFFER, IndexHeader )          \
                      + IndexHeader->FirstIndexEntry;                               \
    _CurrentEntry = Add2Ptr( IndexHeader, IndexHeader->FirstIndexEntry );           \
    while (_AdjustedOffset < LogRecord->AttributeOffset) {                          \
        if ((_Length >= IndexHeader->FirstFreeByte) ||                              \
            (_CurrentEntry->Length == 0)) {                                         \
            break;                                                                  \
        }                                                                           \
        _AdjustedOffset += _CurrentEntry->Length;                                   \
        _Length += _CurrentEntry->Length;                                           \
        _CurrentEntry = Add2Ptr( _CurrentEntry, _CurrentEntry->Length );            \
    }                                                                               \
    if (_AdjustedOffset != LogRecord->AttributeOffset) {                            \
        NtfsMarkVolumeDirty( IrpContext, Vcb, TRUE );                               \
        NtfsUnpinBcb( IrpContext, Bcb );                                            \
        break;                                                                      \
    }                                                                               \
}

//
//  This routine checks if we can safely add this index entry.
//      - The index entry must be within the log record
//      - There must be enough space in the attribute to insert this.
//

#define CheckIfRootEntryFits {                                                      \
    if (((ULONG_PTR) Add2Ptr( Data, IndexEntry->Length ) > (ULONG_PTR) Add2Ptr( LogRecord, LogRecordLength )) || \
        (IndexEntry->Length > FileRecord->BytesAvailable - FileRecord->FirstFreeByte)) {                 \
        NtfsMarkVolumeDirty( IrpContext, Vcb, TRUE );                               \
        NtfsUnpinBcb( IrpContext, Bcb );                                            \
        break;                                                                      \
    }                                                                               \
}

//
//  This routine checks that we can safely add this index entry.
//      - The entry must be contained in a log record.
//      - The entry must fit in the index buffer.
//

#define CheckIfAllocationEntryFits {                                                \
    if (((ULONG_PTR) Add2Ptr( Data, IndexEntry->Length ) >                              \
         (ULONG_PTR) Add2Ptr( LogRecord, LogRecordLength )) ||                          \
        (IndexEntry->Length > IndexHeader->BytesAvailable - IndexHeader->FirstFreeByte)) { \
        NtfsMarkVolumeDirty( IrpContext, Vcb, TRUE );                               \
        NtfsUnpinBcb( IrpContext, Bcb );                                            \
        break;                                                                      \
    }                                                                               \
}

//
//  This routine will check that the data will fit in the tail of an index buffer.
//

#define CheckWriteIndexBuffer {                                                 \
    if (LogRecord->AttributeOffset + Length >                                   \
        (FIELD_OFFSET( INDEX_ALLOCATION_BUFFER, IndexHeader ) +                 \
         IndexHeader->BytesAvailable)) {                                        \
        NtfsMarkVolumeDirty( IrpContext, Vcb, TRUE );                           \
        NtfsUnpinBcb( IrpContext, Bcb );                                        \
        break;                                                                  \
    }                                                                           \
}

//
//  This routine verifies that the bitmap bits are contained in the Lcns described.
//

#define CheckBitmapRange {                                                      \
    if ((BytesFromLogBlocks( LogRecord->ClusterBlockOffset ) +                  \
         ((BitMapRange->BitMapOffset + BitMapRange->NumberOfBits + 7) / 8)) >   \
        BytesFromClusters( Vcb, LogRecord->LcnsToFollow )) {                    \
        NtfsMarkVolumeDirty( IrpContext, Vcb, TRUE );                           \
        NtfsUnpinBcb( IrpContext, Bcb );                                        \
        break;                                                                  \
    }                                                                           \
}

VOID
DoAction (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PNTFS_LOG_RECORD_HEADER LogRecord,
    IN NTFS_LOG_OPERATION Operation,
    IN PVOID Data,
    IN ULONG Length,
    IN ULONG LogRecordLength,
    IN PLSN RedoLsn OPTIONAL,
    IN PSCB Scb OPTIONAL,
    OUT PBCB *Bcb,
    OUT PLSN *PageLsn
    )

/*++

Routine Description:

    This routine is a common routine for the Redo and Undo Passes, for performing
    the respective redo and undo operations.  All Redo- and Undo-specific
    processing is performed in RedoPass or UndoPass; in this routine all actions
    are treated identically, regardless of whether the action is undo or redo.
    Note that most actions are possible for both redo and undo, although some
    are only used for one or the other.


    Basically this routine is just a big switch statement dispatching on operation
    code.  The parameter descriptions provide some insight on how some of the
    parameters must be initialized differently for redo or undo.

Arguments:

    Vcb - Vcb for the volume being restarted.

    LogRecord - Pointer to the log record from which Redo or Undo is being executed.
                Only the common fields are accessed.

    Operation - The Redo or Undo operation to be performed.

    Data - Pointer to the Redo or Undo buffer, depending on the caller.

    Length - Length of the Redo or Undo buffer.

    LogRecordLength - Length of the entire log record.

    RedoLsn - For Redo this must be the Lsn of the Log Record for which the
              redo is being applied.  Must be NULL for transaction abort/undo.

    Scb - If specified this is the Scb for the stream to which this log record
        applies.  We have already looked this up (with proper synchronization) in
        the abort path.

    Bcb - Returns the Bcb of the page to which the action was performed, or NULL.

    PageLsn - Returns a pointer to where a new Lsn may be stored, or NULL.

Return Value:

    None.

--*/

{
    PFILE_RECORD_SEGMENT_HEADER FileRecord;
    PATTRIBUTE_RECORD_HEADER Attribute;

    PINDEX_HEADER IndexHeader;
    PINDEX_ALLOCATION_BUFFER IndexBuffer;
    PINDEX_ENTRY IndexEntry;

    //
    //  The following are used in the Check macros
    //

    PATTRIBUTE_RECORD_HEADER _AttrHeader;
    PINDEX_ENTRY _CurrentEntry;
    ULONG _Length;
    ULONG CorruptHint;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("DoAction:\n") );
    DebugTrace( 0, Dbg, ("Operation = %08lx\n", Operation) );
    DebugTrace( 0, Dbg, ("Data = %08lx\n", Data) );
    DebugTrace( 0, Dbg, ("Length = %08lx\n", Length) );

    //
    //  Initially clear outputs.
    //

    *Bcb = NULL;
    *PageLsn = NULL;

    //
    //  Dispatch to handle log record depending on type.
    //

    switch (Operation) {

    //
    //  To initialize a file record segment, we simply do a prepare write and copy the
    //  file record in.
    //

    case InitializeFileRecordSegment:

        //
        //  Check the log record and that the data is a valid file record.
        //

        CheckWriteFileRecord;

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        *PageLsn = &FileRecord->Lsn;

        RtlCopyMemory( FileRecord, Data, Length );
        break;

    //
    //  To deallocate a file record segment, we do a prepare write (no need to read it
    //  to deallocate it), and clear FILE_RECORD_SEGMENT_IN_USE.
    //

    case DeallocateFileRecordSegment:

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        *PageLsn = &FileRecord->Lsn;

        ASSERT( FlagOn( IrpContext->Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS )
                || FlagOn( FileRecord->Flags, FILE_RECORD_SEGMENT_IN_USE ));

        ClearFlag(FileRecord->Flags, FILE_RECORD_SEGMENT_IN_USE);

        FileRecord->SequenceNumber += 1;

        break;

    //
    //  To write the end of a file record segment, we calculate a pointer to the
    //  destination position (OldAttribute), and then call the routine to take
    //  care of it.
    //

    case WriteEndOfFileRecordSegment:

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        CheckLsn( FileRecord );
        CheckFileRecordBefore;
        CheckIfAttribute( TRUE );
        CheckWriteFileRecord;

        *PageLsn = &FileRecord->Lsn;

        Attribute = Add2Ptr( FileRecord, LogRecord->RecordOffset );

        NtfsRestartWriteEndOfFileRecord( FileRecord,
                                         Attribute,
                                         (PATTRIBUTE_RECORD_HEADER)Data,
                                         Length );
        CheckFileRecordAfter;

        break;

    //
    //  For Create Attribute, we read in the designated Mft record, and
    //  insert the attribute record from the log record.
    //

    case CreateAttribute:

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        CheckLsn( FileRecord );
        CheckFileRecordBefore;
        CheckIfAttribute( TRUE );
        CheckInsertAttribute;

        *PageLsn = &FileRecord->Lsn;

        NtfsRestartInsertAttribute( IrpContext,
                                    FileRecord,
                                    LogRecord->RecordOffset,
                                    (PATTRIBUTE_RECORD_HEADER)Data,
                                    NULL,
                                    NULL,
                                    0 );

        CheckFileRecordAfter;

        break;

    //
    //  To Delete an attribute, we read the designated Mft record and make
    //  a call to remove the attribute record.
    //

    case DeleteAttribute:

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        CheckLsn( FileRecord );
        CheckFileRecordBefore;
        CheckIfAttribute( FALSE );

        *PageLsn = &FileRecord->Lsn;

        NtfsRestartRemoveAttribute( IrpContext,
                                    FileRecord,
                                    LogRecord->RecordOffset );

        CheckFileRecordAfter;

        break;

    //
    //  To update a resident attribute, we read the designated Mft record and
    //  call the routine to change its value.
    //

    case UpdateResidentValue:

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        CheckLsn( FileRecord );
        CheckFileRecordBefore;
        CheckIfAttribute( FALSE );
        CheckResidentFits;

        *PageLsn = &FileRecord->Lsn;

        NtfsRestartChangeValue( IrpContext,
                                FileRecord,
                                LogRecord->RecordOffset,
                                LogRecord->AttributeOffset,
                                Data,
                                Length,
                                (BOOLEAN)((LogRecord->RedoLength !=
                                           LogRecord->UndoLength) ?
                                             TRUE : FALSE) );

        CheckFileRecordAfter;

        break;

    //
    //  To update a nonresident value, we simply pin the attribute and copy
    //  the data in.  Log record will limit us to a page at a time.
    //

    case UpdateNonresidentValue:

        {
            PVOID Buffer;

            //
            //  Pin the desired index buffer, and check the Lsn.
            //

            ASSERT( Length <= PAGE_SIZE );

            PinAttributeForRestart( IrpContext,
                                    Vcb,
                                    LogRecord,
                                    Length,
                                    Bcb,
                                    &Buffer,
                                    &Scb );

            CheckNonResidentFits;
            RtlCopyMemory( (PCHAR)Buffer + LogRecord->RecordOffset, Data, Length );

            break;
        }

    //
    //  To update the mapping pairs in a nonresident attribute, we read the
    //  designated Mft record and call the routine to change them.
    //

    case UpdateMappingPairs:

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        CheckLsn( FileRecord );
        CheckFileRecordBefore;
        CheckIfAttribute( FALSE );
        CheckMappingFits;

        *PageLsn = &FileRecord->Lsn;

        NtfsRestartChangeMapping( IrpContext,
                                  Vcb,
                                  FileRecord,
                                  LogRecord->RecordOffset,
                                  LogRecord->AttributeOffset,
                                  Data,
                                  Length );

        CheckFileRecordAfter;

        break;

    //
    //  To set new attribute sizes, we read the designated Mft record, point
    //  to the attribute, and copy in the new sizes.
    //

    case SetNewAttributeSizes:

        {
            PNEW_ATTRIBUTE_SIZES Sizes;

            //
            //  Pin the desired Mft record.
            //

            PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

            CheckLsn( FileRecord );
            CheckFileRecordBefore;
            CheckIfAttribute( FALSE );
            CheckIfNonResident;

            *PageLsn = &FileRecord->Lsn;

            Sizes = (PNEW_ATTRIBUTE_SIZES)Data;

            Attribute = (PATTRIBUTE_RECORD_HEADER)((PCHAR)FileRecord +
                          LogRecord->RecordOffset);

            NtfsVerifySizesLongLong( Sizes );
            Attribute->Form.Nonresident.AllocatedLength = Sizes->AllocationSize;

            Attribute->Form.Nonresident.FileSize = Sizes->FileSize;

            Attribute->Form.Nonresident.ValidDataLength = Sizes->ValidDataLength;

            if (Length >= SIZEOF_FULL_ATTRIBUTE_SIZES) {

                Attribute->Form.Nonresident.TotalAllocated = Sizes->TotalAllocated;
            }

            CheckFileRecordAfter;

            break;
        }

    //
    //  To insert a new index entry in the root, we read the designated Mft
    //  record, point to the attribute and the insertion point, and call the
    //  same routine used in normal operation.
    //

    case AddIndexEntryRoot:

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        CheckLsn( FileRecord );
        CheckFileRecordBefore;
        CheckIfIndexRoot;

        Attribute = (PATTRIBUTE_RECORD_HEADER)((PCHAR)FileRecord +
                      LogRecord->RecordOffset);

        IndexEntry = (PINDEX_ENTRY)Data;
        IndexHeader = &((PINDEX_ROOT) NtfsAttributeValue( Attribute ))->IndexHeader;

        CheckIfRootIndexEntry;
        CheckIfRootEntryFits;

        *PageLsn = &FileRecord->Lsn;

        NtfsRestartInsertSimpleRoot( IrpContext,
                                     IndexEntry,
                                     FileRecord,
                                     Attribute,
                                     Add2Ptr( Attribute, LogRecord->AttributeOffset ));

        CheckFileRecordAfter;

        break;

    //
    //  To insert a new index entry in the root, we read the designated Mft
    //  record, point to the attribute and the insertion point, and call the
    //  same routine used in normal operation.
    //

    case DeleteIndexEntryRoot:

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        CheckLsn( FileRecord );
        CheckFileRecordBefore;
        CheckIfIndexRoot;

        Attribute = (PATTRIBUTE_RECORD_HEADER)((PCHAR)FileRecord +
                      LogRecord->RecordOffset);

        IndexHeader = &((PINDEX_ROOT) NtfsAttributeValue( Attribute ))->IndexHeader;
        CheckIfRootIndexEntry;

        *PageLsn = &FileRecord->Lsn;

        IndexEntry = (PINDEX_ENTRY) Add2Ptr( Attribute,
                                             LogRecord->AttributeOffset);

        NtfsRestartDeleteSimpleRoot( IrpContext,
                                     IndexEntry,
                                     FileRecord,
                                     Attribute );

        CheckFileRecordAfter;

        break;

    //
    //  To insert a new index entry in the allocation, we read the designated index
    //  buffer, point to the insertion point, and call the same routine used in
    //  normal operation.
    //

    case AddIndexEntryAllocation:

        //
        //  Pin the desired index buffer, and check the Lsn.
        //

        ASSERT( Length <= PAGE_SIZE );

        PinAttributeForRestart( IrpContext, Vcb, LogRecord, 0, Bcb, (PVOID *)&IndexBuffer, &Scb );

        CheckLsn( IndexBuffer );
        CheckIndexBufferBefore;

        IndexEntry = (PINDEX_ENTRY)Data;
        IndexHeader = &IndexBuffer->IndexHeader;

        CheckIfAllocationIndexEntry;
        CheckIfAllocationEntryFits;

        *PageLsn = &IndexBuffer->Lsn;

        NtfsRestartInsertSimpleAllocation( IndexEntry,
                                           IndexBuffer,
                                           Add2Ptr( IndexBuffer, LogRecord->AttributeOffset ));

        CheckIndexBufferAfter;

        break;

    //
    //  To delete an index entry in the allocation, we read the designated index
    //  buffer, point to the deletion point, and call the same routine used in
    //  normal operation.
    //

    case DeleteIndexEntryAllocation:

        //
        //  Pin the desired index buffer, and check the Lsn.
        //

        ASSERT( Length <= PAGE_SIZE );

        PinAttributeForRestart( IrpContext, Vcb, LogRecord, 0, Bcb, (PVOID *)&IndexBuffer, &Scb );

        CheckLsn( IndexBuffer );
        CheckIndexBufferBefore;

        IndexHeader = &IndexBuffer->IndexHeader;
        CheckIfAllocationIndexEntry;

        IndexEntry = (PINDEX_ENTRY)((PCHAR)IndexBuffer + LogRecord->AttributeOffset);

        ASSERT( (0 == Length) || (Length == IndexEntry->Length) );
        ASSERT( (0 == Length) || (0 == RtlCompareMemory( IndexEntry, Data, Length)) );

        *PageLsn = &IndexBuffer->Lsn;

        NtfsRestartDeleteSimpleAllocation( IndexEntry, IndexBuffer );

        CheckIndexBufferAfter;

        break;

    case WriteEndOfIndexBuffer:

        //
        //  Pin the desired index buffer, and check the Lsn.
        //

        ASSERT( Length <= PAGE_SIZE );

        PinAttributeForRestart( IrpContext, Vcb, LogRecord, 0, Bcb, (PVOID *)&IndexBuffer, &Scb );

        CheckLsn( IndexBuffer );
        CheckIndexBufferBefore;

        IndexHeader = &IndexBuffer->IndexHeader;
        CheckIfAllocationIndexEntry;
        CheckWriteIndexBuffer;

        *PageLsn = &IndexBuffer->Lsn;

        IndexEntry = (PINDEX_ENTRY)((PCHAR)IndexBuffer + LogRecord->AttributeOffset);

        NtfsRestartWriteEndOfIndex( IndexHeader,
                                    IndexEntry,
                                    (PINDEX_ENTRY)Data,
                                    Length );
        CheckIndexBufferAfter;

        break;

    //
    //  To set a new index entry Vcn in the root, we read the designated Mft
    //  record, point to the attribute and the index entry, and call the
    //  same routine used in normal operation.
    //

    case SetIndexEntryVcnRoot:

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        CheckLsn( FileRecord );
        CheckFileRecordBefore;
        CheckIfIndexRoot;

        Attribute = (PATTRIBUTE_RECORD_HEADER) Add2Ptr( FileRecord, LogRecord->RecordOffset );

        IndexHeader = &((PINDEX_ROOT) NtfsAttributeValue( Attribute ))->IndexHeader;

        CheckIfRootIndexEntry;

        *PageLsn = &FileRecord->Lsn;

        IndexEntry = (PINDEX_ENTRY)((PCHAR)Attribute +
                       LogRecord->AttributeOffset);

        NtfsRestartSetIndexBlock( IndexEntry,
                                  *((PLONGLONG) Data) );
        CheckFileRecordAfter;

        break;

    //
    //  To set a new index entry Vcn in the allocation, we read the designated index
    //  buffer, point to the index entry, and call the same routine used in
    //  normal operation.
    //

    case SetIndexEntryVcnAllocation:

        //
        //  Pin the desired index buffer, and check the Lsn.
        //

        ASSERT( Length <= PAGE_SIZE );

        PinAttributeForRestart( IrpContext, Vcb, LogRecord, 0, Bcb, (PVOID *)&IndexBuffer, &Scb );

        CheckLsn( IndexBuffer );
        CheckIndexBufferBefore;

        IndexHeader = &IndexBuffer->IndexHeader;
        CheckIfAllocationIndexEntry;

        *PageLsn = &IndexBuffer->Lsn;

        IndexEntry = (PINDEX_ENTRY) Add2Ptr( IndexBuffer, LogRecord->AttributeOffset );

        NtfsRestartSetIndexBlock( IndexEntry,
                                  *((PLONGLONG) Data) );
        CheckIndexBufferAfter;

        break;

    //
    //  To update a file name in the root, we read the designated Mft
    //  record, point to the attribute and the index entry, and call the
    //  same routine used in normal operation.
    //

    case UpdateFileNameRoot:

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        CheckLsn( FileRecord );
        CheckFileRecordBefore;
        CheckIfIndexRoot;

        Attribute = (PATTRIBUTE_RECORD_HEADER) Add2Ptr( FileRecord, LogRecord->RecordOffset );

        IndexHeader = &((PINDEX_ROOT) NtfsAttributeValue( Attribute ))->IndexHeader;
        CheckIfRootIndexEntry;

        IndexEntry = (PINDEX_ENTRY) Add2Ptr( Attribute, LogRecord->AttributeOffset );

        NtfsRestartUpdateFileName( IndexEntry,
                                   (PDUPLICATED_INFORMATION) Data );

        CheckFileRecordAfter;

        break;

    //
    //  To update a file name in the allocation, we read the designated index
    //  buffer, point to the index entry, and call the same routine used in
    //  normal operation.
    //

    case UpdateFileNameAllocation:

        //
        //  Pin the desired index buffer, and check the Lsn.
        //

        ASSERT( Length <= PAGE_SIZE );

        PinAttributeForRestart( IrpContext, Vcb, LogRecord, 0, Bcb, (PVOID *)&IndexBuffer, &Scb );

        CheckLsn( IndexBuffer );
        CheckIndexBufferBefore;

        IndexHeader = &IndexBuffer->IndexHeader;
        CheckIfAllocationIndexEntry;

        IndexEntry = (PINDEX_ENTRY) Add2Ptr( IndexBuffer, LogRecord->AttributeOffset );

        NtfsRestartUpdateFileName( IndexEntry,
                                   (PDUPLICATED_INFORMATION) Data );

        CheckIndexBufferAfter;

        break;

    //
    //  To set a range of bits in the volume bitmap, we just read in the a hunk
    //  of the bitmap as described by the log record, and then call the restart
    //  routine to do it.
    //

    case SetBitsInNonresidentBitMap:

        {
            PBITMAP_RANGE BitMapRange;
            PVOID BitMapBuffer;
            ULONG BitMapSize;
            RTL_BITMAP Bitmap;

            //
            //  Open the attribute first to get the Scb.
            //

            OpenAttributeForRestart( IrpContext, Vcb, LogRecord, &Scb );

            //
            //  Pin the desired bitmap buffer.
            //

            ASSERT( Length <= PAGE_SIZE );

            PinAttributeForRestart( IrpContext, Vcb, LogRecord, 0, Bcb, &BitMapBuffer, &Scb );

            BitMapRange = (PBITMAP_RANGE)Data;

            CheckBitmapRange;

            //
            //  Initialize our bitmap description, and call the restart
            //  routine with the bitmap Scb exclusive (assuming it cannot
            //  raise).
            //

            BitMapSize = BytesFromClusters( Vcb, LogRecord->LcnsToFollow ) * 8;

            RtlInitializeBitMap( &Bitmap, BitMapBuffer, BitMapSize );

            NtfsRestartSetBitsInBitMap( IrpContext,
                                        &Bitmap,
                                        BitMapRange->BitMapOffset,
                                        BitMapRange->NumberOfBits );

            if (!FlagOn( IrpContext->Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS ) &&
                (Scb == Vcb->BitmapScb)) {

                ULONGLONG ThisLcn;
                LONGLONG FoundLcn;
                LONGLONG FoundClusters;
                BOOLEAN FoundMatch = FALSE;
                PDEALLOCATED_CLUSTERS Clusters;

                ThisLcn = (ULONGLONG) ((BytesFromClusters( Vcb, LogRecord->TargetVcn ) + BytesFromLogBlocks( LogRecord->ClusterBlockOffset )) * 8);
                ThisLcn += BitMapRange->BitMapOffset;

                //
                //  Best odds are that these are in the active deallocated clusters.
                //

                Clusters = (PDEALLOCATED_CLUSTERS)Vcb->DeallocatedClusterListHead.Flink;

                do {

                    if (FsRtlLookupLargeMcbEntry( &Clusters->Mcb,
                                                  ThisLcn,
                                                  &FoundLcn,
                                                  &FoundClusters,
                                                  NULL,
                                                  NULL,
                                                  NULL ) &&
                        (FoundLcn != UNUSED_LCN)) {

                        ASSERT( FoundClusters >= BitMapRange->NumberOfBits );

                        FsRtlRemoveLargeMcbEntry( &Clusters->Mcb,
                                                  ThisLcn,
                                                  BitMapRange->NumberOfBits );

                        //
                        //  Assume again that we will always be able to remove
                        //  the entries.  Even if we don't it just means that it won't be
                        //  available to allocate this cluster.  The counts should be in-sync
                        //  since they are changed together.
                        //

                        Clusters->ClusterCount -= BitMapRange->NumberOfBits;
                        Vcb->DeallocatedClusters -= BitMapRange->NumberOfBits;
                        FoundMatch = TRUE;
                        break;
                    }

                    Clusters = (PDEALLOCATED_CLUSTERS)Clusters->Link.Flink;
                } while ( &Clusters->Link != &Vcb->DeallocatedClusterListHead );
            }

#ifdef NTFS_CHECK_BITMAP
            if (!FlagOn( IrpContext->Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS ) &&
                (Scb == Vcb->BitmapScb) &&
                (Vcb->BitmapCopy != NULL)) {

                ULONG BitmapOffset;
                ULONG BitmapPage;
                ULONG StartBit;

                BitmapOffset = (BytesFromClusters( Vcb, LogRecord->TargetVcn ) + BytesFromLogBlocks( LogRecord->ClusterBlockOffset )) * 8;

                BitmapPage = (BitmapOffset + BitMapRange->BitMapOffset) / (PAGE_SIZE * 8);
                StartBit = (BitmapOffset + BitMapRange->BitMapOffset) & ((PAGE_SIZE * 8) - 1);

                RtlSetBits( Vcb->BitmapCopy + BitmapPage, StartBit, BitMapRange->NumberOfBits );
            }
#endif

            break;
        }

    //
    //  To clear a range of bits in the volume bitmap, we just read in the a hunk
    //  of the bitmap as described by the log record, and then call the restart
    //  routine to do it.
    //

    case ClearBitsInNonresidentBitMap:

        {
            PBITMAP_RANGE BitMapRange;
            PVOID BitMapBuffer;
            ULONG BitMapSize;
            RTL_BITMAP Bitmap;

            //
            //  Open the attribute first to get the Scb.
            //

            OpenAttributeForRestart( IrpContext, Vcb, LogRecord, &Scb );

            //
            //  Pin the desired bitmap buffer.
            //

            ASSERT( Length <= PAGE_SIZE );

            PinAttributeForRestart( IrpContext, Vcb, LogRecord, 0, Bcb, &BitMapBuffer, &Scb );

            BitMapRange = (PBITMAP_RANGE)Data;

            CheckBitmapRange;

            BitMapSize = BytesFromClusters( Vcb, LogRecord->LcnsToFollow ) * 8;

            //
            //  Initialize our bitmap description, and call the restart
            //  routine with the bitmap Scb exclusive (assuming it cannot
            //  raise).
            //

            RtlInitializeBitMap( &Bitmap, BitMapBuffer, BitMapSize );

            NtfsRestartClearBitsInBitMap( IrpContext,
                                          &Bitmap,
                                          BitMapRange->BitMapOffset,
                                          BitMapRange->NumberOfBits );

            //
            //  Look and see if we can return these to the free cluster Mcb.
            //

            if (!FlagOn( IrpContext->Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS ) &&
                (Scb == Vcb->BitmapScb)) {

                ULONGLONG ThisLcn;

                ThisLcn = (ULONGLONG) ((BytesFromClusters( Vcb, LogRecord->TargetVcn ) + BytesFromLogBlocks( LogRecord->ClusterBlockOffset )) * 8);
                ThisLcn += BitMapRange->BitMapOffset;

                //
                //  Use a try-finally to protect against failures.
                //

                try {

                    NtfsAddCachedRun( IrpContext,
                                      IrpContext->Vcb,
                                      ThisLcn,
                                      BitMapRange->NumberOfBits,
                                      RunStateFree );

                } except( (GetExceptionCode() == STATUS_INSUFFICIENT_RESOURCES) ?
                          EXCEPTION_EXECUTE_HANDLER :
                          EXCEPTION_CONTINUE_SEARCH ) {

                      NtfsMinimumExceptionProcessing( IrpContext );

                }
            }

#ifdef NTFS_CHECK_BITMAP
            if (!FlagOn( IrpContext->Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS ) &&
                (Scb == Vcb->BitmapScb) &&
                (Vcb->BitmapCopy != NULL)) {

                ULONG BitmapOffset;
                ULONG BitmapPage;
                ULONG StartBit;

                BitmapOffset = (BytesFromClusters( Vcb, LogRecord->TargetVcn ) + BytesFromLogBlocks( LogRecord->ClusterBlockOffset )) * 8;

                BitmapPage = (BitmapOffset + BitMapRange->BitMapOffset) / (PAGE_SIZE * 8);
                StartBit = (BitmapOffset + BitMapRange->BitMapOffset) & ((PAGE_SIZE * 8) - 1);

                RtlClearBits( Vcb->BitmapCopy + BitmapPage, StartBit, BitMapRange->NumberOfBits );
            }
#endif
            break;
        }

    //
    //  To update a file name in the root, we read the designated Mft
    //  record, point to the attribute and the index entry, and call the
    //  same routine used in normal operation.
    //

    case UpdateRecordDataRoot:

        //
        //  Pin the desired Mft record.
        //

        PinMftRecordForRestart( IrpContext, Vcb, LogRecord, Bcb, &FileRecord );

        CheckLsn( FileRecord );
        CheckFileRecordBefore;
        CheckIfIndexRoot;

        Attribute = (PATTRIBUTE_RECORD_HEADER)((PCHAR)FileRecord +
                      LogRecord->RecordOffset);

        IndexHeader = &((PINDEX_ROOT) NtfsAttributeValue( Attribute ))->IndexHeader;
        CheckIfRootIndexEntry;

        IndexEntry = (PINDEX_ENTRY)((PCHAR)Attribute +
                       LogRecord->AttributeOffset);

        NtOfsRestartUpdateDataInIndex( IndexEntry, Data, Length );

        CheckFileRecordAfter;

        break;

    //
    //  To update a file name in the allocation, we read the designated index
    //  buffer, point to the index entry, and call the same routine used in
    //  normal operation.
    //

    case UpdateRecordDataAllocation:

        //
        //  Pin the desired index buffer, and check the Lsn.
        //

        ASSERT( Length <= PAGE_SIZE );

        PinAttributeForRestart( IrpContext, Vcb, LogRecord, 0, Bcb, (PVOID *)&IndexBuffer, &Scb );

        CheckLsn( IndexBuffer );
        CheckIndexBufferBefore;

        IndexHeader = &IndexBuffer->IndexHeader;
        CheckIfAllocationIndexEntry;

        IndexEntry = (PINDEX_ENTRY)((PCHAR)IndexBuffer +
                       LogRecord->AttributeOffset);

        NtOfsRestartUpdateDataInIndex( IndexEntry, Data, Length );

        CheckIndexBufferAfter;

        break;

    //
    //  The following cases require no action during the Redo or Undo Pass.
    //

    case Noop:
    case DeleteDirtyClusters:
    case HotFix:
    case EndTopLevelAction:
    case PrepareTransaction:
    case CommitTransaction:
    case ForgetTransaction:
    case CompensationLogRecord:
    case OpenNonresidentAttribute:
    case OpenAttributeTableDump:
    case AttributeNamesDump:
    case DirtyPageTableDump:
    case TransactionTableDump:

        break;

    //
    //  All codes will be explicitly handled.  If we see a code we
    //  do not expect, then we are in trouble.
    //

    default:

        DebugTrace( 0, Dbg, ("Record address: %08lx\n", LogRecord) );
        DebugTrace( 0, Dbg, ("Redo operation is: %04lx\n", LogRecord->RedoOperation) );
        DebugTrace( 0, Dbg, ("Undo operation is: %04lx\n", LogRecord->RedoOperation) );

        ASSERTMSG( "Unknown Action!\n", FALSE );

        break;
    }

    DebugDoit(
        if (*Bcb != NULL) {
            DebugTrace( 0, Dbg, ("**** Update applied\n") );
        }
    );

    DebugTrace( 0, Dbg, ("Bcb > %08lx\n", *Bcb) );
    DebugTrace( 0, Dbg, ("PageLsn > %08lx\n", *PageLsn) );
    DebugTrace( -1, Dbg, ("DoAction -> VOID\n") );
}


//
//  Internal support routine
//

VOID
PinMftRecordForRestart (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PNTFS_LOG_RECORD_HEADER LogRecord,
    OUT PBCB *Bcb,
    OUT PFILE_RECORD_SEGMENT_HEADER *FileRecord
    )

/*++

Routine Description:

    This routine pins a record in the Mft for restart, as described
    by the current log record.

Arguments:

    Vcb - Supplies the Vcb pointer for the volume

    LogRecord - Supplies the pointer to the current log record.

    Bcb - Returns a pointer to the Bcb for the pinned record.

    FileRecord - Returns a pointer to the desired file record.

Return Value:

    None

--*/

{
    LONGLONG SegmentReference;

    PAGED_CODE();

    //
    //  Calculate the file number part of the segment reference.  Do this
    //  by obtaining the file offset of the file record and then convert to
    //  a file number.
    //

    SegmentReference = LlBytesFromClusters( Vcb, LogRecord->TargetVcn );
    SegmentReference += BytesFromLogBlocks( LogRecord->ClusterBlockOffset );
    SegmentReference = LlFileRecordsFromBytes( Vcb, SegmentReference );

    //
    //  Pin the Mft record.
    //

    NtfsPinMftRecord( IrpContext,
                      Vcb,
                      (PMFT_SEGMENT_REFERENCE)&SegmentReference,
                      TRUE,
                      Bcb,
                      FileRecord,
                      NULL );

    ASSERT( (*FileRecord)->MultiSectorHeader.Signature !=  BaadSignature );
}


//
//  Internal support routine
//

VOID
OpenAttributeForRestart (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PNTFS_LOG_RECORD_HEADER LogRecord,
    IN OUT PSCB *Scb
    )

/*++

Routine Description:

    This routine opens the desired attribute for restart, as described
    by the current log record.

Arguments:

    Vcb - Supplies the Vcb pointer for the volume

    LogRecord - Supplies the pointer to the current log record.

    Scb - On input points to an optional Scb.  On return it points to
        the Scb for the log record.  It is either the input Scb if specified
        or the Scb for the attribute entry.

Return Value:

    None

--*/

{
    POPEN_ATTRIBUTE_ENTRY AttributeEntry;

    PAGED_CODE();

    //
    //  Get a pointer to the attribute entry for the described attribute.
    //

    if (*Scb == NULL) {

        AttributeEntry = GetRestartEntryFromIndex( Vcb->OnDiskOat, LogRecord->TargetAttribute );

        //
        //  Check if want to go to the other table.
        //

        if (Vcb->RestartVersion == 0) {

            AttributeEntry = GetRestartEntryFromIndex( &Vcb->OpenAttributeTable,
                                                       ((POPEN_ATTRIBUTE_ENTRY_V0) AttributeEntry)->OatIndex );

        }

        *Scb = AttributeEntry->OatData->Overlay.Scb;
    }

    if ((*Scb)->FileObject == NULL) {
        NtfsCreateInternalAttributeStream( IrpContext, *Scb, TRUE, NULL );

        if (FlagOn( IrpContext->Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS )) {

            CcSetAdditionalCacheAttributes( (*Scb)->FileObject, TRUE, TRUE );
        }
    }

    return;
}


//
//  Internal support routine
//

VOID
PinAttributeForRestart (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PNTFS_LOG_RECORD_HEADER LogRecord,
    IN ULONG Length OPTIONAL,
    OUT PBCB *Bcb,
    OUT PVOID *Buffer,
    IN OUT PSCB *Scb
    )

/*++

Routine Description:

    This routine pins the desired buffer for restart, as described
    by the current log record.

Arguments:

    Vcb - Supplies the Vcb pointer for the volume

    LogRecord - Supplies the pointer to the current log record.

    Length - If specified we will use this to determine the length
        to pin.  This will handle the non-resident streams which may
        change size (ACL, attribute lists).  The log record may have
        more clusters than are currently in the stream.

    Bcb - Returns a pointer to the Bcb for the pinned record.

    Buffer - Returns a pointer to the desired buffer.

    Scb - Returns a pointer to the Scb for the attribute

Return Value:

    None

--*/

{
    LONGLONG FileOffset;
    ULONG ClusterOffset;
    ULONG PinLength;

    PAGED_CODE();

    //
    //  First open the described atttribute.
    //

    OpenAttributeForRestart( IrpContext, Vcb, LogRecord, Scb );

    //
    //  Calculate the desired file offset and pin the buffer.
    //

    ClusterOffset = BytesFromLogBlocks( LogRecord->ClusterBlockOffset );
    FileOffset = LlBytesFromClusters( Vcb, LogRecord->TargetVcn ) + ClusterOffset;

    ASSERT((!FlagOn( (*Scb)->ScbPersist, SCB_PERSIST_USN_JOURNAL )) || (FileOffset >= Vcb->UsnCacheBias));

    //
    //  We only want to pin the requested clusters or to the end of
    //  a page, whichever is smaller.
    //

    if (Vcb->BytesPerCluster > PAGE_SIZE) {

        PinLength = PAGE_SIZE - (((ULONG) FileOffset) & (PAGE_SIZE - 1));

    } else if (Length != 0) {

        PinLength = Length;

    } else {

        PinLength = BytesFromClusters( Vcb, LogRecord->LcnsToFollow ) - ClusterOffset;
    }

    //
    //  We don't want to pin more than a page
    //

    NtfsPinStream( IrpContext,
                   *Scb,
                   FileOffset,
                   PinLength,
                   Bcb,
                   Buffer );

#if DBG

    //
    // Check index signature integrity
    //

    {
        PVOID AlignedBuffer;
        PINDEX_ALLOCATION_BUFFER AllocBuffer;

        AlignedBuffer = (PVOID) ((((ULONG_PTR)(*Buffer) + (0x1000)) & ~(0x1000 -1)) - 0x1000);
        AllocBuffer = (PINDEX_ALLOCATION_BUFFER) AlignedBuffer;

        if ((LogRecord->RedoOperation != UpdateNonresidentValue) &&
            (LogRecord->UndoOperation != UpdateNonresidentValue) &&
            ((*Scb)->AttributeTypeCode == $INDEX_ALLOCATION) &&
            ((*Scb)->AttributeName.Length == 8) &&
            (wcsncmp( (*Scb)->AttributeName.Buffer, L"$I30", 4 ) == 0)) {

            if (*(PULONG)AllocBuffer->MultiSectorHeader.Signature != *(PULONG)IndexSignature) {
                KdPrint(( "Ntfs: index signature is: %d %c%c%c%c for LCN: 0x%I64x\n",
                          *(PULONG)AllocBuffer->MultiSectorHeader.Signature,
                          AllocBuffer->MultiSectorHeader.Signature[0],
                          AllocBuffer->MultiSectorHeader.Signature[1],
                          AllocBuffer->MultiSectorHeader.Signature[2],
                          AllocBuffer->MultiSectorHeader.Signature[3],
                          LogRecord->LcnsForPage[0] ));

                if (*(PULONG)AllocBuffer->MultiSectorHeader.Signature != 0 &&
                    *(PULONG)AllocBuffer->MultiSectorHeader.Signature != *(PULONG)BaadSignature &&
                    *(PULONG)AllocBuffer->MultiSectorHeader.Signature != *(PULONG)HoleSignature) {

                    DbgBreakPoint();
                }
            } //endif signature fork
        } //endif index scb fork
    }
#endif

}


//
//  Internal support routine
//

BOOLEAN
FindDirtyPage (
    IN PRESTART_POINTERS DirtyPageTable,
    IN ULONG TargetAttribute,
    IN VCN Vcn,
    OUT PDIRTY_PAGE_ENTRY *DirtyPageEntry
    )

/*++

Routine Description:

    This routine searches for a Vcn to see if it is already in the Dirty Page
    Table, returning the Dirty Page Entry if it is.

Arguments:

    DirtyPageTable - pointer to the Dirty Page Table to search.

    TargetAttribute - Attribute for which the dirty Vcn is to be searched.

    Vcn - Vcn to search for.

    DirtyPageEntry - returns a pointer to the Dirty Page Entry if returning TRUE.

Return Value:

    TRUE if the page was found and is being returned, else FALSE.

--*/

{
    PDIRTY_PAGE_ENTRY DirtyPage;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("FindDirtyPage:\n") );
    DebugTrace( 0, Dbg, ("TargetAttribute = %08lx\n", TargetAttribute) );
    DebugTrace( 0, Dbg, ("Vcn = %016I64x\n", Vcn) );

    //
    //  If table has not yet been initialized, return.
    //

    if (DirtyPageTable->Table == NULL) {
        return FALSE;
    }

    //
    //  Loop through all of the dirty pages to look for a match.
    //

    DirtyPage = NtfsGetFirstRestartTable( DirtyPageTable );

    //
    //  Loop to end of table.
    //

    while (DirtyPage != NULL) {

        if ((DirtyPage->TargetAttribute == TargetAttribute)

                &&

            (Vcn >= DirtyPage->Vcn)) {

            //
            //  Compute the Last Vcn outside of the comparison or the xxAdd and
            //  xxFromUlong will be called three times.
            //

            LONGLONG BeyondLastVcn;

            BeyondLastVcn = DirtyPage->Vcn + DirtyPage->LcnsToFollow;

            if (Vcn < BeyondLastVcn) {

                *DirtyPageEntry = DirtyPage;

                DebugTrace( 0, Dbg, ("DirtyPageEntry %08lx\n", *DirtyPageEntry) );
                DebugTrace( -1, Dbg, ("FindDirtypage -> TRUE\n") );

                return TRUE;
            }
        }

        //
        //  Point to next entry in table, or NULL.
        //

        DirtyPage = NtfsGetNextRestartTable( DirtyPageTable,
                                             DirtyPage );
    }
    *DirtyPageEntry = NULL;

    DebugTrace( -1, Dbg, ("FindDirtypage -> FALSE\n") );

    return FALSE;
}



//
//  Internal support routine
//

VOID
PageUpdateAnalysis (
    IN PVCB Vcb,
    IN LSN Lsn,
    IN OUT PRESTART_POINTERS DirtyPageTable,
    IN PNTFS_LOG_RECORD_HEADER LogRecord
    )

/*++

Routine Description:

    This routine updates the Dirty Pages Table during the analysis phase
    for all log records which update a page.

Arguments:

    Vcb - Pointer to the Vcb for the volume.

    Lsn - The Lsn of the log record.

    DirtyPageTable - A pointer to the Dirty Page Table pointer, to be
                     updated and potentially expanded.

    LogRecord - Pointer to the Log Record being analyzed.

Return Value:

    None.

--*/

{
    PDIRTY_PAGE_ENTRY DirtyPage;
    ULONG i;
    RESTART_POINTERS NewDirtyPageTable;
    ULONG ClustersPerPage;
    ULONG PageIndex;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("PageUpdateAnalysis:\n") );

    //
    //  Calculate the number of clusters per page in the system which wrote
    //  the checkpoint, possibly creating the table.
    //

    if (DirtyPageTable->Table != NULL) {
        ClustersPerPage = ((DirtyPageTable->Table->EntrySize -
                            sizeof(DIRTY_PAGE_ENTRY)) / sizeof(LCN)) + 1;
    } else {
        ClustersPerPage = Vcb->ClustersPerPage;
        NtfsInitializeRestartTable( sizeof(DIRTY_PAGE_ENTRY) +
                                      (ClustersPerPage - 1) * sizeof(LCN),
                                    INITIAL_NUMBER_DIRTY_PAGES,
                                    DirtyPageTable );
    }

    //
    //  If the on disk number of lcns doesn't match our curent page size
    //  we need to reallocate the entire table to accomodate this
    //

    if((ULONG)LogRecord->LcnsToFollow > ClustersPerPage) {

        PDIRTY_PAGE_ENTRY OldDirtyPage;

        DebugTrace( +1, Dbg, ("Ntfs: resizing table in pageupdateanalysis\n") );

        //
        // Adjust clusters per page up to the number of clusters in this record
        //

        ClustersPerPage = (ULONG)LogRecord->LcnsToFollow;

        ASSERT( DirtyPageTable->Table->NumberEntries >= INITIAL_NUMBER_DIRTY_PAGES );

        NtfsInitializeRestartTable( sizeof(DIRTY_PAGE_ENTRY) +
                                      (ClustersPerPage - 1) * sizeof(LCN),
                                    DirtyPageTable->Table->NumberEntries,
                                    &NewDirtyPageTable );

        OldDirtyPage = (PDIRTY_PAGE_ENTRY) NtfsGetFirstRestartTable( DirtyPageTable );

        //
        // Loop to copy table entries
        //

        while (OldDirtyPage) {

            //
            //  Allocate a new dirty page entry.
            //

            PageIndex = NtfsAllocateRestartTableIndex( &NewDirtyPageTable, TRUE );

            //
            //  Get a pointer to the entry we just allocated.
            //

            DirtyPage = GetRestartEntryFromIndex( &NewDirtyPageTable, PageIndex );

            DirtyPage->TargetAttribute = OldDirtyPage->TargetAttribute;
            DirtyPage->LengthOfTransfer = BytesFromClusters( Vcb, ClustersPerPage );
            DirtyPage->LcnsToFollow = ClustersPerPage;
            DirtyPage->Vcn = OldDirtyPage->Vcn;
            ((ULONG)DirtyPage->Vcn) &= ~(ClustersPerPage - 1);
            DirtyPage->OldestLsn = OldDirtyPage->OldestLsn;

            for (i = 0; i < OldDirtyPage->LcnsToFollow; i++) {
                DirtyPage->LcnsForPage[i] = OldDirtyPage->LcnsForPage[i];
            }

            OldDirtyPage = (PDIRTY_PAGE_ENTRY) NtfsGetNextRestartTable( DirtyPageTable, OldDirtyPage );
        }

        //
        //  OldTable is really on the stack so swap the new restart table into it
        //  and free up the old one and the rest of the new restart pointers
        //

        NtfsFreePool( DirtyPageTable->Table );
        DirtyPageTable->Table = NewDirtyPageTable.Table;
        NewDirtyPageTable.Table = NULL;
        NtfsFreeRestartTable( &NewDirtyPageTable );
    }  //  endif table needed to be resized

    //
    //  Update the dirty page entry or create a new one
    //

    if (!FindDirtyPage( DirtyPageTable,
                        LogRecord->TargetAttribute,
                        LogRecord->TargetVcn,
                        &DirtyPage )) {

        //
        //  Allocate a dirty page entry.
        //

        PageIndex = NtfsAllocateRestartTableIndex( DirtyPageTable, TRUE );

        //
        //  Get a pointer to the entry we just allocated.
        //

        DirtyPage = GetRestartEntryFromIndex( DirtyPageTable, PageIndex );

        //
        //  Initialize the dirty page entry.
        //

        DirtyPage->TargetAttribute = LogRecord->TargetAttribute;
        DirtyPage->LengthOfTransfer = BytesFromClusters( Vcb, ClustersPerPage );
        DirtyPage->LcnsToFollow = ClustersPerPage;
        DirtyPage->Vcn = LogRecord->TargetVcn;
        ((ULONG)DirtyPage->Vcn) &= ~(ClustersPerPage - 1);
        DirtyPage->OldestLsn = Lsn;
    }

    //
    //  Copy the Lcns from the log record into the Dirty Page Entry.
    //
    //  *** for different page size support, must somehow make whole routine a loop,
    //  in case Lcns do not fit below.
    //

    for (i = 0; i < (ULONG)LogRecord->LcnsToFollow; i++) {

        DirtyPage->LcnsForPage[((ULONG)LogRecord->TargetVcn) - ((ULONG)DirtyPage->Vcn) + i] =
          LogRecord->LcnsForPage[i];
    }

    DebugTrace( -1, Dbg, ("PageUpdateAnalysis -> VOID\n") );
}


//
//  Internal support routine
//

VOID
OpenAttributesForRestart (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PRESTART_POINTERS DirtyPageTable
    )

/*++

Routine Description:

    This routine is called immediately after the Analysis Pass to open all of
    the attributes in the Open Attribute Table, and preload their Mcbs with
    any run information required to apply updates in the Dirty Page Table.
    With this trick we are effectively doing physical I/O directly to Lbns on
    the disk without relying on any of the file structure to be correct.

Arguments:

    Vcb - Vcb for the volume, for which the Open Attribute Table has been
          initialized.

    DirtyPageTable - Dirty Page table reconstructed from the Analysis Pass.

Return Value:

    None.

--*/

{
    POPEN_ATTRIBUTE_ENTRY OpenEntry;
    POPEN_ATTRIBUTE_ENTRY OldOpenEntry;
    PDIRTY_PAGE_ENTRY DirtyPage;
    ULONG i;
    PSCB TempScb;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("OpenAttributesForRestart:\n") );

    //
    //  First we scan the Open Attribute Table to open all of the attributes.
    //

    OpenEntry = NtfsGetFirstRestartTable( &Vcb->OpenAttributeTable );

    //
    //  Loop to end of table.
    //

    while (OpenEntry != NULL) {

        //
        //  Create the Scb from the data in the Open Attribute Entry.
        //

        TempScb = NtfsCreatePrerestartScb( IrpContext,
                                           Vcb,
                                           &OpenEntry->FileReference,
                                           OpenEntry->AttributeTypeCode,
                                           &OpenEntry->OatData->AttributeName,
                                           OpenEntry->BytesPerIndexBuffer );

        //
        //  If we dynamically allocated a name for this guy, then delete
        //  it here.
        //

        if (OpenEntry->OatData->Overlay.AttributeName != NULL) {
            NtfsFreePool( OpenEntry->OatData->Overlay.AttributeName );
            OpenEntry->OatData->AttributeNamePresent = FALSE;
        }

        OpenEntry->OatData->AttributeName = TempScb->AttributeName;

        //
        //  Now we can lay in the Scb.  We must say the header is initialized
        //  to keep anyone from going to disk yet.
        //

        SetFlag( TempScb->ScbState, SCB_STATE_HEADER_INITIALIZED );

        //
        //  Now store the index in the newly created Scb if its newer.
        //  precalc oldopenentry buts its only good if the scb's attributeindex is nonzero
        //

        OldOpenEntry = GetRestartEntryFromIndex( &Vcb->OpenAttributeTable, TempScb->NonpagedScb->OpenAttributeTableIndex );

        if ((TempScb->NonpagedScb->OpenAttributeTableIndex == 0) ||
            (OldOpenEntry->LsnOfOpenRecord.QuadPart < OpenEntry->LsnOfOpenRecord.QuadPart)) {

            TempScb->NonpagedScb->OpenAttributeTableIndex = GetIndexFromRestartEntry( &Vcb->OpenAttributeTable, OpenEntry );
            TempScb->NonpagedScb->OnDiskOatIndex = OpenEntry->OatData->OnDiskAttributeIndex;

        }

        OpenEntry->OatData->Overlay.Scb = TempScb;

        //
        //  Point to next entry in table, or NULL.
        //

        OpenEntry = NtfsGetNextRestartTable( &Vcb->OpenAttributeTable,
                                             OpenEntry );
    }

    //
    //  Now loop through the dirty page table to extract all of the Vcn/Lcn
    //  Mapping that we have, and insert it into the appropriate Scb.
    //

    DirtyPage = NtfsGetFirstRestartTable( DirtyPageTable );

    //
    //  Loop to end of table.
    //

    while (DirtyPage != NULL) {

        PSCB Scb;

        //
        //  Safety check
        //

        if (!IsRestartIndexWithinTable( Vcb->OnDiskOat, DirtyPage->TargetAttribute )) {

            NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
        }

        OpenEntry = GetRestartEntryFromIndex( Vcb->OnDiskOat,
                                              DirtyPage->TargetAttribute );

        if (IsRestartTableEntryAllocated( OpenEntry )) {

            //
            //  Get the entry from the other table if necessary.
            //

            if (Vcb->RestartVersion == 0) {

                //
                //  Safety check
                //

                if (!IsRestartIndexWithinTable( &Vcb->OpenAttributeTable, ((POPEN_ATTRIBUTE_ENTRY_V0) OpenEntry)->OatIndex )) {

                    NtfsRaiseStatus( IrpContext, STATUS_DISK_CORRUPT_ERROR, NULL, NULL );
                }

                OpenEntry = GetRestartEntryFromIndex( &Vcb->OpenAttributeTable,
                                                      ((POPEN_ATTRIBUTE_ENTRY_V0) OpenEntry)->OatIndex );
            }

            Scb = OpenEntry->OatData->Overlay.Scb;

            //
            //  Loop to add the allocated Vcns.
            //

            for (i = 0; i < DirtyPage->LcnsToFollow; i++) {

                VCN Vcn;
                LONGLONG Size;

                Vcn = DirtyPage->Vcn + i;
                Size = LlBytesFromClusters( Vcb, Vcn + 1);

                //
                //  Add this run to the Mcb if the Vcn has not been deleted,
                //  and it is not for the fixed part of the Mft.
                //

                if ((DirtyPage->LcnsForPage[i] != 0)

                        &&

                    (NtfsSegmentNumber( &OpenEntry->FileReference ) > MASTER_FILE_TABLE2_NUMBER ||
                     (Size >= ((VOLUME_DASD_NUMBER + 1) * Vcb->BytesPerFileRecordSegment)) ||
                     (OpenEntry->AttributeTypeCode != $DATA))) {


                    if (!NtfsAddNtfsMcbEntry( &Scb->Mcb,
                                         Vcn,
                                         DirtyPage->LcnsForPage[i],
                                         (LONGLONG)1,
                                         FALSE )) {

                        //
                        //  Replace with new entry if collision comes from
                        //  the newest attribute
                        //

                        if (DirtyPage->TargetAttribute == Scb->NonpagedScb->OnDiskOatIndex) {
#if DBG
                            BOOLEAN Result;
#endif
                            NtfsRemoveNtfsMcbEntry( &Scb->Mcb,
                                                    Vcn,
                                                    1 );
#if DBG
                            Result =
#endif
                            NtfsAddNtfsMcbEntry( &Scb->Mcb,
                                                 Vcn,
                                                 DirtyPage->LcnsForPage[i],
                                                 (LONGLONG)1,
                                                 FALSE );
#if DBG
                            ASSERT( Result );
#endif
                        }
                    }

                    if (Size > Scb->Header.AllocationSize.QuadPart) {

                        Scb->Header.AllocationSize.QuadPart =
                        Scb->Header.FileSize.QuadPart =
                        Scb->Header.ValidDataLength.QuadPart = Size;
                    }
                }
            }
        }

        //
        //  Point to next entry in table, or NULL.
        //

        DirtyPage = NtfsGetNextRestartTable( DirtyPageTable,
                                             DirtyPage );
    }

    //
    //  Now we know how big all of the files have to be, and recorded that in the
    //  Scb.  We have not created streams for any of these Scbs yet, except for
    //  the Mft, Mft2 and LogFile.  The size should be correct for Mft2 and LogFile,
    //  but we have to inform the Cache Manager here of the final size of the Mft.
    //

    TempScb = Vcb->MftScb;

    ASSERT( !FlagOn( TempScb->ScbPersist, SCB_PERSIST_USN_JOURNAL ) );
    CcSetFileSizes( TempScb->FileObject,
                    (PCC_FILE_SIZES)&TempScb->Header.AllocationSize );

    DebugTrace( -1, Dbg, ("OpenAttributesForRestart -> VOID\n") );
}


NTSTATUS
NtfsCloseAttributesFromRestart (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    )

/*++

Routine Description:

    This routine is called at the end of a Restart to close any attributes
    that had to be opened for Restart purposes.  Actually what this does is
    delete all of the internal streams so that the attributes will eventually
    go away.  This routine cannot raise because it is called in the finally of
    MountVolume.  Raising in the main line path will leave the global resource
    acquired.

Arguments:

    Vcb - Vcb for the volume, for which the Open Attribute Table has been
          initialized.

Return Value:

    NTSTATUS - STATUS_SUCCESS if all of the I/O completed successfully.  Otherwise
        the error in the IrpContext or the first I/O error.

--*/

{
    NTSTATUS Status = STATUS_SUCCESS;
    POPEN_ATTRIBUTE_ENTRY OpenEntry;

    PAGED_CODE();

    DebugTrace( +1, Dbg, ("CloseAttributesForRestart:\n") );

    //
    //  Set this flag again now, so we do not try to flush out the holes!
    //

    SetFlag(Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS);

    //
    //  Remove duplicate Scbs - in rare case no dirty pages the scb's were never
    //  opened at all
    //

   OpenEntry = NtfsGetFirstRestartTable( &Vcb->OpenAttributeTable );
   while (OpenEntry != NULL) {
       if ((OpenEntry->OatData->Overlay.Scb != NULL) &&
           (!(OpenEntry->OatData->AttributeNamePresent)) &&
           (OpenEntry->OatData->Overlay.Scb->NonpagedScb->OpenAttributeTableIndex !=
            GetIndexFromRestartEntry( &Vcb->OpenAttributeTable, OpenEntry ))) {

           OpenEntry->OatData->Overlay.Scb = NULL;
       }

       //
       //  Point to next entry in table, or NULL.
       //

       OpenEntry = NtfsGetNextRestartTable( &Vcb->OpenAttributeTable,
                                            OpenEntry );
   }

    //
    //  Scan the Open Attribute Table to close all of the open attributes.
    //

    OpenEntry = NtfsGetFirstRestartTable( &Vcb->OpenAttributeTable );

    //
    //  Loop to end of table.
    //

    while (OpenEntry != NULL) {

        IO_STATUS_BLOCK IoStatus;
        PSCB Scb;

        if (OpenEntry->OatData->AttributeNamePresent) {

            NtfsFreePool( OpenEntry->OatData->Overlay.AttributeName );
            OpenEntry->OatData->Overlay.AttributeName = NULL;
        }

        Scb = OpenEntry->OatData->Overlay.Scb;

        //
        //  We clean up the Scb only if it exists and this is index in the
        //  OpenAttributeTable that this Scb actually refers to.
        //  If this Scb has several entries in the table, we nulled out older
        //  duplicates in the loop above
        //

        if (Scb != NULL) {

            FILE_REFERENCE FileReference;

            //
            //  Only shut it down if it is not the Mft or its mirror.
            //

            FileReference = Scb->Fcb->FileReference;
            if (NtfsSegmentNumber( &FileReference ) > LOG_FILE_NUMBER ||
                (Scb->AttributeTypeCode != $DATA)) {

                //
                //  Now flush the file.  It is important to call the
                //  same routine the Lazy Writer calls, so that write.c
                //  will not decide to update file size for the attribute,
                //  since we really are working here with the wrong size.
                //

                NtfsAcquireScbForLazyWrite( (PVOID)Scb, TRUE );
                CcFlushCache( &Scb->NonpagedScb->SegmentObject, NULL, 0, &IoStatus );
                NtfsReleaseScbFromLazyWrite( (PVOID)Scb );

                if (NT_SUCCESS( Status )) {

                    if (!NT_SUCCESS( IrpContext->ExceptionStatus )) {

                        Status = IrpContext->ExceptionStatus;

                    } else if (!NT_SUCCESS( IoStatus.Status )) {

                        Status = FsRtlNormalizeNtstatus( IoStatus.Status,
                                                         STATUS_UNEXPECTED_IO_ERROR );
                    }
                }

                //
                //  If there is an Scb and it is not for a system file, then delete
                //  the stream file so it can eventually go away.
                //

                NtfsUninitializeNtfsMcb( &Scb->Mcb );
                NtfsInitializeNtfsMcb( &Scb->Mcb,
                                       &Scb->Header,
                                       &Scb->McbStructs,
                                       FlagOn( Scb->Fcb->FcbState,
                                               FCB_STATE_PAGING_FILE ) ? NonPagedPool :
                                                                         PagedPool );

                //
                //  Now that we are restarted, we must clear the header state
                //  so that we will go look up the sizes and load the Scb
                //  from disk.
                //

                ClearFlag( Scb->ScbState, SCB_STATE_HEADER_INITIALIZED |
                                          SCB_STATE_FILE_SIZE_LOADED );

                //
                //  Show the indexed portions are "uninitialized".
                //

                if (Scb->AttributeTypeCode == $INDEX_ALLOCATION) {

                    Scb->ScbType.Index.BytesPerIndexBuffer = 0;
                }

                //
                //  If this Fcb is past the log file then remove it from the
                //  Fcb table.
                //

                if ((NtfsSegmentNumber( &FileReference ) > LOG_FILE_NUMBER) &&
                    FlagOn( Scb->Fcb->FcbState, FCB_STATE_IN_FCB_TABLE )) {

#if __NDAS_NTFS_SECONDARY__
					NtfsDeleteFcbTableEntry( Scb->Fcb, Scb->Fcb->Vcb, FileReference );
#else
                    NtfsDeleteFcbTableEntry( Scb->Fcb->Vcb, FileReference );
#endif
                    ClearFlag( Scb->Fcb->FcbState, FCB_STATE_IN_FCB_TABLE );
                }

                //
                //  If the Scb is a root index scb then change the type to INDEX_SCB.
                //  Otherwise the teardown routines will skip it.
                //

                if (SafeNodeType( Scb ) == NTFS_NTC_SCB_ROOT_INDEX) {

                    SafeNodeType( Scb ) = NTFS_NTC_SCB_INDEX;
                }

                if (Scb->FileObject != NULL) {

                    NtfsDeleteInternalAttributeStream( Scb, TRUE, FALSE );
                } else {

                    //
                    //  Make sure the Scb is acquired exclusively.
                    //

                    NtfsAcquireExclusiveFcb( IrpContext, Scb->Fcb, NULL, ACQUIRE_NO_DELETE_CHECK );
                    NtfsTeardownStructures( IrpContext,
                                            Scb,
                                            NULL,
                                            FALSE,
                                            0,
                                            NULL );
                }
            }

        } else {

            //
            //  We want to check whether there is also an entry in the other table.
            //

            if (Vcb->RestartVersion == 0) {

                NtfsFreeRestartTableIndex( Vcb->OnDiskOat, OpenEntry->OatData->OnDiskAttributeIndex );
            }

            NtfsFreeOpenAttributeData( OpenEntry->OatData );

            NtfsFreeRestartTableIndex( &Vcb->OpenAttributeTable,
                                       GetIndexFromRestartEntry( &Vcb->OpenAttributeTable,
                                                                 OpenEntry ));
        }

        //
        //  Point to next entry in table, or NULL.
        //

        OpenEntry = NtfsGetNextRestartTable( &Vcb->OpenAttributeTable,
                                             OpenEntry );
    }

    //
    //  Resume normal operation.
    //

    ClearFlag(Vcb->VcbState, VCB_STATE_RESTART_IN_PROGRESS);

    DebugTrace( -1, Dbg, ("CloseAttributesForRestart -> %08lx\n", Status) );

    return Status;
}

