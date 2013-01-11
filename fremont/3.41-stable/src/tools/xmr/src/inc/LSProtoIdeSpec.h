#ifndef LANSCSI_PROTOCOL_IDE_SPEC_H
#define LANSCSI_PROTOCOL_IDE_SPEC_H

//////////////////////////////////////////////////////////////////////////
//
//	Lanscsi/Ide protocol version 1.0
//

#include <pshpack1.h>

// IDE Request.
typedef struct _LANSCSI_IDE_REQUEST_PDU_HEADER {
	unsigned _int8	Opcode;
	
	// Flags.
	union {
		struct {
			unsigned _int8	FlagReserved:5;
			unsigned _int8	W:1;
			unsigned _int8	R:1;
			unsigned _int8	F:1;
		};
		unsigned _int8	Flags;
	};

	unsigned _int16	Reserved1;
	unsigned _int32	HPID;
	unsigned _int16	RPID;
	unsigned _int16	CPSlot;
	unsigned _int32	DataSegLen;
	unsigned _int16	AHSLen;
	unsigned _int16	CSubPacketSeq;
	unsigned _int32	PathCommandTag;
	unsigned _int32	InitiatorTaskTag;
	unsigned _int32	DataTransferLength;	
	unsigned _int32	TargetID;
	unsigned _int64	LUN;
	
	unsigned _int8	Reserved2;
	unsigned _int8	Feature;
	unsigned _int8	SectorCount_Prev;
	unsigned _int8	SectorCount_Curr;
	unsigned _int8	LBALow_Prev;
	unsigned _int8	LBALow_Curr;
	unsigned _int8	LBAMid_Prev;
	unsigned _int8	LBAMid_Curr;
	unsigned _int8	LBAHigh_Prev;
	unsigned _int8	LBAHigh_Curr;
	unsigned _int8	Reserved3;
	
	union {
		unsigned _int8	Device;
		struct {
			unsigned _int8	LBAHeadNR:4;
			unsigned _int8	DEV:1;
			unsigned _int8	obs1:1;
			unsigned _int8	LBA:1;
			unsigned _int8	obs2:1;
		};
	};

	unsigned _int8	Reserved4;
	unsigned _int8	Command;
	unsigned _int16	Reserved5;

} LANSCSI_IDE_REQUEST_PDU_HEADER, *PLANSCSI_IDE_REQUEST_PDU_HEADER;

// IDE Reply.
typedef struct _LANSCSI_IDE_REPLY_PDU_HEADER {
	unsigned _int8	Opcode;
	
	// Flags.
	union {
		struct {
			unsigned _int8	FlagReserved:5;
			unsigned _int8	W:1;
			unsigned _int8	R:1;
			unsigned _int8	F:1;

		};
		unsigned _int8	Flags;
	};

	unsigned _int8	Response;
	
	unsigned _int8	Status;
	
	unsigned _int32	HPID;
	unsigned _int16	RPID;
	unsigned _int16	CPSlot;
	unsigned _int32	DataSegLen;
	unsigned _int16	AHSLen;
	unsigned _int16	CSubPacketSeq;
	unsigned _int32	PathCommandTag;
	unsigned _int32	InitiatorTaskTag;
	unsigned _int32	DataTransferLength;	
	unsigned _int32	TargetID;
	unsigned _int64	LUN;
	
	unsigned _int8	Reserved2;
	unsigned _int8	Feature;
	unsigned _int8	SectorCount_Prev;
	unsigned _int8	SectorCount_Curr;
	unsigned _int8	LBALow_Prev;
	unsigned _int8	LBALow_Curr;
	unsigned _int8	LBAMid_Prev;
	unsigned _int8	LBAMid_Curr;
	unsigned _int8	LBAHigh_Prev;
	unsigned _int8	LBAHigh_Curr;
	unsigned _int8	Reserved3;

	union {
		unsigned _int8	Device;
		struct {
			unsigned _int8	LBAHeadNR:4;
			unsigned _int8	DEV:1;
			unsigned _int8	obs1:1;
			unsigned _int8	LBA:1;
			unsigned _int8	obs2:1;
		};
	};

	unsigned _int8	Reserved4;
	unsigned _int8	Command;
	unsigned _int16	Reserved5;
} LANSCSI_IDE_REPLY_PDU_HEADER, *PLANSCSI_IDE_REPLY_PDU_HEADER;



