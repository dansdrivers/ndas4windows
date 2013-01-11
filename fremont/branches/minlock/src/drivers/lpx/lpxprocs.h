/*++

Copyright (C) 2002-2007 XIMETA, Inc.
All rights reserved.

This header file defines private functions for the NT LPX transport
provider.

--*/

#ifndef _LPXPROCS_
#define _LPXPROCS_


#define LPX_BUG		1
//
// MACROS.
//
//
// Debugging aids
//

//
//  VOID
//  IF_LPXDBG(
//      IN PSZ Message
//      );
//

#if DBG
#define IF_LPXDBG(flags) \
    if (LpxDebug & (flags))
#else
#define IF_LPXDBG(flags) \
    if (0)
#endif

//
//  VOID
//  PANIC(
//      IN PSZ Message
//      );
//

#if DBG
#define PANIC(Msg) \
    DbgPrint ((Msg))
#else
#define PANIC(Msg)
#endif


//
// These are define to allow DbgPrints that disappear when
// DBG is 0.
//

#if DBG
#define LpxPrint0(fmt) DbgPrint(fmt)
#define LpxPrint1(fmt,v0) DbgPrint(fmt,v0)
#define LpxPrint2(fmt,v0,v1) DbgPrint(fmt,v0,v1)
#define LpxPrint3(fmt,v0,v1,v2) DbgPrint(fmt,v0,v1,v2)
#define LpxPrint4(fmt,v0,v1,v2,v3) DbgPrint(fmt,v0,v1,v2,v3)
#define LpxPrint5(fmt,v0,v1,v2,v3,v4) DbgPrint(fmt,v0,v1,v2,v3,v4)
#define LpxPrint6(fmt,v0,v1,v2,v3,v4,v5) DbgPrint(fmt,v0,v1,v2,v3,v4,v5)
#else
#define LpxPrint0(fmt)
#define LpxPrint1(fmt,v0)
#define LpxPrint2(fmt,v0,v1)
#define LpxPrint3(fmt,v0,v1,v2)
#define LpxPrint4(fmt,v0,v1,v2,v3)
#define LpxPrint5(fmt,v0,v1,v2,v3,v4)
#define LpxPrint6(fmt,v0,v1,v2,v3,v4,v5)
#endif

//
// The REFCOUNTS message take up a lot of room, so make
// removing them easy.
//

#if 1
#define IF_REFDBG IF_LPXDBG (LPX_DEBUG_REFCOUNTS)
#else
#define IF_REFDBG if (0)
#endif

#if 1 //DBG

#if DBG

#define LpxReferenceConnection(Reason, Connection, Type)\
    if ((Connection)->Destroyed) { \
        DbgPrint("LPX: Attempt to reference destroyed conn %p\n", Connection); \
        DbgBreakPoint(); \
    } \
    IF_REFDBG { \
        DbgPrint ("RefC %p: %s %s, %ld : %ld\n", Connection, Reason, __FILE__, __LINE__, (Connection)->ReferenceCount);\
    } \
    (VOID)ExInterlockedAddUlong ( \
        (PULONG)(&(Connection)->RefTypes[Type]), \
        1, \
        &LpxGlobalInterlock); \
    LpxRefConnection (Connection)

#define LpxDereferenceConnection(Reason, Connection, Type)\
    if ((Connection)->Destroyed) { \
        DbgPrint("LPX: Attempt to dereference destroyed conn %p\n", Connection); \
        DbgBreakPoint(); \
    } \
    IF_REFDBG { \
        DbgPrint ("DeRefC %p: %s %s, %ld : %ld\n", Connection, Reason, __FILE__, __LINE__, (Connection)->ReferenceCount);\
    } \
    (VOID)ExInterlockedAddUlong ( \
        (PULONG)&((Connection)->RefTypes[Type]), \
        (ULONG)-1, \
        &LpxGlobalInterlock); \
    LpxDerefConnection (Connection)

#define LpxDereferenceConnectionMacro(Reason, Connection, Type)\
    LpxDereferenceConnection(Reason, Connection, Type)

#define LpxDereferenceConnectionSpecial(Reason, Connection, Type)\
    IF_REFDBG { \
        DbgPrint ("DeRefCL %p: %s %s, %ld : %ld\n", Connection, Reason, __FILE__, __LINE__, (Connection)->ReferenceCount);\
    } \
    (VOID)ExInterlockedAddUlong ( \
        (PULONG)&((Connection)->RefTypes[Type]), \
        (ULONG)-1, \
        &LpxGlobalInterlock); \
    LpxDerefConnectionSpecial (Connection)

#else

