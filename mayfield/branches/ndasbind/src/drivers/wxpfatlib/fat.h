 IN PFCB Fcb,
    IN OUT PUNICODE_STRING Lfn
    );

VOID
FatSetFullFileNameInFcb (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb
    );

VOID
FatSetFullNameInFcb(
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PUNICODE_STRING FinalName
    );

VOID
FatUnicodeToUpcaseOem (
    IN PIRP_CONTEXT IrpContext,
    IN POEM_STRING OemString,
    IN PUNICODE_STRING UnicodeString
    );

VOID
FatSelectNames (
    IN PIRP_CONTEXT IrpContext,
    IN PDCB Parent,
    IN POEM_STRING OemName,
    IN PUNICODE_STRING UnicodeName,
    IN OUT POEM_STRING ShortName,
    IN PUNICODE_STRING SuggestedShortName OPTIONAL,
    IN OUT BOOLEAN *AllLowerComponent,
    IN OUT BOOLEAN *AllLowerExtension,
    IN OUT BOOLEAN *CreateLfn
    );

VOID
FatEvaluateNameCase (
    IN PIRP_CONTEXT IrpContext,
    IN PUNICODE_STRING Name,
    IN OUT BOOLEAN *AllLowerComponent,
    IN OUT BOOLEAN *AllLowerExtension,
    IN OUT BOOLEAN *CreateLfn
    );

BOOLEAN
FatSpaceInName (
    IN PIRP_CONTEXT IrpContext,
    IN PUNICODE_STRING UnicodeName
    );


//
//  Resources support routines/macros, implemented in ResrcSup.c
//
//  The following routines/macros are used for gaining shared and exclusive
//  access to the global/vcb data structures.  The routines are implemented
//  in ResrcSup.c.  There is a global resources that everyone tries to take
//  out shared to do their work, with the exception of mount/dismount which
//  take out the global resource exclusive.  All other resources only work
//  on their individual item.  For example, an Fcb resource does not take out
//  a Vcb resource.  But the way the file system is structured we know
//  that when we are processing an Fcb other threads cannot be trying to remove
//  or alter the Fcb, so we do not need to acquire the Vcb.
//
//  The procedures/macros are:
//
//          Macro          FatData    Vcb        Fcb         Subsequent macros
//
//  AcquireExclusiveGlobal Read/Write None       None        ReleaseGlobal
//
//  AcquireSharedGlobal    Read       None       None        ReleaseGlobal
//
//  AcquireExclusiveVcb    Read       Read/Write None        ReleaseVcb
//
//  AcquireSharedVcb       Read       Read       None        ReleaseVcb
//
//  AcquireExclusiveFcb    Read       None       Read/Write  ConvertToSharFcb
//                                                           ReleaseFcb
//
//  AcquireSharedFcb       Read       None       Read        ReleaseFcb
//
//  ConvertToSharedFcb     Read       None       Read        ReleaseFcb
//
//  ReleaseGlobal
//
//  ReleaseVcb
//
//  ReleaseFcb
//

//
//  FINISHED
//  FatAcquireExclusiveGlobal (
//      IN PIRP_CONTEXT IrpContext
//      );
//
//  FINISHED
//  FatAcquireSharedGlobal (
//      IN PIRP_CONTEXT IrpContext
//      );
//

#define FatAcquireExclusiveGlobal(IRPCONTEXT) (                                                                \
    ExAcquireResourceExclusiveLite( &FatData.Resource, BooleanFlagOn((IRPCONTEXT)->Flags, IRP_CONTEXT_FLAG_WAIT) ) \
)

#define FatAcquireSharedGlobal(IRPCONTEXT) (                                                                \
    ExAcquireResourceSharedLite( &FatData.Resource, BooleanFlagOn((IRPCONTEXT)->Flags, IRP_CONTEXT_FLAG_WAIT) ) \
)

//
//  The following macro must only be called when Wait is TRUE!
//
//  FatAcquireExclusiveVolume (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb
//      );
//
//  FatReleaseVolume (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb
//      );
//

#define FatAcquireExclusiveVolume(IRPCONTEXT,VCB) {                                     \
    PFCB Fcb = NULL;                                                                    \
    ASSERT(FlagOn((IRPCONTEXT)->Flags, IRP_CONTEXT_FLAG_WAIT));                         \
    (VOID)FatAcquireExclusiveVcb( (IRPCONTEXT), (VCB) );                                \
    while ( (Fcb = FatGetNextFcbBottomUp((IRPCONTEXT), Fcb, (VCB)->RootDcb)) != NULL) { \
        (VOID)FatAcquireExclusiveFcb((IRPCONTEXT), Fcb );                               \
    }                                                                                   \
}

