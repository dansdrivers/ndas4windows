#ifndef __LFS_PROC_H__
#define __LFS_PROC_H__

#include <ntifs.h>
#include <stdlib.h>
#include <initguid.h>
#include <ntdddisk.h>
#include <ntddvol.h>


#if WINVER < 0x0501
#include "Win2kHeader.h"
#endif

#include "ioevent.h"

// from <ntddk.h>
NTKERNELAPI
NTSTATUS
NTAPI
IoGetDeviceInterfaces(
    IN CONST GUID		*InterfaceClassGuid,
    IN PDEVICE_OBJECT	PhysicalDeviceObject OPTIONAL,
    IN ULONG			Flags,
    OUT PWSTR			*SymbolicLinkList
    );

NTSYSAPI
NTSTATUS
NTAPI
ZwQueryEaFile(
    IN HANDLE FileHandle,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    OUT PVOID Buffer,
    IN ULONG Length,
    IN BOOLEAN ReturnSingleEntry,
    IN PVOID EaList OPTIONAL,
    IN ULONG EaListLength,
    IN PULONG EaIndex OPTIONAL,
    IN BOOLEAN RestartScan
    );


NTSYSAPI
NTSTATUS
NTAPI
ZwSetEaFile(
    IN HANDLE FileHandle,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    IN PVOID Buffer,
    IN ULONG Length
    );

//
// The following structure header is used for all other (i.e., 3rd-party)
// target device change events.  The structure accommodates both a
// variable-length binary data buffer, and a variable-length unicode text
// buffer.  The header must indicate where the text buffer begins, so that
// the data can be delivered in the appropriate format (ANSI or Unicode)
// to user-mode recipients (i.e., that have registered for handle-based
// notification via RegisterDeviceNotification).
//

typedef struct _TARGET_DEVICE_CUSTOM_NOTIFICATION {
    USHORT Version;
    USHORT Size;
    GUID Event;
    //
    // Event-specific data
    //
    PFILE_OBJECT FileObject;    // This field must be set to NULL by callers of
                                // IoReportTargetDeviceChange.  Clients that
                                // have registered for target device change
                                // notification on the affected PDO will be
                                // called with this field set to the file object
                                // they specified during registration.
                                //
    LONG NameBufferOffset;      // offset (in bytes) from beginning of
                                // CustomDataBuffer where text begins (-1 if none)
                                //
    UCHAR CustomDataBuffer[1];  // variable-length buffer, containing (optionally)
                                // a binary data at the start of the buffer,
                                // followed by an optional unicode text buffer
                                // (word-aligned).
                                //
} TARGET_DEVICE_CUSTOM_NOTIFICATION, *PTARGET_DEVICE_CUSTOM_NOTIFICATION;


NTKERNELAPI
NTSTATUS
IoReportTargetDeviceChange(
    IN PDEVICE_OBJECT PhysicalDeviceObject,
    IN PVOID NotificationStructure  // always begins with a PLUGPLAY_NOTIFICATION_HEADER
    );


typedef struct _FILESPY_DEVICE_EXTENSION *PFILESPY_DEVICE_EXTENSION;
#define DEVICE_NAMES_SZ  100

#include <SocketLpx.h>
#include <lpxtdi.h>
#include <ndasbusioctl.h>
#include <lfsfilterpublic.h>

#include "thunk.h"

#include "NdfsProtocolHeader.h"
#include "NdftProtocolHeader.h"
#include "md5.h"
#include "rsaeuro.h"
#include "des.h"

#include "inc\lfs.h"
#include "inc\ntfs.h"

#include "LfsDbg.h"

#include "FastIoDispatch.h"
#include "Lfs.h"

#ifdef __NETDISK_MANAGER__
#include "NetdiskManager.h"
#endif

//
// lfslib.c
//
#define LL_MAX_TRANSMIT_DATA	(64*1024)
#define LL_MIN_TRANSMIT_DATA	(1024)
#define LL_TRANSMIT_UNIT		(512)

typedef struct _LFS_TRANS_CTX {
	// data transfer rate
	LONG		MaxSendBytes;
	LONG		DynMaxSendBytes;

	// stats
	TRANS_STAT	AccSendTransStat;
	LONG		StableSendCnt;
} LFS_TRANS_CTX, *PLFS_TRANS_CTX;


#ifdef __PRIMARY__
#include "Primary.h"
#endif

#ifdef __SECONDARY__
#include "Secondary.h"
#endif

