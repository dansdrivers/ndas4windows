#ifndef __PRIMARY_H__
#define __PRIMARY_H__


typedef struct	_PRIMARY_SESSION {

    LONG					ReferenceCount;
	FAST_MUTEX				FastMutex;
	ULONG					Flags;

#define PRIMARY_SESSION_FLAG_INITIALIZING	0x00000001
#define PRIMARY_SESSION_FLAG_START			0x00000004
#define PRIMARY_SESSION_FLAG_ERROR			0x80000000

	PVOLUME_DEVICE_OBJECT	VolDo;	

	LIST_ENTRY				ListEntry;
	
	PIRP							Irp;
	HANDLE							ConnectionFileHandle;
	PFILE_OBJECT					ConnectionFileObject;
	NETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation;
	SESSION_CONTEXT					SessionContext;

	KEVENT					ReadyEvent;
	
	LIST_ENTRY				RequestQueue;
	KSPIN_LOCK				RequestQSpinLock;
	KEVENT					RequestEvent;

	HANDLE					ThreadHandle;
	PVOID					ThreadObject;


#ifdef __ND_NTFS_RECOVERY_TEST__
	LONG	ReceiveCount;
#endif

	struct _PRIMARY_SESSION_THREAD {

		ULONG			Flags;

#define PRIMARY_SESSION_THREAD_FLAG_INITIALIZING		0x00000001
#define PRIMARY_SESSION_THREAD_FLAG_START				0x00000002
#define PRIMARY_SESSION_THREAD_FLAG_STOPED				0x00000004
#define PRIMARY_SESSION_THREAD_FLAG_TERMINATED			0x00000008

#define PRIMARY_SESSION_THREAD_FLAG_CONNECTED			0x00000010
#define PRIMARY_SESSION_THREAD_FLAG_DISCONNECTED		0x00000020

#define PRIMARY_SESSION_THREAD_FLAG_SHUTDOWN			0x00000100

#define PRIMARY_SESSION_THREAD_FLAG_UNCONNECTED			0x10000000
#define PRIMARY_SESSION_THREAD_FLAG_REMOTE_DISCONNECTED	0x20000000
#define PRIMARY_SESSION_THREAD_FLAG_ERROR				0x80000000

		enum {

			SESSION_CLOSE = 1,
			SESSION_NEGOTIATE,
			SESSION_SETUP,
			SESSION_TREE_CONNECT,
			SESSION_CLOSED
		
		} SessionState;

		TDI_RECEIVE_CONTEXT	TdiReceiveContext;
		BOOLEAN				TdiReceiving;

		LIST_ENTRY				OpenedFileQueue;
		KSPIN_LOCK				OpenedFileQSpinLock;

		struct _PRIMARY_SESSION_REQUEST	*ShutdownPrimarySessionRequest;

#include <pshpack1.h>
		NDFS_REQUEST_HEADER		NdfsRequestHeader;
#include <poppack.h>
		
		KEVENT					WorkCompletionEvent;
		LONG					IdleSlotCount;

#define SESSION_SLOT_COUNT	1

		struct {

			WORK_QUEUE_ITEM		WorkQueueItem;
		
			_U32				ReplyDataSize;
			NTSTATUS			status;

			enum {

				SLOT_WAIT = 0,
				SLOT_EXECUTING,
				SLOT_FINISH

			} State;

#include <pshpack1.h>
	
			_U32						RequestMessageBufferLength;
			_U8							RequestMessageBuffer[sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER) + DEFAULT_MAX_DATA_SIZE];
			
			_U32						ExtendWinxpRequestMessagePoolLength;
			_U8							*ExtendWinxpRequestMessagePool;
			
			_U8							CryptWinxpMessageBuffer[sizeof(NDFS_WINXP_REQUEST_HEADER) + DEFAULT_MAX_DATA_SIZE];

			PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader;

			_U32						ReplyMessageBufferLength;
			_U8							ReplyMessageBuffer[sizeof(NDFS_REPLY_HEADER) + sizeof(NDFS_WINXP_REPLY_HEADER) + DEFAULT_MAX_DATA_SIZE];

			_U32						ExtendWinxpReplyMessagePoolLength;
			_U8							*ExtendWinxpReplyMessagePool;

			PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplyHeader;

			NDFS_REQUEST_HEADER			SplitNdfsRequestHeader;

#include <poppack.h>
		
		} SessionSlot[SESSION_SLOT_COUNT];

	} Thread;


} PRIMARY_SESSION, *PPRIMARY_SESSION;


typedef enum _PRIMARY_SESSION_REQ_TYPE
{
	PRIMARY_SESSION_REQ_DISCONNECT = 1,
	PRIMARY_SESSION_REQ_DOWN,
	PRIMARY_SESSION_SHUTDOWN

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

	_U32			OpenFileId;
	
	PFILE_OBJECT	FileObject;
	HANDLE			FileHandle;

	HANDLE			EventHandle;
	BOOLEAN			CleanUp;

	PPRIMARY_SESSION PrimarySession;
	LIST_ENTRY		 ListEntry;

} OPEN_FILE, *POPEN_FILE;


#endif