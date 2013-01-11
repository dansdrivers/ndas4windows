#ifndef LANSCSI_PROTOCOL_SPEC_H
#define LANSCSI_PROTOCOL_SPEC_H


//
//	NetDisk default port number based on LPX.
//
#define	LPX_PORT_NUMBER				10000

//
//	PDU max length
//
#define	MAX_REQUEST_SIZE			1500

#define LSPROTO_MAX_TRANSFER_SIZE	(16 * 4096)	// 64K

//
//	Lanscsi/IDE Protocol versions
//
#define LSIDEPROTO_VERSION_1_0			0
#define LSIDEPROTO_VERSION_1_1			1


#define LSIDEPROTO_CURRENT_VERSION		LSIDEPROTO_VERSION_1_1


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

#include <pshpack1.h>

//
// Host To Remote PDU Format
//
typedef struct _LANSCSI_H2R_PDU_HEADER {
	BYTE	Opcode;
	BYTE	Flags;
	UINT16	Reserved1;
	
	UINT32	HPID;
	UINT16	RPID;
	UINT16	CPSlot;
	UINT32	DataSegLen;
	UINT16	AHSLen;
	UINT16	CSubPacketSeq;
	UINT32	PathCommandTag;
	UINT32	InitiatorTaskTag;
	UINT32	DataTransferLength;
	UINT32	TargetID;
	UINT64	Lun;
	
	UINT32	Reserved2;
	UINT32	Reserved3;
	UINT32	Reserved4;
	UINT32	Reserved5;
} LANSCSI_H2R_PDU_HEADER, *PLANSCSI_H2R_PDU_HEADER;

//
// Host To Remote PDU Format
//
typedef struct _LANSCSI_R2H_PDU_HEADER {
	BYTE	Opcode;
	BYTE	Flags;
	BYTE	Response;

	BYTE	Reserved1;

	UINT32	HPID;
	UINT16	RPID;
	UINT16	CPSlot;
	UINT32	DataSegLen;
	UINT16	AHSLen;
	UINT16	CSubPacketSeq;
	UINT32	PathCommandTag;
	UINT32	InitiatorTaskTag;
	UINT32	DataTransferLength;
	UINT32	TargetID;
	UINT64	Lun;

	UINT32	Reserved2;
	UINT32	Reserved3;
	UINT32	Reserved4;
	UINT32	Reserved5;
} LANSCSI_R2H_PDU_HEADER, *PLANSCSI_R2H_PDU_HEADER;

//
// Basic PDU.
//
typedef struct _LANSCSI_PDU_POINTERS {

	union {
		PLANSCSI_H2R_PDU_HEADER	pH2RHeader;
		PLANSCSI_R2H_PDU_HEADER	pR2HHeader;
	};
	PCHAR					pAHS;
	PCHAR					pHeaderDig;
	PCHAR					pDataSeg;
	PCHAR					pDataDig;

} LANSCSI_PDU_POINTERS, *PLANSCSI_PDU_POINTERS;

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
	BYTE	Opcode;
	
	// Flags.
	union {
		struct {
			BYTE	NSG:2;
			BYTE	CSG:2;
			BYTE	FlagReserved:2;
			BYTE	C:1;
			BYTE	T:1;
		};
		BYTE	Flags;
	};
	
	UINT16	Reserved1;
	UINT32	HPID;
	UINT16	RPID;
	UINT16	CPSlot;
	UINT32	DataSegLen;
	UINT16	AHSLen;
	UINT16	CSubPacketSeq;
	UINT32	PathCommandTag;
	
	UINT32	Reserved2;
	UINT32	Reserved3;
	UINT32	Reserved4;
	UINT64	Reserved5;

	BYTE	ParameterType;
	BYTE	ParameterVer;
	BYTE	VerMax;
	BYTE	VerMin;
	
	UINT32	Reserved6;
	UINT32	Reserved7;
	UINT32 Reserved8;
} LANSCSI_LOGIN_REQUEST_PDU_HEADER, *PLANSCSI_LOGIN_REQUEST_PDU_HEADER;

// Login Reply.
typedef struct _LANSCSI_LOGIN_REPLY_PDU_HEADER {
	BYTE	Opcode;
	
	// Flags.
	union {
		struct {
			BYTE	NSG:2;
			BYTE	CSG:2;
			BYTE	FlagReserved:2;
			BYTE	C:1;
			BYTE	T:1;
		};
		BYTE	Flags;
	};

	BYTE	Response;
	
	BYTE	Reserved1;
	
	UINT32	HPID;
	UINT16	RPID;
	UINT16	CPSlot;
	UINT32	DataSegLen;
	UINT16	AHSLen;
	UINT16	CSubPacketSeq;
	UINT32	PathCommandTag;

	UINT32	Reserved2;
	UINT32	Reserved3;
	UINT32	Reserved4;
	UINT64	Reserved5;
	
	BYTE	ParameterType;
	BYTE	ParameterVer;
	BYTE	VerMax;
	BYTE	VerActive;
	
	UINT32	Reserved6;
	UINT32	Reserved7;
	UINT32	Reserved8;
} LANSCSI_LOGIN_REPLY_PDU_HEADER, *PLANSCSI_LOGIN_REPLY_PDU_HEADER;

