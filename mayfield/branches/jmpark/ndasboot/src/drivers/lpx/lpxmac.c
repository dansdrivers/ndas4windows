/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "precomp.h"
#pragma hdrstop



NTSTATUS
MacReturnMaxDataSize(
    IN UINT DeviceMaxFrameSize,
    OUT PUINT MaxFrameSize
    )
{
    //
    // For 802.3, we always have a 14-byte MAC header.
    //
    if(DeviceMaxFrameSize <= 14) {
        *MaxFrameSize = 1300;
        DebugPrint(1, ("[LPX] MacReturnMaxDataSize: CAUTION!!!!!!! DeviceMaxFrameSize=%u. Setting to 1300\n", DeviceMaxFrameSize));
        //
		// To do: re-scan MTU again if scanned MTU value is invalid.
		//
    } else {
        *MaxFrameSize = DeviceMaxFrameSize - 14;
    }
    
#if 0
    //
    //	should be 1500 because header size( Ethernet 14 ) is subtracted from MaxFrameSize.
    //	To allow jumbo packet, cut off 1501~1514 size to block malicious NIC drivers that reports wrong size.
    //
    if(*MaxFrameSize > 1500 && *MaxFrameSize <= 1514) {

        DebugPrint(1, ("[LPX] MacReturnMaxDataSize: NdisMedium802_3: MaxFrameSize will be adjusted: %u\n", *MaxFrameSize));
        *MaxFrameSize = 1500;
    }
#endif

    if (*MaxFrameSize > (UINT)LpxMaximumTransferUnit) { // default LpxMaximumTransferUnit is 1500
       *MaxFrameSize = LpxMaximumTransferUnit;
        DebugPrint(1, ("[LPX] MacReturnMaxDataSize: Setting Max frame size to configuration value %d\n", *MaxFrameSize));
    }

    if(*MaxFrameSize > 1500) { // Currently lpx does not support jumbo frame
        DebugPrint(1, ("[LPX] MacReturnMaxDataSize: NdisMedium802_3: MaxFrameSize will be adjusted: %u\n", *MaxFrameSize));    
        *MaxFrameSize = 1500;
    }

    *MaxFrameSize -= sizeof(LPX_HEADER2);
    *MaxFrameSize >>= 2;
    *MaxFrameSize <<= 2;

    return STATUS_SUCCESS;
}

