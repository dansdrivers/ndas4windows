#ifndef __XIXFSD_INTERNAL_API_H__
#define __XIXFSD_INTERNAL_API_H__

#include "SocketLpx.h"

/*
 *	XixFsAllocate.c
 */
PXIFS_FCB 	
XixFsdAllocateFCB(VOID);

void
XixFsdFreeFCB(
	PXIFS_FCB pFCB
);

PXIFS_CCB	
XixFsdAllocateCCB(VOID);

void
XixFsdFreeCCB(
	PXIFS_CCB pCCB
);

PXIFS_LCB	
XixFsdAllocateLCB(uint16 Length);

void
XixFsdFreeLCB(
	PXIFS_LCB pLCB
);

PXIFS_IRPCONTEXT 
XixFsdAllocateIrpContext(
	PIRP				Irp,
	PDEVICE_OBJECT		PtrTargetDeviceObject);

void 
XixFsdReleaseIrpContext(
	PXIFS_IRPCONTEXT					PtrIrpContext);


PXIFS_CLOSE_FCB_CTX	
XixFsdAllocateCloseFcbCtx(VOID);

void
XixFsdFreeCloseFcbCtx(
	PXIFS_CLOSE_FCB_CTX pCtx
);

/*
 * XixFsdFastIo.c	
 */

VOID
XixFsdFinishIoEof(
	IN PXIFS_FCB pFCB
);

BOOLEAN
XixFsdCheckEofWrite(
	IN PXIFS_FCB		pFCB,
	IN PLARGE_INTEGER	FileOffset,
	IN ULONG			Length
);


/*
 *	XixFsdDispatch.c
 */
VOID
XixFsdMdlComplete(
	PXIFS_IRPCONTEXT			PtrIrpContext,
	PIRP						PtrIrp,
	PIO_STACK_LOCATION			PtrIoStackLocation,
	BOOLEAN						ReadCompletion
);



VOID
XixFsdCompleteRequest(
	IN PXIFS_IRPCONTEXT		IrpContext,
	IN NTSTATUS				Status,
	IN uint32				ReturnByte
);

LONG
XixFsdExceptionFilter (
    IN PXIFS_IRPCONTEXT IrpContext,
    IN PEXCEPTION_POINTERS ExceptionPointer
    );

NTSTATUS
XixFsdPostRequest(
	IN PXIFS_IRPCONTEXT PtrIrpContext,
	IN PIRP				Irp
);


NTSTATUS
XixFsdDispatch (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP Irp);

NTSTATUS
XixFsdCommonDispatch(
	IN PVOID		Context
	);


NTSTATUS
XixFsdDoDelayProcessing(
	IN PXIFS_IRPCONTEXT PtrIrpContext,
	IN XIFS_DELAYEDPROCESSING Func
);

NTSTATUS
XixFsdDoDelayNonCachedIo(
	IN PVOID pContext 
);

/*
 *	XixFsdMisc.c
 */
typedef enum _TYPE_OF_OPEN {

    UnopenedFileObject = 0,
    StreamFileOpen,
    UserVolumeOpen,
    UserDirectoryOpen,
    UserFileOpen,
    BeyondValidType

} TYPE_OF_OPEN, *PTYPE_OF_OPEN;



VOID
XixFsdSetFileObject(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PFILE_OBJECT FileObject,
	IN TYPE_OF_OPEN TypeOfOpen,
	IN PXIFS_FCB pFCB OPTIONAL,
	IN PXIFS_CCB pCCB OPTIONAL
);


TYPE_OF_OPEN
XixFsdDecodeFileObject(
	IN PFILE_OBJECT pFileObject, 
	OUT PXIFS_FCB *ppFCB, 
	OUT PXIFS_CCB *ppCCB 
);

/*
 *	XixFsdFcbTable.c
 */
