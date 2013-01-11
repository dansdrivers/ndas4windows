#ifndef LANSCSI_PROTOCOL_IDE_SPEC_H
#define LANSCSI_PROTOCOL_IDE_SPEC_H

//
// Default PIO mode
//

#define NDAS_DEFAULT_PIO_MODE 3


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
	unsigned _int8	Feature;		// Set to error register when disk error occurred. See Bits for HD_ERROR at hdreg.h
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
	unsigned _int8	Command;		// NDAS chip 1.0 does not set status register.
	unsigned _int16	Reserved5;
} LANSCSI_IDE_REPLY_PDU_HEADER, *PLANSCSI_IDE_REPLY_PDU_HEADER;



//////////////////////////////////////////////////////////////////////////
//
//	Lanscsi/Ide protocol version 1.1
//

// IDE Request.
typedef struct _LANSCSI_IDE_REQUEST_PDU_HEADER_V1 {
	unsigned _int8	Opcode; // byte 1
	
	// Flags.
	union {
		struct {
			unsigned _int8	FlagReserved:5;
			unsigned _int8	W:1;
			unsigned _int8	R:1;
			unsigned _int8	F:1;
		};
		unsigned _int8	Flags;
	}; // byte 2

	unsigned _int16	Reserved1; // byte 4
	unsigned _int32	HPID;  // byte 8
	unsigned _int16	RPID;  // byte 10
	unsigned _int16	CPSlot; // byte 12
	unsigned _int32	DataSegLen; // byte 16
	unsigned _int16	AHSLen; // byte 18
	unsigned _int16	CSubPacketSeq; // byte 20
	unsigned _int32	PathCommandTag; // byte 24
	unsigned _int32	InitiatorTaskTag;	// 28
	unsigned _int32	DataTransferLength;	 // 32
	unsigned _int32	TargetID; // 36
//	unsigned _int64	LUN; // As of NDAS 2.5 changed to 8 bit value
	unsigned _int8	LUN;
	unsigned _int8	Reserved2;
	unsigned _int16	Reserved3;
	unsigned _int32	Reserved4; // 44

	unsigned _int32 COM_Reserved : 2;
	unsigned _int32 COM_TYPE_E : 1;
	unsigned _int32 COM_TYPE_R : 1;
	unsigned _int32 COM_TYPE_W : 1;
	unsigned _int32 COM_TYPE_D_P : 1;
	unsigned _int32 COM_TYPE_K : 1;
	unsigned _int32 COM_TYPE_P : 1;
	unsigned _int32 COM_LENG : 24; // 48

	unsigned _int8	Feature_Prev;
	unsigned _int8	Feature_Curr;
	unsigned _int8	SectorCount_Prev;
	unsigned _int8	SectorCount_Curr; //52
	unsigned _int8	LBALow_Prev;
	unsigned _int8	LBALow_Curr;	// 54
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
	}; // 60

} LANSCSI_IDE_REQUEST_PDU_HEADER_V1, *PLANSCSI_IDE_REQUEST_PDU_HEADER_V1;

// IDE Reply.
typedef struct _LANSCSI_IDE_REPLY_PDU_HEADER_V1 {

	//
	//	Operation code
	//	Do not confuse operation code and IDE command register
	//

	unsigned _int8	Opcode;

	//
	// Flags
	//

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


	//
	//	result from IDE or SCSI commands
	//	For 2.0, always 0 (good).
	//	For 2.5, not used.
	//

	unsigned _int8	Status;


	//
	//	Host port ID
	//

	unsigned _int32	HPID;


	//
	//	Remote port ID ( NDAS device port ID )
	//

	unsigned _int16	RPID;


	//
	//	Command processing slot number
	//	HPID-RPID session may have more than one slot
	//	Each slot can process commands independently
	//

	unsigned _int16	CPSlot;


	//
	//	Data segment length
	//	Obsolete
	//

	unsigned _int32	DataSegLen;


	//
	//	Additional header length
	//	Parameter length when logging in
	//	Packet command length with packet device
	//

	unsigned _int16	AHSLen;



	//
	//	Command sub-packet sequence number
	//	Increments sequentially from zero.
	//

	unsigned _int16	CSubPacketSeq;


	//
	//	Path command tag
	//	unique number for a command
	//

	unsigned _int32	PathCommandTag;


	//
	//	Not used
	//

	unsigned _int32	InitiatorTaskTag;


	//
	//	Data length to be transfer
	//

	unsigned _int32	DataTransferLength;	


	//
	//	SCSI address
	//

	unsigned _int32	TargetID;
//	unsigned _int64	LUN; // As of NDAS 2.5 changed to 8 bit value
	unsigned _int8	LUN;
	unsigned _int8	Reserved1;
	unsigned _int16	Reserved2;
	unsigned _int16	Reserved3;
	unsigned _int8	PendingWriteCount;
	unsigned _int8	RetransmitCount;


	//
	//	IDE Registers
	//

	unsigned _int32 COM_Reserved : 2;
	unsigned _int32 COM_TYPE_E : 1;
	unsigned _int32 COM_TYPE_R : 1;
	unsigned _int32 COM_TYPE_W : 1;
	unsigned _int32 COM_TYPE_D_P : 1;
	unsigned _int32 COM_TYPE_K : 1;
	unsigned _int32 COM_TYPE_P : 1;
	unsigned _int32 COM_LENG : 24;

	unsigned _int8	Feature_Prev;
	unsigned _int8	Feature_Curr;		// Used as error register when replying. See Bits for HD_ERROR at hdreg.h
	unsigned _int8	SectorCount_Prev;
	unsigned _int8	SectorCount_Curr;
	unsigned _int8	LBALow_Prev;
	unsigned _int8	LBALow_Curr;
	unsigned _int8	LBAMid_Prev;
	unsigned _int8	LBAMid_Curr;
	unsigned _int8	LBAHigh_Prev;
	unsigned _int8	LBAHigh_Curr;
	unsigned _int8	Command;		// Status register. At 1.1,2.0, this value does not change.
									// But error bit (bit0) will be ORed, so useless in most cases
									// At 2.5, this value is status register of IDE register

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
			unsigned _int8	P:1;	// not used???
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
//	unsigned _int64	LUN; // As of NDAS 2.5 changed to 8 bit value
	unsigned _int8	LUN;
	unsigned _int8	Reserved2;
	unsigned _int16	Reserved3;
	unsigned _int32	Reserved4;

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
//	unsigned _int64	LUN;
	unsigned _int8	LUN;
	unsigned _int8	Reserved1;
	unsigned _int16	Reserved2;
	unsigned _int16	Reserved3;
	unsigned _int8	PendingWriteCount;
	unsigned _int8	RetransmitCount;

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



//////////////////////////////////////////////////////////////////////////
//
//	WAN SCSI definition
//


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