#ifdef __READONLY__

#ifndef _WIN64

#include "W2kNtfsLib.h"
#include "W2kFatLib.h"

#endif

#include "WxpNtfsLib.h"
#include "WxpFatLib.h"
#include "WnetFatLib.h"
#endif

#include "LfsTable.h"

#include "filespy.h"
#include "fspyKern.h"


#define LFS_TIME_OUT					(HZ * 180)  // This timeout should not happen. Because of possible delay in ConnectToPrimary, set to at least over 120 sec
#define LFS_SECONDARY_THREAD_TIME_OUT	(10*HZ)
#define LFS_CHANGECONNECTION_TIMEOUT	((RECONNECTION_MAX_TRY * MAX_RECONNECTION_INTERVAL) * 2)
#define LFS_MARK						0xF23AC135 // It's Real Random Number

#define LFS_INSUFFICIENT_RESOURCES	0
#define LFS_BUG						0
#define LPX_BUG						0
#define LFS_UNEXPECTED				0
#define LFS_REQUIRED				0
#define LFS_IRP_NOT_IMPLEMENTED		0
#define LFS_CHECK					0

#define LFS_ALLOC_TAG				'taFL'
#define LFS_FCB_NONPAGED_TAG		'nfFL'
#define LFS_FCB_TAG                 'tfFL'
#define LFS_ERESOURCE_TAG			'teFL'

#define NETDISK_MANAGER_TAG			'mnFL'
#define NETDISK_MANAGER_REQUEST_TAG	'rmFL'
#define NETDISK_MANAGER_ENABLED_TAG	'emFL'
#define NETDISK_MANAGER_PARTITION_TAG	'pmFL'

#define PRIMARY_AGENT_MESSAGE_TAG	'apFL'
#define PRIMARY_SESSION_MESSAGE_TAG	'msFL'
#define PRIMARY_SESSION_BUFFERE_TAG	'bpFL'
#define SECONDARY_MESSAGE_TAG		'esFL'
#define FILE_EXT_TAG				'efFL'
#define OPEN_FILE_TAG				'foFL'
#define STOPPED_VOLUME_TAG			'vsFL'

#define LFSTAB_ENTRY_TAG			'etFL'
#define LFS_ERRORLOGWORKER_TAG		'weLF'

#define IDNODECALLBACK_TAG			'ciFL'

#define THREAD_WAIT_OBJECTS 3           // Builtin usable wait blocks from <ntddk.h>

#define ADD_ALIGN8(Length) ((Length+7) >> 3 << 3)


NTSTATUS
LfsDriverEntry (
    IN PDRIVER_OBJECT	DriverObject,
    IN PUNICODE_STRING	RegistryPath,
	IN PDEVICE_OBJECT	ControlDeviceObject
	);


VOID
LfsDriverUnload (
    IN PDRIVER_OBJECT	DriverObject
	);


VOID
LfsReference (
	PLFS	Lfs
	);


VOID
LfsDereference (
	PLFS	Lfs
	);


BOOLEAN
CreateFastIoDispatch(
	VOID
	);


VOID
CloseFastIoDispatch(
	VOID
	);


NTSTATUS
LfsPreAcquireForSectionSynchronization (
    IN PFS_FILTER_CALLBACK_DATA Data,
    OUT PVOID *CompletionContext
    );

VOID
LfsPostAcquireForSectionSynchronization (
    IN PFS_FILTER_CALLBACK_DATA Data,
    IN NTSTATUS OperationStatus,
    IN PVOID CompletionContext
    );


NTSTATUS
LfsPreReleaseForSectionSynchronization (
    IN PFS_FILTER_CALLBACK_DATA Data,
    OUT PVOID *CompletionContext
    );


VOID
LfsPostReleaseForSectionSynchronization (
    IN PFS_FILTER_CALLBACK_DATA Data,
    IN NTSTATUS OperationStatus,
    IN PVOID CompletionContext
    );


NTSTATUS
LfsPreAcquireForCcFlush (
    IN PFS_FILTER_CALLBACK_DATA Data,
    OUT PVOID *CompletionContext
    );


VOID
LfsPostAcquireForCcFlush (
    IN PFS_FILTER_CALLBACK_DATA Data,
    IN NTSTATUS OperationStatus,
    IN PVOID CompletionContext
    );


NTSTATUS
LfsPreReleaseForCcFlush (
    IN PFS_FILTER_CALLBACK_DATA Data,
    OUT PVOID *CompletionContext
    );