FSRTL_COMPARISON_RESULT
XixFsdFullCompareNames (
    IN PXIFS_IRPCONTEXT IrpContext,
    IN PUNICODE_STRING NameA,
    IN PUNICODE_STRING NameB
    );

BOOLEAN
XixFsdInsertNameLink (
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PRTL_SPLAY_LINKS *RootNode,
	IN PXIFS_LCB NameLink
);

PXIFS_LCB
XixFsdFindNameLinkIgnoreCase (
    IN PXIFS_IRPCONTEXT IrpContext,
    IN PRTL_SPLAY_LINKS *RootNode,
    IN PUNICODE_STRING Name
    );

BOOLEAN
XixFsdInsertNameLinkIgnoreCase (
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PRTL_SPLAY_LINKS *RootNode,
	IN PXIFS_LCB NameLink
);

PXIFS_LCB
XixFsdFindNameLink (
    IN PXIFS_IRPCONTEXT IrpContext,
    IN PRTL_SPLAY_LINKS *RootNode,
    IN PUNICODE_STRING Name
    );


PXIFS_LCB
XixFsdInsertPrefix (
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PXIFS_FCB Fcb,
	IN PUNICODE_STRING Name,
	IN PXIFS_FCB ParentFcb
);

VOID
XixFsdRemovePrefix (
	IN BOOLEAN  CanWait,
	IN PXIFS_LCB Lcb
);


PXIFS_LCB
XixFsdFindPrefix (
    IN PXIFS_IRPCONTEXT IrpContext,
    IN OUT PXIFS_FCB *CurrentFcb,
    IN OUT PUNICODE_STRING RemainingName,
	IN BOOLEAN	bIgnoreCase
    );


RTL_GENERIC_COMPARE_RESULTS
XixFsdFCBTableCompare(
	IN PRTL_GENERIC_TABLE Table,
	IN PVOID id1,
	IN PVOID id2
);

PVOID
XixFsdFCBTableAllocate(
	IN PRTL_GENERIC_TABLE Table,
	IN uint32 ByteSize
);


VOID
XixFsdFCBTableDeallocate(
	IN PRTL_GENERIC_TABLE Table,
	IN PVOID Buffer
);

VOID
XixFsdFCBTableInsertFCB(
	PXIFS_FCB pFCB
);


BOOLEAN
XixFsdFCBTableDeleteFCB(
	PXIFS_FCB pFCB
);

PXIFS_FCB
XixFsdLookupFCBTable(
	PXIFS_VCB	VCB,
	uint64		FileId
);

PXIFS_FCB
XixFsdGetNextFcb (
    IN PXIFS_VCB Vcb,
    IN PVOID *RestartKey
);

/*
 *	XixFsdFileStub.c
 */
NTSTATUS
XixFsdCheckVerifyVolume(
	IN PXIFS_VCB	pVCB
);

BOOLEAN
XixFsdVerifyFcbOperation (
	IN PXIFS_IRPCONTEXT IrpContext OPTIONAL,
	IN PXIFS_FCB Fcb
);


NTSTATUS
XixFsdNonCachedIo(
 	IN PXIFS_FCB		FCB,
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PIRP				Irp,
	IN uint64 			ByteOffset,
	IN ULONG 			ByteCount,
	IN BOOLEAN			IsReadOp,
	IN TYPE_OF_OPEN		TypeOfOpen,
	IN PERESOURCE		PtrResourceAcquired
);

VOID
XixFsdPrePostIrp(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PIRP Irp
);


VOID
XixFsdOplockComplete(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PIRP Irp
);


FAST_IO_POSSIBLE
XixFsdCheckFastIoPossible(
	IN PXIFS_FCB pFCB
);


NTSTATUS
XifsdProcessNonCachedIo(
	IN PVOID			pContext
);