#define LpxReferenceConnection(Reason, Connection, Type)\
    if (((Connection)->ReferenceCount == -1) &&   \
        ((Connection)->SpecialRefCount == 0))     \
        DbgBreakPoint();                          \
                                                  \
    if (InterlockedIncrement( \
            &(Connection)->ReferenceCount) == 0) { \
        ExInterlockedAddUlong( \
            (PULONG)(&(Connection)->SpecialRefCount), \
            1, \
            (Connection)->ProviderInterlock); \
    }

#define LpxDereferenceConnection(Reason, Connection, Type)\
    if (((Connection)->ReferenceCount == -1) &&   \
        ((Connection)->SpecialRefCount == 0))     \
        DbgBreakPoint();                          \
                                                  \
    LpxDerefConnection (Connection)

#if __LPX__

#define LpxDereferenceConnectionMacro(Reason, Connection, Type){ \
    if (((Connection)->ReferenceCount == -1) &&   \
        ((Connection)->SpecialRefCount == 0))     \
        DbgBreakPoint();                          \
                                                  \
                                                  \
    if (InterlockedDecrement( \
            &(Connection)->ReferenceCount) < 0) { \
        LpxDerefConnectionSpecial (Connection); \
    } \
}

#endif

#define LpxDereferenceConnectionSpecial(Reason, Connection, Type)\
    LpxDerefConnectionSpecial (Connection)

#endif

#define LpxReferenceSendIrp( Reason, IrpSp, Type)\
    IF_REFDBG {   \
        DbgPrint ("RefSI %p: %s %s, %ld : %ld\n", IrpSp, Reason, __FILE__, __LINE__, IRP_SEND_REFCOUNT(IrpSp));}\
    LpxRefSendIrp (IrpSp)

#define LpxDereferenceSendIrp(Reason, IrpSp, Type)\
    IF_REFDBG { \
        DbgPrint ("DeRefSI %p: %s %s, %ld : %ld\n", IrpSp, Reason, __FILE__, __LINE__, IRP_SEND_REFCOUNT(IrpSp));\
    } \
    LpxDerefSendIrp (IrpSp)

#define LpxReferenceReceiveIrpLocked( Reason, IrpSp, Type)\
    IF_REFDBG {   \
        DbgPrint ("RefRI %p: %s %s, %ld : %ld\n", IrpSp, Reason, __FILE__, __LINE__, IRP_RECEIVE_REFCOUNT(IrpSp));}\
    LpxRefReceiveIrpLocked (IrpSp)

#define LpxDereferenceReceiveIrp(Reason, IrpSp, Type)\
    IF_REFDBG { \
        DbgPrint ("DeRefRI %p: %s %s, %ld : %ld\n", IrpSp, Reason, __FILE__, __LINE__, IRP_RECEIVE_REFCOUNT(IrpSp));\
    } \
    LpxDerefReceiveIrp (IrpSp)

#define LpxDereferenceReceiveIrpLocked(Reason, IrpSp, Type)\
    IF_REFDBG { \
        DbgPrint ("DeRefRILocked %p: %s %s, %ld : %ld\n", IrpSp, Reason, __FILE__, __LINE__, IRP_RECEIVE_REFCOUNT(IrpSp));\
    } \
    LpxDerefReceiveIrpLocked (IrpSp)

#if DBG

#define LpxReferenceAddress( Reason, Address, Type)\
    IF_REFDBG {   \
        DbgPrint ("RefA %p: %s %s, %ld : %ld\n", Address, Reason, __FILE__, __LINE__, (Address)->ReferenceCount);}\
    (VOID)ExInterlockedAddUlong ( \
        (PULONG)(&(Address)->RefTypes[Type]), \
        1, \
        &LpxGlobalInterlock); \
    LpxRefAddress (Address)

#define LpxDereferenceAddress(Reason, Address, Type)\
    IF_REFDBG { \
        DbgPrint ("DeRefA %p: %s %s, %ld : %ld\n", Address, Reason, __FILE__, __LINE__, (Address)->ReferenceCount);\
    } \
    (VOID)ExInterlockedAddUlong ( \
        (PULONG)(&(Address)->RefTypes[Type]), \
        (ULONG)-1, \
        &LpxGlobalInterlock); \
    LpxDerefAddress (Address)

#else

#define LpxReferenceAddress(Reason, Address, Type)\
    if ((Address)->ReferenceCount <= 0){ DbgBreakPoint(); }\
    (VOID)InterlockedIncrement(&(Address)->ReferenceCount)

#define LpxDereferenceAddress(Reason, Address, Type)\
    if ((Address)->ReferenceCount <= 0){ DbgBreakPoint(); }\
    LpxDerefAddress (Address)

#endif

#if DBG

#define LpxReferenceDeviceContext( Reason, DeviceContext, Type)\
    if ((DeviceContext)->ReferenceCount == 0)     \
        DbgBreakPoint();                          \
    IF_REFDBG {   \
        DbgPrint ("RefDC %p: %s %s, %ld : %ld\n", DeviceContext, Reason, __FILE__, __LINE__, (DeviceContext)->ReferenceCount);}\
    (VOID)ExInterlockedAddUlong ( \
        (PULONG)(&(DeviceContext)->RefTypes[Type]), \
        1, \
        &LpxGlobalInterlock); \
    LpxRefDeviceContext (DeviceContext)

