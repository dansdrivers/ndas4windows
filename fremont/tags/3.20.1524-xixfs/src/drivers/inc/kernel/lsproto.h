#ifndef LANSCSI_PROTOCOL_H
#define LANSCSI_PROTOCOL_H

#include "TdiKrnl.h"
#include "LSTransport.h"
#include "LSProtoSpec.h"

#if DBG
#undef INLINE
#define INLINE
#else
#undef INLINE
#define INLINE __inline
#endif



#define LSS_ENCRYPTBUFFER_POOLTAG			'beSL'
#define LSS_BUFFER_POOLTAG					'fbSL'
#define LSS_DIGEST_PATCH_POOLTAG			'pbSL'
#define LSS_POOLTAG							'slSL'
#define LSS_OVERLAPPED_BUFFER_POOLTAG		'voSL'
#define LSS_OVERLAPPED_EVENT_POOLTAG		'eoSL'
#define LSS_OVERLAPPED_WAITBLOCK_POOLTAG	'woSL'
#define LSS_OVERLAPPED_LSS_POOLTAG			'loSL'

//////////////////////////////////////////////////////////////////////////
//
//	Session structures
//

typedef struct _LANSCSI_SESSION LANSCSI_SESSION, *PLANSCSI_SESSION;

//
//	Lanscsi prtocol/IDE versions.
//
#define	LSPROTO_IDE_V10		0x0000
#define LSPROTO_IDE_V11		0x0001

#define HARDWARETYPE_TO_PROTOCOLTYPE(HARDWARETYPE) (HARDWARETYPE)

typedef	UINT16 LSPROTO_TYPE, *PLSPROTO_TYPE;

//
// information structures
//
typedef struct _TARGETINFO_ENTRY {

	UINT32			TargetID;
	CHAR			NRRWHost;
	CHAR			NRROHost;
	UINT64			TargetData;

} TARGETINFO_ENTRY, *PTARGETINFO_ENTRY;

typedef struct _TARGETINFO_LIST {

	UINT32				TargetInfoEntryCnt;
	TARGETINFO_ENTRY	TargetInfoEntry[1];

} TARGETINFO_LIST, *PTARGETINFO_LIST;

typedef struct _LUINFO_ENTRY {

	BYTE	LUN;
	UINT32	Characteristic;
	UINT32	LUType;

} LUINFO_ENTRY, *PLUINFO_ENTRY;

typedef UINT64 TARGET_DATA, *PTARGET_DATA;

typedef	struct _LSSLOGIN_INFO {

	BYTE			LoginType;
	UINT32			UserID;
	UINT64			Password;
	UINT32			MaxDataTransferLength;

	UCHAR			LanscsiTargetID;
	UCHAR			LanscsiLU;
	UCHAR			HWType;
	UCHAR			HWVersion;
	UINT16			HWRevision;
	BOOLEAN			IsEncryptBuffer;

} LSSLOGIN_INFO, *PLSSLOGIN_INFO;


//////////////////////////////////////////////////////////////////////////
//
//	Lanscsi PDU descriptor structures
//
#define PDUDESC_FLAG_DATA_MDL			0x00000001
#define PDUDESC_FLAG_DATA_IN			0x00000002
#define PDUDESC_FLAG_DATA_OUT			0x00000004
#define PDUDESC_FLAG_LBA				0x00000008
#define PDUDESC_FLAG_LBA48				0x00000010
#define PDUDESC_FLAG_PIO				0x00000020
#define PDUDESC_FLAG_DMA				0x00000040
#define PDUDESC_FLAG_UDMA				0x00000080
#define PDUDESC_FLAG_VOLATILE_BUFFER	0x00010000

