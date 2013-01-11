#include "ntkrnlapi.h"
#include "debug.h"

#include <ntverp.h>

ULONG NtMajorVersion;
ULONG NtMinorVersion;

#ifndef NtBuildNumber
#  if DBG
#    define NtBuildNumber   (VER_PRODUCTBUILD | 0xC0000000)
#  else
#    define NtBuildNumber (VER_PRODUCTBUILD | 0xF0000000)
# endif
#endif
UNICODE_STRING CmCSDVersionString;

VOID RtlInitUnicodeString(          
	PUNICODE_STRING DestinationString,
    PCWSTR SourceString
)
{
	ULONG Length;

    DestinationString->Buffer = (PWSTR)SourceString;
    if (ARGUMENT_PRESENT( SourceString )) {
        Length = wcslen( SourceString ) * sizeof( WCHAR );
        DestinationString->Length = (USHORT)Length;
        DestinationString->MaximumLength = (USHORT)(Length + sizeof(UNICODE_NULL));
        }
    else {
        DestinationString->MaximumLength = 0;
        DestinationString->Length = 0;
        }

}


NTSTATUS
  ObReferenceObjectByHandle(
    IN HANDLE  Handle,
    IN ACCESS_MASK  DesiredAccess,
    IN POBJECT_TYPE  ObjectType  OPTIONAL,
    IN KPROCESSOR_MODE  AccessMode,
    OUT PVOID  *Object,
    OUT POBJECT_HANDLE_INFORMATION  HandleInformation  OPTIONAL
    )
{
	UNREFERENCED_PARAMETER(Handle);
	UNREFERENCED_PARAMETER(DesiredAccess);
	UNREFERENCED_PARAMETER(ObjectType);
	UNREFERENCED_PARAMETER(AccessMode);
	UNREFERENCED_PARAMETER(Object);
	UNREFERENCED_PARAMETER(HandleInformation);

	return STATUS_SUCCESS;
}

VOID
RtlAssert(
    IN PVOID FailedAssertion,
    IN PVOID FileName,
    IN ULONG LineNumber,
    IN PCHAR Message OPTIONAL
    )
{
	UNREFERENCED_PARAMETER(FailedAssertion);
	UNREFERENCED_PARAMETER(FileName);
	UNREFERENCED_PARAMETER(LineNumber);
	UNREFERENCED_PARAMETER(Message);
}

NTSTATUS
  PsCreateSystemThread(
    OUT PHANDLE  ThreadHandle,
    IN ULONG  DesiredAccess,
    IN POBJECT_ATTRIBUTES  ObjectAttributes  OPTIONAL,
    IN HANDLE  ProcessHandle  OPTIONAL,
    OUT PCLIENT_ID  ClientId  OPTIONAL,
    IN PKSTART_ROUTINE  StartRoutine,
    IN PVOID  StartContext
    )
{
	UNREFERENCED_PARAMETER(ThreadHandle);
	UNREFERENCED_PARAMETER(DesiredAccess);
	UNREFERENCED_PARAMETER(ObjectAttributes);
	UNREFERENCED_PARAMETER(ProcessHandle);
	UNREFERENCED_PARAMETER(ClientId);
	UNREFERENCED_PARAMETER(StartRoutine);
	UNREFERENCED_PARAMETER(StartContext);

	return STATUS_SUCCESS;
}


VOID
KeInitializeSpinLock (
    IN PKSPIN_LOCK SpinLock
    )


{

    *(volatile ULONG *)SpinLock = 0;
    return;
}


VOID
  KeInitializeEvent(
    IN PRKEVENT  Event,
    IN EVENT_TYPE  Type,
    IN BOOLEAN  State
    )
{
	
    //
    // Initialize standard dispatcher object header, set initial signal
    // state of event object, and set the type of event object.
    //

    Event->Header.Type = (UCHAR)Type;
    Event->Header.Size = sizeof(KEVENT) / sizeof(LONG);
    Event->Header.SignalState = State;
    InitializeListHead(&Event->Header.WaitListHead);
    return;

}

BOOLEAN
  PsGetVersion(
    PULONG  MajorVersion  OPTIONAL,
    PULONG  MinorVersion  OPTIONAL,
    PULONG  BuildNumber  OPTIONAL,
    PUNICODE_STRING  CSDVersion  OPTIONAL
    )
{
	if (ARGUMENT_PRESENT(MajorVersion)) {
        *MajorVersion = NtMajorVersion;
    }

    if (ARGUMENT_PRESENT(MinorVersion)) {
        *MinorVersion = NtMinorVersion;
    }

    if (ARGUMENT_PRESENT(BuildNumber)) {
        *BuildNumber = NtBuildNumber & 0x3FFF;
    }

    if (ARGUMENT_PRESENT(CSDVersion)) {
        *CSDVersion = CmCSDVersionString;
    }

    return (BOOLEAN)((NtBuildNumber >> 28) == 0xC);
}

VOID
FASTCALL
ObfDereferenceObject(
    IN PVOID Object
    )
{
	UNREFERENCED_PARAMETER(Object);
}

VOID
FASTCALL
  ObReferenceObject(
    IN PVOID  Object
    )
{
	UNREFERENCED_PARAMETER(Object);
}

NTSTATUS
  KeWaitForSingleObject(
    IN PVOID  Object,
    IN KWAIT_REASON  WaitReason,
    IN KPROCESSOR_MODE  WaitMode,
    IN BOOLEAN  Alertable,
    IN PLARGE_INTEGER  Timeout  OPTIONAL
    )
{
	UNREFERENCED_PARAMETER(Object);
	UNREFERENCED_PARAMETER(WaitReason);
	UNREFERENCED_PARAMETER(WaitMode);
	UNREFERENCED_PARAMETER(Alertable);
	UNREFERENCED_PARAMETER(Timeout);

	return STATUS_SUCCESS;

}


LONG
FASTCALL
  InterlockedExchange(
    IN OUT PLONG  Target,
    IN LONG  Value
    )
{
    __asm {
        mov     eax, Value
        mov     ecx, Target
        xchg    [ecx], eax
    }
}

LONG
FASTCALL
  InterlockedExchangeAdd(
    IN OUT PLONG  Addend,
    IN LONG  Value
    )
{
    __asm {
        mov     eax, Value
        mov     ecx, Addend
        xadd    [ecx], eax
    }
}

LONG
FASTCALL
  InterlockedCompareExchange(
    IN OUT PLONG  Destination,
    IN LONG  Exchange,
    IN LONG  Comparand
    )
{
    __asm {
        mov     eax, Comparand
        mov     ecx, Destination
        mov     edx, Exchange
        cmpxchg [ecx], edx
    }
}


LONG
FASTCALL
InterlockedIncrement(
    IN OUT PLONG Addend
    )
{
	return InterlockedExchangeAdd (Addend, 1)+1;
}

LONG
FASTCALL
  InterlockedDecrement(
    IN PLONG  Addend
    )
{
	return InterlockedExchangeAdd (Addend, -1)-1;
}

