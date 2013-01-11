#ifndef _NDASPORT_WMI_H_
#define _NDASPORT_WMI_H_
#pragma once
#include <wmilib.h>

typedef
NTSTATUS
NDASPORT_LOGICALUNIT_WMI_QUERY_REGINFO(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out_opt PUNICODE_STRING *RegistryPath,
	__out PUNICODE_STRING MofResourceName);

typedef
NTSTATUS
NDASPORT_LOGICALUNIT_WMI_QUERY_DATABLOCK(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PIRP Irp,
	__in ULONG GuidIndex,
	__in ULONG InstanceIndex,
	__in ULONG InstanceCount,
	__inout PULONG InstanceLengthArray,
	__in ULONG BufferAvail,
	__out PUCHAR Buffer);

typedef
NTSTATUS
NDASPORT_LOGICALUNIT_WMI_SET_DATABLOCK(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PIRP Irp,
	__in ULONG GuidIndex,
	__in ULONG InstanceIndex,
	__in ULONG BufferSize,
	__in PUCHAR Buffer);

typedef
NTSTATUS
NDASPORT_LOGICALUNIT_WMI_SET_DATAITEM(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PIRP Irp,
	__in ULONG GuidIndex,
	__in ULONG InstanceIndex,
	__in ULONG DataItemId,
	__in ULONG BufferSize,
	__in PUCHAR Buffer);

typedef
NTSTATUS
NDASPORT_LOGICALUNIT_WMI_EXECUTE_METHOD(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PIRP Irp,
	__in ULONG GuidIndex,
	__in ULONG InstanceIndex,
	__in ULONG MethodId,
	__in ULONG InBufferSize,
	__in ULONG OutBufferSize,
	__inout PUCHAR Buffer);

typedef
NTSTATUS
NDASPORT_LOGICALUNIT_WMI_FUNCTION_CONTROL(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PIRP Irp,
	__in ULONG GuidIndex,
	__in WMIENABLEDISABLECONTROL Function,
	__in BOOLEAN Enable);

typedef NDASPORT_LOGICALUNIT_WMI_QUERY_REGINFO *PNDASPORT_LOGICALUNIT_WMI_QUERY_REGINFO;
typedef NDASPORT_LOGICALUNIT_WMI_SET_DATABLOCK *PNDASPORT_LOGICALUNIT_WMI_SET_DATABLOCK;
typedef NDASPORT_LOGICALUNIT_WMI_QUERY_DATABLOCK *PNDASPORT_LOGICALUNIT_WMI_QUERY_DATABLOCK;
typedef NDASPORT_LOGICALUNIT_WMI_SET_DATAITEM *PNDASPORT_LOGICALUNIT_WMI_SET_DATAITEM;
typedef NDASPORT_LOGICALUNIT_WMI_EXECUTE_METHOD *PNDASPORT_LOGICALUNIT_WMI_EXECUTE_METHOD;
typedef NDASPORT_LOGICALUNIT_WMI_FUNCTION_CONTROL *PNDASPORT_LOGICALUNIT_WMI_FUNCTION_CONTROL;

typedef struct _NDASPORT_LOGICALUNIT_WMILIB_CONTEXT {
	ULONG GuidCount;
	PWMIGUIDREGINFO GuidList;
	PNDASPORT_LOGICALUNIT_WMI_QUERY_REGINFO QueryWmiRegInfo;
	PNDASPORT_LOGICALUNIT_WMI_QUERY_DATABLOCK QueryWmiDataBlock;
	PNDASPORT_LOGICALUNIT_WMI_SET_DATABLOCK SetWmiDataBlock;
	PNDASPORT_LOGICALUNIT_WMI_SET_DATAITEM SetWmiDataItem;
	PNDASPORT_LOGICALUNIT_WMI_EXECUTE_METHOD ExecuteWmiMethod;
	PNDASPORT_LOGICALUNIT_WMI_FUNCTION_CONTROL WmiFunctionControl;
} NDASPORT_LOGICALUNIT_WMILIB_CONTEXT, *PNDASPORT_LOGICALUNIT_WMILIB_CONTEXT;

