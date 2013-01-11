/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#ifndef _LPXCONFIG_
#define _LPXCONFIG_

//
// configuration structure.
//

typedef struct {

    ULONG InitConnections;
    ULONG InitAddressFiles;
    ULONG InitAddresses;
    ULONG MaxConnections;
    ULONG MaxAddressFiles;
    ULONG MaxAddresses;

    ULONG UseDixOverEthernet;

    // Lpx configurations. In ms unit.
    LONG ConnectionTimeout;
    LONG SmpTimeout;
    LONG WaitInterval;
    LONG AliveInterval;
    LONG RetransmitDelay;
    LONG MaxRetransmitDelay;
    LONG MaxAliveCount;
    LONG MaxRetransmitTime;
    

    //
    // Names contains NumAdapters pairs of NDIS adapter names (which
    // lpx binds to) and device names (which lpx exports). The nth
    // adapter name is in location n and the device name is in
    // DevicesOffset+n (DevicesOffset may be different from NumAdapters
    // if the registry Bind and Export strings are different sizes).
    //

    ULONG NumAdapters;
    ULONG DevicesOffset;
    NDIS_STRING Names[1];

} CONFIG_DATA, *PCONFIG_DATA;

#endif
