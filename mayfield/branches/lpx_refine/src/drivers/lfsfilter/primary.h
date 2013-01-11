#ifndef __PRIMARY_H__
#define __PRIMARY_H__


typedef enum _SLOT_STATE
{
	SLOT_WAIT = 0,
	SLOT_EXECUTING,
	SLOT_FINISH

} SLOT_STATE, *PSLOT_STATE;


typedef struct	_PRIMARY_SESSION 
{
	KSPIN_LOCK				SpinLock;
    LONG					ReferenceCount;

	struct _PRIMARY			*Primary;
	ULONG					ListenSocketIndex;
    LIST_ENTRY				ListEntry;
	
	HANDLE					ConnectionFileHandle;
	PFILE_OBJECT			ConnectionFileObject;

#define PRIMARY_SESSION_INITIALIZING	0x00000001
#define PRIMARY_SESSION_START			0x00000004
#define PRIMARY_SESSION_ERROR			0x80000000

	ULONG					Flags;

	KEVENT					ReadyEvent;
	LIST_ENTRY				RequestQueue;
	KSPIN_LOCK				RequestQSpinLock;
	KEVENT					RequestEvent;

	struct _PRIMARY_SESSION_STRUCT
	{
		HANDLE			ThreadHandle;
		PVOID			ThreadObject;
	
#define PRIMARY_SESSION_THREAD_INITIALIZING	0x00000001
#define PRIMARY_SESSION_THREAD_CONNECTED	0x00000002
#define PRIMARY_SESSION_THREAD_START		0x00000004
#define PRIMARY_SESSION_THREAD_SHUTDOWN		0x00000008
#define PRIMARY_SESSION_THREAD_ERROR		0x80000000
#define PRIMARY_SESSION_THREAD_TERMINATED	0x01000000
#define PRIMARY_SESSION_THREAD_UNCONNECTED	0x02000000
#define PRIMARY_SESSION_THREAD_DISCONNECTED	0x04000000

		ULONG			ThreadFlags;

		LIST_ENTRY		OpenedFileQueue;
		KSPIN_LOCK		OpenedFileQSpinLock;


		//
		//	[64bit issue]
		//	File handle thunking module.
		//	Generates 32 bit file handle.
		//

		THUNKER32		FileHandleThunker32;

		enum
		{
			SESSION_CLOSE = 1,
			SESSION_NEGOTIATE,
			SESSION_SETUP,
			SESSION_TREE_CONNECT,
			SESSION_CLOSED
		} State;

		_U32	SessionKey;
		union 
		{
		_U8		SessionFlags;
			struct 
			{
			_U8 MessageSecurity:1;
			_U8 RwDataSecurity:1;
			_U8 Reserved:6;
			};
		};

		_U32						PrimaryMaxDataSize;
		_U32						SecondaryMaxDataSize;
		_U16						Uid;
		_U16						Tid;
		_U16						NdfsMajorVersion;
		_U16						NdfsMinorVersion;

		LPX_ADDRESS					NetDiskAddress;
		USHORT						UnitDiskNo;
		LARGE_INTEGER				StartingOffset;

		TDI_RECEIVE_CONTEXT			TdiReceiveContext;
		struct _NETDISK_PARTITION	*NetdiskPartition;
		BOOLEAN						Receiving;

		ULONG						BytesPerFileRecordSegment;
		ULONG						BytesPerSector;
		ULONG						BytesPerCluster;
		
		DES_CBC_CTX		DesCbcContext;
		_U8				Iv[8];

#define REQUEST_PER_SESSION	1

#include <pshpack1.h>

		_U16					RequestPerSession;
		NDFS_REQUEST_HEADER		NdfsRequestHeader;
		KEVENT					WorkCompletionEvent;

		struct
		{
			WORK_QUEUE_ITEM		WorkQueueItem;
		
			_U32				ReplyDataSize;
			NTSTATUS			ReturnStatus;

			SLOT_STATE			SlotState;
		
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
		
		} Slot[REQUEST_PER_SESSION];

#include <poppack.h>

	};

} PRIMARY_SESSION, *PPRIMARY_SESSION;


typedef enum _PRIMARY_SESSION_REQ_TYPE
{
	PRIMARY_SESSION_REQ_DISCONNECT = 1,
	PRIMARY_SESSION_REQ_DOWN,
	PRIMARY_SESSION_SHUTDOWN

} PRIMARY_SESSION_REQ_TYPE, *PPRIMARY_SESSION_REQ_TYPE;


