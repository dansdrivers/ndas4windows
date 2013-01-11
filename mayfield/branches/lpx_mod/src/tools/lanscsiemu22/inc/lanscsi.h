#ifndef __LANSCSI_H__
#define __LANSCSI_H__

#include <pshpack1.h>

#define LANSCSI_CURRENT_VERSION		0
#define LANSCSI_VERSION_1_1			1
#define	LPX_PORT_NUMBER				10000

//
// Operation Codes.
//

// Host to Remote
#define NOP_H2R						0x00
#define LOGIN_REQUEST				0x01
#define LOGOUT_REQUEST				0x02
#define	TEXT_REQUEST				0x03
#define	TASK_MANAGEMENT_REQUEST		0x04
#define	SCSI_COMMAND				0x05
#define	DATA_H2R					0x06
#define	IDE_COMMAND					0x08
#define	VENDER_SPECIFIC_COMMAND		0x0F	

// Remote to Host
#define NOP_R2H						0x10
#define LOGIN_RESPONSE				0x11
#define LOGOUT_RESPONSE				0x12
#define	TEXT_RESPONSE				0x13
#define	TASK_MANAGEMENT_RESPONSE	0x14
#define	SCSI_RESPONSE				0x15
#define	DATA_R2H					0x16
#define READY_TO_TRANSFER			0x17
#define	IDE_RESPONSE				0x18
#define	VENDER_SPECIFIC_RESPONSE	0x1F	

//
// Error code.
//
#define LANSCSI_RESPONSE_SUCCESS                0x00
#define LANSCSI_RESPONSE_RI_NOT_EXIST           0x10
#define LANSCSI_RESPONSE_RI_BAD_COMMAND         0x11
#define LANSCSI_RESPONSE_RI_COMMAND_FAILED      0x12
#define LANSCSI_RESPONSE_RI_VERSION_MISMATCH    0x13
#define LANSCSI_RESPONSE_RI_AUTH_FAILED         0x14
#define LANSCSI_RESPONSE_T_NOT_EXIST			0x20
#define LANSCSI_RESPONSE_T_BAD_COMMAND          0x21
#define LANSCSI_RESPONSE_T_COMMAND_FAILED       0x22
#define LANSCSI_RESPONSE_T_BROKEN_DATA			0x23

//
// Host To Remote PDU Format
//
typedef struct _LANSCSI_H2R_PDU_HEADER {
	UCHAR	Opcode;
	UCHAR	Flags;
	_int16	Reserved1;
	
	unsigned	HPID;
	_int16	RPID;
	_int16	CPSlot;
	unsigned	DataSegLen;
	_int16	AHSLen;
	_int16	CSubPacketSeq;
	unsigned	PathCommandTag;
	unsigned	InitiatorTaskTag;
	unsigned	DataTransferLength;
	unsigned	TargetID;
	_int64	Lun;
	
	unsigned	Reserved2;
	unsigned	Reserved3;
	unsigned	Reserved4;
	unsigned	Reserved5;
} LANSCSI_H2R_PDU_HEADER, *PLANSCSI_H2R_PDU_HEADER;

//
// Host To Remote PDU Format
//
typedef struct _LANSCSI_R2H_PDU_HEADER {
	UCHAR	Opocde;
	UCHAR	Flags;
	UCHAR	Response;
	
	UCHAR	Reserved1;
	
	unsigned	HPID;
	_int16	RPID;
	_int16	CPSlot;
	unsigned	DataSegLen;
	_int16	AHSLen;
	_int16	CSubPacketSeq;
	unsigned	PathCommandTag;
	unsigned	InitiatorTaskTag;
	unsigned	DataTransferLength;
	unsigned	TargetID;
	_int64	Lun;
	
	unsigned	Reserved2;
	unsigned	Reserved3;
	unsigned	Reserved4;
	unsigned	Reserved5;
} LANSCSI_R2H_PDU_HEADER, *PLANSCSI_R2H_PDU_HEADER;

//
// Basic PDU.
//
typedef struct _LANSCSI_PDU {
	union {
		PLANSCSI_H2R_PDU_HEADER	pH2RHeader;
		PLANSCSI_R2H_PDU_HEADER	pR2HHeader;
	};
	PCHAR					pAHS;
	PCHAR					pHeaderDig;
	PCHAR					pDataSeg;
	PCHAR					pDataDig;
} LANSCSI_PDU, *PLANSCSI_PDU;

//
// Login Operation.
//

// Stages.
#define	FLAG_SECURITY_PHASE			0
#define	FLAG_LOGIN_OPERATION_PHASE	1
#define	FLAG_FULL_FEATURE_PHASE		3

#define LOGOUT_PHASE				4

// Parameter Types.
#define	PARAMETER_TYPE_TEXT		0x0
#define	PARAMETER_TYPE_BINARY	0x1

