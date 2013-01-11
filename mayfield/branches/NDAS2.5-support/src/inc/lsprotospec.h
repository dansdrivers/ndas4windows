#ifndef LANSCSI_PROTOCOL_SPEC_H
#define LANSCSI_PROTOCOL_SPEC_H


//
//	NetDisk default port number based on LPX.
//
#define	LPX_PORT_NUMBER				10000 // frame.vhd : PORT_NUM

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
#define NOP_H2R						0x00 // frame.vhd : OP_NOP_REQ
#define LOGIN_REQUEST				0x01 // frame.vhd : OP_LOGIN_REQ
#define LOGOUT_REQUEST				0x02 // frame.vhd : OP_LOGOUT_REQ
#define	TEXT_REQUEST				0x03 // frame.vhd : OP_TEXT_REQ
#define	TASK_MANAGEMENT_REQUEST		0x04 // frame.vhd : OP_TASK_REQ
#define	SCSI_COMMAND				0x05 // frame.vhd : OP_SCSI_REQ
#define	DATA_H2R					0x06 // frame.vhd : OP_DATA_REQ
#define	IDE_COMMAND					0x08 // frame.vhd : OP_IDE_REQ
#define	VENDOR_SPECIFIC_COMMAND		0x0F // frame.vhd : OP_VENDOR_REQ

// Remote to Host
#define NOP_R2H						0x10 // frame.vhd : OP_NOP_REP
#define LOGIN_RESPONSE				0x11 // frame.vhd : OP_LOGIN_REP
#define LOGOUT_RESPONSE				0x12 // frame.vhd : OP_LOGOUT_REP
#define	TEXT_RESPONSE				0x13 // frame.vhd : OP_TEXT_REP
#define	TASK_MANAGEMENT_RESPONSE	0x14 // frame.vhd : OP_TASK_REP
#define	SCSI_RESPONSE				0x15 // frame.vhd : OP_SCSI_REP
#define	DATA_R2H					0x16 // frame.vhd : OP_DATA_REP
#define READY_TO_TRANSFER			0x17 // frame.vhd : OP_READY_REP
#define	IDE_RESPONSE				0x18 // frame.vhd : OP_IDE_REP
#define	VENDOR_SPECIFIC_RESPONSE	0x1F // frame.vhd : OP_VENDOR_REP

//
// Error code from NDAS device. Set at remote to host PDU's response field
//
#define LANSCSI_RESPONSE_SUCCESS                0x00 // frame.vhd : pREP_SUCCESS
#define LANSCSI_RESPONSE_RI_NOT_EXIST           0x10 // not exist
#define LANSCSI_RESPONSE_T_SET_SEMA_FAIL		0x11 // Used by 1.1, 2.0. Removed at 2.5. 
#define LANSCSI_RESPONSE_RI_COMMAND_FAILED      0x12 // not exist
#define LANSCSI_RESPONSE_RI_VERSION_MISMATCH    0x13 // frame.vhd : pREP_INVALID_VERSION
#define LANSCSI_RESPONSE_RI_AUTH_FAILED         0x14 // not exist at HW
#define LANSCSI_RESPONSE_T_NOT_EXIST			0x20 // not exist at HW
#define LANSCSI_RESPONSE_T_BAD_COMMAND          0x21 // frame.vhd : pREP_NO_TARGET 
													// HW datasheet says it is NO_TARGET?? Anyway this is not sent by HW
#define LANSCSI_RESPONSE_T_COMMAND_FAILED       0x22 // frame.vhd : cREP_FAIL . Set when disk returned error.
#define LANSCSI_RESPONSE_T_BROKEN_DATA			0x23 // Added at 2.5. DATA digest does not match
#define LANSCSI_RESPONSE_T_NO_PERMISSION		0x24 // User has no permission
#define LANSCSI_RESPONSE_T_CONFLICT_PERMISSION	0x25 // Returned when user has access permission, 
					 //  but cannot access disk because another user has already accessing


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
		PUCHAR					pBufferBase;
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

	UINT16	Revision;
	UINT16	Reserved6;
//	UINT32	Reserved6;
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

