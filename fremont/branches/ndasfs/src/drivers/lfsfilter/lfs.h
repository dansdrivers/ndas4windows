#ifndef __LFS_H__
#define __LFS_H__


#include "NdfsProtocolHeader2.h"
#include "NdfsInteface.h"

//////////////////////////////////////////////////////////////////////////
//
//		Event queue
//

#define POOLTAG_XEVENT_QUEUE	'uqEX'
#define POOLTAG_XEVENT			'tvEX'


typedef struct _PXEVENT_QUEUE_MANAGER {

	//
	//	Event queue list
	//

	LIST_ENTRY	EventQueueList;
	KSPIN_LOCK	EventQueueListSpinLock;
	LONG		EventQueueCnt;

} XEVENT_QUEUE_MANAGER, *PXEVENT_QUEUE_MANAGER;

#define XEVENT_QUEUE_SIG	'qvEX'

typedef struct _XEVENT_QUEUE {
	//
	//	Signature
	//

	ULONG		Signature;


	//
	//	Owner process ID
	//	Informational use only.
	//	Do not access memory that this pointer points.
	//

	PEPROCESS NotifiedProcessId;


	//
	//	Event queue list
	//

	LIST_ENTRY	EventQueueEntry;


	//
	//	Event queue
	//

	LIST_ENTRY	EventList;
	KSPIN_LOCK	EventListSpinLock;
	LONG		EventCnt;


	//
	//	Notification event to a user application
	//

	HANDLE		EventOccur;
	PVOID		EventOccurObject;

} XEVENT_QUEUE, *PXEVENT_QUEUE;

typedef struct _XEVENT_ITEM_ENTRY {
	XEVENT_ITEM	EventItem;
	LIST_ENTRY	EventListEntry;
} XEVENT_ITEM_ENTRY, *PXEVENT_ITEM_ENTRY;


NTSTATUS
XEvtqInitializeEventQueueManager(
	PXEVENT_QUEUE_MANAGER EventQueueManager
);

NTSTATUS
XEvtqDestroyEventQueueManager(
	PXEVENT_QUEUE_MANAGER EventQueueManager
);

NTSTATUS
XevtqInsertEvent(
	PXEVENT_QUEUE_MANAGER	EventQueueManager,
	PXEVENT_ITEM				EventItem
);

NTSTATUS
XEvtqCreateEventQueue(
	PLFS_EVTQUEUE_HANDLE	EventQueueHandle,
	HANDLE				*EventWaitHandle
);

NTSTATUS
XEvtqCloseEventQueue(
	LFS_EVTQUEUE_HANDLE	EventQueueHandle
);

NTSTATUS
XevtqRegisterEventQueue(
	PXEVENT_QUEUE_MANAGER	EventQueueManager,
	LFS_EVTQUEUE_HANDLE		EventQueueHandle
);

NTSTATUS
XevtqUnregisterEventQueue(
	PXEVENT_QUEUE_MANAGER	EventQueueManager,
	LFS_EVTQUEUE_HANDLE		EventQueueHandle,
	BOOLEAN					SafeLocking
);

NTSTATUS
XEvtqVerifyEventQueueHandle(
	LFS_EVTQUEUE_HANDLE	EventQueueHandle
);

NTSTATUS
XEvtqQueueEvent(
	LFS_EVTQUEUE_HANDLE	EventQueueHandle,
	PXEVENT_ITEM		EventItem,
	PBOOLEAN			QueueFull
);

NTSTATUS
XEvtqGetEventHeader(
	LFS_EVTQUEUE_HANDLE	EventQueueHandle,
	PUINT32				EventLength,
	PUINT32				EventClass
);

NTSTATUS
XEvtqDequeueEvent(
	LFS_EVTQUEUE_HANDLE	EventQueueHandle,
	PXEVENT_ITEM		EventItem
);

NTSTATUS
XEvtqEmptyEventQueue(
	LFS_EVTQUEUE_HANDLE	EventQueueHandle
);