//////////////////////////////////////////////////////////////////////////
//
//	Lanscsi/Ide protocol version 1.1
//

// IDE Request.
typedef struct _LANSCSI_IDE_REQUEST_PDU_HEADER_V1 {
	unsigned _int8	Opcode;
	
	// Flags.
	union {
		struct {
			unsigned _int8	FlagReserved:5;
			unsigned _int8	W:1;
			unsigned _int8	R:1;
			unsigned _int8	F:1;
		};
		unsigned _int8	Flags;
	};
	
	unsigned _int16	Reserved1;
	unsigned _int32	HPID;
	unsigned _int16	RPID;
	unsigned _int16	CPSlot;
	unsigned _int32	DataSegLen;
	unsigned _int16	AHSLen;
	unsigned _int16	CSubPacketSeq;
	unsigned _int32	PathCommandTag;
	unsigned _int32	InitiatorTaskTag;
	unsigned _int32	DataTransferLength;	
	unsigned _int32	TargetID;
	unsigned _int64	LUN;
	
	unsigned _int32 COM_Reserved : 2;
	unsigned _int32 COM_TYPE_E : 1;
	unsigned _int32 COM_TYPE_R : 1;
	unsigned _int32 COM_TYPE_W : 1;
	unsigned _int32 COM_TYPE_D_P : 1;
	unsigned _int32 COM_TYPE_K : 1;
	unsigned _int32 COM_TYPE_P : 1;
	unsigned _int32 COM_LENG : 24;
	
	unsigned _int8	Feature_Prev;
	unsigned _int8	Feature_Curr;
	unsigned _int8	SectorCount_Prev;
	unsigned _int8	SectorCount_Curr;
	unsigned _int8	LBALow_Prev;
	unsigned _int8	LBALow_Curr;
	unsigned _int8	LBAMid_Prev;
	unsigned _int8	LBAMid_Curr;
	unsigned _int8	LBAHigh_Prev;
	unsigned _int8	LBAHigh_Curr;
	unsigned _int8	Command;
	
	union {
		unsigned _int8	Device;
		struct {
			unsigned _int8	LBAHeadNR:4;
			unsigned _int8	DEV:1;
			unsigned _int8	obs1:1;
			unsigned _int8	LBA:1;
			unsigned _int8	obs2:1;
		};
	};

} LANSCSI_IDE_REQUEST_PDU_HEADER_V1, *PLANSCSI_IDE_REQUEST_PDU_HEADER_V1;

// IDE Reply.
typedef struct _LANSCSI_IDE_REPLY_PDU_HEADER_V1 {
	unsigned _int8	Opcode;
	
	// Flags.
	union {
		struct {
			unsigned _int8	FlagReserved:5;
			unsigned _int8	W:1;
			unsigned _int8	R:1;
			unsigned _int8	F:1;

		};
		unsigned _int8	Flags;
	};

	unsigned _int8	Response;
	
	unsigned _int8	Status;
	
	unsigned _int32	HPID;
	unsigned _int16	RPID;
	unsigned _int16	CPSlot;
	unsigned _int32	DataSegLen;
	unsigned _int16	AHSLen;
	unsigned _int16	CSubPacketSeq;
	unsigned _int32	PathCommandTag;
	unsigned _int32	InitiatorTaskTag;
	unsigned _int32	DataTransferLength;	
	unsigned _int32	TargetID;
	unsigned _int64	LUN;
	
	unsigned _int32 COM_Reserved : 2;
	unsigned _int32 COM_TYPE_E : 1;
	unsigned _int32 COM_TYPE_R : 1;
	unsigned _int32 COM_TYPE_W : 1;
	unsigned _int32 COM_TYPE_D_P : 1;
	unsigned _int32 COM_TYPE_K : 1;
	unsigned _int32 COM_TYPE_P : 1;
	unsigned _int32 COM_LENG : 24;
	
	unsigned _int8	Feature_Prev;
	unsigned _int8	Feature_Curr;
	unsigned _int8	SectorCount_Prev;
	unsigned _int8	SectorCount_Curr;
	unsigned _int8	LBALow_Prev;
	unsigned _int8	LBALow_Curr;
	unsigned _int8	LBAMid_Prev;
	unsigned _int8	LBAMid_Curr;
	unsigned _int8	LBAHigh_Prev;
	unsigned _int8	LBAHigh_Curr;
	unsigned _int8	Command;
	
	union {
		unsigned _int8	Device;
		struct {
			unsigned _int8	LBAHeadNR:4;
			unsigned _int8	DEV:1;
			unsigned _int8	obs1:1;
			unsigned _int8	LBA:1;
			unsigned _int8	obs2:1;
		};
	};

} LANSCSI_IDE_REPLY_PDU_HEADER_V1, *PLANSCSI_IDE_REPLY_PDU_HEADER_V1;

