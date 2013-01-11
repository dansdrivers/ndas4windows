#ifndef _NDAS_MINI_DEBUG_H_
#define _NDAS_MINI_DEBUG_H_


#if DBG

typedef enum _NDASSCSI_DEBUG_FLAGS {

    NDASSCSI_DEBUG_LUR_NOISE		    = 0x00000001,
    NDASSCSI_DEBUG_LUR_TRACE            = 0x00000002,
    NDASSCSI_DEBUG_LUR_INFO	            = 0x00000004,
    NDASSCSI_DEBUG_LUR_ERROR			= 0x00000008,

	NDASSCSI_DEBUG_LURN_NOISE		    = 0x00000010,
	NDASSCSI_DEBUG_LURN_TRACE			= 0x00000020,
	NDASSCSI_DEBUG_LURN_INFO			= 0x00000040,
	NDASSCSI_DEBUG_LURN_ERROR			= 0x00000080,

	NDASSCSI_DEBUG_ALL_NOISE		    = 0x10000000,
	NDASSCSI_DEBUG_ALL_TRACE			= 0x20000000,
	NDASSCSI_DEBUG_ALL_INFO				= 0x40000000,
	NDASSCSI_DEBUG_ALL_ERROR			= 0x80000000,

} NDASSCSI_DEBUG_FLAGS;


extern NDASSCSI_DEBUG_FLAGS NdasScsiDebugLevel;

#ifndef FlagOn
#define FlagOn(F,SF) ( \
    (((F) & (SF)))     \
)
#endif

#define DebugTrace( _dbgLevel, _string )			\
	(FlagOn(NdasScsiDebugLevel, (_dbgLevel)) ?      \
	 DbgPrint _string  :							\
	 ((void)0))

#else

#define DebugTrace( _dbgLevel, _string )

#endif


#endif // _NDAS_MINI_DEBUG_H_
