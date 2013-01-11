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
#define LSS_WRITECHECK_BUFFER_POOLTAG		'weSL'

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
	CHAR			NREWHost; // Former NRRWHost
	CHAR			NRSWHost; 
	CHAR			NRROHost; // Former NRROHost	
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
	BYTE			Password_v2[16];
	UINT32			MaxBlocksPerRequest;
	UINT32			BlockInBytes;

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
	union {
		UINT64				DestAddr;
		BYTE				Param8[6];
		UINT32				Param32[3];
//		UINT64				Param64;
	};
	UINT32				DataBufferLength;		// sector unit. TODO: make it byte unit and create block unit length
	PVOID				DataBuffer;

	//
	//	Packet command support.
	//
	ULONG				DVD_TYPE;
	PBYTE				PKCMD;
	PVOID				PKDataBuffer;
	UINT32				PKDataBufferLength;


} LANSCSI_PDUDESC, *PLANSCSI_PDUDESC;


#define INITIALIZE_PDUDESC(PDUDESC_POINTER, TARGETID, LUNID, OPCODE, COMMAND, FLAGS, DESTADDR, DATABUFFER_LENGTH, DATABUFFER_POINTER) { \
		ASSERT(PDUDESC_POINTER);																			\
		RtlZeroMemory((PDUDESC_POINTER), sizeof(LANSCSI_PDUDESC));											\
		(PDUDESC_POINTER)->TargetId = (TARGETID);															\
		(PDUDESC_POINTER)->LUN = (LUNID);																	\
		(PDUDESC_POINTER)->Opcode = (OPCODE);																\
		(PDUDESC_POINTER)->Command = (COMMAND);																\
		(PDUDESC_POINTER)->Flags = (FLAGS);																	\
		(PDUDESC_POINTER)->DestAddr = (DESTADDR);															\
		(PDUDESC_POINTER)->DataBufferLength = (DATABUFFER_LENGTH);											\
		(PDUDESC_POINTER)->DataBuffer = (DATABUFFER_POINTER);												\
}

#define INITIALIZE_ATAPIPDUDESC(PDUDESC_POINTER, TARGETID, LUNID, OPCODE, COMMAND, FLAGS, PKCMD_POINTER, PKCMDDATAABUFFER, PKCMDDATAABUFFER_LEN, Dvd_type) {\
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
}

#define LSS_INITIALIZE_PDUDESC(LSS_POINTER, PDUDESC_POINTER, OPCODE, COMMAND, FLAGS, DESTADDR, DATABUFFER_LENGTH, DATABUFFER_POINTER) { \
		ASSERT(PDUDESC_POINTER);																			\
		RtlZeroMemory((PDUDESC_POINTER), sizeof(LANSCSI_PDUDESC));											\
		(PDUDESC_POINTER)->TargetId = (LSS_POINTER)->LanscsiTargetID;										\
		(PDUDESC_POINTER)->LUN = (LSS_POINTER)->LanscsiLU;													\
		(PDUDESC_POINTER)->Opcode = (OPCODE);																\
		(PDUDESC_POINTER)->Command = (COMMAND);																\
		(PDUDESC_POINTER)->Flags = (FLAGS);																	\
		(PDUDESC_POINTER)->DestAddr = (DESTADDR);															\
		(PDUDESC_POINTER)->DataBufferLength = (DATABUFFER_LENGTH);											\
		(PDUDESC_POINTER)->DataBuffer = (DATABUFFER_POINTER);												\
}

//
//	Lanscsi protocol functions
//
typedef NTSTATUS (*LSPROTO_SENDREQUEST)(PLANSCSI_SESSION LSS, PLANSCSI_PDU_POINTERS Pdu, PLSTRANS_OVERLAPPED OverlappedData);
typedef NTSTATUS (*LSPROTO_READREPLY)(PLANSCSI_SESSION LSS, PCHAR Buffer, PLANSCSI_PDU_POINTERS Pdu, PLSTRANS_OVERLAPPED OverlappedData);
typedef NTSTATUS (*LSPROTO_LOGIN)(PLANSCSI_SESSION LSS, PLSSLOGIN_INFO	LoginInfo);
typedef NTSTATUS (*LSPROTO_LOGOUT)(PLANSCSI_SESSION LSS);
typedef NTSTATUS (*LSPROTO_TEXTTARGETLIST)(PLANSCSI_SESSION LSS, PTARGETINFO_LIST	TargetList);
typedef NTSTATUS (*LSPROTO_TEXTTARGETDATA)(PLANSCSI_SESSION LSS, BYTE GetorSet, UINT32	TargetID, PTARGET_DATA	TargetData);
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
#define LSPROTO_PASSWORD_V2_LENGTH		16
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
	TA_LSTRANS_ADDRESS			SourceAddress;

    LSTRANS_ADDRESS_FILE		AddressFile;
    LSTRANS_CONNECTION_FILE		ConnectionFile;

	LARGE_INTEGER				TimeOuts[4];

	//
	//	Session information from a user
	//
	BYTE			LoginType;
	UINT32			UserID;
	UINT64			Password;
	BYTE			Password_v2[16];

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
	UINT32			CHAP_C_v2[4];
	
	// Speedup for the encryption
	BYTE			EncryptIR[4];		// Intermediate Result.
	BYTE			DecryptIR[4];		// Intermediate Result.
	ULONG			EncryptBufferLength;
	PBYTE			EncryptBuffer;

	//
	//	Lanscsi node information
	//
	TA_LSTRANS_ADDRESS	LSNodeAddress;
	BYTE			HWType;
	BYTE			HWVersion;
	BYTE			HWProtoType;
	BYTE			HWProtoVersion;
	UINT16			HWRevision;
	UINT32			NumberofSlot;
	BYTE			HeaderEncryptAlgo;
	BYTE			HeaderDigestAlgo;
	BYTE			DataEncryptAlgo;
	BYTE			DataDigestAlgo;
	UINT32			MaxTargets;
	UINT32			MaxLUs;
