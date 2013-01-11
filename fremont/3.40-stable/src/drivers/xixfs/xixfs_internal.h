#ifndef __XIXFSD_INTERNAL_H__
#define __XIXFSD_INTERNAL_H__

#include "SocketLpx.h"

/*
 *	XixFsAllocate.c
 */
PXIXFS_FCB 	
xixfs_AllocateFCB(VOID);

void
xixfs_FreeFCB(
	PXIXFS_FCB pFCB
);

PXIXFS_CCB	
xixfs_AllocateCCB(VOID);

void
xixfs_FreeCCB(
	PXIXFS_CCB pCCB
);

PXIXFS_LCB	
xixfs_AllocateLCB(uint16 Length);

void
xixfs_FreeLCB(
	PXIXFS_LCB pLCB
);

PXIXFS_IRPCONTEXT 
xixfs_AllocateIrpContext(
	PIRP				Irp,
	PDEVICE_OBJECT		PtrTargetDeviceObject);

void 
xixfs_ReleaseIrpContext(
	PXIXFS_IRPCONTEXT					PtrIrpContext);


PXIFS_CLOSE_FCB_CTX	
xixfs_AllocateCloseFcbCtx(VOID);

void
xixfs_FreeCloseFcbCtx(
	PXIFS_CLOSE_FCB_CTX pCtx
);





/*
 *	XixFsdFcbTable.c
 */
FSRTL_COMPARISON_RESULT
xixfs_FCBTLBFullCompareNames (
    IN PXIXFS_IRPCONTEXT IrpContext,
    IN PUNICODE_STRING NameA,
    IN PUNICODE_STRING NameB
    );

BOOLEAN
xixfs_NLInsertNameLink (
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PRTL_SPLAY_LINKS *RootNode,
	IN PXIXFS_LCB NameLink
);

PXIXFS_LCB
xixfs_NLFindNameLinkIgnoreCase (
    IN PXIXFS_IRPCONTEXT IrpContext,
    IN PRTL_SPLAY_LINKS *RootNode,
    IN PUNICODE_STRING Name
    );

BOOLEAN
xixfs_NLInsertNameLinkIgnoreCase (
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PRTL_SPLAY_LINKS *RootNode,
	IN PXIXFS_LCB NameLink
);

PXIXFS_LCB
xixfs_NLFindNameLink (
    IN PXIXFS_IRPCONTEXT IrpContext,
    IN PRTL_SPLAY_LINKS *RootNode,
    IN PUNICODE_STRING Name
    );


PXIXFS_LCB
xixfs_FCBTLBInsertPrefix (
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PXIXFS_FCB Fcb,
	IN PUNICODE_STRING Name,
	IN PXIXFS_FCB ParentFcb
);

VOID
xixfs_FCBTLBRemovePrefix (
	IN BOOLEAN  CanWait,
	IN PXIXFS_LCB Lcb
);


PXIXFS_LCB
xixfs_FCBTLBFindPrefix (
    IN PXIXFS_IRPCONTEXT IrpContext,
    IN OUT PXIXFS_FCB *CurrentFcb,
    IN OUT PUNICODE_STRING RemainingName,
	IN BOOLEAN	bIgnoreCase
    );


RTL_GENERIC_COMPARE_RESULTS
xixfs_FCBTLBCompareEntry(
	IN PRTL_GENERIC_TABLE Table,
	IN PVOID id1,
	IN PVOID id2
);

PVOID
xixfs_FCBTLBAllocateEntry(
	IN PRTL_GENERIC_TABLE Table,
	IN uint32 ByteSize
);


VOID
xixfs_FCBTLBDeallocateEntry(
	IN PRTL_GENERIC_TABLE Table,
	IN PVOID Buffer
);

VOID
xixfs_FCBTLBInsertEntry(
	PXIXFS_FCB pFCB
);


BOOLEAN
xixfs_FCBTLBDeleteEntry(
	PXIXFS_FCB pFCB
);

PXIXFS_FCB
xixfs_FCBTLBLookupEntry(
	PXIXFS_VCB	VCB,
	uint64		FileId
);

PXIXFS_FCB
xixfs_FCBTLBGetNextEntry (
    IN PXIXFS_VCB Vcb,
    IN PVOID *RestartKey
);






/*
 *	XixFsdMisc.c
 */
uint64
xixfs_GetLcnFromLot(
	IN uint32	LotSize,
	IN uint64	LotIndex
);

VOID
xixfs_RemoveLastSlashOfName(
	IN PUNICODE_STRING pName 
);

typedef enum _TYPE_OF_ACQUIRE {    
    AcquireExclusive,
    AcquireShared,
    AcquireSharedStarveExclusive
} TYPE_OF_ACQUIRE, *PTYPE_OF_ACQUIRE;

BOOLEAN
xixfs_AcquireResource(
	IN BOOLEAN Waitable,
	IN PERESOURCE Resource,
	IN BOOLEAN IgnoreWait,
	IN TYPE_OF_ACQUIRE Type
);

