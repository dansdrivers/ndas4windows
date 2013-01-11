/*++

Copyright (C) 2006 XIMETA, Inc.

This file defines the necessary structures, defines, and functions 
for NDAS SCSI port driver.

--*/
#ifndef _PORT_H_
#define _PORT_H_
#pragma once

#if _WIN32_WINNT <= 0x0500
#define NTSTRSAFE_LIB
#endif

#ifndef _WIN2K_COMPAT_SLIST_USAGE
#define _WIN2K_COMPAT_SLIST_USAGE
#endif /* _WIN2K_COMPAT_SLIST_USAGE */

#ifdef __cplusplus
extern "C" {
#endif

// #pragma warning (disable: 4115 4127 4200 4201 4214 4255 4619 4668 4820)

#include <ntddk.h>
#include <scsi.h>
#include <tdi.h>
#include <tdikrnl.h>
#include <ntddscsi.h>
#include <ntdddisk.h>
#include <ntddstor.h>
#include <wmistr.h>
#include <wdmguid.h>
#include <devguid.h>
#include <csq.h> 
#include <wdmsec.h>
#include <ntintsafe.h>

#include <ntstrsafe.h>
//
// WDK replaces RtlUnicodeStringCatUnicodeString with RtlUnicodeStringCat
//
#if !defined(NTDDI_VERSION)
#define RtlUnicodeStringCat RtlUnicodeStringCatUnicodeString
#endif


#include <wmilib.h>

//#include <socketlpx.h>
//#include <lspx/lsp.h>
//#include <lspx/lsp_util.h>

#include "ndasportioctl.h"
#include "ndasportguid.h"
#include "utils.h"
#include "constants.h"
#include "ndasport.h"
#include "trace.h"
#include "ndasportwmi.h"
#include "rtfunc.h"

// #pragma warning (default: 4115 4200 4214 4255 4619 4668 4820)

// #pragma warning (error: 4244 4100)

#define NDASPORT_BUS_MAJOR_VERSION 4
#define NDASPORT_BUS_MINOR_VERSION 00

extern PDRIVER_DISPATCH FdoMajorFunctionTable[];
extern PDRIVER_DISPATCH PdoMajorFunctionTable[];

extern PVOID NdasPortDriverExtensionTag;
extern PDRIVER_OBJECT NdasPortDriverObject;

extern volatile ULONG __DisableDriverLoad;
extern volatile ULONG __DisablePersistentLogicalUnits;

//
// Tag for NDASPORT
//
enum {
	NDASPORT_TAG_GENERIC          = '0pdn',
	NDASPORT_TAG_TDI              = '1pdn',
	NDASPORT_TAG_REGPATH          = '2pdn',
	NDASPORT_TAG_DEVICE_RELATIONS = '7pdn',
	NDASPORT_TAG_PNP_ID           = '8pdn',
	NDASPORT_TAG_SRB_DATA         = 'Fpdn',
	NDASPORT_TAG_REGISTRY_ENUM    = 'Rpdn',
	NDASPORT_TAG_WMI              = 'Wpdn',
	NDASPORT_TAG_PNP_EVENT        = 'Epdn',
	NDASPORT_TAG_LU_DESC          = 'Lpdn',
	NDASPORT_TAG_TDI_PNP          = '3pdn',
	NDASPORT_TAG_SENSE            = 'spdn',
};

#if _WIN32_WINNT < 0x0502
#define PVPD_IDENTIFICATION_PAGE PVOID
#define PSTORAGE_DEVICE_ID_DESCRIPTOR PVOID
#else
#endif

static CONST WCHAR NDASPORT_SERVICE_NAME[] = L"ndasport";

//
// Value for IsRemoved
//
static CONST ULONG NO_REMOVE       = 0;
static CONST ULONG REMOVE_PENDING  = 1;
static CONST ULONG REMOVE_COMPLETE = 2;

typedef struct _TDI_ADDRESS_LPX_LIST_ENTRY {
	LIST_ENTRY ListEntry;
	TDI_ADDRESS_LPX TdiAddress;
} TDI_ADDRESS_LPX_LIST_ENTRY, *PTDI_ADDRESS_LPX_LIST_ENTRY;

typedef struct _NDASPORT_DRIVER_EXTENSION {

    //
    // Pointer back to the driver object
    //
    PDRIVER_OBJECT DriverObject;

    //
    // Registry Path info for this driver
    //
    UNICODE_STRING RegistryPath;

    //
    // The bus type for this driver.
    // 
    STORAGE_BUS_TYPE BusType;

	//
	// NdasPort%d
	//
	LONG NextNdasPortNumber;

	//
	// List of FDOs
	//
	LIST_ENTRY FdoList;
	KSPIN_LOCK FdoListSpinLock;

	//
	// TDI Notification Data
	//
	HANDLE TdiNotificationHandle;
	BOOLEAN IsNetworkReady;

	LONG LpxLocalAddressListUpdateCounter;
	KSPIN_LOCK LpxLocalAddressListSpinLock;
	LIST_ENTRY LpxLocalAddressList;

} NDASPORT_DRIVER_EXTENSION, *PNDASPORT_DRIVER_EXTENSION;

typedef enum _DEVICE_PNP_STATE {

	NotStarted = 0,         // Not started yet
	Started,                // Device has received the START_DEVICE IRP
	StopPending,            // Device has received the QUERY_STOP IRP
	Stopped,                // Device has received the STOP_DEVICE IRP
	RemovePending,          // Device has received the QUERY_REMOVE IRP
	SurpriseRemovePending,  // Device has received the SURPRISE_REMOVE IRP
	Deleted,                // Device has received the REMOVE_DEVICE IRP
	UnKnown                 // Unknown state

} DEVICE_PNP_STATE;

typedef struct _NDASPORT_SRB_DATA {
	ULONG Size;
	ULONG ProcessorNumber;
	PSCSI_REQUEST_BLOCK Srb; // Back pointer to SRB
	LIST_ENTRY Link;         // ActiveSrb List Link
	PIRP OriginalIrp;
	PVOID OriginalRequest;
	PVOID OriginalBuffer;
} NDASPORT_SRB_DATA, *PNDASPORT_SRB_DATA;

C_ASSERT(ALIGN_UP(sizeof(NDASPORT_SRB_DATA), PVOID) == sizeof(NDASPORT_SRB_DATA));

typedef struct _NDASPORT_COMMON_EXTENSION {

    //
    // Back pointer to the device object
    //
    PDEVICE_OBJECT DeviceObject;

    struct {

        //
        // TRUE if the device object is a physical device object
        //
        BOOLEAN IsPdo : 1;

        //
        // TRUE if the device has been initialized
        //
        BOOLEAN IsInitialized : 1;
    };

	//
	// We track the state of the device with every PnP Irp
	// that affects the device through these two variables.
	//
	KSPIN_LOCK PowerStateSpinLock;

	DEVICE_PNP_STATE DevicePnPState;
	DEVICE_PNP_STATE PreviousPnPState;

	SYSTEM_POWER_STATE SystemPowerState;
	DEVICE_POWER_STATE DevicePowerState;

	PDEVICE_OBJECT LowerDeviceObject;

    PDRIVER_DISPATCH *MajorFunction;

	//
	// The number of IRPs sent from the bus to the underlying device object
	//
	LONG           OutstandingIO; // Biased to 1

	//
	// On remove device plug & play request we must wait until all outstanding
	// requests have been completed before we can actually delete the device
	// object. This event is when the Outstanding IO count goes to zero
	//
	KEVENT          RemoveEvent;

	//
	// This event is set when the Outstanding IO count goes to 1.
	//
	KEVENT          StopEvent;

} NDASPORT_COMMON_EXTENSION, *PNDASPORT_COMMON_EXTENSION;

C_ASSERT(ALIGN_UP(sizeof(NDASPORT_COMMON_EXTENSION), PVOID) == sizeof(NDASPORT_COMMON_EXTENSION));

typedef struct _NDASPORT_FDO_EXTENSION {

    union {
        PDEVICE_OBJECT DeviceObject;
        NDASPORT_COMMON_EXTENSION CommonExtensionHolder;
    };

	// Link point to hold all the FDOs for a driver
	LIST_ENTRY  Link;

	PNDASPORT_COMMON_EXTENSION CommonExtension;

	PDRIVER_OBJECT DriverObject;

	// Set the PDO for use with PlugPlay functions
	PDEVICE_OBJECT LowerPdo;

    // IO_SCSI_CAPABILITIES IoScsiCapabilities;

	UNICODE_STRING InterfaceName;

	BOOLEAN NdasPortSymbolicLinkCreated;
	BOOLEAN ScsiPortSymbolicLinkCreated;

	//
	// Persistent logical unit support
	//

	//
	// Number of logical units currently initializing
	//
	LONG InitChildDelegated;
	LONG InitChildActiveCount;
	LONG InitQueueCount;
	//
	// Event is set when there is no initializing logical units
	//
	KEVENT InitChildCompletionEvent;

	//
	// List of uninitialized logical units
	//
	LIST_ENTRY InitChildDescriptorList;
	FAST_MUTEX InitChildDescriptorListMutex;

	//
	// List of PDOs created so far
	//
	LIST_ENTRY ListOfPDOs;

	//
	// The PDOs currently enumerated.
	//

	ULONG NumberOfPDOs;

	ULONG NdasPortNumber;
	UCHAR PortNumber;
	UCHAR Reserved3[3];

	//
	// A synchronization for access to the device extension.
	//
	FAST_MUTEX     Mutex;

	//
	// WMI Support
	//
	BOOLEAN IsWmiRegistered;
	WMILIB_CONTEXT WmiLibInfo;

	//
	// Adapter properties
	//
	STORAGE_ADAPTER_DESCRIPTOR StorageAdapterDescriptor;
	IO_SCSI_CAPABILITIES IoScsiCapabilities;

} NDASPORT_FDO_EXTENSION, *PNDASPORT_FDO_EXTENSION;

C_ASSERT(ALIGN_UP(sizeof(NDASPORT_FDO_EXTENSION), PVOID) == sizeof(NDASPORT_FDO_EXTENSION));

typedef enum _NDASPORT_PDO_FLAGS {
	NDASPORT_PDO_FLAG_COMPLETE_ACTIVE_REQUEST = 0x00000001,
	NDASPORT_PDO_FLAG_REMOVED       = 0x00000002,
	NDASPORT_PDO_FLAG_PNP_STOPPED   = 0x00000004,
	NDASPORT_PDO_FLAG_DEVICE_FAILED = 0x00000008,
	NDASPORT_PDO_FLAG_QUEUE_LOCKED  = 0x00000010,
} NDASPORT_PDO_FLAGS;

FORCEINLINE
BOOLEAN
NdasPortpIsEqualLogicalUnitAddress(
	NDAS_LOGICALUNIT_ADDRESS Address1,
	NDAS_LOGICALUNIT_ADDRESS Address2)
{
	//
	// PortNumber is ignored on comparison
	//
	return Address1.PathId == Address2.PathId &&
		Address1.TargetId == Address2.TargetId &&
		Address1.Lun == Address2.Lun;
}

typedef struct _NDASPORT_PDO_EXTENSION {

	union {
		PDEVICE_OBJECT DeviceObject;
		NDASPORT_COMMON_EXTENSION CommonExtensionHolder;
	};

	LIST_ENTRY Link;

	PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor;
	PNDASPORT_COMMON_EXTENSION CommonExtension;

	//
	// Address of the device
	//
	// NOTE: Do not change any order of these fields.
	//       (PortNumber, PathId, TargetId, Lun)
	//
	NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress;

	PNDASPORT_FDO_EXTENSION ParentFDOExtension;

	BOOLEAN Present : 1;
	BOOLEAN ReportedMissing : 1;
	BOOLEAN IsTemporary : 1;
	BOOLEAN IsClaimed : 1;
	BOOLEAN IsDeviceRelationReported : 1;

	NPAGED_LOOKASIDE_LIST SrbDataLookasideList;
	ULONG SrbDataExtensionSize;

	NDAS_LOGICALUNIT_TYPE LogicalUnitType;
	GUID ExternalTypeGuid;
	NDAS_LOGICALUNIT_INTERFACE LogicalUnitInterface;

	// External Device Object Pointer
	PFILE_OBJECT ExternalFileObject;
	PDEVICE_OBJECT ExternalDeviceObject;

	KSPIN_LOCK ActiveSrbListSpinLock;
	LIST_ENTRY ActiveSrbListHead;

	ULONG PdoFlags;

#define NDASPORT_DPC_MAX_PROCESSORS 8

	ULONG CompletionDpcCount;
	KDPC CompletionDpc[NDASPORT_DPC_MAX_PROCESSORS];
	KSPIN_LOCK CompletionDpcSpinLock[NDASPORT_DPC_MAX_PROCESSORS];
	LIST_ENTRY CompletionSrbListHead[NDASPORT_DPC_MAX_PROCESSORS];

	PNDASPORT_LOGICALUNIT_WMILIB_CONTEXT LogicalUnitWmiInfo;
	WMILIB_CONTEXT WmiLibInfo;
	LONG WmiRegistered;

	//
	// placeholder for LogicalUnitExtension
	//
	UCHAR LogicalUnitExtensionHolder[1]; 

} NDASPORT_PDO_EXTENSION, *PNDASPORT_PDO_EXTENSION;

C_ASSERT(ALIGN_UP(sizeof(NDASPORT_PDO_EXTENSION), PVOID) == sizeof(NDASPORT_PDO_EXTENSION));

typedef struct _NDASPORT_ADAPTER_EXTENSION {

	union {
		PDEVICE_OBJECT DeviceObject;
		NDASPORT_COMMON_EXTENSION CommonExtensionHolder;
	};

	LIST_ENTRY Link;

	PNDASPORT_COMMON_EXTENSION CommonExtension;

	UCHAR PortNumber;
	UCHAR Reserved1;
	UCHAR Reserved2;
	UCHAR Reserved3;

	PNDASPORT_FDO_EXTENSION ParentFDOExtension;

} NDASPORT_ADAPTER_EXTENSION, *PNDASPORT_ADAPTER_EXTENSION;

C_ASSERT(ALIGN_UP(sizeof(NDASPORT_ADAPTER_EXTENSION), PVOID) == sizeof(NDASPORT_ADAPTER_EXTENSION));

FORCEINLINE
VOID
NpInitializeCommonExtension(
	PNDASPORT_COMMON_EXTENSION CommonExtension,
	BOOLEAN IsPdo,
	PDEVICE_OBJECT DeviceObject,
	PDEVICE_OBJECT LowerDeviceObject,
	PDRIVER_DISPATCH* DispatchFunctionTable);

FORCEINLINE
PNDASPORT_DRIVER_EXTENSION
NdasPortDriverGetExtension(PDRIVER_OBJECT DeviceObject);

FORCEINLINE
PNDASPORT_FDO_EXTENSION
NdasPortFdoGetExtension(PDEVICE_OBJECT DeviceObject);

FORCEINLINE
PNDASPORT_PDO_EXTENSION
NdasPortPdoGetExtension(PDEVICE_OBJECT DeviceObject);

FORCEINLINE
PNDAS_LOGICALUNIT_EXTENSION
NdasPortPdoGetLogicalUnitExtension(PNDASPORT_PDO_EXTENSION PdoExtension);

FORCEINLINE
PNDASPORT_PDO_EXTENSION
NdasPortLogicalUnitGetPdoExtension(
	PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension);

NTSTATUS
NdasPortDispatch(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp);

NTSTATUS
NdasPortDispatchUnsupported(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp);

NTSTATUS
NdasPortFdoAddDevice(
    __in PDRIVER_OBJECT DriverObject,
    __in PDEVICE_OBJECT PhysicalDeviceObject);

VOID
NdasPortUnload(
    __in PDRIVER_OBJECT DriverObject);

VOID
NdasPortInitializeDispatchTables();

NTSTATUS
NdasPortFdoDispatchSystemControl(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp);

NTSTATUS
NdasPortDispatchPower(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp);

NTSTATUS
NdasPortFdoDispatchDeviceControl(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp);

NTSTATUS
NdasPortFdoDispatchScsi(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp);

NTSTATUS
NdasPortFdoPnpQueryDeviceRelations(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PIRP Irp);

NTSTATUS
NdasPortFdoDispatchPnp(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp);

NTSTATUS
NdasPortFdoDispatchCreateClose(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp);

FORCEINLINE
NTSTATUS
NdasPortFdoQueryProperty(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PSTORAGE_PROPERTY_QUERY Query,
	__in ULONG InputBufferLength,
	__in ULONG OutputBufferLength,
	__out PULONG ResultLength,
	__out PSTORAGE_DESCRIPTOR_HEADER DescriptorHeader);

NTSTATUS
NdasPortPdoCreatePhysicalDeviceObject(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PNDAS_LOGICALUNIT_INTERFACE LogicalUnitInterface,
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in ULONG LogicalUnitExtensionSize,
	__in ULONG LogicalUnitSrbExtensionSize,
	__in PDEVICE_OBJECT ExternalDeviceObject,
	__in PFILE_OBJECT ExternalFileObject,
	__out PDEVICE_OBJECT* PhysicalDeviceObject);

NTSTATUS
NdasPortFdoNotifyPnpEvent(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in NDASPORT_PNP_NOTIFICATION_TYPE Type,
	__in NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress);

NTSTATUS
NdasPortPdoNotifyWmiEvent(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in NDASPORT_PNP_NOTIFICATION_TYPE Type,
	__in NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress);

NTSTATUS
NdasPortPdoDispatchDeviceControl(
    __in PDEVICE_OBJECT Pdo,
    __in PIRP Irp);

NTSTATUS
NdasPortPdoDispatchPnp(
    __in PDEVICE_OBJECT LogicalUnit,
    __in PIRP Irp);

NTSTATUS
NdasPortPdoDispatchScsi(
	__in PDEVICE_OBJECT LogicalUnit,
	__in PIRP Irp);

NTSTATUS
NdasPortPdoDispatchCreateClose(
    __in PDEVICE_OBJECT LogicalUnit,
    __in PIRP Irp);

NTSTATUS
NdasPortPdoDispatchSystemControl(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp);

NTSTATUS
NdasPortPdoQueryProperty(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in PIRP QueryIrp);

NTSTATUS
NdasPortSendIrpSynchronous(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp);

NTSTATUS
NpSetEvent(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp,
    __in PVOID Context);

NTSTATUS
NpRegisterForNetworkNotification(VOID);

VOID
NpCopyField(
    __in PSTR Destination,
    __in PCSTR Source,
    __in ULONG Count,
    __in UCHAR Change);

NTSTATUS
NdasPortPdoClaimLogicalUnit(
    __in PNDASPORT_PDO_EXTENSION PdoExtension,
    __in PIRP Irp);

VOID
NdasPortPdoStopDevice(
	__in PNDASPORT_PDO_EXTENSION PdoExtension);

VOID
NdasPortpCompleteRequestDpc(
	__in PRKDPC Dpc,
	__in PVOID DeferredContext,
	__in PVOID SystemArgument1,
	__in PVOID SystemArgument2);

NTSTATUS
NdasPortFdoDeviceControlGetLogicalUnitDescriptor(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress,
	__in PVOID OutputBuffer,
	__inout PULONG OutputBufferLength);

typedef struct _NDASPORT_LOGICALUNIT_DESCRIPTOR_ENTRY {
	LIST_ENTRY ListEntry;
	PIO_WORKITEM InitWorkItem;
	NDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor;
} NDASPORT_LOGICALUNIT_DESCRIPTOR_ENTRY, *PNDASPORT_LOGICALUNIT_DESCRIPTOR_ENTRY;

PNDASPORT_LOGICALUNIT_DESCRIPTOR_ENTRY
NdasPortpCreateLogicalUnitDescriptorListEntry(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor);

VOID
NdasPortpDeleteLogicalUnitDescriptorListEntry(
	__in PNDASPORT_LOGICALUNIT_DESCRIPTOR_ENTRY DescriptorEntry);

VOID
NdasPortpDeleteLogicalUnitDescriptorList(
	__in PLIST_ENTRY LogicalUnitDescriptorListHead);

VOID
NdasPortFdoInitializeLogicalUnits(
	__in PNDASPORT_FDO_EXTENSION FdoExtension);

VOID
NdasPortFdopInitializeLogicalUnitWorkItem(
	__in PDEVICE_OBJECT DeviceObject,
	__in PVOID Context);

typedef enum _NDASPORT_FDO_PLUGIN_FLAG {
	NDASPORT_FDO_PLUGIN_FLAG_NONE = 0x00000000,
	NDASPORT_FDO_PLUGIN_FLAG_NO_REGISTRY = 0x00000001,
} NDASPORT_FDO_PLUGIN_FLAG;

NTSTATUS
NdasPortFdoDeviceControlPlugInLogicalUnit(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PNDAS_LOGICALUNIT_DESCRIPTOR Descriptor);

NTSTATUS
NdasPortFdoDeviceControlEjectLogicalUnit(
	PNDASPORT_FDO_EXTENSION FdoExtension,
	PNDASPORT_LOGICALUNIT_EJECT Parameter);

NTSTATUS
NdasPortFdoDeviceControlUnplugLogicalUnit(
	PNDASPORT_FDO_EXTENSION FdoExtension,
	PNDASPORT_LOGICALUNIT_UNPLUG Parameter);

NTSTATUS
NdasPortFdoDeviceControlLogicalUnitAddressInUse(
	PNDASPORT_FDO_EXTENSION FdoExtension,
	NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress);

VOID
NdasPortFdoGetIoScsiCapabilities(
	__inout PIO_SCSI_CAPABILITIES Capabilities);

typedef enum _NDASPORT_FDO_REGKEY_TYPE {
	// HKLM\SYSTEM\CurrentControlSet\Enum\SYSTEM\XXXX
	NDASPORT_FDO_REGKEY_ROOT,
	// <root>\LogicalUnits
	NDASPORT_FDO_REGKEY_LOGICALUNIT_LIST
} NDASPORT_FDO_REGKEY_TYPE;

NTSTATUS
NdasPortFdoOpenRegistryKey(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in NDASPORT_FDO_REGKEY_TYPE RegKeyType,
	__in ACCESS_MASK DesiredAccess,
	__out PHANDLE KeyHandle);

typedef
NTSTATUS
NDASPORT_ENUMERATE_LOGICALUNIT_CALLBACK(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in PVOID Context);

typedef NDASPORT_ENUMERATE_LOGICALUNIT_CALLBACK* PNDASPORT_ENUMERATE_LOGICALUNIT_CALLBACK;

NTSTATUS
NdasPortFdoRegEnumerateLogicalUnits(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PNDASPORT_ENUMERATE_LOGICALUNIT_CALLBACK Callback,
	__in PVOID CallbackContext);

NTSTATUS
NdasPortFdoRegSaveLogicalUnit(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor);

NTSTATUS
NdasPortFdoRegDeleteLogicalUnit(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress);

#include "lock.h"

FORCEINLINE
VOID
NpInitializePnpState(
	__in PNDASPORT_COMMON_EXTENSION CommonExtension)
{
	CommonExtension->DevicePnPState = NotStarted;
	CommonExtension->PreviousPnPState = NotStarted;
}

FORCEINLINE
VOID
NpSetNewPnpState(
	__in PNDASPORT_COMMON_EXTENSION CommonExtension, 
	__in DEVICE_PNP_STATE NewState)
{
	CommonExtension->PreviousPnPState = CommonExtension->DevicePnPState;
	CommonExtension->DevicePnPState = NewState;
}

FORCEINLINE
VOID
NpRestorePreviousPnpState(
	__in PNDASPORT_COMMON_EXTENSION CommonExtension)
{
	CommonExtension->DevicePnPState = CommonExtension->PreviousPnPState;
}

VOID
NdasPortPdoStartIo(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp);

VOID
NdasPortPdoRemoveDevice(
	__in PNDASPORT_PDO_EXTENSION PdoExtension);

VOID
NdasPortPdoStopDevice(
	__in PNDASPORT_PDO_EXTENSION PdoExtension);

NTSTATUS
NdasPortFdoWmiDeregister(
	__in PNDASPORT_FDO_EXTENSION FdoExtension);

NTSTATUS
NdasPortFdoWmiRegister(
    __in PNDASPORT_FDO_EXTENSION FdoExtension);

NTSTATUS
NdasPortRegisterForNetworkNotification(
	__out HANDLE* BindingHandle);

NTSTATUS
NdasPortpFirePnpEvent(
	__in PDEVICE_OBJECT PhysicalDeviceObject,
	__in CONST GUID* EventGuid,
	__in_opt CONST WCHAR* EventName,
	__in_bcount_opt(CustomDataSize) PVOID CustomData,
	__in ULONG CustomDataSize);

VOID
NdasPortpFirePnpEventCompletion(
	__in PVOID Context);

NTSTATUS
NdasPortWmipQueryRegInfo(
	__in PDEVICE_OBJECT DeviceObject,
	__out PULONG RegFlags,
	__out PUNICODE_STRING InstanceName,
	__out PUNICODE_STRING *RegistryPath,
	__out PUNICODE_STRING MofResourceName,
	__out PDEVICE_OBJECT *Pdo);

NTSTATUS
NdasPortWmipQueryDataBlock(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp,
	__in ULONG GuidIndex,
	__in ULONG InstanceIndex,
	__in ULONG InstanceCount,
	__inout PULONG InstanceLengthArray,
	__in ULONG BufferAvail,
	__out PUCHAR Buffer);

NTSTATUS
NdasPortWmipSetDataBlock(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp,
	__in ULONG GuidIndex,
	__in ULONG InstanceIndex,
	__in ULONG BufferSize,
	__in PUCHAR Buffer);

NTSTATUS
NdasPortWmipSetDataItem(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp,
	__in ULONG GuidIndex,
	__in ULONG InstanceIndex,
	__in ULONG DataItemId,
	__in ULONG BufferSize,
	__in PUCHAR Buffer);

NTSTATUS
NdasPortWmipExecuteMethod(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp,
	__in ULONG GuidIndex,
	__in ULONG InstanceIndex,
	__in ULONG MethodId,
	__in ULONG InBufferSize,
	__in ULONG OutBufferSize,
	__inout PUCHAR Buffer);

NTSTATUS
NdasPortWmipFunctionControl(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp,
	__in ULONG GuidIndex,
	__in WMIENABLEDISABLECONTROL Function,
	__in BOOLEAN Enable);

FORCEINLINE
PNDASPORT_DRIVER_EXTENSION
NdasPortDriverGetExtension(PDRIVER_OBJECT DriverObject)
{
	PNDASPORT_DRIVER_EXTENSION DriverExtension;
	DriverExtension = IoGetDriverObjectExtension(
		DriverObject, NdasPortDriverExtensionTag);
	ASSERT(NULL != DriverExtension);
	return DriverExtension;
}

FORCEINLINE
PNDASPORT_PDO_EXTENSION
NdasPortPdoGetExtension(PDEVICE_OBJECT DeviceObject)
{
#if DBG
	PNDASPORT_COMMON_EXTENSION commonExtension;
	commonExtension = (PNDASPORT_COMMON_EXTENSION)DeviceObject->DeviceExtension;
	ASSERT(NULL != commonExtension);
	ASSERT(TRUE == commonExtension->IsPdo);
#endif
	return (PNDASPORT_PDO_EXTENSION) DeviceObject->DeviceExtension;
}

FORCEINLINE
PNDASPORT_FDO_EXTENSION
NdasPortFdoGetExtension(PDEVICE_OBJECT DeviceObject)
{
#if DBG
	PNDASPORT_COMMON_EXTENSION commonExtension;
	commonExtension = (PNDASPORT_COMMON_EXTENSION)DeviceObject->DeviceExtension;
	ASSERT(NULL != commonExtension);
	ASSERT(FALSE == commonExtension->IsPdo);
#endif
	return (PNDASPORT_FDO_EXTENSION) DeviceObject->DeviceExtension;
}

FORCEINLINE
PNDAS_LOGICALUNIT_EXTENSION
NdasPortPdoGetLogicalUnitExtension(PNDASPORT_PDO_EXTENSION PdoExtension)
{
	PNDAS_LOGICALUNIT_EXTENSION logicalUnitExtension;
	logicalUnitExtension = (PNDAS_LOGICALUNIT_EXTENSION) PdoExtension->LogicalUnitExtensionHolder;
	ASSERT(PdoExtension == NdasPortLogicalUnitGetPdoExtension(logicalUnitExtension));
	return logicalUnitExtension;
}

FORCEINLINE
PNDASPORT_PDO_EXTENSION
NdasPortLogicalUnitGetPdoExtension(
	PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension)
{
	PNDASPORT_PDO_EXTENSION pdoExtension;
	
	pdoExtension = CONTAINING_RECORD(
		LogicalUnitExtension,
		NDASPORT_PDO_EXTENSION, 
		LogicalUnitExtensionHolder);

#if DBG
	ASSERT((PVOID)pdoExtension->CommonExtension == (PVOID)pdoExtension);
#endif

	return pdoExtension;
}

FORCEINLINE
VOID
NpInitializeCommonExtension(
	PNDASPORT_COMMON_EXTENSION CommonExtension,
	BOOLEAN IsPdo,
	PDEVICE_OBJECT DeviceObject,
	PDEVICE_OBJECT LowerDeviceObject,
	PDRIVER_DISPATCH* DispatchFunctionTable)
{
	CommonExtension->DeviceObject = DeviceObject;
	CommonExtension->LowerDeviceObject = LowerDeviceObject;
	CommonExtension->IsPdo = IsPdo;
	CommonExtension->MajorFunction = DispatchFunctionTable;
	CommonExtension->OutstandingIO = 1;
	CommonExtension->IsInitialized; // Set this at START_DEVICE

	NpInitializePnpState(CommonExtension);

	KeInitializeSpinLock(&CommonExtension->PowerStateSpinLock);

	KeInitializeEvent(
		&CommonExtension->RemoveEvent,
		SynchronizationEvent,
		FALSE);

	KeInitializeEvent(
		&CommonExtension->StopEvent,
		SynchronizationEvent,
		FALSE);
}

// imported from wnet ddk
#ifndef RtlInitEmptyUnicodeString
#define RtlInitEmptyUnicodeString(_ucStr,_buf,_bufSize) \
	((_ucStr)->Buffer = (_buf), \
	(_ucStr)->Length = 0, \
	(_ucStr)->MaximumLength = (USHORT)(_bufSize))
#endif 

#ifdef __cplusplus
}
#endif

#endif // _PORT_H_
