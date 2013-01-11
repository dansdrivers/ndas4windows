#pragma once
#include <ntddk.h>
#include <tdi.h>
#include <tdikrnl.h>

#ifdef __BUILDMACHINE__
#define STR2(x) #x
#define STR(x) STR2(x)
#define __BUILDMACHINE_STR__ STR(__BUILDMACHINE__)
#else
#define __BUILDMACHINE_STR__ "(none)"
#endif

#ifdef countof
#undef countof
#endif
#define countof(x) (sizeof(x) / sizeof((x)[0]))
#define min(a,b)    (((a) < (b)) ? (a) : (b))

#ifndef ExFreePoolWithTag
#define ExFreePoolWithTag(POINTER, TAG) ExFreePool(POINTER)
#endif

FORCEINLINE 
VOID 
pClearFlag(ULONG* Flags, ULONG Bit) 
{ 
	*Flags &= ~Bit; 
}

FORCEINLINE 
VOID 
pSetFlag(ULONG* Flags, ULONG Bit)
{ 
	*Flags |= Bit; 
}

FORCEINLINE 
BOOLEAN 
pTestFlag(ULONG Flags, ULONG Bit) 
{ 
	return (Flags & Bit) != 0; 
}

//
// Calculate the size of a field in a structure of type type, without
// knowing or stating the type of the field.
//
#ifndef RTL_FIELD_SIZE
#define RTL_FIELD_SIZE(type, field) (sizeof(((type *)0)->field))
#endif

//
// Calculate the size of a structure of type type up through and
// including a field.
//
#ifndef RTL_SIZEOF_THROUGH_FIELD
#define RTL_SIZEOF_THROUGH_FIELD(type, field) \
	(FIELD_OFFSET(type, field) + RTL_FIELD_SIZE(type, field))
#endif

/*++

Routine Description:

This routine translates an srb status into an ntstatus.

Arguments:

Srb - Supplies a pointer to the failing Srb.

Return Value:

An nt status approprate for the error.

--*/
FORCEINLINE
NTSTATUS
NpTranslateScsiStatus(
    IN PSCSI_REQUEST_BLOCK Srb)
{
    switch (SRB_STATUS(Srb->SrbStatus)) 
	{
	case SRB_STATUS_PENDING: return SRB_STATUS_PENDING;
	case SRB_STATUS_INTERNAL_ERROR: return Srb->InternalStatus;
	case SRB_STATUS_SUCCESS:              return STATUS_SUCCESS;
	//case SRB_STATUS_PENDING:              return STATUS_PENDING;
    case SRB_STATUS_INVALID_LUN:
    case SRB_STATUS_INVALID_TARGET_ID: 
	case SRB_STATUS_NO_DEVICE:
    case SRB_STATUS_NO_HBA:		          return STATUS_DEVICE_DOES_NOT_EXIST;
	case SRB_STATUS_COMMAND_TIMEOUT:
    case SRB_STATUS_TIMEOUT:	          return STATUS_IO_TIMEOUT;
    case SRB_STATUS_SELECTION_TIMEOUT:    return STATUS_DEVICE_NOT_CONNECTED;
	case SRB_STATUS_BAD_FUNCTION:
    case SRB_STATUS_BAD_SRB_BLOCK_LENGTH: return STATUS_INVALID_DEVICE_REQUEST;
	case SRB_STATUS_DATA_OVERRUN:         return STATUS_BUFFER_OVERFLOW;
	default:                              return STATUS_IO_DEVICE_ERROR;
    }
}

enum {
	IN_100_NS = 1,
	MICROSEC_IN_100_NS = 10,
	MILLISEC_IN_100_NS = 1000 * MICROSEC_IN_100_NS,
	SECOND_IN_100_NS = 1000 * MILLISEC_IN_100_NS,
	IN_MILLISECOND = 1,
	SECOND_IN_MILLISECOND = 1000
};

FORCEINLINE
BOOLEAN
IsEqual4Byte(CONST VOID* Source1, CONST VOID* Source2)
{
	return (BOOLEAN)(4 == RtlCompareMemory(Source1, Source2, 4));
}

FORCEINLINE
BOOLEAN
IsEqual2Byte(CONST VOID* Source1, CONST VOID* Source2)
{
	return (BOOLEAN)(2 == RtlCompareMemory(Source1, Source2, 2));
}

FORCEINLINE
VOID
Copy4Bytes(VOID UNALIGNED* Destination, CONST VOID UNALIGNED* Source)
{
	RtlCopyBytes(Destination, Source, 4);
}

FORCEINLINE 
VOID 
GetUlongFromArray(const UCHAR* UCharArray, PULONG ULongValue)
{
	*ULongValue = 0;
	*ULongValue |= UCharArray[0];
	*ULongValue <<= 8;
	*ULongValue |= UCharArray[1];
	*ULongValue <<= 8;
	*ULongValue |= UCharArray[2];
	*ULongValue <<= 8;
	*ULongValue |= UCharArray[3];
}

FORCEINLINE 
VOID 
SetUlongInArray(UCHAR* UCharArray, ULONG ULongValue)
{
	UCharArray[0] = (UCHAR)((ULongValue & 0xFF000000) >> 24); 
	UCharArray[1] = (UCHAR)((ULongValue & 0x00FF0000) >> 16);
	UCharArray[2] = (UCHAR)((ULongValue & 0x0000FF00) >> 8);
	UCharArray[3] = (UCHAR)(ULongValue & 0x000000FF);
}

FORCEINLINE
VOID
SetRelativeDueTimeInSecond(
	__inout PLARGE_INTEGER DueTime,
	__in ULONG Seconds)
{
	DueTime->QuadPart = -(10000000 * (LONGLONG) Seconds);
}

