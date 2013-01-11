/*++

Copyright (c) 2003  Ximeta Technology

Module Name:

    LpxCfg.c

Abstract:



Author:

  

Revision History:

  
--*/

#include "precomp.h"
#pragma hdrstop

//
// Local functions used to access the registry.
//










#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,Lpx_WstrLength)
#pragma alloc_text(PAGE,Lpx_OpenParametersKey)
#pragma alloc_text(PAGE,Lpx_CloseParametersKey)
#pragma alloc_text(PAGE,Lpx_CountEntries)
#pragma alloc_text(PAGE,Lpx_AddBind)
#pragma alloc_text(PAGE,Lpx_AddExport)
#pragma alloc_text(PAGE,Lpx_ReadLinkageInformation)
#pragma alloc_text(PAGE,Lpx_ReadSingleParameter)
#pragma alloc_text(PAGE,Lpx_WriteSingleParameter)
#endif

UINT
Lpx_WstrLength(
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
    UINT _L = Lpx_WstrLength(_N)+sizeof(WCHAR); \
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
    UINT _L = Lpx_WstrLength(_N)+sizeof(WCHAR); \
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
        Lpx_ReadSingleParameter( \
             ParametersHandle, \
             Str ## _Field, \
             ConfigurationInfo->_Field); \
}

#define WRITE_HIDDEN_CONFIG(_Field) \
{ \
    Lpx_WriteSingleParameter( \
        ParametersHandle, \
        Str ## _Field, \
        ConfigurationInfo->_Field); \
}



NTSTATUS
Lpx_ConfigureTransport (
    IN PUNICODE_STRING RegistryPath,
    IN PCONFIG_DATA * ConfigurationInfoPtr
    )
{

    NTSTATUS OpenStatus;
    HANDLE ParametersHandle;
    HANDLE Lpx_ConfigHandle;
    NTSTATUS Status;
    ULONG Disposition;
    PWSTR RegistryPathBuffer;
    OBJECT_ATTRIBUTES TmpObjectAttributes;
    PCONFIG_DATA ConfigurationInfo;


    DECLARE_STRING(InitConnections);
    DECLARE_STRING(InitAddresses);
  
    DECLARE_STRING(MaxConnections);
    DECLARE_STRING(MaxAddresses);

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
                 &Lpx_ConfigHandle,
                 KEY_WRITE,
                 &TmpObjectAttributes,
                 0,                 // title index
                 NULL,              // class
                 0,                 // create options
                 &Disposition);     // disposition

    if (!NT_SUCCESS(Status)) {
        DebugPrint(8,("Lpx: Could not open/create NBF key: %lx\n", Status));
        return Status;
    }

    
	DebugPrint(0,("%s lpx key: %lx\n",
		(Disposition == REG_CREATED_NEW_KEY) ? "created" : "opened",
		Lpx_ConfigHandle));
   


    OpenStatus = Lpx_OpenParametersKey (Lpx_ConfigHandle, &ParametersHandle);

    if (OpenStatus != STATUS_SUCCESS) {
        return OpenStatus;
    }

    //
    // Read in the NDIS binding information (if none is present
    // the array will be filled with all known drivers).
    //
    // Lpx_ReadLinkageInformation expects a null-terminated path,
    // so we have to create one from the UNICODE_STRING.
    //

    RegistryPathBuffer = (PWSTR)ExAllocatePoolWithTag(
                                    NonPagedPool,
                                    RegistryPath->Length + sizeof(WCHAR),
                                    LPX_MEM_TAG_REGISTRY_PATH);
    if (RegistryPathBuffer == NULL) {
        Lpx_CloseParametersKey (ParametersHandle);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory (RegistryPathBuffer, RegistryPath->Buffer, RegistryPath->Length);
    *(PWCHAR)(((PUCHAR)RegistryPathBuffer)+RegistryPath->Length) = (WCHAR)'\0';

    Lpx_ReadLinkageInformation (RegistryPathBuffer, ConfigurationInfoPtr);

    if (*ConfigurationInfoPtr == NULL) {
        ExFreePool (RegistryPathBuffer);
        Lpx_CloseParametersKey (ParametersHandle);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    ConfigurationInfo = *ConfigurationInfoPtr;


    //
    // Configure the initial values for some NBF resources.
    //

    ConfigurationInfo->InitConnections = 10;
    ConfigurationInfo->InitAddresses = 10;

  
    ConfigurationInfo->MaxConnections = 128;         
    ConfigurationInfo->MaxAddresses = 128;  
    

   


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
    READ_HIDDEN_CONFIG (InitAddresses);

  
    READ_HIDDEN_CONFIG (MaxConnections);
    READ_HIDDEN_CONFIG (MaxAddresses);




    //
    // 5/22/92 - don't write the parameters that are set
    // based on Size, since otherwise these will overwrite
    // those values since hidden parameters are set up
    // after the Size-based configuration is done.
    //


    WRITE_HIDDEN_CONFIG (MaxConnections);
    WRITE_HIDDEN_CONFIG (MaxAddresses);

 

    ExFreePool (RegistryPathBuffer);
    Lpx_CloseParametersKey (ParametersHandle);
    ZwClose (Lpx_ConfigHandle);

    return STATUS_SUCCESS;

}   /* Lpx_ConfigureTransport */


VOID
Lpx_FreeConfigurationInfo (
    IN PCONFIG_DATA ConfigurationInfo
    )
{
    UINT i;

    for (i=0; i<ConfigurationInfo->NumAdapters; i++) {
        RemoveAdapter (ConfigurationInfo, i);
        RemoveDevice (ConfigurationInfo, i);
    }
    ExFreePool (ConfigurationInfo);

}   /* Lpx_FreeConfigurationInfo */


NTSTATUS
Lpx_OpenParametersKey(
    IN HANDLE Lpx_ConfigHandle,
    OUT PHANDLE ParametersHandle
    )
{

    NTSTATUS Status;
    HANDLE ParamHandle;
    PWSTR ParametersString = L"Parameters";
    UNICODE_STRING ParametersKeyName;
    OBJECT_ATTRIBUTES TmpObjectAttributes;

    //
    // Open the NBF parameters key.
    //

    RtlInitUnicodeString (&ParametersKeyName, ParametersString);

    InitializeObjectAttributes(
        &TmpObjectAttributes,
        &ParametersKeyName,         // name
        OBJ_CASE_INSENSITIVE,       // attributes
        Lpx_ConfigHandle,            // root
        NULL                        // security descriptor
        );


    Status = ZwOpenKey(
                 &ParamHandle,
                 KEY_READ,
                 &TmpObjectAttributes);

    if (!NT_SUCCESS(Status)) {

        DebugPrint(8,("Could not open parameters key: %lx\n", Status));
        return Status;

    }

 
    *ParametersHandle = ParamHandle;


    //
    // All keys successfully opened or created.
    //

    return STATUS_SUCCESS;

}   /* Lpx_OpenParametersKey */

VOID
Lpx_CloseParametersKey(
    IN HANDLE ParametersHandle
    )

{

    ZwClose (ParametersHandle);

}   /* Lpx_CloseParametersKey */


NTSTATUS
Lpx_CountEntries(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    )
{
    ULONG StringCount;
    PWCHAR ValuePointer = (PWCHAR)ValueData;
    PCONFIG_DATA * ConfigurationInfo = (PCONFIG_DATA *)Context;
    PULONG TotalCount = ((PULONG)EntryContext);
    ULONG OldTotalCount = *TotalCount;


    UNREFERENCED_PARAMETER(ValueType);


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

}   /* Lpx_CountEntries */


NTSTATUS
Lpx_AddBind(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    )
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

}   /* Lpx_AddBind */


