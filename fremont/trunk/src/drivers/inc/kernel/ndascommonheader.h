#ifndef __NDAS_COMMON_HEADER_H__
#define __NDAS_COMMON_HEADER_H__

#if 1

#if WINVER >= 0x0501

#define NdasDbgBreakPoint()	(KD_DEBUGGER_ENABLED ? DbgBreakPoint() : TRUE)

#else

#define NdasDbgBreakPoint()	((*KdDebuggerEnabled) ? DbgBreakPoint() : TRUE) 

#endif

#else

#define NdasDbgBreakPoint() TRUE

#endif

//  These macros are used to test, set and clear flags respectively

#ifndef FlagOn
#define FlagOn(_F, _SF)        ((_F) & (_SF))
#endif

#ifndef BooleanFlagOn
#define BooleanFlagOn(F, SF)   ((BOOLEAN)(((F) & (SF)) != 0))
#endif

#ifndef SetFlag
#define SetFlag(_F, _SF)       ((_F) |= (_SF))
#endif

#ifndef ClearFlag
#define ClearFlag(_F, _SF)     ((_F) &= ~(_SF))
#endif

#define NANO100_PER_SEC		((LONGLONG)(10 * 1000 * 1000))
#define NANO100_PER_MSEC	((LONGLONG)(10 * 1000))

__inline 
LARGE_INTEGER 
NdasCurrentTime (
	VOID
	)
{
	LARGE_INTEGER	time;
	
	KeQueryTickCount( &time );
	time.QuadPart *= KeQueryTimeIncrement();

	return time;
}

#define HTONS2(Data)	(((((UINT16)Data)&(UINT16)0x00FF) << 8) | ((((UINT16)Data)&(UINT16)0xFF00) >> 8))
#define NTOHS2(Data)	(((((UINT16)Data)&(UINT16)0x00FF) << 8) | ((((UINT16)Data)&(UINT16)0xFF00) >> 8))

#define HTONL2(Data)	( ((((UINT32)Data)&(UINT32)0x000000FF) << 24) | ((((UINT32)Data)&(UINT32)0x0000FF00) << 8) \
						| ((((UINT32)Data)&(UINT32)0x00FF0000)  >> 8) | ((((UINT32)Data)&(UINT32)0xFF000000) >> 24))
#define NTOHL2(Data)	( ((((UINT32)Data)&(UINT32)0x000000FF) << 24) | ((((UINT32)Data)&(UINT32)0x0000FF00) << 8) \
						| ((((UINT32)Data)&(UINT32)0x00FF0000)  >> 8) | ((((UINT32)Data)&(UINT32)0xFF000000) >> 24))

#define HTONLL2(Data)	( ((((UINT64)Data)&(UINT64)0x00000000000000FFLL) << 56) | ((((UINT64)Data)&(UINT64)0x000000000000FF00LL) << 40) \
						| ((((UINT64)Data)&(UINT64)0x0000000000FF0000LL) << 24) | ((((UINT64)Data)&(UINT64)0x00000000FF000000LL) << 8)  \
						| ((((UINT64)Data)&(UINT64)0x000000FF00000000LL) >> 8)  | ((((UINT64)Data)&(UINT64)0x0000FF0000000000LL) >> 24) \
						| ((((UINT64)Data)&(UINT64)0x00FF000000000000LL) >> 40) | ((((UINT64)Data)&(UINT64)0xFF00000000000000LL) >> 56))

#define NTOHLL2(Data)	( ((((UINT64)Data)&(UINT64)0x00000000000000FFLL) << 56) | ((((UINT64)Data)&(UINT64)0x000000000000FF00LL) << 40) \
						| ((((UINT64)Data)&(UINT64)0x0000000000FF0000LL) << 24) | ((((UINT64)Data)&(UINT64)0x00000000FF000000LL) << 8)  \
						| ((((UINT64)Data)&(UINT64)0x000000FF00000000LL) >> 8)  | ((((UINT64)Data)&(UINT64)0x0000FF0000000000LL) >> 24) \
						| ((((UINT64)Data)&(UINT64)0x00FF000000000000LL) >> 40) | ((((UINT64)Data)&(UINT64)0xFF00000000000000LL) >> 56))


//#define HTONS(Data)		( (sizeof(Data) != 2) ? NDAS_ASSERT(FALSE) : 0, HTONS2(Data) )
//#define NTOHS(Data)		( (sizeof(Data) != 2) ? NDAS_ASSERT(FALSE) : 0, NTOHS2(Data) )

#define HTONS(Data)		( HTONS2(Data) )
#define NTOHS(Data)		( NTOHS2(Data) )


#define HTONL(Data)		( (sizeof(Data) != 4) ? NDAS_ASSERT(FALSE) : 0, HTONL2(Data) )
#define NTOHL(Data)		( (sizeof(Data) != 4) ? NDAS_ASSERT(FALSE) : 0, NTOHL2(Data) )

#define HTONLL(Data)	( (sizeof(Data) != 8) ? NDAS_ASSERT(FALSE) : 0, HTONLL2(Data) )
#define NTOHLL(Data)	( (sizeof(Data) != 8) ? NDAS_ASSERT(FALSE) : 0, NTOHLL2(Data) )


#endif
