/*++

Module Name:

    lspkrnl.h

Abstract:

Environment:

    Kernel mode only.


Revision History:

--*/


#include <initguid.h>

//
// Since this driver is a legacy driver and gets installed as a service 
// (without an INF file),  we will define a class guid for use in 
// IoCreateDeviceSecure function. This would allow  the system to store
// Security, DeviceType, Characteristics and Exclusivity information of the
// deviceobject in the registery under 
// HKLM\SYSTEM\CurrentControlSet\Control\Class\ClassGUID\Properties. 
// This information can be overrided by an Administrators giving them the ability
// to control access to the device beyond what is initially allowed 
// by the driver developer.
//

// {82A5FDB5-D988-4818-B5B8-A593D3A032CC}
DEFINE_GUID(GUID_DEVCLASS_LSP, 
			0x82a5fdb5, 0xd988, 0x4818, 0xb5, 0xb8, 0xa5, 0x93, 0xd3, 0xa0, 0x32, 0xcc);

//
// GUID definition are required to be outside of header inclusion pragma to avoid
// error during precompiled headers.
//

#ifndef __LSPKRNL_H
#define __LSPKRNL_H

//
// GUID definition are required to be outside of header inclusion pragma to 
// avoid error during precompiled headers.
//
#include <ntddk.h>
//
// Include this header file and link with csq.lib to use this 
// sample on Win2K, XP and above.
//
#include <csq.h> 
#include <wdmsec.h> // for IoCreateDeviceSecure
#include <lspx/lsp.h>
#include <lspx/lsp_util.h>
#include <xtdi.h>

//  Debugging macros

#if DBG2
#define LSP_KDPRINT(_x_) \
	DbgPrint("LSPKRNL.SYS: ");\
	DbgPrint _x_;
#define LSP_KDPRINT_CONT(_x_) \
	DbgPrint _x_;
#else

#define LSP_KDPRINT(_x_)
#define LSP_KDPRINT_CONT(_x_)

#endif

#define LSP_DEVICE_NAME_U     L"\\Device\\Lsp0"
#define LSP_DOS_DEVICE_NAME_U L"\\DosDevices\\Lsp0"
#define LSP_RETRY_INTERVAL    500*1000 //500 ms

#include <socketlpx.h>
#include <lpxtdi.h>
#include <ntintsafe.h>

typedef struct _INPUT_DATA{

	UCHAR Data[512]; //device data is stored here

} INPUT_DATA, *PINPUT_DATA;

#define LSPDSTF_ADDRESS_CREATED     0x00000001
#define LSPDSTF_CONNECTION_CREATED  0x00000002
#define LSPDSTF_ADDRESS_ASSOCIATED  0x00000004
#define LSPDSTF_CONNECTED           0x00000008
#define LSPDSTF_LOGGED_IN           0x00000010

typedef struct _LSP_TRANSFER_DATA {
	PIRP Irp;
	XTDI_OVERLAPPED Overlapped;
	ULONG Flags;
} LSP_TRANSFER_DATA, *PLSP_TRANSFER_DATA;

__inline
VOID
LspDataReset(
	__in PDEVICE_OBJECT DeviceObject,
	__in PLSP_TRANSFER_DATA LspTransferData);

VOID
LspTransferCompletion(
	PIRP Irp,
	PXTDI_OVERLAPPED Overlapped);

NTSTATUS
LspDataInitialize(
	__in PDEVICE_OBJECT DeviceObject,
	__in PDEVICE_OBJECT ConnectionDeviceObject, 
	__inout PLSP_TRANSFER_DATA LspTransferData);

VOID
LspDataCleanup(
	__in PLSP_TRANSFER_DATA LspTransferData);

VOID
LspDataReset(
	__in PDEVICE_OBJECT DeviceObject,
	__in PLSP_TRANSFER_DATA LspTransferData);

PLSP_TRANSFER_DATA
LspDataSelect(
	__in PDEVICE_OBJECT DeviceObject);