// XEVTCLS_PRIMARY_VOLUME_INVALID_OR_LOCKED

NTSTATUS
XevtQueueVolumeInvalidOrLocked(
	UINT32	PhysicalDriveNumber,
	UINT32	SlotNumber,
	UINT32	UnitNumber
);

// XEVTCLS_PRIMARY_SHUTDOWN

NTSTATUS
XevtQueueShutdown();

//////////////////////////////////////////////////////////////////////////
//
//
//


#define UNUSED_LCN	((LONGLONG)(-1))
#define UNUSED_VCN  ((LONGLONG)(-1))


typedef struct _NETDISK_PARTITION_INFO {

	LARGE_INTEGER	EnabledTime;
	LPX_ADDRESS		NetDiskAddress;
	USHORT			UnitDiskNo;
	UINT8				NdscId[NDSC_ID_LENGTH];
	LARGE_INTEGER	StartingOffset;

	UCHAR			UserId[4];
	UCHAR			Password[8];

} NETDISK_PARTITION_INFO, *PNETDISK_PARTITION_INFO;


typedef struct _LOCAL_NETDISK_PARTITION_INFO {

	NETDISK_PARTITION_INFO	NetDiskPartitionInfo;

	LPX_ADDRESS				BindAddress;

	ACCESS_MASK				DesiredAccess;
	ACCESS_MASK				GrantedAccess;

	BOOLEAN					MessageSecurity;
	BOOLEAN					RwDataSecurity;
	BOOLEAN					DiskGroup;

	ULONG					SlotNo;

} LOCAL_NETDISK_PARTITION_INFO, *PLOCAL_NETDISK_PARTITION_INFO;


typedef	struct _LFS {

	FAST_MUTEX				FastMutex;
    LONG					ReferenceCount;

    PDRIVER_OBJECT			DriverObject;
    PUNICODE_STRING			RegistryPath;
	PDEVICE_OBJECT			ControlDeviceObject;

	NPAGED_LOOKASIDE_LIST	NonPagedFcbLookasideList;
	NPAGED_LOOKASIDE_LIST	EResourceLookasideList;

	struct _NETDISK_MANAGER	*NetdiskManager;
	struct _PRIMARY			*Primary;
	struct _LFS_TABLE		*LfsTable;

	LIST_ENTRY				LfsDeviceExtQueue;
	KSPIN_LOCK				LfsDeviceExtQSpinLock;

	NDFS_CALLBACK			NdfsCallback;
	
	BOOLEAN					ShutdownOccured;

	NETEVTCTX				NetEvtCtx;
	XEVENT_QUEUE_MANAGER	EvtQueueMgr;

	BOOLEAN					NdasFsMiniMode;

	BOOLEAN					NdasNtfsRwSupport;
	BOOLEAN					NdasNtfsRwIndirect;
	BOOLEAN					NdasNtfsRoSupport;
	BOOLEAN					NdasFatRwSupport;
	BOOLEAN					NdasFatRwIndirect;
	BOOLEAN					NdasFatRoSupport;

	UNICODE_STRING	MountMgrRemoteDatabase;
	WCHAR			MountMgrRemoteDatabaseBuffer[NDFS_MAX_PATH];
	UNICODE_STRING	ExtendReparse;
	WCHAR			ExtendReparseBuffer[NDFS_MAX_PATH];
	UNICODE_STRING	MountPointManagerRemoteDatabase;	
	WCHAR			MountPointManagerRemoteDatabaseBuffer[NDFS_MAX_PATH];

	UINT16			Uid;
	UINT16			Tid;

} LFS, *PLFS;


//
//	Registry values
//
typedef	struct _LFS_REGISTRY {

	ULONG					MaxDataTransferPri;
	ULONG					MaxDataTransferSec;

} LFS_REGISTRY, *PLFS_REGISTRY;


