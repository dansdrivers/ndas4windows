16 );

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

        DebugUnwind( NtfsContinueIndexEnumeration );

        //
        //  Remember if we are not returning anything, to save work later.
        //

        if (!Result && (Ccb->IndexEntry != NULL)) {
            Ccb->IndexEntry->Length = 0;
        }
    }

    DebugTrace( 0, Dbg, ("*IndexEntry < %08lx\n", *IndexEntry) );
    DebugTrace( -1, Dbg, ("NtfsContinueIndexEnumeration -> %08lx\n", Result) );

    return Result;
}


PFILE_NAME
NtfsRetrieveOtherFileName (
    IN PIRP_CONTEXT IrpContext,
    IN PCCB Ccb,
    IN PSCB Scb,
    IN PINDEX_ENTRY IndexEntry,
    IN OUT PINDEX_CONTEXT OtherContext,
    IN PFCB AcquiredFcb OPTIONAL,
    OUT PBOOLEAN SynchronizationError
    )

/*++

Routine Description:

    This routine may be called to retrieve the other index entry for a given
    index entry.  I.e., for an input Ntfs-only index entry it will find the
    Dos-only index entry for the same file referenced, or visa versa.  This
    is a routine which clearly is relevant only to file name indices, but it
    is located here because it uses the Index Context in the Ccb.

    The idea is that nearly always the other name for a given index entry will
    be very near to the given name.

    This routine first scans the leaf index buffer described by the index
    context for the Dos name.  If this fails, this routine attempts to look
    the other name up in the index.  Currently there will always be a Dos name,
    however if one does not exist, we treat that as benign, and simply return
    FALSE.


Arguments:

    Ccb - Pointer to the Ccb for this enumeration.

    Scb - Supplies the Scb for the index.

    IndexEntry - Suppliess a pointer to an index entry, for which the Dos name
                 is to be retrieved.


    OtherContext - Must be initialized on input, and subsequently cleaned up
                   by the caller after the information has been extracted from
                   the other index entry.

    AcquiredFcb - An Fcb which has been acquired so that its file record may be
                  read

    SynchronizationError - Returns TRUE if no file name is being returned because
                           of an error trying to acquire an Fcb to read its file
                           record.

Return Value:

    Pointer to the other desired file name.

--*/

{
    PINDEX_CONTEXT IndexContext;
    PINDEX_HEADER IndexHeader;
    PINDEX_ENTRY IndexTemp, IndexLast;
    PINDEX_LOOKUP_STACK Top;

    struct {
        FILE_NAME FileName;
        WCHAR NameBuffer[2];
    }Other