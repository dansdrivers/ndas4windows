/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/


#include "precomp.h"
#pragma hdrstop



//
// This is a one-per-driver variable used in binding
// to the NDIS interface.
//

NDIS_HANDLE LpxNdisProtocolHandle = (NDIS_HANDLE)NULL;


NDIS_STATUS
LpxSubmitNdisRequest(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PNDIS_REQUEST NdisRequest,
    IN PNDIS_STRING AdapterName
    );

VOID
LpxOpenAdapterComplete (
    IN NDIS_HANDLE BindingContext,
    IN NDIS_STATUS NdisStatus,
    IN NDIS_STATUS OpenErrorStatus
    );

VOID
LpxCloseAdapterComplete(
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_STATUS Status
    );

VOID
LpxResetComplete(
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_STATUS Status
    );

VOID
LpxRequestComplete (
    IN NDIS_HANDLE BindingContext,
    IN PNDIS_REQUEST NdisRequest,
    IN NDIS_STATUS NdisStatus
    );

VOID
LpxStatusIndication (
    IN NDIS_HANDLE NdisBindingContext,
    IN NDIS_STATUS NdisStatus,
    IN PVOID StatusBuffer,
    IN UINT StatusBufferLength
    );

VOID
LpxProcessStatusClosing(
    IN PDEV