typedef struct _LANSCSI_PDUDESC {
	//
	//	management information
	//
	UINT32				Flags;
	LIST_ENTRY			PDUDescListEntry;

	//
	//	parameter for a Lanscsi packet.
	//
	UINT32				TargetId;
	BYTE				LUN;

	BYTE				Opcode;
	BYTE				Command;			// set status register by callee
	BYTE				Feature;			// set error register by callee
	BYTE				Reserved[1];
	union {	// Increased to 12 bytes as of 2.5
		UINT64				DestAddr;
		BYTE				Param8[12];
		UINT32				Param32[12/sizeof(UINT32)];
		UINT64				Param64;
	};
	UINT32				BlockCount;
	UINT32				DataBufferLength;		// byte unit
	PVOID				DataBuffer;

	//
	// Time-out
	//

	PLARGE_INTEGER		TimeOut;				// 100-nano unit

	//
	//	Packet command support.
	//

	ULONG				DVD_TYPE;
	PBYTE				PKCMD;
	PVOID				PKDataBuffer;
	UINT32				PKDataBufferLength;			// byte unit

	//
	// Transfer logs
	//

	ULONG				Retransmits;
	ULONG				PacketLoss;

} LANSCSI_PDUDESC, *PLANSCSI_PDUDESC;


#define INITIALIZE_PDUDESC(PDUDESC_POINTER, TARGETID, LUNID, OPCODE, COMMAND, FLAGS, DESTADDR, BLOCKCNT, DATABUFFER_LENGTH, DATABUFFER_POINTER, TIMEOUT_POINTER) { \
		ASSERT(PDUDESC_POINTER);																			\
		RtlZeroMemory((PDUDESC_POINTER), sizeof(LANSCSI_PDUDESC));											\
		(PDUDESC_POINTER)->TargetId = (TARGETID);															\
		(PDUDESC_POINTER)->LUN = (LUNID);																	\
		(PDUDESC_POINTER)->Opcode = (OPCODE);																\
		(PDUDESC_POINTER)->Command = (COMMAND);																\
		(PDUDESC_POINTER)->Flags = (FLAGS);																	\
		(PDUDESC_POINTER)->DestAddr = (DESTADDR);															\
		(PDUDESC_POINTER)->BlockCount = (BLOCKCNT);															\
		(PDUDESC_POINTER)->DataBufferLength = (DATABUFFER_LENGTH);											\
		(PDUDESC_POINTER)->DataBuffer = (DATABUFFER_POINTER);												\
		(PDUDESC_POINTER)->TimeOut = (TIMEOUT_POINTER);														\
		(PDUDESC_POINTER)->Retransmits = 0;																	\
		(PDUDESC_POINTER)->PacketLoss = 0;																	\
}

#define INITIALIZE_ATAPIPDUDESC(PDUDESC_POINTER, TARGETID, LUNID, OPCODE, COMMAND, FLAGS, PKCMD_POINTER, PKCMDDATAABUFFER, PKCMDDATAABUFFER_LEN, Dvd_type, TIMEOUT_POINTER) {\
		ASSERT(PDUDESC_POINTER);																			\
		ASSERT(PKCMD_POINTER);																				\
		RtlZeroMemory((PDUDESC_POINTER), sizeof(LANSCSI_PDUDESC));											\
		(PDUDESC_POINTER)->TargetId = (TARGETID);															\
		(PDUDESC_POINTER)->LUN = (LUNID);																	\
		(PDUDESC_POINTER)->Opcode = (OPCODE);																\
		(PDUDESC_POINTER)->Command = (COMMAND);																\
		(PDUDESC_POINTER)->Flags = (FLAGS);																	\
		(PDUDESC_POINTER)->PKCMD = (PKCMD_POINTER);															\
		(PDUDESC_POINTER)->PKDataBuffer = (PKCMDDATAABUFFER);												\
		(PDUDESC_POINTER)->PKDataBufferLength = (PKCMDDATAABUFFER_LEN);										\
		(PDUDESC_POINTER)->DVD_TYPE = (Dvd_type);															\
		(PDUDESC_POINTER)->TimeOut = (TIMEOUT_POINTER);														\
		(PDUDESC_POINTER)->Retransmits = 0;																	\
		(PDUDESC_POINTER)->PacketLoss = 0;																	\
}

