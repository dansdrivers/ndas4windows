#ifndef __SECONDARY_H__
#define __SECONDARY_H__


typedef enum _SECONDARY_REQ_TYPE {

	SECONDARY_REQ_CONNECT = 1,
	SECONDARY_REQ_SEND_MESSAGE,
	SECONDARY_REQ_DISCONNECT,
	SECONDARY_REQ_DOWN

} SECONDARY_REQ_TYPE, *PSECONDARY_REQ_TYPE;


typedef struct _SECONDARY {

	KSEMAPHORE						Semaphore;
	//_U16							SemaphoreConsumeRequiredCount;		
	//_U16							SemaphoreReturnCount;

	FAST_MUTEX						FastMutex ;	//  make critical section.
	
    LONG							ReferenceCount;

#define SECONDARY_FLAG_INITIALIZING	0x00000001
#define SECONDARY_FLAG_START		0x00000002
#define SECONDARY_FLAG_CLOSED		0x00000004

#define SECONDARY_FLAG_RECONNECTING	0x00000010
#define SECONDARY_FLAG_ERROR		0x80000000
		
	ULONG							Flags;

	struct _LFS_DEVICE_EXTENSION	*LfsDeviceExt;
	
	LPX_ADDRESS						PrimaryAddress;

	ULONG							SessionId;
	
	LIST_ENTRY						FcbQueue;
	KSPIN_LOCK						FcbQSpinLock;	// protects FcbQueue

	LIST_ENTRY						FileExtQueue;
	FAST_MUTEX						FileExtQMutex;	// protects FileExtQueue
	ULONG							FileExtCount;

	LARGE_INTEGER					TryCloseTime;

	HANDLE							ThreadHandle;
	PVOID							ThreadObject;
	KEVENT							ReadyEvent;

	LIST_ENTRY						RequestQueue;
	KSPIN_LOCK						RequestQSpinLock;	// protects RequestQueue
	KEVENT							RequestEvent;

    LIST_ENTRY						DirNotifyList;
    PNOTIFY_SYNC					NotifySync;

	TDI_CONNECTION_INFO				ConnectionInfo;

	struct {

#define SECONDARY_THREAD_FLAG_INITIALIZING			0x00000001
#define SECONDARY_THREAD_FLAG_START					0x00000002
#define SECONDARY_THREAD_FLAG_STOPED				0x00000004
#define SECONDARY_THREAD_FLAG_TERMINATED			0x00000008

#define SECONDARY_THREAD_FLAG_CONNECTED				0x00000010
#define SECONDARY_THREAD_FLAG_DISCONNECTED			0x00000020

#define SECONDARY_THREAD_FLAG_UNCONNECTED			0x10000000
#define SECONDARY_THREAD_FLAG_REMOTE_DISCONNECTED	0x20000000
#define SECONDARY_THREAD_FLAG_ERROR					0x80000000

		ULONG						Flags;

		NTSTATUS					SessionStatus;

		HANDLE						AddressFileHandle;
		PFILE_OBJECT				AddressFileObject;

		HANDLE						ConnectionFileHandle;
		PFILE_OBJECT				ConnectionFileObject;

		struct _SECONDARY_REQUEST	*SessionSlot[MAX_SLOT_PER_SESSION];
		LONG						IdleSlotCount;

		LONG						ReceiveWaitingCount;
		TDI_RECEIVE_CONTEXT			TdiReceiveContext;
		NDFS_REPLY_HEADER			NdfsReplyHeader;
		
		SESSION_CONTEXT				SessionContext;
		LFS_TRANS_CTX				TransportCtx;

		NDFS_REPLY_HEADER			SplitNdfsReplyHeader;
		
		BOOLEAN						DiskGroup;
		LONG						VolRefreshTick;

	}Thread;
	
} SECONDARY, *PSECONDARY;