#define FatReleaseVolume(IRPCONTEXT,VCB) {                                              \
    PFCB Fcb = NULL;                                                                    \
    ASSERT(FlagOn((IRPCONTEXT)->Flags, IRP_CONTEXT_FLAG_WAIT));                         \
    while ( (Fcb = FatGetNextFcbBottomUp((IRPCONTEXT), Fcb, (VCB)->RootDcb)) != NULL) { \
        (VOID)ExReleaseResourceLite( Fcb->Header.Resource );                                \
    }                                                                                   \
    FatReleaseVcb((IRPCONTEXT), (VCB));                                                 \
}

//
//  These macros can be used to determine what kind of FAT we have for an
//  initialized Vcb.  It is somewhat more elegant to use these (visually).
//

#define FatIsFat32(VCB) ((BOOLEAN)((VCB)->AllocationSupport.FatIndexBitSize == 32))
#define FatIsFat16(VCB) ((BOOLEAN)((VCB)->AllocationSupport.FatIndexBitSize == 16))
#define FatIsFat12(VCB) ((BOOLEAN)((VCB)->AllocationSupport.FatIndexBitSize == 12))

FINISHED
FatAcquireExclusiveVcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

FINISHED
FatAcquireSharedVcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

FINISHED
FatAcquireExclusiveFcb (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb
    );

FINISHED
FatAcquireSharedFcb (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb
    );

FINISHED
FatAcquireSharedFcbWaitForEx (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb
    );

#define FatVcbAcquiredExclusive(IRPCONTEXT,VCB) (                   \
    ExIsResourceAcquiredExclusiveLite(&(VCB)->Resource)  ||             \
    ExIsResourceAcquiredExclusiveLite(&FatData.Resource)                \
)

#define FatFcbAcquiredShared(IRPCONTEXT,FCB) (                      \
    ExIsResourceAcquiredSharedLite((FCB)->Header.Resource)              \
)

#define FatAcquireDirectoryFileMutex(VCB) {                         \
    ASSERT(KeAreApcsDisabled());                                    \
    ExAcquireFastMutexUnsafe(&(VCB)->DirectoryFileCreationMutex);   \
}

#define FatReleaseDirectoryFileMutex(VCB) {                         \
    ASSERT(KeAreApcsDisabled());                                    \
    ExReleaseFastMutexUnsafe(&(VCB)->DirectoryFileCreationMutex);   \
}

//
//  The following are cache manager call backs

BOOLEAN
FatAcquireVolumeForClose (
    IN PVOID Vcb,
    IN BOOLEAN Wait
    );

VOID
FatReleaseVolumeFromClose (
    IN PVOID Vcb
    );

BOOLEAN
FatAcquireFcbForLazyWrite (
    IN PVOID Null,
    IN BOOLEAN Wait
    );

VOID
FatReleaseFcbFromLazyWrite (
    IN PVOID Null
    );

BOOLEAN
FatAcquireFcbForReadAhead (
    IN PVOID Null,
    IN BOOLEAN Wait
    );

VOID
FatReleaseFcbFromReadAhead (
    IN PVOID Null
    );

NTSTATUS
FatAcquireForCcFlush (
    IN PFILE_OBJECT FileObject,
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
FatReleaseForCcFlush (
    IN PFILE_OBJECT FileObject,
    IN PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
FatNoOpAcquire (
    IN PVOID Fcb,
    IN BOOLEAN Wait
    );

VOID
FatNoOpRelease (
    IN PVOID Fcb
    );

//
//  VOID
//  FatConvertToSharedFcb (
//      IN PIRP_CONTEXT IrpContext,
//      IN PFCB Fcb
//      );
//

#define FatConvertToSharedFcb(IRPCONTEXT,Fcb) {             \
    ExConvertExclusiveToSharedLite( (Fcb)->Header.Resource );   \
    }

//
//  VOID
//  FatReleaseGlobal (
//      IN PIRP_CONTEXT IrpContext
//      );
//
//  VOID
//  FatReleaseVcb (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb
//      );
//
//  VOID
//  FatReleaseFcb (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb
//      );
//

#define FatDeleteResource(RESRC) {                  \
    ExDeleteResourceLite( (RESRC) );                    \
}

