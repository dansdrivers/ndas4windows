/*++

	Copyright (C) 2007 XIMETA, Inc. All rights reserved.

Abstract:

    This module contains the common private declarations
    for the Toaster Bus enumerator.

--*/

#include <ntddk.h>
#include <wdf.h>
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>
#include <ntintsafe.h>
#include "ndasport.h"
#include <ndas/ndasportguid.h>

#ifndef NDASPORT_FX_H
#define NDASPORT_FX_H

#define NDASPORT_MOF_RESOURCENAME L"MofResourceName"

//
// Tag for NDASPORT
//

enum {
	NDASPORT_TAG_GENERIC          = '0pdn',
	NDASPORT_TAG_TDI              = '1pdn',
	NDASPORT_TAG_PDOLIST          = '3pdn',
	NDASPORT_TAG_REGPATH          = '2pdn',
	NDASPORT_TAG_DEVICE_RELATIONS = '7pdn',
	NDASPORT_TAG_PNP_ID           = '8pdn',
	NDASPORT_TAG_SRB_DATA         = 'Fpdn',
	NDASPORT_TAG_REGISTRY_ENUM    = 'Rpdn'
};

#define DEF_STATICALLY_ENUMERATED_TOASTERS      0
#define MAX_STATICALLY_ENUMERATED_TOASTERS      10
// #define MAX_INSTANCE_ID_LEN 80

//
// Structure for reporting data to WMI
//

typedef struct _NDASPORT_WMI_STD_DATA {

    //
    // The error Count
    //
    UINT32   ErrorCount;

    //
    // Debug Print Level
    //

    UINT32  DebugPrintLevel;

} NDASPORT_WMI_STD_DATA, * PNDASPORT_WMI_STD_DATA;

//
// The goal of the identification and address description abstractions is that enough
// information is stored for a discovered device so that when it appears on the bus,
// the framework (with the help of the driver writer) can determine if it is a new or
// existing device.  The identification and address descriptions are opaque structures
// to the framework, they are private to the driver writer.  The only thing the framework
// knows about these descriptions is what their size is.
// The identification contains the bus specific information required to recognize
// an instance of a device on its the bus.  The identification information usually
// contains device IDs along with any serial or slot numbers.
// For some buses (like USB and PCI), the identification of the device is sufficient to
// address the device on the bus; in these instances there is no need for a separate
// address description.  Once reported, the identification description remains static
// for the lifetime of the device.  For example, the identification description that the
// PCI bus driver would use for a child would contain the vendor ID, device ID,
// subsystem ID, revision, and class for the device. This sample uses only identification
// description.
// On other buses (like 1394 and auto LUN SCSI), the device is assigned a dynamic
// address by the hardware (which may reassigned and updated periodically); in these
// instances the driver will use the address description to encapsulate this dynamic piece
// of data.    For example in a 1394 driver, the address description would contain the
// device's current generation count while the identification description would contain
// vendor name, model name, unit spec ID, and unit software version.
//
typedef struct _PDO_IDENTIFICATION_DESCRIPTION {
    WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER Header; // should contain this header
	NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress;
	PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor;
} PDO_IDENTIFICATION_DESCRIPTION, *PPDO_IDENTIFICATION_DESCRIPTION;

NTSTATUS
NdasPortEvtChildListIdentificationDescriptionDuplicate(
    WDFCHILDLIST DeviceList,
    PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER SourceIdentificationDescription,
    PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER DestinationIdentificationDescription
    );

BOOLEAN
NdasPortEvtChildListIdentificationDescriptionCompare(
    WDFCHILDLIST DeviceList,
    PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER FirstIdentificationDescription,
    PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER SecondIdentificationDescription
    );

VOID
NdasPortEvtChildListIdentificationDescriptionCleanup(
    WDFCHILDLIST DeviceList,
    PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER IdentificationDescription
    );

//
// This is PDO device-extension.
//

