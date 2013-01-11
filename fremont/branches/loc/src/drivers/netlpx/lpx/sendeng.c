/*++

Copyright (C) 2002-2007 XIMETA, Inc.
All rights reserved.

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
