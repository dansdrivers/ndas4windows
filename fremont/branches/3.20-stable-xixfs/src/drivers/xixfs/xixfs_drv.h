#ifndef __XIXFS_DRV_H__
#define __XIXFS_DRV_H__

#include "xixfs_types.h"
#include "xixfscontrol.h"

// When you include module.ver file, you should include <ndasverp.h> also
// in case module.ver does not include any definitions
#include "xixfs.ver"
#include <ndasverp.h>


#include "xixcore/file.h"
#include "xixcore/dir.h"
#include "xixcore/volume.h"
#include "xixcore/lotinfo.h"
#include "xixcore/lotlock.h"
#include "xixcore/layouts.h"
#include "xixcore/hostreg.h"
#include "xixcore/evtpkt.h"

#ifndef FILE_READ_ONLY_VOLUME
#define FILE_READ_ONLY_VOLUME           0x00080000  
#endif

#define XIXFS_READ_AHEAD_GRANULARITY			(0x10000)
#define XIFSD_MAX_BUFF_LIMIT					(0x10000)
#define XIFSD_MAX_IO_LIMIT						(0x10000)		


#define try_return(S)						{ (S); leave; }


#define Add2Ptr(P, I)			((PVOID)((PUCHAR)(P) + (I)))


#define XIFS_NODE_UNDEFINED					(0x0000)
#define XIFS_NODE_VCB						XIXCORE_NODE_TYPE_VCB
#define XIFS_NODE_FCB						XIXCORE_NODE_TYPE_FCB
#define XIFS_NODE_CCB						(0x1904)
#define XIFS_NODE_FS_GLOBAL					(0x1908)
#define XIFS_NODE_IRPCONTEXT				(0x1910)
#define XIFS_NODE_CLOSEFCBCTX				(0x1920)
#define XIFS_NODE_LCB						(0x1940)


#define XifsNodeType(P)			( (P) != NULL ? (*((uint16 *)(P))) : XIFS_NODE_UNDEFINED )
#define XifsSafeNodeType(P)		( *((uint16 *)(P)) )


#define TAG_FCB								'tbcf'
#define TAG_CCB								'tbcc'
#define TAG_IPCONTEXT						'tcip'
#define TAG_CLOSEFCBCTX						'tcfx'
#define TAG_CHILD							'tdch'
#define TAG_BUFFER							'tfub'
#define TAG_ADDRESS							'trda'
#define TAG_G_TABLE							'ttgf'
#define TAG_FILE_NAME						'tmnf'
#define TAG_LCB								'tbcl'
#define TAG_EXP								'tpxe'
#define TAG_IOCONTEX						'txoi'
#define TAG_REMOVE_LOCK						'tler'
#define TAG_UPCASETLB						'tltu'
#define TAG_COREBUFF						'tfbc'


typedef struct _XIFS_NODE_ID {
	uint16  NodeTypeCode;
	uint16  NodeByteSize;
}XIFS_NODE_ID, *PXIFS_NODE_ID;



/*
	NAME CHECK FLAGS
*/
#define XIFS_FILE_NAME_TYPE_INVALID			(0x00000000)
#define XIFS_FILE_NAME_TYPE_GENERAL			(0x00000001)
#define XIFS_FILE_NAME_TYPE_DOT				(0x00000002)
#define XIFS_FILE_NAME_TYPE_DOTDOT			(0x00000003)
#define XIFS_FILE_NAME_TYPE_STAR			(0x00000004)



/*
	ACCESS CHECK FLAGS
*/
#define XIFSD_DESIRED_ACCESS_FILE_READ			(FILE_GENERIC_READ | FILE_GENERIC_EXECUTE)		
#define	XIFSD_DESIRED_ACCESS_FILE_WRITE			(FILE_GENERIC_WRITE | DELETE | WRITE_DAC | WRITE_OWNER)
#define	XIFSD_DERIRED_ACCESS_DIRECTORY_READ 	(FILE_LIST_DIRECTORY | FILE_TRAVERSE )
#define	XIFSD_DERIRED_ACCESS_DIRECOTRY_WRITE	(FILE_ADD_SUBDIRECTORY | FILE_ADD_SUBDIRECTORY | FILE_DELETE_CHILD ) 