VOID
LfsPostReleaseForCcFlush (
    IN PFS_FILTER_CALLBACK_DATA Data,
    IN NTSTATUS OperationStatus,
    IN PVOID CompletionContext
    );


NTSTATUS
LfsPreAcquireForModifiedPageWriter (
    IN PFS_FILTER_CALLBACK_DATA Data,
    OUT PVOID *CompletionContext
    );


VOID
LfsPostAcquireForModifiedPageWriter (
    IN PFS_FILTER_CALLBACK_DATA Data,
    IN NTSTATUS OperationStatus,
    IN PVOID CompletionContext
    );


NTSTATUS
LfsPreReleaseForModifiedPageWriter (
    IN PFS_FILTER_CALLBACK_DATA Data,
    OUT PVOID *CompletionContext
    );


VOID
LfsPostReleaseForModifiedPageWriter (
    IN PFS_FILTER_CALLBACK_DATA Data,
    IN NTSTATUS OperationStatus,
    IN PVOID CompletionContext
    );


NTSTATUS
LfsInitializeLfsDeviceExt(
	IN struct _ENABLED_NETDISK	*EnabledNetdisk,
    IN PDEVICE_OBJECT			FileSpyDeviceObject,
	IN PDEVICE_OBJECT			DiskDeviceObject
	);


NTSTATUS
LfsAttachToFileSystemDevice (
    IN PDEVICE_OBJECT	DeviceObject,
    IN PDEVICE_OBJECT	FileSpyDeviceObject,
    IN PUNICODE_STRING	DeviceName
    );


VOID
LfsDetachFromFileSystemDevice (
    IN PDEVICE_OBJECT				DeviceObject
    );


NTSTATUS
LfsFsControlMountVolumeComplete (
    IN PDEVICE_OBJECT	FsDeviceObject,
    IN PIRP							Irp,
    IN PDEVICE_OBJECT	NewDeviceObject
    );


VOID
LfsPostAttachToMountedDevice (
    IN PDEVICE_OBJECT	DeviceObject,
    IN PDEVICE_OBJECT	FilespyDeviceObject,
	IN PIRP				Irp
    );


NTSTATUS
LfsAttachToMountedDevice (
    IN PDEVICE_OBJECT				FsDeviceObject,
    IN PDEVICE_OBJECT				VolumeDeviceObject,
    IN PDEVICE_OBJECT				NewDeviceObject
    );


VOID
LfsFastIoDetachDevice (
    IN PDEVICE_OBJECT				SourceDevice,
    IN PDEVICE_OBJECT				TargetDevice
	);


VOID
LfsCleanupMountedDevice (
    IN PDEVICE_OBJECT				DeviceObject
    );


VOID
LfsIoDeleteDevice (
    IN PDEVICE_OBJECT				DeviceObject
    );


VOID
LfsDeviceExt_Reference (
	PLFS_DEVICE_EXTENSION	LfsDeviceExt
	);


VOID
LfsDeviceExt_Dereference (
	PLFS_DEVICE_EXTENSION	LfsDeviceExt
	);
	

NTSTATUS
LfsDeviceExt_QueryNDASUsage(
	IN UINT32	SlotNo,
	OUT PUINT32	NdasUsageFlags,
	OUT PUINT32	MountedFSVolumeCount
	);


BOOLEAN
LfsDeviceExt_SecondaryToPrimary(
	IN PLFS_DEVICE_EXTENSION		LfsDeviceExt
	);

BOOLEAN
LfsPassThrough (
    IN PDEVICE_OBJECT				DeviceObject,
    IN PIRP							Irp,
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	OUT PNTSTATUS					NtStatus
	);


BOOLEAN
Lfs_IsLocalAddress(
	PLPX_ADDRESS	LpxAddress
	);


VOID
LfsDeviceExt_TryDismountThisWorker (
    IN PLFS_DEVICE_EXTENSION LfsDeviceExt
    );


VOID
PrintIrp(
	ULONG					DebugLevel,
	PCHAR					Where,
	PLFS_DEVICE_EXTENSION	LfsDeviceExt,
	PIRP					Irp
	);


#ifdef __NETDISK_MANAGER__

PNETDISK_MANAGER
NetdiskManager_Create (
	IN PLFS	Lfs
	);


VOID
NetdiskManager_Close (
	IN PNETDISK_MANAGER	NetdiskManager
	);