typedef enum _LFS_FILTERING_MODE {

	LFS_NO_FILTERING = 0,
	LFS_FILE_CONTROL,
	LFS_READONLY,
	LFS_PRIMARY,
	LFS_SECONDARY,
	LFS_SECONDARY_TO_PRIMARY
	
} LFS_FILTERING_MODE, *PLFS_FILTERING_MODE;


typedef enum _LFS_SECONDARY_STATE {

	WAIT_PURGE_SAFE_STATE = 1,
	SECONDARY_STATE,
	VOLUME_PURGE_STATE,
	CONNECTED_TO_LOCAL_STATE,
		
} LFS_SECONDARY_STATE, *PLFS_SECONDARY_STATE;


typedef	enum _LFS_FILE_SYSTEM_TYPE {

	LFS_FILE_SYSTEM_OTHER = 0,
	LFS_FILE_SYSTEM_FAT,
	LFS_FILE_SYSTEM_NTFS,
	LFS_FILE_SYSTEM_NDAS_FAT,
	LFS_FILE_SYSTEM_NDAS_NTFS

} LFS_FILE_SYSTEM_TYPE, *PLFS_FILE_SYSTEM_TYPE;


//
//	LFS device extension
//

typedef struct _LFS_DEVICE_EXTENSION {

	FAST_MUTEX					FastMutex;
    LONG						ReferenceCount;
	
	LIST_ENTRY					LfsQListEntry;
	
    PDEVICE_OBJECT				FileSpyDeviceObject;

#if __NDAS_FS_MINI__
	PFLT_CONTEXT				InstanceContext;
#endif

	PVOID						NetdiskPartition;
	NETDISK_ENABLE_MODE			NetdiskEnabledMode;
	
#define LFS_DEVICE_FLAG_INITIALIZING		0x00000001
#define LFS_DEVICE_FLAG_MOUNTING			0x00000002
#define LFS_DEVICE_FLAG_MOUNTED				0x00000004
#define LFS_DEVICE_FLAG_DISMOUNTING			0x00000008
#define LFS_DEVICE_FLAG_QUERY_REMOVE		0x00000010
#define LFS_DEVICE_FLAG_DISMOUNTED			0x00000020
#define LFS_DEVICE_FLAG_SHUTDOWN			0x00000040
#define LFS_DEVICE_FLAG_REGISTERED			0x00000100
#define LFS_DEVICE_FLAG_INDIRECT			0x00001000
#define LFS_DEVICE_FLAG_CLNEANUPED			0x01000000
#define LFS_DEVICE_FLAG_SURPRISE_REMOVED	0x02000000
#define LFS_DEVICE_FLAG_ERROR				0x80000000

	ULONG							Flags;

	LFS_FILE_SYSTEM_TYPE			FileSystemType;
	LFS_FILTERING_MODE				FilteringMode;

    WORK_QUEUE_ITEM					WorkItem;

	PVPB							Vpb;

	PDEVICE_OBJECT					DiskDeviceObject;
	PDEVICE_OBJECT					MountVolumeDeviceObject;

	PDEVICE_OBJECT					BaseVolumeDeviceObject;
	PDEVICE_OBJECT					AttachedToDeviceObject;

	NETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation;
	LPX_ADDRESS						PrimaryAddress;
	UNICODE_STRING					FileSystemVolumeName;
    WCHAR							FileSystemVolumeNameBuffer[DEVICE_NAMES_SZ];	

	struct _SECONDARY				*Secondary;		
	LFS_SECONDARY_STATE				SecondaryState;
	ULONG							ProcessingIrpCount;
	
	//
	//	Set TRUE if this volume is locked
	//	through LOCK_VOLUME file system control
	//

	BOOLEAN							VolumeLocked;

#if DBG
	ULONG							IrpMajorFunctionCount[IRP_MJ_MAXIMUM_FUNCTION];
#endif

#define LFS_DEVICE_EXTENTION_THREAD_FLAG_TIME_OUT			(3*NANO100_PER_SEC)
#define LFS_DEVICE_EXTENTION_TRY_FLUSH_OR_PURGE_DURATION	(LFS_DEVICE_EXTENTION_THREAD_FLAG_TIME_OUT - 1*NANO100_PER_SEC)

	LARGE_INTEGER					TryFlushOrPurgeTime;
	LARGE_INTEGER					CommandReceiveTime;
	BOOLEAN							ReceiveWriteCommand;

	HANDLE							ThreadHandle;
	PVOID							ThreadObject;
	
	KEVENT							ReadyEvent;
	KEVENT							RequestEvent;

	struct _READONLY				*Readonly;		


	struct {

#define LFS_DEVICE_EXTENTION_THREAD_FLAG_INITIALIZING			0x00000001
#define LFS_DEVICE_EXTENTION_THREAD_FLAG_START					0x00000002
#define LFS_DEVICE_EXTENTION_THREAD_FLAG_STOPED					0x00000004
#define LFS_DEVICE_EXTENTION_THREAD_FLAG_TERMINATED				0x00000008

#define LFS_DEVICE_EXTENTION_THREAD_FLAG_CONNECTED				0x00000010
#define LFS_DEVICE_EXTENTION_THREAD_FLAG_DISCONNECTED			0x00000020

#define LFS_DEVICE_EXTENTION_THREAD_FLAG_UNCONNECTED			0x10000000
#define LFS_DEVICE_EXTENTION_THREAD_FLAG_REMOTE_DISCONNECTED	0x20000000
#define LFS_DEVICE_EXTENTION_THREAD_FLAG_ERROR					0x80000000
		
		ULONG						Flags;
	
	} Thread;

} LFS_DEVICE_EXTENSION, *PLFS_DEVICE_EXTENSION;