#define XIFSD_LCB_STATE_DELETE_ON_CLOSE			(0x00000001)
#define XIFSD_LCB_STATE_LINK_IS_GONE			(0x00000002)
#define XIFSD_LCB_STATE_IGNORE_CASE_SET			(0x00000010)
#define XIFSD_LCB_NOT_FROM_POOL					(0x80000000)

typedef struct _XIXFS_LCB{
	XIFS_NODE_ID			;
	LIST_ENTRY ParentFcbLinks;
	struct _XIXFS_FCB *ParentFcb;

	LIST_ENTRY ChildFcbLinks;
	struct _XIXFS_FCB *ChildFcb;


	ULONG Reference;

	ULONG LCBFlags;

	ULONG FileAttributes;


	RTL_SPLAY_LINKS Links;
	RTL_SPLAY_LINKS	IgnoreCaseLinks;


	UNICODE_STRING FileName;
	UNICODE_STRING IgnoreCaseFileName;
	
}XIXFS_LCB, *PXIXFS_LCB;


#define	XIFSD_VCB_STATE_VOLUME_MOUNTED				(0x00000001)
#define	XIFSD_VCB_STATE_VOLUME_DISMOUNT_PROGRESS	(0x00000002)
#define	XIFSD_VCB_STATE_VOLUME_DISMOUNTED			(0x00000004)
#define XIFSD_VCB_STATE_VOLUME_MOUNTING_PROGRESS	(0x00000008)
#define XIFSD_VCB_STATE_VOLUME_SHOTDOWN				(0x00001000)
#define	XIFSD_VCB_STATE_VOLUME_INVALID				(0x00000020)

#define	XIFSD_VCB_FLAGS_VOLUME_LOCKED				(0x00000100)
#define	XIFSD_VCB_FLAGS_VOLUME_READ_ONLY			(0x00000200)
#define	XIFSD_VCB_FLAGS_VCB_INITIALIZED				(0x00000400)
#define	XIFSD_VCB_FLAGS_VCB_NEED_CHECK				(0x00000800)
#define XIFSD_VCB_FLAGS_PROCESSING_PNP				(0x00010000)
#define XIFSD_VCB_FLAGS_DEFERED_CLOSE				(0x00020000)


#define XIFSD_RESIDENT_REFERENCE					2
#define XIFSD_RESIDUALUSERREF						3
#define XIFSD_RESIDUALREF							5


#define	DEFAULT_XIFS_UPDATEWAIT				( 180 * TIMERUNIT_SEC)	// 60 seconds.
#define DEFAULT_XIFS_UMOUNTWAIT				( 30 * TIMERUNIT_SEC)
#define DEFAULT_XIFS_RECHECKRESOURCE		( 180 * TIMERUNIT_SEC)

