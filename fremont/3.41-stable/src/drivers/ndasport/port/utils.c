/*++

This module contains utility routines

--*/
#include "port.h"

#ifdef RUN_WPP
#include "utils.tmh"
#endif

#if DBG

#define NDASPORT_DEBUG_BUFFER_LEN 128

ULONG DebugLevel = 0x04;
CHAR DebugBuffer[NDASPORT_DEBUG_BUFFER_LEN];

VOID
NdasPortDebugPrint(
    ULONG DebugPrintLevel,
    PCCHAR DebugMessage,
    ...)
{
    va_list ap;
    va_start(ap, DebugMessage);
    if (DebugPrintLevel <= DebugLevel) 
	{
		RtlStringCchVPrintfA(DebugBuffer, NDASPORT_DEBUG_BUFFER_LEN, DebugMessage, ap);
        DbgPrint(DebugBuffer);
    }
    va_end(ap);
}

#endif


/*++

Routine Description:

	This routine will take a null terminated array of ascii strings and merge
	them together into a unicode multi-string block.

	This routine allocates memory for the string buffer - is the caller's
	responsibility to free it.

Arguments:

	MultiString - a UNICODE_STRING structure into which the multi string will
	be built.

	StringArray - a NULL terminated list of narrow strings which will be combined
	together.  This list may not be empty.

Return Value:

	status

--*/
NTSTATUS
NpStringArrayToMultiString(
    PUNICODE_STRING MultiString,
    CONST PSTR* StringArray)
{
    ANSI_STRING ansiEntry;

    UNICODE_STRING unicodeEntry;
    // PWSTR unicodeLocation;

    UCHAR i;

    NTSTATUS status;

    PAGED_CODE();

    //
    // Make sure we aren't going to leak any memory
    //

    ASSERT(MultiString->Buffer == NULL);

    RtlInitUnicodeString(MultiString, NULL);

    for (i = 0; StringArray[i] != NULL; i++) 
	{
        RtlInitAnsiString(&ansiEntry, StringArray[i]);
        MultiString->Length += (USHORT) RtlAnsiStringToUnicodeSize(&ansiEntry);
    }

    ASSERT(MultiString->Length != 0);

    MultiString->MaximumLength = MultiString->Length + sizeof(UNICODE_NULL);

    MultiString->Buffer = (PWCHAR) ExAllocatePoolWithTag(
		PagedPool,
        MultiString->MaximumLength,
        NDASPORT_TAG_PNP_ID);

    if (MultiString->Buffer == NULL) 
	{
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(
		MultiString->Buffer, 
		MultiString->MaximumLength);

    unicodeEntry = *MultiString;

    for (i = 0; StringArray[i] != NULL; i++) 
	{
		RtlInitAnsiString(
			&ansiEntry, 
			StringArray[i]);

		status = RtlAnsiStringToUnicodeString(
                    &unicodeEntry,
                    &ansiEntry,
                    FALSE);

        //
        // Since we're not allocating any memory the only failure possible
        // is if this function is bad
        //

        ASSERT(NT_SUCCESS(status));

        //
        // Push the buffer location up and reduce the maximum count
        //

		unicodeEntry.Buffer = 
			(PWSTR)(
				(PSTR)(unicodeEntry.Buffer) + 
				unicodeEntry.Length + sizeof(WCHAR));
        unicodeEntry.MaximumLength -= unicodeEntry.Length + sizeof(WCHAR);
    }

    //
    // Stick the final NUL on the end of the multisz
    //

//    RtlZeroMemory(unicodeEntry.Buffer, unicodeEntry.MaximumLength);

    return STATUS_SUCCESS;
}


NTSTATUS
NpMultiStringToStringArray(
    __in PUNICODE_STRING MultiString,
    __out PWSTR *StringArray[],
    BOOLEAN Forward)

{
    ULONG stringCount = 0;
    ULONG stringNumber;
    ULONG i;
    PWSTR *stringArray;

    PAGED_CODE();

    //
    // Pass one: count the number of string elements.
    //

    for (i = 0; i < (MultiString->MaximumLength / sizeof(WCHAR)); i++) 
	{
        if (MultiString->Buffer[i] == UNICODE_NULL) 
		{
            stringCount++;
        }
    }

    //
    // Allocate the memory for a NULL-terminated string array.
    //

    stringArray = (PWSTR*) ExAllocatePoolWithTag(
		PagedPool,
        (stringCount + 1) * sizeof(PWSTR),
        NDASPORT_TAG_PNP_ID);

    if (stringArray == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(
		stringArray, 
		(stringCount + 1) * sizeof(PWSTR));

    //
    // Pass two : Put the string pointers in place.
    //

    i = 0;

    for (stringNumber = 0; stringNumber < stringCount; stringNumber++) {

        ULONG arrayNumber;

        if (Forward) {
            arrayNumber = stringNumber;
        } else {
            arrayNumber = stringCount - stringNumber - 1;
        }

        //
        // Put a pointer to the head of the string into the array.
        //

        stringArray[arrayNumber] = &MultiString->Buffer[i];

        //
        // Scan for the end of the string.
        //

        while((i < (MultiString->MaximumLength / sizeof(WCHAR))) &&
              (MultiString->Buffer[i] != UNICODE_NULL)) {
            i++;
        }

        //
        // Jump past the NULL.
        //

        i++;
    }

    *StringArray = stringArray;
    return STATUS_SUCCESS;
}

CONST CHAR* DbgPnPMinorFunctionStringA(__in UCHAR MinorFunction)
{
    switch (MinorFunction)
    {
    case IRP_MN_START_DEVICE: return "IRP_MN_START_DEVICE";
    case IRP_MN_QUERY_REMOVE_DEVICE: return "IRP_MN_QUERY_REMOVE_DEVICE";
    case IRP_MN_REMOVE_DEVICE: return "IRP_MN_REMOVE_DEVICE";
    case IRP_MN_CANCEL_REMOVE_DEVICE: return "IRP_MN_CANCEL_REMOVE_DEVICE";
    case IRP_MN_STOP_DEVICE: return "IRP_MN_STOP_DEVICE";
    case IRP_MN_QUERY_STOP_DEVICE: return "IRP_MN_QUERY_STOP_DEVICE";
    case IRP_MN_CANCEL_STOP_DEVICE: return "IRP_MN_CANCEL_STOP_DEVICE";
    case IRP_MN_QUERY_DEVICE_RELATIONS: return "IRP_MN_QUERY_DEVICE_RELATIONS";
    case IRP_MN_QUERY_INTERFACE: return "IRP_MN_QUERY_INTERFACE";
    case IRP_MN_QUERY_CAPABILITIES: return "IRP_MN_QUERY_CAPABILITIES";
    case IRP_MN_QUERY_RESOURCES: return "IRP_MN_QUERY_RESOURCES";
    case IRP_MN_QUERY_RESOURCE_REQUIREMENTS: return "IRP_MN_QUERY_RESOURCE_REQUIREMENTS";
    case IRP_MN_QUERY_DEVICE_TEXT: return "IRP_MN_QUERY_DEVICE_TEXT";
    case IRP_MN_FILTER_RESOURCE_REQUIREMENTS: return "IRP_MN_FILTER_RESOURCE_REQUIREMENTS";
    case IRP_MN_READ_CONFIG: return "IRP_MN_READ_CONFIG";
    case IRP_MN_WRITE_CONFIG: return "IRP_MN_WRITE_CONFIG";
    case IRP_MN_EJECT: return "IRP_MN_EJECT";
    case IRP_MN_SET_LOCK: return "IRP_MN_SET_LOCK";
    case IRP_MN_QUERY_ID: return "IRP_MN_QUERY_ID";
    case IRP_MN_QUERY_PNP_DEVICE_STATE: return "IRP_MN_QUERY_PNP_DEVICE_STATE";
    case IRP_MN_QUERY_BUS_INFORMATION: return "IRP_MN_QUERY_BUS_INFORMATION";
    case IRP_MN_DEVICE_USAGE_NOTIFICATION: return "IRP_MN_DEVICE_USAGE_NOTIFICATION";
    case IRP_MN_SURPRISE_REMOVAL: return "IRP_MN_SURPRISE_REMOVAL";
    case IRP_MN_QUERY_LEGACY_BUS_INFORMATION: return "IRP_MN_QUERY_LEGACY_BUS_INFORMATION";           
    default: return "PNP_IRP_MN_?UNKNOWN?";
	}
}

CONST CHAR* DbgDeviceRelationStringA(__in DEVICE_RELATION_TYPE Type)
{
	switch (Type)
	{
	case BusRelations: return "BusRelations";
	case EjectionRelations: return "EjectionRelations";
	case RemovalRelations: return "RemovalRelations";
	case TargetDeviceRelation: return "TargetDeviceRelation";
	case PowerRelations: return "PowerRelations";
	case 5 /* SingleBusRelations*/ : return "SingleBusRelations";
	default: return "UnKnown Relation";
	}
}

CONST CHAR* DbgDeviceIDStringA(__in BUS_QUERY_ID_TYPE Type)
{
    switch (Type)
    {
    case BusQueryDeviceID: return "BusQueryDeviceID";
    case BusQueryHardwareIDs: return "BusQueryHardwareIDs";
    case BusQueryCompatibleIDs: return "BusQueryCompatibleIDs";
    case BusQueryInstanceID: return "BusQueryInstanceID";
    case BusQueryDeviceSerialNumber: return "BusQueryDeviceSerialNumber";
    default: return "UnKnown ID";
    }
}

CONST CHAR* DbgDeviceTextTypeStringA(__in DEVICE_TEXT_TYPE Type)
{
	switch (Type)
	{
	case DeviceTextDescription: return "DeviceTextDescription";
	case DeviceTextLocationInformation: return "DeviceTextLocationInformation";
	default: return "UnknownDeviceTextType";
	}
}

CONST CHAR* DbgSrbFunctionStringA(__in UCHAR SrbFunction)
{
	switch (SrbFunction)
	{
	case SRB_FUNCTION_EXECUTE_SCSI:  /* 0x00 */ return "SRB_FUNCTION_EXECUTE_SCSI";
	case SRB_FUNCTION_CLAIM_DEVICE:  /* 0x01 */ return "SRB_FUNCTION_CLAIM_DEVICE";
	case SRB_FUNCTION_IO_CONTROL:    /* 0x02 */ return "SRB_FUNCTION_IO_CONTROL";
	case SRB_FUNCTION_RECEIVE_EVENT: /* 0x03 */ return "SRB_FUNCTION_RECEIVE_EVENT";
	case SRB_FUNCTION_RELEASE_QUEUE: /* 0x04 */ return "SRB_FUNCTION_RELEASE_QUEUE";
	case SRB_FUNCTION_ATTACH_DEVICE: /* 0x05 */ return "SRB_FUNCTION_ATTACH_DEVICE";
	case SRB_FUNCTION_RELEASE_DEVICE:/* 0x06 */ return "SRB_FUNCTION_RELEASE_DEVICE";
	case SRB_FUNCTION_SHUTDOWN:      /* 0x07 */ return "SRB_FUNCTION_SHUTDOWN";
	case SRB_FUNCTION_FLUSH:         /* 0x08 */ return "SRB_FUNCTION_FLUSH";
	case SRB_FUNCTION_ABORT_COMMAND: /* 0x10 */ return "SRB_FUNCTION_ABORT_COMMAND";
	case SRB_FUNCTION_RELEASE_RECOVERY: /* 0x11 */ return "SRB_FUNCTION_RELEASE_RECOVERY";
	case SRB_FUNCTION_RESET_BUS:     /* 0x12 */ return "SRB_FUNCTION_RESET_BUS";
	case SRB_FUNCTION_RESET_DEVICE:  /* 0x13 */ return "SRB_FUNCTION_RESET_DEVICE";
	case SRB_FUNCTION_TERMINATE_IO:  /* 0x14 */ return "SRB_FUNCTION_TERMINATE_IO";
	case SRB_FUNCTION_FLUSH_QUEUE:   /* 0x15 */ return "SRB_FUNCTION_FLUSH_QUEUE";
	case SRB_FUNCTION_REMOVE_DEVICE: /* 0x16 */ return "SRB_FUNCTION_REMOVE_DEVICE";
	case SRB_FUNCTION_WMI:           /* 0x17 */ return "SRB_FUNCTION_WMI";
	case SRB_FUNCTION_LOCK_QUEUE:    /* 0x18 */ return "SRB_FUNCTION_LOCK_QUEUE";
	case SRB_FUNCTION_UNLOCK_QUEUE:  /* 0x19 */ return "SRB_FUNCTION_UNLOCK_QUEUE";
	case SRB_FUNCTION_RESET_LOGICAL_UNIT: /* 0x20 */ return "SRB_FUNCTION_RESET_LOGICAL_UNIT";
	case SRB_FUNCTION_SET_LINK_TIMEOUT: /* 0x21 */ return "SRB_FUNCTION_SET_LINK_TIMEOUT";
	case SRB_FUNCTION_LINK_TIMEOUT_OCCURRED: /* 0x22 */ return "SRB_FUNCTION_LINK_TIMEOUT_OCCURRED";
	case SRB_FUNCTION_LINK_TIMEOUT_COMPLETE: /* 0x23 */ return "SRB_FUNCTION_LINK_TIMEOUT_COMPLETE";
	case SRB_FUNCTION_POWER: /* 0x24 */ return "SRB_FUNCTION_POWER";
	case SRB_FUNCTION_PNP: /* 0x25 */ return "SRB_FUNCTION_PNP";

	default: return "SRB_FUNCTION_?UNKNOWN?";
	}
}

CONST CHAR* DbgStorageQueryTypeStringA(__in STORAGE_QUERY_TYPE Type)
{
	switch (Type)
	{
	case PropertyStandardQuery: return "PropertyStandardQuery";
	case PropertyExistsQuery: return "PropertyExistsQuery";
	case PropertyMaskQuery: return "PropertyMaskQuery";
	case PropertyQueryMaxDefined: return "PropertyQueryMaxDefined";
	default: return "PropertyQueryType?UNKNOWN?";
	}
}

CONST CHAR* DbgStoragePropertyIdStringA(__in STORAGE_PROPERTY_ID Id)
{
	switch (Id)
	{
	case StorageDeviceProperty: return "StorageDeviceProperty";
	case StorageAdapterProperty: return "StorageAdapterProperty";
	default: return "StorageProperty?UNKNOWN?";
	}
}

CONST CHAR* DbgPowerMinorFunctionString(__in UCHAR MinorFunction)
{
	switch (MinorFunction)
	{
	case IRP_MN_WAIT_WAKE: return "IRP_MN_WAIT_WAKE";
	case IRP_MN_POWER_SEQUENCE: return "IRP_MN_POWER_SEQUENCE";
	case IRP_MN_SET_POWER: return "IRP_MN_SET_POWER";
	case IRP_MN_QUERY_POWER: return "IRP_MN_QUERY_POWER";
	default: return "POWER_IRP_MN_?UNKNOWN?";
	}
}

CONST CHAR* DbgSystemPowerString(__in SYSTEM_POWER_STATE Type) 
{  
    switch (Type)
    {
    case PowerSystemUnspecified: return "PowerSystemUnspecified";
    case PowerSystemWorking: return "PowerSystemWorking";
    case PowerSystemSleeping1: return "PowerSystemSleeping1";
    case PowerSystemSleeping2: return "PowerSystemSleeping2";
    case PowerSystemSleeping3: return "PowerSystemSleeping3";
    case PowerSystemHibernate: return "PowerSystemHibernate";
    case PowerSystemShutdown: return "PowerSystemShutdown";
    case PowerSystemMaximum: return "PowerSystemMaximum";
    default: return "SystemPowerState?UNKNOWN?";
    }
}

CONST CHAR* DbgDevicePowerString(__in DEVICE_POWER_STATE Type)   
{
    switch (Type)
    {
    case PowerDeviceUnspecified: return "PowerDeviceUnspecified";
    case PowerDeviceD0: return "PowerDeviceD0";
    case PowerDeviceD1: return "PowerDeviceD1";
    case PowerDeviceD2: return "PowerDeviceD2";
    case PowerDeviceD3: return "PowerDeviceD3";
    case PowerDeviceMaximum: return "PowerDeviceMaximum";
    default: return "DevicePowerState?UNKNOWN?";
    }
}

CONST CHAR* DbgTdiPnpOpCodeString(__in TDI_PNP_OPCODE OpCode)
{

	switch (OpCode)
	{
	case TDI_PNP_OP_MIN: return "TDI_PNP_OP_MIN";
	case TDI_PNP_OP_ADD: return "TDI_PNP_OP_ADD";
	case TDI_PNP_OP_DEL: "TDI_PNP_OP_DEL";
	case TDI_PNP_OP_UPDATE: return "TDI_PNP_OP_UPDATE";
	case TDI_PNP_OP_PROVIDERREADY: return "TDI_PNP_OP_PROVIDERREADY";
	case TDI_PNP_OP_NETREADY: return "TDI_PNP_OP_NETREADY";
	case TDI_PNP_OP_ADD_IGNORE_BINDING: return "TDI_PNP_OP_ADD_IGNORE_BINDING";
	case TDI_PNP_OP_DELETE_IGNORE_BINDING: return "TDI_PNP_OP_DELETE_IGNORE_BINDING";
	case TDI_PNP_OP_MAX: return "TDI_PNP_OP_MAX";
	default: return "TDI_PNP_OP_?UNKNOWN?";
	}
}

CONST CHAR* DbgNetPnpEventCodeString(__in NET_PNP_EVENT_CODE PnpEventCode)
{
	switch (PnpEventCode)
	{
		case NetEventSetPower: return "NetEventSetPower";
		case NetEventQueryPower: return "NetEventQueryPower";
		case NetEventQueryRemoveDevice: return "NetEventQueryRemoveDevice";
		case NetEventCancelRemoveDevice: return "NetEventCancelRemoveDevice";
		case NetEventReconfigure: return "NetEventReconfigure";
		case NetEventBindList: return "NetEventBindList";
		case NetEventBindsComplete: return "NetEventBindsComplete";
		case NetEventPnPCapabilities: return "NetEventPnPCapabilities";
		case NetEventPause: return "NetEventPause";
		case NetEventRestart: return "NetEventRestart";
		case NetEventPortActivation: return "NetEventPortActivation";
		case NetEventPortDeactivation: return "NetEventPortDeactivation";
		case NetEventIMReEnableDevice: return "NetEventIMReEnableDevice";
		case NetEventMaximum: return "NetEventMaximum";
		default: return "NetEvent?Unknown?";
	}
}

//////////////////////////////////////////////////////////////////////////
//
// srb.h
//
//////////////////////////////////////////////////////////////////////////

CONST CHAR* DbgScsiOpStringA(__in UCHAR ScsiOpCode)
{

	// RegEx Replacement
	// From: \#define[ \t]+SCSIOP_{[A-Z0-9_]+}[ \t]+{[A-Za-z0-9]+}
	// To:   \tcase SCSIOP_\1: /* \2 */ return "SCSIOP_\1";
	//

	// SCSI CDB operation codes
	//
	switch (ScsiOpCode)
	{
	// 6-byte commands:
	case SCSIOP_TEST_UNIT_READY: /* 0x00 */ return "SCSIOP_TEST_UNIT_READY";
	// case SCSIOP_REZERO_UNIT: /* 0x01 */ 
	// case SCSIOP_REWIND: /* 0x01 */ 
	case 0x01: return "SCSIOP_REZERO_UNIT"; // |SCSIOP_REWIND";
	case SCSIOP_REQUEST_BLOCK_ADDR: /* 0x02 */ return "SCSIOP_REQUEST_BLOCK_ADDR";
	case SCSIOP_REQUEST_SENSE: /* 0x03 */ return "SCSIOP_REQUEST_SENSE";
	case SCSIOP_FORMAT_UNIT: /* 0x04 */ return "SCSIOP_FORMAT_UNIT";
	case SCSIOP_READ_BLOCK_LIMITS: /* 0x05 */ return "SCSIOP_READ_BLOCK_LIMITS";
	// case SCSIOP_REASSIGN_BLOCKS: /* 0x07 */
	// case SCSIOP_INIT_ELEMENT_STATUS: /* 0x07 */ 
	case 0x07: return "SCSIOP_REASSIGN_BLOCKS"; //|SCSIOP_INIT_ELEMENT_STATUS";
	// case SCSIOP_READ6: /* 0x08 */ 
	// case SCSIOP_RECEIVE: /* 0x08 */ 
	case 0x08: return "SCSIOP_READ6"; // |SCSIOP_RECEIVE";
	// case SCSIOP_WRITE6: /* 0x0A */
	// case SCSIOP_PRINT: /* 0x0A */
	// case SCSIOP_SEND: /* 0x0A */ 
	case 0x0A: return "SCSIOP_WRITE6"; // |SCSIOP_PRINT|SCSIOP_SEND";
	// case SCSIOP_SEEK6: /* 0x0B */ return "SCSIOP_SEEK6";
	// case SCSIOP_TRACK_SELECT: /* 0x0B */ return "SCSIOP_TRACK_SELECT"
	// case SCSIOP_SLEW_PRINT: /* 0x0B */ return "SCSIOP_SLEW_PRINT";
	case 0x0B: return "SCSIOP_SEEK6"; // |SCSIOP_TRACK_SELECT|SCSIOP_SLEW_PRINT";
	case SCSIOP_SEEK_BLOCK: /* 0x0C */ return "SCSIOP_SEEK_BLOCK";
	case SCSIOP_PARTITION: /* 0x0D */ return "SCSIOP_PARTITION";
	case SCSIOP_READ_REVERSE: /* 0x0F */ return "SCSIOP_READ_REVERSE";
	// case SCSIOP_WRITE_FILEMARKS: /* 0x10 */
	// case SCSIOP_FLUSH_BUFFER: /* 0x10 */ 
	case 0x10: return "SCSIOP_WRITE_FILEMARKS"; // |SCSIOP_FLUSH_BUFFER";
	case SCSIOP_SPACE: /* 0x11 */ return "SCSIOP_SPACE";
	case SCSIOP_INQUIRY: /* 0x12 */ return "SCSIOP_INQUIRY";
	case SCSIOP_VERIFY6: /* 0x13 */ return "SCSIOP_VERIFY6";
	case SCSIOP_RECOVER_BUF_DATA: /* 0x14 */ return "SCSIOP_RECOVER_BUF_DATA";
	case SCSIOP_MODE_SELECT: /* 0x15 */ return "SCSIOP_MODE_SELECT";
	case SCSIOP_RESERVE_UNIT: /* 0x16 */ return "SCSIOP_RESERVE_UNIT";
	case SCSIOP_RELEASE_UNIT: /* 0x17 */ return "SCSIOP_RELEASE_UNIT";
	case SCSIOP_COPY: /* 0x18 */ return "SCSIOP_COPY";
	case SCSIOP_ERASE: /* 0x19 */ return "SCSIOP_ERASE";
	case SCSIOP_MODE_SENSE: /* 0x1A */ return "SCSIOP_MODE_SENSE";
	// case SCSIOP_START_STOP_UNIT: /* 0x1B */
	// case SCSIOP_STOP_PRINT: /* 0x1B */ 
	// case SCSIOP_LOAD_UNLOAD: /* 0x1B */
	case 0x1B: return "SCSIOP_START_STOP_UNIT"; // |SCSIOP_STOP_PRINT|SCSIOP_LOAD_UNLOAD";
	case SCSIOP_RECEIVE_DIAGNOSTIC: /* 0x1C */ return "SCSIOP_RECEIVE_DIAGNOSTIC";
	case SCSIOP_SEND_DIAGNOSTIC: /* 0x1D */ return "SCSIOP_SEND_DIAGNOSTIC";
	case SCSIOP_MEDIUM_REMOVAL: /* 0x1E */ return "SCSIOP_MEDIUM_REMOVAL";

	// 10-byte commands
	case SCSIOP_READ_FORMATTED_CAPACITY: /* 0x23 */ return "SCSIOP_READ_FORMATTED_CAPACITY";
	case SCSIOP_READ_CAPACITY: /* 0x25 */ return "SCSIOP_READ_CAPACITY";
	case SCSIOP_READ: /* 0x28 */ return "SCSIOP_READ";
	case SCSIOP_WRITE: /* 0x2A */ return "SCSIOP_WRITE";
	// case SCSIOP_SEEK: /* 0x2B */ return "SCSIOP_SEEK";
	// case SCSIOP_LOCATE: /* 0x2B */ return "SCSIOP_LOCATE";
	// case SCSIOP_POSITION_TO_ELEMENT: /* 0x2B */ return "SCSIOP_POSITION_TO_ELEMENT";
	case 0x2B: return "SCSIOP_SEEK"; // |SCSIOP_LOCATE|SCSIOP_POSITION_TO_ELEMENT";
	case SCSIOP_WRITE_VERIFY: /* 0x2E */ return "SCSIOP_WRITE_VERIFY";
	case SCSIOP_VERIFY: /* 0x2F */ return "SCSIOP_VERIFY";
	case SCSIOP_SEARCH_DATA_HIGH: /* 0x30 */ return "SCSIOP_SEARCH_DATA_HIGH";
	case SCSIOP_SEARCH_DATA_EQUAL: /* 0x31 */ return "SCSIOP_SEARCH_DATA_EQUAL";
	case SCSIOP_SEARCH_DATA_LOW: /* 0x32 */ return "SCSIOP_SEARCH_DATA_LOW";
	case SCSIOP_SET_LIMITS: /* 0x33 */ return "SCSIOP_SET_LIMITS";
	case SCSIOP_READ_POSITION: /* 0x34 */ return "SCSIOP_READ_POSITION";
	case SCSIOP_SYNCHRONIZE_CACHE: /* 0x35 */ return "SCSIOP_SYNCHRONIZE_CACHE";
	case SCSIOP_COMPARE: /* 0x39 */ return "SCSIOP_COMPARE";
	case SCSIOP_COPY_COMPARE: /* 0x3A */ return "SCSIOP_COPY_COMPARE";
	case SCSIOP_WRITE_DATA_BUFF: /* 0x3B */ return "SCSIOP_WRITE_DATA_BUFF";
	case SCSIOP_READ_DATA_BUFF: /* 0x3C */ return "SCSIOP_READ_DATA_BUFF";
	case SCSIOP_CHANGE_DEFINITION: /* 0x40 */ return "SCSIOP_CHANGE_DEFINITION";
	case SCSIOP_READ_SUB_CHANNEL: /* 0x42 */ return "SCSIOP_READ_SUB_CHANNEL";
	case SCSIOP_READ_TOC: /* 0x43 */ return "SCSIOP_READ_TOC";
	case SCSIOP_READ_HEADER: /* 0x44 */ return "SCSIOP_READ_HEADER";
	case SCSIOP_PLAY_AUDIO: /* 0x45 */ return "SCSIOP_PLAY_AUDIO";
	// case SCSIOP_GET_CONFIGURATION: /* 0x46 */ return "SCSIOP_GET_CONFIGURATION";
	case 0x46: /* 0x46 */ return "SCSIOP_GET_CONFIGURATION";
	case SCSIOP_PLAY_AUDIO_MSF: /* 0x47 */ return "SCSIOP_PLAY_AUDIO_MSF";
	case SCSIOP_PLAY_TRACK_INDEX: /* 0x48 */ return "SCSIOP_PLAY_TRACK_INDEX";
	case SCSIOP_PLAY_TRACK_RELATIVE: /* 0x49 */ return "SCSIOP_PLAY_TRACK_RELATIVE";
	// case SCSIOP_GET_EVENT_STATUS: /* 0x4A */ return "SCSIOP_GET_EVENT_STATUS";
	case 0x4A: /* 0x4A */ return "SCSIOP_GET_EVENT_STATUS";
	case SCSIOP_PAUSE_RESUME: /* 0x4B */ return "SCSIOP_PAUSE_RESUME";
	case SCSIOP_LOG_SELECT: /* 0x4C */ return "SCSIOP_LOG_SELECT";
	case SCSIOP_LOG_SENSE: /* 0x4D */ return "SCSIOP_LOG_SENSE";
	case SCSIOP_STOP_PLAY_SCAN: /* 0x4E */ return "SCSIOP_STOP_PLAY_SCAN";
	// case SCSIOP_READ_DISK_INFORMATION: /* 0x51 */ return "SCSIOP_READ_DISK_INFORMATION";
	// case SCSIOP_READ_DISC_INFORMATION: /* 0x51 */ return "SCSIOP_READ_DISC_INFORMATION";  // proper use of disc over disk
	case 0x51: return "SCSIOP_READ_DISC_INFORMATION";
	case SCSIOP_READ_TRACK_INFORMATION: /* 0x52 */ return "SCSIOP_READ_TRACK_INFORMATION";
	// case SCSIOP_RESERVE_TRACK_RZONE: /* 0x53 */ return "SCSIOP_RESERVE_TRACK_RZONE";
	case 0x53: /* 0x53 */ return "SCSIOP_RESERVE_TRACK_RZONE";
	// case SCSIOP_SEND_OPC_INFORMATION: /* 0x54 */ return "SCSIOP_SEND_OPC_INFORMATION";  // optimum power calibration
	case 0x54: /* 0x54 */ return "SCSIOP_SEND_OPC_INFORMATION";  // optimum power calibration
	case SCSIOP_MODE_SELECT10: /* 0x55 */ return "SCSIOP_MODE_SELECT10";
	// case SCSIOP_RESERVE_UNIT10: /* 0x56 */ return "SCSIOP_RESERVE_UNIT10";
	case 0x56: /* 0x56 */ return "SCSIOP_RESERVE_UNIT10";
	// case SCSIOP_RELEASE_UNIT10: /* 0x57 */ return "SCSIOP_RELEASE_UNIT10";
	case 0x57: /* 0x57 */ return "SCSIOP_RELEASE_UNIT10";
	case SCSIOP_MODE_SENSE10: /* 0x5A */ return "SCSIOP_MODE_SENSE10";
	// case SCSIOP_CLOSE_TRACK_SESSION: /* 0x5B */ return "SCSIOP_CLOSE_TRACK_SESSION";
	case 0x5B: /* 0x5B */ return "SCSIOP_CLOSE_TRACK_SESSION";
	// case SCSIOP_READ_BUFFER_CAPACITY: /* 0x5C */ return "SCSIOP_READ_BUFFER_CAPACITY";
	case 0x5C: /* 0x5C */ return "SCSIOP_READ_BUFFER_CAPACITY";
	// case SCSIOP_SEND_CUE_SHEET: /* 0x5D */ return "SCSIOP_SEND_CUE_SHEET";
	case 0x5D: /* 0x5D */ return "SCSIOP_SEND_CUE_SHEET";
	// case SCSIOP_PERSISTENT_RESERVE_IN: /* 0x5E */ return "SCSIOP_PERSISTENT_RESERVE_IN";
	case 0x5E: /* 0x5E */ return "SCSIOP_PERSISTENT_RESERVE_IN";
	// case SCSIOP_PERSISTENT_RESERVE_OUT: /* 0x5F */ return "SCSIOP_PERSISTENT_RESERVE_OUT";
	case 0x5F: /* 0x5F */ return "SCSIOP_PERSISTENT_RESERVE_OUT";

	// 12-byte commands
	case SCSIOP_REPORT_LUNS: /* 0xA0 */ return "SCSIOP_REPORT_LUNS";
	// case SCSIOP_BLANK: /* 0xA1 */ return "SCSIOP_BLANK";
	case 0xA1: /* 0xA1 */ return "SCSIOP_BLANK";
	// case SCSIOP_SEND_EVENT: /* 0xA2 */ return "SCSIOP_SEND_EVENT";
	case 0xA2: /* 0xA2 */ return "SCSIOP_SEND_EVENT";
	case SCSIOP_SEND_KEY: /* 0xA3 */ return "SCSIOP_SEND_KEY";
	case SCSIOP_REPORT_KEY: /* 0xA4 */ return "SCSIOP_REPORT_KEY";
	case SCSIOP_MOVE_MEDIUM: /* 0xA5 */ return "SCSIOP_MOVE_MEDIUM";
	// case SCSIOP_LOAD_UNLOAD_SLOT: /* 0xA6 */ return "SCSIOP_LOAD_UNLOAD_SLOT";
	// case SCSIOP_EXCHANGE_MEDIUM: /* 0xA6 */ return "SCSIOP_EXCHANGE_MEDIUM";
	case 0xA6: return "SCSIOP_LOAD_UNLOAD_SLOT"; // |SCSIOP_EXCHANGE_MEDIUM";
	case SCSIOP_SET_READ_AHEAD: /* 0xA7 */ return "SCSIOP_SET_READ_AHEAD";
	case SCSIOP_READ_DVD_STRUCTURE: /* 0xAD */ return "SCSIOP_READ_DVD_STRUCTURE";
	case SCSIOP_REQUEST_VOL_ELEMENT: /* 0xB5 */ return "SCSIOP_REQUEST_VOL_ELEMENT";
	case SCSIOP_SEND_VOLUME_TAG: /* 0xB6 */ return "SCSIOP_SEND_VOLUME_TAG";
	case SCSIOP_READ_ELEMENT_STATUS: /* 0xB8 */ return "SCSIOP_READ_ELEMENT_STATUS";
	case SCSIOP_READ_CD_MSF: /* 0xB9 */ return "SCSIOP_READ_CD_MSF";
	case SCSIOP_SCAN_CD: /* 0xBA */ return "SCSIOP_SCAN_CD";
	// case SCSIOP_SET_CD_SPEED: /* 0xBB */ return "SCSIOP_SET_CD_SPEED";
	case 0xBB: /* 0xBB */ return "SCSIOP_SET_CD_SPEED";
	case SCSIOP_PLAY_CD: /* 0xBC */ return "SCSIOP_PLAY_CD";
	case SCSIOP_MECHANISM_STATUS: /* 0xBD */ return "SCSIOP_MECHANISM_STATUS";
	case SCSIOP_READ_CD: /* 0xBE */ return "SCSIOP_READ_CD";
	// case SCSIOP_SEND_DVD_STRUCTURE: /* 0xBF */ return "SCSIOP_SEND_DVD_STRUCTURE";
	case 0xBF: /* 0xBF */ return "SCSIOP_SEND_DVD_STRUCTURE";
	case SCSIOP_INIT_ELEMENT_RANGE: /* 0xE7 */ return "SCSIOP_INIT_ELEMENT_RANGE";

	// 16-byte commands
	// case SCSIOP_READ16: /* 0x88 */ return "SCSIOP_READ16";
	case 0x88:  /* 0x88 */ return "SCSIOP_READ16";
	// case SCSIOP_WRITE16: /* 0x8A */ return "SCSIOP_WRITE16";
	case 0x8A: /* 0x8A */ return "SCSIOP_WRITE16";
	// case SCSIOP_VERIFY16: /* 0x8F */ return "SCSIOP_VERIFY16";
	case 0x8F: /* 0x8F */ return "SCSIOP_VERIFY16";
	// case SCSIOP_SYNCHRONIZE_CACHE16: /* 0x91 */ return "SCSIOP_SYNCHRONIZE_CACHE16";
	case 0x91: /* 0x91 */ return "SCSIOP_SYNCHRONIZE_CACHE16";
	// case SCSIOP_READ_CAPACITY16: /* 0x9E */ return "SCSIOP_READ_CAPACITY16";
	case 0x9E: /* 0x9E */ return "SCSIOP_READ_CAPACITY16";

	default: return "SCSIOP_?UNKNOWN?";
	}

}

/*++

Replace \#define[ \t]+{[A-Za-z_0-9]+}[ \t]+.*$
-> \tcase \1: return "\1";

--*/

CONST CHAR* DbgIoControlCodeStringA(ULONG IoControlCode)
{
	switch (IoControlCode)
	{
	case IOCTL_NDASPORT_PLUGIN_LOGICALUNIT: return "IOCTL_NDASPORT_LOGICALUNIT_PLUGIN";
	case IOCTL_NDASPORT_EJECT_LOGICALUNIT: return "IOCTL_NDASPORT_LOGICALUNIT_EJECT";
	case IOCTL_NDASPORT_UNPLUG_LOGICALUNIT: return "IOCTL_NDASPORT_LOGICALUNIT_UNPLUG";
	case IOCTL_NDASPORT_IS_LOGICALUNIT_ADDRESS_IN_USE: return "IOCTL_NDASPORT_IS_LOGICALUNIT_ADDRESS_IN_USE";
	case IOCTL_NDASPORT_GET_LOGICALUNIT_DESCRIPTOR: return "IOCTL_NDASPORT_GET_LOGICALUNIT_DESCRIPTOR";
	case IOCTL_NDASPORT_GET_PORT_NUMBER: return "IOCTL_NDASPORT_GET_PORT_NUMBER";
	case IOCTL_NDASPORT_LOGICALUNIT_EXIST: return "IOCTL_NDASPORT_LOGICALUNIT_EXIST";
	case IOCTL_NDASPORT_LOGICALUNIT_GET_ADDRESS: return "IOCTL_NDASPORT_LOGICALUNIT_GET_ADDRESS";
	case IOCTL_NDASPORT_LOGICALUNIT_GET_DESCRIPTOR: return "IOCTL_NDASPORT_LOGICALUNIT_GET_DESCRIPTOR";

	case IOCTL_STORAGE_CHECK_VERIFY: return "IOCTL_STORAGE_CHECK_VERIFY";
	case IOCTL_STORAGE_CHECK_VERIFY2: return "IOCTL_STORAGE_CHECK_VERIFY2";
	case IOCTL_STORAGE_MEDIA_REMOVAL: return "IOCTL_STORAGE_MEDIA_REMOVAL";
	case IOCTL_STORAGE_EJECT_MEDIA: return "IOCTL_STORAGE_EJECT_MEDIA";
	case IOCTL_STORAGE_LOAD_MEDIA: return "IOCTL_STORAGE_LOAD_MEDIA";
	case IOCTL_STORAGE_LOAD_MEDIA2: return "IOCTL_STORAGE_LOAD_MEDIA2";
	case IOCTL_STORAGE_RESERVE: return "IOCTL_STORAGE_RESERVE";
	case IOCTL_STORAGE_RELEASE: return "IOCTL_STORAGE_RELEASE";
	case IOCTL_STORAGE_FIND_NEW_DEVICES: return "IOCTL_STORAGE_FIND_NEW_DEVICES";
	case IOCTL_STORAGE_EJECTION_CONTROL: return "IOCTL_STORAGE_EJECTION_CONTROL";
	case IOCTL_STORAGE_MCN_CONTROL: return "IOCTL_STORAGE_MCN_CONTROL";
	case IOCTL_STORAGE_GET_MEDIA_TYPES: return "IOCTL_STORAGE_GET_MEDIA_TYPES";
	case IOCTL_STORAGE_GET_MEDIA_TYPES_EX: return "IOCTL_STORAGE_GET_MEDIA_TYPES_EX";
	case IOCTL_STORAGE_GET_MEDIA_SERIAL_NUMBER: return "IOCTL_STORAGE_GET_MEDIA_SERIAL_NUMBER";
	case IOCTL_STORAGE_GET_HOTPLUG_INFO: return "IOCTL_STORAGE_GET_HOTPLUG_INFO";
	case IOCTL_STORAGE_SET_HOTPLUG_INFO: return "IOCTL_STORAGE_SET_HOTPLUG_INFO";
	case IOCTL_STORAGE_RESET_BUS: return "IOCTL_STORAGE_RESET_BUS";
	case IOCTL_STORAGE_RESET_DEVICE: return "IOCTL_STORAGE_RESET_DEVICE";
	case IOCTL_STORAGE_BREAK_RESERVATION: return "IOCTL_STORAGE_BREAK_RESERVATION";
	case IOCTL_STORAGE_GET_DEVICE_NUMBER: return "IOCTL_STORAGE_GET_DEVICE_NUMBER";
	case IOCTL_STORAGE_PREDICT_FAILURE: return "IOCTL_STORAGE_PREDICT_FAILURE";
	case IOCTL_STORAGE_READ_CAPACITY: return "IOCTL_STORAGE_READ_CAPACITY";
	case IOCTL_STORAGE_QUERY_PROPERTY: return "IOCTL_STORAGE_QUERY_PROPERTY";
	case OBSOLETE_IOCTL_STORAGE_RESET_BUS:  return "OBSOLETE_IOCTL_STORAGE_RESET_BUS";
	case OBSOLETE_IOCTL_STORAGE_RESET_DEVICE: return "OBSOLETE_IOCTL_STORAGE_RESET_DEVICE";

	case IOCTL_SCSI_PASS_THROUGH: return "IOCTL_SCSI_PASS_THROUGH";
	case IOCTL_SCSI_MINIPORT: return "IOCTL_SCSI_MINIPORT";
	case IOCTL_SCSI_GET_INQUIRY_DATA: return "IOCTL_SCSI_GET_INQUIRY_DATA";
	case IOCTL_SCSI_GET_CAPABILITIES: return "IOCTL_SCSI_GET_CAPABILITIES";
	case IOCTL_SCSI_PASS_THROUGH_DIRECT: return "IOCTL_SCSI_PASS_THROUGH_DIRECT";
	case IOCTL_SCSI_GET_ADDRESS: return "IOCTL_SCSI_GET_ADDRESS";
	case IOCTL_SCSI_RESCAN_BUS: return "IOCTL_SCSI_RESCAN_BUS";
	case IOCTL_SCSI_GET_DUMP_POINTERS: return "IOCTL_SCSI_GET_DUMP_POINTERS";
	case IOCTL_SCSI_FREE_DUMP_POINTERS: return "IOCTL_SCSI_FREE_DUMP_POINTERS";
	case IOCTL_IDE_PASS_THROUGH: return "IOCTL_IDE_PASS_THROUGH";
	case IOCTL_ATA_PASS_THROUGH: return "IOCTL_ATA_PASS_THROUGH";
	case IOCTL_ATA_PASS_THROUGH_DIRECT: return "IOCTL_ATA_PASS_THROUGH_DIRECT";
	case IOCTL_DISK_GET_DRIVE_GEOMETRY: return "IOCTL_DISK_GET_DRIVE_GEOMETRY";
	case IOCTL_DISK_GET_PARTITION_INFO: return "IOCTL_DISK_GET_PARTITION_INFO";
	case IOCTL_DISK_SET_PARTITION_INFO: return "IOCTL_DISK_SET_PARTITION_INFO";
	case IOCTL_DISK_GET_DRIVE_LAYOUT: return "IOCTL_DISK_GET_DRIVE_LAYOUT";
	case IOCTL_DISK_SET_DRIVE_LAYOUT: return "IOCTL_DISK_SET_DRIVE_LAYOUT";
	case IOCTL_DISK_VERIFY: return "IOCTL_DISK_VERIFY";
	case IOCTL_DISK_FORMAT_TRACKS: return "IOCTL_DISK_FORMAT_TRACKS";
	case IOCTL_DISK_REASSIGN_BLOCKS: return "IOCTL_DISK_REASSIGN_BLOCKS";
	case IOCTL_DISK_PERFORMANCE: return "IOCTL_DISK_PERFORMANCE";
	case IOCTL_DISK_IS_WRITABLE: return "IOCTL_DISK_IS_WRITABLE";
	case IOCTL_DISK_LOGGING: return "IOCTL_DISK_LOGGING";
	case IOCTL_DISK_FORMAT_TRACKS_EX: return "IOCTL_DISK_FORMAT_TRACKS_EX";
	case IOCTL_DISK_HISTOGRAM_STRUCTURE: return "IOCTL_DISK_HISTOGRAM_STRUCTURE";
	case IOCTL_DISK_HISTOGRAM_DATA: return "IOCTL_DISK_HISTOGRAM_DATA";
	case IOCTL_DISK_HISTOGRAM_RESET: return "IOCTL_DISK_HISTOGRAM_RESET";
	case IOCTL_DISK_REQUEST_STRUCTURE: return "IOCTL_DISK_REQUEST_STRUCTURE";
	case IOCTL_DISK_REQUEST_DATA: return "IOCTL_DISK_REQUEST_DATA";
	case IOCTL_DISK_PERFORMANCE_OFF: return "IOCTL_DISK_PERFORMANCE_OFF";
	case IOCTL_DISK_CONTROLLER_NUMBER: return "IOCTL_DISK_CONTROLLER_NUMBER";
	case SMART_GET_VERSION: return "SMART_GET_VERSION";
	case SMART_SEND_DRIVE_COMMAND: return "SMART_SEND_DRIVE_COMMAND";
	case SMART_RCV_DRIVE_DATA: return "SMART_RCV_DRIVE_DATA";
	case IOCTL_DISK_GET_PARTITION_INFO_EX: return "IOCTL_DISK_GET_PARTITION_INFO_EX";
	case IOCTL_DISK_SET_PARTITION_INFO_EX: return "IOCTL_DISK_SET_PARTITION_INFO_EX";
	case IOCTL_DISK_GET_DRIVE_LAYOUT_EX: return "IOCTL_DISK_GET_DRIVE_LAYOUT_EX";
	case IOCTL_DISK_SET_DRIVE_LAYOUT_EX: return "IOCTL_DISK_SET_DRIVE_LAYOUT_EX";
	case IOCTL_DISK_CREATE_DISK: return "IOCTL_DISK_CREATE_DISK";
	case IOCTL_DISK_GET_LENGTH_INFO: return "IOCTL_DISK_GET_LENGTH_INFO";
	case IOCTL_DISK_GET_DRIVE_GEOMETRY_EX: return "IOCTL_DISK_GET_DRIVE_GEOMETRY_EX";
	case IOCTL_DISK_REASSIGN_BLOCKS_EX: return "IOCTL_DISK_REASSIGN_BLOCKS_EX";
	case IOCTL_DISK_UPDATE_DRIVE_SIZE: return "IOCTL_DISK_UPDATE_DRIVE_SIZE";
	case IOCTL_DISK_GROW_PARTITION: return "IOCTL_DISK_GROW_PARTITION";
	case IOCTL_DISK_GET_CACHE_INFORMATION: return "IOCTL_DISK_GET_CACHE_INFORMATION";
	case IOCTL_DISK_SET_CACHE_INFORMATION: return "IOCTL_DISK_SET_CACHE_INFORMATION";
	case OBSOLETE_DISK_GET_WRITE_CACHE_STATE: return "OBSOLETE_DISK_GET_WRITE_CACHE_STATE";
	case IOCTL_DISK_DELETE_DRIVE_LAYOUT: return "IOCTL_DISK_DELETE_DRIVE_LAYOUT";
	case IOCTL_DISK_UPDATE_PROPERTIES: return "IOCTL_DISK_UPDATE_PROPERTIES";
	case IOCTL_DISK_FORMAT_DRIVE: return "IOCTL_DISK_FORMAT_DRIVE";
	case IOCTL_DISK_SENSE_DEVICE: return "IOCTL_DISK_SENSE_DEVICE";
	case IOCTL_DISK_GET_CACHE_SETTING: return "IOCTL_DISK_GET_CACHE_SETTING";
	case IOCTL_DISK_SET_CACHE_SETTING: return "IOCTL_DISK_SET_CACHE_SETTING";
	case IOCTL_DISK_COPY_DATA: return "IOCTL_DISK_COPY_DATA";
	case IOCTL_DISK_INTERNAL_SET_VERIFY: return "IOCTL_DISK_INTERNAL_SET_VERIFY";
	case IOCTL_DISK_INTERNAL_CLEAR_VERIFY: return "IOCTL_DISK_INTERNAL_CLEAR_VERIFY";
	case IOCTL_DISK_INTERNAL_SET_NOTIFY: return "IOCTL_DISK_INTERNAL_SET_NOTIFY";
	case IOCTL_DISK_CHECK_VERIFY: return "IOCTL_DISK_CHECK_VERIFY";
	case IOCTL_DISK_MEDIA_REMOVAL: return "IOCTL_DISK_MEDIA_REMOVAL";
	case IOCTL_DISK_EJECT_MEDIA: return "IOCTL_DISK_EJECT_MEDIA";
	case IOCTL_DISK_LOAD_MEDIA: return "IOCTL_DISK_LOAD_MEDIA";
	case IOCTL_DISK_RESERVE: return "IOCTL_DISK_RESERVE";
	case IOCTL_DISK_RELEASE: return "IOCTL_DISK_RELEASE";
	case IOCTL_DISK_FIND_NEW_DEVICES: return "IOCTL_DISK_FIND_NEW_DEVICES";
	case IOCTL_DISK_GET_MEDIA_TYPES: return "IOCTL_DISK_GET_MEDIA_TYPES";
	default: return "IOCTL_?UNKNOWN?";
	}
}

CONST CHAR* DbgScsiMiniportIoControlCodeString(ULONG IoControlCode)
{
	switch (IoControlCode)
	{
	case IOCTL_SCSI_MINIPORT_SMART_VERSION: return "IOCTL_SCSI_MINIPORT_SMART_VERSION";
	case IOCTL_SCSI_MINIPORT_IDENTIFY: return "IOCTL_SCSI_MINIPORT_IDENTIFY";
	case IOCTL_SCSI_MINIPORT_READ_SMART_ATTRIBS: return "IOCTL_SCSI_MINIPORT_READ_SMART_ATTRIBS";
	case IOCTL_SCSI_MINIPORT_READ_SMART_THRESHOLDS: return "IOCTL_SCSI_MINIPORT_READ_SMART_THRESHOLDS";
	case IOCTL_SCSI_MINIPORT_ENABLE_SMART: return "IOCTL_SCSI_MINIPORT_ENABLE_SMART";
	case IOCTL_SCSI_MINIPORT_DISABLE_SMART: return "IOCTL_SCSI_MINIPORT_DISABLE_SMART";
	case IOCTL_SCSI_MINIPORT_RETURN_STATUS: return "IOCTL_SCSI_MINIPORT_RETURN_STATUS";
	case IOCTL_SCSI_MINIPORT_ENABLE_DISABLE_AUTOSAVE: return "IOCTL_SCSI_MINIPORT_ENABLE_DISABLE_AUTOSAVE";
	case IOCTL_SCSI_MINIPORT_SAVE_ATTRIBUTE_VALUES: return "IOCTL_SCSI_MINIPORT_SAVE_ATTRIBUTE_VALUES";
	case IOCTL_SCSI_MINIPORT_EXECUTE_OFFLINE_DIAGS: return "IOCTL_SCSI_MINIPORT_EXECUTE_OFFLINE_DIAGS";
	case IOCTL_SCSI_MINIPORT_ENABLE_DISABLE_AUTO_OFFLINE: return "IOCTL_SCSI_MINIPORT_ENABLE_DISABLE_AUTO_OFFLINE";
	case IOCTL_SCSI_MINIPORT_READ_SMART_LOG: return "IOCTL_SCSI_MINIPORT_READ_SMART_LOG";
	case IOCTL_SCSI_MINIPORT_WRITE_SMART_LOG: return "IOCTL_SCSI_MINIPORT_WRITE_SMART_LOG";
	case IOCTL_SCSI_MINIPORT_NOT_QUORUM_CAPABLE: return "IOCTL_SCSI_MINIPORT_NOT_QUORUM_CAPABLE";
	case IOCTL_SCSI_MINIPORT_NOT_CLUSTER_CAPABLE: return "IOCTL_SCSI_MINIPORT_NOT_CLUSTER_CAPABLE";
	default: return "IOCTL_?UNKNOWN?";
	}
}


CONST CHAR* DbgWmiMinorFunctionStringA(UCHAR MinorFunction)
{
    switch (MinorFunction)
    {
	case IRP_MN_QUERY_ALL_DATA: return "IRP_MN_QUERY_ALL_DATA";
	case IRP_MN_QUERY_SINGLE_INSTANCE: return "IRP_MN_QUERY_SINGLE_INSTANCE";
	case IRP_MN_CHANGE_SINGLE_INSTANCE: return "IRP_MN_CHANGE_SINGLE_INSTANCE";
	case IRP_MN_CHANGE_SINGLE_ITEM: return "IRP_MN_CHANGE_SINGLE_ITEM";
	case IRP_MN_ENABLE_EVENTS: return "IRP_MN_ENABLE_EVENTS";
	case IRP_MN_DISABLE_EVENTS: return "IRP_MN_DISABLE_EVENTS";
	case IRP_MN_ENABLE_COLLECTION: return "IRP_MN_ENABLE_COLLECTION";
	case IRP_MN_DISABLE_COLLECTION: return "IRP_MN_DISABLE_COLLECTION";
	case IRP_MN_REGINFO: return "IRP_MN_REGINFO";
	case IRP_MN_EXECUTE_METHOD: return "IRP_MN_EXECUTE_METHOD";
	case 0x0b /*IRP_MN_REGINFO_EX*/: return "IRP_MN_REGINFO_EX";
	default: return "SYSCTL_IRP_MN_?UNKNOWN?";
    }
}

//
// Replace Regex:
// \#define[ \t]+{[A-Za-z_0-9]+}[ \t]+{0x[0-9A-Za-z]+}[ \t]*$
// \tcase \1: /* \2 */ return "\1";
//

CONST CHAR* DbgSrbStatusStringA(UCHAR SrbStatus)
{
	switch (SrbStatus) {
	case SRB_STATUS_PENDING: /* 0x00 */ return "SRB_STATUS_PENDING";
	case SRB_STATUS_SUCCESS: /* 0x01 */ return "SRB_STATUS_SUCCESS";
	case SRB_STATUS_ABORTED: /* 0x02 */ return "SRB_STATUS_ABORTED";
	case SRB_STATUS_ABORT_FAILED: /* 0x03 */ return "SRB_STATUS_ABORT_FAILED";
	case SRB_STATUS_ERROR: /* 0x04 */ return "SRB_STATUS_ERROR";
	case SRB_STATUS_BUSY: /* 0x05 */ return "SRB_STATUS_BUSY";
	case SRB_STATUS_INVALID_REQUEST: /* 0x06 */ return "SRB_STATUS_INVALID_REQUEST";
	case SRB_STATUS_INVALID_PATH_ID: /* 0x07 */ return "SRB_STATUS_INVALID_PATH_ID";
	case SRB_STATUS_NO_DEVICE: /* 0x08 */ return "SRB_STATUS_NO_DEVICE";
	case SRB_STATUS_TIMEOUT: /* 0x09 */ return "SRB_STATUS_TIMEOUT";
	case SRB_STATUS_SELECTION_TIMEOUT: /* 0x0A */ return "SRB_STATUS_SELECTION_TIMEOUT";
	case SRB_STATUS_COMMAND_TIMEOUT: /* 0x0B */ return "SRB_STATUS_COMMAND_TIMEOUT";
	case SRB_STATUS_MESSAGE_REJECTED: /* 0x0D */ return "SRB_STATUS_MESSAGE_REJECTED";
	case SRB_STATUS_BUS_RESET: /* 0x0E */ return "SRB_STATUS_BUS_RESET";
	case SRB_STATUS_PARITY_ERROR: /* 0x0F */ return "SRB_STATUS_PARITY_ERROR";
	case SRB_STATUS_REQUEST_SENSE_FAILED: /* 0x10 */ return "SRB_STATUS_REQUEST_SENSE_FAILED";
	case SRB_STATUS_NO_HBA: /* 0x11 */ return "SRB_STATUS_NO_HBA";
	case SRB_STATUS_DATA_OVERRUN: /* 0x12 */ return "SRB_STATUS_DATA_OVERRUN";
	case SRB_STATUS_UNEXPECTED_BUS_FREE: /* 0x13 */ return "SRB_STATUS_UNEXPECTED_BUS_FREE";
	case SRB_STATUS_PHASE_SEQUENCE_FAILURE: /* 0x14 */ return "SRB_STATUS_PHASE_SEQUENCE_FAILURE";
	case SRB_STATUS_BAD_SRB_BLOCK_LENGTH: /* 0x15 */ return "SRB_STATUS_BAD_SRB_BLOCK_LENGTH";
	case SRB_STATUS_REQUEST_FLUSHED: /* 0x16 */ return "SRB_STATUS_REQUEST_FLUSHED";
	case SRB_STATUS_INVALID_LUN: /* 0x20 */ return "SRB_STATUS_INVALID_LUN";
	case SRB_STATUS_INVALID_TARGET_ID: /* 0x21 */ return "SRB_STATUS_INVALID_TARGET_ID";
	case SRB_STATUS_BAD_FUNCTION: /* 0x22 */ return "SRB_STATUS_BAD_FUNCTION";
	case SRB_STATUS_ERROR_RECOVERY: /* 0x23 */ return "SRB_STATUS_ERROR_RECOVERY";
	case SRB_STATUS_NOT_POWERED: /* 0x24 */ return "SRB_STATUS_NOT_POWERED";
	default: return "SRB_STATUS_?UNKNOWN?";
	}
}


CONST CHAR* DbgSenseCodeStringA(PSCSI_REQUEST_BLOCK Srb)
{
    CHAR* senseCodeStr = "?";

    if (Srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID)
	{
        PSENSE_DATA senseData;
        UCHAR senseCode;

        ASSERT(Srb->SenseInfoBuffer);
        senseData = (PSENSE_DATA) Srb->SenseInfoBuffer;
        senseCode = senseData->SenseKey & 0xf;
                    
        switch (senseCode)
		{
        #undef MAKE_CASE             
        #define MAKE_CASE(snsCod) case snsCod: senseCodeStr = #snsCod; break;
    
        MAKE_CASE(SCSI_SENSE_NO_SENSE)
        MAKE_CASE(SCSI_SENSE_RECOVERED_ERROR)
        MAKE_CASE(SCSI_SENSE_NOT_READY)
        MAKE_CASE(SCSI_SENSE_MEDIUM_ERROR)
        MAKE_CASE(SCSI_SENSE_HARDWARE_ERROR)
        MAKE_CASE(SCSI_SENSE_ILLEGAL_REQUEST)
        MAKE_CASE(SCSI_SENSE_UNIT_ATTENTION)
        MAKE_CASE(SCSI_SENSE_DATA_PROTECT)
        MAKE_CASE(SCSI_SENSE_BLANK_CHECK)
        MAKE_CASE(SCSI_SENSE_UNIQUE)
        MAKE_CASE(SCSI_SENSE_COPY_ABORTED)
        MAKE_CASE(SCSI_SENSE_ABORTED_COMMAND)
        MAKE_CASE(SCSI_SENSE_EQUAL)
        MAKE_CASE(SCSI_SENSE_VOL_OVERFLOW)
        MAKE_CASE(SCSI_SENSE_MISCOMPARE)
        MAKE_CASE(SCSI_SENSE_RESERVED)               
        }
    }

    return senseCodeStr;
}

CONST CHAR* DbgAdditionalSenseCodeStringA(PSCSI_REQUEST_BLOCK Srb)
{
    char *adSenseCodeStr = "?";
    
    if (Srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID)
	{
        PSENSE_DATA senseData;
        UCHAR adSenseCode;

        ASSERT(Srb->SenseInfoBuffer);
        senseData = (PSENSE_DATA) Srb->SenseInfoBuffer;
        adSenseCode = senseData->AdditionalSenseCode;
                    
        switch (adSenseCode)
		{

        #undef MAKE_CASE             
        #define MAKE_CASE(adSnsCod) case adSnsCod: adSenseCodeStr = #adSnsCod; break;

        MAKE_CASE(SCSI_ADSENSE_NO_SENSE)
        MAKE_CASE(SCSI_ADSENSE_LUN_NOT_READY)
        MAKE_CASE(SCSI_ADSENSE_TRACK_ERROR)
        MAKE_CASE(SCSI_ADSENSE_SEEK_ERROR)
        MAKE_CASE(SCSI_ADSENSE_REC_DATA_NOECC)
        MAKE_CASE(SCSI_ADSENSE_REC_DATA_ECC)
        MAKE_CASE(SCSI_ADSENSE_ILLEGAL_COMMAND)
        MAKE_CASE(SCSI_ADSENSE_ILLEGAL_BLOCK)
        MAKE_CASE(SCSI_ADSENSE_INVALID_CDB)
        MAKE_CASE(SCSI_ADSENSE_INVALID_LUN)
        MAKE_CASE(SCSI_ADSENSE_WRITE_PROTECT)   // aka SCSI_ADWRITE_PROTECT
        MAKE_CASE(SCSI_ADSENSE_MEDIUM_CHANGED)
        MAKE_CASE(SCSI_ADSENSE_BUS_RESET)
        MAKE_CASE(SCSI_ADSENSE_INVALID_MEDIA)
        MAKE_CASE(SCSI_ADSENSE_NO_MEDIA_IN_DEVICE)
        MAKE_CASE(SCSI_ADSENSE_POSITION_ERROR)
        MAKE_CASE(SCSI_ADSENSE_OPERATOR_REQUEST)
        MAKE_CASE(SCSI_ADSENSE_FAILURE_PREDICTION_THRESHOLD_EXCEEDED)
        MAKE_CASE(SCSI_ADSENSE_COPY_PROTECTION_FAILURE)
        MAKE_CASE(SCSI_ADSENSE_VENDOR_UNIQUE)
        MAKE_CASE(SCSI_ADSENSE_MUSIC_AREA)
        MAKE_CASE(SCSI_ADSENSE_DATA_AREA)
        MAKE_CASE(SCSI_ADSENSE_VOLUME_OVERFLOW)
        }
    }

    return adSenseCodeStr;
}

CONST CHAR* DbgAdditionalSenseCodeQualifierStringA(PSCSI_REQUEST_BLOCK Srb)
{
    char *adSenseCodeQualStr = "?";
    
    if (Srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID)
	{
        PSENSE_DATA senseData;
        UCHAR adSenseCode;
        UCHAR adSenseCodeQual;
        
        ASSERT(Srb->SenseInfoBuffer);
        senseData = (PSENSE_DATA) Srb->SenseInfoBuffer;
        adSenseCode = senseData->AdditionalSenseCode;
        adSenseCodeQual = senseData->AdditionalSenseCodeQualifier;
        
        switch (adSenseCode)
		{

        #undef MAKE_CASE             
        #define MAKE_CASE(adSnsCodQual) case adSnsCodQual: adSenseCodeQualStr = #adSnsCodQual; break;

        case SCSI_ADSENSE_LUN_NOT_READY:
            switch (adSenseCodeQual)
			{
            MAKE_CASE(SCSI_SENSEQ_CAUSE_NOT_REPORTABLE)
            MAKE_CASE(SCSI_SENSEQ_BECOMING_READY)
            MAKE_CASE(SCSI_SENSEQ_INIT_COMMAND_REQUIRED)
            MAKE_CASE(SCSI_SENSEQ_MANUAL_INTERVENTION_REQUIRED)
            MAKE_CASE(SCSI_SENSEQ_FORMAT_IN_PROGRESS)
            MAKE_CASE(SCSI_SENSEQ_REBUILD_IN_PROGRESS)
            MAKE_CASE(SCSI_SENSEQ_RECALCULATION_IN_PROGRESS)
            MAKE_CASE(SCSI_SENSEQ_OPERATION_IN_PROGRESS)
            MAKE_CASE(SCSI_SENSEQ_LONG_WRITE_IN_PROGRESS)                        
            }
            break;
        case SCSI_ADSENSE_NO_SENSE:
            switch (adSenseCodeQual)
			{
            MAKE_CASE(SCSI_SENSEQ_FILEMARK_DETECTED)
            MAKE_CASE(SCSI_SENSEQ_END_OF_MEDIA_DETECTED)
            MAKE_CASE(SCSI_SENSEQ_SETMARK_DETECTED)
            MAKE_CASE(SCSI_SENSEQ_BEGINNING_OF_MEDIA_DETECTED)
            }
            break;
        case SCSI_ADSENSE_ILLEGAL_BLOCK:
            switch (adSenseCodeQual)
			{
            MAKE_CASE(SCSI_SENSEQ_ILLEGAL_ELEMENT_ADDR)
            }
            break;
        case SCSI_ADSENSE_POSITION_ERROR:
            switch (adSenseCodeQual)
			{
            MAKE_CASE(SCSI_SENSEQ_DESTINATION_FULL)
            MAKE_CASE(SCSI_SENSEQ_SOURCE_EMPTY)
            }
            break;
        case SCSI_ADSENSE_INVALID_MEDIA:
            switch (adSenseCodeQual)
			{
            MAKE_CASE(SCSI_SENSEQ_INCOMPATIBLE_MEDIA_INSTALLED)
            MAKE_CASE(SCSI_SENSEQ_UNKNOWN_FORMAT)
            MAKE_CASE(SCSI_SENSEQ_INCOMPATIBLE_FORMAT)
            MAKE_CASE(SCSI_SENSEQ_CLEANING_CARTRIDGE_INSTALLED)
            }
            break;
        case SCSI_ADSENSE_OPERATOR_REQUEST:
            switch (adSenseCodeQual)
			{
            MAKE_CASE(SCSI_SENSEQ_STATE_CHANGE_INPUT)
            MAKE_CASE(SCSI_SENSEQ_MEDIUM_REMOVAL)
            MAKE_CASE(SCSI_SENSEQ_WRITE_PROTECT_ENABLE)
            MAKE_CASE(SCSI_SENSEQ_WRITE_PROTECT_DISABLE)
            }
            break;
        case SCSI_ADSENSE_COPY_PROTECTION_FAILURE:
            switch (adSenseCodeQual)
			{
            MAKE_CASE(SCSI_SENSEQ_AUTHENTICATION_FAILURE)
            MAKE_CASE(SCSI_SENSEQ_KEY_NOT_PRESENT)
            MAKE_CASE(SCSI_SENSEQ_KEY_NOT_ESTABLISHED)
            MAKE_CASE(SCSI_SENSEQ_READ_OF_SCRAMBLED_SECTOR_WITHOUT_AUTHENTICATION)
            MAKE_CASE(SCSI_SENSEQ_MEDIA_CODE_MISMATCHED_TO_LOGICAL_UNIT)
            MAKE_CASE(SCSI_SENSEQ_LOGICAL_UNIT_RESET_COUNT_ERROR)
            }
            break;
        }
    }

    return adSenseCodeQualStr;
}

/*
 *  DbgCheckReturnedPkt
 *
 *      Check a completed TRANSFER_PACKET for all sorts of error conditions
 *      and warn/trap appropriately.
 */

#define DBG_SCSIOP_STRING(x) DbgScsiOpStringA(x)
#define DBG_SRBSTATUS_STRING(x) DbgSrbStatusStringA(x)
#define DBG_SENSECODE_STRING(x) DbgSenseCodeStringA(x)
#define DBG_ADSENSECODE_STRING(x) DbgAdditionalSenseCodeStringA(x)
#define DBG_ADSENSECODEQUAL_STRING(x) DbgAdditionalSenseCodeQualifierStringA(x)

VOID 
DbgCheckScsiReturn(
	__in PIRP Irp, 
	__in PSCSI_REQUEST_BLOCK Srb)
{
    PCDB Cdb = (PCDB) Srb->Cdb;
    
    ASSERT(Srb->OriginalRequest == Irp);
    // ASSERT(Srb->DataBuffer == BufPtrCopy);
    // ASSERT(Srb->DataTransferLength <= BufLenCopy);
    // ASSERT(!Irp->CancelRoutine);
        
    if (SRB_STATUS(Srb->SrbStatus) == SRB_STATUS_PENDING)
	{
        DebugPrint((1, "SRB completed with status PENDING in Irp %ph: (op=%s srbstat=%s(%xh), irpstat=%xh)\n",
			Irp, 
			DBG_SCSIOP_STRING(Srb->Cdb[0]), 
			DBG_SRBSTATUS_STRING(Srb->SrbStatus), 
			(ULONG)Srb->SrbStatus, 
			Irp->IoStatus.Status));
    }
    else if (SRB_STATUS(Srb->SrbStatus) == SRB_STATUS_SUCCESS)
	{
        /*
         *  Make sure SRB and IRP status match.
         */
        if (!NT_SUCCESS(Irp->IoStatus.Status))
		{
            DebugPrint((2, "SRB and IRP status don't match in Irp %ph: (op=%s srbstat=%s(%xh), irpstat=%xh)\n",
				Irp, 
				DBG_SCSIOP_STRING(Srb->Cdb[0]), 
                DBG_SRBSTATUS_STRING(Srb->SrbStatus), 
				(ULONG)Srb->SrbStatus, 
                Irp->IoStatus.Status));
        }

        if (Irp->IoStatus.Information != Srb->DataTransferLength){
            DebugPrint((1, "SRB and IRP result transfer lengths don't match in succeeded Irp %ph: (op=%s, SrbStatus=%s, Srb.DataTransferLength=%xh, Irp->IoStatus.Information=%xh).\n",
				Irp, 
                DBG_SCSIOP_STRING(Srb->Cdb[0]),
				DBG_SRBSTATUS_STRING(Srb->SrbStatus),
                Srb->DataTransferLength, 
				Irp->IoStatus.Information));
        }            
    }
    else 
	{
        if (NT_SUCCESS(Irp->IoStatus.Status))
		{
            DebugPrint((2, "SRB and IRP status don't match in Irp %ph: (op=%s srbstat=%s(%xh), irpstat=%xh)\n",
				Irp, 
				DBG_SCSIOP_STRING(Srb->Cdb[0]), 
                DBG_SRBSTATUS_STRING(Srb->SrbStatus),
                (ULONG)Srb->SrbStatus, 
                Irp->IoStatus.Status));
        }            
        DebugPrint((3, "Irp %p failed (op=%s srbstat=%s(%xh), irpstat=%xh, sense=%s/%s/%s)\n", 
			Irp, 
			DBG_SCSIOP_STRING(Srb->Cdb[0]), 
            DBG_SRBSTATUS_STRING(Srb->SrbStatus),
            (ULONG)Srb->SrbStatus, 
            Irp->IoStatus.Status, 
            DBG_SENSECODE_STRING(Srb), 
            DBG_ADSENSECODE_STRING(Srb), 
            DBG_ADSENSECODEQUAL_STRING(Srb)));

        /*
         *  If the SRB failed with underrun or overrun, then the actual
         *  transferred length should be returned in both SRB and IRP.
         *  (SRB's only have an error status for overrun, so it's overloaded).
         */
        if ((SRB_STATUS(Srb->SrbStatus) == SRB_STATUS_DATA_OVERRUN) &&
           (Irp->IoStatus.Information != Srb->DataTransferLength))
		{
            DebugPrint((1, "SRB and IRP result transfer lengths don't match in failed Irp %ph: (op=%s, SrbStatus=%s, Srb.DataTransferLength=%xh, Irp->IoStatus.Information=%xh).\n", 
				Irp, DBG_SCSIOP_STRING(Srb->Cdb[0]), 
				DBG_SRBSTATUS_STRING(Srb->SrbStatus),
                Srb->DataTransferLength,
                Irp->IoStatus.Information));
        }            
    }


    /*
     *  Some miniport drivers have been caught changing the SCSI operation
     *  code in the SRB.  This is absolutely disallowed as it breaks our error handling.
     */
    switch (Cdb->CDB10.OperationCode)
	{
    case SCSIOP_MEDIUM_REMOVAL:
    case SCSIOP_MODE_SENSE:
    case SCSIOP_READ_CAPACITY:
    case SCSIOP_READ:
    case SCSIOP_WRITE:
    case SCSIOP_START_STOP_UNIT:    
        break;
    default:
   //     DebugPrint((1, "Miniport illegally changed Srb.Cdb.OperationCode in Irp %ph failed (op=%s srbstat=%s(%xh), irpstat=%xh, sense=%s/%s/%s)\n", 
			//Irp, 
			//DBG_SCSIOP_STRING(Srb->Cdb[0]), 
			//DBG_SRBSTATUS_STRING(Srb->SrbStatus),
			//(ULONG)Srb->SrbStatus, 
			//Irp->IoStatus.Status, 
			//DBG_SENSECODE_STRING(Srb), 
			//DBG_ADSENSECODE_STRING(Srb), 
			//DBG_ADSENSECODEQUAL_STRING(Srb)));
    break;
    }
    
}

CONST CHAR*
AtaMinorVersionNumberString(
	__in USHORT MinorVersionNumber)
{
	static CONST CHAR* Table[] = {
		"Prior to ATA-3 Standard or Unknown",
		/* 0001h */ "Obsolete",
		/* 0002h */ "Obsolete",
		/* 0003h */ "Obsolete",
		/* 0004h */ "Obsolete",
		/* 0005h */ "Obsolete",
		/* 0006h */ "Obsolete",
		/* 0007h */ "Obsolete",
		/* 0008h */ "Obsolete",
		/* 0009h */ "Obsolete",
		/* 000Ah */ "Obsolete",
		/* 000Bh */ "Obsolete",
		/* 000Ch */ "Obsolete",
		/* 000Dh */ "ATA/ATAPI-4 X3T13 1153D revision 6",
		/* 000Eh */ "ATA/ATAPI-4 T13 1153D revision 13",
		/* 000Fh */ "ATA/ATAPI-4 X3T13 1153D revision 7",
		/* 0010h */ "ATA/ATAPI-4 T13 1153D revision 18",
		/* 0011h */ "ATA/ATAPI-4 T13 1153D revision 15",
		/* 0012h */ "ATA/ATAPI-4 published, ANSI INCITS 317-1998",
		/* 0013h */ "ATA/ATAPI-5 T13 1321D revision 3",
		/* 0014h */ "ATA/ATAPI-4 T13 1153D revision 14",
		/* 0015h */ "ATA/ATAPI-5 T13 1321D revision 1",
		/* 0016h */ "ATA/ATAPI-5 published, ANSI INCITS 340-2000",
		/* 0017h */ "ATA/ATAPI-4 T13 1153D revision 17",
		/* 0018h */ "ATA/ATAPI-6 T13 1410D revision 0",
		/* 0019h */ "ATA/ATAPI-6 T13 1410D revision 3a",
		/* 001Ah */ "ATA/ATAPI-7 T13 1532D revision 1",
		/* 001Bh */ "ATA/ATAPI-6 T13 1410D revision 2",
		/* 001Ch */ "ATA/ATAPI-6 T13 1410D revision 1",
		/* 001Dh */ "Reserved",
		/* 001Eh */ "ATA/ATAPI-7 T13 1532D revision 0",
		/* 001Fh */ "Reserved",
		/* 0020h */ "Reserved",
		/* 0021h */ "ATA/ATAPI-7 T13 1532D revision 4a",
		/* 0022h */ "ATA/ATAPI-6 published, ANSI INCITS 361-2002",
		/* 0023h - FFFFh */ "Reserved"
	};

	if (MinorVersionNumber < countof(Table)) return Table[MinorVersionNumber];
	else if (0xFFFF == MinorVersionNumber) return Table[0];
	else return "Reserved";
}


