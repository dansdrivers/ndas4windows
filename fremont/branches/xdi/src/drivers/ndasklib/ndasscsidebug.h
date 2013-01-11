#ifndef _NDAS_SCSI_DEBUG_H_
#define _NDAS_SCSI_DEBUG_H_


#if DBG

typedef enum _NDASSCSI_DBG_FLAGS {

    NDASSCSI_DBG_LUR_NOISE		    = 0x00000001,
    NDASSCSI_DBG_LUR_TRACE          = 0x00000002,
    NDASSCSI_DBG_LUR_INFO	        = 0x00000004,
    NDASSCSI_DBG_LUR_ERROR			= 0x00000008,

	NDASSCSI_DBG_LURN_NDASR_NOISE	= 0x00000010,
	NDASSCSI_DBG_LURN_NDASR_TRACE	= 0x00000020,
	NDASSCSI_DBG_LURN_NDASR_INFO	= 0x00000040,
	NDASSCSI_DBG_LURN_NDASR_ERROR	= 0x00000080,

	NDASSCSI_DBG_LURN_IDE_NOISE		= 0x00000100,
	NDASSCSI_DBG_LURN_IDE_TRACE		= 0x00000200,
	NDASSCSI_DBG_LURN_IDE_INFO		= 0x00000400,
	NDASSCSI_DBG_LURN_IDE_ERROR		= 0x00000800,

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


#endif // _NDAS_SCSI_DEBUG_H_