typedef struct _XIXFS_VCB{
	XIFS_NODE_ID;

	uint32 VCBCleanup;
    uint32 VCBReference;
    uint32 VCBUserReference;

	uint32 VCBResidualReference;
	uint32 VCBResidualUserReference;	


	LIST_ENTRY							VCBLink;
	struct _XI_VOLUME_DEVICE_OBJECT		*PtrVDO;
	SHARE_ACCESS  						ShareAccess;	
	PFILE_OBJECT						LockVolumeFileObject;
	
	// Syncronization object
	ERESOURCE							VCBResource;
	ERESOURCE							FileResource;
	FAST_MUTEX							ResourceMetaLock;
	FAST_MUTEX							ChildCacheLock;
	FAST_MUTEX							VCBMutex;
	PVOID								VCBLockThread;
	PFILE_OBJECT						VCBLockFileObject;
	
	
	PDEVICE_OBJECT						TargetDeviceObject;		//same value with XIXCORE_VCB->XixcoreBlockDevice
	PVPB								PtrVPB;

	PNOTIFY_SYNC						NotifyIRPSync;
	LIST_ENTRY							NextNotifyIRP;

	uint32								VCBFlags;
	uint32								VCBState;

	
	BOOLEAN								IsVolumeRemovable;


	PARTITION_INFORMATION				PartitionInformation;
	

	uint64								NdasVolBacl_Id;


	XIXCORE_VCB							XixcoreVcb;


	RTL_GENERIC_TABLE					FCBTable;

	struct _XIXFS_FCB					*VolumeDasdFCB;					
	struct _XIXFS_FCB					*RootDirFCB;
	struct _XIXFS_FCB					*MetaFCB;


	PVPB								SwapVpb;

	/* Delayed close queue */
	WORK_QUEUE_ITEM		WorkQueueItem;
	LIST_ENTRY			DelayedCloseList;
	uint32				DelayedCloseCount;

	/* Resource Check Event */
	KEVENT				ResourceEvent;

	/* Meta Delayed Write Thread */
	KEVENT				VCBUmountEvent;
	KEVENT				VCBUpdateEvent;
	KEVENT				VCBGetMoreLotEvent;
	KEVENT				VCBStopOkEvent;
	HANDLE				hMetaUpdateThread;

}XIXFS_VCB, *PXIXFS_VCB;



typedef struct _XI_VOLUME_DEVICE_OBJECT {
	DEVICE_OBJECT	DeviceObject;
    uint32 PostedRequestCount;
    uint32 OverflowQueueCount;
    LIST_ENTRY OverflowQueue;
    KSPIN_LOCK OverflowQueueSpinLock;
	XIXFS_VCB		VCB;
}XI_VOLUME_DEVICE_OBJECT, *PXI_VOLUME_DEVICE_OBJECT;


#define FCB_FILE_LOCK_INVALID		INODE_FILE_LOCK_INVALID
#define FCB_FILE_LOCK_HAS			INODE_FILE_LOCK_HAS
#define FCB_FILE_LOCK_OTHER_HAS		INODE_FILE_LOCK_OTHER_HAS


#define FCB_TYPE_FILE		(0x000000001)
#define FCB_TYPE_DIR		(0x000000002)
#define FCB_TYPE_VOLUME		(0x000000003)


#define FCB_TYPE_DIR_INDICATOR	(0x8000000000000000)
#define FCB_TYPE_FILE_INDICATOR	(0x0000000000000000)
#define FCB_ADDRESS_MASK		(0x7FFFFFFFFFFFFFFF)

#define XIFSD_TYPE_FILE(FileId)	(!((FileId) & FCB_TYPE_DIR_INDICATOR))
#define XIFSD_TYPE_DIR(FileId)	((FileId) & FCB_TYPE_DIR_INDICATOR) 


#define XIFSD_FCB_UPDATE_TIMEOUT		(10 * TIMERUNIT_SEC)	

