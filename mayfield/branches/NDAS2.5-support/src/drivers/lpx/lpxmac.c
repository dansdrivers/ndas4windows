/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "precomp.h"
#pragma hdrstop


NTSTATUS
LpxReturnMaxDataSize(
    IN UINT DeviceMaxFrameSize,
    OUT PUINT MaxFrameSize,    
    OUT PUINT MaxJumboFrameSize
    )
{
	UINT FrameSize;
    //
    // For 802.3, we always have a 14-byte MAC header.
    //
    if(DeviceMaxFrameSize <= 14) {
        FrameSize = 1300;
        DebugPrint(1, ("[LPX] LpxReturnMaxDataSize: CAUTION!!!!!!! DeviceMaxFrameSize=%u. Setting to 1300\n", DeviceMaxFrameSize));
        //
		// To do: re-scan MTU again if scanned MTU value is invalid.
		//
    } else {
        FrameSize = DeviceMaxFrameSize - 14;
    }

	// Set jumbo frame size
    if (FrameSize > NDAS_JUMBO_FRAME_LENGTH) {
		*MaxJumboFrameSize = NDAS_JUMBO_FRAME_LENGTH;
	    *MaxJumboFrameSize -= sizeof(LPX_HEADER2);
	    // Packet should be 16 byte aligned as of NDAS 2.5
	    // So round down to 16 bytes boundary.
	    *MaxJumboFrameSize >>= 4; 
	    *MaxJumboFrameSize <<= 4;		
    } else {
		*MaxJumboFrameSize = 0; // Jumbo frame is not available
   	}
	DebugPrint(1, ("[LPX] LpxReturnMaxDataSize: Setting Jumbo frame size to %d\n", *MaxJumboFrameSize));

	// Limit frame size with default or registry value
	if (LpxMaximumTransferUnit>0) {
	    if (FrameSize > (UINT)LpxMaximumTransferUnit) { // default LpxMaximumTransferUnit is 1500
	       	FrameSize = LpxMaximumTransferUnit;
			DebugPrint(1, ("[LPX] LpxReturnMaxDataSize: Setting Max frame size to configuration value %d\n", FrameSize));
	    }
	}
	
    if(FrameSize > 1488) { // *MaxFrameSize is for non-jumbo packet.
    	FrameSize = 1488;
    }

    FrameSize -= sizeof(LPX_HEADER2);
    // Packet should be 16 byte aligned as of NDAS 2.5
    // So round down to 16 bytes boundary.
    FrameSize >>= 4; 
    FrameSize <<= 4;
	*MaxFrameSize = FrameSize;

	DebugPrint(0, ("[LPX] LpxReturnMaxDataSize: MTU=%u\n", *MaxFrameSize));

    return STATUS_SUCCESS;
}