VOID
NetdiskManager_Reference (
	IN PNETDISK_MANAGER	NetdiskManager
	);


VOID
NetdiskManager_Dereference (
	IN PNETDISK_MANAGER			NetdiskManager
	);


BOOLEAN
NetdiskManager_IsNetdiskPartition(
	IN  PNETDISK_MANAGER	NetdiskManager,
	IN  PDEVICE_OBJECT		DiskDeviceObject,
	OUT PENABLED_NETDISK	*EnabledNetdisk
	);


NTSTATUS
NetdiskManager_MountVolume(
	IN  PNETDISK_MANAGER				NetdiskManager,
	IN  PENABLED_NETDISK				EnabledNetdisk,
	IN	LFS_FILE_SYSTEM_TYPE			FileSystemType,
	IN  PLFS_DEVICE_EXTENSION			LfsDeviceExt,
	IN  PDEVICE_OBJECT					DiskDeviceObject,
	OUT	PNETDISK_PARTITION_INFORMATION	NetdiskPartionInformation,
	OUT	PNETDISK_ENABLE_MODE			NetdiskEnableMode
	);


VOID
NetdiskManager_MountVolumeComplete(
	IN PNETDISK_MANAGER			NetdiskManager,
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt,
	IN NTSTATUS					MountStatus
	);


VOID
NetdiskManager_ChangeMode(
	IN PNETDISK_MANAGER			NetdiskManager,
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	);


VOID
NetdiskManager_SurpriseRemoval(
	IN PNETDISK_MANAGER			NetdiskManager,
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	);


VOID
NetdiskManager_DisMountVolume(
	IN PNETDISK_MANAGER			NetdiskManager,
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	);


BOOLEAN
NetdiskManager_ThisVolumeHasSecondary(
	IN PNETDISK_MANAGER			NetdiskManager,
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt,
	IN BOOLEAN					IncludeLocalSecondary
	);


BOOLEAN
NetdiskManager_Secondary2Primary(
	IN PNETDISK_MANAGER			NetdiskManager,
	IN PENABLED_NETDISK			EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	);


PNETDISK_PARTITION
MountManager_GetPrimaryPartition(
	IN PNETDISK_MANAGER	NetdiskManager,
	IN PLPX_ADDRESS		NetDiskAddress,
	IN USHORT			UnitDiskNo,
	IN PLARGE_INTEGER	StartingOffset,
	IN BOOLEAN				LocalSecondary
	);


VOID
MountManager_ReturnPrimaryPartition(
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN BOOLEAN				LocalSecondary
	);


VOID
MountManager_FileSystemShutdown(
	IN PNETDISK_MANAGER		NetdiskManager
	);


VOID
NetdiskManager_UnplugNetdisk(
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PENABLED_NETDISK		EnabledNetdisk,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	);


#endif //	__NETDISK_MANAGER__


#ifdef __PRIMARY__

PPRIMARY
Primary_Create (
	IN PLFS	Lfs
	);


VOID
Primary_Close (
	IN PPRIMARY	Primary
	);


VOID
Primary_FileSystemShutdown (
	IN PPRIMARY				Primary
	);


VOID
Primary_Reference (
	IN PPRIMARY	Primary
	);


VOID
Primary_Dereference (
	IN PPRIMARY	Primary
	);



PPRIMARY_SESSION
PrimarySession_Create(
	IN  PPRIMARY			Primary,
	IN	HANDLE				ListenFileHandle,
	IN  PFILE_OBJECT		ListenFileObject,
	IN  ULONG				ListenSocketIndex,
	IN  PLPX_ADDRESS		RemoteAddress
	);


VOID
PrimarySession_Close(
	IN 	PPRIMARY_SESSION	PrimarySession
	);


VOID
PrimarySession_FileSystemShutdown(
	IN 	PPRIMARY_SESSION	PrimarySession
	);


VOID
PrimarySession_Reference (
	IN 	PPRIMARY_SESSION	PrimarySession
	);


VOID
PrimarySession_Dereference (
	IN 	PPRIMARY_SESSION	PrimarySession
	);


NTSTATUS
Primary_FsControlMountVolumeComplete
(
	IN PPRIMARY					Primary,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	);


VOID
Primary_CleanupMountedDevice(
	IN PPRIMARY					Primary,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	);

	
BOOLEAN
Primary_PassThrough(
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt,
	IN  PIRP					Irp,
	OUT PNTSTATUS				NtStatus
	);