typedef struct _XIXFS_FCB {
	union {
        FSRTL_COMMON_FCB_HEADER Header;
        FSRTL_COMMON_FCB_HEADER;
    };

	SECTION_OBJECT_POINTERS				SectionObject;
	PXIXFS_VCB							PtrVCB;
	PFILE_OBJECT						FileObject;

	ERESOURCE							MainResource;
	ERESOURCE							RealPagingIoResource;
	PERESOURCE							FileResource;

	ERESOURCE							FCBResource;
	FAST_MUTEX							AddrResource;
	FAST_MUTEX							FCBMutex;
	PVOID								FCBLockThread;
	uint32								FCBLockCount;

	/*
	May be used only File
	*/
	PFILE_LOCK							FCBFileLock;
	OPLOCK								FCBOplock;


	SHARE_ACCESS						FCBShareAccess;

	uint64								FileId;
	UNICODE_STRING						FCBFullPath;
	uint32								FCBTargetOffset;

	ULONG								FCBCleanup;
    ULONG								FCBReference;
    ULONG								FCBUserReference;
	int32								FCBCloseCount;
	uint32								FcbNonCachedOpenCount;


	uint64								RealFileSize;
	PVOID								LazyWriteThread;


	XIXCORE_FCB							XixcoreFcb;

	
    LIST_ENTRY							ParentLcbQueue;
    LIST_ENTRY							ChildLcbQueue;
	
	LIST_ENTRY							EndOfFileLink;
	LIST_ENTRY							CCBListQueue;

	PRTL_SPLAY_LINKS					Root;
	PRTL_SPLAY_LINKS					IgnoreCaseRoot;

	

}XIXFS_FCB, *PXIXFS_FCB;


/*
typedef struct _XIXFS_CCB_FILE_INFO_HINT{
	uint32			FileIndex;
	uint32			FileLength;
	PWCHAR			FileNameBuffer;
}XIXFS_CCB_FILE_INFO_HINT, *PXIXFS_CCB_FILE_INFO_HINT;
*/

/*
	CCB FLAGS
*/
#define	XIXFSD_CCB_OPENED_BY_FILEID					(0x00000001)
#define	XIXFSD_CCB_OPENED_FOR_SYNC_ACCESS			(0x00000002)
#define	XIXFSD_CCB_OPENED_FOR_SEQ_ACCESS				(0x00000004)

#define	XIXFSD_CCB_CLEANED							(0x00000008)
#define	XIXFSD_CCB_ACCESSED							(0x00000010)
#define	XIXFSD_CCB_MODIFIED							(0x00000020)

#define	XIXFSD_CCB_ACCESS_TIME_SET					(0x00000040)
#define	XIXFSD_CCB_MODIFY_TIME_SET					(0x00000080)
#define	XIXFSD_CCB_CREATE_TIME_SET					(0x00000100)

#define	XIXFSD_CCB_ALLOW_XTENDED_DASD_IO				(0x00000200)
#define XIXFSD_CCB_DISMOUNT_ON_CLOSE					(0x00001000)
#define XIXFSD_CCB_OUTOF_LINK						(0x00002000)
#define XIXFSD_CCB_OPENED_RELATIVE_BY_ID				(0x00004000)
#define XIXFSD_CCB_OPENED_AS_FILE					(0x00008000)

#define XIXFSD_CCB_FLAG_ENUM_NAME_EXP_HAS_WILD		(0x00010000)
#define XIXFSD_CCB_FLAG_ENUM_VERSION_EXP_HAS_WILD	(0x00020000)
#define XIXFSD_CCB_FLAG_ENUM_MATCH_ALL				(0x00040000)
#define XIXFSD_CCB_FLAG_ENUM_VERSION_MATCH_ALL		(0x00080000)
#define XIXFSD_CCB_FLAG_ENUM_RETURN_NEXT				(0x00100000)
#define XIXFSD_CCB_FLAG_ENUM_INITIALIZED				(0x00200000)
#define XIXFSD_CCB_FLAG_ENUM_NOMATCH_CONSTANT_ENTRY	(0x00400000)
#define XIXFSD_CCB_FLAG_NOFITY_SET					(0x00800000)


#define XIXFSD_CCB_FLAGS_DELETE_ON_CLOSE				(0x01000000)
#define XIXFSD_CCB_FLAGS_IGNORE_CASE					(0x02000000)
#define	XIXFSD_CCB_NOT_FROM_POOL						(0x80000000)



#define XIXFSD_CCB_TYPE_INVALID					XIFS_FD_TYPE_INVALID
#define XIXFSD_CCB_TYPE_FILE						(XIFS_FD_TYPE_FILE |XIFS_FD_TYPE_SYSTEM_FILE |\
														XIFS_FD_TYPE_PIPE_FILE| XIFS_FD_TYPE_DEVICE_FILE)
