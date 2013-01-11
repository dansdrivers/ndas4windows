/*++

Copyright (C) 2002-2007 XIMETA, Inc.
All rights reserved.

Private include file for the LPX transport

--*/

#define __LPX__					1

#define __LPX_STATISTICS__		0
#define __LPX_OPTION_ADDRESSS__	0

#include <ntddk.h>

#include <windef.h>
#include <nb30.h>
//#include <ntiologc.h>
//#include <ctype.h>
//#include <assert.h>
//#include <stdio.h>
//#include <stdlib.h>
//#include <memory.h>
//#include <nt.h>
//#include <ntrtl.h>
//#include <nturtl.h>
//#include <string.h>
//#include <windows.h>

#pragma warning(error:4100)   // Unreferenced formal parameter
#pragma warning(error:4101)   // Unreferenced local variable

#ifdef BUILD_FOR_511
#define ExAllocatePoolWithTag(a,b,c) ExAllocatePool(a,b)
#endif

#include <tdikrnl.h>                    // Transport Driver Interface.

#include <ndis.h>                       // Physical Driver Interface.

#if DEVL
#define STATIC
#else
#define STATIC static
#endif

#if __LPX__

#include "ndascommonheader.h"

#if 0

#define NDAS_ASSERT_INSUFFICIENT_RESOURCES	FALSE
#define NDAS_ASSERT_NETWORK_FAIL			FALSE
#define NDAS_ASSERT_NODE_UNRECHABLE			FALSE
#define NDAS_ASSERT_PAKCET_TEST				FALSE

#else

#define NDAS_ASSERT_INSUFFICIENT_RESOURCES	TRUE
#define NDAS_ASSERT_NETWORK_FAIL			TRUE
#define NDAS_ASSERT_NODE_UNRECHABLE			TRUE
#define NDAS_ASSERT_PAKCET_TEST				TRUE

#endif

extern BOOLEAN NdasTestBug;

#if DBG

#define NDAS_ASSERT(exp)	ASSERT(exp)

#else

#define NDAS_ASSERT(exp)				\
	((NdasTestBug && (exp) == FALSE) ?	\
	NdasDbgBreakPoint() :				\
	FALSE)

#endif

#include "lpx.ver"
#include <ndasverp.h>

#include "socketlpx.h"
#undef TDI_ADDRESS_TYPE_NETBIOS
#define TDI_ADDRESS_TYPE_NETBIOS	TDI_ADDRESS_TYPE_LPX

#undef	TDI_ADDRESS_LENGTH_NETBIOS	
#define	TDI_ADDRESS_LENGTH_NETBIOS	TDI_ADDRESS_LENGTH_LPX

#endif

#include "lpxconst.h"                   // private NETBEUI constants.
#include "lpxmac.h"                     // mac-specific definitions

#if __LPX__
#include "lpxproto.h"
#endif

#include "lpxcnfg.h"                    // configuration information.
#include "lpxtypes.h"                   // private NETBEUI types.
#include "lpxprocs.h"                   // private NETBEUI function prototypes.
#ifdef MEMPRINT
#include "memprint.h"                   // drt's memory debug print
#endif

#if __LPX__

#include "SocketlpxProc.h"

#if DBG
extern PKSPIN_LOCK DebugSpinLock;
#endif

#endif

#if defined(NT_UP) && defined(DRIVERS_UP)
#define LPX_UP 1
#endif

//
// Resource and Mutex Macros
//

//
// We wrap each of these macros using
// Enter,Leave critical region macros
// to disable APCs which might occur
// while we are holding the resource
// resulting in deadlocks in the OS.
//

#define ACQUIRE_RESOURCE_EXCLUSIVE(Resource, Wait) \
    KeEnterCriticalRegion(); ExAcquireResourceExclusiveLite(Resource, Wait);
    
#define RELEASE_RESOURCE(Resource) \
    ExReleaseResourceLite(Resource); KeLeaveCriticalRegion();

#define ACQUIRE_FAST_MUTEX_UNSAFE(Mutex) \
    KeEnterCriticalRegion(); ExAcquireFastMutexUnsafe(Mutex);

#define RELEASE_FAST_MUTEX_UNSAFE(Mutex) \
    ExReleaseFastMutexUnsafe(Mutex); KeLeaveCriticalRegion();


#ifndef LPX_LOCKS

