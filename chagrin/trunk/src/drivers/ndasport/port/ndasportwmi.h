#ifndef _NDASPORT_WMI_H_
#define _NDASPORT_WMI_H_
#pragma once

#define typedef struct _NDASWMI_REQUEST_CONTEXT {
	PVOID UserContext;
	ULONG BufferSize;
	PUCHAR Buffer;
	UCHAR MinorFunction;
	UCHAR ReturnStatus;
	ULONG ReturnSize;
} NDASWMI_REQUEST_CONTEXT, *PNDASWMI_REQUEST_CONTEXT;

typedef 
BOOLEAN
(*PNDAS_LOGICALUNIT_WMI_EXECUTE_METHOD)(
	__in PVOID LogicalUnitExtension,
	__in PNDASWMI_REQUEST_CONTEXT RequestContext,
	__in ULONG GuidIndex,
	__in ULONG InstanceIndex,
	__in ULONG MethodId,
	__in ULONG InBufferSize,
	__in ULONG OutBufferSize,
	__inout PUCHAR Buffer);

typedef enum
{
    NdasWmiEventControl,       // Enable or disable an event
    NdasWmiDataBlockControl    // Enable or disable data block collection
} NDASWMI_ENABLE_DISABLE_CONTROL;

typedef
BOOLEAN
(*PNDAS_LOGICALUNIT_WMI_FUNCTION_CONTROL)(
    __in PVOID  DeviceContext,
    __in PNDASWMI_REQUEST_CONTEXT  RequestContext,
    __in ULONG  GuidIndex,
    __in NDASWMI_ENABLE_DISABLE_CONTROL  Function,
    __in BOOLEAN  Enable);

typedef
BOOLEAN
(*PNDAS_LOGICALUNIT_WMI_QUERY_DATABLOCK)(
    __in PVOID  Context,
    __in PNDASWMI_REQUEST_CONTEXT  DispatchContext,
    __in ULONG  GuidIndex,
    __in ULONG  InstanceIndex,
    __in ULONG  InstanceCount,
    __inout PULONG  InstanceLengthArray,
    __in ULONG  BufferAvail,
    __out PUCHAR  Buffer);

typedef
UCHAR
(*PNDAS_LOGICALUNIT_WMI_QUERY_REGINFO)(
    __in PVOID  DeviceContext,
    __in PNDASWMI_REQUEST_CONTEXT  RequestContext,
    __out PWCHAR  *MofResourceName);

typedef
BOOLEAN
(*PNDAS_LOGICALUNIT_WMI_SET_DATABLOCK)(
    __in PVOID  DeviceContext,
    __in PNDASWMI_REQUEST_CONTEXT  RequestContext,
    __in ULONG  GuidIndex,
    __in ULONG  InstanceIndex,
    __in ULONG  BufferSize,
    __in PUCHAR  Buffer);

typedef
BOOLEAN
(*PNDAS_LOGICALUNIT_WMI_SET_DATAITEM)(
    __in PVOID  DeviceContext,
    __in PNDASWMI_REQUEST_CONTEXT  RequestContext,
    __in ULONG  GuidIndex,
    __in ULONG  InstanceIndex,
    __in ULONG  DataItemId,
    __in ULONG  BufferSize,
    __in PUCHAR  Buffer);

//
// This defines a guid to be registered with WMI.
//
typedef struct
{
    LPCGUID Guid;            // Guid representing data block
    ULONG InstanceCount;     // Count of Instances of Datablock
    ULONG Flags;             // Additional flags (see WMIREGINFO in wmistr.h)
} NDASWMIGUIDREGINFO, *PNDASWMIGUIDREGINFO;

typedef struct _NDAS_WMILIB_CONTEXT
{
    //
    // WMI data block guid registration info
	//
    ULONG GuidCount;
    PNDASWMIGUIDREGINFO GuidList;

    //
    // WMI functionality callbacks
    PNDAS_LOGICALUNIT_WMI_QUERY_REGINFO QueryWmiRegInfo;
    PNDAS_LOGICALUNIT_WMI_QUERY_DATABLOCK QueryWmiDataBlock;
    PNDAS_LOGICALUNIT_WMI_SET_DATABLOCK SetWmiDataBlock;
    PNDAS_LOGICALUNIT_WMI_SET_DATAITEM SetWmiDataItem;
    PNDAS_LOGICALUNIT_WMI_EXECUTE_METHOD ExecuteWmiMethod;
    PNDAS_LOGICALUNIT_WMI_FUNCTION_CONTROL WmiFunctionControl;
} NDAS_WMILIB_CONTEXT, *PNDAS_WMILIB_CONTEXT;

BOOLEAN
NdasPortWmiDispatchFunction(
    __in PNDAS_WMILIB_CONTEXT WmiLibInfo,
    __in UCHAR  MinorFunction,
    __in PVOID  DeviceContext,
    __in PNDASWMI_REQUEST_CONTEXT  RequestContext,
    __in PVOID  DataPath,
    __in ULONG  BufferSize,
    __in PVOID  Buffer);

VOID
NdasPortWmiFireAdapterEvent(
  __in PVOID  LogicalUnitExtension,
  __in LPGUID  Guid,
  __in ULONG  InstanceIndex,
  __in ULONG  EventDataSize,
  __in PVOID  EventData);

VOID
NdasPortWmiFireLogicalUnitEvent(
    __in PVOID LogicalUnitExtension,
    __in UCHAR  PathId,
    __in UCHAR  TargetId,
    __in UCHAR  Lun,
    __in LPGUID  Guid,
    __in ULONG  InstanceIndex,
    __in ULONG  EventDataSize,
    __in PVOID  EventData);

PWCHAR 
NdasPortWmiGetInstanceName (
  __in PNDASWMI_REQUEST_CONTEXT  RequestContext);

ULONG 
NdasPortWmiGetReturnSize(
	__in PNDASWMI_REQUEST_CONTEXT  RequestContext); 

UCHAR 
NdasPortWmiGetReturnStatus(
    __in PNDASWMI_REQUEST_CONTEXT  RequestContext); 

VOID
NdasPortWmiPostProcess(
    __in PNDASWMI_REQUEST_CONTEXT  RequestContext,
    __in UCHAR  SrbStatus,
    __in ULONG  BufferUsed);

PVOID 
NdasPortWmiSetData(
  __in PNDASWMI_REQUEST_CONTEXT  RequestContext,
  __in ULONG  InstanceIndex,
  __in ULONG  DataLength,
  __out PULONG  BufferAvail,
  __inout PULONG  SizeNeeded);

BOOLEAN 
NdasPortWmiSetInstanceCount(
  __in PNDASWMI_REQUEST_CONTEXT  RequestContext,
  __in ULONG  InstanceCount,
  __out PULONG  BufferAvail,
  __out PULONG  SizeNeeded);

PWCHAR 
NdasPortWmiSetInstanceName (
  __in PNDASWMI_REQUEST_CONTEXT  RequestContext,
  __in ULONG  InstanceIndex,
  __in ULONG  InstanceNameLength,
  __out PULONG  BufferAvail,
  __inout PULONG  SizeNeeded);

#endif /* _NDASPORT_WMI_H_ */
