#ifndef __PRIMARY_H__
#define __PRIMARY_H__


typedef struct	_PRIMARY_SESSION {

    LONG					ReferenceCount;
	FAST_MUTEX				FastMutex;

#define PRIMARY_SESSION_FLAG_INITIALIZING		0x00000001
#define PRIMARY_SESSION_FLAG_START				0x00000002
#define PRIMARY_SESSION_FLAG_STOPPING			0x00000004
#define PRIMARY_SESSION_FLAG_STOP				0x00000008
#define PRIMARY_SESSION_FLAG_SURPRISE_REMOVAL	0x00000010
#define PRIMARY_SESSION_FLAG_ERROR				0x80000000

	ULONG					Flags;

	PRIMARY_SESSION_ID		PrimarySessionId;

#if __NDAS_FS__
	struct _PRIMARY			*Primary;
	ULONG					ListenSocketIndex;
#else
	PVOLUME_DEVICE_OBJECT	VolDo;	
#endif

	LIST_ENTRY				ListEntry;

	NDAS_FC_STATISTICS		SendNdasFcStatistics;
	NDAS_FC_STATISTICS		RecvNdasFcStatistics;

	HANDLE					ConnectionFileHandle;
	PFILE_OBJECT			ConnectionFileObject;
	
	LPX_ADDRESS				RemoteAddress;
	BOOLEAN					IsLocalAddress;

	KEVENT					ReadyEvent;
	
	LIST_ENTRY				RequestQueue;
	KSPIN_LOCK				RequestQSpinLock;
	KEVENT					RequestEvent;

	HANDLE					ThreadHandle;
	PVOID					ThreadObject;

#if __NDAS_FS__

	LPX_ADDRESS				NetDiskAddress;
	USHORT					UnitDiskNo;

	UINT8						NdscId[NDSC_ID_LENGTH];

	LARGE_INTEGER			StartingOffset;

	SESSION_INFORMATION		SessionInformation;

#endif

	NETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation;

#if __NDAS_FS__
	PNETDISK_PARTITION				NetdiskPartition;
	LFS_FILE_SYSTEM_TYPE			FileSystemType;
	LIST_ENTRY						NetdiskPartitionListEntry;
#endif

	SESSION_CONTEXT					SessionContext;

	struct {

#define PRIMARY_SESSION_THREAD_FLAG_INITIALIZING		0x00000001
#define PRIMARY_SESSION_THREAD_FLAG_START				0x00000002
#define PRIMARY_SESSION_THREAD_FLAG_STOPED				0x00000008

#define PRIMARY_SESSION_THREAD_FLAG_TERMINATED			0x00000010

#define PRIMARY_SESSION_THREAD_FLAG_CONNECTED			0x00000100
#define PRIMARY_SESSION_THREAD_FLAG_DISCONNECTED		0x00000200

#define PRIMARY_SESSION_THREAD_FLAG_SHUTDOWN			0x00001000
#define PRIMARY_SESSION_THREAD_FLAG_SHUTDOWN_WAIT		0x00002000

#define PRIMARY_SESSION_THREAD_FLAG_UNCONNECTED			0x00100000
#define PRIMARY_SESSION_THREAD_FLAG_REMOTE_DISCONNECTED	0x00200000

#define PRIMARY_SESSION_THREAD_FLAG_ERROR				0x80000000

		ULONG			Flags;

		enum {

			SESSION_CLOSE = 1,
			SESSION_NEGOTIATE,
			SESSION_SETUP,
			SESSION_TREE_CONNECT,
			SESSION_CLOSED
		
		} SessionState;

		LPXTDI_OVERLAPPED_CONTEXT		ReceiveOverlapped;

		LIST_ENTRY				OpenedFileQueue;
		KSPIN_LOCK				OpenedFileQSpinLock;

		struct _PRIMARY_SESSION_REQUEST	*ShutdownPrimarySessionRequest;

#include <pshpack1.h>
		NDFS_REQUEST_HEADER		NdfsRequestHeader;
#include <poppack.h>

		KEVENT					WorkCompletionEvent;
		UINT16					IdleSlotCount;

#define SESSION_SLOT_COUNT	1

#include <pshpack1.h>

		struct {

			WORK_QUEUE_ITEM		WorkQueueItem;
		
			UINT32				ReplyDataSize;
			NTSTATUS			Status;

			enum _SLOT_STATE {
	
				SLOT_WAIT = 0,
				SLOT_EXECUTING,
				SLOT_FINISH

			} State;
		
			UINT32						RequestMessageBufferLength;
#if __NDAS_FS__
			UINT8						RequestMessageBuffer[sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER) + DEFAULT_MAX_DATA_SIZE];
#else
			UINT8						RequestMessageBuffer[sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER) + DEFAULT_NDAS_MAX_DATA_SIZE];
#endif

			UINT32						ExtendWinxpRequestMessagePoolLength;
			UINT8						*ExtendWinxpRequestMessagePool;

#if __NDAS_FS__
			UINT8						CryptWinxpMessageBuffer[sizeof(NDFS_WINXP_REQUEST_HEADER) + DEFAULT_MAX_DATA_SIZE];
#else
			UINT8						CryptWinxpMessageBuffer[sizeof(NDFS_WINXP_REQUEST_HEADER) + DEFAULT_NDAS_MAX_DATA_SIZE];
#endif

			PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader;

			UINT32						ReplyMessageBufferLength;
#if __NDAS_FS__
			UINT8							ReplyMessageBuffer[sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER) + DEFAULT_MAX_DATA_SIZE];
