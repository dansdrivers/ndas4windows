/*++

Copyright (c) 1989-1999  Microsoft Corporation

Module Name:

    fspyKern.h

Abstract:
    Header file which contains the structures, type definitions,
    constants, global variables and function prototypes that are
    only visible within the kernel.

    As of the Windows XP SP1 IFS Kit version of this sample and later, this
    sample can be built for each build environment released with the IFS Kit
    with no additional modifications.  To provide this capability, additional
    compile-time logic was added -- see the '#if WINVER' locations.  Comments
    tagged with the 'VERSION NOTE' header have also been added as appropriate to
    describe how the logic must change between versions.

    If this sample is built in the Windows XP environment or later, it will run
    on Windows 2000 or later.  This is done by dynamically loading the routines
    that are only available on Windows XP or later and making run-time decisions
    to determine what code to execute.  Comments tagged with 'MULTIVERISON NOTE'
    mark the locations where such logic has been added.


Environment:

    Kernel mode

--*/
#ifndef __FSPYKERN_H__
#define __FSPYKERN_H__

#include "namelookup.h"

//
//  VERSION NOTE:
//
//  The following useful macros are defined in NTIFS.H in Windows XP and later.
//  We will define them locally if we are building for the Windows 2000
//  environment.
//

#if WINVER == 0x0500

//
//  These macros are used to test, set and clear flags respectively
//

#ifndef FlagOn
#define FlagOn(_F,_SF)        ((_F) & (_SF))
#endif

#ifndef BooleanFlagOn
#define BooleanFlagOn(F,SF)   ((BOOLEAN)(((F) & (SF)) != 0))
#endif

#ifndef SetFlag
#define SetFlag(_F,_SF)       ((_F) |= (_SF))
#endif

#ifndef ClearFlag
#define ClearFlag(_F,_SF)     ((_F) &= ~(_SF))
#endif


#define RtlInitEmptyUnicodeString(_ucStr,_buf,_bufSize) \
    ((_ucStr)->Buffer = (_buf), \
     (_ucStr)->Length = 0, \
     (_ucStr)->MaximumLength = (USHORT)(_bufSize))


#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#define ExFreePoolWithTag( a, b ) ExFreePool( (a) )
#endif /* WINVER == 0x0500 */

//
//  This controls how FileSpy is built.  It has 2 options:
//  0 - Build using NameHashing (old way, see fspyHash.c)
//  1 - Build using StreamContexts (new Way, see fspyCtx.c)
//
//  VERSION NOTE:
//
//  Filter stream contexts are only supported on Windows XP and later
//  OS versions.  This support was not available in Windows 2000 or NT 4.0.
//

#define USE_STREAM_CONTEXTS 0

#if USE_STREAM_CONTEXTS && WINVER < 0x0501
#error Stream contexts on only supported on Windows XP or later.
#endif

//
//  POOL Tag definitions
//

#define FILESPY_POOL_TAG                'yPsF'
#define FILESPY_LOGRECORD_TAG           'rLsF'
#define FILESPY_CONTEXT_TAG             'xCsF'
#define FILESPY_NAME_BUFFER_TAG         'bNsF'
#define FILESPY_DEVNAME_TAG             'nDsF'
#define FILESPY_USERNAME_TAG            'nUsF'
#define FILESPY_TRANSACTION_TAG         'xTsF'

#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((HANDLE) -1)
#endif

#define CONSTANT_UNICODE_STRING(s)   { sizeof( s ) - sizeof( WCHAR ), sizeof(s), s }

//
//  Delay values for KeDelayExecutionThread()
//  (Values are negative to represent relative time)
//

#define DELAY_ONE_MICROSECOND   (-10)
#define DELAY_ONE_MILLISECOND   (DELAY_ONE_MICROSECOND*1000)
#define DELAY_ONE_SECOND        (DELAY_ONE_MILLISECOND*1000)

//
//  Don't use look-aside-list in the debug versions.
//

#if DBG
#define MEMORY_DBG
#endif

//---------------------------------------------------------------------------
//  Macros for FileSpy DbgPrint levels.
//---------------------------------------------------------------------------

#if __NDAS_FS__

#if DBG

#define SPY_LOG_PRINT( _dbgLevel, _string )                 \
    (FlagOn(gFileSpyDebugLevel,(_dbgLevel)) ?               \
        DbgPrint _string  :                                 \
        ((void)0))

#else

#define SPY_LOG_PRINT( _dbgLevel, _string )

#endif

#else

#define SPY_LOG_PRINT( _dbgLevel, _string )                 \
    (FlagOn(gFileSpyDebugLevel,(_dbgLevel)) ?               \
        DbgPrint _string  :                                 \
        ((void)0))

#endif


//---------------------------------------------------------------------------
//  Generic Resource acquire/release macros
//---------------------------------------------------------------------------

#define SpyAcquireResourceExclusive( _r, _wait )                            \
    (ASSERT( ExIsResourceAcquiredExclusiveLite((_r)) ||                     \
            !ExIsResourceAcquiredSharedLite((_r)) ),                        \
     KeEnterCriticalRegion(),                                               \
     ExAcquireResourceExclusiveLite( (_r), (_wait) ))

#define SpyAcquireResourceShared( _r, _wait )                               \
    (KeEnterCriticalRegion(),                                               \
     ExAcquireResourceSharedLite( (_r), (_wait) ))

#define SpyReleaseResource( _r )                                            \
    (ASSERT( ExIsResourceAcquiredSharedLite((_r)) ||                        \
             ExIsResourceAcquiredExclusiveLite((_r)) ),                     \
     ExReleaseResourceLite( (_r) ),                                         \
     KeLeaveCriticalRegion())

//---------------------------------------------------------------------------
//  Macro to test if we are logging for this device
//
//  NOTE: We don't bother synchronizing to check the gControlDeviceState since
//    we can tolerate a stale value here.  We just look at it here to avoid
//    doing the logging work if we can.  We synchronize to check the
//    gControlDeviceState before we add the log record to the gOutputBufferList
//    and discard the log record if the ControlDevice is no longer OPENED.
//---------------------------------------------------------------------------

#define SHOULD_LOG(pDeviceObject) \
    ((gControlDeviceState == OPENED) && \
     FlagOn(((PFILESPY_DEVICE_EXTENSION)(pDeviceObject)->DeviceExtension)->Flags,LogThisDevice))


//---------------------------------------------------------------------------
//      Global variables
//---------------------------------------------------------------------------

//
//  Debugger definitions
//

