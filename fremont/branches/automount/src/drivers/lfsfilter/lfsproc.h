#ifndef __LFS_PROC_H__
#define __LFS_PROC_H__


#define __NDAS_FS__										1
#define __NDAS_FS_ALLOW_UNLOAD_DRIVER__					1
#define __NDAS_FS_ENUMERATION__							0

#ifdef _NDAS_FS_MINI_
#define __NDAS_FS_MINI__								1
#endif

#ifdef _NDAS_FS_MINI_CHECK_				
#define __NDAS_FS_MINI_CHECK__							0
#endif

#define __NDAS_FS_NTFS_RW__								0
#define __NDAS_FS_NTFS_RW_INDIRECT__					0
#define __NDAS_FS_NTFS_RO__								0

#define __NDAS_FS_FAT_RW__								1
#define __NDAS_FS_FAT_RW_INDIRECT__						1
#define __NDAS_FS_FAT_RO__								1

#define __NDAS_FS_FLUSH_VOLUME__						0

#define	__NDAS_FS_TEST_MODE__							0
#define	__NDAS_FS_HCT_TEST_MODE__						0
#define	__NDAS_FS_DBG_MEMORY__							0
#define	__NDAS_FS_PROCESS_SECONDARY_REQUEST_LOCALLY__	0

#define __NDAS_FS_PRIMARY_DISMOUNT__					1

#ifndef NTDDI_VERSION

#define __NDAS_FS_COMPILE_FOR_VISTA__					1

#endif

#if (__NDAS_FS__ && !defined NTDDI_VERSION)
#define __analysis_assume(a)
#endif

#if __NDAS_FS_MINI__
#include <fltKernel.h>
#else
#include <ntifs.h>
#endif

#if __NDAS_FS_COMPILE_FOR_VISTA__
#include "lh_supp.h"
#endif

#include <stdlib.h>
#include <initguid.h>
#include <ntdddisk.h>
#include <ntddvol.h>

#include "filespylib.h"

#include "namelookup.h"

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

#ifndef NTDDI_VERSION

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

#endif // NTDDI_VERSION

NTKERNELAPI
NTSTATUS
IoReportTargetDeviceChange(
    IN PDEVICE_OBJECT PhysicalDeviceObject,
    IN PVOID NotificationStructure  // always begins with a PLUGPLAY_NOTIFICATION_HEADER
    );


typedef struct _FILESPY_DEVICE_EXTENSION *PFILESPY_DEVICE_EXTENSION;
#define DEVICE_NAMES_SZ  100

#include <ndasfs.h>
#include <SocketLpx.h>
#include <lpxtdi.h>
#include <ndasbusioctl.h>
#include <lfsfilterpublic.h>


#include "NdfsProtocolHeader2.h"
#include "NdftProtocolHeader.h"
#include "NdfsInteface.h"

#include "md5.h"

#if __NDAS_FS_MINI__
#include "minispy.h"
#endif

#include "inc\lfs.h"
#include "inc\ntfs.h"

#include "LfsDbg.h"

#include "FastIoDispatch.h"
#include "Lfs.h"

#if __NDAS_FS_MINI__
#include "pch.h"
#endif

#include "NetdiskManager.h"

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


#include "Primary.h"
#include "Secondary.h"
#include "Readonly.h"

#ifndef _WIN64

#include "W2kNtfsLib.h"
#include "W2kFatLib.h"
#include "WxpNtfsLib.h"
#include "WxpFatLib.h"
#endif
#include "WnetFatLib.h"

#include "LfsTable.h"

#include "filespy.h"
#include "fspyKern.h"

#if __NDAS_FS_MINI__
#include "mspyKern.h"
#endif

// When you include module.ver file, you should include <ndasverp.h> also
// in case module.ver does not include any definitions
#include "lfsfilt.ver"
#include <ndasverp.h>

#ifdef  _WIN64
#include <ntintsafe.h>
#endif

#include "NdftProtocolHeader.h"
#include "LfsDGSvrCli.h"


#include <srb.h>
#include <initguid.h>
#include <stdio.h>

#include <LanScsi.h>
#include "ndasscsiioctl.h"

#include <ntddscsi.h>

#define LFS_TIME_OUT					(HZ * 360000)  // This timeout should not happen. Because of possible delay in ConnectToPrimary, set to at least over 120 sec
#define LFS_SECONDARY_THREAD_FLAG_TIME_OUT	(10 * HZ)
#define LFS_CHANGECONNECTION_TIMEOUT	(HZ * 60000) // ((RECONNECTION_MAX_TRY * MAX_RECONNECTION_INTERVAL) * 2)
#define LFS_DEADLOCK_AVOID_TIMEOUT		(HZ * 180) // ((RECONNECTION_MAX_TRY * MAX_RECONNECTION_INTERVAL) * 2)
#define LFS_MARK						0xF23AC135 // It's Real Random Number
#define LFS_QUERY_REMOVE_TIME_OUT		(HZ * 120) 

