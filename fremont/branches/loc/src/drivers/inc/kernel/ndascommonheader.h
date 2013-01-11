#ifndef __NDAS_COMMON_HEADER_H__
#define __NDAS_COMMON_HEADER_H__

#if WINVER >= 0x0501

#define NdasDbgBreakPoint()	(KD_DEBUGGER_ENABLED ? DbgBreakPoint() : TRUE)

#else

#define NdasDbgBreakPoint()	((*KdDebuggerEnabled) ? DbgBreakPoint() : TRUE) 

#endif

#undef NdasDbgBreakPoint
#define NdasDbgBreakPoint() TRUE

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

#ifndef _SOCKET_LPX_H_

#define HTONS2(Data)	( ((Data)&(UINT16)0x00FF) << (UINT16)8) | (((Data)&(UINT16)0xFF00) >> (UINT16)8 )
#define NTOHS2(Data)	( ((Data)&(UINT16)0x00FF) << (UINT16)8) | (((Data)&(UINT16)0xFF00) >> (UINT16)8 )

#define HTONL2(Data)	( (((Data)&0x000000FF) << 24) | (((Data)&0x0000FF00) << 8)  |	\
						  (((Data)&0x00FF0000)  >> 8) | (((Data)&0xFF000000) >> 24) )

#define NTOHL2(Data)	( (((Data)&0x000000FF) << 24) | (((Data)&0x0000FF00) << 8)  |	\
						  (((Data)&0x00FF0000)  >> 8) | (((Data)&0xFF000000) >> 24) )

#define HTONLL2(Data)	( (((Data)&0x00000000000000FF) << 56) | (((Data)&0x000000000000FF00) << 40) | \
						  (((Data)&0x0000000000FF0000) << 24) | (((Data)&0x00000000FF000000) << 8)  | \
						  (((Data)&0x000000FF00000000) >> 8)  | (((Data)&0x0000FF0000000000) >> 24) | \
						  (((Data)&0x00FF000000000000) >> 40) | (((Data)&0xFF00000000000000) >> 56) )

#define NTOHLL2(Data)	( (((Data)&0x00000000000000FF) << 56) | (((Data)&0x000000000000FF00) << 40) | \
						  (((Data)&0x0000000000FF0000) << 24) | (((Data)&0x00000000FF000000) << 8)  | \
						  (((Data)&0x000000FF00000000) >> 8)  | (((Data)&0x0000FF0000000000) >> 24) | \
						  (((Data)&0x00FF000000000000) >> 40) | (((Data)&0xFF00000000000000) >> 56) )


//#define HTONS(Data)		( (sizeof(Data) != 2) ? NDAS_BUGON(FALSE) : 0, HTONS2(Data) )
//#define NTOHS(Data)		( (sizeof(Data) != 2) ? NDAS_BUGON(FALSE) : 0, NTOHS2(Data) )

#define HTONS(Data)		( HTONS2(Data) )
#define NTOHS(Data)		( NTOHS2(Data) )

#define HTONL(Data)		( (sizeof(Data) != 4) ? NDAS_BUGON(FALSE) : 0, HTONL2(Data) )
#define NTOHL(Data)		( (sizeof(Data) != 4) ? NDAS_BUGON(FALSE) : 0, NTOHL2(Data) )

#define HTONLL(Data)	( (sizeof(Data) != 8) ? NDAS_BUGON(FALSE) : 0, HTONLL2(Data) )
#define NTOHLL(Data)	( (sizeof(Data) != 8) ? NDAS_BUGON(FALSE) : 0, NTOHLL2(Data) )

#endif

#endif