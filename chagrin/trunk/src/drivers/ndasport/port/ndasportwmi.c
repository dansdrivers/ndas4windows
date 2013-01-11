#include "port.h"


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
  __in PVOID  HwDeviceExtension,
  __in LPGUID  Guid,
  __in ULONG  InstanceIndex,
  __in ULONG  EventDataSize,
  __in PVOID  EventData);

VOID
NdasPortWmiFireLogicalUnitEvent(
    __in PVOID  HwDeviceExtension,
    __in UCHAR  PathId,
    __in UCHAR  TargetId,
    __in UCHAR  Lun,
    __in LPGUID  Guid,
    __in ULONG  InstanceIndex,
    __in ULONG  EventDataSize,
    __in PVOID  EventData)
{

}

PWCHAR 
NdasPortWmiGetInstanceName (
  __in PNDASWMI_REQUEST_CONTEXT  RequestContext)
{

}

ULONG 
NdasPortWmiGetReturnSize(
	__in PNDASWMI_REQUEST_CONTEXT  RequestContext)
{

}

UCHAR 
NdasPortWmiGetReturnStatus(
    __in PNDASWMI_REQUEST_CONTEXT  RequestContext)
{

}

VOID
NdasPortWmiPostProcess(
    __in PNDASWMI_REQUEST_CONTEXT  RequestContext,
    __in UCHAR  SrbStatus,
    __in ULONG  BufferUsed)
{

}

PVOID 
NdasPortWmiSetData(
  __in PNDASWMI_REQUEST_CONTEXT  RequestContext,
  __in ULONG  InstanceIndex,
  __in ULONG  DataLength,
  __out PULONG  BufferAvail,
  __inout PULONG  SizeNeeded)
{

}

BOOLEAN 
NdasPortWmiSetInstanceCount(
  __in PNDASWMI_REQUEST_CONTEXT  RequestContext,
  __in ULONG  InstanceCount,
  __out PULONG  BufferAvail,
  __out PULONG  SizeNeeded)
{

}

PWCHAR 
NdasPortWmiSetInstanceName (
  __in PNDASWMI_REQUEST_CONTEXT  RequestContext,
  __in ULONG  InstanceIndex,
  __in ULONG  InstanceNameLength,
  __out PULONG  BufferAvail,
  __inout PULONG  SizeNeeded)
{

}