#define FatReleaseGlobal(IRPCONTEXT) {              \
    ExReleaseResourceLite( &(FatData.Resource) );       \
    }

#define FatReleaseVcb(IRPCONTEXT,Vcb) {             \
    ExReleaseResourceLite( &((Vcb)->Resource) );        \
    }

#define FatReleaseFcb(IRPCONTEXT,Fcb) {             \
    ExReleaseResourceLite( (Fcb)->Header.Resource );    \
    }


//
//  In-memory structure support routine, implemented in StrucSup.c
//

VOID
FatInitializeVcb (
    IN PIRP_CONTEXT IrpContext,
    IN OUT PVCB Vcb,
    IN PDEVICE_OBJECT TargetDeviceObject,
    IN PVPB Vpb,
    IN PDEVICE_OBJECT FsDeviceObject
    );
VOID
FatDeleteVcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

VOID
FatCreateRootDcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

PFCB
FatCreateFcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PDCB ParentDcb,
    IN ULONG LfnOffsetWithinDirectory,
    IN ULONG DirentOffsetWithinDirectory,
    IN PDIRENT Dirent,
    IN PUNICODE_STRING Lfn OPTIONAL,
    IN BOOLEAN IsPagingFile,
    IN BOOLEAN SingleResource
    );

PDCB
FatCreateDcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PDCB ParentDcb,
    IN ULONG LfnOffsetWithinDirectory,
    IN ULONG DirentOffsetWithinDirectory,
    IN PDIRENT Dirent,
    IN PUNICODE_STRING Lfn OPTIONAL
    );

VOID
FatDeleteFcb_Real (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb
    );

#ifdef FASTFATDBG
#define FatDeleteFcb(IRPCONTEXT,FCB) {     \
    FatDeleteFcb_Real((IRPCONTEXT),(FCB)); \
    (FCB) = NULL;                          \
}
#else
#define FatDeleteFcb(IRPCONTEXT,VCB) {     \
    FatDeleteFcb_Real((IRPCONTEXT),(VCB)); \
}
#endif // FASTFAT_DBG

PCCB
FatCreateCcb (
    IN PIRP_CONTEXT IrpContext
    );
    
VOID
FatDeallocateCcbStrings(
        IN PCCB Ccb
        );
        
VOID
FatDeleteCcb_Real (
    IN PIRP_CONTEXT IrpContext,
    IN PCCB Ccb
    );

#ifdef FASTFATDBG
#define FatDeleteCcb(IRPCONTEXT,CCB) {     \
    FatDeleteCcb_Real((IRPCONTEXT),(CCB)); \
    (CCB) = NULL;                          \
}
#else
#define FatDeleteCcb(IRPCONTEXT,VCB) {     \
    FatDeleteCcb_Real((IRPCONTEXT),(VCB)); \
}
#endif // FASTFAT_DBG

PIRP_CONTEXT
FatCreateIrpContext (
    IN PIRP Irp,
    IN BOOLEAN Wait
    );

VOID
FatDeleteIrpContext_Real (
    IN PIRP_CONTEXT IrpContext
    );

#ifdef FASTFATDBG
#define FatDeleteIrpContext(IRPCONTEXT) {   \
    FatDeleteIrpContext_Real((IRPCONTEXT)); \
    (IRPCONTEXT) = NULL;                    \
}
#else
#define FatDeleteIrpContext(IRPCONTEXT) {   \
    FatDeleteIrpContext_Real((IRPCONTEXT)); \
}
#endif // FASTFAT_DBG

PFCB
FatGetNextFcbTopDown (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PFCB TerminationFcb
    );

PFCB
FatGetNextFcbBottomUp (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PFCB TerminationFcb
    );

//
//  These two macros just make the code a bit cleaner.
//

#define FatGetFirstChild(DIR) ((PFCB)(                          \
    IsListEmpty(&(DIR)->Specific.Dcb.ParentDcbQueue) ? NULL :   \
    CONTAINING_RECORD((DIR)->Specific.Dcb.ParentDcbQueue.Flink, \
                      DCB,                                      \
                      ParentDcbLinks.Flink)))

#define FatGetNextSibling(FILE) ((PFCB)(                     \
    &(FILE)->ParentDcb->Specific.Dcb.ParentDcbQueue.Flink == \
    (PVOID)(FILE)->ParentDcbLinks.Flink ? NULL :             \
    CONTAINING_RECORD((FILE)->ParentDcbLinks.Flink,          \
                      FCB,                                   \
                      ParentDcbLinks.Flink)))