PLFS_DEVICE_EXTENSION
Primary_LookUpVolume(
	PPRIMARY			Primary, 
	PPRIMARY_SESSION	PrimarySession, 
	PNETDISK_PARTITION_INFO	NetdiskPartition
	);


PPRIMARY_SESSION_REQUEST
AllocPrimarySessionRequest(
	IN	BOOLEAN	Synchronous
); 


VOID
DereferencePrimarySessionRequest(
	IN	PPRIMARY_SESSION_REQUEST	PrimarySessionRequest
	);


FORCEINLINE
VOID
QueueingPrimarySessionRequest(
	IN	PPRIMARY_SESSION			PrimarySession,
	IN	PPRIMARY_SESSION_REQUEST	PrimarySessionRequest
	);



#endif // __PRIMARY__


#ifdef __SECONDARY__


PERESOURCE
LfsAllocateResource (
	VOID
    );


VOID
LfsFreeResource (
    IN PERESOURCE Resource
    );


PNON_PAGED_FCB
LfsAllocateNonPagedFcb (
	VOID
    );


VOID
LfsFreeNonPagedFcb (
    IN  PNON_PAGED_FCB NonPagedFcb
    );


PSECONDARY
Secondary_Create(
	IN	PLFS_DEVICE_EXTENSION	LfsDeviceExt			 
	);


VOID
Secondary_Close (
	IN  PSECONDARY	Secondary
	);

VOID
Secondary_Stop (
	IN  PSECONDARY	Secondary
	);

VOID
Secondary_Reference (
	IN  PSECONDARY	Secondary
	);

VOID
Secondary_Dereference (
	IN  PSECONDARY	Secondary
	);


BOOLEAN
Secondary_PassThrough(
	IN  PSECONDARY	Secondary,
	IN  PIRP		Irp,
	OUT PNTSTATUS	NtStatus
	);


BOOLEAN
Secondary_RedirectFileIo(
	IN  PSECONDARY		Secondary,
	IN  PLFS_FILE_IO	LfsFileIo
	);

VOID
Secondary_TryCloseFilExts(
	PSECONDARY Secondary
	);

VOID
SecondaryThreadProc(
	IN	PSECONDARY	Secondary
	);


VOID
DereferenceSecondaryRequest(
	IN  PSECONDARY_REQUEST	SecondaryRequest
	);


PSECONDARY_REQUEST
AllocSecondaryRequest(
	IN	PSECONDARY	Secondary,
	IN 	UINT32	MessageSize,
	IN	BOOLEAN	Synchronous
);


FORCEINLINE
VOID
QueueingSecondaryRequest(
	IN	PSECONDARY			Secondary,
	IN	PSECONDARY_REQUEST	SecondaryRequest
	);


PFILE_EXTENTION
AllocateFileExt(
	IN	PSECONDARY		Secondary,
	IN	PFILE_OBJECT	FileObject,
	IN  ULONG			BufferLength
	) ;
	

VOID
FreeFileExt(
	IN	PSECONDARY			Secondary,
	IN  PFILE_EXTENTION	FileExt
	);


PLFS_FCB
AllocateFcb (
	IN	PSECONDARY		Secondary,
	IN PUNICODE_STRING	FullFileName,
	IN  ULONG			BufferLength
    );


PLFS_FCB
Secondary_LookUpFcb(
	IN PSECONDARY		Secondary,
	IN PUNICODE_STRING	FullFileName,
    IN BOOLEAN			CaseInSensitive
	);


VOID
Secondary_DereferenceFcb (
	IN	PLFS_FCB		Fcb
   );


PFILE_EXTENTION
Secondary_LookUpFileExtension(
	IN PSECONDARY	Secondary,
	IN PFILE_OBJECT	FileObject
	);


PFILE_EXTENTION
Secondary_LookUpFileExtensionByHandle(
	IN PSECONDARY	Secondary,
	IN HANDLE		FileHandle
	);


NTSTATUS
Secondary_MakeFullFileName(
	PFILE_OBJECT	FileObject, 
	PUNICODE_STRING	FullFileName,
	BOOLEAN			fileDirectoryFile
	);


FORCEINLINE
PSECONDARY_REQUEST
ALLOC_WINXP_SECONDARY_REQUEST(
	IN PSECONDARY	Secondary,
	IN _U8			IrpMajorFunction,
	IN UINT32		DataSize
	);