#if !defined(LPX_UP)

#define ACQUIRE_SPIN_LOCK(lock,irql) KeAcquireSpinLock(lock,irql)
#define RELEASE_SPIN_LOCK(lock,irql) KeReleaseSpinLock(lock,irql)
#define ACQUIRE_DPC_SPIN_LOCK(lock) KeAcquireSpinLockAtDpcLevel(lock)
#define RELEASE_DPC_SPIN_LOCK(lock) KeReleaseSpinLockFromDpcLevel(lock)

#else // LPX_UP

#define ACQUIRE_SPIN_LOCK(lock,irql) KeAcquireSpinLock(lock,irql)
#define RELEASE_SPIN_LOCK(lock,irql) KeReleaseSpinLock(lock,irql)
#define ACQUIRE_DPC_SPIN_LOCK(lock)
#define RELEASE_DPC_SPIN_LOCK(lock)

#endif

#if DBG

#define ACQUIRE_C_SPIN_LOCK(lock,irql) { \
    PTP_CONNECTION _conn = CONTAINING_RECORD(lock,TP_CONNECTION,SpinLock); \
    KeAcquireSpinLock(lock,irql); \
    _conn->LockAcquired = TRUE; \
    strncpy(_conn->LastAcquireFile, strrchr(__FILE__,'\\')+1, 7); \
    _conn->LastAcquireLine = __LINE__; \
}
#define RELEASE_C_SPIN_LOCK(lock,irql) { \
    PTP_CONNECTION _conn = CONTAINING_RECORD(lock,TP_CONNECTION,SpinLock); \
    _conn->LockAcquired = FALSE; \
    strncpy(_conn->LastReleaseFile, strrchr(__FILE__,'\\')+1, 7); \
    _conn->LastReleaseLine = __LINE__; \
    KeReleaseSpinLock(lock,irql); \
}

#define ACQUIRE_DPC_C_SPIN_LOCK(lock) { \
    PTP_CONNECTION _conn = CONTAINING_RECORD(lock,TP_CONNECTION,SpinLock); \
    KeAcquireSpinLockAtDpcLevel(lock); \
    _conn->LockAcquired = TRUE; \
    strncpy(_conn->LastAcquireFile, strrchr(__FILE__,'\\')+1, 7); \
    _conn->LastAcquireLine = __LINE__; \
}
#define RELEASE_DPC_C_SPIN_LOCK(lock) { \
    PTP_CONNECTION _conn = CONTAINING_RECORD(lock,TP_CONNECTION,SpinLock); \
    _conn->LockAcquired = FALSE; \
    strncpy(_conn->LastReleaseFile, strrchr(__FILE__,'\\')+1, 7); \
    _conn->LastReleaseLine = __LINE__; \
    KeReleaseSpinLockFromDpcLevel(lock); \
}

#if __LPX__

#undef ACQUIRE_SPIN_LOCK 
#undef RELEASE_SPIN_LOCK 
#undef ACQUIRE_DPC_SPIN_LOCK
#undef RELEASE_DPC_SPIN_LOCK

#define ACQUIRE_SPIN_LOCK(lock,irql) { \
	if (lock == DebugSpinLock) { \
	PDEVICE_CONTEXT _conn = CONTAINING_RECORD(lock,DEVICE_CONTEXT,SpinLock); \
	KeAcquireSpinLock(lock,irql); \
	ASSERT( _conn->LockAcquired == 0 );	\
	_conn->LockAcquired++; \
	_conn->LockType = 0;	\
	strncpy(_conn->LastAcquireFile, strrchr(__FILE__,'\\')+1, 7); \
	_conn->LastAcquireLine = __LINE__; \
	} else \
	KeAcquireSpinLock(lock,irql); \
}

#define RELEASE_SPIN_LOCK(lock,irql) { \
	if (lock == DebugSpinLock) { \
	PDEVICE_CONTEXT _conn = CONTAINING_RECORD(lock,DEVICE_CONTEXT,SpinLock); \
	ASSERT( _conn->LockType == 0 );	\
	ASSERT( _conn->LockAcquired == 1 );	\
	_conn->LockAcquired--; \
    strncpy(_conn->LastReleaseFile, strrchr(__FILE__,'\\')+1, 7); \
    _conn->LastReleaseLine = __LINE__; \
	KeReleaseSpinLock(lock,irql); \
	} else \
    KeReleaseSpinLock(lock,irql); \
}