BOOLEAN
FatCheckForDismount (
    IN PIRP_CONTEXT IrpContext,
    PVCB Vcb,
    IN BOOLEAN Force
    );

VOID
FatConstructNamesInFcb (
    IN PIRP_CONTEXT IrpContext,
    PFCB Fcb,
    PDIRENT Dirent,
    PUNICODE_STRING Lfn OPTIONAL
    );

VOID
FatCheckFreeDirentBitmap (
    IN PIRP_CONTEXT IrpContext,
    IN PDCB Dcb
    );

ULONG
FatVolumeUncleanCount (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

VOID
FatPreallocateCloseContext (
    );

//  VOID
//  FatAllocateCloseContext (
//     )
//
//    This routine allocates a close context, presumeably on behalf
//    of a fileobject which does not have a structure we can embed one
//    in.
    
#define FatAllocateCloseContext() (PCLOSE_CONTEXT)                                       \
                                  ExInterlockedPopEntrySList( &FatCloseContextSList,     \
                                                              &FatData.GeneralSpinLock )
                                

//
//  BOOLEAN
//  FatIsRawDevice (
//      IN PIRP_CONTEXT IrpContext,
//      IN NTSTATUS Status
//      );
//

#define FatIsRawDevice(IC,S) (          \
    ((S) == STATUS_DEVICE_NOT_READY) || \
    ((S) == STATUS_NO_MEDIA_IN_DEVICE)  \
)


//
//  Routines to support managing file names Fcbs and Dcbs.
//  Implemented in SplaySup.c
//

VOID
FatInsertName (
    IN PIRP_CONTEXT IrpContext,
    IN PRTL_SPLAY_LINKS *RootNode,
    IN PFILE_NAME_NODE Name
    );

VOID
FatRemoveNames (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb
    );

PFCB
FatFindFcb (
    IN PIRP_CONTEXT IrpContext,
    IN OUT PRTL_SPLAY_LINKS *RootNode,
    IN PSTRING Name,
    OUT PBOOLEAN FileNameDos OPTIONAL
    );