FORCEINLINE
VOID
SetRelativeDueTimeInMillisecond(
	__inout PLARGE_INTEGER DueTime,
	__in ULONG Milliseconds)
{
	DueTime->QuadPart = -(10000 * (LONGLONG) Milliseconds);
}

FORCEINLINE
VOID
SetRelativeDueTimeInMicrosecond(
	__inout PLARGE_INTEGER DueTime,
	__in ULONG Microseconds)
{
	DueTime->QuadPart = -(10 * (LONGLONG) Microseconds);
}

FORCEINLINE
BOOLEAN
IsDispatchObjectSignaled(
	__in PVOID Object)
{
	static LARGE_INTEGER ZeroTimeout = {0};
	NTSTATUS status;

	status = KeWaitForSingleObject(
		Object, 
		Executive,
		KernelMode,
		FALSE,
		&ZeroTimeout);

	return (BOOLEAN)(STATUS_SUCCESS == status);
}

FORCEINLINE
BOOLEAN
IsCleanupRequest(
	PIO_STACK_LOCATION irpStack)
{
	return (irpStack->MajorFunction == IRP_MJ_CLOSE) || 
		(irpStack->MajorFunction == IRP_MJ_CLEANUP) ||
		(irpStack->MajorFunction == IRP_MJ_SHUTDOWN) ||
		((irpStack->MajorFunction == IRP_MJ_SCSI) && 
		((irpStack->Parameters.Scsi.Srb->Function == SRB_FUNCTION_RELEASE_DEVICE) ||
			(irpStack->Parameters.Scsi.Srb->Function == SRB_FUNCTION_FLUSH_QUEUE) ||
			(pTestFlag(irpStack->Parameters.Scsi.Srb->SrbFlags, SRB_FLAGS_BYPASS_FROZEN_QUEUE |
			SRB_FLAGS_BYPASS_LOCKED_QUEUE))));
}

#define GetLowWord(l)  ((USHORT)((ULONG_PTR)(l) & 0xffff))
#define GetHighWord(l) ((USHORT)((ULONG_PTR)(l) >> 16))
#define GetLowByte(w)  ((UCHAR)((ULONG_PTR)(w) & 0xff))
#define GetHighByte(w) ((UCHAR)((ULONG_PTR)(w) >> 8))

FORCEINLINE
ULONG
Cdb10LogicalBlockAddress(PCDB Cdb)
{
	return 
		((ULONG)(Cdb->CDB10.LogicalBlockByte0) << (8 * 3)) +
		((ULONG)(Cdb->CDB10.LogicalBlockByte1) << (8 * 2)) +
		((ULONG)(Cdb->CDB10.LogicalBlockByte2) << (8 * 1)) +
		((ULONG)(Cdb->CDB10.LogicalBlockByte3) << (8 * 0));
}

FORCEINLINE
ULONG
Cdb10TransferBlocks(PCDB Cdb)
{
	return
		((UINT32)(Cdb->CDB10.TransferBlocksMsb) << (8 * 1)) +
		((UINT32)(Cdb->CDB10.TransferBlocksLsb) << (8 * 0));
}

#ifdef DebugPrint
#undef DebugPrint
#endif

#if DBG
void
NdasPortDebugPrint(
    ULONG DebugPrintLevel,
    PCCHAR DebugMessage,
    ...);

#define DebugPrint(x) NdasPortDebugPrint x
#else
#define DebugPrint(x) __noop()
#endif

//#if DBG

CONST CHAR* DbgWmiMinorFunctionStringA(UCHAR MinorFunction);
CONST CHAR* DbgPnPMinorFunctionStringA(__in UCHAR MinorFunction);

CONST CHAR* DbgDeviceRelationStringA(__in DEVICE_RELATION_TYPE Type);
CONST CHAR* DbgDeviceIDStringA(__in BUS_QUERY_ID_TYPE Type);
CONST CHAR* DbgDeviceTextTypeStringA(__in DEVICE_TEXT_TYPE Type);

CONST CHAR* DbgSrbFunctionStringA(__in UCHAR SrbFunction);
CONST CHAR* DbgScsiOpStringA(__in UCHAR ScsiOpCode);
CONST CHAR* DbgSrbStatusStringA(UCHAR SrbStatus);
CONST CHAR* DbgIoControlCodeStringA(ULONG IoControlCode);
CONST CHAR* DbgScsiMiniportIoControlCodeString(ULONG IoControlCode);

CONST CHAR* DbgStorageQueryTypeStringA(__in STORAGE_QUERY_TYPE Type);
CONST CHAR* DbgStoragePropertyIdStringA(__in STORAGE_PROPERTY_ID Id);

CONST CHAR* DbgPowerMinorFunctionString(__in UCHAR MinorFunction);
CONST CHAR* DbgSystemPowerString(__in SYSTEM_POWER_STATE Type);
CONST CHAR* DbgDevicePowerString(__in DEVICE_POWER_STATE Type);  

CONST CHAR* DbgTdiPnpOpCodeString(__in TDI_PNP_OPCODE OpCode);
CONST CHAR* DbgNetPnpEventCodeString(__in NET_PNP_EVENT_CODE PnpEventCode);

CONST CHAR* AtaMinorVersionNumberString(__in USHORT MinorVersionNumber);

VOID DbgCheckScsiReturn(__in PIRP Irp, __in PSCSI_REQUEST_BLOCK Srb);

#if DBG
#define DBG_CHECK_SCSI_RETURN(Irp, Srb) DbgCheckScsiReturn(Irp, Srb)
#else
#define DBG_CHECK_SCSI_RETURN(Irp, Srb) 
#endif