//
// IDE PACKET Operation.
//

// IDE PACKET Request.
typedef struct _LANSCSI_PACKET_REQUEST_PDU_HEADER {
	unsigned _int8	Opcode;
	
	// Flags.
	union {
		struct {
			unsigned _int8	FlagReserved:4;
			unsigned _int8	P:1;
			unsigned _int8	W:1;
			unsigned _int8	R:1;
			unsigned _int8	F:1;
		};
		unsigned _int8	Flags;
	};
	
	unsigned _int16	Reserved1;
	unsigned _int32	HPID;
	unsigned _int16	RPID;
	unsigned _int16	CPSlot;
	unsigned _int32	DataSegLen;
	unsigned _int16	AHSLen;
	unsigned _int16	CSubPacketSeq;
	unsigned _int32	PathCommandTag;
	unsigned _int32	InitiatorTaskTag;
	unsigned _int32	DataTransferLength;	
	unsigned _int32	TargetID;
	unsigned _int64	LUN;

/*
	union {
		unsigned	_int32 COM_LENG;
		struct {
			unsigned _int8 COM_Reserved : 2;
			unsigned _int8 COM_TYPE_E : 1;
			unsigned _int8 COM_TYPE_R : 1;
			unsigned _int8 COM_TYPE_W : 1;
			unsigned _int8 COM_TYPE_D_P : 1;
			unsigned _int8 COM_TYPE_K : 1;
			unsigned _int8 COM_TYPE_P : 1;
			unsigned _int16 lLength;
		};
	};
*/	
	unsigned _int32 COM_Reserved : 2;
	unsigned _int32 COM_TYPE_E : 1;
	unsigned _int32 COM_TYPE_R : 1;
	unsigned _int32 COM_TYPE_W : 1;
	unsigned _int32 COM_TYPE_D_P : 1;
	unsigned _int32 COM_TYPE_K : 1;
	unsigned _int32 COM_TYPE_P : 1;
	unsigned _int32 COM_LENG : 24;
	
	unsigned _int8	Feature_Prev;
	unsigned _int8	Feature_Curr;
	unsigned _int8	SectorCount_Prev;
	unsigned _int8	SectorCount_Curr;
	unsigned _int8	LBALow_Prev;
	unsigned _int8	LBALow_Curr;
	unsigned _int8	LBAMid_Prev;
	unsigned _int8	LBAMid_Curr;
	unsigned _int8	LBAHigh_Prev;
	unsigned _int8	LBAHigh_Curr;
	unsigned _int8	Command;
	
	union {
		unsigned _int8	Device;
		struct {
			unsigned _int8	LBAHeadNR:4;
			unsigned _int8	DEV:1;
			unsigned _int8	obs1:1;
			unsigned _int8	LBA:1;
			unsigned _int8	obs2:1;
		};
	};
	// packet command area
	// additional field
	// --> ded key is not included to  additional field
	unsigned _int8			PKCMD[12];
} LANSCSI_PACKET_REQUEST_PDU_HEADER, *PLANSCSI_PACKET_REQUEST_PDU_HEADER;