#define XifsdIncCloseCount(Fcb)		InterlockedIncrement(&((Fcb)->FCBCloseCount))
#define XifsdDecCloseCount(Fcb)		InterlockedDecrement(&((Fcb)->FCBCloseCount))

#define XifsdIncCleanUpCount(Fcb){					\
			ASSERT_LOCKED_VCB((Fcb)->PtrVCB);		\
			(Fcb)->FCBCleanup += 1;					\
			(Fcb)->PtrVCB->VCBCleanup += 1; 		\
}

#define XifsdDecrementClenupCount(Fcb){				\
			ASSERT_LOCKED_VCB((Fcb)->PtrVCB);		\
			(Fcb)->FCBCleanup -= 1;					\
			(Fcb)->PtrVCB->VCBCleanup -= 1; 		\
}

#define XifsdIncRefCount(Fcb, C, UC){				\
			ASSERT_LOCKED_VCB((Fcb)->PtrVCB);		\
			(Fcb)->FCBReference += (C);				\
			(Fcb)->FCBUserReference += (UC);		\
			(Fcb)->PtrVCB->VCBReference += (C);		\
			(Fcb)->PtrVCB->VCBUserReference += (UC);\
}			

#define XifsdDecRefCount(Fcb,C,UC){					\
			ASSERT_LOCKED_VCB((Fcb)->PtrVCB);		\
			(Fcb)->FCBReference -= (C);				\
			(Fcb)->FCBUserReference -= (UC);		\
			(Fcb)->PtrVCB->VCBReference -= (C);		\
			(Fcb)->PtrVCB->VCBUserReference -= (UC);\
}			



#define XifsdAcquireGData(IC)		ExAcquireResourceExclusiveLite( &XiGlobalData.DataResource, TRUE )
#define XifsdReleaseGData(IC)		ExReleaseResourceLite( &XiGlobalData.DataResource )

#define XifsdAcquireVcbExclusive(IC,V,I)	xixfs_AcquireResource( (IC), &(V)->VCBResource, (I), AcquireExclusive )

#define XifsdAcquireVcbShared(IC,V,I)			xixfs_AcquireResource( (IC), &(V)->VCBResource, (I), AcquireShared )

#define XifsdReleaseVcb(IC,V)					ExReleaseResourceLite( &(V)->VCBResource )

#define XifsdAcquireAllFiles(IC,V)			xixfs_AcquireResource( (IC), &(V)->FileResource, FALSE, AcquireExclusive )

#define XifsdReleaseAllFiles(IC,V)			ExReleaseResourceLite( &(V)->FileResource )

#define XifsdAcquireFileExclusive(IC,F)		xixfs_AcquireResource( (IC), (F)->Resource, FALSE, AcquireExclusive )

#define XifsdAcquireFileShared(IC,F)			xixfs_AcquireResource( (IC), (F)->Resource, FALSE, AcquireShared )

#define XifsdAcquireFileSharedStarveExclusive(IC,F)	xixfs_AcquireResource( (IC), (F)->Resource, FALSE, AcquireSharedStarveExclusive )

#define XifsdReleaseFile(IC,F)				ExReleaseResourceLite( (F)->Resource )

//#define XifsdAcquireVmcbForCcMap(IC,V)		XifsdAcquireResource( (IC), &(V)->VmcbMappingResource, FALSE, AcquireShared)
    
//#define XifsdAcquireVmcbForCcPurge(IC,V)	XifsdAcquireResource( (IC), &(V)->VmcbMappingResource, FALSE, AcquireExclusive)

//#define XifsdReleaseVmcb( IC, V)				ExReleaseResourceLite( &(V)->VmcbMappingResource)

#define XifsdAcquireFcbExclusive(IC,F,I)		xixfs_AcquireResource( (IC), &(F)->FCBResource, (I), AcquireExclusive )

#define XifsdAcquireFcbShared(IC,F,I)			xixfs_AcquireResource( (IC), &(F)->FCBResource, (I), AcquireShared )

#define XifsdReleaseFcb(IC,F)					ExReleaseResourceLite( &(F)->FCBResource )



//ASSERT(KeAreApcsDisabled());							
#define XifsdLockVcb(IC,V)									\
	ExAcquireFastMutexUnsafe( &(V)->VCBMutex );				\
	(V)->VCBLockThread = PsGetCurrentThread()

#define XifsdUnlockVcb(IC,V)								\
    (V)->VCBLockThread = NULL;								\
    ExReleaseFastMutexUnsafe( &(V)->VCBMutex )

//ASSERT(KeAreApcsDisabled());								
#define XifsdLockFcb(IC,F) {										\
	PVOID _CurrentThread = PsGetCurrentThread();					\
	if (_CurrentThread != (F)->FCBLockThread) {						\
		ExAcquireFastMutexUnsafe( &(F)->FCBMutex );					\
		ASSERT( (F)->FCBLockCount == 0 );							\
		(F)->FCBLockThread = _CurrentThread;						\
	}																\
	(F)->FCBLockCount += 1;											\
}