//
// Vender Specific Operation.
//

#define	NKC_VENDOR_ID				0x0001
#define	VENDOR_OP_CURRENT_VERSION	0x01

#define	VENDOR_OP_SET_MAX_RET_TIME			0x01 // frame.vhd : VENDOR_SET_MAX_RET_TIME
#define VENDOR_OP_SET_MAX_CONN_TIME			0x02 // frame.vhd : VENDOR_SET_MAX_CONN_TIME
#define	VENDOR_OP_GET_MAX_RET_TIME			0x03 // frame.vhd : VENDOR_GET_MAX_RET_TIME
#define	VENDOR_OP_GET_MAX_CONN_TIME			0x04 // frame.vhd : VENDOR_GET_MAX_CONN_TIME
#define VENDOR_OP_SET_MUTEX					0x05 // frame.vhd : VENDOR_SET_SEMA
#define VENDOR_OP_FREE_MUTEX				0x06 // frame.vhd : VENDOR_FREE_SEMA
#define VENDOR_OP_GET_MUTEX_INFO			0x07 // frame.vhd : VENDOR_GET_SEMA
#define VENDOR_OP_GET_MUTEX_OWNER			0x08 // frame.vhd : VENDOR_GET_OWNER_SEMA
#define VENDOR_OP_SET_MUTEX_INFO			0x09 // frame.vhd : VENDOR_GET_OWNER_SEMA
#define VENDOR_OP_BREAK_MUTEX				0x0b // frame.vhd : VENDOR_GET_OWNER_SEMA // Added at 2.5

#define VENDOR_OP_SET_DYNAMIC_MAX_CONN_TIME	0x0a // frame.vhd : VENDOR_SET_DYNAMIC_MAX_CONN_TIME, obsolete
													// Removed at 2.5?
#define VENDOR_OP_SET_HEART_TIME			0x0c // Added at 2.5
#define VENDOR_OP_GET_HEART_TIME			0x0d // Added at 2.5
#define VENDOR_OP_SET_USER_PERMISSION		0x0e // Added at 2.5
#define VENDOR_OP_GET_USER_PERMISSION		0x0f // Added at 2.5


#define VENDOR_OP_SET_SUPERVISOR_PW			0x11 // frame.vhd : VENDOR_SET_SUSER_PASSWD
#define VENDOR_OP_SET_USER_PW				0x12 // frame.vhd : VENDOR_SET_USER_PASSWD
#define VENDOR_OP_SET_ENC_OPT				0x13 // frame.vhd : VENDOR_SET_ENC_OPT
#define VENDOR_OP_SET_OPT					VENDOR_OP_SET_ENC_OPT // Added more field at 2.5 and changed name


#define VENDOR_OP_SET_STANBY_TIMER			0x14 // frame.vhd : VENDOR_SET_STANBY_TIMER
#define VENDOR_OP_GET_STANBY_TIMER			0x15 // frame.vhd : VENDOR_GET_STANBY_TIMER

#define VENDOR_OP_SET_DELAY					0x16 // frame.vhd : VENDOR_SET_DELAY
#define VENDOR_OP_GET_DELAY					0x17 // frame.vhd : VENDOR_GET_DELAY
#define VENDOR_OP_SET_DYNAMIC_MAX_RET_TIME	0x18 // frame.vhd : VENDOR_SET_DYNAMIC_MAX_RET_TIME
#define VENDOR_OP_GET_DYNAMIC_MAX_RET_TIME	0x19 // frame.vhd : VENDOR_GET_DYNAMIC_MAX_RET_TIME
#define VENDOR_OP_SET_D_ENC					0x1a // frame.vhd : VENDOR_SET_D_ENC
#define VENDOR_OP_SET_D_OPT					VENDOR_OP_SET_D_ENC  // Added more field at 2.5 and changed name
#define VENDOR_OP_GET_D_ENC					0x1b // frame.vhd : VENDOR_GET_D_ENC
#define VENDOR_OP_GET_D_OPT					VENDOR_OP_GET_D_ENC // Added more field at 2.5 and changed name

