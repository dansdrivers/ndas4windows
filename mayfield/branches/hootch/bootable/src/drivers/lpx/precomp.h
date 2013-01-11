/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include <ntddk.h>

#include <windef.h>
#include <nb30.h>
 #include <stdio.h>
 
#include <ndis.h>                       // Physical Driver Interface.

#include <tdikrnl.h>                        // Transport Driver Interface.

#include <ndis.h>                       // Physical Driver Interface.

#if DEVL
#define STATIC
#else
#define STATIC static
#endif

// When you include module.ver file, you should include <ndasverp.h> also
// in case module.ver does not include any definitions
#include "lpx.ver"
#include <ndasverp.h>

#include "socketlpx.h"

#include "lpxconst.h"                   // private NETBEUI constants.
#include "lpxmac.h"                     // mac-specific definitions

#include "lpxproto.h"
#include "lpx.h"

#include "lpxcnfg.h"                    // configuration information.
#include "lpxtypes.h"                   // private LPX types.
#include "lpxprocs.h"                   // private LPX function prototypes.


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

#define ACQUIRE_SPIN_LOCK(lock,irql) KeAcquireSpinLock(lock,irql)
#define RELEASE_SPIN_LOCK(lock,irql) KeReleaseSpinLock(lock,irql)
#define ACQUIRE_DPC_SPIN_LOCK(lock) KeAcquireSpinLockAtDpcLevel(lock)
#define RELEASE_DPC_SPIN_LOCK(lock) KeReleaseSpinLockFromDpcLevel(lock)

#if DBG
extern LONG	DebugLevel;
#define DebugPrint(_l_, _x_)			\
		do{								\
			if(_l_ < DebugLevel)		\
				DbgPrint _x_;			\
		}	while(0)					\
		
#else	
#define DebugPrint(_l_, _x_)			\
		do{								\
		} while(0)
#endif