#define XifsdUnlockFcb(IC,F) {										\
	ASSERT( PsGetCurrentThread() == (F)->FCBLockThread);			\
	(F)->FCBLockCount -= 1;											\
	if ((F)->FCBLockCount == 0) {									\
		(F)->FCBLockThread = NULL;									\
		ExReleaseFastMutexUnsafe( &(F)->FCBMutex );					\
	}																\
}






#define XixFsdIncGlobalWorkItem()		{					\
    InterlockedIncrement(&(XiGlobalData.QueuedWork));		\
    KeResetEvent(&(XiGlobalData.QueuedWorkcleareEvent));	\
}


#define XixFsdDecGlobalWorkItem()		{							\
	if(InterlockedDecrement(&(XiGlobalData.QueuedWork)) <= 0){		\
		KeSetEvent(&(XiGlobalData.QueuedWorkcleareEvent), 0, FALSE);\
	}																\
}

VOID
xixfs_NotifyReportChangeToXixfs(
	
	IN PXIXFS_FCB	pFCB,
	IN uint32		FilterMatch,
	IN uint32		Action
);

typedef enum _TYPE_OF_OPEN {

    UnopenedFileObject = 0,
    StreamFileOpen,
    UserVolumeOpen,
    UserDirectoryOpen,
    UserFileOpen,
    BeyondValidType

} TYPE_OF_OPEN, *PTYPE_OF_OPEN;



VOID
xixfs_SetFileObject(
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PFILE_OBJECT FileObject,
	IN TYPE_OF_OPEN TypeOfOpen,
	IN PXIXFS_FCB pFCB OPTIONAL,
	IN PXIXFS_CCB pCCB OPTIONAL
);


TYPE_OF_OPEN
xixfs_DecodeFileObject(
	IN PFILE_OBJECT pFileObject, 
	OUT PXIXFS_FCB *ppFCB, 
	OUT PXIXFS_CCB *ppCCB 
);

PVOID
xixfs_GetCallersBuffer(
	PIRP PtrIrp
);

NTSTATUS 
xixfs_PinCallersBuffer(
	PIRP				PtrIrp,
	BOOLEAN			IsReadOperation,
	uint32			Length
);

uint32
xixfs_SearchLastComponetOffsetOfName(
	IN PUNICODE_STRING pName
);



VOID
xixfs_EventComSvrThreadProc(
		PVOID	lpParameter
);

VOID
xixfs_EventComCliThreadProc(
		PVOID	lpParameter
);

/*
XixFsRawFileStub.c
*/

typedef struct _EOF_WAIT_CTX{
	LIST_ENTRY	EofWaitLink;
	KEVENT		EofEvent;
}EOF_WAIT_CTX, *PEOF_WAIT_CTX;

VOID
xixfs_FileFinishIoEof(
	IN PXIXFS_FCB pFCB
);


BOOLEAN
xixfs_FileCheckEofWrite(
	IN PXIXFS_FCB		pFCB,
	IN PLARGE_INTEGER	FileOffset,
	IN ULONG			Length
);

NTSTATUS
xixfs_CheckAndVerifyVolume(
	IN PXIXFS_VCB	pVCB
);

BOOLEAN
xixfs_VerifyFCBOperation (
	IN PXIXFS_IRPCONTEXT IrpContext OPTIONAL,
	IN PXIXFS_FCB Fcb
);


NTSTATUS
xixfs_FileNonCachedIo(
 	IN PXIXFS_FCB		FCB,
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PIRP				Irp,
	IN uint64 			ByteOffset,
	IN ULONG 			ByteCount,
	IN BOOLEAN			IsReadOp,
	IN TYPE_OF_OPEN		TypeOfOpen,
	IN PERESOURCE		PtrResourceAcquired
);

VOID
xixfs_PrePostIrp(
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PIRP Irp
);


VOID
xixfs_OplockComplete(
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PIRP Irp
);


FAST_IO_POSSIBLE
xixfs_CheckFastIoPossible(
	IN PXIXFS_FCB pFCB
);


NTSTATUS
xixfs_ProcessFileNonCachedIo(
	IN PVOID			pContext
);





NTSTATUS
xixfs_OpenExistingFCB(
	IN PXIXFS_IRPCONTEXT	IrpContext,
	IN PIO_STACK_LOCATION IrpSp,
	IN OUT PXIXFS_FCB * ppCurrentFCB,
	IN PXIXFS_LCB	pLCB OPTIONAL,
	IN TYPE_OF_OPEN	TypeOfOpen,
	IN BOOLEAN		DeleteOnCloseSpecified,
	IN BOOLEAN		bIgnoreCase,
	IN uint32		RequestDeposition,
	IN PXIXFS_CCB	pRelatedCCB OPTIONAL,
	IN PUNICODE_STRING AbsolutePath OPTIONAL
);


