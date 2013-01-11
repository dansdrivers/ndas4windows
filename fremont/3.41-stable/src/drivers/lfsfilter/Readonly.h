#ifndef __READONLY_H__
#define __READONLY_H__


typedef enum _READONLY_REQ_TYPE {

	READONLY_REQ_CONNECT = 1,
	READONLY_REQ_SEND_MESSAGE,
	READONLY_REQ_DISCONNECT,
	READONLY_REQ_DOWN

} READONLY_REQ_TYPE, *PREADONLY_REQ_TYPE;


typedef struct _READONLY {

	KSEMAPHORE						Semaphore;
	//UINT16							SemaphoreConsumeRequiredCount;		
	//UINT16							SemaphoreReturnCount;

	FAST_MUTEX						FastMutex ;	//  make critical section.
	
    LONG							ReferenceCount;

#define READONLY_FLAG_INITIALIZING	0x00000001
#define READONLY_FLAG_START			0x00000002
#define READONLY_FLAG_CLOSED		0x00000004

#define READONLY_FLAG_RECONNECTING	0x00000010
#define READONLY_FLAG_ERROR			0x80000000
		
	ULONG							Flags;

	struct _LFS_DEVICE_EXTENSION	*LfsDeviceExt;
	
	LPX_ADDRESS						PrimaryAddress;

	ULONG							SessionId;
	
	LIST_ENTRY						FcbQueue;
	KSPIN_LOCK						FcbQSpinLock;	// protects FcbQueue

	LIST_ENTRY						CcbQueue;
	FAST_MUTEX						CcbQMutex;	// protects CcbQueue
	ULONG							CcbCount;

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

	HANDLE							DismountThreadHandle;
	KEVENT							DiskmountReadyEvent;

	//CHAR							Buffer[4096];

	struct {

#define READONLY_THREAD_FLAG_INITIALIZING			0x00000001
#define READONLY_THREAD_FLAG_START					0x00000002
#define READONLY_THREAD_FLAG_STOPED					0x00000004
#define READONLY_THREAD_FLAG_TERMINATED				0x00000008

#define READONLY_THREAD_FLAG_CONNECTED				0x00000010
#define READONLY_THREAD_FLAG_DISCONNECTED			0x00000020

#define READONLY_THREAD_FLAG_UNCONNECTED			0x10000000
#define READONLY_THREAD_FLAG_REMOTE_DISCONNECTED	0x20000000
#define READONLY_THREAD_FLAG_ERROR					0x80000000

		ULONG						Flags;

		NTSTATUS					SessionStatus;

		HANDLE						AddressFileHandle;
		PFILE_OBJECT				AddressFileObject;

		HANDLE						ConnectionFileHandle;
		PFILE_OBJECT				ConnectionFileObject;

		struct _READONLY_REQUEST	*SessionSlot[MAX_SLOT_PER_SESSION];
		LONG						IdleSlotCount;

		LONG						ReceiveWaitingCount;
		//TDI_RECEIVE_CONTEXT		TdiReceiveContext;
		NDFS_REPLY_HEADER			NdfsReplyHeader;
		
		SESSION_CONTEXT				SessionContext;
		LFS_TRANS_CTX				TransportCtx;

		NDFS_REPLY_HEADER			SplitNdfsReplyHeader;
		
		BOOLEAN						DiskGroup;
		LONG						VolRefreshTick;

	}Thread;
	
} READONLY, *PREADONLY;


typedef struct _READONLY_REQUEST {

	READONLY_REQ_TYPE		RequestType;
	LONG					ReferenceCount;			// reference count
	
	LIST_ENTRY				ListEntry;

	ULONG					SessionId;

	BOOLEAN					Synchronous;
	KEVENT					CompleteEvent;
	NTSTATUS				ExecuteStatus;

	ULONG					OutputBufferLength;
   	PUCHAR					OutputBuffer;

	struct	_NDAS__CCB		*Ccb;
	BOOLEAN					PotentialDeadVolLock;

	ULONG					NdfsMessageAllocationSize;

#include <pshpack1.h>

	union {

		UINT8		NdfsMessage[1];

		UNALIGNED struct {

			NDFS_REQUEST_HEADER	NdfsRequestHeader;
			UINT8					NdfsRequestData[1];
		};

		UNALIGNED struct {

			NDFS_REPLY_HEADER	NdfsReplyHeader;
			UINT8					NdfsReplyData[1];
		};
	};

#include <poppack.h>
	
} READONLY_REQUEST, *PREADONLY_REQUEST;