FORCEINLINE
VOID
INITIALIZE_NDFS_REQUEST_HEADER(
	PNDFS_REQUEST_HEADER	NdfsRequestHeader,
	_U8						Command,
	PSECONDARY				Secondary,
	_U8						IrpMajorFunction,
	_U32					DataSize
	)
{
	KIRQL	oldIrql;

	KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);

	RtlCopyMemory(NdfsRequestHeader->Protocol, NDFS_PROTOCOL, sizeof(NdfsRequestHeader->Protocol));
	NdfsRequestHeader->Command	= Command;
	NdfsRequestHeader->Flags	= Secondary->Thread.SessionContext.Flags;
	NdfsRequestHeader->Uid		= Secondary->Thread.SessionContext.Uid;
	NdfsRequestHeader->Tid		= Secondary->Thread.SessionContext.Tid;
	NdfsRequestHeader->Mid		= 0;
	NdfsRequestHeader->MessageSize
		= sizeof(NDFS_REQUEST_HEADER)
		+ (
		(Secondary->Thread.SessionContext.MessageSecurity == 0)
		? sizeof(NDFS_WINXP_REQUEST_HEADER) + DataSize
		: (
		((IrpMajorFunction == IRP_MJ_WRITE
		&& Secondary->Thread.SessionContext.RwDataSecurity == 0
		&& DataSize <= Secondary->Thread.SessionContext.PrimaryMaxDataSize)
		||
		IrpMajorFunction == IRP_MJ_READ
		&& Secondary->Thread.SessionContext.RwDataSecurity == 0
		&& DataSize <= Secondary->Thread.SessionContext.SecondaryMaxDataSize)

		? ADD_ALIGN8(sizeof(NDFS_WINXP_REQUEST_HEADER) + DataSize)
		: ADD_ALIGN8(sizeof(NDFS_WINXP_REQUEST_HEADER) + DataSize)
		)
		);

	KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);

	return;
}																										


NTSTATUS
RedirectIrp(
	IN  PSECONDARY	Secondary,
	IN  PIRP		Irp,
	OUT	PBOOLEAN	FastMutexSet,
	OUT	PBOOLEAN	Retry
	);


#endif // __SECONDARY__

#ifdef __READONLY__

BOOLEAN
ReadOnlyPassThrough(
    IN PDEVICE_OBJECT				DeviceObject,
    IN PIRP							Irp,
    IN PIO_STACK_LOCATION			IrpSp,
	IN PLFS_DEVICE_EXTENSION		LfsDevExt,
	OUT	PNTSTATUS					NtStatus
   );

#endif

PLFS_TABLE
LfsTable_Create(
	IN PLFS	Lfs
	);


VOID
LfsTable_Close (
	IN PLFS_TABLE	LfsTable

	);


VOID
LfsTable_Reference (
	IN PLFS_TABLE	LfsTable
	);


VOID
LfsTable_Dereference (
	IN PLFS_TABLE	LfsTable
	);


VOID
LfsTable_InsertNetDiskPartitionInfoUser(
	IN PLFS_TABLE				LfsTable,
	IN PNETDISK_PARTITION_INFO	NetDiskPartitionInfo,
	IN PLPX_ADDRESS				BindAddress,
	IN BOOLEAN					Primary
	);


VOID
LfsTable_DeleteNetDiskPartitionInfoUser(
	IN PLFS_TABLE				LfsTable,
	IN PNETDISK_PARTITION_INFO	NetDiskPartitionInfo,
	IN BOOLEAN					Primary
	);

NTSTATUS
LfsTable_QueryPrimaryAddress(
	IN PLFS_TABLE				LfsTable,
	IN  PNETDISK_PARTITION_INFO	NetDiskPartitionInfo,
	OUT PLPX_ADDRESS			PrimaryAddress
	);


VOID
LfsTable_CleanCachePrimaryAddress(
	IN PLFS_TABLE				LfsTable,
	IN  PNETDISK_PARTITION_INFO	NetDiskPartitionInfo
	);

BOOLEAN
IsNetDisk(
	IN	PDEVICE_OBJECT			DiskDeviceObject,
	OUT PNETDISK_INFORMATION	NetdiskInformation
	);


NTSTATUS
LfsFilterDeviceIoControl(
    IN PDEVICE_OBJECT	DeviceObject,
    IN ULONG			IoCtl,
    IN PVOID			InputBuffer OPTIONAL,
    IN ULONG			InputBufferLength,
    IN PVOID			OutputBuffer OPTIONAL,
    IN ULONG			OutputBufferLength,
    OUT PULONG_PTR		IosbInformation OPTIONAL
    );


