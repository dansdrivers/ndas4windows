#ifndef __LFS_H__
#define __LFS_H__


#define LPX_ALLOC_TAG 'abdc'


#define UNUSED_LCN	((LONGLONG)(-1))
#define UNUSED_VCN  ((LONGLONG)(-1))


typedef struct _NETDISK_INFORMATION
{
	LPX_ADDRESS		NetDiskAddress;
	USHORT			UnitDiskNo;

	ACCESS_MASK		DesiredAccess;
	ACCESS_MASK		GrantedAccess;

	UCHAR			UserId[4];
	UCHAR			Password[8];

	BOOLEAN			MessageSecurity;
	BOOLEAN			RwDataSecurity;

	ULONG			SlotNo;
	LARGE_INTEGER	EnabledTime;
	LPX_ADDRESS		BindAddress;

} NETDISK_INFORMATION, *PNETDISK_INFORMATION;


typedef struct _NETDISK_PARTITION_INFORMATION 
{
	NETDISK_INFORMATION		NetDiskInformation;
    PARTITION_INFORMATION	PartitionInformation;
	PDEVICE_OBJECT			DiskDeviceObject;
	UNICODE_STRING			VolumeName;
    WCHAR					VolumeNameBuffer[DEVICE_NAMES_SZ];
	
} NETDISK_PARTITION_INFORMATION, *PNETDISK_PARTITION_INFORMATION;


typedef struct _NETDISK_PARTITION_INFO 
{
	LARGE_INTEGER	EnabledTime;
	LPX_ADDRESS		NetDiskAddress;
	USHORT			UnitDiskNo;
	LARGE_INTEGER	StartingOffset;

	UCHAR			UserId[4];
	UCHAR			Password[8];

} NETDISK_PARTITION_INFO, *PNETDISK_PARTITION_INFO;


typedef struct _LOCAL_NETDISK_PARTITION_INFO 
{
	NETDISK_PARTITION_INFO	NetDiskPartitionInfo;

	LPX_ADDRESS				BindAddress;

	ACCESS_MASK				DesiredAccess;
	ACCESS_MASK				GrantedAccess;

	BOOLEAN					MessageSecurity;
	BOOLEAN					RwDataSecurity;

	ULONG					SlotNo;

} LOCAL_NETDISK_PARTITION_INFO, *PLOCAL_NETDISK_PARTITION_INFO;


typedef	struct _LFS
{	
	KSPIN_LOCK				SpinLock;	
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

	LIST_ENTRY				StoppedVolumeQueue;			// Stoped Lfs Primary Device Extension Queue 
	KSPIN_LOCK				StoppedVolumeQSpinLock;

	BOOLEAN					ShutdownOccured;
	//
	//	Networking event detector
	//
	NETEVTCTX				NetEvtCtx;

	FAST_MUTEX				PurgeFastMutex;
	
} LFS, *PLFS;


typedef enum _LFS_FILTERING_MODE
{
	LFS_NO_FILTERING = 0,
	LFS_FILE_CONTROL,
	LFS_READ_ONLY,
	LFS_PRIMARY,		 
	LFS_SECONDARY,
	LFS_SECONDARY_TO_PRIMARY
	
} LFS_FILTERING_MODE, *PLFS_FILTERING_MODE;


typedef enum _LFS_SECONDARY_STATE
{
	WAIT_PURGE_SAFE_STATE = 1,
	SECONDARY_STATE,
//	WAIT_SECONDARY2PRIMARY_STATE,
	VOLUME_PURGE_STATE,
	CONNECT_TO_LOCAL_STATE,
		
} LFS_SECONDARY_STATE, *PLFS_SECONDARY_STATE;


typedef	enum _LFS_FILE_SYSTEM_TYPE
{
	LFS_FILE_SYSTEM_OTHER = 0,
	LFS_FILE_SYSTEM_NTFS,
	LFS_FILE_SYSTEM_FAT

} LFS_FILE_SYSTEM_TYPE, *PLFS_FILE_SYSTEM_TYPE;


