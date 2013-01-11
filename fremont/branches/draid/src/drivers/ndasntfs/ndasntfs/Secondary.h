#ifndef __SECONDARY_H__
#define __SECONDARY_H__


typedef enum _SECONDARY_REQ_TYPE {

	SECONDARY_REQ_CONNECT = 1,
	SECONDARY_REQ_SEND_MESSAGE,
	SECONDARY_REQ_DISCONNECT,
	SECONDARY_REQ_DOWN

} SECONDARY_REQ_TYPE, *PSECONDARY_REQ_TYPE;



typedef struct _SECONDARY {

	ERESOURCE						RecoveryResource;
	ERESOURCE						Resource;
	ERESOURCE						SessionResource;
	ERESOURCE						CreateResource;

	FAST_MUTEX						FastMutex;
		
    LONG							ReferenceCount;

#define SECONDARY_FLAG_INITIALIZING			0x00000001
#define SECONDARY_FLAG_START				0x00000002
#define SECONDARY_FLAG_CLOSED				0x00000004

#define SECONDARY_FLAG_RECONNECTING			0x00000010
#define SECONDARY_FLAG_CLEANUP_VOLUME		0x00000020
		
	ULONG							Flags;

	PVOLUME_DEVICE_OBJECT			VolDo;	

	LPX_ADDRESS						PrimaryAddress;
	
	LIST_ENTRY						RecoveryCcbQueue;
	FAST_MUTEX						RecoveryCcbQMutex;	// protects CcbQueue

	LIST_ENTRY						DeletedFcbQueue;

	ULONG							SessionId;

	HANDLE							ThreadHandle;
	PVOID							ThreadObject;
	KEVENT							ReadyEvent;

	_U8								Buffer[sizeof(NDFS_REQUEST_HEADER) + sizeof(NDFS_WINXP_REQUEST_HEADER) + DEFAULT_MAX_DATA_SIZE];

	LARGE_INTEGER					TryCloseTime;
	PIO_WORKITEM					TryCloseWorkItem;
	BOOLEAN							TryCloseActive;


	LIST_ENTRY						RequestQueue;
	KSPIN_LOCK						RequestQSpinLock;	// protects RequestQueue
	KEVENT							RequestEvent;


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
		NDFS_REPLY_HEADER			SplitNdfsReplyHeader;

	} Thread;

} SECONDARY, *PSECONDARY;


typedef struct _SECONDARY_REQUEST {

	ULONG				Flags;

#define SECONDARY_REQUEST_FLAG_ALLOC_FROM_POOL               (0x00000001)

	SECONDARY_REQ_TYPE	RequestType;
	LONG				ReferenceCount;			// reference count
	
	LIST_ENTRY			ListEntry;

	ULONG				SessionId;

	BOOLEAN				Synchronous;
	KEVENT				CompleteEvent;
	NTSTATUS			ExecuteStatus;

	ULONG				NdfsMessageAllocationSize;

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



#endif