#define LSS_INITIALIZE_PDUDESC(LSS_POINTER, PDUDESC_POINTER, OPCODE, COMMAND, FLAGS, DESTADDR, BLOCKCNT, DATABUFFER_LENGTH, DATABUFFER_POINTER, TIMEOUT_POINTER) { \
		ASSERT(PDUDESC_POINTER);																			\
		RtlZeroMemory((PDUDESC_POINTER), sizeof(LANSCSI_PDUDESC));											\
		(PDUDESC_POINTER)->TargetId = (LSS_POINTER)->LanscsiTargetID;										\
		(PDUDESC_POINTER)->LUN = (LSS_POINTER)->LanscsiLU;													\
		(PDUDESC_POINTER)->Opcode = (OPCODE);																\
		(PDUDESC_POINTER)->Command = (COMMAND);																\
		(PDUDESC_POINTER)->Flags = (FLAGS);																	\
		(PDUDESC_POINTER)->DestAddr = (DESTADDR);															\
		(PDUDESC_POINTER)->BlockCount = (BLOCKCNT);															\
		(PDUDESC_POINTER)->DataBufferLength = (DATABUFFER_LENGTH);											\
		(PDUDESC_POINTER)->DataBuffer = (DATABUFFER_POINTER);												\
		(PDUDESC_POINTER)->TimeOut = (TIMEOUT_POINTER);														\
		(PDUDESC_POINTER)->Retransmits = 0;																	\
		(PDUDESC_POINTER)->PacketLoss = 0;																	\
}

//
//	Lanscsi protocol functions
//
typedef NTSTATUS (*LSPROTO_SENDREQUEST)(PLANSCSI_SESSION LSS, PLANSCSI_PDU_POINTERS Pdu, PLSTRANS_OVERLAPPED OverlappedData, PLARGE_INTEGER Timeout);
typedef NTSTATUS (*LSPROTO_READREPLY)(PLANSCSI_SESSION LSS, PCHAR Buffer, PLANSCSI_PDU_POINTERS Pdu, PLSTRANS_OVERLAPPED OverlappedData, PLARGE_INTEGER Timeout);
typedef NTSTATUS (*LSPROTO_LOGIN)(PLANSCSI_SESSION LSS, PLSSLOGIN_INFO	LoginInfo, PLARGE_INTEGER Timeout);
typedef NTSTATUS (*LSPROTO_LOGOUT)(PLANSCSI_SESSION LSS, PLARGE_INTEGER Timeout);
typedef NTSTATUS (*LSPROTO_TEXTTARGETLIST)(PLANSCSI_SESSION LSS, PTARGETINFO_LIST TargetList, PLARGE_INTEGER Timeout);
typedef NTSTATUS (*LSPROTO_TEXTTARGETDATA)(PLANSCSI_SESSION LSS, BYTE GetorSet, UINT32	TargetID, PTARGET_DATA	TargetData, PLARGE_INTEGER Timeout);
typedef NTSTATUS (*LSPROTO_REQUEST)(PLANSCSI_SESSION LSS, PLANSCSI_PDUDESC PduDesc, PBYTE PduResponse);
typedef NTSTATUS (*LSPROTO_REQUEST_ATAPI)(PLANSCSI_SESSION LSS, PLANSCSI_PDUDESC PduDesc, PBYTE PduResponse, PBYTE PduRegister);	
typedef NTSTATUS (*LSPROTO_REQUEST_VENDOR)(PLANSCSI_SESSION LSS, PLANSCSI_PDUDESC PduDesc, PBYTE PduResponse);

typedef struct _LSPROTO_FUNC {

	//
	//	16 entries
	//
	LSPROTO_SENDREQUEST		LsprotoSendRequest;
	LSPROTO_READREPLY		LsprotoReadReply;
	LSPROTO_LOGIN			LsprotoLogin;
	LSPROTO_LOGOUT			LsprotoLogout;
	LSPROTO_TEXTTARGETLIST	LsprotoTextTargetList;
	LSPROTO_TEXTTARGETDATA	LsprotoTextTargetData;
	LSPROTO_REQUEST			LsprotoRequest;
	LSPROTO_REQUEST_ATAPI	LsprotoRequestATAPI;
	LSPROTO_REQUEST_VENDOR	LsprotoRequestVendor;
	PVOID					Reserved[6];
	PVOID					LsprotoExtension;

} LSPROTO_FUNC, *PLSPROTO_FUNC;


