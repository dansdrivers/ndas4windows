#define	_LPX_

#define	NR_MAX_TARGET			2
#define BLOCK_SIZE				512
#define MAX_REQUESTBLOCK				128
#define	MAX_DATA_BUFFER_SIZE	MAX_REQUESTBLOCK * BLOCK_SIZE
#define	MAX_TRANSFER_SIZE		(MAX_REQUESTBLOCK * BLOCK_SIZE)
#define MAX_TRANSFER_BLOCKS		 MAX_TRANSFER_SIZE / BLOCK_SIZE

#define SEC			(LONGLONG)(1000)
#define TIME_OUT	(SEC * 30)					// 5 min.

#define HTONLL(Data)	( (((Data)&0x00000000000000FF) << 56) | (((Data)&0x000000000000FF00) << 40) \
						| (((Data)&0x0000000000FF0000) << 24) | (((Data)&0x00000000FF000000) << 8)  \
						| (((Data)&0x000000FF00000000) >> 8)  | (((Data)&0x0000FF0000000000) >> 24) \
						| (((Data)&0x00FF000000000000) >> 40) | (((Data)&0xFF00000000000000) >> 56))

#define NTOHLL(Data)	( (((Data)&0x00000000000000FF) << 56) | (((Data)&0x000000000000FF00) << 40) \
						| (((Data)&0x0000000000FF0000) << 24) | (((Data)&0x00000000FF000000) << 8)  \
						| (((Data)&0x000000FF00000000) >> 8)  | (((Data)&0x0000FF0000000000) >> 24) \
						| (((Data)&0x00FF000000000000) >> 40) | (((Data)&0xFF00000000000000) >> 56))

typedef	struct _TARGET_DATA {
	BOOL	bPresent;
	_int8	NRRWHost;
	_int8	NRROHost;
	_int64	TargetData;
	
	// IDE Info.
	BOOL			bLBA;
	BOOL			bLBA48;
	unsigned _int64	SectorCount;
} TARGET_DATA, *PTARGET_DATA;

BOOL
MakeConnection(
			   IN	PLPX_ADDRESS		pAddress,
			   OUT	SOCKET				*pSocketData
			   );

int
Discovery(
		  SOCKET	connsock
		  );

int
Login(
	  SOCKET			connsock,
	  UCHAR				cLoginType,
	  _int32			iUserID,
	  unsigned _int64	iKey
	  );

int
Logout(
	   SOCKET	connsock
	   );

int
GetDiskInfo(
			SOCKET	connsock,
			UINT	TargetId
			);

int
TextTargetData(
			   SOCKET	connsock,
			   UCHAR	cGetorSet,
			   UINT		TargetID
			   );

int
IdeCommand(
		   SOCKET	connsock,
		   _int32	TargetId,
		   _int64	LUN,
		   UCHAR	Command,
		   _int64	Location,
		   _int16	SectorCount,
		   _int8	Feature,
		   PCHAR	pData
		   );

void
InitLanscsiGlobalValue(void);
