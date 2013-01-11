L_CHARACTERISTICS));

    if (ndisStatus != NDIS_STATUS_SUCCESS) {
#if DBG
        IF_LPXDBG (LPX_DEBUG_RESOURCE) {
            LpxPrint1("LpxInitialize: NdisRegisterProtocol failed: %s\n",
                        LpxGetNdisStatus(ndisStatus));
        }
#endif
        return (NTSTATUS)ndisStatus;
    }

    return STATUS_SUCCESS;
}


VOID
LpxDeregisterProtocol (
    VOID
    )

/*++

Routine Description:

    This routine removes this transport to the NDIS interface.

Arguments:

    None.

Return Value:

    None.

--*/

{
    NDIS_STATUS ndisStatus;

    if (LpxNdisProtocolHandle != (NDIS_HANDLE)NULL) {
        NdisDeregisterProtocol (
            &ndisStatus,
            LpxNdisProtocolHandle);
        LpxNdisProtocolHandle = (NDIS_HANDLE)NULL;
    }
}


NDIS_STATUS
LpxSubmitNdisRequest(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PNDIS_REQUEST NdisRequest2,
    IN PNDIS_STRING AdapterString
    )

/*++

Routine Description:

    This routine passed an NDIS_REQUEST to the MAC and waits
    until it has completed before returning the final status.

Arguments:

    DeviceContext - Pointer to the device context for this driver.

    NdisRequest - Pointer to the NDIS_REQUEST to submit.

    AdapterString - The name of the adapter, in case an error needs
        to be logged.

Return Value:

    The function value is the status of the operation.

--*/
{
    NDIS_STATUS NdisStatus;
//    KIRQL   irql;
    AdapterString;
//    ASSERT( LpxControlDeviceContext != DeviceContext) ;
//    ACQUIRE_SPIN_LOCK(&DeviceContext->SpinLock, &irql);
    if (DeviceContext->NdisBindingHandle) {
        NdisRequest(
            &NdisStatus,
            DeviceContext->NdisBindingHandle,
            NdisRequest2);
//        RELEASE_SPIN_LOCK(&DeviceContext->SpinLock, irql);            
    } else {
//        RELEASE_SPIN_LOCK(&DeviceContext->SpinLock, irql);    
        NdisStatus = STATUS_INVALID_DEVICE_STATE;
    }
    
    if (NdisStatus == NDIS_STATUS_PENDING) {

        IF_LPXDBG (LPX_DEBUG_NDIS) {
            LpxPrint1 ("OID %lx pended.\n",
                NdisRequest2->DATA.QUERY_INFORMATION.Oid);
        }

        //
        // The completion routine will set NdisRequestStatus.
        //

        KeWaitForSingleObject(
            &DeviceContext->NdisRequestEvent,
            Executive,
            KernelMode,
            TRUE,
            (PLARGE_INTEGER)NULL
            );

        NdisStatus = DeviceContext->NdisRequestStatus;

        KeResetEvent(
            &DeviceContext->NdisRequestEvent
            );

    }

    if (NdisStatus == STATUS_SUCCESS) {

        IF_LPXDBG (LPX_DEBUG_NDIS) {
            if (NdisRequest2->RequestType == NdisRequestSetInformation) {
                LpxPrint1 ("Lpxdrvr: Set OID %lx succeeded.\n",
                    NdisRequest2->DATA.SET_INFORMATION.Oid);
            } else {
                LpxPrint1 ("Lpxdrvr: Query OID %lx succeeded.\n",
                    NdisRequest2->DATA.QUERY_INFORMATION.Oid);
            }
        }

    } else {
#if DBG
        if (NdisRequest2->RequestType == NdisRequestSetInformation) {
            LpxPrint2 ("Lpxdrvr: Set OID %lx failed: %s.\n",
                NdisRequest2->DATA.SET_INFORMATION.Oid, LpxGetNdisStatus(NdisStatus));
        } else {
            LpxPrint2 ("Lpxdrvr: Query OID %lx failed: %s.\n",
                NdisRequest2->DATA.QUERY_INFORMATION.Oid, LpxGetNdisStatus(NdisStatus));
        }
#endif
    }

    return NdisStatus;
}


NTSTATUS
LpxInitializeNdis (
    IN PDEVICE_CONTEXT DeviceContext,