//
//	Lanscsi protocol interface
//
typedef struct _LANSCSIPROTOCOL_INTERFACE {

	UINT16			Length;
	UINT16			Type;
	LSPROTO_TYPE	LsprotoType;
	UINT32			SupportHw;
	UINT16			LsnodeHwVerMin;
	UINT16			LsnodeHwVerMax;
	UINT32			LsprotoCharacteristic;
	LSPROTO_FUNC	LsprotoFunc;

} LANSCSIPROTOCOL_INTERFACE, *PLANSCSIPROTOCOL_INTERFACE;


//
//	Lanscsi protocol interface list
//
#define NR_LSPROTO_PROTOCOL			2
#define NR_LSPROTO_MAX_PROTOCOL		16

extern PLANSCSIPROTOCOL_INTERFACE	LsprotoList[NR_LSPROTO_MAX_PROTOCOL];


#define LSPROTO_PASSWORD_LENGTH		sizeof(UINT64)
#define LSPROTO_USERID_LENGTH		sizeof(UINT32)
#define CONVERT_TO_ROUSERID(USERID)	((USERID) & 0xffff)

#define LSS_LOGIN_SUCCESS 				1
#define LSS_LOGIN_INTERNAL_ERROR 		2 // Should not happen
#define LSS_LOGIN_SEND_FAIL_1			3 // Failed to send first request. 
#define LSS_LOGIN_RECV_FAIL_1			4 // Failed to receive at first step. May be disk is hanged or just network problem.
#define LSS_LOGIN_BAD_REPLY			5 // SW or chip bug. Ususally does not happen.
#define LSS_LOGIN_ERROR_REPLY_1		6 // HW returned error for first login request. Maybe HW does not support version requested by SW
#define LSS_LOGIN_SEND_FAIL			7 // Failed to send request other than first one. Network error or target hanged.
#define LSS_LOGIN_RECV_FAIL			8 // Failed to recv reply other than first one. Network error or disk hang
#define LSS_LOGIN_ERROR_REPLY_2		9 // HW returned error for second login request. Does not happen?
#define LSS_LOGIN_INVALID_PASSWORD 	10	// 3rd step can return LSS_LOGIN_INVALID_PASSWORD, LSS_LOGIN_NO_PERMISSION, LSS_LOGIN_CONFLICT_PERMISSION
#define LSS_LOGIN_NO_PERMISSION		11
#define LSS_LOGIN_CONFLICT_PERMISSION	12
#define LSS_LOGIN_ERROR_REPLY_4		13	// Usually does not happen.


//
//	Lanscsi Session
//	Must be allocated in None paged pool
//	because PduBuffer will be passed to LPX's DPC routine.
//	
//