NTSTATUS
XixFsdOpenExistingFCB(
	IN PXIFS_IRPCONTEXT	IrpContext,
	IN PIO_STACK_LOCATION IrpSp,
	IN OUT PXIFS_FCB * ppCurrentFCB,
	IN PXIFS_LCB	pLCB OPTIONAL,
	IN TYPE_OF_OPEN	TypeOfOpen,
	IN BOOLEAN		DeleteOnCloseSpecified,
	IN BOOLEAN		bIgnoreCase,
	IN uint32		RequestDeposition,
	IN PXIFS_CCB	pRelatedCCB OPTIONAL,
	IN PUNICODE_STRING AbsolutePath OPTIONAL
);


NTSTATUS
XixFsdOpenByFileId(
	IN PXIFS_IRPCONTEXT		IrpContext,
	IN PIO_STACK_LOCATION	IrpSp,
	IN PXIFS_VCB			pVCB,
	IN BOOLEAN				DeleteOnCloseSpecified,
	IN BOOLEAN				bIgnoreCase,
	IN BOOLEAN				DirectoryOnlyRequested,
	IN BOOLEAN				FileOnlyRequested,
	IN uint32				RequestDeposition,
	IN OUT PXIFS_FCB		*ppFCB
);

NTSTATUS
XixFsdOpenObjectFromDirContext (
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PIO_STACK_LOCATION IrpSp,
	IN PXIFS_VCB Vcb,
	IN OUT PXIFS_FCB *CurrentFcb,
	IN PXIFS_DIR_EMUL_CONTEXT DirContext,
	IN BOOLEAN PerformUserOpen,
	IN BOOLEAN		DeleteOnCloseSpecified,
	IN BOOLEAN		bIgnoreCase,
	IN uint32		RequestDeposition,
	IN PXIFS_CCB RelatedCcb OPTIONAL,
	IN PUNICODE_STRING AbsolutePath OPTIONAL
);


NTSTATUS
XixFsdOpenNewFileObject(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PIO_STACK_LOCATION IrpSp,
	IN PXIFS_VCB Vcb,
	IN PXIFS_FCB *CurrentFCB, //ParentFcb,
	IN uint64	OpenFileId,
	IN uint32	OpenFileType,
	IN BOOLEAN	DeleteOnCloseSpecified,
	IN BOOLEAN	bIgnoreCase,
	IN PUNICODE_STRING OpenFileName,
	IN PUNICODE_STRING AbsolutePath OPTIONAL
);

/*
 *	XixFsdVolumeStub.c
 */

NTSTATUS
XixFsdCheckVolume(
	IN PDEVICE_OBJECT TargetDevice,
	IN uint32		SectorSize,
	IN uint8			*DiskID
);


NTSTATUS
XixFsdGetSuperBlockInformation(
	IN PXIFS_VCB VCB
);


NTSTATUS
XixFsdRegisterHost(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PXIFS_VCB VCB
);

NTSTATUS
XixFsdDeRegisterHost(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PXIFS_VCB VCB
);

NTSTATUS
XixFsdGetMoreCheckOutLotMap(
	IN PXIFS_VCB VCB
);

NTSTATUS
XixFsdUpdateMetaData(
	IN PXIFS_VCB VCB
);
/*
 *	XixFsdMisc.c
 */

typedef enum _TYPE_OF_ACQUIRE {    
    AcquireExclusive,
    AcquireShared,
    AcquireSharedStarveExclusive
} TYPE_OF_ACQUIRE, *PTYPE_OF_ACQUIRE;