typedef enum __LFS_FILE_IO_TYPE
{
	LFS_FILE_IO_CREATE						= 0x00,
	LFS_FILE_IO_READ						= 0x03,
	LFS_FILE_IO_WRITE						= 0x04,
	LFS_FILE_IO_FAST_IO_CHECK_IF_POSSIBLE	= 0x1c,
	LFS_FILE_IO_QUERY_BASIC_INFO			= 0x1d,
	LFS_FILE_IO_QUERY_STANDARD_INFO			= 0x1e

} LFS_FILE_IO_TYPE, *PLFS_FILE_IO_TYPE;



#ifndef NTDDI_VERSION 

NTKERNELAPI
VOID
FsRtlTeardownPerStreamContexts (
    __in PFSRTL_ADVANCED_FCB_HEADER AdvancedHeader
    );

//
//  Function pointer to above routine for modules that need to dynamically import
//

typedef VOID (*PFN_FSRTLTEARDOWNPERSTREAMCONTEXTS) (__in PFSRTL_ADVANCED_FCB_HEADER AdvancedHeader);

#endif

extern LFS									GlobalLfs;
extern LFS_REGISTRY							LfsRegistry;
extern PFN_FSRTLTEARDOWNPERSTREAMCONTEXTS	LfsFsRtlTeardownPerStreamContexts;


//////////////////////////////////////////////////////////////////////////
//
//	LFS event log entry
//

#define LFS_MAX_ERRLOG_DATA_ENTRIES	4

typedef struct _LFS_ERROR_LOG_ENTRY {

	UCHAR MajorFunctionCode;
	UCHAR DumpDataEntry;
	ULONG ErrorCode;
	ULONG IoctlCode;
	ULONG UniqueId;
	ULONG ErrorLogRetryCount;
	ULONG SequenceNumber;
	ULONG Parameter2;
	ULONG FinalStatus;
	ULONG DumpData[LFS_MAX_ERRLOG_DATA_ENTRIES];

} LFS_ERROR_LOG_ENTRY, *PLFS_ERROR_LOG_ENTRY;


#endif