typedef struct _PRIMARY_SESSION_REQUEST
{
	PRIMARY_SESSION_REQ_TYPE	RequestType;
	LONG						ReferenceCount ;			// reference count
	LIST_ENTRY					ListEntry ;

	BOOLEAN						Synchronous;
	KEVENT						CompleteEvent ;
	BOOLEAN						Success;
    IO_STATUS_BLOCK				IoStatus;
	
} PRIMARY_SESSION_REQUEST, *PPRIMARY_SESSION_REQUEST;


typedef	struct _PRIMARY_LISTEN_SOCKET
{
	BOOLEAN				Active;
	
    HANDLE				AddressFileHandle;
    PFILE_OBJECT		AddressFileObject;

	HANDLE				ListenFileHandle;
	PFILE_OBJECT		ListenFileObject;

	TDI_LISTEN_CONTEXT	TdiListenContext;
	ULONG				Flags;

	LPX_ADDRESS		NICAddress;

} PRIMARY_LISTEN_SOCKET, *PPRIMARY_LISTEN_SOCKET;


typedef enum _PRIMARY_AGENT_REQ_TYPE
{
	PRIMARY_AGENT_REQ_DISCONNECT = 1,
	PRIMARY_AGENT_REQ_DOWN,
	PRIMARY_AGENT_REQ_NIC_DISABLED,
	PRIMARY_AGENT_REQ_NIC_ENABLED,
	PRIMARY_AGENT_REQ_SHUTDOWN

} PRIMARY_AGENT_REQ_TYPE, *PPRIMARY_AGENT_REQ_TYPE;


typedef struct _PRIMARY_AGENT_REQUEST
{
	PRIMARY_AGENT_REQ_TYPE	RequestType;
	LONG					ReferenceCount;

	LIST_ENTRY				ListEntry;

	BOOLEAN					Synchronous;
	KEVENT					CompleteEvent ;
	BOOLEAN					Success;
    IO_STATUS_BLOCK			IoStatus;
	
	//
	//	data
	//
	union {
		UCHAR					ByteData[1] ;
		SOCKETLPX_ADDRESS_LIST	AddressList ;
	};
	
} PRIMARY_AGENT_REQUEST, *PPRIMARY_AGENT_REQUEST;


typedef struct _PRIMARY
{
	KSPIN_LOCK			SpinLock;
    LONG				ReferenceCount;

	PLFS				Lfs;

//	LIST_ENTRY				LfsDeviceExtQueue;			// Mounted Device Lfs Primary Device Extension Queue
//	KSPIN_LOCK				LfsDeviceExtQSpinLock;		// protect LfsDeviceExtQueue

	LIST_ENTRY				PrimarySessionQueue[MAX_SOCKETLPX_INTERFACE];			// Mounted Device Lfs Primary Device Extension Queue
	KSPIN_LOCK				PrimarySessionQSpinLock[MAX_SOCKETLPX_INTERFACE];

	struct
	{
		HANDLE					ThreadHandle;
		PVOID					ThreadObject;

#define PRIMARY_AGENT_INITIALIZING		0x00000001
#define PRIMARY_AGENT_START				0x00000004
#define PRIMARY_AGENT_ERROR				0x80000000
#define PRIMARY_AGENT_SHUTDOWN			0x01000000
#define PRIMARY_AGENT_TERMINATED		0x02000000
		
		ULONG					Flags;

		KEVENT					ReadyEvent;

		LIST_ENTRY				RequestQueue;
		KSPIN_LOCK				RequestQSpinLock;		// protect RequestQueue
		KEVENT					RequestEvent;

		USHORT					ListenPort;
	
		ULONG					ActiveListenSocketCount;
		PRIMARY_LISTEN_SOCKET	ListenSocket[MAX_SOCKETLPX_INTERFACE];
	
		SOCKETLPX_ADDRESS_LIST	SocketLpxAddressList;
	
	} Agent;


} PRIMARY, *PPRIMARY;



typedef struct _OPEN_FILE
{
	_U32			OpenFileId;
	HANDLE			FileHandle;
	PFILE_OBJECT	FileObject;
	HANDLE			EventHandle;
	BOOLEAN			AlreadyClosed;
	BOOLEAN			CleanUp;

	UNICODE_STRING	FullFileName;
    WCHAR			FullFileNameBuffer[NDFS_MAX_PATH];	
    ACCESS_MASK		DesiredAccess;
	ULONG			CreateOptions;

	PPRIMARY_SESSION PrimarySession;
	LIST_ENTRY		 ListEntry;

} OPEN_FILE, *POPEN_FILE;


#endif