BOOLEAN	GetScsiportAdapter(
  	IN	PDEVICE_OBJECT	DiskDeviceObject,
  	IN	PDEVICE_OBJECT	*ScsiportAdapterDeviceObject
	);

/*
BOOLEAN	
GetHarddisk(
  	IN	PDEVICE_OBJECT	DiskDeviceObject,
  	IN	PDEVICE_OBJECT	*HarddiskDeviceObject
	);
*/
//
// lfslib.c
//
NTSTATUS
LfsStopSecVolume(
	UINT32	PhysicalDriveNumber,
	UINT32	Hint
);

__inline
VOID
InitTransCtx(PLFS_TRANS_CTX TransCtx, LONG	MaxSendBytes) {

	TransCtx->MaxSendBytes = MaxSendBytes;
	if(TransCtx->MaxSendBytes > LL_MAX_TRANSMIT_DATA) {
		TransCtx->MaxSendBytes = LL_MAX_TRANSMIT_DATA;
	}

	TransCtx->DynMaxSendBytes = TransCtx->MaxSendBytes / 2;
	if(TransCtx->DynMaxSendBytes < LL_MIN_TRANSMIT_DATA) {
		TransCtx->DynMaxSendBytes = LL_MIN_TRANSMIT_DATA;
	}

	TransCtx->AccSendTransStat.PacketLoss = 0;
	TransCtx->StableSendCnt = 0;
}

NTSTATUS
SendMessage(
	IN PFILE_OBJECT		ConnectionFileObject,
	IN _U8				*Buf, 
	IN LONG				Size,
	IN PLARGE_INTEGER	TimeOut,
	IN PLFS_TRANS_CTX	TransCtx
	);


NTSTATUS
RecvMessage(
	IN PFILE_OBJECT		ConnectionFileObject,
	OUT _U8				*Buf, 
	IN LONG				Size,
	IN PLARGE_INTEGER	TimeOut
	);


struct Create
{
	PIO_SECURITY_CONTEXT		SecurityContext;
    ULONG						Options;
    USHORT POINTER_ALIGNMENT	FileAttributes;
    USHORT						ShareAccess;
    ULONG  POINTER_ALIGNMENT	EaLength;
};


struct DeviceIoControl
{
	ULONG					OutputBufferLength;
    ULONG POINTER_ALIGNMENT InputBufferLength;
    ULONG POINTER_ALIGNMENT IoControlCode;
    PVOID					Type3InputBuffer;
};


struct FileSystemControl
{
	ULONG					OutputBufferLength;
    ULONG POINTER_ALIGNMENT InputBufferLength;
    ULONG POINTER_ALIGNMENT FsControlCode;
    PVOID					Type3InputBuffer;
};


struct QueryDirectory
{
	ULONG					Length;
    PSTRING					FileName;
	FILE_INFORMATION_CLASS	FileInformationClass;
	ULONG POINTER_ALIGNMENT FileIndex;
};

 
struct QueryFile
{
	ULONG										Length;
    FILE_INFORMATION_CLASS POINTER_ALIGNMENT	FileInformationClass;
};

struct SetFile
{
	ULONG										Length;
    FILE_INFORMATION_CLASS POINTER_ALIGNMENT	FileInformationClass;
    PFILE_OBJECT								FileObject;
    union 
	{
		struct 
		{
			BOOLEAN ReplaceIfExists;
            BOOLEAN AdvanceOnly;
		};
        ULONG  ClusterCount;
        HANDLE DeleteHandle;
	};
};


struct QueryVolume
{
	ULONG									Length;
	FS_INFORMATION_CLASS POINTER_ALIGNMENT	FsInformationClass;
};

        //
        // System service parameters for:  NtSetVolumeInformationFile
        //

struct SetVolume
{
	ULONG Length;
	FS_INFORMATION_CLASS POINTER_ALIGNMENT FsInformationClass;
};

struct Read
{
	ULONG					Length;
	ULONG POINTER_ALIGNMENT Key;
    LARGE_INTEGER			ByteOffset;
};


struct Write
{
	ULONG					Length;
	ULONG POINTER_ALIGNMENT Key;
	LARGE_INTEGER			ByteOffset;
};


struct LockControl 
{
	PLARGE_INTEGER			Length;
    ULONG POINTER_ALIGNMENT Key;
    LARGE_INTEGER			ByteOffset;
};


