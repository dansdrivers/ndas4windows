/*++

Copyright (c) 2003  Ximeta Technology
Module Name:

    LpxCfg.h

Abstract:

  

Author:


Revision History:

--*/

#ifndef _LPXCFG_H_
#define _LPXCFG_H_

#define NBF_SUPPORTED_ADAPTERS 10

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


//
// configuration structure.
//

typedef struct {

    ULONG InitConnections;
    ULONG InitAddresses;
 
    ULONG MaxConnections;
    ULONG MaxAddresses;

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

NTSTATUS
Lpx_OpenParametersKey(
    IN HANDLE Lpx_ConfigHandle,
    OUT PHANDLE ParametersHandle
    );

VOID
Lpx_CloseParametersKey(
    IN HANDLE ParametersHandle
    );

NTSTATUS
Lpx_CountEntries(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

NTSTATUS
Lpx_AddBind(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

NTSTATUS
Lpx_AddExport(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

VOID
Lpx_ReadLinkageInformation(
    IN PWSTR RegistryPathBuffer,
    IN PCONFIG_DATA * ConfigurationInfo
    );

ULONG
Lpx_ReadSingleParameter(
    IN HANDLE ParametersHandle,
    IN PWCHAR ValueName,
    IN ULONG DefaultValue
    );

VOID
Lpx_WriteSingleParameter(
    IN HANDLE ParametersHandle,
    IN PWCHAR ValueName,
    IN ULONG ValueData
    );

UINT
Lpx_WstrLength(
    IN PWSTR Wstr
    );

NTSTATUS
Lpx_MatchBindName(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

NTSTATUS
Lpx_ExportAtIndex(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

NTSTATUS
Lpx_GetExportNameFromRegistry(
    IN  PUNICODE_STRING RegistryPath,
    IN  PUNICODE_STRING BindName,
    OUT PUNICODE_STRING ExportName
    );

VOID
Lpx_FreeConfigurationInfo (
    IN PCONFIG_DATA ConfigurationInfo
    );

NTSTATUS
Lpx_ConfigureTransport (
    IN PUNICODE_STRING RegistryPath,
    IN PCONFIG_DATA * ConfigurationInfoPtr
    );
#endif