typedef struct _NDASPORT_PDO_EXTENSION
{
    NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress;

	PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor;

	NPAGED_LOOKASIDE_LIST SrbDataLookasideList;
	ULONG SrbDataExtensionSize;

	NDAS_LOGICALUNIT_TYPE LogicalUnitType;
	GUID ExternalTypeGuid;
	NDAS_LOGICALUNIT_INTERFACE LogicalUnitInterface;

	// External Device Object Pointer
	PFILE_OBJECT ExternalFileObject;
	PDEVICE_OBJECT ExternalDeviceObject;

	// incremented after NdasPortCompleteRequest calls
	PSCSI_REQUEST_BLOCK ActiveSrb;
	ULONG Flags;
	ULONG ActiveRequestNumber;
	UCHAR CompletingSrbStatus;

	//
	// placeholder for LogicalUnitExtension
	//
	UCHAR LogicalUnitExtensionHolder[1]; 

} NDASPORT_PDO_EXTENSION, *PNDASPORT_PDO_EXTENSION;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NDASPORT_PDO_EXTENSION, NdasPortPdoGetExtention)

//
// The device extension of the bus itself.  From whence the PDO's are born.
//

typedef struct _NDASPORT_FDO_EXTENSION
{
    NDASPORT_WMI_STD_DATA FdoWmiStdData;
} NDASPORT_FDO_EXTENSION, *PNDASPORT_FDO_EXTENSION;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(NDASPORT_FDO_EXTENSION, NdasPortFdoGetExtension)

//
// Prototypes of functions
//

DRIVER_INITIALIZE DriverEntry;

VOID
NdasPortEvtIoDeviceControl(
    IN WDFQUEUE Queue,
    IN WDFREQUEST Request,
    IN size_t OutputBufferLength,
    IN size_t InputBufferLength,
    IN ULONG IoControlCode
    );

NTSTATUS
NdasPortEvtDeviceAdd(
    IN WDFDRIVER Driver,
    IN PWDFDEVICE_INIT Device
    );

NTSTATUS
NdasPortPlugInDevice(
    __in WDFDEVICE Device,
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in ULONG InternalFlags);

NTSTATUS
NdasPortUnPlugDevice(
    __in WDFDEVICE Device,
	__in PNDASPORT_LOGICALUNIT_UNPLUG UnplugParam);

NTSTATUS
NdasPortEjectDevice(
    __in WDFDEVICE Device,
	__in PNDASPORT_LOGICALUNIT_EJECT EjectParam);

NTSTATUS
NdasPortEvtDeviceListCreatePdo(
    __in WDFCHILDLIST DeviceList,
    __in PWDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER IdentificationDescription,
    __in PWDFDEVICE_INIT ChildInit
    );

NTSTATUS
NdasPortCreatePdo(
    __in WDFDEVICE       Device,
    __in PWDFDEVICE_INIT ChildInit,
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor);

NTSTATUS
NdasPortDoStaticEnumeration(
    __in WDFDEVICE Device
    );

//
// Defined in wmi.c
//

NTSTATUS
NdasPortWmiRegistration(
    WDFDEVICE      Device
    );

NTSTATUS
NdasPortEvtStdDataSetItem(
    IN  WDFWMIINSTANCE WmiInstance,
    IN  ULONG DataItemId,
    IN  ULONG InBufferSize,
    IN  PVOID InBuffer
    );

NTSTATUS
NdasPortEvtStdDataSetInstance(
    IN  WDFWMIINSTANCE WmiInstance,
    IN  ULONG InBufferSize,
    IN  PVOID InBuffer
    );

NTSTATUS
NdasPortEvtStdDataQueryInstance(
    IN  WDFWMIINSTANCE WmiInstance,
    IN  ULONG OutBufferSize,
    IN  PVOID OutBuffer,
    OUT PULONG BufferUsed
    );

VOID
NdasPortPdoEvtIoScsi(
	__in WDFQUEUE Queue,
	__in WDFREQUEST Request,
	__in size_t OutputBufferLength,
	__in size_t InputBufferLength,
	__in ULONG IoControlCode);

VOID
NdasPortPdoEvtIoDeviceControl(
	__in WDFQUEUE Queue,
	__in WDFREQUEST Request,
	__in size_t OutputBufferLength,
	__in size_t InputBufferLength,
	__in ULONG IoControlCode);

VOID
NdasPortPdoEvtCleanup(
	__in WDFDEVICE Device);

#endif