typedef struct _LFS_DEVICE_EXTENSION
{
	KSPIN_LOCK					SpinLock;	
	FAST_MUTEX					FastMutex;
    LONG						ReferenceCount;
	
	LIST_ENTRY					LfsQListEntry;
	LIST_ENTRY					PrimaryQListEntry;
	
    PDEVICE_OBJECT				FileSpyDeviceObject;
	PFILESPY_DEVICE_EXTENSION	FileSpyDevExt;

	struct _ENABLED_NETDISK		*EnabledNetdisk;
	
#define LFS_DEVICE_INITIALIZING		0x00000001
#define LFS_DEVICE_MOUNTING			0x00000002
#define LFS_DEVICE_START			0x00000004
#define LFS_DEVICE_STOPPING			0x00000008
#define LFS_DEVICE_STOP				0x00000010
#define LFS_DEVICE_SHUTDOWN			0x00000020
#define LFS_DEVICE_SURPRISE_REMOVAL	0x01000000
#define LFS_DEVICE_ERROR			0x80000000

	ULONG							Flags ;
	BOOLEAN							Purge ;
	BOOLEAN							HookFastIo ;
	BOOLEAN							Filtering;
	LFS_FILTERING_MODE				FilteringMode;

	LFS_FILE_SYSTEM_TYPE			FileSystemType;
    PFAST_IO_ACQUIRE_FOR_MOD_WRITE	OriginalAcquireForModWrite;

    WORK_QUEUE_ITEM					WorkItem;

	PVPB							Vpb;
	PDEVICE_OBJECT					DiskDeviceObject;
	PDEVICE_OBJECT					BaseVolumeDeviceObject;
	PDEVICE_OBJECT					AttachedToDeviceObject;

	NETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation;
	LPX_ADDRESS						PrimaryAddress;
	//LOCAL_NETDISK_PARTITION_INFO	LocalNetDiskPartitionInfo;
	UNICODE_STRING					FileSystemVolumeName;
    WCHAR							FileSystemVolumeNameBuffer[DEVICE_NAMES_SZ];	

	struct _SECONDARY				*Secondary;		
	LFS_SECONDARY_STATE				SecondaryState;
	BOOLEAN							PurgeVolumeSafe;
    WORK_QUEUE_ITEM					DisMountWorkItem;
	//ULONG							OpenFileCount;
	//VPB							SecondaryVpb;

	UNICODE_STRING					MountMgr;	
	UNICODE_STRING					ExtendPlus;

#define FS_BUFFER_SIZE				PAGE_SIZE
	ULONG							BufferLength;
	PCHAR							Buffer;
	
#if DBG
	ULONG							IrpMajorFunctionCount[IRP_MJ_MAXIMUM_FUNCTION];
#endif

} LFS_DEVICE_EXTENSION, *PLFS_DEVICE_EXTENSION;


typedef struct __FAST_IO_CHECK_IF_POSSIBLE_FILE_IO
{
    PFILE_OBJECT		FileObject;
    PLARGE_INTEGER		FileOffset;
    ULONG				Length;
    BOOLEAN				Wait;
    ULONG				LockKey;
    BOOLEAN				CheckForReadOperation;
    PIO_STATUS_BLOCK	IoStatus;

} FAST_IO_CHECK_IF_POSSIBLE_FILE_IO, *PFAST_IO_CHECK_IF_POSSIBLE_FILE_IO;


typedef struct __READ_FILE_IO
{
    PFILE_OBJECT		FileObject;
    PLARGE_INTEGER		FileOffset;
    ULONG				Length;
    BOOLEAN				Wait;
    ULONG				LockKey;
    PVOID				Buffer;
    PIO_STATUS_BLOCK	IoStatus;
    
} READ_FILE_IO, *PREAD_FILE_IO;


typedef struct __WRITE_FILE_IO
{        
    PFILE_OBJECT		FileObject;
    PLARGE_INTEGER		FileOffset;
    ULONG				Length;
    BOOLEAN				Wait;
    ULONG				LockKey;
    PVOID				Buffer;
    PIO_STATUS_BLOCK	IoStatus;

} WRITE_FILE_IO, *PWRITE_FILE_IO;


typedef struct __QUERY_BASIC_INFO_FILE_IO
{        
    PFILE_OBJECT		FileObject;
    BOOLEAN				Wait;
     PVOID				Buffer;
    PIO_STATUS_BLOCK	IoStatus;

} QUERY_BASIC_INFO_FILE_IO, *PQUERY_BASIC_INFO_FILE_IO;


typedef struct __QUERY_STANDARD_INFO_FILE_IO
{        
    PFILE_OBJECT		FileObject;
    BOOLEAN				Wait;
     PVOID				Buffer;
    PIO_STATUS_BLOCK	IoStatus;

} QUERY_STANDARD_INFO_FILE_IO, *PQUERY_STANDARD_INFO_FILE_IO;


typedef struct __LOCK_FILE_IO
{        
    PFILE_OBJECT		FileObject;
    PLARGE_INTEGER		FileOffset;
    PLARGE_INTEGER		Length;
    PEPROCESS			ProcessId;		
    ULONG				Key;
    BOOLEAN				FailImmediately;
    BOOLEAN				ExclusiveLock;
    PIO_STATUS_BLOCK	IoStatus;

} LOCK_FILE_IO, *PLOCK_FILE_IO;


