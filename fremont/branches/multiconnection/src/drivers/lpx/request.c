/*++

Copyright (c) 1989, 1990, 1991  Microsoft Corporation

Module Name:

    request.c

Abstract:

    This module contains code which implements the TP_REQUEST object.
    Routines are provided to create, destroy, reference, and dereference,
    transport request objects.

Author:

    David Beaver (dbeaver) 1 July 1991

Environment:

    Kernel mode

Revision History:


--*/

#include "precomp.h"
#pragma hdrstop


#if 1 // DBG
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

        PIRP Irp = IRP_SEND_IRP(IrpSp);

        IRP_SEND_REFCOUNT(IrpSp) = 0;
        IRP_SEND_IRP (IrpSp) = NULL;

#if __LPX__
		{
			KIRQL  cancelIrql;

			if (Irp->Tail.Overlay.DriverContext[0]) {
			
				LARGE_INTEGER systemTime;

				LPX_ASSERT( ((PCONNECTION_PRIVATE)Irp->Tail.Overlay.DriverContext[0])->Connection );			

				KeQuerySystemTime( &systemTime );

				if (Irp->IoStatus.Information < 1500) {

					((PCONNECTION_PRIVATE)Irp->Tail.Overlay.DriverContext[0])->Connection->LpxSmp.NumberofSmallSendRequests ++;

					((PCONNECTION_PRIVATE)Irp->Tail.Overlay.DriverContext[0])->Connection->LpxSmp.ResponseTimeOfSmallSendRequests.QuadPart +=
						systemTime.QuadPart - ((PCONNECTION_PRIVATE)Irp->Tail.Overlay.DriverContext[0])->CurrentTime.QuadPart;					

					((PCONNECTION_PRIVATE)Irp->Tail.Overlay.DriverContext[0])->Connection->LpxSmp.BytesOfSmallSendRequests.QuadPart += 
						IRP_SEND_LENGTH(IrpSp);

				} else {
			
					((PCONNECTION_PRIVATE)Irp->Tail.Overlay.DriverContext[0])->Connection->LpxSmp.NumberofLargeSendRequests ++;

					((PCONNECTION_PRIVATE)Irp->Tail.Overlay.DriverContext[0])->Connection->LpxSmp.ResponseTimeOfLargeSendRequests.QuadPart +=
						systemTime.QuadPart - ((PCONNECTION_PRIVATE)Irp->Tail.Overlay.DriverContext[0])->CurrentTime.QuadPart;					

					((PCONNECTION_PRIVATE)Irp->Tail.Overlay.DriverContext[0])->Connection->LpxSmp.BytesOfLargeSendRequests.QuadPart += IRP_SEND_LENGTH(IrpSp);
				}

				LpxDereferenceConnection( "LpxIoCompleteRequest", 
										  ((PCONNECTION_PRIVATE)Irp->Tail.Overlay.DriverContext[0])->Connection, 
										  CREF_LPX_PRIVATE );

				ExFreePool( Irp->Tail.Overlay.DriverContext[0] );
				Irp->Tail.Overlay.DriverContext[0] = 0;
			}

			IoAcquireCancelSpinLock( &cancelIrql );
			IoSetCancelRoutine( Irp, NULL );
			IoReleaseCancelSpinLock( cancelIrql );
		}
#endif

        IoCompleteRequest (Irp, IO_NETWORK_INCREMENT);

    }

} /* LpxDerefSendIrp */
#endif