NTSTATUS
xixfs_OpenByFileId(
	IN PXIXFS_IRPCONTEXT		IrpContext,
	IN PIO_STACK_LOCATION	IrpSp,
	IN PXIXFS_VCB			pVCB,
	IN BOOLEAN				DeleteOnCloseSpecified,
	IN BOOLEAN				bIgnoreCase,
	IN BOOLEAN				DirectoryOnlyRequested,
	IN BOOLEAN				FileOnlyRequested,
	IN uint32				RequestDeposition,
	IN OUT PXIXFS_FCB		*ppFCB
);

NTSTATUS
xixfs_OpenObjectFromDirContext (
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PIO_STACK_LOCATION IrpSp,
	IN PXIXFS_VCB Vcb,
	IN OUT PXIXFS_FCB *CurrentFcb,
	IN PXIXCORE_DIR_EMUL_CONTEXT DirContext,
	IN BOOLEAN PerformUserOpen,
	IN BOOLEAN		DeleteOnCloseSpecified,
	IN BOOLEAN		bIgnoreCase,
	IN uint32		RequestDeposition,
	IN PXIXFS_CCB RelatedCcb OPTIONAL,
	IN PUNICODE_STRING AbsolutePath OPTIONAL
);


NTSTATUS
xixfs_OpenNewFileObject(
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PIO_STACK_LOCATION IrpSp,
	IN PXIXFS_VCB Vcb,
	IN PXIXFS_FCB *CurrentFCB, //ParentFcb,
	IN uint64	OpenFileId,
	IN uint32	OpenFileType,
	IN BOOLEAN	DeleteOnCloseSpecified,
	IN BOOLEAN	bIgnoreCase,
	IN PUNICODE_STRING OpenFileName,
	IN PUNICODE_STRING AbsolutePath OPTIONAL
);



NTSTATUS
xixfs_ReLoadFileFromFcb(
	IN PXIXFS_FCB pFCB
);



NTSTATUS
xixfs_ReadFileInfoFromFcb(
	IN PXIXFS_FCB pFCB,
	IN BOOLEAN	Waitable,
	IN PXIXCORE_BUFFER LotHeader,
	IN PXIXCORE_BUFFER Buffer
);

NTSTATUS
xixfs_WriteFileInfoFromFcb(
	IN PXIXFS_FCB pFCB,
	IN BOOLEAN	Waitable,
	IN PXIXCORE_BUFFER Buffer
);


NTSTATUS
xixfs_InitializeFCBInfo(
	IN PXIXFS_FCB pFCB,
	IN BOOLEAN	Waitable
);


/*
 *	Used for directory suffing. << FILE/DIRECOTRY >>
 */

NTSTATUS
xixfs_InitializeFileContext(
	PXIFS_FILE_EMUL_CONTEXT FileContext
);


NTSTATUS
xixfs_SetFileContext(
	PXIXFS_VCB	pVCB,
	uint64	LotNumber,
	uint32	FileType,
	PXIFS_FILE_EMUL_CONTEXT FileContext
);


NTSTATUS
xixfs_ClearFileContext(
	PXIFS_FILE_EMUL_CONTEXT FileContext
);


NTSTATUS
xixfs_FileInfoFromContext(
	BOOLEAN					Waitable,
	PXIFS_FILE_EMUL_CONTEXT FileContext
);



















/*
dispatch
*/

/*
 *	xixfs_Dispatch.c
 */
VOID
xixfs_MdlComplete(
	PXIXFS_IRPCONTEXT			PtrIrpContext,
	PIRP						PtrIrp,
	PIO_STACK_LOCATION			PtrIoStackLocation,
	BOOLEAN						ReadCompletion
);



VOID
xixfs_CompleteRequest(
	IN PXIXFS_IRPCONTEXT		IrpContext,
	IN NTSTATUS				Status,
	IN uint32				ReturnByte
);

LONG
xixfs_ExceptionFilter (
    IN PXIXFS_IRPCONTEXT IrpContext,
    IN PEXCEPTION_POINTERS ExceptionPointer
    );

NTSTATUS
xixfs_PostRequest(
	IN PXIXFS_IRPCONTEXT PtrIrpContext,
	IN PIRP				Irp
);


NTSTATUS
xixfs_Dispatch (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP Irp);

NTSTATUS
xixfs_CommonDispatch(
	IN PVOID		Context
	);


NTSTATUS
xixfs_DoDelayProcessing(
	IN PXIXFS_IRPCONTEXT PtrIrpContext,
	IN XIFS_DELAYEDPROCESSING Func
);

NTSTATUS
xixfs_DoDelayNonCachedIo(
	IN PVOID pContext 
);

/*
 *	XixFsFileSystemCtrl.c
 */