NTSTATUS
Lpx_AddExport(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    )
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

}   /* Lpx_AddExport */


VOID
Lpx_ReadLinkageInformation(
    IN PWSTR RegistryPathBuffer,
    IN PCONFIG_DATA * ConfigurationInfo
    )
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
    // 1) Switch to the Linkage key below NBF
    //

    QueryTable[0].QueryRoutine = NULL;
    QueryTable[0].Flags = RTL_QUERY_REGISTRY_SUBKEY;
    QueryTable[0].Name = Subkey;

    //
    // 2) Call Lpx_CountEntries for the "Bind" multi-string
    //

    QueryTable[1].QueryRoutine = Lpx_CountEntries;
    QueryTable[1].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;
    QueryTable[1].Name = Bind;
    QueryTable[1].EntryContext = (PVOID)&NameCount;
    QueryTable[1].DefaultType = REG_NONE;

    //
    // 3) Call Lpx_CountEntries for the "Export" multi-string
    //

    QueryTable[2].QueryRoutine = Lpx_CountEntries;
    QueryTable[2].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;
    QueryTable[2].Name = Export;
    QueryTable[2].EntryContext = (PVOID)&NameCount;
    QueryTable[2].DefaultType = REG_NONE;

    //
    // 4) Call Lpx_AddBind for each string in "Bind"
    //

    QueryTable[3].QueryRoutine = Lpx_AddBind;
    QueryTable[3].Flags = 0;
    QueryTable[3].Name = Bind;
    QueryTable[3].EntryContext = (PVOID)&BindCount;
    QueryTable[3].DefaultType = REG_NONE;

    //
    // 5) Call Lpx_AddExport for each string in "Export"
    //

    QueryTable[4].QueryRoutine = Lpx_AddExport;
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

}   /* Lpx_ReadLinkageInformation */