#define XIXFSD_CCB_TYPE_DIRECTORY				(XIFS_FD_TYPE_DIRECTORY | XIFS_FD_TYPE_ROOT_DIRECTORY)
#define XIXFSD_CCB_TYPE_PAGEFILE					XIFS_FD_TYPE_PAGE_FILE
#define	XIXFSD_CCB_TYPE_VOLUME					XIFS_FD_TYPE_VOLUME
#define XIXFSD_CCB_TYPE_MASK						(0x000008FF)




typedef struct _XIXFS_CCB {
	XIFS_NODE_ID				;
	PXIXFS_FCB					PtrFCB;
	PXIXFS_LCB					PtrLCB;
	uint64						currentFileIndex;
	uint64						highestFileIndex;
	UNICODE_STRING				SearchExpression;
	uint32						TypeOfOpen;
	uint32						CCBFlags;
	uint64						CurrentByteOffset;

	// Test
	LIST_ENTRY					LinkToFCB;
	PFILE_OBJECT				FileObject;
}XIXFS_CCB, *PXIXFS_CCB;




#define	XIFSD_IRP_CONTEXT_WAIT					(0x00000001)
#define	XIFSD_IRP_CONTEXT_WRITE_THROUGH			(0x00000002)
#define	XIFSD_IRP_CONTEXT_EXCEPTION				(0x00000004)
#define	XIFSD_IRP_CONTEXT_DEFERRED_WRITE		(0x00000008)
#define	XIFSD_IRP_CONTEXT_ASYNC_PROCESSING		(0x00000010)
#define	XIFSD_IRP_CONTEXT_NOT_TOP_LEVEL			(0x00000020)
#define XIFSD_IRP_CONTEXT_ALLOC_IO_CONTEXT		(0x00000040)
#define XIFSD_IRP_CONTEXT_RECURSIVE_CALL		(0x00000100)
#define	XIFSD_IRP_CONTEXT_NOT_FROM_POOL			(0x80000000)






typedef struct _XIXFS_IO_CONTEXT {

    //
    //  These two fields are used for multiple run Io
    //
	struct _XIXFS_IRPCONTEXT * pIrpContext;
	PIRP pIrp;
    LONG IrpCount;
    PIRP MasterIrp;
    UCHAR IrpSpFlags;
    NTSTATUS Status;
    BOOLEAN PagingIo;

    union {

        //
        //  This element handles the asynchronous non-cached Io
        //

        struct {

            PERESOURCE Resource;
            ERESOURCE_THREAD ResourceThreadId;
            ULONG RequestedByteCount;

        } Async;

        //
        //  and this element handles the synchronous non-cached Io.
        //

        KEVENT SyncEvent;

    };

} XIXFS_IO_CONTEXT, *PXIXFS_IO_CONTEXT;



#define LOCK_INVALID				(0x00000000)
#define LOCK_OWNED_BY_OWNER			(0x00000001)
#define LOCK_NOT_OWNED				(0x00000002)
#define LOCK_TIMEOUT				(0x00000004)



typedef struct _XIFS_LOCK_CONTROL{
	KEVENT KEvent;
	int32	Status;
}XIFS_LOCK_CONTROL, *PXIFS_LOCK_CONTROL;


typedef struct _XIXFS_IRPCONTEXT{
	XIFS_NODE_ID					;
	uint32							IrpContextFlags;
	uint8							MajorFunction;
	uint8							MinorFunction;
	WORK_QUEUE_ITEM					WorkQueueItem;
	PIRP							Irp;
	PDEVICE_OBJECT					TargetDeviceObject;
	NTSTATUS						SavedExceptionCode;
	PXIXFS_VCB						VCB;
	PXIXFS_IO_CONTEXT				IoContext;
	PXIXFS_FCB						*TeardownFcb;
}XIXFS_IRPCONTEXT, *PXIXFS_IRPCONTEXT;


