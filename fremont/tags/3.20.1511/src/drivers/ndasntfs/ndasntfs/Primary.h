#ifndef __PRIMARY_H__
#define __PRIMARY_H__

typedef struct	_PRIMARY_SESSION {

    LONG					ReferenceCount;
	FAST_MUTEX				FastMutex;

#define PRIMARY_SESSION_FLAG_INITIALIZING	0x00000001
#define PRIMARY_SESSION_FLAG_START			0x00000002
#define PRIMARY_SESSION_FLAG_STOPPING		0x00000004
#define PRIMARY_SESSION_FLAG_STOP			0x00000008
#define PRIMARY_SESSION_FLAG_ERROR			0x80000000

	ULONG					Flags;

	PVOLUME_DEVICE_OBJECT	VolDo;	

	LIST_ENTRY				ListEntry;
	
	PIRP					Irp;

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

	NETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation;

	SESSION_CONTEXT					SessionContext;

#if __NDAS_NTFS_RECOVERY_TEST__
	LONG	ReceiveCount;
#endif

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

		TDI_RECEIVE_CONTEXT		TdiReceiveContext;
		BOOLEAN					TdiReceiving;

		LIST_ENTRY				OpenedFileQueue;
		KSPIN_LOCK				OpenedFileQSpinLock;

		struct _PRIMARY_SESSION_REQUEST	*ShutdownPrimarySessionRequest;

#include <pshpack1.h>
		NDFS_REQUEST_HEADER		NdfsRequestHeader;
#include <poppack.h>

		KEVENT					WorkCompletionEvent;
		_U16					IdleSlotCount;

#define SESSION_SLOT_COUNT	1

#include <pshpack1.h>

		struct {

			WORK_QUEUE_ITEM		WorkQueueItem;
		
			_U32				ReplyDataSize;
			NTSTATUS			Status;

			enum _SLOT_STATE {
	
				SLOT_WAIT = 0,
				SLOT_EXECUTING,
				SLOT_FINISH

			} State;
		
			_U32						RequestMessageBufferLength;
			_U8							RequestMessageBuffer[sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER) + DEFAULT_NDAS_MAX_DATA_SIZE];
			
			_U32						ExtendWinxpRequestMessagePoolLength;
			_U8							*ExtendWinxpRequestMessagePool;
			
			_U8							CryptWinxpMessageBuffer[sizeof(NDFS_WINXP_REQUEST_HEADER) + DEFAULT_NDAS_MAX_DATA_SIZE];
			
			PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader;

			_U32						ReplyMessageBufferLength;
			_U8							ReplyMessageBuffer[sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER) + DEFAULT_NDAS_MAX_DATA_SIZE];
			
			_U32						ExtendWinxpReplyMessagePoolLength;
			_U8							*ExtendWinxpReplyMessagePool;
			
			PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplyHeader;

			NDFS_REQUEST_HEADER			SplitNdfsRequestHeader;
		
		} SessionSlot[SESSION_SLOT_COUNT];

#include <poppack.h>

	} Thread;

} PRIMARY_SESSION, *PPRIMARY_SESSION;


typedef enum _PRIMARY_SESSION_REQ_TYPE {

	PRIMARY_SESSION_REQ_DISCONNECT = 1,
	PRIMARY_SESSION_REQ_DISCONNECT_AND_TERMINATE,
	PRIMARY_SESSION_REQ_DOWN,
	PRIMARY_SESSION_REQ_SHUTDOWN,
	PRIMARY_SESSION_REQ_STOPPING,
	PRIMARY_SESSION_REQ_CANCEL_STOPPING

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

	_U64			OpenFileId;
	
	PFILE_OBJECT	FileObject;
	HANDLE			FileHandle;

	HANDLE			EventHandle;
	BOOLEAN			CleanUp;

	PPRIMARY_SESSION PrimarySession;
	LIST_ENTRY		 ListEntry;

} OPEN_FILE, *POPEN_FILE;


#endif