#define LpxDereferenceDeviceContext(Reason, DeviceContext, Type)\
    if ((DeviceContext)->ReferenceCount == 0)     \
        DbgBreakPoint();                          \
    IF_REFDBG { \
        DbgPrint ("DeRefDC %p: %s %s, %ld : %ld\n", DeviceContext, Reason, __FILE__, __LINE__, (DeviceContext)->ReferenceCount);\
    } \
    (VOID)ExInterlockedAddUlong ( \
        (PULONG)(&(DeviceContext)->RefTypes[Type]), \
        (ULONG)-1, \
        &LpxGlobalInterlock); \
    LpxDerefDeviceContext (DeviceContext)

#else

#define LpxReferenceDeviceContext(Reason, DeviceContext, Type)\
    if ((DeviceContext)->ReferenceCount == 0)                 \
        DbgBreakPoint();                                      \
    LpxRefDeviceContext (DeviceContext)

#define LpxDereferenceDeviceContext(Reason, DeviceContext, Type)\
    if ((DeviceContext)->ReferenceCount == 0)                   \
        DbgBreakPoint();                                        \
    LpxDerefDeviceContext (DeviceContext)

#endif

#else

#define LpxReferenceConnection(Reason, Connection, Type)\
    if (((Connection)->ReferenceCount == -1) &&   \
        ((Connection)->SpecialRefCount == 0))     \
        DbgBreakPoint();                          \
                                                  \
    if (InterlockedIncrement( \
            &(Connection)->ReferenceCount) == 0) { \
        ExInterlockedAddUlong( \
            (PULONG)(&(Connection)->SpecialRefCount), \
            1, \
            (Connection)->ProviderInterlock); \
    }

#define LpxDereferenceConnection(Reason, Connection, Type)\
    if (((Connection)->ReferenceCount == -1) &&   \
        ((Connection)->SpecialRefCount == 0))     \
        DbgBreakPoint();                          \
                                                  \
    LpxDerefConnection (Connection)

#if __LPX__

#define LpxDereferenceConnectionMacro(Reason, Connection, Type){ \
    if (((Connection)->ReferenceCount == -1) &&   \
        ((Connection)->SpecialRefCount == 0))     \
        DbgBreakPoint();                          \
                                                  \
                                                  \
    if (InterlockedDecrement( \
            &(Connection)->ReferenceCount) < 0) { \
        LpxDerefConnectionSpecial (Connection); \
    } \
}

#endif


#define LpxDereferenceConnectionSpecial(Reason, Connection, Type)\
    LpxDerefConnectionSpecial (Connection)

#define LpxReferenceSendIrp(Reason, IrpSp, Type)\
    (VOID)InterlockedIncrement( \
        &IRP_SEND_REFCOUNT(IrpSp))

#if __LPX__

#define LpxDereferenceSendIrp(Reason, IrpSp, Type) {\
    PIO_STACK_LOCATION _IrpSp = (IrpSp); \
    if (InterlockedDecrement( \
            &IRP_SEND_REFCOUNT(_IrpSp)) == 0) { \
        PIRP _Irp = IRP_SEND_IRP(_IrpSp); \
        IRP_SEND_REFCOUNT(_IrpSp) = 0; \
        IRP_SEND_IRP (_IrpSp) = NULL; \
		{ \
			KIRQL	ilgu_cancelIrql; \
			IoAcquireCancelSpinLock(&ilgu_cancelIrql); \
			IoSetCancelRoutine(_Irp, NULL) \
			IoReleaseCancelSpinLock(ilgu_cancelIrql); \
			IoCompleteRequest (_Irp, IO_NETWORK_INCREMENT); } \
		} \
	} \
}

#endif // __LPX__

#define LpxReferenceReceiveIrpLocked(Reason, IrpSp, Type)\
    ++IRP_RECEIVE_REFCOUNT(IrpSp)

#define LpxDereferenceReceiveIrp(Reason, IrpSp, Type)\
    LpxDerefReceiveIrp (IrpSp)

#define LpxDereferenceReceiveIrpLocked(Reason, IrpSp, Type) { \
    if (--IRP_RECEIVE_REFCOUNT(IrpSp) == 0) { \
        ExInterlockedInsertTailList( \
            &(IRP_DEVICE_CONTEXT(IrpSp)->IrpCompletionQueue), \
            &(IRP_RECEIVE_IRP(IrpSp))->Tail.Overlay.ListEntry, \
            &(IRP_DEVICE_CONTEXT(IrpSp)->Interlock)); \
    } \
}

