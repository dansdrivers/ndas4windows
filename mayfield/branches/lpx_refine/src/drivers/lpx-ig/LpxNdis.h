/*++

Copyright (c) 1991  Ximeta Technology Inc

Module Name:

    LpxNdis.h

Abstract:


Author:


Environment:

    Kernel mode

Revision History:

--*/
#ifndef __LPXNDIS_H_
#define __LPXNDIS_H_



typedef struct _NET_PNP_EVENT_RESERVED {
    PWORK_QUEUE_ITEM PnPWorkItem;
    PDEVICE_CONTEXT DeviceContext;
} NET_PNP_EVENT_RESERVED, *PNET_PNP_EVENT_RESERVED;

/* 
 * 	declaration Export function to Ndis  
 */

VOID
LpxProtoOpenAdapterComplete(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN NDIS_STATUS  Status,
    IN NDIS_STATUS  OpenErrorStatus
    );

VOID 
LpxProtoCloseAdapterComplete(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN NDIS_STATUS  Status
    );

VOID
LpxProtoResetComplete(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN NDIS_STATUS  Status
    );

VOID
LpxProtoRequestComplete(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN PNDIS_REQUEST  NdisRequest,
    IN NDIS_STATUS  Status
    );

VOID
LpxProtoStatusComplete(
    IN NDIS_HANDLE  ProtocolBindingContext
    );

VOID
LpxProtoStatus(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN NDIS_STATUS  GeneralStatus,
    IN PVOID  StatusBuffer,
    IN UINT  StatusBufferSize
    );

VOID
lpx_ProcessStatusClosing(
    IN PVOID Parameter
    );

VOID
LpxProtoSendComplete(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN PNDIS_PACKET  Packet,
    IN NDIS_STATUS Status
    );

VOID
LpxProtoTransferDataComplete(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN PNDIS_PACKET  Packet,
    IN NDIS_STATUS  Status,
    IN UINT  BytesTransferred
    );

NDIS_STATUS
LpxProtoReceiveIndicate(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN NDIS_HANDLE  MacReceiveContext,
    IN PVOID  HeaderBuffer,
    IN UINT  HeaderBufferSize,
    IN PVOID  LookAheadBuffer,
    IN UINT  LookaheadBufferSize,
    IN UINT  PacketSize
    );

VOID
LpxProtoReceiveComplete(
    IN NDIS_HANDLE  ProtocolBindingContext
    );


VOID 
LpxProtoBindAdapter(
    OUT PNDIS_STATUS NdisStatus,
    IN NDIS_HANDLE  BindContext,
    IN PNDIS_STRING  DeviceName,
    IN PVOID  SystemSpecific1,
    IN PVOID  SystemSpecific2
    );

VOID
LpxProtoUnbindAdapter(
    OUT PNDIS_STATUS  NdisStatus,
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN NDIS_HANDLE  UnbindContext
    );

VOID
Lpx_ControlUnbindAdapter(
	IN	PCONTROL_DEVICE_CONTEXT DeviceContext
    );

NTSTATUS
LpxInitializeNdis (
    IN PDEVICE_CONTEXT DeviceContext,
    IN PCONFIG_DATA LpxConfig,
    IN PNDIS_STRING AdapterString
    );

VOID
MacReturnMaxDataSize(
    IN PNBF_NDIS_IDENTIFICATION MacInfo,
    IN PUCHAR SourceRouting,
    IN UINT SourceRoutingLength,
    IN UINT DeviceMaxFrameSize,
    IN BOOLEAN AssumeWorstCase,
    OUT PUINT MaxFrameSize
    );

VOID
MacSetNetBIOSMulticast (
    IN NDIS_MEDIUM Type,
    IN PUCHAR Buffer
    );

NDIS_STATUS
LpxSubmitNdisRequest(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PNDIS_REQUEST NdisRequest2,
    IN PNDIS_STRING AdapterString
    );

VOID
LpxCloseNdis (
    IN PDEVICE_CONTEXT DeviceContext
    );

NDIS_STATUS
LpxProtoPnPEventHandler(
                    IN NDIS_HANDLE ProtocolBindContext,
                    IN PNET_PNP_EVENT NetPnPEvent
                          );

VOID
LpxPnPEventDispatch(
                    IN PVOID NetPnPEvent
                   );

VOID
LpxPnPEventComplete(
                    IN PNET_PNP_EVENT   NetPnPEvent,
                    IN NTSTATUS         retVal
                   );

NTSTATUS
LpxPnPBindsComplete(
                    IN PDEVICE_CONTEXT  DeviceContext,
                    IN PNET_PNP_EVENT   NetPnPEvent
                   );

PUCHAR
lpx_GetNdisStatus(
    NDIS_STATUS GeneralStatus
    );


#endif //#ifndef __LPXNDIS_H_