typedef enum _SPY_DEBUG_FLAGS {

    SPYDEBUG_DISPLAY_ATTACHMENT_NAMES       = 0x00000001,
    SPYDEBUG_ERROR                          = 0x00000002,
    SPYDEBUG_TRACE_NAME_REQUESTS            = 0x00000004,
    SPYDEBUG_TRACE_IRP_OPS                  = 0x00000010,
    SPYDEBUG_TRACE_FAST_IO_OPS              = 0x00000020,
    SPYDEBUG_TRACE_FSFILTER_OPS             = 0x00000040,
    SPYDEBUG_TRACE_TX_OPS                   = 0x00000080,
    SPYDEBUG_TRACE_CONTEXT_OPS              = 0x00000100,
    SPYDEBUG_TRACE_DETAILED_CONTEXT_OPS     = 0x00000200,
    SPYDEBUG_TRACE_MISMATCHED_NAMES         = 0x00001000,
    SPYDEBUG_ASSERT_MISMATCHED_NAMES        = 0x00002000,

    SPYDEBUG_BREAK_ON_DRIVER_ENTRY          = 0x80000000,

#if __NDAS_FS__

    LFS_DEBUG_LFS_NOISE		            = 0x10000000,
    LFS_DEBUG_LFS_TRACE		            = 0x20000000,
    LFS_DEBUG_LFS_INFO		            = 0x40000000,
    LFS_DEBUG_LFS_ERROR		            = 0x80000000,

    LFS_DEBUG_NETDISK_MANAGER_NOISE		= 0x01000000,
    LFS_DEBUG_NETDISK_MANAGER_TRACE		= 0x02000000,
    LFS_DEBUG_NETDISK_MANAGER_INFO		= 0x04000000,
    LFS_DEBUG_NETDISK_MANAGER_ERROR		= 0x08000000,

    LFS_DEBUG_PRIMARY_NOISE			    = 0x00100000,
    LFS_DEBUG_PRIMARY_TRACE				= 0x00200000,
    LFS_DEBUG_PRIMARY_INFO				= 0x00400000,
    LFS_DEBUG_PRIMARY_ERROR				= 0x00800000,

    LFS_DEBUG_SECONDARY_NOISE		    = 0x00010000,
    LFS_DEBUG_SECONDARY_TRACE	        = 0x00020000,
    LFS_DEBUG_SECONDARY_INFO	        = 0x00040000,
    LFS_DEBUG_SECONDARY_ERROR	        = 0x00080000,

	LFS_DEBUG_READONLY_NOISE		    = 0x00000100,
	LFS_DEBUG_READONLY_TRACE	        = 0x00000200,
	LFS_DEBUG_READONLY_INFO				= 0x00000400,
	LFS_DEBUG_READONLY_ERROR	        = 0x00000800,

    LFS_DEBUG_TABLE_NOISE		        = 0x00001000,
    LFS_DEBUG_TABLE_TRACE		        = 0x00002000,
    LFS_DEBUG_TABLE_INFO		        = 0x00004000,
    LFS_DEBUG_TABLE_ERROR		        = 0x00008000,

    LFS_DEBUG_LIB_NOISE				    = 0x00000100,
    LFS_DEBUG_LIB_TRACE					= 0x00000200,
    LFS_DEBUG_LIB_INFO					= 0x00000400,
    LFS_DEBUG_LIB_ERROR					= 0x00000800,

#endif

} SPY_DEBUG_FLAGS;

//
//  FileSpy global variables.
//

extern PAGED_LOOKASIDE_LIST gFileSpyNameBufferLookasideList;

extern SPY_DEBUG_FLAGS gFileSpyDebugLevel;
extern ULONG gFileSpyAttachMode;

extern PDEVICE_OBJECT gControlDeviceObject;
extern PDRIVER_OBJECT gFileSpyDriverObject;

extern FAST_MUTEX gSpyDeviceExtensionListLock;
extern LIST_ENTRY gSpyDeviceExtensionList;

extern KSPIN_LOCK gOutputBufferLock;
extern LIST_ENTRY gOutputBufferList;

extern NPAGED_LOOKASIDE_LIST gFreeBufferList;

extern ULONG gLogSequenceNumber;
extern KSPIN_LOCK gLogSequenceLock;

extern UNICODE_STRING gVolumeString;
extern UNICODE_STRING gOverrunString;
extern UNICODE_STRING gPagingIoString;
extern UNICODE_STRING gEmptyUnicode;
extern UNICODE_STRING gInsufficientUnicode;

#if (__NDAS_FS__ && !defined NTDDI_VERSION)
extern LONG gStaticBufferInUse;
#else
__volatile extern LONG gStaticBufferInUse;
#endif

extern CHAR gOutOfMemoryBuffer[RECORD_SIZE];

//
//  Statistics definitions.  Note that we don't do interlocked operations
//  because loosing a count once in a while isn't important enough vs the
//  overhead.
//

extern FILESPY_STATISTICS gStats;

#define INC_STATS(field)    (gStats.field++)
#define INC_LOCAL_STATS(var) ((var)++)

//
//  Attachment lock.
//

extern FAST_MUTEX gSpyAttachLock;

//
//  FileSpy Registry values.
//

#define DEFAULT_MAX_RECORDS_TO_ALLOCATE 100;
#define DEFAULT_MAX_NAMES_TO_ALLOCATE   100;
#define DEFAULT_FILESPY_DEBUG_LEVEL     SPYDEBUG_ERROR;
#define MAX_RECORDS_TO_ALLOCATE         L"MaxRecords"
#define MAX_NAMES_TO_ALLOCATE           L"MaxNames"
#define DEBUG_LEVEL                     L"DebugFlags"
#define ATTACH_MODE                     L"AttachMode"

#if __NDAS_FS__
#define MAX_DATA_TRANSFER_PRI			L"MaxDataTransferP"
#define MAX_DATA_TRANSFER_SEC			L"MaxDataTransferS"
#endif

extern LONG gMaxRecordsToAllocate;
extern LONG gRecordsAllocated;
extern LONG gMaxNamesToAllocate;
extern LONG gNamesAllocated;

//
//  Our Control Device State information.
//

typedef enum _CONTROL_DEVICE_STATE {

    OPENED,
    CLOSED,
    CLEANING_UP

} CONTROL_DEVICE_STATE;

extern CONTROL_DEVICE_STATE gControlDeviceState;
extern KSPIN_LOCK gControlDeviceStateLock;

//
//  Given a device type, return a valid name.
//

extern const PCHAR DeviceTypeNames[];
extern ULONG SizeOfDeviceTypeNames;

#define GET_DEVICE_TYPE_NAME( _type ) \
            ((((_type) > 0) &&      \
            ((_type) < (SizeOfDeviceTypeNames / sizeof(PCHAR)))) ? \
                DeviceTypeNames[ (_type) ] : \
                "[Unknown]")

//
//  Filespy global variables for transaction supports.
//

#if (WINVER >= 0x0600 || __NDAS_FS_COMPILE_FOR_VISTA__)

extern HANDLE gKtmTransactionManagerHandle;

extern HANDLE gKtmResourceManagerHandle;

extern PKRESOURCEMANAGER gKtmResourceManager;

extern NPAGED_LOOKASIDE_LIST gTransactionList;

extern UNICODE_STRING gNtfsDriverName;

#endif

//---------------------------------------------------------------------------
//      Global defines
//---------------------------------------------------------------------------

//
//  Macro to test for device types we want to attach to.
//

#if __NDAS_FS__
#define IS_SUPPORTED_DEVICE_TYPE(_type) \
     (((_type) == FILE_DEVICE_CD_ROM_FILE_SYSTEM) || \
      ((_type) == FILE_DEVICE_DISK_FILE_SYSTEM))
#else
#define IS_SUPPORTED_DEVICE_TYPE(_type) \
    (((_type) == FILE_DEVICE_DISK_FILE_SYSTEM) || \
     ((_type) == FILE_DEVICE_CD_ROM_FILE_SYSTEM) || \
     ((_type) == FILE_DEVICE_NETWORK_FILE_SYSTEM))
#endif

//
//  Returns the number of BYTES unused in the RECORD_LIST structure.
//