ULONG
Lpx_ReadSingleParameter(
    IN HANDLE ParametersHandle,
    IN PWCHAR ValueName,
    IN ULONG DefaultValue
    )
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

}   /* Lpx_ReadSingleParameter */


VOID
Lpx_WriteSingleParameter(
    IN HANDLE ParametersHandle,
    IN PWCHAR ValueName,
    IN ULONG ValueData
    )
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
        DebugPrint(8,("NBF: Could not write dword key: %lx\n", Status));
    }

}   /* Lpx_WriteSingleParameter */


NTSTATUS
Lpx_GetExportNameFromRegistry(
    IN  PUNICODE_STRING RegistryPath,
    IN  PUNICODE_STRING BindName,
    OUT PUNICODE_STRING ExportName
    )
{
    NTSTATUS OpenStatus;
    HANDLE ParametersHandle;
    HANDLE Lpx_ConfigHandle;
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
                     &Lpx_ConfigHandle,
                     KEY_WRITE,
                     &TmpObjectAttributes
                     );

    if (!NT_SUCCESS(OpenStatus)) {
        DebugPrint(8,("NBF: Could not open NBF key: %lx\n", OpenStatus));
        return OpenStatus;
    }

    Status = Lpx_OpenParametersKey (Lpx_ConfigHandle, &ParametersHandle);

    if (Status != STATUS_SUCCESS) {
        ZwClose (Lpx_ConfigHandle);
        return Status;
    }

    //
    // Lpx_ReadLinkageInformation expects a null-terminated path,
    // so we have to create one from the UNICODE_STRING.
    //

    RegistryPathBuffer = (PWSTR)ExAllocatePoolWithTag(
                                    NonPagedPool,
                                    RegistryPath->Length + sizeof(WCHAR),
                                    LPX_MEM_TAG_REGISTRY_PATH);
                                    
    if (RegistryPathBuffer == NULL) {
        Lpx_CloseParametersKey (ParametersHandle);
        ZwClose (Lpx_ConfigHandle);
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
    // 1) Switch to the Linkage key below NBF
    //

    QueryTable[0].QueryRoutine = NULL;
    QueryTable[0].Flags = RTL_QUERY_REGISTRY_SUBKEY;
    QueryTable[0].Name = Subkey;

    //
    // 2) Call Lpx_MatchBindName for each string in "Bind"
    //

    QueryTable[1].QueryRoutine = Lpx_MatchBindName;
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


	DebugPrint(0, ("Status from Lpx_MatchBindName's = %08x, Bind Number = %d\n",
					Status, BindNumber));
  

    if (Status != STATUS_NO_MORE_MATCHES)
    {

    
        if (Status == STATUS_SUCCESS) {
        
            // We did not find the device 'bind name'
            Status = NDIS_STATUS_ADAPTER_NOT_FOUND;
            
        
            DebugPrint(8, ("Lpx - cannot find dynamic binding %S\n", BindName->Buffer));
           
        }

        goto Done;
    }
    
    ASSERT(BindNumber >= 0);

    // First we need to get export name given index
    
    // Set up QueryTable to do the following:

    //
    // 1) Switch to the Linkage key below NBF
    //

    QueryTable[0].QueryRoutine = NULL;
    QueryTable[0].Flags = RTL_QUERY_REGISTRY_SUBKEY;
    QueryTable[0].Name = Subkey;

    //
    // 2) Call Lpx_AddExport for each string in "Export"
    //

    QueryTable[1].QueryRoutine = Lpx_ExportAtIndex;
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

   
	DebugPrint(0,("Status from Lpx_ExportAtIndex's = %08x, ExportLength = %d\n",
					Status,
					ExportName->Length));

	if (ExportName->Length > 0)
	{
		DebugPrint(0,("ExportName = %S\n", ExportName->Buffer));
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
    
    Lpx_CloseParametersKey (ParametersHandle);
    
    ZwClose (Lpx_ConfigHandle);

    return Status;
}


NTSTATUS
Lpx_MatchBindName(
    IN PWSTR ValueName,
    IN ULONG ValueType,
    IN PVOID ValueData,
    IN ULONG ValueLength,
    IN PVOID Context,
    IN PVOID EntryContext
    )
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
    
}   /* Lpx_MatchBindName */

NTSTATUS
Lpx_ExportAtIndex(
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
    UNICODE_STRING ValueString;

    UNREFERENCED_PARAMETER(ValueName);
    UNREFERENCED_PARAMETER(ValueType);

    ASSERT(*CurBindNum >= 0);

    if (*CurBindNum == 0)
    {
        ValueWideLength = Lpx_WstrLength(ValueData) + sizeof(WCHAR);

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
    
}   /* Lpx_ExportAtIndex */

