/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    spnlckdb.c

Abstract:

    This module contains code which allows debugging of spinlock related LPX
    problems. Most of this code is conditional on the manifest constant
    LPX_LOCKS.

Author:

    David Beaver 13-Feb-1991
    (From Chuck Lenzmeier, Jan 1991)

Environment:

    Kernel mode

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

#ifdef LPX_LOCKS

KSPIN_LOCK LpxGlobalLock = NULL;
PKTHREAD LpxGlobalLockOwner = NULL;
ULONG LpxGlobalLockRecursionCount = 0;
ULONG LpxGlobalLockMaxRecursionCount = 0;
KIRQL LpxGlobalLockPreviousIrql = (KIRQL)-1;
BOOLEAN LpxGlobalLockPrint = 1;

#define PRINT_ERR if ( (LpxGlobalLockPrint & 1) != 0 ) DbgPrint
#define PRINT_INFO if ( (LpxGlobalLockPrint & 2) != 0 ) DbgPrint

VOID
LpxAcquireSpinLock(
    IN PKSPIN_LOCK Lock,
    OUT PKIRQL OldIrql,
    IN PSZ LockName,
    IN PSZ FileName,
    IN ULONG LineNumber
    )
{
    KIRQL previousIrql;

    PKTHREAD currentThread = KeGetCurrentThread( );

#ifdef __LPX__
	UNREFERENCED_PARAMETER( OldIrql );
	UNREFERENCED_PARAMETER( LockName );
#endif

    if ( LpxGlobalLockOwner == currentThread ) {

        ASSERT( Lock != NULL ); // else entering LPX with lock held

        ASSERT( LpxGlobalLockRecursionCount != 0 );
        LpxGlobalLockRecursionCount++;
        if ( LpxGlobalLockRecursionCount > LpxGlobalLockMaxRecursionCount ) {
            LpxGlobalLockMaxRecursionCount = LpxGlobalLockRecursionCount;
        }

        PRINT_INFO( "LPX reentered from %s/%ld, new count %ld\n",
                    FileName, LineNumber, LpxGlobalLockRecursionCount );

    } else {

        ASSERT( Lock == NULL ); // else missing an ENTER_LPX call

        KeAcquireSpinLock( &LpxGlobalLock, &previousIrql );

        ASSERT( LpxGlobalLockRecursionCount == 0 );
        LpxGlobalLockOwner = currentThread;
        LpxGlobalLockPreviousIrql = previousIrql;
        LpxGlobalLockRecursionCount = 1;

        PRINT_INFO( "LPX entered from %s/%ld\n", FileName, LineNumber );

    }

    ASSERT( KeGetCurrentIrql() == DISPATCH_LEVEL );

    return;

} // LpxAcquireSpinLock

VOID
LpxReleaseSpinLock(
    IN PKSPIN_LOCK Lock,
    IN KIRQL OldIrql,
    IN PSZ LockName,
    IN PSZ FileName,
    IN ULONG LineNumber
    )
{
    PKTHREAD currentThread = KeGetCurrentThread( );
    KIRQL previousIrql;

#ifdef __LPX__
	UNREFERENCED_PARAMETER( OldIrql );
	UNREFERENCED_PARAMETER( LockName );
#endif

    ASSERT( LpxGlobalLockOwner == currentThread );
    ASSERT( LpxGlobalLockRecursionCount != 0 );
    ASSERT( KeGetCurrentIrql() == DISPATCH_LEVEL );

    if ( --LpxGlobalLockRecursionCount == 0 ) {

        ASSERT( Lock == NULL ); // else not exiting LPX, but releasing lock

        LpxGlobalLockOwner = NULL;
        previousIrql = LpxGlobalLockPreviousIrql;
        LpxGlobalLockPreviousIrql = (KIRQL)-1;

        PRINT_INFO( "LPX exited from %s/%ld\n", FileName, LineNumber );

        KeReleaseSpinLock( &LpxGlobalLock, previousIrql );

    } else {

        ASSERT( Lock != NULL ); // else exiting LPX with lock held

        PRINT_INFO( "LPX semiexited from %s/%ld, new count %ld\n",
                    FileName, LineNumber, LpxGlobalLockRecursionCount );

    }

    return;

} // LpxReleaseSpinLock

VOID
LpxFakeSendCompletionHandler(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET NdisPacket,
    IN NDIS_STATUS NdisStatus
    )
{
    ENTER_LPX;
    LpxSendCompletionHandler (ProtocolBindingContext, NdisPacket, NdisStatus);
    LEAVE_LPX;
}

VOID
LpxFakeTransferDataComplete (
    IN NDIS_HANDLE BindingContext,
    IN PNDIS_PACKET NdisPacket,
    IN NDIS_STATUS NdisStatus,
    IN UINT BytesTransferred
    )
{
    ENTER_LPX;
    LpxTransferDataComplete (BindingContext, NdisPacket, NdisStatus, BytesTransferred);
    LEAVE_LPX;
}

#endif // def LPX_LOCKS
