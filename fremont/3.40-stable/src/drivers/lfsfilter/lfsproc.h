#ifndef __LFS_PROC_H__
#define __LFS_PROC_H__


#define __NDAS_FS__										1
#define __NDAS_FS_W2K_SUPPORT__							1
#define __NDAS_FS_ALLOW_UNLOAD_DRIVER__					1
#define __NDAS_FS_ENUMERATION__							0

#define __NDAS_FS_CALL_CLOSE_ON_PNP__					0

#define __NDAS_FS_DBG__									1
#ifdef _NDAS_FS_MINI_
#define __NDAS_FS_MINI__								1
#define __NDAS_FS_ENABLE_MINI_MODE__					0
#else		
#define __NDAS_FS_MINI_CHECK__							1
#endif

#define __NDAS_FS_NDAS_NTFS_MODE__						0

#define __NDAS_FS_CHECK_NTFS__							1
#define __NDAS_FS_CHECK_FAT__							1
#define __NDAS_FS_CHECK_ROFS__							1

#define __NDAS_FS_FORCE_FILTERING__						0

#define __NDAS_FS_FLUSH_VOLUME__						0

#define	__NDAS_FS_TEST_MODE__							0
#define	__NDAS_FS_PROCESS_SECONDARY_REQUEST_LOCALLY__	0


#if __NDAS_FS_W2K_SUPPORT__

#ifndef _WIN2K_COMPAT_SLIST_USAGE
#define _WIN2K_COMPAT_SLIST_USAGE
#endif

#endif

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

#include <srb.h>
#include <initguid.h>
#include <stdio.h>
#include <ntddscsi.h>

#include "ndascommonheader.h"

extern BOOLEAN NdasTestBug;

#if DBG

#define NDAS_ASSERT(exp)	ASSERT(exp)

#else

#define NDAS_ASSERT(exp)				\
	((NdasTestBug && (exp) == FALSE) ?	\
	NdasDbgBreakPoint() :				\
	FALSE)

#endif

#if __NDAS_FS_COMPILE_FOR_VISTA__
#include "lh_supp.h"
#endif

#include <stdlib.h>
#include <initguid.h>
#include <ntdddisk.h>
#include <ntddvol.h>

#if __NDAS_FS_MINI__
#include "minispy.h"
#endif

#include "filespylib.h"
#include "namelookup.h"
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

NTKERNELAPI
NTSTATUS
IoReportTargetDeviceChange(
    IN PDEVICE_OBJECT PhysicalDeviceObject,
    IN PVOID NotificationStructure  // always begins with a PLUGPLAY_NOTIFICATION_HEADER
    );

#if 1

typedef struct _MFT_SEGMENT_REFERENCE {

    //
    //  First a 48 bit segment number.
    //

    ULONG SegmentNumberLowPart;                                    //  offset = 0x000
    USHORT SegmentNumberHighPart;                                  //  offset = 0x004

    //
    //  Now a 16 bit nonzero sequence number.  A value of 0 is
    //  reserved to allow the possibility of a routine accepting
    //  0 as a sign that the sequence number check should be
    //  repressed.
    //

    USHORT SequenceNumber;                                          //  offset = 0x006

} MFT_SEGMENT_REFERENCE, *PMFT_SEGMENT_REFERENCE;                   //  sizeof = 0x008

//
//  A file reference in NTFS is simply the MFT Segment Reference of
//  the Base file record.
//

typedef MFT_SEGMENT_REFERENCE FILE_REFERENCE, *PFILE_REFERENCE;

#endif


typedef struct _FILESPY_DEVICE_EXTENSION *PFILESPY_DEVICE_EXTENSION;
#define DEVICE_NAMES_SZ  100

#include "ndasscsi.h"
#include "ndasbusioctl.h"

#include "ndasfs.h"
#include "SocketLpx.h"
#include "lpxtdiv2.h"

#include "lfsfilterpublic.h"

#include "ndasflowcontrol.h"

#include "NdfsProtocolHeader2.h"
#include "NdftProtocolHeader.h"
#include "NdfsInteface.h"

#include "md5.h"

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

#define LFS_MARK						0xF23AC135 // It's Real Random Number