#else
			UINT8							ReplyMessageBuffer[sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER) + DEFAULT_NDAS_MAX_DATA_SIZE];
#endif

			UINT32						ExtendWinxpReplyMessagePoolLength;
			UINT8							*ExtendWinxpReplyMessagePool;
			
			PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplyHeader;

			NDFS_REQUEST_HEADER			SplitNdfsRequestHeader;
		
		} SessionSlot[SESSION_SLOT_COUNT];

#include <poppack.h>

#if __NDAS_FS__
		ULONG						BytesPerFileRecordSegment;
		ULONG						BytesPerSector;
		ULONG						BytesPerCluster;
	
		LFS_TRANS_CTX				TransportCtx;
#endif

	} Thread;

} PRIMARY_SESSION, *PPRIMARY_SESSION;


typedef enum _PRIMARY_SESSION_REQ_TYPE {

	PRIMARY_SESSION_REQ_DISCONNECT = 1,
	PRIMARY_SESSION_REQ_DISCONNECT_AND_TERMINATE,
	PRIMARY_SESSION_REQ_DOWN,
	PRIMARY_SESSION_REQ_SHUTDOWN,
	PRIMARY_SESSION_REQ_STOPPING,
	PRIMARY_SESSION_REQ_CANCEL_STOPPING,
	PRIMARY_SESSION_REQ_SUPRISE_REMOVAL

} PRIMARY_SESSION_REQ_TYPE, *PPRIMARY_SESSION_REQ_TYPE;


typedef struct _PRIMARY_SESSION_REQUEST {

	PRIMARY_SESSION_REQ_TYPE	RequestType;
	LONG						ReferenceCount;			// reference count
	LIST_ENTRY					ListEntry;

	BOOLEAN						Synchronous;
	KEVENT						CompleteEvent;

	NTSTATUS					ExecuteStatus;
	
} PRIMARY_SESSION_REQUEST, *PPRIMARY_SESSION_REQUEST;


typedef struct _OPEN_FILE {

	UINT64			OpenFileId;
	
	PFILE_OBJECT	FileObject;
	HANDLE			FileHandle;

	HANDLE			EventHandle;
	BOOLEAN			CleanUp;

	PPRIMARY_SESSION PrimarySession;
	LIST_ENTRY		 ListEntry;

} OPEN_FILE, *POPEN_FILE;


#endif