#define ACQUIRE_DPC_SPIN_LOCK(lock) { \
	if (lock == DebugSpinLock) { \
	PDEVICE_CONTEXT _conn = CONTAINING_RECORD(lock,DEVICE_CONTEXT,SpinLock); \
    KeAcquireSpinLockAtDpcLevel(lock); \
	ASSERT( _conn->LockAcquired == 0 );	\
    _conn->LockAcquired++; \
	_conn->LockType = 1;	\
    strncpy(_conn->LastAcquireFile, strrchr(__FILE__,'\\')+1, 7); \
    _conn->LastAcquireLine = __LINE__; \
	} else \
	KeAcquireSpinLockAtDpcLevel(lock); \
}
#define RELEASE_DPC_SPIN_LOCK(lock) { \
	if (lock == DebugSpinLock) { \
	PDEVICE_CONTEXT _conn = CONTAINING_RECORD(lock,DEVICE_CONTEXT,SpinLock); \
	ASSERT( _conn->LockType == 1 );	\
	ASSERT( _conn->LockAcquired == 1 );	\
	_conn->LockAcquired--; \
   strncpy(_conn->LastReleaseFile, strrchr(__FILE__,'\\')+1, 7); \
    _conn->LastReleaseLine = __LINE__; \
    KeReleaseSpinLockFromDpcLevel(lock); \
	} else \
    KeReleaseSpinLockFromDpcLevel(lock); \
}

#endif

#else  // DBG

#define ACQUIRE_C_SPIN_LOCK(lock,irql) ACQUIRE_SPIN_LOCK(lock,irql)
#define RELEASE_C_SPIN_LOCK(lock,irql) RELEASE_SPIN_LOCK(lock,irql)
#define ACQUIRE_DPC_C_SPIN_LOCK(lock) ACQUIRE_DPC_SPIN_LOCK(lock)
#define RELEASE_DPC_C_SPIN_LOCK(lock) RELEASE_DPC_SPIN_LOCK(lock)

#endif // DBG

#define ENTER_LPX
#define LEAVE_LPX

#else

VOID
LpxAcquireSpinLock(
    IN PKSPIN_LOCK Lock,
    OUT PKIRQL OldIrql,
    IN PSZ LockName,
    IN PSZ FileName,
    IN ULONG LineNumber
    );

VOID
LpxReleaseSpinLock(
    IN PKSPIN_LOCK Lock,
    IN KIRQL OldIrql,
    IN PSZ LockName,
    IN PSZ FileName,
    IN ULONG LineNumber
    );

#define ACQUIRE_SPIN_LOCK(lock,irql) \
    LpxAcquireSpinLock( lock, irql, #lock, __FILE__, __LINE__ )
#define RELEASE_SPIN_LOCK(lock,irql) \
    LpxReleaseSpinLock( lock, irql, #lock, __FILE__, __LINE__ )

#define ACQUIRE_DPC_SPIN_LOCK(lock) \
    { \
        KIRQL OldIrql; \
        LpxAcquireSpinLock( lock, &OldIrql, #lock, __FILE__, __LINE__ ); \
    }
#define RELEASE_DPC_SPIN_LOCK(lock) \
    LpxReleaseSpinLock( lock, DISPATCH_LEVEL, #lock, __FILE__, __LINE__ )

#define ENTER_LPX                   \
    LpxAcquireSpinLock( (PKSPIN_LOCK)NULL, (PKIRQL)NULL, "(Global)", __FILE__, __LINE__ )
#define LEAVE_LPX                   \
    LpxReleaseSpinLock( (PKSPIN_LOCK)NULL, (KIRQL)-1, "(Global)", __FILE__, __LINE__ )

#if __LPX__
#define ACQUIRE_C_SPIN_LOCK(lock,irql) ACQUIRE_SPIN_LOCK(lock,irql)
#define RELEASE_C_SPIN_LOCK(lock,irql) RELEASE_SPIN_LOCK(lock,irql)
#define ACQUIRE_DPC_C_SPIN_LOCK(lock) ACQUIRE_DPC_SPIN_LOCK(lock)
#define RELEASE_DPC_C_SPIN_LOCK(lock) RELEASE_DPC_SPIN_LOCK(lock)
#endif

#endif