typedef struct __UNLOCK_SINGLE_FILE_IO
{        
    PFILE_OBJECT		FileObject;
    PLARGE_INTEGER		FileOffset;
    PLARGE_INTEGER		Length;
    PEPROCESS			ProcessId;		
    ULONG				Key;
    PIO_STATUS_BLOCK	IoStatus;

} UNLOCK_SINGLE_FILE_IO, *PUNLOCK_SINGLE_FILE_IO;


typedef struct __UNLOCK_ALL_FILE_IO
{        
    PFILE_OBJECT		FileObject;
    PEPROCESS			ProcessId;		
    PIO_STATUS_BLOCK	IoStatus;

} UNLOCK_ALL_FILE_IO, *PUNLOCK_ALL_FILE_IO;


typedef struct __UNLOCK_ALL_BY_KEY_FILE_IO
{        
    PFILE_OBJECT		FileObject;
    PEPROCESS			ProcessId;		
    ULONG				Key;
    PIO_STATUS_BLOCK	IoStatus;

} UNLOCK_ALL_BY_KEY_FILE_IO, *PUNLOCK_ALL_BY_KEY_FILE_IO;


typedef struct __MDL_READ_FILE_IO
{        
    PFILE_OBJECT		FileObject;
    PLARGE_INTEGER		FileOffset;
    ULONG				Length;
    ULONG				LockKey;
    PMDL				*MdlChain;
    PIO_STATUS_BLOCK	IoStatus;

} MDL_READ_FILE_IO, *PMDL_READ_FILE_IO;


typedef struct __MDL_READ_COMPLETE_FILE_IO
{        
    PFILE_OBJECT		FileObject;
    PMDL				MdlChain;
    PIO_STATUS_BLOCK	IoStatus;

} MDL_READ_COMPLETE_FILE_IO, *PMDL_READ_COMPLETE_FILE_IO;


typedef struct __PREPARE_MDL_WRITE_FILE_IO
{        
    PFILE_OBJECT		FileObject;
    PLARGE_INTEGER		FileOffset;
    ULONG				Length;
    ULONG				LockKey;
    PMDL				*MdlChain;
    PIO_STATUS_BLOCK	IoStatus;

} PREPARE_MDL_WRITE_FILE_IO, *PPREPARE_MDL_WRITE_FILE_IO;


typedef struct __MDL_WRITE_COMPLETE_FILE_IO
{        
    PFILE_OBJECT		FileObject;
    PLARGE_INTEGER		FileOffset;
    PMDL				MdlChain;

} MDL_WRITE_COMPLETE_FILE_IO, *PMDL_WRITE_COMPLETE_FILE_IO;


typedef struct __MDL_READ_COMPRESSED_FILE_IO
{        
    PFILE_OBJECT					FileObject;
    PLARGE_INTEGER					FileOffset;
    ULONG							Length;
    ULONG							LockKey;
    PMDL							*MdlChain;
    PIO_STATUS_BLOCK				IoStatus;
    struct _COMPRESSED_DATA_INFO	*CompressedDataInfo;
    ULONG							CompressedDataInfoLength;

} MDL_READ_COMPRESSED_FILE_IO, *PMDL_READ_COMPRESSED_FILE_IO;


typedef struct __MDL_WRITE_COMPRESSED_FILE_IO
{        
    PFILE_OBJECT					FileObject;
    PLARGE_INTEGER					FileOffset;
    ULONG							Length;
    ULONG							LockKey;
    PMDL							*MdlChain;
    PIO_STATUS_BLOCK				IoStatus;
    struct _COMPRESSED_DATA_INFO	*CompressedDataInfo;
    ULONG							CompressedDataInfoLength;

} MDL_WRITE_COMPRESSED_FILE_IO, *PMDL_WRITE_COMPRESSED_FILE_IO;


typedef struct __MDL_READ_COMPLETE_COMPRESSED_FILE_IO
{        
    PFILE_OBJECT	FileObject;
    PMDL			MdlChain;

} MDL_READ_COMPLETE_COMPRESSED_FILE_IO, *PMDL_READ_COMPLETE_COMPRESSED_FILE_IO;


typedef struct __MDL_WRITE_COMPLETE_COMPRESSED_FILE_IO
{        
    PFILE_OBJECT	FileObject;
    PLARGE_INTEGER	FileOffset;
    PMDL			MdlChain;

} MDL_WRITE_COMPLETE_COMPRESSED_FILE_IO, *PMDL_WRITE_COMPLETE_COMPRESSED_FILE_IO;