#define VENDOR_OP_GET_WRITE_LOCK			0x20	// Added at 2.5
#define VENDOR_OP_FREE_WRITE_LOCK			0x21	// Added at 2.5

#define VENDOR_OP_GET_HOST_LIST				0x24	// Added at 2.5

#define VENDOR_OP_SET_DEAD_LOCK_TIME		0x28	// Added at 2.5
#define VENDOR_OP_GET_DEAD_LOCK_TIME		0x29	// Added at 2.5

#define VENDOR_OP_SET_EEP					0x2a	// Added at 2.5
#define VENDOR_OP_GET_EEP					0x2b	// Added at 2.5
#define VENDOR_OP_U_SET_EEP					0x2c	// Added at 2.5
#define VENDOR_OP_U_GET_EEP					0x2d	// Added at 2.5

#define VENDOR_OP_SET_WATCHDOG_TIME			0x2e	// Added at 2.5
#define VENDOR_OP_GET_WATCHDOG_TIME			0x2f	// Added at 2.5

#define VENDOR_OP_SET_MAC					0xfe // frame.vhd : VENDOR_SET_MAC
													// Removed at 2.5
#define VENDOR_OP_RESET						0xFF // frame.vhd : VENDOR_RESET



// Vendor Specific Request.
typedef struct _LANSCSI_VENDOR_REQUEST_PDU_HEADER {
	UCHAR	Opcode;

	// Flags.
	union {
		struct {
			unsigned _int8	FlagReserved:7;
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

	unsigned _int32	Reserved2;
	unsigned _int32	Reserved3;
	unsigned _int32	Reserved4;
	unsigned _int64	Reserved5;

	unsigned _int16	VendorID;
	unsigned _int8	VendorOpVersion;
	unsigned _int8	VendorOp;

	// Version 1.0, 1.1, 2.0 uses 
	//		Parameter 0 as Bit 63~32
	//		Parameter 1 as Bit 31~0
	//		Parameter 2 is not used.
	// Version 2.5 uses
	//		Parameter 0 as Bit 95~64
	//		Parameter 1 as Bit 63~32
	//		Parameter 2 is not 31~0	
	unsigned _int32	VendorParameter0;	
	unsigned _int32 VendorParameter1;	
	unsigned _int32 VendorParameter2;
	
} LANSCSI_VENDOR_REQUEST_PDU_HEADER, *PLANSCSI_VENDOR_REQUEST_PDU_HEADER;

// Vendor Specific Reply.
typedef struct _LANSCSI_VENDOR_REPLY_PDU_HEADER {
	unsigned _int8	Opcode;

	// Flags.
	union {
		struct {
			unsigned _int8	FlagReserved:7;
			unsigned _int8	F:1;
		};
		unsigned _int8	Flags;
	};

	unsigned _int8	Response;

	unsigned _int8	Reserved1;

	unsigned _int32	HPID;
	unsigned _int16	RPID;
	unsigned _int16	CPSlot;
	unsigned _int32	DataSegLen;
	unsigned _int16	AHSLen;
	unsigned _int16	CSubPacketSeq;
	unsigned _int32	PathCommandTag;

	unsigned _int32	Reserved2;
	unsigned _int32	Reserved3;
	unsigned _int32	Reserved4;
	unsigned _int64	Reserved5;

	unsigned _int16	VendorID;
	unsigned _int8	VendorOpVersion;
	unsigned _int8	VendorOp;

	// Version 1.0, 1.1, 2.0 uses 
	//		Parameter 0 as Bit 63~32
	//		Parameter 1 as Bit 31~0
	//		Parameter 2 is not used.
	// Version 2.5 uses
	//		Parameter 0 as Bit 95~64
	//		Parameter 1 as Bit 63~32
	//		Parameter 2 is not 31~0	
	unsigned _int32	VendorParameter0;	
	unsigned _int32 VendorParameter1;	
	unsigned _int32 VendorParameter2;	
} LANSCSI_VENDOR_REPLY_PDU_HEADER, *PLANSCSI_VENDOR_REPLY_PDU_HEADER;


#include <poppack.h>


#endif