typedef struct _NDAS_FCB {

    FSRTL_ADVANCED_FCB_HEADER	Header;
    PNON_PAGED_FCB				NonPaged;

    FILE_LOCK					FileLock;

	LONG						ReferenceCount;			// reference count
	LIST_ENTRY					ListEntry;

	PREADONLY					Readonly;

	UNICODE_STRING				FullFileName;
    WCHAR						FullFileNameBuffer[NDFS_MAX_PATH];	

	UNICODE_STRING				CaseInSensitiveFullFileName;
    WCHAR						CaseInSensitiveFullFileNameBuffer[NDFS_MAX_PATH];	

	LONG						OpenCount;
	LONG						UncleanCount;

	BOOLEAN						DeletePending;

	BOOLEAN						FileRecordSegmentHeaderAvail;
	//FILE_RECORD_SEGMENT_HEADER	FileRecordSegmentHeader;

} NDAS_FCB, *PNDAS_FCB;

typedef struct _CREATE_CONTEXT {

	UINT32	IrpFlags;
	UINT8		IrpSpFlags;

	UINT32	Options;
    UINT16	FileAttributes;
    UINT16	ShareAccess;
    UINT32	EaLength;

	HANDLE	RelatedFileHandle;
	UINT16	FileNameLength;

	UINT64	AllocationSize;

	struct {

	    UINT32	DesiredAccess;
		UINT32	FullCreateOptions;

		struct {

			UINT32	Flags;
			UINT32	RemainingDesiredAccess;
			UINT32	PreviouslyGrantedAccess;
			UINT32	OriginalDesiredAccess;

		} AccessState;
	
	} SecurityContext;

} CREATE_CONTEXT, *PCREATE_CONTEXT;

#define CCB_MARK	0xF23AC137 // It's Real Random Number

typedef struct	_NDAS_CCB {

	UINT32			Mark;

	PREADONLY		Readonly;
	PFILE_OBJECT	FileObject;
	
	BOOLEAN			RelatedFileObjectClosed;

	PNDAS_FCB		Fcb;
	
	LIST_ENTRY		ListEntry;

	BOOLEAN			Corrupted;
	ULONG			SessionId;

	HANDLE			ReadonlyFileHandle;
	PFILE_OBJECT	ReadonlyFileObject;

	TYPE_OF_OPEN	TypeOfOpen;

	//
	// Valid only in secondary mode(or handles opened in secondary mode) 
	//			  and after IRP_MJ_DIRECTORY_CONTROL/IRP_MN_QUERY_DIRECTORY is called with DirectoryFile.
	// Used to restore current file query index after reconnecting to another primary. 
	// Set to (ULONG)-1 for unused.
	//

	ULONG			LastQueryFileIndex;

	// 
	//	Set when LastQueryFileIndex is set with current session's ID
	//  Need to use LastQueryFileIndex value for next query if session ID is changed.
	//

	ULONG			LastDirectoryQuerySessionId;

	CREATE_CONTEXT	CreateContext;

	ULONG			BufferLength;
	UINT8				Buffer[1];

} NDAS_CCB, *PNDAS_CCB;

#define READONLY_REDIRECT_REQUEST_TAG	0x2132280

typedef struct _READONLY_REDIRECT_REQUEST {

	ULONG							Tag;
#if DBG
	ULONG							DebugTag;
#endif
	IN  PFILESPY_DEVICE_EXTENSION	DevExt;
	PIRP							OriginalTopLevelIrp;
	
	struct {

		UCHAR	ReadonlyDismount:1;
		UCHAR	reserved:7;
	};

} READONLY_REDIRECT_REQUEST, *PREADONLY_REDIRECT_REQUEST;

#endif
