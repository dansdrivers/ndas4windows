/*++

Copyright (c) 1989, 1990, 1991  Microsoft Corporation

Module Name:

    timer.c

Abstract:

    This module contains code that implements the lightweight timer system
    for the LPX protocol provider.  This is not a general-purpose timer system;
    rather, it is specific to servicing LLC (802.2) links with three timers
    each.

    Services are provided in macro form (see LPXPROCS.H) to start and stop
    timers.  This module contains the code that gets control when the timer
    in the device context expires as a result of calling kernel services.
    The routine scans the device context's link database, looking for timers
    that have expired, and for those that have expired, their expiration
    routines are executed.

Author:

    David Beaver (dbeaver) 1-July-1991

Environment:

    Kernel mode

Revision History:


--*/

#include "precomp.h"
#pragma hdrstop

ULONG StartTimer = 0;
ULONG StartTimerSet = 0;
ULONG StartTimerT1 = 0;
ULONG StartTimerT2 = 0;
ULONG StartTimerDelayedAck = 0;
ULONG StartTimerLinkDeferredAdd = 0;
ULONG StartTimerLinkDeferredDelete = 0;


#if DBG
extern ULONG LpxDebugPiggybackAcks;
ULONG LpxDebugShortTimer = 0;
#endif

#if DBG
//
// These are temp, to track how the timers are working
//
ULONG TimerInsertsAtEnd = 0;
ULONG TimerInsertsEmpty = 0;
ULONG TimerInsertsInMiddle = 0;
#endif

//
// These are constants calculated by InitializeTimerSystem
// to be the indicated amound divided by the tick increment.
//

ULONG LpxTickIncrement = 0;
ULONG LpxTwentyMillisecondsTicks = 0;
ULONG LpxMaximumIntervalTicks = 0;     // usually 60 seconds in ticks

VOID
StopStalledConnections(
    IN PDEVICE_CONTEXT DeviceContext
    );


VOID
ScanLongTimersDpc(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )

/*++

Routine Description:

    This routine is called at DISPATCH_LEVEL by the system at regular
    intervals to determine if any long timers have expired, and
    if they have, to execute their expiration routines.

Arguments:

    DeferredContext - Pointer to our DEVICE_CONTEXT object.

Return Value:

    none.

--*/

{
    LARGE_INTEGER DueTime;
	PDEVICE_CONTEXT DeviceContext;

    Dpc, SystemArgument1, SystemArgument2; // prevent compiler warnings

    ENTER_LPX;

    DeviceContext = DeferredContext;

    IF_LPXDBG (LPX_DEBUG_TIMERDPC) {
        LpxPrint0 ("ScanLongTimersDpc:  Entered.\n");
    }

    //
    // See if we got any multicast traffic last time.
    //

    if (DeviceContext->MulticastPacketCount == 0) {

        ++DeviceContext->LongTimeoutsWithoutMulticast;

        if (DeviceContext->EasilyDisconnected &&
            (DeviceContext->LongTimeoutsWithoutMulticast > 5)) {

            PLIST_ENTRY p;
            PTP_ADDRESS address;

            //
            // We have had five timeouts in a row with no
            // traffic, mark all the addresses as needing
            // reregistration next time a connect is
            // done on them.
            //

            ACQUIRE_DPC_SPIN_LOCK (&DeviceContext->SpinLock);

            for (p = DeviceContext->AddressDatabase.Flink;
                 p != &DeviceContext->AddressDatabase;
                 p = p->Flink) {

                address = CONTAINING_RECORD (p, TP_ADDRESS, Linkage);
                address->Flags |= ADDRESS_FLAGS_NEED_REREGISTER;

            }

            RELEASE_DPC_SPIN_LOCK (&DeviceContext->SpinLock);

            DeviceContext->LongTimeoutsWithoutMulticast = 0;

        }

    } else {

        DeviceContext->LongTimeoutsWithoutMulticast = 0;

    }

    DeviceContext->MulticastPacketCount = 0;


    //
    // Every thirty seconds, check for stalled connections
    //

    //
    // Scan for any uncompleted receive IRPs, this may happen if
    // the cable is pulled and we don't get any more ReceiveComplete
    // indications.

    LpxReceiveComplete((NDIS_HANDLE)DeviceContext);


    //
    // Start up the timer again.  Note that because we start the timer
    // after doing work (above), the timer values will slip somewhat,
    // depending on the load on the protocol.  This is entirely acceptable
    // and will prevent us from using the timer DPC in two different
    // threads of execution.
    //

    if (DeviceContext->State != DEVICECONTEXT_STATE_STOPPING) {
        DueTime.HighPart = -1;
        DueTime.LowPart = (ULONG)-(LONG_TIMER_DELTA);          // delta time to next click.
        START_TIMER(DeviceContext, 
                    LONG_TIMER,
                    &DeviceContext->LongSystemTimer,
                    DueTime,
                    &DeviceContext->LongTimerSystemDpc);
    } else {
        LpxDereferenceDeviceContext ("Don't restart long timer", DeviceContext, DCREF_SCAN_TIMER);
    }

    LEAVE_TIMER(DeviceContext, LONG_TIMER);
    
    LEAVE_LPX;
    return;

} /* ScanLongTimersDpc */