VOID
NdasPortWmiInitializeContext(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PNDASPORT_LOGICALUNIT_WMILIB_CONTEXT LogicalUnitWmiLibContext);

NTSTATUS
NdasPortWmiCompleteRequest(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PIRP Irp,
	__in NTSTATUS Status,
	__in ULONG BufferUsed,
	__in CCHAR PriorityBoost);

NTSTATUS
NdasPortWmiFireEvent(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in LPGUID Guid,
	__in ULONG InstanceIndex,
	__in ULONG EventDataSize,
	__in PVOID EventData);

#ifdef NDASPORT_IMP_SCSI_WMI

typedef struct _NDASPORT_WMI_REQUEST_CONTEXT {
	PVOID UserContext;
	ULONG BufferSize;
	PUCHAR Buffer;
	UCHAR MinorFunction;
	UCHAR ReturnStatus;
	ULONG ReturnSize;
} NDASPORT_WMI_REQUEST_CONTEXT, *PNDASPORT_WMI_REQUEST_CONTEXT;

typedef 
BOOLEAN
NDAS_LOGICALUNIT_WMI_EXECUTE_METHOD(
	__in PVOID LogicalUnitExtension,
	__in PNDASPORT_WMI_REQUEST_CONTEXT RequestContext,
	__in ULONG GuidIndex,
	__in ULONG InstanceIndex,
	__in ULONG MethodId,
	__in ULONG InBufferSize,
	__in ULONG OutBufferSize,
	__inout PUCHAR Buffer);

typedef NDAS_LOGICALUNIT_WMI_EXECUTE_METHOD *PNDAS_LOGICALUNIT_WMI_EXECUTE_METHOD;

typedef enum
{
	NdasWmiEventControl, // Enable or disable an event
	NdasWmiDataBlockControl // Enable or disable data block collection
} NDASWMI_ENABLE_DISABLE_CONTROL;

typedef
BOOLEAN
NDAS_LOGICALUNIT_WMI_FUNCTION_CONTROL(
	__in PVOID LogicalUnitExtension,
	__in PNDASPORT_WMI_REQUEST_CONTEXT RequestContext,
	__in ULONG GuidIndex,
	__in NDASWMI_ENABLE_DISABLE_CONTROL Function,
	__in BOOLEAN Enable);

typedef NDAS_LOGICALUNIT_WMI_FUNCTION_CONTROL *PNDAS_LOGICALUNIT_WMI_FUNCTION_CONTROL;

typedef
BOOLEAN
NDAS_LOGICALUNIT_WMI_QUERY_DATABLOCK(
	__in PVOID LogicalUnitExtension,
	__in PNDASPORT_WMI_REQUEST_CONTEXT DispatchContext,
	__in ULONG GuidIndex,
	__in ULONG InstanceIndex,
	__in ULONG InstanceCount,
	__inout PULONG InstanceLengthArray,
	__in ULONG BufferAvail,
	__out PUCHAR Buffer);

typedef NDAS_LOGICALUNIT_WMI_QUERY_DATABLOCK *PNDAS_LOGICALUNIT_WMI_QUERY_DATABLOCK;

typedef
UCHAR
NDAS_LOGICALUNIT_WMI_QUERY_REGINFO(
	__in PVOID LogicalUnitExtension,
	__in PNDASPORT_WMI_REQUEST_CONTEXT RequestContext,
	__out PWCHAR *MofResourceName);

typedef NDAS_LOGICALUNIT_WMI_QUERY_REGINFO *PNDAS_LOGICALUNIT_WMI_QUERY_REGINFO;

typedef
BOOLEAN
NDAS_LOGICALUNIT_WMI_SET_DATABLOCK(
	__in PVOID LogicalUnitExtension,
	__in PNDASPORT_WMI_REQUEST_CONTEXT RequestContext,
	__in ULONG GuidIndex,
	__in ULONG InstanceIndex,
	__in ULONG BufferSize,
	__in PUCHAR Buffer);

typedef NDAS_LOGICALUNIT_WMI_SET_DATABLOCK *PNDAS_LOGICALUNIT_WMI_SET_DATABLOCK;