NTSTATUS
xixfs_CommonFileSystemControl(
	IN PXIXFS_IRPCONTEXT IrpContext
	);

NTSTATUS
xixfs_CommonShutdown(
	IN PXIXFS_IRPCONTEXT pIrpContext
	);


/*
 *	XixFsdFileCtrl.c	
 */


NTSTATUS
xixfs_CommonQueryInformation(
	IN PXIXFS_IRPCONTEXT pIrpContext
	);

NTSTATUS
xixfs_CommonSetInformation(
	IN PXIXFS_IRPCONTEXT pIrpContext
	);

/*
 *	XixFsdDirCtrl.c
 */

NTSTATUS
xixfs_CommonDirectoryControl(
	IN PXIXFS_IRPCONTEXT pIrpContext
	);


/*
 *	XixFsdEa.c
 */

NTSTATUS
xixfs_CommonQueryEA(
	IN PXIXFS_IRPCONTEXT IrpContext
	);

NTSTATUS
xixfs_CommonSetEA(
	IN PXIXFS_IRPCONTEXT IrpContext
	);


/*
 *	XixFsdFileLock.c
 */

NTSTATUS
xixfs_CommonLockControl(
	IN PXIXFS_IRPCONTEXT pIrpContext
	);


/*
 *	XixFsdFlushCtrl.c
 */



NTSTATUS
xixfs_CommonFlushBuffers(
	IN PXIXFS_IRPCONTEXT pIrpContext
);


/*
 *	XixFsdPnpCtrl.c
 */
NTSTATUS
xixfs_CommonPNP(
	IN PXIXFS_IRPCONTEXT pIrpContext
	);

/*
 *	XixFsdDeviceCtrl.c
 */

NTSTATUS
xixfs_CommonDeviceControl(
	IN PXIXFS_IRPCONTEXT pIrpContext
	);


/*
 *	XixFsdSecurityCtrl.c
 */

NTSTATUS
xixfs_CommonQuerySecurity(
	IN PXIXFS_IRPCONTEXT IrpContext
	);


NTSTATUS
xixfs_CommonSetSecurity(
	IN PXIXFS_IRPCONTEXT IrpContext
	);



/*
 *	XixFsdVolumeCtrl.c
 */
NTSTATUS
xixfs_CommonQueryVolumeInformation(
	IN PXIXFS_IRPCONTEXT pIrpContext
	);

NTSTATUS
xixfs_CommonSetVolumeInformation(
	IN PXIXFS_IRPCONTEXT IrpContext
	);

/*
 *	XixFsdRead.c
 */

NTSTATUS
xixfs_CommonRead(
	IN PXIXFS_IRPCONTEXT PtrIrpContext
);


/*
 *	XixFsdWrite.c
 */
NTSTATUS
xixfs_CommonWrite(
	IN PXIXFS_IRPCONTEXT PtrIrpContext
	);

/*
 *	XixFsdCreate.c
 */

NTSTATUS
xixfs_CommonCreate(
	IN PXIXFS_IRPCONTEXT PtrIrpContext
	);

/*
 *	XixFsdCleaUp.c
 */
NTSTATUS
xixfs_CommonCleanUp(
	IN PXIXFS_IRPCONTEXT pIrpContext
	);

/*
 *	XixFsdClose.c
 */
NTSTATUS
xixfs_CommonClose(
	IN PXIXFS_IRPCONTEXT pIrpContext
	);



/*
 *	XixFsdCreateCloseMisc.c	
 */

NTSTATUS
xixfs_UpdateFCB( 
	IN	PXIXFS_FCB	pFCB
);

NTSTATUS
XixFsdDeleteFileLotAddress(
	IN PXIXFS_VCB pVCB,
	IN PXIXFS_FCB pFCB
);

NTSTATUS
xixfs_DeleteUpdateFCB(
	IN	PXIXFS_FCB	pFCB
);


NTSTATUS
xixfs_LastUpdateFileFromFCB( 
	IN	PXIXFS_FCB	pFCB
);

VOID
xixfs_DeleteVCB(
    IN PXIXFS_IRPCONTEXT pIrpContext,
    IN PXIXFS_VCB pVCB
);


VOID
xixfs_DeleteFCB(
	IN BOOLEAN CanWait,
	IN PXIXFS_FCB		pFCB
);

VOID
xixfs_DeleteInternalStream( 
	IN BOOLEAN Waitable, 
	IN PXIXFS_FCB		pFCB 
);


VOID
xixfs_TeardownStructures(
	IN BOOLEAN Waitable,
	IN PXIXFS_FCB pFCB,
	IN BOOLEAN Recursive,
	OUT PBOOLEAN RemovedFCB
);


BOOLEAN
xixfs_CloseFCB(
	IN BOOLEAN		Waitable,
	IN PXIXFS_VCB	pVCB,
	IN PXIXFS_FCB	pFCB,
	IN uint32	UserReference
);