#define LpxReferenceAddress(Reason, Address, Type)\
    if ((Address)->ReferenceCount <= 0){ DbgBreakPoint(); }\
    (VOID)InterlockedIncrement(&(Address)->ReferenceCount)

#define LpxDereferenceAddress(Reason, Address, Type)\
    if ((Address)->ReferenceCount <= 0){ DbgBreakPoint(); }\
    LpxDerefAddress (Address)

#define LpxReferenceDeviceContext(Reason, DeviceContext, Type)\
    if ((DeviceContext)->ReferenceCount == 0)                 \
        DbgBreakPoint();                                      \
    LpxRefDeviceContext (DeviceContext)

#define LpxDereferenceDeviceContext(Reason, DeviceContext, Type)\
    if ((DeviceContext)->ReferenceCount == 0)                   \
        DbgBreakPoint();                                        \
    LpxDerefDeviceContext (DeviceContext)

#endif


//
// Error and statistics Macros
//


//  VOID
//  LogErrorToSystem(
//      NTSTATUS ErrorType,
//      PUCHAR ErrorDescription
//      )

/*++

Routine Description:

    This routine is called to log an error from the transport to the system.
    Errors that are of system interest should be logged using this interface.
    For now, this macro is defined trivially.

Arguments:

    ErrorType - The error type, a conventional NT status

    ErrorDescription - A pointer to a string describing the error.

Return Value:

    none.

--*/

#if DBG
#define LogErrorToSystem( ErrorType, ErrorDescription)                    \
            DbgPrint ("Logging error: File: %s Line: %ld \n Description: %s\n",__FILE__, __LINE__, ErrorDescription)
#else
#define LogErrorToSystem( ErrorType, ErrorDescription)
#endif


//
// Routines in TIMER.C (lightweight timer system package).
// Note that all the start and stop routines for the timers assume that you
// have the link spinlock when you call them!
// Note also that, with the latest revisions, the timer system now works by
// putting those links that have timers running on a list of links to be looked
// at for each clock tick. This list is ordered, with the most recently inserted
// elements at the tail of the list. Note further that anything already on the
// is moved to the end of the list if the timer is restarted; thus, the list
// order is preserved.
//

VOID
LpxInitializeTimerSystem(
    IN PDEVICE_CONTEXT DeviceContext
    );

VOID
LpxStopTimerSystem(
    IN PDEVICE_CONTEXT DeviceContext
    );

//
// Timer Macros - these are make sure that no timers are
// executing after we finish call to LpxStopTimerSystem
//
// State Descriptions -
//
// If TimerState is
//      <  TIMERS_ENABLED       -   Multiple ENABLE_TIMERS happened,
//                                  Will be corrected in an instant
//
//      =  TIMERS_ENABLED       -   ENABLE_TIMERS done but no timers
//                                  that have gone through START_TIMER
//                                  but not yet executed a LEAVE_TIMER
//
//      >  TIMERS_ENABLED &&
//      <  TIMERS_DISABLED      -   ENABLE_TIMERS done and num timers =
//                                  (TimerInitialized - TIMERS_ENABLED)
//                                  that have gone through START_TIMER
//                                  but not yet executed a LEAVE_TIMER
//
//      = TIMERS_DISABLED       -   DISABLE_TIMERS done and no timers
//                                  executing timer code at this pt
//                                  [This is also the initial state]
//
//      > TIMERS_DISABLED &&
//      < TIMERS_DISABLED + TIMERS_RANGE
//                              -   DISABLE_TIMERS done and num timers =
//                                  (TimerInitialized - TIMERS_ENABLED)
//                                  that have gone through START_TIMER
//                                  but not yet executed a LEAVE_TIMER
//
//      >= TIMERS_DISABLED + TIMERS_RANGE
//                              -   Multiple DISABLE_TIMERS happened,
//                                  Will be corrected in an instant
//
//  Allow basically TIMER_RANGE = 2^24 timers 
//  (and 2^8 / 2 simultaneous stops or starts)
//

#if DBG_TIMER
#define DbgTimer DbgPrint
#else
#define DbgTimer
#endif

#define TIMERS_ENABLED      0x08000000
#define TIMERS_DISABLED     0x09000000
#define TIMERS_RANGE_ADD    0x01000000 /* TIMERS_DISABLED - TIMERS_ENABLED */
#define TIMERS_RANGE_SUB    0xFF000000 /* TIMERS_ENABLED - TIMERS_DISABLED */

#define INITIALIZE_TIMER_STATE(DeviceContext)                               \
        DbgTimer("*--------------- Timers State Initialized ---------*\n"); \
        /* Initial state is set to timers disabled */                       \
        DeviceContext->TimerState = TIMERS_DISABLED;                        \

#define TIMERS_INITIALIZED(DeviceContext)                                   \
        (DeviceContext->TimerState == TIMERS_DISABLED)                      \