VOID
LpxInitializeTimerSystem(
    IN PDEVICE_CONTEXT DeviceContext
    )

/*++

Routine Description:

    This routine initializes the lightweight timer system for the transport
    provider.

Arguments:

    DeviceContext - Pointer to our device context.

Return Value:

    none.

--*/

{
    LARGE_INTEGER DueTime;

    IF_LPXDBG (LPX_DEBUG_TIMER) {
        LpxPrint0 ("LpxInitializeTimerSystem:  Entered.\n");
    }

    ASSERT(TIMERS_INITIALIZED(DeviceContext));
    
    //
    // Set these up.
    //

    LpxTickIncrement = KeQueryTimeIncrement();

    if (LpxTickIncrement > (20 * MILLISECONDS)) {
        LpxTwentyMillisecondsTicks = 1;
    } else {
        LpxTwentyMillisecondsTicks = (20 * MILLISECONDS) / LpxTickIncrement;
    }

    //
    // MaximumIntervalTicks represents 60 seconds, unless the value
    // when shifted out by the accuracy required is too big.
    //

    if ((((ULONG)0xffffffff) >> (DLC_TIMER_ACCURACY+2)) > ((60 * SECONDS) / LpxTickIncrement)) {
        LpxMaximumIntervalTicks = (60 * SECONDS) / LpxTickIncrement;
    } else {
        LpxMaximumIntervalTicks = ((ULONG)0xffffffff) >> (DLC_TIMER_ACCURACY + 2);
    }

    //
    // The AbsoluteTime cycles between 0x10000000 and 0xf0000000.
    //

    DeviceContext->ShortAbsoluteTime = 0x10000000;   // initialize our timer click up-counter.
    DeviceContext->LongAbsoluteTime = 0x10000000;   // initialize our timer click up-counter.

    DeviceContext->MulticastPacketCount = 0;
    DeviceContext->LongTimeoutsWithoutMulticast = 0;

    KeInitializeDpc(
        &DeviceContext->LongTimerSystemDpc,
        ScanLongTimersDpc,
        DeviceContext);

    KeInitializeTimer (&DeviceContext->LongSystemTimer);

    DueTime.HighPart = -1;
    DueTime.LowPart = (ULONG)-(LONG_TIMER_DELTA);

    ENABLE_TIMERS(DeviceContext);

    //
    // One reference for the long timer.
    //

    LpxReferenceDeviceContext ("Long timer active", DeviceContext, DCREF_SCAN_TIMER);

    START_TIMER(DeviceContext, 
                LONG_TIMER,
                &DeviceContext->LongSystemTimer,
                DueTime,
                &DeviceContext->LongTimerSystemDpc);

} /* LpxInitializeTimerSystem */


VOID
LpxStopTimerSystem(
    IN PDEVICE_CONTEXT DeviceContext
    )

/*++

Routine Description:

    This routine stops the lightweight timer system for the transport
    provider.

Arguments:

    DeviceContext - Pointer to our device context.

Return Value:

    none.

--*/

{

    //
    // If timers are currently executing timer code, then this
    // function blocks until they are done executing. Also
    // no new timers will be allowed to be queued after this.
    //
    
    {
        if (KeCancelTimer(&DeviceContext->LongSystemTimer)) {
            LEAVE_TIMER(DeviceContext, LONG_TIMER);
            LpxDereferenceDeviceContext ("Long timer cancelled", DeviceContext, DCREF_SCAN_TIMER);
        }
    }

    DISABLE_TIMERS(DeviceContext);
}