VOID
xixfs_CallCloseFCB(
	IN PVOID Context
);

VOID
xixfs_RealCloseFCB(
	IN PVOID Context
);

VOID
xixfs_InsertCloseQueue(
	IN PXIXFS_FCB pFCB
);




PXIXFS_FCB
xixfs_CreateAndAllocFCB(
	IN PXIXFS_VCB		pVCB,
	IN uint64			FileId,
	IN uint32			FCB_TYPE_CODE,
	OUT PBOOLEAN		Exist OPTIONAL
);


VOID
xixfs_CreateInternalStream (
    IN PXIXFS_IRPCONTEXT IrpContext,
    IN PXIXFS_VCB pVCB,
    IN PXIXFS_FCB pFCB,
	IN BOOLEAN	ReadOnly
);


NTSTATUS
xixfs_InitializeFCB(
	IN PXIXFS_FCB pFCB,
	IN BOOLEAN	Waitable
);

PXIXFS_CCB
xixfs_CreateCCB(
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PFILE_OBJECT FileObject,
	IN PXIXFS_FCB pFCB, 
	IN PXIXFS_LCB pLCB,
	IN uint32	CCBFlags
);


VOID
xixfs_InitializeLcbFromDirContext(
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PXIXFS_LCB Lcb
);


/*
 *	XixFsdNetEvent.c
 */


BOOLEAN
xixfs_IHaveLotLock(
	IN uint8	* HostMac,
	IN uint64	 LotNumber,
	IN uint8	* VolumeId
);

NTSTATUS
xixfs_SendRenameLinkBC(
	IN BOOLEAN		Wait,
	IN uint32		SubCommand,
	IN uint8		* HostMac,
	IN uint64		LotNumber,
	IN uint8		* VolumeId,
	IN uint64		OldParentLotNumber,
	IN uint64		NewParentLotNumber
);


NTSTATUS
xixfs_SendFileChangeRC(
	IN BOOLEAN		Wait,
	IN uint8		* HostMac,
	IN uint64		LotNumber,
	IN uint8		* VolumeId,
	IN uint64		FileLength,
	IN uint64		AllocationLength,
	IN uint64		UpdateStartOffset
);


/*
 *	XixFsdFilePurgeFlushMisc.c	
 */
NTSTATUS
xixfs_FlusVolume(
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PXIXFS_VCB		pVCB
);


NTSTATUS
xixfs_PurgeVolume(
	IN PXIXFS_IRPCONTEXT IrpContext,
	IN PXIXFS_VCB		pVCB,
	IN BOOLEAN			DismountForce
);

VOID
xixfs_CleanupFlushVCB(
	IN PXIXFS_IRPCONTEXT pIrpContext,
	IN PXIXFS_VCB		pVCB,
	IN BOOLEAN			DisMountVCB
);



/*
	XixfsNdasDeviceControl.c
*/
#include "ndasscsi.h"
BOOLEAN	
xixfs_GetScsiportAdapter(
  	IN	PDEVICE_OBJECT				DiskDeviceObject,
  	IN	PDEVICE_OBJECT				*ScsiportAdapterDeviceObject
	);

NTSTATUS
xixfs_RemoveUserBacl(
	IN	PDEVICE_OBJECT	DeviceObject,
	IN	BLOCKACE_ID		BlockAceId
);


NTSTATUS
xixfs_CheckXifsd(
	PDEVICE_OBJECT			DeviceObject,
	PPARTITION_INFORMATION	partitionInfo,
	PBLOCKACE_ID			BlockAceId,
	PBOOLEAN				IsWriteProtected
);




/*
 *	XixFsdFileCtrl.c	
 */

NTSTATUS
xixfs_FileDeleteParentChild(
	IN	PXIXFS_IRPCONTEXT			pIrpContext,
	IN	PXIXFS_FCB					ParentFCB, 
	IN	PUNICODE_STRING				pOrgFileName
);

NTSTATUS
xixfs_QueryBasicInfo( 
	IN PXIXFS_FCB 		pFCB, 
	IN PXIXFS_CCB		pCCB, 
	IN OUT PFILE_BASIC_INFORMATION BasicInformation, 
	IN OUT ULONG *Length 
);

NTSTATUS
xixfs_QueryStandardInfo(
	IN PXIXFS_FCB 		pFCB, 
	IN PXIXFS_CCB		pCCB, 
	IN OUT PFILE_STANDARD_INFORMATION StandardInformation, 
	IN OUT ULONG *Length 
);

NTSTATUS
xixfs_QueryNetworkInfo (
	IN PXIXFS_FCB 		pFCB, 
	IN PXIXFS_CCB		pCCB, 
	IN OUT PFILE_NETWORK_OPEN_INFORMATION FileNetworkInformation,
	IN OUT PULONG Length
    );





/*
 *	Fast Io routine
 */




