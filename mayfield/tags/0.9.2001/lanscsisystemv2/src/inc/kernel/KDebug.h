#ifndef KERNEL_DEBUG_MODULE_H
#define KERNEL_DEBUG_MODULE_H

#define DBG_DEFAULT_MASK				0xffffcccc
//#define DBG_DEFAULT_MASK				0xffffffff

#if DBG

#define	KDPrint(DEBUGLEVEL, FORMAT) {			\
	if((DEBUGLEVEL) <= KDebugLevel)				\
		_KDebugPrint FORMAT ;					\
}

#define	KDPrintM(DEBUGMASK, FORMAT) {			\
	if((DEBUGMASK)& KDebugMask)					\
		_KDebugPrint FORMAT ;					\
}

#else

#define	KDPrint(DEBUGLEVEL, FORMAT)
#define	KDPrintM(DEBUGMASK, FORMAT)

#endif

#pragma warning(error:4100)   // Unreferenced formal parameter
#pragma warning(error:4101)   // Unreferenced local variable


extern ULONG	KDebugLevel;
extern ULONG	KDebugMask;

VOID
_KDebugPrint(
   IN PCCHAR	DebugMessage,
   ...
   ) ;

PCHAR
CdbOperationString(
		UCHAR	Code
	);

PCHAR
SrbFunctionCodeString(
		ULONG	Code
	);


#endif