BOOLEAN
XixFsdAcquireResource(
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

#define XifsdAcquireVcbExclusive(IC,V,I)	XixFsdAcquireResource( (IC), &(V)->VCBResource, (I), AcquireExclusive )

#define XifsdAcquireVcbShared(IC,V,I)			XixFsdAcquireResource( (IC), &(V)->VCBResource, (I), AcquireShared )

#define XifsdReleaseVcb(IC,V)					ExReleaseResourceLite( &(V)->VCBResource )

#define XifsdAcquireAllFiles(IC,V)			XixFsdAcquireResource( (IC), &(V)->FileResource, FALSE, AcquireExclusive )

#define XifsdReleaseAllFiles(IC,V)			ExReleaseResourceLite( &(V)->FileResource )

#define XifsdAcquireFileExclusive(IC,F)		XixFsdAcquireResource( (IC), (F)->Resource, FALSE, AcquireExclusive )

#define XifsdAcquireFileShared(IC,F)			XixFsdAcquireResource( (IC), (F)->Resource, FALSE, AcquireShared )

#define XifsdAcquireFileSharedStarveExclusive(IC,F)	XixFsdAcquireResource( (IC), (F)->Resource, FALSE, AcquireSharedStarveExclusive )

#define XifsdReleaseFile(IC,F)				ExReleaseResourceLite( (F)->Resource )

//#define XifsdAcquireVmcbForCcMap(IC,V)		XifsdAcquireResource( (IC), &(V)->VmcbMappingResource, FALSE, AcquireShared)
    
//#define XifsdAcquireVmcbForCcPurge(IC,V)	XifsdAcquireResource( (IC), &(V)->VmcbMappingResource, FALSE, AcquireExclusive)

//#define XifsdReleaseVmcb( IC, V)				ExReleaseResourceLite( &(V)->VmcbMappingResource)

#define XifsdAcquireFcbExclusive(IC,F,I)		XixFsdAcquireResource( (IC), &(F)->FCBResource, (I), AcquireExclusive )

#define XifsdAcquireFcbShared(IC,F,I)			XixFsdAcquireResource( (IC), &(F)->FCBResource, (I), AcquireShared )

#define XifsdReleaseFcb(IC,F)					ExReleaseResourceLite( &(F)->FCBResource )


#define XifsdLockXiGData()									\
    ExAcquireFastMutex( &XiGlobalData.DataMutex );			\
    XiGlobalData.DataLockThread = PsGetCurrentThread()

#define XifsdUnlockXiGData()								\
    XiGlobalData.DataLockThread = NULL;							\
    ExReleaseFastMutex( &XiGlobalData.DataMutex )

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



VOID
XixFsdSetFileObject(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PFILE_OBJECT FileObject,
	IN TYPE_OF_OPEN TypeOfOpen,
	IN PXIFS_FCB pFCB OPTIONAL,
	IN PXIFS_CCB pCCB OPTIONAL
);

TYPE_OF_OPEN
XixFsdDecodeFileObject(
	IN PFILE_OBJECT pFileObject, 
	OUT PXIFS_FCB *ppFCB, 
	OUT PXIFS_CCB *ppCCB 
);

PVOID
XixFsdGetCallersBuffer(
	PIRP PtrIrp
);




NTSTATUS 
XixFsdPinUserBuffer(
	PIRP				PtrIrp,
	BOOLEAN			IsReadOperation,
	uint32			Length
);



uint32
XixFsdSearchLastComponetOffset(
	IN PUNICODE_STRING pName
);


VOID
XixFsdRemoveLastSlash(
	IN PUNICODE_STRING pName 
);

VOID
XixFsdNotifyReportChange(
	
	IN PXIFS_FCB	pFCB,
	IN uint32		FilterMatch,
	IN uint32		Action
);


#define XixFsdIncGlobalWorkItem()		{					\
    InterlockedIncrement(&(XiGlobalData.QueuedWork));		\
    KeResetEvent(&(XiGlobalData.QueuedWorkcleareEvent));	\
}


#define XixFsdDecGlobalWorkItem()		{							\
	if(InterlockedDecrement(&(XiGlobalData.QueuedWork)) <= 0){		\
		KeSetEvent(&(XiGlobalData.QueuedWorkcleareEvent), 0, FALSE);\
	}																\
}




/*
 *	XixFsdCreateCloseMisc.c	
 */

NTSTATUS
XixFsdUpdateFCB( 
	IN	PXIFS_FCB	pFCB
);

