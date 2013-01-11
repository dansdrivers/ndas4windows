NtfsFlags, NDAS_NTFS_FCB_FLAG_SECONDARY)) {

		SecondaryReleaseResourceLite( NULL, &volDo->Resource );
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

#if __NDAS_NTFS_SECONDARY__
	PVOLUME_DEVICE_OBJECT	volDo = CONTAINING_RECORD( Scb->Vcb, VOLUME_DEVICE_OBJECT, Vcb );
#endif

    ASSERT_SCB(Scb);

    PAGED_CODE();

    //
    //  Clear the toplevel at this point, if we set it above.
    //

    if ((((ULONG_PTR) IoGetTopLevelIr