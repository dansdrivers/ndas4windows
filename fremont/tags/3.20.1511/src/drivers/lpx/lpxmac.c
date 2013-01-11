/*++

Copyright (c) 1989, 1990, 1991  Microsoft Corporation

Module Name:

    nbfmac.c

Abstract:

    This module contains code which implements Mac type dependent code for
    the LPX transport.

Author:

    David Beaver (dbeaver) 1-July-1991

Environment:

    Kernel mode (Actually, unimportant)

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

UCHAR SingleRouteSourceRouting[] = { 0xc2, 0x70 };
UCHAR GeneralRouteSourceRouting[] = { 0x82, 0x70 };
ULONG DefaultSourceRoutingLength = 2;

//
// This is the interpretation of the length bits in
// the 802.5 source-routing information.
//

ULONG SR802_5Lengths[8] = {  516,  1500,  2052,  4472,
                            8144, 11407, 17800, 17800 };


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,MacInitializeMacInfo)
#pragma alloc_text(PAGE,MacSetNetBIOSMulticast)
#endif


VOID
MacInitializeMacInfo(
    IN NDIS_MEDIUM MacType,
    IN BOOLEAN UseDix,
    OUT PLPX_NDIS_IDENTIFICATION MacInfo
    )

/*++

Routine Description:

    Fills in the MacInfo table based on MacType.

Arguments:

    MacType - The MAC type we wish to decode.

    UseDix - TRUE if we should use DIX encoding on 802.3.

    MacInfo - The MacInfo structure to fill in.

Return Value:

    None.

--*/

{
    switch (MacType) {
    case NdisMedium802_3:
        MacInfo->DestinationOffset = 0;
        MacInfo->SourceOffset = 6;
        MacInfo->SourceRouting = FALSE;
        MacInfo->AddressLength = 6;
        if (UseDix) {
            MacInfo->TransferDataOffset = 3;
            MacInfo->MaxHeaderLength = 17;
            MacInfo->MediumType = NdisMediumDix;
        } else {
            MacInfo->TransferDataOffset = 0;
            MacInfo->MaxHeaderLength = 14;
            MacInfo->MediumType = NdisMedium802_3;
        }
        MacInfo->MediumAsync = FALSE;
        break;
    case NdisMedium802_5:
        MacInfo->DestinationOffset = 2;
        MacInfo->SourceOffset = 8;
        MacInfo->SourceRouting = TRUE;
        MacInfo->AddressLength = 6;
        MacInfo->TransferDataOffset = 0;
        MacInfo->MaxHeaderLength = 32;
        MacInfo->MediumType = NdisMedium802_5;
        MacInfo->MediumAsync = FALSE;
        break;
    case NdisMediumFddi:
        MacInfo->DestinationOffset = 1;
        MacInfo->SourceOffset = 7;
        MacInfo->SourceRouting = FALSE;
        MacInfo->AddressLength = 6;
        MacInfo->TransferDataOffset = 0;
        MacInfo->MaxHeaderLength = 13;
        MacInfo->MediumType = NdisMediumFddi;
        MacInfo->MediumAsync = FALSE;
        break;
    case NdisMediumWan:
        MacInfo->DestinationOffset = 0;
        MacInfo->SourceOffset = 6;
        MacInfo->SourceRouting = FALSE;
        MacInfo->AddressLength = 6;
        MacInfo->TransferDataOffset = 0;
        MacInfo->MaxHeaderLength = 14;
        MacInfo->MediumType = NdisMedium802_3;
        MacInfo->MediumAsync = TRUE;
        break;
    default:
        ASSERT(FALSE);
    }
}


VOID
MacReturnMaxDataSize(
    IN PLPX_NDIS_IDENTIFICATION MacInfo,
    IN PUCHAR SourceRouting,
    IN UINT SourceRoutingLength,
    IN UINT DeviceMaxFrameSize,
    IN BOOLEAN AssumeWorstCase,
    OUT PUINT MaxFrameSize
    )

/*++

Routine Description:

    This routine returns the space available for user data in a MAC packet.
    This will be the available space after the MAC header; all LLC and LPX
    headers will be included in this space.

Arguments:

    MacInfo - Describes the MAC we wish to decode.

    SourceRouting - If we are concerned about a reply to a specific
        frame, then this information is used.

    SourceRouting - The length of SourceRouting.

    MaxFrameSize - The maximum frame size as returned by the adapter.

    AssumeWorstCase - TRUE if we should be pessimistic.

    MaxDataSize - The maximum data size computed.

Return Value:

    None.

--*/

{
	UNREFERENCED_PARAMETER( SourceRouting );
	UNREFERENCED_PARAMETER( SourceRoutingLength );
	UNREFERENCED_PARAMETER( AssumeWorstCase );

#if __LPX__
	ASSERT( DeviceMaxFrameSize >= LPX_MAX_DATAGRAM_SIZE + sizeof(LPX_HEADER) + 14 );

	if (DeviceMaxFrameSize < LPX_MAX_DATAGRAM_SIZE + sizeof(LPX_HEADER) + 14 + 3) {

		ASSERT( FALSE );

		*MaxFrameSize = 1300;
		DebugPrint(1, ("[LPX] MacReturnMaxDataSize: CAUTION!!!!!!! DeviceMaxFrameSize=%u. Setting to 1300\n", DeviceMaxFrameSize));
		return;
	}
#endif

    switch (MacInfo->MediumType) {

    case NdisMedium802_3:

        //
        // For 802.3, we always have a 14-byte MAC header.
        //

        *MaxFrameSize = DeviceMaxFrameSize - 14;
	if(*MaxFrameSize > 1500) { 
		DebugPrint(1, ("[LPX] MacReturnMaxDataSize: MaxFrameSize is %d. Setting to 1500\n", *MaxFrameSize));
		*MaxFrameSize = 1500;
	}

#if __LPX__

		*MaxFrameSize -= sizeof(LPX_HEADER);
		*MaxFrameSize >>= 2;
		*MaxFrameSize <<= 2;
		*MaxFrameSize += sizeof(LPX_HEADER);

#endif

		break;

	default:

		*MaxFrameSize = 0;

		break;
    }
}



VOID
MacSetNetBIOSMulticast (
    IN NDIS_MEDIUM Type,
    IN PUCHAR Buffer
    )
/*++

Routine Description:

    This routine sets the NetBIOS broadcast address into a buffer provided
    by the user.

Arguments:

    Type the Mac Medium type.

    Buffer the buffer to put the multicast address in.


Return Value:

    none.

--*/
{
    switch (Type) {
    case NdisMedium802_3:
    case NdisMediumDix:
        Buffer[0] = 0x03;
        Buffer[ETHERNET_ADDRESS_LENGTH-1] = 0x01;
        break;

    case NdisMedium802_5:
        Buffer[0] = 0xc0;
        Buffer[TR_ADDRESS_LENGTH-1] = 0x80;
        break;

    case NdisMediumFddi:
        Buffer[0] = 0x03;
        Buffer[FDDI_ADDRESS_LENGTH-1] = 0x01;
        break;

    default:
        PANIC ("MacSetNetBIOSAddress: PANIC! called with unsupported Mac type.\n");
    }
}