//	UINT32			MaxBlocks;
	UINT32			MaxBlocksPerRequest;
	UINT32			BlockInBytes;

	//
	// Login status. Store last login error.
	//
	UINT32			LastLoginError; // LSS_LOGIN_SUCCESS, LSS_LOGIN_*
	
	
	LS_TRANS_STAT   TransStat;
	//
	// write block size control information	
	//
	UINT32			NrWritesAfterLastRetransmits;
	UINT32			WriteSplitSize; // in sectors

	// Read block size control information
	UINT32			NrReadAfterLastPacketLoss;
	UINT32			ReadSplitSize; // in sectors

	//
	//	Asynchronous IO data
	//
	LSTRANS_OVERLAPPED	NullOverlappedData;
	LSTRANS_OVERLAPPED	BodyOverlappedData;


	//
	//	PDU buffer for Asynchronous operation.
	//
	_int8			PduBuffer[MAX_REQUEST_SIZE];

	// Buffer for work-around NDAS 2.0 write bug.
	PBYTE			WriteCheckBuffer;

	//
	// This buffer is tempory use to add CRC when writing to support 2.5
	// need to remove to improve speed when DATA CRC is used.
	PBYTE			DigestPatchBuffer;
	
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
		PLANSCSI_SESSION	LSS
	);

VOID
LspSetTimeOut(
		PLANSCSI_SESSION	LSS,
		LARGE_INTEGER		TimeOut[]
	);

NTSTATUS
LspConnect(
		IN OUT PLANSCSI_SESSION LSS,
		IN PTA_LSTRANS_ADDRESS SrcAddr,
		IN PTA_LSTRANS_ADDRESS DestAddr
	);

NTSTATUS
LspConnectEx(
		IN OUT PLANSCSI_SESSION	LSS,
		OUT PTA_LSTRANS_ADDRESS	BoundAddr,
		IN PTA_LSTRANS_ADDRESS	BindAddr,
		IN PTA_LSTRANS_ADDRESS	BindAddr2,
		IN PTA_LSTRANS_ADDRESS	DestAddr,
		IN BOOLEAN				BindAnyAddress
	);

NTSTATUS
LspReconnect(
		IN OUT PLANSCSI_SESSION LSS,
		IN LSTRANS_TYPE LstransType
	);

NTSTATUS
LspRelogin(
		IN OUT PLANSCSI_SESSION LSS,
		IN BOOLEAN		IsEncryptBuffer
	);

NTSTATUS
LspDisconnect(
		PLANSCSI_SESSION LSS
	);

NTSTATUS
LspBuildLoginInfo(
		PLANSCSI_SESSION	LSS,
		PLSSLOGIN_INFO		LoginInfo
	);

VOID
LspCopy(
		PLANSCSI_SESSION	ToLSS,
		PLANSCSI_SESSION	FromLSS
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
	   OUT PBYTE			PduResponse
	);

//////////////////////////////////////////////////////////////////////////
//
//	Stub function for Lanscsi protocol interface
//
NTSTATUS
LspSendRequest(
			PLANSCSI_SESSION			LSS,
			PLANSCSI_PDU_POINTERS		Pdu,
			PLSTRANS_OVERLAPPED			OverlappedData
	);

NTSTATUS
LspReadReply(
		PLANSCSI_SESSION			LSS,
		PCHAR						Buffer,
		PLANSCSI_PDU_POINTERS		Pdu,
		PLSTRANS_OVERLAPPED			OverlappedData
	);

NTSTATUS
LspLogin(
		PLANSCSI_SESSION LSS,
		PLSSLOGIN_INFO	LoginInfo,
		LSPROTO_TYPE	LSProto,
		BOOLEAN			LockCleanup
	);

NTSTATUS
LspLogout(
		PLANSCSI_SESSION LSS
	);

NTSTATUS
LspTextTargetList(
		PLANSCSI_SESSION LSS,
		PTARGETINFO_LIST	TargetList
	);

NTSTATUS
LspTextTartgetData(
		PLANSCSI_SESSION LSS,
		BOOLEAN GetorSet,
		UINT32	TargetID,
		PTARGET_DATA	TargetData
	);

NTSTATUS
LspRequest(
		PLANSCSI_SESSION	LSS,
		PLANSCSI_PDUDESC	PduDesc,
		PBYTE				PduResponse
	);

NTSTATUS
LspPacketRequest(
		PLANSCSI_SESSION	LSS,
		PLANSCSI_PDUDESC	PduDesc,
		PBYTE				PduResponse,
		PUCHAR				PduRegister
		);

NTSTATUS
LspVendorRequest(
		PLANSCSI_SESSION	LSS,
		PLANSCSI_PDUDESC	PduDesc,
		PBYTE				PduResponse
);

NTSTATUS
LspAcquireLock(
	PLANSCSI_SESSION		LSS,
	UCHAR					LockNo,
	PUINT32					LockCount

);

NTSTATUS
LspReleaseLock(
	PLANSCSI_SESSION		LSS,
	UCHAR					LockNo,
	PUINT32					LockCount

);


NTSTATUS
LspCleanupLock(
   PLANSCSI_SESSION	LSS,
   UCHAR			LockNo
);

#endif
