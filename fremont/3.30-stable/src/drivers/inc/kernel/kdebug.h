#ifndef KERNEL_DEBUG_MODULE_H
#define KERNEL_DEBUG_MODULE_H

#if DBG
#undef INLINE
#define INLINE
#else
#undef INLINE
#define INLINE __inline
#endif

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


#if DBG

typedef enum _NDASSCSI_DBG_FLAGS {

	NDASSCSI_DBG_MINIPORT_NOISE		= 0x00000001,
	NDASSCSI_DBG_MINIPORT_TRACE		= 0x00000002,
	NDASSCSI_DBG_MINIPORT_INFO		= 0x00000004,
	NDASSCSI_DBG_MINIPORT_ERROR		= 0x00000008,

    NDASSCSI_DBG_LUR_NOISE		    = 0x00000010,
    NDASSCSI_DBG_LUR_TRACE          = 0x00000020,
    NDASSCSI_DBG_LUR_INFO	        = 0x00000040,
    NDASSCSI_DBG_LUR_ERROR			= 0x00000080,

	NDASSCSI_DBG_LURN_NDASR_NOISE	= 0x00000100,
	NDASSCSI_DBG_LURN_NDASR_TRACE	= 0x00000200,
	NDASSCSI_DBG_LURN_NDASR_INFO	= 0x00000400,
	NDASSCSI_DBG_LURN_NDASR_ERROR	= 0x00000800,

	NDASSCSI_DBG_LURN_IDE_NOISE		= 0x00001000,
	NDASSCSI_DBG_LURN_IDE_TRACE		= 0x00002000,
	NDASSCSI_DBG_LURN_IDE_INFO		= 0x00004000,
	NDASSCSI_DBG_LURN_IDE_ERROR		= 0x00008000,

	NDASSCSI_DBG_LURN_PROTO_NOISE	= 0x00010000,
	NDASSCSI_DBG_LURN_PROTO_TRACE	= 0x00020000,
	NDASSCSI_DBG_LURN_PROTO_INFO	= 0x00040000,
	NDASSCSI_DBG_LURN_PROTO_ERROR	= 0x00080000,

	NDASSCSI_DBG_ALL_NOISE		    = 0x10000000,
	NDASSCSI_DBG_ALL_TRACE			= 0x20000000,
	NDASSCSI_DBG_ALL_INFO			= 0x40000000,
	NDASSCSI_DBG_ALL_ERROR			= 0x80000000,

} NDASSCSI_DBG_FLAGS;


extern NDASSCSI_DBG_FLAGS NdasScsiDebugLevel;

#ifndef FlagOn
#define FlagOn(F,SF) ( \
    (((F) & (SF)))     \
)
#endif

VOID
NdasscsiPrintWithFunction (
	IN PCCHAR	DebugMessage,
	IN PCCHAR	FunctionName,
	IN UINT32	LineNumber
	);

#define DebugTrace( _dbgLevel, _string )			\
	(FlagOn(NdasScsiDebugLevel, (_dbgLevel)) ?      \
	NdasscsiPrintWithFunction(_LSKDebugPrintVa _string , __FUNCTION__, __LINE__) : \
	 ((void)0))

#else

#define DebugTrace( _dbgLevel, _string )

#endif

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