#define ENABLE_TIMERS(DeviceContext)                                        \
    {                                                                       \
        ULONG Count;                                                        \
                                                                            \
        DbgTimer("*--------------- Enabling Timers ------------------*\n"); \
        Count= InterlockedExchangeAdd(&DeviceContext->TimerState,           \
                                      TIMERS_RANGE_SUB);                    \
        DbgTimer("Count = %08x, TimerState = %08x\n", Count,                \
                    DeviceContext->TimerState);                             \
        if (Count < TIMERS_ENABLED)                                         \
        {                                                                   \
        DbgTimer("*--------------- Timers Already Enabled -----------*\n"); \
            /* We have already enabled the timers */                        \
            InterlockedExchangeAdd(&DeviceContext->TimerState,              \
                                   TIMERS_RANGE_ADD);                       \
        DbgTimer("Count = %08x, TimerState = %08x\n", Count,                \
                    DeviceContext->TimerState);                             \
        }                                                                   \
        DbgTimer("*--------------- Enabling Timers Done -------------*\n"); \
    }                                                                       \

#if __LPX__

#define DISABLE_TIMERS(DeviceContext)                                       \
    {                                                                       \
        ULONG Count;                                                        \
                                                                            \
        DbgTimer("*--------------- Disabling Timers -----------------*\n"); \
        Count= InterlockedExchangeAdd(&DeviceContext->TimerState,           \
                                      TIMERS_RANGE_ADD);                    \
        DbgTimer("Count = %08x, TimerState = %08x\n", Count,                \
                    DeviceContext->TimerState);                             \
        if (Count >= TIMERS_DISABLED)                                       \
        {                                                                   \
        DbgTimer("*--------------- Timers Already Disabled ----------*\n"); \
            /* We have already disabled the timers */                       \
            InterlockedExchangeAdd(&DeviceContext->TimerState,              \
                                   TIMERS_RANGE_SUB);                       \
        DbgTimer("Count = %08x, TimerState = %08x\n", Count,                \
                    DeviceContext->TimerState);                             \
        }                                                                   \
                                                                            \
        /* Loop until we have zero timers active */                         \
        while (((ULONG)DeviceContext->TimerState)!=TIMERS_DISABLED)\
            DbgTimer("Number of timers active = %08x\n",                    \
                      DeviceContext->TimerState                             \
                         - TIMERS_DISABLED);                                \
        DbgTimer("*--------------- Disabling Timers Done ------------*\n"); \
    }                                                                       \

#endif

#define START_TIMER(DeviceContext, TimerId, Timer, DueTime, Dpc)            \
        /*DbgTimer("*---------- Entering Timer %d ---------*\n", TimerId);*/\
        if (InterlockedIncrement(&DeviceContext->TimerState) <              \
                TIMERS_DISABLED)                                            \
        {                                                                   \
            KeSetTimer(Timer, DueTime, Dpc);                                \
        }                                                                   \
        else                                                                \
        {                                                                   \
            /* Timers disabled - get out and reset */                       \
            LpxDereferenceDeviceContext("Timers disabled",                  \
                                         DeviceContext,                     \
                                         DCREF_SCAN_TIMER);                 \
            LEAVE_TIMER(DeviceContext, TimerId);                            \
        }                                                                   \
        /*DbgTimer("*---------- Entering Done  %d ---------*\n", TimerId);*/\

#define LEAVE_TIMER(DeviceContext, TimerId)                                 \
        /* Get out and adjust the time count */                             \
        /*DbgTimer("*---------- Leaving Timer %d ---------*\n", TimerId);*/ \
        InterlockedDecrement(&DeviceContext->TimerState);                   \
        /*DbgTimer("*---------- Leaving Done  %d ---------*\n", TimerId);*/ \


// Basic timer types (just for debugging)
#define LONG_TIMER          0
#define SHORT_TIMER         1


//
// These routines are used to maintain counters.
//

#define INCREMENT_COUNTER(_DeviceContext,_Field) \
    ++(_DeviceContext)->Statistics._Field

#define DECREMENT_COUNTER(_DeviceContext,_Field) \
    --(_DeviceContext)->Statistics._Field

#define ADD_TO_LARGE_INTEGER(_LargeInteger,_Ulong) \
    ExInterlockedAddLargeStatistic((_LargeInteger), (ULONG)(_Ulong))


//
// Routines in SEND.C (Receive engine).
//

NTSTATUS
LpxTdiSend(
    IN PIRP Irp
    );

NTSTATUS
LpxTdiSendDatagram(
    IN PIRP Irp
    );

VOID
LpxSendCompletionHandler(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET NdisPacket,
    IN NDIS_STATUS NdisStatus
    );

//
// Routines in DEVCTX.C (TP_DEVCTX object manager).
//

VOID
LpxRefDeviceContext(
    IN PDEVICE_CONTEXT DeviceContext
    );