BOOLEAN 
xixfs_FastIoCheckIfPossible(
	IN PFILE_OBJECT				FileObject,
	IN PLARGE_INTEGER			FileOffset,
	IN ULONG					Length,
	IN BOOLEAN					Wait,
	IN ULONG					LockKey,
	IN BOOLEAN					CheckForReadOperation,
	OUT PIO_STATUS_BLOCK		IoStatus,
	IN PDEVICE_OBJECT			DeviceObject
);


BOOLEAN 
xixfs_FastIoRead(
	IN PFILE_OBJECT				FileObject,
	IN PLARGE_INTEGER			FileOffset,
	IN ULONG					Length,
	IN BOOLEAN					Wait,
	IN ULONG					LockKey,
	OUT PVOID					Buffer,
	OUT PIO_STATUS_BLOCK		IoStatus,
	IN PDEVICE_OBJECT			DeviceObject
);

BOOLEAN 
xixfs_FastIoWrite(
	IN PFILE_OBJECT					FileObject,
	IN PLARGE_INTEGER				FileOffset,
	IN ULONG						Length,
	IN BOOLEAN						Wait,
	IN ULONG						LockKey,
	OUT PVOID						Buffer,
	OUT PIO_STATUS_BLOCK			IoStatus,
	IN PDEVICE_OBJECT				DeviceObject
);

BOOLEAN 
xixfs_FastIoQueryBasicInfo(
	IN PFILE_OBJECT					FileObject,
	IN BOOLEAN						Wait,
	OUT PFILE_BASIC_INFORMATION		Buffer,
	OUT PIO_STATUS_BLOCK 			IoStatus,
	IN PDEVICE_OBJECT				DeviceObject
);

BOOLEAN 
xixfs_FastIoQueryStdInfo(
	IN PFILE_OBJECT						FileObject,
	IN BOOLEAN							Wait,
	OUT PFILE_STANDARD_INFORMATION 	Buffer,
	OUT PIO_STATUS_BLOCK 				IoStatus,
	IN PDEVICE_OBJECT					DeviceObject
);

BOOLEAN 
xixfs_FastIoLock(
	IN PFILE_OBJECT				FileObject,
	IN PLARGE_INTEGER			FileOffset,
	IN PLARGE_INTEGER			Length,
	PEPROCESS					ProcessId,
	ULONG						Key,
	BOOLEAN						FailImmediately,
	BOOLEAN						ExclusiveLock,
	OUT PIO_STATUS_BLOCK		IoStatus,
	IN PDEVICE_OBJECT			DeviceObject
);

BOOLEAN 
xixfs_FastIoUnlockSingle(
	IN PFILE_OBJECT			FileObject,
	IN PLARGE_INTEGER		FileOffset,
	IN PLARGE_INTEGER		Length,
	PEPROCESS				ProcessId,
	ULONG					Key,
	OUT PIO_STATUS_BLOCK	IoStatus,
	IN PDEVICE_OBJECT		DeviceObject
);

BOOLEAN 
xixfs_FastIoUnlockAll(
	IN PFILE_OBJECT				FileObject,
	PEPROCESS						ProcessId,
	OUT PIO_STATUS_BLOCK			IoStatus,
	IN PDEVICE_OBJECT				DeviceObject
);

BOOLEAN 
xixfs_FastIoUnlockAllByKey(
	IN PFILE_OBJECT				FileObject,
	PEPROCESS						ProcessId,
	ULONG								Key,
	OUT PIO_STATUS_BLOCK			IoStatus,
	IN PDEVICE_OBJECT				DeviceObject
);

void 
xixfs_FastIoAcqCreateSec(
	IN PFILE_OBJECT			FileObject
);

void 
xixfs_FastIoRelCreateSec(
	IN PFILE_OBJECT			FileObject
);

BOOLEAN 
xixfs_FastIoQueryNetInfo(
	IN PFILE_OBJECT								FileObject,
	IN BOOLEAN									Wait,
	OUT PFILE_NETWORK_OPEN_INFORMATION 			Buffer,
	OUT PIO_STATUS_BLOCK 						IoStatus,
	IN PDEVICE_OBJECT							DeviceObject
);

NTSTATUS 
xixfs_FastIoAcqModWrite(
	IN PFILE_OBJECT					FileObject,
	IN PLARGE_INTEGER					EndingOffset,
	OUT PERESOURCE						*ResourceToRelease,
	IN PDEVICE_OBJECT					DeviceObject
);

NTSTATUS 
xixfs_FastIoRelModWrite(
	IN PFILE_OBJECT				FileObject,
	IN PERESOURCE					ResourceToRelease,
	IN PDEVICE_OBJECT				DeviceObject
);

NTSTATUS 
xixfs_FastIoAcqCcFlush(
	IN PFILE_OBJECT			FileObject,
	IN PDEVICE_OBJECT			DeviceObject
);

NTSTATUS 
xixfs_FastIoRelCcFlush(
	IN PFILE_OBJECT			FileObject,
	IN PDEVICE_OBJECT			DeviceObject
);