BOOLEAN
FatIsHandleCountZero (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

//
//  Time conversion support routines, implemented in TimeSup.c
//

BOOLEAN
FatNtTimeToFatTime (
    IN PIRP_CONTEXT IrpContext,
    IN PLARGE_INTEGER NtTime,
    IN BOOLEAN Rounding,
    OUT PFAT_TIME_STAMP FatTime,
    OUT OPTIONAL PCHAR TenMsecs
    );

LARGE_INTEGER
FatFatTimeToNtTime (
    IN PIRP_CONTEXT IrpContext,
    IN FAT_TIME_STAMP FatTime,
    IN UCHAR TenMilliSeconds
    );

LARGE_INTEGER
FatFatDateToNtTime (
    IN PIRP_CONTEXT IrpContext,
    IN FAT_DATE FatDate
    );

FAT_TIME_STAMP
FatGetCurrentFatTime (
    IN PIRP_CONTEXT IrpContext
    );


//
//  Low level verification routines, implemented in VerfySup.c
//
//  The first routine is called to help process a verify IRP.  Its job is
//  to walk every Fcb/Dcb and mark them as need to be verified.
//
//  The other routines are used by every dispatch routine to verify that
//  an Vcb/Fcb/Dcb is still good.  The routine walks as much of the opened
//  file/directory tree as necessary to make sure that the path is still valid.
//  The function result indicates if the procedure needed to block for I/O.
//  If the structure is bad the procedure raise the error condition
//  STATUS_FILE_INVALID, otherwise they simply return to their caller
//

typedef enum _FAT_VOLUME_STATE {
    VolumeClean,
    VolumeDirty,
    VolumeDirtyWithSurfaceTest
} FAT_VOLUME_STATE, *PFAT_VOLUME_STATE;

VOID
FatMarkFcbCondition (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN FCB_CONDITION FcbCondition,
    IN BOOLEAN Recursive
    );

VOID
FatVerifyVcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

VOID
FatVerifyFcb (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb
    );

VOID
FatCleanVolumeDpc (
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

VOID
FatMarkVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN FAT_VOLUME_STATE VolumeState
    );

VOID
FatFspMarkVolumeDirtyWithRecover (
    PVOID Parameter
    );

VOID
FatCheckDirtyBit (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

VOID
FatQuickVerifyVcb (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

VOID
FatVerifyOperationIsLegal (
    IN PIRP_CONTEXT IrpContext
    );

NTSTATUS
FatPerformVerify (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PDEVICE_OBJECT Device
    );


//
//  Work queue routines for posting and retrieving an Irp, implemented in
//  workque.c
//

VOID
FatOplockComplete (
    IN PVOID Context,
    IN PIRP Irp
    );

VOID
FatPrePostIrp (
    IN PVOID Context,
    IN PIRP Irp
    );

VOID
FatAddToWorkque (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatFsdPostRequest (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

//
//  Miscellaneous support routines
//

//
//  This macro returns TRUE if a flag in a set of flags is on and FALSE
//  otherwise.  It is followed by two macros for setting and clearing
//  flags
//

//#ifndef BooleanFlagOn
//#define BooleanFlagOn(Flags,SingleFlag) ((BOOLEAN)((((Flags) & (SingleFlag)) != 0)))
//#endif

//#ifndef SetFlag
//#define SetFlag(Flags,SingleFlag) { \
//    (Flags) |= (SingleFlag);        \
//}
//#endif

//#ifndef ClearFlag
//#define ClearFlag(Flags,SingleFlag) { \
//    (Flags) &= ~(SingleFlag);         \
//}
//#endif

//
//      ULONG
//      PtrOffset (
//          IN PVOID BasePtr,
//          IN PVOID OffsetPtr
//          );
//

#define PtrOffset(BASE,OFFSET) ((ULONG)((ULONG_PTR)(OFFSET) - (ULONG_PTR)(BASE)))

//
//  This macro takes a pointer (or ulong) and returns its rounded up word
//  value
//

#define WordAlign(Ptr) (                \
    ((((ULONG)(Ptr)) + 1) & 0xfffffffe) \
    )

//
//  This macro takes a pointer (or ulong) and returns its rounded up longword
//  value
//

#define LongAlign(Ptr) (                \
    ((((ULONG)(Ptr)) + 3) & 0xfffffffc) \
    )

//
//  This macro takes a pointer (or ulong) and returns its rounded up quadword
//  value
//

#define QuadAlign(Ptr) (                \
    ((((ULONG)(Ptr)) + 7) & 0xfffffff8) \
    )

//
//  The following types and macros are used to help unpack the packed and
//  misaligned fields found in the Bios parameter block
//

typedef union _UCHAR1 {
    UCHAR  Uchar[1];
    UCHAR  ForceAlignment;
} UCHAR1, *PUCHAR1;

typedef union _UCHAR2 {
    UCHAR  Uchar[2];
    USHORT ForceAlignment;
} UCHAR2, *PUCHAR2;

typedef union _UCHAR4 {
    UCHAR  Uchar[4];
    ULONG  ForceAlignment;
} UCHAR4, *PUCHAR4;

//
//  This macro copies an unaligned src byte to an aligned dst byte
//

#define CopyUchar1(Dst,Src) {                                \
    *((UCHAR1 *)(Dst)) = *((UNALIGNED UCHAR1 *)(Src)); \
    }

//
//  This macro copies an unaligned src word to an aligned dst word
//

#define CopyUchar2(Dst,Src) {                                \
    *((UCHAR2 *)(Dst)) = *((UNALIGNED UCHAR2 *)(Src)); \
    }

//
//  This macro copies an unaligned src longword to an aligned dsr longword
//

#define CopyUchar4(Dst,Src) {                                \
    *((UCHAR4 *)(Dst)) = *((UNALIGNED UCHAR4 *)(Src)); \
    }

#define CopyU4char(Dst,Src) {                                \
    *((UNALIGNED UCHAR4 *)(Dst)) = *((UCHAR4 *)(Src)); \
    }

//
//  VOID
//  FatNotifyReportChange (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb,
//      IN PFCB Fcb,
//      IN ULONG Filter,
//      IN ULONG Action
//      );
//