typedef struct _SECONDARY_REQUEST {

	SECONDARY_REQ_TYPE		RequestType;
	LONG					ReferenceCount;			// reference count
	
	LIST_ENTRY				ListEntry;

	ULONG					SessionId;

	BOOLEAN					Synchronous;
	KEVENT					CompleteEvent;
	NTSTATUS				ExecuteStatus;

	ULONG					OutputBufferLength;
   	PUCHAR					OutputBuffer;

	struct	_FILE_EXTENTION	*FileExt;
	BOOLEAN					PotentialDeadVolLock;

	ULONG					NdfsMessageAllocationSize;

#include <pshpack1.h>

	union {

		_U8		NdfsMessage[1];

		UNALIGNED struct {

			NDFS_REQUEST_HEADER	NdfsRequestHeader;
			_U8					NdfsRequestData[1];
		};

		UNALIGNED struct {

			NDFS_REPLY_HEADER	NdfsReplyHeader;
			_U8					NdfsReplyData[1];
		};
	};

#include <poppack.h>
	
} SECONDARY_REQUEST, *PSECONDARY_REQUEST;


typedef struct _NON_PAGED_FCB {

	SECTION_OBJECT_POINTERS	SectionObjectPointers;
	FAST_MUTEX				AdvancedFcbHeaderMutex;

} NON_PAGED_FCB, *PNON_PAGED_FCB;


typedef struct _LFS_FCB {

    FSRTL_ADVANCED_FCB_HEADER	Header;
    PNON_PAGED_FCB				NonPaged;

    FILE_LOCK					FileLock;

	LONG						ReferenceCount;			// reference count
	LIST_ENTRY					ListEntry;

	UNICODE_STRING				FullFileName;
    WCHAR						FullFileNameBuffer[NDFS_MAX_PATH];	

	UNICODE_STRING				CaseInSensitiveFullFileName;
    WCHAR						CaseInSensitiveFullFileNameBuffer[NDFS_MAX_PATH];	

	LONG						OpenCount;
	LONG						UncleanCount;
	BOOLEAN						FileRecordSegmentHeaderAvail;
	BOOLEAN						DeletePending;
	LARGE_INTEGER				OpenTime;
	CHAR						Buffer[1];

} LFS_FCB, *PLFS_FCB;


typedef enum _TYPE_OF_OPEN {

    UnopenedFileObject = 1,
    UserFileOpen,
    UserDirectoryOpen,
    UserVolumeOpen,
    VirtualVolumeFile,
    DirectoryFile,
    EaFile

} TYPE_OF_OPEN;


typedef struct	_FILE_EXTENTION {

	UINT32					LfsMark;
	PSECONDARY				Secondary;
	PFILE_OBJECT			FileObject;
	BOOLEAN					RelatedFileObjectClosed;

	PLFS_FCB				Fcb;
	
	LIST_ENTRY				ListEntry;

	BOOLEAN					Corrupted;
	ULONG					SessionId;
	_U64					PrimaryFileHandle;

	TYPE_OF_OPEN			TypeOfOpen;
	
	_U32					IrpFlags;
	_U8						IrpSpFlags;
	WINXP_REQUEST_CREATE	CreateContext;

	VCN						CachedVcn;
	PCHAR					Cache;

	//
	// Valid only in secondary mode(or handles opened in secondary mode) 
	//			  and after IRP_MJ_DIRECTORY_CONTROL/IRP_MN_QUERY_DIRECTORY is called with DirectoryFile.
	// Used to restore current file query index after reconnecting to another primary. 
	// Set to (ULONG)-1 for unused.
	//
	ULONG					LastQueryFileIndex;

	// 
	//	Set when LastQueryFileIndex is set with current session's ID
	//  Need to use LastQueryFileIndex value for next query if session ID is changed.
	//
	ULONG					LastDirectoryQuerySessionId;

		
#if __TEST_MODE__
	PVOID					OriginalFsContext;
	PVOID					OriginalFsContext2;
#endif

	ULONG					BufferLength;
	_U8						Buffer[1];

} FILE_EXTENTION, *PFILE_EXTENTION;


#endif