#ifndef __WIN_SCSI_EXTENSION_H__
#define __WIN_SCSI_EXTENSION_H__


//
//	Copied from wnet scsi.h
//
#if !defined(_CDBEXT_DEFINED)
#define _CDBEXT_DEFINED

#pragma pack(push, cdb, 1)

typedef union _CDBEXT {
	//
	// Standard 16-byte CDB
	//

	struct _EXT_CDB16 {
		UCHAR OperationCode;
		UCHAR Reserved1        : 3;
		UCHAR ForceUnitAccess  : 1;
		UCHAR DisablePageOut   : 1;
		UCHAR Protection       : 3;
		UCHAR LogicalBlock[8];
		UCHAR TransferLength[4];
		UCHAR Reserved2;
		UCHAR Control;
	} CDB16;

	//
	// 16-byte CDBs
	//

	struct _EXT_READ16 {
		UCHAR OperationCode;      // 0x88 - SCSIOP_READ16
		UCHAR Reserved1         : 3;
		UCHAR ForceUnitAccess   : 1;
		UCHAR DisablePageOut    : 1;
		UCHAR ReadProtect       : 3;
		UCHAR LogicalBlock[8];
		UCHAR TransferLength[4];
		UCHAR Reserved2         : 7;
		UCHAR Streaming         : 1;
		UCHAR Control;
	} READ16;

	struct _EXT_WRITE16 {
		UCHAR OperationCode;      // 0x8A - SCSIOP_WRITE16
		UCHAR Reserved1         : 3;
		UCHAR ForceUnitAccess   : 1;
		UCHAR DisablePageOut    : 1;
		UCHAR WriteProtect      : 3;
		UCHAR LogicalBlock[8];
		UCHAR TransferLength[4];
		UCHAR Reserved2         : 7;
		UCHAR Streaming         : 1;
		UCHAR Control;
	} WRITE16;

	struct _EXT_VERIFY16 {
		UCHAR OperationCode;      // 0x8F - SCSIOP_VERIFY16
		UCHAR Reserved1         : 1;
		UCHAR ByteCheck         : 1;
		UCHAR BlockVerify       : 1;
		UCHAR Reserved2         : 1;
		UCHAR DisablePageOut    : 1;
		UCHAR VerifyProtect     : 3;
		UCHAR LogicalBlock[8];
		UCHAR VerificationLength[4];
		UCHAR Reserved3         : 7;
		UCHAR Streaming         : 1;
		UCHAR Control;
	} VERIFY16;

	struct _EXT_SYNCHRONIZE_CACHE16 {
		UCHAR OperationCode;      // 0x91 - SCSIOP_SYNCHRONIZE_CACHE16
		UCHAR Reserved1         : 1;
		UCHAR Immediate         : 1;
		UCHAR Reserved2         : 6;
		UCHAR LogicalBlock[8];
		UCHAR BlockCount[4];
		UCHAR Reserved3;
		UCHAR Control;
	} SYNCHRONIZE_CACHE16;

	struct _EXT_READ_CAPACITY16 {
		UCHAR OperationCode;      // 0x9E - SCSIOP_READ_CAPACITY16
		UCHAR ServiceAction     : 5;
		UCHAR Reserved1         : 3;
		UCHAR LogicalBlock[8];
		UCHAR BlockCount[4];
		UCHAR PMI               : 1;
		UCHAR Reserved2         : 7;
		UCHAR Control;
	} READ_CAPACITY16;

} CDBEXT, *PCDBEXT;

#pragma pack(pop, cdb)
#endif /* _CDBEXT_DEFINED */

#ifndef SCSIOP_READ_CAPACITY16

#pragma pack(push, byte_stuff, 1)

#if WINVER >= 0x0501
#else	// #if WINVER >= 0x0501

typedef union _EIGHT_BYTE {

	struct {
		UCHAR Byte0;
		UCHAR Byte1;
		UCHAR Byte2;
		UCHAR Byte3;
		UCHAR Byte4;
		UCHAR Byte5;
		UCHAR Byte6;
		UCHAR Byte7;
	};

	ULONGLONG AsULongLong;
} EIGHT_BYTE, *PEIGHT_BYTE;

#endif // #if WINVER >= 0x0501

#pragma pack(pop, byte_stuff)

//
// Byte reversing macro for converting
// between big- and little-endian formats
//

#define REVERSE_BYTES_QUAD(Destination, Source) {           \
	PEIGHT_BYTE d = (PEIGHT_BYTE)(Destination);             \
	PEIGHT_BYTE s = (PEIGHT_BYTE)(Source);                  \
	d->Byte7 = s->Byte0;                                    \
	d->Byte6 = s->Byte1;                                    \
	d->Byte5 = s->Byte2;                                    \
	d->Byte4 = s->Byte3;                                    \
	d->Byte3 = s->Byte4;                                    \
	d->Byte2 = s->Byte5;                                    \
	d->Byte1 = s->Byte6;                                    \
	d->Byte0 = s->Byte7;                                    \
}


#pragma pack(push, read_capacity_ex, 1)
typedef struct _READ_CAPACITY_DATA_EX {
	LARGE_INTEGER LogicalBlockAddress;
	ULONG BytesPerBlock;
} READ_CAPACITY_DATA_EX, *PREAD_CAPACITY_DATA_EX;
#pragma pack(pop, read_capacity_ex)


// 16-byte commands

#define SCSIOP_READ16                   0x88
#define SCSIOP_WRITE16                  0x8A
#define SCSIOP_VERIFY16                 0x8F
#define SCSIOP_SYNCHRONIZE_CACHE16      0x91
#define SCSIOP_READ_CAPACITY16          0x9E

#endif	// #ifndef SCSIOP_READ_CAPACITY16

//
// Might have less than 16 CDB size in WNET AMD64 configuration.
//

#if MAXIMUM_CDB_SIZE < 16
#undef MAXIMUM_CDB_SIZE
#define MAXIMUM_CDB_SIZE 16
#endif

#endif