struct QuerySecurity 
{
	SECURITY_INFORMATION SecurityInformation;
	ULONG POINTER_ALIGNMENT Length;
} ;


struct SetSecurity 
{
	SECURITY_INFORMATION SecurityInformation;
	PSECURITY_DESCRIPTOR SecurityDescriptor;
} ;


struct QueryEa {
	ULONG Length;
	PVOID EaList;
	ULONG EaListLength;
	ULONG POINTER_ALIGNMENT EaIndex;
};

struct SetEa {
	ULONG Length;
};


struct QueryQuota {
	ULONG Length;
	PSID StartSid;
	PFILE_GET_QUOTA_INFORMATION SidList;
	ULONG SidListLength;
};


struct SetQuota {
	ULONG Length;
};


PVOID
MapInputBuffer(
	PIRP Irp
 );


PVOID
MapOutputBuffer(
	PIRP Irp
 );


BOOLEAN
RdsvrDatagramInit(
		PLFS_TABLE			LfsTable,
		PVOID				*DGSvrCtx,
		PVOID				*NtcCtx
	) ;

BOOLEAN
RdsvrDatagramDestroy(VOID);


VOID
PrintFileInfoClass(
		ULONG			infoType,
		ULONG			systemBuffLength,
		PCHAR			systemBuff
) ;


VOID
PrintTime (
	IN ULONG	DebugLevel,
    IN PTIME	Time
    );


NTSTATUS
NDCtrlUpgradeToWrite(
	ULONG				SlotNo,
	PIO_STATUS_BLOCK	IoStatus
) ;


NTSTATUS
NDCtrlUnplug(
	ULONG				SlotNo
);

NTSTATUS
NDCtrlEject(
	ULONG				SlotNo
);
 
VOID
Primary_NetEvtCallback(
		PSOCKETLPX_ADDRESS_LIST	Original,
		PSOCKETLPX_ADDRESS_LIST	Updated,
		PSOCKETLPX_ADDRESS_LIST	Disabled,
		PSOCKETLPX_ADDRESS_LIST	Enabled,
		PVOID					Context
	) ;


VOID
DgSvr_NetEvtCallback(
		PSOCKETLPX_ADDRESS_LIST	Original,
		PSOCKETLPX_ADDRESS_LIST	Updated,
		PSOCKETLPX_ADDRESS_LIST	Disabled,
		PSOCKETLPX_ADDRESS_LIST	Enabled,
		PVOID					Context
	) ;

VOID
DgNtc_NetEvtCallback(
		PSOCKETLPX_ADDRESS_LIST	Original,
		PSOCKETLPX_ADDRESS_LIST	Updated,
		PSOCKETLPX_ADDRESS_LIST	Disabled,
		PSOCKETLPX_ADDRESS_LIST	Enabled,
		PVOID					Context
	) ;


//LfsDbg.c

VOID
GetIrpName (
    IN UCHAR MajorCode,
    IN UCHAR MinorCode,
    IN ULONG FsctlCode,
    OUT PCHAR MajorCodeName,
    OUT PCHAR MinorCodeName
    );


//
//	thunk.c
//

#ifdef _WIN64

#if DBG

VOID
SplayTest();

VOID
SplayTest2();

#endif

NTSTATUS
Th32Init(
	PTHUNKER32	Thunker32
);

VOID
Th32Destroy(
	PTHUNKER32	Thunker32
);

NTSTATUS
Th32RegisterPointer(
	IN PTHUNKER32	Thunker32,
	IN PVOID		ThunkPointer,
	OUT _U32		*PointerId
);

VOID
Th32UnregisterPointer(
	PTHUNKER32	Thunker32,
	_U32		PointerId
);

#else

#define Th32Init(THUNKER32)		__noop
#define Th32Destroy(THUNKER32)	__noop
#define Th32UnregisterPointer(THUNKER32, POINTERID)	__noop
#if DBG
#define SplayTest()				__noop
#define SplayTest2()			__noop
#endif

NTSTATUS
__inline
Th32RegisterPointer(
	IN PTHUNKER32	Thunker32,
	IN PVOID		ThunkPointer,
	OUT _U32		*PointerId
) {
	UNREFERENCED_PARAMETER(Thunker32);

	*PointerId = (_U32)PtrToUlong(ThunkPointer);

	return STATUS_SUCCESS;
}

#endif


#endif