VOID
LpxDerefDeviceContext(
    IN PDEVICE_CONTEXT DeviceContext
    );

NTSTATUS
LpxCreateDeviceContext(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING DeviceName,
    IN OUT PDEVICE_CONTEXT *DeviceContext
    );

VOID
LpxDestroyDeviceContext(
    IN PDEVICE_CONTEXT DeviceContext
    );


//
// Routines in ADDRESS.C (TP_ADDRESS object manager).
//

#if DBG
VOID
LpxRefAddress(
    IN PTP_ADDRESS Address
    );
#endif

VOID
LpxDerefAddress(
    IN PTP_ADDRESS Address
    );

VOID
LpxAllocateAddressFile(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_ADDRESS_FILE *TransportAddressFile
    );

VOID
LpxDeallocateAddressFile(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PTP_ADDRESS_FILE TransportAddressFile
    );

NTSTATUS
LpxCreateAddressFile(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_ADDRESS_FILE * AddressFile
    );

VOID
LpxReferenceAddressFile(
    IN PTP_ADDRESS_FILE AddressFile
    );

VOID
LpxDereferenceAddressFile(
    IN PTP_ADDRESS_FILE AddressFile
    );

VOID
LpxDestroyAddress(
    IN PVOID Parameter
    );

NTSTATUS
LpxOpenAddress(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
LpxCloseAddress(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

VOID
LpxStopAddress(
    IN PTP_ADDRESS Address
    );

VOID
LpxRegisterAddress(
    IN PTP_ADDRESS Address
    );

VOID
LpxAllocateAddress(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_ADDRESS *TransportAddress
    );

VOID
LpxDeallocateAddress(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PTP_ADDRESS TransportAddress
    );

NTSTATUS
LpxCreateAddress(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PLPX_ADDRESS NetworkName,
    OUT PTP_ADDRESS *Address
    );

NTSTATUS
LpxStopAddressFile(
    IN PTP_ADDRESS_FILE AddressFile,
    IN PTP_ADDRESS Address
    );

VOID
AddressTimeoutHandler(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

TDI_ADDRESS_NETBIOS *
LpxParseTdiAddress(
    IN TRANSPORT_ADDRESS UNALIGNED * TransportAddress,
    IN BOOLEAN BroadcastAddressOk
);

BOOLEAN
LpxValidateTdiAddress(
    IN TRANSPORT_ADDRESS UNALIGNED * TransportAddress,
    IN ULONG TransportAddressLength
);

NTSTATUS
LpxVerifyAddressObject (
    IN PTP_ADDRESS_FILE AddressFile
    );

//
// Routines in CONNECT.C.
//

NTSTATUS
LpxTdiAccept(
    IN PIRP Irp
    );

NTSTATUS
LpxTdiConnect(
    IN PIRP Irp
    );

NTSTATUS
LpxTdiDisconnect(
    IN PIRP Irp
    );

NTSTATUS
LpxTdiDisassociateAddress (
    IN PIRP Irp
    );

NTSTATUS
LpxTdiAssociateAddress(
    IN PIRP Irp
    );

NTSTATUS
LpxTdiListen(
    IN PIRP Irp
    );

NTSTATUS
LpxOpenConnection(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

NTSTATUS
LpxCloseConnection(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PIO_STACK_LOCATION IrpSp
    );

//
//
// Routines in CONNOBJ.C (TP_CONNECTION object manager).
//

#if DBG
VOID
LpxRefConnection(
    IN PTP_CONNECTION TransportConnection
    );
#endif

VOID
LpxDerefConnection(
    IN PTP_CONNECTION TransportConnection
    );

VOID
LpxDerefConnectionSpecial(
    IN PTP_CONNECTION TransportConnection
    );

VOID
LpxStopConnection(
    IN PTP_CONNECTION TransportConnection,
    IN NTSTATUS Status
    );

VOID
LpxCancelConnection(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
LpxAllocateConnection(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_CONNECTION *TransportConnection
    );

VOID
LpxDeallocateConnection(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PTP_CONNECTION TransportConnection
    );

NTSTATUS
LpxCreateConnection(
    IN PDEVICE_CONTEXT DeviceContext,
    OUT PTP_CONNECTION *TransportConnection
    );

VOID
ConnectionEstablishmentTimeout(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

NTSTATUS
LpxVerifyConnectionObject (
    IN PTP_CONNECTION Connection
    );

NTSTATUS
LpxIndicateDisconnect(
    IN PTP_CONNECTION TransportConnection
    );

//
// Routines in INFO.C (QUERY_INFO manager).
//

NTSTATUS
LpxTdiQueryInformation(
    IN PDEVICE_CONTEXT DeviceContext,
    IN PIRP Irp
    );

NTSTATUS
LpxTdiSetInformation(
    IN PIRP Irp
    );

//
// Routines in EVENT.C.
//

NTSTATUS
LpxTdiSetEventHandler(
    IN PIRP Irp
    );

//
// Routines in REQUEST.C (TP_REQUEST object manager).
//

#if 1 //DBG
VOID
LpxRefSendIrp(
    IN PIO_STACK_LOCATION IrpSp
    );

VOID
LpxDerefSendIrp(
    IN PIO_STACK_LOCATION IrpSp
    );
#endif

#if DBG
VOID
LpxRefReceiveIrpLocked(
    IN PIO_STACK_LOCATION IrpSp
    );
#endif

VOID
LpxDerefReceiveIrp(
    IN PIO_STACK_LOCATION IrpSp
    );

#if DBG
VOID
LpxDerefReceiveIrpLocked(
    IN PIO_STACK_LOCATION IrpSp
    );
#endif

//
// Routines in RCV.C (data copying routines for receives).
//

NTSTATUS
LpxTdiReceive(
    IN PIRP Irp
    );

NTSTATUS
LpxTdiReceiveDatagram(
    IN PIRP Irp
    );

//
// Routines in nbfndis.c.
//

#if DBG
PUCHAR
LpxGetNdisStatus (
    IN NDIS_STATUS NdisStatus
    );
#endif

//
// Routines in nbfdrvr.c
//

VOID
LpxWriteResourceErrorLog(
    IN PDEVICE_CONTEXT DeviceContext,
    IN NTSTATUS ErrorCode,
    IN ULONG UniqueErrorValue,
    IN ULONG BytesNeeded,
    IN ULONG ResourceId
    );

VOID
LpxWriteGeneralErrorLog(
    IN PDEVICE_CONTEXT DeviceContext,
    IN NTSTATUS ErrorCode,
    IN ULONG UniqueErrorValue,
    IN NTSTATUS FinalStatus,
    IN PWSTR SecondString,
    IN ULONG DumpDataCount,
    IN ULONG DumpData[]
    );

VOID
LpxWriteOidErrorLog(
    IN PDEVICE_CONTEXT DeviceContext,
    IN NTSTATUS ErrorCode,
    IN NTSTATUS FinalStatus,
    IN PWSTR AdapterString,
    IN ULONG OidValue
    );

VOID
LpxFreeResources(
    IN PDEVICE_CONTEXT DeviceContext
    );


extern
ULONG
LpxInitializeOneDeviceContext(
    OUT PNDIS_STATUS NdisStatus,
    IN PDRIVER_OBJECT DriverObject,
    IN PCONFIG_DATA LpxConfig,
    IN PUNICODE_STRING BindName,
    IN PUNICODE_STRING ExportName,
    IN PVOID SystemSpecific1,
    IN PVOID SystemSpecific2
    );


extern
VOID
LpxReInitializeDeviceContext(
    OUT PNDIS_STATUS NdisStatus,
    IN PDRIVER_OBJECT DriverObject,
    IN PCONFIG_DATA LpxConfig,
    IN PUNICODE_STRING BindName,
    IN PUNICODE_STRING ExportName,
    IN PVOID SystemSpecific1,
    IN PVOID SystemSpecific2
    );

//
// routines in lpxcnfg.c
//

NTSTATUS
LpxConfigureTransport (
    IN PUNICODE_STRING RegistryPath,
    IN PCONFIG_DATA * ConfigData
    );

NTSTATUS
LpxGetExportNameFromRegistry(
    IN  PUNICODE_STRING RegistryPath,
    IN  PUNICODE_STRING BindName,
    OUT PUNICODE_STRING ExportName
    );

//
// Routines in nbfndis.c
//

NTSTATUS
LpxRegisterProtocol (
    IN PUNICODE_STRING NameString
    );

VOID
LpxDeregisterProtocol (
    VOID
    );


NTSTATUS
LpxInitializeNdis (
    IN PDEVICE_CONTEXT DeviceContext,
    IN PCONFIG_DATA ConfigInfo,
    IN PUNICODE_STRING AdapterString
    );

VOID
LpxCloseNdis (
    IN PDEVICE_CONTEXT DeviceContext
    );


#if __LPX__

NTSTATUS
LpxAssignPort(
	IN PDEVICE_CONTEXT	AddressDeviceContext,
	IN PLPX_ADDRESS		SourceAddress
	);

PTP_ADDRESS
LpxLookupAddress(
    IN PDEVICE_CONTEXT	DeviceContext,
	IN PLPX_ADDRESS		SourceAddress
    );


NTSTATUS
LpxConnect(
    IN 		PTP_CONNECTION	Connection,
 	IN OUT	PIRP			Irp
   );

NTSTATUS
LpxListen(
    IN 		PTP_CONNECTION	Connection,
 	IN OUT	PIRP			Irp
   );

NDIS_STATUS
LpxAccept(
    IN 		PTP_CONNECTION	Connection,
 	IN OUT	PIRP			Irp
   );

NDIS_STATUS
LpxDisconnect(
    IN 		PTP_CONNECTION	Connection,
 	IN OUT	PIRP			Irp
    );


NTSTATUS
LpxSend(
    IN 		PTP_CONNECTION	Connection,
 	IN OUT	PIRP			Irp
   );

VOID
LpxSendComplete(
    IN NDIS_HANDLE   ProtocolBindingContext,
    IN PNDIS_PACKET  pPacket,
    IN NDIS_STATUS   Status
    );

NTSTATUS
LpxRecv(
    IN 		PTP_CONNECTION	Connection,
 	IN OUT	PIRP			Irp
   );


NDIS_STATUS
LpxSendDatagram(
    IN 		PTP_ADDRESS	Address,
 	IN OUT	PIRP		Irp
   );

NDIS_STATUS
LpxReceiveIndication(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_HANDLE MacReceiveContext,
    IN PVOID HeaderBuffer,
    IN UINT HeaderBufferSize,
    IN PVOID LookAheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    );

INT
LpxProtocolReceivePacket(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN PNDIS_PACKET  Packet
    );

VOID
LpxTransferDataComplete (
    IN NDIS_HANDLE   ProtocolBindingContext,
    IN PNDIS_PACKET  Packet,
    IN NDIS_STATUS   Status,
    IN UINT          BytesTransfered
    );

VOID
DeferredLpxReceiveComplete (
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
	);

VOID
LpxReceiveComplete (
	IN NDIS_HANDLE BindingContext
	);

VOID 
LpxIoCompleteRequest (
    IN PIRP  Irp,
    IN CCHAR  PriorityBoost
    );


#if DBG
VOID
SmpPrintState(
	IN	LONG	        Debuglevel,
	IN	PCHAR	        Where,
	IN PTP_CONNECTION	Connection
	);
#else
#define SmpPrintState(l, w, s) 
#endif


//@ILGU@ <2003_1120>
VOID 
LpxFreeDeviceContext(
					 IN PDEVICE_CONTEXT DeviceContext
					 );
//@ILGU@ <2003_1120>
extern LONG		NumberOfExportedPackets;
extern LONG		NumberOfAllockPackets;
extern LONG		NumberOfCloned;
extern ULONG	NumberOfSentComplete;

extern ULONG	NumberOfSent;
extern ULONG	NumberOfSentComplete;

PNDIS_PACKET
PacketDequeue(
	PLIST_ENTRY	PacketQueue,
	PKSPIN_LOCK	QSpinLock
	);

VOID
PacketFree2 (
	IN PNDIS_PACKET		Packet
	);

#define PacketFree( DeviceContext, Packet )	PacketFree2( Packet )

PNDIS_PACKET
PacketCopy(
	IN	PNDIS_PACKET Packet,
	OUT	PLONG		 CloneCount
	);

BOOLEAN
PacketQueueEmpty(
	PLIST_ENTRY	PacketQueue,
	PKSPIN_LOCK	QSpinLock
	);

VOID LpxChangeState(
	IN PTP_CONNECTION	Connection,
	IN SMP_STATE		NewState,
	IN BOOLEAN			Locked	
	);


VOID
SmpTimerDpcRoutine(
	IN PKDPC			Dpc,
	IN PTP_CONNECTION	Connection,
	IN PVOID			Junk1,
	IN PVOID			Junk2
	);

VOID
SmpWorkDpcRoutine(
	IN    PKDPC    dpc,
	IN    PVOID    Context,
	IN    PVOID    junk1,
	IN    PVOID    junk2
	);

NTSTATUS
SendPacketAlloc(
	IN  PDEVICE_CONTEXT		DeviceContext,
	IN  PTP_ADDRESS			Address,
	IN  UCHAR				DestinationAddressNode[],
	IN	PUCHAR				UserData,
	IN	ULONG				UserDataLength,
	IN	PIO_STACK_LOCATION	IrpSp,
	IN  UCHAR				Option,
	OUT	PNDIS_PACKET		*Packet
	);

NTSTATUS
RcvPacketAlloc(
	IN  PDEVICE_CONTEXT		DeviceContext,
	IN	ULONG				PacketDataLength,
	OUT	PNDIS_PACKET		*Packet
	);

#if DBG

VOID
PrintStatistics( 
	LONG			DebugLevel,
	PTP_CONNECTION Connection
	);

#else

#define PrintStatistics( a, b )

#endif

#if __LPX_STATISTICS__

VOID
PrintDeviceContextStatistics( 
	LONG			DebugLevel,
	PDEVICE_CONTEXT	DeviceContext
	);

#else
#define PrintDeviceContextStatistics( a, b )
#endif

#endif

#endif // def _LPXPROCS_