typedef struct __IO_QUERY_OPEN_FILE_IO
{        
    PIRP							Irp;
    PFILE_NETWORK_OPEN_INFORMATION	NetworkInformation;

} IO_QUERY_OPEN_FILE_IO, *PIO_QUERY_OPEN_FILE_IO;


typedef enum __LFS_FILE_IO_TYPE
{
	LFS_FILE_IO_CREATE						= 0x00,
	LFS_FILE_IO_READ						= 0x03,
	LFS_FILE_IO_WRITE						= 0x04,
	LFS_FILE_IO_FAST_IO_CHECK_IF_POSSIBLE	= 0x1c,
	LFS_FILE_IO_QUERY_BASIC_INFO			= 0x1d,
	LFS_FILE_IO_QUERY_STANDARD_INFO			= 0x1e

} LFS_FILE_IO_TYPE, *PLFS_FILE_IO_TYPE;

#if 0
#define IRP_MJ_CREATE                   0x00
#define IRP_MJ_CREATE_NAMED_PIPE        0x01
#define IRP_MJ_CLOSE                    0x02
#define IRP_MJ_READ                     0x03
#define IRP_MJ_WRITE                    0x04
#define IRP_MJ_QUERY_INFORMATION        0x05
#define IRP_MJ_SET_INFORMATION          0x06
#define IRP_MJ_QUERY_EA                 0x07
#define IRP_MJ_SET_EA                   0x08
#define IRP_MJ_FLUSH_BUFFERS            0x09
#define IRP_MJ_QUERY_VOLUME_INFORMATION 0x0a
#define IRP_MJ_SET_VOLUME_INFORMATION   0x0b
#define IRP_MJ_DIRECTORY_CONTROL        0x0c
#define IRP_MJ_FILE_SYSTEM_CONTROL      0x0d
#define IRP_MJ_DEVICE_CONTROL           0x0e
#define IRP_MJ_INTERNAL_DEVICE_CONTROL  0x0f
#define IRP_MJ_SHUTDOWN                 0x10
#define IRP_MJ_LOCK_CONTROL             0x11
#define IRP_MJ_CLEANUP                  0x12
#define IRP_MJ_CREATE_MAILSLOT          0x13
#define IRP_MJ_QUERY_SECURITY           0x14
#define IRP_MJ_SET_SECURITY             0x15
#define IRP_MJ_POWER                    0x16
#define IRP_MJ_SYSTEM_CONTROL           0x17
#define IRP_MJ_DEVICE_CHANGE            0x18
#define IRP_MJ_QUERY_QUOTA              0x19
#define IRP_MJ_SET_QUOTA                0x1a
#define IRP_MJ_PNP                      0x1b
#define IRP_MJ_PNP_POWER                IRP_MJ_PNP      // Obsolete....
#define IRP_MJ_MAXIMUM_FUNCTION         0x1b
#endif


typedef struct __LFS_FILE_IO
{
	PLFS_DEVICE_EXTENSION	LfsDeviceExt;
	LFS_FILE_IO_TYPE		FileIoType;
	
	union 
	{
		PFILE_OBJECT							FileObject;
		FAST_IO_CHECK_IF_POSSIBLE_FILE_IO		FastIoCheckIfPossible;
		READ_FILE_IO							Read;
		WRITE_FILE_IO							Write;
		QUERY_BASIC_INFO_FILE_IO				QueryBasicInfo;
		QUERY_STANDARD_INFO_FILE_IO				QueryStandardInfo;
		LOCK_FILE_IO							Lock;
		UNLOCK_SINGLE_FILE_IO					UnlockSingle;
		UNLOCK_ALL_FILE_IO						UnlockAll;
		UNLOCK_ALL_BY_KEY_FILE_IO				UnlockAllByKey;
		MDL_READ_FILE_IO						MdlRead;
		MDL_READ_COMPLETE_FILE_IO				MdlReadComplete;
		PREPARE_MDL_WRITE_FILE_IO				PrepareMdlWrite;
		MDL_READ_COMPRESSED_FILE_IO				MdlReadCompressed;
		MDL_WRITE_COMPRESSED_FILE_IO			MdlWriteCompressed;
		MDL_WRITE_COMPLETE_COMPRESSED_FILE_IO	MdlWriteCompleteCompressed;
		IO_QUERY_OPEN_FILE_IO					IoQueryOpenFile;
	};

} LFS_FILE_IO, *PLFS_FILE_IO;


extern LFS	GlobalLfs;

#endif