#define	XIFSD_CTX_FLAGS_NOT_FROM_POOL				(0x80000000)

typedef struct _XIFS_CLOSE_FCB_CTX{
	XIFS_NODE_ID					;
	uint32							CtxFlags;
	LIST_ENTRY						DelayedCloseLink;
	PXIXFS_FCB						TargetFCB;
}XIFS_CLOSE_FCB_CTX, *PXIFS_CLOSE_FCB_CTX;



typedef
NTSTATUS
(*XIFS_DELAYEDPROCESSING)(
	IN PVOID			Context
    );



typedef struct _XIFS_COMPLETE_CONTEXT{
	PXIXFS_IRPCONTEXT	pIrpContext;
	KEVENT				Event;
}XIFS_COMPLETE_CONTEXT, *PXIFS_COMPLETE_CONTEXT;



#define SELF_ENTRY		0
#define PARENT_ENTRY	1
extern UNICODE_STRING XifsdUnicodeDirectoryNames[];




typedef struct _XIFS_FILE_EMUL_CONTEXT{
	PXIXFS_VCB	pVCB;
	uint64		LotNumber;
	uint32		FileType;
	PXIXCORE_BUFFER	LotHeaderBuffer;
	PXIXCORE_BUFFER Buffer;
}XIFS_FILE_EMUL_CONTEXT, *PXIFS_FILE_EMUL_CONTEXT;


#if defined(XIXCORE_DEBUG)

#define ASSERT_STRUCT(S,T)				ASSERT(XifsSafeNodeType(S) == (T))
#define ASSERT_OPTIONAL_STRUCT(S,T)		ASSERT( ((S) == NULL) || (XifsSafeNodeType(S) == (T)) )

#define ASSERT_VCB(F)					ASSERT_STRUCT((F), XIFS_NODE_VCB)
#define ASSERT_OPTIONAL_VCB(F)			ASSERT_OPTIONAL_STRUCT((F), XIFS_NODE_VCB)

#define ASSERT_FCB(F)					ASSERT_STRUCT((F), XIFS_NODE_FCB)
#define ASSERT_OPTIONAL_FCB(F)			ASSERT_OPTIONAL_STRUCT((F), XIFS_NODE_FCB)

#define ASSERT_CCB(F)					ASSERT_STRUCT((F), XIFS_NODE_CCB)
#define ASSERT_OPTIONAL_CCB(F)			ASSERT_OPTIONAL_STRUCT((F), XIFS_NODE_CCB)

#define ASSERT_IRPCONTEXT(F)			ASSERT_STRUCT((F), XIFS_NODE_IRPCONTEXT)
#define ASSERT_OPTIONAL_IRPCONTEXT(F)	ASSERT_OPTIONAL_STRUCT((F), XIFS_NODE_IRPCONTEXT)

#define ASSERT_LCB(F)					ASSERT_STRUCT((F), XIFS_NODE_LCB)
#define ASSERT_OPTIONAL_LCB(F)			ASSERT_OPTIONAL_STRUCT((F), XIFS_NODE_LCB)


#define ASSERT_EXCLUSIVE_RESOURCE(R)	ASSERT( ExIsResourceAcquiredExclusiveLite(R))	
#define ASSERT_SHARED_RESOURCE(R)		ASSERT( ExIsResourceAcquiredSharedLite(R))

#define ASSERT_EXCLUSIVE_XIFS_GDATA		ASSERT_EXCLUSIVE_RESOURCE( &XiGlobalData.DataResource)	
#define ASSERT_EXCLUSIVE_VCB(V)			ASSERT_EXCLUSIVE_RESOURCE( &(V)->VCBResource)
#define ASSERT_SHARED_VCB(V)			ASSERT_SHARED_RESOURCE ( &(V)->VCBResource)
#define ASSERT_EXCLUSIVE_FCB(F)			ASSERT_EXCLUSIVE_RESOURCE( &(F)->FCBResource)
#define ASSERT_SHARED_FCB(F)			ASSERT_SHARED_RESOURCE ( &(F)->FCBResource)
#define ASSERT_EXCLUSIVE_FILE(F)		ASSERT_EXCLUSIVE_RESOURCE( (F)->Resource)
#define ASSERT_SHARED_FILE(F)			ASSERT_SHARED_RESOURCE ( (F)->Resource)