PLIST_ENTRY
FASTCALL
  ExInterlockedRemoveHeadList(
    IN PLIST_ENTRY  ListHead,
    IN PKSPIN_LOCK  Lock
    )
{
	PLIST_ENTRY list;
	KIRQL Irql;

	KeAcquireSpinLock(Lock, &Irql);
	if(IsListEmpty(ListHead)) list = NULL;
	else list = RemoveHeadList(ListHead); 
	KeReleaseSpinLock(Lock, Irql);

	return list;

}

PLIST_ENTRY
FASTCALL
  ExfInterlockedInsertHeadList(
    IN PLIST_ENTRY  ListHead,
    IN PLIST_ENTRY  ListEntry,
    IN PKSPIN_LOCK  Lock
    )
{
	KIRQL Irql;
	
	KeAcquireSpinLock(Lock, &Irql);
	InsertHeadList(ListHead, ListEntry); 
	KeReleaseSpinLock(Lock, Irql);

	return ListHead;
}

PLIST_ENTRY
FASTCALL
  ExfInterlockedInsertTailList(
    IN PLIST_ENTRY  ListHead,
    IN PLIST_ENTRY  ListEntry,
    IN PKSPIN_LOCK  Lock
    )
{
	KIRQL Irql;
	
	KeAcquireSpinLock(Lock, &Irql);
	InsertTailList(ListHead, ListEntry); 
	KeReleaseSpinLock(Lock, Irql);

	return ListHead;

}

NTSTATUS
  PsTerminateSystemThread(
    IN NTSTATUS  ExitStatus
    )
{	
	return ExitStatus;
}

VOID
  KeClearEvent(
    IN PRKEVENT  Event
    )
{    
    //
    // Clear signal state of event object.
    //

    Event->Header.SignalState = 0;
    return;
}

KIRQL
  KeGetCurrentIrql(
    )
{
	return (KIRQL) PASSIVE_LEVEL;
}

KIRQL
FASTCALL
KfAcquireSpinLock (
    IN PKSPIN_LOCK SpinLock
    )

{
	ULONG flags;
	
	__asm	{
		pushfd
		pop flags
		cli
	};

	*((ULONG*)SpinLock) = flags;

	return (KIRQL)flags;

	return 0;
}


VOID
FASTCALL
KfReleaseSpinLock (
    IN PKSPIN_LOCK SpinLock,
    IN KIRQL NewIrql
    )
{

	ULONG flags = (ULONG)(*SpinLock);

	__asm	{
		push flags
		popfd	
	};	

}

VOID
FASTCALL
  KefAcquireSpinLockAtDpcLevel(
    IN PKSPIN_LOCK  SpinLock
    )
{
	ULONG flags;

	__asm	{
		pushfd
		pop flags
		cli
	};	

	*((ULONG*)SpinLock) = flags;
}

VOID
FASTCALL
  KefReleaseSpinLockFromDpcLevel(
    IN PKSPIN_LOCK  SpinLock
    )
{
	ULONG flags = (ULONG)(*SpinLock);

	__asm	{
		push flags
		popfd
	};
}

VOID
FASTCALL
KfLowerIrql (
    IN KIRQL NewIrql
    )
{
}

KIRQL
  KeRaiseIrqlToDpcLevel(
    )
{
	return PASSIVE_LEVEL;
}

LONG
  KeSetEvent(
    IN PRKEVENT  Event,
    IN KPRIORITY  Increment,
    IN BOOLEAN  Wait
    )
{
	UNREFERENCED_PARAMETER(Event);
	UNREFERENCED_PARAMETER(Increment);
	UNREFERENCED_PARAMETER(Wait);

	return 0;
}

NTSTATUS
FASTCALL
  IoCallDriver(
    IN PDEVICE_OBJECT  DeviceObject,
    IN OUT PIRP  Irp
    )
{
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Irp);

	return STATUS_SUCCESS;
}

PIRP
  IoBuildDeviceIoControlRequest(
    IN ULONG  IoControlCode,
    IN PDEVICE_OBJECT  DeviceObject,
    IN PVOID  InputBuffer  OPTIONAL,
    IN ULONG  InputBufferLength,
    OUT PVOID  OutputBuffer  OPTIONAL,
    IN ULONG  OutputBufferLength,
    IN BOOLEAN  InternalDeviceIoControl,
    IN PKEVENT  Event,
    OUT PIO_STATUS_BLOCK  IoStatusBlock
    )
{
	UNREFERENCED_PARAMETER(IoControlCode);
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(InputBuffer);
	UNREFERENCED_PARAMETER(InputBufferLength);
	UNREFERENCED_PARAMETER(OutputBuffer);
	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(InternalDeviceIoControl);
	UNREFERENCED_PARAMETER(Event);	

	IoStatusBlock->Status = 0;
	IoStatusBlock->Information = 0;

	return (PIRP) 0;
}

NTSTATUS
  IoGetDeviceObjectPointer(
    IN PUNICODE_STRING  ObjectName,
    IN ACCESS_MASK  DesiredAccess,
    OUT PFILE_OBJECT  *FileObject,
    OUT PDEVICE_OBJECT  *DeviceObject
    )
{
	UNREFERENCED_PARAMETER(ObjectName);
	UNREFERENCED_PARAMETER(DesiredAccess);
	UNREFERENCED_PARAMETER(FileObject);
	UNREFERENCED_PARAMETER(DeviceObject);

	return STATUS_SUCCESS;
}

NTSTATUS
  IoGetDeviceInterfaces(
    IN CONST GUID  *InterfaceClassGuid,
    IN PDEVICE_OBJECT  PhysicalDeviceObject  OPTIONAL,
    IN ULONG  Flags,
    OUT PWSTR  *SymbolicLinkList
    )
{
	UNREFERENCED_PARAMETER(InterfaceClassGuid);
	UNREFERENCED_PARAMETER(PhysicalDeviceObject);
	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(SymbolicLinkList);

	return STATUS_SUCCESS;
}

VOID
  IoFreeIrp(
    IN PIRP  Irp
    )
{
	UNREFERENCED_PARAMETER(Irp);
}

PIRP
  IoAllocateIrp(
    IN CCHAR  StackSize,
    IN BOOLEAN  ChargeQuota
    )
{
	UNREFERENCED_PARAMETER(StackSize);
	UNREFERENCED_PARAMETER(ChargeQuota);

	return (PIRP) 0;
}

PIRP
  IoBuildSynchronousFsdRequest(
    IN ULONG  MajorFunction,
    IN PDEVICE_OBJECT  DeviceObject,
    IN OUT PVOID  Buffer  OPTIONAL,
    IN ULONG  Length  OPTIONAL,
    IN PLARGE_INTEGER  StartingOffset  OPTIONAL,
    IN PKEVENT  Event,
    OUT PIO_STATUS_BLOCK  IoStatusBlock
    )
{
	UNREFERENCED_PARAMETER(MajorFunction);
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Buffer);
	UNREFERENCED_PARAMETER(Length);
	UNREFERENCED_PARAMETER(StartingOffset);
	UNREFERENCED_PARAMETER(Event);
	
	(IoStatusBlock)->Status = 0;
	(IoStatusBlock)->Information = 0;

	return (PIRP) 0;
}

