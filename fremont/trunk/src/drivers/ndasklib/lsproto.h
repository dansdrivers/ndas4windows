#ifndef LANSCSI_PROTOCOL_H
#define LANSCSI_PROTOCOL_H

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

struct _LANSCSI_SESSION;

//
//	Lanscsi prtocol/IDE versions.
//
#define LSIDEPROTO_VERSION_1_0			0
#define LSIDEPROTO_VERSION_1_1			1

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
	TARGETINFO_ENTRY	TargetInfoEntry[2]; // two default entry.

} TARGETINFO_LIST, *PTARGETINFO_LIST;

#define TARGETINFO_LIST_LENGTH(ENTRY_CNT)  \
	( FIELD_OFFSET(TARGETINFO_LIST, TargetInfoEntry) + \
	sizeof(TARGETINFO_ENTRY) * (ENTRY_CNT) )

static
INLINE
ULONG
TARGETINFO_LIST_ENTRYCNT(ULONG Length) {
	if(Length < FIELD_OFFSET(TARGETINFO_LIST, TargetInfoEntry)) {
		return 0;
	}
	return (Length - FIELD_OFFSET(TARGETINFO_LIST, TargetInfoEntry)) / sizeof(TARGETINFO_ENTRY);
}

typedef struct _LUINFO_ENTRY {

	BYTE	LUN;
	UINT32	Characteristic;
	UINT32	LUType;

} LUINFO_ENTRY, *PLUINFO_ENTRY;

typedef UINT64 TARGET_DATA, *PTARGET_DATA;


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

typedef NTSTATUS 
(*LSPROTO_SENDREQUEST)( 
	struct _LANSCSI_SESSION		*LSS, 
	PLANSCSI_PDU_POINTERS		Pdu, 
	PLPXTDI_OVERLAPPED_CONTEXT	OverlappedData, 
	ULONG						RequestIdx,
	PLARGE_INTEGER Timeout);

typedef struct _LSTRANS_OVERLAPPED LSTRANS_OVERLAPPED, *PLSTRANS_OVERLAPPED;
struct _LSSLOGIN_INFO;

typedef NTSTATUS (*LSPROTO_READREPLY)(struct _LANSCSI_SESSION *LSS, PCHAR Buffer, PLANSCSI_PDU_POINTERS Pdu, PLSTRANS_OVERLAPPED OverlappedData, PLARGE_INTEGER Timeout);
typedef NTSTATUS (*LSPROTO_LOGIN)(struct _LANSCSI_SESSION *LSS, struct _LSSLOGIN_INFO *LoginInfo, PLARGE_INTEGER Timeout);
typedef NTSTATUS (*LSPROTO_LOGOUT)(struct _LANSCSI_SESSION *LSS, PLARGE_INTEGER Timeout);
typedef NTSTATUS (*LSPROTO_TEXTTARGETLIST)(struct _LANSCSI_SESSION *LSS, PTARGETINFO_LIST TargetList, ULONG TargetListLength, PLARGE_INTEGER Timeout);
typedef NTSTATUS (*LSPROTO_TEXTTARGETDATA)(struct _LANSCSI_SESSION *LSS, BYTE GetorSet, UINT32	TargetID, PTARGET_DATA	TargetData, PLARGE_INTEGER Timeout);
typedef NTSTATUS (*LSPROTO_REQUEST)(struct _LANSCSI_SESSION *LSS, PLANSCSI_PDUDESC PduDesc, PBYTE PduResponse);
typedef NTSTATUS (*LSPROTO_REQUEST_ATAPI)(struct _LANSCSI_SESSION *LSS, PLANSCSI_PDUDESC PduDesc, PBYTE PduResponse, PBYTE PduRegister);	
typedef NTSTATUS (*LSPROTO_REQUEST_VENDOR)(struct _LANSCSI_SESSION *LSS, PLANSCSI_PDUDESC PduDesc, PBYTE PduResponse);

typedef struct _LSPROTO_FUNC {

	//
	//	16 entries
	//
	LSPROTO_SENDREQUEST		LspSendRequest;
	LSPROTO_READREPLY		LspReadReply;
	LSPROTO_LOGIN			LspLogin;
	LSPROTO_LOGOUT			LspLogout;
	LSPROTO_TEXTTARGETLIST	LspTextTargetList;
	LSPROTO_TEXTTARGETDATA	LspTextTargetData;

	NTSTATUS	
	(*LspRequest) ( 
		IN  struct _LANSCSI_SESSION *LSS,
		IN  PLANSCSI_PDUDESC		PduDesc,
		OUT PBYTE					PduResponse,
		IN  struct _LANSCSI_SESSION *LSS2,
		IN  PLANSCSI_PDUDESC		PduDesc2,
		OUT PBYTE					PduResponse2,
		IN	BOOLEAN					SetStatics );

	LSPROTO_REQUEST_ATAPI	LspPacketRequest;
	LSPROTO_REQUEST_VENDOR	LspVendorRequest;
	PVOID					Reserved[6];
	PVOID					LspExtension;

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


struct _NDAS_TRANSPORT_INTERFACE;

typedef struct _NDAS_TRANS_ADDRESS_FILE {

	struct _NDAS_TRANSPORT_INTERFACE	*Protocol;
	HANDLE								AddressFileHandle;
	PFILE_OBJECT						AddressFileObject;

} NDAS_TRANS_ADDRESS_FILE, *PNDAS_TRANS_ADDRESS_FILE;


typedef struct _NDAS_TRANS_CONNECTION_FILE {

	struct _NDAS_TRANSPORT_INTERFACE	*Protocol;
	HANDLE								ConnectionFileHandle;
	PFILE_OBJECT						ConnectionFileObject;
	LPXTDI_OVERLAPPED_CONTEXT			OverlappedContext;

} NDAS_TRANS_CONNECTION_FILE, *PNDAS_TRANS_CONNECTION_FILE;

#endif