// IDE Reply.
typedef struct _LANSCSI_PACKET_REPLY_PDU_HEADER {
	unsigned _int8	Opcode;
	
	// Flags.
	union {
		struct {
			unsigned _int8	FlagReserved:4;
			unsigned _int8  P:1;
			unsigned _int8	W:1;
			unsigned _int8	R:1;
			unsigned _int8	F:1;

		};
		unsigned _int8	Flags;
	};

	unsigned _int8	Response;
	
	unsigned _int8	Status;
	
	unsigned _int32	HPID;
	unsigned _int16	RPID;
	unsigned _int16	CPSlot;
	unsigned _int32	DataSegLen;
	unsigned _int16	AHSLen;
	unsigned _int16	CSubPacketSeq;
	unsigned _int32	PathCommandTag;
	unsigned _int32	InitiatorTaskTag;
	unsigned _int32	DataTransferLength;	
	unsigned _int32	TargetID;
	unsigned _int64	LUN;
/*
	union {
		unsigned	_int32 COM_LENG;
		struct {
			unsigned _int8 COM_Reserved : 2;
			unsigned _int8 COM_TYPE_E : 1;
			unsigned _int8 COM_TYPE_R : 1;
			unsigned _int8 COM_TYPE_W : 1;
			unsigned _int8 COM_TYPE_D_P : 1;
			unsigned _int8 COM_TYPE_K : 1;
			unsigned _int8 COM_TYPE_P : 1;
			unsigned _int16 lLength;
		};
	};
*/	
	unsigned _int32 COM_Reserved : 2;
	unsigned _int32 COM_TYPE_E : 1;
	unsigned _int32 COM_TYPE_R : 1;
	unsigned _int32 COM_TYPE_W : 1;
	unsigned _int32 COM_TYPE_D_P : 1;
	unsigned _int32 COM_TYPE_K : 1;
	unsigned _int32 COM_TYPE_P : 1;
	unsigned _int32 COM_LENG : 24;
	
	unsigned _int8	Feature_Prev;
	unsigned _int8	Feature_Curr;
	unsigned _int8	SectorCount_Prev;
	unsigned _int8	SectorCount_Curr;
	unsigned _int8	LBALow_Prev;
	unsigned _int8	LBALow_Curr;
	unsigned _int8	LBAMid_Prev;
	unsigned _int8	LBAMid_Curr;
	unsigned _int8	LBAHigh_Prev;
	unsigned _int8	LBAHigh_Curr;
	unsigned _int8	Command;
	
	union {
		unsigned _int8	Device;
		struct {
			unsigned _int8	LBAHeadNR:4;
			unsigned _int8	DEV:1;
			unsigned _int8	obs1:1;
			unsigned _int8	LBA:1;
			unsigned _int8	obs2:1;
		};
	};
	// packet command area
	// additional field
	// --> ded key is not included to  additional field
	unsigned _int8			PKCMD[12];
} LANSCSI_PACKET_REPLY_PDU_HEADER, *PLANSCSI_PACKET_REPLY_PDU_HEADER;

//
// Vender Specific Operation.
//

#define	NKC_VENDER_ID				0x0001
#define	VENDER_OP_CURRENT_VERSION	0x01

#define	VENDER_OP_SET_MAX_RET_TIME	0x01
#define VENDER_OP_SET_MAX_CONN_TIME	0x02
#define VENDER_OP_SET_SUPERVISOR_PW	0x11
#define VENDER_OP_SET_USER_PW		0x12
#define VENDER_OP_SET_ENC_OPT		0x13
#define VENDER_OP_SET_STANBY_TIMER  0x14
#define VENDER_OP_RESET				0xFF
#define VENDER_OP_RESET2			0xFE

#define	VENDER_OP_GET_MAX_RET_TIME	0x03
#define	VENDER_OP_GET_MAX_CONN_TIME	0x04

#define VENDER_OP_SET_SEMA			0x05
#define VENDER_OP_FREE_SEMA			0x06
#define VENDER_OP_GET_SEMA			0x07
#define VENDER_OP_OWNER_SEMA		0x08
#define VENDER_OP_SET_DYNAMIC_MAX_CONN_TIME			0x0a
#define VENDER_OP_SET_DELAY			0x16
#define VENDER_OP_GET_DELAY			0x17
#define VENDER_OP_SET_DYNAMIC_MAX_RET_TIME			0x18
#define VENDER_OP_GET_DYNAMIC_MAX_RET_TIME			0x19
#define VENDER_OP_SET_D_ENC_OPT						0x1A
#define VENDER_OP_GET_D_ENC_OPT						0x1B


	
// Vender Specific Request.
typedef struct _LANSCSI_VENDER_REQUEST_PDU_HEADER {
	UCHAR	Opcode;
	
	// Flags.
	union {
		struct {
			UCHAR	FlagReserved:7;
			UCHAR	F:1;
		};
		UCHAR	Flags;
	};
	
	_int16	Reserved1;
	unsigned	HPID;
	_int16	RPID;
	_int16	CPSlot;
	unsigned	DataSegLen;
	_int16	AHSLen;
	_int16	CSubPacketSeq;
	unsigned	PathCommandTag;

	unsigned	Reserved2;
	unsigned	Reserved3;
	unsigned	Reserved4;
	_int64	Reserved5;
	
	unsigned _int16	VenderID;
	unsigned _int8	VenderOpVersion;
	unsigned _int8	VenderOp;
	unsigned _int64	VenderParameter;
	
	unsigned	Reserved6;
} LANSCSI_VENDER_REQUEST_PDU_HEADER, *PLANSCSI_VENDER_REQUEST_PDU_HEADER;

