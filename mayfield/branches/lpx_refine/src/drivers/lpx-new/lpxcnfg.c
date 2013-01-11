/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "precomp.h"
#pragma hdrstop

//
// Local functions used to access the registry.
//

VOID
LpxFreeConfigurationInfo (
    IN PCONFIG_DATA ConfigurationInfo
    );

NTSTATUS
LpxOpenParametersKey(
    IN HANDLE LpxConfigHandle,
    OUT PHANDLE ParametersHandle
    );

VOID
LpxCloseParametersKey(
    IN HANDLE ParametersHandle
    );

NTSTATUS
LpxCountEntries(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

NTSTATUS
LpxAddBind(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

NTSTATUS
LpxAddExport(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

VOID
LpxReadLinkageInformation(
    IN PWSTR RegistryPathBuffer,
    IN PCONFIG_DATA * ConfigurationInfo
    );

ULONG
LpxReadSingleParameter(
    IN HANDLE ParametersHandle,
    IN PWCHAR ValueName,
    IN ULONG DefaultValue
    );

VOID
LpxWriteSingleParameter(
    IN HANDLE ParametersHandle,
    IN PWCHAR ValueName,
    IN ULONG ValueData
    );

UINT
LpxWstrLength(
    IN PWSTR Wstr
    );

NTSTATUS
LpxMatchBindName(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );

NTSTATUS
LpxExportAtIndex(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    );


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,LpxWstrLength)
#pragma alloc_text(PAGE,LpxConfigureTransport)
#pragma alloc_text(PAGE,LpxFreeConfigurationInfo)
#pragma alloc_text(PAGE,LpxOpenParametersKey)
#pragma alloc_text(PAGE,LpxCloseParametersKey)
#pragma alloc_text(PAGE,LpxCountEntries)
#pragma alloc_text(PAGE,LpxAddBind)
#pragma alloc_text(PAGE,LpxAddExport)
#pragma alloc_text(PAGE,LpxReadLinkageInformation)
#pragma alloc_text(PAGE,LpxReadSingleParameter)
#pragma alloc_text(PAGE,LpxWriteSingleParameter)
#endif

UINT
LpxWstrLength(
    IN PWSTR Wstr
    )
{
    UINT Length = 0;
    while (*Wstr++) {
        Length += sizeof(WCHAR);
    }
    return Length;
}

#define InsertAdapter(ConfigurationInfo, Subscript, Name)                \
{ \
    PWSTR _S; \
    PWSTR _N = (Name); \
    UINT _L = LpxWstrLength(_N)+sizeof(WCHAR); \
    _S = (PWSTR)ExAllocatePoolWithTag(NonPagedPool, _L, LPX_MEM_TAG_DEVICE_EXPORT); \
    if (_S != NULL) { \
        RtlCopyMemory(_S, _N, _L); \
        RtlInitUnicodeString (&(ConfigurationInfo)->Names[Subscript], _S); \
    } \
}

#define InsertDevice(ConfigurationInfo, Subscript, Name)                \
{ \
    PWSTR _S; \
    PWSTR _N = (Name); \
    UINT _L = LpxWstrLength(_N)+sizeof(WCHAR); \
    _S = (PWSTR)ExAllocatePoolWithTag(NonPagedPool, _L, LPX_MEM_TAG_DEVICE_EXPORT); \
    if (_S != NULL) { \
        RtlCopyMemory(_S, _N, _L); \
        RtlInitUnicodeString (&(ConfigurationInfo)->Names[(ConfigurationInfo)->DevicesOffset+Subscript], _S); \
    } \
}


#define RemoveAdapter(ConfigurationInfo, Subscript)                \
    ExFreePool ((ConfigurationInfo)->Names[Subscript].Buffer)

#define RemoveDevice(ConfigurationInfo, Subscript)                \
    ExFreePool ((ConfigurationInfo)->Names[(ConfigurationInfo)->DevicesOffset+Subscript].Buffer)



//
// These strings are used in various places by the registry.
//

#define DECLARE_STRING(_str_) WCHAR Str ## _str_[] = L#_str_


#define READ_HIDDEN_CONFIG(_Field) \
{ \
    ConfigurationInfo->_Field = \
        LpxReadSingleParameter( \
             ParametersHandle, \
             Str ## _Field, \
             ConfigurationInfo->_Field); \
}

#define WRITE_HIDDEN_CONFIG(_Field) \
{ \
    LpxWriteSingleParameter( \
        ParametersHandle, \
        Str ## _Field, \
        ConfigurationInfo->_Field); \
}



NTSTATUS
LpxConfigureTransport (
    IN PUNICODE_STRING RegistryPath,
    IN PCONFIG_DATA * ConfigurationInfoPtr
    )
/*++

Routine Description:

    This routine is called by LPX to get information from the configuration
    management routines. We read the registry, starting at RegistryPath,
    to get the parameters. If they don't exist, we use the defaults
    set in lpxcnfg.h file.

Arguments:

    RegistryPath - The name of LPX's node in the registry.

    ConfigurationInfoPtr - A pointer to the configuration information structure.

Return Value:

    Status - STATUS_SUCCESS if everything OK, STATUS_INSUFFICIENT_RESOURCES
            otherwise.

--*/
{

    NTSTATUS OpenStatus;
    HANDLE ParametersHandle;
    HANDLE LpxConfigHandle;
    NTSTATUS Status;
    ULONG Disposition;
    PWSTR RegistryPathBuffer;
    OBJECT_ATTRIBUTES TmpObjectAttributes;
    PCONFIG_DATA ConfigurationInfo;

     DECLARE_STRING(InitConnections);
    DECLARE_STRING(InitAddressFiles);
    DECLARE_STRING(InitAddresses);

    DECLARE_STRING(MaxConnections);
    DECLARE_STRING(MaxAddressFiles);
    DECLARE_STRING(MaxAddresses);

    DECLARE_STRING(InitPackets);
    DECLARE_STRING(InitReceivePackets);
    DECLARE_STRING(InitReceiveBuffers);

    DECLARE_STRING(SendPacketPoolSize);
    DECLARE_STRING(ReceivePacketPoolSize);
 
     DECLARE_STRING(GeneralRetries);
    DECLARE_STRING(GeneralTimeout);
 
    DECLARE_STRING(UseDixOverEthernet);

    // LPX specific configurations
    DECLARE_STRING(ConnectionTimeout);
    DECLARE_STRING(SmpTimeout);
    DECLARE_STRING(WaitInterval);
    DECLARE_STRING(AliveInterval);
    DECLARE_STRING(RetransmitDelay);
    DECLARE_STRING(MaxRetransmitDelay);
    DECLARE_STRING(MaxAliveCount);
    DECLARE_STRING(MaxRetransmitTime);
       
    //
    // Open the registry.
    //

    InitializeObjectAttributes(
        &TmpObjectAttributes,
        RegistryPath,               // name
        OBJ_CASE_INSENSITIVE,       // attributes
        NULL,                       // root
        NULL                        // security descriptor
        );

    Status = ZwCreateKey(
                 &LpxConfigHandle,
                 KEY_WRITE,
                 &TmpObjectAttributes,
                 0,                 // title index
                 NULL,              // class
                 0,                 // create options
                 &Disposition);     // disposition

    if (!NT_SUCCESS(Status)) {
        LpxPrint1("LPX: Could not open/create LPX key: %lx\n", Status);
        return Status;
    }

    IF_LPXDBG (LPX_DEBUG_REGISTRY) {
        LpxPrint2("%s LPX key: %lx\n",
            (Disposition == REG_CREATED_NEW_KEY) ? "created" : "opened",
            LpxConfigHandle);
    }


    OpenStatus = LpxOpenParametersKey (LpxConfigHandle, &ParametersHandle);

    if (OpenStatus != STATUS_SUCCESS) {
        return OpenStatus;
    }

    //
    // Read in the NDIS binding information (if none is present
    // the array will be filled with all known drivers).
    //
    // LpxReadLinkageInformation expects a null-terminated path,
    // so we have to create one from the UNICODE_STRING.
    //

    RegistryPathBuffer = (PWSTR)ExAllocatePoolWithTag(
                                    NonPagedPool,
                                    RegistryPath->Length + sizeof(WCHAR),
                                    LPX_MEM_TAG_REGISTRY_PATH);
    if (RegistryPathBuffer == NULL) {
        LpxCloseParametersKey (ParametersHandle);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlCopyMemory (RegistryPathBuffer, RegistryPath->Buffer, RegistryPath->Length);
    *(PWCHAR)(((PUCHAR)RegistryPathBuffer)+RegistryPath->Length) = (WCHAR)'\0';

    LpxReadLinkageInformation (RegistryPathBuffer, ConfigurationInfoPtr);

    if (*ConfigurationInfoPtr == NULL) {
        ExFreePool (RegistryPathBuffer);
        LpxCloseParametersKey (ParametersHandle);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    ConfigurationInfo = *ConfigurationInfoPtr;


    //
    // Configure the initial values for some LPX resources.
    //

#if 1
    ConfigurationInfo->InitConnections = 2;
    ConfigurationInfo->InitAddressFiles = 0;
    ConfigurationInfo->InitAddresses = 0;
#else
    ConfigurationInfo->InitConnections = 10;
    ConfigurationInfo->InitAddressFiles = 10;
    ConfigurationInfo->InitAddresses = 10;
#endif
    ConfigurationInfo->ConnectionTimeout = DEFAULT_CONNECTION_TIMEOUT;
    ConfigurationInfo->SmpTimeout = DEFAULT_SMP_TIMEOUT;
    ConfigurationInfo->WaitInterval = DEFAULT_TIME_WAIT_INTERVAL;
    ConfigurationInfo->AliveInterval = DEFAULT_ALIVE_INTERVAL;
    ConfigurationInfo->RetransmitDelay = DEFAULT_RETRANSMIT_DELAY;
    ConfigurationInfo->MaxRetransmitDelay = DEFAULT_MAX_RETRANSMIT_DELAY;

#if 1
    ConfigurationInfo->MaxAliveCount = DEFAULT_MAX_ALIVE_COUNT;
    ConfigurationInfo->MaxRetransmitTime = DEFAULT_MAX_RETRANSMIT_TIME;
#else
    ConfigurationInfo->MaxAliveCount = 2;
    ConfigurationInfo->MaxRetransmitTime = 8000;
#endif
    //
    // Now initialize the timeout etc. values.
    //
    
  
    ConfigurationInfo->UseDixOverEthernet = 0;
 

    //
    // Now read the optional "hidden" parameters; if these do
    // not exist then the current values are used. Note that
    // the current values will be 0 unless they have been
    // explicitly initialized above.
    //
    // NOTE: These macros expect "ConfigurationInfo" and
    // "ParametersHandle" to exist when they are expanded.
    //

    READ_HIDDEN_CONFIG (InitConnections);
    READ_HIDDEN_CONFIG (InitAddressFiles);
    READ_HIDDEN_CONFIG (InitAddresses);

    READ_HIDDEN_CONFIG (MaxConnections);
    READ_HIDDEN_CONFIG (MaxAddressFiles);
    READ_HIDDEN_CONFIG (MaxAddresses);

    READ_HIDDEN_CONFIG (UseDixOverEthernet);

    READ_HIDDEN_CONFIG(ConnectionTimeout);
    READ_HIDDEN_CONFIG(SmpTimeout);
    READ_HIDDEN_CONFIG(WaitInterval);
    READ_HIDDEN_CONFIG(AliveInterval);
    READ_HIDDEN_CONFIG(RetransmitDelay);
    READ_HIDDEN_CONFIG(MaxRetransmitDelay);
    READ_HIDDEN_CONFIG(MaxAliveCount);
    READ_HIDDEN_CONFIG(MaxRetransmitTime);
    

    LpxConnectionTimeout = MSEC_TO_HZ(ConfigurationInfo->ConnectionTimeout);
    LpxSmpTimeout = MSEC_TO_HZ(ConfigurationInfo->SmpTimeout);
    LpxWaitInterval = MSEC_TO_HZ(ConfigurationInfo->WaitInterval);
    LpxAliveInterval = MSEC_TO_HZ(ConfigurationInfo->AliveInterval);
    LpxRetransmitDelay = MSEC_TO_HZ(ConfigurationInfo->RetransmitDelay);
    LpxMaxRetransmitDelay = MSEC_TO_HZ(ConfigurationInfo->MaxRetransmitDelay);
    LpxMaxAliveCount = ConfigurationInfo->MaxAliveCount;
    LpxMaxRetransmitTime = MSEC_TO_HZ(ConfigurationInfo->MaxRetransmitTime);

 
    //
    // Save the "hidden" parameters, these may not exist in
    // the registry.
    //
    // NOTE: These macros expect "ConfigurationInfo" and
    // "ParametersHandle" to exist when they are expanded.
    //

    //
    // 5/22/92 - don't write the parameters that are set
    // based on Size, since otherwise these will overwrite
    // those values since hidden parameters are set up
    // after the Size-based configuration is done.
    //

//    WRITE_HIDDEN_CONFIG (MaxConnections);
//    WRITE_HIDDEN_CONFIG (MaxAddressFiles);
//    WRITE_HIDDEN_CONFIG (MaxAddresses);

 
 
//    WRITE_HIDDEN_CONFIG (UseDixOverEthernet);
 
    // ZwFlushKey (ParametersHandle);

    ExFreePool (RegistryPathBuffer);
    LpxCloseParametersKey (ParametersHandle);
    ZwClose (LpxConfigHandle);

    return STATUS_SUCCESS;

}   /* LpxConfigureTransport */


VOID
LpxFreeConfigurationInfo (
    IN PCONFIG_DATA ConfigurationInfo
    )

/*++

Routine Description:

    This routine is called by LPX to get free any storage that was allocated
    by LpxConfigureTransport in producing the specified CONFIG_DATA structure.

Arguments:

    ConfigurationInfo - A pointer to the configuration information structure.

Return Value:

    None.

--*/
{
    UINT i;

    for (i=0; i<ConfigurationInfo->NumAdapters; i++) {
        RemoveAdapter (ConfigurationInfo, i);
        RemoveDevice (ConfigurationInfo, i);
    }
    ExFreePool (ConfigurationInfo);

}   /* LpxFreeConfigurationInfo */


NTSTATUS
LpxOpenParametersKey(
    IN HANDLE LpxConfigHandle,
    OUT PHANDLE ParametersHandle
    )

/*++

Routine Description:

    This routine is called by LPX to open the LPX "Parameters" key.

Arguments:

    ParametersHandle - Returns the handle used to read parameters.

Return Value:

    The status of the request.

--*/
{

    NTSTATUS Status;
    HANDLE ParamHandle;
    PWSTR ParametersString = L"Parameters";
    UNICODE_STRING ParametersKeyName;
    OBJECT_ATTRIBUTES TmpObjectAttributes;

    //
    // Open the LPX parameters key.
    //

    RtlInitUnicodeString (&ParametersKeyName, ParametersString);

    InitializeObjectAttributes(
        &TmpObjectAttributes,
        &ParametersKeyName,         // name
        OBJ_CASE_INSENSITIVE,       // attributes
        LpxConfigHandle,            // root
        NULL                        // security descriptor
        );


    Status = ZwOpenKey(
                 &ParamHandle,
                 KEY_READ,
                 &TmpObjectAttributes);

    if (!NT_SUCCESS(Status)) {

        LpxPrint1("Could not open parameters key: %lx\n", Status);
        return Status;

    }

    IF_LPXDBG (LPX_DEBUG_REGISTRY) {
        LpxPrint1("Opened parameters key: %lx\n", ParamHandle);
    }


    *ParametersHandle = ParamHandle;


    //
    // All keys successfully opened or created.
    //

    return STATUS_SUCCESS;

}   /* LpxOpenParametersKey */

VOID
LpxCloseParametersKey(
    IN HANDLE ParametersHandle
    )

/*++

Routine Description:

    This routine is called by LPX to close the "Parameters" key.
    It closes the handles passed in and does any other work needed.

Arguments:

    ParametersHandle - The handle used to read other parameters.

Return Value:

    None.

--*/

{

    ZwClose (ParametersHandle);

}   /* LpxCloseParametersKey */


NTSTATUS
LpxCountEntries(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    )

/*++

Routine Description:

    This routine is a callback routine for RtlQueryRegistryValues
    It is called with the "Bind" and "Export" multi-strings.
    It counts the number of name entries required in the
    CONFIGURATION_DATA structure and then allocates it.

Arguments:

    ValueName - The name of the value ("Bind" or "Export" -- ignored).

    ValueType - The type of the value (REG_MULTI_SZ -- ignored).

    ValueData - The null-terminated data for the value.

    ValueLength - The length of ValueData (ignored).

    Context - A pointer to a pointer to the ConfigurationInfo structure.
        When the "Export" callback is made this is filled in
        with the allocate structure.

    EntryContext - A pointer to a counter holding the total number
        of name entries required.

Return Value:

    STATUS_SUCCESS

--*/

{
    ULONG StringCount;
    PWCHAR ValuePointer = (PWCHAR)ValueData;
    PCONFIG_DATA * ConfigurationInfo = (PCONFIG_DATA *)Context;
    PULONG TotalCount = ((PULONG)EntryContext);
    ULONG OldTotalCount = *TotalCount;

#if DBG
    ASSERT (ValueType == REG_MULTI_SZ);
#else
    UNREFERENCED_PARAMETER(ValueType);
#endif

    //
    // Count the number of strings in the multi-string; first
    // check that it is NULL-terminated to make the rest
    // easier.
    //

    if ((ValueLength < 2) ||
        (ValuePointer[(ValueLength/2)-1] != (WCHAR)'\0')) {
        return STATUS_INVALID_PARAMETER;
    }

    StringCount = 0;
    while (*ValuePointer != (WCHAR)'\0') {
        while (*ValuePointer != (WCHAR)'\0') {
            ++ValuePointer;
        }
        ++StringCount;
        ++ValuePointer;
        if ((ULONG)((PUCHAR)ValuePointer - (PUCHAR)ValueData) >= ValueLength) {
            break;
        }
    }

    (*TotalCount) += StringCount;

    if (*ValueName == (WCHAR)'E') {

        //
        // This is "Export", allocate the config data structure.
        //

        *ConfigurationInfo = ExAllocatePoolWithTag(
                                 NonPagedPool,
                                 sizeof (CONFIG_DATA) +
                                     ((*TotalCount-1) * sizeof(NDIS_STRING)),
                                 LPX_MEM_TAG_CONFIG_DATA);

        if (*ConfigurationInfo == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlZeroMemory(
            *ConfigurationInfo,
            sizeof(CONFIG_DATA) + ((*TotalCount-1) * sizeof(NDIS_STRING)));

        (*ConfigurationInfo)->DevicesOffset = OldTotalCount;

    }

    return STATUS_SUCCESS;

}   /* LpxCountEntries */


NTSTATUS
LpxAddBind(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    )

/*++

Routine Description:

    This routine is a callback routine for RtlQueryRegistryValues
    It is called for each piece of the "Bind" multi-string and
    saves the information in a ConfigurationInfo structure.

Arguments:

    ValueName - The name of the value ("Bind" -- ignored).

    ValueType - The type of the value (REG_SZ -- ignored).

    ValueData - The null-terminated data for the value.

    ValueLength - The length of ValueData (ignored).

    Context - A pointer to the ConfigurationInfo structure.

    EntryContext - A pointer to a count of binds that is incremented.

Return Value:

    STATUS_SUCCESS

--*/

{
    PCONFIG_DATA ConfigurationInfo = *(PCONFIG_DATA *)Context;
    PULONG CurBindNum = ((PULONG)EntryContext);

    UNREFERENCED_PARAMETER(ValueName);
    UNREFERENCED_PARAMETER(ValueType);
    UNREFERENCED_PARAMETER(ValueLength);

    InsertAdapter(
        ConfigurationInfo,
        *CurBindNum,
        (PWSTR)(ValueData));

    ++(*CurBindNum);

    return STATUS_SUCCESS;

}   /* LpxAddBind */


NTSTATUS
LpxAddExport(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    )

/*++

Routine Description:

    This routine is a callback routine for RtlQueryRegistryValues
    It is called for each piece of the "Export" multi-string and
    saves the information in a ConfigurationInfo structure.

Arguments:

    ValueName - The name of the value ("Export" -- ignored).

    ValueType - The type of the value (REG_SZ -- ignored).

    ValueData - The null-terminated data for the value.

    ValueLength - The length of ValueData (ignored).

    Context - A pointer to the ConfigurationInfo structure.

    EntryContext - A pointer to a count of exports that is incremented.

Return Value:

    STATUS_SUCCESS

--*/

{
    PCONFIG_DATA ConfigurationInfo = *(PCONFIG_DATA *)Context;
    PULONG CurExportNum = ((PULONG)EntryContext);

    UNREFERENCED_PARAMETER(ValueName);
    UNREFERENCED_PARAMETER(ValueType);
    UNREFERENCED_PARAMETER(ValueLength);

    InsertDevice(
        ConfigurationInfo,
        *CurExportNum,
        (PWSTR)(ValueData));

    ++(*CurExportNum);

    return STATUS_SUCCESS;

}   /* LpxAddExport */


VOID
LpxReadLinkageInformation(
    IN PWSTR RegistryPathBuffer,
    IN PCONFIG_DATA * ConfigurationInfo
    )

/*++

Routine Description:

    This routine is called by LPX to read its linkage information
    from the registry. If there is none present, then ConfigData
    is filled with a list of all the adapters that are known
    to LPX.

Arguments:

    RegistryPathBuffer - The null-terminated root of the LPX registry tree.

    ConfigurationInfo - Returns LPX's current configuration.

Return Value:

    None.

--*/

{

    UINT ConfigBindings;
    UINT NameCount = 0;
    NTSTATUS Status;
    RTL_QUERY_REGISTRY_TABLE QueryTable[6];
    PWSTR Subkey = L"Linkage";
    PWSTR Bind = L"Bind";
    PWSTR Export = L"Export";
    ULONG BindCount, ExportCount;
    UINT i;


    //
    // Set up QueryTable to do the following:
    //

    //
    // 1) Switch to the Linkage key below LPX
    //

    QueryTable[0].QueryRoutine = NULL;
    QueryTable[0].Flags = RTL_QUERY_REGISTRY_SUBKEY;
    QueryTable[0].Name = Subkey;

    //
    // 2) Call LpxCountEntries for the "Bind" multi-string
    //

    QueryTable[1].QueryRoutine = LpxCountEntries;
    QueryTable[1].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;
    QueryTable[1].Name = Bind;
    QueryTable[1].EntryContext = (PVOID)&NameCount;
    QueryTable[1].DefaultType = REG_NONE;

    //
    // 3) Call LpxCountEntries for the "Export" multi-string
    //

    QueryTable[2].QueryRoutine = LpxCountEntries;
    QueryTable[2].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;
    QueryTable[2].Name = Export;
    QueryTable[2].EntryContext = (PVOID)&NameCount;
    QueryTable[2].DefaultType = REG_NONE;

    //
    // 4) Call LpxAddBind for each string in "Bind"
    //

    QueryTable[3].QueryRoutine = LpxAddBind;
    QueryTable[3].Flags = 0;
    QueryTable[3].Name = Bind;
    QueryTable[3].EntryContext = (PVOID)&BindCount;
    QueryTable[3].DefaultType = REG_NONE;

    //
    // 5) Call LpxAddExport for each string in "Export"
    //

    QueryTable[4].QueryRoutine = LpxAddExport;
    QueryTable[4].Flags = 0;
    QueryTable[4].Name = Export;
    QueryTable[4].EntryContext = (PVOID)&ExportCount;
    QueryTable[4].DefaultType = REG_NONE;

    //
    // 6) Stop
    //

    QueryTable[5].QueryRoutine = NULL;
    QueryTable[5].Flags = 0;
    QueryTable[5].Name = NULL;


    BindCount = 0;
    ExportCount = 0;

    Status = RtlQueryRegistryValues(
                 RTL_REGISTRY_ABSOLUTE,
                 RegistryPathBuffer,
                 QueryTable,
                 (PVOID)ConfigurationInfo,
                 NULL);

    if (Status != STATUS_SUCCESS) {
        return;
    }

    //
    // Make sure that BindCount and ExportCount match, if not
    // remove the extras.
    //

    if (BindCount < ExportCount) {

        for (i=BindCount; i<ExportCount; i++) {
            RemoveDevice (*ConfigurationInfo, i);
        }
        ConfigBindings = BindCount;

    } else if (ExportCount < BindCount) {

        for (i=ExportCount; i<BindCount; i++) {
            RemoveAdapter (*ConfigurationInfo, i);
        }
        ConfigBindings = ExportCount;

    } else {

        ConfigBindings = BindCount;      // which is equal to ExportCount

    }

    (*ConfigurationInfo)->NumAdapters = ConfigBindings;

}   /* LpxReadLinkageInformation */


ULONG
LpxReadSingleParameter(
    IN HANDLE ParametersHandle,
    IN PWCHAR ValueName,
    IN ULONG DefaultValue
    )

/*++

Routine Description:

    This routine is called by LPX to read a single parameter
    from the registry. If the parameter is found it is stored
    in Data.

Arguments:

    ParametersHandle - A pointer to the open registry.

    ValueName - The name of the value to search for.

    DefaultValue - The default value.

Return Value:

    The value to use; will be the default if the value is not
    found or is not in the correct range.

--*/

{
    ULONG InformationBuffer[32];   // declare ULONG to get it aligned
    PKEY_VALUE_FULL_INFORMATION Information =
        (PKEY_VALUE_FULL_INFORMATION)InformationBuffer;
    UNICODE_STRING ValueKeyName;
    ULONG InformationLength;
    LONG ReturnValue;
    NTSTATUS Status;

    RtlInitUnicodeString (&ValueKeyName, ValueName);

    Status = ZwQueryValueKey(
                 ParametersHandle,
                 &ValueKeyName,
                 KeyValueFullInformation,
                 (PVOID)Information,
                 sizeof (InformationBuffer),
                 &InformationLength);

    if ((Status == STATUS_SUCCESS) &&
        (Information->DataLength == sizeof(ULONG))) {

        RtlCopyMemory(
            (PVOID)&ReturnValue,
            ((PUCHAR)Information) + Information->DataOffset,
            sizeof(ULONG));

        if (ReturnValue < 0) {

            ReturnValue = DefaultValue;

        }

    } else {

        ReturnValue = DefaultValue;

    }

    return ReturnValue;

}   /* LpxReadSingleParameter */


VOID
LpxWriteSingleParameter(
    IN HANDLE ParametersHandle,
    IN PWCHAR ValueName,
    IN ULONG ValueData
    )

/*++

Routine Description:

    This routine is called by LPX to write a single parameter
    from the registry.

Arguments:

    ParametersHandle - A pointer to the open registry.

    ValueName - The name of the value to store.

    ValueData - The data to store at the value.

Return Value:

    None.

--*/

{
    UNICODE_STRING ValueKeyName;
    NTSTATUS Status;
    ULONG TmpValueData = ValueData;

    RtlInitUnicodeString (&ValueKeyName, ValueName);

    Status = ZwSetValueKey(
                 ParametersHandle,
                 &ValueKeyName,
                 0,
                 REG_DWORD,
                 (PVOID)&TmpValueData,
                 sizeof(ULONG));

    if (!NT_SUCCESS(Status)) {
        LpxPrint1("LPX: Could not write dword key: %lx\n", Status);
    }

}   /* LpxWriteSingleParameter */


NTSTATUS
LpxGetExportNameFromRegistry(
    IN  PUNICODE_STRING RegistryPath,
    IN  PUNICODE_STRING BindName,
    OUT PUNICODE_STRING ExportName
    )
{
    NTSTATUS OpenStatus;
    HANDLE ParametersHandle;
    HANDLE LpxConfigHandle;
    NTSTATUS Status;
    PWSTR RegistryPathBuffer;
    OBJECT_ATTRIBUTES TmpObjectAttributes;
    
    RTL_QUERY_REGISTRY_TABLE QueryTable[3];
    PWSTR Subkey = L"Linkage";
    PWSTR Bind = L"Bind";
    PWSTR Export = L"Export";
    LONG BindNumber;

    //
    // Open the registry.
    //

    InitializeObjectAttributes(
        &TmpObjectAttributes,
        RegistryPath,               // name
        OBJ_CASE_INSENSITIVE,       // attributes
        NULL,                       // root
        NULL                        // security descriptor
        );

    OpenStatus = ZwOpenKey(
                     &LpxConfigHandle,
                     KEY_WRITE,
                     &TmpObjectAttributes
                     );

    if (!NT_SUCCESS(OpenStatus)) {
        LpxPrint1("LPX: Could not open LPX key: %lx\n", OpenStatus);
        return OpenStatus;
    }

    Status = LpxOpenParametersKey (LpxConfigHandle, &ParametersHandle);

    if (Status != STATUS_SUCCESS) {
        ZwClose (LpxConfigHandle);
        return Status;
    }

    //
    // LpxReadLinkageInformation expects a null-terminated path,
    // so we have to create one from the UNICODE_STRING.
    //

    RegistryPathBuffer = (PWSTR)ExAllocatePoolWithTag(
                                    NonPagedPool,
                                    RegistryPath->Length + sizeof(WCHAR),
                                    LPX_MEM_TAG_REGISTRY_PATH);
                                    
    if (RegistryPathBuffer == NULL) {
        LpxCloseParametersKey (ParametersHandle);
        ZwClose (LpxConfigHandle);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory (RegistryPathBuffer, RegistryPath->Buffer, RegistryPath->Length);
    *(PWCHAR)(((PUCHAR)RegistryPathBuffer)+RegistryPath->Length) = (WCHAR)'\0';

    //
    // We have a new device whose binding was absent 
    // at boot - get export name given the bind name
    //

    // First we need to get index of the bind name
    
    // Set up QueryTable to do the following:

    //
    // 1) Switch to the Linkage key below LPX
    //

    QueryTable[0].QueryRoutine = NULL;
    QueryTable[0].Flags = RTL_QUERY_REGISTRY_SUBKEY;
    QueryTable[0].Name = Subkey;

    //
    // 2) Call LpxMatchBindName for each string in "Bind"
    //

    QueryTable[1].QueryRoutine = LpxMatchBindName;
    QueryTable[1].Flags = 0;
    QueryTable[1].Name = Bind;
    QueryTable[1].EntryContext = (PVOID)&BindNumber;
    QueryTable[1].DefaultType = REG_NONE;

    //
    // 3) Stop
    //

    QueryTable[2].QueryRoutine = NULL;
    QueryTable[2].Flags = 0;
    QueryTable[2].Name = NULL;


    BindNumber = -1;

    Status = RtlQueryRegistryValues(
                 RTL_REGISTRY_ABSOLUTE,
                 RegistryPathBuffer,
                 QueryTable,
                 (PVOID)BindName,
                 NULL);

    IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint2 ("Status from LpxMatchBindName's = %08x, Bind Number = %d\n",
                        Status, BindNumber);
    }

    if (Status != STATUS_NO_MORE_MATCHES)
    {
#if DBG
        DbgBreakPoint();
#endif
    
        if (Status == STATUS_SUCCESS) {
        
            // We did not find the device 'bind name'
            Status = NDIS_STATUS_ADAPTER_NOT_FOUND;
            
            IF_LPXDBG (LPX_DEBUG_PNP) {
                LpxPrint1 ("LPX - cannot find dynamic binding %S\n", BindName->Buffer);
            }
        }

        goto Done;
    }
    
    ASSERT(BindNumber >= 0);

    // First we need to get export name given index
    
    // Set up QueryTable to do the following:

    //
    // 1) Switch to the Linkage key below LPX
    //

    QueryTable[0].QueryRoutine = NULL;
    QueryTable[0].Flags = RTL_QUERY_REGISTRY_SUBKEY;
    QueryTable[0].Name = Subkey;

    //
    // 2) Call LpxAddExport for each string in "Export"
    //

    QueryTable[1].QueryRoutine = LpxExportAtIndex;
    QueryTable[1].Flags = 0;
    QueryTable[1].Name = Export;
    QueryTable[1].EntryContext = (PVOID)&BindNumber;
    QueryTable[1].DefaultType = REG_NONE;

    //
    // 3) Stop
    //

    QueryTable[2].QueryRoutine = NULL;
    QueryTable[2].Flags = 0;
    QueryTable[2].Name = NULL;

    RtlInitUnicodeString(ExportName, NULL);

    Status = RtlQueryRegistryValues(
                 RTL_REGISTRY_ABSOLUTE,
                 RegistryPathBuffer,
                 QueryTable,
                 (PVOID)ExportName,
                 NULL);

    IF_LPXDBG (LPX_DEBUG_PNP) {
        LpxPrint2("Status from LpxExportAtIndex's = %08x, ExportLength = %d\n",
                        Status,
                        ExportName->Length);

        if (ExportName->Length > 0)
        {
            LpxPrint1("ExportName = %S\n", ExportName->Buffer);
        }
    }

    if (ExportName->Length != 0) {

        ASSERT(Status == STATUS_NO_MORE_MATCHES);
        
        Status = STATUS_SUCCESS;
    }
    else {
    
        // We found the bind, but no corr export  
        Status = NDIS_ERROR_CODE_MISSING_CONFIGURATION_PARAMETER;
    }

Done:

    ExFreePool (RegistryPathBuffer);
    
    LpxCloseParametersKey (ParametersHandle);
    
    ZwClose (LpxConfigHandle);

    return Status;
}


NTSTATUS
LpxMatchBindName(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    )
/*++

Routine Description:

    This routine is a callback routine for RtlQueryRegistryValues
    It is called for each piece of the "Bind" multi-string and
    tries to match a given bind name with each of these pieces.

Arguments:

    ValueName - The name of the value ("Bind" -- ignored).

    ValueType - The type of the value (REG_SZ -- ignored).

    ValueData - The null-terminated data for the value.

    ValueLength - The length of ValueData (ignored).

    Context - Bind name that we are trying to match.

    EntryContext - A pointer where index of the match is stored.

Return Value:

    STATUS_SUCCESS

--*/

{
    PUNICODE_STRING BindName = (PUNICODE_STRING) Context;
    PLONG CurBindNum = (PLONG) EntryContext;
    UNICODE_STRING ValueString;

    UNREFERENCED_PARAMETER(ValueName);
    UNREFERENCED_PARAMETER(ValueType);
    UNREFERENCED_PARAMETER(ValueLength);

    RtlInitUnicodeString(&ValueString, ValueData);

    // We are yet to find a match

    (*CurBindNum)++ ;
    
    if (NdisEqualString(BindName, &ValueString, TRUE)) {
        return STATUS_NO_MORE_MATCHES;
    }

    return STATUS_SUCCESS;
    
}   /* LpxMatchBindName */

NTSTATUS
LpxExportAtIndex(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    )
{
    PUNICODE_STRING ExportName = (PUNICODE_STRING) Context;
    PLONG CurBindNum = (PLONG) EntryContext;
    PWSTR ValueWideChars;
    UINT ValueWideLength;    
//    UNICODE_STRING ValueString;

    UNREFERENCED_PARAMETER(ValueName);
    UNREFERENCED_PARAMETER(ValueType);
    UNREFERENCED_PARAMETER(ValueLength);

    ASSERT(*CurBindNum >= 0);

    if (*CurBindNum == 0)
    {
        ValueWideLength = LpxWstrLength(ValueData) + sizeof(WCHAR);

        ValueWideChars = (PWSTR) ExAllocatePoolWithTag(NonPagedPool, 
                                                       ValueWideLength, 
                                                       LPX_MEM_TAG_DEVICE_EXPORT);
        if (ValueWideChars == NULL)
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        
        RtlCopyMemory (ValueWideChars, ValueData, ValueWideLength);
        
        RtlInitUnicodeString (ExportName, ValueWideChars);

        return STATUS_NO_MORE_MATCHES;
    }
    
    (*CurBindNum)-- ;
    
    return STATUS_SUCCESS;
    
}   /* LpxExportAtIndex */