NTSTATUS
RtlUnicodeToMultiByteN(
    PCHAR MultiByteString,
    ULONG MaxBytesInMultiByteString,
    PULONG BytesInMultiByteString,
    PWSTR UnicodeString,
    ULONG BytesInUnicodeString
    )
{
	return STATUS_SUCCESS;
}

VOID
  IoQueueWorkItem(
    IN PIO_WORKITEM  IoWorkItem,
    IN PIO_WORKITEM_ROUTINE  Routine,
    IN WORK_QUEUE_TYPE  QueueType,
    IN PVOID  Context
    )
{
	UNREFERENCED_PARAMETER(IoWorkItem);
	UNREFERENCED_PARAMETER(Routine);
	UNREFERENCED_PARAMETER(QueueType);
	UNREFERENCED_PARAMETER(Context);
}

PIO_WORKITEM
  IoAllocateWorkItem(
    IN PDEVICE_OBJECT  DeviceObject
    )
{
	UNREFERENCED_PARAMETER(DeviceObject);

	return (PIO_WORKITEM) 0;
}

VOID
  IoWriteErrorLogEntry(
    IN PVOID  ElEntry
    )
{
	UNREFERENCED_PARAMETER(ElEntry);
}

PVOID
  IoAllocateErrorLogEntry(
    IN PVOID  IoObject,
    IN UCHAR  EntrySize
    )
{
	UNREFERENCED_PARAMETER(IoObject);
	UNREFERENCED_PARAMETER(EntrySize);

	return 0;
}

VOID
  IoFreeWorkItem(
    IN PIO_WORKITEM  IoWorkItem
    )
{
	UNREFERENCED_PARAMETER(IoWorkItem);
}


SIZE_T
  RtlCompareMemory(
    IN CONST VOID  *Source1,
    IN CONST VOID  *Source2,
    IN SIZE_T  Length
    )
{
	if(memcmp(Source1,Source2,Length) == 0) {
		return Length;
	}
	else  {
		return 0;
	}
}

LONG
  KeReadStateEvent(
    IN PRKEVENT  Event
    )
{
	return Event->Header.SignalState;
}

VOID
  ExInitializeNPagedLookasideList(
    IN PNPAGED_LOOKASIDE_LIST  Lookaside,
    IN PALLOCATE_FUNCTION  Allocate  OPTIONAL,
    IN PFREE_FUNCTION  Free  OPTIONAL,
    IN ULONG  Flags,
    IN SIZE_T  Size,
    IN ULONG  Tag,
    IN USHORT  Depth
    )
{

    UNREFERENCED_PARAMETER(Lookaside);
	UNREFERENCED_PARAMETER(Allocate);
	UNREFERENCED_PARAMETER(Free);
	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(Size);
	UNREFERENCED_PARAMETER(Tag);
	UNREFERENCED_PARAMETER(Depth);

    return;
}

VOID
  ExDeleteNPagedLookasideList(
    IN PNPAGED_LOOKASIDE_LIST  Lookaside
    )
{
	UNREFERENCED_PARAMETER(Lookaside);
}

PSINGLE_LIST_ENTRY
FASTCALL
  ExInterlockedPushEntrySList(
    IN PSLIST_HEADER  ListHead,
    IN PSINGLE_LIST_ENTRY  ListEntry,
    IN PKSPIN_LOCK  Lock
	)    
{
	UNREFERENCED_PARAMETER(ListHead);
	UNREFERENCED_PARAMETER(ListEntry);
	UNREFERENCED_PARAMETER(Lock);

	return (PSINGLE_LIST_ENTRY) 0;
}

PSINGLE_LIST_ENTRY
FASTCALL
  ExInterlockedPopEntrySList(
    IN PSLIST_HEADER  ListHead,
    IN PKSPIN_LOCK  Lock
    )
{
	UNREFERENCED_PARAMETER(ListHead);	
	UNREFERENCED_PARAMETER(Lock);

	return (PSINGLE_LIST_ENTRY) 0;
}

KPRIORITY
  KeSetPriorityThread(
    IN PKTHREAD  Thread,
    IN KPRIORITY  Priority
    )
{
	UNREFERENCED_PARAMETER(Thread);	
	UNREFERENCED_PARAMETER(Priority);

	return (KPRIORITY) 0;
}

#undef KeGetCurrentThread
PKTHREAD
  KeGetCurrentThread(
    )
{
	return (PKTHREAD) 0;
}

//
//  Macro that tells how many contiguous bits are set (i.e., 1) in
//  a byte
//

#define RtlpBitSetAnywhere( Byte ) RtlpBitsClearAnywhere[ (~(Byte) & 0xFF) ]


//
//  Macro that tells how many contiguous LOW order bits are set
//  (i.e., 1) in a byte
//

#define RtlpBitsSetLow( Byte ) RtlpBitsClearLow[ (~(Byte) & 0xFF) ]


//
//  Macro that tells how many contiguous HIGH order bits are set
//  (i.e., 1) in a byte
//

#define RtlpBitsSetHigh( Byte ) RtlpBitsClearHigh[ (~(Byte) & 0xFF) ]


//
//  Macro that tells how many set bits (i.e., 1) there are in a byte
//

#define RtlpBitsSetTotal( Byte ) RtlpBitsClearTotal[ (~(Byte) & 0xFF) ]



//
//  There are three macros to make reading the bytes in a bitmap easier.
//

#define GET_BYTE_DECLARATIONS() \
    PUCHAR _CURRENT_POSITION;

#define GET_BYTE_INITIALIZATION(RTL_BITMAP,BYTE_INDEX) {               \
    _CURRENT_POSITION = &((PUCHAR)((RTL_BITMAP)->Buffer))[BYTE_INDEX]; \
}

#define GET_BYTE(THIS_BYTE)  (         \
    THIS_BYTE = *(_CURRENT_POSITION++) \
)


//
//  Lookup table that tells how many contiguous bits are clear (i.e., 0) in
//  a byte
//

CONST CCHAR RtlpBitsClearAnywhere[] =
         { 8,7,6,6,5,5,5,5,4,4,4,4,4,4,4,4,
           4,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
           5,4,3,3,2,2,2,2,3,2,2,2,2,2,2,2,
           4,3,2,2,2,2,2,2,3,2,2,2,2,2,2,2,
           6,5,4,4,3,3,3,3,3,2,2,2,2,2,2,2,
           4,3,2,2,2,1,1,1,3,2,1,1,2,1,1,1,
           5,4,3,3,2,2,2,2,3,2,1,1,2,1,1,1,
           4,3,2,2,2,1,1,1,3,2,1,1,2,1,1,1,
           7,6,5,5,4,4,4,4,3,3,3,3,3,3,3,3,
           4,3,2,2,2,2,2,2,3,2,2,2,2,2,2,2,
           5,4,3,3,2,2,2,2,3,2,1,1,2,1,1,1,
           4,3,2,2,2,1,1,1,3,2,1,1,2,1,1,1,
           6,5,4,4,3,3,3,3,3,2,2,2,2,2,2,2,
           4,3,2,2,2,1,1,1,3,2,1,1,2,1,1,1,
           5,4,3,3,2,2,2,2,3,2,1,1,2,1,1,1,
           4,3,2,2,2,1,1,1,3,2,1,1,2,1,1,0 };

