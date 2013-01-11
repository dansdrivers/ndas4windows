#ifndef __NETDISK_MANAGER_H__
#define __NETDISK_MANAGER_H__


typedef struct _NETDISK_MANAGER {

	FAST_MUTEX			FastMutex;
    LONG				ReferenceCount;

	ERESOURCE			Resource;

	PLFS				Lfs;

	KSPIN_LOCK			EnabledNetdiskQSpinLock;		
	LIST_ENTRY			EnabledNetdiskQueue;			// Enabled Netdisk Queue

	FAST_MUTEX			NDManFastMutex;

	struct {

		HANDLE			ThreadHandle;
		PVOID			ThreadObject;

#define NETDISK_MANAGER_THREAD_INITIALIZING	0x00000001
#define NETDISK_MANAGER_THREAD_START		0x00000004
#define NETDISK_MANAGER_THREAD_ERROR		0x80000000
#define NETDISK_MANAGER_THREAD_TERMINATED	0x01000000
		
		ULONG			Flags;

		KEVENT			ReadyEvent;

		KSPIN_LOCK		RequestQSpinLock;		// protect RequestQueue
		LIST_ENTRY		RequestQueue;
		KEVENT			RequestEvent;
	
	} Thread;

} NETDISK_MANAGER, *PNETDISK_MANAGER;


typedef enum _NETDISK_VOLUME_STATE {

	VolumeEnabled = 0,
	VolumePreMounting,
	VolumePostMounting,
	VolumeSemiMounted,
	VolumeMounted,
	VolumeQueryRemove,
	VolumeStopping,
	VolumeStopped,
	VolumeSurpriseRemoved,
	VolumeDismounted

} NETDISK_VOLUME_STATE, *PNETDISK_VOLUME_STATE;


typedef struct _NETDISK_VOLUME {

	NETDISK_VOLUME_STATE	VolumeState;
	PVOID					LfsDeviceExt;	
	NTSTATUS				MountStatus;
	PDEVICE_OBJECT			AttachedToDeviceObject;

} NETDISK_VOLUME, *PNETDISK_VOLUME;


typedef struct _NETDISK_PARTITION {

	FAST_MUTEX					FastMutex;

    LONG						ReferenceCount;

#define NETDISK_PARTITION_FLAG_ENABLED				0x00000001
#define NETDISK_PARTITION_FLAG_DISABLED				0x00000002
#define NETDISK_PARTITION_FLAG_SURPRISE_REMOVED		0x00000004

#define NETDISK_PARTITION_FLAG_MOUNT_SUCCESS		0x00000010
#define NETDISK_PARTITION_FLAG_MOUNT_CORRUPTED		0x00000020

#define NETDISK_PARTITION_FLAG_PREMOUNTING			0x00000100
#define NETDISK_PARTITION_FLAG_POSTMOUNTING			0x00000200
#define NETDISK_PARTITION_FLAG_MOUNTED				0x00000400
#define NETDISK_PARTITION_FLAG_DISMOUNTED			0x00000800

#define NETDISK_PARTITION_FLAG_STOPPING				0x00001000

#define NETDISK_PARTITION_FLAG_SHUTDOWN				0x00010000
#define NETDISK_PARTITION_FLAG_ALREADY_DELETED		0x04000000
#define NETDISK_PARTITION_FLAG_ERROR				0x80000000
	
	ULONG							Flags;

	LIST_ENTRY						ListEntry;
	struct _ENABLED_NETDISK			*EnabledNetdisk;
	
	NETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation;

	NETDISK_VOLUME					NetdiskVolume[NETDISK_SECONDARY2PRIMARY+1];
	LFS_FILE_SYSTEM_TYPE			FileSystemType;

	ULONG							ExportCount;
	ULONG							LocalExportCount;

	KSPIN_LOCK						PrimarySessionQSpinLock;
	LIST_ENTRY						PrimarySessionQueue;

	PDEVICE_OBJECT					ScsiportAdapterDeviceObject;
	ULONG							ScsiportAdapterDeviceObjectReferenceCount;
	BOOLEAN							UnreferenceByQueryRemove;

	UINT16							Tid;

} NETDISK_PARTITION, *PNETDISK_PARTITION;


typedef struct _ENABLED_NETDISK {

	FAST_MUTEX					FastMutex;
    LONG						ReferenceCount;

	ERESOURCE					Resource;

	LIST_ENTRY					ListEntry;
	PNETDISK_MANAGER			NetdiskManager;

#define ENABLED_NETDISK_ENABLED					0x00000001

#define ENABLED_NETDISK_UNPLUGING				0x00000002
#define ENABLED_NETDISK_UNPLUGED				0x00000004

#define ENABLED_NETDISK_INSERT_DISK_INFORMATION	0x00000100
#define ENABLED_NETDISK_SHUTDOWN				0x00000200

#define ENABLED_NETDISK_SURPRISE_REMOVAL		0x01000000
#define ENABLED_NETDISK_CORRUPTED				0x02000000
#define ENABLED_NETDISK_ERROR					0x80000000
	
	ULONG						Flags;

	NETDISK_INFORMATION			NetdiskInformation;
	NETDISK_ENABLE_MODE			NetdiskEnableMode;

	union {

		PDRIVE_LAYOUT_INFORMATION		DriveLayoutInformation;		// for windows 2000
		PDRIVE_LAYOUT_INFORMATION_EX	DriveLayoutInformationEx;	// for winxp and later
	};

	NETDISK_PARTITION			DummyNetdiskPartition;

	KSPIN_LOCK					NetdiskPartitionQSpinLock;
	LIST_ENTRY					NetdiskPartitionQueue;

} ENABLED_NETDISK, *PENABLED_NETDISK;


typedef enum _NETDISK_MANAGER_REQUEST_TYPE {

	NETDISK_MANAGER_REQUEST_DISCONNECT = 1,
	NETDISK_MANAGER_REQUEST_DOWN,
	NETDISK_MANAGER_REQUEST_TOUCH_VOLUME,
	NETDISK_MANAGER_REQUEST_CREATE_FILE,

} NETDISK_MANAGER_REQUEST_TYPE, *PNETDISK_MANAGER_REQUEST_TYPE;


typedef struct _NETDISK_MANAGER_REQUEST {

	NETDISK_MANAGER_REQUEST_TYPE	RequestType;
	LONG							ReferenceCount;

	LIST_ENTRY						ListEntry;

	BOOLEAN							Synchronous;
	KEVENT							CompleteEvent;
	BOOLEAN							Success;
    IO_STATUS_BLOCK					IoStatus;
	PNETDISK_PARTITION				NetdiskPartition;
	
} NETDISK_MANAGER_REQUEST, *PNETDISK_MANAGER_REQUEST;


#endif