BOOLEAN 
xixfs_FastIoMdlRead(
	IN PFILE_OBJECT				FileObject,
	IN PLARGE_INTEGER			FileOffset,
	IN ULONG					Length,
	IN ULONG					LockKey,
	OUT PMDL					*MdlChain,
	OUT PIO_STATUS_BLOCK		IoStatus,
	IN PDEVICE_OBJECT			DeviceObject
);

BOOLEAN 
xixfs_FastIoMdlReadComplete(
	IN PFILE_OBJECT				FileObject,
	OUT PMDL							MdlChain,
	IN PDEVICE_OBJECT				DeviceObject
);

BOOLEAN 
xixfs_FastIoPrepareMdlWrite(
	IN PFILE_OBJECT				FileObject,
	IN PLARGE_INTEGER			FileOffset,
	IN ULONG					Length,
	IN ULONG					LockKey,
	OUT PMDL					*MdlChain,
	OUT PIO_STATUS_BLOCK		IoStatus,
	IN PDEVICE_OBJECT			DeviceObject
);

BOOLEAN 
xixfs_FastIoMdlWriteComplete(
	IN PFILE_OBJECT				FileObject,
	IN PLARGE_INTEGER				FileOffset,
	OUT PMDL							MdlChain,
	IN PDEVICE_OBJECT				DeviceObject
);

BOOLEAN
xixfs_AcqLazyWrite(
	IN PVOID	Context,
	IN BOOLEAN	Wait
);

VOID
xixfs_RelLazyWrite(
	IN PVOID Context
);

BOOLEAN
xixfs_AcqReadAhead(
	IN PVOID	Context,
	IN BOOLEAN	Wait
);

VOID
xixfs_RelReadAhead(
	IN PVOID Context
);


BOOLEAN
xixfs_NopAcqLazyWrite(
	IN PVOID	Context,
	IN BOOLEAN	Wait
);

VOID
xixfs_NopRelLazyWrite(
	IN PVOID Context
);

BOOLEAN
xixfs_NopAcqReadAhead(
	IN PVOID	Context,
	IN BOOLEAN	Wait
);

VOID
xixfs_NopRelReadAhead(
	IN PVOID Context
);

/*
 *	XixFsFileSystemCtrl.c
 */


BOOLEAN
xixfs_DismountVCB(
	IN PXIXFS_IRPCONTEXT pIrpContext,
	IN PXIXFS_VCB		pVCB
);


BOOLEAN
xixfs_CheckForDismount (
    IN PXIXFS_IRPCONTEXT pIrpContext,
    IN PXIXFS_VCB pVCB,
    IN BOOLEAN Force
    );

NTSTATUS
xixfs_LockVolumeInternal(
	IN PXIXFS_IRPCONTEXT 	pIrpContext,
	IN PXIXFS_VCB 		pVCB,
	IN PFILE_OBJECT		pFileObject
);

NTSTATUS	
xixfs_UnlockVolumeInternal(
	IN PXIXFS_IRPCONTEXT 	pIrpContext,
	IN PXIXFS_VCB 		pVCB,
	IN PFILE_OBJECT		pFileObject
);


NTSTATUS
xixfs_CommonFileSystemControl(
	IN PXIXFS_IRPCONTEXT IrpContext
	);

NTSTATUS
xixfs_CommonShutdown(
	IN PXIXFS_IRPCONTEXT pIrpContext
	);


/*
 *	XixFsdEventComSerCli.c
 */
VOID
xixfs_SevEventCallBack(
		PSOCKETLPX_ADDRESS_LIST	Original,
		PSOCKETLPX_ADDRESS_LIST	Updated,
		PSOCKETLPX_ADDRESS_LIST	Disabled,
		PSOCKETLPX_ADDRESS_LIST	Enabled,
		PVOID					Context
	);

VOID
xixfs_CliEventCallBack(
		PSOCKETLPX_ADDRESS_LIST	Original,
		PSOCKETLPX_ADDRESS_LIST	Updated,
		PSOCKETLPX_ADDRESS_LIST	Disabled,
		PSOCKETLPX_ADDRESS_LIST	Enabled,
		PVOID					Context
	);



VOID
xixfs_EventComSvrThreadProc(
		PVOID	lpParameter
);

VOID
xixfs_EventComCliThreadProc(
		PVOID	lpParameter
);

/*XixFsLotBitmapOp.c
*/
NTSTATUS
xixfs_GetMoreCheckOutLotMap(
	IN PXIXFS_VCB VCB
);


NTSTATUS
xixfs_UpdateMetaData(
	IN PXIXFS_VCB VCB
);


VOID
xixfs_MetaUpdateFunction(
		PVOID	lpParameter
);



NTSTATUS
Xixfs_GenerateUuid(OUT void * uuid);
#endif //#ifndef __XIXFSD_INTERNAL_H__