#define REMAINING_NAME_SPACE(RecordList) \
    (USHORT)(RECORD_SIZE - \
            (((RecordList)->LogRecord.Length) + sizeof(LIST_ENTRY)))


//---------------------------------------------------------------------------
//      Device Extension defines
//---------------------------------------------------------------------------

typedef enum _FSPY_DEV_FLAGS {

    //
    //  If set, this is an attachment to a volume device object,
    //  If not set, this is an attachment to a file system control device
    //  object.
    //

    IsVolumeDeviceObject = 0x00000001,

    //
    //  If set, logging is turned on for this device.
    //

    LogThisDevice = 0x00000002,

    //
    //  If set, contexts are initialized.
    //

    ContextsInitialized = 0x00000004,

    //
    //  If set, this is linked into the extension list.
    //

    ExtensionIsLinked = 0x00000008,

    //
    //  If set, this is an attachment to a NTFS volume.
    //

    IsAttachedToNTFS = 0x00000010,

} FSPY_DEV_FLAGS;


//
//  Define the device extension structure that the FileSpy driver
//  adds to each device object it is attached to.  It stores
//  the context FileSpy needs to perform its logging operations on
//  a device.
//

typedef struct _FILESPY_DEVICE_EXTENSION {

    //
    //  Include all fields in NL_EXTENSION.  All these fields
    //  are used by the name lookup routines.  With this syntax
    //  we can reference NL_EXTENSION fields on a
    //  FILESPY_DEVICE_EXTENSION object directly.  For example:
    //      FILESPY_DEVICE_EXTENSION FilespyDevExt;
    //      PDEVICE_OBJECT foo;
    //      foo = FilespyDevExt->ThisDeviceObject;
    //

    NL_DEVICE_EXTENSION_HEADER NLExtHeader;

    //
    //  Linked list of devices we are attached to.
    //

    LIST_ENTRY NextFileSpyDeviceLink;

    //
    //  Flags for this device.
    //

    FSPY_DEV_FLAGS Flags;

    //
    //  Linked list of contexts associated with this volume along with the
    //  lock.
    //

    LIST_ENTRY CtxList;
    ERESOURCE CtxLock;

    //
    //  When renaming a directory there is a window where the current names
    //  in the context cache may be invalid.  To eliminate this window we
    //  increment this count every time we start doing a directory rename
    //  and decrement this count when it is completed.  When this count is
    //  non-zero then we query for the name every time so we will get a
    //  correct name for that instance in time.
    //

    ULONG AllContextsTemporary;

    //
    //  Names the user used to start logging this device.  This is
    //  used for devices where the DeviceType field of the device
    //  object is FILE_DEVICE_NETWORK_FILE_SYSTEM.  We cannot get
    //  a nice name (DOS device name, for example) for such devices,
    //  so we store the names the user has supplied and use them
    //  when constructing file names.
    //

    UNICODE_STRING UserNames;

#if (WINVER >= 0x0600 || __NDAS_FS_COMPILE_FOR_VISTA__)

    //
    //  Linked list of transaction contexts. For each enlisted transaction,
    //  we allocate a transaction context and put it into the list.
    //

    LIST_ENTRY TxListHead;
    FAST_MUTEX TxListLock;

#endif

#if __NDAS_FS__

	LFS_DEVICE_EXTENSION LfsDeviceExt;

#endif

} FILESPY_DEVICE_EXTENSION, *PFILESPY_DEVICE_EXTENSION;


#define IS_FILESPY_DEVICE_OBJECT( _devObj )                               \
    (((_devObj) != NULL) &&                                               \
     ((_devObj)->DriverObject == gFileSpyDriverObject) &&                 \
     ((_devObj)->DeviceExtension != NULL))

#if (WINVER >= 0x0600 || __NDAS_FS_COMPILE_FOR_VISTA__)

typedef struct _FILESPY_TRANSACTION_CONTEXT {

    LIST_ENTRY List;

    //
    //  Pointer to a transaction object.
    //

    PKTRANSACTION Transaction;

    //
    //  Pointer to the device object Filespy attached to the file system stack
    //

    PDEVICE_OBJECT DeviceObject;

    //
    //  Pointer to a file object bound to this transaction.
    //  Note there can be multiple file objects bound to a transaction. 
    //  This file object is the one that triggers the enlistment. 
    //

    PFILE_OBJECT FileObject;

} FILESPY_TRANSACTION_CONTEXT, *PFILESPY_TRANSACTION_CONTEXT;

#endif

#if WINVER >= 0x0501
//
//  MULTIVERSION NOTE:
//
//  If built in the Windows XP environment or later, we will dynamically import
//  the function pointers for routines that were not supported on Windows 2000
//  so that we can build a driver that will run, with modified logic, on
//  Windows 2000 or later.
//
//  Below are the prototypes for the function pointers that we need to
//  dynamically import because not all OS versions support these routines.
//

typedef
NTSTATUS
(*PSPY_REGISTER_FILE_SYSTEM_FILTER_CALLBACKS) (
    __in PDRIVER_OBJECT DriverObject,
    __in PFS_FILTER_CALLBACKS Callbacks
    );

typedef
NTSTATUS
(*PSPY_ENUMERATE_DEVICE_OBJECT_LIST) (
    __in  PDRIVER_OBJECT DriverObject,
    __out_bcount_part_opt(DeviceObjectListSize,(*ActualNumberDeviceObjects)*sizeof(PDEVICE_OBJECT)) PDEVICE_OBJECT *DeviceObjectList,
    __in  ULONG DeviceObjectListSize,
    __out PULONG ActualNumberDeviceObjects
    );

typedef
NTSTATUS
(*PSPY_ATTACH_DEVICE_TO_DEVICE_STACK_SAFE) (
    __in PDEVICE_OBJECT SourceDevice,
    __in PDEVICE_OBJECT TargetDevice,
    __out PDEVICE_OBJECT *AttachedToDeviceObject
    );

typedef
PDEVICE_OBJECT
(*PSPY_GET_LOWER_DEVICE_OBJECT) (
    __in  PDEVICE_OBJECT  DeviceObject
    );

typedef
PDEVICE_OBJECT
(*PSPY_GET_DEVICE_ATTACHMENT_BASE_REF) (
    __in PDEVICE_OBJECT DeviceObject
    );

typedef
NTSTATUS
(*PSPY_GET_STORAGE_STACK_DEVICE_OBJECT) (
    __in  PDEVICE_OBJECT  FileSystemDeviceObject,
    __out PDEVICE_OBJECT  *DiskDeviceObject
    );

typedef
PDEVICE_OBJECT
(*PSPY_GET_ATTACHED_DEVICE_REFERENCE) (
    __in PDEVICE_OBJECT DeviceObject
    );

typedef
NTSTATUS
(*PSPY_GET_VERSION) (
    __inout PRTL_OSVERSIONINFOW VersionInformation
    );

#if (WINVER >= 0x0600 || __NDAS_FS_COMPILE_FOR_VISTA__)
typedef
NTSTATUS
  (*PSPY_CREATE_TRANSACTION_MANAGER) (
    __out PHANDLE TmHandle,
    __in ACCESS_MASK DesiredAccess,
    __in_opt POBJECT_ATTRIBUTES ObjectAttributes,
    __in_opt PUNICODE_STRING LogFileName,
    __in_opt ULONG CreateOptions,
    __in_opt ULONG CommitStrength
    );