typedef struct _LANSCSI_SESSION {

	UINT16				Type;
	UINT16				Length;

	//
	//	Lanscsi Protocol interface
	//
	PLANSCSIPROTOCOL_INTERFACE	LanscsiProtocol;

	//
	//	Connection
	//
	TA_LSTRANS_ADDRESS			BindAddress;

    LSTRANS_ADDRESS_FILE		AddressFile;
    LSTRANS_CONNECTION_FILE		ConnectionFile;

	LARGE_INTEGER				DefaultTimeOut;

	//
	//	Session information from a user
	//
	BYTE			LoginType;
	UINT32			UserID;
	UINT64			Password;

	UINT32			HPID;
	UINT16			RPID;
	UINT16			CPSlot;
	BYTE			SessionPhase;

	UCHAR			LanscsiTargetID;
	UCHAR			LanscsiLU;

	//
	//	Lanscsi sequence information
	//
	UINT32			CommandTag;

	//
	//	encryption
	//
	UINT32			CHAP_C;
	// Speedup for the encryption
	BYTE			EncryptIR[4];		// Intermediate Result.
	BYTE			DecryptIR[4];		// Intermediate Result.
	ULONG			EncryptBufferLength;
	PBYTE			EncryptBuffer;

	//
	//	Lanscsi node information
	//
	TA_LSTRANS_ADDRESS	LSNodeAddress;

	//
	// Hardware type and versions
	//
	BYTE			HWType;
	BYTE			HWVersion;
	UINT16			HWRevision;

	//
	// Hardware's supported protocol type and versions
	//
	BYTE			HWProtoType;
	BYTE			HWProtoVersion;

	//
	// Protocol parameters
	//
	UINT32			NumberofSlot;
	BYTE			HeaderEncryptAlgo;
	BYTE			HeaderDigestAlgo;
	BYTE			DataEncryptAlgo;
	BYTE			DataDigestAlgo;
	UINT32			MaxTargets;
	UINT32			MaxBlocks;
	UINT32			MaxLUs;
	UINT32			MaxDataTransferLength;

	//
	// Login status. Store last login error.
	//
	UINT32			LastLoginError; // LSS_LOGIN_SUCCESS, LSS_LOGIN_*
	

	LS_TRANS_STAT   TransStat;

	//
	//	Asynchronous IO data
	//
	LSTRANS_OVERLAPPED	RequestOverlappedData;
	LSTRANS_OVERLAPPED	BodyOverlappedData;


	//
	//	PDU buffer for Asynchronous operation.
	//
	_int8			PduBuffer[MAX_REQUEST_SIZE];


} LANSCSI_SESSION, *PLANSCSI_SESSION;


//////////////////////////////////////////////////////////////////////////
//
//	Lanscsi protocol APIs
//

NTSTATUS
LspLookupProtocol(
		IN	ULONG			NdasHw,
		IN	ULONG			NdasHwVersion,
		OUT PLSPROTO_TYPE	Protocol
	);

NTSTATUS
LspUpgradeUserIDWithWriteAccess(
		IN PLANSCSI_SESSION	LSS
	);

NTSTATUS
LspConnect(
		IN OUT PLANSCSI_SESSION	LSS,
		IN PTA_LSTRANS_ADDRESS	SrcAddr,
		IN PTA_LSTRANS_ADDRESS	DestAddr,
		IN PLSTRANS_OVERLAPPED	Overlapped,
		IN PLARGE_INTEGER		Timeout
	);

NTSTATUS
LspConnectMultiBindAddr(
		IN OUT PLANSCSI_SESSION	LSS,
		OUT PTA_LSTRANS_ADDRESS	BoundAddr,
		IN PTA_LSTRANS_ADDRESS	BindAddr,
		IN PTA_LSTRANS_ADDRESS	BindAddr2,
		IN PTA_LSTRANS_ADDRESS	DestAddr,
		IN BOOLEAN				BindAnyAddress,
		IN PLARGE_INTEGER		TimeOut
	);

NTSTATUS
LspReconnect(
		IN OUT PLANSCSI_SESSION LSS,
		IN PLARGE_INTEGER		TimeOut
	);

NTSTATUS
LspRelogin(
		IN OUT PLANSCSI_SESSION LSS,
		IN BOOLEAN				IsEncryptBuffer,
		IN PLARGE_INTEGER		TimeOut
	);

NTSTATUS
LspDisconnect(
		IN PLANSCSI_SESSION LSS
	);

NTSTATUS
LspBuildLoginInfo(
		PLANSCSI_SESSION	LSS,
		PLSSLOGIN_INFO		LoginInfo
	);

VOID
LspCopy(
		PLANSCSI_SESSION	ToLSS,
		PLANSCSI_SESSION	FromLSS,
		BOOLEAN				CopyBufferPointers
	);

VOID
LspSetDefaultTimeOut(
		IN PLANSCSI_SESSION	LSS,
		IN PLARGE_INTEGER	TimeOut
	);

