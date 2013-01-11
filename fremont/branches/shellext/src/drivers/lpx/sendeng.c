/*++

Copyright (c) 1989, 1990, 1991  Microsoft Corporation

Module Name:

    sendeng.c

Abstract:

    This module contains code that implements the send engine for the
    Jetbeui transport provider.  This code is responsible for the following
    basic activities, including some subordinate glue.

    1.  Packetizing TdiSend requests already queued up on a TP_CONNECTION
        object, using I-frame packets acquired from the PACKET.C module,
        and turning them into shippable packets and placing them on the
        TP_LINK's WackQ.  In the process of doing this, the packets are
        actually submitted as I/O requests to the Physical Provider, in
        the form of PdiSend requests.

    2.  Retiring packets queued to a TP_LINK's WackQ and returning them to
        the device context's pool for use by other links.  In the process
        of retiring acked packets, step 1 may be reactivated.

    3.  Resending packets queued to a TP_LINK's WackQ because of a reject
        condition on the link.  This involves no state update in the
        TP_CONNECTION object.

    4.  Handling of Send completion events from the Physical Provider,
        to allow proper synchronization of the reuse of packets.

    5.  Completion of TdiSend requests.  This is triggered by the receipt
        (in IFRAMES.C) of a DataAck frame, or by a combination of other
        frames when the proper protocol has been negotiated.  One routine
        in this routine is responsible for the actual mechanics of TdiSend
        request completion.

Author:

    David Beaver (dbeaver) 1-July-1991

Environment:

    Kernel mode

Revision History:


--*/

#include "precomp.h"
#pragma hdrstop

#if DBG
ULONG LpxSendsIssued = 0;
ULONG LpxSendsCompletedInline = 0;
ULONG LpxSendsCompletedOk = 0;
ULONG LpxSendsCompletedFail = 0;
ULONG LpxSendsPended = 0;
ULONG LpxSendsCompletedAfterPendOk = 0;
ULONG LpxSendsCompletedAfterPendFail = 0;

ULONG LpxPacketPanic = 0;
#endif



VOID
LpxSendCompletionHandler(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET NdisPacket,
    IN NDIS_STATUS NdisStatus
    )

/*++

Routine Description:

    This routine is called by the I/O system to indicate that a connection-
    oriented packet has been shipped and is no longer needed by the Physical
    Provider.

Arguments:

    NdisContext - the value associated with the adapter binding at adapter
                  open time (which adapter we're talking on).

    NdisPacket/RequestHandle - A pointer to the NDIS_PACKET that we sent.

    NdisStatus - the completion status of the send.

Return Value:

    none.

--*/

{
	ProtocolBindingContext;  // avoid compiler warnings

#if DBG
    if (NdisStatus != NDIS_STATUS_SUCCESS) {
        LpxSendsCompletedAfterPendFail++;
        IF_LPXDBG (LPX_DEBUG_SENDENG) {
            LpxPrint2 ("LpxSendComplete: Entered for packet %p, Status %s\n",
                NdisPacket, LpxGetNdisStatus (NdisStatus));
        }
    } else {
        LpxSendsCompletedAfterPendOk++;
        IF_LPXDBG (LPX_DEBUG_SENDENG) {
            LpxPrint2 ("LpxSendComplete: Entered for packet %p, Status %s\n",
                NdisPacket, LpxGetNdisStatus (NdisStatus));
        }
    }
#endif

#if __LPX__

	LpxSendComplete( ProtocolBindingContext, NdisPacket, NdisStatus );	
	return;

#endif

} /* LpxSendCompletionHandler */