typedef struct _DEVICE_EXTENSION{

    BOOLEAN ThreadShouldStop;
    
    // Irps waiting to be processed are queued here
    LIST_ENTRY   PendingIrpQueue;

    //  SpinLock to protect access to the queue
    KSPIN_LOCK QueueLock;

    IO_CSQ CancelSafeQueue;   

    // Time at which the device was last polled
    LARGE_INTEGER LastPollTime;  

    // Polling interval (retry interval)
    LARGE_INTEGER PollingInterval;

    KSEMAPHORE IrpQueueSemaphore;

    PETHREAD ThreadObject;

	TDI_ADDRESS_LPX LocalAddress;
	TDI_ADDRESS_LPX DeviceAddress;

	ULONG StatusFlags;
	ERESOURCE StatusFlagsResource;

	HANDLE AddressHandle;
	PFILE_OBJECT AddressFileObject;
	PDEVICE_OBJECT AddressDeviceObject;

	HANDLE ConnectionHandle;
	PFILE_OBJECT ConnectionFileObject;
	PDEVICE_OBJECT ConnectionDeviceObject;

	lsp_login_info_t LspLoginInfo;
	lsp_handle_t LspHandle;
	lsp_status_t LspStatus;
	LSP_TRANSFER_DATA LspTransferData[4];
	LONG LspTransferCount;
	KSPIN_LOCK LspLock;
	KEVENT LspCompletionEvent;

	PIRP CurrentIrp;

	PIO_WORKITEM CloseWorkItem;

	//
	// Session Buffer must be the last member of the structure
	// as the buffer extends beyond DeviceExtension
	//
	UCHAR LspSessionBuffer[1];

	//
	// Do not place any member after this!!!
	//

}  DEVICE_EXTENSION, *PDEVICE_EXTENSION;

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING registryPath
);

VOID
LspUnload(
    IN PDRIVER_OBJECT DriverObject);

VOID
LspPollingThread(
    IN PVOID Context);

NTSTATUS
LspDispatchRead(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp);

NTSTATUS
LspDispatchCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp);

NTSTATUS
LspDispatchClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp);

NTSTATUS
LspDispatchCleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp);

NTSTATUS
LspPollDevice(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp);

VOID 
LspInsertIrp(
    IN PIO_CSQ Csq,
    IN PIRP Irp);

VOID 
LspRemoveIrp(
    IN  PIO_CSQ Csq,
    IN  PIRP Irp);

PIRP 
LspPeekNextIrp(
    IN  PIO_CSQ Csq,
    IN  PIRP Irp,
    IN  PVOID PeekContext);

VOID 
LspAcquireLock(
    IN  PIO_CSQ Csq,
    OUT PKIRQL Irql);

VOID 
LspReleaseLock(
    IN PIO_CSQ Csq,
    IN KIRQL Irql);

VOID 
LspCompleteCanceledIrp(
    IN  PIO_CSQ pCsq,
    IN  PIRP Irp);

VOID
LspStartIo(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp);

NTSTATUS
pAllocateAndLockMdl(
	__in PVOID Buffer,
	__in ULONG BufferLen,
	__in BOOLEAN NonPagedPool,
	__in KPROCESSOR_MODE AccessMode,
	__in LOCK_OPERATION Operation,
	__out PMDL* Mdl);

VOID
pUnlockAndFreeMdl(
    __in PMDL Mdl);

#ifndef RTL_FIELD_TYPE
#define RTL_FIELD_TYPE(type, field) (((type*)0)->field)
#endif

#ifndef RTL_NUMBER_OF_FIELD
#define RTL_NUMBER_OF_FIELD(type, field) (RTL_NUMBER_OF(RTL_FIELD_TYPE(type, field)))
#endif

#ifndef RTL_NUMBER_OF
#define RTL_NUMBER_OF(a) (sizeof(a)/sizeof(a[0]))
#endif

#if WINVER < 0x0501
#define ExFreePoolWithTag(POINTER, TAG) ExFreePool(POINTER)
#endif

#define LSPDATA_IN_USE 0x00000001
#define LSPDATA_POOL_TAG 'ipsl'


#endif /* __LSPKRNL_H */

