#ifndef KERNEL_DEBUG_MODULE_H
#define KERNEL_DEBUG_MODULE_H

#ifdef __ENABLE_LOADER__
#include <stdio.h>
#include <stdarg.h>
#include <scsi.h>
#endif

#define DBG_DEFAULT_MASK				0xcccccccc


#if DBG

#define __MODULE__ __FILE__
#define KDPrintLocation() _LSKDebugPrint("[%s(%04d)]%s : ", __MODULE__, __LINE__, __FUNCTION__);

#define	KDPrint(DEBUGLEVEL, FORMAT) do { if((DEBUGLEVEL) <= KDebugLevel) { _LSKDebugPrintWithLocation(_LSKDebugPrintVa FORMAT , __MODULE__, __LINE__, __FUNCTION__); } } while(0);

#define	KDPrintM(DEBUGMASK, FORMAT)	do { if((DEBUGMASK)& KDebugMask) { _LSKDebugPrintWithLocation(_LSKDebugPrintVa FORMAT , __MODULE__, __LINE__, __FUNCTION__); } } while(0);

#define	KDPrintCont(DEBUGLEVEL, FORMAT) do { if((DEBUGLEVEL) <= KDebugLevel) { _LSKDebugPrint FORMAT; } } while(0);

#define	KDPrintMCont(DEBUGMASK, FORMAT)	do { if((DEBUGMASK)& KDebugMask) { _LSKDebugPrint  FORMAT; } } while(0);

#else

#define	KDPrint(DEBUGLEVEL, FORMAT)
#define	KDPrintM(DEBUGMASK, FORMAT)
#define	KDPrintCont(DEBUGLEVEL, FORMAT)
#define	KDPrintMCont(DEBUGMASK, FORMAT)

#endif

#pragma warning(error:4100)   // Unreferenced formal parameter
#pragma warning(error:4101)   // Unreferenced local variable

extern ULONG	KDebugLevel;
extern ULONG	KDebugMask;

VOID
_LSKDebugPrint(
   IN PCCHAR	DebugMessage,
   ...
	);

PCHAR
_LSKDebugPrintVa(
	IN PCCHAR	DebugMessage,
	 ...
	);

VOID
_LSKDebugPrintWithLocation(
   IN PCCHAR	DebugMessage,
   PCCHAR		ModuleName,
   UINT32		LineNumber,
   PCCHAR		FunctionName
   ) ;

PCHAR
CdbOperationString(
		UCHAR	Code
	);

PCHAR
SrbFunctionCodeString(
		ULONG	Code
	);

//////////////////////////////////////////////////////////////////////////
//
//
//

#if DBG && defined(__SPINLOCK_DEBUG__)

#define ACQUIRE_SPIN_LOCK(PSPINLOCK, POLDIRQL) {												\
		ULONG	TryCount = 1 ;																	\
																								\
		*(POLDIRQL) = KeRaiseIrqlToDpcLevel() ;													\
																								\
		while( InterlockedExchange(PSPINLOCK, 1) ) {											\
			TryCount ++ ;																		\
		}																						\
		if(TryCount > 10) {																	\
			DbgPrint("%p acquired Spinlock %p: Try:%lu \t name:%s \t file:%s \t line:%lu\n",	\
				KeGetCurrentThread(),															\
				PSPINLOCK,																		\
				TryCount,																		\
				#PSPINLOCK, __FILE__, __LINE__ ) ; \
			TryCount = 0; \
		}									\
																								\
}

#define RELEASE_SPIN_LOCK(PSPINLOCK, OLDIRQL) {													\
		InterlockedExchange(PSPINLOCK, 0) ;														\
		KeLowerIrql(OLDIRQL) ;																	\
																								\
/*		DbgPrint("%p released Spinlock %p: name:%s \t file:%s \t line:%lu\n",					\
			KeGetCurrentThread(),																\
			PSPINLOCK,																			\
			#PSPINLOCK, __FILE__, __LINE__ ) ;											*/		\
}

#define ACQUIRE_DPC_SPIN_LOCK(PSPINLOCK) {														\
		ULONG	TryCount = 1 ;																	\
																								\
		while( InterlockedExchange(PSPINLOCK, 1) ) {											\
			TryCount ++ ;																		\
			if(TryCount == 10000000) {																\
				DbgPrint("%p acquired Spinlock %p: Try:%lu \t name:%s \t file:%s \t line:%lu\n",	\
					KeGetCurrentThread(),															\
					PSPINLOCK,																		\
					TryCount,																		\
					#PSPINLOCK, __FILE__, __LINE__ ) ; \
				TryCount = 0; \
			}									\
		}																						\
																								\
}

#define RELEASE_DPC_SPIN_LOCK(PSPINLOCK) {														\
		InterlockedExchange(PSPINLOCK, 0) ;														\
																								\
/*		DbgPrint("%p released Spinlock %p: name:%s \t file:%s \t line:%lu\n",					\
			KeGetCurrentThread(),																\
			PSPINLOCK,																			\
			#PSPINLOCK, __FILE__, __LINE__ ) ;											*/		\
}

#else

#define ACQUIRE_SPIN_LOCK(lock,irql) KeAcquireSpinLock(lock,irql)
#define RELEASE_SPIN_LOCK(lock,irql) KeReleaseSpinLock(lock,irql)
#define ACQUIRE_DPC_SPIN_LOCK(lock) KeAcquireSpinLockAtDpcLevel(lock)
#define RELEASE_DPC_SPIN_LOCK(lock) KeReleaseSpinLockFromDpcLevel(lock)

#endif

#endif