#define LFS_TIME_OUT						(360000 * NANO100_PER_SEC)  // This timeout should not happen. Because of possible delay in ConnectToPrimary, set to at least over 120 sec
#define LFS_SECONDARY_THREAD_FLAG_TIME_OUT	(10 * NANO100_PER_SEC)
#define LFS_CHANGECONNECTION_TIMEOUT		(60000 * NANO100_PER_SEC) // ((RECONNECTION_MAX_TRY * MAX_RECONNECTION_INTERVAL) * 2)
#define LFS_DEADLOCK_AVOID_TIMEOUT			(180 * NANO100_PER_SEC) // ((RECONNECTION_MAX_TRY * MAX_RECONNECTION_INTERVAL) * 2)
#define LFS_QUERY_REMOVE_TIME_OUT			(120 * NANO100_PER_SEC) 

#define LFS_READONLY_THREAD_FLAG_TIME_OUT	(30 * NANO100_PER_SEC)

#define LFS_INSUFFICIENT_RESOURCES	0
#define LFS_BUG						0
#define LFS_LPX_BUG					0
#define LFS_UNEXPECTED				0
#define LFS_REQUIRED				0
#define LFS_IRP_NOT_IMPLEMENTED		0
#define LFS_CHECK					0

#define LFS_ALLOC_TAG					'taFL'
#define LFS_FCB_NONPAGED_TAG			'nfFL'
#define LFS_FCB_TAG						'tfFL'
#define LFS_ERESOURCE_TAG				'teFL'

#define NETDISK_MANAGER_TAG				'mnFL'
#define NETDISK_MANAGER_REQUEST_TAG		'rmFL'
#define NETDISK_MANAGER_ENABLED_TAG		'emFL'
#define NETDISK_MANAGER_PARTITION_TAG	'pmFL'

#define PRIMARY_AGENT_MESSAGE_TAG		'apFL'
#define PRIMARY_SESSION_MESSAGE_TAG		'msFL'
#define PRIMARY_SESSION_BUFFERE_TAG		'bpFL'
#define SECONDARY_MESSAGE_TAG			'esFL'
#define LFS_CCB_TAG						'efFL'
#define OPEN_FILE_TAG					'foFL'
#define STOPPED_VOLUME_TAG				'vsFL'

#define LFSLIB_SRBIOCTL					'rsFL'

#define LFSTAB_ENTRY_TAG				'etFL'
#define LFS_ERRORLOGWORKER_TAG			'weLF'

#define IDNODECALLBACK_TAG				'ciFL'

#define READONLY_MESSAGE_TAG			'RsFL'

#define NDFS_OVERRAP_DATA_TAG			'oveL'

#define THREAD_WAIT_OBJECTS 3           // Builtin usable wait blocks from <ntddk.h>

#define ADD_ALIGN8(Length) ((Length+7) >> 3 << 3)


#if __NDAS_FS_MINI__

// ctxinit.c

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
NdasFsReadonlyPreOperationCallback (
	IN PCFLT_RELATED_OBJECTS	FltObjects,
	IN PCTX_INSTANCE_CONTEXT	InstanceContext,
	IN OUT PFLT_CALLBACK_DATA	Data
	);

