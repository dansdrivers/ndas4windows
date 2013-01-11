/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "precomp.h"
#pragma hdrstop



VOID
MacReturnMaxDataSize(
    IN UINT DeviceMaxFrameSize,
    OUT PUINT MaxFrameSize
    )
{
    //
    // For 802.3, we always have a 14-byte MAC header.
    //
    if(DeviceMaxFrameSize <= 14) {
        DebugPrint(1, ("[LPX] MacReturnMaxDataSize: CAUTION!!!!!!! DeviceMaxFrameSize=%u. Adjusted to 1500\n", *MaxFrameSize));
        *MaxFrameSize = 1500;
    } else {
        *MaxFrameSize = DeviceMaxFrameSize - 14;
    }
    
    //
    //	should be 1500 because header size( Ethernet 14 ) is subtracted from MaxFrameSize.
    //	To allow jumbo packet, cut off 1501~1514 size to block malicious NIC drivers that reports wrong size.
    //
    if(*MaxFrameSize > 1500 && *MaxFrameSize <= 1514) {

        DebugPrint(1, ("[LPX] MacReturnMaxDataSize: NdisMedium802_3: MaxFrameSize will be adjusted: %u\n", *MaxFrameSize));
        *MaxFrameSize = 1500;
    }

    *MaxFrameSize -= sizeof(LPX_HEADER2);
    *MaxFrameSize >>= 2;
    *MaxFrameSize <<= 2;
}

