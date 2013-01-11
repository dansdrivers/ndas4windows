#ifdef __cplusplus
extern "C"
{
#endif

//#define BUILD_FOR_DIST

#define	_LPX_

#define MB (1024 * 1024)

#define PASSWORD_LENGTH_V1 8
#define PASSWORD_LENGTH 16

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


#if 0

#define HTONS(Data)		( HTONS2(Data) )
#define NTOHS(Data)		( NTOHS2(Data) )


#define HTONL(Data)		( (sizeof(Data) != 4) ? NDAS_ASSERT(FALSE) : 0, HTONL2(Data) )
#define NTOHL(Data)		( (sizeof(Data) != 4) ? NDAS_ASSERT(FALSE) : 0, NTOHL2(Data) )

#define HTONLL(Data)	( (sizeof(Data) != 8) ? NDAS_ASSERT(FALSE) : 0, HTONLL2(Data) )
#define NTOHLL(Data)	( (sizeof(Data) != 8) ? NDAS_ASSERT(FALSE) : 0, NTOHLL2(Data) )

#else

#define HTONS(Data)		HTONS2(Data)
#define NTOHS(Data)		NTOHS2(Data)


#define HTONL(Data)		HTONL2(Data)
#define NTOHL(Data)		NTOHL2(Data)

#define HTONLL(Data)	HTONLL2(Data)
#define NTOHLL(Data)	NTOHLL2(Data)

#endif

typedef	struct _TARGET_DATA {
	union {
		struct {
			unsigned _int8	NRRWHost;
			unsigned _int8	NRROHost;
			unsigned _int16	Reserved1;
		} V1; 	// NDAS 1.0, 1.1, 2.0
		struct {
			unsigned _int8	NREWHost;
			unsigned _int8	NRSWHost;
			unsigned _int8	NRROHost;
			unsigned _int8	Reserved1;
		} V2;	// NDAS 2.5
	};
	_int64	TargetData;
	
	// IDE Info. This should be array of target.
	BOOL	bPresent;
	BOOL			bLBA;
	BOOL			bLBA48;
	int		bPIO;
	unsigned _int64	SectorCount;

	BOOL	bSmartSupported;
	BOOL	bSmartEnabled;
} TARGET_DATA, *PTARGET_DATA;

#define WRITE_BEBUG		0
#define	NR_MAX_TARGET			2
#define	MAX_DATA_BUFFER_SIZE	(64 * 1024 + 16)
#define BLOCK_SIZE				512
#define	MAX_TRANSFER_SIZE		(64 * 1024)
#define MAX_TRANSFER_BLOCKS		 MAX_TRANSFER_SIZE / BLOCK_SIZE

// HW version that this CLI will use.
#define HW_VER					LANSCSIIDE_VERSION_2_5
//#define HW_VER					LANSCSIIDE_VERSION_2_0
//#define HW_VER					LANSCSIIDE_VERSION_1_1

#define SEC			(LONGLONG)(1000)
#define TIME_OUT	(SEC * 30*2*5)					// 5 min.

// Global Variable.
extern int				NRTarget;
extern TARGET_DATA		PerTarget[NR_MAX_TARGET];

extern unsigned char def_password0[PASSWORD_LENGTH];
extern unsigned char def_supervisor_password[PASSWORD_LENGTH];
extern UINT16  MaxPendingTasks;


//
// From encdec.cpp
//

void
Hash32To128_l(
			unsigned char	*pSource,
			unsigned char	*pResult,
			unsigned char	*pKey
			);

void
Encrypt32_l(
		  unsigned char		*pData,
		  unsigned	_int32	uiDataLength,
		  unsigned char		*pKey,
		  unsigned char		*pPassword
		  );

void
Decrypt32_l(
		  unsigned char		*pData,
		  unsigned	_int32	uiDataLength,
		  unsigned char		*pKey,
		  unsigned char		*pPassword
		  );

void
AES_cipher_dummy(
		   unsigned char	*Text_in,
		   unsigned char	*pText_out,
		   unsigned char	*pKey
		   );

void
AES_cipher(
		   unsigned char	*Text_in,
		   unsigned char	*pText_out,
		   unsigned char	*pKey
		   );

void
__stdcall
Encrypt128(
		  unsigned char		*pData,
		  unsigned	_int32	uiDataLength,
		  unsigned char		*pKey,
		  unsigned char		*pPassword
		  );

void
__stdcall
Decrypt128(
		  unsigned char		*pData,
		  unsigned	_int32	uiDataLength,
		  unsigned char		*pKey,
		  unsigned char		*pPassword
		  );

void
__stdcall
CRC32(
	  unsigned char	*pData,
	  unsigned char	*pOutput,
	  unsigned		uiDataLength
	  );


//
// From lanscsicli.cpp
//

void 
PrintHex(
		 unsigned char* Buf, 
		 int len
);

void
PrintError(
		   int		ErrorCode,
		   PTCHAR	prefix
);

BOOL
lpx_addr(
		 PCHAR			pStr,
		 PLPX_ADDRESS	pAddr
);

BOOL
MakeConnection(
			   IN	PLPX_ADDRESS		pAddress,
			   OUT	SOCKET				*pSocketData
);

int
Login(
	  SOCKET			connsock,
	  UCHAR				cLoginType,
	  _int32			iUserID,
	  unsigned char*	iPassword,
	  BOOL				Silent
);

int
GetDiskInfo(
			SOCKET	connsock,
			UINT	TargetId,
			BOOL	Silent,
			BOOL	SetDefaultTransferMode
);
int
GetDiskInfo2(
			SOCKET	connsock,
			UINT	TargetId
);


#define IDECMD_OPT_UNLOCK_BUFFER_LOCK	0x00001
#define IDECMD_OPT_BAD_HEADER_CRC		0x00002 // for request
#define IDECMD_OPT_BAD_DATA_CRC			0x00004	// for write data.

typedef struct _NDASCLI_TASK {

	_int32 TargetId;
	_int8 LUN;
	UINT32 BufferLength;
	PCHAR Buffer;
	UINT32 Option;
	UINT32 TaskTag;

	UCHAR IdeCommand;
	UCHAR SentIdeCommand;
	_int64	Location;
	_int16	SectorCount;
	_int8	Feature;
	UINT32	Info;

} NDASCLI_TASK, *PNDASCLI_TASK;


int
SendIdeCommandRequestAndData(
		   SOCKET	connsock,
		   PNDASCLI_TASK Task
);

int
ReceiveIdeCommandReplyAndData(
		   SOCKET	connsock,
		   PNDASCLI_TASK Task
);

int
IdeCommand(
		   SOCKET	connsock,
		   _int32	TargetId,
		   _int8	LUN,
		   UCHAR	Command,
		   _int64	Location,
		   _int16	SectorCount,
		   _int8	Feature,
		   UINT32   pDataLen,
		   PCHAR	pData,
		   UINT32   Option,
		   UINT32*	Info
);

int
NopCommand(
			   SOCKET	connsock
);

int
Logout(
	   SOCKET	connsock
);

int
VendorCommand(
			  SOCKET			connsock,
			  UCHAR				cOperation,
			  unsigned _int32	*pParameter0,
			  unsigned _int32	*pParameter1,
			  unsigned _int32	*pParameter2,
			  PCHAR				AhsData,
			  int				AhsLen,
			  PCHAR				OptData				
);
int
TextTargetData(
			   SOCKET	connsock,
			   UCHAR	cGetorSet,
			   UINT		TargetID,
			   UINT64*	TargetData
);
int
Discovery(
		  SOCKET	connsock
);

int
ReadReply(
			SOCKET			connSock,
			PCHAR			pBuffer,
			PLANSCSI_PDU_POINTERS	pPdu
			);

int
SendRequest(
			SOCKET			connSock,
			PLANSCSI_PDU_POINTERS	pPdu
			);

// From Patterns.c
int GetNumberOfPattern(void);
BOOL CheckPattern(int PatternNum, int PatternOffset, PUCHAR Buf, int BufLen);
void FillPattern(int PatternNum, int PatternOffset, PUCHAR Buf, int BufLen);


//
// Returns negative value if test is failed. 
//
typedef int (*CLI_CMD_FUNC)(char* target, char* arg[]);

//
// From misccmds.cpp
//
int ConnectToNdas(SOCKET* connsock, char* target, UINT32 UserId, PUCHAR Password);
int DisconnectFromNdas(SOCKET connsock, UINT32 UserId);

int CmdLoginTest00(char* target, char* arg[]);
int CmdAesLibTest(char* target, char* arg[]);
int CmdRawVendorCommand(char* target, char* arg[]);
int CmdTestVendorCommand0(char* target, char* arg[]);
int CmdBatchTest(char* target, char* arg[]);
int CmdDumpEep(char* target, char* arg[]);
int CmdGetEep(char* target, char* arg[]);
int CmdSetEep(char* target, char* arg[]);
int CmdGetUEep(char* target, char* arg[]);
int CmdSetUEep(char* target, char* arg[]);
int CmdShowAccount(char* target, char* arg[]);
int CmdSetPermission(char* target, char* arg[]);
int CmdPnpRequest(char* target, char* arg[]);
int CmdLockedWrite(char* target, char* arg[]);
int CmdBufferLockDeadlockTest(char* target, char* arg[]);
int CmdNop(char* target, char* arg[]);
int CmdTextTargetData(char* target, char* arg[]);
int CmdTextTargetList(char* target, char* arg[]);
int CmdDiscovery(char* target, char* arg[]);
int CmdDynamicOptionTest(char* target, char* arg[]);
int CmdLoginRw(char* target, char* arg[]);
int CmdLoginR(char* target, char* arg[]);
int CmdSetPassword(char* target, char* arg[]);
int CmdResetAccount(char* target, char* arg[]);
int CmdLoginWait(char* target, char* arg[]);
int CmdGetOption(char* target, char* arg[]);
int CmdSetOption(char* target, char* arg[]);
int CmdBlockVariedIo(char* target, char* arg[]);
int CmdMutexCli(char* target, char* arg[]);
int CmdLpxConnect(char* target, char* arg[]);
int CmdSetUserPassword(char* target, char* arg[]);
int CmdSetPacketDrop(char* target, char* arg[]);
int CmdHostList(char* target, char* arg[]);
int CmdExIo(char* target, char* arg[]);
int CmdDigestTest(char* target, char* arg[]);
int CmdPnpListen(char* target, char* arg[]);
int CmdRead(char* target, char* arg[]);
int CmdVerify(char* target, char* arg[]);
int CmdReadPattern(char* target, char* arg[]);
int CmdWritePattern(char* target, char* arg[]);
int CmdInterleavedIo(char* target, char* arg[]);
int CmdGetConfig(char* target, char* arg[]);
int CmdWrite(char* target, char* arg[]);
int CmdDelayedIo(char* target, char* arg[]);
int CmdSetMac(char* target, char* arg[]);
int CmdMutexTest1(char* target, char* arg[]);
int CmdStandbyTest(char* target, char* arg[]);
int CmdStandby(char* target, char* arg[]);
int CmdStandbyIo(char* target, char* arg[]);
int CmdCheckPowerMode(char* target, char* arg[]);
int CmdTransferModeIo(char* target, char* arg[]);
int CmdSetMode(char* target, char* arg[]);
int CmdWriteCache(char* target, char* arg[]);
int CmdFlushTest(char* target, char* arg[]);
int CmdSmart(char* target, char* arg[]);
int CmdSetRetransmit(char* target, char* arg[]);
int CmdSetDefaultConfig(char* target, char* arg[]);
int CmdSetDefaultConfigAuto(char* target, char* arg[]);
int CmdIdentify(char* target, char* arg[]);
int CmdViewMeta(char* target, char* arg[]);
int CmdGenBc(char* target, char* arg[]);
int CmdGetNativeMaxAddr(char* target, char* arg[]);
int CmdCompare(char* target, char* arg[]);
int CmdWriteFile(char* target, char* arg[]);
int CmdCheckFile(char* target, char* arg[]);
int CmdReadFile(char* target, char* arg[]);
//int (char* target, char* arg[]);
//int (char* target, char* arg[]);

#ifdef __cplusplus
}
#endif
