/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#ifndef _MAC_
#define _MAC_

//
// MAC-specific definitions, some of which get used below
//

#define ETHERNET_ADDRESS_LENGTH        6
#define ETHERNET_PACKET_LENGTH      1514  // max size of an ethernet packet
#define ETHERNET_HEADER_LENGTH        14  // size of the ethernet MAC header


NTSTATUS
MacReturnMaxDataSize(
    IN UINT DeviceMaxFrameSize,
    OUT PUINT MaxFrameSize
    );

#endif // ifdef _MAC_