NTSTATUS
NdasFsReadonlyPostOperationCallback (
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
	OUT PLPX_ADDRESS					PrimaryAddressList,
	OUT PLONG							NumberOfPrimaryAddress,
	OUT PBOOLEAN						IsLocalAddress	
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

// lfsstruct.c

INLINE
PNON_PAGED_FCB
LfsAllocateNonPagedFcb (
	VOID
    );

INLINE
VOID
LfsFreeNonPagedFcb (
    IN  PNON_PAGED_FCB NonPagedFcb
    );

INLINE
PERESOURCE
LfsAllocateResource (
	VOID
    );

INLINE
VOID
LfsFreeResource (
    IN PERESOURCE Resource
    );

PLFS_FCB
AllocateFcb (
	IN	PSECONDARY		Secondary,
	IN PUNICODE_STRING	FullFileName,
	IN BOOLEAN			IsPagingFile
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

PLFS_CCB
AllocateCcb (
	IN	PSECONDARY		Secondary,
	IN	PFILE_OBJECT	FileObject,
	IN  ULONG			BufferLength
	); 

VOID
FreeCcb (
	IN  PSECONDARY		Secondary,
	IN  PLFS_CCB	Ccb
	);

PLFS_CCB
Secondary_LookUpCcbByHandle (
	IN PSECONDARY	Secondary,
	IN HANDLE		FileHandle
	);
	
PLFS_CCB
Secondary_LookUpCcb (
	IN PSECONDARY	Secondary,
	IN PFILE_OBJECT	FileObject
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

#if __NDAS_FS_MINI__

VOID
PrintData (
	IN ULONG					DebugLevel,
	IN PCHAR					Where,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt,
	IN PFLT_CALLBACK_DATA		Data
	);

#endif

#else

#define PrintIrp( DebugLevel, Where, LfsDeviceExt, Irp )

#define PrintData( DebugLevel, Where, LfsDeviceExt, Irp )

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
NetdiskManager_QueryRemoveMountVolume (
	IN PNETDISK_MANAGER		NetdiskManager,
	IN PNETDISK_PARTITION	NetdiskPartition,
	IN NETDISK_ENABLE_MODE	NetdiskEnableMode
	);

VOID
NetdiskManager_CancelRemoveMountVolume (
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
	IN  BOOLEAN					CallFromMiniFilter,
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
PrimarySession_Create (
	IN  PPRIMARY			Primary,
	IN	HANDLE				ListenFileHandle,
	IN  PFILE_OBJECT		ListenFileObject,
	IN  PLPXTDI_OVERLAPPED_CONTEXT	ListenOverlapped,
	IN  ULONG				ListenSocketIndex,
	IN  PLPX_ADDRESS		RemoteAddress
	);


VOID
PrimarySession_Close (
	IN 	PPRIMARY_SESSION	PrimarySession
	);


VOID
PrimarySession_FileSystemShutdown (
	IN 	PPRIMARY_SESSION	PrimarySession
	);


VOID
PrimarySession_Stopping (
	IN 	PPRIMARY_SESSION	PrimarySession
	);

VOID
PrimarySession_Disconnect (
	IN 	PPRIMARY_SESSION	PrimarySession
	);

VOID
PrimarySession_CancelStopping (
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
Primary_CleanupMountedDevice (
	IN PPRIMARY					Primary,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt
	);


PPRIMARY_SESSION_REQUEST
AllocPrimarySessionRequest (
	IN	BOOLEAN	Synchronous
); 


VOID
DereferencePrimarySessionRequest (
	IN	PPRIMARY_SESSION_REQUEST	PrimarySessionRequest
	);


FORCEINLINE
NTSTATUS
QueueingPrimarySessionRequest (
	IN	PPRIMARY_SESSION			PrimarySession,
	IN	PPRIMARY_SESSION_REQUEST	PrimarySessionRequest,
	IN  BOOLEAN						FastMutexAcquired
	);

// primarysessionthread.c

VOID
PrimarySessionThreadProc (
	IN PPRIMARY_SESSION PrimarySession
	);



// primarySessiondispatchwinxprequest.c

POPEN_FILE
PrimarySession_AllocateOpenFile (
	IN	PPRIMARY_SESSION	PrimarySession,
	IN  HANDLE				FileHandle,
	IN  PFILE_OBJECT		FileObject,
	IN	PUNICODE_STRING		FullFileName
	);


NTSTATUS
DispatchWinXpRequest (
	IN  PPRIMARY_SESSION			PrimarySession,
	IN  PNDFS_WINXP_REQUEST_HEADER	NdfsWinxpRequestHeader,
	IN  PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplytHeader,
	IN  UINT32						DataSize,
	OUT	UINT32						*replyDataSize
	);


// secondary.c


FORCEINLINE
VOID
INITIALIZE_NDFS_REQUEST_HEADER (
	PNDFS_REQUEST_HEADER	NdfsRequestHeader,
	UINT8					Command,
	PSECONDARY				Secondary,
	UINT8					IrpMajorFunction,
	UINT32					DataSize
	)
{
	//KIRQL	oldIrql;

	//KeAcquireSpinLock(&Secondary->SpinLock, &oldIrql);
	ExAcquireFastMutex( &Secondary->FastMutex );

	RtlCopyMemory(NdfsRequestHeader->Protocol, NDFS_PROTOCOL, sizeof(NdfsRequestHeader->Protocol));
	NdfsRequestHeader->Command	= Command;
	NdfsRequestHeader->Flags	= Secondary->Thread.SessionContext.Flags;
	NdfsRequestHeader->Uid2		= HTONS(Secondary->Thread.SessionContext.Uid);
	NdfsRequestHeader->Tid2		= HTONS(Secondary->Thread.SessionContext.Tid);
	NdfsRequestHeader->Mid2		= 0;

	NdfsRequestHeader->MessageSize4 = sizeof(NDFS_REQUEST_HEADER) + 
									((Secondary->Thread.SessionContext.MessageSecurity == 0)
										? sizeof(NDFS_WINXP_REQUEST_HEADER) + DataSize : 
									 (((IrpMajorFunction == IRP_MJ_WRITE && 
										Secondary->Thread.SessionContext.RwDataSecurity == 0 && 
										DataSize <= Secondary->Thread.SessionContext.PrimaryMaxDataSize) ||
										IrpMajorFunction == IRP_MJ_READ && 
										Secondary->Thread.SessionContext.RwDataSecurity == 0 && 
										DataSize <= Secondary->Thread.SessionContext.SecondaryMaxDataSize)

										? ADD_ALIGN8(sizeof(NDFS_WINXP_REQUEST_HEADER) + DataSize) : 
										  ADD_ALIGN8(sizeof(NDFS_WINXP_REQUEST_HEADER) + DataSize)));

	NdfsRequestHeader->MessageSize4 = HTONL(NdfsRequestHeader->MessageSize4);

	ExReleaseFastMutex( &Secondary->FastMutex );

	return;
}																										

PSECONDARY
Secondary_Create (
	IN	PLFS_DEVICE_EXTENSION	LfsDeviceExt
	);

VOID
Secondary_Close (
	IN	PLFS_DEVICE_EXTENSION	LfsDeviceExt,
	IN  PSECONDARY				Secondary
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
Secondary_TryCloseCcbs (
	PSECONDARY Secondary
	);

BOOLEAN 
SecondaryPassThrough (
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
	PLFS_CCB		Ccb,
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
	IN PREADONLY	Readonly,
	IN PFILE_OBJECT	FileObject
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

VOID
ReadonlyTryCloseCcb (
	IN PREADONLY Readonly
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

#if 0
NTSTATUS
LfsTable_QueryPrimaryAddress (
	IN  PLFS_TABLE				LfsTable,
	IN  PNETDISK_PARTITION_INFO	NetDiskPartitionInfo,
	OUT PLPX_ADDRESS			PrimaryAddress
	);
#endif

NTSTATUS
LfsTable_QueryPrimaryAddressList (
	IN  PLFS_TABLE				LfsTable,
	IN  PNETDISK_PARTITION_INFO	NetDiskPartitionInfo,
	OUT PLPX_ADDRESS			PrimaryAddressList,
	OUT PLONG					NumberOfPrimaryAddress
	);

VOID
LfsTable_CleanCachePrimaryAddress (
	IN  PLFS_TABLE				LfsTable,
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
SendMessage (
	IN PFILE_OBJECT			ConnectionFileObject,
	IN PNDAS_FC_STATISTICS	SendNdasFcStatistics,
	IN PLARGE_INTEGER		TimeOut,
	IN UINT8				*Buffer, 
	IN UINT32				BufferLength
	);


NTSTATUS
RecvMessage (
	IN PFILE_OBJECT			ConnectionFileObject,
	IN PNDAS_FC_STATISTICS	RecvNdasFcStatistics,
	IN PLARGE_INTEGER		TimeOut,
	IN UINT8				*Buffer, 
	IN UINT32				BufferLength
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
MapUserBuffer (
    IN PIRP Irp
    );

PVOID
MapInputBuffer (
	PIRP Irp
	);


PVOID
MapOutputBuffer (
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
NDCtrlUnplug(
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

NTSTATUS
GetNdasScsiBacl(
	IN	PDEVICE_OBJECT	DiskDeviceObject,
	OUT PNDAS_BLOCK_ACL	NdasBacl,
	IN BOOLEAN			SystemOrUser
	);

NTSTATUS
NdasScsiCtrlUpgradeToWrite(
	IN PDEVICE_OBJECT	DiskDeviceObject,
	OUT PIO_STATUS_BLOCK	IoStatus
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
	IN  UINT16				Mid
	);


NTSTATUS
SendNdfsWinxpMessage(
	IN PPRIMARY_SESSION			PrimarySession,
	IN PNDFS_REPLY_HEADER		NdfsReplyHeader, 
	IN PNDFS_WINXP_REPLY_HEADER	NdfsWinxpReplyHeader,
	IN UINT32						ReplyDataSize,
	IN UINT16						Mid
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

#if __NDAS_FS_MINI__

// minisecondary.c

NTSTATUS 
MiniSecondaryPassThrough (
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