#define LFS_READONLY_THREAD_FLAG_TIME_OUT	(30 * HZ)

#define LFS_INSUFFICIENT_RESOURCES	0
#define LFS_BUG						0
#define LFS_LPX_BUG					0
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

#define LFSLIB_SRBIOCTL				'rsFL'

#define LFSTAB_ENTRY_TAG			'etFL'
#define LFS_ERRORLOGWORKER_TAG		'weLF'

#define IDNODECALLBACK_TAG			'ciFL'

#define READONLY_MESSAGE_TAG		'RsFL'


#define THREAD_WAIT_OBJECTS 3           // Builtin usable wait blocks from <ntddk.h>

#define ADD_ALIGN8(Length) ((Length+7) >> 3 << 3)

#if WINVER >= 0x0501
#define LfsDbgBreakPoint()	(KD_DEBUGGER_ENABLED ? DbgBreakPoint() : TRUE)
#else
#define LfsDbgBreakPoint()	((*KdDebuggerEnabled) ? DbgBreakPoint() : TRUE) 
#endif

#define NDASFS_ASSERT( exp ) \
	((!(exp)) ?				 \
	LfsDbgBreakPoint() :	 \
	TRUE)

// ctxinit.c

#if __NDAS_FS_MINI__

NTSTATUS
CtxDriverEntry (
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

VOID
CtxContextCleanup (
    IN PFLT_CONTEXT Context,
    IN FLT_CONTEXT_TYPE ContextType
    );

NTSTATUS
CtxInstanceSetup (
    IN PCFLT_RELATED_OBJECTS FltObjects,
    IN FLT_INSTANCE_SETUP_FLAGS Flags,
    IN DEVICE_TYPE VolumeDeviceType,
    IN FLT_FILESYSTEM_TYPE VolumeFilesystemType
    );

NTSTATUS
CtxInstanceQueryTeardown (
    IN PCFLT_RELATED_OBJECTS FltObjects,
    IN FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    );

VOID
CtxInstanceTeardownStart (
    IN PCFLT_RELATED_OBJECTS FltObjects,
    IN FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

VOID
CtxInstanceTeardownComplete (
    IN PCFLT_RELATED_OBJECTS FltObjects,
    IN FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

#endif


#if __NDAS_FS_MINI__

//filespy.c

NTSTATUS
FileSpyDriverEntry (
    __in PDRIVER_OBJECT  DriverObject,
    __in PUNICODE_STRING RegistryPath
	);

VOID
FileSpyDriverUnload (
    __in PDRIVER_OBJECT DriverObject
    );

NTSTATUS
SpyFsControlSecondaryMountVolumeComplete (
	IN PLFS_DEVICE_EXTENSION	NewLfsDeviceExt
    );

NTSTATUS
SpyFsControlReadonlyMountVolumeComplete (
	IN PLFS_DEVICE_EXTENSION	NewLfsDeviceExt
	);

//minindasfs.c

NTSTATUS
NdasFsGeneralPreOperationCallback (
	IN PCFLT_RELATED_OBJECTS	FltObjects,
	IN PCTX_INSTANCE_CONTEXT	InstanceContext,
	IN OUT PFLT_CALLBACK_DATA	Data
	);

NTSTATUS
NdasFsGeneralPostOperationCallback (
	IN PCFLT_RELATED_OBJECTS	FltObjects,
	IN PCTX_INSTANCE_CONTEXT	InstanceContext,
	IN OUT PFLT_CALLBACK_DATA	Data
	);

NTSTATUS
NdasFsSecondaryPreOperationCallback (
	IN PCFLT_RELATED_OBJECTS	FltObjects,
	IN PCTX_INSTANCE_CONTEXT	InstanceContext,
	IN OUT PFLT_CALLBACK_DATA	Data
	);

NTSTATUS
NdasFsSecondaryPostOperationCallback (
	IN PCFLT_RELATED_OBJECTS	FltObjects,
	IN PCTX_INSTANCE_CONTEXT	InstanceContext,
	IN OUT PFLT_CALLBACK_DATA	Data
	);

#endif

// Lfs.c

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

NTSTATUS
LfsInitializeLfsDeviceExt (
    IN PDEVICE_OBJECT	FileSpyDeviceObject,
    IN PDEVICE_OBJECT	DiskDeviceObject,
	IN PDEVICE_OBJECT	MountVolumeDeviceObject
	);

VOID
LfsIoDeleteDevice (
    IN PDEVICE_OBJECT DeviceObject
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
LfsDeviceExt_QueryNDASUsage (
	IN  UINT32	SlotNo,
	OUT PUINT32	NdasUsageFlags,
	OUT PUINT32	MountedFSVolumeCount
	);

NTSTATUS
LfsAttachToFileSystemDevice (
    IN PDEVICE_OBJECT	DeviceObject,
    IN PDEVICE_OBJECT	FileSpyDeviceObject,
    IN PUNICODE_STRING	DeviceName
    );

VOID
LfsDetachFromFileSystemDevice (
    IN PDEVICE_OBJECT FileSpyDeviceObject
    );

VOID
LfsCleanupMountedDevice (
    IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
    );

VOID
LfsDismountVolume (
    IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
    );

BOOLEAN
LfsPassThrough (
    IN  PDEVICE_OBJECT				DeviceObject,
    IN  PIRP						Irp,
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	OUT PNTSTATUS					NtStatus
	);

BOOLEAN
LfsSecondaryPassThrough (
    IN  PDEVICE_OBJECT				DeviceObject,
    IN  PIRP						Irp,
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	OUT PNTSTATUS					NtStatus
	);

PNON_PAGED_FCB
LfsAllocateNonPagedFcb (
	VOID
    );

VOID
LfsFreeNonPagedFcb (
    IN  PNON_PAGED_FCB NonPagedFcb
    );

PERESOURCE
LfsAllocateResource (
	VOID
    );

VOID
LfsFreeResource (
    IN PERESOURCE Resource
    );

BOOLEAN
LfsDeviceExt_SecondaryToPrimary (
	IN PLFS_DEVICE_EXTENSION LfsDeviceExt
	);

NTSTATUS
LfsSecondaryToPrimary (
	IN PLFS_DEVICE_EXTENSION LfsDeviceExt
	);

BOOLEAN
Lfs_IsLocalAddress (
	PLPX_ADDRESS	LpxAddress
	);

NTSTATUS
RegisterNdfsCallback (
	PDEVICE_OBJECT	DeviceObject
	);

NTSTATUS
UnRegisterNdfsCallback(
	PDEVICE_OBJECT	DeviceObject
	);

NTSTATUS
CallbackQueryPartitionInformation (
	IN  PDEVICE_OBJECT					RealDevice,
	OUT PNETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation
	);

NTSTATUS
CallbackQueryPrimaryAddress ( 
	IN  PNETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation,
	OUT PLPX_ADDRESS					PrimaryAddress,
	IN  PBOOLEAN						IsLocalAddress	
	);

NTSTATUS
CallbackSecondaryToPrimary (
	IN PDEVICE_OBJECT	DiskDeviceObject,
	IN BOOLEAN			ModeChange
	);

NTSTATUS
CallbackAddWriteRange (
	IN	PDEVICE_OBJECT	DiskDeviceObject,
	OUT PBLOCKACE_ID	BlockAceId
	);

VOID
CallbackRemoveWriteRange (
	IN	PDEVICE_OBJECT	DiskDeviceObject,
	OUT BLOCKACE_ID		BlockAceId
	);

NTSTATUS
CallbackGetNdasScsiBacl (
	IN	PDEVICE_OBJECT	DiskDeviceObject,
	OUT PNDAS_BLOCK_ACL	NdasBacl,
	IN BOOLEAN			SystemOrUser
	); 


// fastiodispatch.c

NTSTATUS
LfsPreAcquireForSectionSynchronization (
    IN PFS_FILTER_CALLBACK_DATA	Data,
    OUT PVOID					*CompletionContext
    );

VOID
LfsPostAcquireForSectionSynchronization (
    IN PFS_FILTER_CALLBACK_DATA	Data,
    IN NTSTATUS					OperationStatus,
    IN PVOID					CompletionContext
    );

NTSTATUS
LfsPreReleaseForSectionSynchronization (
    IN PFS_FILTER_CALLBACK_DATA	Data,
    OUT PVOID					*CompletionContext
    );

VOID
LfsPostReleaseForSectionSynchronization (
    IN PFS_FILTER_CALLBACK_DATA	Data,
    IN NTSTATUS					OperationStatus,
    IN PVOID					CompletionContext
    );

NTSTATUS
LfsPreAcquireForCcFlush (
    IN PFS_FILTER_CALLBACK_DATA	Data,
    OUT PVOID					*CompletionContext
    );

VOID
LfsPostAcquireForCcFlush (
    IN PFS_FILTER_CALLBACK_DATA Data,
    IN NTSTATUS OperationStatus,
    IN PVOID CompletionContext
    );

NTSTATUS
LfsPreReleaseForCcFlush (
    IN PFS_FILTER_CALLBACK_DATA	Data,
    OUT PVOID				    *CompletionContext
    );

VOID
LfsPostReleaseForCcFlush (
    IN PFS_FILTER_CALLBACK_DATA	Data,
    IN NTSTATUS					OperationStatus,
    IN PVOID					CompletionContext
    );

NTSTATUS
LfsPreAcquireForModifiedPageWriter (
    IN PFS_FILTER_CALLBACK_DATA	Data,
    OUT PVOID					*CompletionContext
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
    IN PFS_FILTER_CALLBACK_DATA	Data,
    IN NTSTATUS					OperationStatus,
    IN PVOID					CompletionContext
    );

// lfsdbg.c

#if DBG

VOID
PrintIrp(
	ULONG					DebugLevel,
	PCHAR					Where,
	PLFS_DEVICE_EXTENSION	LfsDeviceExt,
	PIRP					Irp
	);

#else

#define PrintIrp( DebugLevel, Where, LfsDeviceExt, Irp )

#endif

// netdiskmanager.c

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
NetdiskManager_IsNetdiskPartition (
	IN  PNETDISK_MANAGER	NetdiskManager,
	IN  PDEVICE_OBJECT		DiskDeviceObject,
	OUT PENABLED_NETDISK	*EnabledNetdisk
	);


NTSTATUS
NetdiskManager_MountVolume (
	IN  PNETDISK_MANAGER				NetdiskManager,
	IN  PENABLED_NETDISK				EnabledNetdisk,
	IN	LFS_FILE_SYSTEM_TYPE			FileSystemType,
	IN  PLFS_DEVICE_EXTENSION			LfsDeviceExt,
	IN  PDEVICE_OBJECT					DiskDeviceObject,
	OUT	PNETDISK_PARTITION_INFORMATION	NetdiskPartionInformation,
	OUT	PNETDISK_ENABLE_MODE			NetdiskEnableMode
	);


VOID
NetdiskManager_MountVolumeComplete (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode,
	IN NTSTATUS				MountStatus,
	IN PDEVICE_OBJECT		AttachedToDeviceObject
	);

VOID
NetdiskManager_CreateFile (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition
	);

VOID
NetdiskManager_ChangeMode (
	IN		PNETDISK_MANAGER		NetdiskManager,
	IN		PNETDISK_PARTITION		NetdiskPartition,
	IN OUT	PNETDISK_ENABLE_MODE	NetdiskEnableMode
	);


VOID
NetdiskManager_SurpriseRemoval (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	);


VOID
NetdiskManager_DisMountVolume (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	);


BOOLEAN
NetdiskManager_ThisVolumeHasSecondary (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode,
	IN BOOLEAN				IncludeLocalSecondary
	);


NTSTATUS
NetdiskManager_Secondary2Primary (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	);


NTSTATUS
NetdiskManager_AddWriteRange (
	IN	PNETDISK_MANAGER	NetdiskManager,
	IN  PNETDISK_PARTITION	NetdiskPartition,
	OUT PBLOCKACE_ID		BlockAceId
	);


VOID
NetdiskManager_RemoveWriteRange (
	IN	PNETDISK_MANAGER	NetdiskManager,
	IN  PNETDISK_PARTITION	NetdiskPartition,
	OUT BLOCKACE_ID			BlockAceId
	);


NTSTATUS
NetdiskManager_GetPrimaryPartition (
	IN  PNETDISK_MANAGER				NetdiskManager,
	IN  PPRIMARY_SESSION				PrimarySession,
	IN  PLPX_ADDRESS					NetDiskAddress,
	IN  USHORT							UnitDiskNo,
	IN PUCHAR							NdscId,
	IN  PLARGE_INTEGER					StartingOffset,
	IN  BOOLEAN							LocalSecondary,
	OUT PNETDISK_PARTITION				*NetdiskPartition,
	OUT PNETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation,
	OUT PLFS_FILE_SYSTEM_TYPE			FileSystemType
	);


VOID
NetdiskManager_ReturnPrimaryPartition (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PPRIMARY_SESSION		PrimarySession,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN BOOLEAN				LocalSecondary
	);

NTSTATUS
NetdiskManager_TakeOver (
	IN PNETDISK_MANAGER		NetdiskManager, 
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN PSESSION_INFORMATION	SessionInformation
	);

VOID
NetdiskManager_FileSystemShutdown (
	IN PNETDISK_MANAGER		NetdiskManager
	);


NTSTATUS
NetdiskManager_UnplugNetdisk (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	);

BOOLEAN
NetdiskManager_IsStoppedNetdisk (
	IN PNETDISK_MANAGER	NetdiskManager,
	IN PENABLED_NETDISK	EnabledNetdisk
	);


NTSTATUS
NetdiskManager_QueryPartitionInformation (
	IN  PNETDISK_MANAGER				NetdiskManager,
	IN  PDEVICE_OBJECT					RealDevice,
	OUT PNETDISK_PARTITION_INFORMATION	NetdiskPartitionInformation
	);

PNETDISK_PARTITION
NetdiskManager_QueryNetdiskPartition (
	IN  PNETDISK_MANAGER		NetdiskManager,
	IN  PDEVICE_OBJECT			DiskDeviceObject
	);

NTSTATUS
NetdiskManager_PreMountVolume (
	IN  PNETDISK_MANAGER		NetdiskManager,
	IN  BOOLEAN					Indirect,
	IN  PDEVICE_OBJECT			VolumeDeviceObject,
	IN  PDEVICE_OBJECT			DiskDeviceObject,
	OUT PNETDISK_PARTITION		*NetdiskPartition,
	OUT PNETDISK_ENABLE_MODE	NetdiskEnableMode
	);

NTSTATUS
NetdiskManager_PostMountVolume (
	IN  PNETDISK_MANAGER				NetdiskManager,
	IN  PNETDISK_PARTITION				NetdiskPartition,
	IN  NETDISK_ENABLE_MODE				NetdiskEnableMode,
	IN  BOOLEAN							Mount,
	IN	LFS_FILE_SYSTEM_TYPE			FileSystemType,
	IN  PLFS_DEVICE_EXTENSION			LfsDeviceExt,
	OUT	PNETDISK_PARTITION_INFORMATION	NetdiskPartionInformation
	);


VOID
NetdiskManager_PrimarySessionStopping (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	);


VOID
NetdiskManager_PrimarySessionCancelStopping (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	);

VOID
NetdiskManager_PrimarySessionDisconnect (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	);

VOID
NetdiskPartition_Dereference (
	PNETDISK_PARTITION	NetdiskPartition
	);


// primary.c

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
	IN PPRIMARY	Primary
	);

VOID
Primary_Reference (
	IN PPRIMARY	Primary
	);

VOID
Primary_Dereference (
	IN PPRIMARY	Primary
	);

BOOLEAN
LfsGeneralPassThrough (
    IN  PDEVICE_OBJECT				DeviceObject,
    IN  PIRP						Irp,
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	OUT PNTSTATUS					NtStatus
	);


//PrimarySession.c

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
PrimarySession_Stopping(
	IN 	PPRIMARY_SESSION	PrimarySession
	);

VOID
PrimarySession_Disconnect(
	IN 	PPRIMARY_SESSION	PrimarySession
	);

VOID
PrimarySession_CancelStopping(
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
Primary_FsControlMountVolumeComplete (
	IN PPRIMARY					Primary,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	);


VOID
Primary_CleanupMountedDevice(
	IN PPRIMARY					Primary,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
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
NTSTATUS
QueueingPrimarySessionRequest(
	IN	PPRIMARY_SESSION			PrimarySession,
	IN	PPRIMARY_SESSION_REQUEST	PrimarySessionRequest,
	IN  BOOLEAN						FastMutexAcquired
	);


// primarySessiondispatchwinxprequest.c

NTSTATUS
DispatchWinXpRequest (
	IN  PPRIMARY_SESSION			PrimarySession,
	IN  PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader,
	IN  PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplytHeader,
	IN  _U32						DataSize,
	OUT	_U32						*replyDataSize
	);


// secondary.c



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
	//KIRQL	oldIrql;

	//KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
	ExAcquireFastMutex( &Secondary->FastMutex );

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

	//KeReleaseSpinLock(&Secondary->SpinLock, oldIrql);
	ExReleaseFastMutex( &Secondary->FastMutex );

	return;
}																										

PSECONDARY
Secondary_Create (
	IN	PLFS_DEVICE_EXTENSION	LfsDeviceExt
	);

VOID
Secondary_Close (
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

PSECONDARY_REQUEST
AllocSecondaryRequest (
	IN	PSECONDARY	Secondary,
	IN 	UINT32		MessageSize,
	IN	BOOLEAN		Synchronous
	); 

VOID
ReferenceSecondaryRequest (
	IN	PSECONDARY_REQUEST	SecondaryRequest
	); 

VOID
DereferenceSecondaryRequest (
	IN  PSECONDARY_REQUEST	SecondaryRequest
	);

FORCEINLINE
NTSTATUS
QueueingSecondaryRequest (
	IN	PSECONDARY			Secondary,
	IN	PSECONDARY_REQUEST	SecondaryRequest
	);

VOID
Secondary_TryCloseFilExts (
	PSECONDARY Secondary
	);

BOOLEAN 
Secondary_PassThrough (
	IN  PSECONDARY	Secondary,
	IN  PIRP		Irp,
	OUT PNTSTATUS	NtStatus
	);

BOOLEAN 
SecondaryNtfsPassThrough (
	IN  PSECONDARY	Secondary,
	IN  PIRP		Irp,
	OUT PNTSTATUS	NtStatus
	);

VOID
SecondaryFileObjectClose (
	IN PSECONDARY		Secondary,
	IN OUT PFILE_OBJECT	FileObject
	);

FORCEINLINE
PSECONDARY_REQUEST
AllocateWinxpSecondaryRequest (
	IN PSECONDARY	Secondary,
	IN UINT32		DataSize
	);

BOOLEAN
RecoverySession (
	IN  PSECONDARY	Secondary
	);

PLFS_FCB
AllocateFcb (
	IN	PSECONDARY		Secondary,
	IN PUNICODE_STRING	FullFileName,
	IN BOOLEAN			IsPagingFile,
	IN  ULONG			BufferLength
    );

VOID
Secondary_DereferenceFcb (
	IN	PLFS_FCB	Fcb
	);

PLFS_FCB
Secondary_LookUpFcb (
	IN PSECONDARY		Secondary,
	IN PUNICODE_STRING	FullFileName,
    IN BOOLEAN			CaseInSensitive
	);

PFILE_EXTENTION
AllocateFileExt (
	IN	PSECONDARY		Secondary,
	IN	PFILE_OBJECT	FileObject,
	IN  ULONG			BufferLength
	); 

VOID
FreeFileExt (
	IN  PSECONDARY		Secondary,
	IN  PFILE_EXTENTION	FileExt
	);

PFILE_EXTENTION
Secondary_LookUpFileExtensionByHandle (
	IN PSECONDARY	Secondary,
	IN HANDLE		FileHandle
	);
	
PFILE_EXTENTION
Secondary_LookUpFileExtension (
	IN PSECONDARY	Secondary,
	IN PFILE_OBJECT	FileObject
	);

// secondaryThread.c

VOID
SecondaryThreadProc (
	IN	PSECONDARY	Secondary
	);

// secondaryRedirectIrp.c

NTSTATUS
RedirectIrp (
	IN  PSECONDARY	Secondary,
	IN  PIRP		Irp,
	OUT	PBOOLEAN	FastMutexSet,
	OUT	PBOOLEAN	Retry
	);

NTSTATUS
CallRecoverySessionAsynchronously (
	IN  PSECONDARY	Secondary
	); 

NTSTATUS	
AcquireLockAndTestCorruptError (
	PSECONDARY			Secondary,
	PBOOLEAN			FastMutexSet,
	PFILE_EXTENTION		FileExt,
	PBOOLEAN			Retry,
	PSECONDARY_REQUEST	*SecondaryRequest,
	ULONG				CurrentSessionId
	);

NTSTATUS
Secondary_MakeFullFileName (
	IN PFILE_OBJECT		FileObject, 
	IN PUNICODE_STRING	FullFileName,
	IN BOOLEAN			fileDirectoryFile
	);


// readonly.c

PREADONLY
Readonly_Create (
	IN	PLFS_DEVICE_EXTENSION	LfsDeviceExt
	);

VOID
Readonly_Close (
	IN  PREADONLY	Readonly
	);

BOOLEAN
ReadonlyPassThrough (
    IN  PDEVICE_OBJECT				DeviceObject,
    IN  PIRP						Irp,
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	OUT PNTSTATUS					NtStatus
	);

NTSTATUS
ReadonlyRedirectIrp (
	IN  PFILESPY_DEVICE_EXTENSION	DevExt,
	IN  PIRP						Irp,
	OUT PBOOLEAN					Result
	);

PNDAS_FCB
ReadonlyAllocateFcb (
	IN PFILESPY_DEVICE_EXTENSION	DevExt,
	IN PUNICODE_STRING				FullFileName,
	IN BOOLEAN						IsPagingFile
	);

VOID
ReadonlyDereferenceFcb (
	IN	PNDAS_FCB	Fcb
	);

PNDAS_FCB
ReadonlyLookUpFcb (
	IN PFILESPY_DEVICE_EXTENSION	DevExt,
	IN PUNICODE_STRING				FullFileName,
    IN BOOLEAN						CaseInSensitive
	);

PNDAS_CCB
ReadonlyAllocateCcb (
	IN PFILESPY_DEVICE_EXTENSION	DevExt,
	IN	PFILE_OBJECT				FileObject,
	IN  ULONG						BufferLength
	); 

VOID
ReadonlyFreeCcb (
	IN PFILESPY_DEVICE_EXTENSION	DevExt,
	IN PNDAS_CCB					Ccb
	);


PNDAS_CCB
ReadonlyLookUpCcbByHandle (
	IN PFILESPY_DEVICE_EXTENSION	DevExt,
	IN HANDLE						FileHandle
	);

PNDAS_CCB
ReadonlyLookUpCcb (
	IN PFILESPY_DEVICE_EXTENSION	DevExt,
	IN PFILE_OBJECT					FileObject
	);

PNDAS_CCB
ReadonlyLookUpCcbByReadonlyFileObject (
	IN PFILESPY_DEVICE_EXTENSION	DevExt,
	IN PFILE_OBJECT					ReadonlyFileObject
	);

NTSTATUS
ReadonlyMakeFullFileName (
	IN PFILESPY_DEVICE_EXTENSION	DevExt,
	IN PFILE_OBJECT					FileObject, 
	IN PUNICODE_STRING				FullFileName,
	IN BOOLEAN						FileDirectoryFile
	);


// lfstable.c

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
IsNetdiskPartition(
   IN	PDEVICE_OBJECT			DiskDeviceObject,
   OUT  PNETDISK_INFORMATION	NetdiskInformation,
   OUT	PDEVICE_OBJECT			*ScsiportDeviceObject
	);


NTSTATUS
LfsFilterDeviceIoControl(
    IN  PDEVICE_OBJECT	DeviceObject,
    IN  ULONG			IoCtl,
    IN  PVOID			InputBuffer OPTIONAL,
    IN  ULONG			InputBufferLength,
    IN  PVOID			OutputBuffer OPTIONAL,
    IN  ULONG			OutputBufferLength,
    OUT PULONG_PTR		IosbInformation OPTIONAL
    );


NTSTATUS
GetScsiportAdapter(
  	IN	PDEVICE_OBJECT	DiskDeviceObject,
  	OUT	PDEVICE_OBJECT	*ScsiportAdapterDeviceObject
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


NTSTATUS
LfsQueryDirectoryByIndex(
    HANDLE 	FileHandle,
    ULONG	FileInformationClass,
    PVOID  	FileInformation,
    ULONG	Length,
    ULONG	FileIndex,
    PUNICODE_STRING FileName,
    PIO_STATUS_BLOCK IoStatusBlock,    
    BOOLEAN ReturnSingleEntry
    );

ULONG
LfsGetLastFileIndexFromQuery(
		ULONG			infoType,
		PCHAR			QueryBuff,
		ULONG			BuffLength
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

BOOLEAN	
IsMetaFile (
	IN PUNICODE_STRING	fileName
	); 

struct Create {

	PIO_SECURITY_CONTEXT		SecurityContext;
    ULONG						Options;
    USHORT POINTER_ALIGNMENT	FileAttributes;
    USHORT						ShareAccess;
    ULONG  POINTER_ALIGNMENT	EaLength;
};


struct DeviceIoControl {

	ULONG					OutputBufferLength;
    ULONG POINTER_ALIGNMENT InputBufferLength;
    ULONG POINTER_ALIGNMENT IoControlCode;
    PVOID					Type3InputBuffer;
};


struct FileSystemControl {

	ULONG					OutputBufferLength;
    ULONG POINTER_ALIGNMENT InputBufferLength;
    ULONG POINTER_ALIGNMENT FsControlCode;
    PVOID					Type3InputBuffer;
};


struct QueryDirectory {

	ULONG					Length;
    PSTRING					FileName;
	FILE_INFORMATION_CLASS	FileInformationClass;
	ULONG POINTER_ALIGNMENT FileIndex;
};

 
struct QueryFile {

	ULONG										Length;
    FILE_INFORMATION_CLASS POINTER_ALIGNMENT	FileInformationClass;
};

struct SetFile {

	ULONG										Length;
    FILE_INFORMATION_CLASS POINTER_ALIGNMENT	FileInformationClass;
    PFILE_OBJECT								FileObject;
    
	union {

		struct {

			BOOLEAN ReplaceIfExists;
            BOOLEAN AdvanceOnly;
		};

        ULONG  ClusterCount;
        HANDLE DeleteHandle;
	};
};


struct QueryVolume {

	ULONG									Length;
	FS_INFORMATION_CLASS POINTER_ALIGNMENT	FsInformationClass;
};

        //
        // System service parameters for:  NtSetVolumeInformationFile
        //

struct SetVolume {

	ULONG Length;
	FS_INFORMATION_CLASS POINTER_ALIGNMENT FsInformationClass;
};

struct Read {

	ULONG					Length;
	ULONG POINTER_ALIGNMENT Key;
    LARGE_INTEGER			ByteOffset;
};


struct Write {

	ULONG					Length;
	ULONG POINTER_ALIGNMENT Key;
	LARGE_INTEGER			ByteOffset;
};


struct LockControl {

	PLARGE_INTEGER			Length;
    ULONG POINTER_ALIGNMENT Key;
    LARGE_INTEGER			ByteOffset;
};


struct QuerySecurity {

	SECURITY_INFORMATION SecurityInformation;
	ULONG POINTER_ALIGNMENT Length;
};


struct SetSecurity {

	SECURITY_INFORMATION SecurityInformation;
	PSECURITY_DESCRIPTOR SecurityDescriptor;
};


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

#if __NDAS_FS_MINI__

PVOID
MinispyMapInputBuffer (
	IN  PFLT_IO_PARAMETER_BLOCK	Iopb,
	IN  KPROCESSOR_MODE			RequestorMode
	);

PVOID
MinispyMapOutputBuffer (
	IN  PFLT_IO_PARAMETER_BLOCK	Iopb,
	IN  KPROCESSOR_MODE			RequestorMode
	);

#endif

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


BOOLEAN
AddUserBacl(
	IN	PDEVICE_OBJECT	DiskDeviceObject,
	IN	PNDAS_BLOCK_ACE	NdasBace,
	OUT PBLOCKACE_ID	BlockAceId
	);

NTSTATUS
RemoveUserBacl(
	IN PDEVICE_OBJECT	DiskDeviceObject,
	IN BLOCKACE_ID		BlockAceId
	);

NTSTATUS
GetNdasScsiBacl(
	IN	PDEVICE_OBJECT	DiskDeviceObject,
	OUT PNDAS_BLOCK_ACL	NdasBacl,
	IN BOOLEAN			SystemOrUser
	);


BOOLEAN
Secondary_Ioctl(
	IN  PSECONDARY			Secondary,
	IN  PIRP				Irp,
	IN PIO_STACK_LOCATION	IrpSp,
	OUT PNTSTATUS			NtStatus
	);

VOID
CloseOpenFiles(
	IN PPRIMARY_SESSION	PrimarySession,
	IN BOOLEAN			Remove
	);

NTSTATUS
ReceiveNtfsWinxpMessage(
	IN	PPRIMARY_SESSION	PrimarySession,
	IN  _U16				Mid
	);


NTSTATUS
SendNdfsWinxpMessage(
	IN PPRIMARY_SESSION			PrimarySession,
	IN PNDFS_REPLY_HEADER		NdfsReplyHeader, 
	IN PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplyHeader,
	IN _U32						ReplyDataSize,
	IN _U16						Mid
	);


NTSTATUS
DispatchRequest(
	IN PPRIMARY_SESSION	PrimarySession
	);


VOID
DisconnectFromSecondary(
	IN	PPRIMARY_SESSION	PrimarySession
	);


NTSTATUS
PrimarySessionTakeOver( 
	IN  PPRIMARY_SESSION	PrimarySession
    );


VOID
PrimarySession_FreeOpenFile(
	IN	PPRIMARY_SESSION PrimarySession,
	IN  POPEN_FILE		 OpenedFile
	);

NTSTATUS
GetVolumeInformation(
	IN PPRIMARY_SESSION	PrimarySession,
	IN PUNICODE_STRING	VolumeName
	);

VOID
DispatchWinXpRequestWorker(
	IN  PPRIMARY_SESSION	PrimarySession,
	IN  _U16				Mid
    );

#if __NDAS_FS_MINI__

// minisecondary.c

NTSTATUS 
MiniSecondaryNtfsPassThrough (
	IN  PSECONDARY				Secondary,
	IN OUT PFLT_CALLBACK_DATA	Data
	);

BOOLEAN
MiniSecondary_Ioctl (
	IN  PSECONDARY				Secondary,
	IN OUT PFLT_CALLBACK_DATA	Data
	);

// minisecondaryredirectirp.c

NTSTATUS
MiniRedirectIrp (
	IN  PSECONDARY				Secondary,
	IN OUT PFLT_CALLBACK_DATA	Data,
	OUT	PBOOLEAN				FastMutexSet,
	OUT	PBOOLEAN				Retry
	);

#endif

#endif