// Vender Specific Reply.
typedef struct _LANSCSI_VENDER_REPLY_PDU_HEADER {
	UCHAR	Opcode;
	
	// Flags.
	union {
		struct {
			UCHAR	FlagReserved:7;
			UCHAR	F:1;
		};
		UCHAR	Flags;
	};


	UCHAR	Response;
	
	UCHAR	Reserved1;
	
	unsigned	HPID;
	_int16	RPID;
	_int16	CPSlot;
	unsigned	DataSegLen;
	_int16	AHSLen;
	_int16	CSubPacketSeq;
	unsigned	PathCommandTag;
	
	unsigned	Reserved2;
	unsigned	Reserved3;
	unsigned	Reserved4;
	_int64	Reserved5;
	
	unsigned _int16	VenderID;
	unsigned _int8	VenderOpVersion;
	unsigned _int8	VenderOp;
	unsigned _int64	VenderParameter;
	
	unsigned	Reserved6;
} LANSCSI_VENDER_REPLY_PDU_HEADER, *PLANSCSI_VENDER_REPLY_PDU_HEADER;


		// com type
#define WANSCSI_COM_CONTROL		1
#define WANSCSI_COM_SESSION		2
#define WANSCSI_COM_LANSCSI		3
		
		// SESSION command
#define WANSCSI_SESSION_INIT_DATA				0x09
#define WANSCSI_SESSION_MAKE_NDASCONN			0x0A
#define WANSCSI_SESSION_CLOSE_NDASCONN			0x0B
#define WANSCSI_SESSION_LOGINED					0x0C
		
		// error code
#define WANSCSI_ERR_SUCCESS						0x00
#define WANSCSI_ERR_READ_SESSION_REQUEST_H2R	0x01
#define WANSCSI_ERR_WRITE_SESSION_REPLY_R2H		0x02
#define WANSCSI_ERR_READ_SESSION_DATA_H2R		0x03
#define WANSCSI_ERR_WRITE_SESSION_DATA_R2H		0x04
#define WANSCSI_ERR_MAKE_NDAS_CONN				0x05
#define WANSCSI_ERR_INVALIDE_STATE_REQUEST		0x06
#define WANSCSI_ERR_NOT_SUPPORTED_REQUEST		0x07
#define WANSCSI_ERR_READ_LANSCSI_REQUEST_H2R	0x08
#define WANSCSI_ERR_WRITE_LANSCSI_REQUEST_R2N	0x09
#define WANSCSI_ERR_READ_LANSCSI_REPLY_N2R		0x10
#define WANSCSI_ERR_WRITE_LANSCSI_REPLY_R2H		0x11
#define WANSCSI_ERR_WRITE_DATA_R2H				0x12
#define WANSCSI_ERR_READ_DATA_H2R				0x13
#define WANSCSI_ERR_WRITE_DATA_R2N				0x14
#define WANSCSI_ERR_READ_DATA_N2R				0x15
#define WANSCSI_ERR_UNSUCCESSFULL_LANSCSI_COM	0x16
/*
		// Session Status
#define	SESSION_CLOSED						0x00
#define	SESSION_CONNECTED					0x01
#define SESSION_INIT						0x02
#define SESSION_LOGIN						0x04
#define	SESSION_ABNORMAL_DISCONN			0x08
*/

// NDAS Status
#define	NDAS_DISCONNECTED					0x00
#define	NDAS_CONNECTED						0x01 
#define NDAS_ABNORMAL_DISCONN				0x02


typedef struct _WANSCSI_COMMANDREQ_HEADER{
	unsigned _int8	OPCODE;
	unsigned _int8	SubCOM;
	unsigned _int16	RESERVED1_2;
	unsigned _int32	RESERVED2;
	unsigned _int32 RESERVED3;
	unsigned _int32 RESERVED4;
}WANSCSI_COMMANDREQ_HEADER, *PWANSCSI_COMMANDREQ_HEADER;