#define ASSERT_EXCLUSIVE_FCB_OR_VCB(F)	ASSERT( ExIsResourceAcquiredExclusiveLite( &(F)->FCBResource ) || \
												ExIsResourceAcquiredExclusiveLite( &(F)->PtrVCB->VCBResource ))


#define ASSERT_LOCKED_VCB(V)			ASSERT( (V)->VCBLockThread == PsGetCurrentThread())
#define ASSERT_UNLOCKED_VCB(V)			ASSERT( (V)->VCBLockThread != PsGetCurrentThread())

#define ASSERT_LOCKED_FCB(V)			ASSERT( (V)->FCBLockThread == PsGetCurrentThread())
#define ASSERT_UNLOCKED_FCB(V)			ASSERT( (V)->FCBLockThread != PsGetCurrentThread())

#else

#define ASSERT_STRUCT(S,T)				{NOTHING;}
#define ASSERT_OPTIONAL_STRUCT(S,T)		{NOTHING;}

#define ASSERT_VCB(F)					{NOTHING;}
#define ASSERT_OPTIONAL_VCB(F)			{NOTHING;}

#define ASSERT_FCB(F)					{NOTHING;}
#define ASSERT_OPTIONAL_FCB(F)			{NOTHING;}

#define ASSERT_CCB(F)					{NOTHING;}
#define ASSERT_OPTIONAL_CCB(F)			{NOTHING;}

#define ASSERT_IRPCONTEXT(F)			{NOTHING;}
#define ASSERT_OPTIONAL_IRPCONTEXT(F)	{NOTHING;}

#define ASSERT_LCB(F)					{NOTHING;}
#define ASSERT_OPTIONAL_LCB(F)			{NOTHING;}


#define ASSERT_EXCLUSIVE_RESOURCE(R)	{NOTHING;}
#define ASSERT_SHARED_RESOURCE(R)		{NOTHING;}

#define ASSERT_EXCLUSIVE_XIFS_GDATA		{NOTHING;}
#define ASSERT_EXCLUSIVE_VCB(V)			{NOTHING;}
#define ASSERT_SHARED_VCB(V)			{NOTHING;}
#define ASSERT_EXCLUSIVE_FCB(F)			{NOTHING;}
#define ASSERT_SHARED_FCB(F)			{NOTHING;}

#define ASSERT_EXCLUSIVE_FCB_OR_VCB(F)	{NOTHING;}

#define ASSERT_LOCKED_VCB(V)			{NOTHING;}
#define ASSERT_UNLOCKED_VCB(V)			{NOTHING;}

#define ASSERT_LOCKED_FCB(V)			{NOTHING;}
#define ASSERT_UNLOCKED_FCB(V)			{NOTHING;}

#endif


#define GenericTruncate(B, U) (					\
			(B) & ~((U) - 1)					\
)

#define GenericAlign(B, U) (					\
	GenericTruncate((B) + (U) - 1, U)			\
)

#define XifsdQuadAlign(B) GenericAlign((B), 8)



#ifndef Min
#define Min(a, b)   ((a) < (b) ? (a) : (b))
#endif

#ifndef Max
#define Max(a, b)   ((a) > (b) ? (a) : (b))
#endif


typedef struct _FCB_TABLE_ENTRY{
	uint64 FileId;
	PXIXFS_FCB pFCB;
}FCB_TABLE_ENTRY, *PFCB_TABLE_ENTRY;


#endif // #ifndef __XIXFS_DRV_H__