typedef
NTSTATUS
  (*PSPY_CREATE_RESOURCE_MANAGER) (
    __out PHANDLE ResourceManagerHandle,
    __in ACCESS_MASK DesiredAccess,
    __in HANDLE TmHandle,
    __in_opt LPGUID ResourceManagerGuid,
    __in_opt POBJECT_ATTRIBUTES ObjectAttributes,
    __in_opt ULONG CreateOptions,
    __in_opt PUNICODE_STRING Description
    );

typedef
NTSTATUS
  (*PSPY_ENABLE_TM_CALLBACKS) (
    __in PKRESOURCEMANAGER ResourceManager,
    __in PTM_RM_NOTIFICATION CallbackRoutine,
    __in_opt PVOID RMKey
    );

typedef
NTSTATUS
  (*PSPY_CREATE_ENLISTMENT) (
    __out PHANDLE           EnlistmentHandle,
    __in KPROCESSOR_MODE    PreviousMode,
    __in ACCESS_MASK        DesiredAccess,
    __in POBJECT_ATTRIBUTES ObjectAttributes,
    __in PRKRESOURCEMANAGER ResourceManager,
    __in PKTRANSACTION      Transaction,
    __in_opt ULONG          CreateOptions,
    __in NOTIFICATION_MASK  NotificationMask,
    __in_opt PVOID          EnlistmentKey
    );

typedef
PTXN_PARAMETER_BLOCK
  (*PSPY_GET_TRANSACTION_PARAMETER_BLOCK) (
    __in PFILE_OBJECT  FileObject
    );

typedef
NTSTATUS
  (*PSPY_COMPLETE_TX_PHASE) (
    __in PKENLISTMENT Enlistment,
    __in PLARGE_INTEGER TmVirtualClock
    );
#endif // WINVER >= 0x0600

#if __NDAS_FS__
typedef
VOID 
(*NDFAT_FSRTL_TEARDOWN_PER_STREAM_CONTEXTS) ( 
	IN PFSRTL_ADVANCED_FCB_HEADER  AdvancedHeader 
	); 
#endif

typedef struct _SPY_DYNAMIC_FUNCTION_POINTERS {

    PSPY_REGISTER_FILE_SYSTEM_FILTER_CALLBACKS RegisterFileSystemFilterCallbacks;
    PSPY_ATTACH_DEVICE_TO_DEVICE_STACK_SAFE AttachDeviceToDeviceStackSafe;
    PSPY_ENUMERATE_DEVICE_OBJECT_LIST EnumerateDeviceObjectList;
    PSPY_GET_LOWER_DEVICE_OBJECT GetLowerDeviceObject;
    PSPY_GET_DEVICE_ATTACHMENT_BASE_REF GetDeviceAttachmentBaseRef;
    PSPY_GET_STORAGE_STACK_DEVICE_OBJECT GetStorageStackDeviceObject;
    PSPY_GET_ATTACHED_DEVICE_REFERENCE GetAttachedDeviceReference;
    PSPY_GET_VERSION GetVersion;

#if (WINVER >= 0x0600 || __NDAS_FS_COMPILE_FOR_VISTA__)
    PSPY_CREATE_TRANSACTION_MANAGER CreateTransactionManager;
    PSPY_CREATE_RESOURCE_MANAGER CreateResourceManager;
    PSPY_ENABLE_TM_CALLBACKS EnableTmCallbacks;
    PSPY_CREATE_ENLISTMENT CreateEnlistment;
    PSPY_GET_TRANSACTION_PARAMETER_BLOCK GetTransactionParameterBlock;
    PSPY_COMPLETE_TX_PHASE PrePrepareComplete;
    PSPY_COMPLETE_TX_PHASE PrepareComplete;
    PSPY_COMPLETE_TX_PHASE CommitComplete;
    PSPY_COMPLETE_TX_PHASE RollbackComplete;
#endif

} SPY_DYNAMIC_FUNCTION_POINTERS, *PSPY_DYNAMIC_FUNCTION_POINTERS;

extern SPY_DYNAMIC_FUNCTION_POINTERS gSpyDynamicFunctions;

//
//  MULTIVERSION NOTE: For this version of the driver, we need to know the
//  current OS version while we are running to make decisions regarding what
//  logic to use when the logic cannot be the same for all platforms.  We
//  will look up the OS version in DriverEntry and store the values
//  in these global variables.
//

extern ULONG gSpyOsMajorVersion;
extern ULONG gSpyOsMinorVersion;

//
//  Here is what the major and minor versions should be for the various
//  OS versions:
//
//  OS Name                                 MajorVersion    MinorVersion
//  ---------------------------------------------------------------------
//  Windows 2000                             5                 0
//  Windows XP                               5                 1
//  Windows Server 2003                      5                 2
//  Windows Vista                            6                 0
//

#define IS_WINDOWSXP_OR_LATER() \
    (((gSpyOsMajorVersion == 5) && (gSpyOsMinorVersion >= 1)) || \
     (gSpyOsMajorVersion > 5))

#define IS_VISTA_OR_LATER() \
    (gSpyOsMajorVersion >= 6)

#if __NDAS_FS__

#define IS_WINDOWS2K() \
	((gSpyOsMajorVersion == 5) && (gSpyOsMinorVersion == 0))

#define IS_WINDOWSXP() \
	((gSpyOsMajorVersion == 5) && (gSpyOsMinorVersion == 1))

#define IS_WINDOWSNET() \
	((gSpyOsMajorVersion == 5) && (gSpyOsMinorVersion == 2))

#define IS_WINDOWSVISTA() \
	((gSpyOsMajorVersion == 6) && (gSpyOsMinorVersion == 0))

#define IS_WINDOWSVISTA_OR_LATER() \
	(gSpyOsMajorVersion >= 6)

#endif

#endif

//
//  Structure used to pass context information from dispatch routines to
//  completion routines for FSCTRL operations.  We need a different structures
//  for Windows 2000 from what we can use on Windows XP and later because
//  we handle the completion processing differently.
//

typedef struct _SPY_COMPLETION_CONTEXT {

    PRECORD_LIST RecordList;

} SPY_COMPLETION_CONTEXT, *PSPY_COMPLETION_CONTEXT;

typedef struct _SPY_COMPLETION_CONTEXT_W2K {

    SPY_COMPLETION_CONTEXT;

    WORK_QUEUE_ITEM WorkItem;
    PDEVICE_OBJECT DeviceObject;
    PIRP Irp;
    PDEVICE_OBJECT NewDeviceObject;

} SPY_COMPLETION_CONTEXT_W2K, *PSPY_COMPLETION_CONTEXT_W2K;

#if WINVER >= 0x0501
typedef struct _SPY_COMPLETION_CONTEXT_WXP_OR_LATER {

    SPY_COMPLETION_CONTEXT;

    KEVENT WaitEvent;

} SPY_COMPLETION_CONTEXT_WXP_OR_LATER,
  *PSPY_COMPLETION_CONTEXT_WXP_OR_LATER;
#endif

//
//  The context used to send getting the DOS device name off to a worker
//  thread.
//