#define FatNotifyReportChange(I,V,F,FL,A) {                                                         \
    if ((F)->FullFileName.Buffer == NULL) {                                                         \
        FatSetFullFileNameInFcb((I),(F));                                                           \
    }                                                                                               \
    ASSERT( (F)->FullFileName.Length != 0 );                                                        \
    ASSERT( (F)->FinalNameLength != 0 );                                                            \
    ASSERT( (F)->FullFileName.Length > (F)->FinalNameLength );                                      \
    ASSERT( (F)->FullFileName.Buffer[((F)->FullFileName.Length - (F)->FinalNameLength)/sizeof(WCHAR) - 1] == L'\\' ); \
    FsRtlNotifyFullReportChange( (V)->NotifySync,                                                   \
                                 &(V)->DirNotifyList,                                               \
                                 (PSTRING)&(F)->FullFileName,                                       \
                                 (USHORT) ((F)->FullFileName.Length -                               \
                                           (F)->FinalNameLength),                                   \
                                 (PSTRING)NULL,                                                     \
                                 (PSTRING)NULL,                                                     \
                                 (ULONG)FL,                                                         \
                                 (ULONG)A,                                                          \
                                 (PVOID)NULL );                                                     \
}


//
//  The FSD Level dispatch routines.   These routines are called by the
//  I/O system via the dispatch table in the Driver Object.
//
//  They each accept as input a pointer to a device object (actually most
//  expect a volume device object, with the exception of the file system
//  control function which can also take a file system device object), and
//  a pointer to the IRP.  They either perform the function at the FSD level
//  or post the request to the FSP work queue for FSP level processing.
//

NTSTATUS
FatFsdCleanup (                         //  implemented in Cleanup.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FatFsdClose (                           //  implemented in Close.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FatFsdCreate (                          //  implemented in Create.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FatFsdDeviceControl (                   //  implemented in DevCtrl.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FatFsdDirectoryControl (                //  implemented in DirCtrl.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FatFsdQueryEa (                         //  implemented in Ea.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FatFsdSetEa (                           //  implemented in Ea.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FatFsdQueryInformation (                //  implemented in FileInfo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FatFsdSetInformation (                  //  implemented in FileInfo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FatFsdFlushBuffers (                    //  implemented in Flush.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FatFsdFileSystemControl (               //  implemented in FsCtrl.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FatFsdLockControl (                     //  implemented in LockCtrl.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FatFsdPnp (                            //  implemented in Pnp.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FatFsdRead (                            //  implemented in Read.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FatFsdShutdown (                        //  implemented in Shutdown.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FatFsdQueryVolumeInformation (          //  implemented in VolInfo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FatFsdSetVolumeInformation (            //  implemented in VolInfo.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FatFsdWrite (                           //  implemented in Write.c
    IN PVOLUME_DEVICE_OBJECT VolumeDeviceObject,
    IN PIRP Irp
    );

//
//  The following macro is used to determine if an FSD thread can block
//  for I/O or wait for a resource.  It returns TRUE if the thread can
//  block and FALSE otherwise.  This attribute can then be used to call
//  the FSD & FSP common work routine with the proper wait value.
//

#define CanFsdWait(IRP) IoIsOperationSynchronous(Irp)


//
//  The FSP level dispatch/main routine.  This is the routine that takes
//  IRP's off of the work queue and calls the appropriate FSP level
//  work routine.
//

VOID
FatFspDispatch (                        //  implemented in FspDisp.c
    IN PVOID Context
    );

//
//  The following routines are the FSP work routines that are called
//  by the preceding FatFspDispath routine.  Each takes as input a pointer
//  to the IRP, perform the function, and return a pointer to the volume
//  device object that they just finished servicing (if any).  The return
//  pointer is then used by the main Fsp dispatch routine to check for
//  additional IRPs in the volume's overflow queue.
//
//  Each of the following routines is also responsible for completing the IRP.
//  We moved this responsibility from the main loop to the individual routines
//  to allow them the ability to complete the IRP and continue post processing
//  actions.
//

NTSTATUS
FatCommonCleanup (                      //  implemented in Cleanup.c
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatCommonClose (                        //  implemented in Close.c
    IN PVCB Vcb,
    IN PFCB Fcb,
    IN PCCB Ccb,
    IN TYPE_OF_OPEN TypeOfOpen,
    IN BOOLEAN Wait,
    IN PVOLUME_DEVICE_OBJECT *VolDo OPTIONAL
    );

VOID
FatFspClose (                           //  implemented in Close.c
    IN PVCB Vcb OPTIONAL
    );

NTSTATUS
FatCommonCreate (                       //  implemented in Create.c
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatCommonDirectoryControl (             //  implemented in DirCtrl.c
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatCommonDeviceControl (                //  implemented in DevCtrl.c
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatCommonQueryEa (                      //  implemented in Ea.c
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatCommonSetEa (                        //  implemented in Ea.c
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
FatCommonQueryInformation (             //  implemented in FileInfo.c
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp
    );

NTSTATUS
Fa