//
// Logout Operation.
//

// Logout Request.
typedef struct _LANSCSI_LOGOUT_REQUEST_PDU_HEADER {
	BYTE	Opcode;
	
	// Flags.
	union {
		struct {
			BYTE	FlagReserved:7;
			BYTE	F:1;
		};
		BYTE	Flags;
	};
	
	UINT16	Reserved1;
	UINT32	HPID;
	UINT16	RPID;
	UINT16	CPSlot;
	UINT32	DataSegLen;
	UINT16	AHSLen;
	UINT16	CSubPacketSeq;
	UINT32	PathCommandTag;

	UINT32	Reserved2;	
	UINT32	Reserved3;
	UINT32	Reserved4;
	UINT64	Reserved5;
	
	UINT32	Reserved6;
	UINT32	Reserved7;
	UINT32	Reserved8;
	UINT32	Reserved9;
} LANSCSI_LOGOUT_REQUEST_PDU_HEADER, *PLANSCSI_LOGOUT_REQUEST_PDU_HEADER;

// Logout Reply.
typedef struct _LANSCSI_LOGOUT_REPLY_PDU_HEADER {
	BYTE	Opcode;
	
	// Flags.
	union {
		struct {
			BYTE	FlagReserved:7;
			BYTE	F:1;
		};
		BYTE	Flags;
	};

	BYTE	Response;
	
	BYTE	Reserved1;
	
	UINT32	HPID;
	UINT16	RPID;
	UINT16	CPSlot;
	UINT32	DataSegLen;
	UINT16	AHSLen;
	UINT16	CSubPacketSeq;
	UINT32	PathCommandTag;

	UINT32	Reserved2;	
	UINT32	Reserved3;
	UINT32	Reserved4;
	UINT64	Reserved5;

	UINT32	Reserved6;
	UINT32	Reserved7;
	UINT32	Reserved8;
	UINT32	Reserved9;
} LANSCSI_LOGOUT_REPLY_PDU_HEADER, *PLANSCSI_LOGOUT_REPLY_PDU_HEADER;

//
// Text Operation.
//

// Text Request.
typedef struct _LANSCSI_TEXT_REQUEST_PDU_HEADER {
	BYTE	Opcode;
	
	// Flags.
	union {
		struct {
			BYTE	FlagReserved:7;
			BYTE	F:1;
		};
		BYTE	Flags;
	};
	
	UINT16	Reserved1;
	UINT32	HPID;
	UINT16	RPID;
	UINT16	CPSlot;
	UINT32	DataSegLen;
	UINT16	AHSLen;
	UINT16	CSubPacketSeq;
	UINT32	PathCommandTag;

	UINT32	Reserved2;
	UINT32	Reserved3;
	UINT32	Reserved4;
	UINT64	Reserved5;
	
	BYTE	ParameterType;
	BYTE	ParameterVer;
	UINT16	Reserved6;	
	
	UINT32	Reserved7;
	UINT32	Reserved8;
	UINT32	Reserved9;
} LANSCSI_TEXT_REQUEST_PDU_HEADER, *PLANSCSI_TEXT_REQUEST_PDU_HEADER;

// Text Reply.
typedef struct _LANSCSI_TEXT_REPLY_PDU_HEADER {
	BYTE	Opcode;
	
	// Flags.
	union {
		struct {
			BYTE	FlagReserved:7;
			BYTE	F:1;
		};
		BYTE	Flags;
	};

	BYTE	Response;
	
	BYTE	Reserved1;
	
	UINT32	HPID;
	UINT16	RPID;
	UINT16	CPSlot;
	UINT32	DataSegLen;
	UINT16	AHSLen;
	UINT16	CSubPacketSeq;
	UINT32	PathCommandTag;

	UINT32	Reserved2;
	UINT32	Reserved3;
	UINT32	Reserved4;
	UINT64	Reserved5;
	
	BYTE	ParameterType;
	BYTE	ParameterVer;
	UINT16	Reserved6;
	
	UINT32	Reserved7;
	UINT32	Reserved8;
	UINT32	Reserved9;
} LANSCSI_TEXT_REPLY_PDU_HEADER, *PLANSCSI_TEXT_REPLY_PDU_HEADER;


#include <poppack.h>


#endif