typedef struct _DOS_NAME_COMPLETION_CONTEXT {

    WORK_QUEUE_ITEM WorkItem;

    //
    //  The device object to get the DOS name of.
    //

    PDEVICE_OBJECT DeviceObject;

} DOS_NAME_COMPLETION_CONTEXT, *PDOS_NAME_COMPLETION_CONTEXT;


#ifndef FORCEINLINE
#define FORCEINLINE __inline
#endif

#ifndef INLINE
#define INLINE __inline
#endif

FORCEINLINE
VOID
SpyCopyFileNameToLogRecord (
    PLOG_RECORD LogRecord,
    PUNICODE_STRING FileName
    )
/*++

Routine Description:

    Inline function to copy the file name into the log record.  The routine
    only copies as much of the file name into the log record as the log
    record allows.  Therefore, if the name is too long for the record, it will
    be truncated.  Also, the name is always NULL-terminated.

Arguments:

    LogRecord - The log record for which the name should be set.

    FileName - The file name to be set in the log record.

Return Value:

    None.

--*/
{
    //
    //  Include space for NULL when copying the name
    //

    ULONG toCopy = min( MAX_NAME_SPACE,
                        ((ULONG)FileName->Length) + sizeof(WCHAR) );

    __analysis_assume(toCopy >= sizeof(WCHAR));

    RtlCopyMemory( LogRecord->Name,
                   FileName->Buffer,
                   toCopy - sizeof(WCHAR) );

    //
    //  NULL terminate
    //

    LogRecord->Name[(toCopy/sizeof(WCHAR)) - 1] = L'\0';
    LogRecord->Length += toCopy ;
}



////////////////////////////////////////////////////////////////////////
//
//    Prototypes for the routines this driver uses to filter the
//    the data that is being seen by this file systems.
//
//                   implemented in filespy.c
//
////////////////////////////////////////////////////////////////////////

NTSTATUS
DriverEntry (
    __in PDRIVER_OBJECT DriverObject,
    __in PUNICODE_STRING RegistryPath
    );

VOID
DriverUnload (
    __in PDRIVER_OBJECT DriverObject
    );

NTSTATUS
SpyDispatch (
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp
    );

NTSTATUS
SpyPassThrough (
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp
    );

NTSTATUS
SpyPassThroughCompletion (
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp,
    __in PVOID Context
    );

NTSTATUS
SpyCreate (
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp
    );

NTSTATUS
SpyClose (
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp
    );