VOID
LspGetAddresses(
		PLANSCSI_SESSION	LSS,
		PTA_LSTRANS_ADDRESS	BindingAddress,
		PTA_LSTRANS_ADDRESS	TargetAddress
	);

NTSTATUS
LspNoOperation(
		IN PLANSCSI_SESSION	LSS,
		IN UINT32			TargetId,
		OUT PBYTE			PduResponse,
		IN PLARGE_INTEGER	TimeOut
	);

//////////////////////////////////////////////////////////////////////////
//
//	Stub function for Lanscsi protocol interface
//
NTSTATUS
LspSendRequest(
		IN PLANSCSI_SESSION			LSS,
		IN PLANSCSI_PDU_POINTERS	Pdu,
		IN PLSTRANS_OVERLAPPED		OverlappedData,
		IN PLARGE_INTEGER			TimeOut
	);

NTSTATUS
LspReadReply(
		IN PLANSCSI_SESSION			LSS,
		OUT PCHAR					Buffer,
		IN PLANSCSI_PDU_POINTERS	Pdu,
		IN PLSTRANS_OVERLAPPED		OverlappedData,
		IN PLARGE_INTEGER			TimeOut
	);

NTSTATUS
LspLogin(
		IN PLANSCSI_SESSION LSS,
		IN PLSSLOGIN_INFO	LoginInfo,
		IN LSPROTO_TYPE		LSProto,
		IN PLARGE_INTEGER	TimeOut,
		IN BOOLEAN			LockCleanup
	);

NTSTATUS
LspLogout(
		IN PLANSCSI_SESSION	LSS,
		IN PLARGE_INTEGER	TimeOut
	);

NTSTATUS
LspTextTargetList(
		IN PLANSCSI_SESSION		LSS,
		OUT PTARGETINFO_LIST	TargetList,
		IN PLARGE_INTEGER		TimeOut
	);

NTSTATUS
LspTextTartgetData(
		IN PLANSCSI_SESSION LSS,
		IN BOOLEAN			GetorSet,
		IN UINT32			TargetID,
		IN OUT PTARGET_DATA	TargetData,
		IN PLARGE_INTEGER	TimeOut
	);

NTSTATUS
LspRequest(
		IN PLANSCSI_SESSION		LSS,
		IN OUT PLANSCSI_PDUDESC	PduDesc,
		OUT PBYTE				PduResponse
	);

NTSTATUS
LspPacketRequest(
		IN PLANSCSI_SESSION		LSS,
		IN OUT PLANSCSI_PDUDESC	PduDesc,
		OUT PBYTE				PduResponse,
		OUT PUCHAR				PduRegister
		);

NTSTATUS
LspVendorRequest(
		IN PLANSCSI_SESSION		LSS,
		IN OUT PLANSCSI_PDUDESC	PduDesc,
		OUT PBYTE				PduResponse
);

NTSTATUS
LspAcquireLock(
	IN PLANSCSI_SESSION	LSS,
	IN UCHAR			LockNo,
	OUT PUCHAR			LockData,
	IN PLARGE_INTEGER	TimeOut
);

NTSTATUS
LspReleaseLock(
	IN PLANSCSI_SESSION	LSS,
	IN UCHAR			LockNo,
	IN PUCHAR			LockData,
	IN PLARGE_INTEGER	TimeOut
);

NTSTATUS
LspGetLockOwner(
	IN PLANSCSI_SESSION	LSS,
	IN UCHAR			LockNo,
	OUT PUCHAR			LockOwner,
	IN PLARGE_INTEGER	TimeOut
);

NTSTATUS
LspGetLockData(
	IN PLANSCSI_SESSION	LSS,
	IN UCHAR			LockNo,
	OUT PUCHAR			LockData,
	IN PLARGE_INTEGER	TimeOut
);

NTSTATUS
LspWorkaroundCleanupLock(
	IN PLANSCSI_SESSION	LSS,
	IN UCHAR			LockNo,
	IN PLARGE_INTEGER	TimeOut
);

#endif
