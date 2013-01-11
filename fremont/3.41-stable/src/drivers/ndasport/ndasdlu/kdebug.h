#ifndef KERNEL_DEBUG_MODULE_H
#define KERNEL_DEBUG_MODULE_H

#if DBG
#undef INLINE
#define INLINE
#else
#undef INLINE
#define INLINE __inline
#endif

//
//	default debug mask
//
#define DBG_CCB_NOISE					0x00000001
#define DBG_CCB_TRACE					0x00000002
#define DBG_CCB_INFO					0x00000004
#define DBG_CCB_ERROR					0x00000008

#define DBG_LURN_NOISE					0x00000010
#define DBG_LURN_TRACE					0x00000020
#define DBG_LURN_INFO					0x00000040
#define DBG_LURN_ERROR					0x00000080

#define DBG_PROTO_NOISE					0x00000100
#define DBG_PROTO_TRACE					0x00000200
#define DBG_PROTO_INFO					0x00000400
#define DBG_PROTO_ERROR					0x00000800

#define DBG_TRANS_NOISE					0x00001000
#define DBG_TRANS_TRACE					0x00002000
#define DBG_TRANS_INFO					0x00004000
#define DBG_TRANS_ERROR					0x00008000

#define DBG_OTHER_NOISE					0x00010000
#define DBG_OTHER_TRACE					0x00020000
#define DBG_OTHER_INFO					0x00040000
#define DBG_OTHER_ERROR					0x00080000

#define DBG_DEFAULT_MASK				0x88888888

#if DBG

#pragma warning(disable: 4296)
#define __MODULE__ __FILE__
#define KDPrintLocation() _LSKDebugPrint("[%s(%04d)]%s : ", __MODULE__, __LINE__, __FUNCTION__);

#define	KDPrint(DEBUGLEVEL, FORMAT) do { if((DEBUGLEVEL) <= KDebugLevel) { _LSKDebugPrintWithLocation(_LSKDebugPrintVa FORMAT , __MODULE__, __LINE__, __FUNCTION__); } } while(0);

#define	KDPrintM(DEBUGMASK, FORMAT)	do { if((DEBUGMASK)& KDebugMask) { _LSKDebugPrintWithLocation(_LSKDebugPrintVa FORMAT , __MODULE__, __LINE__, __FUNCTION__); } } while(0);

#define	KDPrintCont(DEBUGLEVEL, FORMAT) do { if((DEBUGLEVEL) <= KDebugLevel) { _LSKDebugPrint FORMAT; } } while(0);

#define	KDPrintMCont(DEBUGMASK, FORMAT)	do { if((DEBUGMASK)& KDebugMask) { _LSKDebugPrint  FORMAT; } } while(0);
#pragma warning(default: 4296)

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
CcbOperationCodeString (
	UINT32	OperationCode
	);

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

#if DBG && __SPINLOCK_DEBUG__

#define ACQUIRE_SPIN_LOCK(PSPINLOCK, POLDIRQL) {												\
		ULONG	TryCount = 1 ;																	\
																								\
		*(POLDIRQL) = KeRaiseIrqlToDpcLevel() ;													\
																								\
		while( InterlockedExchange(PSPINLOCK, 1) ) {											\
			TryCount ++ ;																		\
		}																						\
		if(TryCount > 10) {																		\
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