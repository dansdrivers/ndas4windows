/*++

Copyright (C) 2002-2007 XIMETA, Inc.
All rights reserved.

Private include file for the LPX (NetBIOS Frames Protocol) transport. This
file defines all constants and structures necessary for support of
the dynamic configuration of LPX. Note that this file will be replaced
by calls to the configuration manager over time.

--*/

#ifndef _LPXCONFIG_
#define _LPXCONFIG_

#if 0
//
// Define the devices we support; this is in leiu of a real configuration
// manager.
//

#define LPX_SUPPORTED_ADAPTERS 10

#define NE3200_ADAPTER_NAME L"\\Device\\NE320001"
#define ELNKII_ADAPTER_NAME L"\\Device\\Elnkii"   // adapter we will talk to
#define ELNKMC_ADAPTER_NAME L"\\Device\\Elnkmc01"
#define ELNK16_ADAPTER_NAME L"\\Device\\Elnk1601"
#define SONIC_ADAPTER_NAME L"\\Device\\Sonic01"
#define LANCE_ADAPTER_NAME L"\\Device\\Lance01"
#define PC586_ADAPTER_NAME L"\\Device\\Pc586"
#define IBMTOK_ADAPTER_NAME L"\\Device\\Ibmtok01"
#define PROTEON_ADAPTER_NAME L"\\Device\\Proteon01"
#define WDLAN_ADAPTER_NAME L"\\Device\\Wdlan01"

#endif

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
    ULONG MaxMemoryUsage;

    ULONG UseDixOverEthernet;
    ULONG QueryWithoutSourceRouting;
    ULONG AllRoutesNameRecognized;
    ULONG MinimumSendWindowLimit;

    //
    // Names contains NumAdapters pairs of NDIS adapter names (which
    // nbf binds to) and device names (which nbf exports). The nth
    // adapter name is in location n and the device name is in
    // DevicesOffset+n (DevicesOffset may be different from NumAdapters
    // if the registry Bind and Export strings are different sizes).
    //

    ULONG NumAdapters;
    ULONG DevicesOffset;
    NDIS_STRING Names[1];

} CONFIG_DATA, *PCONFIG_DATA;

#endif