NTSTATUS
XixFsdDeleteFileLotAddress(
	IN PXIFS_VCB pVCB,
	IN PXIFS_FCB pFCB
);

NTSTATUS
XixFsdDeleteUpdateFCB(
	IN	PXIFS_FCB	pFCB
);


NTSTATUS
XixFsdLastUpdateFileFromFCB( 
	IN	PXIFS_FCB	pFCB
);

VOID
XixFsdDeleteVCB(
    IN PXIFS_IRPCONTEXT pIrpContext,
    IN PXIFS_VCB pVCB
);


VOID
XixFsdDeleteFcb(
	IN BOOLEAN CanWait,
	IN PXIFS_FCB		pFCB
);

VOID
XixFsdDeleteInternalStream( 
	IN BOOLEAN Waitable, 
	IN PXIFS_FCB		pFCB 
);


VOID
XixFsdTeardownStructures(
	IN BOOLEAN Waitable,
	IN PXIFS_FCB pFCB,
	IN BOOLEAN Recursive,
	OUT PBOOLEAN RemovedFCB
);


BOOLEAN
XixFsdCloseFCB(
	IN BOOLEAN		Waitable,
	IN PXIFS_VCB	pVCB,
	IN PXIFS_FCB	pFCB,
	IN uint32	UserReference
);


VOID
XixFsdCallCloseFCB(
	IN PVOID Context
);

VOID
XixFsdRealCloseFCB(
	IN PVOID Context
);

VOID
XixFsdInsertCloseQueue(
	IN PXIFS_FCB pFCB
);


NTSTATUS
XixFsdCreateNewFileObject(
	IN PXIFS_IRPCONTEXT	pIrpContext,
	IN PXIFS_VCB 		PtrVCB, 
	IN PXIFS_FCB 		ParentFCB,
	IN BOOLEAN			IsFileOnly,
	IN BOOLEAN			DeleteOnCloseSpecified,
	IN uint8 *			Name, 
	IN uint32 			NameLength, 
	IN uint32 			FileAttribute,
	IN PXIFS_DIR_EMUL_CONTEXT DirContext,
	OUT uint64			*NewFileId
);


PXIFS_FCB
XixFsdCreateAndAllocFCB(
	IN PXIFS_VCB		pVCB,
	IN uint64			FileId,
	IN uint32			FCB_TYPE_CODE,
	OUT PBOOLEAN		Exist OPTIONAL
);


VOID
XixFsdCreateInternalStream (
    IN PXIFS_IRPCONTEXT IrpContext,
    IN PXIFS_VCB pVCB,
    IN PXIFS_FCB pFCB,
	IN BOOLEAN	ReadOnly
);


NTSTATUS
XixFsdInitializeFCBInfo(
	IN PXIFS_FCB pFCB,
	IN BOOLEAN	Waitable
);

PXIFS_CCB
XixFsdCreateCCB(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PFILE_OBJECT FileObject,
	IN PXIFS_FCB pFCB, 
	IN PXIFS_LCB pLCB,
	IN uint32	CCBFlags
);


VOID
XixFsdInitializeLcbFromDirContext(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PXIFS_LCB Lcb
);

/*
 *	XixFsdFilePurgeFlushMisc.c	
 */
NTSTATUS
XixFsdFlusVolume(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PXIFS_VCB		pVCB
);


NTSTATUS
XixFsdPurgeVolume(
	IN PXIFS_IRPCONTEXT IrpContext,
	IN PXIFS_VCB		pVCB,
	IN BOOLEAN			DismountForce
);

VOID
XixFsdCleanupFlushVCB(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PXIFS_VCB		pVCB,
	IN BOOLEAN			DisMountVCB
);


/*
 *	XixFsdEventComSerCli.c
 */
