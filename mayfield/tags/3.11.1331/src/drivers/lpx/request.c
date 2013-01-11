/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "precomp.h"
#pragma hdrstop
 
#if DBG
VOID
LpxRefSendIrp(
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine increments the reference count on a send IRP.

Arguments:

    IrpSp - Pointer to the IRP's stack location.

Return Value:

    none.

--*/

{

    IF_LPXDBG (LPX_DEBUG_REQUEST) {
        LpxPrint1 ("LpxRefSendIrp:  Entered, ReferenceCount: %x\n",
            IRP_SEND_REFCOUNT(IrpSp));
    }

    ASSERT (IRP_SEND_REFCOUNT(IrpSp) > 0);

    InterlockedIncrement (&IRP_SEND_REFCOUNT(IrpSp));

} /* LpxRefSendIrp */


VOID
LpxDerefSendIrp(
    IN PIO_STACK_LOCATION IrpSp
    )

/*++

Routine Description:

    This routine dereferences a transport send IRP by decrementing the
    reference count contained in the structure.  If, after being
    decremented, the reference count is zero, then this routine calls
    IoCompleteRequest to actually complete the IRP.

Arguments:

    Request - Pointer to a transport send IRP's stack location.

Return Value:

    none.

--*/

{
    LONG result;

    IF_LPXDBG (LPX_DEBUG_REQUEST) {
        LpxPrint1 ("LpxDerefSendIrp:  Entered, ReferenceCount: %x\n",
            IRP_SEND_REFCOUNT(IrpSp));
    }

    result = InterlockedDecrement (&IRP_SEND_REFCOUNT(IrpSp));


    ASSERT (result >= 0);

    //
    // If we have deleted all references to this request, then we can
    // destroy the object.  It is okay to have already released the spin
    // lock at this point because there is no possible way that another
    // stream of execution has access to the request any longer.
    //

    if (result == 0) {
        KIRQL  cancelIrql;

        PIRP Irp = IRP_SEND_IRP(IrpSp);

        IRP_SEND_REFCOUNT(IrpSp) = 0;
        IRP_SEND_IRP (IrpSp) = NULL;


        IoAcquireCancelSpinLock(&cancelIrql);
        IoSetCancelRoutine(Irp, NULL);
        IoReleaseCancelSpinLock(cancelIrql);
        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);
    }

} /* LpxDerefSendIrp */
#endif