#define LOGIN_FLAG_CSG_MASK		0x0C
#define LOGIN_FLAG_NSG_MASK		0x03

// Login Request.
typedef struct _LANSCSI_LOGIN_REQUEST_PDU_HEADER {
	UCHAR	Opocde;
	
	// Flags.
	union {
		struct {
			UCHAR	NSG:2;
			UCHAR	CSG:2;
			UCHAR	FlagReserved:2;
			UCHAR	C:1;
			UCHAR	T:1;
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

	UCHAR	ParameterType;
	UCHAR	ParameterVer;
	UCHAR	VerMax;
	UCHAR	VerMin;
	
	unsigned	Reserved6;
	unsigned	Reserved7;
	unsigned	Reserved8;
} LANSCSI_LOGIN_REQUEST_PDU_HEADER, *PLANSCSI_LOGIN_REQUEST_PDU_HEADER;

// Login Reply.
typedef struct _LANSCSI_LOGIN_REPLY_PDU_HEADER {
	UCHAR	Opocde;
	
	// Flags.
	union {
		struct {
			UCHAR	NSG:2;
			UCHAR	CSG:2;
			UCHAR	FlagReserved:2;
			UCHAR	C:1;
			UCHAR	T:1;
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
	
	UCHAR	ParameterType;
	UCHAR	ParameterVer;
	UCHAR	VerMax;
	UCHAR	VerActive;
	
	unsigned	Reserved6;
	unsigned	Reserved7;
	unsigned	Reserved8;
} LANSCSI_LOGIN_REPLY_PDU_HEADER, *PLANSCSI_LOGIN_REPLY_PDU_HEADER;

//
// Logout Operation.
//

// Logout Request.
typedef struct _LANSCSI_LOGOUT_REQUEST_PDU_HEADER {
	UCHAR	Opocde;
	
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
	
	unsigned	Reserved6;
	unsigned	Reserved7;
	unsigned	Reserved8;
	unsigned	Reserved9;
} LANSCSI_LOGOUT_REQUEST_PDU_HEADER, *PLANSCSI_LOGOUT_REQUEST_PDU_HEADER;

// Logout Reply.
typedef struct _LANSCSI_LOGOUT_REPLY_PDU_HEADER {
	UCHAR	Opocde;
	
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

	unsigned	Reserved6;
	unsigned	Reserved7;
	unsigned	Reserved8;
	unsigned	Reserved9;
} LANSCSI_LOGOUT_REPLY_PDU_HEADER, *PLANSCSI_LOGOUT_REPLY_PDU_HEADER;

//
// Text Operation.
//

// Text Request.
typedef struct _LANSCSI_TEXT_REQUEST_PDU_HEADER {
	UCHAR	Opocde;
	
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
	
	UCHAR	ParameterType;
	UCHAR	ParameterVer;
	_int16	Reserved6;	
	
	unsigned	Reserved7;
	unsigned	Reserved8;
	unsigned	Reserved9;
} LANSCSI_TEXT_REQUEST_PDU_HEADER, *PLANSCSI_TEXT_REQUEST_PDU_HEADER;

// Text Reply.
typedef struct _LANSCSI_TEXT_REPLY_PDU_HEADER {
	UCHAR	Opocde;
	
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
	
	UCHAR	ParameterType;
	UCHAR	ParameterVer;
	_int16	Reserved6;
	
	unsigned	Reserved7;
	unsigned	Reserved8;
	unsigned	Reserved9;
} LANSCSI_TEXT_REPLY_PDU_HEADER, *PLANSCSI_TEXT_REPLY_PDU_HEADER;

//
// IDE Operation.
//
//#define PACKET_COMMAND_SIZE	12
// IDE Request.
typedef struct _LANSCSI_IDE_REQUEST_PDU_HEADER {
	UCHAR	Opocde;
	
	// Flags.
	union {
		struct {
			UCHAR	FlagReserved:5;
			UCHAR	W:1;
			UCHAR	R:1;
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
	unsigned	InitiatorTaskTag;
	unsigned	DataTransferLength;	
	unsigned	TargetID;
	_int64	LUN;
	
	
	//union {
	//	unsigned int COM;
	//	struct {
			_int32 COM_Reserved : 2;
			_int32 COM_TYPE_E : 1;
			_int32 COM_TYPE_R : 1;
			_int32 COM_TYPE_W : 1;
			_int32 COM_TYPE_D_P : 1;
			_int32 COM_TYPE_K : 1;
			_int32 COM_TYPE_P : 1;
			_int32 COM_LENG : 24;
	//	};
	//};
	

	
	UCHAR	Feature_Prev;
	UCHAR	Feature_Curr;
	UCHAR	SectorCount_Prev;
	UCHAR	SectorCount_Curr;
	UCHAR	LBALow_Prev;
	UCHAR	LBALow_Curr;
	UCHAR	LBAMid_Prev;
	UCHAR	LBAMid_Curr;
	UCHAR	LBAHigh_Prev;
	UCHAR	LBAHigh_Curr;
	UCHAR	Command;
	
	union {
		UCHAR	Device;
		struct {
			UCHAR	LBAHeadNR:4;
			UCHAR	DEV:1;
			UCHAR	obs1:1;
			UCHAR	LBA:1;
			UCHAR	obs2:1;
		};
	};

	//UCHAR	Reserved4;
	//UCHAR	Command;
	//_int16	Reserved5;
	//UCHAR	PacketCommand[PACKET_COMMAND_SIZE];

} LANSCSI_IDE_REQUEST_PDU_HEADER, *PLANSCSI_IDE_REQUEST_PDU_HEADER;

// IDE Reply.
typedef struct _LANSCSI_IDE_REPLY_PDU_HEADER {
	UCHAR	Opocde;
	
	// Flags.
	union {
		struct {
			UCHAR	FlagReserved:5;
			UCHAR	W:1;
			UCHAR	R:1;
			UCHAR	F:1;

		};
		UCHAR	Flags;
	};

	UCHAR	Response;
	
	UCHAR	Status;
	
	unsigned	HPID;
	_int16	RPID;
	_int16	CPSlot;
	unsigned	DataSegLen;
	_int16	AHSLen;
	_int16	CSubPacketSeq;
	unsigned	PathCommandTag;
	unsigned	InitiatorTaskTag;
	unsigned	DataTransferLength;	
	unsigned	TargetID;
	_int64	LUN;
	
	//union {
	//	unsigned int COM;
	//	struct {
			_int32 COM_Reserved : 2;
			_int32 COM_TYPE_E : 1;
			_int32 COM_TYPE_R : 1;
			_int32 COM_TYPE_W : 1;
			_int32 COM_TYPE_D_P : 1;
			_int32 COM_TYPE_K : 1;
			_int32 COM_TYPE_P : 1;
			_int32 COM_LENG : 24;
	//	};
	//};
	

	
	UCHAR	Feature_Prev;
	UCHAR	Feature_Curr;
	UCHAR	SectorCount_Prev;
	UCHAR	SectorCount_Curr;
	UCHAR	LBALow_Prev;
	UCHAR	LBALow_Curr;
	UCHAR	LBAMid_Prev;
	UCHAR	LBAMid_Curr;
	UCHAR	LBAHigh_Prev;
	UCHAR	LBAHigh_Curr;
	UCHAR	Command;
	
	union {
		UCHAR	Device;
		struct {
			UCHAR	LBAHeadNR:4;
			UCHAR	DEV:1;
			UCHAR	obs1:1;
			UCHAR	LBA:1;
			UCHAR	obs2:1;
		};
	};

	//UCHAR	Reserved4;
	//UCHAR	Command;
	//_int16	Reserved5;
	//UCHAR	PacketCommand[PACKET_COMMAND_SIZE];

} LANSCSI_IDE_REPLY_PDU_HEADER, *PLANSCSI_IDE_REPLY_PDU_HEADER;

//
// Vender Specific Operation.
//

#define	NKC_VENDER_ID				0x0001
#define	VENDER_OP_CURRENT_VERSION	0x01

#define	VENDER_OP_SET_MAX_RET_TIME	0x01
#define VENDER_OP_SET_MAX_CONN_TIME	0x02
#define	VENDER_OP_GET_MAX_RET_TIME	0x03
#define	VENDER_OP_GET_MAX_CONN_TIME	0x04
#define VENDOR_OP_SET_SEMA			0x05
#define VENDOR_OP_FREE_SEMA			0x06
#define VENDOR_OP_GET_SEMA			0x07
#define VENDOR_OP_OWNER_SEMA		0x08
#define VENDOR_OP_SET_DYNAMIC_MAX_RET_TIME			0x09
#define VENDOR_OP_SET_DYNAMIC_MAX_CONN_TIME			0x0a
#define VENDER_OP_SET_SUPERVISOR_PW	0x11
#define VENDER_OP_SET_USER_PW		0x12
#define VENDOR_OP_SET_ENC_OPT		0x13
#define VENDOR_OP_SET_STANBY_TIMER  0x14
#define VENDOR_OP_GET_STANBY_TIMER  0x15
#define VENDER_OP_RESET				0xFF
	
// Vender Specific Request.
typedef struct _LANSCSI_VENDER_REQUEST_PDU_HEADER {
	UCHAR	Opocde;
	
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
	UCHAR	Opocde;
	
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

#include <poppack.h>

#define	MAX_REQUEST_SIZE	1500

#endif