//
//  Lookup table that tells how many contiguous LOW order bits are clear
//  (i.e., 0) in a byte
//

CONST CCHAR RtlpBitsClearLow[] =
          { 8,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            6,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            7,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            6,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            5,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0,
            4,0,1,0,2,0,1,0,3,0,1,0,2,0,1,0 };

//
//  Lookup table that tells how many contiguous HIGH order bits are clear
//  (i.e., 0) in a byte
//

CONST CCHAR RtlpBitsClearHigh[] =
          { 8,7,6,6,5,5,5,5,4,4,4,4,4,4,4,4,
            3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
            2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
            2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
            1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

//
//  Lookup table that tells how many clear bits (i.e., 0) there are in a byte
//

CONST CCHAR RtlpBitsClearTotal[] =
          { 8,7,7,6,7,6,6,5,7,6,6,5,6,5,5,4,
            7,6,6,5,6,5,5,4,6,5,5,4,5,4,4,3,
            7,6,6,5,6,5,5,4,6,5,5,4,5,4,4,3,
            6,5,5,4,5,4,4,3,5,4,4,3,4,3,3,2,
            7,6,6,5,6,5,5,4,6,5,5,4,5,4,4,3,
            6,5,5,4,5,4,4,3,5,4,4,3,4,3,3,2,
            6,5,5,4,5,4,4,3,5,4,4,3,4,3,3,2,
            5,4,4,3,4,3,3,2,4,3,3,2,3,2,2,1,
            7,6,6,5,6,5,5,4,6,5,5,4,5,4,4,3,
            6,5,5,4,5,4,4,3,5,4,4,3,4,3,3,2,
            6,5,5,4,5,4,4,3,5,4,4,3,4,3,3,2,
            5,4,4,3,4,3,3,2,4,3,3,2,3,2,2,1,
            6,5,5,4,5,4,4,3,5,4,4,3,4,3,3,2,
            5,4,4,3,4,3,3,2,4,3,3,2,3,2,2,1,
            5,4,4,3,4,3,3,2,4,3,3,2,3,2,2,1,
            4,3,3,2,3,2,2,1,3,2,2,1,2,1,1,0 };

//
//  Bit Mask for clearing and setting bits within bytes.  FillMask[i] has the first
//  i bits set to 1.  ZeroMask[i] has the first i bits set to zero.
//

static CONST UCHAR FillMask[] = { 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF };
static CONST UCHAR ZeroMask[] = { 0xFF, 0xFE, 0xFC, 0xF8, 0xf0, 0xe0, 0xc0, 0x80, 0x00 };


VOID
  RtlInitializeBitMap(
    IN PRTL_BITMAP  BitMapHeader,
    IN PULONG  BitMapBuffer,
    IN ULONG  SizeOfBitMap
    )
{
    BitMapHeader->SizeOfBitMap = SizeOfBitMap;
    BitMapHeader->Buffer = BitMapBuffer;

}

VOID
  RtlClearBits(
    IN PRTL_BITMAP  BitMapHeader,
    IN ULONG  StartingIndex,
    IN ULONG  NumberToClear
    )
{
	PCHAR CurrentByte;
    ULONG BitOffset;

    //DbgPrint("ClearBits %08lx, ", NumberToClear);
    //DbgPrint("%08lx", StartingIndex);

    ASSERT( StartingIndex + NumberToClear <= BitMapHeader->SizeOfBitMap );

    //
    //  Special case the situation where the number of bits to clear is
    //  zero.  Turn this into a noop.
    //

    if (NumberToClear == 0) {

        return;
    }

    //
    //  Get a pointer to the first byte that needs to be cleared.
    //

    CurrentByte = ((PCHAR) BitMapHeader->Buffer) + (StartingIndex / 8);

    //
    //  If all the bit's we're setting are in the same byte just do it and
    //  get out.
    //

    BitOffset = StartingIndex % 8;
    if ((BitOffset + NumberToClear) <= 8) {

        *CurrentByte &= ~(FillMask[ NumberToClear ] << BitOffset);

    }  else {

        //
        //  Do the first byte manually because the first bit may not be byte aligned.
        //
        //  Note:   The first longword will always be cleared byte wise to simplify the
        //          logic of checking for short copies (<32 bits).
        //

        if (BitOffset > 0) {

            *CurrentByte &= FillMask[ BitOffset ];
            CurrentByte += 1;
            NumberToClear -= 8 - BitOffset;

        }

        //
        //  Fill the full bytes in the middle.  Use the RtlZeroMemory() because its
        //  going to be hand tuned asm code spit out by the compiler.
        //

        if (NumberToClear > 8) {

            RtlZeroMemory( CurrentByte, NumberToClear / 8 );
            CurrentByte += NumberToClear / 8;
            NumberToClear %= 8;

        }

        //
        //  Clear the remaining bits, if there are any, in the last byte.
        //

        if (NumberToClear > 0) {

            *CurrentByte &= ZeroMask[ NumberToClear ];

        }

    }

    //
    //  And return to our caller
    //

    //DumpBitMap(BitMapHeader);

    return;
}

VOID
  RtlClearAllBits(
    IN PRTL_BITMAP  BitMapHeader
    )
{
    RtlZeroMemory( BitMapHeader->Buffer,
              ((BitMapHeader->SizeOfBitMap + 31) / 32) * 4
	          );
}

ULONG
  RtlFindSetBits(
    IN PRTL_BITMAP  BitMapHeader,
    IN ULONG  NumberToFind,
    IN ULONG  HintIndex
    )
{
	    ULONG SizeOfBitMap;
    ULONG SizeInBytes;

    ULONG HintBit;
    ULONG MainLoopIndex;

    GET_BYTE_DECLARATIONS();

    //
    //  To make the loops in our test run faster we'll extract the
    //  fields from the bitmap header
    //

    SizeOfBitMap = BitMapHeader->SizeOfBitMap;
    SizeInBytes = (SizeOfBitMap + 7) / 8;

    //
    //  Set any unused bits in the last byte so we won't count them.  We do
    //  this by first checking if there is any odd bits in the last byte.
    //

    if ((SizeOfBitMap % 8) != 0) {

        //
        //  The last byte has some odd bits so we'll set the high unused
        //  bits in the last byte to 0's
        //

        ((PUCHAR)BitMapHeader->Buffer)[SizeInBytes - 1] &=
                                                    FillMask[SizeOfBitMap % 8];
    }

    //
    //  Calculate from the hint index where the hint byte is and set ourselves
    //  up to read the hint on the next call to GET_BYTE.  To make the
    //  algorithm run fast we'll only honor hints down to the byte level of
    //  granularity.  There is a possibility that we'll need to execute
    //  our main logic twice.  Once to test from the hint byte to the end of
    //  the bitmap and the other to test from the start of the bitmap.  First
    //  we need to make sure the Hint Index is within range.
    //

    if (HintIndex >= SizeOfBitMap) {

        HintIndex = 0;
    }

    HintBit = HintIndex % 8;

    for (MainLoopIndex = 0; MainLoopIndex < 2; MainLoopIndex += 1) {

        ULONG StartByteIndex;
        ULONG EndByteIndex;

        UCHAR CurrentByte;

        //
        //  Check for the first time through the main loop, which indicates
        //  that we are going to start our search at our hint byte
        //

        if (MainLoopIndex == 0) {

            StartByteIndex = HintIndex / 8;
            EndByteIndex = SizeInBytes;

        //
        //  This is the second time through the loop, make sure there is
        //  actually something to check before the hint byte
        //

        } else if (HintIndex != 0) {

            //
            //  The end index for the second time around is based on the
            //  number of bits we need to find.  We need to use this inorder
            //  to take the case where the preceding byte to the hint byte
            //  is the start of our run, and the run includes the hint byte
            //  and some following bytes, based on the number of bits needed
            //  The computation is to take the number of bits needed minus
            //  2 divided by 8 and then add 2.  This will take in to account
            //  the worst possible case where we have one bit hanging off
            //  of each end byte, and all intervening bytes are all zero.
            //  We only need to add one in the following equation because
            //  HintByte is already counted.
            //

            if (NumberToFind < 2) {

                EndByteIndex = (HintIndex + 7) / 8;

            } else {

                EndByteIndex = (HintIndex + 7) / 8 + ((NumberToFind - 2) / 8) + 2;

                //
                //  Make sure we don't overrun the end of the bitmap
                //

                if (EndByteIndex > SizeInBytes) {

                    EndByteIndex = SizeInBytes;
                }
            }

            StartByteIndex = 0;
            HintIndex = 0;
            HintBit = 0;

        //
        //  Otherwise we already did a complete loop through the bitmap
        //  so we should simply return -1 to say nothing was found
        //

        } else {

            return 0xffffffff;
        }

        //
        //  Set ourselves up to get the next byte
        //

        GET_BYTE_INITIALIZATION(BitMapHeader, StartByteIndex);

        //
        //  Get the first byte, and clear any bits before the hint bit.
        //

        GET_BYTE( CurrentByte );

        CurrentByte &= ZeroMask[HintBit];

        //
        //  If the number of bits can only fit in 1 or 2 bytes (i.e., 9 bits or
        //  less) we do the following test case.
        //

        if (NumberToFind <= 9) {

            ULONG CurrentBitIndex;

            UCHAR PreviousByte;

            PreviousByte = 0x00;

            //
            //  Examine all the bytes within our test range searching
            //  for a fit
            //

            CurrentBitIndex = StartByteIndex * 8;

            while (TRUE) {

                //
                //  Check to see if the current byte coupled with the previous
                //  byte will satisfy the requirement. The check uses the high
                //  part of the previous byte and low part of the current byte.
                //

                if (((ULONG)RtlpBitsSetHigh(PreviousByte) +
                             (ULONG)RtlpBitsSetLow(CurrentByte)) >= NumberToFind) {

                    ULONG StartingIndex;

                    //
                    //  It all fits in these two bytes, so we can compute
                    //  the starting index.  This is done by taking the
                    //  index of the current byte (bit 0) and subtracting the
                    //  number of bits its takes to get to the first set
                    //  high bit.
                    //

                    StartingIndex = CurrentBitIndex -
                                               (LONG)RtlpBitsSetHigh(PreviousByte);

                    //
                    //  Now make sure the total size isn't beyond the bitmap
                    //

                    if ((StartingIndex + NumberToFind) <= SizeOfBitMap) {

                        return StartingIndex;
                    }
                }

                //
                //  The previous byte does not help, so check the current byte.
                //

                if ((ULONG)RtlpBitSetAnywhere(CurrentByte) >= NumberToFind) {

                    UCHAR BitMask;
                    ULONG i;

                    //
                    //  It all fits in a single byte, so calculate the bit
                    //  number.  We do this by taking a mask of the appropriate
                    //  size and shifting it over until it fits.  It fits when
                    //  we can bitwise-and the current byte with the bit mask
                    //  and get back the bit mask.
                    //

                    BitMask = FillMask[ NumberToFind ];
                    for (i = 0; (BitMask & CurrentByte) != BitMask; i += 1) {

                        BitMask <<= 1;
                    }

                    //
                    //  return to our caller the located bit index, and the
                    //  number that we found.
                    //

                    return CurrentBitIndex + i;
                }

                //
                //  For the next iteration through our loop we need to make
                //  the current byte into the previous byte, and go to the
                //  top of the loop again.
                //

                PreviousByte = CurrentByte;

                //
                //  Increment our Bit Index, and either exit, or get the
                //  next byte.
                //

                CurrentBitIndex += 8;

                if ( CurrentBitIndex < EndByteIndex * 8 ) {

                    GET_BYTE( CurrentByte );

                } else {

                    break;
                }

            } // end loop CurrentBitIndex

        //
        //  The number to find is greater than 9 but if it is less than 15
        //  then we know it can be satisfied with at most 2 bytes, or 3 bytes
        //  if the middle byte (of the 3) is all ones.
        //

        } else if (NumberToFind < 15) {

            ULONG CurrentBitIndex;

            UCHAR PreviousPreviousByte;
            UCHAR PreviousByte;

            PreviousByte = 0x00;

            //
            //  Examine all the bytes within our test range searching
            //  for a fit
            //

            CurrentBitIndex = StartByteIndex * 8;

            while (TRUE) {

                //
                //  For the next iteration through our loop we need to make
                //  the current byte into the previous byte, the previous
                //  byte into the previous previous byte, and go to the
                //  top of the loop again.
                //

                PreviousPreviousByte = PreviousByte;
                PreviousByte = CurrentByte;

                //
                //  Increment our Bit Index, and either exit, or get the
                //  next byte.
                //

                CurrentBitIndex += 8;

                if ( CurrentBitIndex < EndByteIndex * 8 ) {

                    GET_BYTE( CurrentByte );

                } else {

                    break;
                }

                //
                //  if the previous byte is all ones then maybe the
                //  request can be satisfied using the Previous Previous Byte
                //  Previous Byte, and the Current Byte.
                //

                if ((PreviousByte == 0xff)

                        &&

                    (((ULONG)RtlpBitsSetHigh(PreviousPreviousByte) + 8 +
                            (ULONG)RtlpBitsSetLow(CurrentByte)) >= NumberToFind)) {

                    ULONG StartingIndex;

                    //
                    //  It all fits in these three bytes, so we can compute
                    //  the starting index.  This is done by taking the
                    //  index of the previous byte (bit 0) and subtracting
                    //  the number of bits its takes to get to the first
                    //  set high bit.
                    //

                    StartingIndex = (CurrentBitIndex - 8) -
                                       (LONG)RtlpBitsSetHigh(PreviousPreviousByte);

                    //
                    //  Now make sure the total size isn't beyond the bitmap
                    //

                    if ((StartingIndex + NumberToFind) <= SizeOfBitMap) {

                        return StartingIndex;
                    }
                }

                //
                //  Check to see if the Previous byte and current byte
                //  together satisfy the request.
                //

                if (((ULONG)RtlpBitsSetHigh(PreviousByte) +
                             (ULONG)RtlpBitsSetLow(CurrentByte)) >= NumberToFind) {

                    ULONG StartingIndex;

                    //
                    //  It all fits in these two bytes, so we can compute
                    //  the starting index.  This is done by taking the
                    //  index of the current byte (bit 0) and subtracting the
                    //  number of bits its takes to get to the first set
                    //  high bit.
                    //

                    StartingIndex = CurrentBitIndex -
                                               (LONG)RtlpBitsSetHigh(PreviousByte);

                    //
                    //  Now make sure the total size isn't beyond the bitmap
                    //

                    if ((StartingIndex + NumberToFind) <= SizeOfBitMap) {

                        return StartingIndex;
                    }
                }
            } // end loop CurrentBitIndex

        //
        //  The number to find is greater than or equal to 15.  This request
        //  has to have at least one byte of all ones to be satisfied
        //

        } else {

            ULONG CurrentByteIndex;

            ULONG OneBytesNeeded;
            ULONG OneBytesFound;

            UCHAR StartOfRunByte;
            LONG StartOfRunIndex;

            //
            //  First precalculate how many one bytes we're going to need
            //

            OneBytesNeeded = (NumberToFind - 7) / 8;

            //
            //  Indicate for the first time through our loop that we haven't
            //  found a one byte yet, and indicate that the start of the
            //  run is the byte just before the start byte index
            //

            OneBytesFound = 0;
            StartOfRunByte = 0x00;
            StartOfRunIndex = StartByteIndex - 1;

            //
            //  Examine all the bytes in our test range searching for a fit
            //

            CurrentByteIndex = StartByteIndex;

            while (TRUE) {

                //
                //  If the number of zero bytes fits our minimum requirements
                //  then we can do the additional test to see if we
                //  actually found a fit
                //

                if ((OneBytesFound >= OneBytesNeeded - 1)

                        &&

                    ((ULONG)RtlpBitsSetHigh(StartOfRunByte) + OneBytesFound*8 +
                     (ULONG)RtlpBitsSetLow(CurrentByte)) >= NumberToFind) {

                    ULONG StartingIndex;

                    //
                    //  It all fits in these bytes, so we can compute
                    //  the starting index.  This is done by taking the
                    //  StartOfRunIndex times 8 and adding the number of bits
                    //  it takes to get to the first set high bit.
                    //

                    StartingIndex = (StartOfRunIndex * 8) +
                                       (8 - (LONG)RtlpBitsSetHigh(StartOfRunByte));

                    //
                    //  Now make sure the total size isn't beyond the bitmap
                    //

                    if ((StartingIndex + NumberToFind) <= SizeOfBitMap) {

                        return StartingIndex;
                    }
                }

                //
                //  Check to see if the byte is all ones and increment
                //  the number of one bytes found
                //

                if (CurrentByte == 0xff) {

                    OneBytesFound += 1;

                //
                //  The byte isn't all ones so we need to start over again
                //  looking for one bytes.
                //

                } else {

                    OneBytesFound = 0;
                    StartOfRunByte = CurrentByte;
                    StartOfRunIndex = CurrentByteIndex;
                }

                //
                //  Increment our Byte Index, and either exit, or get the
                //  next byte.
                //

                CurrentByteIndex += 1;

                if ( CurrentByteIndex < EndByteIndex ) {

                    GET_BYTE( CurrentByte );

                } else {

                    break;
                }
            } // end loop CurrentByteIndex
        }
    }

    //
    //  We never found a fit so we'll return -1
    //

    return 0xffffffff;

}

VOID
RtlSetBits (
    IN PRTL_BITMAP BitMapHeader,
    IN ULONG StartingIndex,
    IN ULONG NumberToSet
    )

/*++

Routine Description:

    This procedure sets the specified range of bits within the
    specified bit map.

Arguments:

    BitMapHeader - Supplies a pointer to the previously initialied BitMap.

    StartingIndex - Supplies the index (zero based) of the first bit to set.

    NumberToSet - Supplies the number of bits to set.

Return Value:

    None.

--*/
{
    PCHAR CurrentByte;
    ULONG BitOffset;

    //DbgPrint("SetBits %08lx, ", NumberToSet);
    //DbgPrint("%08lx", StartingIndex);

    ASSERT( StartingIndex + NumberToSet <= BitMapHeader->SizeOfBitMap );

    //
    //  Special case the situation where the number of bits to set is
    //  zero.  Turn this into a noop.
    //

    if (NumberToSet == 0) {

        return;
    }

    //
    //  Get a pointer to the first byte that needs to be set.
    //

    CurrentByte = ((PCHAR) BitMapHeader->Buffer) + (StartingIndex / 8);

    //
    //  If all the bit's we're setting are in the same byte just do it and
    //  get out.
    //

    BitOffset = StartingIndex % 8;
    if ((BitOffset + NumberToSet) <= 8) {

        *CurrentByte |= (FillMask[ NumberToSet ] << BitOffset);

    } else {

        //
        //  Do the first byte manually because the first bit may not be byte aligned.
        //
        //  Note:   The first longword will always be set byte wise to simplify the
        //          logic of checking for short copies (<32 bits).
        //

        if (BitOffset > 0) {

            *CurrentByte |= ZeroMask[ BitOffset ];
            CurrentByte += 1;
            NumberToSet -= 8 - BitOffset;

        }

        //
        //  Fill the full bytes in the middle.  Use the RtlFillMemory() because its
        //  going to be hand tuned asm code spit out by the compiler.
        //

        if (NumberToSet > 8) {

            RtlFillMemory( CurrentByte, NumberToSet / 8, 0xff );
            CurrentByte += NumberToSet / 8;
            NumberToSet %= 8;

        }

        //
        //  Set the remaining bits, if there are any, in the last byte.
        //

        if (NumberToSet > 0) {

            *CurrentByte |= FillMask[ NumberToSet ];

        }

    }

    //
    //  And return to our caller
    //

    //DumpBitMap(BitMapHeader);

    return;
}

VOID
RtlFillMemoryUlong (
   PVOID Destination,
   SIZE_T Length,
   ULONG Pattern
   )
{	
	PULONG p = Destination;
	ULONG i;

	for(i=0; i<Length/sizeof(Pattern); i++)
		*p++ = Pattern;
}

VOID
RtlSetAllBits (
    IN PRTL_BITMAP BitMapHeader
    )

/*++

Routine Description:

    This procedure sets all bits in the specified Bit Map.

Arguments:

    BitMapHeader - Supplies a pointer to the previously initialized BitMap

Return Value:

    None.

--*/

{
    //
    //  Set all the bits
    //

    RtlFillMemoryUlong( BitMapHeader->Buffer,
                        ((BitMapHeader->SizeOfBitMap + 31) / 32) * 4,
                        0xffffffff
                      );

    //
    //  And return to our caller
    //

    //DbgPrint("SetAllBits"); DumpBitMap(BitMapHeader);
    return;
}


ULONG
RtlFindClearBits (
    IN PRTL_BITMAP BitMapHeader,
    IN ULONG NumberToFind,
    IN ULONG HintIndex
    )

/*++

Routine Description:

    This procedure searches the specified bit map for the specified
    contiguous region of clear bits.  If a run is not found from the
    hint to the end of the bitmap, we will search again from the
    beginning of the bitmap.

Arguments:

    BitMapHeader - Supplies a pointer to the previously initialized BitMap.

    NumberToFind - Supplies the size of the contiguous region to find.

    HintIndex - Supplies the index (zero based) of where we should start
        the search from within the bitmap.

Return Value:

    ULONG - Receives the starting index (zero based) of the contiguous
        region of clear bits found.  If not such a region cannot be found
        a -1 (i.e. 0xffffffff) is returned.

--*/

{
    ULONG SizeOfBitMap;
    ULONG SizeInBytes;

    ULONG HintBit;
    ULONG MainLoopIndex;

    GET_BYTE_DECLARATIONS();

    //
    //  To make the loops in our test run faster we'll extract the
    //  fields from the bitmap header
    //

    SizeOfBitMap = BitMapHeader->SizeOfBitMap;
    SizeInBytes = (SizeOfBitMap + 7) / 8;

    //
    //  Set any unused bits in the last byte so we won't count them.  We do
    //  this by first checking if there is any odd bits in the last byte.
    //

    if ((SizeOfBitMap % 8) != 0) {

        //
        //  The last byte has some odd bits so we'll set the high unused
        //  bits in the last byte to 1's
        //

        ((PUCHAR)BitMapHeader->Buffer)[SizeInBytes - 1] |=
                                                    ZeroMask[SizeOfBitMap % 8];
    }

    //
    //  Calculate from the hint index where the hint byte is and set ourselves
    //  up to read the hint on the next call to GET_BYTE.  To make the
    //  algorithm run fast we'll only honor hints down to the byte level of
    //  granularity.  There is a possibility that we'll need to execute
    //  our main logic twice.  Once to test from the hint byte to the end of
    //  the bitmap and the other to test from the start of the bitmap.  First
    //  we need to make sure the Hint Index is within range.
    //

    if (HintIndex >= SizeOfBitMap) {

        HintIndex = 0;
    }

    HintBit = HintIndex % 8;

    for (MainLoopIndex = 0; MainLoopIndex < 2; MainLoopIndex += 1) {

        ULONG StartByteIndex;
        ULONG EndByteIndex;

        UCHAR CurrentByte;

        //
        //  Check for the first time through the main loop, which indicates
        //  that we are going to start our search at our hint byte
        //

        if (MainLoopIndex == 0) {

            StartByteIndex = HintIndex / 8;
            EndByteIndex = SizeInBytes;

        //
        //  This is the second time through the loop, make sure there is
        //  actually something to check before the hint byte
        //

        } else if (HintIndex != 0) {

            //
            //  The end index for the second time around is based on the
            //  number of bits we need to find.  We need to use this inorder
            //  to take the case where the preceding byte to the hint byte
            //  is the start of our run, and the run includes the hint byte
            //  and some following bytes, based on the number of bits needed
            //  The computation is to take the number of bits needed minus
            //  2 divided by 8 and then add 2.  This will take in to account
            //  the worst possible case where we have one bit hanging off
            //  of each end byte, and all intervening bytes are all zero.
            //

            if (NumberToFind < 2) {

                EndByteIndex = (HintIndex + 7) / 8;

            } else {

                EndByteIndex = (HintIndex + 7) / 8 + ((NumberToFind - 2) / 8) + 2;

                //
                //  Make sure we don't overrun the end of the bitmap
                //

                if (EndByteIndex > SizeInBytes) {

                    EndByteIndex = SizeInBytes;
                }
            }

            HintIndex = 0;
            HintBit = 0;
            StartByteIndex = 0;

        //
        //  Otherwise we already did a complete loop through the bitmap
        //  so we should simply return -1 to say nothing was found
        //

        } else {

            return 0xffffffff;
        }

        //
        //  Set ourselves up to get the next byte
        //

        GET_BYTE_INITIALIZATION(BitMapHeader, StartByteIndex);

        //
        //  Get the first byte, and set any bits before the hint bit.
        //

        GET_BYTE( CurrentByte );

        CurrentByte |= FillMask[HintBit];

        //
        //  If the number of bits can only fit in 1 or 2 bytes (i.e., 9 bits or
        //  less) we do the following test case.
        //

        if (NumberToFind <= 9) {

            ULONG CurrentBitIndex;
            UCHAR PreviousByte;

            PreviousByte = 0xff;

            //
            //  Examine all the bytes within our test range searching
            //  for a fit
            //

            CurrentBitIndex = StartByteIndex * 8;

            while (TRUE) {

                //
                //  If this is the first itteration of the loop, mask Current
                //  byte with the real hint.
                //

                //
                //  Check to see if the current byte coupled with the previous
                //  byte will satisfy the requirement. The check uses the high
                //  part of the previous byte and low part of the current byte.
                //

                if (((ULONG)RtlpBitsClearHigh[PreviousByte] +
                           (ULONG)RtlpBitsClearLow[CurrentByte]) >= NumberToFind) {

                    ULONG StartingIndex;

                    //
                    //  It all fits in these two bytes, so we can compute
                    //  the starting index.  This is done by taking the
                    //  index of the current byte (bit 0) and subtracting the
                    //  number of bits its takes to get to the first cleared
                    //  high bit.
                    //

                    StartingIndex = CurrentBitIndex -
                                             (LONG)RtlpBitsClearHigh[PreviousByte];

                    //
                    //  Now make sure the total size isn't beyond the bitmap
                    //

                    if ((StartingIndex + NumberToFind) <= SizeOfBitMap) {

                        return StartingIndex;
                    }
                }

                //
                //  The previous byte does not help, so check the current byte.
                //

                if ((ULONG)RtlpBitsClearAnywhere[CurrentByte] >= NumberToFind) {

                    UCHAR BitMask;
                    ULONG i;

                    //
                    //  It all fits in a single byte, so calculate the bit
                    //  number.  We do this by taking a mask of the appropriate
                    //  size and shifting it over until it fits.  It fits when
                    //  we can bitwise-and the current byte with the bitmask
                    //  and get a zero back.
                    //

                    BitMask = FillMask[ NumberToFind ];
                    for (i = 0; (BitMask & CurrentByte) != 0; i += 1) {

                        BitMask <<= 1;
                    }

                    //
                    //  return to our caller the located bit index, and the
                    //  number that we found.
                    //

                    return CurrentBitIndex + i;
                }

                //
                //  For the next iteration through our loop we need to make
                //  the current byte into the previous byte, and go to the
                //  top of the loop again.
                //

                PreviousByte = CurrentByte;

                //
                //  Increment our Bit Index, and either exit, or get the
                //  next byte.
                //

                CurrentBitIndex += 8;

                if ( CurrentBitIndex < EndByteIndex * 8 ) {

                    GET_BYTE( CurrentByte );

                } else {

                    break;
                }

            } // end loop CurrentBitIndex

        //
        //  The number to find is greater than 9 but if it is less than 15
        //  then we know it can be satisfied with at most 2 bytes, or 3 bytes
        //  if the middle byte (of the 3) is all zeros.
        //

        } else if (NumberToFind < 15) {

            ULONG CurrentBitIndex;

            UCHAR PreviousPreviousByte;
            UCHAR PreviousByte;

            PreviousByte = 0xff;

            //
            //  Examine all the bytes within our test range searching
            //  for a fit
            //

            CurrentBitIndex = StartByteIndex * 8;

            while (TRUE) {

                //
                //  For the next iteration through our loop we need to make
                //  the current byte into the previous byte, the previous
                //  byte into the previous previous byte, and go forward.
                //

                PreviousPreviousByte = PreviousByte;
                PreviousByte = CurrentByte;

                //
                //  Increment our Bit Index, and either exit, or get the
                //  next byte.
                //

                CurrentBitIndex += 8;

                if ( CurrentBitIndex < EndByteIndex * 8 ) {

                    GET_BYTE( CurrentByte );

                } else {

                    break;
                }

                //
                //  if the previous byte is all zeros then maybe the
                //  request can be satisfied using the Previous Previous Byte
                //  Previous Byte, and the Current Byte.
                //

                if ((PreviousByte == 0)

                    &&

                    (((ULONG)RtlpBitsClearHigh[PreviousPreviousByte] + 8 +
                          (ULONG)RtlpBitsClearLow[CurrentByte]) >= NumberToFind)) {

                    ULONG StartingIndex;

                    //
                    //  It all fits in these three bytes, so we can compute
                    //  the starting index.  This is done by taking the
                    //  index of the previous byte (bit 0) and subtracting
                    //  the number of bits its takes to get to the first
                    //  cleared high bit.
                    //

                    StartingIndex = (CurrentBitIndex - 8) -
                                     (LONG)RtlpBitsClearHigh[PreviousPreviousByte];

                    //
                    //  Now make sure the total size isn't beyond the bitmap
                    //

                    if ((StartingIndex + NumberToFind) <= SizeOfBitMap) {

                        return StartingIndex;
                    }
                }

                //
                //  Check to see if the Previous byte and current byte
                //  together satisfy the request.
                //

                if (((ULONG)RtlpBitsClearHigh[PreviousByte] +
                           (ULONG)RtlpBitsClearLow[CurrentByte]) >= NumberToFind) {

                    ULONG StartingIndex;

                    //
                    //  It all fits in these two bytes, so we can compute
                    //  the starting index.  This is done by taking the
                    //  index of the current byte (bit 0) and subtracting the
                    //  number of bits its takes to get to the first cleared
                    //  high bit.
                    //

                    StartingIndex = CurrentBitIndex -
                                             (LONG)RtlpBitsClearHigh[PreviousByte];

                    //
                    //  Now make sure the total size isn't beyond the bitmap
                    //

                    if ((StartingIndex + NumberToFind) <= SizeOfBitMap) {

                        return StartingIndex;
                    }
                }

            } // end loop CurrentBitIndex

        //
        //  The number to find is greater than or equal to 15.  This request
        //  has to have at least one byte of all zeros to be satisfied
        //

        } else {

            ULONG CurrentByteIndex;

            ULONG ZeroBytesNeeded;
            ULONG ZeroBytesFound;

            UCHAR StartOfRunByte;
            LONG StartOfRunIndex;

            //
            //  First precalculate how many zero bytes we're going to need
            //

            ZeroBytesNeeded = (NumberToFind - 7) / 8;

            //
            //  Indicate for the first time through our loop that we haven't
            //  found a zero byte yet, and indicate that the start of the
            //  run is the byte just before the start byte index
            //

            ZeroBytesFound = 0;
            StartOfRunByte = 0xff;
            StartOfRunIndex = StartByteIndex - 1;

            //
            //  Examine all the bytes in our test range searching for a fit
            //

            CurrentByteIndex = StartByteIndex;

            while (TRUE) {

                //
                //  If the number of zero bytes fits our minimum requirements
                //  then we can do the additional test to see if we
                //  actually found a fit
                //

                if ((ZeroBytesFound >= ZeroBytesNeeded - 1)

                        &&

                    ((ULONG)RtlpBitsClearHigh[StartOfRunByte] + ZeroBytesFound*8 +
                     (ULONG)RtlpBitsClearLow[CurrentByte]) >= NumberToFind) {

                    ULONG StartingIndex;

                    //
                    //  It all fits in these bytes, so we can compute
                    //  the starting index.  This is done by taking the
                    //  StartOfRunIndex times 8 and adding the number of bits
                    //  it takes to get to the first cleared high bit.
                    //

                    StartingIndex = (StartOfRunIndex * 8) +
                                     (8 - (LONG)RtlpBitsClearHigh[StartOfRunByte]);

                    //
                    //  Now make sure the total size isn't beyond the bitmap
                    //

                    if ((StartingIndex + NumberToFind) <= SizeOfBitMap) {

                        return StartingIndex;
                    }
                }

                //
                //  Check to see if the byte is zero and increment
                //  the number of zero bytes found
                //

                if (CurrentByte == 0) {

                    ZeroBytesFound += 1;

                //
                //  The byte isn't a zero so we need to start over again
                //  looking for zero bytes.
                //

                } else {

                    ZeroBytesFound = 0;
                    StartOfRunByte = CurrentByte;
                    StartOfRunIndex = CurrentByteIndex;
                }

                //
                //  Increment our Byte Index, and either exit, or get the
                //  next byte.
                //

                CurrentByteIndex += 1;

                if ( CurrentByteIndex < EndByteIndex ) {

                    GET_BYTE( CurrentByte );

                } else {

                    break;
                }

            } // end loop CurrentByteIndex
        }
    }

    //
    //  We never found a fit so we'll return -1
    //

    return 0xffffffff;
}


NTSTATUS
  KeWaitForMultipleObjects(
    IN ULONG  Count,
    IN PVOID  Object[],
    IN WAIT_TYPE  WaitType,
    IN KWAIT_REASON  WaitReason,
    IN KPROCESSOR_MODE  WaitMode,
    IN BOOLEAN  Alertable,
    IN PLARGE_INTEGER  Timeout  OPTIONAL,
    IN PKWAIT_BLOCK  WaitBlockArray  OPTIONAL
    )
{
	UNREFERENCED_PARAMETER(Count);
	UNREFERENCED_PARAMETER(Object);
	UNREFERENCED_PARAMETER(WaitType);
	UNREFERENCED_PARAMETER(WaitReason);
	UNREFERENCED_PARAMETER(WaitMode);
	UNREFERENCED_PARAMETER(Alertable);
	UNREFERENCED_PARAMETER(Timeout);
	UNREFERENCED_PARAMETER(WaitBlockArray);

	return STATUS_SUCCESS;

}

VOID
FASTCALL
__security_check_cookie(PVOID p)
{
	UNREFERENCED_PARAMETER(p);
//	return 0;
}