typedef struct _WANSCSI_COMMANDREP_HEADER{
	unsigned _int8	OPCODE;
	unsigned _int8	SubCOM;
	unsigned _int8	SESSION_STATUS;
	unsigned _int8	NDAS_STATUS;
	unsigned _int8	ERR_CODE;
	unsigned _int8	HWType;
	unsigned _int8	HWVersion;
	unsigned _int8	RESERVED1;
	unsigned _int32 RESERVED2;
	unsigned _int32 RESERVED3;	
}WANSCSI_COMMANDREP_HEADER, *PWANSCSI_COMMANDREP_HEADER;



typedef struct _WANSCSI_COMMANDREQ_LOGINED_HEADER{
	unsigned _int8			OPCODE;
	unsigned _int8			SubCOM;
	unsigned _int8			RESERVED1;
	unsigned _int8			iSessionPhase;
	unsigned _int16			iHeaderEncryptAlgo;
	unsigned _int16			iDataEncryptAlgo;
	unsigned _int32			RESERVED2;
	unsigned _int32			RESERVED3;
}WANSCSI_COMMANDREQ_LOGINED_HEADER, *PWANSCSI_COMMANDREQ_LOGINED_HEADER;

// type specifier
#define DEV_DISK	0;
#define DEV_ODD		1;


typedef struct _WANSCSI_INIT_DATA{
	unsigned _int8	DeviceType;
	unsigned _int8	DestAddress[6];
	unsigned _int8	TargetID;
	unsigned _int32	AccessRright;
	unsigned _int8	Reserved;
}WANSCSI_INIT_DATA, *PWANSCSI_INIT_DATA;


// AddOp
#define NONOP			0
#define READOP			1
#define WRITEOP			2
#define WRITEREAD		3

typedef struct _WANSCSI_COMMANDREQ_HEADER2{
	unsigned _int8	OPCODE;
	unsigned _int8	SubCOM;
	unsigned _int8	LanscsiOP;				// Lanscsi OpCode for debugging
	unsigned _int8	AddOP;					// Additional OP  for debugging
	unsigned _int32	AdditionalWriteSize;	// Additional OP  Write size
	unsigned _int32	AdditioanlReadSize;		// Additional OP  Read	size			
	unsigned _int32	AdditionalSize;	
	unsigned _int32 ReqHeaderSize;
	unsigned _int32 ReplyHeaderSize;
}WANSCSI_COMMANDREQ_HEADER2, *PWANSCSI_COMMANDREQ_HEADER2;

typedef struct _WANSCSI_COMMANDREP_HEADER2{
	unsigned _int8	OPCODE;
	unsigned _int8	SubCOM;
	unsigned _int8	SESSION_STATUS;
	unsigned _int8	NDAS_STATUS;
	unsigned _int8	ERR_CODE;
	unsigned _int8	HWType;
	unsigned _int8	HWVersion;
	unsigned _int8	RESERVED1;
	unsigned _int32 RESERVED2;
	unsigned _int32 RESERVED3;
	unsigned _int64 RESERVED4;
}WANSCSI_COMMANDREP_HEADER2, *PWANSCSI_COMMANDREP_HEADER2;

//		Error Code
#define WAN_ERR_SUCCESS						0x00
#define WAN_ERR_READ_SESSION_REQUEST_H2R	0x01
#define WAN_ERR_WRITE_SESSION_REPLY_R2H		0x02
#define WAN_ERR_READ_DATA_H2R				0x03
#define WAN_ERR_WRITE_DATA_R2H				0x04
#define WAN_ERR_READ_DATA_N2R				0x05
#define WAN_ERR_WRITE_DATA_R2N				0x06
#define WAN_ERR_INVALID_STATE				0x07
#define WAN_ERR_INVALID_PARAMETER			0x08
#define WAN_ERR_NOT_SUPPORTED_REQ			0x09
#define WAN_ERR_DISCONNECTED_NDAS			0x10
#define WAN_ERR_DISCONNECTED_WAN			0x11
#define WAN_ERR_CONNECT_NDAS				0x12
#define WAN_ERR_INVALID_COMMAND				0x13
#include <poppack.h>

#endif