VOID
XixFsdSevEventCallBack(
		PSOCKETLPX_ADDRESS_LIST	Original,
		PSOCKETLPX_ADDRESS_LIST	Updated,
		PSOCKETLPX_ADDRESS_LIST	Disabled,
		PSOCKETLPX_ADDRESS_LIST	Enabled,
		PVOID					Context
	);

VOID
XixFsdCliEventCallBack(
		PSOCKETLPX_ADDRESS_LIST	Original,
		PSOCKETLPX_ADDRESS_LIST	Updated,
		PSOCKETLPX_ADDRESS_LIST	Disabled,
		PSOCKETLPX_ADDRESS_LIST	Enabled,
		PVOID					Context
	);



VOID
XixFsdComSvrThreadProc(
		PVOID	lpParameter
);

VOID
XixFsdComCliThreadProc(
		PVOID	lpParameter
);


/*
 *	XixFsdNetEvent.c
 */

BOOLEAN
XixFsAreYouHaveLotLock(
	IN BOOLEAN		Wait,
	IN uint8		* HostMac,
	IN uint8		* LockOwnerMac,
	IN uint64		LotNumber,
	IN uint8		* DiskId,
	IN uint32		PartitionId,
	IN uint8		* LockOwnerId
);

BOOLEAN
XixFsIHaveLotLock(
	IN uint8	* HostMac,
	IN uint64	 LotNumber,
	IN uint8	* DiskId,
	IN uint32	 PartitionId
);

NTSTATUS
XixFsdSendRenameLinkBC(
	IN BOOLEAN		Wait,
	IN uint32		SubCommand,
	IN uint8		* HostMac,
	IN uint64		LotNumber,
	IN uint8		* DiskId,
	IN uint32		PartitionId,
	IN uint64		OldParentLotNumber,
	IN uint64		NewParentLotNumber
);


NTSTATUS
XixFsdSendFileChangeRC(
	IN BOOLEAN		Wait,
	IN uint8		* HostMac,
	IN uint64		LotNumber,
	IN uint8		* DiskId,
	IN uint32		PartitionId,
	IN uint64		FileLength,
	IN uint64		AllocationLength,
	IN uint64		UpdateStartOffset
);

/*
 *	XixFsFileSystemCtrl.c
 */


BOOLEAN
XixFsdDismountVCB(
	IN PXIFS_IRPCONTEXT pIrpContext,
	IN PXIFS_VCB		pVCB
);


BOOLEAN
XixFsdCheckForDismount (
    IN PXIFS_IRPCONTEXT pIrpContext,
    IN PXIFS_VCB pVCB,
    IN BOOLEAN Force
    );

NTSTATUS
XixFsdLockVolumeInternal(
	IN PXIFS_IRPCONTEXT 	pIrpContext,
	IN PXIFS_VCB 		pVCB,
	IN PFILE_OBJECT		pFileObject
);

NTSTATUS	
XixFsdUnlockVolumeInternal(
	IN PXIFS_IRPCONTEXT 	pIrpContext,
	IN PXIFS_VCB 		pVCB,
	IN PFILE_OBJECT		pFileObject
);


NTSTATUS
XixFsdCommonFileSystemControl(
	IN PXIFS_IRPCONTEXT IrpContext
	);

NTSTATUS
XixFsdCommonShutdown(
	IN PXIFS_IRPCONTEXT pIrpContext
	);


/*
 *	XixFsdFileCtrl.c	
 */

NTSTATUS
DeleteParentChild(
	IN	PXIFS_IRPCONTEXT			pIrpContext,
	IN	PXIFS_FCB					ParentFCB, 
	IN	PUNICODE_STRING				pOrgFileName
);

NTSTATUS
XixFsdQueryBasicInfo( 
	IN PXIFS_FCB 		pFCB, 
	IN PXIFS_CCB		pCCB, 
	IN OUT PFILE_BASIC_INFORMATION BasicInformation, 
	IN OUT ULONG *Length 
);