typedef
BOOLEAN
NDAS_LOGICALUNIT_WMI_SET_DATAITEM(
	__in PVOID LogicalUnitExtension,
	__in PNDASPORT_WMI_REQUEST_CONTEXT RequestContext,
	__in ULONG GuidIndex,
	__in ULONG InstanceIndex,
	__in ULONG DataItemId,
	__in ULONG BufferSize,
	__in PUCHAR Buffer);

typedef NDAS_LOGICALUNIT_WMI_SET_DATAITEM *PNDAS_LOGICALUNIT_WMI_SET_DATAITEM;

//
// This defines a guid to be registered with WMI.
//

typedef struct _NDASPORT_WMILIB_CONTEXT
{
	//
	// WMI data block guid registration info
	//
	ULONG GuidCount;
	PWMIGUIDREGINFO GuidList;

	//
	// WMI functionality callbacks
	//
	PNDAS_LOGICALUNIT_WMI_QUERY_REGINFO QueryWmiRegInfo;
	PNDAS_LOGICALUNIT_WMI_QUERY_DATABLOCK QueryWmiDataBlock;
	PNDAS_LOGICALUNIT_WMI_SET_DATABLOCK SetWmiDataBlock;
	PNDAS_LOGICALUNIT_WMI_SET_DATAITEM SetWmiDataItem;
	PNDAS_LOGICALUNIT_WMI_EXECUTE_METHOD ExecuteWmiMethod;
	PNDAS_LOGICALUNIT_WMI_FUNCTION_CONTROL WmiFunctionControl;
} NDASPORT_WMILIB_CONTEXT, *PNDASPORT_WMILIB_CONTEXT;

BOOLEAN
NdasPortWmiDispatchFunction(
	__in PNDASPORT_WMILIB_CONTEXT WmiLibInfo,
	__in UCHAR MinorFunction,
	__in PVOID DeviceContext,
	__in PNDASPORT_WMI_REQUEST_CONTEXT RequestContext,
	__in PVOID DataPath,
	__in ULONG BufferSize,
	__in PVOID Buffer);

VOID
NdasPortWmiFireAdapterEvent(
	__in PVOID LogicalUnitExtension,
	__in LPGUID Guid,
	__in ULONG InstanceIndex,
	__in ULONG EventDataSize,
	__in PVOID EventData);

VOID
NdasPortWmiFireLogicalUnitEvent(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in UCHAR PathId,
	__in UCHAR TargetId,
	__in UCHAR Lun,
	__in LPGUID Guid,
	__in ULONG InstanceIndex,
	__in ULONG EventDataSize,
	__in PVOID EventData);

PWCHAR 
NdasPortWmiGetInstanceName (
	__in PNDASPORT_WMI_REQUEST_CONTEXT RequestContext);

ULONG 
NdasPortWmiGetReturnSize(
	__in PNDASPORT_WMI_REQUEST_CONTEXT RequestContext); 

UCHAR 
NdasPortWmiGetReturnStatus(
	__in PNDASPORT_WMI_REQUEST_CONTEXT RequestContext); 

VOID
NdasPortWmiPostProcess(
	__in PNDASPORT_WMI_REQUEST_CONTEXT RequestContext,
	__in UCHAR SrbStatus,
	__in ULONG BufferUsed);

PVOID 
NdasPortWmiSetData(
	__in PNDASPORT_WMI_REQUEST_CONTEXT RequestContext,
	__in ULONG InstanceIndex,
	__in ULONG DataLength,
	__out PULONG BufferAvail,
	__inout PULONG SizeNeeded);

BOOLEAN 
NdasPortWmiSetInstanceCount(
	__in PNDASPORT_WMI_REQUEST_CONTEXT RequestContext,
	__in ULONG InstanceCount,
	__out PULONG BufferAvail,
	__out PULONG SizeNeeded);

PWCHAR 
NdasPortWmiSetInstanceName (
	__in PNDASPORT_WMI_REQUEST_CONTEXT RequestContext,
	__in ULONG InstanceIndex,
	__in ULONG InstanceNameLength,
	__out PULONG BufferAvail,
	__inout PULONG SizeNeeded);

#endif /* NDASPORT_IMP_SCSI_WMI */

#endif /* _NDASPORT_WMI_H_ */