BOOLEAN
SpyFastIoCheckIfPossible (
    __in PFILE_OBJECT FileObject,
    __in PLARGE_INTEGER FileOffset,
    __in ULONG Length,
    __in BOOLEAN Wait,
    __in ULONG LockKey,
    __in BOOLEAN CheckForReadOperation,
    __inout PIO_STATUS_BLOCK IoStatus,
    __in PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
SpyFastIoRead (
    __in PFILE_OBJECT FileObject,
    __in PLARGE_INTEGER FileOffset,
    __in ULONG Length,
    __in BOOLEAN Wait,
    __in ULONG LockKey,
    __out_bcount(Length) PVOID Buffer,
    __inout PIO_STATUS_BLOCK IoStatus,
    __in PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
SpyFastIoWrite (
    __in PFILE_OBJECT FileObject,
    __in PLARGE_INTEGER FileOffset,
    __in ULONG Length,
    __in BOOLEAN Wait,
    __in ULONG LockKey,
    __in_bcount(Length) PVOID Buffer,
    __inout PIO_STATUS_BLOCK IoStatus,
    __in PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
SpyFastIoQueryBasicInfo (
    __in PFILE_OBJECT FileObject,
    __in BOOLEAN Wait,
    __out_bcount(sizeof(FILE_BASIC_INFORMATION)) PFILE_BASIC_INFORMATION Buffer,
    __inout PIO_STATUS_BLOCK IoStatus,
    __in PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
SpyFastIoQueryStandardInfo (
    __in PFILE_OBJECT FileObject,
    __in BOOLEAN Wait,
    __out_bcount(sizeof(FILE_STANDARD_INFORMATION)) PFILE_STANDARD_INFORMATION Buffer,
    __inout PIO_STATUS_BLOCK IoStatus,
    __in PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
SpyFastIoLock (
    __in PFILE_OBJECT FileObject,
    __in PLARGE_INTEGER FileOffset,
    __in PLARGE_INTEGER Length,
    __in PEPROCESS ProcessId,
    __in ULONG Key,
    __in BOOLEAN FailImmediately,
    __in BOOLEAN ExclusiveLock,
    __inout PIO_STATUS_BLOCK IoStatus,
    __in PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
SpyFastIoUnlockSingle (
    __in PFILE_OBJECT FileObject,
    __in PLARGE_INTEGER FileOffset,
    __in PLARGE_INTEGER Length,
    __in PEPROCESS ProcessId,
    __in ULONG Key,
    __inout PIO_STATUS_BLOCK IoStatus,
    __in PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
SpyFastIoUnlockAll (
    __in PFILE_OBJECT FileObject,
    __in PEPROCESS ProcessId,
    __inout PIO_STATUS_BLOCK IoStatus,
    __in PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
SpyFastIoUnlockAllByKey (
    __in PFILE_OBJECT FileObject,
    __in PVOID ProcessId,
    __in ULONG Key,
    __inout PIO_STATUS_BLOCK IoStatus,
    __in PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
SpyFastIoDeviceControl (
    __in PFILE_OBJECT FileObject,
    __in BOOLEAN Wait,
    __in_bcount_opt(InputBufferLength) PVOID InputBuffer,
    __in ULONG InputBufferLength,
    __out_bcount_opt(OutputBufferLength) PVOID OutputBuffer,
    __in ULONG OutputBufferLength,
    __in ULONG IoControlCode,
    __inout PIO_STATUS_BLOCK IoStatus,
    __in PDEVICE_OBJECT DeviceObject
    );

VOID
SpyFastIoDetachDevice (
    __in PDEVICE_OBJECT SourceDevice,
    __in PDEVICE_OBJECT TargetDevice
    );

BOOLEAN
SpyFastIoQueryNetworkOpenInfo (
    __in PFILE_OBJECT FileObject,
    __in BOOLEAN Wait,
    __out_bcount(sizeof(FILE_NETWORK_OPEN_INFORMATION)) PFILE_NETWORK_OPEN_INFORMATION Buffer,
    __inout PIO_STATUS_BLOCK IoStatus,
    __in PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
SpyFastIoMdlRead (
    __in PFILE_OBJECT FileObject,
    __in PLARGE_INTEGER FileOffset,
    __in ULONG Length,
    __in ULONG LockKey,
    __deref_out PMDL *MdlChain,
    __inout PIO_STATUS_BLOCK IoStatus,
    __in PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
SpyFastIoMdlReadComplete (
    __in PFILE_OBJECT FileObject,
    __in PMDL MdlChain,
    __in PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
SpyFastIoPrepareMdlWrite (
    __in PFILE_OBJECT FileObject,
    __in PLARGE_INTEGER FileOffset,
    __in ULONG Length,
    __in ULONG LockKey,
    __deref_out PMDL *MdlChain,
    __inout PIO_STATUS_BLOCK IoStatus,
    __in PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
SpyFastIoMdlWriteComplete (
    __in PFILE_OBJECT FileObject,
    __in PLARGE_INTEGER FileOffset,
    __in PMDL MdlChain,
    __in PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
SpyFastIoReadCompressed (
    __in PFILE_OBJECT FileObject,
    __in PLARGE_INTEGER FileOffset,
    __in ULONG Length,
    __in ULONG LockKey,
    __out_bcount(Length) PVOID Buffer,
    __deref_out PMDL *MdlChain,
    __inout PIO_STATUS_BLOCK IoStatus,
    __out_bcount(CompressedDataInfoLength) struct _COMPRESSED_DATA_INFO *CompressedDataInfo,
    __in ULONG CompressedDataInfoLength,
    __in PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
SpyFastIoWriteCompressed (
    __in PFILE_OBJECT FileObject,
    __in PLARGE_INTEGER FileOffset,
    __in ULONG Length,
    __in ULONG LockKey,
    __in_bcount(Length) PVOID Buffer,
    __deref_out PMDL *MdlChain,
    __inout PIO_STATUS_BLOCK IoStatus,
    __out_bcount(CompressedDataInfoLength) struct _COMPRESSED_DATA_INFO *CompressedDataInfo,
    __in ULONG CompressedDataInfoLength,
    __in PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
SpyFastIoMdlReadCompleteCompressed (
    __in PFILE_OBJECT FileObject,
    __in PMDL MdlChain,
    __in PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
SpyFastIoMdlWriteCompleteCompressed (
    __in PFILE_OBJECT FileObject,
    __in PLARGE_INTEGER FileOffset,
    __in PMDL MdlChain,
    __in PDEVICE_OBJECT DeviceObject
    );

BOOLEAN
SpyFastIoQueryOpen (
    __in PIRP Irp,
    __out_bcount(sizeof(FILE_NETWORK_OPEN_INFORMATION)) PFILE_NETWORK_OPEN_INFORMATION NetworkInformation,
    __in PDEVICE_OBJECT DeviceObject
    );

#if !__NDAS_FS__

#if WINVER >= 0x0501 /* See comment in DriverEntry */

NTSTATUS
SpyPreFsFilterOperation (
    __in PFS_FILTER_CALLBACK_DATA Data,
    __deref_out PVOID *CompletionContext
    );

VOID
SpyPostFsFilterOperation (
    __in PFS_FILTER_CALLBACK_DATA Data,
    __in NTSTATUS OperationStatus,
    __in PVOID CompletionContext
    );

#endif

#endif

#if (__NDAS_FS__ && defined _WIN64)

NTSTATUS
SpyCommonDeviceIoControl (
	IN PIRP Irp OPTIONAL,
    IN PVOID InputBuffer OPTIONAL,
    IN ULONG InputBufferLength,
    OUT PVOID OutputBuffer OPTIONAL,
    IN ULONG OutputBufferLength,
    IN ULONG IoControlCode,
    OUT PIO_STATUS_BLOCK IoStatus
    );

#else

NTSTATUS
SpyCommonDeviceIoControl (
    __in_bcount_opt(InputBufferLength) PVOID InputBuffer,
    __in ULONG InputBufferLength,
    __out_bcount_opt(OutputBufferLength) PVOID OutputBuffer,
    __in ULONG OutputBufferLength,
    __in ULONG IoControlCode,
    __inout PIO_STATUS_BLOCK IoStatus
    );

#endif

#if (WINVER >= 0x0600 || __NDAS_FS_COMPILE_FOR_VISTA__)

NTSTATUS
SpyKtmNotification (
    __in PKENLISTMENT EnlistmentObject,
    __in PVOID RmContext,
    __in PVOID TransactionContext,
    __in ULONG TransactionNotification,
    __inout PLARGE_INTEGER TmVirtualClock,
    __in ULONG ArgumentLength,
    __in_bcount(ArgumentLength) PVOID Argument
    );

#endif

//-----------------------------------------------------
//
//  These routines are only used if Filespy is attaching
//  to all volumes in the system instead of attaching to
//  volumes on demand.
//
//-----------------------------------------------------

NTSTATUS
SpyFsControl (
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp
    );

NTSTATUS
SpyFsControlMountVolume (
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp
    );

VOID
SpyFsControlMountVolumeCompleteWorker (
    __in PSPY_COMPLETION_CONTEXT_W2K Context
    );

NTSTATUS
SpyFsControlMountVolumeComplete (
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp,
    __in PDEVICE_OBJECT NewDeviceObject
    );

NTSTATUS
SpyFsControlLoadFileSystem (
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp
    );

NTSTATUS
SpyFsControlLoadFileSystemComplete (
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp
    );

VOID
SpyFsControlLoadFileSystemCompleteWorker (
    __in PSPY_COMPLETION_CONTEXT_W2K Context
    );

VOID
SpyFsNotification (
    __in PDEVICE_OBJECT DeviceObject,
    __in BOOLEAN FsActive
    );

NTSTATUS
SpyMountCompletion (
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp,
    __in PVOID Context
    );

NTSTATUS
SpyLoadFsCompletion (
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp,
    __in PVOID Context
    );

////////////////////////////////////////////////////////////////////////
//
//                  Library support routines
//                   implemented in fspylib.c
//
////////////////////////////////////////////////////////////////////////

VOID
SpyReadDriverParameters (
    __in PUNICODE_STRING RegistryPath
    );

#if WINVER >= 0x0501
VOID
SpyLoadDynamicFunctions (
    VOID
    );

VOID
SpyGetCurrentVersion (
    VOID
    );
#endif

////////////////////////////////////////////////////////////////////////
//
//                  Memory allocation routines
//                   implemented in fspylib.c
//
////////////////////////////////////////////////////////////////////////

PVOID
SpyAllocateBuffer (
    __inout_opt PLONG Counter,
    __in LONG MaxCounterValue,
    __out_opt PULONG RecordType
    );

VOID
SpyFreeBuffer (
    __in PVOID Buffer,
    __in PLONG Counter
    );

////////////////////////////////////////////////////////////////////////
//
//                      Logging routines
//                   implemented in fspylib.c
//
////////////////////////////////////////////////////////////////////////

PRECORD_LIST
SpyNewRecord (
    __in ULONG AssignedSequenceNumber
    );

VOID
SpyFreeRecord (
    __in PRECORD_LIST Record
    );

PRECORD_LIST
SpyLogFastIoStart (
    __in FASTIO_TYPE FastIoType,
    __in PDEVICE_OBJECT DeviceObject,
    __in_opt PFILE_OBJECT FileObject,
    __in_opt PLARGE_INTEGER FileOffset,
    __in_opt ULONG Length,
    __in_opt BOOLEAN Wait
    );

VOID
SpyLogFastIoComplete (
    __in_opt PIO_STATUS_BLOCK ReturnStatus,
    __in PRECORD_LIST RecordList
    );

#if WINVER >= 0x0501 /* See comment in DriverEntry */

VOID
SpyLogPreFsFilterOperation (
    __in PFS_FILTER_CALLBACK_DATA Data,
    __inout PRECORD_LIST RecordList
    );

VOID
SpyLogPostFsFilterOperation (
    __in NTSTATUS OperationStatus,
    __inout PRECORD_LIST RecordList
    );

#endif

NTSTATUS
SpyAttachDeviceToDeviceStack (
    __in PDEVICE_OBJECT SourceDevice,
    __in PDEVICE_OBJECT TargetDevice,
    __deref_out PDEVICE_OBJECT *AttachedToDeviceObject
    );

NTSTATUS
SpyLog (
    __in PRECORD_LIST NewRecord
    );

////////////////////////////////////////////////////////////////////////
//
//                    FileName cache routines
//                    implemented in fspylib.c
//
////////////////////////////////////////////////////////////////////////


NTSTATUS
SpyQueryInformationFile (
    __in PDEVICE_OBJECT NextDeviceObject,
    __in PFILE_OBJECT FileObject,
    __out_bcount_part(Length,*LengthReturned) PVOID FileInformation,
    __in ULONG Length,
    __in FILE_INFORMATION_CLASS FileInformationClass,
    __out_opt PULONG LengthReturned
    );


NTSTATUS
SpyQueryCompletion (
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp,
    __in PKEVENT SynchronizingEvent
    );

////////////////////////////////////////////////////////////////////////
//
//         Common attachment and detachment routines
//              implemented in fspylib.c
//
////////////////////////////////////////////////////////////////////////

NTSTATUS
SpyIsAttachedToDeviceByName (
    __in PNAME_CONTROL DeviceName,
    __out PBOOLEAN IsAttached,
    __deref_out PDEVICE_OBJECT *StackDeviceObject,
    __deref_out PDEVICE_OBJECT *OurAttachedDeviceObject
    );

BOOLEAN
SpyIsAttachedToDevice (
    __in PDEVICE_OBJECT DeviceObject,
    __deref_opt_out PDEVICE_OBJECT *AttachedDeviceObject
    );

BOOLEAN
SpyIsAttachedToDeviceW2K (
    __in PDEVICE_OBJECT DeviceObject,
    __deref_opt_out PDEVICE_OBJECT *AttachedDeviceObject
    );

#if WINVER >= 0x0501
BOOLEAN
SpyIsAttachedToDeviceWXPAndLater (
    __in PDEVICE_OBJECT DeviceObject,
    __deref_opt_out PDEVICE_OBJECT *AttachedDeviceObject
    );
#endif

NTSTATUS
SpyAttachToMountedDevice (
    __in PDEVICE_OBJECT DeviceObject,
    __in PDEVICE_OBJECT FilespyDeviceObject
    );

VOID
SpyCleanupMountedDevice (
    __in PDEVICE_OBJECT DeviceObject
    );

////////////////////////////////////////////////////////////////////////
//
//                 Start/stop logging routines and helper functions
//                  implemented in fspylib.c
//
////////////////////////////////////////////////////////////////////////

NTSTATUS
SpyAttachToDeviceOnDemand (
    __in PDEVICE_OBJECT DeviceObject,
    __in PNAME_CONTROL UserDeviceName,
    __deref_out PDEVICE_OBJECT *FileSpyDeviceObject
    );

NTSTATUS
SpyAttachToDeviceOnDemandW2K (
    __in PDEVICE_OBJECT DeviceObject,
    __in PNAME_CONTROL UserDeviceName,
    __deref_out PDEVICE_OBJECT *FileSpyDeviceObject
    );

#if WINVER >= 0x0501
NTSTATUS
SpyAttachToDeviceOnDemandWXPAndLater (
    __in PDEVICE_OBJECT DeviceObject,
    __in PNAME_CONTROL UserDeviceName,
    __deref_out PDEVICE_OBJECT *FileSpyDeviceObject
    );
#endif

NTSTATUS
SpyStartLoggingDevice (
    __in PCWSTR UserDeviceName
    );

NTSTATUS
SpyStopLoggingDevice (
    __in PCWSTR deviceName
    );

////////////////////////////////////////////////////////////////////////
//
//       Attaching/detaching to all volumes in system routines
//                  implemented in fspylib.c
//
////////////////////////////////////////////////////////////////////////

NTSTATUS
SpyAttachToFileSystemDevice (
    __in PDEVICE_OBJECT DeviceObject,
    __in PNAME_CONTROL Name
    );

VOID
SpyDetachFromFileSystemDevice (
    __in PDEVICE_OBJECT DeviceObject
    );

#if WINVER >= 0x0501
NTSTATUS
SpyEnumerateFileSystemVolumes (
    __in PDEVICE_OBJECT FSDeviceObject
    );
#endif

////////////////////////////////////////////////////////////////////////
//
//             Private Filespy IOCTLs helper routines
//                  implemented in fspylib.c
//
////////////////////////////////////////////////////////////////////////

NTSTATUS
SpyGetAttachList (
    __out_bcount_part(BufferSize,*ReturnLength) PVOID Buffer,
    __in ULONG BufferSize,
    __out PULONG_PTR ReturnLength
    );

VOID
SpyGetLog (
    __out_bcount(OutputBufferLength) PVOID OutputBuffer,
    __in ULONG OutputBufferLength,
    __inout PIO_STATUS_BLOCK IoStatus
    );

VOID
SpyCloseControlDevice (
    VOID
    );

////////////////////////////////////////////////////////////////////////
//
//               Device name tracking helper routines
//                  implemented in fspylib.c
//
////////////////////////////////////////////////////////////////////////

NTSTATUS
SpyGetBaseDeviceObjectName (
    __in PDEVICE_OBJECT DeviceObject,
    __inout PNAME_CONTROL Name
    );

BOOLEAN
SpyFindSubString (
    __in PUNICODE_STRING String,
    __in PUNICODE_STRING SubString
    );

VOID
SpyStoreUserName (
    __inout PFILESPY_DEVICE_EXTENSION DeviceExtension,
    __in PNAME_CONTROL UserName
    );

////////////////////////////////////////////////////////////////////////
//
//                       Debug support routines
//                       implemented in fspylib.c
//
////////////////////////////////////////////////////////////////////////

VOID
SpyDumpIrpOperation (
    __in BOOLEAN InOriginatingPath,
    __in PIRP Irp
    );

VOID
SpyDumpFastIoOperation (
    __in BOOLEAN InPreOperation,
    __in FASTIO_TYPE FastIoOperation
    );

#if WINVER >= 0x0501 /* See comment in DriverEntry */

VOID
SpyDumpFsFilterOperation (
    __in BOOLEAN InPreOperationCallback,
    __in PFS_FILTER_CALLBACK_DATA Data
    );

#endif

////////////////////////////////////////////////////////////////////////
//
//                      COMMON Naming Routines
//
//  Common named routines implemented differently between name Context
//  and name Hashing
//
////////////////////////////////////////////////////////////////////////

VOID
SpyInitNamingEnvironment (
    VOID
    );

VOID
SpyInitDeviceNamingEnvironment (
    __in PDEVICE_OBJECT DeviceObject
    );

VOID
SpyCleanupDeviceNamingEnvironment (
    __in PDEVICE_OBJECT DeviceObject
    );

VOID
SpySetName (
    __inout PRECORD_LIST RecordList,
    __in PDEVICE_OBJECT DeviceObject,
    __in_opt PFILE_OBJECT FileObject,
    __in ULONG LookupFlags,
    __in_opt PVOID Context
);

VOID
SpyNameDeleteAllNames (
    VOID
    );

VOID
SpyLogIrp (
    __in PIRP Irp,
    __inout PRECORD_LIST RecordList
    );

VOID
SpyLogIrpCompletion (
    __in PIRP Irp,
    __inout PRECORD_LIST RecordList
    );


#if USE_STREAM_CONTEXTS

////////////////////////////////////////////////////////////////////////
//
//                  Stream Context name routines
//                    implemented in fspyCtx.c
//
////////////////////////////////////////////////////////////////////////

//
//  Context specific flags
//

typedef enum _CTX_FLAGS {

    //
    //  If set, then we are currently linked into the device extension linked
    //  list.
    //

    CTXFL_InExtensionList       = 0x00000001,

    //
    //  If set, then we are linked into the stream list.  Note that there is
    //  a small period of time when we might be unlinked with this flag still
    //  set (when the file system is calling SpyDeleteContextCallback).  This is
    //  fine because we still handle not being found in the list when we do
    //  the search.  This flag handles the case when the file has been
    //  completely closed (and the memory freed) on us.
    //

    CTXFL_InStreamList          = 0x00000002,


    //
    //  If set, this is a temporary context and should not be linked into
    //  any of the context lists.  It will be freed as soon as the user is
    //  done with this operation.
    //

    CTXFL_Temporary             = 0x00000100,

    //
    //  If set, we are performing a significant operation that affects the state
    //  of this context so we should not use it.  If someone tries to get this
    //  context then create a temporary context and return it.  Cases where this
    //  occurs:
    //  - Source file of a rename.
    //  - Source file for the creation of a hardlink
    //

    CTXFL_DoNotUse              = 0x00000200

} CTX_FLAGS, *PCTX_FLAGS;

//
//  Structure for tracking an individual stream context.  Note that the buffer
//  for the FileName is allocated as part of this structure and follows
//  immediately after it.
//

typedef struct _SPY_STREAM_CONTEXT
{

    //
    //  OS Structure used to track contexts per stream.  Note how we use
    //  the following fields:
    //      OwnerID     -> Holds pointer to our DeviceExtension
    //      InstanceId  -> Holds Pointer to FsContext associated
    //                     with this structure
    //  We use these values to get back to these structures
    //

    FSRTL_PER_STREAM_CONTEXT ContextCtrl;

    //
    //  Linked list used to track contexts per device (in our device
    //  extension).
    //

    LIST_ENTRY ExtensionLink;

    //
    //  This is a counter of how many threads are currently using this
    //  context.  The count is used in this way:
    //  - It is set to 1 when it is created.
    //  - It is incremented every time it is returned to a thread
    //  - It is decremented when the thread is done with it.
    //  - It is decremented when the underlying stream that is using it is freed
    //  - The context is deleted when this count goes to zero
    //

    LONG UseCount;

    //
    //  Holds the name of the file
    //

    UNICODE_STRING Name;

    //
    //  Flags for this context.  All flags are set or cleared via
    //  the interlocked bit routines except when the entry is being
    //  created, at this time we know nobody is using this entry.
    //

    CTX_FLAGS Flags;

    //
    //  Contains the FsContext value for the stream we are attached to.  We
    //  track this so we can delete this entry at any time.
    //

    PFSRTL_ADVANCED_FCB_HEADER Stream;

} SPY_STREAM_CONTEXT, *PSPY_STREAM_CONTEXT;

//
//  Macros for locking the context lock
//

#define SpyAcquireContextLockShared(_devext) \
            SpyAcquireResourceShared( &(_devext)->CtxLock, TRUE )

#define SpyAcquireContextLockExclusive(_devext) \
            SpyAcquireResourceExclusive( &(_devext)->CtxLock, TRUE )

#define SpyReleaseContextLock(_devext) \
            SpyReleaseResource( &(_devext)->CtxLock )


VOID
SpyDeleteAllContexts (
    __in PDEVICE_OBJECT DeviceObject
    );

VOID
SpyDeleteContext (
    __in PDEVICE_OBJECT DeviceObject,
    __in PSPY_STREAM_CONTEXT pContext
    );

VOID
SpyLinkContext (
    __in PDEVICE_OBJECT DeviceObject,
    __in PFILE_OBJECT FileObject,
    __inout PSPY_STREAM_CONTEXT *ppContext
    );

NTSTATUS
SpyCreateContext (
    __in PDEVICE_OBJECT DeviceObject,
    __in PFILE_OBJECT FileObject,
    __in NAME_LOOKUP_FLAGS LookupFlags,
    __deref_out PSPY_STREAM_CONTEXT *pRetContext
    );

#define SpyFreeContext( pCtx ) \
    (ASSERT((pCtx)->UseCount == 0), \
     ExFreePool( (pCtx) ))

NTSTATUS
SpyGetContext (
    __in PDEVICE_OBJECT DeviceObject,
    __in PFILE_OBJECT pFileObject,
    __in NAME_LOOKUP_FLAGS LookupFlags,
    __deref_out PSPY_STREAM_CONTEXT *pRetContext
    );

PSPY_STREAM_CONTEXT
SpyFindExistingContext (
    __in PDEVICE_OBJECT DeviceObject,
    __in PFILE_OBJECT FileObject
    );

VOID
SpyReleaseContext (
    __in PSPY_STREAM_CONTEXT pContext
    );
#endif


#if !USE_STREAM_CONTEXTS
////////////////////////////////////////////////////////////////////////
//
//                  Name Hash support routines
//                  implemented in fspyHash.c
//
////////////////////////////////////////////////////////////////////////

typedef struct _HASH_ENTRY {

    LIST_ENTRY List;
    PFILE_OBJECT FileObject;
    UNICODE_STRING Name;

} HASH_ENTRY, *PHASH_ENTRY;


PHASH_ENTRY
SpyHashBucketLookup (
    __in PLIST_ENTRY ListHead,
    __in PFILE_OBJECT FileObject
);

VOID
SpyNameLookup (
    __in PRECORD_LIST RecordList,
    __in PFILE_OBJECT FileObject,
    __in ULONG LookupFlags,
    __in PFILESPY_DEVICE_EXTENSION DeviceExtension
    );

VOID
SpyNameDelete (
    __in PFILE_OBJECT FileObject
    );

#endif


#if (WINVER >= 0x0600 || __NDAS_FS_COMPILE_FOR_VISTA__)

////////////////////////////////////////////////////////////////////////
//
//                  KTM transaction support routines
//                  implemented in fspyTx.c
//
////////////////////////////////////////////////////////////////////////

NTSTATUS
SpyCreateKtmResourceManager (
    VOID
    );

VOID
SpyCloseKtmResourceManager (
    VOID
    );

NTSTATUS
SpyEnlistInTransaction (
    __in PKTRANSACTION Transaction,
    __in PKRESOURCEMANAGER KtmResourceManager,
    __in PDEVICE_OBJECT DeviceObject,
    __in PFILE_OBJECT FileObject
    );

NTSTATUS
SpyCheckTransaction (
    __in PIRP Irp
    );

NTSTATUS
SpyLogTransactionNotify (
    __in PFILESPY_TRANSACTION_CONTEXT txData,
    __in ULONG TransactionNotification
    );

VOID
SpyDumpTransactionNotify (
    __in PFILESPY_TRANSACTION_CONTEXT txData,
    __in ULONG TransactionNotification
    );

NTSTATUS
SpyIsAttachedToNtfs (
    __in PDEVICE_OBJECT DeviceObject,
    __out PBOOLEAN IsAttachToNtfs
    );

#endif

//
//  Include definitions
//

#include "fspydef.h"

#endif /* __FSPYKERN_H__ */