NTSTATUS
XixFsdQueryStandardInfo(
	IN PXIFS_FCB 		pFCB, 
	IN PXIFS_CCB		pCCB, 
	IN OUT PFILE_STANDARD_INFORMATION StandardInformation, 
	IN OUT ULONG *Length 
);

NTSTATUS
XixFsdQueryNetworkInfo (
	IN PXIFS_FCB 		pFCB, 
	IN PXIFS_CCB		pCCB, 
	IN OUT PFILE_NETWORK_OPEN_INFORMATION FileNetworkInformation,
	IN OUT PULONG Length
    );


NTSTATUS
XixFsdCommonQueryInformation(
	IN PXIFS_IRPCONTEXT pIrpContext
	);

NTSTATUS
XixFsdCommonSetInformation(
	IN PXIFS_IRPCONTEXT pIrpContext
	);

/*
 *	XixFsdDirCtrl.c
 */

NTSTATUS
XixFsdCommonDirectoryControl(
	IN PXIFS_IRPCONTEXT pIrpContext
	);


/*
 *	XixFsdEa.c
 */

NTSTATUS
XixFsdCommonQueryEA(
	IN PXIFS_IRPCONTEXT IrpContext
	);

NTSTATUS
XixFsdCommonSetEA(
	IN PXIFS_IRPCONTEXT IrpContext
	);


/*
 *	XixFsdFileLock.c
 */

NTSTATUS
XixFsdCommonLockControl(
	IN PXIFS_IRPCONTEXT pIrpContext
	);


/*
 *	XixFsdFlushCtrl.c
 */
VOID
XixFsdFlushFile(
	IN PXIFS_IRPCONTEXT pIrpContext, 
	IN PIRP				pIrp, 
	IN PXIFS_FCB			pFCB
);

NTSTATUS
XixFsdCommonFlushBuffers(
	IN PXIFS_IRPCONTEXT pIrpContext
);


/*
 *	XixFsdPnpCtrl.c
 */
NTSTATUS
XixFsdCommonPNP(
	IN PXIFS_IRPCONTEXT pIrpContext
	);

/*
 *	XixFsdDeviceCtrl.c
 */

NTSTATUS
XixFsdCommonDeviceControl(
	IN PXIFS_IRPCONTEXT pIrpContext
	);


/*
 *	XixFsdSecurityCtrl.c
 */

NTSTATUS
XixFsdCommonQuerySecurity(
	IN PXIFS_IRPCONTEXT IrpContext
	);


NTSTATUS
XixFsdCommonSetSecurity(
	IN PXIFS_IRPCONTEXT IrpContext
	);



/*
 *	XixFsdVolumeCtrl.c
 */
NTSTATUS
XixFsdCommonQueryVolumeInformation(
	IN PXIFS_IRPCONTEXT pIrpContext
	);

NTSTATUS
XixFsdCommonSetVolumeInformation(
	IN PXIFS_IRPCONTEXT IrpContext
	);

/*
 *	XixFsdRead.c
 */

NTSTATUS
XixFsdCommonRead(
	IN PXIFS_IRPCONTEXT PtrIrpContext
);


/*
 *	XixFsdWrite.c
 */
NTSTATUS
XixFsdCommonWrite(
	IN PXIFS_IRPCONTEXT PtrIrpContext
	);

/*
 *	XixFsdCreate.c
 */

NTSTATUS
XixFsdCommonCreate(
	IN PXIFS_IRPCONTEXT PtrIrpContext
	);

/*
 *	XixFsdCleaUp.c
 */
NTSTATUS
XixFsdCommonCleanUp(
	IN PXIFS_IRPCONTEXT pIrpContext
	);

/*
 *	XixFsdClose.c
 */
NTSTATUS
XixFsdCommonClose(
	IN PXIFS_IRPCONTEXT pIrpContext
	);
#endif //#ifndef __XIXFSD_INTERNAL_API_H__